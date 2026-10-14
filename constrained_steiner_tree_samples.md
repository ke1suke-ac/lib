# constrained_steiner_tree サンプルコード集

対象: `constrained_steiner_tree_v10.hpp`。C++20以上・GCC系の環境を使用する。数学は高校数学までを想定し、C++の変数、関数、繰り返し、`std::vector` は知っているものとする。

本書は「問題の説明」「APIリファレンス」「用途別の実行例」の3部で構成する。実行例はそれぞれ独立したプログラムであり、別の例の関数をコピーする必要はない。ライブラリを呼ぶ行や設定を変更する行には、日本語のコメントを付けている。

## 1. 何を解くライブラリか？

### 1.1 必要な拠点を、共有部分を使いながら安くつなぐ

道路、配線、通信網を考える。場所を「頂点」、2つの場所を直接結べる候補を「辺」と呼ぶ。各辺には、その辺を採用したときに一度だけ支払う費用がある。必ずつなぐ場所を「端子」、APIでは `terminal` と呼ぶ。

端子ではない頂点を中継点として使ってよい。例えば3つの拠点を別々の高い道路で結ぶより、安い交差点を共有した方が、合計費用が小さくなる場合がある。このように、必要な拠点だけをつなぐ辺の集合を探す。

辺は**無向**であり、両方向に通れる。費用は0以上の整数で、同じ辺を複数の端子間で共有しても、その辺の費用は1回しか数えない。頂点を訪れる順番を決めたり、車両ごとの経路を割り当てたりする問題ではない。

### 1.2 通常の目的関数

採用する辺の集合を選び、その合計費用をできるだけ小さくする。

$$\min_ {F \subseteq E} \sum_ {e \in F} c_ {e}$$

|記号|意味|
|---|---|
|$V$|登録した頂点の集合。場所全体|
|$E$|登録した辺の集合。建設・利用候補全体|
|$T \subseteq V$|互いにつなぐ必要がある端子の集合|
|$F \subseteq E$|答えとして採用する辺の集合。これを選ぶ|
|$e$|1本の辺|
|$c_ {e}$|辺 $e$ の費用。0以上の整数|
|$\sum_ {e \in F}$|$F$ に含まれる辺について足し合わせる、という意味|
|$\min$|条件を満たす選択の中で、値を最小にしたいという意味|

ただし、$F$ の辺を使って、$T$ の全端子が互いに行き来できることが必要である。全頂点をつなぐ必要はない。端子が0個または1個なら、接続自体には辺が不要である。

### 1.3 同時に使えない頂点や、一時的な利用禁止

接続条件のほかに、次の条件を指定できる。

- **排他ペア:** 指定した2頂点を同時に使ってはいけない。
- **排他グループ:** 指定した頂点の集まりから、使えるのは高々1頂点。「0個でもよい」ので、必ず1つ選ぶ条件ではない。
- **禁止頂点・禁止辺:** 今回の問い合わせで使ってはいけない頂点・辺を指定する。
- **外側で選択済みの頂点:** 別の部分で使っている頂点との排他条件も守る。その頂点まで今回の辺を延ばす義務はない。

判定する使用頂点は、端子、答えの辺の両端、外側で選択済みの頂点である。中継点も含まれるため、「端子同士が衝突しなければよい」わけではない。同じ頂点が何度現れても、1頂点として数える。

通常の `solve` は上の問題を解く。`normalize` は利用できる候補辺を渡した集合の中に限定する。`improve` と `repair` は既存の辺集合を出発点として利用する。

### 1.4 既設の道路や配線を撤去せずに拡張する場合

`augment` では、既設辺を残し、追加する辺の費用だけを小さくする。

$$\min_ {A \subseteq E \setminus B} \sum_ {e \in A} c_ {e}$$

|記号|意味|
|---|---|
|$E$|登録した全辺の集合|
|$B \subseteq E$|撤去できない既設辺の集合。利用者が指定する|
|$A \subseteq E \setminus B$|今回追加する辺の集合。既設辺は含めない|
|$e$|1本の追加候補の辺|
|$c_ {e}$|その辺の登録費用|
|$E \setminus B$|全辺から既設辺を除いた集合|
|$\sum$ と $\min$|追加辺の費用を足し、その合計を最小にしたいという意味|

接続条件は、既設辺と追加辺を合わせた $B \cup A$ で全端子がつながること。既設辺の登録費用を書き換える必要はなく、その費用は今回の目的関数に入れない。

既設辺が複数の離れた成分に分かれていてもよい。すべての既設成分を接続する義務はない。ただし、**端子の接続に使わない既設辺も含め、全既設辺の両端は使用頂点として制約を受ける**。既設網に閉路があれば、拡張後の全体にも閉路が残り得る。

### 1.5 解の保証と対象外の条件

実行可能解を探すヒューリスティックライブラリであり、最適解の発見は保証しない。実行可能解が存在しても、探索範囲や時間によって見つからない場合がある。`ok()` が `true` の結果を利用し、失敗時の費用を成功した解の費用と取り違えない。

有向辺、負の費用、辺の容量、頂点の次数上限、辺同士の排他、頂点の設置費用、車両ごとの経路、グループから必ず1個選ぶ条件は直接の設定項目にはない。通常の全域木のように全頂点をつなぐ場合は、全頂点を端子に指定できるが、全頂点が固定されるため頂点同士の排他と両立しない場合がある。

## 2. APIリファレンス

### 2.1 使用環境・番号・オブジェクトの寿命

- クラス名は `constrained_steiner_tree`。以下の表と実行例では `using Solver = constrained_steiner_tree;` で `Solver` と略す。
- ヘッダーは `bits/stdc++.h` とC++20の `std::span` を使う。GCC系のC++20以上で利用する。
- 頂点数を `n`、登録辺数を `m` とすると、頂点番号は `0` 以上 `n` 未満、辺番号は `0` 以上 `m` 未満。両者は別の番号体系。
- 辺番号は `add_edge` の登録順。並行辺は別々の番号になる。自己ループと費用0の辺も登録できる。
- `std::vector<int>` を取る端子・辺番号リストは、その呼び出しの間だけ必要。重複は許されるが、重複した辺を何度も買うという意味ではない。
- `query_context` の `std::span` は配列を所有しない。参照するvector・array、`rules` の指すオブジェクトは呼び出し完了まで生存させる。span設定後に参照元vectorを再確保させた場合は、spanを設定し直す。
- `selection_rules` は渡されたグループを内部にコピーする。グループ登録後、その登録用vectorを保持する必要はない。
- 同じsolverへの同時呼び出しは行わない。`const` な探索関数も内部の作業領域を更新する。スレッドごとにsolverを分け、呼び出し中に参照先の制約を変更しない。
- solverはコピー不可。所有を移すmoveは可能だが、通常は同じオブジェクトを保持して繰り返し使う。`selection_rules`、設定、結果の各型は値としてコピーできる。

### 2.2 列挙型: `preset`

|値|意味と使い分け|
|---|---|
|`preset::fast`|探索を軽くする設定。多数の小さな問い合わせなどで使う。時間予算のない制約付き探索では、最初の合法候補を得ると終了する。有限の時間予算があれば追加候補も比較し得る|
|`preset::balanced`|計算量と解品質の中間設定。既定値|
|`preset::quality`|より多くの候補や改善を試す設定。時間に余裕があるときの選択肢|

これらは速度・品質の厳密な順位保証ではない。同じ入力でも `quality` が必ず `balanced` より安い答えを返す、とは限らない。列挙されていない数値をキャストして設定しない。

### 2.3 列挙型: `path_mode`

最短路は、2地点を最も安く結ぶ道のこと。ここでのmodeは、その計算結果の保存方法を選ぶ設定である。

|値|意味|
|---|---|
|`path_mode::auto_select`|端子数、メモリ上限、残り時間などから自動選択。入力設定の既定値|
|`path_mode::terminal_cache`|端子から各頂点への距離と経路情報の表を保存して使うことを要求する。同じグラフへの繰り返し問い合わせに利用できる|
|`path_mode::on_demand`|端子ごとの距離表を保持せず、必要な計算を行う。全作業領域を毎回捨てるという意味ではない|

`terminal_cache` を指定しても、表がメモリ上限を超える場合や、時間が尽きている場合は `on_demand` になる。制約付きの探索では、禁止条件が変わるため `on_demand` を使う。`augment` の既設辺を無料にする距離表は、通常グラフの永続cacheとは別に扱われる。実際に報告されたmodeは `answer.stats.path_mode_used` で確認する。

### 2.4 列挙型: `status`

|値|意味|判断してよいこと|
|---|---|---|
|`status::feasible`|実行可能解を保持している|費用と辺集合を答えとして使える。最適性までは意味しない|
|`status::proven_infeasible`|必須頂点同士の排他、禁止された既設辺など、固定条件から矛盾を証明できた|その入力条件のままでは解けない|
|`status::not_found`|この呼び出しで実行可能解を得られなかった|解の不存在までは分からない。接続不能、探索上限、期限など原因を区別する専用フィールドはない|
|`status::invalid_candidate`|`improve` に渡した初期辺が、現在の排他・禁止条件に違反する|初期解を見直すか、違反も修復する `repair` を使う|
|`status::invalid_input`|クエリの頂点・辺番号、rulesの頂点数、制約探索上限などが不正|入力や設定を修正する|

辺がないグラフの接続要求が `not_found` になるなど、すべての不可能な入力を `proven_infeasible` に分類するわけではない。`invalid_candidate` も「初期解に何か問題があれば必ずこの値」という意味ではなく、単に非連結の初期辺集合では `not_found` になる経路がある。

### 2.5 `options`: 探索設定の全項目

`Solver::options opts;` で次の既定値が入る。各メンバーは直接代入して変更する。

|メンバー|型|既定値|有効な値・意味|
|---|---|---|---|
|`preset_mode`|`preset`|`balanced`|上記3値から選ぶ|
|`path_mode_value`|`path_mode`|`auto_select`|上記3値から選ぶ。制約付き探索では指定にかかわらず `on_demand`|
|`seed`|`std::uint64_t`|`0`|0から64bit符号なし整数の最大値まで。乱数を用いる探索の種。値を変えても必ず別解になるわけではない|
|`time_limit_ms`|`int`|`0`|正なら1回の相対時間予算、単位はミリ秒。0以下は相対制限なし。通常は0か正数を使う|
|`restart_limit`|`int`|`-1`|0以上なら追加の初期候補生成の再スタート数上限。0でも初期候補生成や局所改善は行われる。負数は自動設定|
|`memory_limit_bytes`|`size_t`|`256ULL << 20`|268,435,456バイト、256MiB。距離表のデータ量上限。0なら正サイズの表を使わない。グラフや全作業領域のメモリ上限ではない|
|`deadline`|`std::chrono::steady_clock::time_point`|`time_point::max()`|この時計の絶対停止目標。既定値は締切なし。過去の時点なら期限切れとして扱う|
|`constraint_node_limit`|`int`|`-1`|`-1`は自動、0以上は制約付き探索で評価する状態数上限。0は新規の制約探索を行わない。`-2`以下は `invalid_input`|
|`constraint_depth_limit`|`int`|`-1`|`-1`は自動、0以上は制約付き探索の深さ上限。0でも最初の状態は評価し得る。`-2`以下は `invalid_input`|

