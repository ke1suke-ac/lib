# McmcEstimator 利用ガイド

対象ヘッダは `mcmc_estimator_v09.hpp` です。C++20で、隠れた設定値を推定するためのライブラリです。本書はこのヘッダの公開APIと実装に基づきます。ガイドはソースと自動同期せず、明示的な更新指示があったときに更新します。

初めて使う場合は第1〜3章を読み、第4章から自分の問題に近い題材を選んでください。第4章と第5章は、同じU01〜U17の番号で対応しています。U16には、共通の標準偏差の同時推定、残差からの標準偏差推定、整数係数の三つの亜種も含みます。三入力の一次関数を扱いたい場合はU16、一次・二次・正弦の式の種類も推定したい場合はU17へ進めます。第5章は基本17例と3亜種の全20コード例で構成します。各コードは、それぞれ単独でコンパイルできるプログラムです。配布ZIPではヘッダがルート、個別コードが`examples/`に入るため、コンパイル時に`-I .`を指定します。

本書の構成は次のとおりです。

1. 何を解くライブラリか？
2. ユーザー定義関数
3. 利用準備
4. ユースケース
5. ユースケース詳細とコード例
6. 制約・注意点
7. 実装

## 1. 何を解くライブラリか？

### 1.1 入力と結果から、内部の設定値を逆に調べる

装置に電圧を加えると表示値が返ってくるとします。計算式の形は分かっていますが、倍率などの設定値は分かりません。いくつかの電圧と表示値の組を観測でき、設定値を仮定すれば、その装置がどのような値を返すかを自分のプログラムで計算できます。

このとき「どの設定値なら、これまでの観測をよく説明できるか」を調べるのが、このライブラリの役割です。対象は装置に限りません。道路の移動時間、作業の成功率、機械の速度、要素の順序なども扱えます。

利用者が指定するのは、主に次の三つです。

- **何が未知か**：倍率、速度、種類IDなどと、それぞれが取り得る範囲。
- **候補を仮定すると何が起きるか**：入力と候補から、結果を計算する関数。
- **結果をどう比較するか**：観測との差、測定誤差、既知の制約など。

実数・整数・カテゴリの変数には標準の状態型と補助APIがあります。順列や合計1の割合などは、独自の状態型でも表せます。予測式が滑らかである必要はなく、分岐やシミュレーションを含められます。

### 1.2 予測と目的関数

既知の入力から予測値を計算する式を、次のように書きます。

$$\widehat y_i=f(c,x_i;\theta)$$

| 記号 | 意味 |
|---|---|
| $i$ | 観測の番号。1から観測数までを数える |
| $x_i$ | 第$i$番目に与えた既知の入力。数値だけでなく、経路や仕事列などでもよい |
| $y_i$ | その入力に対して実際に観測した結果 |
| $\theta$ | 調べたい隠れパラメータをまとめた候補。複数の数値や順列など |
| $c$ | 地図など、複数の観測で共用する既知のデータ。不要なら省略する |
| $f$ | 利用者が用意する予測関数 |
| $\widehat y_i$ | 候補$\theta$を使った予測結果。観測値そのものとは区別する |

通常の観測モデルでは、次の目的関数を小さくする候補を求めます。

$$E(\theta)=R(\theta)+\sum_{i=1}^{N}w_i\ell(\theta,x_i,y_i),\qquad \widehat\theta\in\operatorname*{arg\,min}_{\theta\in\Theta}E(\theta)$$

| 記号 | 意味 |
|---|---|
| $E(\theta)$ | 候補の悪さを表す実数。コードではenergy。小さいほどよい |
| $R(\theta)$ | 候補全体に対する事前項。観測を見る前に分かる範囲・傾向・制約などを表す |
| $N$ | 観測数。観測がないモデルなら0でもよい |
| $w_i$ | 第$i$番目の観測の非負の重み。通常は1 |
| $\ell(\theta,x_i,y_i)$ | 一件分の損失。予測$f(c,x_i;\theta)$と観測$y_i$の合い方を数値化する。既知データ$c$への依存はここでは省略して書いている |
| $\Theta$ | 許される候補の集合。変数の範囲や順列などの構造を含む |
| $\widehat\theta$ | 目的関数を最小にする候補を表す記号。実際に返るのは、予算内で見つかった最良候補 |
| $\sum$ | 一件ずつの値を全部足す操作 |
| $\operatorname*{arg\,min}$ | 関数の最小値ではなく、その値を与える候補を選ぶ操作 |

例えば、測定誤差が正規分布に従うと考える場合、標準のGaussian損失は次の形です。

$$\ell(\theta,x_i,y_i)=\frac{1}{2}\left(\frac{y_i-f(c,x_i;\theta)}{\sigma_i}\right)^2+\log\sigma_i$$

$\sigma_i>0$は、その観測の誤差の標準偏差、つまり通常のぶれの大きさです。$\log$は自然対数です。ずれを$\sigma_i$で割ることで「通常のぶれの何倍ずれたか」を比べます。二乗するので大きなずれほど強く罰します。$\log\sigma_i$も含まれますが、全ての$\sigma_i$が既知なら候補によって変わらない項です。候補によらない正規分布の共通定数は省いています。

標準損失は重みまで掛けた値を返します。独自損失を書く場合も、必要な重み付けをその関数内で行います。ライブラリが、その戻り値にもう一度重みを掛けることはありません。

観測同士が関連する場合や、損失を一件ずつに分けにくい場合は、**全体の$E(\theta)$を直接返す関数**を渡せます。例えば、時間的に連続する測定誤差はU12で扱います。

### 1.3 最良候補と、不確かさを含む予測

利用目的には、次の二つがあります。

- **一つの有力な候補を得る**：小さい目的関数を持つ設定値を採用し、それを次の計算に使う。
- **複数のあり得る候補から予測する**：一つに決め切れない状況で、予測値の平均やばらつきを求める。

後者を確率として解釈する場合、目的関数は負の対数確率密度として設計します。対象となる分布は次の形です。

$$\pi(\theta\mid\mathcal D)=\frac{\exp(-E(\theta))}{Z}$$

$\mathcal D$は全観測の集合、$\pi(\theta\mid\mathcal D)$は観測を踏まえた候補の確率または密度、$\exp$は自然指数関数です。$Z$は、全候補の確率の合計が1になるようにする正の有限な定数です。連続変数では合計の代わりに積分を使います。$E$が小さい候補ほど重く扱われます。

$R$を事前分布の負の対数、各損失を観測の負の対数尤度にすれば、これは事後分布になります。**尤度**とは「その候補が正しいとしたら、今の観測がどの程度起こりやすいか」です。重みが全て1以外なら、通常の尤度とは異なる重み付けされたモデルになる場合があります。

任意の罰則を加えた目的関数でも最良候補の探索はできます。ただし、その標本のばらつきが現実の不確かさを表すかどうかは、モデルの設計に依存します。また、違う候補でも同じ出力になる問題では、観測だけから真の候補を一つに特定できません。

## 2. ユーザー定義関数

### 2.1 具体的な問題を、既知・未知・誤差に分ける

U01の装置を例に、次の順で整理します。

1. **既知の入力**を決める。ここでは加えた電圧$x$。
2. **観測結果**を決める。ここでは表示値$y$。
3. **未知の設定値**を挙げる。ここでは増加幅$a$、曲がり方$k$、基準値$b$。
4. **再計算の式**を書く。例えば$y=b+ax/(k+x)$。
5. **観測のぶれ方**を決める。例えば標準偏差0.1の測定誤差。
6. **候補の範囲と開始値**を決める。特に$k>0$、$x\geq0$なら分母が0にならない。

「あてはまりのよい式を自動で発見する」機能ではありません。式の形やシミュレーション規則は利用者が指定し、その中の未知の設定値を調べます。複数の式の種類を候補に含めたい場合は、U07・U17のように種類IDを未知変数にできます。入力が複数ある場合も、それぞれを既知のデータとしてまとめ、U16のように未知係数を使って一つの出力を計算できます。

### 2.2 通常は予測関数だけを用意する

`using Solver = McmcEstimator<>;` としたとき、観測モデルで使う関数は次の形です。`Input`、`Row`、`Prediction`は、説明用の型名です。数値、配列、自分で定義した構造体などに置き換えます。

| 関数 | 呼び出される形 | 役割 |
|---|---|---|
| 予測 | `predict(const Input&, const McmcState&)` | 入力と候補から`Prediction`を返す。必須 |
| 予測、Contextあり | `predict(const Context&, const Input&, const McmcState&)` | 地図などの既知データも使って予測する |
| 損失 | `loss(const McmcState&, const Row&, Prediction)` | 一件分の重みを含む損失を`double`で返す。省略時はGaussian |
| 事前項 | `prior(const McmcState&)` | 観測列に対して一度だけ加える値。省略時は0 |
| 事前項、Contextあり | `prior(const Context&, const McmcState&)` | 既知データにも依存する事前項 |
| 全体評価 | `energy(const State&)` | 観測モデルを使わず、目的関数全体を直接返す |

引数は、上の呼び出しに対応できれば値渡しでも構いません。独自の`State`を使う場合は、`McmcEstimator<State>`に替え、各関数の状態引数もその型にします。

標準状態`McmcState`の`real`は`std::vector<double>`、`discrete`は`std::vector<std::int64_t>`です。`Parameters`で宣言した変数の添字を、予測関数に値キャプチャすると、変数の並びを変更したときの取り違えを減らせます。

標準的な一出力の観測は`Solver::Observation<Input>`で表します。複数出力、観測できた部分だけの出力、独自の付帯情報には、自分の観測構造体を使えます。その場合も、予測へ渡す情報を置く`input`メンバーは必要です。損失は観測行全体を受け取れるので、残りのメンバー名は自由です。

入力が三つでも出力が一つなら、`Observation<std::array<double,3>>`と既定のGaussian損失を組み合わせられます。U16がこの形です。U02の「出力が複数ある場合」と区別してください。

### 2.3 損失は、出力の種類と誤差に合わせる

| 観測の性質 | 用意されている損失 | 予測関数が返す値 | 題材 |
|---|---|---|---|
| 誤差を含む実数 | `Gaussian{}` | 観測と同じ単位の予測値 | U01、U16、U17 |
| ときどき極端に外れる実数 | `Huber{threshold}` | 観測と同じ単位の予測値 | U03 |
| 成功・失敗 | `BernoulliLogit{}` | 成功確率を変換する前のlogit | U04 |
| 一定の時間内の件数 | `PoissonLogMean{}` | 件数の平均の自然対数 | U05 |
| 複数出力、未知の誤差尺度など | 独自損失 | 自分の損失が扱える値 | U02、U06、U16-A |
| 時間方向の相関など | 全体評価 | 全観測をまとめた目的関数 | U12 |

logitは、0〜1に制限する前の実数です。例えば予測関数が$t$を返し、成功確率を$p=1/(1+\exp(-t))$と定めます。確率が0.8だからといって、`BernoulliLogit`へ0.8を渡すと別の意味になります。件数でも同様に、`PoissonLogMean`へ渡すのは平均件数そのものではなく、その対数です。

`Gaussian`と`Huber`の`scale`は観測と同じ単位で指定します。測定値が大きいという理由だけで大きな`scale`にするのではなく、「測定がどれくらいぶれるか」を考えます。精度の違いを既に`scale`へ反映しているなら、さらに同じ理由で`weight`を調整する必要はありません。

複数の数値出力がある場合、各成分が独立だと考えられるなら成分ごとの損失を足せます。同時に同じ方向へずれるなどの相関がある場合は、そのまとまりを一つの観測として扱います。U02で両方の書き方につながる具体例を示します。

### 2.4 事前項と制約

「係数$a$は中心$m$付近で、大きさ$s$程度のぶれがあり得る」と考えるなら、事前項に$(a-m)^2/(2s^2)$を加える方法があります。$m$は中心、$s>0$は事前に想定する標準偏差です。これらは利用者が指定する既知の値です。小さい$s$ほど中心へ強く寄せ、大きい$s$ほど観測結果を優先しやすくなります。

範囲制約は`Parameters`へ書けます。変数同士に関わる制約は、違反時に正の無限大`std::numeric_limits<double>::infinity()`を返す方法もあります。ただし**少なくとも一つ、有限の目的関数を持つ初期候補が必要**です。全初期候補が制約違反なら、制約を満たす点を自動的に見つけに行くことはできません。合計1や順列のような構造は、U08・U11のように、有効な候補だけを作れる状態と変更方法にする方が扱いやすい場合があります。

### 2.5 評価関数の約束

- 同じ候補・同じモデルなら、同じ目的関数を返してください。途中まで計算した値を再利用するためです。
- 戻り値は有限の`double`または正の無限大です。`NaN`と負の無限大は禁止です。負の有限値は使えます。
- 予測関数から参照したデータを、通知せず書き換えないでください。所有モデルの変更は`update_model`などを通します。
- モデルは関数オブジェクトを所有しますが、ラムダの参照キャプチャ先までは所有しません。長く保持するデータは値キャプチャかContextに入れるのが分かりやすい方法です。
- `model().prediction`など読み取り用の補助関数を使う予測は、モデルを変更せず呼び出せるようにします。
- 予測の実行中に推定器自身を更新・再実行しないでください。

内部で乱数を使うシミュレータは、そのまま毎回違う結果を返させる使い方には適しません。固定した乱数列を入力データとして持てば、固定された近似目的関数の探索には使えます。しかし、それが乱数の不確かさまで含めた正しい尤度になるわけではありません。標本を確率として使うには、観測が起こる確率のモデルを別に設計する必要があります。

## 3. 利用準備

### 3.1 ヘッダと、通常の構成

`#include "mcmc_estimator_v09.hpp"`で読み込みます。C++20と、ヘッダが使う`<bits/stdc++.h>`を提供するコンパイラ環境が必要です。配布ZIPのルートでU01をビルドする場合は、`g++ -std=c++20 -O2 -I . examples/mcmc_estimator_guide_example_u01_v06.cpp -o mcmc_guide_u01_v06`を実行します。`-ffast-math`は指定しません。

通常の順序は、**変数を宣言 → 観測モデルを作成 → Sessionを作成 → 予算を渡して実行**です。型名が長いSessionを自分で書く必要はなく、`auto fit = ...;`で受け取れます。U01に一式を示します。

| 作成方法 | 引数とデフォルト | 用途 |
|---|---|---|
| `Solver::make_observation_model(data, predict, loss, prior)` | `data`は`std::vector<Row>`、`predict`は必須。`loss`省略時Gaussian、`prior`省略時0 | 独立な観測行から全体評価を作る |
| `Solver::make_context_model(data, context, predict, loss, prior)` | 上記に値所有する`Context`を追加。予測と事前項の先頭にContext引数が付く | 既知のグラフなどを共用する |
| `Solver::make_numeric_session(parameters, model, param, move, chains)` | `Parameters`、モデルが必須。`param=Param{}`、`move=MoveParam{}`、`chains=1`、型は`std::size_t` | 実数・整数・カテゴリに標準の変更方法を使う |
| `Solver::make_session(initial, model, move, param)` | `initial`は空でない`std::vector<State>`。モデル・変更方法が必須。`param=Param{}` | 独自状態や独自の変更方法を使う |
| `Solver::Session<Model,Move>(initial, model, move, param)` | 上と同じ。通常は型を推論するfactoryで足りる | Sessionを直接構築したい場合 |
| `Solver(param)` | `param=Param{}` | 管理を自分で行う下位API用。通常の利用では不要 |

`model`には、観測モデルの代わりに全体評価関数も渡せます。Session構築時に目的関数の評価は行いません。初回の`solve`または`sample`で、与えた予算を使って評価します。モデルと変更方法は値として所有されるので、大きいデータや関数オブジェクトを移す場合は`std::move`を使えます。

### 3.2 未知変数の宣言

`Solver::Parameters p;`は空の変数集合を作ります。

| 宣言 | 引数の型と既定値 | 設定の考え方 |
|---|---|---|
| `p.real(initial, lower, upper, scale)` | 全て`double`。`scale=0`、他は必須 | 実数。初期値は有限で範囲内。正のscaleは最初の変更幅 |
| `p.integer(initial, lower, upper)` | 全て`std::int64_t`。省略不可 | 隣り合う整数に近さの意味がある値。例：整数補正 |
| `p.category(initial, count)` | 初期IDは`std::int64_t`、種類数は`int`。省略不可 | IDは0以上count未満。IDの数値的な距離を使わない |
| `p.initial()` | `const State&`を返す | 宣言済み変数の初期値を読む |
| `p.domain()` | `const Solver::Domain&`を返す | 宣言済み変数の範囲を読む |

追加メソッドの戻り値は`std::size_t`の添字です。実数は`state.real[index]`、整数とカテゴリは共通の`state.discrete[index]`へ入ります。初期値と下限・上限を同じにすると固定変数になります。

実数の`scale=0`は自動設定の指定です。有限の上下限なら範囲幅の10%、固定値なら1になります。片側または両側を無限にする場合は、自動設定ができないため正の有限なscaleを明示します。scaleは変数の単位で考え、「まずこれくらい動かしたい」という値を入れます。対象が0.01程度で変化するなら、単に大きな範囲を与えるより、その大きさを明示する方が意図を伝えられます。

整数は絶対値と範囲幅が$10^9$以下の通常のAHC規模を想定します。変数数は使用中に増減できません。`Parameters`を後から変更しても、作成済みSessionには反映されません。

U16の基本形では、三入力自体は観測行に置き、`real`で四係数を宣言します。U17では、`category(0,3)`で種類を一つ、`real`で係数を二つ宣言します。どちらも係数の初期値を0、範囲を`[-10,10]`としています。これは例の設定であり、実際の問題では既知の範囲に合わせて変更します。

U16の亜種では、次のように準備します。コードは第5.16節にまとめています。

| 亜種 | 未知変数と設定 | 観測・損失の準備 |
|---|---|---|
| U16-A | 四係数を`real(0,-10,10)`、標準偏差を`real(1.0,0.001,10.0)`で宣言 | 全行のscale・weightを既定値1とし、候補の標準偏差を使う独自損失を渡す |
| U16-B | 四係数を`real(0,-10,10)`で宣言。標準偏差は後で計算 | 全行のscale・weightを1として既定Gaussianを使い、solve後に残差を集計する |
| U16-C | 四係数を`integer(0,-10,10)`で宣言 | 実数の入力・観測値と既定Gaussianを使う。既知の標準偏差は行のscaleに指定できる |

U16-A・U16-Bでは全観測に共通する未知の標準偏差を調べるため、観測行へ標準偏差の推測値を埋める必要はありません。U16-Aの0.001〜10という範囲は、観測値の単位に合わせて設定する例です。

直接範囲を組み立てる場合、`Solver::Domain`に次のフィールドがあります。

| フィールド | 型・デフォルト |
|---|---|
| `real` | `std::vector<Domain::Real>`、空。各要素は`double lower, upper, scale`で、3値とも指定する |
| `discrete` | `std::vector<Domain::Discrete>`、空。各要素は`std::int64_t lower, upper`と`bool categorical=false` |

`Solver::make_numeric_move(domain, move_param)`または`Solver::NumericMove(domain, move_param)`で変更方法を作れます。`move_param`は省略時`MoveParam{}`です。直接使う場合は、別に用意する各初期状態とDomainの形・順番を一致させます。

### 3.3 実行設定 `Param`

