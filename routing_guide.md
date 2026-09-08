# Routingライブラリ ユーザーガイド

対象ソースは `routing_solver_v05.hpp`。C++20の単一ヘッダで、名前空間は `routing` です。本書は、このヘッダの公開仕様と実装を説明します。第4・6・8章のR01〜R13は同じユースケースを指します。

## 1. 何を解くライブラリか？

### 1.1 複数の車両に仕事を割り当て、訪問順を決める

たとえば、倉庫にある荷物を複数の車両で顧客へ届けるとき、「どの車両が、どの顧客を、どの順番で訪ねるか」を求めます。車両ごとの積載量と経路コストの上限を守り、移動と作業にかかる総コストを小さくします。

すべての仕事を実施する問題に加え、限られた資源で報酬の大きい仕事を選ぶ問題、ちょうど指定件数だけ実施する問題を扱います。仕事は、1か所を訪ねれば完了する「単一訪問」と、集荷地点を訪ねてから同じ車両で配達地点へ運ぶ「集荷・配達対」の2種類です。

### 1.2 経路コストの定義

車両 $r$ の経路を、作業地点の列 $P_r=(v_{r,1},\ldots,v_{r,m_r})$ とします。空でない経路のコストは次の値です。

$$C_r=d(s_r,v_{r,1})+\sum_{j=1}^{m_r-1}d(v_{r,j},v_{r,j+1})+d(v_{r,m_r},t_r)+\sum_{j=1}^{m_r}h(v_{r,j})$$

全車両のコストと獲得報酬を次のように定義します。

$$C=\sum_{r=1}^{M}C_r,\qquad R=\sum_{i\in I}p_i x_i$$

| 記号 | 意味 |
|---|---|
| $I$、$i$ | 入力された仕事の集合と、その1件。集荷・配達では2地点をまとめて1仕事と数える |
| $M$、$r$ | 車両数と車両番号 |
| $P_r$、$m_r$ | 車両 $r$ の作業地点列と、その長さ。車両の出発・終了地点はこの列に含めない |
| $v_{r,j}$、$j$ | 車両 $r$ が $j$ 番目に作業する地点と、経路内の位置 |
| $s_r$、$t_r$ | 車両 $r$ の出発地点と終了地点。同じ地点でもよい |
| $d(u,v)$ | 地点 $u$ から地点 $v$ への移動コスト。距離、時間、燃料など同じ単位の数値 |
| $h(v)$ | 地点 $v$ での作業コスト。集荷・配達ではそれぞれの地点に設定する |
| $C_r$、$C$ | 車両ごとの移動・作業コストと、その合計 |
| $x_i$ | 仕事 $i$ を完了すれば1、実施しなければ0。集荷・配達対は両地点を条件どおり訪ねて初めて1 |
| $p_i$、$R$ | 仕事 $i$ の報酬と、完了した仕事の報酬合計 |

$m_r=0$ の空経路は $C_r=0$ と定義します。出発・終了地点が異なっていても、仕事を割り当てなかった車両の移動は数えません。閉じた経路は $s_r=t_r$、指定した別の場所で終える経路は $s_r\ne t_r$ です。後者も終了地点までの移動を数えます。

### 1.3 目的関数と実施条件

**全件実施**では、すべての仕事を実施したうえで総コストを最小化します。

$$\min C\quad\text{subject to }x_i=1\quad(i\in I)$$

**報酬付き選択**では、必須の仕事を実施し、まず獲得報酬を最大化します。報酬が同じ解の間だけで総コストを比較します。

$$\max_{\mathrm{lex}}(R,-C)\quad\text{subject to }x_i=1\quad(i\in I_{\mathrm{must}})$$

ここで $I_{\mathrm{must}}$ は必須仕事の集合、$\max_{\mathrm{lex}}$ は左の成分を優先する辞書式最大化です。報酬が1増える解は、コストが増えても、資源制約を守る限り目的関数上は優れます。報酬と移動コストの加重和を最大化する仕様ではありません。

**件数固定選択**では、必須仕事を含めてちょうど $K$ 件を実施し、総コストを最小化します。

$$\min C\quad\text{subject to }\sum_{i\in I}x_i=K,\quad x_i=1\quad(i\in I_{\mathrm{must}})$$

$K$ は呼び出し側が指定する実施件数です。「最大 $K$ 件」や「少なくとも $K$ 件」ではありません。この問題の選択に報酬は使いません。

### 1.4 容量・経路上限・集荷配達の制約

すべての問題で、車両ごとの経路上限 $B_r$ に対して $C_r\le B_r$ を守ります。

単一訪問で容量制約を使う問題では、車両の担当仕事集合を $I_r$、仕事 $i$ の需要を $q_i$、車両容量を $Q_r$ として、次を守ります。

$$\sum_{i\in I_r}q_i\le Q_r$$

純配送なら出発時にこの需要合計を積み、配達に伴って荷物が減る状況を表せます。Orienteeringだけは積載量を制約に使いません。

集荷・配達では、同じ車両が集荷を先に、配達を後に行います。仕事 $i$ の集荷地点を $a_i$、配達地点を $b_i$ とし、地点 $v$ を処理したときの荷物増減を $\Delta(v)$ とします。

$$\Delta(a_i)=q_i,\qquad\Delta(b_i)=-q_i,\qquad L_{r,j}=\sum_{k=1}^{j}\Delta(v_{r,k}),\qquad 0\le L_{r,j}\le Q_r$$

$L_{r,j}$ は車両 $r$ が $j$ 番目の作業を終えた直後の積載量、$k$ は経路内の位置を足し合わせる添字です。出発時と全仕事完了時の積載量は0です。各仕事を複数車両で分担したり、同じ仕事から報酬を複数回得たりはしません。

## 2. 厳密解アルゴリズム

このroutingヘッダは厳密最適解を保証する入口を持ちません。小さい部分問題や、制約を固定できる問題では、次のように別の厳密計算を使う選択肢があります。

| 条件 | 厳密に解く考え方 | 適用上の境界 |
|---|---|---|
| 車両が1台、訪問集合が確定、容量・経路上限を満たす | 全訪問順を列挙する、または部分集合と最後の地点を状態にするDP | 単一訪問ならTSP・固定端点の経路問題になる。集荷配達の順序制約は通常のTSP DPには含まれない |
| 車両ごとの担当集合を外側で固定できる | 各車両の訪問順をそれぞれ厳密に最適化する | 割当てを含む全体の最適性は保証しない。作業コストが順序によらない単一訪問に適する |
| 仕事・車両がごく少ない | 実施部分集合、車両割当て、各車両の順序を列挙し、制約を検査する | 対応する目的関数で最良の実行可能解を選べば厳密。ただし組合せ数が急増する |
| 任意訪問だが、1台・仕事が少ない | 部分集合ごとの最短経路コストをDPで計算し、容量・予算を満たす集合を比較する | 報酬最大化と件数固定で集合の比較規則が異なる。必須仕事を落とさない |
| 各車両が高々1仕事だけを担当できる設定 | 車両と仕事の実行可能な組合せを辺にした割当て問題を解く | 1仕事の単独経路の容量・上限を先に検査する。この条件を外すと一般のroutingになる |

別ヘッダ `tsp_solver_v07.hpp` の `tsp::held_karp_tsp` は、閉路と端点指定可能な開路を、**入力頂点数22以下**で厳密に解きます。たとえば1台で顧客をすべて訪ねて倉庫へ戻るなら、顧客に倉庫1頂点を加えて渡します。別の終了地点が必要なら両端を含めます。22の上限は顧客数ではなく、この追加後の頂点数です。

そのDPは時間・メモリを指数的に使い、時間制限による中断はありません。上限内でも実行予算に入るかを確かめて使います。集荷前に配達しないこと、途中積載量、複数車両への割当ては自動では検査しません。厳密な集荷配達を実装するなら、順序や積載量を満たす遷移に限定した列挙・DPが別途必要です。

「全順序を厳密に調べる余裕があるか」「担当集合を固定してよいか」が、routingの近似探索を使うかを決める観点になります。

## 3. 差分更新

### 3.1 再利用できるもの

