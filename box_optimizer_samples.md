# BoxOptimizer サンプルコード集

対象ソースは`box_optimizer_v06.hpp`です。このファイルの公開APIに基づき、問題の説明、APIリファレンス、入力と出力を備えた28個の独立したコード例をまとめます。

数学の前提は、高校で扱う関数、二乗、三角関数、指数・対数、微分です。複数変数の微分は「ほかの変数を止めたときの傾き」から説明します。コードはC++20を使用します。

## 1. 何を解くライブラリか？

### 1.1 設定値を変えて、悪さを小さくする

機械の出力、座標、配分量、予測式の係数などを調整したいとします。設定値を決めると、「目標とのずれ」「移動距離」「費用」などの悪さを計算できるなら、その悪さをできるだけ小さくする問題として扱えます。

BoxOptimizerが扱う基本形は次のとおりです。添字はC++に合わせて0から始めます。

$$\min_ {x} f(x),\qquad x=(x_ {0},x_ {1},\ldots,x_ {n-1}),\qquad l_ {i}\le x_ {i}\le u_ {i}\quad(i=0,\ldots,n-1)$$

| 記号 | 意味 |
|---|---|
| $n$ | 調整する設定値の個数 |
| $i$ | 設定値の番号。0から$n-1$まで |
| $x_ {i}$ | $i$番目の設定値。小数を含む実数として扱う |
| $x$ | 全設定値を順番に並べたもの。コードでは配列として受け渡す |
| $l_ {i}$ | $i$番目の下限 |
| $u_ {i}$ | $i$番目の上限。下限と同じ値にもできる |
| $f$ | 設定値の配列を受け取り、悪さを1個の数値で返す目的関数 |
| $f(x)$ | その設定を使った場合の目的値。小さいほど良い |
| $\min$ | 制約を守る設定の中から、右側の値をできるだけ小さくするという意味 |

各設定に独立した下限・上限を付ける条件を**箱制約**と呼びます。2変数なら長方形、3変数なら直方体に相当します。下限と上限が同じ成分は、その値に固定されます。

### 1.2 目的関数の具体例

出力$x$を0〜10で調整し、目標$t=3$へ近づけたいなら、ずれの二乗を目的関数にできます。

$$f(x)=(x-t)^2,\qquad t=3,\qquad 0\le x\le 10$$

$x$が調整する出力、$t$が利用者の決めた目標、$f(x)$が悪さです。$x=2$でも$x=4$でも悪さは1、$x=3$なら0です。$t$は探索する変数ではなく、探索中は一定の入力データです。

2つの出力を調整し、合計を$S$へ、差を$D$へ近づけたい場合は、例えば次のようにします。

$$f(x_ {0},x_ {1})=(x_ {0}+x_ {1}-S)^2+4(x_ {0}-x_ {1}-D)^2$$

$x_ {0},x_ {1}$が調整する2つの出力、$S$が合計の目標、$D$が差の目標です。係数4は、同じ大きさのずれなら差の誤差を4倍重く数える指定です。どちらの出力を変えても両方の項に影響するため、2変数をまとめて考えます。

ライブラリへ渡すのは式の文字列ではありません。「候補を受け取り、その式の値を計算して返すC++の関数」です。関数の中では、入力データ、グラフ、固定したシミュレーションなども参照できます。

### 1.3 返される結果の意味

返されるのは、**実際に評価した候補の中で、目的値が最も小さかった点**です。一般の目的関数について、全範囲で最良の点を必ず得る保証や、最適性の証明はありません。目的値が同じ候補が複数あれば、先に最良値を得た点が残ります。

十分な計算予算がなく、1回も評価できない結果もあります。この場合は「解なし」として扱い、配列を読む前に`has_value()`で確認します。

同じ候補には同じ目的値を返すことが条件です。各回で新しい乱数を引く評価には、そのまま使いません。状況をあらかじめ固定して平均や最悪値を計算する形を、ユースケースで示します。

### 1.4 表現できるものと、利用者側で用意するもの

| したいこと | このライブラリでの表し方 |
|---|---|
| 実数の座標や係数を調整する | その値を変数にして上下限を指定する |
| 一部の値を固定する | その成分の下限と上限を同じにする |
| 得点を大きくする | 得点にマイナスを付けて、小さくする目的へ変える |
| 最終的に整数を出す | 候補を丸め、実際に出力する整数を採点する |
| 合計が一定などの追加制約を守る | 箱内の変数から、制約を満たす出力へ変換する |
| 順列・集合・一般の等式や不等式を直接登録する | そのための公開APIはない。別の表現や専用の道具が必要 |

整数化や変数変換を使う場合も、ライブラリが操作するものは実数の箱です。変換後の出力が条件を満たすか、その値で何を採点しているかは、利用者が決めます。

## 2. APIリファレンス

### 2.1 公開APIの全体一覧

型・関数は名前空間を付けずに利用します。標準ライブラリの型には`std::`を付けます。サンプルでは`using namespace std;`でこの部分を短く記述しています。

| 公開されているもの | 用途 |
|---|---|
| `BoxBound` | 1変数の下限・上限 |
| `BoxProblem` | 全変数の上下限の配列 |
| `BoxBudget` | 時間上限、評価回数上限、目的値の目標 |
| `BoxParam` | 探索方針、乱数seed、初期幅、手法 |
| `BoxParam::Search` | 探索を継続するか、局所停止で終えるか |
| `BoxParam::Method` | 使用する探索手法 |
| `BoxResult` | 最良点、目的値、回数、時間、停止理由 |
| `BoxResult::Stop` | 停止理由の列挙型 |
| `BoxResult::has_value()` | 評価済みの解があるかを調べる |
| `operator<<(std::ostream&, const BoxResult&)` | 結果を`cout`・`cerr`などへ表示する |
| `BoxOptimizer`のコンストラクタ | 箱と設定を保持する探索オブジェクトを作る |
| `BoxOptimizer::reset(...)` | 箱と設定を入れ替える |
| `BoxOptimizer::solve(...)` | 新しい探索を開始する |
| `BoxOptimizer::resume(...)` | 同じ問題の探索を追加予算で続ける |
| `box_optimize(...)` | 構築から1回の探索まで一括で行う |

このほかに、目的関数の呼び出し規約があり、一引数形式と二引数形式を受け付けます。目的関数そのものは利用者が用意します。

### 2.2 `BoxBound`と`BoxProblem`

| 項目 | 型 | 初期状態・取り得る値 | 意味 |
|---|---|---|---|
| `BoxBound::lower` | `double` | 有限値。上限以下にする | 下限 |
| `BoxBound::upper` | `double` | 有限値。下限以上にする | 上限 |
| `BoxProblem::bounds` | `std::vector<BoxBound>` | 初期状態は空配列 | 変数の順に並べた上下限 |

`BoxBound b;`だけでは、2つの数値に既定値は入りません。`BoxBound{0, 10}`のように両方を明示してください。`BoxBound{}`という値初期化では両方0になりますが、これは0に固定した変数です。

`bounds.size()`が全変数数です。`{{0,10},{-5,5}}`なら2変数、1番目は0〜10、2番目は−5〜5です。両端も許されます。下限と上限が等しければ固定変数になります。空の`bounds`も有効で、「要素のない配列」という1通りの設定だけを持つ問題です。

上下限に無限大は使えません。幅やその逆数が`double`で表現できる範囲にします。

### 2.3 `BoxBudget`：1回の呼び出しで使う予算

| フィールド | 型 | 既定値 | 取り得る値・意味 |
|---|---|---|---|
| `time_limit_us` | `double` | `1'950'000` | 有限値。正ならマイクロ秒の時間上限、0なら新規評価なし、負なら時間上限なし |
| `max_evaluations` | `long long` | `LLONG_MAX` | 0以上。今回の目的関数呼び出し回数の上限 |
| `target_cost` | `std::optional<double>` | 未指定 | 指定する値は有限。最良目的値がこれ以下なら終了 |

1秒は1,000,000マイクロ秒です。既定時間は1.95秒、`2000`は2ミリ秒です。時間を負にする場合は、`max_evaluations`を`LLONG_MAX`未満にしてください。目標値だけに終了を頼り、時間も回数も無制限にする指定はできません。

目標値は、最適値との差や相対誤差ではなく、目的関数が返す数値そのものです。例えば目的関数の理論上の最小値が10なら、目標0にしても到達できません。負の目的値を使う問題では、負の目標も指定できます。

目標を外すには`budget.target_cost.reset()`または`budget.target_cost = std::nullopt`を使います。これは標準の`optional`の操作であり、探索オブジェクトの`reset`とは別です。

時間・回数・目標のうち、成立した終了条件で止まります。評価回数上限は守りますが、既に始めた目的関数を途中で中断できないため、時間には超過があり得ます。候補作成、内部更新、結果配列の作成にも時間がかかります。入出力や、利用者が直接呼んだ採点は別に見積もってください。

`resume`では、この予算は**追加の1回の呼び出し**に対する値です。100評価した後に上限200で再開すれば、最大200評価を追加し、累計は最大300になります。

### 2.4 `BoxParam`：探索方針と手法

| フィールド | 型 | 既定値 | 取り得る値・意味 |
|---|---|---|---|
| `search` | `BoxParam::Search` | `Explore` | 下表の2種類 |
| `seed` | `uint64_t` | `1` | 0を含む64bit符号なし整数。内部乱数の初期状態 |
| `initial_step` | `std::vector<double>` | 空配列 | 空なら自動。指定時は全変数数と同じ長さ |
| `method` | `BoxParam::Method` | `Auto` | 下表の4種類 |

#### `Search`の全値

| 値 | 意味 | 誤解しやすい点 |
|---|---|---|
| `BoxParam::Search::Explore` | 停滞後も再始動や探索区間の切替を使い、残り予算で探索を続ける | 全範囲の最適点を必ず見つける保証ではない |
| `BoxParam::Search::Refine` | 初期解から局所的に改善し、その探索の停止条件で終了する | 初期解から一定距離以内だけ動く指定ではない。範囲を制限するなら箱を狭める |

`search`は「停滞した後の方針」、`method`は「調べ方」の指定です。別の設定として組み合わせます。

#### `Method`の全値

| 値 | 動作と必要な目的関数 | 利用者が押さえる点 |
|---|---|---|
| `BoxParam::Method::Auto` | 一引数形式なら座標探索から開始。Exploreでは停滞後に焼きなまし・対角CMA系も使用。二引数形式なら自由変数があれば投影L-BFGS型の勾配経路 | 目的関数の引数形式で経路が決まる。任意の関数から自動で勾配を求めるわけではない |
| `BoxParam::Method::Coordinate` | 1成分ずつの変更を中心に調べる。勾配は使わない | 二引数形式を渡しても勾配配列は空 |
| `BoxParam::Method::DiagonalCma` | 複数の候補の成績から、中心と座標ごとの広がりを調整する。勾配は使わない | 初めからこの経路を使う。全座標間の相関を扱う密なCMAと同じではない |
| `BoxParam::Method::SpectralGradient` | 直近の移動と勾配変化から歩幅を調整する勾配経路 | 自由変数がある場合は二引数形式が必須。Autoの勾配経路にある複数組の曲率履歴は持たない |

全固定・変数数0の場合は、どの手法でも予算があれば値を1回評価するだけで、勾配を要求しません。この場合だけはSpectralGradientに一引数形式を渡すこともできます。

どの手法が適するかは関数の形と評価費用によります。手法名だけで速度や品質の順位は決まりません。

#### `seed`と`initial_step`

新しい`solve`では乱数を`seed`から開始し直し、`resume`ではその乱数状態を続けます。目的関数の外側の乱数は管理しません。同じ条件で候補列を比較したい場合、時間上限より評価回数上限を使う方が、実行時間の揺れを避けられます。別のコンパイラや浮動小数点環境まで完全一致させる保証ではありません。

`initial_step[i]`は**元の変数の単位での初期幅**です。自由成分は正で箱幅以下にし、固定成分の値は使いません。ただし配列長は固定成分も含む全変数数にします。空なら、自由成分ごとに箱幅の1/4です。

初期幅は、探索中の最大移動距離や初期点からの距離制限ではありません。探索中に調整されます。勾配経路では最大の正規化初期幅が初回の尺度に使われるため、各変数を常に指定幅ずつ動かす設定とも異なります。

### 2.5 目的関数の呼び出し規約

受け付ける形は次の2つです。以下は型の説明であり、独立したサンプルプログラムではありません。

```text
double objective(std::span<const double> x)
double objective(std::span<const double> x, std::span<double> gradient)
```

| 引数・戻り値 | 意味と条件 |
|---|---|
| `x` | 固定成分を含む全変数の候補。指定した箱内・元の単位。書き換え不可 |
| `gradient`が非空 | 全変数数と同じ長さ。全成分を有限値で埋める。固定成分は0でよい |
| `gradient`が空 | 値だけを要求している。配列へアクセスしない |
| 戻り値 | 小さいほど良い有限の目的値。勾配付きでも値は`return`で返す |

**勾配**は、ほかの変数を固定して1個だけを少し動かしたときの傾きを、変数の順に並べた配列です。例えば目的値が$(x_ {0}-2)^2+4(x_ {1}+1)^2$なら、勾配は$2(x_ {0}-2)$と$8(x_ {1}+1)$です。

勾配は、目的関数へ渡された`x`の単位について計算します。箱の内部正規化への変換はライブラリが行うため、利用者がさらに箱幅を掛ける必要はありません。一方、利用者自身が対数や角度などへ変換しているなら、入力である変換後の変数についての微分を渡します。

`span`は配列の窓口で、配列を所有しません。受け取った`span`やそのデータへのポインタを保存し、次の評価後に参照してはいけません。値を残すなら自分の`vector`などへコピーします。

一引数・二引数の両方で呼べる関数オブジェクトは、二引数形式を優先します。勾配不要の経路では、その第2引数を空にして呼びます。ライブラリは自動で数値微分しません。

目的関数や捕捉したデータは、呼び出し中に有効である必要があります。関数自体はsolverへ保存されません。コピーできない関数オブジェクトも渡せます。同じsolverへ目的関数の内部から再入する使い方はできません。

最良出力の記録や回数の集計などの副作用は使えますが、それによって同じ`x`に返す値や勾配を変えないでください。未評価点を示すための無限大や`NaN`も返しません。

### 2.6 探索を呼ぶ関数と全引数

