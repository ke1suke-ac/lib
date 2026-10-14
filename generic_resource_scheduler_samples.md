# Generic Resource Scheduler サンプルコード集

対象は`generic_resource_scheduler_v09.hpp`。C++20・GCCで使用する。第1章で問題を説明し、第2章に全公開API、第3章に18個の独立した実行例を載せる。

各例は`main`を含むプログラム全体である。使いたい例を一つだけ`sample.cpp`へコピーし、対象ヘッダを同じフォルダに置けば、その例だけでコンパイルできる。複数例のmainを一つのソースへ貼り付けない。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined sample.cpp -o sample
./sample < sample.in
```

各節の入力例を`sample.in`へ保存して実行する。ZIP内の`examples/`には、同じコードを`.cpp`、入力を`.in`、掲載出力を`.out`として収録している。ZIP展開後にそのまま使う場合は、例えば次で実行できる。

```bash
g++ -std=c++20 -O2 -I . examples/example01_workers.cpp -o sample
./sample < examples/example01_workers.in
```

プログラム内の`assert`による確認を有効にするため、例の検証時は`-DNDEBUG`を付けない。入力は各例に記した形式・範囲を満たすことを前提にする。意味上の不正を扱う例は、第3.16節にまとめる。

全18例をGCC 13.3.0・C++20・上記の警告オプションで個別にビルドし、警告0件で実行した。時刻・資源容量・先行関係・モード・目的値・ID変換を別の検査処理でも照合している。GCC 12.2.0での実行検証は、この環境では行っていない。

## 1. 何を解くライブラリか？

### 1.1 「何を、いつ、どの方法で実行するか」を決める

例えば、複数の製品を作る工場を考える。各製品には加工・検査・梱包があり、加工が終わるまで検査を始められない。一方、検査員や加工機は複数の製品で共有するため、同時に使える数にも上限がある。さらに、ある工程には「時間は短いが人手と費用が多い方法」と「時間は長いが省人化できる方法」があるかもしれない。

本ライブラリは、このような問題を次の要素で受け取る。

| 要素 | 意味 | 具体例 |
|---|---|---|
| タスク | 必ず1回実行する、中断できない仕事 | 1回の加工、1つの検査、1つの計算ジョブ |
| 資源 | 同時利用量に上限があり、仕事の終了後に再利用できるもの | 機械、作業員、計算枠、実行中だけ占有するメモリ |
| 先行制約 | 前のタスクが完了するまで次を開始できない関係 | 加工→検査、データ生成→データ利用 |
| モード | そのタスクを実行する方法の選択肢 | 機械Aで5分、機械Bで3分、2人で4分、4人で2分 |
| 開始可能時刻 | それ以前には開始できない時刻 | 材料到着時刻、ジョブ投入時刻 |
| 納期と重要度 | 早く終えたい仕事、遅れると困る仕事の優先度 | 出荷期限、遅延1単位あたりの損失 |

タスクの処理時間と資源使用量は、選んだモードによって決まる。実行途中でモードを切り替えたり、使用量を変えたりはしない。資源容量も計画の全期間で一定である。「電力を同時に何kW使えるか」は資源容量にできるが、「燃料を合計何Lまで使えるか」は使い切る量の制約なので別の問題になる。

返される主な結果は、各タスクの開始時刻と選択モードである。全タスクを実行することが前提であり、仕事を受注するかどうか、途中で仕事を破棄するかどうかは選ばない。

### 1.2 満たす制約

タスク数を$N$、資源数を$R$とする。タスク$i$のモード集合を$M_ {i}$、選択モードを$m_ {i}$、開始時刻を$S_ {i}$とする。各タスクは$M_ {i}$からちょうど1つのモードを選ぶ。

$$C_ {i}=S_ {i}+p_ {i,m_ {i}}$$

$p_ {i,m_ {i}}$は選んだモードの処理時間、$C_ {i}$は完了時刻である。開始可能時刻を$r_ {i}$、先行制約の集合を$E$とすると、次を満たす。

$$S_ {i}\ge r_ {i},\qquad C_ {i}\le S_ {j}\quad ((i,j)\in E)$$

$E$の$(i,j)$は「タスク$i$の完了後にタスク$j$を開始できる」という意味で、閉路のない有向グラフでなければならない。つまり、A→B→C→Aのように、前後関係をたどって同じタスクへ戻る指定はできない。

資源$r$の容量を$B_ {r}$、タスク$i$がモード$m$で実行中に使う量を$a_ {i,m,r}$とすると、全ての時刻$t$について次を満たす。

$$\sum_ {i:S_ {i}\le t<C_ {i}}a_ {i,m_ {i},r}\le B_ {r}\quad (0\le r<R)$$

使用区間は半開区間$[S_ {i},C_ {i})$である。あるタスクが時刻5に終われば、別のタスクは時刻5から同じ資源を使える。処理時間0のタスクは資源を占有する時間がない。

開始時刻・処理時間・資源量は整数で扱う。タスクごとの処理時間や開始可能時刻は入力では`int`、返却される開始時刻は`long long`である。小数時間を扱う場合は、例えば「1単位＝0.1秒」のように単位を決めて整数化する。

### 1.3 最小化する目的関数

組み込みの目的関数は、全体の終了時刻、各タスクの完了時刻、納期遅延、モード費用の重み付き和である。

$$F=w_ {M} C_ {\max}+\sum_ {i=0}^{N-1}w_ {i}^C C_ {i}+\sum_ {i:d_ {i}\ge0}w_ {i}^T\max(0,C_ {i}-d_ {i})+w_ {K}\sum_ {i=0}^{N-1}k_ {i,m_ {i}}$$

| 記号 | 意味 | 対応するデータ |
|---|---|---|
| $F$ | 小さいほど良い目的値 | 返却値の`objective` |
| $N$ | タスク数 | `problem.tasks.size()` |
| $i$ | タスクID。0から$N-1$ | `problem.tasks[i]`の添字 |
| $m_ {i}$ | タスク$i$で選んだモードの添字 | `schedule.mode_of_task[i]` |
| $S_ {i}$ | タスク$i$の開始時刻 | `schedule.start_time[i]` |
| $p_ {i,m_ {i}}$ | 選択モードの処理時間 | `problem.tasks[i].modes[m_i].duration` |
| $C_ {i}$ | タスク$i$の完了時刻 | $S_ {i}+p_ {i,m_ {i}}$で計算 |
| $C_ {\max}$ | 全タスクの完了時刻の最大値。タスク0個なら0 | 各$C_ {i}$の最大値 |
| $w_ {M}$ | 全体の終了時刻を1単位遅らせる費用 | `problem.makespan_weight` |
| $w_ {i}^C$ | タスク$i$の完了時刻を1単位遅らせる費用 | `problem.tasks[i].completion_weight` |
| $d_ {i}$ | タスク$i$の納期。負値の指定は納期なしを表す`-1`だけ | `problem.tasks[i].due_time` |
| $w_ {i}^T$ | タスク$i$の納期超過1単位あたりの費用 | `problem.tasks[i].tardiness_weight` |
| $k_ {i,m_ {i}}$ | 選択モードを1回使う費用 | `problem.tasks[i].modes[m_i].fixed_cost` |
| $w_ {K}$ | モード費用を目的値へ換算する係数 | `problem.mode_cost_weight` |

第1項は「最後の仕事が終わるまでの時間」を短くする。第2項は、全体の終了時刻が同じでも重要な仕事を先に終えることを評価する。第3項は納期を超えた部分だけを評価し、納期前に終えた仕事の遅延費用は0になる。第4項は時間と実行費用の交換条件を決める。

例えば、2つのタスクの完了時刻が3と5、後者の納期が4、後者だけに完了重み2・遅延重み3を付けるとする。全体終了時刻の重みは1、選択モードの費用合計は2、その換算係数は4とする。このとき目的値は、全体終了の5、後者の完了費用の10、遅延費用の3、モード費用の8を合わせて26になる。

重みは有限の非負値で、使わない項は重みを0にする。モードの`fixed_cost`自体には負値も使える。全体終了時刻だけを最小化したい場合は`makespan_weight=1`、各タスクの2つの重みを0、`mode_cost_weight=0`とする。

`due_time`は超過を禁止する時刻ではない。納期を必ず守る必要がある場合、遅延重みを大きくするだけでは制約の保証にならない。

### 1.4 独自の目的関数を使う場合

組み込み式では表せないときは、問題とスケジュールを受け取り、最小化する有限値を返す`Evaluator`を指定できる。例えば「各作業の終了後に残る配送時間まで含めた全体終了時刻」を評価できる。

対象は、モードを固定したとき、タスクを余分に遅らせるだけで良くなることのない目的である。早すぎる完成への罰則、安い時刻帯まで意図的に待つ利益などが重要なら、本ライブラリが扱う目的の条件に合わない。独自Evaluatorは評価方法を変える仕組みであり、資源容量以外の任意の制約を自動的に追加する仕組みではない。


## 2. APIリファレンス

### 2.1 全体の見取り図

公開APIは**9種類のデータ構造体、スケジューリング用関数の8呼出し形式、2種類の出力演算子**である。以下に全てを載せる。専用のsolverオブジェクトや、引数付きの専用コンストラクタはない。構造体を作り、公開フィールドへ値を入れて、自由関数を呼ぶ。

**公開の列挙型はない。** 利用者が指定する探索方式や資源表の方式を選ぶenumもない。タスクID、資源ID、モードIDは0始まりの整数であり、列挙値ではない。`due_time=-1`、`max_iterations=-1`などの特別な整数値は、各フィールドの表で意味を明記する。

| 型 | 役割 |
|---|---|
| `scheduling_resource` | 一つの資源の同時使用量上限 |
| `scheduling_resource_use` | 一つのモードが一つの資源を使う量 |
| `scheduling_mode` | 一つのタスクの実行方法 |
| `scheduling_task` | 必ず一度実行する仕事 |
| `scheduling_precedence` | タスク間の前後関係 |
| `resource_scheduling_problem` | 資源・全タスク・前後関係・全体の重み |
| `resource_schedule` | 優先順序・選択モード・開始時刻 |
| `resource_scheduling_options` | 時間、終了期限、反復数、乱数seed |
| `resource_scheduling_result` | 返却解、目的値、探索カウンタ |

各構造体の宣言時に、以下の既定値が使われる。vectorは空で作られる。「既定値がある」ことと「そのままで有効な入力である」ことは別である。特にタスクの`modes`は追加が必須であり、資源使用の`amount=0`と先行辺の`before=after=0`も、そのままでは登録できない。

### 2.2 資源と資源使用量

**`scheduling_resource`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `capacity` | `int` | `1` | 同時使用量の上限。1以上。時間によらず一定で、0容量は登録できない |

**`scheduling_resource_use`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `resource` | `int` | `0` | `problem.resources`の添字。0以上、資源数未満 |
| `amount` | `int` | `0` | 選択モードの実行中に占有する量。登録時は1以上、対象資源のcapacity以下 |

資源を使わない場合は`amount=0`の要素を作らず、使用一覧からその資源を省く。各モードの使用一覧では、同じ資源IDを二度登録できない。時間0のモードも入力値の検査を受けるので、0容量や容量を超える量を登録してよいわけではない。

### 2.3 モードとタスク

**`scheduling_mode`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `duration` | `int` | `1` | 処理時間。0以上。0なら瞬時に完了し、資源を占有する時間はない |
| `renewable_uses` | `std::vector<scheduling_resource_use>` | 空 | 実行中だけ占有し、終了時に返す資源の一覧。空なら資源を使わない |
| `fixed_cost` | `long double` | `0` | この方法を1回使う固定費用。有限値。負値も利用できる |

**`scheduling_task`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `release_time` | `int` | `0` | 開始可能時刻。0以上。開始時刻はこの値以上になる |
| `due_time` | `int` | `-1` | -1は納期なし、0以上は納期。-2以下は不正。超過を禁止する条件ではない |
| `modes` | `std::vector<scheduling_mode>` | 空 | 実行方法の候補。各タスク1個以上が必要。入力順の添字がモードID |
| `completion_weight` | `long double` | `0` | 完了時刻に掛ける重み。有限かつ0以上。0ならこの目的項を使わない |
| `tardiness_weight` | `long double` | `0` | 納期超過時間に掛ける重み。有限かつ0以上。0ならこの目的項を使わない |

一つのタスクで選ばれるモードはちょうど一つ。モードの並び順を変えるとモードIDの意味も変わる。返されたモードを読むときや外部解を再利用するときは、対応するタスクの`modes`と照合する。

`due_time=-1`のとき、遅延の目的項は0になる。ただし`tardiness_weight`は初期の優先度を作る処理では参照される。納期も遅延の意図もないタスクでは、`tardiness_weight=0`を基本にする。

全モードが入力検査の対象になる。例えば「候補に必要人数100の方法があっても、選ばなければ容量10の問題で使える」という扱いにはならない。不可能な方法は候補から除いておく。

### 2.4 先行制約と問題全体

**`scheduling_precedence`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `before` | `int` | `0` | 先に完了するタスクID。0以上、タスク数未満 |
| `after` | `int` | `0` | その完了後に開始できるタスクID。0以上、タスク数未満。beforeと異なること |

辺`{a,b}`は「bの開始時刻をaの完了時刻以上にする」という意味である。同時刻にaが完了しbが始まることはできる。bの開始をaの直後に固定する意味ではない。全ての辺を合わせて閉路を作らないことが必要になる。

**`resource_scheduling_problem`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `resources` | `std::vector<scheduling_resource>` | 空 | 全資源。添字が資源ID。資源を使わない問題では空でよい |
| `tasks` | `std::vector<scheduling_task>` | 空 | 全タスク。添字がタスクID。空問題も有効 |
| `precedences` | `std::vector<scheduling_precedence>` | 空 | 全ての先行辺。辺なしも有効 |
| `makespan_weight` | `long double` | `1` | 最も遅い完了時刻の重み。有限かつ0以上 |
| `mode_cost_weight` | `long double` | `1` | 選択モードの固定費用合計の重み。有限かつ0以上 |

全体終了時刻だけを小さくしたい場合は、`makespan_weight=1`、`mode_cost_weight=0`、全タスクの完了重み・遅延重みを0にする。単に`makespan_weight=0`にしても、タスクに設定した重みは消えない。

問題の公開フィールドは次の呼出し前に更新できる。容量、時間、候補モード、重み、先行辺、タスク数も変更可能だが、呼出しごとに検証と内部準備をやり直す。内部データを保持して差分だけを反映する専用APIはない。元解を使うなら、新問題のID体系で有効かを検査してから`improve`へ渡す。

### 2.5 解の表現

**`resource_schedule`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `task_order` | `std::vector<int>` | 空 | 各タスクIDを1回ずつ含む優先順列。全先行辺でbeforeがafterより前に並ぶ必要がある |
| `mode_of_task` | `std::vector<int>` | 空 | 添字iはタスクID。値は0以上、`problem.tasks[i].modes.size()`未満 |
| `start_time` | `std::vector<long long>` | 空 | 添字iはタスクID。値は0以上で、開始可能時刻・先行制約・資源容量を満たすこと |

3配列の長さはいずれもタスク数と一致させる。`task_order[k]`は「順序のk番目のタスクID」だが、`mode_of_task[k]`と`start_time[k]`は「タスクkの値」である。順序のk番目の仕事の開始時刻は`start_time[task_order[k]]`で読む。

`task_order`は開始時刻順ではない。例えば、順序の先頭にある仕事の開始可能時刻が10で、次の仕事が資源を取り合わなければ、次の仕事が時刻0に始まる場合がある。

外部から渡す開始時刻には、必要以上の待機時間があってもよい。ただし、開始時刻に処理時間を加えた値が`long long`に収まることも検査される。

### 2.6 探索オプション

**`resource_scheduling_options`の全フィールド**

| フィールド | 型 | 既定値 | 意味・有効な値 |
|---|---|---|---|
| `time_limit_ms` | `double` | `1950.0` | 初期構築後の相対改善時間。有限値。0以下なら改善段階に進まない |
| `deadline` | `std::optional<std::chrono::steady_clock::time_point>` | `std::nullopt` | 絶対終了期限。未設定なら使わない。設定時は相対時間による終了時刻と早い方を使う |
| `seed` | `std::uint64_t` | `1` | 乱数seed。0も有効で、uint64_tの範囲を使える |
| `max_iterations` | `long long` | `-1` | -1は反復数上限なし、0は初期構築まで、正値は追加提案数の上限。-2以下は不正 |

注意する値の組合せは次のとおり。

- `time_limit_ms=0`でも入力検証と最初の解の構築・評価は行う。`improve`では元解との比較も行う。
- `max_iterations=0`でも`improve`は元解の時刻をそのまま返すとは限らない。入力された順序・モードで配置し直した解と、元の開始時刻を持つ解を比較する。
- 相対時間・絶対期限・反復数の上限を併用したときは、先に達した停止条件が有効になる。空問題や、順序とモードが一意の問題では上限まで使い切らない。
- `deadline`へ渡すのは`steady_clock`の時刻。現実世界の日付を示す`system_clock`や、スケジュール上のタスクの納期ではない。
- 過去の`deadline`でも初期構築は省略されない。停止時刻を定期的に確認する方式なので、厳密なms上限は保証されない。提出では検査・初期化・出力などの時間も考えて余裕を取る。
- `time_limit_ms`のNaNと無限大は不正。表現範囲を超える有限の巨大値は、相対終了時刻を`steady_clock::time_point::max()`に収める。
- 固定seed・固定反復数でも、冷却の進捗に経過時間が使われるので、実行環境をまたぐ完全一致は保証されない。

目安として、通常の利用では実際に使える改善時間を指定する。複数試行や他処理と時間を共有するときは`deadline`を設定する。比較・デバッグではseedと反復数も記録する。温度や近傍確率を指定する公開フィールドはない。

### 2.7 戻り値

**`resource_scheduling_result`の全フィールド**

| フィールド | 型 | 既定値 | 戻り値としての意味 |
|---|---|---|---|
| `schedule` | `resource_schedule` | 3配列とも空 | 得られた最良の実行可能解 |
| `objective` | `long double` | `0` | 呼出しに用いた目的関数で評価した値。小さいほどよい |
| `iterations` | `long long` | `0` | 追加初期順序の評価数と、その後の近傍提案数の合計。初回配置を含まず、無効な提案は含む |
| `decoded_schedules` | `long long` | `0` | 実際に配置・評価した候補数。非空問題なら初回配置の1回を含む |
| `accepted_moves` | `long long` | `0` | 探索中に受理した有効な変更数。同値・悪化の受理も含む。追加初期順序の採用は含まない |

カウンタはその呼出し1回の値である。外側で何度も呼ぶ場合は、合計が必要なら利用者側で加える。`improve`で比較する元解の評価は`decoded_schedules`に含まれない。公開`evaluate`を後から呼んでも、既に返ったカウンタは増えない。

空問題では解の3配列は空、3カウンタは0になる。独自Evaluatorは空解に対しても1回呼ばれ、その値が`objective`になる。組み込み目的値は0である。実行可能性フラグや「最適解である」フラグは持たない。

### 2.8 検査・評価の関数

以下のシグネチャは説明用であり、ヘッダをincludeした後に再宣言する必要はない。`p`と`s`は書き換えられない。

| シグネチャ | 引数・既定値 | 戻り値と動作 |
|---|---|---|
| `void resource_scheduling_validate_problem(const resource_scheduling_problem& p)` | pは必須 | 問題のみを検査する。正常なら戻り、不正なら`std::invalid_argument` |
| `bool resource_scheduling_is_feasible(const resource_scheduling_problem& p, const resource_schedule& s)` | 両方必須 | pを検査後、sが実行可能ならtrue、解だけが不正ならfalse。p自体が不正なら例外 |
| `long double resource_scheduling_evaluate(const resource_scheduling_problem& p, const resource_schedule& s)` | 両方必須 | pとsを検査し、組み込み目的値を返す。解が不正、または値が非有限なら例外 |
| `template<class Evaluator> long double resource_scheduling_evaluate(const resource_scheduling_problem& p, const resource_schedule& s, Evaluator evaluator)` | 全て必須。evaluatorは値渡し | pとsを検査し、独自Evaluatorの有限値を返す。不正なら例外 |

`validate_problem`は問題を修正しない。タスクIDやモードを自動補完せず、検査結果をキャッシュするオブジェクトも返さない。`is_feasible`のfalseを、問題自体の不正も全て通知する値だと考えないことが大切である。

`evaluate`は値だけを計算する軽い関数ではなく、問題と解の検証も行う。候補ごとの採点で呼ぶとその検査費用も繰り返しかかるので、独自Evaluatorの内部では必要な目的項を直接計算する例を採用している。

### 2.9 新規に解く関数

| シグネチャ | 引数・既定値 | 戻り値と動作 |
|---|---|---|
| `resource_scheduling_result resource_scheduling_solve(const resource_scheduling_problem& p, const resource_scheduling_options& options = {})` | pは必須。optionsは省略時に第2.6節の既定値 | 新しい実行可能解を作り、組み込み目的値を小さくする |
| `template<class Evaluator> resource_scheduling_result resource_scheduling_solve(const resource_scheduling_problem& p, Evaluator evaluator, const resource_scheduling_options& options = {})` | pとevaluatorは必須。optionsだけ省略可能 | 新しい実行可能解を作り、独自目的値を小さくする |

`solve`へ外部の初期解を渡す引数はない。元解を使いたいときは次の`improve`を使う。求まるのは本モデルの制約を満たす解で、一般の入力での最適性は保証されない。

### 2.10 元解を改善する関数

| シグネチャ | 引数・既定値 | 戻り値と動作 |
|---|---|---|
| `resource_scheduling_result resource_scheduling_improve(const resource_scheduling_problem& p, const resource_schedule& initial, const resource_scheduling_options& options = {})` | pとinitialは必須。optionsは省略可能 | 組み込み目的値で元解以下の結果を返す |
| `template<class Evaluator> resource_scheduling_result resource_scheduling_improve(const resource_scheduling_problem& p, const resource_schedule& initial, Evaluator evaluator, const resource_scheduling_options& options = {})` | p、initial、evaluatorは必須。optionsだけ省略可能 | 同じ決定的な独自評価で元解以下の結果を返す |

initialは**呼出し時のpに対して実行可能な全タスクの解**であることが必要。不正なら`std::invalid_argument`であり、実行不能解を修復するAPIではない。初期解の順序・モード・開始時刻を固定する指定でもない。同値の解が複数ある場合に、配列や開始時刻が元と同じである保証はない。

改善前後で比較する目的関数は同一にする。例えば独自評価で改善した結果について、組み込み目的値まで改善する保証はない。pを変更したなら、過去のresult.objectiveではなく、変更後のpで元解を評価した値と比較する。

### 2.11 独自Evaluatorの契約

呼出し形は`evaluator(const resource_scheduling_problem&, const resource_schedule&)`である。戻り値は`long double`へ変換できる型とし、その値は有限でなければならない。小さい値ほど良い。

| 項目 | 必要な扱い |
|---|---|
| 値の意味 | 求めたい目的を全て自分で返す。組み込みの4項は自動加算されない |
| 同じ入力 | 同じ問題・同じ解なら同じ値を返す。乱数や呼出し回数で評価値を変えない |
| 時刻への性質 | モードを固定したとき、余計に遅らせるだけで得をしない目的を使う |
| データの変更 | 渡された問題・解を書き換えない。別の診断カウンタを増やすことは可能 |
| 空問題 | 空のtasksと空の解を受け取っても有限値を返せるようにする |
| 所有・寿命 | 引数は値渡しで、その後moveされる。lambdaや関数オブジェクトを渡せる。move-onlyの評価器はmoveして渡す |
| 外部オブジェクトの参照 | `std::ref`や参照captureを使うときは、参照先を呼出し終了まで生存させる |
| 入力の検査 | 独自評価でも、problem内の全重み・モード・資源は通常どおり検証される |

独自Evaluatorは任意の必須制約を追加する仕組みではない。違反に大きな罰則を付けても、その制約を満たす解が見つかる保証は増えない。特に、早すぎる完了を嫌う評価や、安い時間帯まで待つことで有利になる評価では、本ライブラリが探す解の形と合わないことがある。

### 2.12 デバッグ出力演算子

| シグネチャ | 表示内容 |
|---|---|
| `std::ostream& operator<<(std::ostream& out, const resource_schedule& s)` | `task_order`、`mode_of_task`、`start_time`の名前と値 |
| `std::ostream& operator<<(std::ostream& out, const resource_scheduling_result& result)` | objective、3カウンタ、scheduleの内容 |

引数は両方必須で、返すのは渡されたout自身への参照。`std::cout << result`、`std::cerr << result.schedule`の形で利用でき、前後の文字列と連結できる。各配列は**先頭20要素まで**で、残りがあれば`... (5 more)`のように省略数を出す。空配列は`[]`になる。

明示的な改行・flushは行わないので、改行は呼出し側で付ける。数値はストリームの現在の書式・精度に従い、演算子は書式フラグや精度を変更しない。配列はコピー・全走査せず、配列長に対して処理量と補助メモリは$O(1)$である。出力中に解の検証や再評価は行わず、長さが揃っていない作成途中の解も表示できる。

表示はデバッグ用であり、全要素を復元できる保存形式ではない。コンテストで決められた標準出力は、各例のように必要な値を取り出して別途作る。

### 2.13 共通の制約と例外

| 内容 | 条件・注意 |
|---|---|
| 整数時刻 | 入力の時間・開始可能時刻は0以上のint。返却開始時刻と時刻の加算はlong longで扱う |
| 件数 | タスク数・資源数・各モード数・内部の添字がintで表せる範囲に収める。極端な件数の全てを安全検査する設計ではない |
| 数値 | 全重み・費用・評価値は有限。重みは非負、固定費用は負も可。時刻範囲の加算や評価結果の非有限化は検査される |
| 容量 | 全期間で一定の再生可能資源だけ。合計燃料・材料消費・予算などの使い切る資源の上限ではない |
| 実行条件 | 全タスク必須、実行途中の中断なし。モードの連動条件、開始時刻固定、外部予約は直接指定できない |
| 前後関係 | 完了後に開始できる下限だけ。直後開始、待機中の機械保持、直前の仕事に依存する段取り時間は表していない |
| 納期 | due_timeは超過可能な評価基準。必須期限ではない |
| 実行スレッド | 単一スレッドで利用する |
| 例外 | 不正な問題・設定・improveの初期解、非有限の評価値などで主に`std::invalid_argument`。Evaluator自身が投げた例外はそのまま伝播する |

このリファレンスは利用者へ提供される型・フィールド・関数を対象とする。ヘッダを直接コンパイルしたときだけ有効になるテスト用定義は利用APIに含めない。


## 3. ユースケース

全ての時刻・処理時間・資源量は、入力で決めた同一単位を使う。例えば時刻の1単位を1分、人数の1単位を1人と決める。整数IDは0始まりである。

多くの例では、共通の出力形式を使う。1行目は目的値、2行目はタスク数、以降はタスクID順の`task mode start finish`である。`print_answer`は各例の中で定義する出力用関数であり、ライブラリのAPIではない。この関数も含めてコピーする。

入力例に対する掲載出力は実際に実行して得た一例である。同値の解が複数ある場合や、実行時間による停止位置が変わる場合は、時刻・順序・カウンタが変わり得る。一般の問題で最適性を保証する出力ではない。実行可能性と、その解に対応する目的値を確認する。

多くの例は読みやすい小問題なので、改善時間1000msに加えて提案回数2000回の上限も設定している。本番用の推奨回数ではなく、例を短時間で動かすための設定である。各例のコメントを見て、問題規模と使える時間に合わせて変更する。

| 節 | ユースケース | 主に使う機能 |
|---|---|---|
| 3.1 | 作業員を共有する工程計画 | 問題・資源・モード・先行辺、組み込みsolve、実行可能性検査 |
| 3.2 | CPUとメモリを同時に使う計算ジョブ | 複数資源、renewable_uses、使用量0の省略 |
| 3.3 | 人員・速さ・費用が異なる実行方法を選ぶ | 複数モード、fixed_cost、mode_cost_weight |
| 3.4 | 同じ性能の複数機械に実際の機械番号を割り当てる | 容量でまとめた機械群、返却開始時刻の利用 |
| 3.5 | 製品ごとに通る機械が決まっているJob Shop | 資源のassign、タスクの追加、先行辺、ID変換 |
| 3.6 | 代替機械を選べるFlexible Job Shop | 機械選択をモードで表す、選択モードから資源IDを復元する |
| 3.7 | 材料の到着・納期・重要度をまとめて考慮する | release_time、due_time、両タスク重み、両問題重み、組み込みevaluate |
| 3.8 | 複数プロジェクトの完了時刻を評価する | 時間0のタスク、案件単位の完了・遅延、IDの変換 |
| 3.9 | 配送・後処理の終了まで含む独自評価 | 独自solve・improve・evaluate、lambda Evaluator、外部データの参照 |
| 3.10 | 外部で作った計画を検査して改善する | resource_scheduleの手動作成、is_feasible、組み込みevaluate・improve |
| 3.11 | 次ターンの容量・開始可能時刻を反映して再計画する | 公開フィールドの更新、変更後の検査、solve/improveの切替 |
| 3.12 | 完了済みの仕事を除いた残り工程を解く | 問題の組立て直し、タスクIDの圧縮・復元、release_timeの更新 |
| 3.13 | 共通の終了期限で複数のseedを試す | deadline、time_limit_ms、seed、max_iterations=-1、複数試行 |
| 3.14 | 反復数を決めて探索し、短いデバッグ表示で確認する | 全探索オプション、全結果フィールド、2種類のoperator<<、20要素の上限 |
| 3.15 | 人手・機械・冷却で資源使用が変わる工程 | 資源なしの正時間タスク、複数段階への分割、最低待ち時間の表現 |
| 3.16 | 入力問題と外部スケジュールを分けて検査する | validate_problem、is_feasibleのfalse、evaluateの例外、解のデバッグ表示 |
| 3.17 | 資源を使わない直列処理と大きな時刻 | 空資源、時間0、solve/improveの既定引数、long long時刻、空問題 |
| 3.18 | 最大の納期遅れを抑え、評価器を参照で渡す | 関数オブジェクト、std::ref、独自目的の最大値、評価器の寿命 |

### 3.1 作業員を共有する工程計画

建設現場や検査場で、同じ作業員チームを複数の仕事に振り分ける。前工程が終わった仕事から着手でき、必要人数の合計が現場の人数を超えないようにする。

**具体的に解く問題**

各仕事の時間と必要人数、前後関係を入力し、全仕事が終わる時刻を小さくする。全仕事は必須で、途中中断はしない。

**入力**

`N E B`、続いてN行の`duration workers`、続いてE行の`before after`。Nは0以上、Bは正、durationは0以上、workersは1〜B。タスクIDは0〜N-1、辺は閉路を作らない。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**コード：`example01_workers.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各仕事の時間と必要人数、前後関係を入力し、全仕事が終わる時刻を小さくする。全仕事は必須で、
// 途中中断はしない。

