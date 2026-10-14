# AHC向け solverカタログ

対象：`solvers_20261005(1).zip` に含まれる23本のC++ヘッダ。以下は、この添付ソースの実装範囲に基づく説明です。

このカタログは、**「何を決めたいか」「どのような条件があるか」から、使えるsolverを探す**ためのものです。まず分類一覧を示し、その後で各ライブラリを具体例とともに説明します。番号は一覧と詳細説明で共通です。

## 1. 分類一覧

英語名は、対応するソースファイル名からバージョン番号・拡張子・`solver` を除いた表記です。ファイルと照合しやすいよう、アンダースコアをそのまま残しています。

### A. 候補の中から選ぶ

| 番号 | 日本語名 | 英語名（ソース対応） | 解く問題を一言で |
|---|---|---|---|
| 01 | 最大被覆・最小費用集合被覆 | `maximum_coverage` | センサーや施設などを選び、限られた個数・予算で多くの対象をカバーする。全対象を安くカバーする使い方もできる |
| 02 | 重み付き集合パッキング・独立集合 | `set_packing` | 同時に採用できない候補や、同じ資源を使う候補の中から、利益の大きい組合せを選ぶ |
| 03 | 疎な0–1最適化 | `pbo` | 採用・不採用を決める際に、複数の予算、個数、依存関係、違反時の減点をまとめて扱う |

### B. 仕事・要素を割り当てる、グループに分ける

| 番号 | 日本語名 | 英語名（ソース対応） | 解く問題を一言で |
|---|---|---|---|
| 04 | 一般化割当 | `generalized_assignment` | 各仕事の担当先と実行方法を決め、担当先の容量を守りながら総費用を減らす |
| 05 | 施設配置 | `facility_location` | 候補地から指定数の拠点を選び、各利用者を最も安く対応できる拠点へ結び付ける |
| 06 | 容量制約付きグラフラベリング | `capacitated_graph_labeling` | 各要素の所属を決め、グループの容量と、要素同士の相性を同時に考慮する |
| 07 | 連結性付き領域分割 | `connected_partition` | 地図やグラフを、飛び地のない領域へ分け、境界・負荷・領域間の接触条件を調整する |
| 08 | 容量制約付きハイパーグラフ分割 | `hypergraph_partitioner` | 共通の工具・データなどを使う要素をまとめ、共有物の準備費用や容量消費を減らす |

### C. 場所へ配置する、平面に詰める

| 番号 | 日本語名 | 英語名（ソース対応） | 解く問題を一言で |
|---|---|---|---|
| 09 | 二次割当 | `qap` | 物体を用意された位置へ一対一に配置し、物体間の関係と位置間の距離から決まる費用を減らす |
| 10 | 矩形パッキング | `rectangle_packing` | 長方形を重ならないように配置し、収容利益・使用高さ・箱数・外接枠の大きさを改善する |

### D. 順序・開始時刻を決める

| 番号 | 日本語名 | 英語名（ソース対応） | 解く問題を一言で |
|---|---|---|---|
| 11 | 重み付き順序最適化 | `linear_ordering` | 前後関係の希望、配置する順位、隣り合う要素の費用を考え、全要素を並べる |
| 12 | 汎用資源スケジューリング | `generic_resource_scheduler` | 先行作業と機械・人員の取り合いを考え、各作業の実行方法と開始時刻を決める |

### E. 訪問する、経路を通す、ネットワークをつなぐ

| 番号 | 日本語名 | 英語名（ソース対応） | 解く問題を一言で |
|---|---|---|---|
| 13 | 巡回セールスマン問題・ハミルトン路 | `tsp` | 指定された地点をすべて1回ずつ訪れる順序を決め、移動費用を減らす |
| 14 | 複数経路・配送計画 | `routing` | 複数車両について、訪問先・担当・順序を決める。任意訪問や集荷と配送の組も扱う |
| 15 | 容量制約付き複数経路 | `capacitated_router` | 複数の始点・終点の組を結び、辺・頂点・共有資源の容量を守るように経路を引く |
| 16 | シュタイナー木 | `heuristic_steiner_tree` | 必要な地点を、途中の分岐点や共有区間も使って安く一つにつなぐ |
| 17 | 排他制約付きシュタイナー木 | `constrained_steiner_tree` | 同時使用できない中継点や使用禁止箇所を避けながら、必要な地点を一つにつなぐ |
| 18 | 複数エージェント経路計画 | `multi_agent_path_finding` | 複数ロボットの各時刻の位置を決め、衝突を避けて移動・搬送する |

### F. 数値を調整する、未知の値を推定する

| 番号 | 日本語名 | 英語名（ソース対応） | 解く問題を一言で |
|---|---|---|---|
| 19 | 箱制約付き非線形最適化 | `box_optimizer` | 各変数の上下限を守りながら、座標・出力・配分量などの実数値を調整する |
| 20 | 線形式の推定 | `linear_estimator` | 合計値や差などの観測から、未知の実数・整数・有限候補の値を推定する |
| 21 | 汎用推定 | `mcmc_estimator` | 利用者が定義する観測モデルから、もっともらしい未知状態や、その不確かさを調べる |
| 22 | 逐次推定 | `particle_filter` | 観測が届くたびに、あり得る状態の候補と重みを更新する |

### G. 不確実な将来を考えて行動を選ぶ

| 番号 | 日本語名 | 英語名（ソース対応） | 解く問題を一言で |
|---|---|---|---|
| 23 | 有限手数のモンテカルロ計画 | `monte_carlo_planner` | 確率的な結果や将来の観測を試し、残り手数で得られる期待得点が大きい行動を選ぶ |

## 2. 各ライブラリで表現できる問題

### サンプルコードの実行方法

各詳細パートには、そのsolverの特徴を試せる独立したC++20プログラムを1本ずつ載せています。**各例は別々に保存・コンパイルして実行します。** `main`、標準入力からの読込み、結果の標準出力まで含みます。

1. 例01ならコードを `example01.cpp`、入力例を `example01.in` に保存します。
2. その例の `#include` に書かれたヘッダを、同じディレクトリへ置きます。ヘッダは冒頭に記載した対象zipのものを使います。
3. 次のようにコンパイルし、入力例を与えます。他の例では番号を読み替えてください。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined example01.cpp -o example01
./example01 < example01.in
```

例18のMAPFだけは、追加でAtCoder Libraryが必要です。`ac-library/atcoder/mincostflow` が存在するように展開し、次のように `-I` を付けます。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined -I ./ac-library example18.cpp -o example18
./example18 < example18.in
```

- IDは、特記がなければ0から始まります。入力例の数値だけを入力ファイルへコピーしてください。
- コードは、説明した形式・前提を満たす入力を使う想定です。入力形式そのものの検査は省いています。
- 出力例は、このカタログのコードと入力を実行して得た結果です。同点の別解、時間制限、乱数処理の環境差により、順序や小数値が変わる場合があります。後の説明で、結果の意味を確認してください。
- `NO_FEASIBLE_SOLUTION` は、その実行で条件を満たす解を取得できなかった意味です。解が存在しない証明とは限りません。
- 探索予算は教育用の小さな入力に合わせています。本番で使う場合は、各例の `options` や `limits` などの予算設定を調整します。

**検証環境：** GCC 13.3.0、`-std=c++20`。全23例を上記の警告オプション付きでコンパイル・実行し、出力の目的値・条件またはモデルとの整合性を確認しました。例23では、対象ヘッダ内の既存の整数から実数への変換について警告が出ます。掲載したサンプル本体には警告はありません。

### 01. 最大被覆・最小費用集合被覆（`maximum_coverage`）

ファイル：`maximum_coverage_solver_v08.hpp`

**一つの候補で複数の対象をカバーできるとき、どの候補を選ぶかを決めます。** 候補の価値は単独の点数ではなく、選んだ候補を合わせて何をカバーできるかで決まります。

例えば、監視カメラの設置候補が10か所あり、設置位置ごとに見えるマスが違うとします。3台だけ設置するなら、同じ場所を重ねて見る3台より、重要な場所を広く見渡せる3台を選びたい、という問題です。同じ対象を何回カバーしても、その対象の得点は1回分です。

主に次の3通りに使えます。

- **個数を制限する：** 最大3個などの条件で、カバーした対象の重要度合計を大きくする。選択数の下限・上限を使い、ちょうど3個とも指定できる。
- **予算を制限する：** 候補ごとに異なる費用を設定し、予算内でカバーする重要度合計を大きくする。
- **全対象をカバーする：** カバー漏れをなくし、選んだ候補の費用合計を小さくする。

必ず選ぶ候補、選ばない候補、今回検討する候補集合も指定できます。既設センサーを固定して追加分だけを選ぶ、といった部分問題にも使えます。

**用意するものと結果：** 対象ごとの重要度、候補ごとの費用、各候補がカバーする対象一覧を渡すと、選んだ候補と被覆状況が得られます。距離や視界から「何がカバーされるか」を計算する部分は利用者側で用意します。

**使い分け：** 対象の重複を除いた広がりを評価したいときに向きます。候補同士の同時採用禁止や複数種類の予算を組み合わせたい場合は、02や03も検討します。個数制約版・予算制約版・全被覆版は別の入口であり、すべての条件を自由に重ねる汎用制約solverではありません。

#### サンプル：カメラ2台で、重要な場所を広く見守る

各カメラが見える場所は入力で与えます。重なる場所の重要度は1回だけ数えるため、単体の得点が高いカメラを並べるだけでは最善とは限りません。

**入力形式**

1行目は対象数 `N`、候補数 `M`、設置台数上限 `K`。2行目は対象ごとの重要度。続く `M` 行は、その候補が見える対象の個数と対象IDです。

**実行コード：`example01.cpp`**

```cpp
// 目的：台数の上限を守り、監視できる場所の重要度合計を最大にします。
// 同じ場所が複数のカメラに映っても、その重要度は1回だけ加算します。

#include "maximum_coverage_solver_v08.hpp"
using namespace std;

int main() {
    // 入力は、場所の数n、カメラ候補の数m、設置できる台数kです。
    // 場所IDは0〜n-1、カメラIDは0〜m-1で、入力に現れる順に対応します。
    int n, m, k;
    cin >> n >> m >> k;

    coverage_problem problem;
    // 各場所の重要度を登録します。重要度が高い場所ほど、カバーする価値があります。
    problem.target_weights.resize(n);
    for (auto& weight : problem.target_weights) {
        cin >> weight;
    }

    // この例では台数だけを制限し、カメラごとの費用は同じ1とします。
    problem.candidate_costs.assign(m, 1);
    // カメラ候補ごとに、見える場所のID一覧を登録します。
    // 見える場所を座標などから計算する処理は、この例では入力側で済ませています。
    problem.covered_targets.resize(m);
    for (auto& targets : problem.covered_targets) {
        int count;
        cin >> count;
        targets.resize(count);
        for (int& target : targets) {
            cin >> target;
        }
    }

    // 時間制限を無効にし、探索回数の上限を1000にします。
    // 乱数seedを固定すると、同じ入力で探索を再現しやすくなります。
    coverage_solver_options options;
    options.time_limit_ms = -1;
    options.iteration_limit = 1000;
    options.seed = 1;

    coverage_solver solver(problem);
    // 個数制約版の入口です。k台以下の候補を選び、重複を除いた被覆価値を高めます。
    auto result = solver.solve_maximum_coverage(k, options);
    // 結果を使う前に、台数などの条件を満たす解が得られたか確認します。
    if (!result.feasible) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 見やすいようにID順に表示します。選択集合の意味は変わりません。
    sort(result.selected_candidates.begin(), result.selected_candidates.end());
    // covered_weightはカバーできた場所の重要度合計、selectedは選んだカメラIDです。
    cout << "covered_weight " << result.covered_weight << '\n';
    cout << "selected";
    for (int camera : result.selected_candidates) {
        cout << ' ' << camera;
    }
    cout << '\n';
}
```

**入力例**

```text
6 4 2
5 5 4 4 3 3
3 0 1 2
3 0 1 3
3 2 4 5
2 3 4
```

**出力例**

```text
covered_weight 24
selected 1 2
```

**結果の読み方**

`selected` は設置するカメラの候補IDです。候補1と2を組み合わせると6対象をすべてカバーでき、重要度の合計は24です。

### 02. 重み付き集合パッキング・独立集合（`set_packing`）

ファイル：`set_packing_solver_v06.hpp`

**それぞれに利益がある候補から、互いに両立する組合せを選びます。** 最大化するのは、選んだ候補の利益合計です。

例えば、盤面上に置ける図形を「形と置き場所まで決まった候補」として多数列挙します。各候補には得点があり、同じマスを使う候補は同時に選べません。このsolverが決めるのは、列挙した候補のうちどれを採用するかです。

両立条件は、次の二つの形で表現できます。

- **競合ペア：** 候補AとBを同時に選べないと指定する。競合関係をグラフで与えると、最大重み独立集合になる。
- **資源の容量：** 各候補が使用する資源の一覧を与える。容量1なら重複使用禁止、容量3ならその資源を使う候補を3個まで選べる。

資源を使う候補は、その資源を1単位ずつ消費します。競合ペアと資源容量は併用でき、必須候補・禁止候補も指定できます。

図形の選択のほか、干渉しない改善操作の一括採用、事前に列挙した経路候補の選択、同じ部品を取り合う作業セットの選択にも使えます。

**用意するものと結果：** 候補の利益と競合・資源情報を渡すと、採用した候補の一覧が得られます。候補の位置や経路自体を新しく作る部分は利用者側です。

**使い分け：** 「この資源を使う候補は高々何個」という条件に向きます。「候補Aは電力2、Bは電力7を使う」のような候補別の消費量や、採用依存・最低個数まで扱うなら03が自然です。

#### サンプル：同じ部品を取り合う製品候補から選ぶ

候補ごとに利益と使用する部品を与えます。同じ部品を使う候補は容量の範囲でしか選べず、部品とは別に指定した競合ペアも同時採用できません。

**入力形式**

1行目は候補数 `N`、資源数 `R`、競合ペア数 `E`。2行目は各資源の容量。続く `N` 行は `利益 使用資源数 資源ID…`、最後の `E` 行は競合する候補IDの組です。

**実行コード：`example02.cpp`**

```cpp
// 目的：資源の容量と同時採用禁止の条件を守り、候補の利益合計を最大にします。
// 各候補は、必要な資源をまとめて使う仕事や予約に対応します。

#include "set_packing_solver_v06.hpp"
using namespace std;

int main() {
    // 候補数、資源の種類数、同時採用を禁止する候補ペアの数を読み込みます。
    // 候補IDと資源IDは、それぞれ0から始まります。
    int n, resource_count, conflict_count;
    cin >> n >> resource_count >> conflict_count;

    SetPackingProblem<long long> problem;
    // capacity[r]は、資源rを同時に使える候補の数です。
    // 例えば容量1の会議室を使う候補は、同時には1つしか選べません。
    problem.capacity.resize(resource_count);
    for (int& capacity : problem.capacity) {
        cin >> capacity;
    }

    // weight[i]は候補iを採用したときの利益です。
    // resources[i]には必要な資源IDを並べ、採用時に各資源を1単位ずつ使います。
    problem.weight.resize(n);
    problem.resources.resize(n);
    for (int i = 0; i < n; ++i) {
        int count;
        cin >> problem.weight[i] >> count;
        problem.resources[i].resize(count);
        for (int& resource : problem.resources[i]) {
            cin >> resource;
        }
    }

    // 資源の取り合い以外の理由で、両立しない候補も登録できます。
    for (int i = 0; i < conflict_count; ++i) {
        int a, b;
        cin >> a >> b;
        problem.conflicts.emplace_back(a, b);
    }

    // 第2引数の1は乱数seedです。利益を整数で扱うためlong longを使います。
    SetPackingSolver<long long> solver(problem, 1);
    SetPackingSolver<long long>::Options options;
    // 時計による打切りを無効にし、最大2000回の探索で候補の組合せを改善します。
    options.time_limit_us = -1;
    options.iteration_limit = 2000;
    auto result = solver.solve(options);
    // 利益が大きくても、容量や排他条件に違反した解は採用できません。
    if (!result.feasible) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 選んだ候補をID順に表示します。valueはそれらのweightを足した値です。
    sort(result.selected.begin(), result.selected.end());
    cout << "profit " << result.value << '\n';
    cout << "selected";
    for (int candidate : result.selected) {
        cout << ' ' << candidate;
    }
    cout << '\n';
}
```

