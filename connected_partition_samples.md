# ConnectedPartitionSolver サンプルコード集・APIリファレンス

対象：`connected_partition_solver_v12.hpp`。C++20／C++23、GCCとlibstdc++を想定します。

この文書は、グラフを条件付きで領域に分けるライブラリの使い方を、独立して実行できるサンプルで説明します。必要な数学は、足し算・不等式・二乗・平均です。C++の変数・配列・ループ・関数は知っているものとします。

構成は「1. 対象問題」「2. 全公開APIのリファレンス」「3. 用途別の実行例」です。各例は単独の `main` を持ち、ライブラリの設定・呼出し・結果の参照には日本語の行コメントを付けています。前の例のコードを継ぎ足す必要はありません。

## 1. 何を解くライブラリか？

### 1.1 区画を、つながった担当区域に分ける

地図の区画をいくつかの担当区域に分ける場面を考えます。区画を**頂点**、区画間のつながりを**辺**と呼び、この組を**グラフ**と呼びます。盤面のマスや、隣り合う施設も同じ形で表せます。

各頂点に領域番号を1個付けます。この番号を**ラベル**とも呼びます。同じラベルの頂点を集めたものが1つの領域です。頂点が6個、領域番号が2種類なら、頂点番号は0〜5、領域番号は0〜1です。この2種類の番号を区別してください。

頂点0、1、2、3、4、5がこの順で1列につながっているとします。

| 頂点番号 | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| 分け方A | 0 | 0 | 0 | 1 | 1 | 1 |
| 分け方B | 0 | 1 | 0 | 1 | 1 | 1 |

Aでは領域0の中だけを通って頂点0から2へ移動できます。Bでは間の頂点1が別の領域なので移動できません。領域の中だけを通って任意の2頂点の間を移動できることを**連結**と呼びます。

既定では、全領域が空でなく、連結であることを要求します。そのうえで、分け方の費用を小さくします。出力は各頂点のラベルの配列です。訪問順、経路、領域の代表点は出力しません。

### 1.2 必ず守る条件と、良さを比べる費用

**制約**は必須条件です。すべての制約を満たす分け方を、この文書では**合法解**と呼びます。次の条件を組み合わせられます。

| 条件 | 例 |
|---|---|
| 領域の連結性 | 配達担当区域を飛び地にしない |
| 領域の使用・未使用 | 必須区域は空にしない。任意区域は空でもよい |
| 頂点数の上下限 | 使用する区域は2〜4区画にする |
| 複数種類の整数負荷の上下限 | 人数と荷物量を、それぞれの容量内に収める |
| 固定ラベル | 拠点のある区画を特定の担当に固定する |
| 許可ラベル集合 | ある区画を担当できる候補を限定する |
| 領域間の接触辺数 | 区域0と1は接し、区域0と2は接しない |
| 使用領域数 | 3候補のうち2〜3区域を使う |
| 独自の追加条件 | 複数区域を比較する利用者定義の条件を守る |

**目的関数**は合法解どうしの良さを比べる式です。このライブラリでは、目的値は**小さいほど良い**値です。「必ず負荷を3〜5にする」は制約、「負荷が4に近いほど良い」は目的関数です。両方を同時に設定できます。

すべての頂点にラベルを付けます。「どこにも割り当てない」を表すラベル `-1` は解には使えません。未割当や背景を表したい場合は、その意味を持つ領域番号を1つ用意します。

### 1.3 標準の目的関数

独自モデルを渡さない場合、合法なラベル配列 $x$ の集合 $\mathcal{F}$ の中で、次の目的値を小さくします。

$$\min_ {x\in\mathcal{F}} F(x)$$

$$F(x)=\sum_ {v=0}^{N-1}U_ {v,x_ {v}}+\sum_ {e\in E_ {\mathrm{cost}}}c_ {e}[x_ {u_ {e}}\ne x_ {v_ {e}}]+\sum_ {r=0}^{K-1}[n_ {r}>0]a_ {r}+\sum_ {t\in\mathcal{B}}[n_ {r_ {t}}>0]\lambda_ {t}(L_ {r_ {t},d_ {t}}-T_ {t})^{2}$$

$$n_ {r}=\sum_ {v=0}^{N-1}[x_ {v}=r],\qquad L_ {r,d}=\sum_ {v=0}^{N-1}[x_ {v}=r]w_ {v,d}$$

$\sum$ は指定した範囲の値を全部足す記号です。角括弧 `[条件]` は、条件が成り立てば1、成り立たなければ0です。

| 記号 | 意味 | APIとの対応 |
|---|---|---|
| $N$ | 頂点の個数 | `Problem.n` |
| $K$ | 領域番号の種類数 | `Problem.k` |
| $v$ | 頂点番号。0〜$N-1$ | `labels[v]` など |
| $r$ | 領域番号。0〜$K-1$ | `regions[r]` など |
| $x$ | 全頂点のラベルを並べた配列 | `improve` 等の入力、`best_labels()` の出力 |
| $x_ {v}$ | 頂点 $v$ のラベル | `labels[v]` |
| $\mathcal{F}$ | 設定した全制約を満たす配列の集合 | 連結性、個数、負荷、接触、追加条件など |
| $F(x)$ | 分け方 $x$ の目的値 | `Evaluation.cost`、`Result.best_cost` |
| $U_ {v,r}$ | 頂点 $v$ を領域 $r$ に割り当てる費用 | `unary_cost[v*k+r]` |
| $E_ {\mathrm{cost}}$ | 有効な `PairCost` 辺の集合 | `edges` のうち、その役割を持つ辺 |
| $e$ | 費用を計算する辺の番号 | `edges[e]` |
| $u_ {e},v_ {e}$ | 辺 $e$ の端点 | `Edge.u`、`Edge.v` |
| $c_ {e}$ | 端点のラベルが異なる場合の費用 | `Edge.cut_cost` |
| $n_ {r}$ | 領域 $r$ の頂点数 | `RegionStatsView.vertices` |
| $a_ {r}$ | 領域 $r$ を使用する固定費 | `activation_cost[r]` |
| $D$ | 整数負荷の種類数 | `load_dim` |
| $d$ | 負荷の種類番号。0〜$D-1$ | `load_bounds[d]` など |
| $w_ {v,d}$ | 頂点 $v$ の種類 $d$ の負荷 | `load[v*load_dim+d]` |
| $L_ {r,d}$ | 領域 $r$ の種類 $d$ の負荷合計 | `RegionStatsView.load[d]` |
| $\mathcal{B}$ | 登録した均衡項の集まり | `balance_terms` |
| $t$ | 均衡項の番号 | `balance_terms[t]` |
| $r_ {t},d_ {t}$ | 項 $t$ が対象とする領域と負荷番号 | `BalanceTerm.region`、`.dim` |
| $T_ {t}$ | 項 $t$ が目指す負荷合計 | `BalanceTerm.target` |
| $\lambda_ {t}$ | 項 $t$ のずれに掛ける係数 | `BalanceTerm.coefficient` |

式は、左から「配置費用」「異なる領域を結ぶ辺の費用」「使用領域の固定費」「使用領域の負荷の偏り」の合計です。空の領域には、標準の固定費も均衡項も掛かりません。

目標が4なら、負荷3でも5でも、ずれの二乗は1です。負荷2なら4です。係数2を掛けると、それぞれ費用2、2、8になります。偏りを減らす目的なら、通常は係数を非負にします。

配置費用・固定費・均衡項を省略すると、それらは0です。辺の切断費用を省略すると1です。そのため最小例は、領域の境界になった辺の費用だけを最小化します。辺の `length` と実数特徴 `feature` は、設定しただけでは標準の目的値へ入りません。

### 1.4 独自の費用を使う場合

費用を関数で定義する場合も、頂点・辺・領域・全体の費用を足します。

$$F(x)=\sum_ {v=0}^{N-1}f_ {\mathrm{vertex}}(v,x_ {v})+\sum_ {e\in E_ {\mathrm{cost}}}f_ {\mathrm{edge}}(e,x_ {u_ {e}},x_ {v_ {e}})+\sum_ {r=0}^{K-1}f_ {\mathrm{region}}(r,S_ {r}(x))+f_ {\mathrm{global}}(S(x))$$

ここで $f_ {\mathrm{vertex}}$ は `vertex_cost`、$f_ {\mathrm{edge}}$ は `edge_cost`、$f_ {\mathrm{region}}$ は `region_cost`、$f_ {\mathrm{global}}$ は `global_cost` が返す費用です。$S_ {r}(x)$ は領域 $r$ の頂点数・負荷合計・特徴合計・境界長、$S(x)$ は全ラベルと全領域・登録接触の情報です。その他の記号は前節と同じです。

定義した関数は、対応する標準項を**置き換えます**。たとえば `region_cost` を書くと、標準の使用固定費と均衡項は自動では加わりません。未定義の関数に対応する項は、標準のままです。

割当先で負荷が変わる場合は、$w_ {v,r,d}$ を `load(v,r,d)` の戻り値として、次の合計を使います。$w_ {v,r,d}$ は、頂点 $v$ を領域 $r$ に置いたときの種類 $d$ の整数負荷です。

$$L_ {r,d}=\sum_ {v=0}^{N-1}[x_ {v}=r]w_ {v,r,d}$$

このライブラリは、最適解や合法解の発見を保証するものではありません。合法解を得られなかったという結果は、解が存在しないことの証明にはなりません。

## 2. APIリファレンス

### 2.1 型、配列、寿命の共通ルール

以下では `Solver = ConnectedPartitionSolver<Cost>` と書きます。

| 公開型・別名 | 内容 |
|---|---|
| `ConnectedPartitionSolver<Cost = double>` | 本体。`Cost` は `double` または `int64_t` のみ |
| `Solver::Clock` | `std::chrono::steady_clock`。絶対締切に使用 |
| `Solver::Problem` | 入力問題を所有する構造体 |
| `Solver::IntRange`, `Edge`, `RegionRule`, `LabelDomain`, `ContactRule`, `BalanceTerm` | 問題を構成する型。以下で全フィールドを説明 |
| `Solver::Options` | 1回の探索呼出しの設定 |
| `Solver::Acceptance`, `Neighborhood`, `EdgeRole`, `StopReason` | 列挙型。全値を後述 |
| `Solver::Evaluation`, `Result`, `Statistics`, `NeighborhoodStats` | 評価結果・探索結果・統計 |
| `Solver::RegionStatsView`, `ContactStats`, `SummaryView` | 独自モデルで読む集計・全体ビュー |

`std::optional<T>` は、型 `T` の値がある場合とない場合を表します。`if (result.best_cost)` は値の有無を判定し、`*result.best_cost` は値があるときだけ使います。目的値0にも値はあります。

`std::span<const T>` は配列をコピーせずに読む窓口です。元の配列を所有しません。`best_labels()` の返すspanは、solverの次の変更操作まで有効です。長く保存したい結果は `std::vector` にコピーします。`movable` に渡す配列は、その呼出しが終わるまで有効に保ちます。

頂点・領域・辺などの番号は0始まりです。通常のAHC規模を想定し、ID・要素数は `int`、整数の合計・差分・中間積は `int64_t` に収めます。`double` の入力・評価値は有限値とします。整数の厳密な目的値比較には `int64_t` を選びます。実数では集計に丸め誤差があります。

表の `INT_MAX` は `int` で表せる最大の整数、`INT64_MIN`・`INT64_MAX` は `int64_t` の最小値・最大値です。既定の範囲でこれらを使う箇所は、通常の入力規模で追加の上限・下限を設けないための設定です。

形式違反は `assert` の対象です。たとえば配列長不一致や範囲外IDを、解なしという戻り値で受け付けるAPIではありません。`assert` が無効でも利用条件を守る必要があります。ライブラリは明示的に例外をthrowしません。同一solverを複数スレッドから同時に使いません。

### 2.2 `Problem` とコンストラクタ

`explicit Problem(int vertices, int labels)` は、`n=vertices`、`k=labels` とし、`regions` に `k` 個の既定ルールを作ります。`vertices >= 0`、`labels > 0` が条件です。引数の既定値はありません。ほかの配列は空です。

| フィールド | 型・既定値 | 値・意味 |
|---|---|---|
| `n` | `int`、コンストラクタ第1引数 | 頂点数。0以上 |
| `k` | `int`、コンストラクタ第2引数 | 領域番号の種類数。1以上 |
| `edges` | `vector<Edge>`、空 | 入力順が辺ID。自己ループは禁止。平行辺は別の辺として数える |
| `regions` | `vector<RegionRule>`、長さ `k` | 添字が領域番号。長さは必ず `k` |
| `load_dim` | `int = 0` | 整数負荷の種類数。0以上 |
| `feature_dim` | `int = 0` | 実数特徴の種類数。0以上 |
| `load` | `vector<int64_t>`、空 | 空なら標準負荷は全て0。設定時は長さ `n*load_dim`、添字 `v*load_dim+d`。負数も可 |
| `feature` | `vector<double>`、空 | 空なら特徴は全て0。設定時は長さ `n*feature_dim`、添字 `v*feature_dim+q`。各領域内の和を提供 |
| `fixed` | `vector<int>`、空 | 空なら固定なし。設定時は長さ `n`。`-1` は固定なし、`0..k-1` は固定先 |
| `domains` | `vector<LabelDomain>`、空 | 頂点ごとの許可ラベル。未登録頂点は全ラベルを許可。固定条件との両方を満たす必要がある |
| `contacts` | `vector<ContactRule>`、空 | 領域ペアの接触制約と集計登録。順序が `SummaryView.contact(i)` の添字 |
| `allow_unlisted_contacts` | `bool = true` | trueなら未登録ペアの接触を許可。falseなら有効な `Contact` 辺を介する未登録ペアの接触を禁止 |
| `min_active_regions` | `int = 0` | 使用領域数の下限。0以上 |
| `max_active_regions` | `int = INT_MAX` | 使用領域数の上限。下限以上。実際の使用数は `k` 以下 |
| `unary_cost` | `vector<Cost>`、空 | 空なら配置費用0。設定時は長さ `n*k`、添字 `v*k+r`。負の費用も可 |
| `activation_cost` | `vector<Cost>`、空 | 空なら使用固定費0。設定時は長さ `k`。使用中の領域だけ加算 |
| `balance_terms` | `vector<BalanceTerm>`、空 | 空なら均衡費用0。複数項は全て加算 |

`n` や `k` を変える場合は、依存する配列の長さ・IDも合わせます。新しい `Problem` を作ると、古いサイズの配列を残しにくくなります。

### 2.3 辺と `EdgeRole`

`Edge` の集成体初期化の順序は `{u, v, cut_cost, length, enabled, roles}` です。省略した末尾の項目は表の既定値になります。