```text
explicit BoxOptimizer(BoxProblem problem, BoxParam param = {})
void BoxOptimizer::reset(BoxProblem problem, BoxParam param = {})

template<class Objective>
BoxResult BoxOptimizer::solve(Objective&& objective,
                             const BoxBudget& budget = {},
                             std::span<const double> initial = {})

template<class Objective>
BoxResult BoxOptimizer::resume(Objective&& objective,
                              const BoxBudget& budget = {})

template<class Objective>
BoxResult box_optimize(const BoxProblem& problem, Objective&& objective,
                       const BoxBudget& budget = {},
                       std::span<const double> initial = {},
                       const BoxParam& param = {})
```

| 引数 | 必須か・既定値 | 条件と意味 |
|---|---|---|
| `problem` | コンストラクタ・reset・一括関数で必須 | 前述の上下限配列。空の問題も明示的に渡せる |
| `param` | 省略可。既定の`BoxParam` | 箱を保持する際の設定。外側のparamを書き換えるだけでは既存solverに反映されない |
| `objective` | 必須 | 前節の呼び出し規約を満たす関数。`Objective`というテンプレート引数は通常自動推論される |
| `budget` | 省略可。既定の`BoxBudget` | その呼び出しの予算。省略時は約1.95秒の時間上限 |
| `initial` | 省略可。空のspan | 空なら箱の中央。非空なら全変数数と同じ長さで全成分が箱内・有限 |

初期解には`vector<double>`や`array<double,N>`などを渡せます。非空の問題に空配列を渡した場合も、「初期解未指定」として箱の中央を使います。指定した初期解は、最初の評価にそのまま渡されます。箱外の初期解を自動的に切り詰める機能はありません。

| 呼び出し | 状態の扱い | 戻り値 |
|---|---|---|
| コンストラクタ | 箱と設定を保持し、前処理する。目的関数はまだ呼ばない | 新しい`BoxOptimizer` |
| `reset` | 箱・設定を変更し、探索を未開始にする。次はsolveが必要。param省略は設定を既定値へ戻す意味 | `void` |
| `solve` | 最良値・履歴・累計評価回数・乱数を新しい探索の状態にする。作業配列の容量は再利用できる | `BoxResult` |
| `resume` | 最良値・履歴・乱数・途中候補を保持して続ける。初期解引数はない | `BoxResult` |
| `box_optimize` | 呼び出しごとにsolverを構築してsolveし、破棄する | `BoxResult` |

`resume`には、そのオブジェクトで先に`solve`を行っている必要があります。目的関数の意味、捕捉データ、箱、設定、一引数・二引数の形式も保ちます。目標や需要が変わったときは、新しい`solve`で前の解を評価し直します。箱も変わるなら先に`reset`します。

`solve`と`resume`の計測にコンストラクタや`reset`の費用は含まれません。一括関数はsolverの構築・探索・破棄まで計測します。時間制限を無効にしても、`elapsed_us`は経過時間を返します。

`BoxOptimizer`は通常のC++のコピー・代入が可能で、保持している配列は独立してコピーされます。コピーしても、利用者側の目的関数の捕捉データまで複製・固定してくれるわけではありません。

### 2.7 `BoxResult`の全フィールド

| フィールド・関数 | 型 | 初期状態 | 意味 |
|---|---|---|---|
| `x` | `std::vector<double>` | 空 | 実際に評価した最良点。固定成分を含む元の単位の所有配列 |
| `cost` | `double` | 正の無限大 | その点の目的値。評価済みなら有限 |
| `evaluations` | `long long` | 0 | 今回の目的関数呼び出し回数 |
| `gradient_evaluations` | `long long` | 0 | 今回、非空の勾配配列を渡した回数 |
| `total_evaluations` | `long long` | 0 | 直近のsolveからの累計評価回数 |
| `elapsed_us` | `double` | 0 | 今回の実測時間。マイクロ秒 |
| `stop` | `BoxResult::Stop` | `EvaluationLimit` | 停止理由。単に作った結果オブジェクトの既定値もこの値 |
| `has_value() const` | 戻り値`bool` | 初期状態ではfalse | `cost`が有限、つまり評価済み解があるか |

値と勾配を同じコールバックで計算しても、目的評価1回・勾配評価1回です。勾配評価回数は目的評価回数の内数です。利用者が検算や代替解の採点のために直接呼んだ関数は、これらの回数に入りません。

`x`は結果が所有している配列なので、次の`solve`や`resume`の後も内容を保持できます。`x.empty()`だけでは未評価か判断できません。変数数0の有効な結果も空配列だからです。

### 2.8 `BoxResult::Stop`の全値と優先順位

| 値 | 何が成立したか | 追加予算だけで進むか |
|---|---|---|
| `TargetReached` | 最良目的値が`target_cost`以下 | 同じ目標なら新規評価なし。目標を厳しくするか外せば続けられる場合がある |
| `TimeLimit` | 今回の時間上限に達した | 他の停止条件がなければresumeで続けられる |
| `EvaluationLimit` | 今回の評価回数上限に達した | 他の停止条件がなければresumeで続けられる |
| `LocalStop` | Refineの局所探索が停止した | その状態のresumeでは進まない。新しいsolveや設定・初期解の見直しを使う |
| `AllFixed` | 自由変数がなく、初回の値の評価が終わった | 同じ設定は評価し直さない。新規評価が必要なら新しいsolve |

同じ判定地点では、**目標到達、全固定、局所停止、評価回数、時間**の順に優先します。例えば最後の1評価で目標に達したら、回数上限と同時でも`TargetReached`です。全固定でも初回評価前に予算が0なら、`AllFixed`ではなく予算側の終了理由になります。

`LocalStop`は最適性の証明ではありません。また、Exploreにも目標・時間・評価回数・全固定による終了があります。

### 2.9 `operator<<`でのデバッグ出力

```text
std::ostream& operator<<(std::ostream& out, const BoxResult& result)
```

`out`は必須の出力先、`result`は必須の読み取り専用の結果です。戻り値は同じ出力先への参照なので、`cerr << result << '\n';`のようにつなげられます。

```text
BoxResult{cost=0.25, stop=EvaluationLimit, evaluations=200, gradient_evaluations=0, total_evaluations=200, elapsed_us=120.5, x=[2.5]}
```

未評価は`cost=none`、空配列は`x=[]`です。配列は先頭20要素まで表示し、多ければ`... (25 total)`のように全要素数を示します。データ本体を切り詰めません。改行は付かず、数値の精度は出力先の設定に従います。通常のデバッグには`cerr`を使うと、提出する解の標準出力へ混ざりません。

### 2.10 共通の入力条件と規模

全変数数は100,000以下、自由変数数は10,000以下、直近のsolveからの累計評価は10億回以下を想定しています。これは最大規模で短時間に良い解が得られるという保証ではありません。

上下限、初期解、目的値、要求された勾配は有限値にします。箱幅・その逆数、正規化幅の二乗、勾配の単位変換、内積なども通常の`double`で無理なく表現できる尺度にします。有限値同士でも引き算や二乗があふれる場合があるため、極端に大きい・小さい単位は利用者側で調整します。

不正入力は主に`assert`で扱います。`-DNDEBUG`で検査を外しても条件は変わりません。無効な点を無限大で採点する方式や、コールバックの例外からの探索再開は前提にしません。

## 3. ユースケース

### 3.1 コード例の実行方法と読み方

各例は、入力を`cin`から読み、結果を`cout`へ出す完全なプログラムです。例28では、デバッグ表示だけ`cerr`へ出します。入力例を対応する`.in`へ、コードを示した`.cpp`へ保存してください。全ての例で同じ`box_optimizer_v06.hpp`を使用し、`.cpp`と同じフォルダへ置きます。ヘッダは`bits/stdc++.h`を使用するため、C++20とこのヘッダを使えるGCC・libstdc++の環境を用意してください。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined example01_calibration.cpp -o example01
./example01 < example01_calibration.in
```

同梱の例集には、ヘッダ、全28個の`.cpp`、入力`.in`、実行結果`.out`、このMarkdown、検証スクリプトが入っています。全例を確認するには、展開したフォルダで`python3 verify_samples.py`を実行します。別のC++コンパイラを使う場合は`CXX`環境変数で指定できます。

単一ファイルにまとめる場合は、同梱の`amalgamate.py`でヘッダを展開できます。

```sh
python3 amalgamate.py example01_calibration.cpp > submission.cpp
g++ -std=c++20 -O2 submission.cpp -o submission
./submission < example01_calibration.in
```

ヘッダをそのまま全文貼り付けると、直接実行用のテストの`main`も有効になるため、この方法を使います。複数のサンプルの`main`を一緒に連結する用途ではありません。

各例の入力は、記載した形式・条件を満たすものとします。数値は有限とし、教材では個数は原則1〜50、通常のデータは絶対値100以下を想定します。対数変数の例、評価回数、時間、変数数0などは各例に示す条件を使います。全ての不正な入力形式を検出するための入出力パーサーではありません。

出力例はGCC 13.3、C++20、O2で実行したものです。末尾の小数や時間制限時の評価回数は環境で変わります。目的値が表示上`0.000000`でも、内部で数学的に誤差0と証明されたわけではありません。`1e-10`は$10^{-10}$の意味です。

`span`、空の勾配への対処、初期解の長さや範囲など、利用上の条件は各コードにも日本語で記しています。例に出てくる整数や係数には、入力データ、目的関数の重み、探索予算の3種類があるので、コメントで役割を区別してください。

### 3.2 用途と使用APIの対応

| 例 | 用途 | 主に使うAPI・パターン |
|---|---|---|
| 01 | 1個の出力を目標へ合わせる | box_optimize、BoxProblem、BoxBudget、has_value |
| 02 | 合計と差の両方を満たす2つの出力 | BoxOptimizer、solve、初期解、Search::Refine |
| 03 | 観測データへ直線を当てはめる | 二引数の目的関数、Autoの勾配経路 |
| 04 | 一部を固定した設定列を滑らかにする | BoxBoundの同値上下限、固定成分を含む勾配 |
| 05 | 大きなグラフの一部分だけを小さい問題として改善する | 自由成分だけのBoxProblem、初期解と結果の添字変換 |
| 06 | 単位と範囲が異なる変数の初期幅を決める | initial_step、box_optimizeの全5引数 |
| 07 | 縦横の移動距離で施設の位置を選ぶ | Method::Coordinate、一引数の目的関数 |
| 08 | 減衰する観測値へ指数曲線を当てはめる | 任意の決定的な関数、Autoの勾配なし経路 |
| 09 | 何桁も変わる正の倍率を対数で調整する | 利用者側の変数変換、変換後の勾配、target_cost |
| 10 | 複数の観測方向へ近い向きを選ぶ | Method::SpectralGradient、角度の表現 |
| 11 | 総量を守って3か所へ配分する | 箱内変数からの実行可能な出力変換 |
| 12 | 円形の領域内で目的地に近い点を選ぶ | 半径・角度への変換、勾配の連鎖 |
| 13 | 費用の最小化ではなく利益を最大化する | 負の目的値、負のtarget_cost |
| 14 | 整数へ丸めた出力を直接評価する | 丸め後の採点、Coordinate + Explore |
| 15 | 代理目的を改善しながら、本来の良い出力を保つ | コールバックでの記録、実際の最良出力の外部保持 |
| 16 | 整数の装置台数と連続的な出力を同時に調整する | 成分ごとの異なるdecode、Auto + Explore |
| 17 | 固定した複数状況の平均誤差を小さくする | 決定的なシナリオ評価、一引数の目的関数 |
| 18 | 最悪の状況での誤差を小さくする | max・絶対値を含む目的、Coordinate |
| 19 | 観測が少ないときに、大きすぎる係数を抑える | 目的への正則化項、値と勾配の整合 |
| 20 | 短い時間で改善し、評価できなければ代替解を使う | time_limit_us、has_value、box_optimize |
| 21 | 同じ探索を小分けの予算で続ける | solve、resume、evaluations、total_evaluations |
| 22 | いったん十分な解を得てから、目標を厳しくする | target_cost、TargetReached、resume |
| 23 | ターンごとに変わる目標へ、前の解から合わせ直す | 繰り返しsolve、前解を初期解へ渡す |
| 24 | 使える範囲や変数数が変わったら問題を入れ替える | reset、BoxBound、初期解のclamp |
| 25 | 全体の時間枠を複数の初期解で共有する | DiagonalCma、seed、5引数のbox_optimize |
| 26 | 同じ関数で4手法を比較する | Methodの全値、二引数形式の空勾配、seed |
| 27 | 勾配を小さな差分で検算してから使う | 空のgradientで値だけ呼ぶ、二引数solve |
| 28 | 全固定・変数数0・未評価を同じコードで扱う | AllFixed、has_value、gradient.empty、operator<< |

### 3.3 例01：1個の出力を目標へ合わせる

装置の設定が1個だけあり、希望する出力へ近づけたい場面です。まず、箱と採点関数を渡して最良点を受け取る最小の使い方を確認できます。

**具体的に解く問題**

入力の目標を$t$、上下限を$l,u$とし、$l\le x\le u$の範囲で$(x-t)^2$を小さくします。$x$だけが調整する変数です。目標が箱の外なら、近い方の端が正解になります。この簡単な問題は式から直接解けますが、APIの動作を確かめる教材にしています。

**入力形式**

1行に`target lower upper`。全て有限、`lower <= upper`。教材では絶対値100以下。

**出力形式**

1行に、最良の出力`x`と目的値。入力例では`x=3`、目的値0の近くになります。

**サンプルコード：`example01_calibration.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 箱制約付き最小化の型と関数を読み込む

using namespace std;

int main() {
    // 入力条件：入力は有限なtarget、lower、upper。lower <= upper、絶対値100以下を想定する。
    double target, lower, upper;
    if (!(cin >> target >> lower >> upper)) {
        return 1;
    }

    BoxProblem problem; // 調整する変数と許される範囲を用意する
    problem.bounds = {{lower, upper}}; // 有限な下限・上限を指定する。lower <= upperが必要

    // 候補xは読み取り専用で、呼び出し中だけ有効
    auto objective = [=](span<const double> x) {
        double error = x[0] - target; // 目的関数は元の単位の候補を受け取る
        return error * error; // 小さいほど良い有限の値を返す
    };

    BoxBudget budget; // 1回の探索で使える予算
    budget.time_limit_us = -1; // 時間では止めず、下の有限な評価上限で止める
    budget.max_evaluations = 200; // 目的関数を最大200回呼ぶ

    // 初期解を省略し、箱の中央から1回だけ探索する
    auto result = box_optimize(problem, objective, budget);
    // 未評価ならxへアクセスしない
    if (!result.has_value()) {
        return 1;
    }

    // 最良の設定と、その目的値
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.cost << '\n';
}
```


**入力例**

```text
3 0 10
```


**出力例（標準出力）**

```text
3.000000 0.000000
```


**利用上の注意**

初期解を省略すると箱の中央です。時間を無制限にしているので、有限の評価回数200を必ず併用しています。

### 3.4 例02：合計と差の両方を満たす2つの出力

2つの装置を操作し、全体の出力も装置間の差も調整する場面です。一方を動かすと複数の評価項が変わるため、2つの変数を同じ関数で採点します。

**具体的に解く問題**

各出力$x_ {0},x_ {1}$を0〜10に制限し、合計の目標$S$と差の目標$D$に対し、$(x_ {0}+x_ {1}-S)^2+4(x_ {0}-x_ {1}-D)^2$を最小化します。入力例の$S=3,D=1$なら、2と1で両条件を満たします。

**入力形式**

1行に`total difference`。合計目標は0〜20、差の目標は−10〜10。両方を同時に満たせなくても、重み付きのずれを最小化する問題として有効です。

**出力形式**

最良の`x[0]`、`x[1]`、目的値。

**サンプルコード：`example02_coupled_controls.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // BoxProblem、BoxOptimizerなどを使う

