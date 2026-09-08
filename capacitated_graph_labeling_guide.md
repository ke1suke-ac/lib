# 容量制約付きグラフラベリング ユーザーガイド

対象ソース: `capacitated_graph_labeling_v22.hpp`。C++20の単一ヘッダを利用するためのガイドです。

問題をモデル化するには1〜4章、コードへ組み込むには5〜8章、探索処理を理解するには9章を参照してください。8章の関数例は、入力から結果の利用までを一つのまとまりとしてコピーできます。

## 1. 何を解くライブラリか？

### 1.1 「何を、どこへ割り当てるか」を決める

このライブラリは、**各対象を、用意したグループのどれか一つへ割り当てる問題**を扱います。対象を「頂点」、割当先の番号を「ラベル」と呼びます。

例えば、仕事を担当者へ割り当てるなら、頂点は仕事、ラベルは担当者です。各仕事には工数があり、担当者ごとの総工数に上限を置けます。「仕事Aは担当者0に頼むと安い」という個別の費用と、「仕事Aと仕事Bは同じ人に頼むと連携しやすい」という対象間の関係を、同時に評価します。

同じ枠組みで、データをサーバーへ分ける、領域へ状態を割り当てる、作業を時間帯へ置く、といった問題も表せます。入力するラベル数は事前に決めます。ラベルが未使用でもよいか、全ラベルを使うかは、下限の指定で決まります。

### 1.2 目的関数

求めたいのは、次の合計コストを小さくするラベル列です。

$$\min_{\ell} C(\ell),\qquad C(\ell)=\sum_{v=0}^{N-1}U(v,\ell_v)+\sum_{e=0}^{M-1}P_e(\ell_{u_e},\ell_{v_e})$$

| 記号 | 意味 | 利用者が決めるものか |
|---|---|---|
| $N$ | 頂点数。割り当てる対象の個数 | 入力 |
| $K$ | ラベル数。割当先の個数。1以上 | 入力 |
| $M$ | 辺数。評価する対象間の関係の個数 | 入力 |
| $v$ | 頂点番号。$0$から$N-1$ | 入力上の番号 |
| $e$ | 辺番号。$0$から$M-1$ | 入力配列の順序 |
| $u_e,v_e$ | 辺$e$の両端の頂点番号 | 入力 |
| $\ell_v$ | 頂点$v$へ割り当てるラベル。$0$から$K-1$ | 求める値 |
| $\ell$ | 全頂点のラベル列$(\ell_0,\ldots,\ell_{N-1})$ | 求める解 |
| $U(v,a)$ | 頂点$v$だけをラベル$a$へ置く単項コスト | 入力 |
| $P_e(a,b)$ | 辺$e$の一方をラベル$a$、他方を$b$へ置く二項コスト | 入力するコスト規則 |
| $a,b$ | ラベルを表す引数。ともに$0$から$K-1$ | 関数の引数 |
| $C(\ell)$ | ラベル列全体の目的値。小さいほどよい | 解から計算される値 |

第1項は「個々の割当の都合」、第2項は「関係する2対象を同時に割り当てたときの都合」です。関係がない対象対には、辺を入力する必要がありません。入力した辺は、それぞれ一度ずつ合計します。

例えば、4仕事を2人へ割り当てるとします。仕事0と1を別担当にすると10の連携費用、仕事2と3を別担当にすると8の連携費用がかかるなら、その2組を辺にします。さらに、仕事ごとの担当費用を単項コストにします。「0と1を同じ人へ置く」という関係と、「仕事0だけは担当者1が安い」という希望が競合するとき、その合計で割当の良さを比べます。

### 1.3 必ず守る制約

各頂点は分割せず、一つのラベルへ丸ごと置きます。頂点$v$の需要量を$d_v$、ラベル$a$の総需要量を$W_a$、割当頂点数を$n_a$とすると、次を満たします。

$$W_a=\sum_{v:\ell_v=a}d_v,\qquad L_a\leq W_a\leq R_a$$

$$n_a=\sum_{v=0}^{N-1}[\ell_v=a],\qquad A_a\leq n_a\leq B_a$$

| 記号 | 意味 |
|---|---|
| $d_v$ | 頂点$v$の非負整数の需要量。工数、面積、荷物量など。省略すると1 |
| $W_a$ | ラベル$a$に割り当てた需要量の合計 |
| $L_a,R_a$ | ラベル$a$の需要量の下限・上限。省略時は0・全頂点の総需要量 |
| $n_a$ | ラベル$a$へ割り当てた頂点の個数 |
| $A_a,B_a$ | 頂点数の下限・上限。省略時は0・$N$ |
| $[\text{condition}]$ | 条件が真なら1、偽なら0となる記号 |

需要量と頂点数は別に指定できます。例えば「担当者ごとに工数は20以下、担当件数は3〜6件」とできます。需要量0の仕事も1件として数えます。需要量をすべて1にすると、需要量制約と件数制約は同じ量を制限します。

頂点ごとに「このラベルへ固定」「この集合のラベルだけ許可」も指定できます。固定指定がある頂点では、その固定ラベルが優先されます。固定先と許可集合の両方を満たす必要がある業務ルールなら、入力側で両者の整合性を確認します。

### 1.4 二項コストの代表形

**同じグループかどうか**だけを見るPotts型は、次の式です。

$$P_e(a,b)=w_e[a\neq b]$$

$w_e$は辺の重みです。正なら別グループへ分かれたときに費用を払い、負なら分かれたときに報酬を得ます。ゼロならその辺は目的値に影響しません。

**割当先の組合せ**まで見る場合は、共通の表$D$を使えます。

$$P_e(a,b)=w_eD(a,b)$$

$D(a,b)$はラベル$a,b$の組合せ単価です。位置間の距離、時間帯の順序違反、設備間の通信単価などを入れます。$D(a,b)$と$D(b,a)$が違っても構いません。その場合は辺の両端の順序に意味があります。

辺の種類ごとに異なる表を使うなど、辺番号・辺データ・両端ラベルから値が一意に決まる任意の二項コストも指定できます。一方、「3対象を同時に見た条件」「全グループの形状」などは、この単項・二項の和で表せるかを別途検討する必要があります。

## 2. 厳密解アルゴリズム

この章の方法は、対象問題を見て**別途選ぶ解法**です。本ライブラリが条件を自動判定し、これらへ切り替えるわけではありません。条件が合い、計算が完了すれば、最適解を求められます。本ライブラリの結果と同じ最適値になる場合もあります。

### 2.1 まず確認する条件

| 問題の条件 | 検討する厳密解法 | 適用時に確認する点 |
|---|---|---|
| 二項コストがなく、ラベル間を結ぶ容量・件数制約もない | 頂点ごとに許可ラベルの最小単項コストを選ぶ | 頂点間の独立性が必要 |
| 二項コストがなく、全需要量が1、ラベル別の人数上下限がある | 下限付き最小費用流 | 需要量と件数の制約を人数の上下限へ統合できる |
| 二項コストがなく、各位置に1個ずつ置く | 線形割当・最小費用流 | 真の二項コストがあるQAPには、この帰着を使えない |
| 2ラベル、二項コストが劣モジュラ、全体容量・人数制約がない | $s$-$t$最小カット | 固定・許可条件は扱える。均等分割制約を加えると同じ帰着では解けない |
| 順序付きラベル、ラベル差の凸な二項コスト、全体容量制約がない | 多層グラフによる最小カット | 一般のラベル行列や任意の距離表まで対象が広がるわけではない |
| 相互作用のグラフが木・森、全体容量・人数制約がない | 木DP | 容量を加えるなら、部分木ごとの使用量も状態に必要 |
| 可変頂点数が少ない | 全探索・分枝限定 | 固定部分を除いたラベル組合せ数が計算可能か |
| 整数の単項・二項表と容量を持つ小〜中規模問題 | 整数計画・CP-SAT | 制限時間内に最適性が証明されるとは限らない |

### 2.2 単項コストだけなら、割当・フローを検討する

