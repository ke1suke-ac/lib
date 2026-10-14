# HypergraphPartitioner サンプルコード集

対象ヘッダ：`hypergraph_partitioner_v08.hpp`

この文書は、容量や共有物の費用を考慮した割当問題を、C++で記述するための教材です。高校数学の範囲で目的関数を説明し、全公開APIのリファレンスと、独立して実行できる31個の用途別サンプルを収録します。どの例も、他の例を実行した状態には依存しません。

読む順番は「1で何を数えるかを理解する」「2で必要な設定を調べる」「3で近い用途のコードを実行する」です。単にコードを流用する場合も、各例のコメントで入力の意味と注意点を確認できます。

## 1. 何を解くライブラリか？

### 1.1 仕事・配置先・容量・関係する集合

例えば、仕事をいくつかのチームへ分ける問題を考えます。各チームには引き受けられる仕事量の上限があり、関係の深い仕事を同じチームへ集めると連絡費用を節約できます。一方、資格が必要な仕事は特定のチームしか担当できず、一部の仕事は担当先がすでに決まっているかもしれません。

このライブラリでは、仕事を**要素**、チームなどの配置先を**グループ**と呼びます。要素は1個の通常グループに置くか、許可されている場合だけ**未採用**にします。1個の要素を複数グループへ複製するモデルではありません。

CPU、メモリ、人数、予算など、上限を守りたい量を**資源**と呼びます。資源は複数種類指定でき、資源ごとの上下限を別々に守ります。

関係する要素の集合を**ハイパーエッジ**、略して**辺**と呼びます。普通のグラフの辺が2要素を結ぶのに対して、この辺は0個以上の任意個数の要素を含められます。同じ要素が複数の辺に所属しても構いません。例えば「同じデータを使う仕事」「同じ製品の製造方式候補」「同じ技能を持つ人」が1本の辺になります。

各辺について、次の3種類の個数を区別します。

| 個数 | 数えるもの | 例：辺の4要素を `[0, 0, 2, -1]` へ配置した場合 |
|---|---|---|
| `span` | 採用された要素が使う通常グループの種類数 | グループ0・2の2種類 |
| `selected` | 未採用でない要素数 | 3個 |
| `local` | 指定した1グループ内の要素数 | グループ0では2個、1では0個、2では1個 |

未採用を表す`-1`は、通常グループの1種類としては数えません。`span`・`selected`・各`local`には、費用と上下限を別々に設定できます。

### 1.2 割当を数式で表す

数学の記号とAPIの名前を結び付けます。添字はC++と同じく0から始めます。

| 記号 | 意味・値の範囲 | APIとの対応 |
|---|---|---|
| $N$ | 要素数。0以上 | `p.vertices.size()` |
| $K$ | 通常グループ数。1以上 | `p.num_groups` |
| $R$ | 資源の種類数。1以上 | `p.num_resources` |
| $M$ | 辺数。0以上 | `p.hyperedges.size()` |
| $v$ | 要素番号。$0$から$N-1$ | `p.vertices[v]` |
| $k$ | 通常グループ番号。$0$から$K-1$ | `p.groups[k]` |
| $r$ | 資源番号。$0$から$R-1$ | 負荷や上下限の配列添字 |
| $e$ | 辺番号。$0$から$M-1$ | `p.hyperedges[e]` |
| $x$ | 要素ごとの配置先を並べた割当全体 | `Result::assignment`など |
| $x_ {v}$ | 要素$v$の配置先。$-1$または$0$から$K-1$ | `assignment[v]` |
| $T_ {e}$ | 辺$e$に所属する要素番号の集合 | `hyperedges[e].vertices` |
| $I(A)$ | 条件$A$が成り立てば1、そうでなければ0 | 数式上の補助記号。APIではない |

例えば、$I(x_ {v}=k)$は「要素$v$がグループ$k$にあるなら1」です。$sum$は、添字に該当する値をすべて足す記号です。要素0〜3の負荷を足すなら、4個の数の足し算を短く書いているだけです。対象が空の和は0とします。

グループ全体の要素数$n_ {k}$と、辺に関する3種類の個数は次のように求めます。

$$n_ {k}=sum_ {v=0}^{N-1}I(x_ {v}=k),\quad c_ {ek}=sum_ {v\in T_ {e}}I(x_ {v}=k),\quad t_ {e}=sum_ {k=0}^{K-1}c_ {ek},\quad s_ {e}=sum_ {k=0}^{K-1}I(c_ {ek}>0)$$

| 記号 | 意味 |
|---|---|
| $n_ {k}$ | グループ$k$に置かれた全要素の個数 |
| $c_ {ek}$ | 辺$e$の要素のうち、グループ$k$にある個数。`local`に相当 |
| $t_ {e}$ | 辺$e$全体の採用個数。`selected`に相当 |
| $s_ {e}$ | 辺$e$が使う通常グループ数。`span`に相当 |

### 1.3 個別負荷と共有負荷

要素ごとの負荷に加え、その辺を使うグループに1回だけ発生する共有負荷を指定できます。例えば同じデータを使う仕事が3件あっても、同じサーバ内のデータ複製は1個だけです。

$$L_ {kr}=\sum_ {v=0}^{N-1}I(x_ {v}=k)a_ {vkr}+\sum_ {e=0}^{M-1}I(c_ {ek}>0)b_ {ekr}$$

| 記号 | 意味 | APIとの対応 |
|---|---|---|
| $a_ {vkr}$ | 要素$v$をグループ$k$へ置いたときの資源$r$の負荷 | `Vertex::load[r]`。そのグループの`load_by_group`があれば置換 |
| $b_ {ekr}$ | 辺$e$をグループ$k$が使うときに1回だけ加える共有負荷 | `Hyperedge::shared_load[r]`。`shared_load_by_group`があれば置換 |
| $L_ {kr}$ | 個別負荷と共有負荷を合計した、グループ$k$の資源$r$の総負荷 | グループの容量・負荷費用が対象とする量 |

負荷は非負整数です。未採用の要素は個別負荷を消費せず、共有物の使用も発生させません。同じ要素が複数の辺に入っていれば、各辺の共有負荷はそれぞれ加算されます。

### 1.4 最小化する目的関数

ライブラリは、制約を満たす割当$x$について次の費用$F(x)$を小さくします。

$$F(x)=c_ {0}+\sum_ {v=0}^{N-1}A_ {v}(x_ {v})+\sum_ {k=0}^{K-1}\left(B_ {k}I(n_ {k}>0)+\sum_ {r=0}^{R-1}P_ {kr}(L_ {kr})\right)+\sum_ {e=0}^{M-1}\left(H_ {e}(s_ {e})+Q_ {e}(t_ {e})+\sum_ {k=0}^{K-1}J_ {ek}(c_ {ek})\right)$$

| 記号・関数 | 何の費用か | APIとの対応 |
|---|---|---|
| $F(x)$ | 割当$x$の合計費用。小さい方がよい | `Evaluation::cost`、`Result::cost` |
| $c_ {0}$ | どの割当にも共通の定数 | `Problem::constant` |
| $A_ {v}(x_ {v})$ | 要素$v$をその配置先へ置く費用。未採用なら未採用費用 | 通常は`default_cost`または`cost_by_group`、未採用は`reject_cost` |
| $B_ {k}$ | グループ$k$を開く費用 | `Group::open_cost`。$n_ {k}>0$のときだけ加算 |
| $P_ {kr}(L_ {kr})$ | グループ$k$・資源$r$の負荷が理想区間から離れた費用 | `Group::load_penalty[r]` |
| $H_ {e}(s_ {e})$ | 辺$e$の使用グループ数への費用 | `Hyperedge::span_cost` |
| $Q_ {e}(t_ {e})$ | 辺$e$の採用個数への費用 | `Hyperedge::selected_cost` |
| $J_ {ek}(c_ {ek})$ | 辺$e$のグループ$k$内の個数への費用 | `local_cost`、そのグループに指定があれば`local_cost_by_group` |

式の意味は「個々の配置費用」「開いた拠点の費用」「負荷の偏りの費用」「集合に関する3種類の費用」を全部足す、ということです。未設定の費用は0なので、必要な項だけ設定すれば使えます。例えば例01は、関係する2仕事が別グループなら5、それ以外の費用は0です。

費用は有限の負の値も使えます。利益を最大化したいなら、その利益を負の費用にすれば扱えます。定数$c_ {0}$を足して表示額を調整しても、同じ問題内の割当の優劣は変わりません。

### 1.5 費用と必ず守る制約を分ける

次の条件は、安い費用と引換えに破ってよい条件ではありません。

| 必ず守る条件 | 設定箇所 |
|---|---|
| 許可されたグループ・未採用への配置だけを使う | `Vertex::allowed_groups`、`allow_reject` |
| 固定された配置先を守る | `Vertex::fixed` |
| 資源ごとの総負荷を上下限内にする | `Group::lower`、`upper` |
| 使用グループ数を上下限内にする | `Hyperedge::min_span`、`max_span` |
| 採用個数を上下限内にする | `Hyperedge::min_selected`、`max_selected` |
| グループごとの辺内個数を上下限内にする | `local_lower`、`local_upper`と配置先別の例外 |

`lower_only_if_used=true`なら、空グループだけは負荷のハード下限を免除します。グループの使用判定は負荷ではなく要素数なので、負荷0の要素でも置けば使用中です。

制約を満たす結果は`feasible=true`です。`feasible=false`でも`cost`は上の目的関数値を表し、制約違反への罰金を含みません。費用が低いことだけを見て、違反する割当を採用しないでください。

実行可能解の発見や最小費用の達成は保証されません。`feasible=false`は「この実行で実行可能解が見つかっていない」という意味であり、「存在しないことを証明した」という意味ではありません。

## 2. APIリファレンス

### 2.1 環境と共通の入力規則

GCCとlibstdc++のC++20以上を使います。ヘッダは`<bits/stdc++.h>`と`__INCLUDE_LEVEL__`を使用するので、すべてのC++処理系でそのまま利用できるとは限りません。外部の数理最適化ライブラリや追加のリンク指定は不要です。

以降、`S`は`HypergraphPartitioner`の短縮名です。公開の設定・結果型はすべて`S::型名`として使います。表中の`vector`・`pair`・`optional`・`span`は、それぞれ`std::`名前空間の型を略記しています。

| 共通事項 | 規則 |
|---|---|
| 要素・辺・グループ番号 | 0始まり。要素は`[0,N)`、辺は`[0,M)`、通常グループは`[0,K)` |
| 未採用番号 | `-1`。`allow_reject=true`の要素だけが選べる |
| 資源配列 | `Vertex::load`、グループ上下限などは長さ`R`。共有負荷の省略だけは0配列へ正規化される |
| 負荷・容量 | `int64_t`の非負整数。実際の合計や中間計算も範囲内に収める |
| 費用 | `double`の有限値。負の値も可。計算途中も有限に収める |
| 配置先別の例外 | `vector<pair<int,T>>`で`{グループ番号, 値}`を列挙。未指定グループは共通値。加算ではなく置換 |
| 例外のキー | `[0,K)`の番号。同じリスト内で同じ番号を重複指定しない。`-1`は不可 |
| 許可先・辺所属の重複 | ソルバーが整列して重複を取り除く。呼出し側の`p`は書き換わらない |
| 同じ所属集合を持つ別の辺 | 別の辺として残る。費用・共有負荷も辺ごとに加算される |
| 入力形式の検査 | 多くは`assert`。例外やエラー構造体を返す仕様ではない。`NDEBUG`で検査を消しても入力条件は必要 |

形式として正しい入力でも、全制約を同時に満たす割当が存在するとは限りません。例えば容量不足や、相互に矛盾する採用個数制約はあり得ます。

