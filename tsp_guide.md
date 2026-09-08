# TSPライブラリ ユーザーガイド

対象ソースは `tsp_solver_v07.hpp`。C++20の単一ヘッダで、名前空間は `tsp` です。本書は、このヘッダの公開仕様と実装を説明します。第4・6・8章のT01〜T12は同じユースケースを指します。

## 1. 何を解くライブラリか？

### 1.1 与えられた地点をすべて訪ねる順序を決める

たとえば、既に選ばれた点検地点をロボットがすべて回るとき、どの順で回れば移動コストが小さくなるかを求めます。対象地点の集合は利用者が与え、ライブラリはその要素を増減せず、訪問順だけを決めます。

出発地点へ戻る「閉路」と、最後の地点で終了する「開路」を扱います。開路は始点だけ、終点だけ、両端、またはどちらも固定しない指定ができます。移動コストは方向によって異なってもよく、距離のほかに静的な移動時間や段取りコストを使えます。

### 1.2 目的関数

入力の地点集合を $V$、その要素数を $n$ とし、全地点をちょうど1回ずつ並べた順序を $P=(v_0,\ldots,v_{n-1})$ とします。開路の目的は次の総コストを最小にすることです。

$$\min_P C_{\mathrm{path}}(P),\qquad C_{\mathrm{path}}(P)=\sum_{i=0}^{n-2}d(v_i,v_{i+1})$$

閉路では最後から先頭への移動を加えます。

$$\min_P C_{\mathrm{cycle}}(P),\qquad C_{\mathrm{cycle}}(P)=\sum_{i=0}^{n-2}d(v_i,v_{i+1})+d(v_{n-1},v_0)$$

| 記号 | 意味 |
|---|---|
| $V$、$n$ | 訪問対象の地点集合と、その数。すべて訪問する |
| $P$ | $V$ の全要素を重複なく並べた順序。最適化の対象 |
| $v_i$、$i$ | 順序の $i$ 番目の地点と、その位置。位置は0から数える |
| $d(u,v)$ | 地点 $u$ から地点 $v$ へ移るコスト。$u,v$ は地点ID |
| $C_{\mathrm{path}}$ | 戻り辺を数えない開路の総コスト |
| $C_{\mathrm{cycle}}$ | 最後から先頭へ戻る移動を含めた閉路の総コスト |
| $\min_P$ | 許される全順序の中で総コストを最小化するという目的 |

上の閉路式は $n\ge2$ の場合です。空集合と1地点の場合は、対角コストによらず開路・閉路とも0とします。戻り先の地点を順序の末尾へ重ねて入れる必要はありません。

始点を $s$ に固定するなら $v_0=s$、終点を $t$ に固定する開路なら $v_{n-1}=t$ の条件を加えます。$s,t$ は入力集合に含まれる指定地点です。2地点以上の開路で両端を固定するなら、異なる地点を指定します。閉路の始点固定は、同じ巡回路をどの地点から表記するかを固定する指定です。

### 1.3 本ライブラリに任せる範囲

「どの地点を選ぶか」「何台に分けるか」「積載容量や時間窓を守るか」は目的関数に含まれません。訪問対象・担当車両を外側で決め、その後の1本の順序最適化に使います。容量・報酬選択・集荷配達を同時に扱う必要があれば、別のroutingライブラリの対象です。

各地点の作業時間が訪問順によらない定数で、全地点を必ず訪ねるなら、その合計はどの順序でも同じです。移動順をこのライブラリで求め、作業時間の合計を呼び出し側で加算できます。時刻や直前の状態によって作業時間が変わる場合は、この切り離しはできません。

## 2. 厳密解アルゴリズム

### 2.1 頂点数22以下の部分集合DP

`tsp::held_karp_tsp` は、入力頂点数が22以下のときに閉路・開路の厳密最適解を求めます。対称・非対称のどちらにも使え、開路では始点・終点の固定にも対応します。求める目的は第1章と同じです。

「既に訪問した集合」と「最後にいる地点」を状態にし、次の地点を1つずつ加えるDPです。固定した端点を状態集合の外へ置けるため、状態数を少なくできます。ただし、**頂点数22以下という入力上限は端点を除く前の数**にかかります。

自由に並べる地点数を $m$ とすると、主要な時間量は $O(m^2 2^m)$、メモリは $O(m2^m)$ です。別途、入力全頂点間の距離を保存する $O(n^2)$ の領域と計算を使います。

| 問題形式 | $m$ の値 | 入力22頂点・`Calc=long long`でのDPと親配列の目安 |
|---|---|---|
| 閉路 | $n-1$ | 約378 MiB |
| 開路・両端自由 | $n$ | 約792 MiB |
| 開路・片端固定 | $n-1$ | 約378 MiB |
| 開路・両端固定 | $n-2$ | 約180 MiB |

ここで $n$ は入力頂点数、$m$ はDPの部分集合に含める頂点数です。目安は、1状態につき8バイトのコストと1バイトの親を持つ配列から計算した値で、その他のメモリを含みません。大きな `Calc` を使えば増えます。

### 2.2 どの入口を選ぶか

小さい経路が多数現れる場合や、最適性が必要な小規模問題では、厳密解が有力です。車両ごとの担当地点を固定できるなら、各経路を小さな独立TSPとして厳密に解くこともできます。ただし、それで車両割当て全体の最適性が得られるわけではありません。

`held_karp_tsp` には時間制限引数がなく、探索途中で打ち切って解を返す機能もありません。22以下なら常に実行予算に収まるという意味ではありません。利用環境で時間・メモリを測り、たとえば18頂点を出発点に、外側で厳密解へ渡す上限を決めます。第8章に、上限以下だけ厳密に解く関数を載せます。

容量・時間窓・集荷前後関係を通常のTSP DPへ渡しても、それらの制約は検査しません。そうした問題を厳密に解くには、制約を満たす順列だけを列挙するか、状態や遷移に制約を組み込んだ別実装が必要です。内部の小区間厳密最適化も、経路全体の厳密最適性を保証するものではありません。

## 3. 差分更新

### 3.1 前ターンの順序を使う

`tsp::improve_tsp` に既存の順序を渡すと、その順序から改善します。`solve_tsp` は訪問集合から構築し直す入口なので、前の順序を活かしたいときは `improve_tsp` を選びます。

