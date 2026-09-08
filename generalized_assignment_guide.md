# 一般化割当ソルバー 利用ガイド

対象は `generalized_assignment_solver_v12.hpp` に定義された `generalized_assignment_solver` です。GNU C++20で利用する、1ヘッダー完結のライブラリです。

用途を判断したい場合は1～4章、実際に呼び出したい場合は5～8章、内部の仕組みを理解したい場合は9章を読んでください。ユースケース番号U01～U13は4章・6章・8章で共通です。

この文書の `N` は仕事数、`A` は割当先数、`E` は登録候補数を表します。C++コードでは `Gap` を `generalized_assignment_solver` の別名として使います。

## 1. 何を解くライブラリか？

### 1.1 「仕事をどこへ割り当てるか」を決める

各仕事について、実行可能な選択肢の中から1個を選びます。選択肢は「割当先・消費する容量・費用」の組です。すべての仕事を割り当て、各割当先の容量上限を守り、費用の合計をできるだけ小さくします。

例えば、仕事を作業員へ割り当てるなら、仕事がタスク、割当先が作業員、容量が勤務可能時間、需要がその人の所要時間、費用が人件費です。同じ仕事でも、担当者によって所要時間と費用が異なって構いません。

同じ仕事・同じ割当先に複数の候補を登録することもできます。例えば「高速だが高額」「低速だが安価」という実行モードを区別できます。選べるのは、その仕事に属する全候補のうち1個です。

### 1.2 目的関数と制約

各仕事が選ぶ候補を $x_i$ と書くと、最小化する値は次です。

$$z(x)=\sum_{i=0}^{N-1} c_{x_i}$$

選択と容量に関する条件は次です。

$$x_i\in O_i\quad(0\le i<N)$$

$$L_a(x)=\sum_{i:a_{x_i}=a} d_{x_i}\le C_a\quad(0\le a<A)$$

| 記号 | 意味 | ライブラリとの対応 |
| --- | --- | --- |
| $N$ | 仕事の個数 | `jobs` / `job_count()` |
| $A$ | 割当先の個数 | `agents` / `agent_count()` |
| $i$ | 仕事番号。0以上N未満 | `job` |
| $a$ | 割当先番号。0以上A未満 | `agent` |
| $O_i$ | 仕事iに許された候補の集合 | `add_option(i, ...)` で登録した候補 |
| $o$ | 1個の候補を表す番号 | `add_option` が返す候補ID |
| $a_o$ | 候補oを選んだときの割当先 | 候補の `agent` |
| $d_o$ | 候補oを選んだときに消費する容量。非負 | 候補の `demand` |
| $c_o$ | 候補oを選んだときの費用。負値も可 | 候補の `cost` |
| $C_a$ | 割当先aが持つ総容量。非負 | `capacities[a]` |
| $x_i$ | 仕事iが選択する候補 | `result.option_ids[i]` |
| $x$ | すべての仕事の選択を並べたもの | `result.option_ids` 全体 |
| $L_a(x)$ | 割当先aで消費される容量の合計 | `result.load_of_agent[a]` |
| $z(x)$ | 選択した候補の費用の合計 | 完全解の `result.objective` |

目的関数の意味は、各仕事が実際に選んだ候補の費用を1回ずつ足すことです。容量の式では、その割当先を選んだ仕事だけの需要を足します。費用と需要は別の値で、単位も同じである必要はありません。

### 1.3 数値例

割当先0の容量を5、割当先1の容量を4とします。

| 仕事 | 割当先 | 需要 | 費用 |
| --- | ---: | ---: | ---: |
| 0 | 0 | 4 | 3 |
| 0 | 1 | 2 | 6 |
| 1 | 0 | 3 | 2 |
| 1 | 1 | 3 | 5 |

両方を割当先0へ入れると費用は5ですが、需要合計が7となり容量5を超えます。一方、仕事0を割当先0、仕事1を割当先1へ入れると、負荷は4と3、費用は8です。この割当は容量条件を満たします。

容量上限を守ることと、費用を小さくすることは別の条件です。安いだけの割当を有効な解として扱ってはいけません。

### 1.4 対象となるモデルの範囲

直接表せる資源制約は、割当先ごとに1種類の上限です。例えばCPU使用量とメモリ使用量の両方を同時に制限することはできません。また、仕事の分割、順序制約、仕事同士の相性費用はこの目的関数には含まれません。

候補を登録しなかった組合せは禁止です。仕事を採用しない選択を認める場合は、「未採用」を表す候補を明示的に追加します。

戻り値が完全な実行可能解であるとは限りません。部分解の `objective` は割り当てられた仕事だけの費用であり、上の「全仕事を割り当てた問題」の目的値ではありません。完全解として使用する前に `feasible()` を確認します。

## 2. 厳密解アルゴリズム

本ライブラリは一般に最適性を保証しません。次の条件がある場合は、最適性を保証できる別の方法を優先する余地があります。「厳密な方法なら必ず短時間で終わる」という意味ではなく、入力構造と実行時間の両方で判断します。

### 2.1 容量の競合がない場合

各仕事の最安候補を選んだ割当がすべての容量内なら、その割当は最適です。どの完全解でも各仕事の最安費用の合計より安くはできないためです。すべての需要が0の場合などが該当します。候補を1回ずつ見るだけで足り、GAP探索を実行する必要はありません。

### 2.2 単位需要なら最小費用流

すべての候補の需要が1で、容量が受入可能な仕事数を表すなら、次のネットワークに変換できます。

| 辺 | 流量上限 | 単位費用 | 意味 |
| --- | ---: | ---: | --- |
| 始点 → 各仕事 | 1 | 0 | 各仕事を1回だけ選ぶ |
| 仕事 → 候補の割当先 | 1 | その候補の費用 | 選択できる割当を表す |
| 各割当先 → 終点 | その割当先の容量 | 0 | 受入件数を守る |

N単位を流せれば、最小費用流の整数解がこの割当問題の最適解です。N単位流せない場合は、完全割当が存在しません。仕事1個が需要3を持つ場合に、単に3単位流す変換ではいけません。仕事が複数の割当先へ分割され得るからです。

