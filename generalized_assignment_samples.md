# 一般化割当ソルバー サンプルコード集

対象ヘッダーは `generalized_assignment_solver_v13.hpp`、公開クラスは `generalized_assignment_solver` です。C++20とGCCを使います。本文ではこの型を `Gap` と略記します。

1章で問題の意味を確認し、2章をAPI辞書として使い、3章から目的に合うプログラムをコピーできます。各プログラムはそれぞれ独立した `main` を持ち、標準入力・標準出力まで含みます。別のサンプルの関数をコピーする必要はありません。

## 1. 何を解くライブラリか？

### 1.1 仕事ごとに1つの選択肢を選ぶ

例えば、複数の仕事を作業員へ割り当てる場面を考えます。同じ仕事でも、作業員によって所要時間や費用が異なります。作業員ごとの勤務時間を超えず、全仕事の費用をできるだけ小さくしたい、という問題です。

このライブラリでは「作業員」を一般に**割当先**と呼びます。候補1個は「どの仕事を、どの割当先へ、いくらの容量を使い、いくらの費用で置くか」という情報です。各仕事について、登録した候補からちょうど1個を選びます。

仕事は配送先・品物・顧客・処理要求などに、割当先は倉庫・箱・施設・設備などに読み替えられます。容量は個数・重さ・時間など1種類の量です。需要とは、その候補を選んだときに使う容量です。費用と需要の単位は異なって構いません。

登録しなかった候補は使えません。同じ仕事を同じ割当先へ置く場合でも、モードごとに需要・費用の異なる候補を複数登録できます。候補を区別する番号が候補IDです。

### 1.2 目的関数と容量条件

仕事iが選ぶ候補IDを $x_ {i}$ とします。最小化したい費用は次です。

$$z(x)=\sum_ {i=0}^{N-1}c_ {x_ {i}}$$

仕事ごとの選択条件は次です。

$$x_ {i}\in O_ {i}\quad(0\le i<N)$$

各割当先aで使う容量の合計を $L_ {a}(x)$ とすると、次の上限を守ります。

$$L_ {a}(x)=\sum_ {i:p_ {x_ {i}}=a}d_ {x_ {i}}\le C_ {a}\quad(0\le a<A)$$

| 記号 | 意味 | コードでの対応 |
| --- | --- | --- |
| $N$ | 仕事の個数 | `jobs`、`job_count()` |
| $A$ | 割当先の個数 | `agents`、`agent_count()` |
| $i$ | 仕事番号。0以上N未満 | `job` |
| $a$ | 割当先番号。0以上A未満 | `agent` |
| $o$ | 登録候補1個の番号 | `add_option` が返す候補ID |
| $O_ {i}$ | 仕事iに登録された候補IDの集合 | 第1引数をiとして登録した候補 |
| $p_ {o}$ | 候補oの割当先 | `assignment_option::agent` |
| $d_ {o}$ | 候補oが使う容量。0以上 | `assignment_option::demand` |
| $c_ {o}$ | 候補oの費用。負でもよい | `assignment_option::cost` |
| $C_ {a}$ | 割当先aの容量。0以上 | `capacities[a]` |
| $x_ {i}$ | 仕事iが選んだ候補ID | `result.option_ids[i]` |
| $x$ | 全仕事の候補IDを並べた割当 | `result.option_ids` 全体 |
| $L_ {a}(x)$ | 割当先aで使う容量の合計 | `result.load_of_agent[a]` |
| $z(x)$ | 割当全体の費用合計 | 完全解の `result.objective` |

目的関数は「選んだ候補の費用を、全仕事について1回ずつ足す」という意味です。容量の式は「割当先がaである仕事だけの需要を足す」という意味です。複数の候補を少しずつ混ぜることはできません。

例えば、仕事0を候補0で置くと需要2・費用5、仕事1を候補1で同じ割当先へ置くと需要1・費用3なら、使用量は3、費用は8です。その割当先の容量が3以上なら容量条件を満たし、2なら満たしません。

### 1.3 そのまま扱える条件と、変換が必要な条件

| 条件 | 扱い方 |
| --- | --- |
| 仕事ごと・割当先ごとに需要が異なる | 各候補の `demand` を変える |
| 負の費用や利益最大化 | 負費用も登録可能。利益最大化は費用を負の利益にする |
| 禁止された組合せ | その候補を登録しない |
| 同じ割当先の異なる実行モード | 同じjob・agentに複数候補を登録する |
| 仕事を採用しない選択 | 需要0の未採用先などを明示的に追加する |
| すでに決めた仕事を固定 | 可変仕事だけを指定して `repair` する |
| 設備×時間枠のような組ごとの容量 | 組を1個の割当先番号へ変換する |
| 1候補がCPUとメモリの両方を消費 | 直接は扱えない。容量は割当先ごとに1種類 |
| 仕事の分割、先行関係、順序、時間窓内の細かい開始時刻 | 直接は扱えない |
| 開設固定費、仕事ペアの相性、非線形な混雑費用 | 候補費用の単純な和ではない。外側での問題分解や再評価が必要 |

容量と需要は整数なので、小数の時間などは単位を揃えて整数化します。例えば0.1分単位で管理するなら、容量も需要も同じ倍率で変換します。

## 2. APIリファレンス

### 2.1 利用環境・型・番号

