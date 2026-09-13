# McmcEstimator ステップバイステップガイド

対象は`mcmc_estimator_v09.hpp`、言語はC++20です。既存の利用ガイドとは別冊の、コードを動かしながら読む入門書です。数学は高校生程度を想定し、専門用語は必要になった段階で説明します。本書とコード例は、明示的な更新依頼があるときにだけ更新します。

## この本の読み方

最初から全APIを覚える必要はありません。まずステップ01のプログラムを動かし、その後に一つずつ変更します。発展例には「土台」と「今回の変更」を記します。全機能を一つの大きなプログラムへ積み上げるのではなく、必要な場所で以前の小さい例へ戻って分岐します。これによって、新しい機能以外の部分をなるべく同じに保ちます。

- **01〜08：何を推定するか。** 倍率一つから、切片・三入力・測定誤差・整数・式の種類へ。
- **09〜13：繰り返し使う。** 制限時間、計算の再開、観測の追加・訂正、共有する既知データへ。
- **14〜20：観測の表し方を広げる。** 未知のσ、成功・失敗、件数、複数出力、再現シミュレーションへ。
- **21〜23：一つに決め切れない場合。** 複数のあり得る候補を使い、平均やばらつきを調べる。
- **24〜28：独自モデルが必要な場合。** 観測間の関連、差分評価、割合・順列、自分で作る候補へ。
- **29〜32：上級の組み込み。** 通常のSessionが代行していた管理を、自分で行う下位APIへ。

初回は01〜04までで一区切りにして構いません。観測を毎ターン追加する用途なら11まで、不確かさも使う用途なら21まで進みます。24以降は必要になったときに読みます。巻末には、学習したAPIの索引と設定値の参照表があります。

各例は`main`を持つ独立したプログラムで、省略記号や「前のコードをここへ」の箇所はありません。コードブロック全体を一つの`.cpp`にコピーしてください。ZIPには同じコードが`examples/mcmc_step_01_v01.cpp`〜`mcmc_step_32_v01.cpp`として入っています。複数の例をそのまま連結すると`main`などが重複するので、一つずつ使います。

### 最初の実行準備

ZIPを展開すると、ルートにヘッダ、`examples/`にコードがあります。そのルートで次を実行します。

```bash
g++ -std=c++20 -O2 -I . examples/mcmc_step_01_v01.cpp -o mcmc_step_01
./mcmc_step_01
```

`-std=c++20`は言語規格、`-O2`は最適化、`-I .`は現在のフォルダからヘッダを探す指定です。ヘッダはGCC系の`bits/stdc++.h`を利用します。例が標準ライブラリ用のincludeを追加していないのは、必要な定義がこのヘッダから読み込まれるためです。`-ffast-math`は指定しません。

実際の問題の入出力はコンテストごとに異なるため、この教材はデータをコード内に置き、推定結果を標準出力へ出します。真の係数を使ってデータを生成する例でも、推定器が受け取るのは入力と観測値です。推定関数へ正解の係数を教えているわけではありません。

## ステップ01：倍率を一つだけ求める

**今回の到達点：最小の準備、予測関数、実行、結果の読み取り。**

### 何が分かっていて、何が分からないか

ある装置へ数値$x$を入れると、$a$倍した表示$y$が返るとします。式は$y=ax$だと分かっていますが、倍率$a$が分かりません。入力1で表示2、入力2で表示4、入力3で表示6という記録を得ました。

未知なのは$a$だけです。$x$と$y$は観測済みの入力データであって、探索する変数ではありません。人なら$a=2$と分かる例ですが、まずこれをライブラリへ正しく伝えることを目指します。

### 候補を比べる方法

候補$a$を入れて計算した表示を$ax_i$と書きます。$i$は観測の番号、$x_i$はその入力、$y_i$は実測の表示です。実測との差を二乗して足し、半分にした値を使います。

$$E(a)=\frac12\sum_{i=1}^{3}(y_i-ax_i)^2$$

$E$は「その候補が観測とどれくらい合っていないか」です。$\sum$は全観測について足す記号です。差を二乗するので、正負の差が打ち消されません。半分にしても、どの候補が最小かは変わりません。

$a=0$なら$E=28$、$a=1$なら$E=7$、$a=2$なら$E=0$です。ライブラリは、この値が小さい候補を探します。今回の既定設定では、この式を自分で実装する必要はありません。

