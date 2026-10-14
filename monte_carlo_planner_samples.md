# MonteCarloPlanner サンプルコード集

対象は `monte_carlo_planner_v08.hpp` です。C++20以降を使います。説明とコードはこのファイルのAPIに対応し、各例は単独のプログラムとして実行できます。

数式をすべて先に覚える必要はありません。第1章で「何を比較するか」をつかみ、第3章から近い用途を選び、必要に応じて第2章へ戻る読み方ができます。

## 1. 何を解くライブラリか？

### 1.1 今の行動を、将来の得点まで考えて選ぶ

このライブラリは、**現在選べる行動のうち、将来を含めた平均得点が高そうな行動を一つ推薦する**ためのものです。

たとえば、目先の収入が大きい仕事を引き受けるか、費用を払って設備を改良し、その後の収入を増やすかを比べられます。配送時間、需要、故障などが毎回変わる問題にも使えます。天候が見えた後に次の行動を変える、といった将来の判断も表現できます。変動のない確定的な問題にも使えます。

ライブラリへ渡すのは、行動候補と、「その行動を取ると何が起こり、何点を得るか」を書いたモデルです。問題そのものを自動的に読み取るものではありません。グラフ、在庫、盤面などの表現と、合法な行動、遷移、採点は利用者が決めます。

主な使い方は次の二つです。

- **案を一回で評価する**：経路案や割り当て案を丸ごと一つの行動とし、評価関数で比較します。`mc_choose` が短く書けます。
- **一手ずつ進む問題を表す**：行動によって状態が変わり、その先でも行動を選びます。`MonteCarloPlanner` と利用者定義の `Model` を使います。

返るものは基本的に**今の一行動**です。将来の全行動列を返すAPIではありません。ただし、行動型を配列にすれば、用意済みの全体計画を一つの候補として選べます。

### 1.2 完全観測と部分観測

**完全観測**では、判断に必要な現在の状態がすべて見えています。現在地、荷物、残り資金などを `State` にまとめます。

**部分観測**では、実際の状態の一部しか分かりません。たとえば、箱の中身や将来の需要が見えない場合です。この場合は次を分けます。

- `State`：一試行の中で「真実はこうだった」と仮定する状態。
- `Info`：行動を選ぶ側が実際に知ってよい情報。観測結果や過去の行動など。

見えない状態の仮定は、利用者の `sample_root` が `Info` に合うように作ります。観測後の確率分布や候補集合をどう更新するかも、利用者が決めます。ライブラリが自動的に推定するわけではありません。

### 1.3 目的関数

一回の未来で得られる合計得点を、次で表します。

$$G=\sum_ {t=0}^{\tau-1}\gamma^t r_ {t}+\gamma^{\tau}F(S_ {\tau},I_ {\tau},z)$$

今の情報の下で、最初に行動 $a$ を取り、その後の行動ルールを $\pi$ としたときの平均得点を、次で表します。

$$Q_ {\pi}(a\mid I_ {0})=\mathbb{E}[G\mid I_ {0},a_ {0}=a,\pi]$$

その平均得点が最も大きい現在の行動を選びたい、というのが目的です。

$$a^{\ast}\in\arg\max_ {a\in A(I_ {0})}Q_ {\pi}(a\mid I_ {0})$$

記号は次の意味です。行動を選ぶ場面として、残り手数 $H$ は1以上とします。APIでは手数0も表せますが、その場合は選ぶ行動がありません。

| 記号 | 意味 |
|---|---|
| $H$ | 先読みする最大手数。たとえば、今日を含む残り5日なら5。 |
| $t$ | 何手目かを表す番号。最初の手は0。 |
| $\tau$ | その未来で実際に進んだ手数。途中で終了すれば $H$ より小さく、終了しなければ $H$。 |
| $S_ {t}$ | $t$ 手進んだ時点の実際の状態。一試行の中で仮定した見えない値も含む。 |
| $I_ {t}$ | $t$ 手進んだ時点で、行動を選ぶ側が知ってよい情報。完全観測では $S_ {t}$ と同じ。 |
| $a_ {t}$ | $t$ 手目に取る行動。現在の候補を示す記号 $a$ は $a_ {0}$ に当たる。 |
| $A(I_ {0})$ | 現在の情報で選べる行動の集合。 |
| $r_ {t}$ | $t$ 手目の行動と遷移から得る一手分の得点。支払いなら負にする。 |
| $\gamma$ | 割引率。0以上1以下。1ならすべての手の得点をそのまま足し、1未満なら遠い未来の得点を小さく数える。 |
| $\gamma^t$ | $t$ 手目の得点に掛ける倍率。たとえば割引率0.9なら、最初は1倍、次は0.9倍、その次は0.81倍。 |
| $z$ | 終了理由。「本当に終端へ着いた」か「先読み手数を使い切った」か。 |
| $F(S_ {\tau},I_ {\tau},z)$ | 終了時に追加する得点。未使用の在庫価値や到着ボーナスなど。不要なら0。 |
| $G$ | 一つの未来の合計得点。一手ごとの得点と終了時の得点を、割引率を掛けて足した値。 |
| $\sum$ | 指定範囲の値をすべて足す記号。ここでは0手目から最後の一手まで足す。 |
| $\pi$ | 最初の行動の後で、各時点の情報を見て行動を選ぶルール。見えない真実を直接使って行動を決めてはいけない。 |
| $\mathbb{E}$ | 起こり得る未来を、その起こりやすさで重み付けして平均する操作。「期待値」と呼ぶ。 |
| $Q_ {\pi}(a\mid I_ {0})$ | 現在の情報が $I_ {0}$ のとき、最初に $a$、その後は $\pi$ で行動した場合の期待得点。縦線 $\mid$ は「この条件の下で」を表す。 |
| $\arg\max$、$a^{\ast}$ | 最大の値を与える行動を選ぶ、という記号と、その選ばれた行動。最大値が同じ候補が複数あれば、そのうち一つ。 |

たとえば、半分の確率で10点、残り半分で0点なら、期待値は $0.5\times10+0.5\times0=5$ 点です。費用を小さくしたい場合は、得点を「費用にマイナスを付けた値」にすれば、同じ最大化の形で扱えます。

一手報酬で売上を数えたなら、終了時の得点で同じ売上をもう一度足してはいけません。終了時の得点は、まだ数えていない価値にします。

この目的関数の良い行動を、有限の計算予算で推定します。必ず最適解が得られるわけではありません。また、将来に使う行動ルールや見えない状態の仮定が変われば、今の行動の評価も変わります。あらゆる将来の方策について厳密な最適化を保証するAPIではありません。

## 2. APIリファレンス

### 2.1 導入、共通の型と制約

用語の「根」は今から行動を選ぶ出発点、「試行」は仮の未来を一回作って採点することです。「ロールアウト」は、途中から一定のルールで行動を選び、仮の未来を進める部分を指します。

`#include "monte_carlo_planner_v08.hpp"` で利用します。ヘッダは `bits/stdc++.h` を使うため、本書のビルド環境はGCCとlibstdc++です。C++20以降が必要です。

主な公開名は次の四つです。`Model` はライブラリの基底クラスではなく、利用者が必要な型とメソッドを持たせる任意の型です。

| 公開名 | 役割 |
|---|---|
| `mc_choose` | 候補と評価関数だけで、一行動を時間予算内に選ぶ関数。 |
| `MonteCarloPlanner<Model, Rng, Clock>` | 状態遷移、追加探索、候補追加、木の再利用などを扱う計画器。 |
| `MonteCarloNoise` | 既定RNGと、出来事に結び付けた乱数を作る機能。 |
| `MonteCarloFinishReason` | 一試行の終了理由。 |

以下では `Planner` を `MonteCarloPlanner<Model>` などの別名として使います。`Planner::Options` のように書く型は、各 `Planner` に属する公開の入れ子型です。

共通の契約は次のとおりです。

- 単一スレッドで使います。探索中や中断中に、参照するモデルの遷移・採点・推定分布を変更しないでください。変更したら `start` でやり直します。
- `State`、`Info`、`Action` はコピー可能にします。試行中に書き換える内容まで共有ポインタなどで共有すると、別の試行や実状態を汚すため、独立に更新できるようにします。固定グラフや読み取り専用の表は `Model` に置いて借用できます。
- 手数、件数、IDの添字、試行数は `int` に収まる範囲で使います。得点は有限の `double` で表現でき、二乗した得点と試行数の積なども `double` の範囲に収めます。NaN、無限大、極端な値を渡しません。
- 浮動小数の設定も有限値にします。`widening * sqrt(完了数 + 1)` は `int` に収まる範囲にし、時間指定も時計の表現範囲に収めます。AHCで使う通常の規模を想定しています。
- 不正な引数は回復用エラーとして返す設計ではありません。一部は `assert` で検出しますが、すべては検査しません。APIの契約を満たす入力を渡します。ライブラリ自身が検証失敗の例外を投げることはありませんが、メモリ確保など標準ライブラリ由来の例外まで排除するものではありません。

### 2.2 最短の入口：mc_choose

シグネチャは `mc_choose<Rng>(candidates, evaluate, time_us, use_crn=false, seed=1)` です。`Rng` を省略すると `MonteCarloNoise` になります。

| 引数・型 | 意味、取り得る値、既定値 |
|---|---|
| `Rng` | 一様乱数ビット生成器（URBG）に準拠し、`uint64_t` のseedから構築できる型。既定は `MonteCarloNoise`。 |
| `candidates` | `const` として要素を読み、要素数を取得し、添字相当でアクセスできる範囲。例は `vector`、`array`、`span`。要素が候補行動で、戻すためコピー可能にする。空も許される。通常の比較では候補数を131072以下にする。既定値なし。 |
| `evaluate` | 一回の試行の得点を返す関数。下記のいずれかの形式。有限の `double` に変換できる値を返し、大きい方を良いとする。既定値なし。 |
| `time_us` | 呼び出し全体へ与える相対時間。単位はマイクロ秒、0以上の `int64_t`。既定値なし。1000は1ミリ秒。 |
| `use_crn` | `true` なら比較する候補の乱数を共有する。`false` なら共有しない。既定は `false`。評価回数や配分規則を変更する設定ではない。 |
| `seed` | `uint64_t` の乱数seed。0を含む同型の全範囲。既定は1。 |

評価関数は `evaluate(const Action&, Rng&)`、または `evaluate(const Action&, Rng&, const MonteCarloNoise&)` を受け付けます。両方呼べる場合は後者を使います。最後の引数は、出来事キーを使いたい場合の `context` です。詳しくは2.9節と例21を参照してください。

戻り値は `std::optional<Action>` です。候補が空なら `nullopt`、一つだけなら評価せずその候補を返します。時間が0なら評価せず先頭を返します。正の時間でも、初回の評価までに時間が尽きれば先頭が暫定候補になります。この関数には暫定フラグや実行統計の戻り値がないため、それらが必要なら `Planner` を使います。

候補と評価関数は、この関数の実行中だけ借用します。一回の `evaluate` の途中では停止できません。評価関数が長ければ、時間超過もその分大きくなり得ます。

### 2.3 Plannerのテンプレート引数、型別名、構築

型は `MonteCarloPlanner<Model, Rng=MonteCarloNoise, Clock=std::chrono::steady_clock>` です。

| 項目 | 意味と条件 |
|---|---|
| `Model` | 問題の型。必須。2.8節の型・コールバックを、使う機能に応じて実装する。 |
| `Rng` | URBGに準拠し、`uint64_t` から構築できる型。既定は `MonteCarloNoise`。`std::mt19937_64` なども使用可能。CRN有効時はコールバックごとに構築するため、構築の軽い型が向く。 |
| `Clock` | `time_point`、`duration`、`static now()` を持ち、時刻の比較・差分・マイクロ秒の加算ができる時計型。単調に進むものを使う。既定は `std::chrono::steady_clock`。通常は指定不要。 |
| `Planner::State` | `Model::State` の別名。 |
| `Planner::Info` | `Model::Info` があればその別名。なければ `State` の別名。 |
| `Planner::Action` | `Model::Action` の別名。 |

`Planner(Model& model, Options options={})` は `model` を参照し、設定をコピーして構築します。`model` と、その内部で参照するデータは、計画器が使う間ずっと生存させます。後で手元の `options` を書き換えても、構築済み計画器の設定は変わりません。別設定を試すなら別の計画器を作ります。

一時的な `Model` を受け取る `Planner(Model&&, Options={})` は削除されています。先に名前のあるモデルを作って渡します。構築直後は根がないので、`search` の前に `start` が必要です。

### 2.4 Options：探索器を構築するときの設定

すべて `Planner::Options` の公開フィールドです。

| 名前・型 | 既定値 | 値の範囲と意味 |
|---|---:|---|
| `seed: uint64_t` | 1 | 0を含む全範囲。同じ設定、モデル、操作列、固定回数で再現するためのseed。`start` 等で根の世代が変わると系列も変わるため、同じ計画器を再開始しても初回の乱数列へ巻き戻るわけではない。 |
| `use_crn: bool` | `false` | `true` は乱数共有あり、`false` はなし。詳細は2.9節。試行数の補正や均等化は行わない。 |
| `tree: bool` | `false` | `false` は根の候補を比較し、その先はロールアウト。`true` は将来の判断点も保存して行動選択に使う。キーの等値比較が必要。 |
| `simulation_depth: int` | 0 | 0以上。0は先読み手数まで進める。正ならその手数で浅く打ち切る。終端や先読み手数へ先に着けばそちらを優先。実際に浅く打ち切る場合は `leaf_value` が必要。 |
| `discount: double` | 1 | 0以上1以下。目的関数の割引率。0では最初の一手報酬だけが効き、1では割引なし。 |
| `exploration: double` | 1 | 0以上。評価がまだ不確かな候補を調べる強さ。0は探索加点をなくすが、新候補の参加まで止めない。自動尺度が0の間は参加済み候補の試行数が少ない方を選ぶ。 |
| `value_scale: double` | 0 | 0以上。0は観測された得点のばらつきから尺度を推定。正なら、探索のための得点幅を固定する。得点と同じ単位で、値の上限や切り詰め幅ではない。 |
| `max_nodes: int` | 32768 | 1以上。保存する判断点の最大数。根も1点に数える。上限に達しても計画全体を終了せず、保存できない先はロールアウトする。 |
| `max_edges: int` | 131072 | 1以上。木全体で保存する行動の最大数。根だけの上限ではない。自動候補は保存可能な分までで、候補が欠ける場合がある。明示候補の登録・手動追加では上限超過を利用者が避ける。 |
| `widening: double` | 2 | 0以上。正なら参加上限の目安を `max(1, floor(widening * sqrt(その判断点の完了数 + 1)))` とする。1試行で新たに参加させる候補は高々一つ。0はこの平方根による制限を外し、可能なら毎試行一候補を参加させる。 |
| `clock_interval: int` | 8 | 1以上。時間制限を確認する `step` 回数の基本間隔。1なら頻繁に確認する。新試行の準備後などにも確認し、締切近くでは間隔を1へ縮める場合がある。 |

「登録」と「参加」は別です。`actions` や明示候補によって多数の行動を登録しても、全部をすぐ一回ずつ評価するわけではありません。候補は少しずつ参加し、既に参加した候補にも適応的に試行を配ります。候補の順番は短い予算での結果へ影響するため、有望なものを先に置く方法も使えます。

`tree=true` の場合、`tree_key` の戻り値、または省略時の `Info` 自体が `==` で比較できる必要があります。保存先が別の親行動であれば、同じキーでも自動的に木全体の一つのノードへ統合するものではありません。

### 2.5 Limits：一回のsearchへ与える停止条件

`Planner::Limits` の各フィールドは、今回の `search` 呼び出しだけに適用します。**少なくとも一つの制限を有効にします。** すべて既定値のまま渡すことは契約違反です。

| 名前・型 | 既定値 | 値の範囲と意味 |
|---|---|---|
| `time_us: int64_t` | -1 | -1は相対時間の制限なし。0以上なら今回の `search` 内の経過時間上限。単位はマイクロ秒。 |
| `deadline: Clock::time_point` | `Clock::time_point::max()` | この既定値は絶対締切なし。それ以外は指定時計の締切時刻。現在以前なら、時間の確認時に停止する。 |
| `max_simulations: int` | -1 | -1は完了試行数の制限なし。0以上なら今回の呼び出しで完了させる試行数の上限。途中から再開した試行の完了も1回に数える。 |
| `max_transitions: int` | -1 | -1は遷移回数の制限なし。0以上なら今回の `step` 呼び出し回数の上限。候補生成や `sample_root` の回数ではない。 |

複数設定したら、最初に確認された停止条件で止まります。同じ確認位置では、完了試行数、遷移回数、時間、根の終端判定の順です。このため、たとえば根が終端でも `max_simulations=0` なら停止理由は `simulations` になり得ます。

相対時間は `start` の前処理、`result`、出力などを含みません。それらを含む締切を意識するなら、前処理前に絶対締切を計算して渡します。それでも、利用者コールバックの途中で割り込んでは止められません。終了時刻を保証するリアルタイム機能ではないため、提出処理の時間を残した予算にします。

途中で停止した試行は捨てず、次の `search` で再開します。途中までの得点は候補の平均へ入れません。時間制限は、`sample_root` が終わって最初の `step` に入る前に停止する場合もあります。