| フィールド | 型・既定値 | 意味・条件 |
|---|---|---|
| `u`, `v` | `int`、メンバー既定値なし | 異なる2頂点。各 `0..n-1`。通常は必ず明示する |
| `cut_cost` | `Cost = 1` | 標準モデルで、両端のラベルが違う場合に加算する費用。負数も可 |
| `length` | `double = 1` | 接触長。有限で0以上。標準の費用とは独立 |
| `enabled` | `bool = true` | falseなら全役割で無効。役割や費用を変更した入力を反映するには `update` が必要 |
| `roles` | `unsigned = Connect \| Contact \| PairCost` | 0〜7のビットの組合せ。0はどの役割にも使わない |

| `EdgeRole` の値 | 数値 | 意味 |
|---|---|---|
| `Solver::Connect` | 1 | 同じ領域の頂点間の連結性を表す。無向のつながり |
| `Solver::Contact` | 2 | 異なる領域の接触辺数・接触長・領域境界長に使う |
| `Solver::PairCost` | 4 | 標準の切断費用、または独自の `edge_cost` に使う |

ビットの組合せはC++の `|` で指定します。たとえば `Connect | Contact` は接続と接触に使い、費用には使いません。接続用の辺と、離れた設備間の費用用の辺を分けて登録できます。`Connect` のない辺は、費用があっても領域をつなぎません。

独自の `edge_cost(e,a,b)` では、`a` が保存された `u` 側、`b` が `v` 側のラベルです。費用だけは非対称にも定義できます。接続自体が有向になるわけではありません。平行辺を入力すると、その本数分だけ接触と費用を数えます。

### 2.4 領域ルールと負荷範囲

| `RegionRule` のフィールド | 型・既定値 | 意味・条件 |
|---|---|---|
| `required` | `bool = true` | trueなら必ず1頂点以上を含める。`min_vertices=0` にしても空にできない |
| `connected` | `bool = true` | 非空領域を `Connect` 辺で連結にする。1頂点なら連結 |
| `min_vertices` | `int = 1` | 非空領域の頂点数下限。0以上 |
| `max_vertices` | `int = INT_MAX` | 非空領域の頂点数上限。下限以上 |
| `load_bounds` | `vector<IntRange>`、空 | 空なら負荷制約なし。設定時は長さ `load_dim`。非空領域の各負荷合計を閉区間に収める |

| `IntRange` のフィールド | 型・既定値 | 意味 |
|---|---|---|
| `lower` | `int64_t = INT64_MIN` | 含む下限 |
| `upper` | `int64_t = INT64_MAX` | 含む上限。`lower <= upper` |

`required=false` の空領域では、頂点数の下限と負荷範囲を免除します。使用すると指定範囲を守ります。たとえば「閉鎖可、開くなら3区画以上」は `required=false, min_vertices=3` です。接触制約や使用領域数の制約まで免除するわけではありません。

### 2.5 許可ラベル、接触条件、均衡項

| 型とフィールド | 型・既定値 | 意味・条件 |
|---|---|---|
| `LabelDomain.vertex` | `int`、メンバー既定値なし | 対象頂点。`0..n-1`。同じ頂点を複数回登録しない |
| `LabelDomain.labels` | `vector<int>`、空 | 許可する `0..k-1` の集合。順序不問、重複は内部コピーで整理される。登録した集合が空なら、その頂点には許可先がない |
| `ContactRule.a`, `.b` | `int`、メンバー既定値なし | 異なる領域番号。各 `0..k-1`。順不同の同じペアを重複登録しない |
| `ContactRule.min_edges` | `int = 0` | そのペアを結ぶ有効な `Contact` 辺数の下限。0以上 |
| `ContactRule.max_edges` | `int = INT_MAX` | 接触辺数の上限。下限以上 |
| `BalanceTerm.region` | `int`、メンバー既定値なし | 対象領域。`0..k-1` |
| `BalanceTerm.dim` | `int`、メンバー既定値なし | 対象負荷。`0..load_dim-1` |
| `BalanceTerm.target` | `Cost`、メンバー既定値なし | 目標負荷合計 |
| `BalanceTerm.coefficient` | `Cost`、メンバー既定値なし | 二乗したずれに掛ける係数。通常は非負。負値を使うと大きなずれを好む別の目的になる |

「メンバー既定値なし」の値は利用者が明示してください。C++の `{}` による値初期化と、ライブラリが意味のある既定値を与えることは別です。

`ContactRule{0,1}` は接触量の登録だけを行い、接触を必須にはしません。`{0,1,1,INT_MAX}` なら1本以上、`{0,1,0,0}` なら禁止です。制約は**長さではなく辺数**です。長さ0の接触辺も1本と数えます。領域内の辺は接触数に数えません。

### 2.6 `Options` の全設定

| フィールド | 型・既定値 | 意味・取り得る値 |
|---|---|---|
| `budget_us` | `int64_t = 1'000'000` | 呼出し開始からの相対時間予算、単位はマイクロ秒。0以上、または `-1` で無効 |
| `max_steps` | `int64_t = -1` | 候補試行数の上限。0以上、または `-1` で無効。修復も数える |
| `deadline` | `optional<Clock::time_point>`、値なし | 絶対締切。相対時間予算と同時なら早い方。`reset()` で解除 |
| `seed` | `uint64_t = 0` | 新規開始の乱数初期値。型の範囲内の値を指定。`solve`・`improve`・`repair` で使う |
| `movable` | `optional<span<const int>>`、値なし | 変更してよい頂点番号の集合。未指定なら固定条件を除いて全頂点。明示的な空spanなら誰も動かさない |
| `acceptance` | `Acceptance::annealing` | 合法解の改善中の採用方式。下表の2値 |
| `weights` | `array<int,5> = {60,18,12,2,7}` | 5近傍の選択重み。各0以上、合計が `int` に収まる値。0の種類は改善探索で選ばない |
| `temperature` | `double = 0` | 0なら悪化差分から自動推定。正の有限値なら初期温度を指定。負値は不可 |
| `final_temperature_ratio` | `double = 0.02` | 最後の温度の割合。`0 < 値 <= 1`。1なら冷却の割合は一定 |
| `profile` | `bool = false` | trueなら近傍ごとの時間を測る。計時にも費用が掛かる |

停止条件は、相対時間・試行数・絶対締切の少なくとも1つを有効にします。`budget_us=-1, max_steps=-1, deadlineなし` は不可です。`max_steps=0` や `budget_us=0` でも、初期解の取り込み・検査・採点は行います。`solve` では最初の構築も行います。ゼロ予算が何もせず即座に戻るという意味ではありません。

時間は指定単位がマイクロ秒という意味で、厳密な復帰時刻の保証ではありません。重い1回の処理や利用者関数を途中で強制中断しません。入出力、構築前の処理、代替解の準備に必要な余裕も確保します。

`movable` の要素は頂点番号で、各 `0..n-1` です。順序と重複は内部で整理します。固定頂点を含めても、改善中に固定先を変える許可にはなりません。修復開始時には、可動集合内の固定・許可違反を正しい割当先へ直す処理があります。集合外の違反は自動で変更しません。

`Options` は呼出しごとの設定です。前回 `greedy` を指定していても、次に引数を省略すれば既定の `annealing` になります。`resume` は乱数の内部状態を継続するため、そこへ別の `seed` を渡しても独立開始にはなりません。固定seed・試行数だけの停止は、同じ実装・環境で比較しやすい設定です。時間停止では試行数が変わり得ます。

### 2.7 `Acceptance` と `Neighborhood`

| `Acceptance` の値 | 動作・用途 |
|---|---|
| `Acceptance::annealing` | 既定。改善と同点の候補を受け入れ、悪化も温度と差分に応じた確率で受け入れる。保持する最良解は別に残る |
| `Acceptance::greedy` | 改善と同点を受け入れ、悪化を受け入れない山登り法。良い初期解の短い仕上げ、短時間の反復更新、温度の影響を外した比較で試しやすい |

どちらも合法性を緩める設定ではありません。厳しい制約で1頂点も合法に動かせない場合は、複数頂点をまとめて変更する近傍や別の初期解が必要になることがあります。修復段階は専用の違反量・採用方式を使うため、`acceptance`・`temperature`・`weights` は修復の方針をそのまま指定するものではありません。

| `Neighborhood` の値 | 添字 | 既定重み | 提案する変更 |
|---|---|---|---|
| `Solver::Move` | 0 | 60 | 1頂点を別の領域へ移す |
| `Solver::Swap` | 1 | 18 | 異なる領域の2頂点の割当先を交換する |
| `Solver::Block` | 2 | 12 | つながった複数頂点の塊を移す、または塊どうしを交換する |
| `Solver::Relabel` | 3 | 2 | 領域全体のラベル交換や、別ラベルへの統合を提案する |
| `Solver::Recombine` | 4 | 7 | 2領域を合わせて、つながりを利用して分け直す |
| `Solver::NeighborhoodCount` | 5 | 対象外 | 近傍の種類数。実際の近傍ではなく、配列長・ループ上限に使う |

重みは百分率ではありません。全体を2倍しても選択比率は同じです。選ばれた種類で必ず変更案が作れるとは限らず、提案できても制約違反なら採用されません。全重み0は改善探索を止めますが、`solve` の構築・修復まで無効にする設定ではありません。

### 2.8 本体の全公開メソッド

表の `model = 標準` は、引数を省略すると組込みモデルを使う意味です。利用者が内部の既定モデル型を指定する必要はありません。独自モデルを使うときは、該当する呼出しごとに渡します。

| 宣言の形 | パラメータ・戻り値・動作 |
|---|---|
| `explicit Solver(Problem problem)` | `problem` は必須。本体が問題を所有する。lvalueを渡すとコピー、`std::move` なら移動できる。元の問題を書き換えるだけでは反映されない |
| `Evaluation evaluate(span<const int> labels, const Model& model = 標準) const` | 完全な割当の合法性と目的値を計算。非合法な割当も受け取る。探索状態や保存解を変更しない |
| `Result improve(span<const int> labels, const Options& options = {}, const Model& model = 標準)` | 完全な初期割当を取り込み、新規開始する。非合法なら `invalid_initial` と目的値なしを返す。前回の最良解と混ぜない |
| `Result solve(const Options& options = {}, const Model& model = 標準)` | 初期割当なしで構築・修復・改善する。常に新規開始。以前の最良解は自動では合流しない。`movable` は未指定でなければならない |
| `Result repair(span<const int> labels, const Options& options = {}, const Model& model = 標準)` | 完全だが非合法でもよい割当を取り込み、修復後に残予算で改善。合法解が見つからない場合もある |
| `Result repair(const Options& options = {}, const Model& model = 標準)` | 保持した候補から修復。事前に `solve`・`improve`・`repair` などで候補を取り込んでいる必要がある。乱数は `options.seed` から再開始 |
| `Result resume(const Options& options = {}, const Model& model = 標準)` | 保持中の状態から続行。候補の取り込みが必要。必要なら再評価し、出発点が非合法なら `invalid_initial`。自動修復はしない |
| `void update(Problem problem)` | 問題全体を置き換える。戻り値なし。旧目的値・合法性は失効。`n,k` が同じなら候補を保持できる |
| `span<const int> best_labels() const` | 現在の問題・直近の評価で合法解が確認されている場合だけ呼べる。長さ `n` の最良ラベルを参照する。次の変更操作まで有効 |

`labels` は必ず長さ `n`、各値は `0..k-1` です。`repair` も `-1` の穴を持つ配列は受け付けません。標準の `vector<int>` や `array<int,N>` は、条件を満たせばspanとして渡せます。戻り値にはラベル配列を含まず、成功確認後に `best_labels()` で読みます。`evaluate` だけでは保持候補は登録されないため、それだけを呼んだ状態から `resume` はできません。

`resume` は、焼きなましの途中の現在解が最良解より悪い場合、その現在解から続けることがあります。最良解から明示的に仕切り直すなら、ラベルをコピーして `improve` に渡します。`resume` でも冷却の進み具合は呼出しごとの予算から計算するため、短い呼出しを並べることと1回の長い探索は同一ではありません。

`movable` を変えて `resume` すると、集合外の頂点は前回返却された最良ラベルを基準に固定します。独自モデルを渡す `resume` は、モデルが同じに見えても保持候補を再評価します。モデルを省略して呼ぶと標準モデルに戻ります。

`update` 後は、以前の `Result` のコピーに値があっても `best_labels()` を呼べません。新しい結果の成功を確認します。`n` または `k` を変えると候補も破棄され、`resume` や引数なし割当の `repair` は使えません。`solve` または新サイズの割当付き `improve`・`repair` から始めます。同じ頂点数・辺数・順序・端点なら隣接用の内部配列を再利用できますが、問題全体を受け取るAPIであり、変更箇所だけを更新するAPIではありません。

### 2.9 結果、終了理由、統計

| 構造体・フィールド | 型・初期値 | 意味 |
|---|---|---|
| `Evaluation.feasible` | `bool = false` | 全制約を満たしたか |
| `Evaluation.cost` | `optional<Cost>`、値なし | 合法な場合だけ目的値がある |
| `Result.best_cost` | `optional<Cost>`、値なし | この呼出しで扱った問題・モデルにおける確認済み最良目的値。合法解がなければ値なし |
| `Result.reason` | `StopReason`、メンバー既定値なし | solverが返すときは必ず設定。成功判定とは別 |
| `Result.statistics` | `Statistics`、各値0から | その呼出しだけの統計。累積ではない |
| `Statistics.steps` | `int64_t = 0` | 修復を含む試行数。棄却・提案失敗も数える |
| `Statistics.construction_attempts` | `int64_t = 0` | 初期構築、追加構築、再構築の回数 |
| `Statistics.elapsed_us` | `int64_t = 0` | 呼出し全体の経過マイクロ秒。外側のコンストラクタ・`update` は含まない |
| `Statistics.neighborhood` | `array<NeighborhoodStats,5>`、全件0 | `Neighborhood` の添字順の改善統計。修復の内訳は含まない |
| `NeighborhoodStats.proposed` | `int64_t = 0` | その種類の候補を提案しようとした回数 |
| `NeighborhoodStats.feasible` | `int64_t = 0` | 合法であるところまで確認した候補数 |
| `NeighborhoodStats.accepted` | `int64_t = 0` | 採用した候補数。同点を含む |
| `NeighborhoodStats.improved` | `int64_t = 0` | 保存した最良目的値を厳密に改善した回数 |
| `NeighborhoodStats.elapsed_ns` | `int64_t = 0` | その種類の処理に使ったナノ秒。`profile=false` なら計測せず0 |

| `StopReason` の値 | 意味 |
|---|---|
| `StopReason::time_limit` | 時間上限を検知した。合法解の有無は `best_cost` で別途確認 |
| `StopReason::step_limit` | 試行数上限に達した。最適性を示すものではない |
| `StopReason::no_moves` | 可動頂点なし、改善用重みが全て0、ラベル候補が1個など、自明に探索しない状態。全近傍を調べ尽くした証明ではない |
| `StopReason::invalid_initial` | `improve`・`resume` の出発点が非合法だった |

