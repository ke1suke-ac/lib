# 配送・巡回ルートライブラリ サンプルコード集

対象は `routing_solver_v07.hpp` です。問題の意味、全公開API、独立して動く18個の例を順に説明します。C++20の基本的な配列・関数・入出力が分かる読者を想定し、数式の記号は本文で説明します。

## 1. 何を解くライブラリか？

### 1.1 複数の車両に仕事を割り当て、訪問順を決める

たとえば、倉庫にある荷物を複数の車両で顧客へ届けるとき、「どの車両が、どの顧客を、どの順番で訪ねるか」を求めます。車両ごとの積載量と経路コストの上限を守り、移動と作業にかかる総コストを小さくします。

すべての仕事を実施する問題に加え、限られた資源で報酬の大きい仕事を選ぶ問題、ちょうど指定件数だけ実施する問題を扱います。仕事は、1か所を訪ねれば完了する「単一訪問」と、集荷地点を訪ねてから同じ車両で配達地点へ運ぶ「集荷・配達対」の2種類です。

ここでの「最小化」は、条件を守る計画の中で費用をできるだけ小さくすること、「最大化」は報酬をできるだけ大きくすることです。記号$\sum$は「指定された範囲の値を全部足す」、$\le$は「以下」を表します。添字は仕事や車両、訪問順の番号です。

### 1.2 経路コストの定義

車両 $r$ の経路を、作業地点の列 $P_ {r}=(v_ {r,1},\ldots,v_ {r,m_ {r}})$ とします。空でない経路のコストは次の値です。

$$C_ {r}=d(s_ {r},v_ {r,1})+\sum_ {j=1}^{m_ {r}-1}d(v_ {r,j},v_ {r,j+1})+d(v_ {r,m_ {r}},t_ {r})+\sum_ {j=1}^{m_ {r}}h(v_ {r,j})$$

たとえば拠点→A→B→拠点の移動費用が2、3、4で、AとBの作業費用が各1なら、その車両の費用は2+3+4+1+1=11です。距離で測るなら作業費用も距離に換算するか0にし、時間で測るなら移動・作業を同じ時間単位でそろえます。

全車両のコストと獲得報酬を次のように定義します。

$$C=\sum_ {r=1}^{M}C_ {r},\qquad R=\sum_ {i\in I}p_ {i} x_ {i}$$

| 記号 | 意味 |
|---|---|
| $I$、$i$ | 入力された仕事の集合と、その1件。集荷・配達では2地点をまとめて1仕事と数える |
| $M$、$r$ | 車両数と車両番号 |
| $P_ {r}$、$m_ {r}$ | 車両 $r$ の作業地点列と、その長さ。車両の出発・終了地点はこの列に含めない |
| $v_ {r,j}$、$j$ | 車両 $r$ が $j$ 番目に作業する地点と、経路内の位置 |
| $s_ {r}$、$t_ {r}$ | 車両 $r$ の出発地点と終了地点。同じ地点でもよい |
| $d(u,v)$ | 地点 $u$ から地点 $v$ への移動コスト。$u,v$ は地点ID。距離、時間、燃料など同じ単位の数値 |
| $h(v)$ | 地点 $v$ での作業コスト。集荷・配達ではそれぞれの地点に設定する |
| $C_ {r}$、$C$ | 車両ごとの移動・作業コストと、その合計 |
| $x_ {i}$ | 仕事 $i$ を完了すれば1、実施しなければ0。集荷・配達対は両地点を条件どおり訪ねて初めて1 |
| $p_ {i}$、$R$ | 仕事 $i$ の報酬と、完了した仕事の報酬合計 |

$m_ {r}=0$ の空経路は $C_ {r}=0$ と定義します。出発・終了地点が異なっていても、仕事を割り当てなかった車両の移動は数えません。閉じた経路は $s_ {r}=t_ {r}$、指定した別の場所で終える経路は $s_ {r}\ne t_ {r}$ です。後者も終了地点までの移動を数えます。

### 1.3 目的関数と実施条件

**全件実施**では、すべての仕事を実施したうえで総コストを最小化します。

$$\min C\quad\text{subject to }x_ {i}=1\quad(i\in I)$$

**報酬付き選択**では、必須の仕事を実施し、まず獲得報酬を最大化します。報酬が同じ解の間だけで総コストを比較します。

$$\max_ {\mathrm{lex}}(R,-C)\quad\text{subject to }x_ {i}=1\quad(i\in I_ {\mathrm{must}})$$

ここで $I_ {\mathrm{must}}$ は必須仕事の集合、$\max_ {\mathrm{lex}}$ は左の成分を優先する辞書式最大化です。$-C$は費用の符号を逆にした値であり、これを大きくすることは費用を小さくすることと同じです。報酬が1増える解は、コストが増えても、資源制約を守る限り目的関数上は優れます。報酬と移動コストの加重和を最大化する仕様ではありません。

**件数固定選択**では、必須仕事を含めてちょうど $K$ 件を実施し、総コストを最小化します。

$$\min C\quad\text{subject to }\sum_ {i\in I}x_ {i}=K,\quad x_ {i}=1\quad(i\in I_ {\mathrm{must}})$$

$K$ は呼び出し側が指定する実施件数です。「最大 $K$ 件」や「少なくとも $K$ 件」ではありません。この問題の選択に報酬は使いません。

たとえば報酬10・費用30の計画と報酬9・費用5の計画がどちらも制約内なら、報酬付き選択は前者を優先します。報酬が両方10なら、費用の小さい方を優先します。

### 1.4 容量・経路上限・集荷配達の制約

すべての問題で、車両ごとの経路上限 $B_ {r}$ に対して $C_ {r}\le B_ {r}$ を守ります。

単一訪問で容量制約を使う問題では、車両の担当仕事集合を $I_ {r}$、仕事 $i$ の需要を $q_ {i}$、車両容量を $Q_ {r}$ として、次を守ります。

$$\sum_ {i\in I_ {r}}q_ {i}\le Q_ {r}$$

純配送なら出発時にこの需要合計を積み、配達に伴って荷物が減る状況を表せます。Orienteeringだけは積載量を制約に使いません。

集荷・配達では、同じ車両が集荷を先に、配達を後に行います。仕事 $i$ の集荷地点を $a_ {i}$、配達地点を $b_ {i}$ とし、地点 $v$ を処理したときの荷物増減を $\Delta(v)$ とします。

$$\Delta(a_ {i})=q_ {i},\qquad\Delta(b_ {i})=-q_ {i},\qquad L_ {r,j}=\sum_ {k=1}^{j}\Delta(v_ {r,k}),\qquad 0\le L_ {r,j}\le Q_ {r}$$

$L_ {r,j}$ は車両 $r$ が $j$ 番目の作業を終えた直後の積載量、$k$ は経路内の位置を足し合わせる添字です。出発時と全仕事完了時の積載量は0です。各仕事を複数車両で分担したり、同じ仕事から報酬を複数回得たりはしません。

## 2. APIリファレンス

### 2.1 読み込み、型、共通の前提

`routing_solver_v07.hpp` を同じフォルダへ置き、C++20でincludeします。名前空間は `routing`。標準ライブラリとGCC拡張だけを使い、別ライブラリのリンクは不要です。通常は `.cpp` からincludeし、ヘッダを主ファイルとして直接コンパイルすると内蔵テスト用mainが有効になります。

本ライブラリは自由関数を呼ぶ形式です。探索器のコンストラクタを呼ぶ必要はありません。**ユーザー向けの列挙型はありません**。問題の種類は `solve_*` / `improve_*` の関数名で、真偽の設定は次の表のbool値で選びます。

| 型パラメータ | 省略時 | 意味・取り得る型 |
|---|---|---|
| `Vertex` | 入力型の宣言では省略不可。関数では入力から推論 | 元の頂点ID。`std::integral`を満たす、bool以外の整数型。通常はint。非連続・負のIDもよいが距離callbackが対応すること |
| `Load` | `long long` | 需要・容量・積載量。数量とその合計が収まる数値型。PDPでは配達に伴う減算を行うので符号付き型を使う |
| `Reward` | `long long` | 仕事の報酬とその合計。非負値を使う |
| `ServiceCost` | `long long` | 各作業の費用。非負値。内部ではCalcへ変換される |
| `CostLimit` | `long long` | 車両の経路費用上限。非負値。内部ではCalcへ変換される |
| `Calc` | `void`が自動選択を意味し、距離callbackの戻り値が浮動小数点型ならdouble、それ以外はlong long | 距離・費用・差分の計算型。整数なら符号付き。`solve_cvrp<__int128_t>`など先頭のテンプレート引数で指定する。long doubleも明示が必要 |
| `F` | callbackから推論 | `dist(Vertex from, Vertex to)`として呼べる関数オブジェクト。元IDで移動費用を返す |

仕事と車両のVertex・Loadは一致させます。Calcをdoubleへ変えても、車両のCostLimitや仕事のServiceCostが自動でdoubleになるわけではありません。各入力型も目的の単位に合わせます。Context版のCalcはContextの型で決まり、solve時に指定し直しません。

Calcの自動選択は、距離関数版solveとmake_routing_contextに対する規則です。RoutingSolutionとRoutingContextの型名を自分で書く場合は、VertexとCalcの両方を指定します。vector版の生成関数に現れるAllocatorは、そのvectorの型から推論されます。

距離、作業費、数量、報酬は非負の有限値を使います。総和だけでなく、差分計算の途中も各型に収めてください。NaN・無限大を渡せません。巨大な有限値の辺も「禁止辺」としては扱われません。距離は1回の実行中に変化させず、時刻・積載量・訪問履歴に依存させません。通常は最短路を事前に求めて渡します。

### 2.2 仕事と車両の入力型

これらは公開メンバーを持つ集成体です。`Task t;` や `Task t{};` で下表の初期値になり、`{...}` では表の上から順に指定します。独自の設定メソッドはありません。

**`RouteTask<Vertex, Load=long long, Reward=long long, ServiceCost=long long>`**

| メンバー | 型 | 初期値 | 意味・有効な値 |
|---|---|---|---|
| `vertex` | Vertex | `Vertex{}`、整数なら0 | 仕事の地点ID。全仕事の中で一意 |
| `demand` | Load | 0 | 非負の需要。単一訪問の容量制約では担当需要の合計を使う。Orienteeringでは無視 |
| `reward` | Reward | 0 | 完了報酬。報酬付き入口で最大化し、他の入口では合計を返すだけ |
| `service_cost` | ServiceCost | 0 | 作業時間などの費用。移動費用に加算する |
| `mandatory` | bool | true | trueは必ず実施、falseは選択可能。全件必須の入口では値によらず全件を要求 |