`RoutingContext` は、登録頂点間の距離行列と候補集合のスナップショットです。元の距離関数を保持しません。`improve_*` は前の経路を初期解にできますが、呼び出し間で探索途中の状態を保存するAPIではありません。

| 次のターンで変える内容 | Context | 前の解の扱い |
|---|---|---|
| 報酬、必須指定、需要、作業コスト、指定件数 | 登録済み頂点だけなら再利用可能 | 新しい制約を満たすか確認する。報酬・コストは再評価される |
| 車両容量、経路上限、登録済み地点への出発・終了地点変更 | 再利用可能 | 容量・経路コストを再確認する。現在位置を出発地点にできる |
| 仕事の追加・削除 | 必要な地点がすべて登録済みなら再利用可能 | 削除した仕事を経路から除く。追加仕事は初期解から省いて構わない |
| 車両数・車両順序 | 登録済み地点なら再利用可能 | 経路配列を新しい車両数・順序に対応させる |
| 辺コストの変更、元グラフの変化で最短距離が変わる | **再構築が必要** | 新しいContextと新しい費用・制約で有効な初期経路を渡す |
| 未登録頂点の追加、対称性の変更 | **再構築が必要** | IDを新しいContextに登録してから渡す |

元の距離配列を書き換えても既存Contextの値は変わりません。辺1本を更新するAPIもありません。単純な `dist` 版 `solve_*` は毎回Contextを作るため、距離が同じ問題を何度も解くなら明示的にContextを持つ方が前処理を共有できます。

### 3.2 前の経路を渡すときの条件

`improve_*` の初期経路は、現在の仕事のIDだけを含む、重複のない経路でなければなりません。容量と経路上限を既に満たしている必要があります。全必須仕事がそろっている必要はなく、不足仕事の挿入は探索が試みます。

集荷配達では、残す依頼の両地点を同じ車両に残し、集荷を先にします。削除するときも対で削除します。件数固定問題では、初期経路に既に入っている仕事数を $K$ 以下にします。無効な経路を何でも修復する入口ではありません。

ターン制の純配送なら、完了した顧客を仕事・経路から外し、各車両の出発地点を現在位置へ変更して再実行できます。ただし、既に積んでいる個々の荷物を別車両へ自由に割り当て直してはいけない状況は、この更新だけでは表現できません。集荷済み・未配達の荷物を初期積載として引き継ぐ機能もありません。

### 3.3 再利用の計算量

登録頂点数を $V$、仕事数を $N$、車両数を $M$、初期経路に含まれる作業地点数を $L$ とします。距離を定数時間で返せる場合、Context構築は概ね $O(V^2)$ 時間・メモリです。

Context再利用時も、頂点IDの二分探索による変換と状態配列の構築を毎回行います。入力・初期経路変換の目安は $O(V+(N+M+L)\log(\max(2,V)))$ で、これに修復・探索の時間が加わります。差分更新で変更箇所だけを処理する計算量にはなりません。

## 4. ユースケース

ここでは問題の使いどころを説明します。距離の表し方や再実行の仕方は、基本問題と組み合わせて使えます。

### R01 全顧客への容量付き配送

同じ倉庫から出発する配送車で全顧客を回り、倉庫へ戻ります。顧客ごとの荷物量、車両数、車両容量、地点間の移動コストを与えます。純配送・純回収のように、担当需要合計で容量を判定できる仕事に適します。短い距離だけでなく、勤務時間を経路上限として課すこともできます。

### R02 複数拠点・異なる終了地点・車両差

各ロボットが現在位置から担当地点を訪ね、充電場所へ戻る場合などです。車両ごとに出発地点、終了地点、容量、経路上限を指定できます。作業時間も経路コストに含められます。終了地点は利用者が指定し、最適な終了地点を自動選択する問題にはなりません。

### R03 時間内に巡回価値を最大化する

限られた時間で観測地点を選ぶ、複数班で点検する、といった用途です。各地点に報酬と作業時間を設定し、班ごとの時間上限を指定します。必ず回る地点も指定できます。運ぶ荷物の容量は考慮せず、班が1つなら単一経路、複数ならチームの問題になります。

### R04 容量内で価値の高い配送を選ぶ

すべてを配送できない日に、対応価値の合計を最大にする仕事を選びます。R03の移動・作業予算に加え、需要と積載容量が必要です。報酬を未対応損失と考えることもできますが、その場合は「未対応損失の最小化を最優先し、同点なら移動コスト最小化」です。損失と距離を足した値の最小化とは異なります。

### R05 ちょうど一定数の地点を選んで回る

候補地からちょうど10地点を点検する、毎ターン一定件数の注文を処理する、といった用途です。実施件数、必須地点、車両容量、経路上限を指定します。どの地点を選ぶかは報酬ではなく総コストで決まります。

### R06 すべての集荷・配達依頼を処理する

荷物や物体を、依頼ごとの出発地点から到着地点へ運びます。各依頼に集荷地点、配達地点、荷物量を設定します。車両容量は経路途中でも守り、必ず同じ車両で集荷を先に行います。車両は空荷で出発する設定です。

### R07 報酬付きの集荷・配達依頼を選ぶ

ロボット群が、運搬できる依頼の中から価値の高いものを選びます。R06の情報に依頼の報酬と必須指定を加えます。集荷だけ、配達だけで報酬を得ることはなく、依頼全体を完了して報酬を得ます。

### R08 ちょうど一定件数の集荷・配達を行う

候補依頼から、必須依頼を含めてちょうど指定件数を選び、運搬コストを小さくします。件数は依頼の数で、訪問地点数ではありません。3件なら集荷・配達を合わせて6地点の作業になります。

### R09 方向によってコストが変わる配送

一方通行、坂道、方向依存の段取りなどで、往路と復路のコストが異なる場合です。両方向の移動コストを独立に与えます。容量付き配送の例を載せますが、報酬選択や集荷配達にも同じ考え方を使えます。移動中の時刻・積載量によって同じ辺のコストが変わる問題は別です。

### R10 道路・通路グラフ上の配送

多数の交差点を持つ地図で、仕事のある地点だけの担当と訪問順を決めます。通路とその移動コストを与え、作業地点間の最短距離を利用します。出力は作業地点の順であり、交差点をすべて含む移動経路は別途復元します。途中の通過だけで仕事が自動完了する問題には追加のモデル化が必要です。

### R11 同じ距離を使う複数試行

外側の探索で仕事集合や指定件数を変える、乱数を変えて複数解を比較する用途です。変化しない距離・登録地点を共有して、各試行の仕事と制約を与えます。第8章は件数固定問題の複数試行を例にします。報酬問題で試行結果を選ぶ場合は、報酬を先に比較する必要があります。

### R12 既存計画・次ターンの残り仕事を改善する

別の処理が作った担当経路や、前ターンの未完了部分から再計画します。距離の共有情報、今回の仕事と車両、制約を満たす既存経路を渡します。報酬や容量が変わる場合は、前の経路を新しい条件で確認してから使います。実行中の荷物や固定済み担当など、前述の表現できない状態は外側で処理します。

### R13 大きなソルバー全体の終了時刻を共有する

経路計画以外にも処理がある問題で、全体の終了時刻から出力の余裕を差し引いて探索を終了させます。各試行の相対時間と共通の終了時刻を指定します。前処理や中断不能な処理があるため、厳密にその時刻で関数が戻る保証ではありません。

## 5. 利用準備

### 5.1 ヘッダ・型・呼び出し形

`#include "routing_solver_v05.hpp"` として利用します。別のライブラリのリンクは不要で、ACLや添付hash mapに依存しません。ソースと掲載例はGCCの `<bits/stdc++.h>` を使うため、GCC系のC++20環境を想定します。ヘッダ単体を主ファイルにすると埋め込みテスト用 `main` が有効になるので、通常は利用側の `.cpp` からincludeします。

solverオブジェクトを生成するコンストラクタはありません。仕事・車両の配列と `RoutingParam` を作り、自由関数 `solve_*` または `improve_*` を呼びます。