### 2.2 `unlimited`と`Problem`

`S::unlimited`は`std::numeric_limits<int64_t>::max()`、すなわち`9223372036854775807`です。負荷の上限を実質的に制限しない指定に使います。無限精度の数ではなく、実負荷の加算でオーバーフローしないことは呼出し側の責任です。

#### `Problem(int n = 0, int k = 1, int r = 1)`

問題の入れ物を作るコンストラクタです。コンストラクタなので戻り値はありません。

| 引数 | 型・値の範囲 | 省略時 | 意味 |
|---|---|---|---|
| `n` | `int`、0以上 | 0 | 要素数。`vertices`をこの長さにする |
| `k` | `int`、1以上 | 1 | 通常グループ数 |
| `r` | `int`、1以上 | 1 | 資源数 |

各要素の`load`は長さ`r`で、第0資源が1、それ以外が0になります。各グループの`lower`は全0、`upper`は全`unlimited`、`load_penalty`は長さ`r`の既定設定です。`hyperedges`は空です。

| 公開フィールド | 型 | 作成時の値・有効な使い方 |
|---|---|---|
| `num_groups` | `int` | 引数`k`。`groups.size()`と一致させる |
| `num_resources` | `int` | 引数`r`。資源配列の長さと一致させる |
| `vertices` | `vector<Vertex>` | 引数`n`個。配列の添字が要素番号 |
| `groups` | `vector<Group>` | 引数`k`個。配列の添字がグループ番号 |
| `hyperedges` | `vector<Hyperedge>` | 空。添字が辺番号。`add_hyperedge`で追加できる |
| `constant` | `double` | 0。すべての割当に足す有限の定数 |

`num_groups`や`num_resources`だけを変えて配列を不整合にしないでください。構成を変える場合は新しい`Problem`を作り、必要なら`reset_problem`で渡すと扱いやすくなります。

#### `int Problem::add_hyperedge(vector<int> vertices_, CountCost cost = CountCost::linear(0))`

| 引数・戻り値 | 値・既定値 | 意味 |
|---|---|---|
| `vertices_` | 必須。`[0,N)`の要素番号を並べた配列。空も可 | 追加する辺の所属集合 |
| `cost` | 省略時は全個数で費用0の関数 | この辺の`span_cost`。`local_cost`や`selected_cost`ではない |
| 戻り値 | `int`、追加した辺の番号 | `p.hyperedges[e]`でさらに設定するために使う |

所属・費用を所有する辺を末尾へ追加します。他の辺フィールドは既定値です。辺の追加や要素数変更をソルバーへ反映するには`reset_problem`を使います。`set_hyperedge`は既存の辺番号の置換です。

### 2.3 `Vertex`：要素ごとの負荷・費用・許可先

`Vertex`単独の作成では`load`が空です。通常は`Problem`が用意した`p.vertices[v]`を編集してください。単独作成したものを渡す場合は、`load`を長さ`R`にする必要があります。

| フィールド | 型 | 既定値 | 意味・取り得る値 |
|---|---|---|---|
| `load` | `vector<int64_t>` | 単独では空。`Problem`内では`[1,0,...]` | 長さ`R`、各値0以上。通常配置時の個別負荷 |
| `load_by_group` | `vector<pair<int,vector<int64_t>>>` | 空 | 指定グループの負荷配列を置換。各配列は長さ`R`、非負 |
| `default_cost` | `double` | 0 | 通常グループへ置く既定費用。有限値 |
| `cost_by_group` | `vector<pair<int,double>>` | 空 | 指定グループの配置費用を置換。各費用は有限値 |
| `allowed_groups` | `optional<vector<int>>` | `nullopt` | 未指定なら全通常グループ。指定時はその集合だけを許可 |
| `fixed` | `optional<int>` | `nullopt` | 未指定なら固定なし。指定値は`-1`または`[0,K)`の許可済み配置先 |
| `allow_reject` | `bool` | `false` | `true`なら未採用`-1`も選択肢に加える |
| `reject_cost` | `double` | 0 | `-1`にした場合だけ加える有限の費用 |

`allowed_groups = vector<int>{}`は「通常配置先なし」です。この場合は`allow_reject=true`が必要です。`nullopt`と空配列は反対の意味になり得ます。

`fixed`は他の許可条件を上書きしません。例えば許可先が`{0}`なのに`fixed=1`とする入力は不正です。`fixed=-1`も`allow_reject=true`のときだけ有効です。固定値0も正しく固定を意味します。

### 2.4 `Group`：容量・負荷目標・開設費

単独作成の`Group`は3個の配列が空です。通常は`p.groups[k]`を編集します。単独作成して渡すなら`lower`・`upper`・`load_penalty`をすべて長さ`R`にしてください。

| フィールド | 型 | `Problem`内の既定値 | 意味・取り得る値 |
|---|---|---|---|
| `lower` | `vector<int64_t>` | 全0 | 資源ごとのハード下限。長さ`R`、非負 |
| `upper` | `vector<int64_t>` | 全`unlimited` | 資源ごとのハード上限。長さ`R`。各資源で`lower <= upper` |
| `load_penalty` | `vector<LoadPenalty>` | 全要素が既定設定 | 資源ごとのソフト費用。長さ`R` |
| `open_cost` | `double` | 0 | 1要素以上配置したときに1回だけ加える有限の費用 |
| `lower_only_if_used` | `bool` | `false` | `true`なら空グループのハード下限を免除。上限とソフト費用は免除しない |

容量を測る負荷には、要素の個別負荷と辺の共有負荷の両方が入ります。`lower_only_if_used=false`では、空グループにも下限が掛かります。

### 2.5 `LoadPenalty`：理想区間からの費用

| フィールド | 型 | 既定値 | 意味・条件 |
|---|---|---|---|
| `lower` | `int64_t` | 0 | 理想区間の下端。0以上 |
| `upper` | `int64_t` | `unlimited` | 理想区間の上端。`lower <= upper` |
| `under_linear` | `double` | 0 | 不足量の1乗に掛ける有限係数 |
| `under_quadratic` | `double` | 0 | 不足量の2乗に掛ける有限係数 |
| `over_linear` | `double` | 0 | 超過量の1乗に掛ける有限係数 |
| `over_quadratic` | `double` | 0 | 超過量の2乗に掛ける有限係数 |

`double operator()(int64_t load) const`は、非負の負荷`load`を必須引数として受け取り、費用を返します。引数の既定値はありません。

不足量を$a=\max(0,\mathrm{lower}-\mathrm{load})$、超過量を$b=\max(0,\mathrm{load}-\mathrm{upper})$とすると、費用は次の値です。$a$・$b$は0以上で、理想区間内なら両方0です。

$$P(\mathrm{load})=\alpha_ {1}a+\alpha_ {2}a^2+\beta_ {1}b+\beta_ {2}b^2$$

係数$\alpha_ {1}$は`under_linear`、$\alpha_ {2}$は`under_quadratic`、$\beta_ {1}$は`over_linear`、$\beta_ {2}$は`over_quadratic`です。負荷の二乗ではなく「区間から離れた量」の二乗です。$P$はこの1個の`LoadPenalty`が返す費用です。

負の係数も入力形式上は有効ですが、その場合は区間から離れることが報酬になる場合があります。普通の罰点として使うなら係数を0以上にします。ハード上下限とは独立で、空グループの負荷0にも適用されます。

### 2.6 `CountCost`：個数に応じた費用

`span_cost`・`selected_cost`・`local_cost`は、すべて同じ`CountCost`型です。**何を数えるかは、置くフィールドによって決まります。**

| フィールド | 型 | 既定値 | 意味・条件 |
|---|---|---|---|
| `active` | `double` | 0 | 個数が1以上なら1回加える有限費用 |
| `extra` | `double` | 0 | 2個目以降の1個ごとに加える有限費用 |
| `cut_value` | `double` | 0 | 個数が2以上なら1回加える有限費用 |
| `table` | `vector<double>` | 空 | 空なら追加0。非空なら個数を添字として追加。全値が有限で`table[0] == 0` |

個数を$q$、空表なら0・非空なら`table[q]`を返す補助関数を$T(q)$とすると、合計費用は次のとおりです。

$$C(q)=\alpha I(q>0)+\beta\max(q-1,0)+\gamma I(q\geq2)+T(q)$$

$q$は0以上の整数、$C(q)$はこの`CountCost`の返す費用です。この式の$\alpha$は`active`、$\beta$は`extra`、$\gamma$は`cut_value`です。$I$は第1部と同じ0・1の関数です。`table`は他の3項に加算されるため、全体を料金表だけで決めるなら他の3フィールドを0にします。

| API | 引数・既定値 | 戻り値・意味 |
|---|---|---|
| `static CountCost activation(double w)` | 有限の`w`、省略不可 | 個数0なら0、1以上なら`w` |
| `static CountCost connectivity(double w)` | 有限の`w`、省略不可 | 個数0・1なら0、2以上なら`w * (q-1)` |
| `static CountCost cut(double w)` | 有限の`w`、省略不可 | 個数0・1なら0、2以上なら一律`w` |
| `static CountCost linear(double w)` | 有限の`w`、省略不可 | 個数に比例した`w * q` |
| `static CountCost lookup(vector<double> values)` | 配列は必須。空、または先頭0の有限値配列 | 配列を所有し、`table[q]`の費用を返す設定。ほかの3フィールドは0 |
| `double operator()(int count) const` | 非負の個数`count`、省略不可 | 合成後の費用。表が非空なら`count < table.size()`が必要 |

`w`は負でも構いません。例えば`linear(-2)`は、個数1につき2の報酬です。

**表の長さを決める規則：** 辺の重複除去後の要素数を$m$とします。`span_cost.table`は少なくとも`min(K,m)+1`個、`selected_cost.table`・`local_cost.table`・各`local_cost_by_group`の表は少なくとも`m+1`個必要です。空表は許されます。ハード上限を小さくしても、制約違反を含む割当を採点できる長さが必要なので、表をその上限までに縮めないでください。ここで$m$は、この段落だけで使う辺サイズの記号です。

### 2.7 `Hyperedge`：集合の費用・共有負荷・個数制約

| フィールド | 型 | 既定値 | 意味・有効範囲 |
|---|---|---|---|
| `vertices` | `vector<int>` | 空 | 辺に属する要素番号。各値`[0,N)`。重複は除去される |
| `span_cost` | `CountCost` | 全費用0 | 使用する通常グループ数への費用 |
| `local_cost` | `CountCost` | 全費用0 | 各通常グループ内の個数への共通費用 |
| `selected_cost` | `CountCost` | 全費用0 | 全通常グループを合計した採用個数への費用 |
| `local_cost_by_group` | `vector<pair<int,CountCost>>` | 空 | 指定グループの局所費用関数を丸ごと置換 |
| `shared_load` | `vector<int64_t>` | 空 | 省略時は長さ`R`の0へ正規化。指定時は長さ`R`、全値非負 |
| `shared_load_by_group` | `vector<pair<int,vector<int64_t>>>` | 空 | 指定グループの共有負荷を置換。各配列は長さ`R`、全値非負 |
| `min_span` | `int` | 0 | 使用グループ数の下限。0以上 |
| `max_span` | `int` | `INT_MAX` | 使用グループ数の上限。`min_span <= max_span` |
| `min_selected` | `int` | 0 | 採用個数の下限。0以上 |
| `max_selected` | `int` | `INT_MAX` | 採用個数の上限。`min_selected <= max_selected` |
| `local_lower` | `int` | 0 | **全通常グループ**に課す辺内個数の共通下限。0以上 |
| `local_upper` | `int` | `INT_MAX` | 共通上限。`local_lower <= local_upper` |
| `local_lower_by_group` | `vector<pair<int,int>>` | 空 | 指定グループの下限を置換。0以上 |
| `local_upper_by_group` | `vector<pair<int,int>>` | 空 | 指定グループの上限を置換。実効下限以上 |