同じ距離・端点条件なら、改善後の目的値は入力順序の目的値以下になるよう最良解を保持します。距離が変わった場合は、新しい距離で入力順序を再評価し、その値から改善します。変更前のコストとの大小に保証はありません。

| ターン間の変更 | 必要な準備 | 再利用されるもの |
|---|---|---|
| 既存順序だけをさらに改善 | 順序をそのまま `improve_tsp` へ渡す | 入力の順序 |
| 辺コスト・道路状況の変更 | 新しい値を返す距離関数を渡す。最短距離を事前計算しているなら更新する | 前の順序を初期解にできる |
| 訪問済み地点の削除 | 入力順序からその地点を除く | 残った地点の相対順序 |
| 新規地点の追加 | 各地点を1回だけ入力順序へ挿入する | 既存部分の順序。新規地点を置く場所は呼び出し側で決めてよい |
| 現在位置を新しい始点にする | 現在位置IDを先頭へ1回だけ置き、開路・始点固定にする | それ以外の未完了地点の順序 |
| 終点・閉路開路・対称性の変更 | 入力順序と対応するフラグを揃える | 条件に適合する順序 |

### 3.2 差分更新されないもの

公開のContextや辺更新メソッドはありません。呼び出しごとに入力を内部番号へ対応づけ、候補集合や作業配列を作ります。ヒューリスティックの候補構築は最悪 $O(n^2)$ 回の距離参照を必要とし、前の候補集合や探索途中の状態を引き継ぎません。内部の差分評価は1回の呼び出し内の高速化です。

そのため、ターン間での利点は「既存の良い順序から始められること」です。変更した辺や地点の数だけに比例して再計算するAPIではありません。外側に静的な距離行列を持ち、各ターンの距離関数がそれを定数時間で参照する形は有効です。

実行中に距離関数の返す値を変えてはいけません。現在位置が辺の途中なら、新しい地点として位置を表し、残り地点との移動コストを作る等の処理が呼び出し側に必要です。時刻や訪問履歴に応じた動的コストは直接表現できません。

## 4. ユースケース

### T01 平面上の点をすべて回って戻る

検査点、採取点、都市などを巡回して出発地へ戻ります。各点の座標と移動コストの定義を与えます。第8章はユークリッド距離を整数へ丸める例ですが、格子上の縦横移動距離などにも置き換えられます。丸め方は問題の採点式と一致させます。

### T02 始点・終点を自由にした一筆の訪問順

材料の処理順や、どこから開始してもよい点検順を決めます。訪問地点と地点間コストを与え、最後から最初へ戻る移動は数えません。始点を自由にできない作業ならT03の条件が必要です。

### T03 片端または両端が決まった移動

現在位置から残りの点検を行うなら始点固定、最終回収場所が決まっているなら終点固定、両方決まっていれば両端固定です。訪問対象に指定地点を含め、どちらの端を固定するかを指定します。両端を同じ地点にして周回を表す場合は、開路ではなく閉路を使います。

### T04 指定拠点から表記する巡回路

同じ巡回路でも、出力を倉庫やロボットの待機位置から始めたい場合です。拠点を訪問集合に1回だけ含め、先頭をその地点に固定します。閉路は最後から先頭に戻るため、末尾にもう一度拠点を入れません。

### T05 一方通行・方向依存の段取り順

地点間の往復時間が違う、工程AからBへの変更費用とBからAへの変更費用が違う、といった問題です。両方向の静的コストを与え、非対称として扱います。直前の工程より前の履歴までコストに影響するなら、この2地点間の費用だけでは表せません。

### T06 大きなグラフ上の重要地点巡回

多数の交差点のうち、仕事がある地点だけの巡回順を決めます。グラフ、非負の辺コスト、重要地点の集合、出発地へ戻るかどうかを与えます。重要地点間の最短距離を使い、結果の連続地点間を実際の通路へ展開して移動します。例は重要地点の順だけを返します。

### T07 既存順序・次ターンの計画の改善

問題固有の規則で作った順序や、前ターンの未完了順序を短くします。訪問対象を更新した既存順序、今回の距離、端点条件を与えます。距離が変わっていれば、変更後の費用に対して改善します。外側で固定した中間順序や訪問時刻まで守る機能はありません。

### T08 車両割当て・クラスタ分割後の経路改善

どの車両がどの顧客を担当するかは決まっていて、各担当内の順序だけを直す用途です。車両ごとの出発・終了地点と顧客順序を与えます。単一訪問の純配送なら需要合計は順序によらないため容量を保てます。集荷配達・時間窓では順序変更で制約を破る可能性があり、このままは使えません。

### T09 小規模の厳密解で品質を確かめる

同じ入力に対する短時間の解と厳密最適解を比較し、どの程度コストが残っているかを把握します。訪問集合、コスト、ヒューリスティックに使う時間を指定します。厳密計算の時間は別に確保します。小規模での結果を大規模の品質保証とは解釈しません。

### T10 部分問題の大きさに応じて厳密計算を使う

外側の処理から、大小さまざまな訪問集合が繰り返し渡される場合です。訪問集合と、自分の時間・メモリ予算に合う厳密計算の頂点数上限を与えます。上限以下は最適解、それより大きい場合は短時間の解を求めます。

### T11 小数の距離・64bitを超える整数コスト

丸めずに距離を最適化したい場合と、大きな重みで整数コストを正確に扱いたい場合です。訪問順の問題は同じでも、評価に使う数値型を明示的に選びます。個々の辺だけでなく、総コストと変更差分まで収まる型が必要です。第8章にそれぞれ独立した関数を載せます。

### T12 複数試行から良い順序を選ぶ

外側の予算をいくつかに分け、乱数を変えて候補を作り、最もコストが小さいものを採用します。試行数、総時間の割当て、最初の乱数seedを指定します。試行ごとに前処理を行うため、分割しすぎると各試行で改善する時間が減ります。単一の長い試行より必ず優れるとは限りません。

## 5. 利用準備

### 5.1 環境と入力順序

`#include "tsp_solver_v07.hpp"` として使います。追加リンクやACLは不要です。ソースと掲載例はGCCの `<bits/stdc++.h>` を使い、大整数例は `__int128_t` 拡張を使うため、GCC系のC++20環境を想定します。通常は利用側の `.cpp` からincludeします。ヘッダを主ファイルにすると埋め込みテスト用 `main` が有効になります。

solverクラスのコンストラクタはありません。変更可能な `order`、距離関数、`TSPParam` を準備し、自由関数を呼びます。