**入力例**

```text
5 3 1
1 1 1
9 2 0 1
6 1 0
6 1 1
5 1 2
10 2 1 2
1 4
```

**出力例**

```text
profit 17
selected 1 2 3
```

**結果の読み方**

`selected` が採用候補です。候補1・2・3はそれぞれ異なる資源を使い、合計利益17を得ます。単体で利益9の候補0を選ぶより、候補1と2を組み合わせる方が得です。

### 03. 疎な0–1最適化（`pbo`）

ファイル：`pbo_solver_v08.hpp`

**多くの採用・不採用の判断に、複数の条件が同時にかかる問題を扱います。** 各判断を0か1で表し、採用による利益から、違反に対する減点を引いた値を大きくします。

例えば、実施したい作業が多数あり、「費用は100以内」「電力は50以内」「Aを実施するなら設備Bも設置」「この5作業から最低2個は実施」を同時に満たしたい場合です。一つの作業が、費用・電力・個数など複数の条件へ同時に影響して構いません。

表現できる主な条件は次のとおりです。

- 整数の消費量を足し合わせた予算の上限、必要量の下限、ちょうど一致する条件。
- 指定候補から高々1個、少なくとも2個、ちょうど3個といった個数条件。
- 「Aを選ぶならBも選ぶ」「AとBは同時に選ばない」といった、線形の条件へ書き換えられる論理関係。
- 必ず守る条件と、違反しても減点で済む条件の併用。
- 違反量に比例する減点、違反の有無だけで決まる一律減点。
- 指定した候補をすべて選んだときだけ発生する、追加の利益や減点。

固定した判断を残して一部だけを再検討する使い方もできます。

**用意するものと結果：** 各0–1変数の利益と、関係する変数・整数係数・上下限を列挙した条件を渡します。結果は各変数の0–1値、得点、必須条件の充足状況です。

**使い分け：** 条件を複数組み合わせる選択問題の受け皿です。ただし、任意の実数変数や任意の非線形関数をそのまま渡すものではありません。被覆だけ、競合だけという構造なら01・02の方が入力を簡単にできます。

#### サンプル：予算・電力・設備の依存関係を守って作業を選ぶ

作業の採否を0–1変数にします。費用と電力という二つの上限に加え、「作業Aを行うなら設備Bも採用する」を同時に表現します。設備には負の利益を付け、設置の負担を表しています。

**入力形式**

1行目は変数数 `N`、費用上限、電力上限、依存関係数 `D`。続く `N` 行は `利益 費用 電力`。最後の `D` 行の `A B` は、Aを採用するならBも採用する、という意味です。

**実行コード：`example03.cpp`**

```cpp
// 目的：各設備を採用するかどうかを決め、予算・電力・依存関係を守って利益を最大にします。
// 採用を1、不採用を0とする変数を使うため、複数の条件を一次式で表せます。

#include "pbo_solver_v08.hpp"
using namespace std;

int main() {
    // 設備数n、予算上限、電力上限、依存関係の数を読み込みます。
    // 設備IDは0〜n-1です。
    int n, dependency_count;
    long long budget, power_limit;
    cin >> n >> budget >> power_limit >> dependency_count;

    PboProblem problem;
    // profit[i]は設備iを採用したときの利益です。負なら維持費などの負担を表せます。
    problem.profit.resize(n);
    // PboTermの {i, 係数} は「係数 × x[i]」という項です。
    // 採用した設備だけが費用と電力を消費する式を組み立てます。
    vector<PboTerm> cost_terms, power_terms;
    for (int i = 0; i < n; ++i) {
        long long cost, power;
        cin >> problem.profit[i] >> cost >> power;
        cost_terms.push_back({i, cost});
        power_terms.push_back({i, power});
    }

    // add_le は「係数×採用値の合計が上限以下」という必須条件です。
    problem.add_le(cost_terms, budget);
    problem.add_le(power_terms, power_limit);
    for (int i = 0; i < dependency_count; ++i) {
        // 入力a bは「設備aを採用するなら、前提となる設備bも採用する」という意味です。
        int a, b;
        cin >> a >> b;
        // x[a] <= x[b] とすれば、Aだけを採用する組合せを禁止できます。
        problem.add_le({{a, 1}, {b, -1}}, 0);
    }

    // seedを1に固定し、時計による打切りなしで最大3000回探索します。
    PboSolver solver(problem, 1);
    PboParam options;
    options.time_limit_us = -1;
    options.max_iterations = 3000;
    const auto& result = solver.solve(options);
    // この例の制約は全て必須条件です。満たした結果だけを表示します。
    if (!result.evaluation.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // profitは採用設備の利益合計です。続くxのi番目が設備iの採用・不採用を表します。
    cout << "profit " << result.evaluation.objective << '\n';
    cout << "x";
    for (uint8_t value : result.x) {
        // uint8_tを文字ではなく、数値の0または1として表示します。
        cout << ' ' << static_cast<int>(value);
    }
    cout << '\n';
}
```

**入力例**

```text
4 8 6 1
-2 2 0
12 4 3
8 3 3
6 2 2
1 0
```

**出力例**

```text
profit 16
x 1 1 0 1
```

**結果の読み方**

`x` の各値が採用なら1、不採用なら0です。設備0と作業1・3を選ぶと、費用8・電力5で、利益は -2 + 12 + 6 = 16です。設備を採用せず作業1だけを選ぶことは許されません。

### 04. 一般化割当（`generalized_assignment`）

ファイル：`generalized_assignment_solver_v13.hpp`

**各仕事について担当先と実行方法を一つ選び、担当先ごとの容量を守りながら総費用を減らします。** 同じ仕事でも、担当先や実行方法によって費用と容量消費が変わります。

例えば、仕事Xを機械Aへ割り当てると容量を6使って費用3、機械Bへ割り当てると容量を2使って費用5とします。すべてを安い機械に任せると容量を超えるため、どの仕事を別の機械へ回すかを調整します。

表現できる主な内容は次のとおりです。

- 仕事ごとに異なる割当可能な担当先。
- 担当先ごとに異なる容量上限。
- 仕事と担当先の組合せごとに異なる容量消費・費用。
- 同じ仕事・同じ担当先に対する複数の実行方法。例えば「安いが容量を多く使う方法」と「高いが容量を節約する方法」。
- 一部の仕事の割当を固定した部分修復。

基本の目標は、すべての仕事を一つずつ割り当てることです。仕事を実施しない選択肢が必要なら、容量と費用を設定したダミーの担当先を設けるなど、入力側でその意味を表します。

**用意するものと結果：** 各担当先の容量と、仕事ごとの割当候補を渡すと、選ばれた候補・担当先・容量使用量が得られます。

**使い分け：** 担当先ごとの一種類の容量と、仕事単位の費用で決まる割当に向きます。実行開始時刻や同時稼働は12、仕事同士の相性は06、複数資源や共有物の一括消費は08が候補です。

#### サンプル：安さと機械の空き容量を両立する仕事の割当

同じ仕事でも、機械によって容量消費と費用が違います。各仕事に一つの候補を選び、どの機械も容量を超えないようにします。

**入力形式**

1行目は仕事数 `N`、機械数 `M`、割当候補数 `E`。2行目は各機械の容量。続く `E` 行は `仕事ID 機械ID 容量消費 費用` で、行の順が候補IDになります。

**実行コード：`example04.cpp`**

```cpp
// 目的：全ての仕事を機械へ割り当て、各機械の容量内で総費用を小さくします。
// 仕事ごとに最安の機械を選ぶだけでは、同じ機械に負荷が集中する場合があります。

#include "generalized_assignment_solver_v13.hpp"
using namespace std;

int main() {
    // 仕事数、機械数、選べる割当案の数を読み込みます。IDはそれぞれ0から始まります。
    int jobs, machines, option_count;
    cin >> jobs >> machines >> option_count;
    // capacities[a]は機械aが引き受けられる負荷の上限です。
    vector<long long> capacities(machines);
    for (auto& capacity : capacities) {
        cin >> capacity;
    }

    // 各仕事について、登録した割当案のうち1つを選ぶ問題を作ります。
    // 登録しなかった仕事と機械の組合せは、割当候補になりません。
    generalized_assignment_solver solver(jobs, machines, capacities);
    for (int i = 0; i < option_count; ++i) {
        int job, machine;
        long long demand, cost;
        cin >> job >> machine >> demand >> cost;
        // 同じ仕事でも、担当機械に応じて消費量と費用を変えられます。
        // demandは機械の容量消費、costは最小化する費用です。単位や値が同じとは限りません。
        solver.add_option(job, machine, demand, cost);
    }

    // 最大2000回まで探索し、seedを固定して再現しやすくします。
    generalized_assignment_solver::solve_options options;
    options.iteration_limit = 2000;
    options.seed = 1;
    auto result = solver.solve(options);
    // 全仕事の割当と全機械の容量条件を満たす結果か、先に確認します。
    if (!result.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // objectiveは選んだ割当案の費用合計です。
    // option_idsはadd_optionで登録した順の通し番号で、仕事ごとの番号ではありません。
    cout << "cost " << result.objective << '\n';
    for (int job = 0; job < jobs; ++job) {
        cout << "job " << job
             << " machine " << result.agent_of_job[job]
             << " option " << result.option_ids[job] << '\n';
    }
    // 機械ID順に負荷を表示します。入力したcapacitiesと比べて余裕を確認できます。
    cout << "load";
    for (long long load : result.load_of_agent) {
        cout << ' ' << load;
    }
    cout << '\n';
}
```

**入力例**

```text
3 2 6
5 5
0 0 4 1
0 1 2 4
1 0 3 1
1 1 3 3
2 0 2 2
2 1 3 1
```

**出力例**

```text
cost 6
job 0 machine 1 option 1
job 1 machine 0 option 2
job 2 machine 1 option 5
load 3 5
```

**結果の読み方**

各 `job` 行は `仕事ID machine 機械ID option 候補ID` です。`load` は機械別の使用容量です。全仕事を単独で最安の機械へ割り当てると容量超過になるため、仕事0を機械1へ回すなどの調整が必要です。

### 05. 施設配置（`facility_location`）

ファイル：`facility_location_v13.hpp`

**候補地から指定した数の拠点を選び、利用者全体の対応費用を減らします。** 拠点を選んだ後、各利用者は選択済み拠点の中で最も安いものを利用します。

例えば、20か所の候補地から配送拠点を3か所選び、100人の利用者までの距離合計を小さくする問題です。重要な利用者や利用頻度の高い利用者には、大きい重みを付けられます。

距離は地図上の直線距離である必要はありません。「拠点Aが利用者Xへ対応する費用」を数値表で与えればよいため、移動時間、処理費用、代表点との不一致度などにも使えます。

さらに、次を表現できます。

- 拠点ごとの設置費用・選択費用。
- 利用者ごとの外部サービス費用。選択拠点より外注が安ければ外注する。
- 必ず残す既存拠点と、追加・交換を検討する候補地の限定。

**用意するものと結果：** 候補拠点と利用者の対応費用、利用者の重み、選ぶ拠点数を渡すと、拠点集合と利用者ごとの対応先が得られます。

**使い分け：** この実装では拠点数を固定し、各利用者の費用の合計を評価します。拠点の処理容量、最も遠い利用者の距離の最小化、拠点同士をつなぐ費用は標準の対象外です。「施設配置」という名称だけから、これらも自動で扱えるとは考えないのが選定のポイントです。

#### サンプル：一本道の町に配送拠点を2か所置く

利用者の座標と利用頻度を与え、候補地から指定数の拠点を選びます。各利用者は選ばれた拠点のうち最も近いものを使い、「距離×利用頻度」の合計を小さくします。

**入力形式**

1行目は利用者数 `N`、候補地数 `M`、選ぶ拠点数 `K`。続く `N` 行は利用者の `座標 重み`。最後の1行は `M` 個の候補地の座標です。

**実行コード：`example05.cpp`**

```cpp
// 目的：候補地からkか所の拠点を開き、利用者の重要度を考慮した移動費用を減らします。
// 拠点の場所と、各利用者をどの拠点へ割り当てるかを一緒に決めます。

#include "facility_location_v13.hpp"
using namespace std;

int main() {
    // 利用者数n、拠点候補数m、開く拠点数kを読み込みます。
    // この例では、利用者と拠点を同じ直線上の座標で表します。
    int n, m, k;
    cin >> n >> m >> k;
    facility_location_problem<long long> problem;
    problem.demand_count = n;
    problem.candidate_count = m;
    problem.demand_weight.resize(n);

    // 利用者には座標と重みを設定します。重み2なら、距離1につき費用2と評価します。
    // 拠点候補IDは0〜m-1で、入力された座標の順に対応します。
    vector<long long> customer_x(n), facility_x(m);
    for (int i = 0; i < n; ++i) {
        cin >> customer_x[i] >> problem.demand_weight[i];
    }
    for (auto& x : facility_x) {
        cin >> x;
    }

    // 費用表は「拠点を先、利用者を後」に並べる1次元配列です。
    problem.cost.resize(static_cast<size_t>(m) * n);
    for (int f = 0; f < m; ++f) {
        for (int i = 0; i < n; ++i) {
            // ここには距離だけを入れます。利用者の重みはsolver側で掛けるため、二重に掛けません。
            problem.cost[f * n + i] = abs(facility_x[f] - customer_x[i]);
        }
    }

    // 拠点の組合せをいったん変えてやり直す回数の上限を16にし、seedを固定します。
    facility_location_options options;
    options.seed = 1;
    options.max_perturbations = 16;
    // 選んだ拠点の中から各利用者に安い拠点を割り当てます。拠点の収容人数は制限しません。
    auto result = solve_k_median(problem, k, options);

    // facilitiesは開いた拠点IDの集合です。表示順を変えても割当先のIDは変わりません。
    sort(result.facilities.begin(), result.facilities.end());
    cout << "cost " << result.cost << '\n';
    cout << "facilities";
    for (int f : result.facilities) {
        cout << ' ' << f;
    }
    // assignment[i]は利用者iの担当拠点IDです。座標やfacilities内の添字ではありません。
    cout << "\nassignment";
    for (int f : result.assignment) {
        cout << ' ' << f;
    }
    cout << '\n';
}
```

**入力例**

```text
4 3 2
0 2
2 1
9 1
10 2
0 5 10
```

**出力例**

```text
cost 3
facilities 0 2
assignment 0 0 2 2
```

**結果の読み方**

`facilities` は選んだ候補地のID、`assignment` は利用者順の担当拠点IDです。この例では座標0と10の拠点を選ぶと、距離×重みの合計は3になります。拠点のIDと座標を区別して読みます。

### 06. 容量制約付きグラフラベリング（`capacitated_graph_labeling`）

ファイル：`capacitated_graph_labeling_v25.hpp`

**要素をグループへ割り当てるとき、個々の割当の良し悪しと、要素同士の関係を合わせて評価します。** ここでいうラベルは、担当者、日、設備、チームなどの識別番号です。

例えば、仕事を3日間へ振り分けます。仕事ごとに実施しやすい日があり、同じ日にまとめたい仕事の組もあります。一方、各日の作業量や仕事数には上下限があります。これらを満たすように実施日を決める問題です。

主な評価と条件は次のとおりです。

- **単独の割当費用：** 仕事Aを1日目へ置く費用、2日目へ置く費用など。
- **ペアの費用：** 関係する二つの仕事の割当先から決まる費用。同じグループに集めたい関係も、離したい関係も表せる。
- 各グループの総需要量と要素数の下限・上限。
- 要素ごとの固定ラベル・許可ラベル。

