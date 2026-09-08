# Maximum Coverage Solver 利用ガイド

対象ファイルは [maximum_coverage_solver_v06.hpp](maximum_coverage_solver_v06.hpp)。候補集合と被覆対象を入力し、選択数・予算・全対象被覆のいずれかを条件として候補を選ぶC++20ライブラリである。外部のアプリケーションや最適化プログラムから利用するための入力設計、公開API、関数例、実装を説明する。

第1章で問題を理解し、第2章で厳密解法の適用可否、第3章で反復利用の可否を判断する。第4章・第6章・第8章の同じ枝番号は、同じユースケースを表す。たとえばセンサー配置は4.2節で問題を理解し、6.2節でAPIの使い方を確認し、8.2節の関数をコピーして使える。

## 1. 何を解くライブラリか？

### 1.1 「選ぶもの」と「覆いたいもの」を分ける

たとえば、3か所の施設候補から2か所を選び、できるだけ多くの住民にサービスを届けたいとする。このとき「施設候補」が選ぶもの、「住民または需要地区」が覆いたいものである。施設ごとに、サービスを届けられる地区の一覧を用意する。

本ガイドでは、選ぶものを**候補**、覆いたいものを**対象**、ある候補によって対象を利用可能・到達可能・検査済みなどにできる関係を**被覆**と呼ぶ。同じ対象を2つの候補が覆っても、その対象の価値は1回だけ数える。

候補と対象は別の番号空間である。候補0と対象0が同じ実物を意味する必要はない。距離、実行時間、通信条件などから被覆関係を作るのは利用側の責任になる。

### 1.2 共通の記号と選択条件

以下の記号を、3種類の問題で共通して使う。

| 記号 | 意味 | 入力か、求める値か |
|---|---|---|
| $T$ | 対象の個数。0以上の整数 | 入力 |
| $C$ | 候補の個数。0以上の整数 | 入力 |
| $\mathcal{T}=\{0,\ldots,T-1\}$ | 対象番号の集合。$T=0$ なら空集合 | 入力から決まる |
| $\mathcal{C}=\{0,\ldots,C-1\}$ | 候補番号の集合。$C=0$ なら空集合 | 入力から決まる |
| $t$ | 1つの対象番号 | 式中の添字 |
| $c$ | 1つの候補番号 | 式中の添字 |
| $S_c\subseteq\mathcal{T}$ | 候補 $c$ が覆う対象集合 | 入力 |
| $w_t\ge0$ | 対象 $t$ を少なくとも1回覆う価値。整数 | 入力 |
| $a_c\ge0$ | 候補 $c$ を選ぶ費用。整数 | 入力 |
| $X\subseteq\mathcal{C}$ | 実際に選ぶ候補番号の集合 | 求める値 |
| $U(X)$ | 選択集合 $X$ によって覆われる対象の和集合 | $X$ から決まる |
| $W(X)$ | 覆われた対象の価値の合計 | 目的値または診断値 |
| $A(X)$ | 選択候補の費用の合計 | 目的値または制約値 |
| $F$ | 必ず選ぶ候補の集合 | 任意指定。省略時は空集合 |
| $D$ | 選んではいけない候補の集合 | 任意指定。省略時は空集合 |
| $P$ | 固定候補以外で今回選んでよい候補の集合 | 任意指定。制限しなければ全候補 |
| $L,K$ | 選択数の下限と上限。$0\le L\le K$ | 選択数制約で指定。通常は $L=0$ |
| $B$ | 総予算。0以上の整数 | 予算制約で指定 |

和集合、価値、費用は次のように定義する。空集合についての和は0である。

$$U(X)=\bigcup_{c\in X}S_c,\qquad W(X)=\sum_{t\in U(X)}w_t,\qquad A(X)=\sum_{c\in X}a_c$$

任意の固定・禁止・候補制限を使う場合、どの問題でも次の条件を加える。

$$F\subseteq X\subseteq P\cup F,\qquad X\cap D=\varnothing$$

固定候補は候補制限の外でも選ばれるが、禁止候補にはできない。固定候補の個数・費用も制約に含める。

### 1.3 選択数制約付き最大被覆

「最大5か所」「最低3個、最大8個」のように選択数を決め、その範囲で覆える価値を大きくする。

$$\underset{X\subseteq\mathcal{C}}{\operatorname{maximize}}\ W(X)\qquad\text{subject to}\quad L\le |X|\le K$$

$|X|$ は選択候補数である。式は「個数の条件を満たす選び方の中から、覆われる対象の重み和が大きいものを求める」という意味になる。$L=0$ なら最大 $K$ 個、$L=K$ ならちょうど $K$ 個を選ぶ。候補費用 $a_c$ はこの問題の予算制約には使わない。

### 1.4 予算付き最大被覆

候補によって設置費・実行費が異なる場合に、総予算の範囲で覆える価値を大きくする。

$$\underset{X\subseteq\mathcal{C}}{\operatorname{maximize}}\ W(X)\qquad\text{subject to}\quad A(X)\le B$$

式は「選んだ候補の費用合計が $B$ 以下になるようにし、覆われる対象の重み和を大きくする」という意味である。候補数の上限・下限は同時には指定できない。

### 1.5 最小費用集合被覆

「全要件を検査したい」「すべての仕事を実施したい」のように、全対象を必ず覆い、そのための費用を小さくする。

$$\underset{X\subseteq\mathcal{C}}{\operatorname{minimize}}\ A(X)\qquad\text{subject to}\quad U(X)=\mathcal{T}$$

式は「1つも対象を取りこぼさない選び方の中で、候補費用合計を小さくする」という意味である。すべての費用を1にすると、選ぶ候補数を小さくする問題になる。対象重み $w_t$ はこの目的関数に使わず、重み0の対象も被覆必須である。

3問題とも最適解を保証するAPIではない。また、制約を満たす解が存在しない場合や、締切までに全対象被覆が完成しない場合がある。返却結果では、目的値より先に実行可能性を確認する。

## 2. 厳密解アルゴリズム

### 2.1 本ライブラリを使う前に確認すること

本ライブラリは、大きな集合系や短い計算時間で実行可能な解を得たい場合の選択肢である。最適性の証明が必要な場合や、以下の小さな規模・特殊構造に当てはまる場合は、厳密解法も検討する。実行可能解が存在し、厳密解法を最後まで実行できれば、同じ目的・制約について本ライブラリの解より悪くない解が得られる。最大被覆なら同等以上の重み、集合被覆なら同等以下の費用という意味である。ただし、すでに最適解を得ていれば同値であり、必ず速いとは限らない。

この章の全探索・DP・最小カット・整数計画は、ライブラリに内蔵されたモードではない。利用側で別途実装・導入する選択肢である。以下の計算量は主目的値を求める目安で、入力の読込み、解の復元、同点時の副目的を厳密に扱うための追加記録は別に考える。

固定集合 $F$ を最初に採用し、禁止集合 $D$ とpool制限を反映した後、自由に選べる候補の個数を $q$ とする。固定候補の費用・個数は予算・選択数から差し引き、すでに覆った対象を二重に加点しない。固定条件だけで資源上限を超えていれば、探索を始める前に実行不能である。

| 入力の特徴 | 厳密解法の候補 | 判断のポイント |
|---|---|---|
| 自由候補数 $q$ が小さい | 候補部分集合の全探索 | $2^q$ 通りを列挙できるか |
| 残り選択枠がごく少ない | 候補の組合せ列挙 | 候補数全体より、調べる組合せ数が重要 |
| 対象数 $T$ が小さい | 被覆集合をbit maskにしたDP | $2^T$ 状態の時間・メモリが収まるか |
| 候補間で被覆対象が重ならない | 個数なら並べ替え、予算なら0/1ナップサックDP | 重なりがないことが必要。小予算だけでは不十分 |
| 集合被覆が二部グラフの頂点被覆になる | 最小カット | 第2.5節の構造条件を満たすか |
| 最適性証明や追加の組合せ制約が必要 | 整数計画・CP-SAT | 時間内に最適性を証明できるか、追加依存を許容できるか |

「候補は多いがLNSで動かす候補poolが小さい」場合も、この判断を部分問題に適用できる。元問題の候補数だけで厳密解を諦める必要はない。

### 2.2 候補全探索と、選択数を固定した組合せ列挙

自由候補を採用するかしないか、全 $2^q$ 通りを調べる。各選択の被覆和集合・費用・個数を計算し、第1章の条件を満たすものの中から最良を選べば、3種類の問題すべてを厳密に解ける。探索中の被覆回数を差分管理すれば、各集合で最初から和集合を作り直す必要はない。

個数制約の残り上限を $k=K-|F|$、残り下限を $\ell=\max(0,L-|F|)$ とすると、列挙対象は $\sum_{j=\ell}^{\min(k,q)}\binom{q}{j}$ 通りである。ここで $j$ は今回追加する候補数。$\ell>\min(k,q)$ なら実行不能になる。exact Kなら $j=k$ の組合せだけでよい。

特に追加が最大1候補なら、固定候補からの追加利得を全候補について調べるだけで主目的の厳密解を得られる。全体の $K=1$ だけでなく、固定部分が大きく残り1枠の部分問題にも使える。

指数時間でも、$2^{20}=1,048,576$ と $2^{30}=1,073,741,824$ では大きく違う。これらは選択集合の数であり、実行時間の保証ではない。被覆関係の密度、1集合の評価費用、許される時間を含めて試算する。

### 2.3 対象数が小さい場合のbit mask DP