using namespace std;

int main() {
    // 入力条件：入力は合計目標0〜20と差の目標-10〜10。出力変数はそれぞれ0〜10で動かす。
    double total, difference;
    if (!(cin >> total >> difference)) {
        return 1;
    }

    BoxProblem problem; // 2つの出力を同時に調整する
    problem.bounds = {{0, 10}, {0, 10}}; // x[0]、x[1]のそれぞれを0〜10に制限

    auto objective = [=](span<const double> x) { // 2要素の候補を受け取る
        double a = x[0] + x[1] - total; // 合計の目標からのずれ
        double b = x[0] - x[1] - difference; // 差の目標からのずれ
        return a * a + 4 * b * b; // 差のずれを合計のずれの4倍重く扱う
    };

    BoxParam param; // 調べ方と探索方針を設定する
    param.search = BoxParam::Search::Refine; // 局所的な改善が止まったら終了する
    BoxOptimizer solver(problem, param); // 箱と設定を持つ探索オブジェクトを作る

    BoxBudget budget; // 今回のsolveに対する予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 3000; // 目的評価は最大3000回

    vector<double> initial = {8, 8}; // 長さ2で、両成分とも箱内の初期解

    // 指定点から新しい探索を開始する
    auto result = solver.solve(objective, budget, initial);
    // 評価済み解があるか確認する
    if (!result.has_value()) {
        return 1;
    }

    // 2出力と目的値
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.x[1] << ' '
         << result.cost << '\n';
}
```


**入力例**

```text
3 1
```


**出力例（標準出力）**

```text
2.000000 1.000000 0.000000
```


**利用上の注意**

係数4は目的関数の重みです。`Refine`は探索方針、3000は評価予算であり、役割が異なります。

### 3.5 例03：観測データへ直線を当てはめる

時刻や入力に対する観測値を、簡単な予測式で表したい場面です。値の計算と一緒に傾きも安く求められるので、勾配を渡します。

**具体的に解く問題**

観測組$(t_ {i},y_ {i})$に対し、予測を$a t_ {i}+b$とします。調整するのは$a,b$で、どちらも−10〜10です。目的値は$\sum_ {i=0}^{n-1}(a t_ {i}+b-y_ {i})^2$。$n$は観測数、$i$は観測番号、$t_ {i}$と$y_ {i}$は固定データです。$\sum$は指定範囲の項を全て足す記号です。

**入力形式**

最初に観測数`n`、続くn行に`t[i] y[i]`。nは1〜50、各値の絶対値は10以下。

**出力形式**

傾き`a`、切片`b`、二乗誤差の合計。入力例は直線`y=2t+1`上の点です。

**サンプルコード：`example03_line_fit.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 勾配付きのBoxOptimizerも同じヘッダで使える

using namespace std;

int main() {
    // 入力条件：nは1〜50、各tとyの絶対値は10以下。データはsolve実行中に変更しない。
    int n;
    if (!(cin >> n) || n < 1) {
        return 1;
    }

    vector<double> t(n), y(n);

    for (int i = 0; i < n; ++i) {
        cin >> t[i] >> y[i];
    }

    BoxProblem problem; // 直線の傾きaと切片bを変数にする
    problem.bounds = {{-10, 10}, {-10, 10}}; // x[0]=a、x[1]=bの許容範囲

    // 第2引数は勾配を書き込む配列
    auto objective = [&](span<const double> x, span<double> gradient) {
        // 勾配が要求された場合だけ全成分を初期化
        if (!gradient.empty()) {
            fill(gradient.begin(), gradient.end(), 0.0);
        }

        double cost = 0;

        for (int i = 0; i < n; ++i) {
            double error = x[0] * t[i] + x[1] - y[i]; // 候補の直線と観測値のずれ
            cost += error * error;
            // 空配列へ書くと範囲外アクセスになるため必ず確認
            if (!gradient.empty()) {
                gradient[0] += 2 * error * t[i]; // 元の単位のaについての傾き
                gradient[1] += 2 * error; // 元の単位のbについての傾き
            }
        }

        return cost; // 値はreturn、勾配は配列への代入で渡す
    };

    BoxParam param; // 手法は既定のAutoを使う
    // 二引数形式なのでAutoは勾配を使って局所改善する
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param); // データt、yは外側で保持し、探索中は変更しない

    BoxBudget budget; // 1回の探索予算
    budget.time_limit_us = -1; // 時間制限なし
    budget.max_evaluations = 3000; // 値と勾配を一緒に計算しても1呼び出しで1評価

    auto result = solver.solve(objective, budget); // a=b=0という箱の中央から開始
    // 有効な結果だけ参照する
    if (!result.has_value()) {
        return 1;
    }

    // a、b、二乗誤差
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.x[1] << ' '
         << result.cost << '\n';
}
```


**入力例**

```text
5
0 1
1 3
2 5
3 7
4 9
```


**出力例（標準出力）**

```text
2.000000 1.000000 0.000000
```


**利用上の注意**

勾配は、aについて`2*error*t[i]`、bについて`2*error`を足します。データから直線が一意に決まらない場合でも実行できますが、係数の一意性までは保証しません。

### 3.6 例04：一部を固定した設定列を滑らかにする

照明の強さや地点ごとの設定を、希望値へ近づけながら隣同士で急変しないようにします。すでに確定した地点は動かさず、元の配列の添字をそのまま利用します。

**具体的に解く問題**

目的値は$\sum_ {i=0}^{n-1}(x_ {i}-t_ {i})^2+w\sum_ {i=0}^{n-2}(x_ {i}-x_ {i+1})^2$です。$t_ {i}$は地点の目標、$w$は滑らかさの重み、$n$は地点数です。自由成分は0〜10、固定フラグ1の成分は入力された初期値に固定します。

**入力形式**

1行目は`n weight`。続くn行は`initial[i] target[i] fixed`。nは1〜50、初期値と目標は0〜10、weightは0〜10、fixedは0または1。

**出力形式**

1行目に固定成分も含むn個の値、2行目に目的値。

**サンプルコード：`example04_fixed_smoothing.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 固定変数を含む箱制約を使う

using namespace std;

int main() {
    // 入力条件：nは1〜50、初期値・目標は0〜10、weightは0〜10、fixedは0または1。
    int n;
    double weight;
    if (!(cin >> n >> weight) || n < 1 || weight < 0) {
        return 1;
    }

    vector<double> initial(n), target(n);

    BoxProblem problem; // 全n要素の添字を保ったまま、一部を固定する

    for (int i = 0; i < n; ++i) {
        int fixed_value;
        cin >> initial[i] >> target[i] >> fixed_value;
        // 同じ上下限なら固定、自由成分は0〜10
        problem.bounds.push_back(fixed_value ? BoxBound{initial[i], initial[i]}
                                             : BoxBound{0, 10});
    }

    // xと非空の勾配は固定成分込みの長さn
    auto objective = [&](span<const double> x, span<double> gradient) {
        // 固定成分も含め全成分を有限値で埋める
        if (!gradient.empty()) {
            fill(gradient.begin(), gradient.end(), 0.0);
        }

        double cost = 0;

        for (int i = 0; i < n; ++i) {
            double error = x[i] - target[i]; // 各位置の目標からのずれ
            cost += error * error;
            // 各変数についての偏微分
            if (!gradient.empty()) {
                gradient[i] += 2 * error;
            }
        }

        for (int i = 0; i + 1 < n; ++i) {
            double difference = x[i] - x[i + 1]; // 隣同士を滑らかにするための差
            cost += weight * difference * difference;
            // 勾配を要求されたときだけ書き込む
            if (!gradient.empty()) {
                gradient[i] += 2 * weight * difference; // 左側の変数に対する傾き
                gradient[i + 1] -= 2 * weight * difference; // 右側の変数に対する傾き
            }
        }

        return cost; // 探索中はtargetとweightを変えない
    };

    BoxParam param; // 箱の中での改善方針
    param.search = BoxParam::Search::Refine; // 現在の設定を局所改善する
    BoxOptimizer solver(problem, param); // 固定成分は内部の探索対象から外れる

    BoxBudget budget; // 今回の探索予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 5000; // 最大5000評価

    // 初期解の固定成分は上下限と一致させる
    auto result = solver.solve(objective, budget, initial);
    // 未評価の配列は使わない
    if (!result.has_value()) {
        return 1;
    }

    cout << fixed << setprecision(6);

    // 結果にも固定成分が含まれる
    for (double value : result.x) {
        cout << value << ' ';
    }

    cout << '\n' << result.cost << '\n'; // 全体の目的値
}
```


**入力例**

```text
5 0.5
0 0 1
2 3 0
2 1 0
2 4 0
2 2 1
```


**出力例（標準出力）**

```text
0.000000 1.928571 1.714286 2.928571 2.000000 
5.857143
```


**利用上の注意**

固定成分を除いた長さの勾配ではありません。非空の勾配は全n成分を埋めます。固定されている場所も目的関数の隣接項には影響します。

### 3.7 例05：大きなグラフの一部分だけを小さい問題として改善する

グラフ全体の設定を持っているものの、今回変更する頂点が少ない場面です。ライブラリへ渡す配列自体を自由な頂点だけにし、候補に関係する項だけ再計算します。

**具体的に解く問題**

全頂点について目標との差の二乗を足し、各辺$(u,v)$について$w(x_ {u}-x_ {v})^2$を加えます。$u,v$は辺の両端、$w$はその辺の重みです。指定したk頂点だけ0〜10で動かし、ほかは`base`へ固定します。変わらない項は定数として一度計算します。

**入力形式**

`n m k`、n個のbase、n個のtarget、m行の`u v weight`、最後に自由にするk個の頂点番号。頂点番号は0〜n−1で、自由頂点は重複させません。nは1〜50、mは0〜200、1<=k<=n、baseとtargetは0〜10、重みは0〜10。

**出力形式**

1行目に元の長さnへ戻した設定、2行目に部分計算の目的値と、全体の式で再計算した目的値。

**サンプルコード：`example05_compact_graph.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 小さい部分問題にまとめて最適化する

using namespace std;

struct Edge {
    int u, v;
    double weight;
};

int main() {
    // 入力条件：nは1〜50、mは0〜200、1<=k<=n。辺の端点は0〜n-1、自由頂点は重複不可。
    // base・target・重みは0〜10。
    int n, m, k;
    if (!(cin >> n >> m >> k) || n < 1 || k < 1 || k > n || m < 0) {
        return 1;
    }

    vector<double> base(n), target(n);

    for (double& x : base) {
        cin >> x;
    }

    for (double& x : target) {
        cin >> x;
    }

    vector<Edge> edges(m), affected;

    for (auto& edge : edges) {
        cin >> edge.u >> edge.v >> edge.weight;
    }

    vector<int> free_index(k), slot(n, -1);

    for (int j = 0; j < k; ++j) {
        cin >> free_index[j];
        slot[free_index[j]] = j;
    }

    double constant = 0;

    for (int i = 0; i < n; ++i) {
        if (slot[i] < 0) {
            constant += (base[i] - target[i]) * (base[i] - target[i]);
        }
    }

    for (auto edge : edges) {
        if (slot[edge.u] >= 0 || slot[edge.v] >= 0) {
            affected.push_back(edge);
        } else {
            constant +=
                edge.weight * (base[edge.u] - base[edge.v]) * (base[edge.u] - base[edge.v]);
        }
    }

    BoxProblem problem; // 自由なk成分だけをライブラリへ渡す
    // z[j]は元のfree_index[j]に対応し、0〜10で動く
    problem.bounds.assign(k, BoxBound{0, 10});

    vector<double> initial(k);

    // 初期解も自由成分だけ詰める
    for (int j = 0; j < k; ++j) {
        initial[j] = base[free_index[j]];
    }

    auto objective = [&](span<const double> z) { // zの長さはnではなくk。添字の対応を守る
        // 固定成分は外側のbaseを読む
        auto value = [&](int i) {
            return slot[i] < 0 ? base[i] : z[slot[i]];
        };

        double cost = constant;

        for (int j = 0; j < k; ++j) {
            double error = z[j] - target[free_index[j]]; // 自由成分に依存する誤差だけ再計算
            cost += error * error;
        }

        for (auto edge : affected) {
            double difference = value(edge.u) - value(edge.v);
            cost += edge.weight * difference * difference;
        }

        return cost; // 固定項も加え、元の問題と同じ目的値を返す
    };

    BoxParam param; // 部分問題を局所改善する
    param.search = BoxParam::Search::Refine; // 大域最適性の保証を意味する指定ではない
    BoxOptimizer solver(problem, param); // 初期化する変数数はk

    BoxBudget budget; // 目的評価の上限を指定する
    budget.time_limit_us = -1; // 時間制限なし
    budget.max_evaluations = 5000; // 最大5000評価

    // base、target、edgesは探索中に変更しない
    auto result = solver.solve(objective, budget, initial);
    // 評価済み解を確認
    if (!result.has_value()) {
        return 1;
    }

    vector<double> output = base;

    // 元の長さnへ戻す
    for (int j = 0; j < k; ++j) {
        output[free_index[j]] = result.x[j];
    }

    double full_cost = 0;

    for (int i = 0; i < n; ++i) {
        full_cost += (output[i] - target[i]) * (output[i] - target[i]);
    }

    for (auto edge : edges) {
        full_cost += edge.weight * (output[edge.u] - output[edge.v])
                     * (output[edge.u] - output[edge.v]);
    }

    cout << fixed << setprecision(6);

    for (double value : output) {
        cout << value << ' ';
    }

    // ライブラリの値と元の式の値を照合できる
    cout << '\n' << result.cost << ' ' << full_cost << '\n';
}
```


**入力例**

```text
4 3 2
1 4 6 9
0 3 7 10
0 1 0.5
1 2 0.5
2 3 0.5
1 2
```


**出力例（標準出力）**

```text
1.000000 3.400000 6.600000 9.000000 
13.200000 13.200000
```


**利用上の注意**

固定変数の機能だけでは、目的関数の全走査は自動で減りません。この例は利用者が式を等価な小さい計算へ整理しています。「前回の候補との差分」を渡すAPIではありません。

### 3.8 例06：単位と範囲が異なる変数の初期幅を決める

長さと割合のように、値の大きさが大きく異なる設定を同時に調整する場面です。目的関数での重み付けと、探索を始める幅の指定を分けて扱います。

**具体的に解く問題**

長さ$x_ {0}$は0〜1000、割合$x_ {1}$は0〜1。目標を$L,R$とし、$((x_ {0}-L)/100)^2+(x_ {1}-R)^2$を最小化します。長さ100の誤差を割合1の誤差と同じ重さで数えます。初期幅は長さ50、割合0.02です。

**入力形式**

`length_target rate_target`。それぞれ0〜1000、0〜1。

**出力形式**

最良の長さ、割合、目的値。

**サンプルコード：`example06_initial_steps.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 単位の異なる変数に初期幅を設定する