`INT_MAX`は`int`型の最大値です。個数の上限に用いられ、`int64_t`負荷用の`unlimited`とは型が異なります。

グループ別の例外を適用した後も、各グループで実効下限が実効上限以下になる必要があります。共通下限・共通上限自体も順序を満たしてください。下限が実際の辺サイズを上回るなど、形式は正しくても実行不可能になる条件は設定できます。

`local_lower=1`は未使用グループにも1個以上を要求します。「使ったグループに限って1個以上」という条件ではありません。空の辺でも下限が正なら違反になります。個数0の`CountCost`は常に0なので、全要素未採用なら辺の費用と共有負荷は0ですが、辺の下限違反は残り得ます。

### 2.8 `Options`とソルバーの作成

| `Options`のフィールド | 型 | 既定値 | 意味・値の範囲 |
|---|---|---|---|
| `time_limit_us` | `int64_t` | 100000 | 0以上。今回の呼出しの時間予算をマイクロ秒で指定 |
| `annealing` | `bool` | `true` | `true`で悪化を確率的に受け入れる。`false`でその受入れを無効化 |

`1000`マイクロ秒は1ミリ秒、`100000`マイクロ秒は100ミリ秒です。`solve({20'000})`なら予算20ミリ秒、`annealing=true`になります。`solve({20'000, false})`なら予算20ミリ秒で確率的な悪化受入れを止めます。`solve({})`は既定の`Options`ですが、**引数なしの`solve()`はありません。**

時間予算は厳密な打切り時刻ではありません。初期化・同期・最終診断などで超える場合があります。0でもこれらは行われ、初期解が未作成なら初期配置を作ります。探索の採点は制約違反も扱うため、`annealing=false`を「表示される費用だけが必ず毎手改善する」と解釈しないでください。

#### `explicit HypergraphPartitioner(Problem problem, uint64_t seed = 0)`

| 引数 | 既定値・有効値 | 意味 |
|---|---|---|
| `problem` | 必須。前述の入力条件を満たす問題 | 入力を所有し、検査・正規化する。左辺値を渡せば呼出し側の入力は残る |
| `seed` | 0。`uint64_t`の全範囲 | 乱数の初期状態 |

コンストラクタなので戻り値はありません。ソルバー作成後に手元の`p`だけを変えても、ソルバー内部は変わりません。変更はsetterか`reset_problem`で渡します。

### 2.9 探索・継続・採点・結果取得

| メソッド | 戻り値 | 動作 |
|---|---|---|
| `solve(const Options& options)` | `const Result&` | 新しい割当を作り、全要素を対象に探索。以前の可動範囲を解除 |
| `improve(span<const int> initial, const Options& options, optional<vector<int>> movable = nullopt)` | `const Result&` | 指定割当から改善。可動範囲外はその初期配置を維持 |
| `resume(const Options& options)` | `const Result&` | 変更を同期し、既存状態・最良解・可動範囲で継続。初回なら初期配置も作る |
| `evaluate(span<const int> assignment) const` | `Evaluation` | 指定割当を本来の費用で採点し、全違反を列挙。探索状態や結果を更新しない |
| `result() const` | `const Result&` | 最後に同期・実行した結果を参照。引数なし |

`span<const int>`は整数配列をコピーせず受け取るための型です。例では`vector<int>`をそのまま渡しています。渡す割当は長さ`N`、各値は`-1`または`[0,K)`です。これらの引数の既定値はありません。

`improve`の`initial`は、各要素の許可先・未採用許可・固定条件を満たす必要があります。容量・辺の個数制約は満たさなくても開始できます。一方、`evaluate`は許可先や固定条件に違反する割当も診断できます。ただし長さ・番号の数値範囲は常に守ります。

`movable`は、動かしてよい**要素番号**の一覧です。

| `movable`の指定 | 意味 |
|---|---|
| 省略、または`std::nullopt` | 全要素を対象にする。ただし`fixed`は守る |
| `std::vector<int>{2,3}`など | この要素だけ可動。各番号は`[0,N)`。重複指定で動作回数は増えない |
| `std::vector<int>{}` | 誰も動かさない |

第3引数に単に`{}`と書くと空の`optional`、つまり全要素対象になります。空の可動範囲は`std::vector<int>{}`と明記してください。`movable`に含めても、入力の`fixed`を解除することはできません。

`result()`は、`solve`・`improve`・`resume`の少なくとも1回が完了し、未同期の入力変更がないときだけ呼べます。コンストラクタ直後、入力を変更する3種類のsetterの直後、`reset_problem`直後は呼べません。`evaluate`を呼んだだけでは、この条件を満たしません。`set_seed`は入力を変更しないため、すでに有効な結果を無効化しません。

返された`const Result&`はソルバー内の同じ結果オブジェクトへの参照です。次の変更操作で内容が更新される可能性があるので、独立した記録が必要なら`S::Result saved = solver.result();`のようにコピーします。`Evaluation`は値で返るため、保持した評価結果は後の探索から独立しています。

### 2.10 入力の更新と乱数状態の更新

以下のメソッドの戻り値はすべて`void`です。引数の既定値はありません。

| メソッド | 引数の意味・取り得る値 | 動作・注意 |
|---|---|---|
| `set_vertex(int v, Vertex value)` | `v`は`[0,N)`、`value`は長さ`R`の負荷などを備えた有効な要素 | その要素の全フィールドを置換。省略フィールドだけ維持する更新ではない |
| `set_group(int k, Group value)` | `k`は`[0,K)`、`value`は3配列が長さ`R`の有効なグループ | 容量・ソフト費用・開設費などをまとめて置換 |
| `set_hyperedge(int e, Hyperedge value)` | `e`は`[0,M)`、`value`は有効な辺 | 所属集合も含めて辺全体を置換。新規辺の追加ではない |
| `reset_problem(Problem problem)` | 有効な新しい問題 | サイズも含めて問題を置換し、解・可動範囲を破棄。乱数状態は保持 |
| `set_seed(uint64_t seed)` | `uint64_t`の全範囲 | 乱数状態だけを変更。入力・解・可動範囲は保持 |

複数のsetterをまとめて呼び、その後に`resume(options)`で同期・継続できます。変更後の入力で`evaluate(assignment)`することもできますが、`evaluate`は保存結果を同期しないので、そのまま`result()`は呼べません。

条件変更によって現在の配置が許可先・固定条件に反した場合、可動範囲内は修復対象になります。範囲外は変更せず、`scope_conflict=true`として探索を止める場合があります。これは一般の容量違反とは別の状態です。全体の再配置を許せるなら`solve`、新たな初期案と可動範囲を渡すなら`improve`を使います。

`solve`や`resume`は暗黙にシードをリセットしません。シードを指定して新規探索をやり直すなら、`set_seed`の後に`solve`を呼びます。時間予算で試行を打ち切るため、同じシードだけで別環境の完全な再現性が保証されるわけではありません。

### 2.11 `Evaluation`と`Result`

| 型・フィールド | 型 | 空の構造体を作ったときの既定値 | 実際の結果での意味 |
|---|---|---|---|
| `Evaluation::cost` | `double` | 0 | 定数項を含む本来の目的関数値。違反への探索用の罰金は含まない |
| `Evaluation::feasible` | `bool` | `false` | すべてのハード制約を満たせば`true` |
| `Evaluation::violation` | `vector<Violation>` | 空 | 発生した違反の一覧。データは全件保持 |
| `Result::assignment` | `vector<int>` | 空 | 長さ`N`の配置先。各値は`-1`または`[0,K)` |
| `Result::scope_conflict` | `bool` | `false` | 入力更新後、可動範囲外の配置と許可・固定条件が衝突したことを示す |
| `Result::iterations` | `int64_t` | 0 | 今回の探索の試行回数。受け入れた変更数ではなく、継続呼出し間の累計でもない |
| `Result::elapsed_us` | `double` | 0 | 今回の呼出し開始から結果作成までの経過マイクロ秒。初期化・同期・診断を含む |

`Result`は`Evaluation`を継承するため、`cost`・`feasible`・`violation`も直接持つ結果として使います。`Result`から`Evaluation`として扱うこともできます。

`scope_conflict=false`は実行可能性の保証ではありません。容量違反などがあれば`feasible=false`です。また、自分で空の`Evaluation{}`を作っただけでは採点していないので、`violation`が空でも`feasible`の既定値は`false`です。ソルバーの採点結果では、違反一覧が空かどうかと`feasible`が一致します。

### 2.12 `ViolationKind`と`Violation`

`enum class ViolationKind`の列挙値は次の9個です。`S::ViolationKind::LoadUpper`のように修飾して使います。

| 列挙値 | 意味 | `id` | `group` | `resource` | `amount` |
|---|---|---|---|---|---|
| `Domain` | 許可先・未採用許可・固定先のいずれかを守っていない | 要素番号 | その要素の実際の配置先。未採用なら-1 | -1 | 1。複数の許可条件に反しても1要素で1件 |
| `LoadLower` | グループの資源負荷がハード下限より小さい | グループ番号 | 同じグループ番号 | 資源番号 | 下限−実負荷 |
| `LoadUpper` | グループの資源負荷がハード上限より大きい | グループ番号 | 同じグループ番号 | 資源番号 | 実負荷−上限 |
| `SpanLower` | 辺の使用グループ数が下限未満 | 辺番号 | -1 | -1 | 下限−使用グループ数 |
| `SpanUpper` | 辺の使用グループ数が上限超過 | 辺番号 | -1 | -1 | 使用グループ数−上限 |
| `SelectedLower` | 辺の採用個数が下限未満 | 辺番号 | -1 | -1 | 下限−採用個数 |
| `SelectedUpper` | 辺の採用個数が上限超過 | 辺番号 | -1 | -1 | 採用個数−上限 |
| `LocalLower` | 辺の指定グループ内の個数が実効下限未満 | 辺番号 | 対象グループ番号 | -1 | 実効下限−そのグループ内個数 |
| `LocalUpper` | 辺の指定グループ内の個数が実効上限超過 | 辺番号 | 対象グループ番号 | -1 | そのグループ内個数−実効上限 |

負荷の`amount`は、設定した元の整数単位です。個数の違反は個数単位です。これらを資源の違いを無視して単純合計しても、一般的な意味での損失や改善優先度になるとは限りません。

| `Violation`のフィールド | 型 | 既定値 | 補足 |
|---|---|---|---|
| `kind` | `ViolationKind` | 明示的なメンバー既定値なし | 手作業で作る場合は列挙値を指定する。ソルバーが返す要素には設定済み |
| `id` | `int` | -1 | 列挙値によって要素・グループ・辺のいずれかを指す |
| `group` | `int` | -1 | 対象グループ。`Domain`では実配置先 |
| `resource` | `int` | -1 | 負荷違反のときだけ対象資源 |
| `amount` | `int64_t` | 0 | ソルバーが報告する違反では正の値 |

`group=-1`は、`Domain`では未採用先を意味し、それ以外の一部の種類では「グループを特定しない」という意味です。`id`だけを見て常に要素番号と解釈しないでください。

### 2.13 デバッグ出力の3つの演算子

次の3つを提供します。

| シグネチャ | 出力内容 |
|---|---|
| `std::ostream& operator<<(std::ostream& os, const Evaluation& value)` | `feasible`、`cost`、`violation` |
| `std::ostream& operator<<(std::ostream& os, const Result& value)` | 評価項目と`assignment`、`scope_conflict`、`iterations`、`elapsed_us` |
| `std::ostream& operator<<(std::ostream& os, const Violation& value)` | 違反名と`id`、`group`、`resource`、`amount` |

