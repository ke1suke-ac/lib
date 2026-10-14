# LinearEstimator サンプルコード集

対象ヘッダーは `linear_estimator_v09.hpp` です。既知の一次式から未知の値を推定する使い方を、公開APIの一覧と21本の独立した実行例で説明します。各例の変数IDは0始まりです。数学の専門用語は、その場で意味を補います。

コードは各例だけをコピーして使えます。`main` がそれぞれにあるため、複数の例を1本へ連結せず、別々にコンパイルしてください。ヘッダーの配置だけは必要です。配布ZIPにはヘッダー・全 `.cpp`・入力例・実行結果・検証スクリプトを収録しています。

本書はソースに合わせて自動更新する文書ではありません。更新は明示的に依頼されたときに行います。

## 1. 何を解くライブラリか？

### 1.1 合計や差から、直接見えない値を求める

たとえば、3個の荷物の重さをそれぞれ量れず、「A+B」「B+C」「A+C」の合計だけを量れるとします。各測定には多少の誤差もあります。このとき、測定結果とできるだけ矛盾しないA・B・Cの重さを求めたい、という問題を扱います。

足し算だけでなく、「起動時間＋処理個数×1個当たり時間」「左のグループの合計−右のグループの合計」など、**未知の値を定数倍して足した式**を使えます。式の係数と定数項は利用者が指定し、式に入る未知変数を推定します。通常の回帰でいう未知の係数も、本ライブラリでは未知変数です。

未知変数ごとに、実数の範囲、整数の範囲、有限の候補集合、既知の固定値を指定できます。観測のほかにも「この値は50付近らしい」「隣同士の差は小さそう」「合計は必ず12」といった情報を同じ形で加えます。

### 1.2 目的関数

各観測や条件を、一次式とその式を評価するコストの組として登録します。この組を**因子**と呼びます。

$$z_ {r}(x)=b_ {r}+\sum_ {j=0}^{n-1}a_ {rj}x_ {j}$$

$$E(x)=\sum_ {r\in A}\ell_ {r}\bigl(z_ {r}(x)\bigr),\qquad x_ {j}\in D_ {j}\quad(0\leq j\lt n)$$

$$x^{\ast}\in\mathrm{arg\,min}_ {x_ {j}\in D_ {j}}E(x)$$

| 記号 | 意味 |
|---|---|
| $n$ | 未知変数・固定変数を合わせた個数。1以上。コンストラクタへ渡す値域列の長さ |
| $j$ | 変数ID。0から $n-1$ まで |
| $x$ | 変数の値を全部並べた候補。具体例では荷物3個の重量の組 |
| $x_ {j}$ | 変数jの値。整数や候補集合の場合もAPI上は `double` で保持する |
| $D_ {j}$ | 変数jの取り得る値の集合。`Domain` で指定する |
| $r$ | 因子ID。`add_factor` の戻り値。登録順に0から付く |
| $A$ | 有効になっている因子IDの集合。無効化した因子は合計に含めない |
| $a_ {rj}$ | 因子rの式で、変数jに掛ける既知係数。式に含めない変数の係数は0 |
| $b_ {r}$ | 因子rの既知の定数項。`LinearForm::offset` |
| $z_ {r}(x)$ | 候補xから計算した一次式の値。たとえば候補から予測した合計重量 |
| $\ell_ {r}(z)$ | 一次式の値zに対する因子rのコスト。`Loss` または独自関数で定める |
| $E(x)$ | 有効な因子のコストの合計。小さい候補を良いとみなす |
| $x^{\ast}$ | 数学上の最小コストの候補。`arg min` は「最小値を取る候補の集合」を表す。ライブラリの返す候補が必ずこの集合に入る保証ではない |
| $\sum$ | 指定範囲の項を足す記号。$r\in A$ は有効因子だけを足すという指定 |

Gaussianを使う観測因子なら、具体的には次のコストになります。

$$\ell_ {r}(z)=\frac12\left(\frac{z-y_ {r}}{\sigma_ {r}}\right)^2$$

ここで $y_ {r}$ は観測値、$\sigma_ {r}>0$ は観測ノイズの標準偏差です。z、観測値、標準偏差には同じ単位を使います。予測値と観測値の差を、誤差の典型的な大きさで割って二乗しています。したがって、誤差が小さい測定ほど強い情報として働きます。$1/2$ はこのライブラリでのコストの定義です。

たとえば予測101g、観測105g、標準偏差2gなら、差は標準偏差の2倍なのでコストは2です。予測が観測と一致すれば0です。「荷物Aは50g付近、標準偏差20g」という事前情報も、一次式をA単体にして同じ方法で表せます。

絶対に禁止する条件では、条件内を0、条件外を $+\infty$、つまり正の無限大のコストにします。「できれば守る」条件は違反時も有限のコストにします。値域外も許されません。実行可能な候補とは、各値域に入り、全有効因子のコストが有限な候補です。

### 1.3 一つの候補を選ぶか、不確かさも調べるか

`optimize()` は合計コストの小さい候補を探します。結果は `best()` で取得します。整数やカタログ候補をそのまま採用したいとき、具体的な設定値を決めたいときに使います。

`sample()` は、コストの小さい候補ほど現れやすい分布から標本を集めます。その目標の分布は次の形です。

$$p(x)\propto\exp\bigl(-E(x)\bigr)$$

ここで $p(x)$ は候補の確率、連続値では確率密度です。$\exp(t)$ は自然対数の底をt乗する関数で、$\propto$ は「全体で確率1になるよう共通の倍率を掛けると等しい」という意味です。コストが1大きい候補の相対的な重みは約0.368倍になります。離散候補は各候補を一つずつ数え、連続変数は通常の長さ・面積などに対して密度を考えます。

標本から平均、分散、候補の出現割合、しきい値を超える割合を調べられます。分散は「平均からのずれの二乗」の平均で、平方根を取ると元の値と同じ単位の標準偏差になります。ただし、標本数が多いだけで推定が十分に正確だとは限りません。

観測コストを「確率または確率密度の負の対数」にすると、観測に基づく確率推定として読めます。任意の意思決定コストを入れることもできますが、その場合の標本分布が現実の不確かさを自動的に表すわけではありません。`sample()` は全体を確率1へ正規化できるモデルを前提とします。無制限の実数に情報を一切与えないモデルなどは、その前提を満たしません。

### 1.4 対象外になる問題の見分け方

一つの因子が見られるのは、**一つの既知の一次式の値**です。その値への非線形なコストはcustomで書けますが、未知変数の積や任意の複数変数関数を直接渡すAPIではありません。未知のノイズ尺度と平均を同時に自由に推定する一般的なモデルも、そのままの構成では表せません。

グラフを扱う例では、グラフそのものを渡すのではなく、辺や頂点に変数IDを割り当て、経路の和や隣接差を登録します。経路生成、最短経路、観測を選ぶ戦略、コンテストの入出力プロトコルは利用側の仕事です。

## 2. APIリファレンス

### 2.1 ビルド、型、読み方

