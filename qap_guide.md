# qap_solver ユーザーガイド

対象: `qap_solver_v07.hpp`（v0.7.3）。このヘッダー1ファイルで利用できます。想定環境はGCC・C++20・単一スレッドです。

本書は現行ソースの仕様と使い方を説明します。4章・6章・8章は同じユースケース番号で対応します。まず4章で目的に近い用途を選び、5章で入力を準備し、6章と8章で呼出し方を確認してください。8章の関数は利用側でコピーする例であり、ヘッダーに追加されている公開APIではありません。

1. 何を解くライブラリか？
2. 厳密解アルゴリズム
3. 差分更新
4. ユースケース
5. 利用準備
6. ユースケースごとの使い方
7. 制約・注意点
8. ユースケースごとのコード例
9. 実装

## 1. 何を解くライブラリか？

### 1.1 物体を位置へ一対一に割り当てる

例えば、3台の機器を3か所の設置場所へ置くことを考えます。通信量の多い機器同士が遠いと費用が増え、機器によっては特定の設置場所に置く費用もかかります。「それぞれをどこへ置くか」を同時に決め、全体の費用を小さくするのがこのライブラリの対象です。

ここでいう「物体」は機器・人・施設・グラフの頂点などです。「位置」は設置場所・席・グリッドのセル・グループ内の席数に相当するスロットなどです。物理的な座標を持っている必要はありません。

物体数と位置数は同じです。各物体はちょうど1つの位置を使い、各位置にも物体がちょうど1つ入ります。求める出力は、この一対一対応と、その対応で生じる目的関数値です。

### 1.2 目的関数

配置を表す置換 $p$ に対し、次の値を最小化します。

$$C(p)=\sum_{i=0}^{n-1}U_{i,p_i}+\sum_{i=0}^{n-1}\sum_{j=0}^{n-1}F_{i,j}D_{p_i,p_j}$$

$$\{p_0,p_1,\ldots,p_{n-1}\}=\{0,1,\ldots,n-1\}$$

| 記号 | 意味 | 入力・出力との対応 |
| --- | --- | --- |
| $n$ | 物体数と位置数。どちらも同じ個数 | 入力の `n` |
| $i,j$ | 物体の番号。0以上、$n$ 未満 | 物体間行列の行と列 |
| $x,y$ | 位置の番号。0以上、$n$ 未満 | 位置間行列の行と列 |
| $p$ | すべての物体の配置をまとめた置換 | 出力の `location_of` |
| $p_i$ | 物体 $i$ が使う位置番号 | `location_of[i]` |
| $F_{i,j}$ | 物体 $i$ から物体 $j$ への関係の強さ、通信量、相互作用量 | 入力の `flow` |
| $D_{x,y}$ | 位置 $x$ から位置 $y$ への距離、移動単価、位置ペアの係数 | 入力の `distance` |
| $U_{i,x}$ | 物体 $i$ を位置 $x$ に置くことだけで決まる費用 | 任意入力の `unary`。省略時はすべて0 |
| $C(p)$ | 配置 $p$ に対する全費用 | 出力の `cost` |

第1項は、物体と位置の組だけで決まる費用を足します。例えば「機器0を位置2へ置くと移設費が10かかる」という費用です。

第2項では、まず物体の組を1つ選び、その関係量と、実際に置かれた位置同士の係数を掛けます。これをすべての物体の組について足します。機器間の通信量が5、配置先間の距離が3なら、その向きの費用は15です。

### 1.3 向き、対角成分、二重計上

二重和には $(i,j)$ と $(j,i)$ が別々に現れます。送信と受信で量が異なっても、そのまま表せます。また、$i=j$ も含むので、自己相互作用を使う場合は $F_{i,i}D_{p_i,p_i}$ が加算されます。不要な対角成分は0にします。

例えば配置が $p=(0,2,1)$、$F_{0,1}=5$、$F_{1,0}=2$、$D_{0,2}=3$、$D_{2,0}=4$ で、ほかの二次項が0とします。単項費用が $U_{0,0}=4$、$U_{1,2}=6$、$U_{2,1}=0$ なら、全費用は次の値です。

$$C(p)=4+6+0+5\cdot3+2\cdot4=33$$

無向辺を両方向へ同じ値で入れ、位置間距離も対称にすると、その辺の費用を2回数えます。単項項を加える場合は、二次項だけが2倍になると両者の重み付けまで変わる点に注意してください。「最終costを2で割れば常に同じ問題になる」というわけではありません。

### 1.4 表せる問題と、別の工夫が必要な問題

負の関係量、非対称な係数、非ゼロの対角成分も扱えます。`distance` は名前に反して、距離の三角不等式を満たす必要はありません。

ただし、二次費用は「物体ペア側の係数×位置ペア側の係数」という1組の積で表せる必要があります。物体 $i,j$ と位置 $x,y$ の4つの番号に任意に依存する費用、独立した複数種類の積の和、3物体以上の同時関係を、そのまま入力する機構はありません。式をこの形に変換できるか、本来の評価の一部分として利用できるかを確認します。

人数などの容量を「位置の個数」で表せる場合と、物体ごとに異なる重さが容量を消費する場合も区別してください。前者は4.3のように表現できますが、後者を一般に保証する容量制約APIはありません。

## 2. 厳密解アルゴリズム

### 2.1 全体が非常に小さければ全列挙する

物体数 $n$ の全置換は $n!$ 個です。すべてを列挙し、各置換を `evaluate` で評価すれば厳密最適解が得られます。単純な実装の時間計算量は $O(n!\,n^2)$ です。

階乗は急増します。8物体で40,320通り、10物体で3,628,800通りあり、各通りでさらに全評価が必要です。「10以下なら2秒に収まる」のような一律の保証はありません。制限時間内に列挙を完走できる大きさなら、近似探索に任せるより厳密列挙が明快です。

途中で列挙を止めた場合、そこまでで見つかった最良解は返せますが、最適性の保証はなくなります。内蔵の `solve_qap` が小さい問題を自動的に全列挙するわけではありません。

### 2.2 動かす物体が少数なら、その部分だけを厳密に解く

全体が大きくても、動かす物体を $k$ 個に固定し、その物体が現在使っている $k$ 個の位置だけで並べ替えるなら、候補は $k!$ 通りです。

`make_qap_subproblem` は、固定した外部物体との相互作用を単項費用へまとめた、小さいQAPを作ります。この変換で目的値の対応は失われません。その小さいQAPを全列挙し、`restore` で全体へ戻すと、「外部を固定した条件のもとでの厳密最適解」が得られます。全体問題の厳密最適解という意味ではありません。

8.16に関数形の実装を載せます。列挙部分は $O(k!\,k^2)$、その前に縮約構築の時間が必要です。例の `k <= 10` は誤って巨大な列挙を始めないための上限であり、実用上の推奨値ではありません。

### 2.3 二次項がなくなるなら線形割当を使う

例えば `flow` がすべて0なら、残るのは次の式です。

$$C(p)=\sum_{i=0}^{n-1}U_{i,p_i}$$