各サンプルを `sample.cpp` として保存し、同じディレクトリへ対象ヘッダーを置きます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined sample.cpp -o sample
./sample < input.txt
```

確認環境はGCC 13.3.0、C++20です。掲載16例の独立コンパイルは通常・`-DNDEBUG`の両方で確認しています。入力例16件、境界・診断25件、ランダム入力300件の実行と、同じ実行ケースのASan・UBSan検査も通過しています。leak検査とGCC 12.2での実行検査は含みません。

ヘッダーは標準ライブラリだけを使い、`bits/stdc++.h` を含みます。include時はヘッダー内のテスト用 `main` が無効になります。各サンプルの `main` 同士は別々のプログラムとして扱ってください。

| 型・記号 | 意味 |
| --- | --- |
| `Gap` | `generalized_assignment_solver` のサンプル内での別名 |
| `Gap::assignment_option` | 登録候補の情報 |
| `Gap::solve_options` | 探索方法・打切り・乱数などの設定 |
| `Gap::profile` | どこまで探索するかを選ぶ列挙型 |
| `Gap::repair_status` | 部分修復の処理状態を示す列挙型 |
| `Gap::result` | 探索・修復・評価の結果 |
| `Gap::session` | 候補構造を前処理し、費用・容量を変更しながら使える独立snapshot |
| N / A / E | 仕事数 / 割当先数 / 登録候補数 |
| `int` | 仕事番号・割当先番号・候補ID・件数 |
| `long long` | 容量・需要・費用・費用和などの整数 |
| `std::span<const T>` | 配列を所有せず参照する引数。`vector<T>` や `array<T, K>` を渡せる |

仕事番号は `[0, N)`、割当先番号は `[0, A)`、登録済み候補IDは `[0, E)` です。候補IDはjob番号やagent番号とは別物です。候補ID列は仕事番号順にN個並べます。

`span` に既定の長さはありません。呼出し中は参照元の配列を生存させてください。引数既定値は、下表のどのメソッドにもありません。`solve({})` の `{}` は `solve_options` の既定値を明示的に渡しています。`solve()` とは書けません。

### 2.2 候補情報：assignment_option

`struct assignment_option { int job; int agent; long long demand; long long cost; };` という集成体です。`solver.options()` から登録内容を読む際に使います。構造体を作っただけではsolverに登録されません。

| field | 型 | 意味・取り得る値 | 初期値 |
| --- | --- | --- | --- |
| `job` | `int` | 所属する仕事。`0 <= job < N` | メンバー既定値なし |
| `agent` | `int` | 割当先。`0 <= agent < A` | メンバー既定値なし |
| `demand` | `long long` | この候補で使う容量。0以上 | メンバー既定値なし |
| `cost` | `long long` | この候補の費用。負・0・正が可能 | メンバー既定値なし |

`Gap::assignment_option c;` とだけ書くと数値メンバーは初期化されません。直接作る場合は `Gap::assignment_option c{job, agent, demand, cost};` のように全項目を指定します。`c{}` なら0で初期化されますが、その0が問題に対して有効とは限りません。

### 2.3 solverの構築・登録・参照API

| API | 引数の意味・条件 | 戻り値・動作 |
| --- | --- | --- |
| `Gap()` | 引数なし | 仕事0・割当先0・候補0の空solver |
| `Gap(int jobs, int agents, vector<long long> capacities)` | `jobs >= 0`、`agents >= 0`。容量列はagents個で全て非負 | 容量を保持したsolver。候補はまだない |
| `void init(int jobs, int agents, vector<long long> capacities)` | コンストラクタと同じ条件 | 問題を初期化し、既存候補を全削除。以後の候補IDは0から |
| `int add_option(int job, int agent, long long demand, long long cost)` | 有効なjob・agent、非負需要、符号付き費用 | 登録順の候補IDを返す。重複候補・同じjob/agentの異なるモードも可 |
| `int job_count() const` | 引数なし | N |
| `int agent_count() const` | 引数なし | A |
| `int option_count() const` | 引数なし | E |
| `const vector<assignment_option>& options() const` | 引数なし | 登録候補をID順に並べた配列への読み取り専用参照 |
| `const vector<long long>& capacities() const` | 引数なし | solverに登録された容量配列への読み取り専用参照 |
| `session make_session() const` | 候補登録後に呼ぶ | その時点の問題データをコピー・前処理したsession |

需要が容量を超える候補自体も登録できますが、その容量の下では選べません。需要0・容量0の組合せは有効です。Nが正でAが0、または候補を持たない仕事がある場合、全仕事を割り当てることはできません。

`solver` には候補の削除・需要の変更・費用のsetterはありません。費用・容量だけならsessionを使い、仕事数・割当先数・候補の所属・需要・候補集合が変わるなら構築し直します。

参照を返すAPIはコピーを返すわけではありません。候補追加・`init`・容量変更などをまたいで要素参照、iterator、`data()` を保持しないでください。固定した値が欲しいときは配列をコピーします。

### 2.4 探索設定：solve_options

`Gap::solve_options options;` または `Gap::solve_options{}` は、次の8項目を既定値で初期化します。

| field | 型 | 既定値 | 意味・設定範囲 |
| --- | --- | --- | --- |
| `algorithm` | `Gap::profile` | `profile::alns` | 2.5節の5種類から選ぶ。無関係な整数をcastして渡さない |
| `deadline` | `steady_clock::time_point` | `time_point::max()` | 探索を打ち切る絶対時刻。経過ミリ秒そのものではない |
| `iteration_limit` | `long long` | `10000` | ALNS反復上限。通常0以上。実装上0以下はALNS反復を行わない |
| `seed` | `std::uint64_t` | `123456789` | 呼出しごとの乱数初期値。0から64bit符号なし整数の最大値まで |
| `maximum_ruin_jobs` | `int` | `64` | ALNSで一度に外す仕事数の設定上限。通常4以上。内部の下限・問題規模による制限あり |
| `repair_price_weight` | `double` | `0.25` | ALNS再挿入時に容量の混雑を費用へ反映する倍率。有限な非負値を使う |
| `alns_temperature` | `double` | `0.05` | ALNSが一時的な費用増加を受け入れる温度の倍率。有限な非負値を使う |
| `local_search_interval` | `int` | `128` | ALNS中に小さな局所改善を挟む反復間隔。正値で有効、0以下でこの周期処理を無効化 |

これらは検証付きsetterではなく公開メンバーです。特にNaN・無限大を渡しても自動修正されません。倍率には有限な非負値を使ってください。大きい値が常に良いわけではありません。

`deadline` の時計は `std::chrono::steady_clock` です。例えば「今から100ms」は `steady_clock::now() + milliseconds(100)` で作ります。`system_clock` の時刻や整数の100を直接代入するものではありません。1ターンごとに予算を与えるなら、毎ターン期限を作り直します。

`iteration_limit` が数えるのはALNSだけです。0にしても、価格計算・初期構築・最初の局所探索は止まりません。`algorithm` が `local_search` 以下なら、ALNSにだけ関係する5項目の調整はその処理に使われません。

`maximum_ruin_jobs` の実効値は、探索対象の仕事数をnとすると次です。`repair` ではnは可変仕事数になります。

```text
上限 = min(max(4, maximum_ruin_jobs), max(4, n / 20))
実際に外す数 = min(n, 4以上「上限」以下から選ぶ数)
```

`n / 20` は整数除算です。例えばn=100なら設定64でも上限5、n=20なら上限4、n=1なら実際は1仕事です。0を指定してもALNSを無効化する設定にはなりません。

`repair_price_weight = 0` はALNS再挿入の容量価格項だけを外します。初期構築の価格計算までは止めません。`alns_temperature = 0` でも小さい基礎温度が残るので、厳密な山登り法へ切り替える設定ではありません。`local_search_interval = 0` はALNS中の周期処理だけを止めます。

最初は既定値を使い、変更するなら1項目ずつ比較します。ruin設定は16・32・64、価格倍率は0・0.25・0.5、温度倍率は0・0.05・0.1、周期は0・64・128・256などが比較の起点です。入力規模による上限と、実時間あたりの解品質を一緒に見て判断します。

### 2.5 探索範囲の列挙型：profile

| 列挙値 | 行うこと | 用途と誤解しやすい点 |
| --- | --- | --- |
| `profile::greedy` | 初期構築1回と必要な実行可能化 | 軽い構築を試す起点。「各候補を1回見るだけ」とは限らない。seedが効く処理もある |
| `profile::regret` | 複数の初期構築から選ぶ | 後回しにすると選択肢が悪くなる仕事を重視する構築を複数試す |
| `profile::lagrangian` | 容量の混雑を価格として考慮した複数構築、追加の実行可能化 | 容量が厳しく完全解を作りにくい場面の候補。最適性保証を意味しない |
| `profile::local_search` | 上記に1仕事移動・2仕事の組替による改善を加える | 完成した解を小さく変更して改善する。ALNSは行わない |
| `profile::alns` | 上記に仕事の一部を外して入れ直す反復探索を加える | 既定値。まとまった変更を試す予算がある場面 |

`greedy` / `regret` でも、実行可能化や条件に合う小問題の救済処理が動く場合があります。表はユーザーが選ぶ主な探索範囲を表しています。

後ろのprofileほど探索範囲は広いですが、同じ時間で常に良い解になる保証はありません。前段処理で時間を使うこともあります。これらは解法の選択であり、厳密解・最適解を保証する列挙値はありません。

### 2.6 solve・improve・repair・evaluate

以下の4メソッドはsolverとsessionの両方にあります。戻り値はすべて `Gap::result`、引数既定値はありません。

| API | 引数・条件 | 返すもの・重要な挙動 |
| --- | --- | --- |
| `result solve(const solve_options& options)` | 2.4節の探索設定 | 初期割当から探索した結果。前回の結果を自動では引き継がない |
| `result improve(span<const int> option_ids, const solve_options& options)` | 仕事番号順にN個の候補IDと探索設定 | 完全で容量内なら初期解として保持し、同じ条件で費用を悪化させず改善を試す |
| `result repair(span<const int> option_ids, span<const int> mutable_jobs, const solve_options& options)` | N個の候補ID、重複しない有効な可変仕事番号列、探索設定 | 可変集合だけを修復・改善。集合に含まれない仕事は固定 |
| `result evaluate(span<const int> option_ids)` | 仕事番号順の候補ID列 | 探索せず費用・負荷・未割当・超過を集計。計画は修正しない |

solverの4メソッドはすべて `const` です。sessionでは `evaluate` だけが `const`、探索3メソッドは内部統計を更新する場合があるため非constです。

solverの探索では呼出しごとに候補を前処理します。sessionは候補の構造的な前処理を再利用しますが、探索状態そのものを再開する機能ではありません。前回の割当を使うときは `improve` / `repair` へ結果のID列を渡します。

| 入力状態 | evaluate | improve | repair |
| --- | --- | --- | --- |
| 完全かつ容量内 | その割当を集計 | 初期解に採用 | 固定部分を守って可変部分を改善 |
| ID列の長さがNではない | N仕事を全て未割当とした結果 | 初期解全体を無視して構築から開始 | `invalid_input` |
| -1・他の負値・範囲外ID・別仕事のID | 該当仕事を未割当として集計 | 初期解全体を無視 | 可変仕事なら再構築できる。固定仕事なら `invalid_input` |
| 容量超過 | 超過量も集計 | 初期解全体を無視 | 固定部分が容量内なら可変部分の修復を試す |
| 固定仕事だけで容量超過 | 固定の概念なし | 固定の概念なし | `fixed_part_infeasible` |
| 可変仕事番号が不正・重複 | 該当引数なし | 該当引数なし | `invalid_input` |

`improve` は不完全な初期解を一部だけ残すAPIではありません。1仕事でも不正なら初期解全体を無視します。部分的に守る仕事があるなら `repair` を使います。

`repair` の可変部分は、全体として有効なら初期解として保持されます。可変部分に1つでも未割当や残余容量違反があれば、その可変部分を構築し直します。固定部分は変えません。

可変集合が空で固定部分が有効なら、全仕事をそのまま返し `completed` になります。可変集合を全仕事にすれば固定仕事はなくなり、不完全・超過した入力から全体修復を試せます。どちらもID列そのものはN個必要です。

### 2.7 結果：result

`result` は値を所有する構造体です。solverやsessionの参照ではないので、コピーして保持したり、solverを再構築した後も保持したりできます。ただし候補IDの意味は登録した問題に依存するので、新しく登録した問題へ古いIDをそのまま流用できません。

| field | 型 | 宣言時の既定値 | メソッドが返すときの意味 |
| --- | --- | --- | --- |
| `option_ids` | `vector<int>` | 空 | N要素。仕事ごとの登録候補ID。未割当は-1 |
| `agent_of_job` | `vector<int>` | 空 | N要素。仕事ごとの割当先。未割当は-1 |
| `load_of_agent` | `vector<long long>` | 空 | A要素。各割当先の需要和 |
| `objective` | `long long` | 0 | 有効に割り当てられた仕事だけの費用和 |
| `total_overflow` | `long long` | 0 | 全割当先の `max(0, 使用量 - 容量)` の合計 |
| `unassigned_count` | `int` | 0 | 未割当仕事数。evaluateでは不正IDも含む |
| `iterations` | `long long` | 0 | 実行したALNS反復数。前処理・構築・初回局所探索は含まない |
| `lower_bound` | `double` | 負の無限大 | 計算できた下界。評価だけの呼出しでは負の無限大 |
| `initial_solution_used` | `bool` | false | 完全な初期解を初期候補として採用したか。repairでは可変部分についての判定 |
| `repair_state` | `repair_status` | `not_requested` | 2.8節の修復処理状態 |

| API | 引数・戻り値・既定値 |
| --- | --- |
| `bool feasible() const` | 引数なし。`unassigned_count == 0 && total_overflow == 0` を返す |
| `ostream& operator<<(ostream& os, const result& value)` | 引数既定値なし。全fieldとfeasibleを名前付きでosへ出力し、同じosへの参照を返す |

`std::cerr << answer << '\n';` のように直接出力できます。真偽値は `true` / `false`、修復状態は列挙値の名前です。3配列はそれぞれ先頭20要素まで表示します。配列名の直後の `[25]` は全要素数、末尾の `...` は残りの省略です。空配列は `[0]=[]` です。元の配列は変更されません。

演算子は改行やflushを追加しません。数値は出力先の精度・基数に従い、演算子自身はそれらの設定を変えません。`cout`、`cerr`、`ostringstream` で使え、constの結果やメソッドの一時戻り値も渡せます。

**完全性を確認してから結果を使います。** 未割当を含む `objective` は全仕事の費用ではありません。仕事が少なくしか割り当てられていない結果を、費用が小さいだけで良い解と判定しないでください。

単に `Gap::result r;` と作った場合、配列は空で集計値は0なので `r.feasible()` はtrueです。これは「この問題の解が見つかった」という印ではありません。結果の保持有無には `optional<Gap::result>` などを使います。`feasible()` は、呼出し元の入力形式を含めた総合バリデータでもありません。

`lower_bound` は最適費用がこれより小さくなれない、という目安です。未計算なら負の無限大です。有限でも浮動小数誤差があるため、`objective == lower_bound` だけで厳密な最適性証明にしないでください。独立した最適性証明フラグはありません。`repair` の下界は固定条件を課した部分問題の下界であり、固定を外した全体問題の下界には流用できません。

`initial_solution_used` は、最終解が初期解と同一という意味ではありません。空可変集合の正常repairではtrueです。結果の配列や集計値は利用者が書き換えられますが、その整合性を自動的に維持する機能はありません。

### 2.8 修復状態の列挙型：repair_status

| 列挙値 | 意味 | ユーザーが確認すること |
| --- | --- | --- |
| `not_requested` | solve・improve・evaluateの結果、または既定のresult | 修復状態としての成否判定はしない |
| `completed` | 固定検査・必要な部分探索を通常どおり処理した | さらに `feasible()` を確認。未割当が残る場合がある |
| `invalid_input` | ID列長、可変仕事番号、重複可変仕事、固定仕事の候補IDなどが不正 | 入力を直す。探索時間を延ばしても形式エラーは直らない |
| `fixed_part_infeasible` | 固定仕事だけで容量超過 | 固定条件を緩める、可変集合を広げる、または容量を見直す |

入力長や可変集合のエラーでは、全仕事未割当の診断結果を返します。固定仕事の検査中のエラーでは、有効だった固定仕事の費用・負荷を含む場合があります。エラー結果は入力計画をそのまま返すものではありません。不正な固定IDと固定部分の超過が同時にあれば `invalid_input` が優先されます。

### 2.9 sessionの作成・参照・更新API

`session` のデフォルトコンストラクタは非公開です。`Gap::session s;` とは書かず、`auto s = solver.make_session();` と作ります。snapshotができた後のsolverへの候補追加・`init` は、そのsessionに反映されません。

sessionには2.6節の4メソッドに加え、次の公開メソッドがあります。引数既定値はありません。

| API | 引数条件 | 戻り値・効果 |
| --- | --- | --- |
| `int job_count() const` | 引数なし | snapshotの仕事数N |
| `int agent_count() const` | 引数なし | snapshotの割当先数A |
| `int option_count() const` | 引数なし | snapshotの候補数E |
| `const vector<long long>& capacities() const` | 引数なし | 現在の容量列への読み取り専用参照 |
| `long long option_cost(int option_id) const` | snapshotに存在する候補ID | 現在の候補費用 |
| `void set_option_cost(int option_id, long long cost)` | 有効な候補ID、符号付き費用 | 1候補の費用を絶対値で変更 |
| `void set_option_costs(span<const long long> costs)` | 登録候補ID順にE個 | 全費用を絶対値で変更 |
| `void reset_costs()` | 引数なし | session作成時の全費用へ戻す |
| `void set_capacity(int agent, long long capacity)` | 有効な割当先番号、非負容量 | 1割当先の総容量を絶対値で変更 |
| `void set_capacities(span<const long long> capacities)` | 割当先順にA個、全て非負 | 全容量を絶対値で変更 |
| `void reset_capacities()` | 引数なし | session作成時の全容量へ戻す |

sessionの候補IDは、作成元solverで登録したIDのままです。sessionには `options()` はありません。候補のjob・agent・demandは構造として変わらないので、必要なら作成元の候補情報を利用者側で保持します。更新後費用を `solver.options()[id].cost` から読んでも更新前の値なので、`session.option_cost(id)` を使います。

費用更新で割当自体は変わりません。容量更新でも割当を自動修復しません。更新後は `evaluate` で現在の計画を集計し直し、必要に応じて `improve` / `repair` を呼びます。

単一費用・単一容量の設定は定数時間です。一括費用・一括容量・各resetは対象配列の長さに応じて処理します。費用・容量の変更後の探索では統計の再計算も起きるため、setterが軽いことと、後続solveが完全な定数時間で更新できることは別です。

### 2.10 コピー・再利用・共通の注意点

| 対象・操作 | 意味と注意点 |
| --- | --- |
| solver / session / resultのコピー | 標準のコピー構築・コピー代入が使える。vectorもコピーするので独立したデータになる |
| 移動構築・移動代入 | 標準の移動操作が使える。移動元を元どおりの問題として使い続けない |
| solve_options / assignment_option / result | 集成体として初期化できる。既定値は各表のとおり |
| sessionを繰り返し呼ぶ | 候補構造の前処理を再利用。前回解や途中の探索状態を自動保存するわけではない |
| reset後に使う計画 | 条件が変わるので再評価する。resetは計画自体を作成時へ戻さない |
| 構造変更 | 登録し直し・session再作成が必要。外部キー等で候補IDを移し替える |
| 期限 | 前処理や一部の内側処理は中断しない。deadlineは厳密な実時間上限ではない |
| 実行可能解未発見 | 解が存在しないという証明ではない。時間やprofile変更で発見できる場合もある |
| 下界 | 負の無限大の可能性と、doubleの丸め誤差を考慮する |
| 並行利用 | 単一スレッドで使う前提。同じsessionを複数スレッドから同時に変更・探索しない |

**整数範囲と入力形式は呼出し側で保証します。** 件数、N+1、候補数の合計などはintに収まる必要があります。費用・容量・需要の総和だけでなく、途中の加減算・目的値差・利益の符号反転もlong longに収めます。極端に大きい値では、内部のdouble計算の丸めが探索順へ影響します。

コンストラクタ・`init`・`add_option` は主にassertで入力条件を調べます。`-DNDEBUG` では不正入力が安全に処理される保証はありません。sessionのsetterには範囲・長さを拒否するガードもありますが、戻り値はvoidであり、成功フラグはありません。不正IDの `option_cost` がreleaseで0を返し得ることを、エラー処理の仕様として利用しないでください。

`evaluate` / `repair` が表で説明した不正ID等を扱うことと、候補定義そのものに不正番号・負需要を登録してよいことは別です。3章の全例は、明記した入力形式・値域を満たすことを前提とします。

## 3. ユースケース

各例は、冒頭に問題、入力・出力の定義、その後に完全なプログラム、入力例、期待出力を載せています。ライブラリを操作する行・結果を使う行には日本語コメントを付けています。全例で番号は0始まりです。

基本形式を使う例でも、プログラムのコメントに必要な入力を繰り返し記載しています。候補IDは候補行の入力順です。解の候補が複数ある入力や時間制限を使う入力では、同等以上の別の割当になる場合があります。完全性と費用を確認して使ってください。

| 例 | 問題・利用目的 | 主に使うAPI・表現 |
| --- | --- | --- |
| U01 | 容量と禁止候補を持つ一般の割当 | 構築、add_option、solve、結果の読み取り |
| U02 | 人数の定員と満足度最大化 | 単位需要、負費用、solveの既定値 |
| U03 | 任意採用・複数ナップサック | 未採用候補、需要0、完全初期解からimprove |
| U04 | 同じ割当先の複数モード | 候補ID対応表、options、assignment_option |
| U05 | 外部計画の診断 | evaluate、各count、capacities、operator<< |
| U06 | 時間制限内の追加改善 | improve、deadline、iteration_limit、seed |
| U07 | 一部固定・部分修復 | repair、repair_statusの全列挙値 |
| U08 | ターンごとの費用変更 | session、単一費用更新、再評価、improve |
| U09 | 容量縮小後の修復 | 単一容量更新、負荷確認、session.repair |
| U10 | 独立した費用・容量シナリオ | 一括更新、reset、コピー、sessionの各count |
| U11 | 複数seedで最良解を保持 | session再利用、resultの保持、solve/improve |
| U12 | 探索範囲と調整値の比較 | profileの全列挙値、solve_optionsの全field |
| U13 | ペア罰則を含む外側探索 | 代理費用、repair提案、真の目的値で採否 |
| U14 | 仕事・候補・需要の変化 | 空の構築、init、session再作成、外部キー |
| U15 | 担当変更の負担を抑える | 候補費用への加算、元費用との区別 |
| U16 | 時間枠と設備の組ごとの容量 | 組を割当先番号へ変換、結果の復元 |


### 3.1 U01：容量と適合条件を守る基本の割当

作業員への仕事、機械への処理、開設済み倉庫への配送先など、「置く先によって必要量と費用が変わる」問題です。ここでは3仕事を2人へ割り当てます。各人が使える作業量を超えず、総費用を最小化します。

候補行が存在しない組合せは利用できません。費用は負でも構いません。表にない候補を巨大な費用で追加する必要はありません。

**入力と出力**

入力は `N A E`、A個の容量、E行の `仕事 割当先 需要 費用` です。各番号は0始まりです。
出力は `OK 総費用`、続いてN行の `仕事 割当先 候補ID` です。未発見時は `NO_FEASIBLE_SOLUTION` だけを出力します。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N A E、容量A個、E行のjob agent demand cost。番号は範囲内、容量・需要は非負
    // 各例とも入力形式は正しく、件数はint、数値の総和・差はlong longに収まることが前提

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    // 全項目を既定値で初期化する。探索はalns、反復上限10000
    Gap::solve_options options;

    // ALNSの反復上限だけを2000へ変更する
    options.iteration_limit = 2000;

    // 初期割当から探索し、resultを受け取る
    const auto answer = solver.solve(options);

    // 完全割当かつ容量内であることを確認する
    if (!answer.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";

        // 未発見は「解が存在しない」という証明ではない
        return 0;
    }

    // 完全解の費用和を出力する
    cout << "OK " << answer.objective << '\n';

    // 仕事数は登録時のN
    for (int j = 0; j < solver.job_count(); ++j) {
        // 割当先と登録候補ID
        cout << j << ' '
             << answer.agent_of_job[j] << ' '
             << answer.option_ids[j] << '\n';
    }
}
```