`Solver::Param settings;`で全て既定値になります。変更したいフィールドだけ代入してから、Sessionの作成時に渡します。作成済みSessionへParamを設定し直すメソッドはありません。

| フィールド | 型 | デフォルト | 選び方・意味 |
|---|---|---|---|
| `seed` | `std::uint64_t` | `1` | 乱数の開始値。比較実験では固定し、別の試行では変える |
| `sample_stride` | `std::uint64_t` | `1` | 各鎖の何遷移ごとに標本を出すか。正の値。出力・集計が重い場合に増やす。独立性の保証にはならない |
| `start_temperature` | `double` | `1` | Search開始時の温度。正の有限値。普段の目的関数差と比べて決める |
| `end_temperature` | `double` | `0.001` | Search終了側の温度。正の有限値。最後に許容したい小さな悪化の大きさに合わせる |
| `cooling_steps` | `std::uint64_t` | `0` | Searchで予定する総遷移数。0なら予算から決定。分割探索を同じ計画で進めたいときは全体の予定を指定 |
| `cooling_deadline` | `Solver::Clock::time_point` | `Clock::time_point::max()` | Searchの冷却の絶対期限。指定すると遷移数計画より優先 |
| `observation_check_interval` | `std::size_t` | `32` | 観測モデルの何項ごとに時間を確認するか。正の値。一項が重いなら1にする |
| `warmup_steps` | `std::optional<std::uint64_t>` | `std::nullopt` | `sample`の前に行う準備遷移数。明示した0なら省略。収束を保証する値ではない |

準備遷移数が未指定の場合、標準数値状態では、可動変数があれば`max(512, 64 * 可動変数数 * 鎖数)`、全て固定なら0です。独自の変更方法では通常`512 * 鎖数`です。指定値も自動値も、**全鎖を合わせた遷移数**です。

温度はSearchにだけ作用します。目的関数全体を大きくすると、同じ温度でも悪化を許容しにくくなります。正規誤差のモデルで、誤差尺度を適切に設定したうえで既定値から試し、固定した予算と複数の入力で調整してください。Sampleの分布を調整するためにSearch温度を変更することはできません。

冷却計画を両方省略した場合、最初に計算を行うSearchの予算を使います。その予算に時間期限があれば時間、なければ遷移数です。同じ目的関数で分割してSearchを続ける場合、呼ぶたびに冷却が最初へ戻るわけではありません。元の計画が終わった後は終端温度で継続します。モデル更新後は新しい計画で始まります。

### 3.4 標準数値提案の設定 `MoveParam`

| フィールド | 型 | デフォルト | 選び方・意味 |
|---|---|---|---|
| `adapt` | `bool` | `true` | Searchと準備中に変更幅を調整する。幅を自分で固定したいならfalse。Sample中はこの設定によらず調整しない |
| `difference_probability` | `double` | `0` | 準備・Sampleで他の二鎖の差を使う確率。3鎖以上と可動実数が必要 |
| `cauchy` | `bool` | `true` | Searchの座標変更で、ときどき大きく動く幅を使う。falseなら正規分布の幅 |
| `stretch_probability` | `double` | `0.3` | 準備・Sampleで他の一鎖を中心に伸縮する確率。2鎖以上と可動実数が必要 |

二つの確率は非負で、合計は1以下にします。必要な鎖数や実数変数がない場合は座標の変更に戻ります。これらの集団提案はSearchでは使いません。

まず既定値を使い、変数間の強い関連で動きにくいときに鎖数と合わせて比較するのが分かりやすい選び方です。鎖数を増やすと同じ総予算で一鎖が受け取る計算量は減ります。`make_numeric_session`は全鎖に同じ初期値を複製します。異なる初期候補を使いたい場合は、同じDomainで`make_session`を使うか、作成後に`restart`で渡します。

集団提案だけに頼ると、初期候補の広がりによっては探索できる領域が狭くなります。特に同じ初期値を複製して伸縮提案だけにすると動けません。通常の座標変更が残る設定から始めてください。

### 3.5 観測行と損失の設定

`Solver::Observation<Input>`のフィールドは次のとおりです。

| フィールド | 型 | デフォルト | 用途 |
|---|---|---|---|
| `input` | `Input` | 独自のメンバー初期値なし。明示して渡す | 予測関数への入力 |
| `value` | `double` | 独自のメンバー初期値なし。明示して渡す | 観測結果 |
| `scale` | `double` | `1` | Gaussian・Huberで使う既知の正の誤差尺度 |
| `weight` | `double` | `1` | 非負の有限な重み |

例えば`Row{x, y, sigma, weight}`の順です。`Row{x,y}`ならscaleとweightは1です。デフォルト構築して未設定の`value`を読む使い方はしません。

Gaussian、BernoulliLogit、PoissonLogMeanには利用者が設定するフィールドがありません。Huberは`double threshold=1.345`を持ちます。正の有限な固定値として使い、小さい値では大きな残差の影響を早めに抑えます。詳細な式はU03にあります。

観測モデルは`size()`、`observations()`を公開します。後者は更新まで有効な`std::span<const Row>`です。`prediction(input,state)`は予測関数の生の結果を返し、`mean_prediction(input,state)`は損失が対応している場合に平均へ変換します。Gaussian・Huberではそのまま、BernoulliLogitでは成功確率、PoissonLogMeanでは件数平均です。独自損失に`mean`がない場合、`mean_prediction`は使えません。

### 3.6 計算予算と実行メソッド

| 予算の作り方 | 引数・デフォルト | 意味 |
|---|---|---|
| `Budget::for_us(us)` | 非負の`std::int64_t`、省略不可 | 呼んだ時点からのマイクロ秒数で期限を作る |
| `Budget::for_steps(steps)` | `std::uint64_t`、省略不可 | 完了した提案・遷移数を制限する。再現確認に向く |
| `Budget::until(end, steps)` | 絶対時刻、`std::uint64_t steps=UINT64_MAX` | 時間と遷移数を併用し、先に到達する方で止める |
| `Budget{}` | `deadline=Clock::time_point::max()`、`remaining_steps=UINT64_MAX` | 実用上ほぼ無制限。AHCでは明示的な制限を推奨 |

`Solver::Clock`は`std::chrono::steady_clock`です。`Budget::for_us`を作った後に別の準備をすると、その時間も期限までの残り時間を消費します。実行の直前に作るか、競技全体で共通の絶対期限を使います。

初期評価やモデル更新後の再評価は、遷移数には含まれません。時間予算には含まれます。制限は協調的であり、一回のユーザー関数を途中で強制停止できません。結果のコピーや標本処理にも時間が掛かります。コンテストの出力などに必要な余裕を別に残してください。

| Sessionのメソッド | 引数 | 戻り値と使い方 |
|---|---|---|
| `solve(budget)` | `Budget`を値渡し | Searchを進め、`Result`を返す。最良候補を保持したいとき |
| `solve_view(budget)` | `Budget`を値渡し | `Report`のみ。大きい状態の結果コピーを避けたいとき。候補は`best()`で読む |
| `best()` | なし | `Best{const State* state, double energy}`。候補なしならnullptr。次の実行・変更までのビュー |
| `sample(budget, project)` | `project(const State&)` | 予測・変換値を集計した`SampleResult<Value>`。必要な準備も同じ予算で行う |
| `sample_each(budget, emit)` | `emit(const State&, double energy, int chain)` | 標本ごとに処理し、`Report`を返す。自分で保存・集計したいとき |
| `warmup_remaining()` | なし | 残る準備遷移数を`std::uint64_t`で返す |
| `revision()` | なし | 現在の目的関数・再初期化に対応する版番号 |
| `model()` | なし | 所有モデルへの読み取り専用参照 |

`Result`には`std::optional<State> state`、`double energy`、`Report report`、`std::uint64_t revision`があります。`state`は現在の目的関数で評価済みの最良候補のコピーです。得られていなければ空、energyは正の無限大です。空かどうかを確認してから利用します。

`sample`のprojectは`double`、`std::array<double,N>`、`std::vector<double>`を返します。結果の`summary`は、`count()`、`mean()`、`variance()`を持ちます。平均は0件なら空、分散は2件未満なら空です。分散は各成分の標本分散であり、平均値の推定誤差や成分間の共分散ではありません。戻り値には`report`、`revision`、`warmup_remaining`もあります。

`sample`の集計は呼ぶたびに新しくなります。一方、推定器の鎖・準備の進捗は継続します。複数回の結果をまとめる場合は、U15の`sample_each`と`Session::Moments<Value>`のように利用側で集計します。`Moments::add(value)`へ渡す配列・vectorの長さは途中で変えません。

`Report`の全フィールドは次のとおりです。

| フィールド | 型・初期値 | 意味 |
|---|---|---|
| `stop` | `Stop::Budget` | 通常の予算による終了。`Stop::NoFiniteState`は全初期候補の評価を終えても有限候補がないことを示す |
| `steps` | `std::uint64_t = 0` | 今回完了した遷移数。sampleでは準備分も含む |
| `accepted` | 同上 | 受理した提案数 |
| `evaluations` | 同上 | 完了した状態評価数。初期評価・再評価も含む |
| `terms` | 同上 | 計算した評価項数。観測モデルでは事前項も一項として数える |
| `emitted` | 同上 | 今回出力した標本数。準備中は出力しない |
| `elapsed_us` | `std::int64_t = 0` | 今回の呼び出しの実時間。マイクロ秒 |
| `synchronized` | `bool = false` | 保持中の各鎖の評価を現在のモデルに揃え終わったか |
| `has_estimate` | `bool = false` | 現在のモデルで評価済みの有限候補があるか |

全鎖の再評価が終わる前でも、有限候補を一つ評価できれば`has_estimate`がtrueになることがあります。`stop`や`has_estimate`は、統計的な収束や大域最適性を表すものではありません。

### 3.7 差分更新と、再構築の境界

ターン制問題では、Sessionをターンループの外で一度作り、同じオブジェクトを使います。更新メソッドは変更を登録し、次の`solve`または`sample`の予算で必要な評価を行います。変更直後は、以前のenergyを新しいモデルの値として使えないため、公開される最良候補は一度空になります。

| 変更したいもの | 呼び出し | 継続できるもの・注意 |
|---|---|---|
| 観測の追加 | `add_observations(std::vector<Row> batch)` | 現在の候補を保持。条件が整えば追加分だけを評価する。新しい行だけ渡す。空は変更なし |
| 観測の訂正・全体差し替え | `replace_observations(std::vector<Row> data)` | 残す全観測を指定。以前の候補を全体再評価する。空なら全観測を除く |
| 古い観測の削除 | `retain_last(std::size_t count)` | 末尾count件を残す。countが現在件数以上なら変更なし。0なら全て除く |
| 所有する既知Context、独自モデルの公開データ | `update_model(edit)` | `edit(Model&)`内で変更し、全体再評価を登録する。候補と数値変更幅を再利用する |
| 初期値・鎖数 | `restart(std::vector<State> initial)` | 同じ形・範囲の開始状態へ変更。乱数と探索履歴を初期化し、数値提案の学習済み幅は保持 |
| 変数数、変数の意味、Domainの上下限 | Sessionを作り直す | 新しい初期状態・モデル・変更方法を整合させる |
| モデルや変更方法の型、Param・MoveParam | Sessionを作り直す | 作成後に設定し直す専用メソッドはない |

これらの更新は、標本採取前の準備を再設定します。目的関数を変更したら、外部に保持する標本・平均も新しい集計に切り替えます。`revision()`で変更を検出できます。追加・更新を複数回登録してから一度だけ実行しても構いません。途中で時間切れになった場合は、更新の登録を繰り返さず、実行メソッドを再び呼びます。

観測モデルの予測関数・損失・事前項には、それぞれを書き換える専用setterはありません。後から変更したい既知情報は、先にContextや独自モデルのデータとして設計します。ただし、損失の呼び出しにはContext引数が付かないため、Contextを使う予測・事前項と混同しないでください。例えばHuberのthresholdを直接設定し直すAPIはなく、必要なら適切なモデルを作り直します。

**既知データの訂正と、実際の時間変化は異なります。** 地図の登録距離が間違っていた場合、Contextを訂正して過去の観測も計算し直すことができます。一方、道路状況が時刻によって本当に変わった場合、過去の観測には当時の状況を値または不変の識別子として残します。最新のContextで過去を一律に再計算すると、別のモデルになってしまいます。隠れた係数自体の時間変化も自動追跡するわけではありません。

### 3.8 独自状態と独自提案

数値配列以外の状態でも、コピーできる型なら`McmcEstimator<State>`を使えます。変更方法は次の呼び出しに対応させます。

`move(const State& current, State& candidate, Solver::Rng& rng, const Solver::MoveContext& ctx)`

呼び出し時点でcandidateにはcurrentのコピーが入っています。利用者は変更したい箇所だけを書き換えます。

| 用意する方法 | 関数の戻り値 | 使い方 |
|---|---|---|
| `Solver::symmetric_move(function)` | `bool` | trueで候補を評価、falseでその試行を棄却。正逆が同じ提案確率であることを利用者が保証する |
| `Solver::hastings_move(function)` | `double` | 逆向き確率/順向き確率の対数。負の無限大なら棄却。正の無限大とNaNは禁止 |
| そのままの関数・関数オブジェクト | 同じ対数提案比 | 通常のSessionではSearch用。Sampleを公開するには安全性の明示が必要 |

`hastings_move`で、現在が$s$、候補が$t$、提案確率を$q(t\mid s)$と書く場合、返す値は$\log q(s\mid t)-\log q(t\mid s)$です。ここで$q$は目的関数から得る確率ではなく、**自分の変更手順がその候補を選ぶ確率**です。例えば状態と無関係な同じ分布$q$から候補を選ぶなら`log(q_current)-log(q_candidate)`です。

独自の関数オブジェクトに`static constexpr bool sampling_safe=true;`を宣言する方法もあります。ラッパーもこの宣言も、数学的な正しさを検査する機能ではありません。Sample中に提案分布を勝手に適応させないことや、正しい提案比を返すことは利用者が保証します。U08とU11では、説明しやすい対称な変更方法を使います。

`MoveContext`は`Phase phase`、`double temperature`、`std::size_t chain`、`std::span<const State> population`、`std::uint64_t revision`を持ちます。phaseは`Search`・`Warmup`・`Sample`、chainは現在の鎖の番号です。populationは他の鎖も含む読み取り専用ビューで、コールバック中だけ参照します。

必要なら`feedback(bool accepted, const MoveContext&)`を変更方法に実装できます。完了した試行の結果を受け取る拡張点です。標本採取中に分布を変える適応には使いません。

乱数は渡された`Rng`から取れます。`uniform()`は0以上1未満、`integer(n)`は0以上n未満の整数、`normal()`は平均0・標準偏差1の正規乱数です。nは正のintです。`next()`は64bit整数、`log_uniform()`は0より大きく1以下の一様乱数の対数を返します。独立に構築する場合の`Rng(seed)`のseed既定値は1です。

### 3.9 下位APIを使う場合だけ必要な準備

通常はSessionに任せます。評価用データの所有や更新管理を既存プログラム側に置きたい場合は、`Solver engine(settings);`を直接作れます。

| メソッド | 用途・契約 |
|---|---|
| `reset(initial_span, evaluate, budget)` | 空でない`std::span<const State>`をコピーし初期評価する。履歴を初期化 |
| `run(phase, evaluate, propose, budget, emit)` | 指定フェーズを続行する。emit省略も可。自動の準備フェーズ管理は行わない |
| `refresh(evaluate, budget)` | 外部データや全体評価を変更した後、候補を現在の評価に揃える |
| `append_energy(delta, budget)` | 完全に同期済みかつ有限最良候補がある状態で、各候補の正確な増分を加える |
| `observe(model, batch, budget)` | 観測モデルへ新規行を一度追加し、同期評価する |
| `replace_observations(model, data, budget)` | 観測モデルの全行を置き換え、同期評価する |
| `best()` | 次の実行・変更まで有効な最良候補のビュー |

上表の計算メソッドは`Report`を返します。**下位APIのBudgetは参照渡し**で、残り遷移数が減ります。Sessionは値渡しです。下位APIでは、実行を中断した後も同じ評価・提案を渡し、外部モデルを変更した際の無効化を利用者が管理します。

`append_energy`のdeltaは「新しい目的関数−古い目的関数」を返します。U14の`candidate`は「新しい目的関数全体」を返すため、意味が違います。`append_energy`が途中で止まったら、次は**更新後の全体評価を渡したrun**で同期を続けます。同じ増分を再登録しません。また、この下位APIにはSessionのSample用のコンパイル時制限がないため、提案の正しさと準備は全て利用側で管理します。

## 4. ユースケース

この章では問題の意味に集中します。次章の同じ番号で、具体的な数値・式・呼び出し方・実行可能なコードへ進みます。

| 番号 | 具体的な問題 | 未知のもの | 必ず指定する既知情報 | 追加で調整できるもの |
|---|---|---|---|---|
| U01 | 電圧を上げると表示が頭打ちになる装置を調べる | 増加幅、曲がり方、基準値 | 電圧と表示値、誤差の大きさ、応答式 | 係数の範囲、初期値、計算予算 |
| U02 | 二つの計測系の座標を対応させる | 共通倍率と横・縦のずれ | 対応する点、各軸の精度、測れた軸 | 両軸の誤差の関連の強さ |
| U03 | ときどき大きな待ち時間が入る処理の通常時間を調べる | 仕事量あたりの時間、固定時間 | 仕事量と処理時間、通常時の誤差 | どの程度のずれから影響を抑えるか |
| U04 | 作業の成功・失敗から、未実施の作業の成功確率を予測する | 難しさと成功しやすさの関係 | 作業の難しさ、成否、新しい作業 | 係数への事前の想定、標本の計算量 |
| U05 | 観測時間が違う要求件数から、今後の件数を予測する | 負荷に応じた単位時間あたりの発生率 | 負荷、観測時間、件数、予測する時間 | 発生率モデル、係数の範囲 |
| U06 | 測定系の補正値と、測定のぶれを一緒に調べる | 直線の係数と誤差の標準偏差 | 入力と表示、許容する誤差の範囲 | 対数尺度上での事前の想定 |
| U07 | 装置の種類も分からない状態から、応答を推定する | 種類、実数倍率、整数補正 | 種類ごとの式、入力と表示 | 種類数、各値の範囲 |
| U08 | 三種類の染料の混合割合を調べる | 非負で合計1の三割合 | 純粋な各染料の応答、混合物の測定 | 測定精度、変更幅、標本数 |
| U09 | 移動時間から道路種別のコストを推定し、地図の誤登録を直す | 道路種別ごとの距離あたり時間 | 辺・道路種別・距離、通った経路、所要時間、訂正内容 | 種別数、範囲、訂正前後の予算 |
| U10 | 作業全体の終了時刻から、二台の機械の速度を調べる | 各機械の速度 | 仕事列、割当規則、終了時刻、測定精度 | 次に投入したい仕事列 |
| U11 | 誤りを含む比較から、要素の隠れた順番を調べる | 全要素の順列 | 比較結果、比較が逆になる確率 | 初期順列、試行数 |
| U12 | 続けて同じ方向へずれるセンサーを補正する | 補正線の係数 | 時間順の測定、誤差の持続率、新しい誤差の大きさ | 次時刻の入力 |
| U13 | 毎ターン観測を追加し、その時点の推定を使う | ターン間で共通の処理時間の係数 | 新しく得た観測、ターンごとの予算 | 観測訂正、古い観測の削除 |
| U14 | 二端子ずつの測定から、多数の端子の電圧を調べる | 基準端子以外の電圧 | 測定した端子の組、電圧差、基準電圧、測定精度 | 電圧の範囲、依存項だけの評価 |
| U15 | 推定の不確かさも考えて、次に使う設定値を選ぶ | 設定値とコストの関係 | 試した設定とコスト、選べる設定の一覧 | 予測のばらつきをどれだけ嫌うか |
| U16 | 三つの設定値と表示値から、一次関数の係数を調べる | 三つの効き方と基準表示。亜種では共通の標準偏差も推定、または係数を整数に限定 | 三入力と出力の記録、係数の範囲 | 観測精度、標準偏差の扱い、計算予算 |
| U17 | 装置の応答が一次・二次・正弦のどれかを、係数と一緒に調べる | 全観測に共通する種類と二実数 | 三つの候補式、入力と出力、係数の範囲 | 観測精度、計算予算 |