`solve`・`repair` の失敗は `time_limit` や `step_limit` と目的値なし、などで表れます。終了理由だけから合法解の有無を決めないでください。また、標準モデルの山登りでは費用で先に棄却して合法性を調べない候補があるため、`feasible/proposed` をすべての候補の真の合法率と解釈しないでください。

### 2.10 ストリーム出力

次の4つを提供します。いずれも渡した `os` を参照で返すため、`<< '\n'` などを続けて書けます。引数に既定値はありません。

| 関数 | 表示 |
|---|---|
| `std::ostream& operator<<(std::ostream& os, const Evaluation& value)` | 合法性と目的値 |
| `std::ostream& operator<<(std::ostream& os, const Result& value)` | 最良目的値、終了理由の名前、全統計 |
| `std::ostream& operator<<(std::ostream& os, const Statistics& value)` | 全体統計と近傍別統計 |
| `std::ostream& operator<<(std::ostream& os, const NeighborhoodStats& value)` | 1種類の提案数・合法数・採用数・改善数・時間 |

目的値がなければ `none` と表示します。近傍配列は固定5件を1件ずつ改行して表示し、問題サイズに応じて出力が増えません。ラベル配列は含みません。末尾の改行は呼出し側で付けます。`std::cout`・`std::cerr` の両方で使え、ストリームのフラグや精度設定を変更しません。提出用の標準出力と混ぜたくない診断には `std::cerr` を使います。

### 2.11 独自モデルの全コールバック

モデルは利用者が定義する構造体です。継承や登録APIはありません。必要な関数だけを、表の形で定義して渡します。各引数に既定値はありません。

| 関数の形 | 引数・戻り値 | 定義した場合の効果／省略した場合 |
|---|---|---|
| `int64_t load(int vertex, int region, int dim) const` | 頂点・候補領域・負荷種類の番号から整数負荷を返す | `Problem.load` を置換／省略時は入力配列、空なら0 |
| `Cost vertex_cost(int vertex, int region) const` | 頂点と割当先の費用 | `unary_cost` を置換／省略時は入力配列、空なら0 |
| `Cost edge_cost(int edge_id, int label_u, int label_v) const` | 辺IDと保存端点順のラベルから費用 | `cut_cost` の規則を置換／省略時は異ラベルだけ `cut_cost`。有効な `PairCost` 辺だけ呼ぶ |
| `Cost region_cost(int region, const RegionStatsView& stats) const` | 領域番号とその集計から費用 | `activation_cost` と `balance_terms` の両方を置換／省略時は標準の領域費用 |
| `Cost global_cost(const SummaryView& summary) const` | 候補全体の情報から費用 | 全体費用を追加／省略時0 |
| `bool extra_feasible(const SummaryView& summary) const` | 候補全体が追加条件を満たせばtrue | 組込み制約に追加／省略時true |

頂点番号は `0..n-1`、領域番号は `0..k-1`、負荷番号は `0..load_dim-1`、辺番号は `0..edges.size()-1` です。関数は決定的で、呼出し中に参照データを書き換えず、例外を投げないようにします。モデルはsolverに保存されないので、呼出しごとに渡してください。

費用関数は合法性が確認された割当の評価に使います。`extra_feasible` は組込み条件を通った候補に呼ばれ、`load` はその前の集計に使うため、非合法な割当の修復中にも呼ばれます。全ての範囲内の引数で負荷を定義してください。

空の任意領域にも `region_cost` は呼ばれます。平均などを計算する前に `stats.vertices==0` を処理してください。`region_cost` は渡された領域の集計と固定データだけに依存させます。他領域との比較や、全ラベルを読む費用は `global_cost` に書きます。全頂点を毎候補で走査すると、その分だけ重くなります。

### 2.12 `RegionStatsView`、`ContactStats`、`SummaryView`

`RegionStatsView` と `SummaryView` は、コールバック中だけ有効な参照です。後で使うために保存しません。`SummaryView` を利用者が直接構築する公開コンストラクタはありません。

| `RegionStatsView` のフィールド | 型 | 意味 |
|---|---|---|
| `vertices` | `int` | その領域の頂点数。0以上 |
| `load` | `span<const int64_t>` | 長さ `load_dim` の負荷合計。モデルの `load` があればそれに基づく |
| `feature` | `span<const double>` | 長さ `feature_dim` の特徴合計 |
| `boundary` | `double` | その領域と他領域を結ぶ有効な `Contact` 辺の長さの和 |

これらの値はsolverが与えるため、利用者向けの既定設定はありません。境界1本の長さは両側の領域にそれぞれ入ります。全領域で合計すると2回数えるため、1回分が欲しいときは2で割ります。盤面外周は自動で追加されません。

| `ContactStats` のフィールド | 型・既定値 | 意味 |
|---|---|---|
| `edges` | `int = 0` | 登録ペアを結ぶ接触辺数 |
| `length` | `double = 0` | 同じ接触辺の長さ合計 |

| `SummaryView` のメソッド | 引数条件 | 戻り値・意味 |
|---|---|---|
| `int size() const` | 引数なし | 頂点数 `n` |
| `int region_count() const` | 引数なし | 領域番号の種類数 `k` |
| `int label(int v) const` | `0 <= v < size()` | 頂点 `v` の候補ラベル |
| `int active_regions() const` | 引数なし | 頂点が1個以上入った領域数 |
| `RegionStatsView region(int r) const` | `0 <= r < region_count()` | 領域 `r` の集計ビュー |
| `int contact_count() const` | 引数なし | `Problem.contacts` に登録したペア数 |
| `ContactStats contact(int i) const` | `0 <= i < contact_count()` | 登録順の添字 `i` の接触集計を値で返す。引数は領域番号ではない |

各メソッドの参照自体は一定時間です。未登録ペアの接触集計を読むメソッドはないため、必要なペアを `Problem.contacts` に登録します。

## 3. ユースケース

### 3.1 実行方法と読み方

各例のコードを `main.cpp` に保存し、`connected_partition_solver_v12.hpp` と同じフォルダーで次を実行します。標準入力を使う例以外は、入力データをコード内に含むため、入力ファイルは不要です。

```sh
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o example
./example
```

C++23なら `-std=c++23` にします。別途リンクするライブラリは不要ですが、ヘッダー内の `bits/stdc++.h` を使えるGCC環境が必要です。添付ZIPでは、たとえば次のように実行します。

```sh
g++ -std=c++20 -O2 -I. examples/01_minimal.cpp -o example
./example
```

出力例は実際の実行から採取した一例です。同点のラベル配列や時間・統計は変わる場合があります。`cost = ...` の次の行は、頂点番号順のラベルです。`no feasible solution found` は合法解未発見を意味します。各例の中でも成功を確認してからラベルを参照します。

1ファイル提出へまとめる場合、このヘッダーの `#if __INCLUDE_LEVEL__ == 0` 以降にある単独実行用テスト部分を除いてクラス定義を取り込みます。先頭の `#pragma once` と例側のこのヘッダーの `#include` 行も除きます。ヘッダー全体をそのまま貼ると、テスト用 `main` と提出用 `main` が重複します。通常の `#include` 利用では除去作業は不要です。

### 3.2 用途から選ぶ

| 例 | ユースケース | 主なAPI・設定 |
|---|---|---|
| 01 | 境界の費用が小さい担当区域を作る | `Problem`、`Edge`、`improve`、`Result.best_cost`、`best_labels` |
| 02 | 入力の合法性を検査し、結果をコピーして保存する | `evaluate`、`Evaluation`、`operator<<`、`Options.seed`、`best_labels` |
| 03 | 担当区域を小さすぎず、大きすぎない範囲にする | `RegionRule.min_vertices`、`RegionRule.max_vertices` |
| 04 | 各区画の希望担当を配置費用として表す | `Problem.unary_cost` |
| 05 | 区画ごとの仕事量を容量内に分散する | `Problem.load_dim`、`Problem.load`、`RegionRule.load_bounds`、`IntRange` |
| 06 | 複数資源と、正負のある収支を同時に制約する | `Problem.load`、`Problem.load_dim`、`IntRange.lower`、`IntRange.upper` |
| 07 | 容量の必須条件ではなく、負荷を目標へ近づける | `Problem.balance_terms`、`BalanceTerm` |
| 08 | 拠点のある区画を特定の担当へ固定する | `Problem.fixed` |
| 09 | 区画ごとに担当可能な候補を絞る | `Problem.domains`、`LabelDomain.vertex`、`LabelDomain.labels`、`Problem.fixed` |
| 10 | 2次元の盤面を連結なマス集合へ分ける | `Problem.n`、`Problem.k`、`Problem.edges`、`Edge`、`best_labels` |
| 11 | 接続・接触・費用の関係を別々に定義する | `Edge.cut_cost`、`Edge.length`、`Edge.enabled`、`Edge.roles`、`EdgeRole.Connect`、`EdgeRole.Contact`、`EdgeRole.PairCost` |
| 12 | 区域どうしが接する順番を制約する | `Problem.contacts`、`Problem.allow_unlisted_contacts`、`ContactRule` |
| 13 | 区域の開設費を見ながら、使う区域数も選ぶ | `RegionRule.required`、`Problem.min_active_regions`、`Problem.max_active_regions`、`Problem.activation_cost` |
| 14 | 背景だけは飛び地を許す | `RegionRule.connected`、`Problem.fixed` |
| 15 | 盤面の外側を通る接続を仮想頂点で表す | `EdgeRole.Connect`、`Problem.n`、`Problem.fixed`、`best_labels` |
| 16 | 初期解を用意せずに構築から始める | `solve`、`Options.max_steps`、`Result` |
| 17 | 完全だが制約違反のある分け方を修復する | `improve`、`repair(labels,options,model)`、`StopReason.invalid_initial` |
| 18 | 焼きなましと近傍の選択比率を設定する | `Acceptance.annealing`、`Options.temperature`、`Options.final_temperature_ratio`、`Options.weights`、`Neighborhood` |
| 19 | 短い調整や比較のために山登りへ切り替える | `Acceptance.greedy`、`Options.acceptance` |
| 20 | 時間予算を共有し、探索の診断情報を見る | `Clock`、`Options.deadline`、`Options.budget_us`、`Options.profile`、`Statistics`、`NeighborhoodStats`、`operator<<`、`NeighborhoodCount` |
| 21 | 変更できる区画を限定して、続きから改善する | `Options.movable`、`resume`、`StopReason.no_moves`、`best_labels` |
| 22 | 焼きなましの最良解を山登りで仕上げる | `best_labels`、`improve`、`Acceptance.greedy` |
| 23 | 整数の費用を丸めずに扱う | `ConnectedPartitionSolver<int64_t>` |
| 24 | 大きな配置費用表を関数で置き換える | `Model.vertex_cost`、`improve(labels,options,model)` |
| 25 | ラベルの組合せで設備間の費用を変える | `Model.edge_cost`、`EdgeRole.PairCost` |
| 26 | 領域内のばらつきと境界長を抑える | `Problem.feature_dim`、`Problem.feature`、`RegionStatsView`、`Model.region_cost` |
| 27 | 領域間の負荷差と連絡窓口をまとめて評価する | `Model.global_cost`、`SummaryView.region`、`SummaryView.contact_count`、`SummaryView.contact`、`ContactStats.edges`、`ContactStats.length` |
| 28 | 領域どうしの関係を追加の必須条件にする | `Model.extra_feasible`、`SummaryView.region` |
| 29 | 担当によって必要な作業時間が違う | `Model.load`、`RegionRule.load_bounds` |
| 30 | 評価基準の変更を受けて再開する | `resume(options,model)`、`evaluate(labels,model)`、`Model.vertex_cost` |
| 31 | 新しい固定条件と費用を反映し、保持候補を修復する | `update`、`resume`、`repair(options,model)`、`Options.deadline`、`Options.max_steps=0` |
| 32 | 複数の開始点から探索し、外側で最良解を選ぶ | `solve`、`Options.seed`、`best_labels`、`improve` |
| 33 | 標準入力のグラフをそのまま解く | `Problem`、`Problem.edges`、`evaluate`、`improve`、`ConnectedPartitionSolver<int64_t>` |
| 34 | 合法解がない結果を安全に扱う | `Evaluation.feasible`、`Evaluation.cost`、`Result.best_cost`、`LabelDomain.labels`、`solve` |
| 35 | 大きさが厳密に固定された領域を交換で改善する | `Options.weights`、`Neighborhood.Move`、`Neighborhood.Swap`、`Acceptance.greedy`、`RegionRule` |
| 36 | 離れた設備ペアと領域数の非線形費用を扱う | `SummaryView.size`、`SummaryView.label`、`SummaryView.region_count`、`SummaryView.active_regions`、`Model.global_cost` |
| 37 | 頂点数や領域数が変わる問題へ差し替える | `Problem`、`update`、`solve`、`best_labels` |

### 3.3 実行例

#### 例01 境界の費用が小さい担当区域を作る

隣り合う区画を同じ担当へまとめ、担当どうしをまたぐ負担を減らします。まず、問題・初期解・結果を扱う最小の流れを確認します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。6区画を2つの必須・連結な領域に分けます。初期ラベルは0,0,0,1,1,1で、中央の辺だけを切るため費用10です。領域の大きさをそろえる制約はまだありません。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/01_minimal.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // オプション省略時は自動温度の焼きなまし、相対予算1秒でこの初期解を改善する。
    auto result = solver.improve(initial);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 1
1 0 0 0 0 0
```

**読み方・カスタマイズ時の注意**

端の費用1の辺で分ければ費用1です。片側が1頂点でも合法です。引数を省略したimproveは自動温度の焼きなましを相対予算1秒で使います。合法解が見つかったかはbest_costの値の有無で確認します。

#### 例02 入力の合法性を検査し、結果をコピーして保存する

自作の初期解をsolverへ渡す前に検査し、探索後にも費用を確認します。次の呼出しをまたいで結果を残す方法も扱います。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。2領域の合法解0,0,0,1,1,1と、領域0が分断する0,1,0,1,1,1を比較します。合法解を2000試行だけ改善し、ラベルを保存します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/02_evaluate.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 初期ラベルの制約と目的値を調べる。評価だけで探索状態は変更しない。
    auto before = solver.evaluate(initial);

    // Evaluationをそのまま診断出力する。合法ならcostに値がある。
    std::cerr << "initial: " << before << '\n';

    // 頂点0と2が領域0なのに間の頂点1が領域1で、領域0が分断した入力。
    std::vector<int> broken{0, 1, 0, 1, 1, 1};

    // 非合法な配列もevaluateで検査できる。ただし長さとラベル番号は正しくする。
    auto bad = solver.evaluate(broken);

    // 非合法な場合はfeasible=false、cost=noneと表示される。
    std::cerr << "broken: " << bad << '\n';

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 新規開始の乱数初期値を42にする。選び抜いた推奨値ではなく、再現用の例。
    opt.seed = 42;

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。

    // 返されたラベルを全体から再評価する。best_costがある場合だけ参照する。
    auto checked = solver.evaluate(solver.best_labels());
    assert(checked.feasible && checked.cost); // 合法性と目的値の存在を確認する。

    // 差分計算による結果を再評価と照合する。doubleなので小さな丸め誤差を許す。
    assert(std::abs(*checked.cost - *result.best_cost) < 1e-9);

    // 最良ラベルへの読み取り専用spanを得る。これ自体はコピーではない。
    auto view = solver.best_labels();

    // 次の探索やupdateの後も使えるよう、ラベルの実体をvectorにコピーする。
    std::vector<int> saved(view.begin(), view.end());

    // 保存した配列の頂点数を確認する。
    std::cout << "saved vertices = " << saved.size() << '\n';
}
```