有効範囲の検査がすべてのメンバーに同じ形で入るわけではない。特に `restart_limit` の負数は自動設定だが、制約上限の `-2`以下は不正である。

自動設定時の上限は次のとおり。「有限予算あり」は、正の `time_limit_ms` または有限の `deadline` が設定されている場合。

|設定|fast|balanced|quality|
|---|---:|---:|---:|
|再スタート上限、有限予算なし|0|4|20|
|再スタート上限、有限予算あり|0|時間予算内で継続|時間予算内で継続|
|制約状態数上限、有限予算なし|4|16|48|
|制約状態数上限、有限予算あり|16|32|48|
|制約深さ上限、有限予算なし|4|12|24|
|制約深さ上限、有限予算あり|8|12|24|

再スタート設定が主に使われるのは `solve` と `augment`。空の部分辺を渡した `repair` は `solve` に進むが、非空の `repair` や `improve` が再スタート数だけ一から解き直すわけではない。`seed` を変えれば全関数の結果が変わる、という意味でもない。

制約上限は、制約用の処理へ進む問い合わせに適用される。制約なしの通常 `solve` 全体を `constraint_node_limit = 0` で停止することはできない。`normalize` は `options` を受け取らない。

### 2.6 時間・メモリ設定の読み方

相対時間と絶対deadlineを両方指定したときは、早い方を停止目標にする。複数回の合計時間を抑える場合は、外側で一度決めた同じdeadlineを各回へ渡す。毎回 `now() + 100ms` と作り直すと、全体の締切が延びてしまう。

停止目標は、関数が必ずその時間内に返るというハードな保証ではない。入力確認、初回の作業領域構築、候補整理、途中で中断されない一まとまりの計算などに時間がかかる。短い予算でも初期候補を作る経路があり、期限切れならすべての関数が即時に `not_found` を返すわけではない。

一般的な64bit GCC環境では、端子数を $K$、頂点数を $N$ とすると、距離表のデータ量はおよそ $12 N K$ バイト。$N$ と $K$ は個数であり、12は距離8バイトと経路情報4バイトの合計である。コンテナ管理情報などは別に必要になる。メモリ上限を下げても、確保済み領域がすべて即時解放されるわけではない。通常のterminal cacheを明示的に解放するには `clear_terminal_cache()` を使う。

### 2.7 `selection_rules`: 排他条件

|公開関数|引数・既定値|戻り値・動作|
|---|---|---|
|`explicit selection_rules(int vertex_count = 0)`|0以上の頂点数。通常はsolverと同じ `n` を渡す|空の排他条件を持つオブジェクト。引数なしは0頂点用であり、任意サイズに合わせる設定ではない|
|`void add_conflict(int u, int v)`|有効な異なる2頂点。既定値なし|両頂点を同時に使えなくする。1つの排他グループとして登録|
|`void add_exclusive_group(std::span<const int> vertices)`|有効な頂点番号の配列。既定値なし|重複を除き、その中から高々1頂点まで使用可能にする。重複除去後に0個または1個なら登録しない|
|`void clear()`|引数なし|全排他条件を消す。頂点数は変えない|
|`int group_count() const`|引数なし|登録グループ数。`add_conflict` も数える。同じグループの再登録を自動で統合するわけではない|
|`int vertex_count() const`|引数なし|制約オブジェクトの対象頂点数|

複数グループは重なってよい。同じ頂点が複数グループに属すると、そのすべての条件を守る。`std::vector<int>` や `std::array<int, ...>` を `add_exclusive_group` に渡せる。短命な一時配列をcontextのspanに保持させる使い方とは違い、この関数は呼び出し中に内容をコピーする。

### 2.8 `query_context`: 呼び出しごとの条件

|メンバー|型|既定値|意味・有効範囲|
|---|---|---|---|
|`rules`|`const selection_rules*`|`nullptr`|排他条件へのポインタ。`nullptr` は排他条件なし。非nullならsolverと同じ頂点数が必要|
|`forbidden_vertices`|`std::span<const int>`|空|今回禁止する有効な頂点番号。端子、外側固定頂点、既設辺端点と重なると固定条件の矛盾になる|
|`forbidden_edges`|`std::span<const int>`|空|今回禁止する有効な辺番号。`augment` の既設辺と重なると矛盾になる|
|`selected_outside`|`std::span<const int>`|空|別の部分で選択済みの有効な頂点番号。排他・禁止条件には参加するが、接続義務を追加しない|

各配列の重複は許される。`selected_outside` は、その頂点の通過禁止でも、外側の辺を保存する命令でもない。外側の辺を無料で使いながら接続したい場合は、その辺を `augment` の `base_edges` として渡す。

### 2.9 結果型と辺情報

#### `result`: `solve`・`normalize`・`improve`・`repair` の戻り値

|メンバー・関数|型・既定値|意味|
|---|---|---|
|`state`|`status`、`not_found`|前述の5状態|
|`cost`|`long long`、`0`|成功時、返却辺の費用合計|
|`edges`|`std::vector<int>`、空|成功時、採用した辺番号。頂点番号や通過順ではない|
|`stats`|`statistics`、後述の初期値|その呼び出しの診断情報|
|`bool ok() const`|引数なし|`state == status::feasible` なら `true`。最適性の判定ではない|

#### `augmentation_result`: `augment` の戻り値

|メンバー・関数|型・既定値|意味|
|---|---|---|
|`state`|`status`、`not_found`|前述の5状態。現在の `augment` は `invalid_candidate` を返す経路を持たない|
|`added_cost`|`long long`、`0`|成功時、新しく追加する辺の費用合計。既設分は含めない|
|`added_edges`|`std::vector<int>`、空|成功時、新規追加辺だけの番号。既設辺は含めない|
|`stats`|`statistics`、後述の初期値|その呼び出しの診断情報|
|`bool ok() const`|引数なし|実行可能な追加接続を保持していれば `true`|

失敗時に費用が0でも、費用0の答えが見つかったとは限らない。失敗経路によって `inf()` 相当の値が入ることもある。まず `ok()` を確認する。

#### `edge_type`: `edge(id)` が参照で返す辺の情報

|メンバー|型|意味・値|
|---|---|---|
|`from`|`int`|登録時の一方の頂点。0以上 `n` 未満|
|`to`|`int`|登録時のもう一方の頂点。0以上 `n` 未満|
|`cost`|`long long`|登録時の非負費用|

無向なので `from` から `to` への片道という意味ではない。この型のメンバーにデフォルトの数値指定はない。利用者が `edge_type e;` とだけ宣言して未初期化の値を読まない。通常は `solver.edge(id)` の返す参照を読む。

### 2.10 `statistics`: 全11項目

|メンバー|型|既定値|読み方|
|---|---|---|---|
|`path_mode_used`|`path_mode`|`on_demand`|報告された距離計算モード。複数処理全体のモード別内訳ではない|
|`path_memory_bytes`|`size_t`|`0`|使ったterminal表のデータ量。総メモリ使用量ではない|
|`initial_solution_count`|`int`|`0`|初期候補生成処理で計上した回数。合法解数ではない|
|`restart_count`|`int`|`0`|追加再スタートを実行した回数|
|`local_move_count`|`int`|`0`|局所的な変更を試した回数として計上した値|
|`improvement_count`|`int`|`0`|費用を改善した局所変更として計上した値|
|`dijkstra_count`|`long long`|`0`|内部の最短路計算の呼び出し回数|
|`edge_relaxation_count`|`long long`|`0`|最短路計算で辺の向きを含めた接続を調べた回数。採用辺数ではない|
|`constraint_nodes`|`int`|`0`|制約付き探索で評価した状態数|
|`conflict_candidates`|`int`|`0`|排他違反が見つかった候補数|
|`feasible_candidates`|`int`|`0`|合法として計上された候補数。互いに異なる解の数とは限らない|

回数系の値は非負の診断カウンタで、制約探索をまたぐと加算される。`path_memory_bytes` は最大値を採用し、`path_mode_used` は報告されたmodeを保持する。正規化や早期終了などでは成功しても0のカウンタがある。統計を成功判定に使わない。

### 2.11 solverの構築・グラフ操作・cache

|公開関数|引数・既定値|戻り値・注意|
|---|---|---|
|`explicit constrained_steiner_tree(int n)`|0以上の頂点数。省略不可|辺がないsolverを構築する。後から頂点数は増減できない|
|`int add_edge(int u, int v, long long cost)`|有効な両端と非負費用。既定値なし|登録順の辺番号。逆向きも同時に使える。構築後・探索後にも追加可能|
|`const edge_type& edge(int edge_id) const`|有効な登録辺番号。既定値なし|辺情報への参照。後の `add_edge` で参照が無効になる場合があるので、長期保存には辺番号を使う|
|`int edge_count() const`|なし|登録辺数。返却解の辺数ではない|
|`int vertex_count() const`|なし|頂点数。返却解の使用頂点数ではない|
|`void clear_terminal_cache() const`|なし|通常グラフの端子距離表を解放する。グラフや全作業領域を消す関数ではない|
|`static constexpr long long inf()`|なし|内部で到達不能を表す値。`LLONG_MAX / 4`。一般的な64bit環境では `2305843009213693951`|

辺の削除、重み変更、頂点数変更を行う公開関数はない。必要ならsolverを作り直す。一時的に通したくない辺なら、`forbidden_edges` を使える。

### 2.12 接続を求める全関数と全オーバーロード

以下で `terminals` は `const std::vector<int>&` の頂点番号列。`candidate_edges`・`initial_edges`・`partial_edges`・`base_edges` はすべて `const std::vector<int>&` の辺番号列である。`opts` は `const options&`、`context` は `const query_context&`。各関数はsolverを参照する `const` メンバー関数。

表の `options{}` は2.5の既定値すべて、`query_context{}` は2.8の空の条件を表す。端子列・辺列は省略できないが、空のvectorは渡せる。