// 入力: N E B、続いてN行のduration workers、続いてE行のbefore after。Nは0以上、Bは正、
// durationは0以上、workersは1〜B。タスクIDは0〜N-1、辺は閉路を作らない。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, e, capacity;
    if (!(std::cin >> n >> e >> capacity)) {
        return 0;
    }

    resource_scheduling_problem p; // 問題全体を作る
    p.resources = {{capacity}}; // 資源0を「作業員全員」、容量を人数とする
    p.tasks.resize(n); // 入力順をタスクIDとする
    p.makespan_weight = 1; // 最後の仕事が終わる時刻を最小化する
    p.mode_cost_weight = 0; // この例では実行費用を評価しない

    for (int i = 0; i < n; ++i) {
        int duration, workers;
        std::cin >> duration >> workers;

        p.tasks[i].modes = {{duration, {{0, workers}}, 0}}; // 時間・資源0の人数・費用0の1モード
    }

    for (int k = 0; k < e; ++k) {
        int a, b;
        std::cin >> a >> b;

        p.precedences.push_back({a, b}); // aの完了後にbを開始できる
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 実行可能な計画を作って改善する

    print_answer(p, result); // タスクID順の時刻を出力する
}
```

**入力例**

```text
4 3 3
       3 2
       2 1
       4 2
       1 1
       0 2
       1 2
       2 3