using namespace std;

int main() {
    // 入力条件：入力は目標長さ0〜1000と目標割合0〜1。初期幅50、0.02はそれぞれの元の単位。
    double length_target, rate_target;
    if (!(cin >> length_target >> rate_target)) {
        return 1;
    }

    BoxProblem problem; // 長さと割合の2変数
    problem.bounds = {{0, 1000}, {0, 1}}; // x[0]は長さ、x[1]は割合

    auto objective = [=](span<const double> x) { // 候補は正規化前の元の単位で渡る
        double a = (x[0] - length_target) / 100; // 長さ100のずれを1として数える
        double b = x[1] - rate_target; // 割合のずれ
        return a * a + b * b; // 重み付けは目的関数側で決める
    };

    BoxParam param; // 調整幅と方針を用意
    param.search = BoxParam::Search::Refine; // 初期点を局所改善する
    param.initial_step = {50, 0.02}; // 元の単位で指定。正で、各箱幅以下にする

    vector<double> initial = {800, 0.1}; // それぞれの箱内の初期設定

    BoxBudget budget; // 1回の探索予算
    budget.time_limit_us = -1; // 時間制限なし
    budget.max_evaluations = 3000; // 有限の評価上限

    // 5引数で箱・関数・予算・初期解・設定を渡す
    auto result = box_optimize(problem, objective, budget, initial, param);
    // 解がある場合だけ出力
    if (!result.has_value()) {
        return 1;
    }

    // 結果も元の単位
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.x[1] << ' '
         << result.cost << '\n';
}
```


**入力例**

```text
200 0.6
```


**出力例（標準出力）**

```text
200.000000 0.600000 0.000000
```


**利用上の注意**

初期幅は正規化された比率ではなく元の単位です。目的関数の100という除数と、探索幅の50は別の設定です。

### 3.9 例07：縦横の移動距離で施設の位置を選ぶ

道路が縦横に通っているとみなし、利用者から施設までの距離を短くする場面です。絶対値の折れ曲がりがあるため、勾配を用意せず値だけで採点します。

**具体的に解く問題**

施設の座標を$(x,y)$、利用地点を$(a_ {i},b_ {i})$、利用頻度を$w_ {i}$とし、$\sum_ {i=0}^{n-1}w_ {i}(\lvert x-a_ {i}\rvert+\lvert y-b_ {i}\rvert)$を小さくします。施設は0〜10の正方形内です。縦棒は絶対値です。

**入力形式**

地点数n、続くn行に`横位置 縦位置 重み`。nは1〜50、各座標は0〜10、重みは0より大きく10以下。

**出力形式**

施設の横位置、縦位置、重み付き総移動距離。

**サンプルコード：`example07_manhattan_location.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 微分しにくい目的関数も一引数形式で扱える

using namespace std;

int main() {
    // 入力条件：nは1〜50、地点の座標は0〜10、重みは0より大きく10以下。
    int n;
    if (!(cin >> n) || n < 1) {
        return 1;
    }

    vector<array<double, 3>> points(n);

    for (auto& p : points) {
        cin >> p[0] >> p[1] >> p[2];
    }

    BoxProblem problem; // 施設の横位置と縦位置
    problem.bounds = {{0, 10}, {0, 10}}; // 設置できる長方形

    auto objective = [&](span<const double> x) { // 勾配は渡さない
        double cost = 0;

        // 重み付きの縦横移動距離
        for (auto p : points) {
            cost += p[2] * (abs(x[0] - p[0]) + abs(x[1] - p[1]));
        }

        return cost; // 絶対値の折れ曲がりがあっても有限値を返す
    };

    BoxParam param; // 手法と方針は別々に指定する
    param.method = BoxParam::Method::Coordinate; // 値だけを使う座標探索を選ぶ
    param.search = BoxParam::Search::Refine; // 局所停止したら終了する
    BoxOptimizer solver(problem, param); // データpointsは探索中不変

    BoxBudget budget; // 呼び出し1回の予算
    budget.time_limit_us = -1; // 時計では止めない
    budget.max_evaluations = 5000; // 最大5000評価

    vector<double> initial = {9, 9}; // 箱内の初期候補

    // 一引数なので数値微分は行わない
    auto result = solver.solve(objective, budget, initial);
    // 有効性を確認
    if (!result.has_value()) {
        return 1;
    }

    // 設置位置と総距離
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.x[1] << ' '
         << result.cost << '\n';
}
```


**入力例**

```text
3
1 2 1
7 3 3
5 8 1
```


**出力例（標準出力）**

```text
7.000000 3.000000 14.000000
```


**利用上の注意**

この距離の問題には専用の解き方もあります。ここでは、微分が一意に定まらない点を持つ目的関数を値だけで渡す例です。

### 3.10 例08：減衰する観測値へ指数曲線を当てはめる

時間とともに残量や反応が減るデータから、減り方の速さを求める場面です。直線では表せない関数も、C++で値を計算できれば渡せます。

**具体的に解く問題**

時刻$t_ {i}$の予測を$\exp(-k t_ {i})$とし、観測値$y_ {i}$に対する二乗誤差を合計します。調整するのは$0\le k\le2$の係数です。$\exp(z)$は自然対数の底$e$の$z$乗。$k$が大きいほど速く減ります。

**入力形式**

観測数n、続くn行に`time[i] observed[i]`。nは1〜50、時刻は0〜10、観測値は0〜1。

**出力形式**

係数kと二乗誤差。入力例はk=0.4の曲線に近いデータです。

**サンプルコード：`example08_decay_fit.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 式を関数として渡して曲線の係数を調整する

using namespace std;

int main() {
    // 入力条件：nは1〜50、時刻は0〜10、観測値は0〜1。
    int n;
    if (!(cin >> n) || n < 1) {
        return 1;
    }

    vector<double> time(n), observed(n);

    for (int i = 0; i < n; ++i) {
        cin >> time[i] >> observed[i];
    }

    BoxProblem problem; // 減衰の速さkを1変数で表す
    problem.bounds = {{0, 2}}; // 負の減衰係数は許さない

    auto objective = [&](span<const double> x) { // x[0]が係数k
        double cost = 0;

        for (int i = 0; i < n; ++i) {
            double error =
                exp(-x[0] * time[i]) - observed[i]; // 時刻に対する予測曲線と観測の差
            cost += error * error;
        }

        return cost; // 同じ候補には同じ観測データで同じ値を返す
    };

    BoxParam param; // 既定のAutoを使う
    param.search = BoxParam::Search::Refine; // 値だけの局所探索を行う
    BoxOptimizer solver(problem, param); // 観測データはコールバックが参照する

    BoxBudget budget; // このfitの予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 2000; // 最大2000評価

    auto result = solver.solve(objective, budget); // 初期係数は箱の中央の1
    // 未評価なら解を出さない
    if (!result.has_value()) {
        return 1;
    }

    // 減衰係数と二乗誤差
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.cost << '\n';
}
```


**入力例**

```text
4
0 1
1 0.670320046
2 0.449328964
3 0.301194212
```


**出力例（標準出力）**

```text
0.400000 0.000000
```


**利用上の注意**

観測データは探索中に固定します。勾配を書いていないため、ライブラリが数値微分を自動生成することはありません。

### 3.11 例09：何桁も変わる正の倍率を対数で調整する

倍率が0.000001から1000000まで変わるように、差より比率で考えたい場面です。倍率の対数を動かすと、同じ移動幅が同じ倍率の変化に対応します。

**具体的に解く問題**

倍率$g$を$10^{-9}$〜$10^9$に制限し、目標$t>0$に対する相対誤差$(g/t-1)^2$を小さくします。入力変数は$z=\log g$、倍率への変換は$g=\exp z$です。$z$についての勾配は$2(g/t-1)g/t$です。

**入力形式**

`target guess`。目標倍率と初期予想はどちらも10の−9乗以上、10の9乗以下の正の値。

**出力形式**

対数から戻した倍率gと相対誤差の二乗。

**サンプルコード：`example09_log_scale.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 対数へ変換した変数を最適化する

using namespace std;

int main() {
    // 入力条件：targetとguessはともに1e-9〜1e9の正の有限値。初期解と箱にはlogを取り、
    // 出力はexpで戻す。
    double target, guess;
    if (!(cin >> target >> guess) || target <= 0 || guess <= 0) {
        return 1;
    }

    BoxProblem problem; // 直接の倍率gではなくz=log(g)を変数にする
    problem.bounds = {{log(1e-9), log(1e9)}}; // gを10の-9乗〜10の9乗に制限する

    // 勾配も入力変数zについて返す
    auto objective = [=](span<const double> z, span<double> gradient) {
        double gain = exp(z[0]); // 正の倍率へ戻して採点する
        double error = gain / target - 1;
        // exp(z)の微分gainを掛ける
        if (!gradient.empty()) {
            gradient[0] = 2 * error * gain / target;
        }

        return error * error; // 相対的なずれの二乗。targetは探索中不変
    };

    BoxParam param; // 正しい勾配を使うAuto経路
    param.search = BoxParam::Search::Refine; // 初期予想から改善する
    BoxOptimizer solver(problem, param); // 箱は対数の単位で保持される

    BoxBudget budget; // 予算はコールバック回数で指定する
    budget.time_limit_us = -1; // 時間制限なし
    budget.max_evaluations = 3000; // 最大3000評価
    budget.target_cost = 1e-12; // 相対誤差の二乗がこの値以下なら終了

    vector<double> initial = {log(guess)}; // 初期解も対数へ変換し、箱内にする

    // ライブラリは利用者のexp変換を自動微分しない
    auto result = solver.solve(objective, budget, initial);
    // 有効な結果だけ変換する
    if (!result.has_value()) {
        return 1;
    }

    // 結果は対数なので倍率へ戻す
    cout << fixed << setprecision(6) << exp(result.x[0]) << ' ' << result.cost << '\n';
}
```


**入力例**

```text
1000000 10000
```


**出力例（標準出力）**

```text
999999.760095 0.000000
```


**利用上の注意**

箱・初期解・返却解は対数の単位です。勾配もzについて計算します。目標の目的値1e−12は、相対誤差の絶対値がおおよそ1e−6以下という基準です。

### 3.12 例10：複数の観測方向へ近い向きを選ぶ

移動方向や機器の向きを、複数の希望方向へ合わせる場面です。角度の数値を直接引くと端で大きな差になるため、向きを平面上のベクトルに直して比較します。

**具体的に解く問題**

選ぶ向きは$(\cos\theta,\sin\theta)$です。観測ベクトル$(a_ {i},b_ {i})$との距離の二乗を合計し、$-\pi\le\theta\le\pi$で小さくします。$\theta$はラジアン単位の角度、$\pi$は円周率です。入力例の2方向を同じ重みで見ると、45度付近が中間です。

**入力形式**

ベクトル数n、続くn行に横成分と縦成分。nは1〜50、成分の絶対値は10以下。

**出力形式**

角度、選んだ単位ベクトルの横・縦成分、目的値。

**サンプルコード：`example10_direction.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 角度を箱内の実数として扱う

using namespace std;

int main() {
    // 入力条件：nは1〜50、ベクトル成分の絶対値は10以下。角度はラジアンで扱う。
    int n;
    if (!(cin >> n) || n < 1) {
        return 1;
    }

    vector<array<double, 2>> vectors(n);

    for (auto& v : vectors) {
        cin >> v[0] >> v[1];
    }

    const double pi = acos(-1.0);

    BoxProblem problem; // 角度thetaを1変数にする
    // 単位は度ではなくラジアン。両端をまたぐ操作は自動では行わない
    problem.bounds = {{-pi, pi}};

    auto objective = [&](span<const double> x, span<double> gradient) { // x[0]がtheta
        double c = cos(x[0]), s = sin(x[0]); // 向きを単位ベクトルへ変換
        double cost = 0, slope = 0;

        for (auto v : vectors) {
            double dx = c - v[0], dy = s - v[1];
            cost += dx * dx + dy * dy;
            slope += 2 * (-dx * s + dy * c);
        }

        // thetaについての微分を渡す
        if (!gradient.empty()) {
            gradient[0] = slope;
        }

        return cost; // ベクトルの距離なので角度の表記の切れ目で採点が飛ばない
    };

    BoxParam param; // 勾配を使う手法を明示する
    param.method = BoxParam::Method::SpectralGradient; // 履歴を持たないスペクトル勾配の経路
    param.search = BoxParam::Search::Refine; // 局所停止で終了する
    // SpectralGradientには自由変数があるので二引数形式が必要
    BoxOptimizer solver(problem, param);

    BoxBudget budget; // 今回の予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 3000; // 最大3000評価

    // 初期角度は0。必要なら別の初期角度も比較する
    auto result = solver.solve(objective, budget);
    // 未評価なら変換しない
    if (!result.has_value()) {
        return 1;
    }

    // 角度、向き、目的値
    cout << fixed << setprecision(6) << result.x[0] << ' ' << cos(result.x[0]) << ' '
         << sin(result.x[0]) << ' ' << result.cost << '\n';
}
```


**入力例**

```text
2
1 0
0 1
```


**出力例（標準出力）**

```text
0.785398 0.707107 0.707107 1.171573
```


**利用上の注意**

端をまたいで動く周期変数のAPIではありません。初期点によって不利な停止が起きる場合は、初期角度を変えて比較します。SpectralGradientには二引数形式が必要です。

### 3.13 例11：総量を守って3か所へ配分する

予算や資源の合計を変えず、3か所の希望量へ近づける場面です。候補を作る段階で合計条件を満たすため、採点時に制約違反へ罰点を付ける必要がありません。

**具体的に解く問題**

総量を$B>0$、割合を$0\le u,v\le1$とし、配分を$a=Bu$、$b=B(1-u)v$、$c=B(1-u)(1-v)$とします。3つとも非負で合計は式の上で$B$です。希望配分との差の二乗を足して小さくします。

**入力形式**

`total target[0] target[1] target[2]`。totalは0より大きく100以下、希望配分は非負で合計がtotal。

**出力形式**

1行目は3か所への配分、2行目は合計と目的値。

**サンプルコード：`example11_total_allocation.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 一般制約を満たす変換を利用者側で作る