ペア費用は「別グループなら減点」という形だけでなく、両端のラベルの組合せに応じた行列や独自関数でも指定できます。分割、相性を考慮した担当決め、同じ色を避けたい彩色の費用最小化などに使えます。

**用意するものと結果：** 要素の需要量、グループ容量、単独の割当費用、関係するペアを渡すと、各要素のラベルが得られます。

**使い分け：** 相性は費用として扱います。「同じグループの頂点が一つにつながる」という連結性は07、3個以上の要素が共同で使う物の費用は08が直接扱います。ペアに大きな罰金を付けることと、必ず守る制約を登録することも区別してください。

#### サンプル：通信の多い仕事を、人数をそろえたチームにまとめる

仕事同士の通信量を辺の重みで与えます。別チームへ分かれた辺だけに費用を払い、各チームの仕事数は指定値ちょうどにします。

**入力形式**

1行目は仕事数 `N`、辺数 `M`、チーム数 `K`。2行目は各チームの仕事数で、合計を `N` にします。続く `M` 行は `仕事ID 仕事ID 通信量` です。仕事0はチーム0に固定します。

**実行コード：`example06.cpp`**

```cpp
// 目的：人数を指定したチームに人を分け、関係の強い人同士が別チームになる費用を減らします。
// 人を頂点、所属チームをラベル、関係の強さを辺の重みとして表します。

#include "capacitated_graph_labeling_v25.hpp"
using namespace std;

int main() {
    // 人数n、関係を表す辺の数m、チーム数kを読み込みます。
    // 人IDは0〜n-1、チームIDは0〜k-1です。
    int n, m, k;
    cin >> n >> m >> k;

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = n;
    problem.label_count = k;
    // 各人がチームの容量を1人分だけ消費するように設定します。
    problem.demand.assign(n, 1);
    problem.lower_capacity.resize(k);
    for (auto& count : problem.lower_capacity) {
        cin >> count;
    }
    // 需要量を全て1、容量の下限と上限を同じにして、人数を固定します。
    problem.upper_capacity = problem.lower_capacity;
    // unary_cost[v * k + c]は人vをチームcに置く個別費用です。
    // この例では個人の希望を考慮せず、人同士の関係だけを評価するので全て0です。
    problem.unary_cost.assign(static_cast<size_t>(n) * k, 0);
    // -1は所属未固定です。人0だけチーム0に固定し、基準となる所属を決めます。
    problem.fixed_label.assign(n, -1);
    problem.fixed_label[0] = 0;
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long weight;
        cin >> u >> v >> weight;
        // 最後の0は辺の種類です。この例では全て同じ種類のPotts費用で評価します。
        problem.edges.push_back({u, v, weight, 0});
    }

    // 時間制限を無効にし、最大3000回探索します。seedは再現用に固定します。
    capacitated_graph_labeling_options options;
    options.time_limit_ms = -1;
    options.iteration_limit = 3000;
    options.seed = 1;
    // Potts費用は、辺の両端が別チームになったときだけ辺重みを払います。
    auto result = solve_capacitated_graph_labeling(
        problem, capacitated_potts_cost<long long>{}, options);
    // 固定した所属と各チームの人数を満たした結果か確認します。
    if (!result.feasible) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // costは別チームを結ぶ辺の重み合計、labelのi番目は人iの所属です。
    // このモデルでは、同じチームの人がグラフ上で連結することまでは要求していません。
    cout << "cost " << result.objective << '\n';
    cout << "label";
    for (int team : result.label) {
        cout << ' ' << team;
    }
    cout << '\n';
}
```

**入力例**

```text
6 7 2
3 3
0 1 5
1 2 5
0 2 5
3 4 5
4 5 5
3 5 5
2 3 1
```

**出力例**

```text
cost 1
label 0 0 0 1 1 1
```

**結果の読み方**

`label` が仕事ごとのチーム番号です。0・1・2と3・4・5をそれぞれまとめると、チーム間の通信は辺2–3の1だけになります。連結性を条件にする必要がなく、ペアの相性が中心の分割です。

### 07. 連結性付き領域分割（`connected_partition`）

ファイル：`connected_partition_solver_v12.hpp`

**グリッドやグラフの各頂点を領域へ割り当て、各領域を飛び地のない形に保ちながら改善します。** 領域内のどの頂点からでも、その領域だけを通って別の頂点へ行けることが「連結」です。

例えば、町のマスを複数の配送担当区域へ分けます。各区域の仕事量を一定範囲に収め、境界を短くし、指定した拠点の周囲は担当を固定したい、という問題です。

表現できる主な条件は次のとおりです。

- 領域ごとの連結性、頂点数、複数種類の負荷の上下限。
- 頂点の所属先固定、許可する所属先の限定。
- 必須領域と、空でもよい領域。使用する領域数の上下限。
- 指定した二領域を接触させる、接触させない、接触辺数を一定範囲にする条件。
- 連結性を判断する辺、接触を数える辺、費用を評価する辺の使い分け。

標準では、領域をまたぐ辺の費用、頂点の割当費用、領域の使用費用、負荷の目標からのずれを評価できます。独自評価では、領域ごとの負荷・特徴量の合計や境界長、全体の割当を利用できます。例えば、各頂点の座標や座標の二乗を特徴量として与えれば、領域の広がりを評価する関数を利用者側で組み立てられます。

**用意するものと結果：** 隣接関係、領域ごとの条件、必要な負荷・費用を渡すと、各頂点の所属領域が得られます。初期構築、既存分割の改善、部分修復に使えます。

**使い分け：** 「飛び地を作らない」が中心ならこのsolverです。連結であっても内部に穴はできるため、穴の禁止は別条件です。また、未使用マスを表したい場合は、連結性を要求しない背景用の領域を用意する、といった表現にします。

#### サンプル：飛び地を作らず、担当区域の境界を短くする

長方形の地図を、同じ面積の2区域へ分けます。入力の連結な初期分割から出発し、両区域の連結性と面積を守って、境界の辺数を減らします。

**入力形式**

1行目は高さ `H` と幅 `W`。続く `H` 行は各マスの初期区域IDで、0または1です。マス数は偶数とし、初期状態で両区域がそれぞれ連結かつ半分ずつの面積を持つ入力を使います。左上は区域0、右下は区域1に固定します。

**実行コード：`example07.cpp`**

```cpp
// 目的：マス目を2つの同じ面積の区域に分け、飛び地を作らず境界を短くします。
// 入力で与えた実行可能な区分けを出発点にして、境界を改善します。

#include "connected_partition_solver_v12.hpp"
using namespace std;

int main() {
    // 縦横の大きさと、各マスの初期区域ID（0または1）を読み込みます。
    // この例ではマス数を偶数とし、初期区分けも面積・連結性・固定条件を満たすものにします。
    int height, width;
    cin >> height >> width;
    const int n = height * width;
    vector<int> initial(n);
    for (int& region : initial) {
        cin >> region;
    }

    using Solver = ConnectedPartitionSolver<int64_t>;
    // 1マスを1頂点として扱います。区域ごとの下限と上限を同じにして面積を固定します。
    Solver::Problem problem(n, 2);
    for (auto& rule : problem.regions) {
        rule.min_vertices = n / 2;
        rule.max_vertices = n / 2;
        // 同じ区域のマスを上下左右にたどれることを必須条件にします。
        rule.connected = true;
    }
    // -1は変更可能なマスです。左上を区域0、右下を区域1に固定します。
    problem.fixed.assign(n, -1);
    problem.fixed[0] = 0;
    problem.fixed[n - 1] = 1;

    // 上下左右の隣接関係を、右と下へ1本ずつ登録します。
    // 辺は連結性の判定にも使い、区域をまたぐと費用1が発生します。
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 2次元の座標(y, x)を1次元の頂点IDへ変換します。出力でも同じ対応を使います。
            int v = y * width + x;
            if (x + 1 < width) {
                problem.edges.push_back({v, v + 1});
            }
            if (y + 1 < height) {
                problem.edges.push_back({v, v + width});
            }
        }
    }

    Solver solver(problem);
    // 時計による打切りを無効にし、最大3000ステップ改善します。seedは固定します。
    Solver::Options options;
    options.budget_us = -1;
    options.max_steps = 3000;
    options.seed = 1;
    // improveは既存の区分けを改善する入口です。initialに区域IDの配列を渡します。
    auto result = solver.improve(initial, options);
    // best_costは値が未取得の場合もあるため、存在を確認してから参照します。
    // 費用0も有効な値です。この判定は「費用が0か」ではなく「値があるか」を見ています。
    if (!result.best_cost) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "boundary " << *result.best_cost << '\n';
    // 探索で見つけた最良の区域IDを取得し、元のマス目の形に戻して表示します。
    auto labels = solver.best_labels();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            cout << (x == 0 ? "" : " ") << labels[y * width + x];
        }
        cout << '\n';
    }
}
```

**入力例**

```text
3 4
0 0 0 1
0 0 1 1
0 1 1 1
```

**出力例**

```text
boundary 3
0 0 1 1
0 0 1 1
0 0 1 1
```

**結果の読み方**

`boundary` は区域間の境界辺数、その後の行が改善後の地図です。初期分割の境界は5本です。左右に6マスずつ分けると境界を3本に減らせます。面積が同じでも、飛び地を含む分割は採用されません。

### 08. 容量制約付きハイパーグラフ分割（`hypergraph_partitioner`）

ファイル：`hypergraph_partitioner_v08.hpp`

**複数の要素が共有する物を意識して、要素をグループへ割り当てます。** ハイパーグラフの「辺」は、2要素のペアに限らず、同じ工具・データ・材料を使う要素の集合を表します。

例えば、同じ工具を使う仕事が4個あります。同じ機械へまとめれば工具の準備は1回、2台へ分ければ2回です。準備した工具が作業台の容量を占有する場合、その容量も「同じ機械に対象の仕事が1個でもあれば1回だけ」消費します。仕事を単純にペアへ分解すると表しにくい、この共有構造を直接扱えます。

集合ごとに、次の三つの個数を評価・制約に使えます。

| 数えるもの | 表現できる例 |
|---|---|
| 集合が分散したグループ数 | 複数グループに分かれたら減点する。2個目以降の使用先ごとに準備費用を払う |
| 各グループ内に入った集合の要素数 | 同種作業は1グループにつき高々3個。グループ内の個数に応じた費用を付ける |
| 集合から採用した要素の総数 | 任意採用の仕事から最低2個を採る。採用数に応じて費用を変える |

これに加え、要素ごとの割当費用、グループごとの開設費用、複数資源の容量、負荷の偏りへの減点、固定・許可割当を設定できます。要素を未採用にして、その代わりに費用を払う選択も可能です。共有物の費用・容量消費や要素自身の消費量は、配置先によって変えられます。

**用意するものと結果：** 要素、グループ、共有物ごとの利用要素一覧を渡すと、要素の割当先または未採用という結果が得られます。

**使い分け：** 「複数要素が同居すると費用や資源を共有できる」が決め手です。名称に含まれる分割は、空間的な連結性を意味しません。領域を地続きにする条件は07で扱います。

#### サンプル：工具を共有する仕事を同じ機械にまとめる

各仕事を機械へ割り当てます。機械には仕事数と工具数の容量があり、同じ工具を使う仕事を何個載せても、その工具の費用と工具容量は機械ごとに1回だけ発生します。

**入力形式**

1行目は仕事数 `N`、機械数 `K`、工具数 `T`。続く `K` 行は `仕事容量 工具容量`。続く `T` 行は `工具の準備費用 利用仕事数 仕事ID…` です。仕事0は機械0に固定します。

**実行コード：`example08.cpp`**

```cpp
// 目的：同じ工具を使う仕事をなるべく同じ機械へ集め、工具の準備費用を減らします。
// 各機械には、仕事数と工具数の2種類の容量制限を設けます。

#include "hypergraph_partitioner_v08.hpp"
using namespace std;

int main() {
    // 仕事数n、機械数k、工具の種類数を読み込みます。仕事ID・機械IDは0から始まります。
    int n, k, tool_count;
    cin >> n >> k >> tool_count;
    using Solver = HypergraphPartitioner;

    // 資源0は仕事数、資源1は準備した工具の数です。
    // 各仕事の既定負荷は {1, 0} なので、そのまま使えます。
    Solver::Problem problem(n, k, 2);
    // 機械ごとに、担当できる仕事数の上限と、備え付けられる工具数の上限を読みます。
    for (auto& group : problem.groups) {
        cin >> group.upper[0] >> group.upper[1];
    }
    // 仕事0は機械0で行うという固定条件を付けます。他の仕事の担当先は探索で決めます。
    problem.vertices[0].fixed = 0;

    // 1つの工具を共用する仕事の集合を、1本のハイパー辺として登録します。
    // 通常の辺と異なり、3つ以上の仕事の関係もまとめて表せます。
    for (int t = 0; t < tool_count; ++t) {
        double setup_cost;
        int count;
        cin >> setup_cost >> count;
        vector<int> jobs(count);
        for (int& job : jobs) {
            cin >> job;
        }

        // linear(費用) は「この工具を使う機械数×費用」です。
        // 同じ機械に利用仕事が何個あっても、その機械での工具準備費用は1回分です。
        int edge = problem.add_hyperedge(
            jobs, Solver::CountCost::linear(setup_cost));
        // 工具の容量消費は、利用仕事数によらず機械ごとに1です。
        problem.hyperedges[edge].shared_load = {0, 1};
    }

    // seedを1に固定し、100000マイクロ秒（100ミリ秒）の予算で割当を探索します。
    Solver solver(problem, 1);
    Solver::Options options;
    options.time_limit_us = 100000;
    auto result = solver.solve(options);
    // 仕事数と工具数の両方の容量、および仕事0の固定条件を確認してから結果を使います。
    if (!result.feasible) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // setup_costは全機械で準備する工具の費用合計です。
    // assignmentのi番目は仕事iの担当機械IDです。
    cout << "setup_cost " << result.cost << '\n';
    cout << "assignment";
    for (int machine : result.assignment) {
        cout << ' ' << machine;
    }
    cout << '\n';
}
```

**入力例**

```text
6 2 2
3 1
3 1
8 3 0 2 4
5 3 1 3 5
```

**出力例**

```text
setup_cost 13
assignment 0 1 0 1 0 1
```

**結果の読み方**

`assignment` は仕事順の機械IDです。同じ工具を使う3仕事ずつをまとめると、工具の準備費用は8 + 5 = 13です。各機械で工具は1個しか使えないため、工具の違う仕事を混ぜることはできません。

### 09. 二次割当（`qap`）

ファイル：`qap_solver_v09.hpp`

**物体を用意された位置へ一対一に置き、物体間の関係と位置間の距離から決まる費用を小さくします。** 一つの位置には一つの物体を置き、各物体は必ずどこか一つへ配置します。

例えば、工場の各区画へ機械を配置します。機械AとBの間では部品を頻繁に運ぶため近くに置き、ほとんどやり取りのない機械は離れていてもよいとします。評価するのは、各機械の組についての「運ぶ量 × 配置した区画間の移動費用」の合計です。

表現できる主な内容は次のとおりです。

- 物体の組ごとの関係の強さと、位置の組ごとの費用。
- 物体Aを位置3へ置くと費用がかかる、といった単独の配置費用。
- 行きと帰りで違う費用や、正負の評価値。
- 既存配置の一部だけを入れ替える部分改善。

空き位置が必要なら、相互作用も配置費用も0のダミー物体を入れて表現できます。通信量の多い機器の配置、よく連続して使う設備のレイアウト、対応関係を保ちたい対象の位置合わせなどが候補になります。

**用意するものと結果：** 物体間の関係の表、位置間の費用の表、必要なら単独の配置費用を渡すと、各物体の配置位置が得られます。