| 型パラメータ | 設定 | 選び方 |
|---|---|---|
| `Vertex` | 必須。`bool`以外の整数型 | 通常は `int`。非連続IDでもよい。距離関数は元のIDで受け取る |
| `Load` | 仕事・車両の既定は `long long` | 需要・積載量とその合計が収まる型。PDPでは必ず符号付き型 |
| `Reward` | 仕事の既定は `long long` | 報酬とその合計が収まる型。戻り値の報酬もこの型 |
| `ServiceCost` | 仕事の既定は `long long` | 地点作業コストの入力型。内部で `Calc` に変換される |
| `CostLimit` | 車両の既定は `long long` | 経路上限の入力型。内部で `Calc` に変換される |
| `Calc` | 距離版では省略可能 | 距離関数の戻り値が浮動小数点型なら `double`、それ以外は `long long`。明示するなら `solve_cvrp<double>(...)` 等 |

Context版の `Calc` はContextの型で確定し、solve時に指定し直しません。大きな整数では `make_routing_context<__int128_t>(...)` のように明示します。距離関数自体が `__int128_t` を返すだけでは、既定の計算型は拡張されません。`long double` を保ちたい場合も明示指定します。

### 5.2 単一訪問の仕事

`routing::RouteTask<Vertex, Load, Reward, ServiceCost>` は集成体です。たとえば `routing::RouteTask<int, int, long long, long long>` の要素を `{vertex, demand, reward, service_cost, mandatory}` の順に初期化できます。

| メンバー | 型 | 既定値 | 指定方法 |
|---|---|---|---|
| `vertex` | `Vertex` | 値初期化、整数なら0 | 作業地点ID。仕事間で重複させない |
| `demand` | `Load` | 0 | 非負の需要。Orienteeringでは使わない |
| `reward` | `Reward` | 0 | 非負の報酬。報酬付き問題で選択に使う |
| `service_cost` | `ServiceCost` | 0 | 作業時間等。距離・経路上限と同じ単位にする |
| `mandatory` | `bool` | **`true`** | 任意仕事にしたければ明示的に `false`。全件実施の入口では指定にかかわらず全件必須 |

### 5.3 集荷・配達の仕事

`routing::PickupDeliveryTask<Vertex, Load, Reward, ServiceCost>` を `{pickup, delivery, quantity, reward, pickup_service_cost, delivery_service_cost, mandatory}` の順で初期化します。

| メンバー | 型 | 既定値 | 指定方法 |
|---|---|---|---|
| `pickup` / `delivery` | `Vertex` | 0 | 集荷・配達の作業ID。全依頼の両端点を互いに異なるIDにする |
| `quantity` | `Load` | 0 | 非負の荷物量。配達では内部で負の荷重変化になるので符号付き型を使う |
| `reward` | `Reward` | 0 | 依頼完了時の報酬。端点ごとの報酬ではない |
| `pickup_service_cost` / `delivery_service_cost` | `ServiceCost` | 0 | 各作業のコスト |
| `mandatory` | `bool` | **`true`** | 任意依頼なら `false`。全依頼実施では無視される |

同じ物理座標に複数の仕事があっても、作業IDは分け、距離関数でIDから同じ座標へ対応づけます。同一点間距離0は利用できます。

### 5.4 車両

`routing::RouteVehicle<Vertex, Load, CostLimit>` を `{start, finish, capacity, max_cost}` の順で初期化します。仕事と車両の `Vertex`・`Load` は揃えます。

| メンバー | 型 | 既定値 | 指定方法 |
|---|---|---|---|
| `start` / `finish` | `Vertex` | 0 | 出発・終了地点。同じなら閉路。車両間で同じ拠点を共有してよい |
| `capacity` | `Load` | `numeric_limits<Load>::max() / 4` | 需要合計または途中積載の上限。Orienteeringでは使わない |
| `max_cost` | `CostLimit` | `numeric_limits<CostLimit>::max() / 4` | 移動と作業を含む経路ごとの上限 |

既定の大きな値は数学的な無限大ではありません。実際の制約があるなら明示します。浮動小数点の `Calc` を使っても `CostLimit` の既定型は整数なので、小数の上限を渡すには `RouteVehicle<int, long long, double>` などにします。車両固定費や台数最小化の目的はありません。

### 5.5 距離関数とContext

距離は `dist(from, to)` で元の頂点IDを受け、移動コストを返す関数オブジェクトです。同じ入力は実行中ずっと同じ値を返す必要があります。距離、時間、サービス時間を混ぜるなら、先に同じ単位のコストへ変換します。

`make_routing_context<Calc>(vertices, dist, symmetric)` では、`vertices` に **仕事の全地点と車両の出発・終了地点**を登録します。`vertices` は `std::span<const Vertex>` または `std::vector<Vertex>`、`dist` は距離関数、`symmetric` の既定値は `true` です。登録配列の重複は除かれますが、仕事IDの重複は許されません。

全方向の距離を保存し、`symmetric=true` でも反対方向を省略して読み出すわけではありません。対称性は検査されないため、実際に `dist(u,v)==dist(v,u)` を満たす場合だけ `true` を指定します。

| `RoutingContext<Vertex, Calc>` の公開操作 | 戻り値・意味 | 計算量・注意 |
|---|---|---|
| 既定コンストラクタ | 空のContext | 使用地点のある問題を解く準備には `make_routing_context` を使う |
| `size()` | `int`、登録頂点数 | $O(1)$ |
| `index_of(vertex_id)` | `int`、内部番号。未登録なら `-1` | $O(\log V)$。IDの昇順で番号付けされる |
| `vertex(index)` | `Vertex`、元のID | $O(1)$。有効な内部番号が必要 |
| `distance(from_index, to_index)` | `Calc`、保存距離 | $O(1)$。**元のIDを直接渡さない** |
| `candidates(index)` | `std::span<const int>`、候補の内部番号 | $O(1)$で参照を取得。最大16件。Contextの寿命を超えて保持しない |
| `symmetric()` | `bool`、構築時の指定 | $O(1)$。Context版solveの対称性はこの値で決まる |

R01〜R10の1回限りの呼び出しなら距離関数版が簡単です。R11〜R13はContextを先に作って再利用します。R12ではContextに今回必要な地点がすべて登録されているかを確認します。

### 5.6 探索パラメータ

| `RoutingParam` メンバー | 型 | 既定値 | 指定・選び方 |
|---|---|---|---|
| `time_limit_ms` | `int` | `100` | 非負の相対時間ms。問題全体の残り時間から前後処理の余裕を差し引く。0では必要仕事を構築できる保証もない |
| `deadline` | `std::chrono::steady_clock::time_point` | `time_point::max()` | 絶対終了時刻。相対時間と比べて早い方を使う。`system_clock`の時刻は渡さない |
| `seed` | `std::uint64_t` | `1` | 乱数seed。比較には固定し、複数試行では変える |
| `symmetric` | `bool` | `true` | 距離関数版でContextを構築するときの指定。Context版では既存Contextの指定を使う |
| `max_move_evaluations` | `std::uint64_t` | `0` | 0なら評価回数上限なし。正ならその回数で停止する。時間上限も同時に有効 |

候補幅、除去率、受理許容幅を設定する公開フィールドはありません。調整するのは主に時間とseedです。固定評価回数で比較するときも、相対時間・絶対時刻が先に到達しないように設定します。評価回数は距離関数の呼び出し回数でも、全処理量でもありません。

### 5.7 統計の準備

`RoutingStats stats;` を用意し、最後の引数に `&stats` を渡すと統計を取得できます。省略時は `nullptr`。メンバーの初期値はすべて0で、各solve・improve呼び出しの開始時にも全体がリセットされます。

| メンバー | 型 | 意味 |
|---|---|---|
| `move_evaluations` | `std::uint64_t` | 挿入・移動候補等の評価数 |
| `accepted_moves` / `local_search_runs` | `std::uint64_t` | 単一訪問の局所探索で採用した変更数／局所探索の実行数 |
| `lns_rounds` / `best_updates` | `std::uint64_t` | 除去・再挿入の反復数／最良状態の更新数 |
| `destroyed_items` / `repaired_items` | `std::uint64_t` | 除去仕事数／repairで必須集合へ挿入した仕事数。任意仕事の充填すべてを表す値ではない |
| `distance_build_ms` / `candidate_build_ms` | `double` | 距離行列／候補集合の構築時間。Context再利用時は0 |
| `construction_ms` / `local_search_ms` / `lns_ms` | `double` | 初期構築／初回の独立局所探索／除去再挿入段階の時間ms |
| `total_ms` | `double` | 呼び出し全体で計測した時間ms。Context版でも入力変換を含む |