このヘッダーはC++20以上、GCC/libstdc++系の `bits/stdc++.h` が使える環境を前提とします。外部ライブラリへのリンクは不要です。標準ライブラリの `std::span` を使うため、C++17ではコンパイルできません。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -I. examples/01_group_weights.cpp -o sample
./sample < input/01_group_weights.txt
```

単独でコピーするときは、コードを `main.cpp`、ヘッダーを同じフォルダへ置き、`g++ -std=c++20 -O2 main.cpp -o sample` でビルドできます。表では `E` を `LinearEstimator<CustomLoss>` の省略名として使います。標準Lossだけなら `LinearEstimator<>` です。`Domain`、`Loss`、`Runtime` などはすべて `E` の内側の公開型です。異なる `CustomLoss` の推定器では入れ子型も別の型になるため、混ぜないでください。

**このライブラリに `solve()` というメソッドはありません。実行入口は `optimize()` と `sample()` です。** 以下の引数で「必須」としたものに省略値はありません。引数なしで呼べるものも含め、明示的な公開APIを列挙します。

### 2.2 テンプレートとコンストラクタ

`template<class CustomLoss = std::nullptr_t> class LinearEstimator`

`explicit LinearEstimator(std::vector<Domain> domains, uint64_t seed = 0, CustomLoss custom = {})`

| 項目 | 型・デフォルト | 意味・範囲 |
|---|---|---|
| `CustomLoss` | 型。既定 `std::nullptr_t` | 標準Lossだけなら省略。独自Lossを使うなら後述の呼出しを受けられる関数オブジェクト型 |
| `domains` | `std::vector<Domain>`、必須 | 空でない値域列。長さが変数数、位置が変数ID。4種類の値域を混在可能 |
| `seed` | `uint64_t`、`0` | 乱数の初期値。型の範囲内の任意の値。再現比較には同じモデル・操作列と固定試行予算を組み合わせる |
| `custom` | `CustomLoss`、`{}` | 評価器を値として受け取り所有する。既定構築できない評価器では実体を渡す |

初期状態は各値域へ0を射影した値です。範囲外の0は端点へ、候補集合では0に近い候補へ寄せます。観測や事前情報を追加しても、その平均が自動的に初期値に設定されるわけではありません。変数数は構築後に変えられません。乱数seedの再設定APIもありません。

### 2.3 `Domain`：変数の値域

| `Domain::Kind` | 意味 |
|---|---|
| `Real` | 指定区間内の有限な実数。両端を含む。無限の端点は「制限しない」という意味で、変数値に無限大を許す意味ではない |
| `Integer` | 指定両端を含む整数。小数値は許さない |
| `Values` | 指定した有限候補のいずれか。値の間を補間して選ぶことはない |
| `Fixed` | 指定した既知値のみ。探索で動かす変数には数えないが、状態ベクトルには含まれる |

| API | 引数・許容値・デフォルト | 戻り値 |
|---|---|---|
| `static Domain real(double lo=-INFINITY, double hi=INFINITY)` | `lo < hi`。片側・両側無制限も可。NaN不可。一点にしたいときは `fixed` | 実数値域 |
| `static Domain integer(int64_t lo, int64_t hi)` | 両引数必須、`lo <= hi`。両端の絶対値は $2^{50}$ 未満 | 整数値域。同じ端点ならFixed |
| `static Domain values(std::vector<double> v)` | 必須。空でなく、全要素が有限。整列と重複除去を行う | 有限候補の値域。重複除去後に1個ならFixed |
| `static Domain fixed(double x)` | 必須。有限の値 | 固定値域 |
| `bool contains(double x) const` | 必須。調べたい数。NaN・無限大は常にfalse | 値域に含まれるならtrue。モデル全体の制約は見ない |

公開フィールドは次のとおりです。通常は上記生成関数を使って整合した値域を作ります。フィールドを直接書き換える場合も、各Kindの整合性は利用側の責任です。

整数・固定値・候補値の判定に自動の許容誤差は入りません。たとえば整数1を意図した値が1.0000001になっていればIntegerには含まれません。利用側で必要な丸めや変換を済ませてから `set_state` などへ渡します。

| フィールド | 型 | `Domain{}` の初期値 | 意味 |
|---|---|---|---|
| `kind` | `Kind` | `Real` | 値域の種類 |
| `lower` | `double` | `-INFINITY` | 下端。Valuesでは最小候補、Fixedでは固定値 |
| `upper` | `double` | `INFINITY` | 上端。Valuesでは最大候補、Fixedでは固定値 |
| `candidates` | `std::vector<double>` | 空 | Valuesの昇順・重複なしの候補列。それ以外では空にする |

### 2.4 `Term` と `LinearForm`：既知の一次式

| 型・フィールド | 型・デフォルト | 意味・範囲 |
|---|---|---|
| `Term::variable` | `int`、メンバー初期値なし | 0以上、変数数未満のID。必ず値を指定する |
| `Term::coefficient` | `double`、メンバー初期値なし | その変数に掛ける有限の係数。負・0も可 |
| `LinearForm::terms` | `std::vector<Term>`、空 | 係数付きの変数列。空なら定数式 |
| `LinearForm::offset` | `double`、`0` | 有限の既知定数項 |

たとえば `E::LinearForm{{{0, 2}, {3, -1}}, 5}` は「変数0の2倍−変数3＋5」です。`Term` は `{variable, coefficient}` で初期化します。`Term t;` と宣言しただけでは二つの数値は初期化されません。

| API | 必須引数 | 戻り値・注意 |
|---|---|---|
| `static LinearForm sum(std::span<const int> indices)` | 変数IDの列 | 各IDの係数1、offset=0の和。重複IDはその回数分数える |
| `static LinearForm difference(std::span<const int> left, std::span<const int> right)` | 左右のID列 | 左の和−右の和、offset=0。両側に同じIDがあれば差し引かれる |
| `double evaluate(std::span<const double> x) const` | 参照する全IDの位置を持つ値列 | 一次式の値。値域や因子コストを調べるAPIではない |

`std::vector<int>` や `std::array<int,N>` はID列として渡せます。裸の `{0,1}` をspanへ渡すことに頼らず、名前付きの配列を用意すると明確です。一次式をモデルへ追加・置換するとき、同じ変数の項を合算して係数0の項を除きます。元の `LinearForm` の配列が利用側で自動的に整列されるわけではありません。

### 2.5 `Loss`：一次式に対するコスト

全生成関数は `static Loss` を返し、引数はすべて必須です。観測値y、尺度s・sigma、式の値zは同じ単位にします。yは有限、尺度は正の有限値です。区間はNaN不可で、`lo <= hi`、`lo < INFINITY`、`hi > -INFINITY` が必要です。`GaussianInterval` だけはさらに `lo < hi` が必要です。

以下で $t=(z-y)/s$ は尺度で割ったずれ、$\nu$ は正の自由度、$d(z,[lo,hi])$ は区間までの距離です。距離は区間内なら0、下ならlo−z、上ならz−hiです。$\log$ は自然対数です。

| `Loss::Kind` | 生成API | コストと使いどころ |
|---|---|---|
| `Gaussian` | `gaussian(double y, double sigma)` | $\tfrac12((z-y)/\sigma)^2$。標準的な正規ノイズ、または二乗誤差の罰点。sigmaは標準偏差 |
| `Laplace` | `laplace(double y, double s)` | $\lvert(z-y)/s\rvert$。ずれの絶対値。大きいずれをGaussianほど強く罰しない。観測分布として使うと標準偏差は $\sqrt{2}s$ |
| `StudentT` | `student_t(double y, double s, double nu)` | $\tfrac{\nu+1}{2}\log(1+t^2/\nu)$。大きなずれを緩やかに罰する。nuは正の有限値で省略不可。nu>2なら観測分布の標準偏差は $s\sqrt{\nu/(\nu-2)}$。sは一般には標準偏差ではない |
| `GaussianInterval` | `gaussian_interval(double lo, double hi, double sigma)` | **zに正規ノイズを足した値**が区間に入る確率の負の対数。丸め・上限表示・符号だけの観測に使う。真値zを区間内へ固定する制約ではない |
| `HardInterval` | `hard_interval(double lo, double hi)` | **ノイズを足さないz自身**が区間内なら0、外なら正の無限大。等しい両端も可。ただし自由な実数の厳密等式は変数消去で扱う |
| `SoftInterval` | `soft_interval(double lo, double hi, double s)` | $\tfrac12(d(z,[lo,hi])/s)^2$。外れても候補として認める希望範囲。等しい両端も可 |
| `Custom` | `custom(int id)` | 推定器が所有する評価器に `(id,z)` を渡した戻り値。idは利用者が意味を定める任意のintで、factor IDとは別 |

`GaussianInterval` を式で書くと $-\log(F((hi-z)/\sigma)-F((lo-z)/\sigma))$ です。Fは「平均0、標準偏差1の正規ノイズが、引数以下になる確率」を返す関数です。負の無限大で0、正の無限大で1になります。確率の大きい区間観測ほどコストが小さくなると読めれば十分です。標準正規分布を自分で実装する必要はありません。

`double Loss::evaluate(double z) const` は、標準Loss単体のコストを返す公開APIです。引数は必須で、通常は有限のzを渡します。**Customには使えません**。評価器はLoss単体でなく推定器が持つためです。customを含む全体コストは `model.evaluate(values)` を使います。

公開フィールドと初期値は次のとおりです。使わないフィールドは選んだ種類で参照されませんが、通常は生成関数で作り、Loss全体を `set_loss` へ渡します。

| フィールド | 型 | `Loss{}` の初期値 | 使用する種類・意味 |
|---|---|---|---|
| `kind` | `Kind` | `Gaussian` | 上表の種類 |
| `target` | `double` | `0` | Gaussian・Laplace・StudentTの中心y |
| `scale` | `double` | `1` | Gaussian・GaussianIntervalのsigma、Laplace・StudentT・SoftIntervalのs。正の有限値 |
| `lower` | `double` | `-INFINITY` | 3種類のIntervalの下端 |
| `upper` | `double` | `INFINITY` | 3種類のIntervalの上端 |
| `df` | `double` | `1` | StudentTの自由度nu。正の有限値 |
| `custom_id` | `int` | `0` | Customの評価器へ渡すID |

汎用の `weight` フィールドはありません。正の重みw倍にしたい場合、Gaussianではsigmaを `sigma/sqrt(w)`、Laplaceではsを `s/w` にすると同じ倍率になります。一般のコスト倍率はcustomで表します。測定値を重複登録すれば二重に数えるので、同じデータを誤って重ねないでください。

### 2.6 独自評価器の契約

評価器は `double operator()(int custom_id, double z) const` に相当する呼出しを受け付ける必要があります。`model.evaluate` もconstなので、評価器をconstとして呼べるようにします。引数zは、登録した一次式の計算結果です。全変数の配列や他の因子の値が渡されるわけではありません。

| 項目 | 必要な条件 |
|---|---|
| 戻り値 | この1因子の有限コスト、または禁止を表す `INFINITY`。有限の負コストも可。NaNと負の無限大は禁止 |
| 安定性 | モデルを更新しない間、同じIDとzに対して同じコスト。評価のたびに乱数を引いたり、時刻で値を変えたりしない |
| ID | 利用者が表の添字などへ対応付ける。範囲の管理は利用側で行う |
| 所有 | 評価器はモデルが所有する。参照・ポインタを持たせるなら、その参照先の寿命を確保する |
| 更新 | 外部データを変えた場合は、影響するすべてのfactor IDへ `set_loss(id, Loss::custom(custom_id))` を通知。その間に解を読んだり実行したりしない |
| 計算時間 | 評価器の途中では時間制限で停止できない。1回の評価を軽く保つ |
| 禁止領域 | customの正の無限大を一般に自動修復できない。全custom因子が有限になる初期値を用意する |

コストを設計する最初の一歩は、「どの予測を良いと考えるか」を決めることです。不足だけ重く罰したいなら不足側だけ倍率を増やします。候補の事前確率pを反映するなら `-log(p)` を返します。観測の確率密度を使う場合、未知のzによって幅が変わるときの正規化項を落とさないでください。例12では `log(2*r*z)` が必要です。

### 2.7 因子・値域・初期状態の登録と変更

以下の引数はすべて必須です。`id` は登録済み因子のint ID、`j` は0以上変数数未満のint IDです。メソッドが返したIDと利用者のcustom IDは別の番号です。

| メソッド | パラメータ | 戻り値・動作 |
|---|---|---|
| `add_factor(LinearForm form, Loss loss)` | 一次式とコスト | `int`の新しい安定したfactor ID。有効な因子として追加する |
| `set_loss(int id, Loss loss)` | 既存IDと新コスト | `void`。一次式を保って観測値・尺度・種類などを変更。同じLossでも更新通知になる |
| `replace_factor(int id, LinearForm form, Loss loss)` | 既存ID、新一次式、新コスト | `void`。IDと有効/無効状態を保って置換。逆引き関係を作り直すため、項1個の差替えでも単純な定数時間ではない |
| `set_enabled(int id, bool enabled)` | trueで有効、falseで無効 | `void`。因子は残る。同じ有効状態を指定した場合は何もしない |
| `set_domain(int j, Domain domain)` | 変数IDと新値域 | `void`。種類の変更も可能。現在・最良候補を新値域へ射影し、関係因子を再評価。新しい全制約を必ず満たす保証はない |
| `set_state(std::span<const double> values)` | **全変数数と同じ長さ**の値列。各値は指定値域内 | `void`。現在・最良候補をこの値へ置換。ハード因子違反は修復候補として保持できる。乱数を初期化する操作ではない |
| `set_directions(std::vector<std::vector<Term>> directions)` | 固定の同時変更方向の集合 | `void`。既存の利用者方向を全部置換。空の集合なら利用者方向を解除 |

方向の例 `{{0,1},{1,-1}}` は、ある移動量tで「変数0にtを足し、変数1からtを引く」という指定です。各方向は重複項をまとめた後に空でない必要があり、全項がReal、または全項がIntegerでなければいけません。Integerでは方向係数も整数にします。Values・Fixedを含めることはできません。整数係数に共通の整数倍がある場合は内部で取り除かれます。ドメイン変更で使えなくなった方向は準備時に除外されるので、必要に応じて再指定します。

後から変更できないのは、変数数・変数IDの並び、乱数seed、評価器の型・所有する評価器オブジェクトそのものです。これらは新しい推定器を構築します。因子の物理削除APIもないため、無効化またはIDの再利用を行います。

同じモデルを保持することで、現在・最良候補や利用可能な計算済み情報を引き継げます。Gaussianの観測値だけを `set_loss` で変え、式とsigmaを保った場合は、既存の一次式の値や利用可能な分解を再利用できます。全変更が常に差分だけの定数時間で済むわけではありません。

### 2.8 `OptimizeParam`・`SampleParam` の全設定

#### 共通フィールド

| フィールド | 型 | 既定値 | 許容値・意味・調整方法 |
|---|---|---|---|
| `time_limit_us` | `int64_t` | `2000` | −1または0以上。今回の呼出しのマイクロ秒予算。−1は時間上限なし。1秒は1000000µs |
| `max_steps` | `int64_t` | `-1` | −1または0以上。今回の更新試行数上限。−1は回数上限なし。方式によって1試行の仕事量は異なる |
| `method` | `Method` | `Automatic` | 次節の列挙値。まず既定値を使い、明示方式は比較したいときに使う |
| `auto_directions` | `bool` | `true` | 自動の同時変更方向を用意するか。利用者方向とは独立。Automaticの有限候補ペア更新にも関係する |
| `direction_probability` | `double` | `0.25` | 0〜1。用意された同時方向を選ぶ確率。Automaticの有限候補ペアを試す確率にも使う。方向がなければ単独更新になる |
| `step_scale` | `double` | `1` | 正の有限値を指定。局所提案・sliceの幅の倍率。0.5・1・2などを比較する出発点にできる。目的分布の温度や正規条件付き分布の分散を任意に変える設定ではない |
| `enumeration_limit` | `int` | `32` | 0以上。小さい離散候補の全列挙上限。有限候補ペアでは候補数の積に適用。0はこの列挙を無効にする。増やすと1試行が重くなる |

二つの予算を**両方−1にしてはいけません**。どちらかが0なら実行を進めません。再現比較は `time_limit_us=-1` と正の `max_steps` を使います。実戦では `time_limit_us` を正にし、全体の残り時間から余裕を引きます。時間制限は協調的なチェックであり、締切時刻ぴったりでの停止は保証されません。customやhookの途中では停止できません。

#### `OptimizeParam` 固有フィールド

| フィールド | 型 | 既定値 | 許容値・意味・選び方 |
|---|---|---|---|
| `temperature_start` | `double` | `1` | 0以上の有限値。探索開始側の温度。コスト差に対して大きいと悪化も試しやすい |
| `temperature_end` | `double` | `0.01` | 0以上の有限値。終了側の温度。開始温度以下でなければならないという制約はない |
| `greedy_probability` | `double` | `0.25` | 0〜1。温度0の改善優先更新を混ぜる確率。1なら通常更新も改善優先になる |

両温度が正なら比率を使って徐々に変化します。一方でも0なら通常更新の温度も0になります。正の `max_steps` があれば温度の進み方は回数を基準にするので、時間制限が先に来ると終端温度へ達しないことがあります。温度計画は呼出しごとに始まります。Gaussianの専用経路が使われる場合、これらの温度設定は使いません。

#### `SampleParam` 固有フィールド

| フィールド | 型 | 既定値 | 許容値・意味・選び方 |
|---|---|---|---|
| `warmup_steps` | `int64_t` | `128` | 0以上。採取に含めない初期更新数。変数数・相関・初期値に応じて調整し、採取の予算も残す。独立な正規生成ではwarmup不要として扱う |
| `sample_stride` | `int` | `0` | 0なら通常は可動変数数ごと、独立な正規生成では毎回採取。1以上なら指定試行数ごとに採取。大きくするだけで収束するわけではない |
| `collect_moments` | `bool` | `true` | 各変数の平均・分散を蓄積するか。falseでも採取hookや保存は行える |
| `sample_capacity` | `int` | `0` | 0以上。直近何組の全体標本を保存するか。0なら保存しない。`predict` と `stored_sample` を使うには正の容量が必要 |

採取時の目標温度は1で、`SampleParam` に温度フィールドはありません。warmup中や制約修復中は標本を集計しません。全変数Fixedなら標本は生成されず、`has_solution()==true` でも統計は空です。

### 2.9 `Method`：更新方式

| 列挙値 | 意味と注意 |
|---|---|
| `Automatic` | モデルの形に応じて正規モデルの専用計算、離散候補列挙、条件付き提案、sliceなどを使い分ける通常の設定。常に同じ手法という意味ではない |
| `Metropolis` | 対称な局所提案を試す基本経路。ハード因子による事前の区間絞り込みや条件付き候補の全列挙を使わない。制約の多い問題では棄却が多くなり得る |
| `Conditional` | 小さい離散候補の列挙や、正規部分を用いた条件付き提案を優先。使えないときは局所提案へ移る。全てのモデルで正確な条件付き分布を直接生成する指定ではない |
| `Slice` | 非正規の連続方向でsliceを利用する。正規方向・整数・有限候補まで必ずsliceになるわけではない。最適化の温度0更新でもslice採取にはならない |

Automatic以外ではモデル全体の正規専用経路やAutomatic固有の初期化を使いません。`auto_directions=true` なら明示方式でも同時方向の準備は行います。方式を変えて比較する際は、同じ試行数だけでなく同じ時間に対する品質も確かめます。

### 2.10 実行APIとhook

| API | 引数・デフォルト | 戻り値 |
|---|---|---|
| `template<class DebugHook=std::nullptr_t> Runtime optimize(const OptimizeParam& param={}, DebugHook debug_hook={})` | 設定省略は前節の全既定値。hook省略は無効 | 今回の `Runtime`。解配列は含まれない |
| `template<class SampleHook=std::nullptr_t, class DebugHook=std::nullptr_t> Runtime sample(const SampleParam& param={}, SampleHook sample_hook={}, DebugHook debug_hook={})` | 設定・両hookとも省略可 | 今回の `Runtime`。統計・保存標本は別APIから取得 |

hookは関数オブジェクトを値で受け取ります。外部の集計値へ結果を書き出すlambdaは、必要な変数を参照キャプチャします。呼出しは同期的で、実行が戻る前に行われます。

| hook | 受け取る引数 | 呼ばれる場面・制約 |
|---|---|---|
| sample hook | `const SampleView&` を受けられる関数 | 採取された標本ごと。warmup中・修復中は呼ばれない。標準統計と保存の更新後に呼ばれる。`collect_moments=false` でも使用可能 |
| debug hook | `const Runtime&` を受けられる関数 | 進捗チェック時と終了時。各更新試行ごとではない。予算0の呼出しでも終了時に呼ばれる |

戻り値は使われないので通常 `void` にします。boolを返して停止を要求する契約ではありません。hook内でモデルを変更・再実行しないでください。`SampleView` は借用なので後へ保持せず、必要な値をコピーします。hookの処理時間は実行時間に含まれ、重い出力は採取数を減らします。sample hookを省いてdebug hookだけ渡すなら `model.sample(p, nullptr, debug)` です。

### 2.11 `Runtime` と `StopReason`

| `Runtime` のフィールド | 型 | 初期値 | 意味 |
|---|---|---|---|
| `stop_reason` | `StopReason` | `StepLimit` | 今回の停止理由。下表を参照 |
| `elapsed_us` | `int64_t` | `0` | 今回の経過マイクロ秒。準備やhookの費用を含む。debug hook内で渡される値には、そのhook自身の実行時間はまだ入っていない |
| `steps` | `int64_t` | `0` | 今回の更新試行数。棄却・無変更も含み得る。修復や専用経路の反復も数える |
| `accepted` | `int64_t` | `0` | 今回の通常更新で受理・実行された更新数。専用経路では意味が異なり、修復回数は別フィールド。単純な品質指標ではない |
| `samples` | `int64_t` | `0` | 今回採取した標本数。`optimize` では0。保存容量や標準統計の有効/無効とは独立 |
| `repair_steps` | `int64_t` | `0` | 今回のハード制約修復の試行数。stepsにも含まれる |
| `warming_up` | `bool` | `false` | `sample` の終了時点で通常のwarmupが残っているか。最適化の進捗率ではない |

| `StopReason` | 意味 | 解・統計を読む際の注意 |
|---|---|---|
| `TimeLimit` | 時間予算0、または時間チェックで上限に到達 | 途中までの良い候補があることもある。解の有無は `has_solution`、統計はcountで確認 |
| `StepLimit` | 試行数予算などで終了。通常ループの終了時の既定理由でもある | 一部の専用経路の内部反復上限や収束未確認の終了も含み、stepsが必ずmax_stepsと同じとは限らない |
| `NoMovableVariable` | 動かす変数がない。典型例は全変数Fixed | 有効な固定候補は存在し得るが、標本は生成されない |
| `NoFeasibleState` | 終了時に実行可能候補を保持していない | 他の停止事情があっても最終的にこの値が優先される。不可能性の証明ではない。best/currentを呼ばない |
| `Solved` | Gaussian専用計算で解いた、またはその収束条件を満たした | 一般の離散・custom問題の大域最適性の証明を意味しない。通常探索の最適解検知フラグでもない |

### 2.12 候補の取得と全体評価

| API | 引数・戻り値 | 条件と意味 |
|---|---|---|
| `bool has_solution() const` | 引数なし、bool | 現行モデルで保持する候補が実行可能か。最適性・採取の収束・標本の存在は保証しない |
| `std::span<const double> current() const` | 引数なし、全変数の借用ビュー | `has_solution()==true` が前提。現在の状態。探索や採取では最良候補と異なり得る |
| `std::span<const double> best() const` | 引数なし、全変数の借用ビュー | `has_solution()==true` が前提。現行モデルで保持している最良候補 |
| `double best_energy() const` | 引数なし、有限の合計コスト | `has_solution()==true` が前提。bestの目的関数値。customによっては負にもなる |
| `double evaluate(std::span<const double> values) const` | 必須の全変数数と同じ長さの値列 | 候補全体のコストを再評価。値域・ハード制約・customの禁止領域の違反なら正の無限大。現在の解を変えない。`has_solution()==false` でも使用可能 |

`current`・`best` のビューは次のモデル変更・実行まで有効として扱います。モデル破棄後や次ターンにも必要なら `std::vector<double>` へコピーします。モデル変更後には保持した候補を再評価しますが、過去の全候補を新モデルで探し直す機能ではありません。

### 2.13 統計・保存・予測のAPIと結果型

| API | 引数・戻り値 | 意味 |
|---|---|---|
| `const Moments& moments() const` | 引数なし、借用参照 | 現在の採取区間の各変数の平均・分散。保存容量とは独立 |
| `int stored_sample_count() const` | 引数なし、0以上のint | 現在保存している全体標本数。容量以下 |
| `std::span<const double> stored_sample(int index) const` | 必須。`0 <= index < stored_sample_count()` | 古い順のindex番目の全変数ビュー。次の変更・実行までに読み終える |
| `Prediction predict(const LinearForm& form) const` | 必須の一次式 | **保存中の標本だけ**から式の平均と分散を計算して値で返す。標本がなければcount=0 |

| 型・フィールド | 型 | 初期値 | 意味・有効条件 |
|---|---|---|---|
| `SampleView::values` | `std::span<const double>` | 既定構築なら空 | hookに渡される全変数の借用ビュー。hook終了後へ保持しない |
| `SampleView::energy` | `double` | メンバー初期値なし | その標本の合計コスト。ライブラリから渡されるときは値が設定されている |
| `Moments::count` | `int64_t` | `0` | 標準統計に蓄積した標本数 |
| `Moments::mean` | `std::vector<double>` | 空 | count>0のとき全変数数の平均列 |
| `Moments::variance` | `std::vector<double>` | 空 | count>0のとき全変数数の分散列。割る数はcountでありcount−1ではない |
| `Prediction::count` | `int64_t` | `0` | 予測に使った保存標本数 |
| `Prediction::mean` | `double` | `0` | count>0のとき式の平均 |
| `Prediction::variance` | `double` | `0` | count>0のとき式の分散。割る数はcount |

**countが0なら、平均・分散は使わないでください。** 統計リセット後の `Moments` 配列には以前の値が残り得ます。要素数が空でないことは、有効性の判定になりません。`Prediction` の初期値0も「確率的に完全に分かった」という意味ではありません。

`predict` は同じ標本の変数間の関係を保ちます。独立と分からない変数の分散を手で足す代用は避けます。結果に将来の観測ノイズは含みません。独立な加算正規ノイズの標準偏差がsigmaだと分かっていて次の**観測値**の分散が欲しいなら、潜在値の予測分散へ `sigma*sigma` を足します。非線形な確率・最大値・しきい値超過はsample hookや保存標本を利用側で集計します。

標本平均は整数や候補集合に入る保証がありません。離散の具体的な設定にはbestや標本の一つを使います。保存メモリは概ね `sample_capacity * 変数数 * sizeof(double)` に比例します。

### 2.14 結果構造体のストリーム出力

次の4つは `E` のfriendとして定義され、通常の `cout << result` / `cerr << result` で利用できます。すべて `std::ostream&` を返すので、その後へ `'\n'` などを連結できます。CustomLossを指定した推定器でも使えます。

第1引数は出力先のストリーム、第2引数は表示する結果のconst参照で、両方とも必須です。結果構造体を変更せず、戻り値は渡した出力先そのものです。

| シグネチャ | 出力内容 |
|---|---|
| `std::ostream& operator<<(std::ostream&, const Runtime&)` | 停止理由の名前、経過時間、試行・受理・採取・修復の各数、warmup中かどうか |
| `std::ostream& operator<<(std::ostream&, const SampleView&)` | `values` と `energy` |
| `std::ostream& operator<<(std::ostream&, const Moments&)` | `count`、`mean`、`variance`。count=0なら両統計は `n/a` |
| `std::ostream& operator<<(std::ostream&, const Prediction&)` | `count`、式の `mean`、`variance`。count=0なら両統計は `n/a` |

各配列は**先頭20要素まで**です。超える場合は `... (25 total)` のように省略と全要素数を表示します。自動で改行せず、数値の桁数などは呼出し元のストリーム設定に従います。`best()` や `current()` の戻り値はspanなので、この4つの構造体用演算子の対象ではありません。配列自身を出すなら利用側でループします。

表示例は `Prediction{count=1000, mean=1.98, variance=0.99}` のような形です。実行時間を含むデバッグ表示を、コンテストの規定の標準出力に混ぜないでください。例ではstderrへ出します。

### 2.15 継続・リセットの一覧

| 操作 | 採取進捗、統計、保存列 |
|---|---|
| 同じモデル・同じ採取設定で `sample` を再実行 | 現在値、乱数、warmup、採取間隔、統計、保存列を継続。Runtimeは毎回の呼出し分 |
| `time_limit_us` / `max_steps` だけを変更 | 同じ採取区間を継続 |
| 因子の追加・変更、実際の有効状態変更、値域変更 | 採取進捗・統計・保存列をリセット。旧モデルの標本を混ぜない |
| `set_state` / `set_directions` | 採取区間をリセット。`set_state` はbestも置換する |
| 正の予算で `optimize` | 採取区間をリセット。次のsampleは新しい採取区間になる |
| `method`、`auto_directions`、`direction_probability`、`step_scale`、`enumeration_limit`、`warmup_steps`、`sample_stride` の変更 | 次のsampleで採取区間をリセット |
| `collect_moments` だけ変更 | 標準統計のcountを0へ。採取進捗と保存列は継続 |
| `sample_capacity` だけ変更 | 保存列を空にする。標準統計と採取進捗は継続 |
| どちらかの予算を0にする | 状態・乱数・準備・統計を進めない。終了debug hookは呼ぶ。変更した採取設定の反映は次の正予算呼出し時 |

hook内で自分が集めた配列やカウンタは、ライブラリが自動リセットしません。モデルを変えたら利用側の集計も分けてください。同じLossを毎ターン全因子に再設定すると、不要に統計をリセットします。`set_enabled` で同じboolを指定した場合だけは何もしません。

### 2.16 共通の制約・失敗を避けるための確認

- 単一スレッドで使います。同じモデルの同時実行やhookからの再入を前提としていません。
- 計算は `double` です。整数値と厳密に扱う整数の積和は絶対値 $2^{50}$ 未満を前提とします。有限でも二乗などでオーバーフローする極端な値は避け、単位を調整します。
- 入力条件違反は主に `assert` の対象です。独自の例外をthrowして入力を救済するAPIではありません。assertを無効にしても不正入力が有効になるわけではありません。
- 自由な実数の厳密等式は、例13のように先に変数を消去します。整数の保存則でも、実行可能な初期値と適切な同時方向が必要になり得ます。
- `NoFeasibleState` でbest/currentを呼ばず、統計はcountを確認します。time budgetが短いと準備だけで終了し、標本が0件になることがあります。
- `sample_capacity=0` では `predict` のcountは0です。`collect_moments=true` だけでは予測用の標本は保存されません。
- 採取値は一般に互いに相関があります。標準偏差を単純に `sqrt(count)` で割るだけで、平均推定の信頼性を保証できるわけではありません。
- 観測の独立性、ノイズの尺度、丸め規則、事前情報は利用者がモデル化します。間違った観測モデルから得た小さい分散は、正しい推定の証拠になりません。
- `set_domain` で範囲を広げても過去の射影前の値が復元されるわけではありません。必要なら値域内の状態を利用側で保持し、`set_state` へ渡します。
- 全固定モデルは解があっても標本を生成しません。既知値をそのまま使うか、利用側で退化した分布として処理します。

## 3. ユースケース

### 3.0 サンプルの使い方と用途一覧

各節は「具体的な問題 → 変数・式・コストへの対応 → 入出力 → 完全なコード」の順です。コメントは、他の節を読まずに設定箇所を見つけられるよう、各操作の直前や同じ行に付けています。入力は各節に記した条件を満たす前提で、汎用的な不正入力処理は入れていません。

コード内の予算は説明用の小規模問題に対する設定です。サンプルの実行結果を再現しやすいよう、すべて時間上限を無効にして有限の試行数で区切っています。制約検査をする例19だけは、意図的に予算0の呼出しも行います。実戦では `time_limit_us` をマイクロ秒で指定できます。乱数を使う結果はコンパイラや環境によって末尾桁が異なり得ます。

表示する「実行結果」は、同梱入力で実際に実行して得た標準出力です。平均・割合などは解析的な厳密値ではありません。stderrのデバッグ出力は時間などが変動するため、出力例には混ぜません。

| 節 | 具体的な問題 | 主な機能 |
|---|---|---|
| 3.1 | 重なる荷物の合計から、整数の重量を求める | Domain::integer, LinearForm::sum, Loss::gaussian, optimize, has_solution, best, best_energy, Runtime表示 |
| 3.2 | 起動時間と1個当たりの処理時間を分ける | Domain::real, LinearForm::terms/offset/evaluate, optimize |
| 3.3 | 道路の合計時間から、候補経路の平均と不確かさを比べる | sample, SampleParam, predict, Prediction |
| 3.4 | 勝ち負けだけから、候補の相対的な性能を推定する | LinearForm::difference, Loss::gaussian_interval, moments |
| 3.5 | 丸められ、上限で止まる表示器から長さを推定する | GaussianIntervalの両側・片側区間, Method::Slice |
| 3.6 | 大きな遅延が混ざった測定から、通常の処理時間を求める | Loss::student_t, Loss::laplace, OptimizeParam温度設定 |
| 3.7 | カタログ候補と入手頻度から、部品の組合せを推定する | Domain::values, CustomLoss, Loss::custom, sample hook, collect_moments=false, enumeration_limit |
| 3.8 | 隣接区画の関係から、センサーのない場所の温度を補う | Domain::fixed, グラフの差分因子 |
| 3.9 | 合計個数が厳密に分かる在庫の内訳を推定する | Loss::hard_intervalの整数等式, set_state, set_directions, Method::Conditional, auto_directions, direction_probability |
| 3.10 | 絶対に守る範囲と、できれば守りたい範囲を分ける | Loss::soft_interval, Loss::evaluate, model.evaluate, ハード不等式 |
| 3.11 | 成功・失敗から、投入量と成功確率の関係を推定する | CustomLossの非正規尤度, SampleView, 非線形予測hook |
| 3.12 | 真の長さに比例してぶれる測定を扱う | CustomLossの支持区間, 有限の負コスト, set_state |
| 3.13 | 合計量が正確な2槽の水量を、変数消去して求める | 実数の厳密等式の消去, LinearForm::offset, 温度0 |
| 3.14 | ターンごとの訂正・無効化・固定値化を前の状態から解き直す | set_loss, replace_factor, set_enabled, set_domain, 差分更新と再実行 |
| 3.15 | 採取を分割して続け、しきい値超過率と予測区間を調べる | sample再実行, sample_stride, stored_sample_count/stored_sample, debug hook, Runtime/SampleView/Moments/Prediction表示 |
| 3.16 | グループ検査から、不良品の位置を推定する | Domain::integer(0,1), Laplaceによる個数罰点 |
| 3.17 | 多数の特徴から、影響の小さい係数を0付近へ寄せる | Method::Metropolis, Method::Conditional, step_scale, Laplace事前 |
| 3.18 | 不足を超過より重く罰して、生産量を決める | 最適化用CustomLoss, optimize debug hook |
| 3.19 | 変更前に候補の実行可能性を調べ、古い制約を取り消す | Domain::contains, current, evaluateのinf, StopReason, 予算0 |
| 3.20 | 外部の観測表を共有する独自評価関数を、ターン間で更新する | CustomLossの所有・共有, 全因子へのset_loss通知 |
| 3.21 | 実数・整数・規格候補・既知値を同じ費用式で推定する | Real/Integer/Values/Fixedの混在 |

### 3.1 重なる荷物の合計から、整数の重量を求める

3個の荷物A・B・Cは、それぞれ1g刻みで0〜100gです。個別には量れませんが、A+B、B+C、A+Cの3組を量れます。入力例では105g、98g、101gでした。

未知変数を順に $x_ {0},x_ {1},x_ {2}$ とします。A+Bの予測値は $x_ {0}+x_ {1}$ なので、この式と観測105を結び付けます。残りも同じです。3つのずれを合計して小さくするため、整数の候補の中で測定に合う組を探せます。測定誤差の標準偏差 `sigma` は**組全体の計測**の値です。

入力例の3本を誤差なしとして解くと54、51、47gになり、整数でも矛盾しません。`optimize` が返すのは実行情報であり、重量は `best()` から取得します。

**入力**：`ab bc ac sigma`。3個の合計観測と、正の標準偏差。単位g。

**出力**：A B C の推定重量、続いて合計コスト。

**ファイル名**：`examples/01_group_weights.cpp`

**コード**

```cpp
// 重なる荷物の合計から、整数の重量を求める
// 入力: ab bc ac sigma。3個の合計観測と、正の標準偏差。単位g。
// 出力: A B C の推定重量、続いて合計コスト。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 同じ配布物のヘッダーを読み込む。C++20以上でビルドする。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 独自評価関数を使わない推定器。
using LE = LinearEstimator<>;