U01は一つの実数出力の基本形です。U02〜U06では、出力の性質や誤差の仮定が変わります。U07・U08・U11は、未知のものの型や構造が変わる題材です。U09・U13は、観測後も計算資源を引き継ぐ題材です。U10は再現計算がシミュレータ、U12は観測間に関連がある場合です。U14は評価量の削減、U15は推定後の行動選択に焦点を当てます。U16は複数の既知入力を一つの出力へまとめる基本形、U17は実数係数に加えて式の種類も選ぶ具体例です。U07は整数補正も含む混合型を扱います。

U16-Aは四係数と共通の標準偏差の同時推定、U16-Bは四係数を求めてから残差を使う標準偏差推定、U16-Cは整数係数の推定です。いずれもU16の三入力の一次モデルを使う亜種として、第5.16節の中で説明します。

これらを組み合わせることもできます。例えば、道路の時間推定に外れ値対策を入れる、機械の種類を整数IDで含める、ターンごとに成功・失敗を追加する、などです。組み合わせる際は、出力形式、損失、候補の制約が矛盾しないことを確認します。

## 5. ユースケース詳細とコード例

全ての例は一つの関数を呼んで推定できる形にし、`main`には具体的な入力と戻り値の使い方を置いています。例同士は独立したプログラムです。同名の`Solver`や`main`を含むので、複数の例を一つのファイルへそのまま連結しないでください。

例の遷移数は、実行結果を確認しやすくするための設定です。AHCの実時間予算に置き換える場合は、例中でBudgetを作る箇所、または引数で受け取る箇所を使い、準備と出力にも余裕を残します。これらの数値は全問題で最適という意味ではありません。

配布ファイルは、ガイドとコード例の版番号をv06で揃えています。対応するライブラリの版番号はv09です。この二つの版番号は別のものです。ZIPは`mcmc_estimator_guide_examples_v06.zip`、展開先のルートは`mcmc_estimator_guide_examples_v06/`です。

全例を一つにまとめた`mcmc_estimator_guide_examples_v06.cpp`も同梱しています。例えばU04を選ぶ場合は、ルートで`g++ -std=c++20 -O2 -DMCMC_GUIDE_EXAMPLE=4 mcmc_estimator_guide_examples_v06.cpp -o mcmc_guide_demo_v06`を実行します。未指定ならU01です。基本例は1〜17、U16の亜種は161〜163で選びます。個別ファイルと同じコードを、コンパイル時に一つ選ぶ構成です。

| ユースケース | ZIP内の個別コード | MCMC_GUIDE_EXAMPLE |
|---|---|---|
| U01 | `examples/mcmc_estimator_guide_example_u01_v06.cpp` | `1` |
| U02 | `examples/mcmc_estimator_guide_example_u02_v06.cpp` | `2` |
| U03 | `examples/mcmc_estimator_guide_example_u03_v06.cpp` | `3` |
| U04 | `examples/mcmc_estimator_guide_example_u04_v06.cpp` | `4` |
| U05 | `examples/mcmc_estimator_guide_example_u05_v06.cpp` | `5` |
| U06 | `examples/mcmc_estimator_guide_example_u06_v06.cpp` | `6` |
| U07 | `examples/mcmc_estimator_guide_example_u07_v06.cpp` | `7` |
| U08 | `examples/mcmc_estimator_guide_example_u08_v06.cpp` | `8` |
| U09 | `examples/mcmc_estimator_guide_example_u09_v06.cpp` | `9` |
| U10 | `examples/mcmc_estimator_guide_example_u10_v06.cpp` | `10` |
| U11 | `examples/mcmc_estimator_guide_example_u11_v06.cpp` | `11` |
| U12 | `examples/mcmc_estimator_guide_example_u12_v06.cpp` | `12` |
| U13 | `examples/mcmc_estimator_guide_example_u13_v06.cpp` | `13` |
| U14 | `examples/mcmc_estimator_guide_example_u14_v06.cpp` | `14` |
| U15 | `examples/mcmc_estimator_guide_example_u15_v06.cpp` | `15` |
| U16 | `examples/mcmc_estimator_guide_example_u16_v06.cpp` | `16` |
| U16-A | `examples/mcmc_estimator_guide_example_u16a_v06.cpp` | `161` |
| U16-B | `examples/mcmc_estimator_guide_example_u16b_v06.cpp` | `162` |
| U16-C | `examples/mcmc_estimator_guide_example_u16c_v06.cpp` | `163` |
| U17 | `examples/mcmc_estimator_guide_example_u17_v06.cpp` | `17` |

### 5.1 U01：頭打ちになる装置の応答

個別コード：`examples/mcmc_estimator_guide_example_u01_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=1`。

**具体的な問題。** 加える電圧が0なら表示は基準値になり、電圧を大きくすると表示は増えるものの、増え方が次第に小さくなる装置を考えます。既知の式に三つの未知係数があります。

$$f(x;\theta)=b+\frac{ax}{k+x},\qquad \theta=(a,k,b)$$

$x\geq0$は電圧、$a>0$は基準値からの増加幅、$k>0$は曲線の曲がり方、$b$は基準値です。例えば$x=k$なら増加分は$a/2$です。測定値$y_i$とこの予測を、既知の誤差$\sigma_i$を使うGaussian損失で比べます。事前項は0とし、指定範囲内の候補を使います。

**モデルからコードへ。** `Parameters::real`の戻り値`a,k,b`は係数の値ではなく、`state.real`の添字です。`Row`のinputが$x_i$、valueが$y_i$、scaleが$\sigma_i$に対応します。予測関数には、候補を使った式だけを書きます。

**呼び出しと結果。** `fit_response(rows, query, budget)`は、宣言・観測モデル・Sessionを作り、`solve`を一度呼びます。戻り値は4要素で、最後の要素がqueryでの予測です。`nullopt`なら有限候補を得ていません。`main`の測定は$a=4,k=2,b=1$から作っているので、電圧3での予測は3.4付近になります。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 入力: input=加えた電圧x>=0、value=表示値y、scale=既知の測定誤差。
// query: 次に予測したい電圧。budget: 今回の計算予算。
// 出力: [増加幅a, 曲がり方k, 基準値b, queryでの予測]。未評価ならnullopt。
std::optional<std::array<double,4>> fit_response(
    std::vector<Row> rows, double query, Solver::Budget budget) {
    Solver::Parameters p;
    const auto a = p.real(2, 0.1, 10);  // 初期値、下限、上限。幅は自動設定。
    const auto k = p.real(1, 0.1, 10);  // k>0にして、分母が0になることを防ぐ。
    const auto b = p.real(0, -5, 5);
    auto predict = [a,k,b](double x, const McmcState& s) {
        assert(x >= 0);
        return s.real[b] + s.real[a]*x/(s.real[k]+x);
    };
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto fit = Solver::make_numeric_session(p, std::move(model));
    const auto r = fit.solve(budget);   // 初期評価と探索をまとめて行う。
    if (!r.state) return std::nullopt;
    const auto& s = *r.state;          // rが所有する最良候補。
    return std::array<double,4>{s.real[a], s.real[k], s.real[b],
                               fit.model().prediction(query,s)};
}

int main() {
    // 例では a=4, k=2, b=1 の装置から、誤差なしの値を作る。
    std::vector<Row> rows;
    for (double x : {0.0, 0.5, 1.0, 2.0, 4.0, 8.0})
        rows.push_back({x, 1+4*x/(2+x), 0.1});
    const auto r = fit_response(rows, 3, Solver::Budget::for_steps(80000));
    // 実時間で制限する場合は、直前にBudget::for_us(20000)などを渡す。
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

入力電圧が狭い範囲に集中すると、違う$a,k$でも似た曲線になり、係数を区別しにくくなります。異なる電圧を測ることが、単に探索時間を増やすより役立つ場合があります。戻り値の係数が真値に近いことと、必要な入力での予測がよいことは、分けて評価してください。

### 5.2 U02：二次元の座標、欠測、誤差の相関

個別コード：`examples/mcmc_estimator_guide_example_u02_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=2`。

**具体的な問題。** 一つの物体を二つの計測系で測ると、片方は倍率が違い、さらに横と縦にずれているとします。対応する点から、その変換を求めます。一部の点では横または縦だけが測れます。

$$\widehat u=cx+d_x,\qquad \widehat v=cy+d_y,\qquad \theta=(c,d_x,d_y)$$

$(x,y)$は変換前の点、$(\widehat u,\widehat v)$は変換後の予測点です。$c$は共通倍率、$d_x,d_y$は二方向のずれです。観測点を$(u,v)$、既知の各軸の標準偏差を$\sigma_u,\sigma_v$とすると、単位をそろえたずれは$z_u=(u-\widehat u)/\sigma_u$、$z_v=(v-\widehat v)/\sigma_v$です。

両軸の誤差が独立なら、二つの二乗損失を足せます。両方が同時に右上へずれやすいなどの関連がある場合は、その強さを既知の相関係数$\rho$で表します。$-1<\rho<1$です。両方観測した一件の損失は次です。

$$\ell=\frac{z_u^2-2\rho z_uz_v+z_v^2}{2(1-\rho^2)}+\log\sigma_u+\log\sigma_v+\frac{1}{2}\log(1-\rho^2)$$

$\rho=0$なら独立な二軸の損失になります。$\rho>0$なら、同じ方向へ一緒にずれることを、逆方向へずれることより起こりやすいと見なします。ここで$\rho$と二つの標準偏差は推定せず、測定系の既知情報として渡します。

**モデルからコードへ。** 標準の一出力の行を使わず、`PointRow`に点・標準偏差・測定の有無・相関を入れます。予測は2要素配列を返し、独自損失がそれを読みます。一軸だけ測れた行では、残った軸のGaussian損失だけを使います。両軸ともない行の損失は0です。

**呼び出しと結果。** `fit_coordinates`へ観測点列と予測したい点を渡します。`solve`後、`prediction`で2次元の予測を取り出します。戻り値は`[c,dx,dy,予測横,予測縦]`です。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Point = std::array<double,2>;
struct PointRow {
    Point input, value;                    // 変換前と変換後の座標。
    Point sigma{1,1};                      // 各軸の既知の標準偏差。
    std::array<bool,2> present{true,true};  // 測れなかった軸はfalse。
    double rho = 0;                        // 両軸の誤差相関。-1<rho<1。
};

// 出力: [倍率c, 横ずれdx, 縦ずれdy, queryの予測横座標, 予測縦座標]。
std::optional<std::array<double,5>> fit_coordinates(
    std::vector<PointRow> rows, Point query, Solver::Budget budget) {
    Solver::Parameters p;
    const auto c = p.real(1, 0.2, 3);
    const auto dx = p.real(0, -5, 5), dy = p.real(0, -5, 5);
    auto predict = [c,dx,dy](const Point& x, const McmcState& s) {
        return Point{s.real[c]*x[0]+s.real[dx], s.real[c]*x[1]+s.real[dy]};
    };
    auto loss = [](const McmcState&, const PointRow& row, const Point& y) {
        if (!row.present[0] && !row.present[1]) return 0.0;
        if (!row.present[0]) return Solver::gaussian_loss(row.value[1],y[1],row.sigma[1]);
        if (!row.present[1]) return Solver::gaussian_loss(row.value[0],y[0],row.sigma[0]);
        assert(row.sigma[0]>0 && row.sigma[1]>0 && std::abs(row.rho)<1);
        const double z0 = (row.value[0]-y[0])/row.sigma[0];
        const double z1 = (row.value[1]-y[1])/row.sigma[1];
        const double v = 1-row.rho*row.rho;
        // 同時に観測できた二軸は、相関を含む一つの損失にする。
        return (z0*z0-2*row.rho*z0*z1+z1*z1)/(2*v)
             + std::log(row.sigma[0])+std::log(row.sigma[1])+0.5*std::log(v);
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,loss));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    const auto& s = *r.state;
    const auto y = fit.model().prediction(query,s); // 独自損失なので生の予測を使う。
    return std::array<double,5>{s.real[c],s.real[dx],s.real[dy],y[0],y[1]};
}

