# particle_filter ステップバイステップガイド

対象は `particle_filter_v11.hpp`、C++20です。未知の値を観測から推定するプログラムを、動く最小例から少しずつ組み立てます。数学は高校で学ぶ確率、平均、指数・対数を出発点にし、新しい道具は登場した場所で説明します。

これは順番に学ぶための独立したガイドです。最初に全APIを覚える必要はありません。ステップ01〜06で、基本的な推定と結果の読み方まで使えるようになります。その後は必要な題材まで読み進めてください。

| 読む範囲 | できるようになること |
| --- | --- |
| 01〜06 | 成否から成功率を学び、重み・平均・不確実性を読む |
| 07〜12 | 推定を行動選択へつなぎ、失敗・追加計算・分岐を扱う |
| 13〜20 | 実数の測定、複数パラメータ、配列、計算の再利用を扱う |
| 21〜23 | 丸め・上限表示・相手の行動という観測をモデルにする |
| 24〜31 | 種類・整数・複合状態・角度・割合・隠れた扉を扱う |
| 32〜34 | 時間とともに変わる状態を追跡する |
| 35〜40 | 必要な場合に限り、粒子群を低水準APIで直接操作する |

各ステップの「出発点」は、それまでのどのコードを拡張したかを示します。近い題材へ戻ることはありますが、未説明のAPIを前提にはしません。01が最小コード、02以降が発展コードです。各コードは、それ以前のコードを貼り足さずに動きます。

## 最初の準備

ZIPを展開すると、ヘッダー、このMarkdown、40個の独立した `.cpp` ファイルが入っています。各コード例を `main.cpp` にコピーし、同じディレクトリへ `particle_filter_v11.hpp` を置いてください。入力データは各例の `main` に入っているので、標準入力を用意する必要はありません。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic main.cpp -o main
./main
```

同梱ファイルをそのまま使う場合は、ZIPから展開した `particle_filter_step_by_step_v01` ディレクトリで次を実行します。

```bash
g++ -std=c++20 -O2 -I. particle_filter_step_by_step_examples_v01/step_01.cpp -o step_01
./step_01
```

`-std=c++20` は使用するC++の版、`-O2` は最適化、`-I.` は現在のディレクトリでヘッダーを探す指定です。`-ffast-math` と `-Ofast` は使わないでください。このライブラリは「起こり得ない」を無限大の対数値で扱います。

出力は説明用に通常の桁数で表示しています。粒子を乱数で生成するため、理論値とは多少ずれます。また、同じseedでも標準ライブラリや実装環境を変えると出力が変わり得ます。掲載した出力は、同梱コードを実際に実行したものです。

## 01. 最小コード：1回の成功から、成功率を推定する

ある装置を動かすと、成功か失敗のどちらかになります。成功する確率は一定ですが、その値は分かりません。この未知の成功率を $p$ と呼びます。例えば $p=0.8$ なら、同じ条件で何度も動かしたときに、およそ8割が成功する装置です。

最初は0から1までのどの成功率も同じ程度に考えます。実際に1回動かすと成功しました。この結果を取り込んで、成功率の代表値を表示します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{}); // 観測前の候補を用意する。
    if (!filter.observe(1).ok) return 1; // 1回の成功を取り込む。
    std::cout << filter.estimate().mean << '\n'; // 成功率の加重平均。
}
```

### コードを読む順序

1. **まず未知の値を決める。** `Particle = double` は、1個の候補が成功率を1個持つという指定です。ライブラリでは、この候補を「粒子」と呼びます。候補が0.2なら「この装置の成功率は0.2かもしれない」という仮説です。
2. **次に観測を決める。** `Observation = int` とし、成功を1、失敗を0で表します。未知の成功率と、実際に得られた1回の成否は別のものです。
3. **観測前の候補を作る。** `sample(rng)` は候補を1個返す関数です。ここでは `[0,1)` の一様乱数を返します。どの範囲の候補をどれほど用意するかという、観測前の考え方を「事前分布」と呼びます。
4. **仮説から観測の起こりやすさを計算する。** `likelihood(p, success)` は、成功したなら $p$、失敗したなら $1-p$ を返します。例えば仮説が0.8なら、成功の確率は0.8、失敗の確率は0.2です。このように、観測を固定して各仮説の説明力として使う確率を「尤度（ゆうど）」と呼びます。
5. **観測を取り込む。** `observe(1)` により、各候補の説明力を重みに反映します。`ok` は取り込めたかどうかです。この例では失敗なら終了コード1で止めています。
6. **代表値を読む。** `estimate().mean` は、各候補にその重みを掛けて足した平均です。後の例で平均以外の取り出し方も扱います。

**ユーザーが返す尤度は「その仮説が正しい確率」ではありません。「その仮説のもとで、今回の観測が得られる確率」です。** 観測からどの仮説を重く扱うかは、ライブラリが計算します。

### このコードにある型・引数・値

| 記述 | 意味と、変える場所 |
| --- | --- |
| `Model` | この問題における粒子・観測・初期候補・尤度をまとめるユーザー定義型 |
| `Particle` | 候補1個の型。この例では成功率1個なので `double` |
| `Observation` | 観測1件の型。この例では0か1を入れる `int` |
| `std::mt19937_64& rng` | ライブラリから渡される乱数器。参照で受け取り、その乱数を使って候補を作る |
| 一様乱数の `0.0, 1.0` | 観測前に可能と考える成功率の下限・上限。下限を含み、上限を含まない乱数を生成する |
| `likelihood` の `p` | 今評価する候補。こちらで書き換えない |
| `likelihood` の `success` | 今回の成否。このモデルへは0か1を渡す |
| `ParticleFilter<Model>` | `Model` の規則を使って推定するオブジェクトの型 |
| コンストラクタの `Model{}` | ユーザー定義モデルの実体。ここでは設定用メンバーがないので空の初期化 |
| `observe(1)` の `1` | 今回は成功したという観測。粒子数や探索回数ではない |
| `estimate()` | 引数を省略すると全粒子の加重平均を求める |

コンストラクタの他の引数は省略しています。この場合は1024個の粒子と、seed 0の乱数器が使われます。それらを変更する方法は03で扱います。重みの偏りへの自動処理は、まずライブラリへ任せます。

### 重みと平均を、小さな手計算で確かめる

説明のため、候補が0.2、0.5、0.8の3個だけだったとします。観測前は同じ重みです。成功後は、成功しやすい候補ほど重くなります。それぞれの尤度は0.2、0.5、0.8です。

これらの和1.5で割ると、重みは約0.133、0.333、0.533になります。和を1にすることを「正規化」と呼びます。加重平均は次の計算です。

$$\frac{0.2\times0.2+0.5\times0.5+0.8\times0.8}{0.2+0.5+0.8}=0.62$$

実際のコードでは3候補ではなく、0から1の範囲に1024候補を作ります。一様な事前分布に1回の成功を反映した理論上の平均は約0.667で、実行結果もその近くになります。

実行結果：

```text
0.665282
```

成功が1回出ただけで、成功率を1と断定するわけではありません。低い成功率でも、たまたま成功することはあります。`observe(1)` を `observe(0)` に変えると、平均は約0.333へ下がります。

## 02. 観測を増やす：同じ装置を繰り返し測る

出発点は01です。装置の成功率は変わらないまま、8回分の成否が得られた場合に広げます。変更する中心は、`observe` を新しい観測ごとに1回ずつ呼ぶことです。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    std::cout << filter.estimate().mean << '\n';
    std::cout << filter.observation_count() << ' ' << filter.history().size() << '\n';
}
```

### 何が積み重なるのか

1回目の重みを捨てて、2回目だけを評価するわけではありません。前までの重みに、今回の観測の尤度を掛けます。候補 $p$ に対して、成功なら $p$、失敗なら $1-p$ を掛けます。

今回の配列には成功が6回、失敗が2回あります。成功率が一定で、$p$ が決まったもとでは各試行が独立という仮定なので、この並びの尤度は $p^6(1-p)^2$ です。観測のたびに同じ `filter` を使うことで、前までの情報が引き継がれます。

| 新しい記述 | 意味 |
| --- | --- |
| `trials` | 実際に得られた観測の列。要素の1と0の意味は01と同じ |
| `for (int success : trials)` | 新しい観測を順番に1件ずつ読む |
| `observation_count()` | 取り込みに成功した観測数。失敗した `observe` は数えない |
| `history()` | 固定パラメータの推定で保存された観測の読み取り専用view |
| `history().size()` | 保存された観測数。ここでは8 |

実行結果：

```text
0.694825
8 8
```

この例の理論上の平均は $(6+1)/(8+2)=0.7$ です。最初の一様な考え方も残るため、単純な成功割合 $6/8=0.75$ と一致するとは限りません。

**同じ観測を、計算を良くする目的でもう一度 `observe` に入れないでください。** 別の試行がもう1回あったという意味になり、情報を二重に数えます。追加計算の方法は10で扱います。

## 03. 粒子数とseedを指定する

出発点は02です。問題の確率モデルは変えず、計算に使う候補の個数と、乱数の出発点だけを明示します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model>::Param param;
    param.count = 4096;                  // 使う仮説の個数。
    std::mt19937_64 rng(7);              // 実験を再現するためのseed。
    ParticleFilter<Model> filter(Model{}, param, rng);
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    std::cout << filter.size() << ' ' << filter.estimate().mean << '\n';
}
```

候補が少なすぎると、ありそうな範囲を十分に表せません。候補を増やすと一般に近似が細かくなりますが、観測評価や再探索の仕事も増えます。粒子数は「成功率の正解を変える値」ではなく、「近似に使う計算量を変える値」です。

| 新しい記述・値 | 意味・既定値・選び方 |
| --- | --- |
| `ParticleFilter<Model>::Param` | この推定器の計算設定をまとめる型。まず既定値で初期化する |
| `param.count = 4096` | 粒子数。型は `int`、正の値、既定値1024。まず1024、時間と推定誤差を見て増減する |
| `std::mt19937_64 rng(7)` | seed 7から乱数器を作る。7に統計上の特別な意味はない |
| 第2引数 `param` | 計算設定。指定しなかった項目は既定値のまま |
| 第3引数 `rng` | 推定に使う乱数器。オブジェクトへ値として渡される |
| `size()` | 現在の粒子数。ここでは4096 |

コンストラクタはモデル・設定・乱数器を内部に保持します。外側の `rng` を後から操作しても、内部の乱数器が連動して変わるわけではありません。

実行結果：

```text
4096 0.698766
```

公平に粒子数の効果を比べるには、観測列は固定し、複数のseedで推定誤差と実行時間を比べます。1個のseedだけで必ず良くなることは期待しないでください。

## 04. 重みの偏りと観測結果を読む

出発点は01です。推定の規則を変えず、「現在どういう状態か」を表示します。最初から内部の粒子を操作する必要はなく、読み取りだけに使います。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const auto result = filter.observe(1);
    if (!result.ok) return 1;
    std::cout << "ESS " << filter.ess() << " / " << filter.size() << '\n';
    std::cout << "log_predictive " << result.log_predictive << '\n';
    std::cout << "resampled " << result.resampled << " moves "
              << result.moves.accepted << '/' << result.moves.attempted << '\n';
    const auto particles = filter.particles(); // 読み取り専用。変更操作をまたいで保持しない。
    const auto weights = filter.weights();
    const auto log_weights = filter.log_weights();
    for (int i = 0; i < 3; ++i) {
        std::cout << i << ' ' << particles[i] << ' '
                  << weights[i] << ' ' << log_weights[i] << '\n';
    }
}
```

### 重みは和が1の重要度

`weights()[i]` は、粒子 `particles()[i]` の重みです。大きい粒子ほど、平均や行動評価への寄与が大きくなります。`log_weights()[i]` はその重みの自然対数です。対数を使った尤度の書き方は14で学びます。ここでは「同じ重みの、別の数値表現」と考えておけば十分です。

`ESS` は、重みが何個分に広がっているかの目安です。粒子数を $N$、正規化済みの重みを $w_1,\ldots,w_N$ とすると、次の式です。

$$\mathrm{ESS}=\frac{1}{w_1^2+\cdots+w_N^2}$$

全員の重みが同じならESSは $N$。1個だけに全重みが集まると1です。ただし、**値の異なる粒子が何種類あるかを数えた数字ではありません。** 同じ値のコピーが並んでも、等重みならESSは大きくなります。

| 新しいAPI・戻り値 | 読み方 |
| --- | --- |
| `ess()` | 現在の重みのESS。理想的な数値計算では1〜粒子数の範囲 |
| `result.ok` | 今回の観測を取り込めたか |
| `result.log_predictive` | 今回の観測を、更新前の分布からどれほど予測していたかを表す対数値。粒子による近似値 |
| `result.resampled` | 今回成功した更新の準備中・段階更新中に、再サンプリングしたか |
| `result.moves.attempted` | 今回の更新に伴った再探索の提案回数 |
| `result.moves.accepted` | そのうち採用された回数 |
| `particles()` | 粒子の読み取り専用view。`ParticleFilter` では内部の付加情報を隠したviewを返す |
| `weights()` / `log_weights()` | 粒子と同じ順序の、正規化済み重み／対数重みのview |
| `i < 3` | 表示する先頭の3粒子。上位3個という意味ではない |

実行結果：

```text
ESS 783.926 / 1024
log_predictive -0.674701
resampled 0 moves 0/0
0 0.159793 0.000306392 -8.09064
1 0.992145 0.00190237 -6.26466
2 0.039569 7.58707e-05 -9.48648
```

今回の観測は1回の成功なので、`log_predictive` はおよそ $\log(0.5)$ になります。確率の代わりに確率密度を使う問題では、この値が正になる場合もあります。

viewは安価な読み取り口ですが、粒子や配列を変更する操作をまたいで保存しないでください。`observe`、`set_param`、`reset`、代入などの後は取り直します。更新に失敗した場合でも作業領域は使われ得るため、失敗ならviewが必ず有効とは考えません。

## 05. 全粒子平均と、上位k粒子の平均を使い分ける

出発点は02です。観測は同じまま、代表値の取り方を3種類比べます。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    const auto all = filter.estimate();     // top_k=0 と同じ。
    const auto top = filter.estimate(128);  // 上位128個の中で重みを再正規化。
    const auto one = filter.estimate(1);    // 重み最大の粒子1個。
    std::cout << "all " << all.mean << ' ' << all.selected_mass << '\n';
    std::cout << "top " << top.mean << ' ' << top.selected_mass << '\n';
    std::cout << "one " << one.mean << ' ' << one.selected_mass << '\n';
}
```

全粒子平均は、現在保持している分布全体を使います。上位k平均は、**現在の重みが大きいk個だけを選び、その中で重みの和を1にし直して**平均します。代表値を有力な一部へ寄せたいときに使えますが、切り捨てた可能性は結果に反映されません。

| 指定・戻り値 | 意味 |
| --- | --- |
| `estimate()` / `estimate(0)` | 全粒子を使う。0が既定値 |
| `estimate(128)` | 上位128個を使う。128は例であり、必要な集中度に合わせて変更する |
| `estimate(1)` | 重み最大の粒子1個の値を返す。同重みなら小さい添字が優先される |
| `top_k` の型・範囲 | `int`、0〜現在の粒子数。粒子数と同じ値も全粒子扱い |
| `.mean` | 選択した粒子群の加重平均 |
| `.selected_mass` | 選択した粒子が、選択前の全重みに占めていた割合。全粒子なら1 |

例えば `selected_mass=0.3` なら、元の重みの7割を平均から外しています。これは「推定の正解率が30%」でも、「30%の信頼区間」でもありません。

実行結果：

```text
all 0.694825 1
top 0.751243 0.326962
one 0.749947 0.00261888
```

最初は全粒子平均を使い、上位kを使う理由がある場合に絞ります。再サンプリング直後は重みが等しいため、上位kは単に同点から選ばれた一部になります。また、`estimate(1)` は現在の最大重み粒子であり、「事前の考え方と全観測を反映した確率・密度が最大の仮説」を常に返すAPIではありません。

## 06. 平均以外を求める：2連続成功と、成功率の不確実性

出発点は02です。成功率の平均だけでなく、次の3つを知りたいとします。

