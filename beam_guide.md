# ビームサーチ9種類の利用ガイド

このガイドは、次の9ファイルを対象にします。各コード例は標準入力を使わず、対応するヘッダを用意すれば、そのままコンパイル・実行できます。小さな問題を使い、探索結果と復元した行動の整合性を`assert`でも確認します。

| ライブラリファイル | 型 | 状態の保持方法 | 1回の遷移 |
| --- | --- | --- | --- |
| `beam_copy_single_v30.hpp` | `bs::Beam<Cost, Hash>` | Stateを保存 | 必ず次のturnへ |
| `beam_delta_single_v36.hpp` | `bs::Beam<Cost, Hash>` | Actionを保存し、外部状態を更新 | 必ず次のturnへ |
| `beam_copy_multi_v34.hpp` | `bs::Beam<Cost, Hash>` | Stateを保存 | 指定した`step`だけturnを進める |
| `beam_delta_multi_v59.hpp` | `bs::Beam<Cost, Hash>` | Actionと時刻情報を保存し、外部状態を更新 | 指定した`step`だけturnを進める |
| `chokudai_copy_single_v06.hpp` | `cs::ChokudaiCopySingle<Cost, Hash>` | Stateと未展開候補を保存 | 必ず次のturnへ |
| `chokudai_copy_multi_v07.hpp` | `cs::ChokudaiCopyMulti<Cost, Hash>` | Stateと未展開候補を保存 | 指定した`step`だけturnを進める |
| `beam_copy_sub_v04.hpp` | `bs::Beam<Cost, Hash>` | Stateを保存 | 同turnの次sub層、または次turnへ |
| `beam_delta_sub_v05.hpp` | `bs::Beam<Cost, Hash>` | Actionを保存し、外部状態を更新 | 同turnの次sub層、または次turnへ |
| `chokudai_copy_sub_v03.hpp` | `cs::ChokudaiCopySub<Cost, Hash>` | turn開始候補を持ち越し、turn内は通常ビーム | 同turnの次sub層、または次turnへ |

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

`copy`では、盤面や履歴を含む`State`を候補ごとに保存します。`expand`は`now.state`を読んで子を作ります。まず実装を簡潔にしたい場合や、状態が小さい場合に適します。結果の`best_state`は`std::optional<State>`で、履歴は必要なら利用側がStateに持たせます。

`delta`では、小さな`Action`と探索木を保存し、盤面などの外部状態を`move_forward`と`move_backward`で移動させます。盤面コピーが重く、変更と取り消しが軽い場合に適します。変更前の値がなければ戻せない操作は、その値をActionへ保存します。`expand`の終了時には、外部状態を呼び出し時と同じ状態に保ってください。試しに手を適用して評価する場合も、その場で戻してから次の候補を扱います。

delta版の正常な`run`返却時には、外部状態は初期状態へ戻ります。最良解の盤面になっているわけではありません。必要なら、返された`path`を改めて再生します。callbackからの例外による中断まで自動復元されることは前提にしないでください。

| delta版 | callbackが受け取る代表的な型 | `result.path`の要素 | turn境界の確認 |
| --- | --- | --- | --- |
| single | `const Action&` | `Action` | 1要素が1turn |
| multi | `const BS::Edge<Action>&`、または`const Action&` | `BS::Edge<Action>` | `edge.turn`、`edge.step`、`edge.next_turn()` |
| sub | `const Action&` | `Action` | `result.turn_offsets` |

### 1.3 通常ビーム・chokudaiの違い

通常ビームは、ある層の候補を展開し、次へ進める候補を幅で絞ります。過去の層へ戻って未展開候補を再開する方式ではありません。

chokudaiは、turnごとに未展開候補を保存します。浅いturnから深いturnまで巡回する1回を「スイープ」と呼び、各turnから最大`chokudai_width`件を取り出します。同じスイープで作った未来のturnの候補も、そのスイープ内で展開できます。最終turnへ到達した後も、時間・回数の余裕と未展開候補があれば、次スイープで別の経路を探索します。

`chokudai_copy_sub`でスイープをまたいで残すのは、各turnの開始候補です。取り出した開始候補からのturn内探索は、`sub_beam_width`の通常ビームとして処理します。時間切れの処理を除けば、選択した開始候補群から生じたsub層を処理してから次turnへ移ります。turn内のsub層そのものを、次スイープへ持ち越して再開する方式ではありません。

| 選ぶ基準 | 第一候補 |
| --- | --- |
| 1行動が必ず1手で、状態が小さい | `beam_copy_single` |
| 1行動が必ず1手で、盤面コピーが重い | `beam_delta_single` |
| 1行動で複数の時間・距離などを消費する | `beam_copy_multi`、または`beam_delta_multi` |
| 固定turnの中で、可変回数の小操作を行う | `beam_copy_sub`、または`beam_delta_sub` |
| 途中評価で後回しにした候補にも、後で探索機会を与えたい | 対応する`chokudai_copy` |

chokudaiは、より多くのturnに候補を残すため、一般に状態保存量が増えます。通常copy singleの保持状態数は主にビーム幅に、copy multiは主に`max_step × beam_width`に、chokudai copyは主に`max_turn × beam_width`に依存します。sub copyではturn境界幅とsub幅の両方が効きます。delta subは、現在地・候補・最良解に必要な祖先を残し、不要ノードをまとめて回収してスロットを再利用します。長い経路自体は保持するため、長いsub探索では`used_nodes`と`reserved_nodes`も確認してください。State内部のvectorや履歴の長さもメモリ使用量に影響します。

## 2. 共通の使い方と注意点

### 2.1 ヘッダと実行方法

ダウンロードした名前に`(1)`や`(2)`などの接尾辞が付いている場合は、冒頭の表の名前へそろえるか、コードの`#include`を実際のファイル名に合わせてください。各コード例は、個別の`.cpp`ファイルとして保存します。

```sh
g++ -std=c++20 -O2 -Wall -Wextra example.cpp -I ./headers -o example
./example
```

ヘッダを`.cpp`と同じフォルダへ置く場合、`-I ./headers`は不要です。C++23でも利用できます。サンプルはGCC向けです。これらのヘッダの利用に、AtCoder Libraryや追加のHashMapライブラリは不要です。

各サンプルではヘッダを1つだけ読み込みます。特に`bs::Beam`の各実装は同じ型名を定義するため、複数を同時にincludeしないでください。方式を比較する場合は、スケルトンのようなプリプロセッサによる切り替え、または別の実行ファイルを使います。複数のコード例をそのまま1ファイルへ連結すると、`main`なども重複します。

### 2.2 costは「小さいほど良い」

全9種類がcostの最小化です。得点や利益を最大化する場合は、たとえば`cost = -score`として渡します。`now.cost`は利用側が渡した評価値であり、ライブラリが移動費を自動加算することはありません。

costは累積費用でも、状態全体に対する評価値でも構いません。ただし、同じ保存先に入る候補同士で比較できる尺度にしてください。また、有効解に渡すcostは最終的に比較したい目的値と整合させます。途中の期待値と完成時の実得点を混ぜる場合は、その切り替えを利用側で設計します。

`Cost`はboolを除く符号付き整数、または浮動小数点型です。NaNの候補は棄却されます。演算時の整数オーバーフローは利用側で防いでください。サンプルは`long long`を使います。

ビーム幅で候補を捨てるため、一般に最適解の保証はありません。コード例の最適値の`assert`は、その小問題と設定で検証した値です。実問題で幅を増やした場合の速度やスコアは、問題の評価関数と分岐に依存します。

### 2.3 Paramの読み方

省略した項目は`Param{}`の既定値です。サンプルでは用途に関係する設定を明示します。

| 設定 | 対象 | 意味・既定値 |
| --- | --- | --- |
| `max_turn` | 全種類 | 到達できる最大turn。既定0。初期状態はturn=0 |
| `beam_width` | 全種類 | 各候補群、または各turnの開始キューの保持幅。既定1 |
| `time_limit_ms` | 全種類 | 探索全体の時間制限。既定0.0で無制限。soft limit |
| `use_turn_time_limit` | 通常ビーム6種類 | 残り時間を残りturn数へ配分する。既定false。全体の時間制限が有効な場合だけ動作 |
| `time_check_interval` | 全種類 | 通常の時間確認間隔。既定64。1なら各展開終了後に確認する。境界などでも確認される |
| `max_turn_is_answer` | 全種類 | 最大turnに到達した状態を有効解にする。既定true |
| `use_hash_dedup` | 全種類 | Hashを明示した候補を重複排除する。既定true |
| `max_step` | multiの3種類 | 1遷移で進めるturn数の上限。既定1 |
| `sub_beam_width` | subの3種類 | 同turnの次sub層の幅。既定0で`beam_width`を使用 |
| `chokudai_width` | chokudaiの3種類 | 1turn・1スイープで取り出す候補数。既定1。sub版では開始候補数 |
| `max_sweeps` | chokudaiの3種類 | スイープ数上限。既定0で無制限。sub版は通常スイープへの上限 |
| `finish_on_timeout` | chokudai single・multi | 解なしの時間切れに、幅1の追加スイープを高々1回行う。既定false |
| `hash_capacity` | single・multiの6種類 | Hash表の容量指定。既定0で自動。幅を基準に調整されるため、通常は0のままでよい |
| `nodes_capacity` | deltaの3種類 | 探索木のノード容量。種類による違いは次の表を参照 |
| `capacity_policy` | delta multi | 自動ノード容量の方針。既定`BS::CapacityPolicy::Safe` |
| `print_warnings` | beam delta single・multi、beam copy multi | 容量不足などの警告を標準エラーへ出す。既定true |

`beam_copy_single`、chokudai single・multi、subの3種類には`print_warnings`はありません。subの3種類には`hash_capacity`もありません。対応していない項目をそのままコピーしないでください。

delta single・multi は内部の整数添字を安全に扱うため、`max_turn == INT_MAX` または `beam_width > 268435451` を `std::length_error` で拒否します。これは表現上の上限であり、その範囲内のメモリ確保を保証するものではありません。

delta single・multi の `Runtime::best_update_count` とCSV行の `period_best_updates`、delta multi の `Runtime::width_pruned_cost_count` は `long long` です。大量の候補を生成する場合も、利用側で `int` に狭めず扱ってください。

| delta版の容量設定 | `nodes_capacity=0`の意味 | 正の上限に達した場合 |
| --- | --- | --- |
| single | `max_turn`と`beam_width`からノード数の上限を自動設定。格納領域は必要に応じて増設・再利用 | 不要ノードを回収しても不足する場合に`result.node_pool_exhausted`となる |
| multi | `capacity_policy`に従い自動設定 | `result.node_pool_exhausted`を確認 |
| sub | 回収・再利用しながら必要に応じて動的に増設 | 不要ノードを回収しても不足する場合に`result.runtime.stop_reason == BS::StopReason::NodeLimit`となる |

delta multiの`Safe`はturn数を考慮する安全側の自動容量です。`Compact`は`max_step`を重視して容量を抑えますが、探索形状によっては容量不足で途中終了し得ます。容量不足でも見つかった解が返ることはありますが、予定した探索を完走したとは扱わないでください。

全種類で`max_turn`の状態は展開されません。`max_turn=0`なら`expand`は呼ばれず、`max_turn_is_answer=true`なら有効な初期costの初期状態が解になります。正の`max_turn`で初期状態自体を有効解として残したい場合、初期状態に`finished`を指定する引数はありません。必要なら利用側で初期解を保持し、探索結果と比較します。

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

delta型は初期Hashが必須です。重複排除を使わない場合も、たとえば`Hash{0}`を渡します。

```cpp
auto result = BS::run<Action>(param, initial_cost, initial_hash,
                              expand, move_forward, move_backward, hook);
```

`run`を呼ぶたびに新しい探索を開始します。前回の未展開候補を引き継ぐ再開APIではありません。`StateView`や`Emitter`への参照を外へ保存して後で使わず、callbackの実行中だけ扱ってください。

### 2.5 pushとpush_lazyの引数

`value`はcopy型では`State`、delta型では`Action`です。`hash`と`finished`は任意ですが、multiの`step`は必須です。次の表は全項目を指定する形です。

| 系統 | `push`の最大引数形 | copy型の`push_lazy`の最大引数形 |
| --- | --- | --- |
| single | `push(value, cost, hash, finished)` | `push_lazy(cost, hash, finished, maker)` |
| multi | `push(value, cost, hash, step, finished)` | `push_lazy(cost, hash, step, finished, maker)` |
| sub | `push(value, cost, hash, finished, next)` | `push_lazy(cost, hash, finished, maker, next)` |

subの`next`は省略すると`BS::Next::Turn`です。`push_lazy`では`maker`の後に置きます。delta型には`push_lazy`はありません。

`step`は`int`型で、1以上かつ`max_step`以下です。到着turnが`max_turn`を超える候補は受理されません。multiの`step=0`を、subの代わりに使うことはできません。

Hash引数はbool以外の符号なし整数で、`Hash`型以下の幅が必要です。たとえば`Hash{123}`を使います。`finished`には`true`または`false`などのboolを渡し、整数の0や1で代用しないでください。StateやActionは指定した型そのものを渡します。

`push_lazy`は、保存する必要があると判断されたときにだけ、引数なしの`maker()`からStateを作ります。makerは同期的に呼ばれ、後で実行するために保存されません。このためサンプルの`[&now, ...]`は有効です。候補を棄却するために必要なcostやHashはmakerより前に計算する必要があります。makerの中だけで計算して外部へ渡す設計にはできません。