int main() {
    std::vector<PointRow> rows{
        {{0,0},{2,-1},{0.1,0.2},{true,true},0.3},
        {{1,2},{3.5,2},{0.1,0.2},{true,true},0.3},
        {{2,-1},{5,0},{0.1,0.2},{true,false},0.3}, // 縦のvalue=0は未使用。
        {{-1,3},{0,3.5},{0.1,0.2},{false,true},0.3}};
    const auto r = fit_coordinates(rows,{2,2},Solver::Budget::for_steps(40000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

欠測軸の値を無理に0の観測として扱うと、原点へ寄せる別のモデルになります。この例では`present=false`が「その軸の情報を使わない」という指定です。損失を0にしても予測関数そのものは呼ばれるので、入力は予測できる形にしておきます。

### 5.3 U03：大きな外れ値を含む処理時間

個別コード：`examples/mcmc_estimator_guide_example_u03_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=3`。

**具体的な問題。** 普段の処理時間は仕事量にほぼ比例します。しかし、ときどき別の処理を待って極端に時間が延びます。その一回に合わせて通常時の予測まで大きくしたくない状況です。

予測は$f(x;\theta)=ax+b$です。$x$は仕事量、$a$は仕事量あたりの時間、$b$は固定時間です。観測時間を$y$、通常のぶれを$\sigma$、標準化した残差を$z=(y-f(x;\theta))/\sigma$とします。Huber損失は、小さなずれでは二乗、大きなずれでは直線的に増える値を使います。

$$h_\delta(z)=\begin{cases}z^2/2&(|z|\leq\delta)\\ \delta(|z|-\delta/2)&(|z|>\delta)\end{cases},\qquad \ell=h_\delta(z)+\log\sigma$$

$\delta>0$は切り替えの閾値で、コードのthresholdです。例えば$\delta=1.345$なら、通常のぶれの1.345倍を超えるずれの影響を抑え始めます。外れ値を完全に捨てるのではなく、影響の増え方を抑えます。

**モデルからコードへ。** U01と同じ一出力の観測モデルを使い、損失だけ`Huber{threshold}`にします。thresholdは関数の引数で、省略時1.345です。各観測のscaleは、外れ値込みの最大幅ではなく通常時のぶれを設定します。

**呼び出しと結果。** `fit_robust_time`の戻り値は`[a,b]`です。次の仕事量を$x_{\mathrm{next}}$とすれば、戻り値から$a x_{\mathrm{next}}+b$を計算できます。コードの13件中一件だけに待ち時間50を加えており、通常の係数2と1に近い推定を確認できます。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 入力: input=仕事量、value=処理時間、scale=通常時の誤差の大きさ。
// thresholdは「scaleの何倍から外れ値として影響を抑えるか」。正の固定値。
// 出力: [仕事量あたりの時間a, 固定時間b]。
std::optional<std::array<double,2>> fit_robust_time(
    std::vector<Row> rows, Solver::Budget budget, double threshold=1.345) {
    Solver::Parameters p;
    const auto a = p.real(1, 0, 5), b = p.real(0, -10, 10);
    auto predict = [a,b](double x, const McmcState& s) {
        return s.real[a]*x+s.real[b];
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::Huber{threshold}));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return std::array<double,2>{r.state->real[a],r.state->real[b]};
}

int main() {
    std::vector<Row> rows;
    for (int x=0; x<=12; ++x) rows.push_back({double(x),2.0*x+1,0.2});
    rows[6].value += 50; // 一度だけ大きな待ち時間が入った。
    const auto r = fit_robust_time(rows,Solver::Budget::for_steps(40000));
    if (!r) return 1;
    std::cout << (*r)[0] << ' ' << (*r)[1] << '\n';
    return 0;
}
```

この損失は、常に正方向へ出る待ち時間の発生過程を厳密に表すものではありません。通常値を安定して求めるためのモデルです。また、thresholdそのものを未知変数にしたい場合は、分布を正規化するために必要な項も含めた独自損失が必要です。標準Huberのフィールドを未知の値として扱う設計にはなっていません。

### 5.4 U04：成功確率を、複数の候補から予測する

個別コード：`examples/mcmc_estimator_guide_example_u04_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=4`。

**具体的な問題。** 難しい作業ほど失敗しやすいことは分かっていますが、その関係は不明です。難しさ$x$と成功・失敗の記録から、次の難しさに対する成功確率を求めます。

$$t=a-bx,\qquad p(x;\theta)=\frac{1}{1+\exp(-t)},\qquad \theta=(a,b)$$

$a$は基本的な成功しやすさ、$b\geq0$は難しさの影響、$t$はlogit、$p$は0〜1の成功確率です。観測$y$は成功なら1、失敗なら0です。対応する損失は$-y\log p-(1-y)\log(1-p)$です。コードは数値的に安定な計算を行う`BernoulliLogit`を使います。

データが少ないと、たまたま全て成功しただけでも非常に大きな係数へ寄ることがあります。例では事前項$R=a^2/18+b^2/8$を置き、さらに$a,b$に範囲を設けます。これは$a$が中心0・標準偏差3、$b$が中心0・標準偏差2の正規形を指定範囲内で使う想定です。

**モデルからコードへ。** 予測関数は$t$を返します。`mean_prediction(query,s)`は、その候補における$p$へ変換します。標本ごとにこの確率を計算して平均します。平均係数を一度だけ確率へ変換する方法とは、一般に値が違います。

**呼び出しと結果。** `predict_success`は4鎖のSessionを作り、`sample`を直接呼びます。先に`solve`を行う必要はありません。total_stepsのうち先頭2000遷移を準備に使い、残りを集計します。戻り値は成功確率、その確率自体の分散、記録数です。標本がなければ`nullopt`です。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;
struct SuccessEstimate {
    double probability;                     // 次の作業の成功確率の推定。
    std::optional<double> probability_variance; // パラメータの不確かさによる確率の分散。
    std::uint64_t samples;                   // 独立な標本の個数とは限らない。
};

// 入力: input=難しさx、value=成功なら1/失敗なら0。scaleは使用しない。
// total_stepsは準備2000遷移を含む。準備だけで終わればnullopt。
std::optional<SuccessEstimate> predict_success(
    std::vector<Row> rows, double query, std::uint64_t total_steps) {
    Solver::Parameters p;
    const auto a = p.real(0, -8, 8), b = p.real(1, 0, 5);
    auto predict = [a,b](double x, const McmcState& s) {
        return s.real[a]-s.real[b]*x; // 確率そのものではなくlogitを返す。
    };
    auto prior = [a,b](const McmcState& s) {
        return 0.5*s.real[a]*s.real[a]/9 + 0.5*s.real[b]*s.real[b]/4;
    };
    Solver::Param settings;
    settings.warmup_steps = 2000;
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::BernoulliLogit{},prior),
        settings, {}, 4); // 4鎖。初期値は全鎖で共通。
    const auto r = fit.sample(Solver::Budget::for_steps(total_steps),
        [&](const McmcState& s) { return fit.model().mean_prediction(query,s); });
    if (!r.summary.mean()) return std::nullopt;
    return SuccessEstimate{*r.summary.mean(),r.summary.variance(),r.summary.count()};
}

int main() {
    std::vector<Row> rows{{-2,1},{-1,1},{0,0},{0,1},{1,0},{2,0}};
    const auto r = predict_success(rows,0,60000);
    if (!r) return 1;
    std::cout << r->probability << ' ' << r->probability_variance.value_or(0)
              << ' ' << r->samples << '\n';
    return 0;
}
```

`probability_variance`は、未知の係数が変わることによる確率のばらつきです。実際の一回の成否は0または1であり、その成否の分散と同じ値ではありません。独立な標本数も計算していません。例の準備2000遷移は実演用の指定で、一般の収束を保証しません。

### 5.5 U05：観測時間が違う件数

個別コード：`examples/mcmc_estimator_guide_example_u05_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=5`。

**具体的な問題。** サーバーに来た要求数を測っています。負荷条件だけでなく、30分測った記録と3時間測った記録が混在しています。観測時間が長いから件数が多いという違いを、負荷の影響と混同せずに推定します。

$$\lambda(x;\theta)=\exp(a+bx),\qquad \mu=h\lambda(x;\theta),\qquad \log\mu=\log h+a+bx$$

$x$は負荷指標、$h>0$は観測時間、$\lambda$は単位時間あたりの平均発生件数、$\mu$はその観測時間内の平均件数です。未知変数は$\theta=(a,b)$です。件数$y$をPoisson分布と考えると、候補によらない項を除いた損失は$\mu-y\log\mu$です。

**モデルからコードへ。** inputに負荷と時間をまとめます。`PoissonLogMean`が期待する値は$\log\mu$なので、時間の対数を予測関数に足します。標準観測のscaleは使いません。valueは0以上の整数を表すdoubleです。

**呼び出しと結果。** `predict_count`は最良候補を求め、`mean_prediction`で平均件数へ戻します。戻り値は整数とは限らず、丸めずに期待値として使えます。例の負荷1・2時間では8件付近になります。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct Exposure { double load, hours; }; // loadは負荷指標、hours>0は観測時間。
using Row = Solver::Observation<Exposure>;

// 入力: value=観測された非負の整数件数。scaleは使用しない。
// 出力: queryの観測時間内に発生する件数の期待値。整数に丸めない。
std::optional<double> predict_count(
    std::vector<Row> rows, Exposure query, Solver::Budget budget) {
    Solver::Parameters p;
    const auto a = p.real(0, -6, 6), b = p.real(0, -3, 3);
    auto predict = [a,b](const Exposure& x, const McmcState& s) {
        assert(x.hours>0 && x.load>=0 && x.load<=2);
        return std::log(x.hours)+s.real[a]+s.real[b]*x.load; // 件数平均の対数。
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::PoissonLogMean{}));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return fit.model().mean_prediction(query,*r.state); // expで件数の単位に戻す。
}

int main() {
    std::vector<Row> rows{{{0,1},2},{{0,3},6},{{1,1},4},{{1,3},12},{{2,2},16}};
    const auto r = predict_count(rows,{1,2},Solver::Budget::for_steps(30000));
    if (!r) return 1;
    std::cout << *r << '\n';
    return 0;
}
```

例は負荷を0〜2に制限しています。これは指数計算を有限範囲に保ち、係数の意味を限定するためのモデル上の範囲です。実際の入力範囲に合わせて変更してください。件数のばらつきが平均より大幅に大きい場合などには、別の件数分布を独自損失として用意する必要があります。

### 5.6 U06：測定のぶれも未知にする

個別コード：`examples/mcmc_estimator_guide_example_u06_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=6`。

**具体的な問題。** 補正式$y\approx ax+b$だけでなく、測定値がどの程度ぶれるかも分かりません。係数と共通の標準偏差$\sigma$を同時に求めます。

$\sigma$は正でなければならないので、コードでは$\eta=\log\sigma$を未知変数にします。状態は$\theta=(a,b,\eta)$で、$\sigma=\exp\eta$です。一件の損失は次のとおりです。

$$\ell=\frac{(y-ax-b)^2}{2\exp(2\eta)}+\eta$$

$x,y$は入力と観測です。最後の$+\eta$は$+\log\sigma$に当たります。この項を抜くと、標準偏差を大きくするだけで二乗項をいくらでも小さくできるため、測定のぶれを正しく比較できません。

**モデルからコードへ。** 標準Gaussianのscaleは既知の値を置くフィールドなので、今回は独自損失が状態から標準偏差を読みます。例は$0.02\leq\sigma\leq3$、事前項0、つまり**対数尺度$\eta$について範囲内一様**というモデルです。sigmaそのものが一様という意味ではありません。

**呼び出しと結果。** `fit_unknown_noise`へ観測とBudgetを渡します。戻り値の第3要素は、内部の対数値ではなく`exp`で戻した標準偏差です。例では同じ入力に上下0.2の観測を置いているため、標準偏差は0.2付近になります。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 入力: input=x、value=y。全観測で共通の標準偏差も推定するためscaleは使わない。
// この例はeta=log(sigma)に対して範囲内一様の事前分布を置く。
// 出力: [傾きa, 切片b, 標準偏差sigma]。sigmaの下限0.02は測定系に合わせて変える。
std::optional<std::array<double,3>> fit_unknown_noise(
    std::vector<Row> rows, Solver::Budget budget) {
    Solver::Parameters p;
    const auto a = p.real(1, -5, 5), b = p.real(0, -5, 5);
    const auto eta = p.real(std::log(0.5),std::log(0.02),std::log(3.0));
    auto predict = [a,b](double x, const McmcState& s) { return s.real[a]*x+s.real[b]; };
    auto loss = [eta](const McmcState& s, const Row& row, double predicted) {
        assert(row.weight>=0 && std::isfinite(row.weight));
        if (row.weight==0) return 0.0;
        const double z = (row.value-predicted)/std::exp(s.real[eta]);
        return row.weight*(0.5*z*z+s.real[eta]); // +log(sigma)を必ず含める。
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,loss));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return std::array<double,3>{r.state->real[a],r.state->real[b],std::exp(r.state->real[eta])};
}

int main() {
    std::vector<Row> rows;
    for (int x=-3; x<=3; ++x) {
        rows.push_back({double(x),1.5*x+0.5-0.2});
        rows.push_back({double(x),1.5*x+0.5+0.2});
    }
    const auto r = fit_unknown_noise(rows,Solver::Budget::for_steps(60000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

変数を置き換えたうえで同じ確率分布を採取したい場合、密度の変換を考慮します。例えばsigmaについて範囲内一様の事前分布を、etaを状態として表すなら、事前項に$-\eta$を一度加えます。etaが少し動いたときのsigmaの幅が$\exp\eta$倍になるためです。最も密度が高い点は、どの座標で密度を表すかにも依存します。何を一様と考えるかを先に決めてください。

ほぼ完全に当てはまるデータでは、標準偏差が下限へ寄ることがあります。測定系として意味のある下限を設定し、未知数の数に対して十分な観測を用意します。

### 5.7 U07：実数・整数・カテゴリを一緒に調べる

個別コード：`examples/mcmc_estimator_guide_example_u07_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=7`。

**具体的な問題。** 装置の種類には一次応答と二次応答があり、さらに未知の倍率と整数補正が入っています。種類を先に決めず、一つの問題として推定します。

$$f(x;\theta)=g r_k(x)+m,\qquad r_0(x)=x,\quad r_1(x)=x^2$$

$x$は入力、$g$は実数倍率、$m$は整数補正、$k$は種類IDで0または1です。$r_k$は種類に対応する既知の応答式です。未知変数は$\theta=(g,m,k)$です。測定誤差はGaussianとします。

**モデルからコードへ。** `real`、`integer`、`category`をそれぞれ一度使います。integerとcategoryは同じdiscrete配列へ入りますが、変更方法に与える意味は違います。整数補正は近い値が似た候補で、種類IDには数値的な近さがありません。

**呼び出しと結果。** `fit_device`は`DeviceEstimate`を返します。gainが倍率、offsetが整数補正、kindが種類です。kindを読んで、該当する式で新しい入力を予測できます。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;
struct DeviceEstimate { double gain; std::int64_t offset, kind; };

// 入力: input=x、value=装置の表示値、scale=既知の測定誤差。
// kind=0なら一次応答、kind=1なら二次応答。offsetは整数に限られる。
// 出力: 倍率、整数補正、装置種類。種類IDの意味は呼び出し側と共有する。
std::optional<DeviceEstimate> fit_device(std::vector<Row> rows, Solver::Budget budget) {
    Solver::Parameters p;
    const auto gain = p.real(1, 0.1, 4);
    const auto offset = p.integer(0, 0, 5);
    const auto kind = p.category(0, 2); // integerとcategoryはdiscreteの添字を共有。
    auto predict = [gain,offset,kind](double x, const McmcState& s) {
        const double response = s.discrete[kind]==0 ? x : x*x;
        return s.real[gain]*response+static_cast<double>(s.discrete[offset]);
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return DeviceEstimate{r.state->real[gain],r.state->discrete[offset],r.state->discrete[kind]};
}

int main() {
    std::vector<Row> rows;
    for (double x : {0.0,1.0,2.0,3.0,4.0}) rows.push_back({x,1.7*x*x+2,0.1});
    const auto r = fit_device(rows,Solver::Budget::for_steps(80000));
    if (!r) return 1;
    std::cout << r->gain << ' ' << r->offset << ' ' << r->kind << '\n';
    return 0;
}
```

種類IDだけを返されても意味が分からないため、IDと式の対応は利用側で保持します。種類の意味や個数を変えたときはSessionを作り直します。入力が0と1しかないと、一次式と二次式は同じ値を返すため、どれだけ計算してもこの二種類を区別できません。

### 5.8 U08：合計1の混合割合

個別コード：`examples/mcmc_estimator_guide_example_u08_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=8`。

**具体的な問題。** 三種類の染料を混ぜた試料があります。ある測定帯域で、各染料だけを測った応答は既知です。帯域を変えた複数回の測定から、混ぜた割合を推定します。

$$\widehat y_i=r_{i0}w_0+r_{i1}w_1+r_{i2}w_2,\qquad w_0,w_1,w_2\geq0,\quad w_0+w_1+w_2=1$$

$w_0,w_1,w_2$が未知の割合です。$r_{ik}$は第$i$測定の帯域における染料$k$単独の既知の応答、$\widehat y_i$は混合物の予測です。観測とのずれはGaussian損失で比べます。

**モデルからコードへ。** 三つの実数を自由に動かしてから合計で割ると、どの候補がどの確率で作られたかが変わります。ここでは状態を3要素配列にし、一方の割合に足した量を他方から引く変更にします。初期値を合計1にしておけば、合計を保ったまま動けます。負になる提案は、その試行を棄却します。

選ぶ二成分と増減量の正逆の確率が同じなので、`symmetric_move`を使えます。事前項0は、独立に選べる二つの割合の平面上で、許される三角形に一様な密度を置くモデルです。

**呼び出しと結果。** `fit_mixture`は`McmcEstimator<std::array<double,3>>`を使い、`sample`で割合そのものを返すprojectを指定します。戻り値は三割合の標本平均です。各標本の合計が1なので、平均も丸め誤差を除いて合計1になります。

```cpp
#include "mcmc_estimator_v09.hpp"

using Fractions = std::array<double,3>;
using Solver = McmcEstimator<Fractions>;
using Row = Solver::Observation<Fractions>;

// 入力: input[k]=染料kだけの既知の応答、value=混合物の応答、scale=測定誤差。
// 出力: 三種類の割合の標本平均。非負で合計が約1。標本0件ならnullopt。
std::optional<Fractions> fit_mixture(std::vector<Row> rows, std::uint64_t total_steps) {
    auto predict = [](const Fractions& response, const Fractions& w) {
        return response[0]*w[0]+response[1]*w[1]+response[2]*w[2];
    };
    auto move = Solver::symmetric_move([](const Fractions&, Fractions& next,
        Solver::Rng& rng, const Solver::MoveContext&) {
        const int i = rng.integer(3), j = (i+1+rng.integer(2))%3;
        const double change = 0.1*rng.normal(); // 固定幅。Sample中は変更しない。
        next[static_cast<std::size_t>(i)] += change;
        next[static_cast<std::size_t>(j)] -= change; // 一方に足した分だけ他方から引く。
        return next[static_cast<std::size_t>(i)]>=0 && next[static_cast<std::size_t>(j)]>=0;
        // 範囲外はその試行を棄却する。有効になるまで引き直さない。
    });
    Solver::Param settings;
    settings.warmup_steps = 2000;
    auto fit = Solver::make_session({Fractions{1.0/3,1.0/3,1.0/3}},
        Solver::make_observation_model(std::move(rows),predict),std::move(move),settings);
    const auto r = fit.sample(Solver::Budget::for_steps(total_steps),
                             [](const Fractions& w) { return w; });
    return r.summary.mean();
}

int main() {
    std::vector<Row> rows{{{1,0,0},0.2,0.03},{{0,1,0},0.3,0.03},{{0,0,1},0.5,0.03}};
    const auto r = fit_mixture(rows,60000);
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

無効な提案が出るたびに、同じ試行の中で有効になるまで引き直すと、境界付近の提案確率が変わります。例の`false`は、その場所にとどまった一回の遷移として扱わせるために必要です。大きな状態や別の制約でも、構造を保つ変更方法を作れるかを考えると、無効候補を減らせます。

### 5.9 U09：地図の誤登録を訂正して継続する

個別コード：`examples/mcmc_estimator_guide_example_u09_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=9`。

**具体的な問題。** 経路の所要時間から、道路種別ごとの距離あたり時間を調べます。推定を進めた後、ある辺の登録距離が間違っていたと分かりました。これまで得た候補を捨てず、正しい距離で再評価します。

$$\widehat t_i=\sum_{e\in P_i}L_e v_{c_e}$$

$P_i$は第$i$観測で通った辺の列、$L_e$は辺$e$の既知の距離、$c_e$は道路種別、$v_c$は種別$c$の未知の距離あたり時間です。$\widehat t_i$が予測所要時間で、実測時間とのGaussian損失を使います。同じ辺を二度通れば、列に現れた回数だけ足します。

**モデルからコードへ。** 変数は道路種別の個数だけ用意します。辺ID・距離・種類の表をContext、通った辺IDの列をinputにします。この分離により、各観測に辺表を複製せずに済みます。

**呼び出しと結果。** `fit_corrected_map`は、最初に`solve_view`で探索し、`update_model`の中でContextを訂正します。その後の`solve`が、古い最良候補と各鎖を訂正後の目的関数で評価してから続行します。戻り値は種類別の距離あたり時間です。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct Edge { std::size_t road_class; double length; };
using Map = std::vector<Edge>; // 辺IDはこのvectorの添字。
using Route = std::vector<std::size_t>;
using Row = Solver::Observation<Route>;

// 入力: 道路種別数classes、辺表、routeごとの所要時間、訂正した辺表。
// 訂正は過去の測定にも適用する。各辺ID・道路種別IDの意味は共通。
// 出力: 道路種別ごとの「距離1あたりの時間」。
std::optional<std::vector<double>> fit_corrected_map(std::size_t classes,
    Map map, std::vector<Row> rows, Map corrected, std::uint64_t steps_per_stage) {
    assert(classes>0 && corrected.size()==map.size());
    Solver::Parameters p;
    for (std::size_t i=0; i<classes; ++i) p.real(1,0.1,10);
    auto predict = [](const Map& current, const Route& route, const McmcState& s) {
        double time = 0;
        for (auto id : route) {
            assert(id<current.size());
            const auto& edge = current[id];
            assert(edge.road_class<s.real.size() && edge.length>0);
            time += edge.length*s.real[edge.road_class];
        }
        return time;
    };
    Solver::Param settings;
    settings.cooling_steps = steps_per_stage;
    auto fit = Solver::make_numeric_session(p,
        Solver::make_context_model(std::move(rows),std::move(map),predict),settings);
    fit.solve_view(Solver::Budget::for_steps(steps_per_stage)); // 候補のコピーを省く。
    fit.update_model([&](auto& model) { model.context() = std::move(corrected); });
    // 以前の候補を保持したまま、訂正後の地図で再評価して探索する。
    const auto r = fit.solve(Solver::Budget::for_steps(steps_per_stage));
    if (!r.state) return std::nullopt;
    return r.state->real;
}

int main() {
    Map map{{0,1},{1,1},{0,2}}, corrected{{0,1},{1,2},{0,2}};
    std::vector<Row> rows{{{0},2,0.1},{{1},8,0.1},{{0,2},6,0.1}};
    const auto r = fit_corrected_map(2,map,rows,corrected,30000);
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

ここでは「距離の登録が間違っていた」ため、訂正を全履歴に適用しています。「途中から工事で道が変わった」場合は、当時の状況を各観測に残す必要があります。また、辺表の編集は状態の変数数を増やす操作ではありません。新しい道路種別の未知変数を増やす場合は再構築します。

### 5.10 U10：シミュレータから機械速度を推定する

個別コード：`examples/mcmc_estimator_guide_example_u10_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=10`。

**具体的な問題。** 二台の機械へ順番に仕事を投入します。次の仕事は、その時点で終了予定時刻が早い機械へ割り当てます。同時なら機械0です。各仕事の量と全体終了時刻だけが分かり、機械速度は不明です。

未知の速度を$v_0,v_1>0$とします。機械$j$へ量$q$の仕事を割り当てたとき、終了予定時刻に$q/v_j$を加えます。全て割り当てた後の二台の終了予定時刻を$F_0,F_1$とすると、観測に対応する予測は$\max(F_0,F_1)$です。$j$は機械ID、$q$は既知の仕事量、$F_j$はシミュレーション中に計算する値で、追加の未知パラメータではありません。

**モデルからコードへ。** inputは仕事量の列、状態は二つの速度です。予測関数が終了時刻を計算します。割当先を決める分岐があるため単純な式でなくても、候補から再現可能な値を計算できれば同じ観測モデルに載せられます。

**呼び出しと結果。** `fit_machines`は観測した仕事列と、新しく予測したい仕事列queryを受け取ります。戻り値には速度候補とqueryの終了時刻が入ります。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Jobs = std::vector<double>;
using Row = Solver::Observation<Jobs>;
struct MachineEstimate { std::array<double,2> speed; double query_finish; };

// 入力: input=既知の順番で投入する仕事量の列、value=全仕事の終了時刻。
// 各仕事は「今の終了予定時刻が早い機械」に割り当てる。同時なら機械0。
// queryも同じ割当規則で動かす仕事列。出力は速度候補とqueryの終了時刻。
std::optional<MachineEstimate> fit_machines(
    std::vector<Row> rows, Jobs query, Solver::Budget budget) {
    Solver::Parameters p;
    p.real(1,0.2,5); p.real(1,0.2,5); // 速度は正。添字0/1が機械ID。
    auto simulate = [](const Jobs& jobs, const McmcState& s) {
        std::array<double,2> finish{0,0};
        for (double work : jobs) {
            assert(work>=0);
            const std::size_t machine = finish[0]<=finish[1] ? 0 : 1;
            finish[machine] += work/s.real[machine];
        }
        return std::max(finish[0],finish[1]);
    };
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),simulate));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return MachineEstimate{{r.state->real[0],r.state->real[1]},
                            fit.model().prediction(query,*r.state)};
}

int main() {
    // 速度[2,1]で生成された測定。一本の仕事だけの行もあり、機械0を区別できる。
    std::vector<Row> rows{{{2},1,0.05},{{2,2},2,0.05},
                          {{4,1,2},3,0.05},{{1,4,1},4,0.05}};
    const auto r = fit_machines(rows,{2,2,2},Solver::Budget::for_steps(80000));
    if (!r) return 1;
    std::cout << r->speed[0] << ' ' << r->speed[1] << ' ' << r->query_finish << '\n';
    return 0;
}
```

割当規則や同時刻の扱いが実機と異なると、その違いを速度の違いとして説明してしまいます。規則を先に一致させてください。また、同じ全体終了時刻を生む速度の組が複数あることがあります。例では一本だけの仕事を含め、機械0の速度を区別する情報も与えています。

### 5.11 U11：誤った比較を含む順序推定

個別コード：`examples/mcmc_estimator_guide_example_u11_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=11`。

**具体的な問題。** 要素には隠れた順番があります。二つを比べると「AがBより前」という答えが得られますが、一定確率で逆の答えが返ります。全てを硬い制約にすると矛盾するため、誤りを許して一番説明しやすい順列を求めます。

比較が逆になる確率を$\varepsilon$とし、$0<\varepsilon<0.5$とします。候補順列が観測の前後関係と一致すれば一件の損失は$-\log(1-\varepsilon)$、違えば$-\log\varepsilon$です。$\varepsilon$は既知の入力で、候補順列だけが未知です。各比較の誤りは独立と考えます。

**モデルからコードへ。** 状態は「各位置にある要素ID」のvectorです。目的関数は各IDの位置を求め、全比較の損失を足します。観測モデルに分けることもできますが、位置表を一度で作れる全体評価にしています。提案は二つの位置の交換なので、順列という制約が自然に保たれます。

**呼び出しと結果。** `fit_order`へ要素数、比較、誤り率、遷移数を渡します。`make_session`に独自状態・全体評価・対称交換を渡し、`solve`から最良順列のコピーを取り出します。

```cpp
#include "mcmc_estimator_v09.hpp"

using Order = std::vector<int>; // 位置から要素IDを引く。各IDが一度ずつ現れる。
using Solver = McmcEstimator<Order>;
struct Comparison { int before, after; }; // 「beforeがafterより前」と観測した。

// 入力: 要素数n、誤りを含み得る比較列、各比較が逆になる既知の確率error。
// 出力: 最もよく観測を説明した順列。n>0、0<error<0.5、IDは[0,n)。
std::optional<Order> fit_order(int n, std::vector<Comparison> comparisons,
                             double error, std::uint64_t steps) {
    assert(n>0 && error>0 && error<0.5);
    Order initial(static_cast<std::size_t>(n));
    std::iota(initial.begin(),initial.end(),0);
    auto energy = [data=std::move(comparisons),error](const Order& order) {
        std::vector<int> position(order.size());
        for (std::size_t i=0; i<order.size(); ++i)
            position[static_cast<std::size_t>(order[i])] = static_cast<int>(i);
        double total = 0;
        for (const auto& row : data) {
            assert(row.before>=0 && row.after>=0 && row.before!=row.after);
            assert(static_cast<std::size_t>(row.before)<order.size() &&
                   static_cast<std::size_t>(row.after)<order.size());
            const bool matches = position[static_cast<std::size_t>(row.before)]
                               < position[static_cast<std::size_t>(row.after)];
            total -= std::log(matches ? 1-error : error);
        }
        return total; // 観測全体の負の対数尤度。
    };
    auto move = Solver::symmetric_move([](const Order&, Order& next,
        Solver::Rng& rng, const Solver::MoveContext&) {
        const auto i = static_cast<std::size_t>(rng.integer(static_cast<int>(next.size())));
        const auto j = static_cast<std::size_t>(rng.integer(static_cast<int>(next.size())));
        std::swap(next[i],next[j]); // 逆向きも同じ確率で選べる。
        return i!=j;              // 同じ位置なら評価を省いて棄却扱い。
    });
    Solver::Param settings;
    settings.cooling_steps = steps;
    auto fit = Solver::make_session({initial},std::move(energy),std::move(move),settings);
    return fit.solve(Solver::Budget::for_steps(steps)).state;
}

int main() {
    // 比較の多数は2→0→1を支持する。0→2という誤った比較が一つある。
    std::vector<Comparison> rows{{2,0},{2,0},{2,0},{0,1},{0,1},{2,1},{0,2}};
    const auto r = fit_order(3,rows,0.1,10000);
    if (!r) return 1;
    for (int x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

各IDが必ず一度だけ現れる初期状態を使います。単純に数値平均した順列は有効な順列になるとは限らないため、標本採取を使う場合でも、要素が前にある確率などの意味のある量を集計します。標本を採る変更方法では、棄却された状態を省かず数えることも必要です。

### 5.12 U12：時間的に相関した誤差を含む補正

個別コード：`examples/mcmc_estimator_guide_example_u12_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=12`。

**具体的な問題。** センサーの誤差は毎回独立ではなく、一度高めにずれると次の測定も高めになりやすいとします。補正線の係数を、誤差の持続性も考慮して求めます。

$i$番目の入力・観測を$x_i,y_i$、未知係数を$a,b$とし、残差を$r_i=y_i-(ax_i+b)$と書きます。誤差のモデルは次です。

$$r_i=\rho r_{i-1}+\epsilon_i,\qquad \epsilon_i\sim N(0,\sigma^2)\quad(i\geq2)$$

$\rho$は既知の持続率で$|\rho|<1$、$\epsilon_i$はその時刻で新しく加わる独立な誤差、$\sigma>0$はその標準偏差です。$N(0,\sigma^2)$は平均0・分散$\sigma^2$の正規分布を表します。最初の残差は、同じ仕組みが続いてきた状態を仮定し、標準偏差$\sigma/\sqrt{1-\rho^2}$で評価します。

**モデルからコードへ。** 二件目からは、残差そのものではなく$r_i-\rho r_{i-1}$をGaussian損失で評価します。現在の候補で直前の残差を計算し直す必要があるため、時間順の全行を受け持つ全体評価関数にします。予測関数が前回の呼び出し結果を記憶する設計にはしません。

**呼び出しと結果。** `fit_correlated_readings`の戻り値は係数と、次時刻の表示の予測です。次の入力を$x_{\mathrm{next}}$、最後の残差を$r_{\mathrm{last}}$とすれば、予測は$a x_{\mathrm{next}}+b+\rho r_{\mathrm{last}}$です。潜在的な直線の値だけを知りたい場合は、最後の項を加えません。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct Reading { double x, y; }; // 時間順。隣接行は同じ長さの時間間隔。

// 入力: 時間順の観測、誤差の持続率rho、毎時刻に新たに加わる誤差のsigma。
// -1<rho<1、sigma>0。queryは最後の観測の「次の時刻」の入力。
// 出力: [傾きa, 切片b, 次の表示値の条件付き予測]。
std::optional<std::array<double,3>> fit_correlated_readings(std::vector<Reading> rows,
    double rho, double sigma, double query, Solver::Budget budget) {
    assert(!rows.empty() && std::abs(rho)<1 && sigma>0);
    Solver::Parameters p;
    const auto a = p.real(1,-5,5), b = p.real(0,-5,5);
    auto energy = [data=rows,rho,sigma,a,b](const McmcState& s) {
        double total = 0, previous = 0;
        for (std::size_t i=0; i<data.size(); ++i) {
            const double residual = data[i].y-(s.real[a]*data[i].x+s.real[b]);
            // 最初は定常分散、その後は前の残差を差し引いた「新しい誤差」を評価。
            const double scale = i==0 ? sigma/std::sqrt(1-rho*rho) : sigma;
            const double innovation = i==0 ? residual : residual-rho*previous;
            total += Solver::gaussian_loss(innovation,0,scale);
            previous = residual;
        }
        return total;
    };
    auto fit = Solver::make_numeric_session(p,std::move(energy));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    const double slope = r.state->real[a], intercept = r.state->real[b];
    const double last_residual = rows.back().y-(slope*rows.back().x+intercept);
    return std::array<double,3>{slope,intercept,slope*query+intercept+rho*last_residual};
}

int main() {
    std::vector<Reading> rows;
    for (int i=0; i<12; ++i) rows.push_back({double(i%4),2.0*(i%4)+1});
    const auto r = fit_correlated_readings(rows,0.6,0.2,4,Solver::Budget::for_steps(50000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

行の順序を変えると別のモデルになります。時間間隔が不均一な場合も、同じ$\rho$をそのまま各行間へ当てはめるとは限りません。例では$\rho$と$\sigma$は既知です。それらも推定したい場合は、初回分散を含めた正規化項と許容範囲を適切に含めます。

### 5.13 U13：ターンごとに観測を追加・訂正する

個別コード：`examples/mcmc_estimator_guide_example_u13_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=13`。

**具体的な問題。** 仕事量に対してどれだけ時間が掛かるかを、作業を実行しながら学びます。各ターンで新しい観測が得られ、その時点の係数を使って次の仕事を計画します。

モデルは処理時間$ax+b$です。$x$はその仕事の量、$a,b$はターン間で共通の未知係数です。目的関数には、その時点で保持している観測を全て入れます。予測式は単純ですが、ここでの主題は推定器を使い捨てずに続けることです。

**モデルからコードへ。** `make_turn_estimator`は空の観測モデルを持つSessionを一度だけ作ります。`estimate_turn`は新着の行だけを追加し、そのターンの予算で`solve`します。観測の全履歴を毎回addへ渡すと、同じ行を重複して数えてしまうので注意します。

**呼び出しと結果。** 各ターンの戻り値は通常の`Result`です。stateがあれば`real[0]`と`real[1]`が係数です。訂正したい場合は`replace_observations`へ残す全行を渡し、古い行を忘れたい場合は`retain_last`を使います。例では7.5と記録した値を7.0へ訂正し、最後に3行を保持しています。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

// 推定器を一度だけ作る。隠れた係数a,bはターン間で共通。
auto make_turn_estimator() {
    Solver::Parameters p;
    const auto a = p.real(1,0,5), b = p.real(0,-5,5);
    auto predict = [a,b](double x, const McmcState& s) { return s.real[a]*x+s.real[b]; };
    return Solver::make_numeric_session(p,
        Solver::make_observation_model(std::vector<Row>{},predict));
}

// 入力: 同じfit、新しく届いた観測だけ、今回の予算。
// 出力: Session::Result。state->realは[a,b]、revisionは目的関数の版番号。
template<class Fit>
auto estimate_turn(Fit& fit, std::vector<Row> newly_observed, Solver::Budget budget) {
    fit.add_observations(std::move(newly_observed)); // 登録だけ行い、まだ再評価しない。
    return fit.solve(budget);                     // 再評価と探索に同じ予算を使う。
}

int main() {
    auto fit = make_turn_estimator(); // 実際の対話問題でもターンループの外に置く。
    const auto first = estimate_turn(fit,{{0,1,0.1},{1,3,0.1}},Solver::Budget::for_steps(20000));
    if (!first.state) return 1;
    // 最後の観測には転記誤りがあり、後から7.0へ訂正するとする。
    const auto second = estimate_turn(fit,{{2,5,0.1},{3,7.5,0.1}},Solver::Budget::for_steps(20000));
    if (!second.state) return 1;

    // 観測を訂正する場合は「訂正後に残す全観測」を渡す。差分だけではない。
    fit.replace_observations({{0,1,0.1},{1,3,0.1},{2,5,0.1},{3,7,0.1}});
    fit.retain_last(3); // 古い一件を除く。実際に忘却が必要なときだけ使う。
    const auto final = fit.solve(Solver::Budget::for_steps(20000));
    if (!final.state) return 1;
    std::cout << final.state->real[0] << ' ' << final.state->real[1]
              << ' ' << fit.model().size() << ' ' << final.revision << '\n';
    // 時間不足ならstateが空か確認する。空でなくても収束したとは限らない。
    // sampleの外部集計を持つ場合は、revisionが変わった時点で破棄する。
    return 0;
}
```

モデル変更時は、候補の値と学習した数値変更幅を引き継ぎます。古いenergyをそのまま使うのではなく、必要な再評価を行います。更新直後に予算がなければ結果が空になる場合があります。そのときの行動用に古い推定を保存しておくことはできますが、それが新しいモデルで評価済みの値とは限りません。

隠れた係数自体が徐々に変わる場合、直近だけを残す方法は一つの近似です。係数の時間変化を表す確率モデルが自動で付くわけではありません。過去の観測時に使った入力・環境も、現在の値へ置き換えず記録します。

### 5.14 U14：依存する測定だけを再計算する

個別コード：`examples/mcmc_estimator_guide_example_u14_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=14`。

**具体的な問題。** 多数の端子について、二端子間の電圧差だけを測れます。端子0は既知の0Vです。他の端子電圧を推定しますが、一つの端子を動かしただけなら、その端子と無関係な測定を計算し直したくありません。

$$E(v)=\sum_{k=1}^{M}\left\{\frac{(d_k-(v_{a_k}-v_{b_k}))^2}{2\sigma_k^2}+\log\sigma_k\right\},\qquad v_0=0$$

$v_j$は端子$j$の電圧です。$M$は測定数、$a_k,b_k$は第$k$測定の二端子ID、$d_k$は観測した電圧差、$\sigma_k$は既知の測定誤差です。$v_0$を固定しないと、全電圧に同じ定数を足しても電圧差が変わらず、基準を決められません。

**モデルからコードへ。** 独自Modelに全体評価の`operator()`と、候補用の`candidate(old,next,old_energy)`を用意します。後者が存在すると、探索中の候補評価に使われます。端子ごとに関連する測定番号を準備し、一変数だけ変わった候補では関連測定の新旧差を古いenergyへ加えます。

**呼び出しと結果。** `fit_voltages`は端子数と測定を受け取り、`solve`の最良候補から電圧vectorを返します。固定端子も含め、添字が端子IDです。複数の変数が同時に変わる提案には全体評価へ戻るため、将来Sampleで集団提案を使っても、差分式の前提を破りません。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
struct VoltageRow { std::size_t a, b; double difference, sigma; };

// 入力: 端子数n、端子aの電圧-端子bの電圧の測定。a!=b、sigma>0。
// 端子0は既知の0V。測定で全端子が端子0につながっていることを想定。
// 出力: 全端子の電圧。未知端子の範囲[-5,5]は対象に合わせて設定する。
std::optional<std::vector<double>> fit_voltages(std::size_t n,
    std::vector<VoltageRow> rows, Solver::Budget budget) {
    assert(n>0);
    struct Model {
        std::vector<VoltageRow> rows;
        std::vector<std::vector<std::size_t>> incident;
        double term(std::size_t k, const McmcState& s) const {
            const auto& row = rows[k];
            return Solver::gaussian_loss(row.difference,s.real[row.a]-s.real[row.b],row.sigma);
        }
        double operator()(const McmcState& s) const {
            double e = 0;
            for (std::size_t k=0; k<rows.size(); ++k) e += term(k,s);
            return e; // 初期評価・モデル更新には常に全評価が必要。
        }
        double candidate(const McmcState& old, const McmcState& next, double old_energy) const {
            std::optional<std::size_t> changed;
            for (std::size_t i=0; i<next.real.size(); ++i) if (old.real[i]!=next.real[i]) {
                if (changed) return (*this)(next); // 複数変数の提案にも正しく対応する。
                changed = i;
            }
            double energy = old_energy;
            if (changed) for (auto k : incident[*changed]) energy += term(k,next)-term(k,old);
            return energy; // 「増分」ではなく、候補の目的関数全体を返す。
        }
    };
    Model model{std::move(rows),std::vector<std::vector<std::size_t>>(n)};
    for (std::size_t k=0; k<model.rows.size(); ++k) {
        const auto& row = model.rows[k];
        assert(row.a<n && row.b<n && row.a!=row.b && row.sigma>0);
        model.incident[row.a].push_back(k); model.incident[row.b].push_back(k);
    }
    Solver::Parameters p;
    p.real(0,0,0); // 固定変数もrealに保持される。提案では動かない。
    for (std::size_t i=1; i<n; ++i) p.real(0,-5,5);
    auto fit = Solver::make_numeric_session(p,std::move(model));
    const auto r = fit.solve(budget);
    if (!r.state) return std::nullopt;
    return r.state->real;
}

int main() {
    std::vector<VoltageRow> rows{{1,0,1,0.1},{2,1,1,0.1},{3,2,1,0.1},{3,0,3,0.1}};
    const auto r = fit_voltages(4,rows,Solver::Budget::for_steps(60000));
    if (!r) return 1;
    for (double x : *r) std::cout << x << ' ';
    std::cout << '\n';
    return 0;
}
```

`candidate`の戻り値は、**候補の全energy**です。増分だけではありません。全体評価と同じ値になることを、自分のモデルで確認してください。候補が棄却される場合もあるので、この関数で共有状態を「採用済み」として変更してはいけません。

この例の評価量は、一変数の変更では「変更箇所を調べるための全変数走査＋その端子に関連する測定数」です。状態コピーもあります。常に定数時間になるわけではありません。また、差分の加算による丸め誤差は蓄積し得ます。自動の定期全評価はないため、値のスケールと全評価との誤差を確認して使います。

### 5.15 U15：予測の不確かさも使って設定値を選ぶ

個別コード：`examples/mcmc_estimator_guide_example_u15_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=15`。

**具体的な問題。** 設定値を調整するとコストが変わりますが、少数の測定しかなく、最適な設定値は不確かです。用意された候補の中から、平均コストが小さく、必要なら不確かさも小さい設定を選びます。

予測モデルを$f(x;\theta)=a(x-b)^2$とします。$x$は設定値、$a>0$は曲がり方、$b$はコストが最小になる位置で、$\theta=(a,b)$が未知です。測定コストには既知のGaussian誤差があるとします。

選べる設定値を$x_0,\ldots,x_{K-1}$とし、候補$\theta$ごとに各設定の予測コストを計算します。$K$は設定候補数です。標本について求めた設定$j$の予測コスト平均を$m_j$、標準偏差を$s_j$とすると、選択基準は次です。

$$j^*=\operatorname*{arg\,min}_{0\leq j<K}(m_j+\lambda s_j)$$

$\lambda\geq0$は利用者が指定する不確かさへの重み、$j^*$は選んだ設定の添字です。$\lambda=0$なら平均だけで選び、大きいほど予測が不確かな候補を避けます。この基準はライブラリが決めるのではなく、行動選択として利用側で定めます。

**モデルからコードへ。** `sample_each`で標本を受け取り、各actionの予測コストをvectorにします。`Session::Moments<std::vector<double>>`は、各要素の平均と分散を蓄積します。計算を5000遷移ずつに分けても、同じ目的関数とactionsなら同じ集計へ足せます。

**呼び出しと結果。** `choose_setting`へ観測、候補設定値、risk、総遷移数を渡します。riskが$\lambda$です。戻り値のindexは入力actionsの添字、settingは実際に使う値です。最初の4000遷移を準備へ使い、標本が2件未満なら`nullopt`を返します。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;
struct Decision {
    std::size_t index;       // actionsの添字。
    double setting;          // 実際に採用する設定値。
    double mean_cost;        // この設定値での、未知パラメータに関する平均予測コスト。
    double model_cost_sd;    // 未知パラメータに由来する予測コストの標準偏差。
    std::uint64_t samples;
};

// 入力: input=試した設定、value=測ったコスト、scale=既知の測定誤差。
// actionsは選べる設定値、risk>=0は不確かさをどれだけ嫌うか。0なら平均だけで選ぶ。
// 出力: 平均コスト+risk*標準偏差が最小の設定。標本2件未満ならnullopt。
std::optional<Decision> choose_setting(std::vector<Row> rows, std::vector<double> actions,
                                      double risk, std::uint64_t total_steps) {
    assert(!actions.empty() && std::isfinite(risk) && risk>=0);
    Solver::Parameters p;
    const auto a = p.real(1,0.1,4), b = p.real(0,-5,5);
    auto predict = [a,b](double x, const McmcState& s) {
        const double d = x-s.real[b]; return s.real[a]*d*d;
    };
    Solver::Param settings;
    settings.warmup_steps = 4000;
    auto fit = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict),settings,{},4);
    using Fit = decltype(fit);
    Fit::Moments<std::vector<double>> moments; // この関数内では同じ目的関数・行動列を使う。
    const auto revision = fit.revision();
    while (total_steps>0) {
        const auto chunk = std::min<std::uint64_t>(total_steps,5000);
        fit.sample_each(Solver::Budget::for_steps(chunk),[&](const McmcState& s,double,int) {
            std::vector<double> costs;
            costs.reserve(actions.size());
            for (double action : actions) costs.push_back(fit.model().prediction(action,s));
            moments.add(costs); // 棄却された遷移の現状態も集計される。
        });
        if (fit.revision()!=revision) return std::nullopt; // 異なる目的関数の集計を混ぜない。
        total_steps -= chunk;
    }
    const auto variance = moments.variance();
    if (!variance) return std::nullopt;
    const auto& mean = *moments.mean();
    std::size_t best = 0;
    auto score = [&](std::size_t j) { return mean[j]+risk*std::sqrt(std::max(0.0,(*variance)[j])); };
    for (std::size_t j=1; j<actions.size(); ++j) if (score(j)<score(best)) best=j;
    return Decision{best,actions[best],mean[best],std::sqrt(std::max(0.0,(*variance)[best])),moments.count()};
}

int main() {
    std::vector<Row> rows{{-1,4,0.5},{0,1,0.5},{2,1,0.5}};
    const auto r = choose_setting(rows,{-1,0,1,2},0.5,60000);
    if (!r) return 1;
    std::cout << r->index << ' ' << r->setting << ' ' << r->mean_cost << ' '
              << r->model_cost_sd << ' ' << r->samples << '\n';
    return 0;
}
```

ここで集計しているのは、未知パラメータによる**平均予測コストのばらつき**です。次の実測に加わる測定誤差までは含めていません。`平均+risk×標準偏差`は、一般に特定の分位点や確率保証にはなりません。実測コストの分位点を使いたいなら、観測誤差も含めた予測分布と、その分位点を計算する処理が必要です。

目的関数を更新したとき、またはactionsの値・順番を変えたときは、同じ集計に足してはいけません。revisionが同じでも、問い合わせ条件を変えれば別の集計です。全標本を保存する必要がなければ、この例のように平均と分散だけを持つと記憶量を抑えられます。

### 5.16 U16：三つの入力から、一次関数の四係数を推定する

個別コード：`examples/mcmc_estimator_guide_example_u16_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=16`。

**具体的な問題。** 三つのつまみを持つ装置があり、それぞれの設定値を$x_1,x_2,x_3$にすると、一つの表示値$y$が返るとします。設定値は基準位置からの差で、正にも負にもできます。三つの設定は表示に足し算で作用しますが、それぞれの効き方と、基準位置での表示は分かりません。異なる設定で測った記録から、この四つの値を調べます。

**観測からモデルへ。** 一つの入力に対する予測式は次です。

$$f(x_1,x_2,x_3;\theta)=a_1x_1+a_2x_2+a_3x_3+b,\qquad \theta=(a_1,a_2,a_3,b)$$

| 記号 | この問題での意味 |
|---|---|
| $x_1,x_2,x_3$ | 利用者が指定した三つの既知の設定値 |
| $a_1,a_2,a_3$ | 各設定の未知の効き方。他の設定を固定して$x_j$を1増やすと、予測値が$a_j$増える。$j$は1、2、3のいずれか |
| $b$ | 三つの設定が全て0のときの未知の基準表示 |
| $\theta$ | 四つの未知係数をまとめた候補。全観測に共通する |
| $f$ | 候補係数を使って表示を再現するユーザー定義関数 |

例えば、誤差がなく、真の係数が$a_1=2,a_2=-3,a_3=0.5,b=1$なら、次のような記録が得られます。

| $x_1$ | $x_2$ | $x_3$ | 観測値$y$ | 読み取れること |
|---|---|---|---|---|
| 0 | 0 | 0 | 1 | 基準表示は1 |
| 1 | 0 | 0 | 3 | 第一の設定だけを1増やすと表示が2増える |
| 0 | 1 | 0 | -2 | 第二の設定だけを1増やすと表示が3減る |
| 0 | 0 | 1 | 1.5 | 第三の設定だけを1増やすと表示が0.5増える |

実測では誤差を含むため、一組の差だけで決める代わりに、全観測で予測と実測がよく合う係数を求めます。観測番号を$i$、観測数を$N$、その行の三入力を$x_{i1},x_{i2},x_{i3}$、実測値を$y_i$とすると、以下のコードの目的関数は次になります。

$$E(a_1,a_2,a_3,b)=\frac{1}{2}\sum_{i=1}^{N}(y_i-a_1x_{i1}-a_2x_{i2}-a_3x_{i3}-b)^2$$

$E$は候補の悪さです。全行の予測誤差を二乗して足します。係数はそれぞれ$-10$以上10以下で、事前項は0です。`scale=1, weight=1`のGaussian損失なので、第1章の式はこの形になります。観測ごとの既知の標準偏差を使う場合は`scale`に正の値を指定すると、第1章の誤差尺度付きの式になります。誤差のない動作確認データでも、`scale=0`にはしません。

**モデルからコードへ。** 三入力は既知のデータなので`std::array<double,3>`にまとめ、`Observation`の`input`へ入れます。出力は一つの実数なので、独自損失は必要ありません。未知変数として宣言するのは四係数だけです。

| 指定・結果 | コードとの対応 |
|---|---|
| 一件の観測 | `AffineRow{{x1,x2,x3}, y}`。標準偏差も指定するなら`AffineRow{{x1,x2,x3}, y, sigma}` |
| 三つの効き方 | `p.real(0,-10,10)`を3回呼ぶ。各戻り値を配列`ai`にまとめる。要素は係数の値ではなく、`state.real`内の添字 |
| 基準表示 | 4回目の`p.real`が返す添字`bi`に対応 |
| 観測の比較 | `make_observation_model(rows,predict)`。既定のGaussian損失と事前項0を使用 |
| 探索 | `make_numeric_session`で作ったSessionに`solve(budget)`を呼ぶ |
| 返される係数 | `r->a[0], r->a[1], r->a[2], r->b`が、それぞれ$a_1,a_2,a_3,b$ |

**呼び出しと結果。** `fit_affine3(rows,budget)`へ空でない観測列を渡します。`std::optional<AffineEstimate>`が空なら、有限な候補を得られていません。初回の予算が0の場合などが該当します。値があれば、その係数を同じ一次式へ代入して新しい三入力の出力を予測できます。例えば上の係数で入力が全て1なら、予測は0.5です。戻るのは探索中の最良候補であり、真値との一致や誤差の上限を保証するものではありません。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;
// 1行 = { {x1, x2, x3}, 観測値y, 誤差の標準偏差scale, 重みweight }。
// scaleとweightは省略すると1。二乗誤差を最小化するだけなら、この既定値でよい。
using AffineRow = Solver::Observation<AffineInput>;
struct AffineEstimate {
    std::array<double, 3> a; // a[0]=a1, a[1]=a2, a[2]=a3
    double b;
};

// 入力: 同じ未知係数で得た複数の観測、計算予算。
// 出力: 探索中に見つけた最良の係数。評価が完了しなければnullopt。
std::optional<AffineEstimate> fit_affine3(
    std::vector<AffineRow> rows, Solver::Budget budget) {
    assert(!rows.empty());
    Solver::Parameters p;
    // real(初期値, 下限, 上限)。範囲は問題の事前知識に合わせて変更する。
    const std::array<std::size_t, 3> ai{
        p.real(0, -10, 10), p.real(0, -10, 10), p.real(0, -10, 10)};
    const auto bi = p.real(0, -10, 10);

    // ライブラリが候補係数sを渡すので、そのときの計算結果を返す。
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.real[ai[0]] * x[0] + s.real[ai[1]] * x[1]
             + s.real[ai[2]] * x[2] + s.real[bi];
    };
    // 既定のGaussian損失で、予測値と各行の観測値を比較する。
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;
    const auto& s = *result.state;
    return AffineEstimate{{s.real[ai[0]], s.real[ai[1]], s.real[ai[2]]}, s.real[bi]};
}