```

**出力例（標準出力）**

```text
8
4
0 0 0 3
1 0 0 2
2 0 3 7
3 0 7 8
```

### 3.2 CPUとメモリを同時に使う計算ジョブ

ジョブを並列実行するとき、CPU枠が足りていてもメモリが不足することがある。実行中だけ保持する資源を種類ごとに指定する。

**具体的に解く問題**

各ジョブの実行時間、CPU枠、メモリ枠、依存関係を入力し、全体の終了時刻を小さくする。ジョブ終了時に両方の資源を返す。

**入力**

`N E CPU_CAP MEM_CAP`、N行の`duration cpu memory`、E行の`before after`。容量は正、使用量は0以上かつ対応する容量以下。メモリは例として1単位=1GB。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**利用上の注意**

終了後も成果物がメモリを占有し続けるなら、その占有はこのモデルでは表していない。

**コード：`example02_cpu_memory.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各ジョブの実行時間、CPU枠、メモリ枠、依存関係を入力し、全体の終了時刻を小さくする。
// ジョブ終了時に両方の資源を返す。

// 入力: N E CPU_CAP MEM_CAP、N行のduration cpu memory、E行のbefore after。容量は正、
// 使用量は0以上かつ対応する容量以下。メモリは例として1単位=1GB。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 注意: 終了後も成果物がメモリを占有し続けるなら、その占有はこのモデルでは表していない。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, e, cpu_cap, mem_cap;
    if (!(std::cin >> n >> e >> cpu_cap >> mem_cap)) {
        return 0;
    }

    resource_scheduling_problem p; // ジョブ全体を一つの問題として作る
    p.resources = {{cpu_cap}, {mem_cap}}; // 資源0=CPU枠、資源1=実行中のメモリ枠
    p.tasks.resize(n); // ジョブ番号をタスクIDにする
    p.mode_cost_weight = 0; // makespan_weightは既定値1のまま使う

    for (int i = 0; i < n; ++i) {
        int duration, cpu, memory;
        std::cin >> duration >> cpu >> memory;

        scheduling_mode mode; // このジョブの唯一の実行方法を作る
        mode.duration = duration; // 処理時間を整数単位で指定する

        if (cpu > 0) {
            mode.renewable_uses.push_back({0, cpu}); // CPUを使う場合だけ登録する
        }

        if (memory > 0) {
            mode.renewable_uses.push_back({1, memory}); // メモリを使う場合だけ登録する
        }

        p.tasks[i].modes.push_back(mode); // 使用量0の要素は登録せず、省略する
    }

    for (int k = 0; k < e; ++k) {
        int a, b;
        std::cin >> a >> b;

        p.precedences.push_back({a, b}); // データ生成aから利用bへの依存関係
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 2種類の容量を同時に満たす

    print_answer(p, result); // CPUとメモリの検査を含めて出力する
}
```

**入力例**

```text
4 1 4 8
       4 2 6
       3 2 4
       2 1 2
       1 0 0
       0 3
```

**出力例（標準出力）**

```text
7
4
0 0 0 4
1 0 4 7
2 0 0 2
3 0 4 5
```

### 3.3 人員・速さ・費用が異なる実行方法を選ぶ

通常作業と特急作業のように、同じ仕事でも実行方法によって時間・必要人数・費用が変わる。早さだけでなく特急費用も含めて選びたい場合に使う。

**具体的に解く問題**

各仕事の候補モードと先行辺を入力し、「全体終了時刻＋alpha×選択モード費用の合計」を小さくする。候補の費用は1回の実行で払う固定費用である。

**入力**

`N E B alpha`。各タスクについて`K`、続いてK行の`duration workers cost`。最後にE行の`before after`。Kは1以上、workersは1〜B、alphaは有限かつ0以上、costは有限。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**コード：`example03_modes_cost.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各仕事の候補モードと先行辺を入力し、
// 「全体終了時刻＋alpha×選択モード費用の合計」を小さくする。
// 候補の費用は1回の実行で払う固定費用である。

