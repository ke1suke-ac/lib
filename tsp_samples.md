# TSP・訪問順ライブラリ サンプルコード集

対象は `tsp_solver_v09.hpp` です。問題の意味、全公開API、独立して動く18個の例を順に説明します。C++20の基本的な配列・関数・入出力が分かる読者を想定し、数式の記号は本文で説明します。

## 1. 何を解くライブラリか？

### 1.1 与えられた地点をすべて訪ねる順序を決める

たとえば、既に選ばれた点検地点をロボットがすべて回るとき、どの順で回れば移動コストが小さくなるかを求めます。対象地点の集合は利用者が与え、ライブラリはその要素を増減せず、訪問順だけを決めます。

出発地点へ戻る「閉路」と、最後の地点で終了する「開路」を扱います。開路は始点だけ、終点だけ、両端、またはどちらも固定しない指定ができます。移動コストは方向によって異なってもよく、距離のほかに静的な移動時間や段取りコストを使えます。

ここでの「最小化」は、条件を守る順序の中で費用をできるだけ小さくすることです。記号$\sum$は「指定された範囲の値を全部足す」を表します。添字は訪問順の位置です。

### 1.2 目的関数

入力の地点集合を $V$、その要素数を $n$ とし、全地点をちょうど1回ずつ並べた順序を $P=(v_ {0},\ldots,v_ {n-1})$ とします。開路の目的は次の総コストを最小にすることです。

$$\min_ {P} C_ {\mathrm{path}}(P),\qquad C_ {\mathrm{path}}(P)=\sum_ {i=0}^{n-2}d(v_ {i},v_ {i+1})$$

閉路では最後から先頭への移動を加えます。

$$\min_ {P} C_ {\mathrm{cycle}}(P),\qquad C_ {\mathrm{cycle}}(P)=\sum_ {i=0}^{n-2}d(v_ {i},v_ {i+1})+d(v_ {n-1},v_ {0})$$

| 記号 | 意味 |
|---|---|
| $V$、$n$ | 訪問対象の地点集合と、その数。すべて訪問する |
| $P$ | $V$ の全要素を重複なく並べた順序。最適化の対象 |
| $v_ {i}$、$i$ | 順序の $i$ 番目の地点と、その位置。位置は0から数える |
| $d(u,v)$ | 地点 $u$ から地点 $v$ へ移るコスト。$u,v$ は地点ID |
| $C_ {\mathrm{path}}$ | 戻り辺を数えない開路の総コスト |
| $C_ {\mathrm{cycle}}$ | 最後から先頭へ戻る移動を含めた閉路の総コスト |
| $\min_ {P}$ | 許される全順序の中で総コストを最小化するという目的 |

たとえば順序A→B→Cで、A→Bが2、B→Cが3、C→Aが4なら、開路は2+3=5、閉路は2+3+4=9です。距離を使えば移動距離が短い順序、時間を使えば移動時間が短い順序を探す問題になります。

上の閉路式は $n\ge2$ の場合です。空集合と1地点の場合は、対角コストによらず開路・閉路とも0とします。戻り先の地点を順序の末尾へ重ねて入れる必要はありません。

始点を $s$ に固定するなら $v_ {0}=s$、終点を $t$ に固定する開路なら $v_ {n-1}=t$ の条件を加えます。$s,t$ は入力集合に含まれる指定地点です。2地点以上の開路で両端を固定するなら、異なる地点を指定します。閉路の始点固定は、同じ巡回路をどの地点から表記するかを固定する指定です。

### 1.3 本ライブラリに任せる範囲

「どの地点を選ぶか」「何台に分けるか」「積載容量や時間窓を守るか」は目的関数に含まれません。訪問対象・担当車両を外側で決め、その後の1本の順序最適化に使います。容量・報酬選択・集荷配達を同時に扱う必要があれば、別のroutingライブラリの対象です。

各地点の作業時間が訪問順によらない定数で、全地点を必ず訪ねるなら、その合計はどの順序でも同じです。移動順をこのライブラリで求め、作業時間の合計を呼び出し側で加算できます。時刻や直前の状態によって作業時間が変わる場合は、この切り離しはできません。

## 2. APIリファレンス

### 2.1 読み込み、型、共通の前提

`tsp_solver_v09.hpp` を同じフォルダへ置き、C++20でincludeします。名前空間は `tsp`。標準ライブラリとGCC拡張だけを使い、別ライブラリのリンクは不要です。通常は `.cpp` からincludeします。ヘッダを主ファイルとして直接コンパイルすると、内蔵テスト用mainが有効になります。

自由関数を呼ぶ形式で、探索器のコンストラクタを呼ぶ必要はありません。**ユーザー向けの列挙型はありません**。閉路・開路や固定端点はboolで指定し、正確な意味と有効な組合せは2.3にまとめます。

| 型パラメータ | 省略時 | 意味・取り得る型 |
|---|---|---|
| `T` | orderから推論 | 地点ID。`std::integral`を満たすbool以外の整数型。通常はint。非連続・負のIDも使える |
| `Calc` | `void`。この指定は自動選択を意味する | 距離callbackの戻り値が浮動小数点型ならdouble、それ以外はlong long。明示指定すればその型で計算・返却する |
| `F` | distから推論 | `dist(T from, T to)`として呼べる関数オブジェクト。元ID間の費用を返す |
| `Allocator` | vectorから推論 | vector版のアロケータ型。通常のstd::vectorでよい |

`Calc`は合計と差分が収まる符号付きの計算型にします。たとえば`solve_tsp<__int128_t>`や`solve_tsp<long double>`と明示できます。callbackがlong doubleを返すだけでは、既定の計算型はdoubleです。頂点ID型と費用型は別の指定です。

入力順序の地点IDは重複させず、戻り先を末尾に追加しません。渡す地点数は内部のintで表せる範囲とし、現実には計算時間・メモリにも収めます。distは1回の呼び出し中に変わらない有限値を返します。NaN・無限大や、時刻・積載量・訪問履歴によって変わる費用は使えません。通常は非負の移動費用を想定します。負の辺も計算自体は可能ですが、型の範囲と問題の意味は利用者が確認します。禁止辺・到達不能を表す特別値はなく、巨大な有限値も通常の費用として扱います。

### 2.2 公開関数の全形式

以下の`Calc`は省略できます。`std::span<T>`は書き換え可能な配列の参照、`std::span<const T>`は読み取り専用の参照です。要素をコピーせず、元のvectorやarrayを参照します。spanを使う間は、元の配列を破棄したり、再確保を伴う追加をしたりしないでください。span版は動的な要素数の形式なので、`std::span<int>(array)`のように要素型を明示すると確実です。

