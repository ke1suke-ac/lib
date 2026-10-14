# McmcEstimator サンプルコード集

対象ヘッダは `mcmc_estimator_v10.hpp`、言語は C++20 です。入力と観測から未知の値を推定する、小さなプログラムを30本掲載します。各例は独立しており、別の例の型定義や関数を先にコピーする必要はありません。

まず3.01を動かし、その後はユースケース一覧から目的に近い例を選んでください。第2章は辞書として引けるよう、通常の使い方に必要なAPIから、細かい制御のためのAPIへ並べています。`S` は説明用の別名 `McmcEstimator<>`、`State` は推定対象の状態型を意味します。

付属ZIPでは、ヘッダ、各例の `.cpp`、入力ファイル、実行確認用スクリプトを一つのフォルダにまとめています。そのフォルダで、例えば次を実行します。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I . examples/mcmc_sample_01_affine3_v01.cpp -o demo
./demo < data/01.in
```

本文のC++コードだけを `demo.cpp` に保存する場合は、同じ場所にヘッダを置き、`g++ -std=c++20 -O2 demo.cpp -o demo` でビルドします。ヘッダは `bits/stdc++.h` を使うため、GCCとlibstdc++を用意してください。入力は各例に記載した範囲を満たすものを与えます。例の標準出力は推定値、必要な診断は標準エラー出力です。

この文書は明示的な更新指示があるときに更新します。

## 1. 何を解くライブラリか？

### 1.1 入力から結果を計算できるが、式の中の値が分からない

例えば、機械の所要時間が「仕事量×係数＋準備時間」で決まるとします。仕事量と実際の所要時間を何回か測っても、係数と準備時間は直接は見えません。この二つに仮の値を入れ、予測と測定を比べることで、観測に合う値を探します。

本ライブラリに渡すのは、**未知の値を指定すれば結果を計算できる関数**と、**入力・観測値の組**です。式は一次関数に限りません。整数の係数、式の種類、機械の処理速度、順序、混合割合なども扱えます。「どの状態が観測をどれだけ説明するか」を一つの数で採点できることが必要です。

例として、三つの入力を使う式を考えます。

$$f(x,\theta)=a_ {1}x_ {1}+a_ {2}x_ {2}+a_ {3}x_ {3}+b$$

ここで $x=(x_ {1},x_ {2},x_ {3})$ は既知の入力、$\theta=(a_ {1},a_ {2},a_ {3},b)$ は未知の四係数です。$f$ は利用者が書く予測関数です。測定に誤差があれば、観測値と予測値が完全に一致する必要はありません。

### 1.2 目的関数：説明できない分を小さくする

観測を行ごとに評価する基本形は、次の最小化です。

$$\theta^{\ast}\in\arg\min_ {\theta\in\Omega} E(\theta),\qquad E(\theta)=R(\theta)+\sum_ {i=1}^{N}w_ {i}\ell_ {i}(y_ {i},f(x_ {i},\theta);\theta)$$

`arg min` は「値を最小にする未知値の組」、記号 `∈` は「その中から選ぶ」という意味です。大きなΣは、観測1件目からN件目までの罰を全部足す記号です。

| 記号 | 意味 |
| --- | --- |
| $\theta$ | 未知の値を一組にしたもの。プログラムでは `State`。実数だけでも、実数と整数の混在でもよい。 |
| $\Omega$ | 許す状態の集合。係数の上下限、整数条件、合計1などの制約を含む。 |
| $\theta^{\ast}$ | 目的関数が最小になる状態。複数あることもある。ライブラリの結果は有限の計算予算内で見つけた候補であり、厳密な最小値の保証ではない。 |
| $E(\theta)$ | 状態の総合的な悪さを表す数。小さいほど観測や事前知識によく合う。APIでは `energy` と呼ぶ。 |
| $R(\theta)$ | 観測以外の事前知識や罰則。例えば「係数は2に近い」を表す。不要なら0。観測全体に一度だけ加える。 |
| $N$ | 観測の件数。 |
| $i$ | 観測の番号。式では1〜N、C++の配列では通常0〜N−1。 |
| $x_ {i}$ | i件目の既知の入力。数、配列、経路、仕事列など。 |
| $y_ {i}$ | i件目の実際の観測。実数、成功・失敗、件数、多成分の値など。 |
| $f(x_ {i},\theta)$ | 仮の未知値を使って計算した予測。 |
| $w_ {i}$ | 観測の重み。0以上の有限値。通常は1。0ならその観測の損失を加えない。 |
| $\ell_ {i}$ | 予測と観測のずれを罰に変える関数。末尾の $\theta$ は、誤差の大きさも未知数にする場合などに使う。 |

標準の `Gaussian` では、各測定の誤差を、平均0・標準偏差 $\sigma_ {i}$ の正規誤差として扱います。未知値を一組に決めたとき、各測定の誤差は互いに影響しないと考えます。標準偏差は「普段どのくらいの幅でずれるか」を表す正の数です。このとき、状態に依存しない定数を省いた目的関数は次の形です。

$$E(\theta)=R(\theta)+\sum_ {i=1}^{N}w_ {i}\left(\frac{(y_ {i}-f(x_ {i},\theta))^{2}}{2\sigma_ {i}^{2}}+\log\sigma_ {i}\right)$$

追加の記号 $\sigma_ {i}>0$ が誤差の標準偏差、$\log$ は自然対数です。「観測−予測」を残差と呼びます。残差を標準偏差で割ることで、単位や測定精度が違っても、どのくらい大きなずれかを比べられます。すべての重みと標準偏差が同じ固定値で、事前項がなければ、残差の二乗和が最小になる係数を探すことと同じです。

標準偏差そのものを推定するときは `log sigma` を省けません。これを省くと、係数が合わなくても標準偏差を際限なく大きくするほど有利になってしまいます。

行どうしが独立でない場合や、全体シミュレーションでしか採点できない場合は、行ごとの形に分けず、利用者が `E(state)` 全体を返して構いません。制約違反には正の無限大を返せます。有限の負の値は許されますが、NaNと負の無限大は許されません。観測数による自動の割り算はありません。

### 1.3 最もよく合う一組と、まだあり得る値の広がり

`solve` では、最も低い目的関数値が見つかった状態を受け取ります。一方、`sample` では、観測後にもあり得る状態の広がりを調べ、予測の平均や分散を集計できます。この二つは同じ出力ではありません。

確率として解釈するときは、次の重みを考えます。

$$p(\theta\mid D)=\frac{\exp(-E(\theta))}{Z},\qquad \theta\in\Omega$$

ここで $D$ は観測全体、$p(\theta\mid D)$ は観測後の確率または確率密度、$\exp$ は自然対数の逆関数、$Z$ は全体の確率が1になるようにする正の定数です。連続値では確率密度を積分し、整数や種類では確率を足し合わせます。$Z$ が有限になるモデルと範囲が必要です。

損失を「その観測の起こりやすさのマイナス対数」、事前項を「観測前の重みのマイナス対数」にすると、この重みは事後分布を表します。単に運用上のスコアを小さくする目的関数なら、最良候補探索には使えますが、得られる広がりを測定の不確かさとみなせるとは限りません。重みを変更したり、目的関数全体を定数倍したりすると、最小位置が変わらなくても広がりは変わります。

## 2. APIリファレンス

### 2.1 全体像と共通の契約

| API群 | 主な役割 | 最初に見る例 |
| --- | --- | --- |
| `McmcState`, `Parameters`, `Observation` | 未知値と観測の入れ物 | 3.01 |
| `make_observation_model`, 標準損失 | 予測と観測から目的関数を作る | 3.01、3.05、3.09 |
| `make_numeric_session`, `Session` | 数値の推定、継続、観測更新 | 3.01、3.13 |
| `Budget`, `Param`, `MoveParam` | 計算予算と動作設定 | 3.15、3.18 |
| `sample`, `sample_each`, `Moments` | 標本と統計量 | 3.16、3.17 |
| `make_context_model`, `update_model` | 既知の地図・状態の更新 | 3.14 |
| `make_session`, 独自提案 | 数値配列以外の状態 | 3.21〜3.23 |
| `Domain`, `NumericMove`, `Rng`, `MoveContext` | 提案部品の直接利用 | 3.28 |
| `McmcEstimator` 本体の下位API | モデル・更新・実行段階を外部管理 | 3.25〜3.27 |
| 結果型の `operator<<` | 見やすい診断出力 | 3.29 |

APIは `McmcEstimator<State>` に属します。ただし、`Result`、`Moments<Value>`、`SampleResult<Value>` は **`Session<Model,Move>` の中の型**です。結果は通常 `auto` で受け取れば十分です。標準の `McmcState` だけはトップレベルの型です。以下では公開の型、設定項目、明示的なコンストラクタ、メソッド、出力演算子を列挙します。言語が自動生成するコピー・ムーブ操作は個別に列挙しません。

共通の前提は次のとおりです。

- 状態はコピー可能で、実行途中に形・次元・各変数の意味を変えません。
- 同じ状態・同じモデルは同じ値を返します。毎回変わる乱数や、外部への副作用を評価関数へ入れません。
- 評価値は有限の `double` または `+INFINITY`。各中間計算も有限範囲に収めます。`-ffast-math` は使いません。
- 単一スレッドで使います。実行コールバック内から同じSessionを更新・再実行しません。
- `vector`、関数オブジェクト、Contextは値として所有します。ただし、ラムダが参照で捕まえた先まで所有するわけではありません。
- 入力違反は主に `assert` で検出します。例外を返す入力検証APIではありません。リリースビルドでも呼び出し側が条件を満たす必要があります。

### 2.2 状態と変数の宣言

**`McmcState`** は既定の状態型です。

| メンバー | 型・初期状態 | 意味 |
| --- | --- | --- |
| `real` | `std::vector<double>`、空 | 実数変数。 |
| `discrete` | `std::vector<std::int64_t>`、空 | 整数とカテゴリID。両者は同じ配列に登録される。 |

**`McmcEstimator<State = McmcState>`** は状態型を受け取るクラステンプレートです。独自状態を指定する場合は、通常は独自提案も渡します。`Parameters` と標準数値提案を使うには、標準状態と同じ `real` / `discrete` の構造が必要です。

**`Parameters`** は既定構築で空になります。

| 呼び出し | 引数、条件、既定値 | 戻り値 |
| --- | --- | --- |
| `real(initial, lower, upper, scale = 0)` | 最初の3個は `double` で必須。初期値は有限で `lower <= initial <= upper`。`scale` も `double`。0なら有限な範囲幅の0.1倍を使い、全固定なら1。明示値は正の有限値。 | `std::size_t`。`State::real` 内の添字。 |
| `integer(initial, lower, upper)` | すべて `std::int64_t`、必須。初期値は両端を含む範囲内。整数の絶対値・範囲幅は10億以下を想定。 | `std::size_t`。`State::discrete` 内の添字。 |
| `category(initial, count)` | `initial` は `std::int64_t`、`count` は正の `int`。必須。IDは0〜count−1。整数と同じ幅制約を守る。 | `std::size_t`。`State::discrete` 内の添字。 |
| `initial() const` | 引数なし。 | `const State&`。登録済みの初期値。 |
| `domain() const` | 引数なし。 | `const Domain&`。登録済みの範囲。 |

`lower == upper` は固定変数です。実数に無限の上下限を使うなら、`scale` を正の有限値で明示します。有限上下限のときも、範囲幅や反射計算の中間値が `double` に収まる通常の大きさで使います。`scale` は測定誤差の標準偏差ではなく、探索時にどのくらい値を動かすかの初期幅です。まず省略し、範囲が極端に広いときは自然な変更幅を明示します。

`initial()` / `domain()` の参照は、`Parameters` の破棄や変更をまたいで保持しません。Session構築後にParametersを変更しても、構築済みSessionの設定は変わりません。変数の追加、範囲変更、種類の意味の変更はSessionを作り直します。

### 2.3 観測と損失

**`Observation<Input>`** は利用者の入力型 `Input` を持つ集約型です。

| メンバー | 型 | 既定値・指定条件 |
| --- | --- | --- |
| `input` | `Input` | メンバー初期値の指定なし。利用者が既知の入力を設定する。 |
| `value` | `double` | メンバー初期値の指定なし。利用者が観測を設定する。 |
| `scale` | `double` | 1。`Gaussian` / `Huber` では正の有限値。他の二つの標準損失では使わない。 |
| `weight` | `double` | 1。0以上の有限値。観測項への倍率。 |

`Row{x,y}` なら `scale=1, weight=1` です。一方、初期化しないローカル変数の `value` を読み出してはいけません。重み0でも予測関数は呼ばれるため、無効なIDなどを入れた行を重み0だけで隠すことはできません。

四つの標準損失はいずれも `double operator()(const State&, const O& row, double prediction) const` を提供します。3引数とも必須で、状態は標準損失では参照しません。`O` は必要な `value`、`scale`、`weight` を持つ型です。各型は `static double mean(double prediction)` も提供します。この引数も必須です。

以下では `y=row.value`、`s=row.scale`、`w=row.weight`、`z=prediction` と書きます。

| 型・構築 | `prediction` の意味と条件 | 損失、戻り値 | `mean(z)` |
| --- | --- | --- | --- |
| `Gaussian{}` | 観測量そのものの予測。`s>0`。 | `w * (0.5 * ((y-z)/s)^2 + log(s))`。 | `z` |
| `Huber{threshold}` | 観測量そのものの予測。`s>0`。 | 残差をsで割った値にHuberの罰を与え、`log(s)`を加え、w倍する。 | `z` |
| `BernoulliLogit{}` | 成功確率を変換する前の値。有限。`y` は厳密に0または1。 | `w * (log(1+exp(z)) - y*z)` と等価な、数値的に安定した値。 | 成功確率 `1/(1+exp(-z))` を安定して計算。 |
| `PoissonLogMean{}` | 平均件数の自然対数。有限。`y` は0以上の有限な整数値。 | `w * (exp(z) - y*z)`。`exp(z)`等がオーバーフローしない範囲で使う。 | 平均件数 `exp(z)` |

`Huber::threshold` は `double`、既定値1.345、正の有限値です。絶対値で測った標準化残差を `r`、閾値を `d` とすると、`r<=d` では `r*r/2`、それ以外は `d*(r-d/2)` の罰です。小さい閾値ほど外れた観測の影響を抑えます。まず既定値を使い、通常の誤差の大きさは `scale` で合わせます。閾値は固定の設定として使います。閾値も推定するときは、その値で変わる正規化定数を含めた独自損失が必要です。

**`static double gaussian_loss(double observed, double predicted, double sigma)`** は一測定の `0.5*((observed-predicted)/sigma)^2 + log(sigma)` を返します。3引数とも必須、`sigma>0` かつ有限です。重みは掛かっていないため、必要なら利用側で掛けます。誤差尺度の推定や事前項に使えます。

標準損失は状態に依存しない定数を省いています。負の損失も正常です。異なる観測の定義や省略定数を持つモデル間で、値をそのまま確率として比較しません。

### 2.4 予測関数・事前項・全体評価の作り方

| 関数の役割 | 必要な呼び出し形 | 戻すもの・注意 |
| --- | --- | --- |
| 予測 `predict` | `predict(input, const State&)` | 観測と比較する値。独自損失なら配列や独自型でもよい。 |
| 独自損失 `loss` | `loss(const State&, const Row&, predicted)` | 一行の損失。重みを使うかも自分で実装する。 |
| 事前項 `prior` | `prior(const State&)` | 全体に一度加える損失。 |
| Context付き予測 | `predict(context, input, const State&)` | 既知の地図や設定も使って計算する。 |
| Context付き事前項 | `prior(context, const State&)` | Contextを含めた事前の損失。 |
| 独自の全体評価 `model` | `model(const State&)` | 目的関数全体。相関する観測などをまとめて扱う。 |
| 任意の差分評価 | `model.candidate(const State& old, const State& next, double old_energy)` | **変更後の目的関数全体**。差だけを返さない。引数はすべて必須。状態や共有キャッシュを書き換えない。 |

独自観測行を観測モデルに渡す場合は `input` メンバーを用意します。他のメンバーは損失が理解できる形にします。Context付きでも、損失の引数形は変わりません。Contextは予測の引数に自動的に付きますが、損失に第四引数が付くわけではありません。

**モデルを作るstatic関数**は、いずれも構築済みの `ObservationModel` を値で返します。

| 呼び出し | 引数と既定値 |
| --- | --- |
| `make_observation_model(data, predict, loss = Gaussian{}, prior = ゼロを返す関数)` | `data` は `std::vector<O>`、`predict` は関数オブジェクトで必須。`loss` / `prior` は任意の対応する関数オブジェクト。 |
| `make_context_model(data, context, predict, loss = Gaussian{}, prior = ゼロを返す関数)` | 最初の3引数が必須。`context` の型は任意の値型C。その他は上と同様。 |

`std::move(data)` を渡せば観測列を移せます。通常の `data` を渡せばコピーするため、例3.08のように残差計算へ元の列を再利用できます。

**`ObservationModel<O,P,L,R,C>`** は観測型、予測型、損失型、事前項型、Context型のテンプレートです。Cを省略するとContextなしです。通常はfactoryの戻り値を `auto` で受け取ります。

| 公開API | 引数・戻り値・条件 |
| --- | --- |
| `using observation_type = O` | 扱う観測行の型別名。 |
| コンストラクタ `(std::vector<O> data, P predictor, L loss, R prior, C context = {})` | 最初の4引数は必須。Contextの省略にはCが既定構築可能であることが必要。 |
| `double operator()(const State& state)` | 必須の状態から全損失を計算。メソッド自体は非const。 |
| `std::size_t size() const` | 観測件数。 |
| `std::span<const O> observations() const` | 読み取り専用ビュー。モデル更新・破棄まで有効。 |
| `prediction(const Input& input, const State& state) const` | 2引数必須。生の予測値。戻り値の型は予測関数に従う。constな予測関数として呼べる必要がある。 |
| `mean_prediction(const Input& input, const State& state) const` | 2引数必須。`loss.mean(prediction(...))`。`mean` が呼べる損失にだけ存在する。単なる損失ラムダには通常ない。 |
| `C& context()` / `const C& context() const` | Contextへの参照。Session所有中の変更は `update_model` のコールバック内で行う。 |

### 2.5 計算予算 `Budget` と時計

**`Clock`** は `std::chrono::steady_clock` の別名です。時計を合わせ直しても経過時間が逆行しない時計を使います。

| メンバー/API | 型・既定値・意味 |
| --- | --- |
| `deadline` | `Clock::time_point`。既定値 `Clock::time_point::max()` は期限なし。 |
| `remaining_steps` | `std::uint64_t`。既定値 `UINT64_MAX`。完了できる遷移の残数。 |
| `Budget::for_us(std::int64_t us)` | `us>=0` が必須。**呼んだ時点**からusマイクロ秒の期限を持つBudgetを返す。遷移数上限は既定の最大値。 |
| `Budget::until(Clock::time_point end, std::uint64_t steps = UINT64_MAX)` | endは必須。絶対期限と遷移数上限を持つBudgetを返す。過去の期限なら新しい計算を始めない。 |
| `Budget::for_steps(std::uint64_t steps)` | stepsは必須。期限なしで遷移数だけ制限したBudgetを返す。 |
| `bool time_left() const` | 時刻だけを確認。遷移数残量0でもtrueになり得る。 |
| `bool available() const` | 遷移数が1以上残り、かつ時間も残っているか。 |

「遷移」は一回の候補変更と採否の判断です。棄却も一回に数えます。初期評価やモデル更新後の再評価は遷移数に含めませんが、時間は使います。遷移数0では、新しい初期評価も始めません。ただし、下位APIなどで既に完了した標本の出力だけが保留されている場合は、時間があればその出力が行われることがあります。

Sessionの `solve` / `sample` はBudgetを**値渡し**で受け、呼び出し元の残量を減らしません。下位APIは **`Budget&`** を受け、実際に残量を減らします。時間はコールバックを強制停止する仕組みではなく、期限を確認して止める仕組みです。長い一回の評価やコールバック分は期限を超えることがあります。

### 2.6 実行設定 `Param` と数値提案設定 `MoveParam`

`Param` は全項目に既定値があります。factoryやコンストラクタへ渡さなければ、この値です。

| 項目 | 型 | 既定値 | 条件・選び方 |
| --- | --- | --- | --- |
| `seed` | `std::uint64_t` | 1 | 任意の64bit符号なし整数。再現比較には固定する。時間予算では実行速度の差により結果が変わり得る。 |
| `sample_stride` | `std::uint64_t` | 1 | 正。各候補列の完了遷移何回ごとに標本を記録するか。大きくすると保存・出力回数を減らせるが、独立標本を保証しない。 |
| `start_temperature` | `double` | 1 | 正の有限値。Search開始時に悪化を許す程度。まず既定値から、損失差の大きさに応じて調整する。 |
| `end_temperature` | `double` | 0.001 | 正の有限値。Search終盤の値。小さいほど改善重視になるが、最適性の保証ではない。 |
| `cooling_steps` | `std::uint64_t` | 0 | Search全体で予定する遷移数。0は自動決定。分割してsolveするなら総予定数を指定すると冷却の進み方を揃えやすい。 |
| `cooling_deadline` | `Clock::time_point` | `Clock::time_point::max()` | Searchで温度を下げ終える絶対期限。指定時は `cooling_steps` より優先。 |
| `observation_check_interval` | `std::size_t` | 32 | 正。観測モデルの何項ごとに時計を調べるか。各観測の評価が重いなら1。全体評価を返す独自関数の内部は分割しない。 |
| `warmup_steps` | `std::optional<std::uint64_t>` | `std::nullopt` | sample前の準備量。未指定時は下記。明示的な0は準備を省く指定。準備量は収束の証明ではない。 |

冷却期限も総遷移数も指定しない場合、最初に実行されるSearchのBudgetから決めます。時間期限があればそれを、なければ遷移数を使います。同じモデルで分割実行する間は進行を引き継ぎます。モデル更新後は改めて始まります。準備と標本採取の温度は1で、Searchの温度設定は使いません。

既定の準備量は、標準数値提案なら `max(512, 64 * 可動変数数 * 候補列数)`、全固定なら0、独自状態・独自提案なら `512 * 候補列数` です。小予算の複数回の呼び出しでも準備を通算します。モデル変更、restart、実際にSearchの遷移を行った後は、再び準備が必要になります。

`MoveParam` は数値提案の設定です。候補列は「別々の現在値を持ちながら動く列」であり、複数あっても互いに独立した標本とは限りません。

| 項目 | 型 | 既定値 | 条件・選び方 |
| --- | --- | --- | --- |
| `adapt` | `bool` | true | Search・Warmup中に変更幅を調整する。Sample中は調整しない。通常はtrue。 |
| `difference_probability` | `double` | 0 | 0以上。Warmup/Sampleで、他の二列の差を使う確率。可動実数と3列以上が必要。 |
| `cauchy` | `bool` | true | Searchで時々大きな変更を起こせる幅を使う。Warmup/Sampleには影響しない。 |
| `stretch_probability` | `double` | 0.3 | 0以上。Warmup/Sampleで他の一列を中心に実数群を伸縮する確率。可動実数と2列以上が必要。 |

二つの確率の和は1以下です。列数などの必要条件を満たさない場合は通常の座標変更へ進みます。集団を使う二種類の提案はSearchでは使いません。まず既定値、強い相関のある実数群では複数の異なる開始値とともに試します。すべての問題で増やせば有利になる設定ではありません。

### 2.7 Sessionの構築・探索・継続

| API | 必須引数、任意引数・既定値 | 戻り値 |
| --- | --- | --- |
| `make_numeric_session(parameters, model, param = {}, move = {}, chains = 1)` | `const Parameters&` とモデルが必須。`Param`、`MoveParam`、正の `std::size_t chains` を任意指定。全列は同じ初期値で始まる。 | `Session<Model,NumericMove>` |
| `make_session(initial, model, move, param = {})` | `std::vector<State>` の空でない初期候補列、モデル、提案が必須。`Param` は任意。 | `Session<Model,Move>` |
| `Session(initial, model, move, param = {})` | 直接コンストラクタも上記と同じ。型は `S::Session<Model,Move>`。 | Sessionオブジェクト |
| `solve(Budget budget)` | Budget必須。同期・探索を実行。 | `Session::Result`。最良状態をコピーして所有する。 |
| `solve_view(Budget budget)` | Budget必須。同じ計算を行うが最良状態をコピーしない。 | `Report`。状態は `best()` で参照する。 |
| `best() const` | 引数なし。 | `Best`。次の実行・変更まで有効な参照。 |
| `model() const` | 引数なし。 | `const Model&`。予測、観測件数、Contextの読み取りなど。 |
| `revision() const` | 引数なし。 | `std::uint64_t`。目的関数・初期化の変更を識別する番号。変更をまたぐ集計の検出に使う。 |
| `warmup_remaining() const` | 引数なし。 | `std::uint64_t`。標本採取前に必要な準備遷移の残数。 |
| `restart(std::vector<State> initial)` | 空でない初期候補列が必須。 | `void`。同じ状態構造で開始値・列数を変更する。 |

構築だけでは目的関数を評価しません。最初のsolve/sampleの予算を使います。`solve` を繰り返せば前の候補から継続し、各回の結果はその時点までに見つかった最良候補です。

`restart` は乱数と探索履歴を初期化し、revisionを変えます。標準数値提案の学習済みの変更幅は保持します。完全に同じ初期設定から独立にやり直したい場合は、新しいSessionを構築してください。モデル・範囲・状態の意味はrestartで変更しません。

### 2.8 Sessionの更新

以下はすべて戻り値 `void` です。引数は必須で、更新内容を登録した後、実際の再評価は次のsolve/sampleの予算内で行います。変更直後の `best()` は、まだ現在の目的関数で評価されていないため空になり得ます。以前の最良候補は内部で再評価の候補として保持します。

| API | 引数と動作 | 利用条件・注意 |
| --- | --- | --- |
| `add_observations(batch)` | `std::vector<Model::observation_type>`。新規観測だけ追加。空なら何もしない。 | 観測モデル用。可能なときは追加分の評価を再利用する。同じbatchを二度追加しない。 |
| `replace_observations(data)` | 同じ型のvector。残す観測全体へ置換。空なら全削除。 | 訂正や任意削除に使う。全再評価。 |
| `retain_last(std::size_t count)` | 最後のcount件だけ保持。0なら全削除。件数以上なら何もしない。 | 観測モデル用。削除時は全再評価。並び順は追加した順。 |
| `update_model(Edit&& edit)` | `edit(Model&)` を一度呼ぶ。 | 所有モデル・所有Contextの編集後、全再評価を予約する。独自全体モデルにも使える。 |

Contextを変えると過去の行も新しいContextで評価されます。これは「昔の地図の登録ミスを直す」場合には有効です。一方、「ターンごとに実際の環境が変わった」場合は、各行に当時の地図や不変な識別情報を保持する必要があります。現在のContextだけで過去の観測を書き換えないでください。

観測件数、観測値、既知の地図、モデルの係数などは更新できます。未知変数の次元・型・範囲、カテゴリIDの意味、独自状態の構造を変える場合は、モデルと提案を含めて作り直します。目的関数が変わった前後の標本は、そのまま合算しません。

### 2.9 標本採取と集計

| API | 引数 | 戻り値・動作 |
| --- | --- | --- |
| `sample(Budget budget, Project&& project)` | 両方必須。`project(const State&)` は `double`、`std::array<double,N>`、`std::vector<double>` のいずれかを返す。 | `Session::SampleResult<Value>`。この呼び出しで出た標本の集計。 |
| `sample_each(Budget budget, Emit&& emit)` | 両方必須。`emit(const State&, double energy, int chain)` を呼べる関数。chainは0始まり。 | `Report`。標本を利用側へ一つずつ通知する。 |

`Move::sampling_safe` がtrueと宣言された提案にだけ提供されます。標準数値提案と `symmetric_move` / `hastings_move` の戻り値は、この宣言を持ちます。単なる提案ラムダではsolveだけが使える構成もあります。宣言があっても、独自提案の対称性や補正の正しさをライブラリが証明するわけではありません。

準備の完了後に標本を出します。**候補を棄却した回も、そのときの現状態を記録**します。採用された回だけを集めると違う分布になります。project/emitは実行中だけ使い、保存・コピーしません。受け取ったStateの参照を後で使うなら、自分でコピーしてください。

予測が非線形なら「平均状態から予測」と「各状態の予測の平均」は違います。後者が欲しい場合は、projectの中で毎回予測してください。複数回の `sample` のsummaryは自動では累積しません。累積には `sample_each` と外部のMomentsを使い、revisionと予測条件が同じことを確認します。

**`Session::Moments<Value>`** は既定構築で空です。Valueは上記の三種類、要素は有限値、vectorの長さは途中で変えません。要素ごとの平均・分散を持ち、標本そのものは保存しません。

| API | 引数・戻り値 |
| --- | --- |
| `void add(const Value& value)` | 値を一つ追加。必須引数。 |
| `std::uint64_t count() const` | 追加した標本数。初期値0。 |
| `const std::optional<Value>& mean() const` | 0件なら空。1件以上なら平均への参照。変更・破棄をまたぐ保持に注意。 |
| `std::optional<Value> variance() const` | 2件未満なら空。2件以上なら各成分の偏差二乗和を `count()-1` で割った分散のコピー。 |

分散は標本の広がりであり、平均の推定誤差や有効な独立標本数ではありません。共分散や分位点は提供しません。標本には相関があるため、件数だけで精度を断定しないでください。

### 2.10 結果型と列挙型

**`Stop`** は二値です。

| 値 | 正確な意味 |
| --- | --- |
| `Stop::Budget` | 通常の停止理由。予算による終了を表す。予算0で何も計算していない場合も含む。収束や有限候補の存在を意味しない。 |
| `Stop::NoFiniteState` | 初期候補・更新後候補の同期評価を終えたが、有限の評価値を持つ候補がない。問題全体に実行可能解がないという証明ではない。 |

**`Report`** は一回の呼び出しの統計です。Sessionのsampleでは、その呼び出し内の準備と採取の両方を含みます。

| メンバー | 型 | 既定値 | 意味 |
| --- | --- | --- | --- |
| `stop` | `Stop` | `Budget` | 停止理由。 |
| `steps` | `std::uint64_t` | 0 | 完了した候補遷移数。初期評価・再評価は含まない。 |
| `accepted` | `std::uint64_t` | 0 | 採用された遷移数。 |
| `evaluations` | `std::uint64_t` | 0 | 最後まで完了した状態の評価回数。 |
| `terms` | `std::uint64_t` | 0 | 評価した基本項の数。観測モデルでは事前項も1項。独自全体評価は一回を1項と数える。 |
| `emitted` | `std::uint64_t` | 0 | 標本の通知回数。準備中は増えない。runの出力コールバックなし版では、内部の何もしない通知も数える。 |
| `elapsed_us` | `std::int64_t` | 0 | この呼び出しの経過マイクロ秒。 |
| `synchronized` | `bool` | false | 全候補列の評価値が現在のモデルで揃ったか。 |
| `has_estimate` | `bool` | false | 現在のモデルで有限に評価できた候補があるか。 |

一部の再評価だけ終えた場合、`has_estimate=true` と `synchronized=false` が同時に成立し得ます。

**`Best`** は借りた状態を指す軽量な結果です。

| メンバー | 型 | 戻されたときの意味 |
| --- | --- | --- |
| `state` | `const State*` | 有限候補がなければnullptr。あれば最良候補へのポインタ。 |
| `energy` | `double` | その候補の目的関数値。候補なしなら正の無限大。 |

Bestのメンバーには宣言時初期値がありません。自分で `Best b;` とだけ書いて読み出さず、`best()` の戻り値を使います。次の実行・更新・破棄後は保持したポインタを使いません。

**`Session::Result`** はsolveの結果を所有します。

| メンバー | 型・既定値 | 意味 |
| --- | --- | --- |
| `state` | `std::optional<State>`、空 | 最良状態のコピー。Sessionを更新してもこのコピーは残る。 |
| `energy` | `double`、正の無限大 | その状態の目的関数値。 |
| `report` | `Report`、上表の既定値 | 今回の実行統計。 |
| `revision` | `std::uint64_t`、0 | どのモデル番号で得た結果か。 |

**`Session::SampleResult<Value>`** はsampleの結果です。

| メンバー | 型・初期状態 | 意味 |
| --- | --- | --- |
| `summary` | `Moments<Value>`、空 | 今回記録した値の集計。 |
| `report` | `Report`、上表の既定値 | 準備と採取を合わせた統計。 |
| `revision` | `std::uint64_t`、メンバー初期値なし | sampleは必ず現在の番号を設定して返す。 |
| `warmup_remaining` | `std::uint64_t`、メンバー初期値なし | sampleは必ず準備の残数を設定して返す。 |

この結果型も、自分で未初期化のオブジェクトを作って数値を読まず、メソッドの戻り値を使います。

`McmcState`、`Report`、`Best`、`Session::Result`、`Session::Moments<Value>`、`Session::SampleResult<Value>` は、すべて **`std::ostream& operator<<(std::ostream&, const 型&)`** を提供します。二引数とも必須、同じostreamの参照を返すので連続して出力できます。自動改行はなく、`std::cerr << result << '\n';` のように使います。書式は呼び出し側の設定に従い、配列はそれぞれ先頭20要素まで、残りは `... (+N more)` で省略します。空は `none`。独自Stateに出力演算子がなければ `<unprintable>` です。`std::optional` 自体の出力演算子は追加しないので、状態を単独出力するときは有無を確認して `*result.state` を出します。

### 2.11 実行段階・提案・乱数のAPI

**`Phase`** は三値です。

| 値 | 意味 |
| --- | --- |
| `Phase::Search` | 良い候補の探索。温度を変え、数値提案も調整する。ここで出た状態列を事後分布の標本として使わない。 |
| `Phase::Warmup` | 温度1で標本採取の準備をする。提案幅の調整を許す。標本を出力しない。 |
| `Phase::Sample` | 温度1で標本を採取する。標準数値提案の幅は固定。strideごとに現状態を出す。 |

**`MoveContext`** は提案に渡される情報です。各メンバーに宣言時初期値はなく、通常はエンジンが埋めます。

| メンバー | 型・意味 |
| --- | --- |
| `phase` | `Phase`。実行段階。 |
| `temperature` | `double`。現在の正の温度。 |
| `chain` | `std::size_t`。対象列の0始まり番号。 |
| `population` | `std::span<const State>`。全列の現在状態の借用ビュー。 |
| `revision` | `std::uint64_t`。現在のモデル・初期化の番号。 |

このビューを実行の外へ保存しません。提案は `move(const State& old, State& next, Rng& rng, const MoveContext& ctx)` の形で呼ばれます。`next` はエンジンが用意したoldのコピーです。戻り値は `double` で、逆向きの提案確率を順向きの確率で割った値の自然対数です。対称なら0、無効な候補なら `-INFINITY`。NaN / `+INFINITY` は禁止です。任意に `void feedback(bool accepted, const MoveContext&)` を用意すると、完了した試行の採否通知を受けます。Sample中に提案分布を適応させてはいけません。

| static factory | 引数と戻り値 |
| --- | --- |
| `symmetric_move(F move)` | moveは必須。上記四引数を受け、**bool**で有効/無効を返す関数を包む。正逆の提案確率が同じであることを利用側が保証する。 |
| `hastings_move(F move)` | moveは必須。上記四引数を受け、提案比の対数または負の無限大を返す関数を包む。補正の正しさを利用側が保証する。 |

返る型は通常 `auto` で保持します。戻り値は `static constexpr bool sampling_safe = true`、上述の提案 `operator()`、`feedback(bool,const MoveContext&)` を公開します。feedbackは元の関数オブジェクトに対応メソッドがある場合だけ転送します。型名を明示せず `decltype(move)` として扱え、関数オブジェクト一つで直接構築するコンストラクタも持ちます。

**`Rng`** は提案に使う乱数です。

| API | 引数・既定値 | 戻り値 |
| --- | --- | --- |
| `explicit Rng(std::uint64_t seed = 1)` | 任意の64bit符号なし整数。 | 乱数器。 |
| `next()` | なし。 | `std::uint64_t`。64bit乱数。 |
| `uniform()` | なし。 | `double`。0以上1未満の一様乱数。 |
| `log_uniform()` | なし。 | `double`。0より大きく1以下の一様乱数の自然対数。有限で0以下。 |
| `integer(int n)` | 必須。1〜2^31−1。 | `int`。0以上n未満の整数。 |
| `normal()` | なし。 | `double`。平均0、標準偏差1の正規乱数。 |

### 2.12 DomainとNumericMoveの直接利用

普通はParametersから自動で作れます。直接部品を組み合わせるときだけ使います。

| 型・メンバー | 型・既定値・条件 |
| --- | --- |
| `Domain::Real::lower`, `upper` | `double`。初期値なし。両端を含む範囲、lower<=upper。 |
| `Domain::Real::scale` | `double`。初期値なし。直接構築では固定変数も含め正の有限値。0による自動設定はParametersの機能。 |
| `Domain::Discrete::lower`, `upper` | `std::int64_t`。初期値なし。両端を含む範囲。整数の絶対値・幅は10億以下を想定。 |
| `Domain::Discrete::categorical` | `bool`、false。falseなら大小のある整数、trueなら種類IDとして扱う。 |
| `Domain::real` | `std::vector<Domain::Real>`、空。State::realと順序・要素数を一致させる。 |
| `Domain::discrete` | `std::vector<Domain::Discrete>`、空。State::discreteと一致させる。 |

| 公開API | 引数・戻り値・注意 |
| --- | --- |
| `explicit NumericMove(Domain domain, MoveParam param = {})` | domain必須。上の条件を満たす。標準数値提案を構築する。 |
| `make_numeric_move(Domain domain, MoveParam param = {})` | 同じ引数から `NumericMove` を値で返すstatic factory。 |
| `NumericMove::sampling_safe` | `static constexpr bool`、true。 |
| `NumericMove::operator()(old,next,rng,ctx)` | 前節と同じ4引数とdoubleの提案比。直接呼ぶ側は先にnextをoldのコピーにする。 |
| `NumericMove::feedback(bool accepted, const MoveContext& ctx)` | 2引数必須、戻り値void。一回の完了試行ごとに一度呼ぶ。通常はエンジンが担当する。 |

### 2.13 McmcEstimator本体を使う下位API

通常はSessionで十分です。下位APIではモデル・提案を利用側で所有し、変更の通知、準備、標本の保存も自分で管理します。

| API | 引数、既定値、戻り値 |
| --- | --- |
| `explicit McmcEstimator(Param param = {})` | 任意のParamから本体を構築する。 |
| `reset(std::span<const State> initial, Evaluate&& evaluate, Budget& budget)` | 3引数必須。初期列は非空。履歴・乱数を初期化し、状態をコピーして初期評価する。戻り値Report。 |
| `refresh(Evaluate&& evaluate, Budget& budget)` | 2引数必須。外部モデル変更後に全評価を無効化し、再評価する。reset後に使う。Reportを返す。 |
| `append_energy(Delta&& delta, Budget& budget)` | 2引数必須。同期済みかつ有限候補ありのとき、`delta(const State&)` が返す新E−旧Eを保持評価へ加える。Reportを返す。 |
| `observe(Model& model, std::vector<Model::observation_type> batch, Budget& budget)` | 3引数必須。観測モデルへ一度追加し、可能なら追加項のみ評価。空batchは目的関数を変えず、未完の同期を進める場合がある。reset後に使う。Reportを返す。 |
| `replace_observations(Model& model, std::vector<Model::observation_type> data, Budget& budget)` | 3引数必須。全観測を置換して再評価。reset後に使う。Reportを返す。 |
| `best() const` | 引数なし。Bestを返す。ポインタは次の実行・変更まで有効。 |
| `run(Phase phase, Evaluate&& evaluate, Propose&& propose, Budget& budget, Emit&& emit)` | 5引数必須。reset後、指定段階を実行。evaluate/propose/emitの形は前節までの契約。Reportを返す。 |
| `run(Phase phase, Evaluate&& evaluate, Propose&& propose, Budget& budget)` | 出力コールバックなしの4引数版。Reportを返す。 |

参照渡しBudgetの `remaining_steps` は完了遷移ごとに減ります。途中で止めた場合は、**同じモデルと提案オブジェクト**を次回も渡します。`run(Sample,...)` は準備を自動で行わず、`sampling_safe` の宣言も検査しません。利用側が正しい提案と十分な準備を用意します。

`append_energy` は候補を変更した際の `candidate` とは違い、**目的関数そのものを変えたとき**の機能です。更新後の全体評価も用意し、次のrunには必ずそれを渡します。時間切れでも同じ差を二度加えず、残りは新しい全体評価によるrunで同期します。差分コールバックは一回の呼び出し中に完了する必要があり、その内部を中断しません。

### 2.14 実行前の確認

- 初期候補のうち一つ以上で、目的関数を有限にします。制約を満たさない状態だけでは探索が始まりません。
- `result.state` / `best.state` / `summary.mean()` / `summary.variance()` は空かどうかを確認してから使います。
- 入力が同じ値ばかりなど、異なる係数を区別できない観測では、未知値を一意に決められません。
- 数値範囲、単位、既知sigma、損失に渡す値の意味を合わせます。成功確率とlogit、平均件数と対数平均を取り違えません。
- 独自シミュレータに固定乱数を入れた場合、評価の再現性は保てますが、厳密な確率モデルになるかは別途モデルの定義に依存します。
- 予測する対象の平均のばらつきと、次回の測定誤差込みのばらつきは別です。例3.16のコメントを参照してください。

## 3. ユースケース

各節は「具体的な問題 → モデルとの対応 → 独立した実行コード → 入出力」の順です。入力例は小さく、何を推定しているか確かめやすい値にしています。探索予算は正解を保証する値ではありません。標本数・離散値以外の小数は実行環境により多少変わり得ます。時間予算や経過時間を含む診断は特に変わります。

すべてのコードは入力形式に従う正常入力を想定しています。長さやIDの妥当性を網羅的に検査するパーサではありません。各コード冒頭にも、問題、入力・出力、必要な注意を記載しています。


| 節 | 具体的な用途 | 主に使うAPI・考え方 |
| --- | --- | --- |
| 3.01 | 三入力の一次関数・既知の測定精度 | Parameters.real / Observation / make_observation_model / make_numeric_session / solve / prediction |
| 3.02 | 三入力の一次関数・整数係数 | Parameters.integer / McmcState.discrete |
| 3.03 | 一次・二次・正弦の式の種類も推定する | Parameters.category / 実数とカテゴリの混在 |
| 3.04 | 入力を増やすと頭打ちになる装置 | 独自予測 / 非線形関数 / 正の範囲 |
| 3.05 | たまに極端に外れる測定 | Huber / threshold / scale |
| 3.06 | 観測が少ないとき、事前知識で候補を絞る | prior / gaussian_loss |
| 3.07 | affine3の四係数と共通の標準偏差を同時に推定 | 独自損失 / 状態に含めたsigma / gaussian_loss |
| 3.08 | affine3の推定後、残差から標準偏差を計算 | model / prediction / 結果の所有 / 残差 |
| 3.09 | 成功・失敗から成功確率を予測 | BernoulliLogit / prediction / mean_prediction |
| 3.10 | 観測時間が異なる発生件数 | PoissonLogMean / mean_prediction |
| 3.11 | 二つの出力・一部欠測・誤差の相関 | 独自Observation / 配列予測 / optionalによる欠測 |
| 3.12 | 再現シミュレーションから機械速度を推定 | 構造を持つinput / 決定的シミュレーション |
| 3.13 | ターンごとの観測追加・訂正・古い観測の削除 | add_observations / replace_observations / retain_last / revision / size / observations |
| 3.14 | 既知の地図データを訂正して再推定 | make_context_model / context / update_model |
| 3.15 | 計算を分割し、残り時間だけ追加で使う | Budget全factory / available / time_left / Clock / cooling_steps / solve_view / best |
| 3.16 | 予測の平均とばらつきを求める | sample / sample_stride / warmup_steps / SampleResult / Moments全メソッド |
| 3.17 | 複数回の標本採取を自分で集計 | sample_each / 外部Moments / revision |
| 3.18 | 異なる初期値から複数の候補列を動かす | MoveParam / chains / restart / Parameters.initial / warmup_remaining |
| 3.19 | 連続する測定の誤差をまとめて評価 | 独自全体評価operator() |
| 3.20 | 端子間の差の測定を、変更箇所だけ再評価 | Model.candidate / 全固定の係数 |
| 3.21 | 合計1になる三種類の混合割合 | 独自State / make_session / symmetric_move / Rng |
| 3.22 | 誤りを含む比較結果から順序を推定 | 独自State / 順列 / symmetric_move |
| 3.23 | 候補を選ぶ確率が偏る場合の標本採取 | hastings_move / sampling_safe / vector集計 |
| 3.24 | 観測一行の損失と予測変換を直接確かめる | 四標準損失のoperator()とmean / ObservationModel直接構築 / observation_type |
| 3.25 | 下位APIで探索・準備・標本採取を自分で管理 | 本体コンストラクタ / reset / run両形式 / Phase全値 / best |
| 3.26 | 下位APIで観測更新と外部変数の変更を通知 | observe / 下位replace_observations / refresh |
| 3.27 | 目的関数へ新しい罰則を加える | append_energy / Report.synchronized / has_estimate |
| 3.28 | 数値提案と乱数を直接調べてから組み込む | Rng全メソッド / Domain / NumericMove直接構築・operator()・feedback / MoveContext / Session直接構築 |
| 3.29 | 候補がない場合の回復・固定変数・デバッグ出力 | Stop全値 / Result / 空optional / 全固定 / 出力演算子6種類 |
| 3.30 | 正の尺度を対数で持ち、標本採取の重みを合わせる | 変数変換 / 目的関数と標本の重み / scalar集計 |


### 3.01 三入力の一次関数・既知の測定精度

**具体的な問題**

作業時間が三種類の仕事量に比例し、さらに準備時間が掛かる。複数回の仕事量と所要時間から、三つの係数と準備時間を推定する。

**モデルとの対応**

$f(x,θ)=a_ {1}x_ {1}+a_ {2}x_ {2}+a_ {3}x_ {3}+b$とする。入力は既知、四係数が未知で、各観測に既知の標準偏差と重みを設定する。大きい標準偏差は測定が粗いことを表す。

一件の入力は三つの数です。例えば入力が `(1,2,3)` なら、予測は `a1 + 2*a2 + 3*a3 + b` になります。未知の四係数だけを `Parameters` に登録し、入力値は `Observation::input` に置きます。`real` の戻り値を添字として使えば、登録順を変えたときも対応が分かりやすくなります。

この例では、各行に既知の標準偏差と重みも入力します。誤差がだいたい0.2なら `scale=0.2`、すべての測定を普通に使うなら `weight=1` です。予測関数へ誤差の乱数を足す必要はありません。予測は誤差を除いた中心値を返し、損失が測定のばらつきを考慮します。入力を別々に変化させないと、四係数を区別できない場合があります。

**入力と出力**

入力はN、続いてN行のx1 x2 x3 y sigma weight、最後に予測したいq1 q2 q3。出力はa1 a2 a3 bと、その入力での予測。sigma>0、weight>=0。

**実行コード：`mcmc_sample_01_affine3_v01.cpp`**

```cpp
// 三入力の一次関数・既知の測定精度
// 問題: 作業時間が三種類の仕事量に比例し、さらに準備時間が掛かる。複数回の仕事量と所要時間
// から、三つの係数と準備時間を推定する。
// モデル: $f(x,θ)=a_ {1}x_ {1}+a_ {2}x_ {2}+a_ {3}x_ {3}+b$とする。入力は既知、四係数が未知
// で、各観測に既知の標準偏差と重みを設定する。大きい標準偏差は測定が粗いことを表す。
// 入出力: 入力はN、続いてN行のx1 x2 x3 y sigma weight、最後に予測したいq1 q2 q3。出力はa1
// a2 a3 bと、その入力での予測。sigma>0、weight>=0。
// 注意: 四係数を区別できる入力の組合せが必要。全観測のsigmaを同じ値へ変えても、事前項なしの
// 最良候補の位置は変わらないが、標本の広がりは変わる。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 標準状態を用いる推定器。
    using Input = std::array<double, 3>;
    // 三入力と、観測値・既知の誤差・重みを持つ行。
    using Row = S::Observation<Input>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }
    // 観測行。入力済みのデータを後でモデルへ移す。
    std::vector<Row> rows(n);
    // sigmaはscaleへ。
    for (auto& r : rows) {
        std::cin >> r.input[0] >> r.input[1] >> r.input[2] >> r.value >> r.scale
            >> r.weight;
    }

    Input query;
    for (double& x : query) {
        std::cin >> x;
    }

    S::Parameters p; // 四つの未知係数を登録する。

    // a1の初期値0、上下限-10と10。提案幅は省略時に範囲から設定。
    const auto a1 = p.real(0, -10, 10);
    const auto a2 = p.real(0, -10, 10); // a2も実数。戻り値はreal配列内の添字。
    // a3も実数。係数の単位に応じて範囲を変更する。
    const auto a3 = p.real(0, -10, 10);
    const auto b = p.real(0, -10, 10); // 切片bの初期値と範囲。

    // 入力と候補状態から予測する純粋な関数。
    auto predict = [=](const Input& x, const McmcState& s) {
        // 登録した添字で未知係数を読む。
        return s.real[a1] * x[0] + s.real[a2] * x[1] + s.real[a3] * x[2] + s.real[b];
    };

    // 既定Gaussianは重み付き正規誤差の損失。
    auto model = S::make_observation_model(std::move(rows), predict);

    // 初期値・範囲・観測を所有する実行器を作る。
    auto session = S::make_numeric_session(p, std::move(model));

    // 8万回の提案を上限に最良候補を探す。
    const auto r = session.solve(S::Budget::for_steps(80'000));
    // 空のoptionalを参照しない。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 結果は状態のコピーを所有し、後の再実行後も保持できる。
    const auto& s = *r.state;

    // 四係数。
    std::cout << std::setprecision(10) << s.real[a1] << ' ' << s.real[a2] << ' '
              << s.real[a3] << ' ' << s.real[b] << '\n';

    // 同じモデルで未観測入力の予測を計算する。
    std::cout << session.model().prediction(query, s) << '\n';
}
```

**入力例：`data/01.in`**

```text
8
-1 -1 -1 1.3 0.2 1
-1 -1 1 2.7 0.2 1
-1 1 -1 -4.3 0.2 1
-1 1 1 -3.7 0.2 1
1 -1 -1 5.7 0.2 1
1 -1 1 6.3 0.2 1
1 1 -1 -0.7 0.2 1
1 1 1 0.7 0.2 1
1 2 3
```

**出力例（実行確認時の標準出力）**

```text
1.999776971 -3.000108428 0.4998314518 0.999856652
-1.501088878
```

**使うときの注意**

四係数を区別できる入力の組合せが必要。全観測のsigmaを同じ値へ変えても、事前項なしの最良候補の位置は変わらないが、標本の広がりは変わる。

### 3.02 三入力の一次関数・整数係数

**具体的な問題**

仕事一個あたりの必要資材と固定費が、すべて整数である。連続値で推定して丸める代わりに、最初から整数候補を比較する。

**モデルとの対応**

式は三入力の一次関数で、四係数の取り得る値を整数に限定する。入力と観測値はdoubleのままでよい。

入力と観測は実数でも構いません。整数である必要があるのは未知の四係数だけです。そこで、実数版の `p.real` を `p.integer` に変え、状態から読む配列を `real` から `discrete` に変えます。

実数として推定してから最後に丸める方法とは違い、探索中の候補自体が整数になります。例えば一つの係数を丸めると、他の係数で埋め合わせた方がよい場合もあります。本例では整数候補の組全体を目的関数で比較します。

**入力と出力**

NとN行のx1 x2 x3 y。出力は整数a1 a2 a3 b。

**実行コード：`mcmc_sample_02_integer_affine3_v01.cpp`**

```cpp
// 三入力の一次関数・整数係数
// 問題: 仕事一個あたりの必要資材と固定費が、すべて整数である。連続値で推定して丸める代わり
// に、最初から整数候補を比較する。
// モデル: 式は三入力の一次関数で、四係数の取り得る値を整数に限定する。入力と観測値はdouble
// のままでよい。
// 入出力: NとN行のx1 x2 x3 y。出力は整数a1 a2 a3 b。
// 注意: 整数の絶対値と範囲幅は10^9以下を想定する。整数制約を課した結果に、通常の最小二乗の
// 自由度補正式をそのまま当てはめない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 標準状態McmcStateを使う推定器。
    using Input = std::array<double, 3>;
    // inputに三入力、valueに観測値を入れる。
    using Row = S::Observation<Input>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }
    // 観測行をn件用意。scale=1、weight=1が既定値。
    std::vector<Row> rows(n);
    // 観測を設定。
    for (auto& r : rows) {
        std::cin >> r.input[0] >> r.input[1] >> r.input[2] >> r.value;
    }

    S::Parameters p; // 未知変数の初期値と探索範囲を登録する。

    const auto a1 = p.integer(0, -10, 10); // 整数a1の初期値0と両端を含む範囲。
    const auto a2 = p.integer(0, -10, 10); // 戻り値はdiscrete配列の添字。
    const auto a3 = p.integer(0, -10, 10); // 実数用real配列の添字とは別管理。
    const auto b = p.integer(0, -10, 10); // 切片も整数に限定。

    // 入力と整数候補から予測する。
    auto predict = [=](const Input& x, const McmcState& s) {
        // 整数係数を読む。
        return s.discrete[a1] * x[0] + s.discrete[a2] * x[1] + s.discrete[a3] * x[2]
               + s.discrete[b];
    };

    // 既定Gaussianで観測を比較。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(std::move(rows), predict));

    // 整数候補の探索予算。
    const auto r = session.solve(S::Budget::for_steps(60'000));
    // 結果の有無を確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 丸め不要の整数推定値。
    for (auto j : {a1, a2, a3, b}) {
        std::cout << r.state->discrete[j] << ' ';
    }

    std::cout << '\n';
}
```

**入力例：`data/02.in`**

```text
8
-1 -1 -1 1
-1 -1 1 3
-1 1 -1 -5
-1 1 1 -3
1 -1 -1 5
1 -1 1 7
1 1 -1 -1
1 1 1 1
```

**出力例（実行確認時の標準出力）**

```text
2 -3 1 1
```

**使うときの注意**

整数の絶対値と範囲幅は10^9以下を想定する。整数制約を課した結果に、通常の最小二乗の自由度補正式をそのまま当てはめない。

### 3.03 一次・二次・正弦の式の種類も推定する

**具体的な問題**

装置の応答が一次関数、二次関数、正弦関数のどれか分からない。式の種類と、その式に入る係数を同時に調べる。

**モデルとの対応**

t=1ならax+b、t=2ならax²+b、t=3ならa sin x+b。種類はカテゴリID 0〜2で保持し、表示時に1を足す。正弦のxはラジアン。

利用者が決める三つの候補式は、`t=1` なら `a*x+b`、`t=2` なら `a*x*x+b`、`t=3` なら `a*sin(x)+b` です。ライブラリのカテゴリIDは0から始まるため、コードでは `id=0,1,2` を使い、出力時だけ1を足してtへ戻します。

種類IDは「2が1の二倍の性質を持つ」といった数値ではありません。`category` はそのような大小関係のない選択肢に使います。aとbは実数なので、状態にカテゴリ一つと実数二つが共存します。`sin` の入力はラジアンです。xがごく狭い範囲にしかなければ、異なる式でも似た値になって区別しにくくなります。

**入力と出力**

NとN行のx y。出力はt a b。

**実行コード：`mcmc_sample_03_function_kind_v01.cpp`**

```cpp
// 一次・二次・正弦の式の種類も推定する
// 問題: 装置の応答が一次関数、二次関数、正弦関数のどれか分からない。式の種類と、その式に入
// る係数を同時に調べる。
// モデル: t=1ならax+b、t=2ならax²+b、t=3ならa sin x+b。種類はカテゴリID 0〜2で保持し、表示
// 時に1を足す。正弦のxはラジアン。
// 入出力: NとN行のx y。出力はt a b。
// 注意: a=0では三種類とも同じ定数関数になる。xが狭い範囲だけに偏ると一次と正弦を区別しにく
// い。式の種類が必ず一意に決まるわけではない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数とカテゴリを同時に扱う。
    using Row = S::Observation<double>; // 入力xと観測yを持つ行。

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // scale=1、weight=1を利用。
    // 観測行を設定。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 各種類で共通のa,bと種類IDを登録。
    const auto a = p.real(0, -10, 10); // aの初期値0と範囲。
    const auto b = p.real(0, -10, 10); // bの初期値0と範囲。
    const auto kind = p.category(0, 3); // 初期ID0、候補数3。格納値は0,1,2。

    // 種類を指定してユーザー関数を実行。
    auto predict = [=](double x, const McmcState& s) {
        const auto t = s.discrete[kind]; // カテゴリはdiscreteから読む。
        const double basis = t == 0 ? x : t == 1 ? x * x : std::sin(x);
        return s.real[a] * basis + s.real[b]; // a,bはrealから読む。
    };

    // 損失はGaussian。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(std::move(rows), predict));

    // 種類変更も含めた探索予算。
    const auto r = session.solve(S::Budget::for_steps(100'000));
    // 解がない場合を扱う。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // tを1始まりへ戻す。
    std::cout << std::setprecision(10) << r.state->discrete[kind] + 1 << ' '
              << r.state->real[a] << ' ' << r.state->real[b] << '\n';
}
```

**入力例：`data/03.in`**

```text
7
-3 0.71775998388
-2 -0.818594853651
-1 -0.682941969616
0 1
1 2.68294196962
2 2.81859485365
3 1.28224001612
```

**出力例（実行確認時の標準出力）**

```text
3 1.999800119 0.9997996601
```

**使うときの注意**

a=0では三種類とも同じ定数関数になる。xが狭い範囲だけに偏ると一次と正弦を区別しにくい。式の種類が必ず一意に決まるわけではない。

### 3.04 入力を増やすと頭打ちになる装置

**具体的な問題**

投入量を増やすと出力は増えるが、次第に増加が小さくなる。装置の最大増分、頭打ちの速さ、基礎出力を求めたい。

**モデルとの対応**

予測をa*x/(b+x)+cとする。a,b,cが未知で、b>0とx>=0により分母0を防ぐ。

入力xが0なら出力はcです。入力を大きくすると `x/(b+x)` が1へ近づき、出力は `a+c` に近づきます。bは頭打ちに近づく速さを左右します。つまりaは増える量、bは入力側の尺度、cは基礎出力という役割です。

予測関数が係数に対して曲がった形でも、使うAPIは一次関数のときと同じです。xを非負、bを正の範囲にして、分母0を避けます。頭打ちが分かる大きなxと、初めの増え方が分かる小さなxの両方があると区別しやすくなります。

**入力と出力**

NとN行のx y。出力はa b c。

**実行コード：`mcmc_sample_04_saturation_v01.cpp`**

```cpp
// 入力を増やすと頭打ちになる装置
// 問題: 投入量を増やすと出力は増えるが、次第に増加が小さくなる。装置の最大増分、頭打ちの速
// さ、基礎出力を求めたい。
// モデル: 予測をa*x/(b+x)+cとする。a,b,cが未知で、b>0とx>=0により分母0を防ぐ。
// 入出力: NとN行のx y。出力はa b c。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 非線形でも予測関数の形をそのまま使う。
    using Row = S::Observation<double>; // 投入量と出力。

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 正規誤差の標準観測行。
    // 分母の安全性に必要な入力条件。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
        assert(r.input >= 0);
    }

    S::Parameters p; // 未知パラメータの範囲を定義。
    const auto a = p.real(1, 0, 10); // 最大増分は非負。
    const auto b = p.real(1, 0.05, 10); // 正の下限で分母0を避ける。
    const auto c = p.real(0, -5, 5); // 基礎出力。

    // 同じ入力・状態なら同じ出力。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x / (s.real[b] + x) + s.real[c];
    };

    // 予測を損失へ接続。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(std::move(rows), predict));

    // 相関する係数なので少し多めの予算。
    const auto r = session.solve(S::Budget::for_steps(100'000));
    // 有限の候補を確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 三係数。
    std::cout << std::setprecision(10) << r.state->real[a] << ' ' << r.state->real[b] << ' '
              << r.state->real[c] << '\n';
}
```

**入力例：`data/04.in`**

```text
6
0 1
0.5 1.8
1 2.33333333333
2 3
4 3.66666666667
8 4.2
```

**出力例（実行確認時の標準出力）**

```text
3.999098544 1.998674916 0.9996822171
```

### 3.05 たまに極端に外れる測定

**具体的な問題**

通常は一次関数に沿う処理時間が、たまの障害で大幅に増える。極端な一件に係数全体を引っ張られにくくする。

**モデルとの対応**

予測はax+bのまま、誤差の罰をHuberへ変える。小さいずれは二乗、大きいずれはほぼ直線的に増える罰にする。

二乗の罰では、ずれが10倍なら罰は100倍になります。これが、極端な一件が結果を大きく動かす理由です。Huberでは一定のずれを超えると、罰の増え方をほぼ直線にします。

変更はモデル作成時の損失指定です。`scale=0.1` は通常時のばらつき、`threshold=1.345` はその尺度で何倍までを二乗で罰するかです。この二つを混同しないでください。外れた行を勝手に消す処理は入れていません。

**入力と出力**

NとN行のx y。出力はa b。既知の通常誤差scale=0.1、切替値threshold=1.345。

**実行コード：`mcmc_sample_05_huber_v01.cpp`**

```cpp
// たまに極端に外れる測定
// 問題: 通常は一次関数に沿う処理時間が、たまの障害で大幅に増える。極端な一件に係数全体を引
// っ張られにくくする。
// モデル: 予測はax+bのまま、誤差の罰をHuberへ変える。小さいずれは二乗、大きいずれはほぼ直線
// 的に増える罰にする。
// 入出力: NとN行のx y。出力はa b。既知の通常誤差scale=0.1、切替値threshold=1.345。
// 注意: Huberのthresholdを未知変数として探索する例ではない。thresholdも推定するなら、確率分
// 布の正規化を含む独自損失が必要になる。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    // 通常時の誤差尺度。外れ値の大きさを入れる値ではない。
    for (auto& row : rows) {
        row.scale = 0.1;
    }

    // 標準化残差の切替値を固定。
    auto model = S::make_observation_model(std::move(rows), predict, S::Huber{1.345});

    // 範囲と損失を使って探索する。
    auto session = S::make_numeric_session(p, std::move(model));

    // 6万遷移を上限にする。
    const auto r = session.solve(S::Budget::for_steps(60'000));

    // 未評価・制約違反なら空なので先に確認する。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 推定したa,b。
    std::cout << std::setprecision(10) << r.state->real[a] << ' ' << r.state->real[b]
              << '\n';
}
```

**入力例：`data/05.in`**

```text
10
0 1
1 3
2 5
3 7
4 9
5 41
6 13
7 15
8 17
9 19
```

**出力例（実行確認時の標準出力）**

```text
2.000906646 1.010881839
```

**使うときの注意**

Huberのthresholdを未知変数として探索する例ではない。thresholdも推定するなら、確率分布の正規化を含む独自損失が必要になる。

### 3.06 観測が少ないとき、事前知識で候補を絞る

**具体的な問題**

x=1でy=5という一件だけではa+b=5しか分からない。別の測定からaは2に近いと分かっているので、その情報を使う。

**モデルとの対応**

観測の誤差に、aが2から外れた分とbが0から外れた分の罰を加える。aの事前の標準偏差は0.5、bは10としてaの情報を強くする。

x=1、y=5だけなら、a=2・b=3でもa=4・b=1でも同じ予測です。観測だけでは一組に決まりません。そこでaには「2から0.5程度の幅」、bには「0から10程度の幅」という事前情報を置きます。

事前項は観測行の損失に繰り返し加えるのでなく、`prior` としてモデルへ渡します。これにより観測件数が増えても、同じ事前情報を二重、三重に数えません。標準偏差0.5のaの事前情報は、標準偏差10のbより狭く、aを強く2へ寄せます。

**入力と出力**

NとN行のx y。出力はa b。

**実行コード：`mcmc_sample_06_prior_v01.cpp`**

```cpp
// 観測が少ないとき、事前知識で候補を絞る
// 問題: x=1でy=5という一件だけではa+b=5しか分からない。別の測定からaは2に近いと分かっている
// ので、その情報を使う。
// モデル: 観測の誤差に、aが2から外れた分とbが0から外れた分の罰を加える。aの事前の標準偏差は
// 0.5、bは10としてaの情報を強くする。
// 入出力: NとN行のx y。出力はa b。
// 注意: 事前項は正解を保証するものではない。根拠のない強い事前項を置くと、観測よりもその思
// い込みに結果が寄る。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    auto prior = [=](const McmcState& s) { // 観測全体に一度だけ加える事前項。
        // 狭い0.5はaを強く拘束する。
        return S::gaussian_loss(s.real[a], 2, 0.5) + S::gaussian_loss(s.real[b], 0, 10);
    };

    // 損失の後ろに事前項を渡す。
    auto model = S::make_observation_model(std::move(rows), predict, S::Gaussian{}, prior);

    // 観測と事前知識を合わせて比較する。
    auto session = S::make_numeric_session(p, std::move(model));

    // 一意な観測解がなくても事前項で選べる。
    const auto r = session.solve(S::Budget::for_steps(80'000));

    // 未評価・制約違反なら空なので先に確認する。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 推定したa,b。
    std::cout << std::setprecision(10) << r.state->real[a] << ' ' << r.state->real[b]
              << '\n';
}
```

**入力例：`data/06.in`**

```text
1
1 5
```

**出力例（実行確認時の標準出力）**

```text
2.007146968 2.963784582
```

**使うときの注意**

事前項は正解を保証するものではない。根拠のない強い事前項を置くと、観測よりもその思い込みに結果が寄る。

### 3.07 affine3の四係数と共通の標準偏差を同時に推定

**具体的な問題**

測定のばらつきが毎回同程度だが、その大きさも不明。四係数と一つの共通sigmaを五つの未知数として求める。

**モデルとの対応**

各行の損失を残差²/(2 sigma²)+log sigmaとする。log sigmaを省くと、sigmaを大きくするだけで得をする誤った目的関数になる。

ここでいう「共通sigma」は、観測ごとに変わらない一つの未知数です。`Observation::scale` に未確定の値を入れる代わりに、sigmaを五つ目の実数として状態へ入れます。予測関数は四係数だけで中心値を計算し、独自損失が現在のsigmaを読みます。

$$E=\sum_ {i=1}^{N}\left(\frac{(y_ {i}-\widehat y_ {i})^{2}}{2\sigma^{2}}+\log\sigma\right)$$

ここで $\widehat y_ {i}$ は候補係数からの予測、$y_ {i}$ は観測値、Nは件数です。sigmaを小さくすると残差の罰が増え、大きくするとlog sigmaが増えるため、両方が釣り合う大きさを探せます。完全に説明できるデータではsigmaを0へ寄せたくなるので、測定精度に応じた正の下限を与えます。

**入力と出力**

NとN行のx1 x2 x3 y。出力はa1 a2 a3 b sigma。

**実行コード：`mcmc_sample_07_unknown_sigma_v01.cpp`**

```cpp
// affine3の四係数と共通の標準偏差を同時に推定
// 問題: 測定のばらつきが毎回同程度だが、その大きさも不明。四係数と一つの共通sigmaを五つの未
// 知数として求める。
// モデル: 各行の損失を残差²/(2 sigma²)+log sigmaとする。log sigmaを省くと、sigmaを大きくす
// るだけで得をする誤った目的関数になる。
// 入出力: NとN行のx1 x2 x3 y。出力はa1 a2 a3 b sigma。
// 注意: 残差が完全に0ならsigmaを0へ寄せたくなるため、正の下限が必要。観測ごとに自由なsigma
// を一つずつ置く設計とは違う。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 標準状態McmcStateを使う推定器。
    using Input = std::array<double, 3>;
    // inputに三入力、valueに観測値を入れる。
    using Row = S::Observation<Input>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }
    // 観測行をn件用意。scale=1、weight=1が既定値。
    std::vector<Row> rows(n);
    // 観測を設定。
    for (auto& r : rows) {
        std::cin >> r.input[0] >> r.input[1] >> r.input[2] >> r.value;
    }

    S::Parameters p; // 未知変数の初期値と探索範囲を登録する。

    // a1の初期値0、上下限-10と10。提案幅は省略時に範囲から設定。
    const auto a1 = p.real(0, -10, 10);
    const auto a2 = p.real(0, -10, 10); // a2も実数。戻り値はreal配列内の添字。
    // a3も実数。係数の単位に応じて範囲を変更する。
    const auto a3 = p.real(0, -10, 10);
    const auto b = p.real(0, -10, 10); // 切片bの初期値と範囲。

    // 入力と候補状態から予測する純粋な関数。
    auto predict = [=](const Input& x, const McmcState& s) {
        // 登録した添字で未知係数を読む。
        return s.real[a1] * x[0] + s.real[a2] * x[1] + s.real[a3] * x[2] + s.real[b];
    };

    // 共通sigmaを正の実数として登録。下限は測定精度に合わせる。
    const auto sigma = p.real(0.5, 0.01, 3);

    // 予測後、現在のsigmaで損失を計算。
    auto loss = [=](const McmcState& s, const Row& row, double predicted) {
        // log sigmaを含む。row.scaleは使わない。
        return row.weight * S::gaussian_loss(row.value, predicted, s.real[sigma]);
    };

    // 独自損失を指定。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(rows, predict, loss));

    // 係数とsigmaを同じ予算で探索。
    const auto r = session.solve(S::Budget::for_steps(140'000));
    // 状態の有無を確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 四係数と共通sigma。
    for (auto j : {a1, a2, a3, b, sigma}) {
        std::cout << std::setprecision(10) << r.state->real[j] << ' ';
    }

    std::cout << '\n';
}
```

**入力例：`data/07.in`**

```text
8
-1 -1 -1 1.3
-1 -1 1 2.7
-1 1 -1 -4.3
-1 1 1 -3.7
1 -1 -1 5.7
1 -1 1 6.3
1 1 -1 -0.7
1 1 1 0.7
```

**出力例（実行確認時の標準出力）**

```text
1.999782104 -2.999706951 0.4998463789 1.000265235 0.1998383914
```

**使うときの注意**

残差が完全に0ならsigmaを0へ寄せたくなるため、正の下限が必要。観測ごとに自由なsigmaを一つずつ置く設計とは違う。

### 3.08 affine3の推定後、残差から標準偏差を計算

**具体的な問題**

共通sigmaが未知でも、四係数はまず残差の二乗和で推定できる。sigmaを探索に含めず、得られた残差から計算したい。

**モデルとの対応**

残差は観測値から予測値を引いたもの。二乗和SSEからsqrt(SSE/N)を求める。独立な正規誤差、四列の独立性、制約に当たらない最小二乗解などの条件を満たす場合はsqrt(SSE/(N−4))も併記できる。

共通sigma、等しい重み、事前項なしなら、係数をどれにするかは残差の二乗和で決められます。四係数を推定した後に、残差を計算します。

$$\mathrm{SSE}=\sum_ {i=1}^{N}(y_ {i}-\widehat y_ {i})^{2},\qquad \widehat\sigma_ {\mathrm{ML}}=\sqrt{\mathrm{SSE}/N}$$

SSEは残差の二乗和、Nは観測数、MLは観測の起こりやすさを最大にする推定の意味です。コードは `sqrt(SSE/(N-4))` も出します。四係数を同じ観測から合わせた分、残差が小さくなりやすいことを補う形です。ただし、独立な正規誤差、四つの入力列の独立性、境界に当たらない最小二乗解などが前提です。

`SSE/(N-4)` が不偏な**分散**の推定であり、その平方根がsigmaそのものの不偏推定になるわけではありません。探索誤差、実際の式との不一致、整数制約がある場合に、N−4だけを無条件に使わないでください。

**入力と出力**

N>4とN行のx1 x2 x3 y。出力は四係数、sigma_ML、sqrt(不偏分散)。

**実行コード：`mcmc_sample_08_residual_sigma_v01.cpp`**

```cpp
// affine3の推定後、残差から標準偏差を計算
// 問題: 共通sigmaが未知でも、四係数はまず残差の二乗和で推定できる。sigmaを探索に含めず、得
// られた残差から計算したい。
// モデル: 残差は観測値から予測値を引いたもの。二乗和SSEからsqrt(SSE/N)を求める。独立な正規
// 誤差、四列の独立性、制約に当たらない最小二乗解などの条件を満たす場合はsqrt(SSE/(N−4))も併
// 記できる。
// 入出力: N>4とN行のx1 x2 x3 y。出力は四係数、sigma_ML、sqrt(不偏分散)。
// 注意: 本例は等しい重み、事前項なし、整数制約なしを前提にする。N−4補正は、境界に張り付いた
// 解やランク不足の入力へ機械的に適用しない。推定の未収束やモデルのずれも残差を増やす。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 標準状態McmcStateを使う推定器。
    using Input = std::array<double, 3>;
    // inputに三入力、valueに観測値を入れる。
    using Row = S::Observation<Input>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }
    // 観測行をn件用意。scale=1、weight=1が既定値。
    std::vector<Row> rows(n);
    // 観測を設定。
    for (auto& r : rows) {
        std::cin >> r.input[0] >> r.input[1] >> r.input[2] >> r.value;
    }

    S::Parameters p; // 未知変数の初期値と探索範囲を登録する。

    // a1の初期値0、上下限-10と10。提案幅は省略時に範囲から設定。
    const auto a1 = p.real(0, -10, 10);
    const auto a2 = p.real(0, -10, 10); // a2も実数。戻り値はreal配列内の添字。
    // a3も実数。係数の単位に応じて範囲を変更する。
    const auto a3 = p.real(0, -10, 10);
    const auto b = p.real(0, -10, 10); // 切片bの初期値と範囲。

    // 入力と候補状態から予測する純粋な関数。
    auto predict = [=](const Input& x, const McmcState& s) {
        // 登録した添字で未知係数を読む。
        return s.real[a1] * x[0] + s.real[a2] * x[1] + s.real[a3] * x[2] + s.real[b];
    };

    // N-4を分母にするため。入力列の一次独立性も利用側で確かめる。
    assert(n > 4);

    // scale=1なので残差二乗和を最小化。
    auto session = S::make_numeric_session(p, S::make_observation_model(rows, predict));

    // 四係数だけを探索。
    const auto r = session.solve(S::Budget::for_steps(100'000));
    // 評価済みの結果か確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    double sse = 0;
    // 入力観測はコピーして残したので残差に再利用できる。
    for (const auto& row : rows) {
        // 各行の観測−予測。
        const double residual = row.value - session.model().prediction(row.input, *r.state);
        sse += residual * residual;
    }
    // 四係数を出力。
    for (auto j : {a1, a2, a3, b}) {
        std::cout << std::setprecision(10) << r.state->real[j] << ' ';
    }

    // 後者はsigmaそのものの不偏推定量ではない。
    std::cout << std::sqrt(sse / n) << ' ' << std::sqrt(sse / (n - 4)) << '\n';
}
```

**入力例：`data/08.in`**

```text
8
-1 -1 -1 1.3
-1 -1 1 2.7
-1 1 -1 -4.3
-1 1 1 -3.7
1 -1 -1 5.7
1 -1 1 6.3
1 1 -1 -0.7
1 1 1 0.7
```

**出力例（実行確認時の標準出力）**

```text
1.998356336 -3.000686288 0.5010687289 0.9988155033 0.2000142941 0.2828629274
```

**使うときの注意**

本例は等しい重み、事前項なし、整数制約なしを前提にする。N−4補正は、境界に張り付いた解やランク不足の入力へ機械的に適用しない。推定の未収束やモデルのずれも残差を増やす。

### 3.09 成功・失敗から成功確率を予測

**具体的な問題**

設定xを変えたときの成功・失敗しか観測できない。次の設定で成功する確率を求めたい。

**モデルとの対応**

ユーザー関数は確率そのものではなくz=ax+bを返す。BernoulliLogitがzを成功・失敗の損失へ変換し、mean_predictionが0〜1の確率へ戻す。

成功率は0〜1ですが、`a*x+b` はその範囲をはみ出します。そこでまず任意の実数zを計算し、次の変換で成功率にします。

$$z=ax+b,\qquad p=\frac{1}{1+\exp(-z)}$$

zが0ならpは0.5、zが大きいほど1へ、小さいほど0へ近づきます。`BernoulliLogit` に渡すのは変換前のzです。`mean_prediction` が変換後のpを返します。成功は1、失敗は0の観測行として渡します。

**入力と出力**

N、N行のx y（yは0または1）、予測したいq。出力はqでの生のzと成功確率。

**実行コード：`mcmc_sample_09_bernoulli_v01.cpp`**

```cpp
// 成功・失敗から成功確率を予測
// 問題: 設定xを変えたときの成功・失敗しか観測できない。次の設定で成功する確率を求めたい。
// モデル: ユーザー関数は確率そのものではなくz=ax+bを返す。BernoulliLogitがzを成功・失敗の損
// 失へ変換し、mean_predictionが0〜1の確率へ戻す。
// 入出力: N、N行のx y（yは0または1）、予測したいq。出力はqでの生のzと成功確率。
// 注意: 完全に成功・失敗を分離できるデータでは係数が大きくなりやすい。必要なら事前項や妥当
// な係数範囲を設ける。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    double q;
    std::cin >> q;

    // yは必ず0か1。
    auto model = S::make_observation_model(std::move(rows), predict, S::BernoulliLogit{});

    // 予測はlogitのまま渡す。
    auto session = S::make_numeric_session(p, std::move(model));

    // 成功・失敗の対数尤度を使って比較。
    const auto r = session.solve(S::Budget::for_steps(70'000));
    // 空なら予測用の状態がない。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // これは確率ではない。
    std::cout << std::setprecision(10) << session.model().prediction(q, *r.state) << ' ';

    // 0〜1の成功確率へ変換する。
    std::cout << session.model().mean_prediction(q, *r.state) << '\n';
}
```

**入力例：`data/09.in`**

```text
8
-1 0
-1 0
-1 0
-1 1
1 0
1 1
1 1
1 1
1
```

**出力例（実行確認時の標準出力）**

```text
1.09851275 0.749981336
```

**使うときの注意**

完全に成功・失敗を分離できるデータでは係数が大きくなりやすい。必要なら事前項や妥当な係数範囲を設ける。

### 3.10 観測時間が異なる発生件数

**具体的な問題**

機械の故障件数を、1時間、2時間など異なる長さで観測した。単位時間あたりの発生率を求め、次の観測時間での平均件数を予測する。

**モデルとの対応**

未知数uを対数発生率とし、生の予測はu+log(観測時間)。PoissonLogMeanが指数関数で平均件数へ変換する。

発生率をr、観測時間をhとすると、平均件数はr*hです。率は正にしたいので、未知の状態には `u=log(r)` を置きます。すると平均件数の対数は `u+log(h)` となります。

`PoissonLogMean` に渡すのはこの対数です。結果を率へ戻すには `exp(u)`、将来の時間hにおける平均件数には `mean_prediction(h,state)` を使います。観測した件数は整数ですが、予測する平均は2.5件のような実数でよい点も区別してください。

**入力と出力**

N、N行のtime count、次の観測時間q。time,q>0、countは0以上の整数。出力は発生率とq時間の平均件数。

**実行コード：`mcmc_sample_10_poisson_v01.cpp`**

```cpp
// 観測時間が異なる発生件数
// 問題: 機械の故障件数を、1時間、2時間など異なる長さで観測した。単位時間あたりの発生率を求
// め、次の観測時間での平均件数を予測する。
// モデル: 未知数uを対数発生率とし、生の予測はu+log(観測時間)。PoissonLogMeanが指数関数で平
// 均件数へ変換する。
// 入出力: N、N行のtime count、次の観測時間q。time,q>0、countは0以上の整数。出力は発生率とq
// 時間の平均件数。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 状態には対数発生率を一つ持つ。
    using Row = S::Observation<double>; // inputが観測時間、valueが件数。

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }
    // scaleはPoisson損失では使わず、weight=1を利用。
    std::vector<Row> rows(n);
    // 時間は正。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
        assert(r.input > 0);
    }

    double q;
    std::cin >> q;
    assert(q > 0);

    S::Parameters p; // 発生率そのものではなく対数を登録。
    // 発生率はexp(-5)〜exp(5)、初期値はexp(0)=1。
    const auto u = p.real(0, -5, 5);

    // 平均件数の対数を返す。
    auto predict = [=](double time, const McmcState& s) {
        return s.real[u] + std::log(time);
    };

    // 件数に合う損失。
    auto session = S::make_numeric_session(
        p, S::make_observation_model(rows, predict, S::PoissonLogMean{}));

    // 一つの未知数を探索。
    const auto r = session.solve(S::Budget::for_steps(30'000));
    // 候補の存在を確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 単位時間の発生率。
    std::cout << std::setprecision(10) << std::exp(r.state->real[u]) << ' ';

    // 生の対数予測ではなく平均件数を使う。
    std::cout << session.model().mean_prediction(q, *r.state) << '\n';
}
```

**入力例：`data/10.in`**

```text
3
1 2
2 4
3 6
5
```

**出力例（実行確認時の標準出力）**

```text
2.000015584 10.00007792
```

### 3.11 二つの出力・一部欠測・誤差の相関

**具体的な問題**

位置センサーが横と縦の二座標を返すが、片方が欠けることがある。両方あるときの誤差には相関もあり、それを考慮して倍率とずれを推定したい。

**モデルとの対応**

予測は(a*x+bx, a*x+by)。欠測は0で埋めて採点せず、その座標の項を除く。両方あるときは相関rho=0.3の二次元正規誤差を使う。

二成分のうち欠けた方は「値が0」ではなく「知らない」ので、`optional<double>` の空として保持します。予測は二成分の配列です。片方しかなければ、その一成分の誤差だけ採点します。

両方あるとき、二つの標準化残差をu、vとすると、中心となる罰は `(u*u-2*rho*u*v+v*v)/(2*(1-rho*rho))` です。rhoが正なので、両方が同じ向きにずれることを、独立と仮定したときより起こりやすく扱います。sigma=0.2、rho=0.3は既知の固定値です。コードは誤差尺度の項も含めていますが、欠測している観測そのものを推測して埋めてはいません。

**入力と出力**

NとN行のx has_x observed_x has_y observed_y。hasは0/1、欠測側の値は読み捨てる。出力はa bx by。

**実行コード：`mcmc_sample_11_missing_vector_v01.cpp`**

```cpp
// 二つの出力・一部欠測・誤差の相関
// 問題: 位置センサーが横と縦の二座標を返すが、片方が欠けることがある。両方あるときの誤差に
// は相関もあり、それを考慮して倍率とずれを推定したい。
// モデル: 予測は(a*x+bx, a*x+by)。欠測は0で埋めて採点せず、その座標の項を除く。両方あるとき
// は相関rho=0.3の二次元正規誤差を使う。
// 入出力: NとN行のx has_x observed_x has_y observed_y。hasは0/1、欠測側の値は読み捨てる。出
// 力はa bx by。
// 注意: 同じ観測行の中の相関は独自損失で扱える。異なる観測行どうしが相関する場合は、全体評
// 価の例を使う。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

// 独自観測でもinputというメンバーを用意する。
struct Row {
    double input;
    std::array<std::optional<double>, 2> value;
};

int main() {
    using S = McmcEstimator<>; // 予測の戻り値は独自損失で配列として扱える。

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n);
    for (auto& r : rows) {
        int hx, hy;
        double x, y;
        std::cin >> r.input >> hx >> x >> hy >> y;
        if (hx) {
            r.value[0] = x;
        }

        if (hy) {
            r.value[1] = y;
        }
    }

    S::Parameters p; // 共有倍率と二方向のずれ。
    const auto a = p.real(0, -10, 10); // aの範囲。
    const auto bx = p.real(0, -10, 10); // 横のずれ。
    const auto by = p.real(0, -10, 10); // 縦のずれ。

    // 二成分を返す。
    auto predict = [=](double x, const McmcState& s) {
        return std::array<double, 2>{s.real[a] * x + s.real[bx],
                                     s.real[a] * x + s.real[by]};
    };

    // 配列と欠測を解釈する独自損失。
    auto loss = [](const McmcState&, const Row& row, const std::array<double, 2>& y) {
        const double sigma = 0.2, rho = 0.3;
        if (row.value[0] && row.value[1]) {
            const double x = (*row.value[0] - y[0]) / sigma,
                         z = (*row.value[1] - y[1]) / sigma;
            return (x * x - 2 * rho * x * z + z * z) / (2 * (1 - rho * rho))
                   + 2 * std::log(sigma) + 0.5 * std::log(1 - rho * rho);
        }

        double e = 0;
        // 一成分ならその周辺誤差だけ。
        for (int j = 0; j < 2; ++j) {
            if (row.value[j]) {
                e += S::gaussian_loss(*row.value[j], y[j], sigma);
            }
        }

        return e; // 両方欠測なら0。どの候補も同じ欠測の扱いにする。
    };

    // 標準Gaussianではなく独自損失を渡す。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(rows, predict, loss));

    // 共有する係数を同時に探す。
    const auto r = session.solve(S::Budget::for_steps(100'000));
    // 結果確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 三係数を出力。
    for (auto j : {a, bx, by}) {
        std::cout << std::setprecision(10) << r.state->real[j] << ' ';
    }

    std::cout << '\n';
}
```

**入力例：`data/11.in`**

```text
5
0 1 1 1 -1
1 1 3 1 1
2 1 5 0 0
3 0 0 1 5
4 1 9 1 7
```

**出力例（実行確認時の標準出力）**

```text
2.00005993 1.000020857 -1.000024279
```

**使うときの注意**

同じ観測行の中の相関は独自損失で扱える。異なる観測行どうしが相関する場合は、全体評価の例を使う。

### 3.12 再現シミュレーションから機械速度を推定

**具体的な問題**

二台の機械のどちらかへ仕事を投入し、各実験の最後の完了時刻だけを記録した。投入時刻と仕事量から、未知の処理速度を推定する。

**モデルとの対応**

一回の観測入力を仕事列とし、指定された速度で待ち行列を再現する。最後の完了時刻を一つの観測値と比較する。

一台の機械は、前の仕事が終わる前に次の仕事を処理できません。次の仕事の開始時刻は「その仕事の到着時刻」と「機械が空く時刻」の大きい方です。そこに `仕事量/速度` を足すと完了時刻になります。

一行のinputが一つの数である必要はありません。本例では一回の実験の仕事列全体を渡し、二台の終了時刻の大きい方を観測値と比較します。再現のたびに機械の空き時刻を0へ戻すので、呼び出し順で採点が変わりません。仕事量と到着時刻は非負、機械IDは0または1です。

**入力と出力**

N回の観測。各回はK、K行のmachine arrival work、最後にobserved_finish。machineは0/1、arrivalとworkは非負。出力は二台の速度。

**実行コード：`mcmc_sample_12_simulator_v01.cpp`**

```cpp
// 再現シミュレーションから機械速度を推定
// 問題: 二台の機械のどちらかへ仕事を投入し、各実験の最後の完了時刻だけを記録した。投入時刻
// と仕事量から、未知の処理速度を推定する。
// モデル: 一回の観測入力を仕事列とし、指定された速度で待ち行列を再現する。最後の完了時刻を
// 一つの観測値と比較する。
// 入出力: N回の観測。各回はK、K行のmachine arrival work、最後にobserved_finish。machineは
// 0/1、arrivalとworkは非負。出力は二台の速度。
// 注意: 仕事はその機械に投入する順に並べる。予測内で毎回違う乱数を発生させない。確率的な再
// 現を使うなら固定した乱数列を入力側で用意し、同じ状態の評価が変わらないようにする。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

struct Job {
    int machine;
    double arrival, work;
};

int main() {
    using S = McmcEstimator<>; // 状態に機械ごとの速度を持つ。
    // 一行のinputに仕事列を丸ごと持てる。
    using Row = S::Observation<std::vector<Job>>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 観測された最終完了時刻をvalueへ格納。
    // 既知の仕事列と結果。
    for (auto& r : rows) {
        int k;
        std::cin >> k;
        r.input.resize(k);
        for (auto& j : r.input) {
            std::cin >> j.machine >> j.arrival >> j.work;
        }

        std::cin >> r.value;
    }

    S::Parameters p; // 速度は0を許さない。
    p.real(1, 0.2, 6); // machine=0の速度、初期値1。
    p.real(1, 0.2, 6); // machine=1の速度、初期値1。

    // 毎回同じ初期状態から再現する。
    auto predict = [](const std::vector<Job>& jobs, const McmcState& s) {
        std::array<double, 2> ready{};
        // 候補速度で所要時間を計算。
        for (const auto& j : jobs) {
            ready[j.machine] =
                std::max(ready[j.machine], j.arrival) + j.work / s.real[j.machine];
        }

        return std::max(ready[0], ready[1]);
    };

    // シミュレータも通常の予測関数として接続。
    auto session = S::make_numeric_session(p, S::make_observation_model(rows, predict));

    // 再現計算の重さに合わせて実問題では時間予算も使う。
    const auto r = session.solve(S::Budget::for_steps(80'000));
    // 最良候補確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 二台の推定速度。
    std::cout << std::setprecision(10) << r.state->real[0] << ' ' << r.state->real[1]
              << '\n';
}
```

**入力例：`data/12.in`**

```text
4
1
0 0 4
2
1
1 0 6
2
2
0 0 2
0 0 4
3
2
1 0 3
1 0 6
3
```

**出力例（実行確認時の標準出力）**

```text
2.000034423 3.000378032
```

**使うときの注意**

仕事はその機械に投入する順に並べる。予測内で毎回違う乱数を発生させない。確率的な再現を使うなら固定した乱数列を入力側で用意し、同じ状態の評価が変わらないようにする。

### 3.13 ターンごとの観測追加・訂正・古い観測の削除

**具体的な問題**

一度に全部の測定が手に入らず、ターンごとに情報が増える。前の候補を残して更新し、記録ミスの訂正や古い観測の除外も行う。

**モデルとの対応**

予測はax+b。同じSessionへ新しい行だけを追加する。訂正時は残す全行へ置換し、必要なら最後のK件だけを残す。

同じ未知係数について測定が増えるなら、同じSessionへ新しい行だけを加えます。例えば二件から三件へ増えたとき、三件全部をaddすると最初の二件を重複して数えてしまいます。

訂正は `replace_observations` で「訂正後に残す全体」を渡し、直近だけ使いたいときは `retain_last` で削ります。追加は既存の計算を利用できる場合があり、訂正・削除は全再評価です。いずれも候補状態を開始の手掛かりとして再利用します。`revision` が変わったら、以前の標本平均は別の目的関数の集計です。

**入力と出力**

T K、各ターンにNとN行のx y、最後にMと訂正済みM行。各ターンと最後にa b 観測数を出力。N>0、M>=K>=2で傾きを区別できる入力を使う。

**実行コード：`mcmc_sample_13_online_v01.cpp`**

```cpp
// ターンごとの観測追加・訂正・古い観測の削除
// 問題: 一度に全部の測定が手に入らず、ターンごとに情報が増える。前の候補を残して更新し、記
// 録ミスの訂正や古い観測の除外も行う。
// モデル: 予測はax+b。同じSessionへ新しい行だけを追加する。訂正時は残す全行へ置換し、必要な
// ら最後のK件だけを残す。
// 入出力: T K、各ターンにNとN行のx y、最後にMと訂正済みM行。各ターンと最後にa b 観測数を出
// 力。N>0、M>=K>=2で傾きを区別できる入力を使う。
// 注意: 以前の標本集計は新しいrevisionへ混ぜない。変数数や種類の意味を変える場合は、Session
// を再構築する。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 同じSessionを全ターンで再利用する。
    using Row = S::Observation<double>; // 入力xと観測y。
    int turns, keep;
    if (!(std::cin >> turns >> keep)) {
        return 1;
    }

    S::Parameters p; // 未知のa,bを一度だけ登録。
    const auto a = p.real(0, -10, 10); // aの初期値と範囲。
    const auto b = p.real(0, -10, 10); // bの初期値と範囲。

    // 全ターンで同じ意味の係数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    // 空の観測から開始。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(std::vector<Row>{}, predict));
    for (int turn = 0; turn < turns; ++turn) {
        int n;
        std::cin >> n;
        assert(n > 0);
        std::vector<Row> batch(n); // 今回新たに得た観測だけを入れる。
        // x,yを設定。
        for (auto& row : batch) {
            std::cin >> row.input >> row.value;
        }

        const auto old = session.revision(); // 変更前のモデル番号を保存。
        // 一度だけ追加。評価は次のsolve内で行われる。
        session.add_observations(std::move(batch));
        // 非空の追加では番号が変わる。空追加は変更しない。
        assert(session.revision() != old);

        // 既存候補を現在の観測で再評価して継続。
        const auto r = session.solve(S::Budget::for_steps(40'000));
        // 更新直後は候補が一時的に空でもよい。
        if (!r.state) {
            std::cout << "NO_ESTIMATE\n";
            return 0;
        }

        // ターンの推定値を送る。
        std::cout << std::setprecision(10) << r.state->real[a] << ' ' << r.state->real[b]
                  << ' ' << session.model().size() << std::endl;
    }

    int n;
    std::cin >> n;
    std::vector<Row> corrected(n); // 追加分でなく、訂正後に残す全観測。
    // 正しい値を読み直す。
    for (auto& row : corrected) {
        std::cin >> row.input >> row.value;
    }
    // 既存の全行を置換し、古い評価を無効化。
    session.replace_observations(std::move(corrected));
    // 最後のkeep件だけを保持。候補を捨てて最初から作り直す必要はない。
    session.retain_last(keep);

    // 二つの変更をまとめて再評価。
    const auto r = session.solve(S::Budget::for_steps(60'000));
    // 結果確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 観測ビューは更新まで有効。
    std::cout << r.state->real[a] << ' ' << r.state->real[b] << ' '
              << session.model().observations().size() << '\n';
}
```

**入力例：`data/13.in`**

```text
3 3
2
0 1
1 3
1
2 5
1
3 7
4
0 2
1 4
2 6
3 8
```

**出力例（実行確認時の標準出力）**

```text
1.999989903 0.9998037823 2
1.999989903 0.9998037823 3
1.999885176 1.000117999 4
2.000487116 1.998691881 3
```

**使うときの注意**

以前の標本集計は新しいrevisionへ混ぜない。変数数や種類の意味を変える場合は、Sessionを再構築する。

### 3.14 既知の地図データを訂正して再推定

**具体的な問題**

道路の長さと通過時間から距離あたりの所要時間を推定したい。道路長に登録ミスが見つかったので、過去の観測も正しい長さで評価し直す。

**モデルとの対応**

Contextに各道路の長さを持ち、入力は通った道路IDの列。予測は経路の長さ合計×未知係数a。Contextをモデルごと所有させ、update_modelで訂正する。

道路の長さは既知のデータ、距離あたりの所要時間aだけが未知です。観測行には道路IDの列を持ち、長さの一覧はContextへ一つだけ持たせます。予測関数は `context,input,state` の三つを使います。

訂正は `session.update_model` の中で `model.context()` を書き換えます。この例は実際の道路が伸びたのではなく、保存した長さの誤りを直す話です。従って過去の測定も新しい長さで評価し直します。道路IDは0以上D未満、長さは正とします。実際の環境が変化した場合との違いをコードにも記載しています。

**入力と出力**

道路数DとD個の長さ、観測数N、各観測はKとK個の道路IDと所要時間。続いて訂正IDと正しい長さ、予測経路のKとID列。出力はa、経路の予測時間、訂正後の長さ。

**実行コード：`mcmc_sample_14_context_v01.cpp`**

```cpp
// 既知の地図データを訂正して再推定
// 問題: 道路の長さと通過時間から距離あたりの所要時間を推定したい。道路長に登録ミスが見つか
// ったので、過去の観測も正しい長さで評価し直す。
// モデル: Contextに各道路の長さを持ち、入力は通った道路IDの列。予測は経路の長さ合計×未知係
// 数a。Contextをモデルごと所有させ、update_modelで訂正する。
// 入出力: 道路数DとD個の長さ、観測数N、各観測はKとK個の道路IDと所要時間。続いて訂正IDと正し
// い長さ、予測経路のKとID列。出力はa、経路の予測時間、訂正後の長さ。
// 注意: これは誤登録の訂正。過去と現在で実際の道路条件が変わったなら、過去の観測には当時の
// Contextまたは不変IDを持たせる。過去の環境を自動保存する機能はない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

struct Map {
    std::vector<double> length;
};

int main() {
    using S = McmcEstimator<>; // Contextのある観測モデルを利用。
    using Row = S::Observation<std::vector<int>>; // inputは経路上の道路ID列。
    int d;
    if (!(std::cin >> d)) {
        return 1;
    }

    Map map;
    map.length.resize(d);
    for (auto& x : map.length) {
        std::cin >> x;
    }

    int n;
    std::cin >> n;
    std::vector<Row> rows(n); // 各経路の観測時間。
    // 距離ではなくIDを保存。
    for (auto& row : rows) {
        int k;
        std::cin >> k;
        row.input.resize(k);
        for (int& id : row.input) {
            std::cin >> id;
        }

        std::cin >> row.value;
    }

    int id;
    double length;
    std::cin >> id >> length;
    int k;
    std::cin >> k;
    std::vector<int> query(k);
    for (int& v : query) {
        std::cin >> v;
    }

    S::Parameters p; // 距離あたりの時間は正とする。
    const auto a = p.real(1, 0.1, 10); // 初期値1、下限0.1、上限10。

    // Context、入力、状態の順。
    auto predict = [=](const Map& c, const std::vector<int>& route, const McmcState& s) {
        double distance = 0;
        for (int v : route) {
            distance += c.length[v];
        }

        return distance * s.real[a]; // 候補aで時間を再現。
    };

    // 地図も値として所有。
    auto model = S::make_context_model(rows, std::move(map), predict);

    // 観測・地図・候補をまとめて管理。
    auto session = S::make_numeric_session(p, std::move(model));
    // 訂正前に一度推定した状況を再現。
    session.solve(S::Budget::for_steps(20'000));
    // この中でだけ所有Contextを編集し、再評価を予約。
    session.update_model([&](auto& m) {
        m.context().length[id] = length;
    });

    // 以前の候補を訂正後の地図で再評価。
    const auto r = session.solve(S::Budget::for_steps(30'000));
    // 有効な候補を確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 新しい地図で予測。
    std::cout << std::setprecision(10) << r.state->real[a] << ' '
              << session.model().prediction(query, *r.state) << ' ';

    // constな地図参照で訂正を確認。
    std::cout << session.model().context().length[id] << '\n';
}
```

**入力例：`data/14.in`**

```text
2
2 3
2
1 0 4
1 1 8
1 4
2 0 1
```

**出力例（実行確認時の標準出力）**

```text
2.000001349 12.0000081 4
```

**使うときの注意**

これは誤登録の訂正。過去と現在で実際の道路条件が変わったなら、過去の観測には当時のContextまたは不変IDを持たせる。過去の環境を自動保存する機能はない。

### 3.15 計算を分割し、残り時間だけ追加で使う

**具体的な問題**

まず決まった回数だけ推定し、その後に余った時間で改善する。時間予算と遷移数予算を使い分け、結果のコピーを省く方法も確認する。

**モデルとの対応**

ax+bを推定するSessionを再利用する。分割前から全体の冷却予定回数を設定し、Sessionに渡すBudgetが値渡しであることを確かめる。

遷移数予算は、同じ条件の実行を比べたいときに使いやすい指定です。マイクロ秒予算は、コンテストの残り時間を使いたいときに向いています。この例はまず1万回ずつ二度進め、その後に時間予算を二通りで使います。

結果の大きな状態をコピーする必要がなければ、`solve_view` のReportと `best()` のポインタを使います。ただし、次に計算を進めた後は以前のポインタを使いません。必要な係数だけ値として保存するか、所有する結果が欲しければsolveを使います。

**入力と出力**

NとN行のx y、追加予算us（0以上のマイクロ秒）。出力はa b、Budgetが値渡しであった確認値1、追加の回数上限を守った確認値1。

**実行コード：`mcmc_sample_15_budget_v01.cpp`**

```cpp
// 計算を分割し、残り時間だけ追加で使う
// 問題: まず決まった回数だけ推定し、その後に余った時間で改善する。時間予算と遷移数予算を使
// い分け、結果のコピーを省く方法も確認する。
// モデル: ax+bを推定するSessionを再利用する。分割前から全体の冷却予定回数を設定し、Session
// に渡すBudgetが値渡しであることを確かめる。
// 入出力: NとN行のx y、追加予算us（0以上のマイクロ秒）。出力はa b、Budgetが値渡しであった確
// 認値1、追加の回数上限を守った確認値1。
// 注意: 二つの時間付き呼び出しにはそれぞれusを与えるため、追加時間の合計は最大でおおむね2us。
// 共通の競技締切にするなら同じ絶対期限を渡す。一回のコールバックは途中で強制停止できない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    std::int64_t us;
    std::cin >> us;
    assert(us >= 0);

    S::Param param; // 省略した設定は標準値を利用。
    // 呼び出しを分割する前に、共通の冷却予定回数を決める。
    param.cooling_steps = 30'000;

    // 設定を渡して構築。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(rows, predict), param);
    session.solve(S::Budget::for_steps(10'000)); // 最初の1万遷移。

    auto budget = S::Budget::for_steps(10'000); // 呼び出し側に保持する予算。

    // 状態をコピーせずReportだけ受け取る。
    const auto report = session.solve_view(budget);
    // Sessionは予算を値渡しする。
    const bool copied = budget.remaining_steps == 10'000 && report.steps == 10'000;
    auto timed = S::Budget::for_us(us); // 作成時点からusマイクロ秒の期限。
    // 回数と時間の両方に余裕があれば開始。
    if (timed.available()) {
        session.solve_view(timed);
    }
    // 単調時計で絶対期限を作る。
    const auto end = S::Clock::now() + std::chrono::microseconds(us);
    // 絶対期限と1000遷移上限の早い方で停止。
    const auto more = session.solve_view(S::Budget::until(end, 1000));

    // コピーしないビュー。次の実行・変更前に使い切る。
    const auto best = session.best();
    // ポインタが空なら参照しない。
    if (!best.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 候補と確認値。
    std::cout << std::setprecision(10) << best.state->real[a] << ' ' << best.state->real[b]
              << ' ' << copied << ' ' << (more.steps <= 1000) << '\n';
    (void)timed.time_left(); // 期限だけの判定。remaining_stepsは調べない。
}
```

**入力例：`data/15.in`**

```text
4
0 1
1 3
2 5
3 7
1000
```

**出力例（実行確認時の標準出力）**

```text
1.999689575 1.000318034 1 1
```

**使うときの注意**

二つの時間付き呼び出しにはそれぞれusを与えるため、追加時間の合計は最大でおおむね2us。共通の競技締切にするなら同じ絶対期限を渡す。一回のコールバックは途中で強制停止できない。

### 3.16 予測の平均とばらつきを求める

**具体的な問題**

測定に誤差があり、一つの係数だけに決めると次の所要時間を断定し過ぎる。あり得る係数を使って二つの入力の予測を繰り返し、平均と分散を求める。

**モデルとの対応**

ax+b、観測の既知sigma=0.5。各標本から二つの予測をarrayとして返すと、成分ごとに集計される。分散は係数の不確かさによる予測平均のばらつき。

一つの標本は、あり得るa,bの一組です。その一組からq1、q2で予測し、二つの値の配列を返します。平均は予測の中心、分散は予測がどのくらい散らばるかを二乗の単位で表します。元の単位の幅が欲しければ分散の平方根です。

ここで集計するのは、誤差を除いた予測平均の不確かさです。次回の実測値にはさらに測定誤差があります。既知sigma=0.5の独立な測定誤差なら、実測値の分散を考えるときは予測の分散に0.25を足します。予算42000のうち2000が準備、残り40000で2遷移ごとに記録するため、この一列の例では20000標本になります。

**入力と出力**

NとN行のx y、二つの予測入力q1 q2。出力は二つの平均、二つの分散、標本数、準備残量。

**実行コード：`mcmc_sample_16_posterior_v01.cpp`**

```cpp
// 予測の平均とばらつきを求める
// 問題: 測定に誤差があり、一つの係数だけに決めると次の所要時間を断定し過ぎる。あり得る係数
// を使って二つの入力の予測を繰り返し、平均と分散を求める。
// モデル: ax+b、観測の既知sigma=0.5。各標本から二つの予測をarrayとして返すと、成分ごとに集
// 計される。分散は係数の不確かさによる予測平均のばらつき。
// 入出力: NとN行のx y、二つの予測入力q1 q2。出力は二つの平均、二つの分散、標本数、準備残量。
// 注意: 将来の実測値のばらつきには、さらに観測ノイズが加わる。独立なsigma=0.5のノイズなら分
// 散に0.25を加える。標本は互いに相関し得るので、標本数を独立標本数や精度保証と解釈しない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    double q1, q2;
    std::cin >> q1 >> q2;
    // 既知の観測ノイズの標準偏差。
    for (auto& row : rows) {
        row.scale = 0.5;
    }

    S::Param param; // 標本採取の設定。
    param.warmup_steps = 2000; // 準備の総遷移数。十分な混合の保証ではない。
    param.sample_stride = 2; // 各鎖で2遷移ごとに一標本を記録。

    // 損失の尺度を保つ。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(rows, predict), param);
    // まず良い係数付近へ移動する。
    session.solve(S::Budget::for_steps(30'000));

    // 準備2000と採取40000を同じ予算で行う。
    const auto r = session.sample(S::Budget::for_steps(42'000), [&](const McmcState& s) {
        // 平均する値を標本ごとに計算する。
        return std::array<double, 2>{predict(q1, s), predict(q2, s)};
    });
    // optional<array>への参照。0標本なら空。
    const auto& mean = r.summary.mean();
    // optional<array>のコピー。2標本未満なら空。
    const auto variance = r.summary.variance();
    // 空を確認してから読む。
    if (!mean || !variance) {
        std::cout << "NOT_ENOUGH_SAMPLES\n";
        return 0;
    }

    // 予測の統計。
    std::cout << std::setprecision(10) << (*mean)[0] << ' ' << (*mean)[1] << ' '
              << (*variance)[0] << ' ' << (*variance)[1] << ' ';

    // 今回の標本数と残る準備遷移数。
    std::cout << r.summary.count() << ' ' << r.warmup_remaining << '\n';
    std::cerr << r << '\n'; // SampleResult全体をデバッグ表示できる。
}
```

**入力例：`data/16.in`**

```text
4
0 1
1 3
2 5
3 7
4 5
```

**出力例（実行確認時の標準出力）**

```text
9.00848597 11.01097498 0.36905461 0.6646580073 20000 0
```

**使うときの注意**

将来の実測値のばらつきには、さらに観測ノイズが加わる。独立なsigma=0.5のノイズなら分散に0.25を加える。標本は互いに相関し得るので、標本数を独立標本数や精度保証と解釈しない。

### 3.17 複数回の標本採取を自分で集計

**具体的な問題**

限られた時間を何回かに分けて使いながら、同じモデルの予測統計を累積したい。全標本を保存せず、平均と分散だけを保持する。

**モデルとの対応**

sampleは呼び出しごとに新しい集計を返すため、sample_eachの通知を外部のMomentsへ加える。目的関数と予測条件が同じ間だけ累積する。

`sample` が返すsummaryは毎回新しく作られます。複数回を合計するには `sample_each` で全標本の通知を受け、自分のMomentsへ入れます。Momentsは平均と分散を更新するだけなので、件数が増えても全標本を保存するメモリは要りません。

二回の15000遷移の合計は30000、初回から通算して準備1000を除き、5遷移ごとに記録するので5800標本です。q1/q2を変えた後や、観測を追加した後は、同じsummaryへ足し続けません。

**入力と出力**

NとN行のx y、予測入力q1 q2。出力は二つの累積平均と標本数。

**実行コード：`mcmc_sample_17_stream_samples_v01.cpp`**

```cpp
// 複数回の標本採取を自分で集計
// 問題: 限られた時間を何回かに分けて使いながら、同じモデルの予測統計を累積したい。全標本を
// 保存せず、平均と分散だけを保持する。
// モデル: sampleは呼び出しごとに新しい集計を返すため、sample_eachの通知を外部のMomentsへ加
// える。目的関数と予測条件が同じ間だけ累積する。
// 入出力: NとN行のx y、予測入力q1 q2。出力は二つの累積平均と標本数。
// 注意: 良い候補だけ、受理された標本だけ、という選別をすると分布が変わる。emit内からsolveや
// update_modelを再入呼び出ししない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    double q1, q2;
    std::cin >> q1 >> q2;
    // 正規誤差の既知sigma。
    for (auto& row : rows) {
        row.scale = 0.5;
    }

    S::Param param; // 分割しても準備残量はSessionが保持する。
    param.warmup_steps = 1000; // 全呼び出しを通算した準備遷移数。
    param.sample_stride = 5; // 5遷移ごとに記録。

    // 一度だけ構築。
    auto session =
        S::make_numeric_session(p, S::make_observation_model(rows, predict), param);
    using Session = decltype(session); // MomentsはこのSessionの公開型。
    // 0件から始まる、二成分の逐次集計。
    Session::Moments<std::vector<double>> summary;
    // 集計の対象であるモデル番号を記録。
    const auto revision = session.revision();
    for (int part = 0; part < 2; ++part) {
        // モデルが変わった場合は新しいsummaryを作る。
        assert(session.revision() == revision);

        // 採取した各標本を通知。
        const auto report = session.sample_each(
            S::Budget::for_steps(15'000),
            [&](const McmcState& s, double energy, int chain) {
                // 目的関数値と0始まりの鎖番号。必要なら独自保存に使う。
                (void)energy;
                (void)chain;
                // 長さを変えず、棄却された遷移の標本も全て加える。
                summary.add(std::vector<double>{predict(q1, s), predict(q2, s)});
            });
        std::cerr << report << '\n'; // 今回分の準備・採取の実行情報。
    }
    // 平均が存在するか確認。
    if (!summary.mean()) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    // 通算5800標本。
    std::cout << std::setprecision(10) << (*summary.mean())[0] << ' '
              << (*summary.mean())[1] << ' ' << summary.count() << '\n';
    std::cerr << summary << '\n'; // count・mean・varianceを一行で確認。
}
```

**入力例：`data/17.in`**

```text
4
0 1
1 3
2 5
3 7
4 5
```

**出力例（実行確認時の標準出力）**

```text
8.991715103 10.98453064 5800
```

**使うときの注意**

良い候補だけ、受理された標本だけ、という選別をすると分布が変わる。emit内からsolveやupdate_modelを再入呼び出ししない。

### 3.18 異なる初期値から複数の候補列を動かす

**具体的な問題**

係数同士が補い合う問題で、一つの開始位置だけに頼りたくない。異なる初期値を持つ複数の鎖を使い、候補間の情報も利用する。

**モデルとの対応**

ax+bを三鎖で扱う。make_numeric_sessionのchainsで鎖数を指定し、restartで三つの異なる状態を入れる。sampleでは差分提案と伸縮提案を有効にする。

候補列を三本持ち、開始点を少しずつ変えます。`make_numeric_session` の列数指定だけでは同じ初期値が三個作られるので、この例ではrestartで異なる値を設定しています。

標本採取では、一つの列が他の列の位置も手掛かりにします。そのため、三本を完全に独立した実験と見なしてはいけません。予算の遷移数は三本合計であり、「60000を各列に60000ずつ」ではありません。出力には最良候補と標本平均の両方を出し、目的が違う値であることも確認できます。

**入力と出力**

NとN行のx y。出力は最良候補のa b、標本平均のa b、標本数。

**実行コード：`mcmc_sample_18_multiple_chains_v01.cpp`**

```cpp
// 異なる初期値から複数の候補列を動かす
// 問題: 係数同士が補い合う問題で、一つの開始位置だけに頼りたくない。異なる初期値を持つ複数
// の鎖を使い、候補間の情報も利用する。
// モデル: ax+bを三鎖で扱う。make_numeric_sessionのchainsで鎖数を指定し、restartで三つの異な
// る状態を入れる。sampleでは差分提案と伸縮提案を有効にする。
// 入出力: NとN行のx y。出力は最良候補のa b、標本平均のa b、標本数。
// 注意: 確率の合計は1以下。これらの集団提案はSearchでは使われない。restartでは学習済みの数
// 値提案幅を保持するため、完全に新しい設定にするならSessionを再構築する。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    // 標本の広がりを決める観測精度。
    for (auto& row : rows) {
        row.scale = 0.5;
    }

    S::Param param; // 鎖をまたいだ総遷移数で予算を数える。
    param.warmup_steps = 3000; // 三鎖を合わせた準備量。

    S::MoveParam move; // 標準の数値提案を調整する。
    // Warmup/Sampleで他の二鎖の差を使う割合。3鎖以上が必要。
    move.difference_probability = 0.3;
    // 他の一鎖を中心に伸縮する割合。2鎖以上が必要。
    move.stretch_probability = 0.3;

    // 最初は同一初期値の3鎖。
    auto session = S::make_numeric_session(
        p, S::make_observation_model(rows, predict), param, move, 3);
    // 登録済みの状態の形をコピーする。
    std::vector<McmcState> initial(3, p.initial());
    initial[0].real = {-2, 4}; // a,bの順序と範囲を維持する。
    initial[1].real = {0, 0}; // 2本目の初期値。
    initial[2].real = {4, -2}; // 3本目の初期値。
    // 乱数・履歴を初期化。モデルと設定は維持する。
    session.restart(std::move(initial));

    // 最良候補のコピーは省く。
    const auto report = session.solve_view(S::Budget::for_steps(60'000));
    // ビューを参照してよいか確認。
    if (!report.has_estimate) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    const auto best = session.best(); // 次の実行まで有効な参照。
    // 次のsample前に必要な数値をコピー。
    const double best_a = best.state->real[a], best_b = best.state->real[b];

    // 準備後の係数を集計。
    const auto r = session.sample(S::Budget::for_steps(60'000), [=](const McmcState& s) {
        return std::array<double, 2>{s.real[a], s.real[b]};
    });
    // 0標本なら平均は空。
    if (!r.summary.mean()) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    // 57000標本。
    std::cout << std::setprecision(10) << best_a << ' ' << best_b << ' '
              << (*r.summary.mean())[0] << ' ' << (*r.summary.mean())[1] << ' '
              << r.summary.count() << '\n';
}
```

**入力例：`data/18.in`**

```text
4
0 1
1 3
2 5
3 7
```

**出力例（実行確認時の標準出力）**

```text
1.999899376 1.000377498 1.997962173 1.002704024 57000
```

**使うときの注意**

確率の合計は1以下。これらの集団提案はSearchでは使われない。restartでは学習済みの数値提案幅を保持するため、完全に新しい設定にするならSessionを再構築する。

### 3.19 連続する測定の誤差をまとめて評価

**具体的な問題**

時系列のセンサーで、前回の測定誤差が次回にも残る。各行を独立とみなすと同じ情報を重ねて数えてしまう。

**モデルとの対応**

残差をeとし、次の残差はrho倍した前の残差の周りにばらつくと考える。観測列全体を受け持つModelで、初回と隣り合う残差の組を採点する。

まず各行の残差eを計算します。次に、二行目以降では「今回のe−0.4×前回のe」を誤差として採点します。前回のずれの一部が次にも残ると考えるためです。

最初の一行だけは前回がないので、長く同じ仕組みで動いていたときの標準偏差 `0.2/sqrt(1-0.4*0.4)` を使います。行ごとの独立な損失には分けず、順番も含めて全体を一つのModelが所有します。全体評価の一回の呼び出しの内部は、時計で自動分割されない点に注意してください。

**入力と出力**

N>0と時刻順のN行のx y。出力はa b。rho=0.4、変化分のsigma=0.2を固定。

**実行コード：`mcmc_sample_19_correlated_rows_v01.cpp`**

```cpp
// 連続する測定の誤差をまとめて評価
// 問題: 時系列のセンサーで、前回の測定誤差が次回にも残る。各行を独立とみなすと同じ情報を重
// ねて数えてしまう。
// モデル: 残差をeとし、次の残差はrho倍した前の残差の周りにばらつくと考える。観測列全体を受
// け持つModelで、初回と隣り合う残差の組を採点する。
// 入出力: N>0と時刻順のN行のx y。出力はa b。rho=0.4、変化分のsigma=0.2を固定。
// 注意: このModelにはadd_observations等の観測専用メソッドは使えない。データを変える場合は
// update_modelで所有rowsを編集する。一回の全体評価の途中では時間中断できない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"
using S = McmcEstimator<>; // 全体評価も通常の数値Sessionへ渡せる。

struct Point {
    double x, y;
};

struct Model {
    std::vector<Point> rows;

    // 状態一つに対する目的関数全体を返す。
    double operator()(const McmcState& s) const {
        const double rho = 0.4, sigma = 0.2;
        double energy = 0, previous = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            // realの0番が傾きa、1番が切片b。
            const double e = rows[i].y - (s.real[0] * rows[i].x + s.real[1]);
            // 初回は定常分布の尺度。
            energy += S::gaussian_loss(
                e, i ? rho * previous : 0, i ? sigma : sigma / std::sqrt(1 - rho * rho));
            previous = e;
        }

        return energy;
    }
};

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    assert(n > 0);
    Model model;
    model.rows.resize(n);
    for (auto& r : model.rows) {
        std::cin >> r.x >> r.y;
    }

    S::Parameters p; // Modelが読む添字と同じ順に登録。
    p.real(0, -10, 10); // real[0]にa。
    p.real(0, -10, 10); // real[1]にb。

    // make_observation_modelを介さず全体評価を渡す。
    auto session = S::make_numeric_session(p, std::move(model));

    // 一評価で全行を処理する点に注意。
    const auto r = session.solve(S::Budget::for_steps(80'000));
    // 候補確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 傾きaと切片bを出力。
    std::cout << std::setprecision(10) << r.state->real[0] << ' ' << r.state->real[1]
              << '\n';
}
```

**入力例：`data/19.in`**

```text
4
0 1
1 3
2 5
3 7
```

**出力例（実行確認時の標準出力）**

```text
2.000003006 1.000117248
```

**使うときの注意**

このModelにはadd_observations等の観測専用メソッドは使えない。データを変える場合はupdate_modelで所有rowsを編集する。一回の全体評価の途中では時間中断できない。

### 3.20 端子間の差の測定を、変更箇所だけ再評価

**具体的な問題**

電圧の差しか測れない端子の電圧を推定する。端子一つの候補値を変更したとき、その端子に関係しない測定まで計算し直す必要はない。

**モデルとの対応**

端子0を0Vに固定し、測定差と予測差を比較する。Modelに全体評価とcandidateを用意し、一座標変更なら関係する項の増減を使う。

端子uとvの電位差を測ります。予測は `voltage[u]-voltage[v]` です。全てに同じ値を足しても電位差は変わらないので、一番目の端子を0に固定して基準を決めます。入力グラフは連結とし、端子IDは0以上D未満です。

ある端子一つだけを変えたとき、その端子を含まない測定の予測は変わりません。そこで候補評価では、古い総損失から関係する古い項を引き、新しい項を足します。戻すのはこの**新しい総損失**です。複数の端子が変わった場合は全体評価へ戻る分岐を入れ、提案設定を変えても誤った値を返さないようにしています。

**入力と出力**

端子数D、測定数N、N行のa b difference。differenceは端子aの電位−端子bの電位。aとbは0以上D未満の異なるIDで、全端子が測定を介して端子0へつながる。出力はD個の電圧。

**実行コード：`mcmc_sample_20_candidate_v01.cpp`**

```cpp
// 端子間の差の測定を、変更箇所だけ再評価
// 問題: 電圧の差しか測れない端子の電圧を推定する。端子一つの候補値を変更したとき、その端子
// に関係しない測定まで計算し直す必要はない。
// モデル: 端子0を0Vに固定し、測定差と予測差を比較する。Modelに全体評価とcandidateを用意し、
// 一座標変更なら関係する項の増減を使う。
// 入出力: 端子数D、測定数N、N行のa b difference。differenceは端子aの電位−端子bの電位。aとb
// は0以上D未満の異なるIDで、全端子が測定を介して端子0へつながる。出力はD個の電圧。
// 注意: 変更座標の検出にはD個の走査が必要。candidateの有無で目的関数が変わってはならない。
// 浮動小数点の差分を長期間積み重ねる用途では、必要に応じて全再評価する。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"
using S = McmcEstimator<>; // 標準状態のrealに端子電圧を並べる。

struct Model {
    struct Row {
        int a, b;
        double difference;
    };

    std::vector<Row> rows;
    std::vector<std::vector<std::size_t>> incident;

    // 利用側の一項評価関数。
    double term(std::size_t k, const McmcState& s) const {
        // 差の観測を採点。
        const auto& r = rows[k];
        return S::gaussian_loss(r.difference, s.real[r.a] - s.real[r.b], 0.1);
    }

    // 初期評価や複数変更に必要な全体評価。
    double operator()(const McmcState& s) const {
        double e = 0;
        for (std::size_t k = 0; k < rows.size(); ++k) {
            e += term(k, s);
        }

        return e;
    }

    // 任意の差分評価フック。
    double candidate(const McmcState& old, const McmcState& next, double old_energy) const {
        std::optional<std::size_t> changed;
        // 変わった座標を調べる。
        for (std::size_t j = 0; j < next.real.size(); ++j) {
            if (old.real[j] != next.real[j]) {
                // 複数座標なら正しい全体評価へ戻す。
                if (changed) {
                    return (*this)(next);
                }

                changed = j;
            }
        }

        double e = old_energy;
        if (changed) {
            for (auto k : incident[*changed]) {
                e += term(k, next) - term(k, old);
            }
        }
        // 増分だけでなく、新候補の目的関数全体を返す。共有データは変更しない。
        return e;
    }
};

int main() {
    int d, n;
    if (!(std::cin >> d >> n)) {
        return 1;
    }

    assert(d > 0);
    Model model;
    model.rows.resize(n);
    model.incident.resize(d);
    for (std::size_t k = 0; k < model.rows.size(); ++k) {
        auto& r = model.rows[k];
        std::cin >> r.a >> r.b >> r.difference;
        assert(r.a != r.b);
        model.incident[r.a].push_back(k);
        model.incident[r.b].push_back(k);
    }

    S::Parameters p; // 一つの基準を固定して全体の平行移動を防ぐ。
    p.real(0, 0, 0); // real[0]は0に固定。次元には含まれるが提案で動かない。
    // 残る端子の電圧範囲。
    for (int j = 1; j < d; ++j) {
        p.real(0, -5, 5);
    }

    // candidateがあれば試行の評価に利用される。
    auto session = S::make_numeric_session(p, std::move(model));

    // 初期評価は通常のoperator()を使う。
    const auto r = session.solve(S::Budget::for_steps(80'000));
    // 結果確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 推定電圧の全要素を通常出力。
    for (double x : r.state->real) {
        std::cout << std::setprecision(10) << x << ' ';
    }

    std::cout << '\n';
}
```

**入力例：`data/20.in`**

```text
4 4
1 0 1
2 1 1
3 2 1
3 0 3
```

**出力例（実行確認時の標準出力）**

```text
0 0.9998879213 2.000142777 3.000181801
```

**使うときの注意**

変更座標の検出にはD個の走査が必要。candidateの有無で目的関数が変わってはならない。浮動小数点の差分を長期間積み重ねる用途では、必要に応じて全再評価する。

### 3.21 合計1になる三種類の混合割合

**具体的な問題**

三つの材料を混ぜた製品の性質から、各材料の割合を求める。割合は非負で、三つを足すと必ず1になる。

**モデルとの対応**

独自Stateに三割合を持つ。予測は各材料の性質×割合の合計。二つの成分間で量を移す提案なら、合計1を保てる。

三種類の材料の混合割合w0,w1,w2は、すべて非負で合計1です。三つを独立に動かすと合計1を壊すため、独自状態に配列を置き、二種類の間だけ量を移します。一方へdeltaを足し、もう一方から同じdeltaを引けば、合計は変わりません。

提案のdeltaは正負を同じ確率で取り、選ぶ二種類も対称なので、逆向きの移動と同じ確率になります。負の割合になる候補は棄却します。観測は、三材料を単独で使ったときの既知特性を混合割合で平均した値です。ここでは独自提案により、制約を維持した状態だけを評価します。

**入力と出力**

NとN行の材料1の値 材料2の値 材料3の値 観測値。出力は三割合と合計。

**実行コード：`mcmc_sample_21_simplex_v01.cpp`**

```cpp
// 合計1になる三種類の混合割合
// 問題: 三つの材料を混ぜた製品の性質から、各材料の割合を求める。割合は非負で、三つを足すと
// 必ず1になる。
// モデル: 独自Stateに三割合を持つ。予測は各材料の性質×割合の合計。二つの成分間で量を移す提
// 案なら、合計1を保てる。
// 入出力: NとN行の材料1の値 材料2の値 材料3の値 観測値。出力は三割合と合計。
// 注意: symmetric_moveは対称性を自動証明しない。正逆の選び方が同確率であることを利用側が保
// 証する。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

struct Mix {
    std::array<double, 3> w{1.0 / 3, 1.0 / 3, 1.0 / 3};

    // 独自状態のデバッグ表示。
    friend std::ostream& operator<<(std::ostream& out, const Mix& s) {
        return out << "Mix{" << s.w[0] << ", " << s.w[1] << ", " << s.w[2] << '}';
    }
};

int main() {
    using S = McmcEstimator<Mix>; // 独自状態型をテンプレート引数にする。
    // 三材料の既知特性と混合後の観測。
    using Row = S::Observation<std::array<double, 3>>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 標準観測行は独自Stateでも使える。
    // 観測精度0.1。
    for (auto& r : rows) {
        std::cin >> r.input[0] >> r.input[1] >> r.input[2] >> r.value;
        r.scale = 0.1;
    }

    // 混合後の予測。
    auto predict = [](const std::array<double, 3>& x, const Mix& s) {
        return std::inner_product(x.begin(), x.end(), s.w.begin(), 0.0);
    };

    // 正逆が等確率の提案と宣言。
    auto move =
        S::symmetric_move([](const Mix&, Mix& next, S::Rng& rng, const S::MoveContext&) {
            const int i = rng.integer(3); // 0,1,2から移動元を等確率で選ぶ。
            // 異なる移動先を選ぶ。
            int j;
            do {
                j = rng.integer(3);
            } while (j == i);
            // 正負対称の移動量。幅0.15は問題に合わせて調整。
            const double amount = 0.15 * rng.normal();
            // nextは既に現状態のコピー。合計を保つ。
            next.w[i] += amount;
            next.w[j] -= amount;
            // 範囲外はfalseで棄却。丸め込みで境界へ押し付けない。
            return next.w[i] >= 0 && next.w[j] >= 0;
        });

    // 独自初期状態と提案を所有。
    auto session = S::make_session(
        std::vector<Mix>{Mix{}}, S::make_observation_model(rows, predict), std::move(move));

    // 実行の流れは標準状態と同じ。
    const auto r = session.solve(S::Budget::for_steps(80'000));
    // 空なら参照しない。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 三割合。
    for (double x : r.state->w) {
        std::cout << std::setprecision(10) << x << ' ';
    }

    // 合計が1であることを確認。
    std::cout << std::accumulate(r.state->w.begin(), r.state->w.end(), 0.0) << '\n';
    std::cerr << r << '\n'; // Resultのstate部分にMixのoperator<<が使われる。
}
```

**入力例：`data/21.in`**

```text
4
1 0 0 0.2
0 1 0 0.3
0 0 1 0.5
1 1 1 1
```

**出力例（実行確認時の標準出力）**

```text
0.2000731515 0.3000849034 0.4998419451 1
```

**使うときの注意**

symmetric_moveは対称性を自動証明しない。正逆の選び方が同確率であることを利用側が保証する。

### 3.22 誤りを含む比較結果から順序を推定

**具体的な問題**

対象AがBより先という比較を何回も得たが、一部は間違っている。全比較とできるだけ整合する順序を求める。

**モデルとの対応**

独自Stateに順列を持つ。比較に一致する確率を0.9、違う確率を0.1として損失を作る。二つの位置を交換する提案で順列を保つ。

隠れた順序について「対象uは対象vより前」という比較結果が観測されます。ただし測定には誤りがあります。候補順序が比較と一致すれば確率0.9、不一致なら0.1とするモデルで、全体の説明しやすさを比べます。

状態はIDを一度ずつ含む順列です。二つの位置を交換すれば、IDの重複や欠落を起こしません。配列の各位置を独立したカテゴリとして登録すると重複を許してしまうため、ここでは独自状態と交換提案にしています。

**入力と出力**

対象数D>=2、比較数N、N行のa b y。y=1はaがbより先、0は後。出力は先頭からの対象ID。

**実行コード：`mcmc_sample_22_permutation_v01.cpp`**

```cpp
// 誤りを含む比較結果から順序を推定
// 問題: 対象AがBより先という比較を何回も得たが、一部は間違っている。全比較とできるだけ整合
// する順序を求める。
// モデル: 独自Stateに順列を持つ。比較に一致する確率を0.9、違う確率を0.1として損失を作る。二
// つの位置を交換する提案で順列を保つ。
// 入出力: 対象数D>=2、比較数N、N行のa b y。y=1はaがbより先、0は後。出力は先頭からの対象ID。
// 注意: 比較が少なければ同点の順序が複数ある。対象IDは0〜D−1、比較する二対象は異なるものと
// する。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

struct Order {
    std::vector<int> ids;
};

int main() {
    using S = McmcEstimator<Order>; // 状態は番号の順列。
    // 比較対象の組と0/1の観測。
    using Row = S::Observation<std::array<int, 2>>;
    int d, n;
    if (!(std::cin >> d >> n)) {
        return 1;
    }

    assert(d >= 2);
    std::vector<Row> rows(n); // weightは既定1。
    // a,b,yを読む。
    for (auto& r : rows) {
        std::cin >> r.input[0] >> r.input[1] >> r.value;
    }

    // 候補順序でaが先かを返す。
    auto predict = [](const std::array<int, 2>& pair, const Order& s) {
        return std::find(s.ids.begin(), s.ids.end(), pair[0])
               < std::find(s.ids.begin(), s.ids.end(), pair[1]);
    };

    // 一致・不一致の確率を損失へ。
    auto loss = [](const Order&, const Row& row, bool before) {
        return -std::log(before == (row.value == 1) ? 0.9 : 0.1);
    };

    // 交換の正逆は同じ確率。
    auto move = S::symmetric_move(
        [](const Order&, Order& next, S::Rng& rng, const S::MoveContext&) {
            const int n = static_cast<int>(next.ids.size());
            const int a = rng.integer(n); // 交換する最初の位置。
            // 異なる位置を選ぶ。
            int b;
            do {
                b = rng.integer(n);
            } while (a == b);
            // nextは現状態のコピーなので順列を壊さない。
            std::swap(next.ids[a], next.ids[b]);
            return true;
        });
    Order initial;
    initial.ids.resize(d);
    std::iota(initial.ids.begin(), initial.ids.end(), 0);

    // 独自状態・損失・提案を接続。
    auto session = S::make_session(std::vector<Order>{initial},
                                   S::make_observation_model(rows, predict, loss),
                                   std::move(move));

    // 小例の探索予算。最適性の証明ではない。
    const auto r = session.solve(S::Budget::for_steps(20'000));
    // 候補を確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 順序の先頭から出力。
    for (int id : r.state->ids) {
        std::cout << id << ' ';
    }

    std::cout << '\n';
}
```

**入力例：`data/22.in`**

```text
3 6
2 0 1
0 1 1
2 1 1
2 0 1
0 1 1
1 0 1
```

**出力例（実行確認時の標準出力）**

```text
2 0 1
```

**使うときの注意**

比較が少なければ同点の順序が複数ある。対象IDは0〜D−1、比較する二対象は異なるものとする。

### 3.23 候補を選ぶ確率が偏る場合の標本採取

**具体的な問題**

三種類の装置のどれかが使われており、成功を一回観測した。それぞれの既知成功率から、装置の種類のあり得る割合を求める。候補を生成する都合で、種類の選択確率には偏りがある。

**モデルとの対応**

種類の事前確率は等しいため、目的の割合は成功率に比例する。候補生成確率qの偏りは、log(q(元の種類)/q(新しい種類))を返して補正する。

例の三種類の成功率は0.2、0.5、0.8です。事前には三種類を同じだけあり得るとすると、成功一回を見た後の割合は、それぞれを合計1.5で割った値になります。

しかし候補生成では、三種類を0.6、0.3、0.1の確率で選びます。これをそのまま使うと、生成しやすい種類の影響が余分に入ります。oldからnextを出す確率はq[next]、逆はq[old]なので、`log(q[old])-log(q[next])` を返します。`hastings_move` はこの補正を受理判定へ渡すための入口です。

**入力と出力**

種類数K>=2、K個の成功率、K個の正の候補重み。出力はK種類の標本出現割合と標本数。

**実行コード：`mcmc_sample_23_hastings_v01.cpp`**

```cpp
// 候補を選ぶ確率が偏る場合の標本採取
// 問題: 三種類の装置のどれかが使われており、成功を一回観測した。それぞれの既知成功率から、
// 装置の種類のあり得る割合を求める。候補を生成する都合で、種類の選択確率には偏りがある。
// モデル: 種類の事前確率は等しいため、目的の割合は成功率に比例する。候補生成確率qの偏りは、
// log(q(元の種類)/q(新しい種類))を返して補正する。
// 入出力: 種類数K>=2、K個の成功率、K個の正の候補重み。出力はK種類の標本出現割合と標本数。
// 注意: qは観測モデルの確率とは別。どの種類も提案できるよう、全て正にする。提案比を0で済ま
// せるとこの例では誤った割合になる。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

struct Kind {
    int id = 0;
};

int main() {
    using S = McmcEstimator<Kind>; // 未知の種類IDだけを持つ独自状態。
    int k;
    if (!(std::cin >> k)) {
        return 1;
    }

    assert(k >= 2);
    std::vector<double> success(k), q(k);
    for (double& x : success) {
        std::cin >> x;
        assert(0 < x && x <= 1);
    }

    for (double& x : q) {
        std::cin >> x;
        assert(x > 0);
    }

    double total = std::accumulate(q.begin(), q.end(), 0.0);
    for (double& x : q) {
        x /= total;
    }

    // 成功1回の負の対数尤度。
    auto energy = [success](const Kind& s) {
        return -std::log(success[s.id]);
    };

    // 偏りを補正する提案比を返す。
    auto move = S::hastings_move(
        [q](const Kind& old, Kind& next, S::Rng& rng, const S::MoveContext&) {
            const double u = rng.uniform(); // [0,1)の一様乱数。
            double cumulative = 0;
            next.id = static_cast<int>(q.size()) - 1;
            for (int j = 0; j + 1 < static_cast<int>(q.size()); ++j) {
                cumulative += q[j];
                if (u < cumulative) {
                    next.id = j;
                    break;
                }
            }
            // 逆向き確率/順向き確率の対数。符号を逆にしない。
            return std::log(q[old.id]) - std::log(q[next.id]);
        });
    // ラッパが標本採取可能という宣言を付ける。正しさの証明は利用側の責務。
    static_assert(decltype(move)::sampling_safe);

    S::Param param;
    param.warmup_steps = 2000; // 独自状態の準備量を明示する。

    // 種類0から開始。
    auto session = S::make_session(std::vector<Kind>{{0}}, energy, std::move(move), param);

    // 準備2000、記録100000。
    const auto r = session.sample(S::Budget::for_steps(102'000), [k](const Kind& s) {
        // 1の出現頻度が各種類の確率になる。
        std::vector<double> indicator(k, 0);
        indicator[s.id] = 1;
        return indicator;
    });
    // 平均があるか確認。
    if (!r.summary.mean()) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }
    // 成功率に比例する割合。
    for (double probability : *r.summary.mean()) {
        std::cout << std::setprecision(10) << probability << ' ';
    }

    std::cout << r.summary.count() << '\n'; // 今回の標本数。
}
```

**入力例：`data/23.in`**

```text
3
0.2 0.5 0.8
0.6 0.3 0.1
```

**出力例（実行確認時の標準出力）**

```text
0.13105 0.32773 0.54122 100000
```

**使うときの注意**

qは観測モデルの確率とは別。どの種類も提案できるよう、全て正にする。提案比を0で済ませるとこの例では誤った割合になる。

### 3.24 観測一行の損失と予測変換を直接確かめる

**具体的な問題**

新しい観測形式を組み込む前に、一行の採点が意図どおりか確認する。正規誤差、外れ値に強い誤差、成功・失敗、件数で、同じ数字の意味がどう違うかを確かめる。

**モデルとの対応**

各損失のoperator()を直接呼ぶ。meanは生の予測を観測量の平均へ変換する。ObservationModelの型を明示し、コンストラクタと全体評価も使う。

ライブラリの損失は単独でも呼べます。この例は推定を回す前の、観測一行の接続確認です。最初の行では実測10、予測0、sigma1なので、Gaussianの残差部分は `10*10/2=50` です。Huberは大きな残差の罰を抑えるため、より小さくなります。

二値のlogit0は成功率0.5、件数のlog_meanがlog2なら平均件数2です。最後にObservationModelを型名付きで構築して、一行のGaussian損失と同じ値を返すことを確認します。通常の利用で毎回この長い型名を書く必要はありません。

**入力と出力**

第1行はobserved predicted sigma weight、第2行は0/1のobservedとlogit、第3行はcountとlog_mean。出力1行目は四つの損失、2行目は四つの平均、3行目はモデル全体の損失と観測件数。

**実行コード：`mcmc_sample_24_loss_check_v01.cpp`**

```cpp
// 観測一行の損失と予測変換を直接確かめる
// 問題: 新しい観測形式を組み込む前に、一行の採点が意図どおりか確認する。正規誤差、外れ値に
// 強い誤差、成功・失敗、件数で、同じ数字の意味がどう違うかを確かめる。
// モデル: 各損失のoperator()を直接呼ぶ。meanは生の予測を観測量の平均へ変換する。
// ObservationModelの型を明示し、コンストラクタと全体評価も使う。
// 入出力: 第1行はobserved predicted sigma weight、第2行は0/1のobservedとlogit、第3行はcount
// とlog_mean。出力1行目は四つの損失、2行目は四つの平均、3行目はモデル全体の損失と観測件数。
// 注意: 目的関数から状態に依存しない定数を省いているので、損失が負でも異常ではない。値だけ
// を見て別の観測モデルと単純比較しない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 標準損失もこの型の公開API。
    // 今回inputは使わないが0を設定しておく。
    using Row = S::Observation<double>;
    // 観測の各値を初期化。
    Row normal{0, 0, 1, 1};
    double predicted, logit, log_mean;
    // 正規誤差の行。
    if (!(std::cin >> normal.value >> predicted >> normal.scale >> normal.weight)) {
        return 1;
    }

    Row binary{0, 0, 1, 1}, count{0, 0, 1, 1}; // 二値・件数も標準観測行で表せる。
    // 生のlogitとlog_meanを読む。
    std::cin >> binary.value >> logit >> count.value >> log_mean;
    // 予測値をreal[0]に持つ候補。独自損失で状態が必要なときにも同じ引数形。
    McmcState state{{predicted}, {}};
    S::Gaussian gaussian; // 既知sigmaによる二乗残差とlog sigma。
    S::Huber huber{1.345}; // 標準化残差の切替値。
    // 成功・失敗用。予測引数は確率そのものではない。
    S::BernoulliLogit bernoulli;
    S::PoissonLogMean poisson; // 件数用。予測引数は平均件数の対数。

    // 二種類の実数観測の採点。
    std::cout << std::setprecision(10) << gaussian(state, normal, predicted) << ' '
              << huber(state, normal, predicted) << ' ';

    // 二値と件数の採点。
    std::cout << bernoulli(state, binary, logit) << ' ' << poisson(state, count, log_mean)
              << '\n';

    // 実数観測の変換は恒等変換。
    std::cout << S::Gaussian::mean(predicted) << ' ' << S::Huber::mean(predicted) << ' ';

    // 確率と平均件数へ戻す。
    std::cout << S::BernoulliLogit::mean(logit) << ' ' << S::PoissonLogMean::mean(log_mean)
              << '\n';

    // inputを使わない一定値のモデル。
    auto predict = [](double, const McmcState& s) {
        return s.real[0];
    };

    // 明示する事前項。ここでは何も加えない。
    auto prior = [](const McmcState&) {
        return 0.0;
    };
    // 型を直接指定する上級の組み立て方。
    using Model = S::ObservationModel<Row, decltype(predict), S::Gaussian, decltype(prior)>;
    // モデルが扱う観測行の公開型別名。
    static_assert(std::is_same_v<Model::observation_type, Row>);
    // 最初の四引数は必須。Contextなしを既定で使う。
    Model model(std::vector<Row>{normal}, predict, gaussian, prior);

    // 全行と事前項の合計、読み取り専用の観測ビュー。
    std::cout << model(state) << ' ' << model.observations().size() << '\n';
}
```

**入力例：`data/24.in`**

```text
10 0 1 1
1 0
3 0.6931471805599453
```

**出力例（実行確認時の標準出力）**

```text
50 12.5454875 0.6931471806 -0.07944154168
0 0 0.5 2
50 1
```

**使うときの注意**

目的関数から状態に依存しない定数を省いているので、損失が負でも異常ではない。値だけを見て別の観測モデルと単純比較しない。

### 3.25 下位APIで探索・準備・標本採取を自分で管理

**具体的な問題**

既存のコンテスト用ループへ組み込みたいので、モデルと提案を外で所有し、各段階を自分で切り替える。

**モデルとの対応**

McmcEstimator本体を作り、reset、Search、Warmup、Sampleの順に呼ぶ。通常Sessionが行う準備の管理を利用側が担当する。

Sessionの内部で管理していた段階を、ここでは自分で呼び分けます。resetで候補と評価値を用意し、Searchで良い値を探し、Warmupで標本採取の準備をし、Sampleで通知を受けます。

下位APIはBudgetを参照で受けるため、runが完了した後は `remaining_steps` が減っています。モデルと提案は同じオブジェクトを生かし続けます。単に一つの推定をしたいだけならSessionの方が短く書けるので、この例は既存の制御ループへ組み込む場合向けです。

**入力と出力**

NとN行のx y、予測入力q。出力は最良a b、標本からの予測平均、標本数、残り遷移数。

**実行コード：`mcmc_sample_25_core_phases_v01.cpp`**

```cpp
// 下位APIで探索・準備・標本採取を自分で管理
// 問題: 既存のコンテスト用ループへ組み込みたいので、モデルと提案を外で所有し、各段階を自分
// で切り替える。
// モデル: McmcEstimator本体を作り、reset、Search、Warmup、Sampleの順に呼ぶ。通常Sessionが行
// う準備の管理を利用側が担当する。
// 入出力: NとN行のx y、予測入力q。出力は最良a b、標本からの予測平均、標本数、残り遷移数。
// 注意: 下位runは独自提案のsampling_safe宣言を検査しない。Sampleに使う提案の正しさを自分で
// 保証する。中断後は同じモデルと提案を渡して再開する。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 実数・整数・カテゴリを持つ標準状態を使う。
    // 一入力・一観測値。既定のscaleとweightは1。
    using Row = S::Observation<double>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    std::vector<Row> rows(n); // 後でモデルへ所有させる観測列。
    // 各行の既知の入力xと観測y。
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }

    S::Parameters p; // 未知変数の宣言をまとめる。
    const auto a = p.real(0, -10, 10); // 傾きaは実数、初期値0、範囲[-10,10]。
    const auto b = p.real(0, -10, 10); // 切片bも実数、初期値0、範囲[-10,10]。

    // 係数を指定した一次関数。
    auto predict = [=](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b];
    };

    double q;
    std::cin >> q;
    // 既知の標準偏差。
    for (auto& row : rows) {
        row.scale = 0.5;
    }

    // 下位APIではモデルを外側で保持する。
    auto model = S::make_observation_model(rows, predict);

    // 宣言したDomainから数値提案を作る。
    auto move = S::make_numeric_move(p.domain());
    // 一つの開始候補。spanの参照先を生存させる。
    std::vector<McmcState> initial{p.initial()};

    S engine; // Param省略時は既定の設定とseed=1。

    auto budget = S::Budget::for_steps(30'000); // 下位APIへ参照渡しする予算。
    // 初期候補をコピー・評価する。
    const auto initialized =
        engine.reset(std::span<const McmcState>(initial), model, budget);
    // 全て不可能な初期候補からは開始できない。
    if (!initialized.has_estimate) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }
    // 最良候補探索。budget.remaining_stepsは減る。
    engine.run(S::Phase::Search, model, move, budget);
    budget = S::Budget::for_steps(2000); // 準備に別の予算を与える。
    // 温度1で準備。標本はまだ出さない。
    engine.run(S::Phase::Warmup, model, move, budget);
    budget = S::Budget::for_steps(40'000); // 標本採取の予算。
    double sum = 0;
    std::uint64_t count = 0;

    // 数値提案はSample中に適応しない。
    const auto report = engine.run(
        S::Phase::Sample, model, move, budget, [&](const McmcState& s, double, int) {
            // 棄却時も現状態の通知を受け、すべて集計する。
            sum += predict(q, s);
            ++count;
        });

    const auto best = engine.best(); // 次の実行・変更まで有効。
    // 候補・標本の両方を確認。
    if (!best.state || count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    // 下位APIでは残量0。
    std::cout << std::setprecision(10) << best.state->real[a] << ' ' << best.state->real[b]
              << ' ' << sum / count << ' ' << report.emitted << ' '
              << budget.remaining_steps << '\n';
}
```

**入力例：`data/25.in`**

```text
4
0 1
1 3
2 5
3 7
4
```

**出力例（実行確認時の標準出力）**

```text
1.999896113 1.000058977 9.008335602 40000 0
```

**使うときの注意**

下位runは独自提案のsampling_safe宣言を検査しない。Sampleに使う提案の正しさを自分で保証する。中断後は同じモデルと提案を渡して再開する。

### 3.26 下位APIで観測更新と外部変数の変更を通知

**具体的な問題**

外部で管理する観測や既知補正値を使う構成で、推定器が古い評価を使わないようにしたい。観測追加、全置換、外部変数の更新を区別する。

**モデルとの対応**

予測はax+b+correction。observeは新規行の追加、replace_observationsは全置換、refreshは外部補正変更後の全再評価に使う。

下位APIでは、モデルの外にある既知補正値を変えても、推定器はその変更を自動では知りません。この例では補正を参照するラムダを使うので、補正値を変えた直後にrefreshを呼びます。

observeは新規観測だけを追加、replace_observationsは観測列全体を置換、refreshは評価関数側の変更を通知します。いずれもその場で予算内の再評価を行います。時間切れになっても同じ更新を二度登録せず、新しい目的関数を使うrunで続きを進めます。

**入力と出力**

Nと初期N行、Kと追加K行、Mと置換M行、最後にcorrection。各行はx y。出力は最後のa bと観測数。

**実行コード：`mcmc_sample_26_core_updates_v01.cpp`**

```cpp
// 下位APIで観測更新と外部変数の変更を通知
// 問題: 外部で管理する観測や既知補正値を使う構成で、推定器が古い評価を使わないようにしたい。
// 観測追加、全置換、外部変数の更新を区別する。
// モデル: 予測はax+b+correction。observeは新規行の追加、replace_observationsは全置換、
// refreshは外部補正変更後の全再評価に使う。
// 入出力: Nと初期N行、Kと追加K行、Mと置換M行、最後にcorrection。各行はx y。出力は最後のa b
// と観測数。
// 注意: observeやrefreshは更新登録だけでなく、渡した予算内で再評価も行う。時間切れでも更新
// を二重登録せず、runへ現在のモデルを渡す。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 外部所有モデルを使う下位API。
    using Row = S::Observation<double>; // x,yの観測行。

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }
    // 初期観測。
    std::vector<Row> rows(n);
    for (auto& r : rows) {
        std::cin >> r.input >> r.value;
    }
    // 新規観測だけ。
    int k;
    std::cin >> k;
    std::vector<Row> extra(k);
    for (auto& r : extra) {
        std::cin >> r.input >> r.value;
    }
    // 置換後の全観測。
    int m;
    std::cin >> m;
    std::vector<Row> replacement(m);
    for (auto& r : replacement) {
        std::cin >> r.input >> r.value;
    }

    double corrected;
    std::cin >> corrected;
    double correction = 0; // mainの最後まで生きる参照先。

    S::Parameters p; // 係数の意味・次元は全過程で変えない。
    const auto a = p.real(0, -10, 10); // 傾きa。
    const auto b = p.real(0, -10, 10); // 切片b。

    // 既知補正は外部参照。
    auto predict = [a, b, &correction](double x, const McmcState& s) {
        return s.real[a] * x + s.real[b] + correction;
    };

    // ラムダの参照先までは所有しない。
    auto model = S::make_observation_model(rows, predict);

    // 同じ提案オブジェクトを使い続ける。
    auto move = S::make_numeric_move(p.domain());
    std::vector<McmcState> initial{p.initial()}; // 初期候補。

    S engine; // 自動の更新管理は利用しない。

    // 再評価と探索で共有する予算。
    auto budget = S::Budget::for_steps(30'000);
    engine.reset(initial, model, budget); // 初期化と初回評価。
    engine.run(S::Phase::Search, model, move, budget); // 最初の探索。
    budget = S::Budget::for_steps(30'000); // 次の区間の予算。
    // 追加は一度だけ。可能なら追加項だけ再計算。
    engine.observe(model, std::move(extra), budget);
    // 追加後の目的関数で継続。
    engine.run(S::Phase::Search, model, move, budget);
    budget = S::Budget::for_steps(40'000); // 置換と探索の予算。
    // 全置換なので全候補を再評価。
    engine.replace_observations(model, std::move(replacement), budget);
    engine.run(S::Phase::Search, model, move, budget); // 置換後の探索。
    correction = corrected;
    budget = S::Budget::for_steps(40'000); // 外部補正の変更に対応。
    // 外部変数を変えた事実を必ず通知し、古い評価を無効化。
    engine.refresh(model, budget);
    // 同じ更新済みモデルで再開。
    engine.run(S::Phase::Search, model, move, budget);

    const auto best = engine.best(); // 現在のモデルで評価済みの候補。
    // ポインタの有無を確認。
    if (!best.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 推定値と置換後件数。
    std::cout << std::setprecision(10) << best.state->real[a] << ' ' << best.state->real[b]
              << ' ' << model.size() << '\n';
}
```

**入力例：`data/26.in`**

```text
3
0 1
1 3
2 5
1
3 7
4
0 2
1 4
2 6
3 8
1
```

**出力例（実行確認時の標準出力）**

```text
1.999971171 1.000254462 4
```

**使うときの注意**

observeやrefreshは更新登録だけでなく、渡した予算内で再評価も行う。時間切れでも更新を二重登録せず、runへ現在のモデルを渡す。

### 3.27 目的関数へ新しい罰則を加える

**具体的な問題**

観測から求めた係数が大き過ぎると運用上困ると分かった。目的関数に係数の二乗に比例する罰を追加し、保持中の評価値も正確な増分で更新する。

**モデルとの対応**

最初は(a−target)²/2だけ、後からlambda*a²/2を足す。append_energyには新旧の目的関数の差を返す関数を渡す。候補変更のcandidateとは別の機能。

最初の目的関数は `(a-target)^2/2` です。後から `lambda*a*a/2` を足すと、大きなaにも罰がつきます。二つを合わせた最小位置は `target/(1+lambda)` なので、例のtarget4・lambda1ではa=2になります。

この例の差は、同じ状態に対する「新しいルールと古いルールの差」です。例3.20の、同じルール内で状態を変更した差とは区別します。保持中の評価値へ差を足す前に全体モデルも更新し、その後の新候補は新しい全体モデルで採点します。

**入力と出力**

targetとlambda>=0。出力は更新後のaと目的関数値。

**実行コード：`mcmc_sample_27_append_energy_v01.cpp`**

```cpp
// 目的関数へ新しい罰則を加える
// 問題: 観測から求めた係数が大き過ぎると運用上困ると分かった。目的関数に係数の二乗に比例す
// る罰を追加し、保持中の評価値も正確な増分で更新する。
// モデル: 最初は(a−target)²/2だけ、後からlambda*a²/2を足す。append_energyには新旧の目的関数
// の差を返す関数を渡す。候補変更のcandidateとは別の機能。
// 入出力: targetとlambda>=0。出力は更新後のaと目的関数値。
// 注意: 途中で時間切れになっても同じ増分を再度append_energyしない。残りの同期は更新済みの全
// 体評価を渡すrunで補う。変更後の全体評価を古いままにすると不整合になる。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"
using S = McmcEstimator<>; // aをreal[0]に置く。

struct Model {
    double target, lambda;

    // 更新後も全体を評価できるようにする。
    double operator()(const McmcState& s) const {
        // 二つの罰の合計。
        const double a = s.real[0];
        return 0.5 * (a - target) * (a - target) + 0.5 * lambda * a * a;
    }
};

int main() {
    double target, lambda;
    if (!(std::cin >> target >> lambda)) {
        return 1;
    }

    assert(lambda >= 0);
    Model model{target, 0};

    // targetと更新後の最適値が範囲内にある小例。
    S::Parameters p;
    p.real(0, -10, 10);

    auto move = S::make_numeric_move(p.domain()); // 外で所有する数値提案。
    std::vector<McmcState> initial{p.initial()}; // 初期候補。

    S engine; // 増分更新を使う下位推定器。

    auto budget = S::Budget::for_steps(30'000); // まず旧モデルで同期・探索。
    engine.reset(initial, model, budget); // 全候補の初期評価。
    // 旧目的関数を探索。
    const auto before = engine.run(S::Phase::Search, model, move, budget);
    // append_energyの前提条件。
    if (!before.synchronized || !before.has_estimate) {
        std::cout << "NOT_READY\n";
        return 0;
    }

    model.lambda = lambda; // 先に全体評価の方も新モデルへ変更。
    // 新E−旧Eを原子的に計算。
    auto delta = [lambda](const McmcState& s) {
        return 0.5 * lambda * s.real[0] * s.real[0];
    };
    budget = S::Budget::for_steps(30'000); // 再評価と次の探索の予算。
    // 保持中の候補に増分を一度だけ加える。
    engine.append_energy(delta, budget);
    // 新候補には更新済みの全体評価を使う。
    engine.run(S::Phase::Search, model, move, budget);

    const auto best = engine.best(); // 新目的関数での最良候補。
    // 候補確認。
    if (!best.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // aと新しい評価値。
    std::cout << std::setprecision(10) << best.state->real[0] << ' ' << best.energy << '\n';
}
```

**入力例：`data/27.in`**

```text
4 1
```

**出力例（実行確認時の標準出力）**

```text
2.000011985 4
```

**使うときの注意**

途中で時間切れになっても同じ増分を再度append_energyしない。残りの同期は更新済みの全体評価を渡すrunで補う。変更後の全体評価を古いままにすると不整合になる。

### 3.28 数値提案と乱数を直接調べてから組み込む

**具体的な問題**

標準提案の変更幅を確かめ、既存コードへ明示的な部品として組み込みたい。乱数の範囲と提案の状態更新を、小さな診断で確認する。

**モデルとの対応**

実数一つとカテゴリ一つをDomainへ直接定義する。診断では提案とfeedbackを対で呼び、その後にSessionを直接構築して簡単な目的関数を解く。

普通は呼ばない部品を、接続を確認するために直接使います。最初の行で乱数の範囲を確認し、次に実数一個・カテゴリ一個の提案を作ります。直接呼ぶ場合は、変更先を現状態のコピーにすることと、採否をfeedbackすることを自分で行います。

前半は候補の形を見る診断なので、有効な候補をすべて採用しています。この列を目的関数に対応する標本として使ってはいけません。最後のSessionで初めて通常の評価・採否を使い、最小が実数2・カテゴリ1にある小問題を解きます。

**入力と出力**

seedと診断回数R（R>=1）。出力1行目はnext、uniform、log_uniform、normal、integer(7)。2行目は診断で有効だった候補数、R。3行目は推定実数とカテゴリID。

**実行コード：`mcmc_sample_28_proposal_diagnostics_v01.cpp`**

```cpp
// 数値提案と乱数を直接調べてから組み込む
// 問題: 標準提案の変更幅を確かめ、既存コードへ明示的な部品として組み込みたい。乱数の範囲と
// 提案の状態更新を、小さな診断で確認する。
// モデル: 実数一つとカテゴリ一つをDomainへ直接定義する。診断では提案とfeedbackを対で呼び、
// その後にSessionを直接構築して簡単な目的関数を解く。
// 入出力: seedと診断回数R（R>=1）。出力1行目はnext、uniform、log_uniform、normal、
// integer(7)。2行目は診断で有効だった候補数、R。3行目は推定実数とカテゴリID。
// 注意: ctx.populationの参照先は提案呼び出し中に生存させる。診断のように全有効候補を受理す
// る処理は、目的関数に対応する標本採取ではない。通常は提案を直接呼ばず、Sessionへ渡す。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 数値提案と補助乱数の公開型を利用。

    std::uint64_t seed;
    int trials;
    if (!(std::cin >> seed >> trials)) {
        return 1;
    }

    assert(trials >= 1);

    S::Rng rng(seed); // seedを明示して再現可能にする。省略時は1。
    const auto raw = rng.next(); // 64bit符号なし整数。
    const double u = rng.uniform(); // 0以上1未満。
    // 0より大きく1以下の乱数の対数。有限で0以下。
    const double log_u = rng.log_uniform();
    const double z = rng.normal(); // 平均0、標準偏差1の正規乱数。
    // 0以上7未満の整数。引数は正で2^31-1以下。
    const int index = rng.integer(7);

    std::cout << std::setprecision(12) << raw << ' ' << u << ' ' << log_u << ' ' << z << ' '
              << index << '\n';

    S::Domain domain; // Parametersを介さず、状態の範囲と形を直接定義。
    // lower,upper,scale。直接指定ではscale>0が必要。
    domain.real.push_back({-5, 5, 0.4});
    // lower,upper,categorical。ID 0,1,2。
    domain.discrete.push_back({0, 2, true});

    // 診断では適応と重い裾を無効にして幅を見やすくする。
    S::MoveParam options;
    options.adapt = false; // 受理履歴による幅変更をしない。
    options.cauchy = false; // Searchでも正規の変更幅を使う。
    options.stretch_probability = 0; // 集団操作を使わない診断。

    // 数値提案を直接構築。domainと状態の配列サイズを一致させる。
    S::NumericMove move(domain, options);
    std::vector<McmcState> population{{{0}, {0}}}; // 実数1個、離散1個。
    // phase,温度,対象鎖,全鎖ビュー,モデル番号。
    const S::MoveContext ctx{S::Phase::Warmup, 1, 0, population, 1};
    int valid = 0;
    for (int i = 0; i < trials; ++i) {
        // 呼び出し側が現状態のコピーを用意する。
        McmcState next = population[0];
        // 提案比の対数、または候補無効を示す-inf。
        const double ratio = move(population[0], next, rng, ctx);
        // ここでは候補の生成診断だけ。事後分布の標本採取ではない。
        const bool accept = ratio != -INFINITY;
        // 完了した一試行に一回だけ対応するfeedbackを返す。
        move.feedback(accept, ctx);
        // 診断用の状態を進める。
        if (accept) {
            population[0] = std::move(next);
            ++valid;
        }
    }

    std::cout << valid << ' ' << trials << '\n';

    // 最小はreal=2、ID=1。
    auto energy = [](const McmcState& s) {
        return 0.5 * std::pow(s.real[0] - 2, 2) + (s.discrete[0] == 1 ? 0.0 : 1.0);
    };
    // 構築済み部品の型からSession型を指定。
    using Session = S::Session<decltype(energy), S::NumericMove>;

    S::Param param;
    param.seed = seed; // 推定器自身の乱数の初期値。
    // 直接構築。最後のParamは省略すると既定値になる。
    Session session(population, energy, move, param);

    // 本来の推定はSessionの受理判定を使う。
    const auto r = session.solve(S::Budget::for_steps(30'000));
    // 結果を確認。
    if (!r.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 推定した実数と種類ID。
    std::cout << r.state->real[0] << ' ' << r.state->discrete[0] << '\n';
}
```

**入力例：`data/28.in`**

```text
17 12
```

**出力例（実行確認時の標準出力）**

```text
9260656408219841379 0.391433679198 -1.1137366442 0.0291461268455 1
11 12
1.9999835767 1
```

**使うときの注意**

ctx.populationの参照先は提案呼び出し中に生存させる。診断のように全有効候補を受理する処理は、目的関数に対応する標本採取ではない。通常は提案を直接呼ばず、Sessionへ渡す。

### 3.29 候補がない場合の回復・固定変数・デバッグ出力

**具体的な問題**

初期値が制約を満たさないため探索を開始できない場合を見分ける。妥当な初期値で再開し、大きな状態や固定した値も確認する。

**モデルとの対応**

D個の実数を0〜1に制限し、先頭が正という追加制約を置く。0予算と不可能な初期値を区別してからrestartする。全固定の別Sessionでは準備が不要になる。

予算0では初期値も評価しないため、結果が空でも理由はBudgetです。正の予算で初期値を評価し、それが全部制約違反だったときにNoFiniteStateになります。どちらも「全ての可能な状態を調べて解なしと分かった」という意味ではありません。

制約を満たす0.5の開始値を入れてrestartした後は、有限な候補を返せます。別のSessionでは上下限を両方0.5にして全変数を固定し、準備なしで同じ値が記録されることも確認します。標準エラー出力では、配列の後半が省略されたことと、省略数を確認できます。

**入力と出力**

次元D>=1。例では25を与える。標準出力は確認値4個、固定Sessionの標本数、準備残量。標準エラー出力には結果構造体を出し、長い配列は20要素で省略される。

**実行コード：`mcmc_sample_29_empty_fixed_debug_v01.cpp`**

```cpp
// 候補がない場合の回復・固定変数・デバッグ出力
// 問題: 初期値が制約を満たさないため探索を開始できない場合を見分ける。妥当な初期値で再開し、
// 大きな状態や固定した値も確認する。
// モデル: D個の実数を0〜1に制限し、先頭が正という追加制約を置く。0予算と不可能な初期値を区
// 別してからrestartする。全固定の別Sessionでは準備が不要になる。
// 入出力: 次元D>=1。例では25を与える。標準出力は確認値4個、固定Sessionの標本数、準備残量。
// 標準エラー出力には結果構造体を出し、長い配列は20要素で省略される。
// 注意: NoFiniteStateは「問題全体に解がない」という証明ではなく、評価した初期候補に有限候補
// がないという意味。制約を満たす開始候補を一つ以上用意する。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 結果構造体をそのまま出力できる。
    int d;
    if (!(std::cin >> d)) {
        return 1;
    }

    assert(d >= 1);

    S::Parameters p; // 最初は追加制約に反する0から開始する。
    // Domain内でも別の制約に反することはある。
    for (int i = 0; i < d; ++i) {
        p.real(0, 0, 1);
    }

    // 有限または+infだけを返す。
    auto energy = [](const McmcState& s) -> double {
        // 制約違反を+infで表す。NaNや-infは使わない。
        if (s.real[0] <= 0) {
            return INFINITY;
        }
        // realの各値が0.5から離れる分を採点。
        double e = 0;
        for (double x : s.real) {
            e += (x - 0.5) * (x - 0.5);
        }

        return e;
    };

    // 初期評価はsolveまで遅延する。
    auto session = S::make_numeric_session(p, energy);
    // 新しい評価も遷移も始めない。
    const auto empty = session.solve(S::Budget::for_steps(0));
    // 未評価と制約違反を混同しない。
    const bool budget_only = !empty.state && empty.report.stop == S::Stop::Budget;
    // 初期候補が全て+infなら探索を開始しない。
    const auto failed = session.solve(S::Budget::for_steps(10));
    // 初期評価を終えた上で有限候補がない。
    const bool impossible = failed.report.stop == S::Stop::NoFiniteState;
    McmcState feasible = p.initial(); // 同じ形・同じ範囲の候補を作る。
    // 追加制約を満たす値。
    std::fill(feasible.real.begin(), feasible.real.end(), 0.5);
    // 乱数と探索履歴を初期化して開始値を差し替える。
    session.restart(std::vector<McmcState>{feasible});

    // 今度は有限候補から動かせる。
    const auto r = session.solve(S::Budget::for_steps(20));
    // まだ新たな標本はない。
    const auto none = session.sample(S::Budget::for_steps(0), [](const McmcState& s) {
        return s.real;
    });
    // 未計算は値0ではなく空optional。
    const bool no_statistics = !none.summary.mean() && !none.summary.variance();

    S::Parameters fixed; // 全固定の条件で挙動を確認する別のSession。
    // lower==upperなら値を動かさない。
    for (int i = 0; i < d; ++i) {
        fixed.real(0.5, 0.5, 0.5);
    }
    // 可動変数0なら既定の準備量は0。
    auto fixed_session = S::make_numeric_session(fixed, energy);
    // 同じ状態を2標本記録。
    const auto two = fixed_session.sample(S::Budget::for_steps(2), [](const McmcState& s) {
        return s.real;
    });

    // 確認値。
    std::cout << budget_only << ' ' << impossible << ' ' << r.report.has_estimate << ' '
              << no_statistics << ' ' << two.summary.count() << ' '
              << fixed_session.warmup_remaining() << '\n';
    // Resultはstate、energy、report、revisionを表示。
    std::cerr << empty << '\n' << failed << '\n' << r << '\n';
    // ReportとBestも出力可能。Bestは更新前に使う。
    std::cerr << r.report << '\n' << session.best() << '\n';
    // McmcState単体の出力。配列は先頭20要素まで。
    if (r.state) {
        std::cerr << *r.state << '\n';
    }
    // SampleResultとMomentsの空・非空を確認。
    std::cerr << none << '\n' << two << '\n' << two.summary << '\n';
}
```

**入力例：`data/29.in`**

```text
25
```

**出力例（実行確認時の標準出力）**

```text
1 1 1 1 2 0
```

**標準エラー出力の例（抜粋）**

省略表示そのものを確認するため、状態と集計を抜き出しています。Reportの経過時間は環境により変わります。

```text
McmcState{real=[0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, ... (+5 more)], discrete=[]}
Moments{count=2, mean=[0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, ... (+5 more)], variance=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (+5 more)]}
```

**使うときの注意**

NoFiniteStateは「問題全体に解がない」という証明ではなく、評価した初期候補に有限候補がないという意味。制約を満たす開始候補を一つ以上用意する。

### 3.30 正の尺度を対数で持ち、標本採取の重みを合わせる

**具体的な問題**

誤差の平均は0と分かっているが、標準偏差sigmaは不明。正の値を取りやすいようu=log sigmaで保持し、sigmaの同じ幅には同じ事前の重みを置きたい。

**モデルとの対応**

まず通常の正規損失で最良sigmaを求める。次に標本採取用の別Sessionでは、uの同じ幅が大きなsigmaほど広いsigma区間に対応するため、事前項に−uを加えて重みを補正する。

sigmaは正なので、`u=log(sigma)` と置けば、実数uから `exp(u)` で必ず正の値に戻せます。最良値だけなら、同じ目的関数をこの変数で評価すれば、対応するsigmaの最良値を探せます。

標本を採る場合は、「同じ幅を同じだけあり得るとみなす対象」が違います。uを0.01だけ増やしたとき、sigmaが1付近ならsigmaは約0.01増え、sigmaが3付近なら約0.03増えます。つまり同じuの幅が、大きいsigmaではより広いsigma区間を表します。

sigmaの同じ幅に同じ事前の重みを置くなら、uの重みにはsigma倍、すなわちexp(u)倍の補正が必要です。マイナス対数で書くと事前項に `-u` を加えます。最良sigmaを求める目的関数と、u上で採取するために補正した目的関数を別のSessionに分け、出力を混同しないようにしています。

**入力と出力**

NとN個の誤差y。平均0は既知。出力はsigmaの最尤推定、sigmaについて一様な事前を使った標本平均、標本数。sigmaの範囲は[0.1,4]。

**実行コード：`mcmc_sample_30_log_sigma_v01.cpp`**

```cpp
// 正の尺度を対数で持ち、標本採取の重みを合わせる
// 問題: 誤差の平均は0と分かっているが、標準偏差sigmaは不明。正の値を取りやすいようu=log
// sigmaで保持し、sigmaの同じ幅には同じ事前の重みを置きたい。
// モデル: まず通常の正規損失で最良sigmaを求める。次に標本採取用の別Sessionでは、uの同じ幅が
// 大きなsigmaほど広いsigma区間に対応するため、事前項に−uを加えて重みを補正する。
// 入出力: NとN個の誤差y。平均0は既知。出力はsigmaの最尤推定、sigmaについて一様な事前を使っ
// た標本平均、標本数。sigmaの範囲は[0.1,4]。
// 注意: 補正−uを入れない採取は、uに一様、すなわちsigmaでは1/sigmaに比例する事前になる。採取
// 用に補正した目的関数をsolveで最小化した値は、sigma上の最尤値とは一致しないので混同しない。
// 推定器と必要な標準ライブラリを読み込む。
#include "mcmc_estimator_v10.hpp"

int main() {
    using S = McmcEstimator<>; // 状態はsigmaではなくu=log sigma。
    // inputは使わない番号、valueは平均0からの誤差。
    using Row = S::Observation<int>;

    int n;
    if (!(std::cin >> n)) {
        return 1;
    }

    assert(n > 0);
    std::vector<Row> rows(n); // scaleは独自損失では使用しない。
    // 既知平均からの誤差を渡す。
    for (int i = 0; i < n; ++i) {
        rows[i].input = i;
        std::cin >> rows[i].value;
    }

    S::Parameters p; // 正のsigmaを対数で表現。
    // 初期sigma=1、範囲[0.1,4]。
    const auto u = p.real(0, std::log(0.1), std::log(4.0));

    // 誤差の平均は既知の0。
    auto predict = [](int, const McmcState&) {
        return 0.0;
    };

    // sigmaへ戻して正規損失。
    auto loss = [=](const McmcState& s, const Row& row, double y) {
        return S::gaussian_loss(row.value, y, std::exp(s.real[u]));
    };

    // 最良値だけなら単なる変数の置換。
    auto optimizer =
        S::make_numeric_session(p, S::make_observation_model(rows, predict, loss));

    // sigmaの尤度が最大になる候補を探す。
    const auto best = optimizer.solve(S::Budget::for_steps(30'000));
    // 結果確認。
    if (!best.state) {
        std::cout << "NO_ESTIMATE\n";
        return 0;
    }

    // sigmaに一様な事前を置くため、uの密度にはexp(u)を掛ける。
    auto prior = [=](const McmcState& s) {
        return -s.real[u];
    };

    S::Param param;
    param.warmup_steps = 2000; // 採取用の準備量。

    // 採取用の別目的関数。
    auto sampler = S::make_numeric_session(
        p, S::make_observation_model(rows, predict, loss, prior), param);

    // uの平均でなくsigmaの平均を集計。
    const auto r = sampler.sample(S::Budget::for_steps(102'000), [=](const McmcState& s) {
        return std::exp(s.real[u]);
    });
    // 0標本なら平均なし。
    if (!r.summary.mean()) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    // 最尤値と平均は別物。
    std::cout << std::setprecision(10) << std::exp(best.state->real[u]) << ' '
              << *r.summary.mean() << ' ' << r.summary.count() << '\n';
}
```

**入力例：`data/30.in`**

```text
4
-1 1 -1 1
```

**出力例（実行確認時の標準出力）**

```text
1 1.446967638 100000
```

**使うときの注意**

補正−uを入れない採取は、uに一様、すなわちsigmaでは1/sigmaに比例する事前になる。採取用に補正した目的関数をsolveで最小化した値は、sigma上の最尤値とは一致しないので混同しない。

### 3.31 実行確認と付属ファイル

全30例をGCC 13.3.0、C++20、`-O2 -Wall -Wextra -Wpedantic`でコンパイルし、警告なしで実行しました。出力は単にプログラムが終わったかだけでなく、推定値・分散・標本数・列挙値の意味を、それぞれの問題で期待される値や範囲と照合しています。探索結果の最適性や、任意の入力での収束を保証する検査ではありません。

例3.19〜3.30はAddressSanitizerとUndefinedBehaviorSanitizerでも確認しています。本文のC++コードは付属 `.cpp` と一致することを検査しています。

| ファイル | 内容 |
| --- | --- |
| `mcmc_estimator_v10.hpp` | この文書の対象ヘッダ。 |
| `examples/` | 独立した30本のC++プログラム。 |
| `data/` | 各例の標準入力。 |
| `results/debug/` | 確認時の標準出力、標準エラー出力、コンパイルログ、検査結果。 |
| `results/sanitize/` | 例3.19〜3.30の追加検査結果。 |
| `verify_examples.py` | コンパイル・実行・数値照合を行うPython 3スクリプト。 |
| `cases.json` | 入力と、検査する期待値・許容幅。 |
| `api_coverage.json` | 公開APIの参照節と使用例の対応。 |
| `verification.json` | 対象ヘッダ、各例、本文の検査記録。 |

ZIPを展開したフォルダで `python3 verify_examples.py` を実行すると全例を確認できます。追加検査は `python3 verify_examples.py --first 19 --last 30 --mode sanitize` です。実行ファイルは同梱しておらず、このスクリプトが各環境で作成します。