int main() {
    // 動作確認用データ: y = 2*x1 - 3*x2 + 0.5*x3 + 1。
    // 実際には、手元の入力と観測値からこのvectorを作る。
    // 各入力を独立に変える。例えば常にx1=x2だとa1とa2を区別できない。
    std::vector<AffineRow> rows;
    for (double x1 : {-1.0, 0.0, 1.0})
        for (double x2 : {-1.0, 0.0, 1.0})
            for (double x3 : {-1.0, 0.0, 1.0})
                rows.push_back({{x1, x2, x3}, 2*x1 - 3*x2 + 0.5*x3 + 1});

    // 再現しやすいよう固定回数で実行。時間指定ならBudget::for_us(20'000)。
    const auto r = fit_affine3(std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;
    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << '\n';
    return 0;
}
```

`main`では、三入力をそれぞれ-1、0、1に変えた27件を作ります。推定結果は$a_1=2,a_2=-3,a_3=0.5,b=1$付近になります。実際の利用では、真の係数を使うデータ生成部分を手元の観測に置き換えます。推定関数へ真の係数を渡す必要はありません。

**入力の選び方と注意。** 4件あれば必ず四係数が決まるわけではありません。例えば全行で$x_1=x_2$なら、観測で分かるのは$a_1+a_2$だけです。また$x_3$がいつも同じなら、その寄与と$b$を分離できない場合があります。三入力を独立に変え、基準表示との違いも観測できる記録を用意します。値の桁が大きく違う場合は入力を既知の基準で換算し、推定係数の単位もその換算に合わせます。

この関数形サンプルは呼ぶたびにSessionを作ります。同じ四係数について毎ターン記録が増える場合は、U13のようにSessionを保持し、新しい`AffineRow`だけを`add_observations`して再実行できます。変数の数・上下限を変える場合の扱いは第3.7節を参照してください。

U16には、同じ三入力の一次関数を使う次の亜種があります。いずれも一つの推定関数と`main`を持つ独立したコード例です。

| 亜種 | 何を変えるか | 選ぶ目安 |
|---|---|---|
| U16-A | 四係数と共通の標準偏差$\sigma$を一緒に探索する | 未知の誤差尺度を独自損失から参照する書き方を使いたい |
| U16-B | 四係数を探索し、残差から$\sigma$を計算する | この一次モデル・等しい観測重み・事前項なしで、係数と$\sigma$の点推定を得たい |
| U16-C | 四係数を整数に限定する | 内部の設定値が整数であることが分かっている |

$\sigma$が既知なら、基本例の`AffineRow{{x1,x2,x3},y,sigma}`で指定します。U16-A・U16-Bは、その共通の値自体が未知の場合です。

#### 5.16.1 U16-A：四係数と共通の標準偏差を同時に推定する

個別コード：`examples/mcmc_estimator_guide_example_u16a_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=161`。

**具体的な問題。** 基本例と同じ装置で、同じ三入力を与えても測定のたびに表示が少し変わるとします。四係数は全測定で共通ですが、測定系がどの程度ぶれるかも分かりません。全観測に共通する一つの標準偏差$\sigma$を、四係数と一緒に調べます。「共通」とは値が測定ごとに変わらないという意味で、値が既知という意味ではありません。

**問題から目的関数へ。** 第$i$番目の測定は、平均値に誤差$\varepsilon_i$を加えたものと考えます。

$$y_i=a_1x_{i1}+a_2x_{i2}+a_3x_{i3}+b+\varepsilon_i,\qquad \varepsilon_i\sim\mathcal N(0,\sigma^2)$$

$i$は観測番号、$x_{i1},x_{i2},x_{i3}$は既知の入力、$y_i$は実測値です。$a_1,a_2,a_3,b$は未知係数です。$\mathcal N(0,\sigma^2)$は平均0、分散$\sigma^2$の正規分布を表し、誤差は観測間で独立と仮定します。$\sigma>0$は表示値と同じ単位の、通常のぶれの大きさです。

観測数を$N$とすると、未知の組$\theta=(a_1,a_2,a_3,b,\sigma)$に対する目的関数は次です。

$$E(\theta)=\frac{1}{2\sigma^2}\sum_{i=1}^{N}(y_i-a_1x_{i1}-a_2x_{i2}-a_3x_{i3}-b)^2+N\log\sigma$$

$E$は観測の負の対数尤度から候補によらない定数を除いた値です。$\log$は自然対数です。第一項は「予測からのずれが、通常のぶれの何倍か」を評価します。第二項も必要で、これを省くと$\sigma$を大きくするだけで第一項を小さくできてしまいます。コードでは各観測の損失に$\log\sigma$を一回ずつ含めるため、合計が$N\log\sigma$になります。

この例は四係数をそれぞれ$-10$以上10以下、$\sigma$を0.001以上10以下に制限し、事前項を0にします。初期値は四係数が0、$\sigma$が1です。標準偏差の単位は観測値と同じなので、下限・上限・初期値は測定系に合わせます。下限0.001は、あらゆる問題に適した値という意味ではありません。

**モデルからコードへ。** 四係数と$\sigma$の計5変数を`p.real`で宣言します。予測関数は平均値だけを返し、独自損失が候補状態から$\sigma$を読みます。`Solver::gaussian_loss(observed,predicted,sigma)`は、二乗誤差項と$\log\sigma$の両方を計算する補助関数です。

標準の`Gaussian`が使う`row.scale`は既知の誤差尺度なので、この例の未知変数を置く場所にはしません。入力する行は`AffineRow{{x1,x2,x3},observed_y}`とし、`scale`と`weight`は全て既定値1で統一します。サンプルはこの前提を`assert`で確認します。重みを変える場合は目的関数自体も変わるため、U16-Bとの等価性をそのまま使わないでください。

**呼び出しと結果。** `fit_affine3_joint_sigma(rows,budget)`は、空でない観測列を受け取り、5変数のSessionを作って`solve`します。戻り値は`std::optional<AffineSigmaEstimate>`です。空なら候補が得られていません。値があれば`a[0],a[1],a[2],b`が四係数、`sigma`が共通の標準偏差の推定値です。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;
using AffineRow = Solver::Observation<AffineInput>;

struct AffineSigmaEstimate {
    std::array<double, 3> a; // a1,a2,a3
    double b;
    double sigma; // 全観測に共通するノイズの標準偏差
};

// 入力: {{x1,x2,x3}, 観測値y}の列、計算予算。
// 全行でscale=1,weight=1とする。未知のsigmaは行に指定しない。
// 出力: 予算内で見つけた最良の四係数とsigma。候補なしならnullopt。
std::optional<AffineSigmaEstimate> fit_affine3_joint_sigma(
    std::vector<AffineRow> rows, Solver::Budget budget) {
    assert(!rows.empty());
    assert(std::all_of(rows.begin(), rows.end(), [](const AffineRow& row) {
        return row.scale == 1 && row.weight == 1;
    }));
    Solver::Parameters p;
    const std::array<std::size_t, 3> ai{
        p.real(0, -10, 10), p.real(0, -10, 10), p.real(0, -10, 10)};
    const auto bi = p.real(0, -10, 10);
    // real(初期値,下限,上限)。sigmaの範囲は観測値の単位に合わせて変更する。
    const auto si = p.real(1.0, 0.001, 10.0);

    // 候補係数から平均値を予測する。ここで乱数を加えない。
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.real[ai[0]]*x[0] + s.real[ai[1]]*x[1]
             + s.real[ai[2]]*x[2] + s.real[bi];
    };
    // 既知のrow.scaleではなく、候補状態に入っている共通sigmaを使う。
    auto loss = [si](const McmcState& s, const AffineRow& row, double predicted) {
        // gaussian_lossは、二乗誤差項とlog(sigma)の両方を含む。
        return Solver::gaussian_loss(row.value, predicted, s.real[si]);
    };
    auto model = Solver::make_observation_model(std::move(rows), predict, loss);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;
    const auto& s = *result.state;
    return AffineSigmaEstimate{
        {s.real[ai[0]], s.real[ai[1]], s.real[ai[2]]}, s.real[bi], s.real[si]};
}

int main() {
    // 動作確認用: a=(2,-3,0.5),b=1、独立な正規ノイズの標準偏差は0.2。
    // 真値はデータ生成にだけ使う。推定関数へsigmaの真値を渡さない。
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0.0, 0.2);
    std::vector<AffineRow> rows;
    for (int repeat = 0; repeat < 4; ++repeat)
        for (double x1 : {-1.0, 0.0, 1.0})
            for (double x2 : {-1.0, 0.0, 1.0})
                for (double x3 : {-1.0, 0.0, 1.0})
                    rows.push_back({{x1,x2,x3}, 2*x1 - 3*x2 + 0.5*x3 + 1 + noise(rng)});

    // 実際には手元の観測列を渡す。時間指定ならBudget::for_us(20'000)など。
    const auto r = fit_affine3_joint_sigma(std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;
    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << " sigma=" << r->sigma << '\n';
    return 0;
}
```

`main`では、27通りの三入力を4回ずつ測り、合計108件を作ります。測定に加える正規ノイズの標準偏差は0.2です。生成側の真値は推定関数へ渡しません。有限個のノイズから推定するため、推定された$\sigma$は0.2ちょうどにはなりません。U16-Bの`main`も同じ乱数と観測を使います。

**注意と発展。** ほぼ完全に観測を再現できる場合、$\sigma$は下限へ寄ることがあります。観測数が少なく未知係数で測定誤差まで吸収していないか、下限が測定系として妥当かを確認します。ここで$\sigma$は標準偏差そのものを状態に置いているため、U06の対数尺度での一様分布とは異なります。このSessionで確率的な標本採取へ進むなら、事前項0は指定範囲内で$\sigma$そのものについて一様に扱う設定です。

#### 5.16.2 U16-B：四係数の推定後、残差から標準偏差を求める

個別コード：`examples/mcmc_estimator_guide_example_u16b_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=162`。

**具体的な問題。** 調べたいものはU16-Aと同じ四係数と共通の$\sigma$です。この一次モデルで、全観測の重みが等しく、事前項がないという条件を使い、四係数を推定した後に残差から$\sigma$を計算します。

**残差とは何か。** 推定した四係数による予測を$\widehat y_i$とすると、実測との差$r_i=y_i-\widehat y_i$を残差と呼びます。残差の二乗を全行について足したものを$\mathrm{RSS}$と書きます。

$$\mathrm{RSS}=\sum_{i=1}^{N}r_i^2,\qquad r_i=y_i-\widehat y_i$$

$N$は観測数です。二乗することで正負のずれが打ち消し合うのを防ぎます。$\mathrm{RSS}/N$は一件あたりの二乗誤差、その平方根は元の表示値と同じ単位の誤差の大きさになります。

**なぜこの計算でよいか。** 四係数を固定して$\sigma$だけを見ると、U16-Aの目的関数は次です。

$$E(\sigma)=\frac{\mathrm{RSS}}{2\sigma^2}+N\log\sigma$$

$\sigma$を増やすと第一項は減り、第二項は増えます。最小になる釣り合いを微分で調べると、次になります。$dE/d\sigma$は、$\sigma$を少し増やしたときの$E$の変化の割合です。

$$\frac{dE}{d\sigma}=-\frac{\mathrm{RSS}}{\sigma^3}+\frac{N}{\sigma}=0$$

$\mathrm{RSS}>0$の場合、これを整理すると$N\sigma^2=\mathrm{RSS}$なので、正の解は次です。

$$\widehat\sigma_{\mathrm{ML}}=\sqrt{\frac{\mathrm{RSS}}{N}}$$

添字MLは最尤推定を表します。この値を目的関数に入れると$N/2+(N/2)\log(\mathrm{RSS}/N)$となり、$\mathrm{RSS}$が小さいほどよいことが分かります。したがって、同じ係数範囲で四係数の二乗誤差を最小化し、その後に上式を使えば、共通$\sigma$も同時に最尤推定する場合と整合します。探索が途中なら、推定した四係数の残差に対する値が返ります。

この簡略化は、正規ノイズの尺度が全観測で共通、重みが全て1、事前項なしという前提のものです。係数や$\sigma$への事前項、外れ値用損失、観測ごとに異なる未知の尺度などへ変更した場合は、その目的関数から計算式を見直します。

**モデルからコードへ。** 基本例と同じ四係数だけを宣言し、観測行の`scale=1,weight=1`で二乗誤差を小さくします。`solve`後に`session.model()`から読み取り専用のモデルを取得し、`observations()`で各観測を読み、`prediction(input,state)`で最良係数の予測を計算します。モデルが所有する観測と予測関数を再利用できるため、残差計算用に観測列や予測式を複製する必要はありません。

**呼び出しと結果。** `fit_affine3_residual_sigma(rows,budget)`の入力はU16-Aと同じです。戻り値も`std::optional<AffineSigmaEstimate>`で、`sigma`が残差の二乗平均の平方根です。`main`はU16-Aと同じ観測を使うため、四係数と$\sigma$を比較できます。二つの方法は探索の進み方が異なるので、有限予算での結果が完全一致するとは限りません。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;
using AffineRow = Solver::Observation<AffineInput>;

struct AffineSigmaEstimate {
    std::array<double, 3> a; // a1,a2,a3
    double b;
    double sigma; // sqrt(残差平方和/観測数)。完全一致なら0になり得る。
};

// 入力: {{x1,x2,x3}, 観測値y}の列、計算予算。
// 共通の未知sigmaを推定するため、全行でscale=1,weight=1とする。
// 出力: 最良の四係数と、その残差から計算したsigma。候補なしならnullopt。
std::optional<AffineSigmaEstimate> fit_affine3_residual_sigma(
    std::vector<AffineRow> rows, Solver::Budget budget) {
    assert(!rows.empty());
    assert(std::all_of(rows.begin(), rows.end(), [](const AffineRow& row) {
        return row.scale == 1 && row.weight == 1;
    }));
    Solver::Parameters p;
    const std::array<std::size_t, 3> ai{
        p.real(0, -10, 10), p.real(0, -10, 10), p.real(0, -10, 10)};
    const auto bi = p.real(0, -10, 10);
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.real[ai[0]]*x[0] + s.real[ai[1]]*x[1]
             + s.real[ai[2]]*x[2] + s.real[bi];
    };
    // 探索するのは四係数だけ。既定のGaussian損失で二乗誤差を小さくする。
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;
    const auto& s = *result.state;

    // Sessionが所有する観測と予測関数を再利用し、残差平方和を計算する。
    // この全観測の走査はsolveから戻った後に行うので、実時間予算に余裕を残す。
    const auto& model = session.model();
    double rss = 0;
    for (const auto& row : model.observations()) {
        const double residual = row.value - model.prediction(row.input, s);
        rss += residual * residual;
    }
    const double sigma = std::sqrt(rss / static_cast<double>(model.size()));
    return AffineSigmaEstimate{
        {s.real[ai[0]], s.real[ai[1]], s.real[ai[2]]}, s.real[bi], sigma};
}