**使い分け：** 位置はあらかじめ用意されており、座標そのものは探索しません。また、ペア費用は上記の積へ分解できる形が基本です。任意の配置禁止は専用の必須条件として扱わず、固定配置には部分改善の機能を使います。座標を動かして長方形を詰めるなら10、実数座標を直接調整するなら19が候補です。

#### サンプル：よく部品を運ぶ機械同士を近くに置く

機械を、用意された位置へ一対一に配置します。機械間の運搬量と、配置先間の距離の積を合計するため、単に各機械の都合だけで位置を選ぶ問題ではありません。

**入力形式**

1行目は機械数兼位置数 `N`。次の行は各位置の座標。続く `N` 行は運搬量の `N×N` 行列で、行i・列jは機械iからjへ運ぶ量です。

**実行コード：`example09.cpp`**

```cpp
// 目的：やり取りの多い機械を近くに配置し、「流量×距離」の合計を小さくします。
// 用意されたnか所へn台を一対一に置くため、1台ずつ独立には配置を決められません。

#include "qap_solver_v09.hpp"
using namespace std;

int main() {
    // 機械数と配置場所の数は同じnです。場所IDは入力座標の順に0〜n-1となります。
    int n;
    cin >> n;
    vector<int> position_x(n);
    for (int& x : position_x) {
        cin >> x;
    }

    // 流量と距離はint、積を足し合わせる総費用はlong longで扱います。
    // flow[i * n + j]は機械iから機械jへの流量です。行ごとにn個ずつ読み込みます。
    qap_problem<int, long long> problem(n);
    for (int& flow : problem.flow) {
        cin >> flow;
    }
    // distance[a * n + b]は場所aと場所bの距離です。機械IDとは区別します。
    for (int a = 0; a < n; ++a) {
        for (int b = 0; b < n; ++b) {
            problem.distance[a * n + b] = abs(position_x[a] - position_x[b]);
        }
    }

    // 位置の座標は入力で固定され、探索するのは機械と位置の対応です。
    auto deadline = chrono::steady_clock::now() + chrono::milliseconds(100);
    // 期限は現在から100ミリ秒後、第3引数の1は乱数seedです。
    auto result = solve_qap(problem, deadline, 1);
    // costは全ての順序付き機械ペアの「流量×配置先間の距離」を足した値です。
    // 入力で両方向の流量を与えた場合、往路と復路はそれぞれ費用に含まれます。
    cout << "cost " << result.cost << '\n';
    // locationのi番目は、機械iを置いた場所IDです。訪問順序の配列ではありません。
    cout << "location";
    for (int location : result.location_of) {
        cout << ' ' << location;
    }
    cout << '\n';
}
```

**入力例**

```text
4
0 1 10 11
0 7 0 0
7 0 1 0
0 1 0 5
0 0 5 0
```

**出力例**

```text
cost 42
location 0 1 2 3
```

**結果の読み方**

`location` は機械ID順の配置先IDです。費用は全ての有向ペアを足すので、対称行列の往路・復路は両方数えます。この入力では、運搬量7の組と5の組をそれぞれ隣接位置へ置く配置が有利です。

### 10. 矩形パッキング（`rectangle_packing`）

ファイル：`rectangle_packing_v11.hpp`

**長方形同士が重ならないように、位置と90度回転の有無を決めます。** 対象とする枠の形によって、いくつかの問題を解き分けられます。

| 問題の形 | 決めたいこと | 例 |
|---|---|---|
| 大きさ固定の箱が1個 | 必須矩形を置いたうえで、収容する矩形の利益を大きくする | 限られた盤面に価値の高い部品を置く |
| 幅が固定された帯 | 全矩形を置き、使う高さを小さくする | 幅が決まった紙への面付け |
| 同じ大きさの箱が複数 | 全矩形を置き、使用箱数を小さくする | 部品を少ない枚数の板へまとめる |
| 外側の枠も変更可能 | 全矩形を囲む面積・周長・重み付き幅高さを小さくする | 全部品をコンパクトにまとめる |

矩形ごとに回転を許すかどうかを指定できます。固定箱では必須矩形と任意矩形を混在させ、利益の低い任意矩形を見送ることもできます。帯・複数箱・外接枠の問題は、全矩形を配置する設定です。

例えば、すでに多数の部品を置いた盤面で、一部だけを取り外して詰め直す使い方もできます。固定箱の部分修復では、動かさない矩形を障害物として残せます。

**用意するものと結果：** 各矩形の幅・高さ、回転可否、必要に応じて利益や枠サイズを渡すと、各矩形の座標・向き・使用箱が得られます。

**使い分け：** 整数座標の、軸に平行な長方形が対象です。自由な角度の回転、多角形、切断手順の制約までは扱いません。また、必須矩形を置けなかった結果も返り得るため、配置の成否を確認して使います。

#### サンプル：必須部品を置き、残りの板に利益の高い部品を詰める

大きさ固定の板へ長方形を重ならないように置きます。必須部品は必ず置き、任意部品は入るものを選び、収容利益を大きくします。座標と90度回転の有無もsolverが決めます。

**入力形式**

1行目は板の幅、高さ、部品数 `N`。続く `N` 行は `幅 高さ 利益 回転可否 必須か`。末尾の2値は真なら1、偽なら0です。

**実行コード：`example10.cpp`**

```cpp
// 目的：大きさが決まった板の中に部品を重ならずに置き、配置した部品の利益を最大にします。
// 必須部品を置く条件を優先し、残りの空間に入る任意部品を選びます。

#include "rectangle_packing_v11.hpp"
using namespace std;

int main() {
    // 板の幅・高さ・部品数を読み込みます。寸法には正の整数を使います。
    long long width, height;
    int n;
    cin >> width >> height >> n;
    vector<rectangle_pack_item> items(n);
    // 部品ごとに幅、高さ、利益、回転可否、必須かどうかを入力します。
    // 最後の2項目は0または1です。rotatable=1なら90度回転を許します。
    for (auto& item : items) {
        cin >> item.width >> item.height >> item.profit
            >> item.rotatable >> item.required;
    }

    // 異なる詰め方を試す再開始回数の上限を32にし、seedを固定します。
    rectangle_pack_options options;
    options.seed = 1;
    options.restart_limit = 32;
    // 板の大きさを固定する入口です。板の内側に収まる配置と、採用する部品を探索します。
    auto result = pack_rectangles_fixed_bin(items, width, height, options);
    if (!result.feasible()) {
        // 任意部品を見送るのは正常ですが、必須部品の未配置は失敗です。
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // placed_profitは、実際に配置できた部品の利益だけを足した値です。
    cout << "profit " << result.placed_profit << '\n';
    for (int i = 0; i < n; ++i) {
        // placementsは入力と同じ部品順です。未配置部品も残るのでplaced()で判定します。
        const auto& placement = result.placements[i];
        cout << "item " << i;
        if (!placement.placed()) {
            cout << " skipped\n";
            continue;
        }
        // widthとheightには回転が反映済みです。
        // xとyは板の左上を原点とした部品の左上座標です。wとhと合わせて配置を復元できます。
        cout << " x " << placement.x << " y " << placement.y
             << " w " << placement.width << " h " << placement.height << '\n';
    }
}
```

**入力例**

```text
6 4 4
3 4 5 1 1
3 2 6 1 0
3 2 5 1 0
2 2 4 1 0
```

**出力例**

```text
profit 16
item 0 x 0 y 0 w 3 h 4
item 1 x 3 y 0 w 3 h 2
item 2 x 3 y 2 w 3 h 2
item 3 skipped
```

**結果の読み方**

各 `item` 行の `x y w h` は左上座標と回転後の寸法です。`skipped` は未採用の任意部品を表します。全4個は面積だけでも板に収まらず、この例では必須部品0と任意部品1・2を収めると利益16です。

### 11. 重み付き順序最適化（`linear_ordering`）

ファイル：`linear_ordering_solver_v10.hpp`

**すべての要素を一列に並べ、前後関係・位置・隣接関係から決まる費用を小さくします。** 前後関係の評価は、要素が隣り合っていなくても効きます。

例えば、「AはBより前がよい」「BはCより前がよい」「CはAより前がよい」という矛盾した希望があるとします。希望を破ったときの損失を設定し、重要な希望をなるべく守る順序を探します。

この実装では、次の3種類の費用を合わせて使えます。

- **前後関係の費用：** AをBより前へ置くと発生する費用、または希望する順序を逆にした場合の罰金。
- **位置の費用：** Aを何番目に置くかで変わる費用。
- **有向の隣接費用：** Aの直後がBである場合の費用。逆順では別の値を設定できる。

さらに、「Aは必ずBより前」という先行制約、置ける位置の範囲・集合、位置固定も指定できます。途中まで決めた順序を保ち、残りだけを並べ直す用途にも向きます。

**用意するものと結果：** 要素間の希望・費用と必要な位置条件を渡すと、全要素を1回ずつ含む順序が得られます。

**使い分け：** 前後関係が中心ならこのsolverです。隣り合う地点間の移動距離だけなら13、作業時間や機械の同時使用まで扱うなら12が自然です。このsolverの「3番目」は順位であり、実時間の「3秒目」ではありません。

#### サンプル：矛盾する順番の希望から、損失の小さい順序を選ぶ

希望する前後関係を逆にしたときの罰金を与えます。希望が循環して全部を守れない場合でも、損失の小さい順番を探せます。必ず守る先行関係は別に登録します。

**入力形式**

1行目は要素数 `N`、希望数 `M`、必須先行関係数 `P`。続く `M` 行は `先にしたい要素 後にしたい要素 逆順時の罰金`、最後の `P` 行は `必ず先 必ず後` です。

**実行コード：`example11.cpp`**

```cpp
// 目的：必ず守る先行関係を満たしつつ、前後関係の希望に反する罰金を最小にします。
// 全ての項目を1回ずつ並べる順序を求めます。

#include "linear_ordering_solver_v10.hpp"
using namespace std;

int main() {
    // 項目数、前後関係の希望数、必須の先行関係数を読み込みます。項目IDは0〜n-1です。
    int n, preference_count, precedence_count;
    cin >> n >> preference_count >> precedence_count;
    using Solver = LinearOrderingSolver<int64_t>;
    Solver solver(n);

    // 希望は「beforeをafterより先に置きたい」という意味です。
    // 希望同士が矛盾しても、払う罰金が小さくなるように折り合いを付けられます。
    for (int i = 0; i < preference_count; ++i) {
        int before, after;
        int64_t penalty;
        cin >> before >> after >> penalty;
        // 希望は破ってもよく、逆順になった場合だけ罰金が発生します。
        solver.problem().add_preference(before, after, penalty);
    }
    // 必須の先行関係は、罰金を払って破ることはできません。
    for (int i = 0; i < precedence_count; ++i) {
        int before, after;
        cin >> before >> after;
        // こちらは費用ではなく、必ず満たす先行制約です。
        solver.problem().require_before(before, after);
    }

    // 100000マイクロ秒（100ミリ秒）を探索予算とし、seedを固定します。
    Solver::Options options;
    options.time_limit_us = 100000;
    options.seed = 1;
    auto result = solver.solve(options);
    // 必須の先行関係を満たす順序が得られた場合にだけ、結果を表示します。
    if (result.status != Solver::Status::Feasible) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // penaltyは逆順になった希望の罰金合計です。小さいほど希望をよく満たしています。
    cout << "penalty " << result.cost << '\n';
    // orderは先頭からの項目ID列です。order[i]は「位置iに置く項目」を表します。
    cout << "order";
    for (int item : result.order) {
        cout << ' ' << item;
    }
    cout << '\n';
}
```

**入力例**

```text
3 3 1
0 1 8
1 2 7
2 0 6
0 2
```

**出力例**

```text
penalty 6
order 0 1 2
```

**結果の読み方**

`order` は先頭からの要素IDです。0→1→2とすると、「2を0より前にしたい」という希望だけを諦め、罰金6を払います。必須条件の「0を2より前」は満たしています。

### 12. 汎用資源スケジューリング（`generic_resource_scheduler`）

ファイル：`generic_resource_scheduler_v09.hpp`

**作業の順序と資源の取り合いを考え、各作業の実行方法と開始時刻を決めます。** 資源は、機械、作業員、同時に使える設備数など、使用が終われば再び利用できるものです。

例えば、部品を切断してから組み立てます。切断には機械1台と作業員1人、組立には作業員2人が必要です。複数の部品を並行して処理しつつ、機械や人員の上限を超えず、全作業を早く終わらせたい場合に使えます。

主な条件は次のとおりです。

- 作業ごとの所要時間と、複数資源の同時使用量。
- 「Aが終わってからBを始める」という先行関係。
- 作業を開始できる最も早い時刻。
- 複数の実行方法。使う機械、所要時間、資源量、費用の異なる方法から選べる。

標準の評価では、全作業が終わる時刻、各作業の完了時刻、納期からの遅れ、実行方法の費用を重み付きで合計します。全体の終了を早めるだけでなく、重要な作業を先に終わらせる設定も可能です。

機械を容量1の資源とすれば、一度に一つの作業しかできない工程計画を表現できます。代替機械を実行方法として登録すれば、どの機械を使うかも選べます。

**用意するものと結果：** 作業、実行方法、先行関係、資源容量を渡すと、作業ごとの実行方法と開始時刻が得られます。

**使い分け：** 作業は途中で中断せず、資源容量は全期間で一定です。納期は遅れへの減点であり、絶対に越えてはいけない期限の専用制約ではありません。時間帯で変わる人員数、前の作業によって変わる段取り時間、使い切りの材料予算は標準の対象外です。独自評価も、単に開始を遅らせるだけでは改善しない性質を前提にします。

#### サンプル：2台の機械で、工程順を守って並行作業する

各作業に所要時間と必要な機械を指定します。同じ機械を同時に取り合わず、前工程の終了後に次工程を始め、全作業が終わる時刻を早くします。

**入力形式**

1行目は作業数 `N`、資源数 `R`、先行関係数 `P`。2行目は資源容量。続く `N` 行は `所要時間 資源0の使用量 … 資源R-1の使用量`、最後の `P` 行は `前作業ID 後作業ID` です。

**実行コード：`example12.cpp`**

```cpp
// 目的：作業の前後関係と人員・機械の容量を守り、全作業を早く終わらせます。
// 作業を単に並べるだけでなく、資源が足りる作業は並行して進めます。

#include "generic_resource_scheduler_v09.hpp"
using namespace std;

int main() {
    // 作業数、資源の種類数、先行関係の数を読み込みます。各IDは0から始まります。
    int n, resource_count, precedence_count;
    cin >> n >> resource_count >> precedence_count;
    resource_scheduling_problem problem;
    // capacityは各時刻に同時利用できる量です。作業終了後には再び使える資源を扱います。
    problem.resources.resize(resource_count);
    for (auto& resource : problem.resources) {
        cin >> resource.capacity;
    }

    problem.tasks.resize(n);
    for (auto& task : problem.tasks) {
        // 1つのmodeが、作業の実行時間と必要な資源量をまとめた実行方法です。
        scheduling_mode mode;
        cin >> mode.duration;
        for (int r = 0; r < resource_count; ++r) {
            int amount;
            cin >> amount;
            // amountは作業中ずっと必要な量で、実行時間を掛けた延べ使用量ではありません。
            // 使わない資源は登録を省きます。
            if (amount > 0) {
                mode.renewable_uses.push_back({r, amount});
            }
        }
        // この例は各作業1モード。別の実行方法があれば追加できます。
        task.modes.push_back(mode);
    }
    for (int i = 0; i < precedence_count; ++i) {
        int before, after;
        cin >> before >> after;
        // beforeが完了してからafterを開始できる、という先行関係を登録します。
        problem.precedences.push_back({before, after});
    }

    // 既定の評価は全作業の終了時刻。追加の費用は全て0です。
    resource_scheduling_options options;
    // 時計による打切りを無効にし、最大3000回探索します。seedは固定します。
    options.time_limit_ms = -1;
    options.max_iterations = 3000;
    options.seed = 1;
    auto result = resource_scheduling_solve(problem, options);
    // 求めた時刻表について、資源の同時使用量や先行関係を満たすことを確認します。
    if (!resource_scheduling_is_feasible(problem, result.schedule)) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    cout << "makespan " << result.objective << '\n';
    for (int i = 0; i < n; ++i) {
        // 選択された実行方法の所要時間を使い、終了時刻を計算します。
        // 資源を使う区間は開始時刻以上・終了時刻未満なので、終了と同時に次の作業を始められます。
        int mode = result.schedule.mode_of_task[i];
        long long start = result.schedule.start_time[i];
        long long finish = start + problem.tasks[i].modes[mode].duration;
        cout << "task " << i << " start " << start << " finish " << finish << '\n';
    }
}
```

