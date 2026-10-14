# heuristic_steiner_tree サンプルコード集

対象ソースは [heuristic_steiner_tree_v19.hpp](heuristic_steiner_tree_v19.hpp)。非負整数費用の無向グラフで、必要な地点を安くつなぐための単一ヘッダのライブラリである。

1章で問題の意味を説明し、2章に公開APIをまとめ、3章に21の用途別プログラムを載せる。各プログラムは、対象ヘッダと同じディレクトリへ一つだけコピーすれば実行できる。他の例の補助関数をコピーする必要はない。コード内の `print_ids`、`print_result` などは各例の補助処理であり、ライブラリの追加APIではない。

## 1. 何を解くライブラリか？

### 1.1 必須の地点を、途中の道を共有してつなぐ

例えば、複数の工場へケーブルを引く場面を考える。工場だけでなく、途中の交差点や中継地点も使える。一本のケーブルを複数の工場への接続に共用すれば、そのケーブルの建設費は一度だけ払えばよい。

本ライブラリには、使える接続候補を無向グラフとして渡す。「無向」は両方向へ通れること、「グラフ」は地点を頂点、接続候補を辺として表すことを意味する。接続が必須の頂点を **terminal** と呼ぶ。terminal以外の頂点は、つなぐために必要なら使ってよい。

返してほしいのは、全terminalが相互に行き来できる辺集合である。全ての頂点を接続する必要はない。rootという専用パラメータはなく、必ず接続したい出発点・供給拠点もterminalへ含める。

### 1.2 目的関数

通常の接続では、次の費用を小さくしたい。

$$\min_ {F\subseteq E} C(F),\qquad C(F)=\sum_ {e\in F}c_ {e}$$

ただし、選んだ辺集合Fによって全terminalが同じ連結成分、すなわち相互に行き来できる一つのまとまりに入ることを条件とする。

| 記号 | 意味 |
|---|---|
| $V$ | 入力グラフの全頂点。地点や中継地点の集合 |
| $E$ | 利用できる無向辺の集合。平行な辺も別々の候補として数える |
| $T\subseteq V$ | 必ず相互接続するterminalの集合 |
| $F\subseteq E$ | 選ぶ辺の集合。これが通常結果の `edges` に対応する |
| $e$ | 辺を一本指す記号 |
| $c_ {e}$ | 辺eの非負の建設費。コードでは `long long` |
| $C(F)$ | 選んだ辺の建設費の合計。通常結果の `cost` に対応する |

例えば「0と1の間が2円」「1と2の間が3円」「1と3の間が4円」で、0・2・3を接続したいなら、3本を使う総費用は9円になる。0から2までの費用5円と、0から3までの費用6円を足した11円ではない。0と1の間の2円を重複して数えないためである。

terminalが0個または1個なら、何も敷かずに条件を満たせるので費用0・辺集合は空になる。複数のterminalの間に元から道がなければ、接続不能として返る。

本ライブラリは良い実行可能解を探すもので、一般入力に対する最小費用の保証はない。費用が予算を超えた候補しか見つからなかった場合も、予算内の別案が存在しないとは限らない。

### 1.3 撤去できない既設網への追加

`augment` では、既設辺の建設費は支払い済みとして扱う。全既設辺を残し、必要な追加辺だけを選ぶ。

$$\min_ {A\subseteq E\setminus B} C_ {\mathrm{add}}(A),\qquad C_ {\mathrm{add}}(A)=\sum_ {e\in A}c_ {e}$$

| 追加の記号 | 意味 |
|---|---|
| $B\subseteq E$ | 既設辺の集合。入力時の元費用にかかわらず、今回の追加費用は0として扱う |
| $A\subseteq E\setminus B$ | 今回新たに採用する辺。`added_edges` に対応する |
| $C_ {\mathrm{add}}(A)$ | 今回の追加費用。`added_cost` に対応する |
| $B\cup A$ | 既設辺と追加辺を合わせた、実際に残すネットワーク |

接続条件は、既設辺と追加辺を合わせた網で全terminalが相互に行き来できることである。既設辺だけを自動的に一つの成分へまとめる条件ではない。既設成分同士を接続したければ、各成分の代表頂点もterminalへ入れる。

返るのは追加辺だけなので、完成後の網は利用側で既設辺と合わせて保存する。既設辺の閉路や無関係な成分も、撤去禁止なら残す。今回初めて購入する必須辺を固定する場合は、3.8のように、その元費用を外側で一度だけ加算する。

### 1.4 直接扱う範囲と、外側で組み合わせる範囲

直接扱うのは、無向辺・非負整数費用・必須頂点・辺集合のヒント・撤去できない既設辺である。有向辺、容量、通過回数、次数上限、負費用、故障に耐える二重経路などを指定する専用APIはない。

顧客を選ぶ、拠点を選ぶ、地区ごとに代表を選ぶ、といった判断は、候補の作り方や評価式を外側に置いて組み合わせる。格子や座標も、先に頂点と辺へ変換する。3章ではこれらを含めて、入力から出力までの例を示す。

## 2. APIリファレンス

### 2.1 ヘッダ、型、呼び出し方

C++20とGCCを前提とする。ヘッダは標準ライブラリの `bits/stdc++.h` を使用し、外部ライブラリのリンクは不要。ヘッダ内の自己テストは、通常の `#include` 時には無効になる。

| 公開名 | 種類と役割 |
|---|---|
| `heuristic_steiner_tree` | グラフ・再利用用の作業領域・通常グラフのcacheを保持するクラス |
| `heuristic_steiner_tree::edge_type` | 登録された辺の端点と費用を表す構造体 |
| `heuristic_steiner_tree_preset` | 探索の量や組合せを選ぶ列挙型 |
| `heuristic_steiner_tree_path_mode` | 最短路の求め方と表の利用方針を選ぶ列挙型 |
| `heuristic_steiner_tree_options` | 呼び出しごとの設定を入れる構造体 |
| `heuristic_steiner_tree_result` | `solve`・`normalize`・`improve`・`repair` の結果 |
| `heuristic_steiner_tree_augmentation_result` | `augment` の追加費用・追加辺の結果 |
| `heuristic_steiner_tree_statistics` | 各呼び出しの探索統計。両結果構造体に含まれる |

以下の表では読みやすくするため、`Solver`、`Options`、`Result`、`Augmentation`、`Statistics` を上の対応する正式名の略記として使う。これらの型別名がヘッダに定義されているわけではない。3章では正式名を使う。

### 2.2 全公開メソッド

下表の `T` と各辺ID列は全て `const std::vector<int>&`。`options` は `const Options&` で、引数にあるメソッドでは既定値が `{}`。それ以外の引数には既定値がなく、呼び出し時に指定する。

| メソッド | パラメータと有効な値 | 戻り値・動作 |
|---|---|---|
| `explicit Solver(int n)` | `n` は0以上。頂点は0..n−1。既定コンストラクタはない | n頂点・0辺のグラフを構築する。頂点数は後から変更できない |
| `int add_edge(int u, int v, long long w)` | `u`,`v` は0..n−1、`w` は0以上。多重辺・自己ループも可 | 追加順の0始まり辺IDを返す。過去の辺IDは変わらない。内部の構築済みデータとcacheを無効化する |
| `Result solve(T, options={}) const` | Tは今回必ず接続する頂点列。重複可、順不同 | 初期辺を指定せず、接続候補を作る |
| `Result normalize(T, candidate_edge_ids) const` | 候補辺IDは0..`edge_count()-1`。重複可。Tが候補内だけで接続できる必要がある | 候補辺の部分集合だけを返す。候補外の辺は追加せず、options引数もない |
| `Result improve(T, initial_edge_ids, options={}) const` | 初期辺がTを接続していること。閉路・重複・無関係な成分はあってよい | 初期辺を整理して改善する。登録済みの別辺を使い、初期辺を削除してもよい。非連結の初期解は修復せず失敗する |
| `Result repair(T, partial_edge_ids, options={}) const` | 部分辺は非連結・空でもよい。有効な登録辺IDだけを渡す | 辺の追加と削除で全terminalを接続し直す。返るのは追加分だけでなく全体解。部分辺が空なら `solve` に進む |
| `Augmentation augment(T, base_edge_ids, options={}) const` | 既設辺は有効な登録辺ID。非連結・閉路・重複も可 | 全base辺を外側で保持する前提で、追加辺と追加費用だけを返す。base辺の元費用は今回の追加費用へ加算しない |
| `long long solve_cost(T, options={}) const` | T・optionsは `solve` と同じ | 成功時は費用、失敗時は `inf()`。内部で通常の `solve` を行うため、辺復元を省く高速版ではない |
| `const edge_type& edge(int edge_id) const` | IDは0..`edge_count()-1`。範囲外は不可 | 登録辺の端点と費用を参照する。書き換え用APIではない |
| `int edge_count() const` | 引数なし | 登録済みの全辺数。選ばれた解の辺数ではない |
| `void clear_terminal_cache() const` | 引数なし | 通常グラフ用の保存表と保存terminal列を解放する。グラフや作業領域全体を消す操作ではない |
| `static constexpr long long inf()` | 引数なし | `std::numeric_limits<long long>::max()/4`。GCCの通常の64bit環境では2305843009213693951 |
| `static constexpr int terminal_cache_auto_max_k()` | 引数なし | 自動cache選択のterminal数上限32。全APIのterminal数上限でも、明示cacheの上限でもない |

`solve(T)` と `solve(T, options)` は、同じ既定引数付きメソッドの呼び出し方である。メンバ関数ポインタから呼ぶときはoptions引数も渡す。

`edge()` が返す参照は `add_edge()` の内部再確保で無効になる場合がある。辺追加をまたいで保存するときは、辺IDか `edge_type` の値コピーを使う。同じインスタンスであれば、辺IDそのものは追加後も有効である。