int main() {
    // U16-Aと同じ観測を作り、残差からのsigma推定と比較できるようにする。
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0.0, 0.2);
    std::vector<AffineRow> rows;
    for (int repeat = 0; repeat < 4; ++repeat)
        for (double x1 : {-1.0, 0.0, 1.0})
            for (double x2 : {-1.0, 0.0, 1.0})
                for (double x3 : {-1.0, 0.0, 1.0})
                    rows.push_back({{x1,x2,x3}, 2*x1 - 3*x2 + 0.5*x3 + 1 + noise(rng)});

    const auto r = fit_affine3_residual_sigma(std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;
    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << " sigma=" << r->sigma << '\n';
    return 0;
}
```

**σの範囲とゼロ残差。** U16-Bは計算した残差尺度をそのまま返します。U16-Aの$0.001\leq\sigma\leq10$と値を比較する場合、その範囲外では結果の扱いが違います。同じ制約付きの$\sigma$が必要なら、残差からの値を`std::clamp(sigma,0.001,10.0)`で範囲内へ収めます。

$\mathrm{RSS}=0$なら、U16-Bは0を返します。この場合、正の$\sigma$の範囲には有限の最尤解がなく、0へ近づけるほど尤度が大きくなります。戻り値0は「残差が全て0」という計算結果として扱い、後続のGaussian損失へ渡すなら正の下限を設けます。完全一致していても、観測不足で測定誤差まで係数に吸収している可能性があります。

**分母NとN−4の違い。** コードは最尤推定に対応する$\mathrm{RSS}/N$を使います。連続な四係数を通常の線形最小二乗法で推定し、四係数を区別できる入力があり、上下限制約が解に影響しない場合は、$N>4$で次の分散推定も使われます。

$$s^2=\frac{\mathrm{RSS}}{N-4}$$

ノイズの一部も使って四係数を合わせた影響を補正するもので、正しい一次モデルと独立な同じ正規ノイズという仮定の下で、$s^2$は分散$\sigma^2$の不偏推定になります。四係数の数が4なので分母が$N-4$になります。$\sqrt{s^2}$そのものが標準偏差の不偏推定になるわけではありません。また、整数係数、制約が効く解、事前項付き推定、最小二乗解まで十分探索できていない場合に、この補正を機械的に適用しないでください。

**計算予算と使い方。** 残差集計は`solve`後の全観測一回の走査で、計算量は観測数に比例します。この処理は`solve`の時間チェックの外なので、実時間予算に余裕を残します。また、残差には測定ノイズだけでなく、式の当てはまりの悪さや探索不足も含まれます。ここで返すのは一点の推定であり、$\sigma$の不確かさを表す標本や区間ではありません。

#### 5.16.3 U16-C：四係数が整数である場合

個別コード：`examples/mcmc_estimator_guide_example_u16c_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=163`。

**具体的な問題。** 三つの入力に対する装置の補正係数が、内部で整数ステップに設定されているとします。入力と表示は小数を含んでも、未知の$a_1,a_2,a_3,b$は整数に限られます。この既知の条件を探索候補にも反映します。

予測式は基本例と同じ$a_1x_1+a_2x_2+a_3x_3+b$、目的関数も予測と実測の二乗誤差です。違いは四係数の許容範囲で、例ではそれぞれ$-10,-9,\ldots,10$の整数のみを許します。値が小さいほどよい目的関数の意味や、入力の変化が不十分だと係数を区別できない点は基本例と共通です。

**モデルからコードへ。** 四係数を`p.integer(0,-10,10)`で登録し、返された添字を`state.discrete`に使います。探索中も整数の候補を作るため、返却時の丸め処理はありません。出力の`AffineIntegerEstimate`も`std::int64_t`を持ちます。一方、入力は`std::array<double,3>`なので、整数係数を掛けた予測値の計算は`double`で行われます。

**呼び出しと結果。** `fit_affine3_integer(rows,budget)`へ観測列と予算を渡し、値が得られたら`a[0],a[1],a[2],b`を整数係数として使います。`main`は$a_1=2,a_2=-3,a_3=1,b=1$で観測を作ります。三番目の係数も整数とし、入力には0.5や0.25を含めることで、整数に制限されるのが係数であることを示します。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;
using AffineInput = std::array<double, 3>;

// 入力x1,x2,x3と観測値yは実数でよい。整数に制限するのは四係数。
// 1行 = {{x1,x2,x3}, y, 誤差の標準偏差scale, 重みweight}。
// scaleとweightは省略すると1。既知の標準偏差sigma>0は第3要素に指定する。
using AffineRow = Solver::Observation<AffineInput>;

struct AffineIntegerEstimate {
    std::array<std::int64_t, 3> a; // a[0]=a1, a[1]=a2, a[2]=a3
    std::int64_t b;
};

// 入力: 全観測に共通する整数係数で得た観測、計算予算。
// 出力: 探索中に見つけた最良の整数係数。候補を得られなければnullopt。
std::optional<AffineIntegerEstimate> fit_affine3_integer(
    std::vector<AffineRow> rows, Solver::Budget budget) {

    assert(!rows.empty());
    Solver::Parameters p;

    // integer(初期値, 下限, 上限)。上下限を含む整数だけを探索する。
    // 範囲は問題の事前知識に合わせて変更する。
    const std::array<std::size_t, 3> ai{
        p.integer(0, -10, 10),
        p.integer(0, -10, 10),
        p.integer(0, -10, 10)
    };
    const auto bi = p.integer(0, -10, 10);

    // 整数係数はstate.discreteから読む。
    // 入力xがdoubleなので、予測値の計算はdoubleになる。
    auto predict = [ai, bi](const AffineInput& x, const McmcState& s) {
        return s.discrete[ai[0]] * x[0]
             + s.discrete[ai[1]] * x[1]
             + s.discrete[ai[2]] * x[2]
             + s.discrete[bi];
    };

    // 既定のGaussian損失で、予測値と実測値を比較する。
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;

    const auto& s = *result.state;
    return AffineIntegerEstimate{
        {s.discrete[ai[0]], s.discrete[ai[1]], s.discrete[ai[2]]},
        s.discrete[bi]
    };
}

int main() {
    // 動作確認用データ: y = 2*x1 - 3*x2 + x3 + 1。
    // 真の係数a1=2,a2=-3,a3=1,b=1は、全て整数。
    // 実際には、この生成部分を手元の入力と観測値に置き換える。
    std::vector<AffineRow> rows;
    for (double x1 : {-1.0, 0.0, 1.0})
        for (double x2 : {-0.5, 0.0, 0.5})
            for (double x3 : {-0.25, 0.0, 0.25})
                rows.push_back({{x1,x2,x3}, 2*x1 - 3*x2 + x3 + 1});

    // 固定回数で実行。時間指定ならBudget::for_us(20'000)など。
    const auto r = fit_affine3_integer(
        std::move(rows), Solver::Budget::for_steps(80'000));
    if (!r) return 1;

    std::cout << "a1=" << r->a[0] << " a2=" << r->a[1]
              << " a3=" << r->a[2] << " b=" << r->b << '\n';
    return 0;
}
```