| 公開関数と全引数 | 目的・書き換え・戻り値 |
|---|---|
| `total_distance<Calc>(std::span<const T> order, const F& dist, bool cycle=true)` | 現在の順序の費用を合計するだけ。順序は変更せず、Calcの数値を返す |
| `solve_tsp<Calc>(std::span<T> order, const F& dist, const TSPParam& param={}, TSPStats* stats=nullptr)` | 渡した地点集合から順序を構築し、orderを結果に書き換える。結果の費用をCalcで返す |
| `solve_tsp<Calc>(std::vector<T, Allocator>& order, const F& dist, const TSPParam& param={}, TSPStats* stats=nullptr)` | 上のvector版。同じ条件・戻り値 |
| `improve_tsp<Calc>(std::span<T> order, const F& dist, const TSPParam& param={}, TSPStats* stats=nullptr)` | 渡した順序を初期解として改善し、orderを書き換える。改善後の費用をCalcで返す |
| `improve_tsp<Calc>(std::vector<T, Allocator>& order, const F& dist, const TSPParam& param={}, TSPStats* stats=nullptr)` | 上のvector版。同じ条件・戻り値 |
| `held_karp_tsp<Calc>(std::span<T> order, const F& dist, bool cycle=true, bool fixed_start=false, bool fixed_end=false)` | 小規模問題の最適な順序へorderを書き換え、最小費用をCalcで返す |
| `held_karp_tsp<Calc>(std::vector<T, Allocator>& order, const F& dist, bool cycle=true, bool fixed_start=false, bool fixed_end=false)` | 上のvector版。同じ条件・戻り値 |

| 共通引数 | 省略時 | 意味・条件 |
|---|---|---|
| `order` | 省略不可 | 訪問する全IDを1回ずつ入れた配列。solveでは集合、improveでは既存順序として利用。両端固定時は呼び出し前に端点を先頭・末尾へ置く |
| `dist` | 省略不可 | 元IDで移動費用を返すcallback。距離行列・座標計算・事前計算済み最短距離などを使う |
| `param` | `{}` | `TSPParam`の初期値を使う。solve/improveのみ |
| `stats` | `nullptr` | 不要なら省略。ポインタを渡すと呼び出し開始時に全項目をリセットし、今回の統計を格納 |
| `cycle` | true | trueなら閉路で戻り辺を加算、falseなら開路。total_distanceとheld_karp_tspの直接引数 |
| `fixed_start` | false | trueなら入力order.front()を始点として固定。held_karp_tspの直接引数 |
| `fixed_end` | false | trueなら入力order.back()を終点として固定。cycle=falseの場合だけ指定可能 |

どの関数も戻り値は**費用の数値**です。順序が入る結果構造体はありません。solve・improve・held_karpでは、返却後のorderを読む必要があります。空配列・1地点なら費用は0、順序はそのままです。対角の距離は費用へ足しません。

total_distanceにはvectorを直接渡すoverloadがありません。`std::span<const int>(order)`などへ変換します。距離callbackが定数時間なら、n地点の合計計算は地点数に比例する時間、追加の作業メモリは一定量です。

solveとimproveは時間制限付きで良い順序を探し、最適性は保証しません。improveは、現在のdistと端点条件で入力順序を評価し、その値より悪くならないよう最良順序を保持します。距離を変更した場合、変更前の費用より小さくなる保証ではありません。浮動小数点の計算結果には丸め誤差があります。

### 2.3 `TSPParam`の全メンバーと端点指定

公開メンバーを持つ設定型です。`TSPParam param;`または`TSPParam param{};`で次の値になります。

| メンバー | 型 | 初期値 | 意味・取り得る値 |
|---|---|---|---|
| `time_limit_ms` | int | 100 | 0以上の相対時間、単位ms。候補準備・初期構築も含む。0でも全地点を含む順序と費用を返すが、探索に時間を使える指定ではない |
| `seed` | `std::uint64_t` | 1 | 乱数seed。0を含む型の範囲内の整数 |
| `cycle` | bool | true | trueは最後から先頭へ戻る閉路、falseは戻らない開路 |
| `fixed_start` | bool | false | trueは呼び出し前の先頭IDを結果の先頭に固定、falseは始点を指定しない |
| `fixed_end` | bool | false | trueは呼び出し前の末尾IDを結果の末尾に固定、falseは終点を指定しない。cycle=trueとの同時指定は禁止 |
| `symmetric` | bool | true | trueは全対象地点対でdist(u,v)=dist(v,u)を利用者が保証する指定。falseは方向別の費用を使う。自動判定はしない |
| `max_move_evaluations` | `std::uint64_t` | 0 | 0は回数制限なし。正の値は探索候補の評価回数上限。時間制限も同時に有効 |

| 解きたい形 | cycle | fixed_start | fixed_end | 入力orderの準備 |
|---|---|---|---|---|
| 始点を指定しない閉路 | true | false | false | 全IDを1回ずつ置く |
| 表示する始点を指定する閉路 | true | true | false | 指定IDを先頭に置く |
| 両端自由の開路 | false | false | false | 全IDを1回ずつ置く |
| 始点だけ固定する開路 | false | true | false | 指定始点を先頭に置く |
| 終点だけ固定する開路 | false | false | true | 指定終点を末尾に置く |
| 両端固定の開路 | false | true | true | 指定始点・終点を先頭・末尾に置く。2地点以上なら両者は別ID |

固定フラグは「0番のIDを固定する」という指定ではありません。呼び出す瞬間の配列端のIDを固定します。閉路は同じ巡回をどの地点から表示しても費用が同じですが、非対称距離では向きを逆にすると費用が変わる場合があります。

時計確認は処理の区切りで行うため、time_limit_msは厳密な実時間上限ではありません。評価回数もcallbackの呼び出し数や全処理量ではありません。同じseed・評価上限でも、時間が先に尽きれば結果は変わり得ます。絶対deadlineを受け取るAPIはありません。全体の残り時間を管理する場合は、呼び出し側で相対時間へ換算します。

### 2.4 厳密解`held_karp_tsp`の利用条件

入力頂点数nは**0以上22以下**です。固定端点を含めた総数に上限がかかります。bool引数は2.3と同じ意味ですが、TSPParam・stats・time_limit_ms・seed・symmetricは受け取りません。方向別の距離をそのまま使うので、対称・非対称の両方に対応します。閉路ではfixed_start=falseでも入力の先頭を先頭として残します。巡回の表示開始位置だけの違いなので、最小費用の条件は変わりません。