Stateコピーを遅らせたい場合、先に巨大な`State next`を作ってそれをラムダへコピーすると利点を失います。小さな変更情報をキャプチャし、maker内で親Stateをコピーして変更します。

`push`や`push_lazy`の戻り値がtrueでも、その候補が最後まで探索候補群に残る保証はありません。後の候補に置換される可能性があります。通常ビームとchokudaiのsingle・multi計6種類では、このboolは「呼び出し時点で探索候補へ入ったか」です。falseでも最良有効解を保存する場合があり、解の有無は`found`で確認します。sub版のtrueは、次候補としての保存に加えて、最良有効解としての保存だけでも返ります。

通常ビームとchokudaiのcopy single・multiの`push_lazy`は、探索候補への保存、または最良有効解の更新に必要な場合にmakerを呼びます。両方に使う場合でも1回だけです。幅・Hashで棄却され、かつ最良解を更新しない場合や、NaN・到着turn超過などの不正候補では呼びません。makerの呼び出し回数を、探索候補の採用数と同一視しないでください。

### 2.6 Hashによる重複排除

Hashを明示した候補だけが重複排除に参加します。初期Hashを渡しただけでは、その後のHashなし`push`が自動的に重複排除対象になるわけではありません。逆に、copy型の初期Hashを省略しても、Hash付き`push`を使えば、その候補は重複排除対象になります。

同じHashは「将来の選択肢と目的値への影響が同じ」とみなされます。ライブラリはStateの等値比較で衝突を確認しません。小さな状態は使用集合のビットマスクなど、衝突のない整数へ変換すると理解しやすくなります。一般の大きな状態に64bit Hashを使う場合は、衝突の可能性を利用側で受け入れる必要があります。

たとえば割当問題では、同じturn・同じ使用仕事集合なら将来の仕事候補が同じです。小さい累積費用の経路を残してよいので、Hashに履歴全体を含める必要はありません。一方、残り燃料や最後の訪問地点が今後の可否を変えるなら、その情報もHashへ含めます。

重複排除の範囲は、その時点で同じ保存先に残っている候補です。chokudaiは展開済みHashの永久的な訪問済み集合を持ちません。subでは次sub層と次turn候補を別々に扱います。次turnへ進んだ後にも経路全体の操作回数などで制約が変わる問題では、異なるdepthを同じHashで誤って統合しないようにします。

同じHashで同じcostなら、後から来た候補が必ず優先されるわけではありません。`finished`の有無が解の可否を変える状態や、終端・非終端の違う状態を安易に同一Hashへまとめないでください。

### 2.7 finishedと解の保存

`finished=true`は「この生成状態を有効解として比較できる」という印です。探索全体の停止命令でも、その状態の自動的な展開停止命令でもありません。終端状態をさらに展開したくない場合は、`expand`の先頭などで判定してreturnします。

`max_turn_is_answer=true`なら、最大turnに到達した状態も有効解です。最大turn到達だけでは目的を達成しない問題ではfalseにし、目的条件を満たす候補へ`finished=true`を渡します。

| 系統 | 有効解として保存されるタイミングと注意 |
| --- | --- |
| 通常ビームsingle・multiの4種類 | 正常な候補の生成時に、最良有効解を幅・Hash選別とは独立して保存する。探索候補として棄却された場合も、保存した解は残る |
| chokudai single・multi | 正常な候補の生成時に、幅・Hashによる探索候補の受理可否と独立して最良解を保存する。棄却された有効解も比較対象になる |
| subの3種類 | 最良有効解の更新を、探索候補群の幅・Hash選別と独立して扱う。探索候補として残せなくても、改善する有効解なら保存する |

通常ビームとchokudaiのsingle・multiでは、候補のcost・step・到着turnが許容範囲であることを確認し、探索候補として残るかどうかにかかわらず有効解を比較します。保存するのは最良の1件で、ビーム幅を消費しません。有効解同士ではcostが小さい方を残し、同costなら先に生成した方を残します。multiでは未来の到着turnの候補も生成時に保存し、`best_turn`にはその到着先を記録します。保存済みの解は、その後の候補置換、時間超過時の候補削減、探索木のノード解放で失われません。生成していない候補まで含めた最適解の保証ではありません。

copy single・multiの結果は`best_state`です。delta singleの`path`はAction列、delta multiの`path`は`action`・`turn`・`step`を持つEdge列です。delta singleは親ノード番号と末尾Actionで解を保持し、返却時に経路を復元します。delta multiはノードが再利用される前に、経路を独立した配列へ保存します。

`found`は有効解の有無です。`completed_max_turn`は最大turnへの到達を示すもので、最適性や全候補展開の保証ではありません。通常ビームsingle・multiでは「最大turnの探索候補を実体化したか」を表し、解の生成時保存とは別です。容量不足などで探索を終了した場合、最大turnの有効解を保存できていても、このフラグはfalseの場合があります。chokudai single・multiの`completed_max_turn`は、最大turnの候補をキューへ受理したかを示します。こちらも、最良解の保存や全候補の展開とは別です。`max_turn_is_answer=false`なら、最大turnへ達しても`found=false`になり得ます。`best_cost`や`best_state`、`path`を解として使う前に、`found`を確認してください。

sub版では、有効解がturn途中でも返ります。`best_turn`、`best_sub_step`、`best_depth`で位置を確認できます。delta subの`turn_offsets`は、found=falseなら空、found=trueなら先頭0を持ちます。完了したturn tのAction区間は`[turn_offsets[t], turn_offsets[t + 1])`です。`turn_offsets.back()`から`path.size()`までが残っていれば、そこは未完了turn内の末尾です。

### 2.8 時間制限と打ち切り

`time_limit_ms`は1turnごとの時間ではなく、1回の探索全体の時間です。全種類とも`expand`を途中で中断しません。長いcallback、返り値の復元、CSV書き出しなども考慮し、提出全体の厳密な締切と同一視しないでください。

| 系統 | 全体の時間制限到達を検出した後の主な動作 |
| --- | --- |
| 通常ビームsingle・multi | 最良1本を中心とする縮退探索へ移り、各turnを先へ進める。時間到達で直ちにreturnする方式ではない |
| chokudai single・multi | 通常探索を終了する。`finish_on_timeout=true`かつ解未発見なら、残った候補から幅1の追加スイープを高々1回行う。解発見後はその救済を終える |
| 通常ビームsub | 次turn候補があればturn内探索を切り上げて進む。次turn候補がなければ、残ったsub候補から先へ進める候補を探す。有効解を持っているだけで即時終了する仕様ではない |
| chokudai sub | 有効解があれば終了する。なければ次turn候補を優先して進み、必要なら残った候補を時間超過後も探索する。次turn候補も有効解もない間は、可能なSub探索を続ける |

chokudai single・multiの追加スイープは1回までで、有効解の発見保証ではありません。幅・Hashで棄却された有効解を保存済みなら、追加スイープは開始しません。追加スイープ中にそのような解を見つけた場合も、実行中の`expand`が返ったところで救済を終えます。chokudai subには`finish_on_timeout`はなく、未発見時の救済は組み込み動作です。subの`max_sweeps`は時間超過後の未発見救済には適用しません。一方、時間制限に達する前に通常スイープ上限へ達すれば、解なしでも回数上限で終了します。

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

turn期限だけでは保持幅を1に縮めず、生成済み候補から通常の選抜で最大`beam_width`件を引き継ぎます。次turnでは期限到達フラグを解除して通常探索へ戻ります。全体の期限にも達した場合は、上表の全体時間超過処理が優先します。`finished=true`を指定しても、探索全体を自動で終了することはありません。

配分を有効にしても、実行中のexpand・遅延生成・状態の移動や復元を強制中断することはありません。初期化やturn間の候補実体化に費やした時間も、次回の残り時間に反映されます。ビーム幅を自動的に増やす機能ではなく、turnごとの探索時間の偏りを抑えるための機能です。重要な探索が特定turnへ集中する問題や、上限より早く探索が終わる問題では、無効時との比較も行ってください。

例02・05・08・11・20・22では全体1,000msとturn別配分を指定しています。小さい例なので余裕を持って探索できる設定ですが、厳密な実行時間保証ではありません。時間切れの動作自体を再現するのは例24です。他の例は時間制限0です。実問題に合わせて予算を変更すると、例中の固定スコアのassertも見直す必要があります。`time_check_interval`を大きくすると時計確認の頻度は減りますが、時間超過の検出が遅くなり得ます。

### 2.9 Runtime・CSV・独自hook

通常ビームsingle・multiでは、`Runtime::found`、`best_cost`、`best_turn`、`best_update_count`は、解を生成した`push`／`push_lazy`の中で更新されます。`BestUpdate` hookは1回の`expand`内の改善をまとめ、`expand`が返った後に1回通知します。`best_update_count`は実際の改善回数なので、hookの通知回数とは一致しない場合があります。初期状態自体が解になる0turn探索では、RunStart後に通知します。delta multiの経路は、解の改善時に保存します。

chokudai single・multiでも、解を保存した`push`／`push_lazy`が返る前に、`Runtime::found`、`best_cost`、`best_turn`、`best_update_count`と`last_best_update_sweep`を更新します。探索候補へは棄却された有効解の改善も含みます。`BestUpdate`イベントはなく、CSVではSweepEndやRunEndで確認します。

`BestUpdate`では最良解のメタ情報は最新ですが、frontier由来の集計値は直前の値の場合があります。またmultiでは、現在の探索turnより`best_turn`の方が先になる場合があります。これは未来の到着候補を生成時に解として保存したためです。 `turns_since_best_update`は、最良解を生成した探索turnから数えます。到着先の`best_turn`からは数えません。

`run`の最後の引数へ`BS::CsvStatHook("filename.csv", interval_ms)`を渡すと、進捗を記録できます。既定の記録間隔は50ms、0なら対象イベントを全て記録します。CSVはメモリに蓄積してRunEndで書き出すため、細かい記録には時間・メモリの費用があります。同名ファイルは上書きされるので、用途ごとに名前を変えてください。

delta single・multi の記録間隔は、負値・NaNも0と同じ扱いです。正の無限大・表現範囲を超える間隔は内部の最大値へ丸めます。CSVの小数点はロケールに依存せず `.`、`signed char` のCostも文字ではなく数値で出力します。

| 系統 | 独自hookが受け取れる主なイベント | CsvStatHookが記録するイベント |
| --- | --- | --- |
| 通常ビームsingle・multi | RunStart、TurnStart、TurnEnd、BestUpdate、RunEnd。delta multiはRootCompressもある | RunStart、TurnEnd、RunEnd |
| chokudai single・multi | RunStart、SweepStart、SweepEnd、RunEnd | RunStart、SweepEnd、RunEnd |
| 通常ビームsub | RunStart、SubStepEnd、TurnEnd、RunEnd | 同左。間隔に従って記録 |
| chokudai sub | RunStart、SubStepEnd、TurnEnd、SweepEnd、RunEnd | 同左。間隔に従って記録 |

subのSubStepEndは、その層を処理し終えた通知です。通常ビームsubが時間超過で層の途中から次turnへ進む場合、その層のSubStepEndは出ませんが、TurnEndは出ます。chokudai subが即時終了する場合などは、TurnEndも出ないことがあります。RunEndの停止理由と併せて読みます。chokudai single・multiの`sweep_completed`はスイープを完走したかを表しますが、chokudai subにはこの項目やSweepStartはありません。

通常ビーム6種類のRuntimeには、turn別配分を観測する次の項目があります。これらは時計を最後に確認した時点の値で、expand中に時刻が進んでも自動更新されません。

| Runtime項目 | 意味 | CSV列 |
| --- | --- | --- |
| `turn_start_us` | run開始から、そのturnの配分を開始するまでの時間 | `turn_start_ms` |
| `turn_deadline_us` | run開始から数えた、そのturnの締切 | `turn_deadline_ms` |
| 上の2値の差 | turn開始時に割り当てた予算 | `turn_budget_ms` |
| `turn_elapsed_us` | turn開始後、最後の時計確認までの経過時間 | `turn_elapsed_ms` |
| `turn_time_limit_reached` | そのturnの締切到達を検出したか。次turnでリセット | 同名 |

配分無効・turn未開始のstart/deadline/budgetは-1、経過時間は0です。CSVの時刻はミリ秒へ変換します。`time_limit_reached`は全体の期限のフラグで、turn期限とは独立です。期限到達フラグは、未展開候補を実際に打ち切ったことを意味しません。全候補を処理した直後に期限到達を検出する場合もあります。返却時の`result.runtime`に残るturn別情報は最後に処理したturnのもので、過去のturnの超過履歴はCSVなどで確認します。

`turn_elapsed_us`はTurnEnd通知前に固定し、その通知のcallback、返却時の最終復元、RunEndの処理時間は加算しません。sub版ではTurnEndの後に行う次turn候補の実体化も、各turnの計測区間の外です。これらの探索中の費用は全体の経過時間に含まれ、次turnの予算に反映されます。全体の経過時間とturn別実測時間の合計が一致するとは限りません。

全turnの予算・実測時間・展開数を解析するときは、`CsvStatHook("beam_stat.csv", 0)`で各TurnEndを記録してください。single・multiのCSVは`use_turn_time_limit`を各行に出し、subのCSVは他の設定と同じくRunStart行へ出します。

独自hookも使えます。

```cpp
auto hook = [](BS::EventType event, const BS::Runtime& runtime) {
    if (event == BS::EventType::RunEnd) {
        std::cerr << "expanded=" << runtime.expanded_nodes << '\n';
    }
};
```

