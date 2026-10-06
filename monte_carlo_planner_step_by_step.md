# MonteCarloPlanner ステップバイステップガイド

対象ヘッダ：`monte_carlo_planner_v07.hpp`。ガイド第6版。C++20/23。

このガイドは、確率・統計や探索法を専門に学んだことがなくても、動く例から使い方を理解できるように構成しています。必要な数学は、高校で扱う平均、確率、平方根、指数程度です。C++の構造体、関数、参照、`vector`の基本は知っているものとします。`optional`やライブラリ特有の用語は登場時に説明します。

本文のコードはすべて独立したプログラムです。前の例の定義を別途貼り付ける必要はありません。同梱のヘッダを用意すれば、そのままコンパイルできます。コードを短く示すための省略記号、未実装の`simulate`、疑似コードは使いません。

## 1. 何を解くライブラリか

### 1.1 「今どの行動を選ぶか」を、将来の得点まで考えて決める

たとえば、次のような問題を考えます。

- 複数の配送経路から一つ選ぶ。ただし渋滞によって所要時間が変わる。
- 今日の売上を増やすか、設備を改良して明日以降の売上を増やすかを選ぶ。
- 中身が見えない箱を選ぶ前に、費用を払って中身を調べるか判断する。

共通するのは、**行動の良さを、その場の得点だけでは決められない**ことです。将来の出来事や、その後に選ぶ行動も考える必要があります。

本ライブラリに利用者が与えるのは、現在の情報、選べる行動、行動した後に状態と得点がどう変わるか、あと何手を考えるかです。受け取るのは、主に**今の一手の推薦**です。将来の全行動列や、あらゆる状態に対する完成済みの方策を返すものではありません。

「方策」とは、見えている情報に応じて次の行動を決めるルールです。たとえば「在庫が少なければ補充し、多ければ販売する」というルールも方策です。最初から固定した行動列とは違い、途中の観測に応じて判断を変えられます。

### 1.2 目的関数

まず、一回の将来の展開における総合得点を次で表します。

$$G=\sum_ {t=0}^{\tau-1}\gamma^t r_ {t}+\gamma^{\tau}F(S_ {\tau},I_ {\tau},z)$$

そして、最初に行動$a$を選んだ場合の期待得点を考えます。

$$Q_ {\pi}(a\mid I_ {0})=\mathbb{E}[G\mid I_ {0},a_ {0}=a,\pi]$$

その期待得点が最も大きい行動を選ぶのが目的です。

$$a^{\ast}\in\arg\max_ {a\in A(I_ {0})}Q_ {\pi}(a\mid I_ {0})$$

式に出てくる記号をすべて説明します。

| 記号 | 意味 |
|---|---|
| $I_ {0}$ | 現在利用できる情報。すべて見える問題なら現在の状態そのもの |
| $A(I_ {0})$ | 現在選べる行動の集合 |
| $a$ | 比較している最初の行動 |
| $a_ {0}$ | 実際に最初に選ぶ行動を表す変数 |
| $a^{\ast}$ | 期待得点が最大になる行動の一つ。複数あればどれか一つ |
| $\pi$ | 最初の一手の後、観測できる情報を使って行動を選ぶ方策 |
| $H$ | 今回考える残り手数の上限。APIの`horizon` |
| $\tau$ | 今回の展開で実際に進んだ手数。終端に早く着けば$H$より小さい |
| $t$ | 手の番号。最初を0として、$\tau-1$まで数える |
| $r_ {t}$ | 第$t$手で新たに得る報酬。APIの`step`の戻り値 |
| $\gamma$ | 一手先の得点に掛ける割引率。APIの`discount`。0以上1以下 |
| $S_ {\tau}$ | 最後に到達した状態 |
| $I_ {\tau}$ | 最後の時点で利用できる情報。完全観測なら状態と同じ |
| $z$ | 終了理由。問題上の終端か、残り手数の上限に達したか |
| $F(S_ {\tau},I_ {\tau},z)$ | 最後に一度だけ加える得点。APIの`finish_value`。情報や理由に依存しない形も使える。省略すれば0 |
| $G$ | ある一回の展開で得た、割引後の総合得点 |
| $\mathbb{E}$ | 期待値。各結果を、その起こりやすさで重み付けした平均 |
| $Q_ {\pi}$ | 最初の行動と、その後の方策を含めた期待得点 |
| $\arg\max$ | 関数値を最大にする「行動」を選ぶ記号。最大得点そのものではない |

`horizon=0`または現在がすでに終端なら、今選ぶ行動はありません。その場合、ライブラリは初期状態の価値を計算して返すのではなく、推薦なしを返します。

たとえば2手で報酬3、5を得て、最後のボーナスが2なら、`discount=1`で総合得点は$3+5+2=10$です。`discount=0.9`なら$3+0.9\times5+0.9^2\times2=9.12$です。最後のボーナスにも、そこへ到達するまでの割引が掛かります。

期待値の例として、80%の確率で10点、20%で0点なら期待得点は$0.8\times10+0.2\times0=8$点です。毎回必ず8点になる、という意味ではありません。

本ライブラリが目指すのは、この期待得点が高い行動です。その後の方策の良し悪しによっても比較結果は変わります。任意の問題について、すべての将来方策の中で最適な方策と行動を厳密に求める保証はありません。

### 1.3 利用者が表現するもの

| 利用者が決めること | 例 |
|---|---|
| 状態・観測情報 | 頂点、在庫、残り資源、これまでに得た観測 |
| 行動 | 移動先、補充量、設備改良の有無 |
| 遷移 | 行動後の状態と、一手分の報酬 |
| 不確実性 | 需要、故障、隠れたパラメータの仮定 |
| 目的 | 報酬和、最終スコア、費用の符号反転 |
| 計算予算 | マイクロ秒、評価回数、遷移回数、絶対締切 |

合法性は、合法な行動だけを渡す、または生成することで表します。予算超過や衝突などの条件をライブラリが問題文から読み取ることはありません。目的も期待値が基本です。最悪値、分散、破綻確率などを重視したい場合は、その目的に合う評価モデルや別の手法を検討します。特に、各試行の値を単に変換して平均したものが、元の問題のCVaRなどと一致するとは限りません。

## 2. 類似ソルバーと使い分け

### 2.1 先に厳密解で解けないか考える

次の条件なら、本ライブラリより厳密解を優先できることがあります。厳密解が計算時間内に求まるなら、推定のばらつきや探索不足を避けられます。

| 条件 | 候補となる厳密解 | 判断の目安 |
|---|---|---|
| 一手だけで、結果の種類が少なく、確率が分かる | 各候補の期待値を全結果について直接計算 | 期待値の式が書けるなら、試行を繰り返す必要がない |
| 状態と行動が少なく、遷移確率が既知で、手数が有限 | 状態と残り手数を用いた動的計画法 | 状態数を実際に数えて、全状態を扱えるか確認する |
| 確率変動のない、非負コストの単純な最短路 | Dijkstra法 | 非負コストの加算以外に在庫・時間帯等の状態が絡むなら、そのまま使えるとは限らない |
| 遷移がDAGをなし、状態を列挙できる | DAG上の動的計画法 | 確率的な遷移でも、全遷移と確率を列挙できれば期待値を計算できる場合がある |
| 手数・分岐が非常に小さい | 全列挙 | 列挙数が予算内なら、探索を近似する必要がない |

小さい有限状態問題での厳密計画と、大きい問題での近似計画という区別は、[Algorithms for Decision Making](https://algorithmsbook.com/decisionmaking/)の第7章・第9章でも扱われています。ここでの選択基準は、アルゴリズム名よりも、状態・遷移・結果を時間内に列挙できるかです。

### 2.2 似た道具との役割の違い

| 道具 | 得意な役割 | 本ライブラリとの使い分け |
|---|---|---|
| モンテカルロ評価 | 指定した一つの候補を、確率的な展開で採点する | その採点関数を`mc_choose`に渡し、複数候補の比較を任せられる |
| 多腕バンディット型の候補選別 | 有望な候補と不確かな候補へ評価を配る | 本ライブラリの一手問題に近い。ただし目的は計算後の推薦であり、実環境で探索しながら報酬を稼ぐ仕組みそのものではない |
| MCTS | 将来の判断も木として保存して改善する | 本ライブラリは`tree=true`で、この系統の探索を使う。MCTSと別物というより、軽量な木探索も選べる計画ライブラリ |
| ビームサーチ | 限られた幅で、良さそうな部分解・行動列を伸ばす | 確定的な遷移と良い途中評価があるなら比較候補。確率的な結果の平均比較が中心なら本ライブラリが導入しやすい |
| 焼きなまし・局所探索 | 完成した配置・順序・割当などを少しずつ改善する | まず完成解を作って直接スコアを計算できる問題に向く。その外側で複数方策を比較するなどの併用も可能 |
| 粒子フィルター・MCMC推定 | 観測から未知状態・パラメータの分布を推定する | 推定された分布を`sample_root`で使う。本ライブラリは推定器を内蔵しない |
| POMCPなどの部分観測計画 | 観測履歴・信念状態を使って将来の行動を計画する | 本ライブラリも部分観測を扱えるが、信念更新を自動実装する完全なPOMCPではない |

[POMCPの原論文](https://papers.nips.cc/paper_files/paper/2010/hash/edfbe1afcf9246bb0d40eb4d8027d90f-Abstract.html)では、信念状態のモンテカルロ更新と木探索を組み合わせています。本ライブラリでは、どの情報を保持し、観測後に分布をどう更新するかを利用者が定義します。

導入の目安は、**候補の期待得点を直接計算するのは難しいが、一回の展開ならプログラムで再現できる**ことです。結果がほぼ確定していて採点も軽いなら、直接比較や問題専用ソルバーの方が簡単なことがあります。

## 3. 準備と読み進め方

`monte_carlo_planner_v07.hpp`と、以下の例を保存した`main.cpp`を同じディレクトリへ置きます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic main.cpp -o main
./main
```

`-std=c++23`でも構いません。ヘッダは`bits/stdc++.h`を使うため、例はGCC環境を想定します。ACLや追加の外部ライブラリは不要です。ヘッダ末尾の単体テストは、通常の`#include`では組み込まれません。

以下の「例01〜例31」は、同梱ZIPの`examples/example01.cpp`〜`examples/example31.cpp`に対応します。ZIPのディレクトリ構成なら、たとえば`g++ -std=c++20 -O2 -I. examples/example01.cpp -o example01`でコンパイルできます。

| 段階 | 主な内容 |
|---|---|
| 例01〜04 | 単発比較、最小化、探索器、結果の読み方 |
| 例05〜11 | 複数ターン、終了時得点、継続方策、浅い打ち切り |
| 例12〜15 | 時間制限、途中再開、候補追加、初期化 |
| 例16〜21 | 木探索、キー、再利用、再計画、大量候補、保存上限 |
| 例22〜24 | 任意のCRN、出来事ごとの乱数、ユーザー指定URBG |
| 例25〜26 | 部分観測と、推定結果を使った再計画 |
| 例27 | テスト用の時計の差し替え。通常利用では読み飛ばしてよい |
| 例28〜29 | 指定RNGと部分観測の組み合わせ、rolloutまでのCRN共有の確認 |
| 例30〜31 | 部分観測で、仮定した真の状態と観測情報を両方使って採点 |
| 付録 | API一覧と、実際の実装アルゴリズム |

時間制限を使う例は、実行環境や負荷によって評価回数と推薦が変わり得ます。固定回数の例も、異なる標準ライブラリでの乱数分布の実装差や浮動小数点差まで同一になる保証はありません。

各例の`max_simulations`に出てくる500、1000、2000、3000、5000は、短時間で違いを観察するための試行予算です。問題の報酬や確率を定義する定数ではなく、実問題での推奨回数でもありません。後半の例で2000から3000などへ増やしても、変わるのは計算量の予算です。

## 4. 最小コード：一回の採点関数だけで候補を選ぶ（例01）

三つの候補0、1、2を比較します。基本得点は10、12、11で、実際の得点は上下に2未満の幅で変動します。各候補の期待得点は、それぞれ10、12、11です。

```cpp
#include "monte_carlo_planner_v07.hpp"

int main() {
    std::array<int, 3> candidates{0, 1, 2};
    std::array<double, 3> base{10, 12, 11};
    auto action = mc_choose(candidates,
        [&](int a, MonteCarloNoise& noise) {
            return base[a] + 4 * (noise.uniform01() - 0.5);
        }, 2000);
    if (action) std::cout << *action << '\n';
}
```

### コードを順に読む

`candidates`は選べる候補です。ここでは整数の番号ですが、コピー可能な構造体でも構いません。`base[a]`は候補`a`の基本得点です。

`mc_choose`の二つ目の引数は、「候補を一回だけ評価する関数」です。`[&]`は外側の`base`などを参照して使うというC++の指定です。引数`a`が今回評価する候補、`noise`がこの一回の評価に使う乱数です。

`noise.uniform01()`は0以上1未満の値を返します。そこから0.5を引き、4を掛けると、-2以上2未満の変動になります。関数は**今回一回分の得点**を返します。ここで何千回も平均する必要はありません。

最後の`2000`は、比較に使う時間をマイクロ秒で指定しています。2000マイクロ秒は2ミリ秒です。候補取り込みも、この時間の計測対象に入ります。ただし処理途中を強制停止する締切ではありません。時間管理の詳細は例12で説明します。

戻り値は`std::optional<int>`です。`optional`は「値があるかもしれないし、ないかもしれない」型です。`if (action)`で値の存在を確認し、`*action`で中身を取り出します。この例では通常1を推薦しますが、短い時間の確率的な比較なので、常に1になるという契約ではありません。

### この段階で必要な引数

| 引数 | 今回の値 | 意味 |
|---|---|---|
| `candidates` | `{0,1,2}` | 比較対象。呼び出し中に変更しない |
| `evaluate` | ラムダ式 | `(候補, MonteCarloNoise&)`を受け、有限の数値得点を返す |
| `time_us` | `2000` | 計算時間の予算。0以上 |

候補が空なら`nullopt`です。候補が一つ、または`time_us=0`なら先頭候補を返し、採点関数は呼びません。「推薦がある」ことと「採点済みである」ことは別です。採点済みか知りたくなったら、例03以降の探索器を使います。

## 5. 発展①：費用を最小化する、seedを指定する（例02）

配送費用が小さい経路を選びます。変更点は、評価値を`-cost`にしたことと、二つの任意引数を明示したことです。

```cpp
#include "monte_carlo_planner_v07.hpp"

int main() {
    std::array<int, 3> routes{0, 1, 2};
    std::array<double, 3> base_cost{10, 8, 9};
    auto route = mc_choose(routes,
        [&](int a, MonteCarloNoise& noise) {
            double cost = base_cost[a] + 4 * noise.uniform01();
            return -cost;
        }, 2000, false, 42);
    if (route) std::cout << *route << '\n';
}
```

ライブラリは常に大きい評価値を選びます。費用8より費用10の方が悪いなら、評価値を-8と-10にすれば、-8が選ばれる向きになります。最小化する量の符号を反転するだけです。

`base_cost`は渋滞がないときの費用です。`4 * noise.uniform01()`は0以上4未満の追加費用です。期待費用は12、10、11になるので、期待費用が最も小さいのは経路1です。

| 新しい引数 | 今回の値 | 意味 |
|---|---|---|
| `use_crn` | `false` | 候補間で乱数を共有する機能をOFFにする。既定値も`false` |
| `seed` | `42` | 内部乱数の初期値。既定値は1。42に特別な意味はない |

seedを同じにしても、時間制限で試せた回数が違えば推薦が変わることがあります。再現性のある比較が必要なら、次の例の固定評価回数を使います。CRNは後で追加できる任意機能です。ここまで、そして例21まではOFFのまま使います。

## 6. 発展②：探索器を作り、評価回数を固定する（例03）

例01と同じ採点を、`MonteCarloPlanner`へ移します。これにより、追加探索、統計の取得、複数ターンへ進めるようになります。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {};
    using Action = int;
    std::array<double, 3> base{10, 12, 11};
    std::array<int, 3> actions(const State&) const { return {0, 1, 2}; }
    double step(State&, int action, MonteCarloNoise& noise) const {
        return base[action] + 4 * (noise.uniform01() - 0.5);
    }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model);
    planner.start(Model::State{}, 1);
    decltype(planner)::Limits limits;
    limits.max_simulations = 1000;
    auto run = planner.search(limits);
    auto action = planner.best_action();
    assert(run.simulations == 1000);
    if (action) std::cout << *action << ' ' << run.simulations << '\n';
}
```

### `Model`は問題のルールを書く場所

`State`は一回の試行中に変化する状態です。この例は一手の採点だけなので、空の構造体で十分です。`Action=int`は、行動を整数で表すという宣言です。

`actions`は、その状態で比較する行動を返します。現在はいつでも0、1、2を選べるため、同じ配列を返します。`step`は行動した結果の一手分の報酬を返します。`State&`は状態を書き換えられる参照ですが、この例では更新する情報がありません。

`Model model;`を先に作り、その参照を使って`planner`を作ります。探索器はモデルをコピーして所有するのではありません。**モデルは探索器より長く生存させます。** `Model{}`のような一時オブジェクトから探索器を作ることはできません。

`planner.start(Model::State{}, 1)`で、新しい問題の根を設定します。「根」は、今回の比較の出発点です。`1`は残り手数で、一回の試行は一手で終わります。`start`の引数は参照から内部へコピーされるため、この空の一時状態を渡す使い方は問題ありません。

`decltype(planner)::Limits`は、この探索器の予算設定型です。名前の長さを避けるためのC++の書き方であり、特別な実行時処理ではありません。`max_simulations=1000`は、**今回の`search`で完了する試行を1000回まで**にします。この例は一手なので、試行数と`step`の実行回数は一致します。

`search`が計算を進め、`best_action`が推薦だけを返します。`best_action`を呼んでも、新しい試行は始まりません。`assert`は教材の確認で、指定どおり1000回完了したことを確かめています。

| 新しい要素 | 意味 |
|---|---|
| `Model::State` | 試行中に更新する状態 |
| `Model::Action` | 行動の型。コピー可能にする |
| `actions(state)` | 現在の候補を返す。`array`・`vector`・`span`などが使える |
| `step(state, action, noise)` | 状態を一手進め、一手分の報酬を返す |
| `start(root, horizon)` | 根をコピーして、新しい比較を開始する。以前の統計は破棄 |
| `Limits::max_simulations` | 今回の呼び出しで完了する試行数の上限 |
| `search(limits)` | 予算内で探索する。戻り値は今回の実行統計 |
| `best_action()` | 推薦する行動のコピー。なければ`nullopt` |

## 7. 発展③：暫定推薦と候補ごとの統計を読む（例04）

今度は根の候補を`start`へ明示します。探索前と探索後の違いを`result`で確認します。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {};
    using Action = int;
    std::array<double, 3> base{10, 12, 11};
    std::array<int, 3> actions(const State&) const { return {0, 1, 2}; }
    double step(State&, int action, MonteCarloNoise& noise) const {
        return base[action] + 4 * (noise.uniform01() - 0.5);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner planner(model);
    std::array<int, 3> candidates{0, 1, 2};
    planner.start(Model::State{}, 1, candidates);
    auto before = planner.result();
    assert(before.action && *before.action == 0 && before.provisional);
    assert(before.status == Planner::Status::ready);
    Planner::Limits limits;
    limits.max_simulations = 1000;
    planner.search(limits);
    auto result = planner.result();
    if (result.action) {
        std::cout << "action=" << *result.action
                  << " provisional=" << result.provisional << '\n';
    }
    for (const auto& candidate : result.candidates) {
        std::cout << "id=" << candidate.id.index
                  << " trials=" << candidate.trials;
        if (candidate.mean) std::cout << " mean=" << *candidate.mean;
        else std::cout << " mean=unevaluated";
        std::cout << '\n';
    }
    const auto& run = result.last_run;
    std::cout << "started=" << run.started << " completed=" << run.simulations
              << " transitions=" << run.transitions << " us=" << run.elapsed_us
              << " count_limit=" << (run.stop == Planner::Stop::simulations) << '\n';
    assert(!result.provisional);
    assert(planner.result(false).candidates.empty());
}
```

