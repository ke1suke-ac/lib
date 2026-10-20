# ビームサーチ9種類の利用ガイド

このガイドは、次の9ファイルを対象にします。まず方式の違いと共通の使い方を説明し、その後に各ライブラリの実用的な利用パターンを紹介します。コード例は標準入力を使わず、対応するヘッダを用意すれば、そのままコンパイル・実行できます。

例は、割当表を返す、操作列を返す、所要時間の異なる行動を選ぶ、turn内の複数操作をまとめる、といった本番での組み込み方に沿って構成しています。データは処理を追いやすい小規模なものとし、同じ題材を異なるライブラリでも扱うことで、使い方を比較できるようにしています。

| ライブラリファイル | 型 | 状態の保持方法 | 1回の遷移 |
| --- | --- | --- | --- |
| `beam_copy_single_v40.hpp` | `bs::Beam<Cost, Hash>` | Stateを保存 | 必ず次のturnへ |
| `beam_delta_single_v46.hpp` | `bs::Beam<Cost, Hash>` | Actionを保存し、外部状態を更新 | 必ず次のturnへ |
| `beam_copy_multi_v44.hpp` | `bs::Beam<Cost, Hash>` | Stateを保存 | 指定した`step`だけturnを進める |
| `beam_delta_multi_v69.hpp` | `bs::Beam<Cost, Hash>` | Actionと時刻情報を保存し、外部状態を更新 | 指定した`step`だけturnを進める |
| `chokudai_copy_single_v13.hpp` | `cs::ChokudaiCopySingle<Cost, Hash>` | Stateと未展開候補を保存 | 必ず次のturnへ |
| `chokudai_copy_multi_v15.hpp` | `cs::ChokudaiCopyMulti<Cost, Hash>` | Stateと未展開候補を保存 | 指定した`step`だけturnを進める |
| `beam_copy_sub_v12.hpp` | `bs::Beam<Cost, Hash>` | Stateを保存 | 同turnの次sub層、または次turnへ |
| `beam_delta_sub_v12.hpp` | `bs::Beam<Cost, Hash>` | Actionを保存し、外部状態を更新 | 同turnの次sub層、または次turnへ |
| `chokudai_copy_sub_v11.hpp` | `cs::ChokudaiCopySub<Cost, Hash>` | turn開始候補を持ち越し、turn内は通常ビーム | 同turnの次sub層、または次turnへ |

以下では、`beam_`で始まる6種類を「通常ビーム」、`chokudai_`で始まる3種類を「chokudai」と呼びます。`Cost`の既定型は`long long`、`Hash`の既定型は`std::uint64_t`です。

## 1. 方式を選ぶ

### 1.1 single・multi・subの違い

`single`は、1回の候補生成がそのまま1手になる問題に向きます。たとえば、1人に1つの仕事を割り当てる問題なら、`turn`は割当済み人数です。異なる経路でも、同じ手数の状態を比較します。

`multi`は、1回の行動が異なる長さの時間や資源を消費する問題に向きます。たとえば所要時間が1時間と3時間の作業を選ぶ場合、`turn`を経過時間にして、それぞれ`step=1`と`step=3`を指定します。比較するのは同じ到着turnの候補です。候補を生成する回数と、進んだturn数は一致しません。途中のturnに状態がなくても、その先の到着候補を探索できます。

`sub`は、「全体としては2回生産するが、各生産の前に研修を何回行うかは経路によって異なる」といった問題に向きます。研修を`Next::Sub`、生産を`Next::Turn`にすると、研修回数にかかわらず、同じ生産回数を終えた状態同士で比較できます。

| sub版の座標 | 意味 | `Next::Sub`後 | `Next::Turn`後 |
| --- | --- | --- | --- |
| `turn` | 完了したturn数 | 変わらない | 1増える |
| `sub_step` | 現在turn内のSub遷移数 | 1増える | 0へ戻る |
| `depth` | 経路全体の遷移数 | 1増える | 1増える |

sub版では、同じ`expand`から両方の保存先へ候補を出せます。

```cpp
emit.push(trained, training_cost, BS::Next::Sub);
emit.push(produced, production_cost, BS::Next::Turn);
```

次sub層の候補群と、次turnの開始候補群は独立しています。次turn候補は、そのturn内の異なるsub層から集めて比較されます。次sub層が空になったら、残していた次turn候補へ進みます。両方が空なら、その経路からは先へ進めません。

Subの連鎖が有限になることは利用側の前提です。回数上限、未処理要素の減少、上限のある能力値などで保証してください。`max_turn`だけでは、同じturn内の無限なSub遷移を止められません。

### 1.2 copy・deltaの違い

`copy`では盤面などの`State`を候補ごとに保存し、`expand`は`now.state`から子状態を作ります。状態が小さい場合や、取り消し処理を実装せず使いたい場合に適します。結果の`best_state`は`std::optional<State>`です。

操作列も必要なら`run<State, Action>`を使います。ライブラリがActionを共有木で保持し、最良解の操作列を`result.path`へ返します。共通の祖先は共有し、不要になった枝は再利用します。祖先のStateは保持しません。操作列が不要なら`run<State>`を使い、履歴木の確保・参照カウント・操作列の復元を省きます。

`delta`では、小さな`Action`と探索木を保存し、盤面などの外部状態を`move_forward`と`move_backward`で移動させます。盤面コピーが重く、変更と取り消しが軽い場合に適します。変更前の値がなければ戻せない操作は、その値をActionへ保存します。`expand`の終了時には、外部状態を呼び出し時と同じ状態に保ってください。試しに手を適用して評価する場合も、その場で戻してから次の候補を扱います。

deltaの処理時間は、候補生成数だけでなく、枝を移るためのActionの戻し・再適用回数にも依存します。共通祖先が浅い多数の枝を長く残す場合は、幅が同じでも移動量が大きくなります。Stateのコピーが軽い問題ではcopy版とも比較してください。`Action`が小さいだけでは、deltaが常に速いとは限りません。

delta版の正常な`run`返却時には、外部状態は初期状態へ戻ります。最良解の盤面になっているわけではありません。必要なら、返された`path`を改めて再生します。callbackからの例外による中断まで自動復元されることは前提にしないでください。

| delta版 | callbackが受け取る代表的な型 | `result.path`の要素 | turn境界の確認 |
| --- | --- | --- | --- |
| single | `const Action&` | `Action` | 1要素が1turn |
| multi | `const BS::Edge<Action>&`、または`const Action&` | `BS::Edge<Action>` | `edge.turn`、`edge.step`、`edge.next_turn()` |
| sub | `const Action&` | `Action` | `result.turn_offsets` |

copyの履歴ありモードの`path`も、上表と同じ要素型とturn境界情報を返します。copyには状態更新のcallbackは不要です。

### 1.3 通常ビーム・chokudaiの違い

通常ビームは、ある層の候補を展開し、次へ進める候補を幅で絞ります。過去の層へ戻って未展開候補を再開する方式ではありません。

chokudaiは、turnごとに未展開候補を保存します。浅いturnから深いturnまで巡回する1回を「スイープ」と呼び、各turnから最大`sweep_width`件を取り出します。同じスイープで作った未来のturnの候補も、そのスイープ内で展開できます。最終turnへ到達した後も、時間・回数の余裕と未展開候補があれば、次スイープで別の経路を探索します。

`chokudai_copy_sub`でスイープをまたいで残すのは、各turnの開始候補です。取り出した開始候補からのturn内探索は、`sub_beam_width`の通常ビームとして処理します。時間切れの処理を除けば、選択した開始候補群から生じたsub層を処理してから次turnへ移ります。turn内のsub層そのものを、次スイープへ持ち越して再開する方式ではありません。

| 選ぶ基準 | 第一候補 |
| --- | --- |
| 1行動が必ず1手で、状態が小さい | `beam_copy_single` |
| 1行動が必ず1手で、盤面コピーが重い | `beam_delta_single` |
| 1行動で複数の時間・距離などを消費する | `beam_copy_multi`、または`beam_delta_multi` |
| 固定turnの中で、可変回数の小操作を行う | `beam_copy_sub`、または`beam_delta_sub` |
| 途中評価で後回しにした候補にも、後で探索機会を与えたい | 対応する`chokudai_copy` |

chokudaiは、より多くのturnに候補を残すため、一般に状態保存量が増えます。通常copy singleの保持状態数は主にビーム幅に、copy multiは主に`max_step × beam_width`に、chokudai copyは主に`max_turn × beam_width`に依存します。sub copyではturn境界幅とsub幅の両方が効きます。delta subは、現在地・候補・最良解に必要な祖先を残し、不要ノードをまとめて回収してスロットを再利用します。長い経路自体は保持するため、長いsub探索では`used_nodes`と`reserved_nodes`も確認してください。State内部のvectorや履歴の長さもメモリ使用量に影響します。

chokudaiは最良候補の取り出しと保持幅による選別の両方を行うため、同じ候補数でも通常ビームと処理費用が異なります。空になったturnキューの領域は再利用しますが、未展開候補の残るキューは次スイープのために保持します。再探索を使わない問題では、通常ビームも比較対象にしてください。

### 1.4 利用ケースからコード例を選ぶ

各ライブラリに、使うインターフェースや解の受け取り方が異なる2例を掲載しています。必要な出力が最終的な配置・集合・計画表であればcopyの`run<State>`、操作の実行順も必要なら`run<State, Action>`を選びます。deltaはAction列を保持し、状態の変更と取消を利用側で実装します。

| ライブラリ | 利用ケースA | 利用ケースB |
| --- | --- | --- |
| `beam_copy_single` | 例01：仕事の割当表を最終Stateから取得 | 例02：目標地点への移動コマンド列を取得 |
| `beam_delta_single` | 例03：割当を差分適用しAction列を取得 | 例04：移動を巻き戻し、途中のゴールを取得 |
| `beam_copy_multi` | 例05：時間内の作業集合を取得 | 例06：道路の順番と出発・到着時刻を取得 |
| `beam_delta_multi` | 例07：時刻依存の状態更新にEdgeを使用 | 例08：時刻を外部状態へ持たずActionで更新 |
| `chokudai_copy_single` | 例09：割当の未展開候補も後から探索 | 例10：未展開経路を残し、移動コマンド列を取得 |
| `chokudai_copy_multi` | 例11：作業集合を複数スイープで改善 | 例12：所要時間の違う経路のEdge列を取得 |
| `beam_copy_sub` | 例13：各期の前処理回数を計画表で取得 | 例14：日内の配達と日終了を分け、操作列を取得 |
| `beam_delta_sub` | 例15：期内操作を差分適用し、期別の区間を取得 | 例16：途中で完成した日の末尾まで操作列を再生 |
| `chokudai_copy_sub` | 例17：期の開始候補を残し、期内を探索 | 例18：日単位で別経路も探索し、途中完成の操作列を取得 |

Hash、遅延コピー、時間配分、CSVは、それらが役立つケースの中で説明します。各例は独立して実行できるように、データ構造・設定・候補生成・結果の出力を省略せず記述しています。

## 2. 共通の使い方と注意点

### 2.1 ヘッダと実行方法