全組合せを効率よく比較する計算で、地点が増えると必要量が急増します。次はCalc=long long、入力22地点で、主要な計算用配列だけのメモリ目安です。

| 形 | メモリ目安 |
|---|---|
| 閉路 | 約378 MiB |
| 両端自由の開路 | 約792 MiB |
| 片端固定の開路 | 約378 MiB |
| 両端固定の開路 | 約180 MiB |

時間制限・中断して暫定解を返す機能はありません。22以下でも実行環境に収まるとは限らないため、T13のように利用側で小さめの切替上限を決めます。整数の正確な演算範囲内では最適解を得られます。浮動小数点では演算誤差を含む比較になります。容量、時間窓、集荷の先行条件などを自動で検査する関数ではありません。

### 2.5 `TSPStats`の全メンバー

| メンバー | 型 | 初期値 | 意味 |
|---|---|---|---|
| `move_evaluations` | uint64_t | 0 | 探索候補を評価した回数 |
| `accepted_moves` | uint64_t | 0 | 局所探索で採用した改善変更数 |
| `local_search_runs` | uint64_t | 0 | 近くの順序を調べて改善する探索の実行数 |
| `kicks` | uint64_t | 0 | 行き詰まりから離れるために順序を崩した回数 |
| `improved_kicks` | uint64_t | 0 | 順序を崩して再探索した結果、最良解を更新した回数 |
| `exact_repairs` | uint64_t | 0 | 小区間を厳密に並べ直す処理の実行数 |
| `improved_exact_repairs` | uint64_t | 0 | 小区間の厳密な並べ直しで改善した回数 |
| `candidate_build_ms` | double | 0 | 近傍候補を準備した時間ms |
| `construction_ms` | double | 0 | 初期構築区間の計測時間ms。improveでは新規構築しないが、時計の計測による微小な時間が記録される場合がある |
| `search_ms` | double | 0 | 候補準備・初期構築以後の探索時間ms |
| `total_ms` | double | 0 | 呼び出し全体で計測した時間ms |

表のuint64_tは`std::uint64_t`です。`TSPStats stats;`で全項目0になります。solve・improveへ渡すと毎回リセットされるため、複数回の合計が必要なら呼び出し側で加算します。厳密解関数はstatsを受け取りません。

### 2.6 デバッグ表示と再利用の範囲

`std::ostream& operator<<(std::ostream& os, const TSPStats& stats)`は全統計を名前付きで1行表示し、osを返します。`std::cerr << stats << '\n';`と書けます。改行・flush・ストリームの書式変更は行いません。統計に配列はありません。費用は数値なので通常どおり出力し、順序はfor文などで出力します。__int128の費用をstd::coutへ直接渡す標準の演算子はないため、T15では十進文字列へ変換します。

前のorderはimproveで再利用できます。訪問済み地点を削除したり、新規地点を重複なく追加したりした後も、固定端点を正しい位置に置き直せば渡せます。距離を共有保存する公開Contextや、探索途中の状態を再開するAPIはありません。距離を事前計算している場合、その表は利用側で保持します。

地点選択、複数車両への割当て、積載制約、時間窓、集荷配達の前後関係は直接指定できません。assertを無効にしても入力条件が緩和されるわけではありません。

## 3. ユースケース

### 3.1 コピーして実行する手順

各例はmain関数を含む完全なプログラムです。**1例ずつ**コード全体を`sample.cpp`へ、入力例を`input.txt`へ保存し、同じフォルダに`tsp_solver_v09.hpp`を置きます。他の例のコードを先にコピーする必要はありません。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined sample.cpp -o sample
./sample < input.txt
```

例は記載した入力条件を満たすデータを対象とします。入力行の空白と改行は同じ区切りとして扱います。各コードの先頭にも用途・入力形式・注意事項を記載しているので、単独で持ち出してカスタマイズできます。

掲載する出力例は、実際の実行から得た標準出力です。時間制限付き探索では、環境や打ち切り時刻により同じ入力でも順序が変わる場合があります。統計を出す例では標準エラーに別途表示されますが、実測時間は環境に依存するため掲載していません。仕事・車両・地点の添字は0始まりです。

### 3.2 例の一覧

| 例 | 解く問題・利用場面 | 主に使うAPI |
|---|---|---|
| T01 | 平面の地点をすべて回って戻る | `solve_tsp` |
| T02 | 始点・終点を自由にした片道の訪問順を求める | `solve_tsp` |
| T03 | 出発地と到着地の両方が決まった片道経路 | `solve_tsp` |
| T04 | 出発地だけ固定し、終了地点を自由にする | `solve_tsp` |
| T05 | 終了地点だけ固定し、出発地点を自由にする | `solve_tsp` |
| T06 | 指定した拠点を先頭にした巡回路を得る | `solve_tsp` |
| T07 | 作業の順番で変わる段取り費用を最小化する | `solve_tsp` |
| T08 | 道路グラフ上の重要地点だけを巡回する | `solve_tsp` |
| T09 | 手元の順序を使って再計画する | `total_distance`、`improve_tsp` |
| T10 | 担当分割後の車両別ルートを改善する | `solve_tsp` |
| T11 | 小規模な巡回路の最適解を求める | `held_karp_tsp` |
| T12 | 両端固定の片道経路を厳密に解く | `held_karp_tsp` |
| T13 | 問題の大きさで厳密解と時間制限付き探索を切り替える | `held_karp_tsp`、`solve_tsp` |
| T14 | ユークリッド距離を小数のまま最小化する | `solve_tsp`、`total_distance` |
| T15 | 64bitを超える費用の訪問順を求める | `solve_tsp`、`total_distance` |
| T16 | 固定長配列の一部分をそのまま並べ替える | `solve_tsp`、`improve_tsp` |
| T17 | 複数seedの結果から最小費用を選ぶ | `solve_tsp` |
| T18 | 既存の順序を採点するだけに使う | `total_distance` |

### T01 平面の地点をすべて回って戻る

5地点をすべて1回ずつ訪ね、最後に出発地点へ戻ります。縦横にしか移動できない街を想定し、横方向の差と縦方向の差の合計を距離にします。

orderの末尾に先頭IDを追加してはいけません。閉路の戻り辺は自動的に費用へ含まれます。固定していない先頭IDが特定の地点になる保証はありません。

**入力形式**

先頭行は地点数n、続くn行は整数座標x y。各座標は絶対値10の9乗以下、nは0以上10000以下を例の入力範囲とします。

**実行コード**

```cpp
// T01: 平面の地点をすべて回って戻る
//
// 解く問題:
// 5地点をすべて1回ずつ訪ね、最後に出発地点へ戻ります。
// 縦横にしか移動できない街を想定し、横方向の差と縦方向の差の合計を距離にします。
//
// 入力:
// 先頭行は地点数n、続くn行は整数座標x y。
// 各座標は絶対値10の9乗以下、nは0以上10000以下を例の入力範囲とします。
//
// 注意:
// orderの末尾に先頭IDを追加してはいけません。
// 閉路の戻り辺は自動的に費用へ含まれます。
// 固定していない先頭IDが特定の地点になる保証はありません。