using namespace std;

int main() {
    // 入力条件：totalは0より大きく100以下。3つの希望配分は非負で、合計がtotal。
    double total;
    array<double, 3> target;
    if (!(cin >> total >> target[0] >> target[1] >> target[2]) || total <= 0) {
        return 1;
    }

    auto decode = [=](span<const double> x) { // ライブラリの2変数を3か所への配分に戻す
        double rest = total * (1 - x[0]); // 最初の配分後に残る量
        // 合計total・非負を式で満たす
        return array<double, 3>{total * x[0], rest * x[1], rest * (1 - x[1])};
    };

    BoxProblem problem; // 動かすのは2つの割合
    problem.bounds = {{0, 1}, {0, 1}}; // 割合を0〜1にする

    auto objective = [&](span<const double> x) { // 一引数形式で値だけを返す
        auto allocation = decode(x); // 変換後の実際の配分を採点する
        double cost = 0;

        for (int i = 0; i < 3; ++i) {
            cost += (allocation[i] - target[i]) * (allocation[i] - target[i]);
        }

        return cost; // 箱の外の追加制約を登録しているわけではない
    };

    BoxParam param; // 局所改善で十分な教材を使う
    param.search = BoxParam::Search::Refine; // 停滞したら終了
    BoxOptimizer solver(problem, param); // 内部変数数は配分先の数3ではなく2

    BoxBudget budget; // 予算の単位は目的評価回数
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 10000; // 最大10000評価
    budget.target_cost = 1e-12; // 目標配分と十分近くなったら終了

    auto result = solver.solve(objective, budget); // 初期割合は0.5、0.5
    // 有効な割合だけ使う
    if (!result.has_value()) {
        return 1;
    }

    auto allocation = decode(result.x); // result.xは配分ではなく割合なので必ず変換する

    cout << fixed << setprecision(6);

    for (double amount : allocation) {
        cout << amount << ' ';
    }

    // 合計と目的値。合計には丸め誤差があり得る
    cout << '\n'
         << accumulate(allocation.begin(), allocation.end(), 0.0) << ' ' << result.cost
         << '\n';
}
```


**入力例**

```text
10 2 3 5
```


**出力例（標準出力）**

```text
2.000000 2.999999 5.000000 
10.000000 0.000000
```


**利用上の注意**

ライブラリが返すのはu、vです。そのまま配分として使わずdecodeします。浮動小数点の合計には丸め誤差があり、厳密な整数総量が必要なら変換後の修復と採点を別に設計します。

### 3.14 例12：円形の領域内で目的地に近い点を選ぶ

設置可能な領域が長方形でなく円になっている場面です。円内に入る座標の作り方を用意し、ライブラリには半径と角度の箱を渡します。

**具体的に解く問題**

原点中心、半径$R>0$の円内で、目的地$(t_ {x},t_ {y})$までの距離の二乗を最小化します。点を$(r\cos\theta,r\sin\theta)$とし、$0\le r\le R$、$-\pi\le\theta\le\pi$で動かします。$r$は中心からの距離、$\theta$は角度です。

**入力形式**

`radius tx ty`。radiusは0より大きく100以下、目的地の各座標の絶対値は100以下。目的地は円外でも構いません。

**出力形式**

採用した平面上の横・縦座標と、目的地までの距離の二乗。

**サンプルコード：`example12_disk_constraint.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 円内制約を半径と角度の箱へ変換する

using namespace std;

int main() {
    // 入力条件：radiusは0より大きく100以下、tx・tyの絶対値は100以下。
    // 初期半径はradius/2にする。
    double radius, tx, ty;
    if (!(cin >> radius >> tx >> ty) || radius <= 0) {
        return 1;
    }

    const double pi = acos(-1.0);

    BoxProblem problem; // x[0]は中心からの距離r、x[1]は角度theta
    problem.bounds = {{0, radius}, {-pi, pi}}; // rを制限すると変換後の点が必ず円内に入る

    // 入力r、thetaについて勾配を作る
    auto objective = [=](span<const double> x, span<double> gradient) {
        double c = cos(x[1]), s = sin(x[1]); // thetaを向きへ変換
        double dx = x[0] * c - tx, dy = x[0] * s - ty; // 変換後の座標と目的地の差
        // 空の勾配配列には書かない
        if (!gradient.empty()) {
            gradient[0] = 2 * (dx * c + dy * s); // rに対する微分
            // thetaに対する微分。座標の勾配をそのまま渡さない
            gradient[1] = 2 * x[0] * (-dx * s + dy * c);
        }

        return dx * dx + dy * dy; // 円内の点から目的地への距離の二乗
    };

    BoxParam param; // 勾配付きのAutoを使う
    param.search = BoxParam::Search::Refine; // 指定した初期点から局所改善
    BoxOptimizer solver(problem, param); // 一般の円制約を直接登録するAPIではない

    BoxBudget budget; // 今回の評価予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 3000; // 最大3000評価

    // r=0では角度を変えても点が動かないため中心から離して開始
    vector<double> initial = {radius / 2, 0};

    // 元座標でなく変換後の変数を渡す
    auto result = solver.solve(objective, budget, initial);
    // 評価済みの結果を確認
    if (!result.has_value()) {
        return 1;
    }

    // 出力用の平面座標へ戻す
    double px = result.x[0] * cos(result.x[1]), py = result.x[0] * sin(result.x[1]);

    // 採用位置と距離の二乗
    cout << fixed << setprecision(6) << px << ' ' << py << ' ' << result.cost << '\n';
}
```


**入力例**

```text
2 3 4
```


**出力例（標準出力）**

```text
1.200000 1.600000 9.000000
```


**利用上の注意**

この単純な問題は幾何から直接解けますが、円内制約を箱へ変換する教材です。r=0では角度を変えても点が動かないため、中心を避けて開始しています。勾配もrとthetaについて計算します。

### 3.15 例13：費用の最小化ではなく利益を最大化する

値が大きいほど良い採点を使っている場面です。ライブラリの「小さいほど良い」という約束へ合わせ、出力時に元の得点へ戻します。

**具体的に解く問題**

生産量$x$は0〜10、利益を$S(x)=P-(x-t)^2$とします。$P$は最大利益、$t$は望ましい生産量です。目的関数として$f(x)=-S(x)$を渡し、利益が$P-0.01$以上になったら止めます。

**入力形式**

`preferred peak`。preferredは0〜10、peakの絶対値は100以下。

**出力形式**

採用した生産量と元の利益。

**サンプルコード：`example13_maximize_profit.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 得点の符号を変えて最大化へ利用する

using namespace std;

int main() {
    // 入力条件：preferredは0〜10、peakの絶対値は100以下。
    // 最大化なので目的値と目標値を両方反転する。
    double preferred, peak;
    if (!(cin >> preferred >> peak)) {
        return 1;
    }

    auto score = [=](double production) {
        return peak - (production - preferred) * (production - preferred);
    };

    BoxProblem problem; // 生産量を調整する
    problem.bounds = {{0, 10}}; // 生産量の上下限

    // 最大化したい得点にマイナスを付ける
    auto objective = [&](span<const double> x) {
        return -score(x[0]);
    };

    BoxBudget budget; // 計算量と十分な得点の条件
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 1000; // 最大1000評価
    budget.target_cost = -(peak - 0.01); // 得点がpeak-0.01以上なら終了。目標値の符号も反転

    auto result = box_optimize(problem, objective, budget); // 目的値の最小化として解く
    // 未評価のcostを得点へ変換しない
    if (!result.has_value()) {
        return 1;
    }

    // 生産量と元の得点を出す
    cout << fixed << setprecision(6) << result.x[0] << ' ' << -result.cost << '\n';
}
```


**入力例**

```text
3 100
```


**出力例（標準出力）**

```text
3.070000 99.995100
```


**利用上の注意**

目標値の符号も反転します。勾配を渡す形へ変更するなら、目的値だけでなく勾配の符号も反転してください。

### 3.16 例14：整数へ丸めた出力を直接評価する

最終的に個数や段階値のような整数を提出する場面です。実数の候補が良くても、丸めた出力が良いとは限らないため、最初から丸めた値を採点します。

**具体的に解く問題**

各設定を0〜10で動かし、最も近い整数へ丸めた値と目標整数との差の二乗を合計します。目標整数は$t_ {i}$、出力は$\operatorname{round}(x_ {i})$です。半端がちょうど0.5なら、この非負の範囲では大きい方へ丸めます。

**入力形式**

個数n、続いてn個の目標整数。nは1〜50、各目標は0〜10。

**出力形式**

1行目に出力整数列、2行目にその整数列に対する目的値。

**サンプルコード：`example14_integer_output.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 実数候補から整数出力を作る

using namespace std;

int main() {
    // 入力条件：nは1〜50、目標は0〜10の整数。採点と最終出力に同じlroundを使う。
    int n;
    if (!(cin >> n) || n < 1) {
        return 1;
    }

    vector<int> target(n);

    for (int& value : target) {
        cin >> value;
    }

    BoxProblem problem; // 探索する配列自体は実数
    problem.bounds.assign(n, BoxBound{0, 10}); // 丸めた後も0〜10の整数になる範囲

    auto objective = [&](span<const double> x) { // 丸めで不連続になるので勾配は渡さない
        double cost = 0;

        for (int i = 0; i < n; ++i) {
            // 出力と同じ丸め方で採点する
            double error = static_cast<double>(lround(x[i])) - target[i];
            cost += error * error;
        }

        return cost; // 同じ整数へ丸められる候補の値は等しくなる
    };

    BoxParam param; // 値だけを使う手法を選ぶ
    param.method = BoxParam::Method::Coordinate; // 微分不要の座標探索
    param.search = BoxParam::Search::Explore; // 平らな場所で停滞しても残り予算で再始動する
    param.seed = 7; // 再現用の内部乱数seed
    BoxOptimizer solver(problem, param); // これは厳密な整数最適化ソルバーではない

    BoxBudget budget; // 探索量と目標値
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 20000; // 最大20000評価
    budget.target_cost = 0; // 全整数が目標に一致すれば終了

    auto result = solver.solve(objective, budget); // 初期解は各成分5
    // 有効な実数候補を確認
    if (!result.has_value()) {
        return 1;
    }

    // 採点時と同じ変換で整数として出力
    for (double value : result.x) {
        cout << lround(value) << ' ';
    }

    // 変換後の整数に対する目的値
    cout << '\n' << fixed << setprecision(6) << result.cost << '\n';
}
```


**入力例**

```text
3
2 7 4
```


**出力例（標準出力）**

```text
2 7 4 
0.000000
```


**利用上の注意**

丸めによる平らな場所ができるため、勾配は渡しません。探索しても整数最適解を必ず得る保証はなく、整数同士の厳しい制約を扱う専用ソルバーではありません。

### 3.17 例15：代理目的を改善しながら、本来の良い出力を保つ

探索を進めやすい滑らかな式と、提出結果の本当の採点が異なる場面です。代理目的だけを信じず、実際の出力の最良記録を外側で持ちます。

**具体的に解く問題**

実際の出力は0〜10の整数2個で、目標整数からの二乗誤差を本来の悪さとします。一方、ライブラリには別の代理目標へ近づく実数の二乗誤差を返します。入力例は、代理目標5,5へ進むと、本来の目標2,7から離れることを意図的に示します。

**入力形式**

`本来の目標2個 代理目標2個 初期解2個`を1行に記述。全て0〜10、本来の目標は整数。

**出力形式**

1行目は保存した本来の最良整数出力、2行目はその本来の目的値。3行目はライブラリの代理最良値と、その点を整数化したときの本来の目的値。

**サンプルコード：`example15_surrogate.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 探索用の目的と本来の採点を分ける

using namespace std;

int main() {
    // 入力条件：目標整数2個、代理目標2個、初期解2個はいずれも0〜10。
    // 初期解も本来の採点で評価して保存する。
    array<int, 2> target;
    array<double, 2> proxy_target;

    vector<double> initial(2);
    if (!(cin >> target[0] >> target[1] >> proxy_target[0] >> proxy_target[1] >> initial[0]
          >> initial[1])) {
        return 1;
    }

    // 候補を出力用整数へ変換
    auto decode = [](span<const double> x) {
        return array<int, 2>{static_cast<int>(lround(x[0])),
                             static_cast<int>(lround(x[1]))};
    };

    auto actual_cost = [&](array<int, 2> output) {
        double a = output[0] - target[0], b = output[1] - target[1];
        return a * a + b * b;
    };

    auto best_output = decode(initial);
    double best_cost = actual_cost(best_output);

    BoxProblem problem; // 実数の2変数を探索する
    problem.bounds = {{0, 10}, {0, 10}}; // 初期解と代理目標もこの範囲に置く

    auto objective = [&](span<const double> x) { // 評価された全候補を本来の採点でも確認する
        auto output = decode(x); // ライブラリへ返す値とは別に、出力候補を作る
        double actual = actual_cost(output);
        if (actual < best_cost) {
            best_cost = actual;
            best_output = output;
        }

        // ライブラリが最小化するのはこちらの代理目的
        double a = x[0] - proxy_target[0], b = x[1] - proxy_target[1];
        return a * a + b * b; // best_costで戻り値を変えない。同じxには常に同じ値を返す
    };

    BoxParam param; // 滑らかな代理目的を局所改善
    param.search = BoxParam::Search::Refine; // この例では値だけで改善する
    BoxOptimizer solver(problem, param); // 本来の最良出力は利用者側で保持する

    BoxBudget budget; // 本来の採点を追加した時間もコールバックの費用に含まれる
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 3000; // 最大3000候補を評価

    auto result = solver.solve(objective, budget, initial); // 結果のcostは代理目的の値

    cout << best_output[0] << ' ' << best_output[1] << '\n';
    cout << fixed << setprecision(6) << best_cost << '\n';
    // 代理最良と本来の採点の食い違いを確認
    if (result.has_value()) {
        cout << result.cost << ' ' << actual_cost(decode(result.x)) << '\n';
    }
}
```


**入力例**

```text
2 7 5 5 2 7
```


**出力例（標準出力）**

```text
2 7
0.000000
0.000000 13.000000
```


**利用上の注意**

全候補の本来の採点も行うため、その費用が増えます。記録する副作用はあっても、同じ候補に返す代理値は変えません。入力例の初期解が既に本来の正解なのは、悪化を防ぐ必要を明確に示すためです。

### 3.18 例16：整数の装置台数と連続的な出力を同時に調整する

何台の装置を使うかは整数、1台ごとの出力は小数、という混合した設定を扱う場面です。台数だけを丸め、出力は連続値のまま使います。

**具体的に解く問題**

整数台数$m$は1〜10、各装置の出力$s$は0〜10、需要は$d$です。目的値を$(ms-d)^2+0.05m+0.01s^2$とし、需要とのずれと装置・出力の費用を小さくします。ライブラリの第1変数を丸めて$m$にします。

**入力形式**

需要dを1個。0〜100。

**出力形式**

整数台数、1台の出力、費用を含む目的値。

**サンプルコード：`example16_mixed_controls.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 整数化する成分と連続成分を同じ候補に持つ