ACLの `mcf_graph` へ辺を追加するときの費用は非負である必要があります。負費用がある場合は、仕事ごとに定数を足して非負化し、全仕事を1回ずつ選ぶ解同士の優劣を保つ方法があります。型・総和の範囲も確認してください。[ACL MinCostFlow公式仕様](https://atcoder.github.io/ac-library/master/document_ja/mincostflow.html)

全候補で需要が共通の正の整数qなら、容量をqで割り捨てて受入件数へ変換することもできます。仕事や候補によって需要が変わる一般の場合には使えません。

### 2.3 一対一割当なら専用の割当・matching

単位需要で、各割当先の容量が1なら一対一割当です。矩形の費用行列を扱う割当アルゴリズムや、二部グラフの最小費用matchingを検討します。候補数が多い場合でも、一般GAPより構造が単純です。禁止候補を巨大な費用で代用する場合は、禁止割当が返っていないことを別途検査する必要があります。

### 2.4 割当先数と整数容量が小さければ容量DP

各仕事を順に処理し、「各割当先でいくつ容量を使ったか」を状態にします。その仕事の候補を1個選び、該当する割当先の使用量を増やします。同じ使用量へ到達する方法のうち、最小費用だけを残せば厳密な動的計画法になります。

容量状態数をSとすると、次の大きさです。

$$S=\prod_{a=0}^{A-1}(C_a+1)$$

素直な実装では時間は概ね `O(E*S)`、費用だけならメモリは `O(S)` です。割当復元には追加の記録か再計算が必要です。容量や割当先数が大きくなると急増するため、小さい整数容量に限って実用的です。1つの実binと未採用候補なら、通常の0/1 knapsack DPが使える場合もあります。

### 2.5 可変仕事数や連結成分が小さい場合

固定仕事の負荷を容量から引き、残った少数の仕事だけ全探索・分枝限定する方法があります。仕事iの候補数をD_iとすると、素朴な組合せ数は候補数の積です。「20仕事なら必ず軽い」とは限りません。

また、仕事と割当先を結ぶ候補関係が複数の連結成分に分かれ、他の共有制約がないなら、成分ごとに独立に解いて足し合わせられます。全体が大きくても各成分が小さい場合は、成分ごとの厳密解が有用です。

### 2.6 一般形を厳密に解くMIP・CP-SAT

各候補を選ぶかどうかの0/1変数を置き、「仕事ごとの選択数は1」「割当先ごとの需要和は容量以下」を制約にしたMIPやCP-SATも選択肢です。外部実行環境や十分な時間を使えるなら、最適性証明まで求められる場合があります。ただし時間制限で得た単なる実行可能解と、最適性を証明した結果は区別します。容量付き割当のモデル例が公式資料にあります。[OR-Tools: Assignment with Task Sizes](https://developers.google.com/optimization/assignment/assignment_cp)

本ヘッダーはこれらの外部solverを呼びません。内部にも小規模な分枝限定処理がありますが、構築に失敗した場合の救済だけで、仕事数・探索node数・期限に制限があります。「小規模なら常に厳密なライブラリ」とは扱わないでください。

## 3. 差分更新

### 3.1 インタラクティブなターン制問題での使い方

同じ候補構造でコストや容量が少しずつ変わる場合は、最初に `make_session()` で前処理済みのsnapshotを1つ作り、ターンをまたいで保持します。各ターンは次の順です。

1. 観測結果からコスト・容量を更新する。
2. 必要なら、変更後の条件で前回の候補ID列を再評価する。
3. 全仕事を動かせるなら `improve`、変更を許す仕事が限定されるなら `repair` へ前回のID列を渡す。
4. 状態・実行可能性を確認し、採用したID列を次ターンへ持ち越す。

sessionが前回解を自動保存するわけではありません。前回解は呼出し側が `result.option_ids` として保持します。`session.solve` を再び呼ぶだけでは、前回解を初期解にすることにはなりません。

### 3.2 更新できるもの・再構築が必要なもの

| 変更対象 | 同じsessionで更新可能か | 方法と影響 |
| --- | --- | --- |
| 1候補のコスト | 可能 | `set_option_cost(id, cost)`。IDは登録時の候補ID |
| 全候補のコスト | 可能 | `set_option_costs(costs)`。登録ID順の全E要素 |
| 1割当先の容量 | 可能 | `set_capacity(agent, capacity)`。非負の総容量 |
| 全割当先の容量 | 可能 | `set_capacities(capacities)`。全A要素 |
| コスト・容量をsnapshot作成時へ戻す | 可能 | `reset_costs()` / `reset_capacities()` |
| 前回解・固定する仕事・可変にする仕事 | 可能 | 呼出しごとの `option_ids` / `mutable_jobs` で指定 |
| seed・期限・profileなどの探索条件 | 可能 | 呼出しごとの `solve_options` で指定 |
| 候補の需要 `demand` | 不可 | 問題を再登録し、新しいsessionを作る |
| 候補の仕事・割当先 | 不可 | 同上。需要0化で無効化することもできない |
| 候補の追加・削除 | 不可 | 元solverへの追加は可能だが、既存sessionには反映されない |
| 仕事数・割当先数の変更 | 不可 | 新しいsolver、または `init` 後に全候補を再登録 |

容量0は、正の需要を持つ候補を使えなくしますが、需要0の候補は依然として使えます。したがって「施設閉鎖」「サーバー停止」を容量0だけで表すには、そこへの全候補の需要が正であることが必要です。

候補の禁止を巨大なコストで代用するのも厳密な禁止ではありません。禁止関係自体が変わる場合は、再構築を選んでください。

### 3.3 何を再利用し、何をやり直すのか

sessionは仕事ごとの候補列、候補ID変換、現在のコスト・容量などを保持します。候補の再登録と仕事ごとのグループ化・基本整列を省けます。

一方、各呼出しの容量価格、初期構築、局所探索用の順序、ALNS状態は探索時に準備します。乱数生成器も指定seedから始まります。探索の途中状態や温度を前ターンから継続するAPIではありません。

コスト・容量setterは変更した時点で統計を再計算せず、次の `solve` / `improve` で必要に応じて全候補を走査します。`evaluate` は直接現在値を読みます。`repair` は可変仕事を残余問題へ取り出し、その部分の統計を計算します。大量の変更をまとめてから探索すると、統計更新を何度も挟まずに済みます。

### 3.4 部分repairの処理量

仕事ごとの最大候補数をD、可変仕事の候補数の合計をE_m、部分探索の処理量をT_mとします。

| 操作 | 処理量の目安 | 含まれるもの |
| --- | --- | --- |
| `make_session()` | `O(N+A+E log(D+1))` | 候補の整理、ID変換、snapshot |
| コスト1件・容量1件のsetter | `O(1)` | 値の変更と更新必要フラグ |
| コスト一括更新／reset | `O(E)` | 全コストを書き換える |
| 容量一括更新／reset | `O(A)` | 全容量を書き換える |
| `evaluate` | `O(N+A)` | ID検査・負荷・費用・超過集計 |
| `session.repair` | `O(N+A+E_m+T_m)` | 固定側検査、残余問題作成、探索、全体結果の復元 |
| sessionのコピー | `O(N+A+E)` | 現在値とreset先を含めて複製 |

可変仕事が1個でも、固定側と全体の戻り値を扱うため `O(N+A)` の処理は残ります。純粋な「変更件数だけの更新」ではありません。極小近傍で問題固有の差分評価が書けるなら、そちらの方が軽い場合があります。

sessionは元solverから独立しています。作成後の元solverの `init` や `add_option` は、既存sessionを更新しません。sessionをコピーすると、現在の変更値もコピーされますが、reset先は引き続きsnapshot作成時の値です。

### 3.5 構造変更時の前回解の扱い

再構築後の候補IDは別の意味になる可能性があります。同じ数値IDをそのまま渡さず、仕事・候補に利用者側の安定した識別子を付け、今回の候補IDへ移し替えます。消えた候補や新規仕事は未割当として扱います。

U13では、生き残った割当をまず固定して新規・欠損部分をrepairし、それが難しければ可変集合を全体へ広げます。構造の前処理はやり直しますが、意味が変わっていない前回の選択を利用できます。

## 4. ユースケース

以下ではコードから離れて、何を問題として与えるかを整理します。業種が違うだけで同じモデルになるものはU01へまとめ、変換や利用方法が異なるものを別の用途にしています。

### 4.1 U01：一般的な容量付き割当・疎な適合関係

仕事を作業員へ、計算タスクをサーバーへ、注文を処理拠点へ振り分けます。資格がない担当者や実行できないサーバーは選択肢から外します。

指定するのは、各割当先の容量と、各仕事の許可候補です。候補ごとに消費容量と加算可能な費用が必要です。容量として扱う資源は1種類に決めます。全仕事と全割当先の組合せを用意する必要はありません。

### 4.2 U02：機械の稼働時間上限を守る負荷割当

機械ごとに作業可能な時間があり、ジョブをどの機械に載せるかを決めます。同じジョブでも機械ごとに処理時間が違って構いません。

各機械の利用可能時間、各ジョブの実行可能な機械、そこでの処理時間と費用を指定します。返すのは機械への割当で、開始時刻や処理順ではありません。順序によらず、割り当てたジョブを任意順に実行できる状況に向いています。

makespanの目標値を容量にして、目標内の割当を探す部品にもできます。ただし探索失敗は「目標達成不可能」の証明ではなく、厳密な二分探索の可否判定には使えません。

### 4.3 U03：開設済み施設・固定中心への顧客割当

倉庫、基地局、集配拠点、クラスタ中心の位置と利用可能集合が決まっており、各顧客をどこで受け持つかを決めます。施設配置問題のうち、施設を決めた後の割当部分です。

施設容量、顧客需要、顧客と施設の組合せごとの費用、サービス不能な組合せを指定します。施設開設費は固定条件に含め、施設を使い始めたときだけ発生する費用として候補へ重複計上しないようにします。複数顧客を同じ車で回る経路長は、一般に顧客別費用の和にはなりません。

### 4.4 U04：任意採用・複数knapsack・未処理の明示

品物や案件を、どれかの箱・予算枠に入れるか、採用しないかを選びます。「採用しない」専用の仮の割当先を追加すると、各仕事を必ず1候補へ割り当てる形式にできます。

実際の箱の容量、各品物の箱別の消費量と純利益を指定します。利益最大化は費用を利益の符号反転にして表します。未採用候補の費用は0とします。

仕事を未処理にすると罰金がある問題では、未処理候補の費用をその罰金にする変形もできます。この場合の目的は「処理費用＋未処理罰金」の最小化で、単純な利益の符号反転とは戻り値の解釈が異なります。

### 4.5 U05：同じ割当先での複数実行モード

同じサーバーでも、圧縮あり・なしでメモリ量と計算費用が変わる、同じ機械でも省電力モードと高速モードがある、といった問題です。

各候補に、割当先、モード識別子、需要、費用を与えます。モードの意味は利用者が管理します。資源を共有するモードを別々の割当先に分けてしまうと、本来共有する容量が別枠になるので、同じ割当先の別候補として表します。

### 4.6 U06：外部で作った計画の検査・残容量の取得

問題固有のgreedy、入力から復元した計画、前ターンの解などが、候補関係と容量条件に合っているかを調べます。新しい割当を探す用途ではありません。

候補定義、容量、仕事ごとの選択候補を指定します。未割当を含めて調べられます。費用合計、未割当数、容量超過、残容量を取得できます。不正な候補番号と、意図的な未割当は別に判定するとデバッグしやすくなります。

### 4.7 U07：既存の完全解を下限品質として改善

すでに有効な割当があり、その品質を落とさずに追加探索をかけたい場合です。良い問題固有の初期解と、汎用探索を組み合わせられます。

完全で容量内に収まる初期割当と、探索予算を指定します。同じ費用・容量の下では、この初期解より費用が増えない結果を保持します。不完全な解を「部分的に生かす」入口とは区別します。

### 4.8 U08：予約済み仕事を固定する部分repair・LNS

確定済みの担当や発送済み注文は動かさず、未確定の仕事だけを再配置します。AHCの外側探索で、一部の仕事を近傍として選ぶ使い方も同じ形式です。

現在の全体割当と、今回変更してよい仕事の集合を指定します。集合に入れなかった仕事は厳密に固定します。固定部分だけで容量を超えている場合は、その固定条件のまま修復することはできません。可変集合を広げる判断は利用者側の役割です。

### 4.9 U09：コストが変わるターン・シナリオ

電力料金、処理単価、品質予測、担当者の優先度などが変わり、候補関係と消費容量は変わらない状況です。

変更対象の候補と変更後の費用を指定し、前回解から全体を改善します。費用だけの変更なら、前回の完全解は容量面では引き続き有効です。ただし費用合計は変更後の値で計算し直す必要があります。

### 4.10 U10：容量縮小・障害・予約量変化への対応

担当者の勤務時間が減る、サーバーが停止する、他用途の予約で利用可能量が変わる、といった場合です。

新しい全体容量と、現在の割当を指定します。超過した割当先の仕事を可変にし、必要なら移動先にいる仕事も動かせるようにします。容量が増えただけなら修復は不要ですが、増えた余裕を使って費用を下げたい場合は、別途改善対象を広げます。

### 4.11 U11：非分離な目的関数のための近傍提案

真の評価が、仕事同士の相性、経路長、シミュレーション結果などを含む場合です。候補ごとの費用だけでは真の目的を正確に表せないため、現在解の近くで有望な移動を誘導する代理費用を作ります。

現在解、可変仕事、代理費用の作り方、真の目的値の計算方法を指定します。GAPで得た提案を真の目的値と元問題の制約で再評価し、採用するか決めます。代理費用の改善を、そのまま真のスコア改善とは扱いません。

### 4.12 U12：複数seedの探索と最良解の保持

同じ入力で複数の探索経路を試し、良い解を残します。候補構造と費用・容量を固定したまま、乱数seedだけを変えます。

seedの列、各試行の探索条件、全体の期限を指定します。固定反復・期限なしなら、時間の揺らぎを受けにくい比較にも使えます。前試行の最良解を次試行へ渡す方式は、独立な複数試行とは違うので、測定条件を区別します。

### 4.13 U13：仕事・候補・需要が変わる問題の再構築

新しい注文が来る、仕事が完了して消える、利用可能な候補が増減する、仕事量の予測が変わる、といった構造変更です。

今回の仕事集合、候補集合、需要と費用、容量を指定します。前回との対応を保つ仕事・候補の外部識別子も必要です。候補IDを作り直し、引き継げる選択だけを今回の構造へ写します。コスト・容量setterだけで更新できるU09・U10とは区別します。

## 5. 利用準備

### 5.1 必要環境とinclude

GNU C++20を使います。ヘッダーは `bits/stdc++.h`、`std::span`、GNUの整数拡張などを使用します。ACLや添付のhash mapは、ライブラリ本体の利用には必要ありません。

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;
```

例えば利用者の `main.cpp` をビルドするなら次です。

```bash
g++ -std=gnu++20 -O2 -DNDEBUG -Wall -Wextra main.cpp -o main
```

### 5.2 コンストラクタと問題の初期化

| 呼出し | 型・引数 | 既定値／状態 | 設定の意味 |
| --- | --- | --- | --- |
| `Gap solver;` | 引数なし | 0仕事・0割当先・0候補 | 後から `init` する場合 |
| `Gap solver(jobs, agents, capacities);` | `int`, `int`, `vector<long long>` | 引数の既定値なし | 最初に問題サイズと総容量を設定 |
| `solver.init(jobs, agents, capacities);` | 上と同じ | 引数の既定値なし | 問題サイズ・容量を設定し、登録済み候補を消去 |

`jobs` と `agents` は非負、`capacities.size()` は `agents` と一致、各容量は非負です。容量列は値渡しで保持されます。渡したvectorを後から変更してもsolverは変わりません。既存solverへの `init` は候補IDの意味も初期化します。

容量には、solverが今回管理する仕事に使ってよい総量を渡します。`repair` で固定する仕事の負荷は内部で引くので、呼出し側で重ねて差し引いてはいけません。solverの外にいる別用途の予約は、あらかじめ利用可能容量へ反映して構いません。

### 5.3 候補の登録と候補ID

```cpp
int option_id = solver.add_option(job, agent, demand, cost);
```

| 引数 | 型 | 既定値 | 設定方法 |
| --- | --- | --- | --- |
| `job` | `int` | なし | 0以上 `jobs` 未満の仕事番号 |
| `agent` | `int` | なし | 0以上 `agents` 未満の割当先番号 |
| `demand` | `long long` | なし | その候補の容量消費量。0も可。割当先の容量と同じ単位 |
| `cost` | `long long` | なし | 加算する最小化費用。負値も可 |
| 戻り値 | `int` | 最初は0 | 登録順に増える候補ID。選択の復元・動的コスト更新に使う |

候補IDは仕事番号でも割当先番号でもありません。同じ仕事・割当先でもモードが違えば別IDです。構造を作り直さない限り、sessionの内部整列やコスト・容量更新で登録IDの意味は変わりません。

候補をまとめて持つための公開型もあります。

```cpp
Gap::assignment_option candidate{0, 1, 3, 8}; // job=0, agent=1, demand=3, cost=8
```

この型のfieldは `int job`, `int agent`, `long long demand`, `long long cost` です。各fieldに既定値はなく、上のように全値を与えてください。登録済み候補を参照する `solver.options()` は `const vector<assignment_option>&` を返します。

同じjob・agentで、需要も費用も別候補以上の候補は、他の意味を持たないなら登録前に除けます。ただしモード固有の外部制約や識別上の意味がある場合は、数値だけで消してはいけません。solverが重複・劣後候補を自動削除することもありません。

### 5.4 全探索パラメータと既定値

`Gap::solve_options options;` によって、次の8項目が初期化されます。`solve` 等にはこの構造体を明示的に渡します。引数を省略する `solve()` はありません。既定値のまま呼ぶなら `solver.solve({})` です。

| field | 型 | 既定値 | 意味・選び方 |
| --- | --- | --- | --- |
| `algorithm` | `Gap::profile` | `profile::alns` | 下表の探索範囲。時間と解品質を本番相当入力で比較 |
| `deadline` | `steady_clock::time_point` | `time_point::max()` | 絶対終了時刻。実行時間の長さそのものではない |
| `iteration_limit` | `long long` | `10000` | ALNSの反復上限。0以上。初期構築や前段の局所探索は数えない |
| `seed` | `std::uint64_t` | `123456789` | 各呼出しの乱数seed。比較条件をそろえるなら固定 |
| `maximum_ruin_jobs` | `int` | `64` | ALNSで一度に外す仕事数の設定上限。まず既定値を使用 |
| `repair_price_weight` | `double` | `0.25` | ALNS再挿入で容量価格を重視する倍率。有限な非負値 |
| `alns_temperature` | `double` | `0.05` | ALNSの悪化受理温度の倍率。有限な非負値 |
| `local_search_interval` | `int` | `128` | ALNS中の周期的relocateの間隔。0で無効、正値で有効 |

| `profile` | 実行する範囲 | 選び方の起点 |
| --- | --- | --- |
| `greedy` | 初期構築1回と必要な実行可能化 | 初期解を素早く作りたい場合 |
| `regret` | 複数の初期構築 | 軽い複数startを試す場合 |
| `lagrangian` | 容量価格付きの複数構築と追加の実行可能化 | 容量が厳しい問題の構築を重視する場合 |
| `local_search` | 上記に1仕事移動と2仕事の組替を追加 | 短い予算で完成解を改善する場合 |
| `alns` | 上記に部分破壊・再挿入探索を追加 | 反復探索の予算を確保できる場合 |

profileを上げれば必ず同じ時間で良い解になるとは限りません。前段だけで予算が尽きる可能性もあります。小さい部分問題では `lagrangian` / `local_search` / `alns` を比較するのが実用的です。`greedy` でもejectionの探索開始位置などにseedが影響する場合があります。

### 5.5 時間と反復数の設定

時間制限中心の例です。以下は設定部分であり、完全な利用例は8章にあります。

```cpp
Gap::solve_options options;
options.algorithm = Gap::profile::alns;
options.deadline = std::chrono::steady_clock::now()
                 + std::chrono::milliseconds(1800);
options.iteration_limit = std::numeric_limits<long long>::max();
options.seed = 42;
```

反復数中心の比較なら、`deadline` を既定の最大時刻にし、`iteration_limit` を例えば1000に設定します。`iteration_limit = 0` はALNSだけを止める設定で、初期構築・価格計算・初回の局所探索まで止めるものではありません。

期限はハード上限ではありません。中断しない前処理や内側の処理があるため、出力・検証・超過余地を含む余白を取ります。具体的な余白は最大規模での実測から決めてください。

同じ `options` を何度も渡せば同じ絶対時刻を共有します。1ターン10msずつなら、各ターンの開始時に新しい期限を作り、全体期限との小さい方を使います。U09～U11の例はこの形です。ライブラリ呼出し以外の重いcallbackや入出力を中断する機能はありません。

### 5.6 高度なパラメータの調整

変更する場合は、1項目ずつ、規模・候補数・容量余裕の異なる複数入力で調べます。以下の候補値は比較の起点であり、改善を保証する設定ではありません。

| パラメータ | 小さくした場合 | 大きくした場合 | 比較の起点 |
| --- | --- | --- | --- |
| `maximum_ruin_jobs` | 局所的で軽い再構成 | 大きい変更を試すが1反復が重い | 16, 32, 64, 128 |
| `repair_price_weight` | 元の費用を重視 | 容量を温存する方向を重視 | 0, 0.25, 0.5 |
| `alns_temperature` | 悪化解の受理を抑える | 探索が移動しやすくなる | 0, 0.05, 0.1 |
| `local_search_interval` | 周期的な局所改善を頻繁に行う | 再挿入探索へ反復を使う | 64, 128, 256。無効化は0 |

ruinの実効上限は、探索対象の仕事数nについて次です。整数除算を使います。

```text
effective_max = min(max(4, maximum_ruin_jobs), max(4, n / 20))
actual_count  = min(n, 4以上effective_max以下で選ぶ整数)
```

`repair` ではnは全体仕事数ではなく可変仕事数です。例えばn=100なら設定64でも上限5、n=20なら最大4、n=1なら実際に外すのは1仕事です。`maximum_ruin_jobs = 0` でALNSが無効になるわけではありません。

`repair_price_weight` が効くのはALNS再挿入です。初期構築の価格計算や、未割当を直す部分LNSの係数を変更する項目ではありません。`alns_temperature = 0` でも小さい基礎温度があるため、悪化受理を完全には止めません。`local_search_interval = 0` もALNS中の周期処理だけを止め、最初の局所探索は残ります。

### 5.7 session生成・変更・参照のAPI

```cpp
auto session = solver.make_session(); // 候補登録が終わってから1回作る
```

sessionを直接デフォルト構築することはできません。作成済みsessionのコピーは可能ですが、問題データを複製するので、反復ループの中で不要にコピーしないでください。

| API | 引数・戻り値の型 | 意味・既定値 |
| --- | --- | --- |
| `session.set_option_cost(id, cost)` | `int`, `long long` → `void` | 1候補の費用を絶対値で変更。引数既定値なし |
| `session.set_option_costs(costs)` | `span<const long long>` → `void` | 候補ID順の全E個。引数既定値なし |
| `session.reset_costs()` | 引数なし → `void` | session作成時の全費用へ戻す |
| `session.set_capacity(agent, capacity)` | `int`, `long long` → `void` | 1割当先の非負総容量。引数既定値なし |
| `session.set_capacities(capacities)` | `span<const long long>` → `void` | 全A個の非負総容量。引数既定値なし |
| `session.reset_capacities()` | 引数なし → `void` | session作成時の容量へ戻す |
| `session.option_cost(id)` | `int` → `long long` | 現在の候補費用。idは有効範囲内 |
| `job_count()` / `agent_count()` / `option_count()` | 引数なし → `int` | solver・session両方にある件数取得 |
| `capacities()` | 引数なし → `const vector<long long>&` | solverなら登録容量、sessionなら現在容量 |
| `solver.options()` | 引数なし → `const vector<assignment_option>&` | 登録順の候補定義。sessionには同名APIはない |

参照で返るvectorはsnapshotのコピーではありません。後の変更をまたいで安定した値が必要なら自分でコピーします。候補追加や容量更新をまたいで、要素参照・iterator・`data()` のポインタを保持しないでください。

### 5.8 ユースケースによる準備の違い

| 用途 | 基本準備に加えて必要なもの |
| --- | --- |
| U01～U03 | 実際の時間・需要・費用を整数単位へ変換。禁止候補を除く |
| U04 | 未採用用dummyと、全仕事をdummyへ入れた有効な初期解 |
| U05 | 候補IDからモードを復元する対応表 |
| U06～U08 | 登録候補IDで表した全仕事分の選択列。U08は可変仕事集合も必要 |
| U09・U10 | ターンをまたいで保持するsessionと前回の候補ID列 |
| U11 | 代理費用を作る処理、真の目的値と外部制約を評価する処理 |
| U12 | 試すseed列。費用・容量・候補構造は試行中に固定 |
| U13 | ターン間で意味を保つ仕事key・候補keyと、今回のIDへの対応表 |

## 6. ユースケースごとの使い方

### 共通：探索メソッドの引数

以下はシグネチャの説明です。solver版は `const` 呼出しができ、session版の探索は内部の統計を更新する場合があります。

| 呼出し | 入力 | 動作 |
| --- | --- | --- |
| `solve(const solve_options& options)` | 探索条件 | 初期解を作って探索 |
| `improve(span<const int> option_ids, const solve_options& options)` | N個の候補IDと探索条件 | 完全かつ容量内なら初期解として保持。無効なら全体を無視して再構築 |
| `repair(span<const int> option_ids, span<const int> mutable_jobs, const solve_options& options)` | N個の候補ID、可変仕事番号列、探索条件 | 補集合を固定して部分問題を探索 |
| `evaluate(span<const int> option_ids)` | N個の候補ID | 探索せず現在の割当を集計 |

4つとも `Gap::result` を返します。solver版の探索では呼出しごとに候補の前処理を行います。solver版 `repair` は内部で一時sessionを作るため、反復用途は明示的なsessionを使います。vectorは `span` 引数へ渡せます。`span` は所有しない参照なので、呼出し中は元の配列を生存させ、他のthreadから書き換えないでください。

### 共通：戻り値の読み方

| `result` のfield | 型・通常の長さ／初期値 | 意味 |
| --- | --- | --- |
| `option_ids` | `vector<int>`、N要素 | 各仕事が選んだ登録候補ID。未割当は-1 |
| `agent_of_job` | `vector<int>`、N要素 | 選んだ割当先。未割当は-1 |
| `load_of_agent` | `vector<long long>`、A要素 | 各割当先の需要和 |
| `objective` | `long long`、初期値0 | 有効に割り当てられた候補だけの費用和 |
| `total_overflow` | `long long`、初期値0 | 各割当先の `max(0, load-capacity)` の合計 |
| `unassigned_count` | `int`、初期値0 | 未割当と、解釈できなかったIDの仕事数 |
| `iterations` | `long long`、初期値0 | 実行したALNS反復数。構築や前段の局所探索は含まない |
| `lower_bound` | `double`、初期値は負の無限大 | 計算できた下界。有限でも浮動小数誤差に注意 |
| `initial_solution_used` | `bool`、初期値false | 与えた完全な初期解を初期incumbentとして使ったか |
| `repair_state` | `Gap::repair_status`、初期値 `not_requested` | repair入力の処理状態 |

戻り値のvector長は通常のメソッドが作る結果についての説明です。利用者が単に `Gap::result r;` と宣言した場合、vectorは空です。

`r.feasible()` は `unassigned_count == 0 && total_overflow == 0` を判定する `bool` メソッドです。基本はこれを確認してから割当を使います。部分解同士を比較するなら未割当数を先に見てから費用を比較し、部分解と完全解の費用だけを比べないでください。

| `repair_state` | 意味 | 呼出し側の扱い |
| --- | --- | --- |
| `not_requested` | `solve` / `improve` / `evaluate` の結果 | repair状態としての意味はない |
| `completed` | 固定検査・必要な部分探索を通常どおり処理 | さらに `feasible()` を確認 |
| `invalid_input` | ID列の長さ、可変集合、固定仕事IDなどが不正 | 入力を直す。探索時間を増やしても解消しない |
| `fixed_part_infeasible` | 固定仕事だけで容量超過 | 固定条件・容量・可変集合を見直す |

### 6.1 U01：候補登録から一括solve

`Gap(jobs, agents, capacity)` でサイズと容量を設定し、候補を `add_option` で登録してから `solve(options)` を呼びます。8.1節の `solve_capacitated_assignment` は、この準備を関数内に含みます。

`choices[k]` をそのまま登録するので、結果の候補IDは `choices` の添字です。完全解なら `agent_of_job[job]` を担当先、`load_of_agent[agent]` を使用量として取り出します。

### 6.2 U02：処理時間を需要として登録

8.2節の `solve_machine_loads` へ `available_time` と `choices_by_job` を渡します。機械ごとの利用可能時間を容量、`processing_time` を需要、運転費などを `cost` として登録し、`solve(options)` を呼びます。

返る `MachinePlan.assignment` が完全解なら、`jobs_on_machine[machine]` に載った仕事をその機械で実行します。リストはjob番号順にまとめただけです。個別の時刻制約を満たすスケジュールを生成したわけではありません。

### 6.3 U03：サービス可能な組合せだけを登録

8.3節の `solve_clients_to_facilities` は、容量列、顧客需要列、`service_cost[job][agent]` の行列を受けます。行列の要素型は `optional<long long>` で、`nullopt` の組合せは登録しません。

`solve(options)` の結果が完全なら `agent_of_job` を施設番号として利用します。行列は顧客数×施設数と一致させます。外側で施設集合が変わる場合は、3章の更新可否に従います。

### 6.4 U04：未採用を有効な初期解として渡す

8.4節の `solve_optional_profit` は、実bin容量と品物別の `{bin, weight, profit}` を受けます。実候補を費用 `-profit` で登録し、追加したdummyへ需要1・費用0の候補を各品物1個ずつ登録します。

全品物をdummyへ入れたID列を `improve(all_skipped, options)` に渡すため、探索開始前から完全な実行可能解があります。返る `bin_of_item == -1` は「未採用」であり、`assignment.option_ids == -1` という「未割当」とは違います。

利益は `total_profit` に復元します。未処理罰金をモデル化したい場合は、dummyの費用を罰金、実候補の費用を処理費に変え、結果を総費用として読みます。費用の意味を変えたまま `-objective` を利益として使わないでください。

### 6.5 U05：モードを候補ID対応表で復元

8.5節の `solve_execution_modes` は `modes_by_job[job]` の各 `{agent, mode, demand, cost}` を登録し、候補IDに対応するモードを別vectorへ保存します。solver自身にmodeパラメータはありません。

`solve(options)` 後に、選んだ `option_ids` から `mode_of_job` を復元します。agent番号だけでは同じagent内の複数モードを区別できません。例では非負のモード番号を入力し、未割当だけを-1で表します。

### 6.6 U06：評価と形式検査

8.6節の `inspect_assignment` は候補を登録して `evaluate(option_ids)` を呼び、`capacity-load` を残容量として返します。何度も評価する実際のループでは、構築済みsolverまたはsessionを外側へ保持し、評価部分だけ反復します。

`ids_well_formed` はID列の長さ・所属仕事を検査します。意図的な-1は形式上許可します。完全解として使うには、この値と `evaluation.feasible()` の両方を確認します。sessionで変更した条件を評価するなら、元solverの `evaluate` ではなくsession版を使います。

### 6.7 U07：完全初期解を確認してimprove

8.7節の `improve_external_assignment` は、登録後に `evaluate(initial_ids)` で完全性を確認し、問題なければ `improve(initial_ids, options)` を呼びます。

入力が無効なら `nullopt` を返します。成功時は `initial_solution_used` と目的値非悪化も確認します。これは例のラッパーの契約です。ライブラリの `improve` を直接呼ぶ場合、無効な初期解は無視されて通常構築へ進みます。

### 6.8 U08：固定仕事を残して近傍だけrepair

8.8節の `repair_neighborhoods` は候補登録後にsessionを1回作り、近傍ごとに `session.repair(current_ids, mutable_jobs, options)` を呼びます。`current_ids` は全N仕事分、`mutable_jobs` は今回可変にする仕事番号だけです。

`completed` かつ完全解になった提案だけを採用します。失敗した結果も `attempts` に残すので、入力不正・固定容量超過・探索失敗を区別できます。完全な現在解をそのまま渡せば同じ費用で品質を保ちますが、可変部分のIDを意図的に-1へ消すと、その初期品質の保証は失われます。

可変集合が空なら固定部分の検査だけです。正常時は同じ解を返し、`iterations == 0`、`initial_solution_used == true`、`lower_bound == double(objective)` になります。

### 6.9 U09：変更候補だけ更新して前回解からimprove

8.9節の `solve_cost_turns` はsessionを1回作り、`CostChange{option_id, new_cost}` を `set_option_cost` で反映します。`new_cost` は差分加算量ではなく変更後の絶対値です。同じIDを1ターン内で複数回指定した場合は、最後の値になります。

初期ID列が空なら初回は `solve`、有効な前回解があれば `improve` です。`per_turn` と全体 `options.deadline` の両方で期限を作ります。返る結果はそのターンの費用です。別ターンの古い `objective` を、そのまま新しい目的関数の比較対象にしないでください。

例はターン列を関数内で処理します。実際のインタラクティブ問題ではsessionとcurrentを保持し、同じループ本体を「観測入力 → 更新 → 再探索 → 出力」の位置へ置きます。

### 6.10 U10：新容量で超過する仕事を可変にする

8.10節の `solve_capacity_turns` は、各ターンの `CapacityTurn.capacity` で `set_capacities` を呼びます。新容量で `evaluate(current)` し、超過agentにいる全仕事と `also_mutable` をまとめて `repair` へ渡します。集合は内部で重複を除きます。

容量が増えただけで超過がなく、`also_mutable` も空なら、可変集合は空となり割当は変えません。増えた余裕を使って改善するなら `also_mutable` に仕事を加えるか、全体を `improve` します。

失敗時は計画を採用しませんが、現在容量そのものは新しい値です。持ち越した計画をそのまま実行可能と考えず、返った結果の状態を確認します。部分repairで見つからなければ、移動先の仕事も可変にするなど近傍を広げます。

### 6.11 U11：代理費用で提案し、真の目的値で採否を決める

8.11節の `solve_with_surrogates` は、現在ID列と反復番号を `build_step` に渡し、全候補の代理費用と可変集合を取得します。`set_option_costs` → `repair` の順で近傍提案を作ります。

`true_cost(ids)` は `optional<long long>` を返し、小さいほど良い真の費用を表します。GAP以外の制約違反なら `nullopt` で拒否できます。真の費用が厳密に下がった場合だけ採用します。最大化問題なら比較方向を一貫して変えるか、安全な範囲で符号反転してください。

この関数の実行中は真の目的関数と外部制約を固定し、callbackは入力ID列を書き換えずに値を返します。外部条件自体がターンで変わる場合は、現在解の真の費用も変更後の条件で再評価する必要があります。

戻り値の `true_cost` と、代理費用の `proposal.objective` は別物です。例は終了時に `reset_costs` し、`assignment` を元費用で再評価して返します。この最後の評価結果の `iterations` は0で、探索全体の反復数ではありません。

同じブロックの `solve_pair_penalties` は具体的な利用例です。上三角行列 `pair_cost[i][j]` に、仕事iとjが同じagentへ入ったときの追加費用を設定します。代理費用の生成と真の評価の両方を含むので、callbackを自作せずにこの問題を試せます。代理費用生成は `O(E*N)`、真の評価は `O(N*N)` の素直な実装で、外側処理も時間予算に含めます。

### 6.12 U12：seed列を変えて最良完全解を維持

8.12節の `solve_seed_portfolio` は、最初の完全解がない間は `session.solve`、見つかった後は `session.improve(best.option_ids, options)` を呼びます。`options.seed` だけを入力seed列の値で上書きします。

seed列が空、開始前に期限切れ、または全試行失敗なら `nullopt` です。完全解が見つかった場合だけ結果を使います。固定反復比較では無期限にし、反復数とseed列を固定します。有限の期限は全seed共通なので、先の試行だけで予算を使い切る場合があります。

### 6.13 U13：外部識別子で再構築先へ対応付け

8.13節の `solve_rebuilt_problem` は `job_keys`、`KeyedChoice`、前回の `job_key -> option_key` 対応を受けます。今回のsolverを構築して候補を登録し、現在存在し、かつ所属仕事も一致する候補だけを初期ID列へ写します。

欠損した仕事だけを可変にしたrepairをまず試します。完全解が得られれば、それを初期解に全体をimproveします。固定負荷超過などで失敗した場合は全仕事を可変にしてrepairします。2回目も同じ期限を使うため、1回目で予算を使い切る可能性があります。

`chosen_keys` は今回の `job_keys` と同じ順で、未割当は `nullopt` です。次ターンへはこの外部keyで持ち越します。成功時に一部の引継ぎ割当が変わることは許しており、確定予約の保持を保証する関数ではありません。絶対に固定すべき仕事がある場合は、全可変fallbackを使わずU08の契約を守ります。

## 7. 制約・注意点

### 7.1 モデル上の制約

| 対象 | 直接扱えるか | 注意点 |
| --- | --- | --- |
| 各仕事が1候補を選ぶ | 可能 | 分割・複数選択は不可 |
| 割当先ごとの非負需要和の上限 | 可能 | 資源は1種類 |
| 同じjob・agentの複数候補 | 可能 | mode等は登録IDで区別 |
| 任意採用 | dummyで変換 | 未割当を採用しない選択の代用にしない |
| 多次元容量、下限使用量 | 不可 | 大きい係数で1数値へ詰めても一般に正しい変換にならない |
| 施設の開設固定費、ペア相性、非線形な負荷費用 | 不可 | 外側で固定・厳密分離するか、代理費用＋真の再評価 |
| 順序・先行・時間窓・経路・仕事間排他 | 不可 | 元問題側で扱う必要がある |

容量合計に余裕があっても、特定の仕事が使える候補先だけが混雑して完全解が存在しないことがあります。需要も候補先で変わるため、単純な総需要と総容量の比較だけでは十分でありません。

### 7.2 入力とメソッドの組合せ

| 入力 | `evaluate` | `improve` | `repair` |
| --- | --- | --- | --- |
| 完全で容量内のID列 | 集計 | 初期解として保持 | 固定側と可変側を分離して保持 |
| ID列の長さがNでない | 全仕事未割当の結果 | 初期解を無視 | `invalid_input` |
| -1、他の負値、範囲外ID、別仕事のID | 該当仕事を未割当として集計 | 初期解全体を無視 | 可変側なら再構築。固定側なら `invalid_input` |
| 全体が容量超過 | 超過を集計 | 初期解全体を無視 | 固定側だけの超過でなければ修復を試せる |
| 固定側だけで容量超過 | 固定集合の概念なし | 固定集合の概念なし | `fixed_part_infeasible` |
| 可変仕事番号が範囲外・重複 | 引数なし | 引数なし | `invalid_input` |

`repair` は可変側の初期解を部分的に継ぎ足すのではありません。可変部分全体が有効なら初期解として保持し、1つでも欠損や残余容量違反があれば可変部分を再構築します。固定側はそのままです。

入力長や可変集合の形式エラーでは、空に相当するN仕事分の結果を返します。固定仕事の検査中のエラーでは、有効だった固定仕事の負荷・費用を集計した診断結果です。エラー結果を「入力計画がそのまま返った」と考えてはいけません。不正な固定IDと固定容量超過が併存する場合は `invalid_input` が優先されます。

`feasible()` は2つの集計値を見るだけです。利用者が作った未初期化のresultや、0仕事に対する不正な入力長まで検出する総合バリデータではありません。入力形式を別に確認したい場合はU06の形を使います。

### 7.3 パラメータの有効範囲

`iteration_limit`、`maximum_ruin_jobs`、`repair_price_weight`、`alns_temperature`、`local_search_interval` が探索に影響するのはALNS部分です。`local_search` 以下ではこれらを調整しても対応処理は実行されません。

反復上限には0以上、周期無効化には0、ruin設定には4以上を起点に使います。価格倍率・温度は有限な非負値にしてください。NaNや無限大を自動拒否・補正する仕組みはなく、比較や整列を壊す可能性があります。`deadline` は `system_clock` ではなく `steady_clock` の時刻です。

### 7.4 整数・浮動小数・サイズの範囲

容量、需要、費用、それらの総和、途中の加減算、目的値の差、利益の符号反転が符号付き64bit整数に収まることを呼出し側で保証します。最終値だけが収まればよいわけではありません。オーバーフロー検査や飽和演算はありません。

価格・温度・一部の候補評価・下界には `double` を使います。非常に大きい整数値やほとんど差のない大きな費用では、丸めで探索の順序が変わり得ます。単位の選択や仕事ごとの定数費用の扱いを検討してください。

N、A、E、N+1や候補数の合計など、内部の添字計算は `int` に収まる必要があります。必要なメモリを確保できることも前提です。8章の例でもこのサイズ条件と数値条件、時刻に予算を足せる範囲までは入力側の責任とします。

### 7.5 入力検証は例外APIではない

コンストラクタ・`init`・`add_option` は主にassertで前提を検査します。`-DNDEBUG` でassertを消して不正な番号や負需要を渡すと、安全にエラーになるとは限りません。

sessionのsetterには不正範囲・長さ・負容量を拒否するガードもありますが、戻り値は `void` で成功を通知しません。`option_cost` の不正IDはreleaseで0になり得ますが、その値を有効な費用として扱う契約ではありません。利用者側で有効性を確認してください。8章のラッパーでは、典型的な形式違反を例外で検出します。

### 7.6 期限と初期解

前処理・候補射影・統計更新・一部内側処理は期限で中断されません。期限を過ぎた `solve` は全仕事未割当で返ることもあります。完全な初期解を渡した `improve` / `repair` は探索前にそれを保持するので、期限が近いときほど外側で有効な解を消さないことが重要です。

完全解が存在しても発見できないことがあります。失敗は厳密な不可能判定ではありません。実行時間・解品質・実行可能解の発見について、未測定入力まで保証するものでもありません。

`greedy` でも、初期構築後のejectionや条件に合う小規模fallbackは実行されます。単純な候補1走査だけで終わると決めつけないでください。

### 7.7 下界と最適性の解釈

通常、`lower_bound` は価格計算の緩和下界です。`greedy` / `regret`、期限切れ、候補欠損などでは負の無限大のままになる場合があります。一方、完全解が内部の整数下界と一致した場合や、空可変集合のrepairでは、目的値のdouble表現を返します。

有限下界には浮動小数誤差があり、`objective == lower_bound` という外部比較だけで厳密最適と判定しないでください。結果には独立した最適性証明フラグがありません。

`repair` の下界は、固定仕事をそのままにした部分問題の下界です。固定仕事も動かせる元の全体問題の最適値を評価する下界として、そのまま使うことはできません。

### 7.8 更新状態・参照・並列利用

コスト変更後に比較する費用は、必ず同じ変更後条件で評価します。容量変更後は、以前の完全解でも容量超過になり得ます。`reset_*` は直前の変更をundoするメソッドではなく、snapshot作成時へ戻すメソッドです。

同じsessionを複数threadから同時に使うことは避けます。探索でも統計更新が起こり得ます。並列に試すなら独立したsessionを用意し、共有最良解の更新は外側で同期します。solverの参照中・探索中に別threadから候補を追加・初期化する使い方も避けます。

## 8. ユースケースごとのコード例

各ブロックはincludeから始まる独立したコードです。使うブロックを自分のソースへコピーし、説明した入力を作って関数を呼びます。`main` は含めません。

候補を `vector<Gap::assignment_option>` として受ける例は、`{job, agent, demand, cost}` を登録順に並べます。その添字が候補IDになります。独自の問題番号からの変換が必要な場合は、入力を作る段階で対応を付けてください。数値・サイズの共通前提は7.4節です。

### 8.1 U01：一般的な容量付き割当

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

// jobs: 仕事数。capacity[a]: 割当先aの総容量（非負）。
// choices: {job, agent, demand, cost} の列。登録しない組合せは禁止。
// choices[k]の候補IDはk。costは最小化する費用で、負値も可。
// options: 探索条件。deadlineは前処理を含む呼出し全体の絶対時刻。
// 戻り値: job順の割当。feasible()を確認してから完全解として使う。
Gap::result solve_capacitated_assignment(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        Gap::solve_options options) {
    const int agents = (int)capacity.size();
    if (jobs < 0 || std::any_of(capacity.begin(), capacity.end(),
                              [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid size or capacity");
    }
    Gap solver(jobs, agents, capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    return solver.solve(options);
}
```

### 8.2 U02：機械の負荷割当と仕事一覧の復元

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct MachineChoice {
    int machine;                // 利用可能時間列の添字
    long long processing_time; // この機械で必要な時間（非負）
    long long cost;            // 運転費など。処理順には依存しない費用
};
struct MachinePlan {
    Gap::result assignment;
    std::vector<std::vector<int>> jobs_on_machine; // 各機械上の仕事番号
};

// choices_by_job[j]: ジョブjを実行できる機械と、その処理時間・費用。
// available_time[a]: 機械aが使える時間の総量。個別ジョブの締切ではない。
// 戻り値: assignment.feasible()なら、jobs_on_machineを各機械で実行できる。
// リストはjob番号順。release time・段取り・先行制約は扱わない。
MachinePlan solve_machine_loads(
        const std::vector<long long>& available_time,
        const std::vector<std::vector<MachineChoice>>& choices_by_job,
        Gap::solve_options options) {
    const int jobs = (int)choices_by_job.size();
    const int machines = (int)available_time.size();
    if (std::any_of(available_time.begin(), available_time.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("negative available time");
    }
    Gap solver(jobs, machines, available_time);
    for (int job = 0; job < jobs; ++job) {
        for (const auto& c : choices_by_job[job]) {
            if (c.machine < 0 || c.machine >= machines || c.processing_time < 0) {
                throw std::invalid_argument("invalid machine choice");
            }
            solver.add_option(job, c.machine, c.processing_time, c.cost);
        }
    }
    MachinePlan plan;
    plan.assignment = solver.solve(options);
    plan.jobs_on_machine.resize(machines);
    for (int job = 0; job < jobs; ++job) {
        int machine = plan.assignment.agent_of_job[job];
        if (machine >= 0) plan.jobs_on_machine[machine].push_back(job);
    }
    return plan;
}
```

### 8.3 U03：開設済み施設への顧客割当

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

// facility_capacity[a]: 開設済み施設aの容量。施設の開設判断は入力時に確定。
// demand[j]: 顧客jの需要（非負）。この例では施設によらず同じ値。
// service_cost[j][a]: 加算可能な配送費など。nulloptなら割当禁止。
// 戻り値: feasible()ならagent_of_job[j]が顧客jの利用施設。
Gap::result solve_clients_to_facilities(
        const std::vector<long long>& facility_capacity,
        const std::vector<long long>& demand,
        const std::vector<std::vector<std::optional<long long>>>& service_cost,
        Gap::solve_options options) {
    const int jobs = (int)demand.size();
    const int agents = (int)facility_capacity.size();
    if ((int)service_cost.size() != jobs ||
        std::any_of(facility_capacity.begin(), facility_capacity.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid matrix or capacity");
    }
    Gap solver(jobs, agents, facility_capacity);
    for (int job = 0; job < jobs; ++job) {
        if (demand[job] < 0 || (int)service_cost[job].size() != agents) {
            throw std::invalid_argument("invalid demand or matrix row");
        }
        for (int agent = 0; agent < agents; ++agent) {
            if (service_cost[job][agent]) {
                solver.add_option(job, agent, demand[job], *service_cost[job][agent]);
            }
        }
    }
    return solver.solve(options);
}
```

### 8.4 U04：任意採用・利益最大化

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct ProfitChoice {
    int bin;          // 実際の箱・予算枠の番号
    long long weight; // この箱で消費する容量（非負）
    long long profit; // この候補を採用した純利益。負値も可
};
struct ProfitPlan {
    Gap::result assignment; // dummyを含む完全割当の評価
    std::vector<int> bin_of_item; // -1なら採用しない
    long long total_profit = 0;
};

// bin_capacity: 実binの容量。choices_by_item[j]: 品物jの採用候補。
// 1品物は1binだけに入れるか、採用しない。未採用の利益は0。
// 戻り値: dummyを実binと区別した割当と純利益合計。
// 全品物を未採用にする初期解を渡すため、期限切れでも完全解を保持する。
ProfitPlan solve_optional_profit(
        const std::vector<long long>& bin_capacity,
        const std::vector<std::vector<ProfitChoice>>& choices_by_item,
        Gap::solve_options options) {
    const int jobs = (int)choices_by_item.size();
    const int bins = (int)bin_capacity.size();
    if (std::any_of(bin_capacity.begin(), bin_capacity.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("negative bin capacity");
    }
    std::vector<long long> capacity = bin_capacity;
    capacity.push_back(jobs); // dummy: 各品物が容量1を使うので全品物を収容可能
    Gap solver(jobs, bins + 1, capacity);
    std::vector<int> all_skipped(jobs);
    for (int job = 0; job < jobs; ++job) {
        for (const auto& c : choices_by_item[job]) {
            if (c.bin < 0 || c.bin >= bins || c.weight < 0 ||
                c.profit == std::numeric_limits<long long>::min()) {
                throw std::invalid_argument("invalid profit choice");
            }
            solver.add_option(job, c.bin, c.weight, -c.profit);
        }
        all_skipped[job] = solver.add_option(job, bins, 1, 0);
    }
    ProfitPlan plan;
    plan.assignment = solver.improve(all_skipped, options);
    plan.bin_of_item.assign(jobs, -1);
    if (!plan.assignment.feasible()) return plan;
    if (plan.assignment.objective == std::numeric_limits<long long>::min()) {
        throw std::overflow_error("total profit does not fit long long");
    }
    plan.total_profit = -plan.assignment.objective;
    for (int job = 0; job < jobs; ++job) {
        int bin = plan.assignment.agent_of_job[job];
        if (bin != bins) plan.bin_of_item[job] = bin;
    }
    return plan;
}
```

### 8.5 U05：複数実行モードの割当

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct ModeChoice {
    int agent;        // 実際の割当先番号
    int mode;         // 利用者が決める非負のモード番号
    long long demand;
    long long cost;
};
struct ModePlan {
    Gap::result assignment;
    std::vector<int> mode_of_job; // -1ならその仕事が未割当
};

// modes_by_job[j]: 同じagentの候補を複数含んでもよい。
// 戻り値: 割当先に加え、選択されたモードを候補IDから復元したもの。
ModePlan solve_execution_modes(
        const std::vector<long long>& capacity,
        const std::vector<std::vector<ModeChoice>>& modes_by_job,
        Gap::solve_options options) {
    const int jobs = (int)modes_by_job.size();
    const int agents = (int)capacity.size();
    if (std::any_of(capacity.begin(), capacity.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("negative capacity");
    }
    Gap solver(jobs, agents, capacity);
    std::vector<int> mode_by_option;
    for (int job = 0; job < jobs; ++job) {
        for (const auto& c : modes_by_job[job]) {
            if (c.agent < 0 || c.agent >= agents || c.mode < 0 || c.demand < 0) {
                throw std::invalid_argument("invalid mode");
            }
            int id = solver.add_option(job, c.agent, c.demand, c.cost);
            mode_by_option.resize(id + 1);
            mode_by_option[id] = c.mode;
        }
    }
    ModePlan plan;
    plan.assignment = solver.solve(options);
    plan.mode_of_job.assign(jobs, -1);
    for (int job = 0; job < jobs; ++job) {
        int id = plan.assignment.option_ids[job];
        if (id >= 0) plan.mode_of_job[job] = mode_by_option[id];
    }
    return plan;
}
```

### 8.6 U06：外部解の検査と残容量

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct AssignmentInspection {
    Gap::result evaluation;
    std::vector<long long> remaining_capacity; // capacity-load。負なら超過
    bool ids_well_formed = false; // -1は許すが、他の不正IDや長さ違いは拒否
};

// choices[k]の候補IDをkとして、option_ids[j]に仕事jの選択を渡す。
// -1は意図的な未割当。この関数は探索せず、入力も変更しない。
// 実行可能性はids_well_formedとevaluation.feasible()の両方で確認する。
AssignmentInspection inspect_assignment(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const int> option_ids) {
    const int agents = (int)capacity.size();
    if (jobs < 0 || std::any_of(capacity.begin(), capacity.end(),
                              [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid size or capacity");
    }
    Gap solver(jobs, agents, capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    AssignmentInspection out;
    out.evaluation = solver.evaluate(option_ids);
    out.ids_well_formed = (int)option_ids.size() == jobs;
    if (out.ids_well_formed) {
        for (int job = 0; job < jobs; ++job) {
            int id = option_ids[job];
            if (id == -1) continue;
            if (id < 0 || id >= (int)choices.size() || choices[id].job != job) {
                out.ids_well_formed = false;
                break;
            }
        }
    }
    out.remaining_capacity.resize(agents);
    for (int agent = 0; agent < agents; ++agent) {
        out.remaining_capacity[agent] = capacity[agent] - out.evaluation.load_of_agent[agent];
    }
    return out;
}
```

### 8.7 U07：有効な完全初期解の改善

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

// initial_ids[j]: choices列での仕事jの選択候補番号。完全かつ容量内が必須。
// 戻り値: 無効な初期解ならnullopt。有効ならその目的値以下の完全解。
// 「不正な初期解を無視して一からsolveする」動作を避けるラッパー。
std::optional<Gap::result> improve_external_assignment(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const int> initial_ids, Gap::solve_options options) {
    const int agents = (int)capacity.size();
    if (jobs < 0 || std::any_of(capacity.begin(), capacity.end(),
                              [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid size or capacity");
    }
    Gap solver(jobs, agents, capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    if ((int)initial_ids.size() != jobs) return std::nullopt;
    auto before = solver.evaluate(initial_ids);
    if (!before.feasible()) return std::nullopt;
    auto after = solver.improve(initial_ids, options);
    if (!after.feasible() || !after.initial_solution_used ||
        after.objective > before.objective) {
        throw std::logic_error("warm-start contract was not preserved");
    }
    return after;
}
```

### 8.8 U08：sessionを再利用した複数近傍repair

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct NeighborhoodRun {
    std::vector<int> current_ids; // 最後に採用した完全解。未成功なら入力のまま
    std::vector<Gap::result> attempts; // 各近傍の成否・部分解を含む記録
};

// initial_ids[j]: choices列での候補ID。未割当は-1。
// neighborhoods[k]: k回目に変更を許す仕事番号。重複なし。
// 可変集合外のIDは有効な割当でなければならない。
// options.deadlineは全近傍で共通。成功した完全解だけをcurrent_idsへ採用する。
NeighborhoodRun repair_neighborhoods(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const int> initial_ids,
        const std::vector<std::vector<int>>& neighborhoods,
        Gap::solve_options options) {
    const int agents = (int)capacity.size();
    if (jobs < 0 || (int)initial_ids.size() != jobs ||
        std::any_of(capacity.begin(), capacity.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid size, IDs, or capacity");
    }
    Gap solver(jobs, agents, capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    auto session = solver.make_session(); // 近傍ループの外で1回だけ準備
    NeighborhoodRun out;
    out.current_ids.assign(initial_ids.begin(), initial_ids.end());
    out.attempts.reserve(neighborhoods.size());
    for (const auto& mutable_jobs : neighborhoods) {
        auto proposal = session.repair(out.current_ids, mutable_jobs, options);
        if (proposal.repair_state == Gap::repair_status::completed && proposal.feasible()) {
            out.current_ids = proposal.option_ids;
        }
        out.attempts.push_back(std::move(proposal));
    }
    return out;
}
```

### 8.9 U09：コスト差分を反映するターン処理

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct CostChange {
    int option_id;      // choices列の添字
    long long new_cost; // 加算差分ではなく、変更後の絶対値
};

// changes_by_turn[t]: ターンtで変わる候補だけを列挙。未指定候補の値は持ち越す。
// initial_ids: 完全解の候補ID列、または空（初回にsolveする）。
// per_turn: 各ターンの時間予算。options.deadlineは全体の終了時刻。
// 戻り値: 各ターンの変更後コストで評価した結果。異なるターンのobjectiveは直接比較しない。
std::vector<Gap::result> solve_cost_turns(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const int> initial_ids,
        const std::vector<std::vector<CostChange>>& changes_by_turn,
        std::chrono::milliseconds per_turn, Gap::solve_options options) {
    const int agents = (int)capacity.size();
    if (jobs < 0 || (!initial_ids.empty() && (int)initial_ids.size() != jobs) ||
        per_turn.count() <= 0 || std::any_of(capacity.begin(), capacity.end(),
                                           [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid turn input");
    }
    Gap solver(jobs, agents, capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    auto session = solver.make_session();
    std::vector<int> current(initial_ids.begin(), initial_ids.end());
    if (!current.empty() && !session.evaluate(current).feasible()) {
        throw std::invalid_argument("initial assignment is not feasible");
    }
    std::vector<Gap::result> answers;
    answers.reserve(changes_by_turn.size());
    for (const auto& changes : changes_by_turn) {
        auto turn_options = options;
        turn_options.deadline = std::min(options.deadline,
            std::chrono::steady_clock::now() + per_turn);
        for (const auto& change : changes) {
            if (change.option_id < 0 || change.option_id >= session.option_count()) {
                throw std::invalid_argument("invalid changed option ID");
            }
            session.set_option_cost(change.option_id, change.new_cost);
        }
        auto answer = (int)current.size() == jobs
            ? session.improve(current, turn_options) : session.solve(turn_options);
        if (answer.feasible()) current = answer.option_ids;
        answers.push_back(std::move(answer));
    }
    return answers;
}
```

### 8.10 U10：容量変更後の部分repair

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct CapacityTurn {
    std::vector<long long> capacity; // 固定負荷を差し引く前の全体容量
    std::vector<int> also_mutable;   // 移動先の仕事など、追加で動かしてよい仕事
};

// initial_idsは候補として全仕事が有効ならよい（容量超過は修復対象）。
// 各ターン、超過agent上の全仕事とalso_mutableを可変にする。
// 戻り値: ターン順のrepair結果。失敗時は入力計画を持ち越して次ターンで再試行する。
// 失敗した計画が新容量で実行可能とは限らない。必ず各結果の状態を確認する。
std::vector<Gap::result> solve_capacity_turns(
        int jobs, const std::vector<long long>& initial_capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const int> initial_ids,
        const std::vector<CapacityTurn>& turns,
        std::chrono::milliseconds per_turn, Gap::solve_options options) {
    const int agents = (int)initial_capacity.size();
    if (jobs < 0 || (int)initial_ids.size() != jobs || per_turn.count() <= 0 ||
        std::any_of(initial_capacity.begin(), initial_capacity.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid turn input");
    }
    Gap solver(jobs, agents, initial_capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    auto session = solver.make_session();
    std::vector<int> current(initial_ids.begin(), initial_ids.end());
    if (session.evaluate(current).unassigned_count != 0) {
        throw std::invalid_argument("incomplete or invalid initial IDs");
    }
    std::vector<Gap::result> answers;
    answers.reserve(turns.size());
    for (const auto& turn : turns) {
        if ((int)turn.capacity.size() != agents ||
            std::any_of(turn.capacity.begin(), turn.capacity.end(),
                        [](long long x) { return x < 0; })) {
            throw std::invalid_argument("invalid new capacities");
        }
        auto turn_options = options;
        turn_options.deadline = std::min(options.deadline,
            std::chrono::steady_clock::now() + per_turn);
        session.set_capacities(turn.capacity);
        auto checked = session.evaluate(current);
        std::vector<unsigned char> movable(jobs, 0);
        for (int job = 0; job < jobs; ++job) {
            int agent = checked.agent_of_job[job];
            movable[job] = checked.load_of_agent[agent] > turn.capacity[agent];
        }
        for (int job : turn.also_mutable) {
            if (job < 0 || job >= jobs) throw std::invalid_argument("invalid mutable job");
            movable[job] = 1;
        }
        std::vector<int> mutable_jobs;
        for (int job = 0; job < jobs; ++job) {
            if (movable[job]) mutable_jobs.push_back(job);
        }
        auto answer = session.repair(current, mutable_jobs, turn_options);
        if (answer.repair_state == Gap::repair_status::completed && answer.feasible()) {
            current = answer.option_ids;
        }
        answers.push_back(std::move(answer));
    }
    return answers;
}
```

### 8.11 U11：代理費用と真の目的による外側探索

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct SurrogateStep {
    std::vector<long long> costs; // 全候補ID順の代理費用
    std::vector<int> mutable_jobs; // 今回動かす仕事。重複なし
};
struct SurrogatePlan {
    Gap::result assignment; // 最後に元の候補コストでevaluateした結果
    long long true_cost = 0;
    int accepted_steps = 0;
};

// build_step(current_ids, round) -> SurrogateStep:
//   現在解に合わせて代理費用と近傍を作る。roundは0始まり。
// true_cost(ids) -> optional<long long>:
//   小さいほど良い真の目的値。GAP外の制約違反ならnullopt。
// 初期解はGAP制約と外部制約を両方満たす必要がある。
// 採否は真の目的値だけで決める。代理費用の低下だけでは採用しない。
template<class BuildStep, class TrueCost>
SurrogatePlan solve_with_surrogates(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const int> initial_ids, int rounds,
        std::chrono::milliseconds per_round, Gap::solve_options options,
        BuildStep build_step, TrueCost true_cost) {
    const int agents = (int)capacity.size();
    if (jobs < 0 || (int)initial_ids.size() != jobs || rounds < 0 ||
        per_round.count() <= 0 || std::any_of(capacity.begin(), capacity.end(),
                                            [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid surrogate input");
    }
    Gap solver(jobs, agents, capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    auto session = solver.make_session();
    std::vector<int> current(initial_ids.begin(), initial_ids.end());
    if (!session.evaluate(current).feasible()) throw std::invalid_argument("invalid initial plan");
    std::optional<long long> score = std::invoke(true_cost, std::as_const(current));
    if (!score) throw std::invalid_argument("initial plan violates external constraints");
    SurrogatePlan out;
    for (int round = 0; round < rounds; ++round) {
        if (std::chrono::steady_clock::now() >= options.deadline) break;
        auto run_options = options;
        run_options.deadline = std::min(options.deadline,
            std::chrono::steady_clock::now() + per_round);
        SurrogateStep step = std::invoke(build_step, std::as_const(current), round);
        if ((int)step.costs.size() != session.option_count()) {
            throw std::invalid_argument("surrogate cost length mismatch");
        }
        session.set_option_costs(step.costs);
        auto proposal = session.repair(current, step.mutable_jobs, run_options);
        if (proposal.repair_state != Gap::repair_status::completed || !proposal.feasible()) continue;
        std::optional<long long> next = std::invoke(true_cost, std::as_const(proposal.option_ids));
        if (next && *next < *score) {
            current = std::move(proposal.option_ids);
            score = next;
            ++out.accepted_steps;
        }
    }
    session.reset_costs(); // 戻り値のassignment.objectiveを元コストへそろえる
    out.assignment = session.evaluate(current);
    out.true_cost = *score;
    return out;
}

// 具体例: 同じagentに置いた仕事ペアごとの費用も最小化する。
// pair_cost[i][j] (i<j): 仕事i,jを同じagentに置くと追加する費用。
// 上三角だけを使い、対角・下三角は無視する。負値なら同居ボーナス。
// 真の費用 = 通常の候補費用和 + 同居ペアの費用和。
// 代理費用は「他仕事を現在位置に固定したときの候補別費用」。
// 複数仕事を同時に動かすと誤差が生じるので、上の関数で真の費用を再評価する。
SurrogatePlan solve_pair_penalties(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const int> initial_ids,
        const std::vector<std::vector<long long>>& pair_cost,
        int rounds, std::chrono::milliseconds per_round,
        Gap::solve_options options) {
    if (jobs < 0 || (int)pair_cost.size() != jobs) {
        throw std::invalid_argument("invalid pair cost matrix");
    }
    for (const auto& row : pair_cost) {
        if ((int)row.size() != jobs) throw std::invalid_argument("invalid pair cost row");
    }
    auto build_step = [&](const std::vector<int>& current, int) {
        SurrogateStep step;
        step.costs.reserve(choices.size());
        step.mutable_jobs.resize(jobs);
        std::iota(step.mutable_jobs.begin(), step.mutable_jobs.end(), 0);
        for (const auto& c : choices) {
            long long value = c.cost;
            for (int other = 0; other < jobs; ++other) {
                if (other == c.job || choices[current[other]].agent != c.agent) continue;
                value += pair_cost[std::min(c.job, other)][std::max(c.job, other)];
            }
            step.costs.push_back(value);
        }
        return step;
    };
    auto true_cost = [&](const std::vector<int>& ids) -> std::optional<long long> {
        long long value = 0;
        for (int id : ids) value += choices[id].cost;
        for (int first = 0; first < jobs; ++first) {
            for (int second = first + 1; second < jobs; ++second) {
                if (choices[ids[first]].agent == choices[ids[second]].agent) {
                    value += pair_cost[first][second];
                }
            }
        }
        return value;
    };
    return solve_with_surrogates(jobs, capacity, choices, initial_ids, rounds,
                                 per_round, options, build_step, true_cost);
}
```

### 8.12 U12：複数seedのportfolio

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

// seeds: 試すseedの列。空ならnullopt。
// options: 各seed共通の探索条件。ただしseedだけはseedsの値で上書きする。
// 固定反復で比較するならdeadlineをmaxにする。有限deadlineなら全seedで共通。
// 戻り値: 発見した最良完全解。全試行が失敗、または試行前に期限ならnullopt。
std::optional<Gap::result> solve_seed_portfolio(
        int jobs, const std::vector<long long>& capacity,
        const std::vector<Gap::assignment_option>& choices,
        std::span<const std::uint64_t> seeds, Gap::solve_options options) {
    const int agents = (int)capacity.size();
    if (jobs < 0 || options.iteration_limit < 0 ||
        std::any_of(capacity.begin(), capacity.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("invalid portfolio input");
    }
    Gap solver(jobs, agents, capacity);
    for (const auto& c : choices) {
        if (c.job < 0 || c.job >= jobs || c.agent < 0 ||
            c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid candidate");
        }
        solver.add_option(c.job, c.agent, c.demand, c.cost);
    }
    auto session = solver.make_session();
    std::optional<Gap::result> best;
    for (std::uint64_t seed : seeds) {
        if (std::chrono::steady_clock::now() >= options.deadline) break;
        options.seed = seed;
        auto candidate = best ? session.improve(best->option_ids, options) : session.solve(options);
        if (candidate.feasible() && (!best || candidate.objective < best->objective)) {
            best = std::move(candidate);
        }
    }
    return best;
}
```

### 8.13 U13：構造変更時の再構築とID移し替え

```cpp
#include "generalized_assignment_solver_v12.hpp"
using Gap = generalized_assignment_solver;

struct KeyedChoice {
    long long job_key;    // ターン間で意味を維持する仕事識別子
    long long option_key; // ターン間で意味を維持する候補識別子。全候補で一意
    int agent;           // 今回のcapacity列での番号
    long long demand;    // 今回の需要。変更してよい
    long long cost;
};
struct RebuiltPlan {
    Gap::result assignment; // 今回のjob順・候補登録順の結果
    std::vector<std::optional<long long>> chosen_keys; // job_keysと同じ順。未割当はnullopt
};

// job_keys: 今回存在する仕事の一意な外部識別子。仕事の追加・削除・並べ替え可。
// choices: 今回の候補集合。候補の追加・削除・需要変更をここに反映する。
// previous_choice: 前回のjob_key -> 選択option_key。消えた仕事は無視される。
// raw候補IDではなく外部識別子で引き継ぐ。候補keyは異なる意味へ使い回さない。
// 戻り値: 今回の構造で解いた割当と外部候補key。
RebuiltPlan solve_rebuilt_problem(
        const std::vector<long long>& job_keys,
        const std::vector<long long>& capacity,
        const std::vector<KeyedChoice>& choices,
        const std::unordered_map<long long, long long>& previous_choice,
        Gap::solve_options options) {
    const int jobs = (int)job_keys.size();
    const int agents = (int)capacity.size();
    if (std::any_of(capacity.begin(), capacity.end(),
                    [](long long x) { return x < 0; })) {
        throw std::invalid_argument("negative capacity");
    }
    std::unordered_map<long long, int> job_by_key;
    for (int job = 0; job < jobs; ++job) {
        if (!job_by_key.emplace(job_keys[job], job).second) {
            throw std::invalid_argument("duplicate job key");
        }
    }
    Gap solver(jobs, agents, capacity);
    std::unordered_map<long long, int> option_by_key;
    std::vector<long long> key_by_option;
    for (const auto& c : choices) {
        auto job = job_by_key.find(c.job_key);
        if (job == job_by_key.end() || c.agent < 0 || c.agent >= agents || c.demand < 0) {
            throw std::invalid_argument("invalid keyed candidate");
        }
        int id = solver.add_option(job->second, c.agent, c.demand, c.cost);
        if (!option_by_key.emplace(c.option_key, id).second) {
            throw std::invalid_argument("duplicate option key");
        }
        key_by_option.push_back(c.option_key);
    }
    std::vector<int> initial(jobs, -1), missing_jobs;
    for (int job = 0; job < jobs; ++job) {
        auto previous = previous_choice.find(job_keys[job]);
        if (previous != previous_choice.end()) {
            auto found = option_by_key.find(previous->second);
            if (found != option_by_key.end() && solver.options()[found->second].job == job) {
                initial[job] = found->second;
            }
        }
        if (initial[job] < 0) missing_jobs.push_back(job);
    }
    auto session = solver.make_session();
    auto answer = session.repair(initial, missing_jobs, options);
    if (answer.repair_state == Gap::repair_status::completed && answer.feasible()) {
        // 引き継げた仕事も含めて改善する。完全初期解なので品質を保持する。
        answer = session.improve(answer.option_ids, options);
    } else {
        // 固定負荷の超過、または固定しすぎて解けない場合は全仕事を可変にする。
        std::vector<int> all_jobs(jobs);
        std::iota(all_jobs.begin(), all_jobs.end(), 0);
        answer = session.repair(initial, all_jobs, options);
    }
    RebuiltPlan out;
    out.assignment = std::move(answer);
    out.chosen_keys.resize(jobs);
    for (int job = 0; job < jobs; ++job) {
        int id = out.assignment.option_ids[job];
        if (id >= 0) out.chosen_keys[job] = key_by_option[id];
    }
    return out;
}
```

## 9. 実装

### 9.1 全体の流れ

ライブラリは、容量条件を守れる割当を作り、その割当を移動・組替・部分再構築で改善します。初期解や可変集合がある場合は、その情報を最初に反映します。

1. 候補を仕事ごとの連続領域へ整理し、登録IDと内部番号の対応を作る。
2. `repair` なら固定仕事を評価し、その負荷を引いた残余問題を作る。
3. `improve` / `repair` の初期解が有効なら、最良候補として保持する。
4. profileに応じて容量価格を計算する。
5. 仕事の割当順を決め、1回または複数回の初期構築を行う。
6. 未割当が残る場合は、追加構築・部分LNS・小規模探索・容量優先構築など、条件に合う実行可能化を試す。
7. 完全解と整数下界の一致を確認できれば、残りの改善探索を省く。
8. 対象profileなら、1仕事の移動と2仕事の組替で改善する。
9. `alns` なら、部分破壊と増分regret再挿入を反復する。
10. 最良解を登録候補IDへ戻して返す。部分問題なら固定側と合成する。

profileによるスキップ、期限による停止、完全解未発見によるスキップがあるため、常にすべての段階を実行するわけではありません。

### 9.2 前処理と内部データ

仕事ごとの候補は `offset[job]` から `offset[job+1]` の区間に並びます。基本の並びはagent、費用、需要の順です。各候補は登録IDを保持し、登録IDから内部候補番号へ変換する配列も用意します。これにより、候補登録順と探索に都合のよい並びを分離しています。

割当状態は、各仕事の選択候補、各割当先の負荷、各割当先に所属する仕事列、仕事の所属列内の位置、費用和、未割当数を持ちます。仕事の削除は所属列の末尾と交換して取り除き、移動した仕事の位置も更新します。1仕事の追加・削除で負荷や目的値全体を再集計する必要がありません。

全仕事の候補幅から平均費用幅R、全候補の需要から平均需要D_barを求め、いずれも少なくとも1にします。これらは価格や乱数揺らぎの尺度として使い、費用の絶対値だけに依存した設定を避けています。

### 9.3 整数下界と早期終了

各仕事について、単独で対象agentの容量内に収まる候補の最小費用を足します。仕事同士の容量競合を無視しているので、完全解の費用はこの和以上です。

$$B=\sum_i\min_{o\in O_i,\ d_o\le C_{a_o}}c_o$$

ここでBは整数下界、他の記号は1章と同じです。該当候補がない仕事があれば、この下界による一致判定は使いません。完全解の費用とBが整数で一致した場合は、その問題ではこれ以上改善できないため停止できます。

この判定は有効な初期解、構築後、ALNS反復などで使います。`repair` ではCは固定負荷を引いた残余容量です。浮動小数のラグランジュ下界との近似的一致で止める処理ではありません。

### 9.4 ラグランジュ容量価格

容量制約ごとに非負の価格lambda_aを付けます。混雑している割当先で容量を使う候補ほど、見かけの費用を高くして構築を誘導します。

$$s_o=c_o+\lambda_{a_o}d_o$$

lambda_aは割当先aの容量価格、s_oは候補oの価格込み費用です。容量制約を外し、仕事ごとに最小のs_oを選んだときの緩和値は次です。

$$B(\lambda)=\sum_i\min_{o\in O_i}(c_o+\lambda_{a_o}d_o)-\sum_a\lambda_a C_a$$

容量内の解なら価格付き需要和から価格付き容量を引いた値は0以下なので、この値は真の最適費用の下界になります。実装は独立に選んだ候補の負荷を集計し、容量超過方向なら価格を上げ、余る方向なら下げ、0未満へは下げません。

更新回数は候補数に応じて4～24回、更新幅はRとD_barを基準に反復ごとに減らします。観測した最良の緩和値と、そのときの価格を保持します。期限で1回も計算できなければ、下界は負の無限大のままです。

### 9.5 初期構築：難しい仕事を先に割り当てる

仕事の候補が少ない、需要が容量に対して大きい、最良候補と次善候補の費用差が大きい場合は、後回しにすると選択肢を失いやすくなります。これを難易度にまとめて、難しい仕事を先に処理します。

仕事iの候補数をk_i、候補の最大需要比をr_i、価格込み最良・第2候補の差をq_iとすると、基本の難易度は次です。

$$r_i=\max_{o\in O_i} d_o/\max(1,C_{a_o})$$

$$h_i=3/\max(1,k_i)+2r_i+q_i/(R+1)$$

候補がない仕事ではr_iを0とします。第2候補がない場合のq_iは4Rを使います。複数startでは難易度へ小さい乱数を加えて順序を変えます。基礎の難易度は同じsolve内で再利用します。

仕事を割り当てる候補の評価は、元費用、容量価格、割当後の余裕の少なさを足したものです。割当後残容量をu_oとすると、容量圧力項は次です。

$$p_o=0.12R\,d_o/(u_o+D_{bar})$$

実行可能な候補だけを比較します。決定的な初回構築では最良1個を保持し、ランダム化構築では上位3個を保持して、少量の乱数で第2・第3候補を選ぶこともあります。乱数を値として使わない区間では、状態だけを必要回数進める処理があります。

`greedy` は基本構築1回、`regret` 以上は候補数に応じて2～9回です。完全解がない場合、`lagrangian` 以上は追加で最大100回の構築を試します。各段階で期限を確認しますが、1回の構築の内部すべてに時刻検査があるわけではありません。

### 9.6 未割当の実行可能化

まず、空き容量へ入れられる候補がないかを探します。なければ、入れたい割当先にいる1仕事を別の候補へ移し、空きを作るejectionを試します。追い出す相手は各候補先で最大128仕事を調べます。同じ割当先の省容量モードへの変更も対象です。

`lagrangian` 以上でまだ未割当があれば、未割当仕事1個と、その候補先に所属する周辺仕事を小さく取り出す部分LNSを試します。各仕事の実行可能な最良・第2候補を調べ、後回しにした損失であるregretの大きい仕事を優先して戻します。戻せなければ元の割当へ復元します。最大1,000回で、可変集合の大きさは概ね8～32仕事を起点に現在の仕事数へ収めます。

これは次のALNS用増分repairとは別の処理です。未割当の解を完全解へ近づけることが目的であり、費用が増えても未割当数を減らすことを優先します。

### 9.7 小規模fallbackと容量優先の救済

仕事数20以下で構築に失敗し、時間が残る場合は、候補数の少ない仕事から深さ優先の分枝限定を行います。残り仕事の最小費用和で枝刈りし、上限は300万nodeです。期限でも打ち切ります。

完全解がすでにあるときには呼ばれず、探索を途中で打ち切ることもあるため、小規模入力すべての最適性を保証しません。

その後も未割当が残り、`lagrangian` 以上で時間がある場合は、容量消費を強く価格化した決定的構築を1回試します。未割当数、次いで費用が改善した場合だけ採用します。この救済価格はその構築だけで使い、以後の改善探索では通常の価格を使います。

### 9.8 relocateと2仕事のexchange

完全解があり `local_search` 以上なら、仕事ごとの候補を整数費用順に並べた索引を作ります。同じ索引を1仕事移動と2仕事の組替で共有します。

relocateは仕事順をランダム化し、現在より安い候補を安い順に調べ、最初の実行可能候補へ移します。費用が下がらない位置に来たら、それより後も改善しないため走査を止めます。同じagent内のモード変更も可能です。

exchangeは仕事を1つ選び、その仕事の候補を一様に4回引いた中の最安候補を移動先の入口にします。その先にいる相手仕事を選び、2仕事を外して最初の仕事を移した後の容量で、相手の最良の実行可能候補を探します。相手の移動先は最初の仕事の元agentに限定せず、任意の候補先を使えます。

2仕事の合計費用が厳密に下がる場合だけ実行します。最初の仕事が同じagent内でモードだけを変える2仕事操作は対象外ですが、相手仕事の省容量モード変更は候補になります。試行上限は候補数の3倍を起点に1,000～100万回へ収めます。

### 9.9 ALNS：部分破壊と再構成

完全解があり、`profile::alns` で反復予算が正なら、現在解と最良解を分けて保持します。1回の反復では、選んだ仕事を外し、同じ仕事をすべて再挿入します。

破壊する仕事の選び方は2種類で、等確率で選びます。

- 一様ランダムに仕事を選ぶ。
- 安い候補先を持つ仕事から、その先に現在いる仕事を拾い、さらにその仕事の安い候補先へ連鎖する。足りない分は一様ランダムに補う。

公開profile名は `alns` ですが、operatorの適応重み更新は行いません。仕事の重複はstamp配列で防ぎ、長い探索ではstampの上限も処理します。

再挿入の候補評価は、元費用に `repair_price_weight * price * demand` を加えたものです。仕事ごとにこの評価順を一度整列し、再挿入時は実行可能な先頭2候補が見つかれば、その仕事の最良値とregretを確定できます。第2候補がない仕事には8Rを使い、小さい仕事別の乱数を加えて選択順を揺らします。

### 9.10 増分regretとwatcher

1回の再挿入中では、仕事を追加するだけなので各agentの負荷は増える方向にしか変わりません。以前は実行不能だった候補が、新しく実行可能になることはありません。

そのため、ある仕事の上位2候補が引き続き実行可能なら、最良値とregretを計算し直す必要はありません。実装は上位候補のagentから、その候補を使う仕事を引けるwatcherを持ち、直前に負荷が増えたagentに関係する仕事だけを調べます。上位候補が実行不能になった場合だけ、その仕事の候補列を再走査します。

watcherにはversionを付け、候補が変わった後に残る古い監視記録を無視します。割当済み仕事のregretは負の無限大にして、次の最大regret選択から除きます。どれかの仕事を戻せなくなった場合は、その反復で取り出した仕事を元へ復元します。

これは実際に必要な再計算を絞る仕組みで、最悪時の走査が変更1件に比例する保証ではありません。多数の上位候補が同じagentへ集中すると再計算も増えます。

### 9.11 悪化受理・冷却・最良解の保持

現在解に対する費用差をdelta、今回取り出した仕事数をk、反復番号をt、`alns_temperature` をtauとすると、温度は次です。

$$T_t=R\sqrt{k}\left(\tau/(1+0.0005t)+0.002\right)$$

費用が下がるか同じなら受理し、悪化する場合も次の確率で受理します。

$$P=\exp(-\mathrm{delta}/T_t)$$

deltaは再構築後費用から直前費用を引いた値です。受理しない場合は元の仕事割当へ戻します。現在解が悪化を受理しても、最良解は別に保持し、最良費用を下げたときだけ更新します。周期的なrelocateも現在解へ適用し、その結果が最良なら保存します。

反復上限・期限・整数下界一致のいずれかで終了し、現在解ではなく最良解を返します。乱数生成は内部の64bit状態を使い、各solve系呼出しのseedから初期化します。

### 9.12 repair・動的更新・結果復元の実装

`repair` は、固定仕事の有効ID・費用・負荷を先に集計します。固定側が不正または容量超過なら、可変仕事を探索せず状態を返します。正常なら、固定負荷を容量から引き、可変仕事の候補だけを連続な部分問題へコピーします。

部分問題の候補は登録IDを保持したままです。探索後は、可変仕事の結果を元の仕事番号へ戻し、固定側の費用・負荷と足し合わせます。固定負荷と残余探索の両方がそれぞれ容量内なので、合成した負荷も元容量内です。有限下界には固定費用を足します。

`evaluate` は候補IDの範囲と所属仕事を検査し、使える候補の費用・負荷を足します。未割当ペナルティを自動追加せず、容量超過も修正しません。このためデバッグや変更後の再評価に利用できます。

コスト・容量setterはsession内の現在値を書き換え、統計更新が必要なことを記録します。登録ID対応や仕事の候補区間は変わりません。`solve` / `improve` の入口で必要な統計を更新し、`repair` ではその時点の可変候補・残余容量から統計を作ります。

前処理とsession保持データの基本サイズは `O(N+A+E)` です。探索では候補整列用索引、現在解・最良解、再挿入workspaceなどの一時領域も使います。大きいruinを指定するとwatcherなどの作業量も増えるため、仕事数だけでなく候補数と可変集合の大きさも測って判断してください。