**`PickupDeliveryTask<Vertex, Load=long long, Reward=long long, ServiceCost=long long>`**

| メンバー | 型 | 初期値 | 意味・有効な値 |
|---|---|---|---|
| `pickup` | Vertex | 0 | 集荷作業のID |
| `delivery` | Vertex | 0 | 配達作業のID。pickupとは別ID |
| `quantity` | Load | 0 | 非負の荷物量。集荷で増え、配達で減る |
| `reward` | Reward | 0 | 集荷と配達の両方を完了したときの報酬 |
| `pickup_service_cost` | ServiceCost | 0 | 集荷時の作業費用 |
| `delivery_service_cost` | ServiceCost | 0 | 配達時の作業費用 |
| `mandatory` | bool | true | trueは必須依頼、falseは選択可能。全件必須の入口では無視 |

全依頼のpickup・deliveryを互いに異なる作業IDにします。同じ物理座標に複数の仕事がある場合はIDを分け、距離callbackで同じ座標へ対応づけます。車両の初期積載は0です。既に集荷済みの荷物を積んだ状態を直接渡す入口はありません。

**`RouteVehicle<Vertex, Load=long long, CostLimit=long long>`**

| メンバー | 型 | 初期値 | 意味・有効な値 |
|---|---|---|---|
| `start` | Vertex | 0 | 出発地点。複数車両で共有可能 |
| `finish` | Vertex | 0 | 終了地点。startと同じでも異なってもよい。自由な終点を指定する特別値はない |
| `capacity` | Load | `numeric_limits<Load>::max()/4` | 非負の容量上限。Orienteeringでは無視 |
| `max_cost` | CostLimit | `numeric_limits<CostLimit>::max()/4` | 非負の経路費用上限。移動費用と作業費用を両方含む |

既定の上限は大きな有限値です。空経路の費用は0であり、その車両のstart→finishの移動は数えません。全車両の使用や使用台数の最小化は要求しません。

### 2.3 `RoutingParam`の全メンバー

| メンバー | 型 | 初期値 | 意味・取り得る値 |
|---|---|---|---|
| `time_limit_ms` | int | 100 | 0以上の相対時間、単位ms。0では必要な仕事を構築できない場合もある |
| `deadline` | `std::chrono::steady_clock::time_point` | `time_point::max()` | 絶対終了時刻。相対時間と比べて早い方を使う。過去の時刻なら探索予算切れ。system_clockの値は使わない |
| `seed` | `std::uint64_t` | 1 | 乱数seed。0を含む型の範囲内の整数。比較時は固定し、複数試行では変える |
| `symmetric` | bool | true | trueは全対象地点対でdist(u,v)=dist(v,u)を利用者が保証する指定。falseは非対称距離に対応。自動検査しない。Context版ではContextの設定が優先される |
| `max_move_evaluations` | `std::uint64_t` | 0 | 0は回数上限なし。正の値は探索候補評価数の上限。時間制限も同時に有効 |

距離関数版では距離の準備も相対時間に含みます。Contextを事前に作った時間は、その後のContext版solveの相対時間には入りません。全体の予算へ含めたければ、準備前に絶対deadlineを決めます。時計確認は処理の区切りで行い、厳密な時刻どおりに停止する保証はありません。評価数は全処理の命令数でも距離callbackの呼び出し数でもありません。時間が先に尽きると、同seed・同評価上限でも探索量は変わります。

### 2.4 全solve・improve関数

表を短くするため、`tasks` は対応する仕事型の `std::span<const Task>`、`vehicles` は `std::span<const Vehicle>`、`initial` は `std::span<const std::vector<Vertex>>` と書きます。`std::vector`をそのまま渡すoverloadはないため、`std::span<const Task>(tasks_vector)`などへ変換します。明示的に要素型を書けば、読み取り専用かつ動的な要素数のspanになります。

| 共通引数 | 省略時 | 意味・条件 |
|---|---|---|
| `tasks` | 省略不可 | 仕事の読み取り専用配列。入力配列の添字が仕事番号になる。空配列も扱える |
| `vehicles` | 省略不可 | 車両の読み取り専用配列。結果の経路と同じ順。実施する仕事があるなら担当可能な車両が必要 |
| `dist` | 省略不可 | 元ID間の静的な有限移動費用。距離関数版のみ |
| `context` | 省略不可 | `const RoutingContext<Vertex,Calc>&`。今回使う全仕事地点・全車両端点が登録済みであること |
| `initial` | 省略不可 | improveのみ。車両数と同じ本数の作業列。拠点は追加しない。仕事の重複・未知ID・資源違反は禁止 |
| `K` | 省略不可 | selectiveのみのint。0以上N以下で必須仕事数以上。PDPでは地点数でなく依頼数 |
| `param` | `{}` | `const RoutingParam&`。前節の初期値を使う |
| `stats` | `nullptr` | `RoutingStats*`。不要なら省略。渡すと呼び出し開始時にリセットして今回の統計を記録 |

次の7行にそれぞれ3種類の入口があり、計21種類です。全行で最後の `param` と `stats` は省略可能です。`Calc`を明示できるのは距離関数版です。

| 関数の基本名 | 仕事型 | 目的・制約 |
|---|---|---|
| `cvrp` | RouteTask | 全件必須、容量・経路上限、費用最小化 |
| `orienteering` | RouteTask | 必須を含む任意選択、経路上限のみ、報酬最大・同報酬なら費用最小 |
| `prize_collecting_vrp` | RouteTask | 必須を含む任意選択、容量・経路上限、報酬最大・同報酬なら費用最小 |
| `selective_vrp` | RouteTask | 必須を含むちょうどK件、容量・経路上限、費用最小化 |
| `pdvrp` | PickupDeliveryTask | 全依頼必須、同一車両・集荷先行・途中容量・経路上限、費用最小化 |
| `prize_collecting_pdvrp` | PickupDeliveryTask | 必須を含む任意依頼、PDPの全制約、報酬最大・同報酬なら費用最小 |
| `selective_pdvrp` | PickupDeliveryTask | 必須を含むちょうどK依頼、PDPの全制約、費用最小化 |

| 公開関数と全引数の順序 | 戻り値・用途 |
|---|---|
| `solve_cvrp<Calc>(tasks, vehicles, dist, param, stats)` | 全件配送の新規構築 |
| `solve_cvrp(context, tasks, vehicles, param, stats)` | 距離を再利用した全件配送の新規構築 |
| `improve_cvrp(context, tasks, vehicles, initial, param, stats)` | 既存の全件配送計画を利用 |
| `solve_orienteering<Calc>(tasks, vehicles, dist, param, stats)` | 容量を使わない報酬巡回の新規構築 |
| `solve_orienteering(context, tasks, vehicles, param, stats)` | 距離を再利用した報酬巡回の新規構築 |
| `improve_orienteering(context, tasks, vehicles, initial, param, stats)` | 既存の報酬巡回を利用 |
| `solve_prize_collecting_vrp<Calc>(tasks, vehicles, dist, param, stats)` | 容量付き報酬配送の新規構築 |
| `solve_prize_collecting_vrp(context, tasks, vehicles, param, stats)` | 距離を再利用した報酬配送の新規構築 |
| `improve_prize_collecting_vrp(context, tasks, vehicles, initial, param, stats)` | 既存の報酬配送を利用 |
| `solve_selective_vrp<Calc>(tasks, vehicles, dist, K, param, stats)` | K地点選択の新規構築 |
| `solve_selective_vrp(context, tasks, vehicles, K, param, stats)` | 距離を再利用したK地点選択 |
| `improve_selective_vrp(context, tasks, vehicles, initial, K, param, stats)` | 既存のK地点計画を利用 |
| `solve_pdvrp<Calc>(tasks, vehicles, dist, param, stats)` | 全依頼処理の新規構築 |
| `solve_pdvrp(context, tasks, vehicles, param, stats)` | 距離を再利用した全依頼処理 |
| `improve_pdvrp(context, tasks, vehicles, initial, param, stats)` | 既存の全依頼計画を利用 |
| `solve_prize_collecting_pdvrp<Calc>(tasks, vehicles, dist, param, stats)` | 報酬付き集荷配達の新規構築 |
| `solve_prize_collecting_pdvrp(context, tasks, vehicles, param, stats)` | 距離を再利用した報酬付き集荷配達 |
| `improve_prize_collecting_pdvrp(context, tasks, vehicles, initial, param, stats)` | 既存の報酬付き集荷配達を利用 |
| `solve_selective_pdvrp<Calc>(tasks, vehicles, dist, K, param, stats)` | K依頼選択の新規構築 |
| `solve_selective_pdvrp(context, tasks, vehicles, K, param, stats)` | 距離を再利用したK依頼選択 |
| `improve_selective_pdvrp(context, tasks, vehicles, initial, K, param, stats)` | 既存のK依頼計画を利用 |

全関数の戻り値は **`RoutingSolution<Vertex, Calc, Reward>`を値で返す** 形です。tasks・vehicles・initial・contextは書き換えません。improveへ距離callbackを直接渡す形式はありません。

improveでは必須仕事が欠けた部分解も渡せますが、既に入っている仕事は各車両の容量・費用上限を守る必要があります。PDPでは依頼の両端を同じ車両に入れ、集荷を先にします。selectiveでは初期実施件数がK以下である必要があります。条件に違反する計画を何でも修復する入口ではありません。有効な入力解が探索後の解より目的上良ければ、入力解を保持します。

時間制限付きの探索であり、最適解や実行可能解の発見は保証しません。全件・件数固定でも報酬合計は返しますが、選択目的には使いません。各関数の資源制約を、単なる表示項目と混同しないでください。

### 2.5 `RoutingSolution<Vertex, Calc, Reward=long long>`