クラスは通常のC++のコピー・ムーブが可能。ただしコピーではグラフだけでなく構築済みの作業領域やcacheもコピーし得るので、反復利用では参照渡しで一つのインスタンスを保つ。`const` メソッドも内部の再利用状態を更新するため、同一インスタンスへの並行呼び出しには対応しない。

### 2.3 辺情報型

| `Solver::edge_type` のフィールド | 型 | 意味・初期化 |
|---|---|---|
| `from` | `int` | 登録した無向辺の一方の端点。向きの制約を表す名前ではない |
| `to` | `int` | もう一方の端点 |
| `cost` | `long long` | `add_edge` に渡した元費用。`augment` でもこの値自体を書き換えない |

この構造体にメンバの既定初期値はない。自分で変数を作るなら `edge_type{u, v, w}` のように全て指定するか、`edge_type{}` で0初期化する。`edge_type e;` とだけ書いて、値を入れる前に読む使い方はしない。辺情報型自体の `operator<<` は提供しないので、各フィールドを出力する。

### 2.4 列挙型の全ての値

`heuristic_steiner_tree_preset` の値は次の3つ。いずれも最適解を保証するモードではない。

| 値 | 意味と使い分け |
|---|---|
| `fast` | 構築と軽い改善を中心にする。短い評価を大量に行う場合の候補。深い局所改善を有効にする指定ではない |
| `balanced` | 既定値。複数の構築候補と局所改善を組み合わせ、時間と品質の折衷を狙う |
| `quality` | 候補や改善の試行を増やす。時間を配れる場合に比較する。全入力で `balanced` より良い結果になる保証はない |

`heuristic_steiner_tree_path_mode` の値は次の3つ。

| 値 | 意味と注意 |
|---|---|
| `auto_select` | 既定値。terminal数、表容量、時間見積り、presetを見て方式を選ぶ。`fast` では自動的に `on_demand` を選ぶ |
| `terminal_cache` | terminalから全頂点への距離と経路復元用情報の表を使うことを希望する。容量超過、表作成前の時間切れ、早期終了等では使用されない。利用を無条件に強制する値ではない |
| `on_demand` | terminal用の全頂点表を使わず、必要な最短路探索をその都度行う。距離・隣接情報などの作業領域まで不要になるわけではない |

代入は `heuristic_steiner_tree_preset::quality` のように型名付きで行う。列挙型にない値を整数から無理に作って渡す使い方は対象外。

自動cacheは、`fast` 以外、重複除去後のterminal数が32以下、表が `memory_limit_bytes/2` 以下、内部の時間見積りに収まる場合に選ばれる。明示 `terminal_cache` なら32を超えても候補になるが、表が `memory_limit_bytes` を超えると `on_demand` へ切り替わる。

`solve`・`augment` の逐次構築とrestartは、cache利用時はterminal数192以下、on-demand時は512以下の範囲で行う。それより多いterminalを指定すること自体は可能で、他の構築・改善を使う。`improve` および部分辺が空でない `repair` の `fast` では、明示cacheを指定してもその表を使う改善段階へ進まない。部分辺が空の `repair` は `solve` の動作に従う。

### 2.5 optionsの全フィールド

`Options options;` と構築したとき、または呼び出しのoptionsを省略したときの既定値は以下のとおり。

| フィールド | 型 | 既定値 | 値の範囲・意味 |
|---|---|---|---|
| `preset` | `heuristic_steiner_tree_preset` | `balanced` | 2.4の3値。探索量を調整する最初の選択肢 |
| `path_mode` | `heuristic_steiner_tree_path_mode` | `auto_select` | 2.4の3値。同じグラフへの反復問い合わせでは明示cacheも比較できる |
| `seed` | `std::uint64_t` | `0` | 型の全範囲。`solve`・`augment` の候補生成の乱数seed。固定入力の比較では固定する |
| `time_limit_ms` | `int` | `0` | 正値は1回の相対目標時間ms。0は相対制限なし。負値も実装上は相対制限なしだが、指定には0を使う |
| `restart_limit` | `int` | `-1` | 負値は自動。0は追加restartなし。正値はその上限。最初の構築や局所改善まで止める指定ではない |
| `memory_limit_bytes` | `size_t` | `256ULL << 20`、256MiB | 0以上。terminal表の概算予算。0は今回の表利用を抑止するが、既存表の明示的な解放は別操作 |
| `deadline` | `std::chrono::steady_clock::time_point` | `time_point::max()` | 絶対停止目標。既定値は無期限。`steady_clock` の時刻を使い、`system_clock` の時刻を渡さない |

どの設定が使われるかも重要である。

| API | preset・path_mode・相対時間・表予算・deadline | seed・restart_limit |
|---|---|---|
| `solve` / `solve_cost` | 使用する。ただし早期終了・preset等で該当段階を省くことがある | 使用する |
| `augment` | 使用する。ただし既設辺だけで接続済みなら早期終了する | 使用する |
| `improve` | 使用する。ただし `fast` や失敗時には該当段階を省く | 使用しない |
| `repair` | 部分辺がある場合に使用する | 部分辺がある場合は使用しない。空なら `solve` に委譲するので使用される |
| `normalize` | options引数を持たない | options引数を持たない |

自動restart、つまり `restart_limit<0` のときの扱いは以下。

| preset | 相対時間が0以下、かつdeadline無期限 | 正の相対時間、または有限deadlineあり |
|---|---|---|
| `fast` | 追加restart上限0 | 追加restart上限0 |
| `balanced` | 追加restart上限4 | 時間を基準に止める |
| `quality` | 追加restart上限20 | 時間を基準に止める |

有限時間の場合でも、探索の内部条件により早く終了することがある。正の `restart_limit` は `fast` でも使えるが、深い改善を有効にする効果はない。

相対時間とdeadlineを両方指定したら、早い方が停止目標になる。どちらも **soft limit** で、必ずその時刻までに戻る制約ではない。最初の実行可能解構築や進行中の処理により超過し得る。初回の内部グラフ構築時間は相対予算から除かれるため、全体の経過時間を管理するなら準備前にdeadlineを作り、呼び出し元でも次の処理を始める前に時刻を見る。

表の論理サイズは、おおむね次の式で見積もれる。

$$B_ {\mathrm{cache}}=KN\bigl(\operatorname{sizeof}(\texttt{long long})+\operatorname{sizeof}(\texttt{int})\bigr)$$

ここでNは頂点数、Kは重複除去後のterminal数、B_cacheは表のバイト数。通常のGCC環境では約12KNバイトになる。元グラフ・隣接表・作業配列・候補解・vectorの予約領域などは別に必要なので、`memory_limit_bytes` は全体メモリ使用量の上限ではない。

### 2.6 通常結果の全フィールド

`heuristic_steiner_tree_result` は次の4フィールドを持つ。

| フィールド | 型・既定値 | 戻り値としての意味 |
|---|---|---|
| `ok` | `bool`、`false` | 成功ならtrue。費用や辺を解として採用する前に必ず見る |
| `cost` | `long long`、`0` | 成功時の選択辺費用合計。通常の有限費用は0以上 `inf()` 未満 |
| `edges` | `std::vector<int>`、空 | 成功時の選択辺ID。昇順・重複なし。経路を通る順序ではない |
| `statistics` | `Statistics`、既定構築 | 今回の探索統計 |

失敗時の `cost` が必ず `inf()` であるという統一仕様ではない。判定は `ok` で行い、失敗時の費用・辺を結果として使わない。費用だけを返す `solve_cost` については、失敗時に `inf()` が返る。

### 2.7 増設結果の全フィールド

`heuristic_steiner_tree_augmentation_result` は次の4フィールドを持つ。

| フィールド | 型・既定値 | 戻り値としての意味 |
|---|---|---|
| `ok` | `bool`、`false` | 全terminalを既設辺と追加辺で接続できたか |
| `added_cost` | `long long`、`0` | 成功時の追加辺費用だけの合計。既設辺の元費用は含まない |
| `added_edges` | `std::vector<int>`、空 | 追加辺IDの昇順・重複なしの列。base辺は含まない |
| `statistics` | `Statistics`、既定構築 | 今回の探索統計 |

こちらも失敗時は `ok` で判定する。成功時の完成網は、渡したbase辺と `added_edges` の和集合。追加辺だけを次のターンの全体解として保存しない。

### 2.8 探索統計の全フィールド

| フィールド | 型・既定値 | 意味 |
|---|---|---|
| `path_mode_used` | `heuristic_steiner_tree_path_mode`、`on_demand` | 表を使う段階に到達したときの実際の方式。通常は `terminal_cache` か `on_demand`。早期終了の既定値は探索を実行した証拠ではない |
| `path_memory_bytes` | `size_t`、0 | 今回利用した最短路表の論理サイズ。再利用でも表を使えば0とは限らない。保存中の全メモリ量ではない |
| `initial_solution_count` | `int`、0 | 構築候補を作った回数。全てが返却可能解とは限らず、最終改善中の構築候補も含む |
| `restart_count` | `int`、0 | 実際に行った追加restartの回数 |
| `local_move_count` | `int`、0 | 局所変更を試した回数 |
| `improvement_count` | `int`、0 | 局所改善を採用した回数。全ての最良候補更新を数える値ではない |
| `dijkstra_count` | `long long`、0 | 今回実行した最短路探索の回数。保存表の行を再利用しただけなら、その行は加算されない |
| `edge_relaxation_count` | `long long`、0 | 最短路探索で調べた有向arcの数。無向辺の両方向や、改善しなかった走査も含む |