**入力例**

```text
3 2 6
3 4
0 0 2 4
0 1 2 7
1 0 2 2
1 1 1 6
2 0 1 -1
2 1 2 1
```

**期待出力（標準出力）**

```text
OK 8
0 1 1
1 0 2
2 0 4
```

### 3.2 U02：人数の定員を守り、満足度を最大化する

研修コースの定員のように、1人を受け入れるたびに席を1つ使う問題です。各人の満足度を足した値を最大にしたいので、費用を「満足度の符号を反転した値」にします。全員を必ずいずれかへ入れます。

一般の需要付き割当とは異なり、全候補の需要を1に固定します。満足度表の-1は利用不可を意味し、0は利用可能な満足度0です。

**入力と出力**

入力は `N A`、A個の定員、N行×A列の満足度です。満足度は0以上、禁止だけ-1です。
出力は満足度の合計、続いて各人の `人 コース` です。完全解未発見時は `NO_FEASIBLE_SOLUTION` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N A、定員A個、N×A個の満足度。満足度-1は禁止、それ以外は非負
    // 定員は非負。件数と添字はint、満足度の合計と符号反転はlong longに収まる範囲

    int n, a;
    if (!(cin >> n >> a)) {
        return 0;
    }

    vector<long long> seats(a);
    for (auto& x : seats) {
        cin >> x;
    }

    // 1人を1仕事、コースを割当先、定員を容量とする
    Gap solver(n, a, seats);

    for (int j = 0; j < n; ++j) {
        for (int agent = 0; agent < a; ++agent) {
            long long satisfaction;
            cin >> satisfaction;
            if (satisfaction >= 0) {
                // 需要1、負の満足度を費用にして最大化を最小化へ変換
                solver.add_option(j, agent, 1, -satisfaction);
            }
        }
    }

    // 空の設定で既定値を明示的に渡す
    const auto answer = solver.solve({});

    // 全員の入れ先が見つかったかを確認する
    if (!answer.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 費用の符号を戻すと満足度の合計
    cout << -answer.objective << '\n';

    for (int j = 0; j < n; ++j) {
        // 選択したコース番号
        cout << j << ' ' << answer.agent_of_job[j] << '\n';
    }
}
```

**入力例**

```text
3 2
1 2
9 3
4 8
-1 7
```

**期待出力（標準出力）**

```text
24
0 0
1 1
2 1
```

### 3.3 U03：採用しない選択を含む複数ナップサック

複数の箱へ品物を入れて利益を最大化します。各品物は高々1箱へ入れ、入れない品物も認めます。箱ごとにサイズや利益が異なって構いません。

本ライブラリの完全解は各仕事に1候補が必要です。そこで容量0の「未採用先」を1個追加し、全品物から需要0・費用0で選べるようにします。未採用は有効な候補であり、結果の候補IDが-1になる未割当とは別です。

**入力と出力**

入力は `N A E`、A個の箱容量、E行の `品物 箱 サイズ 利益` です。箱番号は0以上A未満です。利益は符号付きで構いません。
出力は総利益、続いて `品物 箱` です。箱-1は未採用です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N A E、箱容量A個、E行のitem box size profit。容量・サイズは非負
    // 入力は有効な番号を持ち、利益の符号反転・合計はlong longに収まること

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // 最後の割当先aは、容量を使わない未採用先
    capacity.push_back(0);

    // 実際の箱に加えて未採用先を1個作る
    Gap solver(n, a + 1, capacity);

    for (int k = 0; k < e; ++k) {
        int item, box;
        long long size, profit;
        cin >> item >> box >> size >> profit;

        // 利益最大化のため費用を負の利益にする
        solver.add_option(item, box, size, -profit);
    }

    vector<int> skipped(n);

    for (int j = 0; j < n; ++j) {
        // この候補IDが「未採用」を表す
        skipped[j] = solver.add_option(j, a, 0, 0);
    }

    // 全品物未採用という有効な初期解から改善する
    const auto answer = solver.improve(skipped, {});

    // 初期解を完全に保てていることを確認する
    if (!answer.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 未採用の利益0を含む、利益合計
    cout << -answer.objective << '\n';

    for (int j = 0; j < n; ++j) {
        // 未採用先の番号はa
        int box = answer.agent_of_job[j];
        cout << j << ' ' << (box == a ? -1 : box) << '\n';
    }
}
```

