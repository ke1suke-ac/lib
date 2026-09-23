# sa_func・sa_pop 利用ガイド

対象ソース: `sa_func_v17.hpp`、`sa_pop_v09.hpp`。スケルトン: `sa_func_skelton_v03.cpp`、`sa_pop_skelton_v03.cpp`。C++20、GCC 12.2を前提とする。名前空間は `sa`。数式表示に対応した外部Markdownリーダーで読むことを想定する。

このガイドは、明示的に更新を依頼されたときだけ更新する。ライブラリの変更に連動した自動更新は行わない。

第1章で対象問題、第2章で利用者が用意する処理、第3章で設定、第4章で完成したコード例、第5章で注意点、第6章で内部の仕組みを説明する。第4章の各完成コードは独立したプログラムで、その例に必要な定義を省略していない。

## 1. 何を解くライブラリか？

### 1.1 解の良さを数値にできる最適化問題

「仕事をどの機械へ割り当てるか」「都市をどの順番で訪れるか」「頂点をどちらのグループへ入れるか」のように、選び方が多数ある問題を扱う。選び方全体を**解**、その良さを表す数値を**cost**と呼ぶ。本ライブラリでは、小さいcostほど良い解とする。

目的関数は次の形である。

$$\min_ {x \in \mathcal{X}} C(x)$$

| 記号 | 意味 |
|---|---|
| $x$ | 1つの解。割当先の配列、訪問順の配列など |
| $\mathcal{X}$ | 問題の制約を満たす、許される解の集合 |
| $C(x)$ | 解 $x$ のcostを計算する関数 |
| $\min$ | 許される解の中で、costができるだけ小さいものを求めるという目標 |

例えば、3つの仕事の担当機械が `[0, 1, 0]` なら、仕事0と2は機械0、仕事1は機械1が担当する。この配列が解で、機械間の負荷の偏りなどを数値にしたものがcostになる。

ライブラリは、最適解そのものを保証する厳密解法ではない。実行時間を与えて、より小さいcostの解を求めるために使う。問題固有の制約や、解が正しいかどうかの判定は利用者が設計する。

### 1.2 最大化したいスコアがある場合

最大化したい評価値を $S(x)$ とすると、次のcostに置き換えればよい。

$$C(x)=-S(x)$$

$S(x)$ が大きいほど $C(x)$ は小さくなるので、求める解は同じになる。元のスコアは `-best_cost` で戻せる。costが負でも構わない。ただし、符号付き整数の最小値を反転するとあふれるため、表現範囲を確認する。

定数 $A$ を使って $C(x)=A-S(x)$ としてもよい。$A$ は解によらない固定値である。第4章のグラフ分割では「全辺の重み合計−分断した辺の重み合計」をcostにする。

### 1.3 sa_funcとsa_popの担当

| 利用形態 | 用意するもの | 受け取るもの |
|---|---|---|
| `sa::sa`：sa_funcの単体利用 | 1つの初期解と、その状態を操作する関数 | その呼び出しの最良costと、条件に応じた保存解 |
| `sa::sa`を複数回使うマルチスタート | 異なる初期解、全体の時間管理、全体ベストの保持 | 複数回の結果から利用者が保持した最良解 |
| `sa::sa_pop` | 同じ問題に対する複数の初期状態と、共通の操作関数 | 全個体・全実行段階を通じて見つけた最良解 |

両ライブラリとも、TSP専用や整数配列専用ではない。解の表現とcostは利用者が決める。連続値の変数も扱える。複数の目的を同時にそのまま渡すAPIはないので、必要なら重みを付けて1つのcostにまとめる。

## 2. ユーザー定義関数

### 2.1 最初に、探索状態と保存解を分けて考える

**探索状態**は、今操作している解と、操作を速くする補助情報である。例えば、仕事の担当機械だけでなく、各機械の負荷合計、近傍を作る乱数生成器、今回動かす仕事番号を持つ。

**Snapshot**は、良い解が見つかったときに保存する内容である。出力に担当機械の配列しか要らなければ、その配列だけでよい。負荷合計や乱数生成器まで毎回コピーする必要はない。

`Snapshot` に探索状態へのポインタ、参照、`span`だけを保存すると、後から解が書き換わったときに保存内容も変わってしまう。`vector<int>` など、必要な情報を値として持つ型を使う。同じ入力を読むための変更されない参照と、保存解の値を混同しない。

`sa_func` では、探索状態をラムダの参照キャプチャなどで利用者側に保持する。`sa_pop` では、各個体の探索状態を `WorkState` という型にまとめ、`vector<WorkState>` を渡す。`WorkState` は利用者が付ける任意の型名であり、コード例では `State` としている。

### 2.2 必須の4つの関数

| 関数 | sa_funcでの基本形 | sa_popでの基本形 | 役割 |
|---|---|---|---|
| `get_snapshot` | `Snapshot()` | `Snapshot(const State&)` | 現在の解を保存用の値へ変換する |
| `get_cost` | `Cost()` | `Cost(const State&)` | 現在の解全体の絶対costを返す |
| `propose` | `Cost()` または `optional<Cost>()` | `Cost(State&)` または `optional<Cost>(State&)` | 差分を返す。`nullopt` は強制棄却 |
| `finalize` | `void(bool accepted)` | `void(State&, bool accepted)` | 採否を反映し、状態を確定する |

`Cost` は、costとその差分を表す数値型である。整数なら `long long`、小数なら `double` が分かりやすい。改善する差分は負になるので、整数の `Cost` は符号付き型が必須である。

呼び出し時の並びは **get_snapshot、get_cost、propose、finalize**。この4関数を正しく作ることが、ライブラリ利用の中心になる。

### 2.3 get_cost：何のcostを返すのか

`get_cost` が返すのは、今の解そのもののcostである。これまでの最良costや、最後の変更の差分ではない。

例えば負荷合計が `[8, 12]` で、負荷の二乗和をcostにするなら、返す値は `8*8 + 12*12 = 208`。次の変更でcostが8減るとしても、ここで `-8` を返してはいけない。

キャッシュしたcostを返す実装も可能だが、解本体とキャッシュが一緒に誤ると、終了診断でも検出できない。まずは解から独立に再計算する関数を作ると確認しやすい。sa_funcでは通常、開始時とLOCALで有効にした終了診断時に呼ばれる。sa_popでは初期評価に加え、内部SAの開始・終了や選別の準備でも呼ばれるので、極端に重い再計算には注意する。

### 2.4 propose：差分の符号を決める

現在の解を $x$、変更案を反映した解を $y$ とすると、返す値は次である。

$$\Delta=C(y)-C(x)$$

$\Delta$ は「変更後−変更前」のcost差分。常に有効な案を作れるなら `Cost` 型で返せばよい。制約違反などで案を棄却したいなら、戻り値を `optional<Cost>` にする。これは「差分の値がある／ない」を表す型で、値なしの `nullopt` を返すと必ず棄却される。

| 変更前→変更後 | 返す差分 | 意味 |
|---|---:|---|
| 100→93 | −7 | 改善 |
| 100→100 | 0 | 同じcost |
| 100→108 | +8 | 悪化 |
| 制約違反・操作を作れない | `nullopt` | 強制棄却。`finalize(false)` を呼ぶ |

`0` と `nullopt` は異なる。差分0は「costが同じ有効な変更」であり、SAは受理する。`nullopt` は受理判定へ進まず、受理用の乱数も使わない。値を返す場合は、`return delta;` と書けば `optional` へ変換される。ラムダには `-> optional<Cost>` を明示する。`get_cost` の戻り値は `Cost` のままである。

毎回解全体を再評価して差を取ることもできる。しかし、変更によって影響する部分だけを計算すれば、通常は多くの変更案を試せる。第4章では「移動元と移動先の2機械だけ」「巡回路の境界4点だけ」「反転する頂点につながる辺だけ」を使って差分を求める。

差分を速くする前に、元のcost計算と一致することを確認する。小さな入力で、実際に変更した後のcostと「変更前cost＋差分」が一致するか調べるのが基本である。

### 2.5 finalize：状態管理は2つの書き方から選べる

**書き方A：採用が決まってから変更する。** `propose` は変更箇所と差分を用意するだけ。`finalize(true)` で変更し、`finalize(false)` では何もしない。1要素の移動や反転など、差分を先に求めやすい場合に使う。

**書き方B：先に仮適用する。** `propose` 内で解を変更してから差分を返す。`finalize(true)` はそのまま確定し、`finalize(false)` は変更を取り消す。変更した状態を見ないと差分が計算しづらい場合などに使う。巡回路の例では、同じ区間を再び反転すれば元に戻る。

| 処理後 | 書き方A | 書き方B |
|---|---|---|
| `propose` の直後 | 提案前の解 | 仮適用した解 |
| `finalize(true)` の直後 | 提案後の解 | 提案後の解 |
| `finalize(false)` の直後 | 提案前の解 | 提案前の解 |

解だけでなく、負荷合計・差分キャッシュ・現在costなど、解に依存する補助情報も同じ状態に揃える。例えば解だけ巻き戻して負荷合計を戻し忘れると、次の差分から壊れる。

**温度自動推定中も `propose` と `finalize(false)` が呼ばれる。** そのため「falseは来ないだろう」という実装はできない。乱数生成器は巻き戻さなくてよい。解の意味を持つ部分を元に戻し、次回には新しい変更案が出るようにする。

**`nullopt` でも `finalize(false)` は必ず1回呼ばれる。** 仮適用後に制約違反が分かったなら、そこで元に戻す。変更前に `nullopt` を返すなら、`finalize(false)` が何もしないようにする。毎回、変更内容と「仮適用したか」のフラグを初期化すれば、前回の操作を誤って巻き戻す事故を防げる。正の巨大値やNaNを「必ず棄却する」印にしない。

### 2.6 get_snapshot：呼ばれるときだけ保存する

通常のsa_funcでは、初期解と、その呼び出しの最良costを厳密に更新したときに呼ばれる。既存ベストを指定したsa_funcでは、さらにその値を下回ることが保存条件になる。sa_popでは、初期解群の最良と、全個体共通の最良更新時だけ呼ばれる。

採用された変更がすべて保存されるわけではない。同じcostの別解も最良更新には含まれない。解の更新や必須キャッシュの更新を `get_snapshot` の中に置いてはいけない。保存されなくても探索が正しく進むようにする。

### 2.7 runtimeを受け取る形

必要なら `propose` の引数を、sa_funcでは `const sa::SaRuntime<Cost>&`、sa_popでは `State&, const sa::SaRuntime<Cost>&` にできる。引数なし／状態だけの形と両方呼べる関数オブジェクトでは、runtimeを受け取る形が優先される。

| 主なメンバー | 意味と使い方 |
|---|---|
| `temperature` | 現在温度。近傍の大きさを変える参考にできる |
| `current_cost` | 提案直前の現在cost |
| `best_cost` | 今回のSA呼び出しで見つけた最良cost |
| `iteration` | `nullopt` を含む本探索の提案数。1始まり、温度推定中は0 |
| `progress()` | そのSAの時間予算に対する消費率。0〜1 |
| `accepted_count`、`worse_accepted_count` | 受理回数、悪化を受理した回数 |
| `propose_rejected_count` | 本探索で `nullopt` が返った回数。初期温度推定の棄却は含めない |
| `propose_rejected_this_iter` | `IterationEnd` で今回の `nullopt` 棄却を確認する。提案前にはfalse |
| `best_update_count` | そのSAの最良cost更新回数。Snapshot保存回数とは限らない |
| `elapsed_us`、`time_limit_us` | 経過時間と予算。単位はマイクロ秒 |
| `last_best_update_iter`、`elapsed_us_since_last_best()` | 最後に最良を更新した反復番号と、その更新からの経過時間 |

runtimeは読み取り専用で、ライブラリが管理する。`progress()` は時計を新たに読まず、探索中は原則32反復ごとに更新された時刻を使う。温度推定中は正の予算があれば0である。