| 入力 | 型・既定 | 準備方法 |
|---|---|---|
| `order` | `std::vector<T>&` または `std::span<T>`。省略不可 | 訪問する整数IDをそれぞれ1回入れる。`T`は `bool`以外の整数型。非連続IDでもよい |
| `dist` | `dist(T from, T to)` として呼べる関数オブジェクト。省略不可 | 元のIDで移動コストを返す。実行中に値を変えない |
| `Calc` | 関数の先頭テンプレート引数。省略可能 | 距離の戻り値が浮動小数点型なら `double`、それ以外は `long long`。必要なら `solve_tsp<Calc>(...)` のように明示 |
| `param` | `const tsp::TSPParam&`、既定 `{}` | 次節の設定。`held_karp_tsp`と`total_distance`には渡さない |
| `stats` | `tsp::TSPStats*`、既定 `nullptr` | 統計が必要なら `tsp::TSPStats stats;` を作り `&stats`。solve・improveのみ |

`order` は出力先も兼ねており、関数から戻ると順序が変わります。元の順序が必要ならコピーします。`dist` は値を繰り返し参照されるので、巨大グラフの最短路探索をcallback内で毎回行わず、必要な距離を事前計算します。

整数 `Calc` は負になり得る変更差分も扱える符号付き型を使います。`__int128_t` の距離callbackでも `Calc` の既定は `long long` なので、`tsp::solve_tsp<__int128_t>(...)` と明示します。`long double` を保持したい場合も明示指定が必要です。

### 5.2 `TSPParam`の全パラメータ

| メンバー | 型 | 既定値 | 指定・選び方 |
|---|---|---|---|
| `time_limit_ms` | `int` | `100` | 非負の相対時間ms。前処理を含む。全体の残り時間から出力等の余裕を差し引く |
| `seed` | `std::uint64_t` | `1` | 比較では固定、複数試行では変更する |
| `cycle` | `bool` | `true` | 戻り辺を数えるなら `true`、片道の順序なら `false` |
| `fixed_start` | `bool` | `false` | `true`なら呼び出し時の `order.front()` を先頭に保つ |
| `fixed_end` | `bool` | `false` | 開路で `true`なら入力 `order.back()` を末尾に保つ。閉路では指定不可 |
| `symmetric` | `bool` | `true` | すべての対象地点で `dist(u,v)==dist(v,u)` のときだけ `true`。自動検査はされない |
| `max_move_evaluations` | `std::uint64_t` | `0` | 0は評価回数上限なし。正なら探索候補の評価数を制限。相対時間も同時に有効 |

候補幅、近傍の種類、kick間隔を選ぶ公開パラメータはありません。利用者が選ぶ主な探索予算は時間・評価回数です。評価回数には候補構築の距離参照や小区間DPの全遷移が同じ単位で含まれるわけではありません。固定評価回数でも相対時間で先に停止することがあります。

### 5.3 端点指定の組合せ

| 目的 | `cycle` | `fixed_start` | `fixed_end` | 入力の準備 |
|---|---|---|---|---|
| T01 通常の閉路 | `true` | `false` | `false` | 全地点を1回ずつ |
| T04 指定拠点を先頭にする閉路 | `true` | `true` | `false` | 拠点を先頭へ |
| T02 両端自由の開路 | `false` | `false` | `false` | 全地点を1回ずつ |
| T03 始点だけ固定 | `false` | `true` | `false` | 指定始点を先頭へ |
| T03 終点だけ固定 | `false` | `false` | `true` | 指定終点を末尾へ |
| T03 両端固定 | `false` | `true` | `true` | 指定始点を先頭、終点を末尾へ |
| 使用不可 | `true` | 任意 | `true` | 閉路に終点固定を組み合わせない |

固定フラグはIDを別途指定する引数ではありません。`order`の端に置いたIDが対象です。閉路の `solve_tsp` は `fixed_start=false` でも初期構築を入力先頭から始めます。ただし、そのフラグでは探索後も先頭が同じである保証はありません。閉路の厳密解は、巡回路の表記を入力先頭に固定して返します。

### 5.4 統計

`TSPStats` の各値は初期値0で、solve・improveの呼び出し開始時にもリセットされます。累積は呼び出し側で行います。

| メンバー | 型 | 意味 |
|---|---|---|
| `move_evaluations` | `std::uint64_t` | 局所探索の変更候補評価数 |
| `accepted_moves` | `std::uint64_t` | 局所探索で採用した改善数 |
| `local_search_runs` | `std::uint64_t` | 局所探索を開始した回数 |
| `kicks` / `improved_kicks` | `std::uint64_t` | 摂動を加えた回数／その反復で最良解を更新した回数 |
| `exact_repairs` / `improved_exact_repairs` | `std::uint64_t` | 完了した小区間DP最適化の回数／それでコストを下げた回数 |
| `candidate_build_ms` | `double` | 候補集合構築の時間ms |
| `construction_ms` | `double` | 初期構築段階の時間ms。improveは初期構築をしないので概ね0 |
| `search_ms` | `double` | 局所探索・小区間DP・反復探索の時間ms |
| `total_ms` | `double` | API内部で計測した全体時間ms。通常経路では最終的な出力への書き戻し直前まで |

`exact_repairs` は公開の `held_karp_tsp` を呼んだ回数ではありません。公開の厳密解入口にはstats引数がありません。厳密解の時間は呼び出し側で測ります。

## 6. ユースケースごとの使い方

### 6.1 全公開関数

次の `T` は頂点型、`Calc` は計算型です。表の `param`・`stats` の既定は第5章のとおりです。

| 関数 | 引数 | 戻り値と入力の変更 |
|---|---|---|
| `tsp::solve_tsp<Calc>(order, dist, param, stats)` | 変更可能なvectorまたはspan、距離関数、設定、統計先 | `Calc`の総コストを返し、`order`を求めた順へ書き換える。元の順序から構築する指定ではない |
| `tsp::improve_tsp<Calc>(order, dist, param, stats)` | 同上。`order`は既存順序 | 総コストを返し、入力順序を改善した順へ書き換える |
| `tsp::held_karp_tsp<Calc>(order, dist, cycle, fixed_start, fixed_end)` | 変更可能なvectorまたはspan。bool引数の既定は順に `true, false, false` | 厳密最適コストを返し、`order`を最適順序へ書き換える。22頂点以下、時間制限なし |
| `tsp::total_distance<Calc>(order_span, dist, cycle)` | `std::span<const T>`、距離関数、`cycle`の既定 `true` | 現在順序のコストのみ。順序は変更しない。$O(n)$ |