**入力例**

```text
4 2 2
1 1
3 1 0
2 0 1
4 0 1
2 1 0
0 1
2 3
```

**出力例**

```text
makespan 6
task 0 start 0 finish 3
task 1 start 4 finish 6
task 2 start 0 finish 4
task 3 start 4 finish 6
```

**結果の読み方**

各 `task` 行は作業の開始・終了時刻です。区間は開始を含み終了を含まないため、前の作業の終了時刻に次を開始できます。作業0と2を時刻0に並行して始めると、全工程を時刻6までに終えられます。

### 13. 巡回セールスマン問題・ハミルトン路（`tsp`）

ファイル：`tsp_solver_v09.hpp`

**訪問する地点がすべて決まっているとき、それらを1回ずつ訪れる順序を選びます。** 最小化するのは、順番に移動するときの費用合計です。

例えば、ロボットが指定された20か所を点検する場合、点検箇所は変えずに、回る順番を工夫して移動を短くします。

次のような形を選べます。

- 最後に出発点へ戻る周回路。
- 最後の地点で終わる開いた経路。
- 開いた経路の始点・終点の固定。
- 行きと帰りで移動費用が違う非対称な移動。
- 既存の訪問順序からの改善。小規模問題向けの厳密解を求める入口もある。

費用は、ユークリッド距離だけでなく、あらかじめ計算したグラフ上の最短距離や操作回数でも構いません。

**用意するものと結果：** 訪問地点の一覧と、2地点間の静的な移動費用を返す関数を渡すと、訪問順序と総費用が得られます。道路上の具体的な通過経路が必要なら、地点間の最短経路の復元などを別途行います。

**使い分け：** 訪問先の採否、複数車両への担当分け、積載量は14の領域です。また、グラフの最短距離を費用として使った場合、復元した通過経路では同じ中継点を何度も通ることがあります。「1回ずつ」はsolverに渡した訪問項目の順序についての条件です。

#### サンプル：全ての点検地点を訪れ、出発点へ戻る

訪れる地点は全て必須です。格子状の道を想定し、縦横の移動距離で、1台が巡回する順番を求めます。

**入力形式**

1行目は地点数 `N`（2以上）。続く `N` 行は各地点の整数座標 `x y` です。

**実行コード：`example13.cpp`**

```cpp
// 目的：全ての地点を1回ずつ訪問して出発点へ戻る、総移動距離の短い巡回路を求めます。
// この例では、縦横にだけ移動できる街路を想定したマンハッタン距離を使います。

#include "tsp_solver_v09.hpp"
using namespace std;

int main() {
    // 地点数nと各地点の座標を読み込みます。地点IDは入力順の0〜n-1です。
    int n;
    cin >> n;
    vector<long long> x(n), y(n);
    for (int i = 0; i < n; ++i) {
        cin >> x[i] >> y[i];
    }
    // solverから地点IDを受け取り、2点間の移動費用を返す関数を用意します。
    // 座標配列を参照するので、solve_tspが終わるまで配列を保持します。
    auto distance = [&](int a, int b) {
        return abs(x[a] - x[b]) + abs(y[a] - y[b]);
    };

    // orderに訪問対象のIDを入れます。solve_tspが順序を書き換えます。
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    tsp::TSPParam options;
    // cycle=trueで出発点へ戻る閉路にします。戻らない経路とは評価する距離が異なります。
    options.cycle = true;
    // この距離はa→bとb→aで等しいため、対称な距離用の探索を指定できます。
    options.symmetric = true;
    options.seed = 1;
    // 100ミリ秒の制限に加え、移動候補の評価回数も10000までに抑えます。
    options.time_limit_ms = 100;
    options.max_move_evaluations = 10000;
    auto cost = tsp::solve_tsp(order, distance, options);

    cout << "distance " << cost << '\n';
    // 改善されたorderを訪問順に表示します。先頭の地点を固定する条件は付けていません。
    cout << "tour";
    for (int vertex : order) {
        cout << ' ' << vertex;
    }
    // 閉路の最後の移動も、上のcostに含まれています。
    // order内に出発点は重複させません。最後にもう一度表示するのは帰着を明示するためです。
    cout << ' ' << order.front() << '\n';
}
```

**入力例**

```text
4
0 0
3 0
3 2
0 2
```

**出力例**

```text
distance 10
tour 0 3 2 1 0
```

**結果の読み方**

`tour` は訪問順で、帰着先を分かりやすくするため先頭の地点を末尾にも表示します。末尾の帰着を除けば全地点が1回ずつ現れます。この長方形を一周する距離は10です。

### 14. 複数経路・配送計画（`routing`）

ファイル：`routing_solver_v07.hpp`

**複数車両について、どの仕事を引き受け、どの車両が、どの順番で訪問するかを決めます。** 車両ごとの始点・終点、容量、経路費用の上限を扱えます。

例えば、配達先を複数のトラックへ振り分け、各トラックの積載量を守りながら配送距離を減らす問題です。別の使い方として、制限距離内で価値の高い地点をなるべく多く回る計画も作れます。

| 選択の形式 | 主な目標 |
|---|---|
| すべて必須 | 全仕事を担当させ、総経路費用を小さくする |
| 任意の仕事を選ぶ | 必須仕事と容量・経路上限を守り、獲得報酬を最大化する。同じ報酬なら費用を小さくする |
| 実施件数を固定する | 指定件数を担当させ、総経路費用を小さくする |

仕事は、1地点を訪問する形式と、**集荷地点・配送地点の組**の形式を選べます。後者では同じ車両が集荷を先に行い、配送まで荷物を積載します。集荷と配送の間に別の仕事を挟むこともできます。

また、訪問時の作業費用、仕事ごとの報酬、必須・任意の区別を設定できます。地点間の費用は非対称でも構いません。Orienteeringの入口は経路上限で訪問先を選び、車両容量も使う場合は容量付きの入口を選びます。

**用意するものと結果：** 仕事、車両、地点間の移動費用を渡すと、車両別の訪問列、未実施仕事、総費用・獲得報酬が得られます。

**使い分け：** 配送の担当と訪問順序を決めるsolverです。訪問可能な時間帯や、道路上での車両同士の衝突は扱いません。また、この実装の報酬型は「報酬を優先して最大化し、同報酬なら費用を最小化」であり、報酬から距離を直接差し引く目的とは異なります。

#### サンプル：積載量を守りながら、2台で配送先を分担する

配送先をどちらの車両が担当するかと、訪問する順番を同時に決めます。各車両は倉庫から出発して戻り、担当した荷物の合計が積載容量を超えないようにします。

**入力形式**

1行目は配送先数 `N`、車両数 `V`。2行目は倉庫の座標、3行目は各車両の積載容量。続く `N` 行は配送先の `x y 荷物量`。倉庫IDは0、配送先IDは入力順に1からNです。

**実行コード：`example14.cpp`**

```cpp
// 目的：荷物を車両の積載量以内に分担し、全配送先を回る総移動距離を小さくします。
// 各車両の担当先と、その車両が訪れる順序の両方を決めます。

#include "routing_solver_v07.hpp"
using namespace std;

int main() {
    // 配送先数nと車両数を読み込みます。頂点0を倉庫、頂点1〜nを配送先とします。
    int n, vehicle_count;
    cin >> n >> vehicle_count;
    vector<long long> x(n + 1), y(n + 1);
    cin >> x[0] >> y[0];

    // 各車両は倉庫0を出発して倉庫0へ戻ります。capacityはその車両の積載量上限です。
    vector<routing::RouteVehicle<int>> vehicles(vehicle_count);
    for (auto& vehicle : vehicles) {
        vehicle.start = 0;
        vehicle.finish = 0;
        cin >> vehicle.capacity;
    }

    // 配送先の座標と荷物量demandを読み込みます。
    // 配列の仕事IDは0〜n-1ですが、対応する頂点IDは倉庫の分だけ1つずれます。
    vector<routing::RouteTask<int>> tasks(n);
    for (int i = 0; i < n; ++i) {
        cin >> x[i + 1] >> y[i + 1] >> tasks[i].demand;
        tasks[i].vertex = i + 1;
        // 全配送を必須にします。各車両が担当する荷物量の合計が、その積載量以下になります。
        tasks[i].mandatory = true;
    }
    // 2つの頂点間の移動費用は、縦方向と横方向の距離の和です。
    auto distance = [&](int a, int b) {
        return abs(x[a] - x[b]) + abs(y[a] - y[b]);
    };

    // 最大100ミリ秒、移動候補の評価は最大10000回とし、seedを固定します。
    routing::RoutingParam options;
    options.time_limit_ms = 100;
    options.max_move_evaluations = 10000;
    options.seed = 1;
    // spanは配列の参照を渡すための型です。tasksとvehiclesを保持したまま呼び出します。
    auto result = routing::solve_cvrp(
        span<const routing::RouteTask<int>>(tasks),
        span<const routing::RouteVehicle<int>>(vehicles),
        distance, options);
    // 全配送先への訪問と、各車両の積載量制約を満たした結果か確認します。
    if (!result.feasible) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // travel_costは、全車両の出発・配送先間の移動・帰着を合わせた距離です。
    cout << "distance " << result.travel_cost << '\n';
    for (int v = 0; v < vehicle_count; ++v) {
        // routesは配送先だけの列です。表示時に出発・帰着倉庫を補います。
        cout << "vehicle " << v << ' ' << vehicles[v].start;
        for (int vertex : result.routes[v]) {
            cout << ' ' << vertex;
        }
        cout << ' ' << vehicles[v].finish << '\n';
    }
}
```

**入力例**

```text
4 2
0 0
4 4
1 0 2
2 0 2
0 2 2
0 3 2
```

**出力例**

```text
distance 10
vehicle 0 0 2 1 0
vehicle 1 0 4 3 0
```

**結果の読み方**

各 `vehicle` 行に、倉庫を含めた訪問順を出力します。各車両が2件ずつ担当すれば荷物量は4で、容量内です。東側2件と北側2件を分担すると、総距離は4 + 6 = 10です。

### 15. 容量制約付き複数経路（`capacitated_router`）

ファイル：`capacitated_router_v05.hpp`

**始点と終点が決まった複数の接続要求に対し、容量を超えないように経路を引きます。** 各要求は同じ頂点を繰り返し通らない一本の経路で運び、複数経路へ分割しません。

例えば、配線Aを左端から右端へ、配線Bを上端から下端へ通します。各マスには配線を1本しか通せないため、片方を最短経路から迂回させる必要があります。このように、先に引いた経路も見直して接続を調整します。

表現できる主な内容は次のとおりです。

- 有向辺、または容量を往復方向で共有する無向辺。
- 辺・頂点の通過容量、要求ごとに異なる需要量。
- 複数の辺や頂点が共通して使う資源。例えば同じ通路区画全体の容量。
- 需要量に比例する消費と、1回使用するごとの固定消費。
- 必須接続と、未接続のままにして罰金を払う任意接続。
- 固定した既存経路、すでに外部で消費されている資源量。
- 要求別の使用禁止箇所・費用や、一つの要求が使える資源量の上限。

評価には経路の辺・頂点費用、任意要求を断る費用、資源使用量に応じた費用を使えます。資源費用は利用者が定義する非減少の関数なので、一定量を超えると急に高くなる設定なども表せます。未接続の罰金を費用と合算する方式と、罰金を先に最小化してから接続費用を詰める方式を選べます。

**用意するものと結果：** 通行可能なグラフ、接続要求、容量と費用を渡すと、要求ごとの通過辺列が得られます。

**使い分け：** 14が「どこをどの順で訪れるか」を決めるのに対し、こちらは「決まった2点間をどこに通すか」を決めます。需要2の要求を1本登録することと、需要1の要求を2本登録することでは、別々の経路へ分けられるかが違います。時刻ごとの衝突回避が必要なら18を検討します。

#### サンプル：狭い配線区間を取り合う2本の接続を通す

指定された始点・終点の組を、それぞれ経路で結びます。同じ辺の容量は全要求で共有するため、各要求の最短路を独立に引くだけでは、全てを通せない場合があります。

**入力形式**

1行目は頂点数 `N`、無向辺数 `M`、要求数 `K`。続く `M` 行は `端点 端点 費用 容量`、続く `K` 行は `始点 終点 需要量`。全要求を必須とします。

**実行コード：`example15.cpp`**

```cpp
// 目的：複数の接続要求を同時に通し、各辺の容量を超えずに経路費用を小さくします。
// 要求ごとに最短路を選ぶだけでは、狭い辺に利用が集中してしまう場合があります。

#include "capacitated_router_v05.hpp"
using namespace std;

int main() {
    // 頂点数n、無向辺数m、接続要求数を読み込みます。頂点IDは0〜n-1です。
    int n, m, request_count;
    cin >> n >> m >> request_count;
    using Router = CapacitatedRouter<int64_t>;
    Router::Problem problem(n);
    // 各辺の両端、通行費用、容量を登録します。容量は両方向の利用で共用します。
    for (int i = 0; i < m; ++i) {
        int u, v;
        int64_t cost, capacity;
        cin >> u >> v >> cost >> capacity;
        // 逆向きに使う場合も、同じ1本の辺の容量を共有します。
        problem.add_undirected_edge(u, v, cost, capacity);
    }
    // 接続要求ごとに始点、終点、必要量demandを登録します。
    // 通る辺ごとにdemandだけ容量を使い、同じ辺を通る全要求の使用量が合算されます。
    for (int i = 0; i < request_count; ++i) {
        int source, target;
        int64_t demand;
        cin >> source >> target >> demand;
        // この入口で追加した要求は必須です。この例では接続を省略して費用を減らせません。
        problem.add_request(source, target, demand);
    }

    // solverはproblemを参照するので、結果を使い終わるまで保持します。
    Router::Solver<> solver(problem);
    // 100000マイクロ秒（100ミリ秒）と3000反復を上限とし、seedを固定します。
    Router::Limits limits;
    limits.time_limit_us = 100000;
    limits.max_iterations = 3000;
    Router::Options options;
    options.seed = 1;
    auto result = solver.solve(limits, options);
    // 全要求の接続と容量制約を満たした結果か、評価値を参照する前に確認します。
    if (!result.feasible()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // この例では追加の頂点費用や資源費用を設定しないため、costは通る辺の費用合計です。
    // 同じ辺を複数の要求が通った場合、費用は要求ごとに加算されます。
    cout << "cost " << result.summary.value->total() << '\n';
    for (int i = 0; i < request_count; ++i) {
        cout << "request " << i << ' ' << problem.requests[i].source;
        // 戻り値は有向辺ID列なので、各辺の終点を順に表示します。
        for (int arc : result.solution.routes[i].arcs) {
            cout << ' ' << problem.arcs[arc].to;
        }
        cout << '\n';
    }
}
```

**入力例**

```text
6 6 2
0 1 1 1
3 1 1 1
1 2 1 1
2 4 1 1
2 5 1 1
0 4 5 1
0 4 1
3 5 1
```

**出力例**

```text
cost 8
request 0 0 4
request 1 3 1 2 5
```

**結果の読み方**

各 `request` 行が頂点列です。両方の最短路を選ぶと辺1–2の容量1を超えます。要求0を費用5の直通辺0–4へ回し、要求1を3–1–2–5へ通すと、両方を接続できます。