ダウンロードした名前に`(1)`や`(2)`などの接尾辞が付いている場合は、冒頭の表の名前へそろえるか、コードの`#include`を実際のファイル名に合わせてください。各コード例は、個別の`.cpp`ファイルとして保存します。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -DLOCAL example.cpp -I ./headers -o example
./example
```

`-DLOCAL`はCSV記録を有効にする指定です。省略しても同じコードを実行できますが、`CsvStatHook`は記録・出力を行いません。

ヘッダを`.cpp`と同じフォルダへ置く場合、`-I ./headers`は不要です。C++23でも利用できます。サンプルはGCC向けです。これらのヘッダの利用に、AtCoder Libraryや追加のHashMapライブラリは不要です。

各サンプルではヘッダを1つだけ読み込みます。特に`bs::Beam`の各実装は同じ型名を定義するため、複数を同時にincludeしないでください。方式を比較する場合は、スケルトンのようなプリプロセッサによる切り替え、または別の実行ファイルを使います。複数のコード例をそのまま1ファイルへ連結すると、`main`なども重複します。

### 2.2 costは「小さいほど良い」

全9種類がcostの最小化です。得点や利益を最大化する場合は、たとえば`cost = -score`として渡します。`now.cost`は利用側が渡した評価値であり、ライブラリが移動費を自動加算することはありません。

costは累積費用でも、状態全体に対する評価値でも構いません。ただし、同じ保存先に入る候補同士で比較できる尺度にしてください。また、有効解に渡すcostは最終的に比較したい目的値と整合させます。途中の期待値と完成時の実得点を混ぜる場合は、その切り替えを利用側で設計します。

`Cost`はboolを除く符号付き整数、または浮動小数点型です。NaNの候補は棄却されます。初期コストがNaNなら、全種類でexpandを呼ばず、`found=false`・`completed_max_turn=false`を返します。正負の無限大はCostとして使用できます。演算時の整数オーバーフローは利用側で防いでください。サンプルは`long long`を使います。

ビーム幅で候補を捨てるため、一般に最適解の保証はありません。各例の実行例は、掲載データと設定で得られた結果です。実問題での速度や解品質は、評価関数・分岐数・幅・時間予算に依存します。割当や単純な経路探索など専用の厳密解法が使える場合もありますが、ここでは追加の状態や制約を持つ問題へ組み替えやすい形で、ライブラリの利用方法を示します。

### 2.3 Paramの読み方

省略した項目は`Param{}`の既定値です。サンプルでは用途に関係する設定を明示します。

| 設定 | 対象 | 意味・既定値 |
| --- | --- | --- |
| `max_turn` | 全種類 | 到達できる最大turn。既定0。初期状態はturn=0 |
| `beam_width` | 全種類 | 各候補群、または各turnの開始キューの保持幅。既定1 |
| `time_limit_ms` | 全種類 | 探索全体の時間制限。既定0.0で無制限。soft limit |
| `use_turn_time_limit` | 通常ビーム6種類 | 残り時間を残りturn数へ配分する。既定false。全体の時間制限が有効な場合だけ動作 |
| `time_check_interval` | 全種類 | 通常の時間確認間隔。既定64。1なら各展開終了後に確認する。境界などでも確認される |
| `continue_on_timeout` | 全種類 | 時間切れで解未発見なら救済探索を続ける。既定true。有効解保存済みならどちらでも終了 |
| `expand_answers` | 全種類 | 有効解も探索候補へ登録する。既定true。falseなら最良解の比較・保存だけを行う |
| `max_turn_is_answer` | 全種類 | 最大turnに到達した状態を有効解にする。既定true |
| `use_hash_dedup` | 全種類 | Hashを明示した候補を重複排除する。既定true |
| `max_step` | multiの3種類 | 1遷移で進めるturn数の上限。既定1 |
| `sub_beam_width` | subの3種類 | 同turnの次sub層の幅。既定0で`beam_width`を使用 |
| `sweep_width` | chokudaiの3種類 | 1turn・1スイープで取り出す候補数。既定1、`1..beam_width`へ丸める。sub版では開始候補数 |
| `max_sweeps` | chokudaiの3種類 | 通常スイープ数の上限。既定0で無制限。時間超過後の救済には適用しない |
| `hash_capacity` | single・multiの6種類 | Hash表の容量指定。既定0で自動。幅を基準に調整されるため、通常は0のままでよい |
| `print_warnings` | beam delta single・multi、beam copy multi | 内部Hash表などの異常を標準エラーへ出す。既定true |

`beam_copy_single`、chokudai single・multi、subの3種類には`print_warnings`はありません。subの3種類には`hash_capacity`もありません。対応していない項目をそのままコピーしないでください。

通常ビームのsingle・multi 4種類は内部の整数添字を安全に扱うため、`max_turn == INT_MAX` または `beam_width > 268435451` を `std::length_error` で拒否します。これは表現上の上限であり、その範囲内のメモリ確保を保証するものではありません。

delta single・multi の `Runtime::best_update_count` とCSV行の `period_best_updates`、delta multi の `Runtime::width_pruned_cost_count` は `long long` です。大量の候補を生成する場合も、利用側で `int` に狭めず扱ってください。

delta3種類は、必要に応じてノード格納領域を自動拡張します。利用側でノード容量を設定する必要はありません。single・subは不要ノードをまとめて回収し、multiは空きスロットの再利用と根の圧縮を行います。最良有効解の復元に必要な経路は保護します。

`runtime.used_nodes`は構築済みスロット数で、回収待ち・再利用待ちのスロットを含みます。`runtime.reserved_nodes`は配列に確保済みのスロット数です。multiの`live_nodes`は生存ノード数なので、`used_nodes`とは別の意味です。delta multiでは、正常終了時に復元用ルート1個が`live_nodes`に残りますが、展開候補は残りません（`runtime.live_costs.count == 0`）。Action内部の可変長データや補助配列のメモリは、これらのスロット数に含まれません。

自動拡張時には配列の再確保と要素移動が起こり得ます。整数添字の表現限界は`std::length_error`、通常のメモリ確保失敗は`std::bad_alloc`などの標準例外になります。これらは正常な「解なし」の結果ではありません。deltaの外部State復元は正常な返却時の保証であり、例外が出た場合の復元は保証しません。

全種類で`max_turn`の状態は展開されません。`max_turn=0`なら`expand`は呼ばれず、`max_turn_is_answer=true`なら有効な初期costの初期状態が解になります。正の`max_turn`で初期状態自体を有効解として残したい場合、初期状態に`is_answer`を指定する引数はありません。必要なら利用側で初期解を保持し、探索結果と比較します。

### 2.4 expandとrun

copy型の`expand`は、次のどちらかの形です。

```cpp
auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
    // now.stateを読み、子状態とcostをemitへ渡します。
};
```

```cpp
auto expand = [](const BS::StateView<State>& now, const BS::Runtime& runtime,
                 BS::Emitter<State>& emit) {
    // runtimeを使って統計や時間制限検出状況も参照します。
};
```

delta型は`StateView<State>`を`NodeView`へ、`Emitter<State>`を`Emitter<Action>`へ置き換えます。盤面は利用側で保持する外部状態を読みます。Runtimeは`const`参照です。`elapsed_us`は直近に時計を確認した時点の値で、呼び出し中に自動的に進む値ではありません。

copy型は、次の形で呼び出せます。初期Hashとdebug hookは省略できます。

```cpp
auto result = BS::run<State>(param, initial_state, initial_cost, initial_hash, expand, hook);
auto result_without_hash = BS::run<State>(param, initial_state, initial_cost, expand);
```

delta型も初期Hashとdebug hookを省略できます。省略時の初期Hashは`Hash{0}`です。現在のHashから差分で次のHashを計算する場合は、初期状態に対応する値を指定してください。候補を追加するときのHash指定とは独立しています。`has_hash`を持つViewでは、初期Hashを省略した初期状態の`has_hash`はfalseです。

```cpp
auto result = BS::run<Action>(param, initial_cost, initial_hash,
                              expand, move_forward, move_backward, hook);
auto result_without_hash = BS::run<Action>(param, initial_cost,
                                           expand, move_forward, move_backward);
```

`run`を呼ぶたびに新しい探索を開始します。前回の未展開候補を引き継ぐ再開APIではありません。`StateView`や`Emitter`への参照を外へ保存して後で使わず、callbackの実行中だけ扱ってください。

copyで操作列を分離して保持する呼び出しは次の形です。`expand`の引数型も`Emitter<State, Action>`にします。Runtimeの有無、初期Hash・hookの省略は同じです。

```cpp
auto result = BS::run<State, Action>(param, initial_state, initial_cost, initial_hash, expand, hook);
```

### 2.5 pushとpush_lazyの引数

`value`はcopy型では`State`、delta型では`Action`です。`hash`と`is_answer`は任意ですが、multiの`step`は必須です。次の表は全項目を指定する形です。

| 系統 | `push`の最大引数形 | copy型の`push_lazy`の最大引数形 |
| --- | --- | --- |
| single | `push(value, cost, hash, is_answer)` | `push_lazy(cost, hash, is_answer, maker)` |
| multi | `push(value, cost, hash, step, is_answer)` | `push_lazy(cost, hash, step, is_answer, maker)` |
| sub | `push(value, cost, hash, is_answer, next)` | `push_lazy(cost, hash, is_answer, maker, next)` |

subの`next`は省略すると`BS::Next::Turn`です。`push_lazy`では`maker`の後に置きます。delta型には`push_lazy`はありません。

`step`は`int`型で、1以上かつ`max_step`以下です。到着turnが`max_turn`を超える候補は受理されません。multiの`step=0`を、subの代わりに使うことはできません。

Hash引数はbool以外の符号なし整数で、`Hash`型以下の幅が必要です。たとえば`Hash{123}`を使います。`is_answer`には`true`または`false`などのboolを渡し、整数の0や1で代用しないでください。StateやActionは指定した型そのものを渡します。

`push_lazy`は、保存する必要があると判断されたときにだけ、引数なしの`maker()`からStateを作ります。makerは同期的に呼ばれ、後で実行するために保存されません。このためサンプルの`[&now, ...]`は有効です。候補を棄却するために必要なcostやHashはmakerより前に計算する必要があります。makerの中だけで計算して外部へ渡す設計にはできません。

Stateコピーを遅らせたい場合、先に巨大な`State next`を作ってそれをラムダへコピーすると利点を失います。小さな変更情報をキャプチャし、maker内で親Stateをコピーして変更します。

全9種類で、`push`と`push_lazy`の戻り値は「その呼び出し時点で継続探索用の候補群へ入ったか」です。trueでも後の候補に置換される可能性があります。最良有効解だけを保存する場合はfalseです。最大turnに到達した候補は展開しないので、候補群へ入れずfalseを返し、必要なら最良解として保存します。解の有無は`result.found`で確認します。

copy6種類の`push_lazy`は、探索候補への保存、または最良有効解の更新に必要な場合にmakerを呼びます。両方に使う場合でも1回だけです。幅・Hashで棄却され、かつ最良解を更新しない場合や、NaN・到着turn超過などの不正候補では呼びません。makerの呼び出し回数を、探索候補の採用数と同一視しないでください。

copyで操作履歴も保持する場合は、`Emitter<State, Action>`を使い、`push`のStateの直後、`push_lazy`の先頭へActionを追加します。Hashと`is_answer`の省略規則は同じです。

| 系統 | 履歴あり`push` | 履歴あり`push_lazy` |
| --- | --- | --- |
| single | `push(state, action, cost, hash, is_answer)` | `push_lazy(action, cost, hash, is_answer, maker)` |
| multi | `push(state, action, cost, hash, step, is_answer)` | `push_lazy(action, cost, hash, step, is_answer, maker)` |
| sub | `push(state, action, cost, hash, is_answer, next)` | `push_lazy(action, cost, hash, is_answer, maker, next)` |

Actionはコピー構築可能にしてください。デフォルト構築・代入は不要です。Actionはmakerの外で指定しますが、保存が必要な場合だけ履歴ノードへコピーまたはムーブされます。Stateには評価・遷移に必要な情報を残し、出力だけの履歴を分離するとコピーを軽くできます。

### 2.6 Hashによる重複排除

Hashを明示した候補だけが重複排除に参加します。初期Hashを渡しただけでは、その後のHashなし`push`が自動的に重複排除対象になるわけではありません。逆に、初期Hashを省略しても、Hash付き`push`を使えば、その候補は重複排除対象になります。

同じHashは「将来の選択肢と目的値への影響が同じ」とみなされます。ライブラリはStateの等値比較で衝突を確認しません。小さな状態は使用集合のビットマスクなど、衝突のない整数へ変換すると理解しやすくなります。一般の大きな状態に64bit Hashを使う場合は、衝突の可能性を利用側で受け入れる必要があります。

たとえば割当問題では、同じturn・同じ使用仕事集合なら将来の仕事候補が同じです。小さい累積費用の経路を残してよいので、Hashに履歴全体を含める必要はありません。一方、残り燃料や最後の訪問地点が今後の可否を変えるなら、その情報もHashへ含めます。

重複排除の範囲は、その時点で同じ保存先に残っている候補です。chokudaiは展開済みHashの永久的な訪問済み集合を持ちません。subでは次sub層と次turn候補を別々に扱います。次turnへ進んだ後にも経路全体の操作回数などで制約が変わる問題では、異なるdepthを同じHashで誤って統合しないようにします。

同じHashで同じcostなら、後から来た候補が必ず優先されるわけではありません。`is_answer`の有無が解の可否を変える状態や、終端・非終端の違う状態を安易に同一Hashへまとめないでください。

### 2.7 is_answerと解の保存

`is_answer=true`は「この生成状態を有効解として比較できる」という印です。探索全体を直ちに終了する命令ではありません。有効解からさらに探索するかは、全種類共通の`expand_answers`で指定します。

| 設定 | 向いている使い方 | 動作 |
| --- | --- | --- |
| `expand_answers=true`（既定） | 途中の計画も回答に使え、さらに作業を追加して改善したい | 最良解として比較・保存し、探索候補としても通常の選別に参加させる |
| `expand_answers=false` | ゴール到達・全必須作業完了など、有効解になった時点でその経路を終える | 最良解として比較・保存し、探索候補へは登録しない |

falseにすると有効解は幅・Hashの選別に参加せず、ビーム幅を消費しません。subの`Next::Sub`と`Next::Turn`の両方、chokudaiの未展開キューにも同じ方針を適用します。探索候補へ登録しなくても、最良解のState・Action列は返ります。`push`／`push_lazy`の戻り値は探索候補への採用を表すため、解を保存した場合でもfalseです。非改善の有効解しか生成せず、登録も不要なら、`push_lazy`のmakerは呼びません。

時間内は残りの非有効解を探索し、別経路のより良い有効解を比較し続けます。有効解の一部だけを終端にしたい場合はtrueにし、終端状態だけ利用側の`expand`でreturnします。最大turnの状態は、この設定によらず展開しません。

`max_turn_is_answer=true`なら、最大turnに到達した状態も有効解です。最大turn到達だけでは目的を達成しない問題ではfalseにし、目的条件を満たす候補へ`is_answer=true`を渡します。

| 系統 | 有効解として保存されるタイミングと注意 |
| --- | --- |
| 通常ビームsingle・multiの4種類 | 正常な候補の生成時に、最良有効解を幅・Hash選別とは独立して保存する。探索候補として棄却された場合も、保存した解は残る |
| chokudai single・multi | 正常な候補の生成時に、幅・Hashによる探索候補の受理可否と独立して最良解を保存する。棄却された有効解も比較対象になる |
| subの3種類 | 最良有効解の更新を、探索候補群の幅・Hash選別と独立して扱う。探索候補として残せなくても、改善する有効解なら保存する |

通常ビームとchokudaiのsingle・multiでは、候補のcost・step・到着turnが許容範囲であることを確認し、探索候補として残るかどうかにかかわらず有効解を比較します。解として保存するのは最良の1件で、この保存用領域はビーム幅を消費しません。`expand_answers=true`で別途探索候補にも登録する場合、その探索候補は幅に含まれます。有効解同士ではcostが小さい方を残し、同costなら先に生成した方を残します。multiでは未来の到着turnの候補も生成時に保存し、`best_turn`にはその到着先を記録します。保存済みの解は、その後の候補置換、時間超過時の候補削減、探索木のノード解放で失われません。生成していない候補まで含めた最適解の保証ではありません。

copy single・multiの結果は`best_state`です。delta singleの`path`はAction列、delta multiの`path`は`action`・`turn`・`step`を持つEdge列です。delta singleは親ノード番号と末尾Actionで解を保持し、返却時に経路を復元します。delta multiはノードが再利用される前に、経路を独立した配列へ保存します。

`found`は有効解の有無です。全9種類で`completed_max_turn`は、不正でない最大turn到着候補を生成できたかを表します。最適性や全候補展開の保証ではありません。有効な初期costで初期状態だけを扱う`max_turn=0`も最大turn到達です。`max_turn_is_answer=false`なら、最大turnへ達しても`found=false`になり得ます。`best_cost`や`best_state`、`path`を解として使う前に、`found`を確認してください。

sub版では、有効解がturn途中でも返ります。`best_turn`、`best_sub_step`、`best_depth`で位置を確認できます。delta subの`turn_offsets`は、found=falseなら空、found=trueなら先頭0を持ちます。完了したturn tのAction区間は`[turn_offsets[t], turn_offsets[t + 1])`です。`turn_offsets.back()`から`path.size()`までが残っていれば、そこは未完了turn内の末尾です。

### 2.8 時間制限と打ち切り

`time_limit_ms`は1turnごとの時間ではなく、1回の探索全体の時間です。全種類とも`expand`を途中で中断しません。長いcallback、返り値の復元、CSV書き出しなども考慮し、提出全体の厳密な締切と同一視しないでください。

全9種類で`continue_on_timeout`の意味は共通です。全体の時間切れを検出した時に有効解があれば、設定によらず保存済みの最良解を返します。解未発見の場合、falseなら探索を終了し、true（既定）なら時間超過を許容して救済探索を行います。

| 系統 | 解未発見・`continue_on_timeout=true`の救済方法 |
| --- | --- |
| 通常ビームsingle・multi | 最良1本を中心とする縮退探索で、先のturnへの到達を試みる |
| 通常ビームsub | 次turn候補があればturn内探索を切り上げて進む。なければ、残ったsub候補から生成を試みる |
| chokudai single・multi | 各turnの取り出し幅1で救済スイープを繰り返し、残った別経路も試す |
| chokudai sub | 各turnの開始候補の取り出し幅を1にし、次turnへ進める候補を優先する。必要なら残った開始候補から救済スイープを繰り返す |

救済中に有効解を見つけたら、現在の`expand`が返ったところで終了します。同じ`expand`内で複数の有効解を生成した場合は、その中で最良のものも比較・保存します。幅・Hashで探索候補として棄却された解や、`expand_answers=false`で登録しなかった解も、終了判定に含めます。

救済中も候補の選別を行うため、有効解の発見保証はありません。探索可能な候補が尽きれば解なしで終了します。時間超過の長さには上限を設けないため、追加探索を許容しない用途では`continue_on_timeout=false`を指定します。falseでも時計確認の間隔があり、実行中のcallbackを中断しないため、厳密な時刻での強制停止にはなりません。

chokudaiの`max_sweeps`は通常スイープの上限で、救済スイープには適用しません。時間切れ前に通常スイープ上限へ達した場合は、解未発見でも回数上限で終了し、救済を開始しません。時間制限が無効なら`continue_on_timeout`によって探索を打ち切ることはありません。

通常ビーム6種類では、`use_turn_time_limit=true`でturnごとの時間配分を使えます。`run`、`expand`、`push`の形は同じです。chokudaiの3種類には、この設定はありません。

```cpp
param.max_turn = 100;              // 初期turn=0から100turnを完了させます。
param.beam_width = 1000;           // 次へ引き継ぐ候補数の上限です。
param.time_limit_ms = 1900.0;      // 全体の時間予算です。
param.use_turn_time_limit = true;  // 残り時間を残りturnへ配分します。
param.time_check_interval = 1;     // 各expand終了後に時間を確認します。
```

各turn開始時の予算は`max(0, 全体予算 - 経過時間) / (max_turn - turn)`です。内部ではマイクロ秒の整数で計算し、端数は切り捨てます。1マイクロ秒未満の配分は0になり、そのturnの締切へ直ちに到達したものとして扱います。時間制限なしの指定とは区別されます。最終turnは展開しないので予算を配分せず、`max_turn=0`でも除算しません。

例えば全体1,000ms・10turnで最初が50msで終われば、次の予算は950/9、約105.6msです。最初に150msかかれば850/9、約94.4msになります。予算はそのturnの途中では再計算せず、Sub層を進めても締切は延長しません。候補を全て展開し終えたら、予算が余っていても次へ進みます。

turn期限到達後は、singleでは次turn候補、multiでは現在より先のいずれかの到着turnの候補、subでは次turnの開始候補があれば、そのturnの残りの展開を切り上げます。最終turnへの到達が確定した場合も切り上げられます。未来候補がなければ、探索可能な候補から未来候補を生成できるか、候補が尽きるまで継続します。multiでは直後のturnが空でも、その先の到着候補を維持します。

turn期限だけでは保持幅を1に縮めず、生成済み候補から通常の選抜で最大`beam_width`件を引き継ぎます。次turnでは期限到達フラグを解除して通常探索へ戻ります。全体の期限にも達した場合は、上記の`continue_on_timeout`による終了・救済処理が優先します。全体時間制限に達していない間は、`is_answer=true`を指定しても探索全体を自動で終了することはありません。

配分を有効にしても、実行中のexpand・遅延生成・状態の移動や復元を強制中断することはありません。初期化やturn間の候補実体化に費やした時間も、次回の残り時間に反映されます。ビーム幅を自動的に増やす機能ではなく、turnごとの探索時間の偏りを抑えるための機能です。重要な探索が特定turnへ集中する問題や、上限より早く探索が終わる問題では、無効時との比較も行ってください。

掲載例は探索全体の予算を1,000ms、通常の時計確認間隔を64展開にしています。通常ビームで全turnの完了を目指す例01・03・07・13・15は、turn別配分も有効にしています。途中のゴールや任意の停止時点を比較する例では配分を有効にせず、問題に合わせて利用側が選べる形にしています。これらの値は小さい掲載データ向けで、一般の問題に対する推奨幅・予算ではありません。`time_check_interval`を大きくすると時計確認の頻度は減りますが、時間超過の検出が遅くなり得ます。

### 2.9 Runtime・CSV・独自hook

設定値は全9種類で`runtime.param`から参照します。例えば、現在処理中のターンは`runtime.turn`、最大ターンの設定は`runtime.param.max_turn`です。chokudai single・multiのRuntimeは現在ターンの代わりにスイープの進捗を持ちます。設定を変更して次の探索を行う場合は、runへ渡す`Param`を変更します。callbackが受け取るRuntimeは読み取り専用です。

通常copy single・multiとdelta multiのコスト分布は、`CostStats`型の`runtime.live_costs`と`runtime.selected_costs`で参照します。`live_costs`は保持中の候補群、`selected_costs`は直近の選抜候補群です。`best`が最小cost、`worst`が最大cost、`average()`が平均、`count`が集計件数です。`sum`は平均計算用のdouble型の合計です。集計対象が空なら`count=0`で、best・worst・average()は0です。解として保存した最良値は`runtime.best_cost`で別に扱います。

CSVではこれらを`live_best_cost`・`live_worst_cost`・`live_avg_cost`、`selected_best_cost`・`selected_worst_cost`・`selected_avg_cost`の列へ出力します。分布はHookのスナップショット更新時点の値です。delta multiの選抜分布はHook利用時に集計し、Hookを省略した最終結果でも保持中分布は確認できます。

通常ビームsingle・multiでは、`Runtime::found`、`best_cost`、`best_turn`、`best_update_count`は、解を生成した`push`／`push_lazy`の中で更新されます。`best_update_count`は実際の改善回数です。進捗はTurnEnd、最終値はRunEndまたはResultで確認できます。初期状態自体が解になる0turn探索でも、RunEndとResultで確認できます。delta multiの経路は、解の改善時に保存します。

chokudai single・multiでも、解を保存した`push`／`push_lazy`が返る前に、`Runtime::found`、`best_cost`、`best_turn`、`best_update_count`と`last_best_update_sweep`を更新します。探索候補へは棄却された有効解の改善も含みます。CSVではSweepEndやRunEndで確認します。

copy single・multiの`active_width`は、そのturnで`expand`を呼び出した件数です。`max_active_width`はそこまでの最大値を示します。

multiでは、現在の探索turnより`best_turn`の方が先になる場合があります。これは未来の到着候補を生成時に解として保存したためです。 `turns_since_best_update`は、最良解を生成した探索turnから数えます。到着先の`best_turn`からは数えません。

`LOCAL`を定義してコンパイルし、`run`の最後の引数へ`BS::CsvStatHook("filename.csv", interval_ms)`を渡すと、進捗を記録できます。既定の記録間隔は50ms、0なら対象イベントを全て記録します。CSVはメモリに蓄積してRunEndで書き出すため、細かい記録には時間・メモリの費用があります。同名ファイルは上書きされるので、用途ごとに名前を変えてください。

全9種類とも、`LOCAL`未定義時の`CsvStatHook`はコンストラクタと呼び出し演算子だけを持つ空実装です。`LOCAL`の有無で、コンストラクタの引数・既定値と`run`への渡し方は変わりません。未定義時はCSV用データの保持・ファイル出力を行わず、エンジン側もhook省略時と同じ扱いになります。`rows`などのCSV用メンバを直接参照するデバッグコードも、`#ifdef LOCAL`で囲んでください。独自hookと`Runtime`は`LOCAL`の有無にかかわらず利用できます。