### まず動かす

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{1, 2}, {2, 4}, {3, 6}}; // {入力x, 観測y}
    Solver::Parameters p;
    const auto ai = p.real(0, -10, 10); // aの初期値、下限、上限
    auto predict = [ai](const double& x, const McmcState& s) {
        return s.real[ai] * x; // 候補aなら何が観測されるか
    };
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(Solver::Budget::for_steps(20'000));
    if (!result.state) return 1; // 候補がなければ読み出さない
    std::cout << result.state->real[ai] << '\n'; // 推定したa。2付近
}
```

出力は2付近の数値です。小数点以下がわずかにずれていても、コードの失敗とは限りません。返るのは、与えた計算量で見つかった最良候補です。厳密な最適解や真の係数を必ず返す機能ではありません。

### コードを上から順に読む

1. `using Solver = McmcEstimator<>;`は長い型名に短い別名を付けます。`<>`は標準の状態型を使う指定です。
2. `Solver::Observation<double>`は「入力がdoubleで、出力が実数一つ」という観測の型です。`Row`はこの教材で付けた別名です。
3. `rows`に観測を並べます。`{1,2}`は入力1・出力2であり、係数の下限・上限ではありません。
4. `p.real`で未知の実数$a$を登録します。戻り値`ai`は$a$の値ではなく、その値を読むための添字です。今回は最初の実数なので0です。
5. `predict`は候補から表示を再計算する関数です。ライブラリがいろいろな候補を入れて呼びます。
6. `make_observation_model`で、観測列と予測関数を組み合わせます。
7. `make_numeric_session`で、変数の範囲とモデルを持つ推定器を作ります。Sessionは計算途中の候補も保持するオブジェクトです。
8. `solve`へ計算予算を渡します。計算が終わったら、得られた状態から$a$を読みます。

### この例の引数・値を一つずつ確認する

| コード | 型・値 | 何を指定しているか |
|---|---|---|
| `Row`の第1要素 | `double`、1・2・3 | 実際に装置へ与えた入力$x$ |
| `Row`の第2要素 | `double`、2・4・6 | 実際に観測した表示$y$ |
| `p.real`の第1引数 | `double`、0 | 探索を始めるときの$a$。既知の正解という意味ではない |
| 同第2・第3引数 | `double`、−10・10 | $a$に許す下限・上限。端の値も含む |
| `ai` | `std::size_t` | 状態中の$a$の添字。登録する変数ごとに保存して使う |
| `predict`の`x` | `const double&` | 今評価している観測の既知の入力 |
| `predict`の`s` | `const McmcState&` | 今試している未知パラメータの組 |
| `s.real[ai]` | `double` | 今の候補の$a$。`p.real`を呼び直して取得するのではない |
| `make_observation_model` | 観測列、予測関数 | 一件ごとの比較は省略時の損失に任せる |
| `make_numeric_session` | `Parameters`、モデル | 数値変数の候補の作り方と実行の管理を任せる |
| `for_steps`の引数 | `std::uint64_t`、20,000 | 完了した候補変更の試行数の上限。ミリ秒や観測数ではない |
| `solve`の戻り値 | Sessionの`Result` | 候補のコピー・目的関数値・実行情報を持つ |

`McmcState`は候補を入れる箱です。実数は`real`という`std::vector<double>`に入ります。整数用の別の配列もありますが、ステップ07まで使いません。

`result.state`は`std::optional<McmcState>`です。値が得られなかった可能性を表すため、普通の状態とは別に「空」を持てます。`if (!result.state)`で空か確認し、値があるときだけ`->real`で読みます。新しく作ったSessionへ予算0を渡す場合などは、候補がないことがあります。

### ラムダ式と所有の小さな補足

`[ai](...) { ... }`はラムダ式という関数の書き方です。`[ai]`は添字を関数の中へ値として保存する指定です。`const`なので、予測中に入力や状態を書き換えません。`auto`は、右辺から型をコンパイラに決めてもらう書き方です。

`std::move(rows)`は、行の配列をモデルへ受け渡して再利用するための指定です。受け渡した元の`rows`を、この後もデータ入りとして使いません。同様に、作った`model`をSessionへ渡した後は、Sessionを使います。

この例では範囲も予測も自分で指定しています。ライブラリが「式は$ax$だろう」と発見するわけではありません。実際には、装置の規則やシミュレーションから予測関数を作ります。

## ステップ02：切片も未知にする

**土台：01。変更：実数変数$b$を一つ追加し、予測を$ax+b$にする。**

入力0でも表示1になる装置を考えます。倍率だけでは、この基準表示を説明できません。そこで$x$に関係なく足される切片$b$も推定します。

$$E(a,b)=\frac12\sum_{i=1}^{N}(y_i-ax_i-b)^2$$

$N$は観測数で、今回4です。$b$以外の記号は01と同じです。入力0の行は主に$b$を、入力を変えたときの表示の増え方は$a$を判断する材料になります。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{0, 1}, {1, 3}, {2, 5}, {3, 7}};
    Solver::Parameters p;
    const auto ai = p.real(0, -10, 10);
    const auto bi = p.real(0, -10, 10); // 追加: 切片b
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai] * x + s.real[bi];
    };
    auto model = Solver::make_observation_model(std::move(rows), predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto result = session.solve(Solver::Budget::for_steps(30'000));
    if (!result.state) return 1;
    std::cout << result.state->real[ai] << ' ' << result.state->real[bi] << '\n';
}
```

出力の順は$a,b$で、2と1付近です。

追加した`p.real(0,-10,10)`の三つの引数は、今度は$b$の初期値・下限・上限です。`bi`は二番目の実数の添字1になります。`[ai,bi]`で両方の添字を保存し、`s.real[bi]`を足す以外は01と同じです。予算30,000は、この二変数の動作確認用に与えた試行数であり、全問題での推奨値ではありません。

全ての$x$が同じだと、$a$を増やし$b$を減らして同じ表示にできる場合があります。推定器の問題ではなく、観測だけで未知数を区別できない状況です。少なくとも異なる入力を用意します。ノイズがあるときは、二行だけでなく複数の記録を集めると判断材料が増えます。

## ステップ03：入力が三つあるaffine3へ

**土台：02。変更：既知の入力を三要素の配列にし、その効き方を三つ登録する。**

装置に三つのつまみがあります。表示が$a_1x_1+a_2x_2+a_3x_3+b$で決まるとします。つまみの値$x_1,x_2,x_3$は自分で設定でき、効き方$a_1,a_2,a_3$と基準値$b$が未知です。入力三つ・出力一つの問題です。

$$E(a_1,a_2,a_3,b)=\frac12\sum_{i=1}^{N}(y_i-a_1x_{i1}-a_2x_{i2}-a_3x_{i3}-b)^2$$

$x_{ij}$は観測$i$で与えた$j$番目の入力です。$j=1,2,3$に対して係数$a_j$を掛けます。$y_i$、$N$、$E$の意味は02と同じです。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Input = std::array<double, 3>; // 既知の入力を三つまとめる
using Row = Solver::Observation<Input>;

int main() {
    std::vector<Row> rows;
    for (double x1 : {-1.0, 0.0, 1.0})
        for (double x2 : {-1.0, 0.0, 1.0})
            for (double x3 : {-1.0, 0.0, 1.0})
                rows.push_back({{x1, x2, x3}, 2*x1 - 3*x2 + 0.5*x3 + 1});
    // 上は確認用データ。実利用では{{x1,x2,x3},観測y}をここに入れる。
    Solver::Parameters p;
    const std::array<std::size_t, 3> ai{
        p.real(0, -10, 10), p.real(0, -10, 10), p.real(0, -10, 10)};
    const auto bi = p.real(0, -10, 10);
    auto predict = [ai, bi](const Input& x, const McmcState& s) {
        return s.real[ai[0]]*x[0] + s.real[ai[1]]*x[1]
             + s.real[ai[2]]*x[2] + s.real[bi];
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    const auto result = session.solve(Solver::Budget::for_steps(60'000));
    if (!result.state) return 1;
    for (auto i : ai) std::cout << result.state->real[i] << ' ';
    std::cout << result.state->real[bi] << '\n'; // a1,a2,a3,b
}
```

出力は$a_1,a_2,a_3,b$の順で、2、−3、0.5、1付近です。

| 変更した箇所 | 意味 |
|---|---|
| `std::array<double,3>` | 長さ3で固定した実数の配列。`x[0],x[1],x[2]`が三入力 |
| `Observation<Input>` | 入力型だけを変更。出力`value`は引き続きdouble一つ |
| `{{x1,x2,x3},y}` | 外側が一行、内側が三入力。中括弧が一段増える理由 |
| `ai`の型 | `std::array<std::size_t,3>`。値ではなく、三係数の添字をまとめる |
| 三回の`p.real(0,-10,10)` | 各係数を0から開始し、それぞれ独立に−10〜10を許す |
| `bi` | 四番目の実数の添字。切片も−10〜10、初期値0 |
| `for_steps(60'000)` | 四係数の確認用試行数 |

予測の最後の部分で、モデル作成をSession作成の引数へ直接書きました。名前付きの`model`を一行省いただけで、処理の意味は02と同じです。

確認用データは各入力を−1・0・1と変えた27通りです。いつも$x_2=2x_1$のように連動させると、二つの係数の影響を分離できません。三つの入力を別々に変えることが重要です。入力の種類が増えても、未知変数の宣言と予測式という基本構造は変わりません。

## ステップ04：測定にノイズがある

**土台：02。変更：観測行に測定の標準偏差と重みを指定する。**

同じ入力でも、測定値は毎回少しずれるとします。多くの測定では、平均付近の値がよく現れ、極端に遠い値は出にくい、という正規分布を仮定できます。その広がりを表す値が標準偏差$\sigma$です。標準偏差0.2は「誤差が必ず±0.2に収まる」という意味ではありません。

観測ごとに精度が異なっても構いません。標準のGaussian損失は、ずれをその行の標準偏差で割ってから評価します。

$$E(a,b)=\sum_{i=1}^{N}w_i\left[\frac12\left(\frac{y_i-ax_i-b}{\sigma_i}\right)^2+\log\sigma_i\right]$$

$\sigma_i>0$は観測$i$の既知の標準偏差、$w_i\geq0$はその観測の重みです。$\log$は自然対数です。他の記号は02と同じです。正規分布の「起こりにくさ」を足した式で、候補によらない共通定数は省いています。負の対数尤度という名前は、21で確率として読み直す際に使います。

同じ差0.4でも、通常のぶれが0.2なら2倍、0.4なら1倍のずれです。前者を大きな違いとして扱います。既知の$\sigma_i$なら対数項は候補で変わらないため、01〜03の比較にも矛盾しません。そこでは全行$\sigma_i=w_i=1$でした。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    // {x,y,既知の標準偏差,重み}。重みを省略すれば1。
    std::vector<Row> rows{{0,1.2,0.2}, {1,2.9,0.2}, {2,5.0,0.4},
                          {3,7.1,0.2}, {4,8.8,0.2}, {2,999,0.2,0}};
    // 最後の行は使用しない測定。weight=0なら損失に加えない。
    Solver::Parameters p;
    const auto ai = p.real(0, -10, 10), bi = p.real(0, -10, 10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi]; // 予測に乱数を加えない
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    const auto r = session.solve(Solver::Budget::for_steps(30'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << '\n';
}
```

出力は約1.94、1.12です。ノイズを含む記録に最も合う係数は、データ生成元の2、1ちょうどとは限りません。

| 観測のフィールド | 型・既定値 | 設定の意味 |
|---|---|---|
| `input` | 今回double、必須 | 既知の入力 |
| `value` | double、必須 | 実測値。予測値と取り違えない |
| `scale` | double、既定1 | GaussianとHuberで使う既知の標準偏差・誤差尺度。今回0.2または0.4 |
| `weight` | double、既定1 | 非負の重み。今回最後の行だけ0で、その損失を無視する |

`weight=0`でも予測関数の呼び出し自体は省かれません。入力を壊した行を安全に通せる指定ではありません。また、精度の差を`scale`へ反映した後、同じ理由でさらに`weight`を掛けると二重に調整することになります。通常は重み1から始めます。

予測関数は候補が正しいときの平均表示を返します。ノイズを表現したいからといって、その中で乱数を加えません。同じ入力・状態なら同じ評価値になることが必要です。

## ステップ05：ときどき極端に外れる測定

**土台：04。変更：GaussianをHuberへ置き換える。**

普段は$2x+1$付近なのに、一度だけ通信の待ち時間などで大きな測定値が出たとします。全てを二乗誤差で比べると、その一行を合わせるため他の行の予測がずれることがあります。

Huberは、通常の範囲では二乗誤差、大きく外れた範囲ではずれの大きさに比例する損失です。行を丸ごと捨てるのではなく、極端な差の影響を弱めます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{0,1,0.2}, {1,3,0.2}, {2,5,0.2},
                          {3,7,0.2}, {4,9,0.2}, {2,25,0.2}}; // 最後が外れ値
    Solver::Parameters p;
    const auto ai = p.real(0, -10, 10), bi = p.real(0, -10, 10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi];
    };
    auto model = Solver::make_observation_model(
        std::move(rows), predict, Solver::Huber{1.345}); // 損失だけ変更
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto r = session.solve(Solver::Budget::for_steps(30'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << '\n';
}
```

予測式と変数の範囲は04から変えていません。`make_observation_model`の第3引数`Solver::Huber{1.345}`が今回の追加です。中の数値は正のdoubleの`threshold`で、省略時も1.345です。

差を尺度で割った絶対値を$z=|y-\widehat y|/\sigma$、しきい値を$h$とします。$z\leq h$なら$z^2/2$、それを超えたら$h(z-h/2)$を使い、行の対数尺度項と重みも04と同様に扱います。$\widehat y$は予測、$y$は観測です。

今回$\sigma=0.2$なので、元の単位での切り替わりは$1.345\times0.2=0.269$です。しきい値は出力値そのものの単位ではありません。小さくすると早く影響を抑え、大きくするとGaussianに近い扱いになります。まず既定値を使い、想定する外れ方に合わせます。

出力は2、1.054付近です。外れ値の影響を弱めてもゼロにはしていないので、切片が1ちょうどでなくても正常です。しきい値は固定の設定として使います。これ自体を推定したい場合、単に未知変数で置き換えるだけで正しい確率モデルになるとは限りません。

## ステップ06：観測前に分かっていることを加える

**土台：04。変更：全観測に一度だけ加える事前項を用意する。**

入力0の測定しかなく、$b$は判断できても$a$を判断できないとします。一方、装置の仕様から$a$は2付近、通常のばらつきは0.5程度と分かっています。さらに、この例では$a\geq b$という条件も既知とします。数値の大小を比較できるよう、入力は無次元の設定値とします。

観測の損失に、事前の想定からのずれを足します。

$$E(a,b)=R(a,b)+\sum_{i=1}^{N}\frac{(y_i-ax_i-b)^2}{2\sigma_i^2},\qquad R(a,b)=\frac12\left(\frac{a-2}{0.5}\right)^2\quad(a\geq b)$$

$R$は事前項です。$a<b$なら$R=+\infty$として禁止します。この表示式では既知の尺度にだけ依存する定数を省略しています。コードのGaussianはその定数項を含みますが、最小になる候補は同じです。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{0, 1, 0.2}}; // これだけではaは分からない
    Solver::Parameters p;
    const auto ai = p.real(2, -10, 10), bi = p.real(0, -10, 10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi];
    };
    auto prior = [ai, bi](const McmcState& s) {
        // 今回の装置ではa>=bが必要。初期値a=2,b=0はこれを満たす。
        if (s.real[ai] < s.real[bi]) return std::numeric_limits<double>::infinity();
        const double z = (s.real[ai] - 2.0) / 0.5; // aは2付近、事前の標準偏差0.5
        return 0.5*z*z; // 全観測に対して一回だけ加える
    };
    auto model = Solver::make_observation_model(
        std::move(rows), predict, Solver::Gaussian{}, prior);
    auto session = Solver::make_numeric_session(p, std::move(model));
    const auto r = session.solve(Solver::Budget::for_steps(30'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << '\n';
}
```

新しい第4引数`prior`が事前項の関数です。`prior(const McmcState&)`は候補だけを受け取り、doubleを返します。第3引数を省略して第4引数だけを渡すことはできないため、既定の損失を`Solver::Gaussian{}`と明記します。

中心2は仕様からの想定、0.5はその強さを表す正の標準偏差です。0.5を小さくすると中心へ強く寄せ、大きくすると観測を優先しやすくなります。最初の観測や探索開始値を「事前項」と自動解釈するわけではありません。

`std::numeric_limits<double>::infinity()`は正の無限大です。これは制約違反の候補に使えます。ただし、全ての開始候補が無限大なら、ライブラリはそこから有限候補を探し出せません。今回は初期値$a=2,b=0$で制約を満たしています。NaNや負の無限大を評価値として返してはいけません。

出力は2、1付近です。特に$a$は、今回の観測が新しく教えてくれた値ではなく、事前の想定に基づいて選ばれています。この違いを見落とさないことが大切です。

## ステップ07：affine3の係数を整数に限定する

**土台：03。変更：四係数をrealではなくintegerで登録する。**

三つまみの内部倍率と基準値が、工場で整数ステップに設定されているとします。入力や表示は小数でも、未知の$a_1,a_2,a_3,b$は整数です。03と同じ二乗誤差を、小数を含まない候補の中で小さくします。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Input = std::array<double, 3>;
using Row = Solver::Observation<Input>;

int main() {
    std::vector<Row> rows;
    for (double x1 : {-1.0, 0.0, 1.0})
        for (double x2 : {-0.5, 0.0, 0.5})
            for (double x3 : {-0.25, 0.0, 0.25})
                rows.push_back({{x1,x2,x3}, 2*x1 - 3*x2 + x3 + 1});
    Solver::Parameters p;
    const std::array<std::size_t, 3> ai{
        p.integer(0,-10,10), p.integer(0,-10,10), p.integer(0,-10,10)};
    const auto bi = p.integer(0,-10,10); // 四係数が整数。入力と出力は実数のまま。
    auto predict = [ai, bi](const Input& x, const McmcState& s) {
        return s.discrete[ai[0]]*x[0] + s.discrete[ai[1]]*x[1]
             + s.discrete[ai[2]]*x[2] + s.discrete[bi];
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    const auto r = session.solve(Solver::Budget::for_steps(60'000));
    if (!r.state) return 1;
    for (auto i : ai) std::cout << r.state->discrete[i] << ' ';
    std::cout << r.state->discrete[bi] << '\n'; // 丸め不要の整数
}
```

`p.integer(initial,lower,upper)`の引数は三つとも`std::int64_t`です。今回の0・−10・10は、それぞれ整数の開始値・下限・上限です。−10、−9、…、10の21個を許します。係数を連続値で推定してから丸める方法とは異なります。

戻る添字は`real`ではなく、`s.discrete`という`std::vector<std::int64_t>`へ使います。この変更を忘れると、違う値を読んだり範囲外アクセスになります。出力も`discrete`を読むため、丸め処理なしで整数になります。

データの三番目の係数も、03の0.5から整数1へ変えています。出力は2、−3、1、1です。入力の0.5や0.25は、整数である必要がない部分だと分かるように残しています。測定ノイズがあってもこの形を使えます。

整数でも、全ての組を列挙して厳密に調べる機能ではありません。戻るのは予算内の最良候補です。探索候補を正しく制約するために使う指定と考えます。

## ステップ08：式の種類も未知にする

**土台：02。変更：式を選ぶ種類IDを追加する。**

装置の倍率$a$と基準値$b$だけでなく、応答の種類$t$も不明とします。候補の式は次の三つだと分かっています。

| 外部での種類$t$ | 予測式 |
|---|---|
| 1 | $ax+b$ |
| 2 | $ax^2+b$ |
| 3 | $a\sin x+b$ |

種類$t$は全観測で共通です。観測ごとに別の式を自由に選ぶモデルではありません。各候補の種類と係数で全行を計算し、合計損失を比較します。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows;
    for (double x : {-3.0,-2.0,-1.0,0.0,1.0,2.0,3.0})
        rows.push_back({x, 2*std::sin(x)+1}); // 確認用: t=3,a=2,b=1
    Solver::Parameters p;
    const auto ti = p.category(0, 3); // 内部IDは0,1,2。外部のtはID+1。
    const auto ai = p.real(1,-5,5), bi = p.real(0,-5,5);
    auto predict = [ti, ai, bi](const double& x, const McmcState& s) {
        const int t = static_cast<int>(s.discrete[ti]) + 1;
        const double feature = t == 1 ? x : t == 2 ? x*x : std::sin(x);
        return s.real[ai]*feature + s.real[bi];
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    const auto r = session.solve(Solver::Budget::for_steps(80'000));
    if (!r.state) return 1;
    std::cout << r.state->discrete[ti]+1 << ' ' << r.state->real[ai]
              << ' ' << r.state->real[bi] << '\n'; // t,a,b
}
```

`p.category(0,3)`の第1引数は`std::int64_t`の開始ID、第2引数は正のintの種類数です。IDは0以上3未満なので0・1・2です。人向けの$t=1,2,3$と対応させるため、予測と出力では1を足しています。

カテゴリは「近いIDほど似ている」とは扱わない種類の変数です。07の整数は数値の大小に意味がある場合でした。両方とも`discrete`に格納されます。`ti`が0でも、最初の実数の`ai`も0で構いません。別々の配列の添字だからです。

実数$a,b$の範囲は−5〜5、初期値は1と0です。今回の80,000試行は種類の切り替えも含みます。`std::sin`の入力はラジアンです。

出力は$t,a,b$の順で、3、2、1付近です。全入力が0だったり、$a=0$で全表示が一定だったりすると、種類を区別できません。「種類IDを追加すれば、どんな観測でも式が判明する」という意味ではありません。

## ステップ09：実時間の予算を指定する

**土台：02。変更：試行数の予算を、マイクロ秒の予算に置き換える。**

例の再現確認には試行数が便利ですが、コンテストでは「残り20ミリ秒だけ使いたい」と考えます。推定する式は変えず、止め方を変えます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{0,1}, {1,3}, {2,5}, {3,7}};
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10), bi = p.real(0,-10,10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi];
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    // 実行直前に期限を作る。20,000マイクロ秒=20ミリ秒。
    const auto r = session.solve(Solver::Budget::for_us(20'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << ' '
              << r.report.has_estimate << ' ' << r.report.synchronized << '\n';
    // 共通の絶対期限と遷移数上限も併用できる。残ったSessionを再利用。
    const auto end = Solver::Clock::now() + std::chrono::milliseconds(10);
    const auto more = session.solve(Solver::Budget::until(end, 10'000));
    std::cout << (more.report.steps <= 10'000) << '\n';
}
```

最初の出力は$a,b$と二つの確認値です。通常は2、1付近と、1、1が出ます。時間予算なので、係数の精度や試行数は実行環境に依存します。最後の1は、二回目の完了試行数が10,000以下だったことを表します。

| 新しい指定・戻り値 | 型と意味 |
|---|---|
| `Budget::for_us(20'000)` | 非負の`std::int64_t`のマイクロ秒数。作った時点から20ms後を期限にする |
| `Solver::Clock` | `std::chrono::steady_clock`。時刻差を測るための時計 |
| `Clock::now()` | その時計の現在時刻 |
| `std::chrono::milliseconds(10)` | 10msという長さ。現在時刻へ足して絶対期限を作る |
| `Budget::until(end,10'000)` | 第1引数は`Clock::time_point`の絶対期限、第2引数は試行数上限。先に達した方で止める |
| `report.steps` | 今回完了した試行数。初期候補の評価や再評価は数えない |
| `report.has_estimate` | 現在のモデルで評価済みの有限候補があるか |
| `report.synchronized` | 保持中の各候補の評価を、現在のモデルへ揃え終わったか |

`until`の第2引数は省略でき、既定値は`UINT64_MAX`です。その場合は実用上、期限だけで止めます。`for_steps`も`for_us`も引数の省略はできません。何も制限しない`Budget{}`は、通常のAHC利用では避けます。

予算を作ってから別の準備をすると、その間にも期限が近づきます。`solve`の直前で作るか、プログラム全体で共有する絶対期限を使います。一回の予測関数を途中で強制停止する仕組みではないので、重い再現計算や結果の出力の分だけ余裕を残します。

`result.energy`はその候補の目的関数値、`result.report.elapsed_us`は今回の経過マイクロ秒です。小さいenergyがよくても、問題設定や観測列が違う実行と単純比較はしません。対数項を含む損失では負のenergyも正常です。また、確認値が1でも、最適解や統計的収束を証明しているわけではありません。

## ステップ10：同じ問題の計算を分割する

**土台：09。変更：Sessionを作り直さず、同じ観測で二回solveする。**

まず少し計算して別の処理を行い、その後に余った時間で続きを計算したいとします。同じSessionは途中の候補や探索状態を覚えているので、もう一度`solve`を呼べます。

ここでは結果を再現しやすくするため、時間ではなく10,000試行と20,000試行に分けます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{0,1}, {1,3}, {2,5}, {3,7}};
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10), bi = p.real(0,-10,10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi];
    };
    Solver::Param param;
    param.seed = 42; // 再現確認用。未知パラメータではない。
    param.cooling_steps = 30'000; // 二回分を通じた冷却の予定
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict), param);
    const auto first = session.solve(Solver::Budget::for_steps(10'000));
    const auto second = session.solve(Solver::Budget::for_steps(20'000));
    if (!first.state || !second.state) return 1;
    std::cout << second.state->real[ai] << ' ' << second.state->real[bi] << ' '
              << (second.energy <= first.energy) << ' ' << second.report.steps << '\n';
}
```

新しく`Solver::Param`を渡しました。これは探索の設定で、装置の未知係数とは別物です。

| 設定 | 型・既定値 | 今回の値と理由 |
|---|---|---|
| `seed` | `std::uint64_t`、1 | 42。同じ設定・入力・試行数で再現確認するための乱数の種 |
| `cooling_steps` | `std::uint64_t`、0 | 30,000。二回の実行を通じて探索を絞り込む予定量 |
| `make_numeric_session`の第3引数 | `Param`、省略時は既定設定 | 作成した`param`を渡す |

探索では、最初は多少悪い候補への変更も許し、後半はよい候補の周辺を細かく調べます。この強さの設定を温度と呼び、徐々に下げることを冷却と呼びます。今回は詳しいアルゴリズムを知らなくても、「二回分を一つの探索として予定する」と捉えれば十分です。

`cooling_steps=0`では最初のSearchの予算から予定が決まります。短い最初の呼び出しで冷却を終わらせたくない場合、総予定を明示します。冷却が終わっても、以後の`solve`を禁止するわけではありません。観測やモデルを更新すると、新しい目的関数に対する探索として冷却をやり直します。

出力の3番目の1は、二回目の最良energyが一回目より悪くなっていないことです。同じ目的関数で最良候補を保持しているためです。4番目の20,000は二回目だけの試行数で、合計値ではありません。

`Result`は状態のコピーを持つので、保存した`first`を二回目の後に読めます。これに対し、コピーしないビューには寿命の注意が必要で、11と23で扱います。

## ステップ11：新しい観測を追加する

**土台：10。変更：二回の実行の間に、新しく得た行を追加する。**

ターン1で二行を観測して推定し、ターン2でさらに二行が分かったとします。装置の$a,b$はターン間で共通です。Sessionをターンループの外で一度作り、新しい観測だけを足します。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10), bi = p.real(0,-10,10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi];
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::vector<Row>{{0,1},{1,3}}, predict));
    const auto first = session.solve(Solver::Budget::for_steps(20'000));
    if (!first.state) return 1;
    const auto revision = session.revision();
    // 第2ターン: 新しく得た行だけを渡す。過去の行をもう一度渡さない。
    session.add_observations(std::vector<Row>{{2,5},{3,7}});
    std::cout << (session.best().state == nullptr) << ' ';
    const auto next = session.solve(Solver::Budget::for_steps(20'000));
    if (!next.state) return 1;
    std::cout << next.state->real[ai] << ' ' << next.state->real[bi] << ' '
              << (next.revision != revision) << '\n';
}
```

`add_observations`の引数は`std::vector<Row>`の追加分です。この例では`{2,5}`と`{3,7}`だけです。既存の二行を含む四行を渡すと、既存分が二重に数えられます。行の同一性を判定して重複排除する機能ではありません。

追加直後には以前のenergyをそのまま使えなくなります。そこで公開される最良候補はいったん空になります。ただし、内部の候補まで全て捨てるわけではありません。次の`solve`で必要な評価をし、前の候補を出発点として探索できます。条件が整っていれば、まず追加行の分だけを加えて候補を再評価します。

`session.best()`は`Best`を返し、`state`は状態への読み取り専用ポインタ、`energy`は目的関数値です。候補がないときはポインタが`nullptr`です。次の実行・変更までの一時的なビューなので、長く保持したい場合は`solve`の`Result`のようなコピーを使います。

`revision()`は`std::uint64_t`のモデルの改訂番号です。絶対的に何番かよりも、保存した番号と変わったかを見ます。`Result`にも、どの改訂の結果かを示す`revision`があります。

出力は1、2付近、1付近、1です。最初の1は「追加直後には公開候補がない」、最後の1は「モデルの改訂が変わった」を表します。

追加の登録は一度だけ行います。次の計算が途中で時間切れになったときは、同じ観測をもう一度追加せず、`solve`だけをもう一度呼びます。

## ステップ12：観測を訂正し、古い観測を外す

**土台：11。変更：追加だけでなく、全観測の置換と末尾だけの保持を使う。**

記録の間違いに気づいた場合と、最近の観測だけを使いたい場合を考えます。どちらも係数の意味や数は同じなので、Sessionを作り直す必要はありません。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10), bi = p.real(0,-10,10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi];
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::vector<Row>{{0,1},{1,3},{2,7}}, predict));
    session.solve(Solver::Budget::for_steps(20'000));
    // x=2の測定を5へ訂正。置換後に残したい全観測を渡す。
    session.replace_observations(std::vector<Row>{{0,1},{1,3},{2,5},{3,7}});
    std::cout << session.model().size() << ' ';
    session.retain_last(2); // 末尾の{x=2,x=3}だけ残す。二行とも異なるx。
    const auto r = session.solve(Solver::Budget::for_steps(30'000));
    if (!r.state) return 1;
    std::cout << session.model().size() << ' ' << r.state->real[ai]
              << ' ' << r.state->real[bi] << '\n';
}
```

最初は$x=2$の表示を7と誤記しています。`replace_observations`へ渡す四行で5へ訂正し、さらに`retain_last(2)`で最後の二行だけを残します。変更を二つ登録してから、一度だけ`solve`しています。

| メソッド | 引数と意味 |
|---|---|
| `replace_observations(data)` | `std::vector<Row>`。置換後に残す全行を渡す。空なら観測を全て外す |
| `retain_last(count)` | `std::size_t`。登録順で末尾のcount行を残す。0なら全て外し、現在件数以上なら変更しない |
| `model()` | Sessionが所有するモデルへの読み取り専用参照 |
| `model().size()` | 現在の観測数を`std::size_t`で返す |

出力は4、2、2付近、1付近です。最初の二つは置換後と削除後の件数です。二つの$x$が異なるので、残した二行だけでも今回の二係数を区別できます。

古い観測を捨てると、情報も減ります。また、本ライブラリは「係数が毎ターンどう動くか」を自動で学ぶ時系列フィルタではありません。最近の観測に限る工夫はできますが、どの期間で同じ係数だと仮定するかは利用側で決めます。

## ステップ13：共有する地図を訂正する

**土台：11。三入力をまとめた03の考え方も利用。変更：予測関数へ、全観測で共用する既知データを加える。**

道を通った所要時間から、「距離あたりの時間$a$」と「固定の準備時間$b$」を推定します。各観測は通った辺の番号列と所要時間です。辺の距離は地図に一度だけ持たせます。

経路の距離を$d_i$とすると予測は$ad_i+b$です。$d_i$は未知ではなく、記録した辺の距離を足して計算する既知量です。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Path = std::vector<int>; // 通った辺の番号
using Row = Solver::Observation<Path>;

int main() {
    struct Map { std::vector<double> length; }; // 既知の距離を共有
    std::vector<Row> rows{{{0},3}, {{1},5}, {{0,1},7}};
    Solver::Parameters p;
    const auto ai = p.real(1,0.1,5), bi = p.real(0,-5,5);
    auto predict = [ai, bi](const Map& map, const Path& path, const McmcState& s) {
        double distance = 0;
        for (int edge : path) distance += map.length[edge];
        return s.real[ai]*distance + s.real[bi];
    };
    auto model = Solver::make_context_model(std::move(rows), Map{{1,1}}, predict);
    auto session = Solver::make_numeric_session(p, std::move(model));
    session.solve(Solver::Budget::for_steps(20'000));
    // 辺1の登録距離を訂正。本当は最初から2だった場合。
    session.update_model([](auto& model) { model.context().length[1] = 2; });
    const auto r = session.solve(Solver::Budget::for_steps(30'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << ' '
              << session.model().prediction(Path{0,1}, *r.state) << '\n';
}
```

| 新しい型・引数 | 意味 |
|---|---|
| `Path=std::vector<int>` | 通った辺の番号列。辺番号は`length`の有効範囲内にする |
| `Map::length` | `std::vector<double>`の既知の距離。例では辺0と辺1の二本 |
| `make_context_model`の第1引数 | 観測列。行の入力はPath、valueは所要時間 |
| 同第2引数 | 値として所有するMap。今回`Map{{1,1}}` |
| 同第3引数 | `predict(const Map&,const Path&,const McmcState&)`。既知データが先頭引数に増える |
| `p.real(1,0.1,5)` | 距離あたりの時間$a$の初期値1、正の範囲0.1〜5 |
| `p.real(0,-5,5)` | 固定補正$b$の初期値0、範囲−5〜5 |
| `update_model(edit)` | `edit(Model&)`という編集関数をその場で呼び、評価のやり直しを登録する |
| `model.context()` | 所有しているMapへのアクセス。Sessionでは編集コールバックの中で変更する |
| `model.prediction(input,state)` | 指定した入力と候補で予測する。観測の追加は行わない |

コードの`auto& model`は、長いモデル型をコンパイラに推定させるラムダの引数です。ステップ01の`const McmcState&`と異なり、編集するためconstを付けません。

辺1の距離は本当は最初から2だったのに、登録が1だったとします。これを訂正して過去の経路も再評価すると、出力は$a=2,b=1$付近、経路`{0,1}`の予測7付近になります。候補の次元は同じなので、前の探索を利用できます。

一方、「途中のターンから道が長くなった」という実際の時間変化なら、過去の観測には当時の距離を残す必要があります。最新の地図を過去へ適用すると別の問題を解いてしまいます。予測関数の参照キャプチャ先を外からこっそり変更するのではなく、通常はこのContextと`update_model`で変更を知らせます。

変数の数・意味・範囲を変える場合は、地図の訂正とは違い、Parameters・モデルを整えてSessionを作り直します。

## ステップ14：affine3の残差からσを求める

**土台：03。変更：ノイズのあるデータを使い、四係数の推定後に残差を集計する。**

四係数だけでなく、測定系がどの程度ぶれるかも知りたいとします。全観測に共通する一つの標準偏差$\sigma$は固定ですが、その値が未知です。測定ごとに別の$\sigma$を推定する話ではありません。

まず03と同じように係数を合わせます。その予測を$\widehat y_i$とし、残差$r_i=y_i-\widehat y_i$を求めます。残差は「係数を合わせた後にも残った実測との差」です。

$$\mathrm{RSS}=\sum_{i=1}^{N}r_i^2,\qquad \widehat\sigma=\sqrt{\frac{\mathrm{RSS}}{N}}$$

RSSは残差の二乗和、$N>0$は観測数です。二乗平均の平方根を取ると、表示値と同じ単位で誤差の大きさを表せます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Input = std::array<double,3>;
using Row = Solver::Observation<Input>;

int main() {
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0,0.2); // 確認用。推定には真の0.2を渡さない。
    std::vector<Row> rows;
    for (int repeat=0; repeat<4; ++repeat)
        for (double x1 : {-1.0,0.0,1.0})
            for (double x2 : {-1.0,0.0,1.0})
                for (double x3 : {-1.0,0.0,1.0})
                    rows.push_back({{x1,x2,x3},2*x1-3*x2+0.5*x3+1+noise(rng)});
    Solver::Parameters p;
    const std::array<std::size_t,3> ai{p.real(0,-10,10),p.real(0,-10,10),p.real(0,-10,10)};
    const auto bi = p.real(0,-10,10);
    auto predict = [ai, bi](const Input& x, const McmcState& s) {
        return s.real[ai[0]]*x[0]+s.real[ai[1]]*x[1]+s.real[ai[2]]*x[2]+s.real[bi];
    };
    // scale=weight=1、事前項なし。まず四係数の二乗誤差を小さくする。
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows), predict));
    const auto r = session.solve(Solver::Budget::for_steps(80'000));
    if (!r.state) return 1;
    const auto& model = session.model();
    double rss = 0;
    for (const auto& row : model.observations()) {
        const double residual = row.value-model.prediction(row.input,*r.state);
        rss += residual*residual;
    }
    // 観測数>0が必要。これはsolveの時間チェックの外で行う集計。
    const double sigma = std::sqrt(rss/static_cast<double>(model.size()));
    for (auto i : ai) std::cout << r.state->real[i] << ' ';
    std::cout << r.state->real[bi] << ' ' << sigma << '\n';
}
```

出力は四係数とσです。このデータでは2、−3、0.5、1付近と、σが約0.202になります。

### データ生成と、推定に渡す値を分ける

`std::mt19937 rng(1)`はデータ生成用の乱数で、1は再現用の種です。推定器内部の乱数とは別です。`std::normal_distribution<double> noise(0,0.2)`の0は誤差の平均、0.2は生成側の標準偏差です。27通りの入力を4回測り、108行を作ります。`noise(rng)`で各行の測定誤差を作ります。

モデルには、この0.2を渡していません。全行の`scale`と`weight`を省略し、どちらも1で二乗誤差を最小化します。ノイズが共通の尺度なら、係数の最良点を探すためにその値を先に知る必要はありません。ただし、そのまま標本を採ると尺度1のモデルになります。後で求めたσが自動的にモデルへ設定されるわけではありません。

### solveの後に加えた処理

`model.observations()`は、モデルの行を見るための読み取り専用の`std::span<const Row>`です。元の`rows`はモデルへ渡しましたが、ここから全行を再利用できます。各行について`prediction(row.input,*r.state)`を計算し、実測との差の二乗を`rss`へ足します。`model.size()`をdoubleへ変換して割り、`std::sqrt`で平方根を求めます。

このコードの観測は必ず108件です。自分の入力へ置き換えるときも空でない列を渡してください。残差集計は全観測をもう一度走査し、`solve`から戻った後に行うので、その時間も別に確保します。

### なぜ、このσの式が使えるのか

ここでは独立な平均0の正規ノイズ、全行共通の未知σ、重み1、事前項なしを仮定します。候補の四係数を固定すると、Gaussianの合計は定数を除いて次の形です。

$$E(\sigma)=\frac{\mathrm{RSS}}{2\sigma^2}+N\log\sigma$$

小さすぎるσは第一項を大きくし、大きすぎるσは第二項を大きくします。RSSが正のとき、微分して釣り合いを求めると次になります。

$$\frac{dE}{d\sigma}=-\frac{\mathrm{RSS}}{\sigma^3}+\frac{N}{\sigma}=0,\qquad \sigma^2=\frac{\mathrm{RSS}}{N}$$

$dE/d\sigma$はσを少し増やしたときのEの変化の割合です。この正の解が上の平方根です。これは、今の観測を最も説明しやすい値を選ぶ最尤推定です。

そのσをEへ代入すると$N/2+(N/2)\log(\mathrm{RSS}/N)$です。RSSが小さいほどよいので、先に四係数の二乗誤差を小さくして構いません。係数の探索が途中なら、返るのはその時点の係数の残差からの尺度です。モデルの式自体が不適切なときも残差は大きくなるので、全てを測定ノイズだと決めつけません。

### 分母NとN−4、ゼロ残差

今回の最尤推定は分母Nです。一方、通常の連続な線形最小二乗法で四係数を推定し、入力が四係数を区別でき、範囲制約が解に影響せず、$N>4$なら、分散に次の補正も使われます。

$$s^2=\frac{\mathrm{RSS}}{N-4}$$

四係数を観測に合わせた分だけ、残差は元のノイズより小さくなりやすいための補正です。正しい一次モデルと独立な同じ正規ノイズの仮定の下で、$s^2$は分散$\sigma^2$の不偏推定です。平方根$\sqrt{s^2}$自体が、標準偏差の不偏推定になるわけではありません。整数係数、制約が効く解、事前項あり、探索不足の場合にN−4を機械的に使いません。

RSSが0ならコードはσ=0を返します。これは残差尺度としては正しい計算ですが、正のσの範囲で有限の最尤解があるという意味ではありません。後でGaussianへ渡すなら正の下限が必要です。観測が少なく測定誤差まで係数で合わせてしまった場合も、σが小さくなります。

## ステップ15：四係数とσを一緒に探索する

**土台：14。変更：残差集計を外し、未知σを五番目の実数として追加する。**

今度は誤差の大きさも状態へ入れ、各候補のσで観測を評価します。調べたい問題は14と同じですが、未知の誤差尺度を独自損失から読む基本形を学べます。

$$E(a_1,a_2,a_3,b,\sigma)=\frac{1}{2\sigma^2}\sum_{i=1}^{N}(y_i-a_1x_{i1}-a_2x_{i2}-a_3x_{i3}-b)^2+N\log\sigma$$

四係数、入力、表示、観測数の意味は03・14と同じです。今はσも候補によって変わるため、$N\log\sigma$を省略できません。二乗誤差項だけなら、σを大きくするだけで損失を小さくできてしまいます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Input = std::array<double,3>;
using Row = Solver::Observation<Input>;

int main() {
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0,0.2);
    std::vector<Row> rows;
    for (int repeat=0; repeat<4; ++repeat)
        for (double x1 : {-1.0,0.0,1.0})
            for (double x2 : {-1.0,0.0,1.0})
                for (double x3 : {-1.0,0.0,1.0})
                    rows.push_back({{x1,x2,x3},2*x1-3*x2+0.5*x3+1+noise(rng)});
    Solver::Parameters p;
    const std::array<std::size_t,3> ai{p.real(0,-10,10),p.real(0,-10,10),p.real(0,-10,10)};
    const auto bi = p.real(0,-10,10);
    const auto si = p.real(1,0.001,10); // 追加: 全観測に共通する未知sigma
    auto predict = [ai, bi](const Input& x, const McmcState& s) {
        return s.real[ai[0]]*x[0]+s.real[ai[1]]*x[1]+s.real[ai[2]]*x[2]+s.real[bi];
    };
    auto loss = [si](const McmcState& s, const Row& row, double prediction) {
        return Solver::gaussian_loss(row.value,prediction,s.real[si]);
    }; // 全行の重みは1。独自損失には重みが自動で掛からない。
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,loss));
    const auto r = session.solve(Solver::Budget::for_steps(80'000));
    if (!r.state) return 1;
    for (auto i : ai) std::cout << r.state->real[i] << ' ';
    std::cout << r.state->real[bi] << ' ' << r.state->real[si] << '\n';
}
```

| 新しい指定 | 型・値と意味 |
|---|---|
| `p.real(1,0.001,10)` | σの開始値1、正の下限0.001、上限10。表示値と同じ単位で設定する |
| `si` | 実数配列中のσの添字。今回は4 |
| `loss`の第1引数 | 候補状態。ここから未知σを読む |
| 同第2引数 | 観測行全体。入力や実測値、付帯情報を参照できる |
| 同第3引数 | 先に予測関数が返した表示値 |
| `gaussian_loss`の三引数 | 観測値、予測値、正の標準偏差。返すdoubleは二乗誤差項と対数項の合計 |
| `make_observation_model`の第3引数 | 独自損失。第4引数の事前項は省略して0 |

予測関数は四係数の平均表示だけを返します。σは予測へノイズを加えるためではなく、その予測と実測のずれを評価するために使います。行の`scale`はこの独自損失では使っていません。

独自損失の戻り値へ、ライブラリが自動で`row.weight`を掛けることはありません。この例は重みが全て1です。重みを付けたい場合は、重み0の扱いも含めて自分の損失へ反映し、14との等価性の条件も見直します。

データは14と同一で、σは約0.202です。有限の探索予算での結果は完全一致するとは限りません。14は残差尺度をそのまま返し、15はσを0.001〜10に制限しているという違いもあります。同じ制約が必要なら、14の値も`std::clamp(sigma,0.001,10.0)`で制限します。

下限・上限・初期値は測定系の単位に合わせて変更します。0.001は万能の下限ではありません。完全に観測を再現できる場合、σが下限へ寄ることがあります。

## ステップ16：正の変数を対数で持つ

**土台：15。変更：σの代わりに$q=\log\sigma$を状態へ入れる。**

σが0.001から10までのように何桁にもまたがると、小さい値も大きい値も扱いたくなります。正の数をそのまま持つ代わりに、対数を持つ表現を使えます。

$$q=\log\sigma,\qquad \sigma=\exp(q)$$

$q$が新しい実数変数、$\exp$は自然指数関数です。どの実数qでも$\exp(q)>0$です。qを一定量増やす操作は、σを一定の倍率で増やす操作になります。例えばqを約0.693増やすと、σはおよそ2倍です。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Input = std::array<double,3>;
using Row = Solver::Observation<Input>;

int main() {
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0,0.2);
    std::vector<Row> rows;
    for (int repeat=0; repeat<4; ++repeat)
        for (double x1 : {-1.0,0.0,1.0})
            for (double x2 : {-1.0,0.0,1.0})
                for (double x3 : {-1.0,0.0,1.0})
                    rows.push_back({{x1,x2,x3},2*x1-3*x2+0.5*x3+1+noise(rng)});
    Solver::Parameters p;
    const std::array<std::size_t,3> ai{p.real(0,-10,10),p.real(0,-10,10),p.real(0,-10,10)};
    const auto bi = p.real(0,-10,10);
    const auto qi = p.real(0,std::log(0.001),std::log(10.0)); // q=log(sigma)
    auto predict = [ai, bi](const Input& x, const McmcState& s) {
        return s.real[ai[0]]*x[0]+s.real[ai[1]]*x[1]+s.real[ai[2]]*x[2]+s.real[bi];
    };
    auto loss = [qi](const McmcState& s, const Row& row, double prediction) {
        return Solver::gaussian_loss(row.value,prediction,std::exp(s.real[qi]));
    };
    // この例はsolveによる最尤の点推定。標本の事前分布は別途考える。
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,loss));
    const auto r = session.solve(Solver::Budget::for_steps(80'000));
    if (!r.state) return 1;
    for (auto i : ai) std::cout << r.state->real[i] << ' ';
    std::cout << r.state->real[bi] << ' ' << std::exp(r.state->real[qi]) << '\n';
}
```

`p.real(0,log(0.001),log(10))`の初期値0は、σの初期値が$\exp(0)=1$という意味です。下限・上限にも同じ対数変換を適用します。`qi`はqの添字であり、読み出した値をσと誤解しないよう、損失と出力の両方で`std::exp`を使います。

最小化するのは15と同じ損失です。ここでは変数の表現を変えただけなので、範囲内の最尤の点推定としては同じ問題です。出力も四係数と、元の単位へ戻したσです。この表現がどんな問題でも速くなる保証はありません。

ただし、後で「候補を確率的に集める」用途へ進むと、どの座標で一様と考えるかが重要になります。16はまず`solve`の例として読み、その違いは確率を扱う21で確認してください。

## ステップ17：成功・失敗を観測する

**土台：04。変更：実数の表示ではなく0/1を観測し、成功確率を表す損失へ替える。**

装置をある設定$x$で動かすと、成功か失敗が返るとします。入力−1では10回中2回、入力1では10回中8回成功しました。今度は表示値の二乗誤差ではなく、「その成功・失敗が起こりやすいか」で候補を比べます。

直線$ax+b$は範囲外の値も取るので、そのまま確率にはできません。まず$z=ax+b$という自由な実数を作り、次の式で0〜1の成功確率へ変換します。

$$p=\frac{1}{1+\exp(-z)},\qquad z=ax+b$$

$p$は成功確率、zは変換前の値でlogitと呼びます。zが0ならpは0.5、大きい正数なら1に近づき、小さい負数なら0に近づきます。未知なのは引き続きaとbです。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows;
    for (int x : {-1,1})
        for (int j=0; j<10; ++j)
            rows.push_back({static_cast<double>(x), j < (x < 0 ? 2 : 8) ? 1.0 : 0.0});
    // x=-1では10回中2回成功、x=1では10回中8回成功。valueは0か1。
    Solver::Parameters p;
    const auto ai = p.real(0,-5,5), bi = p.real(0,-5,5);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x + s.real[bi]; // 確率ではなくlogitを返す
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::BernoulliLogit{}));
    const auto r = session.solve(Solver::Budget::for_steps(60'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << ' '
              << session.model().prediction(1.0,*r.state) << ' '
              << session.model().mean_prediction(1.0,*r.state) << '\n'; // 最後が成功確率
}
```

`Solver::BernoulliLogit{}`が第3引数に入りました。追加の設定値はありません。予測関数はzを返し、ライブラリがそれを使って0/1の損失を計算します。自分でpへ変換してからこの損失に渡すと、意味が変わってしまいます。

各行の`value`はdoubleの0または1で、1が成功、0が失敗です。`scale`は使いません。`weight`は既定1で、必要なら非負の重みとして使えます。例の10は各設定での試行回数、2と8は成功した回数で、探索のハイパーパラメータではありません。

一件の悪さは、成功なら$-\log p$、失敗なら$-\log(1-p)$です。実際に起きた側に小さい確率を付ける候補ほど大きな損失になります。ライブラリ内では、この式を数値的に安定した形で計算します。

出力はa、b、入力1でのz、入力1でのpです。約1.386、0、1.386、0.8となります。`prediction`は予測関数の戻り値をそのまま返し、`mean_prediction`は標準損失に対応した変換後の平均を返します。Gaussianでは両者は同じでしたが、今回は区別が必要です。

`p.real(0,-5,5)`はa・bの許容範囲であり、確率の範囲ではありません。限られた観測では、未測定の設定への予測が正しいとは限らないので、入力をどの範囲で使うかも考えます。

## ステップ18：時間内の発生件数を観測する

**土台：17。変更：0/1から非負の件数へ、確率への変換から平均件数への変換へ進む。**

一定時間内に装置が処理した要求の件数を調べます。観測時間が倍なら平均件数も倍になると考え、負荷xに応じた単位時間の発生率を推定します。

発生率を$\lambda=\exp(ax+b)$、観測時間を$d>0$とすると、平均件数は$\mu=d\lambda$です。コードが返すのは、その対数です。

$$\log\mu=ax+b+\log d$$

$\lambda$は単位時間あたりの平均件数、dは既知の時間、μはその時間内の平均件数です。aとbが未知です。正の発生率を、16と同じ指数変換で表しています。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;

int main() {
    struct Input { double load, seconds; }; // 既知の負荷と観測時間。seconds>0。
    using Row = Solver::Observation<Input>;
    std::vector<Row> rows{{{0,1},1},{{0,2},5},{{0,3},6},
                          {{1,1},3},{{1,2},9},{{1,3},12}};
    Solver::Parameters p;
    const auto ai = p.real(0,-3,3), bi = p.real(0,-3,3);
    auto predict = [ai, bi](const Input& x, const McmcState& s) {
        return s.real[ai]*x.load + s.real[bi] + std::log(x.seconds);
    }; // log(平均件数) = log(単位時間の発生率) + log(時間)
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,Solver::PoissonLogMean{}));
    const auto r = session.solve(Solver::Budget::for_steps(60'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << ' '
              << session.model().mean_prediction(Input{1,5},*r.state) << '\n';
}
```

`Input::load`がx、`seconds`がdです。行の`value`は0以上の整数値をdoubleで保持します。`seconds`は正にします。`p.real(0,-3,3)`はa・bの開始値0、範囲−3〜3で、発生件数をこの範囲に制限する意味ではありません。

`Solver::PoissonLogMean{}`には設定引数はありません。予測関数は平均件数そのものではなくlog(平均件数)を返します。Poissonは、ある平均件数のもとで0回、1回、2回、…が起こるモデルです。一件の損失は、候補によらない定数を除いて$\mu-y\log\mu$です。yは観測件数です。`scale`は使わず、`weight`は既定1です。

負荷0では合計6秒で12件、負荷1では合計6秒で24件を観測しています。それぞれの平均発生率は2と4になり、aとbはどちらも$\log 2$付近です。`mean_prediction(Input{1,5},state)`は負荷1・5秒という条件で平均件数へ戻し、20付近を返します。20件を必ず観測するという意味ではありません。

時間が長い観測を、時間情報なしの一行として扱うと発生率を取り違えます。また、Poissonの想定より極端にばらつく件数など、別の誤差モデルが必要なら独自損失を設計します。

## ステップ19：出力が二つあり、一部が欠けている

**土台：03の入力配列、15の独自損失。変更：観測と予測の出力も配列にする。**

ある座標系の点を別の座標系で測ると、共通の倍率aで拡大され、横に$b_x$、縦に$b_y$だけずれるとします。測定によっては横だけ、または縦だけ分かります。

予測は横が$ax_1+b_x$、縦が$ax_2+b_y$です。$x_1,x_2$は既知の点、$a,b_x,b_y$が未知です。観測できた軸のGaussian損失だけを足します。今回は横・縦の測定誤差が独立だと仮定します。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;

int main() {
    using Point = std::array<double,2>;
    struct Row {
        Point input;
        std::array<std::optional<double>,2> value; // 測れない軸はnullopt
        Point sigma{0.1,0.2}; // 横・縦それぞれの既知の標準偏差
    };
    std::vector<Row> rows{{{0,0},{1.0,-1.0}},{{1,0},{3.0,std::nullopt}},
                          {{0,2},{std::nullopt,3.0}},{{1,2},{3.0,3.0}}};
    Solver::Parameters p;
    const auto ai = p.real(1,0.1,5), bxi = p.real(0,-5,5), byi = p.real(0,-5,5);
    auto predict = [ai, bxi, byi](const Point& x, const McmcState& s) {
        return Point{s.real[ai]*x[0]+s.real[bxi],s.real[ai]*x[1]+s.real[byi]};
    };
    auto loss = [](const McmcState&, const Row& row, const Point& predicted) {
        double e = 0;
        for (int k=0; k<2; ++k)
            if (row.value[k]) e += Solver::gaussian_loss(*row.value[k],predicted[k],row.sigma[k]);
        return e; // 独立な二軸のうち測定できた分だけ足す
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict,loss));
    const auto r = session.solve(Solver::Budget::for_steps(60'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bxi]
              << ' ' << r.state->real[byi] << '\n';
}
```

標準の`Observation<Input>`は実数出力一つなので、今回は自分で`Row`を定義しました。予測関数へ渡す情報は`input`という名前で持たせます。それ以外のフィールド名は自由です。

| 追加・変更したもの | 型・設定と意味 |
|---|---|
| `Point` | `std::array<double,2>`。入力点・予測点を表す |
| `Row::value` | 二つの`std::optional<double>`。測定できた軸だけ値を持つ |
| `std::nullopt` | 欠測を表す。測定値0で代用しない |
| `Row::sigma` | 横0.1、縦0.2が既定の既知尺度。行ごとに変更可能 |
| `p.real(1,0.1,5)` | 共通倍率aの開始値・範囲 |
| 二回の`p.real(0,-5,5)` | 横・縦の平行移動の開始値・範囲 |
| `predict`の戻り値 | Point。実数一つに限られない |
| `loss`の第3引数 | その予測Pointへの読み取り専用参照 |

`if (row.value[k])`で測定された軸だけを選び、`*row.value[k]`で値を取り出します。損失の和には観測していない軸の罰則を入れません。出力はa、$b_x$、$b_y$の順で、2、1、−1付近です。

この独自損失には`mean`という変換関数を用意していないため、`mean_prediction`は使いません。生の二次元予測は`prediction`で得られます。横と縦の誤差が同時に同じ方向へ動く場合などは、独立な二つの損失の和ではなく、関連も扱う損失が必要です。

## ステップ20：予測がシミュレーションになってもよい

**土台：03。変更：一次関数の代わりに、二台の作業終了を再現する。**

二台の機械へ同時に仕事を渡し、両方が終わった時刻だけを観測します。各機械の仕事量は既知ですが、仕事量を一秒にどれだけ処理するかという速度が分かりません。

一台目の仕事量を$w_0$、速度を$v_0$、二台目を$w_1,v_1$とします。各終了時刻は仕事量÷速度なので、全体の終了時刻は次です。

$$\widehat y=\max\left(\frac{w_0}{v_0},\frac{w_1}{v_1}\right)$$

$\max$は二つのうち大きい方です。$w_0,w_1$は入力、$v_0,v_1$は正の未知係数、$\widehat y$は予測する終了時刻です。観測との差は04のGaussianで評価します。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Input = std::array<double,2>; // 二台へ同時に渡す仕事量
using Row = Solver::Observation<Input>;

int main() {
    std::vector<Row> rows{{{2,0},1,0.1},{{0,3},1,0.1},
                          {{4,3},2,0.1},{{2,6},2,0.1}};
    Solver::Parameters p;
    const auto si = p.real(1,0.1,10), ti = p.real(1,0.1,10); // 二台の速度
    auto predict = [si, ti](const Input& work, const McmcState& s) {
        const double end0 = work[0]/s.real[si];
        const double end1 = work[1]/s.real[ti];
        return std::max(end0,end1); // 両方が終わった時刻を再現する
    };
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict));
    const auto r = session.solve(Solver::Budget::for_steps(60'000));
    if (!r.state) return 1;
    std::cout << r.state->real[si] << ' ' << r.state->real[ti] << ' '
              << session.model().prediction(Input{6,3},*r.state) << '\n';
}
```

二つの`p.real(1,0.1,10)`は、二台の速度の開始値1・下限0.1・上限10です。ゼロで割らないよう下限を正にしています。各観測の0.1は終了時刻の既知の標準偏差で、速度の下限とは別の量です。

出力は$v_0,v_1$と新しい仕事量`{6,3}`の終了時刻で、2、3、3付近です。片方だけに仕事を与える観測も入れているため、それぞれの速度を区別できます。いつも同じ機械だけが遅い観測では、もう一台の速度を正確に決められない場合があります。

新しいライブラリAPIはありません。予測の中で分岐やmaxを使ってもよく、数式の微分を用意する必要もありません。より長いイベント列の再現も同じ形ですが、一回の評価が長いと時間超過しやすくなります。同じ候補で異なる乱数結果を毎回返すシミュレータは、このままでは使えません。

## ステップ21：一つの係数に決めず、あり得る候補を集める

**土台：04。変更：solveの代わりにsampleを呼び、未知の係数による予測のばらつきを求める。**

ここまでは一つの最良候補を使いました。しかし、ノイズのある少数の観測だけでは、少し違う係数も十分あり得ます。入力4での平均表示を予測するとき、その不確かさも見たいとします。

### 「よい候補だけ」から「あり得る候補」へ

04のGaussian損失は、候補が正しいとしたら今の測定がどの程度起こりやすいかを表せます。その起こりやすさを尤度と呼びます。損失Eは、その尤度の対数を取って符号を反転したものを足した値でした。

観測前に各係数を範囲内で一様に考える場合、観測後に候補を重く扱う度合いは次になります。

$$\pi(\theta\mid D)=\frac{\exp(-E(\theta))}{Z}$$

$\theta=(a,b)$は候補の組、Dは観測全体、πは観測後の確率密度、Zは全体が1になるようにする正の有限な定数です。連続値では「ある一点の確率」ではなく、小さい範囲の確率を表す密度を使います。Zは自分で計算する必要はありません。Eが小さい候補ほど多く現れやすい、という関係をまず捉えます。

06のような事前項を入れれば、観測前の想定も反映します。任意の罰則を加えたEで標本を得ることもできますが、そのばらつきが現実の不確かさを表すかはモデル次第です。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{0,1,0.5},{1,3,0.5},{2,5,0.5},{3,7,0.5}};
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10), bi = p.real(0,-10,10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x+s.real[bi];
    };
    Solver::Param param;
    param.warmup_steps = 4'000; // 標本を記録しない準備の遷移数
    param.sample_stride = 2; // 各鎖で2遷移ごとに標本を記録
    auto session = Solver::make_numeric_session(p,
        Solver::make_observation_model(std::move(rows),predict),param);
    // solveを先に呼ぶ必要はない。x=4での平均表示を候補ごとに計算する。
    const auto sampled = session.sample(Solver::Budget::for_steps(84'000),
        [&](const McmcState& s) { return session.model().mean_prediction(4.0,s); });
    const auto& mean = sampled.summary.mean();
    const auto variance = sampled.summary.variance();
    if (!mean || !variance) return 1;
    std::cout << *mean << ' ' << *variance << ' ' << sampled.summary.count()
              << ' ' << sampled.warmup_remaining << '\n';
}
```

### 新しい呼び出しを順に読む

`sample(budget,project)`は、あり得る候補を標本として取り出し、各標本に`project`を適用して集計します。今回のprojectは`project(const McmcState&)`という関数で、入力4での予測をdoubleで返します。予測条件4は未知数ではなく、利用者が知りたい次の入力です。

`solve`を先に呼ぶ必要はありません。標本を取る前に、SessionがWarmupという準備をします。開始値の影響を減らし、数値変数の変更幅を調整する期間です。その後のSampleでは、分布を変える変更幅の適応を止めて標本を採ります。このように前の候補を少しずつ変えて標本を得る方法がMCMCです。

| 新しい引数・結果 | 型・既定値と今回の意味 |
|---|---|
| `warmup_steps` | `std::optional<std::uint64_t>`、既定は未指定。今回は4,000試行を準備に使う |
| `sample_stride` | `std::uint64_t`、既定1。今回は各鎖の2試行ごとに記録する |
| `for_steps(84'000)` | 準備も含む総試行数。今回は4,000+80,000 |
| `project` | 候補から集計したい量を返す関数。今回の返り値は平均表示のdouble |
| `summary.count()` | 記録した標本の個数 |
| `summary.mean()` | 平均を持つoptional。0件なら空 |
| `summary.variance()` | 分散を持つoptional。2件未満なら空 |
| `warmup_remaining` | 呼び出し後に残る準備試行数 |

既定の準備量は、数値変数が動く場合は`max(512,64×可動変数数×鎖数)`です。全変数固定なら0、独自状態では512×鎖数です。明示的な0は準備を省略する指定で、通常の入門例では使いません。既定値や今回の4,000は、収束の保証ではありません。

### 出力が表している不確かさ

出力は、平均が9付近、分散が0.375付近、標本数40,000、残り準備0です。データが直線上にあっても、既知の測定標準偏差を0.5と仮定しているため、係数には幅が残ります。

ここでの分散は「未知係数が違った場合、平均表示がどれくらい違うか」です。これから実際に一回測る表示には、さらに測定ノイズが加わります。今回の独立な既知ノイズなら、その表示の分散は推定された平均表示の分散に$0.5^2$を足したものです。ノイズを含む将来観測と、ノイズを除いた平均予測を区別します。

### 標本を使うときの注意

標本は最良候補の更新記録ではありません。候補を変更しなかった試行でも現在の状態を数えます。受理された候補だけを記録すると、対象の分布が変わることがあります。

また、近い時刻の標本には関連があります。40,000件あるから独立な40,000回分の情報があるとは限りません。strideを増やしても独立性や収束を保証しません。分散は標本値の散らばりで、平均値の誤差そのものではありません。

短い予算で準備だけが進み、標本0件で終わることもあります。空を確認し、同じSessionでさらに`sample`を呼べば準備の続きから進みます。途中に`solve`を挟むと、実際に探索した後は標本用の準備をやり直します。

### 16の対数変数を標本採取へ使う場合

15はσそのもの、16は$q=\log\sigma$を状態にしました。事前項0で標本を取ると、前者はσについて一様、後者はqについて一様な事前の扱いです。点推定で同じ最小点になることと、同じ分布から標本が得られることは別です。

例えばσの1〜2と2〜4は、qでは同じ幅になります。qで一様なら、この二つの範囲を同じ重さにします。σで一様なら幅が二倍の2〜4を二倍重くします。これが差の理由です。

qを状態に持ちながらσに一様な事前分布を指定するには、σの幅が$\exp(q)$倍になる分を補います。

$$E_q(q)=E_\sigma(\exp(q))-q$$

$E_\sigma$はσ座標での負の対数密度、$E_q$はq座標での負の対数密度です。この幅の補正をヤコビアン補正と呼びます。16へ06と同じ第4引数の事前項を加え、その中で`-s.real[qi]`を返す形です。σ座標に別の事前項があるなら、その項をσへ戻して計算した上で−qも加えます。最尤の点推定のためだけに16を実行するときは、この項を勝手に足しません。

## ステップ22：分割した標本を自分で集計する

**土台：21。変更：sample_eachで標本を受け取り、二つの予測を複数回にわたって集計する。**

入力4と5の両方を予測したいとします。計算は分割し、途中で観測も一行追加します。同じモデルで取った標本はまとめ、モデルが変わったら集計を切り替えます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10), bi = p.real(0,-10,10);
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x+s.real[bi];
    };
    Solver::Param param;
    param.warmup_steps = 4'000;
    param.sample_stride = 2;
    auto session = Solver::make_numeric_session(p,Solver::make_observation_model(
        std::vector<Row>{{0,1,0.5},{1,3,0.5},{2,5,0.5},{3,7,0.5}},predict),param);
    using Session = decltype(session);
    Session::Moments<std::array<double,2>> totals; // x=4,x=5での予測を同時に集計
    auto revision = session.revision();
    for (int part=0; part<3; ++part) {
        if (part==2) session.add_observations(std::vector<Row>{{4,9,0.5}});
        if (session.revision()!=revision) {
            totals = {}; // 古い観測条件の標本を混ぜない
            revision = session.revision();
        }
        session.sample_each(Solver::Budget::for_steps(42'000),
            [&](const McmcState& s, double energy, int chain) {
                (void)energy; (void)chain; // 必要なら診断・保存に利用できる
                totals.add({session.model().mean_prediction(4.0,s),
                            session.model().mean_prediction(5.0,s)});
            });
    }
    if (!totals.mean()) return 1;
    std::cout << (*totals.mean())[0] << ' ' << (*totals.mean())[1]
              << ' ' << totals.count() << '\n';
}
```

21の`sample`は、呼び出すたびに新しい集計を返します。ここでは代わりに`sample_each`へ処理関数を渡し、外で作った`totals`に加えます。

| 新しい要素 | 役割 |
|---|---|
| `using Session=decltype(session)` | 作ったSessionの具体的な型へ別名を付ける |
| `Session::Moments<std::array<double,2>>` | 二つのdoubleの個数・平均・分散を逐次集計する型。標本自体は保存しない |
| `sample_each(budget,emit)` | 準備を管理し、各標本についてemitを呼ぶ。戻り値はReport |
| `emit`の第1引数 | `const McmcState&`。記録する時点の候補 |
| 同第2引数 | `double energy`。その候補の目的関数値 |
| 同第3引数 | `int chain`。どの鎖から出た標本かを表す0始まりの番号 |
| `totals.add(value)` | 今回は二要素の予測配列を一つ追加する |
| `totals={}` | 集計を空の初期状態へ戻す |

`(void)energy`などは、今回は受け取った値を使わないと示すC++の書き方です。`[&]`はこの呼び出し中に`session`と`totals`を参照する指定です。標本処理のコールバックをSessionが後のために保存するわけではありません。

最初の二回は同じモデルなので集計を継続します。三回目の前に観測を足すと`revision`が変わり、集計を空にします。追加後の4,000試行は再準備、残り38,000試行を2回に一回記録するので、最後の標本数は19,000です。平均は入力4で9付近、入力5で11付近です。

`Moments`と`sample`の集計値にはdouble、`std::array<double,N>`、`std::vector<double>`を使えます。配列・vectorの長さを途中で変えません。分散は各成分について計算され、成分間の共分散は自動では得られません。標本自体を保存したいならemit内でコピーします。参照やポインタをそのまま保存しません。

観測が変わらなくても、「入力4の予測」から「別の入力の予測」へ集計内容を変えたら、古い集計を混ぜないようにします。改訂番号だけでは予測条件の変更まで検出できません。

予測値が行動ごとのコストなら、その標本平均が小さい行動を選ぶ使い方にもつながります。ばらつきも嫌うなら、平均に標準偏差の何倍かを足して比較するなど、コンテストの目的に合わせます。非線形の予測では「平均した係数を一度だけ予測式に入れる」結果と一致しないため、ここでのように標本ごとに予測を計算します。

## ステップ23：開始値を変え、複数の鎖を使う

**土台：21。変更：三つの候補列を動かし、開始値と変更幅を明示する。**

一つの開始点に頼りたくない場合、複数の候補列を持てます。この一列を鎖と呼びます。ライブラリは単スレッドで鎖を順番に進めます。三鎖だからCPU三個を使うわけではありません。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10,0.2), bi = p.real(0,-10,10,0.2); // 最初の変更幅
    auto predict = [ai, bi](const double& x, const McmcState& s) {
        return s.real[ai]*x+s.real[bi];
    };
    Solver::Param param;
    param.warmup_steps = 6'000;
    param.sample_stride = 3;
    Solver::MoveParam moves;
    moves.difference_probability = 0.1; // 3鎖以上のWarmup/Sampleで使う
    auto session = Solver::make_numeric_session(p,Solver::make_observation_model(
        std::vector<Row>{{0,1,0.5},{1,3,0.5},{2,5,0.5},{3,7,0.5}},predict),param,moves,3);
    std::vector<McmcState> starts(3,p.initial());
    starts[0].real[ai]=-2; starts[0].real[bi]=2;
    starts[1].real[ai]=2;  starts[1].real[bi]=1;
    starts[2].real[ai]=4;  starts[2].real[bi]=0;
    session.restart(std::move(starts)); // 同じ変数構造で、開始値だけを変更
    const auto report = session.solve_view(Solver::Budget::for_steps(30'000));
    const auto best = session.best(); // 状態のコピーを省いた読み取り
    if (!report.has_estimate || !best.state) return 1;
    std::cout << best.state->real[ai] << ' ' << best.state->real[bi] << ' ';
    // best.stateはこの実行以降に使わない。
    const auto sampled = session.sample(Solver::Budget::for_steps(60'000),
        [&](const McmcState& s) { return session.model().mean_prediction(4.0,s); });
    if (!sampled.summary.mean()) return 1;
    std::cout << *sampled.summary.mean() << ' ' << sampled.summary.count() << '\n';
}
```

