# heuristic_steiner_tree — 利用ガイド

対象ソース：[heuristic_steiner_tree_v17.hpp](heuristic_steiner_tree_v17.hpp)。単一ヘッダで使う、非負整数費用の無向グラフ向け接続ライブラリである。本書は、このヘッダの公開APIと実際の動作を説明する。

初めて使う場合は1・2章で問題との適合性を確認し、4章から目的に合うユースケースを選ぶ。4・6・8章の **U01〜U18** は対応している。5章は設定一覧、7章は制約、9章は実装の説明である。

## 1. 何を解くライブラリか？

### 1.1 必要な地点だけを、途中の道を共有してつなぐ

道路候補の中から、倉庫・店舗・中継施設を結ぶ道を選ぶ場面を考える。全ての交差点を接続する必要はない。接続が必須の地点だけが同じネットワークに入り、途中の交差点を自由に使えればよい。複数の地点へ向かう道が一部を共有すると、その道の建設費は一度だけ支払う。

この「必須地点を全て接続する、費用の小さい辺集合」を求める問題を、グラフ上の **Steiner tree問題** と呼ぶ。本書では必須地点を **terminal**、経由地として使う必須ではない頂点を **Steiner頂点** と呼ぶ。供給源・本部・出発点も接続必須ならterminalに含める。無向の接続問題なので、供給源へ向かう方向自体は区別しない。

### 1.2 入力と目的関数

入力は無向グラフ、各辺の非負費用、terminal集合である。

$$\min_{F\subseteq E}\ C(F)=\sum_{e\in F}c_e\quad\text{subject to all vertices in }T\text{ being connected in }(V,F)$$

| 記号 | 意味 |
|---|---|
| $G=(V,E)$ | 入力の無向グラフ |
| $V$ | 地点・交差点・中継点などの頂点集合 |
| $E$ | 利用可能な辺の集合。多重辺はそれぞれ別の辺として扱う |
| $T\subseteq V$ | 接続が必須のterminal集合 |
| $F\subseteq E$ | 選ぶ辺集合。これが求める解 |
| $e$ | 辺1本を表す添字 |
| $c_e\geq0$ | 辺 $e$ を選んだときの費用 |
| $C(F)$ | 選択した全辺の費用合計 |

制約は「全terminalが相互に行き来できること」を意味する。terminal以外の頂点の使用は任意である。目的は総費用の最小化なので、コストは小さいほどよい。各terminalまでの距離を足す目的関数ではなく、使う辺に一度ずつ課金する目的関数である。

通常の成功結果は、terminalを接続する木の辺IDと総費用である。terminalが0個または1個なら、辺なし・費用0で条件を満たす。全terminalを接続できなければ失敗を返す。本ライブラリが返す解について、最小費用であることは保証しない。

### 1.3 撤去できない既設ネットワークへの追加

既設辺を残し、その建設費を支払い済みとして扱う問題も解ける。この場合の目的は追加費用の最小化になる。

$$\min_{A\subseteq E\setminus B}\ C_{\mathrm{add}}(A)=\sum_{e\in A}c_e\quad\text{subject to all vertices in }T\text{ being connected in }(V,B\cup A)$$

| 追加の記号 | 意味 |
|---|---|
| $B\subseteq E$ | 撤去不能な既設辺集合。辺の入力費用にかかわらず今回の課金は0 |
| $A\subseteq E\setminus B$ | 新たに追加する辺集合 |
| $C_{\mathrm{add}}(A)$ | 今回追加する辺だけの費用合計 |
| $B\cup A$ | 利用者が保持する実際のネットワーク |

全ての既設辺を残すため、実際のネットワークには閉路や、今回のterminalとは無関係な成分が残ることもある。既設成分同士まで必ずつなぎたい場合は、それぞれの代表頂点をterminalへ追加する。

## 2. 厳密解アルゴリズム

最適性が必要な場合や問題が小さい場合は、以下を先に検討する。ここで挙げる厳密解法は、このヘッダの公開solverに切り替えモードとして実装されているものではない。**厳密解法を完走できれば、その最適費用は本ライブラリの実行可能解の費用以下になる。常に差が出るわけではない。**

以下では $N=|V|$、$M=|E|$、$K=|T|$ とする。

| 条件 | 厳密解法と利用判断 |
|---|---|
| $K\leq1$ | 空の辺集合が最適。本ライブラリもこの結果を返す |
| $K=2$ | 2点間の最短路で十分。非負辺ならDijkstraで解ける |
| $K=3$ | 3terminalからの最短距離を求め、全頂点でその距離和を最小化する。最小点への3経路の和集合から最適な接続を得られる |
| 入力グラフが木または森 | 全terminalが同じ成分にあることを確認し、terminalでない葉を除く。残った部分木が最適。重みが非負なので不要な枝を残す必要がない |
| 全頂点がterminal | 最小全域木（MST）問題になる。Kruskalなどで厳密に解ける |
| $K$ が小さい | terminal部分集合DP。頂点とterminal部分集合を状態とし、部分集合の結合と最短路で最適費用を計算する |
| terminal以外の候補頂点が少ない | 採用する非terminal頂点の部分集合を全列挙し、terminalと合わせた誘導部分グラフのMSTを比較できる。頂点選択を全列挙するので指数時間 |
| 利用可能な辺が非常に少ない | 辺部分集合の全列挙と連結性検査。単純だが $2^M$ 個の候補があるので小さな検算向け |
| 幅の小さい木分解が得られる | 木幅をパラメータとするDP。幅が小さい場合に有力だが、分解の用意と実装が必要 |
| 制限がなくても、十分な計算時間を使える | 数理最適化・分枝限定・削減を組み合わせた専用厳密solverを検討する |