`total_distance`にはvector用の推論補助overloadがないため、`tsp::total_distance(std::span<const int>(order), dist, param.cycle)` のようにconst spanを作ります。他の3関数はvectorを直接渡せます。spanを使う場合、その参照先を関数の実行中に移動・破棄しません。

戻り値はコストだけです。経路は変更された `order` から受け取り、連続するIDを移動します。閉路なら最後から先頭への移動も実行します。`feasible`のような成功フラグはなく、入力した全地点の順列を返します。禁止辺や別制約を無視しても成功扱いになるため、モデルの前提を守ります。

### 6.2 用途と呼び出しの対応

| ケース | 呼び出し方 | 戻り値の利用・注意 |
|---|---|---|
| T01 | 座標を読む `dist` と既定の閉路設定で `solve_tsp` | `{cost, order}`として返す例。座標の丸めを採点式と揃える |
| T02 | `cycle=false`、両端固定なしで `solve_tsp` | 先頭と末尾も結果として決まる。戻り辺は加算しない |
| T03 | 指定した端点を入力両端へ配置し、対応する固定フラグで `solve_tsp` | 例の `std::optional<int>` にIDを渡し、省略する端は `std::nullopt`。これは例側の引数で、ライブラリはbool指定 |
| T04 | depotを先頭に置き `cycle=true, fixed_start=true` | 出力先頭はdepot。末尾にdepotを追加しない |
| T05 | `symmetric=false` で `solve_tsp` | 有向費用で再計算する。厳密解入口には対称性フラグが不要 |
| T06 | 重要地点間の最短距離を事前計算して `solve_tsp` | 返すIDは元グラフの重要地点。実際の辺列へ展開する前駆配列は例には含めない |
| T07 | `total_distance`で今回の入力コストを求め、同じ設定で `improve_tsp` | 今回の距離に対する入力値以下であることを確認できる。前ターンの古い費用とは比較しない |
| T08 | 同じ出発・終了点ならdepotを1回追加して閉路、異なるなら両端を追加して固定開路で `improve_tsp` | 改善後に追加した両端を除いて顧客列だけを返す。担当集合は変えない |
| T09 | コピーした同じ頂点集合に `solve_tsp` と `held_karp_tsp` | コストと順序を両方返す。厳密コスト0のとき割合を求める除算に注意 |
| T10 | 大きさを例側で判定し、上限以下なら `held_karp_tsp`、それ以外は `solve_tsp` | 両分岐とも同じ `{cost, order}`。時間制限は近似分岐だけに効く |
| T11 | `double`の距離を渡す、または `solve_tsp<__int128_t>` を明示 | 返すコスト型もそれぞれ `double` / `__int128_t`。再計算も同じ型と丸め方に揃える |
| T12 | 毎回入力集合をコピーし、seedを変えて `solve_tsp(..., &stats)` | 最小コストの順序を保持し、評価数を自分で累積する |

### 6.3 時間制限と再現性

`time_limit_ms` は初期構築と候補構築を含む相対時間です。局所探索は512評価間隔と段階の境界で時刻を確認します。初期構築、候補1行の走査、経路の反転・コピー、小区間DP等は任意の命令で停止できないので、時間を超過することがあります。

0msでも、空でない入力に対する全地点の順序を返すための処理と費用再計算は行います。「一切処理しない」指定ではありません。極小予算では改善量を期待できません。

絶対deadlineの公開パラメータはありません。全体の終了時刻を共有したい場合は、呼び出し側で残りmsを計算し、余裕を差し引いて渡します。T12は試行へ割り当てた時間の和を制御する例で、全体時間の厳密な上限を保証するものではありません。

## 7. 制約・注意点

| 条件・失敗例 | 意味と対処 |
|---|---|
| `cycle=true && fixed_end=true` | 使用不可。閉路の先頭表記は `fixed_start` で指定する |
| 固定端点を入力の端へ置き忘れる | ライブラリは入力先頭・末尾のIDを固定する。例T03のように呼び出し前に配置する |
| 閉路の末尾に先頭IDを重ねて入れる | 同じ作業を2要素にしてしまう。各IDは1回だけ入力する |
| `bool`の頂点型 | 型制約で使用不可。通常は `int` を使う |
| 順序の一部を固定したい | 固定できるのは両端のみ。途中の特定順序、車両制約、訪問時刻の固定はない |
| 容量・時間窓・先行制約を渡したつもりになる | それらの入力欄はない。PDP経路を普通のTSPとして並べ替えると不正になり得る |
| 非対称距離に `symmetric=true` | 対称用の差分式の前提が崩れる。実際の距離に合わせて指定する |
| 対称距離に `symmetric=false` | 利用可能だが、候補集合と管理する情報が変わり、余分な処理を使う |
| 非metric距離 | 三角不等式は必要ない。ただし、解品質や近傍の有効性は入力に依存する |
| 実行中に距離を変える | 内部の差分や保持コストと不整合になる。ターン間で更新し、次の呼び出しで渡す |
| callbackが高価 | 候補構築だけでも二乗回の参照がある。最短路や重い計算を事前に保存する |
| 数値型の不足 | 辺だけでなく総和、DPの部分和、差分が収まる符号付き `Calc` を選ぶ。overflow検査はない |
| `NaN`や無限大を距離に含める | 比較や減算を壊す。到達可能な頂点集合と有限費用を準備する |
| 巨大な有限費用で禁止辺を表す | 高価でも選ばれることがある。禁止辺違反を表す戻り値はない。必要なら結果の辺を別途検査する |
| 内部sentinelに近い整数 | 探索用の無限大値は `numeric_limits<Calc>::max()/4`。DP部分和や候補コストがこの値と衝突しない余裕を確保する |
| 負の辺費用 | 静的な有限値なら和・差分として扱えるが、部分和・差分を含めて型の範囲内にする。通常の非負距離と同じ解品質を保証するものではない |
| 浮動小数点の完全な一致を期待する | 加算順の違い・丸め誤差がある。費用再計算の確認には尺度に合う許容誤差を使う。厳密解も選んだ数値型上での比較 |
| `solve_tsp`で入力解を改善したい | 初期構築で順序を作り直す。入力解の活用には `improve_tsp` |
| `held_karp_tsp`に23頂点以上 | 使用不可。`assert`を無効にしても上限は解除されない |
| 厳密解に時間制限を期待する | `TSPParam`を受け取らない。外側でサイズと資源を判断してから呼ぶ |
| 0または1頂点 | 使用可能でコスト0。1頂点の自己辺コストは数えない |
| 近似入口の最適性を期待する | 全順序・全近傍を調べる保証はない。最適性が必要なら資源範囲内で厳密解を使う |
| 同じseedだけで完全再現を期待する | 実時間停止ではCPU負荷で探索量が変わる。比較するなら評価上限を固定し、時間にも余裕を持たせる |
| 例のassertを全入力検証と思う | 正方形、ID範囲、一意性、対称性、数値範囲等の前提を利用者が守る。`-DNDEBUG`で前提が変わるわけではない |