| 新しい指定 | 型・既定値と意味 |
|---|---|
| `real`の第4引数0.2 | doubleの最初の候補変更幅。省略時は0で、有限区間の幅の10%を自動設定する |
| `MoveParam` | 数値候補の作り方の設定。装置の未知パラメータではない |
| `difference_probability=0.1` | 他の二鎖の差を使う変更の確率。既定0。Warmup/Sampleで、三鎖以上あるときに働く |
| `make_numeric_session`の第4引数 | `MoveParam`。省略時は既定設定 |
| 同第5引数3 | `std::size_t`の鎖数。正にする。既定1 |
| `p.initial()` | 宣言時の開始状態へのconst参照。ここでは三個にコピーする |
| `restart(initial)` | 空でない`std::vector<State>`の開始候補で再初期化する |
| `warmup_steps=6'000` | 今回の三鎖合計の準備試行数。鎖ごとに6,000ではない |
| `sample_stride=3` | 鎖ごとに三試行に一回を記録する |
| `solve_view(budget)` | Resultの状態コピーを作らず、Reportだけを返す |

ここでの0.2は、04の測定ノイズの`scale`とは違います。探索中に係数をどれくらい動かすかという幅です。小さすぎると移動が遅く、大きすぎると外れる候補が増えます。通常は自動設定を出発点にし、問題の単位に合わせて必要なときだけ明示します。

