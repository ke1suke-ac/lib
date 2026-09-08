# rectangle_packing ユーザーガイド

対象ソースは`rectangle_packing_v09.hpp`。C++20の公開インターフェースを使って、整数座標上に直交矩形を配置するためのガイドである。

この文書は次の順に読むと、問題の選択から実装までを追いやすい。

- 1章：何を入力し、何を良い解とするか。
- 2章：このライブラリを使う前に、厳密解で済む条件を確認する。
- 3章：前の解やターン間の変更をどう扱うか。
- 4章：自分の用途をU01〜U14から選ぶ。
- 5章：データ型、初期値、設定を準備する。
- 6章：選んだ用途の呼出方法と結果の読み方を確認する。
- 7章：制約と失敗しやすい点を確認する。
- 8章：同じ番号の関数形コード例をコピーする。
- 9章：内部のアルゴリズムと計算量を理解する。

4・6・8章のU番号は共通である。各コード例は必要な準備を含め、他のユースケースの例を先にコピーしなくても使えるようにしている。

## 1. 何を解くライブラリか？

### 1.1 長方形を重ねずに配置する問題

たとえば、幅と高さが異なる画像を1枚のatlasへまとめたり、長方形の部品を板へ割り当てたりする問題を扱う。画像・部品など、置きたいものを「矩形」、置く先の長方形の領域を「ビン」と呼ぶ。ここでの箱は2次元の領域であり、奥行きは扱わない。

利用者が矩形の寸法と回転の可否を指定すると、各矩形の左上座標、使用するビン、回転後の寸法が返る。置けない矩形を含む結果も返るため、返却されたことと、要求を満たしたことは区別する。

共通する幾何条件は次のとおり。

- 左上が原点。右へ進むと横座標が増え、下へ進むと縦座標が増える。
- 寸法と座標は整数。寸法は正、配置座標は0以上。
- 辺は水平・垂直。回転を許した矩形だけ、幅と高さを入れ替えられる。
- 同じビンの矩形同士は内部が重ならない。辺や角が接することは許す。
- 寸法上限のある軸では、配置全体がその上限内に収まる。

### 1.2 数式で使う変数

矩形を入力順に$0,1,\ldots,N-1$と番号付けする。

| 記号 | 意味 |
|---|---|
| $N$ | 入力矩形の個数 |
| $i,j$ | 矩形の番号 |
| $w_i,h_i$ | 矩形$i$の入力時の幅・高さ |
| $p_i$ | 矩形$i$を配置したときのprofit。固定ビンで使う価値 |
| $q_i$ | 必須矩形なら1、任意矩形なら0 |
| $z_i$ | 結果で配置済みなら1、未配置なら0 |
| $a_i,b_i$ | 配置時の幅・高さ。通常は$(w_i,h_i)$、回転時は$(h_i,w_i)$ |
| $x_i,y_i$ | 配置済み矩形$i$の左上座標 |
| $k_i$ | 配置済み矩形$i$の0始まりのビン番号 |
| $W,H$ | 利用者が指定する固定ビンの幅・高さ。stripでは$W$だけを固定する |
| $U,V$ | 結果の使用幅・使用高さ。配置済み矩形の最大右端・最大下端 |
| $M$ | 未配置の必須矩形数 |
| $P$ | 配置済み矩形のprofit合計 |
| $A$ | 配置済み矩形の面積合計 |
| $B$ | 使用ビン番号の最大値に1を足した数。通常の連番出力では使用ビン数 |
| $A_{\mathrm{last}}$ | 番号$B-1$の末尾ビンへ配置された矩形の面積合計 |
| $\alpha,\beta$ | 外接矩形で幅・高さにかける正の重み |
| $f(U,V)$ | 外接矩形の目的値。面積、半周長、重み付き和のいずれか |

配置済み矩形がなければ$U,V,B,A_{\mathrm{last}}$はすべて0とする。各集計値は次の意味を持つ。

$$M=\sum_{i=0}^{N-1}q_i(1-z_i),\quad P=\sum_{i=0}^{N-1}p_i z_i,\quad A=\sum_{i=0}^{N-1}w_i h_i z_i.$$

配置済みの$i$について右端は$x_i+a_i$、下端は$y_i+b_i$である。$U$と$V$はそれぞれの最大値。複数ビンでも座標は各ビン内の座標なので、$UV$は全ビンを横につないだ面積ではない。

同じビンに配置された異なる2矩形$i,j$は、次のいずれかを満たす。つまり「左・右・上・下のどちらかに完全に分離している」という条件である。

$$x_i+a_i\le x_j\quad\mathrm{or}\quad x_j+a_j\le x_i\quad\mathrm{or}\quad y_i+b_i\le y_j\quad\mathrm{or}\quad y_j+b_j\le y_i.$$

### 1.3 固定ビン：必須矩形と価値の高い任意矩形を置く

$W,H$を両方固定する。配置済みの各矩形について、$x_i+a_i\le W$、$y_i+b_i\le H$が必要になる。

必須矩形はなるべくすべて置き、任意矩形は価値を見て選ぶ。ライブラリが結果を比較する順序は次の辞書式最小化である。

$$\min_{\mathrm{lex}}(M,-P,-A,UV,V,U).$$

辞書式とは、左から比べ、最初に違った項だけで優劣を決めること。profitがいくら増えても、必須矩形の欠落が1個増える解は採用しない。$M$が同じなら$P$が大きい方、その次は$A$が大きい方、その後は小さな外接枠を優先する。

全部置きたいときは全矩形を必須にする。任意矩形の価値だけを最大化したいときは、必須矩形のprofitを0に揃えると考えやすい。たとえば「必須2個、任意3個」なら、まず必須2個が置けたかを確認し、その条件の中で任意のprofitを比べる。

$M=0$は必須矩形を置けたという意味であり、任意矩形まで全配置したことは意味しない。全矩形が任意なら、何も置かない結果でも$M=0$である。

### 1.4 Strip：幅を固定し、必要な高さを減らす

幅$W$のロール材に部品を並べる場面を考える。横には$W$の上限があり、縦には必要に応じて伸ばせる。対象は全矩形を置く問題なので、全入力を必須にして使う。

$$\min_{\mathrm{lex}}(M,V,U).$$

全配置できた解では$M=0$なので、使用高さ$V$が主目的になる。高さが同じなら使用幅$U$で比較する。返る$U$は実際に使った幅であり、指定幅$W$より小さいこともある。profitを最大化する問題ではない。

### 1.5 複数ビン：同じ大きさの領域を何枚使うか

すべてのビンが同じ$W,H$を持ち、必要な数だけ使える。全矩形を必須にし、配置するビンとビン内の座標を求める。

$$\min_{\mathrm{lex}}(M,B,A_{\mathrm{last}}).$$

主目的は全配置したうえで$B$を減らすこと。同じ$B$なら末尾ビンの中身を少なくする。これは全ビンの空き面積を個別に最小化する目的ではなく、各ビンの利用率を均等にする目的でもない。

利用できる枚数が例えば3枚と決まっている場合は、返った$B$が3以下かを呼出側で確認する。ただし、3以下の解が見つからなかったことだけで、3枚での配置不可能が証明されるわけではない。

### 1.6 外接矩形：幅も高さも選ぶ

ビン寸法を先に指定せず、全矩形を囲む枠の幅$U$と高さ$V$も求める。全入力を必須にする。

$$\min_{\mathrm{lex}}(M,f(U,V),UV,V,U).$$

| 目的の種類 | $f(U,V)$ | 意味 |
|---|---|---|
| 面積 | $UV$ | 使用領域の総面積を小さくする |
| 周長 | $U+V$ | 半周長を小さくする。実際の周長$2(U+V)$と最適な解は同じ |
| 重み付き和 | $\alpha U+\beta V$ | 幅と高さで単位当たりの費用が異なる場合。両重みは正 |

幅を広げる費用が高さの3倍なら$\alpha=3,\beta=1$とする。重みを大きくしても幅の上限を指定したことにはならない。幅を絶対に超えさせたくないなら、stripまたは固定ビンを選ぶ。

### 1.7 結果に期待できること

返るのは候補となる配置であり、一般の入力で最適性や配置不可能性を証明するものではない。全配置に失敗した結果、全配置したが最小サイズではない結果、指定した探索時間を使い切る前に返る結果を区別して扱う。

## 2. 厳密解アルゴリズム

### 2.1 まず確認したい条件

本ライブラリは短い探索で良い候補を得るための部品であり、以下の厳密解法を内蔵してはいない。入力が制限されるなら、厳密解法の方が単純になったり、最適性まで確認できたりする。

「回転を禁止した」「幅を固定した」だけでは、一般の矩形配置が簡単な問題になるとは限らない。次の表では、厳密に扱える理由も含めて条件を示す。

| 条件 | 検討する方法 | 何を確定できるか／注意 |
|---|---|---|
| 矩形数と整数の配置領域が十分小さい | 回転・整数座標・配置する部分集合を全列挙 | 列挙を完了すれば最適解または不可能性を確定。候補位置を一部の角に制限すると厳密性を失う |
| 回転と、各矩形対の左右・上下関係が固定済み | 座標に対する差分制約 | その固定した関係の中で最も詰めた配置を求められる。関係自体を選ぶ問題は残る |
| 回転なし、全矩形の幅がstrip幅と同じ、全配置 | 縦に積む | 必要高さは各高さの合計で厳密に最小 |
| 回転なし、全矩形の高さが固定ビンの高さと同じ、任意選択あり | 幅を容量とする0/1ナップサックDP | 利益最大の選択を厳密に求められる。幅が大きいと状態数が大きい |
| 同じ寸法の矩形だけ、回転なし、ビンの両辺が矩形の対応辺の整数倍 | 格子状に配置 | 面積下界を達成でき、必要枚数も厳密に分かる |
| 汎用の整数配置で最適性の証明が必要、外部solverを利用可能 | 制約プログラミングや整数最適化 | 計算時間は問題依存。期限内に最適性を証明できるとは限らない |

### 2.2 小さな整数盤面なら全列挙できる

固定ビンの1矩形について、許された向きと、枠内に入る左上の整数座標をすべて候補にする。矩形を1つ選んでは候補位置を試し、既に置いたものと重なる枝を捨てる。任意矩形なら「置かない」枝も必要になる。

本ライブラリと同じ整数座標の問題に対し、この候補を漏れなく探索すれば厳密解になる。置く順番を1通りに固定するだけなら、各矩形の全位置を試す限り配置の網羅性は失わない。ただし、位置を「現在の空き領域の特定の角」だけに削ると、すべての配置を試したことにはならない。

ここで必須矩形の「置かない」枝を作らなければ、必須を全部配置できるかと、その条件下の最適解を調べることになる。必須欠落を含む中間解まで1章と同じ辞書式で最適化したいなら、必須にも未配置の枝を用意し、欠落数を第一の比較キーにする。

回転を固定すると向きの分岐は減るが、座標の組合せは依然多い。固定した矩形が大部分を占め、可動矩形が数個だけという部分問題にも適用を検討できる。何個まで間に合うかは、盤面寸法と候補位置数にも依存する。

### 2.3 相対的な位置関係まで固定すると差分制約になる

たとえば「矩形$i$は矩形$j$の左」と決めると、横座標に$x_j\ge x_i+a_i$という制約が付く。「上」と決めれば縦座標に同様の制約が付く。

すべての重なりを防ぐ関係が固定され、各矩形の向きも固定されているなら、横と縦をそれぞれ有向グラフにする。辺は「この矩形より先に置く」という制約で、重みは幅または高さ。座標0を起点として最長距離を求めると、各下限制約を満たす最小の座標になる。正の寸法を足しながら一周する閉路があれば矛盾である。

こうして得た最小座標が指定枠を超えるなら、その位置関係では入らない。枠が自由なら、幅と高さに関して単調な目的を、その固定した関係の中で最小化できる。ここで簡単になったのは座標の決定部分であって、「どの矩形をどの方向に分離するか」の選択は別問題である。

### 2.4 一次元に落ちる例

回転なしで各矩形の高さがビン高さ$H$と同じなら、置いた矩形は縦方向に重なるので、横に並べるしかない。必須矩形の幅を先に消費し、残りの幅を容量、任意矩形の幅を重さ、profitを価値として0/1ナップサックを解ける。容量を$C$とすると、整数幅の典型的なDPは時間$O(NC)$、領域$O(C)$で実装できる。$C$は必須矩形を除いた残り幅であり、巨大な実寸をそのまま容量にすると重い。