引数は出力先ストリームと表示する構造体で、どちらも必須です。戻り値は渡した`os`の参照です。通常は`std::cout << result << '\n';`や`std::cerr << solver.evaluate(plan) << '\n';`の形で使います。

配列はそれぞれ先頭20件まで表示し、残りが5件なら`... (+5)`を付けます。空配列は`[]`、ちょうど20件なら省略表示はありません。表示だけが省略され、結果の配列は全件残ります。提出用の完全な割当を出す場合は`assignment`を直接走査してください。

真偽値は`true`・`false`です。数値は呼出し側のストリームの数値書式・精度に従います。演算子はそれらの設定を変更せず、改行や明示的なフラッシュも付けません。違反名は上記列挙値の名前になります。

### 2.14 APIとサンプルの対応

入力型の全フィールド、公開メソッド、結果型、列挙値、出力演算子はこの第2部で列挙しています。以下の表は実コードを探すための索引です。

| 主なAPI・設定 | 対応例 |
|---|---|
| `Problem`、`add_hyperedge`、`solve`、基本結果 | 01、29 |
| `num_groups`・`num_resources`・各配列の構成 | 02、25、29。構成値は`Problem(n,k,r)`を通して設定 |
| `load`、`load_by_group`、`default_cost`、`cost_by_group` | 02 |
| `allowed_groups`、`fixed`、`nullopt` | 03、24、28 |
| `allow_reject`、`reject_cost`、`constant` | 04、14、28 |
| `lower`、`upper`、`open_cost`、`lower_only_if_used` | 06 |
| `LoadPenalty`の全フィールド・呼出し演算子 | 07 |
| `shared_load`、`shared_load_by_group`、`linear` | 08 |
| `connectivity`、`cut`、`activation` | 01、09、13 |
| `min_span`、`max_span` | 10 |
| `local_lower`、`local_upper`と配置先別の例外 | 11、12 |
| `local_cost`、`local_cost_by_group` | 13、15 |
| `selected_cost`、`lookup` | 14 |
| `CountCost`の全フィールド・呼出し演算子 | 16 |
| `min_selected`、`max_selected` | 05、17、31 |
| `improve`、`evaluate` | 18 |
| `movable`、空の可動範囲、結果コピー | 19、20 |
| `Options`、`resume`、`result` | 20、21 |
| `set_seed` | 22 |
| `set_vertex`、`set_group`、`set_hyperedge` | 23 |
| `scope_conflict`、全体再配置 | 24 |
| `reset_problem` | 25 |
| `Evaluation`、`Violation`、9種類の列挙値 | 26 |
| `unlimited`、空問題 | 27 |
| `Result`・`Evaluation`・`Violation`の出力演算子 | 01、26、30 |

## 3. ユースケース

### 実行方法と出力の読み方

各例を`example01.cpp`などの名前で保存し、同じフォルダへ`hypergraph_partitioner_v08.hpp`を置いて実行できます。ZIPではソースが`examples/`、ヘッダがその親フォルダに入っています。以下はZIPを展開したフォルダでのコマンドです。

```bash
g++ -std=c++20 -O2 -I . examples/example01.cpp -o example01
./example01
```

例01〜28・30・31は、コード内の定数や配列が問題入力です。標準入力を待たず、そのまま実行できます。例29だけは、掲載の標準入力を渡します。

```bash
g++ -std=c++20 -O2 -I . examples/example29.cpp -o example29
./example29 < examples/example29.in
```

各例の「実行時の標準出力」は実際の検証で得たものです。時間で打ち切るため、割当・試行回数・経過時間は変わり得ます。同じ費用の別の割当もあり、出力例との文字列完全一致を仕様としません。`elapsed_us`はマイクロ秒です。最適な割当を説明する箇所も、APIによる最適性の保証ではありません。

各設定やAPI呼出しの直前に、日本語コメントを付けています。処理のまとまりは空行で区切っています。各例は独立した`main`を持つので、複数例を1つのソースへそのまま連結しないでください。成功時に最後まで到達した`main`は0を返します。探索例は実行可能解が見つからない場合を明示的に処理します。

| 番号 | 用途・操作 |
|---|---|
| 01 | 関係の深い仕事を同じチームへまとめる |
| 02 | CPU・メモリと機種ごとの処理量を考えて配置する |
| 03 | 資格のある担当だけを許し、確定済みの仕事を固定する |
| 04 | 予算内で利益の大きい案件を採用する |
| 05 | 各製品について製造方式をちょうど1つ選ぶ |
| 06 | 開設費と最低稼働人数を持つ拠点を選ぶ |
| 07 | 負荷目標からの不足・超過を費用で調整する |
| 08 | 同じデータを使う仕事の共有メモリを数える |
| 09 | 部署をまたぐ共同作業に1回だけ調整費を払う |
| 10 | 重要な仕事をちょうど2か所へ分散する |
| 11 | 同じチームに置けない仕事の組を表す |
| 12 | 全チームへ技能者を配り、チーム別に人数枠を変える |
| 13 | 共通ライセンスの利用料を配置先ごとに変える |
| 14 | まとめて採用した個数に応じた料金表を使う |
| 15 | 同じグループへの集中を非線形な混雑費用で抑える |
| 16 | 固定費・追加費・分割費と料金表を重ねる |
| 17 | 採用人数に上下限を設けて委員を選ぶ |
| 18 | 既存の割当から改善を始める |
| 19 | 変更を許した仕事だけ再配置する |
| 20 | 一切動かさず計画を登録し、結果を保存する |
| 21 | 探索時間を分割し、途中から山登りに切り替える |
| 22 | 複数の初期化を試し、実行可能な最良結果を残す |
| 23 | 仕事・容量・共有条件を変更して再計画する |
| 24 | 動かせない範囲と新しい固定条件の衝突を処理する |
| 25 | 仕事数や資源数も含めて問題を入れ替える |
| 26 | 違反の種類・場所・不足量を診断する |
| 27 | 対象仕事が0件の日を同じコードで扱う |
| 28 | 稼働可能な配置先がない仕事を明示的に除外する |
| 29 | 標準入力から一般的なハイパーグラフ分割を解く |
| 30 | 長い結果を省略表示し、必要なら違反を全件読む |
| 31 | 複数の案件集合にまたがる採用枠を同時に守る |

### 3.01 関係の深い仕事を同じチームへまとめる

**利用場面：** 共同作業が必要な仕事を同じチームへ集め、チームをまたぐ連絡を減らします。単なる人数の均等化に、仕事同士の関係を加えた例です。

**解く問題・入力：** 仕事4件、チーム2個。各仕事の負荷は1、各チームの容量は2。仕事0・1が別チームなら費用5、同じなら0。全仕事を配置します。

**コード：** `examples/example01.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 仕事4件、チーム2個。資源数は省略して1、各仕事の負荷は既定で1
    S::Problem p(4, 2);

    // 全チームで第0資源の容量を2にする
    for (auto& g : p.groups) {
        g.upper[0] = 2;
    }

    // この2仕事が使う2番目以降のチームごとに5
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[1, 1, 0, 0], scope_conflict=false, iterations=19488, elapsed_us=23682.3}
```

**読み方・補足：** 例えば割当[0, 0, 1, 1]は実行可能で費用0です。チーム番号を入れ替えても同じ意味です。

### 3.02 CPU・メモリと機種ごとの処理量を考えて配置する

**利用場面：** 仕事はCPUとメモリを同時に使い、機種によって必要量や利用料が異なります。資源ごとに別々の容量を守ります。

**解く問題・入力：** 仕事3件、サーバ2台、資源2種類。通常負荷は順に(2,3)、(2,2)、(1,2)、各サーバの容量は(4,5)。仕事0はサーバ1なら負荷(1,2)、配置費用1、他では4です。

**コード：** `examples/example02.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 仕事3件、サーバ2台、CPUとメモリの2資源
    S::Problem p(3, 2, 2);
    // 仕事0の通常負荷をCPU2、メモリ3に置換
    p.vertices[0].load = {2, 3};
    // 仕事1のCPU・メモリ負荷
    p.vertices[1].load = {2, 2};
    // 仕事2のCPU・メモリ負荷
    p.vertices[2].load = {1, 2};
    // サーバ1での負荷を丸ごと置換。通常負荷への加算ではない
    p.vertices[0].load_by_group = {{1, {1, 2}}};

    // 仕事0を通常サーバへ置く既定費用
    p.vertices[0].default_cost = 4;
    // サーバ1なら費用1に置換。第1値が番号、第2値が費用
    p.vertices[0].cost_by_group = {{1, 1}};

    // 各サーバでCPU4以下、メモリ5以下
    for (auto& g : p.groups) {
        g.upper = {4, 5};
    }

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=1, violation=[], assignment=[1, 0, 1], scope_conflict=false, iterations=15744, elapsed_us=20587}
```

**読み方・補足：** 負荷配列の長さは常に資源数2です。配置先による例外も2要素必要です。

### 3.03 資格のある担当だけを許し、確定済みの仕事を固定する

**利用場面：** 資格を持つ担当へだけ仕事を割り当てます。すでに確定した担当は変更しません。許可先と固定先は同時に守る条件です。

**解く問題・入力：** 仕事4件、担当3人、1人2件まで。仕事0は担当0・2のみ、仕事1は担当1へ固定、仕事2は担当1・2のみ、仕事3は誰でも担当可能です。

**コード：** `examples/example03.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 仕事4件、担当3人、件数を表す1資源
    S::Problem p(4, 3);

    // 1人あたり2件まで
    for (auto& g : p.groups) {
        g.upper[0] = 2;
    }

    // 通常配置先を担当0と2に限定
    p.vertices[0].allowed_groups = std::vector<int>{0, 2};
    // 仕事1の資格条件
    p.vertices[1].allowed_groups = std::vector<int>{1, 2};
    // 担当1に固定。固定先は許可先にも含まれている必要がある
    p.vertices[1].fixed = 1;
    // 仕事2の資格条件
    p.vertices[2].allowed_groups = std::vector<int>{1, 2};
    // 未指定は通常配置先すべて。空vectorとは異なる
    p.vertices[3].allowed_groups = std::nullopt;

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[2, 1, 1, 2], scope_conflict=false, iterations=16832, elapsed_us=20047.7}
```

### 3.04 予算内で利益の大きい案件を採用する

**利用場面：** 全案件を必須にせず、予算内で採用案件を選びます。利益の最大化を「得られなかった利益」の最小化に書き換えます。

**解く問題・入力：** 案件の必要予算は(3,2,2)、利益は(8,5,4)、総予算は4。採用しない案件は配置先-1。目的は17から採用利益を引いた値です。

**コード：** `examples/example04.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    const std::vector<int64_t> budget{3, 2, 2};
    const std::vector<double> profit{8, 5, 4};

    // グループ0を採用枠とみなし、資源を予算とする
    S::Problem p(3, 1);
    // 採用案件の総予算を4以下に制限
    p.groups[0].upper[0] = 4;

    // 利益総額。全割当に同じ定数を加え、得られなかった利益として出す
    p.constant = 17;

    for (int v = 0; v < 3; ++v) {
        // 採用時に使う予算
        p.vertices[v].load[0] = budget[v];

        // 採用利益を負の費用として加える
        p.vertices[v].default_cost = -profit[v];

        // -1を選んで未採用にできる
        p.vertices[v].allow_reject = true;
        // 未採用そのものの費用は0。未採用なら負荷も0
        p.vertices[v].reject_cost = 0;
    }

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=8, violation=[], assignment=[-1, 0, 0], scope_conflict=false, iterations=13792, elapsed_us=29729.2}
```