|関数の形|戻り値|動作|
|---|---|---|
|`solve(terminals, opts = options{}, context = query_context{})`|`result`|一から端子を接続する|
|`solve(terminals, context)`|`result`|optsだけを既定にして条件付きで接続する|
|`normalize(terminals, candidate_edges, context = query_context{})`|`result`|与えた候補辺の中だけで接続を整理する。候補外の辺は追加しない|
|`improve(terminals, initial_edges, opts = options{}, context = query_context{})`|`result`|合法な連結初期解を正規化して改善する|
|`improve(terminals, initial_edges, context)`|`result`|optsを既定にした改善|
|`repair(terminals, partial_edges, opts = options{}, context = query_context{})`|`result`|非連結・制約違反を含む部分辺集合を手掛かりに、辺の追加・削除を許して修復する|
|`repair(terminals, partial_edges, context)`|`result`|optsを既定にした修復|
|`augment(terminals, base_edges, opts = options{}, context = query_context{})`|`augmentation_result`|撤去不能な既設辺を追加費用0として利用し、追加辺を求める|
|`augment(terminals, base_edges, context)`|`augmentation_result`|optsを既定にした追加接続|
|`solve_cost(terminals, opts = options{}, context = query_context{})`|`long long`|`solve` が成功すれば費用だけを返し、失敗なら `inf()` を返す|
|`solve_cost(terminals, context)`|`long long`|optsを既定にして費用だけを得る|

`solve(terminals, {})` のような書き方は、空の設定と空のcontextのどちらかが曖昧になり得る。省略するなら `solve(terminals)`、設定を明示するなら `Solver::options{}`、条件を明示するなら `Solver::query_context{}` と型を分かるように書く。

**normalize:** 最適な部分集合を厳密に求めるAPIではない。候補内に合法な接続があっても発見保証はない。時間設定を受け取らず、制約探索に進む場合の内部上限は状態数32、深さ24。大量の候補を整理する時間も見込む。

**improve:** 合法な連結初期解なら、正規化した初期木より悪い費用の答えを採用せず、探索上限や期限切れでも既知の解を保持する。初期辺に禁止辺や排他違反が含まれる場合は、不要な枝として後で消せそうでも `invalid_candidate` になり得る。非連結を修復する目的なら `repair` を選ぶ。

**repair:** 渡した辺は固定ではない。必要なら取り除ける。空の部分辺も渡せる。不正な辺番号は修復対象ではなく `invalid_input`。

**augment:** `base_edges` は固定で、撤去しない。返却値は追加分だけなので、全体の辺集合が必要なら利用者側で既設辺と結合する。baseに重複があれば全体の費用計算時にも重複を除く。不要な既設成分の端点も排他・禁止の判定に入る。

**solve_cost:** 失敗理由や辺集合・統計は返らない。内部では通常のsolveを行うので、辺集合を一切構成しない高速な専用APIではない。実行可能な合計費用が `inf()` と同じになり得る設計では、数値だけで失敗を区別できないため `solve(...).ok()` を使う。

### 2.13 デバッグ用の全出力演算子

|右辺の型|関数の形|表示|
|---|---|---|
|`result`|`std::ostream& operator<<(std::ostream&, const result&)`|`result{state=..., cost=..., edges=[...]}`|
|`augmentation_result`|`std::ostream& operator<<(std::ostream&, const augmentation_result&)`|状態、追加費用、追加辺だけ|
|`statistics`|`std::ostream& operator<<(std::ostream&, const statistics&)`|全11項目を名前付きで表示|
|`edge_type`|`std::ostream& operator<<(std::ostream&, const edge_type&)`|両端と登録費用|
|`status`|`std::ostream& operator<<(std::ostream&, status)`|`feasible` などの列挙子名|
|`path_mode`|`std::ostream& operator<<(std::ostream&, path_mode)`|`on_demand` などの列挙子名|

左辺は `std::cout`、`std::cerr`、`std::ostringstream` などの出力先。引数の既定値はなく、戻り値は渡した出力先への参照なので、`<< '\n'` などを続けられる。自動改行はしない。`const` な結果や一時的な戻り値も表示できる。

辺配列は先頭20要素まで。残りがあれば `... (+5 more)` のように省略件数を示す。元の配列は変更しない。結果の表示にはstatsを含めず、必要なら `std::cerr << answer.stats << '\n';` で別途出力する。`preset`、`options`、`query_context`、`selection_rules` 全体の出力演算子は用意されていない。

### 2.14 入力検査と数値上の注意

構築時の頂点数、`add_edge` の両端・費用、排他条件の登録に渡す頂点番号は、呼び出す側が有効な値にする。前提違反は `assert` の対象であり、`NDEBUG` を指定したビルドで不正入力まで安全に扱えるという意味ではない。`edge(id)` も有効な辺番号を渡す。

探索クエリに渡す端子、各種辺リスト、禁止リスト、外側選択リスト、rulesの頂点数、制約上限の不正は、通常 `invalid_input` として返される。読み込みに失敗した入力をそのままライブラリへ流さず、利用側で入力形式と有効範囲を保証する。本書の入力例は、記載した形式・範囲を満たすものを想定する。

費用の型が `long long` でも、探索中のすべての値をその最大値まで使えるわけではない。新しく探す経路の費用は `inf()` 未満に収める。与えられた木を `normalize` で集計する場合は、合計が `LLONG_MAX` 以下なら保持でき、`improve` も正規化できた既知の初期解を保持する。合計が型の範囲を超えた候補は成功扱いにしない。大きな費用を使う場合ほど、`ok()` で判定することが重要になる。

## 3. ユースケース

### 実行方法と入力の約束

各例のC++コードを見出しにある `.cpp` 名で保存し、同じフォルダーへ `constrained_steiner_tree_v10.hpp` を置く。例えば最初の例は次のように実行できる。

```sh
g++ -std=c++20 -O2 -Wall -Wextra case01.cpp -o case01
./case01
```

標準入力を読む例では、起動後に併記した入力を貼り付けるか、入力ファイルをリダイレクトする。入力不要の例では、問題データをコード内にすべて記載する。どの例も独立した `main` と出力処理を含む。

サンプルの `Solver` はクラス名の省略であり、別のライブラリではない。各例のコメントには、ライブラリ呼び出しの引数、設定、結果の意味と、その例で注意する点を記載する。表示する費用と辺番号は、その入力に対する動作例であり、任意の入力での最適性や一意性を保証する意味ではない。

### 例の選び方

|例|用途|
|---|---|
|3.1 / `case01.cpp`|共有する中継点を使って複数拠点を結ぶ|
|3.2 / `case02.cpp`|同時に利用できない2つの中継地点を避ける|
|3.3 / `case03.cpp`|複数の排他グループを同時に守る|
|3.4 / `case04.cpp`|故障した頂点を一時的に通れなくする|
|3.5 / `case05.cpp`|並行辺の一方だけを工事で止める|
|3.6 / `case06.cpp`|別の部分ですでに使用中の頂点と矛盾しない接続を作る|
|3.7 / `case07.cpp`|手元の候補辺だけで合法な接続へ整理する|
|3.8 / `case08.cpp`|合法な既存ネットワークを安くする|
|3.9 / `case09.cpp`|途切れた部分ネットワークを修復する|
|3.10 / `case10.cpp`|条件変更で使えなくなった旧解を作り直す|
|3.11 / `case11.cpp`|撤去できない既設辺を使って新しい端子へ接続する|
|3.12 / `case12.cpp`|既設網の別成分と外側の選択を同時に考慮して拡張する|
|3.13 / `case13.cpp`|段階的に拠点を増やし、追加分を既設網へ取り込む|
|3.14 / `case14.cpp`|外側の辺を保存し、一部分だけを解き直す|
|3.15 / `case15.cpp`|同じグラフへの問い合わせでcacheを使い、メモリを解放する|
|3.16 / `case16.cpp`|同じグラフを保ち、運用条件の変更と辺の追加に対応する|
|3.17 / `case17.cpp`|辺集合を使わず、見積もり金額だけを受け取る|
|3.18 / `case18.cpp`|失敗理由を分類し、次の操作を決める|
|3.19 / `case19.cpp`|制約探索を制限しつつ、手元の合法な解を保持する|
|3.20 / `case20.cpp`|探索設定と乱数の種を変え、最良の成功結果を残す|
|3.21 / `case21.cpp`|複数の試行に共通の締切を設ける|
|3.22 / `case22.cpp`|端子が空・1個・重複している場合を扱う|
|3.23 / `case23.cpp`|大きな結果を短く表示し、必要に応じて統計も確認する|
|3.24 / `case24.cpp`|任意の中継点を選ぶ形から、全拠点を必須にする形へ変える|
|3.25 / `case25.cpp`|離れている複数の既設網をつなぎ、既設の閉路も残す|

### APIから例を探す

|API・設定|対応する例|
|---|---|
|`solve`、全頂点を端子にする形|3.1、3.2、3.20、3.21、3.24|
|`normalize`|3.7、3.14、3.21、3.23|
|`improve`|3.8、3.10、3.19|
|`repair`|3.9、3.10、3.14|
|`augment`|3.11〜3.13、3.22、3.25|
|`solve_cost`、`inf`|3.17|
|`selection_rules`、`add_conflict`、`add_exclusive_group`|3.2、3.3、3.6、3.16|
|rulesの`clear`、`vertex_count`、`group_count`|3.3、3.16|
|`query_context`の4メンバー|3.2、3.4〜3.6、3.12、3.14|
|`preset_mode`、`seed`、`restart_limit`|3.8、3.20|
|`path_mode_value`、`memory_limit_bytes`、`clear_terminal_cache`|3.8、3.15、3.20|
|`constraint_node_limit`、`constraint_depth_limit`|3.19|
|`time_limit_ms`、`deadline`|3.21|
|構築、`add_edge`、`edge`、`vertex_count`、`edge_count`|3.1、3.16、3.23、3.24|
|結果の`ok`、各フィールド、出力演算子|3.1、3.11、3.13、3.20、3.23|
|全`status`値、入力検査|3.18|
|`statistics`とその出力演算子|3.15、3.23|


### 3.1. 共有する中継点を使って複数拠点を結ぶ

3つの拠点へ配線を届ける。配線を共通の交差点までまとめられれば、別々に直接結ぶより安くなる。

**具体的な問題:** 4頂点のうち0・2・3を接続する。頂点1は任意の中継点。直接辺の費用7に対し、頂点1を通る各辺は費用2なので、3本を共有網として選ぶと合計6になる。

**入力形式:** 最初の行は `N M K`、続くM行は辺の両端と費用、最後はK個の端子番号。出力の末尾は「辺番号 from to cost」。