`start(root, horizon, candidates)`は、根で比較する候補をその場で登録します。これに対し、例03の自動候補生成では、実際の候補生成は`search`開始後です。自動生成の直後にまだ探索していなければ、推薦がないことがあります。

まだ評価していない段階でも、明示的に登録した先頭の候補0を返せます。このとき`provisional=true`です。「良いと分かった」という意味ではなく、**未評価なので、とりあえず返せる候補**という意味です。

### 結果に含まれる情報

| フィールド | 意味 |
|---|---|
| `action` | 推薦行動のコピー。未準備・終端などでは値がない |
| `id` | 推薦行動を識別する`ActionId`。例18の`advance`に渡せる |
| `status` | `ready`、`not_ready`、`terminal`のいずれか |
| `provisional` | 推薦候補に完了試行がまだなければ`true` |
| `candidates` | 根へ登録した候補のID・完了試行数・平均値 |
| `last_run` | 直前の`search`の実行統計 |

各`Candidate`の`trials`は、現在の根で蓄積された完了試行数です。`mean`はその平均得点ですが、一度も完了していない候補では`nullopt`です。未評価を0点と解釈しないよう、型でも区別しています。候補の行動値そのものは、この統計配列にはコピーしません。

`ActionId`は`generation`と`index`からなる識別子です。`index`を表示していますが、**行動番号ではありません**。本例で番号が一致するのは登録順による偶然です。IDは取得元の探索器・現在の根でのみ使い、内部フィールドから独自に生成しないでください。

### 一回の実行統計

| `RunStats` | 意味 |
|---|---|
| `started` | 今回、新たに開始した試行数 |
| `simulations` | 今回、最後まで完了した試行数 |
| `transitions` | 今回、実行した`step`の回数 |
| `elapsed_us` | 今回の`search`の経過時間。整数マイクロ秒 |
| `stop` | どの条件で停止したか |

この例では一手問題なので、最初の三つは1000で一致します。途中再開では一致しないことがあり、例13で確認します。

`result(false)`は、推薦と実行統計だけを得たいときの形です。候補統計の`vector`を作りません。`result`も`best_action`も試行や乱数消費は行いませんが、必要に応じて根の`terminal`を呼びます。平均が高いというだけで、統計的に最良と確定したという保証はありません。

## 8. 発展④：複数ターンの計画を表す（例05）

設備を使って生産します。行動0は生産、行動1は費用2で設備を1段階改良することです。設備レベルが高いほど、一回の生産の得点が上がります。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model);
    planner.start(Model::State{}, 6);
    decltype(planner)::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    if (auto action = planner.best_action()) std::cout << *action << '\n';
}
```

`State`に`turn`と`level`を追加しました。`turn`は現在までに進んだ手数、`level`は設備レベルです。初期値は0手目・レベル1です。

`step`は最初に`turn`を1増やします。改良なら`level`も1増やし、費用として-2を返します。生産なら`level`に0または1を足した得点を返します。`uniform_int(2)`は0以上2未満の整数を等確率で返すため、0か1です。

`start(..., 6)`の6が残り手数です。各試行は現在状態のコピーから始まり、6手分の報酬を足します。初期状態の`turn`が10でも、`horizon=6`なら「さらに6手」です。`horizon`は最終ターン番号ではありません。

ここで比較しているのは、現在の生産と改良です。その後の行動も得点に影響します。継続ルールをまだ定義していないため、後続の行動は候補から一様に選ばれます。そのルールの下での比較であり、将来も毎回最適な行動をするという仮定ではありません。今回の検証では0が出ましたが、例09で継続ルールを変えると判断も変わります。

`State`を自分で毎回リセットする必要はありません。試行用のコピーは探索器が作ります。一方、グラフや固定パラメータなど大きな不変データまで`State`へ入れると、コピーが重くなります。共有データを`Model`側に置く方法は例19で扱います。

### 完全観測のStateは、いつ作られ、いつ消えるか

ここまでのモデルは、現在の状態をすべて知って行動を選べる「完全観測」です。`Model::Info`は定義しません。ライブラリの説明やAPIに`Info`と書かれていても、この形式では`State`と同じ型を意味し、別のInfoオブジェクトは作りません。

同じStateという型でも、**利用者が持つ現在状態、探索器が保持する根、各試行の作業用状態**は別のオブジェクトです。「根」は、すべての試行の出発点です。

| 場面 | Stateの扱い |
|---|---|
| 利用者が現在状態を用意 | 今、実際に起きている状態を表す。この例なら0手目・レベル1 |
| `start(current,6)` | currentを探索器内の根へコピー。利用者のcurrentをそのまま書き換える参照ではない |
| 新しい試行の開始 | 根をコピーして、一試行専用のStateを作る |
| 試行中の`step` | その試行のStateだけを書き換える。次の手も同じオブジェクトを使う |
| 試行完了 | 報酬を統計へ反映して試行用Stateを破棄。根は出発点のまま |
| 次の試行 | 再び根からコピー。前の試行で改良した設備レベルを持ち越さない |
| `search`が試行途中で止まる | 未完了のStateを保持。同じ根へ追加で`search`すると、その途中から再開 |
| `start`・`advance`で実ターンの根を更新 | 指定した次状態を新しい根にする。未完了の古い試行は破棄 |
| `clear`・探索器の破棄 | 保持している根と未完了の試行を破棄 |

たとえば一つの試行が最初に改良してレベル2になっても、次の試行はレベル1から始まります。一方、同じ試行の2手目はレベル2を引き継ぎます。どちらも`step`へ渡すStateをライブラリが管理するので、利用者が試行番号を見てリセットする必要はありません。

利用者の現在状態を後から変更しても、すでに`start`でコピーした根は自動更新されません。また、シミュレーションを何回行っても実際の状態は進みません。推薦行動を実環境へ適用し、得た次状態を改めて`start`または`advance`へ渡すのは利用者側です。中断再開は例13、実ターン後の更新は例18で扱います。

探索器は`Model`本体をコピーせず参照します。**Modelは全試行で同じ一個、Stateは試行ごとの別物**です。Modelのメンバーに「今回の設備レベル」などを置くと、ライブラリは試行ごとに初期化してくれません。Modelには固定データや前計算、Stateには試行中に変わる値を置くと、この取り違えを避けられます。Modelと借用データは探索器より長く生存させます。

なお、コピーはC++の通常のコピーです。State内のポインタや`shared_ptr`の指す可変データまで自動で複製するものではありません。ある試行の変更が根や別試行へ伝わらないよう、可変部分を独立して更新できる形にします。

## 9. 発展⑤：早い終了と、終了時の得点を加える（例06）

例05を「3回生産したら終了」という問題へ変えます。また、最後に設備を売却でき、改良1段階につき0.5点が戻るものとします。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1, produced = 0; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        ++state.produced;
        return state.level + double(noise.uniform_int(2));
    }
    bool terminal(const State& state) const { return state.produced == 3; }
    double finish_value(const State& state) const { return 0.5 * (state.level - 1); }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model);
    planner.start(Model::State{}, 6);
    decltype(planner)::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
}
```

新しい`produced`は生産した回数です。生産行動のときだけ増えます。`terminal`は、問題上もう行動しない状態なら`true`を返します。終端に到達すると、まだ残り手数があってもその試行は終了します。

`finish_value`は最後に一度だけ加える得点です。本例の`0.5 * (level - 1)`は、改良した段階数に対する売却価値です。これまでの生産得点をもう一度返していないことに注目してください。

| 追加した要素 | 今回の意味 |
|---|---|
| `State::produced` | 生産回数。初期値0 |
| `terminal(state)` | 生産回数が3なら終了 |
| `finish_value(state)` | 終端または手数上限で終了したときの売却価値 |

**`step`で返した累計を、`finish_value`でも返すと二重計上になります。** `step`は一手ごとの増分、`finish_value`はまだ数えていない最後の項目、と分けます。

終端判定は、最初の根と各遷移後に行われます。根がすでに終端なら試行は開始されず、推薦もありません。`terminal`の省略時は常に非終端として扱い、残り手数で終了します。

## 10. 発展⑥：終了理由と割引率を使う（例07）

3回の生産を終えたらボーナス5点、6手で終えられなければペナルティ5点を付けます。さらに、将来の報酬へ0.9の割引を掛けます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1, produced = 0; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        ++state.produced;
        return state.level + double(noise.uniform_int(2));
    }
    bool terminal(const State& state) const { return state.produced == 3; }
    double finish_value(const State& state, MonteCarloFinishReason reason) const {
        double salvage = 0.5 * (state.level - 1);
        return salvage + (reason == MonteCarloFinishReason::terminal ? 5 : -5);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.discount = 0.9;
    Planner planner(model, options);
    planner.start(Model::State{}, 6);
    Planner::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
}
```

`Options`は探索器の作成時に渡す設定です。作成後に書き換える公開APIはありません。変えたい場合は新しい探索器を作ります。

`discount=0.9`は、1手先の報酬を0.9倍、2手先を$0.9^2$倍にする指定です。値域は0以上1以下です。通常のAHCの総得点が単純な和なら、既定の1から変える必要はありません。0.9は教材で効果を示す値で、一般的な推奨値ではありません。

`finish_value(state, reason)`の`reason`は、次の二種類です。

| 理由 | 発生条件 | 本例の加算 |
|---|---|---:|
| `MonteCarloFinishReason::terminal` | 問題上の終端に到達 | 売却価値に加えて+5 |
| `MonteCarloFinishReason::horizon` | 残り手数を使い切った | 売却価値に加えて-5 |

最後の一手で生産回数3と手数上限の両方を満たした場合、終端判定が優先され、`terminal`です。理由付きと理由なしの`finish_value`を両方定義した場合も、理由付きの方が優先されます。

6手後の終了時得点にも$0.9^6$が掛かります。終了時だけ割引を掛けない仕様ではありません。元の問題が割引なしなら、探索の都合だけで割引率を変えると、比較している目的そのものが変わります。

## 11. 発展⑦：最終スコアだけで採点する（例08）

既存シミュレータが、最後にまとめてスコアを返す形になっていることもあります。その場合は`step`を`void`にできます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int level = 1; double total = 0; };
    using Action = int;
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    void step(State& state, int action, MonteCarloNoise& noise) const {
        if (action == 1) { ++state.level; state.total -= 2; }
        else state.total += state.level + double(noise.uniform_int(2));
    }
    double finish_value(const State& state) const { return state.total; }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model); // discount=1
    planner.start(Model::State{}, 6);
    decltype(planner)::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
}
```

`step`の戻り値が`void`なら、その手の報酬は0として扱われます。代わりに`State::total`へ得点と費用を蓄積し、最後の`finish_value`で一度だけ返しています。

`discount=1`で、途中の判断や候補生成が`total`に依存しない本例なら、例05と同じ得点の付け方を表せます。割引率を1未満にすると、この形では累計全体に終了時点までの割引が掛かるため、一手ずつ報酬を返す形と一般には一致しません。

実際の問題で「最終配置の面積」や「完成した割当のスコア」を最後に計算する場合に使えます。報酬を二重計上しないため、**各手で増分を返すか、最後に累計を返すか**を目的に合わせて決めます。

## 12. 発展⑧：その後の行動を賢くする（例09）

ここからは、終了条件を追加する前の例05へ戻ります。新しく`rollout_action`だけを追加し、残りが長い間に設備を改良するルールを与えます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model);
    planner.start(Model::State{}, 6);
    decltype(planner)::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
}
```

`rollout_action`は、比較中の最初の一手の後などに、後続の行動を安く決める関数です。「ロールアウト」は、一つの展開を先へ進めて、最終的な得点を見ることを指します。

| 引数・定数 | 意味 |
|---|---|
| `state` | 現在の試行の状態 |
| `remaining` | これから選ぶ一手も含めた残り手数 |
| `Rng&` | この手の継続方策に使う乱数。今回のルールでは未使用 |
| `remaining >= 4` | 改良費用を後で回収できそうな、十分長い残り時間の目安 |
| `state.level < 3` | この簡易ルールで目標とする設備レベル |

残り4手以上でレベル3未満なら改良し、それ以外は生産します。4と3はこの例の簡単な方策であり、ライブラリが要求する値ではありません。

`template<class Rng>`としているのは、既定の乱数型でも利用者指定の型でも呼べるようにするためです。既定の型だけを使うなら、`MonteCarloNoise&`と書いても構いません。戻り値は`Action`そのもので、`optional`ではありません。非終端なら必ず合法な行動を返します。

この例の検証では、例05の0から1へ推薦が変わりました。「その後も適切に生産する」前提になり、最初の改良の意味が変わるためです。良い継続方策は重要ですが、誤った方策を固定すれば、その方策の下で有利な一手を選んでしまうこともあります。

後続行動を決める乱数と、環境の偶然を作る乱数は、同じ型の別系列です。`rollout_action`には方策用、`step`には遷移用の系列が渡ります。CRNをONにすると両方が共有の対象になります。需要や故障などの環境の変動は、それを処理する`step`の引数から生成します。

## 13. 発展⑨：残り手数を見て候補を生成し、バッファを使い回す（例10）

最後の一手で設備を改良しても、この問題では生産に使えず費用だけ掛かります。残り一手では、生産だけを候補にします。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1; };
    using Action = int; // 0: 生産、1: 設備を改良
    void actions(const State&, int remaining, std::vector<Action>& out) const {
        out.push_back(0);
        if (remaining >= 2) out.push_back(1);
    }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model);
    planner.start(Model::State{}, 6);
    decltype(planner)::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
}
```

変更点は`actions`の形です。

| 引数 | 意味 |
|---|---|
| `const State&` | 候補生成時の状態。本例では値を読まない |
| `int remaining` | 現在の一手を含む残り手数 |
| `std::vector<Action>& out` | 候補を書き込む出力先。呼び出し前に探索器が空にする |

`out`は探索器の作業バッファなので、利用者は必要な候補を`push_back`するだけです。毎回新しい`vector`を返す形に比べ、確保済み容量を使い回せます。候補が少なく固定なら、これまでの`array`を返す形も十分簡単です。

今回は「合法でも必ず不利な候補を省く」例です。一般には、資源不足の行動を入れないなど、合法性の条件もここに書けます。ただし、省いた候補は選ばれないので、安全な削減かは利用者が判断します。

簡易形の`actions(info)`と、この出力形を両方定義した場合は出力形が優先されます。バッファへの参照をコールバック後まで保存したり、別の場所から同時に変更したりしないでください。

## 14. 発展⑩：浅く打ち切り、残りの価値を近似する（例11）