既定では数値変更幅を調整する`adapt=true`、Searchで遠い変更も試す`cauchy=true`です。また、他の一鎖を基準に実数の組を伸縮する`stretch_probability=0.3`がWarmup/Sampleで有効です。こちらは二鎖以上で働きます。差分と伸縮の二つの確率は非負で、合計を1以下にします。どちらもSearchでは使いません。

`make_numeric_session`は、指定した個数の鎖を同じ初期値から作ります。今回は`restart`で−2・2・4という異なるaなどへ変更しています。全候補で変数の数、範囲、添字の意味を同じにします。`restart`は乱数と探索履歴を初期化しますが、数値提案の学習済みの変更幅は保持します。完全に新品と同じ状態が必要ならSessionを作り直します。

`solve_view`の後、23の`best.state`から係数を出力します。このポインタは次の実行・変更までのビューなので、その後の`sample`を呼んだ後には使っていません。出力はa=2、b=1、入力4の平均9付近と、標本数18,000です。

鎖同士を使う変更もあるため、それぞれを独立した実験として誤差評価できるとは限りません。多峰性のある問題では、複数鎖でも別の山を見逃すことがあります。鎖を増やすこと自体が精度の保証ではなく、固定予算の分配も変わります。

## ステップ24：観測同士が関連する全体評価