// TSPの公開API。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<long long> x(n), y(n);
    for (int i = 0; i < n; ++i) {
        std::cin >> x[i] >> y[i];
    }

    // 頂点IDは入力座標の添字。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 非負・対称のマンハッタン距離。
    const auto dist = [&](int a, int b) {
        return std::llabs(x[a] - x[b]) + std::llabs(y[a] - y[b]);
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // cycleの既定trueを使い、全地点の閉路を作る。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5
0 0
2 0
2 2
0 2
1 1
```

**出力例**

```text
cost 10
order 5 0 1 2 3 4
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T02 始点・終点を自由にした片道の訪問順を求める

4地点を全部訪問しますが、出発地点へ戻る必要はありません。どこから出発し、どこで終えるかも含めて決めます。

閉路と開路では目的値が違います。採点し直す際も、total_distanceのcycleへfalseを渡します。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 この例の行列は対称にします。

**実行コード**

```cpp
// T02: 始点・終点を自由にした片道の訪問順を求める
//
// 解く問題:
// 4地点を全部訪問しますが、出発地点へ戻る必要はありません。
// どこから出発し、どこで終えるかも含めて決めます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// この例の行列は対称にします。
//
// 注意:
// 閉路と開路では目的値が違います。
// 採点し直す際も、total_distanceのcycleへfalseを渡します。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 最後から先頭へ戻る費用を数えない開路。
    param.cycle = false;

    // 両端の固定は既定false。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
```

**出力例**

```text
cost 6
order 4 0 1 2 3
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T03 出発地と到着地の両方が決まった片道経路

頂点2を出発し、全地点を通って頂点1で終了する順序を求めます。

固定フラグへ地点IDを代入するのではありません。先にorderの端へIDを配置してから、対応するboolをtrueにします。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。最後にstart finishを入力します。

**実行コード**

```cpp
// T03: 出発地と到着地の両方が決まった片道経路
//
// 解く問題:
// 頂点2を出発し、全地点を通って頂点1で終了する順序を求めます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
// 最後にstart finishを入力します。
//
// 注意:
// 固定フラグへ地点IDを代入するのではありません。
// 先にorderの端へIDを配置してから、対応するboolをtrueにします。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 固定する始点の元ID。
    int start;
    std::cin >> start;

    // 固定する終点の元ID。
    int finish;
    std::cin >> finish;
    if (n == 0 || start < 0 || start >= n || finish < 0 || finish >= n
        || (n > 1 && start == finish)) {
        std::cout << "INVALID_ENDPOINT\n";
        return 0;
    }

    // fixed_startは呼び出し時の先頭IDを固定するので先に置く。
    std::iter_swap(order.begin(), std::find(order.begin(), order.end(), start));

    // fixed_endは呼び出し時の末尾IDを固定するので先に置く。
    std::iter_swap(order.end() - 1, std::find(order.begin(), order.end(), finish));

    // 終点を持つ片道経路にする。
    param.cycle = false;

    // 現在のorder.front()を先頭に保つ。
    param.fixed_start = true;

    // 現在のorder.back()を末尾に保つ。
    // cycle=trueでは使用不可。
    param.fixed_end = true;

    // 固定した端点を保ちながら全地点を並べる。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
2 1
```

**出力例**

```text
cost 11
order 4 2 3 0 1
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T04 出発地だけ固定し、終了地点を自由にする

現在地である頂点2から出発し、残り地点を全部訪ねます。最後の地点は探索に任せます。

固定フラグへ地点IDを代入するのではありません。先にorderの端へIDを配置してから、対応するboolをtrueにします。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。最後にstartを入力します。

**実行コード**

```cpp
// T04: 出発地だけ固定し、終了地点を自由にする
//
// 解く問題:
// 現在地である頂点2から出発し、残り地点を全部訪ねます。
// 最後の地点は探索に任せます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
// 最後にstartを入力します。
//
// 注意:
// 固定フラグへ地点IDを代入するのではありません。
// 先にorderの端へIDを配置してから、対応するboolをtrueにします。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 固定する始点の元ID。
    int start;
    std::cin >> start;
    if (n == 0 || start < 0 || start >= n) {
        std::cout << "INVALID_ENDPOINT\n";
        return 0;
    }

    // fixed_startは呼び出し時の先頭IDを固定するので先に置く。
    std::iter_swap(order.begin(), std::find(order.begin(), order.end(), start));

    // 終点を持つ片道経路にする。
    param.cycle = false;

    // 現在のorder.front()を先頭に保つ。
    param.fixed_start = true;

    // 固定した端点を保ちながら全地点を並べる。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
2
```

**出力例**

```text
cost 9
order 4 2 1 0 3
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T05 終了地点だけ固定し、出発地点を自由にする

最後に頂点1へ到着することだけを固定し、その前に全地点を訪れる順序を求めます。

固定フラグへ地点IDを代入するのではありません。先にorderの端へIDを配置してから、対応するboolをtrueにします。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。最後にfinishを入力します。

**実行コード**

```cpp
// T05: 終了地点だけ固定し、出発地点を自由にする
//
// 解く問題:
// 最後に頂点1へ到着することだけを固定し、その前に全地点を訪れる順序を求めます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
// 最後にfinishを入力します。
//
// 注意:
// 固定フラグへ地点IDを代入するのではありません。
// 先にorderの端へIDを配置してから、対応するboolをtrueにします。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 固定する終点の元ID。
    int finish;
    std::cin >> finish;
    if (n == 0 || finish < 0 || finish >= n) {
        std::cout << "INVALID_ENDPOINT\n";
        return 0;
    }

    // fixed_endは呼び出し時の末尾IDを固定するので先に置く。
    std::iter_swap(order.end() - 1, std::find(order.begin(), order.end(), finish));

    // 終点を持つ片道経路にする。
    param.cycle = false;

    // 現在のorder.back()を末尾に保つ。
    // cycle=trueでは使用不可。
    param.fixed_end = true;

    // 固定した端点を保ちながら全地点を並べる。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
1
```

**出力例**

```text
cost 8
order 4 3 2 0 1
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T06 指定した拠点を先頭にした巡回路を得る

頂点2を出発・帰着拠点として、全地点を回る閉路を求めます。返される順序の先頭を常に2へ揃えます。

閉路ではfixed_end=trueを使えません。始点固定は巡回路の表記を揃える指定であり、途中の頂点を固定する指定ではありません。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。最後に固定する始点IDを入力します。

**実行コード**

```cpp
// T06: 指定した拠点を先頭にした巡回路を得る
//
// 解く問題:
// 頂点2を出発・帰着拠点として、全地点を回る閉路を求めます。
// 返される順序の先頭を常に2へ揃えます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
// 最後に固定する始点IDを入力します。
//
// 注意:
// 閉路ではfixed_end=trueを使えません。
// 始点固定は巡回路の表記を揃える指定であり、途中の頂点を固定する指定ではありません。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 固定する始点の元ID。
    int start;
    std::cin >> start;
    if (n == 0 || start < 0 || start >= n) {
        std::cout << "INVALID_ENDPOINT\n";
        return 0;
    }

    // fixed_startは呼び出し時の先頭IDを固定するので先に置く。
    std::iter_swap(order.begin(), std::find(order.begin(), order.end(), start));

    // 先頭へ置いた拠点を固定。
    // cycleは既定trueのまま。
    param.fixed_start = true;

    // 最後から拠点へ戻る費用も数える。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
2
```

**出力例**

```text
cost 12
order 4 2 1 0 3
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T07 作業の順番で変わる段取り費用を最小化する

作業Aの後にBを行う費用と、Bの後にAを行う費用が異なる4作業を並べます。全作業後に最初の作業の状態へ戻す、繰り返し生産の1周期を想定します。

非対称な費用でsymmetric=trueを使わないでください。三角不等式は不要ですが、実行中に費用が変わるcallbackは使えません。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。

**実行コード**

```cpp
// T07: 作業の順番で変わる段取り費用を最小化する
//
// 解く問題:
// 作業Aの後にBを行う費用と、Bの後にAを行う費用が異なる4作業を並べます。
// 全作業後に最初の作業の状態へ戻す、繰り返し生産の1周期を想定します。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
//
// 注意:
// 非対称な費用でsymmetric=trueを使わないでください。
// 三角不等式は不要ですが、実行中に費用が変わるcallbackは使えません。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 順方向と逆方向の段取り費用が違う。
    param.symmetric = false;

    // 非対称の閉路を求める。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 1 8 8
8 0 1 8
8 8 0 1
1 8 8 0
```

**出力例**

```text
cost 4
order 4 0 1 2 3
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T08 道路グラフ上の重要地点だけを巡回する

道路全体には5頂点ありますが、作業が必要なのは0・2・4だけです。例側で道路の最短路を計算し、重要地点の巡回順と、通過点を含む実際の移動列を出します。

最短路の途中で別の重要地点を通過しても、訪問順の作業とは区別します。全道路頂点を必ず1回ずつ通る問題ではありません。例は連結な無向グラフです。

**入力形式**

先頭行は `V E N`（道路頂点数、道路数、重要地点数）。E行の `a b cost` の後にN個の重要地点IDを入力します。無向辺の費用は非負で、有限の最短路は2の60乗未満です。

**実行コード**

```cpp
// T08: 道路グラフ上の重要地点だけを巡回する
//
// 解く問題:
// 道路全体には5頂点ありますが、作業が必要なのは0・2・4だけです。
// 例側で道路の最短路を計算し、重要地点の巡回順と、通過点を含む実際の移動列を出します。
//
// 入力:
// 先頭行は `V E N`（道路頂点数、道路数、重要地点数）。
// E行の `a b cost` の後にN個の重要地点IDを入力します。
// 無向辺の費用は非負で、有限の最短路は2の60乗未満です。
//
// 注意:
// 最短路の途中で別の重要地点を通過しても、訪問順の作業とは区別します。
// 全道路頂点を必ず1回ずつ通る問題ではありません。
// 例は連結な無向グラフです。

// 重要地点の巡回順を求めるAPI。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int vertex_count, edge_count, visit_count;
    if (!(std::cin >> vertex_count >> edge_count >> visit_count)) {
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

    // 重要地点の元IDを重複なしで指定する。
    std::vector<int> order(visit_count);
    for (int& v : order) {
        std::cin >> v;
    }

    // 事前計算した有限の最短路費用。
    const auto dist = [&](int a, int b) {
        return distance[a][b];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 重要地点だけを並べた閉路を返す。
    const auto cost = tsp::solve_tsp(order, dist, param, &stats);

    // orderには通過点は含まれない。
    print_tour(cost, order);

    std::vector<int> walk;

    // 空の入力も扱い、frontへの不正アクセスを避ける。
    if (!order.empty()) {
        walk.push_back(order.front());
        for (std::size_t i = 1; i <= order.size(); ++i) {
            // 最後は先頭へ戻る。
            const int target = order[i % order.size()];
            while (walk.back() != target) {
                walk.push_back(next[walk.back()][target]);
            }
        }
    }
    std::cout << "walk " << walk.size();
    for (int v : walk) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';

    // 探索の統計を確認する。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5 5 3
0 1 2
1 2 2
2 3 2
3 4 2
0 4 20
0 2 4
```

**出力例**

```text
cost 16
order 3 0 2 4
walk 9 0 1 2 3 4 3 2 1 0
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。 最後の `walk 件数 頂点列` は道路上の移動列です。非連結入力では `UNREACHABLE` と表示します。

### T09 手元の順序を使って再計画する

既にある訪問順0→3→1→2を、現在の距離で評価してから改善します。距離が前ターンから変わった場合も、現在の距離を使って呼び直せます。

入力順序から改善したいときはimprove_tspを使います。solve_tspは初期構築で順序を作り直します。いずれも距離を実行途中で変えてはいけません。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。最後にn個のIDを現在の訪問順で入力します。

**実行コード**

```cpp
// T09: 手元の順序を使って再計画する
//
// 解く問題:
// 既にある訪問順0→3→1→2を、現在の距離で評価してから改善します。
// 距離が前ターンから変わった場合も、現在の距離を使って呼び直せます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
// 最後にn個のIDを現在の訪問順で入力します。
//
// 注意:
// 入力順序から改善したいときはimprove_tspを使います。
// solve_tspは初期構築で順序を作り直します。
// いずれも距離を実行途中で変えてはいけません。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 今ある訪問順を入力。
    // 全IDの重複なしの順列にする。
    for (int& v : order) {
        std::cin >> v;
    }

    // 既定cycle=trueで現在の閉路費用を採点する。
    const auto before = tsp::total_distance(std::span<const int>(order), dist);
    std::cout << "before " << before << '\n';

    // 同じ距離で既存順序を改善し、orderを書き換える。
    const auto cost = tsp::improve_tsp(order, dist, param, &stats);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
0 3 1 2
```

**出力例**

```text
before 14
cost 12
order 4 0 1 3 2
```

最初の `before` は入力順序の費用です。標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T10 担当分割後の車両別ルートを改善する

配送担当は既に2グループへ分かれていて、担当を変更せず各グループ内の訪問順だけを決めたい場面です。この例は各グループを独立した開路として解きます。

グループ間の頂点移動はしません。数量・予算・集荷先行などをTSPは検査しないため、これらの制約が必要な経路を自由に並べ替えないでください。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 対称行列の後にグループ数G、各グループについて件数kとk個のIDを入力します。

**実行コード**

```cpp
// T10: 担当分割後の車両別ルートを改善する
//
// 解く問題:
// 配送担当は既に2グループへ分かれていて、担当を変更せず各グループ内の訪問順だけを決めた
// い場面です。
// この例は各グループを独立した開路として解きます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 対称行列の後にグループ数G、各グループについて件数kとk個のIDを入力します。
//
// 注意:
// グループ間の頂点移動はしません。
// 数量・予算・集荷先行などをTSPは検査しないため、
// これらの制約が必要な経路を自由に並べ替えないでください。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    int groups;
    std::cin >> groups;

    // 各グループは戻り辺のない開路として解く。
    param.cycle = false;
    for (int g = 0; g < groups; ++g) {
        int count;
        std::cin >> count;
        // 今回担当するIDだけを渡す。
        order.resize(static_cast<std::size_t>(count));
        // グループ内で重複しない元ID。
        for (int& v : order) {
            std::cin >> v;
        }
        // それ以外の頂点は訪問対象にならない。
        const auto cost = tsp::solve_tsp(order, dist, param, &stats);
        // このグループの結果だけを出す。
        std::cout << "group " << g << '\n';
        print_tour(cost, order);
        // 呼び出しごとにリセットされた統計。
        std::cerr << stats << '\n';
    }
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
2
2 0 2
2 1 3
```

**出力例**

```text
group 0
cost 3
order 2 2 0
group 1
cost 4
order 2 3 1
```

各 `group 番号` に続いて、そのグループの `cost` と `order` を出します。空グループの費用は0です。

### T11 小規模な巡回路の最適解を求める

4地点の閉路について、近似ではなく最小費用を求めます。小さな入力の正解作成や、他の解法の品質確認にも使えます。

held_karp_tspの上限は入力全体で22頂点です。時間制限パラメータはなく、22頂点では大量の時間・メモリが必要です。APIリファレンスの資源量を確認してください。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 nは22以下。非対称行列も使えます。

**実行コード**

```cpp
// T11: 小規模な巡回路の最適解を求める
//
// 解く問題:
// 4地点の閉路について、近似ではなく最小費用を求めます。
// 小さな入力の正解作成や、他の解法の品質確認にも使えます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// nは22以下。
// 非対称行列も使えます。
//
// 注意:
// held_karp_tspの上限は入力全体で22頂点です。
// 時間制限パラメータはなく、22頂点では大量の時間・メモリが必要です。
// APIリファレンスの資源量を確認してください。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // ライブラリの頂点数上限を守る。
    if (n > 22) {
        std::cout << "TOO_LARGE\n";
        return 0;
    }

    // 既定は閉路。
    // 厳密最適費用を返し、orderも最適順序になる。
    const auto cost = tsp::held_karp_tsp(order, dist);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
```

**出力例**

```text
cost 12
order 4 0 3 2 1
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。この例のAPIは統計を受け取らず、標準エラーへの統計出力はありません。

### T12 両端固定の片道経路を厳密に解く

頂点2を出発し頂点1で終わる、全地点を通る片道経路の最小費用を求めます。配列の参照を渡すspan形式も示します。

held_karp_tspにはsymmetric引数はありません。渡された両方向の距離を使います。第3引数以降のboolはcycle、fixed_start、fixed_endの順です。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 nは22以下。最後にstart finishを入力します。

**実行コード**

```cpp
// T12: 両端固定の片道経路を厳密に解く
//
// 解く問題:
// 頂点2を出発し頂点1で終わる、全地点を通る片道経路の最小費用を求めます。
// 配列の参照を渡すspan形式も示します。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// nは22以下。
// 最後にstart finishを入力します。
//
// 注意:
// held_karp_tspにはsymmetric引数はありません。
// 渡された両方向の距離を使います。
// 第3引数以降のboolはcycle、fixed_start、fixed_endの順です。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 固定する始点の元ID。
    int start;
    std::cin >> start;

    // 固定する終点の元ID。
    int finish;
    std::cin >> finish;
    if (n == 0 || start < 0 || start >= n || finish < 0 || finish >= n
        || (n > 1 && start == finish)) {
        std::cout << "INVALID_ENDPOINT\n";
        return 0;
    }

    // fixed_startは呼び出し時の先頭IDを固定するので先に置く。
    std::iter_swap(order.begin(), std::find(order.begin(), order.end(), start));

    // fixed_endは呼び出し時の末尾IDを固定するので先に置く。
    std::iter_swap(order.end() - 1, std::find(order.begin(), order.end(), finish));

    // 追加した固定端点も含む総頂点数の上限。
    if (n > 22) {
        std::cout << "TOO_LARGE\n";
        return 0;
    }

    // 書き換え可能なspan。
    // 関数実行中は参照元のvectorを動かさない。
    const std::span<int> route(order);

    // 開路・始点固定・終点固定の順に指定する。
    const auto cost = tsp::held_karp_tsp(route, dist, false, true, true);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
2 1
```

**出力例**

```text
cost 11
order 4 2 3 0 1
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。この例のAPIは統計を受け取らず、標準エラーへの統計出力はありません。

### T13 問題の大きさで厳密解と時間制限付き探索を切り替える

小さい部分問題は厳密に、大きい部分問題は時間制限付きで解く入口を例側に作ります。閾値は使える時間とメモリに合わせて決めます。

この切り替え自体は例側の処理です。厳密解を選んだ場合、TSPParamのtime_limit_msは適用されません。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。最後に例側のexact_limitを入力します。

**実行コード**

```cpp
// T13: 問題の大きさで厳密解と時間制限付き探索を切り替える
//
// 解く問題:
// 小さい部分問題は厳密に、大きい部分問題は時間制限付きで解く入口を例側に作ります。
// 閾値は使える時間とメモリに合わせて決めます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
// 最後に例側のexact_limitを入力します。
//
// 注意:
// この切り替え自体は例側の処理です。
// 厳密解を選んだ場合、TSPParamのtime_limit_msは適用されません。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 例側の閾値。
    // 0以上22以下にする。
    int exact_limit;
    std::cin >> exact_limit;
    if (exact_limit < 0 || exact_limit > 22) {
        std::cout << "INVALID_LIMIT\n";
        return 0;
    }

    long long cost;
    if (n <= exact_limit) {
        // 小さい入力を厳密に解く。
        // paramやstatsは受け取らない。
        cost = tsp::held_karp_tsp(order, dist);
        std::cout << "method exact\n";
    } else {
        // 大きい入力に時間・評価回数の上限を適用する。
        cost = tsp::solve_tsp(order, dist, param, &stats);
        std::cout << "method heuristic\n";
    }

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
8
```

**出力例**

```text
method exact
cost 12
order 4 0 3 2 1
```

`method exact` または `method heuristic` で選んだ入口を示します。標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T14 ユークリッド距離を小数のまま最小化する

平面の点を直線移動できる場合の巡回路を求めます。座標差の二乗和の平方根を、整数へ丸めず使います。

浮動小数点では足し合わせる順番で微小な差が出ます。結果の検査は完全一致ではなく、値の大きさに合った許容誤差で行います。

**入力形式**

先頭行は地点数n、続くn行は小数座標x y。NaN・無限大を含めず、距離と総和がdoubleの有限範囲に収まる入力にします。

**実行コード**

```cpp
// T14: ユークリッド距離を小数のまま最小化する
//
// 解く問題:
// 平面の点を直線移動できる場合の巡回路を求めます。
// 座標差の二乗和の平方根を、整数へ丸めず使います。
//
// 入力:
// 先頭行は地点数n、続くn行は小数座標x y。
// NaN・無限大を含めず、距離と総和がdoubleの有限範囲に収まる入力にします。
//
// 注意:
// 浮動小数点では足し合わせる順番で微小な差が出ます。
// 結果の検査は完全一致ではなく、値の大きさに合った許容誤差で行います。

// 小数費用にも対応するTSPのAPI。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<double> x(n), y(n);
    for (int i = 0; i < n; ++i) {
        std::cin >> x[i] >> y[i];
    }

    // 頂点IDは座標配列の添字。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // doubleの有限な直線距離を返す。
    const auto dist = [&](int a, int b) {
        return std::hypot(x[a] - x[b], y[a] - y[b]);
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // doubleを明示。
    // double callbackなら省略時もdouble。
    const auto cost = tsp::solve_tsp<double>(order, dist, param, &stats);

    // 同じ型・閉路の条件で費用を再計算する。
    const auto checked =
        tsp::total_distance<double>(std::span<const int>(order), dist, true);
    if (std::abs(cost - checked) > 1e-9 * std::max(1.0, std::abs(checked))) {
        return 1;
    }
    std::cout << std::fixed << std::setprecision(9);

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
5
0 0
2 0
2 2
0 2
1 1
```

**出力例**

```text
cost 8.828427125
order 5 0 1 2 3 4
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T15 64bitを超える費用の訪問順を求める

整数距離を2の70乗倍した閉路を計算します。費用合計がlong longに収まらない場合も、128bit整数の範囲内で正確に扱います。

Calcを明示しないと、整数callbackの既定計算型はlong longです。再計算するtotal_distanceにも同じ型を明示します。この例の十進変換関数は非負値専用です。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。入力距離を2の70乗倍して使い、全中間値を符号付き128bitの範囲内にします。

**実行コード**

```cpp
// T15: 64bitを超える費用の訪問順を求める
//
// 解く問題:
// 整数距離を2の70乗倍した閉路を計算します。
// 費用合計がlong longに収まらない場合も、128bit整数の範囲内で正確に扱います。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
// 入力距離を2の70乗倍して使い、全中間値を符号付き128bitの範囲内にします。
//
// 注意:
// Calcを明示しないと、整数callbackの既定計算型はlong longです。
// 再計算するtotal_distanceにも同じ型を明示します。
// この例の十進変換関数は非負値専用です。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

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

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << decimal(cost) << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return static_cast<__int128_t>(distance[from][to]) * (__int128_t{1} << 70);
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 大きな費用に合わせて計算型を明示する。
    const auto cost = tsp::solve_tsp<__int128_t>(order, dist, param, &stats);

    // 再計算も128bitに揃える。
    const auto checked =
        tsp::total_distance<__int128_t>(std::span<const int>(order), dist);
    if (cost != checked) {
        return 1;
    }

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
```

**出力例**

```text
cost 14167099448608935641088
order 4 0 1 2 3
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T16 固定長配列の一部分をそのまま並べ替える

最大8個の非連続な整数IDをarrayへ保存し、先頭n個だけを訪問対象にします。vectorを別に作らず、spanから構築と改善を呼びます。

spanは配列を所有しません。参照元が有効な間だけ使います。n=0も有効で、費用0・空の順序になります。

**入力形式**

先頭にn（0以上8以下）、次に重複しないn個のint型ID。負のID・非連続IDも使えます。

**実行コード**

```cpp
// T16: 固定長配列の一部分をそのまま並べ替える
//
// 解く問題:
// 最大8個の非連続な整数IDをarrayへ保存し、先頭n個だけを訪問対象にします。
// vectorを別に作らず、spanから構築と改善を呼びます。
//
// 入力:
// 先頭にn（0以上8以下）、次に重複しないn個のint型ID。
// 負のID・非連続IDも使えます。
//
// 注意:
// spanは配列を所有しません。
// 参照元が有効な間だけ使います。
// n=0も有効で、費用0・空の順序になります。

// spanを受け取るTSPのAPI。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }
    if (n < 0 || n > 8) {
        std::cout << "INVALID_SIZE\n";
        return 0;
    }
    std::array<int, 8> storage{};
    for (int i = 0; i < n; ++i) {
        std::cin >> storage[i];
    }

    // 書き換える範囲を先頭n個だけに限定する。
    const std::span<int> order(storage.data(), static_cast<std::size_t>(n));

    // 元IDの差を費用とする。
    const auto dist = [](int a, int b) {
        return std::llabs(static_cast<long long>(a) - b);
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 戻り辺のない開路として解く。
    param.cycle = false;

    // spanの参照先であるarrayの中身が書き換わる。
    const auto initial_cost = tsp::solve_tsp(order, dist, param, &stats);

    // 同じspanを渡して既存順序を改善する。
    const auto cost = tsp::improve_tsp(order, dist, param, &stats);
    if (cost > initial_cost) {
        return 1;
    }

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);

    // 全統計を名前付きで表示する。
    // 改行は自分で付ける。
    std::cerr << stats << '\n';
}
```

**入力例**

```text
4
40 -3 10 25
```

**出力例**

```text
cost 43
order 4 -3 10 25 40
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T17 複数seedの結果から最小費用を選ぶ

同じ頂点集合を3回解き、最も短かった巡回路を保存します。探索を増やしたい場合の外側の組み立て方です。

毎回元の頂点集合をコピーし、最良費用と最良順序を一緒に保存します。全試行の合計時間は例側で管理します。統計は各試行ごとにリセットされます。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 行列は対称。

**実行コード**

```cpp
// T17: 複数seedの結果から最小費用を選ぶ
//
// 解く問題:
// 同じ頂点集合を3回解き、最も短かった巡回路を保存します。
// 探索を増やしたい場合の外側の組み立て方です。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 行列は対称。
//
// 注意:
// 毎回元の頂点集合をコピーし、最良費用と最良順序を一緒に保存します。
// 全試行の合計時間は例側で管理します。
// 統計は各試行ごとにリセットされます。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 既定値は100ms、閉路、両端自由、対称距離、seed=1。
    tsp::TSPParam param;

    // 例では評価上限を先に使うため長めに設定。
    // 本番は残り時間に合わせる。
    param.time_limit_ms = 1000;

    // 探索候補の評価数を制限。
    // 0なら回数上限なし。
    param.max_move_evaluations = 3000;

    // solve/improveの最後の引数へ渡すと、その呼び出しの統計が入る。
    tsp::TSPStats stats;

    // 初期の頂点集合を保存し、各試行の入力を揃える。
    const auto vertices = order;

    long long cost = std::numeric_limits<long long>::max();

    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        auto candidate = vertices;
        // 探索の乱数seedだけを変える。
        param.seed = seed;
        // candidateが今回の訪問順へ書き換わる。
        const auto value = tsp::solve_tsp(candidate, dist, param, &stats);
        // 費用と順序の対応を保って保存する。
        if (seed == 1 || value < cost) {
            cost = value;
            order = std::move(candidate);
        }
        // この試行だけの統計を確認する。
        std::cerr << stats << '\n';
    }

    // 費用は戻り値、経路は書き換えられたorderから受け取る。
    print_tour(cost, order);
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
```

**出力例**

```text
cost 12
order 4 0 1 2 3
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。統計は標準エラーへ出します。

### T18 既存の順序を採点するだけに使う

探索はせず、渡した訪問順の合計費用だけを求めます。自作ソルバーや別処理の出力を、このライブラリと同じ数え方で評価したいときに使えます。

total_distanceは経路を変更しません。容量、先行順序、全地点の訪問などの制約を検査する関数でもありません。0・1頂点では自己辺を数えず0を返します。

**入力形式**

先頭行は頂点数n、次のn行はn×nの距離行列です。頂点IDは0からn−1。距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。 最後にn個の訪問順IDと、cycle（0または1）を入力します。

**実行コード**

```cpp
// T18: 既存の順序を採点するだけに使う
//
// 解く問題:
// 探索はせず、渡した訪問順の合計費用だけを求めます。
// 自作ソルバーや別処理の出力を、このライブラリと同じ数え方で評価したいときに使えます。
//
// 入力:
// 先頭行は頂点数n、次のn行はn×nの距離行列です。
// 頂点IDは0からn−1。
// 距離は非負の有限値で、整数例では辺・総和・差分が `long long` に収まる入力を使います。
// 最後にn個の訪問順IDと、cycle（0または1）を入力します。
//
// 注意:
// total_distanceは経路を変更しません。
// 容量、先行順序、全地点の訪問などの制約を検査する関数でもありません。
// 0・1頂点では自己辺を数えず0を返します。

// TSPの公開APIと標準ライブラリ。
#include "tsp_solver_v09.hpp"

// 例側の出力関数。
// costはライブラリの戻り値、orderは書き換え後の訪問順。
template <class Cost>
void print_tour(Cost cost, std::span<const int> order) {
    std::cout << "cost " << cost << '\n';
    std::cout << "order " << order.size();
    for (int v : order) {
        std::cout << ' ' << v;
    }
    std::cout << '\n';
}

int main() {
    int n;
    if (!(std::cin >> n)) {
        return 0;
    }

    std::vector<std::vector<long long>> distance(n, std::vector<long long>(n));
    for (auto& row : distance) {
        for (auto& value : row) {
            std::cin >> value;
        }
    }

    // 全訪問IDを重複なく1回ずつ用意する。
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    // 元の頂点IDに対応する静的な費用。
    const auto dist = [&](int from, int to) {
        return distance[from][to];
    };

    // 採点したい順序をそのまま入力する。
    for (int& v : order) {
        std::cin >> v;
    }

    // 0なら開路、1なら戻り辺を含む閉路。
    bool cycle;
    std::cin >> cycle;

    // const spanを使い、順序を変えずに合計する。
    const auto cost = tsp::total_distance(std::span<const int>(order), dist, cycle);

    // 費用は戻り値から受け取り、入力と同じorderを出力する。
    print_tour(cost, order);
}
```

**入力例**

```text
4
0 2 3 6
2 0 1 4
3 1 0 3
6 4 3 0
0 3 1 2
0
```

**出力例**

```text
cost 11
order 4 0 3 1 2
```

標準出力の `cost` は総移動費用、`order 件数 頂点列` は訪問順です。閉路でも先頭頂点は末尾へ重複して出力しません。閉路を実際に移動する際は、最後から先頭へ戻ります。この例のAPIは統計を受け取らず、標準エラーへの統計出力はありません。
