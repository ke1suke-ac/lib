# facility_location_v11.hpp ユーザーガイド

離散候補から施設・代表点・集合を選ぶC++20ライブラリのガイドです。ソースは`facility_location_v11.hpp`、掲載例をまとめた実行用ファイルは`facility_location_guide_examples_v11.cpp`です。

問題への当てはめ方は1〜4章、入力の準備と呼出し方は5〜8章、探索内部の仕組みは9章で説明します。4章・6章・8章の番号U01〜U17は対応しています。

## 1. 何を解くライブラリか？

### 1.1 「選ぶもの」と「サービスを受けるもの」

例えば、倉庫を建てられる土地が20か所あり、配送先が100か所あるとします。建てる倉庫を3か所に決め、各配送先は、その3倉庫のうち配送費が最も安い倉庫を利用します。このとき配送費の合計を小さくする問題を扱います。

本ガイドでは、選択肢となる土地を「候補施設」、配送先を「需要点」と呼びます。施設と需要点は別々の集合です。20候補から3施設を選ぶのであって、100需要点から3点を選ぶ必要はありません。地理的な場所に限らず、施設を代表データ・実行方式・被覆集合、需要点をデータ・処理対象・被覆したい要素と読み替えることもできます。

需要点ごとに重要度を付けられます。配送量が多い地点なら重みを大きくします。また、各需要点に「どの新設施設も使わず、外注や現状のサービスを使う」という選択肢を設けられます。これをfallbackと呼びます。施設を選ぶこと自体に費用や報酬が生じる場合も扱えます。

選ぶ施設数は**ちょうどK個**です。各施設が担当できる需要量には上限がなく、ある需要点の割当てが別の需要点の割当てを妨げることはありません。この独立性が、ライブラリを使えるか判断する最も大切な条件です。

### 1.2 目的関数と制約

選択施設集合を$S$としたときの総費用を、次で定義します。

$$J(S)=\sum_{i=0}^{Q-1}w_i\min\left(b_i,\min_{f\in S}c_{f,i}\right)+\sum_{f\in S}a_f$$

この値を小さくする集合を求めます。

$$\min_{S}J(S)$$

ただし、選択集合は次を満たします。

$$|S|=K,\ F\subseteq S,\ S\subseteq F\cup A$$

|記号|意味|設定する内容|
|---|---|---|
|$Q$|需要点数|サービスを評価する対象の個数。1以上|
|$M$|候補施設数|選択肢として列挙する施設の個数。1以上|
|$i$|需要点の番号|0から$Q-1$|
|$f$|候補施設の番号|0から$M-1$|
|$S$|実際に選ぶ施設IDの集合|solverが決める。重複はない|
|$K$|選択する施設の総数|1以上$M$以下。必須施設もこの個数に含む|
|$c_{f,i}$|施設$f$から需要点$i$をサービスする費用|有限・非負。距離、所要時間、損失など|
|$w_i$|需要点$i$の重み|有限・非負。省略時は1|
|$b_i$|需要点$i$のfallback費用|有限・非負。fallbackを省略したモデルでは$+\infty$とみなす|
|$a_f$|施設$f$を選ぶこと自体の単項費用|有限。正なら負担、負なら選択報酬。省略時は0|
|$F$|必ず選択する施設集合|省略時は空集合|
|$A$|追加選択を許す施設集合|省略時は全候補。必須施設は$A$の外にあっても選択できる|
|$J(S)$|重み付きサービス費用と施設単項費用の合計|小さいほど良い。負になる場合もある|

内側の最小値は、その需要点にとって最も安い利用先を表します。まず選択施設の中で最も安い費用を選び、fallbackがあればそれとも比較します。その費用に需要点の重みを掛け、全需要点について足します。最後に、選んだ施設の単項費用を1施設につき1回だけ足します。

例えば2需要点の重みが2と3で、選択した1施設からの費用が4と2なら、サービス費用は$2\times4+3\times2=14$です。fallback費用が両地点とも3であれば、最初の需要点はfallbackを使えるので$2\times3+3\times2=12$になります。さらに施設の単項費用が$-1$なら、総費用は11です。重みは施設単項費用には掛けません。

ここでの$+\infty$は省略の数学的な説明です。C++入力へ無限大を格納する意味ではありません。fallbackが不要なら、対応する配列を空にします。

### 1.3 解の意味とモデルの境界

返るものは、選択施設ID、各需要点の利用先、総費用です。常に大域最適解を返すライブラリではありません。指定した探索量や時刻を目安に、制約を満たす良い施設集合を求めるために使います。

固定できるのは施設の選択です。「需要点0は必ず施設2へ割り当てる」という割当制約はありません。施設2を必須にしても、需要点0が別施設を使う方が安ければそちらに割り当てます。また、同じ施設が何需要点を担当してもよいため、車両の積載量、工場の生産上限、サーバーの同時処理数などはこの目的関数だけでは表現できません。

## 2. 厳密解アルゴリズム

### 2.1 少数の候補、または残り1施設なら全列挙を考える

必須施設数を$F_c=|F|$、必須施設を除く許可候補数を$P_c=|A\setminus F|$、追加選択数を$R=K-F_c$とします。調べる施設集合は$\binom{P_c}{R}$通りです。これが十分小さければ、全組合せを評価する方が確実です。単純な評価なら1集合あたり$O(QK+K)$、合計で$O(\binom{P_c}{R}(QK+K))$です。何通りまで間に合うかは$Q$と実行環境を含めて測ります。

$R=0$なら選ぶ集合は必須施設だけです。$R=1$なら、必須施設・fallbackによる現在のサービス費用を先に求め、残りの1施設を全候補で比較できます。固定基線の計算を含め$O(Q(F_c+P_c)+K)$程度です。必須施設のない$K=1$もこれに含まれます。施設単項費用や非対称費用があっても全候補を正しく評価すれば厳密です。

本ライブラリの`evaluate_k_median`は1つの非空集合の費用評価に使えます。全組合せの列挙機能はありません。一方、`solve_k_median`の最初の試行は最初の可動施設を全候補で比較するため、可動施設が1個だけの問題は、この処理だけで最適な選択が決まります。数値演算が正しく行えることが前提です。

### 2.2 被覆対象が少なければ集合DPを考える

費用が「被覆できるなら0、できなければ1」で、需要点数$Q$が小さい場合、被覆済み要素を$Q$ビットの集合で表せます。候補集合を1つずつ処理し、「選んだ個数」と「被覆済みビット集合」を状態にして、選ぶ・選ばないを更新します。同じ候補を何度も選ばないよう、候補の処理順をDPの段階にします。

状態数は1段あたり$O(K2^Q)$、単純な計算量は$O(MK2^Q)$です。施設ごとの加算費用があれば同じ状態に到達する最小の選択費用を保持し、最後に未被覆要素の重みを足せます。必須施設は選ばない遷移を禁止し、禁止候補は選ぶ遷移を禁止します。一般の多値費用では被覆ビットだけで費用状態を表せないため、このDPをそのまま流用できません。

### 2.3 一直線上の標準的なk-medianなら区間DPを考える

需要点を一直線上に並べ、距離を絶対値とし、施設候補が各需要点、重みが全て1、fallback・施設単項費用・必須施設・候補制限がない場合を考えます。最適な割当ては、座標順の連続区間に分けて考えられます。1区間の代表は中央値の需要点に取れます。

先頭$j$点を$k$区間に分ける最小費用を$D[k,j]$、点$l$から$r$までを中央値1点で担当する費用を$C(l,r)$とすると、区切り位置$t$を全て調べるDPになります。

$$D[k,j]=\min_{k-1\le t<j}\left(D[k-1,t]+C(t+1,j)\right)$$