**標準出力例**

```text
cost = 1
0 0 0 0 0 1 
saved vertices = 6
```

**標準エラー出力例**

```text
initial: Evaluation{feasible=true, cost=10}
broken: Evaluation{feasible=false, cost=none}
```

**読み方・カスタマイズ時の注意**

evaluateは探索状態を変更しません。cost=noneとcost=0は別です。標準出力に最終解、標準エラー出力にEvaluationを表示します。best_labels()は参照なので、長く保存するときはvectorへコピーします。

#### 例03 担当区域を小さすぎず、大きすぎない範囲にする

境界の費用だけを減らすと、1区画だけの担当ができることがあります。区画数に下限と上限を付け、極端な偏りを防ぎます。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。2領域をそれぞれ2〜4頂点に制限します。初期解は3頂点ずつで合法です。目的値は引き続き境界の費用だけです。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/03_vertex_bounds.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& rule : p.regions) { // 領域番号ごとのルールを順に設定する。
        // 使う領域は少なくとも2頂点を持つ。required=trueなら空も許さない。
        rule.min_vertices = 2;
        rule.max_vertices = 4; // 使う領域は最大4頂点までとする。
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 0 0 0 0
```

**読み方・カスタマイズ時の注意**

個数条件は必須です。費用のために1対5へ崩すことは許しません。領域内のつながりも同時に守ります。

#### 例04 各区画の希望担当を配置費用として表す

区画ごとの担当の得意・不得意を、割り当てた先の費用として入力します。隣接をまとめたい目的との両立を考える例です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。頂点0〜2は領域0、頂点3〜5は領域1に置く費用を0とし、逆の担当へ置くと5掛かります。配置費用と切断費用の合計を最小化します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/04_unary.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 配置費用を頂点ごとに、領域0・領域1の順で並べる。配列の長さは n * k。
    p.unary_cost = {
        0, 5, // 頂点0：領域0、領域1の順。
        0, 5, // 頂点1：領域0、領域1の順。
        0, 5, // 頂点2：領域0、領域1の順。
        5, 0, // 頂点3：領域0、領域1の順。
        5, 0, // 頂点4：領域0、領域1の順。
        5, 0 // 頂点5：領域0、領域1の順。
    };

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 10
0 0 0 1 1 1
```

**読み方・カスタマイズ時の注意**

unary_cost[v*k+r]という順序です。配列はn*k個が必要です。ここでは固定ではないので、境界費用との兼ね合いで希望しない担当になることもあります。

#### 例05 区画ごとの仕事量を容量内に分散する

区画の個数が同じでも、仕事量は同じとは限りません。重い区画と軽い区画を整数負荷で表します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。頂点0〜5の仕事量は2,1,1,1,1,2です。各領域の仕事量合計を3〜5に収め、境界費用を小さくします。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/05_load.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 整数負荷を1種類使う。loadやload_boundsの添字0がその種類。
    p.load_dim = 1;

    // 頂点0〜5の負荷を順に2,1,1,1,1,2とする。各領域内で合計される。
    p.load = {2, 1, 1, 1, 1, 2};

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 両領域の負荷合計を3以上5以下にする。{{下限,上限}}は1種類分の範囲。
        rule.load_bounds = {{3, 5}};
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 0 0 0 0
```

**読み方・カスタマイズ時の注意**

負荷は頂点ごとの値、load_boundsは領域ごとの合計に対する条件です。この例では1種類なので各領域の範囲も1個です。

#### 例06 複数資源と、正負のある収支を同時に制約する

人数と物量のような複数の容量を同時に守ります。供給を正、需要を負とした収支も同じ整数負荷として表せます。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。各頂点に2種類の値を持たせます。第0負荷は2,1,1,1,1,2で、領域合計を3〜5にします。第1負荷は2,-1,-1,1,-1,0で、領域合計を-1〜1にします。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/06_multiple_loads.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 整数負荷を2種類使う。1頂点につき2値を続けて格納する。
    p.load_dim = 2;

    // 頂点ごとに2種類の負荷を並べる。添字は v * load_dim + d。負数の負荷も使える。
    p.load = {
        2, 2, // 頂点0：第0負荷、第1負荷の順。
        1, -1, // 頂点1：第0負荷、第1負荷の順。
        1, -1, // 頂点2：第0負荷、第1負荷の順。
        1, 1, // 頂点3：第0負荷、第1負荷の順。
        1, -1, // 頂点4：第0負荷、第1負荷の順。
        2, 0 // 頂点5：第0負荷、第1負荷の順。
    };

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域で第0負荷は3〜5、第1負荷は-1〜1に収める。上下限を含む。
        rule.load_bounds = {{3, 5}, {-1, 1}};
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 0 0 0 0
```

**読み方・カスタマイズ時の注意**

格納は頂点ごとに2値ずつです。負数も加算されます。負荷の種類数だけ各領域のload_boundsを用意します。

#### 例07 容量の必須条件ではなく、負荷を目標へ近づける

容量を超えたら失格にするのでなく、担当間の仕事量をできるだけ均したい場合です。目標からのずれを費用にします。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。負荷は2,1,1,1,1,2です。両領域について目標4、係数2の均衡項を加え、切断費用と合わせて最小化します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/07_balance.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 整数負荷を1種類使う。loadやload_boundsの添字0がその種類。
    p.load_dim = 1;

    // 頂点0〜5の負荷を順に2,1,1,1,1,2とする。各領域内で合計される。
    p.load = {2, 1, 1, 1, 1, 2};

    // {領域,負荷種類,目標,係数}。両領域の負荷を4へ近づけ、ずれの二乗に2を掛ける。
    p.balance_terms = {
        {0, 0, 4.0, 2.0},
        {1, 0, 4.0, 2.0}
    };

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 10
1 1 1 0 0 0
```

**読み方・カスタマイズ時の注意**

各均衡項は{領域番号,負荷番号,目標値,係数}です。負荷3でも5でも費用2が掛かります。使用していない任意領域には、標準の均衡項は掛かりません。

#### 例08 拠点のある区画を特定の担当へ固定する

既に担当が決まっている拠点を残したまま、周囲の境界を調整します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。頂点0を領域0、頂点5を領域1に固定します。残り4頂点はどちらへ動かしてもよく、両領域を連結に保って切断費用を減らします。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/08_fixed.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 全頂点を固定なしの-1で初期化する。fixedは空か長さnにする。
    p.fixed.assign(p.n, -1);
    p.fixed[0] = 0; // 頂点0のラベルを領域0に固定する。
    p.fixed[5] = 1; // 頂点5のラベルを領域1に固定する。

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 1
0 1 1 1 1 1
```

**読み方・カスタマイズ時の注意**

fixedの-1は固定なしです。解ラベルの-1とは違います。固定した頂点も目的値と連結性に含まれます。

#### 例09 区画ごとに担当可能な候補を絞る

資格や設備の違いにより、担当できる領域が区画ごとに異なる場合です。完全な固定より一般的な許可集合を使います。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。両端を0と1へ固定し、頂点1は領域0のみ、頂点2は0または1、頂点4は1のみを許可します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/09_domains.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 全頂点を固定なしの-1で初期化する。fixedは空か長さnにする。
    p.fixed.assign(p.n, -1);
    p.fixed[0] = 0; // 頂点0のラベルを領域0に固定する。
    p.fixed[5] = 1; // 頂点5のラベルを領域1に固定する。

    // 頂点ごとに許可するラベルを指定する。未登録の頂点は制限しない。
    p.domains = {
        {1, {0}}, // 頂点1は領域0だけを許可する。
        {2, {0, 1}}, // 頂点2は領域0、1を許可する。
        {4, {1}} // 頂点4は領域1だけを許可する。
    };

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
0 0 0 0 1 1
```

**読み方・カスタマイズ時の注意**

未登録の頂点には許可集合による制限がありません。空集合を登録すると、どの領域にも置けません。固定と許可集合の両方を同時に満たす必要があります。

#### 例10 2次元の盤面を連結なマス集合へ分ける

盤面や画像のマスを領域化します。マスの座標を頂点番号へ変換すれば、グラフと同じAPIで扱えます。

**解く問題**

2行3列の6マスを上下左右で結び、辺費用は全て1とします。各領域を2頂点以上の連結なマス集合にして、境界辺数を小さくします。初期解は各行の左2マスが領域0、右1マスが領域1です。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/10_grid.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;
    const int height = 2, width = 3; // 2行3列の盤面を入力データとする。

    // 各マスを1頂点にし、2領域へ分ける。n=高さ×幅、k=2。
    Solver::Problem p(height * width, 2);
    for (int y = 0; y < height; ++y) { // 盤面の行番号を順に処理する。
        for (int x = 0; x < width; ++x) { // 盤面の列番号を順に処理する。
            int v = y * width + x; // マス(y,x)を頂点番号y*width+xへ変換する。
            if (x + 1 < width) {
                // 右隣があれば1本登録する。{始点,終点,切断費用1}で、接続は無向。
                p.edges.push_back({v, v + 1, 1});
            }

            if (y + 1 < height) {
                // 下隣も1本登録する。逆向きの同じ辺を重ねて登録する必要はない。
                p.edges.push_back({v, v + width, 1});
            }
        }
    }

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域を2頂点以上にする。盤面でも領域内の連結性は既定で必要。
        rule.min_vertices = 2;
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 2行とも左2マスを領域0、右1マスを領域1とする連結な初期分割。
    std::vector<int> initial{0, 0, 1, 0, 0, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。

    // 現在の最良ラベルを参照する。次の探索やupdateをまたいで保持しない。
    auto labels = solver.best_labels();
    for (int y = 0; y < height; ++y) { // 盤面の行番号を順に処理する。
        for (int x = 0; x < width; ++x) {
            // ラベル配列を元の盤面の行・列へ戻して表示する。
            std::cout << labels[y * width + x] << ' ';
        }

        std::cout << '\n'; // 出力の行を終える。
    }
}
```

**標準出力例**

```text
cost = 2
0 0 1 0 0 1 
0 0 1 
0 0 1
```

**読み方・カスタマイズ時の注意**

頂点番号はy*width+xです。右と下だけを登録すれば無向辺を1本ずつ入力できます。出力には配列形式に加えて、盤面の行・列へ戻した表示も含めます。

#### 例11 接続・接触・費用の関係を別々に定義する

地理的には隣接していても費用はなく、離れた設備間には連絡費用だけがある、といった問題を表します。

**解く問題**

0〜5の1列の辺は接続・接触専用とし、接触長を2にします。頂点1と4の間には費用20のPairCost専用辺を置きます。頂点0と5の追加接続辺はenabled=falseで無効です。両端の領域は0と1に固定します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/11_edge_roles.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& e : p.edges) { // 既存の辺それぞれの役割と長さを設定する。
        e.cut_cost = 0; // この接続辺自体には切断費用を付けない。

        // 接触量として数える長さを2にする。lengthはcut_costとは別の値。
        e.length = 2.0;

        // 接続と接触の役割だけを残す。PairCostがないため費用計算には使わない。
        e.roles = Solver::Connect | Solver::Contact;
    }

    // 頂点1と4の別領域への分離に費用20を付ける。費用専用なので連結性には使わない。
    p.edges.push_back({1, 4, 20.0, 0.0, true, Solver::PairCost});

    // 無効な接続辺の例。enabled=falseなので0と5をつなぐ効果はない。
    p.edges.push_back({0, 5, 0.0, 0.0, false, Solver::Connect});

    // 全頂点を固定なしの-1で初期化する。fixedは空か長さnにする。
    p.fixed.assign(p.n, -1);
    p.fixed[0] = 0; // 頂点0のラベルを領域0に固定する。
    p.fixed[5] = 1; // 頂点5のラベルを領域1に固定する。

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 0
0 1 1 1 1 1
```

**読み方・カスタマイズ時の注意**

PairCostだけの辺は領域をつなぎません。cut_costとlengthも独立です。Edgeの初期化順は{u,v,cut_cost,length,enabled,roles}です。

#### 例12 区域どうしが接する順番を制約する

担当区域の隣接関係に決まりがある場合です。0と2の間には必ず区域1を挟む、といった配置を表します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。3領域を必須・連結とし、0-1と1-2の接触辺数を各1本に固定します。それ以外のペアの接触は禁止し、頂点0を領域0、頂点5を領域2へ固定します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/12_contacts.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 6頂点に対し領域番号0,1,2の3種類を用意する。既定では全て必須・連結。
    Solver::Problem p(6, 3);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // {領域a,領域b,接触辺数下限,上限}。0-1と1-2は各ちょうど1本接する。
    p.contacts = {
        {0, 1, 1, 1},
        {1, 2, 1, 1}
    };

    // 未登録ペアの接触を禁止する。この例では領域0と2の直接接触が禁止。
    p.allow_unlisted_contacts = false;

    // 両端だけを固定する。頂点0は領域0、頂点5は領域2、-1は固定なし。
    p.fixed = {0, -1, -1, -1, -1, 2};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    std::vector<int> initial{0, 0, 1, 1, 2, 2}; // 各領域へ連続する2頂点ずつを置く初期分割。

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 2
0 1 1 1 1 2
```

**読み方・カスタマイズ時の注意**

制約は接触の長さではなく辺数です。contactsは順不同のペアを1回ずつ登録します。未登録ペアを禁止するにはallow_unlisted_contacts=falseが必要です。

#### 例13 区域の開設費を見ながら、使う区域数も選ぶ