// 入力: N E B alpha。各タスクについてK、続いてK行のduration workers cost。最後にE行のbefore after。
// Kは1以上、workersは1〜B、alphaは有限かつ0以上、costは有限。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, e, capacity;
    long double alpha;
    if (!(std::cin >> n >> e >> capacity >> alpha)) {
        return 0;
    }

    resource_scheduling_problem p; // 方法選択を含む問題を作る
    p.resources = {{capacity}}; // 全モードで共有する作業員の人数
    p.tasks.resize(n); // 一つのタスクに複数モードを登録できる
    p.makespan_weight = 1; // 全体終了時刻の係数
    p.mode_cost_weight = alpha; // 費用1単位を時間何単位と交換するかを決める

    for (int i = 0; i < n; ++i) {
        int k;
        std::cin >> k;

        for (int m = 0; m < k; ++m) {
            int duration, workers;
            long double cost;
            std::cin >> duration >> workers >> cost;

            p.tasks[i].modes.push_back({duration, {{0, workers}}, cost}); // 入力順がモードID
            // 選ばれない候補も検証されるので、全モードの必要人数を容量以下にする
        }
    }

    for (int k = 0; k < e; ++k) {
        int a, b;
        std::cin >> a >> b;

        p.precedences.push_back({a, b}); // 方法にかかわらず共通する前後関係
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 順序と選択モードを決める

    print_answer(p, result); // mode列で各タスクの採用方法を確認できる
}
```

**入力例**

```text
3 2 3 0.5
       2
       5 1 0
       2 3 4
       2
       4 1 0
       2 2 3
       1
       2 1 0
       0 2
       1 2
```

**出力例（標準出力）**

```text
7
3
0 0 0 5
1 0 0 4
2 0 5 7
```

### 3.4 同じ性能の複数機械に実際の機械番号を割り当てる

同じ仕事を同じ時間で処理できる機械が複数台ある。まず「同時に何台まで使えるか」で計画し、その後で空いている実機に割り当てる。

**具体的に解く問題**

N個の独立した仕事をM台の同一機械で処理し、最後の終了時刻を小さくする。返却時刻を保ったまま0〜M-1の機械番号を付ける。

**入力**

`N M`、続いてN個の正の処理時間。Nは0以上、Mは1以上。処理時間0はこの実機割当例の入力対象外。

**出力**

1行目は全体終了時刻、2行目はN、以降は`task machine start finish`。機械番号は0始まり。

**利用上の注意**

機械ごとの速度差・適性・移動費用がある場合は、このまとめ方を使わず、モードに機械を割り当てる。

**コード：`example04_identical_machines.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: N個の独立した仕事をM台の同一機械で処理し、最後の終了時刻を小さくする。
// 返却時刻を保ったまま0〜M-1の機械番号を付ける。

// 入力: N M、続いてN個の正の処理時間。Nは0以上、Mは1以上。処理時間0はこの実機割当例の入力対象外。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 1行目は全体終了時刻、2行目はN、以降はtask machine start finish。機械番号は0始まり。

// 注意: 機械ごとの速度差・適性・移動費用がある場合は、このまとめ方を使わず、
// モードに機械を割り当てる。

int main() {
    int n, machines;
    if (!(std::cin >> n >> machines)) {
        return 0;
    }

    resource_scheduling_problem p; // 同一機械群を一つの累積資源で表す
    p.resources = {{machines}}; // 容量は機械の台数
    p.tasks.resize(n); // 仕事の入力順をタスクIDとする
    p.mode_cost_weight = 0; // 全体終了時刻だけを評価する

    for (int i = 0; i < n; ++i) {
        int duration;
        std::cin >> duration;

        p.tasks[i].modes = {{duration, {{0, 1}}, 0}}; // どの仕事も機械を1台使う
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // ここでは機械番号までは決まらない
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 同時稼働台数を検査する
    std::vector<int> order(n), machine_of(n, -1);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return std::pair(result.schedule.start_time[a], a) // 優先順列ではなく開始時刻で並べる
             < std::pair(result.schedule.start_time[b], b); // 同時刻はタスクID順にする
    });
    std::vector<long long> free_at(machines, 0);

    for (int i : order) {
        const long long start = result.schedule.start_time[i]; // この時刻は変更しない

        for (int m = 0; m < machines; ++m) {
            if (free_at[m] > start) {
                continue;
            }

            machine_of[i] = m;
            free_at[m] = start + p.tasks[i].modes[0].duration; // 処理中の実機を占有する
            break;
        }

        assert(machine_of[i] >= 0); // 正の処理時間と容量制約から必ず実機が空く
    }

    std::cout << result.objective << '\n'; // 全仕事の終了時刻
    std::cout << n << '\n';

    for (int i = 0; i < n; ++i) {
        const long long s = result.schedule.start_time[i]; // タスクIDで開始時刻を読む

        std::cout << i << ' ' << machine_of[i] << ' ' << s << ' '
                  << s + p.tasks[i].modes[0].duration << '\n'; // id machine start finish
    }
}
```

**入力例**

```text
5 2
       4 3 3 2 2
```

**出力例（標準出力）**

```text
7
5
0 0 0 4
1 1 2 5
2 0 4 7
3 1 0 2
4 1 5 7
```

### 3.5 製品ごとに通る機械が決まっているJob Shop

製品Aは切削してから検査、製品Bは研磨してから切削、というように製品ごとの工程順が決まっている。同じ機械を使う工程同士が競合する。

**具体的に解く問題**

機械番号と処理時間の列を製品ごとに入力し、全製品の完了を早める。各機械は同時に1工程だけ処理する。

**入力**

`J M`。各製品について`K`、続いてK行の`machine duration`。Jは0以上、Mは正、Kは0以上、machineは0〜M-1、durationは0以上。

**出力**

全体終了時刻、工程総数、各工程の`job operation machine start finish`。全ての番号は0始まり。

**利用上の注意**

機械上の直前の製品で変わる段取り時間や、工程間で絶対に待てない条件は表現していない。

**コード：`example05_job_shop.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 機械番号と処理時間の列を製品ごとに入力し、全製品の完了を早める。
// 各機械は同時に1工程だけ処理する。

// 入力: J M。各製品についてK、続いてK行のmachine duration。Jは0以上、Mは正、Kは0以上、
// machineは0〜M-1、durationは0以上。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 全体終了時刻、工程総数、各工程のjob operation machine start finish。全ての番号は0始まり。

// 注意: 機械上の直前の製品で変わる段取り時間や、工程間で絶対に待てない条件は表現していない。

int main() {
    int jobs, machines;
    if (!(std::cin >> jobs >> machines)) {
        return 0;
    }

    resource_scheduling_problem p; // 全製品の工程を一つにまとめる
    p.resources.assign(machines, scheduling_resource{1}); // 各機械を容量1の別資源にする
    p.mode_cost_weight = 0; // 終了時刻だけを評価する

    std::vector<std::pair<int, int>> owner;
    std::vector<int> machine_of;

    for (int j = 0; j < jobs; ++j) {
        int operations;
        std::cin >> operations;

        int previous = -1;

        for (int k = 0; k < operations; ++k) {
            int machine, duration;
            std::cin >> machine >> duration;

            const int id = static_cast<int>(p.tasks.size()); // 全体で重複しないタスクIDを付ける

            scheduling_task task; // この工程のタスクを作る
            task.modes = {{duration, {{machine, 1}}, 0}}; // 指定機械を1台使う1モード
            p.tasks.push_back(task); // 全体の工程一覧へ追加する

            if (previous >= 0) {
                p.precedences.push_back({previous, id}); // 同じ製品の工程を順につなぐ
            }

            owner.push_back({j, k});
            machine_of.push_back(machine);
            previous = id;
        }
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 機械競合と製品内の工程順を守る
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 出力前に実行可能性を確認する

    std::cout << result.objective << '\n' << p.tasks.size() << '\n'; // 全体終了時刻と工程数

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 元の製品・工程番号に戻す
        const auto [j, k] = owner[i];
        const long long s = result.schedule.start_time[i]; // 全体タスクIDで時刻を取得する

        std::cout << j << ' ' << k << ' ' << machine_of[i] << ' ' << s << ' '
                  << s + p.tasks[i].modes[0].duration << '\n'; // job operation machine start finish
    }
}
```

**入力例**

```text
2 2
       2
       0 3
       1 2
       2
       1 4
       0 2
```

**出力例（標準出力）**

```text
6
4
0 0 0 0 3
0 1 1 4 6
1 0 1 0 4
1 1 0 4 6
```

### 3.6 代替機械を選べるFlexible Job Shop

ある工程を機械AでもBでも処理できるが、時間や利用料が違う。工程の順番を守りながら、空き具合に応じて機械も選びたい。

**具体的に解く問題**

各工程の候補を「機械・時間・費用」で入力し、全体終了時刻と利用費用の重み付き和を小さくする。1工程につき候補を一つ選ぶ。

**入力**

`J M alpha`。各製品で`K`、各工程で`A`、続いてA行の`machine duration cost`。Kは0以上、Aは1以上。alphaは有限かつ0以上、全候補の機械IDは有効にする。

**出力**

目的値、工程総数、各工程の`job operation mode machine start finish`。modeはその工程の候補番号。

**利用上の注意**

工程ごとのモードは独立に選ばれる。「前工程でAを選んだ場合だけ次工程のBを許す」といった組合せ制約は指定できない。

**コード：`example06_flexible_job_shop.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各工程の候補を「機械・時間・費用」で入力し、
// 全体終了時刻と利用費用の重み付き和を小さくする。1工程につき候補を一つ選ぶ。

// 入力: J M alpha。各製品でK、各工程でA、続いてA行のmachine duration cost。Kは0以上、Aは1以上。
// alphaは有限かつ0以上、全候補の機械IDは有効にする。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 目的値、工程総数、各工程のjob operation mode machine start finish。
// modeはその工程の候補番号。

// 注意: 工程ごとのモードは独立に選ばれる。
// 「前工程でAを選んだ場合だけ次工程のBを許す」といった組合せ制約は指定できない。

int main() {
    int jobs, machines;
    long double alpha;
    if (!(std::cin >> jobs >> machines >> alpha)) {
        return 0;
    }

    resource_scheduling_problem p; // 工程順と機械の選択を同時に表す
    p.resources.assign(machines, scheduling_resource{1}); // 各機械を容量1にする
    p.mode_cost_weight = alpha; // makespan_weightは既定値1、費用の係数だけ変更する

    std::vector<std::pair<int, int>> owner;

    for (int j = 0; j < jobs; ++j) {
        int operations;
        std::cin >> operations;

        int previous = -1;

        for (int k = 0; k < operations; ++k) {
            int alternatives;
            std::cin >> alternatives;

            const int id = static_cast<int>(p.tasks.size()); // 全製品で共通の工程IDを採番する

            scheduling_task task; // この工程の候補モードを集める

            for (int m = 0; m < alternatives; ++m) {
                int machine, duration;
                long double cost;
                std::cin >> machine >> duration >> cost;

                task.modes.push_back({duration, {{machine, 1}}, cost}); // 候補番号mがモードID
            }

            p.tasks.push_back(task); // 全候補を持つ工程を追加する

            if (previous >= 0) {
                p.precedences.push_back({previous, id}); // 製品内の工程順を指定する
            }

            owner.push_back({j, k});
            previous = id;
        }
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 機械選択と時刻を決める
    // 選択された機械の重複使用も検査する
    assert(resource_scheduling_is_feasible(p, result.schedule));

    std::cout << result.objective << '\n' << p.tasks.size() << '\n'; // 費用込みの目的値と工程数

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 返却配列をタスクID順に読む
        const auto [j, k] = owner[i];
        const int m = result.schedule.mode_of_task[i]; // モードID自体は機械番号ではない
        const auto& mode = p.tasks[i].modes[m]; // 採用候補の内容を参照する
        // この例では候補ごとに機械を一つだけ登録した
        const int machine = mode.renewable_uses[0].resource;
        const long long s = result.schedule.start_time[i]; // 工程の開始時刻

        std::cout << j << ' ' << k << ' ' << m << ' ' << machine << ' ' << s << ' '
                  << s + mode.duration << '\n'; // job operation mode machine start finish
    }
}
```

**入力例**

```text
2 2 0.5
       2
       2
       0 4 0
       1 2 2
       1
       0 2 0
       2
       1
       1 3 0
       2
       0 3 0
       1 1 3