数値カウンタは各呼び出し単位で、solverの全実行履歴の累計ではない。通常は非負で、各整数型の範囲内で利用する。`normalize` や0〜1terminalなど、探索をしない結果では既定値のままになる。

### 2.9 デバッグ出力の全オーバーロード

次の3つを提供する。

| 関数 | 入力・戻り値・表示 |
|---|---|
| `std::ostream& operator<<(std::ostream&, const heuristic_steiner_tree_result&)` | 出力先と通常結果。成功可否・費用・辺・統計を表示し、同じ出力ストリームへの参照を返す |
| `std::ostream& operator<<(std::ostream&, const heuristic_steiner_tree_augmentation_result&)` | 出力先と増設結果。成功可否・追加費用・追加辺・統計を表示し、同じ参照を返す |
| `std::ostream& operator<<(std::ostream&, const heuristic_steiner_tree_statistics&)` | 出力先と統計。全8フィールドを表示し、同じ参照を返す |

引数の既定値はない。`std::cout << result`、`std::cerr << result`、`std::ostringstream` による文字列化に使える。改行は自動で付かないので、必要なら `<< '\n'` を添える。

- 辺配列は先頭20要素まで。総数を `edges(25)` や `added_edges(25)` と表示し、残りがあれば末尾へ `...` を付ける。
- `ok` は `true` または `false`。統計の最短路方式は列挙値の名前で表示する。
- 数値は出力先の書式に従い、`boolalpha` などの設定は変更しない。結果やsolverの状態も変更しない。
- 配列表示は先頭20要素までの処理、統計表示は定数時間。提出用出力として全辺が必要なら、`edges` を自分で走査する。

options・preset・path_mode・edge_typeを単独で表示する専用 `operator<<` は提供しない。3章の例は `std::cerr` を診断用、`std::cout` を問題の答え用に使い分ける。

### 2.10 入力制約・更新・cacheの注意

| 項目 | 扱い |
|---|---|
| 頂点・辺の番号 | 頂点は0..n−1。辺IDはそのインスタンスへの追加順。別solverの同じ整数IDは別の辺になり得る |
| 費用 | 非負の `long long`。通常の到達距離・候補費用和は `inf()` 未満にする。全入力辺費用和を `inf()` 未満にするのは安全側の十分条件。外側で足す固定費・開設費・ペナルティ等も加算可能な範囲に収める |
| 既設辺の巨大費用 | `augment` のbase辺の元費用は加算しない。通常の `solve` 等でも安全に使えることを意味しない |
| 個数とサイズ | 頂点・辺・両方向arcと内部の添字を `int` に収める。2Mや2(N−1)も含む。表の配列積は `size_t` 内に収まり、実メモリへ確保可能であること |
| 入力検査 | 主に `assert`。`NDEBUG` では消える。不正な番号・負費用を安全な失敗値に変換する入力パーサではない |
| terminalの変更 | 同じsolverへ新しい列を渡せる。重複と順序は内部で正規化する。0〜1個でも番号は有効なものだけを渡す |
| 辺追加 | `add_edge` で可能。新しい構造は必要時に再構築する。既存IDは変わらない |
| 辺削除・費用変更・頂点数変更 | 専用APIはない。現在の辺からsolverを作り直す。禁止辺を巨大費用で代用せず、登録対象から外す |
| 通常cache | 直近のterminal集合の表を一組保存する。次の集合と共通のterminal行は引き継げる。全ての過去集合を保存する仕組みではない |
| 増設時のcache | baseを費用0とした表はその呼び出しで作る一時表。通常グラフの保存表をそのまま増設用に転用せず、増設表を次回へ保存もしない |
| 表の解放 | `memory_limit_bytes=0` や `on_demand` を指定するだけでは、過去に確保した通常表の明示解放にならない。不要なら `clear_terminal_cache()` |
| 再現性 | 時間無制限・同じ入力・seed・実行環境なら決定的。時間制限付き、入力順、状態、コンパイラが異なる場合は、同じ費用や辺集合になる保証はない |

## 3. ユースケース

### 3.0 コピー方法と入出力の共通約束

各例のC++コードを **一つだけ** `example.cpp` にコピーし、対象ヘッダを同じディレクトリへ置く。どの例にも `main` があるため、複数の例をそのまま一つのファイルへ連結しない。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined example.cpp -o example
./example < input.txt
```

`input.txt` にはその例の入力例をコピーする。入力は指定形式と範囲を満たすことを前提とし、入力ファイルの欠落や壊れた入力を検出する仕組みは例に含めない。頂点・辺・マスの番号は0始まり。費用は整数で、各例の加算結果まで型の範囲に収める。

掲載する出力例は標準出力。`std::cerr` の診断は含めない。同費用の別解や時間制限のある一般入力では、費用・辺集合が変わることがある。以下の入力例と出力例は、実行して対応を確認したものである。

| 節 | 用途 | 主に使うAPI |
|---|---|---|
| 3.1 | 必須拠点をゼロからつなぐ | `solve` |
| 3.2 | 使える候補辺だけで網を整理する | `normalize` |
| 3.3 | 接続済みの外部解を改善する | `improve` |
| 3.4 | 非連結な部分解を修復する | `repair` |
| 3.5 | ターンごとに必須拠点が変わる | `solve / repair` |
| 3.6 | 撤去できない既設網へ増設する | `augment` |
| 3.7 | 複数段階で建設を確定する | `augment` |
| 3.8 | 有料の必須辺を含む一つの網を作る | `augment / edge` |
| 3.9 | 接続しない顧客のペナルティも考える | `solve / edge` |
| 3.10 | 開設費も含めて供給拠点を一つ選ぶ | `solve` |
| 3.11 | 無料の基幹網を複数の入口から利用する | `add_edge / solve` |
| 3.12 | 各グループから代表地点を選ぶ | `solve` |
| 3.13 | 同じグラフで大量の問い合わせを処理する | `solve / statistics / clear_terminal_cache` |
| 3.14 | 利用できる道路が追加される | `add_edge / repair / edge_count` |
| 3.15 | 通行禁止や費用変化に合わせて再構築する | `add_edge / repair` |
| 3.16 | 独立した複数案件の接続費を合計する | `solve / fast` |
| 3.17 | 障害物付き格子の重要マスをつなぐ | `add_edge / solve / edge` |
| 3.18 | 座標から配線費を作る | `add_edge / solve` |
| 3.19 | 共通の時間予算で複数候補を比べる | `solve / options / edge_type` |
| 3.20 | 建設予算に収まる接続を探す | `solve_cost / inf` |
| 3.21 | 2地点の接続を通過順の経路に直す | `solve / edge` |


| API・設定の確認目的 | 主な例 |
|---|---|
| `solve` と通常結果のデバッグ表示 | 3.1 |
| `normalize` / `improve` / `repair` | 3.2 / 3.3 / 3.4 |
| `augment` と増設結果のデバッグ表示 | 3.6〜3.8 |
| `edge` と `edge_type` | 3.8・3.17・3.19・3.21 |
| `edge_count` と `add_edge` による更新 | 3.14 |
| `clear_terminal_cache`、`terminal_cache_auto_max_k`、統計の単独出力 | 3.13 |
| `fast` / `balanced` / `quality` | 3.16 / 既定値を使う各例 / 3.3・3.19 |
| `auto_select` / `terminal_cache` / `on_demand` | 3.19 / 3.13 / 3.3 |
| seed、相対時間、deadline、restart、表予算 | 3.19 |
| `solve_cost` と `inf` | 3.20 |


### 3.1 必須拠点をゼロからつなぐ

工場・倉庫・配送拠点を、共用できる配線や道路でつなぎたい場合。途中の中継点は必須拠点でなくても使える。

**解く問題**：指定されたK頂点が相互に行き来できるように辺を選び、総費用を小さくする。

**入力形式**：1行目は `N M K`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 最後に必須頂点K個。

**出力形式**：成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。辺が0本なら2行目は空行。失敗時は `-1` の1行。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 最後に必須頂点K個。

    // 出力: 成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。
    // 辺が0本なら2行目は空行。
    // 失敗時は `-1` の1行。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 入力: N M K、M本の無向辺(u v w)、必須頂点K個。
    // 全費用は非負で費用和はinf()未満

    int n, m, k;
    std::cin >> n >> m >> k;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    // 既定のbalanced設定で必須頂点を接続する
    const auto result = solver.solve(terminals);

    // 標準エラーへ結果と統計を表示する。
    // 辺ID表示は先頭20個まで
    std::cerr << result << '\n';

    // 提出用の標準出力には全採用辺を出す
    print_result(result);

    return 0;
}
```

**入力例**

```text
4 4 3
0 1 2
1 2 3
1 3 4
0 3 20
0 2 3
```

**出力例**

```text
9 3
0 1 2
```

### 3.2 使える候補辺だけで網を整理する

複数の経路を重ねた結果、閉路や行き止まりが増えた場合。候補外の辺を使わずに余分な辺を落とす。

**解く問題**：P個の候補辺の部分集合から、全terminalを接続する網を作る。候補内に接続がなければ失敗とする。

**入力形式**：1行目は `N M K P`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に必須頂点K個、候補辺ID P個。

**出力形式**：成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。辺が0本なら2行目は空行。失敗時は `-1` の1行。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K P`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に必須頂点K個、候補辺ID P個。

    // 出力: 成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。
    // 辺が0本なら2行目は空行。
    // 失敗時は `-1` の1行。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 候補辺の重複・閉路は許す。
    // 元グラフに道があっても候補内で未接続なら失敗する

    int n, m, k, p;
    std::cin >> n >> m >> k >> p;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    std::vector<int> candidates(p);
    for (int& id : candidates) {
        std::cin >> id;
    }

    // 候補内だけで整理する。
    // 辺を追加せずoptionsも取らない
    const auto result = solver.normalize(terminals, candidates);

    // 成功時は整理後の総費用と辺ID、失敗時は-1を出す
    print_result(result);

    return 0;
}
```