反復完了時（`IterationEnd`、`RunEnd`）の全棄却数は `iteration - accepted_count`、そのうちSAの温度・幅・独自受理規則による棄却数は `iteration - accepted_count - propose_rejected_count` で求められる。本探索の `propose` 内では今回の採否が未確定なので、完了済み提案数には `iteration - 1` を使う。`iteration=0` の事前採取は別扱いとする。`nullopt` でも反復と時間確認は進むので、操作を作れない状態が続いても時間制限で終了する。

sa_popの `propose` に渡るruntimeも **その時点の内部SA** の情報であり、全体の進捗ではない。内部SAが始まるたびに反復番号やローカル最良costも初期化される。外側の全体進捗と取り違えて近傍を設計しない。

sa_funcの `get_snapshot` だけは、引数なしに加えて `Snapshot(const SaRuntime<Cost>&)` の形も使える。両方呼べる場合は引数なしが優先される。保存時のcostは更新済みだが、最良更新回数などの後処理はまだ完了していない。初期保存時はcost以外が既定値なので、時間やカウンタを完成済みの統計として使わない。

sa_popの `get_snapshot/get_cost` は `State&` を受ける形も使えるが、通常は読み取り専用の `const State&` が分かりやすい。sa_popの `finalize` には `void(bool)` の形もあるが、個体を誤らないため、例のように対象の `State&` を受け取る形を勧める。

### 2.8 戻り値と、既存ベストの意味

sa_funcの戻り値は `sa::SaResult<Snapshot, Cost>`。

| メンバー | 意味 |
|---|---|
| `best_cost` | この1回のSAが見つけた最良cost |
| `best_snapshot` | `optional<Snapshot>`。保存条件を満たした最良解があれば値を持つ |

`existing_best_cost` を指定しなければ、時間予算0でも初期Snapshotがある。値を指定したときは、**それより厳密に小さいcost**に到達しない限り空になる。初期解がすでに基準より良ければ、その初期解から保存する。

例えば外部ベストが80で、このSAの最良が90なら、戻り値は `best_cost=90`、`best_snapshot=nullopt`。外部ベスト80を90で上書きしてはいけない。一方、70まで改善できれば、70とそのSnapshotが返る。

この基準は保存のためだけに使う。探索中の `runtime.best_cost` に外部ベストを混ぜたり、基準を超えられないから探索を打ち切ったりはしない。引数は `optional<Cost>` なので、整数costも同じ型で比較できる。実行中に別スレッドから変更する共有ベストへの参照ではなく、呼び出し時の値を渡す。

sa_popの戻り値は `pair<Cost, Snapshot>`。`auto [best_cost, best_snapshot]` で受け取れ、Snapshotは必ず存在する。最後に残った個体に限らず、途中で脱落した個体が発見した最良解も返却対象になる。

## 3. パラメータ

### 3.1 まずは既定値で始める

最初は `sa::SaParam param;`、sa_popなら加えて `sa::SaPopParam population;` を作る。以下の項目を全部書き直す必要はない。時間予算・初期解・近傍だけを問題に合わせて用意する。

`SaParam` は両ライブラリで使う。

| 項目 | 既定値 | 設定範囲・役割 |
|---|---:|---|
| `seed` | `1` | `uint64_t`。SAの受理判定用乱数のseed |
| `auto_mode` | `true` | `true`で温度自動推定。`false`で手動温度 |
| `samples` | `300` | 0以上の整数。温度推定用の提案数。0なら推定しない |
| `start_accept_prob` | `0.8` | 0より大きく1より小さい値。初期温度を算出する基準 |
| `end_accept_prob` | `0.01` | 同上。終端温度を算出する基準 |
| `start_temp` | `1000.0` | 有限の正数。手動時・推定失敗時の初期温度。推定中のruntime温度にも使う |
| `end_temp` | `0.1` | 有限の正数。手動時・推定失敗時の終端温度 |
| `enable_end_cost_check` | `true` | LOCAL時の終了cost診断。sa_popでは最終SAだけが対象 |
| `acceptance_width_scale` | `1.0` | 0以上。sa_funcの悪化を許す幅。正の無限大も可 |
| `enable_temperature_report` | `false` | LOCAL時に本探索を観測し、次回用の温度候補をstderrへ表示。今回の温度は変えない |

`SaPopParam` はsa_pop専用。

| 項目 | 既定値 | 設定範囲・役割 |
|---|---:|---|
| `selection_time_ratio` | `0.75` | 0〜1。全体予算のうち選別段階に割り当てる割合 |
| `max_state_count` | `256` | 渡す個体数の上限チェック。個体を生成する設定ではない |
| `selection_policy` | `Combination` | `Current`、`LifetimeBest`、`Combination`の3種類 |
| `selection_current_weight` | `0.25` | 0〜1。Combinationで現在costを考慮する重み |
| `auto_end_temp_scale` | `0.1` | 有限の正数。自動推定に成功した終端温度に掛ける倍率 |
| `acceptance_width_scale` | `1.0` | 0以上または正の無限大。sa_popの内部SAで使う悪化幅 |

**sa_popでは `SaPopParam.acceptance_width_scale` が使われ、`SaParam` の同名項目は使われない。** 間違えやすいため、変更場所を先に確認する。

関数に直接渡すものも整理しておく。

| 引数など | 最初の使い方 |
|---|---|
| `time_limit_ms` | 明示指定。既定値はない。2秒制限なら入力・初期化後の残り時間を計算し、余裕を含めた外側1950ms程度を出発点にする |
| sa_funcの `existing_best_cost` | 単体では省略／`nullopt`。マルチスタートでは保持している全体最良costを渡す |
| `debug_hook` | 省略時は `sa::NoOp`。CSVを見たいときだけCSV Hookを渡す |
| sa_popの個体数 | `work_states.size()` で決まる。既定個体数はない。例では8個を用意する |

個体数は1以上、256以下、かつ `max_state_count` 以下。`max_state_count` を1000へ変えても256の制限は増えない。初期解の構築や状態のメモリが重いときは少数から始める。8はこのガイドの例の開始点であり、全問題に最適な個体数ではない。

単体SAで十分に良い初期解から改善を続けたいなら、まず単体を使う。初期解の当たり外れが大きいならマルチスタートを考える。その中で、途中の成績が良い個体へ時間を集中させたいときがsa_popの候補になる。一方、途中では悪く見える個体が遅れて良くなる問題では、選別で落とさず均等時間を与える方式にも利点がある。sa_popが常に最良になるという前提にはしない。

### 3.2 次に試す変更を1つに絞るなら

既定値の次に、**受理幅だけを `4.0` にする**候補を用意する。

| 使用する方式 | 変更する箇所 |
|---|---|
| 単体SA・均等時間マルチスタート | `param.acceptance_width_scale = 4.0;` |
| sa_pop | `population.acceptance_width_scale = 4.0;` |

他の温度や選別の設定はそのままにする。最良解の近くにとどまりすぎ、少し悪い状態を経由しないと良い解へ進めない問題を想定した変更である。逆に、悪化を許すと解が崩れやすい問題には1.0が向く。4.0が常に改善する保証はないので、問題の構造から既定値を補う候補として扱う。

この値は絶対costの許容量ではなく、**温度に掛ける倍率**である。温度が下がると、同じ4.0でも許すcost差は小さくなる。外部の既存ベストを渡す設定とは別物である。

### 3.3 受理幅を手動で選ぶ目安

倍率を $w$、温度を $T$、現在costを $c$、その内部SAの最良costを $b$、悪化差分を $\Delta>0$ とする。通常の悪化提案は、まず次の幅に収まる必要がある。

$$c+\Delta-b<wT$$

条件を満たした上で確率による採否がある。つまり、幅の内側なら必ず採用されるわけではない。

| 値 | 意味・選ぶ状況 |
|---|---|
| `0.0` | 悪化を受け入れない。改善と同値だけにしたいとき |
| `1.0` | まず使う既定値。現在温度に対して最良から離れすぎない |
| `4.0` | 次の候補。より大きな一時悪化を経由させたいとき |
| `numeric_limits<double>::infinity()` | 幅の条件を外す。温度による確率判定は残る |

API上はどの非負の有限値も指定できる。最初から多数の細かな倍率を候補にする必要はない。改善・同値の提案には、この悪化幅の条件は掛からない。

### 3.4 自動温度で何が決まるか

`auto_mode=true`、`samples>0`なら、初期状態から作った変更案のうち、有限な正の差分を集める。これらの平均を $D$ とする。改善案、差分0、`nullopt` は、この平均に含まれない。

初期受理率の基準を $p_ {s}$、終端受理率の基準を $p_ {e}$、終端温度の倍率を $\alpha$ とすると、次の温度を使う。

$$T_ {s}=-D/\log(p_ {s})$$

$$T_ {e}=\alpha\left(-D/\log(p_ {e})\right)$$

$T_ {s}$ は初期温度、$T_ {e}$ は終端温度。$\log$ は自然対数である。ここでは「典型的な悪化量をどの程度許すかを、costと同じ単位の温度へ直す計算」と考えればよい。式の理由は第6章で説明する。

sa_funcでは $\alpha=0.1$ が固定。sa_popでは `population.auto_end_temp_scale` が $\alpha$ に当たり、既定値は同じ0.1。例えば $D=100$ なら、既定値から初期温度は約448.14、終端温度は約2.17になる。

`start_accept_prob=0.8` は探索全体の受理率80%を保証する設定ではない。差分の大きさが案ごとに異なり、受理幅でも絞るためである。`end_accept_prob=0.01` も、実測の終端受理率が1%になるという意味ではない。終端にはさらに0.1などの倍率が掛かる。

sa_funcは最大 `samples` 回の提案を使う。`nullopt` もこの試行回数を1回消費し、有効標本が集まるまで無制限に引き直すことはしない。sa_popではK個体に `ceil(samples/K)` 回ずつ配るので、例えば300サンプル・8個体なら38回ずつ、合計304回となる。時間切れなら途中で止まる。推定はsa_pop全体で1回であり、個体ごとに別々の温度を推定するわけではない。

正の有効な差分が1つもない、または推定温度が非有限／0以下になる場合は、`start_temp` と `end_temp` に戻る。`auto_mode=false` または `samples=0` も手動値を使う。sa_popの `auto_end_temp_scale` は、手動値や推定失敗時の値には掛からない。

通常は300のままでよい。1回の提案が重く推定だけで時間を消費する場合や、悪化案が極端に少ない場合には、サンプル数を無闇に増やす前に、差分や近傍の作り方を確認する。**初期状態から改善案しか出ない場合、同じ状態で採取回数を増やしても悪化量は得られない。** 本探索を進めた後なら悪化案が出る問題では、3.7の探索後温度見積もりを利用できる。

### 3.5 手動温度の決め方

`auto_mode=false` にして、`start_temp` と `end_temp` を指定する。どちらも有限の正数が必要で、0を終端温度にすることはできない。

温度はcostと同じ単位で考える。絶対costが100万だから温度も100万にする、という決め方ではない。**1回の変更で何costくらい悪化するか**を見る。

許したい悪化量を $d>0$、確率判定で採用したい割合を $p$ とすると、基準になる温度は次である。

$$T=-d/\log(p)$$

| 割合 $p$ | 温度の目安 | 解釈 |
|---|---:|---|
| 0.8 | 約 $4.48d$ | その悪化量を比較的通しやすい初期温度の候補 |
| 0.5 | 約 $1.44d$ | 初期の悪化をやや抑える候補 |
| 0.01 | 約 $0.217d$ | 確率の上ではほぼ受け入れない温度の候補 |

ただし幅条件もあるので、この表だけで実際の採用割合は決まらない。例えば $w=1$ で $T=0.217d$ なら、最良点からの悪化量 $d$ は幅を超えて却下される。