既知の標準偏差がある場合は、基本例と同じように観測行の第3要素へ正の値を指定します。整数係数でも、観測値にノイズが含まれて構いません。返るのは予算内で見つかった最良の整数候補であり、全ての整数の組を列挙した厳密解を保証するものではありません。

整数係数と共通の未知$\sigma$を組み合わせたい場合も、U16-Aの独自損失を使えます。四係数を`integer`、$\sigma$だけを`real`に登録し、係数は`discrete`、$\sigma$は`real`から読みます。U16-Bの$\sqrt{\mathrm{RSS}/N}$も、等しい重み・事前項なしの条件なら、得られた整数係数に対する尺度として計算できます。ただし、上の$N-4$による分散補正を整数係数へそのまま適用することはできません。


### 5.17 U17：一次・二次・正弦のどの応答かを、係数と一緒に推定する

個別コード：`examples/mcmc_estimator_guide_example_u17_v06.cpp`。一体版では`MCMC_GUIDE_EXAMPLE=17`。

**具体的な問題。** 入力$x$に対して表示値$y$を返す装置があります。応答は「直線的に増える」「入力の二乗に従う」「正弦波のように変わる」の三種類のいずれかですが、種類を示す設定が読めません。さらに、応答の倍率$a$と、表示全体のずれ$b$も不明です。様々な$x$で測った表示から、種類$t$と二つの係数を一緒に推定します。

**観測からモデルへ。** 候補にする式は、利用者が次の三つに限定して指定します。全ての観測は同じ装置から得られ、同じ$t,a,b$を共有するものとします。

| 種類$t$ | 予測式 | 式の意味 |
|---|---|---|
| 1 | $ax+b$ | 入力に対して一定の割合で増減する一次関数 |
| 2 | $ax^2+b$ | 正負が逆の同じ大きさの入力では同じ値になる二次関数 |
| 3 | $a\sin x+b$ | 入力を動かすと上下に繰り返し変化する正弦関数。$x$はラジアン |

$x$は既知の入力、$a$は符号も含めた未知の倍率、$b$は未知の基準表示、$t$は未知の種類です。$t$は観測ごとに別々に決める値ではありません。また、正弦関数の周波数や位相は、このモデルでは未知変数にしていません。

ここで、$g_1(x)=x$、$g_2(x)=x^2$、$g_3(x)=\sin x$という三つの既知の関数を用意すると、予測を共通の形で書けます。

$$f(x;\theta)=a g_t(x)+b,\qquad \theta=(t,a,b)$$

$g_t$は$t$で選んだ関数、$f$は選択と係数適用を含む予測関数、$\theta$は未知の組全体です。観測番号$i$、観測数$N$、入力$x_i$、実測値$y_i$を使うと、コードで小さくする目的関数は次です。

$$E(t,a,b)=\frac{1}{2}\sum_{i=1}^{N}(y_i-a g_t(x_i)-b)^2$$

$E$は全観測での予測誤差を二乗して足した値です。$t$は1、2、3のいずれか、$a,b$はそれぞれ$-10$以上10以下とします。Gaussian損失の`scale=1, weight=1`と事前項0を使います。既知の観測誤差に応じた重み付けをしたい場合は、第3.5節の`scale`を設定します。

**モデルからコードへ。** `category`は種類を表すため、式同士に順序や距離を定める必要がありません。ライブラリ内部のカテゴリIDは0、1、2です。そこで、ID 0を一次、ID 1を二次、ID 2を正弦に対応させ、返却時に1を足して$t$へ戻します。

| 指定・結果 | コードとの対応 |
|---|---|
| 一件の観測 | `FunctionRow{x,y}`。入力は`double`一つで、種類$t$は行に入れない |
| 式の種類 | `p.category(0,3)`。初期ID 0、候補数3。返された添字`ti`を`state.discrete`に使う |
| 倍率と基準表示 | `p.real(0,-10,10)`を2回呼び、添字`ai,bi`を`state.real`に使う |
| 再現計算 | `predict`内の`switch`で候補の式を選び、候補$a,b$を代入して一つの実数を返す |
| 推定の実行 | 観測モデルと数値Sessionを作り、`solve(budget)`を呼ぶ |
| 返される組 | `FunctionEstimate::t`は1〜3の`int`、`a,b`は`double` |

**呼び出しと結果。** `fit_function_type(rows,budget)`へ空でない観測列を渡します。戻り値は`std::optional<FunctionEstimate>`で、値が得られたら、その`t`の式に`a,b`と新しい入力を代入して予測します。例えば`t=3,a=2,b=1`なら、入力$\pi/2$に対する予測は3です。$\pi$は円周率で、$\pi/2$ラジアンは90度に相当します。

```cpp
#include "mcmc_estimator_v09.hpp"

using Solver = McmcEstimator<>;

// 1行 = {入力x, 観測値y, 誤差の標準偏差scale, 重みweight}。
// scaleとweightは省略すると1。通常の二乗誤差なら省略してよい。
using FunctionRow = Solver::Observation<double>;

struct FunctionEstimate {
    int t; // 1: a*x+b、2: a*x*x+b、3: a*sin(x)+b
    double a, b;
};

// 入力: 全観測に共通の未知t,a,bで得た観測、計算予算。
// xの単位はラジアン。
// 出力: 探索中に見つけた最良の組。評価が完了しなければnullopt。
std::optional<FunctionEstimate> fit_function_type(
    std::vector<FunctionRow> rows, Solver::Budget budget) {

    assert(!rows.empty());
    Solver::Parameters p;

    // category(初期ID, 候補数)。内部IDは0,1,2なので、t=内部ID+1。
    const auto ti = p.category(0, 3);

    // real(初期値, 下限, 上限)。範囲は問題に合わせて変更する。
    const auto ai = p.real(0, -10, 10);
    const auto bi = p.real(0, -10, 10);

    // 候補のt,a,bと入力xから予測値を計算する。
    auto predict = [ti, ai, bi](double x, const McmcState& s) {
        const double a = s.real[ai], b = s.real[bi];
        switch (s.discrete[ti]) {
            case 0: return a*x + b;            // t=1
            case 1: return a*x*x + b;          // t=2
            default: return a*std::sin(x) + b; // t=3
        }
    };

    // 式の種類tと実数a,bを一緒に探索する。
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(budget);
    if (!result.state) return std::nullopt;

    const auto& s = *result.state;
    return FunctionEstimate{
        static_cast<int>(s.discrete[ti]) + 1, s.real[ai], s.real[bi]};
}

int main() {
    // 3種類それぞれを、真の係数a=2,b=1で試す。
    // true_tは動作確認用データの生成にだけ使い、推定関数には渡さない。
    for (int true_t : {1, 2, 3}) {
        // 実際には、手元の入力と観測値からこのvectorを作る。
        std::vector<FunctionRow> rows;
        for (double x : {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
            const double response = true_t == 1 ? x
                                  : true_t == 2 ? x*x : std::sin(x);
            rows.push_back({x, 2*response + 1});
        }

        // 固定回数で実行。時間指定ならBudget::for_us(20'000)など。
        const auto r = fit_function_type(
            std::move(rows), Solver::Budget::for_steps(80'000));
        if (!r) return 1;

        std::cout << "true_t=" << true_t << " estimated_t=" << r->t
                  << " a=" << r->a << " b=" << r->b << '\n';
    }
    return 0;
}
```

`main`は三種類を別々に試します。それぞれ$a=2,b=1$で7件の観測を作り、推定関数にはその観測だけを渡します。`true_t`は動作確認データを作るための値で、推定関数に答えとして渡していません。`estimated_t`が1、2、3、各係数が2と1の付近になることを確認できます。実際には、一つの装置から得た観測列について一度呼び出せばよく、真の種類を使う外側のループは不要です。