既定の`BS::NoOp`は通知を無視します。何もしないラムダと比べても、ライブラリが観測用処理を省略できる場合があります。本番で統計が不要ならhookを省略してください。Runtimeの詳細項目は方式ごとに異なります。sub版の正規化後設定は`runtime.param`にまとまっています。

| 調べたいこと | 主に見る情報 |
| --- | --- |
| 探索がどこまで進んだか | turn、sub_step、sweep、completed_max_turn。single・multi通常ビームのcopyではmaterialized_turnも参考になる |
| 生成量と展開量 | expanded_nodes、generated_candidates。subではsub_generatedとturn_generatedを分けて読む |
| 幅が足りないか | pruned_by_width、候補数、selected_widthなど。subではsub側とturn側を分ける |
| 重複が多いか | pruned_by_hash、replaced_by_hashなど、存在する方式の項目 |
| 解がいつ改善したか | best_cost、best_update_count。subではfirst_solution_ms、last_best_update_ms |
| 全体の時間超過後にどれだけ探索したか | time_limit_reached。subではexpanded_after_timeout、timeout_turn_advances、chokudai subではrescue_sweepsも参照。turn期限だけの超過はこれらに加算しない |
| deltaの状態移動費用や容量 | move_forward_count、move_backward_count、used_nodesなど |
| なぜ終わったか | 各Resultのフラグ、chokudai・subのruntime.stop_reason |

表の`sub_generated`、`turn_generated`、`first_solution_ms`、`last_best_update_ms`はCSV列名です。Runtimeではそれぞれ`sub_stats.generated`、`turn_stats.generated`、`first_solution_us`、`last_best_update_us`を参照し、時刻の単位はマイクロ秒になります。

通常ビームとchokudaiのsingle・multiの採用数・幅棄却数・Hash棄却数は、探索候補の選別についての統計です。選別で落ちた候補でも、`best_update_count`を増やす場合があります。有効解の確保は`found`と`best_cost`、改善の回数は`best_update_count`で確認します。

sub版のCSVでは、候補cost欄は対応する候補数が0なら空欄です。subの`sub_stats.accepted`と`turn_stats.accepted`は探索候補群への保存回数で、有効解だけを保存した回数とは一致しません。したがって、`push`のtrue数、accepted数、最終的に残った候補数を同じものとして集計しないでください。棄却率が高いだけで、その設定が悪いとも断定できません。解の改善量、展開数、実時間を一緒に比較します。

## 3. beam_copy_single_v30.hpp

### 例01：固定手数で目標の値へ近づける

0から始め、4回の操作で毎回1か2を加え、7に近づけます。1操作が必ず1turnなのでsingle版が適します。`push(state, cost)`、初期Hashなしの`run`、`best_state`による結果取得を使います。`beam_width=16`はこの小問題を確認するための幅で、一般の問題に対する推奨値ではありません。

途中costは目標との差です。これは累積移動量ではなく、状態の評価値です。実問題では途中の距離だけで良い経路を残せるとは限らないため、評価関数を問題に合わせて設計します。

```cpp
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "beam_copy_single_v30.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, std::uint64_t>;

// copy型では、将来の遷移に必要な情報と、出力したい履歴をStateに入れます。
// Stateはコピー・代入できる型にします。vectorも使えますが、コピー費用がかかります。
struct State {
    int position = 0;
    std::vector<int> moves;
};

int main() {
    constexpr int target = 7;
    BS::Param param{};
    param.max_turn = 4;            // ちょうど4手を評価します。初期状態はturn=0です。
    param.beam_width = 16;         // 次のturnに最大16状態を残します。
    param.time_limit_ms = 0.0;     // この小さな例は時間無制限で再現可能にします。
    param.max_turn_is_answer = true; // 4手後の状態を、自動的に解候補にします。
    param.use_hash_dedup = false;  // 基本例なので、Hashによる重複排除は使いません。

    // now.stateは読み取り専用です。子状態を作ってemitへ渡します。
    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int step : {1, 2}) {
            State next = now.state;
            next.position += step;
            next.moves.push_back(step);
            // costは小さいほど良い値。累積値に限定されず、状態の評価値でも構いません。
            const long long cost = std::abs(target - next.position);
            // single版のpushは1回につき必ずturnを1進めます。
            emit.push(next, cost);
        }
    };

    // 初期Hashを省略したrunです。初期costも同じ評価尺度に合わせます。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 7LL, expand);
    if (!result.found) {
        std::cout << "solution not found\n";
        return 1;
    }
    // copy型の結果はoptional<State>です。foundを確認してから取り出します。
    const State& answer = *result.best_state;
    int replay = 0;
    for (int step : answer.moves) replay += step;
    assert(answer.moves.size() == 4 && replay == answer.position);
    assert(answer.position == 7 && result.best_cost == 0);
    std::cout << "position=" << answer.position
              << " cost=" << result.best_cost << '\n';
}
```

確認する結果は`position=7 cost=0`です。

### 例02：仕事の割当をHashと遅延コピーで探索する

4人へ4仕事を重複なく割り当て、合計費用を最小にします。`turn`が担当者番号、`State.used`が使用済み仕事集合です。同じ担当者まで割り当て、同じ集合を使っている経路は、安い方だけを残してよいので、ビットマスクをHashにします。

`push_lazy`で、探索候補または最良有効解の保存に必要な場合だけ履歴vectorを生成します。`hash_capacity=0`は自動容量、`CsvStatHook`の0ms指定は全TurnEndの記録です。一般の問題で、将来条件が異なる状態まで同じHashへまとめないことが重要です。

全体1,000msを各turnへ配分し、`time_check_interval=1`で各expandの終了後に確認します。この小問題には余裕のある設定です。実問題で予算を短くして探索を打ち切る場合、末尾の固定スコアのassertも見直してください。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include "beam_copy_single_v30.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// usedが将来の選択肢を決めます。assignmentは答えの復元用の履歴です。
// コピーした子のvectorが親と独立する、通常の値型として定義します。
struct State {
    unsigned used = 0;
    std::vector<int> assignment;
};

int main() {
    const std::array<std::array<int, 4>, 4> fee{{
        {{9, 2, 7, 8}}, {{6, 4, 3, 7}}, {{5, 8, 1, 8}}, {{7, 6, 9, 4}}
    }};
    BS::Param param{};
    param.max_turn = 4;             // turnが担当者番号です。各人へ仕事を1つ割り当てます。
    param.beam_width = 16;          // 小さな例の全使用集合を保持できる幅です。
    param.time_limit_ms = 1000.0; // 探索全体の時間予算。小さい例なので十分な余裕を設けます。
    param.use_turn_time_limit = true; // 各turn開始時に、残り時間を残りturn数へ配分します。
    param.time_check_interval = 1; // 各expand終了後に時計を確認します。expand自体は中断しません。
    param.max_turn_is_answer = true; // 全員への割当完了を有効解にします。
    param.use_hash_dedup = true;    // 同turn・同usedなら、小さいcostの候補を優先します。
    param.hash_capacity = 0;       // Hash表の容量はbeam_widthに合わせて自動設定します。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const unsigned used = now.state.used | (1U << job);
            const long long cost = now.cost + fee[now.turn][job];
            // 同じturnとusedなら今後の費用条件も同じなので、履歴をHashに含めません。
            const Hash hash = static_cast<Hash>(used);
            // 探索候補または最良有効解の保存に必要な場合だけ、Stateをコピーします。
            // makerはこの呼び出し中に実行され、後のturnまで保存されません。
            emit.push_lazy(cost, hash, [&now, used, job]() {
                State next = now.state;
                next.used = used;
                next.assignment.push_back(job);
                return next;
            });
        }
    };

    // 第2引数0.0で全TurnEndを記録します。CSVはrun終了時に書き出されます。
    BS::CsvStatHook csv("copy_single_assignment.csv", 0.0);
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) return 1;
    const auto& answer = *result.best_state;
    unsigned used = 0;
    long long actual_cost = 0;
    assert(answer.assignment.size() == 4);
    for (int person = 0; person < 4; ++person) {
        const int job = answer.assignment[person];
        assert((used & (1U << job)) == 0);
        used |= 1U << job;
        actual_cost += fee[person][job];
    }
    assert(used == 15U && actual_cost == result.best_cost && actual_cost == 13);
    std::cout << "cost=" << result.best_cost << " jobs=";
    for (int job : answer.assignment) std::cout << job << ' ';
    std::cout << '\n';
}
```

合計費用は13です。`copy_single_assignment.csv`も生成します。

### 例03：探索候補から落ちた途中の有効解を受け取る

ゴールに到達する経路を探します。低costの行き止まりと、costは高いが有効なゴールを同じ到着turnへ生成します。`beam_width=1`なので探索候補には行き止まりだけが残りますが、ゴールは解として独立に保存されます。`max_turn_is_answer=false`で、ゴールへ渡す`finished=true`だけを解の条件にします。

ゴールは`push_lazy`で生成します。戻り値がfalseでも、最良解の保存に必要なのでmakerは1回呼ばれます。`result.found`を確認し、`best_state`から解を取得します。この例のStateは小さな値だけですが、操作列が必要ならStateにその履歴も持たせます。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_copy_single_v30.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long>;

// node=0は出発点、1は低costだが行き止まり、2は有効なゴールです。
// 有効解の判定は利用側が行い、候補追加時のfinishedへ渡します。
struct State {
    int node = 0;
};

int main() {
    BS::Param param;
    param.max_turn = 5; // 最大5turnまで探索できます。ゴールはそれより前にあります。
    param.beam_width = 1; // 探索候補は1件。cost=0の行き止まりが残ります。
    param.max_turn_is_answer = false; // 最大turnへの到達だけでは解にしません。
    param.time_limit_ms = 0.0; // この小さな例は時間無制限で実行します。

    bool selected = true;
    int goal_expansions = 0;
    int maker_calls = 0;

    auto expand = [&](const BS::StateView<State>& now, auto& emit) {
        if (now.state.node == 2) ++goal_expansions;
        if (now.state.node != 0) return; // 行き止まりとゴールからは子を生成しません。

        // 先に、低costの非有効解でビームを埋めます。
        emit.push(State{1}, 0LL, false);

        // ゴールはcost=10なので、探索候補としては選別で落ちます。
        // finished=trueの有効解としては、選別とは独立に保存されます。
        selected = emit.push_lazy(10LL, true, [&] {
            ++maker_calls;
            return State{2}; // makerは解の保存のために1回だけ呼ばれます。
        });
    };

    // 初期State、初期cost、候補生成処理を渡します。Hashは使っていません。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run(param, State{}, 0LL, expand);
    assert(!selected); // push_lazyのboolは、探索候補としての採用結果です。
    assert(maker_calls == 1);
    assert(result.found && result.best_state->node == 2);
    assert(goal_expansions == 0); // ゴールを探索候補へ残さなくても、解は得られます。
    assert(result.best_cost == 10 && result.best_turn == 1);
    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " selected=" << selected << '\n';
}
```

出力は`found=1 best_cost=10 best_turn=1 selected=0`です。

## 4. beam_delta_single_v36.hpp

### 例04：同じ固定手数の問題をAction列で解く

例01と同じ問題を、外部の`position`と`Action{delta}`で表します。盤面を丸ごと保存せず、加算と減算のcallbackで移動する基本形です。`push`を呼んだ時点で外部状態が子へ進むわけではありません。

この版の`nodes_capacity=0`は、turn数と幅に基づくノード数上限の自動設定です。ノード用の格納領域は必要に応じて増やします。不要ノードはまとめて回収し、空いた領域を再利用します。現在の外部状態、次turnの候補、保存済み最良解に必要な祖先は回収から保護されます。正の上限を指定した場合も、回収してなお不足するときに`result.node_pool_exhausted`が真になります。

`runtime.used_nodes`には次の回収まで残る不要ノードも含まれ、回収時に減ることがあります。`runtime.max_used_nodes`はその最大値です。`runtime.nodes_capacity`はノード数の上限であり、実際に確保したスロット数やメモリ量のバイト上限ではありません。Action内部の可変長データや一時配列のメモリも別に必要です。

正常終了後の初期状態への復元を確認し、`result.path`を再生して最良解へ移動します。

```cpp
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include "beam_delta_single_v36.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// delta型は子状態全体でなくActionを保存します。
// root用にデフォルト構築でき、コピー・代入できる型にします。
struct Action {
    int delta = 0;
};

int main() {
    int position = 0; // expandが読む外部状態。ライブラリがcallbackで位置を合わせます。
    BS::Param param{};
    param.max_turn = 4;            // 各Actionが1turnなので、ちょうど4手です。
    param.beam_width = 16;         // 各turnに残す候補の上限です。
    param.nodes_capacity = 0;      // max_turnとbeam_widthから上限を自動設定し、必要な領域を確保・再利用します。
    param.time_limit_ms = 0.0;     // 例の結果を再現するため時間無制限にします。
    param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
    param.use_hash_dedup = false;  // この例ではHashによる枝の統合を行いません。

    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        (void)now;
        for (int delta : {1, 2}) {
            const long long cost = std::abs(7 - (position + delta));
            // pushは外部状態を進めません。現在のpositionは変更せず候補だけを登録します。
            emit.push(Action{delta}, cost);
        }
    };
    // forwardとbackwardは完全な逆操作にします。別の探索枝への移動でも呼ばれます。
    auto forward = [&](const Action& action) { position += action.delta; };
    auto backward = [&](const Action& action) { position -= action.delta; };

    // delta型では、重複排除を使わなくてもinitial_hash引数が必要です。
    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(param, 7LL, Hash{0}, expand, forward, backward);
    assert(position == 0); // 正常終了時には外部状態が初期状態へ戻ります。
    if (!result.found || result.node_pool_exhausted) return 1;
    // 解を盤面に適用したい場合は、返されたAction列を自分で再生します。
    for (const Action& action : result.path) forward(action);
    assert(result.path.size() == 4 && position == 7 && result.best_cost == 0);
    std::cout << "position=" << position << " cost=" << result.best_cost << '\n';
}
```