| メンバー | 型 | 初期値 | 意味 |
|---|---|---|---|
| `routes` | `std::vector<std::vector<Vertex>>` | 空 | 車両順の作業地点列。出発・終了拠点は含めない。空車両は空配列 |
| `unserved_items` | `std::vector<int>` | 空 | 未完了の入力仕事添字。PDPでは依頼単位。頂点IDではない |
| `travel_cost` | Calc | 0 | 名前にかかわらず、移動費用と作業費用の合計 |
| `collected_reward` | Reward | 0 | 完了仕事の報酬合計 |
| `feasible` | bool | false | trueは必要件数・必須仕事・資源制約を満たす。falseは部分解などの可能性があり、実行計画として採用しない |

### 2.6 距離共有用`RoutingContext<Vertex, Calc>`と生成関数

| 公開API | 引数・既定値 | 戻り値・意味・条件 |
|---|---|---|
| `RoutingContext<Vertex, Calc>()` | 引数なし | 登録0件、symmetric()がtrueの空Context。使用地点がある問題には、生成関数で距離を用意する |
| `make_routing_context<Calc>(std::span<const Vertex> vertices, const F& dist, bool symmetric=true)` | verticesとdistは必須。Calc省略時の規則は2.1と同じ | `RoutingContext<Vertex, Calc>`を値で返す。IDの重複は除いて登録する |
| `make_routing_context<Calc>(const std::vector<Vertex, Allocator>& vertices, const F& dist, bool symmetric=true)` | vector版。任意のAllocator。その他の条件はspan版と同じ | 同じContext型を返す |
| `int size() const` | 引数なし | 一意な登録頂点数 |
| `int index_of(Vertex vertex) const` | 元ID。省略不可 | 内部番号。未登録なら−1。内部番号は元IDの昇順で付く |
| `Vertex vertex(int index) const` | 0以上size未満の内部番号 | 元ID。範囲外の番号を渡さない |
| `Calc distance(int from, int to) const` | 両方とも有効な内部番号 | 保存済み距離。元IDを渡す関数ではない |
| `std::span<const int> candidates(int index) const` | 有効な内部番号 | 最大16件の近傍候補の内部番号。経路や最適解ではない。参照中はContextを破棄・再代入・ムーブしない |
| `bool symmetric() const` | 引数なし | 構築時に指定した対称性 |

生成時に**全仕事地点と全車両のstart・finish**を含めます。登録元配列・callbackがなくなっても、保存したIDと距離は残ります。距離参照が定数時間なら、登録頂点数Vに対して準備時間・主なメモリは概ね$O(V^2)$です。参照操作はindex_ofが$O(\log V)$、他は$O(1)$です。Contextは通常のC++のコピー・ムーブが可能ですが、コピーは距離表も複製するので使い回すときはconst参照で渡します。

頂点追加・辺費用更新のメソッドはありません。登録集合や費用を変えたら再構築します。同じ登録集合と費用のままなら、仕事の選び方、数量、報酬、容量、上限、seedなどを変えてsolveできます。初期経路も使うなら、新条件での資源制約を満たすことを確認します。探索途中の状態を保持するContextではありません。

### 2.7 `RoutingStats`の全メンバー

| メンバー | 型 | 初期値 | 意味 |
|---|---|---|---|
| `move_evaluations` | uint64_t | 0 | 挿入・移動候補などの評価数 |
| `accepted_moves` | uint64_t | 0 | 単一訪問の局所探索で採用した変更数 |
| `local_search_runs` | uint64_t | 0 | 単一訪問の局所探索の実行数 |
| `lns_rounds` | uint64_t | 0 | 仕事を一度外して入れ直す探索の反復数 |
| `best_updates` | uint64_t | 0 | その反復探索で最良状態を更新した回数 |
| `destroyed_items` | uint64_t | 0 | 一度外した仕事数の累計 |
| `repaired_items` | uint64_t | 0 | 必須集合を修復する際に入れた仕事数の累計。任意仕事の追加すべてを数える値ではない |
| `distance_build_ms` | double | 0 | 距離行列の準備時間ms。Context版solveでは0 |
| `candidate_build_ms` | double | 0 | 近傍候補の準備時間ms。Context版solveでは0 |
| `construction_ms` | double | 0 | 初期構築の時間ms |
| `local_search_ms` | double | 0 | 初回の独立した局所探索の時間ms |
| `lns_ms` | double | 0 | 除去・再挿入の探索段階に使った時間ms |
| `total_ms` | double | 0 | 呼び出し全体で計測した時間ms。Context版でも入力変換を含む |

表のuint64_tは`std::uint64_t`です。PDPでは独立局所探索を使わないためaccepted_moves・local_search_runs・local_search_msは0です。statsを再利用しても累積にはならず、各呼び出しでリセットされます。

### 2.8 デバッグ出力と未対応条件

| 出力演算子 | 表示・戻り値 |
|---|---|
| `std::ostream& operator<<(std::ostream& os, const RoutingSolution<Vertex, Calc, Reward>& result)` | 成否・費用・報酬・車両別経路・未実施仕事。経路数、各経路の頂点数、未実施件数はそれぞれ先頭20まで。省略件数も表示する |
| `std::ostream& operator<<(std::ostream& os, const RoutingStats& stats)` | 全統計を名前付きで1行表示する |

どちらもosを返し、連結して出力できます。結果を変更せず、改行・flush・書式フラグの変更は行いません。128bit整数の費用・報酬は十進数で全桁表示し、8bit整数IDも文字ではなく数値で表示します。通常の数値の書式はストリーム設定に従います。正式出力で全要素が必要な場合は、各例のprint_resultのように自分で走査します。

時間窓、複数種類の容量、車両別の距離、固定担当、作業の分割、車両固定費、使用台数の最小化、最も遅い車両の時間最小化、途中状態に依存する距離は直接指定できません。需要に負値を混ぜて集荷配達を代用することも避けます。assertを無効にしても入力条件が緩和されるわけではありません。

## 3. ユースケース

### 3.1 コピーして実行する手順

各例はmain関数を含む完全なプログラムです。**1例ずつ**コード全体を`sample.cpp`へ、入力例を`input.txt`へ保存し、同じフォルダに`routing_solver_v07.hpp`を置きます。他の例のコードを先にコピーする必要はありません。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined sample.cpp -o sample
./sample < input.txt
```

例は記載した入力条件を満たすデータを対象とします。入力行の空白と改行は同じ区切りとして扱います。各コードの先頭にも用途・入力形式・注意事項を記載しているので、単独で持ち出してカスタマイズできます。

掲載する出力例は、実際の実行から得た標準出力です。時間制限付き探索では、環境や打ち切り時刻により同じ入力でも順序が変わる場合があります。統計を出す例では標準エラーに別途表示されますが、実測時間は環境に依存するため掲載していません。仕事・車両・地点の添字は0始まりです。

### 3.2 例の一覧

| 例 | 解く問題・利用場面 | 主に使うAPI |
|---|---|---|
| R01 | 全顧客へ荷物を配る | `solve_cvrp` |
| R02 | 異なる拠点・終了地点を持つ車両を使う | `make_routing_context`、`solve_cvrp` |
| R03 | 時間内に価値の高い地点を巡回する | `solve_orienteering`、`make_routing_context`、`improve_orienteering` |
| R04 | 積める荷物の中から収益の高い配送を選ぶ | `solve_prize_collecting_vrp`、`make_routing_context`、`improve_prize_collecting_vrp` |
| R05 | 必須地点を含めてちょうどK地点を選ぶ | `solve_selective_vrp`、`make_routing_context`、`improve_selective_vrp` |
| R06 | 集荷してから届ける依頼をすべて処理する | `solve_pdvrp`、`make_routing_context`、`improve_pdvrp` |
| R07 | 時間と積載量を守って集荷配達を受注する | `solve_prize_collecting_pdvrp`、`make_routing_context`、`improve_prize_collecting_pdvrp` |
| R08 | ちょうどK件の集荷配達依頼を選ぶ | `solve_selective_pdvrp`、`make_routing_context`、`improve_selective_pdvrp` |
| R09 | 一方通行・方向依存の費用を扱う | `solve_cvrp` |
| R10 | 道路グラフで訪問順と実際の移動列を求める | `make_routing_context`、`solve_cvrp` |
| R11 | 距離を共有しながら複数seedを試す | `make_routing_context`、`solve_cvrp`、`improve_cvrp` |
| R12 | 新しい仕事が増えたターンで再計画する | `make_routing_context`、`solve_cvrp`、`improve_cvrp` |
| R13 | 同じ候補から全件配送・予算巡回・収益配送・件数固定を比較する | `make_routing_context`、`solve_cvrp`、`solve_orienteering`、`solve_prize_collecting_vrp`、`solve_selective_vrp` |
| R14 | 集荷配達の受注方針を比較する | `make_routing_context`、`solve_pdvrp`、`solve_prize_collecting_pdvrp`、`solve_selective_pdvrp` |
| R15 | 小数の積載量と作業時間を扱う | `solve_pdvrp` |
| R16 | 64bitを超える整数費用を正確に扱う | `solve_cvrp` |
| R17 | 非連続IDの距離Contextを調べる | `make_routing_context`、`Contextの全参照API` |
| R18 | 複数の探索で全体の終了時刻を共有する | `make_routing_context`、`solve_cvrp` |

### R01 全顧客へ荷物を配る

容量4の車両2台で、需要2ずつの3顧客へ全件配送します。各車両は拠点0に戻り、移動費用の合計を小さくします。車両を全部使う義務や台数最小化の目的はありません。

solve_cvrpではmandatoryを0にしても全件必須です。feasible=falseは、時間内に実行可能解を見つけられなかったことを示し、解が存在しない証明ではありません。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。

**実行コード**

```cpp
// R01: 全顧客へ荷物を配る
//
// 解く問題:
// 容量4の車両2台で、需要2ずつの3顧客へ全件配送します。
// 各車両は拠点0に戻り、移動費用の合計を小さくします。
// 車両を全部使う義務や台数最小化の目的はありません。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
//
// 注意:
// solve_cvrpではmandatoryを0にしても全件必須です。
// feasible=falseは、時間内に実行可能解を見つけられなかったことを示し、
// 解が存在しない証明ではありません。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // 全件必須・容量制約・費用最小化。
    const auto result =
        routing::solve_cvrp(task_view, vehicle_view, dist, param, &stats);

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4 3 2
0 1 2 3
1 0 1 2
2 1 0 1
3 2 1 0
1 2 0 0 1
2 2 0 0 1
3 2 0 0 1
0 0 4 100
0 0 4 100
```

**出力例**

```text
feasible 1
cost 8
reward 0
vehicles 2
route 0 2 2 3
route 1 1 1
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R02 異なる拠点・終了地点を持つ車両を使う