**読み方・補足：** 案件1・2を採用すると利益9、目的値8です。負の配置費用は有効な入力です。

### 3.05 各製品について製造方式をちょうど1つ選ぶ

**利用場面：** 製品そのものではなく、製品と製造方式の組を要素にします。同じ製品の候補を1本の辺へまとめると「候補から1つだけ」が表せます。

**解く問題・入力：** 製品Aは候補0・1、製品Bは候補2・3。1工場の容量5、候補の負荷(3,2,3,2)、費用(1,3,1,3)。各製品につき1候補だけ採用します。

**コード：** `examples/example05.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 4個の方式候補を、1工場または未採用へ配置する
    S::Problem p(4, 1);
    const std::vector<int64_t> loads{3, 2, 3, 2};
    const std::vector<double> costs{1, 3, 1, 3};
    // 採用した方式の合計負荷を5以下にする
    p.groups[0].upper[0] = 5;

    for (int v = 0; v < 4; ++v) {
        // 候補vを採用したときの負荷
        p.vertices[v].load[0] = loads[v];

        // 候補vの製造費
        p.vertices[v].default_cost = costs[v];

        // 選ばない候補は-1にする
        p.vertices[v].allow_reject = true;
    }

    for (auto members : {std::vector<int>{0, 1}, std::vector<int>{2, 3}}) {
        // 1製品の候補をまとめる。第2引数省略で辺費用0
        const int e = p.add_hyperedge(members);
        // この製品では少なくとも1候補を採用
        p.hyperedges[e].min_selected = 1;
        // この製品では多くても1候補を採用
        p.hyperedges[e].max_selected = 1;
    }

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=4, violation=[], assignment=[-1, 0, 0, -1], scope_conflict=false, iterations=9376, elapsed_us=22609.4}
```

### 3.06 開設費と最低稼働人数を持つ拠点を選ぶ

**利用場面：** 人を配置した拠点だけ開設費を払い、開いた拠点には最低人数を置きます。閉じた拠点にまで最低人数を要求しない設定が要点です。

**解く問題・入力：** 作業員4人、候補拠点3個。各拠点を使うなら2〜4人、開設費は順に5・7・9。全作業員を配置します。

**コード：** `examples/example06.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 作業員4人、候補拠点3個
    S::Problem p(4, 3);

    for (int k = 0; k < 3; ++k) {
        // 使用する拠点に求める最低人数
        p.groups[k].lower[0] = 2;
        // 拠点の最大人数
        p.groups[k].upper[0] = 4;
        // 空の拠点ではハード下限だけを免除
        p.groups[k].lower_only_if_used = true;
        // 1人以上配置した拠点ごとに1回だけ加える費用
        p.groups[k].open_cost = 5 + 2 * k;
    }

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=5, violation=[], assignment=[0, 0, 0, 0], scope_conflict=false, iterations=13472, elapsed_us=20029.8}
```

**読み方・補足：** 使用判定は人数です。負荷0の要素が1個だけ置かれた場合も、その拠点は使用中になります。

### 3.07 負荷目標からの不足・超過を費用で調整する

**利用場面：** 容量は必ず守りつつ、理想の稼働量から離れるほど費用を課します。少しの偏りは許容し、大きな偏りを二乗費用で強く避けられます。

**解く問題・入力：** 仕事4件、グループ2個。容量は各4、理想の負荷は各2。不足aに3a+a²、超過bに2b+2b²を加えます。

**コード：** `examples/example07.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 負荷1の仕事4件を2グループへ配置
    S::Problem p(4, 2);

    // 下限0、上限unlimited、4個の係数0から作る
    S::LoadPenalty penalty;
    // 理想区間の下端。ハード制約のlowerとは別
    penalty.lower = 2;
    // 理想区間の上端。今回は負荷2だけが無罰点
    penalty.upper = 2;

    // 不足1単位ごとに費用3
    penalty.under_linear = 3;
    // 不足量の二乗にも係数1を掛ける
    penalty.under_quadratic = 1;

    // 超過1単位ごとに費用2
    penalty.over_linear = 2;
    // 超過量の二乗にも係数2を掛ける
    penalty.over_quadratic = 2;

    // operator()で費用だけ確認。結果4
    std::cout << "load=1 penalty=" << penalty(1) << '\n';

    // 各グループに共通の設定を行う
    for (auto& g : p.groups) {
        // 必ず守る容量は4。理想値2を超えても直ちに違反ではない
        g.upper[0] = 4;
        // 第0資源に理想区間からの費用を設定
        g.load_penalty[0] = penalty;
    }

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
load=1 penalty=4
Result{feasible=true, cost=0, violation=[], assignment=[1, 1, 0, 0], scope_conflict=false, iterations=7168, elapsed_us=22167.7}
```

**読み方・補足：** 空グループにもこのソフト費用は掛かります。lower_only_if_usedで免除されるのはハード下限だけです。

### 3.08 同じデータを使う仕事の共有メモリを数える

**利用場面：** 同じサーバに同じデータを使う仕事が複数あっても、データ本体のメモリは1回分です。仕事が別サーバへ散ると、その数だけ複製が必要になります。

**解く問題・入力：** 仕事3件、サーバ2台、仕事単体のメモリ各1。仕事0・1は共通データを使い、そのサイズはサーバ0で3、サーバ1で2。容量は各5です。

**コード：** `examples/example08.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 3仕事、2サーバ、メモリを表す1資源
    S::Problem p(3, 2);

    // 個別負荷と共有負荷の合計が5以下
    for (auto& g : p.groups) {
        g.upper[0] = 5;
    }

    // 共通データを使う仕事を1本の辺にまとめる
    const int e = p.add_hyperedge({0, 1});
    // この辺を使用する各サーバにメモリ3を1回だけ加える
    p.hyperedges[e].shared_load = {3};
    // サーバ1では共有メモリ2に置換
    p.hyperedges[e].shared_load_by_group = {{1, {2}}};
    // データを置くサーバ1台につき維持費2
    p.hyperedges[e].span_cost = S::CountCost::linear(2);

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=2, violation=[], assignment=[0, 0, 1], scope_conflict=false, iterations=16928, elapsed_us=22384.4}
```

**読み方・補足：** 共有負荷は全体で1回ではなく「辺×使用グループ」ごとに1回です。未採用の仕事はデータの使用を発生させません。

### 3.09 部署をまたぐ共同作業に1回だけ調整費を払う

**利用場面：** 複数部署へまたがったときだけ、会議や調整の固定費を払います。3部署へまたがっても、費用を2倍にしないモデルです。

**解く問題・入力：** 仕事4件、部署3個、各部署2件まで。仕事0・1・2の集合が2部署以上を使うと調整費7を1回加えます。

**コード：** `examples/example09.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 4仕事、3部署
    S::Problem p(4, 3);

    // 部署容量は2件
    for (auto& g : p.groups) {
        g.upper[0] = 2;
    }

    // 使用部署数0・1なら0、2以上なら一律7
    p.add_hyperedge({0, 1, 2}, S::CountCost::cut(7));

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=7, violation=[], assignment=[1, 0, 0, 1], scope_conflict=false, iterations=6912, elapsed_us=22493.3}
```

**読み方・補足：** この例では3仕事を1部署に収められないので調整費7は避けられません。connectivity(7)なら3部署で14になります。

### 3.10 重要な仕事をちょうど2か所へ分散する

**利用場面：** 同じ種類の仕事を1か所へ集め過ぎず、同時に管理先を増やし過ぎないようにします。数えるのは仕事数ではなく、仕事が存在する配置先の種類数です。

**解く問題・入力：** 仕事4件、サーバ3台、各台3件まで。仕事0・1・2が使うサーバ数をちょうど2台にします。

**コード：** `examples/example10.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 4仕事、3サーバ
    S::Problem p(4, 3);

    // 各サーバ3件まで
    for (auto& g : p.groups) {
        g.upper[0] = 3;
    }

    // 分散条件を付ける重要な仕事3件
    const int e = p.add_hyperedge({0, 1, 2});
    // この集合が少なくとも2台を使用する
    p.hyperedges[e].min_span = 2;
    // この集合が多くても2台を使用する
    p.hyperedges[e].max_span = 2;

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[0, 0, 1, 1], scope_conflict=false, iterations=6944, elapsed_us=21496.1}
```

**読み方・補足：** 要素を複製する機能ではありません。各仕事は1台に置かれ、集合全体が2台へ分散します。

### 3.11 同じチームに置けない仕事の組を表す

**利用場面：** 同時に担当すると不都合な仕事を、別チームへ分離します。辺に含めた仕事の「各チーム内の個数」を1以下にします。

**解く問題・入力：** 仕事4件、チーム2個。組(0,1)、(1,2)、(2,3)は同じチームへ置けません。各チーム2件までです。

**コード：** `examples/example11.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 4仕事、2チーム
    S::Problem p(4, 2);

    // 各チームの最大件数
    for (auto& g : p.groups) {
        g.upper[0] = 2;
    }

    for (auto pair : {
        std::vector<int>{0, 1},
        std::vector<int>{1, 2},
        std::vector<int>{2, 3}
    }) {
        // 一緒に配置できない組ごとに1辺を作る
        const int e = p.add_hyperedge(pair);
        // 各チームにこの組の仕事は最大1件
        p.hyperedges[e].local_upper = 1;
    }

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[0, 1, 0, 1], scope_conflict=false, iterations=5088, elapsed_us=20081}
```

**読み方・補足：** 3要素を同じ辺に入れてlocal_upper=1にすると、その3要素をすべて別グループへ置く条件になります。

### 3.12 全チームへ技能者を配り、チーム別に人数枠を変える

**利用場面：** 各チームに技能者を最低1人ずつ置き、特定チームには2人を求めます。共通条件と配置先別の例外を組み合わせる例です。

**解く問題・入力：** 技能者5人、チーム3個。チーム0は技能者ちょうど2人、チーム1・2は1〜3人。5人全員を配置します。

**コード：** `examples/example12.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 技能者5人、3チーム
    S::Problem p(5, 3);

    // 今回は全要素が技能者
    const int e = p.add_hyperedge({0, 1, 2, 3, 4});
    // 空チームも含む全チームに技能者1人以上を要求
    p.hyperedges[e].local_lower = 1;
    // 共通の上限は3人
    p.hyperedges[e].local_upper = 3;
    // チーム0の下限を2へ置換
    p.hyperedges[e].local_lower_by_group = {{0, 2}};
    // チーム0の上限も2へ置換
    p.hyperedges[e].local_upper_by_group = {{0, 2}};

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[0, 0, 2, 2, 1], scope_conflict=false, iterations=1952, elapsed_us=21647.4}
```

**読み方・補足：** local_lowerは「辺がすでに使っているチームだけ」に掛かる条件ではありません。未使用チームにも適用されます。

### 3.13 共通ライセンスの利用料を配置先ごとに変える

**利用場面：** 特定ソフトを使う仕事が1件でもあれば、配置先ごとにライセンス料を払います。契約済みの配置先は割安にできます。

**解く問題・入力：** 仕事3件、サーバ2台、各台2件まで。仕事0・1は同じソフトを使います。ライセンス料は通常6、サーバ1だけ2です。