### 2.6 根の設定、探索、結果取得、実ターンの更新

各メソッドの引数に、以下の記載以外の既定値はありません。

| メソッド | 引数と条件 | 戻り値と動作 |
|---|---|---|
| `void start(const Info& root, int horizon)` | `root` は現在の情報。完全観測なら `State`。`horizon >= 0`。 | 根をコピーし、統計・木・中断中試行をリセットする。候補は `search` 開始後にモデルから自動生成する。開始直後の結果は候補未生成で `not_ready` の場合がある。 |
| `void start(const Info& root, int horizon, const Range& candidates)` | `Range` は `const` として順に読める範囲。各要素を `Action` としてコピーでき、件数は `max_edges` 以下。空も可。`horizon >= 0`。 | 根へ指定候補をすぐ登録する。この根ではモデルから自動で候補を追加しない。未来の状態の行動は別途モデルで定義する。全統計はリセットされる。 |
| `ActionId add_root_action(const Action& action)` | `start` 後。木全体の行動数に空きが必要。渡す行動は現在の根で合法にする。 | 行動をコピーし、新しいIDを返す。既存統計は保持し、未参加候補の先頭へ挿入する。値が同じ行動でも重複除去はしない。 |
| `RunStats search(int64_t time_us)` | `start` 後、`time_us >= 0`。 | 相対時間だけを指定した追加探索。単位はマイクロ秒。今回の統計を返す。 |
| `RunStats search(const Limits& limits)` | `start` 後、少なくとも一つの制限を有効にする。 | 条件に従って追加探索し、今回の統計を返す。中断中の試行があれば、その続きから進める。 |
| `optional<Action> best_action() const` | 未開始や終端でも呼べる。 | 推薦行動のコピー。候補がない、未開始、手数0、根が終端なら `nullopt`。探索や候補生成はしない。 |
| `Result result(bool include_candidates=true) const` | `true` は候補統計を含める。`false` はその配列を作らない。未開始でも呼べる。 | 推薦、状態、暫定フラグ、直前の実行統計のスナップショット。探索や乱数の消費はしない。ただし根の `terminal` を呼ぶ場合がある。 |
| `bool advance(ActionId action, const Info& next, int horizon)` | `action` は同じ計画器・現在の根の有効なID。`next` は実行後に実際に得た次情報。`horizon >= 0`。 | 次の根へ移る。対応する部分木を再利用できれば `true`、できなければ統計をリセットして `false`。どちらでも次の根への更新自体は完了している。 |
| `void clear()` | 未開始でも可。 | 根・統計・中断試行をなくす。主要配列の確保容量を保持するが、内部の全動的領域が残る保証ではない。モデルは変更しない。 |

推薦は、完了した試行がある根候補の中から**標本平均が最も高いもの**を選びます。同点なら試行数が多いもの、それも同じなら登録時のID添字が小さいものを優先します。完了済み候補が一つもなければ、最初に登録した候補を暫定で返します。これを `Result::provisional` で区別できます。

`start`、`advance`、`clear` はIDの世代を変え、以前のIDを無効にします。また、これらは `last_run` を初期値へ戻し、中断中の試行を破棄します。`clear` は乱数を初期seedの最初へ巻き戻す操作ではありません。

#### advanceで再利用できる条件

再利用には、完全観測、`tree=true`、整合する状態キー、残り手数が一つ減ること、実際の浅い打ち切りがないことが必要です。実際の次状態に対応する保存済みの子があり、その子に候補があり、候補が保存上限で欠けていない場合に再利用します。

部分観測では、`advance` は次の `Info` を根にして統計をリセットします。完全観測でも、条件を満たさなければ同じです。`false` が返った後にさらに `start` を呼ぶ必要はありません。

`advance` 自体は現実の環境を一手進めません。利用者が行動を実行し、次状態を受け取って渡します。費用、遷移確率、合法手、採点規則などが変わった場合は、状態キーが同じでも統計の意味が変わるため、`start` でやり直します。

### 2.7 結果型、列挙型、デバッグ出力

#### Status：現在の根から推薦を取り出せるか

| `Planner::Status` の値 | 意味 |
|---|---|
| `ready` | 行動が一つ返る。十分に評価したことは意味せず、暫定なら `provisional=true`。 |
| `not_ready` | 根がない、まだ候補を生成していない、候補が空などで、行動を返せない。 |
| `terminal` | 手数0、または現在の根が `terminal`。返す行動はない。 |

#### Stop：直前のsearchが止まった理由

| `Planner::Stop` の値 | 意味 |
|---|---|
| `none` | まだ `search` の実行結果がない初期状態、またはリセット後。通常、実行済み `search` の停止理由にはならない。 |
| `time` | 相対時間または絶対締切へ達したことを時計確認で検出した。 |
| `simulations` | 今回の完了試行数が上限へ達した。 |
| `transitions` | 今回の `step` 回数が上限へ達した。 |
| `not_ready` | 開始済みの根で比較できる候補を用意できなかった。`search` を `start` より先に呼んでよい、という意味ではない。 |
| `terminal` | 根が既に終端、または根の先読み手数が0。個々の試行が終端へ着くたびに探索全体が止まる、という意味ではない。 |

`Status` は「今、行動を返せるか」、`Stop` は「直前の計算をなぜ止めたか」です。たとえば時間切れでも、推薦があれば `status=ready`、`stop=time` になります。

#### MonteCarloFinishReason：一試行を最後まで評価した理由

これは `Planner` の内側ではなく、ヘッダのトップレベルにある列挙型です。

| 値 | 意味 |
|---|---|
| `MonteCarloFinishReason::terminal` | 一手進んだ後に `terminal` が真になった。先読み手数と同時に達した場合もこちらを使う。 |
| `MonteCarloFinishReason::horizon` | 真の終端には着かず、指定された先読み手数を使い切った。 |

計算時間切れや `max_transitions` による中断は、この列挙型にはありません。その時点で `finish_value` を呼んで精算するのではなく、試行を中断して保持します。浅い打ち切りも別で、`leaf_value` を呼びます。

#### ActionId

`Planner::ActionId` は、現在の根に登録された行動の識別子です。

| フィールド | 型 | 初期値 | 意味 |
|---|---|---:|---|
| `generation` | `uint64_t` | 0 | 根の世代。同じ計画器で有効性を区別するための値。 |
| `index` | `int` | -1 | 内部の行動添字。行動の値そのものでも、`result.candidates` の添字でもない。 |

既定構築のIDは無効です。`add_root_action`、`Result::id`、`Candidate::id` から得た有効なものを使い、利用者側でIDの数値を作って渡さないでください。他の計画器のIDとも混ぜません。

`operator==(const ActionId&, const ActionId&)` は、両フィールドが同じとき `true` を返します。C++20の `!=` による比較も可能です。引数に既定値はありません。

#### RunStats

`Planner::RunStats` は、**一回のsearchだけ**の実行統計です。

| フィールド | 型 | 初期値 | 意味 |
|---|---|---:|---|
| `started` | `int` | 0 | 今回、新しく開始した試行の数。中断試行の再開は増加させない。 |
| `simulations` | `int` | 0 | 今回、完了して平均へ反映した試行の数。前回開始した試行の完了も含む。 |
| `transitions` | `int` | 0 | 今回、呼び出した `step` の回数。 |
| `elapsed_us` | `int64_t` | 0 | 今回の `search` 内の経過マイクロ秒。通常の単調時計なら非負。 |
| `stop` | `Stop` | `none` | 上表の停止理由。 |

たとえば一試行に10手必要で、前回3手だけ進めたなら、今回は残り7手を進めるだけで `started=0, simulations=1, transitions=7` になり得ます。

#### Candidate

`Planner::Candidate` は、根候補一つの**蓄積統計**です。

| フィールド | 型 | 初期値 | 意味 |
|---|---|---|---|
| `id` | `ActionId` | 無効ID | この候補の識別子。 |
| `trials` | `int` | 0 | この候補から完了した試行の蓄積数。0以上。 |
| `mean` | `optional<double>` | `nullopt` | 完了した試行の標本平均。`trials=0` なら値なし。0点の平均とは別。 |

行動値自体はこの構造体に入りません。外側でIDと候補を対応付けたいときは、`add_root_action` の戻り値を保存できます。候補配列の位置からID添字や行動値を推測しないでください。

#### Result

`Planner::Result` は、取得時点の結果をコピーしたものです。その後の探索を自動的に反映する参照ではありません。

| フィールド | 型 | 初期値 | 意味 |
|---|---|---|---|
| `action` | `optional<Action>` | `nullopt` | 推薦行動のコピー。使用前に値の有無を確認する。 |
| `id` | `ActionId` | 無効ID | 推薦行動のID。`action` があるときだけ有効。 |
| `status` | `Status` | `not_ready` | 行動を取り出せるかどうか。 |
| `provisional` | `bool` | `false` | 返した行動に完了試行が一つもなければ `true`。最適性や統計的な確信度を表すものではない。 |
| `candidates` | `vector<Candidate>` | 空 | 登録済み根候補の統計。未参加候補も含む。`result(false)`、未開始、終端では空。内部の候補順で、手動追加によって順番は変わり得る。 |
| `last_run` | `RunStats` | 上表の初期値 | 最も最近の `search` の統計。`start`、`advance`、`clear` で初期値へ戻る。 |

#### ostreamへの出力

以下の四つのオーバーロードがあります。すべて第1引数へ出力し、同じ `std::ostream&` を返すので、続けて `<< '\n'` などを書けます。引数に既定値はありません。

| シグネチャ | 表示内容 |
|---|---|
| `std::ostream& operator<<(std::ostream&, const Planner::ActionId&)` | `generation:index`。 |
| `std::ostream& operator<<(std::ostream&, const Planner::Candidate&)` | ID、試行数、平均。未評価の平均は `unevaluated`。 |
| `std::ostream& operator<<(std::ostream&, const Planner::RunStats&)` | 開始数、完了数、遷移数、経過時間、停止理由名。 |
| `std::ostream& operator<<(std::ostream&, const Planner::Result&)` | 状態名、行動、ID、暫定フラグ、候補統計、直前の実行統計。 |

出力演算子は改行や明示的なflushをしません。浮動小数の桁数などは、利用者がストリームに設定した書式を使います。`Status`、`Stop`、`MonteCarloFinishReason` 単体の専用出力演算子はありません。

`Result` の候補表示は先頭20件までで、残り件数を省略表示します。これは表示上の上限であり、`result.candidates` の要素を削除しません。

行動自体に `operator<<` があればそれを優先します。なければ、`const` のまま繰り返し読める範囲を、入れ子全体で最大20要素まで表示します。それもできない型は `<unprintable>` と表示します。独自の出力演算子や文字列の出力長はライブラリ側では制限しないため、独自型の長い表示は利用者側で抑えます。

### 2.8 Modelの定義とコールバック

#### 必要な型

| 利用者が定義する型 | 必須性と意味 |
|---|---|
| `Model::State` | 必須。一試行の世界の状態。 |
| `Model::Action` | 必須。一行動の値。整数、独自構造体、配列などを使える。 |
| `Model::Info` | 部分観測だけで定義する。宣言の存在で部分観測版へ切り替わる。完全観測版では宣言しない。 |

以下の `remaining` は、**次の行動より前の残り手数**です。途中打ち切り後の `leaf_value` では、**既に進めた分を引いた残り手数**です。`simulation_depth` までの距離ではありません。

`state` はその試行の `State`、`info` はその試行で判断に使える `Info`、`action` は選択済みの行動です。`rng` は指定したRNG型の参照、`context` は出来事キー用の読み取り専用情報です。コールバックの引数に既定値はなく、以下の形式から必要なものを定義します。

#### step：一手を実行する

| 対応 | 受け付ける形式 |
|---|---|
| 完全観測 | `step(State& state, const Action& action, Rng& rng)` |
| 完全観測・context付き | `step(State& state, const Action& action, Rng& rng, const MonteCarloNoise& context)` |
| 部分観測 | `step(State& state, Info& info, const Action& action, Rng& rng)` |
| 部分観測・context付き | `step(State& state, Info& info, const Action& action, Rng& rng, const MonteCarloNoise& context)` |

戻り値は有限の数値、または `void` です。数値なら一手報酬として `double` へ変換し、`void` なら一手報酬0として扱います。`State` を次へ進め、部分観測では、実際に知ることになる観測を `Info` へ反映します。真実をすべて `Info` にコピーする必要はありません。

`step` は必須です。context付きとなしの両方を呼べる場合は、context付きを優先します。例のように、小さい `Action` を値で受け取っても構いません。

#### sample_root：見えない初期状態を一つ仮定する

部分観測では、`State sample_root(const Info& info, Rng& rng)`、または `State sample_root(const Info& info, Rng& rng, const MonteCarloNoise& context)` が必須です。両方ある場合はcontext付きが優先です。完全観測では使いません。

現在の情報と整合する `State` を返します。新しい試行につき一回呼び、試行再開時には呼び直しません。確率分布や粒子集合をモデルに持たせる場合、それらの作成・観測による更新は利用者の担当です。

#### 候補列挙、候補提案、ロールアウト

| メソッド | 引数・戻り値と意味 |
|---|---|
| `actions(const Info& info)` | 現在の合法手の範囲を返す。値で返しても、存続するコンテナへの `const` 参照でもよい。既定ロールアウトでも使う場合、`const` として反復でき、空かどうかと要素数を取得できる範囲にする。 |
| `void actions(const Info& info, int remaining, vector<Action>& out)` | `remaining >= 1`。空にされた出力バッファへ合法手を追加する。バッファ容量は再利用される。参照を呼び出し後まで保存しない。上の簡易形もあれば、この出力バッファ形式を優先する。 |
| `optional<Action> propose_action(const Info& info, int remaining, int index, Rng& rng)` | 候補を一つずつ提案する。`remaining >= 1`、保存する判断点の提案番号 `index` は0始まり。`nullopt` はその判断点で提案が尽きたことを表す。再提案を待つ一時的な空振りには使わない。 |
| `optional<Action> propose_action(const Info& info, int remaining, int index, Rng& rng, const MonteCarloNoise& context)` | context付き提案。同名の上の形式より優先。意味と戻り値は同じ。 |
| `Action rollout_action(const Info& info, int remaining, Rng& rng)` | `remaining >= 1`。以降の一行動を、利用者の継続ルールで返す。合法な行動を必ず一つ返す。 |
| `Action rollout_action(const Info& info, int remaining, Rng& rng, const MonteCarloNoise& context)` | context付き継続ルール。同名の上の形式より優先。 |

これらをすべて実装する必要はありません。使う場所に応じた優先順位は次のとおりです。

| 使用場所 | 優先順位 |
|---|---|
| 明示候補付き `start` の根 | 渡した候補だけを登録する。追加したい場合は `add_root_action`。 |
| 自動生成する根や保存した判断点 | `propose_action` → 出力バッファ形式 `actions` → 簡易形式 `actions`。 |
| ロールアウトでの一行動 | `rollout_action` → 出力バッファ形式から一様選択 → 簡易形式から一様選択 → `propose_action` を `index=0` で呼ぶ。 |

提案型をロールアウトの代用にした場合、保存ノードの連続した提案番号とは異なり、各呼び出しで `index=0` になります。そこで `nullopt` を返してはいけません。根候補だけを明示して複数手進める場合も、後続の合法手を選ぶ手段が必要です。

根の候補が空なら推薦できず `not_ready` になります。根より先の非終端状態で、ロールアウトの候補が空になる使い方はできません。行動できない状態は `terminal` とするか、問題として合法なら待機行動を用意します。

#### terminal：本当に終了したか

`bool terminal(const Info& info)` は省略でき、省略時は常に `false` です。`true` は、その情報の状態でもう行動しないことを示します。完全観測では `Info` が `State` の別名なので、例のように `const State&` を受け取ります。

同じ情報へ何回呼んでも同じ結果になるようにします。ここで乱数を引いたり、呼び出し回数により終了を変えたりしません。`search` だけでなく `result` や `best_action` も根の終端を確認するためです。

部分観測では `State` を直接受け取りません。終了が観測できる問題なら、その情報を `step` で `Info` に反映します。見えないまま終了扱いだけを変える定義は、情報の漏れになります。

#### finish_value：終端または先読み末尾で加える得点

| 形式 | 使えるモデルと意味 |
|---|---|
| `double finish_value(const State& state)` | 完全観測・部分観測。終了時の状態だけで採点する。 |
| `double finish_value(const State& state, MonteCarloFinishReason reason)` | 完全観測・部分観測。`reason` によって終端と手数消尽を分けられる。 |
| `double finish_value(const State& state, const Info& info)` | 部分観測。仮定した真実と、観測・決定済み内容を併せて採点する。 |
| `double finish_value(const State& state, const Info& info, MonteCarloFinishReason reason)` | 部分観測。状態・情報・終了理由を使って採点する。 |

複数定義した場合、部分観測の3引数、部分観測の2引数、`State + reason`、`State` の順に優先します。省略時の追加得点は0です。表では `double` と書きましたが、有限の `double` へ変換できる数値を返せます。

戻す値は終了時点の価値です。ライブラリが経過手数分の割引を掛けるので、外側の割引を二重に掛けません。また、既に `step` で返した得点を重ねて足しません。

#### leaf_value：浅く打ち切った先の価値