具体例として、開始時に悪化量100を0.8程度の基準で扱い、最後は改善中心にしたければ、初期温度を約448、終端温度を数程度にする設計が考えられる。終盤にも悪化を残したい場合は終端温度を上げ、細かな改善に集中させたい場合は下げる。初期と終端が同じなら温度一定、初期より終端を高くすれば加熱になる。通常の出発点は初期より終端を低くする設定である。

### 3.6 sa_popの選別設定を変える場合

まず `Combination`、`selection_current_weight=0.25`、`selection_time_ratio=0.75` を使う。選別時点の現在costを $c$、その個体がそれまでに見つけた最良costを $b$、重みを $\lambda$ とすると、選別に使う値は次である。

$$k=b+\lambda\max(0,c-b)$$

$k$ は小さいほど優先される値。$\lambda$ が `selection_current_weight`。最良実績を基本に、現在位置がその実績から悪化している分を少し減点する。

| 選別方針 | 何を重視するか | 使用を考える状況 |
|---|---|---|
| `Combination` | 過去の最良実績と現在位置 | まず使う方針 |
| `Current` | 現在costだけ | 現在位置の良さが、その後の改善につながりやすいと考えられる場合 |
| `LifetimeBest` | 過去の最良costだけ | 一時的に現在位置が悪化しても、良い解へ到達した実績を残したい場合 |

`selection_current_weight` はCombinationのときだけ選別式に効く。小さくすると実績重視、大きくすると現在値重視になる。ただし入力値としては、どの方針でも0〜1に収める。

`selection_time_ratio` を0.5へ下げると、選別を短めにして最後の1個体へ時間を多めに残す。0.9へ上げると、複数個体を残して評価する時間を長めにする。2個体以上の場合、0では選別段階の探索時間がなく、1では最後の磨き込み時間がほぼ残らない。最初から端の値にせず、0.75を基本にする。この割合は温度区間の分割にも使われるので、時間配分だけの独立した変更ではない。

`auto_end_temp_scale` は通常0.1を保つ。終盤も動かしたいなら大きく、より冷やしたいなら小さくするが、最初に受理幅と同時には変更しない。手動温度ではこの項目は作用しない。

### 3.7 LOCALで最後まで走らせ、次回用の温度を見積もる

初期状態の周囲だけでは温度を決められないときや、探索前半と後半で悪化量の大きさが変わるときに使う。`SaParam` の設定へ次の1行を加え、`-DLOCAL` でコンパイルする。

```cpp
param.enable_temperature_report = true;
```

通常どおり時間制限までSAを実行すると、**本探索中の悪化提案**から開始・終端温度の候補を計算し、終了時に標準エラー出力へ表示する。初期状態での自動推定が失敗しても、その後の探索で悪化を観測できれば見積もれる。`auto_mode=true` と `false` のどちらでも利用でき、CSV Hookは不要である。

| 機能 | 観測するもの | 得た値を使う実行 |
|---|---|---|
| `auto_mode=true` の初期温度推定 | 初期状態を変えずに試した悪化提案 | 今回の実行 |
| `enable_temperature_report=true` の温度見積もり | 時間制限まで探索した過程の悪化提案 | 利用者が設定を貼り付けた次回以降の実行 |

見積もりができると、`param.auto_mode = false;`、`param.start_temp = 数値;`、`param.end_temp = 数値;` の3行が表示される。**3行とも**パラメータ設定の最後、`sa`を呼ぶ前へ貼り付ける。開始・終端温度だけを書き、`auto_mode=true` のままにすると、自動推定に成功した際にその値は使われない。

開始側は悪化を最初に観測した時間区間、終端側は最後に観測した時間区間を参考にする。区間内の標本が少なければ全体平均で補う。基準確率は `start_accept_prob`、`end_accept_prob`、終端倍率は0.1であり、受理幅も含めた最適化を行う機能ではない。具体的な手順は4.7、集計と式は6.7で説明する。

既定値は `false`。`LOCAL`なしでは `true` にしても観測・出力されない。観測中に今回の温度や受理規則を書き換えたり、追加のSAを走らせたりはしない。ただしLOCALの観測・ログの負担で反復数は変わり得る。候補は観測した入力と探索経路に依存し、最善の温度やスコア向上を保証しない。

sa_popでこのフラグを直接有効にすると、**各内部SAごと**に表示される。集団全体を通した1つの見積もりにはならない。1個体を制限時間まで走らせて参考値を得るには、4.8のsa_popスケルトンのLOCAL専用分岐を使う。

## 4. ユースケースとコード例

### 4.1 準備と実行方法

コード例は、必要なヘッダを見つけられる場所に置いてC++20でコンパイルする。sa_popのヘッダは内部で `sa_func_v17.hpp` をincludeするので、両ヘッダを同じディレクトリへ置く。

単独の `solver.cpp` とヘッダを同じ場所に置いた場合:

```sh
g++ -std=c++20 -O2 -Wall -Wextra solver.cpp -o solver
./solver < input.txt
```

同梱ZIPのルートには `include/`（ヘッダ）、`examples/`（完成例）、`skeltons/`（問題ごとに埋める雛形）がある。展開したディレクトリでは、例えば次のように実行できる。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -I include examples/01_load_single.cpp -o solver
./solver 25 < input.txt
```

5つの完成例は共通して、コマンドライン引数なしなら外側の時間目安1950ms、引数を1つ渡すとそのミリ秒数を使う。これは例の動作確認用で、ライブラリがコマンドライン引数を読む機能ではない。`./solver 0` でも初期解から有効な答えを出す。

時刻は入力・初期化前から計り、SAに渡す直前に残り時間を計算する。ただしライブラリの初期評価・保存・終了処理や出力を含む厳密な締め切り保証ではない。処理が重い問題では追加の余裕を取る。コード例のstdoutは説明用の形式なので、提出時には各問題の指定出力へ変更する。

### 4.2 仕事の割当：単体SAと、受理後に反映する書き方

#### 問題をcostへ変える

$n$ 個の仕事を $m$ 台の機械へ割り当てる。仕事 $i$ の重さを $w_ {i}$、担当機械を $a_ {i}$ とする。仕事番号は0〜$n-1$、機械番号は0〜$m-1$。1つの仕事は必ず1台へ丸ごと割り当てる。

機械 $j$ の合計負荷を $L_ {j}$ とする。

$$L_ {j}=\sum_ {i:a_ {i}=j}w_ {i}$$

負荷の二乗和を小さくする。

$$C(a)=\sum_ {j=0}^{m-1}L_ {j}^{2}$$

$a$ は担当機械の配列全体である。負荷総量は同じなので、偏りが小さい方がこの値も小さくなる。例えば `[10, 0]` はcost100、`[5, 5]` はcost50。ここで扱う目的は二乗和であり、「最大負荷だけを最小化する問題」とは異なる。

#### 変更案と差分を作る

1つの仕事を別の機械へ移す。移動元の負荷を $A$、移動先の負荷を $B$、仕事の重さを $w$ とすると、影響するのはその2台だけである。

$$\Delta=(A-w)^2+(B+w)^2-A^2-B^2$$

差分は負荷キャッシュから定数時間で求まる。`propose` は仕事番号と移動先を記録し、`finalize(true)` で担当機械と2つの負荷を一緒に更新する。`finalize(false)` では何もしない。

#### 完成コード

入力例:

```text
6 3
3 4 5 6 7 8
```

この入力では、3+8、4+7、5+6と分けると各負荷11、cost363になる。SAが必ずこの解を返すという保証ではなく、目的関数を理解するための一例である。

ファイル: `examples/01_load_single.cpp`

```cpp
// 入力: n m / w[0] ... w[n-1]。出力: 最小化したcost / 各仕事の担当機械(0始まり)。
// この例の範囲: 1<=n<=2000, 1<=m<=256, 0<=w[i]<=100000。
#include "sa_func_v17.hpp"
using Cost = long long;
using Snapshot = vector<int>;

Cost evaluate(const vector<Cost>& weights, int machines, const Snapshot& assignment) {
    vector<Cost> load(machines, 0);
    for (int i = 0; i < (int)weights.size(); ++i) load[assignment[i]] += weights[i];
    Cost cost = 0;
    for (Cost x : load) cost += x * x;
    return cost;
}

int main(int argc, char** argv) {
    const auto started = chrono::steady_clock::now();
    // 引数なしなら外側1950ms。短い動作確認では ./solver 20 のように指定できる。
    const double total_ms = argc >= 2 ? stod(argv[1]) : 1950.0;
    assert(isfinite(total_ms) && total_ms >= 0.0);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    if (!(cin >> n >> m)) return 0;
    assert(1 <= n && n <= 2000 && 1 <= m && m <= 256);
    vector<Cost> weights(n);
    for (Cost& x : weights) { cin >> x; assert(0 <= x && x <= 100000); }

    mt19937_64 rng(123456);  // 初期解と近傍生成用。SAの受理乱数とは別。
    Snapshot assignment(n);
    vector<Cost> load(m, 0);  // 差分計算用キャッシュ。Snapshotには含めない。
    for (int i = 0; i < n; ++i) {
        assignment[i] = int(rng() % m);
        load[assignment[i]] += weights[i];
    }
    int job = 0, from = 0, to = 0;  // proposeからfinalizeへ渡す今回の変更内容。

    auto get_snapshot = [&]() -> Snapshot { return assignment; };
    auto get_cost = [&]() -> Cost {
        // キャッシュとは独立に計算し、LOCALの終了診断で反映ミスも検出する。
        return evaluate(weights, m, assignment);
    };
    auto propose = [&]() -> optional<Cost> {
        job = int(rng() % n);
        from = assignment[job];
        to = from;
        if (m == 1) return nullopt;  // 変更先がない。finalize(false)で何もせず戻る。
        to = int(rng() % (m - 1));
        if (to >= from) ++to;  // 現在と異なる機械を選ぶ。
        const Cost w = weights[job], a = load[from], b = load[to];
        // 提案後cost - 提案前cost。ここではまだ状態を変えない。
        return (a - w) * (a - w) + (b + w) * (b + w) - a * a - b * b;
    };
    auto finalize = [&](bool accepted) -> void {
        if (!accepted || from == to) return;
        load[from] -= weights[job];
        load[to] += weights[job];
        assignment[job] = to;
    };

    sa::SaParam param;  // 温度自動推定・受理幅などはすべて既定値で開始。
    param.seed = 1;    // 受理判定用のseed。rngのseedは別に設定する。
    sa::SaCsvStatHook<Cost> csv;  // LOCAL時のみ sa_stat.csv に記録する。
    const double elapsed = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
    auto result = sa::sa<Snapshot, Cost>(
        param, max(0.0, total_ms - elapsed),
        get_snapshot, get_cost, propose, finalize,
        nullopt,       // 単体SAなので外部の既存ベストを指定しない。
        std::ref(csv));

    // 既存ベストを指定しないので、時間予算0でもSnapshotが必ずある。
    assert(result.best_snapshot);
    const Snapshot& answer = *result.best_snapshot;
    assert(evaluate(weights, m, answer) == result.best_cost);
    // AHCで使う場合は、この出力部分を問題の指定形式に合わせる。
    cout << result.best_cost << '\n';
    for (int i = 0; i < n; ++i) cout << (i ? " " : "") << answer[i];
    cout << '\n';
}
```

#### 呼び出しと戻り値の利用

1. 入力を読み、各仕事をランダムな機械へ割り当てる。これが有効な初期解になる。
2. `load` を初期化する。以後、差分の計算に使う。
3. `SaParam` を既定値で作り、残り時間と4つの関数を `sa::sa` へ渡す。
4. 第7引数の `nullopt` は外部の既存ベストなし、第8引数はCSV Hook。`std::ref(csv)` により、呼び出し側のHookを参照して使う。
5. `result.best_cost` と `*result.best_snapshot` を使う。終了時の `assignment` は最良時点とは限らない。

カスタマイズする中心は `evaluate` と `propose` の差分式、そして `finalize` の更新内容である。仕事の担当機械が出力情報のすべてなので、Snapshotに `load` は含めない。整数あふれを避けるため、この例では入力範囲をコメントとassertで限定している。

### 4.3 巡回路：2-optと、仮適用して巻き戻す書き方

#### 問題と目的関数

$n$ 個の都市を1回ずつ訪れ、最後に出発都市へ戻る。訪問順を配列 $p$ とし、$p_ {i}$ は $i$ 番目に訪れる都市、$D(u,v)$ は都市 $u$ と $v$ の距離とする。$p_ {n}=p_ {0}$ と約束すると、巡回路長は次になる。

$$C(p)=\sum_ {i=0}^{n-1}D(p_ {i},p_ {i+1})$$

訪問順は都市番号0〜$n-1$ の順列でなければならない。この例は座標間のユークリッド距離を四捨五入して整数にした、対称な距離を使う。入力都市の重複座標も許す。

#### 区間反転で有効な巡回路を保つ

配列の位置 `left` から `right` までを反転する。順列を反転しても、都市の重複・欠落は起きない。

反転前の境界の都市を、区間直前から順に $a,b$、区間終端とその直後を $c,d$ とする。境界の辺は $a$–$b$ と $c$–$d$ から、$a$–$c$ と $b$–$d$ に変わる。内部の辺は逆向きになるだけで、対称距離では長さが変わらない。

$$\Delta=D(a,c)+D(b,d)-D(a,b)-D(c,d)$$

全巡回路を反転する場合は境界の扱いが重なるので、このコードでは変更前に `nullopt` を返す。`changed=false` により、その後の `finalize(false)` は何もしない。有効な区間なら差分を求めて仮反転し、却下なら同じ区間を再反転して戻す。この仕組みは、一般の非対称距離にはそのまま使えない。

#### 完成コード

入力例:

```text
4
0 0
10 0
10 10
0 10
```

ファイル: `examples/02_tsp_single.cpp`

```cpp
// 入力: n / x[0] y[0] / ...。出力: 巡回路長 / 訪問順(都市番号は0始まり)。
// 4<=n<=1000, 座標は-1000000以上1000000以下の整数。距離は四捨五入した整数。
#include "sa_func_v17.hpp"
using Cost = long long;
using Snapshot = vector<int>;