全9種類で、`CsvStatHook::rows`にはRuntimeとイベントのスナップショットを保存します。期間差分や比率を出すsingle・multiでは、これらをCSVへ書き出す際に計算します。探索中に独自の解析を行う場合は、独自hookから`Runtime`を参照してください。

全9種類のCSV記録間隔は、負値・NaNも0と同じ扱いです。正の無限大・表現範囲を超える間隔は内部の最大値へ丸めます。CSVの小数点はロケールに依存せず `.`、`signed char` のCostも文字ではなく数値で出力します。浮動小数点のCostは再読込時に値を復元できる精度で書き、`found=false`の行の`best_cost`・`best_turn`は空欄にします。

| 系統 | 独自hookが受け取れる主なイベント | CsvStatHookが記録するイベント |
| --- | --- | --- |
| 通常ビームsingle・multi | RunStart、TurnEnd、RunEnd | RunStart、TurnEnd、RunEnd |
| chokudai single・multi | RunStart、SweepStart、SweepEnd、RunEnd | RunStart、SweepEnd、RunEnd |
| 通常ビームsub | RunStart、SubStepEnd、TurnEnd、RunEnd | 同左。間隔に従って記録 |
| chokudai sub | RunStart、SubStepEnd、TurnEnd、SweepEnd、RunEnd | 同左。間隔に従って記録 |

subのSubStepEndは、その層を処理し終えた通知です。通常ビームsubが時間超過で層の途中から次turnへ進む場合、その層のSubStepEndは出ませんが、TurnEndは出ます。全体の時間切れで探索自体を終了する場合は、通常ビームもchokudaiもTurnEndが出ないことがあります。RunEndの停止理由と併せて読みます。chokudai single・multiの`sweep_completed`はスイープを完走したかを表しますが、chokudai subにはこの項目やSweepStartはありません。

通常ビーム6種類のRuntimeには、turn別配分を観測する次の項目があります。これらは時計を最後に確認した時点の値で、expand中に時刻が進んでも自動更新されません。

| Runtime項目 | 意味 | CSV列 |
| --- | --- | --- |
| `turn_start_us` | run開始から、そのturnの配分を開始するまでの時間 | `turn_start_ms` |
| `turn_deadline_us` | run開始から数えた、そのturnの締切 | `turn_deadline_ms` |
| 上の2値の差 | turn開始時に割り当てた予算 | `turn_budget_ms` |
| `turn_elapsed_us` | turn開始後、最後の時計確認までの経過時間 | `turn_elapsed_ms` |
| `turn_time_limit_reached` | そのturnの締切到達を検出したか。次turnでリセット | 同名 |

配分無効・turn未開始のstart/deadline/budgetは-1、経過時間は0です。CSVの時刻はミリ秒へ変換します。`time_limit_reached`は全体の期限のフラグで、turn期限とは独立です。期限到達フラグは、未展開候補を実際に打ち切ったことを意味しません。全候補を処理した直後に期限到達を検出する場合もあります。返却時の`result.runtime`に残るturn別情報は最後に処理したturnのもので、過去のturnの超過履歴はCSVなどで確認します。

`turn_elapsed_us`はTurnEnd通知前、または全体時間切れで探索を打ち切った時に固定し、その通知のcallback、返却時の最終復元、RunEndの処理時間は加算しません。sub版ではTurnEndの後に行う次turn候補の実体化も、各turnの計測区間の外です。これらの探索中の費用は全体の経過時間に含まれ、次turnの予算に反映されます。全体の経過時間とturn別実測時間の合計が一致するとは限りません。

全9種類のCSVには`continue_on_timeout`と`expand_answers`も記録します。single・multiでは各行、subではRunStart行の設定欄に出します。Runtimeでは全9種類とも`runtime.param.continue_on_timeout`、`runtime.param.expand_answers`から参照できます。

`answer_only_candidates`は、`expand_answers=false`により探索候補へ登録しなかった、最大turn未満の有効候補数です。最良解を更新したかどうかにかかわらず数え、幅・Hashによる棄却数には加えません。最大turn到着候補は`terminal_candidates`だけに数えます。どちらも生成候補数に含まれますが、探索候補の採用数には含まれません。

全turnの予算・実測時間・展開数を解析するときは、`CsvStatHook("beam_stat.csv", 0)`で各TurnEndを記録してください。single・multiのCSVは`use_turn_time_limit`を各行に出し、subのCSVは他の設定と同じくRunStart行へ出します。

独自hookも使えます。

```cpp
auto hook = [](BS::EventType event, const BS::Runtime& runtime) {
    if (event == BS::EventType::RunEnd) {
        std::cerr << "expanded=" << runtime.expanded_nodes << '\n';
    }
};
```

既定の`BS::NoOp`は通知を無視します。何もしないラムダと比べても、ライブラリが観測用処理を省略できる場合があります。本番で統計が不要ならhookを省略してください。Runtimeの詳細項目は方式ごとに異なります。全9種類で設定は`runtime.param`にまとまっています。`max_turn`や`beam_width`などは探索に使用する範囲へ正規化した値です。chokudai single・multiの`hash_capacity`は指定値を保持し、実際のHash表サイズは幅に応じて調整されます。

| 調べたいこと | 主に見る情報 |
| --- | --- |
| 探索がどこまで進んだか | turn、sub_step、sweep、completed_max_turn。single・multi通常ビームのcopyではmaterialized_turnも参考になる |
| 生成量と展開量 | expanded_nodes、generated_candidates、terminal_candidates。subではsub_generatedとturn_generatedを分けて読む |
| 幅が足りないか | pruned_by_width、候補数、selected_widthなど。subではsub側とturn側を分ける |
| 重複が多いか | pruned_by_hash、replaced_by_hashなど、存在する方式の項目 |
| 解がいつ改善したか | best_cost、best_update_count。subではfirst_solution_ms、last_best_update_ms |
| 全体の時間超過後にどれだけ探索したか | time_limit_reached。subではexpanded_after_timeout、timeout_turn_advances、chokudai subではrescue_sweepsも参照。turn期限だけの超過はこれらに加算しない |
| deltaの状態移動費用や容量 | move_forward_count、move_backward_count、used_nodesなど |
| なぜ終わったか | 各Resultのフラグ、chokudai・subのruntime.stop_reason |

