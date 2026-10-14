# CapacitatedRouter サンプルコード集

対象は同梱の `capacitated_router_v05.hpp` です。C++20 / GCC で利用します。数学の前提は、足し算・掛け算・不等式・関数の基本だけです。

この文書は、問題の意味、全公開API、独立して実行できる30のユースケースの順に構成しています。各コードはその例だけで完結しています。コードを `main.cpp` に保存し、ヘッダーを同じ場所に置けば実行できます。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -pedantic main.cpp -o main
./main < input.txt
```

`input.txt` には各例の「入力例」を保存します。ZIPではコードを `examples/`、入力を `inputs/`、期待する標準出力を `expected/` に分けています。ZIPからの実行方法は第3節にあります。

## 1. 何を解くライブラリか？

### 1.1 複数の「ここから、ここまで」を、容量を守ってつなぐ

道路網を考えてください。交差点を**頂点**、一方通行の道路を**有向辺**と呼びます。「荷物を頂点0から頂点4へ届けたい」という依頼が**要求**です。要求ごとに出発地・目的地・荷物量を指定します。

本ライブラリは、複数の要求について、**1要求を1本の道順で接続するか、接続しないか**を選びます。道順を**経路**と呼びます。荷物を2本の経路へ半分ずつ分けることはしません。同じ要求の経路は、同じ頂点を繰り返して通らない**単純路**です。

すべての要求を接続する必要があるなら「必須要求」にします。一部を断ってよいなら「任意要求」にし、断る損失を非負の費用として指定します。任意要求が未接続でも、他の条件を守っていれば有効な解です。

制約には次のようなものがあります。

- **道路や場所の容量**：ある道路は荷物2単位まで、ある中継所は1単位まで。
- **共用の容量**：離れた複数の道路が同じ通信帯域や電源を使う。
- **要求ごとの上限**：各車の電池消費は10まで、各通信の経由区間数は5まで。
- **禁止と固定**：通れない辺・頂点を指定したり、予約済みの経路を変更不可にする。
- **外部利用**：管理対象外の利用者がすでに使っている容量を加える。

共用の上限を持つものを**資源**と呼びます。資源は物体である必要はありません。電池消費や経由区間数のような「数えたい量」も資源で表せます。

同じ枠組みで、通信経路、配送、配線、設備利用、作業の割当、時刻を区別した予約などを表せます。ただし現実の制約が自動的に追加されるわけではありません。何を頂点・辺・資源・要求にするかは利用者が決めます。

### 1.2 費用と資源消費を分ける

**費用**は小さくしたい値、**資源消費**は上限と比較する量です。道路の距離を費用、燃料を資源にすると「燃料が足りる範囲で、短い経路を探す」と表せます。資源を1消費したことが、そのまま費用1になるわけではありません。

辺・頂点・要求には、資源ごとに「需要1単位当たりの消費」と「1回当たりの固定消費」を指定できます。需要2に対し、単位消費3、固定消費1なら、その指定1回分の消費は $3\times2+1=7$ です。

同じ資源を2本の辺が使えば、両方の消費を足します。資源IDが同じだから1回分にまとめる、という意味ではありません。要求に直接付けた消費は、その要求を接続したときに1回だけ足します。

### 1.3 目的関数

**目的関数**は、どの解を良いとするかを数で表したものです。本ライブラリには2種類あります。まず、それらに共通する3つの費用を定義します。

記号は次の意味です。英字の添字は「どの要求か」「どの資源か」を区別する番号です。

| 記号 | 意味 |
|---|---|
| $K$ | 有効な要求の集合。`active=false` の要求は含めない |
| $K_{\mathrm{optional}}$ | $K$ のうち任意要求の集合 |
| $k$ | 要求の番号。和の下に $k\in K$ とあれば、対象の全要求について足す |
| $R$ | 資源の集合。ここでの数式の $R$ は、コード例の型の別名 `R` とは別の記号 |
| $r$ | 資源の番号 |
| $z_ {k}$ | 要求 $k$ を接続するなら1、接続しないなら0 |
| $P_ {k}$ | 接続した要求 $k$ の経路。未接続時には経路費用と消費を0とする |
| $A(P_ {k})$ | 経路上の有向辺の列 |
| $V(P_ {k})$ | 経路上の頂点の列。出発地と目的地も含む。同じ頂点間の接続ではその頂点1個 |
| $a,v$ | それぞれ辺の番号、頂点の番号 |
| $d_ {k}$ | 要求 $k$ の需要量 `demand`。非負整数 |
| $s_ {k}$ | 要求 $k$ の費用倍率 `cost_scale`。非負 |
| $p_ {k}$ | 要求 $k$ を断る費用 `reject_cost`。非負 |
| $c_ {ka}$ | 要求 $k$ が辺 $a$ を通るときの、倍率適用前の費用。標準では `Arc::cost` |
| $t_ {kv}$ | 要求 $k$ が頂点 $v$ を使うときの、倍率適用前の費用。標準では `Vertex::cost` |
| $L_ {r}$ | 資源 $r$ の、背景使用を含めた全体使用量 |
| $f_ {r}(L_ {r})$ | 資源 $r$ の全体使用量に対する費用。標準では0 |
| $C_{\mathrm{reject}}$ | 未接続の任意要求に対する費用の合計 |
| $C_{\mathrm{route}}$ | 接続した全要求の経路費用の合計 |
| $C_{\mathrm{resource}}$ | 全資源の費用の合計 |
| $J$ | 3種類の費用を足した値。和の目的で小さくする値 |

記号 $\sum$ は「指定した対象を全部足す」という意味です。例えば、接続した2件の経路費用が2と5なら、その和は7です。

$$C_{\mathrm{reject}}=\sum_{k\in K_{\mathrm{optional}}}(1-z_ {k})p_ {k}$$

接続すれば $z_ {k}=1$ なので $(1-z_ {k})p_ {k}=0$、断れば $z_ {k}=0$ なので $p_ {k}$ を払います。必須要求の未接続は費用を払えば許されるものではなく、制約違反です。

$$C_{\mathrm{route}}=\sum_{k\in K,\ z_ {k}=1}s_ {k}\left(\sum_{a\in A(P_ {k})}c_ {ka}+\sum_{v\in V(P_ {k})}t_ {kv}\right)$$

各経路の辺費用と頂点費用を足し、その要求の倍率を掛けます。**需要量は費用に自動では掛かりません。** 荷物1単位ごとの運賃にしたい場合などは、利用者が `cost_scale` を設定します。

$$C_{\mathrm{resource}}=\sum_{r\in R}f_ {r}(L_ {r})$$

資源費用は、要求ごとの使用量を足した後に資源ごとに1回計算します。例えば、使用量の2乗の料金なら、2要求が1ずつ使った場合は $1^2+1^2=2$ ではなく $(1+1)^2=4$ です。

**既定の目的 `PenaltyPlusCost`** は、次の和を最小にします。

$$\min J=C_{\mathrm{reject}}+C_{\mathrm{route}}+C_{\mathrm{resource}}$$

例えば断る費用3、接続費用5、資源費用0の任意要求なら、断る方がこの目的では安くなります。

**もう1つの目的 `PenaltyThenCost`** は、次の組を**辞書順**で最小にします。

$$\min_{\mathrm{lex}}\left(C_{\mathrm{reject}},\ C_{\mathrm{route}}+C_{\mathrm{resource}}\right)$$

`lex` は辞書順という比較方法の印です。まず左側を比べ、小さい方を選びます。左側が等しい場合だけ右側を比べます。この目的なら、断る費用3を0にできる接続が先に評価され、接続費用5との交換条件にはしません。重み付きの未接続費用を比べるので、必ずしも接続件数の最大化ではありません。

### 1.4 資源と実行可能性の数式

目的を小さくする前提として、次の制約を満たす必要があります。

| 記号 | 意味 |
|---|---|
| $O_ {kr}(P_ {k})$ | 要求 $k$ の経路で資源 $r$ を消費する指定の集まり。辺・対象となる頂点・要求自身の `ResourceUse` を含む。同じ指定が複数の場所にあれば、その出現ごとに数える |
| $o$ | 上記の消費指定の1つ |
| $\alpha_ {o}$ | 指定 $o$ の単位消費 `per_unit`。非負整数 |
| $\beta_ {o}$ | 指定 $o$ の固定消費 `per_use`。非負整数 |
| $u_ {kr}$ | 要求 $k$ だけが資源 $r$ を消費する量 |
| $B_ {r}$ | 管理対象外の背景使用 `background_load`。非負整数 |
| $U_ {r}$ | 資源 $r$ の全体容量 `capacity`。有限の場合の非負整数 |
| $H_ {kr}$ | 要求 $k$ に指定した資源 $r$ の個別上限 `LocalLimit::upper`。指定がある場合だけ使う |

$$u_ {kr}=\begin{cases}\displaystyle\sum_{o\in O_ {kr}(P_ {k})}(\alpha_ {o}d_ {k}+\beta_ {o})&z_ {k}=1\\0&z_ {k}=0\end{cases}$$

$$L_ {r}=B_ {r}+\sum_{k\in K}u_ {kr}$$

有限容量の資源は $L_ {r}\le U_ {r}$ を満たします。APIで容量を `-1` にした資源にはこの全体上限がありません。個別上限を指定した組については $u_ {kr}\le H_ {kr}$ を満たします。**個別上限には背景使用や他要求の消費を含めません。**

さらに、必須要求は接続し、使用禁止の辺・頂点を通らず、固定された要求は固定内容と一致させます。経路は指定した始点から終点へつながる単純路でなければなりません。

`consume_endpoints=false` の場合、端点の**頂点資源の消費指定**だけを $O_ {kr}(P_ {k})$ から除きます。端点の費用・通行許可、辺と要求の消費は除きません。

すべての制約を満たす解を**実行可能解**と呼び、`feasible()` で判定します。実行可能解が返ることと、最も良い実行可能解が返ることは別です。本ライブラリは最適性や、存在する実行可能解を必ず発見することを保証しません。

## 2. APIリファレンス

### 2.1 型・値・寿命の共通ルール

以下で `R` は `CapacitatedRouter<>` または `CapacitatedRouter<double>` の別名です。表の `Cost` はこのテンプレート引数です。すべての公開型は `CapacitatedRouter` の中にあります。`LimitViolation` だけはさらに `Evaluation` の中です。

| 公開名 | 意味・取り得る値・既定 |
|---|---|
| `template<class Cost = int64_t> class CapacitatedRouter` | 費用型を選ぶ外側のクラス。通常は既定の `int64_t` または `double`。型検査は64bit以上の符号付き整数型、または `double` を受け付ける。`float`・符号なし整数・`long double` は受け付けない |
| `R::Clock` | `std::chrono::steady_clock` の別名。絶対締切にはこの時計の時刻を使う |
| ID | 頂点・有向辺・資源・要求のIDはそれぞれ独立した `int`。各配列の0始まりの添字。存在する要素の範囲に限る |
| 費用 | `Cost`。辺・頂点・未接続費用・倍率・モデルが返す費用は有限かつ非負 |
| 消費・容量 | `int64_t`。消費・需要・個別上限・背景使用は非負。全体容量だけは `-1` を無制限の印に使う |
| 入力の大きさ | 配列長、ID、全ての中間積・総和、時刻の加算が使用する型に収まる範囲。極大値の飽和計算や自動的な型拡張はない |
| 契約違反 | 想定外の入力には `assert` を使う。APIは入力エラーを例外やエラーコードで網羅的に報告する設計ではない。`NDEBUG` で検査を無効にしても入力条件は同じ |
| `Problem` の寿命 | `Solver` は参照する。使用中は同じ `Problem` オブジェクトを生存させる。構築後に問題オブジェクト自体を移動しない |
| `Model` の寿命 | `Solver` は値として保持する。ただし参照・ポインタを持つモデルの参照先は利用者が生存させる |
| 変更の反映 | 探索中に問題やモデルの参照先を変更しない。呼び出し間の変更には後述の `refresh` / `reset` / `set_model` を使う |

「既定値なし」とある整数メンバーには、意味のあるIDや値を明示してください。例えば `ResourceUse use;` の `resource` を未設定のまま使ってはいけません。集成体の `{}` による値初期化で0になる場合があっても、適切なIDを選んだことにはなりません。以下では公開データのメンバーと、このライブラリが定義するメソッド・演算子を列挙します。`std::vector` など標準型の全メソッドを個別に再定義しているわけではありません。

### 2.2 列挙型

#### `R::Objective`

| 列挙値 | 正確な意味 |
|---|---|
| `PenaltyPlusCost` | 未接続費用＋経路費用＋資源費用の和を最小化。`Problem::objective` の既定値 |
| `PenaltyThenCost` | まず未接続費用の合計を最小化。同点時だけ経路費用＋資源費用を最小化。全要求の接続必須化ではなく、接続件数最大化とも限らない |

#### `R::StopReason`

停止理由と実行可能性は別です。成功判定は `Result::feasible()` で行います。

| 列挙値 | 正確な意味 |
|---|---|
| `TimeLimit` | 相対時間または絶対締切に達して探索を終了した。実行可能な解がある場合も、未発見の場合もある |
| `IterationLimit` | 指定反復数まで実行した。`Result` の既定の列挙値でもある。最適性や失敗の証明を意味しない |
| `NoMovableRequests` | 有効かつ非固定の要求がない。全件固定、全件無効、要求なしなど。固定分に矛盾がある場合は `FixedConflict` が先に使われる |
| `FixedConflict` | 背景使用や固定分だけで容量超過する、固定要求が禁止条件・個別上限に反する、必須要求を未接続固定する、接続固定を残して無効化するなど、可動要求の経路変更では解消できない矛盾がある |

通常の「可動要求を予算内に全部接続できなかった」は、`FixedConflict` とは限りません。列挙型は名前で指定し、整数からの無検査なキャストは避けてください。

### 2.3 資源・頂点・辺の型

#### `R::ResourceUse`

1つの場所で1資源を消費する指定です。消費は `per_unit * demand + per_use`。複数指定は加算します。

| メンバー | 型 | 既定値 | 意味・許される値 |
|---|---|---|---|
| `resource` | `int` | なし | 既存資源のID |
| `per_unit` | `int64_t` | `1` | 需要1単位当たりの消費。0以上 |
| `per_use` | `int64_t` | `0` | この指定を使ったときの固定消費。0以上 |

#### `R::LocalLimit`

| メンバー | 型 | 既定値 | 意味・許される値 |
|---|---|---|---|
| `resource` | `int` | なし | 個別に制限する既存資源のID |
| `upper` | `int64_t` | なし | この要求だけの総消費上限。0以上。同じ要求の `limits` 内では資源IDを重複させない |

#### `R::Resource`

| メンバー | 型 | 既定値 | 意味・許される値 |
|---|---|---|---|
| `capacity` | `int64_t` | `-1` | 全体使用量の上限。-1は無制限、0以上は有限容量 |
| `background_load` | `int64_t` | `0` | 外部の既存使用量。0以上。全体容量と資源費用に含め、各要求の個別上限には含めない |

背景使用が有限容量を超えると、探索結果は `FixedConflict` です。背景使用は外部経路の費用までは表しません。

#### `R::Vertex`

| メンバー | 型 | 既定値 | 意味・許される値 |
|---|---|---|---|
| `cost` | `Cost` | `0` | 標準モデルでこの頂点を使う費用。有限・非負。端点にも適用する |
| `enabled` | `bool` | `true` | falseなら全要求に対しこの頂点を禁止。端点も例外ではない |
| `uses` | `vector<ResourceUse>` | 空 | この頂点での資源消費。端点で数えるかは要求の `consume_endpoints` に従う |

#### `R::Arc`

| メンバー | 型 | 既定値 | 意味・許される値 |
|---|---|---|---|
| `from` | `int` | なし | 既存の出発頂点ID |
| `to` | `int` | なし | 既存の到着頂点ID |
| `cost` | `Cost` | `1` | 標準モデルでこの有向辺を使う費用。有限・非負 |
| `enabled` | `bool` | `true` | falseなら全要求に対しこの辺を禁止 |
| `uses` | `vector<ResourceUse>` | 空 | この辺での資源消費 |

同じ両端の平行辺を作れます。それぞれ別の辺IDです。自己ループの辺を格納しても、同じ頂点を繰り返す経路は有効な単純路になりません。通常は自己ループを作る必要はありません。

### 2.4 要求・経路・解の型

#### `R::Route`

| メンバー / メソッド | 型・戻り値 | 既定値・条件・意味 |
|---|---|---|
| `connected` | `bool` | `false`。trueなら接続済み |
| `arcs` | `vector<int>` | 空。通る**有向辺ID**を順番に並べる。頂点ID列ではない |
| `operator==(const Route&) const` | `bool` | 接続状態と辺ID列が完全一致するか。比較相手に既定値なし |

未接続は `connected=false` かつ空の辺列です。接続済みなら、始点から終点につながり、同じ頂点を繰り返さない必要があります。`source==target` の接続は `Route{true, {}}`。その1頂点だけを使います。異なる端点で接続済みの辺列が空、という形は無効です。

#### `R::Request`

| メンバー | 型 | 既定値 | 意味・許される値 |
|---|---|---|---|
| `source` | `int` | なし | 既存の出発頂点ID。無効な要求でも範囲内のIDを持たせる |
| `target` | `int` | なし | 既存の目的頂点ID。sourceと同じでもよい |
| `demand` | `int64_t` | `1` | 非負の需要量。経路分割はしない。0でも固定消費 `per_use` は残る |
| `active` | `bool` | `true` | falseなら費用・資源消費・必須接続の対象から外れ、返却経路は未接続の空経路。接続固定を残したままfalseにすると矛盾 |
| `required` | `bool` | `true` | 有効な要求について、trueなら接続必須。falseなら未接続を許す |
| `reject_cost` | `Cost` | `0` | 有効な任意要求を接続しなかった場合の費用。有限・非負。必須未接続を合法化する値ではない |
| `cost_scale` | `Cost` | `1` | 辺費用＋頂点費用に掛ける非負の倍率。資源費用・未接続費用には掛けない |
| `consume_endpoints` | `bool` | `true` | falseなら始点・終点の頂点資源だけを消費しない。頂点費用や禁止判定は有効なまま |
| `uses` | `vector<ResourceUse>` | 空 | この要求を接続した場合に1回加える資源消費。未接続では0 |
| `limits` | `vector<LocalLimit>` | 空 | この要求の資源別上限。背景・他要求を除いた、辺・対象頂点・要求自身の消費合計と比べる |
| `fixed_route` | `optional<Route>` | 値なし | 値なしは変更可。接続経路が入っていればその経路に固定。`Route{}` が入っていれば未接続に固定。解除は `.reset()` |

固定経路にも、通常の費用・消費・禁止条件・個別上限が適用されます。既存の解から経路を固定する場合は、対象要求と経路の対応を守ってください。

#### `R::Solution`

| メンバー / メソッド | 型・戻り値 | 既定値・条件・意味 |
|---|---|---|
| `routes` | `vector<Route>` | 空。`routes[k]` が要求kの経路。返却解には無効要求を含め全要求分が入る |
| `operator==(const Solution&) const` | `bool` | 全経路が完全一致するか。比較相手に既定値なし |

`evaluate` では要求数と同じ経路数が必要です。`improve` では要求数以下を受け付け、省略された**末尾**を未接続で埋めます。要求数より多い経路は渡せません。

### 2.5 問題の構築：`R::Problem`

| 公開メンバー | 型 | 既定値・意味 |
|---|---|---|
| `vertices` | `vector<Vertex>` | `Problem(n)` では既定の頂点n個 |
| `arcs` | `vector<Arc>` | 空。添字が有向辺ID |
| `resources` | `vector<Resource>` | 空。添字が資源ID |
| `requests` | `vector<Request>` | 空。添字が要求ID |
| `objective` | `Objective` | `PenaltyPlusCost`。許される2値は列挙型の表を参照 |

配列は直接編集できます。IDの対応と各メンバーの条件を守り、構築済みsolverには変更を反映してください。

| コンストラクタ / メソッド | パラメータ・既定値 | 戻り値・動作 |
|---|---|---|
| `explicit Problem(int n = 0)` | `n`：0以上の頂点数、既定0 | 問題を構築。頂点だけn個作り、他の配列は空 |
| `int add_resource(int64_t capacity = -1, int64_t background_load = 0)` | `capacity`：-1または0以上。`background_load`：0以上 | 末尾に資源を追加し、その資源IDを返す |
| `int add_arc(int u, int v, Cost cost = 1, int64_t capacity = -1)` | `u`,`v`：既存頂点ID、既定なし。`cost`：有限・非負、既定1。`capacity`：-1または0以上、既定-1 | u→vの辺IDを返す。有限容量なら専用資源を1個作り、`{resource,1,0}` を辺のusesへ追加。-1ならその資源を作らない |
| `pair<int,int> add_undirected_edge(int u, int v, Cost cost = 1, int64_t capacity = -1)` | 各引数は `add_arc` と同じ条件・既定値 | u→v、v→uの順の辺IDペア。有限容量なら**1つの共用資源**を2辺で使用。往復の消費は足される |
| `int add_vertex_capacity(int v, int64_t capacity)` | `v`：既存頂点ID。`capacity`：0以上。両方既定なし | 頂点v用の新しい資源を作り、頂点usesに `{resource,1,0}` を追加。返すのは**資源ID**。複数回呼べば制約が追加され、置換はしない |
| `int add_request(int s, int t, int64_t demand = 1)` | `s`,`t`：既存頂点ID、既定なし。`demand`：0以上、既定1 | 有効・必須の要求を追加し、要求IDを返す。他の属性は `Request` の既定値 |

双方向で独立した容量が必要なら `add_arc` を両方向へ呼びます。任意の複数辺・頂点を同じ容量へ結び付けるなら、`add_resource` で作ったIDをそれぞれの `uses` に入れます。

### 2.6 費用・禁止条件のモデル：`R::DefaultModel`

`DefaultModel` 自体は状態を持たず、引数なしで既定構築できます。独自のモデルには、次の3つの `const` メソッドを用意します。返り値の型の `Cost` はrouterの費用型と合わせます。`DefaultModel` を継承すれば、変更するメソッドだけを書けます。

| メソッド | パラメータ | 戻り値と標準動作 |
|---|---|---|
| `optional<Cost> arc_cost(const Problem& p, int request, int arc) const` | `p`：現在の問題。`request`：要求ID。`arc`：辺ID。全引数に既定値なし | 倍率適用前の有限・非負の辺費用。`nullopt` はこの要求の使用禁止。標準は `p.arcs[arc].cost` |
| `optional<Cost> vertex_cost(const Problem& p, int request, int vertex) const` | `p`：現在の問題。`request`：要求ID。`vertex`：頂点ID。全引数に既定値なし | 倍率適用前の有限・非負の頂点費用。`nullopt` は使用禁止。標準は `p.vertices[vertex].cost` |
| `Cost resource_cost(const Problem& p, int resource, int64_t load) const` | `p`：現在の問題。`resource`：資源ID。`load`：背景使用を含む非負の全体使用量。全引数に既定値なし | 資源全体の有限・非負の費用。標準は常に0。負荷が0なら0、負荷が増えても費用が減らない関数にする |

コールバックは**同じ入力なら同じ結果を返す**関数にします。呼び出すたびに乱数を引く、現在時刻により料金を変える、呼び出し順に内部状態を変更する、といった実装は使いません。経路の途中でどの動作を済ませたかを扱う場合は、例26のように頂点の状態へ表します。

`enabled=false` はモデルが返す値によらず使用禁止です。有限容量の資源について、ライブラリは `resource_cost` を使用量0〜容量の範囲内で呼びます。容量超過をモデルの返り値で独自の罰金に置き換える仕組みではありません。容量超過のある解を `evaluate` すると費用値は空になります。

モデルを別の型へ変更する場合は、その型のsolverを新しく作り、必要なら解を `improve` に渡します。同じ型で中身を交換するには `set_model` を使います。

### 2.7 実行の上限：`R::Limits` と `R::Options`

#### `R::Limits`

| メンバー | 型 | 既定値 | 許される値と意味 |
|---|---|---|---|
| `time_limit_us` | `int64_t` | `100000` | 相対時間の上限。マイクロ秒単位。0以上、または-1で無効化。100000µsは100ms |
| `deadline` | `Clock::time_point` | `Clock::time_point::max()` | 絶対締切。既定は無効化に相当。過去の時刻や現在時刻も指定でき、探索時間が残っていない扱いになる |
| `max_iterations` | `int64_t` | `-1` | 1呼び出し当たりの反復上限。0以上、または-1で無効化。1反復で何回も経路を探す場合があり、経路探索回数とは異なる |

少なくとも1種類の上限を有効にします。相対時間・絶対締切・反復数をすべて無効化した指定は不可です。相対時間と絶対締切の早い方を時間上限にし、反復上限にも達すれば終了します。固定矛盾や可動要求なしの場合はその理由で終了します。

0µsや0反復でも、初期状態の取込みと結果の保存などは行います。時間制限は処理の区切りで確認するもので、指定時刻ぴったりに終了する保証ではありません。AHC全体の制限から入力・構築・更新・出力などの時間と余裕を差し引いて指定してください。

#### `R::Options`

| メンバー | 型 | 既定値 | 許される値と意味 |
|---|---|---|---|
| `seed` | `uint64_t` | `0` | 0〜2の64乗−1の乱数種。`solve` / `improve` の開始時に使う。`resume` には渡さない |

同じ入力・モデル・乱数種・固定反復数・実行環境で比較すると再現性を確認しやすくなります。時間で止める場合、同じ乱数種でも実行できる反復数は一定とは限りません。

### 2.8 費用・診断・結果の型

#### `R::Value`

| メンバー / メソッド | 型・戻り値 | 既定値・意味 |
|---|---|---|
| `reject_cost` | `Cost` | `0`。有効な任意要求の未接続費用合計 |
| `route_cost` | `Cost` | `0`。倍率適用後の経路費用合計 |
| `resource_cost` | `Cost` | `0`。背景使用を含めた資源費用合計 |
| `total() const` | `Cost` | 引数なし。上の3値の和。辞書順の目的の優劣をこの値だけで比べない |
| `operator==(const Value&) const` | `bool` | 引数に既定値なし。3費用の完全一致。doubleでも許容誤差を設けない |

#### `R::Summary`

| メンバー / メソッド | 型・戻り値 | 既定値・意味 |
|---|---|---|
| `missing_required` | `int` | `0`。未接続の有効な必須要求数 |
| `overloaded_resources` | `int` | `0`。全体容量を超えた資源数 |
| `locally_invalid_requests` | `int` | `0`。1つ以上の個別上限に反した要求数。同じ要求で複数上限を超えても1件 |
| `forbidden_requests` | `int` | `0`。禁止条件に反した経路を持つ要求数 |
| `fixed_mismatches` | `int` | `0`。指定された固定経路と異なる要求数 |
| `value` | `optional<Value>` | 値なし。費用を計算できる場合に値が入る。全体容量超過または禁止経路がある評価では空 |
| `feasible() const` | `bool` | 引数なし。上の5種類の件数がすべて0かを返す |

**`value` が存在するだけでは実行可能とは限りません。** 必須未接続・個別上限違反・固定不一致があっても費用が計算できる場合があります。反対に、単に自分で既定構築した `Summary{}` の `feasible()` は件数が0なのでtrueですが、実際の問題を検査したことにはならず `value` も空です。実行・評価で得た結果を使います。

#### `R::Evaluation::LimitViolation`

| メンバー | 型 | 既定値・意味 |
|---|---|---|
| `request` | `int` | 明示の既定値なし。違反した要求ID |
| `resource` | `int` | 明示の既定値なし。上限を超えた資源ID |
| `used` | `int64_t` | 明示の既定値なし。この要求だけの消費量 |
| `upper` | `int64_t` | 明示の既定値なし。指定された個別上限。評価結果では used が upper を超える |

#### `R::Evaluation`

| メンバー / メソッド | 型・戻り値 | 既定値・意味 |
|---|---|---|
| `summary` | `Summary` | 既定構築。診断件数と費用 |
| `resource_load` | `vector<int64_t>` | 空。評価後は全資源分あり、資源ID順の全体使用量。背景を含む |
| `missing_required` | `vector<int>` | 空。未接続の必須要求ID一覧 |
| `overloaded_resources` | `vector<int>` | 空。容量超過の資源ID一覧 |
| `forbidden_requests` | `vector<int>` | 空。禁止条件に反した要求ID一覧 |
| `fixed_mismatches` | `vector<int>` | 空。固定経路と異なる要求ID一覧 |
| `local_violations` | `vector<LimitViolation>` | 空。要求と個別上限の組ごとの違反。1要求に複数要素が入り得る |
| `feasible() const` | `bool` | 引数なし。`summary.feasible()` と同じ |

`Summary::locally_invalid_requests` と `local_violations.size()` は数える対象が違うため、必ずしも一致しません。

#### `R::Statistics`

全メンバーは `int64_t`、既定値は0です。`solve` / `improve` / `resume` の**各呼び出しごと**に集計し直します。

| メンバー | 意味 |
|---|---|
| `iterations` | 実行した反復数。`max_iterations` と対応する |
| `path_searches` | 通常の経路探索を呼んだ回数 |
| `label_searches` | 複数の途中候補を保持する補助的な経路探索を呼んだ回数。個別上限がある問題などで増える。成功経路数ではない |
| `accepted` | 採用された試行数。初期の経路構築も含む。最良値を更新した回数や接続要求数とは異なる |
| `elapsed_us` | 呼び出し内の実測経過マイクロ秒。初期化・結果確定の処理を含む。外で行うコンストラクタ・reset・refresh・ユーザーの出力や結果コピーは含まない |

#### `R::Result`

| メンバー / メソッド | 型・戻り値 | 既定値・意味 |
|---|---|---|
| `solution` | `Solution` | 空の解。実行後は全要求に対応する経路を保持 |
| `summary` | `Summary` | 既定構築。返された解の診断と費用 |
| `stop_reason` | `StopReason` | `IterationLimit`。実行後の停止理由は列挙型の表を参照 |
| `statistics` | `Statistics` | 全値0。直近の呼び出しの統計 |
| `feasible() const` | `bool` | 引数なし。`summary.feasible()` と同じ |

実行可能解を保持できている場合は、探索中に得た最良の実行可能解を返します。なければ実行可能でない結果も返ります。新しく問題を更新した場合、以前の実行可能性・費用は更新後の問題にはそのまま適用できません。

### 2.9 solverの構築・実行・更新

`template<class Model = DefaultModel> class R::Solver` が実行器です。下記の `Model` はそのsolverのテンプレート引数です。

| 公開API | パラメータ・既定値 | 戻り値・動作 |
|---|---|---|
| `explicit Solver(const Problem& problem, Model model = {})` | `problem`：生存中の問題、既定なし。`model`：既定は `Model{}` | 問題を参照しモデルを値で保持。必要なグラフ情報・作業領域を準備。モデルが既定構築できなければ明示して渡す |
| `static Solver<Model> R::make_solver(const Problem& problem, Model model = {})` | テンプレート引数 `Model=DefaultModel`。引数はコンストラクタと同じ。モデルを渡せば型を推論 | solverを値で返す。コンストラクタを直接呼ぶ代わりの作成関数 |
| `const Result& solve(const Limits& limits, const Options& options = {})` | `limits`：有効な上限設定、必須。`options`：省略時はseed=0 | 背景使用・固定分から新規探索。前の可動経路は引き継がず、乱数と累積探索段階を開始状態に戻す |
| `const Result& improve(const Solution& initial, const Limits& limits, const Options& options = {})` | `initial`：要求数以下の経路、必須。`limits`：必須。`options`：省略時seed=0 | 外部解を取り込んで再開始。末尾不足を未接続で補完。乱数と累積探索段階を初期化。引数の解を書き換えない。自身の返却解を渡すこともできる |
| `const Result& resume(const Limits& limits)` | `limits`：今回追加する上限、必須。options引数なし | 初期化済みの探索を継続。作業状態、最良解、乱数、累積探索段階を保持。先にsolve/improveが必要。統計は今回分へ戻す |
| `void refresh()` | 引数なし | 同じグラフ構造のまま、属性・資源・要求・モデルの参照先の変更を反映。初期化済みなら保持する最良解、なければ作業中の解を基に再集計。乱数と累積探索段階は引き継ぐ。経路の順序などは再準備する |
| `void reset(const Problem& problem)` | 再び参照する問題、必須。同じオブジェクトでも別オブジェクトでもよい | グラフ構造から作り直す。モデルは保持、探索状態と内部結果は破棄。次はsolve/improve。reset直後にresumeは不可 |
| `void set_model(Model model)` | 置き換える同じ型のモデル、必須 | モデルを値で置き換えてrefresh相当の再評価。別途refreshは不要。グラフ構造の変更には使わない |
| `Evaluation evaluate(const Solution& solution) const` | 全要求と同じ数の経路を持つ解、必須 | 現在の問題とモデルで全経路を再評価して値で返す。solve前でも呼べる。探索を行わず、内部の解を差し替えない |

重要な境界条件をまとめます。

- `solve` / `improve` / `resume` は**solverが所有する結果へのconst参照**を返します。後続の変更操作でも保持したい場合は `const auto result = ...;` のようにコピーします。`const auto&` で保存した参照や、その中の経路への参照を永続的な記録にしないでください。
- `refresh` で使う「グラフ構造が同じ」とは、**頂点数・辺数・各辺のfrom/toが同じ**という意味です。どれかを変えたら `reset` が必要です。
- 費用、enabled、資源容量、背景使用、uses、要求、目的などの変更は `refresh` の対象です。資源・要求の追加でも既存IDの意味を保ってください。削除・並べ替えでIDを変更する場合、各参照や保存解を利用者が対応し直す必要があります。
- `refresh` は完全な差分通知APIではなく、入力と旧経路を走査します。グラフの接続情報を再利用しつつ、費用・消費は再集計します。変更前の最良解と作業状態の両方をそのまま残す処理ではありません。
- `improve` / `refresh` では、現在の条件で禁止・不適合となった可動の旧経路を取り除くことがあります。固定経路の問題は固定矛盾として扱います。存在しない辺IDを渡してよいという意味ではありません。
- `evaluate` は経路の構造条件を `assert` するため、壊れた辺ID列を何でも安全に調べる用途には向きません。通常の入力は、正しいID・連続する辺・正しい端点・頂点の重複なしという前提を守ります。
- `refresh` / `set_model` をsolve前に呼んでも探索を開始したことにはなりません。その後はsolve/improveを呼びます。

### 2.10 デバッグ出力と比較演算子

次の各型には `std::ostream& operator<<(std::ostream&, const Xxx&)` があり、`std::cout << value` または `std::cerr << value` と書けます。

| `Xxx` | 出力内容 |
|---|---|
| `R::Route` | 接続状態と辺ID列 |
| `R::Solution` | 要求IDと各経路 |
| `R::Value` | 未接続・経路・資源の各費用とtotal |
| `R::Summary` | 実行可能か、費用またはnone、5種類の診断件数 |
| `R::Evaluation::LimitViolation` | 要求ID・資源ID・実消費・個別上限 |
| `R::Evaluation` | 要約、資源ID付き使用量、違反一覧 |
| `R::Statistics` | 反復・経路探索・補助探索・採用回数と経過µs |
| `R::Result` | 停止理由、要約、統計、解 |

列挙型には `std::ostream& operator<<(std::ostream&, R::StopReason)` があり、`TimeLimit` などの名前を出します。`Objective` や入力用の `Problem`、`Request` などには専用の出力演算子を定義していません。

第1引数は出力先ストリーム、第2引数は表示対象です。どちらにも既定値はなく、戻り値は渡した同じストリームへの参照なので、`<< '\n'` と続けられます。表示対象を書き換えず、出力自体は探索や再評価を行いません。

- 各配列は**先頭20要素**まで。残りがあれば `... (+N more)` と個数を表示します。
- `Solution::routes` と `Evaluation::resource_load` では添字を表示し、要求ID・資源IDが分かります。
- 入れ子の配列はそれぞれ20要素上限です。出力全体で20要素という制限ではありません。
- 改行・flushは自動追加しません。`Result` / `Evaluation` の内部は見やすく複数行になります。
- 数値の精度などは出力先の書式に従い、書式フラグを変更しません。
- 全要素の保存やコンテストの正式な出力には、必要な配列を自分で走査してください。デバッグ文字列を提出形式として前提にしないでください。

`Route`、`Solution`、`Value` には前述の `operator==` があり、C++20では `!=` も使えます。順序比較や「この目的でどちらが良いか」の演算子はありません。辞書順の外部比較は例29を参照してください。

## 3. ユースケース

### 実行方法と読み方

各例は、読み込む入力、解く問題、全コード、標準出力を含みます。省略記号を含む未完成のコードはありません。各例の見出しにあるファイル名のコードだけをコピーして利用できます。ライブラリ使用箇所には、日本語で引数や注意点をコメントしています。

入力例以外の値へ変更する場合は、第2節の非負条件、IDの範囲、整数の収まる範囲などを守ってください。サンプルは正常な競技入力を前提としており、任意の壊れた入力を検査するプログラムではありません。各例の `INFEASIBLE` は「この実行で実行可能な結果を得られなかった」という表示であり、一般に数学的な不可能性の証明ではありません。

ZIPを展開したフォルダで、例えば次のように実行します。

```sh
g++ -std=c++20 -O2 -Wall -Wextra -pedantic -I . examples/01_one_delivery.cpp -o example
./example < inputs/01_one_delivery.txt
```

全例をまとめてコンパイルし、標準出力を同梱の期待値と比較するには、Python 3から実行します。

```sh
python3 tools/check_examples.py --jobs 2
```

コード例はGCC 13.3.0、C++20、`-O2 -Wall -Wextra -pedantic` で独立してコンパイルし、警告なし・入力例と期待出力の一致を確認しています。数値や経路は示した入力に対する検証例です。大きな問題や異なる実行環境で同じ解品質を保証するものではありません。時間制限を使う例の経過時間は実行ごとに変わります。

全体の見取り図です。最初の10例で経路・費用・容量・採否の基本を扱い、その後にモデルの拡張、診断と再実行、表現の応用を並べています。

| 例 | ユースケース | 主なAPI・パターン |
|---|---|---|
| 01 | 1件の配送を安い経路で運ぶ | Problem、add_arc、add_request、make_solver、Limits、solve、Result、Route、Value::total |
| 02 | 複数の配送が道路の容量を分け合う | 複数Request、evaluate、Evaluation::resource_load |
| 03 | まとまった荷物量と、1単位当たりの運賃 | Request::demand、cost_scale、Routeの出力 |
| 04 | 往復で1つの容量を共有する橋 | add_undirected_edge、Arc::uses、ResourceUse::resource |
| 05 | 乗換駅・中継設備にも費用と容量を設ける | Vertex::cost、add_vertex_capacity |
| 06 | 離れた区間が同じ通信帯域を使う | add_resource、ResourceUseの既定値、複数辺での共有 |
| 07 | 荷物量に比例する燃料と、出発準備の消費 | ResourceUse::per_unit、per_use、Request::uses |
| 08 | 各配送の電池残量と経由回数に上限を設ける | LocalLimit、Request::limits、複数資源 |
| 09 | 利益に見合う注文だけを引き受ける | Request::required、reject_cost、Objective::PenaltyPlusCost |
| 10 | 未提供サービスの重みを最優先で減らす | Objective::PenaltyThenCost、Valueの比較の意味 |
| 11 | 移動のない作業と、端点の資源消費を扱う | Request::consume_endpoints、需要0、source=target、空の接続経路 |
| 12 | 管理対象外の利用分を差し引く | Resource::background_load、全体と個別の集計範囲 |
| 13 | 予約済み経路・不採用の決定を固定する | Request::fixed_route、StopReason::FixedConflict、NoMovableRequests、refresh、resume |
| 14 | 車種・利用者ごとに通行条件や料金を変える | DefaultModel、arc_cost、vertex_cost、nullopt、make_solver<Model> |
| 15 | 混むほど高くなる料金と料金改定 | resource_cost、Solver<Model>、set_model |
| 16 | 設備を1回でも使うと開設費がかかる | 段差のあるresource_cost、平行辺 |
| 17 | 自作の解を検査し、問題箇所を特定する | evaluate、Evaluation全診断配列、LimitViolation、Summary::value |
| 18 | 既存の配送案を初期解にして改善する | improve、Solutionの末尾補完 |
| 19 | 探索を区切って続け、結果を保存する | Options、resume、Statistics::iterations、Route/Solution/Valueのoperator== |
| 20 | AHCの残り時間から探索時間を決める | Clock、Limits::time_limit_us/deadline/max_iterations、TimeLimit、Statisticsの出力 |
| 21 | ターンごとに通行止めや依頼を更新する | Arc/Vertex::enabled、Arc::cost、Request::active、refreshとresumeの組み合わせ |
| 22 | 新しい地点や道路を追加して旧解を引き継ぐ | Solver<>、Problem::vertices、reset、IDを保ったimprove |
| 23 | 外部の料金表を参照するモデルを更新する | 参照を持つModel、外部変更時のrefresh |
| 24 | 小数の距離や料金を費用にする | CapacitatedRouter<double>、ストリームの数値書式 |
| 25 | 時刻を頂点にして、便や装置の時間枠を割り当てる | 時刻展開グラフ、時刻別容量、待機辺 |
| 26 | 各要求が1回だけ使える特急券を表す | 状態展開グラフ、全体制約と個別の履歴条件 |
| 27 | 作業を担当者へ割り当てる | 割当の経路化、要求別費用表、容量による担当数上限 |
| 28 | 障害物を避け、マスが重ならない配線を引く | 格子グラフ、頂点を共有しない経路、障害物 |
| 29 | 乱数種を変えた候補から目的に合う解を選ぶ | 複数seed、Resultのコピー、目的別の外部比較、Valueの出力 |
| 30 | 結果をそのまま表示し、長い配列を省略する | 全結果型のデバッグ出力、配列20要素の省略、StopReason::IterationLimit |

### 3.1. 1件の配送を安い経路で運ぶ

**場面。** 交差点を頂点、一方通行の道路を辺と考えます。荷物を途中で分けず、出発地から目的地まで1本の道順で運びます。

**具体的に解く問題。** 0→1→2 は費用 1+1、0→2 は費用 5。各道路の容量は1、荷物量は1です。費用の小さい道順を探します。

**入力形式。** `n m`、続いて m 行の `出発頂点 到着頂点 費用 容量`、最後に `出発地 目的地 需要量`。容量 -1 は無制限。頂点は 0〜n-1。

**コード：`examples/01_one_delivery.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int n, m;
    if (!(std::cin >> n >> m)) {
        return 0;
    }

    R::Problem p(n); // n 個の頂点を作る。辺、資源、要求はまだ空。
    for (int i = 0; i < m; ++i) {
        int u, v;
        int64_t cost, capacity;
        std::cin >> u >> v >> cost >> capacity;
        p.add_arc(u, v, cost, capacity); // u→v の有向辺。費用は非負、容量 -1 は上限なし。
    }

    int s, t;
    int64_t demand;
    std::cin >> s >> t >> demand;

    // 必須要求を追加。需要を複数経路へ分割しない。
    const int request = p.add_request(s, t, demand);

    auto solver = R::make_solver(p); // p を参照するので、p を solver より長く生存させる。

    // 時間制限を外し200反復。deadline は既定で無制限。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    // 結果をコピーして保持。省略した Options の seed は0。
    const auto result = solver.solve(limits);

    if (!result.feasible()) { // 費用を読む前に、全制約を満たすか確認する。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 採否費用・経路費用・資源費用の和。
    std::cout << "cost=" << result.summary.value->total() << '\n';

    const auto& route = result.solution.routes[request]; // 要求IDと routes の添字が対応する。
    std::cout << "vertices: " << p.requests[request].source; // 経路の最初の頂点を出力。
    for (int a : route.arcs) {
        std::cout << ' ' << p.arcs[a].to; // arcs は頂点IDではなく、有向辺IDの列。
    }

    std::cout << "\narcs:";
    for (int a : route.arcs) {
        std::cout << ' ' << a; // 辺IDは add_arc の追加順。平行辺も区別できる。
    }

    std::cout << '\n';
}
```

**入力例：`inputs/01_one_delivery.txt`**

```text
3 3
0 1 1 1
1 2 1 1
0 2 5 1
0 2 1
```

**標準出力：`expected/01_one_delivery.txt`**

```text
cost=2
vertices: 0 1 2
arcs: 0 1
```

費用2の経路を得ます。辺ID 0,1 はそれぞれ 0→1、1→2 です。一般の入力で最適解が保証されるわけではありません。

### 3.2. 複数の配送が道路の容量を分け合う

**場面。** 個々の配送には安い道があっても、全件を同じ道へ入れると容量を超える場合があります。すべての配送をまとめて考えます。

**具体的に解く問題。** 例1と同じ3辺に、0→2 の配送を2件入れます。安い2辺の道には1件しか入らず、もう1件は費用5の直行路を使います。

**入力形式。** `要求数`。この入力例では2。グラフはコード内の3頂点・3辺です。

**コード：`examples/02_competing_deliveries.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int count;
    if (!(std::cin >> count)) {
        return 0;
    }

    R::Problem p(3); // 頂点0から頂点2へ配送する。

    p.add_arc(0, 1, 1, 1); // 安い道の前半。費用1、容量1。
    p.add_arc(1, 2, 1, 1); // 安い道の後半。費用1、容量1。
    p.add_arc(0, 2, 5, 1); // 直行路。費用5、容量1。

    for (int i = 0; i < count; ++i) {
        p.add_request(0, 2); // 需要の既定値は1。各要求は必須。
    }

    auto solver = R::make_solver(p); // 全要求を同じ容量の下で解く。

    // 200反復、相対時間制限なし。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 解をコピーする。

    if (!result.feasible()) { // 未接続・超過などを確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    int connected = 0;
    for (const auto& route : result.solution.routes) {
        connected += route.connected; // 接続できた要求数を数える。
    }

    std::cout << "connected=" << connected << '\n';
    std::cout << "cost=" << result.summary.value->total() << '\n'; // 合計は2+5=7。

    // 全経路を再集計し、資源別の使用量を得る。
    const auto evaluation = solver.evaluate(result.solution);

    std::cout << "loads:";
    for (int64_t load : evaluation.resource_load) {
        std::cout << ' ' << load; // 各有向辺の専用容量資源の使用量。
    }

    std::cout << '\n';
}
```

**入力例：`inputs/02_competing_deliveries.txt`**

```text
2
```

**標準出力：`expected/02_competing_deliveries.txt`**

```text
connected=2
cost=7
loads: 1 1 1
```

容量を守りながら2件を接続します。どちらの要求が直行路を選ぶかより、合計費用と使用量が重要な例です。

### 3.3. まとまった荷物量と、1単位当たりの運賃

**場面。** 2個の荷物を同じ便で運びたい場合です。容量に使う荷物量と、運賃の倍率を別々に指定します。

**具体的に解く問題。** 安い道の容量は1、直行路の容量は2。需要2は分割できないので直行路を使います。直行路の基本費用5に倍率3を掛けます。

**入力形式。** `需要量 費用倍率`。ともに非負整数。この例では需要2、倍率3。

**コード：`examples/03_batch_size.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t demand, scale;
    if (!(std::cin >> demand >> scale)) {
        return 0;
    }

    R::Problem p(3); // 3頂点の配送網。

    p.add_arc(0, 1, 1, 1); // 需要2を通すには容量不足。
    p.add_arc(1, 2, 1, 1); // 需要2を通すには容量不足。
    p.add_arc(0, 2, 5, 2); // 需要2をまとめて運べる直行路。

    const int k = p.add_request(0, 2, demand); // demand は資源消費の計算に使う。
    p.requests[k].cost_scale = scale; // 経路上の辺費用と頂点費用の合計を、この値で掛ける。

    // demand を費用倍率にしたいときも cost_scale を明示する。
    auto solver = R::make_solver(p);

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 1要求につき1本の経路を探す。

    if (!result.feasible()) { // 必須配送が成立したか確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 5×3=15。需要2では自動的に掛けない。
    std::cout << "route_cost=" << result.summary.value->route_cost << '\n';
    // Route はそのままストリームに出せる。
    std::cout << "route=" << result.solution.routes[k] << '\n';
}
```

**入力例：`inputs/03_batch_size.txt`**

```text
2 3
```

**標準出力：`expected/03_batch_size.txt`**

```text
route_cost=15
route=Route{connected=true, arcs=[2]}
```

需要を2本の経路へ1ずつ分けることはしません。分割可能な荷物を表したい場合は、意味が変わることを理解して別要求へ分けます。

### 3.4. 往復で1つの容量を共有する橋

**場面。** 橋には両方向から車が入れますが、全体として同時に2台まで、という制約を表します。方向ごとに2台ではありません。

**具体的に解く問題。** 頂点0と1を結ぶ橋の費用は片道1。0→1 と1→0 の2件を接続し、共有容量2をちょうど使います。

**入力形式。** `橋全体の容量`。非負整数。

**コード：`examples/04_two_way_bridge.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t capacity;
    if (!(std::cin >> capacity)) {
        return 0;
    }

    R::Problem p(2); // 橋の両端を頂点0と1にする。

    // 往復2辺を作り、1つの容量を共有。
    const auto [forward, backward] = p.add_undirected_edge(0, 1, 1, capacity);
    // 容量を指定したため、共用資源IDを取得できる。
    const int resource = p.arcs[forward].uses.front().resource;

    p.add_request(0, 1); // 0→1 の車1台。
    p.add_request(1, 0); // 1→0 の車1台。

    auto solver = R::make_solver(p); // 往復の消費は加算され、相殺されない。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 両要求を接続する。

    if (!result.feasible()) { // 容量1なら両方の接続は不可能。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // 橋全体の使用量を再計算。

    std::cout << "cost=" << result.summary.value->total() << '\n'; // 片道1を2件分。
    std::cout << "shared_load=" << e.resource_load[resource] << '\n'; // 両方向の需要の合計。
    (void)backward; // 戻り値の後半は1→0の辺ID。方向別に辺の属性も変更できる。
}
```

**入力例：`inputs/04_two_way_bridge.txt`**

```text
2
```

**標準出力：`expected/04_two_way_bridge.txt`**

```text
cost=2
shared_load=2
```

方向ごとに独立した容量が欲しい場合は、add_arc を2回呼びます。

### 3.5. 乗換駅・中継設備にも費用と容量を設ける

**場面。** 通信の中継器や配送センターは、道路だけでなく場所自体が混みます。通過した頂点に費用と容量を付けます。

**具体的に解く問題。** 0→1→3 は辺費用2に中継費3を加えて5、0→2→3 は費用8。頂点1は1件までなので、2件を分散させます。

**入力形式。** `頂点1の費用 頂点1の容量`。いずれも非負整数。

**コード：`examples/05_transfer_station.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t fee, capacity;
    if (!(std::cin >> fee >> capacity)) {
        return 0;
    }

    R::Problem p(4); // 出発地0、目的地3、中継地点1と2。

    p.add_arc(0, 1, 1); // 容量を省略した辺は上限なし。
    p.add_arc(1, 3, 1); // 中継地点1を通る経路の後半。
    p.add_arc(0, 2, 4); // 代替経路の前半。
    p.add_arc(2, 3, 4); // 代替経路の後半。

    p.vertices[1].cost = fee; // 頂点1を通る要求に1回ずつ加える費用。
    const int resource = p.add_vertex_capacity(1, capacity); // 頂点1の専用容量資源IDを返す。

    p.add_request(0, 3); // 需要1の必須要求。
    p.add_request(0, 3); // もう1件。頂点容量も各要求の需要で消費する。

    auto solver = R::make_solver(p); // 頂点費用は端点を含む経路上の全頂点で数える。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 中継設備の混雑を避ける経路を探す。

    if (!result.feasible()) { // 全制約を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // 頂点の資源も同じ配列で読める。

    std::cout << "cost=" << result.summary.value->total() << '\n'; // 合計は5+8=13。
    // 中継地点1を使った需要。
    std::cout << "station_load=" << e.resource_load[resource] << '\n';
}
```

**入力例：`inputs/05_transfer_station.txt`**

```text
3 1
```

**標準出力：`expected/05_transfer_station.txt`**

```text
cost=13
station_load=1
```

辺の容量と頂点の容量は同時に設定できます。端点の資源消費を除く指定は例11にあります。

### 3.6. 離れた区間が同じ通信帯域を使う

**場面。** 複数の通信リンクが同じ周波数帯や電源を使う場合、リンクごとの容量だけでは全体の上限を表せません。共有資源を1つ作り、複数の辺から消費させます。

**具体的に解く問題。** 0→1 と1→2 が同じ帯域を各1消費。帯域容量3のため、この2辺を通る通信を2件入れると4になり超過します。1件は直行路へ回します。

**入力形式。** `共有帯域の容量`。非負整数。

**コード：`examples/06_shared_band.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t capacity;
    if (!(std::cin >> capacity)) {
        return 0;
    }

    R::Problem p(3); // 2区間の通信路と直行路。

    const int band = p.add_resource(capacity); // 複数の辺から参照する共用容量を作る。

    const int a = p.add_arc(0, 1, 1); // この呼び出し自体では専用容量を作らない。
    const int b = p.add_arc(1, 2, 1); // 共用容量は後から uses に追加する。
    p.add_arc(0, 2, 5); // この辺は共用帯域を使わない。

    p.arcs[a].uses.push_back({band}); // per_unit=1、per_use=0 が既定値。需要1なら1消費。
    p.arcs[b].uses.push_back({band}); // 同じ要求が2辺を通ると、同じ資源を計2消費する。

    p.add_request(0, 2); // 通信1件目。
    p.add_request(0, 2); // 通信2件目。

    auto solver = R::make_solver(p); // 資源IDの重複は「1回だけ」ではなく、すべて足し算。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 共用容量を守る。

    if (!result.feasible()) { // 超過の有無を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // 共用帯域の総消費を読む。

    std::cout << "cost=" << result.summary.value->total() << '\n'; // 合計は2+5=7。
    std::cout << "band_load=" << e.resource_load[band] << '\n'; // 安い2辺を通る1件が2消費。
}
```

**入力例：`inputs/06_shared_band.txt`**

```text
3
```

**標準出力：`expected/06_shared_band.txt`**

```text
cost=7
band_load=2
```

同じ資源を複数回指定すると、その回数分だけ消費します。「経路で1度でも使えば1」は直接の uses だけでは表せません。

### 3.7. 荷物量に比例する燃料と、出発準備の消費

**場面。** 燃料には積載量に比例する分と、走行のたびに必要な分があります。さらに、どの道を選んでも出発準備に使う燃料を要求に付けます。

**具体的に解く問題。** 1本の道路で 2×需要+1 消費し、出発準備で2消費。需要2なら合計7となり、燃料の全体上限7を使い切ります。

**入力形式。** `需要量 燃料容量`。非負整数。

**コード：`examples/07_variable_and_fixed_consumption.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t demand, capacity;
    if (!(std::cin >> demand >> capacity)) {
        return 0;
    }

    R::Problem p(2); // 出発地0から目的地1へ1本の道路。

    const int fuel = p.add_resource(capacity); // 全要求で共有する燃料の上限。

    const int a = p.add_arc(0, 1); // 費用は既定値1、辺の専用容量は無制限。

    p.arcs[a].uses.push_back({fuel, 2, 1}); // 走行1回につき 2×demand+1 消費。

    const int k = p.add_request(0, 1, demand); // 需要量を指定。
    // 接続時に1回だけ2消費。未接続なら消費しない。
    p.requests[k].uses.push_back({fuel, 0, 2});

    auto solver = R::make_solver(p); // 辺・頂点・要求の uses はすべて合算する。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 燃料を守る接続を探す。

    if (!result.feasible()) { // 容量不足のまま費用を使わない。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // 資源消費を確認。

    std::cout << "fuel=" << e.resource_load[fuel] << '\n'; // 合計は2×2+1+2=7。
    // 燃料消費がそのまま費用に加わるわけではない。
    std::cout << "cost=" << result.summary.value->total() << '\n';
}
```

**入力例：`inputs/07_variable_and_fixed_consumption.txt`**

```text
2 7
```

**標準出力：`expected/07_variable_and_fixed_consumption.txt`**

```text
fuel=7
cost=1
```

消費量と費用は別です。燃料1単位ごとの料金も加えたいなら、resource_cost で定義できます。

### 3.8. 各配送の電池残量と経由回数に上限を設ける

**場面。** 全車の電池を合計するのではなく、「この車は電池4まで、走行1区間まで」と個別に制限します。全体容量と個別上限は独立です。

**具体的に解く問題。** 安い2区間の道は電池6・区間数2、直行路は電池4・区間数1。個別上限を満たす直行路を選びます。

**入力形式。** `電池上限 区間数上限`。非負整数。

**コード：`examples/08_individual_limits.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t battery, hops;
    if (!(std::cin >> battery >> hops)) {
        return 0;
    }

    R::Problem p(3); // 2区間の安い道と、1区間の高い道。

    const int fuel = p.add_resource(); // 全体容量は既定値-1で無制限。個別上限だけに使う。
    const int hop = p.add_resource(); // 区間数を数える別の資源。

    const int a = p.add_arc(0, 1, 1); // 安い道の前半。
    const int b = p.add_arc(1, 2, 1); // 安い道の後半。
    const int c = p.add_arc(0, 2, 5); // 直行路。

    p.arcs[a].uses = {{fuel, 0, 3}, {hop, 0, 1}}; // この辺は電池3、区間数1を消費。
    p.arcs[b].uses = {{fuel, 0, 3}, {hop, 0, 1}}; // 2辺を通ると電池6、区間数2。
    p.arcs[c].uses = {{fuel, 0, 4}, {hop, 0, 1}}; // 直行路なら電池4、区間数1。

    const int k = p.add_request(0, 2); // 需要は1。
    // 同じ要求の limits で資源IDを重複させない。
    p.requests[k].limits = {{fuel, battery}, {hop, hops}};

    auto solver = R::make_solver(p); // 個別上限には他の要求や background_load を足さない。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 2種類の個別上限を同時に守る。

    if (!result.feasible()) { // 個別制約も feasible に含まれる。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // この例は1要求なので全体使用量と個別消費が一致。
    const auto e = solver.evaluate(result.solution);

    std::cout << "cost=" << result.summary.value->total() << '\n'; // 直行路の費用5。
    // 4と1。
    std::cout
        << "fuel=" << e.resource_load[fuel]
        << " hops=" << e.resource_load[hop]
        << '\n';
}
```

**入力例：`inputs/08_individual_limits.txt`**

```text
4 1
```

**標準出力：`expected/08_individual_limits.txt`**

```text
cost=5
fuel=4 hops=1
```

需要に関係なく1区間ごとに加える量は per_unit=0、per_use=量 です。LocalLimit は加算型の制約なので、最大値や積の制約をそのまま指定するものではありません。

### 3.9. 利益に見合う注文だけを引き受ける

**場面。** 注文を断ると失う利益を「未接続費用」と考えます。配送費より失う利益が大きい注文を優先すると、利益を守りつつ配送費を抑えられます。

**具体的に解く問題。** 費用5・容量1の道路に、断る費用3と8の2注文があります。8の注文だけ運ぶと、配送費5と未接続費用3で合計8です。

**入力形式。** `注文0の未接続費用 注文1の未接続費用`。非負整数。

**コード：`examples/09_optional_orders.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t low, high;
    if (!(std::cin >> low >> high)) {
        return 0;
    }

    R::Problem p(2); // 注文は同じ出発地0と目的地1。

    p.add_arc(0, 1, 5, 1); // 1注文だけ運べる。配送費5。
    const std::vector<int64_t> penalties{low, high};
    for (int64_t penalty : penalties) {
        const int k = p.add_request(0, 1); // 既定では必須なので、次の行で任意に変える。
        p.requests[k].required = false; // 断っても制約違反にはならない。
        // 断った場合のみ加わる費用。負の利益としては渡さない。
        p.requests[k].reject_cost = penalty;
    }

    auto solver = R::make_solver(p); // 既定目的は未接続費用+経路費用+資源費用の最小化。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 注文の採否と経路を同時に選ぶ。

    if (!result.feasible()) { // 任意注文の未接続自体は実行可能。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << "connected:";
    for (const auto& route : result.solution.routes) {
        std::cout << ' ' << route.connected; // 1は採用、0は不採用。
    }

    const auto& value = *result.summary.value; // feasible を確認済みなので値を読める。
    // 内訳と和。
    std::cout
        << "\nreject=" << value.reject_cost
        << " route=" << value.route_cost
        << " total=" << value.total()
        << '\n';
}
```

**入力例：`inputs/09_optional_orders.txt`**

```text
3 8
```

**標準出力：`expected/09_optional_orders.txt`**

```text
connected: 0 1
reject=3 route=5 total=8
```

全注文の利益の和は一定なので、「未接続の利益+配送費」を小さくすることは「採用した利益−配送費」を大きくすることに対応します。負の辺費用は不要です。

### 3.10. 未提供サービスの重みを最優先で減らす

**場面。** 費用を節約するより、サービスを断る損失を先に減らしたい場合です。まず未接続費用、その同点時だけ経路と資源の費用を比べます。

**具体的に解く問題。** 道路費5、断る費用3の任意要求1件。和なら断る方が安い一方、この例の目的では未接続費用を0にするため接続します。

**入力形式。** `未接続費用 道路費`。非負整数。

**コード：`examples/10_service_priority.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t penalty, cost;
    if (!(std::cin >> penalty >> cost)) {
        return 0;
    }

    R::Problem p(2); // 1本の道路と1件のサービス要求。

    p.add_arc(0, 1, cost); // 経路費用は非負。

    const int k = p.add_request(0, 1); // 需要1の要求を作る。
    p.requests[k].required = false; // 未接続も許容する。
    p.requests[k].reject_cost = penalty; // 比較の第一順位となる未接続費用。
    // (未接続費用, 経路費用+資源費用) の順で比較する。
    p.objective = R::Objective::PenaltyThenCost;

    // これは要求数最大化ではない。重み0の要求は第一順位に影響しない。
    auto solver = R::make_solver(p);

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 未接続費用を先に小さくする。

    if (!result.feasible()) { // 容量などの制約は依然として必須。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto& value = *result.summary.value; // 目的の2成分を取り出せる。

    // この例では接続する。
    std::cout << "connected=" << result.solution.routes[k].connected << '\n';
    // total だけで解の優劣を比べない。
    std::cout
        << "reject=" << value.reject_cost
        << " route=" << value.route_cost
        << " total=" << value.total()
        << '\n';
}
```

**入力例：`inputs/10_service_priority.txt`**

```text
3 5
```

**標準出力：`expected/10_service_priority.txt`**

```text
connected=1
reject=0 route=5 total=5
```

辞書順は、辞書で前の文字から順番に比べるのと同じ考え方です。この目的で外部の解を選ぶ場合、total の大小だけでは正しく比較できません（例29）。

### 3.11. 移動のない作業と、端点の資源消費を扱う

**場面。** 同じ場所で行う作業も、出発地と目的地が同じ要求として表せます。移動量は0でも、受付枠を1つ使い、作業費を払う例です。

**具体的に解く問題。** 頂点0だけの問題で、作業費7、受付枠1。端点に付けた通過設備は容量0ですが、この作業では端点の資源消費を除外します。

**入力形式。** `作業費`。非負整数。

**コード：`examples/11_local_job.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t fee;
    if (!(std::cin >> fee)) {
        return 0;
    }

    R::Problem p(1); // 1頂点。移動しない作業を扱う。

    const int station = p.add_resource(0); // 通過設備は利用不可。
    const int slots = p.add_resource(1); // 同時に受け付けられる作業は1件。

    p.vertices[0].cost = fee; // 端点の費用は後の設定でも除外されない。
    p.vertices[0].uses.push_back({station, 0, 1}); // 通常は通過1回につき設備を1消費。

    const int k = p.add_request(0, 0, 0); // source=target、demand=0 でも接続作業を表せる。
    // 端点の頂点資源だけを除外。費用・禁止判定は残る。
    p.requests[k].consume_endpoints = false;
    p.requests[k].uses.push_back({slots, 0, 1}); // 需要0でも、接続した要求は受付枠1を使う。

    auto solver = R::make_solver(p); // 接続済みで空の辺列は、同じ頂点間の要求で有効。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 移動を伴わない作業を接続状態にする。

    if (!result.feasible()) { // 必須作業の実行を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto& route = result.solution.routes[k]; // connected=false の空経路とは区別する。

    const auto e = solver.evaluate(result.solution); // 端点消費0、要求の消費1を確認。

    // 接続済みでも辺数0。
    std::cout
        << "connected=" << route.connected
        << " arc_count=" << route.arcs.size()
        << '\n';

    // 頂点費用は1回だけ。
    std::cout
        << "cost=" << result.summary.value->total()
        << " station=" << e.resource_load[station]
        << " slots=" << e.resource_load[slots]
        << '\n';
}
```

**入力例：`inputs/11_local_job.txt`**

```text
7
```

**標準出力：`expected/11_local_job.txt`**

```text
connected=1 arc_count=0
cost=7 station=0 slots=1
```

consume_endpoints=false は、辺の資源や要求の資源、端点の費用を消しません。始点と終点が同じ場合、頂点費用は1回だけです。

### 3.12. 管理対象外の利用分を差し引く

**場面。** 外部の利用者がすでに容量を使っており、その経路を管理しない場合です。外部利用量だけを background_load に置きます。

**具体的に解く問題。** 容量3の回線を外部が2使用。新しい通信1件は1消費し、個別上限1を満たしながら全体使用量3になります。

**入力形式。** `全体容量 外部使用量`。容量は非負整数、外部使用量は非負整数。

**コード：`examples/12_background_load.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t capacity, background;
    if (!(std::cin >> capacity >> background)) {
        return 0;
    }

    R::Problem p(2); // 管理する通信は0→1。

    // 外部利用を経路なしで全体容量へ加える。
    const int r = p.add_resource(capacity, background);

    const int a = p.add_arc(0, 1); // 費用は既定値1。

    p.arcs[a].uses.push_back({r}); // 通信1件の需要1を加える。

    const int k = p.add_request(0, 1); // 新規通信を1件だけ解く。
    p.requests[k].limits.push_back({r, 1}); // 個別上限には外部利用2を含めない。

    // 外部の経路費用は加算しない。資源費用には外部利用も含む。
    auto solver = R::make_solver(p);

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 残容量の範囲で接続する。

    if (!result.feasible()) { // 背景使用だけで容量超過なら FixedConflict。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // resource_load は背景使用を含む。

    std::cout << "total_load=" << e.resource_load[r] << '\n'; // 合計は2+1=3。

    // 個別消費1なので上限内。
    std::cout
        << "local_ok=" << e.local_violations.empty()
        << " cost=" << result.summary.value->total()
        << '\n';
}
```

**入力例：`inputs/12_background_load.txt`**

```text
3 2
```

**標準出力：`expected/12_background_load.txt`**

```text
total_load=3
local_ok=1 cost=1
```

外部利用も後から変更して再計算できます。その場合は refresh を呼びます。固定経路として管理する必要がある場合は次の例を使います。

### 3.13. 予約済み経路・不採用の決定を固定する

**場面。** 一度利用者に約束した経路は、費用が高くても勝手に変えられないことがあります。固定・固定解除・固定と必須の矛盾を確認します。

**具体的に解く問題。** 安い道は費用2、予約した直行路は費用5。予約を守る場合、接続禁止にした場合、任意要求として不採用を確定した場合、予約を解除した場合を順に表示します。

**入力形式。** `不採用時の費用`。非負整数。

**コード：`examples/13_commitments.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t penalty;
    if (!(std::cin >> penalty)) {
        return 0;
    }

    R::Problem p(3); // 予約経路と代替経路を持つ問題。

    p.add_arc(0, 1, 1); // 安い道の前半。
    p.add_arc(1, 2, 1); // 安い道の後半。
    const int direct = p.add_arc(0, 2, 5); // 予約対象の直行路。

    const int k = p.add_request(0, 2); // 既定では必須。
    p.requests[k].fixed_route = R::Route{true, {direct}}; // 接続状態と辺列を固定する。

    auto solver = R::make_solver(p); // 固定経路も通常どおり費用と容量を使う。

    // 200反復が上限。全件固定なら探索せず止まる。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    auto result = solver.solve(limits); // まず予約を守った結果。

    if (!result.feasible()) { // 固定経路の有効性も確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 停止理由を名前で出す。
    std::cout
        << "reserved=" << result.summary.value->total()
        << " stop=" << result.stop_reason
        << '\n';

    p.requests[k].fixed_route = R::Route{}; // 「未接続で固定」。固定解除ではない。

    solver.refresh(); // 要求属性を変更したので、既存状態を再評価する。

    result = solver.resume(limits); // 必須要求の未接続固定は矛盾なので修復できない。

    // FixedConflict を確認。
    std::cout
        << "conflict=" << result.stop_reason
        << " feasible=" << result.feasible()
        << '\n';

    p.requests[k].required = false; // 任意要求なら未接続固定が有効。
    p.requests[k].reject_cost = penalty; // 不採用の費用を設定。

    solver.refresh(); // 新しい採否条件を反映。

    result = solver.resume(limits); // 不採用の決定をそのまま守る。

    if (!result.feasible()) { // 実行可能性を再確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 例では未接続費用7。
    std::cout
        << "rejected=" << result.summary.value->total()
        << " stop=" << result.stop_reason
        << '\n';

    p.requests[k].fixed_route.reset(); // optional を空にして初めて固定解除。
    p.requests[k].required = true; // 再び接続必須にする。

    solver.refresh(); // 解除した要求を動かせる状態にする。

    result = solver.resume(limits); // 既存状態から安い経路を探す。

    if (!result.feasible()) { // 必須接続の成功を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 費用2の道へ変更できる。
    std::cout << "unlocked=" << result.summary.value->total() << '\n';
}
```

**入力例：`inputs/13_commitments.txt`**

```text
7
```

**標準出力：`expected/13_commitments.txt`**

```text
reserved=5 stop=NoMovableRequests
conflict=FixedConflict feasible=0
rejected=7 stop=NoMovableRequests
unlocked=2
```

fixed_route の「値なし」と「未接続の Route が入っている」は別の状態です。固定された要求の不整合は、探索時間を増やしても解消しません。

### 3.14. 車種・利用者ごとに通行条件や料金を変える

**場面。** 小型車と大型車、会員と非会員などでは、同じ道でも通れるか、支払う料金が異なります。Model で要求IDごとの条件を定義します。

**具体的に解く問題。** 0→1→3 は辺費用2、0→2→3 は辺費用3。要求0は頂点2を使えず頂点1で追加料金2、要求1は辺0を使えません。費用は4と3です。

**入力形式。** `要求0の頂点1追加料金`。非負整数。

**コード：`examples/14_request_specific_rules.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

struct Model : R::DefaultModel { // 変更しない resource_cost は既定の0を使う。
    int64_t toll;

    explicit Model(int64_t x) : toll(x) {}

    // 戻り値は倍率を掛ける前の非負費用。
    std::optional<int64_t> arc_cost(
        const R::Problem& p,
        int k,
        int a
    ) const {
        if (k == 1 && a == 0) {
            return std::nullopt; // 要求1のみ、この辺を通行禁止。
        }

        return p.arcs[a].cost; // それ以外は辺に格納した費用。
    }

    // 頂点にも要求別の条件を付けられる。
    std::optional<int64_t> vertex_cost(
        const R::Problem& p,
        int k,
        int v
    ) const {
        if (k == 0 && v == 2) {
            return std::nullopt; // 要求0のみ、頂点2を禁止。
        }

        // 頂点1の追加料金は要求0だけ。
        return p.vertices[v].cost + ((k == 0 && v == 1) ? toll : 0);
    }
};

int main() {
    int64_t toll;
    if (!(std::cin >> toll)) {
        return 0;
    }

    R::Problem p(4); // 出発0、目的3、中継1と2。

    p.add_arc(0, 1, 1); // 辺ID0。要求1には禁止する。
    p.add_arc(1, 3, 1); // 頂点1側の後半。
    p.add_arc(0, 2, 1); // 頂点2側の前半。
    p.add_arc(2, 3, 2); // 頂点2側の後半。

    p.add_request(0, 3); // 要求0。頂点2を禁止し、頂点1で追加料金。
    p.add_request(0, 3); // 要求1。辺0を禁止。

    Model model(toll); // コールバックは同じ入力なら同じ結果を返すこと。

    auto solver = R::make_solver(p, model); // 型を推論してカスタムモデルを値で保持する。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 要求ごとの料金と禁止条件を使う。

    if (!result.feasible()) { // 禁止区間を含まないことも確認される。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << "route_cost=" << result.summary.value->route_cost << '\n'; // 合計4+3=7。

    std::cout << "routes:";
    for (int k = 0; k < 2; ++k) {
        // 始点の費用も1回含める。
        int64_t cost = *model.vertex_cost(p, k, p.requests[k].source);
        for (int a : result.solution.routes[k].arcs) {
            // 許可された経路なので値がある。
            cost += *model.arc_cost(p, k, a)
                + *model.vertex_cost(p, k, p.arcs[a].to);
        }

        std::cout << ' ' << cost * p.requests[k].cost_scale; // 最後に要求の倍率を掛ける。
    }

    std::cout << '\n';
}
```

**入力例：`inputs/14_request_specific_rules.txt`**

```text
2
```

**標準出力：`expected/14_request_specific_rules.txt`**

```text
route_cost=7
routes: 4 3
```

nullopt はその要求にとって通行禁止という意味です。費用を非常に大きくして禁止を代用する必要はありません。全員を禁止する enabled=false も常に有効です。

### 3.15. 混むほど高くなる料金と料金改定

**場面。** 回線や設備を集中利用すると追加費用が大きくなる状況です。利用量の2乗の費用を付けると、分散する価値が生まれます。

**具体的に解く問題。** 共用設備を使う経路は費用1、使わない代替経路は費用3。2要求について設備費を係数×使用量の2乗とし、係数1から4へ改定します。

**入力形式。** `最初の係数 改定後の係数`。非負整数。積が int64_t に収まる範囲。

**コード：`examples/15_congestion_price.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

struct Model : R::DefaultModel { // 辺・頂点費用は既定のものを使う。
    int64_t factor;

    explicit Model(int64_t x) : factor(x) {}

    // 第2引数は資源ID。この例は資源1個。
    int64_t resource_cost(const R::Problem&, int, int64_t load) const {
        return factor * load * load; // 係数は非負。中間積も int64_t に収まる入力を使う。
    }
};

int main() {
    int64_t first, second;
    if (!(std::cin >> first >> second)) {
        return 0;
    }

    R::Problem p(2); // 並行する2本の経路。

    const int r = p.add_resource(2); // 最大2件が使う設備。

    const int cheap = p.add_arc(0, 1, 1); // 基本費用の安い経路。

    p.arcs[cheap].uses.push_back({r}); // この経路だけ設備を消費する。

    p.add_arc(0, 1, 3); // 設備を使わない代替経路。

    p.add_request(0, 1); // 必須要求1件目。
    p.add_request(0, 1); // 必須要求2件目。

    // make_solver を使わず、モデル型を明示してもよい。
    R::Solver<Model> solver(p, Model(first));

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    auto result = solver.solve(limits); // 最初の料金で解く。

    if (!result.feasible()) { // 費用を読む前に確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    auto e = solver.evaluate(result.solution); // 設備利用量を読む。

    // 合計は4+1=5。
    std::cout
        << "before=" << result.summary.value->total()
        << " load=" << e.resource_load[r]
        << '\n';

    // 同じ型のモデルに置換し、既存経路も再評価。追加の refresh は不要。
    solver.set_model(Model(second));

    result = solver.resume(limits); // 新しい料金で既存状態から改善する。

    if (!result.feasible()) { // 更新後も確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    e = solver.evaluate(result.solution); // 新料金下の使用量を再計算。

    // 代替経路2件で6。
    std::cout
        << "after=" << result.summary.value->total()
        << " load=" << e.resource_load[r]
        << '\n';
}
```

**入力例：`inputs/15_congestion_price.txt`**

```text
1 4
```

**標準出力：`expected/15_congestion_price.txt`**

```text
before=5 load=1
after=6 load=0
```

資源費用は各要求へ個別に課金して足すのではなく、全要求と背景使用を足した後に1回計算します。非負・非減少・使用量0で費用0という条件を守ってください。

### 3.16. 設備を1回でも使うと開設費がかかる

**場面。** 共同配送センターの開設費は、利用者ごとではなく設備全体に1回だけ払います。利用しなければ費用0、使えば定額という資源費用で表せます。

**具体的に解く問題。** 共同設備を使う配送は1件あたり費用1、個別配送は費用4。開設費5で2件を共同設備へ集めると合計7、個別配送2件なら8です。2件をまとめた初期案を渡し、費用を評価しながら改善します。

**入力形式。** `開設費`。非負整数。

**コード：`examples/16_opening_fee.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

struct Model : R::DefaultModel { // 標準の辺・頂点費用を継承する。
    int64_t fee;

    explicit Model(int64_t x) : fee(x) {}

    // 使用量には背景使用も含まれる。
    int64_t resource_cost(const R::Problem&, int, int64_t load) const {
        return load == 0 ? 0 : fee; // f(0)=0 を守り、使用があれば開設費を1回課す。
    }
};

int main() {
    int64_t fee;
    if (!(std::cin >> fee)) {
        return 0;
    }

    R::Problem p(2); // 共同配送と個別配送を平行辺にする。

    const int r = p.add_resource(2); // 共同設備は2件まで。

    const int common = p.add_arc(0, 1, 1); // 共同配送の1件当たり費用。

    p.arcs[common].uses.push_back({r}); // 共同設備を使った量を集計する。

    p.add_arc(0, 1, 4); // 個別配送は共同設備を使わない。

    p.add_request(0, 1); // 必須配送1件目。
    p.add_request(0, 1); // 必須配送2件目。

    // 開設費は要求ごとでなく、総使用量から1回計算する。
    auto solver = R::make_solver(p, Model(fee));

    // 200反復。一般の開設問題の最適性は保証しない。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    R::Solution initial; // 開設費は単独利用では割高なので、まとめて利用する初期案を用意。
    // 2要求とも共同設備を使う、容量内の接続経路。
    initial.routes = {{true, {common}}, {true, {common}}};

    // 開設の有無は使用量が0かどうかで決まる。
    const auto result = solver.improve(initial, limits);

    if (!result.feasible()) { // 全配送の成立を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto& value = *result.summary.value; // 費用の内訳を取得。

    const auto e = solver.evaluate(result.solution); // 共同設備の使用量を取得。

    // 合計は2+5=7。
    std::cout
        << "route=" << value.route_cost
        << " facility=" << value.resource_cost
        << " total=" << value.total()
        << '\n';
    std::cout << "load=" << e.resource_load[r] << '\n'; // 2件をまとめる。
}
```

**入力例：`inputs/16_opening_fee.txt`**

```text
5
```

**標準出力：`expected/16_opening_fee.txt`**

```text
route=2 facility=5 total=7
load=2
```

この料金は、使用量が増えても値が減らないので利用できます。ただし固定費のある問題では、新規探索だけで有利な共同利用を発見しにくい場合があります。この例は2件をまとめた案を improve に渡します。初期案がない任意の開設問題を最適に解けるとは限りません。利用量が増えると総料金自体が下がる関数は、モデルの条件に反します。

### 3.17. 自作の解を検査し、問題箇所を特定する

**場面。** 別のプログラムで作った経路や、手で用意した初期解が条件を満たすか確認する用途です。solve を呼ばなくても evaluate だけ使えます。

**具体的に解く問題。** 電池容量5に対し6使用、要求0の個別上限4を超過、要求1は必須なのに未接続、要求2は通行禁止の辺を使用、要求0は予約経路と不一致、という5種類の問題を含む解を調べます。

**入力形式。** `全体電池容量 要求0の個別上限`。非負整数。

**コード：`examples/17_diagnose_solution.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t capacity, upper;
    if (!(std::cin >> capacity >> upper)) {
        return 0;
    }

    R::Problem p(3); // 経路の形自体は正しい解を検査する。

    const int fuel = p.add_resource(capacity); // 資源ID0。

    const int a = p.add_arc(0, 1, 1); // 辺ID0。
    const int b = p.add_arc(1, 2, 1); // 辺ID1。
    const int c = p.add_arc(0, 2, 5); // 辺ID2。

    p.arcs[a].uses = {{fuel, 0, 3}}; // 前半で電池3。
    p.arcs[b].uses = {{fuel, 0, 3}}; // 後半で電池3。
    p.arcs[c].enabled = false; // 全要求に対して直行路を禁止。

    for (int i = 0; i < 3; ++i) {
        p.add_request(0, 2); // 必須要求を3件。
    }

    p.requests[0].limits = {{fuel, upper}}; // 要求0だけ個別上限。
    p.requests[0].fixed_route = R::Route{true, {c}}; // 予約内容と異なる解も検査できる。

    auto solver = R::make_solver(p); // evaluate の前に solve は不要。

    R::Solution candidate; // 自作解を作る。
    // 要求数と同じ3経路。未接続は空の Route。
    candidate.routes = {{true, {a, b}}, {}, {true, {c}}};

    // 経路ID範囲・連続性・単純路などの構造は呼び出し側で守る。
    const auto e = solver.evaluate(candidate);

    // 禁止・全体容量超過では value が空。
    std::cout
        << "feasible=" << e.feasible()
        << " value=" << e.summary.value.has_value()
        << '\n';

    std::cout << "missing:";
    for (int k : e.missing_required) {
        std::cout << ' ' << k; // 必須なのに未接続の要求ID。
    }

    std::cout << "\noverloaded:";
    for (int r : e.overloaded_resources) {
        std::cout << ' ' << r; // 容量を超えた資源ID。
    }

    std::cout << "\nforbidden:";
    for (int k : e.forbidden_requests) {
        std::cout << ' ' << k; // 使用禁止を含む要求ID。
    }

    std::cout << "\nfixed:";
    for (int k : e.fixed_mismatches) {
        std::cout << ' ' << k; // 固定内容と異なる要求ID。
    }

    std::cout << '\n';
    for (const auto& violation : e.local_violations) {
        std::cout << violation << '\n'; // 要求ID・資源ID・使用量・上限を表示。
    }

    R::Solution empty; // 全要求を未接続にした別の解。
    empty.routes.resize(p.requests.size()); // evaluate では経路数の省略をしない。

    // 必須未接続や固定不一致だけなら value は存在し得る。
    const auto empty_e = solver.evaluate(empty);

    // value の存在を成功判定に使わない。
    std::cout
        << "empty_feasible=" << empty_e.feasible()
        << " empty_value=" << empty_e.summary.value.has_value()
        << '\n';
}
```

**入力例：`inputs/17_diagnose_solution.txt`**

```text
5 4
```

**標準出力：`expected/17_diagnose_solution.txt`**

```text
feasible=0 value=0
missing: 1
overloaded: 0
forbidden: 2
fixed: 0
LimitViolation{request=0, resource=0, used=6, upper=4}
empty_feasible=0 empty_value=1
```

evaluate は不正な入力を何でも安全に受け入れる検証器ではありません。有効範囲の辺IDで、始点から終点につながる単純路を渡します。制約違反と経路構造の破損は区別してください。

### 3.18. 既存の配送案を初期解にして改善する

**場面。** 人が作った案や他のソルバーの案を引き継ぎます。後から増えた要求については、初期解の末尾を省略できます。

**具体的に解く問題。** 容量1の安い2区間経路と容量1の直行路に2件を入れます。初期解は要求0を直行路に割り当て、要求1を省略しています。改善後は両方を接続します。

**入力形式。** `初期経路の辺数` と、その個数の `辺ID`。ここでは直行路のID2を指定。

**コード：`examples/18_external_start.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int count;
    if (!(std::cin >> count)) {
        return 0;
    }

    R::Problem p(3); // 0→2 の配送を2件解く。

    p.add_arc(0, 1, 1, 1); // 辺ID0、安い道の前半。
    p.add_arc(1, 2, 1, 1); // 辺ID1、安い道の後半。
    p.add_arc(0, 2, 5, 1); // 辺ID2、直行路。

    p.add_request(0, 2); // 初期経路を与える要求0。
    p.add_request(0, 2); // 初期経路を省略する要求1。

    R::Route route; // 1要求分の経路。
    route.connected = true; // 今回の入力は接続済みの正しい単純路を指定する。
    route.arcs.resize(count); // 辺IDを保持する。空なら source=target の場合だけ有効。
    for (int& a : route.arcs) {
        std::cin >> a; // 頂点列ではなく辺ID列を読み込む。
    }

    R::Solution initial; // 全体の初期解。
    // 要求0のみ指定。要求1の末尾省略は improve なら未接続で補われる。
    initial.routes.push_back(route);

    auto solver = R::make_solver(p); // 初期解の経路IDはこの Problem に対応させる。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    // 外部解から開始。初期解自体は書き換えない。
    const auto result = solver.improve(initial, limits);

    if (!result.feasible()) { // 改善後に全要求の成立を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    int connected = 0;
    for (const auto& r : result.solution.routes) {
        connected += r.connected; // 補われた要求1も含む。
    }

    // 費用7、接続2件。
    std::cout
        << "cost=" << result.summary.value->total()
        << " connected=" << connected
        << '\n';
}
```

**入力例：`inputs/18_external_start.txt`**

```text
1 2
```

**標準出力：`expected/18_external_start.txt`**

```text
cost=7 connected=2
```

途中の要求だけを省略して詰めることはできません。routes[k] は常に要求kです。途中を未接続にするなら、その位置へ Route{} を置きます。

### 3.19. 探索を区切って続け、結果を保存する

**場面。** 途中結果を表示したり、他の処理へ制御を戻した後に、同じ探索を続ける用途です。結果をコピーしておけば、次の呼び出し後も前の解を比較できます。

**具体的に解く問題。** 2件の配送を20反復で解き、その後80反復を追加。別の solver の100反復と同じ乱数種で比較します。

**入力形式。** `乱数種`。uint64_t に収まる非負整数。

**コード：`examples/19_continue_search.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    uint64_t seed;
    if (!(std::cin >> seed)) {
        return 0;
    }

    R::Problem p(3); // 2本の代替経路を持つ配送網。

    p.add_arc(0, 1, 1, 1); // 安い道の前半。
    p.add_arc(1, 2, 1, 1); // 安い道の後半。
    p.add_arc(0, 2, 5, 1); // 直行路。

    p.add_request(0, 2); // 要求0。
    p.add_request(0, 2); // 要求1。

    auto solver = R::make_solver(p); // 継続して使う solver。

    const R::Options options{.seed = seed}; // solve / improve の開始時に使う乱数種。
    // 最初の呼び出しの反復数。
    const R::Limits first_limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 20 // この呼び出しの最大反復数。
    };

    // const auto& ではなくコピーで前の結果を保存。
    const auto first = solver.solve(first_limits, options);

    // 追加の呼び出しに80反復。
    const R::Limits next_limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 80 // この呼び出しの最大反復数。
    };

    // 乱数状態と探索状態を引き継ぐ。seed を渡し直さない。
    const auto next = solver.resume(next_limits);

    auto another = R::make_solver(p); // 比較用に別の solver を作る。

    // まとめて100反復。
    const R::Limits total_limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 100 // この呼び出しの最大反復数。
    };

    // 同じ環境・同じ入力・固定反復数なら再現性を比較できる。
    const auto whole = another.solve(total_limits, options);

    if (!next.feasible() || !whole.feasible()) { // 両結果の有効性を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 統計は各呼び出し分。
    std::cout
        << "first_iterations=" << first.statistics.iterations
        << " next_iterations=" << next.statistics.iterations
        << '\n';

    std::cout << "cost=" << next.summary.value->total(); // 継続後の費用。

    // Solution の厳密なフィールド比較。
    std::cout << " same_solution=" << (next.solution == whole.solution);

    // Value の各費用を厳密比較。
    std::cout
        << " same_value=" << (*next.summary.value == *whole.summary.value);

    // Route の接続状態と辺列を比較。
    std::cout
        << " same_route="
        << (next.solution.routes[0] == whole.solution.routes[0])
        << '\n';
}
```

**入力例：`inputs/19_continue_search.txt`**

```text
42
```

**標準出力：`expected/19_continue_search.txt`**

```text
first_iterations=20 next_iterations=80
cost=7 same_solution=1 same_value=1 same_route=1
```

resume は直前の solve / improve が必要です。時間制限では実行できる反復数が変わるため、同じ結果になるとは限りません。異なる標準ライブラリや実行環境の間での再現性も保証しません。

### 3.20. AHCの残り時間から探索時間を決める

**場面。** ターン全体の締切と、この探索に割り当てる短い時間の両方を指定します。開始時点ですでに有効な解を持たせると、時間がなくても返却解を確認しやすくなります。

**具体的に解く問題。** 費用1の道路1本、既知の接続経路1本を渡し、相対時間と絶対締切の早い方まで改善します。出力の費用は時間によらず1です。

**入力形式。** `探索時間µs 全体の残り時間µs`。非負整数。入力例は2000µsと20000µs。

**コード：`examples/20_time_budget.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t search_us, remaining_us;
    if (!(std::cin >> search_us >> remaining_us)) {
        return 0;
    }

    // 現在からの絶対締切。通常の競技時間の範囲で使う。
    const auto deadline =
        R::Clock::now() + std::chrono::microseconds(remaining_us);

    R::Problem p(2); // 準備時間も絶対締切までの残り時間を減らす。

    const int a = p.add_arc(0, 1, 1); // 費用1、容量無制限。

    p.add_request(0, 1); // 必須要求1件。

    R::Solution initial; // 既知の実行可能解を用意する。
    initial.routes = {{true, {a}}}; // 時間0でも初期化で取り込めるよう正しい経路を渡す。

    auto solver = R::make_solver(p); // 構築処理自体は solve / improve の相対時間の外。

    R::Limits limits; // 既定は time_limit_us=100000、deadline=最大値、max_iterations=-1。
    limits.time_limit_us = search_us; // マイクロ秒単位の相対制限。0も有効、-1は無効化。
    limits.deadline = deadline; // この時刻と相対制限のうち早い方を使う。
    limits.max_iterations = -1; // 反復数の上限を設けず、時間で停止する。

    // 協調的な停止なので初期化・保存などで時間を超えることがある。
    const auto result = solver.improve(initial, limits);

    if (!result.feasible()) { // 時間切れ自体は失敗を意味しない。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 実行可能な初期解を保持。
    std::cout
        << "feasible=" << result.feasible()
        << " cost=" << result.summary.value->total()
        << '\n';

    // この例には可動要求があり、反復上限なしなので TimeLimit。
    std::cout << "stop=" << result.stop_reason << '\n';
    std::cerr << result.statistics << '\n'; // elapsed_us などは実測値なので実行ごとに変わる。

    // AHC全体の制限には余裕を残す。マイクロ秒指定は、同精度の終了保証を意味しない。
}
```

**入力例：`inputs/20_time_budget.txt`**

```text
2000 20000
```

**標準出力：`expected/20_time_budget.txt`**

```text
feasible=1 cost=1
stop=TimeLimit
```

標準エラーには Statistics も出ます。elapsed_us はこの呼び出し内の経過時間で、出力時間などは含みません。標準出力の例だけを以下に示します。

### 3.21. ターンごとに通行止めや依頼を更新する

**場面。** 同じ地図で道路の開閉、拠点の閉鎖、依頼の終了・追加が起こる場面です。頂点数・辺数・辺の両端を変えなければ、refresh で既存の計算領域と経路を引き継げます。

**具体的に解く問題。** 最初は費用2の経路。次に道路を閉鎖して直行費を5へ変更。道路が開いても中継点を閉鎖すれば費用5。最後に元の依頼を終了し、中継点を開いて1→2の新依頼を解きます。

**入力形式。** `更新後の直行費`。非負整数。

**コード：`examples/21_turn_updates.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t new_cost;
    if (!(std::cin >> new_cost)) {
        return 0;
    }

    R::Problem p(3); // 頂点・辺の構造は全ターン共通。

    const int a = p.add_arc(0, 1, 1); // 開閉する道路。
    p.add_arc(1, 2, 1); // 中継地から目的地への道路。
    const int direct = p.add_arc(0, 2, 6); // 代替道路。

    const int old_k = p.add_request(0, 2); // 最初の依頼。

    auto solver = R::make_solver(p); // p のオブジェクト自体を移動・破棄しない。

    // 各ターンの探索に200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    auto result = solver.solve(limits); // 初回だけ新規探索。

    if (!result.feasible()) { // 初回結果を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto old_solution = result.solution; // 後で検査するため、結果の経路をコピーする。

    std::cout << "turn1=" << result.summary.value->total() << '\n'; // 最初は費用2。

    p.arcs[a].enabled = false; // 全要求に対して道路を閉じる。
    p.arcs[direct].cost = new_cost; // 代替道路の費用を変更。

    solver.refresh(); // 属性変更を反映し、禁止された可動経路を取り除く。

    result = solver.resume(limits); // 更新した問題で探索を継続。

    if (!result.feasible()) { // 更新後の制約で確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto old_e = solver.evaluate(old_solution); // 保存した旧経路を現在の条件で検査。

    // 旧経路は通行止め。
    std::cout
        << "turn2=" << result.summary.value->total()
        << " old_forbidden=" << old_e.summary.forbidden_requests
        << '\n';

    p.arcs[a].enabled = true; // 道路を再開する。

    p.vertices[1].enabled = false; // 代わりに中継地点そのものを閉じる。端点でも使用禁止。

    solver.refresh(); // 頂点の属性も反映が必要。

    result = solver.resume(limits); // 中継地点を避ける。

    if (!result.feasible()) { // 実行可能性を再確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << "turn3=" << result.summary.value->total() << '\n'; // 直行路の費用5。

    p.vertices[1].enabled = true; // 次のターンで中継地点を再開。

    p.requests[old_k].active = false; // 終了した要求は費用・容量・必須接続の対象から外す。
    // 接続固定が残ったまま無効化すると矛盾するため、固定を外す。
    p.requests[old_k].fixed_route.reset();
    const int new_k = p.add_request(1, 2); // 要求の追加はグラフ構造の変更ではない。

    solver.refresh(); // 追加要求を取り込み、既存IDは保持する。

    result = solver.resume(limits); // 新しい依頼を解く。

    if (!result.feasible()) { // 新しい問題に対して成功を判定。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 新依頼だけの費用1。
    std::cout
        << "turn4=" << result.summary.value->total()
        << " old_active=" << p.requests[old_k].active
        << " new_connected=" << result.solution.routes[new_k].connected
        << '\n';

    // refresh は入力と経路を再走査する。変更箇所だけの定数時間更新ではない。
}
```

**入力例：`inputs/21_turn_updates.txt`**

```text
5
```

**標準出力：`expected/21_turn_updates.txt`**

```text
turn1=2
turn2=5 old_forbidden=1
turn3=5
turn4=1 old_active=0 new_connected=1
```

要求を配列から消して詰めるとIDの対応が変わります。ターン制では active=false と末尾追加が扱いやすい選択です。容量・背景使用・需要・個別上限の変更も、同じく refresh が必要です。

### 3.22. 新しい地点や道路を追加して旧解を引き継ぐ

**場面。** 地図の構造そのものが変わる場合です。探索用のグラフを作り直し、以前の経路を外部解として渡します。

**具体的に解く問題。** 0→1→2 の費用4の道に、新頂点3と 0→3→2 の費用1の道を追加します。旧道の辺IDは変更しません。

**入力形式。** `新経路の後半の費用`。非負整数。

**コード：`examples/22_map_expansion.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t new_cost;
    if (!(std::cin >> new_cost)) {
        return 0;
    }

    R::Problem p(3); // 最初は3頂点。

    p.add_arc(0, 1, 2); // 辺ID0。
    p.add_arc(1, 2, 2); // 辺ID1。

    p.add_request(0, 2); // 既存の要求。

    R::Solver<> solver(p); // 既定モデルを使う solver を型名で構築する。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    auto result = solver.solve(limits); // 古い地図で解く。

    if (!result.feasible()) { // 旧解を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto saved = result.solution; // reset で内部結果が変わるので、先に経路をコピー。

    std::cout << "before=" << result.summary.value->total() << '\n'; // 旧経路の費用4。

    p.vertices.emplace_back(); // 頂点3を末尾追加。既定費用0、有効、資源消費なし。

    p.add_arc(0, 3, 0); // 新しい辺を末尾追加して既存の辺IDを維持する。
    p.add_arc(3, 2, new_cost); // 新しい近道の後半。

    solver.reset(p); // 頂点数・辺数・辺の両端の変更では refresh でなく reset。

    // reset 後は resume できない。保存した解から再開始する。
    result = solver.improve(saved, limits);

    if (!result.feasible()) { // 新地図での制約を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << "after=" << result.summary.value->total() << '\n'; // 新経路の費用1。
    // 辺の削除や並べ替えをした場合は、保存経路の辺IDを自分で対応し直してから渡す。
}
```

**入力例：`inputs/22_map_expansion.txt`**

```text
1
```

**標準出力：`expected/22_map_expansion.txt`**

```text
before=4
after=1
```

reset はモデルを保持しますが探索状態を捨てます。モデルが辺ごとの別配列を持つ場合、その配列も呼び出し側で更新してください。

### 3.23. 外部の料金表を参照するモデルを更新する

**場面。** 料金表を大きな配列で管理し、モデルがその配列を参照する使い方です。参照先の値を変えただけでは、探索状態の費用は更新されません。

**具体的に解く問題。** 平行する道路の費用は1と3。最初は追加料金0、次に安い道路へ追加料金10を課し、選ぶ道を切り替えます。

**入力形式。** `追加する料金`。非負整数。

**コード：`examples/23_external_prices.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

struct Model : R::DefaultModel { // 頂点・資源の費用は既定動作。
    const std::vector<int64_t>& tolls;

    explicit Model(const std::vector<int64_t>& x) : tolls(x) {}

    // 要求IDは今回使わない。
    std::optional<int64_t> arc_cost(
        const R::Problem& p,
        int,
        int a
    ) const {
        return p.arcs[a].cost + tolls[a]; // 同じ入力・同じ料金表なら同じ非負費用を返す。
    }
};

int main() {
    int64_t extra;
    if (!(std::cin >> extra)) {
        return 0;
    }

    R::Problem p(2); // 2本の平行道路。

    p.add_arc(0, 1, 1); // 辺ID0。
    p.add_arc(0, 1, 3); // 辺ID1。

    p.add_request(0, 1); // 必須要求1件。
    // この配列オブジェクトを solver より長く生存させる。
    std::vector<int64_t> tolls(p.arcs.size(), 0);

    // モデルは値で保持されるが、その中の参照先はコピーされない。
    auto solver = R::make_solver(p, Model(tolls));

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    auto result = solver.solve(limits); // 初期料金で探索。

    if (!result.feasible()) { // 成功確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << "before=" << result.summary.value->total() << '\n'; // 安い道路の費用1。

    tolls[0] = extra;

    solver.refresh(); // 外部参照先の変更にも必要。探索中に料金を書き換えない。

    result = solver.resume(limits); // 再評価した経路を基に探索する。

    if (!result.feasible()) { // 更新後の制約で確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << "after=" << result.summary.value->total() << '\n'; // もう一方の道路の費用3。
}
```

**入力例：`inputs/23_external_prices.txt`**

```text
10
```

**標準出力：`expected/23_external_prices.txt`**

```text
before=1
after=3
```

モデルの型を変えずに値を置き換えるなら set_model、モデルの外から参照データを変えたなら refresh を使います。

### 3.24. 小数の距離や料金を費用にする

**場面。** 距離や時間のように小数をそのまま費用として扱いたい場合、Cost を double にします。容量や需要は引き続き整数です。

**具体的に解く問題。** 0→1→2 の費用が0.5+0.75、直行費が2。費用倍率1.5を掛けて1.875の経路を得ます。

**入力形式。** `前半費用 後半費用 直行費用 費用倍率`。有限の非負小数。

**コード：`examples/24_fractional_cost.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<double>; // 費用だけを double にする。容量と需要は整数のまま。

int main() {
    double first, second, direct, scale;
    if (!(std::cin >> first >> second >> direct >> scale)) {
        return 0;
    }

    R::Problem p(3); // R は CapacitatedRouter<double>。

    p.add_arc(0, 1, first); // 小数費用。NaN・無限大・負値は渡さない。
    p.add_arc(1, 2, second); // 後半の費用。
    p.add_arc(0, 2, direct); // 直行路の費用。

    const int k = p.add_request(0, 2, 1); // 需要は int64_t のまま。
    p.requests[k].cost_scale = scale; // 費用倍率も double になる。

    auto solver = R::make_solver(p); // 浮動小数点の丸め誤差を許容できる用途で使う。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 小さい費用の経路を探す。

    if (!result.feasible()) { // 費用参照前に確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    // 表示桁数は利用側で指定できる。
    std::cout
        << std::fixed
        << std::setprecision(3)
        << "cost=" << result.summary.value->total()
        << '\n';

    // double の Value::operator== は誤差許容なし。近い数の比較は利用側で許容誤差を設ける。
}
```

**入力例：`inputs/24_fractional_cost.txt`**

```text
0.5 0.75 2.0 1.5
```

**標準出力：`expected/24_fractional_cost.txt`**

```text
cost=1.875
```

小数第2位までの金額など、単位を100倍すれば整数で正確に表せる用途では、整数費用も選択肢です。

### 3.25. 時刻を頂点にして、便や装置の時間枠を割り当てる

**場面。** 「場所だけ」では同じ設備の利用時刻を区別できません。「場所と時刻の組」を1頂点にすれば、待つ動作と移動する動作を辺で表せます。

**具体的に解く問題。** 場所AからBへ1時間で移動。各時刻に出発できる便は1件まで。時刻0にAにいる2件を時刻3までにBへ送り、出発前の待ち時間と移動費を最小化します。

**入力形式。** `最終時刻 要求数`。最終時刻は1以上、要求数は非負整数。この例は3と2。

**コード：`examples/25_time_slots.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int horizon, count;
    if (!(std::cin >> horizon >> count)) {
        return 0;
    }

    // place=0 はA、1はB。組を整数IDに変換。
    auto state = [](int time, int place) {
        return 2 * time + place;
    };

    R::Problem p(2 * (horizon + 1)); // 全時刻のAとBを別頂点として用意。
    std::vector<int> departure_resources;
    for (int t = 0; t < horizon; ++t) {
        p.add_arc(state(t, 0), state(t + 1, 0), 1); // Aで1時間待つ。待ち時間の費用1。
        p.add_arc(state(t, 1), state(t + 1, 1), 0); // B到着後の待機は費用0。
        // 時刻tの移動便。費用1、便の容量1。
        const int a = p.add_arc(state(t, 0), state(t + 1, 1), 1, 1);
        // 出発時刻ごとの使用量を読むため保存。
        departure_resources.push_back(p.arcs[a].uses.front().resource);
    }

    for (int k = 0; k < count; ++k) {
        p.add_request(state(0, 0), state(horizon, 1)); // 期限までにBへ届くことを必須にする。
    }

    auto solver = R::make_solver(p); // 時間は頂点に表しただけで、専用の時刻APIは不要。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 各便の容量を分け合って移動する。

    if (!result.feasible()) { // 期限内に全要求を届けたか確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // 各時刻に出発した件数を読む。

    // 即出発の1と、1時間待つ2で合計3。
    std::cout << "cost=" << result.summary.value->total() << '\n';
    std::cout << "departures:";
    for (int r : departure_resources) {
        std::cout << ' ' << e.resource_load[r]; // 例では時刻0と1に1件ずつ。
    }

    std::cout << '\n';

    // 時刻の刻みを細かくすると頂点・辺が増える。時刻別容量は、
    // このコードで明示的に作っている。
}
```

**入力例：`inputs/25_time_slots.txt`**

```text
3 2
```

**標準出力：`expected/25_time_slots.txt`**

```text
cost=3
departures: 1 1 0
```

経路は展開した頂点上で単純路です。同じ実際の場所に異なる時刻で滞在することは禁止されません。現実の制約が正しく展開後の容量に対応するか確認します。

### 3.26. 各要求が1回だけ使える特急券を表す

**場面。** 過去に特急券を使ったかによって、次に選べる動作が変わる例です。「位置と使用済みかどうか」を頂点に持たせます。

**具体的に解く問題。** 位置0から2へ2区間移動。通常は1区間3、特急は1。ただし各要求で特急は1回まで、全要求共通でも特急券は1枚。2要求の合計費用は10です。

**入力形式。** `全体の特急券枚数`。非負整数。

**コード：`examples/26_once_only_ticket.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int64_t tickets;
    if (!(std::cin >> tickets)) {
        return 0;
    }

    // used=0 は未使用、1は使用済み。
    auto state = [](int position, int used) {
        return 2 * position + used;
    };

    const int goal = 6;

    R::Problem p(7); // 位置3種類×状態2種類と、共通の終点1個。

    const int fast = p.add_resource(tickets); // 全要求で共有する特急券の枚数。
    for (int position = 0; position < 2; ++position) {
        for (int used = 0; used < 2; ++used) {
            // 通常移動では使用状態を変えない。
            p.add_arc(state(position, used), state(position + 1, used), 3);
        }

        // 未使用状態からだけ特急を選べる。
        const int a = p.add_arc(state(position, 0), state(position + 1, 1), 1);
        p.arcs[a].uses.push_back({fast, 0, 1}); // 特急1回で共通券1枚を消費。
    }

    p.add_arc(state(2, 0), goal, 0); // 券を使わず到着してもよい。
    p.add_arc(state(2, 1), goal, 0); // 券を使って到着してもよい。

    p.add_request(state(0, 0), goal); // 要求0は未使用状態から開始。
    p.add_request(state(0, 0), goal); // 要求1も未使用状態から開始。

    // 経路の履歴を callback 内の隠れた状態にせず、頂点で表す。
    auto solver = R::make_solver(p);

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 個別の1回制限と全体の枚数を同時に守る。

    if (!result.feasible()) { // 両方の到着を確認。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // 共通券の消費枚数を確認。

    // 1+3 の経路と3+3の経路で10。
    std::cout
        << "fast_uses=" << e.resource_load[fast]
        << " cost=" << result.summary.value->total()
        << '\n';

    // 必須チェックポイント、移動方向、作業段階も同様に状態へ入れられるが、
    // 状態数に応じてグラフが大きくなる。
}
```

**入力例：`inputs/26_once_only_ticket.txt`**

```text
1
```

**標準出力：`expected/26_once_only_ticket.txt`**

```text
fast_uses=1 cost=10
```

この例は「2回目の特急への辺を作らない」ことで制限します。同様の考え方で、チェックポイントを通過した状態だけを終点へつなぐ表現もできます。

### 3.27. 作業を担当者へ割り当てる

**場面。** 道を探す形に見えない割当問題も、「開始→選んだ担当者→終了」という経路で表せます。担当者の容量が担当件数の上限になります。

**具体的に解く問題。** 2作業を2人へ1件ずつ割り当てます。作業0の費用は担当者0なら1、担当者1なら6。作業1はそれぞれ3と2。合計3の割当を求めます。

**入力形式。** `作業数 担当者数`、続いて作業数行の費用表（各行に担当者数個の非負整数）。担当者は1件まで。

**コード：`examples/27_task_assignment.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

struct Model : R::DefaultModel { // 標準モデルの必要な部分だけ変更する。
    std::vector<std::vector<int64_t>> fees;

    explicit Model(std::vector<std::vector<int64_t>> x) : fees(std::move(x)) {}

    // 要求IDが作業IDに対応する。
    std::optional<int64_t> arc_cost(
        const R::Problem& p,
        int job,
        int a
    ) const {
        const auto& edge = p.arcs[a]; // 現在評価している有向辺。
        // 担当者を選ぶ最初の辺だけ作業別料金。
        return edge.from == 0 ? fees[job][edge.to - 1] : edge.cost;
    }
};

int main() {
    int jobs, workers;
    if (!(std::cin >> jobs >> workers)) {
        return 0;
    }

    std::vector<std::vector<int64_t>> fees(jobs, std::vector<int64_t>(workers));
    for (auto& row : fees) {
        for (auto& x : row) {
            std::cin >> x;
        }
    }

    const int start = 0, finish = workers + 1;

    R::Problem p(workers + 2); // 担当者wは頂点w+1。前後に開始と終了を置く。
    for (int w = 0; w < workers; ++w) {
        p.add_arc(start, w + 1, 0, 1); // この担当者を選ぶ辺。容量1で1作業まで。
        p.add_arc(w + 1, finish, 0); // 担当者から終了へ進む。
    }

    for (int j = 0; j < jobs; ++j) {
        p.add_request(start, finish); // 作業jを要求jとして表す。
    }

    auto solver = R::make_solver(p, Model(fees)); // 要求別の費用表をモデルへ渡す。

    // 200反復。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 各作業の経路が担当者の選択になる。

    if (!result.feasible()) { // 必須作業数が担当者数を超えれば成立しない。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << "cost=" << result.summary.value->total() << '\n'; // 割当費用の合計。
    std::cout << "assignment:";
    for (const auto& route : result.solution.routes) {
        // 最初の辺の到着頂点から担当者IDを復元。
        std::cout << ' ' << p.arcs[route.arcs.front()].to - 1;
    }

    std::cout << '\n';
}
```

**入力例：`inputs/27_task_assignment.txt`**

```text
2 2
1 6
3 2
```

**標準出力：`expected/27_task_assignment.txt`**

```text
cost=3
assignment: 0 1
```

担当者ごとに複数件を受け持つなら容量を変更できます。作業別の禁止担当者を設ける場合は、その組の arc_cost を nullopt にします。

### 3.28. 障害物を避け、マスが重ならない配線を引く

**場面。** 基板の配線や盤面上の接続を考えます。各マスを頂点、上下左右の移動を辺にし、各マスの容量を1にして配線同士の重なりを防ぎます。

**具体的に解く問題。** 3×3盤面の中央を使用禁止とし、上段左から右、下段左から右を接続します。費用は移動マス数で、合計4です。

**入力形式。** `行数 列数 障害物数`、障害物のマスID、`要求数`、各要求の `始点ID 終点ID`。IDは 行×列数+列。行・列は0始まり。

**コード：`examples/28_grid_wiring.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int height, width, blocked;
    if (!(std::cin >> height >> width >> blocked)) {
        return 0;
    }

    const int n = height * width;

    R::Problem p(n); // 各マスを1頂点にする。行数×列数が int に収まる入力を使う。
    std::vector<int> cell_resource(n);

    for (int v = 0; v < n; ++v) {
        // 端点を含め各マスは1要求だけ使用可能。
        cell_resource[v] = p.add_vertex_capacity(v, 1);
    }

    for (int i = 0; i < blocked; ++i) {
        int v;
        std::cin >> v;
        p.vertices[v].enabled = false; // 障害物のマスはすべての要求に対して禁止。
    }

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const int v = row * width + col;
            if (col + 1 < width) {
                p.add_undirected_edge(v, v + 1); // 左右の移動。費用1、辺容量は無制限。
            }

            if (row + 1 < height) {
                // 上下の移動。重なりは頂点容量で制限する。
                p.add_undirected_edge(v, v + width);
            }
        }
    }

    int count;
    std::cin >> count;
    for (int i = 0; i < count; ++i) {
        int s, t;
        std::cin >> s >> t;
        p.add_request(s, t); // 需要1の必須配線。入力の端点も有効なマスIDにする。
    }

    auto solver = R::make_solver(p); // 辺を共有しないだけでなく、頂点も共有しない経路を探す。

    // 200反復。難しい配線では時間や反復数を調整する。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 指定ペアの接続を同時に解く。

    if (!result.feasible()) { // 未発見は、一般に解が存在しない証明ではない。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    const auto e = solver.evaluate(result.solution); // マスごとの使用量を再集計。
    int used = 0;
    for (int r : cell_resource) {
        used += e.resource_load[r] > 0; // 使ったマス数。移動回数とは異なる。
    }

    // 2移動×2経路、3マス×2経路。
    std::cout
        << "cost=" << result.summary.value->total()
        << " used_cells=" << used
        << '\n';
}
```

**入力例：`inputs/28_grid_wiring.txt`**

```text
3 3 1
4
2
0 2
6 8
```

**標準出力：`expected/28_grid_wiring.txt`**

```text
cost=4 used_cells=6
```

斜め移動、交差、層の移動などは、その動作に対応する辺・頂点・資源を自分で追加して表現します。この例には斜め移動はありません。

### 3.29. 乱数種を変えた候補から目的に合う解を選ぶ

**場面。** 短い探索を複数回実行して、良い実行可能解を保存します。和の目的と辞書順の目的では、比較方法を変える必要があります。

**具体的に解く問題。** 容量1・費用1の道路に、未接続費用2と5の任意要求があります。3つの乱数種で試し、目的に従って最良の実行可能解を選びます。

**入力形式。** `目的の番号 試行回数`。目的0は和、1は辞書順。試行回数は1以上。

**コード：`examples/29_multiple_starts.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int mode, trials;
    if (!(std::cin >> mode >> trials)) {
        return 0;
    }

    if ((mode != 0 && mode != 1) || trials < 1) {
        return 1;
    }

    R::Problem p(2); // 1本の道路に2つの任意要求。
    // 入力番号を明示的に列挙値へ変換。
    p.objective = mode == 0
        ? R::Objective::PenaltyPlusCost
        : R::Objective::PenaltyThenCost;

    p.add_arc(0, 1, 1, 1); // どちらか1要求だけ運べる。
    for (int64_t penalty : {int64_t{2}, int64_t{5}}) {
        const int k = p.add_request(0, 1); // 需要1で要求を追加。
        p.requests[k].required = false; // 未接続を許容。
        p.requests[k].reject_cost = penalty; // 断る損失を指定。
    }

    auto solver = R::make_solver(p); // 同じ solver を新しい solve で再開始できる。

    // 1試行あたり200反復。合計は試行数倍。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 200 // この呼び出しの最大反復数。
    };

    std::optional<R::Result> best; // 結果をコピーで保持。戻り値の参照だけを保存しない。

    // feasible な値同士だけを比較する。
    auto better = [&](const R::Value& a, const R::Value& b) {
        if (p.objective == R::Objective::PenaltyPlusCost) {
            return a.total() < b.total(); // 和の目的。
        }

        if (a.reject_cost != b.reject_cost) {
            return a.reject_cost < b.reject_cost; // 辞書順の第一順位。
        }

        // 第一順位が同点なら運用費。
        return a.route_cost + a.resource_cost < b.route_cost + b.resource_cost;
    };

    for (int i = 0; i < trials; ++i) {
        // 毎回固定分から再開始し乱数種を変更。
        const auto& result = solver.solve(
            limits,
            R::Options{.seed = static_cast<uint64_t>(i)}
        );
        // 実行可能な候補だけを、指定した目的に従って比較する。
        if (result.feasible()
            && (!best || better(*result.summary.value, *best->summary.value))) {
            best = result; // コピーして次回上書きから守る。
        }
    }

    if (!best) { // 発見できなかった場合を分ける。
        std::cout << "NO_FEASIBLE_SOLUTION\n";
        return 0;
    }

    std::cout << "best=" << *best->summary.value << '\n'; // Value をそのまま出力。
}
```

**入力例：`inputs/29_multiple_starts.txt`**

```text
1 3
```

**標準出力：`expected/29_multiple_starts.txt`**

```text
best=Value{reject_cost=2, route_cost=1, resource_cost=0, total=3}
```

試行回数を増やすと総時間も増えます。共通の deadline を各試行へ渡せば、全試行で同じ絶対締切を意識した実行もできます。

### 3.30. 結果をそのまま表示し、長い配列を省略する

**場面。** 実装中に経路・費用・違反・探索回数をまとめて読みたい場合です。構造体を cout / cerr にそのまま渡せます。

**具体的に解く問題。** 21辺の一本道を1要求で接続。結果全体と詳細評価を標準エラーへ、経路と要約を標準出力へ出します。辺列は先頭20個だけ表示されます。

**入力形式。** `一本道の辺数`。1以上の整数。この例は21。

**コード：`examples/30_debug_output.cpp`**

```cpp
#include <iostream>
#include <iomanip>
#include <optional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdint>

// 同じフォルダ、または -I で指定した場所のヘッダーを使う。
#include "capacitated_router_v05.hpp"

using R = CapacitatedRouter<>; // 費用に既定の int64_t を使う。ID は 0 始まり。

int main() {
    int length;
    if (!(std::cin >> length)) {
        return 0;
    }

    R::Problem p(length + 1); // 0からlengthまでの一本道。

    for (int v = 0; v < length; ++v) {
        p.add_arc(v, v + 1, 1, 1); // 各辺に費用1・専用容量1。
    }

    p.add_request(0, length); // 一本道の全区間を接続する要求。

    auto solver = R::make_solver(p); // 標準モデルで解く。

    // この一本道・1要求の例では1反復で接続。一般には予算を増やす。
    const R::Limits limits{
        .time_limit_us = -1, // 相対時間制限は設けない。
        .max_iterations = 1 // この呼び出しの最大反復数。
    };

    const auto result = solver.solve(limits); // 結果はコピーして保存。

    // Result は要約・停止理由・統計・解をまとめて表示。出力自体は探索をしない。
    std::cerr << result << '\n';
    std::cerr << result.solution << '\n'; // Solution は要求ID付き。経路数も先頭20件まで。
    // Statistics は探索回数や経過µs。実測時間は毎回変わる。
    std::cerr << result.statistics << '\n';

    // 詳細な使用量や違反が必要なときだけ再評価する。
    const auto e = solver.evaluate(result.solution);

    // Evaluation は資源ID付き使用量と違反一覧。各配列は先頭20要素まで。
    std::cerr << e << '\n';

    if (!result.feasible()) { // デバッグ表示とは別に成功を判定する。
        std::cout << "INFEASIBLE\n";
        return 0;
    }

    std::cout << result.solution.routes[0] << '\n'; // Route の辺列も先頭20個まで。
    // Summary は件数と費用。value がなければ none と表示。
    std::cout << result.summary << '\n';
    std::cout << "stop=" << result.stop_reason << '\n'; // StopReason は列挙値の名前を表示。

    // operator<< は改行やflushを自動追加しない。全要素が必要なら自分で配列を走査する。
}
```

**入力例：`inputs/30_debug_output.txt`**

```text
21
```

**標準出力：`expected/30_debug_output.txt`**

```text
Route{connected=true, arcs=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, ... (+1 more)]}
Summary{feasible=true, value=Value{reject_cost=0, route_cost=21, resource_cost=0, total=21}, missing_required=0, overloaded_resources=0, locally_invalid_requests=0, forbidden_requests=0, fixed_mismatches=0}
stop=IterationLimit
```

「... (+1 more)」は残り1要素の省略を表します。出力の上限は各配列ごとに20で、ネストした全体を20要素に制限するものではありません。標準エラーの elapsed_us は実行環境によって変わるので、以下には標準出力のみを載せます。