**入力例**

```text
4 4 2 5
0 1 2
1 2 3
0 2 10
1 3 7
0 2
0 1 2 3 0
```

**出力例**

```text
5 2
0 1
```

### 3.3 接続済みの外部解を改善する

自作の貪欲法などで、すでに全必須拠点を接続できている場合。その網を出発点に、より安い接続を探す。

**解く問題**：接続済みのP辺を改善する。入力辺の削除も、入力にない登録済み辺の追加も許す。

**入力形式**：1行目は `N M K P`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に必須頂点K個、接続済みの辺ID P個。

**出力形式**：成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。辺が0本なら2行目は空行。失敗時は `-1` の1行。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K P`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に必須頂点K個、接続済みの辺ID P個。

    // 出力: 成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。
    // 辺が0本なら2行目は空行。
    // 失敗時は `-1` の1行。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // initialは全terminalを接続している必要がある。
    // 撤去禁止の辺を表す入力ではない

    int n, m, k, p;
    std::cin >> n >> m >> k >> p;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    std::vector<int> initial(p);
    for (int& id : initial) {
        std::cin >> id;
    }

    // 省略した設定値はライブラリの既定値になる
    heuristic_steiner_tree_options options;

    // 改善の候補を多めに試す。
    // 最適解の保証ではない
    options.preset = heuristic_steiner_tree_preset::quality;

    // 大きなterminal最短路表を作らずに探索する
    options.path_mode = heuristic_steiner_tree_path_mode::on_demand;

    // 非連結な初期解は修復せず失敗する
    const auto result = solver.improve(terminals, initial, options);

    // 新しい全体解を出力する。
    // 入力辺が全て残るとは限らない
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 3 2 1
0 1 2
1 2 3
0 2 20
0 2
2
```

**出力例**

```text
5 2
0 1
```

### 3.4 非連結な部分解を修復する

大きな解の一部を壊して再探索する場合や、途中まで作った網を完成させる場合。残した辺は有用なヒントとして渡す。

**解く問題**：P個の部分辺を参考に、全必須頂点を接続し直す。部分辺は非連結でも空でもよく、削除も許す。

**入力形式**：1行目は `N M K P`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に必須頂点K個、部分辺ID P個。

**出力形式**：成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。辺が0本なら2行目は空行。失敗時は `-1` の1行。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K P`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に必須頂点K個、部分辺ID P個。

    // 出力: 成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。
    // 辺が0本なら2行目は空行。
    // 失敗時は `-1` の1行。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // partialは削除可能なヒント。
    // 残す義務のある既設網にはaugmentを使う

    int n, m, k, p;
    std::cin >> n >> m >> k >> p;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    std::vector<int> partial(p);
    for (int& id : partial) {
        std::cin >> id;
    }

    // 必要な辺を追加し、不要な入力辺は削除できる
    const auto result = solver.repair(terminals, partial);

    // 追加辺だけでなく、修復後の全採用辺を出す
    print_result(result);

    return 0;
}
```

**入力例**

```text
4 3 2 2
0 1 2
1 2 3
2 3 4
0 3
0 2
```

**出力例**

```text
9 3
0 1 2
```

### 3.5 ターンごとに必須拠点が変わる

道路候補は固定だが、配送先や使用する端末が毎ターン変わる場合。前の解を保存して次のターンのヒントにする。

**解く問題**：Q個のterminal集合を順に処理する。各ターンでは、そのターンのterminalだけを接続する。

**入力形式**：1行目は `N M Q`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 続くQ件は `K` と必須頂点K個。

**出力形式**：ターン順に通常結果を出す。各成功結果は総費用・辺数の行と辺IDの行、失敗結果は `-1` の1行。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M Q`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 続くQ件は `K` と必須頂点K個。

    // 出力: ターン順に通常結果を出す。
    // 各成功結果は総費用・辺数の行と辺IDの行、失敗結果は `-1` の1行。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // グラフは全ターン共通。
    // 前ターンの辺は撤去可能で、新しいterminalに合わせて変えてよい

    int n, m, q;
    std::cin >> n >> m >> q;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    // ok=falseで初期化され、最初は保存済みの成功解がない
    heuristic_steiner_tree_result previous;

    for (int turn = 0; turn < q; ++turn) {
        int k;
        std::cin >> k;

        std::vector<int> terminals(k);
        for (int& v : terminals) {
            std::cin >> v;
        }

        // 成功解があれば辺IDをヒントにする
        const auto result = previous.ok
            ? solver.repair(terminals, previous.edges)
            : solver.solve(terminals);

        // 今回の全体解を出す。
        // 失敗時は-1
        print_result(result);

        // 成功時だけ保存し、失敗時は最後の成功解を保持する
        if (result.ok) {
            // 次のターンで使う成功解を保存する
            previous = result;
        }
    }

    return 0;
}
```

**入力例**

```text
4 3 3
0 1 2
1 2 3
2 3 4
2 0 2
2 1 3
1 3
```

**出力例**

```text
5 2
0 1
7 2
1 2
0 0

```

### 3.6 撤去できない既設網へ増設する

支払い済みのケーブルや道路を残し、新しい需要点をつなぐ場合。既設辺の入力費用は、今回の請求には含めない。

**解く問題**：rootと新しいK地点を、B本の既設辺も利用して接続する。既設辺を全て残した上で、追加費用を小さくする。

**入力形式**：1行目は `N M K B root`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に新しい地点K個、既設辺ID B個。rootは0..N−1。

**出力形式**：成功時は「追加費用 追加辺数」、追加辺ID列、完成後の全辺数、完成後の全辺ID列の4行。失敗時は `-1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

int main() {
    // 入力: 1行目は `N M K B root`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に新しい地点K個、既設辺ID B個。
    // rootは0..N−1。

    // 出力: 成功時は「追加費用 追加辺数」、追加辺ID列、完成後の全辺数、
    // 完成後の全辺ID列の4行。
    // 失敗時は `-1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // rootは接続したい既設成分の代表頂点。
    // 新しい地点1個だけをterminalにすると接続が発生しない

    int n, m, k, b, root;
    std::cin >> n >> m >> k >> b >> root;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    std::vector<int> base(b);
    for (int& id : base) {
        std::cin >> id;
    }

    terminals.push_back(root);

    // baseを撤去不能・追加費用0として使う
    const auto added = solver.augment(terminals, base);

    // 増設結果も直接デバッグ表示できる。
    // 全体解とは区別される
    std::cerr << added << '\n';
    // 失敗時の追加辺・費用は採用しない
    if (!added.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    // 今回購入する分だけの費用と本数
    std::cout << added.added_cost << ' '
              << added.added_edges.size() << '\n';
    // base辺はこの配列に含まれない
    print_ids(added.added_edges);

    // 完成網は既設辺との和集合
    base.insert(
        base.end(), added.added_edges.begin(), added.added_edges.end()
    );
    std::sort(base.begin(), base.end());
    base.erase(std::unique(base.begin(), base.end()), base.end());

    std::cout << base.size() << '\n';
    // 全base辺を保持する。
    // 無関係な既設成分や閉路も消さない
    print_ids(base);

    return 0;
}
```

**入力例**

```text
4 3 1 1 0
0 1 100
1 2 2
2 3 3
3
0
```

**出力例**

```text
5 2
1 2
3
0 1 2
```

### 3.7 複数段階で建設を確定する

毎年、毎ターン少しずつ設備を増やし、一度建設したものは撤去できない場合。購入済みの辺に再び課金しない。

**解く問題**：rootへ接続する需要をQ段階で追加する。各段階で購入した辺を次段階のbaseに加え、累計購入費を求める。

**入力形式**：1行目は `N M Q root`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 続くQ件は `K` と今回新しく接続する頂点K個。初期の既設辺は空。

**出力形式**：各成功段階で「今回の追加費用 累計購入費 全設置辺数」と全設置辺ID列の2行。失敗段階は `-1` の1行で、設置状態は変更しない。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

int main() {
    // 入力: 1行目は `N M Q root`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 続くQ件は `K` と今回新しく接続する頂点K個。
    // 初期の既設辺は空。

    // 出力: 各成功段階で「今回の追加費用 累計購入費 全設置辺数」と全設置辺ID列の2行。
    // 失敗段階は `-1` の1行で、設置状態は変更しない。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 一度買った辺は残す。
    // 各段階の需要は共通rootにつながるので、過去の需要も接続を保つ

    int n, m, q, root;
    std::cin >> n >> m >> q >> root;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> installed;
    long long spent = 0;

    for (int step = 0; step < q; ++step) {
        int k;
        std::cin >> k;

        std::vector<int> terminals(k);
        for (int& v : terminals) {
            std::cin >> v;
        }
        terminals.push_back(root);

        // 全購入済み辺を無料の既設網として引き継ぐ
        const auto added = solver.augment(terminals, installed);
        // 失敗した購入計画は確定しない
        if (!added.ok) {
            std::cout << -1 << '\n';
            continue;
        }

        // 既設分を二重計上せず、今回の購入費だけを累計に足す
        spent += added.added_cost;

        // 返却辺は既設辺と重複しない
        installed.insert(
            installed.end(), added.added_edges.begin(), added.added_edges.end()
        );
        std::sort(installed.begin(), installed.end());

        // 今回・累計・全体の3情報
        std::cout << added.added_cost << ' '
                  << spent << ' '
                  << installed.size() << '\n';
        print_ids(installed);
    }

    return 0;
}
```

**入力例**