using namespace std;

int main() {
    // 入力条件：需要は0〜100。台数だけを丸め、出力は実数のまま採点・出力する。
    double demand;
    if (!(cin >> demand) || demand < 0) {
        return 1;
    }

    BoxProblem problem; // x[0]は装置台数の元値、x[1]は1台の出力
    problem.bounds = {{1, 10}, {0, 10}}; // 台数は1〜10、出力は0〜10

    auto objective = [=](span<const double> x) { // 台数を丸めるため勾配は渡さない
        double machines = static_cast<double>(lround(x[0])); // 出力する整数台数で採点する
        double error = machines * x[1] - demand; // 合計出力と需要との差
        // 不足・過剰と、装置・出力の費用をまとめる
        return error * error + 0.05 * machines + 0.01 * x[1] * x[1];
    };

    BoxParam param; // 複数の台数候補を探索する
    param.seed = 19; // 内部乱数を固定する
    param.search = BoxParam::Search::Explore; // 停滞後も探索を続ける
    BoxOptimizer solver(problem, param); // Autoの勾配なし経路を使う

    BoxBudget budget; // 探索候補数を制限する
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 20000; // 最大20000評価

    auto result = solver.solve(objective, budget); // 中央の初期候補から始める
    // 結果がある場合のみ出力
    if (!result.has_value()) {
        return 1;
    }

    // 台数だけ整数へ変換する
    cout << lround(result.x[0]) << ' ' << fixed << setprecision(6) << result.x[1] << ' '
         << result.cost << '\n';
}
```


**入力例**

```text
12
```


**出力例（標準出力）**

```text
4 2.998126 0.289944
```


**利用上の注意**

需要を完全一致させる等式制約ではなく、ずれに費用を付ける問題です。そのため最良点でも小さな不足や過剰があり得ます。

### 3.19 例17：固定した複数状況の平均誤差を小さくする

装置の効率が状況で変わるが、1つの指示値を選ぶ必要がある場面です。平均的に良い設定を探します。

**具体的に解く問題**

指示出力$x$は0〜10、状況$i$の効率は$e_ {i}$、目標応答は$t$です。実際の応答は$e_ {i}x$とし、$f(x)=\frac{1}{n}\sum_ {i=0}^{n-1}(e_ {i}x-t)^2$を小さくします。$n$は固定した状況数です。

**入力形式**

`n target`、続いてn個の効率。nは1〜50、効率は0より大きく10以下、targetは0〜100。

**出力形式**

指示出力と、固定状況群での平均二乗誤差。

**サンプルコード：`example17_average_scenarios.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 固定した状況群で同じ候補を採点する

using namespace std;

int main() {
    // 入力条件：nは1〜50、効率は0より大きく10以下、目標応答は0〜100。状況群は探索中不変。
    int n;
    double target;
    if (!(cin >> n >> target) || n < 1) {
        return 1;
    }

    vector<double> efficiency(n);

    for (double& value : efficiency) {
        cin >> value;
    }

    BoxProblem problem; // 装置への指示出力を調整する
    problem.bounds = {{0, 10}}; // 指示出力の範囲

    // 毎回同じefficiencyを使い、新しい乱数を引かない
    auto objective = [&](span<const double> x) {
        double cost = 0;

        for (double e : efficiency) {
            double error = e * x[0] - target; // 状況ごとの実際の応答と目標のずれ
            cost += error * error;
        }

        return cost / static_cast<double>(n); // 全状況を等しい重みで平均する
    };

    BoxParam param; // 決定的な平均目的を改善する
    param.search = BoxParam::Search::Refine; // 局所改善で終了する
    // 状況群を変えるならresumeではなく新しいsolveを使う
    BoxOptimizer solver(problem, param);

    BoxBudget budget; // 今回の評価予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 2000; // 最大2000評価

    // param.seedは目的関数側の乱数を固定する機能ではない
    auto result = solver.solve(objective, budget);
    // 有効な結果を確認
    if (!result.has_value()) {
        return 1;
    }

    // 指示出力と平均二乗誤差
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.cost << '\n';
}
```


**入力例**

```text
4 5
0.7 0.9 1.1 1.3
```


**出力例（標準出力）**

```text
4.761905 1.190476
```


**利用上の注意**

毎回新たな状況を抽選するのではなく同じ群を使います。平均値は未知の状況での良さを保証しないので、実用では探索に使わなかった状況でも確認します。

### 3.20 例18：最悪の状況での誤差を小さくする

平均の良さより、悪い状況で大きく外れないことを重視する場面です。全状況を平均せず、一番悪かった誤差を採点値にします。

**具体的に解く問題**

指示出力$x$は0〜10、状況の効率は$e_ {i}$、目標は$t$。目的値は$f(x)=\max_ {i}\lvert e_ {i}x-t\rvert$です。$\max$は候補となる値の最大を取る意味です。入力例では低効率と高効率のずれを釣り合わせます。

**入力形式**

`n target`、続いてn個の効率。nは1〜50、効率は0より大きく10以下、targetは0〜100。

**出力形式**

指示出力と最悪絶対誤差。

**サンプルコード：`example18_worst_scenario.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 最悪の状況を重視した目的を作る

using namespace std;

int main() {
    // 入力条件：nは1〜50、効率は0より大きく10以下、目標応答は0〜100。
    // 入力した状況だけの最悪値を扱う。
    int n;
    double target;
    if (!(cin >> n >> target) || n < 1) {
        return 1;
    }

    vector<double> efficiency(n);

    for (double& value : efficiency) {
        cin >> value;
    }

    BoxProblem problem; // 全状況に共通の指示出力
    problem.bounds = {{0, 10}}; // 出力の許容範囲

    auto objective = [&](span<const double> x) { // 最大値の切替点で微分しにくいので勾配なし
        double worst = 0;

        // 最も悪い状況の絶対誤差
        for (double e : efficiency) {
            worst = max(worst, abs(e * x[0] - target));
        }

        return worst; // 小さくすると、この固定状況群での最悪誤差が小さくなる
    };

    BoxParam param; // 手法を値だけの座標探索に指定
    param.method = BoxParam::Method::Coordinate; // 勾配は不要
    param.search = BoxParam::Search::Refine; // 局所停止まで改善
    BoxOptimizer solver(problem, param); // 探索中は状況群を固定する

    BoxBudget budget; // 評価回数で予算を指定する
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 3000; // 最大3000評価

    auto result = solver.solve(objective, budget); // 箱の中央から開始
    // 評価済み解が必要
    if (!result.has_value()) {
        return 1;
    }

    // 指示出力と最悪絶対誤差
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.cost << '\n';
}
```


**入力例**

```text
2 5
0.5 1.5
```


**出力例（標準出力）**

```text
5.000000 2.500000
```


**利用上の注意**

最大の誤差を持つ状況が切り替わる点で滑らかでなくなるため、勾配なしで使います。「最悪」は入力した状況群の中だけの意味です。

### 3.21 例19：観測が少ないときに、大きすぎる係数を抑える

観測へ合う係数が多数ある場合や、過度に大きな係数を避けたい場面です。データへの誤差に加え、係数そのものの大きさにも費用を付けます。このような項を正則化と呼びます。

**具体的に解く問題**

係数$a,b$は−10〜10。観測$(t_ {i},y_ {i})$に対し、$\sum_ {i=0}^{n-1}(at_ {i}+b-y_ {i})^2+w(a^2+b^2)$を小さくします。$w>0$が係数を小さくしたい強さです。観測1個でも、当てはまりと係数の小ささの両方で選べます。

**入力形式**

`n weight`、続くn行に`t[i] y[i]`。nは1〜50、各データの絶対値は10以下、weightは0より大きく100以下。

**出力形式**

係数a、b、正則化を含む目的値。

**サンプルコード：`example19_regularization.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 係数の大きさにも費用を付けて推定する

using namespace std;

int main() {
    // 入力条件：nは1〜50、データの絶対値は10以下、weightは0より大きく100以下。
    int n;
    double weight;
    if (!(cin >> n >> weight) || n < 1 || weight <= 0) {
        return 1;
    }

    vector<double> t(n), y(n);

    for (int i = 0; i < n; ++i) {
        cin >> t[i] >> y[i];
    }

    BoxProblem problem; // 直線の係数a、bを推定する
    problem.bounds = {{-10, 10}, {-10, 10}}; // 許容する係数の範囲

    // 勾配にも正則化の項を入れる
    auto objective = [&](span<const double> x, span<double> gradient) {
        double cost = weight * (x[0] * x[0] + x[1] * x[1]); // 大きすぎる係数への費用
        // 勾配が必要な呼び出しか確認
        if (!gradient.empty()) {
            gradient[0] = 2 * weight * x[0]; // aの罰則の微分
            gradient[1] = 2 * weight * x[1]; // bの罰則の微分
        }

        for (int i = 0; i < n; ++i) {
            double error = x[0] * t[i] + x[1] - y[i]; // 観測への当てはまり
            cost += error * error;
            // データの項の勾配を加える
            if (!gradient.empty()) {
                gradient[0] += 2 * error * t[i]; // aへの寄与
                gradient[1] += 2 * error; // bへの寄与
            }
        }

        return cost; // weightはライブラリ設定ではなく、この目的関数の定数
    };

    BoxParam param; // 勾配付きAutoを使う
    param.search = BoxParam::Search::Refine; // 局所改善で終了
    BoxOptimizer solver(problem, param); // 正則化は目的関数内に記述する

    BoxBudget budget; // 予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 3000; // 最大3000評価

    auto result = solver.solve(objective, budget); // a=b=0から開始
    // 有効な結果のみ出力
    if (!result.has_value()) {
        return 1;
    }

    // 係数と罰則込みの目的値
    cout << fixed << setprecision(6) << result.x[0] << ' ' << result.x[1] << ' '
         << result.cost << '\n';
}
```


**入力例**

```text
1 1
2 5
```


**出力例（標準出力）**

```text
1.666667 0.833333 4.166667
```


**利用上の注意**

weightを大きくすると、観測への誤差を受け入れても係数を小さくする傾向になります。追加した項の微分も、勾配へ忘れずに加えます。

### 3.22 例20：短い時間で改善し、評価できなければ代替解を使う

リアルタイム処理や制限時間のある問題で、短い時間だけ改善したい場面です。極端に短い時間や時間0でも有効な出力を持てるようにします。

**具体的に解く問題**

2つの値を−5〜5で動かし、目標$(t_ {x},t_ {y})$との二乗誤差を小さくします。初期解は0,0です。指定時間内に1回も評価できなければ、この初期解と事前に計算した目的値を出します。

**入力形式**

`time_us tx ty`。時間は0〜1,000,000マイクロ秒、目標座標は−5〜5。

**出力形式**

1行目に採用解と目的値、2行目に評価済み結果を使ったかの0/1とライブラリの評価回数。時間制限なので回数は実行ごとに変わります。

**サンプルコード：`example20_time_and_fallback.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 時間制限と未評価時の代替解を用意する

using namespace std;

int main() {
    // 入力条件：time_usは0〜1000000マイクロ秒、目標は各成分-5〜5。
    // 代替解の採点・入出力の時間は別に確保する。
    double time_us, tx, ty;
    if (!(cin >> time_us >> tx >> ty) || time_us < 0) {
        return 1;
    }

    BoxProblem problem; // 2変数の調整
    problem.bounds = {{-5, 5}, {-5, 5}}; // 初期解・返却解が入る箱

    auto objective = [=](span<const double> x) { // 同じ候補には同じ二乗誤差を返す
        double a = x[0] - tx, b = x[1] - ty; // 元の単位で目標との差を測る
        return a * a + b * b; // 有限値を返す
    };

    vector<double> initial = {0, 0}; // 評価できない場合も使える有効な代替解
    // 利用者が直接行う評価はresult.evaluationsに含まれない
    double fallback_cost = objective(initial);

    BoxBudget budget; // 1回の最適化に使える予算
    budget.time_limit_us = time_us; // 単位はマイクロ秒。2000なら2ミリ秒
    budget.max_evaluations = 100000; // 時間と回数のいずれかで終了

    // solverの構築も一括APIの計測に含まれる
    auto result = box_optimize(problem, objective, budget, initial);
    // 0回評価なら初期解へ戻す
    const vector<double>& output = result.has_value() ? result.x : initial;
    // 未評価の無限大を出力に使わない
    double cost = result.has_value() ? result.cost : fallback_cost;

    cout << fixed << setprecision(6) << output[0] << ' ' << output[1] << ' ' << cost
         << '\n';
    // どちらを使ったかとライブラリ内の評価回数
    cout << result.has_value() << ' ' << result.evaluations << '\n';
    // コールバックは途中中断されない。入出力・代替解の採点にも別の時間を残す。
}
```


**入力例**

```text
2000 2 -1
```


**出力例（標準出力）**

```text
2.000000 -1.000000 0.000000
1 18885
```


**利用上の注意**

代替解の採点はライブラリの予算外で行っています。実際の全体の締切にはこの処理や入出力も含めて余裕を残します。入力の時間を0にすると代替解を使う分岐を確認できます。

### 3.23 例21：同じ探索を小分けの予算で続ける

いったん結果を得た後、追加の計算時間が使えると分かった場面です。最初からやり直さず、途中まで進めた探索状態を引き継ぎます。

**具体的に解く問題**

変数$x,y$を−2〜2に制限し、$(1-x)^2+100(y-x^2)^2$を小さくします。初期解は−1.2,1です。最初の評価予算を使った後、同じ問題へ追加予算を渡します。

**入力形式**

`first_count extra_count`。最初は1〜100000、追加は0〜100000評価。

**出力形式**

1行目は最初と再開後の最良値、2行目は最初の回数・追加の回数・累計回数。

**サンプルコード：`example21_resume.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 同じ問題へ後から追加予算を与える