## 8. ユースケースごとのコード例

各ブロックは単独でコピーできる関数定義です。標準入力や未定義の補助関数に依存しません。行列例ではIDを行列の添字とし、正方行列・有効ID・IDの一意性・安全な数値範囲を前提とします。対称設定の例は対称行列で呼びます。

戻り値を `std::pair<Cost, std::vector<int>>` に揃えた例では、`first` が総コスト、`second` が訪問順です。入力を値で受ける例は、関数内のコピーだけを書き換えます。例側の時間や厳密サイズの既定値は、ライブラリの既定値とは区別してください。

### T01 平面上の点をすべて回って戻る

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: point[v]={x,y}、IDは配列添字。座標は絶対値10^9以下の整数を想定。
//   距離をhypotで計算して最も近い整数に丸める。問題の採点式がこれと一致する場合に使う。
//   総和はlong longと内部sentinelより十分小さくする。time_limit_msはms(既定100)。
// 出力: {戻り辺を含む整数費用, 各IDを1回含む巡回順}。末尾に先頭を重ねない。

std::pair<long long, std::vector<int>> solve_euclidean_tsp_cycle(
    const std::vector<std::pair<long long, long long>>& point,
    int time_limit_ms = 100) {
    std::vector<int> order(point.size());
    std::iota(order.begin(), order.end(), 0);
    auto dist = [&](int from, int to) {
        const long long dx = point[from].first - point[to].first;
        const long long dy = point[from].second - point[to].second;
        return std::llround(std::hypot(
            static_cast<double>(dx), static_cast<double>(dy)));
    };

    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = true;
    param.fixed_start = false;
    param.fixed_end = false;
    param.symmetric = true;
    const long long cost = tsp::solve_tsp(order, dist, param);
    return {cost, std::move(order)};
}
```

### T02 始点・終点を自由にした一筆の訪問順

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは非負・有限・対称な正方行列。verticesは訪問IDの一意な集合。
//   seedは乱数seed(既定1)、time_limit_msはms(既定100)。両端は自由に選ばれる。
// 出力: {戻り辺を含まない費用, 訪問順}。入力verticesは値渡しなので元配列を変更しない。

std::pair<long long, std::vector<int>> solve_free_end_hamilton_path(
    const std::vector<std::vector<long long>>& distance,
    std::vector<int> vertices, std::uint64_t seed = 1,
    int time_limit_ms = 100) {
    const int n = static_cast<int>(distance.size());
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);
    for (int vertex : vertices) assert(0 <= vertex && vertex < n);
    auto dist = [&](int from, int to) { return distance[from][to]; };

    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.seed = seed;
    param.cycle = false;
    param.fixed_start = false;
    param.fixed_end = false;
    param.symmetric = true;
    const long long cost = tsp::solve_tsp(vertices, dist, param);
    return {cost, std::move(vertices)};
}
```

### T03 片端または両端が決まった移動

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは有限正方行列、verticesは訪問IDの一意な集合。
//   start/goalは固定したい地点ID、自由にする端はstd::nullopt。指定IDはverticesに1回含める。
//   例: start=0, goal=std::nulloptなら始点だけ固定、start=0, goal=7なら両端固定。
//   2地点以上で両端指定する場合は別ID。symmetricは実際の対称性、時間はms(既定100)。
// 出力: {開路の費用, 条件どおりの端点を持つ順序}。戻り辺は数えない。

std::pair<long long, std::vector<int>> solve_endpoint_hamilton_path(
    const std::vector<std::vector<long long>>& distance,
    std::vector<int> vertices, std::optional<int> start,
    std::optional<int> goal, bool symmetric, int time_limit_ms = 100) {
    const int n = static_cast<int>(distance.size());
    for (const auto& row : distance) assert(static_cast<int>(row.size()) == n);
    for (int vertex : vertices) assert(0 <= vertex && vertex < n);
    if (start) assert(std::count(vertices.begin(), vertices.end(), *start) == 1);
    if (goal) assert(std::count(vertices.begin(), vertices.end(), *goal) == 1);
    if (start && goal) assert(*start != *goal || vertices.size() == 1);
    if (start) {
        std::iter_swap(vertices.begin(),
                       std::find(vertices.begin(), vertices.end(), *start));
    }
    if (goal) {
        std::iter_swap(std::prev(vertices.end()),
                       std::find(vertices.begin(), vertices.end(), *goal));
    }
    auto dist = [&](int from, int to) { return distance[from][to]; };
    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = false;
    param.fixed_start = start.has_value();
    param.fixed_end = goal.has_value();
    param.symmetric = symmetric;
    const long long cost = tsp::solve_tsp(vertices, dist, param);
    assert(!start || vertices.front() == *start);
    assert(!goal || vertices.back() == *goal);
    return {cost, std::move(vertices)};
}
```

### T04 指定拠点から表記する巡回路

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは有限正方行列、verticesは一意な訪問ID集合でdepotを1回含む。
//   depotを巡回順の先頭に固定する。symmetricは実際の対称性、時間はms(既定100)。
// 出力: {戻り辺込み費用, depotを先頭とする順序}。depotは末尾には追加しない。

std::pair<long long, std::vector<int>> solve_depot_tsp_cycle(
    const std::vector<std::vector<long long>>& distance,
    std::vector<int> vertices, int depot,
    bool symmetric, int time_limit_ms = 100) {
    assert(std::count(vertices.begin(), vertices.end(), depot) == 1);
    std::erase(vertices, depot);
    vertices.insert(vertices.begin(), depot);
    auto dist = [&](int from, int to) { return distance[from][to]; };

    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = true;
    param.fixed_start = true;
    param.fixed_end = false;
    param.symmetric = symmetric;
    const long long cost = tsp::solve_tsp(vertices, dist, param);
    assert(vertices.empty() || vertices.front() == depot);
    return {cost, std::move(vertices)};
}
```