表の`sub_generated`、`turn_generated`、`first_solution_ms`、`last_best_update_ms`はCSV列名です。Runtimeではそれぞれ`sub_stats.generated`、`turn_stats.generated`、`first_solution_us`、`last_best_update_us`を参照し、時刻の単位はマイクロ秒になります。

通常ビームとchokudaiのsingle・multiの採用数・幅棄却数・Hash棄却数は、探索候補の選別についての統計です。選別で落ちた候補でも、`best_update_count`を増やす場合があります。有効解の確保は`found`と`best_cost`、改善の回数は`best_update_count`で確認します。

全9種類で`terminal_candidates`は、不正でない最大turn到着候補の生成数です。最大turn候補も生成数に含みますが、accepted数・幅棄却数・Hash棄却数には含めません。最大turnを解と見なさない設定でも数えます。初期状態だけで終わる`max_turn=0`では候補を生成しないため0です。`completed_max_turn`は最大turnの候補を生成できたことを示し、解の存在は`found`で確認します。copy single・multiの`materialized_turn`は到着候補の確定処理が済んだturnで、最大turnでStateを候補群へ保存したことを意味しません。

sub版のCSVでは、候補cost欄は対応する候補数が0なら空欄です。subの`sub_stats.accepted`と`turn_stats.accepted`は探索候補群への保存回数で、有効解だけを保存した回数とは一致しません。`push`のtrue数はaccepted数に対応しますが、後の置換があるため最終的に残った候補数とは一致しません。棄却率が高いだけで、その設定が悪いとも断定できません。解の改善量、展開数、実時間を一緒に比較します。

### 2.10 copyの操作履歴を参照する

履歴ありモードでは`emit.history()`が、現在展開している状態へ到達するまでの操作列を指す軽量な`HistoryView<Action>`を返します。Viewはノード番号を持つ参照用オブジェクトで、操作列を複製しません。次に生成する子の履歴ではなく、`now.state`の履歴です。

```cpp
auto history = emit.history();
if (!history.empty()) {
    const Action& last = history.action(); // 直前の操作をconst参照で読む。
    auto parent = history.parent();       // その操作より前の経路を指す。
    // lastを使った判定はpush / push_lazyより前に済ませる。
}
```

`empty()`がtrueのViewでは`action()`・`parent()`を呼ばないでください。初期状態の履歴は空です。`parent()`を繰り返すと直前から初期方向へたどれます。結果の`result.path`は逆に、初期状態から解へ向かう実行順です。

Viewは当該`expand`内だけ有効です。View自体は`push`・`push_lazy`後も同じ祖先を指しますが、取得した`const Action&`やその内部への参照・ポインタは履歴配列の拡張で無効になり得ます。追加後に必要ならViewから再取得してください。参照をStateやActionへ保存して次のexpandへ持ち越さないでください。祖先Stateの参照APIはありません。

履歴参照を呼ばなければ、参照用の走査や配列生成は行いません。`empty()`・`action()`・`parent()`はそれぞれ定数時間です。全履歴を毎回たどると経路長に比例するため、頻繁に必要な集計値はStateへ持たせる方が適します。履歴保持自体の費用は`run<State, Action>`にだけ発生します。

履歴で次の操作を制限する場合、その違いは将来の探索に影響します。Hashで重複排除するなら、必要な履歴の要約をState・Hashへ含めてください。操作列が異なるだけで将来の選択肢が同じなら、全履歴をHashに含める必要はありません。

singleの`path`はAction列、multiは`Edge<Action>`列です。multiのAction参照では時刻情報は自動で付かないため、expand内の判断に必要ならActionへ必要な値を含めてください。subの`turn_offsets[0]`は0で、各Turn遷移の直後の位置が追加されます。途中Sub状態が最良解なら最後の区間は未完了turnで、`path.size()`が最後のoffsetより大きくなる場合があります。

## 3. beam_copy_single_v40.hpp

### 例01：仕事の割当表を最終Stateから受け取る

仕事を1人ずつ割り当て、最後に「各担当者へ何の仕事を割り当てたか」の表を出力します。配置や割当そのものが回答になる問題では、`run<State>`で最終状態を受け取る形が簡単です。操作列は保存しません。

`max_turn`は担当者数、`beam_width`は同じ割当済み人数の候補数です。使用済み仕事集合をHashにし、同じ集合なら累積費用が小さい割当を残します。最終割当表のコピーは`push_lazy`のmaker内で行います。`max_turn_is_answer=true`により全員への割当が完成解になり、`best_state`から表を取り出せます。

`use_turn_time_limit=true`で全体1,000msを残り担当者数へ配分します。CSVは通常の探索へ付ける補助機能として併用し、`LOCAL`定義時だけ記録します。費用は加算型なので、同じ使用集合に対する安い部分解が将来も有利になることが、Hashによる統合の前提です。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_copy_single_v40.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

// 最終的な割当表そのものが回答なので、操作履歴を別に持つ必要はありません。
// Stateはコピー可能な値型にし、探索中に共有の配列を書き換えないようにします。
struct State {
    unsigned used = 0;
    std::array<int, 4> job_for_worker{{-1, -1, -1, -1}};
};

int main() {
    // fee[担当者][仕事]。標準入力なしで動く、小さな割当問題です。
    const std::array<std::array<int, 4>, 4> fee{{
        {{9, 2, 7, 8}}, {{6, 4, 3, 7}},
        {{5, 8, 1, 8}}, {{7, 6, 9, 4}}
    }};
    BS::Param param{};
    param.max_turn = 4; // 1人への割当が1turn。初期turn=0、4人で完成です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = true; // 最大turnへの到達を完成解として扱います。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.use_turn_time_limit = true; // 残り時間を残りturn数に配分します。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const unsigned used = now.state.used | (1U << job);
            const long long cost = now.cost + fee[now.turn][job];
            // 同turn・同使用集合なら、安い方を残してよい問題です。
            // 割当表が大きくなる場合に備え、必要な候補だけStateをコピーします。
            // makerはこのpush_lazy中に高々1回呼ばれ、後で呼ばれることはありません。
            emit.push_lazy(cost, static_cast<Hash>(used), [&now, job, used] {
                State next = now.state;
                next.used = used;
                next.job_for_worker[now.turn] = job;
                return next;
            });
        }
    };

    // -DLOCALを付けたビルドでのみCSVを記録します。未定義時は空実装です。
    // 第2引数50.0は記録の最小間隔ms。0.0なら対象イベントを毎回記録します。
    BS::CsvStatHook csv("copy_single_assignment.csv", 50.0);
    // run<State>はStateだけを保存します。初期costは0、初期使用集合も0です。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) { // best_stateを参照する前に、有効解の有無を確認します。
        std::cout << "no solution\n";
        return 0;
    }
    std::cout << "cost=" << result.best_cost << " jobs=";
    for (int job : result.best_state->job_for_worker) std::cout << job << ' ';
    std::cout << '\n';
}
```

実行例：

```text
cost=13 jobs=1 0 2 3
```

仕事番号は0始まりです。出力の並びは担当者0・1・2・3への割当を表します。

### 例02：目標地点への移動コマンド列を受け取る

壁のある盤面で目的地へ移動し、`U/R/D/L`の操作列を出力します。最終位置だけでは回答できないため、`run<State, Action>`を使い、Stateには現在位置、Actionには1回の移動を分けて持たせます。`max_turn`は手数上限であり、上限より前のゴールも解です。

`max_turn_is_answer=false`とし、目的地へ着く候補へだけ`is_answer=true`を渡します。途中の評価は「使用手数＋マンハッタン距離」、ゴールでは実際の使用手数です。`result.path`は初期状態からのAction列で、そのまま出力できます。

`emit.history()`で直前のActionを参照し、この静的な迷路では不要な直後の往復を省きます。生成可能な手が直前方向に依存するので、Hashにも直前方向を含めます。Actionへの参照は`push`で無効になり得るため、必要な方向だけを先に読み取ります。履歴を使わない問題に、この枝刈りをそのまま持ち込む必要はありません。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include "beam_copy_single_v40.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

constexpr int HEIGHT = 4, WIDTH = 5;
constexpr int GOAL_ROW = 3, GOAL_COL = 4;
const std::array<std::string_view, HEIGHT> board{
    "S..#.", ".#...", "...#.", "#...G"
};
constexpr std::array<int, 4> dr{-1, 0, 1, 0}, dc{0, 1, 0, -1};
constexpr std::array<char, 4> command{'U', 'R', 'D', 'L'};

// 探索中は残距離も評価に使います。ゴールで0になるので、解のcostは実手数です。
int distance_to_goal(int row, int col) {
    return std::abs(row - GOAL_ROW) + std::abs(col - GOAL_COL);
}
Hash state_hash(int row, int col, int previous) {
    // 直前の逆操作を省くため、位置だけでなく直前方向もHashに含めます。
    // previous=-1は初期状態。小さい状態なので、この整数化には衝突がありません。
    return static_cast<Hash>((row * WIDTH + col) * 5 + previous + 1);
}

// Stateは現在位置だけ。出力用の文字列を毎回コピーしません。
struct State { int row = 0, col = 0; };
// Actionは1回分の操作です。ライブラリが共有木で履歴を保存します。
struct Action { int direction = 0; };

int main() {
    BS::Param param{};
    param.max_turn = 12; // 最大12手。ゴールなら12手より前でも完成です。
    param.beam_width = 64; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State, Action>& emit) {
        // 1手前のActionを参照します。履歴全体のコピーや走査は不要です。
        const auto history = emit.history();
        int previous = -1;
        if (!history.empty()) {
            const Action& last = history.action();
            previous = last.direction; // 参照が無効化され得るpushより前に読み取ります。
        }
        for (int dir = 0; dir < 4; ++dir) {
            // この静的な迷路では、直後の往復は最短路に不要です。
            if (previous >= 0 && dir == (previous + 2) % 4) continue;
            const int row = now.state.row + dr[dir], col = now.state.col + dc[dir];
            if (row < 0 || row >= HEIGHT || col < 0 || col >= WIDTH) continue;
            if (board[row][col] == '#') continue;
            const bool goal = row == GOAL_ROW && col == GOAL_COL;
            const long long cost = now.turn + 1LL + distance_to_goal(row, col);
            // 履歴ありpushはStateの直後にActionを渡します。
            emit.push(State{row, col}, Action{dir}, cost, state_hash(row, col, dir), goal);
        }
    };
    // 第2テンプレート引数Actionを指定すると、最終Stateと操作列を両方受け取れます。
    const auto result = BS::run<State, Action>(
        param, State{}, static_cast<long long>(distance_to_goal(0, 0)),
        state_hash(0, 0, -1), expand
    );
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    // pathは初期状態から解へ向かう実行順です。pushの受理可否で解を判定しません。
    std::cout << "steps=" << result.best_turn << " cost=" << result.best_cost << " moves=";
    for (const Action& action : result.path) std::cout << command[action.direction];
    std::cout << '\n';
}
```

実行例：

```text
steps=7 cost=7 moves=RRDRRDD
```

同じ手数の経路が複数ある場合、コマンド列の形は1通りとは限りません。

## 4. beam_delta_single_v46.hpp

### 例03：割当を外部状態へ差分適用し、Action列を返す

担当者へ仕事を1つずつ割り当てる問題です。制約の管理表や盤面が大きくなる場合を想定し、使用集合を外部状態へ置き、Actionには選んだ仕事番号だけを保存します。`run<Action>`の結果は操作列なので、Stateを候補ごとに持たなくても割当表を出力できます。

`expand`は現在の使用集合を読むだけで、`push`の時点では変更しません。`forward`が仕事を追加し、`backward`が同じ仕事を取り消します。`max_turn`は担当者数、`max_turn_is_answer=true`は全員の割当を完成解とする設定です。Hashは使用集合、costは累積費用にします。

正常な`run`返却時、外部状態は初期状態へ戻っています。割当表は`path`の順番から直接出力できます。外部状態を最良解へ移したい場合には、その後でpathを再適用します。この例の状態は小さいため、実際にdeltaを選ぶ基準は、実問題でのコピー費用と取り消し費用です。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_delta_single_v46.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

// 1人分の割当です。Actionはコピー可能にし、undoに必要な情報を持たせます。
// この問題では、未使用の仕事を追加するので仕事番号だけで取り消せます。
struct Action { int job = -1; };