予備の担当区域を使うと固定費が掛かるため、必要なときだけ開きたい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。候補領域は3個で、各領域は空でもよく、使うなら2〜4頂点にします。使用数は2〜3個、領域2の開設費は6、ほかは0です。切断費用と開設費の合計を減らします。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/13_optional_regions.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 6頂点に対し領域番号0,1,2の3種類を用意する。既定では全て必須・連結。
    Solver::Problem p(6, 3);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& rule : p.regions) { // 領域番号ごとのルールを順に設定する。
        // この領域を使わず空にすることも許す。空のときは個数・負荷の下限を免除する。
        rule.required = false;

        // 使う領域は少なくとも2頂点を持つ。required=trueなら空も許さない。
        rule.min_vertices = 2;
        rule.max_vertices = 4; // 使う領域は最大4頂点までとする。
    }

    // 実際に頂点が入る領域を少なくとも2個にする。
    p.min_active_regions = 2;

    // 使用領域数は最大3個にする。この例の候補領域も3個。
    p.max_active_regions = 3;

    // 領域2を使う場合だけ固定費6を加える。領域0と1の固定費は0。
    p.activation_cost = {0, 0, 6};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    std::vector<int> initial{0, 0, 1, 1, 2, 2}; // 各領域へ連続する2頂点ずつを置く初期分割。

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 0 0 0 0
```

**読み方・カスタマイズ時の注意**

required=falseだけで領域の使用を任意にできます。空の領域には個数・負荷の下限と標準の開設費を掛けません。min_active_regionsは実際に使う領域数を制御します。

#### 例14 背景だけは飛び地を許す

選択した対象はひと続きにしたい一方、選ばなかった背景は分かれていてもよい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。領域0を背景とし、その連結性だけを不要にします。両端を背景0、中央の頂点2と3を対象の領域1に固定します。対象領域1は連結にします。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/14_background.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 領域0だけは飛び地を許す。領域1の連結性は既定どおり必要。
    p.regions[0].connected = false;

    // 両端を背景の領域0、中央の頂点2と3を領域1に固定する。
    p.fixed = {0, -1, 1, 1, -1, 0};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 両端に分かれた背景0と、中央の連結な領域1を表す初期割当。
    std::vector<int> initial{0, 0, 1, 1, 0, 0};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 2
0 1 1 1 1 0
```

**読み方・カスタマイズ時の注意**

connected=falseはその領域の連結性を外すだけで、個数・負荷・固定条件まで外しません。全頂点には引き続き0か1のラベルが必要です。

#### 例15 盤面の外側を通る接続を仮想頂点で表す

背景を単に飛び地可とするのでなく、盤面外側でつながっている背景として扱いたい場合です。

**解く問題**