int main() {
    double ab, bc, ac, sigma;

    if (!(std::cin >> ab >> bc >> ac >> sigma)) {
        return 0;
    }

    // 変数0=A、1=B、2=C。乱数seedは7。
    LE model(std::vector<LE::Domain>(3, LE::Domain::integer(0, 100)), 7);
    const std::array<int, 2> ids_ab{0, 1}, ids_bc{1, 2}, ids_ac{0, 2};

    // A+Bの観測。sigmaは必ず正。
    model.add_factor(LE::LinearForm::sum(ids_ab), LE::Loss::gaussian(ab, sigma));

    // B+Cも独立な1回の観測として追加。
    model.add_factor(LE::LinearForm::sum(ids_bc), LE::Loss::gaussian(bc, sigma));

    // A+Cの観測を追加。
    model.add_factor(LE::LinearForm::sum(ids_ac), LE::Loss::gaussian(ac, sigma));

    // 省略した設定はライブラリの既定値を使う。
    LE::OptimizeParam p;

    // この例は再現しやすい回数制限。実戦では正のマイクロ秒予算を指定できる。
    p.time_limit_us = -1;

    // 今回の更新試行を最大20000回にする。
    p.max_steps = 20'000;

    // 合計コストの小さい整数候補を探す。
    const auto runtime = model.optimize(p);

    // 実行情報はstderrへ。時間などは実行環境で変わる。
    std::cerr << runtime << '\n';

    // best()の前に、実行可能な候補があるかを確かめる。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 借用ビュー。次のモデル変更・実行をまたぐならvectorへコピーする。
    const auto x = model.best();

    std::cout << std::fixed << std::setprecision(6);
    std::cout << x[0] << ' ' << x[1] << ' ' << x[2] << '\n';

    // 最良候補のコスト。小さいほど観測と整合する。
    std::cout << model.best_energy() << '\n';
}
```

**入力例**

```text
105 98 101 3
```

**実行結果（標準出力）**

```text
54.000000 51.000000 47.000000
-0.000000
```

### 3.2 起動時間と1個当たりの処理時間を分ける

機械で品物を処理すると、「起動のための固定時間」と「個数に比例する時間」がかかります。さらに搬送にかかる時間 `transport` は既知です。異なる個数を処理した測定から、起動時間と1個当たり時間を求め、次の注文の時間を予測します。

未知の起動時間を $x_ {0}$、1個当たり時間を $x_ {1}$、個数を $q$ とすると、予測は `transport` $+x_ {0}+q x_ {1}$ です。`LinearForm` の `offset` に搬送時間、`terms` に未知値の係数1と $q$ を入れます。新しい注文も同じ式で評価します。

ここでは符号制限のない線形近似を使います。負の時間を許したくない場合は `Domain::real(0, upper)` に変えます。個数がすべて同じでは2係数を分離できないため、異なる個数の測定を含めます。

**入力**：`m transport sigma query_q`、続くm行が `q measured_seconds`。m≥2、sigma>0、qは既知の非負個数。

**出力**：起動時間、1個当たり時間、次の注文の予測時間。

**ファイル名**：`examples/02_processing_time.cpp`

**コード**

```cpp
// 起動時間と1個当たりの処理時間を分ける
// 入力: m transport sigma query_q、続くm行が q measured_seconds。m≥2、sigma>0、
// qは既知の非負個数。
// 出力: 起動時間、1個当たり時間、次の注文の予測時間。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// ヘッダーとこのcppを同じ場所に置くか、-Iで場所を指定する。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数0は起動時間、変数1は1個当たり時間。
using LE = LinearEstimator<>;