6手先まで毎回進める代わりに、2手だけ進め、残りは「今のレベルで生産を続けたら得られる期待得点」で近似します。例09からの変更です。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
    double leaf_value(const State& state, int remaining) const {
        return remaining * (state.level + 0.5);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.simulation_depth = 2;
    Planner planner(model, options);
    planner.start(Model::State{}, 6);
    Planner::Limits limits;
    limits.max_simulations = 2000;
    auto run = planner.search(limits);
    assert(run.transitions == 4000);
    std::cout << *planner.best_action() << ' ' << run.transitions << '\n';
}
```

`simulation_depth=2`が、新たな打ち切り深さです。一方、`start(...,6)`の6は目的としている残り手数のままです。**考える問題を2手へ変えるのではなく、6手の得点を浅い試行で近似しています。**

`leaf_value(state, remaining)`は、打ち切り地点から先の得点を返します。本例では、生産一回の期待得点が`level + 0.5`なので、残り回数を掛けています。改良を将来も選べる問題ですが、それを無視して「この先はずっと生産」と近似しています。

`remaining`には、すでに進めた2手は含みません。6手から2手進めた時点なら4です。これまでの報酬を`leaf_value`にもう一度入れないでください。

| 設定・関数 | 意味 |
|---|---|
| `simulation_depth=0` | 既定値。浅い打ち切りをしない |
| `simulation_depth=2` | 根から2手で、未完了なら近似値を使う |
| `leaf_value(info, remaining)` | その時点を起点とする、残りの報酬と終了時得点の近似 |

この例は終端がないので、2000試行で遷移は4000回になります。`simulation_depth`が`horizon`以上なら、通常の終了が先になり、`leaf_value`は呼ばれません。途中で本当の終端へ着いた場合も`finish_value`です。

浅い打ち切り時は、`leaf_value`と`finish_value`を両方足すわけではありません。最後の売却価値などを見込むなら、その分も`leaf_value`へ含めます。`discount<1`なら、残りの価値もその地点を起点に割り引いて返し、根までの割引はライブラリへ任せます。近似の偏りがあるため、単に速くなれば良いとは限りません。

ここまでは完全観測です。部分観測では、観測情報だけから期待値を計算する形に加え、試行で仮定した真の状態も使って採点する形を選べます。例30で、両者を分けて学びます。

### 完全観測のModelが呼ばれる順番

ここまでの`tree=false`の例をまとめます。省略したメソッドは呼ばれません。表中の「根」と「試行用State」は、例05で分けた別々のオブジェクトです。

| 順番 | 処理・呼び出し | 読む・更新する状態 |
|---:|---|---|
| 1 | `start`で根をコピー | Modelのメソッドはまだ呼ばない。明示候補があればここで登録 |
| 2 | `search`で予算を確認し、`terminal(root)` | 現在の根を読む。すでに終端、または残り0手なら試行しない |
| 3 | 必要なら`actions(root)`を呼び、根の一手を選ぶ | 根候補は初回に生成して保存する。毎試行呼ぶわけではない |
| 4 | 根をコピーし、試行用Stateを作る | 完全観測では`sample_root`を呼ばない |
| 5 | `step(state, action, rng)` | 選択した行動で試行用Stateを進め、一手の報酬を受け取る |
| 6 | `terminal(state)` | 更新後のStateを読む。trueなら`finish_value(...,terminal)`で完了 |
| 7 | 残り手数の上限を確認 | 到達していれば`finish_value(...,horizon)`で完了 |
| 8 | 浅い打ち切り深さを確認 | 到達していれば`leaf_value(state,remaining)`で完了 |
| 9 | 続けるなら`rollout_action(state,remaining,rng)` | 次の一手を決め、5へ戻る。省略時は`actions`からライブラリが一様選択 |
| 10 | 完了した得点を統計へ反映 | 試行用Stateを破棄し、予算があれば3から新しい試行 |

根の`terminal`は、予算が残って処理を進める各`search`内で最初に一度確認します。同じ`search`の中で試行が変わるたびに根の判定をやり直すわけではありません。一方、試行用Stateの`terminal`は各`step`の後に呼びます。`terminal`を省略したモデルでは、この判定はfalseとして扱います。

6〜8は順番に判定し、最初に成立したものだけを使います。終端と手数上限が同時なら終端が優先、手数上限と浅い打ち切りが同時なら上限が優先です。`finish_value`を省略した場合は0を加え、浅い打ち切りが起きた場合は`leaf_value`だけを使います。開始時点ですでに終端なら、`finish_value`を呼んで採点することもありません。

予算が尽きたときは、その時点の処理を保持して停止します。時間切れ自体を理由に`leaf_value`や`finish_value`を呼ぶことはありません。未完了の試行があれば、次の`search`で根候補を選び直したりStateを根から作り直したりせず、続きから進めます。木探索・逐次提案を含む全メソッドの分岐は、付録A.7にまとめています。

## 15. 発展⑪：時間・締切・作業量の予算を組み合わせる（例12）

例09のモデルを使い、複数の停止条件を指定します。次の値は短時間で実行を確認するための教材用です。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
};

int main() {
    using Clock = std::chrono::steady_clock;
    auto deadline = Clock::now() + std::chrono::microseconds(5000);
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.clock_interval = 1;
    Planner planner(model, options);
    std::array<int, 2> candidates{0, 1};
    planner.start(Model::State{}, 6, candidates);
    Planner::Limits limits;
    limits.time_us = 2000;
    limits.deadline = deadline;
    limits.max_simulations = 100000;
    limits.max_transitions = 500000;
    auto run = planner.search(limits);
    auto result = planner.result(false);
    std::cout << *result.action << " us=" << run.elapsed_us
              << " completed=" << run.simulations << '\n';
}
```

| 設定 | 今回の値 | 対象 |
|---|---:|---|
| 外側の`deadline` | 現在から5000マイクロ秒後 | モデル・探索器・根候補の準備を含めた、この部分の締切 |
| `limits.time_us` | 2000 | 今回の`search`開始からの相対時間 |
| `limits.deadline` | 上で作った時刻 | 絶対時刻による締切 |
| `limits.max_simulations` | 100000 | 今回の完了試行数の上限 |
| `limits.max_transitions` | 500000 | 今回の`step`回数の上限 |
| `options.clock_interval` | 1 | 通常時の時計確認を一遷移ごとにする |

指定した条件のうち、最初に達した条件で止まります。回数・相対時間の既定値-1は制約なし、`deadline`の既定値は時計型の最大時刻です。少なくとも一つの制約が必要です。

時間だけでよければ、例01と同じ単位で`planner.search(2000)`と書けます。`search(0)`は試行を進めません。

### マイクロ秒指定は、強制停止の精度ではない

`step`、候補生成、状態や行動のコピー、`finish_value`などの実行途中へ割り込んで止める仕組みではありません。たとえば一回の`step`に5ミリ秒掛かると、その途中で2ミリ秒の締切を迎えても、戻ってくるまで止まれません。

`clock_interval`の既定値は8で、時計を見る負担を抑えます。1にすると確認が細かくなりますが、時計を読む回数が増えます。また探索器は締切付近で間隔を細かくしますが、OSによる遅延や重い処理の超過をなくす保証はありません。

AHCで全体1950ミリ秒を使うなら、たとえばプログラムの早い段階で1950000マイクロ秒後の締切を一度だけ決め、各探索へ同じ締切を渡す形にできます。入出力、`start`、結果取得、実際の行動適用の時間も必要なので、コンテストの制限いっぱいを`search`に割り当てないよう調整します。

`elapsed_us`は`search`内部の時間です。`start`や`result`の時間は含みません。候補を明示しているので、仮に探索開始前に締切を迎えても、先頭候補の暫定推薦は取得できます。

## 16. 発展⑫：途中で止め、同じ試行を再開する（例13）

1試行は6手ですが、最初の呼び出しでは3手しか許しません。次の呼び出しで、その続きを含めて57手進めます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model);
    planner.start(Model::State{}, 6);
    decltype(planner)::Limits limits;
    limits.max_transitions = 3;
    auto first = planner.search(limits);
    assert(first.started == 1 && first.simulations == 0 && first.transitions == 3);
    assert(planner.result().provisional);
    limits.max_transitions = 57;
    auto second = planner.search(limits);
    assert(second.started == 9 && second.simulations == 10);
    assert(second.transitions == 57);
    int total_trials = 0;
    for (const auto& candidate : planner.result().candidates) total_trials += candidate.trials;
    assert(total_trials == 10);
    std::cout << first.simulations << ' ' << second.simulations << ' ' << total_trials << '\n';
}
```

最初の結果は、開始1回・完了0回・遷移3回です。まだ最終得点がないので、この試行は候補の平均へ反映されません。推薦があっても暫定です。

次は、残っていた3手で最初の試行を完了し、新しく9試行を6手ずつ進めます。合計で$3+9\times6=57$手です。そのため二回目は、**開始9回・完了10回**になります。`started`より`simulations`が多いこと自体は異常ではありません。

根を変えずに追加探索したいときは、`start`を呼び直さず、`search`をもう一度呼びます。`Limits`の回数は、その呼び出し分の予算です。累計の上限ではありません。一方、各候補の`trials`は蓄積され、この例では合計10になります。

中断時の状態、すでに選んだ行動、試行seedと手数、途中の報酬は内部に保持されます。CRN OFFでは、使い続けているRNGの現在位置も保持します。同じモデル・seed・URBG・操作列で、時間制限を使わず固定遷移数だけを分割すれば、まとめて同じ遷移数を実行した場合と同じ評価列になります。途中でモデルを変更したり、コールバック内で別の共有RNGや外部状態に依存したりすると、この条件から外れます。

## 17. 発展⑬：比較中に候補を追加する（例14）

最初は候補0と2だけを比較し、後から候補1を追加します。問題の採点ルールは変えません。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {};
    using Action = int;
    std::array<double, 3> base{10, 12, 11};
    std::array<int, 3> actions(const State&) const { return {0, 1, 2}; }
    double step(State&, int action, MonteCarloNoise& noise) const {
        return base[action] + 4 * (noise.uniform01() - 0.5);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner planner(model);
    std::array<int, 2> candidates{0, 2};
    planner.start(Model::State{}, 1, candidates);
    Planner::Limits limits;
    limits.max_simulations = 500;
    planner.search(limits);
    auto added_id = planner.add_root_action(1);
    planner.search(limits);
    auto result = planner.result();
    int total = 0;
    for (const auto& candidate : result.candidates) {
        total += candidate.trials;
        if (candidate.id == added_id) std::cout << "added_trials=" << candidate.trials << '\n';
    }
    assert(total == 1000);
    std::cout << "recommended=" << *result.action << '\n';
}
```

`add_root_action(1)`は新しい根候補を登録し、`ActionId`を返します。既存候補の試行数と平均は残ります。次の`search`では、前半500回にさらに500回を足し、累計1000回になります。

新しい候補は、まだ比較へ参加していない候補の先頭へ置かれます。すでに参加中の候補を先頭からやり直したり、全候補の評価回数を揃えたりする機能ではありません。参加枠が広がったときに優先して取り込めるという意味です。

`Candidate::id == added_id`で、追加した候補の統計を見つけています。`index`から行動値を復元せず、IDの等値比較を使います。重複する行動を追加しても自動除去しないため、重複を避けたい場合は利用者側で管理します。

外部候補を根へ渡す方法は、候補生成が別の探索器や別のヒューリスティックで実装済みの場合に便利です。この制限は根だけに適用され、後続状態の行動はモデルの`actions`や`rollout_action`などで決まります。

## 18. 発展⑭：空候補、終端、初期化を扱う（例15）

推薦がない場合と、まだ評価していない場合を、動くコードで区別します。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {};
    using Action = int;
    std::array<double, 3> base{10, 12, 11};
    std::array<int, 3> actions(const State&) const { return {0, 1, 2}; }
    double step(State&, int action, MonteCarloNoise& noise) const {
        return base[action] + 4 * (noise.uniform01() - 0.5);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner planner(model);
    assert(planner.result().status == Planner::Status::not_ready);
    std::array<int, 0> empty{};
    planner.start(Model::State{}, 1, empty);
    Planner::Limits limits;
    limits.max_simulations = 10;
    assert(planner.search(limits).stop == Planner::Stop::not_ready);
    assert(!planner.best_action());
    planner.start(Model::State{}, 0);
    assert(planner.search(limits).stop == Planner::Stop::terminal);
    assert(planner.result().status == Planner::Status::terminal);
    std::array<int, 1> only{2};
    planner.start(Model::State{}, 1, only);
    assert(planner.search(0).transitions == 0);
    assert(planner.result().provisional);
    planner.clear();
    assert(!planner.best_action());
    planner.start(Model::State{}, 1, only);
    planner.search(limits);
    assert(!planner.result().provisional);
    std::cout << *planner.best_action() << '\n';
}
```

| 操作・状況 | 結果 |
|---|---|
| 構築直後、まだ`start`していない | `result().status`は`not_ready` |
| 根候補を明示的に空にする | 探索は`Stop::not_ready`。推薦なし |
| `horizon=0` | 探索は`Stop::terminal`。推薦なし |
| 候補を一つ登録し、`search(0)` | 未評価の暫定推薦あり |
| `clear()` | 根と論理状態を消し、推薦なしになる |
| その後`start`して探索 | 新しい問題として利用できる |

`clear`も`start`も、主要な連続配列や作業バッファの確保済み容量を再利用できます。すべての入れ子の`vector`や状態内部の動的メモリまで容量を保持する保証はありません。モデルそのものは外部にあるので消えません。

本例の`max_simulations=10`は動作確認用の小さな回数です。`only{2}`の2は「候補が二つ」ではなく行動値2であり、候補数は一つです。`mc_choose`の一候補時の簡略処理とは違い、探索器へ一候補を渡して正の予算で探索すれば、その候補を実際に評価できます。

`clear`の後、再び`start`する前に`search`すると契約違反です。また、`start`や`clear`は内部乱数を最初のseedへ巻き戻しません。初回と同じ乱数系列から完全にやり直したいなら、同じ設定で探索器を新しく作ります。

`Status`は現在の根の状態、`Stop`は直前の停止理由です。たとえば手数0の根でも、同時に評価回数上限0を渡せば、予算チェックが先に働いて停止理由が`simulations`になることがあります。両者を同じ意味として扱わないでください。

## 19. 発展⑮：将来の判断も木に保存する（例16）

例09までは、根の一手を比較し、その後は継続方策へ任せていました。`tree=true`にすると、将来の判断点も保存し、繰り返し訪れる場所では次の行動も比較できます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {
        int turn = 0, level = 1;
        friend bool operator==(const State&, const State&) = default;
    };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.tree = true;
    Planner planner(model, options);
    planner.start(Model::State{}, 6);
    Planner::Limits limits;
    limits.max_simulations = 3000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
}
```

変更点は二つです。`Options::tree`を`true`にし、`State`へ等値比較の`operator==`を追加しました。`= default`は、`turn`と`level`が両方一致するかをC++に比較してもらう指定です。

保存済みの行動の先で、同じ状態へ再び到達したことが分かれば、そこに保存した行動の統計を使えます。今回、生産の得点には0か1の変動がありますが、それによって`turn`や`level`は変わりません。そのため同じ将来状態を再利用しやすい問題です。

木にまだ保存していない場所や、保存上限に達した先では、`rollout_action`を使います。木探索をONにしても、継続方策は引き続き役に立ちます。

**木をONにすれば必ず良くなるわけではありません。** 毎回ほぼ異なる連続値状態へ進む問題では、同じ場所を訪れにくく、保存・比較の費用ばかり増えることがあります。まずOFFで使い、同じ計算時間で品質を比較するのが自然です。

この木は、すべての同一状態を全探索履歴から集約する表ではありません。同じ親の同じ行動から生じた結果を、保存済みの子と比較します。異なる行動履歴の先に同じ状態があっても、木全体を横断して一つにまとめる機能はありません。

## 20. 発展⑯：状態の識別に必要なキーだけを使う（例17）

状態に、あとで表示したい過去の行動履歴を入れます。しかし、この例の将来の候補・報酬は`turn`と`level`だけで決まります。履歴全体を比較せず、この二つをキーにします。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, level = 1; std::vector<int> past_actions; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        state.past_actions.push_back(action);
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
    auto tree_key(const State& state) const { return std::pair(state.turn, state.level); }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.tree = true;
    Planner planner(model, options);
    planner.start(Model::State{}, 6);
    Planner::Limits limits;
    limits.max_simulations = 3000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
}
```

`tree_key(state)`は、木で状態を識別するキーを返します。本例の`std::pair(turn, level)`は、二つの整数を組にした型で、等値比較できます。`State`自体に`operator==`を定義する必要はなくなりました。

`past_actions`は教材用の表示・記録情報です。将来の得点に影響しないのでキーから省けます。ただし、`State`のコピー時にはこの履歴もコピーされるため、キーを短くするだけで状態のコピーまで軽くなるわけではありません。記録が不要なら持たせない方が簡単です。

### 正しいキーの条件

同じキーとする状態は、後の合法行動、報酬・遷移の分布、観測可能な終端、継続方策、採点に必要な既知情報が同じである必要があります。たとえば、在庫が違えば次の行動が変わる問題で、頂点番号だけをキーにしてはいけません。部分観測では、最後の観測が同じでも、過去の観測から得た推定が違うことがあります。同じInfoの下でサンプルされた隠れたStateやその採点値は、試行ごとに違って構いません。その真値をキーに追加して、見えていない違いで行動選択を分けてはいけません。

キーは値でも参照でも返せます。内部へ保存するときはコピーします。ただし、`string_view`やポインタを返すと、コピーしても参照先まで所有しません。参照先を木の有効期間中維持し、意味が途中で変わらないようにします。単純な整数や`pair`、`tuple`なら、その点で扱いやすくなります。

ハッシュ値一つだけをキーにすることも型の上では可能ですが、衝突して異なる状態を混ぜる危険は利用者側に残ります。ライブラリが衝突を追加検査するわけではありません。

`terminal`や`tree_key`は、渡された情報と固定したモデルに基づく値を返します。呼び出し回数で答えが変わる実装にはしません。

## 21. 発展⑰：実際に一手進め、部分木を再利用する（例18）

例16の問題を実際に6ターン進めます。各ターンで探索して一手を実行し、観測した次状態へ根を移します。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {
        int turn = 0, level = 1;
        friend bool operator==(const State&, const State&) = default;
    };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.tree = true;
    Planner planner(model, options);
    Model::State current;
    int remaining = 6;
    planner.start(current, remaining);
    MonteCarloNoise actual_noise(123);
    double actual_score = 0;
    while (remaining > 0) {
        Planner::Limits limits;
        limits.max_simulations = 1000;
        planner.search(limits);
        auto result = planner.result(false);
        assert(result.action);
        actual_score += model.step(current, *result.action, actual_noise);
        --remaining;
        bool reused = planner.advance(result.id, current, remaining);
        std::cout << "action=" << *result.action << " reused=" << reused << '\n';
    }
    assert(planner.result().status == Planner::Status::terminal);
    std::cout << "score=" << actual_score << '\n';
}
```

ここでの`actual_noise`は、教材で実環境を模擬するための別の乱数です。本当のインタラクティブ問題なら、行動を出力した後、問題側から返された観測を読み取り、`current`を更新する部分に置き換えます。

`actual_noise(123)`の123は環境役の乱数の初期値で、特別な値ではありません。`actual_score`は実際に実行した六手の得点を外側で合計する変数です。探索中に試した仮の得点とは別に管理します。

`result(false)`から、行動値とそのIDを一緒に取得します。実行後に`remaining`を1減らし、`advance(result.id, current, remaining)`を呼びます。

| 引数・戻り値 | 意味 |
|---|---|
| `result.id` | 実際に選んだ、現在の根の行動ID |
| `current` | 実際に観測・確定した次の状態 |
| `remaining` | 次の状態から考える残り手数 |
| 戻り値`true` | 条件が合い、保存済み部分木の統計を再利用できた |
| 戻り値`false` | 再利用せず、指定した新しい根でリセットした |

`false`でも、新しい根への移動そのものは済んでいます。失敗して元の根に残るわけではないため、同じ目的で`start`をもう一度呼ぶ必要はありません。

### 統計を再利用できる条件

- 完全観測のモデルで、`tree=true`である。
- 次の残り手数が、前の残り手数からちょうど1減っている。
- 前の探索が、浅い打ち切りによる近似を実際には使っていない。
- 選んだ行動の先に、観測した状態に一致する子が保存されている。
- その子に行動があり、一括候補の取り込みが保存上限で途中になっていない。
- モデルの採点・遷移・継続方策などを変更していない。

最後の条件は自動判定されません。利用者が守る契約です。再利用した統計は、それまでの継続方策や木の成長中に得た評価も含みます。新しい根からゼロから始めた場合と評価列が一致する、という意味ではありません。

本例の検証では途中の移動は`true`、最後の移動は`false`になりました。最後は残り手数0で、再利用する次の判断点を持たないためです。将来の一般の問題で、毎回`true`になる保証はありません。

`advance`や`start`の後は、古い`ActionId`を使い回せません。次に必要なIDは、新しい根の結果から取得します。また、`advance`後の候補はモデル由来です。次の根も外部候補で制限したい場合は、明示候補付きの`start`を使います。

## 22. 発展⑱：グラフ・採点を更新して再計画する（例19）