struct TspState {
    Snapshot tour;
    mt19937_64 rng;
    int left = 0, right = 0;
    bool changed = false;

    explicit TspState(int n, uint64_t seed) : tour(n), rng(seed) {
        iota(tour.begin(), tour.end(), 0);
        shuffle(tour.begin(), tour.end(), rng);
    }
    Cost cost(const vector<vector<Cost>>& distance) const {
        Cost result = 0;
        const int n = (int)tour.size();
        for (int i = 0; i < n; ++i) result += distance[tour[i]][tour[(i + 1) % n]];
        return result;
    }
    optional<Cost> propose(const vector<vector<Cost>>& distance) {
        const int n = (int)tour.size();
        left = int(rng() % n);
        right = int(rng() % (n - 1));
        if (right >= left) ++right;
        if (left > right) swap(left, right);
        changed = !(left == 0 && right == n - 1);
        if (!changed) return nullopt;  // 全巡回路の反転を除外。未変更なので巻き戻しは不要。
        const int a = tour[(left + n - 1) % n], b = tour[left];
        const int c = tour[right], d = tour[(right + 1) % n];
        const Cost delta = distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
        // 仮適用する流儀。以降、却下されたときには必ず元へ戻す。
        reverse(tour.begin() + left, tour.begin() + right + 1);
        return delta;
    }
    void finalize(bool accepted) {
        // nulloptと自動温度推定もfalse。changed=falseなら安全に何もしない。
        if (!accepted && changed) reverse(tour.begin() + left, tour.begin() + right + 1);
        changed = false;
    }
};

int main(int argc, char** argv) {
    const auto started = chrono::steady_clock::now();
    const double total_ms = argc >= 2 ? stod(argv[1]) : 1950.0;
    assert(isfinite(total_ms) && total_ms >= 0.0);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    assert(4 <= n && n <= 1000);
    vector<pair<int, int>> points(n);
    for (auto& [x, y] : points) {
        cin >> x >> y;
        assert(abs(x) <= 1000000 && abs(y) <= 1000000);
    }
    vector<vector<Cost>> distance(n, vector<Cost>(n));
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j)
        distance[i][j] = llround(hypot(double(points[i].first - points[j].first),
                                     double(points[i].second - points[j].second)));
    TspState state(n, 123456);
    sa::SaParam param;  // まず既定の自動温度で実行する。
    param.seed = 1;
    const double elapsed = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
    auto result = sa::sa<Snapshot, Cost>(
        param, max(0.0, total_ms - elapsed),
        [&]() -> Snapshot { return state.tour; },
        [&]() -> Cost { return state.cost(distance); },
        [&](const sa::SaRuntime<Cost>& runtime) -> optional<Cost> {
            (void)runtime;  // 必要ならtemperatureやprogress()で近傍を切り替えられる。
            return state.propose(distance);
        },
        [&](bool accepted) -> void { state.finalize(accepted); });

    assert(result.best_snapshot);
    const Snapshot& answer = *result.best_snapshot;
    // state.tourは探索終了時点の解。出力には必ず保存されたanswerを使う。
    cout << result.best_cost << '\n';
    for (int i = 0; i < n; ++i) cout << (i ? " " : "") << answer[i];
    cout << '\n';
}
```

#### 呼び出しと戻り値の利用

1. 距離行列を作る。以後、近傍では4つの距離を読むだけで差分が分かる。
2. `TspState` がランダムな順列と近傍用の乱数生成器を持つ。
3. `get_cost` は現在の巡回路全体を再計算する。`get_snapshot` は訪問順を値でコピーする。
4. `state.propose` は差分を返すまでに仮反転する。`state.finalize(false)` で必ず戻す。自動温度推定の提案も、この同じ組合せで処理される。
5. 既存ベスト・Hookを省略して `sa::sa` を呼ぶ。戻ったSnapshotの都市順を出力する。

差分計算は定数時間だが、区間反転そのものは区間の長さに比例する。距離行列のメモリは都市数の二乗に比例するため、大きな問題では距離の持ち方も検討する。runtimeを使って近傍を切り替える場合も、返す差分の単位と巻き戻しの責任は変わらない。

### 4.4 巡回路の均等時間マルチスタート：全体ベストだけを保存する

#### 何を追加するか

解く問題と2-optは4.3と同じ。初期巡回路を変えて8回実行し、全体の残り時間を未実行回数で割る。例えば残り1000msで4回残っていれば、次のSAには250msを渡す。

全体の `best_cost` と `best_snapshot` を、ループの外に置く。初回は両方空。2回目以降は全体ベストcostをsa_funcへ渡し、それより良くないSnapshotのコピーを省く。

これは保存量を減らす設定であり、悪い初期解からの探索そのものを省略する設定ではない。各SAは自分自身のローカル最良costを保持して進む。

#### 完成コード

入力・出力は4.3と同じ。コピーしやすいように、必要な `TspState` の定義も含めている。

ファイル: `examples/03_tsp_multistart.cpp`

```cpp
// 入力: n / x[0] y[0] / ...。出力: 巡回路長 / 訪問順(都市番号は0始まり)。
// 4<=n<=1000, 座標は-1000000以上1000000以下の整数。距離は四捨五入した整数。
#include "sa_func_v17.hpp"
using Cost = long long;
using Snapshot = vector<int>;

struct TspState {
    Snapshot tour;
    mt19937_64 rng;
    int left = 0, right = 0;
    bool changed = false;

    explicit TspState(int n, uint64_t seed) : tour(n), rng(seed) {
        iota(tour.begin(), tour.end(), 0);
        shuffle(tour.begin(), tour.end(), rng);
    }
    Cost cost(const vector<vector<Cost>>& distance) const {
        Cost result = 0;
        const int n = (int)tour.size();
        for (int i = 0; i < n; ++i) result += distance[tour[i]][tour[(i + 1) % n]];
        return result;
    }
    optional<Cost> propose(const vector<vector<Cost>>& distance) {
        const int n = (int)tour.size();
        left = int(rng() % n);
        right = int(rng() % (n - 1));
        if (right >= left) ++right;
        if (left > right) swap(left, right);
        changed = !(left == 0 && right == n - 1);
        if (!changed) return nullopt;  // 全巡回路の反転を除外。未変更なので巻き戻しは不要。
        const int a = tour[(left + n - 1) % n], b = tour[left];
        const int c = tour[right], d = tour[(right + 1) % n];
        const Cost delta = distance[a][c] + distance[b][d] - distance[a][b] - distance[c][d];
        // 仮適用する流儀。以降、却下されたときには必ず元へ戻す。
        reverse(tour.begin() + left, tour.begin() + right + 1);
        return delta;
    }
    void finalize(bool accepted) {
        // nulloptと自動温度推定もfalse。changed=falseなら安全に何もしない。
        if (!accepted && changed) reverse(tour.begin() + left, tour.begin() + right + 1);
        changed = false;
    }
};

int main(int argc, char** argv) {
    const auto started = chrono::steady_clock::now();
    const double total_ms = argc >= 2 ? stod(argv[1]) : 1950.0;
    assert(isfinite(total_ms) && total_ms >= 0.0);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    if (!(cin >> n)) return 0;
    assert(4 <= n && n <= 1000);
    vector<pair<int, int>> points(n);
    for (auto& [x, y] : points) {
        cin >> x >> y;
        assert(abs(x) <= 1000000 && abs(y) <= 1000000);
    }
    vector<vector<Cost>> distance(n, vector<Cost>(n));
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j)
        distance[i][j] = llround(hypot(double(points[i].first - points[j].first),
                                     double(points[i].second - points[j].second)));

    constexpr int starts = 8;  // 初期解ごとに均等な時間を配る。
    optional<Cost> best_cost;  // 初回は「既存ベストなし」。適当な番兵値は不要。
    optional<Snapshot> best_snapshot;
    for (int run = 0; run < starts; ++run) {
        TspState state(n, 123456 + run);  // 初期解・近傍乱数を実行ごとに変える。
        sa::SaParam param;
        param.seed = 1 + run;
        // 全体の残り時間を、まだ走らせていないSA数で割る。
        const double elapsed = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
        const double budget_ms = max(0.0, total_ms - elapsed) / (starts - run);
        auto result = sa::sa<Snapshot, Cost>(
            param, budget_ms,
            [&]() -> Snapshot { return state.tour; },
            [&]() -> Cost { return state.cost(distance); },
            [&]() -> optional<Cost> { return state.propose(distance); },
            [&](bool accepted) -> void { state.finalize(accepted); },
            best_cost);  // 全体ベストを厳密に下回るまでSnapshotを作らない。

        if (result.best_snapshot) {
            // best_costはこのSAの最良値。改善Snapshotがある場合だけ全体へ反映する。
            best_cost = result.best_cost;
            best_snapshot.emplace(move(*result.best_snapshot));
        }
        // 未改善なら、これまでのbest_costとbest_snapshotをそのまま保持する。
    }

    assert(best_cost && best_snapshot);  // 初回は基準なしなので、必ず解が残る。
    cout << *best_cost << '\n';
    for (int i = 0; i < n; ++i) cout << (i ? " " : "") << (*best_snapshot)[i];
    cout << '\n';
}
```

#### 呼び出しと戻り値の利用

1. 距離行列は1回だけ作り、全実行で共有する。
2. 毎回 `TspState` を新しく作り、初期解と近傍用seedを変える。SAの受理用seedも別に変える。
3. `budget_ms` はその回だけの予算。各回に1950msを渡すと全体で約8倍使ってしまうので、必ず外側の残り時間から割り当てる。
4. `best_cost` を第7引数へ渡す。初回は空なので初期解が保存され、以後は全体ベストを更新した場合だけSnapshotが返る。
5. `if (result.best_snapshot)` の内側で、全体costとSnapshotを一緒に更新する。空だったときはどちらも変更しない。

`result.best_cost` はその1回の成績なので、Snapshotを確認せず毎回代入してはいけない。`best_snapshot.emplace(move(...))` は、返された配列を再コピーせず移して保持する。

この例の均等配分には、呼び出しごとの初期評価などによる小さな差がある。各SAは温度自動推定もそれぞれ実行する。極端に短い1回分の時間では、推定や初期化の比率が大きくなる点に注意する。

### 4.5 重み付きグラフの分割：sa_popと最大化問題の扱い

#### 最大化したい量を決める

頂点を0側と1側へ分け、異なる側を結ぶ辺の重み合計を最大にしたい。これが重み付きMax-Cut問題である。

頂点数を $n$、辺数を $m$、辺の集合を $E$、頂点 $v$ の側を $c_ {v}$ とし、その値は0または1とする。各辺は両端 $u,v$ と非負の重み $w$ を持つ。全辺の重み合計を $W$、異なる側へ分けた辺の重み合計を $S(c)$ とする。

同じ側に残ってしまった辺の重みをcostにすればよい。

$$C(c)=\sum_ {(u,v,w)\in E,\ c_ {u}=c_ {v}}w=W-S(c)$$

$c$ は全頂点の側を並べた配列。$W$ は解によらないので、costを小さくするほど分断した重みが大きくなる。この例では左右の頂点数を同数にする制約はない。均等分割が必要なら、1頂点反転では制約を保てないため、左右の頂点の交換など別の近傍が必要になる。

#### 1頂点だけを反転する

頂点 $v$ の側を0↔1で変える。$v$ につながる辺だけが影響する。もともと同じ側だった辺はcostが重み分だけ減り、異なる側だった辺は重み分だけ増える。この増減を足せば差分になる。

複数の初期分割を作り、それぞれに近傍用乱数を持たせる。入力の辺や隣接リストは読み取り専用として全個体で共有する。Snapshotには最終出力に必要な側の配列だけを保存する。

#### 完成コード

入力例:

```text
3 3
0 1 1
1 2 1
2 0 1
```

三角形の3辺のうち分断できるのは最大2辺なので、最大スコア2、最小cost1になる。

ファイル: `examples/04_maxcut_population.cpp`

```cpp
// 入力: n m / u v w をm行。頂点番号は0始まり。自己ループなし、重複辺は可。
// 出力: 異なる側へ分けた辺の重み合計 / 各頂点の側(0または1)。
// 1<=n<=2000, 0<=m<=20000, 0<=w<=1000000。
#include "sa_pop_v09.hpp"
using Cost = long long;
using Snapshot = vector<int>;