int main() {
    // fee[担当者][仕事]。標準入力なしで動く、小さな割当問題です。
    const std::array<std::array<int, 4>, 4> fee{{
        {{9, 2, 7, 8}}, {{6, 4, 3, 7}},
        {{5, 8, 1, 8}}, {{7, 6, 9, 4}}
    }};
    BS::Param param{};
    param.max_turn = 4; // 1人への割当が1turn。初期turn=0、4人で完成です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = true; // 最大turnへの到達を完成解として扱います。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.use_turn_time_limit = true; // 残り時間を残りturn数に配分します。

    unsigned used = 0; // ライブラリがcallbackを通して探索位置に合わせる外部状態です。
    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (used & (1U << job)) continue;
            const Hash hash = static_cast<Hash>(used | (1U << job));
            // 同turn・同使用集合なら将来条件は同じ。安い割当だけを残せます。
            // pushは候補を登録するだけで、ここでusedを子状態に変えません。
            emit.push(Action{job}, now.cost + fee[now.turn][job], hash);
        }
    };
    auto forward = [&](const Action& action) {
        used |= 1U << action.job;
    };
    auto backward = [&](const Action& action) {
        used &= ~(1U << action.job); // forwardの更新を完全に取り消します。
    };

    // -DLOCALを付けたビルドでのみCSVを記録します。未定義時は空実装です。
    // 第2引数50.0は記録の最小間隔ms。0.0なら対象イベントを毎回記録します。
    BS::CsvStatHook csv("delta_single_assignment.csv", 50.0);
    // 初期Hashは省略でき、0になります。Action列はライブラリが保持・復元します。
    const auto result = BS::run<Action>(
        param, 0LL, expand, forward, backward, csv
    );
    // 正常なrun返却時、usedは初期値0へ戻っています。
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    std::cout << "cost=" << result.best_cost << " jobs=";
    for (const Action& action : result.path) {
        // path[0]は担当者0への割当です。Stateの復元なしでも回答を出せます。
        std::cout << action.job << ' ';
    }
    std::cout << '\n';
}
```

実行例：

```text
cost=13 jobs=1 0 2 3
```

仕事番号は0始まりです。出力の並びは担当者0・1・2・3への割当を表します。

### 例04：移動と巻き戻しを実装し、途中のゴールを返す

盤面を共有しながら目的地へ移動する問題です。最大手数まで歩くこと自体は完成条件ではないため、`max_turn_is_answer=false`にして、ゴールへの遷移だけへ`is_answer=true`を渡します。正常終了後に返るAction列を移動コマンドへ変換します。

現在位置と直前方向を外部状態へ持ちます。座標は逆向きに移動すれば戻せますが、直前方向の上書きは逆算できないので、変更前の方向をActionへ保存します。これは、盤面のセルや資源量を上書きする問題でも同じ考え方です。

途中costは使用手数と残距離の和、ゴールのcostは実手数です。Hashには直前方向も含めます。`is_answer=true`は自動的な展開停止ではないため、ゴール状態からは利用側の`expand`で候補を出さないようにします。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include "beam_delta_single_v46.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

constexpr int HEIGHT = 4, WIDTH = 5;
constexpr int GOAL_ROW = 3, GOAL_COL = 4;
const std::array<std::string_view, HEIGHT> board{
    "S..#.", ".#...", "...#.", "#...G"
};
constexpr std::array<int, 4> dr{-1, 0, 1, 0}, dc{0, 1, 0, -1};
constexpr std::array<char, 4> command{'U', 'R', 'D', 'L'};

// 探索中は残距離も評価に使います。ゴールで0になるので、解のcostは実手数です。
int distance_to_goal(int row, int col) {
    return std::abs(row - GOAL_ROW) + std::abs(col - GOAL_COL);
}
Hash state_hash(int row, int col, int previous) {
    // 直前の逆操作を省くため、位置だけでなく直前方向もHashに含めます。
    // previous=-1は初期状態。小さい状態なので、この整数化には衝突がありません。
    return static_cast<Hash>((row * WIDTH + col) * 5 + previous + 1);
}

// 大きい盤面を扱う場合も、移動で変わる情報だけを外部状態へ持たせます。
struct World { int row = 0, col = 0, previous = -1; };
// 上書きする直前方向を戻すため、変更前の値もActionへ保存します。
struct Action { int direction = 0, old_previous = -1; };

int main() {
    BS::Param param{};
    param.max_turn = 12; // 最大12手。ゴールなら12手より前でも完成です。
    param.beam_width = 64; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。

    World world;
    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        for (int dir = 0; dir < 4; ++dir) {
            if (world.previous >= 0 && dir == (world.previous + 2) % 4) continue;
            const int row = world.row + dr[dir], col = world.col + dc[dir];
            if (row < 0 || row >= HEIGHT || col < 0 || col >= WIDTH) continue;
            if (board[row][col] == '#') continue;
            const bool goal = row == GOAL_ROW && col == GOAL_COL;
            const long long cost = now.turn + 1LL + distance_to_goal(row, col);
            // is_answer=trueで解として保存し、expand_answers=falseにより次のビームへは登録しません。
            emit.push(Action{dir, world.previous}, cost, state_hash(row, col, dir), goal);
        }
    };
    auto forward = [&](const Action& action) {
        world.row += dr[action.direction];
        world.col += dc[action.direction];
        world.previous = action.direction;
    };
    auto backward = [&](const Action& action) {
        world.row -= dr[action.direction];
        world.col -= dc[action.direction];
        world.previous = action.old_previous; // 上書き前の値へ戻します。
    };
    const auto result = BS::run<Action>(
        param, static_cast<long long>(distance_to_goal(0, 0)), state_hash(0, 0, -1),
        expand, forward, backward
    );
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    // pathは初期状態から解へ向かう実行順です。pushの受理可否で解を判定しません。
    std::cout << "steps=" << result.best_turn << " cost=" << result.best_cost << " moves=";
    for (const Action& action : result.path) std::cout << command[action.direction];
    std::cout << '\n';
}
```

実行例：

```text
steps=7 cost=7 moves=RRDRRDD
```

同じ手数の経路が複数ある場合、コマンド列の形は1通りとは限りません。

## 5. beam_copy_multi_v44.hpp

### 例05：時間内に行う作業の集合を選ぶ

所要時間と利益が異なる仕事から、5時間以内に行うものを選びます。各仕事は高々1回で、実行順には意味がありません。作業数ではなく経過時間をturnにし、各遷移の`step`へ所要時間を渡します。回答は使用集合なので`run<State>`で十分です。

`max_step`は1作業の最大所要時間です。時間を使い切らない計画も有効なので`max_turn_is_answer=false`にし、各作業の終了状態へ`is_answer=true`を渡します。それでも、その計画へ仕事を追加する探索は続けられます。costは利益の負値です。

何もしない空集合も有効解ですが、正の`max_turn`の初期状態へ解の印を付ける引数はありません。そこで利用側で利益0の初期解を保持し、探索結果と比較します。Stateは小さいので`push`で直接渡し、同じ到着時間の同じ使用集合をHashで統合します。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_copy_multi_v44.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

// 作業順に意味がなく、選んだ集合が回答です。Stateだけを返す形を使います。
struct State { unsigned used = 0; long long profit = 0; };

int main() {
    const std::array<int, 4> duration{2, 3, 1, 2};
    const std::array<int, 4> reward{6, 8, 2, 5};
    BS::Param param{};
    param.max_turn = 5; // turnは作業数ではなく、消費した時間です。
    param.max_step = 3; // 1作業にかかる時間の最大値です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const int step = duration[job]; // stepは1..max_stepのintです。
            if (step > param.max_turn - now.turn) continue;
            State next = now.state; // このStateは小さいので、通常のpushを使います。
            next.used |= 1U << job;
            next.profit += reward[job];
            // 同じ到着turn・使用集合なら将来の選択肢も利益も同じです。
            // 途中で停止してもよいので、各作業の終了状態を有効解にします。
            emit.push(next, -next.profit, static_cast<Hash>(next.used), step, true);
        }
    };
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand);
    // 正のmax_turnでは初期状態にis_answerを渡せないので、空集合は自分で比較します。
    State answer;
    int hours = 0;
    if (result.found && result.best_cost < 0) {
        answer = *result.best_state;
        hours = result.best_turn;
    }
    std::cout << "profit=" << answer.profit << " hours=" << hours << " jobs=";
    for (int job = 0; job < 4; ++job) if (answer.used & (1U << job)) std::cout << job << ' ';
    std::cout << '\n';
}
```

実行例：

```text
profit=14 hours=5 jobs=0 1
```

出力は選んだ仕事の集合です。実行順による段取り費用がないため、仕事番号順に表示しています。

### 例06：所要時間の異なる道路を選び、経路と時刻を返す

6分以内に目的地へ到達する道路の組合せを、通行料が小さくなるように選びます。`step`は移動時間、costは通行料であり、別の量です。回答に道路の順番が必要なので、`run<State, Action>`で現在都市と道路番号の履歴を分離します。

`max_turn_is_answer=false`とし、目的地へ到着する候補を有効解にします。同じ到着時刻・都市なら以降の選択肢が同じなので、都市番号をHashとして通行料の小さい経路を残します。時間に加えて燃料などが制約になる場合は、その情報もStateとHashへ加えます。

multiの`path`の要素は`Edge<Action>`です。`edge.action`が選んだ道路、`edge.turn`が出発時刻、`edge.next_turn()`が到着時刻です。経路の要素数を経過時間と取り違えないようにします。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_copy_multi_v44.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

// 道路データは不変なのでStateには複製しません。
struct Road { int from, to, minutes, toll; };
struct State { int city = 0; }; // 現在都市だけで次の行動を決められます。
struct Action { int road = -1; }; // 出力したい道路番号を1操作分だけ保存します。

int main() {
    const std::array<Road, 8> roads{{
        {0, 1, 2, 4}, {0, 2, 1, 1}, {1, 3, 2, 1}, {1, 5, 4, 2},
        {2, 3, 3, 2}, {2, 4, 1, 5}, {3, 5, 1, 1}, {4, 5, 2, 1}
    }};
    constexpr int goal_city = 5;
    BS::Param param{};
    param.max_turn = 6; // 6分以内。到着が6分ちょうどである必要はありません。
    param.max_step = 4; // 1本の道路の所要時間の最大値です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State, Action>& emit) {
        for (int id = 0; id < static_cast<int>(roads.size()); ++id) {
            const Road& road = roads[id];
            if (road.from != now.state.city) continue;
            if (road.minutes > param.max_turn - now.turn) continue;
            const bool goal = road.to == goal_city;
            // 同じ到着時刻・都市なら将来条件が同じ。安い通行料の経路を残します。
            // costは通行料、stepは所要時間であり、両者は別の量です。
            emit.push(State{road.to}, Action{id}, now.cost + road.toll,
                      static_cast<Hash>(road.to), road.minutes, goal);
        }
    };
    const auto result = BS::run<State, Action>(param, State{}, 0LL, Hash{0}, expand);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    std::cout << "toll=" << result.best_cost << " arrival=" << result.best_turn << " roads=";
    // multiのpathはEdge<Action>列です。経路長と経過turn数は一致しません。
    for (const auto& edge : result.path) std::cout << edge.action.road << ' ';
    std::cout << '\n';
    for (const auto& edge : result.path) {
        const Road& road = roads[edge.action.road];
        // Edgeには出発turn・stepがあり、next_turn()で到着turnを取れます。
        std::cout << road.from << " -> " << road.to
                  << " depart=" << edge.turn << " arrive=" << edge.next_turn() << '\n';
    }
}
```

実行例：

```text
toll=4 arrival=5 roads=1 4 6
0 -> 2 depart=0 arrive=1
2 -> 3 depart=1 arrive=4
3 -> 5 depart=4 arrive=5
```

5分目に到着した有効解です。時間上限の6分まで、待機操作を追加する必要はありません。

## 6. beam_delta_multi_v69.hpp

### 例07：時刻に依存する生産をEdge付きcallbackで扱う

1〜3時間かかる生産方式を選び、6時間分の計画を作ります。早く完成した製品の単価が高いため、状態の更新に到着時刻を使います。外部状態へ経過時間と利益を置き、移動callbackは`const BS::Edge<Action>&`を受け取ります。

Actionは生産方式の番号だけを持ち、出発時刻と所要時間はEdgeが持ちます。`forward`では`edge.next_turn()`で売上を計算し、`backward`では同じ計算を引いて`edge.turn`へ戻します。時刻依存の資源回復や設備稼働を差分更新する場合にも使える形です。

`max_turn_is_answer=true`で6時間ちょうどの計画を完成解にします。費用関数や差分適用が重い実問題では、評価用の計算を共通化するか、必要な差分値をActionへ保存して取り消しを軽くします。この例はHashを指定せず、幅で候補を選びます。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_delta_multi_v69.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

// 時刻はEdgeが持つので、Actionは実行する生産方式の番号だけを持ちます。
struct Action { int recipe = -1; };

int main() {
    const std::array<int, 3> duration{1, 2, 3}, quantity{1, 5, 8};
    auto income = [&](int recipe, int finish_time) {
        // 3時間目までに完了した製品は単価2、それ以降は単価1です。
        return static_cast<long long>(quantity[recipe]) * (finish_time <= 3 ? 2 : 1);
    };
    int hours = 0;
    long long profit = 0; // この2変数が探索位置に対応した外部状態です。
    BS::Param param{};
    param.max_turn = 6; // 6時間を使い切る生産計画です。
    param.max_step = 3; // 生産方式により1回で1〜3時間進みます。
    param.beam_width = 32; // 同じ到着時刻に残す候補数です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = true; // 6時間に達した計画を完成解にします。
    param.use_hash_dedup = false; // この例はHashを使わず、幅だけで候補を選びます。
    param.time_limit_ms = 1000.0; // 探索全体の時間予算です。
    param.use_turn_time_limit = true; // 残り時間を残りturn数で配分します。
    param.time_check_interval = 64; // expand自体を途中で中断する指定ではありません。

    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        for (int recipe = 0; recipe < 3; ++recipe) {
            const int step = duration[recipe];
            if (step > param.max_turn - now.turn) continue;
            // hoursはcallbackがnow.turnと一致するように復元しています。
            const long long next_profit = profit + income(recipe, hours + step);
            emit.push(Action{recipe}, -next_profit, step);
        }
    };
    // callbackがEdgeを受け取ると、Actionだけでなく出発・到着時刻も使えます。
    auto forward = [&](const BS::Edge<Action>& edge) {
        profit += income(edge.action.recipe, edge.next_turn());
        hours = edge.next_turn();
    };
    auto backward = [&](const BS::Edge<Action>& edge) {
        profit -= income(edge.action.recipe, edge.next_turn());
        hours = edge.turn; // 更新前の時刻はEdgeに保存されています。
    };
    // この例ではHashを使わないため、初期Hashを省略します。
    const auto result = BS::run<Action>(param, 0LL, expand, forward, backward);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    // runから正常に戻ると初期状態です。解の状態が必要ならpathを再適用します。
    for (const auto& edge : result.path) forward(edge);
    std::cout << "profit=" << profit << " hours=" << hours << " recipes=";
    for (const auto& edge : result.path) std::cout << edge.action.recipe << ' ';
    std::cout << '\n';
}
```

実行例：

```text
profit=24 hours=6 recipes=2 2
```

生産方式2を2回使い、3時間目までの高単価と、その後の生産収入を合わせています。

### 例08：時刻を外部状態に持たず、Actionだけで作業を取り消す

5時間以内に実行する仕事の利益を最大にします。時間はライブラリのturnに任せ、外部状態は使用集合と利益だけにすると、callbackは`const Action&`で足ります。時刻に依存しない更新へEdgeを持ち込む必要はありません。

`push`へHash・`int`のstep・`is_answer=true`を渡します。各作業の終了時点で停止してよいので、最大turnより前の解も保存します。利益0の空集合は利用側で保持し、改善する結果があれば採用します。

callbackがAction型でも、返る`path`は`Edge<Action>`列です。外部状態の再生には`edge.action`を渡し、所要時間は`best_turn`やEdgeの時刻情報で確認します。正常終了時の外部状態への復元を前提に、返った経路を初期状態から再適用します。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_delta_multi_v69.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

// 仕事番号から追加・取消の両方を計算できます。出力順もAction列に残ります。
struct Action { int job = -1; };

int main() {
    const std::array<int, 4> duration{2, 3, 1, 2};
    const std::array<int, 4> reward{6, 8, 2, 5};
    BS::Param param{};
    param.max_turn = 5; // turnは作業数ではなく、消費した時間です。
    param.max_step = 3; // 1作業にかかる時間の最大値です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。

    unsigned used = 0;
    long long profit = 0;
    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (used & (1U << job)) continue;
            const int step = duration[job]; // stepはboolやlong longでなくintで渡します。
            if (step > param.max_turn - now.turn) continue;
            const Hash hash = static_cast<Hash>(used | (1U << job));
            // 仕事を終えるたびに停止できるので、どの到着状態も有効解です。
            // is_answer=trueでも、そこへ仕事を追加する探索は続けられます。
            emit.push(Action{job}, -(profit + reward[job]), hash, step, true);
        }
    };
    // 時刻は外部状態へ持たないので、callbackはEdgeでなくActionを受け取ります。
    auto forward = [&](const Action& action) {
        used |= 1U << action.job;
        profit += reward[action.job];
    };
    auto backward = [&](const Action& action) {
        used &= ~(1U << action.job);
        profit -= reward[action.job];
    };
    const auto result = BS::run<Action>(param, 0LL, expand, forward, backward);
    // runは初期状態へ戻します。空の作業集合は利用側の初期解として保持します。
    int hours = 0;
    if (result.found && result.best_cost < 0) {
        // callbackがAction型でも、multiのpathの要素はEdge<Action>です。
        for (const auto& edge : result.path) forward(edge.action);
        hours = result.best_turn;
    }
    std::cout << "profit=" << profit << " hours=" << hours << " jobs=";
    for (int job = 0; job < 4; ++job) if (used & (1U << job)) std::cout << job << ' ';
    std::cout << '\n';
}
```