**入力例**

```text
3 2 4
3 2
0 0 3 8
1 0 2 5
1 1 2 6
2 1 1 -2
```

**期待出力（標準出力）**

```text
14
0 0
1 1
2 -1
```

### 3.4 U04：同じ割当先の複数モードを選び分ける

同じ機械でも「高速で高価」「低速で安価」というモードを選べる場合です。ここでは容量を使わない高価なモードも候補として扱います。

同じ仕事・同じ割当先に複数候補を登録し、入力したモード番号を候補IDと対応付けます。割当先だけを見るとモードが失われるため、復元には必ず `option_ids` を使います。

**入力と出力**

入力は `N A E`、A個の容量、E行の `仕事 割当先 モード番号 需要 費用` です。モード番号は任意の非負整数です。
出力は総費用、続いて `仕事 割当先 モード番号 需要` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N A E、容量A個、E行のjob agent mode demand cost。番号は有効、需要・容量は非負
    // 数値の総和・差はlong long、候補数はint範囲。modeは利用者側の識別番号

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // 仕事と割当先の数を決める
    Gap solver(n, a, capacity);

    vector<int> mode_of_option(e);

    for (int k = 0; k < e; ++k) {
        int job, agent, mode;
        long long demand, cost;
        cin >> job >> agent >> mode >> demand >> cost;

        // 同一job・agentでも別候補IDになる
        int id = solver.add_option(job, agent, demand, cost);
        mode_of_option[id] = mode;
    }

    // 需要と費用を両方考慮して候補を選ぶ
    const auto answer = solver.solve({});

    // 未割当の候補ID=-1で対応表を引かない
    if (!answer.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 選んだモードの費用合計
    cout << answer.objective << '\n';

    for (int j = 0; j < n; ++j) {
        // モードを識別できる登録候補ID
        int id = answer.option_ids[j];

        // 登録済みassignment_optionを参照する
        const auto& choice = solver.options()[id];

        // 候補の割当先と需要
        cout << j << ' '
             << choice.agent << ' '
             << mode_of_option[id] << ' '
             << choice.demand << '\n';
    }
}
```

**入力例**

```text
2 1 4
3
0 0 10 3 1
0 0 11 1 6
1 0 20 2 2
1 0 21 0 8
```

**期待出力（標準出力）**

```text
8
0 0 11 1
1 0 20 2
```

### 3.5 U05：外部で作った計画の検査とデバッグ出力

別のプログラムが作った候補ID列を受け取り、費用・未割当数・容量超過を調べます。割当を探索したり修正したりせず、入力計画を診断する用途です。

各候補IDは登録順の番号です。別仕事の候補IDや範囲外IDは該当仕事の未割当として評価されます。未割当を許容する診断なので、完全解かどうかを出力してから残容量を表示します。

**入力と出力**

入力は `N A E`、A個の容量、E行の `仕事 割当先 需要 費用`、最後にN個の候補IDです。
出力は件数、`result` のデバッグ表示、続いて割当先ごとの `割当先 残容量` です。負の残容量は超過を表します。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N A E、容量A個、候補E行、候補IDをN個。候補の定義自体は有効なもの
    // 最後のID列には-1や不正IDを含めてもよい。数値の総和・差はlong long範囲

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    vector<int> ids(n);
    for (int& id : ids) {
        cin >> id;
    }

    // 不正IDは未割当として集計し、容量超過もそのまま報告する
    const auto checked = solver.evaluate(ids);

    // 登録した3種類の件数
    cout << solver.job_count() << ' '
         << solver.agent_count() << ' '
         << solver.option_count() << '\n';

    // 全fieldを表示。各配列は先頭20要素まで、要素数と省略記号付き
    cout << checked << '\n';

    for (int agent = 0; agent < a; ++agent) {
        // 負ならこの割当先の容量超過
        long long free = solver.capacities()[agent] - checked.load_of_agent[agent];
        cout << agent << ' ' << free << '\n';
    }
}
```