これは「各物体を各位置へ置く費用だけ」がある線形割当問題です。Hungarian法などで厳密解を $O(n^3)$ で求められるため、QAPの近似探索を使う必要性は小さくなります。[Hungarian法の実装と計算量](https://cp-algorithms.com/graph/hungarian-algorithm.html)

次の条件も、同じ判断ができます。以下は本書の目的式を直接整理して分かる条件です。

| 入力の条件 | 線形割当へ直せる理由 |
| --- | --- |
| `flow` または `distance` がすべて0 | 二次項が0 |
| `flow` の非対角成分がすべて0 | $F_{i,i}D_{x,x}$ を物体 $i$・位置 $x$ の単項費用へ加えればよい |
| `distance` の非対角成分がすべて0 | 一対一配置では異なる物体が同じ位置を使わないため、対角項しか残らない |
| `flow` の全要素が同じ定数 | 位置ペアの総和は置換で変わらず、二次項全体が定数 |
| `distance` の全要素が同じ定数 | 二次項は物体ペアの総和に比例し、置換で変わらない |

例えば対角項だけが残る場合は、線形割当用の費用を $U_{i,x}+F_{i,i}D_{x,x}$ とします。
これらの判定をして別の厳密solverへ自動切替する機能は、本ヘッダーにはありません。

### 2.4 純粋な小規模TSPなら部分集合DPを比較する

費用が「巡回順で隣り合う2都市間の移動費」だけなら、訪問済み集合と最後の都市を状態とする部分集合DPで厳密解を求められます。代表的なBellman–Held–Karp型DPの時間計算量は $O(n^2 2^n)$ です。[TSPの厳密DPに関する研究論文](https://people.csail.mit.edu/virgi/6.s078/papers/biptsp.pdf)

この状態数は一般QAPにはそのまま使えません。一般QAPでは、最後の都市だけでなく、既に配置した各物体がどの位置にあるかによって将来の費用が変わるためです。

対称である、グラフが疎である、値が非負である、という条件だけを理由に厳密解が容易になるとは限りません。厳密化できる構造がなければ、全体を近似的に解く用途や、小さい部分問題を繰り返し改善する用途を検討します。

## 3. 差分更新

### 3.1 再利用できるものを3つに分ける

ターンごとに入力が少し変わる問題では、「前の解」と「前処理」と「探索途中の状態」を区別すると使い方が明確になります。

| 再利用したいもの | 可否と方法 |
| --- | --- |
| 前回の配置 | 同じ物体・位置番号の完全置換なら、`improve_qap` に初期解として渡せる |
| 同じ入力行列に対する前処理 | `qap_prepared` を保持し、`solve`・`improve`・`swap_delta` で再利用できる |
| 自分の探索で動かしている配置とcost | `qap_swap_state` を保持し、`apply_swap` で同期して更新できる |
| 内蔵探索のtabu履歴・乱数の現在位置・差分表 | 公開APIでは取り出せず、別の `improve` 呼出しへ持ち越せない |

したがって、同じ入力に対する短い反復呼出しでは前処理を省けますが、内蔵探索を停止した場所から完全に再開する機能ではありません。

### 3.2 何を変更したら何を作り直すか

| 変更内容 | 再利用できるもの | 再構築・再計算するもの |
| --- | --- | --- |
| 問題は不変で、配置だけをswapする | `problem` と `prepared` | `state.apply_swap` が現在配置とcostを更新する |
| 問題は不変で、別の完全初期解へ切り替える | `prepared` | `prepared.improve(initial, ...)`、または新しい `state` を作る |
| `flow` の重み変更・辺追加・辺削除 | 番号体系が同じなら前回配置 | `prepared` と `state` を作り直し、新しい目的値を計算する |
| `distance` の値変更 | 同上 | 対称性判定を含む `prepared`、および `state` を作り直す |
| `unary` の変更だけ | 同上 | 契約上 `prepared` を作り直す。古いstateのcostも使わない |
| 物体数・位置数・番号の対応を変更 | 必要なら配置を利用側で写像し直す | 行列と完全置換を新しい大きさで構築する |
| 可動集合だけを変更 | 元問題と現在の完全配置 | 部分QAPを新たに作る。元問題用preparedを縮約問題に流用しない |
| 元問題を変えた後、以前の部分QAPを使う | 以前の部分QAPは以前の入力のコピーとしてのみ有効 | 変更後の問題へ使う部分QAPは再抽出する |

`qap_problem` の行列は公開vectorなので、準備段階では要素を直接代入できます。しかし、`qap_prepared` の参照先は使用中ずっと不変という契約です。「unaryならCSRに入っていないから大丈夫」と推測して更新する使い方はしません。

辺の変更を受け取ってCSRだけを部分修正する `add_edge`・`update_weight` のようなメソッドはありません。入力行列の更新量が1要素でも、preparedの構築は $O(n^2)$ です。

### 3.3 入力変更後に前回解から再探索する手順

同じ物体数・同じ番号体系のターンなら、次の順序にします。

1. 前回の完全配置を保存する。
2. 古いpreparedとstateの利用を終える。保持したまま参照先を変更しない。
3. `flow`・`distance`・`unary` を今回の入力へ更新する。
4. 前回配置を初期解として `improve_qap` を呼ぶ。同じ新入力で何度も呼ぶなら、新しいpreparedを1回作る。
5. 返された配置と、新入力に対するcostを次の状態として保存する。

ここで保証される非悪化は「新しい問題で前回配置を評価した値」に対してです。前ターンのcostとは目的関数自体が変わっているので、直接比較しません。古いcostを既知cost版へ渡すのも誤りです。

8.19は、変更する行列要素を受け取り、前回配置から再探索する完結した関数です。入力問題を値渡しでコピーして更新するため、呼出し元の問題を借りているpreparedを壊しません。一方、そのコピーだけでも $O(n^2)$ の時間・メモリが必要です。

### 3.4 局所的な変更に対する部分repair

入力更新後に影響の大きい物体を数個選べる場合は、`improve_qap_subset` でその物体だけを動かせます。ただし、それらが使える位置は「現在、その物体群が占有している位置集合」に限定されます。

例えば物体1と4が位置2と7にいる場合、この2物体は位置2と7の間で入れ替われます。位置9へ移したいなら、位置9を占める物体も可動集合に含めます。空き位置を表すダミー物体であっても同じです。

外部固定物体との相互作用は縮約費用へ正確に含まれます。しかし、可動集合をどう選ぶと良くなるかは入力更新の内容によります。ライブラリが影響範囲を自動検出するわけではなく、選んだ集合の外に改善余地が残ることもあります。

既知cost版の縮約構築は $O(n+k^2n)$、costを持っていない版はさらに全評価 $O(n^2)$ が加わります。入力が変わった直後のcostは新しい問題で計算してください。

### 3.5 問題は不変で、配置だけを更新する場合

この場合が最も直接的な差分利用です。`qap_prepared` を1つ作り、それを参照する `qap_swap_state` に現在配置を持たせます。

- `swap_delta(i, j)` は「交換後cost − 交換前cost」を返すだけで、配置は変えません。
- `apply_swap(i, j)` は交換を実行し、配置とcostを更新して、その差分を返します。改善手かどうかは判定せず、渡した交換を実行します。
- 元問題が変わらなければ、stateをターン間で保持できます。任意の置換へまとめて差し替えるset/resetメソッドはないので、その場合は新しいstateを作ります。

stateは最良解を別に保持しません。悪化手も受理する独自探索を書くなら、最良配置の保存も利用側で行います。8.17の例は追加目的値との合計が改善する交換だけを受理します。

## 4. ユースケース

用途1〜10は入力のモデル化、11は数値型、12〜19は組込み・反復方法、20はグラフ比較です。同じ問題で組み合わせることもできますが、各節は入力の作り方や返却値の意味が異なる用途を説明します。

### 4.1 対称な施設・機器配置

工場の設備、サーバー、教室内の機器などを、決まった設置場所へ1つずつ置きます。頻繁にやり取りする物体同士が近くなるほど、通信・運搬費を小さくできる状況です。

必要なのは物体の個数、向きを区別しない関係の強さ、同数の設置場所の座標です。例では横移動と縦移動の長さを足した距離を使います。独自の位置適性を加える用途は6、方向別に費用が違う用途は5を参照してください。

### 4.2 疎グラフのグリッド配置

通信する部品や、近くに置きたい頂点を長方形のマスへ配置します。例えば、基板の少数の接続線を短くしたい場合です。すべての頂点ペアが接続されている必要はありません。

頂点数、グリッドの高さと幅、辺の重みを指定します。マスが余る場合は、どのマスを空けるかも同時に決まります。障害物を避けた経路距離や使用禁止マスは、この長方形全体を使う例に自動で反映されるものではありません。

### 4.3 固定人数のグループ分け

各チームに同じ人数を入れながら、関係の強い人同士が別チームになる損失を小さくします。固定人数のクラスタリングや、収容数が同じゾーンへの配置も同じ形です。

グループ数、各グループの人数、物体ペアの関係を指定します。特定の人を特定のグループへ入れる個別費用は任意です。人数は各物体が1席を使うことで保証します。重量など、物体ごとに異なる容量消費は表していません。

### 4.4 巡回順序・円環配置

工程の巡回順、円卓の座順、巡回訪問する都市の順番を決めます。「物体iの直後に物体jを置く費用」の合計を小さくする用途です。最後から先頭へ戻る費用も含めます。

必要なのは物体数と、すべての向き付き物体ペアの接続費用です。順序に依存しない一般の設備距離を指定する用途とは、位置の意味が異なります。純粋な小規模TSPの厳密解を求めたい場合は2.4も参照してください。

### 4.5 有向・非対称の物流や通信配置

送信と受信の量が違う通信や、行きと帰りで移動単価が違う物流を配置します。例えば上り方向だけ輸送負担が大きいとき、行きと帰りを平均してしまうと別の問題になります。

物体間の向き付き流量と、位置間の向き付き費用を指定します。逆向きのデータは独立です。自己相互作用も必要なら指定でき、不要なら0にします。

### 4.6 位置適性と移設費を含む再配置

今ある配置を改善したい一方、移動するほど費用がかかる場合です。機器の再設置、人の席替え、毎ターンの配置変更などで、配置の良さと移動負担を同じ目的値に含めます。

物体間の関係と配置先間の二次係数に加え、現在位置、移動距離、移動単価を指定します。物体×位置の基礎費用は任意です。配置先同士の関係距離と、実際に移設する距離は、意味が違えば別々に与えます。

### 4.7 親和度・配置報酬の最大化

仲の良い人を近い席に置く、関係の強い機能を隣接させるなど、費用を減らすより報酬を増やす言い方が自然な用途です。

物体間の親和度と、位置ペアの近接度・報酬係数を指定します。位置ペアの係数は隣接なら1、それ以外0でも構いません。特定の物体を特定の位置へ置く追加報酬は任意です。

### 4.8 位置数が多い一般配置と空き位置の選択

機器数より設置候補が多く、どの場所を空けるかも選ぶ用途です。例えば10台の機器を15か所の候補へ重複なく配置します。グリッドに限らず、任意の位置間費用を指定できます。

実物体数、位置数、実物体間の関係、全位置間の係数を指定します。実物体×位置の単項費用は任意です。空き場所は何も相互作用を発生させないという前提で、空きを表す仮想物体を補います。

### 4.9 候補の選択と配置を同時に決める

置きたい候補の数が場所より多いとき、どれを採用し、どこへ置くかを同時に選びます。例えば20候補から12個を選んで12か所へ配置する状況です。

候補間の関係、実際の場所間の係数、採用時の場所別費用、不採用費用を指定します。この例は実際の場所をすべて埋めます。不採用の候補はほかの候補とのペア相互作用から外れるという前提です。

### 4.10 配置の希望・禁止を罰則で扱う

できれば特定の席へ置きたい、特定の組合せを避けたいが、必要なら違反も許す用途です。希望しない配置に追加費用を付け、ほかの費用との交換条件として扱います。

基礎となる配置問題、各物体の希望位置、避けたい物体×位置の組、違反1件の罰則を指定します。罰則が大きくても、絶対に違反しないという保証にはなりません。動かしてはいけない物体の厳密な固定には用途14を使います。

### 4.11 大きな整数目的値を持つ配置

通信量や距離は64bit整数に収まっても、それらの積や全体の和が64bitを超える用途です。大きな金額、拡大した整数座標、精度維持のために倍率を掛けた費用などが該当します。

入力行列に加えて、計算全体が収まる目的値の型を選ぶ必要があります。この例では128bitの符号付き整数で計算し、結果を10進文字列へ変換できます。大きい型にしただけで全中間値が必ず安全になるわけではありません。

### 4.12 既存の完全配置を改善する

貪欲法、別のsolver、人が作った配置など、すでに使える完全な解がある用途です。その配置を出発点に、同じ目的関数のもとで改善します。

構築済みの問題と、すべての物体の位置が重複なく決まった配置を指定します。途中までしか決まっていない配置は渡せません。前回配置を使う場合でも、入力が変わったときの扱いは用途19の手順に従います。

### 4.13 同じ問題に対する複数の初期解を比較する

同じ問題について、性質の違う複数の初期配置を持っている用途です。どの初期解から改善すると良いか分からないとき、持ち時間を分けて試します。

共通の問題、完全初期配置の一覧、全体の締切を指定します。入力問題が変わらないので前処理を共有できます。ただし時間を分ければ各初期解を改善する時間は短くなり、1回の長い探索より良いとは限りません。

### 4.14 固定物体を保った部分repair

配置全体を崩したくないとき、選んだ物体だけを動かして改善します。例えば重要な設備は固定し、周辺の数台だけを入れ替える状況です。

現在の完全配置、正しい現在費用、動かしてよい物体番号を指定します。可動物体の選び方は利用側で決めます。候補位置は、その可動物体群が現在使う位置集合に限定されます。

### 4.15 可動集合を変えながら部分repairを反復する

外側の処理が毎回異なる部分集合を選び、局所的な改善を積み重ねる用途です。混雑している領域、関係の強い集団、前回の変更箇所の周囲などを順番に見直せます。

初期配置と費用、反復上限、可動集合を選ぶ規則、全体締切を指定します。どの領域が重要かという問題固有の知識を、汎用の部分問題solverと組み合わせる使い方です。

### 4.16 小部分を厳密列挙でrepairする

選ぶ可動物体がごく少数で、その範囲については最適な並べ方を確実に選びたい用途です。全体の大きさより、動かす個数が小さいことが重要です。

現在の完全配置、正しい費用、少数の可動物体を指定します。外側の配置は固定され、その条件のもとでの最適解を求めます。全体の最適性を証明するものではありません。

### 4.17 QAP差分を独自の局所探索へ組み込む

QAPで表せる費用に加え、別の状態や追加費用も動かす必要がある用途です。例えば配置swapに伴って、問題固有の混雑や局所違反の費用も更新します。

初期配置、追加費用の現在値、swapによる追加費用差を計算する処理、追加状態を更新する処理を指定します。QAP側の配置と費用の管理をライブラリへ任せ、手の採否は利用側で決めます。

### 4.18 QAPを代理評価として使い、本来スコアで採否する

本当の評価が複雑で、その一部や近似をQAPとして解く用途です。QAPで良くなった候補が、シミュレーション後の本来スコアでも良いとは限りません。

現在配置、QAP費用、本来スコア、可動集合、本来スコアを計算する処理を指定します。候補を作った後に1回だけ本評価し、良くなったときだけ配置を入れ替えます。

### 4.19 入力行列が少し変わるターン制問題

通信量や移動費がターンごとに変わり、前の配置を基に次の配置を決める用途です。問題は変わりますが、物体と位置の番号の意味は変わらない状況を扱います。

前ターンの問題、前回配置、書き換える行列要素と新しい値を指定します。変更を適用した問題で前回配置を評価し直してから改善します。前回の数値スコアをそのまま次の問題へ持ち込まない点が重要です。

### 4.20 2つの重み付きグラフの対応付け

頂点番号の異なる2つのグラフを、関係構造ができるだけ似るように対応付けます。例えば同じ個数の部品を持つ2つの配置やネットワークを比較し、どの頂点同士が対応しそうかを求めます。

同じ頂点数の2行列を指定し、対応後の辺重みの差の二乗和を小さくします。頂点属性による対応費用も任意に追加できます。有向・重み付きでも扱えますが、グラフ同型を判定し証明する専用機能ではありません。

## 5. 利用準備

### 5.1 導入と型

`qap_solver_v07.hpp` をプログラムと同じディレクトリへ置き、次をincludeします。

```cpp
#include "qap_solver_v07.hpp"
```

標準ライブラリ以外の追加ヘッダーは不要です。実装は `bits/stdc++.h`、`__uint128_t`、`__INCLUDE_LEVEL__` などのGCC環境を使います。C++20で利用し、本書の検証環境はGCC 13.3.0です。GCC 12.2での今回の実行確認はしていません。

`qap_problem`・`qap_prepared`・`qap_swap_state`・`qap_subproblem` は共通の型引数を使います。

| 型引数 | 既定値 | 意味と選び方 |
| --- | --- | --- |
| `Weight` | `int` | `flow` と `distance` の両方の要素型。入力1要素と、その入力を作るときの加算が収まる型を選ぶ |
| `Score` | `long long` | `unary`、目的値、swap差分、差分表の計算型。負の差分を表せる符号付き整数を使う |

`qap_result<Score>` の型引数は `Score` だけで、既定値は `long long` です。型を広くする例は8.11です。

### 5.2 qap_problemの構築方法

利用可能なコンストラクタは次の3つです。これはAPIの宣言一覧であり、自分のコードに再宣言する必要はありません。

```cpp
qap_problem();
explicit qap_problem(int n_);
qap_problem(int n_,
            std::vector<Weight> flow_,
            std::vector<Weight> distance_,
            std::vector<Score> unary_ = {});
```

| 方法 | 構築直後 | 適した用途 |
| --- | --- | --- |
| `qap_problem<> problem;` | `n=0`、3行列は空 | 空問題、後で完成済みの問題を代入する |
| `qap_problem<> problem(n);` | `flow`・`distance` は各 `n*n` 要素の0、`unary` は空 | 辺・座標・グループから行列を作る |
| `qap_problem<> problem(n, flow, distance, unary);` | 渡した3行列を保持する | 既に行列を持っている |

行列受取りコンストラクタの引数は値渡しです。lvalueのvectorを渡すと引数へコピーされ、`std::move` で渡すと所有権を移せます。コンストラクタ内のmoveだけを見て、lvalueでも常に $O(1)$ と考えないでください。コピーを含めると通常は $O(n^2)$ です。

公開メンバーは次のとおりです。solverを呼ぶ前に、すべてのサイズを揃えます。

| メンバー | 型 | 既定値・必須条件 | 格納する内容 |
| --- | --- | --- | --- |
| `n` | `int` | 既定構築では0。0以上 | 物体数＝位置数 |
| `flow` | `vector<Weight>` | `n*n` 要素が必須 | `flow[i*n+j]` は物体i→jの係数 |
| `distance` | `vector<Weight>` | `n*n` 要素が必須 | `distance[x*n+y]` は位置x→yの係数 |
| `unary` | `vector<Score>` | 空、または `n*n` 要素 | `unary[i*n+x]` は物体iを位置xへ置く費用 |

行優先なので、行番号に列数を掛けて列番号を足します。`flow` の添字は物体、`distance` の添字は位置、`unary` の行は物体・列は位置です。

```cpp
// 例: 3物体を3位置へ置く入力の準備。
qap_problem<int, long long> problem(3);
problem.flow[0 * 3 + 1] = 10;      // 物体0→物体1の関係量
problem.distance[0 * 3 + 2] = 4;   // 位置0→位置2の係数
problem.unary.assign(3 * 3, 0);    // 1要素でも使うなら全行列を確保
problem.unary[1 * 3 + 2] = 7;      // 物体1を位置2へ置く費用
```

未設定の要素は上の例では0です。必要な距離や辺を未設定のままにすると「費用0」という別の問題になります。距離行列をライブラリが座標から自動計算することも、逆向きの値を自動補完することもありません。

### 5.3 初期配置と返却値

初期解を持っている場合は `vector<int>` に物体→位置の向きで格納します。

```cpp
// 物体0は位置2、物体1は位置0、物体2は位置1。
std::vector<int> initial{2, 0, 1};
assert(problem.is_valid_permutation(initial));
const long long initial_cost = problem.evaluate(initial);
```

`is_valid_permutation` は長さ・範囲・重複を検査して `bool` を返します。`evaluate` は渡した配置のcostを全再計算します。これらは準備にも検証にも使える公開メソッドです。

返却値 `qap_result<Score>` は、`vector<int> location_of` と `Score cost` を持ちます。`cost` の初期値は0ですが、`qap_result<> result;` と宣言するだけで非空問題の解になるわけではありません。

### 5.4 deadlineとseed

| 引数 | 型 | 既定値 | 設定方法 |
| --- | --- | --- | --- |
| `deadline` | `std::chrono::steady_clock::time_point` | なし。必須 | 全体の制限より余裕を持った絶対時刻 |
| `seed` | `std::uint64_t` | ライブラリの探索APIでは1 | 比較実験は固定値、複数試行は異なる値も試す |

ミリ秒数や `system_clock` の時刻は直接渡せません。例えば、入力の構築も含めて約1.9秒を目安に使うなら、構築より前に締切を作ります。

```cpp
using Clock = std::chrono::steady_clock;
const auto deadline = Clock::now() + std::chrono::milliseconds(1900);
// この後に問題を構築し、同じdeadlineをsolve_qap等へ渡す。
const std::uint64_t seed = 1;
```

1900msは使用例であり、環境に関係なく2秒以内に終わる保証ではありません。I/O、問題の変換、全評価、本来スコアの再評価、返却・出力に必要な時間を見込んで短くします。内側ループのたびに `now()+1900ms` と作り直すと、全体の時間制限を守れません。

seedは0も指定できますが、自動的にランダムseedを生成する意味ではありません。同じseedでも、時計による打切り位置が違えば解が変わります。比較には複数の入力とseedを使い、コンパイラ、最適化、実行環境、時間予算を揃えます。

### 5.5 前処理とswap状態のコンストラクタ

```cpp
explicit qap_prepared(const qap_problem<Weight, Score>& problem);

qap_swap_state(const qap_prepared<Weight, Score>& prepared,
               std::vector<int> location_of);
qap_swap_state(const qap_prepared<Weight, Score>& prepared,
               std::vector<int> location_of,
               Score known_cost);
```

| 構築時引数 | 意味 | 省略・既定値 |
| --- | --- | --- |
| `prepared` の `problem` | 完成済みの不変な問題。参照先として借用する | 必須 |
| `state` の `prepared` | その配置と同じ問題の前処理。参照先として借用する | 必須 |
| `state` の `location_of` | 現在の完全置換。値渡しでstateへ所有させる | 必須 |
| `state` の `known_cost` | 同じ問題・同じ置換の正しい目的値 | 省略すると全評価する。既定の数値があるわけではない |

```cpp
// problemとinitialを完成させてから構築する。
qap_prepared<int, long long> prepared(problem);
qap_swap_state<int, long long> state(prepared, initial);
// 既に正しいcostがあれば次の構築でもよい。
qap_swap_state<int, long long> known_state(
    prepared, initial, problem.evaluate(initial));
```

最後の行のようにその場で `evaluate` しても、計算を省いたことにはなりません。既知cost版の利点は、外側で既に持っている正しいcostを渡せることです。

problemの寿命はpreparedより長く、preparedの寿命はstateより長くします。どちらも参照先を所有しません。default構築して後で問題を設定するメソッドはありません。preparedをコピーしても同じproblemを借り、stateをコピーしても同じpreparedを借ります。

### 5.6 部分問題を作る準備

必要なのは完成済みの `problem`、完全配置 `full_initial`、重複のない物体番号列 `movable_objects` です。可動集合を作るために位置番号を渡してはいけません。

```cpp
// 例: 物体0と2だけを動かす。現在の位置集合はfull_initialから決まる。
std::vector<int> movable_objects{0, 2};
auto sub = make_qap_subproblem(problem, initial, movable_objects);
// costが既知なら第4引数へ渡して元問題の全評価を省略できる。
auto known_sub = make_qap_subproblem(
    problem, initial, movable_objects, initial_cost);
```

`qap_subproblem` の公開メンバーは次の内容を持ちます。通常はfactory関数で作ったまま使います。

| メンバー | 型 | 意味 |
| --- | --- | --- |
| `problem` | `qap_problem<Weight,Score>` | 可動数kの縮約QAP。行列を所有 |
| `base_location_of` | `vector<int>` | 元の完全配置のコピー |
| `object` | `vector<int>` | 縮約物体番号a→元物体番号 |
| `location` | `vector<int>` | 縮約位置番号x→元位置番号 |
| `initial_location_of` | `vector<int>` | 縮約初期解。factoryでは0..k-1の恒等置換 |
| `constant_offset` | `Score` | 縮約costへ足すと完全costになる定数。既定構築時は0 |

それぞれの配列や定数は連動しています。publicだからといって任意に変更すると、元問題との対応が壊れます。縮約問題の入力自体を変更して解くなら、それを元の完全QAPへ正しく復元できるかは利用側で再設計する必要があります。

### 5.7 調整できる値と、内部で決まる値

公開探索APIで調整するのは `deadline` と `seed` です。温度、tabu期間、疎密判定の閾値、探索モードを渡す設定クラスはありません。内部の値と役割は9章で説明します。

8章のadapterには、用途を表現するための値や外側の反復設定があります。これらは内蔵solverの隠れた設定ではありません。

| 値 | 種類 | 選び方 |
| --- | --- | --- |
| `move_weight` | 目的関数の移動単価 | 移設を1単位減らす価値を、二次費用と同じ単位へ換算する |
| `penalty` | soft制約の目的費用 | 違反1件と、ほかの費用改善をどこまで交換可能とするかで決める |
| `capacity` | 問題の容量 | 現実の人数制約を指定する。探索品質のために勝手に変更しない |
| 可動数 `k` | 外側の部分集合選択 | 小さいほど構築・探索が軽く、大きいほど動かせる範囲が広い。実際の入力で測る |
| `rounds` | 外側の反復上限 | 構築費を含む1回の時間を測り、極端に短いrepairを大量に作らない |
| 初期候補の個数 | 外側の探索配分 | 有望な初期解に絞り、1候補あたりの時間も確保する |

ユースケースごとの準備の差は6章にまとめます。施設・グリッド・グループ・巡回順は行列への変換、最大化は符号、物体数と位置数が違う場合はダミー、反復利用は寿命と前処理の共有が主な違いです。

## 6. ユースケースごとの使い方

### 6.0 共通する公開API

以下の `W` は問題の `Weight`、`S` は `Score` です。`p`・`initial` は物体→位置の完全置換、`movable` は可動物体番号のvectorです。`deadline` と `seed` は5.4の型で、探索APIのseedは省略すると1です。

| 呼出し | 戻り値 | 意味 |
| --- | --- | --- |
| `solve_qap(problem, deadline, seed=1)` | `qap_result<S>` | 初期配置を内部生成して探索 |
| `improve_qap(problem, initial, deadline, seed=1)` | `qap_result<S>` | 指定の完全配置から非悪化の改善 |
| `prepared.solve(deadline, seed=1)` | `qap_result<S>` | 前処理を再利用して初期生成・探索 |
| `prepared.improve(initial, deadline, seed=1)` | `qap_result<S>` | 前処理を再利用して既存解改善。開始costは全評価 |
| `improve_qap_subset(problem, initial, movable, deadline, seed=1)` | `qap_result<S>` | 開始costを全評価し、可動物体だけ改善 |
| `improve_qap_subset(problem, initial, movable, known_cost, deadline, seed=1)` | `qap_result<S>` | 正しい既知costを使って部分改善 |
| `make_qap_subproblem(problem, initial, movable)` | `qap_subproblem<W,S>` | 開始costを全評価して縮約 |
| `make_qap_subproblem(problem, initial, movable, known_cost)` | `qap_subproblem<W,S>` | 既知costから縮約 |
| `sub.restore(reduced_result)` | `qap_result<S>` | 縮約配置を完全配置へ戻し、costへ定数項を加える |

`known_cost` の型は `S` と揃えます。例えば `Score=long long` の問題へ整数リテラルを渡すなら `0` ではなく `0LL` とする必要がある場合があります。関数テンプレートの型推論で、問題から得た型とcost引数の型が矛盾するためです。もちろん、0を使ってよいのは実際のcostが0の場合だけです。

配置と差分を直接扱うメソッドは次のとおりです。

| 呼出し | 戻り値 | 変更する状態 |
| --- | --- | --- |
| `problem.is_valid_permutation(p)` | `bool` | なし |
| `problem.evaluate(p)` | `S` | なし。全評価する |
| `problem.swap_delta(p, i, j)` | `S` | なし。一般式で交換後−交換前を返す |
| `prepared.problem()` | `const qap_problem<W,S>&` | なし。借りている問題を参照する |
| `prepared.evaluate(p)` | `S` | なし。全評価する |
| `prepared.swap_delta(p, i, j)` | `S` | なし。前処理を使って交換差分を返す |
| `state.location_of()` | `const vector<int>&` | なし。現在配置を参照する |
| `state.cost()` | `S` | なし。保持中の現在costを返す |
| `state.swap_delta(i, j)` | `S` | なし |
| `state.apply_swap(i, j)` | `S` | 現在配置とcostを変更し、適用した差分を返す |

`restore` は縮約costを再評価しません。`reduced_result.cost` は、その縮約配置を `sub.problem` で評価した正しい値でなければなりません。

#### 締切・退化ケースの扱い

| 条件 | 実際の動作 |
| --- | --- |
| `solve_qap` 呼出し時点で締切到達 | 初期配置の構築・評価は実行する。締切だけで定数時間にはならない |
| `improve_qap`、`prepared.improve` 呼出し時点で締切到達 | 開始costを全評価して開始配置を返す |
| 既知cost版subsetで `k<2` または締切到達 | 完全配置をコピーして、渡したcostと共に返す |
| cost省略版subsetで `k<2` または締切到達 | 先に完全costを全評価し、その後返す |
| 既知cost版subsetで `k==n`、かつ早期返却しない場合 | 通常improveへ委譲し、開始costを全評価する |
| subsetの縮約構築中に締切到達 | その構築自体を途中停止する保証はない |

以下の6.1〜6.20では、4章と同じ番号ごとに入力・準備・返却値を説明します。短い呼出し例の入力変数は各表のとおり用意してください。呼び出している関数は8章に全文がある利用側のadapterで、必要なコンストラクタ呼出しと行列設定もその中に含まれています。8章の対応ブロックをコピーするか、配布の `qap_solver_usecase_examples.hpp` を使います。

### 6.1 対称な施設・機器配置

**準備と呼出し経路。** `qap_problem<int,long long>`(n)を作り、各辺をflowの両方向へ加算します。全位置ペアのマンハッタン距離をdistanceへ設定し、unaryは空のままsolve_qapを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `n` | 物体数＝位置数。0以上 |
| `edges` | 無向辺の列。各要素のu,vは物体番号、weightは関係の強さ。各辺は1回だけ渡す。重複辺は加算 |
| `location_xy` | 位置番号順の整数座標(x,y)をn個 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_symmetric_facility_layout(
    n,
    edges,
    location_xy,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。location_of[i]が機器iの位置番号、costが向き付き二重和です。位置座標はlocation_xy[location_of[i]]で取得します。

**制限・注意。** 単項項を使わないこの例では、costは無向辺を1回ずつ数えた費用の2倍です。座標の差・距離・辺重みの加算はint、目的値の全中間値はlong longに収まる必要があります。

### 6.2 疎グラフのグリッド配置

**準備と呼出し経路。** n=height*widthの`qap_problem`を作り、実頂点のflowだけ設定します。残りの物体は0コストのダミーです。セル番号y*width+xの全ペアにマンハッタン距離を設定してsolve_qapを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `vertex_count` | 実頂点数。0以上、height*width以下 |
| `height` | グリッドの行数 |
| `width` | グリッドの列数。通常はheight,widthとも正 |
| `edges` | 実頂点間の無向辺(u,v,weight)。各辺1回、重複は加算 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_sparse_graph_on_grid(
    vertex_count,
    height,
    width,
    edges,
    deadline,
    seed);
```

**戻り値と利用。** `qap_grid_layout_result`。cell_of_vertex[v]がセル番号、empty_cellsが昇順の空きセル一覧、costが費用です。行はcell/width、列はcell%widthです。

**制限・注意。** costは無向辺の重み付き距離和の2倍です。実頂点の返却値は0..vertex_count-1の置換ではなく、全セルを範囲とする重複なしの割当です。疎な辺でも入力行列のメモリはO(n^2)です。

### 6.3 固定人数のグループ分け

**準備と呼出し経路。** 各グループにcapacity個の位置を用意します。`qap_problem`(n)のflowへaffinity、distanceへ同グループ0・別グループ1を設定します。group_costを使うときは各グループの全スロットへunaryを展開し、solve_qapを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `group_count` | グループ数。0以上 |
| `capacity` | 各グループの正の人数。n=group_count*capacity |
| `affinity` | n*n要素。affinity[i*n+j]は別グループに分けたときの関係費用 |
| `group_cost` | 空またはn*group_count要素。group_cost[i*group_count+g]は物体iをグループgへ入れる単項費用 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_fixed_capacity_partition(
    group_count,
    capacity,
    affinity,
    group_cost,
    deadline,
    seed);
```

**戻り値と利用。** `qap_fixed_partition_result`。group_of_object[i]がグループ番号、costがペア分割費用と単項費用の合計です。

**制限・注意。** 関係が正なら同じグループへ集めたい意味、負なら離したい意味になります。対称入力のペア費用は2回数えます。同じグループ内の位置番号に意味はありません。不均一な容量には位置→グループ配列を持つ別のadapterが必要です。

### 6.4 巡回順序・円環配置

**準備と呼出し経路。** `qap_problem`(n)のflowへdirected_costを設定します。distanceは位置xから(x+1)%nへだけ1、それ以外0としてsolve_qapを呼びます。返却された物体→順序の逆写像も作ります。

| 引数 | 設定する内容 |
| --- | --- |
| `n` | 物体数。例では2以上 |
| `directed_cost` | n*n要素。directed_cost[i*n+j]はiの次をjにする費用 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_cyclic_order_as_qap(
    n,
    directed_cost,
    deadline,
    seed);
```

**戻り値と利用。** `qap_cycle_result`。order_of_object[i]が巡回順の番号、object_at_order[t]がt番目の物体です。costは最後から先頭も含む巡回費用です。

**制限・注意。** この変換は有向巡回にも対応し、巡回辺を1回ずつ数えます。純粋なTSP専用近傍と同じ探索性能を意味しません。円環の回転対称性を除くには、固定を満たす初期置換を作って固定物体を可動集合から外せます。

### 6.5 有向・非対称の物流や通信配置

**準備と呼出し経路。** `qap_problem`(n)のdistanceへ位置間費用を設定し、arcsに書かれた向きだけflowを加算します。unaryは空のままsolve_qapを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `n` | 物体数＝位置数 |
| `arcs` | 有向辺(from,to,amount)。amountはその向きの流量。重複は加算 |
| `directed_location_cost` | n*n要素。位置xからyへの費用を[x*n+y]に入れる |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_directed_transport_layout(
    n,
    arcs,
    directed_location_cost,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。location_of[i]が物体iの位置、costが方向を維持した流量×位置間費用の総和です。

**制限・注意。** 逆向きの辺を自動補完しません。from==toも入力できます。対角項を不要とするなら、入力時に0にしてください。

### 6.6 位置適性と移設費を含む再配置

**準備と呼出し経路。** `qap_problem`(n)のflowとdistanceを設定し、unary[i*n+x]を基礎費用＋move_weight*relocation_distance[old_location_of[i]*n+x]にします。old_location_ofからimprove_qapを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `n` | 物体数＝位置数 |
| `flow` | n*nの物体間行列 |
| `pair_distance` | n*nの二次費用用位置間行列 |
| `base_assignment_cost` | 空またはn*nの基礎単項費用 |
| `old_location_of` | 物体→現在位置の完全置換 |
| `relocation_distance` | n*n。旧位置xから新位置yへ移す非負距離 |
| `move_weight` | 非負の移動単価。距離と掛けて単項費用へ加える |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_qap_reconfiguration(
    n,
    flow,
    pair_distance,
    base_assignment_cost,
    old_location_of,
    relocation_distance,
    move_weight,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。costには移設費も含みます。移設費を含む新しい目的関数で開始配置以下の値を返します。

**制限・注意。** ターンが進んで移動の起点が変わったらunaryを作り直します。古いpreparedや古いcostを使い回しません。move_weight=0は移設費を無視する意味です。

### 6.7 親和度・配置報酬の最大化

**準備と呼出し経路。** affinityだけを符号反転してflowへ入れ、proximityをdistance、符号反転したassignment_rewardをunaryとする`qap_problem<long long,long long>`を作ります。solve_qap後にcostの符号を戻します。

| 引数 | 設定する内容 |
| --- | --- |
| `n` | 物体数＝位置数 |
| `affinity` | long longのn*n要素。物体ペアの親和度 |
| `proximity` | long longのn*n要素。位置ペアの近接度・報酬係数 |
| `assignment_reward` | 空またはn*n要素。物体×位置の報酬 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = maximize_qap_affinity(
    n,
    affinity,
    proximity,
    assignment_reward,
    deadline,
    seed);
```

**戻り値と利用。** `qap_maximization_result`。location_ofが配置、scoreが元の最大化目的値です。

**制限・注意。** flowとdistanceの両方を反転しないでください。単項報酬も別に反転します。最小の符号付き整数値の反転や、積・総和のオーバーフローを避けます。

### 6.8 位置数が多い一般配置と空き位置の選択

**準備と呼出し経路。** `qap_problem`(location_count)を作り、先頭real_object_count物体のflowとunaryだけ設定します。残りのダミー物体の行・列・unaryは0です。全位置間distanceを設定し、solve_qapを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `real_object_count` | 実物体数m。0以上、location_count以下 |
| `location_count` | 位置数l。solver内部のnはl |
| `real_flow` | m*mの実物体間行列 |
| `location_distance` | l*lの全位置間行列 |
| `assignment_cost` | 空またはm*lの実物体×位置費用 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_qap_with_empty_locations(
    real_object_count,
    location_count,
    real_flow,
    location_distance,
    assignment_cost,
    deadline,
    seed);
```

**戻り値と利用。** `qap_with_empty_locations_result`。location_of_real_objectは実物体の位置だけ、empty_locationsは空き位置の昇順一覧、costは実物体に関する費用です。

**制限・注意。** 実物体の配置は全位置範囲への単射です。ダミー番号自体に意味はありません。空き位置の維持費が必要なら、この例のダミーunary=0というモデルを拡張します。

### 6.9 候補の選択と配置を同時に決める

**準備と呼出し経路。** `qap_problem`(candidate_count)を作り、不採用を表すダミー位置を追加します。ダミーに関係するdistanceを0、ダミー位置のunaryを各候補の不採用費用にしてsolve_qapを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `candidate_count` | 候補数c |
| `real_location_count` | 実位置数l。0以上c以下。採用数はちょうどl |
| `candidate_flow` | c*cの候補間行列 |
| `real_location_distance` | l*lの実位置間行列 |
| `placement_cost` | c*lの採用時費用。不要でも全要素0の行列を渡す |
| `unselected_cost` | 候補ごとの不採用費用をc個。不要なら0 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = select_and_place_by_qap(
    candidate_count,
    real_location_count,
    candidate_flow,
    real_location_distance,
    placement_cost,
    unselected_cost,
    deadline,
    seed);
```

**戻り値と利用。** `qap_selection_placement_result`。object_at_real_location[x]が実位置xの採用候補、unselected_objectsが不採用候補、costが不採用費用も含む目的値です。

**制限・注意。** 実位置を空けることは許していません。最大l個までという条件には空き用ダミー物体も必要です。不採用候補にも相互作用が残る問題はこの変換に一致しません。

### 6.10 配置の希望・禁止を罰則で扱う

**準備と呼出し経路。** 受け取ったproblemのコピーにn*nのunaryを用意し、指定違反の費用を加算します。その問題をsolve_qapで解きます。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 構築済み`qap_problem<int,long long>`。値渡しなので呼出し元は変更されない |
| `preferred_location` | n個。希望位置番号、希望なしは-1 |
| `forbidden` | 避けたい(object,location)の列。重複すると罰則も重複 |
| `penalty` | 非負の罰則。希望位置以外・禁止ペアのそれぞれに加算 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_qap_with_soft_constraints(
    problem,
    preferred_location,
    forbidden,
    penalty,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。costは罰則込みです。返却後に希望・禁止の充足を利用側で調べます。

**制限・注意。** penaltyは物理的な違反費用や許容交換条件から選びます。無根拠に巨大な値を入れると中間値があふれたり、探索尺度が偏ったりします。元の費用が必要なら呼出し元problemで配置を再評価します。

### 6.11 大きな整数目的値を持つ配置

**準備と呼出し経路。** `qap_problem<long long,__int128_t>`へ3行列を渡し、solve_qapを呼びます。行列を値渡しで受け取り、構築時にはmoveします。

| 引数 | 設定する内容 |
| --- | --- |
| `n` | 物体数＝位置数 |
| `flow` | long longのn*n要素 |
| `distance` | long longのn*n要素 |
| `unary` | 空または`__int128_t`のn*n要素 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = solve_wide_score_qap(
    n,
    flow,
    distance,
    unary,
    deadline,
    seed);
const std::string cost_text = qap_int128_to_string(result.cost);
```

**戻り値と利用。** `qap_result<__int128_t>`。配置は通常と同じintのvectorです。costの表示には同じ例の`qap_int128_to_string`(result.cost)を使います。

**制限・注意。** Scoreを先に広げた演算が必要です。呼出し前にlong longで乗算してあふれた値を`__int128_t`へキャストしても直りません。GNU拡張を使います。

### 6.12 既存の完全配置を改善する

**準備と呼出し経路。** 利用側で`qap_problem`(n)とflow・distance・必要なunaryを完成させます。既存配置をimprove_qapの第2引数へ渡します。通常solve_qapに初期解を追加するoverloadはありません。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 構築済みの不変なQAP |
| `initial_location_of` | 長さnで、0..n-1を1回ずつ含む完全置換 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = improve_existing_qap_solution(
    problem,
    initial_location_of,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。cost<=problem.evaluate(initial_location_of)を満たす完全配置です。入力vectorは変更せず、返却値が配置を所有します。

**制限・注意。** 期限切れでも開始配置の目的値は再計算します。改善を保証するのはこのQAP目的値だけです。本来の評価が別なら用途18を使います。

### 6.13 同じ問題に対する複数の初期解を比較する

**準備と呼出し経路。** `qap_prepared`(problem)を1回構築します。残り時間を残り候補数で割り、各候補をprepared.improveへ渡します。比較用のbestには最初の候補を必ず入れます。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 全候補に共通する不変のQAP |
| `initial_candidates` | 1個以上の完全置換。すべて同じnと番号体系 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = improve_best_qap_initial_candidate(
    problem,
    initial_candidates,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。処理できた候補の改善結果と、最初の初期解の中で最良の配置です。未処理候補のcostまで評価する保証はありません。

**制限・注意。** 候補数の上限や配分は外側の方針です。preparedはCSR等を共有しますが、開始costの再評価と各探索の内部状態構築は毎回必要です。

### 6.14 固定物体を保った部分repair

**準備と呼出し経路。** 問題と配置を用意し、必要ならproblem.evaluateでcurrent_costを計算します。既知cost版improve_qap_subset(problem,current_location_of,movable_objects,current_cost,...)を呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 構築済みの不変なQAP |
| `current_location_of` | 現在の完全置換 |
| `current_cost` | このproblemで現在配置を評価した正しいlong long値 |
| `movable_objects` | 重複のない可動物体番号。順序は任意 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = repair_affected_qap_objects(
    problem,
    current_location_of,
    current_cost,
    movable_objects,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。可動集合以外の位置は不変で、costは開始値以下です。返却値は縮約解ではなく完全配置です。

**制限・注意。** 空き位置も使わせるには、その位置のダミー物体を可動集合に入れます。k<2はそのまま返却、k==nは通常improveへ委譲します。costが不明なら6.0のcost省略版を使えます。

### 6.15 可動集合を変えながら部分repairを反復する

**準備と呼出し経路。** 現在配置とcostを保持し、残り時間を残り反復数で割ります。select_movableの返却集合で既知cost版improve_qap_subsetを呼び、その完全結果を次回へ引き継ぎます。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 反復中に変更しないQAP |
| `initial_location_of` | 開始する完全置換。値渡し |
| `initial_cost` | 開始配置の正しいQAP cost |
| `rounds` | 0以上の反復上限 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。この例では省略不可 |
| `select_movable` | (const `vector<int>`&現在配置,int round)->`vector<int>`。範囲内で重複なしの可動集合を返す |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = run_iterated_qap_subset_repair(
    problem,
    initial_location_of,
    initial_cost,
    rounds,
    deadline,
    seed,
    select_movable);
```

**戻り値と利用。** `qap_result<long long>`。各repairで非悪化を維持した、最後の完全配置です。rounds=0なら開始値を返します。

**制限・注意。** seedは例の関数では必須引数です。selectorの処理時間も外側の締切に含まれます。kやroundsは問題分布で測り、短すぎるrepairの構築費に注意します。同じ番号の例に単純な巡回集合selectorも同梱します。

### 6.16 小部分を厳密列挙でrepairする

**準備と呼出し経路。** make_qap_subproblemの既知cost版で縮約します。縮約の全置換をevaluateし、最良の`qap_result`をsubproblem.restoreで完全配置へ戻します。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 構築済みの不変なQAP |
| `current_location_of` | 現在の完全置換 |
| `current_cost` | 現在配置の正しいQAP cost |
| `movable_objects` | 重複なしの可動物体。例では個数k<=10を要求 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = exact_repair_small_qap_subset(
    problem,
    current_location_of,
    current_cost,
    movable_objects);
```

**戻り値と利用。** `qap_result<long long>`。指定位置集合内の厳密な部分最適解で、外部固定条件と非悪化を満たします。

**制限・注意。** この関数にはdeadlineもseedもありません。全列挙を完了するまで戻らず、k=10でも速いとは限りません。時間に収まるkへ利用側で制限してください。

### 6.17 QAP差分を独自の局所探索へ組み込む

**準備と呼出し経路。** `qap_prepared`(problem)と`qap_swap_state`(prepared,initial)を作ります。state.swap_deltaとextra_deltaの合計が負なら、state.apply_swapを実行し、その後apply_extraを呼びます。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 探索中に不変のQAP |
| `initial_location_of` | 完全置換。値渡し |
| `initial_extra_cost` | 開始時の追加目的値。QAP costを含めない |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。この例では省略不可 |
| `extra_delta` | (int i,int j,const `vector<int>`&交換前配置)->long long。追加費用の交換後−交換前を返す |
| `apply_extra` | (int i,int j,const `vector<int>`&交換後配置)->void。採用時だけ追加状態を更新する |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = improve_with_qap_swap_state(
    problem,
    initial_location_of,
    initial_extra_cost,
    deadline,
    seed,
    extra_delta,
    apply_extra);
```

**戻り値と利用。** `qap_combined_search_result`。location_of、`qap_cost`、extra_costを返します。保証する非悪化は2つのcostの合計です。

**制限・注意。** seedは必須です。extra_deltaは状態を変更せず、apply_extraは必ず交換後の配置に同期させてください。apply_swapは差分を再計算します。この例は単純な山登りで、QAP cost単独が悪化する手も合計が良ければ受理します。

### 6.18 QAPを代理評価として使い、本来スコアで採否する

**準備と呼出し経路。** 既知cost版improve_qap_subsetで完全候補を作ります。true_score(candidate.location_of)を呼び、元の本来スコアより小さいときだけ採用します。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 代理目的を表す不変のQAP |
| `current_location_of` | 現在の完全置換。値渡し |
| `current_qap_cost` | 現在配置の正しいQAP cost |
| `current_true_score` | 現在配置の正しい本来スコア。この例では小さいほど良い |
| `movable_objects` | 重複なしの可動物体 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。この例では省略不可 |
| `true_score` | (const `vector<int>`&配置)->long long。本来の最小化目的値を返す |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = repair_qap_if_true_score_improves(
    problem,
    current_location_of,
    current_qap_cost,
    current_true_score,
    movable_objects,
    deadline,
    seed,
    true_score);
```

**戻り値と利用。** `qap_true_score_result`。location_of、`qap_cost`、true_score、acceptedを返します。棄却時は元の3値を保ちaccepted=falseです。

**制限・注意。** seedは必須です。本来スコアの再評価時間も確保します。最大化目的なら比較方向を変更する必要があります。ハード制約がある場合は、本評価時に実行可能性も確認してください。

### 6.19 入力行列が少し変わるターン制問題

**準備と呼出し経路。** problemのコピーへ指定要素を上書きし、必要ならunary全体を0で確保します。更新後のproblemとprevious_location_ofでimprove_qapを呼び、新しいcostと配置を返します。

| 引数 | 設定する内容 |
| --- | --- |
| `problem` | 変更前の`qap_problem<int,long long>`。コピーに変更を適用する |
| `previous_location_of` | 同じnと番号体系の前回完全配置 |
| `flow_updates` | (row,column,value)の列。物体間行列の新しい値 |
| `distance_updates` | (row,column,value)の列。位置間行列の新しい値 |
| `unary_updates` | (object,location,value)の列。単項費用の新しいlong long値 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = reoptimize_qap_after_updates(
    problem,
    previous_location_of,
    flow_updates,
    distance_updates,
    unary_updates,
    deadline,
    seed);
```

**戻り値と利用。** `qap_result<long long>`。更新後の目的関数で前回配置を評価した値以下です。呼出し元problemは変更されません。

**制限・注意。** 同じ要素が複数回出ると最後の指定が勝ちます。変更量を加算するAPIではありません。対称な辺は両方向を指定します。元の行列を次ターンへ持ち越すなら、呼出し元にも同じ更新を適用して管理します。

### 6.20 2つの重み付きグラフの対応付け

**準備と呼出し経路。** 差の二乗を展開し、flow=-2*graph_a、distance=graph_b、unary=assignment_costの`qap_problem<long long,long long>`を作ります。solve_qapのcostへ両行列の二乗和という定数を戻します。

| 引数 | 設定する内容 |
| --- | --- |
| `n` | 両グラフ共通の頂点数 |
| `graph_a` | n*nのint行列。第1グラフの辺重み |
| `graph_b` | n*nのint行列。第2グラフの辺重み |
| `assignment_cost` | 空またはn*nのlong long。頂点iを頂点xへ対応させる費用 |
| `deadline` | std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する |
| `seed` | std::uint64_tの乱数seed。既定値1 |

第8章の対応関数を用意した後の呼出しは次の形です。

```cpp
const auto result = match_weighted_graphs_by_qap(
    n,
    graph_a,
    graph_b,
    assignment_cost,
    deadline,
    seed);
```

**戻り値と利用。** `qap_graph_matching_result`。vertex_in_b[i]が第1グラフの頂点iに対応する第2グラフの頂点、costが差の二乗和＋単項費用です。

**制限・注意。** 全頂点の一対一対応だから、第2グラフの二乗和が置換に依存しません。異なる頂点数や部分対応にはそのまま使えません。対称グラフの非対角差は両方向で数えます。定数項と負の交差項もlong longに収めます。


第1グラフを $A$、第2グラフを $B$、頂点対応を $p$、単項対応費用を $U$ とすると、元の目的式は次です。

$$E(p)=\sum_{i=0}^{n-1}\sum_{j=0}^{n-1}(A_{i,j}-B_{p_i,p_j})^2+\sum_{i=0}^{n-1}U_{i,p_i}$$

二乗を展開すると、$\sum A_{i,j}^2+\sum B_{x,y}^2$ は定数で、対応に依存する交差項は $-2A_{i,j}B_{p_i,p_j}$ です。このためQAPのflowを $-2A$、distanceを $B$ とできます。返却時には定数を戻し、QAP内部の負のcostをそのまま二乗誤差として出力しません。

## 7. 制約・注意点

### 7.1 モデルの制限

| 項目 | 制約・対処 |
| --- | --- |
| 物体数と位置数 | 同数で一対一。数が違う場合はダミーを含むQAPへ変換する |
| 未完成の初期解 | `-1` や重複位置を含む配置は不可。完全な置換を先に作る |
| 二次費用の形 | 1つの `flow` と1つの `distance` の積。独立した複数積をそれぞれ評価するcallbackはない |
| 任意の制約 | 一般の配置禁止、重み付き容量、多資源容量、隣接必須などを直接保証する機構はない |
| 厳密な固定 | 固定を満たす完全配置を先に作り、固定物体を可動集合から外す |
| 任意の候補位置集合 | subsetに位置一覧を直接渡すことはできない。可動物体が現在使う位置から決まる |
| soft penalty | 違反を許す費用。有限のpenaltyだけで返却解の実行可能性は保証されない |
| 最適性 | 内蔵探索は最適解・近似率を保証しない。全列挙を完走した例8.16は固定条件内で厳密 |
| 本来スコア | QAPと別の評価なら非悪化保証は移らない。用途18のように本評価して採否する |

ハード制約を満たす配置だけを使いたい場合は、違反が起きない問題変換、実行可能な初期解と部分固定、または実行可能なswapだけを選ぶ独自探索を検討します。公開のswap差分APIは、その独自探索にも利用できます。

### 7.2 型・サイズとオーバーフロー

`flow` と `distance` は同じ `Weight` 型です。異なる型の入力を使う場合は、共通の型へ変換します。`Score` は符号付き整数を基本とし、符号なし型では負の交換差分を表せません。浮動小数点型は、厳密な対称性・ゼロ判定と差分の累積誤差を含むため、本書の整数向け契約とテストの対象とは区別してください。

このライブラリは数値オーバーフローを検査しません。最終costだけでなく、次も型に収めます。

- 入力作成時の辺重みの加算、座標差、距離、符号反転
- `Score(flow)*Score(distance)`、単項費用の差、全目的値の和
- 初期配置に使う `abs(flow)` の入出和、`distance` の入出和
- `cost+delta`、複数の差分の和、差分表更新の係数差と積
- 3物体の有向辺差6項の和同士の積、その補正を差分へ足し引きする途中値

目的値の絶対値については、少なくとも次の上界を見積もれます。

$$B=\sum_{i=0}^{n-1}\max_x \lvert U_{i,x}\rvert+\left(\sum_{i=0}^{n-1}\sum_{j=0}^{n-1}\lvert F_{i,j}\rvert\right)\max_{x,y}\lvert D_{x,y}\rvert$$

$B$ は最終目的値の絶対値の上界です。すべての中間式の上界という意味ではないため、差分や補正式にも余裕を持たせます。計算型の最小値の絶対値化や符号反転は、その型に収まらない点にも注意します。

行列サイズと添字の `n*n`・`i*n+j` はint演算です。32bit intでは `n<=46340` が必要ですが、この数値は実用的なサイズ上限を保証しません。密行列のメモリが先に問題になります。負のn、サイズ不一致、範囲外添字も入力しないでください。

### 7.3 検査は入口によって異なる

`assert` は `-DNDEBUG` で消えます。また、すべてのメソッドが入力全体を毎回検査するわけではありません。

| 誤り | 起きること |
| --- | --- |
| `flow`・`distance` が `n*n` でない | 範囲外アクセスなどの原因になる。構築後にpublicメンバーを書き換えた場合も利用側の責任 |
| `unary` が空でも `n*n` でもない | 部分行列としては解釈されない |
| `prepared.swap_delta` に無効な置換を渡す | 長さとswap添字の検査だけで、置換全体の正しさは調べない |
| `movable_objects` に重複や範囲外番号 | 不正な縮約になる。物体番号と位置番号の取り違えにも注意 |
| 既知costが実際と異なる | 自動照合されず、返却costがずれるなど正しい結果を保証できない |
| `restore` の縮約costが誤っている | 誤った値へ定数項を足して返す |

入力を組み立てる段階でサイズを確認し、初期置換は必要に応じて `is_valid_permutation` で検査します。n=0の空問題は扱えますが、空問題にswapする物体番号は存在しません。n>0で同じ有効番号を2つ指定したswap差分は0です。

### 7.4 参照先の寿命・変更禁止

preparedはproblemを、stateはpreparedを借ります。参照先の破棄・move・内容変更を、利用中に行ってはいけません。

例えば関数内で作ったproblemを参照するpreparedを返すと、関数終了後に参照先がなくなります。一時的なproblemでpreparedを構築した場合も同様です。コピーしたpreparedが元problemを自動でコピーしてくれるわけではありません。

subproblemは元問題の必要なデータをコピーして所有します。ただし `qap_prepared(sub.problem)` を作った後は、その `sub.problem` が参照先です。subを破棄・moveしてはいけません。入力更新後に古いsubを使っても、新しい問題の縮約にはなりません。

### 7.5 存在しないAPI・組み合わせ

| 呼出しの意図 | 正しい方法 |
| --- | --- |
| `solve_qap` に初期解を渡す | `improve_qap` を使う |
| `prepared.improve(initial, known_cost, ...)` | このoverloadはない。開始costの全評価が必要 |
| `state.apply_swap(i,j,known_delta)` | このoverloadはない。apply_swapが差分を計算する |
| preparedの行列だけを差分更新する | 更新APIはない。新しい問題に対して再構築する |
| stateを別のpreparedへ差し替える | 再接続APIはない。新しいstateを作る |
| 元problem用preparedを縮約problemへ使う | 別問題なので不可。縮約problem用に構築する |
| 単項費用をn個だけ渡す | 物体×位置の `n*n` 行列へ展開する |
| ミリ秒数・system_clock時刻をdeadlineへ渡す | `steady_clock::time_point` を作る |
| benchmark用の内部型をアプリで使う | 公開APIを使う。テスト用アクセスはアプリ用APIではない |

### 7.6 目的値の取り違え

無向辺を両方向へ入れる場合、二次項は二重計上です。unaryとの相対重みも合わせます。最大化へ変換する場合は、flowとdistanceの片方だけを反転し、最大化対象のunaryも反転します。

QAP外の定数費用は通常の `qap_result::cost` に自動では含まれません。用途20はadapterで定数項を戻します。部分問題の `restore` は、自身が管理する `constant_offset` を加えます。

入力更新後のcostと更新前のcostを、そのまま改善率の分母・分子に使わないでください。更新後の問題で開始配置と返却配置を評価して比べます。

### 7.7 実時間と再現性

deadlineは強制停止機構ではありません。初期配置、全評価、対称性判定、CSR構築、縮約構築などに時刻確認を細かく挟まない処理があります。期限を過ぎてから呼んでも、6.0の表にある処理は行われます。

同じseedで厳密に同じ最終配置になる保証もありません。単一スレッドでの使用・検証を前提にし、実時間の比較中にほかの重い計測を同時実行すると条件が変わります。

### 7.8 callbackを使う例の契約

用途15のselectorは範囲内で重複のない物体番号を返します。用途17の `extra_delta` は交換前の追加費用差だけを返し、状態を変更しません。`apply_extra` は交換後の配置を受け取って追加状態を同期します。途中で例外を投げるなどして片側だけを更新すると整合しなくなるので、この例では更新が完了することを前提にします。

用途18の `true_score` は配置だけから本来スコアを計算し、候補の採否とは別に現在状態を変更しないようにします。これらのcallbackは8章の利用側関数の引数であり、内蔵solveのcallback引数ではありません。

## 8. ユースケースごとのコード例

各ブロックは必要なデータ型・関数とincludeをまとめており、ヘッダーがあれば単独の翻訳単位としてコンパイルできます。mainと入力の読み取りは利用側で用意してください。その用途に必要なブロックをコピーするか、全例をまとめた `qap_solver_usecase_examples.hpp` をincludeします。

入力・出力のコメントは各ブロック冒頭にあります。空vectorを許す引数も、引数そのものに既定値がない場合は `{}` を渡します。assertは開発時の検査であり、整数オーバーフローの全面的な検査ではありません。入力構築・コピー・callbackを含む時間予算は利用側で確保します。

### 8.1 対称な施設・機器配置

呼出しと返却値の説明は6.1、概念説明は4.1です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - n: 物体数＝位置数。0以上
// - edges: 無向辺の列。各要素のu,vは物体番号、weightは関係の強さ。各辺は1回だけ渡す。重複辺は加算
// - location_xy: 位置番号順の整数座標(x,y)をn個
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。location_of[i]が機器iの位置番号、costが向き付き二重和です。位置座標はlocation_xy[location_of[i]]で取得します。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_undirected_flow_edge {
    int u;
    int v;
    int weight;
};

inline qap_result<long long> solve_symmetric_facility_layout(
    int n,
    const std::vector<qap_undirected_flow_edge>& edges,
    const std::vector<std::pair<int, int>>& location_xy,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(n >= 0);
    assert(std::ssize(location_xy) == n);
    qap_problem<int, long long> problem(n);

    for (const auto& edge : edges) {
        assert(0 <= edge.u && edge.u < n);
        assert(0 <= edge.v && edge.v < n);
        assert(edge.u != edge.v);
        problem.flow[edge.u * n + edge.v] += edge.weight;
        problem.flow[edge.v * n + edge.u] += edge.weight;
    }

    for (int x = 0; x < n; ++x) {
        for (int y = 0; y < n; ++y) {
            problem.distance[x * n + y] =
                std::abs(location_xy[x].first - location_xy[y].first) +
                std::abs(location_xy[x].second - location_xy[y].second);
        }
    }
    return solve_qap(problem, deadline, seed);
}
```

単項項を使わないこの例では、costは無向辺を1回ずつ数えた費用の2倍です。座標の差・距離・辺重みの加算はint、目的値の全中間値はlong longに収まる必要があります。

### 8.2 疎グラフのグリッド配置

呼出しと返却値の説明は6.2、概念説明は4.2です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - vertex_count: 実頂点数。0以上、height*width以下
// - height: グリッドの行数
// - width: グリッドの列数。通常はheight,widthとも正
// - edges: 実頂点間の無向辺(u,v,weight)。各辺1回、重複は加算
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_grid_layout_result。cell_of_vertex[v]がセル番号、empty_cellsが昇順の空きセル一覧、costが費用です。行はcell/width、列はcell%widthです。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_grid_edge {
    int u;
    int v;
    int weight;
};

struct qap_grid_layout_result {
    std::vector<int> cell_of_vertex;
    std::vector<int> empty_cells;
    long long cost;
};

inline qap_grid_layout_result solve_sparse_graph_on_grid(
    int vertex_count,
    int height,
    int width,
    const std::vector<qap_grid_edge>& edges,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(vertex_count >= 0);
    assert(height >= 0 && width >= 0);
    const int n = height * width;
    assert(vertex_count <= n);
    qap_problem<int, long long> problem(n);

    for (const auto& edge : edges) {
        assert(0 <= edge.u && edge.u < vertex_count);
        assert(0 <= edge.v && edge.v < vertex_count);
        assert(edge.u != edge.v);
        problem.flow[edge.u * n + edge.v] += edge.weight;
        problem.flow[edge.v * n + edge.u] += edge.weight;
    }

    for (int a = 0; a < n; ++a) {
        const int ay = a / width;
        const int ax = a % width;
        for (int b = 0; b < n; ++b) {
            const int by = b / width;
            const int bx = b % width;
            problem.distance[a * n + b] =
                std::abs(ay - by) + std::abs(ax - bx);
        }
    }

    auto result = solve_qap(problem, deadline, seed);
    std::vector<int> cell_of_vertex(vertex_count);
    for (int v = 0; v < vertex_count; ++v) {
        cell_of_vertex[v] = result.location_of[v];
    }
    std::vector<int> empty_cells;
    empty_cells.reserve(n - vertex_count);
    for (int dummy = vertex_count; dummy < n; ++dummy) {
        empty_cells.push_back(result.location_of[dummy]);
    }
    std::sort(empty_cells.begin(), empty_cells.end());
    return {std::move(cell_of_vertex), std::move(empty_cells), result.cost};
}
```

costは無向辺の重み付き距離和の2倍です。実頂点の返却値は0..vertex_count-1の置換ではなく、全セルを範囲とする重複なしの割当です。疎な辺でも入力行列のメモリはO(n^2)です。

### 8.3 固定人数のグループ分け

呼出しと返却値の説明は6.3、概念説明は4.3です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - group_count: グループ数。0以上
// - capacity: 各グループの正の人数。n=group_count*capacity
// - affinity: n*n要素。affinity[i*n+j]は別グループに分けたときの関係費用
// - group_cost: 空またはn*group_count要素。group_cost[i*group_count+g]は物体iをグループgへ入れる単項費用
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_fixed_partition_result。group_of_object[i]がグループ番号、costがペア分割費用と単項費用の合計です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_fixed_partition_result {
    std::vector<int> group_of_object;
    long long cost;
};

inline qap_fixed_partition_result solve_fixed_capacity_partition(
    int group_count,
    int capacity,
    const std::vector<int>& affinity,
    const std::vector<long long>& group_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(group_count >= 0);
    assert(capacity > 0);
    const int n = group_count * capacity;
    assert(std::ssize(affinity) == n * n);
    assert(group_cost.empty() ||
           std::ssize(group_cost) == n * group_count);

    qap_problem<int, long long> problem(n);
    problem.flow = affinity;
    for (int x = 0; x < n; ++x) {
        const int group_x = x / capacity;
        for (int y = 0; y < n; ++y) {
            const int group_y = y / capacity;
            problem.distance[x * n + y] = group_x == group_y ? 0 : 1;
        }
    }

    if (!group_cost.empty()) {
        problem.unary.assign(n * n, 0);
        for (int object = 0; object < n; ++object) {
            for (int location = 0; location < n; ++location) {
                const int group = location / capacity;
                problem.unary[object * n + location] =
                    group_cost[object * group_count + group];
            }
        }
    }

    auto result = solve_qap(problem, deadline, seed);
    std::vector<int> group_of_object(n);
    for (int object = 0; object < n; ++object) {
        group_of_object[object] = result.location_of[object] / capacity;
    }
    return {std::move(group_of_object), result.cost};
}
```

関係が正なら同じグループへ集めたい意味、負なら離したい意味になります。対称入力のペア費用は2回数えます。同じグループ内の位置番号に意味はありません。不均一な容量には位置→グループ配列を持つ別のadapterが必要です。

### 8.4 巡回順序・円環配置

呼出しと返却値の説明は6.4、概念説明は4.4です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - n: 物体数。例では2以上
// - directed_cost: n*n要素。directed_cost[i*n+j]はiの次をjにする費用
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_cycle_result。order_of_object[i]が巡回順の番号、object_at_order[t]がt番目の物体です。costは最後から先頭も含む巡回費用です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_cycle_result {
    std::vector<int> order_of_object;
    std::vector<int> object_at_order;
    long long cost;
};

inline qap_cycle_result solve_cyclic_order_as_qap(
    int n,
    const std::vector<int>& directed_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(n >= 2);
    assert(std::ssize(directed_cost) == n * n);
    qap_problem<int, long long> problem(n);
    problem.flow = directed_cost;

    for (int order = 0; order < n; ++order) {
        const int next_order = (order + 1) % n;
        problem.distance[order * n + next_order] = 1;
    }

    auto result = solve_qap(problem, deadline, seed);
    std::vector<int> object_at_order(n);
    for (int object = 0; object < n; ++object) {
        object_at_order[result.location_of[object]] = object;
    }
    return {std::move(result.location_of),
            std::move(object_at_order), result.cost};
}
```

この変換は有向巡回にも対応し、巡回辺を1回ずつ数えます。純粋なTSP専用近傍と同じ探索性能を意味しません。円環の回転対称性を除くには、固定を満たす初期置換を作って固定物体を可動集合から外せます。

### 8.5 有向・非対称の物流や通信配置

呼出しと返却値の説明は6.5、概念説明は4.5です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - n: 物体数＝位置数
// - arcs: 有向辺(from,to,amount)。amountはその向きの流量。重複は加算
// - directed_location_cost: n*n要素。位置xからyへの費用を[x*n+y]に入れる
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。location_of[i]が物体iの位置、costが方向を維持した流量×位置間費用の総和です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_directed_arc {
    int from;
    int to;
    int amount;
};

inline qap_result<long long> solve_directed_transport_layout(
    int n,
    const std::vector<qap_directed_arc>& arcs,
    const std::vector<int>& directed_location_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(n >= 0);
    assert(std::ssize(directed_location_cost) == n * n);
    qap_problem<int, long long> problem(n);
    problem.distance = directed_location_cost;

    for (const auto& arc : arcs) {
        assert(0 <= arc.from && arc.from < n);
        assert(0 <= arc.to && arc.to < n);
        problem.flow[arc.from * n + arc.to] += arc.amount;
    }
    return solve_qap(problem, deadline, seed);
}
```

逆向きの辺を自動補完しません。from==toも入力できます。対角項を不要とするなら、入力時に0にしてください。

### 8.6 位置適性と移設費を含む再配置

呼出しと返却値の説明は6.6、概念説明は4.6です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - n: 物体数＝位置数
// - flow: n*nの物体間行列
// - pair_distance: n*nの二次費用用位置間行列
// - base_assignment_cost: 空またはn*nの基礎単項費用
// - old_location_of: 物体→現在位置の完全置換
// - relocation_distance: n*n。旧位置xから新位置yへ移す非負距離
// - move_weight: 非負の移動単価。距離と掛けて単項費用へ加える
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。costには移設費も含みます。移設費を含む新しい目的関数で開始配置以下の値を返します。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

inline qap_result<long long> solve_qap_reconfiguration(
    int n,
    const std::vector<int>& flow,
    const std::vector<int>& pair_distance,
    const std::vector<long long>& base_assignment_cost,
    const std::vector<int>& old_location_of,
    const std::vector<int>& relocation_distance,
    long long move_weight,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(n >= 0);
    assert(std::ssize(flow) == n * n);
    assert(std::ssize(pair_distance) == n * n);
    assert(base_assignment_cost.empty() ||
           std::ssize(base_assignment_cost) == n * n);
    assert(std::ssize(relocation_distance) == n * n);
    assert(move_weight >= 0);

    qap_problem<int, long long> problem(n);
    problem.flow = flow;
    problem.distance = pair_distance;
    problem.unary.assign(n * n, 0);
    if (!base_assignment_cost.empty()) {
        problem.unary = base_assignment_cost;
    }
    assert(problem.is_valid_permutation(old_location_of));

    for (int object = 0; object < n; ++object) {
        const int old_location = old_location_of[object];
        for (int location = 0; location < n; ++location) {
            problem.unary[object * n + location] +=
                move_weight *
                static_cast<long long>(
                    relocation_distance[old_location * n + location]);
        }
    }
    return improve_qap(problem, old_location_of, deadline, seed);
}
```

ターンが進んで移動の起点が変わったらunaryを作り直します。古いpreparedや古いcostを使い回しません。move_weight=0は移設費を無視する意味です。

### 8.7 親和度・配置報酬の最大化

呼出しと返却値の説明は6.7、概念説明は4.7です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - n: 物体数＝位置数
// - affinity: long longのn*n要素。物体ペアの親和度
// - proximity: long longのn*n要素。位置ペアの近接度・報酬係数
// - assignment_reward: 空またはn*n要素。物体×位置の報酬
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_maximization_result。location_ofが配置、scoreが元の最大化目的値です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_maximization_result {
    std::vector<int> location_of;
    long long score;
};

inline qap_maximization_result maximize_qap_affinity(
    int n,
    std::vector<long long> affinity,
    std::vector<long long> proximity,
    const std::vector<long long>& assignment_reward,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(n >= 0);
    assert(std::ssize(affinity) == n * n);
    assert(std::ssize(proximity) == n * n);
    assert(assignment_reward.empty() ||
           std::ssize(assignment_reward) == n * n);

    for (long long& value : affinity) {
        assert(value != std::numeric_limits<long long>::min());
        value = -value;
    }
    std::vector<long long> unary;
    if (!assignment_reward.empty()) {
        unary.resize(n * n);
        for (int index = 0; index < n * n; ++index) {
            assert(assignment_reward[index] !=
                   std::numeric_limits<long long>::min());
            unary[index] = -assignment_reward[index];
        }
    }

    qap_problem<long long, long long> problem(
        n, std::move(affinity), std::move(proximity), std::move(unary));
    auto result = solve_qap(problem, deadline, seed);
    assert(result.cost != std::numeric_limits<long long>::min());
    return {std::move(result.location_of), -result.cost};
}
```

flowとdistanceの両方を反転しないでください。単項報酬も別に反転します。最小の符号付き整数値の反転や、積・総和のオーバーフローを避けます。

### 8.8 位置数が多い一般配置と空き位置の選択

呼出しと返却値の説明は6.8、概念説明は4.8です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - real_object_count: 実物体数m。0以上、location_count以下
// - location_count: 位置数l。solver内部のnはl
// - real_flow: m*mの実物体間行列
// - location_distance: l*lの全位置間行列
// - assignment_cost: 空またはm*lの実物体×位置費用
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_with_empty_locations_result。location_of_real_objectは実物体の位置だけ、empty_locationsは空き位置の昇順一覧、costは実物体に関する費用です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_with_empty_locations_result {
    std::vector<int> location_of_real_object;
    std::vector<int> empty_locations;
    long long cost;
};

inline qap_with_empty_locations_result solve_qap_with_empty_locations(
    int real_object_count,
    int location_count,
    const std::vector<int>& real_flow,
    const std::vector<int>& location_distance,
    const std::vector<long long>& assignment_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(0 <= real_object_count && real_object_count <= location_count);
    assert(std::ssize(real_flow) == real_object_count * real_object_count);
    assert(std::ssize(location_distance) == location_count * location_count);
    assert(assignment_cost.empty() ||
           std::ssize(assignment_cost) == real_object_count * location_count);

    const int n = location_count;
    qap_problem<int, long long> problem(n);
    problem.distance = location_distance;
    for (int i = 0; i < real_object_count; ++i) {
        for (int j = 0; j < real_object_count; ++j) {
            problem.flow[i * n + j] =
                real_flow[i * real_object_count + j];
        }
    }
    if (!assignment_cost.empty()) {
        problem.unary.assign(n * n, 0);
        for (int i = 0; i < real_object_count; ++i) {
            for (int x = 0; x < location_count; ++x) {
                problem.unary[i * n + x] =
                    assignment_cost[i * location_count + x];
            }
        }
    }

    auto result = solve_qap(problem, deadline, seed);
    std::vector<int> real_location(real_object_count);
    for (int i = 0; i < real_object_count; ++i) {
        real_location[i] = result.location_of[i];
    }
    std::vector<int> empty_locations;
    empty_locations.reserve(location_count - real_object_count);
    for (int dummy = real_object_count; dummy < n; ++dummy) {
        empty_locations.push_back(result.location_of[dummy]);
    }
    std::sort(empty_locations.begin(), empty_locations.end());
    return {std::move(real_location),
            std::move(empty_locations), result.cost};
}
```

実物体の配置は全位置範囲への単射です。ダミー番号自体に意味はありません。空き位置の維持費が必要なら、この例のダミーunary=0というモデルを拡張します。

### 8.9 候補の選択と配置を同時に決める

呼出しと返却値の説明は6.9、概念説明は4.9です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - candidate_count: 候補数c
// - real_location_count: 実位置数l。0以上c以下。採用数はちょうどl
// - candidate_flow: c*cの候補間行列
// - real_location_distance: l*lの実位置間行列
// - placement_cost: c*lの採用時費用。不要でも全要素0の行列を渡す
// - unselected_cost: 候補ごとの不採用費用をc個。不要なら0
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_selection_placement_result。object_at_real_location[x]が実位置xの採用候補、unselected_objectsが不採用候補、costが不採用費用も含む目的値です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_selection_placement_result {
    std::vector<int> object_at_real_location;
    std::vector<int> unselected_objects;
    long long cost;
};

inline qap_selection_placement_result select_and_place_by_qap(
    int candidate_count,
    int real_location_count,
    const std::vector<int>& candidate_flow,
    const std::vector<int>& real_location_distance,
    const std::vector<long long>& placement_cost,
    const std::vector<long long>& unselected_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(0 <= real_location_count &&
           real_location_count <= candidate_count);
    assert(std::ssize(candidate_flow) ==
           candidate_count * candidate_count);
    assert(std::ssize(real_location_distance) ==
           real_location_count * real_location_count);
    assert(std::ssize(placement_cost) ==
           candidate_count * real_location_count);
    assert(std::ssize(unselected_cost) == candidate_count);

    const int n = candidate_count;
    qap_problem<int, long long> problem(n);
    problem.flow = candidate_flow;
    for (int x = 0; x < real_location_count; ++x) {
        for (int y = 0; y < real_location_count; ++y) {
            problem.distance[x * n + y] =
                real_location_distance[x * real_location_count + y];
        }
    }

    problem.unary.assign(n * n, 0);
    for (int object = 0; object < candidate_count; ++object) {
        for (int location = 0; location < real_location_count; ++location) {
            problem.unary[object * n + location] =
                placement_cost[object * real_location_count + location];
        }
        for (int dummy = real_location_count; dummy < n; ++dummy) {
            problem.unary[object * n + dummy] = unselected_cost[object];
        }
    }

    auto result = solve_qap(problem, deadline, seed);
    std::vector<int> object_at_location(real_location_count, -1);
    std::vector<int> unselected;
    unselected.reserve(candidate_count - real_location_count);
    for (int object = 0; object < candidate_count; ++object) {
        const int location = result.location_of[object];
        if (location < real_location_count) {
            object_at_location[location] = object;
        } else {
            unselected.push_back(object);
        }
    }
    return {std::move(object_at_location),
            std::move(unselected), result.cost};
}
```

実位置を空けることは許していません。最大l個までという条件には空き用ダミー物体も必要です。不採用候補にも相互作用が残る問題はこの変換に一致しません。

### 8.10 配置の希望・禁止を罰則で扱う

呼出しと返却値の説明は6.10、概念説明は4.10です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 構築済みqap_problem<int,long long>。値渡しなので呼出し元は変更されない
// - preferred_location: n個。希望位置番号、希望なしは-1
// - forbidden: 避けたい(object,location)の列。重複すると罰則も重複
// - penalty: 非負の罰則。希望位置以外・禁止ペアのそれぞれに加算
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。costは罰則込みです。返却後に希望・禁止の充足を利用側で調べます。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_forbidden_assignment {
    int object;
    int location;
};

inline qap_result<long long> solve_qap_with_soft_constraints(
    qap_problem<int, long long> problem,
    const std::vector<int>& preferred_location,
    const std::vector<qap_forbidden_assignment>& forbidden,
    long long penalty,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    const int n = problem.n;
    assert(std::ssize(preferred_location) == n);
    assert(penalty >= 0);
    if (problem.unary.empty()) {
        problem.unary.assign(n * n, 0);
    }

    for (int object = 0; object < n; ++object) {
        const int preferred = preferred_location[object];
        assert(preferred == -1 || (0 <= preferred && preferred < n));
        if (preferred == -1) continue;
        for (int location = 0; location < n; ++location) {
            if (location != preferred) {
                problem.unary[object * n + location] += penalty;
            }
        }
    }
    for (const auto& item : forbidden) {
        assert(0 <= item.object && item.object < n);
        assert(0 <= item.location && item.location < n);
        problem.unary[item.object * n + item.location] += penalty;
    }
    return solve_qap(problem, deadline, seed);
}
```

penaltyは物理的な違反費用や許容交換条件から選びます。無根拠に巨大な値を入れると中間値があふれたり、探索尺度が偏ったりします。元の費用が必要なら呼出し元problemで配置を再評価します。

### 8.11 大きな整数目的値を持つ配置

呼出しと返却値の説明は6.11、概念説明は4.11です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - n: 物体数＝位置数
// - flow: long longのn*n要素
// - distance: long longのn*n要素
// - unary: 空または__int128_tのn*n要素
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<__int128_t>。配置は通常と同じintのvectorです。costの表示には同じ例のqap_int128_to_string(result.cost)を使います。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

inline qap_result<__int128_t> solve_wide_score_qap(
    int n,
    std::vector<long long> flow,
    std::vector<long long> distance,
    std::vector<__int128_t> unary,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(n >= 0);
    assert(std::ssize(flow) == n * n);
    assert(std::ssize(distance) == n * n);
    assert(unary.empty() || std::ssize(unary) == n * n);
    qap_problem<long long, __int128_t> problem(
        n, std::move(flow), std::move(distance), std::move(unary));
    return solve_qap(problem, deadline, seed);
}

inline std::string qap_int128_to_string(__int128_t value) {
    if (value == 0) return "0";
    const bool negative = value < 0;
    __uint128_t magnitude;
    if (negative) {
        magnitude = static_cast<__uint128_t>(-(value + 1));
        ++magnitude;
    } else {
        magnitude = static_cast<__uint128_t>(value);
    }

    std::string result;
    while (magnitude > 0) {
        const int digit = static_cast<int>(magnitude % 10);
        result.push_back(static_cast<char>('0' + digit));
        magnitude /= 10;
    }
    if (negative) result.push_back('-');
    std::reverse(result.begin(), result.end());
    return result;
}
```

Scoreを先に広げた演算が必要です。呼出し前にlong longで乗算してあふれた値を`__int128_t`へキャストしても直りません。GNU拡張を使います。

### 8.12 既存の完全配置を改善する

呼出しと返却値の説明は6.12、概念説明は4.12です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 構築済みの不変なQAP
// - initial_location_of: 長さnで、0..n-1を1回ずつ含む完全置換
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。cost<=problem.evaluate(initial_location_of)を満たす完全配置です。入力vectorは変更せず、返却値が配置を所有します。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

inline qap_result<long long> improve_existing_qap_solution(
    const qap_problem<int, long long>& problem,
    const std::vector<int>& initial_location_of,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(problem.is_valid_permutation(initial_location_of));
    return improve_qap(problem, initial_location_of, deadline, seed);
}
```

期限切れでも開始配置の目的値は再計算します。改善を保証するのはこのQAP目的値だけです。本来の評価が別なら用途18を使います。

### 8.13 同じ問題に対する複数の初期解を比較する

呼出しと返却値の説明は6.13、概念説明は4.13です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 全候補に共通する不変のQAP
// - initial_candidates: 1個以上の完全置換。すべて同じnと番号体系
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。処理できた候補の改善結果と、最初の初期解の中で最良の配置です。未処理候補のcostまで評価する保証はありません。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

inline qap_result<long long> improve_best_qap_initial_candidate(
    const qap_problem<int, long long>& problem,
    const std::vector<std::vector<int>>& initial_candidates,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(!initial_candidates.empty());
    const int candidate_count =
        static_cast<int>(initial_candidates.size());
    qap_prepared<int, long long> prepared(problem);
    qap_result<long long> best{
        initial_candidates.front(),
        problem.evaluate(initial_candidates.front())
    };

    std::uint64_t current_seed = seed;
    for (int index = 0; index < candidate_count; ++index) {
        assert(problem.is_valid_permutation(initial_candidates[index]));
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        const int remaining_candidates = candidate_count - index;
        const auto phase_deadline =
            now + (deadline - now) / remaining_candidates;
        auto candidate = prepared.improve(
            initial_candidates[index], phase_deadline, current_seed);
        if (candidate.cost < best.cost) best = std::move(candidate);
        current_seed += 0x9e3779b97f4a7c15ULL;
    }
    return best;
}
```

候補数の上限や配分は外側の方針です。preparedはCSR等を共有しますが、開始costの再評価と各探索の内部状態構築は毎回必要です。

### 8.14 固定物体を保った部分repair

呼出しと返却値の説明は6.14、概念説明は4.14です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 構築済みの不変なQAP
// - current_location_of: 現在の完全置換
// - current_cost: このproblemで現在配置を評価した正しいlong long値
// - movable_objects: 重複のない可動物体番号。順序は任意
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。可動集合以外の位置は不変で、costは開始値以下です。返却値は縮約解ではなく完全配置です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

inline qap_result<long long> repair_affected_qap_objects(
    const qap_problem<int, long long>& problem,
    const std::vector<int>& current_location_of,
    long long current_cost,
    const std::vector<int>& movable_objects,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(problem.is_valid_permutation(current_location_of));
    return improve_qap_subset(
        problem,
        current_location_of,
        movable_objects,
        current_cost,
        deadline,
        seed);
}
```

空き位置も使わせるには、その位置のダミー物体を可動集合に入れます。k<2はそのまま返却、k==nは通常improveへ委譲します。costが不明なら6.0のcost省略版を使えます。

### 8.15 可動集合を変えながら部分repairを反復する

呼出しと返却値の説明は6.15、概念説明は4.15です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 反復中に変更しないQAP
// - initial_location_of: 開始する完全置換。値渡し
// - initial_cost: 開始配置の正しいQAP cost
// - rounds: 0以上の反復上限
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。この例では省略不可
// - select_movable: (const vector<int>&現在配置,int round)->vector<int>。範囲内で重複なしの可動集合を返す
// 出力: qap_result<long long>。各repairで非悪化を維持した、最後の完全配置です。rounds=0なら開始値を返します。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

template <class SelectMovable>
inline qap_result<long long> run_iterated_qap_subset_repair(
    const qap_problem<int, long long>& problem,
    std::vector<int> initial_location_of,
    long long initial_cost,
    int rounds,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed,
    SelectMovable select_movable) {
    assert(problem.is_valid_permutation(initial_location_of));
    assert(rounds >= 0);
    qap_result<long long> current{
        std::move(initial_location_of), initial_cost
    };
    std::uint64_t current_seed = seed;

    for (int round = 0; round < rounds; ++round) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        const int remaining_rounds = rounds - round;
        const auto phase_deadline =
            now + (deadline - now) / remaining_rounds;
        std::vector<int> movable =
            select_movable(current.location_of, round);
        current = improve_qap_subset(
            problem,
            current.location_of,
            movable,
            current.cost,
            phase_deadline,
            current_seed);
        current_seed += 0x9e3779b97f4a7c15ULL;
    }
    return current;
}

inline std::vector<int> choose_first_k_objects_for_qap_repair(
    const std::vector<int>& location_of,
    int round,
    int k) {
    const int n = static_cast<int>(location_of.size());
    assert(round >= 0);
    k = std::clamp(k, 0, n);
    std::vector<int> movable;
    movable.reserve(k);
    for (int offset = 0; offset < k; ++offset) {
        movable.push_back((round % n + offset) % n);
    }
    return movable;
}
```

seedは例の関数では必須引数です。selectorの処理時間も外側の締切に含まれます。kやroundsは問題分布で測り、短すぎるrepairの構築費に注意します。同じ番号の例に単純な巡回集合selectorも同梱します。

### 8.16 小部分を厳密列挙でrepairする

呼出しと返却値の説明は6.16、概念説明は4.16です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 構築済みの不変なQAP
// - current_location_of: 現在の完全置換
// - current_cost: 現在配置の正しいQAP cost
// - movable_objects: 重複なしの可動物体。例では個数k<=10を要求
// 出力: qap_result<long long>。指定位置集合内の厳密な部分最適解で、外部固定条件と非悪化を満たします。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

inline qap_result<long long> exact_repair_small_qap_subset(
    const qap_problem<int, long long>& problem,
    const std::vector<int>& current_location_of,
    long long current_cost,
    const std::vector<int>& movable_objects) {
    assert(std::ssize(movable_objects) <= 10);
    auto subproblem = make_qap_subproblem(
        problem, current_location_of, movable_objects, current_cost);

    std::vector<int> permutation = subproblem.initial_location_of;
    qap_result<long long> best{
        permutation, subproblem.problem.evaluate(permutation)
    };
    while (std::next_permutation(
        permutation.begin(), permutation.end())) {
        const long long cost = subproblem.problem.evaluate(permutation);
        if (cost < best.cost) {
            best.location_of = permutation;
            best.cost = cost;
        }
    }

    return subproblem.restore(best);
}
```

この関数にはdeadlineもseedもありません。全列挙を完了するまで戻らず、k=10でも速いとは限りません。時間に収まるkへ利用側で制限してください。

### 8.17 QAP差分を独自の局所探索へ組み込む

呼出しと返却値の説明は6.17、概念説明は4.17です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 探索中に不変のQAP
// - initial_location_of: 完全置換。値渡し
// - initial_extra_cost: 開始時の追加目的値。QAP costを含めない
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。この例では省略不可
// - extra_delta: (int i,int j,const vector<int>&交換前配置)->long long。追加費用の交換後−交換前を返す
// - apply_extra: (int i,int j,const vector<int>&交換後配置)->void。採用時だけ追加状態を更新する
// 出力: qap_combined_search_result。location_of、qap_cost、extra_costを返します。保証する非悪化は2つのcostの合計です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_combined_search_result {
    std::vector<int> location_of;
    long long qap_cost;
    long long extra_cost;
};

template <class ExtraDelta, class ApplyExtra>
inline qap_combined_search_result improve_with_qap_swap_state(
    const qap_problem<int, long long>& problem,
    std::vector<int> initial_location_of,
    long long initial_extra_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed,
    ExtraDelta extra_delta,
    ApplyExtra apply_extra) {
    assert(problem.is_valid_permutation(initial_location_of));
    qap_prepared<int, long long> prepared(problem);
    qap_swap_state<int, long long> state(
        prepared, std::move(initial_location_of));
    long long extra_cost = initial_extra_cost;
    if (problem.n < 2) {
        return {state.location_of(), state.cost(), extra_cost};
    }

    std::mt19937_64 random(seed);
    std::uniform_int_distribution<int> pick(0, problem.n - 1);
    while (std::chrono::steady_clock::now() < deadline) {
        const int i = pick(random);
        const int j = pick(random);
        if (i == j) continue;

        const long long qap_delta = state.swap_delta(i, j);
        const long long other_delta =
            extra_delta(i, j, state.location_of());
        if (qap_delta + other_delta >= 0) continue;

        state.apply_swap(i, j);
        apply_extra(i, j, state.location_of());
        extra_cost += other_delta;
    }
    return {state.location_of(), state.cost(), extra_cost};
}
```

seedは必須です。extra_deltaは状態を変更せず、apply_extraは必ず交換後の配置に同期させてください。apply_swapは差分を再計算します。この例は単純な山登りで、QAP cost単独が悪化する手も合計が良ければ受理します。

### 8.18 QAPを代理評価として使い、本来スコアで採否する

呼出しと返却値の説明は6.18、概念説明は4.18です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 代理目的を表す不変のQAP
// - current_location_of: 現在の完全置換。値渡し
// - current_qap_cost: 現在配置の正しいQAP cost
// - current_true_score: 現在配置の正しい本来スコア。この例では小さいほど良い
// - movable_objects: 重複なしの可動物体
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。この例では省略不可
// - true_score: (const vector<int>&配置)->long long。本来の最小化目的値を返す
// 出力: qap_true_score_result。location_of、qap_cost、true_score、acceptedを返します。棄却時は元の3値を保ちaccepted=falseです。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_true_score_result {
    std::vector<int> location_of;
    long long qap_cost;
    long long true_score;
    bool accepted;
};

template <class TrueScore>
inline qap_true_score_result repair_qap_if_true_score_improves(
    const qap_problem<int, long long>& problem,
    std::vector<int> current_location_of,
    long long current_qap_cost,
    long long current_true_score,
    const std::vector<int>& movable_objects,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed,
    TrueScore true_score) {
    auto candidate = improve_qap_subset(
        problem,
        current_location_of,
        movable_objects,
        current_qap_cost,
        deadline,
        seed);
    const long long candidate_true_score =
        true_score(candidate.location_of);
    if (candidate_true_score < current_true_score) {
        return {std::move(candidate.location_of),
                candidate.cost, candidate_true_score, true};
    }
    return {std::move(current_location_of),
            current_qap_cost, current_true_score, false};
}
```

seedは必須です。本来スコアの再評価時間も確保します。最大化目的なら比較方向を変更する必要があります。ハード制約がある場合は、本評価時に実行可能性も確認してください。

### 8.19 入力行列が少し変わるターン制問題

呼出しと返却値の説明は6.19、概念説明は4.19です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - problem: 変更前のqap_problem<int,long long>。コピーに変更を適用する
// - previous_location_of: 同じnと番号体系の前回完全配置
// - flow_updates: (row,column,value)の列。物体間行列の新しい値
// - distance_updates: (row,column,value)の列。位置間行列の新しい値
// - unary_updates: (object,location,value)の列。単項費用の新しいlong long値
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_result<long long>。更新後の目的関数で前回配置を評価した値以下です。呼出し元problemは変更されません。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_weight_update {
    int row;       // flowなら物体番号、distanceなら位置番号
    int column;    // flowなら物体番号、distanceなら位置番号
    int value;     // 加算量ではなく、変更後の値
};

struct qap_unary_update {
    int object;    // 物体番号
    int location;  // 位置番号
    long long value;  // 変更後の単項費用
};

inline qap_result<long long> reoptimize_qap_after_updates(
    qap_problem<int, long long> problem,
    const std::vector<int>& previous_location_of,
    const std::vector<qap_weight_update>& flow_updates,
    const std::vector<qap_weight_update>& distance_updates,
    const std::vector<qap_unary_update>& unary_updates,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    const int n = problem.n;
    assert(problem.is_valid_permutation(previous_location_of));
    for (const auto& change : flow_updates) {
        assert(0 <= change.row && change.row < n);
        assert(0 <= change.column && change.column < n);
        problem.flow[change.row * n + change.column] = change.value;
    }
    for (const auto& change : distance_updates) {
        assert(0 <= change.row && change.row < n);
        assert(0 <= change.column && change.column < n);
        problem.distance[change.row * n + change.column] = change.value;
    }
    if (!unary_updates.empty() && problem.unary.empty()) {
        problem.unary.assign(n * n, 0);
    }
    for (const auto& change : unary_updates) {
        assert(0 <= change.object && change.object < n);
        assert(0 <= change.location && change.location < n);
        problem.unary[change.object * n + change.location] = change.value;
    }
    // problemは今回入力のコピー。古いcostや古いpreparedを持ち込まない。
    return improve_qap(problem, previous_location_of, deadline, seed);
}
```

同じ要素が複数回出ると最後の指定が勝ちます。変更量を加算するAPIではありません。対称な辺は両方向を指定します。元の行列を次ターンへ持ち越すなら、呼出し元にも同じ更新を適用して管理します。

### 8.20 2つの重み付きグラフの対応付け

呼出しと返却値の説明は6.20、概念説明は4.20です。

```cpp
#include "qap_solver_v07.hpp"

// 入力:
// - n: 両グラフ共通の頂点数
// - graph_a: n*nのint行列。第1グラフの辺重み
// - graph_b: n*nのint行列。第2グラフの辺重み
// - assignment_cost: 空またはn*nのlong long。頂点iを頂点xへ対応させる費用
// - deadline: std::chrono::steady_clockの絶対締切。省略不可。入力構築やコピーの前に設定する
// - seed: std::uint64_tの乱数seed。既定値1
// 出力: qap_graph_matching_result。vertex_in_b[i]が第1グラフの頂点iに対応する第2グラフの頂点、costが差の二乗和＋単項費用です。
// 前提: 行列サイズ・添字計算はint、各値と全中間演算は宣言した型に収まること。

struct qap_graph_matching_result {
    std::vector<int> vertex_in_b;  // 第1グラフの頂点i→第2グラフの頂点
    long long cost;  // 全向き付きペアの二乗誤差＋単項費用
};

inline qap_graph_matching_result match_weighted_graphs_by_qap(
    int n,
    const std::vector<int>& graph_a,
    const std::vector<int>& graph_b,
    const std::vector<long long>& assignment_cost,
    std::chrono::steady_clock::time_point deadline,
    std::uint64_t seed = 1) {
    assert(n >= 0);
    assert(std::ssize(graph_a) == n * n);
    assert(std::ssize(graph_b) == n * n);
    assert(assignment_cost.empty() ||
           std::ssize(assignment_cost) == n * n);
    qap_problem<long long, long long> problem(n);
    problem.unary = assignment_cost;
    long long constant = 0;
    for (int index = 0; index < n * n; ++index) {
        const long long a = graph_a[index];
        const long long b = graph_b[index];
        problem.flow[index] = -2LL * a;
        problem.distance[index] = b;
        constant += a * a + b * b;
    }
    auto result = solve_qap(problem, deadline, seed);
    // 全頂点を一対一に置換するので、sum graph_b[p[i],p[j]]^2は定数。
    return {std::move(result.location_of), constant + result.cost};
}
```

全頂点の一対一対応だから、第2グラフの二乗和が置換に依存しません。異なる頂点数や部分対応にはそのまま使えません。対称グラフの非対角差は両方向で数えます。定数項と負の交差項もlong longに収めます。

## 9. 実装

### 9.1 全体の流れ

内蔵探索は、初期配置、Threshold Accepting、tabu探索を組み合わせます。動かす近傍は「2物体の位置を交換するswap」です。

1. `solve_qap` なら初期配置を生成する。`improve_qap` なら利用側の初期配置を使う。
2. 開始配置を全評価し、最良解として保存する。物体数が2未満、または締切到達なら返す。
3. 通常APIは評価器を準備する。prepared APIは構築済みの評価器を使う。
4. 探索ドライバへ入った時点の残り時間を基準に、最初の1/4までThreshold Acceptingを行う。
5. それまでの最良配置から、残り時間の基準の1/2まで2回目のThreshold Acceptingを行う。
6. 現在の最良配置でswap差分の計算時間を標本計測し、全swap差分表を作る余裕があるか推定する。
7. 余裕があると判断したら差分表を作り、tabu探索する。余裕がないと判断したらThreshold Acceptingを継続する。
8. 探索中に保存した最良配置と目的値を返す。

「最初の1/4」は呼出し前の全時間予算の1/4ではなく、初期評価・必要な前処理を終えて探索ドライバへ入った時点からの配分です。各フェーズの標本化にも、そのフェーズの時間を使います。

問題を表す `qap_problem` と結果の `qap_result` は公開です。前処理・乱数・差分表・探索処理は `qap_prepared` の非公開実装が保持します。通常の入口とpreparedの入口は、共通の非公開探索ドライバを使います。

### 9.2 初期配置

`solve_qap` と `prepared.solve` は、次の2配置を作って全目的値を比較します。

- seed付きのランダム置換。末尾から順に、選んだ位置と交換するFisher–Yates型の生成。
- 関係量の大きい物体を、他の位置への係数和が小さい位置へ対応させた配置。

2番目では、物体ごとにflowの入出絶対値和、位置ごとにdistanceの入出和を求めます。

$$s_i=\sum_{j=0}^{n-1}(\lvert F_{i,j}\rvert+\lvert F_{j,i}\rvert)$$

$$c_x=\sum_{y=0}^{n-1}(D_{x,y}+D_{y,x})$$

$s_i$ は物体 $i$ の関係量、$c_x$ は位置 $x$ の係数和です。$s_i$ の大きい順の物体と、$c_x$ の小さい順の位置を対応させます。同値の場合は番号で順序が決まります。行を連続走査し、行側の和と列側の和を同時に集計します。

この構成だけではunaryを見ませんが、2配置を比べる全評価にはunaryも含みます。構成解が厳密に良いときだけそちらを使い、同点ならランダム置換を使います。負値や一般の位置係数にも計算自体はできますが、「中央ほど有利」という解釈が常に成り立つわけではありません。

### 9.3 対称性判定と疎なflowのCSR

評価器は、flowとdistanceの両方について、全対角が0で行列が対称かを調べます。この条件がそろったときだけ対称用の式を選びます。unaryには対称性条件を課しません。

またflowの非ゼロ数を $m$ として、$3m<n^2$ のときだけ疎な経路を準備します。ちょうど1/3では疎経路になりません。判定は値の厳密な0を使い、distanceの疎密は使いません。

CSRは、各物体に関係する非ゼロ辺を連続した配列へまとめた形式です。例えば物体rから出る辺は、`out_offset[r]` 以上 `out_offset[r+1]` 未満の範囲に並びます。

1. flowを行方向に1回走査して、出辺の頂点と重みを格納する。同時に各頂点の入次数を数える。
2. 入次数の累積和を作り、各頂点の入辺を置く配列区間を決める。
3. 出辺配列の非ゼロ辺だけを読み、入辺配列へ転置する。

入辺の格納のために密行列をもう1回読む必要はありません。ただし、非ゼロ数の集計や対称性判定もあるため、prepared全体が密行列を1回しか読まないという意味ではありません。

### 9.4 全評価と1回のswap差分

`evaluate` は目的式をそのまま行優先で計算します。疎CSRを使った全評価ではなく、常に全物体ペアを走査します。

swapする物体を $r,s$、交換前の位置を $a=p_r$、$b=p_s$ とします。目的値のうち変わるのは、rまたはsに関係する項だけです。差分は「交換後−交換前」で、次の3部分に分けられます。

単項費用の変化:

$$\Delta_U=U_{r,b}-U_{r,a}+U_{s,a}-U_{s,b}$$

ほかの物体 $k$ との関係の変化:

$$A_k=(F_{r,k}-F_{s,k})(D_{b,p_k}-D_{a,p_k})$$

$$B_k=(F_{k,r}-F_{k,s})(D_{p_k,b}-D_{p_k,a})$$

$$\Delta_{\mathrm{ext}}=\sum_{k\ne r,s}(A_k+B_k)$$

$A_k$ はr・sからkへ出る関係、$B_k$ はkからr・sへ入る関係の変化です。

r・s自身と、その間の関係の変化:

$$\Delta_{\mathrm{pair}}=(F_{r,r}-F_{s,s})(D_{b,b}-D_{a,a})+(F_{r,s}-F_{s,r})(D_{b,a}-D_{a,b})$$

$$\Delta=\Delta_U+\Delta_{\mathrm{ext}}+\Delta_{\mathrm{pair}}$$

これが `qap_problem::swap_delta` の一般式です。rとsを除く走査は、2つの番号の前・間・後の3区間に分けます。各反復で「kがrまたはsか」を調べる分岐を避け、必要な行の先頭ポインタも再利用します。

両行列が対称かつ対角0なら、次の式へ簡約できます。

$$\Delta=\Delta_U+2\sum_{k\ne r,s}(F_{r,k}-F_{s,k})(D_{b,p_k}-D_{a,p_k})$$

疎経路ではflowが非ゼロの項だけを調べます。対称ならr・sの出辺を走査して2倍し、一般形なら出辺と入辺を別々に走査します。r・s自身とその間の項は、重複しないよう分けて加算します。

公開の `problem.swap_delta` は一般式です。自動判定済みの対称・疎な経路を使いたいときは `prepared.swap_delta` またはstateを使います。

### 9.5 Threshold Accepting

Threshold Acceptingは、差分がある閾値以下ならswapを受け入れる探索です。小さな悪化手も一時的に受け入れられるため、改善手だけでは動けない配置から抜ける機会があります。

各フェーズの開始時に最大256個の異なる2物体swapをランダムに標本化します。ここで「異なる」は1回のswapの2物体が異なるという意味で、標本間で同じペアが出ることはあります。正の差分だけを集め、その上側中央値を尺度に使います。正の差分がない場合の尺度は1です。

$m_+$ を正の差分の上側中央値、正の標本がなければ1とします。

$$T_0=0.1171875\cdot\max(1,m_+)$$

$T_0$ は初期閾値です。標本化を終えた時刻から締切までの進捗率を $q$ として、閾値を線形に下げます。

$$T(q)=T_0(1-q)$$

差分が閾値以下なら現在配置を交換し、現在costへ差分を加算します。その結果が保存済み最良costより小さければ、最良配置もコピーして保存します。受理した現在配置と返却用の最良配置は別です。

標本化中は32試行ごと、探索中は256反復ごとに時刻を確認します。閾値の更新もこの時刻確認に合わせるため、実際の閾値は反復ごとに連続的に変わるわけではありません。閾値の比較にはlong doubleを使いますが、整数目的値・差分の計算型はScoreです。

### 9.6 差分表の構築と更新

tabu探索では、すべての組 `i<j` のswap差分を表へ保持します。各手を選ぶときに差分をすべて計算し直すのを避けるためです。領域はn*n個を確保し、上三角の要素を使います。

表を作る前に、ペア数を $P=n(n-1)/2$ とし、$\min(P,\max(16,n))$ 個の差分を計測します。これを全Pペア分へ比例拡大した構築時間の推定値が、残り時間の半分未満のときだけ構築へ進みます。計測結果が最適化で消されないよう、計算値をchecksumへ反映しています。

実際の構築では4行ごとに締切を確認します。構築途中で中断して表が無効になった場合は、それまでの最良解を返します。この場合にThreshold Acceptingを再開するわけではありません。Threshold Acceptingへ進むのは、構築前の余裕判定がfalseだった場合です。

表を使ってr・sを交換した後は、表全体を更新します。

1. 適用した差分を現在costへ加え、配置を交換する。
2. 交換対象へのflow・distanceの係数差を、最大4n個のScore作業領域へまとめる。
3. r・sをどちらも含まない各swapの差分は、変化した相互作用だけを定数時間で補正する。表全体では $O(n^2)$。
4. rまたはsを含むswapは、片方を通常の差分式で再計算し、もう片方を3物体の恒等式から復元する。
5. r・sをもう1回交換する差分は、適用した差分の符号反転にする。

4の恒等式を具体的に書きます。交換前の差分を $\delta_{u,v}$、r・s交換後の差分を $\delta'_{u,v}$、3物体の交換前位置を $a=p_r$、$b=p_s$、$c=p_i$ とすると、次が成り立ちます。

$$G_F=F_{r,s}-F_{s,r}+F_{s,i}-F_{i,s}+F_{i,r}-F_{r,i}$$

$$G_D=D_{a,b}-D_{b,a}+D_{b,c}-D_{c,b}+D_{c,a}-D_{a,c}$$

$$\delta'_{r,i}+\delta'_{s,i}=\delta_{r,i}+\delta_{s,i}-\delta_{r,s}+G_FG_D$$

$G_F$ と $G_D$ は3頂点の有向サイクルに沿った差です。3物体の6通りの置き方の交代和では、単項費用、自己対角項、ほかの物体との相互作用が相殺され、この積だけが残ります。flowまたはdistanceが対称なら対応するGが0になります。非対称の場合も積の補正を含めるため、一般の入力で使えます。

この差分表は内蔵探索用の非公開状態です。`qap_swap_state` がn*nの表を公開保持しているわけではありません。

### 9.7 tabu探索

各反復で表の全swapを走査し、許可された手のうち最も差分が小さい手を選びます。同じ差分なら走査順で先に見つけた手を維持します。改善手がない場合でも、許可された悪化手を選ぶことがあります。

禁止表は物体×位置の形です。ある物体を元の位置へすぐ戻すことを避けるため、交換前の2配置について禁止期限を設定します。候補swapの両方の移動先が禁止期限を過ぎていれば通常の許可手です。

例外として、そのswapで保存済み最良costを厳密に更新できる場合は、禁止中でも許可します。これをaspiration条件と呼びます。

禁止期限へ加える値tenureは、次の整数範囲からランダムに選びます。

$$\mathrm{low}=\max(1,\lfloor n/10\rfloor),\qquad \mathrm{high}=\max(\mathrm{low},\lfloor 3n/10\rfloor)$$

現在のiterationにtenureを加えた値を禁止期限とし、その後iterationを1増やします。許可判定は `tabu_until<=iteration` です。小さいnでtenure=1なら、次反復にはその期限へ到達します。

1回前の表更新に要した時間も測り、次の適用が締切に近すぎる場合は手を適用せず終了します。具体的には前回適用時間の1.25倍を目安に使います。ただし処理時間が毎回同じとは限らないため、強制的な締切保証にはなりません。

### 9.8 部分QAPの縮約と復元

可動物体を利用側が渡した順に $m_0,\ldots,m_{k-1}$、その現在位置を $l_a=p_{m_a}$ とします。縮約の物体番号aは元物体 $m_a$、縮約位置番号xは元位置 $l_x$ に対応します。可動集合の順序をソートすることはありません。

縮約行列は次の式です。$I$ は固定物体の集合です。

$$F'_{a,b}=F_{m_a,m_b},\qquad D'_{x,y}=D_{l_x,l_y}$$

$$U'_{a,x}=U_{m_a,l_x}+\sum_{f\in I}(F_{m_a,f}D_{l_x,p_f}+F_{f,m_a}D_{p_f,l_x})$$

固定物体と可動物体の関係は、可動物体の行き先さえ分かれば決まるため、単項費用へ移せます。固定物体同士の費用は配置によらない定数です。

縮約の初期配置を恒等置換idとし、完全初期costから縮約初期costを引いて定数を求めます。

$$K=C(p)-C_{\mathrm{sub}}(\mathrm{id})$$

任意の縮約置換qを復元して得る完全配置を $p^{(q)}$ とすると、次の対応が成り立ちます。

$$p^{(q)}_{m_a}=l_{q_a},\qquad p^{(q)}_f=p_f\ (f\in I)$$

$$C(p^{(q)})=K+C_{\mathrm{sub}}(q)$$

$K$ が `constant_offset` です。既知cost版では式の $C(p)$ に渡された値をそのまま使うため、誤った値を渡すとこの対応が壊れます。

実装は固定物体を外側から処理し、関係する可動物体があるときだけ、その固定位置とk個の候補位置の距離を作業配列へ展開します。同じ距離を複数の可動物体で共有します。縮約unaryがすべて0なら空vectorにします。

`improve_qap_subset` はこの縮約を作り、縮約problemにpreparedを構築して改善し、restoreします。k<2とk==nの分岐は6.0のとおりです。`make_qap_subproblem` 自体には時間制限引数がなく、k==nでも縮約データを構築します。

### 9.9 乱数と内部パラメータ

乱数生成器は64bit状態を持ち、左7bit・右9bitのxor shiftを行います。seed=0のときは非ゼロの固定初期状態を使います。範囲内の整数は、128bitの積の上位64bitを利用して作ります。

フェーズごとにseedへ固定定数を加える、またはxorして、異なる列を使います。乱数生成器の状態は呼出し間で公開保持されません。

| 内部値 | 設定 | 役割 |
| --- | --- | --- |
| 初期配置候補 | ランダム置換と関係量による構成配置 | 開始点を比較する |
| TAの標本数 | 最大256試行 | 正の差分の尺度を求める |
| TA初期閾値係数 | 0.1171875 | 尺度から初期許容悪化量を決める |
| 前半2フェーズの締切 | 探索開始から残り時間の1/4、1/2 | 最良配置から閾値を再設定する |
| 疎経路 | flowの非ゼロ率が厳密に1/3未満 | 非ゼロ辺だけの差分を使う |
| 表構築判定 | 推定構築時間の2倍が残り時間未満 | 構築だけで時間を使い切るのを避ける |
| tabu期限への加算値 | 9.7のlow〜high | 元の配置への復帰を抑える |
| 次の表更新の時間余裕 | 前回適用時間の1.25倍 | 締切直前の適用を控える |

これらを指定する公開パラメータはありません。用途ごとの切替フラグを増やす必要もなく、入力の数学的性質と残り時間から内蔵経路を選びます。

### 9.10 計算量とメモリ

ここではnを全体サイズ、kを可動数、mをflowの非ゼロ数、dを交換する2物体の関係する入出次数の和とします。入力検査のassertを無効にした通常の計測条件を基準にした上界です。

| 処理 | 時間 | 主な追加メモリ |
| --- | --- | --- |
| 問題行列 | 入力構築による。通常 $O(n^2)$ | $O(n^2)$ |
| 配置の妥当性検査 | $O(n)$ | $O(n)$ |
| 全評価 | $O(n^2)$ | 結果計算自体は $O(1)$ |
| 公開の一般swap差分 | $O(n)$ | $O(1)$ |
| prepared構築 | $O(n^2)$ | 疎CSRは $O(n+m)$ |
| preparedの密swap差分 | $O(n)$ | $O(1)$ |
| preparedの疎swap差分 | $O(1+d)$ | $O(1)$ |
| state構築・cost省略 | $O(n^2)$ | 配置 $O(n)$ |
| state構築・既知cost | 配置のコピー込み $O(n)$ | 配置 $O(n)$ |
| stateのapply_swap | 利用する差分経路と同じ | $O(1)$ |
| 全swap差分表の構築 | 密な一般経路で $O(n^3)$ | $O(n^2)$ |
| 表の1回更新・tabuの1手選択 | 各 $O(n^2)$ | 表に加え更新作業領域 $O(n)$ |
| 既知costからの縮約 | $O(n+k^2n)$ | $O(n+k^2)$ |
| cost省略の縮約 | $O(n^2+k^2n)$ | $O(n+k^2)$ |
| 縮約解の復元 | $O(n+k)$ | 完全配置 $O(n)$ |

stateへ配置をmoveし、入力検査も無効なら、既知cost版構築の配置移動自体は定数時間です。debug時は置換検査が追加されるため、特に一般差分の検査用メモリ・時間は表と異なります。

探索全体の反復回数はdeadlineで決まるため、nだけから総実行時間を固定できません。また疎なflowでも、公開の問題行列と内蔵tabu表は密な領域を持つため、全体を $O(m)$ メモリで使えるライブラリではありません。

### 9.11 検証用コードの使い方

ライブラリヘッダーを直接コンパイルすると、末尾の自己テスト10群とmainが有効になります。includeした場合はそれらが無効になります。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -x c++ qap_solver_v07.hpp -o qap_selftest
./qap_selftest
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror qap_solver_examples_test_v04.cpp -o qap_examples
./qap_examples
g++ -std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wshadow -Wconversion -Werror qap_solver_benchmark_v08.cpp -o qap_bench
./qap_bench test
./qap_bench audit
```

外部テストには `qap_solver_test_access.hpp` を同じディレクトリへ置きます。コード例テストには `qap_solver_usecase_examples.hpp` も必要です。通常のアプリケーションには `qap_solver_v07.hpp` だけで足ります。ヘッダーの直接コンパイル時に出る `#pragma once in main file` という警告は想定内です。

コード例の検証は、全例を呼び出すだけでなく、返却配置の範囲・重複、独立した目的式の再計算、固定物体、ダミー復元、型の境界、入力更新後のcostを確認します。実際の検証結果とコマンドは配布一式の `qap_solver_guide_validation.md` にまとめています。