**コード: `case01.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    const auto answer = solver.solve(terminals); // 端子の頂点番号を渡す。設定・制約は既定値。

    // 実行可能解があるかを先に確認。費用0だけでは成功と判定しない。
    if (!answer.ok()) {
        std::cout << answer.state << '\n'; // 失敗状態を列挙子名で表示する。
        return 0;
    }

    std::cout << "vertices=" << solver.vertex_count() << '\n'; // グラフ全体の頂点数。

    // 選択辺数ではなく登録辺数。
    std::cout << "registered_edges=" << solver.edge_count() << '\n';
    std::cout << "cost=" << answer.cost << '\n'; // 成功した解の辺費用の合計。

    // 返却配列は辺番号の集合であり、道の通過順ではない。
    for (int id : answer.edges) {
        // 登録辺の情報を参照。辺追加後までこの参照を保持しない。
        const auto &edge = solver.edge(id);

        // 番号・両端・費用を出す。
        std::cout << id << ' ' << edge.from << ' ' << edge.to << ' ' << edge.cost
            << '\n';
    }
}
```

**入力**

```text
4 6 3
0 1 2
1 2 2
1 3 2
0 2 7
0 3 7
2 3 7
0 2 3
```

**出力**

```text
vertices=4
registered_edges=6
cost=6
0 0 1 2
1 1 2 2
2 1 3 2
```


### 3.2. 同時に利用できない2つの中継地点を避ける

安い経路が、同時使用を禁止された2つの拠点を通ってしまう。もう少し高くても条件に合う接続を求める。

**具体的な問題:** 端子0・3を接続する。安い0–1–2–3は費用3だが、頂点1と2を同時使用できない。0–4–3なら費用6で合法。

**入力形式:** `N M K`、M本の辺、K個の端子に続き、排他ペア数PとP行の頂点ペアを与える。

**コード: `case02.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int p;
    std::cin >> p; // 追加入力: 排他ペア数、続いて各ペアの頂点番号。

    Solver::selection_rules rules(n); // solverと同じ頂点数の排他条件を作る。
    for (int i = 0; i < p; ++i) {
        int u, v;
        std::cin >> u >> v;

        rules.add_conflict(u, v); // 有効な異なる2頂点を同時使用不可にする。
    }

    Solver::query_context context; // 今回の問い合わせ条件。各配列は空で始まる。
    context.rules = &rules; // rulesを参照させる。solveが終わるまでrulesを生存させる。

    const auto answer = solver.solve(terminals, context); // optionsを省略できる条件付きの形。

    std::cout << answer << '\n'; // 状態・費用・採用辺を直接表示する。失敗時の費用は解ではない。
}
```

**入力**

```text
5 5 2
0 1 1
1 2 1
2 3 1
0 4 3
4 3 3
0 3
1
1 2
```

**出力**

```text
result{state=feasible, cost=6, edges=[3, 4]}
```


### 3.3. 複数の排他グループを同時に守る

同じ設備区分から使える場所は最大1つ。ある場所が複数区分に属していても、すべての条件を守りたい。

**具体的な問題:** 端子0・5を接続する。グループ{1,2,4}から高々1頂点、さらに{2,3}から高々1頂点を使える。頂点2は両グループに属する。0–3–5を選ぶと合計4で両条件を満たす。

**入力形式:** グラフ・端子の後にグループ数Gを与える。各グループの行は「頂点数 頂点番号...」。

**コード: `case03.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int g;
    std::cin >> g; // 追加入力: グループ数。各グループは「個数 頂点番号...」。

    Solver::selection_rules rules(n); // 排他グループを登録する対象頂点数はn。
    for (int i = 0; i < g; ++i) {
        int count;
        std::cin >> count;
        std::vector<int> group(count);
        for (int &v : group) {
            std::cin >> v;
        }

        rules.add_exclusive_group(group); // この呼び出しでコピーする。0個選択も許される。
    }

    Solver::query_context context; // 一時的な条件を作る。
    context.rules = &rules; // グループが重なっても、すべての排他条件を適用する。

    const auto answer = solver.solve(terminals, context); // 端子だけでなく中継頂点も制約対象。

    std::cout << "groups=" << rules.group_count() << '\n'; // 登録グループ数を確認する。
    std::cout << answer << '\n'; // 成否と解を表示。必ず1個選ぶ要求はこのAPIでは指定できない。
}
```

**入力**

```text
6 5 2
0 1 1
1 2 1
2 5 1
0 3 2
3 5 2
0 5
2
3 1 2 4
2 2 3
```

**出力**

```text
groups=2
result{state=feasible, cost=4, edges=[3, 4]}
```


### 3.4. 故障した頂点を一時的に通れなくする

故障した交差点や使えない敷地を通さずに接続する。グラフそのものを作り直さず、問い合わせごとの禁止にする。

**具体的な問題:** 0と3を接続する。費用2の経路は頂点1を通るが、今回は頂点1を禁止する。頂点2経由の費用6の接続を求める。

**入力形式:** グラフ・端子の後に「禁止頂点数 禁止頂点番号...」。

**コード: `case04.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力: 禁止頂点数と、その頂点番号。
    std::vector<int> blocked(count);
    for (int &v : blocked) {
        std::cin >> v;
    }

    Solver::query_context context; // 排他ルールを使わない問い合わせ条件。

    // spanはblockedを参照するため、呼び出し中は再確保・破棄しない。
    context.forbidden_vertices = blocked;

    const auto answer = solver.solve(terminals, context); // 禁止頂点を使わない接続を求める。

    std::cout << answer << '\n'; // 端子自体を禁止した場合はproven_infeasibleになり得る。
}
```

**入力**

```text
4 4 2
0 1 1
1 3 1
0 2 3
2 3 3
0 3
1 1
```

**出力**

```text
result{state=feasible, cost=6, edges=[2, 3]}
```


### 3.5. 並行辺の一方だけを工事で止める

同じ2地点を結ぶ複数の回線のうち、特定の1本だけが使えない。両端の頂点まで禁止する必要はない。

**具体的な問題:** 0と1の間に費用1の辺0と費用4の辺1がある。辺0だけを禁止して0と2を接続する。辺1と辺2を使い、費用は5になる。

**入力形式:** グラフ・端子の後に「禁止辺数 禁止辺番号...」。辺番号は入力順の0始まり。

**コード: `case05.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力: 禁止辺数と、入力順に付いた辺番号。
    std::vector<int> blocked(count);
    for (int &id : blocked) {
        std::cin >> id;
    }

    Solver::query_context context; // 頂点は禁じず、辺だけを制限する。

    // 頂点番号ではなく辺番号を渡す。blockedの寿命を呼び出しまで保つ。
    context.forbidden_edges = blocked;

    const auto answer = solver.solve(terminals, context); // 禁止された並行辺以外は利用できる。

    std::cout << answer << '\n'; // グラフからの恒久的な辺削除ではなく、この呼び出しだけの条件。
}
```

**入力**

```text
3 4 2
0 1 1
0 1 4
1 2 1
0 2 10
0 2
1 0
```

**出力**

```text
result{state=feasible, cost=5, edges=[1, 2]}
```


### 3.6. 別の部分ですでに使用中の頂点と矛盾しない接続を作る

別のネットワークで利用中の設備がある。その設備と排他的な場所は今回使えないが、その設備まで新しい網を延ばす必要はない。

**具体的な問題:** 0と3を接続する。頂点4は別の部分で使用中で、頂点1と排他。頂点4はこのグラフでは孤立しているが接続義務はない。頂点2経由で費用6となる。

**入力形式:** グラフ・端子の後に「外側で使用中の頂点 その排他相手」を与える。

**コード: `case06.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int outside_vertex, incompatible_vertex;

    // 追加入力: 外側の使用頂点と、その排他相手。
    std::cin >> outside_vertex >> incompatible_vertex;

    Solver::selection_rules rules(n); // 頂点数をグラフに合わせる。

    // 外側で使う頂点も排他の対象にする。
    rules.add_conflict(outside_vertex, incompatible_vertex);

    const std::vector<int> outside{outside_vertex};

    Solver::query_context context; // この問い合わせでの外側の状態を渡す。
    context.rules = &rules; // 排他ルール本体への参照。
    context.selected_outside = outside; // 接続端子には追加しない。outsideを呼び出し中保持する。

    const auto answer = solver.solve(terminals, context); // 外側と矛盾しない辺集合を求める。

    std::cout << answer << '\n'; // 外側の辺の保存・結合は利用者側で行う。
}
```

**入力**

```text
5 4 2
0 1 1
1 3 1
0 2 3
2 3 3
0 3
4 1
```

**出力**

```text
result{state=feasible, cost=6, edges=[2, 3]}
```


### 3.7. 手元の候補辺だけで合法な接続へ整理する

別の処理で集めた候補辺に、重複や不要な枝がある。候補外の辺を増やさず、その範囲で条件に合う接続を作りたい。

**具体的な問題:** 0と3の接続候補に、安いが排他違反の0–1–2–3、合法な0–4–3、不要な3–5を含める。頂点1・2は同時使用不可。辺5は重複して渡すが、重複分の費用は数えない。

**入力形式:** グラフ・端子の後に「候補辺数 候補辺番号...」、その後に排他ペアを与える。

**コード: `case07.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力: 候補辺数、候補辺番号、最後に排他ペアu v。
    std::vector<int> candidates(count);
    for (int &id : candidates) {
        std::cin >> id;
    }

    int u, v;
    std::cin >> u >> v;

    Solver::selection_rules rules(n); // 正規化でも排他条件を指定できる。
    rules.add_conflict(u, v); // 候補の一部に違反があっても、合法な部分を探す。

    Solver::query_context context; // 参照先のrulesは呼び出し中に保持する。
    context.rules = &rules; // 今回守る排他条件。

    // 候補外の辺は追加しない。options引数はない。
    const auto answer = solver.normalize(terminals, candidates, context);

    // 厳密最適化や実行可能解の発見保証はない。失敗時はstateを確認する。
    std::cout << answer << '\n';
}
```

**入力**

```text
6 6 2
0 1 1
1 2 1
2 3 1
0 4 3
4 3 3
3 5 8
0 3
7 0 1 2 3 4 5 5
1 2
```

**出力**

```text
result{state=feasible, cost=6, edges=[3, 4]}
```


### 3.8. 合法な既存ネットワークを安くする

すでにつながっている配線を、より安い経路へ置き換えたい。時間切れになっても、使えていた接続を失いたくない。

**具体的な問題:** 初期辺{0,1}は0–1–3を結び、費用8。0–2–3へ置き換えると費用3になる。初期辺は撤去・交換してよい。

**入力形式:** グラフ・端子の後に「初期辺数 初期辺番号...」。初期辺は端子を連結し、現在の制約を満たすものとする。