確認する結果は`position=7 cost=0`です。

### 例05：外部の使用集合を更新しながら割当を探索する

例02と同じ割当問題をdelta型で解きます。外部状態は使用集合だけで、履歴はAction列としてライブラリへ任せます。Runtime付き`expand`、Hash付き`push`、CSV、結果の行動再生を使います。

仕事の使用開始・取り消しが完全な逆操作であることを確認します。`node_pool_exhausted`と`hash_table_full`が真なら予定どおりの探索とは限らないため、この例はエラーとして扱います。

全体1,000msを各turnへ配分し、`time_check_interval=1`で各expandの終了後に確認します。この小問題には余裕のある設定です。実問題で予算を短くして探索を打ち切る場合、末尾の固定スコアのassertも見直してください。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_delta_single_v36.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// Actionには、undoに必要な情報を残します。この問題では仕事番号だけで逆操作できます。
// デフォルト値はroot用です。rootのダミーActionをユーザーが適用する必要はありません。
struct Action {
    int job = -1;
};

int main() {
    const std::array<std::array<int, 4>, 4> fee{{
        {{9, 2, 7, 8}}, {{6, 4, 3, 7}}, {{5, 8, 1, 8}}, {{7, 6, 9, 4}}
    }};
    unsigned used = 0; // 現在の探索状態。履歴はライブラリのAction列が保持します。
    BS::Param param{};
    param.max_turn = 4; // 初期turn=0から到達できる最大turnです。
    param.beam_width = 16;         // 各使用集合を残せる幅を指定します。
    param.nodes_capacity = 0;      // 自動容量。小さく固定すると解の探索途中で容量切れになり得ます。
    param.hash_capacity = 0;       // 重複排除用の表は自動設定にします。
    param.use_hash_dedup = true;   // 同turnの同使用集合を統合します。
    param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
    param.time_limit_ms = 1000.0; // 探索全体の時間予算。小さい例なので十分な余裕を設けます。
    param.use_turn_time_limit = true; // 各turn開始時に、残り時間を残りturn数へ配分します。
    param.time_check_interval = 1; // 各expand終了後に時計を確認します。expand自体は中断しません。

    auto expand = [&](const BS::NodeView& now, const BS::Runtime& runtime,
                      BS::Emitter<Action>& emit) {
        (void)runtime; // Runtimeは読み取り専用で、統計や時間制限検出状況を参照できます。
        for (int job = 0; job < 4; ++job) {
            if (used & (1U << job)) continue;
            const Hash next_hash = static_cast<Hash>(used | (1U << job));
            emit.push(Action{job}, now.cost + fee[now.turn][job], next_hash);
        }
    };
    auto forward = [&](const Action& action) {
        assert((used & (1U << action.job)) == 0);
        used |= 1U << action.job;
    };
    auto backward = [&](const Action& action) {
        assert(used & (1U << action.job));
        used &= ~(1U << action.job);
    };

    BS::CsvStatHook csv("delta_single_assignment.csv", 0.0); // 全TurnEndを記録します。
    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(
        param, 0LL, Hash{0}, expand, forward, backward, csv
    );
    assert(used == 0); // 状態が戻ることも検査します。
    if (!result.found || result.node_pool_exhausted || result.hash_table_full) return 1;
    assert(result.path.size() == 4);
    long long actual_cost = 0;
    for (int person = 0; person < 4; ++person) {
        forward(result.path[person]);
        actual_cost += fee[person][result.path[person].job];
    }
    assert(used == 15U && actual_cost == result.best_cost && actual_cost == 13);
    std::cout << "cost=" << result.best_cost << " jobs=";
    for (const Action& action : result.path) std::cout << action.job << ' ';
    std::cout << '\n';
}
```

合計費用は13です。`delta_single_assignment.csv`も生成します。

### 例06：探索候補から落ちた途中の有効解を受け取る

ゴールに到達する経路を探します。低costの行き止まりと、costは高いが有効なゴールを同じ到着turnへ生成します。`beam_width=1`なので探索候補には行き止まりだけが残りますが、ゴールは解として独立に保存されます。`max_turn_is_answer=false`で、ゴールへ渡す`finished=true`だけを解の条件にします。

Actionは親と遷移先を持ち、前進・巻き戻しで外部状態を復元します。`push`の戻り値はfalseでも、`result.found`はtrueです。返却されたpathを初期状態から再生すると、探索候補に残らなかったゴールを取り出せます。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_delta_single_v36.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long>;

// node=0は出発点、1は低costだが行き止まり、2は有効なゴールです。
// 有効解の判定は利用側が行い、候補追加時のfinishedへ渡します。
// Actionには前進と巻き戻しに必要な情報を持たせます。
// コピー可能な型とし、探索中に消えるオブジェクトへの参照は入れません。
struct Action {
    int from = 0;
    int to = 0;
};

int main() {
    BS::Param param;
    param.max_turn = 5; // 最大5turnまで探索できます。ゴールはそれより前にあります。
    param.beam_width = 1; // 探索候補は1件。cost=0の行き止まりが残ります。
    param.max_turn_is_answer = false; // 最大turnへの到達だけでは解にしません。
    param.time_limit_ms = 0.0; // この小さな例は時間無制限で実行します。
    param.nodes_capacity = 0; // 探索木の容量はライブラリの自動設定を使います。

    bool selected = true;
    int goal_expansions = 0;
    int current_node = 0; // ライブラリの状態移動に合わせて更新する外部状態です。

    auto expand = [&](const BS::NodeView& now, auto& emit) {
        assert(now.turn == 0 || now.turn == 1);
        if (current_node == 2) ++goal_expansions;
        if (current_node != 0) return; // 行き止まりとゴールからは子を生成しません。

        // 先に、低costの非有効解でビームを埋めます。
        emit.push(Action{0, 1}, 0LL, false);

        // ゴールはcost=10なので、探索候補としては選別で落ちます。
        // finished=trueの有効解としては、選別とは独立に保存されます。
        selected = emit.push(Action{0, 2}, 10LL, true);
    };

    // 前進・巻き戻しは互いに逆の操作にします。
    auto forward = [&](const Action& action) {
        assert(current_node == action.from);
        current_node = action.to;
    };
    auto backward = [&](const Action& action) {
        assert(current_node == action.to);
        current_node = action.from;
    };

    // deltaのrunでは初期Hashも渡します。Hashなしpushは重複排除の対象外です。
    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(param, 0LL, std::uint64_t{0}, expand, forward, backward);
    assert(current_node == 0); // run終了時には外部状態が初期状態へ戻ります。
    assert(!selected); // pushのboolは、探索候補としての採用結果です。
    assert(result.found && result.path.size() == 1);

    // 保存された操作列を、初期状態から再生して解を取り出します。
    for (const auto& action : result.path) forward(action);
    assert(current_node == 2);
    assert(goal_expansions == 0); // ゴールを探索候補へ残さなくても、解は得られます。
    assert(result.best_cost == 10 && result.best_turn == 1);
    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " selected=" << selected << '\n';
}
```

出力は`found=1 best_cost=10 best_turn=1 selected=0`です。

## 5. beam_copy_multi_v34.hpp

### 例07：所要時間の違う作業を組み合わせる

1時間で利益1、2時間で利益5、3時間で利益8を得る作業を何度でも選べるとし、合計6時間の利益を最大にします。作業回数ではなく、経過時間が`turn`です。同じ6時間の候補同士で比較するため、multi版を使います。

`max_step=3`と、`push`の`int duration`が重要です。最大時間を超える候補はライブラリ側も棄却しますが、サンプルではState作成前に省きます。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include "beam_copy_multi_v34.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, std::uint64_t>;

// copy型なので、利益と、出力に必要な作業時間の履歴をStateに入れます。
// 独立してコピー・代入できる値型にします。
struct State {
    long long profit = 0;
    std::vector<int> durations;
};

int main() {
    // 1時間・2時間・3時間の作業は何度でも選べます。合計6時間で利益を最大化します。
    const std::array<int, 4> reward{0, 1, 5, 8};
    BS::Param param{};
    param.max_turn = 6;            // turnは作業数でなく、経過時間です。
    param.max_step = 3;            // 1作業で最大3時間進みます。0時間の遷移は使えません。
    param.beam_width = 16;         // 同じ到着時刻の候補を比較し、最大16状態を残します。
    param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。
    param.max_turn_is_answer = true; // ちょうど6時間に到着した状態を有効解にします。
    param.use_hash_dedup = false;  // この基本例では履歴ごとに候補を保持します。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int duration = 1; duration <= 3; ++duration) {
            if (duration > param.max_turn - now.turn) continue;
            State next = now.state;
            next.profit += reward[duration];
            next.durations.push_back(duration);
            // 最大化なのでcost=-profit。multi版では末尾にint型のstepを渡します。
            emit.push(next, -next.profit, duration);
        }
    };

    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, expand);
    if (!result.found) return 1;
    const State& answer = *result.best_state;
    int elapsed = 0;
    long long profit = 0;
    for (int duration : answer.durations) {
        elapsed += duration;
        profit += reward[duration];
    }
    assert(elapsed == 6 && result.best_turn == 6);
    assert(profit == 16 && profit == answer.profit && result.best_cost == -profit);
    std::cout << "hours=" << elapsed << " profit=" << profit << '\n';
}
```

3時間の作業を2回行い、`hours=6 profit=16`になります。

### 例08：時間を使い切らなくてもよい作業選択

各作業を高々1回選び、5時間以内の利益を最大にします。各作業終了時に停止してよいので、毎回`finished=true`を渡します。それでも、次の作業を追加する探索は続きます。

`max_turn_is_answer=false`で有効解条件を明示し、Hashには使用集合を使います。`push_lazy`の引数は`cost, hash, step, finished, maker`です。初期の空集合も解候補にしたい一般形では、利用側で初期解を保持して比較します。この例は全利益が正で、実際に作業を選んだ最良解を確認します。

全体1,000msを各turnへ配分し、`time_check_interval=1`で各expandの終了後に確認します。この小問題には余裕のある設定です。実問題で予算を短くして探索を打ち切る場合、末尾の固定スコアのassertも見直してください。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include "beam_copy_multi_v34.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// 同じusedなら作業済み集合が同じで、次に選べる作業も同じです。
// jobsは答えを出力するための履歴です。コピー可能な型にします。
struct State {
    unsigned used = 0;
    long long profit = 0;
    std::vector<int> jobs;
};

int main() {
    const std::array<int, 4> duration{2, 3, 1, 2};
    const std::array<int, 4> reward{6, 8, 2, 5};
    BS::Param param{};
    param.max_turn = 5;             // 使える時間の上限です。5時間未満の解も認めます。
    param.max_step = 3;             // 最も長い作業の所要時間です。
    param.beam_width = 16;          // この例では全使用集合を扱える幅です。
    param.max_turn_is_answer = false; // 有効解判定はfinishedで明示します。
    param.use_hash_dedup = true; // Hashを明示した候補の重複を、同じ保存先内で除きます。
    param.hash_capacity = 0;        // Hash表容量は自動設定にします。
    param.time_limit_ms = 1000.0; // 探索全体の時間予算。小さい例なので十分な余裕を設けます。
    param.use_turn_time_limit = true; // 各turn開始時に、残り時間を残りturn数へ配分します。
    param.time_check_interval = 1; // 各expand終了後に時計を確認します。expand自体は中断しません。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const int step = duration[job]; // stepは必ずint型にします。
            if (step > param.max_turn - now.turn) continue;
            const unsigned used = now.state.used | (1U << job);
            const long long profit = now.state.profit + reward[job];
            // 各作業終了時点で停止してよいためfinished=trueです。
            // それでも探索は続きます。さらに作業を追加した解も比較します。
            emit.push_lazy(-profit, static_cast<Hash>(used), step, true,
                           [&now, used, profit, job]() {
                State next = now.state;
                next.used = used;
                next.profit = profit;
                next.jobs.push_back(job);
                return next;
            }); // makerは探索候補か最良有効解の保存に必要な場合だけ、同期的に1回実行されます。
        }
    };

    BS::CsvStatHook csv("copy_multi_jobs.csv", 0.0); // 全TurnEndを記録します。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) return 1;
    int hours = 0;
    long long profit = 0;
    unsigned used = 0;
    for (int job : result.best_state->jobs) {
        assert((used & (1U << job)) == 0);
        used |= 1U << job;
        hours += duration[job];
        profit += reward[job];
    }
    assert(hours <= 5 && hours == result.best_turn);
    assert(profit == 14 && result.best_cost == -profit);
    std::cout << "hours=" << hours << " profit=" << profit << '\n';
}
```

最良利益は14です。`copy_multi_jobs.csv`も生成します。

### 例09：探索候補から落ちた途中の有効解を受け取る

ゴールに到達する経路を探します。低costの行き止まりと、costは高いが有効なゴールを同じ到着turnへ生成します。`beam_width=1`なので探索候補には行き止まりだけが残りますが、ゴールは解として独立に保存されます。`max_turn_is_answer=false`で、ゴールへ渡す`finished=true`だけを解の条件にします。