terminal部分集合DPの代表的な計算量は $O(3^K N+2^K(M+N\log N))$、主要な表は $O(2^K N)$ である。指数部分のため、頂点数だけでなくterminal数が判断を左右する。例えば単に「terminalが少ない」と判断する前に、$2^K N$ 個の費用と復元情報がメモリに入るかを計算するとよい。部分集合DP、Dijkstraとの組み合わせ、木幅による解法は [Hougardy・Silvanus・Vygen, Dijkstra meets Steiner](https://arxiv.org/abs/1406.0492) に説明されている。

専用solverの例は [SCIP-Jack](https://scipjack.zib.de/) である。大きな問題でも最適性を証明できる場合があるが、全ての入力で短時間に完了する保証はない。時間切れで得た暫定解と、最適性が証明された解は区別する。

「候補辺だけに絞った」「使用頂点の候補を固定した」という制限だけでは、Steiner tree問題が自動的にMST問題になるわけではない。本ライブラリの `normalize()` も、候補辺内のSteiner最適解を保証しない。全候補頂点を必ず接続する条件に変えたときにMSTが厳密解になる。また、探索範囲を狭めた部分問題の厳密解は、元の全グラフでの最適解を保証しない。

既設辺への増設では、既設辺を費用0としてその連結成分を縮約すると、通常のSteiner tree問題として厳密解法を適用できる。縮約後のterminal成分数が小さくなれば、部分集合DPが使いやすくなる。

## 3. 差分更新

### 3.1 何をそのまま更新できるか

ターンごとに小さな変更が入る使い方では、**solverインスタンスを保持すること**と、**前の解の辺IDを次の入力へ渡すこと**を分けて考える。前者は準備済みデータの再利用、後者は解の再利用である。

| ターン間の変更 | 呼び出し方・必要な作業 | 再利用できるもの |
|---|---|---|
| terminalの追加・削除・交換 | 新しいterminal集合で `repair(new_T, old_edges, options)`。旧解不要なら `solve(new_T, options)` | 同じグラフの作業領域。条件が合えば共通terminalの最短路表の行。`repair`なら旧辺集合もヒントにする |
| 解の一部を壊す | 残した辺を `repair` へ渡す | グラフの準備と、削除してよい部分解 |
| 連結済み解をさらに改善 | `improve(T, old_edges, options)` | 既存の実行可能解とグラフの準備 |
| 既設辺が増える | 次の `augment` に更新後のbase辺集合を渡す | 元グラフの準備。baseに依存する距離表は毎回作る |
| 既存頂点間に辺を追加 | `add_edge(u,v,w)` 後に再実行 | 既存の辺IDと旧解。CSR・辺順序・通常terminal cacheは無効化され、必要時に再構築 |
| seed・preset・時間予算・cache方針を変更 | 次の呼び出しの `options` を変更 | グラフを作り直す必要はない |
| 辺を削除・禁止、重みを変更 | 許される辺と新しい費用でsolverを再構築 | 外部IDを使えば、残っている旧解の辺を新IDへ移して `repair` できる |
| 頂点数・頂点番号を変更 | solverを再構築。terminalと辺端点も新しい番号へ変換 | 外部で対応を管理した旧解だけ。内部cacheは引き継がない |

`add_edge()` は最短路表を局所修正する動的最短路APIではない。辺追加による影響は全グラフに及び得るため、表を無効化する。重みを下げたい辺を新しい多重辺として追加しても元の辺は残り、重み変更や通行禁止の代替にはならない。

### 3.2 terminal cacheの再利用範囲

通常グラフ用の距離・経路表は、solverごとに直近1組を保持する。対象は `solve()`、`solve_cost()`、`improve()`、`repair()` のうち、実際に `terminal_cache` を選んでその処理まで到達した呼び出しである。

- terminal集合が同じなら、各terminalからの最短路を再計算しない。terminalの並びと重複は内部で正規化される。
- 集合が変わった場合は、共通terminalの行を再利用し、追加されたterminalの行だけを計算する。共有行の移動にはおおむね「共通terminal数×頂点数」の仕事があり、変更1個なら必ず定数時間になるわけではない。
- 今回 `on_demand` を使う場合や、表を使う前に時間切れになった場合は、保存済み表があっても使わない。
- `augment()` はbaseの費用を0とした別の距離を使うため、この永続cacheを使用・置換しない。baseが空でも同じ扱いである。
- `clear_terminal_cache()` で保持中の通常表を解放する。`memory_limit_bytes=0` や `on_demand` の指定だけでは、保持済みメモリを解放しない。

新旧の表を明示的に2つ持たず、同じ配列内で共通行を移動する。ただし `std::vector` のcapacityや拡張時の再確保があるため、指定メモリ量が実使用量の上限になるわけではない。

### 3.3 ターン制での扱い

毎ターン同じsolverを使い、成功した `Result` を呼び出し元で保存する。要求terminalが変わるならU05、既設辺を必ず残すならU06・U07を使う。`repair` の旧辺は削除できるヒントなので、建設済み資産の保存には使わない。

探索途中の乱数状態・elite候補・未処理の局所探索は、次の呼び出しへ継続しない。時間を増やして `improve` を再度呼ぶことはできるが、中断地点からの再開ではない。同一インスタンスの内部作業領域を更新するため、並行呼び出しは避ける。

## 4. ユースケース

### U01 必須拠点をゼロから接続する

配送拠点間の道路、設備間のケーブル、重要地点を結ぶ通路など、複数の目的地が共通の道を使う問題。全ての中継地点を接続する必要がない点が、全域木との違いになる。利用可能な地点・辺・辺費用と、必ずつなぐ地点を指定する。供給源がある場合は、それも必須地点に含める。

### U02 使用候補の範囲内で辺を整理する

複数の経路を重ねて作った設計案から、閉路や行き止まりを除きたい場面。外部で承認された候補辺だけを使い、候補にない新しい道を採用しないことが条件になる。候補辺と必須地点を指定する。候補だけで必要地点を接続できない場合は、この整理だけでは解決しない。

### U03 実行可能な外部解を改善する

別の構築法や人手で作った、既に必要地点を接続している設計を出発点にする。候補の範囲外も探索して、費用を下げたい場面である。全ての利用可能辺、必須地点、連結済みの候補を指定する。候補辺は撤去してよく、使用頂点や経路も変わり得る。

### U04 壊した部分解を修復する

LNS（大きな近傍を調べる探索）などで、現在解の高価な部分を外し、残した構造を参考に接続し直す場面。残す辺の候補と、引き続き必要な地点を指定する。残した辺は固定資産ではなく、別の安い構造が見つかれば取り除いてよいものとして扱う。

### U05 ターンごとに必要地点が変わる

需要の発生・終了、訪問対象の交換などで、前ターンと似た接続問題を繰り返す場面。今回の必須地点と前の解を指定する。前に必要だった地点を今回も必須にするかは利用者が決める。前の解は参考にするが、全辺を維持する義務はない。

### U06 既設網へ新しい需要点を追加接続する

撤去できない道路・配管・通信線があり、今回の追加費用を抑えて新しい地点を接続する場面。既設辺、新しい必須地点、必ず接続したい既設成分の代表地点（anchor）を指定する。例えば新規顧客が1点だけでも、既設網のanchorを加えれば、顧客と既設網の接続が要求される。

### U07 複数段階で不可逆に建設する

毎ターン建設した辺を次ターンから既設資産として使う問題。初期の既設網と、各段階で追加される需要を指定する。過去の需要も維持しながら、各段階の追加費用を計算する。将来の全需要を見越した一括最適化は別の問題であり、この使い方は各段階を順番に決める。

### U08 有料の必須辺を含む連結網を作る

契約・設備仕様などで、ある辺は費用を支払って必ず採用しなければならない場面。必須辺、その他の利用可能辺、必須地点を指定する。必須辺の全成分も同じネットワークに含めるなら、必須辺の両端を接続対象へ加える。必須辺の費用は固定費として一度だけ加算する。必須辺に閉路があれば、最終網にもその閉路を残す。

### U09 任意顧客を選ぶ問題の接続費を評価する

全顧客を接続する義務はなく、接続しない顧客にはペナルティを払うPrize-Collecting型の問題。必須拠点、顧客位置、未接続ペナルティ、外側で用意した採用顧客集合の候補を指定する。候補ごとに接続網を作り、接続費と未接続ペナルティの合計を比較する。途中の経路上に入った顧客もサービス済みと数えるモデルなら、その分のペナルティも除く。顧客集合の候補探索そのものは利用者側の仕事である。

### U10 共通の供給拠点を1つ選ぶ

倉庫・基地局・供給源の候補から1施設を開設して、共通の需要点を接続する場面。候補拠点、候補ごとの開設費、需要点を指定する。拠点ごとに接続費と開設費を比較する。選ばなかった候補地点を単なる中継点として通過してよいことを前提にする。

### U11 無料backboneの複数gatewayを利用する

別系統の通信幹線などによって、複数の出入口が既に相互接続され、どの出入口を使っても追加の幹線費が発生しない場面。需要点とgateway集合を指定する。複数gatewayを同時に利用できるため、U10の「施設を1つ選ぶ」とは異なる。物理的な追加辺だけでは森に見えても、backbone込みでは連結していればよい。

### U12 グループごとに代表地点を選ぶ

各地区から少なくとも1つの設備を接続したいが、地区内のどの設備にするかは選べる場面。各グループの候補頂点、グループごとの代表点の組合せ候補、別途必須の地点を指定する。代表点が決まった後の接続部分問題をこのライブラリに任せる。グループ選択の全組合せを内部で探索する機能ではない。

### U13 同じグラフで独立した問い合わせを反復する

施設配置の評価、需要シナリオの比較など、同じ辺と費用の上でterminal集合だけを多数回変える場面。グラフを一度用意し、問い合わせの集合列を渡す。近いterminal集合が続く場合は、距離表の共通部分を再利用できる。一つ前の辺集合を次の解に残す必要はない。

### U14 利用可能な辺を追加する

道路候補の発見、通行許可の追加などで、既存頂点の間に新しい辺が増える場面。追加する辺の両端と費用、今回の必須地点、任意で旧解を指定する。既存の辺は残り、既存辺IDも変わらない。新しい頂点を増やす操作はこのケースに含まれない。

### U15 通行禁止・費用変動・頂点変更へ対応する

障害発生で辺が消える、交通状況で費用が変わる、利用地点が追加される場面。今回利用できるグラフ全体を作り直し、外部で管理した安定した辺IDから、旧解に含まれていた利用可能辺を移し替える。静的な禁止辺・許可領域を最初に絞り込む使い方も同じ方法で扱える。

### U16 互いに独立した複数ネットワークを作る

異なる通信系統や別々の案件ごとに、それぞれの必須地点だけを接続する場面。案件別の必須地点集合を指定し、費用を案件ごとに加算する。同じ道路上に別々のケーブルを敷設してそれぞれ課金する、というモデルに向く。辺の共有割引や容量競合がある場合は、案件を独立に解く方法では全体を最適化できない。

### U17 障害物付き格子の重要マスをつなぐ

盤面上の通路建設や格子配線。壁の配置、上下左右のマス間に辺を作る費用、必須マスを指定する。課金対象は通過する回数や訪問マスではなく、選んだマス間の辺である。曲がり回数や1マス1回の開通費などは別のモデルになる。

### U18 座標付き候補点を線分でつなぐ

敷設可能な中継点を事前に列挙し、その間の候補線分からネットワークを作る場面。全候補点の座標、利用可能線分、必須点、距離を整数化する倍率を指定する。未登録の座標に新たな分岐点を置くことはできない。連続平面の幾何Steiner treeではなく、有限個の候補点と線分に離散化した問題を解く。

## 5. 利用準備

### 5.1 ヘッダと環境

`heuristic_steiner_tree_v17.hpp` をソースと同じディレクトリへ置き、`#include "heuristic_steiner_tree_v17.hpp"` とする。外部ライブラリへのリンクは不要で、AC Libraryやhash mapヘッダも必要ない。`bits/stdc++.h`、`__builtin_clzll`、`__INCLUDE_LEVEL__` を使うためGNU系環境を前提にする。本書のコード例はC++20で記述し、GCC 13.3.0で検証している。GCC 12.2での実機検証は行っていない。

8章の共通定義に続いて必要な例と呼び出し用 `main()` を `example.cpp` に置いた場合、例えば次でビルドする。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Werror example.cpp -o example
```

### 5.2 グラフの構築

| API・引数 | 型・既定値 | 設定方法・意味 |
|---|---|---|
| `heuristic_steiner_tree(n)` | `int n`、既定値なし | `n>=0`。頂点番号は `0..n-1`。構築時には辺を持たない。頂点数は後から変えられない |
| `add_edge(u,v,w)` の `u`,`v` | `int`、必須 | 無向辺の両端。`0<=u,v<n` |
| 同 `w` | `long long`、必須 | 非負の整数費用。距離・費用和の安全範囲は7章参照 |
| `add_edge` の戻り値 | `int` | 追加順の0始まり辺ID。入力順に呼べば元の辺配列の添字になる |
| `edge_count()` | 引数なし、`int` を返す | 現在の登録辺数 |
| `edge(id)` | `int id`、必須 | `const edge_type&` を返す。`edge_type` は `from:int`、`to:int`、`cost:long long` を持つ |

明示的な `build()` や `prepare()` はない。初めて必要になった際に内部の探索用構造を準備する。後から `add_edge()` した場合も同様である。terminal集合はコンストラクタに固定せず、各問い合わせへ渡す。

### 5.3 全optionsと値の選び方

`heuristic_steiner_tree_options options;` で以下の既定値になる。各APIに渡す `options` 自体を省略しても同じ値を使う。

| フィールド | 型 | 既定値 | 意味と選び方 |
|---|---|---|---|
| `preset` | `heuristic_steiner_tree_preset` | `balanced` | `fast` は短い探索、`balanced` は速度と品質の折衷、`quality` は候補数と局所探索を多くする。まず `balanced` を使い、自分の入力と時間予算で他の値を比較する |
| `path_mode` | `heuristic_steiner_tree_path_mode` | `auto_select` | `auto_select` は表の構築費を見積もる。`terminal_cache` はterminalから全頂点への距離・親辺表を希望、`on_demand` は必要な探索をその都度行う。反復問い合わせで表が十分小さいなら明示cacheも比較する |
| `seed` | `std::uint64_t` | `0` | `solve`・`augment` の候補生成の乱数seed。再現実験では固定する。複数seedの結果を比較する場合は、費用が小さい実行可能解を呼び出し元で保持する |
| `time_limit_ms` | `int` | `0` | 正値は1呼び出しの相対時間目標、単位ms。0は相対制限なし。負値も内部では相対制限なしだが、指定には0を使う。短い外側探索なら1回に配る時間を決める |
| `restart_limit` | `int` | `-1` | 負値は自動、0は追加restartなし、正値は追加restart回数の上限。最初の候補生成や局所探索は別枠。再現性を重視する比較では、時間無制限で回数を明示できる |
| `memory_limit_bytes` | `size_t` | `256ULL << 20`（256 MiB） | terminal cacheを使うか判断する費用・親辺表の概算予算。全体のメモリ上限ではない。0は今回のcache利用を抑止。解放は別途 `clear_terminal_cache()` |
| `deadline` | `std::chrono::steady_clock::time_point` | `time_point::max()` | 絶対停止目標。未指定は無期限。同じ時刻を複数呼び出しへ渡すと共通の終了目標になる。`steady_clock::now()+std::chrono::milliseconds(予算)` で作る |

列挙値には型名を付ける。例えば `options.preset = heuristic_steiner_tree_preset::quality;`、`options.path_mode = heuristic_steiner_tree_path_mode::on_demand;` と書く。

相対時間と絶対deadlineを両方指定すると、早い方が停止目標になる。いずれもsoft limitで、開始済みの処理や最初の実行可能解構築によって超過し得る。相対時間には初回CSR・辺順序構築の時間を含めない。アプリケーション全体の経過時間を管理するなら、準備前に絶対deadlineを作り、呼び出し元でも時刻を確認する。

自動restartは次のとおり。これは上限の選び方であり、時間切れや内部条件で実行回数は少なくなる。

| preset | `time_limit_ms=0`、deadline無期限 | 正の相対時間または有限deadlineを指定 |
|---|---:|---|
| `fast` | 0回 | 0回 |
| `balanced` | 4回 | 時間で止める |
| `quality` | 20回 | 時間で止める |

`fast` でも正の `restart_limit` は使える。ただし深い局所探索を有効にする設定ではない。反対に、時間を長く指定するだけでは処理が必ずその時間まで続くとは限らない。

### 5.4 cacheに必要なメモリ

$N$ を頂点数、$K$ を重複除去後のterminal数とすると、表の論理サイズは次のとおり。

$$B_{\mathrm{cache}}=KN\bigl(\operatorname{sizeof}(\texttt{long long})+\operatorname{sizeof}(\texttt{int})\bigr)$$

$B_{\mathrm{cache}}$ は距離と親辺表の合計byte数で、通常は $12KN$ byte。例えば $N=10000$、$K=20$ なら約2.29 MiBである。CSRや作業配列などのメモリは別途必要になる。

`auto_select` は、`fast` 以外・$K\leq32$・表が `memory_limit_bytes/2` 以下・時間見積りに収まる、という条件でcacheを選ぶ。`terminal_cache_auto_max_k()` はこの32を返す。明示 `terminal_cache` なら $K>32$ も指定可能だが、表が `memory_limit_bytes` を超える場合や処理前に時間切れの場合は `on_demand` に切り替わる。保存済み表があることだけで、自動選択がcacheに変わるわけではない。

### 5.5 ユースケースによる準備の違い

U02〜U08・U13・U14・U16は、同じsolverを複数回使いやすい。U09・U10・U12では候補間で同じsolverを共有する。U11は仮想頂点と無料辺を追加してから構築する。U15は変更後のグラフで再構築し、外部IDとの対応表を作る。U17・U18はマスや座標を頂点IDへ、敷設候補を辺へ変換してから構築する。

## 6. ユースケースごとの使い方

### 6.1 公開メソッドと共通の戻り値

以下の `T`・辺ID列はいずれも `const std::vector<int>&`、`options` は `const heuristic_steiner_tree_options&` で、省略時は `{}` になる。`normalize` にはoptions引数がない。解を返す各メソッドは `const` だが、内部の再利用領域は更新する。

| 呼び出し | 返却型 | 入力辺集合の意味 |
|---|---|---|
| `solve(T, options)` | `heuristic_steiner_tree_result` | 初期辺集合を与えず接続を構築する |
| `normalize(T, candidate_edges)` | 同上 | この辺集合の部分集合だけを返す。全terminalが候補内で連結している必要がある |
| `improve(T, initial_edges, options)` | 同上 | 連結済み解を改善。候補外の辺も使い、入力辺を削除してよい |
| `repair(T, partial_edges, options)` | 同上 | 空・非連結でもよいヒント。入力辺を削除してよい |
| `augment(T, base_edges, options)` | `heuristic_steiner_tree_augmentation_result` | 全base辺を外部で残す。今回の費用0として利用する |
| `solve_cost(T, options)` | `long long` | 内部で通常の `solve` を実行し、成功時の費用だけを返す。失敗時は `inf()` |

`solve(T)` と `solve(T, options)` は同じ既定引数付きメソッドの呼び出し方である。メンバ関数ポインタを使う場合の型にはoptions引数も含まれ、ポインタ経由の呼び出しではoptionsを明示する。

| 通常のresult | 型・初期値 | 利用方法 |
|---|---|---|
| `ok` | `bool`、`false` | 最初に判定する。費用と辺は成功時に利用する |
| `cost` | `long long`、`0` | 選択辺の費用合計。失敗時の値を判定に使わない |
| `edges` | `std::vector<int>`、空 | 登録済みの辺ID。成功時は昇順・重複なし。`solver.edge(id)` で端点・費用へ展開する |
| `statistics` | `heuristic_steiner_tree_statistics`、既定構築 | 下表の探索統計 |

`augment` のresultは `ok` と `statistics` が共通で、`cost` の代わりに `added_cost:long long=0`、`edges` の代わりに `added_edges:std::vector<int>` を持つ。追加辺はbaseを含まない昇順の辺ID列。実際のネットワークはbaseとの和集合なので、追加辺だけを次のターンの全体解として保存しない。

| statisticsの全フィールド | 型・既定値 | 意味 |
|---|---|---|
| `path_mode_used` | `heuristic_steiner_tree_path_mode`、`on_demand` | 表利用の段階へ到達した場合の実際の方式。早期returnの既定値は探索実行の証拠ではない |
| `path_memory_bytes` | `size_t`、0 | 今回参照した表の論理サイズ。cache再利用時も表を使えば0ではない。全メモリ量ではない |
| `initial_solution_count` | `int`、0 | 候補構築の回数。最終の破壊修復候補も含み、返却可能解の個数とは限らない |
| `restart_count` | `int`、0 | 実行した追加restartの回数 |
| `local_move_count` | `int`、0 | 局所変更を試した回数 |
| `improvement_count` | `int`、0 | 局所改善を採用した回数。全ての最良候補更新を数える値ではない |
| `dijkstra_count` | `long long`、0 | 今回実行した全域・増分・打切りDijkstraの回数。再利用した表の行は数えない |
| `edge_relaxation_count` | `long long`、0 | Dijkstraで調べた有向arc数。無向辺の両方向や、改善しなかった走査も含む |

### U01 `solve` で新規構築する

`solver.solve(terminals, options)` を呼ぶ。`terminals` は今回必須の全頂点。8章の `connect_required(graph, terminals, options)` はグラフ構築まで含む関数で、`Result` を返す。`result.ok` の確認後、`result.edges` の各IDを元の辺配列へ戻し、建設対象として使う。指定rootも `terminals` に入れる。

### U02 `normalize` で候補内を整理する

`solver.normalize(terminals, candidate_edges)` を呼ぶ。`candidate_edges` は承認済み候補の辺ID列で、重複や閉路を含められる。`clean_candidate` の戻り値はこの集合内だけの木。`ok=false` なら候補の範囲内では接続できない。候補外の辺を追加してよい場合はU04へ切り替える。

### U03 `improve` で連結済み候補を改善する

`solver.improve(terminals, initial_edges, options)` を呼ぶ。`initial_edges` は全terminalを接続する必要がある。`polish_connected` の成功結果を次の候補として保存する。費用は入力を正規化した解以下だが、改善が必ず発生するわけではない。入力が非連結なら失敗するため、U04を使う。

### U04 `repair` で部分解をつなぎ直す

`solver.repair(terminals, remaining_edges, options)` を呼ぶ。`remaining_edges` はdestroy後の辺IDで、空や複数成分でもよい。`repair_destroyed` は修復後の全体解を返す。外側探索では、これを元の全体解や他候補と目的関数で比較して採否を決める。空集合を渡すと内部で `solve` に委譲する。

### U05 前ターンの解を渡す

`solve_next_turn(solver, new_terminals, previous, options)` は、`previous.ok` ならその `edges` を `repair` へ、そうでなければ `solve` へ渡す。成功した戻り値で利用者側の保存解を置き換える。要求が変わるため、前ターンとの費用比較だけで今回の正しさを判断しない。以前のterminalを要求から外すことも利用者側の判断である。

### U06 baseとanchorを渡して増設する

`extend_installed(solver, new_terminals, base_edges, anchors, options)` は、要求頂点とanchorを連結対象にして `augment` を呼ぶ。baseの各成分を必ずつなぎたいなら、その成分ごとにanchorを1個指定する。返る `added_cost` はbase費用を含まない。成功後、`added_edges` をbaseへ追加して保持する。anchorなしでterminalが1個なら、接続作業なしで成功することに注意する。

### U07 増設結果を段階ごとに確定する

`build_in_stages(solver, initial_base, stage_terminals, anchors, options)` に、段階ごとの追加terminal集合を渡す。関数は要求を累積し、成功した追加辺を次段階のbaseへ入れる。`StagedAnswer` は段階別費用、累計追加費用、全既設辺、完了段階数を返す。途中失敗時も完了分を返し、失敗段階は反映しない。各段階を不可逆に決めるため、後段で安い辺が見つかっても先に建てた辺は消さない。

### U08 必須辺を固定費と分ける

`connect_with_mandatory(solver, terminals, mandatory_edges, options)` は必須辺を重複除去し、その両端をterminalへ加えて `augment` を呼ぶ。戻り値 `total_cost` は必須辺の元費用と追加費用の合計、`edges` は必須辺を全て含む辺集合である。baseを費用0と扱うのは可変部分を評価するためであり、固定費を無料にした目的関数ではない。

### U09 顧客候補を評価してペナルティ込みで選ぶ

`choose_customers(graph, root, customers, choices, options)` では `customers` に頂点と未接続ペナルティを入れる。`choices[i]` は頂点番号ではなく **customers配列の添字** の列。関数は候補ごとにrootと選択顧客を `solve` で接続し、返却辺上に入った顧客もサービス済みとして `objective` を計算する。返却する `candidate_index` は評価候補の番号、`network` は通常result。空の採用集合も比較したければ `choices` に空列を入れる。予算以下かを判定するなら、この実行可能解の費用は上界として使えるが、費用超過から不可能とは断定できない。

### U10 rootを列挙して開設費を加える

`choose_one_root(graph, terminals, roots, opening_cost, options)` は、`roots[i]` をterminalへ加えて `solve` し、`opening_cost[i]` を加算する。両配列は同じ長さで、開設費は非負とする。`RootAnswer` は選んだroot、合計費用 `objective`、接続解を返す。全rootを列挙しても内部の接続がヒューリスティックなので、拠点選択全体の厳密最適性は保証しない。

### U11 super-rootを加えてbackboneを表す

`connect_to_backbone(graph, terminals, gateways, options)` は仮想頂点を1個追加し、各gatewayへ費用0の辺を加える。仮想頂点もterminalとして `solve` し、戻す際に仮想辺を除く。`physical_edges` は元グラフの辺ID、`used_gateways` は使った出入口。出力の物理辺だけで全terminalが接続されている必要はない。gateway間の無料接続が実在しない場合、この変換は使えない。

### U12 代表点の組合せを評価する

`connect_group_representatives(graph, groups, representatives, mandatory_vertices, options)` を呼ぶ。`representatives[i][j]` には候補iにおいてグループjから選ぶ **頂点ID** を入れる。各行の長さはグループ数と同じで、各要素は対応グループ内にある必要がある。関数はそれらと `mandatory_vertices` を `solve` へ渡し、費用最小の候補番号と `network` を返す。候補が空、または全候補が接続不能なら失敗する。

### U13 同じインスタンスで問い合わせ列を処理する

`solve_queries(solver, queries, options)` は `queries[i]` ごとに `solve` し、同じ順序の `std::vector<Result>` を返す。cacheを明示するなら `options.path_mode` を `terminal_cache` にし、表の予算を確保する。統計の `path_mode_used` と `path_memory_bytes` で実際の利用を確認する。費用だけ必要なら同じループを `solve_cost` で書けるが、計算自体は通常 `solve` と同じである。表が不要になれば `solver.clear_terminal_cache()` を呼ぶ。

### U14 `add_edge` の後に旧解を修復する

`add_edges_and_resolve(solver, new_edges, terminals, previous, options)` は辺を登録後、旧解があれば `repair`、なければ `solve` を呼ぶ。返り値は新規辺ID列と追加後の `network`。追加先の頂点はコンストラクタで指定した範囲内に限る。ここで返す辺IDは以後も同じsolver内で有効である。

### U15 外部IDを介して再構築する

`rebuild_and_repair(n, current, terminals, previous_keys, options)` に、変更後の辺列を渡す。`CurrentEdge::key` は再構築をまたいで維持する外部IDで、同一入力内で一意にする。削除・禁止された辺は `current` に含めない。関数は残った旧辺を新しい内部IDへ変換して `repair` し、戻り値 `edge_keys` を外部IDへ戻す。内部IDをそのまま異なるsolverへ流用しない。

### U16 案件別の接続を独立に解く

`solve_independent_networks(solver, terminal_groups, options)` は案件ごとに `solve` を呼ぶ。`jobs[i]` が各案件の結果、全成功時の `total_cost` が案件別費用の総和である。どれかが失敗すると全体 `ok=false`、`total_cost=inf()` となるが、各案件の結果は確認できる。同じ辺が複数案件に現れれば複数回課金するので、全辺の和集合に一度だけ課金する問題にはこの費用集計を使わない。

### U17 マス間費用をグラフへ変換する

`connect_grid(grid, horizontal, vertical, required_cells, options)` は上下左右の利用可能マス間だけに辺を登録する。横費用は高さ $H$ ×幅 $(W-1)$、縦費用は $(H-1)$ × $W$。1行・1列の場合の空行列にもこの形を守る。戻り値 `GridAnswer::links` は建設する隣接マスの座標と費用で、移動順序ではない。壁上の必須マスや寸法不一致は例の関数が例外で拒否する。

### U18 線分長を整数費用へ変換する

`connect_geometric_candidates(points, allowed_links, terminals, scale, options)` は、線分長に `scale` を掛けて切り上げた整数を `add_edge` へ渡す。`network.edges` は `allowed_links` の添字、`network.cost` は整数費用、`actual_length` は選択線分の長さの合計を返す。倍率が大きいほど丸めの影響は小さくなるが、費用和の上限に注意する。小さな丸め差で選択結果が変わる場合があり、整数費用を倍率で割った値は元の実長と一致するとは限らない。

## 7. 制約・注意点

### 7.1 問題設定に関する制約

| 制約・間違いやすい点 | 説明 |
|---|---|
| 最適性・近似率 | この実装について厳密最適性や近似率のAPI保証はない。`quality` が全ての入力で `balanced` よりよいとも限らない |
| 無向・非負整数費用 | 有向到達性、負辺、小数重みを直接扱えない。小数の整数化では丸めにより目的関数が変わる |
| terminalは全て必須 | 任意顧客、group、root選択は外側で候補を作る必要がある。terminalを勝手に諦める機能ではない |
| 木と既設網の違い | 通常成功解は木だが、baseや必須辺を全て残すネットワークは閉路を含み得る |
| 頂点費用・制約付き接続 | 頂点を一度使う費用、次数・容量・ホップ数・経路長上限、辺素経路、耐故障性は直接指定できない。無向で安易に頂点分割しても、頂点費用を正しく強制できるとは限らない |
| 禁止辺・使用領域 | `normalize` の候補範囲以外に、各呼び出しの禁止辺マスクはない。使用不可辺を除いたグラフを構築する |
| 単一rootとbackbone | super-rootから複数候補への無料辺は、全候補が無料で相互接続されたモデルになる。「候補rootを1つだけ開設」の代わりにはならない |
| 独立案件と共有建設費 | 独立に解いた辺の和集合は、共同で一度だけ課金する問題の最適解とは限らない |

### 7.2 APIとoptionsの組合せ

| 呼び出し | 有効なoptions・例外 |
|---|---|
| `normalize` | optionsを受け取らない。候補内で非連結なら失敗 |
| `solve` / `solve_cost` | 全optionsを使う。ただし0〜1terminalや時間切れなどで到達しない処理がある |
| `augment` | 全optionsを使う。baseだけで接続済みなら追加なしで返る。baseが空でも永続terminal cacheは使わない |
| 空の部分解に対する `repair` | `solve` に委譲するので全optionsが対象 |
| 空でない部分解に対する `repair` | `seed` と `restart_limit` は使わない。`fast` では `path_mode` と `memory_limit_bytes` も使わない。時間指定は停止目標として有効 |
| `improve` の `balanced` / `quality` | `seed` と `restart_limit` は使わない。時間・path・メモリ設定は後続の改善に使う |
| `improve` の `fast` | 正規化と誘導部分グラフによる改善だけ。seed・restart・path・メモリ・時間指定によって探索量は変わらない |
| 0〜1terminal | 空解で即時成功するため、通常の探索設定は使わない |

`improve` と `normalize` の入力は、全terminalが入力辺で相互接続している必要がある。terminalと無関係な成分が混じることや閉路は許す。「入力の辺が全て単一成分」という条件より、この接続条件が本質である。

`repair` の辺は削除可能、`augment` のbaseは削除不能・追加費用0である。同じ引数でこの2つの意味を混ぜられない。保持義務のない未購入候補をbaseへ入れると、費用の過小評価になる。

### 7.3 数値・番号・失敗処理

| 注意点 | 扱い |
|---|---|
| `inf()` | `std::numeric_limits<long long>::max()/4`。64bit環境で2305843009213693951。到達可能な距離はこれ未満に収める |
| 費用和 | 入力辺1本が `long long` に入るだけでは不十分。経路・候補・固定費・ペナルティの加算も範囲内にする。全入力辺の非負費用和を `inf()` 未満にするのは安全側の十分条件。追加の評価項は別途検査する |
| 頂点・辺・arcの数 | IDと内部の個数には `int` を使う。無向辺を2arcに展開し、木の隣接配列も2倍の添字を使う。`2M`・`2(N-1)` なども `int` 範囲内にする。巨大な配列積が `size_t` を超える入力も扱えない |
| terminal番号 | 全て `0..n-1`。重複と並びは内部で正規化する。0〜1個の早期returnでも不正番号を許してよいわけではない |
| 辺ID | このsolverが返したIDだけを使う。別グラフのIDは同じ整数でも別の辺。U15では外部IDで対応付ける |
| `edge()` の参照 | 範囲外IDは使えない。`add_edge()` の再確保で参照が無効になることがあるので、追加をまたぐ保存にはIDか値コピーを使う |
| 入力検査 | 本体の検査は主に `assert` で、`NDEBUG` では消える。安全な失敗を返す入力パーサではない。8章の一部ラッパーの例外検査は本体の保証ではない |
| 失敗時 | 通常resultとaugmentationは必ず `ok` を見る。失敗時の費用・辺の内容は使わない。`solve_cost` は `inf()` と比較する |
| 非連結グラフ | 入力可能。全terminalが同じ連結成分に入る必要がある。既設辺も元グラフの辺なので、元から存在しない接続は作れない |
| 0辺・多重辺・自己ループ | 入力可能。0費用辺も通常の辺。多重辺は別ID。自己ループは接続に役立たず、通常解に入らない。必須自己ループの外部保持はU08で扱える |

### 7.4 実行時間・メモリ・状態

- **時間はsoft limit。** CSR準備、最初の候補、進行中のDijkstra、正規化などが目標時刻をまたぐ場合がある。期限切れの呼び出しも直ちに返る保証はない。複数候補を回す外側ループでは、次の呼び出しを始める前にも時刻を確認する。
- **deadlineは `steady_clock` 専用。** `system_clock` の時刻を混ぜない。相対時間と絶対時刻の両方を使うなら、停止目標は早い方になる。
- **メモリ設定はcache判定用。** グラフ・CSR・距離作業配列・elite・局所探索の複数距離表・vectorのcapacityは別途メモリを使う。`augment` が一時表を作る間も、通常グラフの保存済み表が残っている場合がある。
- **同じインスタンスは並行利用しない。** `const` メソッドも内部状態を更新する。異なるインスタンスは別々に使えるが、solverのコピーは辺・作業領域・cacheの大きなコピーになり得る。
- **再現性には入力・seed・時間条件を固定する。** 時間無制限かつ同じ実行環境・設定では決定的に動く。有限時間では負荷・初回準備・cache状態によって実行できる探索量が変わる。異なるコンパイラ間の辺選択一致は保証しない。
- **多数terminalでも入力できるが、探索内容は変わる。** 逐次構築はcache使用時に $K\leq192$、on-demand時に $K\leq512$ の範囲で実行する。それを超えると逐次構築とそのrestartを省き、他の構築・局所探索を使う。入力可能terminal数の上限を意味しない。
- **追加辺と全体解を取り違えない。** `augment` の追加費用と通常resultの総費用を、そのまま同じ目的関数として比較しない。固定費を含める場合は外部で一度だけ加算する。
- **候補評価の厳密性。** U09・U10・U12は提示候補だけのヒューリスティック評価。候補なしは失敗であり、自動で全組合せを生成しない。予算を満たす実行可能解は証拠になるが、見つからないことは不可能の証明にならない。

## 8. ユースケースごとのコード例

### 8.0 コピー方法と共通定義

**以下の共通定義を1回コピーし、その後ろに必要なU01〜U18のコードをコピーする。** 各例は共通定義だけに依存し、別のユースケースの関数をコピーする必要はない。全例を同時に使うこともできる。`Solver`・`Options`・`Result` などは本書の型別名、`GraphInput` や各 `Answer` は例で定義する入出力型であり、ライブラリの追加APIではない。

solverを受け取る例は、呼び出し元で `auto solver = make_solver(graph);` と作って保持する。頂点・辺IDはそのsolverに対して有効なものを渡す。`options` は省略可能で、5章の全既定値を使う。成功時にのみ費用・辺を使う。候補列や段階列を処理する例の `time_limit_ms` は1回ごとの目標であり、列全体の上限ではない。

```cpp
#include "heuristic_steiner_tree_v17.hpp"

using Solver = heuristic_steiner_tree;
using Options = heuristic_steiner_tree_options;
using Result = heuristic_steiner_tree_result;
using Augmentation = heuristic_steiner_tree_augmentation_result;
using Cost = long long;

struct InputEdge {
    int u, v;  // 0 <= u,v < GraphInput::n。無向辺なので向きは問わない。
    Cost cost; // 非負整数。0・多重辺・自己ループも入力できる。
};
struct GraphInput {
    int n;                       // 頂点数。頂点IDは 0..n-1。
    std::vector<InputEdge> edges; // 配列添字が返却辺IDになる。
};

// 以下はガイドの補助関数。ライブラリ本体のAPIではない。
int checked_size(std::size_t size) {
    if (size > static_cast<std::size_t>(INT_MAX / 2))
        throw std::invalid_argument("too many elements");
    return static_cast<int>(size);
}
Cost checked_add(Cost a, Cost b) {
    if (a < 0 || b < 0 || a >= Solver::inf() || b >= Solver::inf() - a)
        throw std::overflow_error("nonnegative total must be below inf()");
    return a + b;
}
void check_vertices(int n, const std::vector<int>& vertices) {
    for (int v : vertices)
        if (v < 0 || v >= n) throw std::invalid_argument("invalid vertex");
}
std::vector<int> unique_ids(std::vector<int> ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}
Solver make_solver(const GraphInput& graph) {
    if (graph.n < 0 || graph.n > INT_MAX / 2)
        throw std::invalid_argument("invalid vertex count");
    (void)checked_size(graph.edges.size());
    Solver solver(graph.n);
    Cost total = 0;
    for (const auto& e : graph.edges) {
        check_vertices(graph.n, {e.u, e.v});
        total = checked_add(total, e.cost);
        solver.add_edge(e.u, e.v, e.cost);
    }
    return solver;
}
// make_solver は安全側の十分条件「全入力辺の費用和 < inf()」も検査する。
// Solver& を受け取る例では、呼び出し元が頂点ID・辺ID・費用範囲を守る。
```

### U01 必須拠点をゼロから接続する

```cpp
// 入力: graph=利用可能な全辺、terminals=必ず接続する頂点。
// 出力: ok、総費用cost、graph.edgesの添字edges、探索統計。
Result connect_required(const GraphInput& graph,
                        const std::vector<int>& terminals,
                        const Options& options = {}) {
    check_vertices(graph.n, terminals);
    auto solver = make_solver(graph);
    return solver.solve(terminals, options);
}
```

### U02 候補辺だけを整理する

```cpp
// 入力: 同じグラフのsolver、必須頂点、使用を許す候補辺ID。
// 候補辺だけで全terminalが連結する必要がある。重複・閉路はよい。
// 出力: 候補辺の部分集合。候補外の辺を追加することはない。
Result clean_candidate(Solver& solver, const std::vector<int>& terminals,
                       const std::vector<int>& candidate_edges) {
    return solver.normalize(terminals, candidate_edges);
}
```

### U03 連結済みの外部解を改善する

```cpp
// 入力: 全terminalを接続している辺ID列。木でなくてもよい。
// 出力: 同じterminalを接続する木。元の候補外の辺も利用できる。
Result polish_connected(Solver& solver, const std::vector<int>& terminals,
                        const std::vector<int>& initial_edges,
                        const Options& options = {}) {
    return solver.improve(terminals, initial_edges, options);
}
```

### U04 destroy後の部分解を修復する

```cpp
// 入力: destroy後に残した辺ID。空・非連結でもよい。
// 出力: 接続を修復した全体解。残した辺も必要に応じて削除される。
Result repair_destroyed(Solver& solver, const std::vector<int>& terminals,
                        const std::vector<int>& remaining_edges,
                        const Options& options = {}) {
    return solver.repair(terminals, remaining_edges, options);
}
```

### U05 新しいterminal集合で次ターンを解く

```cpp
// 入力: 全ターンで保持するsolver、新しいterminal集合、前ターンの全体解。
// previousの辺IDはこのsolverのものとする。previous.ok=falseなら新規構築。
// 出力: 今ターンの解。呼び出し元は成功時にpreviousをこの戻り値へ更新する。
Result solve_next_turn(Solver& solver, const std::vector<int>& new_terminals,
                       const Result& previous, const Options& options = {}) {
    if (!previous.ok) return solver.solve(new_terminals, options);
    return solver.repair(new_terminals, previous.edges, options);
}
```

### U06 既設網のanchorへ追加接続する

```cpp
// 入力: base_edges=撤去不能な既設辺、new_terminals=今回接続する頂点。
// anchors=必ず接続先に含めたい既設成分ごとの代表頂点。不要なら空。
// 出力: 追加分だけ。実際には base_edges と added_edges の和集合を保持する。
Augmentation extend_installed(Solver& solver,
                              std::vector<int> new_terminals,
                              const std::vector<int>& base_edges,
                              const std::vector<int>& anchors,
                              const Options& options = {}) {
    new_terminals.insert(new_terminals.end(), anchors.begin(), anchors.end());
    return solver.augment(new_terminals, base_edges, options);
}
```

### U07 段階ごとに建設を確定する

```cpp
struct StagedAnswer {
    bool ok = true;                       // 全段階が成功したか。
    std::size_t completed_stages = 0;     // 失敗時にも完了済み段階数を返す。
    Cost added_cost = 0;                  // 完了段階の追加費用合計。初期baseは含めない。
    std::vector<Cost> stage_costs;        // 完了した各段階の追加費用。
    std::vector<int> installed_edges;     // 完了時点の全既設辺。失敗段階は反映しない。
};
// 入力: stage_terminals[i]=段階iで新たに要求する頂点。以前の要求も維持する。
// anchors=最初から必須の頂点。各段階のoptionsは共通、deadlineも同じ絶対時刻。
StagedAnswer build_in_stages(Solver& solver, std::vector<int> initial_base,
                            const std::vector<std::vector<int>>& stage_terminals,
                            std::vector<int> anchors,
                            const Options& options = {}) {
    StagedAnswer out;
    out.installed_edges = unique_ids(std::move(initial_base));
    for (const auto& additions : stage_terminals) {
        anchors.insert(anchors.end(), additions.begin(), additions.end());
        anchors = unique_ids(std::move(anchors));
        const auto result = solver.augment(anchors, out.installed_edges, options);
        if (!result.ok) { out.ok = false; break; }
        out.added_cost = checked_add(out.added_cost, result.added_cost);
        out.stage_costs.push_back(result.added_cost);
        out.installed_edges.insert(out.installed_edges.end(),
                                   result.added_edges.begin(), result.added_edges.end());
        out.installed_edges = unique_ids(std::move(out.installed_edges));
        ++out.completed_stages;
    }
    return out;
}
```

### U08 有料の必須辺を全て含める

```cpp
struct MandatoryAnswer {
    bool ok = false;
    Cost total_cost = Solver::inf();  // 必須辺の有料費用 + その他の追加費用。
    std::vector<int> edges;           // 必須辺を全て含むネットワーク。閉路も残る。
};
// 入力: mandatory_edges=必ず採用する有料辺ID。全て同じ連結網へ含める。
// 出力: 必須辺の費用を一度ずつ加算した全体費用と全辺ID。
MandatoryAnswer connect_with_mandatory(Solver& solver, std::vector<int> terminals,
                                       std::vector<int> mandatory_edges,
                                       const Options& options = {}) {
    mandatory_edges = unique_ids(std::move(mandatory_edges));
    Cost fixed_cost = 0;
    for (int id : mandatory_edges) {
        const auto& e = solver.edge(id);
        fixed_cost = checked_add(fixed_cost, e.cost);
        terminals.push_back(e.from);
        terminals.push_back(e.to);
    }
    const auto result = solver.augment(terminals, mandatory_edges, options);
    if (!result.ok) return {};
    mandatory_edges.insert(mandatory_edges.end(),
                           result.added_edges.begin(), result.added_edges.end());
    return {true, checked_add(fixed_cost, result.added_cost),
            unique_ids(std::move(mandatory_edges))};
}
```

### U09 任意顧客のペナルティ込みで評価する

```cpp
struct Customer { int vertex; Cost missed_penalty; };
struct PrizeAnswer {
    bool ok = false;
    int candidate_index = -1;         // 入力候補列の添字。厳密な全組合せ最適ではない。
    Cost objective = Solver::inf();   // 接続費 + 実際に未接続の顧客のペナルティ。
    Result network;
};
// 入力: root=必須拠点、customers=顧客頂点と未接続ペナルティ(非負、頂点重複不可)。
// choices[i]=候補iで必ず接続する顧客の「customers内の添字」。空候補も指定可能。
// 出力: 渡された候補を評価した中でobjectiveが最小の解。
PrizeAnswer choose_customers(const GraphInput& graph, int root,
                            const std::vector<Customer>& customers,
                            const std::vector<std::vector<int>>& choices,
                            const Options& options = {}) {
    check_vertices(graph.n, {root});
    auto solver = make_solver(graph);
    std::vector<char> seen(graph.n, false);
    Cost penalty_sum = 0;
    for (const auto& c : customers) {
        check_vertices(graph.n, {c.vertex});
        if (seen[c.vertex]) throw std::invalid_argument("duplicate customer");
        seen[c.vertex] = true;
        penalty_sum = checked_add(penalty_sum, c.missed_penalty);
    }
    PrizeAnswer best;
    for (int i = 0; i < checked_size(choices.size()); ++i) {
        std::vector<int> terminals{root};
        for (int index : choices[i]) {
            if (index < 0 || index >= checked_size(customers.size()))
                throw std::invalid_argument("invalid customer index");
            terminals.push_back(customers[index].vertex);
        }
        auto result = solver.solve(terminals, options);
        if (!result.ok) continue;
        std::vector<char> served(graph.n, false);
        served[root] = true;
        for (int id : result.edges) {
            served[solver.edge(id).from] = true;
            served[solver.edge(id).to] = true;
        }
        Cost objective = result.cost;
        for (const auto& c : customers)
            if (!served[c.vertex]) objective = checked_add(objective, c.missed_penalty);
        if (!best.ok || objective < best.objective)
            best = {true, i, objective, std::move(result)};
    }
    return best;
}
```

### U10 開設するrootを1つ選ぶ

```cpp
struct RootAnswer {
    bool ok = false;
    int root = -1;
    Cost objective = Solver::inf(); // 接続費 + 選んだrootの開設費。
    Result network;
};
// 入力: roots=開設候補頂点、opening_cost[i]=roots[i]の非負開設費。
// terminals=全候補で共通の必須頂点。利用する施設は1個だが他候補頂点の通過は許す。
RootAnswer choose_one_root(const GraphInput& graph,
                           const std::vector<int>& terminals,
                           const std::vector<int>& roots,
                           const std::vector<Cost>& opening_cost,
                           const Options& options = {}) {
    if (roots.size() != opening_cost.size())
        throw std::invalid_argument("one opening cost per root required");
    check_vertices(graph.n, terminals);
    check_vertices(graph.n, roots);
    auto solver = make_solver(graph);
    RootAnswer best;
    for (int i = 0; i < checked_size(roots.size()); ++i) {
        (void)checked_add(0, opening_cost[i]);
        auto required = terminals;
        required.push_back(roots[i]);
        auto result = solver.solve(required, options);
        if (!result.ok) continue;
        const Cost objective = checked_add(result.cost, opening_cost[i]);
        if (!best.ok || objective < best.objective)
            best = {true, roots[i], objective, std::move(result)};
    }
    return best;
}
```

### U11 無料backboneへ接続する

```cpp
struct BackboneAnswer {
    bool ok = false;
    Cost cost = Solver::inf();
    std::vector<int> physical_edges; // graph.edgesの添字。物理辺だけでは森でもよい。
    std::vector<int> used_gateways;  // 無料backboneへ接続する頂点。
};
// 入力: gatewaysは「既に相互接続済みで、利用費0」の同じbackbone上の頂点。
// 出力: 物理辺と使用gateway。仮想辺は出力しない。
BackboneAnswer connect_to_backbone(const GraphInput& graph,
                                  std::vector<int> terminals,
                                  const std::vector<int>& gateways,
                                  const Options& options = {}) {
    check_vertices(graph.n, terminals);
    check_vertices(graph.n, gateways);
    if (graph.n < 0 || graph.n >= INT_MAX / 2)
        throw std::invalid_argument("no room for virtual vertex");
    GraphInput expanded = graph;
    const int super_root = expanded.n++;
    const int physical_count = checked_size(expanded.edges.size());
    for (int gateway : gateways) expanded.edges.push_back({super_root, gateway, 0});
    auto solver = make_solver(expanded);
    terminals.push_back(super_root);
    const auto result = solver.solve(terminals, options);
    if (!result.ok) return {};
    BackboneAnswer out{true, result.cost, {}, {}};
    for (int id : result.edges) {
        if (id < physical_count) out.physical_edges.push_back(id);
        else out.used_gateways.push_back(gateways[id - physical_count]);
    }
    out.used_gateways = unique_ids(std::move(out.used_gateways));
    return out;
}
```

### U12 グループ代表点候補を評価する

```cpp
struct GroupAnswer {
    bool ok = false;
    int candidate_index = -1;
    Result network;
};
// 入力: groups[j]=グループjの選択可能頂点(空グループ不可)。
// representatives[i][j]=候補iでグループjから選ぶ頂点ID。全組合せの生成は呼び出し元。
// mandatory_vertices=グループとは別に必須の頂点。出力は候補中で接続費が最小の解。
GroupAnswer connect_group_representatives(
    const GraphInput& graph, const std::vector<std::vector<int>>& groups,
    const std::vector<std::vector<int>>& representatives,
    const std::vector<int>& mandatory_vertices, const Options& options = {}) {
    check_vertices(graph.n, mandatory_vertices);
    for (const auto& group : groups) {
        if (group.empty()) throw std::invalid_argument("empty group");
        check_vertices(graph.n, group);
    }
    auto solver = make_solver(graph);
    GroupAnswer best;
    for (int i = 0; i < checked_size(representatives.size()); ++i) {
        const auto& choice = representatives[i];
        if (choice.size() != groups.size())
            throw std::invalid_argument("one representative per group required");
        auto terminals = mandatory_vertices;
        for (std::size_t j = 0; j < groups.size(); ++j) {
            if (std::find(groups[j].begin(), groups[j].end(), choice[j]) == groups[j].end())
                throw std::invalid_argument("representative outside group");
            terminals.push_back(choice[j]);
        }
        auto result = solver.solve(terminals, options);
        if (result.ok && (!best.ok || result.cost < best.network.cost))
            best = {true, i, std::move(result)};
    }
    return best;
}
```

### U13 同じグラフで問い合わせ列を処理する

```cpp
// 入力: 同じsolverで評価するterminal集合列。互いに独立した問い合わせ。
// options.path_modeでcache方針を指定。solverを関数の外で保持すると次のbatchにも再利用する。
// 出力: queriesと同じ順・同じ長さの結果列。各要素のokを別々に確認する。
std::vector<Result> solve_queries(Solver& solver,
                                 const std::vector<std::vector<int>>& queries,
                                 const Options& options = {}) {
    std::vector<Result> results;
    results.reserve(queries.size());
    for (const auto& terminals : queries) results.push_back(solver.solve(terminals, options));
    return results;
}
```

### U14 辺を追加して解き直す

```cpp
struct EdgeUpdateAnswer {
    std::vector<int> new_edge_ids; // new_edgesと同じ順。追加前の辺IDも引き続き有効。
    Result network;              // 追加後のグラフ上の全体解。
};
// 入力: new_edgesの両端は既存頂点。既存辺も含む費用範囲・int範囲を呼び出し元が守る。
// previousは同じsolverの旧解。失敗していてもよい。
EdgeUpdateAnswer add_edges_and_resolve(Solver& solver,
                                      const std::vector<InputEdge>& new_edges,
                                      const std::vector<int>& terminals,
                                      const Result& previous,
                                      const Options& options = {}) {
    EdgeUpdateAnswer out;
    for (const auto& e : new_edges) out.new_edge_ids.push_back(solver.add_edge(e.u, e.v, e.cost));
    out.network = previous.ok ? solver.repair(terminals, previous.edges, options)
                              : solver.solve(terminals, options);
    return out;
}
```

### U15 変更後のグラフへ外部辺IDで引き継ぐ

```cpp
struct CurrentEdge {
    int key;       // 再構築をまたいで追跡する非負の外部辺ID。同じsnapshot内で一意。
    int u, v;      // 今回の頂点番号。前回から番号を変える場合はterminalsも変換する。
    Cost cost;     // 今回の非負費用。通行不可の辺は配列自体から除く。
};
struct RebuiltAnswer {
    bool ok = false;
    Cost cost = Solver::inf();
    std::vector<int> edge_keys; // 内部IDではなくCurrentEdge::keyを返す。
};
// 入力: 今回のn・利用可能辺・terminal。previous_keysは旧解の外部辺ID。
// 消えた辺はwarm startから外す。頂点数・費用が変わっても新しいsolverで評価する。
RebuiltAnswer rebuild_and_repair(int n, const std::vector<CurrentEdge>& current,
                                const std::vector<int>& terminals,
                                const std::vector<int>& previous_keys,
                                const Options& options = {}) {
    check_vertices(n, terminals);
    GraphInput graph{n, {}};
    std::unordered_map<int, int> local_id;
    for (int i = 0; i < checked_size(current.size()); ++i) {
        const auto& e = current[i];
        if (e.key < 0 || !local_id.emplace(e.key, i).second)
            throw std::invalid_argument("edge keys must be nonnegative and unique");
        graph.edges.push_back({e.u, e.v, e.cost});
    }
    auto solver = make_solver(graph);
    std::vector<int> partial;
    for (int key : previous_keys) {
        const auto it = local_id.find(key);
        if (it != local_id.end()) partial.push_back(it->second);
    }
    const auto result = solver.repair(terminals, partial, options);
    if (!result.ok) return {};
    RebuiltAnswer out{true, result.cost, {}};
    for (int id : result.edges) out.edge_keys.push_back(current[id].key);
    std::sort(out.edge_keys.begin(), out.edge_keys.end());
    return out;
}
```

### U16 独立した案件をそれぞれ接続する

```cpp
struct IndependentAnswer {
    bool ok = true;
    Cost total_cost = 0;         // 全件成功時のみ有効。同じ辺でも仕事ごとに費用を数える。
    std::vector<Result> jobs;    // 入力と同じ順の解。異なる仕事同士を接続する義務はない。
};
// 入力: terminal_groups[i]=独立した仕事iで相互接続する頂点。
// 辺の共用割引・容量競合がなく、仕事別の費用を加算できる問題に限る。
IndependentAnswer solve_independent_networks(
    Solver& solver, const std::vector<std::vector<int>>& terminal_groups,
    const Options& options = {}) {
    IndependentAnswer out;
    for (const auto& terminals : terminal_groups) {
        auto result = solver.solve(terminals, options);
        if (!result.ok) out.ok = false;
        if (result.ok && out.ok) out.total_cost = checked_add(out.total_cost, result.cost);
        out.jobs.push_back(std::move(result));
    }
    if (!out.ok) out.total_cost = Solver::inf();
    return out;
}
```

### U17 障害物付き格子を接続する

```cpp
struct GridLink { int row1, col1, row2, col2; Cost cost; };
struct GridAnswer {
    bool ok = false;
    Cost cost = Solver::inf();
    std::vector<GridLink> links; // 実際に建設するマス間の辺。マス訪問順ではない。
};
// 入力: gridは非空の長方形。'#'は通行不可、それ以外は利用可能。
// horizontal[r][c]: (r,c)-(r,c+1)の費用。サイズH×(W-1)。
// vertical[r][c]: (r,c)-(r+1,c)の費用。サイズ(H-1)×W。
// required_cells: 必須マスの(row,col)。壁上は不可。費用は辺の建設費である。
GridAnswer connect_grid(const std::vector<std::string>& grid,
                        const std::vector<std::vector<Cost>>& horizontal,
                        const std::vector<std::vector<Cost>>& vertical,
                        const std::vector<std::pair<int, int>>& required_cells,
                        const Options& options = {}) {
    const int h = checked_size(grid.size());
    const int w = h == 0 ? 0 : checked_size(grid[0].size());
    if (h == 0 || w == 0 || 1LL * h * w > INT_MAX / 2)
        throw std::invalid_argument("invalid grid dimensions");
    for (const auto& row : grid)
        if (row.size() != static_cast<std::size_t>(w)) throw std::invalid_argument("ragged grid");
    if (horizontal.size() != static_cast<std::size_t>(h) ||
        vertical.size() != static_cast<std::size_t>(h - 1))
        throw std::invalid_argument("invalid edge cost rows");
    for (const auto& row : horizontal)
        if (row.size() != static_cast<std::size_t>(w - 1)) throw std::invalid_argument("invalid horizontal width");
    for (const auto& row : vertical)
        if (row.size() != static_cast<std::size_t>(w)) throw std::invalid_argument("invalid vertical width");
    GraphInput graph{h * w, {}};
    std::vector<GridLink> links;
    const auto add = [&](int r, int c, int nr, int nc, Cost cost) {
        if (cost < 0) throw std::invalid_argument("negative grid cost");
        if (grid[r][c] == '#' || grid[nr][nc] == '#') return;
        graph.edges.push_back({r * w + c, nr * w + nc, cost});
        links.push_back({r, c, nr, nc, cost});
    };
    for (int r = 0; r < h; ++r) for (int c = 0; c < w; ++c) {
        if (c + 1 < w) add(r, c, r, c + 1, horizontal[r][c]);
        if (r + 1 < h) add(r, c, r + 1, c, vertical[r][c]);
    }
    std::vector<int> terminals;
    for (auto [r, c] : required_cells) {
        if (r < 0 || r >= h || c < 0 || c >= w || grid[r][c] == '#')
            throw std::invalid_argument("invalid required cell");
        terminals.push_back(r * w + c);
    }
    auto solver = make_solver(graph);
    const auto result = solver.solve(terminals, options);
    if (!result.ok) return {};
    GridAnswer out{true, result.cost, {}};
    for (int id : result.edges) out.links.push_back(links[id]);
    return out;
}
```

### U18 座標付き候補点を接続する

```cpp
struct Point { long double x, y; };
struct GeometricAnswer {
    Result network;              // costは整数化した費用、edgesはallowed_linksの添字。
    long double actual_length = 0; // 成功時の選択線分の長さの合計。
};
// 入力: points=利用できる中継点も含む座標、allowed_links=敷設可能な線分の両端ID。
// scale>0: 長さ1あたりの整数単位数。辺費用はceil(線分長×scale)。
// terminals=必須点ID。障害物を横切る線分は呼び出し元がallowed_linksから除く。
GeometricAnswer connect_geometric_candidates(
    const std::vector<Point>& points,
    const std::vector<std::pair<int, int>>& allowed_links,
    const std::vector<int>& terminals, long double scale,
    const Options& options = {}) {
    if (!std::isfinite(scale) || scale <= 0) throw std::invalid_argument("invalid scale");
    GraphInput graph{checked_size(points.size()), {}};
    check_vertices(graph.n, terminals);
    for (const auto& p : points)
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) throw std::invalid_argument("invalid coordinate");
    std::vector<long double> lengths;
    for (auto [u, v] : allowed_links) {
        check_vertices(graph.n, {u, v});
        const long double length = std::hypot(points[u].x - points[v].x, points[u].y - points[v].y);
        const long double rounded = std::ceil(length * scale);
        if (!std::isfinite(rounded) || rounded >= static_cast<long double>(Solver::inf()))
            throw std::overflow_error("scaled length too large");
        graph.edges.push_back({u, v, static_cast<Cost>(rounded)});
        lengths.push_back(length);
    }
    auto solver = make_solver(graph);
    GeometricAnswer out{solver.solve(terminals, options), 0};
    if (out.network.ok)
        for (int id : out.network.edges) out.actual_length += lengths[id];
    return out;
}
```

### 8.19 コードの検証範囲

本章のコードブロックをMarkdownから直接抽出し、共通定義と各例の組合せを独立した翻訳単位としてstrict C++20でコンパイルした。全例を同時に含む実行テストでは、全18関数を呼び、返却辺の連結性・費用・ID変換・固定辺保持を検査した。空集合、接続不能、0費用、多重辺、terminal変更、既設成分のanchor、cache切替、グラフ再構築、1行・1列格子なども対象にした。ASan／UBSanでも実行した。詳しい再現方法と検査結果は配布物の `guide_validation/` に含む。

この検証はサンプルの動作確認であり、任意入力の最適性証明ではない。

## 9. 実装

### 9.1 全体の流れ

通常の `solve` は次の順に進む。各段階の間で時間を確認し、得られた最良の実行可能解を保持する。

1. terminalを整列・重複除去し、必要ならCSRと辺費用順の索引を用意する。
2. 複数terminalを同時始点とするVoronoi構築から、最初の接続候補を作る。
3. 最短路方式を選択し、必要ならterminalごとの距離・親辺表を用意する。
4. 木にterminalを順次つなぐ構築を、始点と選択肢を変えて繰り返す。
5. 良い候補を保存し、候補同士の辺を合成して正規化する。
6. 良い候補へ、誘導部分グラフによる改善、terminal枝再接続、key-path交換を行う。
7. 最良解へ、3成分再接続、分岐の破壊修復、4成分再接続を行い、成功した改善を返す。

`fast` は候補生成と安価な改善を中心に行い、elite合成や深い局所探索を行わない。時間予算がある場合、逐次構築・restart・elite合成は予算のおおむね3/4までを目標とし、残りを局所改善へ回す。処理単位を途中で止めないため、厳密な時間配分ではない。

### 9.2 CSR・辺順序・整数最短路

無向辺を両方向のarcに展開し、頂点ごとの隣接辺を連続配列に格納するCSRを使う。自己ループは探索用arcには入れない。元の辺配列は保持するので辺IDを維持できる。

元グラフの辺順序とVoronoi境界候補の整列には、安定した8bit単位のradix sortを使う。全要素で同じbyteは処理を省く。候補辺集合の正規化には `(cost, edge_id)` による比較ソートを使うため、全ての整列がradix sortというわけではない。

Dijkstraの優先度キューは65bucketのradix heapで、非負整数距離の単調性を利用する。取り出した距離と現在の距離が異なる古い項目は捨てる。用途に応じて次を使い分ける。

- 全頂点への距離を作る全域Dijkstra。
- 複数sourceから最寄りterminalなどを求めるmulti-source Dijkstra。
- 現在の木へ加わった頂点だけを新しい距離0のsourceとして伝播する増分Dijkstra。
- 残った木への接続など、目標集合に到達するか改善不能な距離になったら止めるDijkstra。

局所探索では訪問世代番号を使い、触れた頂点だけ距離状態を初期化する。free-edge版は辺の元費用を書き換えず、markされた辺を探索時だけ費用0として読む。

### 9.3 候補の正規化

候補には重複・閉路・不要な枝が入り得る。候補辺を費用順に並べて重複を除き、Kruskalでterminal成分が1つになるまで辺を採用する。その後、terminalでない次数1の頂点をqueueで繰り返し除く。返却辺は辺ID順に整列する。

DSUは成分ごとにterminalの有無と、terminalを含む残り成分数を管理する。全頂点を必ず接続する通常のMSTと異なり、terminalが接続した時点で打ち切れる。これは候補を安価な木に整理する処理であり、候補内Steiner最適解を求める厳密solverではない。

`augment` ではbase辺を候補に加え、有効費用を0として正規化する。内部の代表木には不要なbase辺を載せない場合があるが、利用者側で保持するbase全体を撤去する意味ではない。

### 9.4 Voronoiによる構築

全terminalを距離0の始点としてDijkstraを行い、各頂点に最寄りterminalの領域と親辺を付ける。異なる領域をまたぐ元辺について、「左terminalから境界までの距離＋境界辺の費用＋右側の距離」を接続候補費用にする。

このterminal間候補グラフでKruskalを行い、採用した接続を元グラフの親経路へ展開する。重なった経路を正規化して木を得る。`repair` のseedや `augment` のbaseがある場合は、それらを費用0とみなしたVoronoiを使える。ただし `repair` の最終評価ではseedにも本来の費用を課す。

### 9.5 逐次最短路とrestart

1つのterminalから開始し、現在の木に近い未接続terminalを選んで経路を加える。基本構築では最も近い候補を選ぶ。restartでは開始terminalを変え、距離上位1〜4候補から選ぶことで異なる接続を作る。

cache方式では、各未接続terminalについて現在の木上の最良接続先を記録する。木へ新しく入った頂点だけを表から調べて更新する。on-demand方式では、新しく木に入った頂点をsourceとして距離の改善を伝播する。terminal数が大きい場合の逐次構築上限は7章のとおりである。

### 9.6 terminal最短路表の構築・更新

表はterminal1個につき、全頂点までの距離 `long long` と親辺 `int` を1行ずつ持つ。同じ集合ならそのまま使い、集合変更時は整列済みterminal集合の共通部分を求める。共有行を前へ詰め、配列サイズを変更してから後ろ向きに目的位置へ展開する。重なり得る領域の移動には `memmove` を使い、新規terminalの行だけDijkstraで埋める。

`auto_select` の時間見積りは全辺両方向をterminalごとに走査する $2MK$ を基にし、予算1msあたり20000arc走査という内部基準と比較する。これは方式選択用の目安であって、実時間の保証やcache命中を考慮した精密な見積りではない。

### 9.7 elite候補の合成と局所改善

`balanced` は最大4、`quality` は最大8の異なるelite候補を保持する。候補対の辺集合を合併して正規化することで、互いの安い部分を取り込む。`quality` は探索途中にも上位候補を記録し、最大12の履歴候補を局所改善の出発点にできる。

局所改善は最良候補だけでなく、上位候補にも適用する。主な近傍は次のとおり。

| 近傍 | 入れ替えるもの・探索方法 |
|---|---|
| 誘導部分グラフによる改善 | 現在使う頂点間に存在する元グラフの辺でKruskalをやり直し、不要葉を除く。既存の木辺にない安い辺を取り込む |
| terminal枝再接続 | 葉terminalから最初のterminalまたは分岐までの枝を外し、そのterminalから残りの木への安い経路を探す |
| key-path交換 | terminalまたは次数2以外の頂点を端点とする最大経路を外し、残った2成分間の安い経路に替える |

通常のterminal枝再接続は、交換経路が残存木へ最初に到達する位置で打ち切って、木を直接更新する。free-edge版などは再正規化で接続と費用を評価する。費用が厳密に小さくなる変更だけを採用する。

`balanced` / `quality` のterminal枝は、それぞれ最大2/8巡、1巡で最大8/24候補。key-pathは最大1/4巡、1巡で最大6/16候補を調べる。改善がない巡や時間切れで終了する。これらは公開optionsではなくpreset内の探索量である。

### 9.8 複数成分をまとめてつなぎ直す最終改善

最良解に対して、次の順序でより大きな変更を調べる。

1. **3成分再接続。** 非terminalの次数3分岐に接する最大経路を外す。残る3成分を費用0の森として扱い、各成分からの距離和が小さい接合点を探してつなぎ直す。
2. **分岐の破壊修復。** 次数3以上の非terminal分岐へ接する経路をまとめて除き、残った森をseedとするVoronoi構築で修復する。
3. **4成分再接続。** key-pathで隣接する2つの次数3分岐を対象に周囲を外す。4成分の単独距離、2成分を合流させて延長する距離、相補な2組の組合せを使って再接続する。

高費用の領域から有限個を試す。`balanced` / `quality` の候補数上限は、3成分が2/8、破壊修復が4/12、4成分が1/2である。残した森の費用はその候補内では固定なので、再接続中は費用0として新しい追加部分を探す。元の接続費用と他成分間の距離下界から、改善し得ない距離を打ち切る。

いずれも経路復元後に本来の目的関数で候補を評価し、最良解より安い場合だけ採用する。これらは限られた領域に対する局所探索で、全グラフの厳密最適性や全ての近傍の探索完了を保証しない。最終改善が成功して時間が残れば、key-path交換と誘導部分グラフによる改善を追加で行う。

### 9.9 APIごとの処理経路

| API | 主な経路 |
|---|---|
| `normalize` | 候補辺の正規化のみ。CSRや全辺順序を構築しない |
| `improve` | 入力を正規化し、失敗なら終了。誘導部分グラフによる改善後、presetに応じて枝・key-path・最終改善へ進む |
| `repair` | 空seedは `solve`。それ以外はseed単体の正規化、seedを無料扱いしたVoronoi、通常Voronoiを候補にし、本来の費用で比較する。局所改善と最終改善を適用する |
| `augment` | baseだけの接続を先に確認。必要ならbaseを費用0とした構築・restart・elite・局所改善を行い、最終の代表木からbase以外の辺だけを返す |
| `solve_cost` | 通常 `solve` の戻り値から費用だけ取り出す。経路生成を省略する別実装ではない |

### 9.10 計算量とメモリの読み方

非負整数重みのDijkstra1回を $D(N,M)$ と書く。固定64bitのradix heapを使うため、一般的な見積りとして $O(N+M(1+\log(C+1)))$ を置ける。$C$ は扱う有限距離の規模で、ビット数は64に制限される。0費用辺だけでも辺の走査は必要である。実時間は訪問範囲と重み分布にも依存する。

| 処理 | 主な費用 |
|---|---|
| 辺登録 | 1回あたり償却 $O(1)$ |
| CSR・全辺のradix整列 | 固定64bitでは $O(N+M)$ 時間・領域 |
| terminalの正規化 | $O(K\log K)$ |
| 長さ $L$ の候補辺の正規化 | $O(K\log K+N+L\log L)$ が公開呼び出し全体の目安。$L$ は入力の重複も含む |
| terminal表の全構築 | $K$ 回のDijkstraと $O(KN)$ 領域 |
| terminal表の共通行再利用 | 新規terminal数を $\Delta K$ とすると、$\Delta K$ 回のDijkstraと共有行移動。最悪 $O(KN)$ の移動領域走査 |
| on-demand逐次構築 | 多くてterminal数に比例する増分伝播。最悪の目安は $O(KD(N,M))$ |
| cache利用の逐次構築 | 木へ入る頂点の走査と未接続terminalの選択が中心。経路復元・正規化は別途必要 |
| 局所探索 | 調べる候補数に応じたDijkstra・全辺走査・候補正規化。時間切れと改善の有無で変わる |

表を作らない場合も、元辺・CSR・距離配列などに $O(N+M)$ の領域を使う。cacheに加え、3成分・4成分再接続では複数の距離・親辺配列を一時保持する。restart数だけで総実行時間を決められるわけではなく、表構築・候補経路長・局所探索の成功回数も影響する。

本書のコード例は、返却解の連結性と費用を利用者側でも検算できる形にしている。性能調整では、同じ入力・同じ評価目的の下で `result.statistics` と実測時間・返却費用を合わせて確認する。