| 形式 | 使えるモデルと意味 |
|---|---|
| `double leaf_value(const Info& info, int remaining)` | 完全観測・部分観測。打ち切り時に分かる情報から残り価値を推定する。完全観測では `Info=State`。 |
| `double leaf_value(const State& state, const Info& info, int remaining)` | 部分観測。打ち切り時の真実の仮定も使って残り価値を採点する。こちらを優先する。 |

`remaining >= 1` で呼びます。現在までに得た得点ではなく、**この時点から先の一手報酬と最後の得点を含む残り価値**を返します。割引率1未満で将来を計算するなら、この時点を起点としてその中の割引を考えます。打ち切り地点までの割引はライブラリが掛けます。

実際に浅い打ち切りが起きる場合は必須で、省略時の既定近似はありません。終端・先読み手数へ先に達したときは `finish_value` を使い、`leaf_value` と両方を加えることはありません。

部分観測で `State` を採点に使えることと、`State` を見て本来見えない条件で行動を選び直すことは別です。例18のように、既に決まった行動の結果を真実の仮定に照らして採点できます。実際には知らない当たり位置を見て最適な箱を選ぶような評価は、実行可能な方策の価値を表しません。

#### tree_key：将来の判断点を識別する

`tree_key(const Info& info)` は省略可能です。戻り値はコピーでき、`==` で比較できるキーにします。参照を返しても、ライブラリはキー値をコピーして保持します。省略時は `Info` 自体をキーにするため、木を使うなら `Info` 自体の等値比較を用意します。

同じキーとみなす状態では、今後の合法手、遷移、採点が一致するよう、必要な情報をキーへ入れます。残り在庫や観測済み価格などを省略してはいけません。木の深さごとに残り手数は決まっていますが、カレンダー上の日付などが別途未来に影響するならキーへ含めます。安易にアドレスだけをキーにすることも避けます。

部分観測ではキーにも `Info` だけを使います。隠れた `State` ごとに方策を分けないためです。キーが大きいと保存と比較の費用が増えるので、固定グラフなどを除いた必要な状態だけを表すと扱いやすくなります。

#### StateとInfoのライフサイクル、呼び出しの順番

完全観測では、`start` が根の `State` をコピーします。新しい試行はこの根をコピーして始め、`step` がその試行だけの `State` を更新します。実際のゲーム状態が自動的に進むことはありません。

部分観測では、`start` が根の `Info` をコピーします。新しい試行の開始時に、根の候補を選んでから `sample_root` が `State` を作り、根の `Info` も試行用へコピーします。以後の `step` はこの組を更新します。仮の観測は実際の根へ書き戻しません。粒子などをモデルから借用しても、試行用の可変状態は独立させます。

通常の一試行の流れは次です。

1. 現在の予算と根の終端を確認します。根が既に終端、または手数0なら、`sample_root`、`step`、`finish_value` を呼ばずに探索を止めます。
2. 根の候補を選びます。必要に応じて `actions` や `propose_action` を呼びます。
3. 完全観測では根の状態をコピーします。部分観測では `sample_root` と根の情報のコピーで、試行を作ります。
4. 選択済み行動で `step` を一回呼び、一手報酬と次の状態を得ます。
5. `terminal` が真なら `finish_value(..., terminal)`、そうでなく手数を使い切ったら `finish_value(..., horizon)`、さらにそうでなく浅い深さへ達したら `leaf_value` で完了します。
6. 続ける場合、保存木の対応確認に必要なら `tree_key` を使います。既存の判断点で行動を選ぶか、`rollout_action` などで次の行動を選び、4へ戻ります。新しい子を保存した最初の試行では、その先はロールアウトします。
7. 完了した得点を統計へ反映し、試行用の状態を破棄します。計算予算が残っていれば次の試行へ進みます。

予算による中断では、試行の `State`、`Info`、選択済み行動、乱数の進行などを保持します。次の `search` はその続きであり、`sample_root` で別の真実を引き直しません。`start`、`advance`、`clear` を呼んだときは中断状態も破棄します。

保存木が主に持つのは、行動・統計・状態キーです。各ノードの遷移済み `State` をそのまま実行用として使い回す機能ではありません。既定キーが大きな `State` や `Info` なら、そのキーのコピー費用は発生します。

### 2.9 RNG、簡易CRN、出来事キー

#### MonteCarloNoiseの全API

`MonteCarloNoise` は、そのままURBGとしても使える既定乱数器です。以下の `key`、`channel`、`draw_index` はいずれも `uint64_t` の非負整数で、型の全範囲を受け付けます。値の意味は利用者が決めます。

| API | 引数・既定値 | 戻り値・動作 |
|---|---|---|
| `using result_type = uint64_t` | 引数なし。 | URBGとして生成する整数の型。 |
| `explicit MonteCarloNoise(uint64_t seed=1)` | `seed` は0を含む `uint64_t` の全範囲。既定は1。 | 指定seedから通常の乱数系列を開始する。 |
| `static constexpr result_type min()` | 引数なし。 | 0。通常の整数乱数が取り得る最小値。 |
| `static constexpr result_type max()` | 引数なし。 | `UINT64_MAX`。最大値も生成範囲に含む。 |
| `result_type operator()()` | 引数なし。 | 次の64bit整数乱数を返し、通常系列を一つ進める。 |
| `uint64_t scenario_seed() const` | 引数なし。 | このオブジェクトを構築したseed。現在の乱数位置ではない。 |
| `double uniform01()` | 引数なし。 | 0以上1未満の一様な実数を返し、通常系列を一つ進める。 |
| `uint64_t uniform_int(uint64_t bound)` | `bound >= 1`。既定値なし。 | 0以上 `bound` 未満の整数。`bound=1` は常に0だが、乱数を一つ消費する。その他は値域の偏りを避けるため複数個消費する場合がある。 |
| `template<URBG Rng=MonteCarloNoise> Rng stream(uint64_t key, uint64_t channel=0) const` | `key` に既定値なし。`channel` は既定0。`Rng` は混合したseedから構築できるURBG。 | seed・key・channelから新しいRNGを値で返す。親の通常系列を進めない。 |
| `template<class Factory> auto stream(uint64_t key, uint64_t channel, Factory&& factory) const` | すべて必須。`factory` は `uint64_t` の混合seedを受け取って、目的の乱数器を返す関数。 | `factory` の戻り値をそのまま返す。独自の初期化を使える。親の通常系列を進めない。 |
| `double uniform01(uint64_t key, uint64_t channel, uint64_t draw_index) const` | すべて必須。`draw_index` は0始まり。 | 指定した出来事系列の、指定位置の0以上1未満の値を返す。通常系列を進めない。既定型の `stream(key, channel)` から `uniform01()` を繰り返して得る位置に対応する。 |

表の `URBG` は説明のための略記で、実際の制約名は `std::uniform_random_bit_generator` です。任意のRNGに `uniform01` や `uniform_int` があるわけではありません。別RNGを使うコールバックでは、その型のAPIか `std::uniform_int_distribution` などを使います。

通常の乱数器としてコピーすれば、その時点の系列位置もコピーします。一方、`stream` とキー付き `uniform01` は、親の現在位置ではなく構築seedを使います。同じseed・key・channelなら、親で何個の乱数を引いた後でも同じ出来事系列を取り出せます。

同じ型の `stream` を同じキーで作り直すと、毎回その系列の先頭へ戻ります。「次の乱数」を欲しいなら作ったRNGを保持して続けて使うか、`draw_index` を変えます。`uniform_int` や別の分布の呼び出し回数は、キー付き `uniform01` の `draw_index` と同じ数え方とは限りません。

#### CRNの目的とON/OFFの違い

CRNは Common Random Numbers の略で、比較する案に**同じ外乱を与え、運の違いが比較へ混ざるのを減らしたい**ときの仕組みです。たとえば二つの工場を比べるとき、一方だけ好天の日ばかりを引くより、同じ天候の組で費用を比べたい、という用途です。

`use_crn=false` が既定です。一つのRNGを根の世代内で使い続け、各手や用途ごとには作り直しません。途中停止から再開しても、その乱数位置を保持します。

`use_crn=true` では、根候補ごとの第0試行、第1試行……を対応させます。対応する試行で、残り手数と用途が同じコールバックへ同じ系列を渡します。部分観測の初期状態の生成、`step`、ロールアウトの行動選択が対象です。既定ロールアウトの一様選択にも適用します。

保存する候補の `propose_action` は、根の世代、残り手数、提案番号を使って対応付けます。その判断点へ最初に着いた試行の番号や、それまでに消費した乱数の数には依存しない系列を使います。状態や提案番号まで同じなら、別の子でも乱数だけを理由に候補が変わらないようにするためです。

この切り替えは**乱数共有だけ**を変更します。候補を均等な回数まで評価し直したり、追い付き用の試行を追加したりしません。評価値が変われば、同じ配分規則でも有望とみなす候補が変わり、候補別の試行数は変わり得ます。時間予算の場合は計算費用の違いで総試行数も変わり得ます。回数予算そのものがON/OFFで書き換わることはありません。

同じ乱数でも入力状態や合法手が違えば、行動や得点まで同じになるとは限りません。また、外側の共有RNGや、内部状態を持つ分布オブジェクトを別途使うと、共有の対応を崩す場合があります。共有したい乱数は、渡された `rng` または `context` から得ます。比較のばらつきがどの程度減るかは問題によるため、CRNを使わない構成も選べます。

#### なぜ出来事キーが役立つのか

同じ乱数列で始めても、一つのコールバック内に条件分岐があると、使う位置がずれる場合があります。

たとえば一か月分を一度の `evaluate` で計算し、「今日の天候を引く。故障したら修理日数も引く」を繰り返すとします。一方の案だけ故障すると、その案だけ修理用の乱数を余分に引きます。次の日には、もう同じ位置の乱数を天候として使っていません。

この場合にだけ、天候を `key=日付, channel=天候`、修理を `key=日付, channel=修理` のように指定すると、途中の乱数消費数によらず同じ出来事を比較できます。これがキー付きAPIの用途です。通常の `rng` だけで十分なら、導入する必要はありません。例20が通常の共有、例21が出来事キーを必要とする形です。

コールバックへ渡る `const MonteCarloNoise& context` は、指定RNG型とは別の補助オブジェクトです。`sample_root`、`step`、`rollout_action` のcontextは、一試行の中で同じシナリオseedを持ちます。**context自身に現在の日付や用途が自動で埋め込まれるわけではない**ため、同じキーを複数手で使えば同じ値になります。日付、手数、出来事の種類などを、必要に応じてキーとchannelに含めます。候補間で同じ天候を共有したいなら、キーへ候補番号は含めません。

CRNをOFFにしてもcontext APIを使えますが、候補間で対応する第k試行を共有しません。キーは、そのcontext内で出来事を再現するために使います。`rng` と `context` の参照はコールバック中だけ使い、呼び出し後へ保存しません。

### 2.10 APIから例を探す

| 使いたい機能 | 例 |
|---|---|
| `mc_choose`、一回評価、最小化の符号変換 | 01、21 |
| 型別名、構築、`Options.seed`、`start` の自動候補、回数制限 | 02 |
| バッファへ候補を列挙、`void step`、`terminal`、状態だけの終了採点 | 03、06 |
| `discount`、簡易範囲の候補、独自ロールアウト | 04 |
| 終了理由の列挙型、理由付き終了採点 | 05 |
| `simulation_depth`、情報だけの `leaf_value` | 07、16 |
| 明示候補付き `start`、配列の行動、`result(false)` | 08 |
| `tree`、`max_nodes`、`max_edges`、`tree_key` | 09 |
| `advance`、既定の状態キー、`ActionId` | 10 |
| モデル更新、候補範囲の参照返却、`clear` | 11 |
| `max_transitions`、途中再開、開始数と完了数、暫定推薦 | 12 |
| `add_root_action`、ID比較、IDと候補の出力 | 13 |
| `propose_action`、`widening`、`exploration`、`value_scale` | 14 |
| `search(time_us)`、`time_us` と `deadline`、`clock_interval`、実行統計の出力 | 15 |
| 部分観測の `Info`、`sample_root`、`step` | 16 |
| 外部粒子の借用、`State + Info` の終了採点、再計画 | 17 |
| `State + Info` の末端採点、終了理由付き部分観測採点 | 18 |
| 指定RNG、context付きの各コールバック | 19 |
| 通常のCRN ON/OFF、ロールアウトの乱数共有 | 20 |
| キー付き実数、型指定ストリーム、条件分岐のある評価 | 21 |
| RNGの構築・整数生成・範囲・seed・生成関数付きストリーム | 22 |
| `Status`、`Stop`、`Result`、`Candidate`、全結果型の出力、空・終端・未評価 | 23 |
| 第3テンプレート引数 `Clock`、時間停止のテスト | 24 |

列挙型の全値、設定・結果型の全フィールド、公開メソッドの全オーバーロード、およびModelが受け付ける形式は、第2章の表に列挙しています。例では、無理に一つのプログラムへ全部を入れず、用途ごとに必要なものを使っています。

## 3. ユースケース

### 実行方法と、各例の読み方

各例のC++コードは、`main` と標準入力・標準出力を含む独立したプログラムです。複数の例を一つのソースへ連結せず、使いたい一つを `main.cpp` にコピーしてください。同じディレクトリへ `monte_carlo_planner_v08.hpp` を置きます。例19と21では、同梱の `fast_rng.hpp` も置きます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o sample
./sample < input.txt
```

各例の「入力例」を `input.txt` に保存すれば実行できます。C++23でも同じ使い方です。ZIP内のファイルを使う場合は、ZIPを展開したディレクトリで次のように実行できます。

```sh
g++ -std=c++20 -O2 -I. examples/01_route_choice.cpp -o sample
./sample < inputs/01_route_choice_01.txt
```

入力は記載した条件を満たすものとし、すべてを細かく検証するコードにはしていません。`seed` を入力する例では、0以上の64bit整数を渡します。件数や手数は `int` に収まり、加算・乗算などの中間結果も各型の範囲に収まる値を使います。得点や時間が2.1節の範囲に収まる、通常のAHCの規模を想定します。

`std::optional<T>` は「Tの値がある、またはない」を表す型です。`if (action)` で値の有無を確認し、ある場合に `*action` で値を読みます。候補と正の予算を必ず用意する例では、その前提をコード中へコメントし、直接読む場合もあります。`std::vector<T>` は要素数を変えられる配列、`std::array<T, N>` はN要素の固定長配列です。

出力の `0/1` は、説明に応じて行動番号または偽/真を表します。入力・出力例は、すべて同梱コードの実行で確認したものです。固定回数の例でも、コンパイラや乱数分布の実装が変わると、同一の数値列まで保証するものではありません。時間予算の例では、そのときの実行環境によって推薦や試行数も変わり得ます。

サンプルは結果を見やすくするため、診断情報も標準出力へ出します。実際のAHCに組み込むときは、問題の指定形式に合わせた行動だけを標準出力へ送り、診断情報を `std::cerr` へ送る形にできます。各例のコード内にある日本語コメントで、使用箇所のAPIと注意点を確認できます。

以下の例は、問題を解くためのモデルの書き方を示すものです。短いコードで伝えるため、継続方策や末端評価は簡素にしています。その方策が対象問題にとって最良だと保証するものではありません。

### 例01：一回の評価関数で、配送業者を選ぶ

配送業者を一社選びます。業者ごとに基本料金と、渋滞による追加料金の最大幅があります。追加料金は0以上、最大幅未満から一様に発生すると仮定します。期待費用が小さい業者を選ぶ問題です。
この例の費用計算は小さい式ですが、同じ位置を配送シミュレータに置き換えられます。ライブラリは得点の大きい候補を選ぶため、評価関数は費用にマイナスを付けて返します。

**入力形式**

1行目は `N time_us seed`。続くN行は `基本料金 追加料金の最大幅`。`1 <= N <= 100`、`time_us >= 0`、料金・幅は非負の有限値です。

**入力例**

```text
3 3000 42
10 2
7 2
12 1
```

**実行できるコード：`examples/01_route_choice.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

int main() {
    int n;
    int64_t time_us;
    uint64_t seed;

    if (!(std::cin >> n >> time_us >> seed)) {
        return 0;
    }

    assert(1 <= n && n <= 100 && time_us >= 0);
    std::vector<double> base(n), width(n);
    for (int i = 0; i < n; ++i) {
        std::cin >> base[i] >> width[i];
    }

    std::vector<int> candidates(n);
    std::iota(candidates.begin(), candidates.end(), 0);

    // 候補と、その評価用RNGを受け取る。
    auto evaluate = [&](int company, MonteCarloNoise& rng) {
        // uniform01()は0以上1未満。
        double cost = base[company] + width[company] * rng.uniform01();
        // mc_chooseは最大化するので、費用最小化には符号を反転する。
        return -cost;
    };

    // falseはCRNなし。時間はマイクロ秒。
    auto chosen = mc_choose(candidates, evaluate, time_us, false, seed);
    // time_us=0または候補が一つなら評価せず先頭を返す。正の予算でも最適性の保証はない。
    // 戻り値optionalの有無を確かめてから読む。
    if (chosen) {
        std::cout << "company=" << *chosen << '\n';
    } else {
        // 候補が空ならnulloptになる。
        std::cout << "company=none\n";
    }
}
```

**出力例と読み方**

実行時の出力例です。時間に関係する値は実行環境で変わります。

```text
company=1
```

`company` は選んだ配送業者の0始まりの番号です。入力例では番号1の業者を推薦しています。

会社番号は0始まりです。出力は推薦番号であり、期待費用そのものではありません。

### 例02：同じ試行予算で、広告案の期待利益を比較する

広告案を一つ選びます。各案には成約確率、成約時の売上、広告費があります。一回の試行で成約の有無を発生させ、「売上−広告費」を採点します。実行時間ではなく完了試行数を固定したい比較や、乱数seedをそろえた検証に向く形です。
試行予算は候補ごとではなく全候補の合計です。候補別の回数と平均は結果から確認します。

**入力形式**

`N trials seed`、続くN行に `成約確率 成約時売上 広告費`。`1 <= N <= 100`、`trials >= 1`、確率は0以上1以下です。

**入力例**

```text
3 3000 42
0.1 10 1
0.8 10 1
0.5 8 2
```

**実行できるコード：`examples/02_fixed_trials.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// Modelに問題固有の候補と一回の評価を定義する。
struct Model {
    // 一手だけなので、試行中の状態は空でよい。
    struct State {};