初期値は$D[0,0]=0$、それ以外の実行不能状態は無限大です。座標をソートし累積和で区間費用を求めれば、素朴なDPでも$O(KQ^2)$です。これはライブラリの汎用探索とは別に実装する厳密解法です。一次元絶対距離の区間費用とDPについては、[Grønlundほかの論文・4.2節](https://arxiv.org/html/1701.07204v4)を参照できます。

ここで述べた条件には、2次元マンハッタン距離、任意の候補部分集合、地点別の設置報酬は含めていません。それらを加えるときは、区間DPの状態だけで必要な制約を表せるかを改めて確認します。

### 2.4 分離できるグループと整数計画

需要と候補が独立したグループに分かれ、グループをまたいだ利用をしないことが問題設定から保証される場合は、各グループについて施設数別の最小費用を求め、総施設数が$K$になるよう外側のDPで配分できます。ただし各グループ内の値も厳密に求めて初めて全体が厳密になります。到達不能に有限の罰則費用を置いただけでは、グループ間の独立性は保証されません。

一般の費用表でも、選択施設を表す0/1変数と、需要点の割当変数を使う混合整数計画へ定式化できます。施設数、必須施設、許可候補、fallbackを制約として加えます。facility locationのモデル化自体は[Gurobiの公式例](https://gurobi.github.io/modeling-examples/facility_location/)にも示されています。このライブラリの固定施設数・fallbackへの適用では、その条件を追加したモデルが必要です。

外部solverを使う場合も、時間切れで見つかった実行可能解と、最適性が証明された解を区別します。コンテスト環境で外部solverが使えない場合や、何度も短時間で部分問題を解く場合は、本ライブラリを使う理由になります。

## 3. 差分更新

### 3.1 前の解を使うことと、内部状態の差分更新は別

前ターンの施設集合を`improve_k_median`へ渡し、その集合から探索を始めることはできます。初期施設をゼロから選び直す必要がなく、少しだけ条件が変わるターン制問題で利用できます。ただし渡すのは施設IDの集合であり、前回の近傍情報・探索途中の状態・乱数器の状態を引き継ぐAPIではありません。

`facility_location_workspace`は、**同じ問題・同じ施設数・同じ制約**を繰り返し解くための作業領域です。配列や必須施設の基線を再利用します。`workspace.improve`でも入力施設集合から最近傍情報を組み直し、統計と乱数状態を呼出しごとに初期化します。前回の探索の一時停止・再開ではありません。

### 3.2 後から変えられるもの

|変更したいもの|既存workspaceで可能か|必要な処理|
|---|---|---|
|初期施設集合|可能|同じ個数で、保存済み制約を満たす集合を`improve`へ渡す|
|seed、探索回数、評価上限、deadline|可能|呼出しごとに`options`を渡す。deadlineは絶対時刻なので更新する|
|新規構築か既存解改善か|可能|同じworkspaceの`solve`または`improve`を選ぶ|
|費用表の1要素・1行|不可|変更後のproblemでone-shot関数を呼ぶか、workspaceを作り直す|
|需要点の重み|不可|同上。固定基線の有無にかかわらずproblemを不変として扱う|
|fallback・施設単項費用|不可|同上。空配列から非空配列への変更も含む|
|需要点数・候補数・ID対応|不可|入力を組み直す。前の施設IDが変わるなら写し替える|
|施設数K|不可|新しいKで構築する。初期施設集合もK個へ整える|
|必須施設・許可候補|不可|新しいrestrictionsで構築する。古い初期解が実行可能か再確認する|
|元グラフの辺や座標|グラフ・座標自体を保持していない|呼出し側で費用表を更新した後、新しい探索状態を作る|

workspaceは`problem`を参照し続けます。workspaceが存在する間は、そのproblemの全フィールドを変更せず、problemを破棄・別の場所へmoveしないでください。workspaceへ一時problemを渡すことも避けます。restrictionsは構築時に内部へコピーされます。元のrestrictionsを書き換えてもworkspaceの制約は変わりません。

### 3.3 ターン制問題での実際の手順

1. 前ターンから保持するものを、`result.facilities`にする。必要なら外部IDも一緒に保持する。
2. 新しい観測から、費用表・重み・fallbackなどを更新する。グラフが変わったなら必要な最短路も再計算する。
3. 前の施設集合が、新しい候補ID・必須施設・候補制限を満たすか確認する。
4. 満たす場合は、新しいproblemとその施設集合で`improve_k_median`を呼ぶ。満たさない場合は、実行可能な初期集合へ補修するか`solve_k_median`で構築する。
5. 新しい問題に対する`result.cost`と割当てを使い、施設集合を次ターンへ保存する。

同じ新しい問題について複数seedを試すなら、問題更新後にworkspaceを1回構築し、そのターンの試行間で再利用できます。次の問題更新前にworkspaceのスコープを終えます。U13は問題を変えない反復、U17は問題を変えた後の再実行を示します。

### 3.4 どの程度の処理を省けるか

初期施設の新規選択を省けますが、内部の作業領域確保・制約の整理・最近傍の構築は必要です。費用が1要素変わっただけでも、その1要素だけをsolver内部へ通知するAPIはありません。需要点ごとの最近傍再構築は、おおむね需要点数と選択施設数の積に比例します。

グラフの辺1本の変更も、多くの最短路へ影響する場合があります。「辺の両端に対応する費用だけ直せばよい」とは限りません。費用表を正しく更新する責任と、その費用表を使って施設集合を改善する処理を分けて考えます。また、費用表を変えた後のスコアは古い問題のスコアと直接比較せず、新しい問題で前の施設集合を評価した値と比較します。

## 4. ユースケース

同じ数理モデルを使っていても、費用表の作り方、制約の意味、探索結果を何に戻すかが異なります。U01〜U06・U09〜U12・U16は主にモデルへの当てはめ方、U07・U08・U13〜U15・U17は外側の探索やターン処理への組み込み方です。例えばグラフ配置に必須施設とdeadlineを組み合わせることもできます。

|番号|用途|選ぶもの・評価するもの|
|---|---|---|
|U01|重み付きの一般的な配置|候補施設を選び、需要点への費用合計を小さくする|
|U02|k-medoidsによる代表選択|実在するデータ点を代表に選ぶ|
|U03|重み付きmaximum coverage|集合を選び、被覆できる要素の重みを大きくする|
|U04|座標からの拠点配置|離散座標候補を選び、移動距離を小さくする|
|U05|グラフ上の配置|候補頂点を選び、最短路費用を小さくする|
|U06|必須施設・候補制限付きの構築|設置義務と利用可能地点を守って選ぶ|
|U07|既存解の改善|手作業・貪欲法・別solverの施設集合を改善する|
|U08|LNSの部分修復|一部の施設だけを置き換える|
|U09|外注・現状維持付きの配置|新設施設と外部サービスを需要点ごとに選び分ける|
|U10|地点別設置費・報酬|サービス費用と施設選択自体の費用を合わせて判断する|
|U11|施設追加の純改善量|既存サービスへ追加した価値を測る|
|U12|施設数も含めた比較|複数のKについて得た解から選ぶ|
|U13|同じ問題の反復探索|作業領域を再利用し、複数seedで改善する|
|U14|外側の時刻制限との協調|コンテスト本体の残り時間内で改善する|
|U15|評価回数による探索量制御|反復呼出しや比較実験の仕事量を制御する|
|U16|小数・非対称のサービス損失|距離ではない一般的な損失表から選ぶ|
|U17|ターンごとに条件が変わる配置|新しい費用表で前ターンの施設集合を改善する|

### 4.1 U01：重み付きの一般的な配置

倉庫、配送拠点、処理サーバーなど、候補ごとに各需要点を担当した費用が分かるときの基本形です。需要の多い地点を軽い地点と同じ1件として扱うと、全体として重要な地点が軽視されます。そこで需要量や重要度を重みにします。

必須なのは、候補数M、需要点数Q、候補から需要点への費用表、選ぶ総数Kです。重みは全て同じなら省略できます。返る施設は採用する選択肢、割当ては各需要点が使う選択肢、総費用は重みを考慮したサービス負担を表します。各施設に処理容量がある場合は、この基本形だけでは不十分です。

### 4.2 U02：k-medoidsによる代表選択

多数の画像から代表画像を選ぶ、シナリオ集合から代表シナリオを選ぶなど、実在するデータを代表として残したい場合です。全データを候補施設にも需要点にも置き、代表から各データへの非類似度を費用にします。代表そのものが入力データなので、平均座標を作るクラスタリングとは返すものが異なります。

必要なのはデータ同士の非類似度表と代表数Kです。頻出データを重く扱うなら重みも指定します。標準的な対称距離なら対角成分は0ですが、非類似度の定義自体は利用側で決めます。割当ては所属クラスタの連番ではなく、代表データのIDです。代表ごとの件数を均等にする制約や、全代表が必ず誰かを担当する制約はありません。

### 4.3 U03：重み付きmaximum coverage

監視カメラの候補位置ごとに見える区画が分かる、広告施策ごとに届く利用者群が分かる、といった問題です。同じ要素を二重に覆っても利益を二重計上せず、少なくとも1つの選択集合が覆えば、その要素の重みを獲得します。

各集合を施設、被覆対象を需要点とし、被覆できる組合せの費用を0、それ以外を1にします。すると最小サービス費用は「未被覆要素の重み合計」になります。必要なのは被覆表、要素の重み、選ぶ集合数Kです。総重みから未被覆重みを引けば、最大化したい被覆重みが得られます。

集合を増やしても被覆は減らず、追加費用もない標準問題なら、「高々K個」と「ちょうどK個」は、不要な集合を追加して埋められる限り同じ最適被覆値を持ちます。各集合の利用費用や相互排他条件を加えた場合には、この読み替えをそのまま使いません。

### 4.4 U04：座標からの拠点配置

格子状の街で、既知の候補位置から拠点を選びたい場合です。候補位置と需要位置の座標から、移動ルールに合った距離を計算して費用表にします。8章では上下左右の移動を想定したマンハッタン距離を使います。

必要なのは候補座標の配列、需要座標の配列、拠点数Kです。需要量が違うなら重みも指定します。座標そのものをsolverが動かすことはなく、返るのは候補配列のIDです。障害物や一方通行がある場合、座標差だけでは実際の移動費用にならないため、U05のグラフ距離を検討します。

### 4.5 U05：グラフ上の配置

道路網や通信網で、拠点から各需要頂点までの実際の経路費用を使いたい場合です。グラフの頂点すべてを施設候補にする必要はありません。候補頂点と需要頂点を別々に列挙し、その間の最短路費用を表にします。

必要なのは辺の向きと非負辺費用、候補頂点、需要頂点、需要重み、Kです。到達できない組合せをどう評価するかも決めます。有限の罰則を入れる例では、その罰則を払う割当ても選択肢に残ります。「到達不能な割当ては禁止」と同義ではありません。

有向グラフでは費用の向きが大切です。拠点から配達するなら拠点から需要点へ、需要点が拠点へ移動するなら需要点から拠点へ測ります。8章の例は前者で、結果の候補IDを元の頂点番号へ戻します。

### 4.6 U06：必須施設・候補制限付きの構築

すでに契約した拠点は必ず残す、今回利用許可のある土地だけを候補にする、といった構築です。まだ初期施設集合がないときも、必須集合Fと許可集合Aを与えて新規に選べます。

指定するKは必須施設も含む総数です。必須施設は許可集合に重複して入れてもよく、許可集合の外にあっても選ばれます。Fの個数がKを超える場合や、FとAの和集合がK個に足りない場合は実行不能です。需要点の担当先を固定する条件や、「この2施設は同時に選べない」という条件は表現しません。

### 4.7 U07：既存解の改善

貪欲法や別のsolverですでに作った施設集合があり、その周辺で費用を減らしたい場合です。候補から最初のK施設を選び直す処理を省き、渡した実行可能な集合から探索を始めます。

入力するのは同じ問題の費用表と初期施設IDの集合です。Kは初期集合の個数で決まります。初期集合を最良解の候補として保持するため、正確な整数演算では探索途中に悪い集合を試しても、その集合へ最終結果を悪化させる必要はありません。変更量そのものへの罰則や「最大2施設しか変更しない」といった制限は別途モデル化が必要です。

### 4.8 U08：LNSの部分修復

大きな解のほとんどを維持し、一部だけを作り直す使い方です。例えば現在の20拠点のうち17拠点は残し、3拠点だけを近くの候補と交換します。外側の探索が「どこを動かすか」を決め、ライブラリがその範囲内の配置を改善します。

指定するのは現在の全施設集合、動かしてよい現在施設、入ってよい追加候補です。動かさない施設をFにし、動かす現在施設もAへ含めます。現在施設を候補から除外すると、元の解が実行不能になってしまうためです。Kは現在解の総数のままです。

候補集合を絞っても需要点は全て評価します。動かさない施設が担当する需要点も、施設を追加すれば担当先が変わることがあるからです。別の外側目的関数がある場合は、この部分費用の改善が外側の総スコアを改善するか、外側でも評価します。

### 4.9 U09：外注・現状維持付きの配置

既存の配送サービスを引き続き使いつつ、新しい拠点の方が安い需要だけを切り替える問題です。需要点ごとの現行サービス費用をfallbackにすれば、拠点を選んだからといって全需要を新設拠点へ移す必要はありません。

必要なのは新設候補からの費用表、需要点ごとのfallback費用、選ぶ新設施設数Kです。外部サービスはKに数えません。結果には外部サービスを利用する需要点も現れます。外注量の上限、外注サービスを使い始めた場合の一括料金は、需要点ごとに独立したfallbackだけでは表せません。

### 4.10 U10：地点別設置費・報酬

移動費は安いが設置費の高い土地と、移動費は高いが設置費の安い土地を比較したい場合です。候補ごとの単項費用を加えることで、サービス負担だけでなく施設そのものの負担も合わせて判断します。負の値なら設置補助や選択報酬を表します。

指定するのは候補ごとの単項費用と固定の施設数Kです。単項費用は担当需要数に関係なく1回だけ発生します。使う費用の単位・評価期間をそろえてください。例えば日次の配送費に一生分の建設費をそのまま足すと、意図した比較になりません。2施設を同時に選んだ場合の値引きは単項費用ではありません。

### 4.11 U11：施設追加の純改善量

「新しい施設を追加すると、現状からどれだけ得になるか」を知りたい場合です。U09の現状サービス費用とU10の新設費用を組み合わせ、現状だけの費用から追加後の総費用を引きます。返す指標を、費用そのものから投資の純効果へ変える用途です。

指定するのは現状の需要点別費用、新しい施設候補、新設単項費用、追加する個数Kです。現状の施設は候補集合に数えず、現状サービスとして表します。純改善量が負なら、指定数の施設を追加した結果は現状より高コストです。ただし近似探索なので、負の結果だけから「どの追加案も損」とまでは断定できません。

### 4.12 U12：施設数も含めた比較

拠点を何か所にするかも外側で選びたい場合です。ライブラリ自体はKを固定して解くため、例えばKを2、3、4と変えて解き、同じ尺度の総費用を比較します。候補ごとの設置費を入れると、施設を増やす利点と負担を比較できます。

必要なのは比較するKの下限・上限、各Kに割り当てる探索量です。全てのKについて必須施設数と許可候補数の条件を満たす必要があります。サービス費用しかなく施設を増やす負担もなければ、最適値はKを増やして悪化しないため、施設数を抑える理由は別に必要です。複数の近似解を比較した結果は、可変K問題の厳密最適解を保証しません。

### 4.13 U13：同じ問題の反復探索

同じ部分問題に対して複数seedを試し、最も良い施設集合を採用する使い方です。各試行で費用表・K・制約が同じなら、workspaceを構築して内部配列を再利用できます。小さなrepairを多数回呼ぶ場面で検討します。

必要なのは不変の問題、同じ個数の実行可能な初期集合、seedの列、試行ごとの探索量です。8章の例では各seedを同じ初期集合から開始します。前試行の結果を次試行へ渡す連鎖的な改善にしたい場合は、そのように初期集合を更新します。どちらの場合も乱数と統計は呼出しごとに初期化されます。

### 4.14 U14：外側の時刻制限との協調

コンテスト本体で前処理、他の探索、出力処理も行う中、施設配置だけに残り時間を使いたい場合です。外側で最終終了時刻を管理し、その前にsolverの停止目標時刻を設定します。

渡すのは実行環境の単調時計による絶対時刻です。各関数を呼ぶたびに「今から2秒」とし直すと、全体の制限を超えます。また、停止目標は厳密な返却時刻ではありません。入力の大きさに応じて、状態構築・最終結果作成・出力のための時間を実測して残します。

### 4.15 U15：評価回数による探索量制御

小さなrepairを外側の探索から何度も呼ぶ場合や、同じ仕事量で手法を比べたい場合です。壁時計の揺らぎで探索の長さが変わるのを抑えるため、追加候補を評価した回数を上限にします。

指定するのは評価回数上限、初期試行数、摂動回数、seedです。評価1回は、未選択施設1個を追加するときに最良の削除位置を調べる処理です。施設の組合せを1組ずつ調べた回数とは異なります。初期解構築や結果作成はこの回数に含まれないので、上限0でも実行時間は0になりません。

### 4.16 U16：小数・非対称のサービス損失

データの型変換損失、ある方式で対象を処理した誤差、候補設備から対象への遅延など、距離の性質を持たない費用表を使う場合です。例えば方式AからBへの変換と、BからAへの変換で損失が違っていてもかまいません。

必要なのは候補から需要点への向きが明確な非負・有限の費用表とKです。8章は小数費用を小数のまま扱う例です。対称化したり、三角不等式が成立するよう修正したりする必要はありません。複数対象を同じ方式へ割り当てた結果として損失が変わる場合は、需要点ごとの独立な費用表にならないため対象外です。

### 4.17 U17：ターンごとに条件が変わる配置

毎ターン需要量や移動時間が少し変わり、その都度施設集合を更新する問題です。前ターンの施設集合を残しておけば、新しい費用表の下でそこから改善を始められます。変化が小さいとき、毎回最初の施設選択から始める作業を省けます。

指定するのは更新済みの問題、前ターンの施設ID、新しい制約、今回の探索量です。施設IDと個数が有効で、前の集合が新しい制約を満たすことが条件です。需要点の数は、新しい問題の中で整合していれば変えられます。比較の基準は新しい問題で再評価した前の集合です。施設を移す回数や距離に応じた費用は自動では加わりません。

## 5. 利用準備

### 5.1 コンパイル環境と入力型

`facility_location_v11.hpp`をincludeできる場所に置きます。C++20、GCC、libstdc++を想定し、ヘッダは`<bits/stdc++.h>`を使います。ACLや`hash_map.hpp`は不要です。手元での例のビルドは次の形です。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror facility_location_guide_examples_v11.cpp -o guide_examples
./guide_examples
```

基本となる型は`facility_location_problem<Cost, Calc>`です。`Cost`は費用表とfallbackの要素型、`Calc`は重み、施設単項費用、合計、改善差分、結果の型です。

|テンプレート引数|既定値|選び方|
|---|---|---|
|`Cost`|なし。明示する|普通の整数距離は`int`、大きな距離は`long long`、小数は`double`など|
|`Calc`|`long long`|重み付きの積・和・差分を保持できる符号付き型。小数費用なら`double`または`long double`を明示|

`Cost=double`で`Calc`を省略すると、計算時に整数へ変換されます。小数費用は`facility_location_problem<double, double>`などとします。`Calc`には改善差分を負に表せることが必要なので、符号なし整数は使いません。大きい整数合計にはGCCの`__int128_t`も使えますが、入出力は利用側で用意します。

### 5.2 problemの全フィールド

|フィールド|C++型|既定値|いつ・どう設定するか|
|---|---|---|---|
|`demand_count`|`int`|`0`|必須。需要点数Qを1以上に設定|
|`candidate_count`|`int`|`0`|必須。候補数Mを1以上に設定|
|`cost`|`std::vector<Cost>`|空|必須。厳密にM×Q要素を格納|
|`demand_weight`|`std::vector<Calc>`|空|空は全需要の重み1。指定するならQ要素|
|`fallback_cost`|`std::vector<Cost>`|空|空は外部サービスなし。指定するならQ要素|
|`facility_cost`|`std::vector<Calc>`|空|空は設置費0。指定するならM要素。負値も可能|

費用表は**施設優先**です。施設`f`から需要点`i`への費用は`problem.cost[static_cast<std::size_t>(f) * Q + i]`に入れます。施設0の全需要費用、施設1の全需要費用、という順番です。需要点優先で詰めても配列長が同じなら間違いに気づきにくいため、向きを確認します。

次はproblemを準備する完結した関数です。実際の入力では同じ順序で配列を埋めます。

```cpp
#include "facility_location_v11.hpp"

// 3候補・2需要点の例。戻り値をsolveやworkspaceの入力に使う。
facility_location_problem<int, long long> make_preparation_example() {
    facility_location_problem<int, long long> problem;
    problem.demand_count = 2;
    problem.candidate_count = 3;
    problem.cost = {
        1, 6,  // 施設0 -> 需要0, 需要1
        4, 2,  // 施設1 -> 需要0, 需要1
        7, 1   // 施設2 -> 需要0, 需要1
    };
    problem.demand_weight = {2, 3};
    // fallback_costとfacility_costは空のまま。
    return problem;
}
```

数値の有限性・非負性・overflowは利用側の前提です。`cost`、`fallback_cost`、`demand_weight`は有限・非負にします。単項費用も有限にし、入力値だけでなく重み付き和・差分・符号反転まで演算範囲へ収めます。

### 5.3 restrictionsの全フィールドと実行可能性

型は`facility_location_restrictions`です。

|フィールド|型|既定値|設定方法|
|---|---|---|---|
|`fixed_facilities`|`std::vector<int>`|空|必須施設IDを列挙。各IDは0以上M未満、配列内の重複不可|
|`candidate_facilities`|`std::vector<int>`|空|追加選択を許す候補IDを列挙。空なら全候補。配列内の重複不可|

同じIDをfixedとcandidateの両方に含めることはできます。内部では必須施設として扱われ、可動候補から除外されます。配列はソートして渡す必要はありません。candidateが非空なら、実行可能性は`fixed.size() <= K <= |fixed ∪ candidate|`です。candidateが空なら右辺はMです。

`candidate_facilities = {}`は候補ゼロを意味しません。必須施設だけで結果を決めたいなら、必須施設をK個にします。`improve`では、初期集合が全必須施設を含み、各初期施設がfixedまたは許可候補でなければなりません。

### 5.4 optionsの全フィールドと値の選び方

型は`facility_location_options`です。`clock`は`std::chrono::steady_clock`の別名です。コンストラクタではなく、各`solve`・`improve`呼出しへ渡します。

|フィールド|型|既定値|範囲・意味|値の選び方|
|---|---|---|---|---|
|`deadline`|`clock::time_point`|`clock::time_point::max()`|絶対停止目標時刻。既定値は時刻制限なし|外側の終了時刻から後処理分を引く。過去の時刻も指定可能だが、状態構築と返却処理は発生|
|`seed`|`std::uint64_t`|`1`|乱数seed。0も可能|再現比較では固定。複数試行では複数seedを使う|
|`initial_trials`|`int`|`4`|1以上。`solve`の初期解試行数|短いrepairでは1を出発点にする。`solve`で増やすと初期解の種類が増えるが、ILSに使える時間は減る。`improve`では試行を生成しないが1以上が必要|
|`max_perturbations`|`int`|`-1`|0以上ならILS回数上限。0はILSなし。-1はdeadlineなしで32回、deadlineありで時刻までを目標|比較時は固定値。局所探索だけなら0。時間を使い切る探索ならdeadlineと-1を組み合わせる|
|`max_candidate_evaluations`|`std::uint64_t`|`std::numeric_limits<std::uint64_t>::max()`|1-swapの追加候補評価回数の累積上限。0も可能|1回のrepairに割ける量を実測して設定。初期構築・再構築・結果作成は含まない|

`max_perturbations=-1`でdeadlineがある場合も、評価回数上限や交換できる施設の有無によって早く終了することがあります。初期試行4、摂動32などは普遍的な最適値ではありません。問題規模、候補pool、1回の呼出し時間に応じて、出力費用と実測時間で調整します。

用途ごとの出発点は次の通りです。下の値は説明用であり、性能保証のある閾値ではありません。

|用途|準備する設定|
|---|---|
|初めての通常solve|まず既定値。実行時間を測って調整|
|1-swapだけの改善|`initial_trials=1`、`max_perturbations=0`|
|同じ仕事量で比較|deadlineを既定値のままにし、seed・初期試行数・摂動数・評価上限を固定|
|短い反復repair|初期集合を渡し、評価上限を設定。必要に応じ少数回の摂動|
|残り時間を利用|絶対deadlineを設定、`max_perturbations=-1`。終了処理の余裕を別に確保|

評価上限は全初期試行とILSを合わせて1回のsolver呼出しに適用されます。初期試行ごとにリセットされません。一方、workspaceを次に呼び出すと統計と上限カウントはリセットされます。

### 5.5 one-shot関数とworkspaceの準備

1回だけ解くなら公開関数を直接使い、solverオブジェクトを作る必要はありません。問題と必要な制約・optionsを作った後、`solve_k_median`または`improve_k_median`を呼びます。

workspaceの構築引数は次の3つです。

|引数|型|既定値|意味|
|---|---|---|---|
|`problem`|`const facility_location_problem<Cost, Calc>&`|なし|構築後も参照する不変の問題。workspaceより長く生存させる|
|`facility_count`|`int`|なし|必須施設を含む総数K|
|`restrictions`|`const facility_location_restrictions&`|`{}`|構築時の制約。内容は内部へコピーされる|

例えば、名前付きのproblemを用意した後に`facility_location_workspace<int, long long> workspace(problem, K, restrictions);`と構築します。ここにseedやdeadlineは渡しません。呼出し時に`workspace.solve(options)`または`workspace.improve(initial, options)`へ渡します。

workspaceを作ったままproblemを変更することはできません。複数seedの例U13でコンストラクタを含む手順を示し、問題が変化する例U17では新しい探索状態を使います。workspaceは毎回one-shotより速いとは限りません。特に通常問題のone-shotには専用経路があり、workspaceは汎用経路を使います。

### 5.6 ユースケースで変わる前処理

U01は表を施設優先へ詰め、U02は代表とデータの両軸を同じID集合にします。U03は被覆表を0/1費用へ変換し、U04・U05は座標やグラフから費用表を作ります。U09〜U11はfallbackや単項費用を設定します。

U06・U08は制約を作り、U07・U08・U13・U14・U17は実行可能な初期施設集合も用意します。U12は各Kの実行可能性を確認します。U16では`Cost`と`Calc`を共に小数型にします。U17は入力更新後に初期施設IDを再確認します。

例の関数が`facility_location_problem<long long, long long>`を受け取る場合、そのproblemは本章の表の通りに準備済みであることを意味します。`int`費用表など別の型を使う場合は公開APIでその型を使うか、例の型を合わせてください。

## 6. ユースケースごとの使い方

6.1に挙げる関数・メソッドがライブラリの公開APIです。6.3以降の`solve_k_medoids`などは、8章に定義を掲載する利用例の関数名です。ヘッダをincludeするだけで追加されるAPIではないため、使う例の関数・結果型もコピーしてください。

### 6.1 公開APIの呼び分け

以下の`problem`は準備済みの`facility_location_problem<Cost, Calc>`、`options`は`facility_location_options`、`restrictions`は`facility_location_restrictions`です。探索の戻り値は全て`facility_location_result<Calc>`です。

|呼出し|各引数と意味|
|---|---|
|`solve_k_median(problem, K, options)`|全候補から新規構築する。`options`は省略可能|
|`solve_k_median(problem, K, restrictions, options)`|必須施設と許可候補を守って新規構築する。`options`は省略可能|
|`improve_k_median(problem, initial, options)`|`std::vector<int>`の初期集合から改善する。Kは`initial.size()`。`options`は省略可能|
|`improve_k_median(problem, initial, restrictions, options)`|初期集合を、指定した実行可能領域内で改善する。`options`は省略可能|
|`workspace.solve(options)`|構築時のproblem・K・restrictionsで新規構築。`options`は省略可能|
|`workspace.improve(initial, options)`|構築時と同じK個の初期集合を改善。`options`は省略可能|
|`evaluate_k_median(problem, facilities)`|非空集合の費用だけを返す。戻り値は`Calc`。探索・割当て作成は行わない|

one-shotの`improve`は初期vectorを値で受け取ります。lvalueを渡すと呼出し側のvectorは維持され、`std::move(initial)`なら所有権を渡せます。workspaceの`improve`は`const std::vector<int>&`で受け取り、内部へコピーします。

`solve_k_median(problem, K, {})`のように空の波括弧だけを3引数目へ渡すと、optionsとrestrictionsのどちらか曖昧です。通常設定なら3引数目自体を省略するか、`facility_location_options options;`のように型の分かる変数を渡します。

### 6.2 戻り値と統計の読み方

|`facility_location_result<Calc>`のフィールド|型|意味|
|---|---|---|
|`cost`|`Calc`|fallback・施設単項費用を含む総費用。型の初期値は`Calc{}`|
|`facilities`|`std::vector<int>`|選択したK個の施設ID。昇順、重複なし|
|`assignment`|`std::vector<int>`|Q需要点それぞれの利用施設ID。fallbackなら-1|
|`statistics`|`facility_location_statistics`|その1回の呼出しの探索統計|

`assignment[i]`は`facilities`配列の添字ではなく、元の候補IDです。例えば選択集合が`{2, 7}`なら、割当てにも2や7が入ります。fallback使用時の-1を、そのまま候補配列の添字にしてはいけません。同額なら実施設がfallbackより優先され、同額の実施設間では小さいIDが選ばれます。重み0の需要点にも割当てが返ります。

|統計フィールド|型・既定値|解釈|
|---|---|---|
|`candidate_evaluations`|`std::uint64_t`、0|未選択の追加候補を1-swap評価した回数|
|`accepted_swaps`|`std::uint64_t`、0|改善として受理した1-swap回数。ILSのランダム交換は含まない|
|`completed_initial_trials`|`int`、0|`solve`で初期構築と局所探索の呼出しまで戻った試行数。途中で上限に達した試行も含む。`improve`では0|
|`completed_perturbations`|`int`、0|摂動後の局所探索呼出しまで戻った回数。局所最適到達を意味しない|
|`deadline_reached`|`bool`、false|内部の時刻確認でdeadline到達を観測したか。返却時刻の事後検査ではない|
|`evaluation_limit_reached`|`bool`、false|有限の評価上限にカウントが到達したか。上限0では評価0でもtrue|

統計は探索全体の値で、最終的に返る最良解を作った1試行だけの値ではありません。`cost`は選択施設集合から再計算して返すので、同じ集合を`evaluate_k_median`で再評価できます。小数値の比較は、利用する費用尺度に応じた許容誤差で行います。

### 6.3 U01：費用表から解く

1. `service_cost[f][i]`をM行Q列で準備し、重みをQ要素または空にする。
2. 8.1の`solve_weighted_discrete_k_median(service_cost, demand_weight, K, seed)`を呼ぶ。関数内でproblemを構築し、配列を施設優先へ詰め、`options.seed`を設定して`solve_k_median(problem, K, options)`を呼ぶ。
3. `result.facilities`で元の行を選び、`result.assignment[i]`から需要iの利用先を得る。

この例の探索量はoptionsの既定値です。deadlineや評価上限も渡したい場合は、5章の通りにproblemを準備し、公開関数へoptionsを直接渡します。

### 6.4 U02：k-medoids

`dissimilarity[representative][point]`を正方行列で用意し、`solve_k_medoids(dissimilarity, point_weight, K, seed)`を呼びます。例は両軸の個数を同じに設定して、`solve_k_median(problem, K, options)`を使います。

返る`k_medoids_answer.medoids`は代表データID、`medoid_of_point`は各点の代表IDです。クラスタ連番0〜K-1が必要なら、`medoids`のIDから連番への対応を呼出し側で作ります。`cost`と`statistics`も返します。

### 6.5 U03：maximum coverage

`covers[set][element]`を0/非0の表、`demand_weight[element]`を要素の重みとして用意します。`solve_weighted_maximum_coverage(covers, demand_weight, K, seed)`は表を0/1費用へ変換してsolveします。

返る`weighted_coverage_answer`の`selected_sets`が採用集合ID、`uncovered_weight`がsolverの最小化費用、`covered_weight`が総重みとの差です。一般の`assignment`は、未被覆の要素にも何らかの施設IDを返すため、「割当てがあるから被覆済み」と判定しません。この例では被覆重みを直接返します。

### 6.6 U04：マンハッタン距離

候補と需要をそれぞれ`std::vector<manhattan_point>`で準備し、`solve_weighted_manhattan_k_median(candidate_points, demand_points, demand_weight, K, seed)`を呼びます。関数内で座標差の絶対値を足して費用表を作り、solveします。

返る施設IDは`candidate_points`の添字です。例えば`candidate_points[result.facilities[j]]`で設置座標を得ます。座標の減算、絶対値、2軸の加算、重み付き総和が`long long`の範囲内に収まることを確認してください。

### 6.7 U05：グラフ距離

`graph[v]`を`(行先頂点, 非負のint辺費用)`の列にします。無向辺なら両方向を登録します。候補頂点・需要頂点・重みを用意し、`solve_graph_k_median(graph, candidate_vertices, demand_vertices, demand_weight, K, unreachable_penalty, seed)`を呼びます。

例は候補頂点を始点としてDijkstra法を実行し、費用表を構築してsolveします。`unreachable_penalty`は到達不能な組合せの有限費用です。選択結果は`graph_k_median_answer.facility_vertices`、需要点ごとの利用先は`assigned_facility_vertex`として、元の頂点番号で返ります。到達不能でも罰則費用を払う施設が割り当てられうる点に注意します。

### 6.8 U06：制約付き新規構築

problemを準備し、必須施設を`mandatory_facilities`、追加許可候補を`allowed_facilities`として列挙します。`solve_constrained_k_median(problem, K, mandatory_facilities, allowed_facilities, options)`は、restrictionsの両フィールドを設定してから`solve_k_median(problem, K, restrictions, options)`を呼びます。

戻り値の施設集合には全必須施設が含まれ、その他は許可候補から選ばれます。`allowed_facilities`が空なら全候補を許可します。費用表は許可候補の行だけに短縮せず、problemのM×Q要素をそのまま用意します。

### 6.9 U07：初期集合から改善

problemと非空の`initial_facilities`を用意し、`improve_existing_k_median_solution(problem, initial_facilities, seed, max_perturbations, max_candidate_evaluations)`を呼びます。例はoptionsを作り、`improve_k_median(problem, initial_facilities, options)`を使います。Kを別に指定しません。

結果の`facilities`を次の初期集合にできます。初期費用を比較したいときは、呼出し前に`evaluate_k_median(problem, initial_facilities)`を使います。初期集合のID範囲・重複をまず確認してください。評価関数自体はその検査をしません。

### 6.10 U08：LNS repair

現在の全施設を`current_facilities`、その部分集合を`movable_current_facilities`、新たに入ってよい候補を`entering_candidates`として用意します。`repair_k_median_neighborhood(problem, current_facilities, movable_current_facilities, entering_candidates, seed, max_candidate_evaluations, max_perturbations)`を呼びます。

関数は現在集合から可動集合を除いたものをfixedにし、可動集合と追加候補をcandidateへまとめます。その後、制約付き`improve_k_median`を呼びます。最後の摂動数は省略時4です。返る施設数は現在集合と同じで、動かさない施設は残ります。呼出し側が渡す追加候補に重複があっても、例は一度だけ登録するようまとめます。

### 6.11 U09：fallback付きsolve

通常のproblemと、Q要素の`existing_service_cost`を用意します。`solve_with_existing_service(problem, existing_service_cost, K, options)`はproblemを値で受け取り、そのコピーの`fallback_cost`を設定してsolveします。

元のproblemは変更されません。`result.assignment[i] == -1`なら、需要iは外部サービスを使います。同額なら実施設が選ばれます。大きいproblemのコピーを避けたい場合は、利用側でfallbackを設定済みのproblemを作り、公開関数へ渡すか、以後使わないproblemを例へmoveします。

### 6.12 U10：施設単項費用付きsolve

M要素の`selection_cost`を用意し、`solve_with_facility_selection_cost(problem, selection_cost, K, options)`を呼びます。例はproblemのコピーへ`facility_cost`を設定し、solveします。渡したproblemにfallbackがあれば、その条件も使います。

返る`cost`には、選んだK施設の単項費用がそれぞれ1回含まれます。`assignment`だけから施設費用を集計すると、需要を担当しない選択施設の費用を落とすので、`facilities`から集計します。

### 6.13 U11：追加効果を返す

`solve_facility_augmentation(problem, existing_service_cost, new_facility_cost, added_facility_count, restrictions, options)`を呼びます。必須なのはQ要素の現状費用、M要素の新設費用、追加個数です。最後のrestrictionsとoptionsは省略できます。

例は現状費用の重み付き和を`baseline_cost`として計算し、fallbackと施設単項費用を設定してsolveします。返る`facility_augmentation_answer.net_improvement`は`baseline_cost - solution.cost`です。`solution`には施設・割当て・統計も入ります。K=0の「何もしない」案はsolverへ渡さず、このbaselineと外側で比較します。

### 6.14 U12：Kを外側で列挙

`solve_best_facility_count(problem, min_facility_count, max_facility_count, restrictions, seed, evaluations_per_k)`を呼びます。例は範囲内のKについてoptionsと評価上限を設定し、制約付きsolveを順に呼びます。各Kが実行可能であることが入力条件です。

返る`variable_k_median_answer.facility_count`が選んだK、`solution`がその解です。同費用なら小さいKを選びます。評価上限はKごとであり、全Kの合計上限ではありません。結果の統計も勝った1呼出しの値で、全Kの累積統計ではありません。

### 6.15 U13：workspaceと複数seed

problem・初期集合・restrictionsを準備し、`repair_with_multiple_seeds(problem, initial_facilities, restrictions, seeds, max_perturbations_per_seed, evaluations_per_seed)`を呼びます。`seeds`は非空にします。

関数内で`facility_location_workspace<long long, long long> workspace(problem, initial_facilities.size(), restrictions)`を、個数をintへ変換して1回構築します。その後、各seedのoptionsを作り、`workspace.improve(initial_facilities, options)`を繰り返して最良結果を保存します。

全試行は同じ初期集合から始まります。返る統計は最良結果を出した試行の値です。試行ごとの全統計や費用を分析したい場合は、ループ内で別の配列へ保存します。反復中にproblem・K・restrictionsを変更しないでください。

### 6.16 U14：絶対deadlineで改善

外側で`std::chrono::steady_clock::now()`を基準に終了時刻を決め、solver用の停止目標を`solver_deadline`とします。`repair_until_deadline(problem, initial_facilities, restrictions, solver_deadline, seed)`はその絶対時刻をoptionsへ設定し、制約付きimproveを呼びます。

返る`statistics.deadline_reached`で、内部の時刻確認が上限を観測したか分かります。ただしfalseでも返却時点の上限超過がないとは限りません。8.14では外側で終了時刻から時間を引いて呼ぶ例もコメントに示します。

### 6.17 U15：評価上限付きsolve

`solve_with_evaluation_budget(problem, K, restrictions, seed, initial_trials, max_perturbations, max_candidate_evaluations)`を呼びます。例はdeadlineを無制限のままにし、指定した探索量をoptionsへ設定してsolveします。

戻り値の`statistics.candidate_evaluations`は上限以下になります。評価上限0でも初期施設集合が構築され、設定した初期試行数の構築を行うことがあります。固定仕事量を比べるときは評価上限だけでなく初期試行数・摂動数・入力・seedもそろえます。

### 6.18 U16：小数の非対称費用

`std::vector<std::vector<double>> directed_service_cost`を候補×需要で用意し、`solve_asymmetric_service_loss(directed_service_cost, K, options)`を呼びます。例は`facility_location_problem<double, double>`を構築して、費用を整数へ切り捨てずにsolveします。

戻り値は`facility_location_result<double>`です。`cost`が小数、施設・割当ては整数IDです。小数費用はNaN・無限大を含めず、誤差を伴う比較の性質について7章も確認してください。

### 6.19 U17：新しい問題で前の解を改善

更新済みproblemと実行可能な`previous_facilities`を用意し、`improve_on_next_turn(new_problem, previous_facilities, new_restrictions, options)`を呼びます。関数は新しい問題で前の集合の費用を再評価し、新しい探索状態を作るone-shotの`improve_k_median`へ渡します。

返る`turn_update_answer.previous_cost_in_new_problem`が同じ新条件での比較基準、`solution`が改善後の結果です。前ターンの古い`result.cost`は比較に使いません。候補IDが変わった場合は、呼出し前に対応付け直し、必須・候補制限にも合う集合へ整えてください。

## 7. 制約・注意点

### 7.1 入力の形と実行可能性

|注意点|失敗を避ける方法|
|---|---|
|Q=0、M=0、K=0は使えない|いずれも1以上。空の問題や施設追加なしは呼出し側で処理する|
|Kが候補数を超える|必須集合と許可集合の和集合からK個選べることを確認する|
|costの軸の取り違え|`cost[f * Q + i]`。施設優先でM×Q要素を用意する|
|任意配列の長さ違い|重み・fallbackは空かQ要素、施設単項費用は空かM要素|
|初期集合内の重複|同じIDを複数回選ばない。Kはユニークな施設の個数|
|restrictions内の重複|fixed内とcandidate内は各々重複不可。両配列間の重複は可能|
|candidateを空にすると候補ゼロだと思う|空は全候補。全施設を固定したいならfixedをK個にする|
|初期解が制約外|全fixedを含み、初期施設がfixedまたは許可候補であることを確認する|

ライブラリの寸法・ID・重複などの検査は`assert`です。`-DNDEBUG`のビルドでは無効になるので、入力契約違反をエラーとして返すAPIではありません。不正入力が安全に扱われることを前提にしません。

`evaluate_k_median`は、配列の寸法と施設集合が非空であることなどを検査しますが、施設IDの範囲や重複は検査しません。範囲外IDは不正アクセス、重複は単項費用の二重計上などにつながります。またrestrictionsを受け取らないので、その集合の制約違反も判定しません。

### 7.2 数値の条件

|注意点|理由と対応|
|---|---|
|非負でないサービス費用・重み|この実装の前提外。負値を使えるのは施設単項費用|
|NaN・無限大|比較・最小値・抽選が意味を失う。到達不能は有限罰則かモデルの作り直しで扱う|
|合計だけが型に収まればよいと思う|重みとの積、改善差分、削除補正、負号反転などの中間値も範囲内にする|
|最小の負整数を施設単項費用にする|削除補正で負号反転するため表現範囲外になりうる|
|`Calc`が符号なし|負の改善差分を表せない。符号付き整数か浮動小数を使う|
|`Cost=double`、`Calc=long long`|費用が整数へ切り捨てられる。小数の計算型を明示する|
|極端に大きい有限値|抽選では`long double`で重み×費用の二乗和を計算する。その計算も有限に収める|

ライブラリはoverflowを検査しません。単に`LLONG_MAX`などを「到達不能」として格納すると、重み付き演算で壊れます。意味と値域の両方を確認した罰則費用を選びます。`numeric_limits<Cost>::max()`は内部の未設定距離にも使われるので、通常の費用は十分余裕を持った値域にします。

小数の改善受理は厳密な`delta < 0`で、epsilonの設定項目はありません。加算誤差による微小差、厳密な同費用判定、丸めの違いに注意します。整数で表せる費用なら整数を使う方が検証しやすくなります。小数を定数倍して整数へ丸める方法は目的関数そのものを変えうるので、精度を決めずに行いません。

### 7.3 時間と探索量

`deadline`は厳密な終了保証ではありません。最初の施設を全候補から比較する走査、最近傍再構築、最終割当て作成などは途中で中断しません。局所探索の時刻確認も毎命令・毎候補ではありません。過去のdeadlineを渡しても、実行可能な集合を作る・初期集合を評価する・結果を返す処理は行われます。

`max_candidate_evaluations=0`は「関数を何もしないで返す」指定ではありません。初期構築を避けて既存集合を使いたいなら`improve`を選びます。`initial_trials`は`improve`でも1以上、`max_perturbations`は-1以上が必要です。

同じ入力・seed・固定仕事量・同じ実行環境なら再現比較を行えます。一方、時刻制限では終了位置が実行速度や負荷に依存し、同じseedでも結果が変わりえます。標準ライブラリやコンパイラをまたぐ乱数操作の完全一致も保証しません。

### 7.4 workspaceとメモリ

workspaceに結び付けたproblemは、workspaceより長く生存し、内容と所在を維持する必要があります。費用表の1要素だけの変更も、サポートされた更新操作ではありません。problem・K・restrictionsを変える場合は、新しいworkspaceかone-shot関数を使います。

候補poolを絞っても、problemは全M×Q要素を保持します。費用表だけで`M * Q * sizeof(Cost)`バイト必要です。例えばM=Q=10000、Costが4バイトなら約400MBです。さらに探索用配列が必要です。全表を持てない場合は、外側で候補IDを圧縮して小さい問題を構築し、結果を元IDへ戻すなど、入力表現を検討します。

重みが全て1なら`demand_weight`を空にでき、fallbackや施設単項費用が不要ならその配列も空にします。0で埋めた単項費用配列は、空配列と目的値は同じでも同じ実装経路とは限りません。どちらも仕様上有効ですが、不要な機能は空で表すと準備も簡単です。

### 7.5 表現できない制約・目的

|対象外の条件|なぜそのまま使えないか|
|---|---|
|施設容量、クラスタ人数上限・下限|需要点間の割当てが結合する|
|担当先の強制、需要分割|割当ては選択施設とfallbackの最小費用で自動的に決まる|
|施設間の相性、同時選択禁止、連結性|施設ごとの単項費用だけでは選択集合間の関係を表せない|
|巡回路・車両経路|各需要への独立費用の和だけでは経路全体の費用にならない|
|最大距離の最小化|本ライブラリは重み付き合計を最小化する|
|連続座標上での自由な施設設置|既知の離散候補IDから選ぶ。座標は動かさない|
|自動の施設数増減|1回のsolveではKが固定。外側で比較する|
|動的グラフの最短路更新|グラフを入力として保持するAPIがない|
|到達不能の厳密な禁止|有限罰則はコストであって実行可能性の制約ではない|
|施設移設の対応付け費用|どの旧施設をどの新施設へ移すかを変数として持たない|

facility costを負にしても、上記の集合間相互作用は表現できません。また、このライブラリの目的値が外側の問題のスコアと同じとは限りません。代理問題として使う場合は、施設集合を戻した後に外側の制約とスコアも評価します。

### 7.6 最適性と返却結果

時間を十分に与えても大域最適性の証明は返りません。局所探索を途中で止めた結果は、1-swap局所最適とも限りません。返る統計から「探索回数が多いので最適」とは判定できません。

fallbackが選ばれた需要点の割当ては-1、実施設と同額なら実施設、同額の実施設間は小さいIDです。施設集合は昇順で返るので、初期集合の並び順は保持されません。これらを利用側のID変換・配列アクセスに反映してください。

## 8. ユースケースごとのコード例

各コードブロックは、必要な結果型・補助処理を含む独立したsolver例です。使うブロックをヘッダと一緒にコピーできます。他のユースケースのhelper関数には依存しません。補助処理が必要なグラフ例では関数内lambdaに閉じ込めています。

problemを引数で受ける例は、5章の通りに費用表・重みなどを準備して渡します。表や座標を受ける例は、関数内でproblemを構築します。どちらも入力値の有効性と数値演算の値域は利用側の前提です。各関数の直前に、配列の軸、個数、IDの意味、既定値、返却値をコメントしています。

同梱の`facility_location_guide_examples_v11.cpp`には、本章の全17ブロックと実行確認用mainを収録しています。提出へ取り込むときは、必要な関数・結果型だけを使ってください。

### 8.1 U01：重み付き費用表

```cpp
#include "facility_location_v11.hpp"

// U01: 候補×需要の費用表からK施設を選ぶ。
// 入力 service_cost[f][i]: M行Q列、M,Q>0、有限・非負の整数費用。
//      demand_weight: 空なら全て1、指定時はQ個の非負重み。
//      facility_count: 1..M。seedは省略時1、他の探索量は既定値。
// 出力 cost: 重み付き費用。facilities: 選択行ID。
//      assignment[i]: 需要iの利用行ID。入力の行IDをそのまま使う。
// 前提: 重みとの積・総和・差分がlong longに収まる。
facility_location_result<long long> solve_weighted_discrete_k_median(
    const std::vector<std::vector<long long>>& service_cost,
    const std::vector<long long>& demand_weight,
    int facility_count,
    std::uint64_t seed = 1) {
    const int candidate_count = static_cast<int>(service_cost.size());
    assert(candidate_count > 0);
    const int demand_count = static_cast<int>(service_cost[0].size());
    assert(demand_count > 0);
    assert(demand_weight.empty() ||
           static_cast<int>(demand_weight.size()) == demand_count);

    facility_location_problem<long long, long long> problem;
    problem.demand_count = demand_count;
    problem.candidate_count = candidate_count;
    problem.demand_weight = demand_weight;
    problem.cost.resize(
        static_cast<std::size_t>(candidate_count) * demand_count);

    for (int facility = 0; facility < candidate_count; ++facility) {
        assert(static_cast<int>(service_cost[facility].size()) == demand_count);
        for (int demand = 0; demand < demand_count; ++demand) {
            problem.cost[static_cast<std::size_t>(facility) * demand_count + demand]
                = service_cost[facility][demand];
        }
    }

    facility_location_options options;
    options.seed = seed;
    return solve_k_median(problem, facility_count, options);
}
```

### 8.2 U02：k-medoids

```cpp
#include "facility_location_v11.hpp"

// U02: 入力データそのものからK代表を選ぶk-medoids。
// 入力 dissimilarity[r][i]: 代表rから点iへの非負非類似度、N行N列。
//      point_weight: 空なら全て1、それ以外はN個。medoid_count: 1..N。
//      seed: 省略時1。費用の積・総和・差分はlong longの範囲内。
// 出力 medoids: 代表点ID。medoid_of_point[i]: 点iを担当する代表点ID。
//      medoid_of_pointは0..K-1のクラスタ連番ではない。
struct k_medoids_answer {
    long long cost;
    std::vector<int> medoids;
    std::vector<int> medoid_of_point;
    facility_location_statistics statistics;
};

k_medoids_answer solve_k_medoids(
    const std::vector<std::vector<long long>>& dissimilarity,
    const std::vector<long long>& point_weight,
    int medoid_count,
    std::uint64_t seed = 1) {
    const int point_count = static_cast<int>(dissimilarity.size());
    assert(point_count > 0);
    assert(point_weight.empty() ||
           static_cast<int>(point_weight.size()) == point_count);

    facility_location_problem<long long, long long> problem;
    problem.demand_count = point_count;
    problem.candidate_count = point_count;
    problem.demand_weight = point_weight;
    problem.cost.resize(
        static_cast<std::size_t>(point_count) * point_count);
    for (int medoid = 0; medoid < point_count; ++medoid) {
        assert(static_cast<int>(dissimilarity[medoid].size()) == point_count);
        for (int point = 0; point < point_count; ++point) {
            problem.cost[static_cast<std::size_t>(medoid) * point_count + point]
                = dissimilarity[medoid][point];
        }
    }

    facility_location_options options;
    options.seed = seed;
    const auto result = solve_k_median(problem, medoid_count, options);

    k_medoids_answer answer;
    answer.cost = result.cost;
    answer.medoids = result.facilities;
    answer.medoid_of_point = result.assignment;
    answer.statistics = result.statistics;
    return answer;
}
```

### 8.3 U03：重み付きmaximum coverage

```cpp
#include "facility_location_v11.hpp"

// U03: K集合による被覆重みを最大化する。
// 入力 covers[s][i]: 集合sが要素iを覆うなら非0、覆わないなら0。
//      M行Q列。demand_weightはQ>0個の非負重み（省略不可）。
//      selected_set_count: 1..M、seed: 省略時1。
// 出力 selected_sets: 集合ID。covered_weight: 被覆重み。
//      uncovered_weight: 未被覆重み。総重みはlong longの範囲内。
struct weighted_coverage_answer {
    long long covered_weight;
    long long uncovered_weight;
    std::vector<int> selected_sets;
};

weighted_coverage_answer solve_weighted_maximum_coverage(
    const std::vector<std::vector<unsigned char>>& covers,
    const std::vector<long long>& demand_weight,
    int selected_set_count,
    std::uint64_t seed = 1) {
    const int candidate_count = static_cast<int>(covers.size());
    assert(candidate_count > 0);
    const int demand_count = static_cast<int>(demand_weight.size());
    assert(demand_count > 0);

    facility_location_problem<int, long long> problem;
    problem.demand_count = demand_count;
    problem.candidate_count = candidate_count;
    problem.demand_weight = demand_weight;
    problem.cost.resize(
        static_cast<std::size_t>(candidate_count) * demand_count);
    for (int facility = 0; facility < candidate_count; ++facility) {
        assert(static_cast<int>(covers[facility].size()) == demand_count);
        for (int demand = 0; demand < demand_count; ++demand) {
            problem.cost[static_cast<std::size_t>(facility) * demand_count + demand]
                = covers[facility][demand] ? 0 : 1;
        }
    }

    facility_location_options options;
    options.seed = seed;
    const auto result = solve_k_median(problem, selected_set_count, options);
    const long long total_weight = std::accumulate(
        demand_weight.begin(), demand_weight.end(), 0LL);

    weighted_coverage_answer answer;
    answer.covered_weight = total_weight - result.cost;
    answer.uncovered_weight = result.cost;
    answer.selected_sets = result.facilities;
    return answer;
}
```

### 8.4 U04：マンハッタン距離

```cpp
#include "facility_location_v11.hpp"

// U04: 離散座標候補からマンハッタン距離の小さいK拠点を選ぶ。
// 入力 candidate_points: M>0個の候補座標、demand_points: Q>0個の需要座標。
//      demand_weight: 空またはQ個。facility_count: 1..M、seed: 省略時1。
// 出力 facilitiesとassignmentはcandidate_pointsの添字。座標自体は動かさない。
// 前提: 座標差、その絶対値、2軸の和、重み付き和・差分がlong long内。
struct manhattan_point {
    long long x;
    long long y;
};

facility_location_result<long long> solve_weighted_manhattan_k_median(
    const std::vector<manhattan_point>& candidate_points,
    const std::vector<manhattan_point>& demand_points,
    const std::vector<long long>& demand_weight,
    int facility_count,
    std::uint64_t seed = 1) {
    const int candidate_count = static_cast<int>(candidate_points.size());
    const int demand_count = static_cast<int>(demand_points.size());
    assert(candidate_count > 0 && demand_count > 0);
    assert(demand_weight.empty() ||
           static_cast<int>(demand_weight.size()) == demand_count);

    facility_location_problem<long long, long long> problem;
    problem.demand_count = demand_count;
    problem.candidate_count = candidate_count;
    problem.demand_weight = demand_weight;
    problem.cost.resize(
        static_cast<std::size_t>(candidate_count) * demand_count);

    for (int facility = 0; facility < candidate_count; ++facility) {
        for (int demand = 0; demand < demand_count; ++demand) {
            const long long dx = std::abs(candidate_points[facility].x -
                                          demand_points[demand].x);
            const long long dy = std::abs(candidate_points[facility].y -
                                          demand_points[demand].y);
            problem.cost[static_cast<std::size_t>(facility) * demand_count + demand]
                = dx + dy;
        }
    }

    facility_location_options options;
    options.seed = seed;
    return solve_k_median(problem, facility_count, options);
}
```

### 8.5 U05：グラフ最短路

```cpp
#include "facility_location_v11.hpp"

// U05: 非負辺の有向グラフ上でK拠点を選ぶ。
// 入力 graph[v]: (行先頂点, 非負int辺費用)の列。無向辺は両方向を登録。
//      candidate_vertices: 重複なしの候補頂点ID、demand_vertices: 需要頂点ID。
//      どの頂点IDも0..graph.size()-1。候補・需要はともに非空。
//      demand_weight: 空または需要個数分。facility_count: 1..候補数。
//      unreachable_penalty: 到達不能な組合せの有限・非負費用。
// 出力 facility_vertices、assigned_facility_vertex: 元グラフの頂点番号。
// 注意: 距離の向きは候補から需要へ。到達不能の割当てを禁止する例ではない。
// 前提: 有限の最短路長と罰則はLLONG_MAX/4未満。重み付き演算も範囲内。
struct graph_k_median_answer {
    long long cost;
    std::vector<int> facility_vertices;
    std::vector<int> assigned_facility_vertex;
    facility_location_statistics statistics;
};

graph_k_median_answer solve_graph_k_median(
    const std::vector<std::vector<std::pair<int, int>>>& graph,
    const std::vector<int>& candidate_vertices,
    const std::vector<int>& demand_vertices,
    const std::vector<long long>& demand_weight,
    int facility_count,
    long long unreachable_penalty,
    std::uint64_t seed = 1) {
    const int vertex_count = static_cast<int>(graph.size());
    const int candidate_count = static_cast<int>(candidate_vertices.size());
    const int demand_count = static_cast<int>(demand_vertices.size());
    assert(vertex_count > 0 && candidate_count > 0 && demand_count > 0);
    assert(demand_weight.empty() ||
           static_cast<int>(demand_weight.size()) == demand_count);
    assert(unreachable_penalty >= 0);

    const long long inf = std::numeric_limits<long long>::max() / 4;
    assert(unreachable_penalty < inf);
    {
        auto sorted_candidates = candidate_vertices;
        std::sort(sorted_candidates.begin(), sorted_candidates.end());
        assert(std::adjacent_find(sorted_candidates.begin(), sorted_candidates.end()) ==
               sorted_candidates.end());
    }
    const auto dijkstra = [&](int source) {
        std::vector<long long> distance(vertex_count, inf);
        using queue_entry = std::pair<long long, int>;
        std::priority_queue<queue_entry, std::vector<queue_entry>,
                            std::greater<queue_entry>> queue;
        distance[source] = 0;
        queue.emplace(0, source);
        while (!queue.empty()) {
            const auto [current_distance, vertex] = queue.top();
            queue.pop();
            if (current_distance != distance[vertex]) continue;
            for (const auto& [next_vertex, edge_cost] : graph[vertex]) {
                assert(0 <= next_vertex && next_vertex < vertex_count);
                assert(edge_cost >= 0);
                const long long next_distance = current_distance + edge_cost;
                if (next_distance < distance[next_vertex]) {
                    distance[next_vertex] = next_distance;
                    queue.emplace(next_distance, next_vertex);
                }
            }
        }
        return distance;
    };

    facility_location_problem<long long, long long> problem;
    problem.demand_count = demand_count;
    problem.candidate_count = candidate_count;
    problem.demand_weight = demand_weight;
    problem.cost.resize(
        static_cast<std::size_t>(candidate_count) * demand_count);

    for (int facility = 0; facility < candidate_count; ++facility) {
        const int source = candidate_vertices[facility];
        assert(0 <= source && source < vertex_count);
        const std::vector<long long> distance = dijkstra(source);
        for (int demand = 0; demand < demand_count; ++demand) {
            const int vertex = demand_vertices[demand];
            assert(0 <= vertex && vertex < vertex_count);
            problem.cost[static_cast<std::size_t>(facility) * demand_count + demand]
                = distance[vertex] == inf ? unreachable_penalty : distance[vertex];
        }
    }

    facility_location_options options;
    options.seed = seed;
    const auto result = solve_k_median(problem, facility_count, options);

    graph_k_median_answer answer;
    answer.cost = result.cost;
    answer.statistics = result.statistics;
    answer.facility_vertices.reserve(result.facilities.size());
    for (int facility : result.facilities) {
        answer.facility_vertices.push_back(candidate_vertices[facility]);
    }
    answer.assigned_facility_vertex.resize(demand_count);
    for (int demand = 0; demand < demand_count; ++demand) {
        answer.assigned_facility_vertex[demand]
            = candidate_vertices[result.assignment[demand]];
    }
    return answer;
}
```

### 8.6 U06：必須施設・候補制限

```cpp
#include "facility_location_v11.hpp"

// U06: 必須施設と許可候補を守ってK施設を新規構築する。
// 入力 problem: M×Q費用表などを5章の契約に従って準備済み。
//      mandatory_facilities: 必ず含めるID。allowed_facilities: 許可ID、空なら全候補。
//      各列内は重複不可、列間の重複は可能。IDは0..M-1。
//      facility_count: 必須施設も含むK。和集合からK個選べること。
//      options: 省略時はライブラリの既定値。
// 出力: 全必須施設を含むK施設、需要ごとの利用先、総費用、統計。
facility_location_result<long long> solve_constrained_k_median(
    const facility_location_problem<long long, long long>& problem,
    int facility_count,
    const std::vector<int>& mandatory_facilities,
    const std::vector<int>& allowed_facilities,
    facility_location_options options = {}) {
    facility_location_restrictions restrictions;
    restrictions.fixed_facilities = mandatory_facilities;
    restrictions.candidate_facilities = allowed_facilities;
    return solve_k_median(problem, facility_count, restrictions, options);
}
```

### 8.7 U07：既存解改善

```cpp
#include "facility_location_v11.hpp"

// U07: 準備済みの施設集合から改善する。初期vectorは変更しない。
// 入力 problem: 準備済み。initial_facilities: 重複なしの有効IDを1..M個。
//      seed、max_perturbations（-1以上）、max_candidate_evaluationsを明示。
//      deadlineは無制限。max_perturbations=-1なら32回が上限。
// 出力: 初期集合と同じ個数の施設とその費用・割当て・統計。
facility_location_result<long long> improve_existing_k_median_solution(
    const facility_location_problem<long long, long long>& problem,
    const std::vector<int>& initial_facilities,
    std::uint64_t seed,
    int max_perturbations,
    std::uint64_t max_candidate_evaluations) {
    facility_location_options options;
    options.seed = seed;
    options.initial_trials = 1;
    options.max_perturbations = max_perturbations;
    options.max_candidate_evaluations = max_candidate_evaluations;
    return improve_k_median(problem, initial_facilities, options);
}
```

### 8.8 U08：LNS repair

```cpp
#include "facility_location_v11.hpp"

// U08: 現在解の一部だけを動かすLNS repair。
// 入力 problem: 準備済み。current_facilities: 重複なしの非空な現在集合。
//      movable_current_facilities: 現在集合の重複なし部分集合。
//      entering_candidates: 新たに入ってよい有効ID。重複はこの例でまとめる。
//      seed、評価上限を指定。max_perturbationsは省略時4（-1以上）。
// 出力: currentと同じ個数で、movable以外の現在施設を全て保持する解。
//      可動の現在施設も許可候補へ入れるため、元の解は実行可能なまま。
facility_location_result<long long> repair_k_median_neighborhood(
    const facility_location_problem<long long, long long>& problem,
    const std::vector<int>& current_facilities,
    const std::vector<int>& movable_current_facilities,
    const std::vector<int>& entering_candidates,
    std::uint64_t seed,
    std::uint64_t max_candidate_evaluations,
    int max_perturbations = 4) {
    const int candidate_count = problem.candidate_count;
    std::vector<unsigned char> selected(candidate_count, 0);
    std::vector<unsigned char> movable(candidate_count, 0);
    std::vector<unsigned char> allowed(candidate_count, 0);

    for (int facility : current_facilities) {
        assert(0 <= facility && facility < candidate_count);
        assert(!selected[facility]);
        selected[facility] = 1;
    }
    for (int facility : movable_current_facilities) {
        assert(0 <= facility && facility < candidate_count);
        assert(selected[facility] && !movable[facility]);
        movable[facility] = 1;
        allowed[facility] = 1;
    }
    for (int facility : entering_candidates) {
        assert(0 <= facility && facility < candidate_count);
        allowed[facility] = 1;
    }

    facility_location_restrictions restrictions;
    for (int facility : current_facilities) {
        if (!movable[facility]) {
            restrictions.fixed_facilities.push_back(facility);
        }
    }
    for (int facility = 0; facility < candidate_count; ++facility) {
        if (allowed[facility]) {
            restrictions.candidate_facilities.push_back(facility);
        }
    }

    facility_location_options options;
    options.seed = seed;
    options.initial_trials = 1;
    options.max_perturbations = max_perturbations;
    options.max_candidate_evaluations = max_candidate_evaluations;
    return improve_k_median(
        problem, current_facilities, restrictions, options);
}
```

### 8.9 U09：fallback

```cpp
#include "facility_location_v11.hpp"

// U09: 外部サービスと新設K施設を需要点ごとに使い分ける。
// 入力 problem: 準備済み。値渡しなのでlvalue入力はコピーされる。
//      existing_service_cost: Q個の有限・非負fallback費用（既存値を置き換える）。
//      facility_count: 新設候補から選ぶ個数1..M。optionsは省略可能。
// 出力: costは外部サービス分も含む。assignment=-1はfallback利用。
//      問題に元から設定した重み・施設単項費用はそのまま使う。
facility_location_result<long long> solve_with_existing_service(
    facility_location_problem<long long, long long> problem,
    const std::vector<long long>& existing_service_cost,
    int facility_count,
    facility_location_options options = {}) {
    assert(static_cast<int>(existing_service_cost.size()) ==
           problem.demand_count);
    problem.fallback_cost = existing_service_cost;
    return solve_k_median(problem, facility_count, options);
}
```

### 8.10 U10：施設単項費用

```cpp
#include "facility_location_v11.hpp"

// U10: 地点別の単項費用・報酬を含めてK施設を選ぶ。
// 入力 problem: 準備済み、lvalueならコピー。
//      selection_cost: M個の有限・符号付き単項費用（既存値を置き換える）。
//      facility_count: 1..M。optionsは省略可能。
// 出力 cost: サービス費用と選択した施設の単項費用の合計。
// 前提: 単項費用の負号反転と差分もlong longに収まる。
facility_location_result<long long> solve_with_facility_selection_cost(
    facility_location_problem<long long, long long> problem,
    const std::vector<long long>& selection_cost,
    int facility_count,
    facility_location_options options = {}) {
    assert(static_cast<int>(selection_cost.size()) ==
           problem.candidate_count);
    problem.facility_cost = selection_cost;
    return solve_k_median(problem, facility_count, options);
}
```

### 8.11 U11：追加の純改善量

```cpp
#include "facility_location_v11.hpp"

// U11: 新設施設の費用も引いた、現状からの純改善量を求める。
// 入力 problem: 新設候補だけをM行で用意。lvalueならコピー。
//      existing_service_cost: Q個の現状サービス費用。
//      new_facility_cost: M個の新設単項費用。0費用もM要素で渡す。
//      added_facility_count: 追加総数1..M。restrictionsの必須新設施設も含む。
//      restrictions、optionsは省略可能。重み付き和・差分がlong long内。
// 出力 baseline_cost: 現状のみの費用。net_improvement: 正なら純改善。
//      solution: 追加後の解。既存サービスの施設はKに数えない。
struct facility_augmentation_answer {
    facility_location_result<long long> solution;
    long long baseline_cost;
    long long net_improvement;
};

facility_augmentation_answer solve_facility_augmentation(
    facility_location_problem<long long, long long> problem,
    const std::vector<long long>& existing_service_cost,
    const std::vector<long long>& new_facility_cost,
    int added_facility_count,
    const facility_location_restrictions& restrictions = {},
    facility_location_options options = {}) {
    assert(static_cast<int>(existing_service_cost.size()) ==
           problem.demand_count);
    assert(static_cast<int>(new_facility_cost.size()) ==
           problem.candidate_count);

    long long baseline_cost = 0;
    for (int demand = 0; demand < problem.demand_count; ++demand) {
        const long long weight = problem.demand_weight.empty()
                                     ? 1
                                     : problem.demand_weight[demand];
        baseline_cost += weight * existing_service_cost[demand];
    }

    problem.fallback_cost = existing_service_cost;
    problem.facility_cost = new_facility_cost;
    auto solution = solve_k_median(
        problem, added_facility_count, restrictions, options);

    facility_augmentation_answer answer;
    answer.baseline_cost = baseline_cost;
    answer.net_improvement = baseline_cost - solution.cost;
    answer.solution = std::move(solution);
    return answer;
}
```

### 8.12 U12：Kの外側列挙

```cpp
#include "facility_location_v11.hpp"

// U12: 複数のKを解き、総費用が最小の結果を選ぶ。
// 入力 problem: 各Kを同じ尺度で比較できる費用表・設置費などを準備。
//      min_facility_count..max_facility_count: 1..Mの閉区間。
//      restrictions: 区間内の全Kが実行可能であること。
//      seed、evaluations_per_k: 各Kでのseedと候補評価上限。
// 出力 facility_count: 選んだK。同費用なら小さいK。solution: その解。
//      統計は選ばれた1呼出しの値。全Kの最適性は保証しない。
struct variable_k_median_answer {
    int facility_count;
    facility_location_result<long long> solution;
};

variable_k_median_answer solve_best_facility_count(
    const facility_location_problem<long long, long long>& problem,
    int min_facility_count,
    int max_facility_count,
    const facility_location_restrictions& restrictions,
    std::uint64_t seed,
    std::uint64_t evaluations_per_k) {
    assert(1 <= min_facility_count);
    assert(min_facility_count <= max_facility_count);
    assert(max_facility_count <= problem.candidate_count);

    variable_k_median_answer best;
    best.facility_count = -1;
    for (int facility_count = min_facility_count;
         facility_count <= max_facility_count; ++facility_count) {
        facility_location_options options;
        options.seed = seed;
        options.max_candidate_evaluations = evaluations_per_k;
        const auto solution = solve_k_median(
            problem, facility_count, restrictions, options);
        if (best.facility_count == -1 || solution.cost < best.solution.cost ||
            (solution.cost == best.solution.cost &&
             facility_count < best.facility_count)) {
            best.facility_count = facility_count;
            best.solution = solution;
        }
    }
    return best;
}
```

### 8.13 U13：workspaceと複数seed

```cpp
#include "facility_location_v11.hpp"

// U13: 不変の問題を同じ初期集合から複数seedで改善し、最良を返す。
// 入力 problem: 関数実行中に変更しない準備済み問題。
//      initial_facilities: 制約を満たす非空な初期集合、restrictions: 共通制約。
//      seeds: 非空のseed列。max_perturbations_per_seed: -1以上。
//      evaluations_per_seed: 各試行の候補評価上限。
// 出力: 最良の解。statisticsはその試行のみ。各試行は同じinitialから開始。
//      workspaceの構築もこの関数内で行い、返却時に破棄する。
facility_location_result<long long> repair_with_multiple_seeds(
    const facility_location_problem<long long, long long>& problem,
    const std::vector<int>& initial_facilities,
    const facility_location_restrictions& restrictions,
    const std::vector<std::uint64_t>& seeds,
    int max_perturbations_per_seed,
    std::uint64_t evaluations_per_seed) {
    assert(!seeds.empty());
    facility_location_workspace<long long, long long> workspace(
        problem, static_cast<int>(initial_facilities.size()), restrictions);

    facility_location_result<long long> best;
    bool has_best = false;
    for (std::uint64_t seed : seeds) {
        facility_location_options options;
        options.seed = seed;
        options.initial_trials = 1;
        options.max_perturbations = max_perturbations_per_seed;
        options.max_candidate_evaluations = evaluations_per_seed;
        const auto result = workspace.improve(initial_facilities, options);
        if (!has_best || result.cost < best.cost) {
            best = result;
            has_best = true;
        }
    }
    return best;
}
```

### 8.14 U14：絶対deadline

```cpp
#include "facility_location_v11.hpp"

// U14: 外側で定めた絶対時刻を目標に、初期施設集合を改善する。
// 入力 problemとinitial_facilities、restrictions: 準備済み・実行可能。
//      solver_deadline: steady_clockの絶対時刻、seed: 今回のseed。
// 例: global_endを外側で作り、global_end - std::chrono::milliseconds(10)
//     を渡す。10msは説明用で、入力に応じた後処理余裕を実測して決める。
// 出力: 最良施設集合・費用・割当て・統計。厳密な返却時刻の保証ではない。
facility_location_result<long long> repair_until_deadline(
    const facility_location_problem<long long, long long>& problem,
    const std::vector<int>& initial_facilities,
    const facility_location_restrictions& restrictions,
    std::chrono::steady_clock::time_point solver_deadline,
    std::uint64_t seed) {
    facility_location_options options;
    options.deadline = solver_deadline;
    options.seed = seed;
    options.initial_trials = 1;
    options.max_perturbations = -1;
    return improve_k_median(
        problem, initial_facilities, restrictions, options);
}
```

### 8.15 U15：候補評価上限

```cpp
#include "facility_location_v11.hpp"

// U15: 時刻制限を使わず、初期試行・摂動・評価回数で探索量を制御する。
// 入力 problem、K、restrictions: 準備済み・実行可能。
//      seed、initial_trials>=1、max_perturbations>=-1、評価上限を明示。
// 出力: result.statistics.candidate_evaluationsは指定上限以下。
//      初期解構築や返却処理は評価回数に含まれず、上限0でも実行される。
facility_location_result<long long> solve_with_evaluation_budget(
    const facility_location_problem<long long, long long>& problem,
    int facility_count,
    const facility_location_restrictions& restrictions,
    std::uint64_t seed,
    int initial_trials,
    int max_perturbations,
    std::uint64_t max_candidate_evaluations) {
    facility_location_options options;
    options.seed = seed;
    options.initial_trials = initial_trials;
    options.max_perturbations = max_perturbations;
    options.max_candidate_evaluations = max_candidate_evaluations;
    return solve_k_median(problem, facility_count, restrictions, options);
}
```

### 8.16 U16：小数・非対称費用

```cpp
#include "facility_location_v11.hpp"

// U16: 小数の非対称サービス損失を、小数のまま最小化する。
// 入力 directed_service_cost[f][i]: M行Q列、M,Q>0、有限・非負double。
//      facility_count: 1..M、optionsは省略可能。重みは全て1。
// 出力: costはdouble、facilitiesとassignmentは元の候補行ID。
//      対称性・三角不等式は不要。小数の合計・差分・二乗和も有限にする。
facility_location_result<double> solve_asymmetric_service_loss(
    const std::vector<std::vector<double>>& directed_service_cost,
    int facility_count,
    facility_location_options options = {}) {
    const int candidate_count =
        static_cast<int>(directed_service_cost.size());
    assert(candidate_count > 0);
    const int demand_count =
        static_cast<int>(directed_service_cost[0].size());
    assert(demand_count > 0);

    facility_location_problem<double, double> problem;
    problem.demand_count = demand_count;
    problem.candidate_count = candidate_count;
    problem.cost.resize(
        static_cast<std::size_t>(candidate_count) * demand_count);
    for (int facility = 0; facility < candidate_count; ++facility) {
        assert(static_cast<int>(directed_service_cost[facility].size()) ==
               demand_count);
        for (int demand = 0; demand < demand_count; ++demand) {
            problem.cost[static_cast<std::size_t>(facility) * demand_count + demand]
                = directed_service_cost[facility][demand];
        }
    }
    return solve_k_median(problem, facility_count, options);
}
```

### 8.17 U17：次ターンの条件変更

```cpp
#include "facility_location_v11.hpp"

// U17: 新しい条件で前ターンの施設集合を改善する。
// 入力 new_problem: 更新済みの全費用表・重みなど。関数中は不変。
//      previous_facilities: 新しいID系でも有効な、重複なしの非空集合。
//      new_restrictions: 前の集合がこの制約を満たすこと。optionsは省略可能。
//      需要数は変えてもよいが、problemの各配列の長さを合わせる。
// 出力 previous_cost_in_new_problem: 同じ新条件で前の集合を使った費用。
//      solution: 改善後の解。solution.facilitiesを次ターンへ保存する。
// 注意: 古いworkspaceは再利用しない。施設IDが変われば呼出し側で写し替える。
struct turn_update_answer {
    long long previous_cost_in_new_problem;
    facility_location_result<long long> solution;
};

turn_update_answer improve_on_next_turn(
    const facility_location_problem<long long, long long>& new_problem,
    const std::vector<int>& previous_facilities,
    const facility_location_restrictions& new_restrictions,
    facility_location_options options = {}) {
    // 新しい探索状態を作る。初期集合はconst参照からコピーされ、変更されない。
    auto solution = improve_k_median(
        new_problem, previous_facilities, new_restrictions, options);
    const long long previous_cost =
        evaluate_k_median(new_problem, previous_facilities);
    return {previous_cost, std::move(solution)};
}
```

## 9. 実装

### 9.1 全体の流れ

新規構築の`solve_k_median`は、問題と制約を整理した後、複数の初期施設集合を作り、それぞれを1施設交換で改善します。その中で得た最良集合を起点に、最大2施設を一時的に置き換え、再び1施設交換で改善する探索を繰り返します。最後に最良集合を整形して返します。

初期集合を渡す`improve_k_median`は、初期施設の選択処理を省きます。入力集合を実行可能な初期最良解として保存し、1施設交換と摂動へ進みます。どちらも探索途中の最後の集合を無条件で返すわけではありません。

|段階|solve|improve|
|---|---|---|
|1. 入力と制約の準備|問題を参照し、候補・必須集合と作業配列を構築|同じ|
|2. 初期施設集合|重み付きD²抽選などでK個を選ぶ。初期試行ごとに実行|渡されたK個を採用|
|3. 最近傍と最良解|最近・第2近傍と目的値を作り、最良候補として保存|同じ|
|4. 局所探索|改善する1-swapをすぐ適用。初期試行ごとに実行|入力集合に対して実行|
|5. ILS|最良解を摂動し、局所探索を繰り返す|同じ|
|6. 結果作成|最良施設をソートし、割当て・費用を再計算|同じ|

deadlineや評価上限に達すると探索を短縮しますが、結果の実行可能性を保つ処理は残ります。以下では、制限に達していない場合の基本動作を説明し、停止については9.8でまとめます。

### 9.2 公開部分と探索状態

公開型はproblem、options、restrictions、statistics、result、workspaceです。`solve_k_median`・`improve_k_median`・`evaluate_k_median`が公開関数です。探索エンジン`search_state`はworkspaceのprivateな入れ子型で、利用者が直接生成する型ではありません。

探索状態は主に次の配列・値を保持します。

|内部情報|役割|
|---|---|
|選択施設と開閉表|現在のK施設と、各候補が選択中かどうか、選択配列内の位置|
|最近施設・最近費用|各需要点を現在担当する実施設またはfallback|
|第2近傍費用|担当実施設を1つ削除した場合の代替候補を調べるための費用|
|重み付き最近・第2近傍費用|候補評価で繰り返す乗算・参照に利用|
|削除補正|追加候補を固定したとき、どの選択施設を削除するかの差分|
|必須集合・許可候補・可動位置|削除できない施設と、入ってよい候補を守る|
|固定基線|必須施設とfallbackだけからなる不変の最近傍情報|
|最良施設集合・最良費用|摂動中に現在解が悪くなっても保持する最良解|
|乱数器・統計・options|探索順、停止条件、実行した仕事量|

one-shot関数は、fallback、施設単項費用、fixed、candidateが全て空なら通常経路を使います。それ以外では汎用経路を使います。空でない全0の施設単項費用も、汎用経路を使う条件に含まれます。workspaceは汎用の探索状態を保持します。

処理の本体を利用メソッド内のlambdaに置き、通常／汎用、fallbackの有無、有限評価上限の有無をコンパイル時特殊化と実行時の振分けで扱います。これは指定された機能に対応する分岐であり、グラフ種や問題サイズを見て別の探索戦略を選ぶ仕組みではありません。

### 9.3 初期解：最初の追加と重み付きD²抽選

最初に必須施設を選択済みにし、需要点ごとの現在サービス費用を必須施設とfallbackから作ります。必須施設もfallbackもなければ、この時点ではまだ利用先がありません。

最初の試行の最初の可動施設は、許可された全候補を比較して選びます。各候補を1施設だけ足した場合のサービス費用と、その施設の単項費用を計算し、最小のものを選びます。必須施設の単項費用はどの候補を足しても同じ定数なので、この比較に加える必要はありません。

2回目以降の初期試行では、最初の可動施設を乱数により変えます。実装は候補領域内の開始位置を乱数で選び、そこから最初の未選択施設を探します。未選択施設の集合から厳密に一様抽選する処理とは異なります。

施設が1つ以上使える状態になった後は、現在サービス費用を$d_i$として、次の重みで需要点を抽選します。

$$p_i=\frac{w_i d_i^2}{\sum_{j=0}^{Q-1}w_j d_j^2}$$

ここで$p_i$は需要点$i$を選ぶ確率です。分母は全需要点の抽選重みの合計です。遠い、またはサービス費用が高く、かつ重要な需要を起点にするため、費用を二乗しています。合計が正でない場合は需要点を乱数で選びます。合計と抽選には`long double`を使います。

抽選した需要点に最も近い未選択施設を許可候補から探し、それを追加して全需要点の現在サービス費用を更新します。この段階では、その1需要点への近さで施設を選び、各追加施設の単項費用を改めて全体最適化するわけではありません。単項費用を含む本来の目的値は、後続の局所探索で評価します。

この初期化は、任意の非対称費用・fallback・単項費用に対する近似比を保証するものではありません。途中で時刻に達しても、残りを許可された未選択施設で埋めて、K個の実行可能な集合にします。

### 9.4 最近・第2近傍と固定基線

1施設を削除するときに必要なのは、その施設が各需要点を担当しているか、担当していた場合の代わりがいくらか、という情報です。そのため最近費用だけでなく第2近傍費用も保持します。fallbackは削除されない代替サービスとして最近傍比較へ含めます。

必須施設がある場合、その施設集合とfallbackだけの最近・第2近傍を一度作ります。状態再構築ではこの配列をコピーし、可動施設だけを追加して最近傍を更新します。固定施設の行を再構築のたびに全て走査する処理を省けます。

一方、初期集合を変更したときの最近傍全体は再構築します。固定基線以外の前回の近傍状態を、そのまま次の呼出しに継続するわけではありません。また、固定基線には費用表やfallbackに依存する情報が入るため、workspaceのproblemを後から書き換えられません。

### 9.5 FastPAM型の1-swap差分

現在の選択集合から施設$g$を削除し、未選択施設$h$を入れる操作を1-swapと呼びます。1組ずつ全需要を再評価すると、追加候補数×削除候補数×需要数の仕事が必要です。実装では追加候補$h$を固定し、全削除位置の差分をまとめて計算します。

需要点$i$について、現在の最近費用を$d_{1,i}$、第2近傍費用を$d_{2,i}$、追加候補からの費用を$c_{h,i}$とします。まず$h$を追加するだけの差分は次です。

$$\Delta_{add}(h)=a_h+\sum_{i=0}^{Q-1}w_i\left(\min(c_{h,i},d_{1,i})-d_{1,i}\right)$$

次に、現在実施設$g$が担当する需要点の集合を$I_g$とします。施設$g$を削除すると、その需要点は最近施設を失うため、第2近傍または追加施設へ切り替わります。削除する施設の単項費用も支払わなくなります。

$$R_g(h)=-a_g+\sum_{i\in I_g}w_i\left(\min(c_{h,i},d_{2,i})-\min(c_{h,i},d_{1,i})\right)$$

従って、交換全体の差分は次の和です。

$$\Delta(g,h)=\Delta_{add}(h)+R_g(h)$$

$I_g$に含まれない需要点は、施設$g$を削除しても現在の担当先を失いません。そのため需要点を1回ずつ走査し、現在の担当施設の補正欄だけに加算すれば、全削除位置の補正を同時に作れます。fallbackを使う需要点は実施設の削除で担当先を失わないので、削除補正を加える実施設がありません。

最後に削除可能な位置だけを比較します。必須施設は削除候補になりません。追加候補1個の評価は、需要走査と削除位置走査を合わせて$O(Q+K)$です。需要点ループにはGCCのunroll指示がありますが、これはループ展開に関するコンパイラへの指定で、評価式を変えるものではありません。

施設単項費用がないときは削除補正が非負なので、追加だけでも改善できない候補は、削除位置の走査を省略できます。施設単項費用があると削除そのものが有利な場合があり、この棄却は使いません。

### 9.6 改善をすぐ適用する局所探索

追加候補の順序をshuffleし、選択済みでない候補を順に評価します。ある追加候補について、最良の削除先との交換差分が負なら、その交換をすぐに適用します。次の候補は更新後の集合に対して評価します。全候補の中で最良の1組が見つかるまで待つ方式ではありません。

候補を一巡して改善があれば、再度順序をshuffleして一巡します。改善がなくなった一巡、時刻制限、評価上限などで終了します。費用が同じ交換は、改善swapとしては受理しません。

交換を適用するときは開閉表を差分更新します。需要点ごとに、退出施設の費用が第2近傍費用以下なら、残る全施設から最近2個を調べ直します。同額の代替候補を失う場合もあるため、等号も含めます。それ以外は最近2個のどちらも退出施設ではないので、追加施設だけを挿入します。

この更新後、重み付き費用・担当位置・総費用を整え、改善swap数と最良解を更新します。更新の最悪計算量は$O(QK+K)$です。全需要点で最近2施設を調べ直す必要がなければ、実際の仕事量はそれより小さくなります。

### 9.7 ILS：最良解から最大2施設を交換する

1-swapで改善できなくても、複数施設を一緒に変更すれば良くなる場合があります。そこで最良集合を復元し、可動施設の位置をshuffleして、最大2施設を未選択候補と交換します。可動施設数や未選択候補数が1しかなければ、交換数も1になります。

直前に退出させた施設は、その摂動の追加先としてすぐ再選択されないようにします。追加後に最近傍状態を再構築し、局所探索を行います。局所探索での改善だけでなく、摂動した集合自体が最良になった場合も保存します。

探索は毎回、保存した最良集合から摂動します。途中の現在集合が悪くても、最良集合は保持されます。この仕組みは2施設交換の全組合せを列挙する厳密な2-swap探索ではありません。

### 9.8 停止、再利用、返却

候補評価上限は未選択候補の評価前に確認し、評価1回ごとにカウントを増やします。局所探索の時刻は外側の巡回開始や、おおむね候補評価16回ごとの確認点で調べます。初期化は後続施設の追加前にも時刻を確認しますが、最初の全候補走査自体は中断しません。

workspaceの次の呼出しでは、options、乱数seed、候補順、最良解、統計などを初期化します。主要な作業配列と固定基線は保持しますが、初期構築に使う一時配列まで全ての確保がなくなるわけではありません。

最終結果は保存した最良施設集合を昇順にして、需要点ごとの最安実施設とfallbackを改めて比較して作ります。実施設の同額候補は最小ID、fallbackが実施設より厳密に安い場合だけ-1を返します。施設単項費用を足し、探索統計を添えます。

`evaluate_k_median`はこの目的値評価を任意の非空施設集合に対して行う関数です。探索もソートも制約確認も行わず、施設費用の加算順は渡した施設順です。小数では加算順による丸め差にも注意します。

### 9.9 計算量を読む

Qを需要数、Mを候補数、Kを選択数、$F_c$を必須施設数とします。以下は入力のグラフ最短路計算などを含まない、solver内部の目安です。

|処理|計算量の目安|
|---|---|
|費用表の保持|$O(MQ)$メモリ|
|探索状態の追加メモリ|$O(M+Q+K)$|
|汎用状態の構築|$O(M\log(M+1)+Q(F_c+1)+K)$の上界|
|初期解1試行|$O(MQ+K(M+Q))$の上界。初回の最初の追加が全候補・全需要を比較|
|最近傍の再構築|$O(M+Q(K-F_c+1)+K)$。固定基線を利用する場合|
|追加候補1個の評価|$O(Q+K)$|
|改善swap1回の適用|最悪$O(QK+K)$|
|摂動後の再構築|$O(M+Q(K-F_c+1)+K)$の上界|
|結果の作成|$O(K\log K+QK)$|
|任意集合の費用評価|施設数Lとして$O(QL+L)$|

全体では、評価した追加候補数Eの項$O(E(Q+K))$と、受理swapの再計算、初期構築などを合わせた仕事量になります。探索回数そのものが入力と終了条件で変わるため、Q・M・Kだけから短い一定の実行時間を保証するものではありません。

候補poolを小さくすると評価する候補や初期選択時の走査を減らせますが、開閉表などには全Mに比例する処理も残ります。固定施設を増やすと可動部分の再構築を減らせますが、需要点ごとの配列コピーや最終割当て作成は必要です。これが、入力準備・workspace再利用・探索量の調整を別々に考える理由です。