集荷配達層には独立局所探索がないため `accepted_moves`・`local_search_runs`・`local_search_ms` は0です。反復内の仕事量は `move_evaluations` や `lns_rounds` で見ます。複数呼び出しの累計は呼び出し側で加算します。

## 6. ユースケースごとの使い方

### 6.1 共通の引数と戻り値

以下の `Task` は第5章の具体的な仕事型、`Vehicle` は車両型です。solverは引数型の推論に `std::span` を使うため、仕事・車両のvectorを渡すときは `std::span<const Task>(tasks)`、`std::span<const Vehicle>(vehicles)` と明示します。

呼び出し形は次の3種類です。角括弧は省略可能な追加引数の説明で、C++構文ではありません。

```text
solve_問題名(tasks_span, vehicles_span, dist, [selected_count], param = {}, stats = nullptr)
solve_問題名(context, tasks_span, vehicles_span, [selected_count], param = {}, stats = nullptr)
improve_問題名(context, tasks_span, vehicles_span, initial_routes_span, [selected_count], param = {}, stats = nullptr)
```

`selected_count` は件数固定の入口だけに必要な `int` です。`initial_routes_span` は `std::span<const std::vector<Vertex>>`。improveには距離関数を直接渡す形式がなく、Contextが必要です。入力配列・初期経路は書き換えません。

すべての入口は `RoutingSolution<Vertex, Calc, Reward>` を値で返します。

| メンバー | 型 | 利用方法 |
|---|---|---|
| `feasible` | `bool`、既定 `false` | **最初に確認する**。`true`なら必要件数・必須仕事・資源制約等を満たす。`false`は解なしの証明ではない |
| `routes` | `std::vector<std::vector<Vertex>>` | 車両と同じ順の作業地点列。出発・終了拠点は含めない。実移動として使う際は空でない経路に両端を付ける |
| `unserved_items` | `std::vector<int>` | 未実施の**入力仕事配列の添字**。頂点IDではない。PDPは依頼単位 |
| `travel_cost` | `Calc`、既定0 | 名前にかかわらず、移動に作業コストを加えた総コスト |
| `collected_reward` | `Reward`、既定0 | 完了仕事の報酬合計。全件・件数固定でも計算されるが、選択目的には使わない |

報酬付きで複数解を選ぶなら、まず `feasible`、次に `collected_reward` の大きさ、同点なら `travel_cost` の小ささで比較します。その他の問題は実行可能解の中で `travel_cost` を比較します。

### 6.2 7種類の基本問題と全入口

| ケース | 仕事型 | 構築関数 | 既存解からの関数 | 追加引数・戻り値の解釈 |
|---|---|---|---|---|
| R01・R02 | `RouteTask` | `solve_cvrp` | `improve_cvrp` | 全仕事必須。`mandatory=false`でも省略できない |
| R03 | `RouteTask` | `solve_orienteering` | `improve_orienteering` | 容量・需要は無視。報酬優先で比較する |
| R04 | `RouteTask` | `solve_prize_collecting_vrp` | `improve_prize_collecting_vrp` | 容量を守る報酬最大化。任意仕事は `mandatory=false` |
| R05 | `RouteTask` | `solve_selective_vrp` | `improve_selective_vrp` | `selected_count=K`。必須数以上・仕事数以下で指定 |
| R06 | `PickupDeliveryTask` | `solve_pdvrp` | `improve_pdvrp` | 全依頼必須。出力経路には集荷・配達の両IDが入る |
| R07 | `PickupDeliveryTask` | `solve_prize_collecting_pdvrp` | `improve_prize_collecting_pdvrp` | 報酬は依頼完了単位。未実施添字も依頼単位 |
| R08 | `PickupDeliveryTask` | `solve_selective_pdvrp` | `improve_selective_pdvrp` | `selected_count=K`は依頼数。実施作業地点は原則2K個 |

たとえばR05の距離関数版は `routing::solve_selective_vrp(tasks_span, vehicles_span, dist, K, param, &stats)`、Contextからの改善は `routing::improve_selective_vrp(context, tasks_span, vehicles_span, initial_routes_span, K, param, &stats)` です。戻り値の `feasible` を確認してから各車両の `routes` を使います。

R02では `RouteVehicle` ごとに異なる `start`・`finish`・`capacity`・`max_cost` を与えます。車両が余る場合の空経路は許され、全車両を必ず稼働させる引数はありません。

### 6.3 距離表現・再利用を伴うケース

| ケース | 呼び出しと準備 | 戻り値の使い方・注意 |
|---|---|---|
| R09 | `param.symmetric=false` とし、方向付き配列を読む `dist` で `solve_cvrp`。Contextなら構築時に `false` | 経路を逆向きにして同じ費用と考えない。他の6問題にも同じ指定が使える |
| R10 | 作業地点・拠点からの最短距離を事前計算し、その参照を `dist` にする | `routes` の連続地点間を元グラフの最短路へ展開するには、前駆情報等を別途保存する。例は順序だけを返す |
| R11 | `make_routing_context` を1回呼び、seedを変えて `solve_selective_vrp(context, ..., K, param)` | 実行可能解の最小コストを保持する。距離関数版をループ内で呼ぶと行列を毎回作る |
| R12 | 今回の入力に適合するContextと経路を `improve_cvrp(context, ..., initial_routes_span, param)` へ渡す | 初期経路はコピーして読み込まれる。新しい報酬・費用が再計算される。途中積載等の持ち越しは第3章の制限に従う |
| R13 | `param.deadline=safe_deadline` と試行ごとの相対時間を設定し、Context版 `solve_cvrp` を呼ぶ | 時刻到達前の最良実行可能解を保持する。既に期限を過ぎた例の戻り値は初期値の `feasible=false` |

### 6.4 時間指定の起点

距離関数版は、API開始からContextを作った時間も相対時間予算に含めます。ただしContext構築の途中では停止しません。

Context版は、仕事・車両・初期経路を内部状態へ変換した**後**から相対時間を数えます。絶対 `deadline` はどちらの形式でも利用できますが、入力変換自体を中断するものではありません。

時間確認は候補評価64回ごとの確認と段階の境界で行います。ソート、状態の再構築などを途中停止できないため、時間指定はsoft limitです。評価上限や時間上限が小さすぎると、実行可能解の構築前に終了することがあります。

## 7. 制約・注意点