今度は、辺の費用が変わる経路計画です。大きな不変・共有データはモデルから参照し、状態は現在頂点だけにします。これはデータの持ち方を示す小さな例であり、この規模・条件なら実用上は厳密な最短路で解けます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct Edge { int to; double cost; };
    using Graph = std::array<std::vector<Edge>, 4>;
    struct State { int vertex = 0; };
    using Action = int; // 現在頂点の隣接リストにおける添字
    const Graph& graph;
    std::vector<Action> actions(const State& state) const {
        std::vector<Action> out(graph[state.vertex].size());
        std::iota(out.begin(), out.end(), 0);
        return out;
    }
    double step(State& state, int action, MonteCarloNoise&) const {
        const auto edge = graph[state.vertex][action];
        state.vertex = edge.to;
        return -edge.cost;
    }
    bool terminal(const State& state) const { return state.vertex == 3; }
    double finish_value(const State& state) const { return state.vertex == 3 ? 0 : -100; }
};

int main() {
    Model::Graph graph{{{{1, 1}, {2, 2}}, {{3, 1}}, {{3, 1}}, {}}};
    Model model{graph};
    MonteCarloPlanner planner(model);
    Model::State current;
    decltype(planner)::Limits limits;
    limits.max_simulations = 500;
    planner.start(current, 3);
    planner.search(limits);
    auto old_action = planner.best_action();
    std::cout << "before=" << *old_action << '\n';
    graph[0][0].cost = 10;
    auto candidates = model.actions(current);
    if (old_action) {
        auto it = std::find(candidates.begin(), candidates.end(), *old_action);
        if (it != candidates.end()) std::rotate(candidates.begin(), it, it + 1);
    }
    planner.start(current, 3, candidates);
    planner.search(limits);
    std::cout << "after=" << *planner.best_action() << '\n';
    assert(*old_action == 0 && *planner.best_action() == 1);
}
```

グラフは頂点0から1または2へ進み、最後に頂点3へ着く形です。当初は0→1→3の費用が2、0→2→3が3です。`step`は費用の符号を反転して返します。頂点3が終端で、手数内に着けなければ`finish_value`で-100を加えます。

`Model::Graph`は、四つの隣接リストを持つ型です。`const Graph& graph`で借用するため、各試行の`State`にグラフ全体をコピーしません。グラフ、モデル、探索器の順に作り、この順序で参照先の寿命を確保しています。

探索後に`graph[0][0].cost`を10へ変えます。モデルは同じグラフを参照しているので新しい値を読みますが、古い平均値は変更前の採点に対するものです。**モデルが変わったら`start`で統計をリセットします。**

古い推薦を候補順の先頭へ移す部分は、弱い初期ヒントとして使う例です。現在も候補として存在することを`find`で確認してから並べ替えます。以前の評価値を新しい問題へ移しているわけではありません。変更で良し悪しが逆転する本例では、最後には別の候補が推薦されます。

| 更新したもの | 適切な操作 |
|---|---|
| 同じ問題に計算時間を追加 | そのまま`search` |
| 既存候補の意味を変えず、新候補だけ追加 | `add_root_action` |
| 実際に一手進み、同じ目的の残り部分を解く | 条件が合えば`advance` |
| グラフ、採点、遷移、推定分布、継続方策を変更 | `start` |
| 毎ターン常に「今から6手」を先読みする | 通常は`start`。残り手数が1減る再利用条件に合わない |

グラフに依存する前計算をモデル側に持っている場合、変更によって無効になる部分は利用者が更新します。依存しない前計算や共有データは使い続けられます。モデルの変更を探索器が自動検出したり、統計のどこまでが有効か自動推定したりする仕組みはありません。

## 23. 発展⑲：大量・連続的な候補を少しずつ提案する（例20）

行動を0以上1以下の投入量とし、すべてを列挙する代わりに一つずつ提案します。`Action`は整数でなくても構いません。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {};
    using Action = double; // 投入量を0以上1以下で選ぶ
    template<class Rng>
    std::optional<Action> propose_action(const State&, int, int index, Rng& rng) const {
        if (index == 1000) return std::nullopt;
        if (index == 0) return 0.5;
        return std::uniform_real_distribution<double>(0, 1)(rng);
    }
    double step(State&, double amount, MonteCarloNoise& noise) const {
        return 10 - 20 * (amount - 0.7) * (amount - 0.7)
                  + 0.2 * (noise.uniform01() - 0.5);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.widening = 2;
    options.exploration = 1;
    options.value_scale = 1;
    Planner planner(model, options);
    planner.start(Model::State{}, 1);
    Planner::Limits limits;
    limits.max_simulations = 5000;
    planner.search(limits);
    auto result = planner.result();
    assert(result.action && *result.action >= 0 && *result.action < 1);
    std::cout << "amount=" << *result.action << " candidates=" << result.candidates.size() << '\n';
}
```

得点は投入量0.7の近くで高くなります。本例の関数は最適量が式から分かりますが、提案APIの働きを見やすくするために使っています。実際には、同じ位置へ重いシミュレータを接続できます。

`10`は変動を除いた最大得点、`0.7`は最も良い投入量、`20`はそこから離れたときの減点の強さです。差を二乗しているので、左右どちらに離れても減点されます。`0.2 * (uniform01() - 0.5)`は、-0.1以上0.1未満の小さな変動です。最初の候補0.5は、範囲の中央を初期案として渡しているだけです。

### 提案関数の引数

| 引数・戻り値 | 意味 |
|---|---|
| `const State&` | 候補を提案する状態 |
| `int remaining` | 残り手数。本例は一手なので値を読まない |
| `int index` | その判断点での提案番号。0から始まる |
| `Rng& rng` | この残り手数・提案番号に対応する候補生成用乱数 |
| `std::optional<Action>` | 新しい候補。`nullopt`なら、その判断点では以降の提案を止める |

最初の提案は0.5、その後は0以上1未満の一様乱数です。`index==1000`で候補切れにします。1000は提案回数の上限であり、各候補の評価回数ではありません。`nullopt`は「今回だけ提案に失敗したので後で再挑戦する」という意味ではありません。

この例では`actions`を定義していません。`propose_action`が候補を供給し、一手で終わるため後続行動も不要です。複数手のモデルで提案だけに頼るなら、後続状態で合法な行動が必ず得られるよう、`rollout_action`なども設計します。

### 候補数と、一候補を調べる深さのバランス

| オプション | 今回の値 | 意味 |
|---|---:|---|
| `widening` | 2 | 完了試行数が増えるにつれ、比較に参加させる候補数を増やす係数 |
| `exploration` | 1 | 有望さだけでなく不確かさも調べる強さ |
| `value_scale` | 1 | 得点の変動を考えるときの尺度を明示。得点の正規化や最大値ではない |

`widening=2`なら、参加枠はおおよそ「完了数+1の平方根の2倍」です。ただし、新規参加は一回の選択につき最大一候補です。候補を増やしすぎず、すでに参加した候補にも計算を使うための設定です。`widening=0`は、候補が残る限り毎回一候補を参加させる指定で、候補数0を意味しません。

`exploration`を大きくすると、まだ十分に調べていない候補も選びやすくなります。小さくすると、高い平均が出ている候補を重視します。0にしても、新しい候補の参加は別に行われるため、すべての探索的な処理が止まるわけではありません。

`value_scale=0`が既定値で、通常の試行から尺度を推定します。本例の1はAPIを示すための明示値です。得点を100倍にするなら、明示尺度も同じように見直します。特定の調整値を、異なる得点範囲の問題へそのまま移さない方が分かりやすくなります。

候補が多い場合、全候補へ先に一回ずつ評価を配る処理はありません。有限時間で全候補を試す保証もありません。候補の初期順や提案器へ良い知識を入れると有用です。手法の具体的な配分式は付録で説明します。

## 24. 発展⑳：保存する木の大きさを制限する（例21）

例16の木探索に、ノード数と行動数の小さな保存上限を設定します。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {
        int turn = 0, level = 1;
        friend bool operator==(const State&, const State&) = default;
    };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, MonteCarloNoise& noise) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(noise.uniform_int(2));
    }
    template<class Rng>
    int rollout_action(const State& state, int remaining, Rng&) const {
        return remaining >= 4 && state.level < 3 ? 1 : 0;
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.tree = true;
    options.max_nodes = 4;
    options.max_edges = 8;
    Planner planner(model, options);
    planner.start(Model::State{}, 6);
    Planner::Limits limits;
    limits.max_simulations = 1000;
    auto run = planner.search(limits);
    assert(run.simulations == 1000 && run.transitions == 6000);
    std::cout << *planner.best_action() << ' ' << run.transitions << '\n';
}
```

| 設定 | 今回の値 | 既定値 | 数えるもの |
|---|---:|---:|---|
| `max_nodes` | 4 | 32768 | 根を含む保存された判断点 |
| `max_edges` | 8 | 131072 | 全判断点を通した保存行動の総数 |

これは試行数の上限ではありません。保存枠を使い切っても、保存できない先をロールアウトで評価できます。本例では1000試行・6000遷移まで完了します。

一方、自動で一括列挙した候補を取り込む領域が足りなければ、その判断点で取り込めない候補が出ます。特に根候補が上限を超えれば、そもそも比較できない候補が生じます。明示的に渡す根候補や`add_root_action`は、上限内に収める契約です。

`max_edges`は根候補だけの数でも、確保バイト数でもありません。行動の型やノード内の`vector`などによって、実際のメモリ量は変わります。候補数が大きい場合、単に上限を増やすほか、例20の逐次提案を使う選択肢もあります。

## 25. 発展㉑：簡易CRNをON/OFFする（例22）

同じ一手問題を、CRNだけOFFとONにして比較します。評価回数、探索設定、seedは揃え、共有の有無だけを変えます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {};
    using Action = int;
    std::array<double, 3> base{10, 12, 11};
    std::array<int, 3> actions(const State&) const { return {0, 1, 2}; }
    double step(State&, int action, MonteCarloNoise& noise) const {
        return base[action] + 4 * (noise.uniform01() - 0.5);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    for (bool use_crn : {false, true}) {
        Planner::Options options;
        options.seed = 42;
        options.use_crn = use_crn;
        Planner planner(model, options);
        planner.start(Model::State{}, 1);
        Planner::Limits limits;
        limits.max_simulations = 1000;
        auto run = planner.search(limits);
        auto result = planner.result();
        assert(run.simulations == 1000);
        std::cout << "crn=" << use_crn << " action=" << *result.action;
        for (const auto& candidate : result.candidates) std::cout << ' ' << candidate.trials;
        std::cout << '\n';
    }
}
```

CRNはCommon Random Numbers、共通乱数のことです。二つの配送経路を比べるとき、一方だけ晴天、もう一方だけ悪天候で評価されるより、同じ天候を使う方が差を見やすい場合があります。

試行の共有単位は、**根の候補ごとの第k試行**です。候補0の最初の試行と候補1の最初の試行は同じ試行seedを使い、二番目同士も同様です。番号は完了したときだけ進み、未完了なら同じseedと手数から再開します。

ONではこのseedから、残り手数と用途ごとに別の系列を作ります。`sample_root`、各手の`step`、`rollout_action`、既定の一様なrollout選択が共有の対象です。たとえば一手目で候補0が乱数を1個、候補1が17個使っても、二手目の遷移用の先頭は同じです。rollout側で多く引いても、同じ手の`step`の系列は変わりません。

ONでは、木へ保存する候補の`propose_action`は、現在の根の世代・残り手数・提案番号から系列を決めます。ノード番号を入れないため、同じ条件の子ノードで同じ第0提案、第1提案を生成できます。どの試行でその子を初めて訪れたかには依存しません。提案器を既定rolloutの代わりに呼ぶ場合は、保存候補用ではなく、その試行のrollout用系列を使います。

OFFでは、一つのRNGを保持し、初期状態の生成・候補提案・rollout・遷移で順に使い続けます。手数、用途、試行が変わるたびに作り直しません。`search`を中断・再開しても続きから引きます。その代わり、ある関数が余分に引いた乱数の個数は、後の関数にも影響します。v05以前と同じseedでも、OFFの結果は変わります。

| ON/OFFで変わるもの | ON/OFFで変えないもの |
|---|---|
| 試行・候補提案の乱数共有 | 候補を参加させるルール |
| ONでは対応位置ごとに初期化、OFFでは連続使用 | 評価配分の式とパラメータ |
| 乱数から得られる実際の観測得点 | 停止条件、回数の予算 |

各候補の評価回数を揃えたり、追い付き評価を追加したりしません。ONにしたから余分な試行を入れることもありません。例では両方とも合計1000試行ですが、候補ごとの試行数は変わり得ます。共有によって観測した得点が変わり、同じ適応的な選択ルールでも次に選ぶ候補が変わるためです。

同じ時間を与えたときの総試行回数も、ON/OFFで同じになる保証はありません。これは評価回数の調整ではなく、実行コストや処理経路が変わる結果です。

CRNは必須ではなく、既定はOFFです。常に差の推定が良くなる保証はありません。木の内部でも評価回数を揃える処理はありません。

**同じ系列を渡すことと、探索結果の完全一致は区別します。** ONで対応する試行番号、残り手数、用途、入力状態、モデルが同じで、関数が渡された乱数だけを使うなら、乱数の違いによる出力差をなくせます。一方、木の統計・候補の参加状況・合法手・状態が違えば、同じ乱数でも違う行動を選び得ます。隠れた外部RNGや状態、コールバック内で保持した分布オブジェクトのキャッシュも、この対応を崩します。1回のコールバック内で同じ出来事までの引く順序が変わる場合は、次の例の出来事キーを使います。

## 26. 発展㉒：乱数を「出来事」に結び付ける（例23）

### 何のための機能か：候補の差を、運の差に埋もれさせない

前章のCRNは、**候補をなるべく同じ偶然の条件で比べ、どちらが良いかを見分けやすくする**ための機能でした。この章は、その対応を、一つの評価関数の中にある「何日目の天候」などにも保つ方法です。

簡単な例として、候補Aの基本得点が10、Bが12で、好天なら両方に8点、悪天候なら両方から8点を引くとします。Aだけ好天、Bだけ悪天候だと18対4になり、運の差でAが良く見えてしまいます。同じ好天なら18対20、同じ悪天候なら2対4なので、どちらでもBの2点の良さが見えます。CRNは、こうした比較のばらつきを減らし、限られた試行を候補の違いの判定に役立てる狙いです。すべての問題で効く保証はありません。

ここからのコードでは、二つの設備の5日間の収益を比べます。設備0は丈夫で、設備1は生産性が高い一方、悪天候の日に修理費が発生します。**二つの設備に、同じ日の同じ天候を経験させたい**、という目的を先に置きます。

### 同じ乱数列を渡すだけでは、何がずれるのか

例01の`mc_choose`の評価関数を拡張し、一回の関数呼び出しの中で5日分を計算するとします。通常の`rng.uniform01()`で、天候も修理費も順番に引く書き方を考えます。

CRNがONなら、対応する候補の評価関数は同じ乱数列から始められます。しかし、最初の日が悪天候で、設備1だけ修理費を一個引くと、乱数の使い道は次のようになります。`u0`、`u1`、`u2`は、同じ系列から順に出る値の呼び名です。

| 同じ系列の値 | 設備0で使う先 | 設備1で使う先 |
|---|---|---|
| `u0` | 1日目の天候 | 1日目の天候 |
| `u1` | 2日目の天候 | 1日目の修理費 |
| `u2` | 3日目の天候 | 2日目の天候 |

2日目の天候は、設備0では`u1`、設備1では`u2`です。乱数列の先頭を揃えただけでは、同じ出来事に同じ値が届かなくなりました。個々の設備のシミュレーションが直ちに不正になるわけではありませんが、同じ天候で比較するというCRNの狙いが崩れます。

ライブラリはModelの別々の手・用途の間では系列を分けます。しかし、この例の5日間は一つの評価関数の中にあり、ライブラリにはどこが天候でどこが修理費か分かりません。そこで利用者が、その区別を番号で伝えます。**一つの`step`が一日を表し、各日で最初に天候を引くようなモデルなら、前章の自動的な対応だけで十分な場合があります。** その場合は、この追加APIを使う必要はありません。

### 解決方法：「何番目に引いたか」ではなく「何の出来事か」で指定する

この例では、`day=0`を1日目、`day=1`を2日目として、日付を出来事のキーにします。さらに同じ日の中でも、天候は`channel=0`、修理費は`channel=1`と分けます。channelは用途の番号です。

`context.uniform01(day,0,0)`は、**今回の試行における、その日の天候用の最初の値**を取り出す指定です。他の処理が途中で乱数を何個使っても、この指定から得る値は変わりません。最後の0は`draw_index`で、同じ出来事から複数の値を使う場合の番号です。

`context`は、この一試行を区別する基準seedを持つ、ライブラリから渡される追加引数です。CRN ONでは根候補の同じ第k試行に同じ基準が渡り、同じ日付・用途・番号から同じ値を得られます。各候補の試行番号が一つ進むとライブラリが別の基準を用意するので、毎回同じ5日間だけを評価するわけではありません。利用者が試行番号を数えたり、乱数の配列を保存したりする必要はありません。

### 動くコードと、追加した引数

```cpp
#include "monte_carlo_planner_v07.hpp"

int main() {
    std::array<int, 2> candidates{0, 1};
    auto action = mc_choose(candidates,
        [](int a, MonteCarloNoise&, const MonteCarloNoise& context) {
            double score = 0;
            for (uint64_t day = 0; day < 5; ++day) {
                double weather = context.uniform01(day, 0, 0);
                if (a == 1 && weather < 0.3) {
                    auto repair = context.stream(day, 1);
                    score -= 0.2 + repair.uniform01();
                }
                score += (a == 0 ? 1 : 1.5) * weather;
            }
            return score;
        }, 2000, true, 42);
    std::cout << *action << '\n';
    // 以下は探索とは独立した、乱数APIの小さな確認。
    static_assert(std::uniform_random_bit_generator<MonteCarloNoise>);
    MonteCarloNoise demo(123);
    std::cout << "seed=" << demo.scenario_seed() << " bits=" << demo()
              << " u=" << demo.uniform01() << " die=" << 1 + demo.uniform_int(6) << '\n';
    auto event = demo.stream(7, 0);
    double keyed = demo.uniform01(7, 0, 0);
    assert(event.uniform01() == keyed);
}
```

最初の`mc_choose`が設備を比較する本体です。これまでの評価関数に、三つ目の引数`const MonteCarloNoise& context`を追加しました。通常RNGの引数は残しますが、この例では乱数をすべて出来事から取得するため未使用にしています。`context`は読み取り専用で、通常の`context.uniform01()`のように逐次消費する使い方はしません。