### 16. シュタイナー木（`heuristic_steiner_tree`）

ファイル：`heuristic_steiner_tree_v19.hpp`

**必ず接続したい地点を、必要な中継点も使って、できるだけ安く一つにつなぎます。** 決めるのは辺の集合であり、訪問順序ではありません。

例えば、離れた3か所へ通信線を引きます。各地点を個別に結ぶより、途中の交差点まで線を共有して、そこから枝分かれさせた方が安いことがあります。この共有部分も含めてネットワーク全体を選びます。選んだ辺の費用は、接続先の数によらず1回だけ数えます。

中継点は、入力グラフに存在する頂点から選びます。平面上の任意の位置へ分岐点を新しく作るわけではありません。

主に次の使い方ができます。

- 必須地点をつなぐネットワークの新規構築。
- 既存の連結したネットワークの改善。
- まだ分断されている候補ネットワークの修復。候補の不要な辺は削除してよい。
- 撤去できない既設ネットワークへの追加接続。既設部分の追加費用を0とし、必要な追加辺と追加費用を求める。

**用意するものと結果：** 非負の辺費用を持つ無向グラフと、接続必須の頂点一覧を渡すと、使う辺と総費用が得られます。

**使い分け：** すべての頂点を接続する必要はなく、必要な地点さえつながればよい点が特徴です。容量を取り合う複数要求は15、同時使用できない頂点がある場合は17が候補です。既設辺への追加機能では既設部分を残すので、全体が必ず木になるという意味ではありません。

#### サンプル：中継点を共有し、複数の施設を一つにつなぐ

接続必須の施設を全て同じネットワークへつなぎます。必須でない頂点も中継点として使えるため、施設同士を直接結ぶだけより安くできる場合があります。

**入力形式**

1行目は頂点数 `N`、無向辺数 `M`、接続必須点数 `K`。続く `M` 行は `端点 端点 費用`、最後の行は `K` 個の必須点IDです。辺IDは入力順に0から始まります。

**実行コード：`example16.cpp`**

```cpp
// 目的：必須の地点を1つのネットワークでつなぎ、使う辺の総費用を小さくします。
// 必須地点以外も中継点に使えるので、接続をまとめて安くできる場合があります。

#include "heuristic_steiner_tree_v19.hpp"
using namespace std;

int main() {
    // 頂点数n、辺数m、必須地点数を読み込みます。頂点IDは0〜n-1です。
    int n, m, terminal_count;
    cin >> n >> m >> terminal_count;
    heuristic_steiner_tree solver(n);
    // 無向辺の両端と敷設費用を登録します。辺IDはadd_edgeした順に0から付きます。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        cin >> u >> v >> cost;
        solver.add_edge(u, v, cost);
    }

    // terminalsには必ず接続する地点のIDだけを入れます。
    // それ以外の頂点は、使うと安くなる場合だけ中継点として選ばれます。
    vector<int> terminals(terminal_count);
    for (int& terminal : terminals) {
        cin >> terminal;
    }

    // 乱数seedを固定します。時間制限0の意味はAPIごとに異なるので、下の設定に注意します。
    heuristic_steiner_tree_options options;
    options.seed = 1;
    options.time_limit_ms = 0;  // このAPIでは0は回数制限での探索です。
    auto result = solver.solve(terminals, options);
    // 必須地点を接続できた場合にだけ、得られた辺集合を利用します。
    if (!result.ok) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 辺はネットワークとして1回だけ支払い、複数施設への接続で共有します。
    sort(result.edges.begin(), result.edges.end());
    cout << "cost " << result.cost << '\n';
    // edgesは採用した辺IDの集合です。入力の辺と照合するとネットワークを復元できます。
    // 辺の表示順はID順であり、端から端へ移動する経路の順序ではありません。
    cout << "edges";
    for (int edge : result.edges) {
        cout << ' ' << edge;
    }
    cout << '\n';
}
```

**入力例**

```text
4 6 3
0 3 2
1 3 2
2 3 2
0 1 5
1 2 5
0 2 5
0 1 2
```

**出力例**

```text
cost 6
edges 0 1 2
```

**結果の読み方**

`edges` は採用した辺のIDです。辺0・1・2を採ると、中継点3を通って施設0・1・2を費用6でつなげます。必須施設同士の直接辺だけを使う場合の費用10より安くなります。

### 17. 排他制約付きシュタイナー木（`constrained_steiner_tree`）

ファイル：`constrained_steiner_tree_v10.hpp`

**必要な地点をつなぐ際に、「この中継点とあの中継点は同時に使えない」という条件も守ります。** 基本の接続目的は16と同じで、使用辺の費用合計を小さくします。

例えば、候補地点AとBが同じ設備用地を使う二つの代替案だとします。どちらか一方は使えますが、同時には設置できません。両方を通る安いネットワークがあっても、それは採用できず、別の中継点や迂回路を選ぶ必要があります。

次の条件を表現できます。

- 指定した2頂点の同時使用禁止。
- 指定グループから高々1頂点だけを使用する条件。
- 呼出しごとの使用禁止頂点・使用禁止辺。
- 今回の部分問題の外側ですでに使っている頂点との排他条件。

排他判定の対象は、必須接続地点と中継点を含む、最終的に使用する頂点です。既存解の改善、非連結・制約違反を含む候補の修復、撤去できない既設辺への追加接続にも対応します。

**用意するものと結果：** 非負の辺費用を持つ無向グラフ、必須頂点、排他・禁止条件を渡すと、条件を満たす接続辺集合が得られます。

**使い分け：** ここでの「制約付き」は、主として頂点の排他・禁止を意味します。任意の容量制約や、「候補集合から必ず1点を選んで接続する」条件を一般的に扱うsolverではありません。外側の使用頂点を指定しても、それらへ自動的に接続する意味にはならず、接続必須点や既設辺は別に与えます。

#### サンプル：同時に使えない中継点を避けてネットワークを作る

通常のシュタイナー木に、使用頂点の排他条件を加えます。安い経路を集めると両方の排他頂点が必要になる場合、片方を使わない別経路へ組み替えます。

**入力形式**

1行目は頂点数 `N`、無向辺数 `M`、必須点数 `K`、排他ペア数 `C`。続く `M` 行は辺の `端点 端点 費用`、次の行は必須点ID、最後の `C` 行は同時使用できない頂点の組です。

**実行コード：`example17.cpp`**

```cpp
// 目的：同時に使えない中継点の組を避けながら、必須地点を安く接続します。
// 選ばれるネットワーク全体に対して、頂点の排他条件を課します。

#include "constrained_steiner_tree_v10.hpp"
using namespace std;

int main() {
    // 頂点数、辺数、必須地点数、同時使用禁止の頂点ペア数を読み込みます。IDは0から始まります。
    int n, m, terminal_count, conflict_count;
    cin >> n >> m >> terminal_count >> conflict_count;
    constrained_steiner_tree solver(n);
    // 無向辺の両端と費用を登録します。辺IDは入力順です。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        cin >> u >> v >> cost;
        solver.add_edge(u, v, cost);
    }
    // この配列に入れた地点は全てつなぎます。それ以外の頂点の使用は任意です。
    vector<int> terminals(terminal_count);
    for (int& terminal : terminals) {
        cin >> terminal;
    }

    // 排他条件は辺ではなく頂点に設定します。
    // 例えばuとvの間に辺がなくても、ネットワークに両方を含めることを禁止できます。
    constrained_steiner_tree::selection_rules rules(n);
    for (int i = 0; i < conflict_count; ++i) {
        int u, v;
        cin >> u >> v;
        // uだけ、vだけ、両方とも使わない選択は可能ですが、両方を使う選択はできません。
        rules.add_conflict(u, v);
    }
    // contextがrulesを参照するので、solveが終わるまでrulesを保持します。
    constrained_steiner_tree::query_context context;
    context.rules = &rules;
    constrained_steiner_tree::options options;
    options.seed = 1;
    // 必須地点と今回の排他条件を一緒に渡し、seedを固定して探索します。
    auto result = solver.solve(terminals, options, context);
    // 条件を満たす接続を取得できなかった場合は、辺集合を利用せず終了します。
    if (!result.ok()) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // 採用辺をID順に表示します。costは各採用辺を1回ずつ数えた費用合計です。
    // 各辺の両端を調べれば、同時使用禁止の頂点が両方含まれていないことを確認できます。
    sort(result.edges.begin(), result.edges.end());
    cout << "cost " << result.cost << '\n';
    cout << "edges";
    for (int edge : result.edges) {
        cout << ' ' << edge;
    }
    cout << '\n';
}
```

**入力例**

```text
5 6 3 1
0 3 1
3 1 1
1 4 1
4 2 1
3 2 4
0 4 4
0 1 2
3 4
```

**出力例**

```text
cost 6
edges 2 3 5
```

**結果の読み方**

中継点3と4の両方を使う費用4の木は違反です。表示された辺集合は、必須点0・1・2をつなぎ、使用頂点に3と4が同時に現れないものです。排他は辺単位ではなく、最終ネットワークで使う頂点に対して働きます。

### 18. 複数エージェント経路計画（`multi_agent_path_finding`）

ファイル：`multi_agent_path_finding_v06.hpp`

**複数ロボットが、いつ、どの頂点にいるかを決め、衝突を避けて移動させます。** 同じ場所を使う場合でも、通過時刻をずらせば共有できます。

各ロボットは1頂点を占有し、1ターンに隣の頂点へ移動するか、その場で待機します。同時刻に同じ頂点へ入ることと、同じ辺を逆向きに同時通過してすれ違うことを禁止します。

例えば、狭い通路でロボットAとBが互いの行き先へ向かう場合、Aを待機させ、Bが通過してからAを進める計画を作ります。

このライブラリには二つの主な用途があります。

- **MAPF：各ロボットの目的地へ移動する。** 始点と目的地を与え、各ロボットの到着までの時間の合計、または全員が到着するまでの時間を小さくする。
- **MAPD：依頼を割り当て、繰り返し搬送する。** 集荷地点・配送地点・依頼が利用可能になる時刻を与える。1台は同時に1件を搬送し、指定した時間内の完了件数を増やす。候補比較では、その後に依頼から完了までの時間合計、移動数を評価する。

MAPFでは、他のロボットや外部の予約を残し、指定ロボットだけ、または指定時間帯だけを再計画する機能もあります。

**用意するものと結果：** 無向・重みなしの移動グラフと、始点・目的地または搬送依頼を渡すと、各ロボットの時刻別の位置列が得られます。

**使い分け：** 衝突が時刻に依存することが決め手です。異なる速度、複数マスを占有するロボット、任意の荷物容量は標準モデルには含みません。利用時には、このヘッダに加えてAtCoder Libraryの `atcoder/mincostflow` が必要です。

#### サンプル：待避場所を使って、2台のロボットをすれ違わせる

ロボットごとの時刻別の位置を求めます。細い通路では単独の最短経路同士が衝突するため、一方が横の待避場所へ移動するなどして通過時刻を調整します。

**入力形式**

1行目は頂点数 `N`、無向辺数 `M`、ロボット数 `A`。続く `M` 行は辺の両端、続く `A` 行はロボットの `始点 目的地` です。各辺の移動時間は1です。

**実行コード：`example18.cpp`**

```cpp
// 目的：複数のロボットをそれぞれの目的地へ運び、同じ頂点での衝突と辺上のすれ違いを避けます。
// 経路の形だけでなく、各ロボットが各時刻にどこにいるかまで決めます。

#include "multi_agent_path_finding_v06.hpp"
using namespace std;

int main() {
    // 頂点数n、無向辺数m、ロボット数を読み込みます。頂点IDは0〜n-1です。
    int n, m, agent_count;
    cin >> n >> m >> agent_count;
    vector<pair<int, int>> edges(m);
    for (auto& [u, v] : edges) {
        cin >> u >> v;
    }
    // ロボットID順に出発点と目的地を登録します。出力の列もこの順になります。
    vector<int> starts(agent_count), goals(agent_count);
    for (int i = 0; i < agent_count; ++i) {
        cin >> starts[i] >> goals[i];
    }

    // 各辺の移動には1時刻かかり、その場で1時刻待つこともできます。
    mapf_graph graph(n, edges);
    // 既定の評価は、各ロボットが到着するまでの時間の合計です。
    // 時計による打切りを無効にし、再開始回数と改善回数を設定します。
    mapf_options options;
    options.time_limit_ms = -1;
    // max_timeは経路上の時刻の上限です。プログラムの実行時間をミリ秒で指定する値ではありません。
    options.max_time = 16;
    options.prioritized_restarts = 8;
    options.lns_iterations = 32;
    options.seed = 1;
    auto result = solve_mapf(graph, starts, goals, options);
    // 全ロボットが衝突せず到着できる経路を得られたか確認します。
    if (!result.success) {
        cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    // makespanは最後のロボットが到着する時刻です。到着時間の合計とは別の集計値です。
    cout << "makespan " << result.makespan << '\n';
    // 時刻0は出発時の位置です。各行の位置を比べると、待機や迂回の様子が分かります。
    for (int time = 0; time <= result.makespan; ++time) {
        cout << "time " << time;
        for (const auto& path : result.paths) {
            // 短い経路のロボットは、到着後も最後の頂点で待機します。
            int index = min(time, static_cast<int>(path.size()) - 1);
            cout << ' ' << path[index];
        }
        cout << '\n';
    }
}
```

**入力例**

```text
4 3 2
0 1
1 2
1 3
0 2
2 0
```

**出力例**

```text
makespan 4
time 0 0 2
time 1 1 2
time 2 3 1
time 3 1 0
time 4 2 0
```

**結果の読み方**

各 `time` 行は `時刻 ロボット0の位置 ロボット1の位置` です。同じ時刻の位置が重ならず、隣り合う時刻間で位置を交換する正面衝突もありません。頂点3が待避場所です。目的地へ到着した後も、その場所を占有し続けます。

### 19. 箱制約付き非線形最適化（`box_optimizer`）

ファイル：`box_optimizer_v06.hpp`

**各変数を決められた上下限の中で動かし、利用者が定義する評価値を小さくします。** 「箱制約」は、例えば座標は0から100、出力は0から1、という変数ごとの範囲のことです。

例えば、設備の組合せはすでに決まっており、各設備の出力だけを調整したいとします。出力の組合せを渡すと消費電力や不足量から総費用を計算できるなら、その関数を使ってよい出力設定を探せます。

主に次のような用途があります。

- 位置・角度・長さ・出力などの実数パラメータの調整。
- 離散的な構造を別solverで決めた後の、数値部分の仕上げ。
- 勾配を計算できる滑らかな関数の最小化。
- 勾配を用意しにくい関数や、丸めなどを含む関数の探索。
- 一部の変数を上下限の一致で固定し、残りだけを調整する。

**用意するものと結果：** 各変数の有限な上下限、初期値、値を小さくしたい評価関数を渡すと、見つかった最良の変数列と評価値が得られます。勾配を返せる場合はそれも利用できます。

勾配は、各変数を少し変えたときの評価値の変化率です。これを計算できれば、改善する方向を選ぶための情報として使えます。

**使い分け：** 任意の評価関数を渡せる一方、直接扱う必須条件は変数ごとの上下限です。「変数の合計を100にする」「物体を重ねない」といった連動制約は、変数の表現を工夫するか、評価関数へ減点として入れる必要があります。減点化だけで条件を必ず満たす保証はありません。同じ入力で評価値が変わらない決定的な関数を前提とします。

#### サンプル：2台の装置の出力を、目標量と消費電力の兼ね合いで調整する

実数の出力x・yを上下限の中で調整します。評価値は `(x + y - 目標量)² + 0.1 × (x² + 2y²)`。目標との差を減らしつつ、消費電力の大きい装置yの使用を抑えます。合計量を厳密に一致させる条件ではなく、差への減点です。

**入力形式**

1行目は目標量。続く2行は、装置x、装置yの `下限 上限` です。