**入力例**

```text
3 1 2
2
0 0 2 5
1 0 1 3
0 1 -1
```

**期待出力（標準出力）**

```text
3 1 2
result{feasible=false, objective=8, unassigned_count=1, total_overflow=1, iterations=0, lower_bound=-inf, initial_solution_used=false, repair_state=not_requested, option_ids[3]=[0, 1, -1], agent_of_job[3]=[0, 0, -1], load_of_agent[1]=[3]}
0 -1
```

### 3.6 U06：既存の完全解を、残り時間の範囲で改善する

すでに実行可能な計画があり、追加の時間だけ使って費用を下げます。完全な初期解を渡せば、同じ費用・容量の下で、それより高い費用の解へ置き換わることはありません。

時間は `steady_clock` の絶対時刻で指定します。入力の0ミリ秒は、探索時間を使い切った場面を再現します。初期解を受理する処理が探索前にあるので、0ミリ秒でも初期解を保持できます。

**入力と出力**

入力は基本形式に続き、N個の初期候補ID、予算ミリ秒です。初期解は完全かつ容量内でなければ `INVALID_INITIAL` を出力します。
正常時は `改善前費用 改善後費用 初期解受理フラグ`、続いて各仕事の `仕事 候補ID` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N A E、容量A個、候補E行、初期候補IDをN個、budget_ms(0以上)
    // 容量・需要・番号は有効、整数演算と時刻への加算は各型の範囲内とする

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    vector<int> initial(n);
    for (int& id : initial) {
        cin >> id;
    }

    long long budget_ms;
    cin >> budget_ms;

    // 入力計画を探索せず検査する
    const auto before = solver.evaluate(initial);

    // 不完全な計画をimproveへ渡すと全体が無視されるため、ここでは拒否
    if (!before.feasible()) {
        cout << "INVALID_INITIAL\n";
        return 0;
    }

    // 既定のalnsを使う
    Gap::solve_options options;

    // deadlineは厳密な実時間上限ではない。前処理・出力・内側処理の余裕を本番では確保する
    // 実行時間ではなく絶対終了時刻
    options.deadline =
        chrono::steady_clock::now() + chrono::milliseconds(budget_ms);

    // 反復上限を十分大きくして期限を主な終了条件にする
    options.iteration_limit = numeric_limits<long long>::max();

    // 入力条件を揃えた比較用の乱数seed
    options.seed = 42;

    // 完全な初期解を保持したうえで改善を試す
    const auto after = solver.improve(initial, options);

    // 初期解受理フラグは0/1
    cout << before.objective << ' '
         << after.objective << ' '
         << after.initial_solution_used << '\n';

    for (int j = 0; j < n; ++j) {
        // 初期解と同じ候補ID体系で戻る
        cout << j << ' ' << after.option_ids[j] << '\n';
    }
}
```

**入力例**

```text
2 2 4
2 2
0 0 1 4
0 1 1 1
1 0 1 2
1 1 1 5
0 3
0
```

**期待出力（標準出力）**

```text
9 9 1
0 0
1 3
```

### 3.7 U07：確定済みの仕事を固定し、一部だけ修復する

確定済みの担当を変えず、指定した仕事だけを割り当て直す問題です。入力例では仕事0を固定し、未割当の仕事1だけを配置します。

`mutable_jobs` に含まれない仕事は固定です。可変仕事の初期IDは-1でも構いませんが、固定仕事には正しい候補IDが必要です。正常終了しても完全解が見つかったとは限らないため、修復状態と完全性を別々に確認します。

**入力と出力**

入力は基本形式、N個の現在候補ID、可変仕事数M、M個の仕事番号です。可変集合は重複を認めません。
出力はエラー状態名、または `COMPLETED 総費用` と各仕事の `仕事 候補ID` です。正常処理だが未完成なら `COMPLETED_BUT_INCOMPLETE` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N A E、容量A個、候補E行、現在IDをN個、M、可変仕事番号M個
    // 候補定義は有効な範囲、数値の総和・差はlong long範囲。可変集合の重複はエラーになる

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    vector<int> ids(n);
    for (int& id : ids) {
        cin >> id;
    }

    int m;
    cin >> m;

    vector<int> movable(m);
    for (int& job : movable) {
        cin >> job;
    }

    // 全N仕事のIDと、今回だけ動かす仕事番号を渡す
    const auto answer = solver.repair(ids, movable, {});

    // 入力検査の状態と解の完全性を混同しない
    switch (answer.repair_state) {
        // 固定IDや可変集合、ID列長などが不正
        case Gap::repair_status::invalid_input:
            cout << "INVALID_INPUT\n";
            return 0;

        // 固定仕事だけで容量超過。可変集合を広げる必要がある
        case Gap::repair_status::fixed_part_infeasible:
            cout << "FIXED_PART_INFEASIBLE\n";
            return 0;

        // repair以外の結果で使われる状態。正常なrepairでは来ない
        case Gap::repair_status::not_requested:
            cout << "NOT_REQUESTED\n";
            return 0;

        // 修復処理を実行したことだけを表す
        case Gap::repair_status::completed:
            break;
    }

    // completedでも未割当が残る場合がある
    if (!answer.feasible()) {
        cout << "COMPLETED_BUT_INCOMPLETE\n";
        return 0;
    }

    // 固定仕事の費用も含む全体費用
    cout << "COMPLETED " << answer.objective << '\n';

    for (int j = 0; j < n; ++j) {
        // 固定仕事の候補IDは入力と一致する
        cout << j << ' ' << answer.option_ids[j] << '\n';
    }
}
```

**入力例**

```text
2 2 4
1 1
0 0 1 4
0 1 1 1
1 0 1 2
1 1 1 5
0 -1
1
1
```

**期待出力（標準出力）**

```text
COMPLETED 9
0 0
1 3
```

### 3.8 U08：費用が変わるターンで前回解を引き継ぐ

移動距離や報酬の変化により、候補の費用だけがターンごとに変わる場面です。候補の仕事・割当先・需要は共通なので、前処理済みの `session` を1回作り、各ターンでは変更された費用だけを書き換えます。

費用変更は容量違反を起こさないため、前回の完全解を再利用できます。ただし費用の比較は変更後の条件で行います。前ターンの古い `objective` は比較に使いません。

**入力と出力**