| 条件・失敗例 | 意味と対処 |
|---|---|
| `feasible=false`の解をそのまま使う | 部分解や資源制約に反する状態の可能性がある。呼び出し側の既存解・代替処理を用意する。実行不能の証明とは解釈しない |
| 任意仕事の `mandatory` を省略する | 既定は `true`。選択可能にしたければ必ず `false` を入れる |
| 件数指定を「最大件数」と考える | 件数固定はちょうどK件。必須数を超えないKや、候補数より大きいKは不適切。K=0なら必須仕事も0件にする |
| `prize_collecting` を罰金と移動費の加重和と考える | 本実装は報酬優先の辞書式目的。重み付き和のトレードオフは指定できない |
| 作業IDの重複・未知ID | 単一訪問の仕事ID、PDPの全端点IDを一意にする。Contextに車両両端を含む全使用IDを登録する |
| 出発・終了拠点を出力経路に重複して入れる | `routes`は作業列。拠点への出発・帰着は車両情報から自動的に費用へ入る。同じ物理場所で作業するなら役割を区別したIDにすると扱いやすい |
| 非対称距離に `symmetric=true` | 正しい差分評価にならない。対称性は自動判定されない。逆に対称距離で `false` は利用可能だが候補・探索が変わる |
| 非metric距離 | 三角不等式は必須ではない。ただし地点を消すと経路費用が増える場合があり、前の経路を編集した後は予算を再確認する |
| 数値範囲不足 | 辺・経路和・積載量・報酬和・差分のすべてが対応型に収まる必要がある。加減算のoverflowは検査されない |
| 内部の無限大sentinelと衝突 | 整数 `Calc` の探索用sentinelは `numeric_limits<Calc>::max()/4`。辺だけでなく有限の候補コストや部分和にも十分な余裕を設ける |
| `NaN`、無限大、巨大値で禁止辺を表す | 比較・差分が不正になる、または巨大費用の辺を選ぶ可能性がある。到達可能な対象で有限コストを構成する。禁止辺の専用制約はない |
| 負の需要・作業費・報酬に依存したモデル | 非負値を前提に入力する。負のコストで報酬最大化を代用しない |
| 任意の0報酬仕事が実施される | 初期充填等で0報酬も入る場合がある。0報酬は「訪問禁止」ではなく、必須でなければ外すことも可能という意味 |
| 時刻・積載・訪問履歴で変わる辺コスト | 1回の実行中の距離は静的。状態依存のcallbackでは表現できない |
| pickup後の初期積載を次ターンへ持ち越す | PDPは各車両の初期積載0。既に集荷済みの荷物、車両固定の積載を直接設定する入口はない |
| CVRPに負の需要を混ぜて集配を表す | 需要和の容量判定では途中積載を検査できない。対になる集配ならPDPを使う |
| 時間窓、多次元容量、車両別の距離、固定担当、同一仕事の分割、同じ作業の再訪問 | 直接の制約指定はない。外側の分割等で本当に独立化できる場合だけ組み合わせる |
| 全車両の使用、車両台数の最小化、総時間の最大値最小化 | 空車両は許され、目的は総コストまたは報酬。車両固定費・makespanの目的はない |
| 指定のない自由な終点 | `finish`は必須の地点値で、終点を自由選択するフラグはない。単一経路の訪問順だけならTSPの自由端点開路も検討する |
| `assert`を無効にすれば無効入力も使えると考える | `-DNDEBUG`は入力条件を解除しない。例のassertも全条件を網羅する入力検証器ではない |
| 時間制限で完全再現を期待する | 同じseedでもCPU負荷・前処理時間で探索量が変わる。固定評価比較では時間上限も十分に確保する |

## 8. ユースケースごとのコード例

各ブロックは単独でコピーできる関数定義です。`main`、標準入力の形式、未定義の補助関数には依存しません。これらの関数の引数の既定値は例として選んだ予算で、ライブラリの既定値とは別です。

距離行列例のIDは行列の添字です。行列は正方形、使用IDは範囲内、仕事IDは一意、数値は第7章の範囲内とします。`symmetric=true`の例では行列を対称にします。戻り値の `feasible` を確認してから計画に使ってください。

### R01 全顧客への容量付き配送

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distance[u][v] は非負・有限・対称な移動費用。IDは0..n-1。
//   地点0は共通倉庫、地点1..n-1は全件必須顧客。demandは地点添字、demand[0]=0。
//   vehicle_count台、各車両の容量vehicle_capacity。作業費用0、経路予算は十分大きい値。
//   time_limit_msはこの例がsolveに渡す時間msで、既定50。時間はsoft limit。
// 出力: feasibleを確認後、routes[r]の顧客ID列を使う。倉庫0は列に含まない。
//   unserved_itemsのiは顧客ID i+1を指す。travel_costは全車両の費用合計。

