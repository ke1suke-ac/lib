# 複数エージェント経路計画・搬送ライブラリ サンプルコード集

対象ヘッダ：`multi_agent_path_finding_v06.hpp`。C++20で使用する。説明はこのヘッダの公開APIと動作に対応する。

1. [何を解くライブラリか？](#1-何を解くライブラリか)
2. [APIリファレンス](#2-apiリファレンス)
3. [ユースケース](#3-ユースケース)

## 1. 何を解くライブラリか？

### 1.1 共通の移動ルール

複数の台車、ロボット、盤面の駒などが同時に移動するとき、互いにぶつからない経路を作るライブラリである。以下では1台・1体をagent（エージェント）と呼ぶ。移動できる場所を「頂点」、直接行き来できる場所の組を「辺」と呼ぶ。頂点番号は0から始める。

移動地図は無向・重みなしのグラフである。つまり、登録した辺は両方向に使え、どの辺の移動も1ターンかかる。各agentは毎ターン、隣接頂点へ1回移動するか、その場で待機する。全員の移動は同時に起こる。agentは1頂点を占有し、次の2種類の衝突を禁止する。

- 同じ時刻に、2体が同じ頂点を占有する。
- 同じターンに、2体が同じ辺を逆方向へ進み、位置を交換する。

先行するagentが出た頂点へ次のagentが同じターンに入ることや、3体以上が輪を描いて同じ方向に動くことは許される。物理的な車幅、曲がるための追加時間、方向、速度の違いはモデルに含まれない。始点同士は相異なる必要がある。

この共通ルールの上で、次の二つの問題を扱う。実行可能な計画を探すためのライブラリであり、常に最適解を得ることや、解が存在すれば必ず発見できることは保証しない。

### 1.2 MAPF：行き先と担当が決まっている移動

「agent0は棚Aへ、agent1は棚Bへ」のように、各agentの目的地が決まっている場合である。全員を目的地まで移動させ、移動と待機にかかる時間を小さくする。始点同士、終点同士はそれぞれ重複不可だが、自分の終点が別のagentの始点と同じでもよい。

| 記号 | 意味 |
| --- | --- |
| $K$ | agent数。1以上 |
| $a$ | agent番号。$0\leq a<K$ |
| $s_ {a}$ | agent $a$ の始点 |
| $g_ {a}$ | agent $a$ の指定終点 |
| $t$ | 時刻。0以上の整数。1ターンで1増える |
| $x_ {a}(t)$ | 時刻 $t$ にagent $a$ がいる頂点 |
| $T_ {a}$ | 返却経路の最後の時刻。`paths[a].size()-1` |
| $S$ | 全agentの経路時間の合計 |
| $L$ | 全agentのうち最長の経路時間 |

各経路は $x_ {a}(0)=s_ {a}$、$x_ {a}(T_ {a})=g_ {a}$ を満たす。記録された経路の後も、そのagentは終点で待機し続ける。終点へ一度立ち寄ってから離れる場合、最初の訪問時刻を終了時刻とは数えない。

$$S=\sum_ {a=0}^{K-1}T_ {a},\qquad L=\max_ {0\leq a<K}T_ {a}$$

目的関数は、選択した設定に応じて次のいずれかとなる。

$$\operatorname{lexmin}(S,L)\qquad\text{または}\qquad\operatorname{lexmin}(L,S)$$

`lexmin` は「左を小さくし、左が同じなら右を小さくする」という優先順位付きの比較を意味する。

- `sum_of_costs`：$S$ を最優先で小さくする。同じなら $L$ が小さい方を選ぶ。全体の拘束時間を減らしたい場合に使う。
- `makespan`：$L$ を最優先で小さくする。同じなら $S$ が小さい方を選ぶ。全員が終わるまで次の工程を始められない場合に使う。

例えば、二つの計画の $(S,L)$ が $(10,7)$ と $(12,6)$ なら、前者は合計時間に優れ、後者は全員完了時刻に優れる。移動も経路内の待機も1ターンとして数える。返却経路の末尾の後に補われる待機は加算しない。自分で経路末尾に待機を記録した場合、その記録も時間として数える。

MAPFでは担当の組合せは入力側で決める。候補となる担当を複数試す使い方は第3章の例06で示す。既存計画の一部のagentだけ、または一部の時間区間だけを作り直すこともできる。

### 1.3 MAPD：集荷・配達を、担当の選択も含めて計画する

「この地点で荷物を受け取り、別の地点へ運ぶ」というタスクを複数与える。担当するagent、着手する順番、衝突しない移動をまとめて計画する。1体が同時に担当できるタスクは1件である。解禁時刻より前には割当も集荷もできない。集荷地点と配達地点が同じタスク、異なるタスクが同じ地点を使う場合も扱える。

| 記号 | 意味 |
| --- | --- |
| $J$ | 入力タスク数。0件でもよい |
| $j$ | 入力順のタスク番号。$0\leq j<J$ |
| $p_ {j}$、$q_ {j}$ | タスク $j$ の集荷頂点、配達頂点 |
| $r_ {j}$ | タスク $j$ の解禁時刻。0以上 |
| $H$ | 最終時刻。時刻0〜$H$ の経路を作る |
| $c_ {j}$ | タスク $j$ の配達完了時刻。未完了なら定義しない |
| $\mathcal C$ | 時刻 $H$ までに完了したタスク番号の集合 |
| $C=\lvert\mathcal C\rvert$ | 完了したタスク数 |
| $F$ | 完了タスクについて、解禁から完了までの時間を合計した値 |
| $Q$ | 全agentの実移動回数。待機は含めない |
| $K$、$a$、$t$、$x_ {a}(t)$ | agent数、agent番号、時刻、位置。MAPFと同じ意味 |
| $\mathbf{1}[P]$ | 条件 $P$ が成り立つと1、成り立たないと0になる値 |

$$F=\sum_ {j\in\mathcal C}(c_ {j}-r_ {j}),\qquad Q=\sum_ {a=0}^{K-1}\sum_ {t=0}^{H-1}\mathbf{1}[x_ {a}(t+1)\ne x_ {a}(t)]$$

$$\operatorname{lexmin}(-C,F,Q)$$

つまり、まず完了数 $C$ を多くする。完了数が同じなら解禁から配達までの合計時間 $F$ を小さくし、それも同じなら実移動数 $Q$ を小さくする。$-C$ は「小さいほど完了数が多い」を表すための符号である。目的の順番を変更するオプションはない。

未完了タスクは $F$ に含めない。そのため、完了数が異なる二つの結果を $F$ だけで比べてはいけない。$Q$ には、未完了タスクへ向かう移動や、他のagentを通すための移動も含まれる。

`valid=true` は「有効な計画」であって、「全タスクを完了した」という意味ではない。全件完了が必要なら `completed_tasks == tasks.size()` 相当も確認する。タスクの担当固定、タスク間の先行関係、配達締切、荷物ごとの容量は直接指定できない。

## 2. APIリファレンス

### 2.1 読み方・準備・入力の共通条件

公開型・公開データメンバ・明示的な公開操作を以下に列挙する。実装のprivate定義と、ヘッダ単独実行時だけ有効になる自己テストは利用APIに含めない。表の「既定値なし」は、その引数の指定が必須という意味である。

全ての型・関数はグローバル名前空間にある。solverオブジェクトの `.solve()` ではなく、`solve_mapf(...)` などの自由関数を呼ぶ。C++20、`bits/stdc++.h` を扱えるGCC環境、AtCoder Libraryの `atcoder/mincostflow` と依存ヘッダが必要である。MAPFだけを使う場合もACLが必要。

同梱ZIPのルートで例01を実行する場合：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined -I. -Idependencies examples/sample01.cpp -o sample01
./sample01 < inputs/sample01.in
```

独自に配置する場合は、`multi_agent_path_finding_v06.hpp` の場所と、`atcoder` ディレクトリの親を `-I` で指定する。ヘッダは通常includeして使う。ヘッダ自体を翻訳単位としてコンパイルすると、埋込み自己テストの `main` が有効になる。

| 入力の種類 | 条件・寿命 |
| --- | --- |
| 頂点番号 | `0 <= v < graph.size()`。グラフは正しく構築済みで、solverへ渡すときは1頂点以上 |
| MAPFの`starts`・`goals` | `span<const int>`。同じ長さ、1体以上。各配列内で重複なし |
| MAPDのagent | 1体以上。始点が範囲内で相異なる |
| `span` | 配列の内容を所有しない参照。`vector`・`array`をそのまま渡せる。呼出し中に元配列を破棄・再確保・変更しない |
| deadline・外部予約のポインタ | `nullptr`は不使用。指すオブジェクトを呼出し終了まで生存させる |
| 計算時間の値 | ミリ秒。有限で時計の表現範囲内の値を使用する。NaN・無限大は使用しない |
| 計算の期限 | ソフト制限。入力検査・前計算・大きな処理の途中で即座に中断する保証はない。外側の締切より余裕を取る |
| サイズ | 頂点数・経路長・タスク数などが内部の`int`の範囲に収まり、必要なメモリを確保できること |
| 並列利用 | 単一スレッドを前提とする。同じworkspaceや予約を同時に変更・使用しない |

### 2.2 `mapf_graph`：地図

| API・メンバ | 型／引数の既定値 | 意味・条件・戻り値 |
| --- | --- | --- |
| `mapf_graph()` | 引数なし | 空。`vertex_count=0`、配列は空。代入先の準備などに使う |
| `mapf_graph(int n, const vector<pair<int,int>>& undirected_edges)` | 両引数とも既定値なし | `n>=0`。辺の両端は範囲内。各辺は1回渡せばよい。自己ループを無視し、重複を除いて両方向を登録する |
| `int size() const` | 引数なし | 頂点数 |
| `span<const int> neighbors(int v) const` | `v`必須 | 昇順の隣接頂点列。`v`は範囲内。返却spanはgraphの変更・破棄後に使わない |
| `int degree(int v) const` | `v`必須 | 頂点`v`の隣接頂点数。`v`は範囲内 |
| `vertex_count` | `int`、既定値`0` | 頂点数。通常は直接変更しない |
| `offsets` | `vector<int>`、既定値は空 | 各頂点の隣接列が`edges`内のどこにあるかを表す境界 |
| `edges` | `vector<int>`、既定値は空 | 隣接頂点列を頂点順につなげた配列 |

`offsets[v]`〜`offsets[v+1]-1` が頂点`v`の隣接列である。直接組み立てる場合は、`offsets.size()==n+1`、先頭0、非減少、末尾が`edges.size()`、頂点番号の範囲、隣接列の昇順・重複なし、無向辺の対称性を利用側で保証する。通常はコンストラクタを使う。コンストラクタと頂点照会は不正番号をassertで検査するが、壊れた内部配列をsolverが安全に診断する設計ではない。

### 2.3 列挙型：全ての値

#### `mapf_strategy`

| 値 | 意味・設定時の注意 |
| --- | --- |
| `prioritized` | 順番に経路を作る方式で構築し、設定が有効ならLNS改善も行う。`prioritized_restarts`は構築の試行上限で最低1回 |
| `pibt` | PIBT方式だけで構築する。`prioritized_restarts`を1〜8に丸めた候補数を試す。`lns_iterations`・`lns_size`・`congestion_weight`は使わない |
| `automatic` | 短いPIBTと優先度付き構築の候補を比較し、得た解を改善する標準設定。通常の優先度付き構築は2回、そこで失敗した場合に追加構築を試す |

LNSは、一部のagentを選び、その経路を繰り返し解き直す改善処理である。PIBTは各ターンの全員の次の位置を決めながら経路を組み立てる方式である。

`strategy`を読むのは `solve_mapf`。`improve_mapf`・`repair_mapf`ではこの指定を切り替えても方式は変わらない。どの方式にも、一般の入力について成功・最適性の保証はない。

#### `mapf_objective`

| 値 | 優先順位 |
| --- | --- |
| `sum_of_costs` | `sum_of_costs`を小さくし、同点なら`makespan`を小さくする |
| `makespan` | `makespan`を小さくし、同点なら`sum_of_costs`を小さくする |

#### `mapf_status`

| 値 | 意味・利用側の判断 |
| --- | --- |
| `success` | 有効な解がある。最適性の証明や、全反復の完走を意味しない |
| `invalid_input` | 空のagent、配列長・番号・重複、基準経路や修復範囲などの条件が不正。入力を直す |
| `unreachable` | 検査対象の始点と終点が静的グラフ上で連結していない。seed変更で解決しない |
| `horizon_too_short` | 検査対象の距離下界や基準経路長に対して移動ターン上限が小さい。条件に応じて上限を見直す |
| `timeout` | 有効解を返せず、所定の時間検査で期限切れと判断した。時間切れでも有効解を保持できた呼出しは`success`になることがある |
| `search_failed` | 試した探索・固定条件では有効解を返せなかった。解が存在しないことの証明ではない |
| `memory_limit` | 主に時空間の状態数などが実装の事前検査を超える。一般のメモリ確保失敗を全て捕捉する値ではない |

優先してどの検査を通るかで、同じような失敗でもstatusは異なり得る。`improve_mapf`は有効な基準解を維持して成功を返すことがあり、repairでも基準解が新条件に有効なら採用されることがある。statusだけから「予算を使い切ったか」は分からない。

#### `mapd_assignment_strategy`

| 値 | 意味・設定時の注意 |
| --- | --- |
| `automatic` | 複数方式・seedの独立した計画を比較し、完了数、フロー時間、実移動数の順で選ぶ。候補数は`portfolio_runs` |
| `greedy` | その時点の空きagentと着手可能なタスクから、近さを基準に軽量に割り当てる |
| `greedy_unique_endpoints` | すでに使う集荷・配達地点と重ならない割当を先に探し、残りは重複も許して割り当てる。端点の排他制約を厳密に守る意味ではない |
| `min_cost_flow` | その時点で割当可能な組について、割当数を確保し、移動距離を基にした費用を小さくする。全搬送計画や最終目的関数の厳密解を意味しない |

同じ方式でも道路の混雑、タスクの解禁順、seedで結果が変わり得る。`automatic`の候補順はgreedy、端点分散、min_cost_flow、greedy、端点分散、greedy、min_cost_flow、greedyで、各候補のseedも変える。候補ごとに満額の時間制限を追加するのではなく、残りの共通予算を使う。

### 2.4 `mapf_options`：全10項目

| メンバ | 型 | デフォルト | 値の範囲・意味 |
| --- | --- | --- | --- |
| `strategy` | `mapf_strategy` | `automatic` | 上記3値のいずれか |
| `objective` | `mapf_objective` | `sum_of_costs` | 上記2値のいずれか |
| `time_limit_ms` | `double` | `1000.0` | 有限の負値なら相対時間制限なし。0は即時の期限。非負値はその呼出しのソフト計算時間制限 |
| `seed` | `uint64_t` | `1` | 64bit符号なし整数全域。再現比較では固定する |
| `max_time` | `int` | `-1` | 非負なら移動・待機のターン上限、負値なら自動設定。時間窓repairでは不使用 |
| `prioritized_restarts` | `int` | `8` | 0以下は1として扱う。指定方式によって最低1、1〜8、または異なる順序の数に制限される。時間切れなどで全回数を試さないこともある |
| `lns_iterations` | `int` | `64` | 改善反復の上限。0以下ならLNS無効。大きくすると改善を試す回数が増える |
| `lns_size` | `int` | `8` | 1回の改善で選ぶagent数。0以下ならLNS無効。正値はagent数までに制限される |
| `congestion_weight` | `int` | `0` | 同じ長さの経路候補を比較する混雑重み。通常は非負で、内部の整数演算があふれない範囲を使う。負値は拒否されないが、混雑を好む方向の評価になる。まず0、混雑時に1程度を試す |
| `deadline` | `const mapf_deadline*` | `nullptr` | 共通の絶対期限。不使用ならnull。相対制限も有効なら早い方で停止する |

最初は既定設定を基準とし、固定反復で比較したいときだけ `time_limit_ms=-1` と `deadline=nullptr` にする。`max_time`を小さくしすぎると待機・迂回が入らず、大きくしすぎると時空間領域のメモリが増える。LNSはまず既定の64回・8体程度から、実際の改善量と計算時間を見て調整する。大きい値ほど良いとは限らない。

| 呼出し | 使用しない／内部で置き換える項目 |
| --- | --- |
| `solve_mapf`で`pibt` | LNSの2項目と`congestion_weight`は不使用 |
| `solve_mapf`で`automatic` | `congestion_weight`は通常構築・改善で0、追加構築で1へ置換。`prioritized_restarts`は主に通常構築失敗後の追加構築に使う |
| `improve_mapf` | `strategy`・`prioritized_restarts`は不使用 |
| 全経路`repair_mapf` | `strategy`・LNSの2項目は不使用。`prioritized_restarts`は選択agentの異なる順序数以下に制限される |
| 時間窓`repair_mapf` | 上に加えて`max_time`も不使用。窓の長さを使う |

自動上限を正確に把握したい場合、$D$を各対象agentの独立な最短距離の最大、$K$を全agent数、$L_ {0}$を基準経路の最大時間とすると、基本値は次である。

$$H_ {\mathrm{auto}}=D+\max(16,D,K)$$

`solve_mapf`はこの値、`improve_mapf`と全経路repairはさらに $L_ {0}$ 以上にする。全経路repairの $D$ は選択agentだけから求める。repairは外部予約のhorizon以上にも広げる。明示的な上限が基準経路より小さいと、improveは基準解を返す一方、全経路repairは`horizon_too_short`になり得る。時間窓は `end_time-begin_time` を使う。空の修復集合はこれらの上限設定より前に基準解の検査だけで返る。

### 2.5 `mapf_result`と出力演算子

| メンバ | 型 | デフォルト | 意味・利用条件 |
| --- | --- | --- | --- |
| `success` | `bool` | `false` | 成功したか。falseなら経路を実行しない |
| `status` | `mapf_status` | `invalid_input` | 上記の状態分類 |
| `paths` | `vector<vector<int>>` | 空 | `paths[a][t]`がagent aの時刻tの頂点。経路末尾の後は同じ頂点で待機 |
| `sum_of_costs` | `long long` | `0` | 各経路の`size()-1`の総和。失敗結果の0を良い解と扱わない |
| `makespan` | `int` | `0` | 各経路の`size()-1`の最大値 |
| `expanded_states` | `long long` | `0` | A*探索が調べて展開した「時刻と頂点の組」の数。PIBTだけなら0。全CPU仕事量を表す値ではない |
| `attempts` | `int` | `0` | 構築や修復で試した候補数。LNS反復数とは異なる |
| `lns_improvements` | `int` | `0` | LNSで改善を採用した回数 |
| `strategy_used` | `mapf_strategy` | `prioritized` | 得られた初期解の構築方式などを示す。入力でautomaticを指定したことをそのまま返す値ではない |

solverが成功して返した経路は、外側がagent数、内側が各1頂点以上で、頂点番号はgraphの範囲内になる。時間・件数・探索統計は非負。始点と終点が同じで移動不要なら、長さ1の経路、経路時間0も有効である。

`improve_mapf`は基準解の試行数・構築方式を維持し、展開数・LNS改善数を加算する。repairは通常、その呼出しの探索統計を返し、構築方式はprioritized、LNS改善数は0。選択集合が空で基準解を返す場合は展開数・試行数を0にするが、基準の構築方式とLNS改善数は継承する。

`std::ostream& operator<<(std::ostream& os, const mapf_result& result)` は全スカラー値と配列の先頭を表示し、同じ `os` を返す。両引数は必須。`cout << result`、`cerr << result`、`ostringstream << result`の形で使える。末尾改行・flush・検証・集計の再計算は行わない。書式設定を変更せず、boolはtrue/false、列挙値は名前で表示する。

経路は20本まで、各経路の頂点列は20要素まで。省略箇所は残り件数を表示する。`size`は元の全要素数であり、配列そのものは縮めない。例20で実行できる。

### 2.6 `mapf_deadline`：共通の絶対期限

| API・メンバ | 引数・型・既定値 | 意味・戻り値 |
| --- | --- | --- |
| `clock` | `using clock = std::chrono::steady_clock` | 日付時計ではない単調時計 |
| `end` | `clock::time_point`、既定値は`clock::time_point::max()` | 公開の絶対終了時刻 |
| `mapf_deadline()` | 引数なし | 実質的に期限なし |
| `explicit mapf_deadline(clock::time_point end_time)` | 引数必須 | 指定した時点を終了時刻にする |
| `static mapf_deadline after_ms(double milliseconds)` | 引数必須 | 今から指定ms後。有限の負値なら期限なし |
| `bool expired() const` | 引数なし | 現在時刻がend以降ならtrue |

`after_ms`は呼んだ時刻を基準とする。繰り返し作り直せば期限も延長されるので、合計予算を共有するときは外側で一度だけ作る。過去の`end_time`や0msはすでに期限切れになり得る。待機して期限まで使い切る機能ではない。

### 2.7 `mapf_workspace`：前計算と作業領域

| API | 引数・戻り値 | 意味 |
| --- | --- | --- |
| `mapf_workspace()` | 引数なし | 空の作業領域を構築する |
| `mapf_workspace(const mapf_workspace&)` | deleted | コピー構築は禁止 |
| `mapf_workspace& operator=(const mapf_workspace&)` | deleted | コピー代入は禁止 |
| `mapf_workspace(mapf_workspace&& other) noexcept` | other必須 | 作業領域を移す。移動元は空になり、再利用できる |
| `mapf_workspace& operator=(mapf_workspace&& other) noexcept` | other必須 | 作業領域を移し、自身を返す。自己move代入は何もしない |
| `void clear()` | 引数なし | 距離キャッシュと主要探索領域を破棄する。小さな補助vectorのcapacityまで全て0にする保証はない |
| `bool prepare(const mapf_graph& graph, span<const int> targets)` | 両引数必須 | 正の頂点数と範囲内のtargetsが必要。不正ならfalse。未登録の終点への距離を追加する。空targets・重複targetsは許される |
| `int cached_target_count() const` | 引数なし | 計算済みの異なる終点の数 |

solverが必要に応じて`prepare`を呼ぶので、事前呼出しは任意。prepareは到達可能性を返す関数ではなく、離れた連結成分の終点でも番号が正しければtrueになり得る。失敗してもworkspaceが完全に元の状態のままとは限らない。

同じ地図で始点・終点を変える場合は再利用できる。新しい終点を追加するほど距離表は増える。地図は別の所有物であり、workspaceはgraphを所有しない。辺・頂点番号・配列を書き換える前や、graphを破棄して同じ領域へ再作成する前は `clear()` する。同じアドレス・頂点数のまま辺だけ変えたことは自動検出されない。

### 2.8 `mapf_repair_request`：変更可能な範囲

| メンバ | 型 | デフォルト | 条件・意味 |
| --- | --- | --- | --- |
| `agents` | `span<const int>` | 空 | 変更可能なagent番号。0〜K−1、重複なし。空は全員指定ではない |
| `begin_time` | `int` | `0` | 全経路修復では0必須。時間窓では0以上 |
| `end_time` | `int` | `-1` | 負値なら全経路修復。非負なら時間窓で、`begin_time < end_time <= 基準makespan` |
| `external_reservations` | `const mapf_reservations*` | `nullptr` | 外部予約。時刻原点は基準経路と同じ。非nullならgraphと同じ頂点数でvalidな表 |

全経路修復では、選択したagentだけ新しいgoalへ変更してよい。非選択agentのgoalは基準経路の末尾と一致させる。時間窓では全員の最終goalが基準と同じでなければならず、窓の開始・終了位置も基準経路と同じに固定される。

非選択経路は固定する。外部予約は選択agentだけでなく全体の検証に作用するので、窓外・固定経路に違反が残れば修復に失敗する。時間窓修復では選択経路の末尾の重複待機を削る場合がある。窓外の保持は「暗黙の末尾待機を補った位置」で比較する。

### 2.9 `mapf_reservations`：外部の占有と移動禁止

| API・メンバ | 引数・既定値 | 意味 |
| --- | --- | --- |
| `mapf_reservations()` | 引数なし | 未設定。`vertex_count=0`、`horizon=-1`、ownerは空 |
| `mapf_reservations(int n, int max_time)` | 両引数必須、いずれも非負 | 頂点数n、時刻0〜max_timeの表を全て空き状態で構築する |
| `vertex_count` | `int`、既定値`0` | 予約表の頂点数 |
| `horizon` | `int`、既定値`-1` | 予約表の最後の時刻。horizonも含む |
| `owner` | `vector<int>`、既定値は空 | `owner[time*vertex_count+vertex]`が占有値。通常は下記メソッドで操作する |

| メソッド | 引数・デフォルト | 戻り値・条件 |
| --- | --- | --- |
| `bool valid() const` | なし | 寸法とownerの長さが整合すればtrue。経路の合法性までは検査しない |
| `bool valid_time_vertex(int time, int vertex) const` | 両方必須 | `0<=time<=horizon`かつ頂点範囲内ならtrue |
| `int at(int time, int vertex) const` | 両方必須 | 占有値。時刻・頂点は範囲内必須。assertあり |
| `bool block_vertex(int time, int vertex)` | 両方必須 | その時刻の頂点を閉鎖する。範囲外や外部経路の占有と重なるとfalse。同じ閉鎖の再登録はtrue |
| `bool block_move(int time, int from, int to)` | 全て必須 | time→time+1の指定方向を禁止する。`0<=time<horizon`と範囲内頂点が必要。範囲外はfalse |
| `bool block_edge(int time, int u, int v)` | 全て必須 | 同じターンのu→vとv→uを両方禁止する。条件はblock_moveと同じ |
| `bool reserve_path(int start_time, span<const int> path, int hold_until=-1)` | 最後だけ既定値`-1` | 外部経路を登録する。`path[0]`がstart_timeの位置。hold_untilが負なら経路末尾まで、非負ならその時刻まで末尾待機も登録する |
| `bool move_is_blocked(int time, int from, int to) const` | 全て必須 | 明示登録された移動禁止ならtrue。範囲外はfalse。占有衝突・外部経路との正面衝突の総合判定ではない |

これらの操作には、寸法・配列が整合した予約表を使う。`owner`は-1が空き、-2が頂点閉鎖、-3以下が登録された外部経路の識別値。外部経路を単なる頂点閉鎖の列で置き換えると、その経路との正面交換を識別できない。

`reserve_path`は、空経路、頂点・時刻の範囲外、終端より前のhold_until、既存の占有・明示禁止移動・別の外部経路との正面衝突などでfalseを返す。falseならその経路の占有は追加されない。ただしgraphを引数に取らないので、隣接していない頂点への飛び移りは検出しない。利用側で移動の合法性を確認する。

`block_move`も実在する辺かを検査しない。`from==to`はその時刻の待機禁止を表す。`block_edge`は頂点の閉鎖ではない。予約削除用の公開メソッドはないので、登録内容を取り消したい場合は必要な内容から表を作り直す。

MAPF結果の末尾待機は暗黙に続く一方、外部経路の占有は登録した最終時刻で終わる。`hold_until`省略が「ずっと留まる」という意味にならない点に注意する。

### 2.10 MAPFの構築・改善・修復：全overload

以下は署名を確認するための宣言例である。ヘッダと一緒に再宣言する必要はない。`options={}`以外の引数は全て必須。

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
```

| 引数 | 内容 |
| --- | --- |
| `graph` | 現在の移動規則。破壊されたCSRを渡さない |
| `starts` | 全agentの時刻0の位置 |
| `goals` | 全agentの最終目的地。部分修復でも選択agent分だけにしない |
| `initial_solution` | improveの有効な基準解。success=true、始終点一致、合法移動、衝突なしが必要 |
| `base` | repairの基準解。success=true、全経路の始点一致・合法移動・衝突なしが必要。新しい外部予約への適合は必須ではない |
| `request` | 修復できるagent・時間範囲・外部予約 |
| `workspace` | 再利用する作業領域。省略overloadは呼出しごとに内部で一時workspaceを用意する |
| `options` | 既定値は`mapf_options{}`。採点と予算などを指定する |

| 関数 | 戻り値の使い方 |
| --- | --- |
| `solve_mapf` | 新規計画。successを確認してからpathsを利用する。外部予約を受け取るoverloadはない |
| `improve_mapf` | 有効な基準解を再採点して改善する。予算・上限が不足すれば基準解を返すことがある。衝突した初期解を直す入口ではない |
| `repair_mapf` | 選択した範囲を作り直す。基準解が新条件にも有効なら、より悪い候補を採用しない。入力・上限の前提違反までfallbackする保証はない |

全経路repairは、選択agentの基準末尾と新goalsが違っていてよい。これを使い、各agentの始点だけの衝突しない基準を作って全員を選べば、外部予約付きで新規計画できる。空の `request.agents` は基準解が新条件を満たすかを調べるだけで、新たな探索をしない。

### 2.11 MAPDの入力型とオプション

| 型・メンバ | 型 | デフォルト | 条件・意味 |
| --- | --- | --- | --- |
| `mapd_agent::start` | `int` | `-1` | 初期位置。範囲内でagent間重複なし。既定のままでは不正 |
| `mapd_task::pickup` | `int` | `-1` | 集荷頂点。範囲内必須 |
| `mapd_task::delivery` | `int` | `-1` | 配達頂点。範囲内必須。pickupと同じでもよい |
| `mapd_task::release_time` | `int` | `0` | 解禁時刻。0以上。最終時刻より先のタスクも登録できるが、その計画内では未着手 |

これらは通常の集成体で、`mapd_agent{0}`、`mapd_task{1,2,0}`のように初期化できる。全タスクは呼出し時に渡す。計算中に外からタスクを追加するAPIはない。番号が正しくても、集荷・配達地点に到達できない仕事や、残りターンでは配達できない仕事は未割当・未完了のままになることがある。

| `mapd_options`メンバ | 型 | デフォルト | 条件・意味 |
| --- | --- | --- | --- |
| `max_time` | `int` | `1000` | 0以上の最終時刻。各返却経路はmax_time+1頂点。負値は不正で、自動設定ではない |
| `time_limit_ms` | `double` | `1000.0` | 有限の負値なら相対時間制限なし。非負ならソフト計算時間制限 |
| `seed` | `uint64_t` | `1` | 乱数seed。再現比較では固定 |
| `assignment` | `mapd_assignment_strategy` | `automatic` | 上記4値のいずれか |
| `portfolio_runs` | `int` | `8` | automaticで比較する候補数。1〜8に丸める。他方式では不使用 |
| `deadline` | `const mapf_deadline*` | `nullptr` | 共通の絶対期限。相対制限もある場合は早い方 |

### 2.12 `mapd_result`・`solve_mapd`・出力

```cpp
mapd_result solve_mapd(const mapf_graph& graph,
    std::span<const mapd_agent> agents, std::span<const mapd_task> tasks,
    const mapd_options& options = {});
```

graph、agents、tasksは必須。空tasksは許されるが、空agentsは不可。optionsを省略すると上の既定値になる。MAPDにはworkspace・既存結果・搬送中の状態・外部予約を受け取るoverloadはない。再呼出しは新規計画であり、前の担当・運搬中の荷物・待ち時間を自動では引き継がない。

| メンバ | 型 | デフォルト | 意味 |
| --- | --- | --- | --- |
| `valid` | `bool` | `false` | 経路と報告された完了集計の整合が取れた計画。全件完了フラグではない |
| `completed_tasks` | `int` | `0` | 完了タスク数 |
| `sum_flow_time` | `long long` | `0` | 完了タスクの「完了時刻−解禁時刻」の総和 |
| `total_moves` | `long long` | `0` | 待機を除いた全agentの移動回数 |
| `paths` | `vector<vector<int>>` | 空 | agent番号順の位置列。validなsolver結果では各配列がmax_time+1頂点 |
| `task_agent` | `vector<int>` | 空 | 入力タスク番号順の担当agent。0〜agent数−1、未割当は-1。割当済みでも未集荷の場合がある |
| `task_pickup_time` | `vector<int>` | 空 | 集荷時刻。0〜max_time、未集荷は-1 |
| `task_completion_time` | `vector<int>` | 空 | 配達完了時刻。0〜max_time、未完了は-1 |

validなsolver結果では、pathsの外側の長さはagent数、3つのタスク配列の長さはそれぞれ入力タスク数である。completed_tasksは0〜タスク数、sum_flow_timeとtotal_movesは非負。valid=falseのときは、これらの長さや内容を成功時と同じ前提で利用しない。

計算途中で時間切れになった単独方式は、残りを現在位置での待機として埋め、有効な未完了計画を返せる。automaticが候補を1回も実行できなければvalid=falseになり得る。MAPFのような詳細statusはない。

新規割当を行う時刻は0〜H−1。Hでは、すでに割当済みの搬送の到着だけを処理する。そのためH=0では新規タスクを割り当てず、release_time=Hのタスクも新たに着手しない。集荷と配達が同じ場所でも、この割当処理は必要である。

`std::ostream& operator<<(std::ostream& os, const mapd_result& result)` は同じosを返す。両引数必須。MAPFと同じ表示上限で経路を出し、3つのタスク配列はそれぞれ先頭20要素までを別々に表示する。末尾改行・flush・検証はしない。配列長が不揃いの手作り結果や失敗結果も表示できるが、有効性を保証するものではない。

### 2.13 検証関数：全overload

以下の引数は全て必須。どれもboolを返し、結果の配列や集計値を書き換えない。

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

| 検証 | trueの意味・検査しない内容 |
| --- | --- |
| MAPF・外部予約なし | success=true、1体以上、始点・終点一致、経路の合法性、頂点衝突・正面交換なしを確認。status・sum_of_costs・makespan・探索統計の正しさは検査しない |
| MAPF・外部予約あり | 上に加え予約への違反を検査。経路が先に終わっても予約のhorizonまで終点待機を補って調べる |
| MAPD | 経路長の一致、移動、衝突、完了と報告したタスクの担当・集荷・配達時刻、同一agentの完了済み搬送区間の重なり、集計値を検査。result.valid自体を要求する検査ではない |

MAPD validatorは未完了タスクの全てのメタデータを完全に認証するものではなく、呼出し時のoptions.max_timeと一致するかも知らない。正しい入力型・番号・解禁時刻・整合したデータを前提として使う。任意の壊れた外部データを安全に受け入れる境界にはしない。

### 2.14 通常の値操作と使い分け

workspace以外の入力型・設定型・結果型は、通常のコピー・move・代入で扱える。`span`やポインタを含む設定のコピーは参照先のデータを複製しない。`mapf_result`・`mapd_result`をコピーすると経路などのvectorもコピーするので、大きい結果の複製コストには注意する。

MAPFとMAPDのスコアは別の意味を持つ。`time_limit_ms`と`max_time`も、それぞれ計算機の時間と、問題内の移動ターン数である。結果表示の上限20はデバッグ表示だけの上限で、解けるagent数・タスク数の上限ではない。

## 3. ユースケース

全24例は独立したプログラムである。必要な例を一つだけコピーしてコンパイルする。全例に標準入力と標準出力があり、ライブラリを使う行には日本語コメントを付けている。入力は記載した型・範囲・形式を満たすことを前提にする。

出力例は同梱ヘッダで実行して取得したもの。経路が複数存在する問題では、別の有効経路が返ることもある。時間制限付きの例16は計算機の負荷にも依存する。`NO_PLAN`は今回の条件で発見できなかったという意味で、問題自体が解けないことの証明ではない。

多くの例で `time_limit_ms=-1` とseed固定を使うのは、入力例と結果を照合しやすくするためである。本番では外側の時間制限に合わせて計算予算を設定する。サンプルの固定地図・小さい上限値は、その例で解く問題の条件であり、未知の問題への推奨値ではない。

| 例 | ユースケース | 主に使うAPI・機能 |
| --- | --- | --- |
| 01 | 一般のグラフで、担当が決まった目的地へ移動する | solve_mapf・validate_mapf_result |
| 02 | 壁のある盤面を、座標のまま扱う | mapf_graph・座標変換 |
| 03 | 全員の完了時刻を重視する | mapf_objective |
| 04 | 環状通路で、まず短い初期計画を作る | mapf_strategy::pibt |
| 05 | 同じ地図で、多数の問い合わせを繰り返す | workspace.prepare・cached_target_count |
| 06 | 目的地の担当候補を、衝突込みで比較する | workspace付きsolve_mapf・結果比較 |
| 07 | 手元の有効な経路を、さらに短くする | improve_mapf・手作りmapf_result |
| 08 | 一部のagentだけ、新しい目的地へ変更する | repair_mapf・修復対象指定 |
| 09 | 一部の経路だけを再最適化する | workspace付きrepair_mapf・混雑重み |
| 10 | 前後を固定し、短い時間区間だけを修復する | begin_time・end_time・外部予約 |
| 11 | 一時的な通行止めを反映する | block_vertex |
| 12 | 移動方向・橋の使用・待機を時刻で制限する | block_move・block_edge |
| 13 | すでに動きが決まった別のロボットを避ける | reserve_path・neighbors |
| 14 | 1ターン実行した後、観測位置から計画し直す | repair_mapf・時刻原点の更新 |
| 15 | 道路が変わったら、地図とキャッシュを更新する | workspace.clear・size・degree |
| 16 | 複数の処理で、共通の計算期限を使う | mapf_deadline・workspace付きimprove_mapf |
| 17 | 失敗理由を見て、必要な条件だけを変更する | mapf_statusの全7値 |
| 18 | 外部予約データの登録と照会を確かめる | 予約表の構築・全照会操作 |
| 19 | 経路を検証して、各ターンの全員の操作へ変換する | validate_mapf_result・経路からの操作生成 |
| 20 | 大きな結果を、長くなりすぎないログへ出す | 結果2型のoperator<< |
| 21 | 担当未定の荷物を、複数の台車で運ぶ | mapd_agent・mapd_task・solve_mapd・validate_mapd_result |
| 22 | 将来解禁される搬送と、未完了の結果を読む | release_time・タスク状態 |
| 23 | 共通の集荷地点がある搬送で、割当方式を比較する | mapd_assignment_strategyの全4値 |
| 24 | 作業領域の所有者を移して、再利用する | workspaceのmove構築・move代入 |

### 3.1 例01：一般のグラフで、担当が決まった目的地へ移動する

倉庫の台車を、それぞれ決められた棚まで移動させる。場所のつながりを辺として入力すれば、格子状でない通路も扱える。

**解く問題**：無向グラフと各agentの始点・終点から、衝突しない経路を1組求める。

**入力**：`N M K`、M行の辺 `u v`、K個の始点、K個の終点。N・Kは1以上、Mは非負。頂点は0〜N−1で、始点同士・終点同士はそれぞれ重複なし。

**出力**：成功時は `sum_of_costs makespan`、続いて各agentの `経路の頂点数 頂点列`。失敗時は `NO_PLAN`。

**サンプルコード**（`examples/sample01.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int n, m, k;
    if (!(cin >> n >> m >> k)) {
        return 0;
    }

    // 無向辺の一覧を読み込む
    vector<pair<int, int>> edges(m);
    for (auto& [u, v] : edges) {
        cin >> u >> v;
    }

    // agent番号順に、始点と終点を読み込む
    vector<int> starts(k), goals(k);
    for (int& v : starts) {
        cin >> v;
    }
    for (int& v : goals) {
        cin >> v;
    }

    // 辺は片方向分だけ入力すれば両方向に使える
    const mapf_graph graph(n, edges);

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // workspaceは呼出しごとに内部で用意される
    const auto result = solve_mapf(graph, starts, goals, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
4 4 2
0 1
1 2
2 3
3 0
0 2
1 3
```

**出力例**：

```text
2 1
2 0 1
2 2 3
```

**結果の読み方**：2体とも1ターンで到着するので、合計2、全員完了時刻1となる。時刻0の頂点も経路に含むため、各行の経路長は2である。

### 3.2 例02：壁のある盤面を、座標のまま扱う

盤面の歩けるマスだけに頂点番号を付ける。外側のプログラムでは行・列を使い、ライブラリへ渡す部分で番号へ変換する。

**解く問題**：上下左右に1マス進むか待機できる盤面上で、複数の駒を目的のマスへ移動させる。

**入力**：`H W K`（いずれも1以上）、H行の盤面（`#`は壁、`.`は通路）、K行の `始点行 始点列 終点行 終点列`。座標は0始まりで盤面内、指定位置は通路。始点同士・終点同士はそれぞれ重複なし。

**出力**：各agentについて `経路の頂点数 行,列 行,列 ...`。失敗時は `NO_PLAN`。

**サンプルコード**（`examples/sample02.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int h, w, k;
    if (!(cin >> h >> w >> k)) {
        return 0;
    }

    // 壁と通路の盤面を読み込む
    vector<string> board(h);
    for (auto& row : board) {
        cin >> row;
    }

    // 通路マスだけに頂点番号を付ける。壁の番号は-1のままにする
    vector<vector<int>> id(h, vector<int>(w, -1));
    vector<pair<int, int>> cell;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (board[y][x] != '#') {
                id[y][x] = int(cell.size());
                cell.push_back({y, x});
            }
        }
    }

    // 上下左右で隣り合う通路マスを辺で結ぶ
    vector<pair<int, int>> edges;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (id[y][x] >= 0) {
                if (y + 1 < h && id[y + 1][x] >= 0) {
                    edges.push_back({id[y][x], id[y + 1][x]});
                }
                if (x + 1 < w && id[y][x + 1] >= 0) {
                    edges.push_back({id[y][x], id[y][x + 1]});
                }
            }
        }
    }

    // 入力された座標を、ライブラリへ渡す頂点番号へ変換する
    vector<int> starts(k), goals(k);
    for (int i = 0; i < k; ++i) {
        int sy, sx, gy, gx;
        cin >> sy >> sx >> gy >> gx;
        starts[i] = id[sy][sx];
        goals[i] = id[gy][gx];
    }

    // 壁を頂点に含めず、通路だけを登録する
    const mapf_graph graph(int(cell.size()), edges);

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 座標から変換した頂点番号を渡す
    const auto result = solve_mapf(graph, starts, goals, options);
    // 経路がなければ終了する
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 出力前に衝突を確認する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // agentごとの経路を元の座標へ戻す
    for (const auto& path : result.paths) {
        cout << path.size();
        for (int v : path) {
            cout << ' ' << cell[v].first << ',' << cell[v].second;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
2 3 2
...
.#.
0 0 1 0
0 2 1 2
```

**出力例**：

```text
2 0,0 1,0
2 0,2 1,2
```

**結果の読み方**：左端と右端の駒がそれぞれ下へ1マス進む。壁のマスに頂点番号を振る必要はなく、出力は元の行・列に戻している。

### 3.3 例03：全員の完了時刻を重視する

全員が配置につくまで次の工程を開始できない場合は、最後の1体の到着を早めたい。合計時間を重視する設定との切替を示す。

**解く問題**：6地点の環状通路で2体を移動させ、入力した評価基準を優先する。

**入力**：`mode`。0なら経路時間の合計優先、1なら全員完了時刻優先。

**出力**：合計時間・全員完了時刻と経路。両設定で同じ結果になる入力もある。

**サンプルコード**（`examples/sample03.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});
    // agentごとの始点・終点。各配列内の重複は禁止
    const vector<int> starts{0, 3}, goals{2, 5};

    int mode;
    if (!(cin >> mode)) {
        return 0;
    }

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 同点時はもう一方の指標で比較する
    options.objective = mode == 0 ? mapf_objective::sum_of_costs : mapf_objective::makespan;

    // 評価基準はoptionsで選び、入力形式は同じ
    const auto result = solve_mapf(graph, starts, goals, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
1
```

**出力例**：

```text
4 2
3 0 1 2
3 3 4 5
```

**結果の読み方**：この入力では、どちらの基準でも各2ターンの計画になる。評価基準を変えれば必ず別の経路が出るわけではない。

### 3.4 例04：環状通路で、まず短い初期計画を作る

全員が1つ隣の場所へ同時に進める配置を使い、軽い構築方式を明示する。短い初期計画を得てから別途改善したい場合にも使える。

**解く問題**：N地点の輪にN体を配置し、全員を同じ方向へ1地点ずつ移動させる。

**入力**：`N`。3以上の頂点数兼agent数。

**出力**：合計時間・完了時刻と経路。2体の正面交換は許されないので、Nは3以上にする。

**サンプルコード**（`examples/sample04.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int n;
    if (!(cin >> n)) {
        return 0;
    }
    if (n < 3) {
        return 1;
    }

    // N地点を輪につなぐ辺を用意する
    vector<pair<int, int>> edges;
    for (int v = 0; v < n; ++v) {
        edges.push_back({v, (v + 1) % n});
    }

    // 3体以上の同方向の循環移動は許される
    const mapf_graph graph(n, edges);

    // 各agentの行き先を、一つ先の頂点にする
    vector<int> starts(n), goals(n);
    for (int i = 0; i < n; ++i) {
        starts[i] = i;
        goals[i] = (i + 1) % n;
    }

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // PIBTだけで構築する。最適性・成功は一般には保証されない
    options.strategy = mapf_strategy::pibt;
    // この方式では候補実行回数。1〜8に丸められる
    options.prioritized_restarts = 1;

    // pibtではlns_iterationsを指定しても改善段階は走らない
    const auto result = solve_mapf(graph, starts, goals, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
4
```

**出力例**：

```text
4 1
2 0 1
2 1 2
2 2 3
2 3 0
```

**結果の読み方**：4体が同時に1つ先へ進む。合計4、全員完了時刻1。禁止している2体の正面交換と、3体以上の同方向の循環移動は区別する。

### 3.5 例05：同じ地図で、多数の問い合わせを繰り返す

同じ道路網で始点や目的地だけを変えて何度も計算する。距離の前計算と作業領域を外側で持ち、再利用する。

**解く問題**：6地点の輪で、入力したQ組の1体移動を順に計画する。

**入力**：`Q`、続いてQ行の `始点 終点`。Qは非負、頂点は0〜5。

**出力**：1行目は前計算した終点数、以後は各問い合わせの経路時間。

**サンプルコード**（`examples/sample05.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});

    int q;
    if (!(cin >> q)) {
        return 0;
    }

    // 問い合わせと、距離を準備する終点の一覧を読み込む
    vector<pair<int, int>> queries(q);
    vector<int> targets;
    for (auto& [s, g] : queries) {
        cin >> s >> g;
        targets.push_back(g);
    }

    // 同じgraphへの反復呼出しで使い回す。graphを変更したらclearが必要
    mapf_workspace workspace;
    // 重複を除いた終点への距離を前計算する。solverにも自動準備がある
    if (!workspace.prepare(graph, targets)) {
        return 1;
    }

    // 前計算済みの異なる終点数を確認する
    cout << workspace.cached_target_count() << '\n';

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    for (auto [s, g] : queries) {
        // 配列は呼出しが終わるまで生存させる
        const array starts{s}, goals{g};

        // 4番目の引数で既存の作業領域を渡す
        const auto result = solve_mapf(graph, starts, goals, workspace, options);
        // 問い合わせごとに成否を確認する
        if (!result.success) {
            cout << "NO_PLAN\n";
            continue;
        }

        // 返却経路を検証する
        if (!validate_mapf_result(graph, starts, goals, result)) {
            return 2;
        }

        // この例では1体なので最終到着までの時間になる
        cout << result.sum_of_costs << '\n';
    }

    return 0;
}
```

**入力例**：

```text
3
0 2
1 2
0 4
```

**出力例**：

```text
2
2
1
2
```

**結果の読み方**：目的地は2・2・4なので、前計算する異なる目的地は2個。後続3行の2・1・2は、それぞれの問い合わせに必要なターン数である。

### 3.6 例06：目的地の担当候補を、衝突込みで比較する

誰をどの場所へ向かわせるかが外側の探索対象になっている場合、候補ごとに経路を作って比較する。静的MAPF自体は担当の組合せを生成しない。

**解く問題**：始点0・3の2体について、入力した複数の終点組から合計経路時間が最小の成功候補を選ぶ。

**入力**：`Q`、Q行の `agent0の終点 agent1の終点`。Qは非負、各行の2頂点は異なり、0〜5。

**出力**：選んだ0始まりの候補番号、合計時間、完了時刻。同点なら最初の候補。

**サンプルコード**（`examples/sample06.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});

    int q;
    if (!(cin >> q)) {
        return 0;
    }

    // 担当を比較してもagentの番号と始点は固定する
    const vector<int> starts{0, 3};

    // 地図が共通なので全候補で距離表を共有する
    mapf_workspace workspace;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    int best_index = -1;

    // 初期状態success=falseは、成功候補がまだないことを表す
    mapf_result best;

    // 目的地の担当候補を一つずつ評価する
    for (int index = 0; index < q; ++index) {
        vector<int> goals(2);
        cin >> goals[0] >> goals[1];
        // 指定した担当だけについて経路を作る
        const auto candidate = solve_mapf(graph, starts, goals, workspace, options);
        // 失敗結果の0点を良いスコアと誤解しない
        if (!candidate.success) {
            continue;
        }

        // 候補の経路を確認する
        if (!validate_mapf_result(graph, starts, goals, candidate)) {
            return 2;
        }

        // 合計時間、同点なら最大時間で比較する
        if (!best.success ||
            tie(candidate.sum_of_costs, candidate.makespan) <
                tie(best.sum_of_costs, best.makespan)) {
            // 結果は通常の値としてコピーできる
            best = candidate;
            best_index = index;
        }
    }

    // 全候補が失敗する場合も扱う
    if (!best.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 採用候補番号と採点値を返す
    cout << best_index << ' ' << best.sum_of_costs << ' ' << best.makespan << '\n';
    return 0;
}
```

**入力例**：

```text
2
2 5
1 4
```

**出力例**：

```text
1 2 1
```

**結果の読み方**：番号1の候補では、0→1と3→4の各1ターンで到着する。合計2となり、最初の候補より小さい。候補番号は0始まりである。

### 3.7 例07：手元の有効な経路を、さらに短くする

自作の初期解や保存済みの解がある場合、その解を結果構造体へ入れて改善に渡す。衝突している経路を修復する入口ではない。

**解く問題**：0–1–2の通路で、0から2までの冗長な経路を短くする。

**入力**：`L`（1以上）、続いてL個の頂点。0から始まり2で終わる合法な1体の経路。

**出力**：改善後の合計時間・最大時間と経路。不正な入力経路は `INVALID_PATH`。

**サンプルコード**（`examples/sample07.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int length;
    if (!(cin >> length)) {
        return 0;
    }

    vector<int> path(length);
    for (int& v : path) {
        cin >> v;
    }

    // 入力経路と同じ移動規則を渡す
    const mapf_graph graph(3, {{0, 1}, {1, 2}});
    // 開始位置と、経路末尾に一致する終点
    const array starts{0}, goals{2};

    // 自作経路をライブラリの結果型へ入れる
    mapf_result base;
    // 検証対象の候補であることを設定する。これだけで有効性は保証されない
    base.success = true;
    // 1体なので外側の配列は長さ1
    base.paths = {path};

    // 成否フラグとは別に移動と始終点を検査する
    if (!validate_mapf_result(graph, starts, goals, base)) {
        cout << "INVALID_PATH\n";
        return 0;
    }

    // validatorは集計値を計算しないので利用側で入れる
    base.sum_of_costs = int(path.size()) - 1;
    // 1体の最大経路時間は合計と同じ
    base.makespan = int(path.size()) - 1;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 基準経路より小さい上限だと改善されず基準解が返る
    options.max_time = max(32, base.makespan);
    // 1回に解き直すagentは1体
    options.lns_size = 1;
    // 改善を最大8回試みる
    options.lns_iterations = 8;

    // 基準解が有効なら指定目的で悪化させない
    const auto result = improve_mapf(graph, starts, goals, base, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
5
0 1 0 1 2
```

**出力例**：

```text
2 2
3 0 1 2
```

**結果の読み方**：0→1→0→1→2の4ターンから、0→1→2の2ターンへ短縮される。入力経路の妥当性確認と、集計値の設定は別々に行っている。

### 3.8 例08：一部のagentだけ、新しい目的地へ変更する

作業先が変更された台車だけを再計画する。変更しない台車の経路は固定したまま、衝突を避ける。

**解く問題**：2体の確定経路のうち、agent0の終点だけを入力値へ変更する。

**入力**：`new_goal`。0〜4。agent1の固定終点5とは重複させない。

**出力**：新しい目的地に対応したスコアと全員の経路。固定経路が邪魔で失敗する場合もある。

**サンプルコード**（`examples/sample08.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});
    // agentごとの始点・終点。各配列内の重複は禁止
    const vector<int> starts{0, 3}, old_goals{2, 5};

    int new_goal;
    if (!(cin >> new_goal)) {
        return 0;
    }

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 変更前の有効な計画を用意する
    const auto base = solve_mapf(graph, starts, old_goals, options);
    // base.success=trueがrepairの前提
    if (!base.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 変更しないagentの終点は基準経路の末尾と一致させる
    const vector<int> goals{new_goal, 5};

    // 変更可能なagent番号。空配列は全員指定ではない
    const vector<int> selected{0};

    // 既定値はbegin=0、end=-1の全経路修復
    mapf_repair_request request;
    // spanなのでselectedを呼出し完了まで生存させる
    request.agents = selected;

    // 選択したagentの始点から新終点まで作り直す
    const auto result = repair_mapf(graph, starts, goals, base, request, options);
    // 非選択agentの経路が保持されたことを確認する
    if (result.success && result.paths[1] != base.paths[1]) {
        return 3;
    }

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
1
```

**出力例**：

```text
3 2
2 0 1
3 3 4 5
```

**結果の読み方**：agent0だけが新しい終点1へ向かう。agent1の3→4→5はそのままで、合計3、全員完了時刻2となる。

### 3.9 例09：一部の経路だけを再最適化する

目的地は変更せず、遠回りや待機が多い台車だけを選んで解き直す。全体の経路を切り出す必要はない。

**解く問題**：環状通路の2体について、待機を含むagent0の経路だけを短くする。

**入力**：`restarts`。1以上の試行上限。この例の選択agentは1体なので順序は1種類。

**出力**：再最適化後のスコアと全経路。repairはLNSの反復設定を使用しない。

**サンプルコード**（`examples/sample09.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});
    // agentごとの始点・終点。各配列内の重複は禁止
    const vector<int> starts{0, 3}, goals{2, 5};

    int restarts;
    if (!(cin >> restarts)) {
        return 0;
    }

    // 2体の衝突しない基準解を用意する
    mapf_result base;
    // repairへ渡す基準解は有効である必要がある
    base.success = true;
    // agent0に不要な待機が1ターンある
    base.paths = {{0, 0, 1, 2}, {3, 4, 5}};
    // 3ターンと2ターンの合計
    base.sum_of_costs = 5;
    // 最長の経路は3ターン
    base.makespan = 3;

    // agent1はその経路全体を固定する
    const vector<int> selected{0};

    // 全経路修復の既定値を使う
    mapf_repair_request request;
    // 部分集合を番号で指定する。spanの参照先は生存させる
    request.agents = selected;

    // 続く修復にも同じ作業領域を再利用できる
    mapf_workspace workspace;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 実際には異なる優先順の数が試行数の上限になる
    options.prioritized_restarts = restarts;
    // 同じ長さの候補では、固定経路が多く通る場所を避ける方向にする
    options.congestion_weight = 1;

    // 有効な基準解より悪い候補は採用しない
    const auto result = repair_mapf(graph, starts, goals, base, request, workspace, options);
    // 非選択経路を維持したか確認する
    if (result.success && result.paths[1] != base.paths[1]) {
        return 3;
    }

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
4
```

**出力例**：

```text
4 2
3 0 1 2
3 3 4 5
```

**結果の読み方**：agent0の最初の待機がなくなり、合計は5から4へ減る。agent1の経路は変更していない。

### 3.10 例10：前後を固定し、短い時間区間だけを修復する

すでに前後の運行計画が決まっており、途中の数ターンだけを変更したい場合。時間窓の両端の位置は動かせない。

**解く問題**：正方形の通路で、基準経路の時刻1〜3だけを変え、時刻2の頂点1の閉鎖を避ける。

**入力**：`closed_time`。0〜4。この例の入力例は2。一般に、窓外に閉鎖を置くと修復できない場合がある。

**出力**：修復後のスコアと経路。末尾の重複待機は縮められるため、窓外は配列長ではなく位置の一致で考える。

**サンプルコード**（`examples/sample10.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 0から2へ二つの迂回路がある無向グラフ
    const mapf_graph graph(4, {{0, 1}, {1, 2}, {2, 3}, {3, 0}});

    int closed_time;
    if (!(cin >> closed_time)) {
        return 0;
    }

    // 時間窓修復では全員の最終goalを変更できない
    const array starts{0}, goals{2};

    // 窓の前後を含む基準経路を保持する
    mapf_result base;
    // 外部制約を除けば基準解は有効でなければならない
    base.success = true;
    // 時刻1の位置0から時刻3の位置2へだけ作り直す
    base.paths = {{0, 0, 1, 2, 2}};
    // 保存された末尾時刻までの経路時間を合計する
    base.sum_of_costs = 4;
    // 1体なので、最大経路時間も4ターンになる
    base.makespan = 4;

    // 予約の時刻は基準経路と同じ原点で指定する
    mapf_reservations external(graph.size(), 4);
    // 閉鎖時刻が予約範囲内かも確認する
    if (!external.block_vertex(closed_time, 1)) {
        return 1;
    }

    // このagentだけ変更可能とする
    const vector<int> selected{0};

    // 修復範囲をまとめて指定する
    mapf_repair_request request;
    // この配列は呼出し中に破棄しない
    request.agents = selected;
    // 時刻1の位置は基準と同じに固定される
    request.begin_time = 1;
    // 時刻3の位置も固定。beginより大きく基準makespan以下にする
    request.end_time = 3;
    // ポインタ先は呼出し終了まで有効にする
    request.external_reservations = &external;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 窓の長さが上限。options.max_timeはこのモードでは不使用
    const auto result = repair_mapf(graph, starts, goals, base, request, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result, external)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
2
```

**出力例**：

```text
3 3
4 0 0 3 2
```

**結果の読み方**：時刻1の0、時刻3の2を保ち、0→3→2の側を通る。末尾の余分な待機が省かれ、記録は時刻3までだが、時刻4も2にいると解釈する。

### 3.11 例11：一時的な通行止めを反映する

地図そのものは同じでも、特定のターンだけ使えない地点がある場合、時刻付きの予約で表す。

**解く問題**：正方形の通路で0から2へ向かう1体について、時刻1の入力頂点を避ける。

**入力**：`closed_vertex`。0〜3。

**出力**：閉鎖を回避したスコアと経路。

**サンプルコード**（`examples/sample11.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 0から2へ二つの迂回路がある無向グラフ
    const mapf_graph graph(4, {{0, 1}, {1, 2}, {2, 3}, {3, 0}});

    int closed_vertex;
    if (!(cin >> closed_vertex)) {
        return 0;
    }

    // 計画全体の始点・終点
    const array starts{0}, goals{2};

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 外部予約なしの有効な基準解
    const auto base = solve_mapf(graph, starts, goals, options);
    // 失敗結果はrepairの基準にできない
    if (!base.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 時刻0〜4の占有を登録できる
    mapf_reservations external(graph.size(), 4);
    // 時刻1だけ、その頂点の使用を禁止する
    if (!external.block_vertex(1, closed_vertex)) {
        return 1;
    }

    // 新制約で経路を変えたいagentを選ぶ
    const vector<int> selected{0};

    // 既定値は始点から終点までの修復
    mapf_repair_request request;
    // 外部予約は全員へ作用するが、変更できるのはこの集合だけ
    request.agents = selected;
    // solve_mapfには外部予約引数がないのでrepairを使う
    request.external_reservations = &external;

    // 失敗時、通行止めに違反したbaseを実行してはいけない
    const auto result = repair_mapf(graph, starts, goals, base, request, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result, external)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
1
```

**出力例**：

```text
2 2
3 0 3 2
```

**結果の読み方**：時刻1の頂点1を使わず、0→3→2と迂回する。頂点1を地図から永久に削除したわけではない。

### 3.12 例12：移動方向・橋の使用・待機を時刻で制限する

頂点の閉鎖と、あるターンの移動禁止は異なる。橋の両方向を止めても、橋の端に留まることまで禁止されるわけではない。

**解く問題**：正方形の通路で、時刻0の0→1、辺0–1の両方向、または0での待機を禁止して計画する。

**入力**：`mode`。0は片方向禁止、1は両方向禁止、2は頂点0での待機禁止。

**出力**：制約を満たすスコアと経路。予約の登録は実在する辺かどうかまでは調べない。

**サンプルコード**（`examples/sample12.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 0から2へ二つの迂回路がある無向グラフ
    const mapf_graph graph(4, {{0, 1}, {1, 2}, {2, 3}, {3, 0}});

    int mode;
    if (!(cin >> mode)) {
        return 0;
    }

    // 1体の新規計画
    const array starts{0}, goals{2};

    // 外部予約付きの新規計画用に、各始点だけの基準を作る
    mapf_result base;
    // 外部予約を除けば、基準解は有効でなければならない
    base.success = true;
    // 始点だけの基準経路。選択agentの基準末尾は新goalと違ってよい
    base.paths = {{0}};

    // 禁止移動の時刻tはt→t+1を表す
    mapf_reservations external(graph.size(), 4);
    bool registered = false;
    if (mode == 0) {
        // 0→1だけ禁止し、1→0は明示禁止しない
        registered = external.block_move(0, 0, 1);
    }

    if (mode == 1) {
        // 同じターンの0→1と1→0の両方を禁止する
        registered = external.block_edge(0, 0, 1);
    }

    if (mode == 2) {
        // from==toは、そのターンの待機禁止になる
        registered = external.block_move(0, 0, 0);
    }
    if (!registered) {
        return 1;
    }

    // 空集合では新goalへ計画されない
    const vector<int> selected{0};

    // 全経路の修復を使う
    mapf_repair_request request;
    // 始点から新goalまでを変更可能にする
    request.agents = selected;
    // 外部制約を参照させる
    request.external_reservations = &external;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 外部制約付きで全経路を構築する
    const auto result = repair_mapf(graph, starts, goals, base, request, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result, external)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
0
```

**出力例**：

```text
2 2
3 0 3 2
```

**結果の読み方**：mode=0では時刻0の0→1が禁止なので、もう一方の0→3→2を使う。mode=1なら同じターンの逆方向も禁止する。

### 3.13 例13：すでに動きが決まった別のロボットを避ける

自分が制御できない移動体の予定経路と共存する。外部経路として登録すれば、同じ場所への同時到着だけでなく正面交換も避ける。

**解く問題**：外部ロボットが1→0と進んで頂点0に留まる間に、自分のロボットを0から2へ動かす。

**入力**：`hold_until`。1〜8。外部ロボットが頂点0に留まる最後の時刻。

**出力**：外部ロボットを避けたスコアと経路。予約終了後の外部ロボットの位置は自動延長されない。

**サンプルコード**（`examples/sample13.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 0から2へ二つの迂回路がある無向グラフ
    const mapf_graph graph(4, {{0, 1}, {1, 2}, {2, 3}, {3, 0}});

    int hold_until;
    if (!(cin >> hold_until)) {
        return 0;
    }

    // 自分が制御する1体の始点と終点
    const array starts{0}, goals{2};
    // 時刻0で1、時刻1で0にいる外部ロボット
    const vector<int> other_path{1, 0};
    for (size_t i = 1; i < other_path.size(); ++i) {
        // reserve_pathはグラフを持たないので隣接を利用側で調べる
        const auto adjacent = graph.neighbors(other_path[i - 1]);
        if (other_path[i] != other_path[i - 1] &&
            !binary_search(adjacent.begin(), adjacent.end(), other_path[i])) {
            return 1;
        }
    }

    // 自分と外部ロボットの時刻0をそろえる
    mapf_reservations external(graph.size(), 8);
    // 第3引数を省略すると時刻1で予約が終わり、以後の待機は予約されない
    if (!external.reserve_path(0, other_path, hold_until)) {
        return 1;
    }

    // 外部経路付き新規計画は、始点だけを基準に全agentを修復する
    mapf_result base;
    // 外部予約を除けば、基準解は有効でなければならない
    base.success = true;
    // 始点だけの基準経路。選択agentの基準末尾は新goalと違ってよい
    base.paths = {{0}};

    // 制御する全agentを選ぶ
    const vector<int> selected{0};

    // 始点から新しいgoalまでの全経路修復
    mapf_repair_request request;
    // spanの元配列を生存させる
    request.agents = selected;
    // 外部経路との正面衝突も判定させる
    request.external_reservations = &external;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 0→1の正面交換を避ける必要がある
    const auto result = repair_mapf(graph, starts, goals, base, request, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result, external)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
4
```

**出力例**：

```text
2 2
3 0 3 2
```

**結果の読み方**：0→1では外部ロボットの1→0と正面交換になる。自分は0→3→2を使い、外部ロボットは時刻1〜4の頂点0を占有する。

### 3.14 例14：1ターン実行した後、観測位置から計画し直す

ターン制の外側プログラムで、実行済みの先頭を捨てて現在を時刻0に置き直す。予測位置と違った場合は、観測位置から短い基準を作る。

**解く問題**：0→1→2の計画を1ターン実行した後、観測位置から新しい終点へ向かう。

**入力**：`observed new_goal`。いずれも正方形グラフの0〜3。

**出力**：現在を時刻0とした新しい経路。MAPDの搬送中状態を再開する例ではない。

**サンプルコード**（`examples/sample14.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 0から2へ二つの迂回路がある無向グラフ
    const mapf_graph graph(4, {{0, 1}, {1, 2}, {2, 3}, {3, 0}});

    int observed, new_goal;
    if (!(cin >> observed >> new_goal)) {
        return 0;
    }

    // 1ターン前に使った始点・終点
    const array old_starts{0}, old_goals{2};

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 過去の計画をこの例内で用意する
    const auto old = solve_mapf(graph, old_starts, old_goals, options);
    // 基準計画がない場合は別途新規構築する
    if (!old.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    const int elapsed = 1;
    // ここでは1体だけなのでagent番号は0
    const auto& previous = old.paths[0];
    // 到着後は末尾の頂点で待機していると解釈する
    const int cut = min(elapsed, int(previous.size()) - 1);
    // 現在位置を新しい時刻0の始点にする
    const array starts{observed}, goals{new_goal};

    // 現在を起点とした基準経路を組み立て直す
    mapf_result base;
    // この後で基準経路の妥当性も検証する
    base.success = true;
    // 予測と違えば、観測位置だけの基準から再構築する
    base.paths = {{observed}};
    // 一致したときだけ残り経路を引き継ぐ
    if (previous[cut] == observed) {
        // 予測と一致する場合だけ、実行済みの先頭を除いた経路を保持する
        base.paths[0].assign(previous.begin() + cut, previous.end());
    }

    // 検証時には基準経路自体の末尾をgoalにする
    const array base_goals{base.paths[0].back()};

    // 多体へ拡張する際は全員の現在位置の重複も避ける
    if (!validate_mapf_result(graph, starts, base_goals, base)) {
        return 2;
    }

    // 目的地変更も許すため、このagentを選択する
    const vector<int> selected{0};

    // 経過時刻elapsedをbegin_timeへ入れる方式ではない
    mapf_repair_request request;
    // 始点からの全経路修復を指定する
    request.agents = selected;

    // 外部予約があるなら、その時刻原点も現在へずらす
    const auto result = repair_mapf(graph, starts, goals, base, request, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
1 3
```

**出力例**：

```text
2 2
3 1 0 3
```

**結果の読み方**：出力の先頭は観測した1であり、以前の計画の始点0ではない。新しい時刻0から、1→0→3と進む。

### 3.15 例15：道路が変わったら、地図とキャッシュを更新する

通路の追加・撤去のように、今後ずっと移動規則が変わる場合。距離キャッシュは辺の変更を自動検出しない。

**解く問題**：正方形の辺1–2を撤去し、0から2までの経路を計画し直す。

**入力**：`remove_edge`。0なら撤去しない、1なら辺1–2を撤去する。

**出力**：更新後の頂点数・次数、その後にスコアと経路。

**サンプルコード**（`examples/sample15.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int remove_edge;
    if (!(cin >> remove_edge)) {
        return 0;
    }

    vector<pair<int, int>> edges{{0, 1}, {1, 2}, {2, 3}, {3, 0}};

    // 既定構築は空のグラフ。代入してから利用する
    mapf_graph graph;
    // コンストラクタで正しい隣接配列を作る
    graph = mapf_graph(4, edges);
    // 頂点番号は地図更新の前後で同じとする
    const array starts{0}, goals{2};

    // graphへの距離キャッシュを持つ作業領域
    mapf_workspace workspace;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 元の地図で一度計画する
    const auto old = solve_mapf(graph, starts, goals, workspace, options);
    // 元の計画に失敗した場合を扱う
    if (!old.success) {
        cout << "NO_PLAN\n";
        return 0;
    }
    if (remove_edge) {
        edges.erase(edges.begin() + 1);
    }

    // graphを書き換える前に、古い距離と探索領域を破棄する
    workspace.clear();
    // 同じアドレス・頂点数でも辺が変わることに注意する
    graph = mapf_graph(4, edges);

    // 撤去辺を通る古い経路はimproveの基準にできない
    const auto result = solve_mapf(graph, starts, goals, workspace, options);
    // 更新後の頂点数と頂点1の隣接数を確認する
    cout << graph.size() << ' ' << graph.degree(1) << '\n';

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
1
```

**出力例**：

```text
4 1
2 2
3 0 3 2
```

**結果の読み方**：頂点数は4のままで、頂点1の隣接先が1個になる。新しい経路は撤去した辺1–2を使わない。

### 3.16 例16：複数の処理で、共通の計算期限を使う

構築と改善にそれぞれ丸ごと予算を与えると、合計が制限を超えてしまう。共通の絶対終了時刻を渡す。

**解く問題**：6地点の2体移動で、指定した計算予算を構築と改善に共有させる。

**入力**：`budget_ms`。有限の非負値。0なら新規構築が時間切れになる。

**出力**：スコアと経路、または `NO_PLAN`。時計依存なので結果は実行環境により変わる。期限は厳密な割込みではなく余裕を取る。

**サンプルコード**（`examples/sample16.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});
    // agentごとの始点・終点。各配列内の重複は禁止
    const vector<int> starts{0, 3}, goals{2, 5};

    double budget_ms;
    if (!(cin >> budget_ms)) {
        return 0;
    }

    // この行の時刻を起点に、全呼出し共通の期限を作る
    const auto deadline = mapf_deadline::after_ms(budget_ms);

    // 構築と改善で作業領域を再利用する
    mapf_workspace workspace;

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // deadlineはポインタを使う全呼出しが終わるまで生存させる
    options.deadline = &deadline;
    // まず構築だけで有効解を確保する
    options.lns_iterations = 0;

    // 相対時間制限は負値なので、共通期限だけで時間判定する
    auto result = solve_mapf(graph, starts, goals, workspace, options);
    // 時間切れの失敗経路は利用しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // すでに期限を過ぎていたら次の処理を始めない
    if (!deadline.expired()) {
        // 残り時間で最大32回の改善を試す
        options.lns_iterations = 32;
        // 期限が来ても有効な基準解を保持できる
        result = improve_mapf(graph, starts, goals, result, workspace, options);
    }

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
1000
```

**出力例**：

```text
4 2
3 0 1 2
3 3 4 5
```

**結果の読み方**：この入力例では期限前に有効解が得られる。構築と改善は同じdeadlineを参照するため、改善を始めた時点で予算がリセットされない。

### 3.17 例17：失敗理由を見て、必要な条件だけを変更する

ターン上限不足と、地図上の到達不能や入力間違いは別の問題である。理由を分け、上限不足のときだけ再試行する。

**解く問題**：一直線のN地点で、頂点0からN−1へ移動する。初回の上限が足りなければ伸ばす。

**入力**：`N initial_horizon`。Nは1以上、初回上限は非負。

**出力**：再試行したかを `retry=0/1` で示し、その後にスコアと経路を出す。

**サンプルコード**（`examples/sample17.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int n, horizon;
    if (!(cin >> n >> horizon)) {
        return 0;
    }

    vector<pair<int, int>> edges;
    for (int v = 1; v < n; ++v) {
        edges.push_back({v - 1, v});
    }

    // この例の始点・終点は必ず連結している
    const mapf_graph graph(n, edges);
    // 1体の始点と終点
    const array starts{0}, goals{n - 1};

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 初回は利用者が指定した移動ターン上限を使う
    options.max_time = horizon;

    // 失敗時はstatusを確認する
    auto result = solve_mapf(graph, starts, goals, options);
    bool retry = false;
    // successフラグだけでは失敗原因は区別できない
    switch (result.status) {
        // この例では上限を増やせばよい
        case mapf_status::horizon_too_short:
            retry = true;
            break;

        // すでに有効解がある
        case mapf_status::success:
            break;

        // 入力を修正すべきで、seed変更では直らない
        case mapf_status::invalid_input:
            break;

        // 始点と終点が静的地図上で連結していない
        case mapf_status::unreachable:
            break;

        // 計算予算を使い切ったので、この例では続行しない
        case mapf_status::timeout:
            break;

        // 探索失敗は不可解の証明ではないが、この例では打ち切る
        case mapf_status::search_failed:
            break;

        // 上限拡大は逆効果になり得るので続行しない
        case mapf_status::memory_limit:
            break;
    }
    cout << "retry=" << retry << '\n';
    if (retry) {
        // 1体の直線移動なので、この長さなら到着できる
        options.max_time = n - 1;
        // 一般の多体問題では、この上限で成功する保証はない
        result = solve_mapf(graph, starts, goals, options);
    }

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
5 1
```

**出力例**：

```text
retry=1
4 4
5 0 1 2 3 4
```

**結果の読み方**：初回の1ターン上限では、4辺先へ届かない。上限不足を確認して4へ増やし、再試行で到着する。

### 3.18 例18：外部予約データの登録と照会を確かめる

通行止めや外部ロボットの予定を作る層で、時刻のずれや登録漏れを調べる。占有表の検査と、移動禁止の照会は分けて考える。

**解く問題**：5地点・時刻0〜5の予約表に頂点閉鎖、片方向禁止、外部経路を登録して内容を確認する。

**入力**：`query_time`。0〜5。頂点1の占有を照会する時刻。

**出力**：既定表のvalid、表の寸法、占有値3個、禁止移動2方向の真偽、登録後のvalid。

**サンプルコード**（`examples/sample18.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int query_time;
    if (!(cin >> query_time)) {
        return 0;
    }

    // 既定状態はvertex_count=0、horizon=-1で未設定
    mapf_reservations external;
    // 未設定なのでfalseになる
    cout << external.valid() << '\n';
    // 頂点5個、最終時刻5の空き予約表へ置き換える
    external = mapf_reservations(5, 5);
    // 時刻1の頂点1を閉鎖する
    if (!external.block_vertex(1, 1)) {
        return 1;
    }

    // 時刻2から3の0→1を禁止する
    if (!external.block_move(2, 0, 1)) {
        return 1;
    }

    // 外部経路の位置列。この例では2と3は隣接する場所とする
    const array path{2, 3};
    // 時刻2で2、時刻3〜5で3を予約する
    if (!external.reserve_path(2, path, 5)) {
        return 1;
    }

    // 長さ1の外部経路
    const array instant{4};
    // hold_until省略時は時刻0だけ予約し、後続時刻は空く
    if (!external.reserve_path(0, instant)) {
        return 1;
    }

    // ownerは時刻優先で5×6セル。直接書き換えは避ける
    cout << external.vertex_count << ' ' << external.horizon << ' ' << external.owner.size()
         << '\n';
    // atは範囲内を前提とするので先に確認する
    if (!external.valid_time_vertex(query_time, 1)) {
        return 1;
    }

    // -1は空き、-2は障害、-3以下は外部経路の識別値
    cout << external.at(query_time, 1) << ' ' << external.at(5, 3) << ' ' << external.at(1, 4)
         << '\n';
    // 明示禁止だけを照会し、占有や正面衝突の総合判定はしない
    cout << external.move_is_blocked(2, 0, 1) << ' ' << external.move_is_blocked(2, 1, 0)
         << '\n';
    // 寸法と配列長の整合を確認する。経路のグラフ上の合法性は別に検査する
    cout << external.valid() << '\n';
    return 0;
}
```

**入力例**：

```text
1
```

**出力例**：

```text
0
5 5 30
-2 -3 -1
1 0
1
```

**結果の読み方**：初期状態は未設定なので0。作成後は5頂点×6時刻で30セル。-2は閉鎖、-3は外部経路、-1は空きで、0→1だけが明示禁止になっている。

### 3.19 例19：経路を検証して、各ターンの全員の操作へ変換する

ライブラリ外で作った計画や保存した計画を実行する前に、経路全体を検証する。早く到着したagentには待機操作を補う。

**解く問題**：6地点の環状通路にある2体の入力経路から、時刻ごとの同期移動列を出す。

**入力**：2体それぞれの `L 頂点列`。始点は0・3、終点は2・5に固定。

**出力**：各行は `時刻 agent0の移動元 移動先 agent1の移動元 移動先`。移動元と先が同じなら待機。不正な経路は `INVALID_PATH`。

**サンプルコード**（`examples/sample19.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});
    // agentごとの始点・終点。各配列内の重複は禁止
    const vector<int> starts{0, 3}, goals{2, 5};

    // 外から得た経路を検証用の結果構造体へ入れる
    mapf_result result;
    // validatorの入口条件。実際の正しさはこの後で確認する
    result.success = true;
    // agent番号順に2本の経路を持つ
    result.paths.resize(2);
    // ここで各agentの経路を入力する
    for (auto& path : result.paths) {
        int length;
        if (!(cin >> length) || length < 1) {
            return 1;
        }
        path.resize(size_t(length));
        for (int& v : path) {
            cin >> v;
        }
    }

    // 正面交換や頂点衝突を含めて検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        cout << "INVALID_PATH\n";
        return 0;
    }

    // 集計値はvalidatorが計算しないので、入力経路から求める
    for (const auto& path : result.paths) {
        // 待機も含めた経路時間を合計する
        result.sum_of_costs += int(path.size()) - 1;
        // 最長の経路時間を求める
        result.makespan = max(result.makespan, int(path.size()) - 1);
    }

    // 最後の1体が終わるまで操作を出す
    for (int time = 0; time < result.makespan; ++time) {
        cout << time;

        // 出力順はagent番号順。全員の移動を同時に適用する
        for (const auto& path : result.paths) {
            // 到着後は末尾の頂点に留まるので、添字を経路末尾までに制限する
            int from = path[min(time, int(path.size()) - 1)];
            int to = path[min(time + 1, int(path.size()) - 1)];
            cout << ' ' << from << ' ' << to;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
3 0 1 2
4 3 3 4 5
```

**出力例**：

```text
0 0 1 3 3
1 1 2 3 4
2 2 2 4 5
```

**結果の読み方**：最後の行ではagent0が2→2と待機し、agent1が4→5へ進む。早く終わった経路にも待機を補うことで、全員分の操作を同時に渡せる。

### 3.20 例20：大きな結果を、長くなりすぎないログへ出す

探索状態と結果を手軽に確認したい場合、結果構造体をそのままストリームへ渡す。表示が省略されても、元の配列はすべて保持されている。

**解く問題**：一直線のN地点の経路と、同じ地点で処理するJ件の搬送タスクを作り、両方の結果を表示する。

**入力**：`N J`。Nは2〜40、Jは0〜40。入力例は表示上限20を超える。

**出力**：MAPFとMAPDのデバッグ表示。表示の `size` は元の要素数、`... (+N)` は省略要素数。

**サンプルコード**（`examples/sample20.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int n, jobs;
    if (!(cin >> n >> jobs)) {
        return 0;
    }

    vector<pair<int, int>> edges;
    for (int v = 1; v < n; ++v) {
        edges.push_back({v - 1, v});
    }

    // MAPFとMAPDで共通の無向グラフを使う
    const mapf_graph graph(n, edges);
    // 1体を端から端まで移動させる
    const array starts{0}, goals{n - 1};

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // N−1ターンの移動が収まる上限
    options.max_time = n;
    // この例のログを読みやすくするため構築方式を明示する
    options.strategy = mapf_strategy::prioritized;
    // 構築は1回だけ試みる
    options.prioritized_restarts = 1;
    // この例ではLNS改善を行わない
    options.lns_iterations = 0;

    // 経路結果にはoperator<<がある
    const auto route = solve_mapf(graph, starts, goals, options);

    // 搬送側も1体。各agentのstartを設定する
    const vector<mapd_agent> agents{{0}};
    // 集荷と配達が同じ地点のタスクも有効
    const vector<mapd_task> tasks(size_t(jobs), mapd_task{0, 0, 0});

    // 搬送計画の設定
    mapd_options delivery_options;
    // この例では単独方式を使う
    delivery_options.assignment = mapd_assignment_strategy::greedy;
    // 全タスクを処理できるよう、最終時刻に余裕を持たせる
    delivery_options.max_time = jobs + 1;
    // 計算時間では打ち切らず、設定した最終時刻まで計画する
    delivery_options.time_limit_ms = -1;

    // 搬送結果にもoperator<<がある
    const auto delivery = solve_mapd(graph, agents, tasks, delivery_options);
    // ここでは正常な結果をログに出す
    if (!route.success || !delivery.valid) {
        return 2;
    }

    ostringstream log;
    // 配列・経路は各段階で先頭20件まで。省略した残り件数も出る
    log << route << '\n' << delivery << '\n';
    // cerrへ直接 result << '\n' と出してもよい。operator自体は末尾の改行を加えない
    cout << log.str();
    return 0;
}
```

**入力例**：

```text
22 21
```

**出力例**：

```text
mapf_result{success=true, status=success, sum_of_costs=21, makespan=21, expanded_states=22, attempts=1, lns_improvements=0, strategy_used=prioritized,
  paths(size=1)=[
    0: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (+2)]
  ]
}
mapd_result{valid=true, completed_tasks=21, sum_flow_time=210, total_moves=0,
  paths(size=1)=[
    0: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (+3)]
  ],
  task_agent(size=21)=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (+1)],
  task_pickup_time(size=21)=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (+1)],
  task_completion_time(size=21)=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (+1)]
}
```

**結果の読み方**：MAPFの22頂点のうち先頭20個を表示し、残り2個を省略する。MAPDの21件は時刻0〜20に1件ずつ完了するので、フロー時間の合計は0+1+…+20=210。各タスク配列の残り1件も省略表示になる。

### 3.21 例21：担当未定の荷物を、複数の台車で運ぶ

荷物ごとの集荷地点と配達地点を与え、担当の選択も搬送計画へ任せる。1体が同時に運べる荷物は1件。

**解く問題**：一直線のN地点で、K体にJ件の搬送を依頼し、Hターンまでの経路と各タスクの担当・時刻を得る。

**入力**：`N K J H`、K個の初期位置、J行の `集荷頂点 配達頂点 解禁時刻`。N・Kは1以上、J・Hは非負、初期位置は相異なる。

**出力**：完了数・フロー時間・移動数、各タスクの担当と集荷・完了時刻、各agentの経路。

**サンプルコード**（`examples/sample21.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int n, k, jobs, horizon;
    if (!(cin >> n >> k >> jobs >> horizon)) {
        return 0;
    }

    vector<pair<int, int>> edges;
    for (int v = 1; v < n; ++v) {
        edges.push_back({v - 1, v});
    }

    // 各辺の移動は1ターン
    const mapf_graph graph(n, edges);
    // 既定のstart=-1は不正なので、全員分を設定する
    vector<mapd_agent> agents(k);
    // 始点は範囲内かつ全員異なる必要がある
    for (auto& agent : agents) {
        // このagentの初期位置を設定する
        cin >> agent.start;
    }

    // タスク数0も許される
    vector<mapd_task> tasks(jobs);
    // 解禁前は割当も集荷も行わない
    for (auto& task : tasks) {
        // 集荷頂点・配達頂点・解禁時刻の順に設定する
        cin >> task.pickup >> task.delivery >> task.release_time;
    }

    // 目的は完了件数優先、同点ならフロー時間、さらに実移動数
    mapd_options options;
    // 最終時刻H。経路は各agentについてH+1頂点になる
    options.max_time = horizon;
    // 計算時間による打切りを行わない
    options.time_limit_ms = -1;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 担当・順序・経路をまとめて決める
    const auto result = solve_mapd(graph, agents, tasks, options);
    // validは全タスク完了の意味ではない
    if (!result.valid) {
        cout << "INVALID_PLAN\n";
        return 0;
    }

    // 経路と報告された完了タスクの整合を検証する
    if (!validate_mapd_result(graph, agents, tasks, result)) {
        return 2;
    }

    // 完了数、完了タスクの滞在時間合計、実移動数
    cout << result.completed_tasks << ' ' << result.sum_flow_time << ' ' << result.total_moves
         << '\n';
    for (int task = 0; task < jobs; ++task) {
        // -1は担当未定・未集荷・未完了をそれぞれ表す
        cout << result.task_agent[task] << ' ' << result.task_pickup_time[task] << ' '
             << result.task_completion_time[task] << '\n';
    }

    // 到着後の待機も含めて最終時刻Hまで明示される
    for (const auto& path : result.paths) {
        cout << path.size();
        for (int v : path) {
            cout << ' ' << v;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
6 2 2 5
0 5
0 1 0
5 4 0
```

**出力例**：

```text
2 2 2
0 0 1
1 0 1
6 0 1 1 1 1 1
6 5 4 4 4 4 4
```

**結果の読み方**：2件とも時刻0に集荷し、時刻1に配達する。フロー時間は1+1=2、実移動も2回。残りは配達先で待機するが、待機は実移動数に加算しない。

### 3.22 例22：将来解禁される搬送と、未完了の結果を読む

将来の仕事を最初から登録できる。計画終了時刻までに完了しない仕事があっても、衝突のない有効な計画は返せる。

**解く問題**：1体に、時刻1に解禁される搬送と、最終時刻H・その次に解禁される仕事を与える。

**入力**：`H`。3以上の最終時刻。

**出力**：完了数・全件完了フラグと各タスクの状態。未完了は到達不能を意味するとは限らない。

**サンプルコード**（`examples/sample22.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int horizon;
    if (!(cin >> horizon)) {
        return 0;
    }

    // 直線の通路
    const mapf_graph graph(3, {{0, 1}, {1, 2}});
    // 初期位置0の1体
    const vector<mapd_agent> agents{{0}};
    // 最終時刻に新規割当は行われないので、後ろ2件は未着手になる
    const vector<mapd_task> tasks{{0, 1, 1}, {1, 1, horizon}, {1, 2, horizon + 1}};

    // 搬送計画の設定
    mapd_options options;
    // 問題内の最終時刻をHにする
    options.max_time = horizon;
    // 計算時間による打切りを行わない。max_timeとは別の設定
    options.time_limit_ms = -1;
    // 単独方式を指定する
    options.assignment = mapd_assignment_strategy::greedy;

    // 新規割当は時刻0〜H−1。既存の搬送の到着はHでも処理する
    const auto result = solve_mapd(graph, agents, tasks, options);
    // 配列を読む前に結果の有効性を確認する
    if (!result.valid) {
        cout << "INVALID_PLAN\n";
        return 0;
    }

    // 経路・完了メタデータ・集計を照合する
    if (!validate_mapd_result(graph, agents, tasks, result)) {
        return 2;
    }

    // 完了数と全件完了かどうかを別々に出す
    cout << result.completed_tasks << ' ' << (result.completed_tasks == int(tasks.size()))
         << '\n';
    for (size_t j = 0; j < tasks.size(); ++j) {
        if (result.task_completion_time[j] >= 0) {  // 配達完了
            cout << "completed";
        } else if (result.task_pickup_time[j] >= 0) {  // 集荷済みで運搬中
            cout << "carrying";
        } else if (result.task_agent[j] >= 0) {  // 担当は決まったが集荷前
            cout << "assigned";
        } else {  // 未割当。解禁前の仕事もここに含まれる
            cout << "unassigned";
        }
        cout << '\n';
    }

    // 最終位置だけでsolve_mapdを呼び直しても、運搬中の荷物や担当は引き継がれない。
    return 0;
}
```

**入力例**：

```text
3
```

**出力例**：

```text
1 0
completed
unassigned
unassigned
```

**結果の読み方**：最初の仕事だけが完了し、完了数1・全件完了フラグ0となる。時刻Hに解禁される仕事も、新規割当の時刻を過ぎるため未着手のままである。

### 3.23 例23：共通の集荷地点がある搬送で、割当方式を比較する

同じ作業地点へ台車が集中すると詰まりやすい。端点分散を試すほか、その時点の組合せをまとめて割り当てる方式と比較する。

**解く問題**：6地点の通路で、集荷・配達地点を共有する4件のタスクを、4つの割当方式でそれぞれ最初から計画する。

**入力**：`H portfolio_runs`。Hは非負。候補数はautomaticで1〜8に丸められる。

**出力**：方式番号0〜3と、完了数・フロー時間・実移動数。どの単独方式が良いかは入力次第。

**サンプルコード**（`examples/sample23.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    int horizon, runs;
    if (!(cin >> horizon >> runs)) {
        return 0;
    }

    // 迂回できる2行3列の通路
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 5}, {5, 4}, {4, 3}, {3, 0}, {1, 4}});
    // 2体の開始位置
    const vector<mapd_agent> agents{{0}, {3}};
    // 同じ集荷・配達地点を複数タスクが共有してもよい
    const vector<mapd_task> tasks{{1, 2, 0}, {1, 5, 0}, {4, 2, 1}, {4, 5, 1}};
    // 比較する4方式を明示する
    const array modes{
        // 距離を基に軽量に割り当てる
        mapd_assignment_strategy::greedy,
        // 端点の重複を先に避けるが、必ず禁止する規則ではない
        mapd_assignment_strategy::greedy_unique_endpoints,
        // その時点の複数の割当をまとめて比較する
        mapd_assignment_strategy::min_cost_flow,
        // 複数方式とseedの候補を比較する
        mapd_assignment_strategy::automatic
    };

    for (size_t index = 0; index < modes.size(); ++index) {
        // 各方式を同じ入力・時間範囲で比較する
        mapd_options options;
        // min_cost_flowの最適化はその時点の割当であり、全体計画の最適性ではない
        options.assignment = modes[index];
        // 問題内の最終時刻をHにする
        options.max_time = horizon;
        // 計算時間による打切りを行わない。max_timeとは別の設定
        options.time_limit_ms = -1;
        // 比較の基準となる乱数seedを固定する
        options.seed = 1;
        // automaticで比較する候補数。他方式では使用しない
        options.portfolio_runs = runs;

        // 各呼出しは独立した新規計画
        const auto result = solve_mapd(graph, agents, tasks, options);
        // 方式ごとに有効性を確認する
        if (!result.valid) {
            cout << index << " INVALID_PLAN\n";
            continue;
        }

        // 得られた搬送計画の整合を確認する
        if (!validate_mapd_result(graph, agents, tasks, result)) {
            return 2;
        }

        // 完了数が同じ場合だけ後ろの指標を比較する
        cout << index << ' ' << result.completed_tasks << ' ' << result.sum_flow_time << ' '
             << result.total_moves << '\n';
    }

    return 0;
}
```

**入力例**：

```text
16 4
```

**出力例**：

```text
0 4 18 18
1 4 16 12
2 4 18 18
3 4 18 18
```

**結果の読み方**：番号0・1・2・3は、順にgreedy、端点分散、min_cost_flow、automatic。この入力では端点分散の単独実行が良い値になる。automaticの各候補はseedも変わるため、別途実行した全単独方式の結果以上になる保証はない。

### 3.24 例24：作業領域の所有者を移して、再利用する

大きな探索領域を保持する部品を、別のオブジェクトへ移したい場合。コピーではなくmoveを使い、移動元も空の状態から再利用できる。

**解く問題**：6地点の地図で前計算したworkspaceをmove構築・move代入し、移動元と先で同じ問い合わせを解く。

**入力**：`goal`。0〜5。始点は0。

**出力**：move構築後とmove代入後の前計算済み終点数、最後のスコアと経路。graph自体の寿命・変更管理は別に行う。

**サンプルコード**（`examples/sample24.cpp`）：

```cpp
// 経路計画・搬送の公開APIを読み込む
#include "multi_agent_path_finding_v06.hpp"

using namespace std;

int main() {
    // 6地点を環状につないだ無向グラフ
    const mapf_graph graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});

    int goal;
    if (!(cin >> goal)) {
        return 0;
    }

    // 比較する問い合わせは全呼出しで共通
    const array starts{0}, goals{goal};

    // コピーできない作業領域を用意する
    mapf_workspace source;
    // 指定終点への距離を前計算する
    if (!source.prepare(graph, goals)) {
        return 1;
    }

    // noexceptのmove構築。移動元は空になる
    mapf_workspace destination(std::move(source));
    // 空になった側と、引継いだ側の終点数
    cout << source.cached_target_count() << ' ' << destination.cached_target_count() << '\n';

    // 省略した設定はライブラリの既定値を使う
    mapf_options options;
    // 計算時間で打ち切らず、固定した反復数で終了する
    options.time_limit_ms = -1;
    // 移動と待機を合わせて最大32ターンまで許す
    options.max_time = 32;
    // 同じ条件で比較するため乱数seedを固定する
    options.seed = 1;

    // 移動先はキャッシュをそのまま使える
    const auto first = solve_mapf(graph, starts, goals, destination, options);

    // 移動元も再びprepareされて利用できる
    const auto second = solve_mapf(graph, starts, goals, source, options);
    // 両方の結果を確認する
    if (!first.success || !second.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 同じ固定条件なので同じ経路になることを確認する
    if (first.paths != second.paths) {
        return 2;
    }

    // move代入。元のsourceの領域は置き換えられる
    source = std::move(destination);
    // 移動後の両側の状態
    cout << source.cached_target_count() << ' ' << destination.cached_target_count() << '\n';

    // 引継いだ側で再び計画する
    const auto result = solve_mapf(graph, starts, goals, source, options);

    // 失敗時は経路を実行しない
    if (!result.success) {
        cout << "NO_PLAN\n";
        return 0;
    }

    // 始終点・衝突・移動を検証する
    if (!validate_mapf_result(graph, starts, goals, result)) {
        return 2;
    }

    // 経路時間の総和と最大値を出す
    cout << result.sum_of_costs << ' ' << result.makespan << '\n';

    // agent番号順の経路。末尾の後はその頂点で待機する
    for (const auto& output_path : result.paths) {
        cout << output_path.size();
        for (int vertex : output_path) {
            cout << ' ' << vertex;
        }
        cout << '\n';
    }

    return 0;
}
```

**入力例**：

```text
2
```

**出力例**：

```text
0 1
1 0
2 2
3 0 1 2
```

**結果の読み方**：move直後の移動元のキャッシュ数は0、移動先は1になる。移動元も後で通常どおりsolverへ渡せる。
