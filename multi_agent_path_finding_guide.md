# 複数エージェント経路計画・搬送ライブラリ 利用ガイド

対象は `multi_agent_path_finding_v04.hpp`。C++20の単一ヘッダで、複数の移動体の経路計画と搬送タスクの割当を扱う。本書はこのヘッダの公開APIと実際の動作を説明する。

読み方：まず第1〜4章で問題への適合性を判断し、第5〜7章で入出力と制約を確認する。第8章はコピーして使える関数例、第9章は実装の説明である。ユースケース番号 `U01`〜`U18` は第4・6・8章で共通。

1. [何を解くライブラリか？](#1-何を解くライブラリか)
2. [厳密解アルゴリズム](#2-厳密解アルゴリズム)
3. [差分更新](#3-差分更新)
4. [ユースケース](#4-ユースケース)
5. [利用準備](#5-利用準備)
6. [ユースケースごとの使い方](#6-ユースケースごとの使い方)
7. [制約・注意点](#7-制約注意点)
8. [ユースケースごとのコード例](#8-ユースケースごとのコード例)
9. [実装](#9-実装)

## 1. 何を解くライブラリか？

### 1.1 移動のルール

駅構内の台車、倉庫内のロボット、盤面上の駒など、複数の移動体を同時に動かす問題を考える。移動できる場所を頂点、1ターンで行き来できる場所の組を無向辺とする。辺の重みはなく、全員が同じ長さのターンで動く。

各エージェントは1頂点を占有し、1ターンに「隣接頂点への移動」または「その場で待機」を選ぶ。次の2種類の衝突は禁止する。

- **頂点衝突**：同じ時刻に2体が同じ頂点にいる。
- **正面衝突**：同じターンに一方が辺を往路、もう一方が復路へ進む。

先行する1体が空けた頂点に次の1体が入る追従移動や、3体以上の環状移動は、この2条件に違反しなければ許される。連続空間での接触判定、ロボットの向きや大きさ、加減速は含まない。

### 1.2 始点と終点が決まった経路計画：MAPF

MAPF（Multi-Agent Path Finding）では、各エージェントの始点と専用の終点が入力で決まっている。「どの終点へ誰を割り当てるか」は入力側の仕事である。

| 記号 | 意味 |
| --- | --- |
| $G=(V,E)$ | 移動グラフ。$V$ は頂点集合、$E$ は無向辺集合 |
| $N=\lvert V\rvert$, $M=\lvert E\rvert$ | 頂点数と、重複を除いた無向辺数 |
| $K$ | エージェント数。1以上 |
| $a\in\{0,\ldots,K-1\}$ | エージェント番号 |
| $s_a$, $g_a$ | エージェント $a$ の始点と終点 |
| $x_a(t)$ | 整数時刻 $t\geq0$ におけるエージェント $a$ の頂点 |
| $T_a$ | 返却経路の終端時刻。以後は $g_a$ で待機する |
| $S$ | 全エージェントの経路時間の合計（sum of costs） |
| $L$ | 最後のエージェントの終端時刻（makespan） |

返却経路は $x_a(0)=s_a$、$x_a(T_a)=g_a$ を満たし、$t>T_a$ では $x_a(t)=g_a$ と解釈する。途中で一度終点を訪れてから離れる場合、その最初の訪問は完了ではない。

$$S=\sum_{a=0}^{K-1}T_a,\qquad L=\max_{0\leq a<K}T_a$$

指定できる目的は次の2つで、いずれも小さい値を好む。

$$\operatorname{lexmin}(S,L)\qquad\text{または}\qquad\operatorname{lexmin}(L,S)$$

`lexmin` は辞書順最小化を表す。左側の値が良い候補を優先し、左側が同じときだけ右側で比べる。重み付き和ではない。

- `sum_of_costs`：$(S,L)$ を比較する。全員の拘束時間・到着までの待ち時間を減らしたい場合に適する。
- `makespan`：$(L,S)$ を比較する。全員が配置につくまで次の工程を開始できない場合に適する。

経路中の移動と待機は、どちらも $T_a$ を1増やす。終端後に暗黙に行う待機は加算しない。任意の報酬関数、エージェントごとの重み、総移動回数だけの最小化は指定できない。

### 1.3 集荷と配達を繰り返す搬送計画：MAPD

MAPD（Multi-Agent Pickup and Delivery）では、各エージェントは初期位置から出発し、集荷地点へ行って荷物を受け取り、配達地点へ運ぶ。誰がどのタスクを担当するかもライブラリが決める。1体が同時に扱うタスクは1件で、集荷・配達の作業時間は0とする。

| 記号 | 意味 |
| --- | --- |
| $J$ | 入力されたタスク数。0件でもよい |
| $j\in\{0,\ldots,J-1\}$ | 入力順のタスク番号 |
| $p_j$, $q_j$ | タスク $j$ の集荷頂点と配達頂点 |
| $r_j$ | タスク $j$ の解禁時刻。これより前は割当も集荷もできない |
| $H$ | 計画する最終時刻。時刻0から $H$ までを返す |
| $c_j$ | タスク $j$ の配達完了時刻。未完了なら定義しない |
| $\mathcal C$ | $H$ までに配達完了したタスク番号の集合 |
| $F$ | 完了タスクのフロー時間（解禁から配達完了まで）の合計 |
| $Q$ | 全エージェントの実移動回数。待機を含まない |
| $x_a(t)$, $K$, $a$ | 位置、エージェント数、番号。1.2節と同じ意味 |
| $\mathbf1[P]$ | 条件 $P$ が真なら1、偽なら0となる指示関数 |

$$F=\sum_{j\in\mathcal C}(c_j-r_j),\qquad Q=\sum_{a=0}^{K-1}\sum_{t=0}^{H-1}\mathbf1[x_a(t+1)\ne x_a(t)]$$

$$\operatorname{lexmin}(-\lvert\mathcal C\rvert,F,Q)$$

まず完了数を最大化し、同じなら $F$ を小さくし、それも同じなら $Q$ を小さくする。未完了タスクは $F$ に含めないため、完了数の異なる2結果を $F$ だけで比較してはいけない。待機中や未完了タスクのための移動も、実際に動いた分は $Q$ に含まれる。

返却結果が有効であることと、全タスクが完了していることは別である。計画の途中で打ち切ると、未割当・集荷前・搬送中のタスクが残り得る。

### 1.4 既存の計画の一部を直す問題

全員の経路が既にある場合、指定したエージェントだけを再計画できる。さらに、時刻区間の内側だけを変更し、区間の両端位置と外側の予定を固定することもできる。他システムが決めたロボットの経路、時刻付きの通行禁止も追加制約として与えられる。

目的は1.2節のMAPFと同じだが、固定した経路・区間と外部制約を満たす範囲で比較する。新しい制約のために、元より長い経路が必要になることはある。

本ライブラリは実行可能な計画を返すためのもので、最適性や、解が存在する場合の発見を一般には保証しない。失敗結果は不可解性の証明ではない。

## 2. 厳密解アルゴリズム

### 2.1 条件を固定すると厳密に解ける場合

次の選択肢は、本ライブラリを使うか、別の厳密ソルバーを使うかを決める観点になる。厳密手法が常に高速という意味ではない。

| 条件・制限 | 厳密に求められるもの | 判断のポイント |
| --- | --- | --- |
| 1体だけ、時刻付き障害なし | BFSによる最短経路。時間 $O(N+M)$ | この条件だけなら専用BFSで十分 |
| 他の経路・外部予約を全て固定し、動かすのが1体、有限の $H$ | 頂点と時刻を状態にしたBFS／適切なA*で、固定制約下の最短終端時刻 | 終点に到着できるだけでなく、その後も保持できることを確認する |
| 各エージェントが独立な通行領域を使う | 個別最短路の集合 | 独立最短路が互いに衝突しない場合も、個別距離の下界を達成し、MAPFの両目的で最適 |
| エージェント数・頂点数が非常に小さい | 全員の位置をまとめた状態探索 | 同期状態のBFSでmakespanを最小化できる。SOCには完了状態を含めた適切な費用設計が必要 |
| 移動ターン上限 $H$ とタスク数が小さい | 時間展開した整数計画、SAT／制約プログラミングなど | 全衝突条件・終点保持・目的関数を正しく表現すれば、その有限範囲内の厳密解を求められる |
| 終点が個体別でなく、誰がどの終点に着いてもよい | anonymous MAPFの時間展開ネットワークフロー | 固定された個体別終点とは別問題。適切な衝突防止構成でmakespanの厳密最小化が可能 |

時刻展開では、状態を $(v,t)$ とし、合法な待機・移動だけを次時刻へ接続する。1体なら状態数は $N(H+1)$、遷移数は $O(H(N+M))$。ここで $v\in V$ は頂点、$t$ は時刻である。他の経路を固定して得た最適解は、その固定条件の下での最適解にすぎない。

全員の位置の組は最大で $N^K$ 通りになり、同時移動の組合せも増える。SOCを扱う場合は「終点を一時的に通過した」と「以後そこに留まる」を区別し、未完了エージェントに対応する費用を設計する必要がある。単に位置の組に対してBFSしただけではSOC最適にはならない。

anonymous MAPFとネットワークフローの関係、および目的関数ごとの違いは、[Yu・LaValleの論文](https://arxiv.org/abs/1204.5717)と[anonymous MAPFの厳密makespan最適化の論文](https://ojs.aaai.org/index.php/AAAI/article/view/29676/31156)を参照。本ライブラリの `solve_mapf` は個体別終点を入力するため、この匿名割当問題を直接解くAPIではない。

### 2.2 一般のMAPFに対する厳密手法

CBS（Conflict-Based Search）やICTS（Increasing Cost Tree Search）は、MAPFを厳密に解く代表的な手法である。CBSは個別経路の衝突を解消する制約に分岐し、ICTSはエージェント別の費用の組を探索する。[CBSの原論文](https://ojs.aaai.org/index.php/AAAI/article/view/8140)、[ICTSの原論文](https://www.bgu.ac.il/~felner/2011/guni.pdf)を参照。

最適性が必須、小規模な部分問題を何度も解く、近似解の誤差を評価するための正解が必要、といった場合は厳密手法が候補になる。目的がmakespanかSOCか、終点保持、外部制約の扱いが一致する実装を選ぶこと。密度や相互干渉によって計算量が大きく変わるため、頂点数だけで一律の切替閾値は決められない。

本ヘッダにはCBS・ICTS・全体整数計画は含まれない。小さい部分問題だけを外側の厳密ソルバーで処理し、その経路を本ライブラリの基準解や外部予約として使う組込み方は可能である。

### 2.3 搬送問題の「割当の最適」と「全体の最適」

その瞬間の空きエージェントと候補タスクを固定し、各組の費用を与えた二部割当は、最小費用流で厳密に解ける。本ライブラリの `min_cost_flow` も、候補対の割当数最大、次にその時点の割当費用和最小を求める。

しかし、その費用は混雑や将来のタスク競合を全て表したものではない。したがって、MAPD全期間の完了数・フロー時間・移動回数の厳密最適化ではない。MAPD全体の厳密性が必要なら、タスク担当・順序・移動・時刻・1体1タスク制約を合わせてモデル化する。有限の小さな問題では全探索や整数計画が選択肢になるが、規模が増えると急速に重くなる。

## 3. 差分更新

### 3.1 何を引き継げるか

MAPFでは「前の解」と「探索用workspace」を別々に引き継ぐ。workspaceは距離表や配列を保持するが、呼出しで得た経路を記憶して自動で初期解にするものではない。

| 後から変えるもの | 使い方 | 再利用と制限 |
| --- | --- | --- |
| 乱数seed・目的・探索予算 | オプションを変えて呼び出す | 同じグラフなら距離表と探索領域を再利用できる |
| 終点の割当候補 | `solve_mapf` を同じworkspaceで反復 | 既知終点の距離表を利用。新終点は追加計算。前の解を使うなら `repair_mapf` |
| 選択したエージェントの終点 | 全経路repair | 非選択エージェントの経路と終点は固定 |
| エージェントを選んだ局所改善 | 同じ終点で全経路repair | 基準解が新条件でも有効なら目的値は悪化させない |
| 一定時刻区間の経路 | 時間窓repair | 区間の両端位置と区間外の位置を固定。勝手には窓を拡張しない |
| 時刻付きの頂点・移動禁止 | 外部予約を作ってrepair | グラフを変えずに扱える。外部予約の距離は前計算へ反映せず、経路探索時に制約として使う |
| 確定済み外部ロボットの経路 | 外部予約を作ってrepair | 頂点占有と正面衝突を回避。外部経路自体は動かさない |
| 計画horizon | オプションを変更 | 必要なら探索配列を拡張。短くしても確保容量が即座に縮むとは限らない |
| 同じグラフ上の新しい始点 | 新規solve、または現在からの基準経路を作ってrepair | 始点だけを差し替えて古い経路を渡すことはできない |
| エージェントの増減・番号変更 | 整合した入力一式で新規solve | 距離cacheは利用可能。repairでは基準経路と対象番号の整合が必要 |
| 恒久的な辺・頂点の変更 | workspaceを `clear()` し、グラフを再構築 | 距離表の部分更新APIはない。現在グラフで不正になった基準経路はrepairへ渡せない |
| 外部予約の削除・移動・期間変更 | 予約オブジェクトを作り直す | 公開の解除・resize・削除APIはない |
| MAPDの新しいタスク・搬送状態 | 入力を組み直して `solve_mapd` | 状態継続APIも共有workspace APIもない。完全な途中再開にはならない |

`prepare(graph, targets)` は同じグラフオブジェクトと頂点数を識別して距離表を扱う。同じオブジェクトをその場で再代入・編集した場合、辺の内容の変更を自動検出しない。グラフを変更する前に `workspace.clear()` を呼ぶ。グラフの寿命・アドレスを安定させ、同一アドレスの別オブジェクトへの再利用でも明示的にclearするのが安全である。

### 3.2 対話的な1ターン後の使い方

外側の問題で $\tau$ ターン経過したとする。$\tau$ は前の計画の時刻原点からの経過時間である。

1. 実際の現在位置を観測する。
2. 前の計画どおりなら、実行済みの接頭辞を取り除く。各短い経路の終端後は終点待機として扱う。
3. 現在を新しい時刻0にする。新しい始点は現在位置、horizonは残りターン数とする。
4. 外部予約も同じ原点へ変換する。旧時刻 $t$ の事象は、新時刻 $t-\tau$ の事象となる。
5. 残りの基準経路が現在グラフで有効なら改善・repairに使う。観測が予測と異なる、または経路が不正になった場合は現在から作り直す。

U12の例は、この判断を行った上で現在からの経路を返す。過去の実行済み操作を修正したかのように扱わない。既に開始時刻を過ぎた外部ロボットは、その経路も切り詰め、現在の位置を新時刻0に置く必要がある。

### 3.3 効率上の限界

部分repairで再探索するエージェント数は減らせるが、入力検証、固定経路の予約、返却経路の構築は残る。したがって、計算時間全体が選択数だけに比例するわけではない。時間窓repairも、前後を含めた有効性確認と全経路出力を行う。

workspaceは再開可能なA*の探索木ではない。中断した探索の途中からの再開、グラフ変更に対する動的最短路更新、MAPDの荷物・担当・乱数状態の保存と復元は提供しない。

## 4. ユースケース

この章は用途と入力の意味を説明する。対応する呼出しは第6章、完全な関数例は第8章にある。

### 4.1 U01：固定された担当先へ全員を移動させる

複数の駒を所定の配置へ動かす、台車をそれぞれの作業位置へ運ぶ、といった用途。必須入力は移動可能な場所の関係、全員の現在地と担当先。全員の合計所要時間を減らすか、最後の1体の到着を早めるかを選ぶ。移動できる最大ターン数と計算時間の予算も指定できる。

### 4.2 U02：障害物付きの盤面を経路問題として使う

壁のある2次元盤面で、複数の駒を上下左右へ動かす用途。必要なのは盤面、各駒の開始座標と終了座標。壁を頂点から取り除き、通行可能なマスだけで問題を作る。斜め移動やワープが必要なら、盤面変換ではなく、その移動規則をグラフとして明示する。

### 4.3 U03：短い時間で初期の実行可能解を確保する

外側の探索を始める前に、まず使える計画が欲しい場合。入力は固定担当の経路問題と短い計算予算。少ない試行で結果を得ることを優先するため、解品質や成功率が十分でなければ、より広い探索へ切り替える。指定ミリ秒内の成功を保証する使い方ではない。

### 4.4 U04：時間の揺らぎを除いて設定を比較する

乱数や反復数を固定し、目的値や計算量を再比較したい場合。入力問題に加え、方式、乱数seed、構築試行数、改善反復数、改善対象数を固定する。壁時計の時間で途中停止しないため、処理の違いを調べやすい。一方、対話問題の厳しい応答期限には、そのまま使わない。

### 4.5 U05：終点の割当候補を外側から評価する

「どのロボットをどの作業位置へ向かわせるか」を外側で探索する場合。必須入力は同じ移動グラフと始点、複数の終点割当候補。候補ごとの経路を計画して、成功候補の目的値を比較する。ここで比較するのは与えた候補集合であり、未列挙の全割当に対する最適性は主張しない。

### 4.6 U06：別の方法で作った有効解を磨く

手作り、貪欲法、外部ソルバーなどで既に衝突のない経路がある場合。始点・終点・その経路を渡し、改善の反復数や一度に動かす対象数を決める。有効な初期解を保ちながら、指定目的を改善する。衝突した初期解を修正するための入口ではない。

### 4.7 U07：一部の担当先だけ変更する

注文の変更などで数台だけ行き先が変わり、残りの予定を動かしたくない場合。必要なのは基準経路、新しい全員分の担当先、再計画してよいエージェント集合。担当先を変えた全員をその集合へ含める。非選択の経路を守る制約のため、全体を解き直せば解ける場合でも失敗し得る。

### 4.8 U08：自分で選んだ近傍だけ再最適化する

外側の局所探索が「この数台をまとめて動かし直すとよさそう」と判断する場合。行き先は変えず、基準経路と対象集合を指定する。自動的な対象選択に任せるU06と違い、外側で選んだ集合を厳密に守る。目的値が同じでも経路の形が変わる場合がある。

### 4.9 U09：予定の前後を保って時間区間の内側だけ迂回する

ある数ターンだけ障害物が現れるが、その前の実行済み操作や後の受渡し予定は維持したい場合。必須なのは有効な基準経路、対象集合、変更できる開始・終了時刻。必要なら時刻付きの外部制約も渡す。窓の始端と終端の位置は固定され、迂回しても終端までに元の予定へ戻る必要がある。

最終到着が窓より後に固定されていれば、窓内の経路を変えてもSOCやmakespanが変わらないことがある。この用途では、到着スコアの改善だけでなく、新しい障害を避けて予定を成立させることが重要になる。

### 4.10 U10：一時的なマス閉鎖・辺閉鎖を追加する

工事や別工程の占有で、特定時刻の場所や移動を禁止したい場合。場所の禁止には頂点と時刻、移動の禁止には出発頂点・到着頂点・出発時刻を指定する。辺の両方向閉鎖と片方向閉鎖を区別できる。グラフ自体を変更せず、選んだエージェントの経路全体を直す。

### 4.11 U11：他システムが動かすロボットと共存する

自分の担当するロボットは計画できるが、別グループの経路は動かせない場合。自グループの始点・終点に加え、外部ロボットの時刻付き経路を与える。外部ロボットが経路終了後もその場に残るか、対象から外れるかを明示する。自グループの初期計画がまだなくても利用できる。

### 4.12 U12：毎ターンの観測を受けて計画し直す

1ターンだけ操作して新しい観測を得る対話型の問題。前の計画、経過ターン数、観測した現在位置、新しい行き先が必要。予測どおりなら未実行の経路を利用し、予測と違えば現在から構築する。外部制約と移動上限は、現在を時刻0とした値へ直して入力する。

### 4.13 U13：通路の開閉などでグラフ自体が変わる

通路が恒久的に追加・削除される、通行可能なマスの集合が変わる場合。変更後の頂点数と辺集合、変更後の番号系での現在地と行き先を与える。使えなくなった辺を含む経路は基準にできないため、この例ではキャッシュとグラフを再構築して新しく解く。

### 4.14 U14：初期構築と改善で同じ計算予算を分け合う

複数の処理が外側の終了時刻を共有する場合。共通の終了時刻、問題入力、改善の設定を用意する。最初に実行可能解を構築し、残り時間で改善する。各段階に最初の予算を丸ごと与え直さず、同じ終了時刻を見る。ただし厳密な実時間停止ではない。

### 4.15 U15：失敗理由に応じて限定的に再試行する

移動上限が短すぎるのか、探索が解を見つけなかったのかを分けて扱いたい場合。最初の移動上限、拡張してよい上限、試行回数、各試行の時間予算を指定する。入力誤りや到達不能を乱数変更で解決しようとはしない。成功しなかったという理由だけで無限に再試行しない。

### 4.16 U16：経路を同期操作列へ変換する

経路は得られたが、問題の出力はターンごとの全員の移動指示である場合。始点・終点・結果経路と、使用した外部制約を入力する。短い経路には終点待機を補い、各ターンの移動元・移動先へ変換する。具体的な文字や操作番号への対応付けは問題側で行う。

### 4.17 U17：事前登録した搬送タスクを実行する

全注文が分かっている配送シミュレーションや、将来の解禁時刻が分かる搬送問題。各ロボットの初期位置、各タスクの集荷地点・配達地点・解禁時刻、操作ターン上限を指定する。全件即時に着手可能なら解禁時刻を0にそろえる。割当方式と計算予算を選び、担当と実際の経路を同時に得る。

### 4.18 U18：期限内の完了数と残作業を評価する

全件完了より、決まったターンまでに何件運べるかが重要な用途。U17の搬送モデルを使い、返却時のタスクを完了・未割当・集荷待ち・搬送中へ分類する。完了数だけでなく同点時のフロー時間や実移動回数も利用する。残作業の分類は評価や外側の判断に使うもので、そのまま内部状態の再開データになるわけではない。

## 5. 利用準備

### 5.1 ビルドと基本的な形

必要なのはC++20、GCC系の標準ライブラリ（`<bits/stdc++.h>` を使用）、AtCoder Libraryの `atcoder/mincostflow` とその依存ヘッダ。`hash_map.hpp` は不要。MAPFだけを使う場合も、このヘッダ全体がACLをincludeする。

```cpp
#include "multi_agent_path_finding_v04.hpp"
```

`ac-library/atcoder/mincostflow` が存在する配置なら、例えば次でビルドする。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -I. -Iac-library main.cpp -o main
```

solverオブジェクトを構築して `.solve()` を呼ぶ形式ではない。公開の自由関数 `solve_mapf`、`improve_mapf`、`repair_mapf`、`solve_mapd` を呼ぶ。必要なデータ型はグローバル名前空間にある。

### 5.2 `mapf_graph`：移動可能な場所

| コンストラクタ・公開操作 | 型・既定状態 | 設定・利用方法 |
| --- | --- | --- |
| `mapf_graph()` | 空。`vertex_count=0`、`offsets`・`edges` は空 | 代入先を用意するとき。空のままsolverには渡さない |
| `mapf_graph(n, undirected_edges)` | `n: int`、`const vector<pair<int,int>>&`。引数の既定値なし | 頂点番号は `0`〜`n-1`。各辺を1回渡せば両方向になる。自己ループを無視し、重複辺を除く |
| `size()` | `int` | 頂点数を返す |
| `neighbors(v)` | `std::span<const int>` | 昇順の隣接頂点列。`v` は範囲内必須 |
| `degree(v)` | `int` | 頂点 `v` の次数。範囲内必須 |
| `vertex_count` | `int`、既定値 `0` | 公開だが通常は直接変更しない |
| `offsets`, `edges` | `vector<int>`、既定値は空 | CSR表現。通常はコンストラクタに任せる |

CSRを直接組み立てるなら、`offsets.size()==n+1`、先頭0、非減少、末尾が `edges.size()`、全隣接番号が範囲内、隣接列が昇順かつ重複なし、両方向の対称性を利用者が保証する。solverは壊れたCSRを安全に診断するためのものではない。

コンストラクタは `n>=0` と辺番号をassertで検査する。外部から未検証の辺を読む場合は、assert任せにせず構築前にチェックする。U02ではマスを頂点へ変換し、U13では変更後の辺集合から構築する。

### 5.3 MAPFの始点・終点

`starts` と `goals` は `std::span<const int>` として受け取る。`std::vector<int>` や `std::array<int,N>` をそのまま渡せる。

- 両配列の長さは同じで、1以上。
- `starts[i]` と `goals[i]` はエージェント `i` の頂点番号。
- 始点同士、終点同士はそれぞれ重複不可。他エージェントの始点と自分の終点が同じでもよい。
- `starts[i] == goals[i]` は許される。必要なら他エージェントのために一時的に終点から離れることもある。
- 入力配列は呼出し中に生存し、変更されないこと。

### 5.4 `mapf_options` の全パラメータ

以下の推奨範囲は開始点であり、入力に依存しない最良値の保証ではない。選択したAPIで実際に有効になる項目は7.2節も参照。

| メンバ | 型 | デフォルト | 意味と値の選び方 |
| --- | --- | --- | --- |
| `strategy` | `mapf_strategy` | `automatic` | `prioritized`、`pibt`、`automatic`。一般用途はautomatic、短い初期構築はpibtから検討 |
| `objective` | `mapf_objective` | `sum_of_costs` | `sum_of_costs` はSOC優先、`makespan` は全員完了時刻優先。外側スコアに合わせる |
| `time_limit_ms` | `double` | `1000.0` | 計算のソフト時間制限。有限の負値で相対時間制限なし。0も時間制限として有効。外側の期限より余裕を取る |
| `seed` | `std::uint64_t` | `1` | 乱数seed。再現試験では固定し、別候補を探すとき変更 |
| `max_time` | `int` | `-1` | 非負なら移動ターン上限、負値なら自動値。短いほど省メモリだが、迂回・待機の余裕が減る |
| `prioritized_restarts` | `int` | `8` | prioritizedでは最低1回、pibtでは1〜8へ丸める。automaticでは通常の構築失敗後の追加試行数。repairでは選択集合の異なる順序数が上限 |
| `lns_iterations` | `int` | `64` | 改善反復の上限。0以下で無効。64から始め、時間と改善量を見て増やす |
| `lns_size` | `int` | `8` | 一度の改善で選ぶエージェント数。0以下で無効、正値は最大 $K$ に丸める。まず4〜10程度を試す。大きいほど1反復が重い |
| `congestion_weight` | `int` | `0` | 経路探索の同点順位に用いる混雑重み。非負かつ演算がoverflowしない範囲。まず0。正値は経路形状を変えるが、スコア改善を保証しない |
| `deadline` | `const mapf_deadline*` | `nullptr` | 共通の絶対終了時刻。相対制限も有効なら早い方を使う。オブジェクトの寿命を呼出し終了まで保つ |

`congestion_weight` は `automatic` のユーザー設定としては使われない。automaticの通常構築と改善は0、追加構築は1を内部指定する。`prioritized`、単独 `improve_mapf`、repairでは指定値が使われる。

#### 自動horizonと明示値

最大独立最短距離を $D$、基準経路から再計算したmakespanを $L_0$ とする。全経路repairの $D$ は選択エージェントだけの新終点までの最大距離であり、$K$ は全エージェント数である。

$$H_{\mathrm{auto}}=D+\max(16,D,K)$$

| API | `max_time < 0` | 非負値を指定した場合 |
| --- | --- | --- |
| `solve_mapf` | $H_{\mathrm{auto}}$ | $D$ 未満なら `horizon_too_short` |
| `improve_mapf` | $\max(L_0,H_{\mathrm{auto}})$ | $L_0$ または $D$ 未満なら有効な基準解を返す |
| 全経路 `repair_mapf` | $\max(L_0,H_{\mathrm{auto}})$ | まず $L_0$ 以上が必要。その後、外部予約が長ければ予約のhorizonまで拡張する。最終値が $D$ 未満なら失敗 |
| 時間窓 `repair_mapf` | `end_time - begin_time` | `options.max_time` 自体を使わない |

全経路repairは自動値の場合も、外部予約のhorizon以上まで拡張する。空の選択集合は基準解の検査だけで返るため、このhorizon設定・検査経路を通らない。

### 5.5 `mapf_deadline`：共有の終了時刻

| 操作・メンバ | 型・デフォルト | 意味 |
| --- | --- | --- |
| `clock` | `std::chrono::steady_clock` の別名 | 単調時計。壁時計の日付とは異なる |
| `mapf_deadline()` | `end=clock::time_point::max()` | 実質的な期限なし |
| `mapf_deadline(end_time)` | `clock::time_point`、引数必須 | 絶対終了時刻を直接設定 |
| `after_ms(milliseconds)` | `static mapf_deadline`、`double`、引数必須 | 現在から指定ms後。負値なら期限なし |
| `expired()` | `bool` | 現在が終了時刻以降か |
| `end` | `clock::time_point` | 公開の終了時刻 |

NaN・無限大や時計の表現範囲を超える値は使わない。複数solverへ同じdeadlineのポインタを渡すと、各呼出しの開始からではなく共通の終了時刻を見る。U14では構築と改善で共有する。

### 5.6 `mapf_workspace`：反復呼出しの作業領域

| 操作 | 宣言・既定状態 | 用途 |
| --- | --- | --- |
| 構築 | `mapf_workspace()`。空の作業領域 | 反復の外側で1回用意 |
| コピー／move | コピー不可、move可能（`noexcept`） | グラフを所有する型ではない |
| `prepare` | `bool prepare(const mapf_graph&, std::span<const int> targets)` | 指定終点の距離を前計算。solverが自動で呼ぶので通常は省略可能 |
| `cached_target_count` | `int cached_target_count() const` | 現在cache済みの異なる終点数 |
| `clear` | `void clear()` | 距離cacheと主要探索領域を破棄。グラフ内容変更の前に呼ぶ |

`prepare` は正の頂点数と範囲内のtargetを要求し、不正ならfalseを返す。空のtarget集合は許される。既知targetは再計算しないが、新しいtargetを追加するほどcacheが増える。割当候補を大量に評価する場合、全頂点に近い数のtargetが蓄積することもある。`clear()` は小さな補助vectorの容量まで必ず全て解放する操作ではない。

### 5.7 `mapf_repair_request`：変更してよい範囲

| メンバ | 型 | デフォルト | 設定方法 |
| --- | --- | --- | --- |
| `agents` | `std::span<const int>` | 空 | 再計画してよいagent番号。範囲内、重複なし。元配列を呼出し終了まで生存させる |
| `begin_time` | `int` | `0` | 時間窓の開始。全経路修復では0必須 |
| `end_time` | `int` | `-1` | 負値なら全経路、非負なら時間窓。窓では `0 <= begin < end <= L_0` |
| `external_reservations` | `const mapf_reservations*` | `nullptr` | 動かせない外部予約。時刻は基準経路の時刻原点に合わせる |

空の `agents` は「全員を修復」の意味ではない。基準解が指定goalと外部予約に適合するかを確認し、有効なら返すだけである。

### 5.8 `mapf_reservations`：外部制約の作成

| コンストラクタ・メンバ | 型・デフォルト | 意味 |
| --- | --- | --- |
| `mapf_reservations()` | `vertex_count=0`, `horizon=-1`, `owner` は空 | 未設定。通常 `valid()` はfalse |
| `mapf_reservations(n, max_time)` | 2引数とも `int`、既定値なし | 頂点数と非負の最終時刻を指定。全セルを空き状態に初期化 |
| `vertex_count`, `horizon` | `int` | グラフと同じ頂点数、予約の最終時刻 |
| `owner` | `vector<int>` | 時刻優先の占有表。通常は直接編集せず、下記メソッドを使う |

| メソッド | 引数・返却型 | 意味と失敗条件 |
| --- | --- | --- |
| `valid()` | `bool` | 寸法と配列長の整合を検査。グラフ上の経路合法性は検査しない |
| `valid_time_vertex(time, vertex)` | 2つの `int` → `bool` | `0 <= time <= horizon`、頂点範囲内か |
| `at(time, vertex)` | 2つの `int` → `int` | 占有値を取得。範囲内必須、assertあり |
| `block_vertex(time, vertex)` | 2つの `int` → `bool` | その時刻の頂点占有を禁止。範囲外や外部経路の占有と重なるとfalse。同じ禁止の再登録はtrue |
| `block_move(time, from, to)` | 3つの `int` → `bool` | `time` から `time+1` への指定方向移動を禁止。`0 <= time < horizon` と範囲内頂点が必要 |
| `block_edge(time, u, v)` | 3つの `int` → `bool` | 同時刻の両方向を禁止。頂点自体は通行不能にならない |
| `reserve_path(start_time, path, hold_until=-1)` | `int`, `span<const int>`, `int` → `bool` | 外部経路を登録。空経路、範囲外、既存占有・禁止移動・外部経路との正面衝突等でfalse |
| `move_is_blocked(time, from, to)` | 3つの `int` → `bool`（const） | 明示的な禁止移動かを照会。範囲外はfalse。占有や外部経路との衝突を全て調べる関数ではない |

`path[0]` が `start_time` の位置である。`hold_until` が負値なら経路末尾の時刻までしか予約しない。それ以降も同じ頂点にいる場合は、その最終時刻を明示する。終端待機の暗黙継続があるMAPF結果とは意味が異なる。

`reserve_path` はグラフを引数に取らないので、隣接していない頂点への飛び移りを検出できない。外部経路の移動合法性は呼出側で確認する。U11の例は確認してから登録する。禁止移動も、それが実在する辺かどうかは検査しない。`from == to` はその時刻の待機禁止として扱える。

`owner` の `-1` は空き、`-2` は頂点障害、`-3` 以下は外部経路の識別値。経路ごとの識別が正面衝突判定に必要なため、外部ロボットを単なる頂点障害の列で代用しない。

### 5.9 MAPDの入力とオプション

| 型・メンバ | 型 | デフォルト | 設定方法 |
| --- | --- | --- | --- |
| `mapd_agent::start` | `int` | `-1` | 範囲内の初期位置。全agentで異なること |
| `mapd_task::pickup` | `int` | `-1` | 集荷頂点 |
| `mapd_task::delivery` | `int` | `-1` | 配達頂点。pickupと同じでもよい |
| `mapd_task::release_time` | `int` | `0` | 解禁時刻。非負。将来のタスクも最初に登録する |

異なるタスクが同じ集荷・配達頂点を共有してもよい。少なくとも1体、正の頂点数が必要。タスクは0件でもよい。各タスクに担当agentを事前指定するメンバはない。

| `mapd_options` メンバ | 型 | デフォルト | 意味と選び方 |
| --- | --- | --- | --- |
| `max_time` | `int` | `1000` | 非負の操作ターン上限。計算時間ではない。出力各経路の長さはこれに1を加えた値 |
| `time_limit_ms` | `double` | `1000.0` | ソフト計算時間制限。有限の負値なら相対時間停止なし |
| `seed` | `std::uint64_t` | `1` | 再現には固定。別の候補を得るとき変更 |
| `assignment` | `mapd_assignment_strategy` | `automatic` | 下表の方式を選択 |
| `portfolio_runs` | `int` | `8` | automaticが比較する候補数を1〜8へ丸める。他方式では不使用。予算が短ければ候補を減らすが、品質は入力依存 |
| `deadline` | `const mapf_deadline*` | `nullptr` | 共通の終了時刻。呼出し中の寿命を保証 |

| `mapd_assignment_strategy` | 選び方 |
| --- | --- |
| `automatic` | 異なる方式・seedの候補を比較する標準設定。予算内で複数候補を試したい場合 |
| `greedy` | 単独の軽量な割当を使いたい場合 |
| `greedy_unique_endpoints` | 同じ作業地点への集中を和らげる割当を試したい場合。端点の排他を必ず守る方式ではない |
| `min_cost_flow` | その時点の割当をまとめて最適化したい場合。大きい候補集合では計算費用が増える |

ユースケースごとの準備の違いは、U01〜U05がグラフ・始点・終点、U06〜U10が加えて基準経路、U09〜U12が必要に応じて外部予約、U13が変更後のグラフ、U14が共有deadline、U17〜U18がagent・task配列となる。

## 6. ユースケースごとの使い方

### 6.1 公開のsolver関数

以下は宣言の形を示すリファレンスであり、この宣言ブロックをヘッダと一緒に再宣言する必要はない。

```cpp
mapf_result solve_mapf(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_options& options = {});
mapf_result solve_mapf(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    mapf_workspace& workspace, const mapf_options& options = {});

mapf_result improve_mapf(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_result& initial_solution, const mapf_options& options = {});
mapf_result improve_mapf(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_result& initial_solution, mapf_workspace& workspace,
    const mapf_options& options = {});

mapf_result repair_mapf(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_result& base, const mapf_repair_request& request,
    const mapf_options& options = {});
mapf_result repair_mapf(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_result& base, const mapf_repair_request& request,
    mapf_workspace& workspace, const mapf_options& options = {});

mapd_result solve_mapd(const mapf_graph& graph,
    std::span<const mapd_agent> agents, std::span<const mapd_task> tasks,
    const mapd_options& options = {});
```

`graph` は移動規則、`starts`・`goals` は全agentの位置、`initial_solution`・`base` は呼出側が持つ経路、`request` は変更許可範囲である。workspaceを省略すると、その呼出し用の一時workspaceが内部で作られる。MAPDにはworkspace付きoverloadはない。

### 6.2 MAPF結果の全メンバ

| メンバ | 型 | 初期値 | 利用方法 |
| --- | --- | --- | --- |
| `success` | `bool` | `false` | まず確認する。falseなら経路を実行しない |
| `status` | `mapf_status` | `invalid_input` | 成功・失敗の分類。下表参照 |
| `paths` | `vector<vector<int>>` | 空 | `paths[i][0]` が始点、末尾が終点。短い経路の終端後はその場で待機 |
| `sum_of_costs` | `long long` | `0` | 経路の `size()-1` の総和 |
| `makespan` | `int` | `0` | 経路の `size()-1` の最大値 |
| `expanded_states` | `long long` | `0` | A*の展開数。PIBTだけなら0。総計算量やCPU命令数ではない |
| `attempts` | `int` | `0` | solveの構築候補数、repairの実行した異なる優先順数。LNS反復数ではない |
| `lns_improvements` | `int` | `0` | LNSで厳密な改善を採用した回数 |
| `strategy_used` | `mapf_strategy` | `prioritized` | solveでは最良初期解を作った方式。automaticという入力値そのものではない |

`improve_mapf` は基準解の `attempts` と `strategy_used` を維持し、展開数・LNS改善数は基準解の値へ加算する。repairの統計は通常その呼出しの探索を表し、`strategy_used` は通常prioritized、`lns_improvements` は0になる。選択集合が空で基準解を直接返す場合は、展開数と試行数を0にし、基準解の `strategy_used` と `lns_improvements` を継承する。

| `mapf_status` | 意味・対応 |
| --- | --- |
| `success` | 有効解がある。予算を使い切っていない、最適性が証明された、という意味ではない |
| `invalid_input` | 入力形状、番号、重複、基準経路、窓条件などが不正。入力を修正する |
| `unreachable` | 検査対象の始点と終点が静的グラフで連結でない。seedでは改善しない |
| `horizon_too_short` | 検査した距離下界や基準経路長に対して上限が不足。上限設定を見直す |
| `timeout` | 解を返せず、所定の時刻検査で時間切れと判断。残り予算を確認する |
| `search_failed` | 試した探索・固定条件で有効候補を返せなかった。不可解性の証明ではない |
| `memory_limit` | 実装の事前検査で時空間index上限などを超えた。一般の割当失敗を全て捕捉する値ではない |

`improve_mapf` は予算不足・不十分なhorizonなどで有効な基準解をそのまま返せる。repairも新条件に有効な基準解を選べる。この場合のstatusはsuccessであり、処理が全反復を完走したかは分からない。入力・horizonの前提違反で先に失敗する場合まで、基準解へのfallbackが保証されるわけではない。

### 6.3 MAPD結果の全メンバ

| メンバ | 型 | 初期値 | 利用方法 |
| --- | --- | --- | --- |
| `valid` | `bool` | `false` | 経路と報告された完了集計の整合。全件完了ではない |
| `completed_tasks` | `int` | `0` | 主目的の完了件数 |
| `sum_flow_time` | `long long` | `0` | 完了タスクの `completion-release_time` の総和 |
| `total_moves` | `long long` | `0` | 待機を除く実移動回数 |
| `paths` | `vector<vector<int>>` | 空 | validなsolver結果では各経路が `max_time+1` 頂点 |
| `task_agent` | `vector<int>` | 空 | 入力タスク順の担当番号。未割当は `-1`。割当済みでも未pickupの場合がある |
| `task_pickup_time` | `vector<int>` | 空 | 未pickupは `-1`、それ以外は集荷時刻 |
| `task_completion_time` | `vector<int>` | 空 | 未完了は `-1`、それ以外は配達完了時刻 |

MAPDにはMAPFのような詳細statusがない。`valid==false` だけで入力不正、期限切れ、その他の理由を特定できない。全件完了を要求する利用側では、validに加えて `completed_tasks == tasks.size()` 相当の確認が必要である。

### 6.4 検証関数

```cpp
bool validate_mapf_result(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_result& result);
bool validate_mapf_result(const mapf_graph& graph,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_result& result, const mapf_reservations& external_reservations);
bool validate_mapd_result(const mapf_graph& graph,
    std::span<const mapd_agent> agents, std::span<const mapd_task> tasks,
    const mapd_result& result);
```

MAPF検証は `result.success`、始点・終点、移動、頂点衝突、正面衝突を確認する。外部予約付きoverloadでは、その予約期間全体まで終点待機を補って確認する。`sum_of_costs`、`makespan`、`status`、探索統計の正しさは検査しない。自作の経路を取り込むときは集計値を別途計算する（U06）。

MAPD検証は経路、報告された完了タスクの担当・集荷・配達時刻、完了済みタスク間の搬送区間重複、完了集計、移動数を確認する。`result.valid` 自体を要求する検査ではない。未完了タスクの全メタデータや、呼出し時の `options.max_time` との一致までは確認しない。任意の外部データを完全に認証する境界として使わず、入力範囲・配列長も利用側で管理する。

### 6.5 U01〜U06：構築と反復評価

| 用途 | 呼出しと指定 | 戻り値の使い方・注意 |
| --- | --- | --- |
| U01 固定担当 | `solve_mapf(graph, starts, goals, options)` | successを確認し、指定objectiveの順で評価する |
| U02 グリッド | マスを頂点へ変換してU01を呼ぶ | `cell[v]` で座標へ戻す。壁に対応する頂点は作らない |
| U03 初期解 | `strategy=pibt`, `prioritized_restarts=1` でsolve | 成功保証はない。失敗時は別予算でautomatic等を検討 |
| U04 固定仕事量 | `time_limit_ms=-1`, `deadline=nullptr` でsolve | 同じseedと反復設定を使う。pibtやautomaticで各項目の意味が異なる点に注意 |
| U05 割当候補 | 同じworkspaceを渡して候補ごとにsolve | 成功候補だけを比較。総予算には共有deadlineを使う |
| U06 初期解改善 | `improve_mapf(graph, starts, goals, initial, workspace, options)` | 初期解は衝突なし必須。目的は改善呼出し時のoptionsに従う |

### 6.6 U07〜U11：repairと外部制約

| 用途 | 呼出しと指定 | 戻り値の使い方・注意 |
| --- | --- | --- |
| U07 担当先変更 | 全員分の新goals、対象agents、`begin=0`, `end<0` でrepair | 非選択のgoalは基準末尾と同じ。入力基準は外部予約を除いたグラフ上で有効であること |
| U08 集合指定改善 | U07と同じだがgoalsを変更しない | 基準解が有効なら目的値を悪化させない。同点でも経路が変わり得る |
| U09 時間窓 | `0<=begin<end<=L_0`、任意の外部予約でrepair | 全goalsは基準と一致必須。窓外位置は維持するが、選択経路末尾の冗長待機が削られる場合がある |
| U10 閉鎖 | `block_vertex`／`block_move`／`block_edge` で予約を作って全経路repair | 基準が新制約に違反する場合、失敗したら基準を実行しない |
| U11 外部経路 | `reserve_path` で予約。各始点だけの基準経路を作り、全agent repair | `solve_mapf` に外部予約引数はない。全agentを選ぶことで新goalへ計画できる |

全経路repairの `base.paths[i].back()` は、選択agentについて新しい `goals[i]` と一致する必要がない。そのためU11の1頂点だけの基準経路を使える。ただし基準経路の始点一致、合法な移動、agent間の衝突なしは必須で、`base.success=true` も必要。

時間窓の外部予約は窓開始からの相対時刻ではなく、基準経路の時刻原点で与える。最終検証は全経路を対象にするため、窓外や非選択経路に残る外部衝突も失敗原因となる。

### 6.7 U12〜U16：対話型の外側処理へ組み込む

| 用途 | 呼出しと指定 | 戻り値の使い方・注意 |
| --- | --- | --- |
| U12 観測後の再計画 | 現在からの基準経路へ切り直し、全agent repair | optionsのhorizonと外部予約の原点も現在に合わせる。観測不一致なら短い基準から構築 |
| U13 グラフ変更 | `workspace.clear()`、グラフ代入、solve | 返った経路は変更後の頂点番号系。旧番号との対応は外側で管理 |
| U14 共通予算 | 同じdeadlineでsolve、次にimprove | 最初に得た有効解を確保。改善段階に進めなくても利用できる |
| U15 再試行 | `status` を調べ、horizon不足／探索失敗だけ上限内で再試行 | 入力不正・到達不能・メモリ上限・時間切れは例では打切る |
| U16 操作列 | validatorの後、時刻ごとの全agentの移動へ展開 | `nullopt` と有効な空操作列を区別。各ターンの全移動を同期適用 |

### 6.8 U17〜U18：搬送計画と結果判定

U17は `solve_mapd(graph, agents, tasks, options)` を呼び、各タスクの担当と時刻、全agentの経路を得る。即時・将来解禁・空タスク集合は同じ関数で扱う。単独方式を調べるときは `assignment` を明示し、automaticでの候補比較と区別する。

U18は同じsolverの出力を次の順で分類する。

1. `task_completion_time[j] >= 0`：完了。
2. それ以外で `task_pickup_time[j] >= 0`：搬送中。
3. それ以外で `task_agent[j] >= 0`：割当済み、集荷前。
4. それ以外：未割当。

validを確認してから分類する。最終位置だけを次のMAPD呼出しへ渡しても、搬送中の荷物、担当の固定、待ち時間、乱数の内部状態は引き継がれない。

## 7. 制約・注意点

### 7.1 対応しないモデル

- 有向グラフ、重み付き移動、異なる移動速度、非同期行動。
- 頂点容量2以上、占有面積のあるロボット、向き・旋回時間、連続空間の安全距離。
- ゴール到達でエージェントが消えるMAPF、任意の順序付き経由地、agent別期限や重み。
- 任意の目的関数callback、ユーザー指定の優先順位列。
- MAPDの複数荷物同時搭載、作業時間、固定担当、荷物受渡し、タスクごとの報酬・締切。
- MAPDへの外部予約、途中状態の再開、公開の1ターンstep API。

問題を単純なグラフへ変換できても、元の問題の衝突や資源制約が自動的に保存されるとは限らない。モデル化の妥当性は別に確認する。

### 7.2 メソッドとパラメータの組合せ

| 呼出し | `strategy` | `prioritized_restarts` | LNS設定 | `congestion_weight` | 外部予約 |
| --- | --- | --- | --- | --- | --- |
| solve・prioritized | prioritized | 最低1回の構築試行数 | 使用 | 使用 | 不可 |
| solve・pibt | pibt | 1〜8の構築試行数 | 不使用 | 不使用 | 不可 |
| solve・automatic | automatic | 通常PP失敗時の追加構築だけ | 使用 | ユーザー値は不使用 | 不可 |
| `improve_mapf` | 不使用 | 不使用 | 使用 | 使用 | 不可 |
| 全経路repair | 不使用 | 異なる優先順の試行上限 | 不使用 | 使用 | 可 |
| 時間窓repair | 不使用 | 異なる優先順の試行上限 | 不使用 | 使用 | 可 |

repairで `strategy=pibt` を指定してもPIBTにはならない。LNSが不要なrepairで `lns_iterations` を増やしても探索は増えない。時間窓で `options.max_time` を増やしても窓は広がらない。`improve_mapf` に `strategy=pibt` を指定してもLNSは無効にならない。

外部制約を満たした解を、予約を扱えない `improve_mapf` へそのまま渡してはいけない。改善後に外部制約を破る可能性がある。外部制約付きでの局所改善は、同じgoalsと予約を渡すrepairを使う。

MAPDの `portfolio_runs` はautomaticのみで使う。MAPDにはMAPFの `objective`、LNS、restart設定はない。

### 7.3 repairで失敗しやすい条件

- 基準経路そのものに頂点衝突、正面衝突、非隣接移動、始点不一致がある。
- 変えたいgoalのagentを選択集合へ入れていない。
- 時間窓の両端位置、窓外の予定、固定経路が新しい障害と衝突する。
- 全経路repairで `begin_time != 0`、時間窓で `end_time <= begin_time`。
- 指定した全経路horizonが基準経路の長さより短い。再計画で短縮できそうでも、入口の条件違反になる。
- 選択集合が小さすぎて、必要な譲り合いを許していない。

選択集合・窓の拡張、または全員の再計画は外側が判断する。外部制約付きの失敗後に単純な `solve_mapf` へfallbackすると制約が消えるため、制約を保った全agent repairなどを使う。

### 7.4 MAPDの端の時刻と結果

- 新規割当を行う時刻は `0`〜`max_time-1`。最終時刻では、既に割当済みのタスクの到着だけ処理する。
- したがって `release_time == max_time` のタスクは新規割当されない。`max_time==0` では初期位置だけを返し、新規タスクは処理しない。
- pickupとdeliveryが同じなら、その場所で割当された時刻に完了できる。ただし同じagentへその時刻に無制限に次のタスクを再割当するわけではない。
- 未到達・期限内に間に合わないタスクがあっても、MAPD全体のvalidがfalseになるとは限らない。未完了として残り得る。
- 割当時の距離下界で間に合いそうでも、衝突待ちのため未完了になることがある。
- automaticは計算予算0や既に過ぎたdeadlineでは、候補を1つも開始せずvalid=falseを返し得る。単独方式は期限切れでも待機で埋めた有効経路を返すことがあり、両者の結果を同一視しない。
- `greedy_unique_endpoints` は端点の重複を絶対禁止するものではない。重複を避ける割当の後に残りも割り当てる。

### 7.5 時間制限はソフト制限

距離の前計算、予約表の初期化、候補作成、最小費用流、出力構築、検証など、内部で時計を頻繁に見ない処理がある。A*も全状態ごとに時計を確認するわけではない。期限を過ぎてすぐ返る保証はないため、外側の制限に余裕を残す。

MAPD automaticは残り時間を次の候補に渡すが、候補ごとの均等配分ではない。1候補が残り時間の大部分を使うと、指定した `portfolio_runs` 件を全て実行できない場合がある。

### 7.6 メモリと整数の範囲

時空間状態数を $W=N(H+1)$、cache済み終点数を $U$ とする。典型的な32 bit `int` の環境では、A*の訪問印・二次費用・親と内部予約の主配列だけで概ね $16W$ byte、距離表で約 $4UN$ byteを使う。heap・出力・補助配列・容量余裕は別に必要。外部占有表はそのhorizonについてさらに約 $4N(H_{\mathrm{ext}}+1)$ byteであり、$H_{\mathrm{ext}}$ は外部予約の最終時刻である。

- `solve_mapf` と非空集合の全経路repairは、時空間状態数が `INT_MAX` を超える場合に `memory_limit` を返す。
- `improve_mapf` は同条件で有効な基準解を返す。
- 時間窓repairには同じ事前検査がない。呼出側で `N*(end-begin+1) <= INT_MAX` を保証する。U09の例はこれを検査する。
- PIBTを明示したsolveも共通の時空間index上限検査を受ける。PIBTだから無制限に大きいhorizonを使えるわけではない。
- これらは実メモリの上限保証ではない。`std::bad_alloc` などは一律に捕捉してstatusへ変換されない。
- グラフ・経路・agent・taskの個数や、距離、時刻、費用の中間演算が `int` の表現範囲に収まるサイズで使用する。極端な重みやhorizon、NaN・無限大の時間予算は使わない。

### 7.7 寿命・並行利用・再現性

`std::span` とポインタは所有権を持たない。呼出し中に元配列、deadline、外部予約を破棄・変更しない。1個のworkspaceを複数スレッドで同時使用しない。外部予約のconst照会でも禁止移動の遅延整列で内部変更が起こり得るため、共有したまま同時照会する設計は避け、スレッドごとに分離する。

固定seedだけでは、時間制限による探索打切り位置まで固定できない。再現比較では `time_limit_ms<0` と `deadline=nullptr` を併用し、同じビルド・実行環境で比べる。異なるコンパイラや標準ライブラリまでビット単位の同一性を保証するAPIではない。

### 7.8 経路長・検証・実行の落とし穴

自作MAPF経路の末尾に待機を余分に並べると、その分もSOCとmakespanに数えられる。validatorはこの集計値を検査しない。経路から集計し、不要な末尾待機を削る場合も、終端後の占有が続く意味を変えないこと。

失敗時の `paths` や統計には、一部探索の途中結果が含まれる場合がある。success／validを見ずに利用しない。逆に全員が始点で到着済みなら、MAPFの有効な経路は各1頂点であり、操作列は空で正しい。

## 8. ユースケースごとのコード例

各節のC++ブロックは独立した関数例であり、他節の関数や共有変数に依存しない。必要な節をヘッダとともにコピーして使える。全例を1つにまとめても名前は衝突しない。独自の `main()` から呼び出すこと。

関数例集 `guide_examples.cpp` は、この章の全C++ブロックをまとめたもの。付属の検証コードは全例の個別・結合コンパイル、正常・異常・境界入力での実行、ランダム入力、ASan/UBSanを確認するために使える。

引数の `options={}` は第5章のライブラリ既定値を使う意味である。関数固有の既定値・上書きは各例のコメントに示す。特に `U04` は時間制限を外すための比較用、`U10`・`U11` は明示したhorizonを使う。結果のエラー表現は例ごとにコメントで区別する。

### 8.1 U01：固定された担当先へ全員を移動させる

`objective` だけを切り替えれば、合計時間優先と全員完了時刻優先の両方に使える。

<!-- EXAMPLE U01 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: graph は無向・重みなし。starts[i], goals[i] は agent i の頂点番号。
// starts 内と goals 内はそれぞれ重複不可。agent 数は 1 以上。
// options.objective に sum_of_costs / makespan を指定する。
// 出力: success のときだけ paths を利用する。失敗理由は status。
mapf_result solve_fixed_goals(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, const mapf_options& options = {}) {
    return solve_mapf(graph, starts, goals, options);
}
```

<!-- END EXAMPLE U01 -->

### 8.2 U02：障害物付きグリッド

`input_valid` は盤面・座標の形状検査、`result.success` はMAPF解の有無を表す。始点・終点の重複などはMAPF側の結果に現れる。

<!-- EXAMPLE U02 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

struct grid_plan {
    bool input_valid = false;  // 座標入力とグリッド形状が妥当か。可解性とは別。
    mapf_result result;
    std::vector<std::vector<int>> id;  // id[y][x]: 壁なら -1。
    std::vector<std::pair<int, int>> cell;  // cell[v] = {y, x}。
};

// 入力: '#' は壁、他の文字は通行可能。上下左右へ 1 ターンで移動する。
// starts/goals の要素は {行 y, 列 x}、0 始まり。壁は指定不可。
// 出力: result.paths[i][t] を cell に通すと時刻 t の座標になる。
grid_plan solve_grid(
    const std::vector<std::string>& board,
    std::span<const std::pair<int, int>> starts,
    std::span<const std::pair<int, int>> goals,
    const mapf_options& options = {}) {
    grid_plan out;
    if (board.empty() || board.front().empty() || starts.empty() ||
        starts.size() != goals.size() ||
        board.size() > std::size_t(INT_MAX) ||
        board.front().size() > std::size_t(INT_MAX)) return out;
    const int height = int(board.size()), width = int(board.front().size());
    if (std::size_t(height) * std::size_t(width) > std::size_t(INT_MAX)) return out;
    for (const auto& row : board) {
        if (row.size() != std::size_t(width)) return out;
    }
    out.id.assign(height, std::vector<int>(width, -1));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (board[y][x] == '#') continue;
            out.id[y][x] = int(out.cell.size());
            out.cell.emplace_back(y, x);
        }
    }
    auto vertex = [&](std::pair<int, int> p) {
        const auto [y, x] = p;
        return y < 0 || y >= height || x < 0 || x >= width ? -1 : out.id[y][x];
    };
    std::vector<int> ss, gg;
    for (std::size_t i = 0; i < starts.size(); ++i) {
        const int s = vertex(starts[i]), g = vertex(goals[i]);
        if (s < 0 || g < 0) return out;
        ss.push_back(s); gg.push_back(g);
    }
    std::vector<std::pair<int, int>> edges;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (out.id[y][x] < 0) continue;
            if (x + 1 < width && out.id[y][x + 1] >= 0)
                edges.emplace_back(out.id[y][x], out.id[y][x + 1]);
            if (y + 1 < height && out.id[y + 1][x] >= 0)
                edges.emplace_back(out.id[y][x], out.id[y + 1][x]);
        }
    }
    const mapf_graph graph(int(out.cell.size()), edges);
    out.input_valid = true;
    out.result = solve_mapf(graph, ss, gg, options);
    return out;
}
```

<!-- END EXAMPLE U02 -->

### 8.3 U03：短時間の初期解

初期構築の方式を明示する。応答時間や成功率の保証ではない。

<!-- EXAMPLE U03 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: budget_ms は計算時間の目安。移動ターン上限ではない。
// 出力: 短い構築探索で得た解。success == false も通常の結果として扱う。
mapf_result solve_quick_initial(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, double budget_ms = 5.0,
    std::uint64_t seed = 1) {
    if (!std::isfinite(budget_ms) || budget_ms < 0.0) return {};
    mapf_options options;
    options.strategy = mapf_strategy::pibt;
    options.prioritized_restarts = 1;
    options.lns_iterations = 0;
    options.time_limit_ms = budget_ms;
    options.seed = seed;
    return solve_mapf(graph, starts, goals, options);
}
```

<!-- END EXAMPLE U03 -->

### 8.4 U04：固定反復の比較

`options` を値で受け取るので、呼出側の設定オブジェクトは書き換えない。

<!-- EXAMPLE U04 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: options で strategy, objective, max_time, seed と反復数を指定。
// この関数では時間制限を意図的に全て外す。対話処理の本番制限には使わない。
// 出力: 同じ実行環境・入力・設定で再比較するための MAPF 結果。
mapf_result solve_fixed_work(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, mapf_options options = {}) {
    options.time_limit_ms = -1.0;
    options.deadline = nullptr;
    return solve_mapf(graph, starts, goals, options);
}
```

<!-- END EXAMPLE U04 -->

### 8.5 U05：終点割当候補の評価

候補別の最短距離の違いだけでなく、衝突回避後の目的値を比較する。同点では先に列挙した候補を残す。個々の失敗理由も必要なら、ループ内で `candidate.status` を別途記録する。

<!-- EXAMPLE U05 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

struct assignment_choice {
    int index = -1;  // 成功候補なし。そうでなければ goal_candidates の添字。
    mapf_result result;
};

// 入力: goal_candidates[c][i] は候補 c における agent i の goal。
// workspace は同じ graph オブジェクトで再利用する。
// options.time_limit_ms は候補ごとの予算。合計時間は options.deadline で制限。
// 出力: 成功した候補の中で、指定 objective の辞書順が最良の候補。
assignment_choice choose_goal_assignment(
    const mapf_graph& graph, std::span<const int> starts,
    const std::vector<std::vector<int>>& goal_candidates,
    mapf_workspace& workspace, const mapf_options& options = {}) {
    assignment_choice best;
    if (goal_candidates.size() > std::size_t(INT_MAX)) return best;
    auto key = [&](const mapf_result& r) {
        return options.objective == mapf_objective::sum_of_costs
            ? std::pair<long long, long long>{r.sum_of_costs, r.makespan}
            : std::pair<long long, long long>{r.makespan, r.sum_of_costs};
    };
    for (int c = 0; c < int(goal_candidates.size()); ++c) {
        if (options.deadline != nullptr && options.deadline->expired()) break;
        auto candidate = solve_mapf(graph, starts, goal_candidates[c], workspace, options);
        if (!candidate.success) continue;
        if (best.index == -1 || key(candidate) < key(best.result)) {
            best.index = c;
            best.result = std::move(candidate);
        }
    }
    return best;
}
```

<!-- END EXAMPLE U05 -->

### 8.6 U06：自作の初期解を検証して改善

集計値が未設定の自作経路を安全に取り込む。衝突した経路は改善の前に拒否する。

<!-- EXAMPLE U06 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: initial_paths[i][0] == starts[i]、末尾 == goals[i] の衝突なし経路。
// 自作の初期解も渡せる。経路長・評価値はこの関数で計算する。
// 出力: 入力不正なら失敗。有効なら指定 objective の辞書順で元解より悪化しない解。
mapf_result refine_existing_plan(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals,
    const std::vector<std::vector<int>>& initial_paths,
    mapf_workspace& workspace, const mapf_options& options = {}) {
    if (starts.empty() || starts.size() != goals.size() ||
        initial_paths.size() != starts.size()) return {};
    mapf_result initial;
    initial.success = true;
    initial.status = mapf_status::success;
    initial.paths = initial_paths;
    for (const auto& path : initial.paths) {
        if (path.empty() || path.size() > std::size_t(INT_MAX)) return {};
        const int length = int(path.size()) - 1;
        initial.sum_of_costs += length;
        initial.makespan = std::max(initial.makespan, length);
    }
    if (!validate_mapf_result(graph, starts, goals, initial)) return {};
    return improve_mapf(graph, starts, goals, initial, workspace, options);
}
```

<!-- END EXAMPLE U06 -->

### 8.7 U07：一部agentの担当先変更

変更するのは `selected` に含めたagentだけ。`new_goals` は選択agent分だけでなく全員分を渡す。

<!-- EXAMPLE U07 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: new_goals は全 agent 分。変更した agent を selected に全て含める。
// base は同じ starts から始まる有効経路。非選択 agent の経路を固定する。
// 出力: 成功時、非選択 agent の経路配列は base と同一。
mapf_result repair_changed_goals(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> new_goals, const mapf_result& base,
    std::span<const int> selected, mapf_workspace& workspace,
    const mapf_options& options = {}) {
    mapf_repair_request request;
    request.agents = selected;
    return repair_mapf(graph, starts, new_goals, base, request, workspace, options);
}
```

<!-- END EXAMPLE U07 -->

### 8.8 U08：自分で選んだ集合の局所改善

基準解に対する目的値の比較はrepair内で行う。新たなLNSを自動で追加する例ではない。

<!-- EXAMPLE U08 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: goals は base と同じ。selected は改善を試す agent 番号の重複なし集合。
// 出力: 他 agent を固定して再最適化した解。元解が有効なら目的値を悪化させない。
// 同点経路へ置換されることはある。strict な改善は呼出側でも評価値を比較する。
mapf_result reoptimize_agents(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, const mapf_result& base,
    std::span<const int> selected, mapf_workspace& workspace,
    const mapf_options& options = {}) {
    mapf_repair_request request;
    request.agents = selected;
    return repair_mapf(graph, starts, goals, base, request, workspace, options);
}
```

<!-- END EXAMPLE U08 -->

### 8.9 U09：時間窓だけの修復

窓サイズのindex上限をラッパーで検査する。実メモリが十分かは別の問題である。

<!-- EXAMPLE U09 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: 0 <= begin < end <= 基準経路の最大長-1。窓の両端位置は固定。
// external の時刻は base.paths の時刻0と同じ原点。不要なら nullptr。
// 出力: 成功時、窓外の位置と非選択 agent の経路を保った全経路。
mapf_result repair_time_window(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, const mapf_result& base,
    std::span<const int> selected, int begin, int end,
    const mapf_reservations* external, mapf_workspace& workspace,
    const mapf_options& options = {}) {
    if (graph.size() <= 0 || begin < 0 || end <= begin) return {};
    const auto states = (std::uint64_t(end) - std::uint64_t(begin) + 1) *
                        std::uint64_t(graph.size());
    if (states > std::uint64_t(INT_MAX)) {
        mapf_result failure;
        failure.status = mapf_status::memory_limit;
        return failure;
    }
    mapf_repair_request request;
    request.agents = selected;
    request.begin_time = begin;
    request.end_time = end;
    request.external_reservations = external;
    return repair_mapf(graph, starts, goals, base, request, workspace, options);
}
```

<!-- END EXAMPLE U09 -->

### 8.10 U10：時刻付きの頂点・移動・両方向閉鎖

閉鎖イベントは全て有限の時刻を明示する。期間閉鎖なら対象時刻分のイベントを作る。ラッパーが拒否した入力は初期状態の失敗結果（invalid_input）を返す。

<!-- EXAMPLE U10 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

struct closed_vertex { int time; int vertex; };
struct closed_move {
    int time; int from; int to;
    bool both_directions = false;  // true なら同時刻の逆方向も閉鎖。
};

// 入力: horizon は予約の最終時刻かつ探索上限。基準経路の最大長-1 以上。
// vertices は時刻 time の占有禁止。moves は time -> time+1 の移動禁止。
// selected 以外の経路は固定。閉鎖の影響を受ける agent は selected に含める。
// 出力: 全閉鎖を満たす修復解。失敗時に base を無条件で使用してはいけない。
mapf_result repair_road_closures(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, const mapf_result& base,
    std::span<const int> selected, int horizon,
    std::span<const closed_vertex> vertices, std::span<const closed_move> moves,
    mapf_workspace& workspace, mapf_options options = {}) {
    if (graph.size() <= 0 || horizon < 0 ||
        (std::uint64_t(horizon) + 1) * std::uint64_t(graph.size()) >
            std::uint64_t(INT_MAX)) return {};
    mapf_reservations external(graph.size(), horizon);
    for (const auto& event : vertices) {
        if (!external.block_vertex(event.time, event.vertex)) return {};
    }
    for (const auto& event : moves) {
        const bool ok = event.both_directions
            ? external.block_edge(event.time, event.from, event.to)
            : external.block_move(event.time, event.from, event.to);
        if (!ok) return {};
    }
    mapf_repair_request request;
    request.agents = selected;
    request.external_reservations = &external;
    options.max_time = horizon;
    return repair_mapf(graph, starts, goals, base, request, workspace, options);
}
```

<!-- END EXAMPLE U10 -->

### 8.11 U11：外部ロボットを避ける新規計画

外部経路が終わった後も残るなら、例えば `hold_until=horizon` を指定する。外部経路同士の不整合や、自グループの始点との時刻0の衝突は失敗になる。

<!-- EXAMPLE U11 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

struct external_route {
    int start_time = 0;
    std::vector<int> vertices;  // vertices[0] が start_time の位置。
    int hold_until = -1;  // -1: 末尾まで。それ以降も占有するなら最終時刻を明示。
};

// 入力: routes は動かせない外部ロボットの合法経路。時刻原点は starts と共通。
// horizon は全予約を含む非負の計画上限。各 external_route の経路末尾も範囲内。
// 出力: 外部予約を避けた全 agent の計画。初期解を別途作る必要はない。
mapf_result solve_around_external_routes(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, std::span<const external_route> routes,
    int horizon, mapf_workspace& workspace, mapf_options options = {}) {
    if (graph.size() <= 0 || starts.empty() || starts.size() != goals.size() ||
        starts.size() > std::size_t(INT_MAX) || horizon < 0 ||
        (std::uint64_t(horizon) + 1) * std::uint64_t(graph.size()) >
            std::uint64_t(INT_MAX)) return {};
    mapf_reservations external(graph.size(), horizon);
    for (const auto& route : routes) {
        if (route.start_time < 0 || route.start_time > horizon || route.vertices.empty() ||
            route.vertices.size() > std::size_t(horizon - route.start_time) + 1) return {};
        for (std::size_t t = 0; t < route.vertices.size(); ++t) {
            const int v = route.vertices[t];
            if (v < 0 || v >= graph.size()) return {};
            if (t > 0 && route.vertices[t - 1] != v) {
                const auto next = graph.neighbors(route.vertices[t - 1]);
                if (!std::binary_search(next.begin(), next.end(), v)) return {};
            }
        }
        if (!external.reserve_path(route.start_time, route.vertices, route.hold_until)) return {};
    }
    // repair の基準は新 goal に到達済みでなくてよい。全 agent を再計画対象にする。
    mapf_result base;
    base.success = true;
    base.status = mapf_status::success;
    for (const int s : starts) base.paths.push_back({s});
    std::vector<int> selected(starts.size());
    std::iota(selected.begin(), selected.end(), 0);
    mapf_repair_request request;
    request.agents = selected;
    request.external_reservations = &external;
    options.max_time = horizon;
    return repair_mapf(graph, starts, goals, base, request, workspace, options);
}
```

<!-- END EXAMPLE U11 -->

### 8.12 U12：観測した現在位置からの再計画

入力の `previous` は書き換えない。返却経路の時刻0は「今」である。基準経路を再利用する場合、明示horizonは残り基準経路の最大長以上にする。新規構築へ切り替わったかの記録が必要なら、`can_reuse` を返却情報に追加できる。

<!-- EXAMPLE U12 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: elapsed は previous の時刻原点から経過したターン数。
// observed_starts は「今」観測した全 agent の位置。new_goals は今から向かう goal。
// external と options.max_time は「今 = 0」に時刻を直して渡す。
// 出力: 今からの経路。実行済み接頭辞は含まない。
// 予測と観測が一致し、有効な残り経路があれば基準に使う。違えば今から作り直す。
mapf_result replan_from_observation(
    const mapf_graph& graph, const mapf_result& previous, int elapsed,
    std::span<const int> observed_starts, std::span<const int> new_goals,
    const mapf_reservations* external, mapf_workspace& workspace,
    const mapf_options& options = {}) {
    if (elapsed < 0 || observed_starts.empty() ||
        observed_starts.size() != new_goals.size() ||
        observed_starts.size() > std::size_t(INT_MAX)) return {};
    mapf_result base;
    base.success = true;
    base.status = mapf_status::success;
    bool can_reuse = previous.success && previous.paths.size() == observed_starts.size();
    std::vector<int> old_goals;
    if (can_reuse) {
        for (std::size_t a = 0; a < observed_starts.size(); ++a) {
            const auto& path = previous.paths[a];
            if (path.empty() || path.size() > std::size_t(INT_MAX)) { can_reuse = false; break; }
            const int cut = std::min(elapsed, int(path.size()) - 1);
            if (path[cut] != observed_starts[a]) { can_reuse = false; break; }
            base.paths.emplace_back(path.begin() + cut, path.end());
            old_goals.push_back(path.back());
        }
        if (can_reuse && !validate_mapf_result(graph, observed_starts, old_goals, base))
            can_reuse = false;
    }
    if (!can_reuse) {
        base.paths.clear();
        for (const int s : observed_starts) base.paths.push_back({s});
    }
    // 基準の集計値は repair が経路から再計算する。
    std::vector<int> selected(observed_starts.size());
    std::iota(selected.begin(), selected.end(), 0);
    mapf_repair_request request;
    request.agents = selected;
    request.external_reservations = external;
    return repair_mapf(graph, observed_starts, new_goals, base, request, workspace, options);
}
```

<!-- END EXAMPLE U12 -->

### 8.13 U13：変更後のグラフで再構築

辺リストの範囲検査に失敗した場合、graphとworkspaceは変更しない。グラフ構築後にMAPFが失敗しても、graph自体は変更後の状態である。

<!-- EXAMPLE U13 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: n と edges は変更後のグラフ全体。starts は現在位置、goals は新頂点番号系。
// graph と workspace は呼出側が所有する。変更中に別スレッドから使わない。
// 出力: 再構築した graph 上の新しい計画。旧経路はこの関数では再使用しない。
mapf_result solve_after_graph_change(
    mapf_graph& graph, mapf_workspace& workspace, int n,
    const std::vector<std::pair<int, int>>& edges,
    std::span<const int> starts, std::span<const int> goals,
    const mapf_options& options = {}) {
    if (n <= 0) return {};
    for (const auto& [u, v] : edges) {
        if (u < 0 || v < 0 || u >= n || v >= n) return {};
    }
    mapf_graph rebuilt(n, edges);
    workspace.clear();
    graph = std::move(rebuilt);
    return solve_mapf(graph, starts, goals, workspace, options);
}
```

<!-- END EXAMPLE U13 -->

### 8.14 U14：共通deadlineで構築と改善

呼出側で `mapf_deadline::after_ms(予算)` を1回作って渡す。各段階で作り直さない。ここではLNSを独立した改善段階として呼ぶため、入力strategyがpibtでも後段の改善は実行可能。

<!-- EXAMPLE U14 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: deadline は外側 solver が作った共有終了時刻で、呼出し中は生存させる。
// options は目的・horizon・反復数等。本関数は相対時間制限を外し、deadline を共有。
// 出力: 初期解を作り、残り予算で改善した結果。有効解があれば期限到達後も返せる。
mapf_result solve_with_shared_deadline(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, mapf_workspace& workspace,
    const mapf_deadline& deadline, mapf_options options = {}) {
    options.deadline = &deadline;
    options.time_limit_ms = -1.0;
    mapf_options construction = options;
    construction.lns_iterations = 0;
    auto initial = solve_mapf(graph, starts, goals, workspace, construction);
    if (!initial.success || deadline.expired()) return initial;
    return improve_mapf(graph, starts, goals, initial, workspace, options);
}
```

<!-- END EXAMPLE U14 -->

### 8.15 U15：上限付きの再試行

例の再試行方針は新規MAPF専用。外部予約付きrepairには、外部制約を保った別のfallback方針が必要である。

<!-- EXAMPLE U15 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: first_horizon から horizon_cap まで必要に応じて拡張。tries は呼出し回数上限。
// per_call_ms は各回のソフト時間制限。負値なら反復数で停止。全体制限は deadline。
// 出力: 最初の成功解、または最後の失敗。status が timeout なら再試行しない。
mapf_result solve_with_retries(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, int first_horizon, int horizon_cap, int tries,
    double per_call_ms, std::uint64_t seed = 1,
    const mapf_deadline* deadline = nullptr) {
    if (first_horizon < 0 || horizon_cap < first_horizon || tries <= 0 ||
        !std::isfinite(per_call_ms)) return {};
    mapf_workspace workspace;
    mapf_result last;
    int horizon = first_horizon;
    for (int trial = 0; trial < tries; ++trial) {
        mapf_options options;
        options.max_time = horizon;
        options.time_limit_ms = per_call_ms;
        options.deadline = deadline;
        options.seed = seed + std::uint64_t(trial);
        last = solve_mapf(graph, starts, goals, workspace, options);
        if (last.success) return last;
        if (last.status != mapf_status::horizon_too_short &&
            last.status != mapf_status::search_failed) return last;
        horizon = int(std::min<long long>(horizon_cap, std::max(1LL, 2LL * horizon)));
    }
    return last;
}
```

<!-- END EXAMPLE U15 -->

### 8.16 U16：同期した操作列への変換

返却値に値があるかを先に確認する。外側で指定した操作horizonまで待機を出力したい場合は、この結果の末尾へ全員待機を追加する。

<!-- EXAMPLE U16 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: 外部制約を使って作った解には external を必ず渡す。
// 出力: moves[t][i] = {時刻tの頂点, 時刻t+1の頂点}。同じ頂点なら待機。
// nullopt は不正な解。正しいが全員到着済みの場合は「値のある空配列」。
// 各 t の全 agent の移動を同期して適用する。逐次適用ではない。
std::optional<std::vector<std::vector<std::pair<int, int>>>> synchronized_moves(
    const mapf_graph& graph, std::span<const int> starts,
    std::span<const int> goals, const mapf_result& result,
    const mapf_reservations* external = nullptr) {
    if (starts.empty()) return std::nullopt;
    for (const auto& path : result.paths) {
        if (path.empty() || path.size() > std::size_t(INT_MAX)) return std::nullopt;
    }
    const bool valid = external == nullptr
        ? validate_mapf_result(graph, starts, goals, result)
        : validate_mapf_result(graph, starts, goals, result, *external);
    if (!valid) return std::nullopt;
    int horizon = 0;
    for (const auto& path : result.paths) {
        horizon = std::max(horizon, int(path.size()) - 1);
    }
    std::vector<std::vector<std::pair<int, int>>> moves(
        horizon, std::vector<std::pair<int, int>>(starts.size()));
    for (int t = 0; t < horizon; ++t) {
        for (std::size_t a = 0; a < starts.size(); ++a) {
            const auto& path = result.paths[a];
            const int last = int(path.size()) - 1;
            moves[t][a] = {path[std::min(t, last)], path[std::min(t + 1, last)]};
        }
    }
    return moves;
}
```

<!-- END EXAMPLE U16 -->

### 8.17 U17：即時・将来解禁タスクの搬送

解禁時刻はtask配列で指定する。タスクは入力順で識別し、solver内部の処理順と混同しない。

<!-- EXAMPLE U17 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

// 入力: agent の start は互いに異なる。tasks は全て事前登録する。
// tasks[j] = {pickup, delivery, release_time}。即時タスクは release_time = 0。
// horizon は操作ターン数。search_ms は計算時間(ms)。両者は別物。
// assignment は automatic / greedy / greedy_unique_endpoints / min_cost_flow。
// 出力: valid のとき全経路の長さは horizon+1。全タスク完了とは限らない。
mapd_result solve_delivery(
    const mapf_graph& graph, std::span<const mapd_agent> agents,
    std::span<const mapd_task> tasks, int horizon,
    mapd_assignment_strategy assignment = mapd_assignment_strategy::automatic,
    double search_ms = 1000.0, std::uint64_t seed = 1,
    int portfolio_runs = 8, const mapf_deadline* deadline = nullptr) {
    if (horizon < 0 || !std::isfinite(search_ms)) return {};
    mapd_options options;
    options.max_time = horizon;
    options.assignment = assignment;
    options.time_limit_ms = search_ms;
    options.seed = seed;
    options.portfolio_runs = portfolio_runs;
    options.deadline = deadline;
    return solve_mapd(graph, agents, tasks, options);
}
```

<!-- END EXAMPLE U17 -->

### 8.18 U18：完了と未完了状態の整理

`all_completed` はplan.validがtrueの場合だけ解釈する。有効な空タスク集合なら全件完了である。

<!-- EXAMPLE U18 -->

```cpp
#include "multi_agent_path_finding_v04.hpp"

struct delivery_summary {
    mapd_result plan;
    bool all_completed = false;
    std::vector<int> completed;  // 配送完了の task 番号。
    std::vector<int> unassigned;  // 未割当。
    std::vector<int> awaiting_pickup;  // 担当は決まったが未pickup。
    std::vector<int> carrying;  // pickup済み、delivery未完了。
};

// 入力: options.max_time に実際の打切りターンを指定。
// 出力: plan.valid のときだけ分類を使う。タスク番号は元の tasks の添字を維持。
// 未完了群をそのまま次の solve_mapd に渡しても「途中状態の再開」にはならない。
delivery_summary plan_until_horizon(
    const mapf_graph& graph, std::span<const mapd_agent> agents,
    std::span<const mapd_task> tasks, const mapd_options& options = {}) {
    delivery_summary out;
    if (tasks.size() > std::size_t(INT_MAX)) return out;
    out.plan = solve_mapd(graph, agents, tasks, options);
    if (!out.plan.valid) return out;
    for (int j = 0; j < int(tasks.size()); ++j) {
        if (out.plan.task_completion_time[j] >= 0) out.completed.push_back(j);
        else if (out.plan.task_pickup_time[j] >= 0) out.carrying.push_back(j);
        else if (out.plan.task_agent[j] >= 0) out.awaiting_pickup.push_back(j);
        else out.unassigned.push_back(j);
    }
    out.all_completed = out.completed.size() == tasks.size();
    return out;
}
```

<!-- END EXAMPLE U18 -->

## 9. 実装

### 9.1 全体の流れ

公開APIの役割は、初期構築、既存解の改善、変更範囲を限定した修復、搬送シミュレーションの4つに分かれる。

| API | 実装上の流れ |
| --- | --- |
| `solve_mapf` | 入力検査 → 終点距離の準備 → horizon・資源検査 → 指定方式の初期構築 → 有効な場合のLNS → 全経路検証 |
| `improve_mapf` | 基準解検証・費用再計算 → 距離とhorizonの準備 → LNS → 候補検証。改善できなければ基準解 |
| `repair_mapf` | 基準経路・対象検査 → 固定経路と外部制約の予約 → 選択agentの再計画 → 必要なら窓の接合 → 全体検証と基準解比較 |
| `solve_mapd` | 入力検査 → 作業地点の距離準備 → 時刻ごとの到着処理・割当・同期移動 → 結果検証。automaticは複数候補を比較 |

内部の距離表、予約表、A*作業領域、PIBT作業領域、共有補助処理は `mapf_workspace` のprivate定義に置かれている。利用者は内部型ではなく、公開のグラフ・options・結果・予約・workspace・自由関数を使う。

### 9.2 グラフと距離表

グラフはCSRで保持する。各頂点の隣接列を昇順・重複なしに整え、移動検証では二分探索を使う。

距離は終点を始点とするBFSで全頂点分を計算する。無向グラフなので、終点からの距離は各頂点から終点への距離と等しい。未使用の終点は計算せず、workspaceのtarget集合へ追加されたときに計算する。全経路repairは選択agentの新goal、時間窓repairは選択agentの窓終端頂点だけを新規targetとして要求する。

新しいtarget数を $U_{\mathrm{new}}$ とするとBFSは $O(U_{\mathrm{new}}(N+M))$。距離の連続配列を拡張するときは、再確保による既存行の移動で追加の $O(UN)$ が発生し得る。$U$ は追加後のcache済みtarget数。

### 9.3 時空間予約と1体のA*

状態は「頂点・時刻」。予約表は各時刻・頂点の占有者を持つ。移動候補は、移動先の次時刻の占有、逆向き移動の相手、明示的な禁止移動を調べる。既に決めた経路は終端後もhorizonまで終点を予約する。

A*の第一キーは、現在時刻に静的最短距離を加えた到着下界と、終点をその後保持できる最早時刻の大きい方。終点に将来予約がある場合、そこへ早着しただけで探索を終了しない。外部の「終点での待機禁止」も保持可能時刻の下界に含める。

第二キーは、待機回数と混雑重み付き訪問数の累積。第一キーを変えず、同等の到着下界の候補の探索順を調整する。さらに同点なら深い時刻の候補を優先する。第一・第二キーを符号順を保った64 bit整数にまとめ、二分heapで管理する。

訪問印を世代番号で管理し、各探索で全配列を初期化する処理を避ける。親頂点と第二費用は訪問した状態にだけ書き込み、現在世代でない値は読まない。世代上限では訪問印を初期化する。展開状態数4096ごとに期限を確認する。

固定予約下の1体探索と、エージェント順序も含む全体MAPFの最適性は別である。1体の経路が良くても、後から計画するエージェントを塞ぐことがある。

### 9.4 優先順付き経路計画（prioritized planning）

全員の始点を時刻0へ予約し、順序に従って1体ずつA*で経路を作る。先に決めた経路は後続の制約となる。全員分ができた候補を目的関数で比較する。

初回は、他エージェントの独立最短路上に自分のgoalを置ける組の数を数え、通行を妨げやすいagentを後ろへ回す。同点では長距離を先にする。2回目は長距離順、同点ならgoal次数の小さい順。3回目はgoal次数の小さい順、同点なら長距離順。その後は乱択を使う。失敗agentがあれば、次の試行で先頭へ移して計画する。

これは順序探索のヒューリスティックであり、全順序・全経路の完全探索ではない。失敗agentを優先しても、解を見つけられる保証はない。

### 9.5 PIBT：各ターンの同時移動

PIBT（Priority Inheritance with Backtracking）は、そのターンの次位置を優先度順に決める。移動先に別agentがいる場合は、そのagentの移動を再帰的に先に決める。失敗した候補が仮に決めた移動はrollbackする。頂点の二重使用と正面衝突を禁止しつつ、押し出しを伴う同時移動を作る。

優先度はage、終点までの距離、乱数キーを使う。ageは現在の目標上で0に戻り、目標にいないターンで増える値で、静的MAPFでは独立最短距離を初期値とする。通常はageの大きい順、同点なら残距離の長い順、さらに同点なら乱数キー順に扱う。

移動候補は終点距離を優先し、直前位置への逆戻りに2、目標未到達での待機に1のペナルティを与え、最後を乱数で決める。候補のキーは全て生成するが、順序の確定は必要になった次候補から行う。

静的MAPFでは全員がgoalに揃うまで進め、成功後に各agentの最後の非goal時刻から経路末尾を決める。失敗した試行を部分解として実行することはない。狭路・高密度・袋小路を含む任意グラフで、到達や完全性を保証する実装ではない。

### 9.6 静的MAPFのautomatic

全入力に対して、次の構築と比較を行う。時間切れにより後続処理が十分に実行できない場合はある。

1. PIBTを1試行する。
2. 混雑重み0でprioritized planningを2試行する。
3. 2の構築が失敗し、期限前なら、混雑重み1で追加のprioritized planningを行う。追加試行数は `prioritized_restarts` に基づく。
4. 得られた有効候補を指定目的で比較し、最良候補をLNSで改善する。ここでの混雑重みは0。
5. 最終結果を検証する。

追加構築の条件は「PIBTを含む全候補が失敗」ではなく、「通常prioritized planningが失敗」である。ユーザーの `congestion_weight` をautomaticがそのまま使うわけではない。

### 9.7 LNS：複数agentをまとめた改善

LNS（Large Neighborhood Search）は現在解の一部agentを選び、その経路だけを再計画する。他の経路は固定予約として扱う。

通常の対象選択では、あるagentの経路上の頂点に2、隣接頂点に1の印を付け、各agentの経路が通る印の合計を関連度にする。0始まりの反復番号が3の倍数なら最大遅延のagent、それ以外なら乱択のagentを起点にする。ただし反復番号を4で割った余りが3のときは、この規則より先に集合全体の乱択を選ぶ。通常は関連度、同点なら独立最短距離に対する遅延で対象を選ぶ。起点agentが必ず対象へ入るという制約は置かない。

再計画順は遅延順を基本とし、交互にshuffleする。候補を採用するのは、指定した辞書順目的を厳密に改善したときだけ。同点の変更はLNSでは採用しない。`lns_iterations<=0` または `lns_size<=0` なら実行しない。

### 9.8 repair：固定範囲と探索の分離

基準の全経路を最初に検証する。全経路repairでは選択agentのgoalを変えられ、時間窓repairでは窓の両端を局所問題の始点・終点にする。

非選択agentと外部経路を先に予約し、選択agentだけを計画する。最初は基準経路に対する遅延の大きい順、その後は失敗agentの優先と乱択を使う。既に試した順序は繰り返さず、対象が1体なら同じ順序を何回も実行しない。順序生成は有限回の乱択で打ち切る場合があるため、要求した回数・全順列を必ず試すわけではない。

restart間では固定予約を再利用し、今回再計画したagentの予約だけを解除する。新条件で基準解も有効なら候補と比較する。基準が厳密に良い場合は基準を残すが、同点なら候補が選ばれ得る点がLNSとは異なる。

時間窓の候補が終端へ早着した場合も、窓末まで待機する占有を予約する。全体へ接合した後、選択経路末尾の連続する待機を取り除き、全経路の費用を再計算する。

外部予約なしでは、入口で成立した基準経路の合法性を再利用し、新goalとの一致を調べる。外部予約ありでは基準解をその制約も含めて検証する。返す候補は外部予約の有無に合ったvalidatorで確認する。

### 9.9 MAPDのタスク割当

現在時刻を $t$、空きagentの現在頂点を $v_a$、グラフの最短距離を $d(u,v)$ とする。$u,v$ は頂点、$p_j,q_j,r_j,H$ は第1章の記号である。解禁済み・未割当タスクに対し、まず次の所要時間下界を確認する。

$$d(v_a,p_j)+d(p_j,q_j)\leq H-t$$

どちらかが到達不能なら候補にしない。等号は許す。衝突待ちを無視した下界なので、候補になっても実際の完了は保証しない。

候補対の割当費用 $c(a,j)$ は次で、空荷の移動を重く見る。

$$c(a,j)=2d(v_a,p_j)+d(p_j,q_j)$$

この $c(a,j)$ は第1章の最終スコアとは異なる内部の代理費用である。

- `greedy`：費用、task番号、agent番号の順で対を選ぶ。空きagent数を $A$ として、各agentの上位 $A$ 件だけを残して全体を整列する。他agentが先に使えるタスクは高々 $A-1$ 件なので、この削減は通常greedyの選択に必要な候補を保つ。
- `greedy_unique_endpoints`：使用中の集荷・配達端点を避ける対を先に選び、2巡目で端点重複も許して残りを割り当てる。
- `min_cost_flow`：空きagentと候補taskの二部ネットワークを作り、容量1の最小費用流で割当数最大、次に割当費用最小を求める。

割当済みタスクを途中で解除・再割当する処理はない。候補から外れた未割当タスクは入力配列に残り、次時刻の候補評価で再び考慮され得る。

### 9.10 MAPDの時刻進行とautomatic

単独方式は、各時刻で到着処理、空きagentへの割当、同時刻の集荷・配達処理、PIBTによる次位置決定を行う。搬送中agentは未集荷・idleより優先する。搬送中同士ではageを優先し、同じなら配達先に近い方を先にする。

idle agentは初期位置または直前の配達地点を待機目標にするが、他agentのために移動することがある。PIBTで全員の合法な次位置を決められなければ、そのターンは全員待機する。期限切れ、または全タスク完了かつ全員が待機目標にいる場合は、残りの経路を待機で埋める。

automaticの候補方式順は、greedy、端点分散greedy、最小費用流、greedy、端点分散greedy、greedy、最小費用流、greedy。各候補は初期状態からシミュレーションし、seedも変える。`portfolio_runs` はこの列の先頭から使う候補数で、最大8。候補間で距離表や搬送途中状態を共有する公開workspaceはない。

各有効候補を、完了数最大、フロー時間最小、実移動回数最小の順で比較する。全候補を実行できるかは計算予算による。

### 9.11 計算量と計測時に見る項目

ここでは $N,M,K,H,U$ を第1・5・7章の意味で使う。$\Delta$ は最大次数、$R$ は構築試行数、$I$ はLNS反復数、$k$ は再計画するagent数、$B$ は明示的な禁止移動数、$P$ は出力経路の総頂点数とする。

| 処理 | 主な費用・注意 |
| --- | --- |
| グラフ構築 | 入力辺対数を $M_{\mathrm{in}}$ として $O(N+M_{\mathrm{in}}\log(M_{\mathrm{in}}+1))$ の上界。重複を含む入力量に依存し、隣接リストを整列・重複除去 |
| 距離準備 | 新target分の $O(U_{\mathrm{new}}(N+M))$ と、必要なら距離配列再確保 |
| 1回の予約初期化 | $O(N(H+1))$。固定経路の保持登録は別に $O(K(H+1))$ 程度 |
| 1体のA* | 実際の展開数、隣接遷移数、heap操作数に依存。第二費用の更新で状態が再登録され得るため、単純に全状態を1回ずつ見るとは限らない |
| 初期構築 | 最大 $R$ 回の経路計画。goal干渉評価には $O(K^2)$ の処理もある |
| LNS | 最大 $I$ 回。毎回の集合選択・予約と $k$ 体の再探索。経路のコピーと費用集計も必要 |
| 時間窓repair | 探索用horizonは窓幅。ただし基準検証と全経路への接合・検証は窓外を含む |
| MAPF検証 | 外部予約なしなら概ね $O(P\log(\Delta+1)+(L+1)(N+K))$。$L$ は経路から求めたmakespan |
| 外部予約付き検証 | 上の時刻走査を $\max(L,H_{\mathrm{ext}})$ まで拡張し、禁止移動照会の費用を加える |
| 外部禁止移動照会 | 追加後の初回は $O(B\log(B+1))$ の整理、その後は1照会 $O(\log(B+1))$。空なら定数時間 |
| MAPD | 距離準備、最大 $H$ ターンの割当・PIBT、$K(H+1)$ の出力と検証。automaticでは実行した候補数分の処理 |

MAPDの通常greedyでは、空きagent数を $A$、解禁済み未割当task数を $T$ とすると、候補費用計算に $O(AT)$、残した候補の整列に最大 $O(A\min(A,T)\log(A\min(A,T)+1))$ を使う。上位候補の抽出には `std::nth_element` を使う。候補を削減しても配列を `reserve(A*T)` するため、この候補配列の確保容量は $O(AT)$ である。端点分散greedyは全候補を扱うため整列が $O(AT\log(AT+1))$。ここで $T$ は時刻ではなく、その割当時点の候補task数である。

最小費用流では辺数が概ね $O(AT+A+T)$、流量が高々 $\min(A,T)$。1回の割当が軽いとは限らない。PIBTは再帰的な候補試行とrollbackを含むため、最悪時まで単純な $O(K)$ の1ターン処理とはいえない。

MAPD検証は経路と時刻ごとの衝突検査に加え、完了タスクの搬送区間をagentごとに整列する。上界の目安は $O(K(H+1)\log(\Delta+1)+(H+1)(N+K)+J\log(J+1))$。

性能を見るときは、時間だけでなく、成功率、指定目的の辞書順、外部制約違反、固定経路の変更有無、展開数を一緒に確認する。距離準備を済ませた計測と、新しいworkspaceからの計測を区別し、時間制限ありの品質比較と、固定反復の処理時間比較も分ける。