ゴールは`push_lazy`で生成します。戻り値がfalseでも、最良解の保存に必要なのでmakerは1回呼ばれます。`result.found`を確認し、`best_state`から解を取得します。この例のStateは小さな値だけですが、操作列が必要ならStateにその履歴も持たせます。

`step=2`は消費turn数です。到着待ちの間も解は保存され、`best_turn`は2になります。`max_step=2`と、到着先が`max_turn`以内であることを守ります。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_copy_multi_v34.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long>;

// node=0は出発点、1は低costだが行き止まり、2は有効なゴールです。
// 有効解の判定は利用側が行い、候補追加時のfinishedへ渡します。
struct State {
    int node = 0;
};

int main() {
    BS::Param param;
    param.max_turn = 5; // 最大5turnまで探索できます。ゴールはそれより前にあります。
    param.beam_width = 1; // 探索候補は1件。cost=0の行き止まりが残ります。
    param.max_turn_is_answer = false; // 最大turnへの到達だけでは解にしません。
    param.time_limit_ms = 0.0; // この小さな例は時間無制限で実行します。
    param.max_step = 2; // 1操作で最大2turn進めます。この例は2turnの遷移を使います。

    bool selected = true;
    int goal_expansions = 0;
    int maker_calls = 0;

    auto expand = [&](const BS::StateView<State>& now, auto& emit) {
        if (now.state.node == 2) ++goal_expansions;
        if (now.state.node != 0) return; // 行き止まりとゴールからは子を生成しません。

        // 先に、低costの非有効解でビームを埋めます。
        emit.push(State{1}, 0LL, 2, false);

        // ゴールはcost=10なので、探索候補としては選別で落ちます。
        // finished=trueの有効解としては、選別とは独立に保存されます。
        selected = emit.push_lazy(10LL, 2, true, [&] {
            ++maker_calls;
            return State{2}; // makerは解の保存のために1回だけ呼ばれます。
        });
    };

    // 初期State、初期cost、候補生成処理を渡します。Hashは使っていません。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run(param, State{}, 0LL, expand);
    assert(!selected); // push_lazyのboolは、探索候補としての採用結果です。
    assert(maker_calls == 1);
    assert(result.found && result.best_state->node == 2);
    assert(goal_expansions == 0); // ゴールを探索候補へ残さなくても、解は得られます。
    assert(result.best_cost == 10 && result.best_turn == 2);
    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " selected=" << selected << '\n';
}
```

出力は`found=1 best_cost=10 best_turn=2 selected=0`です。

## 6. beam_delta_multi_v59.hpp

### 例10：Edgeの時刻情報を使って外部状態を戻す

例07の作業選択をdelta型で解きます。`Edge`にはActionに加えて出発turnとstepがあるため、時間を含む外部状態の整合性を確認できます。`edge.next_turn()`で到着時刻を求めます。

`CapacityPolicy::Safe`と自動容量を使います。`result.path.size()`は作業数で、6時間という`best_turn`とは別の量です。返されたEdge列を再生して、利益と時間を検算します。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_delta_multi_v59.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// Actionは作業による利益増加を保持します。所要時間はEdge.stepに保存されます。
// root用のデフォルト構築と、コピー・代入が可能な型にします。
struct Action {
    int reward = 0;
};

int main() {
    const std::array<int, 4> rewards{0, 1, 5, 8};
    int hours = 0;
    long long profit = 0; // 外部状態はmove callbackが探索位置へ合わせます。
    BS::Param param{};
    param.max_turn = 6;            // turnを経過時間として使います。
    param.max_step = 3;            // 1作業は1〜3時間です。
    param.beam_width = 16; // 同じturnの候補を保持する上限です。
    param.nodes_capacity = 0;      // capacity_policyに従って自動設定します。
    param.capacity_policy = BS::CapacityPolicy::Safe; // まずは容量不足を避ける設定にします。
    param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
    param.use_hash_dedup = false; // この例ではHashによる重複排除を使いません。
    param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。

    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        assert(hours == now.turn); // 外部状態とライブラリの時刻が一致することを確認します。
        for (int duration = 1; duration <= 3; ++duration) {
            if (duration > param.max_turn - now.turn) continue;
            emit.push(Action{rewards[duration]}, -(profit + rewards[duration]), duration);
        }
    };
    // multiのEdgeは出発turn、step、actionを持ちます。next_turn()は到着turnです。
    auto forward = [&](const BS::Edge<Action>& edge) {
        assert(hours == edge.turn);
        hours = edge.next_turn();
        profit += edge.action.reward;
    };
    auto backward = [&](const BS::Edge<Action>& edge) {
        assert(hours == edge.next_turn());
        profit -= edge.action.reward;
        hours = edge.turn;
    };

    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(param, 0LL, Hash{0}, expand, forward, backward);
    assert(hours == 0 && profit == 0); // 正常終了時に初期状態へ戻ります。
    if (!result.found || result.node_pool_exhausted) return 1;
    // multiのpathはvector<Edge<Action>>です。Action数と経過turn数は一致しません。
    for (const auto& edge : result.path) forward(edge);
    assert(hours == 6 && profit == 16 && result.best_cost == -profit);
    std::cout << "hours=" << hours << " profit=" << profit
              << " actions=" << result.path.size() << '\n';
}
```

確認する結果は`hours=6 profit=16 actions=2`です。

### 例11：Actionだけを受け取るcallbackと早期有効解

例08の作業選択を、使用集合と利益の外部状態で表します。時間を外部状態として持たないため、移動callbackは`const Action&`を受け取ります。multi版でもこの形が使えますが、結果のpathの要素は引き続き`Edge<Action>`です。

Hash付き`push`、途中での`finished=true`、CSVを組み合わせます。容量不足のフラグを確認し、解のEdge列の時間と作業番号を検算します。

全体1,000msを各turnへ配分し、`time_check_interval=1`で各expandの終了後に確認します。この小問題には余裕のある設定です。実問題で予算を短くして探索を打ち切る場合、末尾の固定スコアのassertも見直してください。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_delta_multi_v59.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// この問題は仕事番号だけで元へ戻せます。不可逆な更新なら変更前の値も保存します。
struct Action {
    int job = -1; // root用にデフォルト構築できるようにします。
};

int main() {
    const std::array<int, 4> duration{2, 3, 1, 2};
    const std::array<int, 4> reward{6, 8, 2, 5};
    unsigned used = 0;
    long long profit = 0;
    BS::Param param{};
    param.max_turn = 5;            // 5時間以内で作業を選びます。
    param.max_step = 3; // 1回の行動で進められるturn数の上限です。
    param.beam_width = 16; // 同じturnの候補を保持する上限です。
    param.nodes_capacity = 0; // ノード容量は自動設定にします。
    param.capacity_policy = BS::CapacityPolicy::Safe; // 自動ノード容量を安全側の見積もりで決めます。
    param.hash_capacity = 0; // Hash表容量はビーム幅を基準に自動設定します。
    param.use_hash_dedup = true;   // 同じ到着turn・使用集合なら同じ将来条件です。
    param.max_turn_is_answer = false; // 有効解はfinished=trueで明示します。
    param.time_limit_ms = 1000.0; // 探索全体の時間予算。小さい例なので十分な余裕を設けます。
    param.use_turn_time_limit = true; // 各turn開始時に、残り時間を残りturn数へ配分します。
    param.time_check_interval = 1; // 各expand終了後に時計を確認します。expand自体は中断しません。

    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (used & (1U << job)) continue;
            const int step = duration[job];
            if (step > param.max_turn - now.turn) continue;
            const Hash hash = static_cast<Hash>(used | (1U << job));
            // 各作業後に停止してよいのでfinished=true。これは探索停止命令ではありません。
            emit.push(Action{job}, -(profit + reward[job]), hash, step, true);
        }
    };
    // 時刻が外部状態に不要なら、multi版でもActionだけを受け取れます。
    auto forward = [&](const Action& action) {
        assert((used & (1U << action.job)) == 0);
        used |= 1U << action.job;
        profit += reward[action.job];
    };
    auto backward = [&](const Action& action) {
        used &= ~(1U << action.job);
        profit -= reward[action.job];
    };

    BS::CsvStatHook csv("delta_multi_jobs.csv", 0.0); // 状態移動数や容量使用量も解析できます。
    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(param, 0LL, Hash{0}, expand, forward, backward, csv);
    assert(used == 0 && profit == 0);
    if (!result.found || result.node_pool_exhausted || result.hash_table_full) return 1;
    int hours = 0;
    // callbackがAction型でも、戻り値のpathの要素はEdge<Action>です。
    for (const auto& edge : result.path) {
        assert(edge.turn == hours && edge.step == duration[edge.action.job]);
        hours = edge.next_turn();
        forward(edge.action);
    }
    assert(hours <= 5 && hours == result.best_turn && profit == 14);
    assert(result.best_cost == -profit);
    std::cout << "hours=" << hours << " profit=" << profit << '\n';
}
```

最良利益は14です。`delta_multi_jobs.csv`も生成します。

### 例12：探索候補から落ちた途中の有効解を受け取る

ゴールに到達する経路を探します。低costの行き止まりと、costは高いが有効なゴールを同じ到着turnへ生成します。`beam_width=1`なので探索候補には行き止まりだけが残りますが、ゴールは解として独立に保存されます。`max_turn_is_answer=false`で、ゴールへ渡す`finished=true`だけを解の条件にします。

Actionは親と遷移先を持ち、前進・巻き戻しで外部状態を復元します。`push`の戻り値はfalseでも、`result.found`はtrueです。返却されたpathを初期状態から再生すると、探索候補に残らなかったゴールを取り出せます。

`step=2`は消費turn数です。到着待ちの間も解は保存され、`best_turn`は2になります。`max_step=2`と、到着先が`max_turn`以内であることを守ります。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_delta_multi_v59.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long>;

// node=0は出発点、1は低costだが行き止まり、2は有効なゴールです。
// 有効解の判定は利用側が行い、候補追加時のfinishedへ渡します。
// Actionには前進と巻き戻しに必要な情報を持たせます。
// コピー可能な型とし、探索中に消えるオブジェクトへの参照は入れません。
struct Action {
    int from = 0;
    int to = 0;
};

int main() {
    BS::Param param;
    param.max_turn = 5; // 最大5turnまで探索できます。ゴールはそれより前にあります。
    param.beam_width = 1; // 探索候補は1件。cost=0の行き止まりが残ります。
    param.max_turn_is_answer = false; // 最大turnへの到達だけでは解にしません。
    param.time_limit_ms = 0.0; // この小さな例は時間無制限で実行します。
    param.max_step = 2; // 1操作で最大2turn進めます。この例は2turnの遷移を使います。
    param.nodes_capacity = 0; // 探索木の容量はライブラリの自動設定を使います。

    bool selected = true;
    int goal_expansions = 0;
    int current_node = 0; // ライブラリの状態移動に合わせて更新する外部状態です。

    auto expand = [&](const BS::NodeView& now, auto& emit) {
        assert(now.turn == 0 || now.turn == 2);
        if (current_node == 2) ++goal_expansions;
        if (current_node != 0) return; // 行き止まりとゴールからは子を生成しません。

        // 先に、低costの非有効解でビームを埋めます。
        emit.push(Action{0, 1}, 0LL, 2, false);

        // ゴールはcost=10なので、探索候補としては選別で落ちます。
        // finished=trueの有効解としては、選別とは独立に保存されます。
        selected = emit.push(Action{0, 2}, 10LL, 2, true);
    };

    // 前進・巻き戻しは互いに逆の操作にします。
    auto forward = [&](const BS::Edge<Action>& edge) {
        const Action& action = edge.action; // Edgeには操作と開始turn・消費turnが入ります。
        assert(current_node == action.from);
        current_node = action.to;
    };
    auto backward = [&](const BS::Edge<Action>& edge) {
        const Action& action = edge.action;
        assert(current_node == action.to);
        current_node = action.from;
    };

    // deltaのrunでは初期Hashも渡します。Hashなしpushは重複排除の対象外です。
    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(param, 0LL, std::uint64_t{0}, expand, forward, backward);
    assert(current_node == 0); // run終了時には外部状態が初期状態へ戻ります。
    assert(!selected); // pushのboolは、探索候補としての採用結果です。
    assert(result.found && result.path.size() == 1);

    // 保存された操作列を、初期状態から再生して解を取り出します。
    for (const auto& edge : result.path) forward(edge);
    assert(current_node == 2);
    assert(goal_expansions == 0); // ゴールを探索候補へ残さなくても、解は得られます。
    assert(result.best_cost == 10 && result.best_turn == 2);
    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " selected=" << selected << '\n';
}
```

出力は`found=1 best_cost=10 best_turn=2 selected=0`です。

## 7. chokudai_copy_single_v06.hpp

### 例13：最初に後回しにした候補を次スイープで展開する

途中評価では経路Aが良いのに、完成費用はAが100、Bが1になる小さな探索木です。`beam_width=2`で両方を残し、`chokudai_width=1`で1件ずつ展開します。スイープ上限1と2で独立に探索し、後回しの候補を展開する効果を確認します。

この例は、途中costが完成費用の下界である必要がないことも示します。`run`を2回呼ぶのは前回探索の再開ではなく、同じ初期状態からの比較です。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_single_v06.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopySingle<long long, std::uint64_t>;

// id=0が初期状態、1と2が途中経路、3と4がそれぞれの完成状態です。
// Stateはコピー・代入できる型にします。必要な履歴もStateに保存します。
struct State {
    int id = 0;
};