実行例：

```text
profit=14 hours=5 jobs=0 1
```

出力は選んだ仕事の集合です。実行順による段取り費用がないため、仕事番号順に表示しています。

## 7. chokudai_copy_single_v13.hpp

### 例09：割当の未展開候補を残し、時間内に別案も探索する

担当者へ仕事を1件ずつ割り当て、最終的な割当表を返します。途中の評価で優先しなかった割当も未展開候補として残し、完成解を得た後も時間があれば別の経路を探索したい場合にchokudaiを使います。結果の利用は`run<State>`と`best_state`です。

`beam_width`は各turnに保持する候補数、`sweep_width`は1スイープで取り出す数です。この例では32件を保持し、2件ずつ取り出します。`max_sweeps=0`で回数制限は設けず、全体1,000msまたは候補切れで通常探索を終えます。`continue_on_timeout=true`は解未発見時の追加探索を許す設定です。

Hashと遅延コピーの使い方は通常copy singleと同じです。展開済み状態を永久に記録する方式ではないため、同じ状態が後から再生成されることがあります。スイープ数や解の更新はCSVで分析でき、CSVのためだけに探索の仕方を変える必要はありません。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_single_v13.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = cs::ChokudaiCopySingle<long long, Hash>;

// 最終的な割当表そのものが回答なので、操作履歴を別に持つ必要はありません。
// Stateはコピー可能な値型にし、探索中に共有の配列を書き換えないようにします。
struct State {
    unsigned used = 0;
    std::array<int, 4> job_for_worker{{-1, -1, -1, -1}};
};

int main() {
    // fee[担当者][仕事]。標準入力なしで動く、小さな割当問題です。
    const std::array<std::array<int, 4>, 4> fee{{
        {{9, 2, 7, 8}}, {{6, 4, 3, 7}},
        {{5, 8, 1, 8}}, {{7, 6, 9, 4}}
    }};
    BS::Param param{};
    param.max_turn = 4; // 1人への割当が1turn。初期turn=0、4人で完成です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = true; // 最大turnへの到達を完成解として扱います。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sweep_width = 2; // 各turnから1スイープで取り出す候補数です。
    param.max_sweeps = 0; // 回数では制限せず、時間または候補切れまで探索します。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const unsigned used = now.state.used | (1U << job);
            const long long cost = now.cost + fee[now.turn][job];
            // 同turn・同使用集合なら、安い方を残してよい問題です。
            // 割当表が大きくなる場合に備え、必要な候補だけStateをコピーします。
            // makerはこのpush_lazy中に高々1回呼ばれ、後で呼ばれることはありません。
            emit.push_lazy(cost, static_cast<Hash>(used), [&now, job, used] {
                State next = now.state;
                next.used = used;
                next.job_for_worker[now.turn] = job;
                return next;
            });
        }
    };

    // -DLOCALを付けたビルドでのみCSVを記録します。未定義時は空実装です。
    // 第2引数50.0は記録の最小間隔ms。0.0なら対象イベントを毎回記録します。
    BS::CsvStatHook csv("chokudai_single_assignment.csv", 50.0);
    // run<State>はStateだけを保存します。初期costは0、初期使用集合も0です。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) { // best_stateを参照する前に、有効解の有無を確認します。
        std::cout << "no solution\n";
        return 0;
    }
    std::cout << "cost=" << result.best_cost << " jobs=";
    for (int job : result.best_state->job_for_worker) std::cout << job << ' ';
    std::cout << '\n';
}
```

実行例：

```text
cost=13 jobs=1 0 2 3
```

仕事番号は0始まりです。出力の並びは担当者0・1・2・3への割当を表します。

### 例10：未展開経路を残しながらゴールへの操作列を探す

静的な迷路で目的地へ移動し、コマンド列を返します。途中評価だけでは良い経路を選び切れない問題を想定し、浅いturnに残った未展開候補も後のスイープで試せる形にします。Stateは位置、Actionは移動方向です。

`run<State, Action>`で操作列を保持し、`max_turn_is_answer=false`と`is_answer=true`で途中のゴールを完成条件にします。`sweep_width`は保持幅ではなく1回の取出し量です。時間制限があっても、保存済みの最良有効解は`found`と`path`から受け取れます。

`emit.history()`で直前の移動を参照して往復を省き、その方向をHashにも含めます。参照は当該expand内だけで扱い、`push`をまたぐAction参照は保持しません。この小さな迷路には厳密な最短路法も使えますが、コードは追加の盤面条件を持つ経路問題へ組み替えやすい形です。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include "chokudai_copy_single_v13.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = cs::ChokudaiCopySingle<long long, Hash>;

constexpr int HEIGHT = 4, WIDTH = 5;
constexpr int GOAL_ROW = 3, GOAL_COL = 4;
const std::array<std::string_view, HEIGHT> board{
    "S..#.", ".#...", "...#.", "#...G"
};
constexpr std::array<int, 4> dr{-1, 0, 1, 0}, dc{0, 1, 0, -1};
constexpr std::array<char, 4> command{'U', 'R', 'D', 'L'};

// 探索中は残距離も評価に使います。ゴールで0になるので、解のcostは実手数です。
int distance_to_goal(int row, int col) {
    return std::abs(row - GOAL_ROW) + std::abs(col - GOAL_COL);
}
Hash state_hash(int row, int col, int previous) {
    // 直前の逆操作を省くため、位置だけでなく直前方向もHashに含めます。
    // previous=-1は初期状態。小さい状態なので、この整数化には衝突がありません。
    return static_cast<Hash>((row * WIDTH + col) * 5 + previous + 1);
}

// Stateは現在位置だけ。出力用の文字列を毎回コピーしません。
struct State { int row = 0, col = 0; };
// Actionは1回分の操作です。ライブラリが共有木で履歴を保存します。
struct Action { int direction = 0; };

int main() {
    BS::Param param{};
    param.max_turn = 12; // 最大12手。ゴールなら12手より前でも完成です。
    param.beam_width = 64; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sweep_width = 2; // 各turnから1スイープで取り出す候補数です。
    param.max_sweeps = 0; // 回数では制限せず、時間または候補切れまで探索します。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State, Action>& emit) {
        // 1手前のActionを参照します。履歴全体のコピーや走査は不要です。
        const auto history = emit.history();
        int previous = -1;
        if (!history.empty()) {
            const Action& last = history.action();
            previous = last.direction; // 参照が無効化され得るpushより前に読み取ります。
        }
        for (int dir = 0; dir < 4; ++dir) {
            // この静的な迷路では、直後の往復は最短路に不要です。
            if (previous >= 0 && dir == (previous + 2) % 4) continue;
            const int row = now.state.row + dr[dir], col = now.state.col + dc[dir];
            if (row < 0 || row >= HEIGHT || col < 0 || col >= WIDTH) continue;
            if (board[row][col] == '#') continue;
            const bool goal = row == GOAL_ROW && col == GOAL_COL;
            const long long cost = now.turn + 1LL + distance_to_goal(row, col);
            // 履歴ありpushはStateの直後にActionを渡します。
            emit.push(State{row, col}, Action{dir}, cost, state_hash(row, col, dir), goal);
        }
    };
    // 第2テンプレート引数Actionを指定すると、最終Stateと操作列を両方受け取れます。
    const auto result = BS::run<State, Action>(
        param, State{}, static_cast<long long>(distance_to_goal(0, 0)),
        state_hash(0, 0, -1), expand
    );
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    // pathは初期状態から解へ向かう実行順です。pushの受理可否で解を判定しません。
    std::cout << "steps=" << result.best_turn << " cost=" << result.best_cost << " moves=";
    for (const Action& action : result.path) std::cout << command[action.direction];
    std::cout << '\n';
}
```

実行例：

```text
steps=7 cost=7 moves=RRDRRDD
```

同じ手数の経路が複数ある場合、コマンド列の形は1通りとは限りません。

## 8. chokudai_copy_multi_v15.hpp

### 例11：時間内の作業選択を複数スイープで改善する

所要時間の異なる仕事を選び、5時間以内の利益を最大にします。各作業後に停止してよいので、その状態を有効解として比較しつつ、残り時間へ別の仕事を追加します。回答は作業集合であり、Action履歴を持たない`run<State>`を使います。

`max_step`と各pushのstepが経過時間を進め、`sweep_width`は同じ到着turnのキューから少しずつ候補を展開します。`max_turn_is_answer=false`と各候補の`is_answer=true`により、時間を使い切らない高利益の計画も残ります。

`max_sweeps=0`で全体時間を主な終了条件にし、解なしの時間切れには`continue_on_timeout`の追加探索を許可します。利益0の空集合は利用側で確保します。Hashは使用集合ですが、仕事順に依存する段取り費用などを追加する場合には、最後の仕事などもHashへ含めます。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_multi_v15.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = cs::ChokudaiCopyMulti<long long, Hash>;

// 作業順に意味がなく、選んだ集合が回答です。Stateだけを返す形を使います。
struct State { unsigned used = 0; long long profit = 0; };

int main() {
    const std::array<int, 4> duration{2, 3, 1, 2};
    const std::array<int, 4> reward{6, 8, 2, 5};
    BS::Param param{};
    param.max_turn = 5; // turnは作業数ではなく、消費した時間です。
    param.max_step = 3; // 1作業にかかる時間の最大値です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sweep_width = 2; // 各turnから1スイープで取り出す候補数です。
    param.max_sweeps = 0; // 回数では制限せず、時間または候補切れまで探索します。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const int step = duration[job]; // stepは1..max_stepのintです。
            if (step > param.max_turn - now.turn) continue;
            State next = now.state; // このStateは小さいので、通常のpushを使います。
            next.used |= 1U << job;
            next.profit += reward[job];
            // 同じ到着turn・使用集合なら将来の選択肢も利益も同じです。
            // 途中で停止してもよいので、各作業の終了状態を有効解にします。
            emit.push(next, -next.profit, static_cast<Hash>(next.used), step, true);
        }
    };
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand);
    // 正のmax_turnでは初期状態にis_answerを渡せないので、空集合は自分で比較します。
    State answer;
    int hours = 0;
    if (result.found && result.best_cost < 0) {
        answer = *result.best_state;
        hours = result.best_turn;
    }
    std::cout << "profit=" << answer.profit << " hours=" << hours << " jobs=";
    for (int job = 0; job < 4; ++job) if (answer.used & (1U << job)) std::cout << job << ' ';
    std::cout << '\n';
}
```

実行例：

```text
profit=14 hours=5 jobs=0 1
```

出力は選んだ仕事の集合です。実行順による段取り費用がないため、仕事番号順に表示しています。

### 例12：所要時間の異なる経路を探索し、Edge列を返す

6分以内で目的地へ到達する経路を、通行料で評価します。途中の都市に残した未展開候補を後から試し、完成経路を改善するためにchokudai multiを使います。Stateは現在都市だけとし、道路番号はAction履歴へ保存します。

`run<State, Action>`と`Emitter<State, Action>`を組み合わせます。各pushでは所要時間をstepへ渡し、目的地への到着だけを有効解とします。`beam_width`は保持数、`sweep_width`は各turnでの1回の展開量です。

最良経路は`Edge<Action>`列です。Actionの道路番号と、Edgeの出発・到着turnを使えば実行順と時刻を出力できます。都市番号だけのHashが使えるのは、この例では同じ到着時刻・都市からの将来条件が等しいためです。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_multi_v15.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = cs::ChokudaiCopyMulti<long long, Hash>;

// 道路データは不変なのでStateには複製しません。
struct Road { int from, to, minutes, toll; };
struct State { int city = 0; }; // 現在都市だけで次の行動を決められます。
struct Action { int road = -1; }; // 出力したい道路番号を1操作分だけ保存します。

int main() {
    const std::array<Road, 8> roads{{
        {0, 1, 2, 4}, {0, 2, 1, 1}, {1, 3, 2, 1}, {1, 5, 4, 2},
        {2, 3, 3, 2}, {2, 4, 1, 5}, {3, 5, 1, 1}, {4, 5, 2, 1}
    }};
    constexpr int goal_city = 5;
    BS::Param param{};
    param.max_turn = 6; // 6分以内。到着が6分ちょうどである必要はありません。
    param.max_step = 4; // 1本の道路の所要時間の最大値です。
    param.beam_width = 32; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sweep_width = 2; // 各turnから1スイープで取り出す候補数です。
    param.max_sweeps = 0; // 回数では制限せず、時間または候補切れまで探索します。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State, Action>& emit) {
        for (int id = 0; id < static_cast<int>(roads.size()); ++id) {
            const Road& road = roads[id];
            if (road.from != now.state.city) continue;
            if (road.minutes > param.max_turn - now.turn) continue;
            const bool goal = road.to == goal_city;
            // 同じ到着時刻・都市なら将来条件が同じ。安い通行料の経路を残します。
            // costは通行料、stepは所要時間であり、両者は別の量です。
            emit.push(State{road.to}, Action{id}, now.cost + road.toll,
                      static_cast<Hash>(road.to), road.minutes, goal);
        }
    };
    const auto result = BS::run<State, Action>(param, State{}, 0LL, Hash{0}, expand);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    std::cout << "toll=" << result.best_cost << " arrival=" << result.best_turn << " roads=";
    // multiのpathはEdge<Action>列です。経路長と経過turn数は一致しません。
    for (const auto& edge : result.path) std::cout << edge.action.road << ' ';
    std::cout << '\n';
    for (const auto& edge : result.path) {
        const Road& road = roads[edge.action.road];
        // Edgeには出発turn・stepがあり、next_turn()で到着turnを取れます。
        std::cout << road.from << " -> " << road.to
                  << " depart=" << edge.turn << " arrive=" << edge.next_turn() << '\n';
    }
}
```