入力は基本形式、ターン数Q、各ターンの `K 予算ミリ秒` とK行の `候補ID 新費用` です。予算は正とします。
出力は初期費用、続いて各ターンの `ターン 変更後条件での前回解費用 改善後費用` です。完全解未発見時は終了します。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: 基本形式、Q、各ターンのK budget_msとK行のid new_cost。番号は有効、予算は正
    // 仕事・割当先・需要はターン間で不変。数値と時刻計算は型の範囲内とする

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    // 候補登録後に作る独立snapshot。以降の費用変更はこのsessionだけに効く
    auto session = solver.make_session();

    // 最初の完全解を作る
    auto current = session.solve({});

    // 完全解がなければ前回解利用へ進まない
    if (!current.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 最初の条件での費用
    cout << current.objective << '\n';

    int q;
    cin >> q;

    for (int turn = 0; turn < q; ++turn) {
        int k;
        long long budget_ms;
        cin >> k >> budget_ms;

        for (int t = 0; t < k; ++t) {
            int id;
            long long cost;
            cin >> id >> cost;

            // 現在値を読み、同じ費用なら更新を省く
            if (session.option_cost(id) != cost) {
                // 差分加算ではなく、新しい費用そのものを設定する
                session.set_option_cost(id, cost);
            }
        }

        // 新費用で前回の割当を再評価する
        const auto before = session.evaluate(current.option_ids);

        // ターンごとに探索設定を作る
        Gap::solve_options options;

        // 全体時間にも上限がある場合、deadlineを全体期限との小さい方にする
        // 毎ターン新しい絶対期限にする
        options.deadline =
            chrono::steady_clock::now() + chrono::milliseconds(budget_ms);

        // 時間中心の探索にする
        options.iteration_limit = numeric_limits<long long>::max();

        // 前回解を保持しつつ、新費用で改善する
        current = session.improve(current.option_ids, options);

        // 比較対象は両方とも変更後の費用
        cout << turn << ' ' << before.objective << ' ' << current.objective << '\n';
    }
}
```

**入力例**

```text
2 2 4
2 2
0 0 1 4
0 1 1 1
1 0 1 2
1 1 1 5
2
1 1000
1 10
1 1000
3 -3
```

**期待出力（標準出力）**

```text
3
0 12 6
1 6 1
```

### 3.9 U09：容量縮小後、影響を受ける仕事を修復する

作業員の欠勤や設備の能力低下により、割当先の容量が減る問題です。新しい容量で現在の計画を評価し、超過している割当先上の全仕事を可変にします。それ以外は固定したまま修復します。

この選び方でも、固定仕事の移動が必要で完全解を見つけられない場合があります。そこで失敗したときは可変集合を全仕事へ広げて再試行します。容量0でも需要0の候補は使用可能なので、「設備を完全に使えなくする」とは限りません。

**入力と出力**

入力は基本形式、現在候補IDをN個、更新件数Q、Q行の `割当先 新容量` です。更新は上から適用します。
各更新後に `更新番号 総費用` とN行の `仕事 割当先` を出力します。初期計画不正や完全解未発見なら理由を出力して終了します。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: 基本形式、現在IDをN個、Q、Q行のagent new_capacity。新容量は非負
    // 候補の番号・需要・容量は有効、総和・差はlong long範囲。初期計画は完全であること

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    vector<int> ids(n);
    for (int& id : ids) {
        cin >> id;
    }

    // 容量を変更するための独立したsnapshot
    auto session = solver.make_session();

    // 初期容量の下で完全かを確認する
    if (!session.evaluate(ids).feasible()) {
        cout << "INVALID_INITIAL\n";
        return 0;
    }

    int q;
    cin >> q;

    for (int turn = 0; turn < q; ++turn) {
        int agent;
        long long new_capacity;
        cin >> agent >> new_capacity;

        // 1割当先の総容量を絶対値で置き換える
        session.set_capacity(agent, new_capacity);

        // 新容量で超過を調べる
        const auto checked = session.evaluate(ids);

        vector<int> movable;

        for (int j = 0; j < n; ++j) {
            // 初期計画と成功した修復は全仕事に有効な割当先を持つ
            int placed = checked.agent_of_job[j];

            // この割当先に負荷超過があるか
            if (checked.load_of_agent[placed] > session.capacities()[placed]) {
                movable.push_back(j);
            }
        }

        // 超過割当先にいる仕事だけを動かす
        auto answer = session.repair(ids, movable, {});

        // 局所的な修復だけでは不足
        if (answer.repair_state != Gap::repair_status::completed
            || !answer.feasible()) {
            movable.resize(n);
            iota(movable.begin(), movable.end(), 0);

            // 固定を解除し、全仕事を対象に再試行する
            answer = session.repair(ids, movable, {});
        }

        // 未発見は不可能の証明ではない
        if (answer.repair_state != Gap::repair_status::completed
            || !answer.feasible()) {
            cout << "NO_FEASIBLE_SOLUTION\n";
            return 0;
        }

        // 次の更新へ渡す候補ID列を置き換える
        ids = answer.option_ids;

        // 新容量に適合した計画の費用
        cout << turn << ' ' << answer.objective << '\n';

        for (int j = 0; j < n; ++j) {
            // 修復後の割当先
            cout << j << ' ' << answer.agent_of_job[j] << '\n';
        }
    }
}
```

**入力例**

```text
2 2 4
2 2
0 0 1 0
0 1 1 4
1 0 1 0
1 1 1 2
0 2
1
0 1
```

**期待出力（標準出力）**

```text
0 2
0 0
1 1
```

### 3.10 U10：複数シナリオを独立に試し、基準条件へ戻す

需要構造は同じままで、費用表と容量表だけ異なる複数のシナリオを比較します。費用は候補ID順、容量は割当先番号順に一括設定します。

最後に条件を基準へ戻します。`reset_costs` と `reset_capacities` が戻す先は、直前の設定値ではなく `make_session` を呼んだ時点です。sessionのコピーも独立していることを出力で確認します。

**入力と出力**

入力は基本形式、シナリオ数Q、各シナリオのA個の容量とE個の費用です。
出力は各シナリオの `番号 総費用`、基準へ戻した費用、元solverと復元sessionの件数一致フラグです。未発見のシナリオは `番号 NO_FEASIBLE_SOLUTION` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: 基本形式、Q、各シナリオの容量A個と候補ID順の費用E個。容量は非負
    // 候補構造は全シナリオで共通。数値の総和・差はlong long、件数はint範囲

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    // この時点の費用・容量がresetの復帰先になる
    auto base = solver.make_session();

    // データを複製する。sessionの変更はbaseやsolverへ伝わらない
    auto session = base;

    int q;
    cin >> q;

    for (int scenario = 0; scenario < q; ++scenario) {
        vector<long long> capacities(a), costs(e);
        for (auto& x : capacities) {
            cin >> x;
        }
        for (auto& x : costs) {
            cin >> x;
        }

        // 必ずA個の非負容量を渡す
        session.set_capacities(capacities);

        // 必ずE個を登録候補ID順で渡す
        session.set_option_costs(costs);

        // このシナリオの初期解を作り直す
        const auto answer = session.solve({});

        // 容量が厳しいシナリオでは未完成になることもある
        if (answer.feasible()) {
            // シナリオ間で費用尺度が同じかは利用者が判断する
            cout << scenario << ' ' << answer.objective << '\n';
        } else {
            cout << scenario << " NO_FEASIBLE_SOLUTION\n";
        }
    }

    // session作成時の全費用へ戻す。1つ前のシナリオへ戻す操作ではない
    session.reset_costs();

    // session作成時の全容量へ戻す
    session.reset_capacities();

    // 戻した条件で改めて探索する
    const auto restored = session.solve({});

    // コピー元のsessionは一度も変更されていない
    const auto original = base.solve({});

    // 完全な結果同士を比較する
    if (restored.feasible() && original.feasible()) {
        // 同じ基準条件の費用
        cout << "BASE " << restored.objective << ' ' << original.objective << '\n';
    } else {
        cout << "BASE NO_FEASIBLE_SOLUTION\n";
    }

    // 仕事数は変わらない
    bool same_size = session.job_count() == solver.job_count();

    // 割当先数は変わらない
    same_size = same_size && session.agent_count() == solver.agent_count();

    // 候補数は変わらない
    same_size = same_size && session.option_count() == solver.option_count();
    cout << "SAME_SIZE " << same_size << '\n';
}
```

**入力例**

```text
2 2 4
2 2
0 0 1 4
0 1 1 1
1 0 1 2
1 1 1 5
2
2 2
4 1 2 5
2 2
0 10 8 0
```

**期待出力（標準出力）**

```text
0 3
1 0
BASE 3 3
SAME_SIZE 1
```

### 3.11 U11：複数の乱数seedで最良計画を積み上げる

同じ問題に対して複数回の探索機会がある場合です。最初の完全解がない間は新しく探索し、完全解が見つかった後は、その解を次の初期解として改善を続けます。

seedだけを変えても常に異なる結果になるとは限りません。ここでは費用・容量を変えず、比較する目的値を同じ条件に揃えます。sessionが前回の割当を記憶してくれるわけではないので、結果を自分で保持します。

**入力と出力**

入力は基本形式、seed数K、K個の `uint64_t` 範囲のseedです。Kは1以上です。
出力は最良の完全解の総費用、続いて `仕事 候補ID` です。すべて未発見なら `NO_FEASIBLE_SOLUTION` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: 基本形式、K(1以上)、K個の64bit非負seed。候補定義は有効、総和・差はlong long範囲

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    int k;
    cin >> k;

    // K回の呼出しで候補構造の前処理を共有する
    auto session = solver.make_session();

    // 空のresultのfeasible()を「発見済み」の判定に使わず、保持有無を分ける
    optional<Gap::result> best;

    for (int trial = 0; trial < k; ++trial) {
        uint64_t seed;
        cin >> seed;

        // deadlineは最大時刻のまま、固定反復で比較する
        Gap::solve_options options;

        // 1回あたりのALNS反復上限
        options.iteration_limit = 2000;

        // 各呼出しの乱数系列を指定する
        options.seed = seed;

        // この試行で返る結果を受け取る変数
        Gap::result candidate;
        if (best) {
            // 完全な最良解を次の初期解にする
            candidate = session.improve(best->option_ids, options);
        } else {
            // まだ完全解がないので構築から試す
            candidate = session.solve(options);
        }

        // 完全解だけを同じ尺度で比較する
        if (candidate.feasible() && (!best || candidate.objective < best->objective)) {
            // vectorを含む結果を外側で保存する
            best = move(candidate);
        }
    }

    if (!best) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 完全解の最良費用
    cout << best->objective << '\n';

    for (int j = 0; j < n; ++j) {
        // 保存した最良解の登録候補ID
        cout << j << ' ' << best->option_ids[j] << '\n';
    }
}
```