| コード・パラメータ | 意味 |
|---|---|
| `candidates={0,1}` | 比較する二つの設備 |
| `a` | 今回評価する設備の番号 |
| `day<5` | 一回の評価で5日分を合計。これは`mc_choose`内部の5手ではなく、評価関数内のループ |
| `context.uniform01(day,0,0)` | その日の天候用の最初の値。0以上1未満 |
| `weather<0.3` | 天候値のうち30%の範囲を悪天候として扱う |
| `a==1` | 修理が必要なのは設備1だけ |
| `context.stream(day,1)` | その日の修理費専用の乱数生成器を作る |
| `repair.uniform01()` | 修理費用の系列から最初の実数を引く |
| `0.2 + repair.uniform01()` | 固定費0.2に、0以上1未満の追加修理費を足す |
| `(a==0 ? 1 : 1.5) * weather` | 設備1は、同じ天候で設備0の1.5倍の収益 |
| `2000` | 探索時間2000マイクロ秒。出来事キーのための追加試行ではない |
| `true` | CRNをONにし、候補間で対応する試行の基準を共有 |
| `42` | 探索全体の出発点となるseed |

修理があってもなくても、2日目の天候は常に`context.uniform01(1,0,0)`です。これで設備の得点差には「生産性」と「必要な修理費」が現れ、修理処理の都合で天候まで入れ替わることを避けられます。CRNは得点や修理の有無を同じにする仕組みではありません。揃えるのは、比較したい条件に共通する偶然です。

候補ごとの試行回数を揃える処理は追加しません。前章と同じく、共有により観測得点が変われば、その後の評価配分は変わり得ます。出来事キーの生成にも計算費用があるため、対応のずれが起きない単純な評価関数では、通常のRNGを使う方が簡単です。

### キーを決めるときの考え方

| 決めること | この例 | 間違えると起きること |
|---|---|---|
| 同じ外的条件を対応させる | 同じ日付の天候に同じdayとchannelを使う | 候補番号も天候キーへ入れると、候補間で天候を共有できない |
| 本来別の出来事を分ける | 天候0、修理費1という別チャネル | 同じ指定を流用すると、別用途にも同じ値を使う意図しない相関を作る |
| 時間の意味を揃える | 同じ暦日を同じdayにする | 到着日が違うのに「何手目」だけで対応させると、別の日の条件を揃えることがある |
| 同じ出来事から何個使うか決める | 天候は一個なのでdraw_index=0 | 同じ指定を繰り返しても、新しい値にはならない |

これは乱数を全部前もって作る機能でも、真の未来を予測する機能でもありません。試行の基準seedと番号から、対応する疑似乱数をその都度計算します。異なるキーが数学的な独立性を保証するものでもありません。

行動が変わると本来の確率分布も変わる場合は、その分布を保ってください。例えば需要の大きさが候補によって違うなら、同じ0〜1の乱数を各候補の分布へ変換できます。同じ最終的な需要値を無条件に押し付ける、という意味ではありません。

### OFFでも使えるが、contextと通常RNGを混同しない

CRN OFFでも追加contextと出来事キーは使えます。その場合も、同じ試行内の「日付・用途の対応」は安定します。ただし候補間で試行の基準を共有しないため、同じキーだけで天候が一致するわけではありません。

OFFの通常RNGは、根が同じ間は一つを使い続けます。その`rng.scenario_seed()`は試行が進んでも変わらず、`rng.stream(day,0)`を毎回作り直すと、別の試行でも同じ値を使ってしまいます。**試行ごとに変わる出来事の乱数には、ライブラリから渡された追加contextを使います。** 評価関数内で毎回`MonteCarloNoise(42)`を作る書き方も、試行ごとの変動を失うので置き換えにはなりません。

追加contextは`step`、`sample_root`、`rollout_action`、`propose_action`でも通常RNGの後へ付けられ、両形式があればcontext付きが優先されます。試行中の初期状態生成・遷移・継続方策には同じ試行の基準を渡します。そのため、複数の関数から同じキーを使えば値を共有できますが、別の出来事なら利用者がキーやチャネルを分けます。通常RNGの手数・用途の区別は、追加contextへ自動では付かないことに注意してください。

木へ保存する候補の`propose_action`だけは、試行用ではなく残り手数・提案番号ごとの基準です。保存候補の提案と試行中の天候を、同じキーだけで対応させる使い方はしません。通常RNGとcontextの参照はいずれも呼び出し中だけ使い、保存しません。

### コード末尾のAPI確認は、必要になったときに読む

末尾の`demo`は設備の評価とは独立した、小さなAPI確認です。seed123、出来事番号7は確認用の任意の値で、設備モデルの定数ではありません。`static_assert`は、この型がC++標準のURBGという乱数生成器の要件を満たすことを確認しています。URBGは一定範囲の整数を生成し、範囲の`min`・`max`を持つ型です。次の例で、ユーザー指定RNGとの接続に使います。

| API | 意味 |
|---|---|
| `MonteCarloNoise(123)` | seed123から系列を作る |
| `operator()()` | 次の64bit整数乱数 |
| `min()` / `max()` | 整数範囲の下限0と上限`UINT64_MAX` |
| `scenario_seed()` | オブジェクトを構築したseed。通常RNGと追加contextでは、その単位が異なる |
| `uniform01()` | 通常系列を進め、0以上1未満の実数を返す |
| `uniform_int(6)` | 0〜5の整数。`1+`でサイコロの1〜6にする |
| `stream(key, channel=0)` | 出来事用の`MonteCarloNoise`を返す。作った系列から続けて複数回引ける |
| `stream<Rng>(key, channel=0)` | 指定した型の乱数生成器を返す。例28で使う |
| `stream(key, channel, factory)` | 混合したseedを生成関数へ渡す。特別な構築方法が必要な場合の任意API |
| `uniform01(key, channel, draw_index)` | 指定した出来事の、指定番号の実数を直接返す |

`demo.stream(7,0)`から最初に引いた`uniform01()`と、`demo.uniform01(7,0,0)`は同じ値です。コードのassertはこの関係を確認します。どちらのキー付き操作も、元の`demo`の通常系列を進めません。同じキーでstreamを作り直すと先頭へ戻るため、続きが必要なら作ったstreamを使い続けるか、直接取得の`draw_index`を0、1、2と変えます。


## 27. 発展㉓：ユーザー指定の乱数生成器を使う（例24）

例05の生産モデルのまま、添付の`fast_rng.hpp`にある`FastRng`を直接使います。この例では`monte_carlo_planner_v07.hpp`と`fast_rng.hpp`を同じ場所へ置いてください。通常利用では後者のincludeは不要です。

```cpp
#include "monte_carlo_planner_v07.hpp"

#include "fast_rng.hpp"

struct Model {
    struct State { int turn = 0, level = 1; };
    using Action = int; // 0: 生産、1: 設備を改良
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    double step(State& state, int action, FastRng& rng) const {
        ++state.turn;
        if (action == 1) { ++state.level; return -2; }
        return state.level + double(rng.uniform(2));
    }
    int rollout_action(const State&, int, FastRng& rng) const {
        return rng.uniform(2);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model, FastRng>;
    Planner::Options options;
    options.seed = 42;
    options.use_crn = true;
    Planner planner(model, options);
    planner.start(Model::State{}, 6);
    Planner::Limits limits;
    limits.max_simulations = 1000;
    planner.search(limits);
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';

    // 同じRNG型を一回評価のヘルパーにも指定できる。
    auto choice = mc_choose<FastRng>(std::array{0, 1},
        [](int a, FastRng& rng) { return a + 0.1 * rng.uniform(); }, 2000, true, 42);
    std::cout << *choice << '\n';
}
```

### 型とseedを一か所で指定する

| 変更した箇所 | 意味 |
|---|---|
| `MonteCarloPlanner<Model, FastRng>` | 二つ目のテンプレート引数で全乱数コールバックのRNG型を指定 |
| `step(..., FastRng& rng)` | 遷移に使う実際の`FastRng`を直接受け取る |
| `rollout_action(..., FastRng& rng)` | 方策にも同じ型を使う。遷移とは別系列 |
| `options.seed=42` | 全系列の出発点。別途RNGオブジェクトを渡す必要はない |
| `options.use_crn=true` | 遷移とrolloutを対応する候補間で共有 |
| `rng.uniform(2)` | `FastRng`のAPIで整数0または1を生成 |
| `rng.uniform()` | `FastRng`のAPIで0以上1未満の実数を生成 |
| `mc_choose<FastRng>(...)` | 一回評価のヘルパーでも同じ型を直接渡す |

六手を見通し、一回1000試行の`search`を二度呼ぶので累計2000試行です。`start`を間に挟まず、統計と未完了の試行を継続できます。最後のヘルパー例は別の一手問題で、候補0と1を2000マイクロ秒、CRN ON、seed42で比較します。0.1倍の雑音を加えた得点を返しています。

RNG型は`std::uniform_random_bit_generator`を満たし、`uint64_t`のseedから構築できるものを指定します。同じseedから同じ状態で始まることも必要です。CRN OFFでは指定型のRNGを一つ保持し、全用途・全試行で順に使い続けます。`start`・`clear`・`advance`で新しい根の世代へ移るときに初期化し、`search`の中断・再開では初期化しません。`std::mt19937`、`std::mt19937_64`も使えます。**CRN ONでは手数・用途ごとにRNGを作る**ため、構築の重い型は非常に短い`step`で費用が目立ちます。ONでは軽い既定RNGまたは`FastRng`から始めるのが使いやすい形です。

乱数オブジェクトを渡す旧`search(limits,rng)`は廃止しました。RNG型は探索器に固定し、呼び出しは`search(limits)`へ揃えます。既存のシミュレータが`FastRng&`を要求する場合も、そのまま接続できます。後から学ぶ`sample_root`と、例20の`propose_action`も同じ`FastRng&`を受け取ります。

追加contextからも指定型を作れます。`context.stream<FastRng>(event_id, channel)`が通常の形です。特別な構築手順が必要なら`context.stream(event_id, channel, [](uint64_t seed) { return FastRng(seed); })`も同じseedを受け取ります。`event_id`は出来事、`channel`は用途の番号、生成関数の引数は混合済みseedです。この形は探索器全体のRNG型を変える操作ではありません。

同じURBGでも、標準の分布クラスがどう整数乱数を実数や範囲整数へ変換するかは処理系間で異なることがあります。処理系を越えた完全一致が必要なら、その変換方法も統一します。

## 28. 発展㉔：見えていない状態を分ける（例25）

二つの箱のどちらか一つに当たりがあり、当たりの箱を選ぶと10点です。今はどちらが当たりか分かりません。費用1を払えば中身を観測でき、その後でもう一手使って箱を選べます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int winner; };
    struct Info { int observed = -1; bool done = false; };
    using Action = int; // 0,1: 箱を選んで終了、2: 費用1で中身を観測
    State sample_root(const Info& info, MonteCarloNoise& noise) const {
        return {info.observed >= 0 ? info.observed : int(noise.uniform_int(2))};
    }
    void actions(const Info& info, int remaining, std::vector<Action>& out) const {
        out.push_back(0);
        out.push_back(1);
        if (info.observed < 0 && remaining >= 2) out.push_back(2);
    }
    double step(State& state, Info& info, int action, MonteCarloNoise&) const {
        if (action == 2) { info.observed = state.winner; return -1; }
        info.done = true;
        return action == state.winner ? 10 : 0;
    }
    bool terminal(const Info& info) const { return info.done; }
    template<class Rng>
    int rollout_action(const Info& info, int, Rng&) const {
        return info.observed >= 0 ? info.observed : 0;
    }
};

int main() {
    Model model;
    MonteCarloPlanner planner(model);
    planner.start(Model::Info{}, 2);
    decltype(planner)::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    std::cout << *planner.best_action() << '\n';
    assert(*planner.best_action() == 2);
}
```

### `State`と`Info`の区別

| 型 | 保持するもの | 本例 |
|---|---|---|
| `State` | 一回の試行で仮定した真の状態 | 当たりの箱`winner` |
| `Info` | 行動を選ぶ側が実際に知っている情報 | 観測した箱`observed`、終了済みか`done` |

`Model::Info`を定義すると、部分観測の形式へ切り替わります。`observed=-1`は未観測、0または1なら当たりを観測済みです。`done`は箱を選んで終了したかを表します。

`sample_root(info, noise)`は、現在分かっている情報に矛盾しない真の状態を、一試行分作ります。未観測なら0と1を半々で仮定し、観測済みなら観測値をそのまま使います。

`step`は、完全観測版より引数が一つ増え、`step(State&, Info&, Action, Rng&)`になります。環境の実際の変化を`State`で表し、観測できた内容だけを`Info`へ反映します。

行動2なら-1点を払い、当たりを`Info::observed`へ書きます。行動0か1なら終了し、仮定した当たりと一致した場合に10点を返します。観測後の継続方策は`Info`を見て当たりの箱を選べます。

未観測で直接箱を選ぶ期待得点は5です。観測してから正しく選べば-1+10=9です。したがって、この2手問題では観測に価値があります。実行例では行動2を推薦します。

### 見えない情報を行動選択へ漏らさない

行動を決める`actions`、`propose_action`、`rollout_action`と、判断点を分ける`tree_key`へ渡すのは`Info`だけです。一方、結果を採点する`leaf_value`と`finish_value`は、`State`と`Info`を両方受け取れます。採点結果は統計に使いますが、方策へ隠れた真値を引数として渡すわけではありません。従来の`leaf_value(Info,remaining)`と`finish_value(State[,reason])`も使えます。新しい形は例30・31で使います。

ただし、モデルの共有メンバーへ`sample_root`の仮定を書き込み、それを継続方策で読むようにすれば、利用者自身が情報を漏らせてしまいます。隠れた試行状態は`State`へ置き、行動を決める関数では観測済み情報だけを使う、という設計を守ります。

`Info`は最後の観測だけで十分とは限りません。過去の観測によって将来の分布が変わるなら、その履歴や十分な推定量も保持します。部分観測を指定するだけで、自動的にベイズ推定や粒子の重み更新が行われるわけではありません。

### どの時点の情報が渡されるか

完全観測の例05では、根Stateをそのまま試行用Stateへコピーしました。部分観測では、出発点として保持するのは根Infoです。真の状態は分からないため、試行を始めるたびに`sample_root`で仮定したStateを作り、Infoも試行用にコピーします。**一試行の中ではStateとInfoを一組で使い続け、次の試行には引き継ぎません。**

| 場面 | StateとInfoの扱い |
|---|---|
| `start(root, horizon)` | 実際に分かっているrootを根Infoとしてコピー・保持 |
| 根の候補生成・選択 | 根Infoで決める。この試行の隠れたStateはまだ作らない |
| `sample_root` | 読み取り専用の根InfoとRNGから、新しい試行用Stateを返す |
| 試行開始 | 根Infoのコピーを、この試行用のInfoにする |
| `step` | 現在のStateとInfoを更新。Infoには、その行動で観測できた内容だけを書く |
| 後続の候補生成・継続方策・キー | 同じ試行で更新済みのInfoを読む。新しい観測は次の行動に使える |
| `leaf_value`・`finish_value` | 打ち切り・終了地点のStateとInfoを読み取り専用で受け、数値を返す |
| 試行完了 | 採点を統計へ反映し、試行用State・Infoは破棄。実際の根Infoは変更しない |
| 次の試行 | 同じ根Infoを起点に、再びStateをサンプルする。前の試行の観測は持ち込まない |
| `search`の中断・再開 | 未完了の同じ試行なら、更新済みState・Infoを保持して続ける |
| 実ターン後の`start`・`advance` | 利用者が実際の観測から作った次Infoを渡す。部分観測では統計をリセット |
| `clear`・探索器の破棄 | 根Infoと未完了の試行のState・Infoを破棄 |

`step`が返した報酬は自動でInfoへ書かれません。実際に報酬を観測でき、推定に使う問題なら、利用者が`step`内でInfoにも反映します。採点用にだけ使える報酬なら、方策へ漏らしません。参照の寿命もコールバック内に限って使い、参照やポインタをモデルへ保存して次の試行から読まないでください。`const`は深いコピーや漏洩防止の仕組みではないため、Info内のポインタが隠れたStateを指す設計にも注意します。

`sample_root`は、新しく始まる各試行につき一度です。次の手へ進むたび、観測を得るたび、または`search`を再開するたびに呼び直すものではありません。試行中に新しい観測を得たら、同じ仮定のStateに対してInfoを更新し、以後の方策がそのInfoを使います。根候補がなく試行を始められなければ、`sample_root`も呼びません。

完全観測と同様、Model本体は全試行で共有され、`start`や`clear`でそのメンバーが初期化されることはありません。粒子集合などの推定結果をModelに置く場合、根での推定結果として探索中は固定します。試行中に仮定した観測を、その共有粒子集合へ直接反映して他試行へ持ち込むのは避けます。試行内で更新が必要な情報は、その試行のInfoなどへ保持します。実際の観測で推定結果を変える場面は、次の例26で扱います。

### 部分観測の呼び出し順を、一つの試行で追う

例25で「観測する」という根の行動2が選ばれた試行を考えます。`sample_root`が当たり1を仮定したとすると、呼び出しとデータの流れは次の順です。例25は`tree=false`、浅い打ち切りなしです。

| 順番 | 呼び出し・処理 | その時点での状態と結果 |
|---:|---|---|
| 1 | `start(Info{},2)` | 未観測のInfoを根へコピー。Stateはまだ作らない |
| 2 | `search`で根の`terminal(info)` | 根の`done=false`を確認。予算確認が先なので、予算0ならここまで来ない |
| 3 | 必要なら根の`actions(info,remaining,out)`、根の一手を選択 | 初回は0・1・2を登録。ここでは既知情報だけで行動2を選んだとする |
| 4 | `sample_root(root_info,rng)` | 今回の仮定として`State{winner=1}`を返す |
| 5 | 根Infoを試行用へコピー | 試行用Infoも最初は`observed=-1, done=false` |
| 6 | `step(state,info,2,rng)` | 観測して`info.observed=1`。報酬は−1 |
| 7 | `terminal(info)`、手数上限・浅い打ち切りを判定 | 終端ではなく残り1手。Infoは観測済みのまま保持 |
| 8 | `rollout_action(info,1,rng)` | 観測済みの1を返す。Stateを直接参照しない |
| 9 | `step(state,info,1,rng)` | 当たりの箱を選び10点。`info.done=true` |
| 10 | `terminal(info)` | true。このモデルは`finish_value`を省略しているので終了時加算は0 |
| 11 | 合計9点を統計へ反映 | 試行用State・Infoを破棄。根Infoは依然として未観測 |
| 12 | 予算があれば次の試行を開始 | 根の一手を選び、根Infoから`sample_root`をやり直す |

根で候補がすでに保存されていれば、3の`actions`は呼びません。また、最初から箱0・1を選ぶ試行なら6に相当する最初の`step`で終わり、`rollout_action`は呼びません。したがって、すべての試行が同じメソッド列になるわけではありません。

一般の部分観測モデルでも、順序は「既知情報で根の行動を選ぶ → StateをサンプルしInfoをコピー → stepで両方を更新 → 更新後のInfoで終了判定または次の行動選択」です。採点関数を定義した場合は、終了時に`finish_value`、浅い打ち切り時に`leaf_value`を呼びます。両方を同じ完了時点で足すことはありません。各形式でどの引数を受け取れるかは付録A.3、木探索を含む順序は付録A.7を参照してください。

たとえば6の後で探索予算が尽きても、今回の`winner=1`と`observed=1`を保持して止まります。再開したら同じ試行を続け、`sample_root`で別の当たりへ取り替えません。試行準備の直後に時間切れになれば、最初の`step`の前で止まる場合もあります。

### 終了判定はなぜInfoだけか

`terminal(const Info&)`は「現在の情報で、もう行動が不要だと分かるか」です。試行中だけでなく、実際の根に対する`search`・`result`・`best_action`でも使います。結果を取り出すだけで隠れたStateをサンプルしたり、そのサンプル次第で推薦を消したりしないため、Infoだけの形式です。

終了が観測できるなら、例25の`done`のように`step`でInfoへ反映します。逆に、内部では壊れていて以後変化しないが、まだそれを観測できない状況なら、`step`でStateの吸収状態（その後変化しない状態）を表します。問題の定義に応じて追加報酬0などを返し、観測または手数上限まで進めます。シミュレーションを短くするためだけに、見えていない終了をInfoへ入れないでください。

## 29. 発展㉕：推定結果を借用し、観測後に再計画する（例26）

真の状態の仮定を、外部の粒子集合から作るようにします。「粒子」は、あり得る状態の候補です。さらに、観測情報のキーを与えて木探索をONにします。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int winner; };
    struct Info { int observed = -1; bool done = false; };
    using Action = int; // 0,1: 箱を選んで終了、2: 費用1で中身を観測
    const std::vector<int>& particles;
    State sample_root(const Info& info, MonteCarloNoise& noise) const {
        return {info.observed >= 0 ? info.observed : particles[noise.uniform_int(particles.size())]};
    }
    void actions(const Info& info, int remaining, std::vector<Action>& out) const {
        out.push_back(0);
        out.push_back(1);
        if (info.observed < 0 && remaining >= 2) out.push_back(2);
    }
    double step(State& state, Info& info, int action, MonteCarloNoise&) const {
        if (action == 2) { info.observed = state.winner; return -1; }
        info.done = true;
        return action == state.winner ? 10 : 0;
    }
    bool terminal(const Info& info) const { return info.done; }
    template<class Rng>
    int rollout_action(const Info& info, int, Rng&) const {
        return info.observed >= 0 ? info.observed : 0;
    }
    auto tree_key(const Info& info) const { return std::pair(info.observed, info.done); }
};

int main() {
    std::vector<int> particles{0, 1};
    Model model{particles};
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.tree = true;
    Planner planner(model, options);
    Model::Info observed;
    planner.start(observed, 2);
    Planner::Limits limits;
    limits.max_simulations = 2000;
    planner.search(limits);
    auto first = planner.result(false);
    assert(first.action && *first.action == 2);
    Model::State actual{1}; // 本番では未知。ここでは環境の実行を模擬するために置く
    MonteCarloNoise actual_noise(999);
    model.step(actual, observed, *first.action, actual_noise);
    std::erase_if(particles, [&](int winner) { return winner != observed.observed; });
    assert(!particles.empty());
    planner.start(observed, 1);
    planner.search(limits);
    std::cout << *first.action << ' ' << *planner.best_action() << '\n';
    assert(*planner.best_action() == 1);
}
```