using namespace std;

int main() {
    // 入力条件：最初の評価上限は1〜100000、追加は0〜100000。
    // 再開中は目的関数とその意味を変えない。
    long long first_count, extra_count;
    if (!(cin >> first_count >> extra_count) || first_count < 1 || extra_count < 0) {
        return 1;
    }

    BoxProblem problem; // 2変数が結び付いた谷のある目的関数
    problem.bounds = {{-2, 2}, {-2, 2}}; // xとyの範囲

    auto objective = [](span<const double> x) { // resume中もこの式とデータを変えない
        double a = 1 - x[0], b = x[1] - x[0] * x[0]; // 最良点はx=1、y=1だが谷が曲がっている
        return a * a + 100 * b * b; // 勾配なしで値だけを渡す
    };

    BoxOptimizer solver(problem); // Exploreなので局所停滞後も続ける

    BoxBudget budget; // 最初の呼び出しの予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = first_count; // 最初に使う評価回数

    vector<double> initial = {-1.2, 1}; // 箱内の出発点

    auto first = solver.solve(objective, budget, initial); // 探索と乱数状態を新しく開始する

    // 累計上限ではなく、この次の呼び出しに追加する回数
    budget.max_evaluations = extra_count;

    // 状態を引き継ぐ。solveし直すと別の新規探索になる
    auto second = solver.resume(objective, budget);
    // firstの配列も独立して保持されている
    if (!first.has_value() || !second.has_value()) {
        return 1;
    }

    // 再開前後の最良値
    cout << fixed << setprecision(6) << first.cost << ' ' << second.cost << '\n';
    // 各回と累計の評価回数
    cout << first.evaluations << ' ' << second.evaluations << ' '
         << second.total_evaluations << '\n';
}
```


**入力例**

```text
50 2000
```


**出力例（標準出力）**

```text
4.814849 0.012046
50 2000 2050
```


**利用上の注意**

目的関数や参照データを変えないことが条件です。目標が変わったのにresumeすると、過去の目的値との比較が不正になります。

### 3.24 例22：いったん十分な解を得てから、目標を厳しくする

まず早く使える解を確保し、さらに精度が必要になったら改善を続けたい場面です。問題は変えず、終了の基準だけを小さくします。

**具体的に解く問題**

$x_ {0}$は0〜10、$x_ {1}$は−5〜5とし、$(x_ {0}-2)^2+4(x_ {1}+1)^2+0.5(x_ {0}+x_ {1}-1)^2$を小さくします。最小値0が分かる関数に対し、粗い目標と厳しい目標を順に指定します。

**入力形式**

`coarse fine`。有限値で0<fine<coarse、教材では1e−12<=fineかつcoarse<=1000を想定。

**出力形式**

1行目は2段階の目的値、2行目はそれぞれTargetReachedで止まったかの0/1。

**サンプルコード：`example22_tighten_target.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 粗い目標に到達した後、さらに厳しい目標へ進める

using namespace std;

int main() {
    // 入力条件：目標は有限で1e-12<=fine<coarse<=1000。
    // 目標の変更だけなので同じ目的関数でresumeできる。
    double coarse, fine;
    if (!(cin >> coarse >> fine) || fine <= 0 || coarse <= fine) {
        return 1;
    }

    BoxProblem problem; // 2変数の二乗誤差
    problem.bounds = {{0, 10}, {-5, 5}}; // 初期解と最良点を含む箱

    // 値と元の変数の勾配を返す
    auto objective = [](span<const double> x, span<double> gradient) {
        double a = x[0] - 2, b = x[1] + 1, c = x[0] + x[1] - 1; // 目標点は2、-1
        // 勾配が要求されたときだけ書く
        if (!gradient.empty()) {
            gradient[0] = 2 * a + c; // x[0]についての微分
            gradient[1] = 8 * b + c; // x[1]についての微分
        }

        return a * a + 4 * b * b + 0.5 * c * c; // 非負で、目標点では0
    };

    BoxParam param; // 勾配付きAutoで局所改善
    // LocalStopした場合は同じ状態のresumeでも進まない
    param.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, param); // 2段階で共通の探索状態を持つ

    BoxBudget budget; // 呼び出しごとの予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 2000; // 各呼び出し最大2000評価
    budget.target_cost = coarse; // まず粗い目的値に達すれば止める

    vector<double> initial = {8, 4}; // 箱内の出発点

    auto first = solver.solve(objective, budget, initial); // 最初の探索を開始する

    // 値が小さいほど厳しい目標。budget.target_cost.reset()なら目標判定を外せる
    budget.target_cost = fine;

    // 同じ関数のまま、目標だけを厳しくして続ける
    auto second = solver.resume(objective, budget);
    // 有効な最良点があるか確認
    if (!first.has_value() || !second.has_value()) {
        return 1;
    }

    // 2段階の目的値
    cout << scientific << setprecision(6) << first.cost << ' ' << second.cost << '\n';
    // 到達したかは終了理由で確認する
    cout << (first.stop == BoxResult::Stop::TargetReached) << ' '
         << (second.stop == BoxResult::Stop::TargetReached) << '\n';
}
```


**入力例**

```text
0.01 1e-10
```


**出力例（標準出力）**

```text
3.522675e-06 1.079740e-13
1 1
```


**利用上の注意**

同じ目標のままresumeすると、新しい評価なしで目標到達になります。LocalStopや予算不足で終了する場合もあるため、終了理由を確認します。

### 3.25 例23：ターンごとに変わる目標へ、前の解から合わせ直す

需要や状況が毎ターン少しずつ変わり、前回の設定を出発点にしたい場面です。箱とオブジェクトを再利用しながら、採点を新しくやり直します。

**具体的に解く問題**

2つの設定は−10〜10で、各ターンの新しい目標への二乗誤差を小さくします。最初は箱の中央、2回目以降は前回の最良点から始めます。

**入力形式**

ターン数turns、続く各行に新しい目標2個。turnsは1〜50、目標は−10〜10。

**出力形式**

各行にターン番号、設定2個、今ターンの目的値。

**サンプルコード：`example23_turns.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 需要が変わるターンごとに前解を初期解として利用する

using namespace std;

int main() {
    // 入力条件：turnsは1〜50、目標は各成分-10〜10。
    // targetの更新はsolveの呼び出しと呼び出しの間だけで行う。
    int turns;
    if (!(cin >> turns) || turns < 1) {
        return 1;
    }

    array<double, 2> target{};

    BoxProblem problem; // 毎ターン同じ2変数の箱を使う
    problem.bounds = {{-10, 10}, {-10, 10}}; // 目標もこの箱内の入力を想定する

    // targetを参照するが、solve実行中は変えない
    auto objective = [&](span<const double> x, span<double> gradient) {
        double a = x[0] - target[0], b = x[1] - target[1]; // 今ターンの目標との差
        // 勾配要求時だけ全2成分を書く
        if (!gradient.empty()) {
            gradient[0] = 2 * a; // x[0]の傾き
            gradient[1] = 2 * b; // x[1]の傾き
        }

        return a * a + b * b; // ターンが変わると目的関数の意味も変わる
    };

    BoxParam param; // 毎回局所改善する
    param.search = BoxParam::Search::Refine; // 停滞したら終了
    BoxOptimizer solver(problem, param); // 箱の前処理や作業領域を繰り返し使う

    BoxBudget budget; // 1ターン当たりの予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 2000; // 全ターン合計ではなく各ターンの評価上限
    budget.target_cost = 1e-10; // 今ターンの目的値に対する目標
    vector<double> previous; // 空なら初回は箱の中央から始まる

    cout << fixed << setprecision(6);

    for (int turn = 0; turn < turns; ++turn) {
        cin >> target[0] >> target[1];

        // 目的が変わったのでresumeではなく新しいsolve
        auto result = solver.solve(objective, budget, previous);
        // この回の有効な結果を確認
        if (!result.has_value()) {
            return 1;
        }

        previous = result.x; // 結果は所有配列なので次の初期解として保存できる

        // ターンと設定と目的値
        cout << turn << ' ' << result.x[0] << ' ' << result.x[1] << ' ' << result.cost
             << '\n';
    }
}
```


**入力例**

```text
3
2 -1
2.1 -0.95
2.2 -0.9
```


**出力例（標準出力）**

```text
0 2.000000 -1.000000 0.000000
1 2.100000 -0.950000 0.000000
2 2.200000 -0.900000 0.000000
```


**利用上の注意**

同じラムダの変数targetを書き換えても、採点の意味は変わります。resumeは使わずsolveを呼び直します。累計評価も各solveで数え直します。

### 3.26 例24：使える範囲や変数数が変わったら問題を入れ替える

操作できる範囲が途中で狭まる、固定する項目が増える、設定項目そのものが増減する場面です。古い初期解を新しい箱へ収めてから、新しい探索を始めます。

**具体的に解く問題**

各段階で新しいn個の上下限と目標を受け取り、各目標からの二乗誤差の和を小さくします。同じ添字は同じ設定を意味するとし、残っている成分は前解を利用します。新しい成分は箱の中央に置きます。

**入力形式**

段階数stages。各段階にn、その後n行の`lower upper target`。stagesとnは1〜50、有限な値でlower<=upper、各値の絶対値は100以下。

**出力形式**

各行に段階番号、目的値、今回のn個の設定。

**サンプルコード：`example24_reset_boxes.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 制約や変数数を変えるときにresetする

using namespace std;

int main() {
    // 入力条件：stagesと各nは1〜50。上下限と目標は絶対値100以下、lower<=upper。
    // 同じ添字は同じ設定を意味する。
    int stages;
    if (!(cin >> stages) || stages < 1) {
        return 1;
    }

    BoxParam param; // 各段階で引き続き使う設定
    param.search = BoxParam::Search::Refine; // 局所改善の方針
    // 空の問題で作り、最初のresetで実際の箱を設定する
    BoxOptimizer solver(BoxProblem{}, param);

    BoxBudget budget; // 各段階の予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 3000; // 各段階で最大3000評価
    vector<double> previous;

    cout << fixed << setprecision(6);

    for (int stage = 0; stage < stages; ++stage) {
        int n;
        cin >> n;
        if (n < 1) {
            return 1;
        }

        BoxProblem next; // この段階の新しい上下限
        next.bounds.resize(n); // 変数数の変更にも対応する
        vector<double> target(n), initial(n);

        for (int i = 0; i < n; ++i) {
            double lower, upper;
            cin >> lower >> upper >> target[i];
            next.bounds[i] = {lower, upper}; // lower==upperならその成分を固定する
            double old = i < static_cast<int>(previous.size())
                             ? previous[i]
                             : lower + (upper - lower) * 0.5;
            // 新しい箱外の前解は利用者側で箱内へ調整する
            initial[i] = clamp(old, lower, upper);
        }

        auto objective = [&](span<const double> x) { // この段階の変数数nに合う目的関数
            double cost = 0;

            // 各成分の二乗誤差
            for (int i = 0; i < n; ++i) {
                cost += (x[i] - target[i]) * (x[i] - target[i]);
            }

            return cost; // targetはこのsolve中だけ参照される
        };
        // 箱と設定を入れ替える。paramを省略すると設定は既定値へ戻る
        solver.reset(next, param);

        // reset後はresumeでなくsolveで新しく始める
        auto result = solver.solve(objective, budget, initial);
        // 評価済み結果を確認
        if (!result.has_value()) {
            return 1;
        }

        previous = result.x; // 次の段階の初期解候補にする

        cout << stage << ' ' << result.cost; // この段階の目的値

        // 結果の長さは今回のn
        for (double value : result.x) {
            cout << ' ' << value;
        }

        cout << '\n';
    }
}
```


**入力例**

```text
3
2
0 10 2
-5 5 -1
2
0 1 2
-0.5 0.5 -1
3
0 1 0.4
2 2 3
-1 1 0.2
```


**出力例（標準出力）**

```text
0 0.000000 2.000000 -1.000000
1 1.250000 1.000000 -0.500000
2 1.000000 0.400000 2.000000 0.200000
```


**利用上の注意**

resetは探索履歴を破棄します。続いて呼ぶのはsolveです。古いparamを引き継ぎたい場合は、resetの第2引数へ明示的に渡します。

### 3.27 例25：全体の時間枠を複数の初期解で共有する

複数の谷があり、出発点によって結果が変わり得る問題で、いくつかの初期解を明示的に試したい場面です。各回へ全体時間を丸ごと渡すのでなく、残り時間から配分します。

**具体的に解く問題**

変数$x,y$は−2〜2。$a=x-0.3$、$b=y+0.2$と置き、$a^2+b^2+0.2(2-\cos(10a)-\cos(10b))$を小さくします。$a,b$はずれを表す補助量です。入力読み取り後の指定時間を3実行で共有します。

**入力形式**

時間枠limit_us、続く3行に初期解2成分ずつ。時間は0〜1,000,000マイクロ秒、全初期成分は−2〜2。

**出力形式**

1行目は全実行中の最良解と目的値、2行目は終了した実行数。時間に依存して結果・回数は変わります。

**サンプルコード：`example25_multistart.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 外側の締切を共有して独立した探索を行う

using namespace std;