実マスの頂点0〜5は1列で、費用は順に1,8,10,8,1です。頂点6を仮想外部として追加し、0と5へConnect専用辺で接続します。外部と両端を背景0、中央2,3を領域1に固定します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/15_outside_vertex.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 盤面の6頂点に仮想外部頂点6を加え、領域番号は0と1を使う。
    Solver::Problem p(7, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 仮想外部6と左端0を接続専用で結ぶ。費用や接触長は発生させない。
    p.edges.push_back({6, 0, 0.0, 0.0, true, Solver::Connect});

    // 仮想外部6と右端5も結び、背景が外側を通ってつながる状態を表す。
    p.edges.push_back({6, 5, 0.0, 0.0, true, Solver::Connect});

    // 外部頂点6も背景0へ固定する。中央2,3は領域1、両端は背景0。
    p.fixed = {0, -1, 1, 1, -1, 0, 0};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 背景の両端は頂点6を通じて連結になる。解配列には仮想頂点も含める。
    std::vector<int> initial{0, 0, 1, 1, 0, 0, 0};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。

    std::cout << "board: "; // 実際の盤面部分を表示するための見出し。
    for (int v = 0; v < 6; ++v) {
        // 外部頂点6を除いた0〜5だけを盤面の答えとして出力する。
        std::cout << solver.best_labels()[v] << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 2
0 1 1 1 1 0 0 
board: 0 1 1 1 1 0
```

**読み方・カスタマイズ時の注意**

仮想頂点も頂点数・使用領域数などの集計に含まれます。この例は資源制約を付けていません。実問題では外部頂点の負荷を0にするなど、目的や制約への影響を明示します。出力のboard行では外部頂点を省きます。

#### 例16 初期解を用意せずに構築から始める

利用者側で合法な分け方を作るのが難しいときに、ライブラリの構築を使います。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。2領域をそれぞれ2頂点以上にします。初期ラベルは渡さず、solveへ2000試行の設定だけを渡します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/16_solve.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域を2頂点以上にする。盤面でも領域内の連結性は既定で必要。
        rule.min_vertices = 2;
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 初期解を渡さず、構築・必要な修復・改善を行う。movableは未指定にする。
    auto result = solver.solve(opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 1 1 0 0
```

**読み方・カスタマイズ時の注意**

solveは以前の最良解を自動では引き継がず、新しい構築から始めます。movableは指定できません。強い制約下では合法解が見つからない場合もあるため、best_costを必ず確認します。

#### 例17 完全だが制約違反のある分け方を修復する

自作の初期割当や前の状況から得た割当が、連結性などの条件に違反するときに修復を試します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。各領域を2頂点以上にし、交互のラベル0,1,0,1,0,1を初期入力にします。これは両領域が分断しています。improveの拒否を確認した後、repairで修復します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/17_repair.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域を2頂点以上にする。盤面でも領域内の連結性は既定で必要。
        rule.min_vertices = 2;
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全頂点にラベルはあるが、交互に割り当てたため両領域が分断した入力。
    std::vector<int> broken{0, 1, 0, 1, 0, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // improveは非合法な初期解を修復しないことを確認する。
    auto rejected = solver.improve(broken, opt);
    assert(!rejected.best_cost); // 非合法な初期解では最良目的値が存在しない。

    // 改善の開始点が非合法だった終了理由を確認する。
    assert(rejected.reason == Solver::StopReason::invalid_initial);

    // 完全な割当から制約違反を修復し、合法になれば残予算で改善する。
    auto result = solver.repair(broken, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 0 0 0 0
```

**読み方・カスタマイズ時の注意**

repairにも全頂点の範囲内ラベルが必要です。未割当の-1は使えません。合法になれば残予算で改善します。修復中の設定は改善用のacceptanceやweightsとは別です。

#### 例18 焼きなましと近傍の選択比率を設定する

悪化を一時的に許す採用方式や、複数頂点の変更を含む候補の割合を、実際の問題に合わせて比較するときの出発点です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。各領域を2頂点以上にします。2000試行、自動温度、最終温度比0.02、近傍重み60,18,12,2,7を明示して改善します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/18_annealing.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域を2頂点以上にする。盤面でも領域内の連結性は既定で必要。
        rule.min_vertices = 2;
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法解の改善に焼きなましを使う。これは既定値だが、意図を明示している。
    opt.acceptance = Solver::Acceptance::annealing;

    // 正の悪化差分から温度を自動推定する。正の数を入れると初期温度を指定できる。
    opt.temperature = 0;

    // 終了付近の温度の割合を0.02にする。指定範囲は0より大きく1以下。
    opt.final_temperature_ratio = 0.02;

    // 1頂点移動の選択重みを60にする。重みは百分率ではない。
    opt.weights[Solver::Move] = 60;
    opt.weights[Solver::Swap] = 18; // 2頂点交換の選択重みを18にする。
    opt.weights[Solver::Block] = 12; // 連結な塊の移動・交換の選択重みを12にする。
    opt.weights[Solver::Relabel] = 2; // 領域全体のラベル変更の選択重みを2にする。

    // 2領域の分け直しの選択重みを7にする。制約の強い場合の比較候補。
    opt.weights[Solver::Recombine] = 7;

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 0 0 0 0
```

**読み方・カスタマイズ時の注意**

示した数値は既定設定の読み方を示すもので、未知の問題に最良という意味ではありません。重み0の種類は改善探索で選びません。temperatureに正値を指定すると自動推定の代わりに初期温度を与えます。

#### 例19 短い調整や比較のために山登りへ切り替える

良い初期解が既にあり、悪化を経由せずに短く整えたい場合や、温度の影響を外して比較したい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。各領域を2頂点以上にし、初期解0,0,0,1,1,1から2000試行、Acceptance::greedyで改善します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/18_greedy.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域を2頂点以上にする。盤面でも領域内の連結性は既定で必要。
        rule.min_vertices = 2;
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 山登りへ切り替える。改善・同点は採用し、悪化は採用しない。修復方針の指定ではない。
    opt.acceptance = Solver::Acceptance::greedy;

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
0 0 1 1 1 1
```

**読み方・カスタマイズ時の注意**

山登りは同点も採用します。常に最良という意味ではなく、問題と同じ総時間予算で焼きなましと比較します。次回呼出しでも使うならgreedyを持つOptionsを渡します。引数を省略すると既定の焼きなましです。

#### 例20 時間予算を共有し、探索の診断情報を見る

AHCの時間制限内で実行し、候補の生成・採用の傾向を把握したい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。2領域の初期解を改善し、相対5000マイクロ秒、絶対8000マイクロ秒後、100万試行の上限を併用します。profile=trueで近傍別時間も測ります。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/19_budget_statistics.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 探索設定を既定値で作る。採用方式は焼きなまし、seedは0。
    Solver::Options opt;

    // 呼出し開始から5000マイクロ秒を予算にする。厳密な復帰時刻の保証ではない。
    opt.budget_us = 5'000;

    // 候補試行100万回も上限にする。時間・試行数のうち先に達した条件で止まる。
    opt.max_steps = 1'000'000;

    // 今から8000マイクロ秒後の絶対締切。相対予算との早い方を使う。
    opt.deadline =
        Solver::Clock::now() + std::chrono::microseconds(8'000);

    // 近傍別のelapsed_nsも測る。計時の費用があるため通常はfalse。
    opt.profile = true;

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。

    // Resultをそのまま診断出力する。目的値・終了理由・固定5件の近傍統計を表示する。
    std::cerr << result << '\n';

    // 必要なら統計だけでも直接出力できる。末尾の改行は自分で付ける。
    std::cerr << result.statistics << '\n';

    // 1種類の統計だけを読む例。添字は近傍の列挙値。
    std::cerr << result.statistics.neighborhood[Solver::Move] << '\n';

    // 近傍は固定5種類。Countは実際の近傍の添字ではない。
    assert(result.statistics.neighborhood.size() == Solver::NeighborhoodCount);
}
```

**標準出力例**

```text
cost = 1
0 1 1 1 1 1
```

**標準エラー出力例**

```text
Result{best_cost=1, reason=time_limit, statistics=Statistics{steps=18709, construction_attempts=0, elapsed_us=5000, neighborhood={
  Move: {proposed=11300, feasible=1810, accepted=158, improved=2, elapsed_ns=1806702}
  Swap: {proposed=3411, feasible=358, accepted=358, improved=0, elapsed_ns=594008}
  Block: {proposed=2264, feasible=557, accepted=399, improved=0, elapsed_ns=597439}
  Relabel: {proposed=386, feasible=144, accepted=144, improved=0, elapsed_ns=88425}
  Recombine: {proposed=1348, feasible=751, accepted=306, improved=0, elapsed_ns=761242}
}}}
Statistics{steps=18709, construction_attempts=0, elapsed_us=5000, neighborhood={
  Move: {proposed=11300, feasible=1810, accepted=158, improved=2, elapsed_ns=1806702}
  Swap: {proposed=3411, feasible=358, accepted=358, improved=0, elapsed_ns=594008}
  Block: {proposed=2264, feasible=557, accepted=399, improved=0, elapsed_ns=597439}
  Relabel: {proposed=386, feasible=144, accepted=144, improved=0, elapsed_ns=88425}
  Recombine: {proposed=1348, feasible=751, accepted=306, improved=0, elapsed_ns=761242}
}}
{proposed=11300, feasible=1810, accepted=158, improved=2, elapsed_ns=1806702}
```

**読み方・カスタマイズ時の注意**

時間と試行数の早い条件で終了します。単位はマイクロ秒ですが厳密な復帰時刻は保証しません。標準エラー出力へ結果全体、統計だけ、Moveだけの3通りを表示します。重複表示は書き方の選択肢を示すためです。通常は必要なものだけ残してください。

#### 例21 変更できる区画を限定して、続きから改善する

ターンごとに注目する周辺だけを修正し、ほかの区域を前回の結果に保ちたい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。両端を領域0と1へ固定します。最初は内側1,2,3,4を動かし、次は2,3だけ、次は空集合で全て凍結し、最後は可動集合の指定を解除します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/20_partial_resume.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 頂点0は領域0、頂点5は領域1に固定し、内側だけを調整できるようにする。
    p.fixed = {0, -1, -1, -1, -1, 1};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 今回動かしてよい頂点番号を並べる。ラベル番号の集合ではない。
    std::vector<int> focus{1, 2, 3, 4};

    // 配列への参照を渡す。focusを呼出し終了まで生存させ、再代入後はspanも設定し直す。
    opt.movable = std::span<const int>(focus);

    // 初期ラベルから最初の探索を行い、その結果を保持する。
    auto first = solver.improve(initial, opt);

    // 成功を確認してから最良ラベルを読む。失敗時はこの例を終了する。
    if (!first.best_cost) {
        return 1;
    }

    auto first_view = solver.best_labels(); // 最初の探索の最良ラベルを参照する。

    // 次のresumeで参照が変わる前に、比較用のラベルをコピーする。
    std::vector<int> saved(first_view.begin(), first_view.end());

    // 次の呼出しでは頂点2と3だけを動かす。配列の再代入後はspanも設定し直す。
    focus = {2, 3};

    // 配列への参照を渡す。focusを呼出し終了まで生存させ、再代入後はspanも設定し直す。
    opt.movable = std::span<const int>(focus);

    // 保持状態から続行する。可動集合の外は前回の最良ラベルを基準に固定される。
    auto second = solver.resume(opt);

    if (!second.best_cost) { // 続行後も合法解の存在を確認する。
        return 1;
    }

    for (int v : {0, 1, 4, 5}) {
        // 今回の可動集合の外にある頂点が、前回の最良ラベルから変わっていないことを確認する。
        assert(solver.best_labels()[v] == saved[v]);
    }

    // 明示的な空集合により全頂点を凍結する。未指定とは意味が違う。
    opt.movable = std::span<const int>{};

    // 全頂点を凍結して再開するため、改善の移動は行わない。
    auto frozen = solver.resume(opt);

    // 合法解は保持され、終了理由がno_movesになることを確認する。
    assert(frozen.best_cost && frozen.reason == Solver::StopReason::no_moves);

    // 可動集合の指定を解除する。以後は固定条件を除く全頂点を動かせる。
    opt.movable.reset();

    // 保持状態から指定の予算で続行する。seedの設定値で乱数を再初期化はしない。
    auto result = solver.resume(opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 1
0 0 0 0 0 1
```

**読み方・カスタマイズ時の注意**

movableはspanで配列を所有しません。配列を再代入したらspanも設定し直します。空集合と未指定は異なります。部分探索でもグラフ全体の接続を見ます。

#### 例22 焼きなましの最良解を山登りで仕上げる

広く探索した後、保存した最良解だけを出発点に、悪化を許さない追加調整を行います。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。両端を0と1へ固定し、2000試行の焼きなましを行います。その最良ラベルをコピーし、500試行の山登りで仕上げます。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/20_polish_greedy.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 頂点0は領域0、頂点5は領域1に固定し、内側だけを調整できるようにする。
    p.fixed = {0, -1, -1, -1, -1, 1};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 初期ラベルから最初の探索を行い、その結果を保持する。
    auto first = solver.improve(initial, opt);

    // 成功を確認してから最良ラベルを読む。失敗時はこの例を終了する。
    if (!first.best_cost) {
        return 1;
    }

    // 最良ラベルへの読み取り専用spanを得る。これ自体はコピーではない。
    auto view = solver.best_labels();

    // 最良ラベルの実体を保存する。焼きなましの現在解とは区別する。
    std::vector<int> best(view.begin(), view.end());

    // 予算などを引き継ぎ、仕上げ用の設定を別に作る。
    Solver::Options finish = opt;

    // 仕上げだけは悪化を受け入れない山登りにする。
    finish.acceptance = Solver::Acceptance::greedy;
    finish.max_steps = 500; // 仕上げに最大500候補の試行を使う。

    // 保存した最良解を開始点として仕切り直す。resumeなら内部の現在解からの続行になる。
    auto result = solver.improve(best, finish);

    // 同じ目的のもとで、仕上げ後の最良値が悪化していないことを確認する。
    assert(result.best_cost && *result.best_cost <= *first.best_cost);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 1
0 0 0 0 0 1
```

**読み方・カスタマイズ時の注意**

resumeは内部の現在解から続くため、最良解から仕切り直したいこの例ではimproveを使います。同じ目的関数なので最良値が悪化しないことをassertで確認します。追加した500試行も総予算に含めます。

#### 例23 整数の費用を丸めずに扱う

目的値が整数で、同点かどうかを厳密に比較したい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。2領域へ分ける基本問題を、ConnectedPartitionSolver<int64_t>で解きます。入力の切断費用と合計を整数として扱います。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/21_integer_cost.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値をint64_tで扱う。合計・差分・二乗などの中間演算も範囲内に収める。
    using Solver = ConnectedPartitionSolver<int64_t>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 合法な初期ラベルを取り込み、指定した予算と方式で新しく改善を始める。
    auto result = solver.improve(initial, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 1
0 0 0 0 0 1
```

**読み方・カスタマイズ時の注意**

Costを変えてもlengthやfeatureはdouble、負荷はint64_tのままです。整数のオーバーフローを自動回避するAPIではないため、差分・二乗・係数との積も含めて範囲内に収めます。

#### 例24 大きな配置費用表を関数で置き換える

全頂点×全領域の費用表を持つ代わりに、希望や距離の規則からその場で配置費用を計算したい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。前半3頂点の希望は領域0、後半3頂点の希望は領域1です。希望先なら0、それ以外なら5を返すvertex_costを定義します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/22_vertex_model.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 必要なコールバックだけを持つ独自モデル。継承や登録操作は不要。
    struct Model {
        double penalty = 5; // 希望しない領域へ置くときの費用を5にする。

        // 頂点と候補領域から配置費用を返す。定義するとunary_costの標準項を置き換える。
        double vertex_cost(int vertex, int region) const {
            // 頂点0〜2の希望先は領域0、頂点3以降は領域1。
            int preferred = vertex < 3 ? 0 : 1;

            // 希望先なら0、そうでなければ指定したペナルティを返す。
            return region == preferred ? 0.0 : penalty;
        }
    };

    // モデルの実体を作る。参照する値は1回のsolver呼出し中には変更しない。
    Model model;

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 独自モデルを明示して改善する。以後の評価・再開にも同じ意図のモデルを渡す。
    auto result = solver.improve(initial, opt, model);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 10
0 0 0 1 1 1
```

**読み方・カスタマイズ時の注意**

vertex_costを定義するとunary_costは加算されず置き換わります。関数は同じ引数に同じ値を返し、呼出し中のデータを変えないようにします。標準の辺費用は残ります。

#### 例25 ラベルの組合せで設備間の費用を変える

同じ境界でも、どちらの端をどの担当が受け持つかで費用が変わる場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。異なるラベルを結ぶ辺では、保存したu側のラベルが0なら重みの1倍、1なら2倍を費用にします。同ラベルなら0です。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/23_edge_model.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 必要なコールバックだけを持つ独自モデル。継承や登録操作は不要。
    struct Model {
        // 辺ID順に費用の重みを保存する。辺の順序を変えたら対応も合わせる。
        std::vector<double> weights;

        // PairCost辺の独自費用。label_uは保存したu側、label_vはv側のラベル。
        double edge_cost(int edge_id, int label_u, int label_v) const {
            if (label_u == label_v) {
                // このモデルでは同じ領域なら辺費用を0にする。
                return 0.0;
            }

            // 異なる領域ならu側のラベルに応じて重みを1倍または2倍する。接続自体は無向。
            return weights[edge_id] * (label_u == 0 ? 1.0 : 2.0);
        }
    };

    // 入力edgesと同じ順序でモデルの辺重みを設定する。
    Model model{{1, 8, 10, 8, 1}};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 独自モデルを明示して改善する。以後の評価・再開にも同じ意図のモデルを渡す。
    auto result = solver.improve(initial, opt, model);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 1
0 1 1 1 1 1
```

**読み方・カスタマイズ時の注意**

edge_costはcut_costの標準規則を置き換えます。u,vの保存順を意味のあるものとしてそろえます。連結性は無向のままです。辺順をupdateで変えるときは、モデル側の重みの順序も合わせます。

#### 例26 領域内のばらつきと境界長を抑える

座標や特徴が近い区画をまとめたい場合です。全頂点を毎回読み直す代わりに、特徴の和から費用を計算します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。座標を頂点番号0〜5とし、特徴に座標と座標の二乗を持たせます。標準の切断費用に加え、各領域の平均との差の二乗和と、境界長の合計を費用にします。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/24_region_features.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 実数特徴を2種類使う。設定だけで費用が加わるわけではない。
    p.feature_dim = 2;

    // 頂点番号順に、その頂点が持つ特徴を用意する。
    for (int v = 0; v < p.n; ++v) {
        // この例では頂点番号を1列上の座標として使う。
        double x = v;

        // 特徴0に座標を入れる。領域内では座標の和として参照できる。
        p.feature.push_back(x);

        // 特徴1に座標の二乗を入れる。格納順は頂点ごとに特徴0,1。
        p.feature.push_back(x * x);
    }

    // 必要なコールバックだけを持つ独自モデル。継承や登録操作は不要。
    struct Model {
        double spread_weight = 1.0; // 座標のばらつきに掛ける係数を1にする。

        // 両領域で二重に数える境界長を、合計1回分にする係数0.5。
        double boundary_weight = 0.5;

        // 領域集計から費用を返す。標準のactivation_costとbalance_termsは置き換わる。
        double region_cost(int, const Solver::RegionStatsView& s) const {
            // 空の任意領域にも呼ばれ得るため、頂点数で割る前に空を処理する。
            if (s.vertices == 0) {
                return 0.0;
            }

            // 領域内の座標合計を読む。featureのspanはコールバック中だけ有効。
            double sum = s.feature[0];
            double sum_sq = s.feature[1]; // 領域内の座標の二乗和を読む。

            // 平均との差の二乗和を求める。丸め誤差でごく小さい負値になる場合を0へ丸める。
            double spread =
                std::max(0.0, sum_sq - sum * sum / s.vertices);

            // この領域の広がりと境界長の費用を返す。標準の辺費用は別に残る。
            return spread_weight * spread
                + boundary_weight * s.boundary;
        }
    };

    // モデルの実体を作る。参照する値は1回のsolver呼出し中には変更しない。
    Model model;

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 独自モデルを明示して改善する。以後の評価・再開にも同じ意図のモデルを渡す。
    auto result = solver.improve(initial, opt, model);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 12
1 0 0 0 0 0
```

**読み方・カスタマイズ時の注意**

n個の座標の和をS、二乗の和をQとすると、平均との差の二乗和はQ-S*S/nです。平均がS/nなので、各点の差の二乗を展開するとこの形になります。空領域では割り算しません。boundaryは両領域に入るため係数0.5で全体1回分にします。region_costを定義すると標準の使用費・均衡項は置き換わります。

#### 例27 領域間の負荷差と連絡窓口をまとめて評価する

自分の区域だけではなく、複数区域の負荷差や、区域間の連絡費用を見たい場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。負荷は2,1,1,1,1,2です。標準の切断費用に、両領域の負荷差の二乗の2倍、登録ペア0-1の接触辺1本当たり2、接触長1当たり0.5を加えます。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/25_global_model.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 整数負荷を1種類使う。loadやload_boundsの添字0がその種類。
    p.load_dim = 1;

    // 頂点0〜5の負荷を順に2,1,1,1,1,2とする。各領域内で合計される。
    p.load = {2, 1, 1, 1, 1, 2};

    // 領域0-1の接触集計を登録する。省略した範囲は0〜INT_MAXで、接触必須ではない。
    p.contacts = {
        {0, 1}
    };

    // 必要なコールバックだけを持つ独自モデル。継承や登録操作は不要。
    struct Model {
        double weight = 2.0; // 2領域の負荷差の二乗に掛ける係数を2にする。
        double contact_weight = 0.5; // 登録した接触長に掛ける係数を0.5にする。

        // 候補全体の情報を使う費用を定義する。標準の頂点・辺・領域費用に加わる。
        double global_cost(const Solver::SummaryView& s) const {
            // 各領域の負荷合計を取り出し、doubleへ変換してから差を取る。
            double difference =
                double(s.region(0).load[0]) - double(s.region(1).load[0]);
            double contact_cost = 0; // 登録したペアごとの連絡窓口費用を合計する。

            // 登録ペア数だけ走査する。iは領域番号ではない。
            for (int i = 0; i < s.contact_count(); ++i) {
                auto c = s.contact(i); // contactsの登録順の接触集計を値で受け取る。

                // 接触辺1本につき2、長さ1につき0.5を加える。
                contact_cost += 2.0 * c.edges + contact_weight * c.length;
            }

            // 負荷差の二乗と、全登録ペアの接触費用を加える。
            return weight * difference * difference + contact_cost;
        }
    };

    // モデルの実体を作る。参照する値は1回のsolver呼出し中には変更しない。
    Model model;

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 独自モデルを明示して改善する。以後の評価・再開にも同じ意図のモデルを渡す。
    auto result = solver.improve(initial, opt, model);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 12.5
1 1 1 0 0 0
```

**読み方・カスタマイズ時の注意**

contact(i)のiはcontactsへの登録番号です。接触辺の本数edgesと長さlengthは別です。contact_count()の全登録ペアを走査します。この1列の例では連絡窓口は1か所ですが、盤面などでは分割で本数が変わります。

#### 例28 領域どうしの関係を追加の必須条件にする

特定の2担当の人数差のように、複数領域の状態を比較する条件を、費用ではなく必須条件として扱います。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。3つの必須・連結な領域へ分け、領域0と1の頂点数の差を1以下にします。領域2の個数は変わってよく、初期解は2頂点ずつです。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/26_extra_constraint.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜2の問題。3領域とも必須・連結。
    Solver::Problem p(6, 3);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 必要なコールバックだけを持つ独自モデル。継承や登録操作は不要。
    struct Model {
        // 領域0と1の頂点数の差を1以下にする。領域2の個数は別に動ける。
        int max_gap = 1;

        // 追加の必須条件を定義する。組込み制約を通った候補についてboolを返す。
        bool extra_feasible(const Solver::SummaryView& s) const {
            // 候補の領域0と1の頂点数の差を計算する。
            int difference = s.region(0).vertices - s.region(1).vertices;

            // 差の絶対値が上限以下なら合法。費用へのペナルティではなく必須条件。
            return std::abs(difference) <= max_gap;
        }
    };

    // モデルの実体を作る。参照する値は1回のsolver呼出し中には変更しない。
    Model model;

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全3領域を2頂点ずつにする合法な開始点。個数差は0。
    std::vector<int> initial{0, 0, 1, 1, 2, 2};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 独自モデルを明示して改善する。以後の評価・再開にも同じ意図のモデルを渡す。
    auto result = solver.improve(initial, opt, model);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 2
1 2 2 2 2 0
```

**読み方・カスタマイズ時の注意**

extra_feasibleは組込み制約を通った候補にtrueまたはfalseを返します。この条件を保つ専用の候補生成が自動で作られるわけではありません。組込み条件で同じ意味を表せる場合は、そちらを先に検討します。

#### 例29 担当によって必要な作業時間が違う

同じ区画でも、得意な担当なら短時間で済むような、割当先に依存する容量を表します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。前半は領域0、後半は領域1が得意です。得意なら負荷1、それ以外なら2とし、各領域の合計を0〜4にします。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/27_label_dependent_load.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 整数負荷を1種類使う。loadやload_boundsの添字0がその種類。
    p.load_dim = 1;

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域の第0負荷を0〜4に制限する。実際の負荷はモデルのloadで計算する。
        rule.load_bounds = {{0, 4}};
    }

    // 必要なコールバックだけを持つ独自モデル。継承や登録操作は不要。
    struct Model {
        // 頂点・割当先・負荷種類からint64_tの負荷を返す。Problem.loadを置き換える。
        int64_t load(int vertex, int region, int dim) const {
            // この例の負荷種類は0だけ。loadは非合法な割当の修復中にも呼ばれる。
            assert(dim == 0);

            // 頂点0〜2の希望先は領域0、頂点3以降は領域1。
            int preferred = vertex < 3 ? 0 : 1;

            // 得意な担当なら作業量1、そうでなければ2とする。
            return region == preferred ? 1 : 2;
        }
    };

    // モデルの実体を作る。参照する値は1回のsolver呼出し中には変更しない。
    Model model;

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 独自モデルを明示して改善する。以後の評価・再開にも同じ意図のモデルを渡す。
    auto result = solver.improve(initial, opt, model);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 10
0 0 0 1 1 1
```

**読み方・カスタマイズ時の注意**

load(v,r,d)の戻り値は目的値型ではなくint64_tです。Problem.loadは置き換わります。loadは制約を調べる材料なので、非合法な割当の修復中にも呼ばれます。

#### 例30 評価基準の変更を受けて再開する

ターンや状況により、希望違反の重みなど、利用者の評価モデルだけが変わる場合です。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。vertex_costの希望違反費用を5として一度改善し、20へ変えてresumeします。その後、モデルを省略した呼出しで標準の目的へ戻す例も示します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/28_model_resume.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 必要なコールバックだけを持つ独自モデル。継承や登録操作は不要。
    struct Model {
        double penalty = 5; // 希望しない領域へ置くときの費用を5にする。

        // 頂点と候補領域から配置費用を返す。定義するとunary_costの標準項を置き換える。
        double vertex_cost(int vertex, int region) const {
            // 頂点0〜2の希望先は領域0、頂点3以降は領域1。
            int preferred = vertex < 3 ? 0 : 1;

            // 希望先なら0、そうでなければ指定したペナルティを返す。
            return region == preferred ? 0.0 : penalty;
        }
    };

    // モデルの実体を作る。参照する値は1回のsolver呼出し中には変更しない。
    Model model;

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 独自モデルの現在の費用で探索し、状態を保持する。
    auto first = solver.improve(initial, opt, model);

    // 成功を確認してから最良ラベルを読む。失敗時はこの例を終了する。
    if (!first.best_cost) {
        return 1;
    }

    // 探索の外で希望違反費用を20へ変える。呼出し中には変更しない。
    model.penalty = 20;

    // モデルを渡して再開し、保持候補を新しい基準で再評価してから改善する。
    auto result = solver.resume(opt, model);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。

    // 同じモデルを渡して再評価する。省略すると標準の目的へ戻ってしまう。
    auto checked = solver.evaluate(solver.best_labels(), model);
    assert(checked.feasible && checked.cost); // 合法性と目的値の存在を確認する。

    // 差分計算による結果を再評価と照合する。doubleなので小さな丸め誤差を許す。
    assert(std::abs(*checked.cost - *result.best_cost) < 1e-9);

    // ここでは意図的にモデルを省略し、標準の目的へ切り替えて再開する。
    auto standard = solver.resume(opt);

    // 標準モデルへ戻した後も合法解の存在を確認する。
    if (!standard.best_cost) {
        return 1;
    }

    // 異なる目的の値なので、変更前の費用と単純な改善率比較をしない。
    std::cout << "standard cost = " << *standard.best_cost << '\n';
}
```

**標準出力例**

```text
cost = 10
0 0 0 1 1 1 
standard cost = 1
```

**読み方・カスタマイズ時の注意**

モデルのデータは呼出しの外で変えます。独自モデルを渡すresumeでは保持候補を再評価するため、問題グラフを変えないならupdateは不要です。モデルを渡し忘れると標準の目的へ戻るので注意します。異なる目的の数値は、そのまま優劣比較しません。

#### 例31 新しい固定条件と費用を反映し、保持候補を修復する

前のターンの分割を使いながら、届いた変更に追従する例です。外部との通信はせず、1ターンの更新をコード内で再現します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。両端を固定して改善した後、境界に隣接する1頂点を反対の担当に固定し、辺2-3の費用を3へ変えます。update後、resumeの拒否を確認し、引数に割当を渡さないrepairで修復します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/29_turn_update.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    // 頂点0は領域0、頂点5は領域1に固定し、内側だけを調整できるようにする。
    p.fixed = {0, -1, -1, -1, -1, 1};

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 全6頂点の初期ラベル。前半を領域0、後半を領域1にする合法な分割。
    std::vector<int> initial{0, 0, 0, 1, 1, 1};

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 初期ラベルから最初の探索を行い、その結果を保持する。
    auto first = solver.improve(initial, opt);

    // 成功を確認してから最良ラベルを読む。失敗時はこの例を終了する。
    if (!first.best_cost) {
        return 1;
    }

    // 現在の最良ラベルを参照する。次の探索やupdateをまたいで保持しない。
    auto labels = solver.best_labels();

    // 更新後の代替解作成に使うため、現在のラベルを独立した配列へコピーする。
    std::vector<int> saved(labels.begin(), labels.end());

    // 1列でラベルが切り替わる辺の左端を探す準備。
    int cut = 0;
    while (saved[cut] == saved[cut + 1]) {
        // 両端固定かつ各領域連結なので境界は1か所。初期条件を変える際はこの仮定も見直す。
        ++cut;
    }

    // 現在の境界に隣接し、両端ではない頂点を1個選ぶ。
    int change_vertex = cut == 0 ? 1 : cut;
    int new_region = cut == 0 ? 0 : 1; // 境界を1頂点ずらす方向のラベルを指定する。

    // 前の解が新しい固定条件に違反するよう入力を変更する。
    p.fixed[change_vertex] = new_region;

    // 辺ID2、すなわち頂点2-3の切断費用を3に変更する。
    p.edges[2].cut_cost = 3;

    // 相対時間予算を無効にする。この例ではmax_stepsを必ず有効にする。
    opt.budget_us = -1;

    // 各呼出しの候補試行上限を10万回にする。呼出し間で自動合算される上限ではない。
    opt.max_steps = 100'000;

    // 更新と再開・修復で共通に使う絶対締切を、今から2万マイクロ秒後にする。
    opt.deadline =
        Solver::Clock::now() + std::chrono::microseconds(20'000);

    // 変更した問題全体を反映する。直後のbest_labels()参照は禁止で、再評価が必要。
    solver.update(p);

    // 保持候補を新条件で再評価する。resumeは非合法な候補を自動修復しない。
    auto continued = solver.resume(opt);

    // 新しい固定条件により、古い候補の目的値が無効になったことを確認する。
    assert(!continued.best_cost);

    // 続行の出発点が非合法だった理由を確認する。
    assert(continued.reason == Solver::StopReason::invalid_initial);

    // 割当を省略するrepairは、内部に保持した候補を修復してから改善する。
    auto result = solver.repair(opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 修復が間に合わない場合、この1列問題専用の代替割当を用意する。
        std::vector<int> fallback(p.n, 1);
        for (int v = 0; v <= cut; ++v) {
            fallback[v] = 0; // 元の境界までを領域0、残りを領域1にする。
        }

        // 変更した固定頂点を、新しい固定先へ合わせて境界をずらす。
        fallback[change_vertex] = new_region;

        // 代替解が新しい問題の制約を満たすか検査する。この処理にも時間は必要。
        auto valid = solver.evaluate(fallback);

        // 代替解まで非合法なら、この例では続けられない。
        if (!valid.feasible) {
            return 1;
        }

        // 探索を行わず、代替解を取り込むための設定を作る。
        Solver::Options check_only;

        // 相対時間上限を外し、次のmax_steps=0だけを有効にする。
        check_only.budget_us = -1;

        // 候補探索は0回にする。ただし入力の検査・採点は実行する。
        check_only.max_steps = 0;

        // 検査済みの代替解をsolverへ取り込み、目的値とラベルを登録する。
        result = solver.improve(fallback, check_only);
    }

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。

    // 返された解が新しい固定条件を守ることを確認する。
    assert(solver.best_labels()[change_vertex] == new_region);

    // 最終ラベル全体の制約を再確認する。
    assert(solver.evaluate(solver.best_labels()).feasible);
}
```

**標準出力例**

```text
cost = 1
0 0 0 0 0 1
```

**読み方・カスタマイズ時の注意**

updateで以前の合法性と目的値は失効します。新しい結果の成功前にbest_labels()を使いません。deadlineを共有し、更新・再開・修復のたびに相対予算を取り直すことを避けます。この1列の入力専用の代替解も用意します。代替解の作成・検査に必要な余裕も別途確保します。

#### 例32 複数の開始点から探索し、外側で最良解を選ぶ

初期配置によって到達先が変わりやすい場合に、独立した新規開始を複数回試します。

**解く問題**

頂点0〜5をこの順で1列につなぎ、辺の切断費用を順に1,8,10,8,1とします。各領域2頂点以上の問題に対し、seedを1,2,3に変え、それぞれ2000試行のsolveを行います。外側に全実行の最良解を保存し、さらに2000試行改善します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/30_multistart.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 同じフォルダーに置いたライブラリを読み込む。
#include <iostream> // 標準出力coutと標準エラー出力cerrを使う。
#include <vector> // 入力ラベルや保存用配列にvectorを使う。

// このファイルだけで実行できるプログラムの入口。
int main() {
    // 目的値を小数で扱うsolver型に短い名前を付ける。
    using Solver = ConnectedPartitionSolver<double>;

    // 頂点番号0〜5、領域番号0〜1の問題を作る。両領域は既定で必須・連結。
    Solver::Problem p(6, 2);

    // 無向辺を {端点u, 端点v, 切断費用} で指定し、0〜5を1列につなぐ。省略した長さは1で、有効。
    // 接続・接触・費用の全役割を持つ。
    p.edges = {
        {0, 1, 1},
        {1, 2, 8},
        {2, 3, 10},
        {3, 4, 8},
        {4, 5, 1}
    };

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各領域を2頂点以上にする。盤面でも領域内の連結性は既定で必要。
        rule.min_vertices = 2;
    }

    // 問題をコピーして所有するsolverを作る。以後pを変えるだけでは反映されない。
    Solver solver(p);

    // 探索設定を作る。既定の採用方式は焼きなまし、seedは0。
    // この例は時間予算を外し、修復を含む候補試行数で止める。
    // 試行数の指定は、最適解を保証するものではない。
    Solver::Options opt;
    opt.budget_us = -1; // 相対時間予算を無効にする。
    opt.max_steps = 2'000; // 候補試行を最大2000回にする。

    // 複数回の実行を通した最良値を、solverの外側に保存する。
    std::optional<double> best_cost;
    std::vector<int> best; // 複数回の実行を通した最良ラベルの保存先。

    // 新規開始のseedを1,2,3へ変える。回数を増やすと総予算も増える点に注意。
    for (uint64_t seed : {1ULL, 2ULL, 3ULL}) {
        opt.seed = seed; // 次のsolveを別の乱数初期値で開始する。

        // solveは前回の最良解を自動で残さず、新しい構築から始める。
        auto trial = solver.solve(opt);

        // 合法解が得られ、外側の最良値より良いときだけ保存する。
        if (trial.best_cost &&
            (!best_cost || *trial.best_cost < *best_cost)) {
            // この実行の最良値を、全実行の最良値として保存する。
            best_cost = trial.best_cost;

            // 最良ラベルへの読み取り専用spanを得る。これ自体はコピーではない。
            auto view = solver.best_labels();

            // 次のsolveで内部配列が変わる前に、最良ラベルを実体コピーする。
            best.assign(view.begin(), view.end());
        }
    }

    if (!best_cost) {
        // どの開始点でも合法解を得られなかった場合を処理する。
        return 1;
    }

    // 最後の実行の状態ではなく、外側で選んだ最良ラベルから追加で改善する。
    auto result = solver.improve(best, opt);

    // 目的値の有無で合法解の発見を確認する。値0も成功なので値そのものとは区別する。
    if (!result.best_cost) {
        // 合法解未発見を表示する。解が存在しないことの証明ではない。
        std::cout << "no feasible solution found\n";
        return 1; // この例の処理を続けられないため、終了コード1で終了する。
    }

    // 値があることを確認してから最良目的値を取り出す。小さいほど良い。
    std::cout << "cost = " << *result.best_cost << '\n';

    // 最良ラベルを頂点番号順に出力する。best_labels()の参照は次の変更操作まで有効。
    for (int r : solver.best_labels()) {
        std::cout << r << ' ';
    }
    std::cout << '\n'; // 出力の行を終える。
}
```

**標準出力例**

```text
cost = 8
1 1 1 1 0 0
```

**読み方・カスタマイズ時の注意**

各solveは以前の最良解を自動で保持しないため、値とラベルの実体を外へ保存します。resumeの繰返しは独立した開始ではありません。比較は構築やコピーも含む同じ総時間予算で行い、回数を増やせば必ず良くなるとは考えません。

#### 例33 標準入力のグラフをそのまま解く

コンテストの入力形式へ組み込むための、読み込みから出力までの雛形です。

**解く問題**

1行目に頂点数n、辺数m、領域数kを読み、続くm行に端点u,vと整数切断費用を読みます。最後にn個の初期ラベルを読みます。全領域を必須・連結とする条件で改善します。

**入力**

次の内容を標準入力へ渡します。付属ファイルは `inputs/31_stdin.txt` です。

```text
6 5 2
0 1 1
1 2 8
2 3 10
3 4 8
4 5 1
0 0 0 1 1 1
```

```sh
g++ -std=c++20 -O2 -I. examples/31_stdin.cpp -o example
./example < inputs/31_stdin.txt
```

**コード：`examples/31_stdin.cpp`**

```cpp
// 同じフォルダー、または-Iで指定した場所のライブラリを読み込む。
#include "connected_partition_solver_v12.hpp"
#include <iostream> // 標準入力と標準出力を使う。
#include <vector> // 完全な初期ラベルを保存する。

int main() {
    // 整数の目的値を厳密に扱う。中間の合計・差分もint64_t内に収める。
    using Solver = ConnectedPartitionSolver<int64_t>;
    int n, m, k; // 入力する頂点数・辺数・領域番号の種類数。

    if (!(std::cin >> n >> m >> k)) {
        return 0; // 入力がなければ終了する。
    }

    // n>=0,k>0。各領域は既定で必須・連結。入力はこの形式条件を満たすものとする。
    Solver::Problem p(n, k);

    // 無向辺を入力順に登録し、その順序を辺IDにする。
    for (int i = 0; i < m; ++i) {
        int u, v; // 端点は0〜n-1で異なる2頂点。
        int64_t cost; // costは切断費用。
        std::cin >> u >> v >> cost; // 1行にu,v,costの3値を入力する。

        // 長さ1、有効、接続・接触・費用の全役割は既定値を使う。
        p.edges.push_back({u, v, cost});
    }

    // 全n頂点のラベルを入れる。未割当の-1は使えない。
    std::vector<int> initial(n);
    for (int& r : initial) {
        std::cin >> r; // 頂点番号順に、0〜k-1の領域番号を読む。
    }

    Solver solver(p); // 入力問題のコピーを所有するsolverを作る。

    // 探索前に、入力ラベルが制約を満たすか確認する。
    auto checked = solver.evaluate(initial);

    if (!checked.feasible) { // improveは非合法な初期解を自動修復しない。
        // 必要なら利用者側でrepairへ切り替える。
        std::cout << "initial assignment is infeasible\n";
        return 0;
    }

    Solver::Options opt; // 既定は自動温度の焼きなまし法。

    // この例では時間ではなく、試行数を上限にする。
    opt.budget_us = -1;
    opt.max_steps = 2'000; // 候補を最大2000回試す。

    auto result = solver.improve(initial, opt); // 入力の合法解から改善を始める。

    // 成功確認をしてから目的値や最良ラベルを使う。
    if (!result.best_cost) {
        std::cout << "no feasible solution found\n";
        return 1;
    }

    std::cout << "cost = " << *result.best_cost << '\n'; // 最良目的値を表示する。
    for (int r : solver.best_labels()) { // 最良ラベルを頂点番号順に表示する。
        std::cout << r << ' ';
    }
    std::cout << '\n';
}
```

**標準出力例**

```text
cost = 1
0 0 0 0 0 1
```

**読み方・カスタマイズ時の注意**

提示する入力例は6頂点・5辺・2領域です。端点とラベルは0始まりです。長さ・IDなどの形式は正しい入力を前提とし、初期解の制約違反はevaluateで検出します。

#### 例34 合法解がない結果を安全に扱う

利用者の入力条件が矛盾している場合や、予算内に合法解を見つけられない場合の分岐を確認します。

**解く問題**

2頂点を1本の辺で結び、領域0へ割り当てます。ただし頂点0の許可集合を明示的に空にします。配列長や番号は正しくても、この条件では割当先がありません。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/32_no_feasible.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 結果構造体のストリーム出力も含むライブラリ。
#include <iostream> // 診断結果を出力する。
#include <vector> // 検査用の完全なラベル配列を持つ。

int main() {
    using Solver = ConnectedPartitionSolver<double>; // 目的値型はdouble。

    // 2頂点を、必須かつ連結な領域0へ割り当てる問題。
    Solver::Problem p(2, 1);
    p.edges = { // 頂点0と1は接続されている。
        {0, 1, 1}
    };

    // 頂点0の許可先を空にする。未登録とは異なり、この頂点を割り当てられない。
    p.domains = {
        {0, {}}
    };

    // 形式として正しい問題なので構築できる。解の存在までは保証しない。
    Solver solver(p);

    // 各ラベル番号は範囲内だが、頂点0の許可条件に違反する。
    std::vector<int> initial{0, 0};

    auto checked = solver.evaluate(initial); // 非合法な割当を検査できる。
    std::cout << checked << '\n'; // feasible=false、cost=noneを表示する。

    Solver::Options opt; // 探索設定を用意する。
    opt.budget_us = -1; // 試行数だけで停止させる。

    // この例は100試行までで合法解の発見を試みる。
    opt.max_steps = 100;

    // 構築・修復を試みるが、この入力には許可先がない頂点がある。
    auto result = solver.solve(opt);

    // 目的値の有無を成功判定に使う。
    std::cout << "found = " << std::boolalpha << bool(result.best_cost) << '\n';

    // この分岐ではbest_labels()を呼んではいけない。
    if (!result.best_cost) {
        // 一般には未発見は不存在の証明ではない。この入力は許可集合から矛盾が分かる。
        std::cout << "no feasible solution found\n";
    }

    // この入力に対する期待を検証する。
    assert(!checked.feasible && !checked.cost && !result.best_cost);
}
```

**標準出力例**

```text
Evaluation{feasible=false, cost=none}
found = false
no feasible solution found
```

**読み方・カスタマイズ時の注意**

best_costがないときにbest_labels()は呼べません。一般の問題では、未発見を解の不存在と断定しません。この例だけは空の許可集合という入力の意味から矛盾が分かります。失敗を想定した教材なので、正常に結果を表示して終了します。

#### 例35 大きさが厳密に固定された領域を交換で改善する

1頂点の移動は容量違反になるが、2頂点を同時に交換すれば合法なまま改善できる問題です。

**解く問題**

4頂点を輪状につなぎ、各辺費用1、各領域の頂点数をちょうど2にします。頂点1を領域0、頂点3を領域1へ置く場合だけ配置費用10を加えます。初期ラベル0,0,1,1の費用は22です。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/33_exact_capacity.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 複数頂点を同時に変える近傍を使う。
#include <iostream> // 目的値とラベルを表示する。
#include <vector> // 初期解と比較用のラベルを持つ。

int main() {
    using Solver = ConnectedPartitionSolver<int64_t>; // 目的値が整数の問題。

    Solver::Problem p(4, 2); // 4頂点を2領域に分ける。
    p.edges = { // 輪状の無向グラフ。
        {0, 1, 1},
        {1, 2, 1},
        {2, 3, 1},
        {3, 0, 1}
    };

    for (auto& rule : p.regions) { // 両領域の大きさを厳密に固定する。
        rule.min_vertices = 2; // 各領域は少なくとも2頂点。
        rule.max_vertices = 2; // 各領域は最大2頂点、したがって常に2対2。
    }

    // 配置費用を頂点ごとに、領域0・領域1の順で並べる。配列の長さは n * k。
    p.unary_cost = {
        0, 0, // 頂点0：領域0、領域1の順。
        10, 0, // 頂点1：領域0、領域1の順。
        0, 0, // 頂点2：領域0、領域1の順。
        0, 10 // 頂点3：領域0、領域1の順。
    };

    Solver solver(p); // 入力を取り込む。

    // 連結な2対2だが、配置費用20と切断費用2が掛かる。
    std::vector<int> initial{0, 0, 1, 1};

    Solver::Options opt; // まず1頂点移動だけで試す設定。
    opt.budget_us = -1; // 試行数で比較する。
    opt.max_steps = 500; // 1回につき500試行。

    // 悪化を採用しない。制約違反は採用方式によらず不可。
    opt.acceptance = Solver::Acceptance::greedy;

    opt.weights.fill(0); // 近傍を一旦全て無効にする。

    // 1頂点移動だけを選ぶ。2対2を崩すため有効な改善がない。
    opt.weights[Solver::Move] = 1;

    auto single = solver.improve(initial, opt); // 元の合法解は保持される。

    // この入力ではMoveだけで目的値22から動けない。
    assert(single.best_cost && *single.best_cost == 22);

    // 制約で止まった基準値を表示する。
    std::cout << "move_only_cost = " << *single.best_cost << '\n';

    opt.weights[Solver::Move] = 0; // 1頂点移動を外す。

    // 2頂点を同時交換して、大きさ2対2を保つ候補を作る。
    opt.weights[Solver::Swap] = 1;

    // 比較のため同じ初期解・同じ試行数からやり直す。
    auto result = solver.improve(initial, opt);

    if (!result.best_cost) { // 成功確認後だけ最良ラベルを参照する。
        return 1;
    }

    // この小さな例では交換により費用2へ改善できる。
    std::cout << "cost = " << *result.best_cost << '\n';
    for (int r : solver.best_labels()) { // 交換後も両領域は連結で2頂点ずつ。
        std::cout << r << ' ';
    }
    std::cout << '\n';

    assert(solver.evaluate(solver.best_labels()).feasible); // 制約を全体から再確認する。
}
```

**標準出力例**

```text
move_only_cost = 22
cost = 2
0 1 1 0
```

**読み方・カスタマイズ時の注意**

Moveのみでは2対2を崩すため動けません。Swapなら2対2を維持した候補を作れます。採用方式を変えるだけでは必須条件は緩みません。表示は同じ初期解からMoveとSwapを各500試行で比べたものです。

#### 例36 離れた設備ペアと領域数の非線形費用を扱う

特定の設備の対を同じ担当に寄せたい条件と、未使用区域が増えるほど重くなる費用を同時に扱います。

**解く問題**

2行3列の6マス、3候補領域、2領域以上を使用します。各隣接辺の費用は1です。対(0,3),(1,4),(2,5)が別領域なら1組につき5、未使用領域数の二乗に2を掛けた費用を加えます。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/34_remote_pairs.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 全体のラベルを読む独自費用を使う。
#include <iostream> // 結果を表示する。
#include <vector> // 初期ラベルを保存する。

int main() {
    using Solver = ConnectedPartitionSolver<double>; // 費用をdoubleで計算する。

    Solver::Problem p(6, 3); // 2行3列の盤面、候補領域は0,1,2。
    p.edges = { // 横と縦の無向隣接。
        {0, 1, 1},
        {1, 2, 1},
        {3, 4, 1},
        {4, 5, 1},
        {0, 3, 1},
        {1, 4, 1},
        {2, 5, 1}
    };

    for (auto& rule : p.regions) { // 各領域の条件を順に設定する。
        // 各候補領域は空でもよい。使う領域の連結性は必要。
        rule.required = false;
    }

    // 全マスを1領域へ寄せず、少なくとも2領域を使う。
    p.min_active_regions = 2;

    // 頂点の対と、領域数をまとめて評価するモデル。
    struct Model {
        // 標準の辺費用へ追加する全体費用。
        double global_cost(const Solver::SummaryView& s) const {
            // この入力は偶数頂点で、前半と後半が上下の行に対応する。
            int half = s.size() / 2;
            int separated = 0; // 対になる設備が異なる担当に分かれた数。

            // 対は(0,3),(1,4),(2,5)。モデルの定義も入力に合わせる。
            for (int v = 0; v < half; ++v) {
                // SummaryViewから候補のラベルを読む。
                separated += s.label(v) != s.label(v + half);
            }

            // 候補領域数から、頂点が入った領域数を引く。
            int unused = s.region_count() - s.active_regions();

            // 分離1組に5、未使用数の二乗に2を掛ける。
            return 5.0 * separated + 2.0 * unused * unused;
        }
    };

    // 呼出し中のモデルは不変。ビューを外へ保存しない。
    Model model;

    Solver solver(p); // 盤面と領域条件を取り込む。

    std::vector<int> initial{0, 0, 1, 0, 0, 1}; // 2領域を使う連結な初期解。領域2は空。

    Solver::Options opt; // 既定の自動温度の焼きなましを使う。
    opt.budget_us = -1; // この例は試行数で停止する。
    opt.max_steps = 2'000; // 2000回を上限にする。

    auto result = solver.improve(initial, opt, model); // 独自の全体費用を含めて改善する。

    if (!result.best_cost) { // 合法解がある場合だけ最良ラベルを読む。
        return 1;
    }

    std::cout << "cost = " << *result.best_cost << '\n'; // 辺費用と独自全体費用の合計。
    for (int r : solver.best_labels()) { // 6マスのラベルを行優先順で表示する。
        std::cout << r << ' ';
    }
    std::cout << '\n';

    // 再評価でもモデルを忘れずに渡す。
    assert(solver.evaluate(solver.best_labels(), model).feasible);
}
```

**標準出力例**

```text
cost = 4
0 0 2 0 0 2
```

**読み方・カスタマイズ時の注意**

SummaryViewのsize、label、region_count、active_regionsを使って候補全体を読みます。一般に全ラベル走査は候補ごとに費用が掛かります。対の配置規則はこの入力に合わせたもので、頂点数や番号の意味を変える場合はモデルも変えます。

#### 例37 頂点数や領域数が変わる問題へ差し替える

頂点が増えたり、担当の候補数が変わったりした場合の、状態の作り直し方です。

**解く問題**

最初は4頂点・2領域の1列を改善します。その後、7頂点・3領域の1列へ更新し、辺費用を順に1,2,3,4,5,6としてsolveで構築し直します。

**入力**

上記の問題データをコード内に定数として含めています。標準入力は不要です。

**コード：`examples/35_resize.cpp`**

```cpp
#include "connected_partition_solver_v12.hpp" // 問題全体をupdateで差し替える。
#include <iostream> // 更新後の結果を表示する。
#include <vector> // 最初の問題の初期ラベルを用意する。

int main() {
    using Solver = ConnectedPartitionSolver<double>; // 費用型をdoubleにする。

    Solver::Problem p(4, 2); // 最初は4頂点、2領域。
    p.edges = { // 1列のグラフ。
        {0, 1, 1},
        {1, 2, 3},
        {2, 3, 1}
    };

    Solver solver(p); // 最初の問題を取り込む。

    Solver::Options opt; // 両方の問題に使う探索設定。
    opt.budget_us = -1; // この例は時間停止を外す。
    opt.max_steps = 2'000; // 各呼出しで最大2000試行。

    std::vector<int> initial{0, 0, 1, 1}; // 最初の問題の合法な初期解。

    auto first = solver.improve(initial, opt); // まず最初の問題を解く。

    if (!first.best_cost) { // 最初の成功を確認する。
        return 1;
    }

    // 更新後は7頂点、3領域。依存配列の長さを間違えないよう新しく作る。
    Solver::Problem expanded(7, 3);
    expanded.edges = { // 新しい辺と費用。
        {0, 1, 1},
        {1, 2, 2},
        {2, 3, 3},
        {3, 4, 4},
        {4, 5, 5},
        {5, 6, 6}
    };

    // nまたはkが変わると、古い候補と目的値は破棄される。
    solver.update(expanded);

    // 候補がないのでresumeやrepair(opt)は使わず、初期解を構築し直す。
    auto result = solver.solve(opt);

    if (!result.best_cost) { // 新しい問題について成功を確認する。
        std::cout << "no feasible solution found\n";
        return 1;
    }

    std::cout << "cost = " << *result.best_cost << '\n'; // 新しい3領域問題の目的値。
    for (int r : solver.best_labels()) { // 長さ7、各ラベル0〜2の新しい解。
        std::cout << r << ' ';
    }
    std::cout << '\n';

    // 古い長さ4のラベルを流用していないことを確認する。
    assert(solver.best_labels().size() == 7);
}
```

**標準出力例**

```text
cost = 3
2 1 0 0 0 0 0
```

**読み方・カスタマイズ時の注意**

nまたはkが変わると古い候補は破棄されます。そこでresumeや割当省略のrepairを呼ばず、solveを使います。新サイズの完全な初期ラベルを用意できるならimproveや割当付きrepairも使えます。


### 3.4 API網羅性と検証情報

公開型・フィールド・列挙値・メソッド・出力演算子・独自モデルのコールバックを第2章で説明し、37例で組み合わせています。特に全7個の `SummaryView` メソッド、6種類のモデルコールバック、4種類の構造体のストリーム出力をサンプルで扱っています。API項目一覧と照合記録は `validation/api_coverage.json` に含めます。

検証環境は `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0` です。全37例をC++20とC++23の両方で `-O2 -Wall -Wextra -pedantic` によりコンパイル・実行し、計74実行が成功しました。コンパイル警告はありませんでした。上記以外のコンパイラ、たとえばGCC15.2での実行は今回検証していません。

出力されたラベルについて、ライブラリとは別のPython処理で連結性、各例の主要な制約、目的値を検査しました。固定変更・可動集合・モデル切替などは例内の `assert` でも確認します。合法解なしの例は、期待した失敗表示と目的値の欠落を確認します。これは提示した小さな入力に対する動作検証であり、未知の入力の解品質や厳密な時間保証を示すものではありません。

本文に掲載したC++コードと付属 `.cpp` の一致、コードと入力・出力例の対応、数式のMarkdown解析とKaTeX処理も確認しています。数式に対応しない表示器ではLaTeX記法のまま表示されます。

対象ヘッダーのSHA-256は `7c5053622250e406d8c155bc8c337389efab559ab0236178e28ab0db897072f0` です。付属のヘッダーはこの内容です。

全例の再検証は、展開したフォルダーで次のように行えます。Python 3とGCCが必要です。

```sh
python3 verify_examples.py --std both --jobs 2
```

`--jobs` は同時にコンパイルする本数です。メモリが少なければ1にします。全入力、標準出力・標準エラー出力、コンパイラ情報は `validation/examples-both.json` に記録します。個別の表示例は `outputs` にあります。