### T05 一方通行・方向依存の段取り順

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: directed_cost[u][v]はuからvへの有限な費用。逆向き費用は独立に設定する。
//   verticesは有効添字の一意な集合、time_limit_msはms(既定100)。
// 出力: {有向の戻り辺を含む費用, 巡回順}。結果を逆順にして同じ費用と考えない。

std::pair<long long, std::vector<int>> solve_asymmetric_tsp_cycle(
    const std::vector<std::vector<long long>>& directed_cost,
    std::vector<int> vertices, int time_limit_ms = 100) {
    const int n = static_cast<int>(directed_cost.size());
    for (const auto& row : directed_cost) {
        assert(static_cast<int>(row.size()) == n);
    }
    for (int vertex : vertices) assert(0 <= vertex && vertex < n);
    auto dist = [&](int from, int to) { return directed_cost[from][to]; };

    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = true;
    param.symmetric = false;
    const long long cost = tsp::solve_tsp(vertices, dist, param);
    return {cost, std::move(vertices)};
}
```

### T06 大きなグラフ上の重要地点巡回

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: graph[u]は{移動先ID,非負重み}の列。importantは一意で空でない重要地点ID集合。
//   重要地点同士は互いに到達可能、最短距離と経路和は安全な有限値とする。
//   cycleは戻り辺を数えるか。undirected=trueなら同じ重みの逆辺も入力する。
//   time_limit_ms(既定100)はTSP呼び出しだけの予算で、最短路前処理は含まない。
// 出力: {最短距離で評価した費用, 元グラフの重要IDの訪問順}。
//   中間交差点を含む実移動経路は返さない。必要なら最短路の前駆配列を別途保存する。

std::pair<long long, std::vector<int>> solve_important_vertices_on_graph(
    const std::vector<std::vector<std::pair<int, long long>>>& graph,
    std::vector<int> important, bool cycle, bool undirected,
    int time_limit_ms = 100) {
    using QueueItem = std::pair<long long, int>;
    constexpr long long inf = std::numeric_limits<long long>::max() / 8;
    const int n = static_cast<int>(graph.size());
    assert(!important.empty());

    std::vector<int> rank(n, -1);
    for (int i = 0; i < static_cast<int>(important.size()); ++i) {
        assert(0 <= important[i] && important[i] < n);
        assert(rank[important[i]] == -1);
        rank[important[i]] = i;
    }
    std::vector<std::vector<long long>> shortest(
        important.size(), std::vector<long long>(n, inf));
    for (int source_index = 0;
         source_index < static_cast<int>(important.size()); ++source_index) {
        const int source = important[source_index];
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
    for (int from : important) {
        for (int to : important) assert(shortest[rank[from]][to] < inf);
    }

    auto dist = [&](int from, int to) { return shortest[rank[from]][to]; };
    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = cycle;
    param.symmetric = undirected;
    const long long cost = tsp::solve_tsp(important, dist, param);
    return {cost, std::move(important)};
}
```

### T07 既存順序・次ターンの計画の改善

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは今回の有限距離行列。initial_orderは今回の未完了地点を1回ずつ含む順序。
//   完了地点の削除・新規地点の追加は呼び出し前に行う。固定端点はあらかじめ両端へ置く。
//   cycle/fixed_start/fixed_end/symmetricは今回の条件。閉路でfixed_endは使えない。
//   time_limit_msはms(既定100)。コストと差分はlong longに収まること。
// 出力: {今回の入力順序以下の費用, 改善順序}。比較対象は変更前の古い費用ではない。

std::pair<long long, std::vector<int>> improve_existing_tsp_order(
    const std::vector<std::vector<long long>>& distance,
    std::vector<int> initial_order, bool cycle,
    bool fixed_start, bool fixed_end, bool symmetric,
    int time_limit_ms = 100) {
    assert(!cycle || !fixed_end);
    auto dist = [&](int from, int to) { return distance[from][to]; };
    const long long before = tsp::total_distance(
        std::span<const int>(initial_order), dist, cycle);

    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = cycle;
    param.fixed_start = fixed_start;
    param.fixed_end = fixed_end;
    param.symmetric = symmetric;
    const long long after = tsp::improve_tsp(initial_order, dist, param);
    assert(after <= before);
    return {after, std::move(initial_order)};
}
```

### T08 車両割当て・クラスタ分割後の経路改善

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは有限正方行列。start[r]/finish[r]は車両rの両拠点。
//   service_routes[r]は顧客IDだけの列。顧客IDは一意で、その車両の両拠点IDと別にする。
//   容量が顧客需要合計だけで決まる単一訪問向け。時間窓・PDP先行制約等は扱わない。
//   symmetricは実際の対称性、time_limit_ms_per_routeは1経路のms(既定20)。
// 出力: 同じ担当顧客を並べ替えた列。追加した拠点は取り除いて返す。空経路は空のまま。

std::vector<std::vector<int>> improve_each_vehicle_route(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<int>& start,
    const std::vector<int>& finish,
    std::vector<std::vector<int>> service_routes,
    bool symmetric, int time_limit_ms_per_route = 20) {
    assert(start.size() == finish.size());
    assert(start.size() == service_routes.size());
    auto dist = [&](int from, int to) { return distance[from][to]; };

    for (int route = 0; route < static_cast<int>(service_routes.size()); ++route) {
        std::vector<int> order = service_routes[route];
        if (order.empty()) continue;  // 空車両には移動経路を追加しない。
        assert(std::find(order.begin(), order.end(), start[route]) == order.end());
        assert(std::find(order.begin(), order.end(), finish[route]) == order.end());

        tsp::TSPParam param;
        param.time_limit_ms = time_limit_ms_per_route;
        param.symmetric = symmetric;
        param.fixed_start = true;
        if (start[route] == finish[route]) {
            order.insert(order.begin(), start[route]);
            param.cycle = true;
            param.fixed_end = false;
            tsp::improve_tsp(order, dist, param);
            assert(order.front() == start[route]);
            order.erase(order.begin());
        } else {
            order.insert(order.begin(), start[route]);
            order.push_back(finish[route]);
            param.cycle = false;
            param.fixed_end = true;
            tsp::improve_tsp(order, dist, param);
            assert(order.front() == start[route]);
            assert(order.back() == finish[route]);
            order.erase(order.begin());
            order.pop_back();
        }
        service_routes[route] = std::move(order);
    }
    return service_routes;
}
```