車両0は地点0から地点4へ、車両1は地点4から地点0へ向かい、途中で全3仕事を分担します。各車両の容量、終了地点、経路上限を別々に設定できます。

何も担当しない車両は空経路となり、startとfinishが異なっていても費用0です。空車両にも移動を義務付けるモデルではありません。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。

**実行コード**

```cpp
// R02: 異なる拠点・終了地点を持つ車両を使う
//
// 解く問題:
// 車両0は地点0から地点4へ、車両1は地点4から地点0へ向かい、途中で全3仕事を分担します。
// 各車両の容量、終了地点、経路上限を別々に設定できます。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
//
// 注意:
// 何も担当しない車両は空経路となり、startとfinishが異なっていても費用0です。
// 空車両にも移動を義務付けるモデルではありません。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    std::vector<int> vertices(vertex_count);
    std::iota(vertices.begin(), vertices.end(), 0);

    // 仕事と拠点を含む全IDの距離を保存する。
    const auto context = routing::make_routing_context(vertices, dist, param.symmetric);

    // 保存済み距離を使い、全顧客を割り当てる。
    const auto result =
        routing::solve_cvrp(context, task_view, vehicle_view, param, &stats);

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 3 2
0 1 3 5 6
1 0 2 4 5
3 2 0 2 3
5 4 2 0 1
6 5 3 1 0
1 1 0 0 1
2 1 0 0 1
3 1 0 0 1
0 4 2 20
4 0 2 20
```

**出力例**