struct Edge { int u, v; Cost weight; };
struct State {
    Snapshot side;
    mt19937_64 rng;
    int pending_vertex = 0;
    State(int n, uint64_t seed) : side(n), rng(seed) {
        for (int& x : side) x = int(rng() & 1);
    }
};

Cost evaluate(const vector<Edge>& edges, const Snapshot& side) {
    // 同じ側に残る辺の重みを最小化する。これで分断する重みが最大になる。
    Cost result = 0;
    for (const Edge& e : edges) if (side[e.u] == side[e.v]) result += e.weight;
    return result;
}

int main(int argc, char** argv) {
    const auto started = chrono::steady_clock::now();
    const double total_ms = argc >= 2 ? stod(argv[1]) : 1950.0;
    assert(isfinite(total_ms) && total_ms >= 0.0);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, m;
    if (!(cin >> n >> m)) return 0;
    assert(1 <= n && n <= 2000 && 0 <= m && m <= 20000);
    vector<Edge> edges(m);
    vector<vector<pair<int, Cost>>> adjacent(n);  // 全個体で共有する読み取り専用入力。
    Cost total_weight = 0;
    for (Edge& e : edges) {
        cin >> e.u >> e.v >> e.weight;
        assert(0 <= e.u && e.u < n && 0 <= e.v && e.v < n && e.u != e.v);
        assert(0 <= e.weight && e.weight <= 1000000);
        adjacent[e.u].emplace_back(e.v, e.weight);
        adjacent[e.v].emplace_back(e.u, e.weight);
        total_weight += e.weight;
    }

    constexpr int starts = 8;  // この例の開始点。ライブラリに個体数の既定値はない。
    vector<State> states;
    states.reserve(starts);
    for (int i = 0; i < starts; ++i) states.emplace_back(n, 123456 + i);

    sa::SaParam param;
    param.seed = 1;  // 各内部SAの受理乱数は、このseedからライブラリが生成する。
    sa::SaPopParam population;  // 選別時間0.75、Combination、現在値の重み0.25。
    // sa_popの受理幅を変えるならpopulation.acceptance_width_scaleを設定する。
    sa::SaPopCsvStatHook<Cost> csv;  // LOCAL時のみ sa_pop_*.csv を出力する。
    const double elapsed = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
    auto [best_cost, best_snapshot] = sa::sa_pop<Snapshot, Cost>(
        param, max(0.0, total_ms - elapsed), move(states),
        [](const State& state) -> Snapshot { return state.side; },
        [&](const State& state) -> Cost { return evaluate(edges, state.side); },
        [&](State& state, const sa::SaRuntime<Cost>& runtime) -> Cost {
            (void)runtime;  // このruntimeは今走っている内部SAの情報。
            state.pending_vertex = int(state.rng() % n);
            const int v = state.pending_vertex;
            Cost delta = 0;
            for (auto [u, w] : adjacent[v])
                delta += state.side[v] == state.side[u] ? -w : w;
            return delta;  // まだ反転しない。
        },
        [](State& state, bool accepted) -> void {
            if (accepted) state.side[state.pending_vertex] ^= 1;
        },
        population,
        std::ref(csv));

    // sa_popは全個体・全段階の最良Snapshotを必ず返す。optionalではない。
    assert(evaluate(edges, best_snapshot) == best_cost);
    cout << total_weight - best_cost << '\n';  // 最小化costを、元の最大化スコアへ戻す。
    for (int i = 0; i < n; ++i) cout << (i ? " " : "") << best_snapshot[i];
    cout << '\n';
}
```

#### 呼び出しと戻り値の利用

1. 入力の辺から隣接リストを作り、異なるseedで8個の `State` を用意する。
2. `SaParam` は共通の温度など、`SaPopParam` は選別などの設定。最初は両方とも既定値を使う。
3. `move(states)` で個体群を渡す。関数はvectorを値で受けるため、moveしなければ個体群がコピーされる。渡した後の `states` の中身を出力に使わない。
4. 各コールバックには、今操作すべき `State&` が渡る。初期解用の1個体をラムダに固定して、全呼び出しでそれだけを操作するような実装にしない。
5. 戻り値は `pair<Cost, Snapshot>` なので、そのまま分解して受け取れる。全体ベストSnapshotがあり、optionalの分岐は要らない。
6. `total_weight - best_cost` を出力して、最大化したかったスコアへ戻す。

自己ループはこの差分式では扱わない。コードは自己ループなしを入力条件としている。重複辺は別々の辺として足し込むので使える。辺のないグラフや重み0も有効である。

この例ではどの頂点も反転できるため、`propose` は数値の `Cost` を返す。固定頂点などの制約を加える場合は、中継ラムダも含めて `optional<Cost>` にし、動かせない頂点を選んだら `nullopt` を返せばよい。受理後に反転する書き方なので、`finalize(false)` は何もしない。

### 4.6 CSV・終了診断を使って確認する

4.2、4.5、4.7はCSV Hookを渡している。`-DLOCAL` を付けたときだけイベントが呼ばれ、CSVが書かれる。通常ビルドでは、両CSV Hookは同じコンストラクタと呼び出し形式を持つ空実装で、直接呼んでも記録・出力しない。`rows` などの記録メンバーはLOCAL時だけ存在する。

```sh
g++ -std=c++20 -O2 -DLOCAL -I include examples/01_load_single.cpp -o solver_local
./solver_local 100 < input.txt
```

| Hook | 既定の出力 | 主な確認内容 |
|---|---|---|
| `SaCsvStatHook<Cost>` | `sa_stat.csv`、50ms間隔 | 温度、現在cost、最良cost、提案数、受理数など |
| `SaPopCsvStatHook<Cost>` | `sa_pop_trace.csv` | 個体の内部SA実行・選別と最終SA |
| 同上 | `sa_pop_phase.csv` | フェーズごとの生存個体・時間・統計 |
| 同上 | `sa_pop_summary.csv` | 自動温度推定、全体時間、最終成績 |

保存名を変える場合は、sa_funcなら `SaCsvStatHook<Cost>("run.csv", 50.0)`、sa_popなら `SaPopCsvStatHook<Cost>("run_")` のように構築する。sa_funcの第2引数は記録間隔のミリ秒で、0以下は1マイクロ秒へ補正される。極端に短くすると記録とメモリの負担が大きくなる。

`accept_rate` は記録区間の受理回数÷全反復数、`worse_accept_rate` も悪化受理回数÷全反復数である。後者は「悪化案のうち何割通ったか」ではない。温度推定の提案数は探索本体の反復数に含まれない。

棄却を区別する列は次のとおり。いずれも事前の温度推定を含めず、`nullopt` は全反復数に含める。

| CSV | 棄却の列 | 対象範囲 |
|---|---|---|
| sa_func | `propose_rejected_count`、`sa_rejected_count` | 本探索の開始から記録時点まで |
| sa_func | `period_propose_rejected`、`period_sa_rejected` | 前回記録からの区間 |
| sa_pop trace | `propose_rejected_count`、`sa_rejected_count` | その個体の今回の内部SA |
| sa_pop phase | `total_propose_rejected_count`、`total_sa_rejected_count` | 選別フェーズ内の合計 |
| sa_pop summary | `total_iterations`、`total_accepted_count`、`total_propose_rejected_count`、`total_sa_rejected_count` | 全内部SAと最後の仕上げの合計 |

`propose_rejected` は `nullopt` による棄却、`sa_rejected` は値を返した後のSA判定による棄却である。各CSVの `valid_accept_rate` は、受理回数÷（全提案数−`nullopt` 数）。sa_funcでは記録区間、sa_popではその行の集計範囲を使い、分母が0なら0を出力する。無効案が多く `accept_rate` が低いのか、有効案をSAが棄却しているのかを切り分けられる。

マルチスタートで同じファイル名を使い回すと、各RunEndで上書きされる。各回のCSVが必要なら、run番号ごとに名前を変える。sa_funcのHookは値で渡されるので、終了後に呼び出し側の `csv.rows` を読みたい場合は、例のように `std::ref(csv)` か参照キャプチャしたラムダを渡す。sa_popのCSV Hookは内部行データを公開しない。

LOCALでは通常の終了時に、ライブラリ内部の現在costと `get_cost()` の差がstderrへ表示される。整数なら `diff=0` を確認する。小数では計算順による誤差も考慮する。この診断は不一致を出力するもので、必ずassert停止する機能ではない。時間予算0の早期終了では診断されない。温度見積もりとは独立した機能であり、sa_funcで温度見積もりを有効にした場合は、時間予算0でも「観測データなし」の報告がある。

sa_funcは、自動推定で有効な悪化案が100件未満ならLOCALで警告する。sa_popの推定結果は `auto_worse_count` や `auto_fallback` をHook／summary CSVで確認する。CSVを含むLOCALビルドは処理速度が変わるため、提出用のスコアや反復数は通常ビルドでも確認する。

### 4.7 初期状態で悪化案が出ない割当問題：温度を見積もって使う

#### 問題と、初期推定ができない理由

4.2と同じく、仕事を機械へ割り当て、負荷の二乗和を小さくする。ただし、ここでは仕事も機械も2個以上、仕事の重みはすべて正とし、**全仕事を機械0へ集めた初期解**から始める。

機械数を $m$、割当を $x$、機械 $j$ の負荷合計を $L_ {j}(x)$ とすると、目的は次である。

$$\min_ {x\in\mathcal{X}} C(x),\qquad C(x)=\sum_ {j=0}^{m-1} L_ {j}(x)^2$$

$\mathcal{X}$ は各仕事をどれか1機械に割り当てる全方法。重い機械へ仕事が集中すると二乗和が大きくなるため、このcostを下げると負荷が分散する。

全仕事の重み合計を $W$、移す1仕事の重みを $w$ とする。初期解では、移動先の機械は必ず空であり、移動後−移動前の差分は次になる。

$$\Delta=(W-w)^2+w^2-W^2=-2w(W-w)<0$$

正の重みの仕事が2個以上あるので、$0<w<W$。したがって、初期状態からの移動はすべて改善になる。初期の自動推定は毎回取り消して同じ状態で採取するため、有効な悪化差分を1つも集められない。

一方、実際に仕事を分散させれば、重い機械へ戻すような悪化案も現れる。本探索を最後まで観測する温度見積もりは、この情報を利用できる。

#### 完成コード

ファイル: `examples/05_load_temperature.cpp`。入力・出力の形式は4.2と同じ。重みの下限など、この例の入力条件はコメントとassertに示している。

```cpp
// 入力: n m / w[0] ... w[n-1]。出力: 最小化したcost / 各仕事の担当機械(0始まり)。
// この例の範囲: 2<=n<=2000, 2<=m<=256, 1<=w[i]<=100000。
// -DLOCAL で実行すると、本探索で得た悪化差分から次回用の温度候補をstderrに出す。
#include "sa_func_v17.hpp"
using Cost = long long;
using Snapshot = vector<int>;