int main() {
    int m;
    double transport, sigma, query_q;

    if (!(std::cin >> m >> transport >> sigma >> query_q)) {
        return 0;
    }

    // 無制限の実数2個。seedは既定の0。
    LE model({LE::Domain::real(), LE::Domain::real()});

    for (int i = 0; i < m; ++i) {
        double q, seconds;
        std::cin >> q >> seconds;

        // 空のtermsとoffset=0で始まる一次式。
        LE::LinearForm form;

        // 起動時間の係数は1、単位時間の係数は処理個数。
        form.terms = {{0, 1}, {1, q}};

        // 測定時間に含まれる、既知の搬送時間。
        form.offset = transport;

        // 観測誤差の標準偏差を秒で指定する。
        model.add_factor(form, LE::Loss::gaussian(seconds, sigma));
    }

    // 二乗誤差を小さくする設定。
    LE::OptimizeParam p;

    // 時間上限を無効化する代わりに、次の回数上限を必ず設定する。
    p.time_limit_us = -1;

    // 専用の計算経路で解ければ、上限より早く終了する。
    p.max_steps = 10'000;

    // パラメータ推定を実行する。
    model.optimize(p);

    // 解の参照は実行可能性確認の後に行う。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // x[0]とx[1]が求めた係数。
    const auto x = model.best();

    // 新しい注文の時間を表す式。
    const LE::LinearForm next{{{0, 1}, {1, query_q}}, transport};

    // 式の値を計算する。Lossや観測ノイズを追加する操作ではない。
    const double seconds = next.evaluate(x);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << x[0] << ' ' << x[1] << ' ' << seconds << '\n';
}
```

**入力例**

```text
3 1 0.5 8
1 6
3 12
5 18
```

**実行結果（標準出力）**

```text
2.000000 3.000000 27.000000
```

### 3.3 道路の合計時間から、候補経路の平均と不確かさを比べる

3本の道路e0・e1・e2が三角形を作っています。目的地への候補は「e0とe1を通る」か「e2を直接通る」の二つです。各道路の時間は未知で、道路2本を通った合計時間だけを3回測っています。

未知変数3個を各道路の秒数に対応させ、観測した経路ごとに和の式を登録します。十分な情報がないときの支えとして、道路ごとの事前の目安とその標準偏差も登録します。`sample` は観測と矛盾しにくい道路時間の組を集めます。保存した組ごとに候補経路の和を取れば、経路全体の平均と分散を計算できます。

e0とe1の推定は互いに関係するため、各道路の分散を単純に足す方法は一般に正しくありません。`predict` は**同じ標本内の和**を使うので、この関係を保ちます。例では平均が小さい経路を選び、分散の平方根である標準偏差も表示します。次の走行時の計測ノイズはこの値には含みません。

**入力**：`prior0 prior1 prior2 prior_sigma`、次行 `y01 y12 y02 observation_sigma`。標準偏差は正。道路時間は0〜100秒。

**出力**：経路0、経路1の順に「平均 標準偏差」、最後に選んだ経路番号。

**ファイル名**：`examples/03_route_choice.cpp`

**コード**

```cpp
// 道路の合計時間から、候補経路の平均と不確かさを比べる
// 入力: prior0 prior1 prior2 prior_sigma、次行 y01 y12 y02 observation_sigma。
// 標準偏差は正。道路時間は0〜100秒。
// 出力: 経路0、経路1の順に「平均 標準偏差」、最後に選んだ経路番号。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// sampleとpredictも同じヘッダーに含まれる。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数jを道路ejの真の時間に対応させる。
using LE = LinearEstimator<>;

int main() {
    std::array<double, 3> prior{};
    double prior_sigma, y01, y12, y02, sigma;

    if (!(std::cin >> prior[0] >> prior[1] >> prior[2] >> prior_sigma)) {
        return 0;
    }

    std::cin >> y01 >> y12 >> y02 >> sigma;

    // 負の道路時間を除いた3変数。
    LE model(std::vector<LE::Domain>(3, LE::Domain::real(0, 100)), 11);

    for (int j = 0; j < 3; ++j) {
        // 道路ごとの事前の目安。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::gaussian(prior[j], prior_sigma));
    }

    const std::array<int, 2> a{0, 1}, b{1, 2}, c{0, 2};

    // 道路0+1の独立な観測。
    model.add_factor(LE::LinearForm::sum(a), LE::Loss::gaussian(y01, sigma));

    // 道路1+2の独立な観測。
    model.add_factor(LE::LinearForm::sum(b), LE::Loss::gaussian(y12, sigma));

    // 道路0+2の独立な観測。
    model.add_factor(LE::LinearForm::sum(c), LE::Loss::gaussian(y02, sigma));

    // 分布から標本を採取する設定。
    LE::SampleParam p;

    // 例の再現性のため、時間ではなく試行数で区切る。
    p.time_limit_us = -1;

    // warmupも含む今回の試行数上限。
    p.max_steps = 50'000;

    // 最初の500試行は統計・保存・hookの対象にしない。
    p.warmup_steps = 500;

    // predict用に、直近最大5000組を保存する。既定の0ではpredictのcountは0。
    p.sample_capacity = 5'000;

    // 観測・事前情報から道路時間の組を採取する。
    model.sample(p);

    // 経路0は道路0+1。相関を含めた平均と分散。
    const auto via = model.predict(LE::LinearForm::sum(a));

    // 経路1は道路2のみ。
    const auto direct = model.predict(LE::LinearForm{{{2, 1}}, 0});

    // 保存標本がない場合、meanやvarianceを使ってはいけない。
    if (via.count == 0 || direct.count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(6);

    // 観測ノイズを含まない潜在時間の標準偏差。
    std::cout << via.mean << ' ' << std::sqrt(via.variance) << '\n';

    // 経路全体の値を直接比較する。
    std::cout << direct.mean << ' ' << std::sqrt(direct.variance) << '\n';

    // 同じ平均なら経路1を選ぶ例。
    std::cout << (via.mean < direct.mean ? 0 : 1) << '\n';
}
```

**入力例**

```text
5 4 7 3
9 11 12 1
```

**実行結果（標準出力）**

```text
9.029192 0.979051
6.984063 0.833482
1
```

### 3.4 勝ち負けだけから、候補の相対的な性能を推定する

候補A・B・Cの正確な性能は測れず、2候補を比較して「左が良いか」だけが分かる問題です。比較にもぶれがあり、真の性能差に正規ノイズを加えた値が0以上なら左が勝つと考えます。

左iと右jの真の差は $x_ {i}-x_ {j}$ です。左が勝ったなら「ノイズを足した差が0以上だった」という区間観測、負けたなら0以下の区間観測を登録します。**真の差が必ず同じ符号だったというハード制約ではありません。**

差だけでは全員に同じ数を足しても区別できないので、各性能に「0付近、標準偏差3」の事前情報を置きます。出力はこの共通基準からの相対性能の平均です。勝敗から性能の絶対単位まで決まるわけではなく、入力のノイズ尺度が性能差の単位を定めます。

**入力**：`n m sigma`、続くm行が `left right win`。IDは0始まり、winは左の勝ち=1・負け=0。n≥2、sigma>0。

**出力**：候補ID順の相対性能の標本平均。

**ファイル名**：`examples/04_comparisons.cpp`

**コード**

```cpp
// 勝ち負けだけから、候補の相対的な性能を推定する
// 入力: n m sigma、続くm行が left right win。IDは0始まり、winは左の勝ち=1・負け=0。n≥2、
// sigma>0。
// 出力: 候補ID順の相対性能の標本平均。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 正規ノイズ付きの区間観測を利用する。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 各変数は候補の未知性能。
using LE = LinearEstimator<>;

int main() {
    int n, m;
    double sigma;

    if (!(std::cin >> n >> m >> sigma)) {
        return 0;
    }

    // 性能は正負を許す実数。
    LE model(std::vector<LE::Domain>(n, LE::Domain::real()), 13);

    for (int j = 0; j < n; ++j) {
        // 共通基準を与え、分布を発散させない事前情報。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::gaussian(0, 3));
    }

    for (int k = 0; k < m; ++k) {
        int left, right, win;
        std::cin >> left >> right >> win;
        const std::array<int, 1> lhs{left}, rhs{right};

        // 左の性能から右の性能を引く一次式。
        const auto difference = LE::LinearForm::difference(lhs, rhs);
        const double lo = win ? 0.0 : -INFINITY;
        const double hi = win ? INFINITY : 0.0;

        // ノイズ後の差の符号だけを観測した。
        model.add_factor(difference, LE::Loss::gaussian_interval(lo, hi, sigma));
    }

    // 比較結果に整合する性能分布を採取する。
    LE::SampleParam p;

    // 回数で予算を指定する。
    p.time_limit_us = -1;

    // 小さい例向けの試行数。大規模入力の収束保証ではない。
    p.max_steps = 60'000;

    // 初期状態の影響を減らすため、この間は採取しない。
    p.warmup_steps = 1'000;

    // 各変数の平均・分散はcollect_moments=trueの既定値で蓄積される。
    model.sample(p);

    // 統計の借用参照。次の変更・実行前に読み取る。
    const auto& moments = model.moments();

    // warmupのみで終わったときなどは平均を読まない。
    if (moments.count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(6);

    // 候補ID順の推定値。
    for (int j = 0; j < n; ++j) {
        std::cout << (j ? " " : "") << moments.mean[j];
    }

    std::cout << '\n';
}
```

**入力例**

```text
3 3 0.5
0 1 1
1 2 1
0 2 1
```

**実行結果（標準出力）**

```text
2.529470 -0.009810 -2.557478
```

### 3.5 丸められ、上限で止まる表示器から長さを推定する

長さを2mm刻みに丸め、0〜10mmしか表示しない測定器を考えます。内部では真の長さに標準偏差 `sigma` の正規ノイズを足してから表示を決めます。表示6なら内部値は5〜7mm、表示10なら内部値は9mm以上でした。10を正確な長さとして登録すると、上限を超える長さを過小評価してしまいます。

未知変数は部品の真の長さ1個です。表示を、丸め幅の半分を使って内部値の区間に戻し、その区間を `gaussian_interval` に渡します。下端表示なら下限を負の無限大、上端表示なら上限を正の無限大にします。真の長さの範囲0〜30mmは、表示器の上限とは別に指定します。

ここでの丸めは最も近い目盛りへの丸めです。0・10の表示も含め、入力表示はこの規則で作られた値とします。連続ノイズなので区間端点を含むかどうかは観測確率に影響しません。

**入力**：`m step display_lo display_hi sigma prior_mean prior_sigma`、続くm個が表示値。step>0、幅はstepの整数倍、標準偏差は正。

**出力**：真の長さの標本平均と標準偏差。

**ファイル名**：`examples/05_rounded_sensor.cpp`

**コード**

```cpp
// 丸められ、上限で止まる表示器から長さを推定する
// 入力: m step display_lo display_hi sigma prior_mean prior_sigma、続くm個が表示値。
// step>0、幅はstepの整数倍、標準偏差は正。
// 出力: 真の長さの標本平均と標準偏差。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 丸め前のノイズ付き区間を因子にする。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数0だけが真の長さ。
using LE = LinearEstimator<>;

int main() {
    int m;
    double step, display_lo, display_hi, sigma, prior, prior_sigma;

    if (!(std::cin >> m >> step >> display_lo >> display_hi >> sigma >> prior >> prior_sigma)) {
        return 0;
    }

    // 表示上限ではなく、物理的な長さの範囲を指定する。
    LE model({LE::Domain::real(0, 30)}, 17);

    // 上限表示だけでも推定が発散しない事前情報。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(prior, prior_sigma));

    for (int i = 0; i < m; ++i) {
        double displayed;
        std::cin >> displayed;
        const double lo = displayed == display_lo ? -INFINITY : displayed - step / 2;
        const double hi = displayed == display_hi ? INFINITY : displayed + step / 2;

        // 真値のハード区間に置き換えない。
        model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian_interval(lo, hi, sigma));
    }

    // 点推定だけでなく長さの不確かさも求める。
    LE::SampleParam p;

    // 固定試行予算を使用する。
    p.time_limit_us = -1;

    // warmupと採取を合わせた上限。
    p.max_steps = 30'000;

    // 初期500試行の値は平均に混ぜない。
    p.warmup_steps = 500;

    // 非正規の実数モデルでslice経路を明示する例。通常はAutomaticから始めてよい。
    p.method = LE::Method::Slice;

    // 1変数なのでsample_stride=0では通常1試行ごとに採取する。
    model.sample(p);

    // 全採取標本の平均と分散。
    const auto& statistics = model.moments();

    // 統計が空なら読み取らない。
    if (statistics.count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(6);

    // 真の長さの不確かさ。
    std::cout << statistics.mean[0] << ' ' << std::sqrt(statistics.variance[0]) << '\n';
}
```

**入力例**

```text
3 2 0 10 1 10 5
10 10 8
```

**実行結果（標準出力）**

```text
9.521444 0.758974
```

### 3.6 大きな遅延が混ざった測定から、通常の処理時間を求める

機械の処理時間を「起動時間＋個数×単位時間」で近似したいものの、まれな停止によって測定値の一部が大幅に増えます。入力例では、通常は2秒＋個数×2秒ですが、3個のときだけ40秒を記録しています。

一次式は通常の回帰と同じです。違うのは、観測からのずれを罰する方法です。Gaussianはずれを二乗するため、大きなずれが非常に強く働きます。Laplaceはずれの絶対値、Student tは大きなずれにさらに緩やかなコストを与えます。この例は入力 `mode` で後者二つを比較できます。

どちらも「遅延したデータを必ず識別して除く」機能ではありません。尺度 `scale` は通常の誤差の大きさに合わせます。Student tの自由度はここでは4に固定します。出力するのはコストを小さくする係数で、分布平均ではありません。

**入力**：`m mode scale query_q`、続くm行が `q seconds`。mode=0はStudent t、1はLaplace。scale>0。

**出力**：起動時間、1個当たり時間、新しい個数での通常時間の予測。

**ファイル名**：`examples/06_robust_regression.cpp`

**コード**

```cpp
// 大きな遅延が混ざった測定から、通常の処理時間を求める
// 入力: m mode scale query_q、続くm行が q seconds。mode=0はStudent t、1はLaplace。scale>0。
// 出力: 起動時間、1個当たり時間、新しい個数での通常時間の予測。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// Student tとLaplaceの標準Lossを利用する。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数0=起動時間、1=単位時間。
using LE = LinearEstimator<>;

int main() {
    int m, mode;
    double scale, query_q;

    if (!(std::cin >> m >> mode >> scale >> query_q)) {
        return 0;
    }

    // 非負で最大100の係数を仮定する。
    LE model({LE::Domain::real(0, 100), LE::Domain::real(0, 100)}, 19);

    for (int i = 0; i < m; ++i) {
        double q, seconds;
        std::cin >> q >> seconds;

        // 自由度4。scaleは標準偏差そのものではない。
        LE::Loss loss = LE::Loss::student_t(seconds, scale, 4);

        if (mode == 1) {
            // Laplaceの標準偏差はsqrt(2)*scale。
            loss = LE::Loss::laplace(seconds, scale);
        }

        // 誤差モデルだけを替え、一次式は同じにする。
        model.add_factor({{{0, 1}, {1, q}}, 0}, loss);
    }

    // 外れ値の影響を抑えた目的関数を最小化する。
    LE::OptimizeParam p;

    // 回数予算で実行する。
    p.time_limit_us = -1;

    // 一般の目的関数なので大域最適解の保証はない。
    p.max_steps = 50'000;

    // 初めはコスト差1程度の悪化も試し得る温度。
    p.temperature_start = 1;

    // 終盤は悪化を受け入れにくくする。
    p.temperature_end = 0.001;

    // 更新の半分を温度0の改善優先更新にする。
    p.greedy_probability = 0.5;

    // 最良候補はモデル内に保存される。
    model.optimize(p);

    // 取得前に確認する。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 次の変更・実行まで有効。
    const auto x = model.best();

    // 次に処理する個数を既知係数にする。
    const LE::LinearForm next{{{0, 1}, {1, query_q}}, 0};

    std::cout << std::fixed << std::setprecision(6);

    // 通常の中心時間を予測する。
    std::cout << x[0] << ' ' << x[1] << ' ' << next.evaluate(x) << '\n';
}
```

**入力例**

```text
6 0 1 5
0 2
1 4
2 6
4 10
8 18
3 40
```

**実行結果（標準出力）**

```text
2.024592 2.000107 12.025126
```

### 3.7 カタログ候補と入手頻度から、部品の組合せを推定する

2個の部品の合計長さだけが分かっており、各部品はカタログに載った長さのどれかです。候補には「よく使われる規格」「まれな規格」の違いもあります。各変数の値域を候補集合にし、観測との一致と候補の事前重みを両方使います。

候補重みを $w$ としたとき、独自コストを $-\log w$ と置くと、採取時の重みは $\exp(\log w)=w$ となります。重みは正なら合計1にそろえる必要はありません。候補の値と重みの対応は利用側の表で管理します。

コードは「最良の具体的な組合せ」と「各候補が標本に現れた割合」を出します。たとえば平均長さが14mmでも、14mmがカタログにない可能性があります。採用する部品の指定には具体的な候補値を使います。

**入力**：最初に `k0 k1`、次に部品0のk0行と部品1のk1行の `value weight`、最後に `observed_sum sigma`。各候補値は部品内で重複なし、weight>0、sigma>0。

**出力**：最良の2候補、続く2行に入力候補順の推定確率。

**ファイル名**：`examples/07_catalog.cpp`

**コード**

```cpp
// カタログ候補と入手頻度から、部品の組合せを推定する
// 入力: 最初に k0 k1、次に部品0のk0行と部品1のk1行の value weight、
// 最後に observed_sum sigma。各候補値は部品内で重複なし、weight>0、sigma>0。
// 出力: 最良の2候補、続く2行に入力候補順の推定確率。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 有限候補と独自Lossを同じモデルで使う。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

struct CatalogCost {
    std::array<std::vector<std::pair<double, double>>, 2> rows;

    // custom_id=id、一次式の値=zを受け取り、この因子のコストを返す。
    double operator()(int id, double z) const {
        for (const auto& [value, weight] : rows[id]) {
            // 正の事前重みを負の対数へ変換する。
            if (z == value) {
                return -std::log(weight);
            }
        }

        // 候補外を禁止する。NaNや負の無限大は返さない。
        return INFINITY;
    }
};

// CustomLossが変わるとDomainなどの入れ子型も別の型になる。
using LE = LinearEstimator<CatalogCost>;

int main() {
    std::array<int, 2> k{};

    if (!(std::cin >> k[0] >> k[1])) {
        return 0;
    }

    CatalogCost cost;

    // 変数ごとの候補集合を作るための値域列。
    std::vector<LE::Domain> domains;

    for (int j = 0; j < 2; ++j) {
        std::vector<double> values;

        for (int i = 0; i < k[j]; ++i) {
            double value, weight;
            std::cin >> value >> weight;
            values.push_back(value);
            cost.rows[j].push_back({value, weight});
        }

        // 候補の整列・重複除去はライブラリ側で行う。
        domains.push_back(LE::Domain::values(values));
    }

    double measured, sigma;
    std::cin >> measured >> sigma;

    // 評価器をコピーして所有させる。入力表costは出力の候補順にも使う。
    LE model(domains, 23, cost);

    for (int j = 0; j < 2; ++j) {
        // custom IDは部品番号。factor IDとは別物。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::custom(j));
    }

    // 合計長さのノイズ付き観測。
    model.add_factor({{{0, 1}, {1, 1}}, 0}, LE::Loss::gaussian(measured, sigma));

    std::array<std::vector<int64_t>, 2> hits{
        std::vector<int64_t>(k[0]),
        std::vector<int64_t>(k[1])
    };
    int64_t count = 0;

    // 採取された1組を読む。ここでモデルを変更・再実行しない。
    auto hook = [&](const LE::SampleView& sample) {
        ++count;

        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < k[j]; ++i) {
                // ビューの値はhook内で消費する。
                if (sample.values[j] == cost.rows[j][i].first) {
                    ++hits[j][i];
                }
            }
        }
    };

    // 候補確率を標本頻度として求める。
    LE::SampleParam p;

    // 再現用の回数制限。
    p.time_limit_us = -1;

    // 採取も含む試行数上限。
    p.max_steps = 50'000;

    // 初期の更新は頻度に含めない。
    p.warmup_steps = 500;

    // 小さい候補集合や候補ペアを列挙できる上限。
    p.enumeration_limit = 64;

    // 平均長さではなく候補頻度をhookで集計するため、標準統計を省略。
    p.collect_moments = false;

    // 標準統計が無効でもsample hookは呼ばれる。
    model.sample(p, hook);

    // 全変数が1候補のFixedになった場合も、標本は生成されない。
    if (!model.has_solution() || count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    // 各要素が実際の候補集合に入る組合せ。
    const auto best = model.best();

    std::cout << std::fixed << std::setprecision(6);
    std::cout << best[0] << ' ' << best[1] << '\n';

    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < k[j]; ++i) {
            std::cout
                << (i ? " " : "")
                << static_cast<double>(hits[j][i]) / static_cast<double>(count);
        }

        std::cout << '\n';
    }
}
```

**入力例**

```text
3 2
8 0.2
12 0.6
20 0.2
5 0.75
10 0.25
22 2
```

**実行結果（標準出力）**

```text
12.000000 10.000000
0.029859 0.752687 0.217455
0.306828 0.693172
```

### 3.8 隣接区画の関係から、センサーのない場所の温度を補う

一直線に並ぶ区画のうち、0番の温度は正確に分かり、最後の区画には誤差のあるセンサーがあります。中央の区画にはセンサーがありません。「隣接する場所の温度は大きく変わりにくい」という情報を使って補います。

各区画の温度を未知変数とし、隣接差 $x_ {j}-x_ {j-1}$ に平均0のGaussian因子を付けます。標準偏差 `neighbor_sigma` が小さいほど、差が大きい状態を強く避けます。これは物理法則による等式ではなく、滑らかさの仮定です。正確に既知の0番は `Domain::fixed` にします。

入力例の0番18度、2番の観測24度では、中央は約20.667度になります。2番の観測にも誤差を許すので、単純な中点21度とは少し異なります。一般のグラフなら、隣接ペアを入力で読み、同じ差の因子を辺ごとに作れば対応できます。連結していない部分にも基準観測や事前情報を与えます。

**入力**：`n fixed_temperature last_measurement observation_sigma neighbor_sigma`。n≥2、両sigma>0。

**出力**：0番からn−1番までの推定温度。

**ファイル名**：`examples/08_smooth_field.cpp`

**コード**

```cpp
// 隣接区画の関係から、センサーのない場所の温度を補う
// 入力: n fixed_temperature last_measurement observation_sigma neighbor_sigma。n≥2、
// 両sigma>0。
// 出力: 0番からn−1番までの推定温度。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 差のGaussian因子で隣接関係を表す。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数IDを区画IDと一致させる。
using LE = LinearEstimator<>;