**コード: `case08.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力: 合法で連結な初期解の辺数と辺番号。
    std::vector<int> initial(count);
    for (int &id : initial) {
        std::cin >> id;
    }

    Solver::options opts; // 省略しない項目以外は既定値を使う。

    // より多くの改善を試す。必ず他のpresetより良い保証ではない。
    opts.preset_mode = Solver::preset::quality;
    opts.path_mode_value = Solver::path_mode::on_demand; // 端子距離表を保持しない計算モード。

    // 合法な連結初期解を保ちつつ改善する。
    const auto answer = solver.improve(terminals, initial, opts);

    std::cout << answer << '\n'; // 初期辺に制約違反や切断がある用途ではrepairを選ぶ。
}
```

**入力**

```text
4 4 2
0 1 4
1 3 4
0 2 1
2 3 2
0 3
2 0 1
```

**出力**

```text
result{state=feasible, cost=3, edges=[2, 3]}
```


### 3.9. 途切れた部分ネットワークを修復する

両側に配線が残っているが、途中が欠けている。残った辺を手掛かりとして、必要な接続を補いたい。

**具体的な問題:** 0–1–2–3の一直線。辺0と辺2だけを部分解として渡す。辺1を追加すれば0と3がつながり、合計費用3になる。

**入力形式:** グラフ・端子の後に「部分辺数 部分辺番号...」。番号は有効な登録辺を指す。

**コード: `case09.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力: 部分辺数と、その辺番号。非連結でもよい。
    std::vector<int> partial(count);
    for (int &id : partial) {
        std::cin >> id;
    }

    // 部分辺はヒント。必要なら追加だけでなく削除も行える。
    const auto answer = solver.repair(terminals, partial);

    // 残存辺を絶対に撤去できないなら、repairではなくaugmentを使う。
    std::cout << answer << '\n';
}
```

**入力**

```text
4 3 2
0 1 1
1 2 1
2 3 1
0 3
2 0 2
```

**出力**

```text
result{state=feasible, cost=3, edges=[0, 1, 2]}
```


### 3.10. 条件変更で使えなくなった旧解を作り直す

昨日までは合法だった接続が、新しい同時使用制限に違反した。旧解を固定せず、必要な部分を置き換える。

**具体的な問題:** 旧解の0–1–2–3は費用3。新しく頂点1と2を排他にする。improveは初期解違反を報告し、repairでは0–4–3への修復を試せる。

**入力形式:** グラフ・端子の後に「旧解の辺数 辺番号...」、最後に新しい排他ペアを与える。

**コード: `case10.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力: 旧解の辺数と辺番号、続いて新しい排他ペア。
    std::vector<int> old_edges(count);
    for (int &id : old_edges) {
        std::cin >> id;
    }

    int u, v;
    std::cin >> u >> v;

    Solver::selection_rules rules(n); // 新しい制約集合を準備する。

    // 旧解がこの新条件に違反していても、辺番号自体は有効でなければならない。
    rules.add_conflict(u, v);

    Solver::query_context context; // この問い合わせの制約をまとめる。
    context.rules = &rules; // 参照先を呼び出し中に変更・破棄しない。

    // 合法な初期解を要求するAPIで確認する。
    const auto rejected = solver.improve(terminals, old_edges, context);

    std::cout << "improve=" << rejected.state << '\n'; // この入力ではinvalid_candidate。

    Solver::options opts; // 修復の設定。時間を指定しなければ探索回数上限まで処理する。
    opts.preset_mode = Solver::preset::fast; // 小さな修復を軽く試す設定。

    // 違反辺の撤去を許して修復する。
    const auto repaired = solver.repair(terminals, old_edges, opts, context);

    std::cout << repaired << '\n'; // 修復できない場合もあるので、利用時はok()を確認する。
}
```

**入力**

```text
5 5 2
0 1 1
1 2 1
2 3 1
0 4 3
4 3 3
0 3
3 0 1 2
1 2
```

**出力**

```text
improve=invalid_candidate
result{state=feasible, cost=6, edges=[3, 4]}
```


### 3.11. 撤去できない既設辺を使って新しい端子へ接続する

建設済みの道路はそのまま利用できる。既設部分の建設費を再び払うのではなく、今回追加する分だけを安くする。

**具体的な問題:** 費用100の辺0–1は建設済み。端子0・3をつなぐため、1–3の辺を費用5で追加する。返却される追加費用は105ではなく5。

**入力形式:** グラフ・端子の後に「既設辺数 既設辺番号...」。既設辺は実際のグラフに登録しておく。

**コード: `case11.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力: 既設辺数と、その辺番号。
    std::vector<int> base(count);
    for (int &id : base) {
        std::cin >> id;
    }

    Solver::query_context context; // 今回は空だが、同じ形で排他・禁止条件も渡せる。

    // 既設辺は撤去不可・追加費用0として扱う。
    const auto answer = solver.augment(terminals, base, context);

    std::cout << answer << '\n'; // added_costとadded_edgesは新規分だけ。

    // 失敗時の追加費用や辺集合をネットワークとして利用しない。
    if (!answer.ok()) {
        return 0;
    }

    auto network = base;

    // 成功した追加辺を既設辺へ結合。
    network.insert(network.end(), answer.added_edges.begin(), answer.added_edges.end());
    std::sort(network.begin(), network.end());
    network.erase(std::unique(network.begin(), network.end()), network.end());

    // 全体が必要なら利用側で結合・重複除去する。
    std::cout << "network_edges=" << network.size() << '\n';
}
```

**入力**

```text
4 4 2
0 1 100
1 3 5
0 2 20
2 3 20
0 3
1 0
```

**出力**

```text
augmentation_result{state=feasible, added_cost=5, added_edges=[1]}
network_edges=2
```


### 3.12. 既設網の別成分と外側の選択を同時に考慮して拡張する

今回の端子に関係しない既設設備も撤去できず、さらに別の担当領域で使用中の設備がある。両方と衝突しない追加接続が必要になる。

**具体的な問題:** 端子0・3を接続する。既設辺4–5は離れた成分だが、端点4と頂点1が排他。外側で使用中の頂点6と頂点2も排他。そのため頂点1経由も2経由も使えず、頂点7経由の追加費用8を選ぶ。

**入力形式:** グラフ・端子の後に既設辺リスト、外側頂点リスト、排他ペアリスト。各リストの前に個数を付ける。ここでは辺番号6と頂点番号6が別々の意味で登場する。

**コード: `case12.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int count;
    std::cin >> count; // 追加入力1: 既設辺数と辺番号。
    std::vector<int> base(count);
    for (int &id : base) {
        std::cin >> id;
    }

    std::cin >> count; // 追加入力2: 外側の使用頂点数と頂点番号。
    std::vector<int> outside(count);
    for (int &v : outside) {
        std::cin >> v;
    }

    int p;
    std::cin >> p; // 追加入力3: 排他ペア数と各ペア。

    Solver::selection_rules rules(n); // 既設端点・外側選択・今回使う頂点の共通ルール。
    for (int i = 0; i < p; ++i) {
        int u, v;
        std::cin >> u >> v;

        rules.add_conflict(u, v); // 端子でない頂点も排他対象にできる。
    }

    Solver::query_context context; // 条件をまとめる。
    context.rules = &rules; // rulesは呼び出し完了まで保持する。
    context.selected_outside = outside; // augmentと同時に使える。接続義務は追加しない。

    Solver::options opts; // 今回は既定値のまま、設定とcontextの両方を明示する。

    // 使わない既設成分の端点も選択済みになる。
    const auto answer = solver.augment(terminals, base, opts, context);

    // baseとforbidden_edgesを重ねると、撤去不能なのに使用禁止という矛盾になる。
    std::cout << answer << '\n';
}
```

**入力**

```text
8 7 2
0 1 1
1 3 1
0 2 2
2 3 2
0 7 4
7 3 4
4 5 100
0 3
1 6
1 6
2
4 1
6 2
```

**出力**

```text
augmentation_result{state=feasible, added_cost=8, added_edges=[4, 5]}
```


### 3.13. 段階的に拠点を増やし、追加分を既設網へ取り込む

最初に小さなネットワークを建設し、後から利用拠点を追加する。追加済みの辺を次の段階で既設扱いにすれば、二重に費用を計上しない。

**具体的な問題:** 最初は端子0・2を費用4で接続する。次に端子4を追加し、辺1–3と3–4を追加費用4で建設する。同じ端子を再び要求したときの追加費用は0になる。

**入力形式:** グラフと初期端子集合の後に、後から追加する端子番号を1つ与える。

**コード: `case13.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int new_terminal;
    std::cin >> new_terminal; // 追加入力: 後から必要になる端子番号。

    const auto first = solver.solve(terminals); // 初回は一から接続する。

    // 初回の成功を確認してから既設網として採用する。
    if (!first.ok()) {
        std::cout << first.state << '\n'; // 失敗時は状態を表示して終了。
        return 0;
    }

    auto base = first.edges; // 初回に建設した辺を、次の段階の既設辺として保持。
    terminals.push_back(new_terminal);

    // 増えた端子集合を、既設辺を残して接続する。
    const auto added = solver.augment(terminals, base);

    // 追加接続が成功した場合だけbaseを更新する。
    if (!added.ok()) {
        std::cout << added.state << '\n'; // 既設網を壊さず、追加失敗を表示する。
        return 0;
    }

    // added_edgesは既設辺と重複しない新規分。
    base.insert(base.end(), added.added_edges.begin(), added.added_edges.end());

    // 初回費用と追加費用を区別する。
    std::cout << "first=" << first.cost << " added=" << added.added_cost << '\n';

    // この例では同じ辺を重複せず建設している。
    std::cout << "paid=" << first.cost + added.added_cost << '\n';

    // 更新後のbaseを渡す。すでにつながっていれば追加0。
    const auto again = solver.augment(terminals, base);

    std::cout << again << '\n'; // 空のadded_edgesでも成功した拡張結果になり得る。
}
```

**入力**

```text
5 5 2
0 1 2
1 2 2
1 3 3
3 4 1
0 4 20
0 2
4
```

**出力**

```text
first=4 added=4
paid=8
augmentation_result{state=feasible, added_cost=0, added_edges=[]}
```


### 3.14. 外側の辺を保存し、一部分だけを解き直す

大きな網を全部作り直さず、一部の高い区間だけを改善したい。外側を別途保存し、境界までの局所接続を解き直す。

**具体的な問題:** 全体では0と4を接続する。外側の2–3–4を固定し、内側の0–1–2だけを交換する。外側の頂点4と頂点6が排他なので、内側の費用0経路は使えない。0–5–2を選び、全体費用を12から6にする。