実行例：

```text
toll=4 arrival=5 roads=1 4 6
0 -> 2 depart=0 arrive=1
2 -> 3 depart=1 arrive=4
3 -> 5 depart=4 arrive=5
```

5分目に到着した有効解です。時間上限の6分まで、待機操作を追加する必要はありません。

## 9. beam_copy_sub_v12.hpp

### 例13：各期の前処理回数を選び、完成した計画表を返す

3期にわたり生産します。各期の生産前に設備を何回増強するかを選び、増強費用を引いた利益を最大にします。増強1回は能力を1上げて費用6、生産1回の収入は能力の5倍です。資金の不足制約はなく、能力の上限は4です。

増強を`Next::Sub`、生産を`Next::Turn`にし、生産を終えた期数をそろえて比較します。`sub_beam_width`は期の中、`beam_width`は期の境界の候補数です。能力上限があるため、どの経路のSub操作も有限です。

回答は各期の増強回数の表なので、それをStateへ持たせて`best_state`から返します。操作列の追加保存は不要です。`push_lazy`のmakerは計画表のコピーを保存が必要なときまで遅らせます。同じ能力なら将来条件が等しいので、利益が大きい計画をHashで残せます。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_copy_sub_v12.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

constexpr int PERIODS = 3, MAX_LEVEL = 4;
constexpr long long UPGRADE_COST = 6, INCOME_PER_LEVEL = 5;

// 回答は各期の増強回数の表です。操作順は「増強してから生産」で固定です。
// これは最終計画のデータなので、Action履歴を追加しなくても出力できます。
struct State {
    int level = 1;
    long long profit = 0;
    std::array<int, PERIODS> upgrades{};
};

int main() {
    BS::Param param{};
    param.max_turn = PERIODS; // 1期の生産を終えるたびにturnが進みます。
    param.beam_width = 16; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = true; // 最大turnへの到達を完成解として扱います。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.use_turn_time_limit = true; // 残り時間を残りturn数に配分します。
    param.sub_beam_width = 8; // 同turnの次sub層へ残す候補数です。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        const long long profit = now.state.profit + INCOME_PER_LEVEL * now.state.level;
        // makerは保存が必要なときだけ実行されます。末尾のNextが保存先を決めます。
        emit.push_lazy(-profit, static_cast<Hash>(now.state.level), false,
            [&now, profit] {
                State next = now.state;
                next.profit = profit;
                return next;
            }, BS::Next::Turn);
        if (now.state.level < MAX_LEVEL) {
            const int level = now.state.level + 1;
            const long long next_profit = now.state.profit - UPGRADE_COST;
            emit.push_lazy(-next_profit, static_cast<Hash>(level), false,
                [&now, level, next_profit] {
                    State next = now.state;
                    next.level = level;
                    next.profit = next_profit;
                    ++next.upgrades[now.turn];
                    return next;
                }, BS::Next::Sub);
        }
        // level上限により、どの経路のSub遷移も有限です。
        // 同じ保存先・levelなら将来条件は同じで、利益が大きい計画を残せます。
    };

    // -DLOCALを付けたビルドでのみCSVを記録します。未定義時は空実装です。
    // 第2引数50.0は記録の最小間隔ms。0.0なら対象イベントを毎回記録します。
    BS::CsvStatHook csv("copy_sub_production.csv", 50.0);
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{1}, expand, csv);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    const State& answer = *result.best_state;
    std::cout << "profit=" << answer.profit << " level=" << answer.level << " upgrades=";
    for (int count : answer.upgrades) std::cout << count << ' ';
    std::cout << '\n';
}
```

実行例：

```text
profit=42 level=4 upgrades=3 0 0
```

最初の期に3回増強し、その後は生産だけを行う計画です。

### 例14：1日内に複数件を配達し、途中で完成した操作列を返す

1日の燃料が8の車で4件を配達します。1日内の配達件数は経路ごとに異なり、車庫へ戻って給油すると次の日へ進みます。最大3日で、最後の配達が済めばその地点で終了できます。目的は総移動距離の最小化です。

配達を`Next::Sub`、帰庫・給油を`Next::Turn`にします。Stateは配達済み集合・現在地・残燃料、Actionは配達先または日終了の指示です。`run<State, Action>`により、状態コピーへ長い出力履歴を含めずにすみます。Subごとに未配達件数が減るので有限です。

最大日数への到達だけでは完成でないため`max_turn_is_answer=false`にし、全配達を終えたSub候補を有効解にします。`best_turn`は完了した日数で、最後の配達日はその次の日です。`turn_offsets.back()`より後の未完了日の操作も出力する点が重要です。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include "beam_copy_sub_v12.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

constexpr int CUSTOMERS = 4, FUEL = 8;
constexpr unsigned ALL_DONE = (1U << CUSTOMERS) - 1;
// 0は車庫、1〜4は配達先です。1次元上の距離を移動費用・燃料消費とします。
constexpr std::array<int, CUSTOMERS + 1> position{0, -3, -1, 2, 4};
struct State { unsigned done = 0; int at = 0, fuel = FUEL; };

Hash state_hash(const State& state) {
    // 同じ保存先での将来条件は配達済み集合・現在地・燃料で決まります。
    // 途中の訪問順は将来条件に影響せず、Hashへ履歴全体を入れる必要はありません。
    return static_cast<Hash>((state.done * (CUSTOMERS + 1) + state.at) * (FUEL + 1) + state.fuel);
}

// Stateへ操作列を入れず、1操作分の出力情報をActionとして渡します。
struct Action { int to = 0; bool end_day = false; };

int main() {
    BS::Param param{};
    param.max_turn = 3; // turn=0,1,2が1〜3日目です。3日目までに配達します。
    param.beam_width = 64; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sub_beam_width = 64; // 同turnの次sub層へ残す候補数です。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State, Action>& emit) {
        for (int to = 1; to <= CUSTOMERS; ++to) {
            const unsigned bit = 1U << (to - 1);
            if (now.state.done & bit) continue;
            const int distance = std::abs(position[to] - position[now.state.at]);
            if (distance > now.state.fuel) continue;
            const State next{now.state.done | bit, to, now.state.fuel - distance};
            // 履歴ありpushはStateの直後にActionを置き、Nextは最後に指定します。
            emit.push(next, Action{to, false}, now.cost + distance, state_hash(next),
                      next.done == ALL_DONE, BS::Next::Sub);
        }
        const int distance = std::abs(position[now.state.at]);
        if (now.turn + 1 < param.max_turn && now.state.at != 0 && distance <= now.state.fuel) {
            const State next{now.state.done, 0, FUEL};
            // 帰庫・給油は日を確定する操作です。配達回数に関係なく次turnへ進みます。
            emit.push(next, Action{0, true}, now.cost + distance, state_hash(next),
                      false, BS::Next::Turn);
        }
        // Subごとに未配達件数が減り、Turnは最大日数に制限されるので有限です。
    };
    const auto result = BS::run<State, Action>(param, State{}, 0LL, state_hash(State{}), expand);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    std::cout << "distance=" << result.best_cost << " completed_days=" << result.best_turn << '\n';
    auto print_range = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const Action& action = result.path[i];
            if (action.end_day) std::cout << " return";
            else std::cout << " deliver:" << action.to;
        }
        std::cout << '\n';
    };
    for (int day = 0; day < result.best_turn; ++day) {
        std::cout << "day=" << day + 1;
        print_range(result.turn_offsets[day], result.turn_offsets[day + 1]);
    }
    // 最後の配達はSubなので、その日のTurn境界はまだありません。
    // turn_offsets.back()より後のActionも、出力する解の一部です。
    if (result.turn_offsets.back() < result.path.size()) {
        std::cout << "day=" << result.best_turn + 1;
        print_range(result.turn_offsets.back(), result.path.size());
    }
}
```

実行例：

```text
distance=10 completed_days=1
day=1 deliver:1 deliver:2 return
day=2 deliver:3 deliver:4
```

1日目は帰庫まで完了し、2日目は最後の配達直後に完成しています。最後の日の操作もpathに含まれます。

## 10. beam_delta_sub_v12.hpp

### 例15：期内の操作を差分適用し、期ごとのAction区間を返す

3期の生産計画で、各期の前に行う設備増強の回数を選びます。増強は費用6で能力を1上げ、生産は能力の5倍の収入を得ます。能力上限は4です。外部状態に能力と累計利益を持ち、Actionへ更新の種類と利益差分を保存します。

`Next::Sub`は同じ期の続き、`Next::Turn`は生産して次の期へ進む操作です。`forward`と`backward`で差分を適用・取消します。能力上限によりSub探索が有限になることを利用側で保証します。

`path`は増強も生産も含むAction列です。各期の操作を出力するときは、`turn_offsets[t]`から`turn_offsets[t+1]`の区間を使います。この例は最大turnで完成するため、最後のoffsetはpath末尾です。copy subで計画表を返す形と、delta subで実行操作を返す形の違いを示します。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "beam_delta_sub_v12.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

constexpr int PERIODS = 3, MAX_LEVEL = 4;
constexpr long long UPGRADE_COST = 6, INCOME_PER_LEVEL = 5;

// upgradeなら能力を1上げます。利益差分を保存すると逆操作が簡単になります。
struct Action { bool upgrade = false; long long profit_delta = 0; };