int main() {
    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        if (now.state.id == 0) {
            emit.push(State{1}, 0LL); // 途中評価は良いが、完成時は費用100になる経路です。
            emit.push(State{2}, 5LL); // 途中評価は悪いが、完成時は費用1になる経路です。
        } else if (now.state.id == 1) {
            emit.push(State{3}, 100LL);
        } else if (now.state.id == 2) {
            emit.push(State{4}, 1LL);
        }
        // single版なので各pushで必ず1turn進みます。
    };

    for (int sweeps : {1, 2}) {
        BS::Param param{};
        param.max_turn = 2; // 初期turn=0から到達できる最大turnです。
        param.beam_width = 2;      // turn=1に両方の途中経路を残せる容量です。
        param.chokudai_width = 1;  // 各turnから1スイープで展開するのは1候補だけです。
        param.max_sweeps = sweeps; // 今回は時間でなくスイープ数を制限します。
        param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。
        param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
        param.use_hash_dedup = false; // この例ではHashによる重複排除を使いません。

        // runごとに独立した探索が始まります。前回の候補を引き継ぐ呼び出しではありません。
        // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
        const auto result = BS::run<State>(param, State{}, 0LL, expand);
        if (!result.found) return 1;
        const long long expected = sweeps == 1 ? 100 : 1;
        assert(result.best_cost == expected);
        assert(result.best_state->id == (sweeps == 1 ? 3 : 4));
        // 2回目のスイープで、turn=1に残っていたid=2を展開できたことが分かります。
        std::cout << "max_sweeps=" << sweeps << " cost=" << result.best_cost << '\n';
    }
}
```

1スイープでは費用100、2スイープでは費用1になります。

### 例14：割当問題を少しずつ展開し、CSVで改善を追う

割当問題へHashと`push_lazy`を使い、各turnから1候補ずつ展開します。`beam_width`は保持量、`chokudai_width`は1スイープの展開量です。両者を区別してください。

この小さな例は`time_limit_ms=0`、`max_sweeps=0`で候補切れまで進めます。時間を制限する場合に備え、`finish_on_timeout=true`も明示していますが、時間制限が0のこの実行では救済は発動しません。正の時間制限へ変えると、同じ最適値や`exhausted`は保証できないため、末尾の固定値確認も用途に合わせて変更します。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include "chokudai_copy_single_v06.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopySingle<long long, Hash>;

// 使用集合は今後の選択に必要で、assignmentは出力用です。
// Stateはコピー・代入可能にします。大きなStateではpush_lazyが有効です。
struct State {
    unsigned used = 0;
    std::vector<int> assignment;
};

int main() {
    const std::array<std::array<int, 4>, 4> fee{{
        {{9, 2, 7, 8}}, {{6, 4, 3, 7}}, {{5, 8, 1, 8}}, {{7, 6, 9, 4}}
    }};
    BS::Param param{};
    param.max_turn = 4; // 初期turn=0から到達できる最大turnです。
    param.beam_width = 16;         // 各turnの未展開候補の保持上限です。
    param.chokudai_width = 1;      // 少数ずつ先へ進め、次のスイープで別の候補を展開します。
    param.max_sweeps = 0;          // スイープ数は無制限。有限な小問題なので候補切れで終わります。
    param.time_limit_ms = 0.0;     // 実問題では、たとえば100.0で全体の時間を制限できます。
    param.finish_on_timeout = true; // 解なしで時間切れなら幅1の追加スイープを高々1回行います。
    param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
    param.use_hash_dedup = true;   // 同turnの未展開候補だけが重複排除の対象です。
    param.hash_capacity = 0; // Hash表容量はビーム幅を基準に自動設定します。

    auto expand = [&](const BS::StateView<State>& now, const BS::Runtime& runtime,
                      BS::Emitter<State>& emit) {
        (void)runtime; // runtime.sweepは現在のスイープ番号です。時刻は直近の計測値です。
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const unsigned used = now.state.used | (1U << job);
            const long long cost = now.cost + fee[now.turn][job];
            emit.push_lazy(cost, static_cast<Hash>(used), [&now, used, job]() {
                // makerは採用時に同期実行されます。参照先nowを後から使うことはありません。
                State next = now.state;
                next.used = used;
                next.assignment.push_back(job);
                return next;
            });
        }
    };

    BS::CsvStatHook csv("chokudai_single_assignment.csv", 0.0); // 全SweepEndを記録します。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) return 1;
    unsigned used = 0;
    long long actual_cost = 0;
    assert(result.best_state->assignment.size() == 4);
    for (int person = 0; person < 4; ++person) {
        const int job = result.best_state->assignment[person];
        assert((used & (1U << job)) == 0);
        used |= 1U << job;
        actual_cost += fee[person][job];
    }
    assert(actual_cost == 13 && actual_cost == result.best_cost);
    assert(result.exhausted); // 時間・回数を制限していないので、この例は候補切れで終わります。
    std::cout << "cost=" << result.best_cost << " sweeps=" << result.runtime.sweep << '\n';
}
```

合計費用は13です。`chokudai_single_assignment.csv`でスイープ単位の進捗を確認できます。

### 例15：幅で落ちた途中の有効解を保存する

低costの行き止まりと、高costのゴールをこの順に生成します。`beam_width=1`なので、ゴールは探索キューへ受理されません。それでも`finished=true`で生成した最良有効解として保存され、未展開候補が尽きた後に受け取れます。

`max_turn_is_answer=false`は利用側がゴールを判定する設定です。`chokudai_width=1`は各turnで1件ずつ展開し、`max_sweeps=0`は回数制限を設けません。singleでは1回の遷移で必ず次のturnへ進みます。

`push_lazy`のfalseは探索キューへの棄却を意味します。makerは解の保存に必要なので1回呼ばれます。`result.found`を確認してから`best_state`を使います。Hashによる棄却でも、改善する有効解は同じように保存されます。履歴が必要な問題では、Stateに操作列などを持たせてください。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_single_v06.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopySingle<long long>;

// node=0は出発点、1は低costだが行き止まり、2は有効なゴールです。
// 有効解の判定は利用側が行い、候補追加時のfinishedへ渡します。
struct State {
    int node = 0;
};

int main() {
    BS::Param param;
    param.max_turn = 5; // 最大5turnまで探索できます。ゴールはそれより前にあります。
    param.beam_width = 1; // 探索候補は1件。cost=0の行き止まりが残ります。
    param.max_turn_is_answer = false; // 最大turnへの到達だけでは解にしません。
    param.time_limit_ms = 0.0; // この小さな例は時間無制限で実行します。

    param.chokudai_width = 1; // 1スイープで各turnから1件ずつ展開します。
    param.max_sweeps = 0; // 回数無制限。未展開候補が尽きれば終了します。

    bool selected = true;
    int goal_expansions = 0;
    int maker_calls = 0;

    auto expand = [&](const BS::StateView<State>& now, auto& emit) {
        if (now.state.node == 2) ++goal_expansions;
        if (now.state.node != 0) return; // 行き止まりとゴールからは子を生成しません。

        // 先に、低costの非有効解でビームを埋めます。
        emit.push(State{1}, 0LL, false);

        // ゴールはcost=10なので、探索候補としては選別で落ちます。
        // finished=trueの有効解としては、選別とは独立に保存されます。
        selected = emit.push_lazy(10LL, true, [&] {
            ++maker_calls;
            return State{2}; // makerは解の保存のために1回だけ呼ばれます。
        });
    };

    // 初期State、初期cost、候補生成処理を渡します。Hashは使っていません。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run(param, State{}, 0LL, expand);
    assert(!selected); // push_lazyのboolは、探索候補としての採用結果です。
    assert(maker_calls == 1);
    assert(result.found && result.best_state->node == 2);
    assert(goal_expansions == 0); // ゴールを探索候補へ残さなくても、解は得られます。
    assert(result.best_cost == 10 && result.best_turn == 1);
    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " selected=" << selected << '\n';
}
```

cost=10のゴールが得られ、`selected=0`になります。保存したゴールはビーム幅を消費せず、この例では展開もされません。

## 8. chokudai_copy_multi_v07.hpp

### 例16：可変時間の経路と、上限より前の目的地到着

最初の移動は2時間で、次の移動が経路によって1時間または2時間かかります。turn=3で目的地へ着いた状態も、turn=4で着いた状態も有効解です。`max_turn_is_answer=false`と`finished=true`で目的地到着を明示します。

同じturn=2に残した経路をスイープごとに展開するため、1回目と2回目で解が改善します。目的地の状態は候補を出さず、余分な遷移を防ぎます。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_multi_v07.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopyMulti<long long, std::uint64_t>;

// 0が出発点、1と2が経路の途中、3と4が目的地です。
// Stateはコピー・代入可能にし、将来の分岐に必要な情報を持たせます。
struct State {
    int id = 0;
};

int main() {
    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        if (now.state.id == 0) {
            // 引数順はstate, cost, step。2時間後の同じturnのキューへ入ります。
            emit.push(State{1}, 0LL, 2);
            emit.push(State{2}, 5LL, 2);
        } else if (now.state.id == 1) {
            emit.push(State{3}, 100LL, 2, true); // turn=4の目的地です。
        } else if (now.state.id == 2) {
            emit.push(State{4}, 1LL, 1, true);   // turn=3でもfinishedにより有効解です。
        }
        // 目的地id=3,4は候補を出さずに終了します。
        // finishedだけでは展開が止まらないため、ユーザー側でこの終端処理が必要です。
    };

    for (int sweeps : {1, 2}) {
        BS::Param param{};
        param.max_turn = 4;             // 到着可能な時刻の上限です。
        param.max_step = 2;             // 1回で進めるのは最大2時間です。
        param.beam_width = 2; // 同じturnの候補を保持する上限です。
        param.chokudai_width = 1;       // turn=2の残り候補を次スイープへ持ち越します。
        param.max_sweeps = sweeps; // 探索を続けるスイープ数の上限です。
        param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。
        param.max_turn_is_answer = false; // 目的地到着だけをfinishedで有効解にします。
        param.use_hash_dedup = false; // この例ではHashによる重複排除を使いません。

        // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
        const auto result = BS::run<State>(param, State{}, 0LL, expand);
        if (!result.found) return 1;
        assert(result.best_cost == (sweeps == 1 ? 100 : 1));
        assert(result.best_turn == (sweeps == 1 ? 4 : 3));
        std::cout << "max_sweeps=" << sweeps << " cost=" << result.best_cost
                  << " arrival=" << result.best_turn << '\n';
    }
}
```

1スイープでは費用100・到着4、2スイープでは費用1・到着3になります。

### 例17：時間制約付き作業選択をHashと遅延生成で探索する

5時間以内の作業選択をchokudai multiで解きます。異なる所要時間を`step`で指定し、同じ到着turnに残った候補を少しずつ展開します。各作業終了時点を有効解として扱います。

展開済み状態を永久に記録する方式ではないため、同じ使用集合が後から再生成されることもあります。この例はturnが必ず進み、作業の重複使用も禁止しているので、無制限設定でも有限に終わります。

```cpp
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
#include "chokudai_copy_multi_v07.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopyMulti<long long, Hash>;

// usedは将来の選択肢、profitは目的値、jobsは出力用の履歴です。
// Stateはコピー・代入可能な値型にします。
struct State {
    unsigned used = 0;
    long long profit = 0;
    std::vector<int> jobs;
};

int main() {
    const std::array<int, 4> duration{2, 3, 1, 2};
    const std::array<int, 4> reward{6, 8, 2, 5};
    BS::Param param{};
    param.max_turn = 5;            // turnは累計作業時間です。
    param.max_step = 3;            // 最長の作業は3時間です。
    param.beam_width = 16;         // 各到着turnの未展開候補の上限です。
    param.chokudai_width = 1;      // 1スイープでは各turnから1候補ずつ展開します。
    param.max_sweeps = 0;          // この有限な例は候補が尽きるまで探索します。
    param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。
    param.finish_on_timeout = true; // 正の時間制限を使う場合の、未発見時の救済設定です。
    param.max_turn_is_answer = false; // 有効解はfinished=trueで明示します。
    param.use_hash_dedup = true;   // 展開済みのHashを永久に禁止する設定ではありません。
    param.hash_capacity = 0; // Hash表容量はビーム幅を基準に自動設定します。

    auto expand = [&](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        for (int job = 0; job < 4; ++job) {
            if (now.state.used & (1U << job)) continue;
            const int step = duration[job];
            if (step > param.max_turn - now.turn) continue;
            const unsigned used = now.state.used | (1U << job);
            const long long profit = now.state.profit + reward[job];
            // 各作業後の状態が有効解です。さらに追加作業を行う探索も継続します。
            // makerは探索候補または最良解の保存時に1回呼ばれ、不要なvectorのコピーを省きます。
            emit.push_lazy(-profit, static_cast<Hash>(used), step, true,
                           [&now, used, profit, job]() {
                State next = now.state;
                next.used = used;
                next.profit = profit;
                next.jobs.push_back(job);
                return next;
            });
        }
    };

    BS::CsvStatHook csv("chokudai_multi_jobs.csv", 0.0); // 各SweepEndの改善状況を記録します。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) return 1;
    unsigned used = 0;
    int hours = 0;
    long long profit = 0;
    for (int job : result.best_state->jobs) {
        assert((used & (1U << job)) == 0);
        used |= 1U << job;
        hours += duration[job];
        profit += reward[job];
    }
    assert(hours <= 5 && hours == result.best_turn);
    assert(profit == 14 && result.best_cost == -profit && result.exhausted);
    std::cout << "hours=" << hours << " profit=" << profit << '\n';
}
```