**入力形式:** 標準入力なし。グラフ、固定する外側、旧内側、全体と局所の端子をコード内に記載している。

**コード: `case14.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    Solver solver(7); // 全体の頂点番号を保ったまま使う。局所用に番号を振り直していない。
    solver.add_edge(0, 1, 4); // 辺0: 旧内側。撤去可能。
    solver.add_edge(1, 2, 4); // 辺1: 旧内側。撤去可能。

    solver.add_edge(2, 3, 2); // 辺2: 外側として保存する。
    solver.add_edge(3, 4, 2); // 辺3: 外側として保存する。

    solver.add_edge(0, 5, 1); // 辺4: 新しい内側の候補。
    solver.add_edge(5, 2, 1); // 辺5: 新しい内側の候補。

    solver.add_edge(0, 6, 0); // 辺6: 安いが外側の頂点と衝突する。
    solver.add_edge(6, 2, 0); // 辺7: 同じく衝突する候補。

    Solver::selection_rules rules(7); // 全体と同じ頂点数の制約。
    rules.add_conflict(4, 6); // 外側で使う4と、内側候補の6を同時使用不可にする。

    const std::vector<int> outside_edges{2, 3};
    const std::vector<int> outside_vertices{2, 3, 4};

    Solver::query_context local; // 局所の問い合わせだけに使う条件。
    local.rules = &rules; // 全体の排他条件を適用する。
    local.selected_outside = outside_vertices; // 外側の全使用頂点を選択済みとして伝える。

    // 局所解への重複取り込みを防ぐ。全体から外側を削除する指示ではない。
    local.forbidden_edges = outside_edges;

    // 内側の端子は0と境界2、辺0・1は撤去できる旧内側。
    const auto part = solver.repair({0, 2}, {0, 1}, local);

    // 失敗した局所解で元の網を置き換えない。
    if (!part.ok()) {
        std::cout << part.state << '\n'; // 状態だけを表示して終了。
        return 0;
    }

    auto network = outside_edges;

    // 外側は利用側で保存し、成功した局所解と結合する。
    network.insert(network.end(), part.edges.begin(), part.edges.end());

    Solver::query_context global; // 局所用の禁止を全体検証へ持ち込まない。
    global.rules = &rules; // 全体では元の排他条件を使う。

    // 全体の端子0・4について、結合した候補の合法性を確認。
    const auto whole = solver.normalize({0, 4}, network, global);

    std::cout << "local=" << part.cost << '\n'; // 局所分の費用。外側の費用は含まない。
    std::cout << whole << '\n'; // この入力では結合した全辺が必要で、全体費用6になる。

    // 外側が複数成分なら、必要な成分同士をつなぐ境界端子を漏れなく指定する。
    // selected_outsideだけでは外側の辺の保存・編集禁止は行われない。
    // 外側の辺も無料で通ってよいなら、この禁止方式ではなくaugmentのbaseとして渡す。
}
```

**出力**

```text
local=2
result{state=feasible, cost=6, edges=[2, 3, 4, 5]}
```

**補足:** 外側が複数成分なら、必要な成分が全体としてつながる境界端子の指定が必要。selected_outside自体はグラフを小さく切り出す機能ではない。


### 3.15. 同じグラフへの問い合わせでcacheを使い、メモリを解放する

同じ道路網について端子集合を少しずつ変えて何度も解く。距離表を使う設定を指定し、不要になったら明示的に解放する。

**具体的な問題:** 0から7まで一直線のグラフ。最初は端子0・7、次に3も追加する。どちらも費用7。3端子の表は一般的な64bit GCC環境で288バイトとなる。

**入力形式:** グラフ・初期端子の後に、追加する端子番号を1つ与える。表示バイト数はlong longが8バイト、intが4バイトの環境の例。

**コード: `case15.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    int extra;
    std::cin >> extra; // 追加入力: 2回目から端子に加える頂点。

    Solver::options opts; // メモリに関する設定もこの型で指定する。

    // 端子からの距離表の利用を要求する。
    opts.path_mode_value = Solver::path_mode::terminal_cache;

    // 表のデータ部分に64KiBを許す。総メモリ上限ではない。
    opts.memory_limit_bytes = 64ULL * 1024;
    opts.restart_limit = 0; // 追加の再スタートを止める。初期候補生成や局所改善は残る。

    // 同じsolver内に通常グラフ用の表を保持できる。
    const auto first = solver.solve(terminals, opts);

    terminals.push_back(extra);

    const auto second = solver.solve(terminals, opts); // 共通する端子の表を再利用できる。

    // 統計値が0かどうかではなく、ok()で成功確認する。
    if (!first.ok() || !second.ok()) {
        return 1;
    }

    // 実際に報告されたmodeを名前で表示。
    std::cout << "first_mode=" << first.stats.path_mode_used << '\n';
    std::cout << "second_cost=" << second.cost << '\n'; // 2回目の接続費用。

    // 表のデータ量。long longが8B、intが4Bならこの例は288B。総メモリではない。
    std::cout << "cache_bytes=" << second.stats.path_memory_bytes << '\n';

    solver.clear_terminal_cache(); // 端子距離表を明示的に解放。グラフは残す。
    opts.memory_limit_bytes = 0; // 正サイズの距離表を使わない設定にする。

    // terminal_cache指定でも上限に収まらなければon_demandになる。
    const auto third = solver.solve(terminals, opts);

    // 表を使わなくても、成功した解は通常どおり利用できる。
    if (!third.ok()) {
        return 1;
    }

    // 切り替わった実際のmodeを確認。
    std::cout << "third_mode=" << third.stats.path_mode_used << '\n';
    std::cout << "third_cost=" << third.cost << '\n'; // 同じ端子集合の費用を表示する。
}
```

**入力**

```text
8 7 2
0 1 1
1 2 1
2 3 1
3 4 1
4 5 1
5 6 1
6 7 1
0 7
3
```

**出力**

```text
first_mode=terminal_cache
second_cost=7
cache_bytes=288
third_mode=on_demand
third_cost=7
```


### 3.16. 同じグラフを保ち、運用条件の変更と辺の追加に対応する

設備の同時使用ルールが変わったり、新しい道路が開通したりしても、登録済みのグラフを続けて使いたい。

**具体的な問題:** 端子0・3を接続する。頂点1・2が排他なら費用6、制約を解除すれば費用3。制約を再登録した後でも、直接辺を費用1で追加すればその辺を使える。

**入力形式:** 標準入力なし。条件変更の前後と、新規道路の追加をコード内に記載している。

**コード: `case16.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    Solver solver(5); // 頂点数は後から変えられないので、全5頂点を先に用意する。
    solver.add_edge(0, 1, 1); // 辺0: 安い経路の一部。
    solver.add_edge(1, 2, 1); // 辺1: 安い経路の一部。
    solver.add_edge(2, 3, 1); // 辺2: 安い経路の一部。
    solver.add_edge(0, 4, 3); // 辺3: 排他条件を避ける経路。
    solver.add_edge(4, 3, 3); // 辺4: 排他条件を避ける経路。

    const std::vector<int> terminals{0, 3};

    Solver::selection_rules rules(5); // 対象頂点数はsolverとそろえる。
    rules.add_conflict(1, 2); // 初期状態では1と2を同時に使えない。

    Solver::query_context context; // この変数からrulesを参照し続ける。
    context.rules = &rules; // rulesの寿命はすべての呼び出しを覆う。

    const auto restricted = solver.solve(terminals, context); // 最初のルールで解く。

    std::cout << restricted << '\n'; // 費用6の合法な結果。

    rules.clear(); // 登録済みの排他条件だけを消す。対象頂点数は5のまま。

    // 同じポインタでも、現在の空のルールが使われる。
    const auto unrestricted = solver.solve(terminals, context);

    std::cout << unrestricted << '\n'; // 費用3の結果。

    const std::vector<int> group{1, 2, 2};
    rules.add_exclusive_group(group); // 重複した2は1頂点として扱い、{1,2}の条件を登録する。

    const std::vector<int> singleton{1};
    rules.add_exclusive_group(singleton); // 1頂点だけなら制限にならず、グループ数も増えない。

    // 対象数と登録数を区別。
    std::cout << "rule_vertices=" << rules.vertex_count() << " groups="
        << rules.group_count() << '\n';

    // 探索後の追加も可能。端子距離表は必要に応じて無効化される。
    const int direct = solver.add_edge(0, 3, 1);

    std::cout << "new_edge=" << direct << '\n'; // 追加辺の番号は5。

    // 再登録した排他条件と、新しい辺の両方を反映。
    const auto extended = solver.solve(terminals, context);

    std::cout << extended << '\n'; // 費用1の結果。

    // 呼び出しの実行中に、別スレッドからグラフやrulesを変更してはいけない。
}
```

**出力**

```text
result{state=feasible, cost=6, edges=[3, 4]}
result{state=feasible, cost=3, edges=[0, 1, 2]}
rule_vertices=5 groups=1
new_edge=5
result{state=feasible, cost=1, edges=[5]}
```


### 3.17. 辺集合を使わず、見積もり金額だけを受け取る

接続案を表示する画面ではなく、費用の見積もりだけを数値として扱いたい。ただし、失敗を通常の金額と混同してはいけない。

**具体的な問題:** 0–1–2を費用1と2で接続するので、端子0・2の費用は3。唯一の中継点1を禁止すると、接続を発見できずinf()が返る。

**入力形式:** グラフ・端子の後に、2回目の問い合わせで禁止する頂点を1つ与える。

**コード: `case17.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    Solver::options opts; // 費用だけを受け取る場合も、通常と同じ設定を使える。
    opts.preset_mode = Solver::preset::fast; // 探索の軽い設定を指定する。

    Solver::query_context empty; // 禁止・排他なしの条件。

    // 設定と条件を両方明示し、成功時の費用を受け取る。
    const long long cost = solver.solve_cost(terminals, opts, empty);

    // この例の費用は十分小さいため、inf()を失敗の目印にできる。
    if (cost == Solver::inf()) {
        std::cout << "failed\n";
        return 0;
    }

    std::cout << "cost=" << cost << '\n';

    int closed_vertex;
    std::cin >> closed_vertex; // 追加入力: 今回使えない中継点。
    const std::vector<int> forbidden{closed_vertex};

    Solver::query_context closed; // 2回目だけの禁止条件。
    closed.forbidden_vertices = forbidden; // vectorを呼び出し終了まで保持する。

    // optionsを省略できるオーバーロード。
    const auto blocked = solver.solve_cost(terminals, closed);

    // 失敗したことを0/1で表示。
    std::cout << "blocked_failed=" << (blocked == Solver::inf()) << '\n';

    // 失敗理由・採用辺・統計が必要ならsolveを使う。solve_cost自体が高速になる保証はない。
    // 合法な合計費用がinf()と等しくなり得る入力では、solveのok()で成否を判定する。
}
```