**実行コード：`example19.cpp`**

```cpp
// 目的：上下限のある2つの出力量x、yを調整し、目標からのずれと運転負担を小さくします。
// この例では滑らかな評価関数と勾配を与え、実数の値を細かく改善します。

#include "box_optimizer_v06.hpp"
using namespace std;

int main() {
    // 目標となる合計出力と、x、yそれぞれの下限・上限を読み込みます。
    double target;
    cin >> target;
    BoxProblem problem;
    // bounds[0]がx、bounds[1]がyの範囲です。上下限は必ず守る条件として扱います。
    problem.bounds.resize(2);
    for (auto& bound : problem.bounds) {
        cin >> bound.lower >> bound.upper;
    }

    // 評価値と、その変化率である勾配を同時に返す関数です。
    auto objective = [target](span<const double> value, span<double> gradient) {
        double x = value[0];
        double y = value[1];
        // 合計出力を目標に近づけます。ただし一致を必須にせず、ずれを費用で評価します。
        double error = x + y - target;

        // 空のspanなら勾配は不要です。値だけを計算する呼出しにも対応します。
        if (!gradient.empty()) {
            // 評価関数をx、yでそれぞれ偏微分した値です。
            // 下のreturnで評価式を変えた場合は、勾配の式も対応させて変更します。
            gradient[0] = 2 * error + 0.2 * x;
            gradient[1] = 2 * error + 0.4 * y;
        }
        // 前半は目標からのずれ、後半は運転負担です。yはxより重い負担として評価します。
        // 両者の折り合いを付けるため、最良点でも合計出力が目標に完全一致するとは限りません。
        return error * error + 0.1 * (x * x + 2 * y * y);
    };

    BoxParam options;
    // Refineを指定し、初期点の周辺から勾配を使って改善する探索を行います。
    options.search = BoxParam::Search::Refine;
    BoxOptimizer solver(problem, options);
    BoxBudget budget;
    // 時計による打切りは使わず、評価回数を最大1000回に制限します。
    budget.time_limit_us = -1;
    budget.max_evaluations = 1000;
    // 初期値を省略すると、各上下限の中点から始めます。
    auto result = solver.solve(objective, budget);
    // 評価済みの点を取得できたことを確認してから、座標と費用を参照します。
    if (!result.has_value()) {
        cout << "NO_EVALUATED_POINT\n";
        return 0;
    }

    // xとyは調整後の出力量、costは評価関数の値です。小数点以下3桁で表示します。
    cout << fixed << setprecision(3);
    cout << "x " << result.x[0] << " y " << result.x[1] << '\n';
    cout << "cost " << result.cost << '\n';
}
```

**入力例**

```text
6
0 4
0 4
```

**出力例**

```text
x 3.750 y 1.875
cost 2.250
```

**結果の読み方**

`x` と `y` が装置の出力、`cost` が最小化する評価値です。出力3.75と1.875は合計5.625で、目標6より少し小さくなります。これは不足の減点と電力費用を合わせた結果で、合計量の必須制約違反ではありません。

### 20. 線形式の推定（`linear_estimator`）

ファイル：`linear_estimator_v09.hpp`

**未知の値の合計・差・重み付き合計を観測し、そこから元の値を推定します。** 関係式の係数は分かっており、式に入る未知の値を求める設定です。

例えば、道A・B・Cの所要時間が分からず、「AとBを通った合計は約20」「BとCを通った合計は約25」といった観測だけが得られる場合です。観測には誤差があり、さらに「各道の時間は1から30」「近隣の道は似た長さだろう」という事前情報も使いたい、とします。

このような観測や事前情報を、**一次式と、その値をどう評価するかの組**で登録します。

- 通常の測定誤差を想定する、二乗誤差に対応した観測。
- 大きな誤差を含む観測に対応するLaplace・Student t型の評価。
- 「観測値は20以上25未満だった」など、区間しか分からない観測。
- 真の合計値・差が一定範囲に入るという必須条件や、範囲を外れた分への減点。
- 個々の値が目安に近い、二つの値の差が小さい、といった事前情報。

未知変数は実数だけでなく、整数、有限個の候補値、既知の固定値を混在させられます。観測の追加・変更にも対応し、もっともらしい一つの候補に加え、サンプル、平均・分散、別の一次式の予測も得られます。

**用意するものと結果：** 変数の値域と、観測・事前情報を表す一次式と評価を渡します。結果は未知変数の推定値や、その分布を調べるためのサンプルです。

**使い分け：** 加算構造が明確な観測に向きます。未知変数同士の積などを含む一般の非線形モデルを、そのまま一次式として登録することはできません。また、自由な実数同士の厳密な等式は、あらかじめ変数を減らすなどして処理する前提です。サンプルから求める予測の分散には、将来の観測ノイズ自体は含まれません。

#### サンプル：経路の合計時間から、各道路の時間を推定する

道路を1本ずつ測れなくても、通った道路の時間の和が観測できれば、複数の観測を組み合わせて各道路の値を推定できます。ここでは各観測の誤差を正規分布で表します。

**入力形式**

1行目は未知変数数 `N`、観測数 `M`。続く `N` 行は値の上下限。続く `M` 行は `観測値 ノイズ標準偏差 項数` に続けて `変数ID 係数` の組を項数分並べます。標準偏差は正とします。

**実行コード：`example20.cpp`**

```cpp
// 目的：合計や差の観測から、直接は観測できない各成分の値を推定します。
// 観測ごとに「どの変数を何倍して足したか」と、誤差の大きさを指定します。

#include "linear_estimator_v09.hpp"
using namespace std;

int main() {
    // 未知変数の数nと観測数を読み込みます。未知変数IDは0〜n-1です。
    int n, observation_count;
    cin >> n >> observation_count;
    using Estimator = LinearEstimator<>;
    // 各変数を実数として扱い、取り得る下限と上限を設定します。
    vector<Estimator::Domain> domains;
    for (int i = 0; i < n; ++i) {
        double lower, upper;
        cin >> lower >> upper;
        domains.push_back(Estimator::Domain::real(lower, upper));
    }
    // 定義した変数の範囲と、再現用の乱数seed=1を渡します。
    Estimator estimator(domains, 1);

    for (int i = 0; i < observation_count; ++i) {
        double observed, sigma;
        int count;
        cin >> observed >> sigma >> count;
        // formは観測に対応する一次式です。係数は既知で、推定するのは変数の値です。
        Estimator::LinearForm form;
        for (int j = 0; j < count; ++j) {
            int variable;
            double coefficient;
            cin >> variable >> coefficient;
            // 例えば{0, 1}と{1, 1}ならx[0]+x[1]、{0, 1}と{1, -1}なら差を表します。
            form.terms.push_back({variable, coefficient});
        }
        // この一次式の値が、観測値に標準偏差sigmaの誤差で近いと考えます。
        // sigmaは正の値を指定します。小さいsigmaの観測ほど、同じずれを大きな損失と評価します。
        estimator.add_factor(form, Estimator::Loss::gaussian(observed, sigma));
    }

    // 全観測の損失を合わせて小さくする値を探索します。
    // 時間制限を無効にし、最大5000ステップに制限します。
    Estimator::OptimizeParam options;
    options.time_limit_us = -1;
    options.max_steps = 5000;
    estimator.optimize(options);
    // 指定範囲内の推定値が得られたことを確認します。
    if (!estimator.has_solution()) {
        cout << "NO_FEASIBLE_ESTIMATE\n";
        return 0;
    }

    cout << fixed << setprecision(3);
    // 未知変数ID順に、得られた最良の推定値を表示します。
    // 観測に誤差があれば、全ての一次式が観測値に完全一致する必要はありません。
    cout << "estimate";
    for (double value : estimator.best()) {
        cout << ' ' << value;
    }
    cout << '\n';
}
```

**入力例**

```text
3 3
0 20
0 20
0 20
7 0.5 2 0 1 1 1
9 0.5 2 1 1 2 1
8 0.5 2 0 1 2 1
```

**出力例**

```text
estimate 3.000 4.000 5.000
```

**結果の読み方**

`estimate` は未知変数の推定値です。この例では道路0+1が7、道路1+2が9、道路0+2が8なので、3・4・5が全観測を説明します。値が厳密に整合しない観測へ変えても、標準偏差を考慮して折り合う値を探せます。

### 21. 汎用推定（`mcmc_estimator`）

ファイル：`mcmc_estimator_v10.hpp`

**「この未知状態なら観測をどのくらい説明できるか」を関数で定義し、もっともらしい状態を探します。** 観測と予測の関係を、線形式に限定せずに書けます。

例えば、設備の隠れた能力、故障しやすさ、種類を推定したいとします。能力は実数、残り寿命は整数、設備の種類は数種類のカテゴリです。これらから作業結果を予測する関数と、実際の結果とのずれを評価する関数を用意して推定します。

主な表現範囲は次のとおりです。

- 実数・整数・カテゴリが混在する未知パラメータ。
- 一般の非線形な予測関数と、事前情報を含む評価。
- 数値観測、外れ値を考慮した観測、成功・失敗の観測、回数の観測。標準の損失関数を組み合わせられる。
- 利用者が独自に定義する状態と、その状態を変更する候補生成処理。
- 観測が増えた際の推定更新。

二つの使い方を区別すると分かりやすくなります。

| 使い方 | 得られるもの |
|---|---|
| 良い候補を探す | 評価値の小さい一つの状態。モデルを負の対数密度として定義すれば、もっともらしい状態に対応する |
| 分布からサンプルを得る | あり得る状態の例と、そこから集計した予測の平均・分散 |

**用意するものと結果：** 数値パラメータなら値域・初期値と評価モデルを渡します。独自状態なら、その状態をどう変更するかも指定します。評価モデルは、状態が与えられたときに同じ値を返すものにします。

**使い分け：** 複雑な観測モデルや、混合した未知変数の不確かさを扱う用途に向きます。分布の意味を持たせるには、観測誤差・事前情報を適切にモデル化する必要があります。最良候補を探す途中の状態を並べただけでは、分布に従うサンプルにはならないため、サンプル取得用の機能を使います。

#### サンプル：成功・失敗の観測から、未知の成功確率と不確かさを推定する

独立な試行の成功回数から、未知の成功確率pを推定します。pには0から1の一様な事前分布を置き、観測を説明しやすい値を探索した後、サンプル用の入口で分布の平均と標準偏差を求めます。

**入力形式**

入力は `試行回数 成功回数` の1行です。この例では、成功回数が0より大きく試行回数より小さい入力を使います。

**実行コード：`example21.cpp`**

```cpp
// 目的：試行回数と成功回数から、成功確率pとその不確かさを推定します。
// もっともらしい1点を探す処理と、あり得る値の広がりを調べる処理を使い分けます。

#include "mcmc_estimator_v10.hpp"
using namespace std;

int main() {
    // 入力は試行回数と成功回数です。試行ごとに同じ確率pで独立に成功すると仮定します。
    int trials, successes;
    cin >> trials >> successes;
    using Estimator = McmcEstimator<>;
    Estimator::Parameters parameters;
    // 引数は初期値、下限、上限、候補を動かす幅です。
    // 最後の0.1は探索の提案幅であり、観測誤差の標準偏差を指定するものではありません。
    parameters.real(0.5, 0.0, 1.0, 0.1);

    // 小さいほど観測を説明しやすい「負の対数尤度」を返します。
    // 一様事前分布は範囲内で一定なので、加算する項はありません。
    auto model = [trials, successes](const McmcState& state) {
        double p = state.real[0];
        // この例では端点を評価対象から外し、log(0)を計算しないようにします。
        if (p <= 0 || p >= 1) {
            return numeric_limits<double>::infinity();
        }
        // 成功の確率pと失敗の確率1-pから、観測の起こりやすさを計算します。
        // pに依存しない二項係数は、比較や標本の分布に影響しないので省略します。
        return -successes * log(p) - (trials - successes) * log1p(-p);
    };

    Estimator::Param options;
    options.seed = 1;
    // 標本は遷移5回ごとに記録します。
    // 保存間隔を空けても、標本間の独立性が保証されるわけではありません。
    options.sample_stride = 5;
    // 本集計前に2000ステップの準備を行います。これは収束を保証する回数ではありません。
    options.warmup_steps = 2000;
    auto session = Estimator::make_numeric_session(parameters, model, options);
    // solveでは評価値の小さい点を探索します。ここで得るbestは、分布の平均ではありません。
    auto best = session.solve(Estimator::Budget::for_steps(5000));
    if (!best.state) {
        cout << "NO_ESTIMATE\n";
        return 0;
    }

    // 最適化の途中経過ではなく、sampleで得た標本から不確かさを集計します。
    // sampleでは確率分布から標本を集めます。ラムダ式で集計対象のpを取り出します。
    auto sampled = session.sample(
        Estimator::Budget::for_steps(50000),
        [](const McmcState& state) { return state.real[0]; });
    auto mean = sampled.summary.mean();
    auto variance = sampled.summary.variance();
    // 平均には1個以上、分散には2個以上の標本が必要なので、集計値の存在を確認します。
    if (!mean || !variance) {
        cout << "NOT_ENOUGH_SAMPLES\n";
        return 0;
    }

    cout << fixed << setprecision(3);
    // best_pは最適化で得た値、mean_pは標本平均です。
    // sd_pはpの分布の広がりを表す標準偏差で、平均値の計算誤差を表す標準誤差とは異なります。
    cout << "best_p " << best.state->real[0] << '\n';
    cout << "mean_p " << *mean << '\n';
    cout << "sd_p " << sqrt(*variance) << '\n';
}
```

**入力例**

```text
10 7
```

**出力例**

```text
best_p 0.700
mean_p 0.666
sd_p 0.131
```

**結果の読み方**

`best_p` はもっともらしい一点、`mean_p` は分布の平均、`sd_p` はpの不確かさを表す標準偏差です。このモデルの最頻値は0.7、分布の平均は約0.667です。MCMCの出力はサンプルによる近似なので、理論値と完全には一致しません。`sd_p` は平均値の計算誤差ではありません。

### 22. 逐次推定（`particle_filter`）

ファイル：`particle_filter_v12.hpp`

**あり得る未知状態を多数の候補として持ち、観測が届くたびに候補の重みを更新します。** 一つ一つの候補を「粒子」と呼びます。

例えば、見えないロボットの位置を推定します。最初は複数の位置があり得ます。ロボットが移動したら各候補も移動させ、新しいセンサー値が届いたら、その値と整合する候補の重みを高くします。これを繰り返し、現在どこにいそうかを追います。

この実装は、次の両方に使えます。

- **時間とともに変化する状態：** 位置、在庫、状態モードなど。状態遷移と観測モデルを定義する。
- **変化しない未知パラメータ：** 長さ、能力、未知の配置など。観測を蓄積し、同じパラメータへの推定を更新する。

状態は実数の配列に限らず、利用者が定義した構造体でも構いません。複数の有力候補を残したまま、重み付き平均や任意の予測値を求められます。例えば「候補ごとに配送時間を計算してから平均する」といった集計もできます。

重みが少数の粒子に集中した際の粒子の選び直しや、固定パラメータの候補を観測履歴に基づいて追加探索する機能があります。低水準の `ParticleCloud` を使えば、粒子操作を自分の手順に組み込めます。

**用意するものと結果：** 初期状態の生成方法、その状態なら観測がどの程度起こりやすいか、必要に応じて状態遷移を用意します。結果は粒子群、各粒子の重み、そこから計算する推定値です。

**使い分け：** 観測が順番に届く問題、とくに状態が変化する問題に自然です。固定パラメータの推定では20・21と用途が重なります。粒子の重み付き平均が実現可能な状態になるとは限らないため、離散的な判断や非線形な予測では、粒子ごとに必要な値を計算して集計する使い方が役立ちます。

#### サンプル：移動命令とセンサー値から、見えないロボットの位置を追う