Cost evaluate(const vector<Cost>& weights, int machines, const Snapshot& assignment) {
    vector<Cost> load(machines, 0);
    for (int i = 0; i < (int)weights.size(); ++i) load[assignment[i]] += weights[i];
    Cost cost = 0;
    for (Cost x : load) cost += x * x;
    return cost;
}

int main(int argc, char** argv) {
    const auto started = chrono::steady_clock::now();
    // 引数なしなら外側1950ms。短い動作確認では ./solver 20 のように指定できる。
    const double total_ms = argc >= 2 ? stod(argv[1]) : 1950.0;
    assert(isfinite(total_ms) && total_ms >= 0.0);
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    if (!(cin >> n >> m)) return 0;
    assert(2 <= n && n <= 2000 && 2 <= m && m <= 256);
    vector<Cost> weights(n);
    for (Cost& x : weights) { cin >> x; assert(1 <= x && x <= 100000); }

    mt19937_64 rng(123456);  // 近傍生成用。SAの受理乱数とは別。
    Snapshot assignment(n, 0);  // 全仕事を機械0へ集める。ここからの移動はすべて改善。
    vector<Cost> load(m, 0);  // 差分計算用キャッシュ。Snapshotには含めない。
    for (Cost w : weights) load[0] += w;
    int job = 0, from = 0, to = 0;  // proposeからfinalizeへ渡す今回の変更内容。

    auto get_snapshot = [&]() -> Snapshot { return assignment; };
    auto get_cost = [&]() -> Cost {
        // キャッシュとは独立に計算し、LOCALの終了診断で反映ミスも検出する。
        return evaluate(weights, m, assignment);
    };
    auto propose = [&]() -> Cost {
        job = int(rng() % n);
        from = assignment[job];
        to = from;
        to = int(rng() % (m - 1));
        if (to >= from) ++to;  // 現在と異なる機械を選ぶ。
        const Cost w = weights[job], a = load[from], b = load[to];
        // 提案後cost - 提案前cost。ここではまだ状態を変えない。
        return (a - w) * (a - w) + (b + w) * (b + w) - a * a - b * b;
    };
    auto finalize = [&](bool accepted) -> void {
        if (!accepted || from == to) return;
        load[from] -= weights[job];
        load[to] += weights[job];
        assignment[job] = to;
    };

    sa::SaParam param;
    param.seed = 1;
    // 初期状態での自動推定は悪化案がなく失敗し、既定の1000.0→0.1で本探索を行う。
    param.enable_temperature_report = true;  // LOCAL限定の観測。今回の温度は変えない。
    // 次回はstderrの param.auto_mode / start_temp / end_temp の3行をここへ貼る。
    // auto_mode=false も必要。提出時は -DLOCAL を外すと観測・診断出力が消える。
    sa::SaCsvStatHook<Cost> csv;  // LOCAL時のみ sa_stat.csv に記録する。
    const double elapsed = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();
    auto result = sa::sa<Snapshot, Cost>(
        param, max(0.0, total_ms - elapsed),
        get_snapshot, get_cost, propose, finalize,
        nullopt,       // 単体SAなので外部の既存ベストを指定しない。
        std::ref(csv));

    // 既存ベストを指定しないので、時間予算0でもSnapshotが必ずある。
    assert(result.best_snapshot);
    const Snapshot& answer = *result.best_snapshot;
    assert(evaluate(weights, m, answer) == result.best_cost);
    // AHCで使う場合は、この出力部分を問題の指定形式に合わせる。
    cout << result.best_cost << '\n';
    for (int i = 0; i < n; ++i) cout << (i ? " " : "") << answer[i];
    cout << '\n';
}
```

#### 実行から設定まで

入力例を `input.txt` に保存する。

```text
6 2
1 2 3 4 5 6
```

初期の負荷は `[21, 0]`、costは441である。重み1を空の機械へ移すと `[20, 1]`、costは401になり、差分は−40となる。最初から悪化案がない理由を手計算でも確認できる。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -DLOCAL -I include examples/05_load_temperature.cpp -o temperature_local
./temperature_local 1950 < input.txt > answer.txt 2> temperature.log
```

1. `get_cost` が初期costを返し、`get_snapshot` が初期割当を保存する。
2. 初期推定では `propose` と `finalize(false)` を繰り返す。悪化がないので、手動値 `start_temp=1000.0`、`end_temp=0.1` で本探索へ進む。
3. 本探索では `finalize(true)` が採用された移動を反映する。温度見積もりは、採否にかかわらず本探索で提案された有限の悪化差分を観測する。
4. 最良costと対応するSnapshotが戻る。既存ベストを指定していないのでSnapshotは必ずあり、`answer.txt` にはこの保存解を出す。温度候補は `temperature.log` へ出る。
5. ログ内の `param.auto_mode`、`param.start_temp`、`param.end_temp` の3行を、コード中の指定コメントへ貼り付ける。これで次回は手動温度として使われる。
6. 次回も観測したければフラグをtrueのままにする。提出用は `-DLOCAL` を外してコンパイルし、出力部分を問題指定の形式へ合わせる。

| ログの項目 | 読み方 |
|---|---|
| `worsening` | 本探索で提案された、有限な正の差分の総数。採用された悪化だけの数ではない |
| `quarters=[n1,n2,n3,n4]` | SAの時間予算を4分割した各区間の標本数 |
| `width_rejected` | 通常の悪化受理で、幅条件を満たさなかった有限の悪化提案数。確率判定で落ちた数とは別 |
| `source quarter ... start=..., end=...` | 開始・終端の見積もりに使った区間番号1〜4。0は全体平均を使用。温度の数値ではない |
| `param.start_temp`、`param.end_temp` | 次回用の実際の温度候補 |
| `recommendation unavailable` | 候補を計算できなかった理由。数値設定の3行は出ない |

ログの数値は、入力、制限時間、CPU、探索経路によって変わる。初期推定の「悪化標本0・手動値を使用」という警告と、本探索後の温度候補は両立する。温度候補が出たことだけで解品質が向上したとは判断しない。使う問題と同じcost・近傍・代表的な入力で見積もり、実行結果を確かめる。

### 4.8 スケルトンを自分の問題へ合わせる

同梱の `skeltons/sa_func_skelton_v03.cpp` と `skeltons/sa_pop_skelton_v03.cpp` は、問題固有の処理を埋める雛形である。未編集では空の解・cost 0のまま動き、問題を解く実装にはならない。完成例から始めたい場合は4.2〜4.5、4.7を使う。

| 埋める箇所 | 決めること |
|---|---|
| `Input`と入力処理（sa_pop版は`read_input`） | 入力の保持方法と読み取り |
| `Cost`、`compute_cost` | 小さいほど良い有限のcost。整数なら符号付き型 |
| `Snapshot`、`Snapshot::print` | 出力に必要な保存データと問題指定の出力形式 |
| 初期解の構築 | 制約を満たす解と、その解に一致するキャッシュ |
| `propose`、`finalize` | 差分の作成、採用時の確定、却下時の巻き戻し |
| `param`、`pop_param` | まず既定値。手動温度など、必要な設定だけ変更 |

sa_func版は `propose` で操作を記録し、`finalize(true)` で反映する書き方。sa_pop版は個体ごとの `WorkState` に解・キャッシュ・乱数・巻き戻し情報を置き、`propose` で仮適用し、`finalize(false)` で元へ戻す書き方である。どちらも第2章の契約を満たすように埋める。

両スケルトンの `propose` は `optional<Cost>` を返す。`pending_move.l < 0` の箇所は、問題に合った操作の有効性判定へ置き換える。未編集なら `nullopt` になる。sa_pop版の `pending_move.applied` は仮適用前の棄却を安全に扱うためのフラグで、変更した場合だけ巻き戻す。常に有効な操作を作るなら、関数と中継ラムダの戻り値を `Cost` にしてもよい。

sa_func版は `main` 内でコールバックを用意して `sa::sa` を呼ぶ。外部の既存ベストを使うときは `existing_best_cost` に値を設定し、`result.best_snapshot` が空なら外部で保持している解を使う。単体利用では `nullopt` のままでよい。

sa_pop版は `main → read_input → solve → Snapshot::print` と進む。`solve(input, time_limit_ms, initial_state_count, seed, csv_hook)` の時間引数は、その関数へ残されたミリ秒数。個体数の既定値は32、seedは1、CSV Hookも省略できる。ライブラリ本体が個体数32を自動生成するのではなく、この雛形が `make_initial_work_states` で作る。`solve` は全探索の最良Snapshotを返す。

両雛形とも入力前から外側の時刻を計り、初期解構築に使った時間も差し引く。sa_pop版は `main` で入力時間を、`solve` で初期個体の構築時間などを差し引く。既定の外側1950msは、問題の制限と初期化・出力の重さに合わせて変える。

#### 雛形で温度を見積もる

| 雛形 | 設定と実行時の動作 |
|---|---|
| sa_func版 | `param.enable_temperature_report = true;` として `-DLOCAL` で実行。その単体SAの過程から次回用の温度を表示する |
| sa_pop版 | LOCAL専用ブロックの `constexpr bool estimate_temperature = true;` として `-DLOCAL` で実行。通常の個体群探索に代わり、1個体を単体SAで制限時間まで走らせる |

例えばsa_pop版を使う場合は、TODOを埋めた後で次のようにコンパイルする。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -DLOCAL -I include skeltons/sa_pop_skelton_v03.cpp -o population_local
./population_local < input.txt > answer.txt 2> temperature.log
```

sa_pop版の見積もり分岐は、共通コールバックと `pop_param.acceptance_width_scale` を使う単体SAである。通常のsa_popを走らせた後に別のSAを追加する処理ではない。この分岐で返るSnapshotも、見積もりに使った1個体の探索結果となる。

見積もり後は `estimate_temperature=false` へ戻し、ログの3行をスケルトンの指定コメントへ貼り付けて通常のsa_popを実行する。`auto_mode=false` なので、`auto_end_temp_scale` が手動終端温度に重ねて掛かることはない。単体で得た値は集団探索への参考値であり、選別や時間配分まで含めた最適温度ではない。

この分岐は `#ifdef LOCAL` 内にある。`LOCAL`なしのビルドでは、フラグの記述にかかわらず通常のsa_popを実行する。普通のsa_popで `param.enable_temperature_report=true` だけを設定した場合は、各内部SAの短い時間区間について別々の診断が出るので、上記の1個体の見積もりとは区別する。