**コード：** `examples/example13.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 3仕事、2サーバ
    S::Problem p(3, 2);

    // 各サーバ2件まで
    for (auto& g : p.groups) {
        g.upper[0] = 2;
    }

    // 同じライセンスを使う2仕事
    const int e = p.add_hyperedge({0, 1});
    // 各使用サーバに、人数によらず費用6
    p.hyperedges[e].local_cost = S::CountCost::activation(6);
    // サーバ1の関数を費用2へ置換
    p.hyperedges[e].local_cost_by_group = {{1, S::CountCost::activation(2)}};

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=2, violation=[], assignment=[1, 1, 0], scope_conflict=false, iterations=4448, elapsed_us=20027.5}
```

**読み方・補足：** open_costはそのグループ全体、local_costはこの辺だけの使用開始に対する費用です。

### 3.14 まとめて採用した個数に応じた料金表を使う

**利用場面：** 価格が単純な比例式にならない一括契約を扱います。料金表の添字が採用した案件の個数です。

**解く問題・入力：** 案件3件、採用枠1個、容量3。採用0・1・2・3件の契約料は0・5・7・8。未採用1件につき機会損失4を加えます。

**コード：** `examples/example14.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 3案件を採用枠0か未採用-1へ配置
    S::Problem p(3, 1);
    // 最大3案件を採用可能
    p.groups[0].upper[0] = 3;

    // 各要素に共通の設定を行う
    for (auto& v : p.vertices) {
        // 各案件の採否を選ぶ
        v.allow_reject = true;
        // 未採用1件あたりの機会損失
        v.reject_cost = 4;
    }

    // 一括契約の対象となる案件
    const int e = p.add_hyperedge({0, 1, 2});
    // 採用数0〜3の料金表
    p.hyperedges[e].selected_cost = S::CountCost::lookup({0, 5, 7, 8});

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=8, violation=[], assignment=[0, 0, 0], scope_conflict=false, iterations=32192, elapsed_us=22354.9}
```

**読み方・補足：** selected_costは配置先によらない総採用数への費用です。表は、ハード上限で除外する個数も含めて辺サイズまで用意します。

### 3.15 同じグループへの集中を非線形な混雑費用で抑える

**利用場面：** 同種の作業が同じ場所へ集中すると、待ち時間などが急増する状況です。各グループに存在する個数ごとに費用を設定します。

**解く問題・入力：** 仕事4件、グループ2個。同じグループ内の対象仕事0〜4件に対して、混雑費用0・0・1・4・9を加えます。

**コード：** `examples/example15.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 対象仕事4件、2グループ
    S::Problem p(4, 2);

    // 混雑を発生させる仕事の集合
    const int e = p.add_hyperedge({0, 1, 2, 3});
    // 各グループの個数0〜4の費用
    p.hyperedges[e].local_cost = S::CountCost::lookup({0, 0, 1, 4, 9});

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=2, violation=[], assignment=[1, 1, 0, 0], scope_conflict=false, iterations=6336, elapsed_us=22541.8}
```

**読み方・補足：** 2件ずつなら費用は1+1=2です。selected_costに同じ表を置くと全体の4件だけを数えるため、偏りを区別できません。

### 3.16 固定費・追加費・分割費と料金表を重ねる

**利用場面：** 実際の契約では「開始料」「2か所目以降の追加料」「分割時の一律料金」「例外的な加算」が同時に存在することがあります。CountCostの各成分は加算されます。

**解く問題・入力：** 関連仕事3件、グループ3個。使用数が正なら開始料5、2個目以降は1ずつ、2個以上なら分割料4、さらに表(0,0,-1,2)を加えます。

**コード：** `examples/example16.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 3仕事、3グループ
    S::Problem p(3, 3);

    // active・extra・cut_valueは0、tableは空から開始
    S::CountCost fee;
    // 個数が1以上なら開始料5
    fee.active = 5;
    // 2個目以降の1個につき追加料1
    fee.extra = 1;
    // 個数が2以上なら分割料4を1回
    fee.cut_value = 4;
    // 個数0〜3に対する追加費用。2個なら1の値引き
    fee.table = {0, 0, -1, 2};

    for (int count = 0; count <= 3; ++count) {
        // operator()で合成後の料金を確認
        std::cout << "count=" << count << " fee=" << fee(count) << '\n';
    }

    // 合成した料金を使用グループ数spanの費用にする
    p.add_hyperedge({0, 1, 2}, fee);

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
count=0 fee=0
count=1 fee=5
count=2 fee=9
count=3 fee=13
Result{feasible=true, cost=5, violation=[], assignment=[0, 0, 0], scope_conflict=false, iterations=10880, elapsed_us=25355.3}
```

**読み方・補足：** tableはactive等を置換しません。lookupで作った関数でも、後からactive等を設定すると加算されます。

### 3.17 採用人数に上下限を設けて委員を選ぶ

**利用場面：** 会議に参加する委員を、開催に必要な人数以上、定員以下で選びます。参加しない人も候補として入力に含めます。

**解く問題・入力：** 委員候補4人、参加枠1個、参加人数は2〜3人。参加費用は1・2・4・6です。

**コード：** `examples/example17.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 候補4人。0は参加、-1は不参加
    S::Problem p(4, 1);
    const std::vector<double> cost{1, 2, 4, 6};

    for (int v = 0; v < 4; ++v) {
        // 各候補の不参加を許す
        p.vertices[v].allow_reject = true;

        // 候補vが参加したときの費用
        p.vertices[v].default_cost = cost[v];
    }

    // 人数を数える候補集合
    const int e = p.add_hyperedge({0, 1, 2, 3});
    // 参加は最低2人
    p.hyperedges[e].min_selected = 2;
    // 参加は最大3人
    p.hyperedges[e].max_selected = 3;

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=3, violation=[], assignment=[0, 0, -1, -1], scope_conflict=false, iterations=11104, elapsed_us=35406.8}
```

### 3.18 既存の割当から改善を始める

**利用場面：** 人が作った計画や別処理の出力を出発点として渡します。初期案が許可先と固定先を守っていれば、容量違反を含んでいても改善対象にできます。

**解く問題・入力：** 仕事4件、グループ2個、容量各2。初期案は全仕事を0へ配置しており容量違反です。仕事0・1の分離費用5を抑えつつ修復します。

**コード：** `examples/example18.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 仕事4件、2グループ
    S::Problem p(4, 2);

    // 各グループの容量
    for (auto& g : p.groups) {
        g.upper[0] = 2;
    }

    // 仕事0・1はできるだけ同じ場所へ
    p.add_hyperedge({0, 1}, S::CountCost::connectivity(5));

    // 入力を所有するソルバー
    S solver(p, 7);
    // 長さ4。容量違反はよいが許可先・固定先違反は渡せない
    const std::vector<int> initial{0, 0, 0, 0};

    // 初期案の容量違反を確認
    std::cout << "before " << solver.evaluate(initial) << '\n';
    // 初期案から30msを目安に全要素を改善
    const auto& r = solver.improve(initial, {30'000});

    // 実行可能性と改善後の割当を表示
    std::cout << "after " << r << '\n';

    // 発見できなかった場合の明示的な処理
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
before Evaluation{feasible=false, cost=0, violation=[LoadUpper{id=0, group=0, resource=0, amount=2}]}
after Result{feasible=true, cost=0, violation=[], assignment=[0, 0, 1, 1], scope_conflict=false, iterations=18176, elapsed_us=37710.6}
```

### 3.19 変更を許した仕事だけ再配置する

**利用場面：** 確定済み計画の大部分を守り、今回見直す仕事だけを動かします。入力のfixedと異なり、可動範囲はこの改善・継続の操作に付く指定です。

**解く問題・入力：** 初期割当(0,1,1,0)、仕事4件、容量各2。仕事0・1は現状維持し、仕事2・3だけを再配置して仕事0・2の分離費用を減らします。

**コード：** `examples/example19.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 4仕事、2グループ
    S::Problem p(4, 2);

    // グループ容量
    for (auto& g : p.groups) {
        g.upper[0] = 2;
    }

    // 仕事0・2の分離を避ける
    p.add_hyperedge({0, 2}, S::CountCost::connectivity(5));

    // 問題と乱数状態を保持
    S solver(p, 7);
    // 全仕事の初期配置。許可先・固定先を守る必要がある
    const std::vector<int> initial{0, 1, 1, 0};
    // 動かしてよい要素番号。配置先の番号ではない
    const std::vector<int> movable{2, 3};
    // 範囲外の仕事0・1は初期配置を維持
    const auto& r = solver.improve(initial, {20'000}, movable);

    // assignment[0]=0、assignment[1]=1が保たれる
    std::cout << r << '\n';

    // 実行可能性の確認
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[0, 1, 0, 1], scope_conflict=false, iterations=9632, elapsed_us=26214.3}
```

**読み方・補足：** 可動範囲はresumeでも保持され、solveで解除されます。fixedで固定した要素は、movableに含めても動きません。

### 3.20 一切動かさず計画を登録し、結果を保存する

**利用場面：** 決定済み計画をソルバーへ登録し、その時点の結果を保持します。結果を参照する場合と、コピーして保存する場合の違いも確認します。

**解く問題・入力：** 仕事2件、グループ2個、初期割当(0,1)。可動範囲を空にして変更を禁止し、その結果を別変数へコピーします。

**コード：** `examples/example20.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 2仕事、2グループ。既定では容量上限なし
    S::Problem p(2, 2);

    // ソルバーを作成
    S solver(p, 7);
    // 登録する全体割当
    const std::vector<int> initial{0, 1};
    // 空vectorは誰も動かさない
    const auto& held = solver.improve(initial, {20'000}, std::vector<int>{});

    // 可動要素がないためiterations=0
    std::cout << "held " << held << '\n';

    // 値コピー。後からソルバーを操作してもこの記録は変わらない
    const S::Result saved = solver.result();
    // 時間予算0で継続。既存の可動範囲も保持
    solver.resume({0});

    // 独立した保存結果を表示
    std::cout << "saved " << saved << '\n';
    // 同期済みの最新結果を参照して表示
    std::cout << "latest " << solver.result() << '\n';
}
```

**実行時の標準出力：**

```text
held Result{feasible=true, cost=0, violation=[], assignment=[0, 1], scope_conflict=false, iterations=0, elapsed_us=18.638}
saved Result{feasible=true, cost=0, violation=[], assignment=[0, 1], scope_conflict=false, iterations=0, elapsed_us=18.638}
latest Result{feasible=true, cost=0, violation=[], assignment=[0, 1], scope_conflict=false, iterations=0, elapsed_us=0.11}
```

### 3.21 探索時間を分割し、途中から山登りに切り替える

**利用場面：** 計画を少しずつ改善したいときに、同じソルバーを継続利用します。後半は悪化を受け入れない設定に変えられます。

**解く問題・入力：** 仕事6件、グループ2個、容量各3。2つの仕事集合をまとめます。最初に20ms、続いて20msの予算で継続します。

**コード：** `examples/example21.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 6仕事、2グループ
    S::Problem p(6, 2);

    // 各グループ3件まで
    for (auto& g : p.groups) {
        g.upper[0] = 3;
    }

    // 第1集合の分離費用
    p.add_hyperedge({0, 1, 2}, S::CountCost::connectivity(5));
    // 第2集合の分離費用
    p.add_hyperedge({3, 4, 5}, S::CountCost::connectivity(4));

    // 状態を継続して使うソルバー
    S solver(p, 7);

    // 既定はtime_limit_us=100000、annealing=true
    S::Options options;
    // この呼出しに与える予算を20msへ変更
    options.time_limit_us = 20'000;

    // 新規に割当を作って探索
    std::cout << "first " << solver.solve(options) << '\n';
    // 悪化の確率的な受入れを無効化する。探索の採点には制約違反も含む
    options.annealing = false;

    // 現状態・最良解を使ってさらに20ms探索
    std::cout << "continued " << solver.resume(options) << '\n';

    // 最終結果を採用できるか確認
    if (!solver.result().feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
first Result{feasible=true, cost=0, violation=[], assignment=[1, 1, 1, 0, 0, 0], scope_conflict=false, iterations=8384, elapsed_us=24891}
continued Result{feasible=true, cost=0, violation=[], assignment=[1, 1, 1, 0, 0, 0], scope_conflict=false, iterations=21536, elapsed_us=24590.5}
```