    // 行動値は広告案の番号。
    using Action = int;

    struct Ad {
        double probability, revenue, cost;
    };

    std::vector<Ad> ads;

    // 現在選べる全候補を返す。
    std::vector<int> actions(const State&) const {
        std::vector<int> out(ads.size());
        std::iota(out.begin(), out.end(), 0);
        return out;
    }

    // 一試行の利益を返す。
    double step(State&, int action, MonteCarloNoise& rng) const {
        const auto& ad = ads[action];
        // 渡されたRNGで成約を発生させる。
        bool sold = rng.uniform01() < ad.probability;
        return (sold ? ad.revenue : 0) - ad.cost;
    }
};

int main() {
    int n, trials;
    uint64_t seed;

    if (!(std::cin >> n >> trials >> seed)) {
        return 0;
    }

    assert(1 <= n && n <= 100 && trials >= 1);

    // plannerが参照するため、plannerより先に作って長く保持する。
    Model model;
    model.ads.resize(n);
    for (auto& ad : model.ads) {
        std::cin >> ad.probability >> ad.revenue >> ad.cost;
    }

    // RNGと時計は既定型を使う。
    using Planner = MonteCarloPlanner<Model>;

    // 省略した項目はすべて既定値。
    Planner::Options options;

    // 同じ操作列・固定回数を再現するためのseed。
    options.seed = seed;

    // 設定はここでコピーされる。
    Planner planner(model, options);

    // 一手で評価を終える問題として開始する。
    planner.start(Model::State{}, 1);

    // この呼び出しの予算を作る。
    Planner::Limits limits;

    // 全候補を合計した完了試行数の上限。
    limits.max_simulations = trials;

    // 追加探索し、今回の実行統計を受け取る。
    const auto run = planner.search(limits);

    // 推薦と候補別の蓄積統計をコピーして取得する。
    const auto result = planner.result();

    // ここでは候補があり正の回数を実行済み。
    std::cout << "action=" << *result.action << " completed=" << run.simulations << '\n';
    // Candidateは直接出力できる。
    for (const auto& candidate : result.candidates) {
        std::cout << candidate << '\n';
    }
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
action=1 completed=3000
Candidate{id=1:0, trials=10, mean=-9.71445e-17}
Candidate{id=1:1, trials=2977, mean=7.0954}
Candidate{id=1:2, trials=13, mean=1.07692}
```

`action` は広告案番号、`completed` は全候補を合計した完了試行数です。各 `Candidate` の `trials` はその案の回数、`mean` はその案の平均利益です。候補0の `-9.71445e-17` は約マイナス0.000000000000000097という丸め誤差の規模で、数学上の0が厳密な0表示にならない例です。IDの `1:0` は「根の世代1・内部添字0」であり、一般に行動値と同じとは限りません。

### 例03：積み込みを続け、最後の価値だけで採点する

容量のある箱へ、品物を一つずつ積みます。同じ品物は二度選べず、残容量を超える品物も選べません。最後に入った品物の価値の合計を最大化します。
一手ごとの報酬は返さず、状態を変更するだけの`void step`と、終了時の`finish_value`を組み合わせます。返るのは最初に積む品物です。

**入力形式**

`N 容量 trials`、続くN行に `重さ 価値`。`1 <= N <= 20`、容量・重さは正、価値は非負、`trials >= 1`です。

**入力例**

```text
4 8 3000
5 9
4 7
3 6
2 3
```

**実行できるコード：`examples/03_packing_terminal.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 終端得点だけを使う完全観測モデル。
struct Model {
    // 変更する量だけを試行ごとにコピーする。
    struct State {
        int room;
        uint32_t used = 0;
        double value = 0;
    };

    // 積む品物の番号。
    using Action = int;

    std::vector<int> weight;
    std::vector<double> value;

    // outは空にして渡され、容量は再利用される。
    void actions(const State& s, int, std::vector<Action>& out) const {
        for (int i = 0; i < int(weight.size()); ++i) {
            if (!(s.used & (uint32_t(1) << i)) && weight[i] <= s.room) {
                out.push_back(i);
            }
        }
    }

    // 合法な品物がない状態を終端にし、空の継続候補を避ける。
    bool terminal(const State& s) const {
        for (int i = 0; i < int(weight.size()); ++i) {
            if (!(s.used & (uint32_t(1) << i)) && weight[i] <= s.room) {
                return false;
            }
        }

        return true;
    }

    // voidなら一手報酬は0。RNGは使わなくても受け取る。
    void step(State& s, int i, MonteCarloNoise&) const {
        s.room -= weight[i];
        s.used |= uint32_t(1) << i;
        s.value += value[i];
    }

    // 終了時に一度だけ合計価値を加える。
    double finish_value(const State& s) const {
        return s.value;
    }
};

int main() {
    int n, capacity, trials;

    if (!(std::cin >> n >> capacity >> trials)) {
        return 0;
    }

    assert(1 <= n && n <= 20 && capacity > 0 && trials > 0);

    // 品物の固定データはModelに一つだけ持つ。
    Model model;
    model.weight.resize(n);
    model.value.resize(n);
    for (int i = 0; i < n; ++i) {
        std::cin >> model.weight[i] >> model.value[i];
    }

    // 設定を省略した完全観測の探索器。
    MonteCarloPlanner planner(model);

    // 最大でもN品なので先読み手数はN。
    planner.start(Model::State{capacity}, n);

    // この探索器の予算型を使う。
    decltype(planner)::Limits limits;

    // 完了試行数で停止する。
    limits.max_simulations = trials;

    // 終端に到達した試行だけが平均に加わる。
    planner.search(limits);

    // 既に終端ならnulloptになる。
    auto action = planner.best_action();
    // 推薦は最初の一品だけ。
    if (action) {
        std::cout << "first_item=" << *action << '\n';
    } else {
        // 最初から何も入らない入力も扱う。
        std::cout << "first_item=none\n";
    }
    // 一手報酬でも同じ価値を足すと二重計上になるため、このモデルのstepはvoidにしている。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
first_item=0
```

`first_item` は最初に積む品物番号です。最終的な全品物の組み合わせは返していません。最初から合法な品物がなければ `first_item=none` と出します。

### 例04：今の収益と設備投資を、割引率付きで比較する

毎日「生産する」か「費用を払って設備を一段階強化する」かを選びます。設備が強いほど後日の生産利益が増えます。先の利益を小さく見積もりたいときは割引率を1未満にします。
根より先では、残り日数が十分なら投資する簡単な継続ルールを使います。そのルールに沿った将来を考えた、今日の行動の比較です。

**入力形式**

`残り日数 初期レベル 投資費用 割引率 trials`。日数・レベルは1以上、費用は非負、割引率は0以上1以下、`trials >= 1`です。

**入力例**

```text
8 1 2 0.95 3000
```

**実行できるコード：`examples/04_investment.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 完全観測なのでInfo型は定義しない。
struct Model {
    // 設備レベルだけが変わる。
    struct State {
        int level;
    };

    // 0は生産、1は投資。
    using Action = int;

    double upgrade_cost;

    // 両行動を常に選べる。
    std::array<int, 2> actions(const State&) const {
        return {0, 1};
    }

    // 一手分の収支を返す。
    double step(State& s, int action, MonteCarloNoise& rng) const {
        if (action == 1) {
            ++s.level;
            return -upgrade_cost;
        }

        // 生産利益には小さな変動を加える。
        return s.level + 0.2 * rng.uniform01();
    }

    // 先の行動を決める利用者のルール。
    int rollout_action(const State& s, int remaining, MonteCarloNoise&) const {
        // remainingにはこれから行う一手も含む。
        return remaining >= 4 && s.level < 3 ? 1 : 0;
    }
};

int main() {
    int horizon, level, trials;
    double cost, discount;

    if (!(std::cin >> horizon >> level >> cost >> discount >> trials)) {
        return 0;
    }

    assert(horizon > 0 && level > 0 && trials > 0 && 0 <= discount && discount <= 1);

    // 大きな固定データがあれば、このModelへ置く。
    Model model{cost};

    // 既定RNGで将来の収益を発生させる。
    using Planner = MonteCarloPlanner<Model>;

    // 必要な設定だけ変更する。
    Planner::Options options;

    // t手先の報酬にdiscountのt乗を掛ける。
    options.discount = discount;

    // 探索中はcostや継続ルールを変更しない。
    Planner planner(model, options);

    // 現在レベルから残り日数を考える。
    planner.start(Model::State{level}, horizon);

    // 一回の探索の予算。
    Planner::Limits limits;

    // この回数の試行を完了させる。
    limits.max_simulations = trials;

    // 木を使わないので根より先はrollout_actionを使う。
    planner.search(limits);

    // 0なら生産、1なら投資。
    std::cout << "today=" << *planner.best_action() << '\n';
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
today=1
```

`today=0` は今日生産する、`today=1` は今日投資するという意味です。先の行動は、この例の `rollout_action` のルールで評価されています。

### 例05：ゴール到着と、期限までに届かなかった場合を分けて採点する

距離D先のゴールへ移動します。歩くと必ず1進み、走ると確率で3進みますが、走る方が一手の費用が高くなります。ゴールへ着けばボーナス、制限手数を使い切ったら残距離に応じた罰点を付けます。
「終端に達した」と「先読み手数を使い切った」を終了理由で区別する例です。

**入力形式**

`距離 残り手数 走る成功確率 trials`。距離・手数・試行数は1以上、確率は0以上1以下です。

**入力例**

```text
8 4 0.8 3000
```

**実行できるコード：`examples/05_deadline_goal.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 終了理由を使って到着と時間切れを区別する。
struct Model {
    // 現在地は試行ごとに独立する。
    struct State {
        int position = 0;
    };

    // 0は歩行、1は走行。
    using Action = int;

    int goal;
    double success;

    // 非終端での候補。
    std::array<int, 2> actions(const State&) const {
        return {0, 1};
    }

    // 状態を進め、移動費用を負の報酬で返す。
    double step(State& s, int action, MonteCarloNoise& rng) const {
        // 走行成功をRNGで決める。
        s.position += action == 0 ? 1 : (rng.uniform01() < success ? 3 : 0);
        return action == 0 ? -1.0 : -2.0;
    }

    // 観測できる到着を終了条件にする。
    bool terminal(const State& s) const {
        return s.position >= goal;
    }

    // APIが渡す終了理由を読む。
    double finish_value(const State& s, MonteCarloFinishReason reason) const {
        // 最終手で到着してもterminalが優先される。
        if (reason == MonteCarloFinishReason::terminal) {
            return 20;
        }

        // horizonなら未到着の残距離を罰点にする。
        return -4.0 * std::max(0, goal - s.position);
    }
};

int main() {
    int goal, horizon, trials;
    double success;

    if (!(std::cin >> goal >> horizon >> success >> trials)) {
        return 0;
    }

    assert(goal > 0 && horizon > 0 && trials > 0 && 0 <= success && success <= 1);

    // 探索中に固定する距離と成功確率。
    Model model{goal, success};

    // 既定では割引しない。
    MonteCarloPlanner planner(model);

    // 実際の期限に対応する先読み手数。
    planner.start(Model::State{}, horizon);

    // 試行回数だけを制限する。
    decltype(planner)::Limits limits;

    // 全候補合計の試行数。
    limits.max_simulations = trials;

    // terminalまたはhorizonでfinish_valueを呼ぶ。
    planner.search(limits);

    // 0は歩く、1は走る。
    std::cout << "move=" << *planner.best_action() << '\n';

    // 検索の計算予算切れはMonteCarloFinishReasonではない。未完了試行は次のsearchへ保持される。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
move=1
```

`move=0` は歩行、`move=1` は走行です。到着ボーナスと未到着罰点も含めて、現在の一行動を比較しています。

### 例06：固定グラフを共有し、試行中は現在地だけをコピーする

有向グラフ上で、数手以内にゴールへ到着する最初の移動先を選びます。辺を通る費用を払い、到着すると100点を得ます。多数の辺を持つグラフを毎試行コピーする必要はありません。
グラフはModelに置き、Stateには現在地だけを持たせます。行き止まりでも「待機」を選べるようにし、非終端で候補が空になることを防ぎます。

**入力形式**

`N M 開始頂点 ゴール 手数 trials`、続くM行に `from to cost`。すべての頂点番号は0以上N未満、`1 <= N <= 100`、`M >= 0`、手数・試行数は正、費用は非負です。

**入力例**

```text
4 5 0 3 4 4000
0 1 1
0 2 5
1 3 2
2 3 1
1 2 4
```

**実行できるコード：`examples/06_shared_graph.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 固定グラフと変化するStateを分ける。
struct Model {
    // 各試行でコピーされるのは現在地だけ。
    struct State {
        int vertex;
    };

    // 行動は移動先と、その辺の費用。
    struct Action {
        int to;
        double cost;
    };

    std::vector<std::vector<Action>> graph;
    int goal;

    // 再利用バッファへ合法な辺を列挙する。
    void actions(const State& s, int, std::vector<Action>& out) const {
        out.insert(out.end(), graph[s.vertex].begin(), graph[s.vertex].end());
        // 行き止まりでも続けられる待機行動。
        out.push_back({s.vertex, 1});
    }

    // 大きい行動型にもconst参照を使える。
    double step(State& s, const Action& a, MonteCarloNoise&) const {
        s.vertex = a.to;
        return -a.cost;
    }

    // 到着したらその試行を終える。
    bool terminal(const State& s) const {
        return s.vertex == goal;
    }

    // 到着ボーナス。
    double finish_value(const State& s) const {
        return s.vertex == goal ? 100 : 0;
    }
};

int main() {
    int n, m, start, goal, horizon, trials;

    if (!(std::cin >> n >> m >> start >> goal >> horizon >> trials)) {
        return 0;
    }

    assert(n > 0 && horizon > 0 && trials > 0);

    // グラフはplannerより長く保持する。
    Model model{std::vector<std::vector<Model::Action>>(n), goal};
    for (int i = 0; i < m; ++i) {
        int from, to;
        double cost;
        std::cin >> from >> to >> cost;
        model.graph[from].push_back({to, cost});
    }

    // Modelへの参照を保持し、グラフ自体はコピーしない。
    MonteCarloPlanner planner(model);

    // 根の現在地をコピーして開始する。
    planner.start(Model::State{start}, horizon);

    // 今回の探索予算。
    decltype(planner)::Limits limits;

    // 完了する試行の上限。
    limits.max_simulations = trials;

    // Stateだけを変えながら固定グラフを参照する。
    planner.search(limits);

    // 独自Actionのコピーを受け取る。
    auto action = planner.best_action();
    // フィールドを読んで実行する。
    if (action) {
        std::cout << "next=" << action->to << " cost=" << action->cost << '\n';
    } else {
        // 開始地点がゴールなら行動はない。
        std::cout << "next=none\n";
    }
    // 探索中にgraphを変えない。辺を変えた後の再計画ではstartを呼び直す。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
next=2 cost=5
```

`next` は最初の移動先、`cost` はその行動の費用です。待機なら現在地が返ります。開始時点で既にゴールなら `next=none` です。複数手の経路全体の費用を表示しているわけではありません。

### 例07：先の在庫価値を近似し、長い期間を浅く評価する

毎日0〜3個を仕入れ、その日の需要に応じて販売します。仕入れは1個3点の費用、販売は1個5点の収益です。需要は0〜3個で変動します。
何十日も毎回最後まで進める代わりに、最初の数日だけシミュレーションし、残った在庫を今後売れる見込みで採点します。この末端評価は「現在庫を売る価値」の近似で、今後の追加仕入れの利益までは含めません。評価の近似を入れる場所を示す例です。

**入力形式**

`初期在庫 残り日数 シミュレーション深さ trials`。在庫は0以上、`1 <= 深さ <= 日数 <= 50`、`trials >= 1`です。

**入力例**

```text
0 10 2 3000
```

**実行できるコード：`examples/07_inventory_leaf.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 浅い打ち切りを使う在庫モデル。
struct Model {
    // その試行の在庫。
    struct State {
        int stock;
    };

    // 今日仕入れる個数。
    using Action = int;

    // 合法な仕入れ量。
    std::array<int, 4> actions(const State&) const {
        return {0, 1, 2, 3};
    }

    // 今日の仕入れと販売を一手とする。
    double step(State& s, int order, MonteCarloNoise& rng) const {
        s.stock += order;
        // 0〜3の需要を作る。引数4自体は含まない。
        int demand = int(rng.uniform_int(4));
        int sold = std::min(s.stock, demand);
        s.stock -= sold;
        return 5.0 * sold - 3.0 * order;
    }

    // 打ち切りまでの継続ルール。
    int rollout_action(const State& s, int, MonteCarloNoise&) const {
        return std::clamp(3 - s.stock, 0, 3);
    }

    // 打ち切り後の残り価値を、この地点の尺度で返す。
    double leaf_value(const State& s, int remaining) const {
        double expected_sold = std::min(double(s.stock), 1.5 * remaining);
        return 5 * expected_sold + 0.2 * (s.stock - expected_sold);
    }

    // 本当の期間終了時は残品を処分する。
    double finish_value(const State& s) const {
        return 0.2 * s.stock;
    }
};

int main() {
    int stock, horizon, depth, trials;

    if (!(std::cin >> stock >> horizon >> depth >> trials)) {
        return 0;
    }

    assert(stock >= 0 && 1 <= depth && depth <= horizon && horizon <= 50 && trials > 0);

    // 採点の係数は探索中に固定する。
    Model model;

    // 完全観測ではInfoはStateと同じ型。
    using Planner = MonteCarloPlanner<Model>;

    // 必要な設定だけ変更する。
    Planner::Options options;

    // 正の深さで、horizonに届く前ならleaf_valueを呼ぶ。
    options.simulation_depth = depth;

    // simulation_depthを構築時に確定する。
    Planner planner(model, options);

    // horizonは問題の期間で、depthとは別。
    planner.start(Model::State{stock}, horizon);

    // 今回の探索予算。
    Planner::Limits limits;

    // 浅い評価で完了した試行も1回として数える。
    limits.max_simulations = trials;

    // leaf_valueとfinish_valueを同じ地点で二重には加えない。
    auto run = planner.search(limits);

    // 今日の注文量と実行したstep数。
    std::cout << "order=" << *planner.best_action() << " steps=" << run.transitions << '\n';

    // 既に受け取った報酬はleaf_valueへ含めない。
    // 割引率を使う場合の外側の割引もライブラリが行う。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
order=3 steps=6000
```

`order` は今日の仕入れ量、`steps` は今回呼んだ `step` の合計回数です。この例には途中終端がないので、試行数とシミュレーション深さの積になります。`depth=horizon` なら浅い近似を使わず、`finish_value` で終わります。

1.5は0〜3の一様需要の平均です。ただし `min(在庫, 平均需要)` は、実際の販売量の期待値と一般には一致しません。この例では、末端評価として扱いやすい近似を意図的に用いています。精度が重要な問題では、ここを適切な推定へ置き換えます。

### 例08：他の方法で作った割り当て案を、ばらつき込みで比較する

仕事を2台の機械へ割り当てます。各仕事の所要時間は、基準時間から最大20%増えるとします。全仕事が終わる時刻、つまり2台の負荷の大きい方を小さくしたい問題です。
この例では「全部を機械0へ」「全部を機械1へ」「番号順に交互」の3案を外側で作ります。ライブラリは案そのものを作らず、与えられた配列を一つのActionとして比較します。

**入力形式**

`仕事数 機械1の速度倍率 trials`、次の行に仕事数個の基準時間。`1 <= 仕事数 <= 50`、速度倍率と基準時間は正、`trials >= 1`です。機械0の速度倍率は1です。

**入力例**

```text
5 1.5 3000
8 5 7 4 6
```

**実行できるコード：`examples/08_assignment_plans.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 配列全体を一つの行動として評価する。
struct Model {
    // 一回で全案を評価するので状態は不要。
    struct State {};

    // 各仕事を載せる機械の番号を並べた配列。
    using Action = std::vector<int>;

    std::vector<double> duration;
    double speed1;

    // Actionはコピーせずconst参照で読む。
    double step(State&, const Action& plan, MonteCarloNoise& rng) const {
        std::array<double, 2> load{};
        for (int i = 0; i < int(duration.size()); ++i) {
            // 各仕事の所要時間を発生させる。
            double time = duration[i] * (1 + 0.2 * rng.uniform01());
            load[plan[i]] += time / (plan[i] == 0 ? 1 : speed1);
        }

        // 完了時刻の最小化を、負の得点の最大化にする。
        return -std::max(load[0], load[1]);
    }
};

int main() {
    int n, trials;
    double speed1;

    if (!(std::cin >> n >> speed1 >> trials)) {
        return 0;
    }

    assert(1 <= n && n <= 50 && speed1 > 0 && trials > 0);

    // 固定の基準時間はModelに保持する。
    Model model{std::vector<double>(n), speed1};
    for (double& x : model.duration) {
        std::cin >> x;
    }

    // 一つの候補の型がvector<int>。
    std::vector<Model::Action> plans(3, Model::Action(n));
    std::fill(plans[1].begin(), plans[1].end(), 1);
    for (int i = 0; i < n; ++i) {
        plans[2][i] = i % 2;
    }

    // 自動候補生成用actionsは、この一手問題では不要。
    MonteCarloPlanner planner(model);

    // 根候補を明示登録する。候補はここでコピーされる。
    planner.start(Model::State{}, 1, plans);

    // 試行数の予算。
    decltype(planner)::Limits limits;

    // 3案合計の試行数。
    limits.max_simulations = trials;

    // 与えた案だけを根で比較する。
    planner.search(limits);

    // 候補統計の配列を作らず、推薦案だけ取得する。
    auto result = planner.result(false);

    std::cout << "assignment=";
    // 推薦された配列をそのまま利用する。
    for (int machine : *result.action) {
        std::cout << machine << ' ';
    }

    std::cout << '\n';

    // 多手問題で根を明示指定しても、根より先の候補・継続ルールは別途Modelに必要。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
assignment=1 1 1 1 1
```

`assignment` のi番目は仕事iを割り当てる機械番号です。表示しているのは、3案のうち推薦された配列全体です。用意していない別の割り当てを自動的に生成するわけではありません。

### 例09：価格を観測してから売るか蓄えるかを決める木探索

毎日1単位の電力を得て、「今ある分を売る」か「容量3の蓄電池へ蓄える」かを決めます。売値は安値2点または高値5点で、その日の値は行動前に見えています。翌日の値は半々で変わります。
将来、高値を観測した状態と安値を観測した状態で、異なる判断を学べるように木を使います。同じ状態を識別するキーには、日数・蓄電量・観測済み価格をすべて含めます。

**入力形式**

`日数 初期蓄電量 初期高値フラグ trials max_nodes max_edges`。`1 <= 日数 <= 20`、蓄電量0〜3、高値フラグ0/1、試行数は正、`max_nodes >= 1`、`max_edges >= 2`です。

**入力例**

```text
5 1 0 5000 1000 4000
```

**実行できるコード：`examples/09_weather_tree.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 将来の観測状態ごとに判断を分ける完全観測モデル。
struct Model {
    // 価格highも現在は観測済み。
    struct State {
        int day = 0, stock = 0, high = 0;
    };

    // 0は売却、1は蓄電。
    using Action = int;

    // 常に2行動。
    std::array<int, 2> actions(const State&) const {
        return {0, 1};
    }

    // 一日分を進める。
    double step(State& s, int action, MonteCarloNoise& rng) const {
        double reward = 0;
        if (action == 0) {
            reward = (s.high ? 5 : 2) * (s.stock + 1);
            s.stock = 0;
        } else {
            s.stock = std::min(3, s.stock + 1);
        }

        ++s.day;
        // 翌日の価格を発生させ、観測済みStateに入れる。
        s.high = int(rng.uniform_int(2));
        return reward;
    }

    // 将来の選択・得点に必要な全情報。
    auto tree_key(const State& s) const {
        return std::tuple(s.day, s.stock, s.high);
    }

    // 木の外へ出た後の簡単な方策。
    int rollout_action(const State& s, int remaining, MonteCarloNoise&) const {
        return s.high || remaining == 1 || s.stock == 3 ? 0 : 1;
    }

    // 期間終了時の残電力の価値。
    double finish_value(const State& s) const {
        return 0.5 * s.stock;
    }
};

int main() {
    int days, stock, high, trials, nodes, edges;

    if (!(std::cin >> days >> stock >> high >> trials >> nodes >> edges)) {
        return 0;
    }

    assert(
        days > 0
        && 0 <= stock
        && stock <= 3
        && 0 <= high
        && high <= 1
        && trials > 0
        && nodes >= 1
        && edges >= 2
    );

    // 共有モデルの定義は探索中に固定する。
    Model model;

    // tree_keyの戻り値に等値比較が必要。
    using Planner = MonteCarloPlanner<Model>;

    // 木に関係する項目を変更する。
    Planner::Options options;

    // 根より先の判断点にも候補と統計を保存する。
    options.tree = true;

    // 根を含む保存ノード数の上限。
    options.max_nodes = nodes;

    // 全ノード合計の保存行動数の上限。
    options.max_edges = edges;

    // 木の設定は構築時に渡す。
    Planner planner(model, options);

    // 現在の観測済み状態を根にする。
    planner.start(Model::State{0, stock, high}, days);

    // 今回の探索予算。
    Planner::Limits limits;

    // 合計の完了試行数。
    limits.max_simulations = trials;

    // 新しい子を保存した試行の残りはrolloutへ進む。
    planner.search(limits);

    // 今日売るなら0、蓄えるなら1。
    std::cout << "today=" << *planner.best_action() << '\n';

    // 保存上限の先ではrolloutを使う。max_edgesが小さすぎると保存される候補自体も減り得る。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
today=1
```

`today=0` は今ある電力の売却、`today=1` は蓄電です。価格の変化で将来の状態が分岐するため、現在が安値でも、後日の高値で売る判断を含めて比較します。

### 例10：実際に一手進め、対応する部分木から追加探索する

一直線の道を歩くか走って、期限内にゴールへ着きます。歩くと1進み費用1、走ると2進み費用3です。この例では移動が確定的なので、計画した次状態と実際の次状態が対応します。
一手を実行した後、残り手数を一つ減らし、`advance`へ実際の次状態を渡します。再利用できない場合も新しい根へ移っているので、そのまま追加探索できます。

**入力形式**

`ゴールまでの距離 手数 各ターンのtrials`。すべて正の整数、手数は20以下です。

**入力例**

```text
6 5 3000
```

**実行できるコード：`examples/10_advance_turns.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 実ターンと同じ確定的な移動規則を使う。
struct Model {
    // キー省略時はState自体を比較する。
    struct State {
        int position = 0;
        friend bool operator==(const State&, const State&) = default;
    };

    // 0は1歩、1は2歩。
    using Action = int;

    int goal;

    // 候補生成をModel側に持つ。
    std::array<int, 2> actions(const State&) const {
        return {0, 1};
    }

    // RNGは使わないが必須引数として受け取る。
    double step(State& s, int action, MonteCarloNoise&) const {
        s.position = std::min(goal, s.position + (action == 0 ? 1 : 2));
        return action == 0 ? -1 : -3;
    }

    // ゴールで終了する。
    bool terminal(const State& s) const {
        return s.position == goal;
    }

    // 到着ボーナス。
    double finish_value(const State& s) const {
        return s.position == goal ? 30 : 0;
    }
};

int main() {
    int goal, horizon, trials;

    if (!(std::cin >> goal >> horizon >> trials)) {
        return 0;
    }

    assert(goal > 0 && 1 <= horizon && horizon <= 20 && trials > 0);

    // 実ターン間でも問題の規則を変更しない。
    Model model{goal};

    // 完全観測のStateを扱う。
    using Planner = MonteCarloPlanner<Model>;

    // 部分木再利用の条件を用意する。
    Planner::Options options;

    // 木を保存する。simulation_depthは既定の0で最後まで進める。
    options.tree = true;

    // 同じ探索器をターンをまたいで使う。
    Planner planner(model, options);
    // 実環境の状態。探索器内の試行用コピーとは別。
    Model::State real;

    // 初回だけ根を設定する。
    planner.start(real, horizon);

    // 各searchに同じ回数予算を渡す。
    Planner::Limits limits;

    // 毎ターンの追加試行数。
    limits.max_simulations = trials;
    for (int turn = 0; turn < horizon && real.position < goal; ++turn) {
        // 引き継いだ統計があれば、その上へ追加する。
        planner.search(limits);

        // 推薦行動と、この根で有効なIDを取る。
        const auto result = planner.result(false);
        // 行動がなければIDをadvanceへ渡さない。
        if (!result.action) {
            break;
        }

        // 実行する値と内部IDは別物。
        int action = *result.action;
        real.position = std::min(goal, real.position + (action == 0 ? 1 : 2));

        // 実際の次Stateと、一つ減らした手数を渡す。
        bool reused = planner.advance(result.id, real, horizon - turn - 1);

        std::cout << "turn=" << turn
                  << " action=" << action
                  << " position=" << real.position
                  << " reused=" << reused << '\n';

        // この後result.idは古い世代のID。次回は新しいresultからIDを取り直す。
        // reused=falseでも根の移動は完了しているため、同じnextでstartを重ねる必要はない。
    }
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
turn=0 action=1 position=2 reused=1
turn=1 action=0 position=3 reused=1
turn=2 action=0 position=4 reused=1
turn=3 action=1 position=6 reused=0
```

各行は、実際に一手進めた後の状態です。`turn` は0始まりのターン、`action` は実行した行動、`position` は実際の次位置です。`reused=1` は部分木を再利用、0は次状態から統計をリセットしたことを表します。0でも根の更新は済んでいます。

実環境から入力を受け取るターン制問題では、コード中の `real.position` の直接更新部分を、行動の送信と次観測の受信へ置き換えます。

### 例11：道路費用などのモデル更新を反映して再計画する

複数の経路候補があり、時間とともに通行費用が更新されます。更新前の平均値をそのまま使うと、現在の費用とは異なる問題を比較してしまいます。
固定の経路番号一覧は再利用しながら、費用を書き換えた後に`start`で統計をリセットします。最後の`clear`は、案件を終えて探索器を未準備状態へ戻す使い方です。

**入力形式**

`経路数 更新数 trials`、次の行に各経路の費用、続く更新数行に `経路番号 新費用`。経路数1〜100、更新数0以上、費用は非負、試行数は正です。

**入力例**

```text
3 2 1000
8 4 6
1 12
0 20
```

**実行できるコード：`examples/11_changed_costs.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 候補番号は固定で、費用だけを更新する。
struct Model {
    // 一回の評価で終わる。
    struct State {};

    // 経路の番号。
    using Action = int;

    std::vector<int> ids;
    std::vector<double> costs;

    // 固定候補を借用し、毎回の配列生成を避ける。
    const std::vector<int>& actions(const State&) const {
        return ids;
    }

    // 現在の費用を負の得点にする。
    double step(State&, int route, MonteCarloNoise&) const {
        return -costs[route];
    }
};

int main() {
    int n, updates, trials;

    if (!(std::cin >> n >> updates >> trials)) {
        return 0;
    }

    assert(1 <= n && n <= 100 && updates >= 0 && trials > 0);

    // plannerが参照する同じModelを保持する。
    Model model;
    model.ids.resize(n);
    model.costs.resize(n);
    std::iota(model.ids.begin(), model.ids.end(), 0);
    for (double& cost : model.costs) {
        std::cin >> cost;
    }

    // 固定データを保持したまま、複数案件に使える。
    MonteCarloPlanner planner(model);

    // 各回の予算。
    decltype(planner)::Limits limits;

    // その回に完了させる試行数。
    limits.max_simulations = trials;
    for (int t = 0; t <= updates; ++t) {
        if (t > 0) {
            int id;
            double cost;
            std::cin >> id >> cost;
            model.costs[id] = cost;
        }

        // 更新後のモデルに対して統計・未完了試行をリセットする。
        planner.start(Model::State{}, 1);

        // 更新中には探索を行わない。
        planner.search(limits);

        // 現時点の推薦。
        std::cout << "stage=" << t << " route=" << *planner.best_action() << '\n';
    }

    // 根・統計を破棄し、主要な作業配列の容量を保持する。
    planner.clear();
    // clear後は未準備。search前にstartが必要。
    assert(planner.result().status == decltype(planner)::Status::not_ready);

    // startやclearはModel.costsそのものを初期化しない。借用データの管理は利用者が行う。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
stage=0 route=1
stage=1 route=2
stage=2 route=2
```

`stage=0` は更新前、1以降は入力した費用更新を順に反映した後です。`route` はその時点で選んだ経路番号です。候補番号は同じでも、得点の意味が変わるので統計を持ち越しません。

### 例12：長いシミュレーションを少しずつ進め、途中から再開する

装置の運転モードを最初に選び、その後の長い運転期間で得られる収益を比べます。一試行を最後まで回す前に、別の作業へ制御を戻したい場面を想定します。
`max_transitions`で一回の呼び出しのstep数を制限します。途中の得点を平均へ混ぜず、次の`search`で同じStateと乱数位置から続きを実行します。

**入力形式**

`運転期間 一回のstep上限 呼び出し回数`。いずれも1以上、運転期間は10000以下です。

**入力例**

```text
10 3 8
```

**実行できるコード：`examples/12_resume_chunks.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 根でモードを選び、その後は固定モードで運転する。
struct Model {
    // 未完了試行のこの状態が保持される。
    struct State {
        int turn = 0, mode = 0;
    };

    // 根は0/1、後続の2は「そのまま運転」。
    using Action = int;

    // 呼び出し一回を1transitionと数える。
    double step(State& s, int action, MonteCarloNoise& rng) const {
        if (s.turn == 0) {
            s.mode = action;
        }

        ++s.turn;
        // このRNG位置も再開に対応する。
        return (s.mode == 0 ? 1.0 : 1.2) + rng.uniform01();
    }

    // 根以降はモードを変えない。
    int rollout_action(const State&, int, MonteCarloNoise&) const {
        return 2;
    }
};

int main() {
    int horizon, chunk, calls;

    if (!(std::cin >> horizon >> chunk >> calls)) {
        return 0;
    }

    assert(1 <= horizon && horizon <= 10000 && chunk > 0 && calls > 0);

    // 再開の間にモデルの意味を変えない。
    Model model;

    // 既定のCRN OFFでは一つのRNGを連続使用する。
    using Planner = MonteCarloPlanner<Model>;

    // 呼び出しごとに作り直さず同じ探索器を使う。
    Planner planner(model);

    // 根候補を先に登録し、未評価でも暫定推薦を用意する。
    planner.start(Model::State{}, horizon, std::array{0, 1});

    // 一回に使う作業量を指定する。
    Planner::Limits limits;

    // 完了試行数ではなくstepの呼び出し数を制限する。
    limits.max_transitions = chunk;
    int completed = 0;
    for (int i = 0; i < calls; ++i) {
        // 中断中の試行があれば、その続きから始める。
        const auto run = planner.search(limits);
        // 今回完了した分だけ加える。
        completed += run.simulations;

        // 途中経過を読むが、探索は進めない。
        const auto result = planner.result(false);

        // 新規開始と完了は別。
        // 暫定かどうかを確認する。
        std::cout << "call=" << i
                  << " started=" << run.started
                  << " completed=" << run.simulations
                  << " steps=" << run.transitions
                  << " provisional=" << result.provisional << '\n';
    }

    // 最後に全候補の蓄積統計を取得する。
    const auto result = planner.result();
    int stored = 0;
    // 統計は完了した試行だけ。
    for (const auto& candidate : result.candidates) {
        stored += candidate.trials;
    }

    assert(stored == completed);

    // 未完了だけなら先頭候補の暫定推薦。
    std::cout << "total_completed=" << completed << " action=" << *result.action << '\n';

    // 途中でstart/clearを呼ぶと未完了試行も捨てる。継続したいときはsearchだけを繰り返す。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
call=0 started=1 completed=0 steps=3 provisional=1
call=1 started=0 completed=0 steps=3 provisional=1
call=2 started=0 completed=0 steps=3 provisional=1
call=3 started=1 completed=1 steps=3 provisional=0
call=4 started=0 completed=0 steps=3 provisional=0
call=5 started=0 completed=0 steps=3 provisional=0
call=6 started=1 completed=1 steps=3 provisional=0
call=7 started=0 completed=0 steps=3 provisional=0
total_completed=2 action=1
```

`started`、`completed`、`steps` は各呼び出しだけの数です。最初の3回は10手の試行を終えられず、`provisional=1` のままです。4回目で最初の試行が完了します。最後の `total_completed` は呼び出しをまたいで足した完了数で、候補別試行数の合計と一致することを `assert` でも確認します。

### 例13：比較中に、別の探索器から届いた候補を追加する

製造ラインの温度候補を比較します。ここでは、目標温度から離れるほど歩留まりが下がり、測定値にも小さな変動があるとします。
最初の候補で評価を始め、後から到着した候補を`add_root_action`で追加します。採点の意味が同じまま候補だけ増えるので、既存候補の統計を保持できます。

**入力形式**

`目標温度 初期候補数 追加候補数 各回のtrials`、次に初期候補の温度、次に追加候補の温度。温度は0〜100、各候補数1〜50、試行数は正です。

**入力例**

```text
12 2 1 1000
5 8
12
```

**実行できるコード：`examples/13_add_candidate.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 候補だけを増やし、評価関数は変更しない。
struct Model {
    // 温度を一回測って評価する。
    struct State {};

    // 設定する温度そのもの。
    using Action = int;

    double target;

    // 温度から歩留まり得点を計算する。
    double step(State&, int temperature, MonteCarloNoise& rng) const {
        double d = temperature - target;
        // 渡されたRNGで測定の変動を加える。
        return 100 - std::abs(d) - 0.02 * d * d + 0.1 * rng.uniform01();
    }
};

int main() {
    double target;
    int n, k, trials;

    if (!(std::cin >> target >> n >> k >> trials)) {
        return 0;
    }

    assert(n > 0 && k > 0 && trials > 0);
    std::vector<int> initial(n), extra(k);
    for (int& x : initial) {
        std::cin >> x;
    }

    for (int& x : extra) {
        std::cin >> x;
    }

    // モデルを変えた場合は追加でなくstartによるリセットが必要。
    Model model{target};

    // 根候補を外側で管理する探索器。
    using Planner = MonteCarloPlanner<Model>;

    // 同じ探索器へ後から候補を加える。
    Planner planner(model);

    // 最初の温度候補をコピー登録する。
    planner.start(Model::State{}, 1, initial);

    // 追加探索一回あたりの予算。
    Planner::Limits limits;

    // 全候補を合計した新たな完了試行数。
    limits.max_simulations = trials;

    // まず初期候補だけで評価する。
    planner.search(limits);

    // 初期段階の推薦温度。
    std::cout << "before=" << *planner.best_action() << '\n';
    // 行動値と内部の識別IDは別に持つ。
    std::vector<Planner::ActionId> added;
    // 既存統計を残したまま追加し、IDを受け取る。
    for (int x : extra) {
        added.push_back(planner.add_root_action(x));
    }

    // 前回の統計の上へ新しい試行を足す。
    planner.search(limits);

    // 候補統計も含めて取得する。
    const auto result = planner.result();

    // 更新された推薦温度。
    std::cout << "after=" << *result.action << '\n';
    // 配列の並びから温度を推測しない。
    for (const auto& candidate : result.candidates) {
        // IDの等値比較で候補を探す。
        if (candidate.id == added.front()) {
            std::cout << "first_added=" << candidate << '\n';
        }
    }

    // ActionIdはgeneration:index形式で直接出力できる。
    std::cout << "added_id=" << added.front() << '\n';

    // 重複した温度も別候補として登録される。重複排除が必要なら追加前に行う。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
before=8
after=12
first_added=Candidate{id=1:2, trials=998, mean=100.051}
added_id=1:2
```

`before` は追加前、`after` は追加後の推薦温度です。`first_added` は最初に追加した候補をIDで探した統計です。追加された候補が配列の末尾にある、という仮定はしていません。

### 例14：連続値の販売価格を、候補を少しずつ作って探す

価格を上げるほど一個あたりの利益は増えますが、需要は減るとします。需要にはランダムな変動もあります。「価格−原価」と需要を掛けた利益を大きくする価格を探します。
価格を細かい刻みですべて並べず、端点・中点・乱数による価格を順に提案します。参加候補数の増やし方と、候補の評価へ使う探索の強さを明示設定する例です。

**入力形式**

1行目は `下限価格 上限価格 基本需要 価格への感度 原価 提案上限 trials`。2行目は `widening exploration value_scale`。価格は非負で下限<上限、感度は正、提案上限・試行数は正、2行目は非負の有限値です。

**入力例**

```text
1 12 30 2 1 30 5000
1.5 1 50
```

**実行できるコード：`examples/14_continuous_price.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// Actionを実数にして連続パラメータを扱う。
struct Model {
    // 一つの価格を一日分評価する。
    struct State {};

    // 値そのものが販売価格。
    using Action = double;

    double low, high, demand, slope, unit_cost;
    int proposals;

    // indexはこの判断点での0始まりの提案番号。
    std::optional<double> propose_action(
        const State&,
        int,
        int index,
        MonteCarloNoise& rng
    ) const {
        // 一度nulloptを返すと、この判断点での保存用提案は終了する。
        if (index >= proposals) {
            return std::nullopt;
        }

        if (index == 0) {
            return low;
        }

        if (index == 1) {
            return high;
        }

        if (index == 2) {
            return (low + high) / 2;
        }

        // 追加候補を範囲内から作る。
        return low + (high - low) * rng.uniform01();
    }

    // 提案された価格を評価する。
    double step(State&, double price, MonteCarloNoise& rng) const {
        // 需要の変動。
        double buyers = std::max(0.0, demand - slope * price + 4 * (rng.uniform01() - 0.5));
        return (price - unit_cost) * buyers;
    }
};

int main() {
    // パラメータを入力してからplannerを作る。
    Model model;
    int trials;
    double widening, exploration, scale;

    if (!(std::cin >> model.low >> model.high >> model.demand >> model.slope >> model.unit_cost
        >> model.proposals >> trials)) {
        return 0;
    }

    std::cin >> widening >> exploration >> scale;
    assert(model.low < model.high && model.slope > 0 && model.proposals > 0 && trials > 0);

    // Action=doubleをそのまま扱う。
    using Planner = MonteCarloPlanner<Model>;

    // 候補数と評価配分の調整項目。
    Planner::Options options;

    // 正なら完了数の平方根に応じて候補を増やす。0なら毎試行最大一つを追加参加。
    options.widening = widening;

    // 大きいほど評価が少ない候補への加点を強める。
    options.exploration = exploration;

    // 0なら得点から自動推定。正なら得点と同じ単位の尺度を指定する。
    options.value_scale = scale;

    // 保存候補数を提案上限に合わせる。この例は一手なので根だけ。
    options.max_edges = model.proposals;

    // 調整した設定をコピーする。
    Planner planner(model, options);

    // 明示候補を渡さず、Modelの提案器を使う。
    planner.start(Model::State{}, 1);

    // 全候補合計の予算。
    Planner::Limits limits;

    // 候補ごとの回数を固定する設定ではない。
    limits.max_simulations = trials;

    // 提案と有望な候補の追加評価を進める。
    planner.search(limits);

    // 実際に保存された候補数も確認する。
    const auto result = planner.result();

    // 推薦された価格のコピー。
    // 提案上限まで必ず到達するとは限らない。
    std::cout << std::fixed << std::setprecision(4)
              << "price=" << *result.action
              << " registered=" << result.candidates.size() << '\n';

    // 正のvalue_scaleは報酬の上限ではなく、探索の加点に使う尺度。
    // 連続範囲の厳密最適解は保証されない。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
price=7.8828 registered=30
```

`price` は推薦された実数価格、`registered` は実際に登録した候補数です。提案上限まで必ず登録・評価するわけではなく、現在の予算と参加設定に依存します。

### 例15：一つの締切の中で、段階的に計算時間を配る

通信方式を選ぶ処理へ、一回の呼び出しに使える時間と、全体の締切を与えます。最初の短い探索の後、残った時間で追加探索する場面です。
時間はマイクロ秒で指定します。相対時間・絶対締切・完了試行数を同時に指定した場合、先に確認された停止条件で止まります。時間がなくても、登録済みの候補を暫定で返せます。

**入力形式**

`全体予算_us 後半の試行数上限`。時間は0以上、試行数上限は1以上です。入力例の3000は3ミリ秒です。

**入力例**

```text
3000 100000
```

**実行できるコード：`examples/15_time_budget.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 通信方式ごとの得点を比較する簡略モデル。
struct Model {
    // 一手で得点を測る。
    struct State {};

    // 方式0/1/2。
    using Action = int;

    // 実際には通信のシミュレータへ置き換えられる。
    double step(State&, int mode, MonteCarloNoise& rng) const {
        // 渡されたRNGを使う有限の得点。
        return mode + rng.uniform01();
    }
};

int main() {
    int64_t total_us;
    int max_trials;

    if (!(std::cin >> total_us >> max_trials)) {
        return 0;
    }

    assert(total_us >= 0 && max_trials > 0);

    // 締切を設定する前に固定データを用意する。
    Model model;

    // 既定時計はsteady_clock。
    using Planner = MonteCarloPlanner<Model>;

    // 時間確認の間隔を設定する。
    Planner::Options options;

    // 各step付近で時計を確認する。確認自体にも費用がある。
    options.clock_interval = 1;

    // 同じ探索器で前半と後半を継続する。
    Planner planner(model, options);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(total_us);

    // 準備時間も絶対締切に含めるため締切設定後に開始。
    planner.start(Model::State{}, 1, std::array{0, 1, 2});

    // 時間だけの簡易形。0なら評価しない。
    const auto first = planner.search(total_us / 4);

    // この簡易形は絶対締切を受け取らない。
    // 準備時間が大きい用途では前半にもLimits.deadlineを渡す。
    // 後半は複数の制約を組み合わせる。
    Planner::Limits limits;

    // 前半やstartに使った時間も含む全体の締切。
    limits.deadline = deadline;

    // 後半単体にも相対時間の上限を設ける。
    limits.time_us = total_us / 2;

    // 後半で完了する試行数の上限。
    limits.max_simulations = max_trials;

    // 前半の統計と未完了状態を保持して追加探索。
    const auto second = planner.search(limits);

    // RunStatsを直接表示し、どの制約で止まったか読む。
    std::cout << "first=" << first << '\n';

    // elapsed_usは今回の呼び出しだけの時間。
    std::cout << "second=" << second << '\n';

    // コピーや表示の時間はsearchの相対予算の外側。
    const auto result = planner.result(false);

    // 予算0なら先頭の未評価候補。
    std::cout << "mode=" << *result.action << " provisional=" << result.provisional << '\n';

    // コールバック途中の強制停止はしない。AHCの全体締切には入出力・後処理の余裕も確保する。
}
```

**出力例と読み方**

実行時の出力例です。時間に関係する値は実行環境で変わります。

```text
first=RunStats{started=8509, simulations=8508, transitions=8508, elapsed_us=750, stop=time}
second=RunStats{started=16560, simulations=16560, transitions=16560, elapsed_us=1500, stop=time}
mode=2 provisional=0
```

`first` と `second` は、それぞれ前半・後半の実行統計です。`mode` は推薦する通信方式、`provisional=1` は未評価の暫定候補です。前半の簡易形 `search(time_us)` には絶対締切が渡らないため、準備時間も大きく変わる用途では、前半も `Limits` で絶対締切を設定してください。

### 例16：見えない箱の中身を、有料で調べる価値を判断する

二つの箱の一方に10点の当たりが入っています。箱を直接選ぶか、費用を払って当たり位置を観測してから選ぶかを決めます。
Stateには試行で仮定した当たり位置、Infoには観測済みの位置と決定済みの箱を入れます。行動を決める処理はInfoだけを読みます。浅く打ち切る設定では、観測済み情報から次の選択で得られる価値を評価します。

**入力形式**

`箱1が当たりの確率 観測費用 trials 浅い評価フラグ`。確率は0〜1、費用は非負、試行数は正、フラグは0/1です。箱番号は0/1、行動2は観測です。

**入力例**

```text
0.5 1 3000 1
```

**実行できるコード：`examples/16_inspect_box.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// Infoの定義があるので部分観測モデルになる。
struct Model {
    // この試行で仮定した真の当たり位置。
    struct State {
        int winner;
    };

    // -1は未観測・未決定。方策が読んでよい情報だけ。
    struct Info {
        int seen = -1, chosen = -1;
    };

    // 0/1は箱の選択、2は観測。
    using Action = int;

    double probability1, inspect_cost;

    // 新規試行ごとに真の状態を一つ作る。
    State sample_root(const Info& info, MonteCarloNoise& rng) const {
        // 既に見た位置とは矛盾させない。
        return {info.seen >= 0 ? info.seen : int(rng.uniform01() < probability1)};
    }

    // 候補生成にはStateを渡さない。
    void actions(const Info& info, int remaining, std::vector<Action>& out) const {
        out = {0, 1};
        if (info.seen < 0 && remaining >= 2) {
            out.push_back(2);
        }
    }

    // 状態と観測情報を一緒に更新する。
    double step(State& state, Info& info, int action, MonteCarloNoise&) const {
        if (action == 2) {
            info.seen = state.winner;
            return -inspect_cost;
        }

        info.chosen = action;
        return action == state.winner ? 10 : 0;
    }

    // 決定したことは観測可能なのでInfoで判定。
    bool terminal(const Info& info) const {
        return info.chosen >= 0;
    }

    // 隠れた当たり位置を直接参照しない。
    int rollout_action(const Info& info, int, MonteCarloNoise&) const {
        return info.seen >= 0 ? info.seen : int(probability1 >= 0.5);
    }

    // Infoだけで採点する形式も使える。
    double leaf_value(const Info& info, int) const {
        return info.seen >= 0 ? 10 : 10 * std::max(probability1, 1 - probability1);
    }
};

int main() {
    double probability, cost;
    int trials, shallow;

    if (!(std::cin >> probability >> cost >> trials >> shallow)) {
        return 0;
    }

    assert(
        0 <= probability
        && probability <= 1
        && cost >= 0
        && trials > 0
        && (shallow == 0 || shallow == 1)
    );

    // 推定確率を探索中に変更しない。
    Model model{probability, cost};

    // Model::Infoを根として受け取る探索器。
    using Planner = MonteCarloPlanner<Model>;

    // 浅い評価を使うか選ぶ。
    Planner::Options options;

    // 1なら観測後の残り一手をleaf_valueで近似する。
    options.simulation_depth = shallow ? 1 : 0;

    // Stateを外から渡して真の当たりを教える使い方はしない。
    Planner planner(model, options);

    // 未観測の情報から、最大2手を計画する。
    planner.start(Model::Info{}, 2);

    // 完了試行数の予算。
    Planner::Limits limits;

    // 候補ごとの回数ではなく全体の回数。
    limits.max_simulations = trials;

    // 各新規試行でsample_rootし、Infoも根からコピーする。
    planner.search(limits);

    // 2ならまず観測する。
    std::cout << "action=" << *planner.best_action() << '\n';

    // stepの返す報酬はInfoへ自動追加されない。実際に知ったことだけをstep内で明示更新する。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
action=2
```

`action=0/1` はその箱を選ぶ、`action=2` は有料観測するという意味です。観測した後の選択は、試行内で得られた `Info.seen` を見て決めます。

### 例17：需要の候補を借用し、実観測で絞り込んで再計画する

見えていない需要を、複数の候補値で表します。各候補値は同じ確率とし、同じ値が複数あればその分だけ確率が高いものとします。仕入れ量qを選び、売れた個数1個につき5点、仕入れ1個につき2点で採点します。
その後「需要がしきい値以上か」という実際の観測を受け取り、矛盾する候補値を外側で除去して再計画します。候補集合の更新は利用者が行い、ライブラリは現在の集合からサンプルするだけです。

**入力形式**

`粒子数 最大仕入れ量 trials`、次に粒子数個の非負需要値、最後に `観測しきい値 以上だったフラグ`。粒子数と試行数は正、最大仕入れ量0〜20、フラグ0/1です。矛盾して粒子がなくなれば、その旨を表示します。

**入力例**

```text
8 6 4000
0 1 1 2 3 4 4 5
3 1
```

**実行できるコード：`examples/17_particle_replan.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 推定用の粒子はModelから借用する。
struct Model {
    // 試行で仮定した真の需要。
    struct State {
        int demand;
    };

    // 未決定なら-1。決めた量は観測可能。
    struct Info {
        int quantity = -1;
    };

    // 仕入れ量そのもの。
    using Action = int;

    const std::vector<int>& particles;
    int capacity;

    // 各新規試行で現在の推定集合から一つ引く。
    State sample_root(const Info&, MonteCarloNoise& rng) const {
        // 空の集合は渡さない。
        return {particles[rng.uniform_int(particles.size())]};
    }

    // 0からcapacityまでの候補。
    auto actions(const Info&) const {
        return std::views::iota(0, capacity + 1);
    }

    // 意思決定だけを記録する。
    void step(State&, Info& info, int quantity, MonteCarloNoise&) const {
        info.quantity = quantity;
    }

    // 選び終わったことはInfoで分かる。
    bool terminal(const Info& info) const {
        return info.quantity >= 0;
    }

    // 真の需要と既に決めた数量を使って採点する。
    double finish_value(const State& state, const Info& info) const {
        return 5.0 * std::min(state.demand, info.quantity) - 2.0 * info.quantity;
    }
};

int main() {
    int n, capacity, trials;

    if (!(std::cin >> n >> capacity >> trials)) {
        return 0;
    }

    assert(n > 0 && 0 <= capacity && capacity <= 20 && trials > 0);
    std::vector<int> particles(n);
    for (int& x : particles) {
        std::cin >> x;
    }

    int threshold, high;
    std::cin >> threshold >> high;

    // vectorオブジェクトはModelとplannerより長く保持する。
    Model model{particles, capacity};

    // Modelへの参照を保持する。
    MonteCarloPlanner planner(model);

    // 両回で同じ試行数を使う。
    decltype(planner)::Limits limits;

    // 一回の追加探索予算。
    limits.max_simulations = trials;

    // 観測前の粒子集合で開始する。
    planner.start(Model::Info{}, 1);

    // 試行中はparticlesを変更しない。
    planner.search(limits);

    // 観測前の推薦。
    std::cout << "prior_order=" << *planner.best_action() << '\n';
    std::erase_if(particles, [&](int d) {
        return (d >= threshold) != bool(high);
    });
    if (particles.empty()) {
        std::cout << "posterior=empty\n";
        return 0;
    }

    // 推定分布が変わったので、以前の統計と未完了状態を捨てる。
    planner.start(Model::Info{}, 1);

    // 絞り込まれた同じvectorを参照して再計画する。
    planner.search(limits);

    // 観測後の推薦。
    std::cout << "posterior_order=" << *planner.best_action()
              << " particles=" << particles.size() << '\n';

    // 真の需要を見て数量を選び直す採点にしない。finish_valueはInfoに記録済みの数量を採点する。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
prior_order=3
posterior_order=4 particles=4
```

`prior_order` は観測前の仕入れ量、`posterior_order` は実観測で絞り込んだ後の仕入れ量です。`particles` は残った需要候補の数です。入力と矛盾して一つも残らなければ、`posterior=empty` を出し、空集合からサンプルしません。

### 例18：隠れた被害の仮定と、既に決めた保険の有無で採点する

災害の有無がまだ分からない時点で保険へ入るかを決めます。保険に入れば保険料を払い、入らなければ災害時に被害額を払います。決定後は確認処理を経て、3手目で精算する問題です。
途中で「被害があると仮定したState」を採点に使うのは構いません。ただし、見えないStateを見て保険の有無を選び直してはいけません。Infoに記録済みの決定を採点します。最後まで進める形と、一手後に残り価値を返す形を同じコードで実行します。

**入力形式**

`災害確率 被害額 保険料 trials`。確率0〜1、被害額・保険料は非負の有限値、試行数は正です。行動0は保険なし、1は加入です。

**入力例**

```text
0.3 20 4 3000
```

**実行できるコード：`examples/18_hidden_state_value.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// StateとInfoの両方を使う採点関数の例。
struct Model {
    // 試行で仮定した災害の有無。方策には渡さない。
    struct State {
        bool damaged;
    };

    // 手続きの進捗と既に決めた加入の有無。
    struct Info {
        int phase = 0, insured = -1;
    };

    // 最初は0/1、その後の2は手続きを進めるだけ。
    using Action = int;

    double probability, loss, premium;

    // 真の仮定を一つ作る。
    State sample_root(const Info&, MonteCarloNoise& rng) const {
        return {rng.uniform01() < probability};
    }

    // 候補は観測済みInfoだけで決まる。
    std::vector<int> actions(const Info& info) const {
        return info.phase == 0 ? std::vector<int>{0, 1} : std::vector<int>{2};
    }

    // 一手報酬を返さず、精算を最後にまとめる。
    void step(State&, Info& info, int action, MonteCarloNoise&) const {
        if (info.phase == 0) {
            info.insured = action;
        }

        ++info.phase;
    }

    // 手続き完了は観測可能な条件。
    bool terminal(const Info& info) const {
        return info.phase == 3;
    }

    // 打ち切り時の両状態を読む。
    double leaf_value(const State& state, const Info& info, int) const {
        return score(state, info);
    }

    // 終了理由も受け取れる形式。
    double finish_value(const State& state, const Info& info, MonteCarloFinishReason) const {
        // この問題では終了理由によって精算額を変えない。
        return score(state, info);
    }

private:

    double score(const State& state, const Info& info) const {
        return info.insured ? -premium : (state.damaged ? -loss : 0);
    }
};

int main() {
    double probability, loss, premium;
    int trials;

    if (!(std::cin >> probability >> loss >> premium >> trials)) {
        return 0;
    }

    assert(0 <= probability && probability <= 1 && loss >= 0 && premium >= 0 && trials > 0);

    // どちらの探索でも同じ問題を使う。
    Model model{probability, loss, premium};

    // 部分観測でも、採点だけはconst Stateとconst Infoを読める。
    using Planner = MonteCarloPlanner<Model>;
    for (int depth : {0, 1}) {
        // 比較用の探索器を別々に作る。
        Planner::Options options;

        // 0は3手完走、1は最初の決定後にleaf_valueで精算相当の価値を返す。
        options.simulation_depth = depth;

        // 根候補の同じ第k試行で災害の乱数を共有する。評価回数のルールは変えない。
        options.use_crn = true;

        // 同じseed・同じ操作列で開始する。
        Planner planner(model, options);

        // 決定と2回の確認処理を合わせて最大3手。
        planner.start(Model::Info{}, 3);

        // 完了試行数の予算。
        Planner::Limits limits;

        // 両方で同じ回数を指定する。
        limits.max_simulations = trials;

        // 終端ならfinish_value、浅い打ち切りならleaf_value。
        const auto run = planner.search(limits);

        // 推薦の保険加入有無。
        // 深さ0は3倍、深さ1は1倍のstep数になる。
        std::cout << "depth=" << depth
                  << " insured=" << *planner.best_action()
                  << " transitions=" << run.transitions << '\n';
    }
    // score内でStateを見てinsuredを再選択すると、未来が見える不正な方策の評価になってしまう。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
depth=0 insured=1 transitions=9000
depth=1 insured=1 transitions=3000
```

`insured=1` は保険に加入、0は加入しないという推薦です。`depth=0` は3手進め、`depth=1` は一手で残りの精算額を返すため、`transitions` はそれぞれ試行数の3倍と1倍です。この例では保険の決定後に新たな意思決定や外乱がなく、同じ精算を浅く返せます。

### 例19：指定RNGで、部分観測・候補提案・継続行動をそろえる

土壌の水分が正確には見えない状態で、数日間の給水量を考えます。初期水分は推定値の前後2、毎日の蒸発量は0〜2とします。最後の水分を目標へ近づけ、給水量も抑えます。
`FastRng`を指定し、初期状態のサンプル、給水量の提案、継続行動、状態遷移のすべてで同じRNG型を受け取ります。追加contextを受け取る形式も示しますが、この例は出来事キーを使わず、渡されたrngだけで乱数を作ります。

**入力形式**

`推定水分 目標水分 一日の給水上限 日数 trials CRNフラグ seed`。給水上限・日数・試行数は正、日数は20以下、フラグ0/1です。同梱の`fast_rng.hpp`も必要です。

**入力例**

```text
3 8 4 4 3000 0 42
```

**実行できるコード：`examples/19_custom_rng.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// この例で指定するURBG。コードと同じ場所へ同梱ヘッダを置く。
#include "fast_rng.hpp"

// 指定RNGをすべての乱数付きコールバックへ渡す部分観測モデル。
struct Model {
    // 仮定した真の水分。
    struct State {
        int moisture;
    };

    // 見えているのは日数と累計給水量。
    struct Info {
        int turn = 0, supplied = 0;
    };

    // 今日の給水量。
    using Action = int;

    int estimate, target, max_water;

    // context付き形式。ここではcontextを使わないため引数名を省略。
    State sample_root(const Info&, FastRng& rng, const MonteCarloNoise&) const {
        // ユーザー指定のFastRngで初期状態をサンプルする。
        return {estimate - 2 + rng.uniform(5)};
    }

    // 提案にもFastRngを受け取る。
    std::optional<int> propose_action(
        const Info&,
        int,
        int index,
        FastRng& rng,
        const MonteCarloNoise&
    ) const {
        // 保存用の提案は12回まで。重複した値も別候補になる。
        if (index >= 12) {
            return std::nullopt;
        }

        // 0〜上限の合法量を提案する。
        return index == 0 ? 0 : rng.uniform(max_water + 1);
    }

    // 木の外の継続方策にも同じ型。
    int rollout_action(const Info&, int, FastRng& rng, const MonteCarloNoise&) const {
        // 将来の給水量を選ぶ。
        return rng.uniform(max_water + 1);
    }

    // 遷移にも同じ型。
    void step(State& state, Info& info, int water, FastRng& rng, const MonteCarloNoise&) const {
        // 0〜2の蒸発量。
        state.moisture += water - rng.uniform(3);
        ++info.turn;
        info.supplied += water;
    }

    // 採点はRNGを使わず、到達状態を読む。
    double finish_value(const State& state, const Info& info) const {
        return -std::abs(state.moisture - target) - 0.2 * info.supplied;
    }
};

int main() {
    int estimate, target, max_water, days, trials, crn;
    uint64_t seed;

    if (!(std::cin >> estimate >> target >> max_water >> days >> trials >> crn >> seed)) {
        return 0;
    }

    assert(max_water > 0 && 1 <= days && days <= 20 && trials > 0 && (crn == 0 || crn == 1));

    // 問題データは共有し、試行中に変更しない。
    Model model{estimate, target, max_water};

    // 第2テンプレート引数でRNGを指定する。
    using Planner = MonteCarloPlanner<Model, FastRng>;

    // 型とseedの指定は別の設定。
    Planner::Options options;

    // 根の乱数系列を決める。
    options.seed = seed;

    // ONは対応する候補間で乱数共有、OFFは一つのRNGを連続使用。
    options.use_crn = crn;

    // Rngはuint64_tのseedから構築できるURBGであることが条件。
    Planner planner(model, options);

    // 観測可能な根Infoから開始する。
    planner.start(Model::Info{}, days);

    // 乱数型によらず同じ予算API。
    Planner::Limits limits;

    // CRNの指定により、この上限や配分ルールは自動変更されない。
    limits.max_simulations = trials;

    // sample_root・提案・rollout・stepへFastRngが渡される。
    planner.search(limits);

    // 今日の給水量。
    std::cout << "water=" << *planner.best_action() << '\n';

    // contextやrngの参照はコールバック中だけ有効。後で使うために参照を保存しない。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
water=2
```

`water` は今日の給水量です。隠れた水分の仮定・候補の提案・将来の給水・蒸発の乱数がすべて `FastRng&` を通じて作られます。`FastRng::uniform(n)` は0以上n未満の整数を返す、この補助RNGのAPIです。

この例では水分は目標との差を表す簡略な指標で、負になることも許しています。現実の水量に合わせて0で打ち切るなど遷移を変える場合は、採点や推定分布も同じ意味にそろえます。

### 例20：同じ出来事の下で、二つの生産ラインを比較する

最初に生産ラインを選び、そのラインで何日か生産します。生産量には共通の変動があり、途中の運転モードもランダムに選びます。
CRNをONにすると、根の各候補の第k試行同士で、同じ残り手数・同じ用途の乱数系列を共有します。遷移だけでなく、継続行動用の乱数にも対応します。ON/OFFの両方を同じ完了試行予算で動かし、候補統計を表示します。共有によって観測得点が変われば、候補への適応的な評価配分も変わり得ます。

**入力形式**

`日数 ライン0の基準生産量 ライン1の基準生産量 trials seed`。日数1〜20、試行数は正、基準値は有限値です。

**入力例**

```text
4 5 5.05 2000 42
```

**実行できるコード：`examples/20_crn_compare.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 最初にラインを決め、その後は運転モードを変える。
struct Model {
    // ラインは試行の最初に決まる。
    struct State {
        int turn = 0, line = -1;
    };

    // 根はライン0/1、後続では運転モード0/1。
    using Action = int;

    std::array<double, 2> base;

    // 根の候補。
    std::array<int, 2> actions(const State&) const {
        return {0, 1};
    }

    // 継続行動用RNGもCRNの対象。
    int rollout_action(const State&, int, MonteCarloNoise& rng) const {
        return int(rng.uniform_int(2));
    }

    // 遷移用RNGは継続行動用と用途が異なる。
    double step(State& s, int action, MonteCarloNoise& rng) const {
        bool first = s.turn == 0;
        if (first) {
            s.line = action;
        }

        ++s.turn;
        // 生産量の変動を発生させる。
        return base[s.line] + (first ? 0 : 0.1 * action) + rng.uniform01() - 0.5;
    }
};

int main() {
    int days, trials;
    double base0, base1;
    uint64_t seed;

    if (!(std::cin >> days >> base0 >> base1 >> trials >> seed)) {
        return 0;
    }

    assert(1 <= days && days <= 20 && trials > 0);

    // 両実行で同じ問題を使う。
    Model model{{base0, base1}};

    // 既定RNGでもCRNを切り替えられる。
    using Planner = MonteCarloPlanner<Model>;
    for (bool crn : {false, true}) {
        // ON/OFFごとに別の探索器を作る。
        Planner::Options options;

        // 同じseedから同じ根の世代で始める。
        options.seed = seed;

        // 変更するのは乱数共有の有無だけ。
        options.use_crn = crn;

        // 設定は構築時にコピーされる。
        Planner planner(model, options);

        // 同じ初期状態と手数を与える。
        planner.start(Model::State{}, days);

        // 両方で同じ予算を設定する。
        Planner::Limits limits;

        // 合計の完了試行数。候補ごとの回数をそろえる設定ではない。
        limits.max_simulations = trials;

        // CRN ONでも追い付き評価や追加評価は行わない。
        const auto run = planner.search(limits);

        // 各候補の回数・平均を読む。
        const auto result = planner.result();

        // 実行条件と推薦。
        std::cout << "crn=" << crn
                  << " completed=" << run.simulations
                  << " line=" << *result.action << '\n';
        // 回数が違えば平均の比較対象となる試行数も違う。
        for (const auto& candidate : result.candidates) {
            std::cout << candidate << '\n';
        }
    }
    // 同じ対応位置でも入力状態や候補集合が異なれば行動・得点まで同じとは限らない。
    // コールバック内で別の共有RNGや、状態を保持する乱数分布を使うとCRNの対応を崩し得る。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
crn=0 completed=2000 line=1
Candidate{id=1:0, trials=89, mean=20.0944}
Candidate{id=1:1, trials=1911, mean=20.3612}
crn=1 completed=2000 line=1
Candidate{id=1:0, trials=148, mean=20.1573}
Candidate{id=1:1, trials=1852, mean=20.3568}
```

`crn=0` が共有なし、1が共有ありです。どちらも `completed` は同じ完了試行予算になります。各候補の `trials` が両方で一致しなくても異常ではありません。平均が変われば、同じ規則による評価配分も変わり得ます。この出力だけからCRNの優劣を決めることはできません。

### 例21：修理の分岐があっても、各日の天候を候補間で対応させる

機械を一台選び、何日かの運転費用を比べます。どの機械にも同じ日の天候を与えたい一方で、機械ごとに故障確率が異なり、故障したときだけ修理日数の乱数を引きます。
乱数を一本の列から順に取るだけだと、修理の有無で翌日の天候に使う位置がずれます。この例では「何日目・天候」「何日目・故障」「何日目・修理」をキーとチャネルで分けます。目的は、CRNで比較する候補へ対応する出来事を与え、運の差を抑えることです。

**入力形式**

`機械数 日数 time_us CRNフラグ seed`、続く機械数行に `基本日額 故障確率 修理一日あたり費用`。機械数は1〜131072、日数は正、時間は0以上、CRNフラグは0/1、確率0〜1、費用は非負です。`fast_rng.hpp`も必要です。

**入力例**

```text
3 8 3000 1 42
10 0.1 3
7 0.3 4
12 0.01 2
```

**実行できるコード：`examples/21_event_keys.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// helperにもユーザー指定RNGを利用できる。
#include "fast_rng.hpp"

int main() {
    int n, days, crn;
    int64_t time_us;
    uint64_t seed;

    if (!(std::cin >> n >> days >> time_us >> crn >> seed)) {
        return 0;
    }

    assert(n > 0 && days > 0 && time_us >= 0 && (crn == 0 || crn == 1));
    struct Machine {
        double daily, failure, repair;
    };

    std::vector<Machine> machines(n);
    for (auto& m : machines) {
        std::cin >> m.daily >> m.failure >> m.repair;
    }

    std::vector<int> candidates(n);
    std::iota(candidates.begin(), candidates.end(), 0);

    // 第3引数は試行に対応するcontext。ここでは順次RNGを使わない。
    auto evaluate = [&](int id, FastRng&, const MonteCarloNoise& context) {
        double cost = 0;
        for (int day = 0; day < days; ++day) {
            // key=日、channel=天候、draw_index=最初の値。
            double weather = context.uniform01(day, 0, 0);
            cost += machines[id].daily * (1 + 0.2 * weather);
            // 同じ日の故障専用系列。RNG型も指定できる。
            auto failure = context.stream<FastRng>(day, 1);
            // 候補ごとの確率は違っても、判定用の一様乱数は共有できる。
            if (failure.uniform() < machines[id].failure) {
                // 既定RNGによる修理専用系列を、この分岐内だけで作る。
                auto repair = context.stream(day, 2);
                // 修理だけ追加で乱数を使っても、翌日の天候キーには影響しない。
                int repair_days = 1 + int(repair.uniform_int(5));
                cost += machines[id].repair * repair_days;
            }
        }

        // 期間全体の費用を小さくする。
        return -cost;
    };

    // ONのとき候補の第k試行同士でcontextを共有する。
    auto selected = mc_choose<FastRng>(candidates, evaluate, time_us, crn, seed);
    // 推薦された機械番号。
    if (selected) {
        std::cout << "machine=" << *selected << '\n';
    } else {
        // 空候補を扱う場合の分岐。
        std::cout << "machine=none\n";
    }
    // keyに機械番号を入れると候補間で天候を共有できない。同じ出来事には同じkey/channelを使う。
    // OFFでは候補間の共有を行わないが、渡されたcontext内の出来事指定は同じように使える。
}
```

**出力例と読み方**

実行時の出力例です。時間に関係する値は実行環境で変わります。

```text
machine=1
```

`machine` は選んだ機械番号です。コードでは、修理の乱数を引く場合だけ別のstreamを作り、翌日の天候との対応を保ちます。日数分のループ全体が一つの評価コールバックなので、その内部で時間制限による中断はできません。

評価関数内のdays日間は、ライブラリからは一回の評価呼び出しです。その途中で時間切れを強制することはできません。

天候の `channel=0`、故障の1、修理の2は、利用者が決めた番号です。ライブラリがこれらの数値へ特別な意味を割り当てているわけではありません。`FastRng::uniform()` は0以上1未満の実数を返します。

### 例22：探索とは別に、同じ出来事を再現する乱数系列を作る

シミュレータの検証で、ある日の天候を何度でも同じ値として取り出したい場面です。実環境用の乱数は、探索中にどれだけ乱数を使ったかの影響を受けないよう、外側で別に持ちます。
通常の連続した乱数と、出来事キーから再現する乱数の両方を使います。既存シミュレータが特別なseedの組み立てを要求する場合に、factory付きstreamで生成方法を渡す例も含めます。

**入力形式**

`seed 日のキー 補助seed`。いずれも非負整数、補助seedは32bitの範囲です。

**入力例**

```text
42 7 123
```

**実行できるコード：`examples/22_scenario_noise.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

int main() {
    uint64_t seed, day;
    uint32_t salt;
    if (!(std::cin >> seed >> day >> salt)) {
        return 0;
    }

    // 探索器とは独立した、外側のシナリオ用オブジェクト。
    MonteCarloNoise scenario(seed);
    // channelを省略すると0。同じseed・keyなら同じ系列。
    auto weather = scenario.stream(day);
    // この出来事系列の最初の0以上1未満の値。
    double first_weather = weather.uniform01();
    // channel=0を明示して同じ出来事を再現する。
    auto same_weather = scenario.stream(day, 0);
    // stream生成は親の通常系列を進めない。
    assert(first_weather == same_weather.uniform01());
    // 同じ出来事の3番目の一様実数を直接取り出す。
    double component = scenario.uniform01(day, 0, 2);
    // URBGとして64bitの乱数を一つ引く。
    MonteCarloNoise::result_type raw = scenario();

    std::cout << "raw=" << raw << '\n';

    // URBGの両端。最大値も生成範囲に含む。
    std::cout << "range=" << MonteCarloNoise::min() << ',' << MonteCarloNoise::max() << '\n';

    // 現在位置ではなく、このオブジェクトを作ったseed。
    std::cout << "seed=" << scenario.scenario_seed() << '\n';

    // [0,6)に1を足して1〜6のサイコロにする。
    std::cout << "dice=" << 1 + scenario.uniform_int(6) << '\n';

    std::cout << std::setprecision(12)
              << "weather=" << first_weather
              << " component=" << component << '\n';
    // 別のURBG型へ、混合済みseedを渡して構築する。
    auto compact = scenario.stream<std::minstd_rand>(day, 3);

    std::cout << "compact=" << compact() << '\n';
    // streamがこの関数へ渡すのは、key/channelまで混ぜたseed。
    auto factory = [salt](uint64_t mixed_seed) {
        std::seed_seq sequence{uint32_t(mixed_seed), uint32_t(mixed_seed >> 32), salt};
        return std::mt19937_64(sequence);
    };

    // 特別な初期化が必要な乱数器は生成関数で受け取る。
    auto external = scenario.stream(day, 4, factory);

    std::cout << "external=" << external() << '\n';
    // 通常系列でrawやdiceを引いた後でも、同じ天候系列を再現できる。
    auto replay = scenario.stream(day);

    // 同じ出来事の再現を確認する。
    std::cout << "replayed=" << (replay.uniform01() == first_weather) << '\n';

    // キーの意味は利用者が決める。意図的に共有する出来事以外へ同じkey/channelを流用しない。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
raw=13679457532755275413
range=0,18446744073709551615
seed=42
dice=2
weather=0.0669054785714 component=0.57392495122
compact=1376741533
external=9719836417291474150
replayed=1
```

`raw` は通常の64bit乱数、`range` はその生成範囲、`seed` は構築seedです。`dice` は1〜6、`weather` はその日の最初の天候値、`component` は同じ出来事の3番目の一様実数です。`compact` と `external` は別型のRNGの整数出力です。`replayed=1` は、通常乱数を途中で引いても同じキーの天候を再現できたことを示します。

### 例23：空候補・終了済み・予算ゼロを区別して、結果を安全に表示する

候補番号が大きいほど基準得点が高い、一手または複数手の判断を題材にします。実運用では、候補がまだない、既に終了している、予算がない、などの状態も起こります。
結果の有無と暫定フラグを確認してから実行します。候補数25・予算ゼロの入力例では、候補統計の表示が先頭20件で打ち切られ、省略件数が出ることも確認できます。

**入力形式**

`候補数 手数 trials 終了済みフラグ`。候補数0〜100、手数・試行数0以上、終了済みフラグ0/1です。

**入力例**

```text
25 1 0 0
```

**実行できるコード：`examples/23_result_states.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// 結果の状態を確認する小さなモデル。
struct Model {
    // 実際に終了済みかを表す。
    struct State {
        bool closed = false;
    };

    // 行動番号。
    using Action = int;

    // 呼び出し回数によらず同じ入力に同じ値を返す。
    bool terminal(const State& s) const {
        return s.closed;
    }

    // 一手分の得点。
    double step(State&, int action, MonteCarloNoise& rng) const {
        return action + rng.uniform01();
    }

    // 手数が2以上でも合法な継続行動を用意する。
    int rollout_action(const State&, int, MonteCarloNoise&) const {
        return 0;
    }
};

int main() {
    int n, horizon, trials, closed;

    if (!(std::cin >> n >> horizon >> trials >> closed)) {
        return 0;
    }

    assert(0 <= n && n <= 100 && horizon >= 0 && trials >= 0 && (closed == 0 || closed == 1));

    // plannerより長く保持する。
    Model model;

    // 列挙型や結果型はこのPlannerに属する。
    using Planner = MonteCarloPlanner<Model>;

    // 構築だけでは根がなく、結果はnot_ready。
    Planner planner(model);

    // Result全体を直接出力できる。
    std::cout << "before_start=" << planner.result() << '\n';
    std::vector<int> candidates(n);
    std::iota(candidates.begin(), candidates.end(), 0);

    // 空候補や手数0も表現できる。
    planner.start(Model::State{bool(closed)}, horizon, candidates);

    // 有効な停止条件を必ず一つ設定する。
    Planner::Limits limits;

    // 0なら試行せず、stop=simulationsで止まる。
    limits.max_simulations = trials;

    // searchはstart後に呼ぶ。未startはnot_readyを返すのでなく契約違反。
    const auto run = planner.search(limits);

    // 推薦・候補統計・直前の実行統計を取得する。
    const auto result = planner.result();

    // RunStatsとResultはconst参照のoperator<<に対応。
    std::cout << run << '\n' << result << '\n';

    // readyでも未評価の可能性がある。
    if (result.status == Planner::Status::ready && result.action) {
        // IDは行動値とは別。
        std::cout << "execute=" << *result.action
                  << " id=" << result.id
                  << " provisional=" << result.provisional << '\n';
    } else {
        // 終端・候補なしではactionを参照しない。
        std::cout << "execute=none\n";
    }

    // Candidateも直接出力できる。
    if (!result.candidates.empty()) {
        std::cout << "first=" << result.candidates.front() << '\n';
    }
    // 表示を20件で省略してもresult.candidates内のデータは全件残る。
    // AHCの本番では上の診断出力をcerrへ送り、提出形式のstdoutと分けると扱いやすい。
}
```

**出力例と読み方**

実行時の出力例です。時間に関係する値は実行環境で変わります。

```text
before_start=Result{status=not_ready, action=none, id=0:-1, provisional=false, candidates(0)=[], last_run=RunStats{started=0, simulations=0, transitions=0, elapsed_us=0, stop=none}}
RunStats{started=0, simulations=0, transitions=0, elapsed_us=0, stop=simulations}
Result{status=ready, action=0, id=1:0, provisional=true, candidates(25)=[Candidate{id=1:0, trials=0, mean=unevaluated}, Candidate{id=1:1, trials=0, mean=unevaluated}, Candidate{id=1:2, trials=0, mean=unevaluated}, Candidate{id=1:3, trials=0, mean=unevaluated}, Candidate{id=1:4, trials=0, mean=unevaluated}, Candidate{id=1:5, trials=0, mean=unevaluated}, Candidate{id=1:6, trials=0, mean=unevaluated}, Candidate{id=1:7, trials=0, mean=unevaluated}, Candidate{id=1:8, trials=0, mean=unevaluated}, Candidate{id=1:9, trials=0, mean=unevaluated}, Candidate{id=1:10, trials=0, mean=unevaluated}, Candidate{id=1:11, trials=0, mean=unevaluated}, Candidate{id=1:12, trials=0, mean=unevaluated}, Candidate{id=1:13, trials=0, mean=unevaluated}, Candidate{id=1:14, trials=0, mean=unevaluated}, Candidate{id=1:15, trials=0, mean=unevaluated}, Candidate{id=1:16, trials=0, mean=unevaluated}, Candidate{id=1:17, trials=0, mean=unevaluated}, Candidate{id=1:18, trials=0, mean=unevaluated}, Candidate{id=1:19, trials=0, mean=unevaluated}, ... (+5)], last_run=RunStats{started=0, simulations=0, transitions=0, elapsed_us=0, stop=simulations}}
execute=0 id=1:0 provisional=1
first=Candidate{id=1:0, trials=0, mean=unevaluated}
```

開始前は `not_ready` です。入力例は候補25個・試行予算0なので、開始後は行動0を返せる `ready` ですが、`provisional=true`、全候補の平均は `unevaluated` です。表示の `... (+5)` は25件中5件を省略した意味で、格納データが20件へ減ったわけではありません。

### 例24：負荷に左右されない、時間切れ分岐のテストを書く

実時間でテストすると、他の処理の負荷によって時間切れの位置が変わります。そこで、時計を読むたびに1マイクロ秒だけ進むテスト用時計を渡します。
これは期限切れや暫定結果の扱いを再現するためのユースケースです。出力のelapsed_usは人工的な時間であり、solverの実際の速度を測る値ではありません。

**入力形式**

`人工時計での予算_us`。0〜1000の整数です。

**入力例**

```text
30
```

**実行できるコード：`examples/24_test_clock.cpp`**

```cpp
// 計画・乱数・結果出力のAPIを読み込む。
#include "monte_carlo_planner_v08.hpp"

// Plannerの第3テンプレート引数へ渡す、テスト専用の時計。
struct TestClock {
    using rep = int64_t;
    using period = std::micro;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<TestClock>;
    static constexpr bool is_steady = true;
    inline static rep ticks = 0;

    // 呼び出しごとに人工的に1us進める。
    static time_point now() {
        return time_point(duration(ticks++));
    }
};

// 結果が追いやすい確定的な一手問題。
struct Model {
    // 状態は不要。
    struct State {};

    // 候補番号がそのまま得点になる。
    using Action = int;

    // ランダムな変動は入れない。
    double step(State&, int action, MonteCarloNoise&) const {
        return action;
    }
};

int main() {
    int budget;

    if (!(std::cin >> budget)) {
        return 0;
    }

    assert(0 <= budget && budget <= 1000);

    // テスト用モデルを保持する。
    Model model;

    // RNGは既定型、時計だけ差し替える。
    using Planner = MonteCarloPlanner<Model, MonteCarloNoise, TestClock>;

    // 時計確認の間隔を一定にする。
    Planner::Options options;

    // 判定の分岐を小さい人工時間で再現しやすくする。
    options.clock_interval = 1;

    // カスタム時計型を持つ探索器。
    Planner planner(model, options);

    // ゼロ予算でも暫定推薦を返せるよう根候補を登録。
    planner.start(Model::State{}, 1, std::array{0, 1, 2});

    // deadlineもTestClockのtime_point型になる。
    Planner::Limits limits;

    // 同じ時計の絶対締切を設定する。
    limits.deadline = TestClock::now() + TestClock::duration(budget);

    // 内部の時間判定もTestClockを使用する。
    const auto run = planner.search(limits);
    // 他の予算を設定していないので時間で停止する。
    assert(run.stop == Planner::Stop::time);

    // 人工時計での開始数・完了数・経過時間を確認する。
    std::cout << run << '\n';

    // 期限切れ時の推薦と暫定フラグを表示する。
    std::cout << planner.result(false) << '\n';

    // 本番では時計の型引数を省略し、既定のsteady_clockで実時間を測る。
}
```

**出力例と読み方**

入力例を実行した出力です。

```text
RunStats{started=14, simulations=14, transitions=14, elapsed_us=30, stop=time}
Result{status=ready, action=2, id=1:2, provisional=false, candidates(0)=[], last_run=RunStats{started=14, simulations=14, transitions=14, elapsed_us=30, stop=time}}
```

`elapsed_us` は人工時計の値です。この入力では時間停止まで14試行を完了しています。0予算の入力では試行せず、先頭候補が暫定になります。実際の1秒あたりの探索性能を示す出力ではありません。