- 次に2回続けて成功する確率。
- 成功率の候補が、平均の周りにどれほど散らばっているか。
- 本当の成功率が0.6以上である可能性。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    const double mean = filter.estimate().mean;
    const double second = filter.estimate([](double p) { return p * p; }).mean;
    const double above = filter.estimate([](double p) { return p >= 0.6 ? 1.0 : 0.0; }).mean;
    std::cout << "mean " << mean << '\n';
    std::cout << "two_successes " << second << '\n';
    std::cout << "variance " << std::max(0.0, second - mean * mean) << '\n';
    std::cout << "P(p>=0.6) " << above << '\n';
}
```

### 「各粒子で計算してから平均する」

`estimate` へ関数を渡すと、粒子をいったんその関数の戻り値へ変換し、その値を加重平均します。この変換を「射影」と呼びます。特別な数学操作を追加するというより、**平均したい数を自分で指定する**機能です。

1. 仮説が $p$ なら、同じ成功率のもとで独立な2試行がともに成功する確率は $p^2$。したがって、各粒子の `p*p` を平均します。
2. 成功率の散らばりを表す分散は、「2乗の平均 − 平均の2乗」で求められます。`std::max(0.0, ...)` は、丸め誤差でごく小さな負値になることを避けています。
3. 仮説が0.6以上なら1、それ以外なら0を返すと、条件を満たす粒子の重みを合計できます。

| 新しい記述・値 | 意味 |
| --- | --- |
| `estimate([](double p) { ... })` | 粒子を受け取り、平均したい値を返す関数を渡す。元の粒子は変更しない |
| `p*p` | その仮説のもとでの2連続成功確率 |
| `0.6` | 「成功率がこれ以上か」を調べる境界。問題に応じて変える |
| `1.0` と `0.0` の返し分け | 条件が真のとき1、偽のとき0 |
| 射影付きの `top_k` | 第2引数として指定可能。省略時は0、つまり全粒子 |

実行結果：

```text
mean 0.694825
two_successes 0.501421
variance 0.0186393
P(p>=0.6) 0.757238
```

**平均成功率を2乗することと、各成功率の2乗を平均することは違います。** 例えば候補が0と1に半分ずつなら、平均は0.5、その2乗は0.25ですが、2乗の平均は0.5です。未知の成功率を共通に使う未来の2試行では、後者が必要です。

逆に、成功率そのものを1個の数で当て、2乗誤差を小さくしたい場合の自然な代表値は全粒子平均です。評価したい目的が変われば、各粒子で計算すべき値も変わります。

## 07. 推定を行動選択へつなぐ

出発点は06です。推定値を表示するだけでなく、次の行動を選びます。安全策なら確実に5点。挑戦すると参加費1点を払い、2回とも成功すれば12点を獲得するルールです。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    // 安全策は確実に5点。挑戦は2連続成功で12点、参加費は常に1点。
    std::array<double, 2> reward{};
    const double mass = filter.for_each_weighted([&](double p, double weight) {
        reward[0] += weight * 5.0;
        reward[1] += weight * (12.0 * p * p - 1.0);
    });
    const int selected = reward[1] > reward[0] ? 1 : 0;
    std::cout << reward[0] << ' ' << reward[1] << ' ' << selected << ' ' << mass << '\n';
}
```

仮説 $p$ のもとでの挑戦の平均得点は $12p^2-1$ です。これを粒子の重みで平均すれば、不確かな成功率を考慮した平均得点になります。

`for_each_weighted` は、粒子と重みを渡してユーザー関数を呼びます。`estimate` より自由に、複数候補の得点を一度に足し込めます。最終的にどの行動を選ぶかは、ライブラリではなく利用側で決めます。

| 新しい記述・値 | 意味 |
| --- | --- |
| `reward[0]`, `reward[1]` | 安全策・挑戦の平均得点を蓄積する入れ物。最初は0 |
| `5.0` | 安全策の確定得点 |
| `12.0` | 挑戦で2連続成功したときの獲得点 |
| `1.0` | 挑戦の参加費。成功・失敗に関係なく引く |
| コールバックの `weight` | 現在の粒子の正規化済み重み。足し込みのときに1回だけ掛ける |
| `for_each_weighted` の戻り値 | 選択質量。省略した `top_k` は0なので、ここでは1 |
| `selected` | 利用側で決めた行動番号。0が安全策、1が挑戦。同点なら安全策 |

実行結果：

```text
5 5.01705 1 1
```

このデータでは両行動の期待得点が近く、近似誤差で選択が変わる余地があります。大差がない選択にこそ、粒子数や複数seedで結果を確認する意味があります。

## 08. 重い行動評価には、少数の粒子を抽出して使う

出発点は07です。行動評価が大きなシミュレーションになり、全粒子を毎回試すと重い場合を考えます。推定器自体の粒子数を減らさず、その分布から256個だけ抽出して評価します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    std::mt19937_64 planning_rng(100003); // 行動評価用。推定用RNGとは別。
    std::vector<int> indices(256);       // 抽出する標本の個数。
    filter.sample_indices(indices, planning_rng);
    const auto particles = filter.particles();
    double sampled_reward = 0;
    for (int index : indices) {
        const double p = particles[index];
        sampled_reward += (12.0 * p * p - 1.0) / indices.size();
        // weights()[index] は掛けない。選ばれる頻度に既に反映されている。
    }
    const double exact_reward = filter.estimate([](double p) { return 12.0 * p * p - 1.0; }).mean;
    std::cout << sampled_reward << ' ' << exact_reward << '\n';
}
```

`sample_indices` は重みの大きい粒子ほど選ばれやすい抽出です。同じ粒子が複数回選ばれて構いません。重複も「何回選ばれたか」という情報なので、そのまま数えます。

| 新しい記述・値 | 意味 |
| --- | --- |
| `planning_rng(100003)` | 行動評価用の乱数器とseed。推定内部の乱数を消費しないために分ける |
| `indices(256)` | 抽出先の配列を256要素で作る。256は行動評価の標本数で、粒子数1024とは独立 |
| `sample_indices(indices, planning_rng)` | 選ばれた粒子の添字を配列へ書く。戻り値はない。フィルターの粒子・重みは変わらない |
| `particles[index]` | その添字の仮説を読む |
| `/ indices.size()` | 抽出後は256標本の等重み平均を取る |
| `exact_reward` | ここでは「現在の全粒子に対する平均」。観測後の真の分布に対する厳密値という意味ではない |

実行結果：

```text
5.2844 5.01705
```

元の `weights()[index]` をもう一度掛けると、重みを二重に反映してしまいます。複数の行動を比較する場合は、同じ抽出結果を全候補へ使うと、抽出の違いに起因する比較のばらつきを抑えやすくなります。

## 09. 観測を取り込めない場合を確認する

出発点は01です。装置の表示に0・1以外の記号2が現れました。しかし、今のモデルでは2が出る経路を一切想定していません。その観測の尤度を、全粒子で0にする例です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗、2=このモデルでは起きない表示。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int symbol) const {
        if (symbol == 1) return p;
        if (symbol == 0) return 1.0 - p;
        return 0.0;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    if (!filter.observe(1).ok) return 1;
    const double before = filter.estimate().mean;
    const auto failed = filter.observe(2);
    std::cout << failed.ok << ' ' << filter.observation_count() << '\n';
    std::cout << before << ' ' << filter.estimate().mean << '\n';
    // ここで同じ観測を勝手に捨てて続けず、表示の読み方とモデルを確認する。
}
```

全候補で観測の確率が0なら、重みの和も0になり、正規化できません。`observe` は `ok=false` を返します。このとき、確定済みの粒子・重み・観測履歴・成功観測数は維持されます。

| 新しい記述・値 | 意味 |
| --- | --- |
| 観測記号 `2` | 現在のモデルで説明できない表示。正常な0/1モデルの都合のよい拡張ではない |
| `return 0.0` | その仮説のもとでは今回の観測が起こらない |
| `before` | 失敗前の平均を値として保存。viewの保存ではない |
| `failed.ok` | 0が出力されれば、今回の取り込みは失敗した |

実行結果：

```text
0 1
0.665282 0.665282
```

表示の読み取りを間違えたのか、あり得る状態を初期候補に入れていなかったのか、ノイズモデルが厳しすぎるのかを調べます。`ok=false` は「観測が現実でも絶対に起こらない」と判定したわけではなく、**現在用意した粒子とモデルでは取り込めなかった**という意味です。

乱数器やユーザー関数の外部への副作用までは巻き戻りません。何も変わらなかったつもりで、同じ失敗を無条件に繰り返す設計にはしないでください。

## 10. 新しい観測なしで追加計算する

出発点は02です。観測は8件のままですが、行動を選ぶまで少し計算時間が残っています。`observe` の再呼び出しではなく、`refine` で候補を追加探索します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    const int before = filter.observation_count();
    const auto moved = filter.refine(4); // 各粒子に4回の追加再探索。新観測は追加しない。
    std::cout << moved.accepted << '/' << moved.attempted << '\n';
    std::cout << before << ' ' << filter.observation_count() << ' ' << filter.estimate().mean << '\n';
}
```

再探索では、現在の候補から別の仮説を提案し、過去の観測全体に照らして採用するかを決めます。この例のモデルには専用の近傍関数を用意していないため、ライブラリは `sample` を使って事前分布から候補を提案します。確率的な採否判定はライブラリの仕事です。

| 新しい記述・値 | 意味 |
| --- | --- |
| `refine(4)` | 各有効粒子に4回の提案を試す。4は全体の合計提案数ではない |
| `steps` | `int`、0以上。0は何もしない。`refine` 自体の引数に既定値はない |
| `moved.attempted` / `.accepted` | この追加探索で試した／採用した提案回数 |
| 観測数の前後比較 | 追加計算で観測数が増えていないことを確認する |

実行結果：

```text
2127/4096
8 8 0.698884
```

`refine` は粒子の値を変更し、重みと履歴を維持します。回数を増やせば必ず推定誤差が単調に下がるわけではありません。まず数回にし、計算時間と平均的な誤差で調整してください。時間とともに状態が動くモデルには、この高水準の `refine` はありません。32で理由を説明します。

## 11. 自動処理と、途中からの計算量調整

出発点は10です。手動の追加計算を理解したところで、観測時に自動で行われる処理を調整します。ここでは初めて、`Param` の全項目をまとめて扱います。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    auto param = filter.param(); // const参照の中身を、自分の変数へコピー。
    param.move_steps = 4;
    param.move_interval = 4;
    param.ess_ratio = 0.6;
    param.tempering_ess_ratio = 0.5;
    filter.set_param(param);     // この4項目だけなら現在の粒子・重みは変わらない。
    param.count = 512;
    filter.set_param(param);     // 粒子数変更は、この場で再サンプリングする。
    std::cout << filter.size() << ' ' << filter.ess() << ' ' << filter.observation_count() << '\n';
    if (!filter.observe(0).ok) return 1;
    std::cout << filter.estimate().mean << '\n';
}
```

### どの処理を調整しているのか

重みが少数の粒子に集中すると、候補の大部分がほとんど使われません。そこで、重みに比例して候補を複製・選び直し、等重みに戻すことを「再サンプリング」と呼びます。これだけでは新しい値は生まれないので、固定パラメータでは再探索と組み合わせます。

さらに、非常に絞り込みの強い観測を一度に掛けると、ほとんどの候補が消えます。その場合、今回の尤度を少しずつ反映し、途中に再サンプリングと再探索を挟むのが「段階更新」です。同じ観測を何回も数えるのではなく、例えば0.3乗と0.7乗に分け、合計1乗だけ反映します。

| `Param` の項目 | 型・既定値 | 意味・選び方 |
| --- | --- | --- |
| `count` | `int`、1024 | 粒子数。正の値。途中で変更すると直ちに再サンプリングする |
| `ess_ratio` | `double`、0.5 | 新観測の前に、ESSが `比率×粒子数` 未満なら再サンプリングする。0でこの判定を無効化。0〜1 |
| `move_steps` | `int`、2 | 自動再探索の、各粒子あたりの提案回数。0以上。0で自動再探索と段階更新を無効化する |
| `move_interval` | `int`、8 | 固定パラメータで、成功観測数が正の倍数になった後、次の観測前に定期再探索する。0で定期分を無効化 |
| `tempering_ess_ratio` | `double`、0.5 | 段階更新で目安にするESS比率。0〜1。0で段階更新を無効化。`move_steps=0` のときも無効 |

このコードでは再探索を各4回、定期処理を4観測間隔、観測前のESS閾値を0.6にしています。段階更新の0.5は既定値のまま明示しています。これらは動作を学ぶ例であり、最適設定を主張する値ではありません。最初は既定値を使い、偏りが問題なら比率、時間が足りなければ粒子数や探索回数を調整します。

`param()` は設定への読み取り専用参照を返します。`auto param = filter.param()` でコピーし、変更して `set_param(param)` へ渡します。設定全体を渡すので、コピーから変更すれば他の項目を誤って初期値へ戻しません。

実行結果：

```text
512 512 8
0.635086
```

回数・頻度だけの変更は、現在の粒子・重み・乱数器を変更しません。一方、粒子数512への変更は、その場で等重みの512粒子に表し直すため、平均もわずかに変わり得ます。観測数と履歴は維持されます。

`ess_ratio=0` だけで全ての再サンプリングが止まるわけではありません。定期再探索や段階更新の経路は独立です。また、最終の観測更新直後に必ず等重みへ戻す実装ではなく、重みを残すので、05の上位kを読み取れます。

## 12. 仮の未来へ分岐する・最初からやり直す

出発点は02です。「もし次が成功だったら」を試したい場合、実際の推定器を変更せずコピーを作ります。続いて、コピーの戻し方と再初期化の違いを確認します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array trials{1, 1, 0, 1, 1, 0, 1, 1};
    for (int success : trials) {
        if (!filter.observe(success).ok) return 1;
    }

    auto branch = filter;                    // 粒子・履歴・モデル・RNGをコピー。
    if (!branch.observe(1).ok) return 1;     // 仮に次が成功した分岐。
    std::cout << filter.observation_count() << ' ' << branch.observation_count() << '\n';
    std::cout << filter.estimate().mean << ' ' << branch.estimate().mean << '\n';
    branch = filter;                         // 代入先の作業容量を再利用して戻す。
    branch.reset();                          // 事前から再生成。RNGは巻き戻らない。
    std::cout << branch.observation_count() << ' ' << branch.estimate().mean << '\n';
    ParticleFilter<Model> fresh(Model{});     // 初期seedも同じ状態で最初からやり直す場合。
    std::cout << fresh.observation_count() << ' ' << fresh.estimate().mean << '\n';
}
```

| 新しい操作 | 何が起きるか |
| --- | --- |
| `auto branch = filter` | モデル、粒子、重み、履歴、成功観測数、乱数器を複製する |
| `branch.observe(1)` | 分岐側だけに、仮の成功を1件追加する |
| `branch = filter` | 既存の分岐へ状態をコピーし直す。代入先の作業領域の確保済み容量は再利用される |
| `branch.reset()` | 事前分布から粒子を再生成し、履歴・成功観測数を空にする。乱数器は今の位置から続く |
| `ParticleFilter<Model> fresh(Model{})` | 既定seed 0を含め、新しいオブジェクトを最初から構築する |

実行結果：

```text
8 9
0.694825 0.73517
0 0.497859
0 0.509309
```

コピーしたモデルが外部のポインターを持っている場合、その参照先まで自動的に独立するわけではありません。この例は外部参照を持たないので、分岐が独立します。モデルや乱数器がコピーできない型なら、対応するコピー操作は使えません。

また、固定パラメータで観測式・事前範囲を変更し、過去の観測も新しい規則で再解釈したい場合、`set_param` では変更できません。新しいモデルで推定器を構築し、必要な履歴を再投入します。グラフや入力条件をどこに置くかは16・19で具体化します。

## 13. 成否から測定値へ：一定の温度を推定する

ここからは01の最小例に戻り、観測を0/1から実数へ変えます。一定の温度を3回測ると、4.1、3.9、4.2でした。測定には誤差があるので、真の温度と表示値は少し違います。未知なのは真の温度1個だけです。

真の温度を $\mu$、今回の測定値を $y$ と書きます。誤差は0を中心とする釣り鐘形の「正規分布」に従うと仮定します。誤差の広がりを表す標準偏差を $\sigma$ と呼び、この例では既知の0.5とします。

### 今回返すのは「確率密度」

連続した測定値では、「4.100000…と無限の桁まで完全一致する確率」を使うと0になってしまいます。代わりに、数直線上の起こりやすさの高さである「確率密度」を使います。

ある値の近くの非常に狭い幅に入る確率は、およそ「その場所の密度×幅」です。幅のある区間全体では、釣り鐘の下の面積が確率になります。密度の値そのものは1を超えても構いません。全体の面積が1であることが必要です。

正規分布の密度は次の式です。式を暗記する必要はなく、コードのひな形として使えます。

$$L(\mu,y)=\frac{1}{\sigma\sqrt{2\pi}}\exp\left(-\frac{(y-\mu)^2}{2\sigma^2}\right)$$

ここで $L$ は今回の密度、$\mu$ は粒子が表す温度、$y$ は実測値、$\sigma$ は標準偏差、$\pi$ は円周率です。`exp(x)` は $e^x$ を計算します。実測値と仮説の温度が近いほど、二乗誤差が小さくなり、密度が高くなります。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;    // 一定の温度 mu。
    using Observation = double; // 測定値 y。
    double sigma = 0.5;         // 既知の測定誤差の標準偏差。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 10.0)(rng);
    }
    double likelihood(double mu, double y) const {
        const double z = (y - mu) / sigma;
        return std::exp(-0.5 * z * z) / (sigma * std::sqrt(2.0 * std::numbers::pi));
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});

    for (double y : {4.1, 3.9, 4.2}) {
        if (!filter.observe(y).ok) return 1;
    }
    std::cout << filter.estimate().mean << '\n';
}
```

| 01から変えたもの | 意味・設定方法 |
| --- | --- |
| `Particle = double` | 型は同じだが、中身は成功率ではなく温度 $\mu$ |
| `Observation = double` | 測定値 $y$ を、その単位のまま入れる |
| `sigma = 0.5` | 既知の測定誤差の標準偏差。温度と同じ単位で、必ず正の値。分散ではない |
| `sample` の `0.0, 10.0` | 観測前に温度があると考える範囲。ここではその範囲で一様 |
| `z = (y - mu) / sigma` | 差が「標準偏差何個分」かを表す、単位のない数 |
| `4.1, 3.9, 4.2` | 独立した3回の測定値。`observe` へ各1回ずつ渡す |
| `.mean` | 温度の加重平均。粒子が実数1個なので、結果も実数1個 |

実行結果：

```text
4.09157
```

平均は3測定の平均約4.067の近くになります。`sigma` を小さくすると、「測定値はかなり正確だ」というモデルになり、絞り込みが強くなります。これは単なる高速化の設定ではありません。実際の誤差の大きさに合わせます。粒子数を増やしても、誤差モデルそのものが間違っている問題は解決しません。