最良利益は14です。`chokudai_multi_jobs.csv`も生成します。

### 例18：幅で落ちた途中の有効解を保存する

低costの行き止まりと、高costのゴールをこの順に生成します。`beam_width=1`なので、ゴールは探索キューへ受理されません。それでも`finished=true`で生成した最良有効解として保存され、未展開候補が尽きた後に受け取れます。

`max_turn_is_answer=false`は利用側がゴールを判定する設定です。`chokudai_width=1`は各turnで1件ずつ展開し、`max_sweeps=0`は回数制限を設けません。multiではstep=2で2turn先へ進み、best_turnにはその到着turnが入ります。

`push_lazy`のfalseは探索キューへの棄却を意味します。makerは解の保存に必要なので1回呼ばれます。`result.found`を確認してから`best_state`を使います。Hashによる棄却でも、改善する有効解は同じように保存されます。履歴が必要な問題では、Stateに操作列などを持たせてください。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "chokudai_copy_multi_v07.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopyMulti<long long>;

// node=0は出発点、1は低costだが行き止まり、2は有効なゴールです。
// 有効解の判定は利用側が行い、候補追加時のfinishedへ渡します。
struct State {
    int node = 0;
};

int main() {
    BS::Param param;
    param.max_turn = 5; // 最大5turnまで探索できます。ゴールはそれより前にあります。
    param.beam_width = 1; // 探索候補は1件。cost=0の行き止まりが残ります。
    param.max_turn_is_answer = false; // 最大turnへの到達だけでは解にしません。
    param.time_limit_ms = 0.0; // この小さな例は時間無制限で実行します。
    param.max_step = 2; // 1操作で最大2turn進めます。この例は2turnの遷移を使います。

    param.chokudai_width = 1; // 1スイープで各turnから1件ずつ展開します。
    param.max_sweeps = 0; // 回数無制限。未展開候補が尽きれば終了します。

    bool selected = true;
    int goal_expansions = 0;
    int maker_calls = 0;

    auto expand = [&](const BS::StateView<State>& now, auto& emit) {
        if (now.state.node == 2) ++goal_expansions;
        if (now.state.node != 0) return; // 行き止まりとゴールからは子を生成しません。

        // 先に、低costの非有効解でビームを埋めます。
        emit.push(State{1}, 0LL, 2, false);

        // ゴールはcost=10なので、探索候補としては選別で落ちます。
        // finished=trueの有効解としては、選別とは独立に保存されます。
        selected = emit.push_lazy(10LL, 2, true, [&] {
            ++maker_calls;
            return State{2}; // makerは解の保存のために1回だけ呼ばれます。
        });
    };

    // 初期State、初期cost、候補生成処理を渡します。Hashは使っていません。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run(param, State{}, 0LL, expand);
    assert(!selected); // push_lazyのboolは、探索候補としての採用結果です。
    assert(maker_calls == 1);
    assert(result.found && result.best_state->node == 2);
    assert(goal_expansions == 0); // ゴールを探索候補へ残さなくても、解は得られます。
    assert(result.best_cost == 10 && result.best_turn == 2);
    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " selected=" << selected << '\n';
}
```

cost=10のゴールが得られ、`selected=0`になります。保存したゴールはビーム幅を消費せず、この例では展開もされません。

## 9. beam_copy_sub_v04.hpp

### 例19：任意回数の研修を行ってから生産する

生産を2回行います。初期レベルは1で、研修は費用2でレベルを1上げ、上限は3です。生産すると、その時点のレベルの5倍の利益を得ます。研修を何回行うかは任意です。

`Next::Sub`で研修を続け、`Next::Turn`で生産回数を進めます。研修費用だけを見ると途中のcostは悪化しますが、生産後の候補は同じ完了turnで比較されます。`beam_width`と`sub_beam_width`を別に指定します。有限性はレベル上限で保証します。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include "beam_copy_sub_v04.hpp"

// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, std::uint64_t>;

// 1turnの間に研修を何回か行い、その後に1回生産します。
// Stateはコピー・代入可能にし、出力用の履歴も含めます。
struct State {
    int level = 1;
    long long profit = 0;
    std::string history; // L=研修、P=生産です。
};

int main() {
    BS::Param param{};
    param.max_turn = 2;            // 生産を2回行う問題です。研修回数はturnに数えません。
    param.beam_width = 8;          // 次turnの開始候補を最大8状態保持します。
    param.sub_beam_width = 4;      // 同turn内の次sub層に最大4状態を保持します。
    param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。
    param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
    param.use_hash_dedup = false;  // 基本例では状態の重複排除を省きます。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        // 研修せず、現在のレベルで生産してturnを終える選択肢です。
        State produced = now.state;
        produced.profit += 5 * produced.level;
        produced.history += 'P';
        emit.push(produced, -produced.profit, BS::Next::Turn);

        if (now.state.level < 3) {
            // 研修は費用2でレベルを1上げます。まだ同じturn内です。
            State trained = now.state;
            ++trained.level;
            trained.profit -= 2;
            trained.history += 'L';
            emit.push(trained, -trained.profit, BS::Next::Sub);
        }
        // levelの上限3によりSubの連鎖は必ず有限になります。
        // Next::Turnの候補は、後のsub層から来る生産候補とも比較されます。
    };

    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, expand);
    if (!result.found) return 1;
    const State& answer = *result.best_state;
    int level = 1;
    int turns = 0;
    long long profit = 0;
    for (char action : answer.history) {
        if (action == 'L') {
            assert(level < 3);
            ++level;
            profit -= 2;
        } else {
            assert(action == 'P');
            ++turns;
            profit += 5 * level;
        }
    }
    assert(turns == 2 && profit == 26 && answer.profit == profit);
    assert(result.best_cost == -26 && result.best_turn == 2);
    assert(result.best_sub_step == 0 && result.best_depth == 4);
    // best_depthはLもPも数えます。best_turnはPだけを数えます。
    std::cout << "profit=" << profit << " history=" << answer.history
              << " turns=" << result.best_turn << " depth=" << result.best_depth << '\n';
}
```

最良利益は26、履歴は`LLPP`です。2turnに対して総遷移数は4です。

### 例20：turn途中の有効解をHash付きpush_lazyで保存する

価値1または2を1回ずつ回収し、合計5以上を目指します。1turnの回収は最大3回で、turnを確定すれば次の回収期間へ進めます。目的は回収回数の最小化です。

`max_turn_is_answer=false`にし、合計5以上になるSub遷移へ`finished=true`を渡します。解はturn=0の途中でも返ります。`sub_step`の上限がturn内の有限性を保証します。CSVでSubとTurnの両方の生成・棄却を確認できます。

全体1,000msを各turnへ配分し、`time_check_interval=1`で各expandの終了後に確認します。この小問題には余裕のある設定です。実問題で予算を短くして探索を打ち切る場合、末尾の固定スコアのassertも見直してください。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include "beam_copy_sub_v04.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// 1回の回収で価値1か2を加え、合計5以上にします。1turn内の回収は最大3回です。
// historyには回収値とturn区切りを残します。Stateはコピー・代入可能にします。
struct State {
    int sum = 0;
    std::string history;
};

int main() {
    BS::Param param{};
    param.max_turn = 3;              // 完了できるturnの上限です。
    param.beam_width = 16; // 各turnの開始候補を保持する上限です。
    param.sub_beam_width = 16;       // 同sub層の異なるsumを残せる幅です。
    param.max_turn_is_answer = false; // 最大turn到達だけでは有効解にしません。
    param.use_hash_dedup = true; // Hashを明示した候補の重複を、同じ保存先内で除きます。
    param.time_limit_ms = 1000.0; // 探索全体の時間予算。小さい例なので十分な余裕を設けます。
    param.use_turn_time_limit = true; // 各turn開始時に、残り時間を残りturn数へ配分します。
    param.time_check_interval = 1; // 各expand終了後に時計を確認します。expand自体は中断しません。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        if (now.state.sum >= 5) return; // finishedは展開停止命令ではないため、終端を明示します。
        if (now.sub_step < 3) {
            for (int value : {1, 2}) {
                const int sum = now.state.sum + value;
                const bool finished = sum >= 5;
                // Subへの遷移でもfinishedを指定できます。turn未完了の解を保存します。
                // costは回収回数です。Hashは将来条件を決めるsumを正確に表します。
                emit.push_lazy(now.cost + 1, static_cast<Hash>(sum), finished,
                               [&now, sum, value]() {
                    State next = now.state;
                    next.sum = sum;
                    next.history += static_cast<char>('0' + value);
                    return next;
                }, BS::Next::Sub); // makerは候補または最良解を保存するときに同期実行されます。
            }
        }
        if (now.sub_step > 0) {
            // 区切り操作は回収回数を増やしませんが、depthは1増えます。
            State next = now.state;
            next.history += '|';
            emit.push(next, now.cost, static_cast<Hash>(next.sum), false, BS::Next::Turn);
        }
    };

    BS::CsvStatHook csv("copy_sub_early.csv", 0.0); // SubStepEndとTurnEndを両方記録します。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) return 1;
    int sum = 0;
    int picks = 0;
    for (char action : result.best_state->history) {
        if (action == '|') continue;
        assert(action == '1' || action == '2');
        sum += action - '0';
        ++picks;
    }
    assert(sum >= 5 && sum == result.best_state->sum && picks == 3);
    assert(result.best_cost == 3 && result.best_turn == 0 && result.best_sub_step == 3);
    std::cout << "sum=" << sum << " picks=" << picks
              << " turn=" << result.best_turn << " sub=" << result.best_sub_step << '\n';
}
```

回収3回で解になります。合計値は選択された同点解によって5または6になり得ます。`best_turn=0`、`best_sub_step=3`です。

## 10. beam_delta_sub_v05.hpp

外部状態は、現在の候補から共通祖先へ巻き戻し、次の候補までActionを適用して復元します。同じSub層では親をまとめ、turn境界で深さの異なる親が混ざる場合も同じ部分木の候補を近づけて、経路間の往復を減らします。展開順そのものに依存する副作用は避けてください。同costの候補が多い場合や時間制限がある場合、展開順は採用される経路にも影響します。

不要ノードの回収では、保存済み最良解、以前のSub層で生成した次turn候補、現在地の祖先も保護します。Actionの移動・破棄で回収するため、Actionは通常のC++の値としてコピー・移動できるようにしてください。ライブラリ内のActionを指す参照やポインタをcallbackの呼び出し後まで保持しないでください。

`used_nodes`は構築済みノード数で、次の回収まで残る不要ノードも含みます。`reserved_nodes`は配列に確保したスロット数です。回収後も確保済み容量は毎回縮めず、次のノードへ再利用します。`nodes_capacity`は構築ノード数の上限で、Action内部の可変長データなどを含むメモリ量のバイト上限ではありません。

### 例21：turnごとのAction区間を取り出す

例19の研修・生産をdelta型で解きます。Actionにレベル差分、利益差分、表示用ラベルを持たせます。SubとTurnはどちらもActionとして保存されるので、Action列をturn単位に切るには`turn_offsets`を使います。

`nodes_capacity=0`は回収・再利用を伴う動的増設です。正の上限を設けた場合は、回収しても不足したときの`NodeLimit`による終了を確認します。この例では、最初のturnに研修2回と生産1回、次のturnに生産1回が入ります。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_delta_sub_v05.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// 差分だけを保存します。変更前の値が必要な問題ではActionへ追加してください。
// root用のデフォルト構築と、コピー・代入が可能な型にします。
struct Action {
    int level_delta = 0;
    long long profit_delta = 0;
    char label = '?';
};

int main() {
    int level = 1;
    long long profit = 0;
    BS::Param param{};
    param.max_turn = 2;            // 生産回数です。研修はSubとして同turn内で行います。
    param.beam_width = 8; // 各turnの開始候補を保持する上限です。
    param.sub_beam_width = 4; // 同turnの次sub層に保持する候補数の上限です。
    param.nodes_capacity = 0;      // sub版では0以下が動的増設です。
    param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。
    param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
    param.use_hash_dedup = true;   // 同じ保存先・同じlevelなら今後の利益条件は同じです。

    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        (void)now;
        const Action produce{0, 5LL * level, 'P'};
        emit.push(produce, -(profit + produce.profit_delta), static_cast<Hash>(level),
                  false, BS::Next::Turn);
        if (level < 3) {
            const Action train{1, -2, 'L'};
            emit.push(train, -(profit - 2), static_cast<Hash>(level + 1),
                      false, BS::Next::Sub);
        }
        // 外部状態は変更せず候補を列挙します。level上限によりSubは有限です。
    };
    auto forward = [&](const Action& action) {
        level += action.level_delta;
        profit += action.profit_delta;
    };
    auto backward = [&](const Action& action) {
        level -= action.level_delta;
        profit -= action.profit_delta;
    };

    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(param, 0LL, Hash{1}, expand, forward, backward);
    assert(level == 1 && profit == 0); // 正常終了時には外部状態が元に戻ります。
    if (!result.found || result.runtime.stop_reason == BS::StopReason::NodeLimit) return 1;
    // turn_offsets[t]〜turn_offsets[t+1]が、0始まりのturn tに属するAction区間です。
    assert(result.turn_offsets.size() == 3);
    assert(result.turn_offsets[0] == 0 && result.turn_offsets[1] == 3 && result.turn_offsets[2] == 4);
    for (int turn = 0; turn < result.best_turn; ++turn) {
        std::cout << "turn " << turn << ':';
        for (std::size_t i = result.turn_offsets[turn]; i < result.turn_offsets[turn + 1]; ++i) {
            const Action& action = result.path[i];
            forward(action);
            std::cout << action.label;
        }
        std::cout << '\n';
    }
    assert(profit == 26 && level == 3 && result.best_cost == -26);
    assert(result.best_depth == 4 && result.best_sub_step == 0);
    std::cout << "profit=" << profit << '\n';
}
```