### T09 小規模の厳密解で品質を確かめる

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは有限・対称行列、verticesは一意な訪問集合で22頂点以下。
//   time_limit_ms(既定50)は近似計算のみ。厳密計算用の時間・メモリを別途確保する。
// 出力: heuristic/exactの費用と順序を持つTspExactComparison。どちらも閉路。
//   exact_cost=0の場合、割合のgapを求めるための除算はしない。

struct TspExactComparison {
    long long heuristic_cost = 0;
    long long exact_cost = 0;
    std::vector<int> heuristic_order;
    std::vector<int> exact_order;
};

TspExactComparison compare_tsp_with_exact_solution(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<int>& vertices, int time_limit_ms = 50) {
    assert(vertices.size() <= 22);
    auto dist = [&](int from, int to) { return distance[from][to]; };
    TspExactComparison result;
    result.heuristic_order = vertices;
    result.exact_order = vertices;

    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = true;
    param.symmetric = true;
    result.heuristic_cost =
        tsp::solve_tsp(result.heuristic_order, dist, param);
    result.exact_cost =
        tsp::held_karp_tsp(result.exact_order, dist, true, false, false);
    assert(result.exact_cost <= result.heuristic_cost);
    return result;
}
```

### T10 部分問題の大きさに応じて厳密計算を使う

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは有限・対称行列、verticesは一意な訪問集合。
//   exact_limitは厳密計算に渡す頂点数上限(0..22、例の既定18)。資源を測って決める。
//   time_limit_ms(既定100)は上限を超えた場合の近似計算だけに効く。
// 出力: {閉路費用, 訪問順}。上限以下なら厳密解で、途中の時間打ち切りはない。

std::pair<long long, std::vector<int>> solve_tsp_exact_or_heuristic(
    const std::vector<std::vector<long long>>& distance,
    std::vector<int> vertices, int exact_limit = 18,
    int time_limit_ms = 100) {
    assert(0 <= exact_limit && exact_limit <= 22);
    auto dist = [&](int from, int to) { return distance[from][to]; };
    long long cost = 0;
    if (static_cast<int>(vertices.size()) <= exact_limit) {
        cost = tsp::held_karp_tsp(vertices, dist, true, false, false);
    } else {
        tsp::TSPParam param;
        param.time_limit_ms = time_limit_ms;
        param.cycle = true;
        param.symmetric = true;
        cost = tsp::solve_tsp(vertices, dist, param);
    }
    return {cost, std::move(vertices)};
}
```

### T11 小数の距離・64bitを超える整数コスト

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 下の2関数は数値型の異なる独立した例。
// 1つ目の入力: point[v]={有限なx,y}。差・距離・総和も有限な範囲にする。
//   time_limit_msはms(既定100)。距離は整数へ丸めずdoubleで比較する。
// 1つ目の出力: {doubleの閉路費用, 各点の添字の巡回順}。

std::pair<double, std::vector<int>> solve_euclidean_tsp_with_double(
    const std::vector<std::pair<double, double>>& point,
    int time_limit_ms = 100) {
    std::vector<int> order(point.size());
    std::iota(order.begin(), order.end(), 0);
    auto dist = [&](int from, int to) {
        return std::hypot(point[from].first - point[to].first,
                          point[from].second - point[to].second);
    };
    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = true;
    param.symmetric = true;
    const double cost = tsp::solve_tsp(order, dist, param);
    return {cost, std::move(order)};
}

// 2つ目の入力: distanceは対称な__int128_t行列、verticesは有効添字の一意な集合。
//   辺・総和・差分・部分和が型に収まり、内部sentinel(max/4)と衝突しないこと。
//   time_limit_msはms(既定100)。Calcは必ず明示指定する。
// 2つ目の出力: {__int128_tの開路費用, 訪問順}。標準ostreamへ直接出す変換は含めない。
std::pair<__int128_t, std::vector<int>> solve_large_cost_hamilton_path(
    const std::vector<std::vector<__int128_t>>& distance,
    std::vector<int> vertices, int time_limit_ms = 100) {
    auto dist = [&](int from, int to) -> __int128_t {
        return distance[from][to];
    };
    tsp::TSPParam param;
    param.time_limit_ms = time_limit_ms;
    param.cycle = false;
    param.symmetric = true;
    const __int128_t cost =
        tsp::solve_tsp<__int128_t>(vertices, dist, param);
    return {cost, std::move(vertices)};
}
```

### T12 複数試行から良い順序を選ぶ

```cpp
#include "tsp_solver_v07.hpp"
#include <bits/stdc++.h>

// 入力: distanceは有限・対称行列、verticesは一意な訪問集合。
//   trials>=1、total_time_ms>=trials。各試行へ整数除算で時間を配る(余りは未使用)。
//   first_seedは最初のseed(既定1)。総時間は配分予算で、実時間の厳密上限ではない。
// 出力: 全試行の最小閉路費用costとorder、評価数の合計move_evaluations。
//   候補集合構築は試行ごとに実行される。

struct TspMultistartResult {
    long long cost = std::numeric_limits<long long>::max();
    std::vector<int> order;
    std::uint64_t move_evaluations = 0;
};