```text
5 4 3 0
0 1 2
1 2 3
2 3 4
3 4 5
1 2
1 4
1 1
```

**出力例**

```text
5 5 2
0 1
9 14 4
0 1 2 3
0 14 4
0 1 2 3
```

### 3.8 有料の必須辺を含む一つの網を作る

契約上必ず敷く道路など、費用はかかるが選択を取り消せない辺がある場合。必須辺の費用を固定費として一度だけ足す。

**解く問題**：全terminalと全必須辺を一つの連結網に含める。必須辺以外の追加費用を小さくする。

**入力形式**：1行目は `N M K B`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に必須頂点K個、必ず採用する辺ID B個。辺IDの重複は除去する。

**出力形式**：成功時は「必須辺費用を含む総費用 全採用辺数」と全採用辺ID列の2行。失敗時は `-1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

int main() {
    // 入力: 1行目は `N M K B`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に必須頂点K個、必ず採用する辺ID B個。
    // 辺IDの重複は除去する。

    // 出力: 成功時は「必須辺費用を含む総費用 全採用辺数」と全採用辺ID列の2行。
    // 失敗時は `-1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 必須辺は既に採用が決まっている。
    // 閉路も保持し、最後にnormalizeして消してはいけない

    int n, m, k, b;
    std::cin >> n >> m >> k >> b;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    std::vector<int> mandatory(b);
    for (int& id : mandatory) {
        std::cin >> id;
    }

    std::sort(mandatory.begin(), mandatory.end());
    mandatory.erase(
        std::unique(mandatory.begin(), mandatory.end()), mandatory.end()
    );

    long long fixed_cost = 0;
    for (int id : mandatory) {
        // このsolverへ登録した辺IDの端点と費用を参照する
        const auto& edge = solver.edge(id);
        // 必須辺の元費用は外側で一度だけ計上する
        fixed_cost += edge.cost;
        // 全必須辺を同じ網へ含めるため端点も必須頂点にする
        terminals.push_back(edge.from);
        // 重複するterminalはライブラリ内で除かれる
        terminals.push_back(edge.to);
    }

    // 最適化中だけ必須辺を費用0として扱う
    const auto added = solver.augment(terminals, mandatory);
    // 必須辺の各成分まで接続できなければ失敗
    if (!added.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    // 必須辺と追加辺を全て残す
    mandatory.insert(
        mandatory.end(), added.added_edges.begin(), added.added_edges.end()
    );

    std::sort(mandatory.begin(), mandatory.end());

    // 固定費と追加費の和が総費用
    std::cout << fixed_cost + added.added_cost << ' '
              << mandatory.size() << '\n';
    print_ids(mandatory);

    return 0;
}
```

**入力例**

```text
4 3 0 2
0 1 10
1 2 2
2 3 20
0 2
```

**出力例**

```text
32 3
0 1 2
```

### 3.9 接続しない顧客のペナルティも考える

全顧客への接続が高すぎるとき、未接続の顧客に対する損失と建設費を比較する。顧客の採否を選ぶ処理をライブラリの外側に置く。

**解く問題**：rootと採用顧客をつなぐ候補を作り、「接続費＋実際に未接続の顧客のペナルティ」を小さくする。顧客数Cは0..12、顧客頂点は重複しない。

**入力形式**：1行目は `N M C root`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 続くC行は `顧客頂点 未接続ペナルティ`。ペナルティは非負。

**出力形式**：1行目に最小の評価値、続く2行に採用した通常結果を出す。評価値は接続費と一致するとは限らない。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M C root`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 続くC行は `顧客頂点 未接続ペナルティ`。
    // ペナルティは非負。

    // 出力: 1行目に最小の評価値、続く2行に採用した通常結果を出す。
    // 評価値は接続費と一致するとは限らない。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない

    // 注意: 選ばなかった顧客でも、完成した経路の途中にいれば接続済みとして扱う。
    // サンプルでは頂点1の顧客にペナルティを課さない。
    // 採否の全列挙は回数が急増するので、小さいC向け。
    // C<=12を前提に顧客の採否を全列挙する。
    // 各接続はヒューリスティックなので全体の最適性は保証されない
    // 入力辺の費用和と全ペナルティの和を合わせてもinf()未満に収める

    int n, m, c, root;
    std::cin >> n >> m >> c >> root;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> customer(c);
    std::vector<long long> penalty(c);
    for (int i = 0; i < c; ++i) {
        std::cin >> customer[i] >> penalty[i];
    }

    // 見つかった接続候補のうち最も評価値が小さいものを保存する
    heuristic_steiner_tree_result best;
    // 十分大きな初期値。
    // 通常の有限費用はこれ未満
    long long best_value = heuristic_steiner_tree::inf();

    for (int mask = 0; mask < (1 << c); ++mask) {
        std::vector<int> terminals{root};
        for (int i = 0; i < c; ++i) {
            if ((mask >> i) & 1) {
                terminals.push_back(customer[i]);
            }
        }

        // 今回選んだ顧客とrootを同じ網へ接続する
        auto result = solver.solve(terminals);
        // この顧客集合を接続できなければ次の候補を試す
        if (!result.ok) {
            continue;
        }

        std::vector<char> served(n, false);
        served[root] = true;
        // 選ばなかった顧客でも、完成網の途中にあれば接続済みになる
        for (int id : result.edges) {
            // 採用辺の端点を取得する
            const auto& edge = solver.edge(id);
            // rootを含む連結網上の頂点を記録する
            served[edge.from] = served[edge.to] = true;
        }

        // 建設費に未接続ペナルティを加える
        long long value = result.cost;
        for (int i = 0; i < c; ++i) {
            if (!served[customer[i]]) {
                value += penalty[i];
            }
        }

        // 費用だけでなく、ペナルティ込みの評価値で比較する
        if (!best.ok || value < best_value) {
            best_value = value;
            // 採用した接続結果を保存する
            best = std::move(result);
        }
    }

    std::cout << best_value << '\n';

    // 顧客を全く選ばない候補があるので、有効なrootなら少なくともその候補は成功する
    print_result(best);

    return 0;
}
```

**入力例**

```text
4 3 3 0
0 1 2
1 2 3
2 3 8
2 7
3 1
1 2
```

**出力例**

```text
6
5 2
0 1
```

**補足**：選ばなかった顧客でも、完成した経路の途中にいれば接続済みとして扱う。サンプルでは頂点1の顧客にペナルティを課さない。採否の全列挙は回数が急増するので、小さいC向け。

### 3.10 開設費も含めて供給拠点を一つ選ぶ

倉庫や基地局を候補地から一つ開設し、そこから全需要点へ接続する場合。近い拠点でも開設費が高ければ不利になる。

**解く問題**：R個の拠点候補について「開設費＋その拠点と全需要点を結ぶ接続費」を比較する。

**入力形式**：1行目は `N M K R`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に需要点K個、続くR行は `拠点頂点 開設費`。Rは1以上、開設費は非負。

**出力形式**：成功時は「選んだ拠点 総評価値」、続く2行に接続結果。どの候補も接続不能なら `-1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K R`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に需要点K個、続くR行は `拠点頂点 開設費`。
    // Rは1以上、開設費は非負。

    // 出力: 成功時は「選んだ拠点 総評価値」、続く2行に接続結果。
    // どの候補も接続不能なら `-1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 全入力辺費用と最大開設費の和をinf()未満にする。
    // rootは必須頂点として毎回追加する

    int n, m, k, r;
    std::cin >> n >> m >> k >> r;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    // 最も良かった候補の接続結果
    heuristic_steiner_tree_result best;
    // 開設費込みの比較値の初期値
    long long best_total = heuristic_steiner_tree::inf();
    int best_root = -1;

    for (int i = 0; i < r; ++i) {
        int root;
        long long opening;
        std::cin >> root >> opening;

        auto required = terminals;
        required.push_back(root);

        // 同じsolverに、拠点だけが異なるterminal集合を渡す
        auto result = solver.solve(required);
        // この拠点で需要点を接続できなければ候補から外す
        if (!result.ok) {
            continue;
        }

        // 開設費はライブラリ外で足す
        const long long total = opening + result.cost;

        // 実行可能な候補のうち総費用が最小のものを選ぶ
        if (!best.ok || total < best_total) {
            best_root = root;
            best_total = total;
            // 採用した接続結果を保存する
            best = std::move(result);
        }
    }

    // 全候補が接続不能の場合
    if (!best.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    std::cout << best_root << ' ' << best_total << '\n';

    // ここで出すcostは開設費を含まない接続費
    print_result(best);

    return 0;
}
```

**入力例**

```text
4 3 1 2
0 1 2
1 2 3
2 3 4
3
0 0
2 1
```

**出力例**

```text
2 5
4 1
2
```

### 3.11 無料の基幹網を複数の入口から利用する

離れた複数の入口が、すでに同じ無料ネットワークで相互接続されている場合。その基幹網自体をグラフに細かく描かずに表す。

**解く問題**：G個の入口を仮想頂点へ費用0でつなぎ、全需要点からこの基幹網へ到達できる物理辺を選ぶ。

**入力形式**：1行目は `N M K G`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に需要点K個、同じ基幹網の入口G個。

**出力形式**：成功時は「物理辺の総費用 物理辺数」と物理辺ID列の2行。失敗時は `-1`。仮想辺は出さない。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

int main() {
    // 入力: 1行目は `N M K G`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に需要点K個、同じ基幹網の入口G個。

    // 出力: 成功時は「物理辺の総費用 物理辺数」と物理辺ID列の2行。
    // 失敗時は `-1`。
    // 仮想辺は出さない。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 入口同士が本当に無料で相互接続済みであることが前提。
    // 単に候補入口を並べる用途ではない

    int n, m, k, g;
    std::cin >> n >> m >> k >> g;

    const int super_root = n;

    // 0..n-1は実頂点、nは基幹網を表す仮想頂点
    heuristic_steiner_tree solver(n + 1);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 物理辺を先に登録し、ID 0..m-1を割り当てる
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    for (int i = 0; i < g; ++i) {
        int gateway;
        std::cin >> gateway;

        // 基幹網への無料接続を仮想辺で表す
        solver.add_edge(super_root, gateway, 0);
    }

    terminals.push_back(super_root);

    // 仮想頂点も必須にして、全需要を基幹網へ接続する
    const auto result = solver.solve(terminals);
    // 入口がない・需要から届かない等の場合は失敗する
    if (!result.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    std::vector<int> physical;
    // 仮想辺は施工対象に含めない
    for (int id : result.edges) {
        if (id < m) {
            physical.push_back(id);
        }
    }

    // 仮想辺の費用は0なのでcostは物理辺費用と等しい
    std::cout << result.cost << ' ' << physical.size() << '\n';
    // 物理辺だけでは複数成分でも、既設基幹網を介して全体は接続する
    print_ids(physical);

    return 0;
}
```