`particles={0,1}`は、当たりが0と1の仮説を一つずつ持つ、重みが等しい粒子集合です。`sample_root`はその中から一つを選びます。粒子フィルターなどが別途推定した集合を借用する場合も、この接続部分を置き換えます。重み付き粒子なら、等確率でなく重みに従って選ぶ必要があります。

`const std::vector<int>& particles`で借用するため、試行のたびに粒子集合全体をコピーしません。粒子集合はモデルと探索器より長く生存し、探索中に変更しないようにします。

`tree_key`は観測済み情報だけから作ります。この例では、未観測・0を観測・1を観測という違いと、終了済みかどうかで十分です。固定されたモデルの同じ観測情報なら、未来の意思決定に必要な情報が同じだからです。

後半では、教材用の実環境として当たり1を置き、選ばれた「観測」行動を実行します。ここで初めて、実際の観測を外側の`observed`へ反映します。続いて、矛盾する粒子0を除きます。本例は完全に正確な観測なので削除できますが、観測に誤りがあるなら、単純に一致しない粒子を全削除する更新は不適切です。

粒子集合を変えた後は、`start(observed,1)`で再計画します。変更前の推定分布に対する統計は引き継ぎません。部分観測モデルでは、`advance`を呼んでも統計の再利用は行わず、指定した次の根へリセットする仕様です。

本番で真の状態が未知なら、`actual`は利用者の意思決定コードにはありません。環境から返された観測を読み、その観測だけから`Info`と推定分布を更新します。ここでは一つのプログラムで最後まで動かすため、環境役も同じファイルに置いています。

## 30. 発展㉖：テスト用の時計に差し替える（例27・任意）

通常は既定の`std::chrono::steady_clock`を使います。時間による停止を、実時間を待たずにテストしたい場合に限り、時計型を差し替えられます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State {};
    using Action = int;
    std::array<double, 3> base{10, 12, 11};
    std::array<int, 3> actions(const State&) const { return {0, 1, 2}; }
    double step(State&, int action, MonteCarloNoise& noise) const {
        return base[action] + 4 * (noise.uniform01() - 0.5);
    }
};

struct TestClock {
    using rep = int64_t;
    using period = std::micro;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<TestClock>;
    static constexpr bool is_steady = true;
    inline static rep ticks = 0;
    static time_point now() { return time_point(duration(ticks++)); }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model, MonteCarloNoise, TestClock>;
    Planner::Options options;
    options.clock_interval = 1;
    Planner planner(model, options);
    std::array<int, 3> candidates{0, 1, 2};
    planner.start(Model::State{}, 1, candidates);
    Planner::Limits limits;
    limits.deadline = TestClock::now() + TestClock::duration(20);
    auto run = planner.search(limits);
    assert(run.stop == Planner::Stop::time);
    std::cout << "completed=" << run.simulations << " fake_us=" << run.elapsed_us << '\n';
}
```

`MonteCarloPlanner<Model, MonteCarloNoise, TestClock>`の三つ目のテンプレート引数が時計型です。二つ目は例24で学んだRNG型で、ここでは既定の`MonteCarloNoise`を明記します。`TestClock`は、`now()`を呼ぶたびに時刻を1マイクロ秒進める、テスト用の時計です。実際の経過時間を測るものではありません。

| 定義 | 役割 |
|---|---|
| `rep=int64_t` | 時間の数値を保持する型 |
| `period=std::micro` | 一目盛りを1マイクロ秒として扱う |
| `duration` | 時間の長さ |
| `time_point` | この時計の時刻 |
| `ticks` | テストが管理する現在の目盛り |
| `now()` | 現在時刻を返す。ここでは呼び出しごとに進める |

`limits.deadline`も同じ時計型の時刻でなければなりません。実時間の`steady_clock::time_point`と混ぜません。例の20は仮想時計上の20マイクロ秒で、実行速度の測定には使えません。普通のAHC提出で、この時計をそのまま使う必要はありません。

## 31. 発展㉗：部分観測・候補提案にも指定RNGを使う（例28）

例24のRNG指定を、例25の部分観測と例20の逐次提案へ広げます。需要の真の値は見えず、三手の生産量を決める小さな問題です。1手の損失は、生産量とその日の需要の差の二乗です。ライブラリは最大化するため、その負数を返します。

```cpp
#include "monte_carlo_planner_v07.hpp"

#include "fast_rng.hpp"

struct Model {
    struct State { double demand; };
    struct Info { int turn = 0; };
    using Action = double;
    State sample_root(const Info&, FastRng& rng) const {
        return {0.4 + 0.2 * rng.uniform()};
    }
    std::optional<double> propose_action(const Info&, int, int index, FastRng& rng) const {
        if (index == 8) return std::nullopt;
        return index == 0 ? 0.5 : rng.uniform();
    }
    double rollout_action(const Info&, int, FastRng& rng) const {
        return 0.4 + 0.2 * rng.uniform();
    }
    double step(State& state, Info& info, double amount, FastRng&,
                const MonteCarloNoise& context) const {
        auto event = context.stream<FastRng>(info.turn, 7);
        double target = state.demand + 0.1 * (event.uniform() - 0.5);
        ++info.turn;
        return -(amount - target) * (amount - target);
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model, FastRng>;
    Planner::Options options;
    options.use_crn = true;
    options.seed = 42;
    Planner planner(model, options);
    planner.start(Model::Info{}, 3);
    Planner::Limits limits;
    limits.max_simulations = 2000;
    auto run = planner.search(limits);
    assert(run.simulations == 2000 && run.transitions == 6000);
    std::cout << *planner.best_action() << '\n';
}
```

| 追加・変更した箇所 | 意味 |
|---|---|
| `State::demand` | 一試行で仮定した需要。方策から直接は見えない |
| `Info::turn` | 観測可能な手数。初めは0、`step`ごとに1増やす |
| `sample_root(..., FastRng&)` | 需要を0.4以上0.6未満で仮定。これもCRN対象 |
| `Action=double` | 生産量を実数で表現 |
| `propose_action(..., index, FastRng&)` | 最初は0.5、以降は0以上1未満から提案。index8で打ち切るので最大8候補 |
| `rollout_action(..., FastRng&)` | 後続手は0.4以上0.6未満から選ぶ簡単な方策 |
| `step(..., FastRng&, const MonteCarloNoise& context)` | 通常RNGの代わりに、任意の出来事キーを使う形 |
| `context.stream<FastRng>(info.turn, 7)` | 現在の手数を出来事キー、7を需要変動用のチャネルとして指定 |
| `0.1 * (event.uniform()-0.5)` | 需要に−0.05以上0.05未満の変動を加える |
| `start(..., 3)` | 三手分の報酬を比較 |
| `max_simulations=2000` | 一回の探索で2000試行を完了。終端のない三手問題なので6000遷移 |

チャネル7に特別な意味はありません。同じ試行の別用途の出来事には別の番号を使います。ここでは一日一回だけ需要変動を作るので、通常の`rng.uniform()`を使ってもよいモデルです。追加contextの接続を学ぶためにキー付きの形にしています。四つのコールバックすべてに`FastRng&`が渡り、追加contextを要求した`step`だけはさらにその引数も受け取ります。

この例もCRNをOFFにして使えます。初期需要の仮定、逐次提案、継続方策という機能はそのままで、候補間の乱数共有だけがなくなります。見えない需要そのものを`Info`へ入れて方策に読ませてはいけません。

## 32. 発展㉘：rolloutと遷移の共有を確認する（例29）

最後に、例22のON/OFF比較を三手へ広げます。ここは解品質の例ではなく、共有の動作を見るための教材です。二つの根行動にラベル0・1を付け、返す得点を常に0にします。状態のラベルはログの行き先にだけ使い、後続の状態や得点には影響させません。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int turn = 0, label = -1; };
    using Action = int;
    std::array<std::vector<uint64_t>, 2> policy_draws, step_draws;
    std::array<int, 2> actions(const State&) const { return {0, 1}; }
    int rollout_action(const State& state, int, MonteCarloNoise& rng) {
        uint64_t value = rng();
        policy_draws[state.label].push_back(value);
        return int(value % 2);
    }
    double step(State& state, int action, MonteCarloNoise& rng) {
        if (state.turn == 0) state.label = action;
        step_draws[state.label].push_back(rng());
        // 片方だけ余分に引く。捨てる値なので問題の状態・得点には影響しない。
        for (int j = 0; j < (state.label == 0 ? 1 : 17); ++j) (void)rng();
        ++state.turn;
        return 0;
    }
};

int main() {
    using Planner = MonteCarloPlanner<Model>;
    for (bool crn : {false, true}) {
        Model model;
        Planner::Options options;
        options.use_crn = crn;
        Planner planner(model, options);
        planner.start(Model::State{}, 3);
        Planner::Limits limits;
        limits.max_simulations = 100;
        auto run = planner.search(limits);
        assert(run.simulations == 100 && run.transitions == 300);
        auto result = planner.result();
        assert(result.candidates[0].trials == 50 && result.candidates[1].trials == 50);
        bool same_policy = model.policy_draws[0] == model.policy_draws[1];
        bool same_step = model.step_draws[0] == model.step_draws[1];
        if (crn) assert(same_policy && same_step);
        std::cout << "crn=" << crn << " policy=" << same_policy << " step=" << same_step << '\n';
    }
}
```

| 追加した要素 | 役割 |
|---|---|
| `policy_draws[0/1]` | 各根候補のrolloutで生成した生の64bit値を記録 |
| `step_draws[0/1]` | 各根候補の遷移で最初に生成した値を記録 |
| `value%2` | この教材のrolloutで0または1を選択 |
| 1回対17回の余分な呼び出し | 乱数の消費数だけを変える。値は捨てる |
| `max_simulations=100` | ON/OFFとも同じ100試行・300遷移 |
| `same_policy` / `same_step` | 二つのログが要素ごとに同じかを確認 |

ONでは両方が1になります。最初の手で乱数を余分に捨てても、後続rolloutの系列や次の手の遷移系列はずれません。OFFの出力はこの固定設定では両方0です。ただし一般にOFFで値の偶然の一致が一度も起きないという保証ではありません。

この例で各候補が50回になるのは、得点が常に同じで、通常の配分ルールが未評価・少評価の候補を選ぶためです。CRNのための回数合わせを追加した結果ではありません。本来の問題で得点が異なれば配分は偏り得ます。木の候補提案は別の単位で共有するため、実装付録B.9の表も確認してください。

## 33. 発展㉙：真の状態も使って浅い評価をする（例30）

例25の箱を簡単にし、「1手目で箱を決め、2手目で代金を受け取る」問題にします。観測する行動はなく、一度決めた箱は変えられません。当たりなら10点、外れなら0点です。先に1手だけシミュレーションし、まだ受け取っていない得点を`leaf_value`で評価します。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int winner; };
    struct Info { int turn = 0, chosen = -1; };
    using Action = int;
    State sample_root(const Info&, MonteCarloNoise& rng) const {
        return {int(rng.uniform_int(2))};
    }
    std::array<int, 2> actions(const Info&) const { return {0, 1}; }
    void step(State&, Info& info, int action, MonteCarloNoise&) const {
        if (info.turn == 0) info.chosen = action;
        ++info.turn;
    }
    bool terminal(const Info& info) const { return info.turn == 2; }
    double leaf_value(const State& state, const Info& info, int remaining) const {
        assert(info.turn == 1 && remaining == 1);
        return state.winner == info.chosen ? 10 : 0;
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.simulation_depth = 1;
    options.seed = 42;
    Planner planner(model, options);
    planner.start(Model::Info{}, 2);
    Planner::Limits limits;
    limits.max_simulations = 4000;
    auto run = planner.search(limits);
    assert(run.simulations == 4000 && run.transitions == 4000);
    std::cout << "chosen=" << *planner.best_action() << '\n';
    for (const auto& candidate : planner.result().candidates)
        std::cout << "mean=" << *candidate.mean << '\n';
}
```

| 定義・パラメータ | 意味 |
|---|---|
| `State::winner` | この試行で仮定した当たり。行動を決める側には見えない |
| `Info::turn=0` | 進めた手数。誰でも知ってよい情報 |
| `Info::chosen=-1` | まだ箱を決めていない。決めた後は0か1 |
| `sample_root`の`uniform_int(2)` | 当たり0と1を同じ確率でサンプル |
| `actions`の`{0,1}` | 最初に選べる箱。2手目の番号は無視され、決定済みの箱は変えない |
| `step` | 最初だけ選んだ箱をInfoへ保存し、手数を増やす。`void`なので一手報酬は0 |
| `terminal`の`turn==2` | 2手目の受け取りで終了する、観測可能な条件 |
| `leaf_value(const State&, const Info&, int remaining)` | 今の真の状態と、すでに決めた箱から残りの得点を評価 |
| `remaining` | 本来の上限までの残り手数。本例では2−1=1。assertで前提を確認 |
| `simulation_depth=1` | 一手で浅く打ち切り、上の採点関数を使う |
| `seed=42` | 試行列を再現するための初期値。CRNは既定のOFF |
| `start(Info{},2)` | まだ箱を決めていない根から、本来は2手の問題として比較 |
| `max_simulations=4000` | 完了試行を4000回実行。浅い打ち切りにより遷移も4000回 |

ここで`leaf_value`が返す10または0は、「この試行の当たりに対して、すでに選んだ箱はいくらの価値か」です。Stateは根でサンプルした現在の真の状態、Infoは`step`で更新された現在の情報です。一般のモデルではStateも`step`で変化するので、どちらも根のコピーのままとは限りません。

箱0も箱1も本来の期待値は5です。試行平均はその近くになりますが、有限回の乱数なのでどちらを推薦するかは変わり得ます。Infoだけから期待値5を直接計算できるなら、従来の`leaf_value(const Info&, int)`で5を返す方がサンプルのばらつきを減らせます。一方、大きな隠れた状態から結果を簡単に採点でき、期待値を直接計算しにくい問題では、Stateも受け取る形が役立ちます。どちらを使うかはモデル側で選べます。

### 採点に使ってよいことと、行動に使ってはいけないこと

この例では、箱を決める時点で当たりは見ていません。その後に当たりと照合して採点するのは正当です。逆に、`leaf_value`内で「当たりの箱をこれから選ぶことにする」と常に10を返すと、未観測の情報を使える前提になって過大評価です。浅い地点から先で行動を仮定するなら、その行動もInfoだけで実行可能な方策にしてください。完全情報なら達成できる値を、意図して楽観的な近似に使う場合は、その偏りを把握して品質を確かめます。

State＋Infoの3引数形と従来のInfoだけの2引数形が両方あれば、3引数形を使います。Stateだけで採点したい場合も3引数にし、Infoを未使用にします。部分観測のStateだけの2引数形は追加していません。同じ型や似た型のState・Infoを使うモデルで、既存の2引数形の意味を取り違えないためです。

## 34. 発展㉚：終了時にも両方の情報で採点する（例31）

例30を、本来の2手目まで進める形にします。`simulation_depth`を0へ変え、終了時の得点を`finish_value`へ追加するだけです。浅い評価の定義は残せるので、深さの設定を変えて使い分けられます。

```cpp
#include "monte_carlo_planner_v07.hpp"