```

**出力例（標準出力）**

```text
7.5
4
0 0 0 0 0 4
0 1 0 0 4 6
1 0 0 1 0 3
1 1 1 1 3 4
```

### 3.7 材料の到着・納期・重要度をまとめて考慮する

材料が届く前には着手できず、重要な注文は早めに終えたい。納期を過ぎた分に追加費用を付け、時間と費用を同時に評価する。

**具体的に解く問題**

開始可能時刻、納期、完了重み、遅延重みをタスクごとに与え、組み込み目的関数の4項全てを使う。

**入力**

`N E B wM wK`、N行の`duration workers release due completion_weight tardiness_weight fixed_cost`、E行の`before after`。dueは-1または0以上。重みは有限かつ非負。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**コード：`example07_timing_weights.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 開始可能時刻、納期、完了重み、遅延重みをタスクごとに与え、組み込み目的関数の4項全てを使う。

// 入力: N E B wM wK、N行のduration workers release due completion_weight tardiness_weight
// fixed_cost、E行のbefore after。dueは-1または0以上。重みは有限かつ非負。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, e, capacity;
    long double makespan_weight, cost_weight;
    if (!(std::cin >> n >> e >> capacity >> makespan_weight >> cost_weight)) {
        return 0;
    }

    resource_scheduling_problem p; // 時刻条件と評価重みを持つ問題
    p.resources = {{capacity}}; // 資源0は共有作業員
    p.tasks.resize(n); // タスクIDは入力順
    p.makespan_weight = makespan_weight; // 全体終了時刻の重み、0ならその項を無効化
    p.mode_cost_weight = cost_weight; // 選択したモードの固定費用に掛ける重み

    for (int i = 0; i < n; ++i) {
        int duration, workers, release, due;
        long double completion, tardiness, cost;
        std::cin >> duration >> workers >> release
            >> due >> completion >> tardiness >> cost;

        auto& task = p.tasks[i]; // タスクiの設定を参照する
        task.release_time = release; // この時刻より前の開始は禁止する
        task.due_time = due; // -1は納期なし、0以上でも超過を禁止する制約ではない
        task.completion_weight = completion; // 完了時刻に直接掛ける重み
        task.tardiness_weight = tardiness; // 納期超過部分だけに掛ける重み
        task.modes = {{duration, {{0, workers}}, cost}}; // 処理時間・必要人数・固定費用
        // 納期を設定してもtardiness_weight=0なら遅延の目的項は0になる
    }

    for (int k = 0; k < e; ++k) {
        int a, b;
        std::cin >> a >> b;

        p.precedences.push_back({a, b}); // 前工程の完了も開始条件になる
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 全4項の和を小さくする

    // 同じ組み込み式で再検査・再評価
    const long double checked = resource_scheduling_evaluate(p, result.schedule);
    assert(checked == result.objective); // 結果に保存された値と照合する

    print_answer(p, result); // 各開始時刻と目的値を出力する
}
```

**入力例**

```text
3 1 2 1 0.5
       3 1 0 4 2 3 1
       2 2 1 3 0 5 2
       2 1 0 -1 1 0 0
       0 2
```

**出力例（標準出力）**

```text
31.5
3
0 0 0 3
1 0 3 5
2 0 5 7
```

### 3.8 複数プロジェクトの完了時刻を評価する

共有チームで複数案件を進める。途中工程を早めること自体より、「案件全体がいつ終わるか」を重要度や納期に応じて評価したい。

**具体的に解く問題**

各案件の全工程の後ろに、時間0・資源使用なしの完了タスクを置く。この完了タスクだけに重みと納期を付け、案件ごとの完了時刻・遅延の重み付き和を小さくする。

**入力**

`J B`。各案件に`release due completion_weight tardiness_weight K E`、K行の`duration workers`、E行の案件内`before after`。Kは0以上。案件内IDは0〜K-1。空案件の完了時刻はreleaseとする。

**出力**

目的値、案件数J、J行の`project finish`、実工程総数、各実工程の`project operation start finish`。

**利用上の注意**

工程ごとにも同じ完了重みを付けると、「案件の完了」以外の項まで加わるため、この例では工程の重みを既定値0に保つ。

**コード：`example08_project_completion.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各案件の全工程の後ろに、時間0・資源使用なしの完了タスクを置く。
// この完了タスクだけに重みと納期を付け、案件ごとの完了時刻・遅延の重み付き和を小さくする。

// 入力: J B。各案件にrelease due completion_weight tardiness_weight K E、K行のduration workers、
// E行の案件内before after。Kは0以上。案件内IDは0〜K-1。空案件の完了時刻はreleaseとする。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 目的値、案件数J、J行のproject finish、実工程総数、
// 各実工程のproject operation start finish。

// 注意: 工程ごとにも同じ完了重みを付けると、「案件の完了」以外の項まで加わるため、
// この例では工程の重みを既定値0に保つ。

int main() {
    int projects, capacity;
    if (!(std::cin >> projects >> capacity)) {
        return 0;
    }

    resource_scheduling_problem p; // 全案件で作業員を共有する
    p.resources = {{capacity}}; // 案件ごとに資源を分けない
    p.makespan_weight = 0; // 全案件の最後の終了時刻の項は使わない
    p.mode_cost_weight = 0; // 案件の完了・遅延だけを評価する

    std::vector<int> end_id;
    std::vector<std::tuple<int, int, int>> operations;

    for (int j = 0; j < projects; ++j) {
        int release, due, count, edges;
        long double completion, tardiness;
        std::cin >> release >> due >> completion >> tardiness >> count >> edges;

        const int base = static_cast<int>(p.tasks.size()); // この案件の全体タスクIDの先頭

        for (int k = 0; k < count; ++k) {
            int duration, workers;
            std::cin >> duration >> workers;

            scheduling_task task; // 実際の工程には完了・遅延重みを付けない
            task.release_time = release; // 案件の着手可能時刻を全工程に適用する
            task.modes = {{duration, {{0, workers}}, 0}}; // 同じ資源0を使用する
            p.tasks.push_back(task); // 全体のタスク一覧に加える
            operations.push_back({j, k, base + k});
        }

        for (int k = 0; k < edges; ++k) {
            int a, b;
            std::cin >> a >> b;

            p.precedences.push_back({base + a, base + b}); // 案件内IDを全体IDへ変換する
        }

        const int terminal = static_cast<int>(p.tasks.size()); // 案件の完了を表す仮想タスクID

        scheduling_task finish; // このタスクにだけ案件の評価重みを付ける
        finish.release_time = release; // 空案件でもreleaseより前には完了しない
        finish.due_time = due; // 案件全体の納期、-1は納期なし
        finish.completion_weight = completion; // 案件全体の完了時刻に掛ける重み
        finish.tardiness_weight = tardiness; // 案件全体の遅れに掛ける重み
        finish.modes = {{0, {}, 0}}; // 時間0・資源なしの到達点
        p.tasks.push_back(finish); // 完了タスクを通常のタスクとして追加する

        for (int k = 0; k < count; ++k) {
            p.precedences.push_back({base + k, terminal}); // 全工程が完了して初めて到達できる
        }

        end_id.push_back(terminal);
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 案件の完了・遅延を評価する
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 仮想タスクも含めて検査する

    std::cout << result.objective << '\n' << projects << '\n'; // 目的値と案件数

    for (int j = 0; j < projects; ++j) {
        // 時間0なので開始=完了
        std::cout << j << ' ' << result.schedule.start_time[end_id[j]] << '\n';
    }

    std::cout << operations.size() << '\n';

    for (const auto& [j, k, id] : operations) {
        const long long s = result.schedule.start_time[id]; // 元工程に対応する時刻を読む

        std::cout << j << ' ' << k << ' ' << s << ' '
                  << s + p.tasks[id].modes[0].duration << '\n'; // project operation start finish
    }
}
```

**入力例**

```text
2 2
       0 5 2 3 2 1
       3 1
       2 1
       0 1
       1 4 1 4 1 0
       4 2
```

**出力例（標準出力）**

```text
39
2
0 5
1 9
3
0 0 0 3
0 1 3 5
1 0 5 9
```

### 3.9 配送・後処理の終了まで含む独自評価

工場から出荷した後に配送が続く。工場の最後の作業を早く終えるだけでは、配送が長い荷物の到着が遅くなることがある。

**具体的に解く問題**

各タスクの工場内処理が終わってからtail時間の配送があるとし、最も遅い到着時刻とモード費用の和を小さくする。配送同士の資源競合はないとする。

**入力**

`N B alpha`、N行の`duration workers tail cost`。tailは0以上のint、costは有限、alphaは有限かつ非負。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**利用上の注意**

tail配列は全呼出しが終わるまで生存させる。配送車の取り合いも計画したい場合は、配送そのものを別タスク・資源として表す必要がある。

**コード：`example09_delivery_tail.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各タスクの工場内処理が終わってからtail時間の配送があるとし、
// 最も遅い到着時刻とモード費用の和を小さくする。配送同士の資源競合はないとする。

// 入力: N B alpha、N行のduration workers tail cost。tailは0以上のint、costは有限、
// alphaは有限かつ非負。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 注意: tail配列は全呼出しが終わるまで生存させる。配送車の取り合いも計画したい場合は、
// 配送そのものを別タスク・資源として表す必要がある。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, capacity;
    long double alpha;
    if (!(std::cin >> n >> capacity >> alpha)) {
        return 0;
    }

    resource_scheduling_problem p; // 工場内のタスクと資源を登録する
    p.resources = {{capacity}}; // 配送中にはこの資源を使わない
    p.tasks.resize(n); // 各出荷の工場内作業を1タスクとする
    p.mode_cost_weight = alpha; // 独自評価でも入力重みの妥当性は検証される

    std::vector<int> tail(n);

    for (int i = 0; i < n; ++i) {
        int duration, workers;
        long double cost;
        std::cin >> duration >> workers >> tail[i] >> cost;

        p.tasks[i].modes = {{duration, {{0, workers}}, cost}}; // 工場内の時間・人員・費用
    }

    const auto evaluator = [&tail, alpha](
        const resource_scheduling_problem& problem, // 解く問題を受け取る
        const resource_schedule& schedule // 候補の解を受け取る
    ) {
        long long last_arrival = 0;
        long double cost = 0;

        for (int i = 0; i < static_cast<int>(problem.tasks.size()); ++i) { // 全タスクを採点する
            const int m = schedule.mode_of_task[i]; // 候補ごとの選択モードを使う
            const auto& mode = problem.tasks[i].modes[m]; // 選択モードの処理時間・費用
            const long long finish = schedule.start_time[i] + mode.duration; // 工場内の完了時刻
            last_arrival = std::max(last_arrival, finish + tail[i]);
            cost += mode.fixed_cost; // 組み込みの費用は自動加算されないので明示的に加える
        }

        // 余分な待機だけでは値が良くならない評価を使い、毎回同じ解に同じ値を返す
        return static_cast<long double>(last_arrival) + alpha * cost;
    };

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    // 独自の到着時刻で最小化する
    const auto first = resource_scheduling_solve(p, evaluator, options);

    options.seed = 2; // 別の乱数列で元解から追加改善する

    // 同じ独自評価で改善
    const auto result = resource_scheduling_improve(
        p, first.schedule, evaluator, options
    );

    // 独自評価で再検査
    const long double checked = resource_scheduling_evaluate(
        p, result.schedule, evaluator
    );
    // 同じ評価で元解以下を確認
    assert(checked == result.objective && result.objective <= first.objective);

    print_answer(p, result); // finish列は工場内の完了、到着はこれにtailを加えた時刻
}
```

**入力例**

```text
3 1 0.5
       3 1 7 2
       2 1 1 0
       4 1 0 1