**入力例**

```text
4 2 2 2
0 1 2
2 3 3
1 3
0 2
```

**出力例**

```text
5 2
0 1
```

### 3.12 各グループから代表地点を選ぶ

各地区のどこか一か所へ通信を届ければよい場合。地区内の全地点をterminalにすると、必要以上の接続になる。

**解く問題**：各グループから代表を一つ選び、共通rootと代表地点を接続する。代表の全組合せを比較する。

**入力形式**：1行目は `N M G root`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 Gは0..12。続くG件は `候補数 候補頂点列`。各候補数は1以上、組合せ数の積は1000以下を想定。

**出力形式**：成功時はグループ順の代表地点列、その後に通常結果の2行。G=0なら代表地点列は空行。全組合せが接続不能なら `-1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M G root`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // Gは0..12。
    // 続くG件は `候補数 候補頂点列`。
    // 各候補数は1以上、組合せ数の積は1000以下を想定。

    // 出力: 成功時はグループ順の代表地点列、その後に通常結果の2行。
    // G=0なら代表地点列は空行。
    // 全組合せが接続不能なら `-1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // グループ選択はライブラリの外側で行う。
    // 選んだ代表の接続はヒューリスティック評価になる

    int n, m, groups_count, root;
    std::cin >> n >> m >> groups_count >> root;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<std::vector<int>> groups(groups_count);
    for (auto& group : groups) {
        int size;
        std::cin >> size;
        group.resize(size);
        for (int& v : group) {
            std::cin >> v;
        }
    }

    std::vector<int> chosen(groups_count), best_chosen;

    // これまでで最小費用の代表点接続
    heuristic_steiner_tree_result best;

    const auto enumerate = [&](auto&& self, int index) -> void {
        if (index == groups_count) {
            auto terminals = chosen;
            terminals.push_back(root);

            // 各地区の代表1点とrootだけを必須にする
            auto result = solver.solve(terminals);
            // 接続できた組合せの費用を比較する
            if (result.ok && (!best.ok || result.cost < best.cost)) {
                best_chosen = chosen;
                // 採用した代表に対応する辺集合も保存する
                best = std::move(result);
            }
            return;
        }

        for (int v : groups[index]) {
            chosen[index] = v;
            self(self, index + 1);
        }
    };

    enumerate(enumerate, 0);

    // 全候補でrootへの接続が作れなかった場合
    if (!best.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    print_ids(best_chosen);

    // 代表以外の頂点も中継点として使われ得る
    print_result(best);

    return 0;
}
```

**入力例**

```text
5 4 2 0
0 1 2
0 2 8
1 3 3
2 4 10
2 1 2
2 3 4
```

**出力例**

```text
1 3
5 2
0 2
```

### 3.13 同じグラフで大量の問い合わせを処理する

候補となる需要集合の費用を何度も評価する場合。特にterminal集合に共通点が多いなら、保存済みの最短路表を再利用できる。

**解く問題**：Q個の独立したterminal集合を同じsolverで解く。各問い合わせに前回の解を押し付けず、共有できる内部データを再利用する。

**入力形式**：1行目は `N M Q`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 続くQ件は `K` と必須頂点K個。

**出力形式**：問い合わせ順に通常結果を出す。標準エラーには自動cacheのterminal上限と各問い合わせの統計を出す。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M Q`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 続くQ件は `K` と必須頂点K個。

    // 出力: 問い合わせ順に通常結果を出す。
    // 標準エラーには自動cacheのterminal上限と各問い合わせの統計を出す。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 同じインスタンスを使い続ける。
    // 内部状態を更新するため並行呼び出しはしない

    int n, m, q;
    std::cin >> n >> m >> q;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    // balanced等の既定値を基に必要な設定だけ変える
    heuristic_steiner_tree_options options;

    // terminalから全頂点への表の利用を希望する
    options.path_mode = heuristic_steiner_tree_path_mode::terminal_cache;

    // 表の概算予算64MiB。
    // solver全体のメモリ上限ではない
    options.memory_limit_bytes = 64ULL << 20;

    // auto_selectのterminal数上限。
    // 明示cacheの上限ではない
    std::cerr << heuristic_steiner_tree::terminal_cache_auto_max_k()
              << '\n';

    for (int query = 0; query < q; ++query) {
        int k;
        std::cin >> k;

        std::vector<int> terminals(k);
        for (int& v : terminals) {
            std::cin >> v;
        }

        // 共通terminalの表を再利用し、必要な行だけ作り直す
        const auto result = solver.solve(terminals, options);

        // 解の費用・辺を標準出力へ出す
        print_result(result);

        // 表が予算を超えるとon_demandへ戻るので実際の方式を確認できる
        std::cerr << result.statistics << '\n';
    }

    // 反復評価が終わったら通常グラフ用の保存表を解放する。
    // 辺は消えない
    solver.clear_terminal_cache();

    return 0;
}
```

**入力例**

```text
4 3 2
0 1 2
1 2 3
2 3 4
2 0 2
2 0 3
```

**出力例**

```text
5 2
0 1
9 3
0 1 2
```

### 3.14 利用できる道路が追加される

新しい道や接続候補が順次追加され、既存の候補は残る場合。solverを作り直さずに新辺を登録できる。

**解く問題**：初期網を作り、その後Q本の新辺を一本ずつ追加して同じterminal集合を解き直す。

**入力形式**：1行目は `N M K Q`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 次に必須頂点K個、続くQ行は新辺 `u v w`。

**出力形式**：まず初期の通常結果。以後、各新辺の登録IDを1行、その追加後の通常結果を出す。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K Q`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 次に必須頂点K個、続くQ行は新辺 `u v w`。

    // 出力: まず初期の通常結果。
    // 以後、各新辺の登録IDを1行、その追加後の通常結果を出す。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 頂点数は固定。
    // add_edgeでは既存辺のIDは変わらないが、内部の探索用構造とcacheは無効化される

    int n, m, k, q;
    std::cin >> n >> m >> k >> q;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    // 最初のグラフの解を保存する
    auto previous = solver.solve(terminals);

    // 接続不能でも、後の辺追加で接続できることがある
    print_result(previous);

    for (int update = 0; update < q; ++update) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 新辺の0始まりIDを受け取る。
        // edge()の参照は追加をまたいで保持しない
        const int id = solver.add_edge(u, v, w);
        std::cout << id << '\n';

        // 旧辺集合を削除可能なヒントにする
        previous = previous.ok
            ? solver.repair(terminals, previous.edges)
            : solver.solve(terminals);

        // 更新後の全体解を出す
        print_result(previous);
        // 登録済みの総辺数を確認する
        std::cerr << solver.edge_count() << '\n';
    }

    return 0;
}
```

**入力例**

```text
3 2 2 1
0 1 10
1 2 10
0 2
0 2 3
```

**出力例**

```text
20 2
0 1
2
3 1
2
```

### 3.15 通行禁止や費用変化に合わせて再構築する

辺を使えなくしたい、料金が変わった、頂点数が変わった場合。本体に削除・費用変更APIはないため、変更後のグラフを作り直す。

**解く問題**：現在利用できる辺だけを登録し直し、前の解の外部辺IDを、新solverのIDへ変換して修復する。

**入力形式**：1行目は `N M K P`。続くM行は `外部辺ID u v w`。外部辺IDは非負・一意。次に必須頂点K個、旧解の外部辺ID P個。消えた辺はM行に含めない。

**出力形式**：成功時は「変更後の総費用 採用辺数」と外部辺IDの昇順列。失敗時は `-1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