**入力**

```text
3 2 2
0 1 1
1 2 2
0 2
1
```

**出力**

```text
cost=3
blocked_failed=1
```


### 3.18. 失敗理由を分類し、次の操作を決める

利用者へ「見つからない」「固定条件が矛盾」「入力ミス」を別々に伝える。失敗時の費用を使わず、状態を確認する。

**具体的な問題:** 同じ小さなグラフに条件を変えて問い合わせ、5種類のstatusをすべて表示する。さらに不正な探索上限も検出する。

**入力形式:** 標準入力なし。各状態を生む入力をコード内に記載している。

**コード: `case18.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    Solver solver(3); // 有効な頂点番号は0・1・2だけ。
    solver.add_edge(0, 1, 1); // 辺0。
    solver.add_edge(1, 2, 1); // 辺1。
    solver.add_edge(0, 2, 5); // 辺2: 中継点を避ける経路。

    const auto good = solver.solve({0, 2}); // 条件なしでは合法な接続を得られる。

    // feasibleは実行可能を意味し、最適性の証明ではない。
    std::cout << "normal=" << good.state << '\n';

    Solver::selection_rules fixed_rules(3); // 必須端子の衝突を表す条件。
    fixed_rules.add_conflict(0, 2); // 必ず使う端子同士が排他なので矛盾する。

    Solver::query_context fixed; // この問い合わせ専用の条件。
    fixed.rules = &fixed_rules; // 生存するルールを参照。

    // 必須端子の矛盾を表すproven_infeasible。
    std::cout << "fixed_conflict=" << solver.solve({0, 2}, fixed).state << '\n';

    Solver disconnected(2); // 頂点はあるが、辺は1本もない。

    // この接続不能例はnot_foundになる。
    std::cout << "disconnected=" << disconnected.solve({0, 1}).state << '\n';

    // クエリ内の範囲外頂点はinvalid_input。
    std::cout << "bad_id=" << solver.solve({3}).state << '\n';

    Solver::selection_rules candidate_rules(3); // 次は端子0と任意の中継点1を衝突させる。
    candidate_rules.add_conflict(0, 1); // 直接辺2による合法な答え自体は存在する。

    Solver::query_context candidate_context; // 初期解を評価する条件。
    candidate_context.rules = &candidate_rules; // improveの検査にも条件が適用される。

    // 初期辺に頂点1があるためinvalid_candidate。
    const auto invalid = solver.improve({0, 2}, {0, 1}, candidate_context);

    // この初期解を直したい場合はrepairを選ぶ。
    std::cout << "bad_candidate=" << invalid.state << '\n';

    Solver::options bad_options; // 設定の値域検査を示す。
    bad_options.constraint_node_limit = -2; // -1は自動だが、-2以下は不正。

    // 設定値の不正を表すinvalid_input。
    std::cout << "bad_limit=" << solver.solve({0, 2}, bad_options).state << '\n';

    // add_edge等の前提違反を試す例ではない。登録APIへ不正値を渡すとassertの対象になる。
}
```

**出力**

```text
normal=feasible
fixed_conflict=proven_infeasible
disconnected=not_found
bad_id=invalid_input
bad_candidate=invalid_candidate
bad_limit=invalid_input
```


### 3.19. 制約探索を制限しつつ、手元の合法な解を保持する

厳しい計算予算で、制約を満たす新しい接続が見つからないことがある。すでに合法な接続案を持っていれば、それを失わずに改善を試したい。

**具体的な問題:** 安い経路0–1–2–3は排他に違反し、0–4–3は費用6で合法。深さ0や状態数0で新規探索を制限した結果と、合法な初期解をimproveに渡した結果を比べる。

**入力形式:** 通常のグラフと端子の入力。排他ペア1・2と、既知の合法辺3・4はコード内で指定している。

**コード: `case19.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    Solver::selection_rules rules(n); // 入力グラフと同じ頂点数の制約。
    rules.add_conflict(1, 2); // 安い経路の中継点1・2を同時使用不可にする。

    Solver::query_context context; // この例の全問い合わせに同じ条件を使う。
    context.rules = &rules; // rulesを呼び出し中に変更しない。

    Solver::options opts; // 探索状態数と深さを個別に指定する。
    opts.constraint_node_limit = 16; // 最大16状態を評価する設定。
    opts.constraint_depth_limit = 0; // 最初の状態だけを評価でき、深い探索へ進まない。

    // この入力では合法解を発見できない。
    std::cout << "depth_zero=" << solver.solve(terminals, opts, context).state << '\n';

    opts.constraint_depth_limit = 12; // 深さの制限を緩める。
    opts.constraint_node_limit = 0; // ただし、今度は新規の制約探索自体を止める。

    // not_foundは不存在の証明ではない。
    std::cout << "node_zero=" << solver.solve(terminals, opts, context).state << '\n';

    const std::vector<int> known_edges{3, 4};

    // 合法で連結した初期解は、状態数0でも保持される。
    const auto kept = solver.improve(terminals, known_edges, opts, context);

    std::cout << "kept=" << kept << '\n'; // 費用6の既知の解を返す。

    opts.constraint_node_limit = 16; // 再び新規探索を許す。

    // この入力では合法な迂回路を発見できる。
    std::cout << "searched=" << solver.solve(terminals, opts, context) << '\n';

    // 上限を増やせば必ず成功・改善するわけではない。制約なしsolveを止める上限でもない。
}
```

**入力**

```text
5 5 2
0 1 1
1 2 1
2 3 1
0 4 3
4 3 3
0 3
```

**出力**

```text
depth_zero=not_found
node_zero=not_found
kept=result{state=feasible, cost=6, edges=[3, 4]}
searched=result{state=feasible, cost=6, edges=[3, 4]}
```


### 3.20. 探索設定と乱数の種を変え、最良の成功結果を残す

計算時間に余裕がある場合、異なる設定や乱数の種を使って複数回解き、最も安い成功結果を採用する。

**具体的な問題:** 端子0・2・3をつなぐ問題を、3種類のpresetと3種類のseedで解く。この小例では共有中継点1を使う費用6の答えが残る。

**入力形式:** 通常のグラフと端子の入力。比較するpreset・seed・再スタート上限はコード内で指定する。

**コード: `case20.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    // 軽量・中間・品質寄りの3設定。
    const Solver::preset presets[] = {
        Solver::preset::fast,      // 計算量を抑える設定。
        Solver::preset::balanced,  // 計算量と品質の中間設定。
        Solver::preset::quality    // より多くの改善を試す設定。
    };

    Solver::result best; // 既定のstateはnot_found。まだ解がない状態として使う。

    // presetは速度や品質の厳密な順位を保証する値ではない。
    for (Solver::preset preset : presets) {
        for (std::uint64_t seed = 0; seed < 3; ++seed) {
            Solver::options opts; // 毎回、他の項目を既定値から設定する。
            opts.preset_mode = preset; // 今回試す探索規模。
            opts.seed = seed; // 乱数を使う探索の種。同じ答えになる場合もある。
            opts.restart_limit = 2; // 各呼び出しの追加再スタートを最大2回にする。

            // 距離表の扱いはライブラリに選ばせる。
            opts.path_mode_value = Solver::path_mode::auto_select;

            auto answer = solver.solve(terminals, opts); // 完成した設定で解く。

            // 成功した答え同士で費用を比較する。
            if (answer.ok() && (!best.ok() || answer.cost < best.cost)) {
                best = std::move(answer); // 結果の辺集合と統計も含めて最良結果を保持。
            }
        }
    }

    std::cout << best << '\n'; // 全回失敗ならnot_found。失敗結果の費用は採用しない。

    // この外側の繰り返し自体には合計時間制限がない。必要なら共通deadlineを追加する。
}
```

**入力**

```text
4 6 3
0 1 2
1 2 2
1 3 2
0 2 7
0 3 7
2 3 7
0 2 3
```

**出力**

```text
result{state=feasible, cost=6, edges=[0, 1, 2]}
```


### 3.21. 複数の試行に共通の締切を設ける

1回ごとの時間予算と、複数回をまとめた締切を両方指定したい。時間切れでも利用できるよう、すでに持っている合法な接続案を保持する。

**具体的な問題:** 共有中継点1を使う辺0・1・2を初期の接続案として検証する。その後、各回10ms、共通の締切100ms、最大3回で別の接続を試し、最良案を表示する。

**入力形式:** 通常のグラフと端子の入力。初期の候補辺は、この入力の辺0・1・2としてコード内に指定している。

**コード: `case21.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    // 入力: N M K、続いてM本の「頂点u 頂点v 費用」、続いてK個の端子番号。
    // N・M・Kは0以上、頂点番号は0〜N-1、費用は0以上の整数という有効な入力を前提とする。
    int n, m, k;
    if (!(std::cin >> n >> m >> k)) {
        return 0;
    }

    Solver solver(n); // 頂点番号0〜n-1を持つ空グラフを作る。nは0以上とする。
    for (int i = 0; i < m; ++i) {
        int u, v;
        long long cost;
        std::cin >> u >> v >> cost;

        solver.add_edge(u, v, cost); // 非負費用の無向辺を登録。入力順のiが辺番号になる。
    }

    std::vector<int> terminals(k);
    for (int &v : terminals) {
        std::cin >> v;
    }

    const std::vector<int> known_edges{0, 1, 2};

    // 時間を区切る前に、手元の接続案を合法な初期結果にする。
    auto best = solver.normalize(terminals, known_edges);

    // 接続できない案を「時間切れ時の答え」として使わない。
    if (!best.ok()) {
        std::cout << best.state << '\n'; // 初期案の検証失敗を表示する。
        return 0;
    }

    using Clock = std::chrono::steady_clock;
    const auto common_deadline = Clock::now() + std::chrono::milliseconds(100);

    for (std::uint64_t seed = 0; seed < 3 && Clock::now() < common_deadline; ++seed) {
        Solver::options opts; // 各呼び出しの設定を作る。
        opts.preset_mode = Solver::preset::quality; // 予算内で品質を重視した探索を試す。
        opts.seed = seed; // 各回で異なる乱数の種を渡す。
        opts.time_limit_ms = 10; // この1回の相対的な停止目標は10ms。
        opts.deadline = common_deadline; // 共通の絶対締切を渡す。各回で100ms先へ延ばさない。
        opts.restart_limit = 2; // この例では時間に加えて、追加再スタート数も制限する。

        // 相対予算と共通締切の早い方を停止目標にする。
        auto answer = solver.solve(terminals, opts);

        // 未発見や高い結果で、既知の合法な答えを上書きしない。
        if (answer.ok() && answer.cost < best.cost) {
            best = std::move(answer); // 成功した安い結果だけを保持する。
        }
    }

    std::cout << best << '\n'; // 試行回数や所要時間に依存せず、保持した合法な答えを表示。

    // deadlineはハードな実行時間保証ではない。前処理や中断されない処理で超過し得る。
    // この例のnormalizeは計時対象外。アプリ全体の予算へ含めるなら、さらに外側で時間を測る。
}
```