初期位置を平均0・標準偏差1の正規分布で考えます。毎ターン、移動命令に標準偏差0.2の誤差が加わり、位置センサーにも標準偏差0.5の誤差があるモデルです。観測のたびに現在位置の候補を更新します。

**入力形式**

1行目はターン数 `T`。続く `T` 行は `移動命令 センサーの観測位置` です。各行は移動してから観測する順序を表します。

**実行コード：`example22.cpp`**

```cpp
// 目的：移動指示とセンサー観測を順に受け取り、見えない現在位置を推定します。
// 1つの位置に決め打ちせず、多数の位置候補（粒子）とその重みで状態を表します。

#include "particle_filter_v12.hpp"
using namespace std;

struct PositionModel {
    // 1粒子が1つの位置候補です。この例では1次元の位置を実数で表します。
    using Particle = double;
    // movementは指示した移動量、measured_positionは移動後のセンサー値です。
    struct Observation {
        double movement;
        double measured_position;
    };

    // 最初の位置候補を生成します。
    double sample(mt19937_64& rng) const {
        // 初期位置は平均0、標準偏差1の正規分布に従うと仮定します。
        return normal_distribution<double>(0.0, 1.0)(rng);
    }

    // 観測値を見て位置を直接書き換えず、移動モデルだけを適用します。
    void transition(double& position, const Observation& observation,
                    mt19937_64& rng) const {
        // 指示通りには動かない誤差を、標準偏差0.2の正規分布で表します。
        double error = normal_distribution<double>(0.0, 0.2)(rng);
        position += observation.movement + error;
    }

    // センサー値に近い候補ほど、観測後の重みが大きくなります。
    double log_likelihood(double position, const Observation& observation) const {
        // センサー誤差の標準偏差は0.5と仮定し、ずれをこの大きさで割って評価します。
        double error = (observation.measured_position - position) / 0.5;
        // 粒子に共通する定数項は、重みの比較に影響しないので省きます。
        return -0.5 * error * error;
    }
};

int main() {
    // 観測回数を読み、その回数だけ「移動量、測定位置」の組を処理します。
    int turns;
    cin >> turns;
    ParticleFilter<PositionModel>::Param options;
    // 4096個の粒子で位置の分布を近似します。粒子を増やすと計算量も増えます。
    options.count = 4096;
    // モデル、粒子数、seed=1の乱数生成器を渡して、初期粒子を用意します。
    ParticleFilter<PositionModel> filter(PositionModel{}, options, mt19937_64(1));

    cout << fixed << setprecision(3);
    for (int turn = 1; turn <= turns; ++turn) {
        PositionModel::Observation observation;
        cin >> observation.movement >> observation.measured_position;
        // observeが、必要な再サンプリング・状態遷移・重み更新を行います。
        auto updated = filter.observe(observation);
        // 更新に成功した場合だけ、観測を反映した推定値を読み出します。
        if (!updated.ok) {
            cout << "OBSERVATION_UPDATE_FAILED\n";
            return 0;
        }
        // estimateは粒子の重み付き平均です。測定位置そのものではなく、移動予測も反映しています。
        cout << "turn " << turn
             << " observation " << observation.measured_position
             << " estimate " << filter.estimate().mean << '\n';
    }
}
```

**入力例**

```text
3
1 1.2
1 1.8
1 3.1
```

**出力例**

```text
turn 1 observation 1.200 estimate 1.157
turn 2 observation 1.800 estimate 1.981
turn 3 observation 3.100 estimate 3.026
```

**結果の読み方**

各行の `estimate` が観測後の推定位置です。センサー値をそのまま位置にするのではなく、前の推定位置と移動命令も考慮します。粒子を使う近似なので、小数値は実行環境によって少し変わることがあります。

### 23. 有限手数のモンテカルロ計画（`monte_carlo_planner`）

ファイル：`monte_carlo_planner_v08.hpp`

**候補の行動をシミュレーションし、残り手数で得られる期待得点が大きい行動を選びます。** 行動の結果が確率で変わったり、現在の本当の状態が分からなかったりする状況を扱います。

例えば、品質の分からない鉱山について、「すぐ採掘する」「1手使って調査する」「別の鉱山へ移動する」を選びます。調査の良し悪しは、調査結果を見た後で採掘するかどうかを変えるところまで含めて評価します。

表現できる主な内容は次のとおりです。

- 完全に見えている状態と、観測から一部しか分からない状態。
- 行動後に確率的に変わる状態と、その際に得る報酬。
- 残り手数、途中終了、最後にまとめて加わる得点。
- 先の報酬を小さく数える割引。
- 読みを途中で打ち切り、それ以降の価値を利用者の関数で見積もる設定。
- 全候補の列挙と、候補を順次生成する方式。

現在の候補だけを比較し、その後を簡単な行動規則で進める使い方と、先の判断も木として育てる探索を選べます。後者では、シミュレーション中に得た状態や観測に応じて、その先の行動を変えられます。同じ乱数条件で候補を比べるCRNの切替もあります。

**用意するものと結果：** 状態、行動候補、状態遷移、報酬、必要なら先の行動規則や打切り評価を定義します。部分観測では、現在の情報からあり得る真の状態を生成する方法も必要です。結果は推薦する次の行動と、候補ごとの試行数・平均得点です。

**使い分け：** 推定した値を使って次の行動を選ぶ役割です。推定分布は利用者側で用意するため、22などと組み合わせられます。部分観測で行動を選ぶ処理には、実際に知り得る情報だけを渡すようにモデルを作ります。実ターンを進めた際の部分木の再利用は、整合する完全観測の場合に限られ、部分観測では新しい根から探索し直します。

#### サンプル：すぐ採掘するか、1手使って調査してから選ぶか

2か所の鉱山の片方だけに当たりがあり、採掘は1回で終了します。残り2手のうち1手を調査に使うと、費用を払う代わりに当たりの鉱山が正確に分かります。今すぐの報酬だけでなく、調査結果に応じて次の行動を変える価値を評価します。

**入力形式**

入力は `鉱山1が当たりである事前確率 当たりの報酬 調査費用 実際の調査結果`。最後の値は0または1で、調査を選んだ場合だけ観測として渡します。初回の計画には渡しません。

**実行コード：`example23.cpp`**

```cpp
// 目的：どちらの鉱山が当たりか不明な状況で、調査費を払う価値があるか判断します。
// 残り2手の間に「直接採掘する」か「調査してから採掘する」かを比較します。

#include "monte_carlo_planner_v08.hpp"
using namespace std;

struct MiningModel {
    // シミュレーション内の真の状態。行動選択には直接渡しません。
    struct State {
        int rich_mine;
    };
    // 実際に知り得る情報だけを持ちます。-1は未調査です。
    struct Info {
        int observed_mine = -1;
        bool done = false;
        bool operator==(const Info&) const = default;
    };
    using Action = int;  // 0:鉱山0、1:鉱山1、2:調査

    // 鉱山1が当たりである事前確率、当たったときの報酬、調査費用を持ちます。
    double probability_one;
    double reward;
    double survey_cost;

    // 現在の情報に合う真の状態を、試行ごとに生成します。
    State sample_root(const Info& info, MonteCarloNoise& rng) const {
        if (info.observed_mine >= 0) {
            return {info.observed_mine};
        }
        return {rng.uniform01() < probability_one ? 1 : 0};
    }

    // plannerが合法手を尋ねるたびに、現在の情報と残り手数から候補を列挙します。
    // 未観測の真の状態Stateは受け取らず、先を知った行動選択を防ぎます。
    void actions(const Info& info, int remaining, vector<int>& out) const {
        out = {0, 1};
        // 調査後の採掘に1手必要です。既に知っている場合も再調査しません。
        if (info.observed_mine < 0 && remaining >= 2) {
            out.push_back(2);
        }
    }

    // 仮想の1手を進め、情報を更新し、その手で得る報酬を返します。
    // 探索中に呼ばれるため、外部の実環境への操作はここには書きません。
    double step(State& state, Info& info, int action, MonteCarloNoise&) const {
        if (action == 2) {
            // 調査した場合だけ当たりの鉱山が分かります。この手では採掘せず、費用だけを払います。
            info.observed_mine = state.rich_mine;
            return -survey_cost;
        }
        // 一度採掘したら終了です。当たりなら報酬を得て、外れなら0になります。
        info.done = true;
        return action == state.rich_mine ? reward : 0.0;
    }

    // 残り手数があっても、採掘済みなら以後のシミュレーションを終了します。
    bool terminal(const Info& info) const {
        return info.done;
    }

    // 木の外を試す際にも、隠れた正解を読まず、観測だけで行動を決めます。
    int rollout_action(const Info& info, int, MonteCarloNoise&) const {
        if (info.observed_mine >= 0) {
            return info.observed_mine;
        }
        return probability_one >= 0.5 ? 1 : 0;
    }
};

int main() {
    MiningModel model;
    // 実際の調査結果も入力しますが、調査を選ぶまではplannerへ渡しません。
    // 探索側には事前確率・報酬・費用だけを知らせます。
    int survey_result;
    cin >> model.probability_one >> model.reward >> model.survey_cost >> survey_result;

    using Planner = MonteCarloPlanner<MiningModel>;
    Planner::Options options;
    options.seed = 1;
    // 行動後に得る情報ごとの分岐を木で探索し、調査後に採掘先を変える判断を評価します。
    options.tree = true;
    // 候補行動を共通の乱数シナリオで比較し、偶然による評価差を抑えます。
    options.use_crn = true;
    Planner planner(model, options);
    Planner::Limits limits;
    // 1回のsearchで試すシミュレーション数を最大10000回にします。
    limits.max_simulations = 10000;

    MiningModel::Info info;
    // 未調査・未採掘の情報から、残り2手で計画します。各手の報酬を合わせて評価します。
    planner.start(info, 2);
    planner.search(limits);
    auto first = planner.best_action();
    // 選べる行動が得られた場合だけ参照します。戻り値は行動IDを包んだoptionalです。
    if (!first) {
        cout << "NO_ACTION\n";
        return 0;
    }
    // 行動IDを読みやすい名前へ変換します。surveyなら、調査費を払う価値があるという判断です。
    const array<string, 3> names = {"mine0", "mine1", "survey"};
    cout << "first " << names[*first] << '\n';

    if (*first == 2) {
        // ここで初めて実際の調査結果を渡し、残り1手を計画し直します。
        info.observed_mine = survey_result;
        // 調査結果を反映した情報で計画を始め直します。今度は採掘に使える1手だけが残っています。
        planner.start(info, 1);
        planner.search(limits);
        auto next = planner.best_action();
        if (next) {
            cout << "after_survey " << names[*next] << '\n';
        }
    }
}
```

**入力例**

```text
0.5 10 1 1
```

**出力例**

```text
first survey
after_survey mine1
```

**結果の読み方**

`mine0`・`mine1` は各鉱山の採掘、`survey` は調査です。事前確率0.5なら即採掘の期待報酬は5ですが、調査後に当たりを採掘する価値は10 - 1 = 9です。初手に調査を選び、観測が1なら残り1手では鉱山1を選びます。

## 3. 似た問題で迷ったときの選び方

### 「選ぶ」なら、利益がどこに付くかを見る

| 問題の特徴 | 最初に見るsolver（日本語名） | 英語名（ソース対応） |
|---|---|---|
| 選んだ候補がカバーした対象に得点が付き、重複は得にならない | 01 最大被覆・最小費用集合被覆 | `maximum_coverage` |
| 各候補に得点が付き、競合や共通資源の使用個数を守りたい | 02 重み付き集合パッキング・独立集合 | `set_packing` |
| 予算・最低個数・依存関係・違反減点などを組み合わせたい | 03 疎な0–1最適化 | `pbo` |
| 指定数の拠点を選び、各利用者への対応費用を減らしたい | 05 施設配置 | `facility_location` |

### 「分ける・割り当てる」なら、何が要素を結び付けるかを見る

| 問題の特徴 | 最初に見るsolver（日本語名） | 英語名（ソース対応） |
|---|---|---|
| 仕事ごとの費用と担当先の容量が中心 | 04 一般化割当 | `generalized_assignment` |
| 要素のペアの相性が中心 | 06 容量制約付きグラフラベリング | `capacitated_graph_labeling` |
| 同じ領域を地続きに保つ必要がある | 07 連結性付き領域分割 | `connected_partition` |
| 共有工具・共通データなど、要素の集合単位の費用や容量が中心 | 08 容量制約付きハイパーグラフ分割 | `hypergraph_partitioner` |
| 各位置へ一つずつ置き、物体間の関係と位置間距離が効く | 09 二次割当 | `qap` |

### 「移動する・つなぐ」なら、何を出力したいかを見る

| 欲しい結果 | 最初に見るsolver（日本語名） | 英語名（ソース対応） |
|---|---|---|
| 1本の訪問順序 | 13 巡回セールスマン問題・ハミルトン路 | `tsp` |
| 車両別の担当と訪問順序 | 14 複数経路・配送計画 | `routing` |
| 指定された始点・終点の組ごとの通過経路 | 15 容量制約付き複数経路 | `capacitated_router` |
| 必要な全地点をつなぐ共有ネットワーク | 16 シュタイナー木 | `heuristic_steiner_tree` |
| 同時使用できない頂点を避けてつなぐ共有ネットワーク | 17 排他制約付きシュタイナー木 | `constrained_steiner_tree` |
| 各ロボットの時刻別の位置列 | 18 複数エージェント経路計画 | `multi_agent_path_finding` |

### 「数値が分からない・調整したい」なら、観測との関係を見る

| 問題の特徴 | 最初に見るsolver（日本語名） | 英語名（ソース対応） |
|---|---|---|
| 与えられた評価関数を小さくする実数値を探したい | 19 箱制約付き非線形最適化 | `box_optimizer` |
| 未知の値の合計・差などから推定したい | 20 線形式の推定 | `linear_estimator` |
| 任意のモデルで未知状態を推定し、分布からのサンプルも欲しい | 21 汎用推定 | `mcmc_estimator` |
| 次々と届く観測から、状態候補と重みを更新したい | 22 逐次推定 | `particle_filter` |
| 不確かさを踏まえ、将来まで考えて次の行動を選びたい | 23 有限手数のモンテカルロ計画 | `monte_carlo_planner` |

## 4. 一つの問題を複数solverへ分ける例

問題全体が一つのsolverへ一致しなくても、**決めたいものの一部分を切り出す**と利用できます。

**配送拠点と配送計画を別々に決める。** 05で拠点候補を選び、その拠点を出発点として14で車両別の配送計画を作ります。ただし05には拠点容量や実際の配送巡回費用が入らないため、全体評価を見て拠点を再選択する処理は利用者側で組み立てます。

**配置候補を作ってから、両立する候補を選ぶ。** 利用者側で図形の置き方や作業セットを多数作り、02へ利益と競合を渡します。位置もsolverに決めてほしい長方形配置なら、10を直接使う選択があります。

**推定と行動計画を分ける。** 22で現在あり得る状態の分布を更新し、その分布から状態を取り出す処理を23へ渡します。23は各状態で先をシミュレーションし、期待得点で次の行動を選びます。二つをつなぐ状態変換やシミュレータは利用者側で用意します。

## 5. 利用前に共通して確認すること

- **よい解を探すことと、最適性を証明することは別です。** 最適化用の多くの入口は、限られた計算時間で良い候補を探す構成です。TSPの小規模厳密解など、一部の入口は別の性質を持ちます。
- **厳しい条件では、合法な解が見つかったかを確認します。** 実行可能解を見つけられなかったことは、解が存在しない証明とは限りません。成否や違反状況を返す型はライブラリごとに異なります。
- **推定値や期待得点は、渡したモデルと計算量に依存します。** 粒子数やサンプル数を増やしても、誤った観測モデルや状態遷移が自動的に修正されるわけではありません。
- **部分改善の範囲は個別に確認します。** 多くのライブラリは既存解の改善に対応しますが、固定できる対象や修復の入口は共通仕様ではありません。大きな探索の部品として使うときは、各ヘッダの公開APIで確認してください。