int main() {
    // 入力: 1行目は `N M K P`。
    // 続くM行は `外部辺ID u v w`。
    // 外部辺IDは非負・一意。
    // 次に必須頂点K個、旧解の外部辺ID P個。
    // 消えた辺はM行に含めない。

    // 出力: 成功時は「変更後の総費用 採用辺数」と外部辺IDの昇順列。
    // 失敗時は `-1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 現在の頂点番号で新しいグラフを与える。
    // 旧solverの内部IDをそのまま渡してはいけない

    int n, m, k, p;
    std::cin >> n >> m >> k >> p;

    // 辺削除・費用変更・頂点数変更は新しいインスタンスで反映する
    heuristic_steiner_tree solver(n);
    std::vector<int> external(m);
    std::vector<std::pair<int, int>> mapping;
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> external[i] >> u >> v >> w;

        // 現在の登録順で新しい内部IDが付く
        const int id = solver.add_edge(u, v, w);
        mapping.push_back({external[i], id});
    }

    std::sort(mapping.begin(), mapping.end());

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    std::vector<int> partial;
    for (int i = 0; i < p; ++i) {
        int old_id;
        std::cin >> old_id;

        const auto it = std::lower_bound(
            mapping.begin(), mapping.end(), std::pair<int, int>{old_id, -1}
        );
        if (it != mapping.end() && it->first == old_id) {
            partial.push_back(it->second);
        }
    }

    // 現存する旧辺だけを、新しい内部IDで渡す
    const auto result = solver.repair(terminals, partial);
    // 通行禁止等により必須頂点を接続できなくなった場合
    if (!result.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    std::vector<int> selected;
    // 利用側で意味のある外部IDへ戻す
    for (int id : result.edges) {
        selected.push_back(external[id]);
    }
    std::sort(selected.begin(), selected.end());

    // 費用は変更後の辺費用で再計算されている
    std::cout << result.cost << ' ' << selected.size() << '\n';
    print_ids(selected);

    return 0;
}
```

**入力例**

```text
3 3 2 2
200 1 2 3
400 0 2 9
100 0 1 2
0 2
100 300
```

**出力例**

```text
5 2
100 200
```

### 3.16 独立した複数案件の接続費を合計する

部署ごと・顧客企業ごとに別設備を敷設する場合。案件同士を接続する必要はなく、同じ道路上を通っても別々に費用がかかる。

**解く問題**：Q案件それぞれの必須頂点だけを接続し、案件別の費用を合計する。案件間の容量制約や、共用による割引はない。

**入力形式**：1行目は `N M Q`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 続くQ件は `K` とその案件の必須頂点K個。全案件の費用合計もlong longに収める。

**出力形式**：各案件の通常結果を順に出し、最後に `TOTAL 合計費用`。一つでも接続不能なら最後は `TOTAL -1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M Q`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 続くQ件は `K` とその案件の必須頂点K個。
    // 全案件の費用合計もlong longに収める。

    // 出力: 各案件の通常結果を順に出し、最後に `TOTAL 合計費用`。
    // 一つでも接続不能なら最後は `TOTAL -1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 別案件の辺が重なっても費用は案件ごとに数える。
    // 一本を共同購入する問題はこの分割では解けない

    int n, m, q;
    std::cin >> n >> m >> q;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    // 多数の小さな案件を短く評価する設定を作る
    heuristic_steiner_tree_options options;

    // 深い改善を省く。
    // 品質が十分かは自分の入力で確認する
    options.preset = heuristic_steiner_tree_preset::fast;

    bool all_ok = true;
    long long total = 0;

    for (int job = 0; job < q; ++job) {
        int k;
        std::cin >> k;

        std::vector<int> terminals(k);
        for (int& v : terminals) {
            std::cin >> v;
        }

        // 全案件のterminalをまとめず、一案件ずつ接続する
        const auto result = solver.solve(terminals, options);

        // この案件だけの採用辺を出す
        print_result(result);

        // 同じ辺でも別案件なら別の敷設費として足す
        if (result.ok) {
            // この案件の接続費を全案件の合計へ足す
            total += result.cost;
        } else {
            all_ok = false;
        }
    }

    std::cout << "TOTAL " << (all_ok ? total : -1) << '\n';

    return 0;
}
```

**入力例**

```text
4 3 2
0 1 2
1 2 3
2 3 4
2 0 2
2 1 3
```

**出力例**

```text
5 2
0 1
7 2
1 2
TOTAL 12
```

### 3.17 障害物付き格子の重要マスをつなぐ

建物や壁を避けながら通路・配線を引く場合。通れるマスを頂点、上下左右の隣接を辺に変換する。

**解く問題**：壁以外の隣接マスを結ぶ長さ1の辺を選び、K個の重要マスを接続する。

**入力形式**：1行目は `H W K`。H行の地図は通行可能な `.` と壁 `#`。続くK行は `行 列`。H,Wは1以上、行列番号は0始まりで範囲内。

**出力形式**：成功時は「総延長 採用隣接辺数」、続いて各辺を `行1 列1 行2 列2` で出す。壁上の必須点、接続不能なら `-1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

int main() {
    // 入力: 1行目は `H W K`。
    // H行の地図は通行可能な `.` と壁 `#`。
    // 続くK行は `行 列`。
    // H,Wは1以上、行列番号は0始まりで範囲内。

    // 出力: 成功時は「総延長 採用隣接辺数」、続いて各辺を `行1 列1 行2 列2` で出す。
    // 壁上の必須点、接続不能なら `-1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // マス数H*Wと隣接辺数はint範囲内に収める。
    // 斜め移動は許さない

    int h, w, k;
    std::cin >> h >> w >> k;

    std::vector<std::string> grid(h);
    for (auto& row : grid) {
        std::cin >> row;
    }

    std::vector<std::vector<int>> vertex(h, std::vector<int>(w, -1));
    std::vector<std::pair<int, int>> cell;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            if (grid[row][col] == '#') {
                continue;
            }
            vertex[row][col] = static_cast<int>(cell.size());
            cell.push_back({row, col});
        }
    }

    // 通れるマスだけを頂点として持つ
    heuristic_steiner_tree solver(static_cast<int>(cell.size()));
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            if (vertex[row][col] < 0) {
                continue;
            }
            if (row + 1 < h && vertex[row + 1][col] >= 0) {
                // 下向きの隣接を1回だけ無向辺にする
                solver.add_edge(vertex[row][col], vertex[row + 1][col], 1);
            }
            if (col + 1 < w && vertex[row][col + 1] >= 0) {
                // 右向きの隣接を1回だけ無向辺にする
                solver.add_edge(vertex[row][col], vertex[row][col + 1], 1);
            }
        }
    }

    std::vector<int> terminals;
    for (int i = 0; i < k; ++i) {
        int row, col;
        std::cin >> row >> col;
        if (vertex[row][col] < 0) {
            std::cout << -1 << '\n';
            return 0;
        }
        terminals.push_back(vertex[row][col]);
    }

    // 重要マスを全て接続する隣接辺を選ぶ
    const auto result = solver.solve(terminals);
    // 障害物で重要マスが別成分に分かれている場合
    if (!result.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    // 単位費用1なのでcostが総延長になる
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 昇順の辺IDを、元のマス座標へ戻す
    for (int id : result.edges) {
        // 辺の端点は圧縮後の頂点番号
        const auto& edge = solver.edge(id);
        // 始点側を行列座標へ変換する
        const auto [r1, c1] = cell[edge.from];
        // 終点側を行列座標へ変換する。
        // 無向なので向きに意味はない
        const auto [r2, c2] = cell[edge.to];
        std::cout << r1 << ' ' << c1 << ' ' << r2 << ' ' << c2 << '\n';
    }

    return 0;
}
```

**入力例**

```text
2 3 2
...
##.
0 0
1 2
```

**出力例**

```text
3 3
0 0 0 1
0 1 0 2
0 2 1 2
```

### 3.18 座標から配線費を作る

候補地点に座標があり、縦横に配線するときの長さで費用を決めたい場合。地点間を結べるかどうかは別の入力として与える。

**解く問題**：許可された地点対だけを辺とし、横方向の距離と縦方向の距離の和を費用として必須地点を接続する。

**入力形式**：1行目は `N M K`。続くN行は整数座標 `x y`、続くM行は許可する地点対 `u v`、最後に必須地点K個。座標の絶対値は10^8以下。

**出力形式**：成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。辺が0本なら2行目は空行。失敗時は `-1` の1行。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K`。
    // 続くN行は整数座標 `x y`、続くM行は許可する地点対 `u v`、最後に必須地点K個。
    // 座標の絶対値は10^8以下。

    // 出力: 成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。
    // 辺が0本なら2行目は空行。
    // 失敗時は `-1` の1行。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない

    // 注意: 連続平面上の好きな位置に中継点を自動追加する問題ではない。
    // 線分同士が途中で重なる場合も、自動では費用を共通化しない。
    // 共有したい場所を頂点と区間へ分けて入力する。
    // 費用は|x1-x2|+|y1-y2|。
    // 障害物や線分途中の交差点を自動で判定・追加する機能はない

    int n, m, k;
    std::cin >> n >> m >> k;

    std::vector<long long> x(n), y(n);
    for (int i = 0; i < n; ++i) {
        std::cin >> x[i] >> y[i];
    }

    // 入力で用意した候補地点だけを頂点にする
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        std::cin >> u >> v;
        const long long cost = std::abs(x[u] - x[v]) + std::abs(y[u] - y[v]);

        // 許可された地点対だけを登録する。
        // 辺IDはこの入力順
        solver.add_edge(u, v, cost);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    // 整数化済みの費用で接続を求める
    const auto result = solver.solve(terminals);

    // costは縦横距離の合計。
    // 実際の配線形状は利用側で決める
    print_result(result);

    return 0;
}
```

**入力例**

```text
4 3 3
0 0
2 0
2 3
2 -4
0 1
1 2
1 3
0 2 3
```

**出力例**

```text
9 3
0 1 2
```

**補足**：連続平面上の好きな位置に中継点を自動追加する問題ではない。線分同士が途中で重なる場合も、自動では費用を共通化しない。共有したい場所を頂点と区間へ分けて入力する。

### 3.19 共通の時間予算で複数候補を比べる

コンテストの残り時間を一つの終了時刻で管理しながら、乱数seedを変えて候補を試す場合。途中の良い解を失わないように保存する。

**解く問題**：全体の目標時間Bミリ秒以内を目安に、最大R回の候補構築を行い、最も安い成功解を選ぶ。Bは1以上、Rは1以上。

**入力形式**：1行目は `N M K B R`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 最後に必須頂点K個。

**出力形式**：成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。辺が0本なら2行目は空行。失敗時は `-1` の1行。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

// solve等が返す通常結果を受け取る
void print_result(const heuristic_steiner_tree_result& result) {
    // 失敗時は費用・辺を解として使わない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return;
    }

    // 総費用と採用辺数
    std::cout << result.cost << ' ' << result.edges.size() << '\n';

    // 辺IDは追加順の0始まりで、経路を通る順番ではない
    print_ids(result.edges);
}

int main() {
    // 入力: 1行目は `N M K B R`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 最後に必須頂点K個。

    // 出力: 成功時は1行目に「総費用 採用辺数」、2行目に辺IDを空白区切りで出す。
    // 辺が0本なら2行目は空行。
    // 失敗時は `-1` の1行。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない

    // 注意: 同じseedでも時間制限付きでは探索量が変わる。
    // 本例の入力は必要な経路が一本に決まるため出力例が安定している。
    // 一般入力では、掲載と異なる成功解が返っても誤りとは限らない。
    // 時間制限はsoft limit。
    // 入力読み込み後の構築時間も含めたdeadlineを作るが、終了時刻の厳守は保証されない

    int n, m, k, budget_ms, attempts;
    std::cin >> n >> m >> k >> budget_ms >> attempts;

    // 公開の辺情報型に、構築前の入力を一時保存する
    std::vector<heuristic_steiner_tree::edge_type> edges(m);
    // from,toは端点、costは非負の整数費用
    for (auto& edge : edges) {
        // 辺情報型の端点と非負費用を入力する
        std::cin >> edge.from >> edge.to >> edge.cost;
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(budget_ms);

    // deadlineを設定した後にグラフを構築する
    heuristic_steiner_tree solver(n);
    // 入力順のIDで全辺を登録する
    for (const auto& edge : edges) {
        // 保存した辺情報から無向辺を登録する
        solver.add_edge(edge.from, edge.to, edge.cost);
    }

    // 各呼び出しへ渡す探索設定
    heuristic_steiner_tree_options options;

    // 改善を多めに試す設定を選ぶ
    options.preset = heuristic_steiner_tree_preset::quality;

    // 時間・表容量・terminal数から方式を選ばせる
    options.path_mode = heuristic_steiner_tree_path_mode::auto_select;

    // 表の予算32MiB。
    // 全体メモリ制限ではない
    options.memory_limit_bytes = 32ULL << 20;

    // 1回あたりの相対目標10ms。
    // 初回の内部構築時間などは別に必要
    options.time_limit_ms = 10;

    // 全呼び出しで同じ絶対終了目標を共有する
    options.deadline = deadline;

    // 1回につき追加restartを最大8回に抑える。
    // 局所改善等は別枠
    options.restart_limit = 8;

    // 最も良かった成功解を保持する
    heuristic_steiner_tree_result best;

    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (attempt != 0 && std::chrono::steady_clock::now() >= deadline) {
            break;
        }

        // 0,1,2,...とseedを変える。
        // 改善するとは限らない
        options.seed = static_cast<std::uint64_t>(attempt);

        // 相対時間とdeadlineの早い方を停止目標にする
        auto result = solver.solve(terminals, options);
        // 良い成功解だけで置き換える
        if (result.ok && (!best.ok || result.cost < best.cost)) {
            // 今回の成功解を、現在の最良解として保存する
            best = std::move(result);
        }
    }

    // 実行環境で探索量が変わる。
    // 最初の1回は解を得るため期限後でも開始する
    print_result(best);

    return 0;
}
```