**土台：02・04。変更：一件ずつの独立な損失の合成をやめ、全体の悪さを返す。**

センサーが一度大きめにずれると、次の測定も同じ方向にずれやすいとします。直線の係数は同じですが、測定誤差を各行で独立だとは扱えません。

残差を$r_i=y_i-(ax_i+b)$とします。前の残差の60%が次にも残り、そこへ新しい独立な正規ノイズが加わると仮定します。持続率を$\rho=0.6$、新しいノイズの標準偏差を$\sigma=0.2$とし、どちらも既知です。

最初の残差も、同じ状態が長く続いたときの分散$\sigma^2/(1-\rho^2)$から生じたと仮定すると、定数を除く合計は次です。

$$E(a,b)=\frac{(1-\rho^2)r_1^2}{2\sigma^2}+\sum_{i=2}^{N}\frac{(r_i-\rho r_{i-1})^2}{2\sigma^2}$$

最初の項は一行目の誤差、残りは「前から予想できた分を引いた、新しい誤差」の悪さです。Nは時間順の観測数です。$|\rho|<1$、$\sigma>0$とします。今回ρ・σは既知なので、それらだけに依存する対数項は省いて構いません。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;
using Row = Solver::Observation<double>;

int main() {
    std::vector<Row> rows{{0,1},{1,3},{2,5},{3,7}}; // 時間順に並べる
    Solver::Parameters p;
    const auto ai = p.real(0,-10,10), bi = p.real(0,-10,10);
    const double rho = 0.6, sigma = 0.2; // 誤差の持続率、新しい誤差の標準偏差
    auto energy = [rows=std::move(rows), ai, bi, rho, sigma](const McmcState& s) {
        double e=0, previous=0;
        for (std::size_t i=0; i<rows.size(); ++i) {
            const double residual = rows[i].value-(s.real[ai]*rows[i].input+s.real[bi]);
            const double z = i==0 ? residual*std::sqrt(1-rho*rho)/sigma
                                  : (residual-rho*previous)/sigma;
            e += 0.5*z*z;
            previous = residual;
        }
        return e; // 全体の負の対数尤度。既知のrho,sigmaにだけ依存する定数は省略。
    };
    // 観測モデルを組み立てず、全体評価をそのまま渡す。
    auto session = Solver::make_numeric_session(p,std::move(energy));
    const auto r = session.solve(Solver::Budget::for_steps(40'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << '\n';
}
```

新しい渡し方は`make_numeric_session(p,energy)`だけです。第2引数には観測モデルだけでなく、`energy(const McmcState&)`でdoubleを返す関数を直接渡せます。この関数が目的関数全体を返すため、外側で自動的に観測数だけ呼び出したり事前項を足したりはしません。

`[rows=std::move(rows),...]`は、ラムダの中へ行の配列を所有させるC++の書き方です。コードの添字は0始まりなので、式の$r_1$が`i==0`の分岐に対応します。`previous`へ前回の残差を入れているため、行の時間順序が重要です。出力は2、1付近です。

この変更で、任意のシミュレーション結果全体の比較、項に分けられない罰則などにも対応できます。評価は同じ状態なら同じ値を返し、有限doubleまたは正の無限大にします。

このラムダには観測モデル専用の`add_observations`などはありません。また、ラムダに所有させた行を外から直接編集するAPIもありません。更新が必要なら、次の例のように公開データを持つモデル型を作り、Sessionの`update_model`から編集するか、Sessionを作り直します。

## ステップ25：候補の変更に関係する項だけを再計算する

**土台：24。変更：全体評価をモデル型へまとめ、候補の差分評価を追加する。**

複数の端子の電圧を調べます。測れるのは、端子aの電圧から端子bの電圧を引いた差です。端子一個の電圧候補を変えても、その端子と関係ない測定の予測は変わりません。

そこで、通常の全評価を残したまま、一部の候補評価を省力化します。ここで変わるのは測定データではなく、試している未知係数の候補です。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver = McmcEstimator<>;

int main() {
    struct Row { std::size_t a,b; double difference; };
    struct Model {
        std::vector<Row> rows;
        std::vector<std::vector<std::size_t>> incident;
        double term(std::size_t k, const McmcState& s) const {
            const auto& row=rows[k];
            return Solver::gaussian_loss(row.difference,s.real[row.a]-s.real[row.b],0.1);
        }
        double operator()(const McmcState& s) const {
            double e=0;
            for (std::size_t k=0; k<rows.size(); ++k) e+=term(k,s);
            return e;
        }
        double candidate(const McmcState& old, const McmcState& next, double old_energy) const {
            std::optional<std::size_t> changed;
            for (std::size_t j=0; j<next.real.size(); ++j) if (old.real[j]!=next.real[j]) {
                if (changed) return (*this)(next); // 複数変更なら全評価へ戻す
                changed=j;
            }
            double e=old_energy;
            if (changed) for (auto k:incident[*changed]) e+=term(k,next)-term(k,old);
            return e; // 増分ではなく、新候補の目的関数全体を返す
        }
    };
    // 端子aの電圧−端子bの電圧。全端子が測定を介して端子0につながる。
    Model model{{{1,0,1},{2,1,1},{3,2,1},{3,0,3}},std::vector<std::vector<std::size_t>>(4)};
    for (std::size_t k=0; k<model.rows.size(); ++k) {
        model.incident[model.rows[k].a].push_back(k);
        model.incident[model.rows[k].b].push_back(k);
    }
    Solver::Parameters p;
    p.real(0,0,0); // 基準端子0は0Vに固定。添字は存在するが探索では動かない。
    for (int i=1; i<4; ++i) p.real(0,-5,5);
    auto session=Solver::make_numeric_session(p,std::move(model));
    const auto r=session.solve(Solver::Budget::for_steps(60'000));
    if (!r.state) return 1;
    for (double v:r.state->real) std::cout << v << ' ';
    std::cout << '\n';
}
```

`Row`のa・bは端子番号、`difference`は測定した電圧差です。ここでのa・bは、02の直線の係数名とは無関係です。`term(k,s)`は、k番目の測定について、候補sの電圧差と実測差を標準偏差0.1のGaussianで比較します。全体のEは、この全測定分の和です。

電圧差だけでは、全端子へ同じ定数を足しても観測が変わりません。その曖昧さを取り除くため、端子0を既知の0Vに固定します。`p.real(0,0,0)`の下限と上限を等しくすると、その変数は状態には残りますが動かされません。他の三端子は初期値0、範囲−5〜5です。出力は0、1、2、3付近です。

### モデル型の二つの入口

| メソッド | 入力・戻り値 |
|---|---|
| `operator()(state)` | 候補全体を受け取り、目的関数全体を返す。初期評価・モデル更新後などに必要 |
| `candidate(old,next,old_energy)` | 旧候補、新候補、旧候補の全体評価を受け取り、新候補の**全体評価**を返す |

`operator()`は、そのオブジェクトを関数のように呼べるようにするC++の書き方です。`Model`には行と、各端子に関係する行の番号列`incident`を持たせています。

一端子だけが変わった場合、新しい全体評価は「旧全体評価＋関係する各項の新値−旧値」です。コードは変更端子を見つけ、`incident`の行だけ計算し直します。変化がないなら旧値を返し、複数端子が変わった場合は全評価へ戻します。23のような複数変数を一緒に動かす提案でも正しく動かすための分岐です。

変更箇所の検出自体には全変数の比較が必要です。そのため、全ての問題で速くなるわけではありません。多数の高価な観測があり、一変数に関係する観測が少ない場合に検討します。通常は全評価で動くことを確認してから追加します。

`candidate`の中で共有キャッシュを新候補のものへ書き換えてはいけません。評価されても、その候補が採用されるとは限らないからです。返すのも増分そのものではありません。この違いは、モデルの更新に増分を使う32で再確認します。モデルを編集する場合は、行と`incident`の対応も更新してから`update_model`のコールバックを終えます。

## ステップ26：合計1になる混合割合を推定する

**土台：24の全体評価。変更：自分の状態型と、その構造を壊さない変更方法を使う。**

三種類の材料を混ぜた割合を調べます。各割合は0以上で、三つの合計は1です。三つを独立な実数として動かすと合計1を壊すので、一成分へ足した分を他の成分から引く変更を使います。

各センサーで純材料が出す反応は既知です。k番目のセンサーでj番目の材料の反応を$c_{kj}$、未知の割合を$w_j$とすると、混合物の予測反応は次です。

$$\widehat y_k=\sum_{j=1}^{3}c_{kj}w_j,\qquad w_j\geq0,\qquad w_1+w_2+w_3=1$$

kは三つのセンサーの番号、$\widehat y_k$は予測値です。三つの実測値とのGaussian損失を足します。

```cpp
#include "mcmc_estimator_v09.hpp"

int main() {
    struct Mixture { std::array<double,3> weight; };
    using Solver=McmcEstimator<Mixture>; // 今回の状態型を指定
    const std::array<std::array<double,3>,3> response{{{1,0.2,0},{0,1,0.5},{0.1,0,1}}};
    const std::array<double,3> observed{0.26,0.55,0.52};
    auto energy=[response, observed](const Mixture& s) {
        double e=0;
        for (int k=0; k<3; ++k) {
            double predicted=0;
            for (int j=0; j<3; ++j) predicted+=response[k][j]*s.weight[j];
            e+=Solver::gaussian_loss(observed[k],predicted,0.02);
        }
        return e;
    };
    auto move=Solver::symmetric_move([](const Mixture&, Mixture& next,
                                      Solver::Rng& rng, const Solver::MoveContext&) {
        const int i=rng.integer(3), j=(i+1+rng.integer(2))%3; // 異なる二成分
        const double amount=0.1*(2*rng.uniform()-1); // -0.1以上0.1未満
        next.weight[i]+=amount;
        next.weight[j]-=amount; // 合計1を保つ
        return next.weight[i]>=0 && next.weight[j]>=0; // 範囲外は切り詰めず棄却
    });
    const Mixture initial{{1.0/3,1.0/3,1.0/3}};
    auto session=Solver::make_session(std::vector<Mixture>{initial},energy,move);
    const auto r=session.solve(Solver::Budget::for_steps(60'000));
    if (!r.state) return 1;
    for (double w:r.state->weight) std::cout << w << ' ';
    std::cout << std::accumulate(r.state->weight.begin(),r.state->weight.end(),0.0) << '\n';
}
```

### 自分の状態を用意する

`Mixture`が状態型です。中には割合の配列`weight`だけを持たせました。`McmcEstimator<Mixture>`と指定すると、標準の`McmcState`の代わりにこの型を使えます。状態はコピー可能で、実行中に形が変わらないものにします。

`response`は三行三列の既知の反応係数で、外側の添字がセンサー、内側が材料です。`observed`が混合物の三実測値、0.02が各測定の既知の標準偏差です。確認用の正解割合は0.2、0.3、0.5です。`Parameters`は使わず、開始状態を直接`Mixture`で作ります。

### 変更方法を一つずつ読む

変更関数は`move(current,next,rng,context)`という形です。呼ばれた時点でnextにはcurrentのコピーが入っています。自分で状態全体を複製する必要はありません。

| 引数・設定 | 意味 |
|---|---|
| `const Mixture&` | 現在の候補。この例はnextを少し変えるだけなので直接は読まない |
| `Mixture& next` | 変更先の候補。参照なのでここへの変更が使われる |
| `Solver::Rng& rng` | 推定器が管理する乱数。自分で毎回初期化しない |
| `const MoveContext&` | フェーズなどの情報。今回は使わない |
| `rng.integer(3)` | 0・1・2を同じ確率で選ぶ。引数は個数で、最大値ではない |
| `rng.integer(2)` | 0・1を選び、iと異なるjを作るために使う |
| `rng.uniform()` | 0以上1未満の実数 |
| `amount`の0.1 | 一度に移す量の尺度。割合の単位。大きすぎると負の割合を作りやすい |
| `initial` | 全成分1/3。非負かつ合計1の実行可能な開始点 |

負の割合になったらfalseを返して、その試行を棄却します。端で0へ切り詰めて無理に採用するのではありません。成分間で足し引きするので合計1を保ちます。浮動小数点の通常の丸め誤差はあり得ます。

`Solver::symmetric_move`は「正方向と逆方向を同じ確率で提案できる」という宣言です。今回の二成分を逆順に選んで同じ量を移せば逆操作になり、その選び方も同じ確率です。ライブラリが対称性を自動で検証するわけではありません。

`make_session`の第1引数は開始状態の空でないvector、第2引数はモデルまたは全体評価、第3引数は変更方法です。第4引数の`Param`は省略して既定値を使います。出力は三割合と合計で、0.2、0.3、0.5、1付近です。

## ステップ27：順列を状態として持つ

**土台：26。変更：割合の移し替えを、順列の二要素の交換へ替える。**

三つの対象0・1・2の本当の順番を知りたいとします。比較結果として「2が0より先」「2が1より先」「0が1より先」を得ました。ただし各比較は10%の確率で逆になるものとします。

候補の順列が比較結果と一致すれば、その観測の起こりやすさは0.9、逆なら0.1です。各比較についてその負の対数を足して、候補の悪さとします。

```cpp
#include "mcmc_estimator_v09.hpp"

int main() {
    struct Order { std::array<int,3> item; };
    using Solver=McmcEstimator<Order>;
    const std::array<std::array<int,2>,3> comparisons{{{2,0},{2,1},{0,1}}}; // 左が先という観測
    auto energy=[comparisons](const Order& s) {
        std::array<int,3> position{};
        for (int i=0; i<3; ++i) position[s.item[i]]=i;
        double e=0;
        for (auto pair:comparisons)
            e-=std::log(position[pair[0]]<position[pair[1]] ? 0.9 : 0.1);
        return e; // 比較が誤る確率を0.1と仮定
    };
    auto move=Solver::symmetric_move([](const Order&, Order& next,
                                      Solver::Rng& rng, const Solver::MoveContext&) {
        const int i=rng.integer(3), j=(i+1+rng.integer(2))%3;
        std::swap(next.item[i],next.item[j]); // 順列を壊さずに候補を作る
        return true;
    });
    auto session=Solver::make_session(std::vector<Order>{Order{{0,1,2}}},energy,move);
    const auto r=session.solve(Solver::Budget::for_steps(20'000));
    if (!r.state) return 1;
    for (int x:r.state->item) std::cout << x << ' ';
    std::cout << '\n';
}
```

`Order::item`は位置順に並べた対象の番号です。初期状態`{0,1,2}`は全対象をちょうど一回ずつ含みます。`position`は逆に、対象番号から位置を引くための配列です。比較行`{u,v}`は、uがvより先だという観測です。

0.1は既知の比較誤り率、0.9は正しく比較される確率です。例では比較誤差が独立だと仮定します。誤り率を0にして不一致の対数を計算するような変更は、そのままではできません。

変更方法の引数や二つの位置の選び方は26と同じです。`std::swap`で二位置の要素を交換します。順列を壊さず、同じ交換をもう一度行えば戻るので対称な提案です。trueは「この候補を評価してよい」という意味で、必ず採用されるという意味ではありません。

出力の2、0、1は、係数値ではなく並び順です。順列を独立な整数変数の列として宣言すると重複や欠落が生じます。そのような構造には、今回のように状態自体で表し、構造を保つ変更を用意できます。

## ステップ28：候補を選ぶ確率が偏る場合

**土台：27の独自提案と21の標本採取。変更：対称でない提案の確率差を補正する。**

三種類の装置があり、成功率はそれぞれ0.2、0.5、0.8だと分かっています。種類の事前確率は三つとも同じです。成功を一回観測したとき、どの種類がどれだけあり得るかを求めます。

最良の種類だけなら成功率0.8の装置ですが、他の種類でも成功は起こります。観測後の割合は成功率に比例し、合計1.5で割った約0.133、0.333、0.533です。

ここで、自分が新しい候補を選ぶ手順は0.6・0.3・0.1と偏っているとします。この選びやすさは観測とは無関係なので、標本の割合へ紛れ込まないよう補正します。

```cpp
#include "mcmc_estimator_v09.hpp"

int main() {
    struct Kind { int id; };
    using Solver=McmcEstimator<Kind>;
    const std::array<double,3> success{0.2,0.5,0.8}; // 各種類の既知の成功率
    const std::array<double,3> q{0.6,0.3,0.1}; // 自分で候補を選ぶ確率。成功率とは別。
    auto energy=[success](const Kind& s) { return -std::log(success[s.id]); };
    // 3種類の事前確率は等しく、成功を1回観測した場合を推定する。
    auto move=Solver::hastings_move([q](const Kind& old, Kind& next,
                                      Solver::Rng& rng, const Solver::MoveContext&) {
        const double u=rng.uniform();
        next.id=u<q[0] ? 0 : u<q[0]+q[1] ? 1 : 2;
        return std::log(q[old.id])-std::log(q[next.id]); // 逆向き/順向きの対数
    });
    auto session=Solver::make_session(std::vector<Kind>{{0}},energy,move);
    const auto r=session.sample(Solver::Budget::for_steps(100'000),[](const Kind& s) {
        std::array<double,3> indicator{};
        indicator[s.id]=1;
        return indicator; // 平均すると各種類の出現割合になる
    });
    if (!r.summary.mean()) return 1;
    for (double probability:*r.summary.mean()) std::cout << probability << ' ';
    std::cout << '\n';
}
```

`success`はモデルの既知の成功率です。`q`は自分が候補を提案する確率です。どちらも三要素のdouble配列ですが、役割は全く異なります。この例のqは全要素が正で、合計1です。候補IDは0・1・2です。

`hastings_move`内の関数は、boolではなく、次の対数比をdoubleで返します。

$$\log q(s\mid t)-\log q(t\mid s)$$

sは現在の候補、tは新候補、$q(t\mid s)$はsにいるときtを提案する確率です。今回の提案は現在の候補に関係なくqから選ぶので、コードは`log(q[old.id])-log(q[next.id])`です。割り算の向きを逆にしないよう注意します。

Sampleでは、候補の目的関数の差と、この提案比から受理確率を作ります。

$$\alpha=\min\left(1,\exp(E(s)-E(t))\frac{q(s\mid t)}{q(t\mid s)}\right)$$

αは新候補へ移る確率です。選びやすい候補が、単にその理由だけで増えすぎることを防ぎます。qが正逆で等しければ比は1になり、26・27の対称な提案に戻ります。

`indicator`は自分の種類だけ1、他は0の配列です。これを標本ごとに平均すれば、各種類の出現割合になります。出力は約0.133、0.333、0.533です。0.6・0.3・0.1に近づけることが目的ではありません。

`hastings_move`も、比が正しいことを検証するものではありません。戻り値が負の無限大なら試行を棄却でき、NaNや正の無限大は返しません。独自提案で標本採取する場合は、必要な候補へ到達できることと、提案の確率が正しく補正されることを利用側で保証します。単純な数値問題では、標準の数値提案を使う方が容易です。

## ステップ29：通常のSessionの組み立てを分けて書く

**土台：02の数値モデルと26のmake_session。変更：開始状態・変更方法を明示して渡す。**

ここからは上級の組み込み方です。通常の数値推定では01〜23のAPIを使えば十分です。自作プログラムに合わせて部品を分けたい場合だけ、次へ進みます。

`make_numeric_session`が受け取っていたParametersには、開始値と許容範囲の二つの役割がありました。それを別々に取り出すと、26で使った汎用の`make_session`へ渡せます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver=McmcEstimator<>;
using Row=Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai=p.real(0,-10,10), bi=p.real(0,-10,10);
    auto predict=[ai,bi](const double& x,const McmcState& s) {
        return s.real[ai]*x+s.real[bi];
    };
    auto model=Solver::make_observation_model(std::vector<Row>{{0,1},{1,3},{2,5},{3,7}},predict);
    std::vector<McmcState> initial{p.initial()}; // 開始状態のコピー
    auto move=Solver::make_numeric_move(p.domain()); // 範囲から数値用の変更方法を作る
    // make_numeric_sessionがまとめて行っていた組み立てを、明示して書く。
    auto session=Solver::make_session(std::move(initial),std::move(model),std::move(move));
    const auto r=session.solve(Solver::Budget::for_steps(30'000));
    if (!r.state) return 1;
    std::cout << r.state->real[ai] << ' ' << r.state->real[bi] << '\n';
}
```

| 新しく明示した部分 | 意味 |
|---|---|
| `p.initial()` | `const McmcState&`。登録した初期値の組。vectorに入れるとコピーされる |
| `p.domain()` | `const Solver::Domain&`。各変数の範囲と数値変更幅の宣言 |
| `make_numeric_move(domain)` | Domainを元に標準の数値変更方法を作る。第2引数のMoveParamは省略時に既定設定 |
| `make_session(initial,model,move)` | 開始状態、観測モデル、変更方法をそれぞれ所有させる |

関数から戻ったSessionを使うときも、渡した元のローカル変数の寿命を気にせずに済む形です。ただし、モデル内のラムダが外部変数を参照キャプチャしている場合、その参照先まで所有するわけではありません。

出力は02と同じ2、1付近です。機能を増やすためだけにこの長い形へ替える必要はありません。ここでは、次の下位APIで何を自分で保持するのかを理解するために分解しています。

## ステップ30：下位APIで実行を管理する

**土台：29。変更：Sessionの代わりにengineを作り、初期化とフェーズを自分で指定する。**

既存プログラムが評価モデルと候補の変更方法を持っていて、それらを推定器の外に置いたまま実行したい場合があります。ここでは、Sessionが代行していた初期化・Search・Warmup・Sampleを順に呼びます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver=McmcEstimator<>;
using Row=Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai=p.real(0,-10,10), bi=p.real(0,-10,10);
    auto predict=[ai,bi](const double& x,const McmcState& s) { return s.real[ai]*x+s.real[bi]; };
    auto model=Solver::make_observation_model(
        std::vector<Row>{{0,1,0.5},{1,3,0.5},{2,5,0.5},{3,7,0.5}},predict);
    auto move=Solver::make_numeric_move(p.domain());
    std::vector<McmcState> initial{p.initial()};
    Solver::Param param;
    param.sample_stride=2;
    Solver engine(param); // Sessionを使わず、評価と変更方法を外で所有する
    auto budget=Solver::Budget::for_steps(30'000);
    engine.reset(initial,model,budget); // 初期評価は遷移数を消費しない
    engine.run(Solver::Phase::Search,model,move,budget);
    const auto best=engine.best();
    if (!best.state) return 1;
    std::cout << best.state->real[ai] << ' ' << best.state->real[bi] << ' ';
    // 下位APIでは準備を自分で行う。param.warmup_stepsによる自動管理はない。
    auto warm=Solver::Budget::for_steps(4'000);
    engine.run(Solver::Phase::Warmup,model,move,warm);
    auto sample_budget=Solver::Budget::for_steps(80'000);
    double sum=0;
    std::uint64_t count=0;
    engine.run(Solver::Phase::Sample,model,move,sample_budget,
        [&](const McmcState& s,double,int) { sum+=model.mean_prediction(4.0,s); ++count; });
    if (count==0) return 1;
    std::cout << sum/static_cast<double>(count) << ' ' << count
              << ' ' << sample_budget.remaining_steps << '\n';
}
```

### Sessionから何が変わったか

`Solver engine(param)`は推定のエンジンだけを作ります。モデルと変更方法は外の変数のままです。`param`は通常のSessionへ渡したものと同じ型で、今回は`sample_stride=2`以外は既定値です。

| 呼び出し | 各引数と役割 |
|---|---|
| `reset(initial,model,budget)` | 空でない開始候補列をコピーし、modelで初期評価する。既存の探索履歴は初期化 |
| `run(Phase::Search,model,move,budget)` | Searchを行う。初期同期が途中なら先に続行する |
| `run(Phase::Warmup,model,move,warm)` | 温度1で準備する。数値変更幅の適応は可能、標本はまだ記録しない |
| `run(Phase::Sample,model,move,sample_budget,emit)` | 温度1の標本採取。各記録でemitを呼ぶ |
| `engine.best()` | 現在のモデルで評価済みの最良候補のビュー |

resetの開始候補引数は`std::span<const State>`として受け取られ、今回のvectorから変換されます。engineは内容をコピーするので、元のvectorを後から変更しても開始候補が自動で変わるわけではありません。

### Budgetが参照渡しになる

下位APIのBudgetは参照で受け取られ、`remaining_steps`が実行によって減ります。コードでは同じbudgetをresetとrunへ渡し、二つを合わせて30,000試行までにしています。初期評価は試行数を消費しませんが、時間予算を指定したときの経過時間には含まれます。

Sessionの`solve`や`sample`はBudgetを値として受け取るため、元のBudgetの残り試行数を減らしません。ここは通常APIとの重要な違いです。

### 準備と出力も自分で管理する

下位APIは`param.warmup_steps`を使って自動で準備を挟みません。今回の4,000試行のWarmupは利用側の判断です。時間で途中終了する構成に変える場合は、完了した準備量を自分で追跡し、途中なのにSampleへ切り替えないようにします。

emitの引数は22と同じ状態・energy・鎖番号です。今回は予測値の和と件数だけを集めています。出力は係数2・1付近、入力4の平均9付近、標本数40,000、残り試行数0です。最良候補のポインタは後続のrunの後には使いません。

下位APIにはSessionの「標本採取用と明示された提案だけsampleを公開する」制限がありません。自分でフェーズを選べる分、提案の正しさと管理の責任も増えます。

## ステップ31：下位APIで観測と外部データを変更する

**土台：30。変更：観測の追加・置換と、外部の既知値の変更を通知する。**

Sessionの11〜13に相当する操作を、外にモデルを置いたまま行います。予測は$ax+b+c$で、cは既知の補正値です。a・bは未知、cは推定対象ではありません。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver=McmcEstimator<>;
using Row=Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai=p.real(0,-10,10), bi=p.real(0,-10,10);
    double correction=0; // 外部の既知補正値。mainの終わりまで生存する。
    auto predict=[ai,bi,&correction](const double& x,const McmcState& s) {
        return s.real[ai]*x+s.real[bi]+correction;
    };
    auto model=Solver::make_observation_model(std::vector<Row>{{0,1},{1,3},{2,5}},predict);
    auto move=Solver::make_numeric_move(p.domain());
    std::vector<McmcState> initial{p.initial()};
    Solver engine;
    auto budget=Solver::Budget::for_steps(20'000);
    engine.reset(initial,model,budget);
    engine.run(Solver::Phase::Search,model,move,budget);
    budget=Solver::Budget::for_steps(20'000);
    engine.observe(model,std::vector<Row>{{3,7}},budget); // 新規行は一度だけ追加
    engine.run(Solver::Phase::Search,model,move,budget);
    budget=Solver::Budget::for_steps(30'000);
    engine.replace_observations(model,std::vector<Row>{{0,2},{1,4},{2,6},{3,8}},budget);
    engine.run(Solver::Phase::Search,model,move,budget); // ここではa=2,b=2
    correction=1;
    budget=Solver::Budget::for_steps(30'000);
    engine.refresh(model,budget); // 外部変更を通知し、古い評価値を無効化
    engine.run(Solver::Phase::Search,model,move,budget); // 今度はa=2,b=1
    const auto best=engine.best();
    if (!best.state) return 1;
    std::cout << best.state->real[ai] << ' ' << best.state->real[bi]
              << ' ' << model.size() << '\n';
}
```

| 呼び出し | 引数と意味 |
|---|---|
| `observe(model,batch,budget)` | modelへ新しい行だけを一度追加し、その予算で保持候補の評価を更新する |
| `replace_observations(model,data,budget)` | modelの全行をdataに置き換え、全体を再評価する |
| `refresh(model,budget)` | 外部データを変えたことを知らせ、旧評価を無効化して現在のモデルで評価し直す |

各メソッドはReportを返します。更新の登録だけでなく、渡した予算で再評価も行う点が、Sessionの追加・更新メソッドと違います。更新後の探索は続く`run`で行います。

最初に$x=3,y=7$だけを追加します。次に全観測を$2x+2$のデータへ置き換え、その時点ではa=2、b=2付近になります。最後に既知補正cを1にすると、同じ測定値を説明する未知のbは1付近になります。最終出力は2、1付近、観測数4です。

`[&correction]`は外部変数の参照です。この例ではmainが終わるまで参照先が生存します。外側で値を変えただけでは、engineは変更を知りません。必ずrefreshで古い評価を無効化します。通常のSession利用では、13のContextで変更を管理する方が簡単です。

追加・置換・refreshが途中で時間切れになった場合は、現在のモデルを渡すrunで同期の続きを行います。追加行をもう一度observeへ渡すと重複し、refreshを繰り返すと再評価を始め直すので注意します。

## ステップ32：目的関数の更新を増分だけで伝える

**土台：31。変更：新しい目的関数と古い目的関数の差を直接渡す。**

最後に、観測モデルのobserveを使わず、独自の全体評価に観測一件を加える場合を扱います。候補a自体は同じでも、評価対象が増えることによって全体の悪さが変わります。

元の目的関数を$E_{old}(a)$、追加観測の入力・出力を$x_{new},y_{new}$とすると、今回の増分は次です。

$$\Delta(a)=\frac12(y_{new}-ax_{new})^2,\qquad E_{new}(a)=E_{old}(a)+\Delta(a)$$

Δは同じ候補aにおける新旧の目的関数の差です。今回は追加一行の二乗誤差ですが、独自モデルで正確な差を計算できる場合にも使えます。

```cpp
#include "mcmc_estimator_v09.hpp"
using Solver=McmcEstimator<>;
using Row=Solver::Observation<double>;

int main() {
    Solver::Parameters p;
    const auto ai=p.real(0,-10,10);
    std::vector<Row> rows{{1,2},{2,4}};
    auto energy=[ai,&rows](const McmcState& s) {
        double e=0;
        for (const auto& row:rows) {
            const double r=row.value-s.real[ai]*row.input;
            e+=0.5*r*r;
        }
        return e;
    };
    auto move=Solver::make_numeric_move(p.domain());
    std::vector<McmcState> initial{p.initial()};
    Solver engine;
    auto budget=Solver::Budget::for_steps(20'000);
    engine.reset(initial,energy,budget);
    const auto ready=engine.run(Solver::Phase::Search,energy,move,budget);
    if (!ready.synchronized || !ready.has_estimate) return 1;
    const Row extra{3,6};
    rows.push_back(extra); // 以降energyは新しい全体評価を返す
    auto delta=[ai,extra](const McmcState& s) {
        const double r=extra.value-s.real[ai]*extra.input;
        return 0.5*r*r; // 新しい目的関数−古い目的関数、という増分だけ
    };
    budget=Solver::Budget::for_steps(20'000);
    engine.append_energy(delta,budget);
    // 中断があってもdeltaを再登録せず、新しい全体評価で続行する。
    engine.run(Solver::Phase::Search,energy,move,budget);
    const auto best=engine.best();
    if (!best.state) return 1;
    std::cout << best.state->real[ai] << ' ' << best.energy << ' '
              << std::abs(best.energy-energy(*best.state)) << '\n';
}
```

まず全候補の評価を揃え、有限候補があることをReportで確認しています。`append_energy`は、その状態を前提とする下位APIです。途中で同期できていない状態に対して重ねて呼ぶものではありません。

`rows.push_back(extra)`で、外側の全体評価は新しい観測を含むようになります。その直後に`append_energy(delta,budget)`を一度呼び、保持候補の旧評価へ正確な増分を加えます。次のrunに渡すのは、deltaではなく更新後の全体評価energyです。追加を二回行ったり、observeとappend_energyの両方で同じ一行を登録したりはしません。

増分の同期が時間切れになった場合も、同じdeltaを再登録せず、新しい全体評価を渡すrunで続きを行います。出力はa=2付近、最良energyが0付近、保存されたenergyと全体を計算し直した値の差が0付近です。

### 「差分」という言葉の三つの意味を整理する

| 何が変わったか | 使用箇所 | 渡す・返すもの |
|---|---|---|
| 観測が増えた | 11の`add_observations`、31の`observe` | 新しい観測行だけ。評価の更新はライブラリへ任せる |
| 同じ目的関数で候補が変わった | 25の`candidate` | 新候補の目的関数全体。増分だけではない |
| 同じ候補に対する目的関数が変わった | 32の`append_energy` | 新目的関数−旧目的関数、という増分だけ |

最初から差分評価まで実装する必要はありません。まず普通の全評価で正しく動かし、評価が計算時間の大きな部分を占める場合にだけ、どの意味の差分を利用できるか検討します。

## 巻末A：目的から学習したAPIへ戻る

この表は最初に暗記するためのものではありません。一度動かした例を探し直すための索引です。特殊な組み合わせを実装するときは、入力・損失・状態・変更方法の前提が両立しているかを確認します。

| やりたいこと | 主なAPI・表現 | ステップ |
|---|---|---|
| 通常の実数パラメータ推定 | `Parameters::real`、`Observation`、`make_observation_model`、`make_numeric_session`、`solve` | 01〜04 |
| 三入力の一次関数 | `Observation<std::array<double,3>>` | 03 |
| 外れ値、既知の想定や制約 | `Huber`、事前項、正の無限大 | 05・06 |
| 整数・カテゴリ・混合した未知変数 | `integer`、`category`、`state.discrete` | 07・08 |
| 時間指定、分割実行、実行情報 | `Budget`、`Param`、`Report` | 09・10 |
| 観測を足す・訂正する・外す | `add_observations`、`replace_observations`、`retain_last` | 11・12 |
| 共有する既知の地図を編集する | `make_context_model`、`update_model`、`context` | 13 |
| 保存済み観測と予測を再利用する | `model`、`size`、`observations`、`prediction` | 12〜14 |
| 共通σ、独自損失、正の変数 | `gaussian_loss`、loss関数、対数変換 | 14〜16 |
| 成功率・件数の予測 | `BernoulliLogit`、`PoissonLogMean`、`mean_prediction` | 17・18 |
| 複数出力・欠測・再現シミュレーション | 独自Row、独自loss、任意のpredict | 19・20 |
| 候補の不確かさ、予測の平均と分散 | `sample`、`SampleResult::summary` | 21 |
| 標本を自分で保存・累積する | `sample_each`、`Moments`、`revision` | 22 |
| 初期値の変更、複数鎖、コピーしない結果 | `restart`、`MoveParam`、`solve_view`、`best` | 23 |
| 評価全体を渡す、候補の差分評価 | 関数モデル、`operator()`、`candidate` | 24・25 |
| 合計1の割合や順列 | 独自State、`make_session`、`symmetric_move` | 26・27 |
| 非対称な候補の標本採取 | `hastings_move` | 28 |
| 部品を分けて構築する | `Parameters::initial`、`domain`、`make_numeric_move` | 29 |
| 下位APIで所有や更新を自分で管理する | コンストラクタ、`reset`、`run`、`refresh`、`observe`、`append_energy` | 30〜32 |

例えば「整数係数で共通σも未知」なら、07の四係数をdiscreteへ入れ、15のσだけをrealへ入れます。「種類も未知で観測を毎ターン足す」なら、08のSessionを11のようにループ外へ持ちます。組み合わせるために全例を連結するのではなく、必要な宣言と呼び出しだけを移します。

## 巻末B：設定値の参照表

ここまでの例で省略した設定も含め、現行の公開設定をまとめます。設定は作成時に渡します。作成後に元の`param`や`p`を書き換えても、既存Sessionの設定が自動で変わるわけではありません。

### 変数の登録と範囲

| 指定 | 引数の型・既定値 | 契約と選び方 |
|---|---|---|
| `real(initial,lower,upper,scale)` | 全てdouble。scaleのみ既定0 | 初期値は有限で範囲内。scale=0なら有限区間の幅の10%、固定変数では1を自動使用。明示時は正の有限値 |
| `integer(initial,lower,upper)` | 全て`std::int64_t`、省略不可 | 両端を含む整数範囲。絶対値・幅は10億以下を想定 |
| `category(initial,count)` | `std::int64_t`、int、省略不可 | countは正、初期IDは0以上count未満。順序の意味を持たない |
| `Domain::real` | `std::vector<Domain::Real>` | 各要素がlower・upper・scaleをdoubleで保持する。通常はParametersから生成する |
| `Domain::discrete` | `std::vector<Domain::Discrete>` | 各要素がint64のlower・upperとboolのcategoricalを保持。categoricalの既定値はfalse |

無限の端を持つ実数範囲を使う場合、scaleを省略できません。初期値は有限のままです。標本採取では、その範囲の確率密度の合計が有限になるモデルが必要です。観測に効かない無制限の変数を一様な事前項0で置くなど、正規化できない設定は避けます。

変数を固定するには下限と上限を等しくします。状態の配列から要素が消えるわけではありません。カテゴリ一種類も同様に固定です。標準の数値提案を使う状態は、宣言と配列の大きさ・添字の意味を一致させます。

### Param

| フィールド | 型・既定値 | 使う場面・値の選び方 |
|---|---|---|
| `seed` | `std::uint64_t = 1` | 再現確認用。別のseedでも挙動を確認すると開始条件への偏りを見つけやすい |
| `sample_stride` | `std::uint64_t = 1` | 正にする。大きくすると標本処理量を減らせるが、独立性は保証しない |
| `start_temperature` | `double = 1` | Searchの開始温度。正の有限値。初めに広く動きたいときに調整するが、目的関数の尺度にも依存 |
| `end_temperature` | `double = 0.001` | Searchの終端温度。正の有限値。最後にどれくらい悪化を許すかを調整 |
| `cooling_steps` | `std::uint64_t = 0` | 0なら最初のSearchの予算から決定。試行数で分割するなら総予定を指定 |
| `cooling_deadline` | `Clock::time_point = Clock::time_point::max()` | 冷却の絶対期限。明示すると冷却は時刻基準になり、cooling_stepsより優先する |
| `observation_check_interval` | `std::size_t = 32` | 観測モデルの評価項の時計確認間隔。正にする。一項が重いなら1などへ下げる |
| `warmup_steps` | `std::optional<std::uint64_t> = std::nullopt` | Sessionの標本採取前の準備量。自動量は21参照。0は意図的に準備を省く指定 |

`cooling_deadline`は探索を自動終了させる期限ではありません。停止はBudgetが担当します。また、温度はWarmup/Sampleでは1です。Searchの温度を変えても、Sampleの対象分布の温度が変わる設定ではありません。

### MoveParam

| フィールド | 型・既定値 | 意味 |
|---|---|---|
| `adapt` | `bool = true` | Search/Warmupで数値変数の変更幅を適応する。Sampleでは適応しない |
| `difference_probability` | `double = 0` | Warmup/Sampleで他の二鎖の差を使う確率。実数の可動変数と三鎖以上が必要 |
| `cauchy` | `bool = true` | Searchで通常より遠い変更も出やすくする。Sampleの標本分布をこの設定で変えるものではない |
| `stretch_probability` | `double = 0.3` | Warmup/Sampleで他の一鎖を基準に実数を伸縮する確率。二鎖以上が必要 |

二つの確率は非負で、和を1以下にします。必要な鎖や可動実数がないと、その集団操作は実行されず通常の座標変更へ進みます。まず既定設定で動かし、計測して問題に適した設定を選びます。小さい教材で最もよい値が、そのままAHC本番でも最良とは限りません。

### 予算と実行情報

`Budget`は`deadline`と`remaining_steps`を持ちます。既定はそれぞれ無期限相当の最大時刻と`UINT64_MAX`です。`time_left()`は時刻だけを調べ、`available()`は時刻と残り試行数の両方を調べます。これらは推定結果や収束の確認ではありません。

| Reportのフィールド | 型・初期値 | 読み方 |
|---|---|---|
| `stop` | `Stop::Budget` | 通常の予算終了。全初期候補の評価を終えても有限候補がないと`Stop::NoFiniteState` |
| `steps` | `std::uint64_t = 0` | 完了した試行数。棄却した試行も含み、初期評価・再評価は含まない |
| `accepted` | 同上 | 採用した候補変更の数。最良更新数ではない |
| `evaluations` | 同上 | 完了した状態評価数。初期評価・再評価も含む |
| `terms` | 同上 | 計算した評価項数。観測モデルでは事前項も一項 |
| `emitted` | 同上 | 今回出力した標本数。準備中には出力しない |
| `elapsed_us` | `std::int64_t = 0` | 今回の呼び出しにかかったマイクロ秒 |
| `synchronized` | `bool = false` | 保持中の各候補の評価が現在のモデルへ揃ったか |
| `has_estimate` | `bool = false` | 現在のモデルで評価済みの有限候補があるか |

全候補の再評価が終わる前でも、一つ評価できれば`has_estimate`がtrueになることがあります。stopがBudgetだから有効候補がある、とは限りません。未評価なら`Result::state`は空、energyは正の無限大です。`SampleResult`にはsummary・report・revision・warmup_remainingがあり、Sessionの`warmup_remaining()`からも現在の準備残量を読めます。

## 巻末C：独自提案を作るときだけ読む補助API

26〜28の変更関数が受け取る`MoveContext`には次があります。これらを読むだけで、実行中にモデルやSessionを編集しません。

| フィールド | 型 | 意味 |
|---|---|---|
| `phase` | `Phase` | Search・Warmup・Sampleのどこか |
| `temperature` | double | 現在の温度。Warmup/Sampleは1 |
| `chain` | `std::size_t` | 今変更する鎖の0始まり番号 |
| `population` | `std::span<const State>` | 他の鎖も含む候補列の読み取り専用ビュー。コールバック中だけ参照 |
| `revision` | `std::uint64_t` | 目的関数や再初期化に対応する改訂番号 |

変更方法を関数オブジェクトにすると、任意で`feedback(bool accepted,const MoveContext&)`を用意できます。試行の完了時に呼ばれ、採否を見て診断や変更幅の調整に使えます。ただしSample中に勝手に提案分布を変える適応はしません。本文の例ではこの追加フックは必要ありません。

Sessionで独自提案の標本採取を使うには、`symmetric_move`・`hastings_move`を通すか、関数オブジェクトに`static constexpr bool sampling_safe=true;`を宣言します。いずれも正しさを自動で証明する仕組みではありません。無印の提案は、通常のSessionではSearch用として使います。直接の提案が返すのも対数提案比であり、boolをそのまま返す契約ではありません。

| Rngのメソッド | 引数・戻り値 |
|---|---|
| `Rng(seed)` | 自分で構築するときの`std::uint64_t`の種。既定1。通常の提案では渡されたRngを使う |
| `uniform()` | double、0以上1未満 |
| `integer(n)` | int、0以上n未満。nは正のint、最大$2^{31}-1$ |
| `normal()` | double、平均0・標準偏差1の正規乱数 |
| `next()` | `std::uint64_t`の乱数 |
| `log_uniform()` | double、0より大きく1以下の一様乱数の自然対数 |

## 巻末D：使い始める前のチェック

- **予測できる形を自分で決める。** 入力と候補から観測を再現する関数は利用側で用意します。未知の数式を何も指定せず発見するライブラリではありません。
- **観測から未知数を区別できるか。** 同じ入力ばかり、係数に影響しない入力、常に同じ機械だけが遅い観測などでは、複数の候補が同じ出力になります。
- **最良候補と標本を混同しない。** Searchの履歴を事後分布の標本として扱わず、sampleかsample_eachを使います。離散変数の平均が有効な状態になるとも限りません。
- **損失の出力形式を揃える。** Gaussian/Huberは表示値、BernoulliLogitはlogit、PoissonLogMeanはlog(平均件数)を受け取ります。独自損失の重みは自分で処理します。
- **正規化しない合計を意識する。** 観測損失の和を件数で自動的に割りません。平均誤差への置換は、事前項との相対的な強さや標本の分布も変えます。
- **空の結果を読まない。** 初期化や再評価が進まなければ候補はなく、準備だけで標本0件にもなります。optional・ポインタを確認してから値を使います。
- **同じ状態なら同じ評価にする。** 予測や評価のたびに乱数を引いたり、現在時刻で規則を変えたりしません。評価値と中間計算はdoubleの範囲に収め、NaN・負の無限大は禁止です。
- **許容範囲を揃える。** 初期値は範囲内に置き、少なくとも一つ有限の初期候補を用意します。変数の数や意味・上下限を変えたら、古いSessionのまま使いません。
- **所有と参照を区別する。** Sessionが所有するモデルでも、参照キャプチャ先まで所有はしません。終了した関数のローカル変数への参照を残しません。
- **更新を通知する。** 通常はSessionの追加・置換・update_model、下位APIではobserve・refreshなどを使います。更新後は古いenergyや標本集計をそのまま混ぜません。
- **既知情報の訂正と実際の時間変化を区別する。** 過去の環境が違うなら、当時の情報を観測の値や不変の識別子として保持します。現在のContextで過去を勝手に書き換えません。
- **コールバック中に再入しない。** 単スレッドの利用を前提に、solveやsampleの実行中に同じSessionの実行・変更メソッドを再び呼びません。予測など読み取り専用の計算は例のように使えます。
- **時間制限には余裕を取る。** ユーザー関数、候補のコピー、標本処理、solve後の残差集計、入出力を途中で強制停止する機能はありません。観測モデル以外の全体評価一回も原子的な計算です。
- **任意の機能の組み合わせを自動で補完するわけではない。** 独自モデルには観測モデル専用のメソッドはありません。独自損失にmeanがなければmean_predictionは使えません。Contextを使う予測の形は`(context,input,state)`、事前項は`(context,state)`ですが、損失にContext引数は増えません。
- **設定変更には再構築を使う。** Param・MoveParam、予測・損失の型、変数Domainを作成後に変更する専用setterはありません。後から変える既知情報はContextまたは独自モデルの公開データとして設計します。

## 巻末E：コード例の検証と配布物

本書の32個のC++コードブロックと、ZIP内の32個の個別ソースは一致します。各ソースはヘッダをincludeする独立したプログラムです。この別冊を単独で読み進められるよう、必要な例を全て同梱しています。

GCC 13.3.0のC++20で、通常のassert有効ビルド、`NDEBUG`のリリースビルド、AddressSanitizerとUndefinedBehaviorSanitizerのビルドを行い、全32例を各設定で実行しました。係数・種類ID・整数値・更新後の値・標本数・予測の平均や分散・提案確率の補正結果を検査しています。時間予算の09は、環境で探索量が変わるため、時間内に必ず一定精度まで到達するという検査にはしていません。

LeakSanitizerは実行環境の制約で無効にしており、リーク検査は含みません。GCC 12.2そのものでは実行していないため、その環境での確認を代替するものではありません。数値検証はこれらの例の動作確認であり、全入力での収束やコンテストのスコアの保証ではありません。

| 配布ファイル | 内容 |
|---|---|
| `mcmc_estimator_step_by_step_guide_v01.md` | この別冊ガイド |
| `mcmc_estimator_v09.hpp` | 対応するライブラリヘッダ |
| `examples/mcmc_step_01_v01.cpp`〜`mcmc_step_32_v01.cpp` | 各段階の実行可能なコード |
| `verify_mcmc_steps_v01.py` | コンパイル・実行・数値検査 |
| `check_mcmc_step_guide_v01.py` | 本文とコードの一致、数式区切り、対応ヘッダの検査 |
| `results_v01/` | 検証結果とコンパイル・実行ログ |
| `mcmc_steps_manifest_v01.json` | 同梱ファイルの一覧とSHA-256 |

ZIP展開後のルートで、Python 3とGCCを使って検証を再実行できます。

```bash
python3 check_mcmc_step_guide_v01.py
python3 verify_mcmc_steps_v01.py --mode debug --jobs 2
python3 verify_mcmc_steps_v01.py --mode release --jobs 2
python3 verify_mcmc_steps_v01.py --mode sanitize --jobs 2
```

検証プログラムは本文を自動更新しません。実行バイナリはZIPへ含めておらず、必要な例だけコンパイルできます。AHCへ取り込むときは、例のmain内の推定処理を自分の入出力へ組み込み、未知変数の範囲、測定精度、計算予算を問題に合わせて指定します。