## 14. 同じモデルを対数尤度で書く

出発点は13です。問題、温度の範囲、誤差、観測列は変えません。変更は `likelihood` を `log_likelihood` へ置き換えることです。

小さい密度を掛け続けると、コンピューターでは0に丸められることがあります。対数を取ると、掛け算を足し算へ変えられます。例えば $\log(ab)=\log a+\log b$ です。このライブラリで使う対数は、自然対数です。

13の式に対数を取ると、次の形になります。

$$\log L(\mu,y)=-\frac{(y-\mu)^2}{2\sigma^2}-\log\sigma-\frac{1}{2}\log(2\pi)$$

変数の意味は13と同じです。`exp` で非常に小さい数を作らず、右辺を直接計算します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;    // 一定の温度 mu。
    using Observation = double; // 測定値 y。
    double sigma = 0.5;         // 既知の測定誤差の標準偏差。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 10.0)(rng);
    }
    double log_likelihood(double mu, double y) const {
        const double z = (y - mu) / sigma;
        return -0.5 * z * z - std::log(sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});

    for (double y : {4.1, 3.9, 4.2}) {
        if (!filter.observe(y).ok) return 1;
    }
    std::cout << filter.estimate().mean << '\n';
}
```

| 変更した記述 | 意味 |
| --- | --- |
| `log_likelihood(mu, y)` | 今回の観測の密度の自然対数を返す。過去の観測分は含めない |
| `-0.5 * z * z` | 測定値から離れるほど小さくなる項 |
| `-log(sigma)` と残りの定数 | 正規分布の密度を正しく規格化するための項 |

実行結果：

```text
4.09157
```

13と14は同じ確率モデルです。掲載環境では表示結果も一致します。通常版はライブラリが対数へ変換しますが、その前にユーザー関数内で `exp` が0へ丸められた場合は回復できません。大きな二乗誤差、多数の独立測定をまとめた観測などでは、対数版が扱いやすくなります。

**`likelihood` と `log_likelihood` は、モデルにどちらか一方だけ定義します。** 両方を同時に置くとコンパイル時に止まります。起こり得ない観測は、通常版なら `0.0`、対数版なら `-std::numeric_limits<double>::infinity()` です。対数版で `0.0` を返すと「密度または確率が1」という別の意味になります。

## 15. 範囲内の連続値を `UniformBox` に任せる

出発点は14です。温度のモデルはそのままに、範囲内の初期生成と再探索の提案を、ライブラリの補助型へ任せます。

`UniformBox<1>` は、1個の連続パラメータについて下限・上限を指定する型です。`struct Model : UniformBox<1>` はC++の継承で、`UniformBox` が持つ型や関数をこのモデルでも使えるようにします。この例では `Particle`、`sample`、`log_prior`、`propose` が用意されるため、自分で書く中心は観測の尤度です。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<1> {
    using Observation = double;
    double sigma = 0.5;
    Model() : UniformBox<1>({0.0}, {10.0}, 0.2) {}
    double log_likelihood(const Particle& p, double y) const {
        const double z = (y - p[0]) / sigma;
        return -0.5 * z * z - std::log(sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});

    for (double y : {4.1, 3.9, 4.2}) {
        if (!filter.observe(y).ok) return 1;
    }
    std::cout << filter.estimate().mean[0] << '\n';
}
```

| 新しい記述・値 | 意味・既定値 |
| --- | --- |
| `UniformBox<1>` の `1` | 未知の連続パラメータの個数。粒子数ではない |
| `Particle` | この場合は `std::array<double, 1>`。温度を `p[0]` に入れる |
| コンストラクタ第1引数 `{0.0}` | 各成分の下限。必須 |
| 第2引数 `{10.0}` | 各成分の上限。必須。対応する下限より大きくする |
| 第3引数 `0.2` | `move_scale`。近くの候補を提案する際の相対的な移動幅。既定値0.2、正の値 |
| `p[0]` | 仮説の温度。配列の添字0は最初の成分 |
| `.mean[0]` | 配列として返る加重平均の、温度成分 |

実行結果：

```text
4.0604
```

範囲は「観測前にどんな値があり得るか」、移動幅は「再探索でどれくらい離れた候補を試すか」です。1次元では、通常の近傍移動の標準偏差の目安は `範囲幅×move_scale` です。既定のまま始め、候補がほとんど棄却されるなら小さくする、動きが小さすぎるなら大きくする、という調整ができます。別の反転提案も混ざるため、毎回必ずこの幅で動くわけではありません。

各成分をそれぞれの範囲から一様に生成するので、観測前は成分同士が独立です。観測後も独立だと仮定するわけではありません。次で、2個の成分が関係し合う例を見ます。

## 16. 未知の値を2個に増やす：倍率と固定ずれ

出発点は15です。温度1個の代わりに、計測装置の倍率 $a$ と固定ずれ $b$ を推定します。既知の入力 $x$ を与えると、理想的には $ax+b$ が表示されます。実際の表示 $y$ には測定誤差が加わります。

1回の観測に必要なのは、「どの入力 $x$ を与えたか」「表示 $y$ は何か」「その測定の誤差の標準偏差 $\sigma$ はいくつか」です。これら3個を `Observation` へまとめます。粒子は未知の2個 $[a,b]$ を持ちます。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<2> {
    struct Observation { double x, y, sigma; };
    Model() : UniformBox<2>({0.0, -2.0}, {4.0, 4.0}) {}
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double predicted = p[0] * o.x + p[1]; // p=[倍率a, 固定ずれb]。
        const double z = (o.y - predicted) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array<Model::Observation, 5> observations{{
        {0, 1.1, 0.25}, {1, 2.9, 0.25}, {2, 5.1, 0.25},
        {3, 6.9, 0.25}, {4, 9.0, 0.25}
    }};
    for (const auto& o : observations) {
        if (!filter.observe(o).ok) return 1;
    }
    const auto mean = filter.estimate().mean;
    std::cout << mean[0] << ' ' << mean[1] << '\n';
    const double next_x = 2.5;
    std::cout << filter.estimate([&](const Model::Particle& p) {
        return p[0] * next_x + p[1];
    }).mean << '\n';
}
```

### コードが行っていること

1. `UniformBox<2>` で、倍率を0〜4、固定ずれを−2〜4の範囲に置きます。
2. 各粒子の `p[0] * o.x + p[1]` で、その仮説ならどんな表示になるかを予測します。
3. 予測値と実測値の差を、13と同じ正規分布へ入れます。
4. 入力を0、1、2、3、4と変えた5件の観測を順に取り込みます。
5. 最後に、入力が2.5の場合の表示を、各粒子で予測して加重平均します。

| 新しい型・値 | 意味 |
| --- | --- |
| `UniformBox<2>` | 粒子は `std::array<double, 2>` |
| 下限 `{0.0, -2.0}` | 順に倍率 $a$ と固定ずれ $b$ の下限 |
| 上限 `{4.0, 4.0}` | 同じ順番の上限。`move_scale` は省略し、既定値0.2 |
| `Observation { x, y, sigma }` | 既知の入力、実測値、既知の標準偏差の順。`sigma` は正の値 |
| 観測の `0.25` | 各測定の標準偏差。この例では全部同じだが、件ごとに変えてよい |
| `mean[0]`、`mean[1]` | 推定した倍率の平均、固定ずれの平均 |
| `next_x = 2.5` | 次に調べたい入力。新しい観測ではないので、`observe` には入れない |
| 射影ラムダの `[&]` | 外側の `next_x` を参照して、予測に使う |

実行結果：

```text
1.98479 1.02552
5.98749
```

入力がいつも同じだと、倍率と固定ずれを別々には決めにくくなります。例えば $x=1$ だけでは、$a+b$ が近い組み合わせを区別できません。ライブラリは、観測が区別してくれない値まで確定できません。これは計算量ではなく、観測内容の問題です。

ターンごとに入力条件が変わるとき、その時点の条件を観測に保存するのが基本です。固定パラメータの再探索では過去の尤度を評価し直します。`likelihood` が外部の「現在の入力」だけを読むと、過去の測定が別の入力で行われたことになってしまいます。

## 17. 誤差の大きさも未知にする

出発点は16です。今度は倍率と固定ずれに加えて、測定誤差の標準偏差も分からないとします。粒子を $[a,b,\sigma]$ の3成分に増やし、観測から既知の `sigma` を取り除きます。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<3> {
    struct Observation { double x, y; };
    Model() : UniformBox<3>({0.0, -2.0, 0.05}, {4.0, 4.0, 1.0}) {}
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double predicted = p[0] * o.x + p[1]; // p=[倍率a, 固定ずれb, 標準偏差sigma]。
        const double z = (o.y - predicted) / p[2];
        return -0.5 * z * z - std::log(p[2]) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{}, {.count = 2048});
    const std::array<double, 8> noise{-0.3, 0.1, 0.2, -0.1, 0.3, -0.2, 0.1, -0.1};
    for (int i = 0; i < 24; ++i) {
        const double x = i % 5;
        const double y = 2.0 * x + 1.0 + noise[i % noise.size()]; // この例の入力を作る。
        if (!filter.observe({x, y}).ok) return 1; // 真のa,b,sigmaはfilterへ渡さない。
    }
    const auto mean = filter.estimate().mean;
    std::cout << mean[0] << ' ' << mean[1] << ' ' << mean[2] << '\n';
}
```

| 16からの変更 | 意味・設定方法 |
| --- | --- |
| `UniformBox<3>` | 未知数を3個にする。成分順は倍率、固定ずれ、標準偏差 |
| 3成分目の下限 `0.05` | 標準偏差を0より大きい範囲に制限する |
| 3成分目の上限 `1.0` | この問題であり得る誤差幅の上限 |
| `Observation { x, y }` | 今回は標準偏差が未知なので、観測に正解を入れない |
| `p[2]` | 今の粒子が仮定する標準偏差 |
| `{.count = 2048}` | C++20の指示付き初期化で粒子数だけを指定。他の設定は既定値 |
| `noise` と24回のループ | 動作例の入力を作るための固定データ。モデルには各回の `x,y` だけを渡す |
| 出力の3番目 | 推定した標準偏差の平均。分散が欲しければ $\sigma^2$ を射影して平均する |

実行結果：

```text
2.0177 0.989402 0.199762
```

ここで特に大切なのは、対数尤度の `-std::log(p[2])` を省略しないことです。二乗誤差の項だけなら、標準偏差を大きくするほど誤差の罰が小さくなり、際限なく広い分布を好む方向へ偏ります。密度は広がるほど高さが下がるので、その分も含めて評価します。

`-log(sigma)` は16では全粒子に共通でしたが、17では粒子ごとに異なります。式から項を省略するときは、「本当に全候補で共通か」を確認してください。このガイドの通常尤度・対数尤度は、観測の予測値も正しく解釈できるよう、規格化の項を含めています。

## 18. 実行時に成分数を決める：地域ごとの移動コスト

出発点は16です。倍率と固定ずれという固定の2成分から、入力によって個数が変わるパラメータへ広げます。

道路が複数の地域に分かれ、地域ごとに「距離1あたりの所要時間」が違うとします。各地域で走った距離と、合計所要時間だけが観測できます。例では3地域で、未知の単位時間を $c_0,c_1,c_2$ とします。各地域を走る距離が $d_0,d_1,d_2$ なら、予測時間は次の足し算です。

$$\text{予測時間}=c_0d_0+c_1d_1+c_2d_2$$

`c` は粒子が持つ未知の単位時間、`d` は観測に含める既知の距離です。合計時間には、標準偏差0.2の正規分布の誤差が加わるとします。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<> {
    struct Observation { std::vector<double> distance; double time, sigma; };
    Model(std::vector<double> lower, std::vector<double> upper)
        : UniformBox<>(std::move(lower), std::move(upper)) {}
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double predicted = std::inner_product(p.begin(), p.end(), o.distance.begin(), 0.0);
        const double z = (o.time - predicted) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model({0.5, 0.5, 0.5}, {6.0, 6.0, 6.0}), {.count = 2048});

    const std::vector<Model::Observation> observations{
        {{1, 0, 0}, 2.0, 0.2}, {{0, 1, 0}, 3.0, 0.2},
        {{0, 0, 1}, 4.0, 0.2}, {{1, 1, 1}, 9.1, 0.2}
    };

    for (const auto& o : observations) {
        if (!filter.observe(o).ok) return 1;
    }
    for (double cost : filter.estimate().mean) std::cout << cost << ' ';
    std::cout << '\n';
}
```

| 新しい記述・値 | 意味 |
| --- | --- |
| `UniformBox<>` | 次元数をテンプレート引数で固定しない版。粒子は `std::vector<double>` |
| `lower`、`upper` | 同じ長さの `std::vector<double>`。長さが成分数になる |
| 下限 `{0.5, 0.5, 0.5}`、上限 `{6.0, 6.0, 6.0}` | 各地域の単位時間の範囲。単位は「時間/距離」 |
| `distance` | 地域ごとの距離の配列。粒子と同じ長さ・同じ地域順にする |
| `time` | 実測した合計時間 |
| `sigma` | 合計時間の測定誤差の標準偏差。単位は時間、正の値 |
| `inner_product(..., 0.0)` | 成分ごとの積を足す。最後の `0.0` は加算の初期値 |
| `.count = 2048` | 近似に使う粒子数。地域数とは別 |
| `std::move(lower)` など | 準備した配列の所有権を補助型へ渡すC++の操作 |

実行結果：

```text
2.0249 3.01265 4.0383 
```

最初の3観測は地域を1個ずつ通り、4観測目は全部を通ります。出力は地域ごとの単位時間で、2、3、4付近になります。

次元数は構築時に決めます。走行経路が変わっても、地域の意味と個数が同じなら、次の観測の `distance` を変えるだけです。地域を追加したり、成分番号の意味を入れ替えたりすると、過去の粒子・履歴の意味も変わるので、新しいモデルで再構築します。

## 19. 出力配列と計算済みの推定を再利用する

出発点は18です。毎ターン、地域ごとの平均と、2本の候補経路の所要時間を求めます。推定モデルは変更せず、集計先を自分で用意する `estimate_into` を使います。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<> {
    struct Observation { std::vector<double> distance; double time, sigma; };
    Model(std::vector<double> lower, std::vector<double> upper)
        : UniformBox<>(std::move(lower), std::move(upper)) {}
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double predicted = std::inner_product(p.begin(), p.end(), o.distance.begin(), 0.0);
        const double z = (o.time - predicted) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model({0.5, 0.5, 0.5}, {6.0, 6.0, 6.0}), {.count = 2048});
    std::vector<double> mean(3); // reserve(3)だけではなく、要素数も3にする。
    std::array<double, 2> route_times{};

    const std::vector<Model::Observation> observations{
        {{1, 0, 0}, 2.0, 0.2}, {{0, 1, 0}, 3.0, 0.2},
        {{0, 0, 1}, 4.0, 0.2}, {{1, 1, 1}, 9.1, 0.2}
    };

    for (const auto& o : observations) {
        if (!filter.observe(o).ok) return 1;
        filter.estimate_into(mean); // 配列を再利用。以前の値は上書きされる。
        // このターンの候補経路。長さや候補集合の変更は推定分布の変更ではない。
        const std::array<std::array<double, 3>, 2> routes{{{1, 2, 0}, {0, 0, 2}}};
        const double mass = filter.estimate_into(route_times, [&](const Model::Particle& p) {
            std::array<double, 2> values{};
            for (int r = 0; r < 2; ++r)
                values[r] = std::inner_product(p.begin(), p.end(), routes[r].begin(), 0.0);
            return values;
        });
        std::cout << mean[0] << ' ' << route_times[0] << ' ' << route_times[1] << ' ' << mass << '\n';
    }
}
```

### 今回追加した2つの使い方

`estimate_into(mean)` は、18の `estimate().mean` と同じ量を、既存の配列 `mean` へ書き込みます。以前の値は上書きされるので、毎回0へ戻す必要はありません。

`estimate_into(route_times, project)` は、各粒子を2本の予測時間へ変換し、その平均を `route_times` に書き込みます。ここでは経路0が「地域0を1、地域1を2」、経路1が「地域2を2」の距離だけ通ります。

| 新しい引数・値 | 意味と条件 |
| --- | --- |
| `std::vector<double> mean(3)` | 3成分の出力先。`reserve(3)` だけでは要素がないので不可 |
| `std::array<double, 2> route_times{}` | 2本の経路の平均時間を書き込む出力先 |
| `routes` | このターンで評価したい候補経路。各行が地域ごとの距離 |
| 第2引数のラムダ | 粒子から、出力先と同じ2成分の予測値を返す |
| 戻り値 `mass` | 選んだ粒子が持っていた重みの合計。今回は全粒子なので1 |
| 省略した `top_k` | 既定値0で全粒子。第2引数に射影がない版では第2引数、ある版では第3引数に指定する |

実行結果：

```text
2.00067 8.64734 6.52678 1
1.99261 7.99842 6.79496 1
1.98936 7.97903 7.9953 1
2.0249 8.0502 8.07659 1
```

出力先は、正しい要素数の、連続した `double` の配列やvector、書き込み可能なspanにします。粒子の内部配列を出力先に使ってはいけません。`estimate_into` は数値列用です。実数1個の集計には通常の `estimate` を使うか、射影を1成分の配列にしてください。

候補経路だけを変更する場合は、粒子を作り直す必要も、過去の観測を入れ直す必要もありません。未知の地域コストに対する同じ推定を、新しい候補の評価に使い回せます。ただし、実際の地域コスト自体が時間とともに動く問題は、32の状態追跡としてモデル化します。

## 20. 履歴の一括評価で再探索を軽くする