対象の被覆状態をTビットのmaskで表す。候補を1つずつ処理し、「選ばない」「選んで被覆maskとのORを取る」の2通りへ遷移する。対象数をパラメータとする集合被覆の部分集合DPは、$2^T$ に多項式因子を掛けた時間で解く基本的な厳密解法である。[Cyganほか「On Problems as Hard as CNF-SAT」](https://cseweb.ucsd.edu/~paturi/myPapers/pubs/CyganDellLokshtanovMarxNederlofOkamotoPaturiSaurabhWahlstrom_2012_ccc.pdf)

以下はその状態設計を第1章の各問題へ適用したものである。maskのOR・参照を定数時間とみなせる小さなTを前提とする。

| 問題 | DPで持つ値 | 答えの取り方 | 時間・値だけのメモリ |
|---|---|---|---|
| 最小費用集合被覆 | 各被覆maskを作る最小追加費用 | 全対象が立つmaskの費用に固定費を加える | $O((q+1)2^T)$、$O(2^T)$ |
| 予算付き最大被覆 | 各被覆maskを作る最小追加費用 | 固定費込みで予算内のmaskから重み和最大を選ぶ | $O((q+1)2^T)$、$O(2^T)$ |
| 下限なしの個数制約 | 各被覆maskを作る最小追加候補数 | 固定候補込みでK個以内のmaskから重み和最大を選ぶ | $O((q+1)2^T)$、$O(2^T)$ |
| 下限・exact Kを含む個数制約 | 被覆maskと追加候補数ごとの到達可否 | 下限・上限を満たす状態から重み和最大を選ぶ | $O((q+1)(k'+1)2^T)$、$O((k'+1)2^T)$ |

表の $k'=\min(K-|F|,q)$ は記録する追加候補数の上限である。最初の状態は固定候補の被覆mask、追加費用・追加個数は0とする。各maskの重み和は、1ビット落としたmaskの重み和へその対象重みを加え、全 $2^T$ 状態ぶん前計算できる。最小費用の状態は「処理済み候補の範囲」とmaskが同じなら安い方だけ残せる。今後使える候補が同じであり、主目的の被覆重みもmaskだけで決まるからである。

各候補は0回または1回だけ使う。前段と次段を分けた2配列で遷移すれば、費用0・空集合・同一被覆候補でも同じ候補を二度使わない。exact Kでは被覆が増えない候補も個数を満たすために必要になり得るため、単純に候補を捨てたり「最小個数だけ」の状態で済ませたりしない。

費用DPの計算量は数値としての予算Bには比例しない。一方、対象数には指数的である。例えばT=24なら状態数は16,777,216、8byteの値配列1本だけで128MiB、2本なら256MiBになる。個数次元、復元情報、入力データはさらに必要になる。

### 2.4 被覆対象が互いに重ならない場合

異なる候補の対象集合が互いに素なら、候補の価値は個別に加算できる。固定候補と禁止条件を先に反映した後の候補価値を使う。

- 個数制約では、価値の大きい候補から残り上限まで選べば主目的の厳密解になる。0価値の候補は必要な下限まで補えばよく、候補を並べ替える時間は $O(q\log(q+1))$ である。
- 予算制約では0/1ナップサックになる。残り整数予算を $b=B-A(F)$ とすると、費用ごとの最大価値を持つDPで $O(q(b+1))$ 時間、$O(b+1)$ メモリを使える。費用0でも候補は1回だけ選ぶ。これは予算の値に依存する擬多項式時間であり、大きな予算には適さない。
- 集合被覆では、各対象を覆う候補が高々1つなので、各対象に対応する非空候補がすべて必要になる。覆える候補のない対象があれば実行不能である。

重なりがある一般の最大被覆では、「どの対象を覆ったか」が次の候補の利得を変える。残り予算だけを持つ通常のナップサックDPへ置き換えることはできない。

### 2.5 二部グラフの頂点被覆に一致する集合被覆

各対象がちょうど2候補のどちらかで覆われ、その2候補を辺で結んだ「候補を頂点とするグラフ」が二部グラフなら、最小費用集合被覆は最小重み頂点被覆になる。この特殊問題は最小カットで厳密に解ける。候補と対象を左右に分けた通常の被覆関係グラフが二部である、というだけではこの条件を満たさない。[Tufts University掲載「Playing Push vs Pull」の重み付き二部頂点被覆](https://www.cs.tufts.edu/comp/150DS/TalkPushPull55.pdf)

対応付けは、候補費用を頂点重み、各対象をその2候補間の辺とするものである。二部の片側へ始点から費用容量の辺、反対側から終点へ費用容量の辺を張る。元の辺に対応して、始点につなぐ側の候補から終点につなぐ側の候補へ有向辺を張り、全候補費用和より大きな容量を与える。最小カットで切る費用辺が採用候補を表す。この大容量を有限整数で実装するときは、その値と容量総和のオーバーフローを別途確認する。

固定候補があればその対象を被覆済みにし、禁止候補を除く。残った対象の選択肢が1候補だけなら、その候補を必須にして同じ処理を繰り返す。0候補なら実行不能。残る対象が2候補で、候補間グラフが二部である場合にこの解法を使える。これは全被覆・費用最小化の話であり、予算付き部分被覆や個数制約付き最大被覆まで同じ最小カットで解けるという意味ではない。

### 2.6 整数計画・CP-SATで最適性を証明する

候補を選ぶ0/1変数を $x_c$、対象が覆われる0/1変数を $y_t$ とする。最大被覆は $\sum_t w_t y_t$ を最大化し、各対象について $y_t\le\sum_{c:t\in S_c}x_c$ を課す。個数なら $L\le\sum_c x_c\le K$、予算なら $\sum_c a_cx_c\le B$ を追加する。非負重みなのでこの制約で主目的の最適値は一致するが、重み0の $y_t$ は実際の被覆と一致しない場合がある。被覆数の報告は採用候補から再計算するか、論理ORの両方向の制約を入れる。

集合被覆では各対象に $\sum_{c:t\in S_c}x_c\ge1$ を課し、$\sum_c a_cx_c$ を最小化する。固定候補は $x_c=1$、禁止・pool外の非固定候補は $x_c=0$ とする。表記のない和の添字は、それぞれ第1章の全対象または全候補を走る。対象を覆う候補がない場合、候補についての和は0である。

外部の整数最適化solverなら、候補間競合・複数予算などを追加できる。ただしモデル構築・依存ライブラリ・探索時間が必要で、一般入力を短時間で厳密に解ける保証はない。CP-SATでは `OPTIMAL` と `FEASIBLE` は別の状態であり、時間内に実行可能解が見つかっただけでは最適性の証明にならない。設定したギャップ許容値がある場合は、その停止条件も確認する。[OR-Tools公式ドキュメント](https://developers.google.com/optimization/cp/cp_solver)

### 2.7 実用上の選び方

最初に固定・禁止・poolを反映した規模を数え、全探索・DP・特殊構造のどれかに収まるかを確認する。厳密性が必須で収まらない場合は、より長い時間の整数最適化や問題固有の厳密解法を検討する。

厳密性より時間内の品質を優先するなら本ライブラリを使い、小規模な同型問題だけ厳密解と照合すると品質差を把握しやすい。比較するときは固定候補、重み0対象の全被覆、exact K、費用0候補も同じ意味で扱う。特殊条件を満たしていても本ライブラリ自身がそれを検出して厳密解へ切り替えることはない。

## 3. 差分更新

### 3.1 何を再利用できるか

同じ被覆関係を使い、ターンごとに重み・費用・予算・利用可能候補が変わる用途では、構築済みの `coverage_solver` を保持して再利用できる。前回の `selected_candidates` を `improve_*` へ渡せば、その選択集合を初期解として使える。

ただし、これは**固定した被覆モデルの再利用と初期解の引継ぎ**である。変更した辺や対象だけを更新する動的グラフsolverではない。被覆回数、heap、乱数の途中状態、探索履歴を次の呼び出しへ持ち越すAPIはない。毎回新しい探索状態を作り、`options.seed` から乱数系列を始める。

| 変更するもの | 同じsolverで可能か | 指定方法・必要な処理 |
|---|---|---|
| 個数上限・下限、予算 | 可能 | その回のsolve・improveの引数を変える。制約の種類も呼び出し単位で選べる |
| 対象の重要度 | 可能 | `target_weights_override` にT個の完全な非負配列を渡す |
| 候補の費用 | 可能 | `candidate_costs_override` にC個の完全な非負配列を渡す |
| 一時的な故障・禁止、今回の維持対象 | 可能 | `forbidden_candidates`、`fixed_candidates` をその回の `coverage_subproblem` に設定 |
| 非固定候補の選択肢 | 可能 | `restrict_to_candidate_pool = true` と `candidate_pool` で制限 |
| 初期の選択集合 | 可能 | 現在の条件に合わせた候補番号を `improve_*` へ渡す |
| 時間・反復上限・seed | 可能 | `coverage_solver_options` をその回に指定 |
| 候補が覆う対象の一覧 | 不可 | `coverage_problem.covered_targets` を作り直してsolverを再構築 |
| 対象数・候補数、番号の意味 | 不可 | 配列と番号対応を再作成してsolverを再構築 |
| 構築時の恒久固定・禁止の解除 | 不可 | 新しい恒久条件でsolverを再構築。ターンで解除する条件は最初から一時条件にする |

`coverage_subproblem` の条件はその1回だけ有効である。配列を省略した次の呼び出しは、直前のターンの値ではなくコンストラクタに渡した値を使う。空のoverrideは「差し替えなし」であり、全0への変更ではない。

### 3.2 グラフの変更と再構築の境界

ライブラリが保持しているのは元の道路・通信グラフではなく、候補から対象への被覆関係である。元グラフの辺、距離、センサー位置、半径が変わっても、被覆対象の一覧がまったく変わらなければ再構築は不要。一覧が1か所でも変われば、その関係を更新したモデルでsolverを再構築する。

元グラフの1辺の変更が多数の候補の到達範囲を変える場合もある。影響する到達範囲の更新は利用側で行い、その後にコンストラクタへ完全なモデルを渡す。構築元の `coverage_problem` を書き換えるだけでは、既存solverは更新されない。

候補の一時的な削除は「禁止」、追加は「あらかじめ登録しておいた候補をpoolへ戻す」と表せる場合がある。将来候補を事前登録すると構築・メモリ・走査の負担は増える。登録時に恒久禁止にすると解除できないため、一時禁止かpoolで制御する。

対象の重要度を0にすると最大被覆の得点対象からは外せるが、集合被覆の必須対象からは外れない。集合被覆で対象を追加・削除したい場合は、その対象集合に合わせた再構築が必要である。

### 3.3 前の解を安全に渡す手順

ターンの開始時に、現在の制約と完全な重み・費用配列を用意する。その後、次の順に処理する。

1. 今回の禁止・pool外になった非固定候補を前回解から取り除く。候補番号が同じモデルを意味すること、重複がないことを確認する。
2. 今回の固定候補を含めた値を `evaluate(initial_candidates, subproblem)` で求める。固定候補は自動追加される。前回返却値の `total_cost` や `covered_weight` は今回の条件では使わない。
3. 個数版では固定追加後の個数が上限以下、予算版では固定追加後の費用が予算内か確認する。超過したまま `improve_*` へ渡すと例外になる。
4. 有効なら `improve_*` へ渡す。超過している場合は、利用側で非固定候補を減らして再評価するか、`solve_*` で新規に解く。固定候補だけで制約を破っているなら実行不能なので、条件自体の見直しが必要になる。
5. `feasible` を確認し、必要な外側制約も検査してから次の解として保存する。

個数の下限不足は `improve_maximum_coverage(bounds, ...)` が補充できる。集合被覆の未被覆は `improve_minimum_set_cover` がrepairできる。上限・予算の超過とは扱いが異なる。

有効な初期解の主目的値を比較対象として保持するが、その基準は**今回の条件で再評価した初期解**である。前回の異なる重みでの得点、故障候補を含む集合の得点、予算超過した旧解との非悪化は保証しない。選択集合を変える費用や変更個数も目的には入っていない。

8.11節には、禁止・pool変更を反映して旧解を絞り、費用超過なら新規solveへ切り替える関数例を示す。これは安全性を優先した呼び出し側の例で、ライブラリへの新しいAPI追加ではない。

### 3.4 再実行のコストと時間管理

再利用により、被覆集合の正規化、両方向CSRの再作成、構築時の重複・包含判定は避けられる。一方、重みを1個だけ変えた場合もT個の配列を渡し、候補ごとの重み和を再計算する。一時条件がある場合はmask・希少度などの構築があり、変更箇所数だけに比例する更新ではない。

対象数T、候補数C、正規化後の被覆関係総数Eとすると、部分問題の準備は概ね $O(T+C+E)$ に候補リストの読込みを加えた量になる。改善探索と状態初期化は別である。前回解を使っても新規の決定的構築との比較を行うので、必ず新規solveより速くなるという意味ではない。

`evaluate` で事前確認してから `improve_*` へ渡す例では、準備と状態評価も複数回になる。外側が同じ制約・集計をすでに正しく管理しているなら、重複検査を省いて直接 `improve_*` を呼べる。初めは安全な例で整合性を確認し、実測上の負担がある部分だけ外側の差分管理へ置き換える。

solverはターンループの外で1回構築し、今回の絶対 `deadline` を入力処理開始時などに決めて各呼び出しへ渡す。期限はsoft limitで、構築・準備・補充・出力用の安全余裕は必要である。同じsolverのsolve・improve・evaluateを並行実行してはいけない。`const` でも内部の作業配列を共有する。

## 4. ユースケース

題材だけが違い、指定内容が同じ用途は同じ節にまとめた。4.1～4.5節は被覆関係の作り方が異なる用途、4.6～4.13節は選択条件・再利用・外側の処理との連携が異なる用途である。この章ではプログラム上の型やメソッド名は使わず、入力として何を決めるかを説明する。

### 4.1 道路・通信グラフ上の施設配置

道路網のどこに拠点を置けば、限られた施設数で多くの地区に到達できるかを考える。通信網の中継拠点や、グラフ上の観測点にも同じ形を使える。

必須なのは、頂点と辺、重要な対象頂点、対象ごとの重要度、何本の辺まで届くかという半径、施設数上限である。施設を置ける頂点と、評価対象の頂点は分けて決める。

距離内なら覆う、距離外なら覆わないという二値モデルである。重み付き道路の移動時間は、利用側で到達範囲を計算してから入力する。

### 4.2 距離と費用を持つセンサー配置

平面上の需要点を、半径と設置費が異なる監視装置で覆う。Wi-Fiアクセスポイント、観測機器、看板から見える地域の選択などに使える。

各需要点の位置と重要度、設置候補の位置・有効半径・費用、総予算を指定する。遮蔽物や方向制限があるなら、それも反映して候補ごとの被覆対象を決める。

出力は採用する装置の候補番号である。1台の処理容量や、複数装置を設置すると電波が弱まる相互作用は表現しない。

### 4.3 全要件を満たすテスト・検査の削減

多数のテストから、全要件を少なくとも1回検査できる組み合わせを選び、実行時間や費用の合計を小さくする。品質検査の項目選択、監査証跡の収集にも対応する。

全要件の一覧、各テストが検査できる要件、各テストの費用が必須になる。最少テスト数を求めたい場合は全費用を1にする。

対象として登録した要件はすべて必須である。テストを並列実行したときの完了時刻や、実行順序までは最適化しない。

### 4.4 特徴を広く含む代表要素の選択

文書から代表文を選んで論点を広く含める、商品を選んで機能や顧客層を広く扱う、研修教材を選んで技能項目を広く学べるようにする、といった用途である。広告案による到達対象の選択も同じモデルになる。

何を1つの特徴・論点・顧客群として数えるか、その重要度、各候補が含む特徴、選択数上限を指定する。

「候補同士が異なること」そのものを得点化するのではなく、覆われる特徴の和集合を評価する。文章の自然なつながり、商品間の相性、広告接触確率は別に扱う。

### 4.5 事前に作ったルート・作業案の組み合わせ

利用側で作った配送ルート、訪問計画、作業パッケージを候補として、すべての仕事を実施できる組み合わせを選ぶ。各候補内部の時間・順序・容量を確認済みなら、それらをまとめた選択問題として使える。

全仕事の一覧、各候補案が実施する仕事、候補案の総費用を指定する。契約済みの案は固定でき、使えない案は禁止できる。

同じ仕事が複数案に含まれることは許す。同じ車両を同時に使う競合や、各仕事をちょうど1回だけ実施する条件は、被覆だけでは表せない。

### 4.6 選択数の下限・上限と、ちょうどK個の選択

展示スペースをちょうど埋める、稼働拠点を最低限確保する、出力形式が一定個数を要求するなど、選択数に下限がある用途である。

対象重み・候補の被覆関係に加え、選択数の下限と上限を指定する。同じ値にすると、ちょうどその個数を選ぶ。費用も入力するが、予算制約にはならない。

必要数を満たすため、追加の被覆価値がない候補が選ばれる場合がある。ちょうどK個という条件だけでは、地域分散やカテゴリーごとの配分は指定できない。

### 4.7 既設候補を維持した増設・故障対応

すでに設置した拠点を残したまま増設する、故障・休止した候補を除いて再配置する用途である。被覆関係が変わらず、採用可能な候補だけが変わる場合に適する。

共通の対象・候補・費用と今回の総予算に加え、今回残す候補と今回使えない候補を指定する。変わらない禁止条件と、呼び出しごとに変わる禁止条件を分けられる。

残す候補の費用も総予算に入る。増設分だけの予算を扱いたいなら、既設分の計上方法を入力費用または総予算に反映する。同じ候補の維持と禁止は両立しない。

### 4.8 外部で作った最大被覆解の改善

手作業や問題固有の処理で作った選択集合を渡し、被覆価値をさらに高くする用途である。すでに良い解があるとき、それを候補として活用できる。

同じ対象・候補・重み・費用、個数上限または予算、初期の候補集合を指定する。個数下限があっても初期集合は下限未満でよいが、上限や予算の超過は許さない。

初期集合をそのまま維持する指定ではない。変更してはいけない候補は別に固定する。比較する価値は、その回の重みと固定条件を反映した値になる。

### 4.9 未完成の集合被覆解の修復

一部のテストや作業案だけが決まっていて、まだ覆えていない要件を埋めたい場合である。全対象を覆った完成解を渡して費用削減を行う用途も含む。

全対象、候補ごとの被覆関係・費用、現在選んでいる候補を指定する。未被覆の対象が残っていても入力できる。

完成のために候補を追加するので、未完成解より費用が増えることはある。必ず残す候補は固定する。完成済みの有効な初期解なら、その費用を悪化させない。

### 4.10 複数の乱数seedによる解の比較

同じ入力に何回か取り組み、その中で良い解を選ぶ用途である。1回の計算結果だけで判断したくないときに使う。

共通の問題と制約に加え、試す乱数seedの列、各回に使う時間、全体で共有する締切を指定する。比較する目的は各回で揃える。

回数を増やしても品質向上の保証はない。候補集合が同じになる場合もある。全体時間には準備と結果処理も含めて余裕を確保する。

### 4.11 需要・費用・予算が変わるシナリオ・ターンの反復

同じ設備配置で朝夕の需要が変わる、調達価格や予算が日ごとに変わる、対象の重要度を変えて感度を見る用途である。

共通の被覆関係と、シナリオごとの対象重み全体・候補費用全体・予算を指定する。変わった要素だけでなく、各回で使う完全な配列を用意する。

各シナリオを独立に解く方法と、ターンごとに前回の選択を見直して再利用する方法がある。後者では、使えなくなった候補を除き、固定候補を加えた費用が今回の予算内かを確認してから引き継ぐ。予算を超える場合は新規に解き直すこともできる。

出力間の変更回数を最小化したり、複数シナリオに共通する1つの解を求めたりする条件ではない。被覆関係そのものが変わるときは再構築が必要になる。

### 4.12 大きな探索の一部分だけを再構築

大きな最適化の途中で、現在解の大部分を固定し、一部だけ選び直す用途である。全候補を毎回自由に選び直すと外側の構造を壊してしまう場合に役立つ。

残す候補、その回だけ禁止する候補、補充に使ってよい候補一覧、完成時の個数、計算時間または反復上限、全体締切を指定する。

固定部分は補充候補一覧の外でも有効である。固定部分と補充可能な候補を合わせても必要数に届かなければ実行不能になる。外側に追加制約があるなら、完成案を外側でも検査する。

### 4.13 別の処理が作った選択集合の評価

別のプログラムや手作業で選んだ集合について、重複被覆を除いた得点・費用・未被覆数を確認する用途である。外側の差分計算の誤りを調べる場合にも使える。

問題と選択集合を指定し、必要なら固定・禁止・対象重み・費用をその回の条件に揃える。個数や予算の判定に使う上限も利用側で用意する。

この用途は新しい解を探索しない。得点が正しくても、予算や他の業務制約を満たすとは限らないため、評価値と制約値を明示的に比較する。

## 5. 利用準備

### 5.1 ビルドとコンストラクタ

ヘッダを利用側のソースと同じディレクトリ、またはinclude検索パスに置き、C++20でコンパイルする。`std::span`、`<bits/stdc++.h>`、`__INCLUDE_LEVEL__` を使用するため、GCC環境を想定する。座標のコード例ではGCCの `__int128` も使う。追加の集合ライブラリは不要である。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion main.cpp -o main
```

コンストラクタの引数は次の1個だけである。アルゴリズムの調整値をコンストラクタに渡す引数はない。

```cpp
explicit coverage_solver(const coverage_problem& problem);
```

| 引数 | 型 | 既定値 | 設定方法 |
|---|---|---|---|
| `problem` | `const coverage_problem&` | なし。必須 | 下記の対象・候補・被覆関係・恒久条件を作って渡す |

入力は内部へコピーされる。構築後に元の配列を書き換えてもsolverへは反映されず、元の `coverage_problem` は破棄してよい。同じ被覆関係を繰り返し使う場合は、構築したsolverを再利用する。第8章の入力変換型の関数は、わかりやすさのため関数内で構築するものもある。

### 5.2 問題データの設定

```cpp
struct coverage_problem {
    std::vector<long long> target_weights;
    std::vector<long long> candidate_costs;
    std::vector<std::vector<int>> covered_targets;
    std::vector<int> forced_candidates;
    std::vector<int> forbidden_candidates;
};
```

| フィールド | 目的 | 値の条件 | 既定値・選び方 |
|---|---|---|---|
| `target_weights` | 各対象の被覆価値 | 各値は0以上。要素数は `int` の範囲内 | 空vector。集合被覆だけを使うなら全要素を1でよい |
| `candidate_costs` | 各候補の費用 | 各値は0以上。要素数は `covered_targets.size()` と一致 | 空vector。選択数制約だけを使うなら全要素を1にすると分かりやすい |
| `covered_targets` | 各候補が覆う対象番号 | 各番号は `0 <= t < target_weights.size()` | 空vector。同一候補内の重複は自動除去される |
| `forced_candidates` | 必ず選択する候補 | 各番号は候補範囲内 | 空vector。重複指定は許容されるが、通常は一意にする |
| `forbidden_candidates` | 選択禁止候補 | 各番号は候補範囲内 | 空vector。重複指定は許容されるが、通常は一意にする |

`forced_candidates` と `forbidden_candidates` に同じ候補を入れることはできない。

選択数制約でも `candidate_costs` は省略できない。費用は主目的の制約には使わないが、同一被覆候補の整理などに利用される。費用の意味がない場合は全候補を1にする。

集合被覆でも `target_weights` は省略できない。値は目的関数には使われないため、通常は全対象を1にする。

各フィールドの型は上の定義どおりで、すべて `std::vector`、既定値は空配列である。対象数Tは `target_weights.size()`、候補数Cは `candidate_costs.size()` から決まる。全要素を同じ値にしたい場合は `problem.target_weights.assign(T, 1)`、`problem.candidate_costs.assign(C, 1)` のように設定する。`covered_targets[c]` の各値は対象番号であり、業務上のIDを直接渡す場合は0始まりの連番へ対応付ける。

### 5.3 呼び出し単位で変更する条件

```cpp
struct coverage_subproblem {
    std::span<const int> fixed_candidates{};
    std::span<const int> forbidden_candidates{};
    bool restrict_to_candidate_pool = false;
    std::span<const int> candidate_pool{};
    std::span<const long long> target_weights_override{};
    std::span<const long long> candidate_costs_override{};
};
```

同じ被覆関係を保ったまま、1回の呼び出しだけに適用する条件である。全フィールドの既定値は空またはfalseなので、既定値なら通常solveになる。

| フィールド | 目的 | 条件と選び方 |
|---|---|---|
| `fixed_candidates` | 今回必ず選択し、LNSでも除去しない候補 | 範囲内、一意、恒久・一時禁止と競合不可。pool外でも選択される |
| `forbidden_candidates` | 今回だけ選択禁止する候補 | 範囲内、一意、固定候補と競合不可 |
| `restrict_to_candidate_pool` | 固定候補以外の選択肢をpoolへ限定 | 既定false。trueのときだけ `candidate_pool` を解釈する |
| `candidate_pool` | 今回探索してよい候補 | `restrict_to_candidate_pool == true` のときだけ範囲内かつ一意である必要がある。恒久禁止候補を含めても利用可能にはならない |
| `target_weights_override` | 対象重みを呼び出し単位で差し替える | 空、または要素数Tで全値0以上 |
| `candidate_costs_override` | 候補費用を呼び出し単位で差し替える | 空、または要素数Cで全値0以上 |

選択数制約付き最大被覆では候補費用は制約でも主目的でもなく、`total_cost` の計算とexact Kの不足分選択などに使う。安価な解を保証するものではない。最小集合被覆では対象重みは主目的に使わないが、返却する `covered_weight` には差し替え値を反映する。

`std::span` はデータを所有しない。solveが返るまで、参照元の `std::vector`、`std::array` などを破棄・再確保・変更しないこと。非同期に保持されることはなく、メソッド終了後まで寿命を延ばす必要はない。

| フィールド | 型 | 既定値 |
|---|---|---|
| `fixed_candidates` | `std::span<const int>` | 空。今回の追加固定なし |
| `forbidden_candidates` | `std::span<const int>` | 空。今回の追加禁止なし |
| `restrict_to_candidate_pool` | `bool` | `false` |
| `candidate_pool` | `std::span<const int>` | 空。制限trueなら固定候補以外を選べない |
| `target_weights_override` | `std::span<const long long>` | 空。構築時の値を使う |
| `candidate_costs_override` | `std::span<const long long>` | 空。構築時の値を使う |

たとえば手元の `std::vector<int> kept` を指定するなら `sub.fixed_candidates = kept` と代入する。overrideの空配列は「すべて0へ変更」ではない。全重みを0にするには、対象数と同じ長さの0配列を渡す。恒久固定・禁止は追加条件で解除できない。

### 5.4 選択数と予算の設定

```cpp
struct coverage_cardinality_bounds {
    int min_selected = 0;
    int max_selected = std::numeric_limits<int>::max();
};
```

条件は `0 <= min_selected <= max_selected`。`{K, K}` でexact K、`{L, K}` で範囲指定になる。最大被覆の探索はまず被覆スコアを改善し、最後に不足数を費用の安い利用可能候補で埋める。したがってexact Kでも「同一スコア内の費用最小化」は厳密には解かない。利用可能候補が `min_selected` 個未満なら `feasible == false` になる。

| 指定項目 | 型 | 既定値 | どのように選ぶか |
|---|---|---|---|
| `bounds.min_selected` | `int` | `0` | 必ず必要な候補数。不要なら0 |
| `bounds.max_selected` | `int` | `std::numeric_limits<int>::max()` | 使える枠数。通常は候補数以下 |
| `max_selected` という直接引数 | `int` | なし | 最大個数。0以上。候補数より大きくても有効 |
| `budget` という直接引数 | `long long` | なし | 費用配列と同じ単位の総予算。0以上 |

これらは解く問題の制約であり、探索のハイパーパラメータではない。集合被覆には個数・予算を渡す引数はなく、全被覆を条件として費用を小さくする。

### 5.5 探索時間・反復上限・乱数の設定

```cpp
struct coverage_solver_options {
    int time_limit_ms = 100;
    long long iteration_limit = std::numeric_limits<long long>::max();
    std::uint64_t seed = 1;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
};
```

#### `time_limit_ms`

- 目的: 1回のsolveに与える改善探索時間の目安
- 有効範囲: `-1` または0以上
- 既定値: `100`
- `-1`: 相対時間による停止を無効にする。絶対 `deadline` は引き続き有効
- `0`: 決定的初期解を作った後、原則としてランダム改善を開始しない

`time_limit_ms` はsoft limitである。相対時間だけを指定した場合、決定的初期解は必ず最後まで作る。また、時計確認を毎反復行わないため、1回の近傍処理と確認間隔の分だけ超過し得る。有限の絶対 `deadline` を併用した場合は、初期解途中でも後述の間隔で停止を確認する。

`coverage_solver` のコンストラクタで行う前処理と、呼び出し冒頭の部分問題条件の検証・view構築は、この相対時間に含まれない。呼び出し全体を制限したい場合は、外側で決めた絶対 `deadline` を併用する。

AtCoder Heuristic Contest（AHC）の部分solverとして使う場合は、全体の締切から安全余裕を引き、残り時間の一部を割り当てる。入力規模によるが、最初は20～200ms程度から実測して調整するとよい。

#### `iteration_limit`

- 目的: ランダム再始動、swap、LNSなどの試行量を制限する
- 有効範囲: 0以上
- 既定値: `std::numeric_limits<long long>::max()`
- `0`: ランダム再始動・swap・LNSを行わない

`iteration_limit = 0` でも、入力と初期解の検証、新規の決定的貪欲解、`improve_*` に渡した初期解の貪欲repairと比較、冗長候補の除去、選択数下限・exact Kの補充は行う。初期解をそのまま返す指定ではない。絶対deadlineによる貪欲構築の中断は別に適用される。

時間上限と反復上限の両方を指定した場合は、原則として先に到達した方で停止する。モードによって1 iterationが表す処理は異なるため、異なるモード間の `iterations` を探索量として直接比較しない。

予算付き最大被覆では、再始動の終了処理を数える都合で、返却される `iterations` が指定値を最大1だけ超える場合がある。厳密な反復回数制御が必要なテストでは、この点を考慮する。

#### `seed`

- 目的: ランダム化貪欲法、sampling、LNSの乱数系列を決める
- 有効範囲: `std::uint64_t` の全範囲
- 既定値: `1`

同じヘッダ・ビルド・実行環境で、`time_limit_ms = -1` かつ絶対deadlineが探索中に到来しない条件では、同じ入力、同じseed、同じ反復上限なら探索を再現できる。コンパイラや標準ライブラリが異なる環境間の完全一致は保証しない。時間上限で停止させる場合は、負荷によって反復数が変わるため、結果の完全再現は保証されない。

複数seedを試す場合は、同じ `coverage_solver` を再利用すると前処理を繰り返さずに済む。

#### `deadline`

- 目的: 外側solverと共有する絶対終了時刻
- 有効範囲: `std::chrono::steady_clock::time_point` の値
- 既定値: `std::chrono::steady_clock::time_point::max()`（無効）
- 停止時刻: `time_limit_ms` が有効なら、相対時間上限と `deadline` の早い方

有限の `deadline` を指定すると、初期貪欲法とLNS内のrepairの開始時、およびheapの取り出し256回ごとに絶対時刻を確認する。heap初期化の候補・対象列挙は途中停止しない。外側AHC solverでは次のように安全余裕を引いた共通締切を渡す。

`options.deadline = contest_start + std::chrono::milliseconds(1900)` のように設定する。ここで `contest_start` は外側で記録した `steady_clock` の開始時刻である。

ただし、`coverage_subproblem` の検証、差し替え重みからの利得和再計算、候補mask・希少度の構築、集合被覆の被覆可能性検査、exact Kの補充、メモリ確保は中断しない。絶対deadlineもhard real-time保証ではなく、入力規模に応じた安全余裕が必要である。

#### 推奨設定例

```cpp
// AHC本番向け: 時間で停止
const auto contest_start = std::chrono::steady_clock::now();
coverage_solver_options contest_options;
contest_options.time_limit_ms = 80;
contest_options.seed = 123456789;
contest_options.deadline = contest_start + std::chrono::milliseconds(1900);

// ベンチマーク向け: 反復数を固定
coverage_solver_options benchmark_options;
benchmark_options.time_limit_ms = -1;
benchmark_options.iteration_limit = 1000;
benchmark_options.seed = 1;

// 決定的な初期解だけを取得
coverage_solver_options greedy_only;
greedy_only.time_limit_ms = -1;
greedy_only.iteration_limit = 0;
```

`time_limit_ms = -1`、既定の `iteration_limit`、既定の無効deadlineを同時に使うと、早期終了条件に該当しない選択数制約・集合被覆の問題では実用上終了しない。固定候補だけで終わる特殊な問題の挙動を一般化しないこと。予算付き最大被覆には内部の再始動回数上限があるが、停止条件としてそれに依存すべきではない。相対時間を無効化するときは、どのモードでも有限の `iteration_limit` または絶対 `deadline` を設定する。

| 公開設定 | 型 | 既定値 | 値を選ぶ基準 |
|---|---|---|---|
| `time_limit_ms` | `int` | `100` | 呼び出せる時間に合わせる。入力変換・構築・出力の余裕を別に確保 |
| `iteration_limit` | `long long` | `std::numeric_limits<long long>::max()` | 再現テストでは有限値。0ならランダム改善なし |
| `seed` | `std::uint64_t` | `1` | 再現時は固定、品質比較時は複数値。数値の大小に品質上の意味はない |
| `deadline` | `std::chrono::steady_clock::time_point` | 同型の `max()` | 外側の処理と共通の絶対時刻。無効のままでもよい |

公開されている探索調整値はこの4項目だけである。ruin量、候補sampling数、ランダム化の強さなどは実装内の固定値であり、利用時の設定項目ではない。まず同じ入力群で時間と品質を測り、処理全体の予算に合わせて `time_limit_ms` を決める。

### 5.6 ユースケースによる準備の違い

| 用途 | 共通モデルを作るまでに必要な処理 |
|---|---|
| 施設・センサー | 距離や到達条件を判定し、候補ごとの対象番号一覧へ変換 |
| テスト・代表要素・ルート | 要件・特徴・仕事を対象番号へ対応付け、候補の対象一覧を入力 |
| 個数範囲・既存解改善 | 共通モデルに加えて個数上下限、予算、初期候補集合を用意 |
| 増設・故障・一部分の再構築 | solverを保持し、その回の固定・禁止・候補poolの配列を用意 |
| 複数seed・シナリオ | 共通の被覆関係を1回構築し、条件を変えて逐次呼び出す |
| 外部解評価 | solverと同じ候補番号で選択集合を用意し、評価条件を揃える |

### 5.7 最小の実行例

以下を `main.cpp` として保存すれば、そのまま実行できる。第8章には実用途の関数形例を載せる。

```cpp
#include "maximum_coverage_solver_v06.hpp"

#include <iostream>

int main() {
    coverage_problem problem;
    problem.target_weights = {10, 20, 30, 40};
    problem.candidate_costs = {3, 4, 2};
    problem.covered_targets = {
        {0, 1},    // 候補0
        {1, 2},    // 候補1
        {2, 3},    // 候補2
    };

    coverage_solver solver(problem);

    coverage_solver_options options;
    options.time_limit_ms = 50;
    options.seed = 12345;

    const coverage_solution solution =
        solver.solve_maximum_coverage(2, options);

    if (!solution.feasible) {
        std::cout << "infeasible\n";
        return 0;
    }

    std::cout << "covered weight = " << solution.covered_weight << '\n';
    for (int candidate : solution.selected_candidates) {
        std::cout << candidate << ' ';
    }
    std::cout << '\n';
}
```

ヘッダをincludeすると内蔵テストの `main` は定義されない。ヘッダそのものを直接コンパイルすると内蔵テストが有効になる。

## 6. ユースケースごとの使い方

全solve・improveメソッドは `coverage_solution` を返す。`options` と `subproblem` は省略可能で、いずれも第5章の既定値を使う。引数の順序は「問題の上限など、初期解があれば初期解、options、subproblem」である。部分問題だけを指定するときも、optionsの位置へ `{}` を渡す。

呼び出し前には、`coverage_problem problem` の `target_weights`、`candidate_costs`、`covered_targets` を第5章の型・条件で必ず設定する。恒久固定・禁止があれば同じ `problem` に指定し、`coverage_solver solver(problem)` で構築する。各節で説明する対象・候補の変換はこの構築より前に済ませる。構築済みsolverを受け取る関数例では、呼び出し側がこの準備を1回行い、solverを保持する。

次に `coverage_solver_options options` を用意し、必要なら時間・seed・有限の反復上限・絶対deadlineを設定する。今回だけの条件がある用途は、参照先配列を保持した `coverage_subproblem subproblem` も作る。8.1～8.6節、8.8～8.10節の入力変換型関数は、このモデル構築を関数内に含む。8.7・8.12・8.13節と8.11節のターン型関数は、構築済みsolverを受け取る。

初期解の `std::span<const int>` には `std::vector<int>` や `std::array<int, N>` を渡せる。波括弧の `{0, 2}` を直接受け取る `std::initializer_list<int>` overloadも用意されている。以下の宣言はクラスの公開APIの抜粋であり、利用側で再宣言する必要はない。

#### 全用途共通の返却値

```cpp
struct coverage_solution {
    std::vector<int> selected_candidates;
    long long covered_weight = 0;
    long long total_cost = 0;
    int covered_target_count = 0;
    int uncovered_target_count = 0;
    long long iterations = 0;
    bool feasible = false;
};
```

| フィールド | 内容 |
|---|---|
| `selected_candidates` | 選択された元の候補番号。昇順にソート済み |
| `covered_weight` | 選択候補の和集合が覆う対象重みの合計 |
| `total_cost` | 選択候補の費用合計 |
| `covered_target_count` | 1回以上被覆された対象数。重み0の対象も数える |
| `uncovered_target_count` | `target_count - covered_target_count` |
| `iterations` | 初期解構築後に数えた再始動、swap、LNSなどの反復数 |
| `feasible` | 呼び出した問題の制約を満たすか |

集合被覆の実行可能解では全対象が被覆されるため、`covered_weight` は全対象重みの合計になる。集合被覆で比較すべき主目的値は `total_cost` である。

最大被覆で `feasible == false` になる主な原因は、選択固定候補だけで候補数上限または予算を超えること、または選択数下限を満たすだけの利用可能候補がないことである。集合被覆では、選択禁止を考慮した後に被覆不能な対象がある場合に加え、有限の絶対deadlineが初期実行可能解の完成前に切れた場合もfalseになる。

不実行可能時も `selected_candidates` などに部分状態が入る場合がある。必ず `feasible` を先に確認する。

内部で解を比較するときの優先順位は次の通りである。

- 選択数制約: 被覆重みが大きい解、次に候補数が少ない解
- 予算制約: 被覆重みが大きい解、次に費用が小さい解、次に候補数が少ない解
- 集合被覆: 実行可能な解、次に費用が小さい解、次に候補数が少ない解

### 6.1 道路・通信グラフ上の施設配置

対象を `important_vertices` の順に連番化し、各施設候補から半径内にある対象番号を `covered_targets` へ入れる。重みは `importance`、費用に意味がなければ全1とする。

`solver.solve_maximum_coverage(max_facilities, options)` を呼ぶ。`max_facilities` は `int` の個数上限で、ちょうどその個数という意味ではない。下限を使う場合は `coverage_cardinality_bounds` overloadにする。

返却解の `feasible` を確認してから `selected_candidates` を施設候補へ戻す。8.1節では候補番号をそのままグラフ頂点番号にしているので、返却番号が設置頂点になる。`covered_weight` が覆えた重要度合計である。

8.1節のコードは全頂点を設置候補とし、各辺の長さを1とする。BFSによる候補ごとの到達範囲計算はsolveの前に実施され、探索時間設定の対象外である。

```cpp
coverage_solution solve_maximum_coverage(
    int max_selected,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;

coverage_solution solve_maximum_coverage(
    coverage_cardinality_bounds bounds,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;
```

### 6.2 距離と費用を持つセンサー配置

需要点を対象、設置可能地点を候補にする。候補ごとの半径判定で `covered_targets` を作り、需要点の `weight` と装置の `cost` を入力する。

`solver.solve_budgeted_maximum_coverage(budget, options)` を呼ぶ。`budget` は `long long` の0以上の総予算で、費用と単位を合わせる。

`feasible` がtrueなら `selected_candidates` を設置候補配列の添字として使う。`total_cost <= budget` を満たし、`covered_weight` が監視できる需要の価値となる。

8.2節では円の境界を被覆に含める。遮蔽物は扱わない。候補数の上限をこのメソッドへ追加することはできず、予算0でも費用0の候補は選ばれ得る。

```cpp
coverage_solution solve_budgeted_maximum_coverage(
    long long budget,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;
```

### 6.3 全要件を満たすテスト・検査の削減

要件を対象番号にし、`requirements_by_test[test]` にテストが検査する要件を入れる。`target_weights` は対象数ぶん全1とし、費用には実行時間などの加算可能な値を使う。

`solver.solve_minimum_set_cover(options)` を呼ぶ。このメソッドに予算・選択数上限の引数はない。

`feasible` がtrueのとき `selected_candidates` が実行するテスト番号である。費用は `total_cost`、全被覆の診断は `uncovered_target_count == 0` で確認する。全費用1なら `total_cost` はテスト数になる。

1つでも利用可能なテストが覆えない要件があればfalseになる。`target_weights[t] = 0` にしてもその要件を任意化できない。対象外にしたい要件は被覆モデルを作る段階で除く。

```cpp
coverage_solution solve_minimum_set_cover(
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;
```

### 6.4 特徴を広く含む代表要素の選択

`features_by_item[item]` に、その候補が含む特徴番号を指定する。`feature_weights` は特徴ごとの非負重み、費用は全1でよい。

`solver.solve_maximum_coverage(item_limit, options)` を呼ぶ。`item_limit` は `int` で、最大いくつの代表要素を採用できるかを表す。公開宣言は6.1節と共通である。

`selected_candidates` を元の文・商品・教材の配列へ対応付ける。`covered_weight` は特徴の和集合の価値であり、選択候補の個別得点の単純な合計ではない。

同一特徴の重複は得点に加算されない。代表性を表現できる特徴と重みを利用側で設計する。必ず一定個数が欲しい場合は6.6節の上下限指定を使う。

### 6.5 事前に作ったルート・作業案の組み合わせ

`jobs_by_route[route]` にルートが処理する仕事番号を指定する。全対象重みを1とし、`route_costs` を総費用にする。今回の問題全体で固定・禁止する案を `problem.forced_candidates` と `problem.forbidden_candidates` に入れる。

`solver.solve_minimum_set_cover(options)` を呼ぶ。8.5節の `committed_routes` と `unavailable_routes` は、この2つの恒久条件へ対応する。

`feasible` を確認して返却候補番号のルートを採用し、`total_cost` を費用として使う。仕事を1回以上覆う条件だけをsolverが検査する。

ルート内部の実行可能性、ルート同士の車両競合などは利用側で確認する。各仕事をちょうど1回とする集合分割問題の解を保証するものではない。

### 6.6 選択数の下限・上限と、ちょうどK個の選択

第5章のモデルと `coverage_cardinality_bounds{min_selected, max_selected}` を作る。両方の値をKにするとexact Kになる。

`solver.solve_maximum_coverage(bounds, options)` を呼ぶ。両値は `int` で `0 <= min_selected <= max_selected` が必要になる。

`feasible` がtrueなら返却候補数が指定範囲内にある。`selected_candidates.size()` を使って出力枠へ配置できる。`covered_weight` が主目的値であり、`total_cost` の最小化を保証しない。

利用可能候補が下限より少なければfalse。空の被覆集合や追加利得0の候補も、下限補充では選ばれ得る。予算とは併用できない。

### 6.7 既設候補を維持した増設・故障対応

被覆関係を持つsolverを保持し、`coverage_subproblem` の `fixed_candidates` に残す候補、`forbidden_candidates` に一時休止候補を設定する。各リストは `std::span<const int>` で、リスト内の候補番号は一意にする。

`solver.solve_budgeted_maximum_coverage(total_budget, options, subproblem)` を呼ぶ。8.7節の関数は構築済みsolverを受け取るため、状態が変わるたびに繰り返し呼べる。

`feasible` がtrueなら返却解には既設の固定候補も含まれる。増設候補だけ欲しいときは、`selected_candidates` から固定候補集合を除く。`total_cost` は既設分を含む。

固定費用だけで予算を超えた場合はfalse。同一候補の固定と禁止は例外。恒久禁止は解除できず、`fixed_candidates` に入れても使えない。

### 6.8 外部で作った最大被覆解の改善

既存解を元の候補番号の重複しない配列で用意する。固定候補は初期解に含めなくても自動追加される。今回の条件で禁止された候補は、初期解からも除かなければならない。

個数版は `solver.improve_maximum_coverage(bounds, initial_candidates, options, subproblem)`、予算版は `solver.improve_budgeted_maximum_coverage(budget, initial_candidates, options, subproblem)` を呼ぶ。個数版には直接 `int max_selected` を渡すoverloadもある。

問題が実行可能で有効な初期解を渡した場合、返却 `covered_weight` は、その回の固定候補とoverrideを反映した初期解より悪化しない。`feasible` を確認して返却候補集合を新しい解として使う。

固定候補追加後の個数上限超過・予算超過は初期解検証時に例外になる。個数の下限未満は許され、補充される。初期集合との近さや候補の維持は保証されない。8.8節に個数版と予算版の2関数を示す。

```cpp
coverage_solution improve_maximum_coverage(
    int max_selected,
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;

coverage_solution improve_maximum_coverage(
    int max_selected,
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;

coverage_solution improve_maximum_coverage(
    coverage_cardinality_bounds bounds,
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;

coverage_solution improve_maximum_coverage(
    coverage_cardinality_bounds bounds,
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;
```

```cpp
coverage_solution improve_budgeted_maximum_coverage(
    long long budget,
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;

coverage_solution improve_budgeted_maximum_coverage(
    long long budget,
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;
```

### 6.9 未完成の集合被覆解の修復

全対象と候補費用を設定し、現在選んでいる候補を `partial_candidates` として用意する。未被覆対象が残っていてもよいが、番号の重複・範囲外・禁止候補は許さない。

`solver.improve_minimum_set_cover(partial_candidates, options, subproblem)` を呼ぶ。候補を絶対に残したい場合は、初期解とは別に固定条件へも指定する。

`feasible` がtrueなら全対象被覆の完成解として採用できる。完成済みの初期解なら、その回の固定候補追加・費用override適用後の費用より `total_cost` は悪化しない。

未完成の初期解には費用非増加を保証しない。絶対deadlineが過ぎている場合、未完成解を完成できずfalseになることがあるが、有効な完成済み初期解は比較対象として保持する。

```cpp
coverage_solution improve_minimum_set_cover(
    std::span<const int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;

coverage_solution improve_minimum_set_cover(
    std::initializer_list<int> initial_candidates,
    const coverage_solver_options& options = {},
    const coverage_subproblem& subproblem = {}) const;
```

### 6.10 複数の乱数seedによる解の比較

1個のsolverを作り、試す `std::uint64_t` のseed列を用意する。各回の `options.seed` を変更し、同じ予算で解く。

`solver.solve_budgeted_maximum_coverage(budget, options)` を各seedで逐次呼ぶ。8.10節のwrapper引数 `time_limit_ms_per_seed` は0以上の `int`、`deadline` は共有する `steady_clock::time_point` である。

実行可能な解だけを比較し、被覆重みが大きい、同点なら費用が小さい、さらに同点なら候補数が少ないものを保存する。wrapperの戻り値は最良回の `coverage_solution` で、`iterations` は全回の合計ではない。

seed列が空ならwrapperは既定のfalse解を返し、solveは実行しない。wrapperは期限後も残りのseedを呼ぶため、回数を過大にしない。時間ではなく固定反復で比較したい場合は、自分のループで有限の `iteration_limit` を設定する。

### 6.11 需要・費用・予算が変わるシナリオ・ターンの反復

共通の被覆関係でsolverを1回作る。各シナリオの対象重みT個、候補費用C個を用意し、`target_weights_override` と `candidate_costs_override` に設定する。

`solver.solve_budgeted_maximum_coverage(scenario.budget, options, subproblem)` を逐次呼ぶ。8.11節のwrapperは各配列長を検証し、シナリオ順に実行する。

`std::vector<coverage_solution>` のi番目が入力シナリオiに対応する。各要素の `feasible` を確認して候補集合と費用を利用する。異なる重みの `covered_weight` を、同一の評価基準とみなして比較しない。

overrideは1回の呼び出しだけ有効で、次の回へ残らない。被覆関係や対象・候補の個数は変更できない。シナリオが空なら空vectorを返す。この一括シナリオ関数は、各回の解を自動で次の初期解にはしない。

ターン制では、同じ節の `solve_budgeted_turn(solver, budget, previous_candidates, options, subproblem)` を1ターンごとに呼ぶ。`previous_candidates` は同じsolverから返った候補番号のspanで、初回は空。今回の `fixed_candidates`・`forbidden_candidates`・poolを `subproblem` に指定し、重み・費用も変わるなら完全なoverride配列を設定する。

この関数は前回解を今回の利用可能候補へ絞り、`evaluate` によって固定追加後の現在費用を確認する。予算内なら `improve_budgeted_maximum_coverage`、予算超過なら `solve_budgeted_maximum_coverage` を呼び、`coverage_solution` を1個返す。採用した候補番号だけを次回へ保存し、評価値は各回の条件で扱う。固定・禁止の競合などの入力不正は例外であり、新規solveへの切替で隠さない。詳細な更新境界は第3章を参照する。

### 6.12 大きな探索の一部分だけを再構築

残す候補を `fixed_candidates`、試行中に使わない候補を `forbidden_candidates` に指定する。`restrict_to_candidate_pool = true` とし、選び直してよい候補を `candidate_pool` に入れる。

`solver.solve_maximum_coverage(coverage_cardinality_bounds{exact_k, exact_k}, options, subproblem)` を呼ぶ。8.12節はこれらの配列と `int exact_k`、時間、seed、共通deadline、反復上限を受け取る。

`feasible` がtrueなら `selected_candidates` が固定部分を含む完成案になる。外側で持つ評価や追加制約を確認してから採用する。falseなら元解を保持するなど、失敗時の扱いを利用側で決める。

固定候補はpool外でもよい。pool制限trueで空poolなら固定候補以外を選べない。固定候補の重複、禁止との競合は例外になる。wrapperは相対時間を無効にして反復・deadlineも実質無制限にする指定を拒否する。

### 6.13 別の処理が作った選択集合の評価

選択集合を元の候補番号で用意し、その選択時と同じ `coverage_subproblem` 条件を設定する。

`solver.evaluate(selected_candidates, subproblem)` を呼ぶ。戻り値の型は `coverage_evaluation` である。`target_count()` と `candidate_count()` は入力時の個数を返し、候補数は前処理で無効化した候補も含む。

`evaluation.selected_count` を個数上限・下限、`evaluation.total_cost` を予算、`evaluation.uncovered_target_count` を0と比較する。8.13節は上限・予算・全被覆を別々のboolとして返す。

この評価関数は候補番号・重複・禁止・poolを検証し、固定候補を自動追加するが、目的固有の `feasible` は返さない。個数と予算を同時に検査できることは、solveが両方を同時に制約できることを意味しない。

```cpp
coverage_evaluation evaluate(
    std::span<const int> selected_candidates,
    const coverage_subproblem& subproblem = {}) const;

coverage_evaluation evaluate(
    std::initializer_list<int> selected_candidates,
    const coverage_subproblem& subproblem = {}) const;
```

```cpp
int target_count() const;
```

```cpp
int candidate_count() const;
```

```cpp
struct coverage_evaluation {
    long long covered_weight = 0;
    long long total_cost = 0;
    int covered_target_count = 0;
    int uncovered_target_count = 0;
    int selected_count = 0;
};
```

`evaluate` の返却型である。`coverage_subproblem` の固定候補は自動追加され、禁止・pool・差し替え値もsolveと同じように解釈される。制約違反量や予算・選択数に対する `feasible` は判定しないため、呼び出し側で目的固有の上限と比較する。

## 7. 制約・注意点

### 7.1 直接は扱えない問題

| 問題構造 | 対応できない背景 | 代替案 |
|---|---|---|
| 候補数上限と予算上限の同時制約 | solverは資源を1種類だけ管理する | 外側で片方を調整するか、問題固有近傍を実装する |
| 複数種類の予算・容量 | 候補費用はスカラー1個 | 多次元制約対応solverを使う |
| 候補間の競合 | 選択可否は候補単体でしか持たない | 競合を外側でrepairする |
| 候補間の依存・先行条件 | 選択集合の組合せ制約を持たない | 依存関係対応の探索を使う |
| 対象を2回以上被覆する条件 | cover countは管理するが、目的は初回被覆だけ | multi-cover専用solverへ拡張する |
| 重複被覆による追加得点 | 被覆重みは対象ごとに1回だけ加算 | 被覆回数に応じた利得を扱うsolverを使う。対象を単に複製しても2回目の被覆価値は表せない |
| 候補と対象の容量付き割当 | 「誰が覆うか」を出力しない | 一般化割当・フローsolverを使う |
| 距離和・最大距離の最小化 | 被覆関係は0/1 | 距離閾値を外側で探索するか、k-median/k-centerを使う |
| 未被覆罰金付き集合被覆 | set coverは全対象被覆が必須 | Prize-Collecting版を別実装する |
| 負の重み・負の費用 | 入力検証で禁止される | 問題を非負へ変換できる場合だけ変換する |
| 動的な被覆関係更新 | solver構築時に内部CSRを固定する | 更新後にsolverを再構築する |

### 7.2 メソッドとパラメータの組み合わせ

- `solve_maximum_coverage` に予算制約を加えることはできない。
- `solve_budgeted_maximum_coverage` に候補数上限を加えることはできない。
- 集合被覆メソッドに候補数上限または予算上限を渡すことはできない。
- `coverage_cardinality_bounds` は選択数制約付き最大被覆でだけ使える。
- 選択数制約の `improve_maximum_coverage` は、固定候補を含めて上限以下の初期解を要求する。`bounds` 版の選択数下限は初期解が満たしていなくてもよく、最後に補充される。
- `improve_budgeted_maximum_coverage` は、固定候補を含めて予算内の初期解を要求する。
- `improve_minimum_set_cover` は、未被覆対象のある初期解を受け取りrepairできる。
- 固定候補は全メソッドで自動追加され、初期解や探索から除去できない。
- 固定候補と恒久・一時禁止候補は、集合が互いに素なら併用できる。同じ候補を固定と禁止の両方には指定できない。
- `restrict_to_candidate_pool == false` なら `candidate_pool` の内容は参照しない。
- したがって、`restrict_to_candidate_pool == false` のときは `candidate_pool` 内の範囲外番号や重複番号も検証されない。将来trueへ切り替える可能性があるなら、常に正規化しておく方が安全である。
- pool外でも固定候補は選択される。恒久禁止候補はpoolへ入れても復活しない。
- 重み・費用overrideは変更要素だけでなく、T個・C個の完全な配列を渡す。
- `coverage_subproblem` の各spanが参照する領域はメソッド終了まで有効に保つ。
- `time_limit_ms = -1` を使う場合は、有限の `iteration_limit` または `deadline` を組み合わせる。
- `seed` を固定しても、時間停止では完全な再現性を得られない。

同じ `coverage_solver` に対して複数のsolveメソッドを順番に呼ぶこと自体は可能である。各呼び出しは独立した探索状態を作り、前回解を自動継承しない。継承したい場合は `improve_*` へ明示的に渡す。

公開solve・improve・evaluateメソッドは `const` だが、部分問題のmask・重み和・希少度を保持する `mutable` な作業配列を再利用する。同じ `coverage_solver` に対するこれらのメソッドの並行呼び出しは行わないこと。逐次呼び出しでは前回の部分問題条件が次の呼び出しへ残ることはない。

### 7.3 境界値の挙動

以下は値域と候補リストの形式が有効な場合の挙動である。負のKや固定・禁止の競合など、入力自体が不正な場合の例外は7.8節を参照する。

| 入力 | 挙動 |
|---|---|
| `max_selected == 0` | 固定候補がなければ空解で実行可能。固定候補があれば実行不能 |
| `bounds == {K, K}` | 固定候補がK個以下で、固定候補と禁止されていないpool内候補（pool制限なしなら全候補）の和集合がK個以上ならちょうどK個。それ以外は実行不能 |
| `bounds.min_selected == 0` | 下限のない最大K個の指定と同じ |
| `budget == 0` | 費用0候補だけ選択可能。固定候補費用が正なら実行不能 |
| 対象数が0 | 集合被覆は固定候補だけで実行可能。最大被覆は固定候補制約を確認し、選択数下限があれば候補を補充 |
| 候補数が0 | 最大被覆は選択数下限が0なら空解、正なら実行不能。集合被覆は対象も0の場合だけ実行可能 |
| 対象重みが0 | 最大被覆では得点にならないが、選択数下限の補充には影響しない。集合被覆では被覆必須 |
| 候補費用が0 | 許容され、予算制約・集合被覆では正の利得があれば最優先。選択数制約のgreedyでは費用による優先はないが、前処理と下限補充には費用を使う |
| 空の被覆集合を持つ候補 | 通常探索では無効化。固定候補、明示した初期解、選択数下限・exact Kの補充では選ばれ得る |
| `iteration_limit == 0` | ランダム改善を行わない。初期解の検証・貪欲repair・比較と、下限補充は行う |
| 呼び出し前から絶対deadlineが過去 | 新規greedyは追加を始めない。最大被覆は制約内の空解・固定部分などを返し得る。集合被覆は全被覆が完成していなければfalse。有効な初期解との比較と選択数下限の補充は行う |

最大被覆では、決定的構築と有効な初期解との比較の後に固定候補以外が一つも残らなければ、ランダム改善を始めず `iterations == 0` で終了する。集合被覆も固定候補だけで全対象を覆える場合は同様に終了する。7.8節に記した即時実行不能の場合を除き、`improve_*` の初期解検証は先に行われる。選択数下限の補充は、実行可能な結果を返す前に別途行う。

ただし `iterations == 0` は最適性の証明ではない。反復上限0、相対時間0、絶対deadlineによる構築中断でも0になる。例えば、価値のある候補が残っていても、過去のdeadlineを指定した新規最大被覆は空解を返し得る。`feasible == true` は制約を満たすことを表し、探索の完了や被覆価値の高さは表さない。

### 7.4 初期解に関する注意

`initial_candidates` は元の候補番号で指定する。前処理で重複・支配候補として通常探索から無効化される候補でも、初期解として明示した場合は状態へ入ることがある。通常は、solverが返した候補集合または入力上意味のある候補だけを渡す。

固定候補は初期解へ書かなくても追加される。禁止候補は初期解へ入れられない。重複番号も許可されない。

初期解との品質比較は、固定候補を自動追加し、指定した重み・費用overrideを適用した後の値を基準にする。条件を差し替える前の費用や被覆重みとの比較ではない。

### 7.5 ヒューリスティック解であること

前処理の重複除去と包含支配除去は最適値を壊さない条件で行うが、その後の探索はヒューリスティックである。次の保証はない。

- 最大被覆で真の最大重みを得る保証
- 集合被覆で真の最小費用を得る保証
- seed間で同じ品質になる保証
- 時間を増やすと必ず目的値が改善する保証

同一呼び出し中に保持する実行可能解の主目的値は悪化させない。主目的は最大被覆なら被覆重み、集合被覆なら費用である。候補集合の一致や、主目的に使っていない費用などの改善までは保証しない。小規模部分問題で厳密性が必要なら、bit DP、全探索、整数計画などを別途使う。

### 7.6 オーバーフローと入力規模

探索中の状態や初期解を含め、次の合計が `long long` の範囲内に収まることは利用側で保証する。総和の符号付きオーバーフロー検査は行わない。

- 被覆対象の重み和
- 選択候補の費用和
- 各候補の被覆重み和

すべて非負なので、全対象の重み総和と全候補の費用総和がそれぞれ `long long` に収まるようにすれば、これらの部分和も範囲内になる。overrideを渡す場合は差し替え後の値にも同じ前提が必要である。

対象数と候補数、および重複除去後の被覆関係総数は `int` の範囲に制限される。被覆関係が密で関係数の多い場合は、内部CSRだけでなく、コンストラクタ中の正規化用コピーによる一時メモリも必要になる。

### 7.7 時間管理

`time_limit_ms` と `deadline` はどちらもsoft limitで、hard real-timeの停止を保証しない。

- 相対 `time_limit_ms` にsolver構築と呼び出し冒頭の部分問題prepareは含まれない。絶対 `deadline` は外側で決めた時刻なので、その間にも近づくが、これらの処理を途中停止する機能はない。
- 相対 `time_limit_ms` だけでは決定的初期解の途中で停止しない。
- 有限の絶対 `deadline` は初期貪欲法とLNS repairでも確認する。
- 部分問題viewの検証・再計算、集合被覆の事前検査、heap初期化、冗長除去、exact Kの補充、メモリ確保は途中停止しない。
- 時計確認間隔の分だけ超過し得る。
- 1回の近傍評価が重い場合、その処理時間分だけ超過し得る。

AHCでは外側でも時計を管理し、出力生成と安全余裕を残す。非常に大きな問題では、solver構築と初期解生成を個別に計測する。

### 7.8 例外一覧

主な `std::invalid_argument` 条件は次の通りである。

- `candidate_costs` と `covered_targets` の要素数不一致
- 対象数、候補数、または重複除去後の被覆関係総数が `int` の範囲を超える
- 負の対象重みまたは候補費用
- 範囲外の対象番号、固定候補番号、禁止候補番号
- 同一候補の固定・禁止
- `0 <= min_selected <= max_selected` を満たさない選択数範囲、または負の `budget`
- `time_limit_ms < -1`
- 負の `iteration_limit`
- 部分問題の固定・禁止にある範囲外番号または重複番号
- `restrict_to_candidate_pool == true` のとき、poolにある範囲外番号または重複番号
- 部分問題で固定候補と禁止候補を競合させること
- overrideの要素数不一致、負のoverride値
- 初期解の範囲外候補、重複候補、禁止候補
- 選択数制約付き最大被覆の初期解が候補数上限を超える、または予算付き最大被覆の初期解が予算を超える

`improve_*` の初期解検証は、呼び出し条件だけで分かる即時の実行可能性判定より後に行われる。固定候補だけで選択数上限・予算を超える場合、利用可能候補数が選択数下限未満の場合、または集合被覆に被覆不能な対象がある場合は、初期解を検証せず `feasible == false` を返すことがある。初期解の妥当性検査だけを目的に `improve_*` を呼ばないこと。

入力を外部データから作る場合は、solver呼び出し前にサイズと添字を検証するか、例外を捕捉する。

### 7.9 AHCでの利用チェックリスト

1. 問題を「候補を選ぶと対象集合を1回被覆する」形へ変換できるか確認する。
2. 候補間制約や複数容量が目的上重要でないか確認する。
3. 選択数、予算、全被覆のどれを制約にするか決める。
4. 費用に意味がない場合も `candidate_costs` を全1で用意する。
5. 集合被覆だけなら `target_weights` を全1で用意する。
6. 被覆関係が同じ部分問題は `coverage_subproblem` で表し、solverを再構築しない。
7. spanの参照元をsolve終了まで保持する。
8. `feasible` を必ず確認し、必要なら `evaluate` で外側評価と照合する。
9. 固定反復でseedごとの品質を比較してから、本番の時間配分を決める。
10. 外側deadlineから出力用余裕を引き、同じ絶対時刻を各部分solverへ渡す。

## 8. ユースケースごとのコード例

各節のC++ブロックは、必要なincludeと、その節で必要な型・関数をまとめたコピー単位である。未定義の補助関数やグローバル変数は使わない。利用する節を1つの `.cpp` にコピーし、呼び出し側の `main` から実行する。複数の翻訳単位で同じ関数定義を重複させないこと。

全例の入力コメントは、候補・対象番号の対応、配列長、単位、空配列の意味を示す。戻り値が `coverage_solution` の関数では、まず `feasible` を確認し、その後に選択候補と目的値を使う。入力検証で不正を検出した場合は `std::invalid_argument` となる。実行不能の早期判定や、空のseed列・シナリオ列などでsolveを呼ばない場合には、検査されない条件もある。総和が整数範囲に収まることなど、第7章の前提は各関数でも必要になる。

全例をまとめた [maximum_coverage_solver_examples_v06.cpp](maximum_coverage_solver_examples_v06.cpp) には実行テスト用のmainも含む。関数群だけを別ソースからincludeした場合は、そのmainは有効にならない。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror maximum_coverage_solver_examples_v06.cpp -o examples
./examples
```

### 8.1 道路・通信グラフ上の施設配置

入力モデルは4.1節、メソッドと注意点は6.1節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleを確認し、selected_candidatesのグラフ頂点へ施設を置く。covered_weightは重要度合計。
coverage_solution solve_graph_facility_coverage(
    // 入力: 0始まりの隣接リスト。辺長は1。有向グラフなら到達方向をそのまま使う。
    const std::vector<std::vector<int>>& graph,
    // 入力: 評価対象とする頂点番号。一意に指定。この順番が対象番号になる。
    const std::vector<int>& important_vertices,
    // 入力: important_verticesと同じ長さの非負重要度。
    const std::vector<long long>& importance,
    // 入力: 到達可能とみなす最大辺数。0以上。
    int radius,
    // 入力: 設置数の上限。0以上。すべてのグラフ頂点が候補。
    int max_facilities,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    if (graph.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("頂点数がintの範囲を超えている");
    }
    if (important_vertices.size() != importance.size()) {
        throw std::invalid_argument("重要頂点数と重み数が異なる");
    }
    if (important_vertices.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("重要頂点数がintの範囲を超えている");
    }
    if (radius < 0) throw std::invalid_argument("radiusは0以上");

    const int n = static_cast<int>(graph.size());
    std::vector<int> target_id(n, -1);
    for (std::size_t target = 0; target < important_vertices.size(); ++target) {
        const int vertex = important_vertices[target];
        if (vertex < 0 || vertex >= n) {
            throw std::invalid_argument("重要頂点が範囲外");
        }
        if (target_id[vertex] != -1) {
            throw std::invalid_argument("重要頂点が重複している");
        }
        target_id[vertex] = static_cast<int>(target);
    }
    for (const auto& edges : graph) {
        for (int next : edges) {
            if (next < 0 || next >= n) {
                throw std::invalid_argument("辺の終点が範囲外");
            }
        }
    }

    coverage_problem problem;
    problem.target_weights = importance;
    problem.candidate_costs.assign(graph.size(), 1);
    problem.covered_targets.resize(graph.size());

    std::vector<int> distance(n);
    for (int candidate = 0; candidate < n; ++candidate) {
        std::fill(distance.begin(), distance.end(), -1);
        std::queue<int> queue;
        distance[candidate] = 0;
        queue.push(candidate);

        while (!queue.empty()) {
            const int vertex = queue.front();
            queue.pop();
            if (target_id[vertex] >= 0) {
                problem.covered_targets[candidate].push_back(target_id[vertex]);
            }
            if (distance[vertex] == radius) continue;
            for (int next : graph[vertex]) {
                if (distance[next] != -1) continue;
                distance[next] = distance[vertex] + 1;
                queue.push(next);
            }
        }
    }

    coverage_solver solver(problem);
    return solver.solve_maximum_coverage(max_facilities, options);
}
```

### 8.2 距離と費用を持つセンサー配置

入力モデルは4.2節、メソッドと注意点は6.2節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

struct weighted_demand_point {
    long long x;  // 入力: 同じ単位で表すx座標。long longの範囲。
    long long y;  // 入力: 同じ単位で表すy座標。long longの範囲。
    long long weight;  // 入力: この需要点の非負重要度。
};

struct sensor_site {
    long long x;  // 入力: 同じ単位で表すx座標。long longの範囲。
    long long y;  // 入力: 同じ単位で表すy座標。long longの範囲。
    long long radius;  // 入力: 座標と同じ単位の半径。0以上。円周も被覆に含む。
    long long cost;  // 入力: 予算と同じ単位の設置費。0以上。
};

// 出力: feasibleを確認し、selected_candidatesをsitesの添字として使う。total_costは設置費合計。
coverage_solution solve_budgeted_sensor_placement(
    // 入力: 需要点の配列。この添字が対象番号。
    const std::vector<weighted_demand_point>& demands,
    // 入力: 設置候補の配列。この添字が返却候補番号。
    const std::vector<sensor_site>& sites,
    // 入力: sites[i].costと同じ単位の総予算。0以上。
    long long budget,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    if (demands.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("需要点数がintの範囲を超えている");
    }

    coverage_problem problem;
    problem.target_weights.reserve(demands.size());
    for (const auto& demand : demands) {
        problem.target_weights.push_back(demand.weight);
    }
    problem.candidate_costs.reserve(sites.size());
    problem.covered_targets.resize(sites.size());

    for (std::size_t candidate = 0; candidate < sites.size(); ++candidate) {
        const sensor_site& site = sites[candidate];
        if (site.radius < 0) {
            throw std::invalid_argument("監視半径は0以上");
        }
        problem.candidate_costs.push_back(site.cost);

        const __int128 radius_value = site.radius;
        const __int128 radius_squared = radius_value * radius_value;
        for (std::size_t target = 0; target < demands.size(); ++target) {
            __int128 dx =
                static_cast<__int128>(site.x) - demands[target].x;
            __int128 dy =
                static_cast<__int128>(site.y) - demands[target].y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx > radius_value || dy > radius_value) continue;
            if (dx * dx + dy * dy <= radius_squared) {
                problem.covered_targets[candidate].push_back(
                    static_cast<int>(target));
            }
        }
    }

    coverage_solver solver(problem);
    return solver.solve_budgeted_maximum_coverage(budget, options);
}
```

### 8.3 全要件を満たすテスト・検査の削減

入力モデルは4.3節、メソッドと注意点は6.3節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleならselected_candidatesのテストを実行する。total_costは合計実行費。
coverage_solution minimize_test_suite(
    // 入力: 全要件数T。0以上。要件番号は0..T-1。
    int requirement_count,
    // 入力: [test]に、そのテストが覆う要件番号を列挙。
    const std::vector<std::vector<int>>& requirements_by_test,
    // 入力: テスト数と同じ長さの非負費用。全1ならテスト数を最小化。
    const std::vector<long long>& execution_costs,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    if (requirement_count < 0) {
        throw std::invalid_argument("要件数は0以上");
    }

    coverage_problem problem;
    problem.target_weights.assign(requirement_count, 1);
    problem.candidate_costs = execution_costs;
    problem.covered_targets = requirements_by_test;

    coverage_solver solver(problem);
    return solver.solve_minimum_set_cover(options);
}
```

### 8.4 特徴を広く含む代表要素の選択

入力モデルは4.4節、メソッドと注意点は6.4節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleならselected_candidatesが代表item番号。covered_weightは特徴の和集合の価値。
coverage_solution select_representative_items(
    // 入力: 特徴数T個の非負重要度。特徴番号は0..T-1。
    const std::vector<long long>& feature_weights,
    // 入力: [item]に、その候補が含む特徴番号を列挙。
    const std::vector<std::vector<int>>& features_by_item,
    // 入力: 選ぶ代表要素の最大個数。0以上。
    int item_limit,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    coverage_problem problem;
    problem.target_weights = feature_weights;
    problem.candidate_costs.assign(features_by_item.size(), 1);
    problem.covered_targets = features_by_item;

    coverage_solver solver(problem);
    return solver.solve_maximum_coverage(item_limit, options);
}
```

### 8.5 事前に作ったルート・作業案の組み合わせ

入力モデルは4.5節、メソッドと注意点は6.5節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleならselected_candidatesが採用ルート番号。固定ルートも含む。
coverage_solution select_routes_covering_all_jobs(
    // 入力: 全仕事数T。0以上。仕事番号は0..T-1。
    int job_count,
    // 入力: [route]に、そのルートが実施する仕事番号を列挙。
    const std::vector<std::vector<int>>& jobs_by_route,
    // 入力: ルート数と同じ長さの非負費用。
    const std::vector<long long>& route_costs,
    // 入力: 必ず採用するルート番号。不要なら空。
    const std::vector<int>& committed_routes,
    // 入力: 採用禁止のルート番号。固定候補と競合不可。不要なら空。
    const std::vector<int>& unavailable_routes,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    if (job_count < 0) throw std::invalid_argument("仕事数は0以上");

    coverage_problem problem;
    problem.target_weights.assign(job_count, 1);
    problem.candidate_costs = route_costs;
    problem.covered_targets = jobs_by_route;
    problem.forced_candidates = committed_routes;
    problem.forbidden_candidates = unavailable_routes;

    coverage_solver solver(problem);
    return solver.solve_minimum_set_cover(options);
}
```

### 8.6 選択数の下限・上限と、ちょうどK個の選択

入力モデルは4.6節、メソッドと注意点は6.6節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleなら候補数が指定範囲内。候補IDは入力時の番号。費用最小化は保証しない。
coverage_solution solve_coverage_with_count_bounds(
    // 入力: 対象数T個の非負重要度。
    const std::vector<long long>& target_weights,
    // 入力: 候補数C個の非負費用。費用の意味がなければ全1。
    const std::vector<long long>& candidate_costs,
    // 入力: C個の対象一覧。[c]に候補cが覆う0..T-1の対象番号を列挙。
    const std::vector<std::vector<int>>& covered_targets,
    // 入力: 選択数下限。0以上。exact Kなら上下限ともK。
    int min_selected,
    // 入力: 選択数上限。下限指定がある場合は下限以上。
    int max_selected,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    coverage_problem problem;
    problem.target_weights = target_weights;
    problem.candidate_costs = candidate_costs;
    problem.covered_targets = covered_targets;

    coverage_solver solver(problem);
    const coverage_cardinality_bounds bounds{min_selected, max_selected};
    return solver.solve_maximum_coverage(bounds, options);
}
```

### 8.7 既設候補を維持した増設・故障対応

入力モデルは4.7節、メソッドと注意点は6.7節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleなら既設固定候補を含む完成集合。増設分だけ欲しければ固定集合との差を取る。
coverage_solution solve_operational_budget_coverage(
    // 入力: 同じ被覆関係で構築済みのsolver。同一インスタンスへ逐次呼び出す。
    const coverage_solver& solver,
    // 入力: 固定候補の費用を含む総予算。0以上。
    long long total_budget,
    // 入力: 今回必ず残す候補番号。一意。なければ空span。
    std::span<const int> retained_candidates,
    // 入力: 今回使えない候補番号。一意。固定と競合不可。なければ空span。
    std::span<const int> unavailable_candidates,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    coverage_subproblem subproblem;
    subproblem.fixed_candidates = retained_candidates;
    subproblem.forbidden_candidates = unavailable_candidates;
    return solver.solve_budgeted_maximum_coverage(
        total_budget, options, subproblem);
}
```

### 8.8 外部で作った最大被覆解の改善

入力モデルは4.8節、メソッドと注意点は6.8節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleを確認して候補集合を置き換える。有効初期解のcovered_weightを悪化させない。
coverage_solution improve_existing_budgeted_solution(
    // 入力: 対象数T個の非負重要度。
    const std::vector<long long>& target_weights,
    // 入力: 候補数C個の非負費用。費用の意味がなければ全1。
    const std::vector<long long>& candidate_costs,
    // 入力: C個の対象一覧。[c]に候補cが覆う0..T-1の対象番号を列挙。
    const std::vector<std::vector<int>>& covered_targets,
    // 入力: candidate_costs等と同じ単位の総予算。0以上。
    long long budget,
    // 入力: 初期候補番号。一意。予算以内であること。
    const std::vector<int>& initial_candidates,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    coverage_problem problem;
    problem.target_weights = target_weights;
    problem.candidate_costs = candidate_costs;
    problem.covered_targets = covered_targets;

    coverage_solver solver(problem);
    return solver.improve_budgeted_maximum_coverage(
        budget, initial_candidates, options);
}

// 出力: feasibleならboundsを満たす候補集合。初期解に下限不足があっても補充する。
coverage_solution improve_existing_count_bounded_solution(
    // 入力: 第5章の完全な問題データ。固定・禁止もここで指定できる。
    const coverage_problem& problem,
    // 入力: 0 <= min_selected <= max_selected。初期解は下限未満でも可。
    coverage_cardinality_bounds bounds,
    // 入力: 初期候補番号。一意。固定追加後にbounds.max_selectedを超えないこと。
    std::span<const int> initial_candidates,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    coverage_solver solver(problem);
    return solver.improve_maximum_coverage(bounds, initial_candidates, options);
}
```

### 8.9 未完成の集合被覆解の修復

入力モデルは4.9節、メソッドと注意点は6.9節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleなら全対象被覆の完成集合。未完成入力よりtotal_costが増える場合がある。
coverage_solution repair_incomplete_set_cover(
    // 入力: 対象数T。0以上。
    int target_count,
    // 入力: 候補数C個の非負費用。費用の意味がなければ全1。
    const std::vector<long long>& candidate_costs,
    // 入力: C個の対象一覧。[c]に候補cが覆う0..T-1の対象番号を列挙。
    const std::vector<std::vector<int>>& covered_targets,
    // 入力: 初期候補番号。一意。未被覆があってよく、空も可。
    const std::vector<int>& partial_candidates,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    if (target_count < 0) throw std::invalid_argument("対象数は0以上");

    coverage_problem problem;
    problem.target_weights.assign(target_count, 1);
    problem.candidate_costs = candidate_costs;
    problem.covered_targets = covered_targets;

    coverage_solver solver(problem);
    return solver.improve_minimum_set_cover(partial_candidates, options);
}
```

### 8.10 複数の乱数seedによる解の比較

入力モデルは4.10節、メソッドと注意点は6.10節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: 実行可能な中の最良解。iterationsは採用回の値。seed列が空ならfeasible=false。
coverage_solution solve_budgeted_coverage_multi_seed(
    // 入力: 第5章の完全な問題データ。固定・禁止もここで指定できる。
    const coverage_problem& problem,
    // 入力: candidate_costs等と同じ単位の総予算。0以上。
    long long budget,
    // 入力: 試すuint64_tのseed列。空ならsolveせずfalse解を返す。
    std::span<const std::uint64_t> seeds,
    // 入力: 1回あたりの相対時間ms。0以上。このwrapperでは-1不可。
    int time_limit_ms_per_seed,
    // 入力: 全回で共有する絶対締切。省略するとmax()で無効。
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max()) {
    if (time_limit_ms_per_seed < 0) {
        throw std::invalid_argument("seedごとの時間は0以上");
    }
    coverage_solver solver(problem);
    coverage_solution best;

    const auto better = [](const coverage_solution& lhs,
                           const coverage_solution& rhs) {
        if (lhs.covered_weight != rhs.covered_weight) {
            return lhs.covered_weight > rhs.covered_weight;
        }
        if (lhs.total_cost != rhs.total_cost) {
            return lhs.total_cost < rhs.total_cost;
        }
        return lhs.selected_candidates.size() <
               rhs.selected_candidates.size();
    };

    for (std::uint64_t seed : seeds) {
        coverage_solver_options options;
        options.time_limit_ms = time_limit_ms_per_seed;
        options.seed = seed;
        options.deadline = deadline;

        coverage_solution current =
            solver.solve_budgeted_maximum_coverage(budget, options);
        if (!current.feasible) continue;
        if (!best.feasible || better(current, best)) {
            best = std::move(current);
        }
    }
    return best;
}
```

### 8.11 需要・費用・予算が変わるシナリオ・ターンの反復

入力モデルは4.11節、メソッドと注意点は6.11節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

struct budgeted_coverage_scenario {
    std::vector<long long> target_weights;  // 入力: T個の非負重み。全0でもT個用意。
    std::vector<long long> candidate_costs;  // 入力: C個の非負費用。
    long long budget;  // 入力: このシナリオの総予算。0以上。
};

// 出力: 入力シナリオ順の解vector。各要素のfeasibleを確認してから利用する。
std::vector<coverage_solution> solve_repeated_budget_scenarios(
    // 入力: 対象数T。0以上。
    int target_count,
    // 入力: C個の対象一覧。[c]に候補cが覆う0..T-1の対象番号を列挙。
    const std::vector<std::vector<int>>& covered_targets,
    // 入力: シナリオ列。各回の全重みT個・全費用C個・予算を指定。空なら結果も空。
    const std::vector<budgeted_coverage_scenario>& scenarios,
    // 入力: 第5章の探索設定。省略時は100ms、seed=1、反復上限と絶対締切は実質無効。
    const coverage_solver_options& options = {}) {
    if (target_count < 0) throw std::invalid_argument("対象数は0以上");

    coverage_problem base_problem;
    base_problem.target_weights.assign(target_count, 0);
    base_problem.candidate_costs.assign(covered_targets.size(), 0);
    base_problem.covered_targets = covered_targets;
    coverage_solver solver(base_problem);

    std::vector<coverage_solution> answers;
    answers.reserve(scenarios.size());
    for (const auto& scenario : scenarios) {
        if (scenario.target_weights.size() !=
            static_cast<std::size_t>(target_count)) {
            throw std::invalid_argument("scenarioの対象重み数が異なる");
        }
        if (scenario.candidate_costs.size() != covered_targets.size()) {
            throw std::invalid_argument("scenarioの候補費用数が異なる");
        }
        coverage_subproblem subproblem;
        subproblem.target_weights_override = scenario.target_weights;
        subproblem.candidate_costs_override = scenario.candidate_costs;
        answers.push_back(solver.solve_budgeted_maximum_coverage(
            scenario.budget, options, subproblem));
    }
    return answers;
}
```

同じ用途を、入力が1ターンずつ到着する対話型処理で使う場合は次の関数を使える。`coverage_problem` を第5章のとおり用意し、ループの外で `coverage_solver solver(problem)` と構築する。各ターンでは今回の `coverage_subproblem` と `options` を用意し、`solve_budgeted_turn(solver, budget, previous_candidates, options, subproblem)` を呼ぶ。返却解が実行可能なら、その `selected_candidates` を前回解用vectorへコピーまたはmoveして次ターンへ渡す。falseなら条件に応じた失敗処理を行い、無効になった前回解を無条件で採用しない。

最初の関数はシナリオを独立に解く。次の関数は前回解を候補として引き継ぐ点が異なる。下のブロックも単独でコピーできる。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleなら今回の条件を満たす解。selected_candidatesを次ターン用に保存する。
// 旧解が今回の予算を超える場合は新規に解くため、旧解との品質非悪化は保証しない。
coverage_solution solve_budgeted_turn(
    // 入力: ターンループの外で構築したsolver。被覆関係・候補番号の意味を変えない。
    const coverage_solver& solver,
    // 入力: 今回の総予算。現在の候補費用と同じ単位。固定分も含める。
    long long budget,
    // 入力: 同じsolverで得た前回解の候補番号。一意。初回は空spanを渡す。
    std::span<const int> previous_candidates,
    // 入力: 第5章の探索設定。ターン全体の締切を使う場合はdeadlineを明示する。
    const coverage_solver_options& options = {},
    // 入力: 今回の固定・禁止・pool・重み・費用。overrideは全要素を渡す。
    // 空overrideは構築時の値を使い、前ターンのoverrideを引き継がない。
    const coverage_subproblem& subproblem = {}) {
    if (budget < 0) throw std::invalid_argument("予算は0以上");
    if (previous_candidates.empty()) {
        return solver.solve_budgeted_maximum_coverage(budget, options, subproblem);
    }

    const int candidate_count = solver.candidate_count();
    const auto check_id = [&](int candidate) {
        if (candidate < 0 || candidate >= candidate_count) {
            throw std::invalid_argument("候補番号が範囲外");
        }
    };
    std::vector<unsigned char> allowed(
        candidate_count, subproblem.restrict_to_candidate_pool ? 0 : 1);
    if (subproblem.restrict_to_candidate_pool) {
        for (int candidate : subproblem.candidate_pool) {
            check_id(candidate);
            allowed[candidate] = 1;
        }
    }
    for (int candidate : subproblem.fixed_candidates) {
        check_id(candidate);
        allowed[candidate] = 1;  // 今回の固定候補はpool外でも有効。
    }
    for (int candidate : subproblem.forbidden_candidates) {
        check_id(candidate);
        allowed[candidate] = 0;
    }

    std::vector<unsigned char> seen(candidate_count, 0);
    std::vector<int> initial_candidates;
    initial_candidates.reserve(previous_candidates.size());
    for (int candidate : previous_candidates) {
        check_id(candidate);
        if (seen[candidate]) throw std::invalid_argument("前回解の候補が重複");
        seen[candidate] = 1;
        if (allowed[candidate]) initial_candidates.push_back(candidate);
    }

    // 恒久・一時固定はevaluateで自動追加される。恒久固定をpoolで落としても失われない。
    // subproblem自体の重複・競合・override不正はここで例外になる。
    const auto current_evaluation = solver.evaluate(initial_candidates, subproblem);
    if (current_evaluation.total_cost > budget) {
        return solver.solve_budgeted_maximum_coverage(budget, options, subproblem);
    }
    return solver.improve_budgeted_maximum_coverage(
        budget, initial_candidates, options, subproblem);
}

```

前回解の範囲外番号・重複は、候補対応の不具合として例外にする。今回の禁止・poolで使えなくなった候補だけを除く。恒久禁止は解除せず、恒久固定は自動追加される。費用超過時は安さだけで旧解を削る処理を入れず、新規solveへ切り替える。`evaluate` と再実行の分だけ準備・状態構築を繰り返すため、変更数に比例する高速更新の例ではない。空の前回解は直接新規solveへ渡す。

### 8.12 大きな探索の一部分だけを再構築

入力モデルは4.12節、メソッドと注意点は6.12節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

// 出力: feasibleなら固定部分を含むexact_k個の案。falseなら外側で元解を保持する等の処理を行う。
coverage_solution repair_exact_k_coverage_neighborhood(
    // 入力: 同じ被覆関係で構築済みのsolver。同一インスタンスへ逐次呼び出す。
    const coverage_solver& solver,
    // 入力: 今回変更しない候補番号。一意。pool外でもよい。
    std::span<const int> kept_candidates,
    // 入力: 今回選ばない候補番号。一意。固定と競合不可。
    std::span<const int> temporarily_forbidden,
    // 入力: 固定候補以外に選んでよい候補番号。一意。空なら追加選択不可。
    std::span<const int> repair_pool,
    // 入力: 完成時の選択数。0以上。固定部分もこの個数に含める。
    int exact_k,
    // 入力: 相対時間ms。-1なら無効にし、有限の反復上限か絶対締切を渡す。
    int time_limit_ms,
    // 入力: 今回の乱数seed。uint64_tの任意の値。
    std::uint64_t seed,
    // 入力: 外側と共有するsteady_clock絶対締切。max()なら無効。
    std::chrono::steady_clock::time_point global_deadline,
    // 入力: 0以上の改善試行上限。既定はlong long最大値。
    long long iteration_limit = std::numeric_limits<long long>::max()) {
    if (time_limit_ms < -1) {
        throw std::invalid_argument("時間上限は-1または0以上");
    }
    if (time_limit_ms == -1 &&
        global_deadline == std::chrono::steady_clock::time_point::max() &&
        iteration_limit == std::numeric_limits<long long>::max()) {
        throw std::invalid_argument("有限の停止条件が必要");
    }
    coverage_subproblem subproblem;
    subproblem.fixed_candidates = kept_candidates;
    subproblem.forbidden_candidates = temporarily_forbidden;
    subproblem.restrict_to_candidate_pool = true;
    subproblem.candidate_pool = repair_pool;

    coverage_solver_options options;
    options.time_limit_ms = time_limit_ms;
    options.seed = seed;
    options.deadline = global_deadline;
    options.iteration_limit = iteration_limit;

    const coverage_cardinality_bounds bounds{exact_k, exact_k};
    return solver.solve_maximum_coverage(bounds, options, subproblem);
}
```

### 8.13 別の処理が作った選択集合の評価

入力モデルは4.13節、メソッドと注意点は6.13節を参照する。

```cpp
#include "maximum_coverage_solver_v06.hpp"

struct checked_coverage_solution {
    coverage_evaluation evaluation;  // 出力: 固定候補も加えた重み・費用・個数。
    bool respects_max_selected;  // 出力: 選択数が指定上限以下か。
    bool respects_budget;  // 出力: 費用が指定予算以下か。
    bool covers_all_targets;  // 出力: 未被覆対象が0か。重み0の対象も検査。
};

// 出力: 個数・予算・全被覆の判定を別々に返す。探索・修復は行わない。
checked_coverage_solution check_external_coverage_solution(
    // 入力: 同じ被覆関係で構築済みのsolver。同一インスタンスへ逐次呼び出す。
    const coverage_solver& solver,
    // 入力: 評価する候補番号。一意。禁止候補は含めない。
    std::span<const int> selected_candidates,
    // 入力: 検査する選択数上限。0以上。この関数では下限は検査しない。
    int max_selected,
    // 入力: candidate_costs等と同じ単位の総予算。0以上。
    long long budget,
    // 入力: 選択時と同じ固定・禁止・pool・override。省略時は追加条件なし。
    const coverage_subproblem& subproblem = {}) {
    if (max_selected < 0) {
        throw std::invalid_argument("選択数上限は0以上");
    }
    if (budget < 0) throw std::invalid_argument("予算は0以上");

    const coverage_evaluation evaluation =
        solver.evaluate(selected_candidates, subproblem);
    return {
        evaluation,
        evaluation.selected_count <= max_selected,
        evaluation.total_cost <= budget,
        evaluation.uncovered_target_count == 0,
    };
}
```

## 9. 実装

### 9.1 アルゴリズムの概要と全体の流れ

内部表現を1回構築し、呼び出しごとの条件を合成した後、貪欲法で解を作り、制限時間・反復上限の範囲で改善する。探索はヒューリスティックであり、分枝限定法による最適性証明は行わない。

1. コンストラクタで入力を検証し、被覆集合を正規化する。候補から対象、対象から候補の両方向の参照表を作り、重複・包含関係によって通常探索で使わない候補を決める。
2. solve・improve・evaluateの呼び出しで、固定・禁止・候補pool・重み・費用をその回の条件に合成する。被覆関係自体は共有する。
3. solve・improveでは、個数・費用の上限や全対象の被覆可能性を検査し、固定候補を含む状態を用意する。集合被覆の被覆可能性検査は、通常の状態構築より先に行う。
4. 決定的なlazy greedyで候補を追加する。improveなら、検証した初期解も補充・整理して比較する。
5. 選択数制約はランダム再始動と候補の一部を除いて埋め直すLNS、予算制約はランダム再始動と1候補交換、集合被覆はランダム再始動と3種類のLNSを使う。
6. 必要なら選択数下限を補充する。元の候補番号を昇順にして、目的値・診断値・実行可能性とともに返す。

`evaluate` は手順2の後、固定候補と指定候補を加えた状態を集計して返し、探索は実行しない。以下では各処理の役割と実装上の判断を説明する。

内部型と共通処理は `coverage_solver` の非公開領域にまとめている。LNS、1-swap、その場だけで使う候補選択や比較は、使用するメソッド内の処理またはlocal lambdaである。以下に登場する内部名は公開APIではなく、利用側から直接呼び出さない。

### 9.2 入力の正規化と両方向CSR

`prepared_problem` はサイズ、非負値、固定・禁止候補を検証する。次に各被覆集合をソートし、同じ対象番号の重複を除く。候補→対象の参照は `candidate_offsets` と `candidate_targets` に格納する。候補cの対象は、offsetsのc番目からc+1番目の直前までの連続区間になる。

対象→候補も `target_offsets` と `target_candidates` に格納する。このように、各集合を大きな配列の区間として表す形式をCSRと呼ぶ。対象から被覆可能候補を逆引きできるため、未被覆の少ない部分解の補充や、似た候補の列挙に利用できる。

被覆関係は元の候補番号のまま保持する。通常探索で無効化した候補もCSRから削除しない。固定候補、初期解、部分問題、個数下限補充で扱う可能性があるためである。空集合候補は固定されていなければ通常探索では無効になる。

候補ごとの対象重み和も前計算する。状態に被覆対象がまだないときは、この値をそのまま追加利得として使える。

### 9.3 重複候補と包含される候補の整理

同一被覆集合はハッシュ値と長さで比較範囲を絞り、実際の整列済み対象列を比較して確認する。ハッシュ値が等しいだけで同一集合とはみなさない。

同一集合を持つ有効候補群に固定候補があれば、その固定候補をすべて残し、他を通常探索から外す。固定候補がなければ最安の候補を残し、同費用なら番号の小さいものを残す。

包含判定では、ある候補の対象集合をすべて含み、費用が同じか安い別候補があれば、含まれる側を通常探索から外せる。固定候補は外さない。対象数、費用、`std::includes` による完全な包含を確認するので、被覆価値や全被覆の可能性を損なう候補除去は行わない。

比較対象を探す際は、その候補が覆う対象のうち、逆参照数の少ない対象を使う。その対象を覆う候補から費用・集合サイズで絞り、包含の実比較を最大96候補に限定する。候補列挙自体は96件で停止するわけではない。この上限は前処理の時間を抑えるためのもので、包含関係をすべて見つける保証はない。

### 9.4 部分問題条件の合成と作業配列

`prepare` は `problem_view` を返す。viewは被覆関係・重み・費用などのspanを持ち、大きなCSRをコピーせずに探索へ渡す。

追加条件がなければ構築済みviewをそのまま使う。重みoverrideがあれば非負値とサイズを検証し、候補ごとの重み和を再計算する。費用overrideは検証後に参照先を差し替える。

恒久条件を作業maskへコピーし、今回の固定・禁止を追加する。pool制限がある場合、固定候補以外のpool外候補を禁止にする。`active` maskには、これらを反映した「今回、通常探索で利用できるか」という最終結果を入れ、候補列挙中はそのmaskを使う。

一時禁止、pool制限、費用overrideのいずれかがある場合、コンストラクタで見つけた包含・重複関係がそのまま安全とは限らない。このときは空集合以外の構造候補をいったん利用対象へ戻し、今回の禁止条件を反映する。恒久禁止は復活しない。追加固定は、その候補が通常探索で無効化されていても選択対象になる。

重みだけの変更では、集合の包含と非負重みの関係は保たれるため、この復活は不要である。条件を反映した有効候補数から対象の希少度も計算する。

mask・重み和・希少度のバッファはsolver内で再利用する。探索状態とgreedy用作業領域は呼び出し内で作る。すべてのメモリ確保がなくなるわけではない。同一solverの並行呼び出しができない理由は、この可変作業領域の共有にある。

### 9.5 選択状態と正確な差分計算

`state` は各候補の選択フラグ、選択候補一覧、一覧内の位置、各対象の被覆回数を持つ。対象の被覆回数が0から1になると、その重みと被覆対象数を加算する。1から0になると減算する。それ以外の被覆回数変化では価値を変えない。

候補削除時は、選択一覧の末尾要素を削除位置へ移してから末尾を消す。位置の逆引きも更新するため、選択一覧そのものからの削除は定数時間でできる。被覆回数の更新には、その候補が覆う対象数ぶんの時間がかかる。

追加利得は未被覆対象の重み和、削除損失は削除候補だけが覆う対象の重み和である。実装では `(cover_count[target] == 0) * weight`、`(cover_count[target] == 1) * weight` のように条件を0/1として加算する。これは整数の被覆定義に基づく集計で、浮動小数点の近似得点ではない。

1候補交換の差分は「追加候補の利得－削除候補の損失」に、両候補の共通対象のうち、削除候補だけが覆っていた対象の重みを戻して求める。この共通部分は整列済み対象列を2本の添字で走査して計算する。追加によって直ちに再被覆される対象を損失として残さないための補正である。同じ追加候補について削除候補を比較する間は状態が変わらないため、追加利得は最初の1回だけ計算する。

### 9.6 lazy greedyとランダム化

greedyは、その時点で価値の高い候補を1つずつ追加する処理である。候補を追加するだけの間、未被覆対象から得られる利得は増えない。この性質を使い、heapに保存した古い利得を現在利得の上界として利用する。

初期候補を配列へ入れ、`std::make_heap` でheapをまとめて構築する。heapから候補を取り出したときに正確な利得を再計算する。再計算した優先度が次点の保存優先度より小さければheapへ戻し、そうでなければ候補を選択する。資源不足や正の利得がなくなった候補は選ばない。

| 用途 | 主に使う優先度 | 資源 |
|---|---|---|
| 選択数制約 | 新たに覆う重み和 | 1候補につき1 |
| 予算制約 | 新たに覆う重み和÷費用。利得のみの構築も比較 | 候補費用 |
| 集合被覆 | 新たに覆う対象の希少度和÷費用 | 候補費用 |

対象の希少度は、その対象を覆える有効候補数をdとして、dが0なら0、そうでなければ `max(1LL, 1000000LL / d)` とする。希少な対象を後回しにして、高費用の候補しか残らなくなることを抑えるための重み付けである。集合被覆の最終目的値はあくまで費用であり、希少度和を返却目的値にしない。

費用0で正利得の候補には無限大の優先度を与える。予算版の利得のみの構築でもこの扱いを使う。選択数版の資源は1なので、費用0の候補がこの理由で優先されることはない。

優先度計算には `double` を使う。heapの比較は優先度、保存利得、候補番号の順だが、取り出した候補を確定する比較は優先度だけである。このため「すべての同優先度候補の利得・番号を再評価して、常に同じ副順位を厳密に守る」とは限らない。返却重み・費用の集計は `long long` で行う。

ランダム化構築では、構築seedと候補番号から一定の係数を作り、候補の優先度に掛ける。同じ構築内では係数が変わらないため、利得の上界として使う関係を保つ。最大被覆では係数範囲820～1180、集合被覆では750～1250を使い、決定的構築では1000に固定する。

乱数生成器 `fast_random` は、64bitの状態へ定数を加え、ビットシフト・排他的論理和・乗算で混ぜるSplitMix64方式を使う。範囲内の整数は生成値の剰余で求める。暗号用途の乱数生成器ではなく、構築順や候補samplingを決めるためのものである。

### 9.7 疎なrepairと冗長候補の削除

repairは、一部の候補を除いた状態や未完成の初期解に、候補を追加して埋め直す処理である。未被覆対象が少ない場合、すべての候補を評価する代わりに、未被覆対象から逆CSRをたどって正利得の候補だけを列挙する。

最大被覆の疎repairでは、未被覆かつ正重みの対象から重みを候補へ加算する。集合被覆では、重み0の対象も必須なので希少度を加算する。集合被覆LNSでは、削除候補が覆っていた対象から未被覆になったものを集め、ソート・重複除去したリストをrepairへ渡す。

冗長候補の除去条件は問題型で異なる。

- 最大被覆では、取り除いても被覆重みが減らない非固定候補を削除する。削除によって重み0の対象が未被覆になることはある。選択一覧を逆順に1回走査する。他候補を削除すると残存候補の削除損失は減らないため、一度残した候補を再検査する必要はない。
- 集合被覆では、費用の高い非固定候補から順に見て、その候補だけが覆う対象が0個なら削除する。重み0の対象も守る。

固定候補は冗長でも削除しない。個数下限の補充は最後に別処理で行う。

### 9.8 選択数制約付き最大被覆の探索

固定候補から決定的なgreedy解を作り、冗長候補を除く。improveなら、固定候補を加えた初期解の個数を検証してから同様に補充・整理し、被覆重み、次に候補数で比較する。

次に、時間・反復上限の範囲で最大4回のランダム化greedy構築を比較する。その後はrandom ruinとrelated ruinによるLNSを使う。LNSは、選択解の一部を取り除き、greedyで補充する大きな変更操作である。

取り除く個数は、除去可能候補数の約3分の1を上限とする。1から始め、一定確率で2倍にして上限で切るので、小さな変更を多く試しつつ大きな変更も扱う。

- random ruinは、非固定候補から重複なくランダムに選ぶ。
- related ruinは、ランダムに選んだ1候補と共通する対象数が多い選択候補を優先する。不足分は残りからランダムに補う。

削除後は、逆CSRによる疎なランダム化repairを行う。被覆重みが増えるか、重みが同じで候補数が増えなければ受理する。条件を満たさなければ、追加候補を取り除いて削除候補を戻す。候補集合を変える同点遷移も許す。

### 9.9 予算付き最大被覆の探索

固定候補費用を検査し、利得÷費用のgreedyと、利得のみを優先するgreedyを作って比較する。improveの初期解は、固定追加後に予算内であることを検証し、利得÷費用で補充して比較する。

ランダム再始動では2つの指標を交互に使い、各構築後に最大8回の1-swapを試す。内部の再始動上限は1,000,000回だが、通常は利用側の時間・反復上限で先に止める。

1-swapは次の手順で行う。

1. 全候補から24回samplingし、未選択・有効・正利得・候補単体で予算内の候補について利得÷費用を比較する。samplingは重複することがある。
2. 採用した追加候補を残り予算でそのまま入れられるなら、交換せず追加する。
3. 入れられない場合、選択済み候補が128個以下なら全件、それより多ければ128回samplingして削除側を調べる。固定候補は除外する。
4. 予算内となる組み合わせについて、共通対象の回復も含む正確な差分を比較する。
5. 被覆重みが増える交換、または同じ重みで費用が減る交換だけを受理する。

各再始動の候補解と保存中の最良解は、被覆重み、費用、候補数の順に比較する。

### 9.10 最小費用集合被覆の探索

すべての対象に有効な被覆候補があるかを先に調べる。対象ごとに候補を選べれば、候補間競合や予算制約がないこの問題では、全候補の和集合による全被覆が可能になる。

固定候補に希少度÷費用のgreedyで追加し、全被覆できたら冗長候補を削除する。improveは未完成の初期解を許し、同じ方法でrepairする。全被覆に成功した状態を、費用、次に候補数で比較する。

最大4回のランダム化構築を比較した後、random ruin、低損失ruin、related ruinを使う。低損失ruinでは、費用に対して「その候補だけが覆う対象数」が小さい候補を優先する。1回の削除候補選択では、選択数が48以下なら全件、それより多ければ48回samplingする。固定候補と費用0の候補は低損失ruinの対象にしない。複数候補を選ぶ際は、先に除いた影響を反映して次を評価する。

削除した候補を一時的にブロックしてrepairし、同じ候補をすぐ戻すだけの構築を避ける。全被覆できなければブロックなしで追加repairを試す。ブロックはこの内部repairだけの条件で、利用側の恒久・一時禁止を解除するものではない。

全被覆ができたら冗長候補を削除し、費用が下がるか、同費用で候補数が減る場合だけ受理する。失敗時は追加候補を消し、元解の候補を戻す。元解の候補をstampで識別するため、途中の冗長削除で元の候補が外れた場合も復元できる。締切によりrepairが中断しても、保存した実行可能解を部分解で置き換えない。

### 9.11 下限補充・初期解検証・返却値

選択数版の公開メソッドは、下限が正の場合、禁止されていない候補数が下限を満たすかを先に確認する。ここには通常探索で無効化された空集合・重複・包含候補も含む。実行可能な探索結果の個数が下限未満なら、未選択かつ非禁止の候補を費用昇順、同費用なら番号順に並べて補充する。重みが非負なので、補充によって被覆重みは減らない。

初期解の共通構築処理は、固定候補を追加してから初期候補を読む。範囲外、禁止、初期リスト内の重複は例外になる。初期リストに固定候補を1回含めることは許され、二重には追加しない。初期解に指定された候補は通常探索用の `active` と別に扱うが、禁止条件は守る。

固定候補だけで個数・予算を超える場合、個数下限を満たせない場合、全被覆不能の場合は、初期解の内容を検証する前にfalseを返すことがある。初期解を独立して検証したい場合は `evaluate` と利用側の制約検査を使う。

返却時には選択候補をコピーして昇順ソートする。被覆重み、費用、被覆対象数は状態の集計値を使い、未被覆数は対象数との差で求める。返却されたvectorはsolverの作業領域を参照せず、次の呼び出し後も利用できる。

### 9.12 時間管理と計算量

`time_keeper` は相対終了時刻と絶対deadlineの早い方を改善探索の締切に使う。外側の反復では毎回確認する箇所と64試行ごとに時計を確認する箇所がある。greedyは開始時とheap取り出し256回ごとに絶対deadlineを確認する。相対時間だけでは決定的初期構築を中断しない。

改善試行を始めない条件でも、入力・初期解の検証、必要な下限補充などは実施する。予算版はswapの計上に加え再始動終了も数えるため、返却反復数が上限を最大1超える場合がある。`iterations == 0` は最適性を表さず、期限切れでも0になり得る。

計算量では、対象数をT、候補数をC、正規化後の被覆関係総数をE、選択候補の関係数合計を $E_X$ とする。入力に同じ対象番号を繰り返し書く場合、正規化前の関係数を $E_{\mathrm{in}}$ とし、その入力を読む・ソートする分も必要になる。以下の表には、別途渡す固定・禁止などの候補リストの読み取り時間も加える。

| 処理 | 時間の目安・保守的な上界 |
|---|---|
| コンストラクタ | $O(E_{\mathrm{in}}\log(E_{\mathrm{in}}+1)+C\log(C+1)+CE+T)$。ハッシュ衝突時の列比較と逆CSR走査も含む |
| 追加条件のないview取得 | $O(1)$。この後の状態初期化やgreedyは別 |
| 部分問題の条件合成 | $O(T+C+E)$ に入力リストの検証時間を加えたもの |
| 候補の追加・削除・利得・損失 | その候補の被覆対象数に比例。空被覆状態の追加利得は前計算値で $O(1)$ |
| 1組の候補の交換差分 | 両候補の被覆対象数の和に比例 |
| evaluate（追加条件なし） | $O(T+C+E_X)$。自動追加の固定候補も $E_X$ に含める |
| evaluate（部分問題あり） | 上記に条件合成が加わり、概ね $O(T+C+E)$ |
| 個数下限補充 | 候補のソートと状態再構築を含め $O(T+C\log(C+1)+E)$ |
| 保持する主データ・探索状態のメモリ | $O(T+C+E)$。構築中は正規化前の入力コピーも必要 |

greedyのheap取り出し回数や探索反復量は、集合の重なりと停止条件によって変わる。全solveの実行時間を単純な線形時間とはみなさない。密な被覆関係ではEが大きくなり、入力変換・構築・初期解だけでも時間を使う。有限deadlineも前処理、heap初期化、冗長除去、下限補充、メモリ確保を途中停止するものではない。