## 5. 制約・注意点

### 5.1 型・数値・入力

| 制約・落とし穴 | 対応 |
|---|---|
| 最大化スコアをそのままcostとして渡す | 符号を反転するか、固定値から引いて最小化へ変換する |
| costを独自の構造体や配列で表す | Costは整数・浮動小数の数値型。複数の評価軸は1つの数値へまとめる |
| `unsigned` をCostに使う | 改善差分が負になるため不可。符号付き整数を使う |
| costや差分が型の範囲を超える | 乗算前から十分大きな型で計算し、絶対値・加算・差分をすべて収める |
| 制約違反を罰則costだけで扱い、返却解の有効性を確認しない | ライブラリは有効性を検査しない。まずは有効な初期解と、制約を保つ近傍を設計する |
| costや差分にNaN・無限大を返す | 探索用の値は有限に保つ。禁止手は `optional<Cost>` の `nullopt` で棄却する |
| 手動温度0、負の温度、受理確率0や1 | 不可。温度は有限の正数、確率は0と1の間 |
| autoなら手動設定を不正値にしてよいと思う | sa_funcはautoでも引数の前提を検査する。失敗時にも使うので有効値を残す |
| 受理幅が負やNaN | 不可。非負、または正の無限大を使う |
| 既存ベストにNaNを渡す | 不可。未指定は `nullopt` で表す。正負の無限大は比較基準としては指定できる |
| 大きな共通定数をcostに足す | 不要なら外す。温度・選別キーはdoubleで計算し、極端な桁差では細かな違いを表しにくい |
| 反復数や統計がintの範囲を超える | 本体のintカウンタが表せる実行規模に収める |

関数の入力前提は主にassertで確認される。`-DNDEBUG` では検査が消えるので、不正な値を渡しても安全に補正されるとは考えない。目的関数や制約は利用者側の責任である。

### 5.2 状態・保存・戻り値

| 制約・落とし穴 | 対応 |
|---|---|
| `propose` が絶対costを返す | 必ず変更後−変更前を返す |
| 仮適用と受理後適用が混ざり、2回変更する | 1つの近傍ごとに、どこで適用しどこで戻すか決める |
| 巻き戻しでキャッシュを戻し忘れる | 解と解依存キャッシュをセットで元に戻す |
| `nullopt` 時に前回の変更箇所が残る | 必ず `finalize(false)` が来る。毎提案、未変更フラグや巻き戻し情報を初期化する |
| 差分0を棄却だと思う | 0は同値の有効提案として受理される。強制棄却には `nullopt` を返す |
| ラムダで数値と `nullopt` をそのまま混在させる | 戻り値推論に任せず `-> optional<Cost>` を指定する。中継ラムダにも型を合わせる |
| get_snapshot内で必須の状態更新をする | 保存条件を満たさないと呼ばれない。状態更新はfinalizeに置く |
| Snapshotが探索状態を参照している | 保存後に変化しない値を持つ |
| 既存ベストを指定したのにSnapshotを無条件で参照する | optionalが値を持つか確認する |
| 未改善時にも外部ベストをresult.best_costで上書きする | Snapshotが返った場合だけcostと保存解を一緒に更新する |
| 探索終了時の状態を答えとして出す | 返却Snapshotを使う。終了状態を最良状態へ自動復元する機能はない |
| 同じcostの別解も自動保存されると思う | 最良更新は厳密な不等号。同値解を選び直す仕組みではない |
| Snapshotの型要件を混同する | sa_funcは移動構築できる型。sa_popはコピー構築可能で、取得したSnapshotを代入できる型が必要 |

`sa_func` は移動専用Snapshotも使える。`sa_pop` はSnapshotのコピー構築を静的に要求するので、移動専用型はそのまま渡せない。`vector` を持つ通常の値型なら両方で使いやすい。

### 5.3 sa_pop固有の注意

| 制約・落とし穴 | 対応 |
|---|---|
| 空の個体群を渡す | 1個以上を用意する |
| 256個を超える | 上限は固定。max_state_countだけ変えても増えない |
| 個体ごとに異なる問題・異なるcost尺度を使う | 同じ問題・同じ目的関数で比較可能な個体を渡す |
| 同じ初期解・同じ近傍乱数を全個体へ複製する | 初期解や乱数を意識して変える。ライブラリは初期解を生成しない |
| WorkStateの中に共有された変更可能データを置く | 個体ごとに独立させる。入力などの読み取り専用データは共有できる |
| 元のstatesをラムダで直接操作する | 渡されたState&を操作する。特にmove後の元vectorを参照しない |
| SaParamの受理幅を変更する | sa_popではSaPopParamの同名項目を変更する |
| `inner_runtime.best_cost` を個体の全期間最良と解釈する | 内部SAごとの最良。全期間の値は外側のlifetime_best_cost関連情報 |
| `runtime.progress()` を全体進捗と解釈する | 内部SAの予算に対する進捗。全体用には別の時計が必要 |
| 過去の最良Snapshotへ各個体が戻ると思う | 個体別の最良状態は保存・復元しない。現在状態から継続する |
| 脱落した個体のメモリがすぐ解放されると思う | 全WorkStateは呼び出し終了まで保持される |
| `final_state_id` を返却Snapshotの発見者と解釈する | 最終SAを走らせた個体のID。返却解の取得元とは限らない |
| sa_funcの独自 `sa_accept_worse` がそのまま使えると思う | sa_popの内部ラッパーはこのメンバーを転送しない |
| 温度見積もりフラグで全個体共通の候補が出ると思う | 出力は内部SAごと。1個体での見積もりは4.8の雛形を使う |

個体数1でもsa_popは使えるが、sa_funcと完全に同じ乱数列・計時になる保証はない。選別しない用途なら、単体SAの方が呼び出しは簡単である。

### 5.4 時間・乱数・デバッグ

時間引数はミリ秒の `double`、内部はマイクロ秒の整数で管理する。1マイクロ秒未満の端数は切り捨てる。sa_funcは有限の負の時間を0へ丸める一方、sa_popは非負を前提とする。両方へ渡すコードでは `max(0.0, remaining_ms)` に揃えると分かりやすい。

時刻確認は通常32提案ごとなので、重い提案では制限を超過し得る。sa_funcでは初期cost評価と初期Snapshot取得が内部時計開始前。sa_popでは最初の全個体cost評価が外側時計開始前。引数の個体群コピーも関数本体へ入る前に起こる。終了診断・CSV出力・利用者の出力まで含めた厳密な締め切りを保証するものではない。

固定seedでも、壁時計による温度と終了時刻が変われば結果は変わる。SAの `param.seed` は、利用者が作る初期解や `propose` 内の乱数のseedを自動設定しない。両方を記録しておく。

DebugHookはLOCAL時だけ呼ばれる。Hookに解の更新・最良解の必須保存・終了判定などを置くと、通常ビルドでそれらが消えてしまう。観測やログ出力のために使う。独自Hookをsa_funcへ渡す位置は第8引数なので、既存ベスト不要なら第7引数に `nullopt` を置く。sa_popでは第8引数がSaPopParam、第9引数がHookである。

ヘッダ末尾の `#if __INCLUDE_LEVEL__ == 0` 内は、ヘッダを単体実行するためのテスト・例である。通常のincludeでは入らない。1ファイル提出用に手作業で展開するときは、このブロックまで貼って複数のmainを作らないようにする。

### 5.5 温度見積もりの制約

| 条件・落とし穴 | 動作・対応 |
|---|---|
| `LOCAL`なし、またはフラグがfalse | 温度見積もりの観測・出力は行わない |
| 本探索で有限の悪化差分を観測できない | 数値候補は出ない。改善だけで解ける近傍なら、悪化受理用の温度をこの観測からは決められない |
| 時間予算0、または初期推定などで時間を使い切る | 本探索のデータがなく、数値候補は出ない |
| 独自 `propose.sa_accept_worse` を使う | 通常の受理式に基づく推奨は行わない。独自規則の調整が必要 |
| 受理幅0 | 悪化受理を無効にしているため、数値候補は出ない |
| 最初・最後の観測区間の標本が100未満 | それぞれ全体平均で補う。全体でも100未満なら候補とともに不確かさを警告する |
| 候補温度が非有限または0以下になる | 数値候補は出ず、表現範囲外である旨を表示する |
| 候補の終端温度が開始温度を超える | 候補と加熱の警告を表示する。自動補正はしないので、その設定が目的に合うか確認する |
| `width_rejected` を実測の受理率とみなす | 幅条件で落ちた件数。確率判定の却下を含む総却下数ではない |
| 温度だけコピーし、`auto_mode=true` を残す | 自動推定成功時はコピーした手動値が使われない。出力された3行とも設定する |
| costの尺度・近傍・問題サイズを変えて同じ値を使う | 悪化量の尺度も変わり得る。代表的な入力・同じ評価と近傍での候補として扱う |

この機能は、複数の温度設定を競わせて最良スコアを選ぶ探索ではない。悪化提案の大きさを手動温度へ変換する診断である。保存された最良Snapshotから別の探索を始めたり、探索状態を復元したりする機能でもない。

## 6. 実装

### 6.1 全体の流れ

sa_funcは1つの状態を対象に、次の順序で動く。

1. 現在costを取得し、そのSAの最良costにする。保存基準を満たせば初期Snapshotを取る。
2. 指定されていれば、初期状態からの変更案で温度を自動推定する。推定用の案はすべて却下して状態を戻す。
3. 時間の進みに合わせて温度を変え、変更案を作る。`nullopt` は強制棄却し、数値ならSAの採否を決める。
4. `finalize` で状態を確定してから、現在cost・最良cost・統計を更新する。
5. 保存基準を満たす最良更新だけSnapshotを取る。
6. 時間切れになったら、LOCALで有効な終了Hook・温度見積もり・終了cost診断を行う。
7. ローカル最良costとoptionalのSnapshotを返す。

sa_popは複数の状態を対象に、次の順序で動く。

1. 全初期個体のcostを評価し、最良初期解のSnapshotを1つ保持する。
2. 全個体から集めた悪化差分で、共通の温度を1回推定する。
3. 生存している個体へ短い内部SAを順番に割り当てる。
4. 各個体の現在cost・過去最良costから順位を付け、下位を落とす。
5. 3と4を繰り返して1個体に絞る。
6. 残り時間でその個体の内部SAを実行し、全体の最良Snapshotを返す。

sa_popは複数のSAを単一スレッドで順番に動かす。スレッド並列化、個体同士の解の交叉、残った個体を増殖させる処理はない。

### 6.2 採否判定と温度の役割

提案差分が0以下なら必ず採用する。悪化差分 $\Delta>0$ は、通常は受理幅の条件を満たした上で、次の確率で採用する。

$$P=\exp(-\Delta/T)$$

$P$ は採用確率、$T>0$ は温度、$\exp(z)$ は $e^z$ のことで、$e$ は約2.718の定数である。温度が高いと同じ悪化量でも通りやすく、温度が低いと通りにくい。

例えば悪化量10なら、温度10では確率判定部分が約0.368、温度100では約0.905。ただし温度10・幅1.0で最良点から提案した場合、幅条件は厳密な不等号なので、差がちょうど10の案は確率判定の前提を満たさない。

確率には、0以上1未満の乱数 $u$ を使い、`u < exp(-delta/temperature)` かどうかを判定する。改善・同値では受理判定用の乱数を消費しない。悪化では幅条件を満たさない場合も乱数を1つ消費する。近傍生成用の乱数は利用者側の責任であり、これとは別である。

自動温度の式は、典型的な悪化量 $D$ の確率を $p$ にするという式 $p=\exp(-D/T)$ を、$T$ について解いたもの。ただし、実際には幅制限と終端倍率が加わるため、観測される受理率そのものの指定にはならない。