```

**出力例（標準出力）**

```text
11.5
3
0 0 0 3
1 0 3 5
2 0 5 9
```

### 3.10 外部で作った計画を検査して改善する

手作業、別のソルバー、前の計算などで得た実行可能な計画を、出発点として使う。開始時刻に空き時間があっても入力できる。

**具体的に解く問題**

先行制約のない仕事について、外部の優先順列と開始時刻を入力し、検査してから全体終了時刻を改善する。

**入力**

`N B`、N行の`duration workers`、N個の`task_order`、N個のタスクID順`start_time`。モードは全タスク0。順列は0〜N-1を1回ずつ含む。

**出力**

有効な元解なら元の目的値を1行出し、その後に共通形式の改善結果。解が不正なら`INVALID_SCHEDULE`の1行。

**利用上の注意**

max_iterations=0でも入力順序・モードを配置し直した解との比較が入るので、元の時刻がそのまま返るとは限らない。

**コード：`example10_external_schedule.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 先行制約のない仕事について、外部の優先順列と開始時刻を入力し、
// 検査してから全体終了時刻を改善する。

// 入力: N B、N行のduration workers、N個のtask_order、N個のタスクID順start_time。モードは全タスク0。
// 順列は0〜N-1を1回ずつ含む。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 有効な元解なら元の目的値を1行出し、その後に共通形式の改善結果。
// 解が不正ならINVALID_SCHEDULEの1行。

// 注意: max_iterations=0でも入力順序・モードを配置し直した解との比較が入るので、
// 元の時刻がそのまま返るとは限らない。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, capacity;
    if (!(std::cin >> n >> capacity)) {
        return 0;
    }

    resource_scheduling_problem p; // 外部解と同じタスクID・資源IDで問題を作る
    p.resources = {{capacity}}; // 作業員容量
    p.tasks.resize(n); // タスクIDは入力順
    p.mode_cost_weight = 0; // 全体終了時刻だけを評価する

    for (int i = 0; i < n; ++i) {
        int duration, workers;
        std::cin >> duration >> workers;

        p.tasks[i].modes = {{duration, {{0, workers}}, 0}}; // 全タスク1モード
    }

    resource_schedule initial; // 外部解を入れる構造体
    initial.task_order.resize(n); // 優先順列の長さは全タスク数
    initial.mode_of_task.assign(n, 0); // モードIDは全タスク0
    initial.start_time.resize(n); // 時刻はタスクIDの順に入れる

    for (int& id : initial.task_order) {
        std::cin >> id; // 順序位置ごとのタスクIDを読む
    }

    for (long long& time : initial.start_time) {
        std::cin >> time; // long longで開始時刻を読む
    }

    if (!resource_scheduling_is_feasible(p, initial)) { // 問題自体が不正ならfalseではなく例外になる
        std::cout << "INVALID_SCHEDULE\n";
        return 0; // improveは実行不能解を修復する関数ではない
    }

    // 元解を組み込み式で評価する
    const long double before = resource_scheduling_evaluate(p, initial);

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    // 順序・モード・時刻は固定されない
    const auto result = resource_scheduling_improve(p, initial, options);
    assert(result.objective <= before); // 同じ問題・評価で元解より悪い値にはならない

    std::cout << before << '\n';

    print_answer(p, result); // 元解の目的値の後に、改善結果を標準形式で出力する
}
```

**入力例**

```text
3 1
       3 1
       2 1
       1 1
       2 0 1
       5 8 0
```

**出力例（標準出力）**

```text
10
6
3
0 0 1 4
1 0 4 6
2 0 0 1
```

### 3.11 次ターンの容量・開始可能時刻を反映して再計画する

毎ターン、使える作業員数や材料の到着予定が変わる。前回の解が新条件でも使えるなら引き継ぎ、使えなければ新しく作り直す。

**具体的に解く問題**

同じタスク集合について、容量と開始可能時刻を更新する。前回解を変更後の問題で検査し、improveまたはsolveを選ぶ。まだどの仕事も開始していない再計画を想定する。

**入力**

`N E old_capacity new_capacity`、N行の`duration workers old_release new_release`、E行の`before after`。各workersは新旧両方の容量以下にする。

**出力**

1行目は前回解を渡せたら1、作り直したら0。その後に共通形式の新しい結果。

**利用上の注意**

実行済み・実行中のタスクを固定する機能ではない。実行済み作業がある場合は次の例のように残り問題を組み立てる。

**コード：`example11_replan.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 同じタスク集合について、容量と開始可能時刻を更新する。前回解を変更後の問題で検査し、
// improveまたはsolveを選ぶ。まだどの仕事も開始していない再計画を想定する。

// 入力: N E old_capacity new_capacity、N行のduration workers old_release new_release、
// E行のbefore after。各workersは新旧両方の容量以下にする。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 1行目は前回解を渡せたら1、作り直したら0。その後に共通形式の新しい結果。

// 注意: 実行済み・実行中のタスクを固定する機能ではない。
// 実行済み作業がある場合は次の例のように残り問題を組み立てる。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, e, old_capacity, new_capacity;
    if (!(std::cin >> n >> e >> old_capacity >> new_capacity)) {
        return 0;
    }

    resource_scheduling_problem p; // タスクIDを維持する問題
    p.resources = {{old_capacity}}; // まず前ターンの容量
    p.tasks.resize(n); // タスク数はこの例では変えない
    p.mode_cost_weight = 0; // 全体終了時刻を最小化する

    std::vector<int> next_release(n);

    for (int i = 0; i < n; ++i) {
        int duration, workers, old_release;
        std::cin >> duration >> workers >> old_release >> next_release[i];

        p.tasks[i].release_time = old_release; // 前ターンの開始可能時刻
        p.tasks[i].modes = {{duration, {{0, workers}}, 0}}; // 新容量にも収まるモードだけを指定する
    }

    for (int k = 0; k < e; ++k) {
        int a, b;
        std::cin >> a >> b;

        p.precedences.push_back({a, b}); // 先行制約も新旧で共通にする
    }

    resource_scheduling_options initial_options; // 前回解を作るための設定
    initial_options.max_iterations = 0; // 初期構築だけを実行する

    const auto previous = resource_scheduling_solve(p, initial_options); // 前ターンの解を保存する

    p.resources[0].capacity = new_capacity; // 次ターンの容量に更新する

    for (int i = 0; i < n; ++i) {
        p.tasks[i].release_time = next_release[i]; // 到着予定も更新する
    }

    resource_scheduling_validate_problem(p); // 未選択モードも含め新条件が有効か検査する

    // 必ず変更後の問題で判定する
    const bool reused = resource_scheduling_is_feasible(p, previous.schedule);

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    resource_scheduling_result result; // 分岐後の結果を受け取る

    if (reused) {
        // 有効なら前回解を利用する
        result = resource_scheduling_improve(p, previous.schedule, options);
    } else {
        result = resource_scheduling_solve(p, options); // 無効なら新しい実行可能解を作る
    }

    std::cout << static_cast<int>(reused) << '\n';

    print_answer(p, result); // 新条件での目的値と解を出力する
    // 内部探索状態の差分更新ではなく、毎回検証・準備・資源表の構築が行われる
    // previous.objectiveは旧条件の値なので、新結果の比較基準にしない
}
```

**入力例**

```text
2 0 2 1
       3 1 0 0
       3 1 0 0
```

**出力例（標準出力）**

```text
0
6
2
0 0 0 3
1 0 3 6
```

### 3.12 完了済みの仕事を除いた残り工程を解く

途中時点で、完了済みの仕事はもう動かさず、これから着手する仕事だけを計画したい。元のタスク番号と残り問題の番号を対応させる。

**具体的に解く問題**

現在時刻nowで実行中の仕事が一つもないとする。未着手の仕事だけを取り出し、開始時刻をnow以上にして解き、元のタスクIDへ戻す。完了済み仕事はこの先の資源を占有しない。

**入力**

`N E B now`、N行の`duration workers release completed_at`、E行の`before after`。completed_at=-1は未着手、それ以外は0〜nowの実際の完了時刻。完了記録は元問題の制約を満たすものとする。

**出力**

残り仕事だけの目的値、残り件数、各行に`original_task mode start finish`。残りなしなら0と0の2行。

**利用上の注意**

完了済み部分の全履歴は再検証していない。入力の完了記録が既に正しいこと、now時点に実行中の仕事がないことが前提。

**コード：`example12_remaining_tasks.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 現在時刻nowで実行中の仕事が一つもないとする。未着手の仕事だけを取り出し、
// 開始時刻をnow以上にして解き、元のタスクIDへ戻す。完了済み仕事はこの先の資源を占有しない。

// 入力: N E B now、N行のduration workers release completed_at、E行のbefore after。
// completed_at=-1は未着手、それ以外は0〜nowの実際の完了時刻。
// 完了記録は元問題の制約を満たすものとする。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 残り仕事だけの目的値、残り件数、各行にoriginal_task mode start finish。
// 残りなしなら0と0の2行。

// 注意: 完了済み部分の全履歴は再検証していない。入力の完了記録が既に正しいこと、
// now時点に実行中の仕事がないことが前提。

int main() {
    int n, e, capacity, now;
    if (!(std::cin >> n >> e >> capacity >> now)) {
        return 0;
    }

    resource_scheduling_problem whole; // 元のタスクIDを保った問題を読む
    whole.resources = {{capacity}}; // 完了済み仕事はnow以降この容量を使わない
    whole.tasks.resize(n); // 元の全タスクを登録する
    whole.mode_cost_weight = 0; // 目的は残り仕事の最も遅い完了時刻

    std::vector<int> completed_at(n);

    for (int i = 0; i < n; ++i) {
        int duration, workers, release;
        std::cin >> duration >> workers >> release >> completed_at[i];

        whole.tasks[i].release_time = release; // 元の開始可能時刻
        whole.tasks[i].modes = {{duration, {{0, workers}}, 0}}; // 元の作業内容

        if (completed_at[i] < -1 || completed_at[i] > now) {
            throw std::invalid_argument("完了記録が不正");
        }
    }

    for (int k = 0; k < e; ++k) {
        int a, b;
        std::cin >> a >> b;

        whole.precedences.push_back({a, b}); // 元IDで先行関係を保持する
    }

    resource_scheduling_validate_problem(whole); // DAGやID範囲を先に検証する

    resource_scheduling_problem p; // 残り仕事だけの小さい問題を作る
    p.resources = whole.resources; // 今後使える容量を引き継ぐ
    p.mode_cost_weight = 0; // 終了時刻だけを評価する

    std::vector<int> global_to_local(n, -1), local_to_global;

    for (int i = 0; i < n; ++i) {
        if (completed_at[i] >= 0) {
            continue;
        }

        // 未着手だけに連続したローカルIDを付ける
        global_to_local[i] = static_cast<int>(p.tasks.size());
        local_to_global.push_back(i);
        p.tasks.push_back(whole.tasks[i]); // 元のモード等をコピーする
        // 過去への配置を防ぐ
        p.tasks.back().release_time = std::max(now, whole.tasks[i].release_time);
    }

    for (const auto& edge : whole.precedences) { // 元の先行辺をローカルIDへ変換する
        const int a = global_to_local[edge.before]; // 前工程の新ID、完了済みなら-1
        const int b = global_to_local[edge.after]; // 後工程の新ID、完了済みなら-1

        if (a >= 0 && b < 0) {
            throw std::invalid_argument("未完了の先行工程が残っている");
        }

        if (a >= 0 && b >= 0) {
            p.precedences.push_back({a, b}); // 両方残る辺だけを登録する
        }
        // 完了済み→未着手の辺は、完了時刻<=nowかつ新開始時刻>=nowによって満たされる
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 残り工程の実行可能解を作る
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 残り問題の範囲で検査する

    std::cout << result.objective << '\n' << local_to_global.size() << '\n'; // 残りなしなら目的値0

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 返却配列をタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // モード添字は元タスクと共通
        const long long s = result.schedule.start_time[i]; // 絶対時刻として返される

        std::cout << local_to_global[i] << ' ' << m << ' ' << s << ' '
                  << s + p.tasks[i].modes[m].duration << '\n'; // 元IDで残り仕事を出力する
    }
    // 実行中の仕事や外部予約がある場合、この例をそのまま適用すると容量を過大評価する
}
```

**入力例**

```text
4 3 2 4
       3 1 0 3
       2 1 0 2
       4 2 0 -1
       2 1 8 -1
       0 2
       1 2
       2 3