int main() {
    BS::Param param{};
    param.max_turn = PERIODS; // 1期の生産を終えるたびにturnが進みます。
    param.beam_width = 16; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = true; // 最大turnへの到達を完成解として扱います。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.use_turn_time_limit = true; // 残り時間を残りturn数に配分します。
    param.sub_beam_width = 8; // 同turnの次sub層へ残す候補数です。

    int level = 1;
    long long profit = 0;
    auto expand = [&](const BS::NodeView&, BS::Emitter<Action>& emit) {
        // 生産は期を確定する操作です。増強回数の違う経路も次turnで比較されます。
        const long long income = INCOME_PER_LEVEL * level;
        emit.push(Action{false, income}, -(profit + income),
                  static_cast<Hash>(level), BS::Next::Turn);
        if (level < MAX_LEVEL) {
            // 増強は同turn内の操作です。level上限によりSubの連鎖は必ず有限です。
            emit.push(Action{true, -UPGRADE_COST}, -(profit - UPGRADE_COST),
                      static_cast<Hash>(level + 1), BS::Next::Sub);
        }
    };
    auto forward = [&](const Action& action) {
        if (action.upgrade) ++level;
        profit += action.profit_delta;
    };
    auto backward = [&](const Action& action) {
        profit -= action.profit_delta;
        if (action.upgrade) --level;
    };

    // -DLOCALを付けたビルドでのみCSVを記録します。未定義時は空実装です。
    // 第2引数50.0は記録の最小間隔ms。0.0なら対象イベントを毎回記録します。
    BS::CsvStatHook csv("delta_sub_production.csv", 50.0);
    // 初期能力1をHashにします。将来条件が同じなら利益の大きい計画を残せます。
    const auto result = BS::run<Action>(param, 0LL, Hash{1}, expand, forward, backward, csv);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    // 外部状態は初期状態に戻っています。以下で解の操作を再生します。
    for (const Action& action : result.path) forward(action);
    std::cout << "profit=" << profit << " level=" << level << '\n';
    for (int period = 0; period < result.best_turn; ++period) {
        // 完了した期の操作範囲は、隣接するturn_offsetsで切り出せます。
        std::cout << "period=" << period + 1 << " commands=";
        for (std::size_t i = result.turn_offsets[period];
             i < result.turn_offsets[period + 1]; ++i) {
            std::cout << (result.path[i].upgrade ? 'U' : 'P');
        }
        std::cout << '\n';
    }
}
```

実行例：

```text
profit=42 level=4
period=1 commands=UUUP
period=2 commands=P
period=3 commands=P
```

最初の期に3回増強し、その後は生産だけを行う計画です。 Uは増強、Pは生産です。

### 例16：配達状態を巻き戻し、未完了日の末尾まで再生する

最大3日で4件の配達を終える問題です。1日内で配達を繰り返し、帰庫・給油で日を進めます。最後の配達後の帰庫は不要です。外部状態へ配達済み集合・現在地・燃料を置き、1操作ごとの差分をActionとして保存します。

現在地と燃料は上書きするため、変更前の値をActionへ持たせます。完了集合は未配達先の1bitを追加・取消します。`max_turn_is_answer=false`とし、全配達完了をSub遷移の`is_answer=true`で知らせます。Hashには現在地・燃料・配達済み集合を含めます。

正常終了後、外部状態は初期状態です。最終状態を作るにはpath全体を再生します。最後の配達日はTurn操作で締められていないため、`turn_offsets.back()`を再生や出力の終端にしてはいけません。完了日の区間に続けて、path末尾まで出力します。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include "beam_delta_sub_v12.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = bs::Beam<long long, Hash>;

constexpr int CUSTOMERS = 4, FUEL = 8;
constexpr unsigned ALL_DONE = (1U << CUSTOMERS) - 1;
// 0は車庫、1〜4は配達先です。1次元上の距離を移動費用・燃料消費とします。
constexpr std::array<int, CUSTOMERS + 1> position{0, -3, -1, 2, 4};
struct State { unsigned done = 0; int at = 0, fuel = FUEL; };

Hash state_hash(const State& state) {
    // 同じ保存先での将来条件は配達済み集合・現在地・燃料で決まります。
    // 途中の訪問順は将来条件に影響せず、Hashへ履歴全体を入れる必要はありません。
    return static_cast<Hash>((state.done * (CUSTOMERS + 1) + state.at) * (FUEL + 1) + state.fuel);
}

// 変更前の現在地・燃料を保存します。完了集合は未訪問先の1bitだけを変えます。
struct Action {
    int from = 0, to = 0, old_fuel = FUEL, distance = 0;
    bool end_day = false;
};

int main() {
    BS::Param param{};
    param.max_turn = 3; // turn=0,1,2が1〜3日目です。3日目までに配達します。
    param.beam_width = 64; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sub_beam_width = 64; // 同turnの次sub層へ残す候補数です。

    State world;
    long long traveled = 0;
    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        for (int to = 1; to <= CUSTOMERS; ++to) {
            const unsigned bit = 1U << (to - 1);
            if (world.done & bit) continue;
            const int distance = std::abs(position[to] - position[world.at]);
            if (distance > world.fuel) continue;
            const State next{world.done | bit, to, world.fuel - distance};
            const Action action{world.at, to, world.fuel, distance, false};
            // 配達は同じ日のSub操作です。全件完了したSub状態を解にします。
            emit.push(action, traveled + distance, state_hash(next),
                      next.done == ALL_DONE, BS::Next::Sub);
        }
        const int distance = std::abs(position[world.at]);
        if (now.turn + 1 < param.max_turn && world.at != 0 && distance <= world.fuel) {
            const State next{world.done, 0, FUEL};
            // 車庫へ戻って給油すると、次の日へ進みます。
            emit.push(Action{world.at, 0, world.fuel, distance, true},
                      traveled + distance, state_hash(next), false, BS::Next::Turn);
        }
        // Subごとに未配達件数が減り、Turnは最大日数に制限されるので有限です。
    };
    auto forward = [&](const Action& action) {
        world.at = action.to;
        world.fuel = action.end_day ? FUEL : action.old_fuel - action.distance;
        if (!action.end_day) world.done |= 1U << (action.to - 1);
        traveled += action.distance;
    };
    auto backward = [&](const Action& action) {
        if (!action.end_day) world.done &= ~(1U << (action.to - 1));
        world.at = action.from;
        world.fuel = action.old_fuel;
        traveled -= action.distance;
    };
    const auto result = BS::run<Action>(
        param, 0LL, state_hash(world), expand, forward, backward
    );
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    // 外部状態は初期状態へ戻っています。未完了日の末尾も含め、path全体を再生します。
    for (const Action& action : result.path) forward(action);
    std::cout << "distance=" << traveled << " completed_days=" << result.best_turn << '\n';
    auto print_range = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const Action& action = result.path[i];
            if (action.end_day) std::cout << " return";
            else std::cout << " deliver:" << action.to;
        }
        std::cout << '\n';
    };
    for (int day = 0; day < result.best_turn; ++day) {
        std::cout << "day=" << day + 1;
        print_range(result.turn_offsets[day], result.turn_offsets[day + 1]);
    }
    // 最後の配達はSubなので、その日のTurn境界はまだありません。
    // turn_offsets.back()より後のActionも、出力する解の一部です。
    if (result.turn_offsets.back() < result.path.size()) {
        std::cout << "day=" << result.best_turn + 1;
        print_range(result.turn_offsets.back(), result.path.size());
    }
}
```

実行例：

```text
distance=10 completed_days=1
day=1 deliver:1 deliver:2 return
day=2 deliver:3 deliver:4
```

1日目は帰庫まで完了し、2日目は最後の配達直後に完成しています。最後の日の操作もpathに含まれます。

## 11. chokudai_copy_sub_v11.hpp

### 例17：期の開始候補を残し、期内の増強回数を探索する

各期の増強回数が異なる3期の生産計画を探索します。増強費用6、能力上限4、生産収入は能力の5倍です。最終的には各期の増強回数の表を返すので、Stateだけを保存します。

`sweep_width`件の期開始候補を取り出し、その期内を`sub_beam_width`の通常ビームで探索します。期の開始候補の残りは次スイープへ持ち越せますが、期内のsub層そのものを再開する仕組みではありません。`Next::Sub`と`Next::Turn`はこの境界に合わせます。

`max_sweeps=0`、全体時間1,000msで、完成計画を得た後も余裕があれば別の開始候補を試します。chokudai subにも`continue_on_timeout`があり、既定trueで未発見時の救済を許可します。`use_turn_time_limit`はありません。時間制限は厳密な強制停止ではありません。

```cpp
#include <array>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_sub_v11.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = cs::ChokudaiCopySub<long long, Hash>;

constexpr int PERIODS = 3, MAX_LEVEL = 4;
constexpr long long UPGRADE_COST = 6, INCOME_PER_LEVEL = 5;

// 回答は各期の増強回数の表です。操作順は「増強してから生産」で固定です。
// これは最終計画のデータなので、Action履歴を追加しなくても出力できます。
struct State {
    int level = 1;
    long long profit = 0;
    std::array<int, PERIODS> upgrades{};
};

int main() {
    BS::Param param{};
    param.max_turn = PERIODS; // 1期の生産を終えるたびにturnが進みます。
    param.beam_width = 16; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も継続探索の候補にします。最大turnは展開しません。
    param.max_turn_is_answer = true; // 最大turnへの到達を完成解として扱います。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sweep_width = 2; // 各turnから1スイープで取り出す候補数です。
    param.max_sweeps = 0; // 回数では制限せず、時間または候補切れまで探索します。
    param.sub_beam_width = 8; // 同turnの次sub層へ残す候補数です。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        const long long profit = now.state.profit + INCOME_PER_LEVEL * now.state.level;
        // makerは保存が必要なときだけ実行されます。末尾のNextが保存先を決めます。
        emit.push_lazy(-profit, static_cast<Hash>(now.state.level), false,
            [&now, profit] {
                State next = now.state;
                next.profit = profit;
                return next;
            }, BS::Next::Turn);
        if (now.state.level < MAX_LEVEL) {
            const int level = now.state.level + 1;
            const long long next_profit = now.state.profit - UPGRADE_COST;
            emit.push_lazy(-next_profit, static_cast<Hash>(level), false,
                [&now, level, next_profit] {
                    State next = now.state;
                    next.level = level;
                    next.profit = next_profit;
                    ++next.upgrades[now.turn];
                    return next;
                }, BS::Next::Sub);
        }
        // level上限により、どの経路のSub遷移も有限です。
        // 同じ保存先・levelなら将来条件は同じで、利益が大きい計画を残せます。
    };

    // -DLOCALを付けたビルドでのみCSVを記録します。未定義時は空実装です。
    // 第2引数50.0は記録の最小間隔ms。0.0なら対象イベントを毎回記録します。
    BS::CsvStatHook csv("chokudai_sub_production.csv", 50.0);
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{1}, expand, csv);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    const State& answer = *result.best_state;
    std::cout << "profit=" << answer.profit << " level=" << answer.level << " upgrades=";
    for (int count : answer.upgrades) std::cout << count << ' ';
    std::cout << '\n';
}
```

実行例：

```text
profit=42 level=4 upgrades=3 0 0
```

最初の期に3回増強し、その後は生産だけを行う計画です。

### 例18：日単位で別経路も探索し、途中完成の配達列を返す

1日内の配達をSub、帰庫・給油をTurnとして、最大3日で全配達を終える経路を探します。候補の開始日をそろえたchokudai探索と、日内の通常ビームを組み合わせます。StateとActionを分離して配達コマンド列を返します。

`beam_width`は各日の開始候補の保持量、`sweep_width`は1スイープで取り出す開始候補数、`sub_beam_width`は日内の幅です。`max_turn_is_answer=false`で、最後の配達を終えたSub状態だけを有効解にします。最終Stateとpathは`run<State, Action>`の結果から受け取れます。

時間を超えたときに有効解があれば、処理中のexpandを終えて探索を終了します。解がなければ可能な探索を続けるため、提出全体の締切には余裕を残します。出力は完了した日ごとの区間と、最終日の未完了区間の両方を含めます。

`expand_answers=false`で、完成した有効解を継続探索の候補から外します。完成解がビーム幅を消費せず、`expand`内の完成判定によるreturnも不要です。

```cpp
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include "chokudai_copy_sub_v11.hpp"

using Hash = std::uint64_t;
// Costはlong long。すべてのライブラリで、小さいcostほど良い候補です。
using BS = cs::ChokudaiCopySub<long long, Hash>;

constexpr int CUSTOMERS = 4, FUEL = 8;
constexpr unsigned ALL_DONE = (1U << CUSTOMERS) - 1;
// 0は車庫、1〜4は配達先です。1次元上の距離を移動費用・燃料消費とします。
constexpr std::array<int, CUSTOMERS + 1> position{0, -3, -1, 2, 4};
struct State { unsigned done = 0; int at = 0, fuel = FUEL; };

Hash state_hash(const State& state) {
    // 同じ保存先での将来条件は配達済み集合・現在地・燃料で決まります。
    // 途中の訪問順は将来条件に影響せず、Hashへ履歴全体を入れる必要はありません。
    return static_cast<Hash>((state.done * (CUSTOMERS + 1) + state.at) * (FUEL + 1) + state.fuel);
}

// Stateへ操作列を入れず、1操作分の出力情報をActionとして渡します。
struct Action { int to = 0; bool end_day = false; };

int main() {
    BS::Param param{};
    param.max_turn = 3; // turn=0,1,2が1〜3日目です。3日目までに配達します。
    param.beam_width = 64; // 同じ保存先に残す候補数の上限です。
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = false; // 有効解は保存だけを行い、その先は展開しません。
    param.max_turn_is_answer = false; // 解の条件は各候補のis_answerで指定します。
    param.use_hash_dedup = true; // 明示したHashで、同じ将来条件の候補をまとめます。
    param.time_limit_ms = 1000.0; // 探索全体の予算です。この小例には余裕を持たせています。
    param.time_check_interval = 64; // 通常は64展開ごとに確認。expandの途中では中断しません。
    param.sweep_width = 2; // 各turnから1スイープで取り出す候補数です。
    param.max_sweeps = 0; // 回数では制限せず、時間または候補切れまで探索します。
    param.sub_beam_width = 64; // 同turnの次sub層へ残す候補数です。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State, Action>& emit) {
        for (int to = 1; to <= CUSTOMERS; ++to) {
            const unsigned bit = 1U << (to - 1);
            if (now.state.done & bit) continue;
            const int distance = std::abs(position[to] - position[now.state.at]);
            if (distance > now.state.fuel) continue;
            const State next{now.state.done | bit, to, now.state.fuel - distance};
            // 履歴ありpushはStateの直後にActionを置き、Nextは最後に指定します。
            emit.push(next, Action{to, false}, now.cost + distance, state_hash(next),
                      next.done == ALL_DONE, BS::Next::Sub);
        }
        const int distance = std::abs(position[now.state.at]);
        if (now.turn + 1 < param.max_turn && now.state.at != 0 && distance <= now.state.fuel) {
            const State next{now.state.done, 0, FUEL};
            // 帰庫・給油は日を確定する操作です。配達回数に関係なく次turnへ進みます。
            emit.push(next, Action{0, true}, now.cost + distance, state_hash(next),
                      false, BS::Next::Turn);
        }
        // Subごとに未配達件数が減り、Turnは最大日数に制限されるので有限です。
    };
    const auto result = BS::run<State, Action>(param, State{}, 0LL, state_hash(State{}), expand);
    if (!result.found) {
        std::cout << "no solution\n";
        return 0;
    }
    std::cout << "distance=" << result.best_cost << " completed_days=" << result.best_turn << '\n';
    auto print_range = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            const Action& action = result.path[i];
            if (action.end_day) std::cout << " return";
            else std::cout << " deliver:" << action.to;
        }
        std::cout << '\n';
    };
    for (int day = 0; day < result.best_turn; ++day) {
        std::cout << "day=" << day + 1;
        print_range(result.turn_offsets[day], result.turn_offsets[day + 1]);
    }
    // 最後の配達はSubなので、その日のTurn境界はまだありません。
    // turn_offsets.back()より後のActionも、出力する解の一部です。
    if (result.turn_offsets.back() < result.path.size()) {
        std::cout << "day=" << result.best_turn + 1;
        print_range(result.turn_offsets.back(), result.path.size());
    }
}
```

実行例：

```text
distance=10 completed_days=1
day=1 deliver:1 deliver:2 return
day=2 deliver:3 deliver:4
```

1日目は帰庫まで完了し、2日目は最後の配達直後に完成しています。最後の日の操作もpathに含まれます。

## 12. コード例を本番へ組み込むとき

まず、問題の1手がsingle・multi・subのどれに相当するかを決め、完成条件を`max_turn_is_answer`と`is_answer`へ対応させます。次に、最終Stateだけで回答できるか、Action列も必要かを決めます。deltaを選ぶ場合は、状態の変更前の値も含め、巻き戻せるActionを設計します。

Hashには、以降の合法手や目的値への影響を変える情報を含めます。掲載例の使用集合や現在地だけでは足りない問題では、残り資源・盤面・直前操作などを追加してください。出力だけの履歴をHashに含める必要はありません。

幅と時間予算は実際の入力で調整します。全turnを進めたい通常ビームではturn別配分を検討し、chokudaiでは保持幅とスイープごとの取出し量を分けて調整します。どの場合も、`found`を確認してから解を取り出し、有効な初期解があるなら利用側でも保持しておきます。

掲載した18本の完全なコードは、対応する9ヘッダを用いてGCC 13.3.0・C++20でコンパイル・実行しています。`LOCAL`あり／なしの両方で`-Wall -Wextra -Werror`を通し、割当・作業選択・生産計画は全列挙や動的計画法、経路・配達は最短路計算で得た値と照合しました。操作列の再生、日別・期別の出力、CSVの出力条件も確認しています。