出力は`turn 0:LLP`、`turn 1:P`、利益26です。`turn_offsets`は`{0, 3, 4}`です。

### 例22：未完了turnの末尾を含むpathを復元する

例20の回収問題をdelta型で解きます。有効解は最初のturnの途中で見つかるため、完了したturnは0ですが、pathには3回の回収が入っています。

`turn_offsets.back()`をpathの終端とみなすと、この末尾を読み落とします。コードではpath全体を再生し、最後のturn境界以降のAction数も確認します。`found=false`では`turn_offsets`が空なので、必ず先にfoundを確認します。

全体1,000msを各turnへ配分し、`time_check_interval=1`で各expandの終了後に確認します。この小問題には余裕のある設定です。実問題で予算を短くして探索を打ち切る場合、末尾の固定スコアのassertも見直してください。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include "beam_delta_sub_v05.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = bs::Beam<long long, Hash>;

// value=1,2が回収、0がturn確定です。差分の逆操作に必要な値を保持します。
// Actionはデフォルト構築・コピー・代入が可能な型にします。
struct Action {
    int value = 0;
};

int main() {
    int sum = 0; // 現在の外部状態。ライブラリがforward/backwardで探索位置を合わせます。
    BS::Param param{};
    param.max_turn = 3; // 初期turn=0から到達できる最大turnです。
    param.beam_width = 16; // 各turnの開始候補を保持する上限です。
    param.sub_beam_width = 16;      // 同turnの次sub層の幅です。
    param.nodes_capacity = 0;       // 動的増設。正の上限ではNodeLimitによる終了に注意します。
    param.max_turn_is_answer = false; // 有効解はfinished=trueで明示します。
    param.use_hash_dedup = true; // Hashを明示した候補の重複を、同じ保存先内で除きます。
    param.time_limit_ms = 1000.0; // 探索全体の時間予算。小さい例なので十分な余裕を設けます。
    param.use_turn_time_limit = true; // 各turn開始時に、残り時間を残りturn数へ配分します。
    param.time_check_interval = 1; // 各expand終了後に時計を確認します。expand自体は中断しません。

    auto expand = [&](const BS::NodeView& now, BS::Emitter<Action>& emit) {
        if (sum >= 5) return; // 有効解の状態を終端にする判定は利用側で書きます。
        if (now.sub_step < 3) {
            for (int value : {1, 2}) {
                const int next_sum = sum + value;
                emit.push(Action{value}, now.cost + 1, static_cast<Hash>(next_sum),
                          next_sum >= 5, BS::Next::Sub);
            }
        }
        if (now.sub_step > 0) {
            // turn確定は回収回数を増やしませんが、pathにActionとして1件入ります。
            emit.push(Action{0}, now.cost, static_cast<Hash>(sum), false, BS::Next::Turn);
        }
    };
    auto forward = [&](const Action& action) { sum += action.value; };
    auto backward = [&](const Action& action) { sum -= action.value; };

    BS::CsvStatHook csv("delta_sub_early.csv", 0.0); // sub層・turn境界とノード使用量を記録します。
    // runへ初期cost・初期Hash・候補生成・前進・巻き戻しを渡します。
    const auto result = BS::run<Action>(param, 0LL, Hash{0}, expand, forward, backward, csv);
    assert(sum == 0);
    if (!result.found || result.runtime.stop_reason == BS::StopReason::NodeLimit) return 1;
    // 有効解がturn途中なら、最後のturn_offsets以降にも未完了turnのActionが残ります。
    assert(result.best_turn == 0 && result.best_sub_step == 3);
    assert(result.turn_offsets.size() == 1 && result.turn_offsets.front() == 0);
    int picks = 0;
    for (const Action& action : result.path) {
        forward(action);
        if (action.value != 0) ++picks;
    }
    assert(sum >= 5 && picks == 3 && result.best_cost == 3);
    const auto unfinished = result.path.size() - result.turn_offsets.back();
    assert(unfinished == 3);
    std::cout << "sum=" << sum << " picks=" << picks
              << " unfinished_actions=" << unfinished << '\n';
}
```

回収3回で合計5以上になり、`unfinished_actions=3`です。

## 11. chokudai_copy_sub_v03.hpp

### 例23：turn内は通常ビーム、開始候補は次スイープへ残す

研修・生産の問題をchokudai subで解きます。`chokudai_width=1`により、各スイープで各turnから開始候補を1件取り出し、そのturn内を`sub_beam_width=4`で探索します。残りの開始候補は次スイープへ持ち越します。

Hashには将来の生産能力であるlevelを使います。同じturn開始状態の同じlevelなら、利益が大きい経路を残してよいためです。履歴文字列のコピーは`push_lazy`のmaker内で行います。最終turnへ到達した後も残り候補を探索したことを、スイープ数で確認します。

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include "chokudai_copy_sub_v03.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopySub<long long, Hash>;

// levelは将来の生産能力、profitは累計利益、historyは解の復元用です。
// Stateはコピー・代入可能な値型にします。
struct State {
    int level = 1;
    long long profit = 0;
    std::string history;
};

int main() {
    BS::Param param{};
    param.max_turn = 2;            // 生産を2回行います。研修はturnを進めません。
    param.beam_width = 8;          // 各turnの開始候補を保持する幅です。
    param.chokudai_width = 1;      // 1turn・1スイープで取り出す開始候補は1件です。
    param.sub_beam_width = 4;      // 取り出した候補のturn内探索に用いる通常ビーム幅です。
    param.max_sweeps = 0;          // 有限な例なので候補が尽きるまで繰り返します。
    param.time_limit_ms = 0.0; // この小問題は時間無制限で実行します。
    param.max_turn_is_answer = true; // 最大turnへの到達を、自動的に有効解とします。
    param.use_hash_dedup = true;   // 同じ保存先・levelなら将来条件が等しいので統合します。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        const long long profit = now.state.profit + 5 * now.state.level;
        // Next::Turnは次turnのキューへ送ります。開始候補としてスイープをまたいで保持されます。
        emit.push_lazy(-profit, static_cast<Hash>(now.state.level), false,
                       [&now, profit]() {
            State next = now.state;
            next.profit = profit;
            next.history += 'P';
            return next;
        }, BS::Next::Turn);
        if (now.state.level < 3) {
            const int level = now.state.level + 1;
            const long long trained_profit = now.state.profit - 2;
            // Next::Subは同turnの次sub層へ送ります。turn内の層は次スイープへ持ち越しません。
            emit.push_lazy(-trained_profit, static_cast<Hash>(level), false,
                           [&now, level, trained_profit]() {
                State next = now.state;
                next.level = level;
                next.profit = trained_profit;
                next.history += 'L';
                return next;
            }, BS::Next::Sub);
        }
        // levelに上限があるため、どの経路でもSub遷移は有限です。
        // makerは保存が必要なときだけ、この呼び出し中に実行されます。
    };

    BS::CsvStatHook csv("chokudai_sub_production.csv", 0.0); // sub・turn・sweepのイベントを記録します。
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{1}, expand, csv);
    if (!result.found) return 1;
    int level = 1;
    int turns = 0;
    long long profit = 0;
    for (char action : result.best_state->history) {
        if (action == 'L') {
            assert(level < 3);
            ++level;
            profit -= 2;
        } else {
            assert(action == 'P');
            ++turns;
            profit += 5 * level;
        }
    }
    assert(turns == 2 && profit == 26 && result.best_cost == -26);
    assert(result.best_turn == 2 && result.best_sub_step == 0 && result.best_depth == 4);
    assert(result.runtime.sweep > 1); // 最終turn到達後にも、残っていた開始候補を探索しました。
    std::cout << "profit=" << profit << " history=" << result.best_state->history
              << " sweeps=" << result.runtime.sweep << '\n';
}
```

最良利益は26、履歴は`LLPP`です。`chokudai_sub_production.csv`にはsub・turn・sweep単位の進捗が入ります。

### 例24：時間超過後も未発見を救済し、Sub途中の有効解で終了する

1turn内で2回の準備を行ってから次turnへ進み、準備が合計4回に達すれば有効解とします。最初のexpandだけ3ms待ち、1msの時間制限を確実に超えさせます。この待機は動作確認専用で、実問題の処理へ持ち込む必要はありません。

最初は有効解も次turn候補もないため、時間超過後もSub探索を続けます。次turn候補ができたら進み、次turnのSub途中で有効解を得た後、expand終了時点で探索を終えます。`time_check_interval=1`、`finished=true`、`StopReason::TimeLimit`の関係を確認する例です。

```cpp
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include "chokudai_copy_sub_v03.hpp"

using Hash = std::uint64_t;
// costは小さいほど良い評価値です。ここではlong longと64bitのHashを使います。
using BS = cs::ChokudaiCopySub<long long, Hash>;

// 1turnで準備を2回進めてから次turnへ移ります。準備が計4回進めば有効解です。
// Stateはコピー・代入可能である必要があります。
struct State {
    int work = 0;
};

int main() {
    BS::Param param{};
    param.max_turn = 2; // 初期turn=0から到達できる最大turnです。
    param.beam_width = 2; // 各turnの開始候補を保持する上限です。
    param.sub_beam_width = 2; // 同turnの次sub層に保持する候補数の上限です。
    param.chokudai_width = 1; // 1turn・1スイープで取り出す開始候補数です。
    param.max_sweeps = 1;           // 通常スイープの上限。時間超過後の解なし救済には適用しません。
    param.time_limit_ms = 1.0;      // 探索全体のsoft limitです。
    param.time_check_interval = 1;  // 各expandの終了後に時間を確認します。
    param.max_turn_is_answer = false; // 有効解はfinished=trueで明示します。
    param.use_hash_dedup = true; // Hashを明示した候補の重複を、同じ保存先内で除きます。

    auto expand = [](const BS::StateView<State>& now, BS::Emitter<State>& emit) {
        if (now.state.work >= 4) return;
        if (now.turn == 0 && now.sub_step == 0) {
            // 時間切れの動作を再現するためだけの待機です。実問題では削除してください。
            // expandは途中中断されないので、この3msの後に候補を生成できます。
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
        if (now.sub_step < 2) {
            const int work = now.state.work + 1;
            const bool finished = work == 4;
            // 有効解がなく次turn候補もない間は、時間を超えてもSub探索を継続します。
            emit.push_lazy(now.cost + 1, static_cast<Hash>(work), finished,
                           [work]() { return State{work}; }, BS::Next::Sub);
        } else {
            // 次turnの開始候補ができたので、時間切れなら速やかに次turnへ進めます。
            emit.push(now.state, now.cost, static_cast<Hash>(now.state.work),
                      false, BS::Next::Turn);
        }
    };

    BS::CsvStatHook csv("chokudai_sub_timeout.csv", 0.0);
    // runは初期Stateから探索し、最良有効解をresult.best_stateへ返します。
    const auto result = BS::run<State>(param, State{}, 0LL, Hash{0}, expand, csv);
    if (!result.found) return 1;
    // 4回目の準備で有効解を得た直後、実行中のexpandが終わった時点で終了します。
    assert(result.time_limit_reached && result.runtime.stop_reason == BS::StopReason::TimeLimit);
    assert(result.best_state->work == 4 && result.best_cost == 4);
    assert(result.best_turn == 1 && result.best_sub_step == 2 && result.best_depth == 5);
    assert(!result.completed_max_turn && result.runtime.expanded_after_timeout > 0);
    std::cout << "found=" << result.found << " timed_out=" << result.time_limit_reached
              << " turn=" << result.best_turn << " sub=" << result.best_sub_step
              << " work=" << result.best_state->work << '\n';
}
```

確認する結果は`found=1 timed_out=1 turn=1 sub=2 work=4`です。最大turn=2を完了する前に有効解を返します。実際の経過ミリ秒はOSのスケジューリングに依存するため固定値と比較しません。

## 12. サンプルの検証範囲

対象9ヘッダを使い、掲載した24本の完全なコード例をGCC 13.3.0で検証しています。

- C++20で全24例をコンパイル・実行し、AddressSanitizerとUndefinedBehaviorSanitizerで検査しました。
- C++23の最適化ビルドでも全24例をコンパイル・実行しました。計48実行が成功しています。
- 警告をエラーとして扱う設定でコンパイルしました。
- 小問題の期待値は、割当の全順列、作業集合の全列挙、短い動的計画法などでも独立に確認しました。
- 解の行動再生、費用・利益の再計算、deltaの初期状態への復元、turn区間、未完了turnの末尾、時間超過後の有効解を確認しました。
- CSVを出力する10例について、ヘッダと各行の列数、RunStart・RunEnd、最終解、時間切れ例の停止理由を確認しました。
- Markdownへ埋め込む際に補った説明はコメントのみで、コンパイル・実行したコードと、コメントを除いた内容が一致することを確認しました。

これらは掲載例の検証です。任意の利用側コードの正しさや、一般の問題に対する最適解・厳密な締切内終了を保証するものではありません。

サンプルの`assert`は問題に合わせた説明用の検査です。盤面、目的関数、時間制限、ビーム幅を変えた場合は、固定の期待値も見直してください。`-DNDEBUG`を付けるとassertは無効になるため、サンプルを確認する最初の実行では付けないでください。