int main() {
    int n;
    double anchor, measured, sigma, neighbor_sigma;

    if (!(std::cin >> n >> anchor >> measured >> sigma >> neighbor_sigma)) {
        return 0;
    }

    // 初めに全区画を実数として用意。
    std::vector<LE::Domain> domains(n, LE::Domain::real());

    // 0番は厳密な既知値。誤差のある観測ならfixedではなく因子を使う。
    domains[0] = LE::Domain::fixed(anchor);

    // 値域列を所有する推定器を作る。
    LE model(domains);

    for (int j = 1; j < n; ++j) {
        // 隣接温度差の目安は0。
        model.add_factor({{{j, 1}, {j - 1, -1}}, 0}, LE::Loss::gaussian(0, neighbor_sigma));
    }

    // 最後の区画の測定誤差を反映。
    model.add_factor({{{n - 1, 1}}, 0}, LE::Loss::gaussian(measured, sigma));

    // 観測と滑らかさの合計コストを小さくする。
    LE::OptimizeParam p;

    // 固定試行予算。
    p.time_limit_us = -1;

    // 解ける場合は早期終了する。
    p.max_steps = 10'000;

    // 連続温度の候補を求める。
    model.optimize(p);

    // 参照APIの前提を確認する。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 固定区画も含め、全n変数が入る。
    const auto x = model.best();

    std::cout << std::fixed << std::setprecision(6);

    for (int j = 0; j < n; ++j) {
        std::cout << (j ? " " : "") << x[j];
    }

    std::cout << '\n';
}
```

**入力例**

```text
3 18 24 1 2
```

**実行結果（標準出力）**

```text
18.000000 20.666667 23.333333
```

### 3.9 合計個数が厳密に分かる在庫の内訳を推定する

赤・青・緑の球が合計12個あり、各色の個数は整数です。色別の測定はぶれますが、合計12だけは正確に分かっています。推定結果にもこの合計を必ず守らせます。

未知変数を色別の個数とし、和に `hard_interval(total, total)` を付けます。しかし1色だけを増減させると必ず合計に違反します。そこで「赤を増やした分だけ緑を減らす」「青を増やした分だけ緑を減らす」という同時変更の方向を渡します。各方向に沿って整数だけ動かせば合計が保たれます。

初期状態も合計を満たすものを与えます。これは一般の整数制約を自動で完全に解く機能ではありません。方向が足りず候補間を行き来できないと探索・採取が偏ります。この3色の単純な合計制約では、例の2方向で各色間の移し替えを表せます。

**入力**：`total red_measurement blue_measurement green_measurement sigma`。totalは正の整数、sigma>0。

**出力**：合計を厳密に満たす最良の赤・青・緑の整数個数。

**ファイル名**：`examples/09_conserved_inventory.cpp`

**コード**

```cpp
// 合計個数が厳密に分かる在庫の内訳を推定する
// 入力: total red_measurement blue_measurement green_measurement sigma。totalは正の整数、
// sigma>0。
// 出力: 合計を厳密に満たす最良の赤・青・緑の整数個数。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 整数値域・ハード制約・同時変更方向を組み合わせる。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数0=赤、1=青、2=緑。
using LE = LinearEstimator<>;

int main() {
    int total;
    std::array<double, 3> measured{};
    double sigma;

    if (!(std::cin >> total >> measured[0] >> measured[1] >> measured[2] >> sigma)) {
        return 0;
    }

    // 各色は0〜total個。
    LE model(std::vector<LE::Domain>(3, LE::Domain::integer(0, total)), 29);
    const std::array<int, 3> all{0, 1, 2};

    // 整数和の厳密等式。
    model.add_factor(LE::LinearForm::sum(all), LE::Loss::hard_interval(total, total));

    for (int j = 0; j < 3; ++j) {
        // 色別計測には誤差を許す。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::gaussian(measured[j], sigma));
    }

    const std::array<double, 3> initial{0, 0, static_cast<double>(total)};

    // 値域内かつ合計を満たす初期値。既存のbestもこの状態で置き換わる。
    model.set_state(initial);

    // 同じ方向内では整数変数だけを使い、係数も整数にする。
    model.set_directions({
        {{0, 1}, {2, -1}},
        {{1, 1}, {2, -1}}
    });

    // 合計を保ったまま色別測定に合う整数内訳を探す。
    LE::OptimizeParam p;

    // 固定試行予算。
    p.time_limit_us = -1;

    // 小さな例には十分な探索回数を確保する。
    p.max_steps = 20'000;

    // この例では利用者が指定した保存方向を使う。
    p.auto_directions = false;

    // 準備済みの同時方向を常に選ぶ。通常の座標変更は合計を破るため。
    p.direction_probability = 1;

    // 小さい整数範囲なら条件付き候補を列挙する。
    p.method = LE::Method::Conditional;

    // 候補が64を超える場合は別の更新経路に移る。
    p.enumeration_limit = 64;

    // 最良の実行可能候補を保持する。
    model.optimize(p);

    // 制約が増えた場合は候補が見つからないこともある。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 各要素は整数を表すdouble。
    const auto x = model.best();

    std::cout << std::fixed << std::setprecision(0);
    std::cout << x[0] << ' ' << x[1] << ' ' << x[2] << '\n';
}
```

**入力例**

```text
12 7 3 2 1
```

**実行結果（標準出力）**

```text
7 3 2
```

### 3.10 絶対に守る範囲と、できれば守りたい範囲を分ける

2個のバルブを0〜10の範囲で調整します。それぞれ希望値がありますが、安全のため合計は決められた範囲から外せません。一方、2個の差は「できれば1〜2にしたい」程度です。

未知変数をバルブ値とし、各希望値へのGaussian因子、和へのハード区間、差へのソフト区間を登録します。ハード区間外は候補として認めません。ソフト区間外には、区間までの距離を二乗した有限の罰点を与えます。したがって差の希望は、他の希望との兼ね合いで破られることがあります。

初期状態は安全な合計の中央を2個に等分した状態です。コード末尾では `evaluate` で全コスト、`Loss::evaluate` で差の罰点だけを確かめます。どの制約が厳密か、どれが希望かを分けて出力を読めます。

**入力**：`wish0 wish1 hard_lo hard_hi soft_lo soft_hi`。0≤hard_lo<hard_hi≤20、soft_lo≤soft_hi。

**出力**：2個の設定値、次行に「合計 差 全コスト 差のソフト罰点」。

**ファイル名**：`examples/10_tolerance_control.cpp`

**コード**

```cpp
// 絶対に守る範囲と、できれば守りたい範囲を分ける
// 入力: wish0 wish1 hard_lo hard_hi soft_lo soft_hi。0≤hard_lo<hard_hi≤20、
// soft_lo≤soft_hi。
// 出力: 2個の設定値、次行に「合計 差 全コスト 差のソフト罰点」。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// hardとsoftでは区間の意味が異なる。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数0と1がバルブ設定値。
using LE = LinearEstimator<>;

int main() {
    double a, b, hard_lo, hard_hi, soft_lo, soft_hi;

    if (!(std::cin >> a >> b >> hard_lo >> hard_hi >> soft_lo >> soft_hi)) {
        return 0;
    }

    // 各バルブの物理的な上下限。
    LE model({LE::Domain::real(0, 10), LE::Domain::real(0, 10)}, 31);

    // 希望値から2ずれるとコスト0.5。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(a, 2));

    // もう一方の希望値も同じ強さで評価。
    model.add_factor({{{1, 1}}, 0}, LE::Loss::gaussian(b, 2));

    // 合計設定値。
    const LE::LinearForm sum{{{0, 1}, {1, 1}}, 0};

    // 設定値の差。
    const LE::LinearForm difference{{{0, 1}, {1, -1}}, 0};

    // 区間外へのずれ1にコスト0.5。
    const auto soft = LE::Loss::soft_interval(soft_lo, soft_hi, 1);

    // 安全範囲は必ず守る。
    model.add_factor(sum, LE::Loss::hard_interval(hard_lo, hard_hi));

    // 差の範囲は有限の罰点で表す希望。
    model.add_factor(difference, soft);
    const double half = (hard_lo + hard_hi) / 4;
    const std::array<double, 2> initial{half, half};

    // 合計がハード区間中央になる安全な初期値。
    model.set_state(initial);

    // 安全範囲内で希望からのずれを小さくする。
    LE::OptimizeParam p;

    // 回数制限で実行する。
    p.time_limit_us = -1;

    // 例用の試行数。
    p.max_steps = 40'000;

    // ハード制約を満たす最良候補を保持する。
    model.optimize(p);

    // best()の前提を確認。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 次の実行まで有効な参照。
    const auto x = model.best();

    // 因子の一次式だけを計算。
    const double total = sum.evaluate(x);

    // ソフト区間と比べる実際の差。
    const double gap = difference.evaluate(x);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << x[0] << ' ' << x[1] << '\n';

    // 全体と1因子の評価を区別。
    std::cout
        << total << ' '
        << gap << ' '
        << model.evaluate(x) << ' '
        << soft.evaluate(gap) << '\n';
}
```

**入力例**

```text
3 3 8 9 1 2
```

**実行結果（標準出力）**

```text
4.444194 3.555806
8.000000 0.888388 0.305556 0.006229
```

### 3.11 成功・失敗から、投入量と成功確率の関係を推定する

投入量を増やすと成功しやすくなる作業があり、投入量ごとの試行数と成功数が記録されています。次の投入量で成功する確率を求めます。

未知係数を切片 $x_ {0}$ と傾き $x_ {1}$ とし、投入量 $q$ に対してまず一次式 $z=x_ {0}+q x_ {1}$ を作ります。z自体は確率ではありません。`sigmoid(z)=1/(1+exp(-z))` という0〜1に収まる変換で成功確率にします。

同じ投入量でt回中s回成功した観測のコストは `t*log(1+exp(z))-s*z` です。コードでは大きなzでも計算しやすい同値な式を使います。係数の各標本を確率へ変換し、その平均を次回の成功確率とします。**平均係数を1回だけ変換した値とは一般に一致しません。**

**入力**：`m query_q`、続くm行 `q successes trials`。trials≥1、0≤successes≤trials。観測は条件ごとに独立とする。

**出力**：次の投入量での予測成功確率。

**ファイル名**：`examples/11_success_probability.cpp`

**コード**

```cpp
// 成功・失敗から、投入量と成功確率の関係を推定する
// 入力: m query_q、続くm行 q successes trials。trials≥1、0≤successes≤trials。
// 観測は条件ごとに独立とする。
// 出力: 次の投入量での予測成功確率。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 非線形の確率変換は1つの一次式に対するcustom Lossへ書く。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