**入力**

```text
4 6 3
0 1 2
1 2 2
1 3 2
0 2 7
0 3 7
2 3 7
0 2 3
```

**出力**

```text
result{state=feasible, cost=6, edges=[0, 1, 2]}
```

**補足:** 出力に実行回数やミリ秒を含めていない。実行環境の負荷によって、その値が変わるためである。


### 3.22. 端子が空・1個・重複している場合を扱う

利用拠点がまだ決まっていない段階や、同じ拠点が複数の要求に現れる場合にも、入力を自然に扱いたい。接続が不要でも固定条件の矛盾は残る。

**具体的な問題:** 0頂点で端子なし、端子1だけを2回指定、費用0の辺による2端子接続を試す。最後に、排他に違反する既設辺を端子なしで渡す。

**入力形式:** 標準入力なし。0頂点の問題と、3頂点の問題をそれぞれコード内で作る。

**コード: `case22.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    Solver empty(0); // 0頂点のグラフも構築できる。このグラフでは辺や端子を追加できない。

    // 第1引数は空の端子vector。必要な辺は0本。
    std::cout << "empty=" << empty.solve({}) << '\n';

    Solver solver(3); // 頂点番号は0・1・2。

    // 費用0は有効。後半では撤去不能な既設辺に使う。
    const int base_edge = solver.add_edge(0, 1, 0);
    solver.add_edge(1, 1, 0); // 自己ループも登録可能だが、別の頂点をつなぐ用途にはならない。
    solver.add_edge(1, 2, 3); // 頂点2へ延ばすための辺。

    // 重複端子は同じ1頂点。接続辺は不要。
    std::cout << "single=" << solver.solve({1, 1}) << '\n';

    // 費用0でもok()がtrueの接続結果になる。
    std::cout << "zero_cost=" << solver.solve({0, 1}) << '\n';

    Solver::selection_rules rules(3); // 既設端点の固定条件を調べる。
    rules.add_conflict(0, 1); // base_edgeの両端が同時使用不可になる。

    Solver::query_context context; // 接続要求が空でも制約を渡せる。
    context.rules = &rules; // 固定された既設頂点にも排他を適用する。

    // 端子なしでも既設辺は撤去できず、固定条件が矛盾する。
    const auto impossible = solver.augment({}, {base_edge}, context);

    // 既設辺の端点の矛盾を表すproven_infeasible。端子数だけで成功を決めない。
    std::cout << "empty_but_fixed=" << impossible.state << '\n';
}
```

**出力**

```text
empty=result{state=feasible, cost=0, edges=[]}
single=result{state=feasible, cost=0, edges=[]}
zero_cost=result{state=feasible, cost=0, edges=[0]}
empty_but_fixed=proven_infeasible
```


### 3.23. 大きな結果を短く表示し、必要に応じて統計も確認する

長い辺一覧でログが埋まらないよう、結果の概要だけを直接出力する。診断したいときは統計や個々の辺を別に表示する。

**具体的な問題:** 26頂点を費用1の25本の辺で一直線につなぎ、端子0・25の接続を正規化する。結果表示は20辺で省略されるが、実際の配列には25辺すべてが入っている。

**入力形式:** 標準入力なし。辺番号は0から24までを順に登録する。

**コード: `case23.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    Solver solver(26); // 頂点番号0〜25の一本道を用意する。

    std::vector<int> candidate_edges;
    for (int v = 0; v < 25; ++v) {
        // 戻り値の辺番号を候補集合に保存する。
        candidate_edges.push_back(solver.add_edge(v, v + 1, 1));
    }

    // 候補だけで端子間をつなぐ。全25辺が必要。
    const auto answer = solver.normalize({0, 25}, candidate_edges);

    // デバッグ表示の前でも、答えとして使う場合は成否を確認する。
    if (!answer.ok()) {
        std::cout << answer.state << '\n'; // 失敗状態の列挙子名を表示。
        return 0;
    }

    std::cout << answer << '\n'; // 配列は先頭20件と省略件数。statsはこの行に含まれない。

    // 表示の省略は元の配列を変更しない。
    std::cout << "actual_edges=" << answer.edges.size() << '\n';

    // edge_typeも両端と費用を直接表示できる。
    std::cout << solver.edge(answer.edges.front()) << '\n';
    std::cout << answer.stats << '\n'; // 全統計を名前付きで表示。成功でも0のカウンタがある。

    // ログを標準エラーへ出したい場合は、上のstd::coutをstd::cerrに置き換えられる。
    // 演算子は改行しないため、各行の末尾に'\n'を付けている。
}
```

**出力**

```text
result{state=feasible, cost=25, edges=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (+5 more)]}
actual_edges=25
edge{from=0, to=1, cost=1}
statistics{path_mode_used=on_demand, path_memory_bytes=0, initial_solution_count=0, restart_count=0, local_move_count=0, improvement_count=0, dijkstra_count=0, edge_relaxation_count=0, constraint_nodes=0, conflict_candidates=0, feasible_candidates=1}
```


### 3.24. 任意の中継点を選ぶ形から、全拠点を必須にする形へ変える

少数の拠点だけでなく、登録したすべての施設を必ず相互接続したい。端子の指定を変えるだけで、全頂点を接続する問題として使える。

**具体的な問題:** 4頂点すべてを端子にする。費用1・2・3の辺で一直線につなぐと合計6で、費用10の追加候補を選ぶ必要はない。

**入力形式:** 標準入力なし。グラフと、全頂点から作る端子集合をコード内に記載している。

**コード: `case24.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    Solver solver(4); // 全4頂点が、今回は必須の拠点になる。
    solver.add_edge(0, 1, 1); // 辺0。
    solver.add_edge(1, 2, 2); // 辺1。
    solver.add_edge(2, 3, 3); // 辺2。

    solver.add_edge(0, 2, 10); // 辺3: 高い代替候補。
    solver.add_edge(0, 3, 10); // 辺4: 高い代替候補。
    solver.add_edge(1, 3, 10); // 辺5: 高い代替候補。

    std::vector<int> terminals;

    // 登録した全頂点の番号を列挙する。
    for (int v = 0; v < solver.vertex_count(); ++v) {
        terminals.push_back(v); // 全頂点を端子にすると、任意に省ける中継頂点はなくなる。
    }

    const auto answer = solver.solve(terminals); // 全端子が互いにつながる接続を求める。

    std::cout << answer << '\n'; // この入力では全域木の費用6を得る。

    // 全頂点が必須なので、異なる2頂点を排他にすると固定条件が矛盾する。
}
```

**出力**

```text
result{state=feasible, cost=6, edges=[0, 1, 2]}
```


### 3.25. 離れている複数の既設網をつなぎ、既設の閉路も残す

部署ごとに建設した離れたネットワークを統合する。既設の余分な回り道も撤去できないため、単純な木へ削らず、接続に必要な新設部分だけを選ぶ。

**具体的な問題:** 頂点0・1・2の既設網には三角形の閉路があり、頂点3・4・5の既設網とは離れている。端子0・5を結ぶには、辺2–3を費用4で追加すればよい。

**入力形式:** 標準入力なし。既設辺5本と追加候補2本をコード内に記載している。

**コード: `case25.cpp`**

```cpp
#include "constrained_steiner_tree_v10.hpp" // 制約付きSteiner treeのライブラリを読み込む。
#include <iostream>
#include <vector>

using Solver = constrained_steiner_tree; // 長いクラス名を、この例だけSolverと略す。

int main() {
    Solver solver(6); // 2つの既設成分を同じグラフに登録する。
    solver.add_edge(0, 1, 10); // 辺0: 第1の既設成分。
    solver.add_edge(1, 2, 10); // 辺1: 第1の既設成分。
    solver.add_edge(0, 2, 25); // 辺2: 既設の閉路を作る辺。これも撤去不可。

    solver.add_edge(3, 4, 8); // 辺3: 第2の既設成分。
    solver.add_edge(4, 5, 8); // 辺4: 第2の既設成分。

    solver.add_edge(2, 3, 4); // 辺5: 追加候補の橋渡し。
    solver.add_edge(1, 4, 7); // 辺6: より高い橋渡し候補。

    const std::vector<int> base{0, 1, 2, 3, 4};

    // 既設辺は非連結・閉路ありでも渡せる。追加費用だけを最小化する。
    const auto answer = solver.augment({0, 5}, base);

    std::cout << answer << '\n'; // 既設費用を再計上せず、新規の辺5だけが返る。

    // 追加接続に失敗したら全体を更新しない。
    if (!answer.ok()) {
        return 0;
    }

    auto network = base;

    // 全既設辺と新規分を保持する。
    network.insert(network.end(), answer.added_edges.begin(), answer.added_edges.end());

    // 6頂点・6辺で、既設の閉路も残っている。
    std::cout << "network_edges=" << network.size() << '\n';

    // 固定辺を残す用途で、このnetworkをnormalizeの返却辺に置き換えない。
    // normalizeは辺を削除できる。
    // 既設成分すべてを接続する要求なら、各成分から少なくとも1頂点を端子に含める。
}
```

**出力**

```text
augmentation_result{state=feasible, added_cost=4, added_edges=[5]}
network_edges=6
```

### 検証環境と実行用ファイル

掲載した25本は、GCC 13.3.0、C++20、`-O0 -Wall -Wextra -Wshadow -Wconversion -Werror` で個別にビルドし、掲載入力で実行して、掲載出力との一致を確認した。時間予算を使う例では、環境で変動する実行時間や試行回数を固定の期待出力に含めていない。

ZIP版には本書、対象ヘッダー、各例の `.cpp`・`.in`・`.out`、再実行用の `validate_samples.py` を同梱する。入力不要の例の `.in` は空ファイル。例1だけ実行する場合は、展開したフォルダーで次のようにする。

```sh
g++ -std=c++20 -O2 -Wall -Wextra examples/case01.cpp -o case01
./case01 < examples/case01.in
```

すべての掲載例をビルドして入出力を照合するには `python3 validate_samples.py` を実行する。コンパイラやOSが違えば、距離表のバイト数など環境に依存する表示が異なることがある。