**種類を区別するための注意。** 候補の式を別々に定義しても、どんな観測からでも種類が決まるわけではありません。

- $a=0$なら全種類が同じ定数$b$になり、観測だけでは$t$を区別できません。
- $x=0$付近だけでは$\sin x$と$x$が近いため、誤差を含む観測で一次と正弦を区別しにくくなります。正負や大きさを変えた入力が役立ちます。
- 入力点が少ないと、違う種類の式でも係数を変えて同じ観測を説明できる場合があります。この例では、-3から3まで大きさの異なる入力を使います。
- `std::sin`の引数はラジアンです。角度を度で記録している場合は、入力時にラジアンへ換算します。種類ごとに入力の意味を勝手に変えないようにします。

`solve`は予算内で得た最良候補を返すため、返された$t$が真の種類であるという確率や保証は付きません。二乗誤差の式へ観測の標準偏差や事前項を追加したい場合は第2章、複数候補の不確かさを利用したい場合は第1.3節・U15へ進んでください。同じ種類と係数を持つ装置の観測を追加しながら使う場合は、U13と同様にSessionを保持できます。

## 6. 制約・注意点

### 6.1 モデルと数値の制約

| 注意点 | 起こり得る問題・対処 |
|---|---|
| 観測だけでは区別できない候補がある | 真のパラメータが一意に決まるとは限らない。入力の種類を増やす、基準を固定する、事前情報を加える、予測精度で評価する |
| 全初期候補が制約違反 | 有限候補を見つけるまで自動探索する仕組みではない。少なくとも一つ有限な開始状態を用意する |
| 一部の初期候補だけが制約違反 | 初期同期後に有限な最良候補を複製して無効な鎖を補う。初期集団の多様性まで保証する処理ではない |
| 評価値や中間値にNaN・負の無限大 | 契約違反。入力、割り算、対数、指数、総和を通常のdoubleに収まる範囲で設計する |
| 無限に広い範囲と事前項0 | 標本の対象分布を正規化できない場合がある。正の有限な正規化定数を持つモデルにする |
| 目的関数の観測数による自動平均を期待する | 自動平均しない。lossとpriorの相対的な強さを意図して設定する |
| 目的関数全体に係数を掛ける | 最小点が同じでも、Sampleの分布の広がりは変わる。Searchの温度との対応も変わる |
| 変数に依存する正規化項を省く | 未知のsigmaなどで推定が変わる。U06のように必要な項を含める |
| 指数・対数による変数変換 | 範囲だけでなく、事前分布をどの変数について定めたかを明確にする |
| 整数・IDに極大値を使う | 整数の絶対値・幅は$10^9$以下を想定。鎖数や乱数の上限もintに収まる通常規模で使う |
| `-ffast-math`を使う | 無限大や有限判定に依存するコードと両立しない。使わない |

条件違反は主にassertで検出します。例外をthrowする設計ではありません。assertを無効にしたビルドでも、条件違反が許されるわけではありません。

U16で常に二入力が等しい場合や、U17で倍率が0の場合のように、観測だけでは係数や種類を区別できないことがあります。探索予算を増やしても、観測に含まれていない情報は得られません。入力の変化や既知の制約を見直します。

未知の標準偏差を推定する場合は、二乗誤差項だけでなく対数項も含めます。U16-Aは正の範囲内で標準偏差を探索し、U16-Bは残差尺度を計算するため完全一致なら0を返します。分母NとN−4の条件や、この0の扱いは第5.16.2節を参照してください。

### 6.2 観測・更新で間違えやすい点

| 注意点 | 対処 |
|---|---|
| `weight=0`なら予測も省かれると思う | 損失は0でも予測は呼ばれる。無効な入力を渡さない。Huberや成否・件数では重み0でも入力条件のassertがある |
| 独自観測の`input`がない | 観測モデルは`row.input`を予測へ渡す。独自損失だけではこの構造を省けない |
| 生の確率をBernoulliLogitへ渡す | logitを返す。成功確率を取り出すときは`mean_prediction` |
| 件数の平均をPoissonLogMeanへ渡す | 平均の自然対数を返す。観測値は非負の整数 |
| 損失にもContext引数が来ると思う | Contextが追加されるのは予測と事前項。損失は状態・観測行・予測値を受け取る |
| 読み取り専用の`fit.model()`から全energyを呼ぶ | 観測モデルの全体評価演算子は非const。公開の読み取り用途は`prediction`、`observations`など。全energyを自分で確認する場合はモデルの別コピー等を使う |
| 参照キャプチャの対象を書き換えて、そのまま続ける | 候補の保存energyとモデルが食い違う。変更は通知可能な所有Contextや独自モデルに置く |
| 各ターンで観測の全履歴をaddする | 重複した尤度になる。addは新規分、replaceは残す全体 |
| 再評価が中断したので同じ更新をもう一度登録する | addなどの登録は既に完了している。再びsolve/sampleだけを呼ぶ |
| Contextの現在値で、時刻ごとに違った過去まで解釈する | 過去の環境を観測ごとに保存する。登録訂正とは分ける |
| Session作成後にParametersを書き換える | Sessionは既に初期値とDomainを所有している。変更は反映されない |
| 型や変数の意味を実行中に変更する | 同じ構造という前提を壊す。Sessionを再構築する |

### 6.3 実行・結果・標本の制約

- **空の結果を確認する。** 初期評価前、変更直後、予算不足などではstateが空になり得ます。標本も0件、分散は1件では空です。
- **標本の意味を混ぜない。** `solve`の最良候補やSearch中の履歴は、事後分布の標本ではありません。標本は`sample`・`sample_each`から取ります。最良候補は、標本採取中でも標本平均とは異なります。
- **標本数は独立な情報量ではない。** 棄却時に同じ状態を繰り返し記録するのは必要な動作です。受理された状態だけ集計すると、対象と異なる分布になります。
- **準備は予算に含まれる。** 小さい予算を何回も渡して準備を続行できます。Searchで実際に遷移した後やモデルを変更した後は、標本の準備が再設定されます。
- **標本の自動収束判定はない。** 有効標本数、自己相関、鎖間の収束診断、分位点、共分散行列は提供しません。複数初期値・試行の結果や、用途に必要な診断を別に確認します。
- **同じシードだけで常に同じ結果とは限らない。** 固定した遷移数、同じ初期状態・モデル・提案・実行順・冷却計画なら再現しやすくなります。実時間予算では実行環境により完了数が変わります。
- **Searchを分割するなら冷却計画を指定する。** 比較したい全体遷移数か共通期限を与えます。小予算の都度、同じ初期温度で再開始する仕様ではありません。
- **制限時間は強制停止ではない。** 一つの予測、全体評価、提案、状態コピー、出力処理が長ければ期限を超え得ます。観測モデルは項ごとに中断可能ですが、一項内部には介入しません。
- **ゼロ予算には新規評価を期待しない。** 同じSampleフェーズで既に完了していた未出力標本は、時間が残れば、遷移数0の呼び出しでも出力されることがあります。新しい遷移を行ったという意味ではありません。
- **ビューを長く保持しない。** `best().state`、観測span、MoveContextのpopulation、emitに渡された状態参照は、更新・次の実行をまたぐ保存には向きません。必要ならコピーします。`solve`のstateは戻り値が所有します。
- **コールバックは呼び出し中だけ実行する。** project・emitは保存されません。コールバック内から同じSessionを再実行したり更新したりしません。計算時間を使う処理や大量の標本保存は、全体の予算・記憶量に含めて考えます。
- **単スレッドで使う。** 同じ推定器・モデルを複数スレッドから共有して評価する設計ではありません。

### 6.4 独自変更・差分評価の制約

`make_numeric_session`は、realとdiscreteを持つ標準数値状態に使います。順列などには`make_session`を使い、自分の制約を守る提案を用意します。独自状態でも、初期状態から到達できない候補があると、その候補は探索できません。

`symmetric_move`は、単に「変更前へ戻せる」というだけでは不十分です。正逆の提案確率が一致する必要があります。一致しない場合は`hastings_move`などで正しい比を返します。Sample中に過去の受理率などを見て分布を変える独自適応も、ラッパーが自動で正しくするわけではありません。

独自の`candidate`を持つモデルは、全体評価も必須です。候補の採用・棄却にかかわらず純粋な評価として使い、共有キャッシュを候補に合わせて破壊的に更新しません。高速化が必要になってから追加し、全体評価との一致を確認するのが安全な導入順です。

### 6.5 掲載コードの確認方法

第5章の基本17例とU16の3亜種、全20個のC++コードを本文から抽出し、配布する各個別ファイルと一致することを確認しています。各コードを個別の実行ファイルとしてビルド・実行し、既知の係数・予測値、一次・二次・正弦の種類、標準偏差の推定、整数係数、確率の範囲、標本数、割合の合計、順列、観測更新後の結果を検査しています。一体版も1〜17と161〜163の全選択値でコンパイル・実行し、個別版と同じ出力になることを確認しています。

追加検査では、予算0・準備未完了の戻り値、複数回の観測追加、値を実際に変えた観測訂正、モデル改訂番号、途中でのモデル変更、同じ推定器からの再開を確認しました。U16-A・U16-Bは別のノイズ付き観測に対する最小二乗解と残差尺度との比較、予算0、残差0と標準偏差の正の下限も検査しています。詳細は同梱の検証プログラムと実行結果にあります。これらはコードの利用契約と例の計算結果の検証であり、あらゆる入力で推定が収束する証明ではありません。

検証環境はGCC 13.3.0、C++20です。通常のassertあり、`NDEBUG`、AddressSanitizerとUndefinedBehaviorSanitizerの設定で確認しています。LeakSanitizerは実行環境の制約で無効にしており、リーク検査は含みません。GCC 12.2そのものでは実行していないため、その環境での確認を代替するものではありません。

## 7. 実装

### 7.1 全体の構成と流れ

実装は、状態と実行を管理するコア、通常の数値変更を担当するNumericMove、予測と損失をまとめるObservationModel、それらを所有して更新を管理するSessionから成ります。利用者は通常Sessionだけを操作します。

一回の実行は次の流れです。

1. 初期状態を評価するか、モデル変更で無効になった保持候補を再評価する。
2. 各鎖を順番に選び、現状態をコピーして候補を作る。
3. 候補の目的関数を計算する。必要なら観測の途中で中断し、再開用の進捗を保存する。
4. 最良候補を更新し、提案比と目的関数差から採用・棄却を決める。
5. Sampleなら、鎖ごとの間隔に従って現状態を出力する。
6. 予算が残る間続け、進捗と結果を返す。

Searchは最小値探索、Warmupは標本採取前の準備、Sampleは標本を出力するフェーズです。Sessionのsample系メソッドは、準備の残りがあれば先にWarmupを行います。いずれも鎖を一つずつ更新し、並列実行はしません。

### 7.2 受理判定と最良候補

現在の状態を$s$、候補を$t$、温度を$T>0$、提案確率を$q(t\mid s)$とすると、採用確率は次です。

$$A(s,t)=\min\left(1,\exp\left(-\frac{E(t)-E(s)}{T}+\log q(s\mid t)-\log q(t\mid s)\right)\right)$$

$E$は目的関数、$A$は採用確率です。実装では0より大きく1以下の一様乱数$U$を作り、その対数$\log U$を指数の中身と比較します。大きな指数を直接作らないための計算です。

対称提案なら、二つのlog提案確率の差は0になります。WarmupとSampleの温度は1です。Searchは温度を変えて探索し、最適化用の変更方法も使うため、その履歴を同じ意味の事後標本とは扱いません。

最良候補は、**有限のenergyを評価できた全候補**から選びます。採用されなかった候補でも最良値なら保存します。そのため`best()`は、現在の鎖のどれかや、直近の標本と一致するとは限りません。

全ての初期候補を評価しても有限値がなければ`NoFiniteState`です。有限候補がある場合は、無効だった鎖をその候補で補い、有限な状態から遷移を始めます。

### 7.3 Searchの冷却と座標変更

温度は開始温度$T_0$から終端温度$T_1$まで、進捗$r$に従って指数的に変わります。

$$T(r)=T_0\exp\left(r\log\frac{T_1}{T_0}\right),\qquad 0\leq r\leq1$$

$r$は予定遷移数または共通の期限から求める0〜1の進捗です。予定を過ぎた部分は1として扱います。温度計画は、同じ目的関数で継続するSearchの間に保持されます。

標準数値変更では、固定変数を除いた一変数を選びます。実数は変更幅と正規乱数の積で動かします。Searchでcauchyが有効なら、別の正規乱数の絶対値で割ることで、ときどき大きい変更を含めます。整数は変更量を丸め、カテゴリは別のIDを選びます。Searchの範囲外提案は棄却します。

Searchで座標変更が棄却され、次も同じ鎖の番なら、一度だけ反対方向を試します。これは最良値を探すための工夫です。Warmup・Sampleではこの処理を使いません。

### 7.4 変更幅の適応

adaptが有効な場合、SearchとWarmupでは座標ごとに32回の試行をまとめ、受理率に応じて変更幅を調整します。受理率を$a$、変更幅を$s$とすると、更新は$s\leftarrow s\exp(a-0.44)$です。ここで$a$はその32回での受理割合で、未知のモデル係数とは別の量です。

受理率が高ければ広げ、低ければ狭めます。幅は初期幅の$10^{-9}$倍から1000倍の範囲に制限します。カテゴリにはこの幅適応を行いません。整数の丸めによる自己遷移は評価を省きますが、幅の適応上は元の自己遷移に対応する受理として扱います。

Sampleでは幅を変更しません。モデル改訂時には途中の試行カウンタなどをリセットしますが、学習済み幅は保持します。Sessionのrestartでも数値変更幅は保持されるため、完全に同じ新規状態から幅も初期化したいならSessionを新しく作ります。

### 7.5 Warmup・Sampleの境界と離散変数

実数の通常座標変更は正規分布の幅を使います。有限の上下限がある場合、外へ出た値を区間で反射します。片側だけ有限の場合もその境界で反射します。Searchや集団提案では反射せず、範囲外を棄却します。

整数は正規乱数による変更量を丸めます。0に丸まった場合は90%を符号に応じた1または-1へ変え、残りには自己遷移を残します。範囲外は棄却します。可動変数が全て離散なら、Warmup・Sampleでは各鎖ごとに変数を巡回します。実数が含まれる場合の通常座標選択はランダムです。

カテゴリは、Warmup・Sampleでは90%の確率で現在と異なるID、10%では現在を含む全IDから選びます。自己遷移の可能性を残すためです。実際に同じIDだった場合は評価を省き、同じ状態にとどまる遷移として数えます。

全変数が固定なら新しい評価を繰り返しません。遷移数と標本記録は進むので、固定された状態の平均や分散を取ることはできます。

### 7.6 複数の鎖を使う実数変更

伸縮提案では、対象以外の一鎖を選び、その点を中心として実数部分を伸縮します。現在の実数ベクトルを$x$、相手を$y$、伸縮率を$z$とすると、候補は$y+z(x-y)$です。$z$は$1/2$以上2未満から、密度が$1/\sqrt z$に比例するように選びます。

可動実数の次元数を$d$とすると、提案比の対数として$(d-1)\log z$を返します。固定変数や離散変数はこの次元数に含めません。範囲外は棄却します。

差分提案では、対象以外の異なる二鎖を順序付きで選び、その実数部分を$y_1,y_2$とすると、候補は次の形です。

$$x'=x+\gamma(y_1-y_2)+\eta,\qquad \gamma=\frac{2.38}{\sqrt{2d}}$$

$x'$は候補、$d$は可動実数の次元数、$\eta$は座標ごとに現在の変更幅の0.001倍の標準偏差を持つ正規ノイズです。二鎖を逆の順序でも同じ確率で選ぶので、この提案比の対数は0です。これも範囲外は棄却します。

集団提案は、残りの鎖を固定したまま対象の一鎖だけを更新します。複数鎖の標本が互いに独立になるという意味ではありません。設定確率に該当しても、必要な鎖数や可動実数がない場合は通常の座標変更を行います。

### 7.7 観測評価、差分再評価、中断

ObservationModelは事前項を一度、その後に各観測の予測と損失を順番に計算します。標準Gaussianは同じscaleの対数を一つ保持し、直前と同じ尺度なら対数計算を再利用します。

観測モデルの評価中には、次に計算する項番号と部分和を保持します。指定した項数の間隔で予算を確認し、中断後は同じ目的関数で未計算の項から続けます。候補状態、提案比、受理判定用乱数、候補を作ったときの温度も保持するので、中断のために別の提案を作り直す必要はありません。独自の全体評価やcandidateは一回の呼び出し単位となり、その内部の部分計算は管理しません。

観測の追加は、以前の評価が全鎖で揃い、有限候補があり、既存の項が変わらない場合に、保存energyへ追加項を足す方法を使います。全置換やContext変更などでは全体を再評価します。以前の最良候補も優先して再評価します。追加を複数回まとめて登録でき、既に途中計算がある状態に別種の変更を登録した場合は、その変更と矛盾する部分和を捨てます。

モデルを変えない再開では、未完了の評価や未出力標本を利用できます。モデルの変更では、それらは古い目的関数に属するため無効になります。実際の計算を開始できない別フェーズへの呼び出しだけで、保留中の試行や標本を切り替え・破棄することはしません。

### 7.8 標本記録と逐次集計

Sampleの間隔は鎖ごとに数えます。棄却も一回の遷移として数え、記録タイミングなら現在の状態を出力します。遷移が終わっても期限のため出力できない場合は保留し、同じSampleを続けたときに出力します。

MomentsはWelford法で、個数・平均・偏差平方和を逐次更新します。標本数を$n$とすると、2件以上で偏差平方和を$n-1$で割った標本分散を返します。doubleは一成分、arrayとvectorは成分ごとに処理します。元の標本は保持しません。

この計算は記録された列の平均と分散です。MCMCの自己相関を補正して平均の標準誤差を求める処理は含みません。非線形の予測では、先に状態ごとの予測をprojectやemitで計算してから集計します。

### 7.9 計算量と実装範囲

鎖数を$C$、状態のコピー量を$D$、観測数を$N$、完了遷移数を$m$、一回の全体評価時間を$F$、一回の提案作成時間を$P$とします。通常の実行時間の目安は$O(m(D+P+F))$に出力時間を加えたものです。観測モデルの$F$は各行の予測・損失の合計なので、観測が増えるほど一提案の費用が増えます。

初期評価はおおむね$C$回、モデル更新の全体再評価は古い最良候補も含め最大$C+1$回の評価です。観測追加では再利用可能な場合に追加項だけを計算します。候補差分評価は別の拡張点で、U14のように利用者が依存構造を与えます。ライブラリが任意関数の依存関係を自動解析することはありません。

状態保持にはおおむね$O(CD)$、そのほかモデルの観測・Contextと最良・作業状態が必要です。標本の逐次集計は投影先の成分数に比例する記憶量です。emitで全標本を外部へ保存すれば、その分の記憶量が増えます。

内部の通常の浮動小数点計算はdoubleです。long doubleや`__int128`をホットパスの標準型にせず、単スレッド、コピー可能で固定した形の状態を前提にしています。ヘッダを通常のincludeで利用する場合、末尾の単体実行用テストは取り込まれません。