**入力例**

```text
4 3 2 30 3
0 1 2
1 2 3
2 3 4
0 3
```

**出力例**

```text
9 3
0 1 2
```

**補足**：同じseedでも時間制限付きでは探索量が変わる。本例の入力は必要な経路が一本に決まるため出力例が安定している。一般入力では、掲載と異なる成功解が返っても誤りとは限らない。

### 3.20 建設予算に収まる接続を探す

辺の詳細より、まず予算内でつなげられる案があるかを知りたい場合。費用だけを受け取り、予算と比較する。

**解く問題**：全必須頂点を接続する候補を一つ評価し、費用が予算以下なら成功案ありと判定する。

**入力形式**：1行目は `N M K budget`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 最後に必須頂点K個。budgetは非負。

**出力形式**：`FOUND 費用` は予算内の案を発見、`NOT_FOUND 費用` は今回の案が予算超過、`DISCONNECTED` は必須頂点が接続不能。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

int main() {
    // 入力: 1行目は `N M K budget`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // 最後に必須頂点K個。
    // budgetは非負。

    // 出力: `FOUND 費用` は予算内の案を発見、`NOT_FOUND 費用` は今回の案が予算超過、
    // `DISCONNECTED` は必須頂点が接続不能。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // NOT_FOUNDは予算内解の不存在を証明しない。
    // solve_costも内部では通常のsolveを実行する

    int n, m, k;
    long long budget;
    std::cin >> n >> m >> k >> budget;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    std::vector<int> terminals(k);
    for (int& v : terminals) {
        std::cin >> v;
    }

    // 成功時は接続費、失敗時はinf()を返す
    const long long cost = solver.solve_cost(terminals);
    // solve_costではこの値で失敗を判定できる
    if (cost == heuristic_steiner_tree::inf()) {
        std::cout << "DISCONNECTED\n";
    } else if (cost <= budget) {
        std::cout << "FOUND " << cost << '\n';
    } else {
        std::cout << "NOT_FOUND " << cost << '\n';
    }

    return 0;
}
```

**入力例**

```text
3 2 2 6
0 1 2
1 2 3
0 2
```

**出力例**

```text
FOUND 5
```

### 3.21 2地点の接続を通過順の経路に直す

接続結果の辺ID列を、そのまま移動順として使いたくなる場合。返却辺はID順なので、端点情報から経路を復元する。

**解く問題**：sとtを接続し、選ばれた辺上でsからtまで辿る頂点列を出す。s=tの場合は費用0、頂点列はsだけ。

**入力形式**：1行目は `N M s t`。続くM行は `u v w`。各辺のIDは0からM−1までの入力順。 Nは1以上、s,tは0..N−1。

**出力形式**：成功時は「費用 経路の頂点数」とsからtへの頂点列の2行。接続不能なら `-1`。

**コード**

```cpp
// このヘッダだけでライブラリを利用できる
#include "heuristic_steiner_tree_v19.hpp"

void print_ids(const std::vector<int>& ids) {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << ids[i];
    }
    std::cout << '\n';
}

int main() {
    // 入力: 1行目は `N M s t`。
    // 続くM行は `u v w`。
    // 各辺のIDは0からM−1までの入力順。
    // Nは1以上、s,tは0..N−1。

    // 出力: 成功時は「費用 経路の頂点数」とsからtへの頂点列の2行。
    // 接続不能なら `-1`。

    // 入力番号は範囲内、費用は非負、通常の経路・候補費用はinf()未満とし、
    // 外側の加算もオーバーフローさせない
    // 2地点だけなら専用の最短路法を使う選択肢もある。
    // この例は返却辺から順序を復元するためのもの

    int n, m, source, target;
    std::cin >> n >> m >> source >> target;

    // n頂点の無向グラフを用意する。
    // 頂点は0..n-1
    heuristic_steiner_tree solver(n);
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long w;
        std::cin >> u >> v >> w;

        // 非負費用wの辺を登録する。
        // この辺のIDはi
        solver.add_edge(u, v, w);
    }

    // terminalは2地点。
    // 同じ頂点を重複指定してもよい
    const auto result = solver.solve({source, target});
    // 元グラフで到達できない場合は経路を作れない
    if (!result.ok) {
        std::cout << -1 << '\n';
        return 0;
    }

    std::vector<std::vector<int>> adjacency(n);
    // この配列の順番は経路順ではなく、辺IDの昇順
    for (int id : result.edges) {
        // 端点と費用を値コピーする
        const heuristic_steiner_tree::edge_type edge = solver.edge(id);
        // 無向辺の両方向を経路復元用の隣接表へ追加する
        adjacency[edge.from].push_back(edge.to);
        // 頂点番号は元グラフと同じ0始まり
        adjacency[edge.to].push_back(edge.from);
    }

    std::vector<int> parent(n, -1);
    std::queue<int> queue;
    parent[source] = source;
    queue.push(source);
    while (!queue.empty()) {
        const int u = queue.front();
        queue.pop();
        for (int v : adjacency[u]) {
            if (parent[v] < 0) {
                parent[v] = u;
                queue.push(v);
            }
        }
    }

    std::vector<int> path{target};
    while (path.back() != source) {
        path.push_back(parent[path.back()]);
    }
    std::reverse(path.begin(), path.end());

    // 費用は選択辺の費用で、頂点数ではない
    std::cout << result.cost << ' ' << path.size() << '\n';
    print_ids(path);

    return 0;
}
```

**入力例**

```text
4 4 0 3
2 3 4
0 1 2
1 2 3
0 3 20
```

**出力例**

```text
9 4
0 1 2 3
```

### 掲載コードの検証

全21例を個別のC++20プログラムとして、`-Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined -Werror` を付けてコンパイルし、入力例と出力例の完全一致を確認した。個別検証は `-O0`、同じコードをまとめた検証用プログラムは `-O2` とASan／UBSanでも実行した。

さらに、独立に計算した木の経路・辺費用・連結性・小さな全列挙との比較を含むランダム検査630件と、サニタイザ付きの境界検査32件を行った。対象には空集合、接続不能、0費用、重複辺ID、必須辺の閉路・自己ループ、候補の全不成立、グラフ再構築、壁上の要求、同じ始終点などを含む。

実行検証環境はGCC 13.3.0。C++20を使用し、GCC 12.2での実行確認は行っていない。これらは掲載プログラムの動作確認であり、一般入力の最適性を保証するものではない。