**読み方・補足：** annealing=falseでも、表示されるcostだけが毎手で単調減少するという意味ではありません。時間・iterationsは今回の呼出し分です。

### 3.22 複数の初期化を試し、実行可能な最良結果を残す

**利用場面：** 初期配置による差を減らすため、複数回の新規探索を自分で組み立てます。異なる問題の結果を比較するのではなく、同じ条件で得た結果を比較します。

**解く問題・入力：** 仕事6件、2グループ、容量各3、2本の関係辺。シード0・1・2で各10ms探索し、実行可能な結果のうち費用が最小のものを保存します。

**コード：** `examples/example22.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 毎回同じ問題を解く
    S::Problem p(6, 2);

    // 容量条件
    for (auto& g : p.groups) {
        g.upper[0] = 3;
    }

    // 第1集合の費用
    p.add_hyperedge({0, 1, 2}, S::CountCost::connectivity(5));
    // 第2集合は一部重なっていてもよい
    p.add_hyperedge({2, 3, 4}, S::CountCost::connectivity(3));

    // シードを省略した場合は0
    S solver(p);
    // まだ実行可能な結果がない状態を区別する
    std::optional<S::Result> best;

    for (uint64_t seed = 0; seed < 3; ++seed) {
        // 乱数状態のみを設定。これだけでは解は作り直さない
        solver.set_seed(seed);
        // solveが新しい初期割当を作る。resumeとの違い
        const auto& r = solver.solve({10'000});

        // 結果をコピーし、次のsolveから独立させる
        if (r.feasible && (!best || r.cost < best->cost)) {
            best = r;
        }
    }

    if (!best) {
        std::cerr << "実行可能解を発見できませんでした\n";
        return 1;
    }

    // 保存した最良の実行可能結果を出力
    std::cout << *best << '\n';
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=3, violation=[], assignment=[0, 0, 0, 1, 1, 1], scope_conflict=false, iterations=10336, elapsed_us=10136.7}
```

**読み方・補足：** 同じシードでも時間打切りでは試行回数が変わるため、別環境で同じ割当になる保証はありません。

### 3.23 仕事・容量・共有条件を変更して再計画する

**利用場面：** 需要や設備条件が変わったとき、問題全体を組み直さずに指定部分を置換します。入力pは呼出し側にも保持し、各レコードの変更元として使います。

**解く問題・入力：** 仕事3件、2グループ。初回は容量各3。続いて仕事0の負荷を2、グループ0の容量を4、共有する仕事集合を(0,1)から(0,2)へ変更します。

**コード：** `examples/example23.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 更新元も保持しておく入力
    S::Problem p(3, 2);

    // 初回の容量
    for (auto& g : p.groups) {
        g.upper[0] = 3;
    }

    // 更新対象の辺番号を保存
    const int e = p.add_hyperedge({0, 1}, S::CountCost::connectivity(2));

    // pのコピーを所有する。以後pだけを書き換えても自動反映されない
    S solver(p, 7);

    // 初回の計画
    std::cout << "before " << solver.solve({10'000}) << '\n';
    // 手元の仕事0のレコードを修正
    p.vertices[0].load[0] = 2;
    // 全Vertexを置換して反映。差分フィールドだけの更新ではない
    solver.set_vertex(0, p.vertices[0]);
    // 手元のグループ0の容量を修正
    p.groups[0].upper[0] = 4;
    // 全Groupを置換。資源配列の長さを維持する
    solver.set_group(0, p.groups[0]);
    // 共有する集合の所属を修正
    p.hyperedges[e].vertices = {0, 2};
    // 同じ辺を使う各グループで共有負荷1
    p.hyperedges[e].shared_load = {1};
    // 費用・所属・制約など辺全体を置換
    solver.set_hyperedge(e, p.hyperedges[e]);
    // 変更を同期し、新しい条件で再評価・継続する
    const auto& r = solver.resume({20'000});

    // setter直後はresult()を呼ばず、まず同期する
    std::cout << "after " << r << '\n';

    // 更新後の条件で実行可能かを確認
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
before Result{feasible=true, cost=0, violation=[], assignment=[0, 0, 1], scope_conflict=false, iterations=7712, elapsed_us=10772.3}
after Result{feasible=true, cost=0, violation=[], assignment=[0, 1, 0], scope_conflict=false, iterations=22112, elapsed_us=20080.6}
```

### 3.24 動かせない範囲と新しい固定条件の衝突を処理する

**利用場面：** 計画を固定した後で、その範囲に矛盾する新しい条件が来た状況です。ソルバーは範囲外の仕事を勝手に移動せず、衝突を結果へ出します。

**解く問題・入力：** 初期配置は(0,0)、仕事0だけ可動。仕事1をグループ1へ固定する変更を加えるため、範囲外の配置と矛盾します。最後にsolveで全体を作り直します。

**コード：** `examples/example24.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 仕事2件、2グループ
    S::Problem p(2, 2);

    // ソルバーを作成
    S solver(p, 7);
    // 現在の配置
    const std::vector<int> initial{0, 0};
    // 仕事0だけ可動として登録。予算0なので探索なし
    solver.improve(initial, {0}, std::vector<int>{0});
    // 範囲外の仕事1に、現在と異なる固定先を要求
    p.vertices[1].fixed = 1;
    // 固定条件の変更を反映
    solver.set_vertex(1, p.vertices[1]);

    // 値コピーで衝突時の結果を保存。範囲外は動かさない
    const auto conflict = solver.resume({10'000});

    // scope_conflict=true、Domain違反、iterations=0になる
    std::cout << "conflict " << conflict << '\n';

    // 部分範囲を解除し、新しい固定条件で全体を作り直す
    const auto& repaired = solver.solve({10'000});

    // 仕事1はグループ1へ配置される
    std::cout << "repaired " << repaired << '\n';

    // 修復後の実行可能性を確認
    if (!repaired.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
conflict Result{feasible=false, cost=0, violation=[Domain{id=1, group=0, resource=-1, amount=1}], assignment=[0, 0], scope_conflict=true, iterations=0, elapsed_us=6.109}
repaired Result{feasible=true, cost=0, violation=[], assignment=[0, 1], scope_conflict=false, iterations=13600, elapsed_us=10023}
```

### 3.25 仕事数や資源数も含めて問題を入れ替える

**利用場面：** 翌日の計画などで問題の大きさが変わった場合は、個別setterではなく全体を置換します。過去の割当を新しい仕事番号へ流用しないための操作です。

**解く問題・入力：** まず仕事2件・グループ2個・資源1個を解き、その後、仕事3件・グループ1個・資源2個の問題へ切り替えます。

**コード：** `examples/example25.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 最初の問題
    S::Problem morning(2, 2);

    // 再利用するソルバー
    S solver(morning, 7);

    // 予算0でも初期配置と評価は作られる
    std::cout << "morning " << solver.solve({0}) << '\n';

    // 仕事数・グループ数・資源数を変更した問題
    S::Problem evening(3, 1, 2);
    // 新問題の2資源の容量
    evening.groups[0].upper = {3, 4};

    // 全仕事が各資源を1ずつ使う
    for (auto& v : evening.vertices) {
        v.load = {1, 1};
    }

    // 問題・割当・可動範囲を置換。乱数状態は保持される
    solver.reset_problem(evening);

    // リセット後の初回resumeは初期配置も作る
    std::cout << "evening " << solver.resume({10'000}) << '\n';

    // 同期済みの結果を参照
    if (!solver.result().feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
morning Result{feasible=true, cost=0, violation=[], assignment=[0, 0], scope_conflict=false, iterations=0, elapsed_us=33.8}
evening Result{feasible=true, cost=0, violation=[], assignment=[0, 0, 0], scope_conflict=false, iterations=0, elapsed_us=17.887}
```

### 3.26 違反の種類・場所・不足量を診断する

**利用場面：** 別システムから届いた計画が条件を満たすかを採点します。この例は、診断形式を確認するため意図的に複数の違反を持たせています。探索は行いません。

**解く問題・入力：** 仕事4件、2グループ。割当(1,1,-1,-1)を採点し、許可・固定、容量、使用先数、採用数、グループ内個数の9種類すべての違反を表示します。

**コード：** `examples/example26.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 診断する問題
    S::Problem p(4, 2);
    // 仕事0は本来グループ0へ固定
    p.vertices[0].fixed = 0;
    // この仕事の-1は有効
    p.vertices[2].allow_reject = true;
    // この仕事の-1も有効
    p.vertices[3].allow_reject = true;

    // 空のグループ0で不足を生む
    p.groups[0].lower[0] = 3;
    // グループ1に2件あると容量超過
    p.groups[1].upper[0] = 1;

    // 下限制約を診断する集合
    const int a = p.add_hyperedge({0, 1, 2});
    // 実際は1グループなのでSpanLower
    p.hyperedges[a].min_span = 2;
    // 実際は2件採用なのでSelectedLower
    p.hyperedges[a].min_selected = 3;
    // グループ0に0件なのでLocalLower
    p.hyperedges[a].local_lower = 1;

    // 上限制約を診断する集合
    const int b = p.add_hyperedge({0, 1, 2, 3});
    // 実際は1グループなのでSpanUpper
    p.hyperedges[b].max_span = 0;
    // 実際は2件採用なのでSelectedUpper
    p.hyperedges[b].max_selected = 1;
    // グループ1に2件なのでLocalUpper
    p.hyperedges[b].local_upper = 1;

    // 入力形式は有効でも、制約全体が両立するとは限らない
    S solver(p);
    // evaluateでは許可・固定違反も診断可能。数値範囲は[-1,2)
    const std::vector<int> plan{1, 1, -1, -1};

    // 値として返る採点結果。solve前でも呼べる
    const S::Evaluation e = solver.evaluate(plan);

    // costは本来の費用であり、違反への罰金を足した値ではない
    std::cout << e << '\n';

    // 全件が必要なら配列そのものを走査する
    for (const S::Violation& v : e.violation) {
        // kindは名前、id/group/resourceは場所、amountは元の単位での違反量
        std::cout << v << '\n';

        // 種類を見て、idが何の番号かを判断する
        if (v.kind == S::ViolationKind::Domain) {
            // Domainのidは要素番号
            std::cout << "許可・固定条件違反の仕事=" << v.id << '\n';
        }

        // 負荷の上限違反だけを取り出す
        if (v.kind == S::ViolationKind::LoadUpper) {
            // 元の整数単位で読む
            std::cout << "容量超過: グループ=" << v.group << " 資源=" << v.resource
                << " 超過=" << v.amount << '\n';
        }
    }

    // この診断例では違反があることを確認する
    if (e.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Evaluation{feasible=false, cost=0, violation=[Domain{id=0, group=1, resource=-1, amount=1}, LoadLower{id=0, group=0, resource=0, amount=3}, LoadUpper{id=1, group=1, resource=0, amount=1}, SpanLower{id=0, group=-1, resource=-1, amount=1}, SelectedLower{id=0, group=-1, resource=-1, amount=1}, LocalLower{id=0, group=0, resource=-1, amount=1}, SpanUpper{id=1, group=-1, resource=-1, amount=1}, SelectedUpper{id=1, group=-1, resource=-1, amount=1}, LocalUpper{id=1, group=1, resource=-1, amount=1}]}
Domain{id=0, group=1, resource=-1, amount=1}
許可・固定条件違反の仕事=0
LoadLower{id=0, group=0, resource=0, amount=3}
LoadUpper{id=1, group=1, resource=0, amount=1}
容量超過: グループ=1 資源=0 超過=1
SpanLower{id=0, group=-1, resource=-1, amount=1}
SelectedLower{id=0, group=-1, resource=-1, amount=1}
LocalLower{id=0, group=0, resource=-1, amount=1}
SpanUpper{id=1, group=-1, resource=-1, amount=1}
SelectedUpper{id=1, group=-1, resource=-1, amount=1}
LocalUpper{id=1, group=1, resource=-1, amount=1}
```