routing::RoutingSolution<int, long long, long long> solve_delivery_cvrp(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<int>& demand, int vehicle_count,
    int vehicle_capacity, int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;
    constexpr long long inf = std::numeric_limits<long long>::max() / 8;

    const int n = static_cast<int>(distance.size());
    assert(n >= 1 && static_cast<int>(demand.size()) == n);
    assert(demand[0] == 0 && vehicle_count >= 1 && vehicle_capacity >= 0);
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Task> tasks;
    tasks.reserve(std::max(0, n - 1));
    for (int vertex = 1; vertex < n; ++vertex) {
        assert(demand[vertex] >= 0);
        tasks.push_back({vertex, demand[vertex], 0, 0, true});
    }
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{0, 0, vehicle_capacity, inf});
    auto dist = [&](int from, int to) { return distance[from][to]; };

    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = true;
    return routing::solve_cvrp(
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R02 複数拠点・異なる終了地点・車両差

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceはID添字の非負・有限な正方行列。
//   customer_vertex[i], demand[i], service_cost[i]は同じ顧客のID・需要・作業費用。
//   customer_vertexは一意。vehicles[r]={出発ID,終了ID,容量,移動+作業費用上限}。
//   symmetricは実際の行列の対称性に合わせる。time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleを確認し、車両と同じ順のroutesを使う。両拠点は作業列に含めない。
//   空車両の費用は0。unserved_itemsはcustomer_vertex配列の添字。

routing::RoutingSolution<int, long long, long long>
solve_multi_depot_open_vrp(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<int>& customer_vertex,
    const std::vector<int>& demand,
    const std::vector<long long>& service_cost,
    const std::vector<routing::RouteVehicle<int, int, long long>>& vehicles,
    bool symmetric, int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;

    assert(customer_vertex.size() == demand.size());
    assert(customer_vertex.size() == service_cost.size());
    assert(!vehicles.empty());
    const int n = static_cast<int>(distance.size());
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Task> tasks;
    tasks.reserve(customer_vertex.size());
    for (int i = 0; i < static_cast<int>(customer_vertex.size()); ++i) {
        assert(0 <= customer_vertex[i] && customer_vertex[i] < n);
        assert(demand[i] >= 0 && service_cost[i] >= 0);
        tasks.push_back(
            {customer_vertex[i], demand[i], 0, service_cost[i], true});
    }
    auto dist = [&](int from, int to) { return distance[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = symmetric;
    return routing::solve_cvrp(
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R03 時間内に巡回価値を最大化する

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称、ID0が全班共通の出発/帰着地点。
//   prize[v]は地点報酬、service_cost[v]は作業費用、mandatory[v]!=0なら必須。
//   これらは距離行列と同じ長さ。添字0の値は仕事として使わない。
//   route_budget[r]は班rの移動+作業上限。班数はこの配列長、荷物容量は扱わない。
//   time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleを確認。collected_rewardを優先し、同点ならtravel_costが小さい解。
//   routesは訪問地点ID、unserved_itemsのiは地点ID i+1を指す。

routing::RoutingSolution<int, long long, long long> solve_team_orienteering(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<long long>& prize,
    const std::vector<long long>& service_cost,
    const std::vector<unsigned char>& mandatory,
    const std::vector<long long>& route_budget,
    int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, long long, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, long long, long long>;

    const int n = static_cast<int>(distance.size());
    assert(n >= 1 && static_cast<int>(prize.size()) == n);
    assert(static_cast<int>(service_cost.size()) == n);
    assert(static_cast<int>(mandatory.size()) == n && !route_budget.empty());
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Task> tasks;
    for (int vertex = 1; vertex < n; ++vertex) {
        assert(prize[vertex] >= 0 && service_cost[vertex] >= 0);
        tasks.push_back({vertex, 0, prize[vertex], service_cost[vertex],
                         mandatory[vertex] != 0});
    }
    std::vector<Vehicle> vehicles;
    vehicles.reserve(route_budget.size());
    for (long long budget : route_budget) {
        assert(budget >= 0);
        vehicles.push_back({0, 0, 0, budget});
    }
    auto dist = [&](int from, int to) { return distance[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = true;
    return routing::solve_orienteering(
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R04 容量内で価値の高い配送を選ぶ

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称、ID0が共通倉庫。配列はすべて地点添字。
//   demand[v]は荷物量、reward[v]は配送価値、mandatory[v]!=0なら必須。添字0は仕事にしない。
//   vehicle_count台に、同じvehicle_capacityとroute_budget(経路費用上限)を設定。
//   作業費用は0。time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleを確認。報酬最大化を優先し、報酬同点なら費用最小化する解。
//   報酬と移動費の差や和の最適化ではない。unserved_itemsのiは顧客ID i+1。

routing::RoutingSolution<int, long long, long long>
solve_rewarded_delivery(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<int>& demand,
    const std::vector<long long>& reward,
    const std::vector<unsigned char>& mandatory,
    int vehicle_count, int vehicle_capacity, long long route_budget,
    int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;

    const int n = static_cast<int>(distance.size());
    assert(n >= 1 && static_cast<int>(demand.size()) == n);
    assert(static_cast<int>(reward.size()) == n);
    assert(static_cast<int>(mandatory.size()) == n && vehicle_count >= 1);
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Task> tasks;
    for (int vertex = 1; vertex < n; ++vertex) {
        assert(demand[vertex] >= 0 && reward[vertex] >= 0);
        tasks.push_back({vertex, demand[vertex], reward[vertex], 0,
                         mandatory[vertex] != 0});
    }
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{0, 0, vehicle_capacity, route_budget});
    auto dist = [&](int from, int to) { return distance[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = true;
    return routing::solve_prize_collecting_vrp(
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R05 ちょうど一定数の地点を選んで回る

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称、0が倉庫、1..n-1が候補顧客。
//   demandとmandatoryは地点添字。mandatory[v]!=0は必須、添字0は仕事にしない。
//   selected_countは必須顧客を含む実施件数K。必須数<=K<=n-1。
//   vehicle_count・vehicle_capacity・route_budgetは全車両に共通。作業費用0。
//   time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleなら合計ちょうどK顧客を訪問。報酬は考慮しない。
//   routesは顧客ID、unserved_itemsのiは顧客ID i+1。

routing::RoutingSolution<int, long long, long long> solve_exact_k_customers(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<int>& demand,
    const std::vector<unsigned char>& mandatory,
    int selected_count, int vehicle_count, int vehicle_capacity,
    long long route_budget, int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;

    const int n = static_cast<int>(distance.size());
    assert(n >= 1 && static_cast<int>(demand.size()) == n);
    assert(static_cast<int>(mandatory.size()) == n && vehicle_count >= 1);
    const int mandatory_count = static_cast<int>(std::count_if(
        mandatory.begin() + 1, mandatory.end(),
        [](unsigned char value) { return value != 0; }));
    assert(mandatory_count <= selected_count && selected_count <= n - 1);
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Task> tasks;
    for (int vertex = 1; vertex < n; ++vertex) {
        assert(demand[vertex] >= 0);
        tasks.push_back({vertex, demand[vertex], 0, 0,
                         mandatory[vertex] != 0});
    }
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{0, 0, vehicle_capacity, route_budget});
    auto dist = [&](int from, int to) { return distance[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = true;
    return routing::solve_selective_vrp(
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles), dist, selected_count, param);
}
```

### R06 すべての集荷・配達依頼を処理する

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称。request[i]={集荷ID,配達ID,非負荷物量}。
//   全依頼の両端点IDは一意、各IDは行列内。depotは全車両の出発/帰着ID。
//   vehicle_count台、共通vehicle_capacityとroute_budget。作業費用0、全依頼必須。
//   各車両は空荷で出発。time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleなら同一車両で集荷が配達より前に現れ、途中積載も容量内。
//   routesには両端点IDが入る。unserved_itemsはrequestの添字。

routing::RoutingSolution<int, long long, long long> solve_all_pd_requests(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<std::array<int, 3>>& request,
    int depot, int vehicle_count, int vehicle_capacity,
    long long route_budget, int time_limit_ms = 50) {
    using Request = routing::PickupDeliveryTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;

    const int n = static_cast<int>(distance.size());
    assert(0 <= depot && depot < n && vehicle_count >= 1);
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Request> tasks;
    tasks.reserve(request.size());
    for (const auto& [pickup, delivery, quantity] : request) {
        assert(0 <= pickup && pickup < n);
        assert(0 <= delivery && delivery < n && pickup != delivery);
        assert(quantity >= 0);
        tasks.push_back({pickup, delivery, quantity, 0, 0, 0, true});
    }
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{depot, depot, vehicle_capacity, route_budget});
    auto dist = [&](int from, int to) { return distance[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = true;
    return routing::solve_pdvrp(
        std::span<const Request>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R07 報酬付きの集荷・配達依頼を選ぶ

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称。
//   request[i]={集荷ID,配達ID,荷物量,依頼報酬,必須か}。全端点IDを一意にする。
//   depot・vehicle_count・vehicle_capacity・route_budgetは全車両に共通、作業費用0。
//   車両は空荷で出発。time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleを確認し、完了依頼のcollected_rewardを優先して評価する。
//   routesは集荷/配達の地点ID、unserved_itemsは未完了依頼の添字。

routing::RoutingSolution<int, long long, long long> solve_prize_pd_requests(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<std::tuple<int, int, int, long long, bool>>& request,
    int depot, int vehicle_count, int vehicle_capacity,
    long long route_budget, int time_limit_ms = 50) {
    using Request = routing::PickupDeliveryTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;

    const int n = static_cast<int>(distance.size());
    assert(0 <= depot && depot < n && vehicle_count >= 1);
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Request> tasks;
    tasks.reserve(request.size());
    for (const auto& [pickup, delivery, quantity, reward, mandatory] : request) {
        assert(0 <= pickup && pickup < n);
        assert(0 <= delivery && delivery < n && pickup != delivery);
        assert(quantity >= 0 && reward >= 0);
        tasks.push_back(
            {pickup, delivery, quantity, reward, 0, 0, mandatory});
    }
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{depot, depot, vehicle_capacity, route_budget});
    auto dist = [&](int from, int to) { return distance[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = true;
    return routing::solve_prize_collecting_pdvrp(
        std::span<const Request>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R08 ちょうど一定件数の集荷・配達を行う

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称。request[i]={集荷ID,配達ID,荷物量}。
//   mandatory[i]!=0なら依頼iが必須。全依頼の両端点IDは一意。
//   selected_count=Kは実施依頼数で、必須依頼数<=K<=request.size()。
//   depot・vehicle_count・vehicle_capacity・route_budgetは全車両に共通、作業費用0。
//   車両は空荷で出発。time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleならちょうどK依頼・2K作業地点。routesは地点ID、未実施は依頼添字。

routing::RoutingSolution<int, long long, long long> solve_exact_k_pd_requests(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<std::array<int, 3>>& request,
    const std::vector<unsigned char>& mandatory,
    int selected_count, int depot, int vehicle_count,
    int vehicle_capacity, long long route_budget,
    int time_limit_ms = 50) {
    using Request = routing::PickupDeliveryTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;

    const int n = static_cast<int>(distance.size());
    assert(request.size() == mandatory.size());
    assert(0 <= depot && depot < n && vehicle_count >= 1);
    const int mandatory_count = static_cast<int>(std::count_if(
        mandatory.begin(), mandatory.end(),
        [](unsigned char value) { return value != 0; }));
    assert(mandatory_count <= selected_count);
    assert(selected_count <= static_cast<int>(request.size()));
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);

    std::vector<Request> tasks;
    tasks.reserve(request.size());
    for (int i = 0; i < static_cast<int>(request.size()); ++i) {
        const auto [pickup, delivery, quantity] = request[i];
        assert(0 <= pickup && pickup < n);
        assert(0 <= delivery && delivery < n && pickup != delivery);
        assert(quantity >= 0);
        tasks.push_back({pickup, delivery, quantity, 0, 0, 0,
                         mandatory[i] != 0});
    }
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{depot, depot, vehicle_capacity, route_budget});
    auto dist = [&](int from, int to) { return distance[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = true;
    return routing::solve_selective_pdvrp(
        std::span<const Request>(tasks),
        std::span<const Vehicle>(vehicles), dist, selected_count, param);
}
```

### R09 方向によってコストが変わる配送

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: directed_cost[u][v]はuからvへの非負・有限費用。逆方向とは独立。
//   demandは地点添字、depot以外の全地点が必須顧客。depotの需要は使わない。
//   vehicle_count台、共通vehicle_capacity、作業費用0、経路上限は十分大きい値。
//   time_limit_msはsolveの時間ms(既定50)。
// 出力: feasibleを確認し、routesをその向きでたどる。両端にdepotを補って移動する。
//   unserved_itemsはdepotを除きID昇順で作った顧客配列の添字。

routing::RoutingSolution<int, long long, long long> solve_directed_cvrp(
    const std::vector<std::vector<long long>>& directed_cost,
    const std::vector<int>& demand, int depot,
    int vehicle_count, int vehicle_capacity, int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;
    constexpr long long inf = std::numeric_limits<long long>::max() / 8;

    const int n = static_cast<int>(directed_cost.size());
    assert(static_cast<int>(demand.size()) == n);
    assert(0 <= depot && depot < n && vehicle_count >= 1);
    for (const auto& row : directed_cost) {
        assert(static_cast<int>(row.size()) == n);
    }

    std::vector<Task> tasks;
    for (int vertex = 0; vertex < n; ++vertex) {
        if (vertex == depot) continue;
        assert(demand[vertex] >= 0);
        tasks.push_back({vertex, demand[vertex], 0, 0, true});
    }
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{depot, depot, vehicle_capacity, inf});
    auto dist = [&](int from, int to) { return directed_cost[from][to]; };
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = false;
    return routing::solve_cvrp(
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R10 道路・通路グラフ上の配送

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: graph[u]の各要素は{移動先ID,非負辺コスト}。IDは0..graph.size()-1。
//   demand[v]>0の非depot地点だけが仕事、demand[v]=0なら通過地点。需要は非負。
//   depotと全仕事地点は互いに到達可能とする。最短距離・合計費用は安全な数値範囲内。
//   undirected=trueなら同じ重みの逆辺も入力する。vehicle_count台・共通vehicle_capacity。
//   time_limit_ms(既定50)はrouting呼び出しだけの時間。最短路前処理は含まない。
// 出力: feasibleを確認。routesは仕事地点の訪問順で、通路の全頂点列ではない。
//   unserved_itemsはID昇順の正需要顧客配列の添字。実辺列の復元は別途行う。

routing::RoutingSolution<int, long long, long long> solve_graph_cvrp(
    const std::vector<std::vector<std::pair<int, long long>>>& graph,
    const std::vector<int>& demand, int depot,
    int vehicle_count, int vehicle_capacity, bool undirected,
    int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;
    using QueueItem = std::pair<long long, int>;
    constexpr long long inf = std::numeric_limits<long long>::max() / 8;

    const int n = static_cast<int>(graph.size());
    assert(static_cast<int>(demand.size()) == n);
    assert(0 <= depot && depot < n && vehicle_count >= 1);

    std::vector<Task> tasks;
    std::vector<int> relevant = {depot};
    for (int vertex = 0; vertex < n; ++vertex) {
        if (vertex == depot || demand[vertex] == 0) continue;
        assert(demand[vertex] > 0);
        tasks.push_back({vertex, demand[vertex], 0, 0, true});
        relevant.push_back(vertex);
    }

    std::vector<int> rank(n, -1);
    for (int i = 0; i < static_cast<int>(relevant.size()); ++i) {
        rank[relevant[i]] = i;
    }
    std::vector<std::vector<long long>> shortest(
        relevant.size(), std::vector<long long>(n, inf));
    for (int source_index = 0;
         source_index < static_cast<int>(relevant.size()); ++source_index) {
        const int source = relevant[source_index];
        auto& distance = shortest[source_index];
        std::priority_queue<QueueItem, std::vector<QueueItem>,
                            std::greater<QueueItem>> queue;
        distance[source] = 0;
        queue.emplace(0, source);
        while (!queue.empty()) {
            const auto [current_distance, vertex] = queue.top();
            queue.pop();
            if (current_distance != distance[vertex]) continue;
            for (const auto& [to, weight] : graph[vertex]) {
                assert(0 <= to && to < n && weight >= 0);
                if (weight > inf - current_distance) continue;
                const long long next_distance = current_distance + weight;
                if (next_distance < distance[to]) {
                    distance[to] = next_distance;
                    queue.emplace(next_distance, to);
                }
            }
        }
    }
    for (int from : relevant) {
        for (int to : relevant) assert(shortest[rank[from]][to] < inf);
    }

    auto dist = [&](int from, int to) { return shortest[rank[from]][to]; };
    std::vector<Vehicle> vehicles(
        vehicle_count, Vehicle{depot, depot, vehicle_capacity, inf});
    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = undirected;
    return routing::solve_cvrp(
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles), dist, param);
}
```

### R11 同じ距離を使う複数試行

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称な正方行列。tasks/vehicles内の全IDを有効添字にする。
//   tasks={ID,需要,報酬,作業費用,必須}。このK件選択では報酬は選択目的に使わない。
//   vehicles={出発ID,終了ID,容量,経路上限}。必須数<=selected_count<=tasks.size()。
//   trials>=1回、各回time_limit_ms_per_trial ms。Context構築時間はこれとは別。
// 出力: 見つかった実行可能解のうち最小費用。全試行失敗時はfeasible=false。
//   routesは車両順、未実施はtasks添字。乱数seedは1,2,...を使う。

routing::RoutingSolution<int, long long, long long>
solve_selective_vrp_with_multiple_seeds(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<routing::RouteTask<int, int, long long, long long>>& tasks,
    const std::vector<routing::RouteVehicle<int, int, long long>>& vehicles,
    int selected_count, int trials, int time_limit_ms_per_trial) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;
    using Solution = routing::RoutingSolution<int, long long, long long>;
    assert(trials >= 1);

    std::vector<int> vertices;
    vertices.reserve(tasks.size() + vehicles.size() * 2);
    for (const Task& task : tasks) vertices.push_back(task.vertex);
    for (const Vehicle& vehicle : vehicles) {
        vertices.push_back(vehicle.start);
        vertices.push_back(vehicle.finish);
    }
    auto dist = [&](int from, int to) { return distance[from][to]; };
    const auto context = routing::make_routing_context<long long>(
        vertices, dist, true);

    Solution best;
    bool has_result = false;
    for (int trial = 0; trial < trials; ++trial) {
        routing::RoutingParam param;
        param.time_limit_ms = time_limit_ms_per_trial;
        param.seed = static_cast<std::uint64_t>(trial + 1);
        const Solution candidate = routing::solve_selective_vrp(
            context, std::span<const Task>(tasks),
            std::span<const Vehicle>(vehicles), selected_count, param);
        if (!has_result ||
            (candidate.feasible &&
             (!best.feasible || candidate.travel_cost < best.travel_cost))) {
            best = candidate;
            has_result = true;
        }
    }
    return best;
}
```

### R12 既存計画・次ターンの残り仕事を改善する

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: contextは今回の全仕事・車両両端を登録した、変更されていない距離スナップショット。
//   tasks={ID,需要,報酬,作業費用,必須}。このCVRP入口では全仕事が必須。
//   vehicles={出発ID,終了ID,容量,経路上限}。次ターンの新しい値を指定できる。
//   initial_routes[r]は今回の車両rに対応する作業ID列。拠点を含めず、重複なし。
//   削除済み仕事は先に外す。残す経路は今回の容量・費用上限を満たすこと。
//   必須仕事の欠落は可。time_limit_msは探索の相対時間ms(既定50)。
// 出力: 初期経路を変更せず、新しい解を返す。feasibleを確認して利用する。
//   距離・未登録地点を変更した場合は、呼び出し前に新しいContextを作る。

routing::RoutingSolution<int, long long, long long> improve_existing_cvrp(
    const routing::RoutingContext<int, long long>& context,
    const std::vector<routing::RouteTask<int, int, long long, long long>>& tasks,
    const std::vector<routing::RouteVehicle<int, int, long long>>& vehicles,
    const std::vector<std::vector<int>>& initial_routes,
    int time_limit_ms = 50) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;
    assert(initial_routes.size() == vehicles.size());
    for (const Task& task : tasks) assert(context.index_of(task.vertex) >= 0);
    for (const Vehicle& vehicle : vehicles) {
        assert(context.index_of(vehicle.start) >= 0);
        assert(context.index_of(vehicle.finish) >= 0);
    }

    routing::RoutingParam param;
    param.time_limit_ms = time_limit_ms;
    param.symmetric = context.symmetric();
    return routing::improve_cvrp(
        context, std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles),
        std::span<const std::vector<int>>(initial_routes), param);
}
```

### R13 大きなソルバー全体の終了時刻を共有する

```cpp
#include "routing_solver_v05.hpp"
#include <bits/stdc++.h>

// 入力: contextはtasks/vehiclesで使う全IDを登録した静的距離情報。
//   tasksは全件必須の単一訪問仕事。vehicles={出発ID,終了ID,容量,経路上限}。
//   safe_deadlineはsteady_clockの絶対時刻。出力処理の余裕を既に差し引いておく。
//   time_limit_ms_per_trial>0は1試行の相対時間。first_seedは最初のseed(既定1)。
// 出力: 期限までに見つかった最小費用の実行可能解。期限が既に過ぎていた場合も含め、
//   解を得ていなければfeasible=false。soft limitなので時刻超過に余裕を持たせる。

routing::RoutingSolution<int, long long, long long> solve_cvrp_until_deadline(
    const routing::RoutingContext<int, long long>& context,
    const std::vector<routing::RouteTask<int, int, long long, long long>>& tasks,
    const std::vector<routing::RouteVehicle<int, int, long long>>& vehicles,
    std::chrono::steady_clock::time_point safe_deadline,
    int time_limit_ms_per_trial, std::uint64_t first_seed = 1) {
    using Task = routing::RouteTask<int, int, long long, long long>;
    using Vehicle = routing::RouteVehicle<int, int, long long>;
    using Solution = routing::RoutingSolution<int, long long, long long>;
    assert(time_limit_ms_per_trial > 0);

    Solution best;
    bool has_result = false;
    for (std::uint64_t seed = first_seed;
         std::chrono::steady_clock::now() < safe_deadline; ++seed) {
        routing::RoutingParam param;
        param.time_limit_ms = time_limit_ms_per_trial;
        param.deadline = safe_deadline;
        param.seed = seed;
        const Solution candidate = routing::solve_cvrp(
            context, std::span<const Task>(tasks),
            std::span<const Vehicle>(vehicles), param);
        if (!has_result ||
            (candidate.feasible &&
             (!best.feasible || candidate.travel_cost < best.travel_cost))) {
            best = candidate;
            has_result = true;
        }
    }
    return best;
}
```

## 9. 実装

### 9.1 概要と流れ

距離行列を参照しながら仕事を挿入して初期解を作り、仕事の一部を除去・再挿入するLarge Neighborhood Search（LNS）を繰り返します。単一訪問では経路内・経路間の局所探索も行います。集荷配達は依頼対を単位にしたLNSで改善し、独立した局所探索段階はありません。

1. 使用IDをContextの内部番号へ変換し、仕事・車両データを作る。
2. 新規構築なら必須仕事を挿入する。improveなら入力経路を読み込み、不足する必須仕事を修復する。
3. 報酬・件数固定問題では、任意仕事の選択と挿入を行う。
4. 単一訪問では局所探索し、その解を最良解と現在解として保持する。
5. 現在解をコピーし、一部の仕事を除去して再挿入する。単一訪問では再び局所探索する。
6. 最良解更新・現在解への受理を判定し、制限まで反復する。
7. 保持した最良状態を元のIDと仕事添字へ変換して返す。

時間切れ時に必須仕事が未完了なら、最良の部分状態が `feasible=false` で返ることがあります。実行可能性の判定と最良状態の保持は、スコア比較と分けて行います。

### 9.2 距離と候補集合

ContextはIDをソート・重複除去し、すべての順序対の距離を保存します。候補は各頂点につき最大16頂点です。対称時は出辺距離、非対称時は `min(d(u,v), d(v,u))` を並び替えのキーにして、合計16頂点を選びます。非対称時に出辺16個と入辺16個を別々に保持する方式ではありません。

候補集合は調べる挿入場所や局所探索相手を絞るために使います。実際のコストは辺の向きを保って評価します。Contextを大きく作り、今回の仕事をそのごく一部に絞ると、候補の中に今回未使用の頂点が多くなることがあります。広く登録すれば常に探索上も有利というわけではありません。

### 9.3 必須仕事の構築とrepair

単一訪問の新規構築は、需要の大きさ、拠点からの距離を用いて必須仕事を並べます。残り必須仕事が64件以下では、各仕事の車両ごとの最良挿入費用を比較し、最良費用と、それより大きい次善費用の差をregretとします。同費用の候補は次善として重複計上せず、次善がなければ差を0とします。regretが大きい仕事を先に扱い、後回しにしたときの挿入先の悪化を避けようとします。64件より多い段階では先頭仕事を順に挿入します。

集荷配達のregretを使う閾値は残り48依頼以下です。大きい初期構築の段階では集荷と配達を連続して挿入し、初期構築の負荷を抑えます。小さい必須集合とrepairでは、集荷・配達を離した位置も調べます。途中のどの積載量も容量を超えないかを検査します。

単一訪問の挿入は、仕事数100以下または選択済み32件未満なら全経路の全位置を走査します。それ以外は各経路の端と候補近傍の前後を主に使います。PDPも問題・経路の大きさに応じて全位置走査と候補位置の走査を使い分けます。これらは内部の走査範囲であり、公開のモード指定ではありません。

### 9.4 任意仕事の選択

1回の充填で未選択仕事を最大32件調べます。仕事数と互いに素な走査幅を用いて入力配列内を巡回し、選択済み件数から開始位置を決めます。全未選択仕事の最良挿入を毎回すべて計算するわけではありません。

件数固定では追加費用の小さい仕事、報酬付きでは主に「報酬を `max(1, 追加費用)` で割った値」の大きい仕事を選びます。これは構築時の選択基準で、返す解の目的関数は第1章の辞書式評価です。報酬とコストの比自体を最終目的にしていません。

### 9.5 単一訪問の局所探索

| 近傍 | 操作 | 主な制約確認 |
|---|---|---|
| relocate | 1仕事を同じ経路内または別経路へ移す | 移動先・移動元の費用、容量 |
| swap | 2仕事の位置を交換する | 両経路の費用・需要合計 |
| 2-opt | 同一経路の区間を反転する | 非対称では反転区間内の辺の向きも評価 |
| 2-opt* | 2経路の後半部分を交換する | 各車両の終了地点への接続、容量、経路上限 |

仕事を抽選し、選択済みなら候補近傍の仕事と変更を試します。改善しない試行が続けば局所探索を終えます。選択対象・候補幅・時間に制限があるので、全近傍についての局所最適性は保証しません。

### 9.6 除去、受理、最良解

除去方法は反復ごとに、ランダム、除去による費用節約が大きい順、近い仕事をまとめる順の3種類を循環します。単一訪問は選択済みの15〜30%を1〜64件、PDPは10〜25%を1〜48依頼の範囲へ収めて除去します。PDPでは両端点を対で外します。

候補が最良なら最良解と現在解を更新します。それ以外でも現在解より良ければ採用し、両方が実行可能ならrecord-to-recordの許容範囲による受理も行います。コストの許容増分は最良コストの絶対値の0.5%と1の大きい方です。報酬問題では、これに加えて報酬減少も最良報酬の絶対値の0.5%と1の大きい方以下である必要があります。小さい数値で常に厳密な0.5%だけを許す仕様ではありません。

32反復ごとに現在解を最良解へ戻します。探索中に一時的な悪化を受け入れても、最終出力は保持している最良状態です。不完全な状態同士では、資源制約、未処理の必須数、全件・K件への近さを先に比較し、実行可能解に近づけます。

### 9.7 差分評価と状態管理

経路の累積費用・需要、頂点位置、担当車両、PDPの各位置の積載量などを保持します。relocateとswapは変更される境界辺を用いて候補を評価します。非対称の区間反転は向きの差を累積した値を使います。候補評価が軽くても、実際にvectorを挿入・削除・反転する処理や、採用後の状態再構築まで常に定数時間になるわけではありません。

PDPの挿入では、集荷位置から配達位置までの積載最大値を前進走査で再利用します。除去費用は、隣接した集荷配達なら3辺を1辺へ、離れた対ならそれぞれの境界辺を接続し直す差分で求めます。最後の依頼を外す場合は、空経路の費用0へ変わるという定義を適用します。

内部の問題型・探索状態は `RoutingContext` のprivateな入れ子型に置かれ、公開の更新用オブジェクトではありません。共有できるのは読み取り用のContextで、探索状態・乱数・統計は各呼び出しのものです。