出発点は15の一定温度です。観測が増えると、再探索で候補を評価するたびに、過去の測定を全部読むのが重くなります。正規分布の二乗誤差は、先に和を計算しておくと短い式へまとめられます。

測定値が $y_1,\ldots,y_n$ のとき、件数を $n$、合計を $S=y_1+\cdots+y_n$、二乗の合計を $Q=y_1^2+\cdots+y_n^2$ とします。候補温度 $\mu$ に対する二乗誤差の合計は、二乗を展開すると次の形です。

$$\sum_{j=1}^{n}(y_j-\mu)^2=Q-2\mu S+n\mu^2$$

右辺なら、測定を1件ずつ読み直さず、$n,S,Q$ と候補 $\mu$ だけで評価できます。これを `log_likelihood_history` に実装します。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<1> {
    struct Observation { double y, sum, squares; int count; };
    double sigma = 0.5;
    Model() : UniformBox<1>({0.0}, {10.0}) {}
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double z = (o.y - p[0]) / sigma;
        return -0.5 * z * z - std::log(sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
    double log_likelihood_history(const Particle& p, std::span<const Observation> history) const {
        if (history.empty()) return 0.0;
        const auto& last = history.back();
        const double sse = last.squares - 2 * p[0] * last.sum + last.count * p[0] * p[0];
        return -sse / (2 * sigma * sigma)
             - last.count * (std::log(sigma) + 0.5 * std::log(2.0 * std::numbers::pi));
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    double sum = 0, squares = 0;
    int count = 0;
    for (double y : {4.1, 3.9, 4.2}) {
        const Model::Observation o{y, sum + y, squares + y * y, count + 1};
        if (!filter.observe(o).ok) return 1;
        sum = o.sum; squares = o.squares; count = o.count; // 成功後に確定。
    }
    filter.refine(4);
    std::cout << filter.estimate().mean[0] << ' ' << filter.history().size() << '\n';
}
```

| 新しい型・値・関数 | 意味 |
| --- | --- |
| 観測の `y` | 今回1件の測定値。通常の `log_likelihood` はこちらだけを使う |
| `sum` | 今回までの測定値の合計 $S$ |
| `squares` | 今回までの測定値の二乗の合計 $Q$ |
| `count` | 今回までの測定件数 $n$ |
| `log_likelihood_history(p, history)` | 渡された履歴全体の対数尤度の合計を返す任意の高速化関数 |
| `std::span<const Observation>` | 保存済み観測を複製せず読み取る範囲。`const` なので書き換えない |
| `history.empty()` の場合の `0.0` | 観測が0件なら、確率の積は1、対数の和は0 |
| `history.back()` | 最後の観測。その時点までの累積値を使う |
| `refine(4)` | 10と同じ追加再探索。一括評価関数を使って過去の説明力を評価する |

実行結果：

```text
4.06967 3
```

この関数は、通常の1件分の `log_likelihood` を置き換えるものではありません。新しい観測の取り込みには1件分、再探索で過去の観測全体を調べるときには一括版が使われます。一括版は、1件版を履歴全体に足した値と、共通定数も含めて一致させてください。事前分布の項はライブラリが別に加えるので、ここには含めません。

外側の累積値は、`observe` に成功してから確定します。失敗した観測まで累積すると、保存された履歴と合わなくなるためです。

この例は、一定温度・全測定で同じ標準偏差という条件で成立します。また、履歴の保存自体は続くので、メモリーが定数になる機能ではありません。非常に大きな温度に小さな誤差が重なる場合、右辺の大きな数同士の引き算で精度を失うため、既知の基準温度を引いた値で集計するなど、データの単位と原点を整えます。

## 21. 整数へ丸められた測定値を扱う

出発点は15です。温度計の内部では誤差を含む実数を測っていますが、画面には整数へ四捨五入した値だけが出るとします。「4」と表示されたことは、内部の測定値が3.5以上4.5未満だったことを意味します。

ここで正規分布の「4における高さ」を返すと、表示規則を取り違えます。返すべきものは、区間3.5〜4.5へ入る確率です。13で説明した、釣り鐘の下の面積を使います。

標準正規分布の値が $z$ 以下となる確率を $F(z)$ とすると、表示値 $d$ の確率は次の差です。

$$L(\mu,d)=F\left(\frac{d+0.5-\mu}{\sigma}\right)-F\left(\frac{d-0.5-\mu}{\sigma}\right)$$

$\mu$ は候補温度、$d$ は表示された整数、$\sigma$ は内部測定の標準偏差です。大きい区間の確率から小さい区間の確率を引くと、間の区間だけが残ります。$F$ を自分で積分して実装する必要はなく、標準関数 `erfc` から計算できます。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<1> {
    using Observation = int; // 四捨五入された温度表示。
    double sigma = 0.5;
    Model() : UniformBox<1>({0.0}, {10.0}) {}
    double likelihood(const Particle& p, int displayed) const {
        const auto cdf = [](double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); };
        const double low = (displayed - 0.5 - p[0]) / sigma;
        const double high = (displayed + 0.5 - p[0]) / sigma;
        if (low >= 0.0) // 右側の裾では「ほぼ1同士の引き算」を避ける。
            return 0.5 * (std::erfc(low / std::sqrt(2.0)) - std::erfc(high / std::sqrt(2.0)));
        return cdf(high) - cdf(low); // 表示値に対応する区間全体の確率。
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    for (int displayed : {4, 4, 5, 4}) {
        if (!filter.observe(displayed).ok) return 1;
    }
    std::cout << filter.estimate().mean[0] << '\n';
}
```

| 新しい記述・値 | 意味 |
| --- | --- |
| `Observation = int` | 画面の整数表示をそのまま入れる |
| `displayed - 0.5`、`displayed + 0.5` | その整数へ四捨五入される区間の下端・上端 |
| `low`、`high` | 区間の両端を、平均0・標準偏差1の尺度へ変換した値 |
| `cdf(z)` | $F(z)$ を計算する補助ラムダ。`cdf` は累積確率を意味する名前 |
| `erfc` を使う右側の分岐 | 両端の累積確率がほぼ1になるとき、小さい右側の面積同士で差を取るための計算 |
| `4, 4, 5, 4` | 実際に表示された4件の整数 |

実行結果：

```text
4.25785
```

誤差の広がりに対して丸め幅が十分小さければ、密度で近似することもできます。しかしこの例の幅1は標準偏差0.5に対して小さくないため、区間の確率をそのまま計算しています。切り捨て、0.1単位の表示などでは、表示値に対応する区間の両端を変更します。

この例は通常の規模の値を扱います。極端な裾の区間では、右側の式でも確率が0へ丸められる場合があります。そのような観測を本当に使うモデルでは、区間確率を対数で安定計算する関数が別途必要です。単に `log(計算済みの0)` としても回復しません。

## 22. 「少なくとも5」しか分からない測定を扱う

出発点は21です。今度は、温度計に測定上限があり、内部測定値が5以上なら数値を出さず「5以上」と報告するとします。これは5.0を正確に測ったこととは違います。

観測を、通常の実測値・上限だけが分かる値・下限だけが分かる値の3種類に分けます。「以下」は左側全体の面積、「以上」は右側全体の面積を返します。種類は装置の出力規則に従って決めます。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<1> {
    enum class Kind { Exact, AtMost, AtLeast };
    struct Observation { Kind kind; double value; };
    double sigma = 0.5;
    Model() : UniformBox<1>({0.0}, {10.0}) {}
    double likelihood(const Particle& p, const Observation& o) const {
        const double z = (o.value - p[0]) / sigma;
        if (o.kind == Kind::AtMost) return 0.5 * std::erfc(-z / std::sqrt(2.0));
        if (o.kind == Kind::AtLeast) return 0.5 * std::erfc(z / std::sqrt(2.0));
        return std::exp(-0.5 * z * z) / (sigma * std::sqrt(2.0 * std::numbers::pi));
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array<Model::Observation, 3> observations{{
        {Model::Kind::Exact, 4.8}, {Model::Kind::AtLeast, 5.0}, {Model::Kind::AtLeast, 5.0}
    }};
    for (const auto& o : observations) {
        if (!filter.observe(o).ok) return 1;
    }
    std::cout << filter.estimate().mean[0] << '\n';
}
```

| 新しい記述・値 | 意味 |
| --- | --- |
| `Kind::Exact` | 実数の測定値が報告された。13と同じ密度を使う |
| `Kind::AtMost` | 内部測定値が `value` 以下という情報。左側の確率を使う |
| `Kind::AtLeast` | 内部測定値が `value` 以上という情報。右側の確率を使う |
| `Observation { kind, value }` | 出力の種類と、実測値または閾値の組 |
| `{Exact, 4.8}` | 最初の測定で4.8が報告された |
| `{AtLeast, 5.0}` | 次の測定で「5以上」と報告された。5.0自体が実測値とは限らない |
| `sigma = 0.5` | 内部測定の既知の標準偏差。報告された閾値の不確かさではない |

実行結果：

```text
5.32885
```

このモデルでは「5以上」の観測が、5.0ちょうどの観測よりも高い温度の候補を残します。通常値の密度と、範囲しか分からない報告の確率を、出力の種類に合わせて返す設計です。

観測を取り込む前に、何を報告する装置なのかを固定してください。大きい値だけを選んで収集したデータを、選別していない通常測定と同じ密度で扱うと、別の偏りが生まれます。この例は、決められた閾値で報告が打ち切られる装置のデータを、その報告規則のまま扱います。

## 23. 相手の選択から、好みとランダム行動率を推定する

出発点は16です。未知数は2個のまま、測定値を「相手が選んだ行動」へ変えます。

相手には3個の行動候補があります。各行動には得点と危険度があり、相手は「得点−危険への重み×危険度」で評価すると仮定します。危険への重み $w$ が大きいほど、安全な行動を好みます。

ただし相手は毎回最良の行動を選ぶとは限りません。確率 $\varepsilon$ で3候補から一様ランダムに選び、残りの確率 $1-\varepsilon$ で評価最大の候補を選ぶとします。同点なら、最大の候補同士で一様に選びます。粒子は $[w,\varepsilon]$ です。

### 選ばれた行動の確率を足し合わせる

ある行動が選ばれる道筋は、「ランダムで選ばれる」と「最大評価なので選ばれる」の2つです。例えば最大評価が1個だけなら、その行動の確率は $\varepsilon/3+(1-\varepsilon)$、他の行動は $\varepsilon/3$ になります。全部を足すと1です。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<2> {
    struct Action { double reward, risk; };
    struct Observation { std::array<Action, 3> actions; int chosen; };
    Model() : UniformBox<2>({0.0, 0.05}, {2.0, 0.5}) {} // [危険への重み, ランダム行動率]。
    double likelihood(const Particle& p, const Observation& o) const {
        std::array<double, 3> score{};
        for (int i = 0; i < 3; ++i) score[i] = o.actions[i].reward - p[0] * o.actions[i].risk;
        const double best = *std::max_element(score.begin(), score.end());
        const int ties = int(std::count(score.begin(), score.end(), best));
        return p[1] / 3.0 + (score[o.chosen] == best ? (1.0 - p[1]) / ties : 0.0);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array<Model::Action, 3> actions{{{3, 0}, {5, 2}, {8, 6}}};
    for (int chosen : {1, 1, 0, 1, 2, 1}) {
        if (!filter.observe({actions, chosen}).ok) return 1;
    }
    const auto next = actions; // 次ターンに別の候補へ差し替えてもよい。
    const Model scoring;      // 確率の計算だけに使う。同じlikelihoodを再利用。
    std::array<double, 3> probabilities{};
    for (int a = 0; a < 3; ++a) {
        probabilities[a] = filter.estimate([&](const Model::Particle& p) {
            return scoring.likelihood(p, {next, a});
        }).mean;
    }
    for (double p : probabilities) std::cout << p << ' ';
    std::cout << '\n';
}
```

| 新しい型・値 | 意味と条件 |
| --- | --- |
| `Action { reward, risk }` | 行動の既知の得点と危険度。相手が選ぶ前の情報 |
| `Observation { actions, chosen }` | その時点の3候補と、実際に選ばれた番号0〜2 |
| `p[0]` の範囲0〜2 | 危険への重み $w$。単位は得点と危険度の尺度に合わせる |
| `p[1]` の範囲0.05〜0.5 | ランダム行動率 $\varepsilon$。この例で事前にあり得ると考える範囲 |
| `best` | その粒子の評価式での最大得点 |
| `ties` | 最大得点と同じ評価の候補数。必ず1以上 |
| `score[o.chosen]` | 今回観測された行動の評価。選ばれなかった行動の尤度を返すのではない |
| `next` | 次に選択確率を予測したい候補集合。別の候補へ変更してよい |
| `scoring` | 確率式を再利用するためのモデル。履歴を持つ推定器ではない |

実行結果：

```text
0.124892 0.754942 0.120166 
```

出力は、次に行動0、1、2を選ぶ確率の推定値です。`estimate` で、各粒子における「その行動を選ぶ確率」を平均しています。尤度関数を、既知の行動の説明にも、次の行動の予測にも使えます。

今回の同点判定は、計算した `double` が完全一致した場合です。実際の相手が誤差幅を使って同点を決めるなら、その規則まで合わせてください。また同じ候補ばかり見せても、「この範囲の重みなら同じ行動になる」という区別しかできません。異なる候補集合での観測が、好みを分ける情報になります。

これは一般的な相手行動モデルの小例です。特定の問題へ使う場合は、その問題で実際に採用されている評価式、ランダム化、候補集合、同点処理へ置き換えます。

## 24. 数値ではなく、種類を推定する

出発点は01です。未知なのが成功率ではなく、装置が「正常か故障か」という種類だとします。正常なら赤ランプの点灯確率が0.1、故障なら0.8と分かっています。観測前に故障と考える確率は0.3です。

粒子には種類を表す整数0または1を入れます。ただし、種類番号をそのまま平均した値を「新しい種類」として使うのは適切ではありません。今回は各種類の確率を取り出す射影を、モデルに用意します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = int;     // 0=正常、1=故障。
    using Observation = int; // 1=赤いランプ点灯、0=消灯。
    int sample(std::mt19937_64& rng) const {
        return std::bernoulli_distribution(0.3)(rng) ? 1 : 0;
    }
    double likelihood(int type, int red) const {
        const double probability_red = type == 1 ? 0.8 : 0.1;
        return red ? probability_red : 1.0 - probability_red;
    }
    std::array<double, 2> project(int type) const {
        return {type == 0 ? 1.0 : 0.0, type == 1 ? 1.0 : 0.0};
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    for (int red : {1, 1, 0}) {
        if (!filter.observe(red).ok) return 1;
    }
    const auto probabilities = filter.estimate().mean; // Model.projectが使われる。
    std::cout << probabilities[0] << ' ' << probabilities[1] << '\n';
    std::cout << (probabilities[1] > probabilities[0] ? 1 : 0) << '\n';
}
```

### `project` は、仮説を集計したい量へ変える

正常の粒子は `[1,0]`、故障の粒子は `[0,1]` に変換します。これらを重み付きで平均すると、1成分目が正常の確率、2成分目が故障の確率になります。06で「条件を満たせば1」を平均した考え方を、2種類まとめて使っています。

| 新しい記述・値 | 意味 |
| --- | --- |
| `Particle = int` | 0=正常、1=故障。連続した数値として動かさない |
| `bernoulli_distribution(0.3)` | 確率0.3で故障の粒子を生成する。事前の故障確率 |
| `probability_red` | 今の種類ならランプが赤になる確率。故障なら0.8、正常なら0.1 |
| `Observation = int` | ランプの観測。1=点灯、0=消灯 |
| `project(type)` | 種類を2成分の指示値へ変換する任意の関数 |
| 引数なしの `estimate()` | `Model.project` がある場合、その値を自動で平均する |
| 最後の比較 | 正常と故障の確率を比較し、最も確率の大きい種類番号を返す |

実行結果：

```text
0.148146 0.851854
1
```

このように、「パラメータを平均する」以外の取り出し方も、モデルへまとめられます。呼び出し側で射影を指定した `estimate(project)` は、その呼び出しで指定した射影を使います。モデルの `project` と二重に変換するわけではありません。

## 25. 種類と連続値を、同じ粒子に入れる

出発点は24と16です。装置の符号が反転しているかどうかと、倍率の両方が分からない場合へ広げます。入力 $x$ に対する理想出力は、通常なら $gx$、反転なら $-gx$ です。$g$ は未知の正の倍率です。

未知の種類と数値を1個の構造体にまとめます。ライブラリは、この構造体全体を1個の仮説として保持します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    struct Particle { bool reversed; double gain; };
    struct Observation { double x, y, sigma; };
    Particle sample(std::mt19937_64& rng) const {
        return {std::bernoulli_distribution(0.2)(rng),
                std::uniform_real_distribution<double>(0.5, 2.0)(rng)};
    }
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double predicted = (p.reversed ? -1.0 : 1.0) * p.gain * o.x;
        const double z = (o.y - predicted) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
    std::array<double, 2> project(const Particle& p) const {
        return {p.gain, p.reversed ? 1.0 : 0.0};
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array<Model::Observation, 3> observations{{{1, -1.4, 0.3}, {2, -3.1, 0.3}, {3, -4.5, 0.3}}};
    for (const auto& o : observations) {
        if (!filter.observe(o).ok) return 1;
    }
    const auto mean = filter.estimate().mean;
    std::cout << "gain " << mean[0] << " P(reversed) " << mean[1] << '\n';
}
```

| 追加した型・値 | 意味 |
| --- | --- |
| `Particle { reversed, gain }` | 符号反転の有無と倍率を同時に持つ仮説 |
| `bernoulli_distribution(0.2)` | 観測前の反転確率を0.2とする |
| 一様乱数の `0.5, 2.0` | 観測前の倍率の範囲。反転の有無とは独立に生成する |
| `Observation { x, y, sigma }` | 既知の入力、実測出力、既知の誤差の標準偏差 |
| `project` の1成分目 | 倍率そのもの。平均すると倍率の平均 |
| `project` の2成分目 | 反転なら1、通常なら0。平均すると反転確率 |
| 観測の `0.3` | 出力の標準偏差。倍率の事前幅や粒子の移動幅ではない |

実行結果：

```text
gain 1.50657 P(reversed) 1
```

入力が正で出力が負なので、反転している仮説が強くなり、倍率は1.5付近になります。構造体全体の平均は一般には定義できませんが、集計したい量へ `project` すれば取り出せます。

このモデルは近傍提案を定義していないため、10と同じく `sample` による事前分布からの再提案を使います。まずモデルを少ない関数で動かし、観測で強く絞られて提案が採用されにくくなった場合に、次のような近傍を考えます。

## 26. 自分で近くの候補を作る：整数パラメータ

出発点は13の一定値推定です。ただし未知の距離が整数0、1、2、3、4のどれかだと分かっている場合に変えます。事前の重みは順に1、2、4、2、1とし、中央の2をやや有力に考えます。

再探索で現在値を1だけ増減する `propose` を追加します。これは真の距離が時間とともに動くという意味ではありません。「同じ固定距離について、別の仮説を試す」操作です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = int; // 整数の距離0..4。
    using Observation = double;
    std::array<double, 5> prior_weight{1, 2, 4, 2, 1};
    int sample(std::mt19937_64& rng) const {
        return std::discrete_distribution<int>(prior_weight.begin(), prior_weight.end())(rng);
    }
    double prior(int p) const { return prior_weight[p] / 10.0; }
    double log_likelihood(int p, double y) const {
        const double sigma = 0.75;
        const double z = (y - p) / sigma;
        return -0.5 * z * z - std::log(sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
    void propose(int& p, std::mt19937_64& rng) const {
        const int candidate = p + (std::bernoulli_distribution(0.5)(rng) ? 1 : -1);
        if (0 <= candidate && candidate <= 4) p = candidate; // 範囲外ならその場に留まる。
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    for (double y : {2.2, 2.9, 3.0}) {
        if (!filter.observe(y).ok) return 1;
    }
    const auto moved = filter.refine(8);
    std::array<double, 5> probabilities{};
    filter.for_each_weighted([&](int p, double weight) { probabilities[p] += weight; });
    const int mode = int(std::max_element(probabilities.begin(), probabilities.end()) - probabilities.begin());
    std::cout << "mean " << filter.estimate().mean << " mode " << mode << '\n';
    std::cout << "moves " << moved.accepted << '/' << moved.attempted << '\n';
}
```

### なぜ `sample` だけでなく `prior` も書くのか

`sample` は初期候補を生成する関数なので、「ある指定値の事前確率はいくつか」という質問には直接答えません。近傍提案を使うと、ライブラリは新旧の候補の事前確率と、過去の観測全体の尤度を比較する必要があります。そのため、この場合は `prior` または `log_prior` を1個用意します。

| 新しい記述・値 | 意味と条件 |
| --- | --- |
| `prior_weight{1,2,4,2,1}` | 各整数の事前の相対重み。合計は10 |
| `discrete_distribution` | 重みに比例して整数を選ぶ標準の乱数分布 |
| `prior(p)` | 指定された整数の事前確率。`sample` の生成分布と一致させる |
| `prior_weight[p] / 10.0` | 相対重みを確率へ変える。このモデルへ来る `p` は常に0〜4 |
| `propose(int& p, rng)` | コピーされた候補を書き換える関数。新旧のどちらを採用するかはライブラリが判定する |
| 増減の確率 `0.5` | +1と−1を等確率にする |
| 範囲外なら据え置く処理 | 端を越える提案では元の値に留まる。境界で提案確率を変えないため |
| `refine(8)` | 各粒子に8回の追加提案を試す |
| `mode` | 同じ整数に属する粒子の重みを合計し、最大の整数を選ぶ |

実行結果：

```text
mean 2.58203 mode 3
moves 3459/8192
```

ここでは0→1と1→0の提案確率がどちらも0.5です。このように行き帰りの確率が同じ提案を「対称提案」と呼びます。`void` を返す `propose` は対称提案という約束です。0では必ず1へ進める、と端だけルールを変えると、対称ではなくなることがあります。

提案した候補が観測をよく説明するほど採用されやすくなりますが、説明力が低い候補も一定の確率で採用します。これにより、最良の1点だけでなく、あり得る値の広がりを保とうとします。この採否判定はメトロポリス・ヘイスティングス法、略してMHと呼ばれます。

加重平均は2.7のような非整数になり得ます。距離そのものを1個出す必要があるなら、例のように整数ごとの確率を集約するか、目的の損失に合わせて整数の行動を比較します。最大重みの粒子1個と、同じ種類の重みを合計した最頻値は別です。

## 27. 行き帰りで提案確率が違う場合を補正する

出発点は26です。観測も事前分布も同じで、`propose` だけを変更します。次の候補を0〜4の範囲から、重み1、1、2、3、3で選ぶことにします。これは候補を試す都合であり、本来の事前分布とは違います。

大きい整数を提案しやすくしただけで、推定結果まで大きい整数へ偏ってはいけません。そのため、候補を出しやすくした分を、採否判定で補正します。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = int; // 整数の距離0..4。
    using Observation = double;
    std::array<double, 5> prior_weight{1, 2, 4, 2, 1};
    int sample(std::mt19937_64& rng) const {
        return std::discrete_distribution<int>(prior_weight.begin(), prior_weight.end())(rng);
    }
    double prior(int p) const { return prior_weight[p] / 10.0; }
    double log_likelihood(int p, double y) const {
        const double sigma = 0.75;
        const double z = (y - p) / sigma;
        return -0.5 * z * z - std::log(sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
    double propose(int& p, std::mt19937_64& rng) const {
        const std::array<double, 5> q{1, 1, 2, 3, 3}; // 事前分布とは別の「提案」の重み。
        const int old = p;
        p = std::discrete_distribution<int>(q.begin(), q.end())(rng);
        return std::log(q[old]) - std::log(q[p]); // 逆向き/順向きの提案確率を補正。
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    for (double y : {2.2, 2.9, 3.0}) {
        if (!filter.observe(y).ok) return 1;
    }
    const auto moved = filter.refine(8);
    std::array<double, 5> probabilities{};
    filter.for_each_weighted([&](int p, double weight) { probabilities[p] += weight; });
    const int mode = int(std::max_element(probabilities.begin(), probabilities.end()) - probabilities.begin());
    std::cout << "mean " << filter.estimate().mean << " mode " << mode << '\n';
    std::cout << "moves " << moved.accepted << '/' << moved.attempted << '\n';
}
```

| 26からの変更 | 意味 |
| --- | --- |
| `double propose(...)` | 提案候補を書き換えたうえで、提案確率の補正を対数で返す |
| `q{1,1,2,3,3}` | 候補を提案するための相対重み。`prior_weight` とは別 |
| `old` | 書き換える前の整数 |
| `q[p]` | 今回提案した新しい整数を出す重み |
| `log(q[old]) - log(q[p])` | 逆向きの提案確率の対数−順向きの提案確率の対数 |

実行結果：

```text
mean 2.5835 mode 3
moves 4094/8192
```

一般には、古い候補を $u$、新しい候補を $v$、$u$ から $v$ を提案する確率または密度を $q(v\mid u)$ と書くと、返す補正は次の式です。

$$\log q(u\mid v)-\log q(v\mid u)$$

この例の提案は出発点に依存しないので、`q[old]` と `q[p]` の比だけになります。相対重みの合計は分母と分子で同じため消えます。

ライブラリは、これに新旧候補の「事前確率の対数＋履歴全体の対数尤度」の差を加えて採否を決めます。**`propose` の戻り値へ、尤度や事前分布の差を重ねて入れないでください。** 対称提案なら補正は0なので、26の `void`、または `0.0` を返す方法が使えます。

逆向きの提案確率が0なら、その候補は採用できません。この場合の補正は負の無限大です。どこからどこへ提案できるかが複雑な場合は、補正式を確かめてから使います。単に「良さそうな候補を多く出す」だけで正しい再探索になるわけではありません。

## 28. 大きな粒子の一部だけを変更し、棄却時に戻す

出発点は26です。近傍提案の意味はそのままに、粒子が大きな配列と計算済みの合計を持つ場合へ広げます。

12個の部品に、それぞれ未知の重量があり、全体の重量だけを測れるとします。各部品は0〜10の範囲です。粒子は12個の重量と、その合計を持ちます。今回は合計58、標準偏差1の測定を1回だけ取り込みます。

提案では1個の重量だけを変更します。毎回12個をコピーせず、その場で1個を書き換え、採用されなければ戻す仕組みを使います。これは履歴の差分追加とは別の、**候補の変更を軽くするための差分提案**です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    struct Particle { std::vector<double> value; double sum; };
    struct Observation { double sum, sigma; };
    struct Change { int index; double old_value, old_sum; };
    int dimensions = 12;
    Particle sample(std::mt19937_64& rng) const {
        Particle p{std::vector<double>(dimensions), 0.0};
        for (double& x : p.value) {
            x = std::uniform_real_distribution<double>(0.0, 10.0)(rng);
            p.sum += x;
        }
        return p;
    }
    double log_prior(const Particle& p) const {
        for (double x : p.value) if (x < 0 || x > 10) return -std::numeric_limits<double>::infinity();
        return 0.0; // 範囲内で一様。共通の事前密度定数は比で相殺される。
    }
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double z = (o.sum - p.sum) / o.sigma;
        return -0.5 * z * z - std::log(o.sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
    auto propose_in_place(Particle& p, std::mt19937_64& rng) const {
        const int j = std::uniform_int_distribution<int>(0, dimensions - 1)(rng);
        const Change saved{j, p.value[j], p.sum};
        p.value[j] += std::normal_distribution<double>(0.0, 0.5)(rng);
        p.sum += p.value[j] - saved.old_value;
        return ParticleCloud<Particle>::MoveProposal<Change>{saved, 0.0};
    }
    void undo(Particle& p, const Change& saved) const {
        p.value[saved.index] = saved.old_value;
        p.sum = saved.old_sum; // キャッシュも元の値に完全に戻す。
    }
    const std::vector<double>& project(const Particle& p) const { return p.value; }
};

int main() {
    ParticleFilter<Model> filter(Model{}, {.count = 512});
    if (!filter.observe({58.0, 1.0}).ok) return 1;
    const auto moved = filter.refine(6);
    const double total = filter.estimate([](const Model::Particle& p) { return p.sum; }).mean;
    std::cout << total << ' ' << filter.estimate().mean.size() << '\n';
    std::cout << moved.accepted << '/' << moved.attempted << '\n';
}
```

### 提案から採否までの流れ

1. `propose_in_place` が、変更位置と変更前の値を `Change` に保存します。
2. その位置の重量を動かし、キャッシュの合計も差分で更新します。
3. 保存情報と提案補正を `MoveProposal` に入れて返します。
4. ライブラリが事前分布と履歴全体から採否を判定します。
5. 棄却の場合だけ `undo` が呼ばれ、重量と合計の両方を元へ戻します。

| 新しい型・値・引数 | 意味 |
| --- | --- |
| `Particle.value` | 各部品の重量配列 |
| `Particle.sum` | 同じ仮説の合計重量のキャッシュ。独立した未知数ではない |
| `dimensions = 12` | 部品数。構築後は粒子配列と一致する値を保つ |
| `Observation { sum, sigma }` | 全体の実測重量と、測定誤差の標準偏差 |
| `Change` | 棄却時に戻すための情報。変更位置、元の重量、元の合計 |
| `log_prior` | 範囲外なら負の無限大、範囲内なら共通定数を除いて0。`sample` と同じ一様事前分布 |
| 添字乱数の `0, dimensions - 1` | 12個のどれを変更するか、端を含む範囲で一様に選ぶ |
| 正規乱数の `0.0, 0.5` | 変更量の平均0、標準偏差0.5。観測の標準偏差1とは別の探索設定 |
| `MoveProposal<Change>{saved, 0.0}` | 保存情報と、対称提案の対数補正0を返す |
| `undo(p, saved)` | 棄却された粒子と保存情報を受け取り、変更前へ完全に戻す |
| `.count = 512`、`refine(6)` | 粒子数512、各粒子への追加提案6回 |

実行結果：

```text
58.0838 12
2443/3072
```

`undo` では「元の値を保存して代入する」方法を使っています。変更量を逆向きに足すだけでは、浮動小数点の丸めにより完全には戻らない場合があります。合計などのキャッシュを戻し忘れると、粒子と尤度の評価が一致しなくなります。

`propose_in_place` を定義した場合は、対応する `undo` も必要です。通常の `propose` と両方ある場合は差分版が優先されます。採否をユーザー側で重ねて行う必要はありません。

合計を1回観測しただけでは、12個の重量を個別に特定できません。例えば1個を増やし別の1個を減らせば、同じ合計になります。出力では全体の平均が58付近になることと、粒子に12成分あることを確認しています。個別重量を求めたいなら、部品の一部だけを量るなど、異なる組み合わせの観測が必要です。

## 29. 角度を平均するときは、向きへ変換する

出発点は15と24です。未知なのが方位角の場合、数値の平均をそのまま代表値にできません。179度と−179度は、どちらもほぼ西向きですが、普通に平均すると0度で東向きになってしまいます。

そこで、角度を「横成分 `cos`、縦成分 `sin`」へ変換して平均し、最後に角度へ戻します。数学の三角比を、同じ向きを近い値で表すために使います。

```cpp
#include "particle_filter_v11.hpp"

struct Model : UniformBox<1> {
    using Observation = std::array<double, 2>; // 方位センサーの横成分・縦成分。
    double sigma = 0.1;
    Model() : UniformBox<1>({-std::numbers::pi}, {std::numbers::pi}) {}
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double dx = (o[0] - std::cos(p[0])) / sigma;
        const double dy = (o[1] - std::sin(p[0])) / sigma;
        return -0.5 * (dx * dx + dy * dy) - 2 * std::log(sigma) - std::log(2.0 * std::numbers::pi);
    }
    std::array<double, 2> project(const Particle& p) const {
        return {std::cos(p[0]), std::sin(p[0])};
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    for (double degrees : {179.0, -179.0}) {
        const double angle = degrees * std::numbers::pi / 180.0;
        if (!filter.observe({std::cos(angle), std::sin(angle)}).ok) return 1;
    }
    const auto direction = filter.estimate().mean;
    const double angle = std::atan2(direction[1], direction[0]);
    std::cout << angle * 180.0 / std::numbers::pi << '\n';
    std::cout << std::hypot(direction[0], direction[1]) << '\n';
}
```

| 新しい記述・値 | 意味 |
| --- | --- |
| 粒子の範囲 `[-pi, pi]` | 方位角をラジアンで表した範囲。$\pi$ ラジアンが180度 |
| `Observation = array<double, 2>` | 方位センサーの横成分と縦成分の測定値 |
| `sigma = 0.1` | 各成分の独立した測定誤差の標準偏差。角度そのものの標準偏差ではない |
| `dx`、`dy` | 仮説の横・縦成分と測定値の差を、標準偏差で割ったもの |
| 対数尤度の2成分分の項 | 独立した横・縦の密度を掛け、対数では足し算にしたもの |
| `project` | 角度を `[cos(angle), sin(angle)]` へ変換する |
| `degrees * pi / 180.0` | 例の入力角度をラジアンへ換算する |
| `atan2(y, x)` | 平均した縦成分・横成分から角度を求める。引数順に注意 |
| `hypot(x, y)` | 平均した方向ベクトルの長さを求める |

実行結果：

```text
-179.896
0.997707
```

角度は180度または−180度の近くになります。この2つは同じ向きです。ベクトルの長さが1に近ければ、粒子の向きがよく揃っています。長さが0に近いと、反対向きの仮説が打ち消し合っている可能性があり、代表角度を1個出しても安定しません。この長さ自体は「正しい確率」ではありません。

観測が角度1個で報告される装置なら、その装置の角度誤差モデルを書いてください。このコードは、横・縦の成分へ独立の誤差が乗るセンサーをモデル化しています。

## 30. 合計1という制約を保つ：混合比を推定する

出発点は26です。整数の代わりに、赤・緑・青の材料の混合比を推定します。比率は全て0以上で、3個を足すと1です。材料をランダムに1個調べると、その色だけが分かります。調査によって比率は変わらないとします。

粒子を `[赤の割合, 緑の割合, 青の割合]` にします。赤を観測した確率は赤の割合そのものなので、尤度は単純です。少し考える必要があるのは、合計1を保つ生成と提案です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = std::array<double, 3>; // 赤・緑・青の割合。非負、合計1。
    using Observation = int; // 取り出した材料の色番号0..2。
    Particle sample(std::mt19937_64& rng) const {
        auto unit = [&] { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); };
        double a = unit(), b = unit();
        if (a + b > 1) { a = 1 - a; b = 1 - b; } // 正方形を三角形へ折り返す。
        return {a, b, 1 - a - b};
    }
    double log_prior(const Particle& p) const {
        for (double x : p) if (x < 0 || x > 1) return -std::numeric_limits<double>::infinity();
        return 0.0; // sample/proposeが合計1を保つ。三角形の中で一様。
    }
    double likelihood(const Particle& p, int color) const { return p[color]; }
    void propose(Particle& p, std::mt19937_64& rng) const {
        const int i = std::uniform_int_distribution<int>(0, 2)(rng);
        const int j = (i + std::uniform_int_distribution<int>(1, 2)(rng)) % 3;
        const double delta = std::uniform_real_distribution<double>(-0.1, 0.1)(rng);
        p[i] += delta; p[j] -= delta; // 片方から片方へ移す。合計は変えない。
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    for (int color : {0, 1, 1, 2, 1, 0, 1, 2, 1}) {
        if (!filter.observe(color).ok) return 1;
    }
    const auto mean = filter.estimate().mean;
    std::cout << mean[0] << ' ' << mean[1] << ' ' << mean[2] << '\n';
}
```

### 初期生成と提案を図形で考える

赤の割合を $a$、緑の割合を $b$ とすると、青は $1-a-b$ です。したがって $a\geq0$、$b\geq0$、$a+b\leq1$ の三角形の中だけを考えれば十分です。

`sample` は0〜1の正方形から2個の乱数を引き、三角形の外なら反対側へ折り返します。これで三角形の中を一様に選べます。各割合を別々に0〜1から引いて、最後に合計で割る方法は、同じ事前分布にはなりません。

提案では、ある成分を少し増やし、別の成分を同じ量だけ減らします。合計を変えずに、新しい混合比を試せます。

| 新しい値・引数 | 意味 |
| --- | --- |
| `Particle = array<double, 3>` | 3色の比率。順番を固定する |
| `Observation = int` | 観測した色番号。0=赤、1=緑、2=青 |
| `likelihood(p, color)` | その粒子での、今回の色の出現確率 |
| `i`、`j` | 増やす成分と減らす成分。必ず異なる2成分を選ぶ |
| 変更量の範囲 `[-0.1, 0.1]` | 混合比を移す幅。問題の比率の単位での探索設定 |
| `log_prior` の範囲判定 | 成分が負などになった候補を棄却する。合計1は生成と提案で保つ |
| 9件の色列 | 赤2回、緑5回、青2回の観測 |

実行結果：

```text
0.248827 0.496486 0.254687
```

事前分布も含めた理論上の平均は `[0.25, 0.5, 0.25]` です。観測比率そのものの `[2/9, 5/9, 2/9]` と少し違います。平均を取っても合計1は保たれます。

負になった成分を単独で0へ丸め、残りを都合よく調整するような提案へ変更すると、提案の対称性を失うことがあります。制約を満たさない候補は事前密度0で棄却する、という今の規則では、採否の計算を単純に保てます。

## 31. 地図全体を仮説にする：通れる経路を選ぶ

出発点は24、26、07です。3個の扉A・B・Cが開いているかどうか分からず、通行テストの結果を観測できる問題を考えます。扉の状態は期間中変わりません。テスト結果には5%の誤報があります。

粒子には「Aは開、Bは閉、Cは開」のような、地図全体の仮説を入れます。3個の真偽値を整数の3ビットで表すとコンパクトです。例えば `0b101` はAとCが開いている仮説です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = std::uint32_t; // bit 0,1,2は、それぞれ扉A,B,Cが開いているか。
    struct Observation { std::uint32_t route; bool passed; };
    double open_prior = 0.7, sensor_error = 0.05;
    Particle sample(std::mt19937_64& rng) const {
        Particle p = 0;
        for (int j = 0; j < 3; ++j)
            if (std::bernoulli_distribution(open_prior)(rng)) p |= Particle{1} << j;
        return p;
    }
    double log_prior(Particle p) const {
        const int open = std::popcount(p);
        return open * std::log(open_prior) + (3 - open) * std::log(1 - open_prior);
    }
    double likelihood(Particle p, const Observation& o) const {
        const bool actually_open = (p & o.route) == o.route;
        return actually_open == o.passed ? 1 - sensor_error : sensor_error;
    }
    void propose(Particle& p, std::mt19937_64& rng) const {
        p ^= Particle{1} << std::uniform_int_distribution<int>(0, 2)(rng);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{});
    const std::array<Model::Observation, 3> observations{{{0b001, true}, {0b010, false}, {0b100, true}}};
    for (const auto& o : observations) {
        if (!filter.observe(o).ok) return 1;
    }
    const std::array<std::uint32_t, 2> routes{0b011, 0b100}; // AとBを通る道／Cを通る道。
    const std::array<double, 2> success_cost{4.0, 7.0};
    std::array<double, 2> costs{};
    filter.for_each_weighted([&](std::uint32_t map, double weight) {
        for (int r = 0; r < 2; ++r) {
            const bool open = (map & routes[r]) == routes[r];
            costs[r] += weight * (open ? success_cost[r] : 20.0);
        }
    });
    std::cout << costs[0] << ' ' << costs[1] << ' ' << (costs[1] < costs[0] ? 1 : 0) << '\n';
}
```

### 観測と行動評価を分ける

観測の尤度では、その経路で必要な扉が全部開いているかを調べ、通行テストの報告と合っていれば0.95、違っていれば0.05を返します。

行動評価では、実際の地図の仮説ごとに経路の成否と費用を計算します。この例の5%はテストの誤報率であり、開いた経路を実際に通るときの失敗確率ではありません。行動の費用へ誤報率を重ねて掛けることはしません。

| 新しい型・値・式 | 意味 |
| --- | --- |
| `uint32_t` のbit 0、1、2 | それぞれA、B、Cが開いているか。例では他のビットを使わない |
| `open_prior = 0.7` | 観測前に各扉が開いている確率。初期状態では扉同士を独立と仮定 |
| `sensor_error = 0.05` | テストの報告が真の成否と逆になる確率 |
| `Observation.route` | テストした経路が必要とする扉のビット集合 |
| `Observation.passed` | テストが「通れた」と報告したか |
| `(p & route) == route` | 必要な扉が全て開いているかを調べる |
| `popcount(p)` | 開いている扉の数。事前確率の計算に使う |
| `propose` のビット反転 | ランダムに扉1個の開閉仮説を反転する対称提案 |
| 候補 `0b011` と `0b100` | AとBを通る経路、Cを通る経路 |
| `success_cost{4.0, 7.0}` と `20.0` | 通れた場合の各経路の費用、通れなかった場合の共通費用 |

実行結果：

```text
18.4525 7.25266 1
```

各経路の期待費用と、費用の小さい経路番号を出力します。地図の整数値を平均しても、意味のある地図にはなりません。また、扉が開く確率を個別に求めて掛けるだけでは、観測後に生まれた扉同士の関係を失うことがあります。完全な地図の仮説ごとに経路を評価すれば、その関係を保持できます。

評価したい経路を後から変えるだけなら、同じ粒子を使い回せます。新しい経路のテスト結果が来たら、新しい `route, passed` を1件追加します。扉の開閉状態そのものが変化した場合は、固定地図という前提が崩れるので、次の状態追跡の考え方が必要になります。

## 32. 時間とともに動く値を追跡する

ここからは13に戻り、一定温度の代わりに移動体の現在位置を推定します。固定パラメータと違い、1ターン後には正しい値そのものが変わります。

1ターンに「指定した移動量だけ進む。ただし移動誤差が加わる。その後、位置センサーで測る」とします。移動誤差の標準偏差は0.3、測定誤差の標準偏差は0.5です。粒子が表すのは、そのターンの現在位置です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double; // 現在位置x。固定のパラメータではない。
    struct Observation { double control, measured; };
    double process_sigma = 0.3, measurement_sigma = 0.5;
    double sample(std::mt19937_64& rng) const {
        return std::normal_distribution<double>(0.0, 1.0)(rng); // 最初の移動より前の位置。
    }
    void transition(double& x, const Observation& o, std::mt19937_64& rng) const {
        x += o.control + std::normal_distribution<double>(0.0, process_sigma)(rng);
    }
    double log_likelihood(double x, const Observation& o) const {
        const double z = (o.measured - x) / measurement_sigma;
        return -0.5 * z * z - std::log(measurement_sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{}, {.count = 4096});
    const std::array<Model::Observation, 4> turns{{{1.0, 1.1}, {1.0, 1.8}, {0.5, 2.5}, {-0.5, 2.1}}};
    for (const auto& turn : turns) {
        if (!filter.observe(turn).ok) return 1; // 内部で遷移してから測定を反映。
        std::cout << filter.observation_count() << ' ' << filter.estimate().mean << '\n';
    }
}
```

### 1回の `observe` の中で起こること

このモデルには `transition` があるため、ライブラリは動的な状態モデルとして扱います。`observe(turn)` は、各仮説を前の位置から次の位置へ移してから、その移動後の位置で測定値の尤度を計算します。重みの偏りが大きければ、その前に自動で再サンプリングします。

| 新しい記述・値 | 意味・順序 |
| --- | --- |
| `Particle = double` | 現在位置。固定の隠しパラメータではない |
| `sample` の平均0、標準偏差1 | 最初のターンの移動より前の位置についての事前分布 |
| `Observation.control` | 今回与えた移動量。正なら右、負なら左という単位系 |
| `Observation.measured` | 今回の移動後に得られた位置の測定値 |
| `process_sigma = 0.3` | 真の移動に伴う揺らぎの標準偏差。位置の単位 |
| `measurement_sigma = 0.5` | センサーの測定誤差の標準偏差。位置の単位 |
| `transition(x, o, rng)` | 古い位置 `x` を、移動量とランダムな移動誤差で書き換える |
| `log_likelihood(x, o)` | 移動後の候補位置 `x` から、今回の測定値の密度を求める |
| `.count = 4096` | 動的な分布を表す粒子数。まずこの小例の設定として使う |
| 各行の出力 | 成功ターン数と、そのターンの現在位置の平均 |

実行結果：

```text
1 1.07811
2 1.92455
3 2.4597
4 2.02597
```

`observe` の前に自分で同じ移動を適用すると、移動を二重に数えてしまいます。`transition` の中に書くのは、観測へ都合よく寄せる操作ではなく、真の移動モデルからの生成です。観測を使って候補を寄せる方法は34で分けて扱います。

### 固定パラメータの再探索をそのまま使えない理由

固定温度なら、過去の全測定を同じ候補温度で説明できました。しかし移動体の現在位置だけを変えて、過去の全測定もその現在位置で説明するのは誤りです。過去の測定は過去の位置で行われています。

このため、動的モデルの `ParticleFilter` には `history()` と `refine()` はありません。成功観測数は引き続き読めます。`move_steps`、`move_interval`、`tempering_ess_ratio` は動的モデルでは使われず、`count` と `ess_ratio` が計算設定として使われます。

## 33. 移動はするが、測定が欠けたターン

出発点は32です。2ターン目だけ位置センサーが値を返さない状況に変えます。測定がなくても移動体は動くので、時間の進行は取り込む必要があります。

「測定がないこと自体は、位置について何も教えない」と仮定すれば、どの候補にも同じ尤度1を返せばよく、対数尤度では0です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double; // 現在位置x。固定のパラメータではない。
    struct Observation { double control; std::optional<double> measured; };
    double process_sigma = 0.3, measurement_sigma = 0.5;
    double sample(std::mt19937_64& rng) const {
        return std::normal_distribution<double>(0.0, 1.0)(rng); // 最初の移動より前の位置。
    }
    void transition(double& x, const Observation& o, std::mt19937_64& rng) const {
        x += o.control + std::normal_distribution<double>(0.0, process_sigma)(rng);
    }
    double log_likelihood(double x, const Observation& o) const {
        if (!o.measured) return 0.0; // 対数尤度0、つまり尤度1。
        const double z = (*o.measured - x) / measurement_sigma;
        return -0.5 * z * z - std::log(measurement_sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{}, {.count = 4096});
    const std::array<Model::Observation, 4> turns{{{1.0, 1.1}, {1.0, std::nullopt}, {0.5, 2.5}, {-0.5, 2.1}}};
    for (const auto& turn : turns) {
        if (!filter.observe(turn).ok) return 1; // 内部で遷移してから測定を反映。
        std::cout << filter.observation_count() << ' ' << filter.estimate().mean << '\n';
    }
}
```

| 32からの変更 | 意味 |
| --- | --- |
| `std::optional<double> measured` | 測定値がある場合と、ない場合を表す型 |
| `std::nullopt` | このターンには測定値がないことを指定する |
| `if (!o.measured) return 0.0` | 測定がないので、重みに観測情報を追加しない。対数尤度0=尤度1 |
| `*o.measured` | 値があることを確かめた後、実測値を取り出す |

実行結果：

```text
1 1.07811
2 2.07454
3 2.53319
4 2.06755
```

2ターン目も `observe` は成功し、成功観測数は増えます。`transition` は実行されるので位置は移動し、移動誤差の分だけ不確かさも増えます。欠測だからと `observe` 自体を呼ばないと、そのターンの移動を飛ばすことになります。

これは「測定がないことが位置と無関係」というモデルです。遠くへ行くほどセンサーが反応しなくなるなら、無反応も位置の手掛かりです。その場合は、無反応になる確率を尤度として返します。

## 34. 観測に合わせて次状態を提案する

出発点は32です。センサーの標準偏差を0.1へ小さくすると、観測の許す範囲が狭くなります。移動モデルだけで候補を作ると、観測から遠い候補が多くなり、重みが偏りやすくなります。

そこで、今回の観測も参考にして候補位置を生成します。ただし、そのまま尤度を掛けるだけでは、同じ観測へ二重に寄せた分布になり得ます。真の移動モデルと、実際に候補を作った提案分布の違いを補正します。

### この小例で提案位置を作る計算

ある古い粒子の位置を $x_{\mathrm{old}}$、移動量を $u$、測定値を $y$ とします。移動後の中心予測は $m_0=x_{\mathrm{old}}+u$ です。移動誤差の分散を $q=0.3^2$、測定誤差の分散を $r=0.1^2$ とします。分散は標準偏差の二乗です。

この粒子からの提案分布は、正規分布の式を整理すると、次の平均 $m$ と標準偏差 $s$ になります。

$$g=\frac{q}{q+r},\qquad m=m_0+g(y-m_0),\qquad s=\sqrt{\frac{qr}{q+r}}$$

$g$ は0〜1の混ぜる割合です。移動の方が曖昧で測定が正確なら、$g$ が大きくなり、中心を測定値へ近づけます。これは古い粒子1個を固定したときの計算であり、全粒子の分布が1個の正規分布だと仮定しているわけではありません。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;
    struct Observation { double control, measured; };
    double process_sigma = 0.3, measurement_sigma = 0.1;
    double sample(std::mt19937_64& rng) const {
        return std::normal_distribution<double>(0.0, 1.0)(rng);
    }
    static double normal_log(double value, double mean, double sigma) {
        const double z = (value - mean) / sigma;
        return -0.5 * z * z - std::log(sigma) - 0.5 * std::log(2.0 * std::numbers::pi);
    }
    double log_likelihood(double x, const Observation& o) const {
        return normal_log(o.measured, x, measurement_sigma);
    }
    double propose_transition(double& next, const double& old,
                              const Observation& o, std::mt19937_64& rng) const {
        const double prior_mean = old + o.control;
        const double qvar = process_sigma * process_sigma;
        const double rvar = measurement_sigma * measurement_sigma;
        const double gain = qvar / (qvar + rvar);
        const double mean = prior_mean + gain * (o.measured - prior_mean);
        const double sd = std::sqrt(qvar * rvar / (qvar + rvar));
        next = std::normal_distribution<double>(mean, sd)(rng);
        // 今回の観測尤度は含めない。ライブラリがlog_likelihoodを別に足す。
        return normal_log(next, prior_mean, process_sigma) - normal_log(next, mean, sd);
    }
};

int main() {
    ParticleFilter<Model> filter(Model{}, {.count = 4096});
    const std::array<Model::Observation, 4> turns{{{1.0, 1.1}, {1.0, 1.8}, {0.5, 2.5}, {-0.5, 2.1}}};
    for (const auto& turn : turns) {
        if (!filter.observe(turn).ok) return 1; // 内部で遷移してから測定を反映。
        std::cout << filter.observation_count() << ' ' << filter.estimate().mean << '\n';
    }
}
```

| 新しい関数・引数・値 | 意味 |
| --- | --- |
| `measurement_sigma = 0.1` | より正確なセンサーへ変更したという問題設定 |
| `normal_log(value, mean, sigma)` | 指定した正規分布の対数密度。14の式を関数へまとめたもの |
| `propose_transition(next, old, o, rng)` | 次状態を提案する高水準API。引数は書き込み先、古い状態、今回の情報、乱数器の順 |
| `next` | 古い粒子のコピーから始まる候補。ここへ新しい位置を書く |
| `old` | 書き換えない古い位置 |
| `qvar`、`rvar` | 移動・測定の分散。それぞれ標準偏差を二乗する |
| `mean`、`sd` | 今回実際にサンプリングする提案分布の平均と標準偏差 |
| 関数の戻り値 | 真の移動密度の対数−提案密度の対数。今回の観測尤度は含めない |

実行結果：

```text
1 1.09824
2 1.82669
3 2.4844
4 2.08832
```

高水準の `propose_transition` は、27のMH提案とは別の操作です。27は同じ時点の固定仮説を再探索し、こちらは時間を1ターン進めます。返す補正も異なります。こちらは「真の移動密度/実際の提案密度」の対数で、逆向き提案との比ではありません。

ライブラリが戻り値に `log_likelihood(next, observation)` を別に足します。ユーザー関数に観測尤度をもう一度含めないでください。`transition` と両方を定義した場合は `propose_transition` が優先され、通常の遷移が追加で実行されることはありません。

この例の式は正規分布同士なので簡単に書けます。別のモデルでは、実際に候補を生成した分布の密度まで計算できる必要があります。式を用意しにくければ、まず32の通常遷移を使います。

## 35. 少数の仮説を全列挙する：`ParticleCloud` の入口

ここまでの `ParticleFilter` は、モデル・観測の保存・自動処理をまとめて管理してくれました。通常はこの高水準APIを使えば十分です。

ここからは、初期粒子や処理の順番を自分で管理したい場合に使う低水準の `ParticleCloud` を扱います。最初は24の正常・故障の問題へ戻ります。仮説が2個だけなら、各種類を必ず1個ずつ用意し、正確な事前重みを付けて全列挙できます。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = int;     // 0=正常、1=故障。
    using Observation = int; // 1=赤いランプ点灯、0=消灯。
    int sample(std::mt19937_64& rng) const {
        return std::bernoulli_distribution(0.3)(rng) ? 1 : 0;
    }
    double likelihood(int type, int red) const {
        const double probability_red = type == 1 ? 0.8 : 0.1;
        return red ? probability_red : 1.0 - probability_red;
    }
    std::array<double, 2> project(int type) const {
        return {type == 0 ? 1.0 : 0.0, type == 1 ? 1.0 : 0.0};
    }
};

int main() {
    ParticleCloud<int> cloud(0.0); // 有限な2種類を全列挙。再サンプリングしない。
    const std::array log_prior{std::log(0.7), std::log(0.3)};
    if (!cloud.assign({0, 1}, log_prior)) return 1; // 各種類を1個ずつ、外部の事前重みで登録。
    const Model model;
    double log_evidence = 0;
    for (int red : {1, 1, 0}) {
        const auto result = cloud.update_log([&](int type) {
            return std::log(model.likelihood(type, red)); // 今回1件分だけ。
        });
        if (!result.ok) return 1;
        log_evidence += result.log_normalizer;
    }
    const auto probabilities = cloud.estimate([&](int type) { return model.project(type); }).mean;
    std::cout << probabilities[0] << ' ' << probabilities[1] << ' ' << log_evidence << '\n';
}
```

### 高水準版から何を自分で決めるようになったか

1. `ParticleCloud<int>` という空の粒子群を作ります。
2. `assign` で正常・故障を1個ずつ登録し、重み0.7・0.3を対数で指定します。
3. 新しい観測ごとに、自分で1件分の対数尤度を渡します。
4. 自動の再サンプリングや再探索は行いません。2仮説を両方維持します。
5. 射影も自分で指定して、各種類の確率を読み取ります。

| 新しいAPI・引数・値 | 意味・既定値 |
| --- | --- |
| `ParticleCloud<int>` | 整数粒子を直接持つ低水準の粒子群。モデル型は渡さない |
| コンストラクタの `0.0` | `ess_ratio`。`resample_if_needed` の閾値。既定値0.5、範囲0〜1。ここでは0で条件付き再サンプリングを無効化 |
| `assign({0,1}, log_prior)` | 初期粒子のvectorと、対応する対数重みのspanを渡す。戻り値は成功したか |
| `log_prior` | 順に `log(0.7)`、`log(0.3)`。確率そのものを渡す引数ではない |
| `update_log(callback)` | 現在の重みに、コールバックが返す増分対数尤度を加える |
| コールバックの `type` | 今回評価する粒子。`red` は外側の現在の観測を参照する |
| `result.ok` | 正規化できたか。全て無効ならfalseで、以前の重みを維持 |
| `result.log_normalizer` | 更新時の規格化定数の対数。今回は今回の観測の予測確率の対数 |
| `log_evidence` | 1件ずつの `log_normalizer` の合計。観測列全体の予測確率の対数 |
| `estimate(project)` | 集計したい数値または数値列を返す関数を、明示的に渡す |

実行結果：

```text
0.14094 0.85906 -3.10778
```

この例では仮説を全列挙し、各仮説の重みを直接更新するため、結果は乱数近似ではなく、浮動小数点精度での有限仮説のベイズ更新になります。「事前の重み×観測の尤度」を和が1になるよう割る、01で手計算した更新のことです。観測を反映した後の仮説の分布を「事後分布」と呼びます。仮説が膨大なら全列挙できないので、粒子として一部を持つ方式が必要です。

`assign` の重みは未正規化でも使えます。例えば `log(7)`、`log(3)` でも同じ比率になります。第2引数を省略すると等重みです。空でない粒子列を渡し、重みを指定する場合は粒子と同じ個数にします。全ての対数重みが負の無限大ならfalseで以前の粒子群を維持します。

このモデルの `sample` は、高水準版と同じモデルを掲載するため残していますが、今回の `ParticleCloud` は自動では呼びません。観測履歴も自動保存されません。

## 36. 初期生成と再サンプリングを手動で行う

出発点は35です。有限の2仮説から、01の未知の成功率へ戻します。連続値なので、`initialize` で乱数から1024粒子を生成します。

6回の成功を反映した後、重みの偏りを表示し、条件付き再サンプリング、コピー先への再サンプリング、粒子数を変える再サンプリングを順に試します。高水準の自動処理で何が行われるかを、明示的な呼び出しで確かめる例です。

```cpp
#include "particle_filter_v11.hpp"

struct Model {
    using Particle = double;  // 成功率についての仮説 p。
    using Observation = int; // 1=成功、0=失敗。
    double sample(std::mt19937_64& rng) const {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }
    double likelihood(double p, int success) const {
        return success ? p : 1.0 - p;
    }
};

int main() {
    std::mt19937_64 rng(0);
    const Model model;
    ParticleCloud<double> cloud(0.5);
    cloud.initialize(1024, [&](std::mt19937_64& r) { return model.sample(r); }, rng);
    for (int t = 0; t < 6; ++t) {
        if (!cloud.update_log([](double p) { return std::log(p); }).ok) return 1;
    }
    std::cout << "before " << cloud.size() << ' ' << cloud.ess() << '\n';
    const bool resampled = cloud.resample_if_needed(rng); // 呼ぶタイミングは自分で決める。
    std::cout << "conditional " << resampled << ' ' << cloud.ess() << '\n';
    ParticleCloud<double> branch;
    const int branch_parents = branch.resample_from(cloud, rng, 256); // 元のcloudは維持。
    const int parents = cloud.resample(rng, 512); // 自身の粒子数も変更できる。
    std::cout << cloud.size() << ' ' << parents << ' ' << branch.size() << ' ' << branch_parents << '\n';
    std::cout << cloud.estimate([](double p) { return p; }).mean << '\n';
}
```

| 新しいAPI・引数・値 | 意味 |
| --- | --- |
| `std::mt19937_64 rng(0)` | 低水準APIへその都度渡す乱数器。低水準粒子群は乱数器を保持しない |
| `initialize(1024, init, rng)` | 粒子数、1粒子を返す初期生成関数、乱数器の順。生成後は等重み |
| 初期生成ラムダの `r` | 呼び出し元から渡された乱数器への参照。ここでは既存の `model.sample` に渡す |
| 6回の `update_log` | 毎回新しい成功が1回あったので、今回分の `log(p)` を加える |
| `resample_if_needed(rng)` | ESSが `0.5×粒子数` 未満なら再サンプリング。実行したかをboolで返す |
| `branch.resample_from(cloud, rng, 256)` | 元の `cloud` を維持して、分岐先へ256粒子を作る |
| `cloud.resample(rng, 512)` | 自分自身を、重みに比例して512粒子へ表し直す |
| `new_count` の省略または0 | 元の粒子数を使う。正の値なら指定個数へ変更 |
| `parents`、`branch_parents` | 選ばれた異なる親インデックスの数。異なるパラメータ値の数ではない |

実行結果：

```text
before 1024 269.756
conditional 1 1024
512 512 256 256
0.874603
```

再サンプリング後は重みが等しくなるので、ESSは粒子数と同じになります。しかし、同じ仮説が複数コピーされていることがあります。ESSが大きく戻ったことだけで、新しい情報や多様な仮説が生まれたとは言えません。

粒子数を増やす操作も、手元の仮説を複製するだけです。固定パラメータで新しい値を探すには38の再探索、動的状態では32のような状態遷移が役割を持ちます。

08の `sample_indices` は、元の粒子・重みを変更せず、行動評価用のインデックスだけを抽出しました。一方、ここでの再サンプリングは、粒子群自体とその重みを変更します。両者を区別して使います。

## 37. 状態変更と重み更新を一括で確定する

出発点は36です。低水準で動的状態を扱うとき、遷移後の観測が取り込めなかった場合の振る舞いを確認します。初期候補は位置0と1の2個、等重みです。これらをそれぞれ1だけ右へ移します。

比較のため、一方は `predict` で直接移動し、その後で重み更新を失敗させます。もう一方は `propose_update_log` で、移動と重み更新を一括で試して失敗させます。

```cpp
#include "particle_filter_v11.hpp"



int main() {
    const double impossible = -std::numeric_limits<double>::infinity();
    std::mt19937_64 rng(0);
    ParticleCloud<double> moved, atomic;
    if (!moved.assign({0.0, 1.0})) return 1; // 重み省略なら等重み。
    atomic = moved;
    moved.predict([](double& x, std::mt19937_64&) { x += 1.0; }, rng);
    const auto failed1 = moved.update_log([&](double) { return impossible; });
    const auto failed2 = atomic.propose_update_log([&](const double&, double& next, std::mt19937_64&) {
        next += 1.0;
        return impossible; // 変更と観測の重みを、一括で確定する経路。
    }, rng);
    std::cout << failed1.ok << ' ' << moved.estimate([](double x) { return x; }).mean << '\n';
    std::cout << failed2.ok << ' ' << atomic.estimate([](double x) { return x; }).mean << '\n';
    const auto ok = atomic.propose_update_log([](const double&, double& next, std::mt19937_64&) {
        next += 1.0;
        const double z = (1.5 - next) / 0.5;
        return -0.5 * z * z - std::log(0.5) - 0.5 * std::log(2.0 * std::numbers::pi);
    }, rng); // 真の決定的遷移なので提案補正は0。上の戻り値は今回の対数尤度。
    std::cout << ok.ok << ' ' << atomic.estimate([](double x) { return x; }).mean << '\n';
}
```

| 新しいAPI・引数 | 意味 |
| --- | --- |
| `assign({0.0, 1.0})` | 重みを省略したので、位置0・1を各0.5の重みで登録 |
| `predict(transition, rng)` | 各粒子をその場で変更する。重みは変更しない |
| `transition(double& x, rng)` | 変更する粒子と乱数器を受け取る。この例の移動は決定的なので乱数を使わない |
| `propose_update_log(proposal, rng)` | 古い粒子のコピーに変更と対数重みの増分を適用し、正規化に成功した場合だけ採用 |
| 提案コールバックの第1引数 | 読み取り専用の古い粒子 |
| 第2引数 `next` | 古い粒子のコピー。次状態を書き込む |
| 第3引数 | 乱数器。確率的な次状態を作る場合に使う |
| コールバックの戻り値 | 今回の対数尤度と、必要なら提案補正を含む、対数重みの増分全体。旧重みは含めない |
| `impossible` | 負の無限大。今回の観測をどの候補も説明できないことを表す |

実行結果：

```text
0 1.5
0 0.5
1 1.5
```

最初の行では更新が失敗しても、直接変更済みの位置の平均は1.5です。2行目の一括更新は失敗したので、古い平均0.5を保っています。最後の行では、1だけ進んだ後に位置1.5を標準偏差0.5で測る更新を成功させ、平均は1.5になります。

高水準の `ParticleFilter.observe` は、この一括更新の考え方で動的状態を扱います。低水準で `predict` と `update_log` を別々に呼ぶ場合は、前の変更まで自動で巻き戻ると思わないでください。

**低水準の引数順は `(old, next, rng)`、34の高水準は `(next, old, observation, rng)` です。戻り値も、低水準は観測尤度を含む増分全体、高水準は移動提案の補正だけです。** 名前が似ていても、この2点は違います。

一括更新に失敗しても、乱数器の消費やコールバックが外部に起こした副作用は戻りません。また、個別の候補が増分負の無限大となった場合、その候補の値は元の値を残し、重みだけ0になります。重み0の粒子の値を、有効な次状態として利用しないでください。

## 38. 履歴を自分で管理してMH再探索する

出発点は36と14です。一定の温度を推定しますが、事前分布を平均0・標準偏差1の正規分布へ変えます。測定値は0.8、1.1、0.9、各測定誤差の標準偏差は1です。

高水準の `refine` が行っていた再探索を、低水準の `rejuvenate` で呼びます。低水準は観測を保存しないので、再探索で比較すべき「事前分布と、全観測を反映した密度」をユーザーが用意します。

### 対象密度に何を入れるか

候補温度を $\mu$、保存した測定値を $y_1,y_2,y_3$ とします。事前分布の対数密度は、共通定数を除けば $-\mu^2/2$ です。各測定の対数密度も、共通定数を除けば $-(y_j-\mu)^2/2$ です。したがって比較したい対象は次の和です。

$$\log T(\mu)=-\frac{\mu^2}{2}-\frac{1}{2}\sum_{j=1}^{3}(y_j-\mu)^2$$

$T$ は再探索で目標とする、事前と全観測を反映した未正規化の密度です。MHでは新旧の密度の比だけを使うため、全候補に共通な定数は省略できます。今回のような定数の省略と、17の粒子ごとに変わる標準偏差の項を省略することは違います。

```cpp
#include "particle_filter_v11.hpp"



int main() {
    std::mt19937_64 rng(0);
    ParticleCloud<double> cloud;
    cloud.initialize(2048, [](std::mt19937_64& r) {
        return std::normal_distribution<double>(0.0, 1.0)(r);
    }, rng);
    const std::array readings{0.8, 1.1, 0.9}; // 各測定の既知の標準偏差は1。
    for (double y : readings) {
        if (!cloud.update_log([&](double mu) {
            return -0.5 * (y - mu) * (y - mu) - 0.5 * std::log(2.0 * std::numbers::pi);
        }).ok) return 1;
    }
    cloud.resample(rng); // 最新の重み付き分布を等重みの粒子で表し直す。
    const auto target = [&](double mu) {
        double value = -0.5 * mu * mu; // 初期分布 N(0,1) の対数密度の可変部分。
        for (double y : readings) value -= 0.5 * (y - mu) * (y - mu);
        return value; // MHの比で消える共通定数は省略できる。
    };

    const auto moved = cloud.rejuvenate(8, target, [](double& candidate, std::mt19937_64& r) {
        candidate += std::normal_distribution<double>(0.0, 0.3)(r);
        return 0.0; // 対称提案。候補を採用するかはライブラリが判定。
    }, rng);
    std::cout << cloud.estimate([](double mu) { return mu; }).mean << '\n';
    std::cout << moved.accepted << '/' << moved.attempted << '\n';
}
```

| 新しいAPI・値 | 意味と条件 |
| --- | --- |
| 初期生成の正規乱数 `(0.0, 1.0)` | 事前分布の平均と標準偏差 |
| `readings` | 低水準の利用側が保存する全測定値 |
| `cloud.resample(rng)` | 最新の重み付き分布を等重みへ表し直す。この例ではここで1回呼ぶ |
| `target(mu)` | 事前分布と、全測定の対数尤度の和。最後の観測だけにはしない |
| `rejuvenate(8, target, propose, rng)` | 各有効粒子あたりの回数、対象対数密度、提案関数、乱数器の順 |
| 提案の `candidate` | 現在の粒子からコピーされた候補。ここを書き換える |
| 提案の標準偏差 `0.3` | 近傍移動の幅。温度の単位。測定誤差の1とは別 |
| 提案の戻り値 `0.0` | 対称提案なので、逆向き/順向きの対数補正は0 |
| `moved.attempted`、`.accepted` | 実際に試した回数と採用された回数 |

実行結果：

```text
0.717293
13380/16384
```

重みが有効な現在の粒子では、`target` が有限値を返す必要があります。提案先が不可能なら負の無限大で構いません。`steps=0` は何もしません。再探索は粒子の値を変更しますが、粒子群の重みは維持します。

低水準のコピー式MHでは、提案関数は必ず対数補正を返します。26の高水準モデルのような `void propose` を、そのままこのオーバーロードに渡すことはできません。

この例の理論上の平均は0.7です。全ての観測を自分で管理する必要がなければ、同じモデルを高水準の `ParticleFilter` に書く方が、履歴の二重計上や入れ忘れを避けやすくなります。

## 39. 低水準でも差分提案とundoを使う

出発点は38です。問題と目標分布は変えず、`rejuvenate` の提案方法だけを28と同じ差分方式にします。この例は実数1個なので速度面の利点は小さく、引数の役割を理解するためのものです。

```cpp
#include "particle_filter_v11.hpp"



int main() {
    std::mt19937_64 rng(0);
    ParticleCloud<double> cloud;
    cloud.initialize(2048, [](std::mt19937_64& r) {
        return std::normal_distribution<double>(0.0, 1.0)(r);
    }, rng);
    const std::array readings{0.8, 1.1, 0.9}; // 各測定の既知の標準偏差は1。
    for (double y : readings) {
        if (!cloud.update_log([&](double mu) {
            return -0.5 * (y - mu) * (y - mu) - 0.5 * std::log(2.0 * std::numbers::pi);
        }).ok) return 1;
    }
    cloud.resample(rng); // 最新の重み付き分布を等重みの粒子で表し直す。
    const auto target = [&](double mu) {
        double value = -0.5 * mu * mu; // 初期分布 N(0,1) の対数密度の可変部分。
        for (double y : readings) value -= 0.5 * (y - mu) * (y - mu);
        return value; // MHの比で消える共通定数は省略できる。
    };

    const auto moved = cloud.rejuvenate(8, target, [](double& current, std::mt19937_64& r) {
        const double old = current;
        current += std::normal_distribution<double>(0.0, 0.3)(r);
        return ParticleCloud<double>::MoveProposal<double>{old, 0.0};
    }, [](double& current, const double& saved) {
        current = saved; // 棄却時に呼ばれるundo。
    }, rng);
    std::cout << cloud.estimate([](double mu) { return mu; }).mean << '\n';
    std::cout << moved.accepted << '/' << moved.attempted << '\n';
}
```

| 38からの変更 | 意味 |
| --- | --- |
| `current` | コピーではなく、粒子群の中の現在値をその場で変更する |
| `old` | 棄却時に戻すため、変更前の値を保存する |
| `MoveProposal<double>{old, 0.0}` | 保存情報の型が `double`。2番目は対数提案補正 |
| 追加したundoラムダ | 変更された粒子と保存情報を受け取り、元の値へ戻す |
| `rejuvenate(8, target, propose, undo, rng)` | undo関数が加わる5引数の版 |

実行結果：

```text
0.717293
13380/16384
```

通常版と差分版で同じ提案・乱数列・復元を使っているので、この例の結果は38と一致します。大きな構造体へ応用するときは、28と同じくキャッシュを含めて全変更を戻してください。採用された場合はundoが呼ばれません。

高水準の28では、対象密度をライブラリが過去の観測から計算しました。低水準の39では、対象密度の関数も利用側が用意する、という違いがあります。

## 40. 目標分布を保つ操作を直接適用する

出発点は38です。最後に、低水準のもう1つの `rejuvenate` を扱います。これは採否判定を行わず、渡した関数で粒子を書き換える版です。どんなランダムな変更でも使えるわけではありません。

「目標とする分布から粒子が来たとき、操作後も同じ分布になる」という性質を持つ操作に限って使います。このような操作を分布を保つカーネルと呼びます。難しく聞こえますが、「目標分布から新しく1個引き直す」なら、この条件を確実に満たします。

### この例では、目標分布を式で求められる

38の式を二乗展開して整理します。測定件数を $n$、測定値の合計を $S$ とすると、$\mu$ に関係する部分は次のようになります。

$$-\frac{1}{2}\left((n+1)\mu^2-2S\mu\right)=-\frac{n+1}{2}\left(\mu-\frac{S}{n+1}\right)^2+\text{共通定数}$$

平方完成の形です。正規分布の式と比べると、平均は $S/(n+1)$、標準偏差は $\sqrt{1/(n+1)}$ です。この例では $n=3$、$S=2.8$ なので、平均0.7・標準偏差0.5です。

```cpp
#include "particle_filter_v11.hpp"



int main() {
    std::mt19937_64 rng(0);
    ParticleCloud<double> cloud;
    cloud.initialize(2048, [](std::mt19937_64& r) {
        return std::normal_distribution<double>(0.0, 1.0)(r);
    }, rng);
    const std::array readings{0.8, 1.1, 0.9}; // 各測定の既知の標準偏差は1。
    for (double y : readings) {
        if (!cloud.update_log([&](double mu) {
            return -0.5 * (y - mu) * (y - mu) - 0.5 * std::log(2.0 * std::numbers::pi);
        }).ok) return 1;
    }
    cloud.resample(rng); // 最新の重み付き分布を等重みの粒子で表し直す。

    // この小例では事後分布を式で求められるので、その分布から直接引き直す。
    const double mean = std::accumulate(readings.begin(), readings.end(), 0.0) / (1.0 + readings.size());
    const double sd = std::sqrt(1.0 / (1.0 + readings.size()));
    cloud.rejuvenate(1, [&](double& mu, std::mt19937_64& r) {
        mu = std::normal_distribution<double>(mean, sd)(r);
    }, rng); // このオーバーロードは自動の受理判定を行わない。
    std::cout << cloud.estimate([](double mu) { return mu; }).mean << ' ' << mean << '\n';
}
```

| 新しいAPI・値 | 意味 |
| --- | --- |
| `readings.size()` | 測定件数 $n$ |
| `accumulate(..., 0.0)` | 測定値の合計 $S$ を計算する |
| 分母の `1.0 + n` | 測定n件分に、標準偏差1の事前分布の寄与を足したもの |
| `mean`、`sd` | この例で解析的に求めた事後分布の平均と標準偏差 |
| `rejuvenate(1, kernel, rng)` | 各有効粒子へ1回、分布を保つ操作を適用する3引数版 |
| `kernel(mu, rng)` | 粒子をその場で変更する関数。この例は目標分布から引き直す |
| 戻り値 | この版は `void`。ライブラリによる採否判定や採用回数の返却はない |

実行結果：

```text
0.699014 0.7
```

先頭の出力が粒子から計算した平均、次が式で求めた0.7です。ここまで簡単に厳密な分布が求まる問題では、粒子フィルターを使わず解析的に解く選択もできます。この例は、分布を保つ操作が何かを、確かめられる形で示すためのものです。

現在の粒子へ適当なノイズを足すだけでは、普通は分布の広がりが変わり、この条件を満たしません。条件を説明できない場合は、38または39のMH版を使い、正しい対象密度と提案補正で採否を判定させてください。

## 学習後のAPI参照表

ここは、必要な機能を学び直すための索引です。先に全項目を覚える必要はありません。表中の番号は、完全な実行例のあるステップを示します。

### 通常利用する `ParticleFilter`

| 目的 | API・型 | 説明したステップ |
| --- | --- | --- |
| 最小構築 | `ParticleFilter<Model>(model)` | 01 |
| 設定・乱数器を指定 | `ParticleFilter<Model, Rng>(model, param, rng)` | 03、11 |
| 設定型と全設定項目 | `Param`、`count`、`ess_ratio`、`move_steps`、`move_interval`、`tempering_ess_ratio` | 03、11 |
| 設定を読む・変更する | `param()`、`set_param(param)` | 11 |
| 新しい観測を1件取り込む | `observe(observation)` → `ObserveResult` | 01、04、09、32〜34 |
| 取り込み結果を読む | `ok`、`log_predictive`、`resampled`、`moves` | 04、09 |
| 状態の規模を読む | `size()`、`ess()`、`observation_count()` | 02〜04 |
| 保存した観測を読む | `history()`。固定パラメータのみ | 02、20 |
| 粒子と重みを読む | `particles()`、`weights()`、`log_weights()` | 04 |
| 全体または上位kの平均 | `estimate(top_k=0)` → `.mean, .selected_mass` | 01、05、24 |
| 集計する量を指定 | `estimate(project, top_k=0)` | 06、16、23 |
| 独自の集計を行う | `for_each_weighted(consume, top_k=0)` → 選択質量 | 07、26、31 |
| 出力配列を再利用 | `estimate_into(buffer, top_k=0)`、`estimate_into(buffer, project, top_k=0)` | 19 |
| 行動評価用に抽出 | `sample_indices(output, rng)` | 08 |
| 同じ観測で追加計算 | `refine(steps)` → `MoveResult`。固定パラメータのみ | 10、28 |
| 分岐・再初期化 | コピー、代入、`reset()` | 12 |

テンプレート引数の `Rng` は省略時 `std::mt19937_64` です。別の乱数器を指定する場合は、モデルのコールバックもその型を受け取るように揃えます。コンストラクタの乱数器の既定値は `Rng(0)` です。ムーブ構築・ムーブ代入も利用できますが、移動元をそのまま推定器として使い続けることは前提にしません。

### モデルが提供する関数

| 条件 | 定義するもの | 説明したステップ |
| --- | --- | --- |
| 全モデルの基本 | `Particle`、`Observation`、`sample(rng)` | 01 |
| 観測の説明力 | `likelihood(p,o)` または `log_likelihood(p,o)` のどちらか1個 | 01、13、14 |
| 平均に使う量をまとめる | 任意の `project(p)` | 24、25、29 |
| 固定パラメータの近傍提案 | `propose(p,rng)`。戻り値は対称ならvoid、または対数提案補正 | 26、27 |
| 近傍提案を使う場合の事前 | `prior(p)` または `log_prior(p)` のどちらか1個 | 26、28、30 |
| コピーを避ける差分提案 | `propose_in_place(p,rng)` と `undo(p,token)` | 28 |
| 履歴評価の高速化 | 任意の `log_likelihood_history(p,history)` | 20 |
| 真の状態遷移 | `transition(p,o,rng)` | 32、33 |
| 観測も使った状態の提案 | `propose_transition(next,old,o,rng)` | 34 |

尤度、事前密度、射影、一括評価は、モデルを `const` として読める関数にします。初期生成・提案・状態遷移は乱数を使ってよく、モデル内部の乱数分布用キャッシュも使えます。ただし、同じ仮説と観測の尤度が呼ぶたびに変わるようにはしません。呼び出し回数や粒子の評価順へ依存するモデルも避けます。

`UniformBox<D>` と `UniformBox<>` は、15・18のように初期生成・一様事前・対称提案をまとめます。数値列の下限・上限と、任意の `move_scale=0.2` を構築時に指定します。カテゴリ、整数、合計1などの制約まで自動で表す型ではありません。

### 直接制御する `ParticleCloud`

| 目的 | API | 説明したステップ |
| --- | --- | --- |
| 空の粒子群を構築 | `ParticleCloud<Particle>(ess_ratio=0.5)` | 35 |
| 外部粒子を登録 | `assign(particles, log_weights={})` → bool | 35、37 |
| 乱数から初期生成 | `initialize(count, init, rng)` | 36 |
| 重みだけ更新 | `update_log(log_increment)` → `UpdateResult` | 35 |
| 状態だけ変更 | `predict(transition, rng)` | 37 |
| 状態と重みを一括更新 | `propose_update_log(proposal, rng)` → `UpdateResult` | 37 |
| 条件付き再サンプリング | `resample_if_needed(rng)` → bool | 36 |
| 自分を再サンプリング | `resample(rng, new_count=0)` → 異なる親インデックス数 | 36 |
| 元を保って再サンプリング | `resample_from(source, rng, new_count=0)` → 異なる親インデックス数 | 36 |
| コピー式MH | `rejuvenate(steps, target, propose, rng)` → `MoveResult` | 38 |
| 差分式MH | `rejuvenate(steps, target, propose, undo, rng)` → `MoveResult` | 39 |
| 分布を保つ直接変更 | `rejuvenate(steps, kernel, rng)` → void | 40 |
| 集計・抽出・読み取り | `estimate(project,top_k)`、`estimate_into(buffer,project,top_k)`、`for_each_weighted`、`sample_indices`、`size`、`ess`、`particles`、`weights`、`log_weights` | 04〜08、19、35〜39 |

低水準の集計・抽出の意味は高水準と同じです。ただし低水準の `estimate` と `estimate_into` では射影関数を省略できません。`top_k` は既定値0です。`UpdateResult` は `ok, log_normalizer`、`MoveResult` は `attempted, accepted`、`MoveProposal<Undo>` は `undo, log_correction` を持ちます。

## 適用するときの確認点

### 観測とモデル

- **粒子から観測の確率を計算する。** 尤度は仮説が正しい確率ではありません。通常の成否、測定密度、丸め区間、打ち切り報告、行動選択など、実際に観測できるものに合わせます。01、13、21〜23を参照してください。
- **1件分だけ返す。** 通常の尤度関数に事前確率や過去の尤度を掛け直しません。独立な測定を1個の観測にまとめる場合だけ、その観測内の密度を積、対数なら和でまとめます。過去の履歴一括評価は20の専用関数です。
- **履歴の入力条件を保存する。** 固定パラメータでは、後から過去の観測を同じ規則で再評価します。観測内のvectorは値として保存されますが、ポインターやspanの参照先は自動では保存されません。16、20、23を参照してください。
- **事前分布と初期生成を揃える。** `prior` を書いただけで、`sample` の生成分布を自動補正する機能ではありません。`sample` でその事前から引き、近傍提案を使う場合は同じ事前の密度を返します。26を参照してください。
- **固定値と動く状態を区別する。** `transition` または `propose_transition` があると動的モデルになります。現在状態と固定パラメータを同じ粒子に持つ場合も、全体は動的モデルで、固定成分だけの自動MH再探索が付くわけではありません。32〜34を参照してください。
- **分からない値が残ることもある。** 同じ予測をする仮説が複数あれば、データだけでは区別できません。粒子数の増加だけでなく、観測条件を変える必要があります。16、23、28を参照してください。

### 数値・型・寿命

- **尤度の許容値。** 通常版は0以上の有限値で、密度なら1超も可です。対数版は有限値または負の無限大です。NaN・正の無限大・負の通常尤度は使えません。`-ffast-math` と `-Ofast` も使わないでください。
- **本当の0と丸められた0。** 対数重みが有限でも、通常重みが小さすぎて0へ丸められる場合があります。低水準の再探索は、有限対数重みの粒子を対象とします。通常重みが0だからと、モデルが不可能とした粒子と同じ扱いにしないでください。
- **集計関数は重み0でも呼ばれることがある。** 射影や行動費用は、渡された全粒子で有効な有限値になるようにします。`0×無限大` は0ではなくNaNになり得ます。
- **出力の型。** 数値の射影は `double`、`array` の射影は同じ長さの `array<double,D>`、vectorやspanなどの数値列は `vector<double>` で平均を返します。数値列はランダムアクセスでき、全粒子で同じ長さにします。構造体は25のように射影してください。
- **`estimate_into` の出力先。** 正しい長さの書き込み可能な連続 `double` 領域を使います。粒子や射影が参照する場所と重ねないでください。19を参照してください。
- **boolの扱い。** 高水準の粒子・観測には `bool` を使えます。`Observation=bool` の `history()` は、`std::vector<bool>` を読むrangeを返し、`span<const bool>` にはなりません。どちらの観測型にも対応する一括評価を書くなら、履歴引数を `const auto&` とします。低水準の `ParticleCloud<bool>` の直接操作は避け、boolを含む構造体などで包みます。
- **viewや参照は保持しすぎない。** `particles()`、重み、履歴などは読み取り用で、次の変更操作をまたいで保持しません。粒子自身は値としてコピー・代入できる型にします。外部参照の所有権と寿命は自分で管理します。
- **単一スレッドで使う。** 読み取りAPIでも内部の作業用キャッシュを使います。同じオブジェクトへの並行呼び出しや、コールバック中に同じフィルターへ再入する操作はしません。
- **通常のAHC規模を前提にする。** 粒子数・総試行回数は `int` の範囲、有限対数値の加減算は `double` の範囲に収めます。誤差の標準偏差は正、配列の長さ・添字・パラメータの単位はモデルに合わせます。前提違反は主にassertで検出され、例外を投げる設計ではありません。

### 結果を行動へ使う

- **全粒子平均を基本にする。** 上位k平均は意図的に一部の質量を捨てます。等重みの粒子を上位kにしても、真の仮説に近いk個を選べるわけではありません。`selected_mass` と目的を確認します。05を参照してください。
- **平均したパラメータを代入するだけで済むとは限らない。** 非線形な得点、丸め、経路の成否では、各粒子で得点を計算してから平均します。06、07、23、31を参照してください。
- **抽出後は重みを二重に掛けない。** `sample_indices` の標本は、元の重みを選ばれる頻度に反映しています。重複を残し、標本数で割ります。08を参照してください。
- **失敗を成功扱いで先へ進めない。** `observe().ok=false` なら、その観測は取り込まれていません。モデルの範囲・表示規則・数値計算を確認します。粒子・重み・履歴・成功観測数は維持されますが、乱数器や外部副作用は巻き戻りません。09、37を参照してください。
- **更新できるものを分ける。** 新しい観測と計算量の設定、行動評価用の候補集合は更新できます。観測済み履歴の書き換え・削除、モデルの型、成分数、事前分布を直接変更するAPIはありません。必要なら別のモデルで再構築し、保持した観測を入れ直します。11、12、18、19を参照してください。

## このガイドのコードの検証

40個のコードは、それぞれが単独の `main` を持ち、外部入力なしで動きます。MarkdownのC++コードブロックは、同梱の `step_01.cpp`〜`step_40.cpp` と一致することを機械的に確認しています。

GCC 13.3.0、C++20で、通常の最適化ビルド、`NDEBUG` ビルド、ASan/UBSanビルドの全40例をコンパイル・実行しています。検証は終了コードだけでなく、次の内容も含みます。

- 成功率や正規分布の例を、解析的に求まる平均や確率と比較する。
- 有限の種類・整数・扉の例を、独立した全列挙計算と比較する。
- 状態追跡を、この小例で計算可能な正規分布の逐次更新と比較する。
- 通常尤度版と対数尤度版、コピー提案と差分提案で結果が一致することを確認する。
- 確率の和、範囲、観測数、失敗時の状態維持、出力の有限性を確認する。

検証を実行する場合は、ZIPから展開した `particle_filter_step_by_step_v01` ディレクトリで次を使います。Python 3とGCCが必要です。検証スクリプトはガイドを書き換えません。

```bash
python3 particle_filter_step_by_step_examples_v01/verify_examples.py --mode normal --markdown particle_filter_step_by_step_v01.md
python3 particle_filter_step_by_step_examples_v01/verify_examples.py --mode release
python3 particle_filter_step_by_step_examples_v01/verify_examples.py --mode sanitized
```

掲載した数値は実行結果であり、全てのモデルや入力に対する精度保証ではありません。粒子による近似では複数seedでの確認も必要です。この一式はAPIとモデルの学習例であり、公式のAHCテスターやコンテストスコアの比較ではありません。サニタイザー実行時はリーク検査を無効にしています。