全需要量が1の場合、1頂点から1単位をいずれかのラベルへ流すモデルを作れます。頂点から許可ラベルへの辺に単項コストを置き、ラベル側の流量に人数上下限を置きます。需要量上下限と件数上下限の両方がある場合、実効下限は両者の最大、実効上限は両者の最小です。下限付き流量は、先に下限分を割り当てて各点の供給量・需要量を調整する形へ変換します。割当のフロー表現は[OR-Toolsの公式解説](https://developers.google.com/optimization/flow/assignment_min_cost_flow)を参照できます。

仕事ごとに需要量が違い、その仕事を分割できない場合は要注意です。需要量ぶんの流量を流すだけでは、1仕事が複数ラベルへ分割される可能性があります。元の「1頂点を1ラベルへ」という問題とは一致しません。二項関係が加わる場合も、通常の割当フローだけでは目的関数を表せません。

### 2.3 2ラベルの劣モジュラコスト

各2頂点間のコストを同じ端点順へそろえ、多重辺を合算した表$P$について、次が成り立つことが十分条件です。

$$P(0,0)+P(1,1)\leq P(0,1)+P(1,0)$$

これは、ラベルをそろえる側の費用が、分ける側に比べて高すぎない条件です。非負重みの2ラベルPotts型は該当します。単項コストと固定・許可条件を加えても、全体の容量・人数制約がなければ最小カットで厳密に解けます。[Kolmogorov・Zabihの原論文](https://www.cs.cornell.edu/~rdz/Papers/KZ-PAMI04.pdf)が適用条件を示しています。

「2グループへ同数ずつ入れる」といった条件は、通常の最小カットに含まれません。まず容量を外して厳密解を求め、その解が元の容量も満たしていたなら、それは元問題に対しても最適です。容量に違反していた場合、その解をそのまま採用することはできません。

### 2.4 順序付きラベルと凸コスト

ラベルが高さ・深度などの順序を持ち、二項コストが非負重み付きの$|a-b|$や$(a-b)^2$などの凸な差分費用である場合、ラベルを段階に展開したグラフの最小カットを検討できます。単項コストは任意に設定できます。これは[石川の凸事前分布に対する厳密最適化](https://waseda.elsevierpure.com/en/publications/exact-optimization-for-markov-random-fields-with-convex-priors/)の対象となる代表形です。

全体のラベル別容量や件数制約を追加しても、そのまま同じ解法が適用できるとは限りません。また、費用を一定値で打ち切ると凸性を失うことがあります。「ラベル間距離」という名前だけで条件を満たすとは判断しないでください。

### 2.5 木・森なら木DP

自己辺を単項項へ、多重辺を同じ頂点対の表へまとめた後の相互作用が木なら、ある頂点のラベルを固定すると、その子部分木を独立に解けます。各頂点・各ラベルについて「その部分木の最小コスト」を保持し、子のラベルを全て比較して足し合わせます。二項コストが定数時間で計算でき、全体容量制約がなければ、計算量は$O(NK^2)$です。非対称二項表でも元の端点順を守って評価できます。[木上のmin-sum法を説明する原論文](https://people.csail.mit.edu/tommi/papers/WaiJaaWil_TRMAP_arxiv.pdf)も参照できます。

ラベル別の総需要量・人数を制限すると、兄弟部分木の選択が互いに影響します。必要な使用量をDP状態へ追加できれば厳密解法になりますが、容量値とラベル数に応じて状態数が大きくなります。木幅が小さいグラフにも状態を持つDPを広げられますが、木幅や容量次元が増えると計算量が増大します。

### 2.6 小さい部分問題と汎用の厳密解法

変更してよい頂点集合を$S$とすると、単純全探索は最大$K^{|S|}$通りです。各頂点の許可ラベルだけに絞れば組合せ数を減らせます。範囲外への辺は単項コストへ加え、範囲外が使っている需要量・人数を容量から差し引くことで、小さい部分問題として厳密に解けます。厳密に解いた部分問題の最適性は、その固定された範囲外状態に対するものです。

一般形でも、各頂点・ラベルの選択を0/1変数にし、辺ごとのラベル組合せを補助変数で表せば整数計画として定式化できます。CP-SATでは整数値として正確に表現できるモデルが前提です。有限の小数を整数化する場合は、丸めで別問題にならないことと桁あふれに注意します。最適性が必要なら、得られた解の有無に加えて、最適であることが証明された終了状態を確認します。[OR-Toolsの終了状態の説明](https://developers.google.com/optimization/cp/cp_solver)では、実行可能解の発見と最適解の証明が区別されています。

以上の条件に当てはまらない、または厳密解法が時間・メモリ予算へ収まらない場合に、本ライブラリを候補にします。厳密解法で作った実行可能解を、本ライブラリの改善処理へ渡すこともできます。

## 3. 差分更新

### 3.1 前の解は再利用できる。内部キャッシュは毎回構築する

公開APIは問題データと関数から構成され、利用者が保持するsolverインスタンスはありません。呼出し間で保持するのは、主に**問題データ、二項コストのデータ、現在の完全なラベル列**です。

前の解が更新後の問題でも実行可能なら、`improve_capacitated_graph_labeling`へ渡せます。初期解を作る処理を省いて、そこから改善を始めます。一部の頂点だけ変えたいなら、`improve_capacitated_graph_labeling_subset`を使います。

いずれの呼出しも、その時点の問題からCSR隣接リスト、必要なラベル別リスト、Potts差分表などを構築します。**辺の変更だけを内部キャッシュへ適用する公開API、温度・乱数状態をそのまま再開するAPIはありません。**

### 3.2 後から変更できるもの

以下は、1回の呼出しが終了してから次を呼ぶまでの間に変更します。実行中の問題・行列・policy参照先は変更しません。

| 変更するもの | 次の呼出しへ反映できるか | 前のラベル列と追加作業 |
|---|---|---|
| 単項コスト | できる | 制約が同じなら前の解は実行可能。目的値は新しい値で再評価 |
| 辺重み、辺種別、行列値、custom policyの係数 | できる | コストだけの変更なら実行可能性は変わらない |
| 辺の追加・削除、端点の変更 | できる | CSRは再構築。辺番号依存の外部配列も新しい辺順へ対応させる |
| 需要量、容量上下限、件数上下限 | できる | 前の解が新しい制約を満たすか再検査 |
| 固定ラベル、許可ラベル | できる | 前の解が新しい指定を満たすか再検査 |
| 変更可能頂点集合 | 毎回別の集合を渡せる | 範囲外はその呼出しの初期ラベルで固定。集合に固定頂点や重複を含めない |
| 頂点数の増減 | 入力を組み直せばできる | 頂点番号、全頂点配列、辺端点、前の解をそろえる。自動補完・削除・番号変換はない |
| ラベル数の増減 | 入力を組み直せばできる | 容量配列、単項表、許可表、二項表、固定先、前の解を新しい番号へそろえる |
| 二項コストの型そのもの | 次の関数呼出しで別の型を渡せる | 内部エンジンの型も変わる。保持済みエンジンの型を変更する操作ではない |

問題構造体の同じオブジェクトを編集しても、毎回新しいオブジェクトを作っても構いません。どちらでも内部探索データは呼出しごとに作られます。大きな入力配列の不要なコピーを避けたいときは、同じ問題オブジェクトを編集して使います。

### 3.3 ターン制問題での手順

1. 新しいターンの辺・費用・制約を問題データへ反映する。
2. 前のラベル列を`is_feasible_capacitated_graph_labeling`で検査する。
3. 実行可能なら全体`improve`、または変更範囲を決めてsubsetを呼ぶ。
4. 実行不可能なら、問題固有の処理で実行可能解を作るか、`solve`から作り直す。
5. 成功した結果を採用し、次ターン用のラベル列として保持する。

名前にrepairが含まれていても、subsetは**制約違反を起こした初期解を修復する機能ではありません**。入力の完全解が実行可能である必要があります。8.9は実行可能なら前の解を使い、そうでなければ構築を試す関数例、8.10はsubsetの成功差分を適用する関数例です。

### 3.4 subsetで省ける仕事と、省けない仕事

変更可能頂点集合を$S$、少なくとも一端が$S$にある辺集合を$E_S$とします。subsetが比較する可変部分は、次です。

$$C_S(\ell)=\sum_{v\in S}U(v,\ell_v)+\sum_{e\in E_S}P_e(\ell_{u_e},\ell_{v_e})$$

範囲外だけの単項・辺コストは変化しないため、差分計算から除けます。境界辺は$E_S$に含まれ、外側のラベルとの関係を評価します。全体の需要量・人数も維持します。

探索候補とPotts差分表は$S$に限定され、返却値は変更頂点と新ラベルだけです。ただし、CSR構築、全体の実行可能性確認、完全ラベル列の内部コピーは残ります。準備に少なくとも$O(N+M+K)$が必要で、Potts表を使う場合は$O(|S|K)$の確保・初期化も加わります。変更が1辺でも、1辺だけの更新時間で再開できるわけではありません。

### 3.5 目的値差分の基準を取り違えない

subsetの`objective_delta`は、**更新後の同じ問題における「新しいラベル列の目的値 − 渡したラベル列の目的値」**です。前ターンから費用表や辺を変えた場合、その変更自体による目的値差分は含みません。

更新後の目的値を管理するときは、まず前のラベル列を新しい問題で再評価し、それへsubset差分を加えます。前ターンの目的値へそのまま加えると、費用変更分を落としてしまいます。ライブラリに含めていない本来のスコア項があるなら、それも利用者側で評価します。

## 4. ユースケース

4.1〜4.8は問題のモデル化、4.9〜4.11はそれらに組み合わせる実行方法です。同じ数理モデルになる業種名だけの違いはまとめています。6章と8章も同じ番号で対応します。

| 番号 | 用途 | 頂点 → ラベル | 主に決めるもの |
|---|---|---|---|
| 1 | グラフ分割・seed付き領域分割 | 対象 → グループ | 分離費用、グループ数、サイズ、固定seed |
| 2 | Max-Cut・衝突の少ない色分け | 対象 → 色 | 同色罰金、色数、色別サイズ |
| 3 | 正負の関係を持つクラスタリング | 対象 → クラスタ | 同居・分離の希望、違反罰金 |
| 4 | 容量付き割当・負荷分散 | 仕事 → 担当先 | 工数、件数、適格性、個別費用、連携費用 |
| 5 | 順序を考慮した時間帯割当 | 作業 → 時間帯 | 各帯の容量、割当費用、先行関係 |
| 6 | 距離を使う領域・状態ラベリング | サイト → 状態・位置 | 単項費用、ラベル間距離、領域容量 |
| 7 | 種類の異なる関係の同時評価 | 対象 → 役割・配置先 | 辺種別と種別ごとの組合せ費用 |
| 8 | 疎なQAP・一対一配置 | 要素 → 固有位置 | 相互作用量、位置間費用、各位置1個 |
| 9 | 全体の再最適化 | 現在の問題 → 更新後の完全解 | 前の解、更新した費用・制約、予算 |
| 10 | 局所的な再最適化 | 現在の問題 → 部分変更 | 変更範囲、前の解、境界の関係 |
| 11 | 複数seedからの探索 | 同じ問題 → 複数の候補解 | 試行数、総時間、seedの開始値 |

### 4.1 グラフ分割・seed付き領域分割

頻繁に通信する処理や、互いに似たデータを、なるべく同じグループへまとめます。辺の重みは「別グループへ分けたときの費用」です。データの分散配置、処理のチーム分割、画像のseed付き領域分割などを想像すると分かりやすくなります。

対象数、グループ数、分離費用を指定します。全てを1グループへ入れる解を避けたいなら、人数・需要量の上下限や、各グループへ固定するseedを指定します。seedは領域の中心を決める既知の対象で、同じラベルへ複数置くこともできます。全ラベルにseedを置けば、各ラベルは必ず使われます。

8.1の例は人数をできるだけ均等にし、必要ならseedも固定します。seedだけで分割したい場合は、一般APIで人数上下限を省略します。seedへ属する領域の連結性は保証されません。

### 4.2 Max-Cut・衝突の少ない色分け

同時に使うと干渉する機器、同じ枠に置くと競合する対象を、別の色へ分けます。2色ならMax-Cut、多色ならMax-k-Cutやsoft coloringとして使えます。

色数と「この2対象が同色になったときの罰金」を指定します。8.2では色ごとの対象数を均等にします。目的は衝突罰金を少なくすることです。全ての隣接対象を必ず異色にする制約はないので、衝突ゼロが必須なら返却後に確認します。

### 4.3 正負の関係を持つクラスタリング

文書や顧客などに「同じ群へ入れたい組」と「別の群に分けたい組」の両方がある問題です。例えば、類似した文書はまとめつつ、対立する属性を持つ対象は分けたい場合に使います。

各関係について希望の向きと、希望に反したときの非負罰金を指定します。クラスタ数は固定し、必要ならクラスタサイズの上下限や、特定クラスタへの事前費用も指定します。希望が互いに矛盾する入力も扱えますが、全罰金をゼロにできるとは限りません。空クラスタを避けるには人数下限を1以上にします。

### 4.4 容量付き割当・負荷分散

仕事を担当者、処理を機械、要求をサーバーへ割り当てます。各対象には工数・CPU量など1種類の需要量があり、割当先の容量を消費します。重い仕事を少数持つ担当者と、軽い仕事を多数持つ担当者を区別するため、工数と担当件数を同時に制限できます。

担当先数、仕事ごとの需要量を指定し、必要な容量・件数上下限を与えます。得意不得意や料金は仕事と担当先の個別費用にします。資格・対応機種は許可先の集合、既に確定した担当は固定先です。仕事間の引継ぎ費用なども関係として追加できます。

需要量は担当先によらず同じです。担当者によって処理時間が違う場合、その違いは個別費用には入れられますが、容量消費量を担当先別に変えることはできません。CPUとメモリを同時に制約するような複数資源容量も直接の対象外です。

### 4.5 順序を考慮した時間帯割当

作業を日・工程・時間帯へ置き、「作業AをBより前に置きたい」という希望を評価します。例ではAとBを同じ時間帯に置くと1段階、AをBより後に置くと逆転幅に応じた罰金を払います。

作業ごとの容量消費量、時間帯ごとの容量、置きやすさを表す単項費用、先行・後続の組と違反の重さを指定します。辺には向きがあり、AとBを取り違えると意味が逆になります。

各作業は一つの時間帯に収まるものとして扱います。複数時間帯を連続占有する作業の開始時刻決定や、順序を必ず守るスケジューリングを、そのまま解くものではありません。

### 4.6 距離を使う領域・状態ラベリング

各画素・領域・観測点へ状態を割り当て、個別の観測への適合度と、隣り合う状態の滑らかさを両立させます。近い状態への変化は小さく、遠い状態への変化は大きく罰する問題です。配置先どうしの輸送距離・通信遅延を評価する用途にも使えます。

頂点ごとのラベル費用、辺ごとの関係の強さ、全ラベル対の距離表を指定します。面積をそろえるなら頂点の面積を需要量にし、ラベル別の総面積を制限します。入力表は一般の組合せ費用でもよいですが、数学的な距離として解釈するには非負性・対称性・三角不等式などを入力側で確かめます。

### 4.7 種類の異なる関係の同時評価

全ての辺に同じ関係規則を当てはめられない場合です。例えば、ある辺は連携、別の辺は競合、さらに別の辺は役割の順序を表すとします。単に辺の重さを変えるだけでは、これらの組合せ規則を表せません。

辺ごとに種類を付け、種類ごとにラベル対の費用表を与えます。単項費用や人数上下限も併用できます。辺番号ごとに個別の係数を持つ規則へ広げることもできます。ただし、費用はその辺と両端ラベルから決まり、他の頂点の割当や実行中の時刻に依存しないものにします。

### 4.8 疎なQAP・一対一配置

設備や部品を、用意した同数の位置へ1個ずつ置きます。互いに大量の物をやり取りする2設備は、輸送費が小さい位置対へ置きたい、という問題です。

要素数と位置数を等しくし、各位置に必ず1個だけ入る件数制約を指定します。要素間の相互作用量と、位置間の距離・費用表を与え、その積を合計します。各要素の置きやすさも単項費用として追加できます。有向の流量・非対称な位置間費用も指定できます。

全要素対が相互作用する大規模な密QAPでは、候補の評価も重くなります。相互作用が疎な問題や、他のラベリング処理へ組み込みたい場合が対象候補です。相互作用がなければ2章の線形割当も検討します。

### 4.9 全体の再最適化

前ターンや別の解法で得た割当を出発点にして、全ての自由な頂点を見直します。費用だけが少し変わったターンや、厳しい制約を満たす初期解を外部で作れる場合に適します。

更新後の問題、前の完全解、再探索に使える時間・反復数を指定します。前の解が新しい制約を満たす場合、その解を起点にします。満たさない場合は実行可能解の構築が必要です。8.9の関数はこの分岐を明示し、前の解を利用できたかも返します。

### 4.10 局所的な再最適化

直前に変えた辺の周囲など、小さい領域だけを見直し、残りの割当を保持します。ターン制の操作、外側の探索で選んだ領域の再配置などに使います。

現在の完全解と、変更してよい頂点の集合を指定します。範囲外との境界関係と全体容量も評価されます。異なるラベルの頂点を範囲へ含めると、交換による調整ができます。範囲が小さすぎると実行可能な変更が一つもなく、何も変わらず終了する場合があります。

### 4.11 複数seedからの探索

同じ問題を異なる乱数の出発点で何回か解き、目的値が最も小さい成功解を選びます。初期状態のばらつきが大きい問題で試す実行方法です。

試行回数、全体の時間、最初のseedを指定します。8.11では残り時間を残り試行数へ分配し、共通の終了時刻を渡します。回数を増やすと1回の探索時間が減り、準備処理も繰り返すため、常に品質が上がるわけではありません。

## 5. 利用準備

### 5.1 ヘッダと公開API

必要なのは`capacitated_graph_labeling_v22.hpp`です。ACLや追加のハッシュマップには依存しません。ヘッダは`<bits/stdc++.h>`、`__uint128_t`、`__INCLUDE_LEVEL__`を使うため、GNU拡張とlibstdc++が利用できるGCC系のC++20環境を前提にします。MSVCへの無修正の移植は前提にしていません。

```cpp
#include "capacitated_graph_labeling_v22.hpp"
```

利用者が呼ぶsolverコンストラクタや`.solve()`メソッドはありません。次の3点を用意して、自由関数を呼びます。

```cpp
capacitated_graph_labeling_problem<long long> problem; // 問題の入力
capacitated_potts_cost<long long> pair_cost;           // 二項コスト規則
capacitated_graph_labeling_options options;           // 時間・乱数など
// problemの必須項目を設定した後:
// auto result = solve_capacitated_graph_labeling(problem, pair_cost, options);
```

公開型は、問題、辺、Pottsコスト、行列コスト、options、通常結果、subset結果、statusの8種類です。公開関数は、`solve_capacitated_graph_labeling`、`improve_capacitated_graph_labeling`、`improve_capacitated_graph_labeling_subset`、`evaluate_capacitated_graph_labeling`、`is_feasible_capacitated_graph_labeling`の5つです。

### 5.2 数値型を決める

`capacitated_graph_labeling_problem<Cost>`の`Cost`が、辺重み・単項費用・目的値・差分値の型です。省略時は`long long`です。負の差分が必要なので符号付き型に限られます。まず整数の`long long`で正確に表せる単位を選ぶと、目的値の検証が容易です。

浮動小数点の`double`なども使えますが、繰り返しの差分加算と全体再計算に丸め差が生じ得ます。比較には目的値の規模に応じた許容誤差を使います。NaN・無限大は入力しません。整数でも、最終和だけでなく、積・中間和・差分の全てが型へ収まる必要があります。

需要量と容量は`Cost`によらず`long long`です。需要量が小数なら、意味を保てる整数単位へそろえます。頂点番号、ラベル番号、辺番号、CSRの添字は`int`です。少なくとも`N*K`、行列policyの`K*K`、辺数、CSR要素数が`int`で表せる規模に制限してください。メモリ容量の制約は別にあります。

### 5.3 問題のパラメータ一覧

問題型のコンストラクタ引数はありません。デフォルト構築後に公開メンバーを設定します。全vectorの初期値は空です。表の「省略時」は空vectorの意味を表します。配列は0始まりです。

| メンバー | 型 | 必要サイズ・デフォルト | 設定する場面と内容 |
|---|---|---|---|
| `vertex_count` | `int` | 初期値0、`N >= 0` | 対象の個数 |
| `label_count` | `int` | 初期値0、利用時は`K > 0`必須 | 割当先の個数。0のまま呼ばない |
| `demand` | `std::vector<long long>` | 空またはN。省略時は全頂点1 | 頂点ごとの容量消費。各値は0以上 |
| `lower_capacity` | `std::vector<long long>` | 空またはK。省略時は全ラベル0 | ラベルごとの総需要量の最低値 |
| `upper_capacity` | `std::vector<long long>` | 空またはK。省略時は全ラベルが総需要量 | ラベルごとの総需要量の最大値 |
| `lower_count` | `std::vector<int>` | 空またはK。省略時は全ラベル0 | ラベルごとの最低件数 |
| `upper_count` | `std::vector<int>` | 空またはK。省略時は全ラベルN | ラベルごとの最大件数 |
| `unary_cost` | `std::vector<Cost>` | 空またはN×K。省略時は全費用0 | `unary_cost[v*K+a]`が頂点vをラベルaへ置く費用 |
| `edges` | `std::vector<edge_type>` | 初期値は辺なし。任意の辺数 | 二項関係。`edge_type`は下記の辺型への別名 |
| `fixed_label` | `std::vector<int>` | 空またはN。省略時は全頂点自由 | `-1`は自由、`0..K-1`はそのラベルへ固定 |
| `allowed` | `std::vector<unsigned char>` | 空またはN×K。省略時は全割当許可 | `allowed[v*K+a] != 0`なら許可。固定頂点では固定指定が優先 |

上下限の配列は、それぞれ独立に省略できます。件数は0〜N、需要量・容量は非負とし、各ラベルの下限が上限以下になるようにします。明示した上限の合計が総需要量より小さいなど、全体に矛盾がある入力では実行可能解はありません。個別上下限に矛盾がなくても、許可先や不可分な需要量のため実行不可能になることがあります。

例えば、工数と件数を同時に指定する準備は次の形です。

```cpp
capacitated_graph_labeling_problem<long long> problem;
problem.vertex_count = 4;
problem.label_count = 2;
problem.demand = {2, 1, 2, 1};
problem.lower_capacity = {2, 2};
problem.upper_capacity = {4, 4};
problem.lower_count = {1, 1};
problem.upper_count = {3, 3};
problem.unary_cost = {
    0, 4, // 頂点0をラベル0へ置く費用0、ラベル1へ置く費用4
    1, 0, // 頂点1
    0, 3, // 頂点2
    2, 0  // 頂点3
};
problem.fixed_label = {-1, -1, 0, -1}; // 頂点2だけラベル0へ固定
problem.allowed.assign(4 * 2, 1);
problem.allowed[3 * 2 + 0] = 0;        // 頂点3をラベル0へ置くことは禁止
problem.edges = {{0, 2, 5, 0}};        // 頂点0と2の分離費用5
```

件数結果が欲しい場合は、`lower_count`または`upper_count`を明示します。制約を実質的に追加せず件数を取得するには、`upper_count.assign(K, N)`とできます。両方を省略すると、通常結果の`count`は空です。

### 5.4 辺のパラメータ

`capacitated_graph_labeling_edge<Cost>`は次の集成体です。`{u, v, weight, type}`の順で初期化できます。辺型そのもののテンプレート引数`Cost`は省略できません。

| メンバー | 型 | デフォルト | 設定方法 |
|---|---|---|---|
| `u` | `int` | 0 | 第1端点。`0..N-1` |
| `v` | `int` | 0 | 第2端点。`0..N-1` |
| `weight` | `Cost` | `Cost{1}` | 組込みpolicyが掛ける重み。負でもよい |
| `type` | `int` | 0 | custom policyへ渡す辺種別。組込みpolicyは参照しない |

無向の1関係は1本だけ登録します。`{u,v}`と`{v,u}`を両方登録すると、両方の費用を払います。有向の関係を各方向で評価したい場合は、それぞれ登録します。自己辺と多重辺も有効で、一般policyの自己辺は`P_e(a,a)`として評価されます。Potts自己辺は常に0です。

### 5.5 二項コスト規則の準備

#### Potts型

```cpp
capacitated_potts_cost<long long> pair_cost;
```

保持する設定項目はなく、`label_u == label_v ? 0 : edge.weight`を返します。`is_potts`はコンパイル時定数`true`です。初期構築でPotts専用処理が使われ、探索では容量に収まればPotts差分表を使います。

#### 共通ラベル行列

```cpp
std::vector<long long> matrix = {
    0, 1, 3,
    1, 0, 2,
    3, 2, 0
};
capacitated_label_matrix_cost<long long> pair_cost{3, &matrix};
// problem.label_countも3に設定する。
```

| メンバー | 型 | デフォルト | 必須条件 |
|---|---|---|---|
| `label_count` | `int` | 0 | 問題のKと同じ値 |
| `matrix` | `const std::vector<Cost>*` | `nullptr` | K×K以上の要素を持つvectorへのポインタ。通常はちょうどK×K |
| `is_potts` | `inline static constexpr bool` | `false` | 変更するパラメータではない |

戻り値は`edge.weight * (*matrix)[label_u*K + label_v]`です。行が元の`edge.u`側、列が`edge.v`側です。行列はコピーされず、呼出しが終わるまで生存している必要があります。関数ローカルの行列でも、同じ関数内でsolverの呼出しを完了すれば使えます。行列オブジェクトを破棄・移動した後のポインタを使ったり、呼出し中に内容を変更したりしません。

#### 任意のcustom policy

constオブジェクトに対し、次の呼出しが可能な型を定義します。`pair_cost`は二項コストを返す関数オブジェクトを指す名称です。

```cpp
struct custom_pair_cost {
    long long operator()(
        int edge_id,
        const capacitated_graph_labeling_edge<long long>& edge,
        int label_u,
        int label_v) const {
        (void)edge_id;
        // 例: type=0は分離費用、type=1は同居費用。
        return edge.type == 0
            ? (label_u != label_v ? edge.weight : 0LL)
            : (label_u == label_v ? edge.weight : 0LL);
    }
};
```

`edge_id`は`problem.edges`の添字です。固定した入力と両端ラベルに対して常に同じ値を返し、計算中に現在解全体や外部時刻へ依存させません。費用の一部に`edge.weight`を使うかどうかもcustom policyが決めます。ヘッダ側がさらに重みを掛けることはありません。

通常は`is_potts`を定義しないか、`false`にします。**厳密に`edge.weight * [label_u != label_v]`と同じ値を返す場合だけ**`inline static constexpr bool is_potts = true`を宣言できます。定数を足しただけの式でも、この条件とは異なります。誤ってtrueにすると、単に速度が変わるのでなく、目的値・差分の整合性が壊れます。

### 5.6 探索オプションの全項目

```cpp
capacitated_graph_labeling_options options;
```

| メンバー | 型 | デフォルト | 動作と選び方 |
|---|---|---|---|
| `time_limit_ms` | `double` | `1000.0` | 0以上で呼出し開始からの相対時間を制限。負値は無効。外側の残り時間から出力・検証分を引く |
| `iteration_limit` | `long long` | `-1` | 0以上で探索の提案反復数を制限。負値は無効。再現検証には固定値を使う |
| `seed` | `std::uint64_t` | `1` | 乱数の初期値。入力と設定の比較では固定し、複数試行では変える |
| `initial_trials` | `int` | `8` | `solve`の初期構築試行数。0以下でも最低1回。`improve`とsubsetでは使わない |
| `initial_temperature` | `double` | `-1.0` | 負値なら自動設定、0以上なら開始温度を明示 |
| `final_temperature` | `double` | `-1.0` | 負値なら自動設定、0以上なら終了温度を明示 |
| `potts_cache_max_bytes` | `std::size_t` | `256ULL << 20` | 256 MiB。Potts差分表と隣接フィルタの合計上限。0なら両方を無効化 |
| `deadline` | `std::chrono::steady_clock::time_point` | `time_point::max()` | 絶対終了時刻。既定値は制限なし。外側の探索と共通の時刻を渡せる |

時間・温度のdouble値は有限値にします。表の既定値はヘッダの値です。8章の例も、optionsを省略した場合はこの値を使います。時間制限は入力構造体を作る時間までは含まず、公開探索関数の内部での準備から計ります。

#### 停止条件の組合せ

相対時間、絶対時刻、反復数のうち、先に達した条件で停止します。少なくとも一つを有効にします。

| 設定・状態 | 動作 |
|---|---|
| `time_limit_ms < 0`、`iteration_limit > 0`、deadlineは最大値 | 固定反復で実行 |
| `time_limit_ms > 0`、`iteration_limit < 0` | 時間で停止 |
| 時間と反復数の両方を設定 | 両方を監視。冷却の進捗には割合の大きい方を使用 |
| `iteration_limit == 0` | 局所探索なし。`solve`は時間内で初期構築試行を行う |
| `time_limit_ms == 0`、または開始時にdeadlineを超過 | `solve`は最初の構築を1回試す。`improve`とsubsetは入力確認後、変更せず終了 |
| 可変頂点なし、またはKが1 | 初期状態を用意し、局所探索せず終了 |
| 時間・反復数とも負、deadlineも最大値 | 通常は止まらないため使用しない |

最初の構築試行、CSR・キャッシュ構築、温度推定の途中では細かく期限を確認しません。探索中も最大1,024反復ごとの確認です。時間・deadlineは厳密な実時間上限ではなく、重い1回の準備や探索ブロックの分だけ超過し得ます。APIが返った後の出力にも時間を残します。

```cpp
capacitated_graph_labeling_options options;
options.time_limit_ms = -1.0;
options.iteration_limit = 200000;
options.seed = 7;
// 固定反復の比較では、有限のdeadlineも付けない。
```

```cpp
const auto outer_deadline = std::chrono::steady_clock::now()
    + std::chrono::milliseconds(1800);
capacitated_graph_labeling_options options;
options.time_limit_ms = 30.0;       // 今回の呼出しの予算
options.deadline = outer_deadline; // 外側の処理全体と共有する期限
```

#### `initial_trials`の選び方

まず既定値8を使い、初期構築時間と構築成功率を確認します。極端に短い呼出しでは1〜4へ減らす選択があります。厳しい重み付き容量・疎な許可先で構築失敗が多いなら、16〜32へ増やすことを試せます。これらの値は調整の出発点で、成功率・品質の保証ではありません。構築が難しいときは、回数だけを増やすより実行可能な外部初期解を使う方が適切な場合があります。

指定回数の構築を必ず実施するわけではなく、2回目以降は期限を確認します。同じ総時間なら、構築を増やすほど改善処理へ使える時間は減ります。

#### 温度の選び方

温度は、費用が悪化する候補も受け入れて探索範囲を広げるための値です。まず両方とも自動設定を使います。自動値は、実行可能な非ゼロ差分の絶対値の標本から得た尺度の0.5倍・0.03倍です。片方だけ明示することもできます。

明示する場合、目的値そのものの大きさでなく、1回の移動で起こる差分の大きさに合わせます。例えば差分が100倍になるよう費用を変更したなら、同じ受理傾向を狙う温度も100倍が目安です。通常は開始温度を終了温度以上にします。逆順も入力できますが、冷却ではなく加熱になります。0は内部で`1e-12`へ補正されるため、数値的に極めて小さい悪化を除けば受理しにくくなります。

#### キャッシュ量の選び方

実際の可変頂点数をQとします。全体探索なら固定頂点を除く個数、subsetなら指定集合の個数です。必要なPotts差分表は`sizeof(Cost) * Q * K`byte、任意の隣接フィルタは`8 * Q`byteです。

| メモリ上限との関係 | 動作 |
|---|---|
| 差分表が入らない | 隣接辺を走査する一般差分評価を使う |
| 差分表だけ入る | 差分表を使い、swapの隣接補正は走査する |
| 差分表とフィルタが入る | 両方を使う |

これはsolver全体のメモリ上限ではありません。入力、CSR、完全ラベル列などは別に必要です。一般行列・一般custom policyでは、この上限を増やしてもPotts表は使いません。探索を開始しないときも表を作りません。

### 5.7 用途による準備の違い

| 用途 | 主な準備 |
|---|---|
| 分割 | 正の辺、分割数、件数上下限。seedがあれば固定ラベル |
| 色分け | 同色罰金を負のPotts重みに変換し、元スコア用の定数も保持 |
| signed clustering | 同居希望は正、分離希望は負の辺。分離希望の罰金分を定数として保持 |
| 容量付き割当 | 需要量・容量・件数・単項費用・固定先・許可先を整える |
| 時間帯・距離 | 元の辺の向きを保ち、K×K表を呼出し中保持 |
| 種別関係 | 辺のtypeと、その範囲を覆う種別ごとのK×K表 |
| QAP | K=N、各ラベル件数1、位置間表、数える向きに合わせたflow辺 |
| 再実行・subset | 更新後の問題、前の完全ラベル列。subsetでは妥当な変更範囲も用意 |
| 複数seed | 共通の問題・policy、試行回数、総予算 |

最初は小さい入力を作り、費用を手で再計算できることを確認してから規模を増やします。ハードな条件は容量・件数・固定・許可で表し、希望は費用として表すと、保証される条件を把握しやすくなります。

## 6. ユースケースごとの使い方

### 6.0 呼出しと戻り値の共通仕様

以下で、`problem`は設定済みの問題、`pair_cost`は二項コスト規則、`options`は5.6の設定です。探索関数の引数は全てconst参照で受け取られ、入力問題や初期ラベル列は変更されません。

```cpp
// 初期解を持っていない場合。optionsは省略可能。
auto result = solve_capacitated_graph_labeling(problem, pair_cost, options);

// 実行可能な完全解を出発点にする場合。
auto improved = improve_capacitated_graph_labeling(
    problem, initial_label, pair_cost, options);

// 完全解のうち、指定頂点だけを変更する場合。
auto proposal = improve_capacitated_graph_labeling_subset(
    problem, initial_label, mutable_vertices, pair_cost, options);
```

`initial_label`は長さNの`std::vector<int>`、`mutable_vertices`は頂点番号の`std::vector<int>`です。前者は全体で実行可能でなければならず、後者は重複・範囲外・固定頂点を含まないことが条件です。subsetもoptionsを省略できます。

#### 通常結果

`solve`と全体`improve`は`capacitated_graph_labeling_result<Cost>`を返します。

| メンバー | 型 | 成功時の意味 |
|---|---|---|
| `label` | `std::vector<int>` | 長さNのラベル列 |
| `load` | `std::vector<long long>` | 長さKのラベル別総需要量 |
| `count` | `std::vector<int>` | 件数上下限を一方でも指定した場合は長さKの件数。両方省略時は空 |
| `objective` | `Cost` | 返した解のライブラリ目的値 |
| `initial_objective` | `Cost` | `solve`では採用した構築解、`improve`では入力解の目的値 |
| `iterations` | `long long` | 探索で行った提案反復数。無効提案・不受理も数える。初期構築・温度推定は数えない |
| `elapsed_ms` | `double` | 内部の準備から結果をまとめるまでの経過時間。入力作成や呼出し側の処理時間は含まない |
| `feasible` | `bool` | 実行可能解を返せたか |
| `used_potts_cache` | `bool` | Potts差分表を構築したか。フィルタだけの有無を示す値ではない |
| `status` | `capacitated_graph_labeling_status` | 成功または失敗理由 |

デフォルトは配列が空、数値が0、boolがfalse、statusが`construction_failed`です。失敗時はその目的値0を良い解と解釈しません。成功した探索では最良解を保持するため、内部で管理する目的値は`objective <= initial_objective`です。浮動小数点で外部再計算と比べる場合は丸め差を考慮します。

```cpp
if (result.feasible) {
    const auto& label = result.label; // label[v]を業務上の担当先などへ変換
    const auto objective = result.objective;
    // ここでlabelとobjectiveを出力・保存・外側の探索へ渡す。
    (void)label;
    (void)objective;
} else {
    // result.statusに応じて、初期解の作り直しなどを行う。
}
```

#### subset結果

subsetは`capacitated_graph_labeling_repair_result<Cost>`を返します。この型には`feasible`、完全ラベル列、全体目的値、load、countはありません。

| メンバー | 型 | 意味 |
|---|---|---|
| `changed_vertices` | `std::vector<int>` | 初期解からラベルが変わった頂点 |
| `new_labels` | `std::vector<int>` | 同じ添字の頂点へ設定する新しいラベル |
| `objective_delta` | `Cost` | 新しい目的値−入力解の目的値。境界辺を含む |
| `iterations` | `long long` | 探索提案反復数 |
| `elapsed_ms` | `double` | 内部経過時間 |
| `used_potts_cache` | `bool` | Potts差分表を構築したか |
| `status` | `capacitated_graph_labeling_status` | 成功または失敗理由 |

デフォルトは配列が空、数値が0、boolがfalse、statusが`invalid_repair_scope`です。**statusを確認してから**変更を適用します。成功でも改善が見つからなければ、差分0・空配列です。同じ目的値の別解へ変更するためのAPIではありません。返却順に依存せず、頂点番号を使って適用します。

```cpp
if (proposal.status == capacitated_graph_labeling_status::success) {
    for (std::size_t i = 0; i < proposal.changed_vertices.size(); ++i) {
        initial_label[proposal.changed_vertices[i]] = proposal.new_labels[i];
    }
}
```

#### statusの全種類

| status | 起きる呼出し | 意味・対処 |
|---|---|---|
| `success` | 全探索API | 実行可能な結果を返した。探索なし・変更なしも含む |
| `construction_failed` | `solve` | 初期解の構築に失敗。実行不可能性の証明ではない |
| `infeasible_initial_solution` | `improve`、subset | 渡した完全解が入力制約を満たさない。実行可能解を用意する |
| `invalid_repair_scope` | subset | 範囲外・重複・固定頂点を含む変更範囲。集合を修正する |

subsetで範囲と初期解の両方に不備があると、先に範囲の失敗を返します。問題データ自体の形・サイズの不備を全てstatusへ変換する仕組みではありません。

#### 評価と実行可能性の確認

```cpp
bool ok = is_feasible_capacitated_graph_labeling(problem, label);
// 長さ、ラベル番号、固定/許可、需要量、件数を確認。費用は計算しない。
if (ok) {
    auto cost = evaluate_capacitated_graph_labeling(problem, label, pair_cost);
    (void)cost;
}
```

評価関数は`Cost`を返し、全単項・全辺を合計します。実行可能性を検査する関数ではありません。長さN、全ラベルが範囲内、問題とpolicyが正しく設定済みであることが前提です。外部スコアが定数補正を必要とする場合、その補正は利用者側で行います。

### 6.1 グラフ分割・seed付き領域分割

8.1の`solve_balanced_graph_partition`へ、頂点数、分割数、`partition_edge`の列、任意のseedラベル列、optionsを渡します。seed列は空なら固定なし、長さNなら`-1`が自由、非負値が固定先です。

```cpp
auto result = solve_balanced_graph_partition(n, k, edges, seed_label, options);
```

内部で件数下限を`N/K`、上限をその切上げ値にし、正のPottsコストで`solve_capacitated_graph_labeling`を呼びます。成功時は`result.label[v]`が分割先、`result.count[a]`が分割サイズ、`result.objective`が分離費用です。seedが一つの分割に多すぎるなど、均等サイズと矛盾する場合は構築に失敗します。

### 6.2 Max-Cut・衝突の少ない色分け

8.2の`solve_balanced_soft_coloring`へ、頂点数、色数、同色罰金を持つ辺列、optionsを渡します。

```cpp
auto answer = solve_balanced_soft_coloring(n, colors, edges, options);
```

各罰金を負の辺重みに変換し、Potts型でsolveします。成功判定は`answer.raw.feasible`、色列は`answer.raw.label`です。`separated_weight`は異色になった辺の重み合計、`conflict_penalty`は同色辺の罰金合計です。raw目的値をC、入力罰金の総和をWとすると、前者は`-C`、後者は`W+C`です。自己辺があれば、その罰金は必ず衝突側へ残ります。

### 6.3 正負の関係を持つクラスタリング

8.3の`solve_signed_clustering`へ、頂点数、クラスタ数、人数下限・上限、単項費用、関係列、optionsを渡します。上下限・単項費用は空で省略できます。

```cpp
auto answer = solve_signed_clustering(
    n, k, lower_size, upper_size, unary_cost, relations, options);
```

関係の`prefer_same_cluster`がtrueなら正、falseなら負のPotts辺を作ります。成功時は`answer.raw.label`がクラスタ番号、`answer.original_penalty`が単項費用と全希望違反の合計です。後者には、負の辺で表した同居罰金の定数分を加えています。raw目的値は負でも異常ではありません。

### 6.4 容量付き割当・負荷分散

8.4の`capacitated_assignment_input`へ仕事負荷、担当先数、各上下限、費用、固定・許可、関係列を設定します。

```cpp
auto answer = solve_capacitated_assignment(input, options);
```

各vectorを問題型へ対応させ、Potts型でsolveします。成功時の`answer.raw.label[v]`が担当先、`raw.load[a]`が負荷、件数上下限を指定した場合の`raw.count[a]`が件数です。元の単項費用と関係罰金の合計は`answer.total_penalty`を使います。

`allowed`を作るときは、N×Kを0で初期化して許可先だけ1にするか、1で初期化して禁止先だけ0にします。固定先を許可集合にも含める運用にすると、入力の意図が明確になります。構築に失敗しやすいときは、実行可能解を用意して6.9の全体改善へ渡します。

### 6.5 順序を考慮した時間帯割当

8.5の`solve_soft_precedence_slots`へ、作業需要量、時間帯容量、単項費用、先行関係、optionsを渡します。作業数は需要量列、時間帯数は容量列の長さから決まります。

```cpp
auto result = solve_soft_precedence_slots(
    task_load, slot_capacity, slot_cost, precedence, options);
```

各先行辺`before -> after`を登録し、行列の行をbeforeの時間帯、列をafterの時間帯にします。行が列より小さければ0、そうでなければ`行-列+1`を関係の重みへ掛けます。成功時は`result.label[v]`が時間帯番号、`objective`が全費用です。返却された順序が必須条件を満たすかは、必要なら別に検査します。

### 6.6 距離を使う領域・状態ラベリング

8.6の`solve_capacitated_metric_labeling`へ、N、K、需要量、容量下限・上限、N×Kの単項表、辺列、K×Kの距離表、optionsを渡します。

```cpp
auto result = solve_capacitated_metric_labeling(
    n, k, demand, lower_capacity, upper_capacity,
    unary_cost, edges, label_distance, options);
```

需要量・上下限は空で省略できます。この例では単項表を必須とし、全ゼロならゼロで埋めます。行列policyでsolveし、`objective`は単項費用と`strength * distance`の和です。距離表が同色0・異色1だけなら、同じ費用をPotts型で表せます。

### 6.7 種類の異なる関係の同時評価

8.7の`typed_labeling_input`へ、N、K、件数上下限、単項表、種別付き辺、種別ごとの行列を設定します。

```cpp
auto result = solve_typed_relation_labeling(input, options);
```

`matrix_by_type[((type*K)+a)*K+b]`が該当単価です。型の内部にあるpolicyはその値と辺重みの積を返し、一般policyとしてsolveします。成功時の`objective`は単項費用と全種別の辺費用です。使う全てのtypeを行列配列が覆う必要があります。policyが一般形なのでPotts表は使いません。

### 6.8 疎なQAP・一対一配置

8.8の`solve_sparse_qap_as_labeling`へ、要素数、任意の単項配置表、flow辺、位置間表、optionsを渡します。

```cpp
auto result = solve_sparse_qap_as_labeling(
    n, placement_cost, flows, position_distance, options);
```

各ラベルの件数を1にし、行列policyでsolveします。成功時の`result.label[item]`が配置位置で、ラベル列はpermutationになります。位置から要素を引きたい場合は、`item_at[result.label[item]] = item`で逆対応を作れます。目的値は単項費用と入力flowごとの積の合計です。対称な相互作用を両方向で数える定義なら、flowにも両方向を入力します。

### 6.9 全体の再最適化

初期解が必ず実行可能なら、直接`improve_capacitated_graph_labeling(problem, initial_label, pair_cost, options)`を呼べます。初期構築は行わず、`initial_trials`は参照しません。

8.9の`resolve_after_update`は、更新後の制約が前の解を無効にする可能性がある場合の補助関数です。

```cpp
auto answer = resolve_after_update(problem, previous_label, pair_cost, options);
```

`answer.reused_previous`がtrueなら全体improve、falseならsolveで構築を試しています。成功判定は`answer.raw.feasible`です。成功時だけ`previous_label = answer.raw.label`として次ターンへ進みます。費用を変更した場合、`raw.initial_objective`も更新後の費用に基づきます。構築へ進んだ場合には、前の解との非悪化を意味する値ではありません。

### 6.10 局所的な再最適化

8.10の`repair_subset_in_place`へ、更新後の問題、現在の完全解、変更可能頂点、policy、optionsを渡します。

```cpp
auto proposal = repair_subset_in_place(
    problem, current_label, mutable_vertices, pair_cost, options);
```

この補助関数は、成功時にだけ`current_label`へ変更を適用します。失敗時は現在解を変更しません。戻り値は元APIのsubset結果です。全体目的値が必要なら更新後の問題で基準値を計算し、`objective_delta`を加えます。

外側のスコアがライブラリ目的値と一致しない場合は、元のsubset APIを直接呼び、候補を一時ラベル列へ適用して本来のスコアで採否を判断します。in-placeの補助関数はライブラリ目的値を採用基準にできる場合に使います。

### 6.11 複数seedからの探索

8.11の`solve_labeling_multistart`へ、問題、policy、試行数、総時間、seed開始値、任意の共通optionsを渡します。

```cpp
auto answer = solve_labeling_multistart(
    problem, pair_cost, 4, 1000.0, 10, common_options);
```

共通optionsの温度、構築回数、キャッシュ量、反復上限を各runへ引き継ぎます。相対時間とseedはこの補助関数が配分し、deadlineは共通optionsの絶対時刻と総時間から得た時刻の早い方です。`iteration_limit`は全run合計でなく、各runの上限です。

成功判定は`answer.best.feasible`、最良解は`answer.best.label`です。`runs_attempted`、`runs_succeeded`、`total_iterations`、`total_elapsed_ms`は全runの情報です。`best.iterations`や`best.elapsed_ms`は採用された1runの値です。予算が既に切れていれば0回で終了し、bestは失敗状態のままです。全runが構築失敗でも同様なので、試行数と成功数も確認します。

## 7. 制約・注意点

### 7.1 表せる問題の範囲

| 条件・目的 | 対応と注意 |
|---|---|
| 各頂点にちょうど1ラベル | 対応。未割当を許すなら、容量・費用を定義した専用ラベルとしてモデル化する |
| 需要量1種類＋件数の上下限 | 同時指定可能。容量消費は割当先に依存しない |
| CPU・メモリなど複数の重み付き資源 | 直接対応しない。巨大整数への詰込みで各資源の不等式を一般に代用できない |
| 固定・許可ラベル | 対応。固定が許可表より優先 |
| 同色禁止、先行順序、領域の連結性 | ハード制約としては対応しない。罰金や外部検証だけでは探索中の保証にならない |
| 3頂点以上の同時条件、使用ラベルの固定費 | そのまま指定するAPIはない。正確な単項・二項モデルへ変換できるか別途検討 |
| ラベル数そのものの選択 | Kを入力する。空ラベルは許せるが、使用個数の最適化項はない |
| 1仕事の複数担当先への分割、複数時間帯の占有 | 直接対応しない |
| 最適性証明・近似比・下界 | 返さない。必要な場合は2章の方法を検討 |

外部で連結性などを満たす初期解を作っても、その条件がライブラリの制約で表されていなければ、改善後に保持されるとは限りません。

### 7.2 入力と失敗処理

- 問題の形・vector長・端点などの検査は主に`assert`です。`NDEBUG`では無効になります。不正な入力を安全なstatusへ変換するAPIではありません。
- 長さNの初期解があっても、容量や許可条件に違反していると全体improve・subsetは失敗します。「repairなら不正解も受け付ける」とは解釈しません。
- `construction_failed`は構築法が見つけられなかったことを示します。実行可能解が存在しないという証明ではありません。
- 失敗結果の空ラベル列や目的値0を利用しません。通常結果は`feasible`、subsetは`status`を先に確認します。
- 固定ラベルと許可表の積集合が必要なら、固定先が許可されているかを入力側で検査します。
- subset集合には重複・固定頂点・範囲外頂点を含めません。空集合は有効ですが、入力解の実行可能性は必要です。

### 7.3 動けなくなる組合せ

各移動は常に実行可能性を守ります。厳密件数なら1頂点だけの移動ができず、交換が中心になります。厳密な重み付き容量で2頂点の需要量が違うと、その交換も不可能になり得ます。3頂点以上の循環交換や、一度制約を破ってから直す処理はありません。

このため、初期解から実行可能な経路で到達できる範囲が狭い場合があります。subset範囲を広げると交換相手が増えますが、問題全体に必要な複合移動を必ず実現できるわけではありません。解が変わらない原因として、時間不足だけでなく実行可能な近傍があるかも確認します。

### 7.4 policyとデータ寿命

- `is_potts=true`を、厳密なPotts式以外へ付けないでください。定数差だけのpolicyも条件外です。
- 行列policyを`label_count=0`、`matrix=nullptr`のまま使いません。問題のKと表の幅を一致させます。
- 行列・policyの参照先は呼出し中に生存し、内容が変わらないことが必要です。
- custom費用が別頂点のラベル、現在の総負荷、乱数、時刻へ依存すると、局所差分だけで目的値を追えなくなります。
- 非対称費用では、辺の端点順を保ちます。辺の削除・並べ替え時は、辺ID依存データも並べ直します。
- 多重辺は全て加算します。重複を合算するなら、向き・種類・policyでの意味が同じことを確かめます。

### 7.5 数値・メモリ・時間

- `Cost`の積・差分・累積和と、`long long`の総需要量・容量計算でオーバーフローしないようにします。符号付き整数の桁あふれは未定義動作です。
- 整数型でも温度計算では差分をdoubleへ変換します。巨大な整数では受理確率計算の精度に限界があります。
- 浮動小数点では、差分更新と完全再計算に丸め差が出ます。厳密な一致比較だけで検証しません。数値的に厳しい入力では、採用時に全目的値を再計算します。
- `potts_cache_max_bytes`は入力やCSRを含みません。subsetでも全体サイズに比例するデータを持ちます。
- 時間・温度にNaNや無限大を指定しません。負の有限値で自動設定・制限無効を表します。
- 停止条件を全て無効にしません。deadlineを指定しても初期構築や間欠検査による超過はあります。
- 固定seedだけで時間制限実行の同一結果は保証されません。再現検証は固定反復・時間無効で行います。policy、入力順、コンパイラ設定、キャッシュ設定もそろえます。

### 7.6 元の問題のスコアとの対応

負のPotts辺で同居罰金を表す場合など、ライブラリ目的値と表示したいスコアの間に定数差が生じます。定数は同じ入力内での解の優劣を変えませんが、罰金ゼロの確認や異なるターンの表示では補正が必要です。8.2〜8.4の例は元の費用を別メンバーで返します。

本来のスコアを近似した費用を入力する場合、その近似費用が改善しても本来のスコアが改善する保証はありません。外側の採用判定で必要なスコアを評価してください。費用更新後のsubset差分の基準は3.5を参照してください。

## 8. ユースケースごとのコード例

### 8.0 コピー方法と共通の入力契約

以下の各コードブロックは、そのブロック内の入力型・結果型・関数をまとめてコピーすれば使用できます。共通の補助関数を別の節から探す必要はありません。全例をまとめた`capacitated_graph_labeling_examples_v13.hpp`も付属します。ヘッダとして複数の翻訳単位から使う場合に備え、通常関数はinline、汎用関数はtemplateにしています。

- 各例は有効な問題入力を前提にします。配列長・番号などのassertは入力契約の確認であり、実行可能解が見つかったかの判定には使いません。
- 8.1〜8.8は整数のlong longを使います。全費用の積・和・差分と需要量が型へ収まること、内部のint添字が収まる規模であることを前提とします。
- 通常の例はoptionsを省略すると相対時間1,000ms、seed 1です。短い呼出しや再現試験ではoptionsを明示します。8.11は総時間とseed開始値を明示します。
- 成功時だけラベルと目的値を使います。rawやbestを持つ結果では、その中のfeasibleを確認します。失敗時に補正済み費用の既定値0を使いません。
- 8.1〜8.8の問題例はN > 0を前提にしています。ライブラリ自体はN=0も扱えます。

### 8.1 グラフ分割・seed付き領域分割

全グループをfloor/ceilの人数にそろえます。seed_labelは省略可能です。seed付きでも人数制約は残るので、固定人数が上限を超えないようにします。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct partition_edge {
    int u; // 0..vertex_count-1
    int v; // 0..vertex_count-1。無向の1関係は1本だけ渡す
    long long communication; // 別分割になったときに払う非負費用
};

// 入力:
//   vertex_count: 頂点数
//   part_count: 分割数。1 <= part_count <= vertex_count
//   edges: 無向辺と、分割をまたいだときに払う非負コスト
//   seed_label: 空なら固定なし。長さNなら-1は自由、0..K-1は固定先
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   feasible=trueならlabel[v]が分割番号、objectiveがcut辺コスト総和
inline capacitated_graph_labeling_result<long long> solve_balanced_graph_partition(
    int vertex_count,
    int part_count,
    const std::vector<partition_edge>& edges,
    const std::vector<int>& seed_label = {},
    const capacitated_graph_labeling_options& options = {}) {
    assert(vertex_count > 0);
    assert(part_count > 0 && part_count <= vertex_count);
    assert(seed_label.empty() ||
           static_cast<int>(seed_label.size()) == vertex_count);

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = vertex_count;
    problem.label_count = part_count;
    problem.fixed_label = seed_label;

    // 全頂点の需要量は1。各分割の頂点数をfloor/ceilの範囲にする。
    problem.lower_count.assign(part_count, vertex_count / part_count);
    problem.upper_count.assign(
        part_count, vertex_count / part_count + (vertex_count % part_count != 0));

    problem.edges.reserve(edges.size());
    for (const auto& edge : edges) {
        assert(edge.u >= 0 && edge.u < vertex_count);
        assert(edge.v >= 0 && edge.v < vertex_count);
        assert(edge.communication >= 0);
        problem.edges.push_back(
            {edge.u, edge.v, edge.communication, 0});
    }

    return solve_capacitated_graph_labeling(
        problem, capacitated_potts_cost<long long>{}, options);
}
```

### 8.2 Max-Cut・衝突の少ない色分け

入力は同色時の罰金です。raw目的値、異色辺の重み、同色罰金を分けて返します。衝突ゼロを必要とする場合は、成功後にconflict_penaltyも確認します。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct conflict_edge {
    int u; // 0..N-1
    int v; // 0..N-1
    long long penalty_if_same; // 同じ色なら払う非負罰金
};

struct balanced_coloring_result {
    capacitated_graph_labeling_result<long long> raw;
    long long separated_weight = 0;
    long long conflict_penalty = 0;
};

// 入力:
//   vertex_count: 対象数N > 0
//   color_count: 2ならbalanced Max-Cut、3以上ならsoft balanced coloring
//                1..N。1の場合は全対象が同色
//   edges: 両端が同じ色になったときの非負罰金
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   raw.feasible=trueならraw.labelが色、separated_weightが異色辺重み、
//   conflict_penaltyが同色辺罰金
inline balanced_coloring_result solve_balanced_soft_coloring(
    int vertex_count,
    int color_count,
    const std::vector<conflict_edge>& edges,
    const capacitated_graph_labeling_options& options = {}) {
    assert(vertex_count > 0);
    assert(color_count > 0 && color_count <= vertex_count);

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = vertex_count;
    problem.label_count = color_count;
    problem.lower_count.assign(color_count, vertex_count / color_count);
    problem.upper_count.assign(
        color_count, vertex_count / color_count + (vertex_count % color_count != 0));

    long long total_edge_weight = 0;
    problem.edges.reserve(edges.size());
    for (const auto& edge : edges) {
        assert(edge.u >= 0 && edge.u < vertex_count);
        assert(edge.v >= 0 && edge.v < vertex_count);
        assert(edge.penalty_if_same >= 0);
        total_edge_weight += edge.penalty_if_same;

        // -w * [色が異なる] を最小化すると、異色辺の重みが最大になる。
        problem.edges.push_back(
            {edge.u, edge.v, -edge.penalty_if_same, 0});
    }


    balanced_coloring_result answer;
    answer.raw = solve_capacitated_graph_labeling(
        problem, capacitated_potts_cost<long long>{}, options);
    if (answer.raw.feasible) {
        answer.separated_weight = -answer.raw.objective;
        answer.conflict_penalty = total_edge_weight + answer.raw.objective;
    }
    return answer;
}
```

### 8.3 正負の関係を持つクラスタリング

同居希望と分離希望を一つの列へ入れます。元の希望違反の合計をoriginal_penaltyとして返すため、負のPotts重みの定数補正を呼出し側で繰り返す必要はありません。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct signed_cluster_relation {
    int u; // 0..N-1
    int v; // 0..N-1
    long long penalty; // 希望に反したときの非負罰金
    bool prefer_same_cluster; // true: 同居希望、false: 分離希望
};

struct signed_clustering_result {
    capacitated_graph_labeling_result<long long> raw;
    long long original_penalty = 0;
};

// 入力:
//   vertex_count: 対象数N > 0。cluster_count: 事前に決めるK > 0
//   lower_size、upper_size: クラスタ別頂点数上下限。空ならその側の制約なし
//   unary_cost[v*K+c]: 頂点vをクラスタcへ置く事前コスト。空なら0
//   relations: 同一または分離を好む関係と、希望に反したときの罰金
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   raw.feasible=trueならraw.label[v]がクラスタ番号、original_penaltyが
//   単項コストと全関係違反罰金の合計
inline signed_clustering_result solve_signed_clustering(
    int vertex_count,
    int cluster_count,
    const std::vector<int>& lower_size,
    const std::vector<int>& upper_size,
    const std::vector<long long>& unary_cost,
    const std::vector<signed_cluster_relation>& relations,
    const capacitated_graph_labeling_options& options = {}) {
    const int n = vertex_count;
    const int k = cluster_count;
    assert(n > 0 && k > 0);
    assert(lower_size.empty() || static_cast<int>(lower_size.size()) == k);
    assert(upper_size.empty() || static_cast<int>(upper_size.size()) == k);
    assert(unary_cost.empty() ||
           unary_cost.size() ==
               static_cast<std::size_t>(n) * static_cast<std::size_t>(k));

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = n;
    problem.label_count = k;
    problem.lower_count = lower_size;
    problem.upper_count = upper_size;
    problem.unary_cost = unary_cost;

    long long constant_offset = 0;
    problem.edges.reserve(relations.size());
    for (const auto& relation : relations) {
        assert(relation.u >= 0 && relation.u < n);
        assert(relation.v >= 0 && relation.v < n);
        assert(relation.penalty >= 0);
        if (relation.prefer_same_cluster) {
            // 異なるクラスタならpenalty。
            problem.edges.push_back(
                {relation.u, relation.v, relation.penalty, 0});
        } else {
            // 同じクラスタならpenalty
            // = penalty - penalty * [異なるクラスタ]。
            problem.edges.push_back(
                {relation.u, relation.v, -relation.penalty, 0});
            constant_offset += relation.penalty;
        }
    }


    signed_clustering_result answer;
    answer.raw = solve_capacitated_graph_labeling(
        problem, capacitated_potts_cost<long long>{}, options);
    if (answer.raw.feasible) {
        answer.original_penalty = answer.raw.objective + constant_offset;
    }
    return answer;
}
```

### 8.4 容量付き割当・負荷分散

必須項目は担当先数と仕事負荷です。それ以外の配列はコメントの条件で省略できます。許可・固定・工数・件数を同時に指定できます。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct assignment_relation {
    int task_a; // 0..N-1
    int task_b; // 0..N-1
    long long penalty; // 希望に反したときの非負罰金
    bool prefer_same_worker; // true: 同じ担当、false: 別担当を希望
};

struct capacitated_assignment_input {
    int worker_count = 0; // 必須。担当先数K > 0
    std::vector<long long> task_load; // 必須。N個、N > 0、各値は非負の需要量
    std::vector<long long> lower_load; // 空=0、またはK個の総需要量下限
    std::vector<long long> upper_load; // 空=総需要量、またはK個の上限
    std::vector<int> lower_task_count; // 空=0、またはK個の最低件数
    std::vector<int> upper_task_count; // 空=N、またはK個の最大件数
    std::vector<long long> assignment_cost; // 空=0、またはN*K個。v*K+w
    std::vector<int> fixed_worker; // 空=自由、またはN個。-1=自由、非負=固定先
    std::vector<unsigned char> allowed; // 空=全許可、またはN*K個。非0=許可
    std::vector<assignment_relation> relations; // 空=関係費用なし
};

struct capacitated_assignment_solution {
    capacitated_graph_labeling_result<long long> raw;
    long long total_penalty = 0;
};

// 入力:
//   task_load[v]: 仕事vの需要量
//   assignment_cost[v*K+w]: 仕事vを担当wへ置く単項コスト
//   relations: 同じ担当または別担当にしたい仕事対と違反罰金
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   raw.feasible=trueならraw.label[v]が担当番号、raw.loadが担当別総需要量、
//   raw.countが指定時の担当件数、total_penaltyが元の問題定義による総罰金
inline capacitated_assignment_solution solve_capacitated_assignment(
    const capacitated_assignment_input& input,
    const capacitated_graph_labeling_options& options = {}) {
    const int n = static_cast<int>(input.task_load.size());
    const int k = input.worker_count;
    assert(n > 0 && k > 0);

    assert(input.lower_load.empty() ||
           static_cast<int>(input.lower_load.size()) == k);
    assert(input.upper_load.empty() ||
           static_cast<int>(input.upper_load.size()) == k);
    assert(input.lower_task_count.empty() ||
           static_cast<int>(input.lower_task_count.size()) == k);
    assert(input.upper_task_count.empty() ||
           static_cast<int>(input.upper_task_count.size()) == k);
    assert(input.assignment_cost.empty() ||
           input.assignment_cost.size() ==
               static_cast<std::size_t>(n) * static_cast<std::size_t>(k));
    assert(input.fixed_worker.empty() ||
           static_cast<int>(input.fixed_worker.size()) == n);
    assert(input.allowed.empty() ||
           input.allowed.size() ==
               static_cast<std::size_t>(n) * static_cast<std::size_t>(k));

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = n;
    problem.label_count = k;
    problem.demand = input.task_load;
    problem.lower_capacity = input.lower_load;
    problem.upper_capacity = input.upper_load;
    problem.lower_count = input.lower_task_count;
    problem.upper_count = input.upper_task_count;
    problem.unary_cost = input.assignment_cost;
    problem.fixed_label = input.fixed_worker;
    problem.allowed = input.allowed;

    long long constant_offset = 0;
    problem.edges.reserve(input.relations.size());
    for (const auto& relation : input.relations) {
        assert(relation.task_a >= 0 && relation.task_a < n);
        assert(relation.task_b >= 0 && relation.task_b < n);
        assert(relation.penalty >= 0);

        // prefer_same: 分離時に+penalty。
        // prefer_different: 同一時の罰金を、定数penaltyと分離報酬-penaltyで表す。
        const long long weight = relation.prefer_same_worker
                                     ? relation.penalty
                                     : -relation.penalty;
        if (!relation.prefer_same_worker) {
            constant_offset += relation.penalty;
        }
        problem.edges.push_back(
            {relation.task_a, relation.task_b, weight, 0});
    }


    capacitated_assignment_solution answer;
    answer.raw = solve_capacitated_graph_labeling(
        problem, capacitated_potts_cost<long long>{}, options);
    if (answer.raw.feasible) {
        answer.total_penalty = answer.raw.objective + constant_offset;
    }
    return answer;
}
```

### 8.5 順序を考慮した時間帯割当

時間帯の番号は早い順に0から付けます。先行作業と後続作業を同じ帯に置くと1段階の違反になる例です。費用表は関数ローカルですが、solver呼出し完了まで生存します。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct precedence_relation {
    int before; // 先行作業番号。0..N-1
    int after; // 後続作業番号。0..N-1
    long long violation_weight; // 順序違反1段階あたりの非負罰金
};

// 入力:
//   task_load[v]: 作業vが時間帯容量を消費する非負量。長さN > 0
//   slot_capacity[s]: 時間帯sの非負容量上限。長さS > 0
//   slot_cost[v*S+s]: 作業vを時間帯sへ置く単項コスト。空なら0
//   precedence: beforeをafterより前の時間帯へ置きたいsoft制約
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   feasible=trueならlabel[v]が時間帯番号、objectiveが単項コストと
//   precedence違反コストの合計
inline capacitated_graph_labeling_result<long long> solve_soft_precedence_slots(
    const std::vector<long long>& task_load,
    const std::vector<long long>& slot_capacity,
    const std::vector<long long>& slot_cost,
    const std::vector<precedence_relation>& precedence,
    const capacitated_graph_labeling_options& options = {}) {
    const int n = static_cast<int>(task_load.size());
    const int slot_count = static_cast<int>(slot_capacity.size());
    assert(n > 0 && slot_count > 0);
    assert(slot_cost.empty() ||
           slot_cost.size() ==
               static_cast<std::size_t>(n) *
                   static_cast<std::size_t>(slot_count));

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = n;
    problem.label_count = slot_count;
    problem.demand = task_load;
    problem.upper_capacity = slot_capacity;
    problem.unary_cost = slot_cost;
    problem.edges.reserve(precedence.size());
    for (const auto& relation : precedence) {
        assert(relation.before >= 0 && relation.before < n);
        assert(relation.after >= 0 && relation.after < n);
        assert(relation.violation_weight >= 0);
        problem.edges.push_back(
            {relation.before, relation.after,
             relation.violation_weight, 0});
    }

    // 行がbefore側、列がafter側。
    // before_slot < after_slotなら0、それ以外は逆転幅+1を払う。
    std::vector<long long> order_matrix(
        static_cast<std::size_t>(slot_count) *
        static_cast<std::size_t>(slot_count));
    for (int before_slot = 0; before_slot < slot_count; ++before_slot) {
        for (int after_slot = 0; after_slot < slot_count; ++after_slot) {
            order_matrix[before_slot * slot_count + after_slot] =
                before_slot < after_slot
                    ? 0LL
                    : static_cast<long long>(before_slot - after_slot + 1);
        }
    }

    const capacitated_label_matrix_cost<long long> pair_cost{
        slot_count, &order_matrix};
    return solve_capacitated_graph_labeling(problem, pair_cost, options);
}
```

### 8.6 距離を使う領域・状態ラベリング

距離表は第1端点のラベルを行、第2端点のラベルを列にします。関係強度は非負を前提とする例です。数学的な距離の性質そのものは検査しません。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct metric_labeling_edge {
    int u; // 第1端点。0..N-1
    int v; // 第2端点。0..N-1
    long long strength; // 距離へ掛ける非負の関係強度
};

// 入力:
//   vertex_count: N > 0、label_count: K > 0
//   demand[v]: 頂点需要量。空なら全頂点1
//   lower_capacity、upper_capacity: ラベル別総需要量上下限。空ならその側を省略
//   unary_cost[v*K+l]: N*K個の単項費用。不要なら全要素0にする
//   edges: 関係を評価する辺列
//   label_distance[a*K+b]: 必須のK*K個の単価。行=u側、列=v側
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   feasible=trueならlabelが割当、objectiveが単項コストと
//   sum(strength * label_distance)の合計
inline capacitated_graph_labeling_result<long long> solve_capacitated_metric_labeling(
    int vertex_count,
    int label_count,
    const std::vector<long long>& demand,
    const std::vector<long long>& lower_capacity,
    const std::vector<long long>& upper_capacity,
    const std::vector<long long>& unary_cost,
    const std::vector<metric_labeling_edge>& edges,
    const std::vector<long long>& label_distance,
    const capacitated_graph_labeling_options& options = {}) {
    const int n = vertex_count;
    const int k = label_count;
    assert(n > 0 && k > 0);
    assert(demand.empty() || static_cast<int>(demand.size()) == n);
    assert(lower_capacity.empty() ||
           static_cast<int>(lower_capacity.size()) == k);
    assert(upper_capacity.empty() ||
           static_cast<int>(upper_capacity.size()) == k);
    assert(unary_cost.size() ==
           static_cast<std::size_t>(n) * static_cast<std::size_t>(k));
    assert(label_distance.size() ==
           static_cast<std::size_t>(k) * static_cast<std::size_t>(k));

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = n;
    problem.label_count = k;
    problem.demand = demand;
    problem.lower_capacity = lower_capacity;
    problem.upper_capacity = upper_capacity;
    problem.unary_cost = unary_cost;
    problem.edges.reserve(edges.size());
    for (const auto& edge : edges) {
        assert(edge.u >= 0 && edge.u < n);
        assert(edge.v >= 0 && edge.v < n);
        assert(edge.strength >= 0);
        problem.edges.push_back({edge.u, edge.v, edge.strength, 0});
    }

    const capacitated_label_matrix_cost<long long> pair_cost{
        k, &label_distance};
    return solve_capacitated_graph_labeling(problem, pair_cost, options);
}
```

### 8.7 種類の異なる関係の同時評価

表を種別ごとに並べ、各種別内は行優先にします。この関数専用のpolicyは関数内へ閉じ込めています。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct typed_relation_edge {
    int u; // 第1端点。0..N-1
    int v; // 第2端点。0..N-1
    long long weight; // 種別の組合せ単価へ掛ける係数。負でもよい
    int type; // 0..種別数-1
};

struct typed_labeling_input {
    int vertex_count = 0; // 必須。N > 0
    int label_count = 0; // 必須。K > 0
    std::vector<int> lower_count; // 空=0、またはK個
    std::vector<int> upper_count; // 空=N、またはK個
    std::vector<long long> unary_cost; // 空=0、またはN*K個。v*K+a
    std::vector<typed_relation_edge> edges; // 空でもよい

    // type-major、row-major。
    // ((type * K + label_u) * K + label_v)番目を使う。
    // 種別数*K*K個。全てのedge.typeに対応する表を用意する。
    std::vector<long long> matrix_by_type;
};

// 入力:
//   matrix_by_type: 辺種別ごとのラベル組合せ単価
//   その他の項目はtyped_labeling_inputの各コメントを参照
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   feasible=trueならlabelが割当、objectiveが単項コストと全種別辺コストの和
inline capacitated_graph_labeling_result<long long> solve_typed_relation_labeling(
    const typed_labeling_input& input,
    const capacitated_graph_labeling_options& options = {}) {
    const int n = input.vertex_count;
    const int k = input.label_count;
    assert(n > 0 && k > 0);
    assert(input.lower_count.empty() ||
           static_cast<int>(input.lower_count.size()) == k);
    assert(input.upper_count.empty() ||
           static_cast<int>(input.upper_count.size()) == k);
    assert(input.unary_cost.empty() ||
           input.unary_cost.size() ==
               static_cast<std::size_t>(n) * static_cast<std::size_t>(k));

    const std::size_t matrix_size =
        static_cast<std::size_t>(k) * static_cast<std::size_t>(k);
    assert(matrix_size > 0);
    assert(input.matrix_by_type.size() % matrix_size == 0);
    const int type_count =
        static_cast<int>(input.matrix_by_type.size() / matrix_size);
    (void)type_count;

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = n;
    problem.label_count = k;
    problem.lower_count = input.lower_count;
    problem.upper_count = input.upper_count;
    problem.unary_cost = input.unary_cost;
    problem.edges.reserve(input.edges.size());
    for (const auto& edge : input.edges) {
        assert(edge.u >= 0 && edge.u < n);
        assert(edge.v >= 0 && edge.v < n);
        assert(edge.type >= 0 && edge.type < type_count);
        problem.edges.push_back({edge.u, edge.v, edge.weight, edge.type});
    }

    // この関数だけが使うpolicyをローカルに定義する。is_pottsは宣言しない。
    struct typed_matrix_pair_cost {
        int label_count;
        const std::vector<long long>* matrix;

        long long operator()(
            int, const capacitated_graph_labeling_edge<long long>& edge,
            int a, int b) const {
            const std::size_t width = static_cast<std::size_t>(label_count);
            const std::size_t index =
                (static_cast<std::size_t>(edge.type) * width +
                 static_cast<std::size_t>(a)) * width + static_cast<std::size_t>(b);
            return edge.weight * (*matrix)[index];
        }
    };
    const typed_matrix_pair_cost pair_cost{k, &input.matrix_by_type};
    return solve_capacitated_graph_labeling(problem, pair_cost, options);
}
```

### 8.8 疎なQAP・一対一配置

各位置を1回だけ使うことを件数上下限で表します。flowは非負を前提とする例です。各入力辺を1回だけ数えるので、元問題の定義に合わせて方向を決めます。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

struct sparse_qap_flow {
    int from_item; // 出発側要素。0..N-1
    int to_item; // 到着側要素。0..N-1
    long long flow; // 非負の相互作用量
};

// 入力:
//   item_count: 要素数=位置数N > 0
//   placement_cost[item*N+position]: 単項配置コスト。空なら0
//   flows: 入力した有向相互作用を各1回だけ加算
//   position_distance[a*N+b]: 位置aからbへの距離・相性コスト
//                             N*N個が必須。非対称でもよい
//   options: 時間・反復数など。省略時はライブラリの既定値
// 出力:
//   feasible=trueならlabel[item]が配置位置。各位置は必ず1回だけ使われる
inline capacitated_graph_labeling_result<long long> solve_sparse_qap_as_labeling(
    int item_count,
    const std::vector<long long>& placement_cost,
    const std::vector<sparse_qap_flow>& flows,
    const std::vector<long long>& position_distance,
    const capacitated_graph_labeling_options& options = {}) {
    const int n = item_count;
    assert(n > 0);
    assert(placement_cost.empty() ||
           placement_cost.size() ==
               static_cast<std::size_t>(n) * static_cast<std::size_t>(n));
    assert(position_distance.size() ==
           static_cast<std::size_t>(n) * static_cast<std::size_t>(n));

    capacitated_graph_labeling_problem<long long> problem;
    problem.vertex_count = n;
    problem.label_count = n;
    problem.lower_count.assign(n, 1);
    problem.upper_count.assign(n, 1);
    problem.unary_cost = placement_cost;
    problem.edges.reserve(flows.size());
    for (const auto& relation : flows) {
        assert(relation.from_item >= 0 && relation.from_item < n);
        assert(relation.to_item >= 0 && relation.to_item < n);
        assert(relation.flow >= 0);
        problem.edges.push_back(
            {relation.from_item, relation.to_item, relation.flow, 0});
    }

    const capacitated_label_matrix_cost<long long> pair_cost{
        n, &position_distance};
    return solve_capacitated_graph_labeling(problem, pair_cost, options);
}
```

### 8.9 全体の再最適化

呼出し前にproblemの費用や辺を更新します。前のラベル列が新しい制約を満たすかで、全体改善と再構築を分けます。更新後も必ず実行可能と分かっている場合は、全体improveを直接呼ぶと事前検査の重複を省けます。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

template <class Cost>
struct turn_update_result {
    capacitated_graph_labeling_result<Cost> raw;
    bool reused_previous = false; // trueなら前の解を起点に改善した
};

// 入力:
//   problem: 今ターンの費用・辺・制約を反映済みの有効な問題
//   previous_label: 前ターンの完全解。空や長さ違いなら再構築へ進む
//   pair_cost: 今ターンの二項コスト規則。参照先は呼出し中に保持する
//   options: 今回の時間・反復数など。省略時はライブラリの既定値
// 出力:
//   raw.feasible=trueならraw.labelが採用候補となる完全解
//   reused_previous=trueならraw.initial_objectiveは今ターンの費用での前の解
//   falseならsolveで構築を試した。失敗もあり得るためfeasibleを確認する
//   入力のproblemとprevious_labelは変更しない
template <class Cost, class PairCost>
turn_update_result<Cost> resolve_after_update(
    const capacitated_graph_labeling_problem<Cost>& problem,
    const std::vector<int>& previous_label,
    const PairCost& pair_cost,
    const capacitated_graph_labeling_options& options = {}) {
    turn_update_result<Cost> answer;
    answer.reused_previous =
        is_feasible_capacitated_graph_labeling(problem, previous_label);
    if (answer.reused_previous) {
        answer.raw = improve_capacitated_graph_labeling(
            problem, previous_label, pair_cost, options);
    } else {
        answer.raw = solve_capacitated_graph_labeling(problem, pair_cost, options);
    }
    return answer;
}
```

### 8.10 局所的な再最適化

成功差分を現在解へ適用する補助関数です。ライブラリ本体のsubset関数は入力ラベルを変更しません。この例の補助関数が適用を担当します。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

// 入力:
//   problem: 今回の有効な問題。外側の固定部分も含む全グラフ
//   mutable_vertices: 今回動かせる頂点。範囲内・重複なし・固定頂点なし
//   pair_cost: 二項コスト規則。呼出し中に値や参照先を変更しない
//   options: 今回の予算。外側の期限はoptions.deadlineへ渡す
// 入出力:
//   current_label: 入力時は全体で実行可能な完全解。成功時だけ差分を適用
// 出力:
//   statusと変更点。失敗時はcurrent_labelを一切変更しない
//   objective_deltaの基準は同じproblemでの入力ラベル列
//   ライブラリ目的値だけで採用してよい場面向けの補助関数
template <class Cost, class PairCost>
capacitated_graph_labeling_repair_result<Cost> repair_subset_in_place(
    const capacitated_graph_labeling_problem<Cost>& problem,
    std::vector<int>& current_label,
    const std::vector<int>& mutable_vertices,
    const PairCost& pair_cost,
    const capacitated_graph_labeling_options& options = {}) {
    auto proposal = improve_capacitated_graph_labeling_subset(
        problem, current_label, mutable_vertices, pair_cost, options);
    if (proposal.status == capacitated_graph_labeling_status::success) {
        for (std::size_t i = 0; i < proposal.changed_vertices.size(); ++i) {
            current_label[proposal.changed_vertices[i]] = proposal.new_labels[i];
        }
    }
    return proposal;
}
```

### 8.11 複数seedからの探索

時間分配を含む補助関数です。全体の試行回数・成功数・反復数・時間を、採用された1runの情報と分けて返します。

```cpp
#include "capacitated_graph_labeling_v22.hpp"

template <class Cost>
struct multistart_labeling_result {
    capacitated_graph_labeling_result<Cost> best; // 最も良かった成功run
    int runs_attempted = 0;
    int runs_succeeded = 0;
    long long total_iterations = 0;
    double total_elapsed_ms = 0.0; // この補助関数全体の時間
};

// 入力:
//   problem、pair_cost: 全runで共通の有効な問題と二項コスト規則
//   start_count: 最大run数。1以上
//   total_time_ms: 補助関数全体の相対予算。有限で0以上。0なら実行しない
//   first_seed: run iでfirst_seed+iを使う。uint64_tの加算は巡回する
//   common_options: 温度・初期試行数・キャッシュ量・反復上限を各runへ継承
//     time_limit_msとseedはこの関数で上書き。deadlineは外側の絶対上限
//     iteration_limitはrunごとの上限であり、合計上限ではない
// 出力:
//   best.feasible=trueならbest.labelが最良の成功解
//   0回で期限切れ、または全run失敗ならbest.feasible=false
//   bestの時間・反復数は1run分。全体値はtotal_*を使う
template <class Cost, class PairCost>
multistart_labeling_result<Cost> solve_labeling_multistart(
    const capacitated_graph_labeling_problem<Cost>& problem,
    const PairCost& pair_cost,
    int start_count,
    double total_time_ms,
    std::uint64_t first_seed,
    const capacitated_graph_labeling_options& common_options = {}) {
    assert(start_count > 0);
    assert(std::isfinite(total_time_ms) && total_time_ms >= 0.0);
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    const auto relative_deadline = start +
        std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double, std::milli>(total_time_ms));
    const auto deadline = std::min(relative_deadline, common_options.deadline);

    multistart_labeling_result<Cost> answer;
    for (int run = 0; run < start_count; ++run) {
        const auto now = Clock::now();
        if (now >= deadline) break;
        const double remaining_ms =
            std::chrono::duration<double, std::milli>(deadline - now).count();

        auto options = common_options;
        options.time_limit_ms = remaining_ms / (start_count - run);
        options.deadline = deadline;
        options.seed = first_seed + static_cast<std::uint64_t>(run);
        auto current = solve_capacitated_graph_labeling(problem, pair_cost, options);
        ++answer.runs_attempted;
        answer.total_iterations += current.iterations;
        if (current.feasible) {
            ++answer.runs_succeeded;
            if (!answer.best.feasible || current.objective < answer.best.objective) {
                answer.best = std::move(current);
            }
        }
    }
    answer.total_elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return answer;
}
```

## 9. 実装

### 9.1 アルゴリズムの概要と流れ

本ライブラリは、容量を守る初期解構築と、recolor・swapを組み合わせた焼きなましで目的値を小さくします。常に実行可能な状態の中で移動し、探索中の最良解を別に保持します。

1. 問題を検査し、全グラフのCSR隣接リストと変更可能範囲を構築する。
2. `solve`なら複数の構築試行から実行可能な初期解を選ぶ。`improve`・subsetなら渡された完全解を検査して読み込む。
3. 既に停止条件を満たす、動かせる頂点がない、Kが1、のいずれかなら初期状態を返す。
4. ラベル別の可変頂点リストを用意し、条件が合えばPotts差分表を構築する。
5. 移動候補の差分から温度の尺度を推定する。
6. recolorまたはswapを提案し、実行可能な候補を費用差分と温度で受理する。時間・反復上限まで繰り返す。
7. 通常APIは最良の完全解を、subsetは初期解から厳密に改善した場合の変更点を返す。

この処理は各公開呼出しの中で完結します。前回の内部状態を保持する永続エンジンはありません。

### 9.2 データの配置と隣接リスト

公開の問題型のprivate領域に、policy型をテンプレート引数とするsolverを置いています。利用者が内部型を構築・操作する必要はありません。補助的な移動型、乱数、CSRの要素も内部にあります。

CSRは、各頂点に接続する辺を連続領域へ並べる隣接リストです。各要素は相手頂点番号と元の辺IDを持ちます。自己辺は1要素、通常の辺は両端に1要素ずつ登録します。目的値へ辺を加えるのは1回ですが、どちらの端点が動いても、その辺へアクセスできます。

subsetでは頂点番号から可変範囲内の添字へ引く表と、少なくとも一端が可変な辺ID列を持ちます。全頂点が可変なら元の頂点・辺番号をそのまま使えます。固定頂点は候補集合から除きます。subsetへ固定頂点を明示して渡した場合は、黙って取り除かず範囲エラーにします。

### 9.3 実行可能初期解の構築

`solve`では最大`max(1, initial_trials)`回の貪欲構築を試します。まず固定頂点を配置して、その需要量と件数を差し引きます。上限を超えればその試行は失敗です。

残り頂点は、許可ラベル数が少ないもの、需要量が大きいものを優先します。その後の順序を試行ごとに変えます。

| 0始まり試行番号を4で割った余り | 構築の特徴 |
|---|---|
| 0 | 上の優先順位に続いて次数の大きい順。同順位は乱数 |
| 1 | 上の優先順位に続いてBFSで得た順序 |
| 2 | 上の優先順位に続いて乱数順。配置先は残容量の小ささを優先 |
| 3 | 上の優先順位に続いて乱数順 |

BFSは近い頂点を近い順序で扱うために使います。連結成分ごとに根を取り、キューへ追加した時点で順位を付けます。その順位が訪問済みの印も兼ねます。未訪問の根を探す位置を進めて保持し、非連結グラフで既訪問の先頭部分を繰り返し走査しません。

各頂点の配置先を選ぶとき、許可先と需要量・件数の上限を守ります。残っている需要量が全下限の不足量以下になった場合は、不足しているラベルを優先して満たします。件数についても同様です。需要量0の頂点は不足容量を消費しないため、需要量の下限を既に満たすラベルにも置けます。

通常の試行では、単項コストと既に割り当てた隣接頂点へのコストが小さい先を選びます。4回に1回のbest-fit試行では、配置後の残容量が小さい先を先に選び、その同順位を費用で比較します。同費用の候補は乱数で順位を付けます。

許可表を与えた場合の許可先の個数は、試行の前に1回集計します。Potts型では、現在の頂点に接続する割当済み隣接辺をラベル別に1回集計します。ラベル$a$への重み和を$H_a$、全割当済み隣接辺の重み和を$H$とすると、その辺費用は$H-H_a$です。候補ごとに共通な$H$を省き、単項費用から$H_a$を引いた値で比較できます。

構築の最後に全制約と全目的値を確認し、成功した構築解のうち最小目的値を保持します。貪欲配置には後戻りがないため、実行可能解が存在しても構築に失敗する場合があります。期限の確認は2回目以降の試行開始時で、1回の構築の途中では打ち切りません。

### 9.4 既存解とsubsetの初期化

`improve`とsubsetでは、初期構築試行をしません。完全ラベル列から全体の負荷と、必要なら件数を集計して制約を確認し、現在解と最良解へ読み込みます。

全体improveは全目的値を計算します。subsetは可変頂点の単項と、少なくとも一端が可変な辺だけを計算します。範囲外だけで決まる費用は定数なので省略できます。最終的に返す差分は、数学的には全目的値の差分と一致します。浮動小数点の場合は、全体を一度に足した計算との丸め差を考慮します。

### 9.5 移動候補の選び方

移動元頂点の選択では、全可変頂点からのランダム選択と、関係する辺の端点からの選択を各50%で混ぜます。後者は関係の多い頂点が選ばれやすくなります。subsetでは境界辺を含む可変部分の辺を使い、外側の端点が選ばれることはありません。

最初に試す近傍はrecolorが55%、swapが45%です。最初の種類で有効な候補が得られなければ、もう一方を試します。したがって、実際に成立する移動の比率が必ず55対45になるわけではありません。

| 近傍 | 変更 | 候補比較と実行可能性 |
|---|---|---|
| recolor | 1頂点のラベルを変更 | 最大`min(K-1,4)`候補。元と先の需要量・件数、許可条件を確認 |
| swap | 異なるラベルの2頂点を交換 | 先のラベル内から最大8頂点。両頂点の許可と交換後の容量を確認。件数は変わらない |

比較する候補はランダムに取り、重複することがあります。差分が最も小さい有効候補を提案します。全ての行先や全ての相手を走査するわけではありません。recolorでは、元ラベルから1頂点を出すだけで下限に違反すると分かる場合、行先を試さず終了します。

Potts型の行先生成は、隣接辺があれば87.5%で隣接関係を使い、残りは現在ラベル以外からランダムに選びます。非負辺では隣接頂点のラベルを候補にし、負辺では隣接頂点と異なるラベルを候補にします。この誘導結果が現在ラベルと同じなら無効候補になり得ます。一般policyと孤立頂点は、現在ラベル以外から選びます。

ラベル別の可変頂点リストからswap相手を直接取り出します。recolor後は元リストの末尾要素で削除位置の穴を埋め、新リストの末尾へ移します。swap後は2リストの対応位置を入れ替えます。リストからの削除・更新に全リスト走査は必要ありません。

### 9.6 一般の二項コストの差分

1頂点を動かすと変化するのは、その頂点の単項費用と接続辺だけです。それらについて新費用から旧費用を引きます。非対称コストは元の辺のu側・v側を保持して呼び、自己辺は両ラベルを同時に変更して`P_e(a,a)`を評価します。

swapでは、移動元uを相手ラベルへ置く差分を一度求め、同じ提案内の複数の相手候補で共有します。相手vの差分を加えるときは、uが既に移動した状態としてu-v間の費用を扱います。これにより直接辺を重複して誤計算せず、自己辺・多重辺・辺ID依存コストも追跡します。

差分評価では、ラベル列・CSR・辺列の連続配列をローカルのポインタから参照します。policyはテンプレート引数なので、仮想関数での動的な呼出しではありません。実際にインライン化されるかはコンパイラに依存します。

### 9.7 Potts差分表と隣接フィルタ

可変頂点vごとに、ラベルaを持つ隣接頂点への辺重み和を保持します。自己辺は除き、多重辺は全て数えます。

$$H(v,a)=\sum_{e\in I(v)}w_e[\ell_{o(e,v)}=a]$$

ここで$H$は差分表、$v$は可変頂点、$I(v)$は自己辺を除く接続辺の集合、$o(e,v)$は辺$e$のもう一方の頂点、$a$は隣接側のラベルです。境界辺の外側頂点も和に含みます。ラベル$b$から$a$へrecolorする差分は次です。

$$\Delta=U(v,a)-U(v,b)+H(v,b)-H(v,a)$$

候補の差分自体は定数時間で求まります。移動を受理したら、接続する可変頂点の差分表から旧ラベル分を引き、新ラベル分を加えます。受理後の更新には隣接辺の走査が必要です。

swapは両頂点のrecolor差分を合成し、異なるラベルを交換したu-v間の直接辺について、重み総和の2倍を補正として加えます。直接辺があり得る場合の確認には、次数の小さい側の隣接リストを走査します。

さらにメモリが許せば、可変頂点ごとに64bitの隣接フィルタを1本持ちます。相手頂点番号の下位6bitで位置を決め、そのbitを立てます。bitが立っていなければ確実に非隣接なので、swap補正の走査を省けます。違う頂点が同じbitへ重なる偽陽性はありますが、その場合は辺を実際に走査します。近似するのは隣接候補の事前判定であり、費用差分を省略・近似するものではありません。

差分表がメモリ上限に入らない場合、一般差分評価へ切り替わります。Potts型の候補誘導と初期構築の性質は使われますが、探索での表参照は使われません。キャッシュ有無によって乱数の整数生成経路も変わるため、同じseedでも最終解の一致は保証しません。

### 9.8 温度、受理、停止判定

探索前に最大256提案を調べ、非ゼロの有効差分を最大128個集めます。実際の解にはまだ適用せず、絶対値の中央値を尺度にします。標本がなければ尺度1、標本があれば最低`1e-12`とします。自動の開始温度は尺度の0.5倍、終了温度は0.03倍です。両温度を明示しても、この標本採取は実施します。温度値自体は最低`1e-12`に補正します。

進捗$p$は、指定された反復上限への割合と時間予算への割合の大きい方を0〜1へ丸めた値です。開始温度を$T_0$、終了温度を$T_1$とすると、次の指数的な変化を使います。

$$T(p)=T_0\left(T_1/T_0\right)^p$$

差分をΔとし、Δが0以下なら受理、正なら`std::exp(-Δ/T)`の確率で受理します。これは一時的な費用悪化を許す処理です。容量や許可などの制約違反は受理対象になりません。

温度・時刻はまとめて間欠更新します。反復上限Iが正なら、更新間隔は`min(1024, max(1, I/256))`反復です。Iが正でない場合は1,024反復です。1回の更新では同じ時計の読み取りを期限判定と経過時間計算へ使います。

### 9.9 最良解、乱数、結果出力

受理した現在解とは別に、最良目的値と最良ラベル列を保持します。各移動で変更した頂点を「前の最良解更新以降に変更された頂点」として記録し、次の厳密な改善が起きたとき、その頂点だけを最良ラベル列へ同期します。負荷と、必要なら件数は、最良解更新時にラベル数ぶんコピーします。

終了時は現在解でなく最良解を返します。subsetでは初期目的値より厳密に小さい場合だけ、初期ラベルと異なる可変頂点を抽出します。探索中に動いてから初期ラベルへ戻った頂点は変更点に含まれません。

乱数は64bitの内部状態から生成します。範囲付き整数の生成には、Potts差分表を使う探索では128bit積の上位を使い必要時に再抽選する方法を、その他では剰余による方法を使います。剰余による範囲変換には、範囲が2のべき乗を割り切らない場合にごく小さい偏りがあります。固定確率の分岐には乱数のbitまたは上位整数を直接比較し、受理確率はdoubleに変換した乱数で判定します。

同じseedでも、時間による停止点、入力順、キャッシュ有無、浮動小数点の丸めが異なれば経路は変わります。seedは探索状態を呼出し間で継続するためのハンドルではありません。

### 9.10 計算量とメモリ量

$N$は全頂点数、$K$はラベル数、$M$は全辺数、$Q$は実際に可変な頂点数、$M_S$は少なくとも一端が可変な辺数です。以下はpolicyを1回定数時間で評価できる場合の目安です。$\deg(v)$は頂点$v$の次数、$t$は実施した構築試行数を表します。

| 処理 | 計算量の目安 |
|---|---|
| 全目的値評価 | $O(N+M)$ |
| 実行可能性検査 | $O(N+K)$。assert有効時の問題検査では辺の走査も加わる |
| CSR・変更範囲の構築、初期解の読込み | $O(N+M+K)$ |
| Potts初期構築をt回 | $O(t(N\log N+NK+M))$。許可表があれば最初に$O(NK)$の集計 |
| 一般policyの初期構築をt回 | $O(t(N\log N+NK+KM))$ |
| Potts表の確保・構築 | $O(QK+M_S)$。全頂点可変なら$M_S=M$ |
| 一般recolor差分 | $O(\deg(v))$ |
| 一般swap差分 | $O(\deg(u)+\deg(v))$。同じ提案内でu側を共有 |
| Potts表によるrecolor差分 | $O(1)$ |
| Potts表によるswap差分 | 非隣接が確定すれば$O(1)$。補正走査時は$O(\min(\deg(u),\deg(v)))$ |
| 受理時のPotts表更新 | recolorは$O(\deg(v))$、swapは$O(\deg(u)+\deg(v))$ |
| 最良解の更新 | 変更頂点数＋Kに比例。ラベル列は変更点だけ、負荷・件数はラベル全体 |
| subsetの変更点抽出 | $O(Q)$ |

主な追加メモリは、CSRに$O(N+M)$、ラベル列・範囲索引・ラベル別リストなどに$O(N+K)$、Potts表に`sizeof(Cost)*Q*K`byte、隣接フィルタに最大`8*Q`byteです。許可表付きの初期構築では、許可先数に`sizeof(int)*N`byteを使います。入力の単項表と許可表は、それぞれN×K要素です。

探索1反復では候補を複数比較しますが、recolorは最大4、swapは最大8という定数上限があります。高次数頂点、一般policyの重い計算、頻繁な最良解更新、準備時間の比率により実行時間は変わります。計算量だけから、特定サイズが特定のミリ秒以内に解けるとは判断できません。