```

**出力例（標準出力）**

```text
10
2
2 0 4 8
3 0 8 10
```

### 3.13 共通の終了期限で複数のseedを試す

一度の探索だけでなく、異なる乱数seedから複数回解き、良い結果を残す。全試行に共通する時計の期限を渡し、計算時間を配分する。

**具体的に解く問題**

一つの作業員資源を共有する独立した仕事に対し、最大trials回の新規探索を行う。各回は最大20ms、全体は入力読込み開始前からbudget_msで切り上げる目標とする。

**入力**

`N B trials budget_ms`、N行の`duration workers`。trialsは1以上、budget_msは0〜10000の整数。この例の時間枠であり、ライブラリの固定上限ではない。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**利用上の注意**

タスクの納期due_timeと、CPU上の終了期限deadlineは別物。期限を過ぎてから呼んでも問題検証・初期構築は行われる。

**コード：`example13_shared_deadline.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 一つの作業員資源を共有する独立した仕事に対し、最大trials回の新規探索を行う。
// 各回は最大20ms、全体は入力読込み開始前からbudget_msで切り上げる目標とする。

// 入力: N B trials budget_ms、N行のduration workers。trialsは1以上、budget_msは0〜10000の整数。
// この例の時間枠であり、ライブラリの固定上限ではない。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 注意: タスクの納期due_timeと、CPU上の終了期限deadlineは別物。
// 期限を過ぎてから呼んでも問題検証・初期構築は行われる。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    const auto begin = std::chrono::steady_clock::now(); // 入力読込みも含めた共通起点
    int n, capacity, trials, budget_ms;
    if (!(std::cin >> n >> capacity >> trials >> budget_ms)) {
        return 0;
    }

    resource_scheduling_problem p; // 全試行で共通の問題
    p.resources = {{capacity}}; // 共有作業員の容量
    p.tasks.resize(n); // 入力順をタスクIDとする
    p.mode_cost_weight = 0; // 最後の終了時刻を評価する

    for (int i = 0; i < n; ++i) {
        int duration, workers;
        std::cin >> duration >> workers;

        p.tasks[i].modes = {{duration, {{0, workers}}, 0}}; // 必ず実行する一つの方法
    }

    resource_scheduling_options options; // 時間の設定を全試行で共有する
    options.deadline = begin + std::chrono::milliseconds(budget_ms); // steady_clockの絶対期限
    options.max_iterations = 0; // 期限が過ぎていても返す初期解をまず用意する

    auto best = resource_scheduling_solve(p, options); // 初期構築・検査の時間はゼロにはならない

    options.max_iterations = -1; // 以降は反復数上限を使わず、時間で止める
    options.time_limit_ms = 20; // 1回あたりの相対改善時間

    for (int attempt = 0; attempt < trials; ++attempt) {
        if (std::chrono::steady_clock::now() >= *options.deadline) {
            break; // 呼出し前にも残り時間を見る
        }

        options.seed = static_cast<std::uint64_t>(attempt) + 1; // 試行ごとにseedを変える

        auto candidate = resource_scheduling_solve(p, options); // 共通deadlineは延長しない

        if (candidate.objective < best.objective) {
            best = std::move(candidate); // 最小の目的値を保存する
        }
    }

    print_answer(p, best); // 採用した1試行の結果を返す
    // ライブラリは厳密な時間上限を保証しないので、本番では出力等の余裕を残す
    // bestのカウンタはその試行だけの値であり、全試行の合計ではない
}
```

**入力例**

```text
5 3 4 60
       6 2
       4 1
       5 2
       2 1
       3 1
```

**出力例（標準出力）**

```text
11
5
0 0 0 6
1 0 2 6
2 0 6 11
3 0 0 2
4 0 6 9
```

### 3.14 反復数を決めて探索し、短いデバッグ表示で確認する

同じ入力で設定を比べたり、タスク数が多いときに結果を簡潔に確認したい。目的値と探索カウンタ、先頭部分の配列を一度に出す。

**具体的に解く問題**

時間1のN個の独立した仕事をB人で処理する。提案回数をiterations以下にし、結果構造体と解構造体をそのまま表示する。

**入力**

`N B iterations seed`。Nは0以上、Bは正、iterationsは0以上、seedはuint64_tの範囲。掲載入力は表示を確定させるためiterations=0。正の値で探索回数の比較ができる。

**出力**

1行目はresource_scheduling_result、2行目はresource_scheduleのデバッグ文字列。全25要素の保存形式ではない。

**コード：`example14_iteration_debug.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 時間1のN個の独立した仕事をB人で処理する。提案回数をiterations以下にし、
// 結果構造体と解構造体をそのまま表示する。

// 入力: N B iterations seed。Nは0以上、Bは正、iterationsは0以上、seedはuint64_tの範囲。
// 掲載入力は表示を確定させるためiterations=0。正の値で探索回数の比較ができる。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 1行目はresource_scheduling_result、2行目はresource_scheduleのデバッグ文字列。
// 全25要素の保存形式ではない。

int main() {
    int n, capacity;
    long long iterations;
    std::uint64_t seed;
    if (!(std::cin >> n >> capacity >> iterations >> seed)) {
        return 0;
    }

    resource_scheduling_problem p; // 同じ長さの独立した仕事を作る
    p.resources = {{capacity}}; // 1仕事が1人を使う
    p.tasks.resize(n); // 0件も有効な空問題になる
    p.mode_cost_weight = 0; // 全体終了時刻だけを評価する

    for (auto& task : p.tasks) {
        task.modes = {{1, {{0, 1}}, 0}}; // 全仕事を時間1・人員1にする
    }

    resource_scheduling_options options; // 反復数とseedを指定する
    options.time_limit_ms = 60000; // 小さい例で反復上限に達するための余裕、無限大は指定しない
    options.max_iterations = iterations; // 0は初期構築だけ、正なら追加提案の上限
    options.seed = seed; // 乱数列を指定する
    options.deadline.reset(); // この例では絶対期限を設けない

    const auto result = resource_scheduling_solve(p, options); // 時間条件も残る点に注意する
    assert(resource_scheduling_is_feasible(p, result.schedule)); // デバッグ表示自体は検査をしない
    assert(result.iterations <= iterations); // 無効提案も反復数に含まれる

    std::cout << result << '\n'; // 目的値・3カウンタ・解を表示、各配列は先頭20要素まで
    std::cout << result.schedule << '\n'; // 解だけの表示、残りは「... (件数 more)」になる
    // 本番の標準出力を汚したくなければ、この2行をstd::cerrに変える
    // 固定反復でも冷却に経過時間を使うため、環境をまたぐ完全再現性は保証されない
}
```

**入力例**

```text
25 25 0 42
```

**出力例（標準出力）**

```text
resource_scheduling_result{objective=1, iterations=0, decoded_schedules=1, accepted_moves=0, schedule=resource_schedule{task_order=[11, 22, 23, 17, 24, 8, 3, 9, 16, 12, 14, 10, 5, 19, 15, 6, 1, 4, 0, 13, ... (5 more)], mode_of_task=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (5 more)], start_time=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (5 more)]}}
resource_schedule{task_order=[11, 22, 23, 17, 24, 8, 3, 9, 16, 12, 14, 10, 5, 19, 15, 6, 1, 4, 0, 13, ... (5 more)], mode_of_task=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (5 more)], start_time=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (5 more)]}
```

### 3.15 人手・機械・冷却で資源使用が変わる工程

準備は人、加工は機械、冷却は何も使わず、最後の梱包は再び人、という工程を考える。全時間を一つのタスクにすると、不要な時間まで人や機械を占有してしまう。

**具体的に解く問題**

各仕事を準備・加工・冷却・梱包の4段階に分け、順につなぐ。冷却時間を必ず経過させながら、共有の人員・機械の容量を守って全体終了時刻を小さくする。

**入力**

`J WORKERS MACHINES`、J行の`release prepare process cooling pack`。時間は0以上、各容量は正。準備・梱包は人1、加工は機械1、冷却は資源なし。

**出力**

全体終了時刻、段階総数4J、各行に`job phase start finish`。phaseは0=準備、1=加工、2=冷却、3=梱包。

**コード：`example15_phased_jobs.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各仕事を準備・加工・冷却・梱包の4段階に分け、順につなぐ。冷却時間を必ず経過させながら、
// 共有の人員・機械の容量を守って全体終了時刻を小さくする。

// 入力: J WORKERS MACHINES、J行のrelease prepare process cooling pack。時間は0以上、各容量は正。
// 準備・梱包は人1、加工は機械1、冷却は資源なし。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 出力: 全体終了時刻、段階総数4J、各行にjob phase start finish。phaseは0=準備、1=加工、2=冷却、
// 3=梱包。