### 6.3 指数補間による温度スケジュール

開始温度を $T_ {s}$、終端温度を $T_ {e}$、時間の消費割合を $r$ とすると、温度を次のように変える。

$$T(r)=\exp\left((1-r)\log T_ {s}+r\log T_ {e}\right)$$

$r$ は0〜1。$r=0$ で開始温度、$r=1$ で終端温度になる。例えば1000から1へ下げる場合、半分の位置では約31.62であり、算術平均の500.5ではない。時間が同じだけ進むごとに、ほぼ同じ倍率で下がる形になる。

sa_funcは温度の対数を開始時に計算し、毎反復の高価な計算を抑える。最初の反復には初期温度を使い、2反復目と、それ以後の時刻更新時に補間する。正の時間予算での通常終了イベントでは終端温度を通知する。時間予算0の早期終了では初期温度のままである。温度推定に使った時間も内部の時間予算に含まれる。

### 6.4 sa_funcの保存と独自受理判定

現在costは受理した差分を足して更新し、初期値を含むローカル最良costを別に保持する。`existing_best_cost` を渡しても、このローカル最良の計算には混ぜない。ローカル最良を更新したときにだけ、さらに外部の保存基準を確認する。

この分離により、良くない初期解が外部ベストへ近づく途中のコピーを省きながら、内部SA自身の最良を基準にした受理幅は維持できる。Snapshotがない場合でも、温度推定・提案・採否・ローカル最良の更新は続く。

上級者向けには、proposeを関数オブジェクトにし、`sa_accept_worse(const SaRuntime<Cost>&, Cost delta, double u)` というboolへ変換可能な結果を返すメンバーを持たせると、sa_funcの悪化時の判定を置き換えられる。$u$ はライブラリが生成する0以上1未満の乱数。この場合、通常の幅制限とMetropolis判定は自動では適用されない。改善・同値の自動受理は残る。

通常の利用ではこの拡張は不要である。sa_popは利用者のproposeを内部ラムダ経由で呼ぶため、このメンバーは転送されない。

### 6.5 sa_popの個体数・時間・温度の配分

現在の生存個体数を $a$ とすると、次の個体数を次で決める。

$$a_ {\mathrm{next}}=\max(1,\lfloor 3a/4\rfloor)$$

$\lfloor z\rfloor$ は小数点以下を切り捨てる操作である。8個体なら、8→6→4→3→2→1の順に減る。選別フェーズ数は5。個体数1なら選別フェーズはない。

選別時間の締め切りは、全体予算に `selection_time_ratio` を掛けた時刻である。各フェーズでは、選別の残り時間を残りフェーズ数で均等に割る。そのフェーズ内でも、残り時間を未実行の生存個体数で均等に割る。途中の処理時間を差し引きながら配り直すので、固定した1回分の時間を単純に繰り返す方式ではない。

温度の全体スケジュール上で、選別部分は進捗0から `selection_time_ratio` まで。これをフェーズ数で等分する。同じフェーズにいる各個体は、同じ開始・終端温度の区間を通る。個体数が減るにつれて、残った個体に配られる時間は増えやすい。

最後の1個体は、温度の進捗 `selection_time_ratio` から1までを、残り時間で進める。最初から1個体なら0から1の全区間を使う。温度区間の進捗と、各内部SAの `runtime.progress()` は別の値である。

### 6.6 sa_popの選別と全体最良の保持

各個体について、現在costと、全フェーズにわたる過去最良costを数値として記録する。現在状態は内部SAの終了状態のまま次のフェーズへ進む。過去最良costを選別に使っても、その当時の個体状態へ復元することはない。

選別キーはCurrentなら現在cost、LifetimeBestなら過去最良cost、Combinationなら第3章の合成値である。キーが同じなら、過去最良cost、現在cost、初期の個体IDの順で決める。IDは `work_states` の初期添字で、0始まり。

生存個体はIDの配列で管理し、WorkStateそのものを選別のたびに並べ替えたりコピーしたりはしない。脱落個体はその後探索されないが、WorkStateを格納するvectorのメモリは終了まで残る。

返却用には全体最良Snapshotを1つ別に持つ。内部SAへその全体最良costを保存基準として渡し、更新できた場合だけSnapshotを取得する。したがって、途中で脱落した個体が最も良い解を見つけていた場合も、その解を返せる。最後に探索した個体と、返却解を見つけた個体が一致するとは限らない。

### 6.7 LOCALの探索後温度見積もり

観測するのは**本探索のproposeが返した有限の正の差分**であり、初期の自動推定サンプルと `nullopt` は含まない。悪化が採用されたかどうかには依存しない。採用された提案だけを集めると、受理されやすい小さな悪化へ偏るためである。

SAの時間予算を4等分し、`runtime.progress()` に応じた区間へ標本を入れる。各区間で件数と逐次平均だけを保持するので、全差分を保存する必要はない。進捗は既存の時刻情報から取得するため、観測のために毎提案で時計を読む処理も追加しない。区間は本探索だけの時間を改めて4等分したものではなく、初期推定も含むSA予算全体に対して決まる。

各区間の平均と件数から、全標本を件数で重み付けした全体平均を求める。開始側は**悪化を最初に観測した区間**、終端側は**最後に観測した区間**を使う。それぞれ100標本以上あればその区間平均を使い、100未満なら全体平均に置き換える。「100個以上ある区間を順に探す」という処理ではない。

こうして選んだ開始側の平均悪化量を $D_ {s}$、終端側を $D_ {e}$、パラメータの開始・終端基準確率をそれぞれ $p_ {s}$、$p_ {e}$ とすると、候補は次のようになる。

$$\widehat{T}_ {s}=-D_ {s}/\log(p_ {s})$$

$$\widehat{T}_ {e}=0.1\left(-D_ {e}/\log(p_ {e})\right)$$

$\widehat{T}_ {s}$ と $\widehat{T}_ {e}$ は次回用の開始・終端温度候補であり、今回使用中の温度ではない。$\log$ は自然対数。終端倍率0.1は、このsa_funcの診断では固定である。sa_popから呼ばれる内部SAの診断にも同じ式が使われ、`SaPopParam.auto_end_temp_scale` で置き換わることはない。

初期付近で悪化量が大きく、終盤では小さい問題でも、開始側と終端側を分けて見積もれる。ただし受理幅による切り捨てを数式で補正する処理はなく、`width_rejected` を参考情報として出す。5.5の推奨不能条件では数値の設定行を表示しない。

観測の処理量は1提案あたり定数、追加メモリも4区間分の定数である。観測がSAの乱数を消費したり、Snapshotを追加取得したり、状態やパラメータを変更したりすることはない。LOCALで有効にしたときだけ集計・表示する。壁時計で停止するため、この追加処理によって同じseedでも反復数や最終解が変わる可能性はある。

### 6.8 Hookと計算量

sa_funcのイベントは `RunStart`、`IterationStart`、`IterationEnd`、`RunEnd`。`RunStart` は初期評価・必要な初期保存・温度決定の後。各反復では、IterationStart、propose、採否、finalize、cost・保存・統計の更新、IterationEndの順となる。RunEndの後に、有効なら温度見積もり、続いて終了cost診断を行う。時間予算0ではRunEndと温度報告は行うが、終了cost診断は行わない。

sa_popのイベントは `RunStart`、`AutoSampleStart`、`AutoSampleEnd`、`PhaseStart`、`StateRunStart`、`StateRunEnd`、`StateSelection`、`PhaseEnd`、`FinalRunStart`、`FinalRunEnd`、`RunEnd`。自動推定を行わなければAutoSampleのイベントは発生しない。全体時間0ならRunStartとRunEndだけになる。sa_popのRunStartは温度推定の前なので、推定結果はAutoSampleEnd以降で読む。

Hookに渡る `SaPopRuntime<Cost>` は、そのイベントに対応する情報を持つ。`phase` は0始まりのフェーズ番号、`state_id` は対象個体、`rank` は0が最上位、`survived` は生存判定、`final_polish` は最終SAかどうか。`current_cost_before/after`、`run_best_cost`、`lifetime_best_cost_before/after` は対象個体の評価値である。`final_best_cost` は終了段階での返却cost。対象のない項目は既定値のことがあるので、イベントの意味と組み合わせて読む。

外側の `elapsed_us/time_limit_us` は全体時間、`inner_runtime` は内部SAの情報、`temp_progress_begin/end` は温度スケジュール上の区間。`auto_total_samples`、`auto_worse_count`、`auto_avg_worse_delta`、`auto_fallback` は自動推定の結果を確認するための値である。`SaPopRuntime` 自体に `progress()` メソッドはない。

おおまかな計算量を整理する。$I$ は本探索の総提案数、$S$ は温度推定の提案数、$B$ は実際のSnapshot保存回数、$P,F,G,C_ {g},H$ はそれぞれpropose、finalize、get_snapshot、get_cost、Hook1回の処理量とする。$C_ {g}$ は目的関数の値ではなく、評価に掛かる処理量である。

sa_funcの通常ビルドは、主に次の時間を使う。

$$O((S+I)(P+F)+BG+C_ {g})$$

LOCALでHookを使う場合はイベント数に応じた処理が加わる。温度見積もりを有効にすると、さらに本探索の各提案に定数時間の観測が加わる。Snapshotのコピーが重ければ、$G$ と保存回数 $B$ も効く。sa_funcが追加で持つ主なメモリはSnapshot1つで、探索状態は利用者側で保持する。

sa_popでは、$K$ を初期個体数とすると、個体の評価と選別に掛かる分が加わる。各フェーズで約4分の3に減るため、フェーズを通じた個体数の和は $O(K)$ に収まる。

$$O((S+I)(P+F)+BG+KC_ {g}+K\log K)$$

LOCALではさらに、イベント数を $E_ {h}$ として $O(E_ {h}H)$ が加わる。メモリはK個のWorkState、全体最良Snapshot1つ、個体管理配列が中心。個体管理配列は最大256用の固定配列である。CSV HookはLOCAL時だけ記録行をメモリにため、終了時にまとめて書く。

数値を返す `propose` はコンパイル時に数値経路へ振り分けられ、optionalの有無を判定する分岐は入らない。`optional<Cost>` 自体はヒープ確保を行わないが、値の有無の分岐や棄却統計には処理が必要である。重い差分計算の前に無効案を発見できれば、その計算を省ける。性能への影響は近傍の重さ・棄却率・最適化に依存する。

### 6.9 このガイドのコード例の検証

掲載した5つの完成コードを、そのままGCC 12.2・C++20でコンパイルし、通常、LOCAL、AddressSanitizer＋UndefinedBehaviorSanitizerの3構成で検証した。`-Wall -Wextra -Werror` を付け、警告もエラーとして扱った。

各例に10入力を用意し、時間予算0と25ms（温度見積もり例は0と100ms）で実行した。計300実行で、出力の要素数・値域・順列条件・独立に再計算した評価値・初期解以上の成績を確認した。LOCALの整数cost診断も差0を確認した。温度見積もり例では、初期推定の悪化標本が0でも、本探索後には有限の正の温度候補が出ること、予算0では候補が出ないこと、通常ビルドでは診断出力が消えることも確認した。出力された設定3行を貼り付けた手動実行についても、コンパイルと出力の整合性を確認した。空の辺集合、重み0、1機械、同一座標、入力上限付近の値などを含む。

さらに、掲載コードの `TspState` を使って64,000提案を検査し、差分、受理後のcost、却下時の完全な巻き戻し、順列の維持を確認した。最適解を必ず求めることを保証するテストではなく、コード例の実装整合性を確認するものである。

Sanitizer検証ではリーク検査を無効にし、不正なメモリアクセスと未定義動作を対象にした。数式についても、通常のMarkdown前処理で添字や記号が強調・HTMLなどへ崩れないことを確認している。検証コード・入力・結果は同梱ZIPの `validation` と `validate_examples.py` に収録した。
