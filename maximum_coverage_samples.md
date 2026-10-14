# Maximum Coverage Solver サンプルコード集

対象は `maximum_coverage_solver_v08.hpp`。C++20・GCC環境で使う、最大被覆・予算付き最大被覆・最小費用集合被覆のライブラリである。本書は、問題の表し方、全公開API、入力と出力を持つ独立した実行例の3章で構成する。

各例のコードを `main.cpp` として保存し、ヘッダを同じディレクトリに置けば実行できる。第3章のプログラムは互いに独立しており、別の例の関数や変数を用意する必要はない。入力の整数や候補番号は、各例に示す条件を満たすものとする。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined main.cpp -o main
./main < input.txt
```

`input.txt` へ各例の「入力例」を保存する。候補番号と対象番号はいずれも0始まりである。両者は別の番号空間なので、候補0が対象0を覆うとは限らない。

## 1. 何を解くライブラリか？

### 1.1 選ぶ候補と、覆いたい対象

たとえば、施設を置ける場所が10か所あり、サービスを届けたい地区が30地区あるとする。各場所について「そこに施設を置くと、どの地区に届くか」を一覧にする。ライブラリはその一覧を受け取り、施設を置く候補を選ぶ。

施設以外にも、候補をセンサー・検査・テスト・広告枠・作業計画と考え、対象を観測地点・要件・顧客群・仕事と考えれば利用できる。対象を1つ以上の候補が含むことを「被覆する」と呼ぶ。同じ対象を2つの候補が覆っても、その対象の価値は1回だけ数える。

入力は「候補ごとの対象番号の集合」である。距離、道路のつながり、テストの実行結果などから、その集合を作る部分は利用側で用意する。

### 1.2 記号と共通の条件

| 記号 | 意味 | 値の条件 |
|---|---|---|
| $T$ | 対象の個数 | 0以上の整数 |
| $C$ | 候補の個数 | 0以上の整数 |
| $t$ | 対象番号 | $0\le t<T$ |
| $c$ | 候補番号 | $0\le c<C$ |
| $S_ {c}$ | 候補 $c$ が覆う対象番号の集合 | 空集合も許す |
| $w_ {t}$ | 対象 $t$ の重要度・価値 | 非負整数 |
| $a_ {c}$ | 候補 $c$ を選ぶ費用 | 非負整数 |
| $X$ | 実際に選ぶ候補番号の集合 | 求める解 |
| $U(X)$ | $X$ の候補によって覆われる対象の集合 | $X$ から決まる |
| $W(X)$ | 覆われた対象の重要度の合計 | 最大被覆の目的値 |
| $A(X)$ | 選択候補の費用の合計 | 予算で制限する値、または最小化する値 |
| $L,K$ | 選ぶ候補数の下限・上限 | $0\le L\le K$ |
| $B$ | 費用の上限である予算 | 非負整数 |
| $F$ | 必ず選ぶ候補の集合 | 省略時は空集合 |
| $D$ | 選んではいけない候補の集合 | 省略時は空集合 |
| $P$ | 固定候補以外で今回選択してよい候補の集合 | 制限を使わない場合は全候補 |

$$U(X)=\bigcup_ {c\in X}S_ {c}$$

これは「選んだ候補の対象一覧を全部合わせ、同じ対象番号を1つにまとめた集合」である。重要度と費用は次の式で数える。

$$W(X)=\sum_ {t\in U(X)}w_ {t},\qquad A(X)=\sum_ {c\in X}a_ {c}$$

記号 $\sum$ は「指定された番号について足す」という意味である。空集合についての合計は0とする。固定・禁止・候補制限を指定した場合、共通して次の条件を満たす必要がある。

$$F\subseteq X\subseteq P\cup F,\qquad X\cap D=\varnothing$$

固定候補は必ず含め、禁止候補は含めない。候補制限の外にある固定候補も選ばれる。固定候補の個数と費用も、上限や予算の内数として数える。同じ候補を固定と禁止の両方にはできない。

### 1.3 候補数を制限して重要度を最大にする

$$\text{最大化する値: }W(X),\qquad\text{条件: }L\le\lvert X\rvert\le K$$

$\lvert X\rvert$ は選ぶ候補の個数である。「最大3台」なら $L=0,K=3$、「3台ちょうど」なら $L=K=3$ を指定する。費用の予算制約は付かない。すべての対象の重みを1にすれば、被覆する対象数を大きくする問題になる。

### 1.4 予算を守って重要度を最大にする

$$\text{最大化する値: }W(X),\qquad\text{条件: }A(X)\le B$$

候補ごとに費用が異なる場合に使う。「費用の合計を予算内に収め、その中で覆える重要度を大きくする」という問題である。候補数の下限・上限は同時には指定できない。費用0の候補も扱える。

### 1.5 全対象を覆って費用を小さくする

$$\text{最小化する値: }A(X),\qquad\text{条件: }U(X)=\{0,\ldots,T-1\}$$

対象が0個なら右辺は空集合とする。「全要件をテストしながら総実行時間を小さくする」などに使う。全候補の費用を1にすれば、選ぶ候補数を小さくできる。重み0の対象も被覆必須であり、重みを0にしても対象の除外にはならない。個数上限や予算上限は指定しない。

### 1.6 結果の読み方と適用範囲

最適解は保証しない。返却値の `feasible` は、その呼び出しの制約を満たすかを表し、「最適か」「十分に探索したか」は表さない。最大被覆では未被覆対象があっても実行可能である。集合被覆では全対象を覆ったときだけ実行可能になる。

候補同士の競合、複数予算、対象を2回覆う条件、負の重み・費用、対象ごとの担当候補の決定は直接表現できない。たとえば2つのルートが同じ車両を同時刻に使うことを、このライブラリは検査しない。単に同じ対象を複製しても「別の候補で2回覆う」という条件にはならない。

## 2. APIリファレンス

### 2.1 全公開APIの見取り図

型名は名前空間を付けずに利用する。公開の列挙型はない。`true` / `false` の意味は各フィールドの表で説明する。ライブラリ内部の型や関数は利用APIではない。

| 区分 | 公開要素 |
|---|---|
| 入力 | `coverage_problem` |
| 探索設定 | `coverage_solver_options` |
| 呼び出し単位の条件 | `coverage_subproblem` |
| 選択数の範囲 | `coverage_cardinality_bounds` |
| 探索の返却値 | `coverage_solution` |
| 評価の返却値 | `coverage_evaluation` |
| solver | `coverage_solver` のコンストラクタと16個のメソッド定義 |
| デバッグ出力 | `coverage_solution` と `coverage_evaluation` 用の `operator<<` |

「overload」は、同じ名前で引数の型が違うメソッドを意味する。第2.6～2.10節で全16定義を列挙する。

### 2.2 `coverage_problem` とコンストラクタ

`coverage_problem problem;` で全フィールドが空の問題を作る。

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `target_weights` | `std::vector<long long>` | 空 | 対象番号順の非負重み。長さが対象数T |
| `candidate_costs` | `std::vector<long long>` | 空 | 候補番号順の非負費用。長さが候補数C |
| `covered_targets` | `std::vector<std::vector<int>>` | 空 | 長さC。各内側配列はその候補が覆う対象番号。各値は0以上T未満 |
| `forced_candidates` | `std::vector<int>` | 空 | 構築後ずっと必ず選ぶ候補番号。各値は0以上C未満 |
| `forbidden_candidates` | `std::vector<int>` | 空 | 構築後ずっと選ばない候補番号。各値は0以上C未満 |

同じ候補の `covered_targets` にある対象番号の重複は自動的に除かれる。恒久固定・恒久禁止の各リスト内の重複は許容されるが、固定と禁止の重なりはエラーになる。部分問題のリストと初期解のリストでは、重複の扱いが異なる点に注意する。

選択数制約だけを使う場合も `candidate_costs` をC個用意する。費用を考えないなら全1にする。集合被覆だけを使う場合も `target_weights` をT個用意する。重要度を考えないなら全1にする。

```cpp
explicit coverage_solver(const coverage_problem& problem);
```

引数 `problem` は必須で、既定値はない。内部に必要な入力をコピーするため、構築後は元の `problem` を破棄してよい。元の `problem` を書き換えても構築済みsolverへは反映されない。コンストラクタで対象数・候補数・被覆関係が固定される。

### 2.3 `coverage_solver_options`

`coverage_solver_options options;` の既定値は次の通り。すべてsolve・improveの呼び出しごとに変更できる。

| フィールド | 型 | 既定値 | 有効な値・意味 |
|---|---|---|---|
| `time_limit_ms` | `int` | `100` | `-1` は相対時間の停止を無効化。0以上は改善探索の時間目安、単位ms |
| `iteration_limit` | `long long` | `std::numeric_limits<long long>::max()` | 0以上。改善試行の回数上限。0ならランダム改善を行わない |
| `seed` | `std::uint64_t` | `1` | 型の全範囲。ランダム改善で使う乱数系列を選ぶ |
| `deadline` | `std::chrono::steady_clock::time_point` | `std::chrono::steady_clock::time_point::max()` | 絶対締切。既定値は無効。過去の時刻も指定可能 |

返却される `iterations` は `iteration_limit` 以下である。ただし時間切れや早期終了で小さくなる。1反復の仕事量は問題の種類によって異なり、個数制約と予算制約の反復数を速度指標として直接比較しない。

`iteration_limit=0` でも初期解の作成、初期解の検証・補充・比較、不要候補の除去、選択数下限の補充は行う。`improve_*` に入力した集合をそのまま返す指定ではない。`time_limit_ms=0` でも初期解作成は行う。

時間制限は厳密な停止保証ではない。相対時間にはsolver構築と呼び出し冒頭の部分問題準備を含まず、相対時間だけでは決定的な初期解作成を途中停止しない。絶対締切は初期解作成でも確認するが、前処理・条件検証・下限補充などは途中停止しない。出力の時間も利用側で確保する。

| 目的 | 設定例 | 注意 |
|---|---|---|
| 短い最初の例 | `time_limit_ms=-1`, `iteration_limit=0` | 初期解を得る設定。最適性の保証はない |
| 同じ条件で比較 | `time_limit_ms=-1`, `iteration_limit=500`, `seed=1` | 同じビルド・環境で再現しやすい。deadlineは無効にする |
| 実戦で時間を配分 | `time_limit_ms=50` と外側の `deadline` | 50msは試す出発点。問題規模で調整する |
| 複数seedを比較 | seedだけを変えて複数回呼ぶ | 各回の予算・重み・制約を揃えて評価する |

`time_limit_ms=-1` のときは有限の反復上限か絶対締切を併用する。seedを固定しても、時間で停止させると負荷によって結果が変わり得る。

### 2.4 `coverage_subproblem`

構築したsolverを再利用しつつ、その呼び出しだけの条件を渡す。`coverage_subproblem sub;` の各spanは空、boolはfalseである。`std::span` は配列の所有者ではなく、既存のvectorやarrayを見る窓である。参照する配列は、メソッドが返るまで破棄・変更・再確保しない。

| フィールド | 型 | 既定値 | 意味・条件 |
|---|---|---|---|
| `fixed_candidates` | `std::span<const int>` | 空 | 今回だけ追加で固定する候補。各番号は0以上C未満、リスト内で一意 |
| `forbidden_candidates` | `std::span<const int>` | 空 | 今回だけ追加で禁止する候補。各番号は0以上C未満、リスト内で一意 |
| `restrict_to_candidate_pool` | `bool` | `false` | trueなら非固定候補をpool内へ制限。falseならpoolを使わない |
| `candidate_pool` | `std::span<const int>` | 空 | trueのとき選択可能な非固定候補。各番号は0以上C未満、リスト内で一意 |
| `target_weights_override` | `std::span<const long long>` | 空 | 空なら構築時の重み。非空なら長さT、全要素非負の配列へ差し替える |
| `candidate_costs_override` | `std::span<const long long>` | 空 | 空なら構築時の費用。非空なら長さC、全要素非負の配列へ差し替える |

固定・禁止は構築時の条件へ追加され、恒久条件は解除できない。恒久固定候補を追加固定リストへ1回含めるのはよい。固定候補はpool外でも選ばれるが、禁止との重なりはエラーである。

`restrict_to_candidate_pool=false` のとき、poolの内容は参照・検証されない。trueで空poolなら固定候補以外を選べない。poolへ入れても禁止候補は選べない。

overrideは変更した要素だけではなく、配列全体を渡す。全重みを0へ変えるにはT個の0を渡す。空配列は「差し替えなし」である。呼び出しを終えると、条件は次の呼び出しへ残らない。同じ差し替えを使うなら毎回同じ `sub` を渡す。

### 2.5 `coverage_cardinality_bounds`

| フィールド | 型 | 既定値 | 意味・条件 |
|---|---|---|---|
| `min_selected` | `int` | `0` | 選択数の下限。0以上 |
| `max_selected` | `int` | `std::numeric_limits<int>::max()` | 選択数の上限。min_selected以上 |

`coverage_cardinality_bounds{L, K}` はL個以上K個以下、`{K, K}` はK個ちょうどを意味する。候補数より大きい上限も有効だが、下限を満たす候補がなければ `feasible=false`。固定候補も個数に含む。空の被覆集合を持つ候補も下限を満たすために選ばれ得る。同じ得点の中で費用まで最小にする保証はない。

### 2.6 選択数制約のsolve・improve：6定義

以下はAPI宣言であり、コピーして再定義するコードではない。`options={}` と `subproblem={}` はそれぞれ第2.3・2.4節の既定値を使う。全メソッドの戻り値は `coverage_solution`。

```cpp
coverage_solution solve_maximum_coverage(
    int max_selected,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution solve_maximum_coverage(
    coverage_cardinality_bounds bounds,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_maximum_coverage(
    int max_selected,
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_maximum_coverage(
    int max_selected,
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_maximum_coverage(
    coverage_cardinality_bounds bounds,
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_maximum_coverage(
    coverage_cardinality_bounds bounds,
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;
```

| 引数 | 既定値 | 意味・有効な値 |
|---|---|---|
| `max_selected` | なし、必須 | 0以上のint。最大選択数であり、ちょうどその個数ではない |
| `bounds` | なし、必須 | 第2.5節の下限・上限。int版の代わりに渡す |
| `initial_candidates` | なし、improveでは必須 | 元の候補番号の一意な列。範囲外・禁止・pool外の非固定候補を含めない |
| `options` | `{}` | 時間・反復数・seed・締切 |
| `subproblem` | `{}` | 今回の固定・禁止・pool・数値差し替え |

span版にはvectorやarrayを渡せる。initializer_list版には `{0, 2}` のように書く。空集合なら `{}` を渡せる。固定候補は自動追加され、初期解へ1回書いても二重には数えない。固定追加後の初期解が上限を超えるのは入力エラーである。一方、下限を満たしていない初期解は受け取り、返却前に補充する。

初期解の候補は固定ではない。改善中に削除され得る。必ず残すには `forced_candidates` または `fixed_candidates` を使う。予算制約は付かない。被覆重みが同じなら候補数が少ない解を優先する。

### 2.7 予算制約のsolve・improve：3定義

```cpp
coverage_solution solve_budgeted_maximum_coverage(
    long long budget,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_budgeted_maximum_coverage(
    long long budget,
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_budgeted_maximum_coverage(
    long long budget,
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;
```

`budget` は必須の非負 `long long` で、候補費用と同じ単位。0なら費用0の候補だけを使える。他の引数は第2.6節と同じ型・意味・既定値である。初期解は固定追加と費用overrideを反映した後も予算内でなければならない。予算内へ自動で削ってくれるAPIではない。

戻り値は `coverage_solution`。被覆重み、費用の小ささ、候補数の小ささの順に比較する。候補数の上限・下限は指定できない。固定候補だけで予算を超える場合は `feasible=false`。

### 2.8 最小費用集合被覆のsolve・improve：3定義

```cpp
coverage_solution solve_minimum_set_cover(
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_minimum_set_cover(
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;

coverage_solution improve_minimum_set_cover(
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}
) const;
```

`initial_candidates` はimproveでは必須で、空集合も許す。型・番号の条件・固定候補の自動追加は第2.6節と同じ。未被覆対象が残る初期解も受け取り、全被覆へ補修する。予算・選択数上限の引数はない。`options` と `subproblem` の既定値は `{}`。

戻り値は `coverage_solution`。全対象を覆う解の中で、費用が小さい解、次に候補数が少ない解を優先する。利用可能な候補から覆えない対象がある場合や、締切までに全被覆を作れない場合は `feasible=false`。重み0の対象も必要である。

### 2.9 `evaluate`：2定義

```cpp
coverage_evaluation evaluate(
    std::span<const int> selected_candidates,
    const coverage_subproblem& subproblem = {}
) const;

coverage_evaluation evaluate(
    std::initializer_list<int> selected_candidates,
    const coverage_subproblem& subproblem = {}
) const;
```

`selected_candidates` は必須で、元の候補番号を一意に列挙する。vector・arrayまたは `{0, 2}` を渡す。番号範囲、重複、禁止、poolの条件を検査する。固定候補は自動で追加し、重み・費用のoverrideを適用する。`subproblem={}` が既定値で、探索設定の引数はない。

戻り値は `coverage_evaluation`。解を探索・改善せず、指定集合を評価する。予算、選択数制約、全被覆を満たすかは利用側で判定する。固定を追加した結果の選択数・費用が返るため、手元の配列長や費用だけで判定しない。

### 2.10 個数の取得：2定義

```cpp
int target_count() const;
int candidate_count() const;
```

引数・既定値はない。構築時の対象数T・候補数Cを返す。候補数は前処理で通常探索から除外された候補も含む。呼び出し単位のpoolの個数ではない。計算量はどちらも $O(1)$。

### 2.11 `coverage_solution`：探索結果

| フィールド | 型 | 既定値 | 返却値の意味 |
|---|---|---|---|
| `selected_candidates` | `std::vector<int>` | 空 | 選択された元の候補番号。昇順、一意。固定候補を含む |
| `covered_weight` | `long long` | `0` | 被覆した対象の重み和。override使用時はその重み |
| `total_cost` | `long long` | `0` | 選択候補の費用和。override使用時はその費用 |
| `covered_target_count` | `int` | `0` | 重み0も含む、1回以上覆った対象数 |
| `uncovered_target_count` | `int` | `0` | Tから被覆対象数を引いたもの |
| `iterations` | `long long` | `0` | 実際に数えた改善試行数。全CPU命令数や貪欲追加回数ではない |
| `feasible` | `bool` | `false` | trueならその呼び出しの制約を満たす。falseなら解として採用しない |

結果は値として返り、vectorも結果が所有する。次にsolverを呼び出しても過去の結果は保持できる。falseの結果にも固定部分や部分解が入る場合がある。`iterations=0` は最適性や未処理を意味しない。

### 2.12 `coverage_evaluation`：評価結果

| フィールド | 型 | 既定値 | 意味 |
|---|---|---|---|
| `covered_weight` | `long long` | `0` | 固定追加後の被覆重み和 |
| `total_cost` | `long long` | `0` | 固定追加後の費用和 |
| `covered_target_count` | `int` | `0` | 重み0を含む被覆対象数 |
| `uncovered_target_count` | `int` | `0` | 未被覆対象数 |
| `selected_count` | `int` | `0` | 固定追加後の選択候補数 |

`feasible` フィールドはない。個数と予算を同時に検査することは可能だが、それによってsolverが両制約を同時に扱えるようになるわけではない。

### 2.13 デバッグ出力：2演算子

```cpp
std::ostream& operator<<(
    std::ostream& os,
    const coverage_solution& solution
);

std::ostream& operator<<(
    std::ostream& os,
    const coverage_evaluation& evaluation
);
```

引数はどちらも必須。`os` は `std::cout`、`std::cerr`、`std::ostringstream` などの出力先、もう一方は表示する結果である。戻り値は同じstreamへの参照なので、さらに `<< '\n'` などを連結できる。演算子は各結果型内のfriend関数として定義され、通常の `os << result` で使える。

全フィールドを名前付きで1行に表示する。solutionの配列は総要素数と先頭20要素を表示し、それより多い場合は `... (+残り件数 more)` を付ける。評価結果は全5項目を表示する。改行やflushは自動では付かない。`feasible` はtrue/falseの文字列、数値は出力先の書式に従う。入力結果と書式フラグは変更しない。デバッグ出力は提出用の形式とは別なので、競技中の診断には通常 `std::cerr` を使う。

### 2.14 エラー・境界値・再利用の契約

| 条件 | 挙動・注意 |
|---|---|
| 負の重み・費用・予算・選択数、下限が上限より大きい | `std::invalid_argument` |
| 配列長の不一致、範囲外番号、固定と禁止の競合 | `std::invalid_argument` |
| 部分問題リスト・初期解・評価対象の同一リスト内の重複 | `std::invalid_argument`。恒久固定・禁止のリストとは扱いが違う |
| `time_limit_ms < -1`、負の `iteration_limit` | `std::invalid_argument` |
| 固定候補だけで個数上限・予算を超える | 入力形式は有効だが、結果は `feasible=false` |
| 下限を満たす候補が足りない、全被覆不能 | `feasible=false` |
| 重み0の対象 | 最大被覆の得点は0。集合被覆では依然として必須 |
| 費用0の候補 | 許容。予算0でも使える |
| 対象も候補も0個 | 下限0の最大被覆と集合被覆は空の実行可能解 |
| 初期解が現在の上限・予算を超える | `std::invalid_argument`。条件変更後はevaluateなどで先に確認する |
| 前回のsolve結果を次へ引き継ぐ | `improve_*` へ候補IDを明示的に渡す。自動継承はしない |
| 被覆関係・対象数・候補数を変える | 新しい `coverage_solver` を構築する |
| 重み・費用・今回の固定・禁止・poolを変える | `coverage_subproblem` で指定して再実行する |

範囲外番号・負数などは、読み取った配列へ利用側が添字アクセスする前にも防ぐ必要がある。本書の例は入力形式と明記した値域を満たすデータを受け取る前提である。

即座に実行不能と判定できる場合は、improveの初期解を検証する前にfalseを返すことがある。たとえば固定部分が予算超過なら、初期解の重複が検査されないまま返り得る。初期解の形式検査だけを目的としてimproveを使わない。

対象数T、候補数C、正規化後の被覆関係総数はintの範囲内。全対象の重み和と全候補の費用和はlong longの範囲内に収める。override後も同じ条件が必要で、総和のオーバーフローを自動で防ぐものではない。

同じsolverを順番に呼んで再利用できるが、内部の作業配列を再利用するため、同じインスタンスへ同時に複数呼び出しを行わない。部分問題も前回の条件や内部探索状態を継続する仕組みではなく、毎回の呼び出しで条件を反映する。

## 3. ユースケース

各例は「具体的な問題」「入力形式」「実行コード」「入力例」「出力例」を持つ。コード中のライブラリ利用行には日本語コメントを付けている。配布ZIPの `examples/exampleNN.cpp` は、その番号の掲載コードと同一である。

再現しやすい例では、相対時間を無効にして有限の反復回数を使う。時間制御の例は、締切が過去のケースも含めて停止時の扱いを示す。出力例は添付入力と掲載ヘッダを用いた実測結果であり、一般入力の最適性を保証する値ではない。

| 例 | ユースケース |
|---|---|
| 3.1 | 設置数を制限して重要な地区を広くカバーする |
| 3.2 | 予算内で施策を選び、無料の施策も使う |
| 3.3 | 全要件を検査し、テスト実行時間を減らす |
| 3.4 | ちょうどK件の代表データを選ぶ |
| 3.5 | 最低台数と最大台数を持つ観測機器を選ぶ |
| 3.6 | 既設施設を残し、恒久的な禁止場所を避ける |
| 3.7 | そのターンだけの故障と継続稼働を指定する |
| 3.8 | 外部で絞った候補だけから追加候補を選ぶ |
| 3.9 | 外部で作った最大被覆案を改善する |
| 3.10 | 初期案の不足台数を補いながら改善する |
| 3.11 | 予算内のキャンペーン案を改善する |
| 3.12 | 未完成の検査計画を全被覆へ補修する |
| 3.13 | 外部案を採点し、固定追加後の個数と予算を検査する |
| 3.14 | 需要と費用が異なる複数シナリオを同じsolverで解く |
| 3.15 | ターンごとに禁止・候補・重み・費用を変え、前の解を使う |
| 3.16 | 大きな解の一部分だけを組み替える |
| 3.17 | 複数の乱数seedを試し、良い案を選ぶ |
| 3.18 | 外側の締切を共有し、完成済みの解を渡す |
| 3.19 | 実行不能と入力エラーを区別する |
| 3.20 | 大きな結果を短くデバッグ表示する |
| 3.21 | 道路グラフの到達範囲から施設候補を作る |
| 3.22 | 平面上の距離から予算付きセンサー配置を作る |

ZIPを展開したフォルダで、例01だけを試す場合は次のように実行する。ヘッダはZIPの直下にあるため、`-I.` を付ける。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined -I. examples/example01.cpp -o example01
./example01 < examples/example01.in
```

### 3.1 設置数を制限して重要な地区を広くカバーする

地区の人口や重要度を重みにし、施設を最大K個選ぶ。すべての地区を覆う必要はない。

**具体的に解く問題**

入力された候補集合からK個以下を選び、覆われる地区の重み和を大きくする。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に0以上の `K` を書く。費用を考えない場合もC個の費用を全1で入力する。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example01.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int max_selected = 0;
    std::cin >> max_selected;

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 最大K個。予算制約は付かない
    const auto answer = solver.solve_maximum_coverage(max_selected, options);

    // feasibleを確認し、固定も含む全候補IDを出力する
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
2
```

**標準出力の例**

```text
100 5
2 0 2
```

### 3.2 予算内で施策を選び、無料の施策も使う

広告や通知などの施策ごとに費用が異なる。無料の施策があっても、同じ顧客群を二重には加点しない。

**具体的に解く問題**

費用合計をB以下にしながら、施策によって到達する顧客群の重み和を大きくする。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に0以上の `B` を書く。候補数の上限は付かない。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example02.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    long long budget = 0;
    std::cin >> budget;

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // Bは費用と同じ単位。0でも無料候補は使える
    const auto answer = solver.solve_budgeted_maximum_coverage(budget, options);

    // 未被覆対象が残っても、予算内なら実行可能になり得る
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
3 3
5 7 11
0 1 0
2 1 1
3 1 2
2
```

**標準出力の例**

```text
12 2
2 0 1
```

### 3.3 全要件を検査し、テスト実行時間を減らす

テスト1件が複数の要件を検査する状況で、全要件を残しつつ総実行時間を小さくする。

**具体的に解く問題**

全対象を1回以上覆い、選択候補の費用和を小さくする。費用を全1にするとテスト件数を小さくする問題になる。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 追加の入力はない。候補費用を実行時間とする。重み0の要件も被覆必須である。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example03.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 全対象被覆が条件で、個数上限・予算上限は指定しない
    const auto answer = solver.solve_minimum_set_cover(options);

    // この用途ではcovered_weightでなくtotal_costの小ささを評価する
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
3 3
1 0 1
5 3 0 1 2
2 2 0 1
1 1 2
```

**標準出力の例**

```text
2 3
2 1 2
```

**この入力で確認する点**

入力の対象1は重み0だが、全被覆のために検査する必要がある。表示される被覆重み2と、被覆対象数3は別の量である。

### 3.4 ちょうどK件の代表データを選ぶ

代表データに含まれる特徴を広く残したいが、保存件数を必ずK件に揃える。

**具体的に解く問題**

候補が持つ特徴の重み和を大きくし、選択数をK件ちょうどにする。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に0以上の `K` を書く。候補が不足すれば実行不能になる。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example04.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int count = 0;
    std::cin >> count;

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 下限と上限を同じKにするとK個ちょうどになる
    const coverage_cardinality_bounds bounds{count, count};

    // 空被覆候補や重複被覆候補も個数補充に使われ得る
    const auto answer = solver.solve_maximum_coverage(bounds, options);

    // 同じ被覆重みの中で費用まで最小化する保証はない
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
3
```

**標準出力の例**

```text
100 5
3 0 2 4
```

**この入力で確認する点**

候補4は何も覆わず費用も0だが、「3個ちょうど」を満たすための候補として使える。

### 3.5 最低台数と最大台数を持つ観測機器を選ぶ

運用上最低L台を置く必要があり、設置枠はK台までという条件を扱う。最低台数は初期値0のままにせず明示する。

**具体的に解く問題**

L個以上K個以下の候補で被覆重みを大きくする。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に `L K` を書く。`0 <= L <= K` とする。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example05.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int lower = 0;
    int upper = 0;
    std::cin >> lower >> upper;

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 既定値は下限0、上限intの最大値
    coverage_cardinality_bounds bounds;
    // 必須の最低台数。固定候補も数える
    bounds.min_selected = lower;
    // 設置可能な最大台数。下限以上にする
    bounds.max_selected = upper;

    // この呼び出しには費用予算を指定できない
    const auto answer = solver.solve_maximum_coverage(bounds, options);

    // 選択数が足りなければ返却前に利用可能な候補で補充される
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
3 4
```

**標準出力の例**

```text
100 5
3 0 2 4
```

### 3.6 既設施設を残し、恒久的な禁止場所を避ける

移設できない既設施設は必ず残す。契約などで使えない候補も、構築したsolverの全呼び出しで禁止する。

**具体的に解く問題**

既設候補の費用も含めて予算B以内にし、禁止候補を使わず被覆重みを大きくする。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 続けて `固定数 固定ID...`、`禁止数 禁止ID...`、最後に `B`。固定と禁止は重ねない。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example06.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int fixed_count = 0;
    int forbidden_count = 0;
    std::cin >> fixed_count;
    // 恒久固定はコンストラクタより前に設定する
    problem.forced_candidates.resize(static_cast<std::size_t>(fixed_count));
    // 候補IDは0以上C未満。固定の費用も予算に含む
    for (int& id : problem.forced_candidates) {
        std::cin >> id;
    }

    std::cin >> forbidden_count;
    // 恒久禁止は後の部分問題で解除できない
    problem.forbidden_candidates.resize(static_cast<std::size_t>(forbidden_count));
    // 固定候補と同じ番号を入れない
    for (int& id : problem.forbidden_candidates) {
        std::cin >> id;
    }

    long long budget = 0;
    std::cin >> budget;

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 既設候補に加えて選ぶ。Bは追加分だけの予算ではない
    const auto answer = solver.solve_budgeted_maximum_coverage(budget, options);

    // 固定費だけでBを超える場合もfalseを検査してから出力する
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
1 3
1 0
5
```

**標準出力の例**

```text
80 3
2 2 3
```

### 3.7 そのターンだけの故障と継続稼働を指定する

同じ設備一覧を使いながら、今回稼働を続ける設備と今回故障した設備を指定する。恒久条件にせず呼び出しごとに変える。

**具体的に解く問題**

追加固定と追加禁止の下で、最大K個の設備を選ぶ。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 続けて `K`、`固定数 固定ID...`、`禁止数 禁止ID...`。各リストは一意で、固定と禁止は互いに重ねない。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example07.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int max_selected = 0;
    int fixed_count = 0;
    int forbidden_count = 0;
    std::cin >> max_selected >> fixed_count;
    std::vector<int> fixed(static_cast<std::size_t>(fixed_count));
    for (int& id : fixed) {
        std::cin >> id;
    }

    std::cin >> forbidden_count;
    std::vector<int> forbidden(static_cast<std::size_t>(forbidden_count));
    for (int& id : forbidden) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 空spanは追加条件なし、pool制限は既定でfalse
    coverage_subproblem sub;
    // fixedの実体を参照する。呼び出しが返るまでvectorを保持する
    sub.fixed_candidates = fixed;
    // 今回だけ禁止する。恒久禁止があればそれにも追加される
    sub.forbidden_candidates = forbidden;

    // 固定も含めてK個以内にする
    const auto answer = solver.solve_maximum_coverage(max_selected, options, sub);

    // このsubを渡さない次の呼び出しには、一時条件は引き継がれない
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
2
1 3
1 0
```

**標準出力の例**

```text
80 3
2 2 3
```

### 3.8 外部で絞った候補だけから追加候補を選ぶ

位置やルールによって事前に候補を絞った後、その候補pool内で組合せを選ぶ。維持する固定候補はpool外にあってよい。

**具体的に解く問題**

固定候補とpool内の候補を合わせ、最大K個で被覆重みを大きくする。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 続けて `K`、`固定数 固定ID...`、`pool数 poolID...`。各リストは一意。poolを空にすると固定候補しか選べない。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example08.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int max_selected = 0;
    int fixed_count = 0;
    int pool_count = 0;
    std::cin >> max_selected >> fixed_count;
    std::vector<int> fixed(static_cast<std::size_t>(fixed_count));
    for (int& id : fixed) {
        std::cin >> id;
    }

    std::cin >> pool_count;
    std::vector<int> pool(static_cast<std::size_t>(pool_count));
    for (int& id : pool) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 今回の候補制限だけを用意する
    coverage_subproblem sub;
    // 固定候補はpool外でも選ばれ、Kの内数になる
    sub.fixed_candidates = fixed;
    // trueにしない限りcandidate_poolは使用も検証もされない
    sub.restrict_to_candidate_pool = true;
    // 元の候補IDを指定。呼び出し中にvectorを変更しない
    sub.candidate_pool = pool;

    // 空poolは「全候補」ではない
    const auto answer = solver.solve_maximum_coverage(max_selected, options, sub);

    // 禁止候補をpoolに入れても、禁止は解除されない
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
2
1 3
2 1 2
```

**標準出力の例**

```text
80 3
2 2 3
```

**この入力で確認する点**

固定候補3はpoolの外にあるが残る。poolは固定候補を除く追加候補の範囲を指定する。

### 3.9 外部で作った最大被覆案を改善する

別のロジックが作った候補リストを出発点として渡す。比較用に、空の初期解を波括弧で渡す呼び方も示す。

**具体的に解く問題**

最大K個という制約の下で、初期案を取り込み被覆重みを改善する。初期案の候補を必ず残す指定ではない。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に `K 初期個数 初期ID...`。初期IDは一意で0以上C未満、個数はK以下にする。

**出力形式**

`from_input` と `from_empty` の見出しの後に、それぞれ次の形式で結果を出す。実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example09.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int max_selected = 0;
    int initial_count = 0;
    std::cin >> max_selected >> initial_count;
    std::vector<int> initial(static_cast<std::size_t>(initial_count));
    for (int& id : initial) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 同じseed・環境で再現しやすいよう、改善の試行量を固定する。最適性は保証されない
    options.iteration_limit = 200;

    // vectorをspanとして渡す。固定追加後もK以内が必要
    const auto from_input = solver.improve_maximum_coverage(
        max_selected, initial, options
    );

    std::cout << "from_input\n";
    // 初期案より得点が悪い候補を採用しないが、候補IDは入れ替わり得る
    print_solution(from_input);

    // initializer_list版。空でも新しい初期解を作る
    const auto from_empty = solver.improve_maximum_coverage(max_selected, {}, options);

    std::cout << "from_empty\n";
    // 二度の呼び出しは独立。前回の内部探索状態を継続するわけではない
    print_solution(from_empty);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
2 1 1
```

**標準出力の例**

```text
from_input
100 5
2 0 2
from_empty
100 5
2 0 2
```

**この入力で確認する点**

初期案の候補1をずっと残したい場合は、この例のinitialに入れるだけでは不十分である。固定条件として指定する。

### 3.10 初期案の不足台数を補いながら改善する

外部案の候補数が最低台数に届いていない場合に、下限まで補充した結果を得る。

**具体的に解く問題**

L個以上K個以下の候補で被覆重みを大きくする。初期案はL個未満でもよいがK個を超えてはいけない。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に `L K 初期個数 初期ID...`。`0 <= L <= K` とし、初期IDは一意で個数はK以下。

**出力形式**

`from_input` と `from_empty` の後に各結果を出す。実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example10.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int lower = 0;
    int upper = 0;
    int initial_count = 0;
    std::cin >> lower >> upper >> initial_count;
    std::vector<int> initial(static_cast<std::size_t>(initial_count));
    for (int& id : initial) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // Lは初期案ではなく返却解が満たす下限
    const coverage_cardinality_bounds bounds{lower, upper};

    // span版。下限未満は許すが上限超過は入力エラー
    const auto from_input = solver.improve_maximum_coverage(bounds, initial, options);

    std::cout << "from_input\n";
    // 必要なら被覆を増やさない候補でも台数を補う
    print_solution(from_input);

    // initializer_list版で空の初期案から下限まで補う
    const auto from_empty = solver.improve_maximum_coverage(bounds, {}, options);

    std::cout << "from_empty\n";
    // optionsの反復上限0でも、下限補充は省略されない
    print_solution(from_empty);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
3 3 1 1
```

**標準出力の例**

```text
from_input
100 5
3 0 2 4
from_empty
100 5
3 0 2 4
```

### 3.11 予算内のキャンペーン案を改善する

既存の施策案を改善する。固定したい施策と、初期案として参考に渡す施策を区別する。

**具体的に解く問題**

予算B以内の初期案を渡し、その条件で被覆重みを大きくする。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に `B 初期個数 初期ID...`。初期IDは一意で予算内。この例の比較用 `{0}` も有効にするためCは1以上、候補0の費用はB以下。

**出力形式**

`from_input` と `from_literal` の後に各結果を出す。実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example11.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    long long budget = 0;
    int initial_count = 0;
    std::cin >> budget >> initial_count;
    std::vector<int> initial(static_cast<std::size_t>(initial_count));
    for (int& id : initial) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期案との比較に加えて有限回の改善も試す。最適性は保証されない
    options.iteration_limit = 200;

    // span版。予算超過の初期案を自動で削るAPIではない
    const auto from_input = solver.improve_budgeted_maximum_coverage(
        budget, initial, options
    );

    std::cout << "from_input\n";
    // 最初にfeasibleを確認してから採用候補と費用を使う
    print_solution(from_input);

    // initializer_list版。候補0を参考案とし、固定にはしない
    const auto from_literal = solver.improve_budgeted_maximum_coverage(
        budget, {0}, options
    );

    std::cout << "from_literal\n";
    // 重みが同じなら費用、その次に候補数が少ない解を優先する
    print_solution(from_literal);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
5 1 1
```

**標準出力の例**

```text
from_input
100 5
2 0 2
from_literal
100 5
2 0 2
```

### 3.12 未完成の検査計画を全被覆へ補修する

一部の要件だけを検査する案から、足りない要件の検査を追加する。不要になった非固定の検査は除かれ得る。

**具体的に解く問題**

未被覆対象がある初期集合を受け取り、全対象被覆と費用削減を目指す。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に `初期個数 初期ID...`。初期IDは一意。すべての対象を覆っている必要はない。

**出力形式**

`repaired` と `from_empty` の後に各結果を出す。実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example12.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int initial_count = 0;
    std::cin >> initial_count;
    std::vector<int> initial(static_cast<std::size_t>(initial_count));
    for (int& id : initial) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // span版。未完成の初期集合も受け取る
    const auto repaired = solver.improve_minimum_set_cover(initial, options);

    std::cout << "repaired\n";
    // 実行不能または期限切れならfalseの可能性がある
    print_solution(repaired);

    // initializer_list版。空集合も補修対象として指定できる
    const auto from_empty = solver.improve_minimum_set_cover({}, options);

    std::cout << "from_empty\n";
    // 重み0の対象も覆う必要がある。費用の小ささを評価する
    print_solution(from_empty);

    return 0;
}
```

**入力例**

```text
3 3
1 0 1
5 3 0 1 2
2 2 0 1
1 1 2
1 1
```

**標準出力の例**

```text
repaired
2 3
2 1 2
from_empty
2 3
2 1 2
```

### 3.13 外部案を採点し、固定追加後の個数と予算を検査する

別の処理が作った案を変更せず採点する。固定候補を入力リストへ書き忘れても自動追加されるため、返却された個数と費用で判定する。

**具体的に解く問題**

候補0を恒久固定として外部集合を評価し、個数上限K、予算B、全被覆をそれぞれ独立に検査する。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 Cは1以上。最後に `K B 外部案の個数 ID...`。IDは一意で0以上C未満。候補0は外部案から省略してよい。

**出力形式**

1行目はTとC。2行目は外部案の評価結果。3行目は「個数内・予算内・全被覆」の順に真なら1、偽なら0。4行目は固定候補だけの評価結果。これらの検査を同時に行っても、solveが個数と予算の同時制約に対応するわけではない。

**実行コード：`example13.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    // 候補0を必ず含めて採点する。入力はC>=1を前提とする
    problem.forced_candidates = {0};
    int max_selected = 0;
    int selected_count = 0;
    long long budget = 0;
    std::cin >> max_selected >> budget >> selected_count;
    std::vector<int> selected(static_cast<std::size_t>(selected_count));
    for (int& id : selected) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);
    // 構築時のTとC。poolや前処理後の個数ではない
    std::cout << solver.target_count() << ' ' << solver.candidate_count() << '\n';

    // span版。候補0を追加し、重み・費用・個数を再計算する
    const auto value = solver.evaluate(selected);
    // coverage_evaluation用のoperator<<で全5項目を表示する
    std::cout << value << '\n';
    // 手元のselected.size()ではなく、固定追加後の個数で判定
    const bool count_ok = value.selected_count <= max_selected;
    // 評価するだけなら予算超過でも例外にはならない
    const bool budget_ok = value.total_cost <= budget;
    // 全被覆が必要な用途でだけ、この条件を要求する
    const bool complete = value.uncovered_target_count == 0;
    std::cout << count_ok << ' ' << budget_ok << ' ' << complete << '\n';
    // initializer_list版。空を渡しても固定候補0だけは加えて評価する
    std::cout << solver.evaluate({}) << '\n';

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
2 5 1 2
```

**標準出力の例**

```text
4 5
coverage_evaluation{covered_weight=100, total_cost=5, covered_target_count=4, uncovered_target_count=0, selected_count=2}
1 1 1
coverage_evaluation{covered_weight=30, total_cost=3, covered_target_count=2, uncovered_target_count=2, selected_count=1}
```

**この入力で確認する点**

外部案は候補2だけだが、評価時に固定候補0が加わるため選択数は2になる。空集合を評価した行にも固定候補0の値が現れる。

### 3.14 需要と費用が異なる複数シナリオを同じsolverで解く

サービス対象の関係は同じまま、朝・昼・夜などの需要と費用を切り替える。再構築せず各シナリオを独立に解く。

**具体的に解く問題**

シナリオごとの重み・費用・予算を用い、予算付き最大被覆を繰り返し解く。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 続けてシナリオ数Q。各シナリオは `B`、T個の重み、C個の費用の順。すべて非負で各総和はlong longの範囲内。

**出力形式**

各シナリオについて `scenario 番号` の後に結果を出す。実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example14.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    int queries = 0;
    std::cin >> queries;
    for (int q = 0; q < queries; ++q) {
        long long budget = 0;
        std::cin >> budget;
        // 差し替えは変更部分だけでなくT個全部を用意する
        std::vector<long long> weights(static_cast<std::size_t>(solver.target_count()));
        // 費用もC個全部が必要
        std::vector<long long> costs(
            static_cast<std::size_t>(solver.candidate_count())
        );
        for (auto& weight : weights) {
            std::cin >> weight;
        }

        for (auto& cost : costs) {
            std::cin >> cost;
        }

        // このシナリオ専用。前回の条件は自動では残らない
        coverage_subproblem sub;
        // 空は「差し替えなし」。全0へ変更するならT個の0を渡す
        sub.target_weights_override = weights;
        // vectorはsolveが返るまでこのスコープに保持する
        sub.candidate_costs_override = costs;

        // 被覆関係は元のままで新しい数値を使う
        const auto answer = solver.solve_budgeted_maximum_coverage(
            budget, options, sub
        );

        std::cout << "scenario " << q << '\n';
        // 重みが違うシナリオ間のcovered_weightを同じ基準だと考えて比較しない
        print_solution(answer);
    }

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
2
5
10 20 30 40
3 4 2 1 0
3
100 0 0 0
3 4 2 1 0
```

**標準出力の例**

```text
scenario 0
100 5
2 0 2
scenario 1
100 1
1 3
```

**この入力で確認する点**

シナリオ1では対象0だけに価値がある。重み0の対象を覆っても追加点はないが、対象そのものが削除されたわけではない。

### 3.15 ターンごとに禁止・候補・重み・費用を変え、前の解を使う

故障や需要変化が毎ターン起きる。前の案をそのまま渡すと禁止違反や予算超過になるため、今回の条件で再検査してから改善する。

**具体的に解く問題**

前回解から利用不能な候補を除き、現在費用で予算内ならimprove、予算超過なら新規solveへ切り替える。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 続けて `初回予算 ターン数Q`。各ターンは `B`、`禁止数 ID...`、`pool数 ID...`、T個の重み、C個の費用。リストは一意で0以上C未満。この例では恒久固定・恒久禁止は設定しない。

**出力形式**

最初に `initial` と初回結果。以後 `turn 番号` と各ターン結果を出す。実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example15.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    long long budget = 0;
    int turns = 0;
    std::cin >> budget >> turns;

    // 初回は構築時の重みと費用で解く
    auto answer = solver.solve_budgeted_maximum_coverage(budget, options);

    std::cout << "initial\n";
    // 出力した候補IDは次ターンにも使う
    print_solution(answer);

    // 不実行可能な結果は初期案として保存しない
    std::vector<int> previous = answer.feasible
        ? answer.selected_candidates
        : std::vector<int>{};
    for (int turn = 0; turn < turns; ++turn) {
        int forbidden_count = 0;
        int pool_count = 0;
        std::cin >> budget >> forbidden_count;
        std::vector<int> forbidden(static_cast<std::size_t>(forbidden_count));
        for (int& id : forbidden) {
            std::cin >> id;
        }

        std::cin >> pool_count;
        std::vector<int> pool(static_cast<std::size_t>(pool_count));
        for (int& id : pool) {
            std::cin >> id;
        }

        std::vector<long long> weights(static_cast<std::size_t>(targets));
        std::vector<long long> costs(static_cast<std::size_t>(candidates));
        for (auto& weight : weights) {
            std::cin >> weight;
        }

        for (auto& cost : costs) {
            std::cin >> cost;
        }

        // 参照するvectorはループ末尾まで保持する
        coverage_subproblem sub;
        // 今回だけの禁止を指定する
        sub.forbidden_candidates = forbidden;
        // 空poolなら今回はどの非固定候補も使えない
        sub.restrict_to_candidate_pool = true;
        // 今回の利用可能候補を元のIDで指定する
        sub.candidate_pool = pool;
        // 現在の重みをT個渡す
        sub.target_weights_override = weights;
        // 現在の費用をC個渡す
        sub.candidate_costs_override = costs;

        std::vector<unsigned char> allowed(static_cast<std::size_t>(candidates), 0);
        for (int id : pool) {
            allowed[id] = 1;
        }

        for (int id : forbidden) {
            allowed[id] = 0;
        }

        std::vector<int> initial;
        for (int id : previous) {
            if (allowed[id]) {
                initial.push_back(id);
            }
        }

        // 前回のtotal_costを使わず、同じsubで現在費用を評価する
        const auto value = solver.evaluate(initial, sub);
        // 予算内の初期解だけをimproveへ渡す
        if (value.total_cost <= budget) {
            // 禁止・pool違反も事前に取り除いた
            answer = solver.improve_budgeted_maximum_coverage(
                budget, initial, options, sub
            );
        } else {
            // 予算超過の初期案は使わず、今回の条件で作り直す
            answer = solver.solve_budgeted_maximum_coverage(budget, options, sub);
        }

        std::cout << "turn " << turn << '\n';
        // 今回の重み・費用による結果である
        print_solution(answer);
        // 次のターンへ渡す候補IDだけを保存する
        previous = answer.feasible
            ? answer.selected_candidates
            : std::vector<int>{};
    }

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
5 3
5
1 2
4 0 1 3 4
10 20 30 40
3 4 2 1 0
5
0
5 0 1 2 3 4
10 20 30 40
6 6 6 6 0
5
0
5 0 1 2 3 4
10 20 30 40
3 4 2 1 0
```

**標準出力の例**

```text
initial
100 5
2 0 2
turn 0
60 5
2 1 3
turn 1
0 0
0
turn 2
100 5
2 0 2
```

**この入力で確認する点**

turn 0では候補2が禁止され、前回の候補列から除かれる。turn 1では前回案の現在費用が予算を超えるため新規solveへ切り替わる。turn 2では費用が戻り、空の前回案から再び改善する。

### 3.16 大きな解の一部分だけを組み替える

外側の探索で「今回はここだけ動かす」と決めたとき、動かさない部分を固定して小さな候補集合を解き直す。

**具体的に解く問題**

既存のK個の候補のうちkeepを残し、残りをpool内の候補へ入れ替えながらK個ちょうどを維持する。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 続けて `K`、`初期個数 ID...`、`固定数 ID...`、`pool数 ID...`。初期個数はK、固定は初期集合の部分集合、初期の非固定候補はpoolに含める。各リストは一意。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example16.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int count = 0;
    int initial_count = 0;
    int keep_count = 0;
    int pool_count = 0;
    std::cin >> count >> initial_count;
    std::vector<int> initial(static_cast<std::size_t>(initial_count));
    for (int& id : initial) {
        std::cin >> id;
    }

    std::cin >> keep_count;
    std::vector<int> keep(static_cast<std::size_t>(keep_count));
    for (int& id : keep) {
        std::cin >> id;
    }

    std::cin >> pool_count;
    std::vector<int> pool(static_cast<std::size_t>(pool_count));
    for (int& id : pool) {
        std::cin >> id;
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 外側の探索へ戻れるよう、部分問題の改善量を制限する。最適性は保証されない
    options.iteration_limit = 300;

    // 恒久条件を変えず、今回の近傍だけを制約する
    coverage_subproblem sub;
    // この候補群は初期解と違って削除されない
    sub.fixed_candidates = keep;
    // 非固定部分の移動先をpool内に限定する
    sub.restrict_to_candidate_pool = true;
    // 固定keepはpool外でもよいが、非固定の初期候補はpool内にする
    sub.candidate_pool = pool;

    // 局所部分の個数ではなく固定も含む全体のK
    const coverage_cardinality_bounds bounds{count, count};

    // 初期案を比較対象として含め、K個を維持する
    const auto answer = solver.improve_maximum_coverage(bounds, initial, options, sub);

    // 固定候補を外側で再結合する必要はなく、結果には既に含まれる
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
3
3 0 1 2
1 0
4 1 2 3 4
```

**標準出力の例**

```text
100 5
3 0 2 4
```

**この入力で確認する点**

結果は固定部分と動かした部分を合わせた全体の候補列である。固定部分を後からもう一度追加すると重複してしまう。

### 3.17 複数の乱数seedを試し、良い案を選ぶ

同じ問題へ異なるseedを使う。構築を繰り返さず、solverを1個再利用して比較する。

**具体的に解く問題**

各seedで同じ予算・同じ反復上限を使い、被覆重み、費用、候補数の順に良い結果を残す。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 続けて `B seed数 seed...`。seed数は1以上、各seedはuint64_tの範囲内。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example17.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    long long budget = 0;
    int seed_count = 0;
    std::cin >> budget >> seed_count;

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 全seedを同じ有限の試行回数で比較する。最適性は保証されない
    options.iteration_limit = 500;

    // 既定ではfeasible=falseなので、最初の実行可能解で更新する
    coverage_solution best;
    for (int i = 0; i < seed_count; ++i) {
        std::uint64_t seed = 0;
        std::cin >> seed;
        // 問題条件を揃え、乱数系列だけを変える
        options.seed = seed;

        // 各回は独立した探索になる
        const auto current = solver.solve_budgeted_maximum_coverage(budget, options);
        // 実行不能な結果は比較に含めない
        if (!current.feasible) {
            continue;
        }

        // 重み最大・費用最小・個数最小
        const auto key = std::tuple{
            // 被覆重みは大きいほど優先する
            current.covered_weight,
            // 費用は符号を反転し、小さい費用ほど優先する
            -current.total_cost,
            // 個数も符号を反転し、少ない候補ほど優先する
            -static_cast<long long>(current.selected_candidates.size())
        };

        // 同じ比較基準を既存の最良解にも使う
        const auto best_key = std::tuple{
            // 被覆重みは大きいほど優先する
            best.covered_weight,
            // 費用は符号を反転し、小さい費用ほど優先する
            -best.total_cost,
            // 個数も符号を反転し、少ない候補ほど優先する
            -static_cast<long long>(best.selected_candidates.size())
        };
        // 結果はvectorを所有しているため次のsolve後も保持できる
        if (!best.feasible || key > best_key) {
            best = current;
        }
    }

    // iterationsは採用した1回の値で、全seedの合計ではない
    print_solution(best);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
5 4 0 1 2 3
```

**標準出力の例**

```text
100 5
2 0 2
```

### 3.18 外側の締切を共有し、完成済みの解を渡す

外側の処理に終了時刻があり、残り時間で改善する。まず得た実行可能案を渡して、締切が近い場合の返却結果も確認する。

**具体的に解く問題**

最大K個の初期案を作成後、相対時間と絶対締切を設定して改善する。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 最後に `K 残り時間ms`。どちらも0以上。例の0msは既に到来した締切を表す再現可能なケース。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example18.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    int max_selected = 0;
    int remaining_ms = 0;
    std::cin >> max_selected >> remaining_ms;

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // まず反復0で初期解を作る。この時間は後で置く締切より前
    const auto initial = solver.solve_maximum_coverage(max_selected, options);
    // 初期実行可能解がない場合は、その結果を返す
    if (!initial.feasible) {
        // falseの候補列をそのまま有効解にしない
        print_solution(initial);

        return 0;
    }

    // 1回の改善に割り当てる相対時間。0でも初期解の検証などは行う
    options.time_limit_ms = remaining_ms;
    // 時間以外にも有限の反復上限を設ける
    options.iteration_limit = 500;
    // 実戦では外側で最初に決めた締切を代入する
    options.deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(remaining_ms);

    // 完成済み候補を明示的に引き継ぐ
    const auto answer = solver.improve_maximum_coverage(
        max_selected, initial.selected_candidates, options
    );

    // deadlineはhard limitではなく、検証・補充などの超過分に余裕を残す
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
4 5
10 20 30 40
3 2 0 1
4 2 1 2
2 2 2 3
1 1 0
0 0
2 0
```

**標準出力の例**

```text
100 5
2 0 2
```

**この入力で確認する点**

この入力では残り時間0なので、初期案を検証して比較する経路を確認できる。正の時間へ変更した場合、停止反復数や結果は実行時の負荷にも依存する。

### 3.19 実行不能と入力エラーを区別する

覆えない対象があることと、候補IDを重複して渡したことは別の問題である。前者は結果のfeasible、後者は例外で分ける。

**具体的に解く問題**

全被覆を試みた後、意図的に重複した評価リストを渡し、入力エラーを捕捉する。

**入力形式**

先頭は `T C`。次にT個の非負重み、続くC行は候補番号順に `費用 対象数 対象番号...` を書く。T・C・対象数は非負、対象番号は0以上T未満。費用は非負で、全重み和・全費用和はlong longの範囲内とする。 追加の入力はない。候補0を使うためCは1以上。入力例は対象1を覆う候補が存在しない。

**出力形式**

全被覆可能なら `FEASIBLE`、不能なら `INFEASIBLE`。続いて、意図的に渡した重複IDを検出して `INVALID_INPUT`。

**実行コード：`example19.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 入力する配列長・番号は、この例の入力条件を満たすものとする
// 対象T個・候補C個の入力を作る
void read_problem(coverage_problem& problem, int targets, int candidates) {
    // 対象番号順にT個の重みを用意する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    // 重みは非負。重複被覆でも1回分だけ加点する
    for (auto& weight : problem.target_weights) {
        std::cin >> weight;
    }

    // 個数制約でも費用配列は必須
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 外側の添字が元の候補番号になる
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        int count = 0;
        // 候補cの非負費用と被覆対象数を読む
        std::cin >> problem.candidate_costs[c] >> count;
        // 空の被覆集合も指定できる
        problem.covered_targets[c].resize(static_cast<std::size_t>(count));
        // 各番号は0以上T未満。重複は構築時に除かれる
        for (int& target : problem.covered_targets[c]) {
            std::cin >> target;
        }
    }
}

int main() {
    int targets = 0;
    int candidates = 0;
    std::cin >> targets >> candidates;

    // 全配列は空。固定・禁止も指定しなければ存在しない
    coverage_problem problem;
    // 重みT個、費用C個、候補ごとの被覆関係を用意する
    read_problem(problem, targets, candidates);

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 覆えない対象があると、例外ではなくfalseを返す
    const auto answer = solver.solve_minimum_set_cover(options);
    // 最適性ではなく全被覆の可否を表示する
    std::cout << (answer.feasible ? "FEASIBLE" : "INFEASIBLE") << '\n';

    try {
        // 意図的な不正入力。評価リスト内で同じ候補を2回指定してはいけない
        const auto invalid = solver.evaluate({0, 0});
        // この例では到達しない。正しい入力なら評価値を表示できる
        std::cout << invalid << '\n';
    } catch (const std::invalid_argument&) {
        std::cout << "INVALID_INPUT\n";
    }

    return 0;
}
```

**入力例**

```text
2 1
1 1
1 1 0
```

**標準出力の例**

```text
INFEASIBLE
INVALID_INPUT
```

**この入力で確認する点**

全被覆不能なsolveがfalseを返した後でも、evaluateは渡された集合を個別に検査する。初期解の形式検査だけを目的にimproveへ渡すことは避ける。

### 3.20 大きな結果を短くデバッグ表示する

候補を大量に選んだとき、全配列をログへ出さずに概要だけを確認する。提出用の標準出力と診断用の標準エラーも分ける。

**具体的に解く問題**

N対象・N候補で、候補iが対象iだけを覆う問題を作り、全N個を選んだ結果を表示する。

**入力形式**

入力は非負整数 `N` だけ。例では25。実用上メモリに収まる大きさを使う。

**出力形式**

標準出力は評価結果の1行。標準エラー出力は探索結果の1行で、選択候補が20個を超えた分だけ省略する。

**実行コード：`example20.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

int main() {
    int count = 0;
    std::cin >> count;

    // 固定・禁止は指定しない
    coverage_problem problem;
    // N対象をすべて重み1にする
    problem.target_weights.assign(static_cast<std::size_t>(count), 1);

    // N候補をすべて費用1にする
    problem.candidate_costs.assign(static_cast<std::size_t>(count), 1);
    // 候補iが持つ対象一覧をN個用意する
    problem.covered_targets.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        // 候補番号と対象番号をこの例だけ1対1に対応させる
        problem.covered_targets[i] = {i};
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // 必ずN個を選ぶ問題
    const auto answer = solver.solve_maximum_coverage(
        coverage_cardinality_bounds{count, count}, options
    );
    // solutionは先頭20候補と省略件数を表示。結果の中のvectorは切り詰めない
    std::cerr << answer << '\n';

    // 全N個を再評価する。表示の省略は計算に影響しない
    const auto value = solver.evaluate(answer.selected_candidates);
    std::ostringstream buffer;
    // evaluationもそのままstreamへ渡せる。演算子自身は改行しない
    buffer << value;
    std::cout << buffer.str() << '\n';

    return 0;
}
```

**入力例**

```text
25
```

**標準出力の例**

```text
coverage_evaluation{covered_weight=25, total_cost=25, covered_target_count=25, uncovered_target_count=0, selected_count=25}
```

**標準エラー出力の例**

```text
coverage_solution{feasible=true, covered_weight=25, total_cost=25, covered_target_count=25, uncovered_target_count=0, iterations=0, selected_candidates(size=25)=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (+5 more)]}
```

**この入力で確認する点**

標準エラーでは候補20～24の5件が省略される。一方、評価結果のselected_countは25のままである。

### 3.21 道路グラフの到達範囲から施設候補を作る

道路上の距離をそのままライブラリへ渡すのではなく、施設候補から半径以内にある重要頂点を対象一覧へ変換する。

**具体的に解く問題**

無向・全辺長1のグラフで、候補頂点からR本以内の辺で届く重要頂点を覆い、施設を最大Kか所選ぶ。

**入力形式**

先頭は `頂点数N 辺数M 対象数T 候補数C 半径R 上限K`。Nは1以上、他は0以上。次のT行は一意な `重要頂点番号 重み`、次にC個の一意な候補頂点番号、続くM行は `a b` の無向辺。頂点番号は0以上N未満、全重み和はlong long内。

**出力形式**

1行目は被覆重要度。2行目は `施設数 設置するグラフ頂点番号...`。順番は候補ID順であり、グラフ頂点番号の昇順とは限らない。

**実行コード：`example21.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

int main() {
    int vertices = 0;
    int edges = 0;
    int targets = 0;
    int candidates = 0;
    int radius = 0;
    int limit = 0;
    std::cin >> vertices >> edges >> targets >> candidates >> radius >> limit;

    // 候補と対象は、グラフの頂点番号とは別の連番にする
    coverage_problem problem;
    // 対象の並び順と重要度の対応を保持する
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    std::vector<int> target_vertex(static_cast<std::size_t>(targets));
    for (int t = 0; t < targets; ++t) {
        // 対象番号tの頂点と非負重要度
        std::cin >> target_vertex[t] >> problem.target_weights[t];
    }

    std::vector<int> candidate_vertex(static_cast<std::size_t>(candidates));
    for (int& vertex : candidate_vertex) {
        std::cin >> vertex;
    }

    std::vector<std::vector<int>> graph(static_cast<std::size_t>(vertices));
    for (int e = 0; e < edges; ++e) {
        int a = 0;
        int b = 0;
        std::cin >> a >> b;
        graph[a].push_back(b);
        graph[b].push_back(a);
    }

    // 個数制約でも費用C個を必ず用意する
    problem.candidate_costs.assign(static_cast<std::size_t>(candidates), 1);
    // 各施設候補から届く対象番号を格納する
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        std::vector<int> distance(static_cast<std::size_t>(vertices), -1);
        std::queue<int> queue;
        distance[candidate_vertex[c]] = 0;
        queue.push(candidate_vertex[c]);
        while (!queue.empty()) {
            const int v = queue.front();
            queue.pop();
            if (distance[v] == radius) {
                continue;
            }

            for (int to : graph[v]) {
                if (distance[to] != -1) {
                    continue;
                }

                distance[to] = distance[v] + 1;
                queue.push(to);
            }
        }

        for (int t = 0; t < targets; ++t) {
            if (distance[target_vertex[t]] != -1) {
                // グラフ頂点番号ではなく対象番号tを入れる
                problem.covered_targets[c].push_back(t);
            }
        }
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // R以内の被覆関係へ変換した後で、最大K施設を選ぶ
    const auto answer = solver.solve_maximum_coverage(limit, options);
    // 制約を満たす解だけを配置へ反映する
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";

        return 0;
    }

    // 被覆した重要頂点の重み合計
    std::cout << answer.covered_weight << '\n';
    // 選んだ施設の個数
    std::cout << answer.selected_candidates.size();
    // 元のグラフ頂点番号へ戻して出力する
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << candidate_vertex[id];
    }
    std::cout << '\n';

    return 0;
}
```

**入力例**

```text
5 4 3 2 1 2
0 5
2 7
4 11
1 3
0 1
1 2
2 3
3 4
```

**標準出力の例**

```text
23
2 1 3
```

**この入力で確認する点**

候補IDは0と1だが、実際に施設を置くグラフ頂点は1と3である。出力時に対応表を使って戻している。

### 3.22 平面上の距離から予算付きセンサー配置を作る

各センサーの円内にある測定地点を対象一覧へ変換する。距離そのものを最小化する問題ではなく、半径以内かどうかの被覆問題にする。

**具体的に解く問題**

半径Rのセンサーを候補位置へ配置し、費用合計B以内で被覆地点の重み和を大きくする。

**入力形式**

先頭は `T C R B`。T・Cは非負、0以上のRは10^9以下、Bは非負。次のT行は `x y 重み`、次のC行は `x y 費用`。座標は絶対値10^9以下の整数。重みと費用は非負で各総和はlong long内。

**出力形式**

実行可能なら、1行目に `被覆重み 総費用`、2行目に `選択個数 候補番号...` を出す。候補番号は0始まりの昇順。実行不能なら `INFEASIBLE` の1行だけを出す。

**実行コード：`example22.cpp`**

```cpp
// このヘッダを同じフォルダかinclude検索先に置く
#include "maximum_coverage_solver_v08.hpp"

// 返却値は配列も含めて独立に保持できる
void print_solution(const coverage_solution& answer) {
    // falseの部分解は有効な解として採用しない
    if (!answer.feasible) {
        std::cout << "INFEASIBLE\n";
        return;
    }

    // 1行目は被覆重みと総費用
    std::cout << answer.covered_weight << ' ' << answer.total_cost << '\n';
    // 2行目の先頭は固定候補を含む選択個数
    std::cout << answer.selected_candidates.size();
    // 続けて元の候補番号を昇順に出す
    for (int id : answer.selected_candidates) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int targets = 0;
    int candidates = 0;
    long long radius = 0;
    long long budget = 0;
    std::cin >> targets >> candidates >> radius >> budget;

    // 点の座標は利用側で持ち、被覆対象の番号だけを渡す
    coverage_problem problem;
    // 測定地点ごとの非負重要度
    problem.target_weights.resize(static_cast<std::size_t>(targets));
    std::vector<std::pair<long long, long long>> points(
        static_cast<std::size_t>(targets)
    );
    for (int t = 0; t < targets; ++t) {
        // 地点tのx,yと重み
        std::cin >> points[t].first >> points[t].second >> problem.target_weights[t];
    }

    // センサーの設置費用をC個用意する
    problem.candidate_costs.resize(static_cast<std::size_t>(candidates));
    // 各センサーが覆う地点番号の一覧
    problem.covered_targets.resize(static_cast<std::size_t>(candidates));
    for (int c = 0; c < candidates; ++c) {
        long long x = 0;
        long long y = 0;
        // 候補cの設置位置と非負費用
        std::cin >> x >> y >> problem.candidate_costs[c];
        for (int t = 0; t < targets; ++t) {
            const long long dx = x - points[t].first;
            const long long dy = y - points[t].second;
            if (dx * dx + dy * dy <= radius * radius) {
                // 半径の境界も被覆。座標の絶対値は10^9以下という入力条件で計算する
                problem.covered_targets[c].push_back(t);
            }
        }
    }

    // 入力をコピーして構築。problemを書き換えてもsolverへは反映されない
    coverage_solver solver(problem);

    // 省略時は100ms、seed=1、反復上限と絶対締切は実質無効
    coverage_solver_options options;
    // 相対時間で止めず、この例では次の有限の反復上限で止める
    options.time_limit_ms = -1;
    // 初期解は作るがランダム改善はしない。最適性は保証されない
    options.iteration_limit = 0;

    // Bは設置費用と同じ単位。候補数上限は付かない
    const auto answer = solver.solve_budgeted_maximum_coverage(budget, options);

    // 出力IDは候補座標を入力した順番の0始まり番号
    print_solution(answer);

    return 0;
}
```

**入力例**

```text
3 3 1 4
0 0 5
2 0 7
4 0 11
1 0 2
3 0 2
0 0 1
```

**標準出力の例**

```text
23 3
2 1 2
```

**この入力で確認する点**

半径1の候補0と1を使うと3地点を覆える。地点1は両方の円に入るが、重み7を2回加算しない。

### 3.23 APIと例の対応・検証方法

| API | 掲載例 |
|---|---|
| coverage_solverのコンストラクタ | 全例 |
| `solve_maximum_coverage(int)` | 1、7、8、18、21 |
| `solve_maximum_coverage(bounds)` | 4、5、20 |
| `improve_maximum_coverage(int, span)` | 9、18 |
| `improve_maximum_coverage(int, initializer_list)` | 9 |
| `improve_maximum_coverage(bounds, span)` | 10、16 |
| `improve_maximum_coverage(bounds, initializer_list)` | 10 |
| `solve_budgeted_maximum_coverage` | 2、6、14、15、17、22 |
| `improve_budgeted_maximum_coverage(span)` | 11、15 |
| `improve_budgeted_maximum_coverage(initializer_list)` | 11 |
| `solve_minimum_set_cover` | 3、19 |
| `improve_minimum_set_cover(span)` | 12 |
| `improve_minimum_set_cover(initializer_list)` | 12 |
| `evaluate(span)` | 13、15、20 |
| `evaluate(initializer_list)` | 13、19 |
| `target_count()` | 13、14 |
| `candidate_count()` | 13、14 |
| `operator<<(coverage_solution)` | 20 |
| `operator<<(coverage_evaluation)` | 13、19、20 |

ここでspan版へ渡しているvectorは、標準の変換によって `std::span<const int>` として扱われる。配布ZIPには、ヘッダ、22本のソース、各入力・期待出力、再検証用スクリプトを収録している。次のコマンドで単独コンパイルと入出力・制約の照合を再実行できる。Pythonは標準ライブラリだけを使用する。

```bash
python3 verify_examples.py
python3 verify_examples.py --sanitize
```

掲載コードはC++20・GCC 13.3で、指定の警告オプションと `-Werror` を付けて検証した。GCC 12.2そのものはこの環境では未検証である。API宣言16定義はヘッダと機械照合し、全ライブラリ利用行のコメント、標準出力・標準エラーの区別、掲載ソースと同梱ソースの一致も確認している。境界入力には空問題、予算0、台数不足、空pool、締切0、表示件数20・21、孤立頂点、座標の境界を含む。サニタイザーでの結果は同梱の `VALIDATION.txt` に記載する。