**入力例**

```text
2 2 4
2 2
0 0 1 4
0 1 1 1
1 0 1 2
1 1 1 5
3
0 42 18446744073709551615
```

**期待出力（標準出力）**

```text
3
0 1
1 2
```

### 3.12 U12：探索の範囲と調整パラメータを比較する

同じ入力に対して5つの探索範囲を順に試し、実行可能性と費用を比較します。モデルそのものは変えません。固定反復は実時間を等しくする設定ではないため、本番で選ぶ際は実時間も別途測ってください。

入力例は、仕事1が割当先0を使うしかなく、その結果が一意に定まる小さな問題です。表示する `iterations` はALNS反復数だけで、初期構築や局所探索の処理回数ではありません。

**入力と出力**

入力は基本形式、続いて `反復上限 ruin設定 価格倍率 温度倍率 周期` です。倍率は有限な非負値、他は非負整数とします。
出力は各行が `profile名 実行可能フラグ 費用 ALNS反復数` です。実行可能フラグ0の費用は部分解の費用です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: 基本形式、iterations ruin price_weight temperature interval
    // 反復上限と周期は0以上、ruinは4以上を推奨。倍率は有限非負、容量・需要・番号は有効とする

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    // 8項目を既定値で初期化する
    Gap::solve_options options;

    // ALNS反復上限と一度に外す仕事数の設定
    cin >> options.iteration_limit >> options.maximum_ruin_jobs;

    // 再挿入の容量価格倍率と悪化受理温度倍率
    cin >> options.repair_price_weight >> options.alns_temperature;

    // ALNS中の局所探索周期。0でこの周期処理だけを無効化する
    cin >> options.local_search_interval;

    // この例は実時間制限を設けない
    options.deadline = chrono::steady_clock::time_point::max();

    // 全profileへ同じseedを渡す
    options.seed = 42;

    // 取り得る5種類を列挙する
    const array<Gap::profile, 5> profiles{
        // 初期構築を1回行う
        Gap::profile::greedy,

        // 複数の初期構築を試す
        Gap::profile::regret,

        // 容量価格を使う構築と追加の実行可能化を行う
        Gap::profile::lagrangian,

        // 1仕事移動・2仕事組替による改善も行う
        Gap::profile::local_search,

        // 部分的に外して入れ直す反復探索まで行う
        Gap::profile::alns
    };

    const array<const char*, 5> names{
        "greedy",
        "regret",
        "lagrangian",
        "local_search",
        "alns"
    };

    for (size_t i = 0; i < profiles.size(); ++i) {
        // 今回使う探索範囲を指定する
        options.algorithm = profiles[i];

        // 各profileを独立に構築から実行する
        const auto answer = solver.solve(options);

        // 完全性・費用・ALNS反復数
        cout << names[i] << ' '
             << answer.feasible() << ' '
             << answer.objective << ' '
             << answer.iterations << '\n';
    }

    // iteration_limit=0でも初期構築や最初の局所探索は動く
    // maximum_ruin_jobs=0でもALNSは停止しない。小問題では実効ruin上限がさらに制限される
    // alns_temperature=0でも基礎温度が残るので、厳密な山登り指定にはならない
}
```

**入力例**

```text
2 2 3
1 1
0 0 1 0
0 1 1 1
1 0 1 0
0 64 0.25 0.05 128
```

**期待出力（標準出力）**

```text
greedy 1 1 0
regret 1 1 0
lagrangian 1 1 0
local_search 1 1 0
alns 1 1 0
```

### 3.13 U13：同じ割当先に置く組合せの罰則を、外側で評価する

同じ担当者へ特定の2仕事を入れると、追加費用が生じる問題です。本ライブラリの候補費用だけでは仕事同士の相互作用を直接表せません。そこで現在の配置をもとに候補ごとの仮の費用を作り、ライブラリには割当変更を提案させます。

提案後には「元の候補費用の合計＋同じ先へ置いたペアの罰則」を正しく計算し直し、その値が下がった場合だけ採用します。仮の費用での改善が本当の改善とは限らないためです。入力例では仕事1だけを別の担当者へ移します。

**入力と出力**

入力は基本形式、N個の完全な初期候補ID、ペア数P、P行の `仕事u 仕事v 罰則`、修復回数Q、各回の `M 可変仕事番号M個` です。ペアは `0 <= u < v < N`、罰則は非負です。
出力は `真の総費用 元の候補費用合計` と各仕事の `仕事 割当先` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: 基本形式、初期IDをN個、P、P行のu v penalty、Q、各回のMと可変仕事番号
    // 初期解は完全、ペアは0<=u<v<N、罰則は非負、可変集合は有効で重複なし
    // 候補費用・代理費用・罰則の全中間計算はlong long範囲とする

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    vector<int> ids(n);
    for (int& id : ids) {
        cin >> id;
    }

    struct PairPenalty {
        int u, v;
        long long cost;
    };

    int p;
    cin >> p;
    vector<PairPenalty> pairs(p);
    for (auto& item : pairs) {
        cin >> item.u >> item.v >> item.cost;
    }

    // solverは元費用のまま保ち、真の基礎費用を計算する
    auto current = solver.evaluate(ids);

    // 不完全な割当ではペア罰則を正しく比較できない
    if (!current.feasible()) {
        cout << "INVALID_INITIAL\n";
        return 0;
    }

    // 完全な結果だけを受け取り、非分離な費用を外側で加える
    auto true_cost = [&](const Gap::result& value) {
        // solver.evaluateで求めた元の候補費用
        long long total = value.objective;

        for (const auto& item : pairs) {
            // 同じ割当先のペアだけ罰則を1回加算
            if (value.agent_of_job[item.u] == value.agent_of_job[item.v]) {
                total += item.cost;
            }
        }
        return total;
    };

    long long best_cost = true_cost(current);

    // 代理費用を書き換える専用snapshot
    auto session = solver.make_session();

    int q;
    cin >> q;

    for (int step = 0; step < q; ++step) {
        int m;
        cin >> m;
        vector<int> movable(m);
        for (int& job : movable) {
            cin >> job;
        }

        vector<long long> costs(e);
        for (int id = 0; id < e; ++id) {
            // 元の候補定義から仕事・割当先・費用を読む
            const auto& option = solver.options()[id];

            // 元費用を代理費用の出発点にする
            costs[id] = option.cost;

            for (const auto& item : pairs) {
                // この仕事の相手を求める
                int other = option.job == item.u
                    ? item.v
                    : (option.job == item.v ? item.u : -1);

                // 相手の現在位置を固定した近似罰則
                if (other >= 0 && current.agent_of_job[other] == option.agent) {
                    costs[id] += item.cost;
                }
            }
        }

        // 全候補の代理費用を候補ID順で置き換える
        session.set_option_costs(costs);

        // 可変仕事だけの変更案を作る
        const auto proposal = session.repair(current.option_ids, movable, {});

        // エラーや不完全な提案は採用しない
        if (proposal.repair_state != Gap::repair_status::completed
            || !proposal.feasible()) {
            continue;
        }

        // 代理費用ではなく元の費用で再評価する
        auto checked = solver.evaluate(proposal.option_ids);
        long long cost = true_cost(checked);

        if (cost < best_cost) {
            best_cost = cost;

            // 真の目的値が改善したときだけ現在解を置き換える
            current = move(checked);
        }
    }

    // 真の総費用と候補費用の合計は区別する
    cout << best_cost << ' ' << current.objective << '\n';

    for (int j = 0; j < n; ++j) {
        // 最後に採用された割当
        cout << j << ' ' << current.agent_of_job[j] << '\n';
    }

    // proposal.lower_boundは代理問題の下界なので、真の目的値の下界として使わない
}
```

**入力例**

```text
2 2 4
2 2
0 0 1 0
0 1 1 3
1 0 1 0
1 1 1 2
0 2
1
0 1 10
1
1 1
```

**期待出力（標準出力）**

```text
2 2
0 0
1 1
```

### 3.14 U14：仕事・候補・需要が変わった問題を再構築する

仕事の追加・削除、候補の追加・削除、需要の変更がある場合です。これらは既存sessionの更新APIでは変更できないので、候補を登録し直します。登録順が変わると候補IDも変わります。

この例では外部の仕事キーと候補キーを使って、前回選んだ候補の意味を新しいIDへ対応付けます。同じキーでも需要変更で容量を超えることがあるため、対応付け後の計画を評価し、必要なら全仕事を修復します。小さな例の読みやすさを優先してキー検索は線形探索にしています。

**入力と出力**