同一寸法の格子配置では、矩形寸法を$a,b$、ビン寸法を$sa,tb$とする。$s,t$は正の整数。1ビンに$st$個を敷き詰められ、面積からもそれ以上は入らないので、必要枚数は$\lceil N/(st)\rceil$で厳密である。寸法が揃わない一般の入力へ、この式をそのまま使ってはいけない。

### 2.5 外部の厳密solverを使う場合

OR-ToolsのCP-SATでは整数変数を使い、2次元の非重複制約で配置を表せる。任意矩形には配置有無を表す変数も対応させる。2次元非重複とoptional intervalの意味は、公式の[モデル定義](https://github.com/google/or-tools/blob/stable/ortools/sat/cp_model.proto)で確認できる。

このモデル化を本問題に適用するなら、向き、座標、必要に応じてビンへの割当を変数にし、枠内条件と非重複条件を加える。辞書式の目的は、各段階の最適値を確定してから次の目的を解く方法が使える。段階の最適性が確定する前に値を固定すると、元の辞書式最適化とは同じにならない。

CP-SATの`FEASIBLE`は「解は見つかったが最適性は未確定」、`OPTIMAL`は最適解が見つかった状態である。期限等による`UNKNOWN`も不可能の証明ではない。結果の状態を読む必要がある。[公式solverガイド](https://developers.google.com/optimization/cp/cp_solver)

これらは外部手段の説明であり、本ライブラリにCP-SATを呼ぶAPIや依存はない。コンテスト環境での外部solverの利用可否、導入時間、1ファイル提出の要件も選択の観点になる。

## 3. 差分更新

### 3.1 結論：解は引き継げるが、内部状態はターン間で保持しない

公開のsolverオブジェクトや`update()`メソッドはない。関数呼出のたびに探索用の内部状態を作る。したがって、辺索引や空き領域を外部から1件ずつ更新し続ける、永続的な差分更新ライブラリではない。

一方、前の配置を値として渡すことはできる。次の2種類を区別する。

| 操作 | 前の解の使い方 | 動いてよい範囲 | 内部状態の再利用 |
|---|---|---|---|
| `improve_*`による全体再探索 | 初期解と新候補を標準目的で比較する | 全矩形 | ターン間の内部状態は引き継がない |
| 固定ビンの`repair_*` | 固定部分を残し、指定部分を置き直して初期解と比較する | 指定したIDだけ | 1回の呼出内で固定領域の処理を候補間に再利用 |
| 固定ビンの`repack_*` | 同じ部分再配置だが、初期解を候補との比較で保護しない | 指定したIDだけ | 同上。外部スコアによる採否を自分で決める用途 |

固定ビンの`improve_*`は初期解を最良候補として探索する。それ以外の3形式の`improve_*`は新たに全体を解き、その結果と初期解を最後に比較する。いずれも、前の座標を少しずつ動かして探索を継続する機能ではない。

### 3.2 部分再配置を使う手順

1. 現在の矩形配列と、同じ番号順の現在配置を用意する。
2. 動かしてよい矩形のIDを選ぶ。未配置の矩形も、置きたいならこの集合へ入れる。
3. それ以外の配置が現在の条件で有効かを確認する。
4. 集計値を最新にし、未配置requiredを許す設定で中間解を検査する。
5. 標準目的なら`repair_*`、問題固有の採否判定なら`repack_*`を呼ぶ。
6. 全配置や問題固有の制約を確認し、次ターンへ配置を引き継ぐ。

可動集合の外にある未配置矩形は、未配置のままである。可動集合が空なら初期解がそのまま返り、集計値の修正や再検査も自動では行われない。

### 3.3 何を変更できるか

| 変更内容 | 呼出前に必要な作業 | 引き継げるもの／再構築されるもの |
|---|---|---|
| 乱数seed、探索予算、deadline | 新しい設定を作る | 入力・初期配置をそのまま使える。探索状態は新規 |
| profitの変更 | 配置済みprofit合計を再計算する | 幾何はそのまま。ただし選択を変えたい矩形は可動に含める |
| requiredの変更 | 必須欠落数を再計算する | 未配置から必須にした矩形を動かす集合へ含める |
| 寸法の変更 | 古い寸法の配置を解除するか、有効な寸法と座標へ直す | 変えない矩形の座標は残せる。変更矩形は通常再配置 |
| 回転許可の変更 | 禁止した向きで置かれた配置を解除する | 既に通常向きなら座標を維持できる |
| 矩形の追加 | 入力とplacementsを同時に拡張。追加配置は未配置で初期化する | 新しいIDを可動に含めて挿入を試せる |
| 矩形の削除・並べ替え | 入力とplacementsへ同じID対応を適用する | 残した矩形の座標をコピーできる。ID対応は利用者が管理 |
| ビンの拡大 | 新寸法で初期解を再確認する | 既存座標が有効なら維持できる |
| ビンの縮小 | 枠外の配置を解除し、可動集合へ入れる | 枠内の固定部分を残せるが、残りが入る保証はない |
| 外接目的・正の重みの変更 | 新しいobjectiveで再探索する | 初期配置は利用可能。比較は新しい目的で行う |
| 固定障害物の移動・増減 | 障害物の配置を更新し、衝突する可動矩形を解除する | 固定障害物同士が重なる状態は受け付けられない |

ライブラリが行うのは渡された配列に対する探索である。追加・削除を検出する機能や、アプリ側のIDを追跡する機能はない。U14の例では「次ターンのID→前ターンのID」という配列を使ってこの準備を明示する。

### 3.4 非悪化の意味と、効率が出る範囲

`repair_*`の非悪化は、同じ入力・同じ枠・同じ目的で比べる場合の性質である。寸法やprofitを変更したら、比較基準も変更後の問題になる。前ターンの点数をそのまま保証することはできない。

固定する矩形が多く、動かす矩形が少ないと、全体を置き直す探索を減らせる。ただし、各呼出で全入力の順序作成、固定領域の登録、結果配列のコピー・集計は必要であり、処理時間が可動矩形数だけに比例するわけではない。

座標を維持したい問題には部分repairが合う。自由に動かしてよいのに固定範囲を狭くしすぎると改善の余地を失う。可動範囲を段階的に広げるか、全体再探索へ切り替える判断は外側で行う。複数ビン・自由な外接矩形には、同じ形の公開部分repair APIはない。

## 4. ユースケース

ここでは用途と入力の意味だけを説明する。コード上の設定は5章、呼出方法は同じU番号の6章、完成した関数例は8章にある。

| U番号 | 用途 | 他の用途との違い |
|---|---|---|
| U01 | 固定領域への全配置 | 枠は変更できず、全部置くことが必要 |
| U02 | 価値に基づく任意矩形の選択 | 一部を置かなくてもよく、選択集合自体が答え |
| U03 | 幅固定で使用高さを短縮 | 枠の高さを結果として求める |
| U04 | 同サイズの複数領域への割当 | 使う領域の個数を減らす |
| U05 | 自由な外接枠の最小化 | 幅・高さの両方を選ぶ |
| U06 | 許された外枠候補からの選択 | 枠を任意の整数寸法にはできない |
| U07 | 同じ問題を複数seedで再探索 | 入力は不変で、現在の良い解を保ちながら試行を増やす |
| U08 | 固定部分を残した標準目的repair | 入力は不変で、動かす範囲だけを制限する |
| U09 | 問題固有のスコアによる部分再配置の採否 | 採否の目的がライブラリの標準目的とは異なる |
| U10 | 矩形障害物を避けた配置 | 最初から使えない領域がある |
| U11 | 一様な隙間・外周余白の確保 | 接触を許さず、距離の条件を追加する |
| U12 | 外部で作成した配置の取込み | 配置だけが手元にあり、内部の集計値はまだない |
| U13 | 複数の処理で共通deadlineを使う | 各呼出の回数より、全体の残り時間を管理する |
| U14 | ターン間の追加・削除・属性変更 | 入力問題自体が変化し、IDの対応付けも必要 |

### 4.1 U01：固定領域へ全矩形を置く

サイズの決まったテクスチャへ全画像を入れる、決まった作業台へ全部品を並べる、といった用途。指定するのは領域の幅・高さと、各矩形の幅・高さ・回転可否である。縦横を入れ替えると意味が変わるラベルや画面部品は回転を禁止する。

枠を大きくすることも、一部を捨てることも許さない。返った候補について全個数が配置済みかを確認する。入らないときの対応を、枠変更ではなく再探索や上位問題への失敗通知として扱いたい場合に合う。

### 4.2 U02：必須を守りながら価値の高いものを選ぶ

限られた掲示スペースへ広告を選ぶ、必須の機材を置いた残りに追加機材を置く、といった用途。U01の入力に加え、必須か任意か、任意の場合にどれほどの価値があるかを指定する。

「小さなものをたくさん」と「大きいが価値の高いもの」を面積だけで判断しない。profitは利用者が決める整数の評価値であり、金額、得点、重要度などの同じ尺度を使う。必須の配置を維持できたか、選ばれた任意矩形はどれか、価値合計はいくつかを結果として使う。

### 4.3 U03：幅の決まったロール材を短く使う

印刷ロール、帯状の素材、横幅が決まったページの使用高さを減らす用途。決める入力は横幅と各矩形の寸法・回転可否であり、縦方向の上限は指定しない。

使用高さが購入長やスクロール長に対応する。あとから高さ上限との比較はできるが、幅も高さも絶対条件として固定するならU01の方が直接的である。

### 4.4 U04：同じサイズの板・ページ・箱をなるべく少なく使う

すべての領域の寸法が同じで、必要な個数だけ使える用途。各矩形をどの領域に割り当てるかと、その中の座標を同時に求める。領域ごとに異なる寸法や費用を指定する用途ではない。

ビンの幅・高さと全矩形を指定する。使用枚数だけでなく、各枚に置いた矩形の一覧が必要になる。物理的な板の切断手順は別であり、配置ができても指定の順序で切れるとは限らない。

### 4.5 U05：全体の外枠を小さくする

画像群をまとめたatlasの面積、矩形部品群を囲う枠の長さ、幅と高さに別々の費用がある領域を小さくする用途。枠の寸法は指定せず、面積・周長・重み付き和から目的を指定する。

重み付き和では幅と高さの単位費用を指定する。幅を嫌うことと、幅に絶対上限を置くことは異なる。細長さの上限、縦横比固定、幅を特定の倍数にする条件は、この目的指定だけでは表せない。

### 4.6 U06：規格の中から外枠を選ぶ

使用可能なatlas寸法が数種類に限られる、規格板から最小の板を選ぶ、縦横が特定の倍数でなければならない、といった用途。外枠の候補一覧と、全部置きたい矩形群を指定する。

候補ごとに全配置を試し、成功したものを外側の基準で選ぶ。例では面積、長辺、幅、高さ、候補番号の順に小さいものを選ぶ。価格順にしたいなら候補ごとの価格も外側で管理する。「成功した候補の中の最小」であり、「不可能性まで証明した最小」とは区別する。

### 4.7 U07：同じ問題を繰り返して解を磨く

入力は変わらず、時間の許す範囲で別の乱数seedを試したい用途。試すseedの一覧と1回当たりの探索予算を指定し、各試行の後も、それまでの最良解を持ち続ける。

8章ではstripを例にする。座標を固定する機能ではないので、同じ高さでも矩形の位置が変わり得る。配置変更に費用があるならU08やU09を検討する。

### 4.8 U08：一部だけを動かして標準目的を改善する

大半の配置を確定したまま、詰まりやすい領域にある数個だけを置き直す用途。現在の入力・配置に加え、動かしてよい矩形番号を指定する。固定する番号を列挙するのではなく、可動の番号を列挙する。

例として、未配置矩形とその近くの数個を可動にし、他の座標を維持する。固定ビンの標準比較で悪くならない結果を得たい場合に使う。どこを動かすかを自動で選ぶ機能はない。

### 4.9 U09：自分のスコアで配置変更を採択する

実際の得点が「特定部品を近づける」「重要画像を上へ置く」「配線長を小さくする」など、標準のprofit・面積とは異なる用途。U08と同じく可動範囲を指定するが、候補を採るかは自分の評価関数で決める。

この機能は、指定スコアを内部で直接最適化するものではない。標準の部分再配置で作られた候補を、自分の条件で選別する使い方になる。必須欠落を許さないスコアなら、その違反を他の得点より優先して評価する必要がある。

### 4.10 U10：固定障害物の隙間へ置く

作業台の固定装置、atlas内の予約領域など、使ってはいけない矩形領域がある用途。障害物ごとに左上座標と寸法を指定し、新たに置く矩形の寸法・回転可否を別に指定する。

障害物を動かせない既存配置として表す。障害物同士は重なってはならず、ビン内に収まっている必要がある。曲線や穴のある形をそのまま指定することはできない。複数の互いに重ならない矩形で表すか、保守的な矩形近似を利用者が行う。

### 4.11 U11：一様な隙間と外周余白を空ける

画像の隣接によるにじみ、印刷物の間隔、部品間の空きスペースを確保する用途。元の矩形群とビン寸法に加え、矩形間の隙間$g$、ビン外周の余白$m$を指定する。どちらも0以上の整数である。

この例の条件は、矩形対が少なくとも横または縦の一方で$g$以上離れていること。全方向に同じ円形の安全距離を厳密に最適化する条件とは異なる。縦横で違う間隔や矩形対ごとに違う間隔は、この例の対象外である。

### 4.12 U12：自作配置や外部solverの結果を取り込む

手書きの初期配置、ファイルに保存した配置、別手法で作った配置が既にある用途。入力矩形と同じ番号順に、座標・配置寸法・回転・ビン番号を指定する。

外部配置から集計値を作り、寸法と重なりなどを検査したうえで再探索する。既存の良いスコアを下回らせたくない場合に使えるが、座標をそのまま固定したい場合はU08になる。

### 4.13 U13：AHC全体の残り時間を共有する

前処理、複数の部分問題、出力処理を合わせて制限時間内に収めたい用途。各関数へ「あと何ミリ秒」と渡すのではなく、外側で決めた共通の終了時刻を渡す。

出力用の余裕を引いた絶対時刻と乱数seedを指定する。内部の終了確認は一定の処理単位の後なので、指定時刻を少し超える可能性がある。必要な余裕は最大入力での実測から決める。

### 4.14 U14：次のターンで入力が変わる

部品が新たに到着した、不要な画像を削除した、価値や寸法が変わった、という用途。次ターンの入力群、前の配置、次ターンの各番号が以前のどの番号だったかを指定する。追加したものには「以前の番号なし」を指定する。

残せる配置をコピーし、新規・寸法不一致・枠外・未配置のものを置き直す。空きが不足するなら、追加で動かしてよい既存矩形も指定する。使える空間を再計算する必要はあるが、変更していない矩形の座標は維持できる。

## 5. 利用準備

### 5.1 導入と呼出の形

通常の利用で必要なのは`rectangle_packing_v09.hpp`だけ。同じディレクトリに置き、`#include "rectangle_packing_v09.hpp"`で読み込む。ACLと添付の`hash_map.hpp`には依存しない。

C++20、GCC系の環境を想定し、`std::span`、`bits/stdc++.h`、`__int128_t`、GNU属性を使う。GCC 13.3でコード例を検証している。標準C++のヘッダーだけで構成された完全な処理系独立ライブラリではない。

利用者が構築するsolverクラスはない。用意するものは次の4種類である。

1. `std::vector<rectangle_pack_item>`などの矩形配列。
2. 固定ビン幅・高さ、または外接目的。
3. `rectangle_pack_options`。
4. 再探索や部分repairの場合だけ、現在の`rectangle_pack_result`と可動ID。

入力引数の`std::span<const T>`は要素を所有しない読み取り専用の参照である。`std::vector<T>`や`std::array<T, N>`を渡せる。呼出中に元配列を破棄・変更しない。関数が返った後まで入力への参照が保持されることはない。

### 5.2 矩形入力：`rectangle_pack_item`

| フィールド | 型 | 既定値 | 設定の意味と使い方 |
|---|---|---|---|
| `width` | `long long` | `0` | 入力幅。利用前に必ず正の値へ設定する |
| `height` | `long long` | `0` | 入力高さ。利用前に必ず正の値へ設定する |
| `profit` | `long long` | `1` | 固定ビンでの配置価値。全部置く他形式では通常1でよい |
| `rotatable` | `bool` | `true` | 幅と高さの交換を許す。印字方向などを守るならfalse |
| `required` | `bool` | `true` | 必須矩形。固定ビンでだけfalseによる任意選択を使う |

集成体初期化の順番は`{width, height, profit, rotatable, required}`。既定構築しただけの矩形は寸法0なので、解ける入力にはなっていない。配置済みかどうかは`required`では決まらない。

U02では必須矩形のprofitを0、任意矩形のprofitを指定値にする。U10では障害物を`profit=0, rotatable=false, required=true`の矩形として表す。U11では元寸法と隙間を加えた寸法を別の配列で持つ。

### 5.3 配置と結果の型

`rectangle_pack_placement`は1矩形分の配置である。

| フィールド | 型 | 既定値 | 意味 |
|---|---|---|---|
| `x`, `y` | `long long` | 各`0` | 左上座標 |
| `width`, `height` | `long long` | 各`0` | 回転適用後の寸法。入力寸法とは向きが違う場合がある |
| `bin` | `int` | `-1` | 0以上なら配置済み。負なら未配置 |
| `rotated` | `bool` | `false` | 入力の幅と高さを交換した配置ならtrue |

`bool placed() const`は`bin >= 0`を返す。外部から未配置にするときは`placement = {}`とすると全フィールドも初期化される。未配置では古い座標を出力に使わない。

`rectangle_pack_result`は結果全体である。

| フィールド | 型 | 既定値 | 結果の使い方 |
|---|---|---|---|
| `placements` | `std::vector<rectangle_pack_placement>` | 空 | 入力と同じ要素数・順番で返る。矩形$i$の結果は要素$i$ |
| `used_width`, `used_height` | `long long` | 各`0` | 最大右端・最大下端。1章の$U,V$ |
| `placed_profit` | `long long` | `0` | 配置済みのprofit合計。必須のprofitも含む |
| `placed_area` | `long long` | `0` | 配置済みの面積合計。固定障害物も含む |
| `placed_count` | `int` | `0` | 配置済み個数 |
| `missing_required_count` | `int` | `0` | 未配置の必須個数 |
| `bin_count` | `int` | `0` | 配置済みの最大ビン番号+1 |

`bool feasible() const`は`missing_required_count == 0`だけを返す。重なりや枠内条件を調べる関数ではない。既定構築した空のresultも`feasible()`はtrueなので、自分でplacementsを入れた直後は必ず集計を再計算する。

### 5.4 外接目的：`rectangle_bounding_objective`

| フィールド | 型 | 既定値 | 指定方法 |
|---|---|---|---|
| `type` | `rectangle_bounding_objective_type` | `area` | `area`, `perimeter`, `weighted_sum`から選ぶ |
| `width_weight` | `long long` | `1` | weighted_sumでの幅の重み。正の整数 |
| `height_weight` | `long long` | `1` | weighted_sumでの高さの重み。正の整数 |

たとえば`rectangle_bounding_objective{rectangle_bounding_objective_type::weighted_sum, 3, 1}`なら幅を高さの3倍で評価する。areaとperimeterでは両weightは目的値に使わない。

`long long evaluate(long long width, long long height) const`は指定した目的値を返す。入力の配置を検査する機能はなく、乗算・加算が型の範囲内であることも呼出側の前提となる。

### 5.5 探索設定：`rectangle_pack_options`

| フィールド | 型 | 既定値 | 効果 |
|---|---|---|---|
| `deadline` | `std::chrono::steady_clock::time_point` | `time_point::max()` | 終了時刻。既定値では時刻による打切りなし |
| `seed` | `std::uint64_t` | `0x243f6a8885a308d3ULL` | 内部の順序摂動等で使う乱数seed |
| `restart_limit` | `int` | `32` | 固定ビンの新規packing、improve、repair、repackの追加再始動回数 |
| `search_iteration_limit` | `int` | `32` | strip・複数ビン・外接矩形の追加探索予算 |
| `time_check_interval` | `int` | `1` | 何回の内部停止確認ごとに時計を見るか |

設定は`rectangle_pack_options options; options.seed = 7;`のように必要なものだけ代入する。各関数で`options`を省略すると、表の全既定値を使う。

値を選ぶ際の出発点は次のとおり。特定の入力で最良の値だという保証ではない。

| 目的 | 選び方 | 注意 |
|---|---|---|
| まず動作を確認 | 両予算を既定の32 | 品質と時間を実入力で測る |
| 決定的な基礎候補だけ欲しい | 該当予算を0 | 何もしない意味ではない。初期候補の構築は行う |
| 高頻度の部分再配置 | `restart_limit=8`から比較 | 可動個数・空き領域の複雑さで処理時間が変わる |
| 最終の再探索 | 32と64等を同じ入力・seed群で比較 | 予算増加に比例した改善や、常に非悪化になることを仮定しない |
| AHCの残り時間を使う | 共通deadline、十分大きい有限予算、`time_check_interval=1` | 初期構築や1候補の途中では止まらず、期限超過の余地がある |
| 再現性を確認 | deadlineなし、入力・全設定・seedを固定 | 実時間による停止は実行環境で変わる |

負の予算は内部で0相当へ丸められるが、読み間違いを避けるため0以上を渡す。`time_check_interval`は1未満なら内部で1として扱う。seedが0の場合は内部で非零の初期状態へ置き換わる。

予算は「解を何個作るか」と常に一致するわけではない。初期構築は別にあり、形式ごとの探索配分、早期終了、複数ビンの統合処理もある。具体的な配分は9章で説明する。

### 5.6 最小の準備から結果まで

次は1枚の固定ビンへ全矩形を置く例である。必須の準備と、結果の確認を省略していない。

```cpp
#include "rectangle_packing_v09.hpp"

rectangle_pack_result make_fixed_bin_example() {
    // 100×80の領域へ置く。寸法の単位は全体で統一する。
    const long long bin_width = 100, bin_height = 80;
    // 幅、高さ、profit、回転許可、必須の順。
    std::vector<rectangle_pack_item> items{
        {30, 20, 1, true, true},
        {40, 10, 1, false, true}
    };
    rectangle_pack_options options;
    options.seed = 7;
    options.restart_limit = 32;
    auto result = pack_rectangles_fixed_bin(items, bin_width, bin_height, options);
    if (result.placed_count != static_cast<int>(items.size()))
        throw std::runtime_error("全配置する候補が見つからなかった");
    std::string error;
    if (!validate_rectangle_packing(items, result, bin_width, bin_height, true, &error))
        throw std::logic_error(error);
    return result; // result.placements[i]がitems[i]の配置。
}
```

U02では入力にprofitと必須フラグを加える。U05では枠寸法の代わりにobjectiveを作る。U08〜U10、U12、U14では、呼出前に初期配置の準備・集計・検査が必要になる。

## 6. ユースケースごとの使い方

### 6.0 全ケースに共通する引数と結果

公開の「solve」はクラスのメソッドではなく自由関数である。以下で使う名前の型は次のとおり。

| 名前 | 型 | 意味 |
|---|---|---|
| `items` | `std::span<const rectangle_pack_item>` | 入力配列。通常はvectorをそのまま渡す |
| `initial` | `const rectangle_pack_result&` | itemsと同じ個数・ID順の有効な初期解 |
| `ids` | `std::span<const int>` | 動かしてよいID。範囲内で重複なし |
| `W`, `H` | `long long` | 正の固定幅・高さ |
| `objective` | `const rectangle_bounding_objective&` | 外接目的 |
| `options` | `const rectangle_pack_options&` | 探索設定。全solverで省略時は`{}` |

packing・improve・repair・repackの戻り値はすべて`rectangle_pack_result`。初期解への参照引数そのものは書き換えない。保持したい結果は戻り値を変数へ代入する。

空入力に対する通常packingは、placementsが空で集計値0の結果になる。枠を取る関数の幅・高さは、空入力でも正の値を渡す。初期解を渡すAPIでは空入力でも要素数の整合が必要である。

### 6.1 U01：固定ビンの全配置

呼出は`pack_rectangles_fixed_bin(items, W, H, options)`。全矩形を`required=true`にして準備する。枠寸法には既定値がない。

`result.placed_count == static_cast<int>(items.size())`を確認する。各要素は`placements[i]`、通常の配置ビンは0。検査は`validate_rectangle_packing(items, result, W, H, true, &error)`とする。8章のwrapperは全配置成功時に`std::optional<rectangle_pack_result>`へ結果を入れ、見つからなければ`std::nullopt`を返す。

### 6.2 U02：profit付き選択

同じ`pack_rectangles_fixed_bin(items, W, H, options)`を使い、任意矩形だけ`required=false`にする。8章のwrapperは必須profitを0へ設定するので、`selected_profit`が選んだ任意矩形の価値合計になる。

まず`result.feasible()`で必須の配置を確認する。任意の採否は`placements[i].placed()`、価値は`placed_profit`から読む。validatorの`require_all_items`はfalseにする。trueにすると置かなかった任意矩形もエラーになる。

### 6.3 U03：strip

呼出は`pack_rectangles_strip(items, W, options)`。高さ引数はない。全入力を`required=true`とし、`search_iteration_limit`を設定する。

使用長は`result.used_height`。全個数を確認してから、`validate_rectangle_packing(items, result, W, 0, true, &error)`で検査する。validatorの高さ0は「高さ上限を検査しない」という意味であり、solverへ高さ0を指定する意味ではない。

### 6.4 U04：複数ビン

呼出は`pack_rectangles_multiple_bins(items, W, H, options)`。全入力を必須にし、全ビン共通の寸法を渡す。`search_iteration_limit`を使う。

使用個数は`bin_count`、各矩形の所属は`placements[i].bin`。8章のwrapperは`item_ids_by_bin[b]`へビンbの矩形番号をまとめる。validatorでは`W,H,true`を指定する。利用可能な最大ビン数はAPI引数にはないので、必要なら戻り値と自分の上限を比較する。

### 6.5 U05：外接矩形

呼出は`pack_rectangles_bounding_box(items, objective, options)`。objectiveも省略時は`{}`、つまり面積目的。optionsだけ変えたい場合も、`pack_rectangles_bounding_box(items, {}, options)`のように第2引数のobjectiveを置く。

外枠は`used_width, used_height`、目的値は`objective.evaluate(result.used_width, result.used_height)`から得る。検査は`validate_rectangle_packing(items, result, 0, 0, true, &error)`。8章のwrapperは配置と目的値を一緒に返す。

### 6.6 U06：許可外枠からの選択

`std::vector<std::pair<long long,long long>>`に候補の幅・高さを入れる。各候補でU01を実行し、全配置できたものだけ比較する。候補の寸法はすべて正にする。

8章の`solve_best_allowed_canvas`は入力候補番号`candidate_index`、選んだ枠寸法、配置を返す。候補一覧が空、または成功する候補がない場合はnullopt。同じ面積の候補の優先規則もwrapper内で明示している。枠候補間でprofitを比較しているわけではない。

### 6.7 U07：初期解を保護した全体再探索

最初は`pack_*`で結果を作り、その後は形式に応じた次の関数を使う。

| 形式 | 呼出 | 保護される比較規則 |
|---|---|---|
| 固定ビン | `improve_rectangles_fixed_bin(items, initial, W, H, options)` | 1.3の辞書式 |
| Strip | `improve_rectangles_strip(items, initial, W, options)` | 1.4の辞書式 |
| 複数ビン | `improve_rectangles_multiple_bins(items, initial, W, H, options)` | 1.5の辞書式 |
| 外接矩形 | `improve_rectangles_bounding_box(items, initial, objective, options)` | 渡したobjectiveによる1.6の辞書式 |

全関数でoptionsは`{}`が既定値。外接矩形ではobjectiveも`{}`が既定値。座標の同一性ではなく、各比較規則に対する非悪化を保つ。同点なら座標が変わる場合がある。

8章では`std::span<const std::uint64_t>`のseed一覧を渡すstrip用wrapperを示す。空seed一覧は「試行指定なし」としてnullopt。各seedの予算は例では12だが、ライブラリの既定値32を変更するものではない。必須を置けない中間解も初期解にできるが、配置済み部分の幾何と集計が正しい必要がある。

### 6.8 U08：標準目的の部分repair

呼出は`repair_rectangles_fixed_bin(items, initial, ids, W, H, options)`。動かせない配置は`bin=0`で、有効な寸法・回転・座標を持っていなければならない。初期解は未配置requiredを含んでもよい。

`restart_limit`で追加探索を調整する。戻り値は固定ビンの標準目的でinitialより悪くない。idsに入れなかった要素は配置済み・未配置の別も含めて不変。8章のwrapperは入力の可動IDの重複・範囲外を例外として検出する。

### 6.9 U09：外部スコアによる採否

呼出は`repack_rectangles_fixed_bin(items, initial, ids, W, H, options)`。引数の意味はU08と同じだが、initialの保護は行わない。結果が悪ければ呼出側で捨てる必要がある。

8章の`try_repack_fixed_bin_by_external_score`は、`score(result)`が小さいほど良いという約束を使う。改善したときだけ参照引数`current`を置き換えてtrueを返す。改善しなければfalseでcurrentは不変。同点も置き換えない。

Scoreは比較可能な値を返す純粋な関数として用意する。欠落を優先するなら、戻り値を`std::pair{result.missing_required_count, 独自の費用}`のようにする。NaNを返す浮動小数スコアや、呼ぶたびに状態が変わる評価は避ける。スコア関数は候補構築の内部には渡されず、候補生成後にだけ呼ばれる。

### 6.10 U10：障害物

入力を`[障害物, 新しく置く矩形]`の順に連結する。障害物のplacementsを固定座標・bin0で初期化し、新規矩形は未配置のままにする。集計を再計算し、未配置requiredを許して検査してから、新規矩形のIDだけを可動にしてrepackする。

8章の戻り値では、障害物数を`obstacles.size()`とすると、通常の矩形iの配置は`placements[obstacles.size()+i]`。障害物も`placed_count`と`placed_area`に含まれる。必要な表示・得点では障害物分を除いて読む。

### 6.11 U11：隙間と余白

各寸法を幅・高さともgapだけ拡張し、枠は外周余白を引いてからgapを加える。呼出は拡張入力に対する固定ビンpackingである。

$$w'_i=w_i+g,\quad h'_i=h_i+g,\quad W'=W-2m+g,\quad H'=H-2m+g.$$

$w'_i,h'_i$は探索用の拡張寸法、$W',H'$は探索用の枠、$g$は`gap`、$m$は`border`である。$W-2m,H-2m$はともに正が必要。成功したら左上座標へborderを足し、配置幅・高さからgapを引く。等量拡張なので90度回転にも対応する。

こうすると拡張矩形の非重複が元矩形間のgapを保証し、探索枠に加えた末尾のgapを取り除くことで外周に余分なgapを要求しない。最後に元入力で集計を再計算する。通常validatorはgapやborder自体を検査しないため、独自条件の検査も別に行う。

### 6.12 U12：外部配置と補助API

外部配置の集計は`recompute_rectangle_packing_summary(items, result)`で作る。この関数の戻り値はvoidで、resultの集計フィールドだけを変更する。placementsの数はitemsと同じ必要があり、座標の修正・重複の解消・IDの対応付けはしない。

検査関数の全引数を省略せずに書くと、`validate_rectangle_packing(items, result, bin_width, bin_height, require_all_items, error_message, allow_missing_required)`である。

| 引数 | 型 | 既定値 | 使い方 |
|---|---|---|---|
| `items` | `std::span<const rectangle_pack_item>` | なし | 対応する入力 |
| `result` | `const rectangle_pack_result&` | なし | 検査する配置と集計 |
| `bin_width` | `long long` | `0` | 正なら幅上限を検査。0以下なら検査しない |
| `bin_height` | `long long` | `0` | 正なら高さ上限を検査。0以下なら検査しない |
| `require_all_items` | `bool` | `false` | trueなら任意矩形も含め未配置を許さない |
| `error_message` | `std::string*` | `nullptr` | 失敗理由の出力先。不要ならnullptr |
| `allow_missing_required` | `bool` | `false` | require_all_itemsがfalseのときだけ、未配置requiredを許す |

| 検査したい状態 | require_all_items | allow_missing_required |
|---|---|---|
| 全配置済みの完成解 | true | false |
| 必須はすべて配置、任意は未配置でもよい | false | false |
| 破壊・挿入途中の中間解 | false | true |

戻り値がtrueなら、検査対象の寸法・回転・非負座標・同ビン内非重複・指定上限・集計が一致する。失敗時はfalseで、error_messageがあれば理由を設定する。成功時に以前のエラー文字列を消す動作はないため、文字列ではなくboolで成否を判定する。

単一ビンという条件はvalidatorに含まれない。U12のwrapperは、配置済みの`bin`が0かも別に確認し、必須を配置済みの外部解だけを受け入れてから`improve_rectangles_fixed_bin`へ渡す。未配置requiredを含む取込みが必要なら、検査設定を変え、完成解かどうかを別に確認する。

### 6.13 U13：共通deadline

外側で`std::chrono::steady_clock::now()`を記録し、終了時刻を1つ作る。たとえば全体が2000msなら、出力・終了用に50msを残した`開始時刻 + milliseconds(1950)`を出発点にできる。ただし50msで必ず十分という保証ではない。

この時刻を`options.deadline`へ設定する。8章のwrapperは呼出前に期限切れなら重い構築を開始せず、全矩形未配置の結果を返す。構築開始後はライブラリの停止単位に従うため、時刻を厳密に守る保証はない。返却後のvalidatorや独自スコア計算の時間も、外側の予算へ含める。

### 6.14 U14：変更後の問題へ配置を引き継ぐ

8章の`solve_updated_fixed_bin_turn`では、`new_items`、`previous`、`new_to_old`、`extra_movable`、変更後の枠、seedを渡す。

- `new_to_old[i] >= 0`なら、次のi番は以前の指定IDに対応する。
- `new_to_old[i] == -1`なら新規矩形。それ以外の負値と以前のIDの重複は禁止。
- 対応表に現れない以前の矩形は削除した扱い。
- `extra_movable`は次ターン側のIDで指定する。
- 寸法・回転状態が新入力と不一致、枠外、未配置、新規の矩形は、自動で配置を解除して可動にする。

変更後の属性で集計を作り、固定配置が互いに有効な中間解であることを確認してrepairする。返る配列は次ターン側の順序。`feasible()`がfalseなら、残した固定配置のために余地が足りない可能性もある。可動範囲を広げるか、通常packingで再構築する判断は外側で行う。

## 7. 制約・注意点

### 7.1 入力と整数演算

- 各寸法とsolverへ渡す枠寸法は正。矩形数・可動IDはintで表せる範囲にする。
- 座標・寸法・profitの合計、各面積、比較用の幅×高さ、重み付き和は`long long`に収める。
- strip・外接では内部の高さ上限として各矩形の幅+高さの合計を使う。その合計だけでなく、探索幅との積も範囲内にする。
- 面積下界の切上げに使う面積合計+幅−1、外接幅の割合計算、丸め前の幅候補も安全な範囲にする。計算後に上限へ丸める処理があっても、計算途中のoverflowを防ぐとは限らない。
- 固定ビン比較ではprofit合計を符号反転するので、`LLONG_MIN`を取らせない。負の任意profitは新規の配置候補から外れるが、固定された既存配置や保護された初期解の中身まで自動で削除するものではない。
- validatorも集計を計算してから幾何を検査する。極端な不正数値を安全に受け入れる入力サニタイザーではない。
- 多くの入力前提はassertであり、`-DNDEBUG`では無効になる。外部入力をそのまま信用してよいという意味ではない。

### 7.2 使えない組合せと、効かない設定

| 指定・期待 | 実際の扱い／代替 |
|---|---|
| strip・複数ビン・外接でoptionalを選別 | 正式な使い方は全入力required。選択は外側で行うか固定ビンを使う |
| 固定ビンで`search_iteration_limit`だけ増やす | 固定ビンの追加探索を制御するのは`restart_limit` |
| strip・複数ビン・外接で`restart_limit`だけ増やす | 追加探索予算は`search_iteration_limit`。複数ビン内部の部分問題予算も独自に設定される |
| 予算0で何もせず即時return | 初期候補は構築する。strip・外接では最終の隙間詰めも行う |
| weighted_sumのweightを0や負にする | 両方正が前提。幅固定を表したいならstripを使う |
| 外接目的のweightで寸法上限を保証 | 不可。上限は目的値ではなく制約として別に扱う |
| 部分repairへ複数ビンの解を直接渡す | 不可。配置済みはbin0だけ。対象ビンの部分問題を外側で切り出す必要がある |
| repackが自分の外部スコアを最大化 | 外部スコアは内部探索に渡らない。候補の採否を外側で決める |
| required=trueなら必ず置ける | 置けない候補も返る。必須欠落を最優先で少なくするという意味 |

### 7.3 既存解を渡す際の落とし穴

- 入力とplacementsは同じID順である必要がある。配列を並べ替えたら両方を対応付ける。
- 配置済みの幅・高さは回転後の寸法で、rotatedフラグとも一致させる。正方形では通常向きと回転後の数値が同じでも、回転許可の検査は残る。
- placementsやprofit、requiredを変更したら集計を再計算する。再計算だけで重なりは直らない。
- improveやrepairは初期解の幾何を自動検査しない。不正な初期解を「良い解」として保護してしまう可能性がある。
- 可動IDの重複と範囲外は禁止。未配置の矩形をidsへ入れ忘れると、部分再配置では置かれない。
- repairの非悪化は標準の辞書式に対してだけであり、座標変更費用や外部スコアには適用されない。

### 7.4 validatorの範囲

validatorは単一ビンのbin0制約、最大ビン数、ビン番号の欠番、障害物IDが固定か、可動集合外が不変か、gap・border、目的値の最適性を検査しない。必要なものは外側で検査する。

未配置の判定は`bin < 0`である。`bin == -1`だけと解釈しない。未配置要素の古い寸法・座標は通常の配置検査対象ではないが、利用側が誤用しないよう初期化しておくとよい。

### 7.5 時間と再現性

期限は候補の途中を強制中断する仕組みではない。固定ビンでは1基本順序の3候補、外接では1幅の複数候補など、まとめて作ってから時計を見る箇所がある。`time_check_interval=1`でも1矩形ごとの終了確認にはならない。

同じ入力・seed・予算でも、実時間のdeadlineで止めれば環境の混雑により探索量と結果が変わる。比較実験ではdeadlineを外し、同じ入力群と設定で比較する。予算を増やすと探索配分や狙う高さも変わり得るので、別の予算設定間でスコアが必ず単調に改善すると仮定しない。

### 7.6 表現できない問題

任意角度の回転、多角形の厳密な形状、3次元、guillotine切断順、矩形同士の先行関係、接続・配線条件、矩形ごとの許可位置、異なる寸法や費用を持つビンの選択は、そのままの入力では表せない。

固定障害物、共通gap、規格候補などは外側のモデル化で扱えるが、そのモデルが元の条件を本当に表しているかは別途確認する。特に、配置可能であることと、物理的にその配置へ移動・加工できることは異なる。

### 7.7 戻り値と失敗の意味

`feasible()`は必須の個数だけを見る。全入力requiredの設定では全配置判定として使えるが、外部resultでは集計の正しさを先に確認する。例のnulloptは「この探索で採用できる候補が見つからない」という意味であり、不可能性証明ではない。

規格候補を試す場合も同じで、ある候補で失敗したからといって、その候補を厳密に不可能と断定しない。厳密な判定が必要な部分には2章の手段を検討する。

## 8. ユースケースごとのコード例

### 8.0 コピーと入出力の約束

以下のU01〜U14は、それぞれ先頭のincludeから独立してコピーできる。別のU番号の補助関数を先にコピーする必要はない。ソルバーは関数形で、入力用の型を使う例では各フィールドへコメントを付けている。

これらはガイド側のwrapperであり、ライブラリへ追加された公開APIではない。例のseed既定値は1、個別予算の既定値は関数宣言に記載した値で、5章のライブラリ既定値とは区別する。寸法・演算範囲などの前提は7章を満たすものとし、assertは入力契約の確認である。入力用構造体の各フィールドはすべて明示して初期化する。省略したboolがライブラリと同じ既定値trueになるとは限らない。

`std::optional`を返す例では、まず`if (!answer)`で候補が得られたかを確認する。nulloptは最適性・不可能性の証明ではない。resultを直接返す例では、未配置requiredを含む中間解もあり得る。初期解の不正やID不正を例外で返すものは、各例の説明に従う。

コードをまとめた実行可能なファイルは[rectangle_packing_examples_v09.cpp](rectangle_packing_examples_v09.cpp)。末尾のmainには代表例と境界ケースの検査を含む。実際の提出では必要な関数だけをコピーしてよい。

### 8.1 U01：固定ビンの全配置

piecesの要素は幅、高さ、回転可否。全要素を必須に変換する。例として`solve_fixed_bin_all({{3, 2, false}, {2, 2, true}}, 8, 6)`を呼べる。戻り値があれば、入力順のplacementsを使う。

```cpp
#include "rectangle_packing_v09.hpp"

struct fixed_bin_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

// 入力: pieces、固定ビン寸法、乱数seed、追加探索回数
// 出力: 全矩形を配置できれば配置結果、できなければnullopt
std::optional<rectangle_pack_result> solve_fixed_bin_all(
    const std::vector<fixed_bin_piece>& pieces,
    long long bin_width,
    long long bin_height,
    std::uint64_t seed = 1,
    int restart_limit = 32) {
    assert(bin_width > 0 && bin_height > 0);
    assert(restart_limit >= 0);

    std::vector<rectangle_pack_item> items;
    items.reserve(pieces.size());
    for (const fixed_bin_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        items.push_back({piece.width, piece.height, 1,
                         piece.rotatable, true});
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    rectangle_pack_result result =
        pack_rectangles_fixed_bin(items, bin_width, bin_height, options);

    if (!result.feasible() ||
        result.placed_count != static_cast<int>(items.size())) {
        return std::nullopt;
    }

    std::string error;
    if (!validate_rectangle_packing(
            items, result, bin_width, bin_height, true, &error)) {
        return std::nullopt;
    }
    return result;
}
```

### 8.2 U02：profit付き任意選択

piecesの要素は幅、高さ、profit、回転可否、必須かどうか。必須profitはこのwrapper内で0にする。`selected_optional_ids`は入力側の0始まりの番号。必須を置けなければnullopt。

```cpp
#include "rectangle_packing_v09.hpp"

struct profitable_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    long long profit; // 任意矩形を選ぶ価値。大きいほど優先したい
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
    bool required; // 必ず置きたいならtrue。採否を選ばせるならfalse
};

struct profitable_fixed_bin_answer {
    rectangle_pack_result packing;
    std::vector<int> selected_optional_ids;
    long long selected_profit;
};

// 入力: 必須・任意矩形と固定ビン寸法
// 出力: 必須矩形を全配置できれば、選択集合と配置。できなければnullopt
std::optional<profitable_fixed_bin_answer> solve_profitable_fixed_bin(
    const std::vector<profitable_piece>& pieces,
    long long bin_width,
    long long bin_height,
    std::uint64_t seed = 1,
    int restart_limit = 32) {
    assert(bin_width > 0 && bin_height > 0);
    assert(restart_limit >= 0);

    std::vector<rectangle_pack_item> items;
    items.reserve(pieces.size());
    for (const profitable_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        const long long packing_profit = piece.required ? 0 : piece.profit;
        items.push_back({piece.width, piece.height, packing_profit,
                         piece.rotatable, piece.required});
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    rectangle_pack_result result =
        pack_rectangles_fixed_bin(items, bin_width, bin_height, options);

    if (!result.feasible()) return std::nullopt;

    std::string error;
    if (!validate_rectangle_packing(
            items, result, bin_width, bin_height, false, &error)) {
        return std::nullopt;
    }

    profitable_fixed_bin_answer answer;
    answer.packing = std::move(result);
    answer.selected_profit = answer.packing.placed_profit;
    for (std::size_t id = 0; id < pieces.size(); ++id) {
        if (!pieces[id].required && answer.packing.placements[id].placed()) {
            answer.selected_optional_ids.push_back(static_cast<int>(id));
        }
    }
    return answer;
}
```

### 8.3 U03：stripの使用高さ最小化

piecesの要素は幅、高さ、回転可否。`strip_width`だけを固定し、使用高さを`used_height`から読む。各矩形に、その幅へ入る向きがあるかも確認する。

```cpp
#include "rectangle_packing_v09.hpp"

struct strip_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

// 入力: pieces、固定strip幅、乱数seed、追加探索予算
// 出力: 全矩形の配置。使用長はresult.used_height。失敗時はnullopt
std::optional<rectangle_pack_result> solve_strip_height(
    const std::vector<strip_piece>& pieces,
    long long strip_width,
    std::uint64_t seed = 1,
    int search_iteration_limit = 32) {
    assert(strip_width > 0);
    assert(search_iteration_limit >= 0);

    std::vector<rectangle_pack_item> items;
    items.reserve(pieces.size());
    for (const strip_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        items.push_back({piece.width, piece.height, 1,
                         piece.rotatable, true});
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.search_iteration_limit = search_iteration_limit;
    rectangle_pack_result result =
        pack_rectangles_strip(items, strip_width, options);

    if (result.placed_count != static_cast<int>(items.size())) {
        return std::nullopt;
    }

    std::string error;
    if (!validate_rectangle_packing(
            items, result, strip_width, 0, true, &error)) {
        return std::nullopt;
    }
    return result;
}
```

### 8.4 U04：同一サイズの複数ビン

全ビン共通の幅・高さを指定する。`item_ids_by_bin[b]`はビンbに入った入力ID一覧。1個でもビンに入らない場合はnullopt。

```cpp
#include "rectangle_packing_v09.hpp"

struct multiple_bin_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

struct multiple_bin_answer {
    rectangle_pack_result packing;
    std::vector<std::vector<int>> item_ids_by_bin;
};

// 入力: piecesと全ビン共通の幅・高さ
// 出力: 全配置とビン別index列。1個でも入らなければnullopt
std::optional<multiple_bin_answer> solve_identical_bins(
    const std::vector<multiple_bin_piece>& pieces,
    long long bin_width,
    long long bin_height,
    std::uint64_t seed = 1,
    int search_iteration_limit = 32) {
    assert(bin_width > 0 && bin_height > 0);
    assert(search_iteration_limit >= 0);

    std::vector<rectangle_pack_item> items;
    items.reserve(pieces.size());
    for (const multiple_bin_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        items.push_back({piece.width, piece.height, 1,
                         piece.rotatable, true});
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.search_iteration_limit = search_iteration_limit;
    rectangle_pack_result result =
        pack_rectangles_multiple_bins(
            items, bin_width, bin_height, options);

    if (result.placed_count != static_cast<int>(items.size())) {
        return std::nullopt;
    }

    std::string error;
    if (!validate_rectangle_packing(
            items, result, bin_width, bin_height, true, &error)) {
        return std::nullopt;
    }

    multiple_bin_answer answer;
    answer.item_ids_by_bin.resize(
        static_cast<std::size_t>(result.bin_count));
    for (std::size_t id = 0; id < result.placements.size(); ++id) {
        const int bin = result.placements[id].bin;
        answer.item_ids_by_bin[static_cast<std::size_t>(bin)].push_back(
            static_cast<int>(id));
    }
    answer.packing = std::move(result);
    return answer;
}
```

### 8.5 U05：自由な外接枠

目的を省略すると面積。周長なら`rectangle_bounding_objective{rectangle_bounding_objective_type::perimeter, 1, 1}`、幅を3倍で評価するなら`rectangle_bounding_objective{rectangle_bounding_objective_type::weighted_sum, 3, 1}`を第2引数へ渡す。`objective_value`と配置を返す。

```cpp
#include "rectangle_packing_v09.hpp"

struct bounding_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

struct bounding_box_answer {
    rectangle_pack_result packing;
    long long objective_value;
};

// 入力: pieces、外接矩形の目的、乱数seed、追加探索予算
// 出力: 全配置、外接幅・高さ、目的値。異常時はnullopt
std::optional<bounding_box_answer> solve_free_bounding_box(
    const std::vector<bounding_piece>& pieces,
    const rectangle_bounding_objective& objective = {},
    std::uint64_t seed = 1,
    int search_iteration_limit = 32) {
    assert(search_iteration_limit >= 0);
    assert(objective.type != rectangle_bounding_objective_type::weighted_sum ||
           (objective.width_weight > 0 && objective.height_weight > 0));

    std::vector<rectangle_pack_item> items;
    items.reserve(pieces.size());
    for (const bounding_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        items.push_back({piece.width, piece.height, 1,
                         piece.rotatable, true});
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.search_iteration_limit = search_iteration_limit;
    rectangle_pack_result result =
        pack_rectangles_bounding_box(items, objective, options);

    if (result.placed_count != static_cast<int>(items.size())) {
        return std::nullopt;
    }

    std::string error;
    if (!validate_rectangle_packing(
            items, result, 0, 0, true, &error)) {
        return std::nullopt;
    }

    const long long value = objective.evaluate(
        result.used_width, result.used_height);
    return bounding_box_answer{std::move(result), value};
}
```

### 8.6 U06：許可された外枠候補の選択

candidatesは幅・高さの組の配列。たとえば`{{256, 256}, {512, 256}, {512, 512}}`。`candidate_index`はこの配列の番号であり、入力順をソートし直さず保持する。成功した候補だけを外枠の基準で比較する。

```cpp
#include "rectangle_packing_v09.hpp"

struct allowed_canvas_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

struct allowed_canvas_answer {
    std::size_t candidate_index;
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    rectangle_pack_result packing;
};

// 入力: piecesと使用可能な外枠候補
// 出力: 全配置できた最小候補と配置。候補が見つからなければnullopt
std::optional<allowed_canvas_answer> solve_best_allowed_canvas(
    const std::vector<allowed_canvas_piece>& pieces,
    const std::vector<std::pair<long long, long long>>& candidates,
    std::uint64_t seed = 1,
    int restart_limit = 32) {
    assert(restart_limit >= 0);

    std::vector<rectangle_pack_item> items;
    items.reserve(pieces.size());
    for (const allowed_canvas_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        items.push_back({piece.width, piece.height, 1,
                         piece.rotatable, true});
    }

    std::optional<allowed_canvas_answer> best;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto [width, height] = candidates[index];
        assert(width > 0 && height > 0);

        rectangle_pack_options options;
        options.seed = seed +
            0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(index + 1);
        options.restart_limit = restart_limit;
        rectangle_pack_result result =
            pack_rectangles_fixed_bin(items, width, height, options);
        if (result.placed_count != static_cast<int>(items.size())) continue;

        std::string error;
        if (!validate_rectangle_packing(
                items, result, width, height, true, &error)) {
            continue;
        }

        const auto score = std::tuple(
            width * height, std::max(width, height), width, height,
            index);
        if (best) {
            const auto best_score = std::tuple(
                best->width * best->height,
                std::max(best->width, best->height),
                best->width, best->height, best->candidate_index);
            if (!(score < best_score)) continue;
        }
        best = allowed_canvas_answer{
            index, width, height, std::move(result)};
    }
    return best;
}
```

### 8.7 U07：複数seedのstrip再探索

たとえば`std::array<std::uint64_t, 3> seeds{1, 2, 3};`を準備する。空seed一覧はnullopt。返るのは指定した試行全体の最良解であり、初期配置の座標固定ではない。

```cpp
#include "rectangle_packing_v09.hpp"

struct multiseed_strip_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

// 入力: strip問題、試すseed列、各seedの追加探索予算
// 出力: 全seedを通した最良の全配置。失敗時はnullopt
std::optional<rectangle_pack_result> solve_strip_with_multiple_seeds(
    const std::vector<multiseed_strip_piece>& pieces,
    long long strip_width,
    std::span<const std::uint64_t> seeds,
    int iterations_per_seed = 12) {
    assert(strip_width > 0);
    assert(iterations_per_seed >= 0);
    if (seeds.empty()) return std::nullopt;

    std::vector<rectangle_pack_item> items;
    items.reserve(pieces.size());
    for (const multiseed_strip_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        items.push_back({piece.width, piece.height, 1,
                         piece.rotatable, true});
    }

    rectangle_pack_options options;
    options.seed = seeds.front();
    options.search_iteration_limit = iterations_per_seed;
    rectangle_pack_result best =
        pack_rectangles_strip(items, strip_width, options);

    for (std::size_t i = 1; i < seeds.size(); ++i) {
        options.seed = seeds[i];
        best = improve_rectangles_strip(
            items, best, strip_width, options);
    }

    if (best.placed_count != static_cast<int>(items.size())) {
        return std::nullopt;
    }
    std::string error;
    if (!validate_rectangle_packing(
            items, best, strip_width, 0, true, &error)) {
        return std::nullopt;
    }
    return best;
}
```

### 8.8 U08：標準目的の部分repair

itemsとcurrentは同じ入力ID順にする。movable_item_idsには、置きたい未配置矩形と動かしてよい配置済み矩形の両方を入れる。前提違反は例外、正常時は中間解を含むresultを返すので、必要ならfeasible()も確認する。

```cpp
#include "rectangle_packing_v09.hpp"

// 入力: 固定ビンの現在解と、今回だけ動かしてよい矩形ID
// 出力: 固定ビン比較でcurrentより悪化しない配置
rectangle_pack_result repair_fixed_bin_neighborhood(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& current,
    std::span<const int> movable_item_ids,
    long long bin_width,
    long long bin_height,
    std::uint64_t seed,
    int restart_limit = 8) {
    assert(bin_width > 0 && bin_height > 0);
    assert(restart_limit >= 0);

    std::string error;
    if (!validate_rectangle_packing(
            items, current, bin_width, bin_height,
            false, &error, true)) {
        throw std::invalid_argument(error);
    }
    for (const rectangle_pack_placement& placement : current.placements) {
        if (placement.placed() && placement.bin != 0) {
            throw std::invalid_argument("固定ビンの配置はbin=0が必要");
        }
    }

    std::vector<unsigned char> seen(items.size(), 0);
    for (int id : movable_item_ids) {
        if (id < 0 || static_cast<std::size_t>(id) >= items.size() ||
            seen[static_cast<std::size_t>(id)] != 0) {
            throw std::invalid_argument("movable IDが範囲外または重複");
        }
        seen[static_cast<std::size_t>(id)] = 1;
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    rectangle_pack_result result = repair_rectangles_fixed_bin(
        items, current, movable_item_ids,
        bin_width, bin_height, options);

    if (!validate_rectangle_packing(
            items, result, bin_width, bin_height,
            false, &error, true)) {
        throw std::logic_error(error);
    }
    return result;
}
```

### 8.9 U09：外部スコアによるrepackの採否

scoreは小さい方を良しとする評価関数。例えば`[](const rectangle_pack_result& r) { return std::pair{r.missing_required_count, r.used_width * r.used_height}; }`。必須欠落を許さないなら、例のように欠落を第1キーへ入れる。採択時だけcurrentを書き換える。

```cpp
#include "rectangle_packing_v09.hpp"

// scoreは小さい値ほど良い。未配置を許さない場合は欠落数を第1キーにする。
// 戻り値がtrueならcurrentを候補で置き換えた。falseならcurrentは不変。
template <class Score>
bool try_repack_fixed_bin_by_external_score(
    std::span<const rectangle_pack_item> items,
    rectangle_pack_result& current,
    std::span<const int> movable_item_ids,
    long long bin_width,
    long long bin_height,
    Score score,
    std::uint64_t seed,
    int restart_limit = 8) {
    assert(bin_width > 0 && bin_height > 0);
    assert(restart_limit >= 0);

    std::string error;
    if (!validate_rectangle_packing(
            items, current, bin_width, bin_height,
            false, &error, true)) {
        throw std::invalid_argument(error);
    }
    for (const rectangle_pack_placement& placement : current.placements) {
        if (placement.placed() && placement.bin != 0) {
            throw std::invalid_argument("固定ビンの配置はbin=0が必要");
        }
    }

    std::vector<unsigned char> seen(items.size(), 0);
    for (int id : movable_item_ids) {
        if (id < 0 || static_cast<std::size_t>(id) >= items.size() || seen[static_cast<std::size_t>(id)])
            throw std::invalid_argument("movable IDが範囲外または重複");
        seen[static_cast<std::size_t>(id)] = 1;
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    rectangle_pack_result candidate = repack_rectangles_fixed_bin(
        items, current, movable_item_ids,
        bin_width, bin_height, options);

    if (!validate_rectangle_packing(
            items, candidate, bin_width, bin_height,
            false, &error, true)) {
        throw std::logic_error(error);
    }

    const auto current_score = score(current);
    const auto candidate_score = score(candidate);
    if (candidate_score < current_score) {
        current = std::move(candidate);
        return true;
    }
    return false;
}
```

### 8.10 U10：固定矩形障害物

obstaclesの要素はx、y、幅、高さ。返るplacementsの先頭は障害物、その後がpieces。既存領域を動かさない用途なので、全体improveへそのまま渡して障害物まで動かさないよう注意する。

```cpp
#include "rectangle_packing_v09.hpp"

struct obstacle_rectangle {
    long long x; // 左上の横座標。ビン左端からの距離
    long long y; // 左上の縦座標。ビン上端からの距離
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
};

struct obstacle_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

// 入力: 固定矩形障害物と、その隙間へ置くpieces
// 出力: [障害物, pieces]の順の配置。全piecesを置けなければnullopt
std::optional<rectangle_pack_result> solve_fixed_bin_with_obstacles(
    const std::vector<obstacle_piece>& pieces,
    const std::vector<obstacle_rectangle>& obstacles,
    long long bin_width,
    long long bin_height,
    std::uint64_t seed = 1,
    int restart_limit = 32) {
    assert(bin_width > 0 && bin_height > 0);
    assert(restart_limit >= 0);
    assert(obstacles.size() + pieces.size() <=
           static_cast<std::size_t>(std::numeric_limits<int>::max()));

    std::vector<rectangle_pack_item> items;
    items.reserve(obstacles.size() + pieces.size());
    for (const obstacle_rectangle& obstacle : obstacles) {
        assert(obstacle.width > 0 && obstacle.height > 0);
        items.push_back({obstacle.width, obstacle.height, 0, false, true});
    }
    for (const obstacle_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        items.push_back({piece.width, piece.height, 1,
                         piece.rotatable, true});
    }

    rectangle_pack_result initial;
    initial.placements.resize(items.size());
    for (std::size_t id = 0; id < obstacles.size(); ++id) {
        const obstacle_rectangle& obstacle = obstacles[id];
        initial.placements[id] = {
            obstacle.x, obstacle.y, obstacle.width, obstacle.height, 0, false};
    }
    recompute_rectangle_packing_summary(items, initial);

    std::string error;
    if (!validate_rectangle_packing(
            items, initial, bin_width, bin_height,
            false, &error, true)) {
        return std::nullopt;
    }

    std::vector<int> movable_item_ids;
    movable_item_ids.reserve(pieces.size());
    for (std::size_t id = 0; id < pieces.size(); ++id) {
        movable_item_ids.push_back(static_cast<int>(obstacles.size() + id));
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    rectangle_pack_result result = repack_rectangles_fixed_bin(
        items, initial, movable_item_ids,
        bin_width, bin_height, options);

    if (result.placed_count != static_cast<int>(items.size())) {
        return std::nullopt;
    }
    if (!validate_rectangle_packing(
            items, result, bin_width, bin_height,
            true, &error)) {
        return std::nullopt;
    }
    return result;
}
```

### 8.11 U11：一様なgapとborder

gapは矩形間の共通間隔、borderは外周の共通余白。元の寸法に戻した結果を返す。返ったplacementsのwidthとheightをさらにgapだけ縮めてはいけない。

```cpp
#include "rectangle_packing_v09.hpp"

struct spaced_piece {
    long long width; // 横方向の寸法。正の整数で、他の寸法と同じ単位
    long long height; // 縦方向の寸法。正の整数で、他の寸法と同じ単位
    bool rotatable; // 90度回転を許すならtrue。向きを固定したいならfalse
};

// 入力: 元矩形、外枠、矩形間gap、外周border
// 出力: 元寸法へ戻した配置。全配置できなければnullopt
std::optional<rectangle_pack_result> solve_fixed_bin_with_spacing(
    const std::vector<spaced_piece>& pieces,
    long long bin_width,
    long long bin_height,
    long long gap,
    long long border,
    std::uint64_t seed = 1,
    int restart_limit = 32) {
    assert(bin_width > 0 && bin_height > 0);
    assert(gap >= 0 && border >= 0);
    assert(restart_limit >= 0);
    assert(border < bin_width && border < bin_height);
    assert(border < bin_width - border);
    assert(border < bin_height - border);

    const long long inner_width = bin_width - border - border;
    const long long inner_height = bin_height - border - border;
    assert(gap <= std::numeric_limits<long long>::max() - inner_width);
    assert(gap <= std::numeric_limits<long long>::max() - inner_height);

    std::vector<rectangle_pack_item> original_items;
    std::vector<rectangle_pack_item> expanded_items;
    original_items.reserve(pieces.size());
    expanded_items.reserve(pieces.size());
    for (const spaced_piece& piece : pieces) {
        assert(piece.width > 0 && piece.height > 0);
        assert(gap <= std::numeric_limits<long long>::max() - piece.width);
        assert(gap <= std::numeric_limits<long long>::max() - piece.height);
        original_items.push_back({piece.width, piece.height, 1,
                                  piece.rotatable, true});
        expanded_items.push_back({piece.width + gap, piece.height + gap, 1,
                                  piece.rotatable, true});
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    rectangle_pack_result result = pack_rectangles_fixed_bin(
        expanded_items, inner_width + gap, inner_height + gap, options);
    if (result.placed_count != static_cast<int>(expanded_items.size())) {
        return std::nullopt;
    }

    for (rectangle_pack_placement& placement : result.placements) {
        placement.x += border;
        placement.y += border;
        placement.width -= gap;
        placement.height -= gap;
    }
    recompute_rectangle_packing_summary(original_items, result);

    std::string error;
    if (!validate_rectangle_packing(
            original_items, result, bin_width, bin_height,
            true, &error)) {
        return std::nullopt;
    }
    return result;
}
```

### 8.12 U12：外部配置の取込み

external_placementsの要素はx、y、配置幅、配置高さ、bin、rotatedの順。入力寸法ではなく回転後の寸法を入れる。このwrapperは必須配置済みの外部解を受け入れる。

```cpp
#include "rectangle_packing_v09.hpp"

// 入力: itemsと、同じindex順の外部placements
// 出力: 外部解を保持した固定ビン再探索結果。不正入力ならnullopt
std::optional<rectangle_pack_result> improve_external_fixed_bin_layout(
    std::span<const rectangle_pack_item> items,
    const std::vector<rectangle_pack_placement>& external_placements,
    long long bin_width,
    long long bin_height,
    std::uint64_t seed = 1,
    int restart_limit = 32) {
    assert(bin_width > 0 && bin_height > 0);
    assert(restart_limit >= 0);
    if (external_placements.size() != items.size()) return std::nullopt;

    rectangle_pack_result initial;
    initial.placements = external_placements;
    recompute_rectangle_packing_summary(items, initial);

    for (const rectangle_pack_placement& placement : initial.placements) {
        if (placement.placed() && placement.bin != 0) return std::nullopt;
    }

    std::string error;
    if (!validate_rectangle_packing(
            items, initial, bin_width, bin_height,
            false, &error)) {
        return std::nullopt;
    }

    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    rectangle_pack_result result = improve_rectangles_fixed_bin(
        items, initial, bin_width, bin_height, options);

    if (!validate_rectangle_packing(
            items, result, bin_width, bin_height,
            false, &error)) {
        return std::nullopt;
    }
    return result;
}
```

### 8.13 U13：共通deadline

入力itemsは全要素required=trueにする。deadlineは外側で安全時間を差し引いた絶対時刻。同じdeadlineを複数の部分問題へ渡す。例の大きい予算は無限ループを意味せず、有限の上限である。

```cpp
#include "rectangle_packing_v09.hpp"

// 入力:
// items: 全矩形をrequired=trueにし、寸法・回転許可を設定する。
// bin_width/bin_height: 全ビン共通の正の寸法。
// deadline: steady_clockによる絶対時刻。出力用の余裕は呼出側で差し引く。
// seed: 探索の乱数seed。時刻以外の条件を固定しても期限停止時は結果が変わり得る。
// 出力: itemsと同じ長さのplacementsを持つ候補。
// 全配置できたかはplaced_countで確認する。期限切れなら未配置の候補を返すこともある。
rectangle_pack_result solve_multiple_bins_before_deadline(
    std::span<const rectangle_pack_item> items,
    long long bin_width,
    long long bin_height,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed) {
    assert(bin_width > 0 && bin_height > 0);
    for (const auto& item : items) {
        assert(item.width > 0 && item.height > 0 && item.required);
        (void)item;
    }
    // 呼出前から期限切れなら、重い構築を開始しない。
    if (std::chrono::steady_clock::now() >= deadline) {
        rectangle_pack_result empty;
        empty.placements.resize(items.size());
        recompute_rectangle_packing_summary(items, empty);
        return empty;
    }
    rectangle_pack_options options;
    options.deadline = deadline;
    options.seed = seed;
    options.search_iteration_limit = 1'000'000; // 回数上限は大きくし、主に期限で止める。
    options.time_check_interval = 1;
    // 内部の停止判定は候補構築後なので、deadlineを超える可能性は残る。
    return pack_rectangles_multiple_bins(items, bin_width, bin_height, options);
}

// 使用例（main内）:
// const auto start = std::chrono::steady_clock::now();
// const auto deadline = start + std::chrono::milliseconds(1950);
// std::vector<rectangle_pack_item> items{{3, 4, 1, true, true}};
// auto answer = solve_multiple_bins_before_deadline(items, 10, 10, deadline, 7);
// answer.placed_count == items.size() を確認し、placementsを出力する。
// 複数の部分問題を解くときも、deadlineを作り直さず同じ値を渡す。
```

### 8.14 U14：ターン間の追加・削除・変更

new_to_oldはnew_itemsと同じ個数。-1が新規、0以上が以前のplacementsの番号。対応表にない以前の矩形は削除扱い。extra_movableは「新しい番号」で指定する。前の幾何配置が有効であることと、整数演算が安全であることは入力契約である。

```cpp
#include "rectangle_packing_v09.hpp"

// 入力:
// new_items: 次ターンの矩形配列。寸法、profit、required、rotatableを更新済みにする。
// previous: 前ターンの有効な単一ビン配置。未配置requiredを含んでもよい。
// new_to_old: new_items[i]が以前の何番か。新規追加なら-1。要素数はnew_itemsと同じ。
//             以前のIDの重複指定は禁止。載せなかった以前の矩形は削除した扱い。
// extra_movable: 追加で動かしてよい「次ターン側」のID。重複は禁止。
// bin_width/bin_height: 次ターンの正のビン寸法。枠外になる以前の配置は解除する。
// seed/restart_limit: 次ターンの探索条件。restart_limitは0以上。
// 出力: 次ターン側のID順の配置。新規・寸法不一致・枠外・未配置の矩形を自動で可動にする。
//       その他はextra_movableに含めない限り座標・寸法・回転が不変。
//       非悪化比較の基準は「次ターンの条件で作った中間解」であり、前ターンのスコアではない。
rectangle_pack_result solve_updated_fixed_bin_turn(
    std::span<const rectangle_pack_item> new_items,
    const rectangle_pack_result& previous,
    std::span<const int> new_to_old,
    std::span<const int> extra_movable,
    long long bin_width,
    long long bin_height,
    std::uint64_t seed,
    int restart_limit = 8) {
    assert(bin_width > 0 && bin_height > 0 && restart_limit >= 0);
    assert(new_items.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    if (new_to_old.size() != new_items.size())
        throw std::invalid_argument("new_to_oldの要素数が不正");

    rectangle_pack_result base;
    base.placements.resize(new_items.size());
    std::vector<unsigned char> used_old(previous.placements.size(), 0);
    std::vector<unsigned char> movable(new_items.size(), 0);
    for (std::size_t i = 0; i < new_items.size(); ++i) {
        const auto& item = new_items[i];
        assert(item.width > 0 && item.height > 0);
        const int old_id = new_to_old[i];
        if (old_id < -1 || (old_id >= 0 && static_cast<std::size_t>(old_id) >= used_old.size()))
            throw std::invalid_argument("以前のIDが範囲外");
        if (old_id >= 0) {
            if (used_old[old_id]) throw std::invalid_argument("以前のIDが重複");
            used_old[old_id] = 1;
            base.placements[i] = previous.placements[old_id];
        }
        auto& p = base.placements[i];
        if (p.placed() && p.bin != 0)
            throw std::invalid_argument("前ターンも単一ビンが必要");
        const bool normal = !p.rotated && p.width == item.width && p.height == item.height;
        const bool rotated = p.rotated && item.rotatable &&
                             p.width == item.height && p.height == item.width;
        const bool inside = p.x >= 0 && p.y >= 0 && p.width > 0 && p.height > 0 &&
                            p.width <= bin_width && p.height <= bin_height &&
                            p.x <= bin_width - p.width && p.y <= bin_height - p.height;
        if (!p.placed() || (!normal && !rotated) || !inside) {
            p = {};
            movable[i] = 1;
        }
    }
    std::vector<unsigned char> extra_seen(new_items.size(), 0);
    for (int id : extra_movable) {
        if (id < 0 || static_cast<std::size_t>(id) >= new_items.size() || extra_seen[id])
            throw std::invalid_argument("追加の可動IDが範囲外または重複");
        extra_seen[id] = 1;
        movable[id] = 1;
    }
    // profitやrequiredの変更もここで集計へ反映する。
    recompute_rectangle_packing_summary(new_items, base);
    std::string error;
    if (!validate_rectangle_packing(new_items, base, bin_width, bin_height, false, &error, true))
        throw std::invalid_argument(error);
    std::vector<int> ids;
    for (std::size_t i = 0; i < movable.size(); ++i)
        if (movable[i]) ids.push_back(static_cast<int>(i));
    rectangle_pack_options options;
    options.seed = seed;
    options.restart_limit = restart_limit;
    auto result = repair_rectangles_fixed_bin(new_items, base, ids, bin_width, bin_height, options);
    if (!validate_rectangle_packing(new_items, result, bin_width, bin_height, false, &error, true))
        throw std::logic_error(error);
    return result;
}

// 使用例（main内。previousは前ターンの結果）:
// std::vector<rectangle_pack_item> next{{3, 3, 1, false, true}, {2, 2, 1, true, true}};
// std::vector<int> mapping{0, -1}; // 以前の0番を残し、次の1番を新規追加。以前の他の矩形は削除。
// std::vector<int> movable;       // 残した0番は動かさない。
// auto answer = solve_updated_fixed_bin_turn(next, previous, mapping, movable, 10, 10, 9);
// answer.feasible()を確認。falseなら可動範囲を広げるか、全体を再構築する。
```

### 8.15 コード例を検証する

ZIPを相対配置を保って展開し、ヘッダーとコード例を同じディレクトリに置く。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror rectangle_packing_examples_v09.cpp -o guide_examples
./guide_examples
python3 verify_guide.py --tag local_check
```

`verify_guide.py`は、Markdownから取り出したC++ブロックを個別にコンパイルし、全例の結合版を通常・NDEBUG・debug STL・ASan/UBSanで実行する。gap・border、固定配置不変、profit選択、追加・削除・変更、失敗時の扱いも検査する。LeakSanitizerは無効としている。新しい実行では既存の記録と異なるtagを指定する。

## 9. 実装

### 9.1 全体の流れ

公開APIからの基本的な流れは「入力の前提確認、複数の並べ方の用意、順に置く構築、候補の比較、追加探索、結果の集計」である。部分repairでは最初に固定領域を用意する。stripと外接では最後に完成配置の隙間を詰める。

同じ矩形群でも、置く順番と置く位置の評価が違うと、後の矩形が入りやすいかが変わる。そこで少数の相補的な構築を組み合わせ、それぞれの問題形式の比較規則で最良候補を残す。全座標を列挙する厳密探索ではない。

実装詳細は`rectangle_pack_result`のprivateな入れ子型`engine`に置かれている。利用者がその状態型や評価則を直接構築・変更する必要はなく、公開APIを通して利用する。

| 内部の役割 | 持つ情報・処理 |
|---|---|
| `box`と`candidate` | 幾何、配置候補の座標・向き・比較キー |
| `maxrects_state` | 配置できる空き矩形の集合と更新 |
| `contact_point_state` | 空き矩形に加え、配置済みの辺の索引 |
| 順序作成・摂動 | 矩形を置く順番を作り、近い順位を入れ替える |
| 各形式の構築処理 | 固定枠、目標高さ、複数ビン、外接幅の探索を組み立てる |
| 集計と比較 | 1章の辞書式に従って候補を残す |
| 期限管理 | 候補や候補群の区切りで終了時刻を確認する |

### 9.2 MaxRects：空いている大きな長方形を持つ

最初の空き領域はビン全体の1矩形。矩形を置くと、それと交差した空き矩形を左右上下の断片へ分ける。別の空き矩形に完全に含まれる断片は不要なので取り除く。

この空き矩形群は、互いに重ならないタイル分割ではない。同じ空間を複数の空き矩形が覆うことがあり、空き矩形の面積を単純合計して空き面積を求めることはできない。その代わり、横長に使える空間と縦長に使える空間の両方を候補として残せる。

通常のMaxRects構築では、各空き矩形の左上へ置く候補を作る。回転を許し、正方形でない矩形は両方の向きを試す。入る候補について次のキーを小さい順に比べる。

空き矩形の幅・高さを$F_w,F_h$、今回の配置幅・高さを$a,b$、左上を$x,y$とし、残りを$\Delta_w=F_w-a,\Delta_h=F_h-b$と定める。

| 評価則 | 比較する4つの値 | 意図 |
|---|---|---|
| BSSF：短辺残差 | $\min(\Delta_w,\Delta_h),\max(\Delta_w,\Delta_h),y+b,x$ | 少なくとも片方の辺をぴったり合わせる |
| BAF：面積残差 | $F_wF_h-ab,\min(\Delta_w,\Delta_h),y+b,x$ | 候補元の空き矩形に対する余りを減らす |
| Bottom-Left | $y+b,x,\min(\Delta_w,\Delta_h),\max(\Delta_w,\Delta_h)$ | 下端を低くし、同点なら左へ寄せる |

この4値が同じなら、候補のy、x、回転フラグの順で比較し、falseの通常向きを先にする。各評価則のキーと、最終結果を比較する1章の目的関数は別物である。

### 9.3 空き領域更新の詳細

空き矩形vectorの元の範囲だけを読み、非交差矩形を前へ詰める。交差部分から生じた断片は末尾へ追加する。vectorの再確保で参照が壊れないよう、読んでいる矩形は値で持つ。

包含除去では、新しい断片を左・右・上・下の切断方向で分ける。同じ方向の断片同士と、対応する切断境界を共有する既存矩形に対象を絞る。これは「空き矩形同士に不要な包含がない」という状態の性質を利用する処理であり、ユーザー入力の種類で動作モードを選んでいるわけではない。

群内で不要になった断片は末尾の要素と交換して除去し、確定した要素を前へ詰める。空き領域を失わないことが重要なので、単に小さな空き矩形を数個捨てるような上限打切りはしていない。

### 9.4 Contact Point：既存の辺へ接する候補を好む

Contact PointもMaxRectsと同じ空き領域を使うが、各空き矩形の左上・右上・左下・右下を試す。左右または上下が一致する候補は重複評価を省く。

第一の評価値は接触スコアの符号反転。接触スコアが大きいほど良い。その後、候補元の空き矩形の面積残差、短辺残差、配置の下端を比較する。最後のy・x・回転による同点処理は共通である。

接触スコアは、既存矩形の対応する辺との重なり長と、壁への接触加点からなる。壁への加点は、左または右に接すれば高さを1回、上または下に接すれば幅を1回足す方式である。両側の壁へ同時に接しても、その軸で2回足すわけではないので、物理的な全接触長そのものと常に一致するわけではない。

既存辺は「辺の座標、左・右・上・下の種類」をキーとする索引に登録する。たとえば候補の左辺なら、同じ横座標を持つ既存の右辺の区間だけを調べる。全配置矩形を毎候補で総当たりする必要を減らせるが、同じ辺座標に多数の区間が集中すれば、その列を長く走査する。

索引はopen addressingの表と、区間を持つ連結リスト状のvectorで構成される。表が混むと容量を増やす。これは内部専用のデータ構造であり、外部のhash_mapヘッダーには依存しない。

### 9.5 基本順序と順序の摂動

基本順序は、長辺、高さ、周長に対応する幅+高さ、面積、幅の降順から作る。固定ビンで任意矩形や通常と異なるprofitがある場合には、profit密度とprofitによる順序も加える。同じ並びになった順序は重複して保持しない。

基本の並べ替えではrequiredを先にし、その順序の主キー、面積、入力IDで同点を解決する。高さ・幅のキーは入力時の寸法である。profit密度は浮動小数で割らず、profitと相手側の面積の積を`__int128_t`で比べる。

追加探索では基本順序を1つ選び、近い順位同士のswapまたはremove-insertを行う。大きさによる大まかな並びを残しながら別の詰まり方を試すためである。この摂動や動的順序は、requiredを必ず先に置くという硬い制約ではない。必須の優先は結果の比較規則で保証する。

### 9.6 Fail-first：置き場所が少ないものを先にする

先頭から一定個数の矩形について、今の空き領域に置ける候補の数を調べ、候補が少ない矩形を優先する。候補の数が同じならBSSFの配置キーで選ぶ。

候補数は、空き矩形と向きの組合せで数える。同じ座標の重複を除いた幾何学的な位置数ではない。BSSFで位置を走査する際に同時に数える。

固定ビンの救済構築では窓幅64、stripと外接の高さ圧縮では16を使う。窓の中に置ける矩形がなければ、残りを走査し最初に置けるものを試す。全体から毎回厳密に最少候補を探す方式ではない。

### 9.7 固定ビンの構築

各基本順序でBSSF・BAF・Bottom-Leftの3候補を作り、固定ビンの辞書式で比較する。通常の構築で必須を置き切れず、restart_limitが正なら、面積順を起点とするfail-firstも試す。

追加再始動ではBSSFとContact Pointを交互に使う。順序の摂動回数は1〜5で変える。全矩形が置けたときは残りの一部探索を省く。この早期終了は、すべて置けた後の細かな外接枠の同点改善を尽くすという意味ではない。

負のprofitを持つ任意矩形は新規の順序候補から除く。必須矩形のprofitは負でも必須として扱う。requiredの欠落数が第一目的であることは変わらない。

### 9.8 部分repairとrollback

可動IDの配置を外した基底解を作り、残った固定矩形を面積降順で空き領域へ登録する。各構築候補では、この固定領域の状態から可動矩形だけを置く。

MaxRectsでは基底の空き領域を各候補へ渡す。Contact Pointの固定辺索引は必要になったときに作り、候補ごとに追加した辺の変更をログへ記録する。候補を評価したら変更を逆順に戻し、固定索引を次候補にも使う。

ログの表添字を有効なまま使うため、候補を始める前に必要容量を確保し、ログ記録中にrehashしない。空き矩形のvectorは保存して復元するので、「すべての状態を差分だけで更新し、コピーがない」実装ではない。

repairでは初期解を最良候補として保護する。repackでは可動配置を外した基底解から候補を選び、元の初期解とは比較しない。repackも候補生成内部では標準の固定ビン目的で比較する点に注意する。

これらの再利用は1回の関数呼出の中だけで行われる。次ターンへの内部状態の持越し機能ではない。

### 9.9 Stripの高さ探索

まず十分な高さ上限でBottom-Left構築を行い、全配置の上界を作る。高さ上限には各矩形の幅+高さの合計を使う。

次に、全矩形の面積合計を幅で割った切上げと、各矩形が許された向きで必要とする最小高さの最大値から、使用高さの下界を作る。この下界と現在の高さの間で、少し低い目標高さを試す。

探索予算を$S$とすると、追加構築回数は$S-\lfloor S/4\rfloor$。前半は窓16のfail-first、後半はContact Pointを使う。全配置に成功し、現在解より良ければ採用する。目標高さは、前半ほど大きな改善を狙い、後半ほど小さな改善を狙う配分になる。

高さの実行可能性を厳密に判定しているわけではないため、通常の二分探索による最適高さの証明にはならない。最後に隙間詰めを行う。

### 9.10 複数ビンの構築とpair-merge

矩形を置くときは既存の全ビンを調べ、最良の配置位置を選ぶ。最初に入るビンへ置くfirst-fitではない。既存ビンに入る候補がなければ、新しいビンを追加する。

基本順序ではBSSFとBAFを使う。追加探索予算を$S$とすると、$\lceil5S/8\rceil$回の構築でBSSFとContact Pointを交互に使う。

全配置があり、予算が正で期限内なら、末尾ビンと他の1ビンを1つへまとめるpair-mergeを試す。2ビンの面積合計が1ビンの面積を超える組合せは除外する。残った組合せを固定ビンの部分問題として全再配置し、成功すればビン数を減らす。この内部再始動予算は$\min(8,S)$である。

統合は成功するたびに繰り返す。主ループの追加構築回数だけでは、pair-mergeの全処理時間は表せない。利用者が渡すrestart_limitでこの内部予算を直接指定するものでもない。

### 9.11 外接矩形の幅探索と高さ圧縮

面積合計付近の平方根を基準に幅の候補を作る。重み付き和では、幅と高さの重み比に応じて基準幅を調整する。矩形の辺長、取り得る幅の端も候補へ入れ、重複を除き、最大17点へ間引く。

長辺順と高さ順の2順序で、各幅についてBottom-Left構築を行う。次に現在の最良使用幅の−12%、−7%、−3%、+3%、+7%、+12%付近を試す。この段階も追加予算が0のときに実行される。

追加予算$S$のうち$\lceil5S/8\rceil$回を幅と順序のランダム探索へ使い、残りを最良使用幅に固定した高さ圧縮へ使う。高さ圧縮は前半fail-first、後半Contact Point。候補の指定幅ではなく、実際の使用幅・使用高さから目的を評価する。

最後に隙間詰めを行う。幅候補をすべての整数幅から列挙しているわけではないので、外接目的の最適性は保証しない。

### 9.12 完成配置の隙間詰め

strip・外接の候補を、現在の外接枠内で右、下、左、上の順に移動し、これを2巡行う。各方向で矩形をその方向の端から処理し、既に詰めた矩形と重ならない最も端の座標へ移す。

配置集合、各矩形の寸法・向き・所属ビンは変えない。右や下への移動も現在の枠内なので、使用幅・高さを増やさず、その後の左・上への移動で空白を減らせる。各方向の開始前に期限を確認し、途中終了でも有効な配置を保つ。

この処理は公開の任意配置編集APIではなく、対応するsolverの内部で行われる。複数ビンや固定ビンの通常結果に、同じ最終隙間詰めを自動適用するわけではない。

### 9.13 集計・validator・期限管理

集計はplacementsを1回走査し、配置済みの最大端、profit、面積、個数、必須欠落、最大ビン番号を数える。validatorはこの集計との一致と、同じビンに置かれた矩形対の交差を調べる。

期限管理では、設定された回数の停止確認ごとにsteady_clockを読む。一度期限切れと分かれば、そのcheckerでは期限切れを保持する。構築や隙間詰め等の処理単位ごとにcheckerが作られる箇所があるので、time_check_intervalを全solver横断の矩形操作カウンタだと解釈しない。

### 9.14 計算量の読み方

ここで$N$は矩形数、$F$は構築中に保持する空き矩形数の最大、$K$は実際の構築試行数、$B$はビン数、$L$は固定矩形数、$M_c$は可動矩形数を表す。$M_c$は1章の必須欠落数$M$とは異なる。

| 処理 | 時間の目安 | 注意 |
|---|---|---|
| 通常MaxRectsの1構築 | $O(NF^2)$ | 位置探索より空き領域の包含除去が重くなり得る |
| Contactの1構築 | 期待的には$O(NF^2)$、保守的には$O(N(F^2+NF))$ | 同じ辺座標に集中する区間数に依存する。ハッシュ処理を平均的に見た評価 |
| 固定ビン・strip・外接の構築部分 | 保守的に$O(KN(F^2+NF))$ | 幅や目標高さの試行もKに含める |
| 部分再配置の主処理 | 保守的に$O(LF^2+KM_c(F^2+NF))$ | Contactで調べる辺には固定矩形も含む。全入力の準備費用は別途必要 |
| 複数ビンの構築部分 | 期待的に$O(KN(BF+F^2))$ | 全ビンの候補を調べる。辺の集中でさらに重くなり得る |
| 複数ビンのpair-merge | 保守的に$O(B^2N(F^2+NF))$ | 内部再始動予算の定数上限を含む目安 |
| 最終隙間詰め | $O(N^2+N\log N)$ | 方向・巡回の数は定数。追加領域は$O(N)$ |
| 集計再計算 | $O(N)$ | 座標の修正はしない |
| validator | $O(N^2)$ | 同じビンの矩形対を検査する |

部分再配置は、可動集合だけを見れば済む処理ではない。基本順序の生成は概ね$O(N\log N)$、候補ごとの結果配列のコピー・集計は$O(N)$を要する。式は支配的な配置処理の見通しであり、これらをゼロとみなす意味ではない。Contactの期待的な式では、ハッシュ探索だけでなく同じ辺座標の区間数も平均的に小さいと見ている。

通常の1状態は空き矩形$O(F)$と配置・辺$O(N)$を保持する。複数ビンでは各ビンの空き状態を保持する。空き矩形数Fが配置に依存するので、「矩形数だけから1回何ms」とは決められない。最大サイズ、回転可否、密度、固定部分の形で実測することが重要である。