struct Model {
    struct State { int winner; };
    struct Info { int turn = 0, chosen = -1; };
    using Action = int;
    State sample_root(const Info&, MonteCarloNoise& rng) const {
        return {int(rng.uniform_int(2))};
    }
    std::array<int, 2> actions(const Info&) const { return {0, 1}; }
    void step(State&, Info& info, int action, MonteCarloNoise&) const {
        if (info.turn == 0) info.chosen = action;
        ++info.turn;
    }
    bool terminal(const Info& info) const { return info.turn == 2; }
    double leaf_value(const State& state, const Info& info, int remaining) const {
        assert(info.turn == 1 && remaining == 1);
        return state.winner == info.chosen ? 10 : 0;
    }
    double finish_value(const State& state, const Info& info,
                        MonteCarloFinishReason reason) const {
        if (reason != MonteCarloFinishReason::terminal) return 0;
        return state.winner == info.chosen ? 10 : 0;
    }
};

int main() {
    Model model;
    using Planner = MonteCarloPlanner<Model>;
    Planner::Options options;
    options.simulation_depth = 0;
    options.seed = 42;
    Planner planner(model, options);
    planner.start(Model::Info{}, 2);
    Planner::Limits limits;
    limits.max_simulations = 4000;
    auto run = planner.search(limits);
    assert(run.simulations == 4000 && run.transitions == 8000);
    std::cout << "chosen=" << *planner.best_action() << '\n';
    for (const auto& candidate : planner.result().candidates)
        std::cout << "mean=" << *candidate.mean << '\n';
}
```

変更したパラメータは`simulation_depth=0`です。これは「0手で打ち切る」ではなく「浅い打ち切りをしない」指定です。2手×4000試行で8000遷移になります。

追加した`finish_value(const State&, const Info&, MonteCarloFinishReason reason)`では、仮定した当たりと決定済みの箱を受け取ります。`reason==terminal`なら2手目の受け取りを終えたので10または0を返し、それ以外は0とします。本例の上限2では毎回`terminal`です。上限を1へ短くした場合は、まだ受け取っていないため`horizon`で0、というこのモデルの採点定義になります。打ち切り時にも未受取価値を加えたい問題なら、その定義に合わせて変えます。

`terminal`と手数上限が同時に成立すると`terminal`が優先です。`leaf_value`と`finish_value`を同じ試行で両方足すことはありません。ここでは`step`の報酬が0で、`discount=1`のままなので、最後の10または0が試行全体の得点です。

終了理由で処理を変えないなら、`finish_value(const State&, const Info&)`という短い形も使えます。選択順は「State＋Info＋reason → State＋Info → State＋reason → State」です。定義しなければ0です。StateとInfoに同じデータを重複して持たせず、それぞれの現在の情報から採点できます。

## 35. 使い始めるときの選び方

| 状況 | 最初に使う形 |
|---|---|
| 候補と一回の評価関数がある | `mc_choose`。例01 |
| 評価回数・統計・追加探索が必要 | 一手の`MonteCarloPlanner`。例03〜04 |
| 複数ターンの先読みが必要 | `State`と`step`を定義。例05 |
| 良い貪欲方策がある | `rollout_action`へ接続。例09 |
| シミュレーションが長い | `leaf_value`と浅い打ち切りを品質比較。例11、部分観測なら例30 |
| 同じ将来状態を何度も訪れる | `tree=true`を比較。例16 |
| 実行した一手の先を続けて考える | 条件を確認して`advance`。例18 |
| モデルや推定分布が変わった | `start`で新しい比較。例19・26 |
| 候補が多い・連続値 | `propose_action`。例20 |
| 候補間で同じ外乱を使いたい | 任意でCRNとキー付き乱数。例22〜23 |
| 見えない状態がある | `State`と`Info`を分離。例25 |

すべての機能を一度に使う必要はありません。手元の問題で意味がある最小の形から始め、同じ時間での品質を見ながら追加します。特に、木探索、浅い打ち切り、CRN、独自RNGは、それぞれ独立に必要性を判断できます。

## 付録A. APIと契約の一覧

### A.1 公開される型

| 型 | 役割 |
|---|---|
| `MonteCarloFinishReason` | `terminal`と`horizon`という終了理由 |
| `MonteCarloNoise` | 既定の乱数型と、出来事ごとの乱数生成context |
| `MonteCarloPlanner<Model, Rng, Clock>` | 探索器。`Rng`と`Clock`は省略可能 |
| `Planner::State` / `Info` / `Action` | モデルの型。`Info`未定義なら`State`が`Info`になる |
| `Planner::Options` | 探索器の構築時設定 |
| `Planner::Limits` | 一回の`search`の予算 |
| `Planner::ActionId` | 現在の根に属する行動ID。等値比較可能 |
| `Planner::RunStats` | 直前の実行統計 |
| `Planner::Candidate` | 行動ID・試行数・平均値 |
| `Planner::Result` | 推薦・状態・暫定フラグ・候補統計・直前の実行統計 |
| `Planner::Status` | `ready`、`not_ready`、`terminal` |
| `Planner::Stop` | `none`、`time`、`simulations`、`transitions`、`not_ready`、`terminal` |

`Stop::none`は、構築後やリセット後など、まだ`search`の停止理由がない状態です。`Result::id`は`action`が存在するときだけ行動IDとして使います。

### A.2 `Options`の全項目

| 項目 | 既定値 | 契約・作用 | 主な例 |
|---|---:|---|---|
| `seed` | 1 | 内部乱数の初期値。`uint64_t` | 02、22、24 |
| `use_crn` | false | 試行・候補提案の乱数共有 | 22、28、29 |
| `tree` | false | 木を保存する。情報のキーの等値比較が必要 | 16 |
| `simulation_depth` | 0 | 0は完走。正数なら浅い打ち切り深さ | 11、30 |
| `discount` | 1 | 0以上1以下。一手ごとの割引 | 07 |
| `exploration` | 1 | 0以上。UCB型の探索項の強さ | 20 |
| `value_scale` | 0 | 0なら自動推定。正数なら得点尺度を固定 | 20 |
| `max_nodes` | 32768 | 1以上。根を含むノード上限 | 21 |
| `max_edges` | 131072 | 1以上。全保存行動の上限 | 21 |
| `widening` | 2 | 0以上。0は毎回最大一候補を追加参加 | 20 |
| `clock_interval` | 8 | 1以上。通常時の時計確認の遷移間隔 | 12 |

### A.3 モデルのコールバック

表の`Rng`は探索器の二つ目の型引数です。乱数を取る4種類の関数は、その末尾に任意で`const MonteCarloNoise& context`を付けられます。両形があればcontext付きが優先です。表の`Info`は、完全観測なら`State`を意味します。引数の`Action`は例のように値で受けても、小さくない型なら`const Action&`で受けても構いません。

| メソッド | 役割と省略時 |
|---|---|
| `step(State&, Action, Rng&)` | 完全観測の遷移。数値なら一手報酬、`void`なら0 |
| `step(State&, Info&, Action, Rng&)` | 部分観測の遷移。`Info`も更新 |
| `sample_root(const Info&, Rng&)` | 部分観測で必須。試行用の`State`を返す |
| `actions(const Info&)` | sized rangeの候補集合を返す簡易形 |
| `actions(const Info&, int, vector<Action>&)` | 空にした再利用バッファへ候補を書く。簡易形より優先 |
| `rollout_action(const Info&, int, Rng&)` | 一手の継続方策。省略時は候補から一様選択等 |
| `propose_action(const Info&, int, int, Rng&)` | 逐次候補提案。`optional<Action>`を返す |
| `terminal(const Info&)` | 終端判定。省略時はfalse |
| `finish_value(const State&)` | 終了時加算。省略時は0 |
| `finish_value(const State&, MonteCarloFinishReason)` | 終了理由付き。理由なし版より優先 |
| `leaf_value(const Info&, int)` | 浅い打ち切り以降の価値。実際に浅く打ち切るなら必須 |
| `leaf_value(const State&, const Info&, int)` | 部分観測のみ。両方の現在値で近似採点。Infoだけの形より優先 |
| `finish_value(const State&, const Info&)` | 部分観測のみ。両方の現在値で終了時採点。従来のStateだけの形式より優先 |
| `finish_value(const State&, const Info&, MonteCarloFinishReason)` | 部分観測のみ。終了時採点の最優先形 |
| `tree_key(const Info&)` | 木のキー。省略時は`Info`そのもの |

採点と`sample_root`に渡すState・Infoは読み取り専用です。変更可能な参照は`step`で使います。採点関数はRNGを受け取らず、与えられた現在値から採点します。ランダムな将来をサンプルする必要があるなら遷移として進めるか、浅い地点で残りの期待値を計算します。採点の中で別の共有RNGを消費すると、CRNの保証から外れます。非常に広いテンプレートで何でも受け取る関数を書く場合も、上記の優先順で判定されるため、意味の異なる引数を誤って受けないよう型を制約してください。

v06から移す場合、文書どおりconst参照で定義した既存形式はそのまま使えます。以前の実装で偶然呼べていた非const参照の`sample_root`や採点関数は、読み取り専用の形へ直してください。v07では実際にconst参照を渡します。完全観測の採点形式は従来のままです。

`actions`は、外部から根候補を与え、必要な後続行動も別途定義しているなら省略できます。何も生成しない非終端の根では推薦がなく、途中の非終端状態で合法な継続行動が得られないモデルは契約違反です。

### A.4 候補生成と継続方策の優先順

| 場所 | 優先順 |
|---|---|
| 明示候補付き`start`の根 | 渡した候補を登録。自動提案器でさらに増やすことはしない。手動追加は可能 |
| 自動生成の根・木の判断点 | 呼び出せる`propose_action`があれば逐次提案。なければ出力形`actions`、次に簡易形`actions` |
| 木の外のロールアウト | `rollout_action`、出力形`actions`から一様選択、簡易形`actions`から一様選択、最後に`propose_action(...,index=0,...)` |

同じモデルに`actions`と`propose_action`を両方書くと、木での候補生成とロールアウトで使うものが違い得ます。提案器だけのモデルは、ロールアウト時のindex0でも合法な行動が得られるようにするか、明示的な継続方策を持たせます。

### A.5 公開操作の全体像

| 操作 | 作用 |
|---|---|
| `Planner(model, options={})` | モデルを参照して構築。オプションをコピー |
| `start(root,horizon)` | 新しい根。候補の自動生成は探索時 |
| `start(root,horizon,candidates)` | 新しい根へ候補を明示的にコピー登録 |
| `add_root_action(action)` | 統計を保って追加、IDを返す |
| `search(time_us)` | 指定したRNG型で相対時間探索 |
| `search(limits)` | 指定したRNG型で複数予算を指定 |
| `best_action()` | 推薦行動だけをコピーして返す |
| `result(true)` / `result()` | 候補統計も含めて返す |
| `result(false)` | 候補統計の配列を作らず返す |
| `advance(id,next,horizon)` | 実際の次状態へ移動。条件が合えば部分木再利用 |
| `clear()` | 根と統計を破棄し、主要な領域の容量を保持 |
| `mc_choose<Rng=MonteCarloNoise>(candidates,evaluate,time_us,use_crn=false,seed=1)` | 一回の評価関数による簡易比較。RNG型は省略可 |

`mc_choose`の候補はランダムアクセス可能なコンテナを想定し、呼び出し中だけ参照します。選んだ値だけを最後にコピーします。ヘルパーで実際に比較する候補数は131072以下にしてください。より大きい集合は、探索器の保存上限や逐次提案を使って設計します。

### A.6 数値・寿命・更新に関する契約

`State`、`Info`、`Action`はコピー・代入可能にします。モデルと借用データは、それを参照する探索器より長く生存させます。状態の浅いコピーが同じ可変配列を共有していると、ある試行が別の試行や根を壊す可能性があります。共有してよいものは固定データとし、変化する状態は試行ごとに独立して更新できるようにします。

探索・追加探索中は、採点、遷移、推定分布、継続方策を固定します。変更したら`start`です。過去の結果を読むための診断カウンタを増やすことは可能ですが、それによって問題の意味を変えないようにします。

添字、保存数、試行数、手数は`int`の範囲で扱います。得点、累計、分散計算は有限の`double`に収め、得点の二乗と試行数の積もオーバーフローさせないことが条件です。`NaN`や無限大をペナルティとして返しません。通常のAHC入力を想定し、極大入力への専用拡張はありません。

契約違反は必要箇所で`assert`を使います。ライブラリ独自の例外送出はありませんが、標準コンテナのメモリ確保失敗などまで起こらないという意味ではありません。本体のホットパスに`long double`や`__int128`は使っていません。

### A.7 Modelの各メソッドは、いつ、どの順で呼ばれるか

完全観測の基本形は例11の末尾、部分観測の具体例は例25で追いました。ここでは木探索・逐次提案・再開も含めて整理します。表のInfoは、完全観測なら同じ試行用State、部分観測ならその試行用Infoを指します。どの形式でもModel本体は同じ一個を参照します。

#### 一試行の基本順序

| 段階 | 呼び出しと順序 |
|---|---|
| 探索へ入る | 予算を確認し、必要なら根の`terminal`。残り0手や根が終端なら新しい試行を作らない |
| 根の一手を選ぶ | 保存済み候補を使うか、必要に応じて`actions`または`propose_action`。両方を必ず呼ぶわけではない |
| 試行を作る | 完全観測：根Stateをコピー。部分観測：`sample_root`でStateを作り、根Infoをコピー |
| 一手を実行 | 選択済み行動で`step`。部分観測はStateとInfoを同時に渡す |
| 完了判定 | `terminal` → 手数上限 → 浅い打ち切りの順。最初に成立した条件で完了 |
| 採点して完了 | 終端・上限なら`finish_value`、浅い打ち切りなら`leaf_value`。得点を統計へ反映し、試行用状態を破棄 |
| まだ続く場合 | 木を辿れるなら`tree_key`等で次の判断点を探す。木で選べる行動、またはrolloutの方策で次の一手を選び、`step`へ戻る |

この表はメソッドをすべて毎回呼ぶという意味ではありません。省略した`terminal`はfalse、`finish_value`は0として扱い、浅い打ち切りがなければ`leaf_value`は呼びません。根がすでに終端の場合は、`sample_root`・`step`・採点を呼ばずに停止します。

#### 木の判断点とrolloutの分岐

| 場面 | Modelの呼び出し |
|---|---|
| その判断点で初めて一括生成の候補を用意して行動選択するとき | `actions`を呼び、候補を保存。以後、その保存候補を使うため同じ判断点で毎回呼ばない |
| 逐次提案を使う判断点 | 候補を追加参加させる必要があり、提案も保存枠も残るときに`propose_action`。候補選択のたびに必ず呼ぶわけではない |
| 明示候補を渡した根 | 根の`actions`・保存用`propose_action`は呼ばない。根より先にはこの制限を適用しない |
| 木の行動を`step`で進め、まだ完了しない | 更新後のInfoで`tree_key`を呼ぶ。省略時はInfo自体をキーにする |
| 対応する子が既存 | 次の一手はその判断点の候補と統計で選ぶ。必要なら候補生成・提案を呼ぶ |
| 対応する子を初めて作った試行 | 子は保存するが、その試行の残りはrollout。この時点で子の`actions`を保存用に呼ぶわけではない |
| 木を使わない、保存上限などで木を続けられない、またはrolloutへ移った後 | `rollout_action`があれば使う。なければ出力形`actions`、簡易形`actions`から一様選択。それもなければ`propose_action(...,index=0,...)` |

rolloutで使う`actions`は、次の一手を選ぶたびに呼びます。保存木の候補生成に使う場合との違いです。また、一度rolloutへ移った試行は、その残りで保存木へ戻りません。`tree_key`はrolloutの全手で呼ばれるものではありません。

木には候補・統計・キーが残りますが、次の試行用Stateを保存ノードから取り出して続ける仕組みではありません。各新規試行は根のコピーまたは`sample_root`から始めます。`tree_key`を省略した場合はInfo自体、完全観測ならState自体がキーの値としてコピー保存されることがあります。これは試行中に更新する作業用オブジェクトとは別の、照合用データです。キーや統計は`start`・`clear`で破棄され、完全観測の`advance`で再利用できた場合だけ対応する部分が残ります。

`propose_action`の提案番号は保存する判断点ごとに進みます。rolloutの代用として呼ぶ場合だけ毎回index=0で、その結果を保存木の候補へ追加するわけではありません。乱数を受け取る各関数では、context付きの形式があればそれを優先します。具体的な引数形式は付録A.3・A.4のとおりです。

#### 探索の前後・中断時に呼ばれるもの

| 利用者の操作 | Modelのメソッドと状態の扱い |
|---|---|
| 探索器の構築 | Modelを参照する。Modelのシミュレーション用メソッドは呼ばない |
| `start` | 根をコピーし統計をリセット。Modelのメソッドは呼ばない。Model自体も初期化しない |
| `add_root_action` | 渡された候補を登録。Modelのメソッドは呼ばない |
| `search` | 上の流れで探索。予算0など、処理前に止まればModelを呼ばない場合もある |
| 未完了試行がある状態で再び`search` | 根の終端を確認する場合はあるが、同じ試行のState・Info・選択済み行動から続ける。根候補の再選択や`sample_root`のやり直しはしない |
| `result`・`best_action` | 根があり残り手数が正なら根の`terminal`。それ以外のModelメソッドは呼ばず、試行・乱数消費を進めない |
| 完全観測の`advance` | 部分木再利用の条件が合えば、渡された次Stateで`tree_key`を使って照合。実行済みの次状態は利用者が渡し、ライブラリは`step`で実行し直さない |
| 部分観測の`advance` | 渡された次Infoへリセット。`sample_root`はここでは呼ばず、次に試行を始めるときに呼ぶ |
| `clear`・探索器の破棄 | 保持する根・未完了試行・統計を破棄。Modelのリセット用メソッドはなく、Model本体も変更しない |

時間や遷移数での停止は、シミュレーションの完了と区別します。途中だからといって`leaf_value`や`finish_value`で仮の一試行を作りません。`sample_root`で準備した直後、最初の`step`より前に時間切れとなる場合も、準備した状態と根の行動を保持します。

`terminal`は状態を進めず、与えられた情報だけから判定する関数にします。`result()`の問い合わせや`search`の分割のしかたでも呼び出し回数が変わるため、呼ばれた回数に応じて状態・推定・乱数を変更すると、結果取得や中断再開の意味が崩れます。

`mc_choose`は一回の評価関数を内部の一手問題へ接続します。利用者が書いた評価関数が、その一手の採点として呼ばれます。評価関数内の独自ループをライブラリが複数の`step`として扱うことはありません。例23の5日間や出来事キーを理解するときも、この境界が重要です。

## 付録B. 実装アルゴリズムの概要と流れ

ここまでは使い方を中心に説明しました。この付録は、対象ヘッダv07の実装に沿って、なぜ各APIがそのように振る舞うかを説明します。数式は実装の判断を読み解くためのもので、統計的な最良候補の確定保証を与えるものではありません。

### B.1 全体像

| 順序 | 処理 | 対応する実装 |
|---:|---|---|
| 1 | CRNの有無を選び、今回の時間・完了数・遷移数の予算を確認 | `search`、`search_impl` |
| 2 | 根の候補を必要に応じて生成し、一候補を選ぶ | `prepare`、`admit`、`select` |
| 3 | 試行seedと、試行用状態を作る | `begin_trial` |
| 4 | 選択済みの行動を一手実行 | `transition`、モデルの`step` |
| 5 | 終端・手数上限・浅い打ち切りを判定 | `terminal`、`finish`、`leaf` |
| 6 | 未終了なら、保存木の子を探すかロールアウトへ進む | `successor`、`rollout` |
| 7 | 完了した得点を、通った判断点の統計へ反映 | `backup` |
| 8 | 予算が残れば次の試行。停止後は根の平均値で推薦 | `search`、`recommend` |

`tree=false`でも、根の行動について試行数と平均値を保存します。違いは、根より先の判断点まで木に保存するかどうかです。`mc_choose`も、内部で一手問題へ変換して同じ探索器を使っています。

### B.2 全列挙と逐次提案を、比較への「参加」と分ける

`actions`による一括生成では、保存上限内で候補を登録します。しかし、登録した全候補をすぐに一回ずつ試すわけではありません。各ノードは、登録した候補と、そのうち比較へ参加した候補の数を別に持ちます。

通常の参加上限は次の形です。

$$K_ {\mathrm{target}}=\max(1,\lfloor w\sqrt{N+1}\rfloor)$$

ここで$w$は`widening`、$N$はその判断点に反映済みの完了数、$K_ {\mathrm{target}}$は参加枠です。床関数$\lfloor x\rfloor$は、小数部分を切り捨てた整数を表します。実装は正の値から`int`へ変換します。

枠があり未参加候補があれば、一回の選択で最大一候補を参加させ、その候補を試します。逐次提案器がある場合は必要なときに一つ提案します。`widening=0`だけはこの式を使わず、候補が残る間は毎回一候補を参加させます。

この分離により、大量候補を最初に均等評価して予算を使い切ることを避けつつ、有望な候補へ追加試行を回せます。逆に、候補順が悪いと良い候補が参加する前に時間切れになります。一括登録が重い場合は、参加制御だけでは登録コストを減らせないため、逐次提案を使います。

### B.3 参加済み候補への評価配分

通常は、参加済み候補$a$に対して次の値を比較します。

$$U(a)=\overline{Q}_ {a}+c\cdot s\sqrt{\frac{\log(N+1)}{n_ {a}}}$$

| 記号 | 意味 |
|---|---|
| $U(a)$ | 次の試行でどれを調べるか決める指標 |
| $\overline{Q}_ {a}$ | 候補$a$の完了試行の平均得点 |
| $n_ {a}$ | 候補$a$の完了試行数 |
| $N$ | その判断点での完了試行数 |
| $c$ | `exploration` |
| $s$ | 得点尺度。明示した`value_scale`か、自動推定値 |
| $\log$ | 自然対数 |

前半は観測された平均の良さ、後半はまだ試行数が少ない候補を調べるための上乗せです。試行数が増えるほど、後半は小さくなります。未評価候補にはこの割り算を行わず、優先して評価します。

`value_scale=0`なら、その判断点で得られたリターン全体の標本標準偏差の2倍を$s$として使います。候補間の平均差も混ざる尺度であり、各候補の誤差だけを厳密に見積もる値ではありません。データが少ない、またはすべて同じ値で尺度が0の場合は、参加済み候補のうち試行数が少ないものを選びます。

尺度の推定に専用の追加評価は行いません。既存の試行から得た値を使い、得点を範囲内へ切り詰めることもしません。各枝に$1/\sqrt{n_ {a}}$を保存し、次の選択のたびに同じ平方根を再計算するのを避けています。

この式はUCB型の配分ですが、標準的なUCBの理論条件・定数・報酬範囲をそのまま採用した実装ではありません。ノイズ分布や、木の成長による評価の変化を含め、最良候補の統計的確定を保証するものではありません。

### B.4 最後の推薦は、探索中の上乗せを使わない

停止時の推薦では、完了試行のある根候補について、次の順に比較します。

1. 平均得点が高い。
2. 平均が等しければ、試行数が多い。
3. それも等しければ、先に登録した。

探索用の上乗せを、そのまま最終得点へ足して推薦するわけではありません。比較対象は参加済み候補ですが、`result().candidates`には未参加を含む登録済み全候補の統計を返します。

完了した根候補が一つもなければ、登録済みのフォールバックを返します。通常は最初に登録した候補で、`provisional=true`です。候補そのものがないなら、推薦はありません。

### B.5 木の成長と、状態キーの比較

木は状態の判断点を`Node`、行動を`Edge`として持ちます。確率的な行動では、同じ行動から複数の結果状態が生じるため、一つの枝が複数の子を持てます。

遷移後のキーを、その行動の保存済みの子へ順に照合します。同じキーがあれば、その判断点へ進みます。見つからずノード枠があれば、新しい子を一つ保存し、**今回の試行の残りはロールアウト**へ進みます。このため、一試行で新たに保存する判断点は最大一つです。新しい判断点の候補を詳しく調べるのは、再訪したときです。

キー照合は、保存された子に対する線形探索です。巨大なハッシュ表、全履歴を横断する状態統合、連続状態の近傍検索は持ちません。ノードには全状態のコピーではなく、統計や行動情報を保存し、別配列にキーを保存します。

子のIDは親より大きくなるよう作られます。根以外のノードiのキーは、キー配列のi-1へ置き、独立したキー添字をノードごとに重複保持しない実装です。

### B.6 ロールアウトと報酬の集計

木の外では、優先順に従って継続行動を選び、状態を進めます。保存木の中で通った判断点については、ノード・行動・一手報酬の経路だけを保存します。

ロールアウトの後半の報酬は、すべてを配列へ保存せず、割引済みの和として逐次集計します。後半で$d$手進めた時点の累計を$T$、次の報酬を$r$とすると、次の形です。

$$T_ {\mathrm{new}}=T+\gamma^d r$$

終了時・打ち切り時の価値$v$も、後半の手数に合わせて足し、保存した木の経路を逆順にたどります。各一手での更新は次の形です。

$$v_ {\mathrm{parent}}=r+\gamma v_ {\mathrm{child}}$$

ここで$r$はその枝の一手報酬、$v_ {\mathrm{child}}$は次の状態から先の価値、$v_ {\mathrm{parent}}$はその一手を含めた価値です。そのため、木の各判断点には、その場所から先の得点が蓄積されます。根より前の過去の報酬を二重に持ち込みません。

終了判定の順序は、終端、手数上限、浅い打ち切りです。浅い打ち切りなら`leaf_value`、それ以外の完了なら`finish_value`を一度だけ使います。

部分観測の採点形式の選択は`if constexpr`と`requires`でコンパイル時に行います。実行時に形式一覧を走査する処理や、追加のState・Infoコピーはありません。現在の試行が保持する両者をconst参照で渡します。採点の引数が増えても、試行数・配分・乱数消費を追加する処理はありません。

### B.7 平均と分散を一件ずつ更新する

全試行の得点配列は保持せず、件数$n$、平均$m$、平均との差の二乗和$M_ {2}$を保持します。新しい得点$x$に対して、次の順で更新します。

$$n'=n+1,\quad d=x-m,\quad m'=m+\frac{d}{n'},\quad M_ {2}'=M_ {2}+d(x-m')$$

$n'$、$m'$、$M_ {2}'$は更新後の値、$d$は更新前平均との差です。完了数が2以上なら、標本分散は$M_ {2}/(n-1)$、標本標準偏差はその平方根です。丸めのための微小な負値が尺度の平方根へ入らないよう、その箇所で0以上にします。

同じ小さな統計型をノードと枝で共用します。枝の平均と件数は選択・推薦に使い、自動尺度はノードの分散から計算します。枝ごとの分散に基づいて別の配分法を切り替える機能はありません。

### B.8 中断中の試行を保持する

`Pending`には、一試行の状態、部分観測なら情報、試行seed、深さ、選択済みの行動、後半の報酬累計などを保存します。予算が尽きても未完了なら、そのまま次の`search`を待ちます。

平均・件数への反映は完了時だけです。途中で得点が良さそうだから仮に一試行として数えることはありません。これにより、長い試行が途中で切れたときに、短い試行と未完了の低い部分和をそのまま比較する問題を避けます。

次の`search`では保留中の行動・試行seed・手数から再開します。`start`、`clear`、`advance`で根を変えるときには、古い未完了試行を破棄します。

### B.9 CRNの実装範囲

現在の根の世代と`Options::seed`から根の基準seedを一度作ります。`start`、`clear`、`advance`では世代を更新します。ONは根候補ごとの完了数、OFFは根全体の完了数を試行番号に使います。基準seedと試行番号から試行seedを作り、ONだけ候補間で同じ第k試行を対応させます。

| ONの呼び出し | 系列を区別する情報 | ONで共有する相手 |
|---|---|---|
| `sample_root` | 試行seed、初期の残り手数、初期状態の用途番号 | 根候補の同じ第k試行 |
| `step` | 試行seed、現在の残り手数、遷移の用途番号 | 同じ第k試行・同じ残り手数 |
| `rollout_action`と既定rollout | 試行seed、現在の残り手数、方策の用途番号 | 同じ第k試行・同じ残り手数 |
| 保存候補の`propose_action` | 根の基準seed、残り手数、提案番号、提案の用途番号 | 同じ残り手数・同じ提案番号 |

初期状態・遷移・方策・提案の用途番号はそれぞれ0・1・2・3です。内部の区別用で、利用者が通常これを意識する必要はありません。追加contextから作る出来事キー・チャネルは別の窓口です。試行中のcontextは試行seed、保存候補の提案contextはその提案専用seedを保持します。OFFでも追加contextの生成規則は同じで、保存候補の提案contextにだけノード番号も含めます。OFFの通常RNGはこの表の系列を作らず、根の基準seedで初期化した一つの系列を使い続けます。

ONの場合だけ根候補ごとのCRNカウンタを持ち、試行完了時に増やします。途中再開では同じ試行seedと手数を使います。途中追加した根候補は第0試行から始まり、他候補の第0試行と同じ系列になります。候補間の待ち合わせはありません。

保存候補の提案に試行番号を使わないのは、同じ条件の子が異なる試行で初めて展開されても提案乱数を揃えるためです。状態の対応表、乱数列の保存、子ごとの追加評価カウンタは持ちません。使い捨てのrolloutに提案器を流用する場合は、試行・手数のrollout系列を渡します。

`advance`で引き継いだ統計の件数と、新しい根でのCRNカウンタは別です。世代を変え、共有番号を0から始めます。古い観測をペアに組み直す処理はしません。OFFで引継ぎ件数から番号が始まっても、世代が変わるので古い根の系列を再使用しません。

候補参加、UCB型の配分、推薦、停止条件はON/OFFで共通です。乱数共有によって観測が変わり、その後の選択が変わることはあります。回数の均等化、追い付き評価、共有のための追加rolloutは行いません。

### B.10 乱数の実装

既定の`MonteCarloNoise`は、64bitの状態に一定値を足し、xor・シフト・乗算で混合するSplitMix64型です。`uniform01`は上位53bitを0以上1未満の`double`にします。`uniform_int(bound)`は一部を引き直す棄却法で剰余の偏りを除きます。範囲が1値だけなら、乱数を一つ消費して0を返します。それ以外では先に一つ引き、その値が`bound`未満の場合だけ、棄却するかを決める閾値を計算します。閾値は剰余なので必ず`bound`より小さく、引いた値が`bound`以上なら棄却されないためです。必要な場合には従来どおり引き直すため、返す値と乱数消費数は変わりません。

キー付き系列は元のseedとキー・チャネルを混合して作り、通常系列の現在位置は変更しません。`stream<Rng>`は混合したseedで指定型を構築します。CRN ONの各コールバックでは、残り手数の4倍に用途番号を足した位置番号と試行seedを64bit演算で混合し、そのseedでRNGを局所変数として構築します。用途を4種類に限定した簡単な計算であり、他のコールバックが何個引いたかを引き継ぎません。公開される`context.stream`と内部の位置番号による生成は別の系列です。追加contextを要求しない普通のモデルにも同じ型が直接渡ります。

OFFでは探索器内の`std::optional<Rng>`に一つだけ保持します。根の世代を更新するときに構築し、初期状態・候補提案・rollout・遷移に同じ参照を渡します。中断時に特別な保存・復元処理はせず、このオブジェクトの現在位置をそのまま残します。RNGをコピー・ムーブできるという追加の要件はありません。ONでは従来どおり局所変数を構築して使い、OFF用オブジェクトは構築しません。`search`の入口でON/OFFを一度選び、内部のテンプレート引数として伝えることで、手ごとのRNG選択から実行時の分岐を除いています。公開APIにテンプレート引数を追加する必要はありません。

大きな状態を初期化する`std::mt19937`系も、OFFでは毎手の初期化費用を避けられます。ONでは軽いRNGが適しています。RNGを毎手保存する管理機構や、新しいユーザーパラメータは追加していません。外部RNGの参照を探索全体へ渡す旧APIもなくし、RNG型・seedの指定箇所を一つにしています。

### B.11 部分木の再利用と詰め直し

`advance`は、指定された根行動の子から、次状態のキーに一致する保存ノードを探します。条件が合えば、その子から到達できるノードと枝だけを残します。

親より子のIDが大きいことを使い、到達可能なものを前から走査して印を付けます。新しい連番との対応を作り、ノード・枝・キーを前へ詰め、参照IDを書き換えます。独立した探索キューを持たない形です。

保持できるのは、次の問題でも意味が変わらない部分の統計です。残り手数を一定に保つローリングホライズン、モデル更新後、部分観測の推定更新後などは、同じ部分問題ではないためリセットします。

候補生成が上限で途中になったノードも、そのまま引き継がず再生成します。古い上限による候補欠落を、新しい根へ固定してしまわないためです。

### B.12 時間確認と計算量の見方

`search`は、完了試行数、遷移数、時刻の順に予算を確認します。未完了試行がなければ試行を準備し、その準備後にも時間を確認します。根の終端判定は、一回の`search`中で必要になった最初の一度だけです。

時計は通常`clock_interval`遷移ごとに確認します。直近の確認間隔に比べ締切が近い場合は一遷移ごとへ切り替えます。これは簡易な追従であり、一回のコールバックがどれだけ掛かるかを完全に予測するものではありません。

| 処理 | 主に増加する要因 |
|---|---|
| 一試行の開始 | `State`のコピー、部分観測では`sample_root`と`Info`のコピー |
| 行動の選択 | その判断点の参加候補数。参加候補を線形走査 |
| 木の子の探索 | その行動から保存済みの結果状態数×キー比較の費用 |
| 一括候補生成 | 列挙した候補数と行動コピー。参加数が少なくても登録費用は掛かる |
| 一試行の遷移 | 実際に進めた手数と、各`step`・継続方策の費用 |
| 結果取得 | 推薦は参加候補の走査。全候補統計を返すなら登録候補数にも比例 |
| 部分木再利用 | 現在保存している木・枝の数に比例する走査と詰め直し |

軽量な`State`、安い合法手生成、安い継続方策が性能を左右します。一方、重い本来のシミュレーションが支配的なら、探索器の数命令を削るより、浅い打ち切りやモデル側の前計算を検討する方が効果的な場合があります。その場合も同じ時間での解品質を確認します。

## 付録C. 検証と参照資料

### C.1 このガイドの検証範囲

ガイド内の31本のC++コードを、そのまま個別のソースファイルに対応させて検証しています。文章と別の短縮版コードだけをテストする形にはしていません。

- 検証コンパイラ：GCC 13.3.0、Linux。AtCoderのGCC 15.2.0実機での検証ではありません。
- C++20：31例すべてコンパイル・実行成功。
- C++23：31例すべてコンパイル・実行成功。
- 両規格で`-O2 -Wall -Wextra -Wpedantic -D_GLIBCXX_ASSERTIONS`を使用し、警告なし。
- ASan/UBSan：途中再開、状態キー、部分木再利用、モデル更新、粒子による再計画、指定RNG、CRN、State＋Infoの採点の10例を検証し、成功。環境上の制約によりリーク検査は無効化。
- Markdown内のC++コード31本と、実際にコンパイルしたファイルが一致することを確認。
- ガイド第6版では例23の配置を変更して再コンパイル・実行。変更のない例は、本体・RNG・ソースのハッシュが一致する検証記録を継承。
- 数式は外部Markdown向けの`$...$`・`$$...$$`で記述。表示数式を一行にし、Markdown前処理で強調やHTMLへ変形しないことを確認。

部分観測の採点形式を追加したv07を使用しています。CRN ON/OFFの乱数生成と評価配分はv06から変更していません。OFFでは引き続き一つのRNGを連続使用します。例24・28用の`fast_rng.hpp`は添付版を変更せず同梱しています。対象ヘッダのSHA-256は次の値です。

`97e23a68db447244dedff6687ede02f3ab3faa4fd941814af7bebf53ab011a9b`

固定予算による動作確認は、APIの意味や例の整合性を確認するためのものです。未知のAHC問題で、この例のパラメータが最善であることを検証したものではありません。時間制限を使う例の実行回数や実行時間も、環境によって変わります。

### C.2 参照資料

本ガイドのAPI・既定値・優先順位・内部処理は、配布対象の`monte_carlo_planner_v07.hpp`と、そのREADMEを確認して記述しました。一般的な手法の背景を読み進める場合は、次の資料が対応します。

- Mykel J. Kochenderfer, Tim A. Wheeler, Kyle H. Wray, [Algorithms for Decision Making](https://algorithmsbook.com/decisionmaking/), MIT Press, 2022。厳密計画、近似価値関数、オンライン計画、部分観測の体系的な背景。
- David Silver, Joel Veness, [Monte-Carlo Planning in Large POMDPs](https://papers.nips.cc/paper_files/paper/2010/hash/edfbe1afcf9246bb0d40eb4d8027d90f-Abstract.html), 2010。部分観測の信念更新と木探索を組み合わせるPOMCP。本ライブラリと同一の実装仕様ではない。

一般の論文や教科書にある保証・設定を、そのままこの軽量実装の保証と解釈しないでください。実際に利用するときの契約は、本ガイドと対象ヘッダのAPI仕様です。