int main() {
    int jobs, workers, machines;
    if (!(std::cin >> jobs >> workers >> machines)) {
        return 0;
    }

    resource_scheduling_problem p; // 段階ごとに資源使用を固定したタスクへ分ける
    p.resources = {{workers}, {machines}}; // 資源0=人、資源1=機械
    p.mode_cost_weight = 0; // 全体終了時刻を小さくする

    std::vector<std::array<int, 4>> ids(jobs);

    for (int j = 0; j < jobs; ++j) {
        int release;
        std::array<int, 4> duration{};
        std::cin >> release >> duration[0] >> duration[1] >> duration[2] >> duration[3];

        for (int phase = 0; phase < 4; ++phase) {
            const int id = static_cast<int>(p.tasks.size()); // 段階を全体タスクIDに変換する
            ids[j][phase] = id;

            scheduling_task task; // この段階のタスク
            task.release_time = release; // 仕事の投入前にはどの段階も始めない

            scheduling_mode mode; // 段階の実行方法は一つ
            mode.duration = duration[phase]; // 段階の処理時間、0も指定可能

            if (phase == 0 || phase == 3) {
                mode.renewable_uses = {{0, 1}}; // 準備と梱包は人1
            }

            if (phase == 1) {
                mode.renewable_uses = {{1, 1}}; // 加工は機械1
            }

            // phase=2の冷却ではrenewable_usesを空にし、資源を占有しない
            task.modes.push_back(mode); // 段階にモードを登録する
            p.tasks.push_back(task); // 全体問題へ追加する

            if (phase > 0) {
                p.precedences.push_back({ids[j][phase - 1], id}); // 準備→加工→冷却→梱包
            }
        }
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    const auto result = resource_scheduling_solve(p, options); // 段階の順と資源容量を守る
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 段階の間に資源を返すモデルを検査

    std::cout << result.objective << '\n' << jobs * 4 << '\n'; // 目的値と段階総数

    for (int j = 0; j < jobs; ++j) {
        for (int phase = 0; phase < 4; ++phase) {
            const int id = ids[j][phase];
            const long long s = result.schedule.start_time[id]; // 段階の開始時刻

            std::cout << j << ' ' << phase << ' ' << s << ' '
                      << s + p.tasks[id].modes[0].duration << '\n'; // job phase start finish
        }
    }
    // 先行辺は「直後に開始」ではないので、段階間の追加待機は発生し得る
    // 冷却中も機械を保持する工程なら、この例の空の使用一覧では表現できない
}
```

**入力例**

```text
2 1 1
       0 2 4 3 1
       0 2 3 1 2
```

**出力例（標準出力）**

```text
12
8
0 0 0 2
0 1 2 6
0 2 6 9
0 3 9 10
1 0 2 4
1 1 6 9
1 2 9 10
1 3 10 12
```

### 3.16 入力問題と外部スケジュールを分けて検査する

外部ツールが出した計画や自分で加工した計画を、探索を走らせずにチェックする。問題定義が不正なのか、解が制約に違反しているのかを分けて扱う。

**具体的に解く問題**

問題、優先順列、モード配列、開始時刻配列を入力し、実行可能なら組み込み目的値を出す。この例では各タスク1モードなので有効なモードIDは0のみ。

**入力**

`N E B`、N行の`duration workers`、E行の`before after`、N個の`task_order`、N個のタスクID順`mode_of_task`、N個のタスクID順`start_time`。件数と入力構文は正しいものとし、値の不正はプログラムで検査する。

**出力**

`VALID value`、`INVALID_PROBLEM`、`INVALID_SCHEDULE`、`INVALID_VALUE`のいずれか。診断の詳細と解の表示は標準エラー出力。

**利用上の注意**

配列長の不一致もライブラリは検出するが、この入力形式では3配列を同じN個だけ読む。NやEが負など、入力構文自体の検査はこの例の対象外。

**コード：`example16_validate_schedule.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 問題、優先順列、モード配列、開始時刻配列を入力し、実行可能なら組み込み目的値を出す。
// この例では各タスク1モードなので有効なモードIDは0のみ。

// 入力: N E B、N行のduration workers、E行のbefore after、N個のtask_order、
// N個のタスクID順mode_of_task、N個のタスクID順start_time。件数と入力構文は正しいものとし、
// 値の不正はプログラムで検査する。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。

// 出力: VALID value、INVALID_PROBLEM、INVALID_SCHEDULE、INVALID_VALUEのいずれか。
// 診断の詳細と解の表示は標準エラー出力。

// 注意: 配列長の不一致もライブラリは検出するが、この入力形式では3配列を同じN個だけ読む。
// NやEが負など、入力構文自体の検査はこの例の対象外。

int main() {
    int n, e, capacity;
    if (!(std::cin >> n >> e >> capacity)) {
        return 0;
    }

    resource_scheduling_problem p; // 検査する問題
    p.resources = {{capacity}}; // 不正な容量ならvalidate_problemが例外を送出する
    p.tasks.resize(n); // 入力の件数は0以上を前提にする
    p.mode_cost_weight = 0; // 評価値は全体終了時刻

    for (int i = 0; i < n; ++i) {
        int duration, workers;
        std::cin >> duration >> workers;

        p.tasks[i].modes = {{duration, {{0, workers}}, 0}}; // 各タスクにモード0だけを登録する
    }

    for (int k = 0; k < e; ++k) {
        int a, b;
        std::cin >> a >> b;

        p.precedences.push_back({a, b}); // 範囲外IDや閉路は後で検査する
    }

    resource_schedule schedule; // 検査する解、3配列は別々の意味を持つ
    schedule.task_order.resize(n); // 優先順序の位置からタスクIDを引く配列
    schedule.mode_of_task.resize(n); // タスクIDからモードIDを引く配列
    schedule.start_time.resize(n); // タスクIDから開始時刻を引く配列

    for (int& x : schedule.task_order) {
        std::cin >> x; // 順列を読む
    }

    for (int& x : schedule.mode_of_task) {
        std::cin >> x; // モードを読む
    }

    for (long long& x : schedule.start_time) {
        std::cin >> x; // 開始時刻を読む
    }

    try {
        resource_scheduling_validate_problem(p); // 問題だけの不正を先に切り分ける
    } catch (const std::invalid_argument& error) {
        std::cout << "INVALID_PROBLEM\n";
        std::cerr << error.what() << '\n';
        return 0;
    }

    if (!resource_scheduling_is_feasible(p, schedule)) { // 問題が有効なら解の不正はfalseで返る
        std::cout << "INVALID_SCHEDULE\n";
        return 0;
    }

    try {
        // 検証と目的値の有限性検査も行う
        const long double value = resource_scheduling_evaluate(p, schedule);

        std::cout << "VALID " << value << '\n';
        std::cerr << schedule << '\n'; // 検査済み解を20要素上限の形式で確認する
    } catch (const std::invalid_argument& error) {
        std::cout << "INVALID_VALUE\n";
        std::cerr << error.what() << '\n';
    }
    // validate_problemの結果を保持する仕組みはなく、後続の公開関数でも問題を再検証する
}
```

**入力例**

```text
2 1 1
       2 1
       3 1
       0 1
       0 1
       0 0
       0 2
```

**出力例（標準出力）**

```text
VALID 5
```

### 3.17 資源を使わない直列処理と大きな時刻

人員や機械を取り合わず、処理の前後関係と投入時刻だけを考えたい場合に使う。工程を多数つなぐと、各時間がintに収まっていても完了時刻はintを超える。

**具体的に解く問題**

N工程を0→1→…の順に実行する。資源は登録せず、各工程の処理時間・開始可能時刻を与える。開始時刻と完了時刻をlong longで受け取る。

**入力**

`N`、N行の`duration release`。Nは0以上、各値は0〜INT_MAX。この例の入力では終了時刻が40億になる。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**利用上の注意**

資源競合がなくモードも固定なら、前後関係から開始時刻を直接計算する厳密な方法でも解ける。本例は空資源・時間0・既定引数・64bit時刻の利用方法を示す。

**コード：`example17_resource_free_chain.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: N工程を0→1→…の順に実行する。資源は登録せず、各工程の処理時間・開始可能時刻を与える。
// 開始時刻と完了時刻をlong longで受け取る。

// 入力: N、N行のduration release。Nは0以上、各値は0〜INT_MAX。
// この例の入力では終了時刻が40億になる。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

// 注意: 資源競合がなくモードも固定なら、前後関係から開始時刻を直接計算する厳密な方法でも解ける。
// 本例は空資源・時間0・既定引数・64bit時刻の利用方法を示す。

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    resource_scheduling_problem p; // resourcesは既定値の空のままにする
    p.tasks.resize(n); // 全工程を入力順で登録する

    for (int i = 0; i < n; ++i) {
        int duration, release;
        std::cin >> duration >> release;

        p.tasks[i].release_time = release; // この工程の開始可能時刻
        p.tasks[i].modes = {{duration, {}, 0}}; // 資源なし・費用0、時間0も有効

        if (i > 0) {
            p.precedences.push_back({i - 1, i}); // 直前の工程からつなぐ
        }
    }

    const auto first = resource_scheduling_solve(p); // optionsを省略して既定設定で呼ぶ

    // improveもoptionsを省略できる
    const auto result = resource_scheduling_improve(p, first.schedule);

    print_answer(p, result); // 完了時刻はlong longで計算する共通出力を使う
    // この問題は順序・モードが一意なので、既定1950msを使い切るための探索は行われない
    // n=0なら空問題として目的値0、タスク数0が返る
}
```

**入力例**

```text
3
       2000000000 0
       2000000000 0
       0 0
```

**出力例（標準出力）**

```text
4000000000
3
0 0 0 2000000000
1 0 2000000000 4000000000
2 0 4000000000 4000000000
```

### 3.18 最大の納期遅れを抑え、評価器を参照で渡す

合計遅延が小さくても、1件だけ極端に遅れるのは避けたい。全注文のうち最も大きい遅れを評価し、外部の評価器で呼出し回数も記録する。

**具体的に解く問題**

各仕事の時間・人員・納期を入力し、最大の納期超過時間を小さくする。納期前の完了は遅れ0、due=-1の仕事は遅延評価の対象外とする。

**入力**

`N B`、N行の`duration workers due`。dueは-1または0以上。全仕事が納期なしなら目的値0。

**出力**

共通形式：目的値、タスク数、各タスクの`task mode start finish`。

**コード：`example18_worst_tardiness.cpp`**

```cpp
#include "generic_resource_scheduler_v09.hpp" // 本体の型・関数を利用する

// 目的: 各仕事の時間・人員・納期を入力し、最大の納期超過時間を小さくする。納期前の完了は遅れ0、
// due=-1の仕事は遅延評価の対象外とする。

// 入力: N B、N行のduration workers due。dueは-1または0以上。全仕事が納期なしなら目的値0。

// 件数は0以上でintに収まる値、IDは0始まり。指定個数の値を入力する。
// 容量は正、時間は0以上、各タスクのモードは1個以上。各モードの使用量は容量以下にする。
// 納期-1は未設定、重みは有限かつ非負、費用は有限。先行辺には閉路を作らない。

struct WorstTardiness {
    long long calls = 0; // これはサンプル側の評価呼出し回数、ライブラリのAPIではない

    long double operator()(
        const resource_scheduling_problem& p, // 候補と対応する問題
        const resource_schedule& s // 評価する候補、書き換えない
    ) {
        ++calls; // 診断だけを更新し、評価値は呼出し回数に依存させない
        long long worst = 0;

        for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 全タスクの遅れを調べる
            const int m = s.mode_of_task[i]; // 候補で選ばれたモード
            const long long c = s.start_time[i] + p.tasks[i].modes[m].duration; // 完了時刻

            if (p.tasks[i].due_time >= 0) { // -1のタスクは納期評価から除外する
                // 初期値0により早着の負値は切り捨てる
                worst = std::max(worst, c - p.tasks[i].due_time);
            }
        }

        return static_cast<long double>(worst); // 遅れの合計ではなく、最大の遅れを最小化する
    }
};

// 出力: 目的値、タスク数、各行に「タスクID モードID 開始時刻 完了時刻」
void print_answer(
    const resource_scheduling_problem& p, // 解いた問題と同じID体系を渡す
    const resource_scheduling_result& result // solve/improveの戻り値を受け取る
) {
    assert(resource_scheduling_is_feasible(p, result.schedule)); // 本モデルの制約を検査する

    // 呼出しに用いた評価値を表示する
    std::cout << std::setprecision(15) << result.objective << '\n';
    std::cout << p.tasks.size() << '\n'; // 出力するタスクの件数を表示する

    for (int i = 0; i < static_cast<int>(p.tasks.size()); ++i) { // 配列はタスクID順に読む
        const int m = result.schedule.mode_of_task[i]; // 選択モードの添字を取得する
        const long long s = result.schedule.start_time[i]; // 開始時刻はlong longで受け取る
        const long long c = s + p.tasks[i].modes[m].duration; // 完了時刻をlong longで計算する

        std::cout << i << ' ' << m << ' ' << s << ' ' << c << '\n';
    }
}

int main() {
    int n, capacity;
    if (!(std::cin >> n >> capacity)) {
        return 0;
    }

    resource_scheduling_problem p; // 資源容量は通常の問題として指定する
    p.resources = {{capacity}}; // 全仕事で共有する人員
    p.tasks.resize(n); // タスクIDは入力順
    p.makespan_weight = 0; // 組み込み全体終了の項を使わないことを設定にも明示する
    p.mode_cost_weight = 0; // 費用の項も使わない

    for (int i = 0; i < n; ++i) {
        int duration, workers, due;
        std::cin >> duration >> workers >> due;

        p.tasks[i].due_time = due; // 独自評価器がこの納期を読む
        p.tasks[i].tardiness_weight = due >= 0 ? 1 : 0; // 初期優先度にも納期の重要性を伝える
        p.tasks[i].modes = {{duration, {{0, workers}}, 0}}; // 全仕事1モード
    }

    resource_scheduling_options options; // 省略時は1950ms、seed=1、反復数上限なし
    options.time_limit_ms = 1000; // この例では初期構築後の改善を最大1000msにする
    options.max_iterations = 2000; // 小さな例なので提案回数も2000回までにする
    options.seed = 1; // 比較しやすいよう乱数seedを固定する

    WorstTardiness evaluator;

    // 評価器本体をコピーせず参照で渡す
    const auto result = resource_scheduling_solve(p, std::ref(evaluator), options);

    // 同じ評価器で検査
    const long double value = resource_scheduling_evaluate(
        p, result.schedule, std::ref(evaluator)
    );
    assert(value == result.objective); // 同じ解なら呼出し回数によらず同じ評価値になる

    std::cerr << "evaluator_calls=" << evaluator.calls << '\n'; // これは再評価の1回も含む診断値

    print_answer(p, result); // objectiveは最大遅延であり、組み込み式の値ではない
    // std::refで渡したevaluatorは、全てのライブラリ呼出しが終わるまで生存させる
    // 独自評価に変えてもdue_timeは必須期限にならず、遅れる結果は許される
}
```

**入力例**

```text
3 1
       3 1 3
       2 1 4
       4 1 7
```

**出力例（標準出力）**

```text
2
3
0 0 0 3
1 0 3 5
2 0 5 9
```