struct BinomialCost {
    std::vector<std::pair<int, int>> counts;

    // idで観測行を選び、zは切片+投入量*傾き。
    double operator()(int id, double z) const {
        const auto [successes, trials] = counts[id];
        const double softplus = std::log1p(std::exp(-std::abs(z)));

        // 負の対数確率。zに依存しない定数は省く。
        return static_cast<double>(trials) * softplus
             + static_cast<double>(trials - successes) * std::max(z, 0.0)
             + static_cast<double>(successes) * std::max(-z, 0.0);
    }
};

// この型のDomain・Loss・SampleParamで統一する。
using LE = LinearEstimator<BinomialCost>;

double sigmoid(double z) {
    const double e = std::exp(-std::abs(z));
    return z >= 0 ? 1 / (1 + e) : e / (1 + e);
}

int main() {
    int m;
    double query_q;

    if (!(std::cin >> m >> query_q)) {
        return 0;
    }

    std::vector<double> q(m);
    BinomialCost cost;

    for (int i = 0; i < m; ++i) {
        int successes, trials;
        std::cin >> q[i] >> successes >> trials;
        cost.counts.push_back({successes, trials});
    }

    // 係数は正負どちらも許す。
    LE model({LE::Domain::real(), LE::Domain::real()}, 37, cost);

    for (int j = 0; j < 2; ++j) {
        // 全成功などでも係数が発散しないよう事前情報を置く。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::gaussian(0, 3));
    }

    for (int i = 0; i < m; ++i) {
        // 各観測の回数表をcustom IDで参照。
        model.add_factor({{{0, 1}, {1, q[i]}}, 0}, LE::Loss::custom(i));
    }

    int64_t count = 0;
    double mean_probability = 0;

    // 次の投入量でのzを表す式。
    const LE::LinearForm query{{{0, 1}, {1, query_q}}, 0};

    // 保存容量なしでも、採取時に非線形量を計算できる。
    auto hook = [&](const LE::SampleView& sample) {
        // 各標本の係数から確率を作る。
        const double probability = sigmoid(query.evaluate(sample.values));
        ++count;
        mean_probability += (probability - mean_probability) / static_cast<double>(count);
    };

    // 温度1のモデル分布から採取する。
    LE::SampleParam p;

    // 再現用の回数予算。
    p.time_limit_us = -1;

    // 相関のある2係数を十分に動かすための例用設定。
    p.max_steps = 80'000;

    // この間の値は成功確率の集計に含めない。
    p.warmup_steps = 1'000;

    // 係数そのものの平均が不要なので、hookだけで集計。
    p.collect_moments = false;

    // hook内ではモデルを更新しない。
    model.sample(p, hook);

    if (count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(6) << mean_probability << '\n';
}
```

**入力例**

```text
3 1.5
0 2 10
1 5 10
2 8 10
```

**実行結果（標準出力）**

```text
0.667769
```

### 3.12 真の長さに比例してぶれる測定を扱う

測定値yが、真の長さxの±r倍の範囲に一様に出る測定器を考えます。たとえばr=0.1なら、長さ10cmのとき測定値は9〜11cmの範囲です。一定幅の加算ノイズとは異なり、長いほど誤差の幅が広がります。

yから可能な真値の範囲を逆算すると `y/(1+r) <= x <= y/(1-r)` です。この範囲外はコストを正の無限大にします。範囲内の確率密度は `1/(2*r*x)` なので、コストは `log(2*r*x)` です。区間判定だけでなくこの対数項も必要です。範囲内を全て同じコストにすると別の確率モデルになります。

ライブラリは独自コストの禁止領域を一般に自動修復できません。コードでは全観測が許す区間の共通部分を先に計算し、その内部を初期値にします。共通部分が空なら、この測定モデルでは観測が両立しません。

**入力**：`m r physical_lo physical_hi`、続くm個が正の測定値。m≥1、0<r<1、0<physical_lo<physical_hi。

**出力**：可能な真値区間の両端と、真の長さの標本平均。共通部分に幅がなければ `INCOMPATIBLE`。

**ファイル名**：`examples/12_relative_noise.cpp`

**コード**

```cpp
// 真の長さに比例してぶれる測定を扱う
// 入力: m r physical_lo physical_hi、続くm個が正の測定値。m≥1、0<r<1、
// 0<physical_lo<physical_hi。
// 出力: 可能な真値区間の両端と、真の長さの標本平均。共通部分に幅がなければ INCOMPATIBLE。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// zにより誤差幅が変わる観測をcustomで表す。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

struct RelativeCost {
    std::vector<double> observed;
    double rate;

    // zは正の真の長さ。idは観測番号。
    double operator()(int id, double z) const {
        const double lo = observed[id] / (1 + rate);
        const double hi = observed[id] / (1 - rate);

        // 許されない真値を禁止する。
        if (!(lo <= z && z <= hi)) {
            return INFINITY;
        }

        // 密度の幅の変化も含める。有限の負コストでもよい。
        return std::log(2 * rate * z);
    }
};

// 独自尤度を所有する推定器。
using LE = LinearEstimator<RelativeCost>;

int main() {
    int m;
    double rate, physical_lo, physical_hi;

    if (!(std::cin >> m >> rate >> physical_lo >> physical_hi)) {
        return 0;
    }

    RelativeCost cost{{}, rate};
    double feasible_lo = physical_lo, feasible_hi = physical_hi;

    for (int i = 0; i < m; ++i) {
        double y;
        std::cin >> y;
        cost.observed.push_back(y);
        feasible_lo = std::max(feasible_lo, y / (1 + rate));
        feasible_hi = std::min(feasible_hi, y / (1 - rate));
    }

    // 幅0の連続分布も、この例では採取対象にしない。
    if (!(feasible_lo < feasible_hi)) {
        std::cout << "INCOMPATIBLE\n";
        return 0;
    }

    // 真値の物理範囲を指定する。
    LE model({LE::Domain::real(physical_lo, physical_hi)}, 41, cost);

    for (int i = 0; i < m; ++i) {
        // 1回の測定につき1因子。
        model.add_factor({{{0, 1}}, 0}, LE::Loss::custom(i));
    }

    const std::array<double, 1> initial{(feasible_lo + feasible_hi) / 2};

    // 全custom因子で有限コストになる初期値を必ず与える。
    model.set_state(initial);

    // 切断された真値の分布から採取する。
    LE::SampleParam p;

    // 回数で制限する。
    p.time_limit_us = -1;

    // 例用の試行数上限。
    p.max_steps = 40'000;

    // 初期標本は捨てる。
    p.warmup_steps = 500;

    // 1次元の有限な支持区間に対する採取例。
    p.method = LE::Method::Slice;

    // model.evaluateも同じcustomコストを使う。
    model.sample(p);

    // 真値の平均を得る。
    const auto& statistics = model.moments();

    // count=0ならmeanは無効。
    if (statistics.count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(6);

    // 観測値の平均とは異なる推定。
    std::cout << feasible_lo << ' ' << feasible_hi << ' ' << statistics.mean[0] << '\n';
}
```

**入力例**

```text
2 0.1 1 20
10 10.5
```

**実行結果（標準出力）**

```text
9.545455 11.111111 10.289517
```

### 3.13 合計量が正確な2槽の水量を、変数消去して求める

2槽A・Bの水量は実数で、合計が正確にtotal Lです。一方、Aの量とA−Bの差には誤差付きの測定があります。実数の厳密等式を2変数のハード制約として直接探索するのではなく、B=total−Aを先に代入します。

自由な未知変数はAの水量xだけになります。Aの測定式はx、A−Bの測定式は `2*x-total` です。したがって係数2と定数項−totalの `LinearForm` を作れば、等式は自動的に守られます。xの範囲を0〜totalにすれば、復元したBも非負です。

出力はxとtotal−xです。より多くの変数がある場合も、独立な変数だけを残し、観測式・事前情報・値域をすべて代入後の式へ変換します。自由変数の箱型の値域だけで表せない境界には、別のハード不等式が必要です。

**入力**：`total measurement_a measurement_difference sigma`。total>0、sigma>0。2測定は独立。

**出力**：AとBの推定水量。

**ファイル名**：`examples/13_real_conservation.cpp`

**コード**

```cpp
// 合計量が正確な2槽の水量を、変数消去して求める
// 入力: total measurement_a measurement_difference sigma。total>0、sigma>0。2測定は独立。
// 出力: AとBの推定水量。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 実数の厳密な合計は、自由変数を減らして表す。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// ライブラリ内の変数はAの水量1個だけ。
using LE = LinearEstimator<>;

int main() {
    double total, measurement_a, measurement_difference, sigma;

    if (!(std::cin >> total >> measurement_a >> measurement_difference >> sigma)) {
        return 0;
    }

    // A>=0かつB=total-A>=0となる範囲。
    LE model({LE::Domain::real(0, total)});

    // Aの観測。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(measurement_a, sigma));

    // A-B=2*A-totalの観測。
    model.add_factor({{{0, 2}}, -total}, LE::Loss::gaussian(measurement_difference, sigma));

    // 変換後の1変数問題を解く。
    LE::OptimizeParam p;

    // 固定試行予算を使う。
    p.time_limit_us = -1;

    // 小さい例向けの上限。
    p.max_steps = 10'000;

    // 改善優先の更新だけを使う例。
    p.temperature_start = 0;

    // どちらか一方が0でも通常更新の温度は0になる。
    p.temperature_end = 0;

    // 元の等式を破る候補は、表現上作られない。
    model.optimize(p);

    // 有効候補を確認する。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 自由変数Aを読み取る。
    const double a = model.best()[0];
    const double b = total - a;

    std::cout << std::fixed << std::setprecision(6) << a << ' ' << b << '\n';
}
```

**入力例**

```text
10 7 4 1
```

**実行結果（標準出力）**

```text
7.000000 3.000000
```

### 3.14 ターンごとの訂正・無効化・固定値化を前の状態から解き直す

2個の荷物の重量を継続的に推定します。まず合計とA単体を測り、次のターンで合計測定を訂正します。さらに記録を確認すると、2本目の測定はA単体ではなくB単体だったことが分かります。最後にBの重量が正確に判明します。

変数の意味をA・Bのまま保ち、同じ推定器へ変更を通知します。観測値だけの変更は `set_loss`、観測対象も変わる訂正は `replace_factor`、一時的な取消は `set_enabled`、正確な重量の判明は `set_domain(...fixed(...))` です。因子IDは追加時に受け取り、以後の操作でも同じIDを使えます。

各段階で再実行し、旧候補を新しい目的関数で評価して探索を続けます。標本統計はモデル変更時にリセットされます。変数数の変更や因子の物理削除はできません。無効因子はIDを残して再利用できます。

**入力**：`old_sum measured_a corrected_sum corrected_b exact_b`。値は有限、最後のexact_bは0〜100。観測sigmaは例では1。

**出力**：6段階の「段階番号 A B」。0=初期、1=合計訂正、2=観測対象訂正、3=2本目無効、4=再有効、5=B固定。

**ファイル名**：`examples/14_online_updates.cpp`

**コード**

```cpp
// ターンごとの訂正・無効化・固定値化を前の状態から解き直す
// 入力: old_sum measured_a corrected_sum corrected_b exact_b。値は有限、
// 最後のexact_bは0〜100。観測sigmaは例では1。
// 出力: 6段階の「段階番号 A B」。0=初期、1=合計訂正、2=観測対象訂正、3=2本目無効、
// 4=再有効、5=B固定。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 同じインスタンスをターン間で保持する。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// AとBの変数IDは途中で入れ替えない。
using LE = LinearEstimator<>;

int main() {
    double old_sum, measured_a, corrected_sum, corrected_b, exact_b;

    if (!(std::cin >> old_sum >> measured_a >> corrected_sum >> corrected_b >> exact_b)) {
        return 0;
    }

    // 値域と乱数状態を保持するモデル。
    LE model({LE::Domain::real(0, 100), LE::Domain::real(0, 100)}, 43);

    // データが少ない段階を支える弱い事前情報。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(5, 10));

    // Bにも同じ事前情報。
    model.add_factor({{{1, 1}}, 0}, LE::Loss::gaussian(5, 10));

    // 更新用のfactor IDを保存。
    const int sum_id = model.add_factor({{{0, 1}, {1, 1}}, 0}, LE::Loss::gaussian(old_sum, 1));

    // 初めはAの測定として登録。
    const int sensor_id = model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(measured_a, 1));

    // 全段階で同じ探索設定を使う。
    LE::OptimizeParam p;

    // 実戦で使うなら、各ターンで残り時間に応じたマイクロ秒予算へ変更する。
    p.time_limit_us = -1;

    // 各呼出しに独立した試行数上限。
    p.max_steps = 15'000;

    std::cout << std::fixed << std::setprecision(6);

    auto solve_and_print = [&](int stage) {
        // 保持している候補から、更新後のモデルで再実行する。
        model.optimize(p);

        // 前ターンに解があっても、新しい制約では再確認が必要。
        if (!model.has_solution()) {
            std::cout << stage << " NO_SOLUTION\n";
            return;
        }

        // このビューを次のモデル変更まで保持しない。
        const auto x = model.best();
        std::cout << stage << ' ' << x[0] << ' ' << x[1] << '\n';
    };

    solve_and_print(0);

    // 一次式とsigmaが同じなら一部の計算済み情報を再利用できる。
    model.set_loss(sum_id, LE::Loss::gaussian(corrected_sum, 1));
    solve_and_print(1);

    // 同じIDでB単体の測定へ訂正。
    model.replace_factor(sensor_id, {{{1, 1}}, 0}, LE::Loss::gaussian(corrected_b, 1));
    solve_and_print(2);

    // 一時的に寄与を止める。物理削除ではない。
    model.set_enabled(sensor_id, false);
    solve_and_print(3);

    // 同じIDで再有効化する。
    model.set_enabled(sensor_id, true);
    solve_and_print(4);

    // Bの正確な値が分かったので固定する。関連因子も再評価される。
    model.set_domain(1, LE::Domain::fixed(exact_b));
    solve_and_print(5);
}
```

**入力例**

```text
12 5 13 8 8
```

**実行結果（標準出力）**

```text
0 5.019416 6.960975
1 5.029123 7.941462
2 5.029123 7.970585
3 6.492562 6.492513
4 5.029123 7.970585
5 5.000000 8.000000
```

### 3.15 採取を分割して続け、しきい値超過率と予測区間を調べる

1個のセンサーの真の値を推定し、「しきい値より大きい確率」と「標本の中央90%が収まる範囲」を知りたい問題です。平均だけでは足りず、全標本からしきい値超過を数え、一部の標本は後で並べ替えたいとします。

同じ `sample` 設定で複数回呼べば、warmupや採取間隔の進捗、統計が引き継がれます。各呼出しの `Runtime.samples` はその回の採取数、`moments().count` は現在の採取区間全体の累計です。保存容量を超えた古い標本は消えますが、標準統計には蓄積済みの値が残ります。

コードはhookで全採取値の超過率を集計し、直近1000標本を取り出して5%点・95%点を求めます。この区間は潜在値の不確かさの要約であり、将来の観測ノイズや推定誤差の保証は含みません。4種類の結果構造体をstderrへそのまま出力する例も兼ねます。

**入力**：`observation sigma threshold chunks steps_per_chunk`。sigma>0、chunks≥1、steps_per_chunk>200を推奨。真値は−100〜100、事前は平均0・標準偏差10。

**出力**：「採取総数 平均 超過率」、次行「保存数 保存値の5%点 95%点」。デバッグ情報はstderr。

**ファイル名**：`examples/15_sampling_and_debug.cpp`

**コード**

```cpp
// 採取を分割して続け、しきい値超過率と予測区間を調べる
// 入力: observation sigma threshold chunks steps_per_chunk。sigma>0、chunks≥1、
// steps_per_chunk>200を推奨。真値は−100〜100、事前は平均0・標準偏差10。
// 出力: 「採取総数 平均 超過率」、次行「保存数 保存値の5%点 95%点」。デバッグ情報はstderr。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 継続採取・保存・hook・結果表示をまとめて使う。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 真値1個を推定する。
using LE = LinearEstimator<>;

int main() {
    double observed, sigma, threshold;
    int chunks;
    int64_t steps;

    if (!(std::cin >> observed >> sigma >> threshold >> chunks >> steps)) {
        return 0;
    }

    // 有限の物理範囲を指定する。
    LE model({LE::Domain::real(-100, 100)}, 47);

    // 事前情報。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(0, 10));

    // 独立な1回の測定。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(observed, sigma));

    // 呼出し間で同じ採取設定を維持する。
    LE::SampleParam p;

    // 時間予算だけを変えて再実行することもできる。
    p.time_limit_us = -1;

    // 今回1回分の試行上限。全chunks合計の上限ではない。
    p.max_steps = steps;

    // このモデルでは最初の呼出しから通算して200試行をwarmupにする。
    p.warmup_steps = 200;

    // warmup後、2試行ごとに1標本を集める。
    p.sample_stride = 2;

    // 直近1000組だけを保持する。
    p.sample_capacity = 1'000;

    int64_t count = 0, above = 0;
    bool first = true;

    // ビューの寿命はhook中だけとして扱う。
    auto sample_hook = [&](const LE::SampleView& view) {
        ++count;

        // 各標本でしきい値を判定し、非線形な確率を求める。
        above += view.values[0] > threshold;

        // SampleViewには配列とenergyの見やすい表示がある。
        if (first) {
            std::cerr << view << '\n';
        }

        first = false;
    };

    // 進捗フックで受け取った最後の値をコピーして保持する。
    LE::Runtime last_debug;

    // 繰り返し呼ばれるため、重い処理や毎回の出力は避ける。
    auto debug_hook = [&](const LE::Runtime& progress) {
        // ここでmodel.sampleやモデル更新を再入させない。
        last_debug = progress;
    };

    for (int k = 0; k < chunks; ++k) {
        // 同じモデル・採取設定なら統計もwarmupも継続。
        const auto runtime = model.sample(p, sample_hook, debug_hook);

        // Runtimeは今回の呼出し分。elapsed_usは環境依存。
        std::cerr << runtime << '\n';
    }

    // debug hookで最後に受け取った実行情報を表示。
    std::cerr << last_debug << '\n';

    // 全採取値の統計。count=0のとき配列は読まない。
    const auto& statistics = model.moments();

    // Moments表示はcount=0ならmean/varianceをn/aと表示する。
    std::cerr << statistics << '\n';

    // こちらは保存中の最大1000標本だけの統計。
    const auto prediction = model.predict(LE::LinearForm{{{0, 1}}, 0});

    // Predictionもcout/cerrへ直接渡せる。自動改行はない。
    std::cerr << prediction << '\n';

    // 各集計の標本数を確認する。
    if (count == 0 || statistics.count == 0 || model.stored_sample_count() == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    std::vector<double> retained;

    // 保存標本は古い順に0始まりで取り出す。
    for (int i = 0; i < model.stored_sample_count(); ++i) {
        // 借用spanを保存せず、必要な値をコピーする。
        retained.push_back(model.stored_sample(i)[0]);
    }

    std::sort(retained.begin(), retained.end());
    const size_t low = (retained.size() - 1) / 20;
    const size_t high = (retained.size() - 1) * 19 / 20;

    std::cout << std::fixed << std::setprecision(6);

    // 全標本の集計。
    std::cout
        << statistics.count << ' '
        << statistics.mean[0] << ' '
        << static_cast<double>(above) / static_cast<double>(count) << '\n';
    std::cout << retained.size() << ' ' << retained[low] << ' ' << retained[high] << '\n';
}
```

**入力例**

```text
2 1 2 3 5000
```

**実行結果（標準出力）**

```text
7400 1.954900 0.483919
1000 0.321853 3.535092
```

### 3.16 グループ検査から、不良品の位置を推定する

複数の部品について、個別ではなくグループごとの「不良品の個数」を誤差付きで測れます。不良品は少ないと分かっているため、その情報も使って位置を推定します。

変数は各部品が良品なら0、不良品なら1です。あるグループの不良品数は、対応する0/1変数の和になります。グループ検査をGaussian因子で登録し、各変数に0へ近づけるLaplace因子を付けます。0/1だけの場合、`laplace(0, 1/penalty)` のコストは不良品1個につきちょうど `penalty` です。

このように推定対象が連続量でなくても、既知の一次式の合計が観測されるなら扱えます。検査結果が本来0/1の「一つでも不良なら陽性」しか返さない場合は、この個数モデルをそのまま使わず、検査の確率に合うcustom因子が必要です。

**入力**：`n m sigma penalty`、続くm行が `k id0 ... id(k-1) measured_count`。n≥1、sigma>0、penalty>0、各グループ内のIDは重複なし。

**出力**：ID順の0/1推定。不良品の位置で1。

**ファイル名**：`examples/16_binary_defects.cpp`

**コード**

```cpp
// グループ検査から、不良品の位置を推定する
// 入力: n m sigma penalty、続くm行が k id0 ... id(k-1) measured_count。n≥1、sigma>0、
// penalty>0、各グループ内のIDは重複なし。
// 出力: ID順の0/1推定。不良品の位置で1。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 整数0〜1を二値状態として扱う。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数j=部品jが不良なら1、良品なら0。
using LE = LinearEstimator<>;

int main() {
    int n, m;
    double sigma, penalty;

    if (!(std::cin >> n >> m >> sigma >> penalty)) {
        return 0;
    }

    // 二値の値域を明示する。
    LE model(std::vector<LE::Domain>(n, LE::Domain::integer(0, 1)), 53);

    for (int j = 0; j < n; ++j) {
        // 不良品1個につきpenaltyを加算する。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::laplace(0, 1 / penalty));
    }

    for (int i = 0; i < m; ++i) {
        int k;
        std::cin >> k;
        std::vector<int> ids(k);

        for (int& id : ids) {
            std::cin >> id;
        }

        double measured;
        std::cin >> measured;

        // グループ内の不良品数を観測。
        model.add_factor(LE::LinearForm::sum(ids), LE::Loss::gaussian(measured, sigma));
    }

    // 二値の組合せを探索する。
    LE::OptimizeParam p;

    // 試行数で制限する。
    p.time_limit_us = -1;

    // 最適性の保証はないため、実戦では予算と入力規模に合わせる。
    p.max_steps = 40'000;

    // 1変数の0/1候補を両方評価できる。
    p.enumeration_limit = 2;

    // 観測への適合と不良数の少なさを両立する候補を探す。
    model.optimize(p);

    // 候補の確認。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 平均ではなく、実際に0/1の候補を得る。
    const auto x = model.best();

    std::cout << std::fixed << std::setprecision(0);

    for (int j = 0; j < n; ++j) {
        std::cout << (j ? " " : "") << x[j];
    }

    std::cout << '\n';
}
```

**入力例**

```text
4 4 0.2 1
2 0 1 1
2 1 2 1
2 2 3 1
1 0 0
```

**実行結果（標準出力）**

```text
0 1 0 1
```

### 3.17 多数の特徴から、影響の小さい係数を0付近へ寄せる

実行時間を、作業量・移動量・積み替え回数など既知の特徴の足し合わせで近似します。特徴は多いものの、実際に効くものは少ないという前提です。測定に合わせるだけでは、情報の少ない係数が大きくぶれることがあります。

各未知係数に0中心のLaplace因子を置きます。これは係数の絶対値に比例する罰点なので、重要でない係数を0付近へ押し戻します。測定側のGaussian因子は通常の線形回帰と同じです。例では係数の分布平均を出すので、値が厳密に0になるとは限りません。

入力 `method` は同じモデルでMetropolisとConditionalを試すための切替です。前者の局所提案の大きさは `step_scale` で調整できます。後者は正規部分を利用しますが、Laplaceを含む全モデルから常に直接引けるという意味ではありません。設定比較は同じ時間と問題で行います。

**入力**：`n m noise_sigma prior_scale method`、続くm行はn個の特徴値と観測値。sigma・scale>0。method=0はMetropolis、1はConditional。特徴の単位はそろえておく。

**出力**：特徴ID順の係数の標本平均。

**ファイル名**：`examples/17_sparse_coefficients.cpp`

**コード**

```cpp
// 多数の特徴から、影響の小さい係数を0付近へ寄せる
// 入力: n m noise_sigma prior_scale method、続くm行はn個の特徴値と観測値。sigma・scale>0。
// method=0はMetropolis、1はConditional。特徴の単位はそろえておく。
// 出力: 特徴ID順の係数の標本平均。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 観測と、係数への事前コストを別々の因子として登録する。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数jは特徴jの未知係数。
using LE = LinearEstimator<>;

int main() {
    int n, m, method;
    double sigma, prior_scale;

    if (!(std::cin >> n >> m >> sigma >> prior_scale >> method)) {
        return 0;
    }

    // 係数は負も許す。
    LE model(std::vector<LE::Domain>(n, LE::Domain::real()), 59);

    for (int j = 0; j < n; ++j) {
        // 小さいscaleほど0付近への罰点が強い。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::laplace(0, prior_scale));
    }

    for (int i = 0; i < m; ++i) {
        // 既知特徴との内積を表す一次式。
        LE::LinearForm form;

        for (int j = 0; j < n; ++j) {
            double feature;
            std::cin >> feature;

            // 変数IDと既知係数を1項ずつ追加する。
            form.terms.push_back({j, feature});
        }

        double y;
        std::cin >> y;

        // 1行の測定を1因子にする。
        model.add_factor(form, LE::Loss::gaussian(y, sigma));
    }

    // 係数の分布を採取する。
    LE::SampleParam p;

    // この例では固定試行予算。
    p.time_limit_us = -1;

    // 高次元・強い相関には追加の検証が必要。
    p.max_steps = 100'000;

    // warmup後の値だけを統計に入れる。
    p.warmup_steps = 2'000;

    // 更新方式を明示して比較できる。
    p.method = method == 0 ? LE::Method::Metropolis : LE::Method::Conditional;

    // 局所提案幅の倍率。0.5や2と比較するなら入力条件もそろえる。
    p.step_scale = 1;

    // この比較例では単独変数更新の挙動を見やすくする。
    p.auto_directions = false;

    // collect_momentsの既定値trueで平均・分散を蓄積。
    model.sample(p);

    // 出力は最良候補ではなく標本平均。
    const auto& statistics = model.moments();

    // 空の統計は使わない。
    if (statistics.count == 0) {
        std::cout << "NO_SAMPLES\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(6);

    // 平均は厳密な0になる保証がない。
    for (int j = 0; j < n; ++j) {
        std::cout << (j ? " " : "") << statistics.mean[j];
    }

    std::cout << '\n';
}
```

**入力例**

```text
3 5 0.2 1 0
1 0 0 2
0 1 0 0
0 0 1 0
1 1 0 2
1 0 1 2
```

**実行結果（標準出力）**

```text
1.980580 0.007900 0.010044
```

### 3.18 不足を超過より重く罰して、生産量を決める

生産量xを決める際、需要targetより不足すると困る一方、作りすぎも避けたいとします。さらに、もともとの生産計画planから変更するにも負担があります。

需要からのずれをscaleで割り、二乗の半分を基本罰点にします。不足時だけこの罰点を `shortage_weight` 倍にします。これをcustom因子として登録し、計画からのずれはGaussian因子で評価します。入力例では需要10、計画4、不足の重み4なので、最良値は両者の間の8.8です。

これは利用者が設計した**意思決定用のコスト**です。そのまま `sample` すれば `exp(-cost)` に対応する分布になりますが、それだけで現実の需要確率を表したことにはなりません。ここでは `optimize` でコストが小さい生産量を選びます。

**入力**：`target scale shortage_weight plan plan_sigma upper`。scale・plan_sigma・shortage_weight・upperは正。生産量は0〜upper。

**出力**：選ぶ生産量と合計コスト。

**ファイル名**：`examples/18_asymmetric_cost.cpp`

**コード**

```cpp
// 不足を超過より重く罰して、生産量を決める
// 入力: target scale shortage_weight plan plan_sigma upper。
// scale・plan_sigma・shortage_weight・upperは正。生産量は0〜upper。
// 出力: 選ぶ生産量と合計コスト。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 独自の罰点も、一次式1つを受け取る関数として登録できる。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

struct ShortageCost {
    double target, scale, weight;

    // zは生産量。この例のcustom IDは0だけ。
    double operator()(int id, double z) const {
        (void)id;
        const double error = (z - target) / scale;

        // 不足と超過で重みを変える。
        return 0.5 * error * error * (error < 0 ? weight : 1);
    }
};

// パラメータを持つ評価器を所有する型。
using LE = LinearEstimator<ShortageCost>;

int main() {
    double target, scale, weight, plan, plan_sigma, upper;

    if (!(std::cin >> target >> scale >> weight >> plan >> plan_sigma >> upper)) {
        return 0;
    }

    // 生産可能範囲と評価器を指定。
    LE model({LE::Domain::real(0, upper)}, 61, ShortageCost{target, scale, weight});

    // 需要の不足・超過コスト。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::custom(0));

    // 計画から変える負担を追加。
    model.add_factor({{{0, 1}}, 0}, LE::Loss::gaussian(plan, plan_sigma));

    // 最小コストの意思決定を求める。
    LE::OptimizeParam p;

    // 回数予算。
    p.time_limit_us = -1;

    // 小さい例向けの上限。
    p.max_steps = 20'000;

    // この例では凸なコストを改善優先で探索する。
    p.temperature_start = 0;

    // 全期間で温度0。
    p.temperature_end = 0;

    // optimizeにも進捗hookを渡せる。
    auto debug = [](const LE::Runtime& runtime) {
        // 実戦ではここで軽い計数を行える。モデルの変更・再実行は禁止。
        (void)runtime;
    };

    // 戻り値を使わず、最良値だけを後で取り出す例。
    model.optimize(p, debug);

    // 有効な候補があるかを確認。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    std::cout << std::fixed << std::setprecision(6);

    // 量と、そのコストを出力する。
    std::cout << model.best()[0] << ' ' << model.best_energy() << '\n';
}
```

**入力例**

```text
10 1 4 4 1 20
```

**実行結果（標準出力）**

```text
8.800000 14.400000
```

### 3.19 変更前に候補の実行可能性を調べ、古い制約を取り消す

2種類の資材の個数A・Bは既に正確に分かっています。それなのに台帳の古い合計が一致しない場合、何をどう探索しても、その3条件を同時には守れません。この小さな例で、候補の検査と停止理由の読み方を確かめます。

A・BはFixed、台帳は和へのハード等式です。`has_solution()` は現在保持する候補が全条件に合うか、`evaluate()` は利用者が渡した候補のコストを調べます。条件違反なら後者は正の無限大です。`time_limit_us=0` の呼出しは探索せず、状態も統計も進めません。

台帳が古いと確認できた場合だけ因子を無効化します。すると固定値の候補が有効になり、動かす変数はないので停止理由は `NoMovableVariable` になります。一般問題での `NoFeasibleState` は、不可能性の証明ではなく「実行可能候補を保持していない」という報告です。この例の矛盾は固定値と合計を直接比較すれば証明できます。

**入力**：`known_a known_b recorded_total`。各値は0〜100の整数。

**出力**：「値域内か 制約まで満たすか 候補の全コスト」、探索しない呼出しの停止理由、取消後の停止理由と固定値。

**ファイル名**：`examples/19_feasibility.cpp`

**コード**

```cpp
// 変更前に候補の実行可能性を調べ、古い制約を取り消す
// 入力: known_a known_b recorded_total。各値は0〜100の整数。
// 出力: 「値域内か 制約まで満たすか 候補の全コスト」、探索しない呼出しの停止理由、
// 取消後の停止理由と固定値。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 失敗時にbest/currentへ進まない使い方を示す。
#include "linear_estimator_v09.hpp"
#include <iostream>

// 固定値の整合性確認にも利用できる。
using LE = LinearEstimator<>;

int main() {
    int a, b, recorded;

    if (!(std::cin >> a >> b >> recorded)) {
        return 0;
    }

    // 候補が許される整数範囲かを調べるための値域。
    const auto integer_domain = LE::Domain::integer(0, 100);

    // 値域だけの判定で、合計制約は見ない。
    const bool inside = integer_domain.contains(a) && integer_domain.contains(b);

    // 正確に既知なので探索変数として動かさない。
    LE model({LE::Domain::fixed(a), LE::Domain::fixed(b)});

    // 台帳の合計制約。
    const int total_id = model.add_factor(
        {{{0, 1}, {1, 1}}, 0},
        LE::Loss::hard_interval(recorded, recorded)
    );
    const std::array<double, 2> candidate{static_cast<double>(a), static_cast<double>(b)};

    // 制約外ならコストはinf。
    std::cout
        << inside << ' '
        << model.has_solution() << ' '
        << model.evaluate(candidate) << '\n';

    // 予算0では探索せず現状の実行情報だけを受け取る。
    LE::OptimizeParam zero;

    // どちらかの予算0で、状態・乱数・統計を進めない。
    zero.time_limit_us = 0;

    // 不整合があれば停止理由はNoFeasibleStateになる。
    const auto before = model.optimize(zero);

    // この0予算呼出しの理由を読む。
    std::cout
        << (before.stop_reason == LE::StopReason::NoFeasibleState
                ? "NoFeasibleState"
                : "TimeLimit") << '\n';

    // 台帳が古いと確認済み、という前提でだけ取り消す。
    model.set_enabled(total_id, false);

    // 取消後のモデルを実行する。
    LE::OptimizeParam p;

    // 有限の回数予算と組み合わせる。
    p.time_limit_us = -1;

    // 全変数Fixedなら探索する前に終了する。
    p.max_steps = 10;

    // NoMovableVariableでも有効候補は存在し得る。
    const auto after = model.optimize(p);

    // 停止理由だけでなく候補の有無を確認する。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 今回は固定状態なのでcurrentとbestは同じ。通常は同じとは限らない。
    const auto x = model.current();

    // 全固定なら標本は採取されない。
    std::cout
        << (after.stop_reason == LE::StopReason::NoMovableVariable
                ? "NoMovableVariable"
                : "Other") << ' '
        << x[0] << ' '
        << x[1] << '\n';
}
```

**入力例**

```text
3 4 10
```

**実行結果（標準出力）**

```text
1 0 inf
NoFeasibleState
NoMovableVariable 3 4
```

### 3.20 外部の観測表を共有する独自評価関数を、ターン間で更新する

2台の装置の設定を、共通の目標値へ近づけます。目標を下回る場合は2倍重く罰し、さらに設定を0から大きく動かす負担もあります。次のターンで共通目標が変更されるため、custom評価器が外部の表を参照する構成にします。

外部表の値を書き換えただけでは、推定器内部のコストの記録は更新されません。共通目標を使う**2つの因子の両方**へ `set_loss` で通知します。同じcustom IDを使っていても、因子ごとのIDへ通知する必要があります。通知中には探索・解取得を行わず、全通知が完了してから再実行します。

`shared_ptr` を使い、推定器が存続する間は参照先が消えないようにします。通常の値コピーで評価器を渡した場合、呼出し元のコピーだけを変更してもモデルには反映されません。この例の共有は意図的な設計です。

**入力**：`old_target new_target`。両目標は0〜20。設定範囲は0〜20。

**出力**：旧目標での2設定、次行に新目標での2設定。

**ファイル名**：`examples/20_custom_updates.cpp`

**コード**

```cpp
// 外部の観測表を共有する独自評価関数を、ターン間で更新する
// 入力: old_target new_target。両目標は0〜20。設定範囲は0〜20。
// 出力: 旧目標での2設定、次行に新目標での2設定。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 外部データ変更を通知してコストのキャッシュを更新する。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>
#include <memory>

struct SharedTargetCost {
    std::shared_ptr<double> target;

    // 同じID・zには、モデルを変えない間は同じコストを返す。
    double operator()(int id, double z) const {
        (void)id;
        const double d = z - *target;

        // 目標を下回るずれを2倍重く評価。
        return 0.5 * d * d * (d < 0 ? 2 : 1);
    }
};

// 評価器自体はコピーされても、目標の参照先は共有される。
using LE = LinearEstimator<SharedTargetCost>;

int main() {
    double old_target, new_target;

    if (!(std::cin >> old_target >> new_target)) {
        return 0;
    }

    auto target = std::make_shared<double>(old_target);

    // shared_ptrで参照先の寿命を確保。
    LE model({LE::Domain::real(0, 20), LE::Domain::real(0, 20)}, 67, SharedTargetCost{target});
    std::array<int, 2> factor_ids{};

    for (int j = 0; j < 2; ++j) {
        // 大きい設定を避けるコスト。
        model.add_factor({{{j, 1}}, 0}, LE::Loss::gaussian(0, 1));

        // 同じcustom ID 0を2因子が共有。
        factor_ids[j] = model.add_factor({{{j, 1}}, 0}, LE::Loss::custom(0));
    }

    // 両ターンで同じ設定を使う。
    LE::OptimizeParam p;

    // 回数制限にする。
    p.time_limit_us = -1;

    // 例用の試行数。
    p.max_steps = 20'000;

    // この凸な目的関数では改善優先で試す。
    p.temperature_start = 0;

    // 全期間で温度0。
    p.temperature_end = 0;

    std::cout << std::fixed << std::setprecision(6);

    auto run = [&]() {
        // 全因子の更新通知が終わってから呼ぶ。
        model.optimize(p);

        // 候補を確認してから参照する。
        if (!model.has_solution()) {
            std::cout << "NO_SOLUTION\n";
            return;
        }

        // 次のモデル変更までに読み終える。
        const auto x = model.best();
        std::cout << x[0] << ' ' << x[1] << '\n';
    };

    run();
    *target = new_target;

    for (int factor_id : factor_ids) {
        // 影響する全factor IDへ通知する。同じLossでも通知になる。
        model.set_loss(factor_id, LE::Loss::custom(0));
    }

    run();
}
```

**入力例**

```text
6 12
```

**実行結果（標準出力）**

```text
4.000000 4.000000
8.000000 8.000000
```

### 3.21 実数・整数・規格候補・既知値を同じ費用式で推定する

料金が「起動料金＋個数×単価＋追加作業の有無×追加料金＋既知の手数料」で決まるサービスを考えます。起動料金は小数を許す実数、単価は整数、追加料金はカタログの3候補、手数料は正確な既知値です。

4変数をそれぞれReal・Integer・Values・Fixedにします。各観測の個数と追加作業の有無を係数にし、同じ一次式へまとめられます。異なる種類の変数を**一つのモデルへ混在させることは可能**です。ただし `set_directions` に渡す一つの同時方向の中ではRealとIntegerを混ぜられず、ValuesとFixedも対象外です。この例は方向を利用者から指定しません。

入力例では起動3、単価4、追加5、手数料2なので、個数8・追加ありの料金は42です。測定条件を変えて、各料金を区別できる観測を集める必要があります。

**入力**：`m known_fee sigma query_quantity query_extra`、続くm行が `quantity extra observed_charge`。extraは0/1、個数は非負、sigma>0。

**出力**：起動料金・単価・追加料金・手数料、次行に次の依頼の料金予測。

**ファイル名**：`examples/21_mixed_domains.cpp`

**コード**

```cpp
// 実数・整数・規格候補・既知値を同じ費用式で推定する
// 入力: m known_fee sigma query_quantity query_extra、
// 続くm行が quantity extra observed_charge。extraは0/1、個数は非負、sigma>0。
// 出力: 起動料金・単価・追加料金・手数料、次行に次の依頼の料金予測。
// 入力は上記の条件を満たす前提。各実行例はこのファイルだけで完結する。

// 4種類の値域はモデル内で混在できる。
#include "linear_estimator_v09.hpp"
#include <iomanip>
#include <iostream>

// 変数0=起動、1=単価、2=追加、3=手数料。
using LE = LinearEstimator<>;

int main() {
    int m, query_extra;
    double fee, sigma, query_quantity;

    if (!(std::cin >> m >> fee >> sigma >> query_quantity >> query_extra)) {
        return 0;
    }

    // 各変数の意味と順序をここで固定する。
    std::vector<LE::Domain> domains;

    // 起動料金は小数を許す。
    domains.push_back(LE::Domain::real(0, 20));

    // 単価は整数。
    domains.push_back(LE::Domain::integer(0, 10));

    // 追加料金はこの3規格に限る。
    domains.push_back(LE::Domain::values({0, 5, 10}));

    // 手数料は既知。変数IDを持つが探索では動かない。
    domains.push_back(LE::Domain::fixed(fee));

    // 種類の異なる値域列をそのまま渡せる。
    LE model(domains, 71);

    for (int i = 0; i < m; ++i) {
        double quantity, observed;
        int extra;
        std::cin >> quantity >> extra >> observed;

        // 料金の足し合わせ。
        LE::LinearForm form{
            {
                {0, 1},
                {1, quantity},
                {2, static_cast<double>(extra)},
                {3, 1}
            },
            0
        };

        // 請求全体の測定誤差を指定する。
        model.add_factor(form, LE::Loss::gaussian(observed, sigma));
    }

    // 具体的な料金パラメータの組を求める。
    LE::OptimizeParam p;

    // 回数予算にする。
    p.time_limit_us = -1;

    // 混合問題では一般に最適性保証はない。
    p.max_steps = 60'000;

    // 専用の混合方向を指定しなくても探索できる。
    model.optimize(p);

    // 候補の確認。
    if (!model.has_solution()) {
        std::cout << "NO_SOLUTION\n";
        return 0;
    }

    // 離散部分も値域内の具体的な候補として得られる。
    const auto x = model.best();

    // 次の料金式。
    const LE::LinearForm query{
        {
            {0, 1},
            {1, query_quantity},
            {2, static_cast<double>(query_extra)},
            {3, 1}
        },
        0
    };

    std::cout << std::fixed << std::setprecision(6);

    for (int j = 0; j < 4; ++j) {
        std::cout << (j ? " " : "") << x[j];
    }

    // 観測ノイズを加える前の予測料金。
    std::cout << '\n' << query.evaluate(x) << '\n';
}
```

**入力例**

```text
4 2 0.1 8 1
0 0 5
2 0 13
0 1 10
3 1 22
```

**実行結果（標準出力）**

```text
3.000000 4.000000 5.000000 2.000000
42.000000
```


### 3.22 公開APIとサンプルの対応

| API群 | 使用例 |
|---|---|
| コンストラクタ、seed、CustomLossの指定 | 全例。customは3.7・3.11・3.12・3.18・3.20 |
| Domain::real / integer / values / fixed | 3.2 / 3.1 / 3.7 / 3.8。全種類の混在は3.21 |
| Domain::contains | 3.19 |
| Term、LinearFormのterms・offset | 3.2・3.17・3.21 |
| LinearForm::sum / difference / evaluate | 3.1 / 3.4 / 3.2・3.10・3.11・3.21 |
| Loss::gaussian / laplace / student_t | 3.1 / 3.6・3.16・3.17 / 3.6 |
| Loss::gaussian_interval | 3.4・3.5 |
| Loss::hard_interval / soft_interval / evaluate | 3.9・3.10・3.19 / 3.10 / 3.10 |
| Loss::customと独自評価器 | 3.7・3.11・3.12・3.18・3.20 |
| add_factor | 全例 |
| set_loss / replace_factor / set_enabled / set_domain | 3.14。customの更新通知は3.20、取消後の実行可能性は3.19 |
| set_state / set_directions | 3.9。実数初期値は3.10・3.12 |
| OptimizeParam、optimize | 3.1・3.2・3.6・3.8〜3.10・3.13・3.14・3.16・3.18〜3.21 |
| SampleParam、sample | 3.3〜3.5・3.7・3.11・3.12・3.15・3.17 |
| Method::Automatic / Metropolis / Conditional / Slice | 省略時の既定 / 3.17 / 3.9・3.17 / 3.5・3.12 |
| 温度・greedy_probability | 3.6・3.13・3.18・3.20 |
| auto_directions・direction_probability・enumeration_limit・step_scale | 3.7・3.9・3.16・3.17 |
| warmup_steps・sample_stride・collect_moments・sample_capacity | 3.3・3.7・3.11・3.15 |
| has_solution / best / best_energy / current / evaluate | 3.1ほか / 3.1ほか / 3.1・3.18 / 3.19 / 3.10・3.19 |
| moments / stored_sample_count / stored_sample / predict | 3.4ほか / 3.15 / 3.15 / 3.3・3.15 |
| sample hook・SampleView | 3.7・3.11・3.15 |
| optimizeのdebug hook / sampleのdebug hook | 3.18 / 3.15 |
| Runtime、StopReasonの読み取り | 3.1・3.15・3.19 |
| Runtime / SampleView / Moments / Prediction のoperator<< | 3.15。Runtimeは3.1でも使用 |
| 予算0、実行可能候補なし、全固定 | 3.19 |
| sampleの分割継続、統計と保存の違い | 3.15 |

参照表には全公開型・フィールド・列挙値・メソッド・結果出力を記載しています。サンプルでは各APIの代表的な使用を網羅し、フィールドは値を読む場面・設定する場面に合わせて使っています。設定値の全組合せや、停止理由をすべて確実に発生させるデモを意味するものではありません。

### 3.23 検証と配布物

次の確認を行いました。検証環境はGCC 13.3.0、C++20です。

- **21本すべて**を `-O2 -Wall -Wextra -Wpedantic -Werror` 付きで独立にコンパイルし、掲載入力で実行。
- 整数・混合値域・保存則・既知の最適値を確認。有限候補の確率は全組合せの独立計算と比較し、相対誤差の例は解析的な平均とも比較。
- **追加8シナリオ**を実行。Laplaceへの切替、Conditionalへの切替、観測の不整合、warmupのみ、整合する固定値、各変数1候補、比較結果の反転、下端での表示打切りを確認。
- Markdown内の21本のコードと、実行した `.cpp` が同一であることを確認。入力・出力の掲載内容も同梱ファイルと突き合わせ。
- 数式のMarkdown解析と、API使用行の日本語コメント、公開APIの記載を確認。

配布物の内容は次のとおりです。

| ファイル・フォルダ | 内容 |
|---|---|
| `linear_estimator_samples_v09.md` | この文書 |
| `linear_estimator_v09.hpp` | 本書が対象とするヘッダー |
| `examples/` | 21本の単独実行プログラム |
| `input/` | 各プログラムの入力例 |
| `output/` | 検証環境で得た標準出力 |
| `verify_samples.py` | ビルド・実行・数値的な確認。Python標準ライブラリとg++が必要。文書は更新しない |
| `verify_document.py` | 文書とコード・入出力・API記載の照合。数式解析はpandocがある場合に実施。文書は更新しない |
| `verification.json` / `document_verification.json` | 今回の検証結果 |
| `SHA256SUMS` | 内容確認用のハッシュ |

ZIPを展開したフォルダで `python3 verify_samples.py` を実行すると全例を再検証します。このスクリプトは `output/` と検証結果をその実行環境の結果で書き直しますが、Markdown・C++ソースを変更しません。掲載済み出力との厳密な文字列一致が必要な場合は、同じ検証環境を使ってください。

各例は独立したプログラムです。利用側の入力サイズや確率モデルを変えると必要な探索量も変わります。掲載した小規模例の成功は、任意の入力での収束や最適性を保証するものではありません。