入力はターン数T。各ターンは `N A E`、A個の容量、N個の仕事キー、E行の `仕事番号 候補キー 割当先 需要 費用` です。
仕事キーはターン内で一意、候補キーは同じ仕事内で一意とします。同じキーを使う間は同じ選択肢を意味させてください。
出力は `ターン 総費用` とN行の `仕事キー 選択候補キー` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: T、各ターンのN A E、容量A個、仕事キーN個、候補E行(job key agent demand cost)
    // 仕事キーは一意、候補キーは仕事内で一意。番号・非負容量/需要・整数演算の範囲は有効

    int turns;
    if (!(cin >> turns)) {
        return 0;
    }

    // 前回の「仕事キー、選択候補キー」。ライブラリのIDは保存しない
    vector<pair<int, int>> previous;

    // 空のsolver。各ターンでinitして登録し直す
    Gap solver;

    for (int turn = 0; turn < turns; ++turn) {
        int n, a, e;
        cin >> n >> a >> e;

        vector<long long> capacity(a);
        for (auto& x : capacity) {
            cin >> x;
        }

        vector<int> job_key(n), option_key(e);
        for (int& key : job_key) {
            cin >> key;
        }

        // 古い候補を全削除し、新しい仕事数・割当先数・容量を設定する
        solver.init(n, a, capacity);

        for (int k = 0; k < e; ++k) {
            int job, key, agent;
            long long demand, cost;
            cin >> job >> key >> agent >> demand >> cost;

            // 候補IDは今回の登録順で新たに決まる
            int id = solver.add_option(job, agent, demand, cost);
            option_key[id] = key;
        }

        vector<int> initial(n, -1);

        for (int j = 0; j < n; ++j) {
            auto old = find_if(
                previous.begin(),
                previous.end(),
                [&](const auto& x) {
                    return x.first == job_key[j];
                }
            );
            if (old == previous.end()) {
                continue;
            }

            // 新候補IDの範囲を走査する
            for (int id = 0; id < solver.option_count(); ++id) {
                // 仕事と外部候補キーの両方を照合する
                if (solver.options()[id].job == j && option_key[id] == old->second) {
                    initial[j] = id;
                    break;
                }
            }
        }

        // 構造が変わったのでsessionも新しく作る
        auto session = solver.make_session();

        // 対応できない仕事や需要変更による超過を調べる
        const auto checked = session.evaluate(initial);

        // このターンの結果を格納する
        Gap::result answer;

        // 対応付けた初期解が完全かつ容量内なら保持する
        if (checked.feasible()) {
            // 新しい費用・需要・容量の下で改善する
            answer = session.improve(initial, {});
        } else {
            vector<int> all(n);
            iota(all.begin(), all.end(), 0);

            // 不完全な初期解は全仕事を可変にして修復する
            answer = session.repair(initial, all, {});
        }

        previous.clear();

        // このターンの不完全な結果は次の初期計画として保存しない
        if (!answer.feasible()) {
            cout << turn << " NO_FEASIBLE_SOLUTION\n";
            continue;
        }

        // 今回の条件での費用
        cout << turn << ' ' << answer.objective << '\n';

        for (int j = 0; j < n; ++j) {
            // 内部候補IDを外部候補キーへ戻す
            int key = option_key[answer.option_ids[j]];
            previous.emplace_back(job_key[j], key);
            cout << job_key[j] << ' ' << key << '\n';
        }
    }
}
```

**入力例**

```text
2
2 2 4
2 2
10 20
0 100 0 1 4
0 101 1 1 1
1 200 0 1 2
1 201 1 1 5
3 2 5
2 2
20 10 30
0 200 0 2 2
0 201 1 1 4
1 103 1 1 1
2 300 0 1 0
2 301 1 1 5
```

**期待出力（標準出力）**

```text
0 3
10 101
20 200
1 5
20 201
10 103
30 300
```

### 3.15 U15：担当変更の負担も費用へ加える

現在の計画を頻繁に変えると引き継ぎ作業が発生する問題です。元の候補費用に、「その仕事の担当者を変えた場合の負担」を加えた値を最小化します。

初期担当を固定した比較なので、この追加費用は候補ごとに決まり、ライブラリで直接扱えます。同じ担当者の中でモードだけを変える場合、この例では変更負担を加えません。

**入力と出力**

入力は基本形式、N個の完全な初期候補ID、N個の非負変更負担です。
出力は `元の費用 変更負担込みの新費用 元費用だけでの新費用 担当変更数` と各仕事の `仕事 候補ID` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: 基本形式、完全な初期IDをN個、担当変更時の非負負担をN個
    // 候補定義は有効。費用と変更負担を足す中間計算を含めlong long範囲に収める

    int n, a, e;
    if (!(cin >> n >> a >> e)) {
        return 0;
    }

    vector<long long> capacity(a);
    for (auto& x : capacity) {
        cin >> x;
    }

    // n仕事・a割当先と、それぞれの非負容量を登録する
    Gap solver(n, a, capacity);

    for (int k = 0; k < e; ++k) {
        int job, agent;
        long long demand, cost;
        cin >> job >> agent >> demand >> cost;

        // 入力順kが候補ID。登録しない組合せは禁止
        solver.add_option(job, agent, demand, cost);
    }

    vector<int> initial(n);
    for (int& id : initial) {
        cin >> id;
    }

    vector<long long> penalty(n);
    for (auto& x : penalty) {
        cin >> x;
    }

    // 初期担当と元の費用を保存する
    const auto before = solver.evaluate(initial);

    // 初期担当が不明な仕事や容量超過をこの例では認めない
    if (!before.feasible()) {
        cout << "INVALID_INITIAL\n";
        return 0;
    }

    // 元の費用を持つsolverとは別に調整後費用を設定する
    auto session = solver.make_session();
    for (int id = 0; id < e; ++id) {
        // 登録候補の仕事・割当先・元費用を参照する
        const auto& option = solver.options()[id];

        // まず元の費用から始める
        long long cost = option.cost;

        // 初期担当と異なる候補だけ負担を加える
        if (option.agent != before.agent_of_job[option.job]) {
            cost += penalty[option.job];
        }

        // 調整後費用を絶対値で書き込む
        session.set_option_cost(id, cost);
    }

    // 初期解では変更負担が0。調整後目的値が悪化しないように改善する
    const auto after = session.improve(initial, {});

    // 元の費用だけの合計も別に求める
    const auto raw = solver.evaluate(after.option_ids);

    int changes = 0;

    for (int j = 0; j < n; ++j) {
        // 担当変更数を数える
        changes += after.agent_of_job[j] != before.agent_of_job[j];
    }

    // 3種類の費用を混同しない
    cout << before.objective << ' '
         << after.objective << ' '
         << raw.objective << ' '
         << changes << '\n';

    for (int j = 0; j < n; ++j) {
        // 変更負担込みで選んだ候補ID
        cout << j << ' ' << after.option_ids[j] << '\n';
    }
}
```

**入力例**

```text
2 2 4
2 2
0 0 1 4
0 1 1 1
1 0 1 2
1 1 1 5
0 3
2 4
```

**期待出力（標準出力）**

```text
9 8 6 1
0 1
1 3
```

### 3.16 U16：時間枠と設備の組を割当先にする

各仕事を「どの時間枠の、どの設備で処理するか」に割り当てます。時間枠と設備の組を1個の割当先へ変換すると、各組の処理量上限を守れます。候補行を限定すれば、仕事ごとの利用可能な時間枠も表せます。

各仕事は1つの時間枠の中で完結する前提です。複数時間枠を連続して占有する仕事、開始時刻の細かい指定、設備間の先行制約を解く例ではありません。

**入力と出力**

入力は `N S M E`、S行×M列の容量、E行の `仕事 時間枠 設備 需要 費用` です。SとMは正、S×Mはint範囲です。
出力は総費用、続いて `仕事 時間枠 設備` です。

**実行できるコード**

```cpp
// 一般化割当ソルバー本体
#include "generalized_assignment_solver_v13.hpp"

using namespace std;

// 長い型名をGapと表記する
using Gap = generalized_assignment_solver;

int main() {
    // 入力: N S M E、S×M個の容量(時間枠を外側)、E行のjob slot machine demand cost
    // S,Mは正、S*Mと件数はint範囲。各番号は有効、需要・容量は非負、整数演算はlong long範囲

    int n, slots, machines, e;
    if (!(cin >> n >> slots >> machines >> e)) {
        return 0;
    }

    vector<long long> capacity(slots * machines);
    for (auto& x : capacity) {
        cin >> x;
    }

    // (slot,machine)をslot*machines+machine番の割当先へ変換する
    Gap solver(n, slots * machines, capacity);

    for (int k = 0; k < e; ++k) {
        int job, slot, machine;
        long long demand, cost;
        cin >> job >> slot >> machine >> demand >> cost;
        int agent = slot * machines + machine;

        // 登録された時間枠・設備だけを選べるようにする
        solver.add_option(job, agent, demand, cost);
    }

    // 各時間枠・設備の容量を守って費用を小さくする
    const auto answer = solver.solve({});

    // 完全解がある場合だけ番号を復元する
    if (!answer.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 全仕事の費用合計
    cout << answer.objective << '\n';

    for (int j = 0; j < n; ++j) {
        // 合成した割当先番号
        int agent = answer.agent_of_job[j];
        cout << j << ' ' << agent / machines << ' ' << agent % machines << '\n';
    }

    // 1候補が消費できる容量は1割当先だけ。時間枠全体と設備別の上限を同時には追加できない
}
```

**入力例**

```text
3 2 2 5
2 2
2 2
0 0 0 2 1
0 1 0 2 6
1 0 0 1 0
1 0 1 1 3
2 1 1 2 2
```

**期待出力（標準出力）**

```text
6
0 0 0
1 0 1
2 1 1
```