TspMultistartResult solve_tsp_with_multiple_seeds(
    const std::vector<std::vector<long long>>& distance,
    const std::vector<int>& vertices, int trials,
    int total_time_ms, std::uint64_t first_seed = 1) {
    assert(trials >= 1 && total_time_ms >= trials);
    const int time_per_trial = total_time_ms / trials;
    auto dist = [&](int from, int to) { return distance[from][to]; };

    TspMultistartResult best;
    for (int trial = 0; trial < trials; ++trial) {
        std::vector<int> order = vertices;
        tsp::TSPParam param;
        param.time_limit_ms = time_per_trial;
        param.seed = first_seed + static_cast<std::uint64_t>(trial);
        param.cycle = true;
        param.symmetric = true;

        tsp::TSPStats stats;
        const long long cost = tsp::solve_tsp(order, dist, param, &stats);
        best.move_evaluations += stats.move_evaluations;
        if (cost < best.cost) {
            best.cost = cost;
            best.order = std::move(order);
        }
    }
    return best;
}
```

## 9. 実装

### 9.1 概要と流れ

近い未訪問頂点から初期順序を作り、候補近傍に限定した局所探索を行います。その後、経路を一度崩す操作と局所探索を繰り返すIterated Local Search（ILS）で改善します。局所的な小区間の厳密最適化も組み合わせます。

1. 入力IDを内部番号へ対応づける。
2. `solve_tsp`ではnearest-neighborで初期解を作る。`improve_tsp`では入力順を費用再計算して使う。
3. 各頂点の近い出辺・必要なら入辺から候補集合を構築する。
4. 局所探索を行い、小区間DPを試す。DPで改善したら再度局所探索する。
5. 経路にkickを1回加え、局所探索する。4 kickごとに小区間DPも試す。
6. 最良値を更新すれば保持し、更新しない場合は同点も含めて最良経路に戻す。
7. 時間・評価回数制限に達したら、最良順序を入力の `order` へ書き戻す。

`held_karp_tsp` はこの反復とは独立した公開の厳密解入口です。内部の補助型・関数は `TSPParam` のprivate領域と呼び出し元のlambdaに置かれ、利用者が状態を操作するクラスではありません。

### 9.2 初期構築

nearest-neighborは現在地点から最も安い未訪問地点を次に選びます。閉路と始点固定の開路は入力先頭から始めます。始点自由の開路は乱数で始点を選び、終点固定ならその地点を始点候補から除外します。固定終点は最後まで残します。

途中で期限に達すると、残り地点を入力順に補って、固定終点を末尾に置きます。その後に全経路費用を計算するので、短い時間でも入力全地点を含む順序になります。初期構築の最悪時間は距離参照が定数時間なら $O(n^2)$ です。

0・1頂点は費用0を返します。2頂点以下や、3頂点で両端固定の開路では、構築後の不要な探索反復を行いません。

### 9.3 候補集合

対称距離では各頂点から近い出辺最大8本の相手を候補にします。非対称では出辺・入辺それぞれ最大8頂点を集め、重複を除いた最大16頂点を使います。候補行を構築する前に時間を確認するため、期限によって一部の行が空のまま探索へ進む場合があります。

候補構築は最悪 $O(n^2)$、保存する候補は $O(n)$ です。ヒューリスティック全体が全頂点間の距離行列を恒久的に持つ方式ではなく、探索中も距離callbackを呼びます。利用者が距離行列を用意している場合は、その参照費用がここに効きます。

### 9.4 局所探索と差分

| 近傍 | 操作 | 評価上の特徴 |
|---|---|---|
| 2-opt | 区間を反転し、前後の接続を変更 | 非対称では内部辺の向きの差も加える |
| Or-opt 1〜3 | 連続1〜3頂点を順序を保って別位置へ移す | 主に境界の辺差分で評価 |
| swap | 2頂点の位置を交換 | 隣接等の重複辺を考慮して、影響する辺だけを評価 |
| block transposition | `A-B-C-D`を`A-C-B-D`へつなぎ替える | ブロック内の向きを保ち、接続辺の差を使う |

順序から頂点位置を引く配列を管理し、近い頂点の経路上の位置をすばやく調べます。非対称2-optでは、各辺の順向きと逆向きの費用差を累積し、反転区間の追加費用を求めます。順序変更後は必要な位置情報と累積情報を更新します。

改善が見つからなかった頂点を一時的に探索対象から外し、周囲が変わると、その頂点と関連する候補を再び有効にします。局所探索用の作業配列は同じsolve内で再利用します。固定端点に触れる操作は実行しません。

候補評価の差分が軽くても、反転・移動のvector操作や累積配列更新には変更範囲に応じた時間がかかります。全組合せを検査するわけではないので、全2-opt・全swapに対する局所最適性は保証しません。

### 9.5 小区間の厳密最適化

経路中の連続区間を1つ選び、両側の地点を固定して区間内部だけを部分集合DPで並べ替えます。区間長は `min(8, n-2)` で、3頂点以上の区間を作れる場合だけ行います。先頭・末尾を境界として残せる位置から選び、閉路で末尾をまたぐ区間は使いません。

区間内と両側境界への距離を小さな連続配列へ保存し、DPの遷移中に元のcallbackを繰り返し呼ぶ負荷を抑えます。初回の局所探索後と4 kickごとに試し、区間費用が下がった場合だけ反映します。改善後は局所探索を再開します。

DP遷移にも一定間隔の期限確認があります。途中停止した区間の未完成結果を経路へ反映しません。この処理が厳密なのは、選ばれた区間・固定された両境界という条件のもとだけです。

### 9.6 kickと最良解の保持

閉路では、十分な長さがあれば複数の切断位置を選んでブロックを回転させ、短い場合は動かせる区間を反転します。開路では固定端点を外した範囲でブロック回転または反転を行います。1反復につきkickは1回です。

kick直後の費用を再計算して局所探索へ渡します。最良値を更新しない反復は同点も含めて最良経路へ戻します。各反復の一時的な悪化が最終解として返ることはありません。乱数は呼び出しのseedから生成され、次の呼び出しに探索状態を引き継ぎません。

### 9.7 公開のHeld–Karp DP

自由な頂点集合の部分集合を $S$、その最後の頂点を $v$ とし、そこまでの最小コストを $D[S,v]$ とします。未訪問頂点 $w$ を加える遷移は次の形です。

$$D[S\cup\{w\},w]=\min\left(D[S\cup\{w\},w],\ D[S,v]+d(v,w)\right)$$

ここで $D$ はDP配列、$S$ は既に並べた自由頂点の集合、$v$ は集合内の最終頂点、$w$ は集合外の次の頂点です。$d$ は第1章と同じ移動費用です。始点固定ならその始点から各自由頂点への費用で初期化し、始点自由なら各頂点を費用0の始点候補にします。閉路は入力先頭を固定始点として扱います。

全自由頂点を訪ねた後、閉路なら始点へ戻る費用、終点固定の開路なら固定終点への費用を加えて最良の最終状態を選びます。各状態に保存した前の頂点をたどり、元のID順序へ復元します。

距離は入力頂点間の二乗サイズの配列へ先に保存します。対角要素は0とし、自己辺callbackを呼ぶ必要はありません。DPは指数規模の全状態を確保するため、小区間repairと違って時間打ち切りはありません。有限費用が内部sentinelに衝突せず、加算が型に収まることが正しい比較と復元の前提です。