```text
feasible 1
cost 12
reward 0
vehicles 2
route 0 2 1 2
route 1 1 3
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R03 時間内に価値の高い地点を巡回する

往復費用8以内で、必須地点1を訪ねながら観測報酬を最大化します。需要100・容量0という入力でも、この入口は容量を使わず、経路上限と必須条件を守ります。 この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。

報酬が同じなら費用が小さい解を選びます。需要と容量を制約に含めたい場合はprize_collecting_vrpを使います。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。

**実行コード**

```cpp
// R03: 時間内に価値の高い地点を巡回する
//
// 解く問題:
// 往復費用8以内で、必須地点1を訪ねながら観測報酬を最大化します。
// 需要100・容量0という入力でも、この入口は容量を使わず、経路上限と必須条件を守ります。
// この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
//
// 注意:
// 報酬が同じなら費用が小さい解を選びます。
// 需要と容量を制約に含めたい場合はprize_collecting_vrpを使います。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // 距離関数から初期計画を作る。
    auto result =
        routing::solve_orienteering(task_view, vehicle_view, dist, param, &stats);

    // 初期計画が有効な場合だけ、追加予算で改善する。
    if (result.feasible) {
        std::vector<int> vertices(vertex_count);
        std::iota(vertices.begin(), vertices.end(), 0);
        // 仕事と拠点を含む全IDの距離を保存する。
        const auto context =
            routing::make_routing_context(vertices, dist, param.symmetric);
        // 拠点を含まない作業列を車両順で渡す。
        const std::span<const std::vector<int>> initial(result.routes);
        // 同じ条件で既存計画を改善する。
        auto refined = routing::improve_orienteering(
            context,
            task_view,
            vehicle_view,
            initial,
            param,
            &stats
        );
        // improve完了後に代入する。
        // 呼び出し中は元のroutesを破棄しない。
        result = std::move(refined);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 4 1
0 1 2 4 8
1 0 1 3 7
2 1 0 2 6
4 3 2 0 4
8 7 6 4 0
1 100 4 0 1
2 100 7 0 0
3 100 6 0 0
4 100 100 0 0
0 0 0 8
```

**出力例**

```text
feasible 1
cost 8
reward 17
vehicles 1
route 0 3 3 2 1
unserved 1 3
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R04 積める荷物の中から収益の高い配送を選ぶ

容量4と往復費用20の両方を守り、受注した配送の報酬を最大化します。例では全仕事が任意です。 この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。

報酬から費用を引いた利益の最大化ではありません。まず報酬を優先し、同報酬の場合だけ費用で比較します。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。

**実行コード**

```cpp
// R04: 積める荷物の中から収益の高い配送を選ぶ
//
// 解く問題:
// 容量4と往復費用20の両方を守り、受注した配送の報酬を最大化します。
// 例では全仕事が任意です。
// この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
//
// 注意:
// 報酬から費用を引いた利益の最大化ではありません。
// まず報酬を優先し、同報酬の場合だけ費用で比較します。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // 距離関数から初期計画を作る。
    auto result = routing::solve_prize_collecting_vrp(
        task_view,
        vehicle_view,
        dist,
        param,
        &stats
    );

    // 初期計画が有効な場合だけ、追加予算で改善する。
    if (result.feasible) {
        std::vector<int> vertices(vertex_count);
        std::iota(vertices.begin(), vertices.end(), 0);
        // 仕事と拠点を含む全IDの距離を保存する。
        const auto context =
            routing::make_routing_context(vertices, dist, param.symmetric);
        // 拠点を含まない作業列を車両順で渡す。
        const std::span<const std::vector<int>> initial(result.routes);
        // 同じ条件で既存計画を改善する。
        auto refined = routing::improve_prize_collecting_vrp(
            context,
            task_view,
            vehicle_view,
            initial,
            param,
            &stats
        );
        // improve完了後に代入する。
        // 呼び出し中は元のroutesを破棄しない。
        result = std::move(refined);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 4 1
0 1 2 4 8
1 0 1 3 7
2 1 0 2 6
4 3 2 0 4
8 7 6 4 0
1 2 4 0 0
2 3 7 0 0
3 2 8 0 0
4 1 5 0 0
0 0 4 20
```

**出力例**

```text
feasible 1
cost 8
reward 12
vehicles 1
route 0 2 3 1
unserved 2 1 3
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R05 必須地点を含めてちょうどK地点を選ぶ

必須地点1を含むちょうど2地点を選び、往復費用を最小にします。報酬値は選択基準になりません。 この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。

Kは上限ではなく、ちょうど実施する件数です。必須仕事数以上かつ全仕事数以下にします。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。 最後にKを1つ入力します。

**実行コード**

```cpp
// R05: 必須地点を含めてちょうどK地点を選ぶ
//
// 解く問題:
// 必須地点1を含むちょうど2地点を選び、往復費用を最小にします。
// 報酬値は選択基準になりません。
// この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
// 最後にKを1つ入力します。
//
// 注意:
// Kは上限ではなく、ちょうど実施する件数です。
// 必須仕事数以上かつ全仕事数以下にします。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // ちょうど選ぶ仕事数。
    // PDPでは依頼数。
    int selected_count;
    std::cin >> selected_count;

    // 距離関数から初期計画を作る。
    auto result = routing::solve_selective_vrp(
        task_view,
        vehicle_view,
        dist,
        selected_count,
        param,
        &stats
    );

    // 初期計画が有効な場合だけ、追加予算で改善する。
    if (result.feasible) {
        std::vector<int> vertices(vertex_count);
        std::iota(vertices.begin(), vertices.end(), 0);
        // 仕事と拠点を含む全IDの距離を保存する。
        const auto context =
            routing::make_routing_context(vertices, dist, param.symmetric);
        // 拠点を含まない作業列を車両順で渡す。
        const std::span<const std::vector<int>> initial(result.routes);
        // 同じ条件で既存計画を改善する。
        auto refined = routing::improve_selective_vrp(
            context,
            task_view,
            vehicle_view,
            initial,
            selected_count,
            param,
            &stats
        );
        // improve完了後に代入する。
        // 呼び出し中は元のroutesを破棄しない。
        result = std::move(refined);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 4 1
0 1 2 4 8
1 0 1 3 7
2 1 0 2 6
4 3 2 0 4
8 7 6 4 0
1 1 0 0 1
2 1 0 0 0
3 1 0 0 0
4 1 0 0 0
0 0 3 100
2
```

**出力例**

```text
feasible 1
cost 4
reward 0
vehicles 1
route 0 2 2 1
unserved 2 2 3
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R06 集荷してから届ける依頼をすべて処理する

地点1で2個集荷して地点2へ届け、地点3で2個集荷して地点4へ届けます。容量2なので同時に両依頼を積めませんが、配達後に次の荷物を集荷できます。 この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。

同じ依頼の集荷と配達は同一車両で、集荷を先に行います。全数量の合計ではなく、途中の積載量を検査します。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 集荷配達の仕事行は `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。全依頼の両端点IDは互いに異なる値にします。

**実行コード**

```cpp
// R06: 集荷してから届ける依頼をすべて処理する
//
// 解く問題:
// 地点1で2個集荷して地点2へ届け、地点3で2個集荷して地点4へ届けます。
// 容量2なので同時に両依頼を積めませんが、配達後に次の荷物を集荷できます。
// この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 集荷配達の仕事行は
// `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。
// 全依頼の両端点IDは互いに異なる値にします。
//
// 注意:
// 同じ依頼の集荷と配達は同一車両で、集荷を先に行います。
// 全数量の合計ではなく、途中の積載量を検査します。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::PickupDeliveryTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.pickup >> t.delivery >> t.quantity >> t.reward
            >> t.pickup_service_cost >> t.delivery_service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // 距離関数から初期計画を作る。
    auto result = routing::solve_pdvrp(task_view, vehicle_view, dist, param, &stats);

    // 初期計画が有効な場合だけ、追加予算で改善する。
    if (result.feasible) {
        std::vector<int> vertices(vertex_count);
        std::iota(vertices.begin(), vertices.end(), 0);
        // 仕事と拠点を含む全IDの距離を保存する。
        const auto context =
            routing::make_routing_context(vertices, dist, param.symmetric);
        // 拠点を含まない作業列を車両順で渡す。
        const std::span<const std::vector<int>> initial(result.routes);
        // 同じ条件で既存計画を改善する。
        auto refined = routing::improve_pdvrp(
            context,
            task_view,
            vehicle_view,
            initial,
            param,
            &stats
        );
        // improve完了後に代入する。
        // 呼び出し中は元のroutesを破棄しない。
        result = std::move(refined);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 2 1
0 1 2 3 4
1 0 1 2 3
2 1 0 1 2
3 2 1 0 1
4 3 2 1 0
1 2 2 0 0 0 1
3 4 2 0 0 0 1
0 0 2 100
```

**出力例**

```text
feasible 1
cost 8
reward 0
vehicles 1
route 0 4 1 2 3 4
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R07 時間と積載量を守って集荷配達を受注する

往復費用8以内で必須の依頼0を完了し、追加で受ける依頼の報酬を最大化します。遠方の高報酬依頼でも、費用上限を超えるなら選べません。 この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。

報酬は集荷地点を訪ねただけでは得られず、対応する配達まで完了して初めて加算されます。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 集荷配達の仕事行は `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。全依頼の両端点IDは互いに異なる値にします。

**実行コード**

```cpp
// R07: 時間と積載量を守って集荷配達を受注する
//
// 解く問題:
// 往復費用8以内で必須の依頼0を完了し、追加で受ける依頼の報酬を最大化します。
// 遠方の高報酬依頼でも、費用上限を超えるなら選べません。
// この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 集荷配達の仕事行は
// `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。
// 全依頼の両端点IDは互いに異なる値にします。
//
// 注意:
// 報酬は集荷地点を訪ねただけでは得られず、対応する配達まで完了して初めて加算されます。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::PickupDeliveryTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.pickup >> t.delivery >> t.quantity >> t.reward
            >> t.pickup_service_cost >> t.delivery_service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // 距離関数から初期計画を作る。
    auto result = routing::solve_prize_collecting_pdvrp(
        task_view,
        vehicle_view,
        dist,
        param,
        &stats
    );

    // 初期計画が有効な場合だけ、追加予算で改善する。
    if (result.feasible) {
        std::vector<int> vertices(vertex_count);
        std::iota(vertices.begin(), vertices.end(), 0);
        // 仕事と拠点を含む全IDの距離を保存する。
        const auto context =
            routing::make_routing_context(vertices, dist, param.symmetric);
        // 拠点を含まない作業列を車両順で渡す。
        const std::span<const std::vector<int>> initial(result.routes);
        // 同じ条件で既存計画を改善する。
        auto refined = routing::improve_prize_collecting_pdvrp(
            context,
            task_view,
            vehicle_view,
            initial,
            param,
            &stats
        );
        // improve完了後に代入する。
        // 呼び出し中は元のroutesを破棄しない。
        result = std::move(refined);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
7 3 1
0 1 2 3 4 5 6
1 0 1 2 3 4 5
2 1 0 1 2 3 4
3 2 1 0 1 2 3
4 3 2 1 0 1 2
5 4 3 2 1 0 1
6 5 4 3 2 1 0
1 2 1 6 0 0 1
3 4 2 9 0 0 0
5 6 1 20 0 0 0
0 0 2 8
```

**出力例**

```text
feasible 1
cost 8
reward 15
vehicles 1
route 0 4 1 2 3 4
unserved 1 2
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R08 ちょうどK件の集荷配達依頼を選ぶ

必須の依頼0を含むちょうど2依頼を選び、総移動費用を最小にします。2依頼なら作業地点は4地点です。 この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。

Kは訪問頂点数ではなく依頼数です。集荷だけ、配達だけを選ぶことはできません。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 集荷配達の仕事行は `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。全依頼の両端点IDは互いに異なる値にします。 最後にKを1つ入力します。

**実行コード**

```cpp
// R08: ちょうどK件の集荷配達依頼を選ぶ
//
// 解く問題:
// 必須の依頼0を含むちょうど2依頼を選び、総移動費用を最小にします。
// 2依頼なら作業地点は4地点です。
// この例は、最初に計画を作り、有効なら同じ条件で追加探索する使い方も示します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 集荷配達の仕事行は
// `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。
// 全依頼の両端点IDは互いに異なる値にします。
// 最後にKを1つ入力します。
//
// 注意:
// Kは訪問頂点数ではなく依頼数です。
// 集荷だけ、配達だけを選ぶことはできません。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::PickupDeliveryTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.pickup >> t.delivery >> t.quantity >> t.reward
            >> t.pickup_service_cost >> t.delivery_service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // ちょうど選ぶ仕事数。
    // PDPでは依頼数。
    int selected_count;
    std::cin >> selected_count;

    // 距離関数から初期計画を作る。
    auto result = routing::solve_selective_pdvrp(
        task_view,
        vehicle_view,
        dist,
        selected_count,
        param,
        &stats
    );

    // 初期計画が有効な場合だけ、追加予算で改善する。
    if (result.feasible) {
        std::vector<int> vertices(vertex_count);
        std::iota(vertices.begin(), vertices.end(), 0);
        // 仕事と拠点を含む全IDの距離を保存する。
        const auto context =
            routing::make_routing_context(vertices, dist, param.symmetric);
        // 拠点を含まない作業列を車両順で渡す。
        const std::span<const std::vector<int>> initial(result.routes);
        // 同じ条件で既存計画を改善する。
        auto refined = routing::improve_selective_pdvrp(
            context,
            task_view,
            vehicle_view,
            initial,
            selected_count,
            param,
            &stats
        );
        // improve完了後に代入する。
        // 呼び出し中は元のroutesを破棄しない。
        result = std::move(refined);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
7 3 1
0 1 2 3 4 5 6
1 0 1 2 3 4 5
2 1 0 1 2 3 4
3 2 1 0 1 2 3
4 3 2 1 0 1 2
5 4 3 2 1 0 1
6 5 4 3 2 1 0
1 2 1 0 0 0 1
3 4 2 0 0 0 0
5 6 1 0 0 0 0
0 0 2 100
2
```

**出力例**

```text
feasible 1
cost 8
reward 0
vehicles 1
route 0 4 1 2 3 4
unserved 1 2
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R09 一方通行・方向依存の費用を扱う

0→1→2→3→0の向きは安く、逆方向は高い道路で全顧客に配送します。距離行列の行が出発地、列が到着地です。

非対称な距離では必ずsymmetric=falseにします。距離の対称性をライブラリは自動判定しません。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。

**実行コード**

```cpp
// R09: 一方通行・方向依存の費用を扱う
//
// 解く問題:
// 0→1→2→3→0の向きは安く、逆方向は高い道路で全顧客に配送します。
// 距離行列の行が出発地、列が到着地です。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
//
// 注意:
// 非対称な距離では必ずsymmetric=falseにします。
// 距離の対称性をライブラリは自動判定しません。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // from→toとto→fromの費用が違うため必須。
    param.symmetric = false;

    // 有向の費用を使って全顧客を配送する。
    const auto result =
        routing::solve_cvrp(task_view, vehicle_view, dist, param, &stats);

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4 3 1
0 1 8 8
8 0 1 8
8 8 0 1
1 8 8 0
1 1 0 0 1
2 1 0 0 1
3 1 0 0 1
0 0 3 100
```

**出力例**

```text
feasible 1
cost 4
reward 0
vehicles 1
route 0 3 1 2 3
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R10 道路グラフで訪問順と実際の移動列を求める

道路のある頂点間だけを移動できる配送問題です。例側で最短路を計算して顧客間距離を作り、返された訪問順を実際の道路列へ展開します。

道路上で作業地点を通過することと、その仕事を処理することは別に扱います。通過しただけで自動的に仕事が完了する問題には、このモデルをそのまま使えません。例は全頂点が連結した無向・非負の道路を扱います。

**入力形式**

先頭行は `V E N M`。続くE行は無向道路 `a b cost`、N行は `vertex demand reward service_cost mandatory`、M行は `start finish capacity max_cost`。辺費用は非負で、すべての有限最短路は2の60乗未満とします。

**実行コード**

```cpp
// R10: 道路グラフで訪問順と実際の移動列を求める
//
// 解く問題:
// 道路のある頂点間だけを移動できる配送問題です。
// 例側で最短路を計算して顧客間距離を作り、返された訪問順を実際の道路列へ展開します。
//
// 入力:
// 先頭行は `V E N M`。
// 続くE行は無向道路 `a b cost`、N行は `vertex demand reward service_cost mandatory`、
// M行は `start finish capacity max_cost`。
// 辺費用は非負で、すべての有限最短路は2の60乗未満とします。
//
// 注意:
// 道路上で作業地点を通過することと、その仕事を処理することは別に扱います。
// 通過しただけで自動的に仕事が完了する問題には、このモデルをそのまま使えません。
// 例は全頂点が連結した無向・非負の道路を扱います。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// 配送APIと標準ライブラリ。
#include "routing_solver_v07.hpp"

// 数量・報酬・作業費はlong long。
using Task = routing::RouteTask<int>;

// 数量と費用上限はlong long。
using Vehicle = routing::RouteVehicle<int>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, edge_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> edge_count >> task_count >> vehicle_count)) {
        return 0;
    }

    // 例側の到達不能印。
    // 有限の最短路はinf未満とする。
    const long long inf = (1LL << 60);

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count, inf)
    );

    std::vector<std::vector<int>> next(
        vertex_count,
        std::vector<int>(vertex_count, -1)
    );
    for (int i = 0; i < vertex_count; ++i) {
        distance[i][i] = 0;
        next[i][i] = i;
    }

    for (int i = 0; i < edge_count; ++i) {
        int a, b;
        long long w;
        std::cin >> a >> b >> w;
        if (w < distance[a][b]) {
            distance[a][b] = distance[b][a] = w;
            next[a][b] = b;
            next[b][a] = a;
        }
    }

    for (int k = 0; k < vertex_count; ++k) {
        for (int a = 0; a < vertex_count; ++a) {
            for (int b = 0; b < vertex_count; ++b) {
                if (distance[a][k] == inf || distance[k][b] == inf) {
                    continue;
                }
                if (distance[a][k] + distance[k][b] < distance[a][b]) {
                    distance[a][b] = distance[a][k] + distance[k][b];
                    next[a][b] = next[a][k];
                }
            }
        }
    }

    for (const auto& row : distance) {
        for (long long d : row) {
            // ライブラリへ到達不能印を渡さない。
            if (d == inf) {
                std::cout << "UNREACHABLE\n";
                return 0;
            }
        }
    }

    // IDはグラフの頂点番号。
    // 作業IDの重複は不可。
    std::vector<Task> tasks(task_count);

    // 単一訪問の5フィールド。
    for (auto& t : tasks) {
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // 車両ごとに出発・終了頂点と2種類の上限を持つ。
    std::vector<Vehicle> vehicles(vehicle_count);

    // 容量と費用上限は別の制約。
    for (auto& v : vehicles) {
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 最短路費用を静的な距離として渡す。
    const auto dist = [&](int a, int b) {
        return distance[a][b];
    };

    std::vector<int> vertices(vertex_count);
    std::iota(vertices.begin(), vertices.end(), 0);

    // 無向道路なのでsymmetricの既定trueを使う。
    const auto context = routing::make_routing_context(vertices, dist);

    // 探索パラメータを既定値で初期化する。
    routing::RoutingParam param;

    // 前後処理も含めた本番の残り時間とは別に、例の探索時間を設定。
    param.time_limit_ms = 1000;

    // 探索候補評価の上限。
    param.max_move_evaluations = 3000;

    // 今回のsolveの統計出力先。
    routing::RoutingStats stats;

    // 道路上の顧客を全件配送する。
    const auto result = routing::solve_cvrp(
        context,
        std::span<const Task>(tasks),
        std::span<const Vehicle>(vehicles),
        param,
        &stats
    );

    // まず作業地点列を全件出力する。
    print_result(result);

    // 実行可能解だけを実際の道路列へ展開する。
    if (result.feasible) {
        // 各車両の担当仕事を順に処理する。
        for (std::size_t r = 0; r < result.routes.size(); ++r) {
            std::vector<int> walk;
            // 空車両は移動しない仕様なので道路列も空にする。
            if (!result.routes[r].empty()) {
                // 実移動の列には出発拠点を追加する。
                walk.push_back(vehicles[r].start);
                // 作業地点だけを返すので、自分で終了拠点を追加する。
                auto stops = result.routes[r];
                // 最後の作業から終了地点までの移動も必要。
                stops.push_back(vehicles[r].finish);
                for (int target : stops) {
                    while (walk.back() != target) {
                        walk.push_back(next[walk.back()][target]);
                    }
                }
            }
            std::cout << "walk " << r << ' ' << walk.size();
            for (int v : walk) {
                std::cout << ' ' << v;
            }
            std::cout << '\n';
        }
    }

    // デバッグ表示。
    // 正式出力は上の全件出力を使う。
    std::cerr << result << '\n' << stats << '\n';
}
```

**入力例**

```text
5 5 2 1
0 1 2
1 2 2
2 3 2
3 4 2
0 4 20
2 1 0 0 1
4 1 0 0 1
0 0 2 100
```

**出力例**

```text
feasible 1
cost 16
reward 0
vehicles 1
route 0 2 4 2
unserved 0
walk 0 9 0 1 2 3 4 3 2 1 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。 続く `walk 車両番号 頂点数 頂点列` は出発・終了地点と通過点を含む実際の道路列です。非連結入力では `UNREACHABLE` を出して終了します。

### R11 距離を共有しながら複数seedを試す

同じ配送問題に対してseedを3つ試し、実行可能な候補の中で総費用が最小の計画を選びます。距離表は全試行で共有します。

報酬最大化の問題へ転用する場合は、費用だけでなく報酬優先で比較する必要があります。統計は各呼び出しでリセットされるため、この例では最後の改善呼び出しの統計を出します。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。

**実行コード**

```cpp
// R11: 距離を共有しながら複数seedを試す
//
// 解く問題:
// 同じ配送問題に対してseedを3つ試し、実行可能な候補の中で総費用が最小の計画を選びます。
// 距離表は全試行で共有します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
//
// 注意:
// 報酬最大化の問題へ転用する場合は、費用だけでなく報酬優先で比較する必要があります。
// 統計は各呼び出しでリセットされるため、この例では最後の改善呼び出しの統計を出します。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    std::vector<int> vertices(vertex_count);
    std::iota(vertices.begin(), vertices.end(), 0);

    // 仕事と拠点を含む全IDの距離を保存する。
    const auto context = routing::make_routing_context(vertices, dist, param.symmetric);

    // feasible=falseの空結果から最良解の保存を始める。
    routing::RoutingSolution<int, long long> result;

    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        // 同じ問題を異なる乱数seedで解く。
        param.seed = seed;
        // Contextを作り直さず再試行する。
        auto candidate =
            routing::solve_cvrp(context, task_view, vehicle_view, param, &stats);
        // CVRPは費用最小化。
        if (candidate.feasible
            && (!result.feasible || candidate.travel_cost < result.travel_cost)) {
            result = std::move(candidate);
        }
    }

    // 有効な最良解だけを追加探索へ渡す。
    if (result.feasible) {
        // 拠点を含まない、車両順の作業列。
        const std::span<const std::vector<int>> initial(result.routes);
        // 既存解を基に探索する。
        auto improved = routing::improve_cvrp(
            context,
            task_view,
            vehicle_view,
            initial,
            param,
            &stats
        );
        // 参照先を呼び出し後に置き換える。
        result = std::move(improved);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4 3 2
0 1 2 3
1 0 1 2
2 1 0 1
3 2 1 0
1 2 0 0 1
2 2 0 0 1
3 2 0 0 1
0 0 4 100
0 0 4 100
```

**出力例**

```text
feasible 1
cost 8
reward 0
vehicles 2
route 0 2 2 3
route 1 1 1
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R12 新しい仕事が増えたターンで再計画する

仕事1件の配送計画を作った後、別の地点の仕事が追加されます。既存の担当・訪問順を出発点として、追加仕事を含む計画へ改善します。

この例では未実行の計画を再利用し、車両位置・距離・資源上限は変えません。実行済みの仕事を消す場合は経路からも消し、容量や費用上限を厳しくした場合は初期経路が新条件を満たすことを別途確認します。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。 最後に追加仕事数Eと、そのE件の仕事行を入力します。距離行列には追加予定地点も含めておきます。

**実行コード**

```cpp
// R12: 新しい仕事が増えたターンで再計画する
//
// 解く問題:
// 仕事1件の配送計画を作った後、別の地点の仕事が追加されます。
// 既存の担当・訪問順を出発点として、追加仕事を含む計画へ改善します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
// 最後に追加仕事数Eと、そのE件の仕事行を入力します。
// 距離行列には追加予定地点も含めておきます。
//
// 注意:
// この例では未実行の計画を再利用し、車両位置・距離・資源上限は変えません。
// 実行済みの仕事を消す場合は経路からも消し、
// 容量や費用上限を厳しくした場合は初期経路が新条件を満たすことを別途確認します。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    std::vector<int> used;

    // 現時点の全仕事IDを登録する。
    for (const auto& t : tasks) {
        used.push_back(t.vertex);
    }

    // 拠点も登録が必要。
    for (const auto& v : vehicles) {
        used.push_back(v.start);
        used.push_back(v.finish);
    }

    // 現時点で使用する地点の距離を準備する。
    auto context = routing::make_routing_context(used, dist);

    // 最初の計画を作る。
    auto result = routing::solve_cvrp(context, task_view, vehicle_view, param, &stats);

    int extra;
    std::cin >> extra;
    for (int i = 0; i < extra; ++i) {
        // 新しい単一訪問の仕事。
        // 既存仕事と異なる頂点IDを使う。
        Task t;
        // 追加仕事の5項目。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
        // vectorを変更したので、既存のtask_viewは以後使わない。
        tasks.push_back(t);
        // 新規地点をContextへ登録するため追加する。
        used.push_back(t.vertex);
    }

    // 変更後の配列からspanを作り直す。
    const std::span<const Task> updated_tasks(tasks);

    // 頂点追加APIはないのでContextを再構築する。
    context = routing::make_routing_context(std::span<const int>(used), dist);

    // 追加前の有効な経路は、追加後も容量・費用上限を守る部分解として使える。
    if (result.feasible) {
        // 未追加の仕事が欠けていても、制約内の部分解は渡せる。
        const std::span<const std::vector<int>> initial(result.routes);
        // 新しい必須仕事も補って再計画する。
        auto improved = routing::improve_cvrp(
            context,
            updated_tasks,
            vehicle_view,
            initial,
            param,
            &stats
        );
        // 元の経路配列は呼び出しが終わってから変更する。
        result = std::move(improved);
    } else {
        // 有効な初期計画がない場合は新規構築する。
        result =
            routing::solve_cvrp(context, updated_tasks, vehicle_view, param, &stats);
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4 1 1
0 1 2 3
1 0 1 2
2 1 0 1
3 2 1 0
1 1 0 0 1
0 0 3 100
2
2 1 0 0 1
3 1 0 0 1
```

**出力例**

```text
feasible 1
cost 6
reward 0
vehicles 1
route 0 3 3 2 1
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R13 同じ候補から全件配送・予算巡回・収益配送・件数固定を比較する

一度作った距離表を共有し、同じ仕事候補について複数の運用方針の結果を並べます。目的が異なるため、費用の大小だけで一律に優劣を決める例ではありません。

全件処理の入口はmandatoryにかかわらず全件を要求します。報酬付き・件数固定ではmandatory=1を守ります。Orienteeringだけは数量と容量を無視します。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。 最後に件数固定案のKを入力します。

**実行コード**

```cpp
// R13: 同じ候補から全件配送・予算巡回・収益配送・件数固定を比較する
//
// 解く問題:
// 一度作った距離表を共有し、同じ仕事候補について複数の運用方針の結果を並べます。
// 目的が異なるため、費用の大小だけで一律に優劣を決める例ではありません。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
// 最後に件数固定案のKを入力します。
//
// 注意:
// 全件処理の入口はmandatoryにかかわらず全件を要求します。
// 報酬付き・件数固定ではmandatory=1を守ります。
// Orienteeringだけは数量と容量を無視します。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    std::vector<int> vertices(vertex_count);
    std::iota(vertices.begin(), vertices.end(), 0);

    // 仕事と拠点を含む全IDの距離を保存する。
    const auto context = routing::make_routing_context(vertices, dist, param.symmetric);

    // 件数固定案にだけ使うK。
    int selected_count;
    std::cin >> selected_count;
    std::cout << "policy cvrp\n";

    // 同じ距離表を、この入口の目的・制約で解く。
    const auto cvrp =
        routing::solve_cvrp(context, task_view, vehicle_view, param, &stats);

    // 各案のfeasible・報酬・費用・経路を別々に確認する。
    print_result(cvrp);
    std::cout << "policy orienteering\n";

    // 同じ距離表を、この入口の目的・制約で解く。
    const auto orienteering =
        routing::solve_orienteering(context, task_view, vehicle_view, param, &stats);

    // 各案のfeasible・報酬・費用・経路を別々に確認する。
    print_result(orienteering);
    std::cout << "policy prize_collecting_vrp\n";

    // 同じ距離表を、この入口の目的・制約で解く。
    const auto prize_collecting_vrp = routing::solve_prize_collecting_vrp(
        context,
        task_view,
        vehicle_view,
        param,
        &stats
    );

    // 各案のfeasible・報酬・費用・経路を別々に確認する。
    print_result(prize_collecting_vrp);
    std::cout << "policy selective_vrp\n";

    // 同じ距離表を、この入口の目的・制約で解く。
    const auto selective_vrp = routing::solve_selective_vrp(
        context,
        task_view,
        vehicle_view,
        selected_count,
        param,
        &stats
    );

    // 各案のfeasible・報酬・費用・経路を別々に確認する。
    print_result(selective_vrp);

    // 最後の方針を解いた際の統計。
    // 累積値ではない。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4 3 1
0 1 2 4
1 0 1 3
2 1 0 2
4 3 2 0
1 2 4 0 0
2 2 8 0 0
3 2 9 0 0
0 0 4 100
2
```

**出力例**

```text
policy cvrp
feasible 0
cost 4
reward 12
vehicles 1
route 0 2 2 1
unserved 1 2
policy orienteering
feasible 1
cost 8
reward 21
vehicles 1
route 0 3 3 2 1
unserved 0
policy prize_collecting_vrp
feasible 1
cost 4
reward 12
vehicles 1
route 0 2 2 1
unserved 1 2
policy selective_vrp
feasible 1
cost 4
reward 12
vehicles 1
route 0 2 2 1
unserved 1 2
```

各 `policy 名前` の直後に、その方針の結果を表示します。標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R14 集荷配達の受注方針を比較する

一度作った距離表を共有し、同じ仕事候補について複数の運用方針の結果を並べます。目的が異なるため、費用の大小だけで一律に優劣を決める例ではありません。

全件処理の入口はmandatoryにかかわらず全件を要求します。報酬付き・件数固定ではmandatory=1を守ります。Orienteeringだけは数量と容量を無視します。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 集荷配達の仕事行は `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。全依頼の両端点IDは互いに異なる値にします。 最後に件数固定案のKを入力します。

**実行コード**

```cpp
// R14: 集荷配達の受注方針を比較する
//
// 解く問題:
// 一度作った距離表を共有し、同じ仕事候補について複数の運用方針の結果を並べます。
// 目的が異なるため、費用の大小だけで一律に優劣を決める例ではありません。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 集荷配達の仕事行は
// `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。
// 全依頼の両端点IDは互いに異なる値にします。
// 最後に件数固定案のKを入力します。
//
// 注意:
// 全件処理の入口はmandatoryにかかわらず全件を要求します。
// 報酬付き・件数固定ではmandatory=1を守ります。
// Orienteeringだけは数量と容量を無視します。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::PickupDeliveryTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.pickup >> t.delivery >> t.quantity >> t.reward
            >> t.pickup_service_cost >> t.delivery_service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    std::vector<int> vertices(vertex_count);
    std::iota(vertices.begin(), vertices.end(), 0);

    // 仕事と拠点を含む全IDの距離を保存する。
    const auto context = routing::make_routing_context(vertices, dist, param.symmetric);

    // 件数固定案にだけ使うK。
    int selected_count;
    std::cin >> selected_count;
    std::cout << "policy pdvrp\n";

    // 同じ距離表を、この入口の目的・制約で解く。
    const auto pdvrp =
        routing::solve_pdvrp(context, task_view, vehicle_view, param, &stats);

    // 各案のfeasible・報酬・費用・経路を別々に確認する。
    print_result(pdvrp);
    std::cout << "policy prize_collecting_pdvrp\n";

    // 同じ距離表を、この入口の目的・制約で解く。
    const auto prize_collecting_pdvrp = routing::solve_prize_collecting_pdvrp(
        context,
        task_view,
        vehicle_view,
        param,
        &stats
    );

    // 各案のfeasible・報酬・費用・経路を別々に確認する。
    print_result(prize_collecting_pdvrp);
    std::cout << "policy selective_pdvrp\n";

    // 同じ距離表を、この入口の目的・制約で解く。
    const auto selective_pdvrp = routing::solve_selective_pdvrp(
        context,
        task_view,
        vehicle_view,
        selected_count,
        param,
        &stats
    );

    // 各案のfeasible・報酬・費用・経路を別々に確認する。
    print_result(selective_pdvrp);

    // 最後の方針を解いた際の統計。
    // 累積値ではない。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 2 1
0 1 2 3 4
1 0 1 2 3
2 1 0 1 2
3 2 1 0 1
4 3 2 1 0
1 2 1 4 0 0 0
3 4 1 9 0 0 0
0 0 1 100
1
```

**出力例**

```text
policy pdvrp
feasible 1
cost 8
reward 13
vehicles 1
route 0 4 1 2 3 4
unserved 0
policy prize_collecting_pdvrp
feasible 1
cost 8
reward 13
vehicles 1
route 0 4 1 2 3 4
unserved 0
policy selective_pdvrp
feasible 1
cost 4
reward 4
vehicles 1
route 0 2 1 2
unserved 1 1
```

各 `policy 名前` の直後に、その方針の結果を表示します。標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R15 小数の積載量と作業時間を扱う

数量0.25・0.5の2つの集荷配達依頼を、容量0.75の車両で処理します。距離・作業時間・上限も小数で与えます。

小数の容量境界は丸め誤差の影響を受けます。厳密な判定が必要なら、グラムやミリ秒のような最小単位の整数へ換算してください。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 集荷配達の仕事行は `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。全依頼の両端点IDは互いに異なる値にします。 数量・報酬・作業費・距離・費用上限には小数を入力できます。

**実行コード**

```cpp
// R15: 小数の積載量と作業時間を扱う
//
// 解く問題:
// 数量0.25・0.5の2つの集荷配達依頼を、容量0.75の車両で処理します。
// 距離・作業時間・上限も小数で与えます。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 集荷配達の仕事行は
// `pickup delivery quantity reward pickup_service_cost delivery_service_cost mandatory`。
// 全依頼の両端点IDは互いに異なる値にします。
// 数量・報酬・作業費・距離・費用上限には小数を入力できます。
//
// 注意:
// 小数の容量境界は丸め誤差の影響を受けます。
// 厳密な判定が必要なら、グラムやミリ秒のような最小単位の整数へ換算してください。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=double。
using Task = routing::PickupDeliveryTask<int, double, double, double>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, double, double>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<double>> distance(
        vertex_count,
        std::vector<double>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.pickup >> t.delivery >> t.quantity >> t.reward
            >> t.pickup_service_cost >> t.delivery_service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;
    std::cout << std::fixed << std::setprecision(6);

    // 費用計算型もdoubleと明示する。
    const auto result =
        routing::solve_pdvrp<double>(task_view, vehicle_view, dist, param, &stats);

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 2 1
0 0.5 1 1.5 2
0.5 0.0 0.5 1.0 1.5
1 0.5 0 0.5 1
1.5 1.0 0.5 0.0 0.5
2 1.5 1 0.5 0
1 2 0.25 1.5 0.25 0.25 1
3 4 0.5 2.5 0.25 0.25 1
0 0 0.75 20
```

**出力例**

```text
feasible 1
cost 5.000000
reward 4.000000
vehicles 1
route 0 4 1 3 4 2
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R16 64bitを超える整数費用を正確に扱う

入力した整数距離と経路上限を2の70乗倍して全件配送します。合計が64bitを超えても、128bitの範囲内で正確に加減算します。

作業費は0に限定した例です。費用の型だけでなく車両上限の型も拡張します。GCCの__int128_tを使い、費用合計・差分の中間値がこの型に収まるようにします。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。 作業費は0。距離とmax_costは入力後に2の70乗倍されます。

**実行コード**

```cpp
// R16: 64bitを超える整数費用を正確に扱う
//
// 解く問題:
// 入力した整数距離と経路上限を2の70乗倍して全件配送します。
// 合計が64bitを超えても、128bitの範囲内で正確に加減算します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
// 作業費は0。
// 距離とmax_costは入力後に2の70乗倍されます。
//
// 注意:
// 作業費は0に限定した例です。
// 費用の型だけでなく車両上限の型も拡張します。
// GCCの__int128_tを使い、費用合計・差分の中間値がこの型に収まるようにします。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, __int128_t>;

// この例の費用は非負なので、非負の128bit整数を十進数へ変換する。
std::string decimal(__int128_t value) {
    std::string s;
    do {
        s.push_back(static_cast<char>('0' + value % 10));
        value /= 10;
    } while (value != 0);
    std::reverse(s.begin(), s.end());
    return s;
}

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << decimal(result.travel_cost) << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        long long limit;
        std::cin >> v.start >> v.finish >> v.capacity >> limit;
        v.max_cost = static_cast<__int128_t>(limit) * (__int128_t{1} << 70);
    }

    // この例は作業費0に限定し、距離と上限だけを拡大する。
    for (const auto& t : tasks) {
        if (t.service_cost != 0) {
            std::cout << "SERVICE_MUST_BE_ZERO\n";
            return 0;
        }
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return static_cast<__int128_t>(distance[from][to]) * (__int128_t{1} << 70);
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // callbackの戻り型だけでは既定Calcは拡張されないため必ず明示。
    const auto result =
        routing::solve_cvrp<__int128_t>(task_view, vehicle_view, dist, param, &stats);

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
3 2 1
0 1 2
1 0 1
2 1 0
1 1 0 0 1
2 1 0 0 1
0 0 2 100
```

**出力例**

```text
feasible 1
cost 4722366482869645213696
reward 0
vehicles 1
route 0 2 2 1
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。

### R17 非連続IDの距離Contextを調べる

地点IDが10、25、40、90のように飛んでいても、そのまま登録できます。重複を含む登録配列から距離Contextを作り、元IDと内部番号の対応・近隣候補を表示します。

Contextのdistanceとcandidatesへ渡すのは内部番号です。距離callbackに渡されるのは元IDです。返された候補spanはContextの寿命までしか使えません。

**入力形式**

先頭行は登録要素数n、次にn個の非負int型ID。登録配列では重複可能です。

**実行コード**

```cpp
// R17: 非連続IDの距離Contextを調べる
//
// 解く問題:
// 地点IDが10、25、40、90のように飛んでいても、そのまま登録できます。
// 重複を含む登録配列から距離Contextを作り、元IDと内部番号の対応・近隣候補を表示します。
//
// 入力:
// 先頭行は登録要素数n、次にn個の非負int型ID。
// 登録配列では重複可能です。
//
// 注意:
// Contextのdistanceとcandidatesへ渡すのは内部番号です。
// 距離callbackに渡されるのは元IDです。
// 返された候補spanはContextの寿命までしか使えません。

// 距離を共有するRoutingContextのAPI。
#include "routing_solver_v07.hpp"

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<int> ids(n);
    for (int& id : ids) {
        std::cin >> id;
    }

    // 元IDの差を距離とする。
    const auto dist = [](int a, int b) {
        return std::llabs(static_cast<long long>(a) - b);
    };

    // 既定構築は空のContextで、登録頂点数0。
    routing::RoutingContext<int, long long> context;

    // sizeは登録済み頂点数を返す。
    std::cout << "empty " << context.size() << '\n';

    // span版。
    // 登録重複は除かれる。
    context = routing::make_routing_context<long long>(std::span<const int>(ids), dist);

    // 登録後の一意な頂点数。
    std::cout << "size " << context.size() << '\n';

    // 構築時に省略したsymmetricはtrue。
    std::cout << "symmetric " << context.symmetric() << '\n';

    // この例の入力は非負IDなので-1は未登録、戻り値は-1。
    std::cout << "missing " << context.index_of(-1) << '\n';

    // 有効な内部番号だけを走査する。
    for (int i = 0; i < context.size(); ++i) {
        // 内部番号を元IDへ変換する。
        const int id = context.vertex(i);
        // 元IDを内部番号へ逆変換する。
        std::cout << "id " << id << " index " << context.index_of(id) << '\n';
        // 最大16件の候補内部番号を参照する。
        const auto neighbors = context.candidates(i);
        std::cout << "neighbors " << neighbors.size();
        // 元ID:保存距離へ変換して表示する。
        for (int j : neighbors) {
            std::cout << ' ' << context.vertex(j) << ':' << context.distance(i, j);
        }
        std::cout << '\n';
    }
}
```

**入力例**

```text
5
40 10 25 10 90
```

**出力例**

```text
empty 0
size 4
symmetric 1
missing -1
id 10 index 0
neighbors 3 25:15 40:30 90:80
id 25 index 1
neighbors 3 10:15 40:15 90:65
id 40 index 2
neighbors 3 25:15 10:30 90:50
id 90 index 3
neighbors 3 40:50 25:65 10:80
```

空の登録数、登録後の一意な頂点数、対称性、未登録IDの検索結果、各元IDと内部番号、候補の元ID:距離を表示します。候補集合は仕事の訪問順ではありません。

### R18 複数の探索で全体の終了時刻を共有する

配送計画を繰り返し作る処理全体に、標準入力で指定した時間の予算を与えます。距離準備もこの時間へ含め、各探索に同じ絶対終了時刻を渡します。

時間上限は厳密なリアルタイム保証ではありません。実際のコンテストでは入出力や後処理分の余裕を別途引きます。予算0では探索せずfeasible=falseの空結果を返します。

**入力形式**

先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。`mandatory` は0が任意、1が必須です。 最後に全体予算budget_msを0以上の整数で入力します。

**実行コード**

```cpp
// R18: 複数の探索で全体の終了時刻を共有する
//
// 解く問題:
// 配送計画を繰り返し作る処理全体に、標準入力で指定した時間の予算を与えます。
// 距離準備もこの時間へ含め、各探索に同じ絶対終了時刻を渡します。
//
// 入力:
// 先頭行は `V N M`（距離行列の頂点数、仕事数、車両数）。
// 次のV行はV×Vの距離行列、次のN行は仕事、次のM行は `start finish capacity max_cost` です。
// IDは0以上V未満、数量・費用・報酬は非負、作業IDは重複なしとします。
// 単一訪問の仕事行は `vertex demand reward service_cost mandatory`。
// `mandatory` は0が任意、1が必須です。
// 最後に全体予算budget_msを0以上の整数で入力します。
//
// 注意:
// 時間上限は厳密なリアルタイム保証ではありません。
// 実際のコンテストでは入出力や後処理分の余裕を別途引きます。
// 予算0では探索せずfeasible=falseの空結果を返します。
//
// 数値条件:
// 数量・報酬・費用は非負の有限値。
// 総和だけでなく計算途中も各型の範囲内に収める。
//
// 距離条件:
// この例は対称距離を使う。
// 全地点対でdist(a,b)=dist(b,a)となる入力を与える。

// routingの公開APIと標準ライブラリを読み込む。
#include "routing_solver_v07.hpp"

// 頂点ID=int、数量・報酬・作業費=long long。
using Task = routing::RouteTask<int, long long, long long, long long>;

// Taskと頂点型・数量型を揃える。
using Vehicle = routing::RouteVehicle<int, long long, long long>;

// この関数は例側の全件出力処理。
// 結果のテンプレート引数は費用型と報酬型に合わせる。
template <class Cost, class Reward>
void print_result(const routing::RoutingSolution<int, Cost, Reward>& result) {
    // 最初に成否を表示し、0なら実行しない。
    std::cout << "feasible " << result.feasible << '\n';

    // 移動と作業の総費用。
    std::cout << "cost " << result.travel_cost << '\n';

    // 完了した仕事の報酬合計。
    std::cout << "reward " << result.collected_reward << '\n';

    // 車両数と経路数は対応する。
    std::cout << "vehicles " << result.routes.size() << '\n';

    // 車両配列と同じ順に取り出す。
    for (std::size_t r = 0; r < result.routes.size(); ++r) {
        // 作業地点数。
        // 拠点は含まない。
        std::cout << "route " << r << ' ' << result.routes[r].size();
        // 全頂点を出す。
        // デバッグ表示の20件制限を使わない。
        for (int v : result.routes[r]) {
            std::cout << ' ' << v;
        }
        std::cout << '\n';
    }

    // 未完了の仕事数。
    // PDPでは依頼数。
    std::cout << "unserved " << result.unserved_items.size();

    // 頂点IDではなく入力仕事配列の添字。
    for (int i : result.unserved_items) {
        std::cout << ' ' << i;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, task_count, vehicle_count;
    if (!(std::cin >> vertex_count >> task_count >> vehicle_count)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(
        vertex_count,
        std::vector<long long>(vertex_count)
    );
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 仕事は入力順に0始まりの添字を持つ。
    std::vector<Task> tasks(task_count);

    // 全作業地点IDを一意にする。
    // mandatoryの入力は0または1。
    for (auto& t : tasks) {
        // 各フィールドを入力形式の順に設定する。
        std::cin >> t.vertex >> t.demand >> t.reward >> t.service_cost >> t.mandatory;
    }

    // この順序が出力経路の車両番号になる。
    std::vector<Vehicle> vehicles(vehicle_count);

    // start/finishは元の頂点ID、後ろ2値は車両別の上限。
    for (auto& v : vehicles) {
        // 数量・費用と同じ単位を使う。
        std::cin >> v.start >> v.finish >> v.capacity >> v.max_cost;
    }

    // 実行中は固定の移動費用を返す。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // APIが要求する読み取り専用span。
    // 配列自体は変更されない。
    const std::span<const Task> task_view(tasks);

    // 仕事と同様に読み取り専用spanを作る。
    const std::span<const Vehicle> vehicle_view(vehicles);

    // 既定値: 100ms、seed=1、対称距離、評価数制限なし。
    routing::RoutingParam param;

    // 例では時間に余裕を持たせる。
    // 本番では残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 1回の探索候補評価の上限。
    // 0なら上限なし。
    param.max_move_evaluations = 3000;

    // 最後の引数へ渡すと、その呼び出しの統計が入る。
    routing::RoutingStats stats;

    // 0以上の全体予算。
    // 例では後処理の余裕を別に確保して使う。
    int budget_ms;
    std::cin >> budget_ms;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);

    // steady_clockの絶対時刻。
    // 相対時間と比べて早い方が使われる。
    param.deadline = deadline;

    // 各試行の相対予算。
    // deadline到達後まで20ms延長する指定ではない。
    param.time_limit_ms = 20;

    std::vector<int> vertices(vertex_count);
    std::iota(vertices.begin(), vertices.end(), 0);

    // 仕事と拠点を含む全IDの距離を保存する。
    const auto context = routing::make_routing_context(vertices, dist, param.symmetric);

    // 有効な候補を得るまでfeasible=false。
    routing::RoutingSolution<int, long long> result;

    for (std::uint64_t seed = 1;
        seed <= 5 && std::chrono::steady_clock::now() < deadline;
        ++seed) {
        // 残り予算内で複数の試行を行う。
        param.seed = seed;
        // 全試行で同じdeadlineを守る。
        auto candidate =
            routing::solve_cvrp(context, task_view, vehicle_view, param, &stats);
        // 実行可能な最良計画を保持。
        if (candidate.feasible
            && (!result.feasible || candidate.travel_cost < result.travel_cost)) {
            result = std::move(candidate);
        }
    }

    // 上で定義した全件出力関数へ結果を渡す。
    print_result(result);

    // ライブラリの出力演算子。
    // 各配列の先頭20要素と省略件数を表示する。
    std::cerr << result << '\n';

    // 全統計を名前付きで表示する。
    // 末尾の改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4 3 2
0 1 2 3
1 0 1 2
2 1 0 1
3 2 1 0
1 2 0 0 1
2 2 0 0 1
3 2 0 0 1
0 0 4 100
0 0 4 100
1000
```

**出力例**

```text
feasible 1
cost 8
reward 0
vehicles 2
route 0 2 2 3
route 1 1 1
unserved 0
```

標準出力は `feasible`、`cost`、`reward`、`vehicles`、各車両の `route 車両番号 頂点数 頂点列`、最後に `unserved 件数 仕事添字列` です。経路は作業地点だけで、出発・終了拠点は含みません。`feasible 0` の結果を実行計画として使わないでください。標準エラーには、先頭20要素までの結果表示と探索統計を出します。