int main() {
    // 入力条件：limit_usは0〜1000000マイクロ秒、3個の初期解は各成分-2〜2。
    // 入力後の探索枠を共有する。
    double limit_us;
    if (!(cin >> limit_us) || limit_us < 0) {
        return 1;
    }

    array<array<double, 2>, 3> starts;

    for (auto& point : starts) {
        cin >> point[0] >> point[1];
    }

    BoxProblem problem; // 複数の谷のある2変数の問題
    problem.bounds = {{-2, 2}, {-2, 2}}; // 全初期解をこの箱内に用意する

    auto objective = [](span<const double> x) { // どの実行でも同じ目的関数を使う
        double a = x[0] - 0.3, b = x[1] + 0.2; // 最良点からのずれ
        return a * a + b * b
               + 0.2 * (2 - cos(10 * a) - cos(10 * b)); // 二乗誤差に細かい起伏を加える
    };

    using Clock = chrono::steady_clock;
    auto start = Clock::now();
    vector<double> best(starts[0].begin(), starts[0].end());
    double best_cost = objective(best); // 探索できない場合の有効な出力を準備する
    int completed = 0;

    for (int k = 0; k < 3; ++k) {
        double used = chrono::duration<double, micro>(Clock::now() - start).count();
        double remaining = limit_us - used;
        if (remaining <= 0) {
            break;
        }

        BoxBudget budget; // 今回の独立実行に割り当てる予算
        budget.time_limit_us = remaining / (3 - k); // 残り時間を残りの実行数で割る
        budget.max_evaluations = 100000; // 回数の上限も設ける

        BoxParam param; // 各回に別の乱数列を使う
        param.method = BoxParam::Method::DiagonalCma; // 最初から対角CMA系を使う
        param.seed = static_cast<uint64_t>(100 + k); // seedは100、101、102

        // array<double,2>も初期解へ渡せる
        auto result = box_optimize(problem, objective, budget, starts[k], param);
        // 実際に評価した結果だけ比較する
        if (result.has_value() && result.cost < best_cost) {
            best = result.x; // 最良候補を外側へ保存する
            best_cost = result.cost; // 最良の目的値を更新する
        }

        ++completed;
    }

    cout << fixed << setprecision(6) << best[0] << ' ' << best[1] << ' ' << best_cost
         << '\n';
    cout << completed << '\n';
    // コールバックの途中中断はできない。実際の締切には出力処理分などの余裕を確保する。
}
```


**入力例**

```text
20000
1.5 -1.4
-1.2 1.1
0 0
```


**出力例（標準出力）**

```text
0.300000 -0.200000 0.000000
3
```


**利用上の注意**

各回は独立した探索で、seedは100、101、102です。Explore自体にも停滞後の継続があるため、外側の複数実行が常に優れるとは限りません。厳密な時刻での強制中断はできません。

### 3.28 例26：同じ関数で4手法を比較する

目的関数の評価を作れた後、どの手法を候補にするかを確かめる場面です。値だけの手法と勾配を使う手法へ、同じコールバックを渡します。

**具体的に解く問題**

2変数を−2〜2で動かし、ずれ$a=x-0.3,b=y+0.2$に対して$a^2+b^2+0.2(2-\cos(10a)-\cos(10b))$を小さくします。初期解・seed・評価回数を揃え、Auto、Coordinate、DiagonalCma、SpectralGradientを独立に実行します。

**入力形式**

`evaluations seed`。評価上限は1〜100000、seedは0以上のuint64_tに収まる整数。

**出力形式**

手法ごとに、名前、目的値、目的評価回数、勾配評価回数。

**サンプルコード：`example26_compare_methods.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 同じ問題で公開されている4手法を使い分ける

using namespace std;

int main() {
    // 入力条件：evaluationsは1〜100000、seedはuint64_tの範囲内。回数を揃えた比較であり、
    // 実行時間は揃えていない。
    long long evaluations;
    uint64_t seed;
    if (!(cin >> evaluations >> seed) || evaluations < 1) {
        return 1;
    }

    BoxProblem problem; // 比較する全手法で箱を揃える
    problem.bounds = {{-2, 2}, {-2, 2}}; // 2変数の許容範囲

    // 勾配不要の手法では第2引数が空になる
    auto objective = [](span<const double> x, span<double> gradient) {
        double a = x[0] - 0.3, b = x[1] + 0.2; // 最良点からのずれ
        // CoordinateとDiagonalCmaにも同じ関数を渡せるよう空を確認
        if (!gradient.empty()) {
            gradient[0] = 2 * a + 2 * sin(10 * a); // x[0]の微分
            gradient[1] = 2 * b + 2 * sin(10 * b); // x[1]の微分
        }

        return a * a + b * b
               + 0.2 * (2 - cos(10 * a) - cos(10 * b)); // 値の式は全手法で共通
    };

    using Method = BoxParam::Method; // 公開手法の列挙型

    // 4値をすべて比較
    const array<Method, 4> methods = {
        Method::Auto,             // この二引数形式では勾配を使う
        Method::Coordinate,       // 座標ごとに値だけで調べる
        Method::DiagonalCma,      // 値だけで候補群と広がりを調整する
        Method::SpectralGradient  // 勾配を使い、歩幅を調整する
    };
    const array<const char*, 4> names = {
        "Auto",
        "Coordinate",
        "DiagonalCma",
        "SpectralGradient"
    };

    vector<double> initial = {1.5, -1.4}; // 全手法で同じ出発点

    BoxBudget budget; // 回数を揃える。勾配計算の費用は異なるので時間比較とは別
    budget.time_limit_us = -1; // 実行時間の揺れで打ち切る位置が変わるのを避ける
    budget.max_evaluations = evaluations; // 各手法で使える評価回数

    cout << fixed << setprecision(6);

    for (int i = 0; i < 4; ++i) {
        BoxParam param; // 既定のExploreで停滞後も続ける
        param.method = methods[i]; // 今回の手法を明示する
        param.seed = seed; // 乱数seedを揃えるが、各手法で同じ候補列になるわけではない

        // 手法ごとに独立して実行
        auto result = box_optimize(problem, objective, budget, initial, param);
        // 評価済み解を確認
        if (!result.has_value()) {
            return 1;
        }

        // 値と2種類の回数を出す
        cout << names[i] << ' ' << result.cost << ' ' << result.evaluations << ' '
             << result.gradient_evaluations << '\n';
    }
}
```


**入力例**

```text
5000 7
```


**出力例（標準出力）**

```text
Auto 0.000000 5000 5000
Coordinate 0.000000 5000 0
DiagonalCma 0.000000 5000 0
SpectralGradient 0.000000 5000 5000
```


**利用上の注意**

同じ評価回数でも勾配の費用は異なり、速度順位の比較ではありません。実用上の判断では、実際の問題群で時間予算も揃えて比較します。掲載結果はこの入力での一例です。

### 3.29 例27：勾配を小さな差分で検算してから使う

勾配の式を自分で書いたとき、添字や符号の間違いを早く見つけたい場面です。左右へ少し動かした目的値から傾きを近似し、自作した式と比べます。

**具体的に解く問題**

2変数を−5〜5に制限し、$(x_ {0}-2)^2+4(x_ {1}+1)^2+0.5(x_ {0}+x_ {1}-1)^2$を扱います。検算点で1成分だけ幅$h=10^{-6}$ずつ動かし、左右の値の差を$2h$で割った値を、その成分の勾配と比較します。

**入力形式**

検算点の2成分。どちらも−4〜4で、箱の境界から離れた点にします。

**出力形式**

微分検算での最大絶対誤差と、探索後の目的値。誤差が1e−5を超えた場合はGRADIENT_ERRORと出して終了します。

**サンプルコード：`example27_check_gradient.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 自分で書いた勾配を検算してから使う

using namespace std;

int main() {
    // 入力条件：検算点は両成分-4〜4。左右へ1e-6動かしても箱内の点で検算する。
    vector<double> point(2);
    if (!(cin >> point[0] >> point[1])) {
        return 1;
    }

    // 値と解析的な勾配を同時に計算する
    auto objective = [](span<const double> x, span<double> gradient) {
        double a = x[0] - 2, b = x[1] + 1, c = x[0] + x[1] - 1; // 2変数が結び付く二次関数
        // 検算時には空配列で値だけ要求することもある
        if (!gradient.empty()) {
            gradient[0] = 2 * a + c; // x[0]についての偏微分
            gradient[1] = 8 * b + c; // x[1]についての偏微分
        }

        return a * a + 4 * b * b + 0.5 * c * c; // 勾配と同じ式の目的値
    };

    vector<double> gradient(2);
    // 利用者による直接呼び出し。ライブラリの評価回数には入らない
    objective(point, gradient);
    const double h = 1e-6;
    double max_error = 0;

    for (int i = 0; i < 2; ++i) {
        auto left = point, right = point;
        left[i] -= h;
        right[i] += h;
        // 空の勾配配列を渡して値だけ計算する
        double slope = (objective(right, {}) - objective(left, {})) / (2 * h);
        max_error = max(max_error, abs(slope - gradient[i]));
    }

    if (max_error > 1e-5) {
        cout << "GRADIENT_ERROR\n";
        return 1;
    }

    BoxProblem problem; // 検算点はこの箱の内側に置く
    problem.bounds = {{-5, 5}, {-5, 5}}; // 入力例では点から左右へh動かしても箱内

    BoxParam param; // 既定のAutoは二引数形式を見て勾配を利用する
    param.search = BoxParam::Search::Refine; // 局所改善
    // 数値微分の検算はライブラリが自動で行う処理ではない
    BoxOptimizer solver(problem, param);

    BoxBudget budget; // solve中の予算
    budget.time_limit_us = -1; // 時間上限なし
    budget.max_evaluations = 2000; // 最大2000評価

    auto result = solver.solve(objective, budget, point); // 検算点を初期解として使う
    // 有効な結果を確認
    if (!result.has_value()) {
        return 1;
    }

    // 微分の最大誤差と最良目的値
    cout << scientific << setprecision(6) << max_error << ' ' << result.cost << '\n';
}
```


**入力例**

```text
1.5 0.2
```


**出力例（標準出力）**

```text
4.627179e-10 3.028405e-25
```


**利用上の注意**

hが小さすぎても丸め誤差が増えます。境界や微分が定まらない折れ曲がりでは同じ検査を機械的に適用できません。1点での検算は全範囲の正しさの証明ではありません。

### 3.30 例28：全固定・変数数0・未評価を同じコードで扱う

全設定が確定している部分問題や、対象が1つも残らない場合にも、呼び出し側の処理を共通にしたい場面です。空配列だから失敗、という誤判定を避けます。

**具体的に解く問題**

n個の入力値を全て固定し、$f(x)=7+\sum_ {i=0}^{n-1}x_ {i}^{2}$を評価します。n=0なら和の部分は0なので目的値は7です。評価予算0では目的関数を呼ばず、未評価を返します。

**入力形式**

`n evaluations`、続いてn個の固定値。nは0〜100000、値の絶対値は100以下、評価上限は0〜100000。

**出力形式**

標準出力の1行目はhas_value、返却配列の長さ、評価回数、勾配評価回数。2行目は目的値またはNONE。標準エラーには結果全体のデバッグ表示。

**サンプルコード：`example28_no_variables_to_move.cpp`**

```cpp
#include "box_optimizer_v06.hpp" // 全固定・変数数0・未評価の結果も扱う

using namespace std;

int main() {
    // 入力条件：nは0〜100000、固定値の絶対値は100以下、評価上限は0〜100000。
    // 全固定でも予算0なら未評価。
    int n;
    long long evaluations;
    if (!(cin >> n >> evaluations) || n < 0 || evaluations < 0) {
        return 1;
    }

    BoxProblem problem; // n=0なら空の問題も表現できる

    for (int i = 0; i < n; ++i) {
        double value;
        cin >> value;
        problem.bounds.push_back({value, value}); // 全成分を同じ上下限で固定する
    }

    // 全固定では勾配を要求されない
    auto objective = [](span<const double> x, span<double> gradient) {
        assert(gradient.empty()); // 空配列へ書き込まない。教材の動作確認用
        double cost = 7;

        // n=0なら値は定数7
        for (double value : x) {
            cost += value * value;
        }

        return cost; // 自由変数がなくても、この値は1回評価しないと分からない
    };

    BoxBudget budget; // 0回と1回以上を入力で切り替えられる
    budget.time_limit_us = -1; // 時計では止めない
    // 全固定なので、上限が大きくても初回は最大1回だけ評価する
    budget.max_evaluations = evaluations;

    // 空初期解なら固定値または空配列から始まる
    auto result = box_optimize(problem, objective, budget);

    cerr << result << '\n'; // operator<<で直接デバッグ出力。xは先頭20要素まで表示する

    // 空配列でも有効な解の場合がある
    cout << result.has_value() << ' ' << result.x.size() << ' ' << result.evaluations << ' '
         << result.gradient_evaluations << '\n';

    // has_value()で未評価かどうか判断する
    if (result.has_value()) {
        cout << fixed << setprecision(6) << result.cost << '\n';
    } else {
        // cost=無限大は未評価の印であり、目的関数が返した値ではない
        cout << "NONE\n";
    }
}
```


**入力例**

```text
2 5
2 -1
```


**出力例（標準出力）**

```text
1 2 1 0
12.000000
```


**デバッグ出力例（標準エラー）**

時間は実行ごとに変わります。

```text
BoxResult{cost=12, stop=AllFixed, evaluations=1, gradient_evaluations=0, total_evaluations=1, elapsed_us=3.585, x=[2, -1]}
```


**利用上の注意**

全固定でも値は1回評価して初めて分かります。勾配は要求されません。xが空でも有効な解がある場合をhas_valueで判断し、ログでは長い配列を先頭20個までに省略します。

#### 同じコードへの別の入力：変数数0でも、評価すれば有効な結果になる

**入力**

```text
0 5
```


**標準出力**

```text
1 0 1 0
7.000000
```


**標準エラーの例**

```text
BoxResult{cost=7, stop=AllFixed, evaluations=1, gradient_evaluations=0, total_evaluations=1, elapsed_us=2.223, x=[]}
```


#### 同じコードへの別の入力：評価予算0では、未評価を返す

**入力**

```text
2 0
2 -1
```


**標準出力**

```text
0 0 0 0
NONE
```


**標準エラーの例**

```text
BoxResult{cost=none, stop=EvaluationLimit, evaluations=0, gradient_evaluations=0, total_evaluations=0, elapsed_us=3.695, x=[]}
```


#### 同じコードへの別の入力：25要素の結果でも、デバッグ表示は20要素まで

**入力**

```text
25 1
0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
```


**標準出力**

```text
1 25 1 0
4907.000000
```


**標準エラーの例**

```text
BoxResult{cost=4907, stop=AllFixed, evaluations=1, gradient_evaluations=0, total_evaluations=1, elapsed_us=3.775, x=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (25 total)]}
```

### 3.31 検証範囲

全28例をGCC 13.3、C++20、O2でコンパイルし、掲載した入力で実行しています。目的値と出力の整合、箱や変換後の制約、評価回数、再開時の累計、勾配の扱いを確認しています。さらに、目標が箱外にある場合、時間0、変数数0、評価予算0、25要素の表示を確認しています。検証プログラムと実行記録は同梱の`verify_samples.py`と`validation.json`です。

掲載された実行結果は、特定の入力に対する確認です。任意の目的関数での最適性、全ての初期解での収束、厳密な時間内の終了を保証するものではありません。