**読み方・補足：** evaluateが受け付ける違反と、不正な入力形式は別です。割当の長さ不足、-2、グループ数以上の番号は渡せません。

### 3.27 対象仕事が0件の日を同じコードで扱う

**利用場面：** 処理対象がない日にも、空の結果を取得できます。ただし、対象0件というだけでグループの下限やソフト費用が自動的に無効になるわけではありません。

**解く問題・入力：** Problemの引数をすべて省略し、仕事0件、グループ1個、資源1個、費用も制約下限も0の問題を扱います。

**コード：** `examples/example27.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 既定はn=0、k=1、r=1。空のverticesと1個のgroupsを持つ
    S::Problem p;
    // int64_tの最大値を容量上限の指定値として使う
    p.groups[0].upper[0] = S::unlimited;

    // 既定シードは0
    S solver(p);
    // 初回resumeで初期化。空問題なので割当も空
    const auto& r = solver.resume({0});

    // assignment=[]、feasible=true、cost=0
    std::cout << r << '\n';

    // 空問題の結果を確認
    if (!r.assignment.empty() || !r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[], scope_conflict=false, iterations=0, elapsed_us=23.034}
```

### 3.28 稼働可能な配置先がない仕事を明示的に除外する

**利用場面：** 停止中・保留中の仕事を要素番号ごと残したまま、通常配置を禁止します。空の許可先一覧と未採用の許可を組み合わせます。

**解く問題・入力：** 仕事2件、グループ2個。仕事0は実行可能な配置先がなく、未採用へ固定して費用3。仕事1は通常どおり配置します。

**コード：** `examples/example28.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 仕事番号を保ったまま計画する
    S::Problem p(2, 2);
    // 空集合は通常配置先を1つも許さない
    p.vertices[0].allowed_groups = std::vector<int>{};
    // 選択肢を残すため-1を許可。空集合なら必須
    p.vertices[0].allow_reject = true;
    // 未採用へ固定。fixedは許可条件を上書きしない
    p.vertices[0].fixed = -1;
    // 保留に伴う費用
    p.vertices[0].reject_cost = 3;

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=3, violation=[], assignment=[-1, 0], scope_conflict=false, iterations=15456, elapsed_us=20025.3}
```

**読み方・補足：** allowed_groupsがstd::nulloptなら全通常配置先を許します。空集合と未指定を取り違えないでください。

### 3.29 標準入力から一般的なハイパーグラフ分割を解く

**利用場面：** 実際の問題データをファイルから渡す入口です。ここでは、負荷1、1資源、必須配置、辺の分離費用という基本形の入力形式を定めます。

**解く問題・入力：** 1行目N K M、続いてK個の容量。各辺は「要素数s、分離費用w、s個の要素番号」。入力例では仕事4件、2グループ、関係辺2本を与えます。

**標準入力：**

```text
4 2 2
2 2
2 5 0 1
2 3 2 3
```

**コード：** `examples/example29.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    int n, k, m;

    if (!(std::cin >> n >> k >> m) || n < 0 || k <= 0 || m < 0) {
        return 1;
    }

    // 入力の仕事数・グループ数で作成。各仕事の負荷は1
    S::Problem p(n, k);

    for (int g = 0; g < k; ++g) {
        int64_t capacity;

        if (!(std::cin >> capacity) || capacity < 0) {
            return 1;
        }

        // 入力された非負整数の容量
        p.groups[g].upper[0] = capacity;
    }

    for (int e = 0; e < m; ++e) {
        int size;
        double weight;

        if (!(std::cin >> size >> weight) || size < 0 || !std::isfinite(weight)) {
            return 1;
        }

        std::vector<int> members(size);

        for (int& v : members) {
            if (!(std::cin >> v) || v < 0 || v >= n) {
                return 1;
            }
        }

        // 辺の使用先が増えるごとの費用
        p.add_hyperedge(members, S::CountCost::connectivity(weight));
    }

    // 入力を検査・正規化して所有
    S solver(p, 7);
    // 50msを目安に解を探索
    const auto& r = solver.solve({50'000});

    // 診断情報は標準エラーへ。長い配列は20件で省略される
    std::cerr << r << '\n';

    // 不存在の証明ではない
    if (!r.feasible) {
        std::cout << "NO_FEASIBLE_SOLUTION_FOUND\n";
        return 0;
    }

    // 本来の目的関数値を出力
    std::cout << "cost " << r.cost << '\n';

    // 割当は全件出す
    for (int v = 0; v < n; ++v) {
        std::cout << r.assignment[v] << (v + 1 == n ? '\n' : ' ');
    }

    if (n == 0) {
        std::cout << '\n';
    }
}
```

**実行時の標準出力：**

```text
cost 0
1 1 0 0
```

**実行時の標準エラー出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[1, 1, 0, 0], scope_conflict=false, iterations=58336, elapsed_us=50032.3}
```

**読み方・補足：** デバッグ演算子は配列を省略するので、提出用などの完全な解出力にはassignmentを直接走査します。入力の要素番号は0始まりです。

### 3.30 長い結果を省略表示し、必要なら違反を全件読む

**利用場面：** ログを大量の配列要素で埋めずに状況を確認します。省略されるのは表示だけで、結果構造体の中身はすべて保持されます。

**解く問題・入力：** 仕事25件、1グループ。実行可能な割当25要素を表示した後、全件未採用という許可違反25件の割当を採点します。

**コード：** `examples/example30.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 全仕事は必須配置。25個の要素を持つ結果を作る
    S::Problem p(25, 1);

    // 通常配置先は0だけ
    S solver(p);
    // 予算0でも初期割当と結果は作る
    const auto& r = solver.solve({0});

    // assignmentは20件までと... (+5)を出力
    std::cout << r << '\n';

    // 許可されていない未採用を診断
    const S::Evaluation e = solver.evaluate(std::vector<int>(25, -1));

    // violationも20件までと... (+5)を出力
    std::cout << e << '\n';
    // 実データは各25件
    std::cout << "assignment_count=" << r.assignment.size() << " violation_count="
        << e.violation.size() << '\n';

    // 省略された末尾のViolationも個別に表示可能
    if (!e.violation.empty()) {
        std::cerr << e.violation.back() << '\n';
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=0, violation=[], assignment=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (+5)], scope_conflict=false, iterations=0, elapsed_us=115.241}
Evaluation{feasible=false, cost=0, violation=[Domain{id=0, group=-1, resource=-1, amount=1}, Domain{id=1, group=-1, resource=-1, amount=1}, Domain{id=2, group=-1, resource=-1, amount=1}, Domain{id=3, group=-1, resource=-1, amount=1}, Domain{id=4, group=-1, resource=-1, amount=1}, Domain{id=5, group=-1, resource=-1, amount=1}, Domain{id=6, group=-1, resource=-1, amount=1}, Domain{id=7, group=-1, resource=-1, amount=1}, Domain{id=8, group=-1, resource=-1, amount=1}, Domain{id=9, group=-1, resource=-1, amount=1}, Domain{id=10, group=-1, resource=-1, amount=1}, Domain{id=11, group=-1, resource=-1, amount=1}, Domain{id=12, group=-1, resource=-1, amount=1}, Domain{id=13, group=-1, resource=-1, amount=1}, Domain{id=14, group=-1, resource=-1, amount=1}, Domain{id=15, group=-1, resource=-1, amount=1}, Domain{id=16, group=-1, resource=-1, amount=1}, Domain{id=17, group=-1, resource=-1, amount=1}, Domain{id=18, group=-1, resource=-1, amount=1}, Domain{id=19, group=-1, resource=-1, amount=1}, ... (+5)]}
assignment_count=25 violation_count=25
```

**実行時の標準エラー出力：**

```text
Domain{id=24, group=-1, resource=-1, amount=1}
```

### 3.31 複数の案件集合にまたがる採用枠を同時に守る

**利用場面：** 1案件が複数の審査区分や委員会の管理対象になる状況です。要素は複製せず、同じ要素を複数の辺へ所属させます。

**解く問題・入力：** 案件4件、採用予算3、各案件負荷1。区分A=(0,1,2)、区分B=(1,2,3)はそれぞれ採用最大1件。未採用の損失は9・8・7・6です。

**コード：** `examples/example31.cpp`

```cpp
// このヘッダを同じフォルダかインクルードパスに置く
#include "hypergraph_partitioner_v08.hpp"

// ライブラリ名の短縮形。以下のS::は公開型を表す
using S = HypergraphPartitioner;

int main() {
    // 案件4件、採用枠1個
    S::Problem p(4, 1);
    // 全体で使える予算
    p.groups[0].upper[0] = 3;
    const std::vector<double> lost{9, 8, 7, 6};

    for (int v = 0; v < 4; ++v) {
        // 採否を選べるようにする
        p.vertices[v].allow_reject = true;
        // 採用しなかった場合の機会損失
        p.vertices[v].reject_cost = lost[v];
    }

    for (auto members : {std::vector<int>{0, 1, 2}, std::vector<int>{1, 2, 3}}) {
        // 同じ要素1・2が複数辺に属してよい
        const int e = p.add_hyperedge(members);
        // 区分ごとに採用1件まで
        p.hyperedges[e].max_selected = 1;
    }

    // 入力pのコピーを所有するソルバー。7は乱数シード
    S solver(p, 7);
    // 20,000マイクロ秒を目安に探索。第2設定のannealingはtrue
    const auto& r = solver.solve({20'000});

    // 費用・実行可能性・割当などを表示。配列は先頭20件まで
    std::cout << r << '\n';

    // 制約を満たさない割当は採用しない。未発見は不存在の証明ではない
    if (!r.feasible) {
        return 1;
    }
}
```

**実行時の標準出力：**

```text
Result{feasible=true, cost=15, violation=[], assignment=[0, -1, -1, 0], scope_conflict=false, iterations=14368, elapsed_us=20054.9}
```

**読み方・補足：** 案件0と3なら同時採用できます。仕事を複製してしまうと別々に採否が選べてしまうため、同じ番号を複数の辺で共有します。

### サンプルの検証と再実行

掲載した31個のC++コードブロックは、同梱する`examples/example01.cpp`〜`example31.cpp`と同一です。GCC 13.3.0でC++20・C++23の双方をコンパイル・実行し、結果の実行可能性、診断名、例中の手計算値などを確認しています。検証時は`-O0`を使用しました。これはAPI使用例の動作確認であり、解品質や速度のベンチマークではありません。

Python 3.9以上があれば、本文とのコード照合を含めて全例を再実行できます。

```bash
python3 run_examples.py --standard c++20 --optimization=-O0
python3 run_examples.py --standard c++23 --optimization=-O0
```

`validation/`には標準出力・標準エラー・コンパイル結果・入力の記録を収録しています。`sample_manifest.json`には例ごとの入力と出力確認項目を記録しています。対象ヘッダのSHA-256は次のとおりです。

```text
5b720ad86a44d4fa2b06aa10842c22ead2cc93a587cde1b6cd7fb8debf055ac4
```
