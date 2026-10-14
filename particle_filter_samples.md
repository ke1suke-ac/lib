# particle_filter サンプルコード集

対象ソースは `particle_filter_v12.hpp`。C++20で使用する。通常利用向けの `ParticleFilter`、直接制御向けの `ParticleCloud`、範囲付き実数モデルを作る `UniformBox` を扱う。

第1章で目的を説明し、第2章で公開APIを参照できるように整理する。第3章の28例はそれぞれ独立したプログラムで、別の例のコードを貼り足す必要はない。目的に近い例から読み始めてもよい。

## 1. 何を解くライブラリか？

### 1.1 観測から、見えない値の不確かさを推定する

成功率が分からない装置を5回使い、4回成功したとする。「成功率は必ず80%」とは断定できない。成功率60%の装置でも4回成功することはあるし、90%の装置でも1回失敗することはある。

本ライブラリが扱うのは、このような**観測から、未知の値についてどの仮説がどれくらいありそうかを更新する問題**である。未知の値は、成功率1個でも、複数の地域コストでも、相手の性格とランダム行動率でもよい。時間とともに変わる現在位置も扱える。

ユーザーが与える中心的な情報は、「この仮説が正しいとしたら、今回の観測はどれくらい起こりやすいか」である。これは**尤度**と呼ばれる。ユーザー自身が「この仮説が正しい確率」を計算して渡すものではない。

### 1.2 求める分布

時間が経っても変わらない未知パラメータの場合、求める分布は次の形になる。

$$\pi_ {t}(\theta)=\frac{p_ {0}(\theta)\prod_ {i=1}^{t}L(y_ {i}\mid\theta,u_ {i})}{Z_ {t}}$$

| 記号 | 意味 |
| --- | --- |
| $t$ | これまでに取り込んだ観測の回数。0ならまだ観測していない |
| $i$ | 観測の番号。1から$t$まで |
| $\theta$ | 未知の値についての1つの仮説。数値、数値の組、種類、地図など |
| $p_ {0}(\theta)$ | 観測前に考えている仮説の確率または確率密度。事前分布と呼ぶ |
| $u_ {i}$ | 観測時の既知の条件。操作量、経路、当時の選択肢など。条件がなければ省略できる |
| $y_ {i}$ | 実際に得られた観測結果 |
| $L(y_ {i}\mid\theta,u_ {i})$ | 仮説と観測条件の下で、今回の結果が得られる確率または確率密度 |
| $\prod$ | 各観測の尤度をすべて掛け合わせる記号。観測0回なら積は1 |
| $Z_ {t}$ | 全体の確率が1になるように調整する共通の数。離散仮説なら分子を全仮説について足したもの |
| $\pi_ {t}(\theta)$ | 観測後に求めたい仮説の確率または確率密度。事後分布と呼ぶ |

この式は$Z_ {t}>0$のときに使える。0なら、考えているすべての仮説で観測が起こり得ず、そのモデルでは観測を説明できない。

例えば成功率0.8の仮説では、成功を1回見ると0.8、失敗を1回見ると0.2が掛かる。事前の考えに、それまでの結果の起こりやすさを重ねている。

上の積は、仮説と保存した条件を与えれば各観測をこの形で評価できるモデルを想定する。観測どうしが関係するなら、その関係に必要な情報もモデルへ含める。同じ測定結果を何度も渡すと、その分だけ証拠を重複して数えることになる。

現在位置の追跡のように未知の状態自体が変わる場合は、「状態がどう移るか」と「移った状態で観測がどう生じるか」を指定し、現時点の状態の分布を求める。固定値に対する上の積を、そのまま現在位置へ適用するものではない。

### 1.3 代表値の目的関数

分布を1つの数値や数値配列へ要約するときの標準的な目的は、**二乗誤差の平均を最小にすること**である。

$$\hat\theta_ {t}=\operatorname{argmin}_ {z}\mathbb{E}[\lVert z-\Theta_ {t}\rVert^{2}\mid D_ {t}]=\mathbb{E}[\Theta_ {t}\mid D_ {t}]$$

| 記号 | 意味 |
| --- | --- |
| $D_ {t}$ | 現時点までの観測結果と、そのときの入力条件をまとめた情報 |
| $\Theta_ {t}$ | 未知の真の数値または数値配列。固定パラメータなら時間が経っても同じ値 |
| $z$ | 代表値として採用する候補。真の値と同じ次元の数値または数値配列 |
| $\lVert z-\Theta_ {t}\rVert^{2}$ | 各成分の差を二乗して足した誤差。1変数なら普通の二乗誤差 |
| $\mathbb{E}[\cdot\mid D_ {t}]$ | 観測後の各可能性を、その起こりやすさで重み付けして取る平均 |
| $\operatorname{argmin}_ {z}$ | この平均誤差が最も小さくなる$z$を選ぶ、という意味 |
| $\hat\theta_ {t}$ | 選ばれた代表値。二乗誤差なら事後平均になる |

種類番号や地図に、そのままこの平均を取る意味はない。種類別の確率や、経路の所要時間など、知りたい数値へ変えてから集計する。角度も方向の成分へ変える方が適切な場合がある。

### 1.4 推定から行動を選ぶ場合の目的関数

AHCで最終的に必要なのが行動なら、推定値を当てはめるだけでなく、不確かさを含めた平均費用を比較できる。

$$a_ {t}^{\ast}=\operatorname{argmin}_ {a\in\mathcal{A}_ {t}}\mathbb{E}[C(a,\Theta_ {t})\mid D_ {t}]$$

| 記号 | 意味 |
| --- | --- |
| $a$ | 比較する1つの行動。経路、操作、注文量など |
| $\mathcal{A}_ {t}$ | 現時点で選べる行動の集合 |
| $C(a,\Theta_ {t})$ | 真の状態が$\Theta_ {t}$だったとき、行動$a$で発生する費用 |
| $a_ {t}^{\ast}$ | 平均費用が最も小さい行動 |
| $D_ {t}$、$\Theta_ {t}$、$\mathbb{E}$ | 前節と同じ、観測情報・未知の真値・観測後の平均 |

利益を大きくしたい場合は、利益の符号を反転した費用を考えればよい。ライブラリは任意の行動集合を自動探索するソルバーではなく、**行動を比較するための分布や平均を提供する**。行動の列挙と最小値の選択は利用側で行う。例16が具体例である。

## 2. APIリファレンス

### 2.1 利用する型と共通ルール

| 公開型 | 用途 |
| --- | --- |
| `ParticleFilter<Model, Rng = std::mt19937_64>` | 通常はこちら。観測更新、必要時の再サンプリング、固定モデルの履歴と再探索を管理する |
| `ParticleCloud<Particle>` | 粒子と重みを直接制御する。履歴・RNG・呼出順は利用側で管理する |
| `UniformBox<D = std::dynamic_extent>` | 独立な一様事前分布を持つ実数パラメータ用のモデル基底 |

公開の列挙型はない。固定モデル／動的モデル、通常尤度／対数尤度は、モデルに定義された関数によって決まる。

以下では `P` を粒子型、`O` を観測型、`Rng` を乱数器型として記す。通常は粒子も観測も値としてコピーできる型を使う。全体は単一スレッド向け。API呼び出し中のコールバックから、同じフィルターへ再入したり、粒子数・履歴などを変更したりしない。

| 共通引数・値 | 取り得る値と意味 |
| --- | --- |
| `top_k` | `int`、既定0。0は全粒子、1〜粒子数は重み上位k個。粒子数と等しい場合も全粒子。負や粒子数超えは不可 |
| 通常の確率・密度 | 有限の`double`で0以上。離散確率は0〜1だが、連続値の密度は1を超えてよい |
| 対数確率・対数密度・対数補正 | 有限の`double`または負の無限大。0確率は負の無限大。NaN・正の無限大は不可 |
| `steps` | `int`、0以上。指定が必要。0なら再探索を実行しない |
| `Rng& rng` | 標準の一様乱数生成器と互換のオブジェクト。低水準APIでは省略不可。例では`std::mt19937_64`または`std::mt19937`を使う |
| 射影`project(p)` | 粒子から数値または数値列を返す関数。数値は有限値にする。数値列はサイズ取得とランダムアクセスができ、全粒子で同じ長さにする |
| `consume(p, weight)` | 読み取り用の粒子と、その集計内で合計1になる重みを受け取る。戻り値は利用しない |

粒子数・総試行回数は`int`の範囲内、有限な対数値の加減算は`double`の範囲内とする。契約違反は主に`assert`で検出する。例外による入力エラー処理を提供するAPIではない。負の無限大を使うため、`-ffast-math`と`-Ofast`は使用しない。

### 2.2 `ParticleFilter`の構築・設定

```text
explicit ParticleFilter(Model model, Param param = {}, Rng rng = Rng(0))
```

| 引数 | 型・既定値 | 意味 |
| --- | --- | --- |
| `model` | `Model`、必須 | 下記の型・関数を提供するモデル。値として受け取り内部へ移動する。初期仮説の生成も構築中に行う |
| `param` | `ParticleFilter<Model,Rng>::Param`、`{}` | 粒子数や自動処理を調整する設定。省略したフィールドは表の既定値になる |
| `rng` | `Rng`、`Rng(0)` | 内部で保持する推定用乱数器。同じ処理を再現するにはモデル・入力・seedも揃える。独自RNGで`Rng(0)`を使えない場合は第3引数を明示する |

公開別名は`Particle`、`Observation`、`MoveResult`。それぞれモデルの粒子型・観測型・再探索結果型を示す。通常の戻り値は`auto`で受けると、内部の型を指定する必要がない。

| `Param`フィールド | 型 | 既定値 | 有効範囲・意味・選び方 |
| --- | --- | --- | --- |
| `count` | `int` | 1024 | 正。粒子数。複数パラメータや複数の有力仮説を残したいときは増やす。時間・メモリと引き換えなので1024〜4096などから実測する |
| `ess_ratio` | `double` | 0.5 | 0〜1。次の観測の冒頭で、現在のESSが`ess_ratio * count`未満なら再サンプリング。0はこの判定を無効化。大きいほど早めに行う |
| `move_steps` | `int` | 2 | 0以上。固定モデルの自動再探索で各有効粒子へ試す回数。0は自動再探索と段階更新を無効化。動的モデルでは使わない |
| `move_interval` | `int` | 8 | 0以上。固定モデルの成功観測数が正の倍数になった次の`observe`冒頭で、再サンプリングと再探索を行う。8なら成功8回の後、9回目の開始前。0で定期処理を止める。`move_steps=0`でも止まる |
| `tempering_ess_ratio` | `double` | 0.5 | 0〜1。固定モデルで鋭い観測を少しずつ反映するときのESS目安。0、または`move_steps=0`で段階更新を無効化。高くすると計算が増えやすい。動的モデルでは使わない |

ESSは重みの偏りを表す数で、等重みなら粒子数に等しく、1粒子へ集中すると1に近づく。同じ値の粒子が何種類あるかを測る数ではない。

**自動処理を全部止めたい固定モデル**では、`ess_ratio=0`、`move_steps=0`を設定する。`ess_ratio=0`だけでは、定期再探索と段階更新は止まらない。逆に段階更新だけを止めたいなら`tempering_ess_ratio=0`にする。

| メソッド | 引数・戻り値・挙動 |
| --- | --- |
| `const Param& param() const` | 引数なし。現在設定への読み取り専用参照。`auto p = filter.param();`でコピーして変更する |
| `void set_param(Param param)` | 設定を丸ごと渡す。履歴と成功観測数は保持。`count`変更時は直ちに新粒子数へ再サンプリングして等重みにする。その他の項目だけなら粒子・重み・RNGは変えない |
| `void reset()` | 引数なし。履歴と成功観測数を消し、現在の`count`で初期分布から再生成。確保領域は再利用するが、RNGのseedは戻さない |
| コピー構築・コピー代入 | モデル・設定・粒子・重み・履歴・RNGを複製する。対応するコピー操作を`Model`と`Rng`が持つことが必要。代入は`ParticleFilter&`を返す |
| ムーブ構築・ムーブ代入 | 通常のC++の所有状態の移動。代入は`ParticleFilter&`を返す。移動元はそのまま推定に使わず、破棄するか別の有効なオブジェクトを代入する |

モデルがポインタや参照で外部データを共有している場合、その参照先までコピーされるわけではない。分岐ごとに異なる値へ変更したいデータは、モデルや観測が値として持つか、別の寿命管理を行う。

### 2.3 モデルが必ず提供するもの

| 定義 | 必要な型・戻り値 | 意味 |
| --- | --- | --- |
| `using Particle = P;` | 粒子型 | 仮説が持つ情報。数値・配列・ユーザー構造体など |
| `using Observation = O;` | 観測型 | 今回の測定結果と、必要な入力条件。固定モデルでは履歴へ値として保存される |
| `P sample(Rng& rng)` | 粒子を値で返す | 事前分布から生成する。初期生成と、局所提案を持たない固定モデルの再探索で使う |
| `double likelihood(const P& p, const O& o) const` | 今回の観測の確率・密度 | 通常版。下の対数版と**どちらか一方だけ**を定義する |
| `double log_likelihood(const P& p, const O& o) const` | 今回の観測の自然対数確率・密度 | 対数版。非常に小さい値を扱うならこちら。今回の尤度を先に掛け合わせてからlogにすると桁落ち・アンダーフローを救えない |

引数には既定値はない。上の`const P&`や`const O&`は読み取りの契約であり、軽い数値型なら例のように値渡しで定義してよい。評価関数は`const Model`から呼べる必要がある。乱数生成・提案関数はモデル内に分布器などの状態を保持してもよい。

`likelihood`に、重みや事後確率を掛けて返さない。これらはライブラリ側で合成される。最小化したい費用の値を、そのまま尤度として渡すのも異なる問題である。

連続値では、正規密度の山の高さなどが1を超えてもよい。密度の規格化定数を省略できるのは、すべての仮説に共通で、相対的な推定だけが必要な場合。`log_predictive`を実際の観測確率・密度として使うなら省略しない。未知のσに依存する定数は、相対比較でも省略できない。

### 2.4 固定モデルの任意の関数

固定モデルとは、`transition`と`propose_transition`をどちらも定義しないモデルである。

| 関数 | 引数・戻り値・条件 |
| --- | --- |
| `double prior(const P& p) const` | 事前確率・密度。局所提案を定義する固定モデルでは、`log_prior`とどちらか一方が必要。`sample`と同じ分布にする |
| `double log_prior(const P& p) const` | 事前確率・密度の自然対数。仮説に共通の乗法定数は省略可能。`sample`が生成する初期粒子では有限であること |
| `void propose(P& candidate, Rng& rng)` | コピーされた旧粒子を新候補へ変更する。戻り値voidは対称提案、つまり提案確率の補正0を意味する |
| `double propose(P& candidate, Rng& rng)` | 上と同じだが非対称提案の対数補正を返す。void版と同時には定義しない |
| `auto propose_in_place(P& p, Rng& rng)` | 粒子を直接変更し、`.undo`と`.log_correction`を持つ結果を返す。`ParticleCloud<P>::MoveProposal<Token>`を使える。`propose`もある場合はこちらが優先 |
| `void undo(P& p, const Token& token)` | 差分提案の棄却時に、状態とユーザーキャッシュを完全に戻す。`propose_in_place`を定義した場合は必須 |
| `double log_likelihood_history(const P& p, const History& observations) const` | 再探索用の、保存済み全観測の対数尤度の総和。今回1回用の関数も引き続き必要。定数項も含めて通常の履歴評価と一致させ、事前分布は加えない。`const auto&`ならbool観測のrangeにも対応する |

これらの関数に既定引数はない。`propose`または`propose_in_place`を持つ場合は局所提案モデルとなり、事前密度の関数も必要になる。自動再探索を設定で止めていても、モデル定義としての必要性は変わらない。局所提案がなければ、ライブラリは`sample`から独立に新候補を作るため、別途`prior`を渡す必要はない。

非対称な局所提案の補正は次の向きである。

$$c=\log q(p_ {\mathrm{old}}\mid p_ {\mathrm{new}})-\log q(p_ {\mathrm{new}}\mid p_ {\mathrm{old}})$$

`c`が返す補正、`q(新|旧)`が旧粒子から新候補を作る確率または密度、`q(旧|新)`が逆向きの確率または密度。`p_old`と`p_new`は旧粒子と新候補。ここへ観測尤度や事前密度の差を足さない。逆向きが不可能なら負の無限大を返せる。補正が負の無限大なら候補の対象密度評価を省略し、必ず棄却する。

同じ候補を繰り返すだけの提案は動作しても探索にならない。平均を保つつもりで適当な乱数を加えるだけでは、事後分布を保つ保証はない。例11〜14に提案・制約・巻き戻しの具体例を示す。

### 2.5 動的モデルと射影の任意の関数

| 関数 | 引数・戻り値・条件 |
| --- | --- |
| `void transition(P& p, const O& o, Rng& rng)` | 旧状態のコピーを、真の状態遷移から生成した次状態へ変更する。移動指示などは`o`へ含める。観測尤度をここで掛けない |
| `double propose_transition(P& next, const P& old, const O& o, Rng& rng)` | 観測を参考に次状態を生成し、下記の対数補正を返す。`next`は最初は旧粒子のコピー。両方の遷移関数があるとこちらが優先 |
| `auto project(const P& p) const` | 引数なしの`estimate()`と射影省略版`estimate_into()`が平均する数値・数値列を返す。参照を返してコピーを避けることもできる。未定義なら粒子そのものを平均する |

遷移関数のいずれかがあると動的モデルになる。`refine`、`history`は使えず、固定モデルの局所提案・履歴評価・自動再探索・段階更新も使用しない。固定値も同時に持ちたい場合に、動的状態の一部へ入れるだけでは固定パラメータの履歴再探索は自動で行われない。

観測誘導遷移の補正は局所提案の補正とは違う。

$$c=\log f(p_ {\mathrm{new}}\mid p_ {\mathrm{old}},u)-\log q(p_ {\mathrm{new}}\mid p_ {\mathrm{old}},o)$$

`f`は本来の状態遷移の確率・密度、`q`は実際に使った提案分布、`u`は今回の移動指示など、`o`は観測情報。`c`が戻り値であり、今回の観測尤度を含めない。尤度は`likelihood`または`log_likelihood`で別に1回だけ反映する。本来の遷移と同じ分布から生成するなら補正は0。

### 2.6 `observe`と`refine`

| メソッド | 引数・戻り値・意味 |
| --- | --- |
| `ObserveResult observe(const Observation& observation)` | 観測1個を渡す。既定引数なし。固定モデルでは観測条件も履歴へ保存し、動的モデルでは遷移後の状態で評価する。成功した更新だけ確定する |
| `MoveResult refine(int steps)` | 固定モデル専用。0以上の試行回数を明示。現在の全履歴を使って追加再探索する。観測数・履歴・重みは変えず、粒子の値を変更する。自動再サンプリングは行わない |

`observe`が失敗すると、粒子・重み・履歴・成功観測数は以前のまま。返される`ObserveResult`は`ok=false`、`log_predictive=-inf`、`resampled=false`、`moves={0,0}`となる。途中で乱数やユーザーコールバックが使われた可能性はあり、乱数の消費や、コールバックが変更したモデル内部・外部の状態まで巻き戻すものではない。

成功直後は、今回の観測を反映した重みが残る。重みが偏ったからといって、必ず直ちに全部を等重みにするわけではない。これにより、直後の上位k平均も利用できる。再サンプリングは次回冒頭の判定や段階更新の途中で行われる。

### 2.7 推定・任意集計・外部への標本抽出

| メソッド | 引数・戻り値 |
| --- | --- |
| `auto estimate(int top_k = 0) const` | `Model.project`、なければ粒子自体を平均。`Estimate<平均型>`に相当する結果を返す |
| `auto estimate(Project&& project, int top_k = 0) const` | 呼び出し時の射影を使う。モデルの既定projectより優先。例：`[](const P& p){ return p.x*p.x; }` |
| `double estimate_into(Output&& buffer, int top_k = 0) const` | モデルの射影による数値列の平均を既存の出力配列へ上書きする。戻り値は選択質量 |
| `double estimate_into(Output&& buffer, Project&& project, int top_k = 0) const` | 呼び出し時の射影による数値列を集計する。戻り値は選択質量 |
| `double for_each_weighted(Consume&& consume, int top_k = 0) const` | `consume(const P&, double weight)`で自由に集計。選んだ集合の中で重みは合計1。戻り値は選択質量 |
| `void sample_indices(std::span<int> output, SamplingRng& rng) const` | 必要な標本数だけ確保した出力先と、外部の乱数器を渡す。現在の重みに従って粒子添字を独立に復元抽出する。空の出力先なら何もしない |

射影の結果が数値なら平均型は`double`、`std::array<T,D>`なら`std::array<double,D>`、`vector`・`span`などの数値列なら`std::vector<double>`。ユーザー構造体のメンバーを自動で列挙して平均する機能はない。必要な射影を定義する。重み0の粒子にも射影や集計コールバックが呼ばれる場合があるため、すべての粒子で有効な数値・配列を返す。

`Estimate`の`selected_mass`と、集計関数の戻り値が示す選択質量は、**上位k個を選ぶ前の重みで、その集合が持っていた重みの合計**。全粒子なら1。上位平均では選択質量で割った重みを使う。小さい集合ほど正確になるという意味ではなく、二乗誤差に対する標準的な代表値は全平均である。

`top_k=1`は最大重みの粒子1個。同じ種類の粒子をまとめた最頻の種類や、連続密度の最大点を保証するAPIではない。同重みでは元の添字が小さい粒子を優先する。上位k集計内の順序は整列を保証しない。

`estimate_into`の出力は、射影と同じ長さの**書き込み可能な連続したdouble列**を用意する。`vector<double>`、`array<double,D>`、`span<double>`などを渡せる。リサイズはしない。スカラーの結果には`estimate`を使う。出力先を粒子や射影が参照する入力領域と重ねない。出力先は先に0クリアされるためである。

`sample_indices`は粒子・重み・内部の推定用RNGを変更しない。外部RNGは消費する。抽出後は等重みで平均し、元の重みを再度掛けない。抽出した添字が指す仮説を使い終えるまでは、フィルターを変更しない。

### 2.8 状態の読み取り

以下はすべて引数なし。viewは読み取り専用で、次の状態変更前までに使い終える。変更後も必要なら値をコピーする。

| メソッド | 戻り値・意味 |
| --- | --- |
| `int size() const` | 現在の粒子数 |
| `double ess() const` | 現在の正規化重みのESS。1〜粒子数程度。粒子の種類数ではない |
| `int observation_count() const` | 成功した`observe`の回数。欠測を尤度1で取り込んだ回も含む |
| `auto particles() const` | `const Particle&`を要素として参照できる読み取り専用range。添字アクセス可能。具体的に`std::span`とは限らない |
| `auto weights() const` | `std::span<const double>`。合計1の通常重み。`particles()[i]`と対応する |
| `auto log_weights() const` | `std::span<const double>`。正規化済み対数重み。通常重みが0へ丸められても、こちらは有限のことがある |
| `auto history() const` | 固定モデル専用。通常は`std::span<const Observation>`。`Observation=bool`なら非連続な読み取り専用range。成功した観測だけを含む |

内部モデルを後から直接書き換える公開API、観測の削除・訂正・履歴の短縮をするAPI、高水準版へ外部粒子を直接設定するAPIはない。モデルの意味が変わった場合や過去の観測を訂正する場合は、保存した元データから再構築する。現在の値から別の量を集計するだけなら、`estimate(project)`などで再利用できる。

### 2.9 結果構造体とデバッグ出力

結果は`auto result = ...;`で受け取り、次のフィールドを使う。

| 結果型 | フィールドと意味 |
| --- | --- |
| `ObserveResult` | `bool ok`：観測を取り込めたか。`double log_predictive`：今回の観測の予測確率・密度の対数の推定値。`bool resampled`：成功した処理内で再サンプリングしたか。`MoveResult moves`：再探索の試行・受理数 |
| `UpdateResult` | `bool ok`：更新成功か。`double log_normalizer`：今回の増分重みに対する正規化定数の対数。通常の観測尤度だけを増分にした場合は予測確率・密度の対数に相当する |
| `MoveResult` | `int attempted = 0`：全有効粒子に対する試行回数の合計。`int accepted = 0`：受理回数。`accepted <= attempted`。受理率を出すときは試行0を除く |
| `Estimate<Value>` | `Value mean`：推定した平均。`double selected_mass`：選択質量。通常はAPIが値を設定する |
| `ParticleCloud<P>::MoveProposal<Undo>` | `Undo undo`：巻き戻し情報。`double log_correction = 0`：提案の対数補正。ユーザーが差分提案から返すための構造体 |

`UpdateResult`、`MoveResult`、`Estimate`、`MoveProposal`は`ParticleCloud<P>`の公開ネスト型。`ObserveResult`は`ParticleFilter<Model,Rng>`の公開ネスト型。高水準版の推定・再探索結果は内部粒子集団の型を使うため、別の`ParticleCloud<P>`の結果型へ強制変換せず`auto`で受ける。

`MoveResult`の2つのカウントと`MoveProposal`の補正以外は、手動のデフォルト構築に初期値を期待しない。例えば未初期化の`ObserveResult r;`をそのまま読まない。メソッドの戻り値として使う場合は、ライブラリが全フィールドを設定する。

最初の4種類は`std::ostream& operator<<(std::ostream&, const Xxx&)`に対応し、`std::cerr << result << '\n';`で項目名付きの出力を得られる。`MoveProposal`はこのデバッグ出力の対象外である。

配列を含む`Estimate`は先頭20要素まで表示し、長い場合は`... (size=25)`のように総要素数を付ける。値自体は省略されない。演算子は改行を付けず、数値の表示精度や形式を変更しない。`ok`と`resampled`は`true`/`false`で出す。`std::cout`も使えるが、インタラクティブ問題の提出コードではデバッグ出力を`std::cerr`へ分ける。

`log_predictive`は対数尤度の累積値ではなく、**今回分**。確率密度は1を超え得るので正になることもある。予測値を比較するなら、同じ観測単位・同じ定数項を使う。`moves`や`resampled`は状態更新に関する情報で、推定誤差が小さいことの保証ではない。

### 2.10 `UniformBox`

| API | 引数・戻り値・既定値 |
| --- | --- |
| `UniformBox<D>` | `D`は`std::size_t`型のコンパイル時の正の次元数。公開別名`Particle`は`std::array<double,D>` |
| `UniformBox<>` | `D = std::dynamic_extent`。公開別名`Particle`は`std::vector<double>`で、実行時に次元数を決める |
| `UniformBox(Particle lower, Particle upper, double move_scale = 0.2)` | 各成分の下限と上限。両方必須、同じ正の長さ、各値は有限で`lower[j] < upper[j]`。`move_scale`は有限で正。既定コンストラクタはない |
| `template<class Rng> Particle sample(Rng& rng)` | 各成分を独立に一様生成。下限以上・上限未満の分布。モデルへ継承すると初期生成に使える |
| `double log_prior(const Particle& p) const` | 全成分が下限以上・上限以下なら0、範囲外なら負の無限大。MHの比で消える密度定数は省略。長さは次元数と一致させる |
| `template<class Rng> void propose(Particle& p, Rng& rng)` | 範囲内の対称な再探索候補へ変更。戻り値なし。長さは次元数と一致させる |
| コピー・ムーブ構築／代入 | 通常のC++の値型として使える。境界と内部乱数分布器の状態も含む。代入は`UniformBox&`を返す |

`move_scale`は各成分の範囲幅に対する提案幅の目安。局所変更では最大4座標を選び、座標当たりの標準偏差は範囲幅×`move_scale`÷選択回数の平方根になる。既定0.2から始め、集中した分布で動きが大きすぎるなら0.05〜0.1などを試す。少数の全体反転提案も混ざる。単純なclampではなく境界で反射する。

範囲は観測に合わせて後から変える設定ではなく、事前分布そのもの。変更にはモデルの再構築が必要。独立な箱の一様分布では表せない合計1・整数・地図・非一様な事前分布には、例11〜14などのユーザー定義モデルを使う。

### 2.11 `ParticleCloud`の構築と初期化

| API | 引数・戻り値・既定値 |
| --- | --- |
| `explicit ParticleCloud(double ess_ratio = 0.5)` | 0〜1のESS比率を受け取る。構築直後は粒子数0。比率0は`resample_if_needed`の判定を無効化する |
| `void initialize(int count, Init&& init, Rng& rng)` | 正の粒子数、`init(rng) -> Particle`、外部乱数器を渡す。等重みで初期化。引数はすべて必須 |
| `bool assign(std::vector<Particle> particles, std::span<const double> log_weights = {})` | 空でない外部粒子を値で受け取る。重み省略は等重み。指定時は粒子数と同じ長さの対数重みを正規化。全重みが負の無限大ならfalseで旧状態を保つ |
| コピー構築・コピー代入 | ESS比率・粒子・重み・ESSを複製。作業用キャッシュは複製しない。代入先の確保済み容量を再利用。代入は`ParticleCloud&`を返す |
| ムーブ構築・ムーブ代入 | 通常のC++の所有状態の移動。代入は`ParticleCloud&`を返す。移動元はそのまま推定に使わない |

低水準版の粒子型に`bool`そのものは使わず、`struct Flag { bool value; };`などに包む。高水準`ParticleFilter`ではbool粒子とbool観測が使える。

空の集団でも`size`、粒子・重みのview取得、空出力への`sample_indices`は使える。推定・更新・ESS計算・自身の再サンプリングには、先に初期化した非空の集団が必要。`resample_from`のコピー先だけは空でもよい。

### 2.12 `ParticleCloud`の状態・重み更新

| API | 引数・戻り値・意味 |
| --- | --- |
| `UpdateResult update_log(LogIncrement&& log_increment)` | `log_increment(const Particle&) -> double`で今回の増分対数重みを返す。現在の対数重みに加えて正規化。全無効なら重みを保つ。既定引数なし |
| `void predict(Transition&& transition, Rng& rng)` | `transition(Particle&, rng)`を各粒子へ適用。重みは維持する。この後の更新が失敗しても、既に行った遷移は戻さない |
| `UpdateResult propose_update_log(Proposal&& proposal, Rng& rng)` | `proposal(const Particle& old, Particle& candidate, rng) -> double`で候補を変更し、増分対数重みを返す。候補と重みを成功時だけまとめて確定 |

`propose_update_log`の戻り補正は、低水準APIなので**今回反映する増分全体**を返す。観測誘導提案なら、遷移／提案の補正と今回の観測尤度を合計する。高水準モデルの`propose_transition`は尤度を含めない点と区別する。

対数重みが負の無限大の粒子は、`propose_update_log`では提案を実行しない。ある候補だけの増分が負の無限大なら、その粒子の状態は旧値を残して重みだけ0にする。全候補無効なら全体を保つ。いずれも外部RNGや副作用は巻き戻さない。

これらのAPIに自動の履歴保存や再サンプリングはない。`update_log`が返す正規化定数は、渡した増分が何かによって解釈が変わる。

### 2.13 `ParticleCloud`の再サンプリング

| API | 引数・戻り値・既定値 |
| --- | --- |
| `bool resample_if_needed(Rng& rng)` | `ess() < ess_ratio * size()`なら再サンプリングしてtrue。未実行ならfalseでRNGも進めない。閾値と等しいときは実行しない |
| `int resample(Rng& rng, int new_count = 0)` | 自身の分布を再サンプリング。0は現在の粒子数を維持、正はその数へ変更。戻り値は採用した異なる親添字の数 |
| `int resample_from(const ParticleCloud& source, Rng& rng, int new_count = 0)` | `source`から自身へ再サンプリング。0はsourceと同じ粒子数。別オブジェクトならsourceを保つ。自身をsourceにしてもよい。戻り値は異なる親添字の数 |

再サンプリング後は等重みで、ESSは新粒子数になる。同じ値の粒子が複製されることはある。`resample_from`はコピー先のESS判定比率を変更しない。sourceは非空、`new_count`は0以上。元の重みを残したい場合は、実施前に推定・保存を行う。

### 2.14 `ParticleCloud`の再探索：3つのオーバーロード

| 呼び出し | コールバックと戻り値 |
| --- | --- |
| `MoveResult rejuvenate(int steps, LogTarget&& target, Propose&& propose, Rng& rng)` | コピー方式。`target(const P&) -> double`は事前分布と全観測を含む対数対象密度。`propose(P& candidate, rng) -> double`は候補を変更し、逆向き／順向きの対数提案補正を返す |
| `MoveResult rejuvenate(int steps, LogTarget&& target, Propose&& propose, Undo&& undo, Rng& rng)` | 差分方式。`propose(P&,rng)`が`.undo`と`.log_correction`を持つ結果を返す。棄却時に`undo(P&, const Token&)`を呼ぶ。targetの意味はコピー方式と同じ |
| `void rejuvenate(int steps, Kernel&& kernel, Rng& rng)` | カーネル方式。`kernel(P&,rng)`を直接適用する。採否判定・対象密度評価・巻き戻しは行わない。目標分布を保つ操作をユーザーが保証する |

すべて`steps`は0以上で必須。0なら何もしない。重みは維持し、対数重みが負の無限大の粒子は試行対象外。通常重みがアンダーフローで0でも対数重みが有限なら対象になる。

コピー方式の低水準`propose`は**doubleの補正が必要**。高水準モデルのようなvoidを0として扱う省略形はない。対称提案なら`return 0.0;`とする。

MH方式のtargetは、試行開始時の有効粒子で有限でなければならない。候補が制約を破る場合などは負の無限大を返せる。提案が事後分布を勝手に変更しないよう、今回の観測だけでなく全履歴と事前分布を使う。第三のカーネル方式は任意のノイズ追加用ではない。例28は対象分布から直接引き直すため、その分布を保つ。

### 2.15 `ParticleCloud`の推定・読み取り

高水準版と共通の意味を持つが、`ParticleCloud`にはモデルがないため、既定射影の省略形はない。

| API | 引数・戻り値 |
| --- | --- |
| `auto estimate(Project&& project, int top_k = 0) const` | 射影必須。平均と選択質量を持つ`Estimate<平均型>`を返す。型変換規則は高水準版と同じ |
| `double estimate_into(Output&& buffer, Project&& project, int top_k = 0) const` | 出力先と射影必須。数値列だけに使える。選択質量を返す |
| `double for_each_weighted(Consume&& consume, int top_k = 0) const` | 集計コールバック必須。選択内で正規化された重みを渡し、選択質量を返す |
| `void sample_indices(std::span<int> output, SamplingRng& rng) const` | 出力先と外部乱数器必須。現在の重みによる独立な復元抽出。空出力なら何もしない |
| `int size() const` | 引数なし。現在の粒子数。空なら0 |
| `double ess() const` | 引数なし。非空集団のESS |
| `std::span<const Particle> particles() const` | 引数なし。読み取り専用粒子view |
| `std::span<const double> weights() const` | 引数なし。正規化済み重みview |
| `std::span<const double> log_weights() const` | 引数なし。正規化済み対数重みview |

viewの寿命、射影の型、`top_k`、出力先の長さ・非重複条件、抽出後の等重み集計は2.7節と同じ。`ParticleCloud`は観測履歴・観測回数・内部RNGを持たず、`reset`や`param`・`set_param`もない。初期化し直すときは`initialize`または`assign`を使う。

### 2.16 間違えやすい組合せ

| 状況 | 正しい扱い |
| --- | --- |
| `likelihood`と`log_likelihood`の両方を定義 | 一方だけにする。継承で片方が残る場合も両方と判定される |
| 固定モデルに局所提案を定義したが事前密度がない | `prior`または`log_prior`を1つだけ定義し、`sample`と一致させる |
| 動的モデルで`history`や`refine`を呼ぶ | 利用できない。必要な履歴は利用側で管理し、別の推定モデルを検討する |
| `prior`だけ変更して初期生成は一様のまま | 意図した事前分布にならない。初期重みにpriorを掛けるAPIではなく、sampleが事前分布から生成する契約 |
| 観測を表示するためにもう一度`observe`を呼ぶ | 観測を二重計上する。戻り値を変数へ保存して出力する |
| 種類番号・ビットマスクをそのまま平均 | 意味のある量へ射影するか、`for_each_weighted`で確率・費用を集計する |
| 上位kが粒子数を超える／kを負にする | 不可。k=0が全粒子という特別値 |
| 復元抽出した標本へ元の重みを再度掛ける | 重みを二重に反映する。抽出後は等重み |
| 過去の入力条件を外部の現在値として参照 | 再探索が過去を別条件で評価してしまう。観測に当時の条件を値で保存する |
| `0 * log(0)`を計算 | NaNになる。項の回数が0ならその項を0として分岐する |
| 全尤度0を適当に小さい正数へ置き換える | 観測モデルが変わる。必要なら誤観測や外れ値を明示したモデルにする |
| モデルの途中変更で古い分布を流用 | 意味が整合する保証はない。設定調整とモデル変更を区別し、必要なら観測から再構築する |

## 3. ユースケースと独立実行サンプル

### 3.0 実行方法と選び方

各C++コードブロックは、それ単体で`main`を含む。ヘッダー`particle_filter_v12.hpp`を同じディレクトリへ置き、コードを`main.cpp`へ保存して実行する。

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic main.cpp -o sample
./sample < input.txt
```

入力例は`input.txt`へ保存する。出力例は付属コードをGCC 13.3・C++20で実行した標準出力である。乱数seedは固定しているが、標準乱数分布器の実装が異なる環境では小数値が変わり得る。意味と許容誤差で比較する。

付属ZIPではコードは`examples/case_01.cpp`など、入力は同名の`.in`に分けてある。

```bash
g++ -std=c++20 -O2 -I. examples/case_01.cpp -o sample
./sample < examples/case_01.in
```

28個のコードを一緒にリンクすると`main`が重複する。1例ずつコンパイルする。特記がなければ観測数`n`は1以上の整数とする。例の入力は、各節に示す型・範囲を満たすものとする。入力ファイルの欠損や任意の壊れた入力を検査するプログラムではない。

`cout`は説明用の出力に使っている。AHCのインタラクティブ問題へ組み込むときは、解答出力と衝突する診断行を`cerr`へ変更し、問題で定められた入出力とflush処理を別途行う。各例はモデルと利用APIを示す最小限の題材であり、そのまま公式問題へ提出する形式ではない。

| 例・節 | 解く問題 | 主な表現 |
| --- | --- | --- |
| 1（3.1節） | 成功・失敗から装置の成功率を推定する | 成功率の実数 |
| 2（3.2節） | 同じ温度モデルを通常尤度と対数尤度で書く | 通常密度と対数密度 |
| 3（3.3節） | 操作量と実際の移動量から倍率・固定ずれを推定する | 2個の実数 |
| 4（3.4節） | 平均値と測定ノイズの大きさを一緒に推定する | 平均とノイズ幅 |
| 5（3.5節） | 地域ごとの通過時間を推定し、出力配列を再利用する | 実行時長の配列 |
| 6（3.6節） | 小数が四捨五入される測定を扱う | 丸め区間の確率 |
| 7（3.7節） | 上限で打ち切られるセンサーを扱う | 打ち切り事象の確率 |
| 8（3.8節） | 相手の行動から評価軸とランダム行動率を推定する | 行動選択の確率 |
| 9（3.9節） | 故障の種類を確率として出す | 離散的な種類 |
| 10（3.10節） | 離散的なモードと連続的な倍率を同時に推定する | 離散＋連続の構造体 |
| 11（3.11節） | 整数の隠れた能力値を近くの候補から再探索する | 整数と局所提案 |
| 12（3.12節） | 非対称な候補生成を補正し、故障率の事前知識を使う | boolと非対称提案 |
| 13（3.13節） | 大きい状態の一部を変更し、棄却時にキャッシュも戻す | 差分提案＋キャッシュ |
| 14（3.14節） | 合計1という制約を保って3種類の混合比を推定する | 合計1の制約 |
| 15（3.15節） | 角度を数値のまま平均せず、方向を推定する | 角度と方向成分 |
| 16（3.16節） | 通行止め地図を推定し、経路を選ぶ | 地図仮説と期待費用 |
| 17（3.17節） | まれに大きく外れる測定を混合モデルで扱う | 混合ノイズ |
| 18（3.18節） | 動く位置を追跡し、欠測ターンも進める | 動的状態＋欠測 |
| 19（3.19節） | 精度の高い観測に合わせて次状態を提案する | 観測誘導の遷移 |
| 20（3.20節） | 仮の未来へ分岐し、途中で計算量を調整する | 分岐・設定変更・再初期化 |
| 21（3.21節） | 観測に矛盾があるとき、失敗を検出して状態を保つ | 矛盾の検出 |
| 22（3.22節） | 長い観測履歴をまとめて評価する | 履歴評価の集約 |
| 23（3.23節） | 少数の仮説を全列挙し、厳密に重みを更新する | 重み付き全列挙 |
| 24（3.24節） | 追跡のタイミングと粒子数を直接制御する | 手動の遷移・再サンプリング |
| 25（3.25節） | 状態遷移と重み更新をまとめて確定する | 遷移と更新の一括確定 |
| 26（3.26節） | 履歴を自分で管理してコピー方式の再探索を行う | 低水準のコピーMH |
| 27（3.27節） | 2台の装置を、差分変更と巻き戻しで再探索する | 低水準の差分MH |
| 28（3.28節） | 既知の事後分布から直接引き直すカーネルを使う | 事後分布を保つ直接生成 |

### 3.1 例1：成功・失敗から装置の成功率を推定する

**具体的に解く問題**

何度押しても成功率が変わらない装置を調べる。成功を1、失敗を0として入力し、観測後の成功率を推定する。

**モデルへの対応**

粒子は成功率pという1個の数。p=0.8という仮説なら、成功の観測には0.8、失敗には0.2を返す。新しい結果をobserveへ1回ずつ渡すだけでよい。次の2回が両方成功する確率は、各仮説のp×pを計算してから平均する。pの平均を先に求めて二乗する計算とは異なる。

**入力・出力**

入力：観測数n、続いてn個の0または1。出力：最後の観測結果、推定成功率、次に2回連続で成功する確率。nは1以上。

**実行可能なコード**

同梱ファイル：`examples/case_01.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Coin {
    // 粒子1個が、装置の成功率pを表す。
    using Particle = double;

    // 1回の観測は成功true／失敗false。
    using Observation = bool;

    // 初期仮説を0以上1未満の一様分布から作る。
    double sample(mt19937_64& rng) {
        return generate_canonical<double, 53>(rng);
    }

    // 仮説pの下で今回の結果が出る確率。
    double likelihood(double p, bool success) const {
        // 「p自体が正しい確率」ではない。
        return success ? p : 1 - p;
    }
};

int main() {
    int n;
    cin >> n;

    // 1024粒子、既定設定、mt19937_64のseed=0で開始。
    ParticleFilter<Coin> filter(Coin{});
    cout << fixed << setprecision(6);

    for (int i = 0; i < n; ++i) {
        int success;
        cin >> success;

        // 今回の観測を1回だけ反映する。
        const auto result = filter.observe(success != 0);

        // 全仮説が観測を説明できない場合。ここでは通常起きない。
        if (!result.ok) {
            return 1;
        }

        // 保存した結果を直接出力できる。
        if (i == n - 1) {
            cout << result << '\n';
        }
    }

    // 全粒子の重み付き平均。粒子自体が数値なので射影は不要。
    const auto estimate = filter.estimate();

    // meanが推定値。selected_massは全体なので1。
    cout << "success_rate=" << estimate.mean << '\n';

    // 各仮説で2回連続成功の確率を計算し、平均する。
    const auto future = filter.estimate([](double p) {
        return p * p;
    });

    // 平均成功率を二乗することとは異なる。
    cout << "two_successes=" << future.mean << '\n';
}
```

**入力例**

```text
5
1 1 0 1 1
```

**実行結果**

```text
ObserveResult{ok=true, log_predictive=-0.412623, resampled=false, moves=MoveResult{attempted=0, accepted=0}}
success_rate=0.707370
two_successes=0.525133
```

### 3.2 例2：同じ温度モデルを通常尤度と対数尤度で書く

**具体的に解く問題**

一定の温度を、標準偏差0.5度の測定誤差がある温度計で測る。温度の候補範囲は0〜40度とする。

**モデルへの対応**

連続的な温度では「ぴったり20度になる確率」の代わりに確率密度を使う。誤差0の付近ほど大きくなる山型の式である。通常版は密度そのもの、対数版はその自然対数を返す。2つのモデルを別の型にし、1つのモデルに両方を定義しない。通常版では極小の密度が0へ丸められることがあり、丸めと再探索の影響で同じseedでも結果の完全一致は保証されない。小さな確率を安定して扱う場合は対数版を選ぶ。

**入力・出力**

入力：n、n個の測定温度。出力：通常版と対数版それぞれの推定温度。nは1以上。

**実行可能なコード**

同梱ファイル：`examples/case_02.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// 1次元の一様事前分布と近傍提案を利用する。
struct ProbabilityModel : UniformBox<1> {
    // 観測は測定温度。
    using Observation = double;

    // 下限、上限、範囲に対する提案幅。
    ProbabilityModel() : UniformBox<1>({0}, {40}, 0.05) {}

    // 今回の測定値の確率密度を返す。
    double likelihood(const Particle& p, double y) const {
        const double sigma = 0.5;
        const double z = (y - p[0]) / sigma;
        return exp(-0.5 * z * z) / (sigma * sqrt(2 * numbers::pi));
    }
};

// 同じ事前分布を持つ対数版。通常版を継承しない。
struct LogModel : UniformBox<1> {
    // 観測の意味は通常版と同じ。
    using Observation = double;

    // 同じ範囲・提案幅にする。
    LogModel() : UniformBox<1>({0}, {40}, 0.05) {}

    // 密度をexpにせず、その対数を返す。
    double log_likelihood(const Particle& p, double y) const {
        const double sigma = 0.5;
        const double z = (y - p[0]) / sigma;
        return -0.5 * z * z - log(sigma * sqrt(2 * numbers::pi));
    }
};

int main() {
    int n;
    cin >> n;

    // 他の設定は既定値。
    ParticleFilter<ProbabilityModel> normal(ProbabilityModel{}, {.count = 2048});

    // 同じseedと粒子数で比較する。通常版の極小密度の丸めにより完全一致は保証されない。
    ParticleFilter<LogModel> logarithmic(LogModel{}, {.count = 2048});

    for (int i = 0; i < n; ++i) {
        double y;
        cin >> y;

        // 密度版の今回の観測更新。
        if (!normal.observe(y).ok) {
            return 1;
        }

        // 対数密度版の同じ観測更新。
        if (!logarithmic.observe(y).ok) {
            return 1;
        }
    }

    cout << fixed << setprecision(6);

    // meanはarray<double,1>。
    cout << normal.estimate().mean[0] << ' ';

    // 通常版とほぼ同じ値になる。
    cout << logarithmic.estimate().mean[0] << '\n';
}
```

**入力例**

```text
4
19.8 20.2 20.1 19.9
```

**実行結果**

```text
19.992721 19.988657
```

### 3.3 例3：操作量と実際の移動量から倍率・固定ずれを推定する

**具体的に解く問題**

ロボットへ操作量xを与えると、およそa×x+bだけ進む。未知の倍率aと固定ずれbを同時に推定する。測定誤差の標準偏差は0.2。

**モデルへの対応**

1粒子を[a,b]の2要素にする。観測には結果yだけでなく、そのとき与えた操作量xも保存する。過去の観測を評価するときに現在の操作量へ置き換えてはいけない。

**入力・出力**

入力：n、n行のx y。出力：全粒子の[a,b]平均、上位64粒子の平均と選択質量。

**実行可能なコード**

同梱ファイル：`examples/case_03.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// 2個の連続値a,bをまとめて推定する。
struct Model : UniformBox<2> {
    // 過去の入力xと結果yを値で保持する。
    struct Observation {
        double x, y;
    };

    // aとbの下限・上限。提案幅は既定0.2。
    Model() : UniformBox<2>({0.5, -2}, {2, 2}) {}

    // yの対数密度。
    double log_likelihood(const Particle& p, const Observation& o) const {
        const double s = 0.2;
        const double z = (o.y - (p[0] * o.x + p[1])) / s;
        return -0.5 * z * z - log(s * sqrt(2 * numbers::pi));
    }
};

int main() {
    int n;
    cin >> n;

    // 仮説数を4096に増やす。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        // 入力条件と結果を1観測にまとめる。
        Model::Observation o;
        cin >> o.x >> o.y;

        // 履歴もライブラリが保存する。
        if (!filter.observe(o).ok) {
            return 1;
        }
    }

    // 全粒子平均が標準的な推定値。
    const auto all = filter.estimate();

    // 重み上位64粒子だけを再正規化して平均する。
    const auto top = filter.estimate(64);

    // meanとselected_massを確認する。
    cout << fixed << setprecision(6) << all << '\n' << top << '\n';

    // 上位平均は狭い仮説へ寄せる選択。全平均より正確とは限らない。
}
```

**入力例**

```text
5
-2 -2.1
-1 -0.9
0 0.3
1 1.5
2 2.7
```

**実行結果**

```text
Estimate{mean=[1.201135, 0.297421], selected_mass=1.000000}
Estimate{mean=[1.205517, 0.288665], selected_mass=0.015663}
```

### 3.4 例4：平均値と測定ノイズの大きさを一緒に推定する

**具体的に解く問題**

同じ量を繰り返し測り、真の平均μだけでなく、温度計のばらつきσも未知として推定する。

**モデルへの対応**

粒子を[μ,σ]にする。σが仮説によって変わるため、正規密度の「1/σ」も尤度に必要になる。誤差の二乗だけを使うと、大きいσが不当に有利になる。

**入力・出力**

入力：n、n個の測定値。出力：推定[平均,標準偏差]。σの候補は0.1〜3、平均は0〜30。

**実行可能なコード**

同梱ファイル：`examples/case_04.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// 粒子p[0]=平均、p[1]=誤差の標準偏差。
struct Model : UniformBox<2> {
    // 今回の測定値。
    using Observation = double;

    // σ=0は密度を定義できないので除く。
    Model() : UniformBox<2>({0, 0.1}, {30, 3}, 0.1) {}

    // 仮説ごとのσで密度を計算する。
    double log_likelihood(const Particle& p, double y) const {
        const double z = (y - p[0]) / p[1];
        return -0.5 * z * z - log(p[1]) - 0.5 * log(2 * numbers::pi);
    }
};

int main() {
    int n;
    cin >> n;

    // 2変数の候補を4096個用意する。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        double y;
        cin >> y;

        // 新しい測定を蓄積する。
        if (!filter.observe(y).ok) {
            return 1;
        }
    }

    // 平均とσを、それぞれ重み付き平均する。
    const auto result = filter.estimate();

    // 分散ではなく標準偏差の推定値が出る。
    cout << fixed << setprecision(6) << result << '\n';
}
```

**入力例**

```text
8
9.1 10.2 10.9 9.8 10.5 9.3 10.1 10.0
```

**実行結果**

```text
Estimate{mean=[9.999403, 0.732479], selected_mass=1.000000}
```

### 3.5 例5：地域ごとの通過時間を推定し、出力配列を再利用する

**具体的に解く問題**

経路ごとの合計時間だけが分かる状況で、G地域それぞれの1回通過にかかる時間を推定する。

**モデルへの対応**

1粒子は長さGの配列。各観測には地域ごとの通過回数と合計時間を入れる。予測合計は「回数×各地域の時間」の和になる。地域数が入力で決まるのでUniformBox<>を使う。

**入力・出力**

入力：G n、n行の「G個の通過回数 合計時間」。出力：地域別平均、上位64仮説の合計時間の平均と二乗平均。Gは1以上、誤差標準偏差は0.5。

**実行可能なコード**

同梱ファイル：`examples/case_05.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// 次元数が実行時に決まるvector<double>粒子。
struct Model : UniformBox<> {
    // 経路条件も値で保存する。
    struct Observation {
        vector<double> visits;
        double time;
    };

    // 各地域0〜10。
    explicit Model(int g)
        : UniformBox<>(vector<double>(g, 0), vector<double>(g, 10), 0.1) {}

    // 経路の合計時間の対数密度。
    double log_likelihood(const Particle& p, const Observation& o) const {
        double prediction = inner_product(p.begin(), p.end(), o.visits.begin(), 0.0);
        double z = (o.time - prediction) / 0.5;
        return -0.5 * z * z - log(0.5 * sqrt(2 * numbers::pi));
    }
};

int main() {
    int g, n;
    cin >> g >> n;

    // g次元のモデルを渡す。
    ParticleFilter<Model> filter(Model(g), {.count = 4096});

    // 出力用。粒子と共有しない、長さgのdouble配列を1回確保。
    vector<double> means(g);

    for (int i = 0; i < n; ++i) {
        // 観測のvectorは全粒子で共通の入力条件。
        Model::Observation o;
        o.visits.resize(g);

        for (double& x : o.visits) {
            cin >> x;
        }

        cin >> o.time;

        // 1経路分を反映する。
        if (!filter.observe(o).ok) {
            return 1;
        }

        // 出力配列を0クリアし、全成分の平均を上書きする。
        filter.estimate_into(means);
    }

    // 出力先momentsの長さは射影と同じにする。estimate_intoはリサイズしない。
    array<double, 2> moments{};

    // 上位64粒子を使う。戻り値は選択された元の重みの合計。
    // 別の量へ射影。
    const double mass = filter.estimate_into(
        span<double>(moments),
        [](const auto& p) {
            // 全地域を1回ずつ通る経路。
            double sum = accumulate(p.begin(), p.end(), 0.0);

            // 平均と二乗平均を一度に計算する。
            return array{sum, sum * sum};
        },
        64
    );

    cout << fixed << setprecision(6);

    for (double x : means) {
        cout << x << ' ';
    }

    cout << '\n' << moments[0] << ' ' << moments[1] << ' ' << mass << '\n';
}
```

**入力例**

```text
3 5
1 0 0 2
0 1 0 4
0 0 1 6
1 1 0 6
1 1 1 12
```

**実行結果**

```text
2.017074 3.968904 6.001008 
12.008279 144.201361 0.039589
```

### 3.6 例6：小数が四捨五入される測定を扱う

**具体的に解く問題**

真の値に標準偏差0.7の誤差を足し、四捨五入した整数だけが返ってくる測定器を使う。

**モデルへの対応**

整数3を得たなら、丸める前の値が2.5以上3.5未満だった確率を返す。整数3での密度だけを返すのは、別の観測モデルになる。正規分布の累積確率はerfcという標準関数で計算できる。

**入力・出力**

入力：n、n個の丸められた整数。出力：真の値の推定平均。候補範囲は0〜10。

**実行可能なコード**

同梱ファイル：`examples/case_06.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// 真の連続値は0〜10。
struct Model : UniformBox<1> {
    // 観測として届くのは整数。
    using Observation = int;

    // 一様事前分布を用いる。
    Model() : UniformBox<1>({0}, {10}) {}

    // 幅1の区間に入る確率を返す。
    double likelihood(const Particle& p, int rounded) const {
        double a = (rounded - 0.5 - p[0]) / 0.7;
        double b = (rounded + 0.5 - p[0]) / 0.7;

        // 同じ尾の差を使う。1に近い累積確率同士の引き算を避ける。
        double value = a > 0 ? 0.5 * (erfc(a / sqrt(2.0)) - erfc(b / sqrt(2.0)))
                             : 0.5 * (erfc(-b / sqrt(2.0)) - erfc(-a / sqrt(2.0)));

        // 丸め誤差で微小な負値にならないようにする。
        return max(0.0, value);
    }
};

int main() {
    int n;
    cin >> n;

    // その他の設定は既定値。
    ParticleFilter<Model> filter(Model{});

    for (int i = 0; i < n; ++i) {
        int y;
        cin >> y;

        // 丸め後の整数をそのまま渡す。
        if (!filter.observe(y).ok) {
            return 1;
        }
    }

    // 連続値として推定する。
    cout << fixed << setprecision(6) << filter.estimate().mean[0] << '\n';
}
```

**入力例**

```text
6
3 4 3 4 3 3
```

**実行結果**

```text
3.336500
```

### 3.7 例7：上限で打ち切られるセンサーを扱う

**具体的に解く問題**

センサーは5未満なら測定値を返し、5以上なら数値を返さず「5以上」とだけ知らせる。標準偏差は1。

**モデルへの対応**

数値が届いた回はその値の密度を使い、「5以上」の回は5から上の範囲に入る確率を使う。打ち切りを「5ちょうど」と扱うと、推定値が5へ不当に寄る。

**入力・出力**

入力：n、各行にflag value。flag=0は通常測定でvalue<5、flag=1は5以上でvalueを無視する。出力：推定平均。

**実行可能なコード**

同梱ファイル：`examples/case_07.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// 真の値は0〜10の範囲。
struct Model : UniformBox<1> {
    // 数値と打ち切りを明確に区別する。
    struct Observation {
        bool clipped;
        double value;
    };

    // 独立一様事前分布。
    Model() : UniformBox<1>({0}, {10}) {}

    // 観測の種類に応じた確率／密度。
    double likelihood(const Particle& p, const Observation& o) const {
        if (o.clipped) {
            return 0.5 * erfc((5 - p[0]) / sqrt(2.0));
        }

        double z = o.value - p[0];
        return exp(-0.5 * z * z) / sqrt(2 * numbers::pi);
    }
};

int main() {
    int n;
    cin >> n;

    // 測定形式の処理はModelに任せる。
    ParticleFilter<Model> filter(Model{});

    for (int i = 0; i < n; ++i) {
        int flag;
        double value;
        cin >> flag >> value;

        // flag=1のvalueは尤度内で参照しない。
        if (!filter.observe({flag != 0, value}).ok) {
            return 1;
        }
    }

    // 全仮説の平均。
    cout << fixed << setprecision(6) << filter.estimate().mean[0] << '\n';
}
```

**入力例**

```text
5
1 0
1 0
0 4.8
1 0
0 4.9
```

**実行結果**

```text
5.575478
```

### 3.8 例8：相手の行動から評価軸とランダム行動率を推定する

**具体的に解く問題**

相手は3つの行動から選ぶ。報酬と安全性の加重和が最大の行動を好むが、ときどきランダムに選ぶ。

**モデルへの対応**

報酬の重みはs、安全性の重みは1−sとして、共通倍率を取り除く。もう1つの未知値εはランダムに選ぶ確率。最大点が同点なら、その中から均等に選ぶモデルにする。各ターンの選択肢と実際の選択を観測へ保存する。

**入力・出力**

入力：n、各行に選択番号0〜2と3行動分の「報酬 安全性」計6数。出力：[報酬の重み,ランダム率]。

**実行可能なコード**

同梱ファイル：`examples/case_08.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// p[0]=報酬重みs、p[1]=ランダム率ε。
struct Model : UniformBox<2> {
    // 過去の盤面から作った特徴を保存。
    struct Observation {
        int chosen;
        array<array<double, 2>, 3> features;
    };

    // εの下限を置いたモデルを仮定する。
    Model() : UniformBox<2>({0, 0.02}, {1, 0.6}, 0.1) {}

    // 実際に選んだ行動の確率。
    double likelihood(const Particle& p, const Observation& o) const {
        array<double, 3> score;

        for (int a = 0; a < 3; ++a) {
            score[a] = p[0] * o.features[a][0] + (1 - p[0]) * o.features[a][1];
        }

        double best = *max_element(score.begin(), score.end());
        int ties = int(count(score.begin(), score.end(), best));

        // 同点判定も相手の仕様に合わせる。この例ではdoubleの完全一致を使う。
        return p[1] / 3 + (score[o.chosen] == best ? (1 - p[1]) / ties : 0);
    }
};

int main() {
    int n;
    cin >> n;

    // 相手ごとに別インスタンスを作れば分離できる。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        // 選択番号も観測の一部。
        Model::Observation o;
        cin >> o.chosen;

        for (auto& row : o.features) {
            cin >> row[0] >> row[1];
        }

        // 現在の盤面ではなく当時の特徴を評価する。
        if (!filter.observe(o).ok) {
            return 1;
        }
    }

    // 2個の内部パラメータを出力。
    cout << fixed << setprecision(6) << filter.estimate() << '\n';
}
```

**入力例**

```text
4
0 10 0 0 10 4 4
1 0 10 10 0 3 3
2 0 8 2 6 9 1
0 8 2 6 4 2 8
```

**実行結果**

```text
Estimate{mean=[0.744180, 0.224538], selected_mass=1.000000}
```

### 3.9 例9：故障の種類を確率として出す

**具体的に解く問題**

装置の状態は正常・故障A・故障Bの3種類。警報の出方から、どれであるかを推定する。

**モデルへの対応**

粒子は種類番号。番号を平均すると「故障0.8」のような無意味な数になるため、種類ごとの0/1配列へprojectし、その平均を各種類の確率として使う。

**入力・出力**

入力：n、n個の警報番号0〜2。出力：[正常,故障A,故障B]の推定確率。警報確率表はコード内で変更できる。

**実行可能なコード**

同梱ファイル：`examples/case_09.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 0=正常、1=故障A、2=故障Bという仮説。
    using Particle = int;

    // 今回鳴った警報の番号。
    using Observation = int;

    // 3種類を同じ事前確率で生成する。
    int sample(mt19937_64& rng) {
        return uniform_int_distribution<int>(0, 2)(rng);
    }

    // 行ごとの合計が1の警報確率表。
    double likelihood(int type, int alarm) const {
        const double table[3][3] = {
            {0.8, 0.1, 0.1}, // 正常なら警報0が出やすい。
            {0.1, 0.8, 0.1}, // 故障Aなら警報1が出やすい。
            {0.1, 0.2, 0.7}, // 故障Bなら警報2が出やすい。
        };
        return table[type][alarm];
    }

    // 既定のestimateで集計する量を定義する。
    array<double, 3> project(int type) const {
        array<double, 3> one_hot{};
        one_hot[type] = 1;
        return one_hot;
    }
};

int main() {
    int n;
    cin >> n;

    // 重複粒子も含めて各仮説の確率を近似する。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        int alarm;
        cin >> alarm;

        // 今回の警報だけを渡す。
        if (!filter.observe(alarm).ok) {
            return 1;
        }
    }

    // projectの3成分の平均を出す。
    cout << fixed << setprecision(6) << filter.estimate() << '\n';

    // 仮説が3個しかない場合の厳密な全列挙は、例23のParticleCloud方式も使える。
}
```

**入力例**

```text
4
1 1 0 1
```

**実行結果**

```text
Estimate{mean=[0.013429, 0.971195, 0.015376], selected_mass=1.000000}
```

### 3.10 例10：離散的なモードと連続的な倍率を同時に推定する

**具体的に解く問題**

センサーは通常モードか、一定のずれが加わるモードのどちらかで、さらに倍率も未知。入力xと読み値yから両方を推定する。

**モデルへの対応**

1粒子を構造体にして、モードboolと倍率doubleをまとめる。modeがtrueなら出力に3を加えるという具体的なモデルにする。平均したい量は[ずれモードの確率,倍率]と明示する。

**入力・出力**

入力：n、n行のx y。出力：ずれモード確率と倍率の平均。測定誤差標準偏差は0.5。

**実行可能なコード**

同梱ファイル：`examples/case_10.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 1粒子の中でモードと倍率の組を保持する。
    struct Particle {
        bool shifted;
        double gain;
    };

    // 入力条件と出力を保存する。
    struct Observation {
        double x, y;
    };

    // モードは半々、倍率は0.5〜2の一様分布。
    Particle sample(mt19937_64& rng) {
        return {
            bernoulli_distribution(0.5)(rng),
            uniform_real_distribution<double>(0.5, 2)(rng)
        };
    }

    // この仮説での測定密度。
    double log_likelihood(const Particle& p, const Observation& o) const {
        double predicted = p.gain * o.x + (p.shifted ? 3 : 0);
        double z = (o.y - predicted) / 0.5;
        return -0.5 * z * z - log(0.5 * sqrt(2 * numbers::pi));
    }

    // boolを0/1へ変換して確率を集計する。
    array<double, 2> project(const Particle& p) const {
        return {double(p.shifted), p.gain};
    }
};

int main() {
    int n;
    cin >> n;

    // 局所提案なしなのでsampleから再探索候補を作る。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        // 値で保存できる観測構造体。
        Model::Observation o;
        cin >> o.x >> o.y;

        // 固定モード・固定倍率に対する観測更新。
        if (!filter.observe(o).ok) {
            return 1;
        }
    }

    // 構造体そのものの平均は求めない。
    cout << fixed << setprecision(6) << filter.estimate() << '\n';
}
```

**入力例**

```text
4
0 3.1
1 4.2
2 5.4
3 6.6
```

**実行結果**

```text
Estimate{mean=[1.000000, 1.199076], selected_mass=1.000000}
```

### 3.11 例11：整数の隠れた能力値を近くの候補から再探索する

**具体的に解く問題**

整数0〜20の能力値を、誤差を含むテスト結果から推定する。候補は必ず整数であり、実数を平均してから丸めることと、最も有力な整数を選ぶことは区別する。

**モデルへの対応**

sampleは事前分布、priorはその分布の確率、proposeは再探索用の候補変更。±1の対称な提案なので補正値は不要。範囲外の候補はprior=0により棄却される。

**入力・出力**

入力：n、n個の測定値。出力：整数能力値の平均と、重み最大の1粒子の能力値。

**実行可能なコード**

同梱ファイル：`examples/case_11.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 候補は整数のまま保持する。
    using Particle = int;

    // テスト結果は誤差付き実数。
    using Observation = double;

    // 一様な整数事前分布から生成する。
    int sample(mt19937_64& rng) {
        return uniform_int_distribution<int>(0, 20)(rng);
    }

    // sampleと同じ事前分布。
    double prior(int p) const {
        return 0 <= p && p <= 20 ? 1.0 / 21 : 0;
    }

    // void戻り値は対称提案の意味。採否判定はライブラリ側。
    void propose(int& p, mt19937_64& rng) {
        // clampしない。範囲外はpriorで棄却する。
        p += bernoulli_distribution(0.5)(rng) ? 1 : -1;
    }

    // 標準偏差0.8の測定密度。
    double log_likelihood(int p, double y) const {
        double z = (y - p) / 0.8;
        return -0.5 * z * z - log(0.8 * sqrt(2 * numbers::pi));
    }
};

int main() {
    int n;
    cin >> n;

    // 固定整数パラメータとして自動再探索する。
    ParticleFilter<Model> filter(Model{}, {.count = 2048});

    for (int i = 0; i < n; ++i) {
        double y;
        cin >> y;

        // 観測を追加。
        if (!filter.observe(y).ok) {
            return 1;
        }
    }

    // 平均はdoubleになる。
    cout << fixed << setprecision(6) << filter.estimate().mean << ' ';

    // 最大重みの1粒子。種類ごとの確率最大とは限らない。
    cout << filter.estimate(1).mean << '\n';
}
```

**入力例**

```text
4
7.2 6.8 7.1 7.0
```

**実行結果**

```text
7.014746 7.000000
```

### 3.12 例12：非対称な候補生成を補正し、故障率の事前知識を使う

**具体的に解く問題**

装置が故障している事前確率は30%。故障なら警報が90%、正常なら20%の確率で鳴る。新しい候補を故障寄りに作る再探索を使う。

**モデルへの対応**

候補生成は故障を80%で選ぶので、そのまま採用すると推定が80%へ寄ってしまう。proposeの戻り値で、戻る向きと進む向きの提案確率の比を対数で補正する。観測尤度や事前確率はここへ重ねて入れない。

**入力・出力**

入力：n、n個の警報0/1。出力：故障確率。bool粒子は高水準ParticleFilterで使用できる。

**実行可能なコード**

同梱ファイル：`examples/case_12.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // true=故障、false=正常。
    using Particle = bool;

    // true=警報が鳴った。
    using Observation = bool;

    // 事前故障率30%。
    bool sample(mt19937_64& rng) {
        return bernoulli_distribution(0.3)(rng);
    }

    // sampleと一致する事前確率の対数。
    double log_prior(bool p) const {
        return log(p ? 0.3 : 0.7);
    }

    // 非対称提案なのでdoubleの補正を返す。
    double propose(bool& p, mt19937_64& rng) {
        bool old = p;
        p = bernoulli_distribution(0.8)(rng);

        // log q(旧|新)−log q(新|旧)。
        return log(old ? 0.8 : 0.2) - log(p ? 0.8 : 0.2);
    }

    // 仮説ごとの、今回の警報の確率。
    double likelihood(bool p, bool alarm) const {
        double rate = p ? 0.9 : 0.2;
        return alarm ? rate : 1 - rate;
    }
};

int main() {
    int n;
    cin >> n;

    // boolを内部で扱える高水準版を使う。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        int alarm;
        cin >> alarm;

        // 警報を1回ずつ反映する。
        if (!filter.observe(alarm != 0).ok) {
            return 1;
        }
    }

    // 最新の全観測を使い、各有効粒子を3回再探索する。
    const auto moved = filter.refine(3);

    // boolの平均=故障確率。
    cout << fixed << setprecision(6) << filter.estimate().mean << '\n';

    // 試行数と受理数。表示のためにrefineをもう一度呼ばない。
    cout << moved << '\n';
}
```

**入力例**

```text
3
1 1 0
```

**実行結果**

```text
0.523115
MoveResult{attempted=12288, accepted=9909}
```

### 3.13 例13：大きい状態の一部を変更し、棄却時にキャッシュも戻す

**具体的に解く問題**

G区画のコストを推定する。個別区画を測ることも、全区画の合計を測ることもある。全体をコピーせず、1区画だけ変更したい。

**モデルへの対応**

粒子は区画配列と合計値のキャッシュ。propose_in_placeで1区画と合計を変え、古い値をUndo情報として返す。棄却されたらundoが両方を元へ戻す。これは既存の仮説を調べ直す操作であり、現実の時間変化ではない。

**入力・出力**

入力：G n、n行の区画番号と測定値。番号−1は全区画の合計、それ以外は0〜G−1。出力：区画コストの推定配列。誤差標準偏差は0.5。

**実行可能なコード**

同梱ファイル：`examples/case_13.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 合計はcostのキャッシュ。
    struct Particle {
        vector<double> cost;
        double sum;
    };

    // index=-1なら合計を観測。
    struct Observation {
        int index;
        double value;
    };

    // 棄却時に戻す情報だけを保存。
    struct Undo {
        int index;
        double old_value, old_sum;
    };

    int dimension;

    // 区画ごとに0〜5の一様事前分布。
    Particle sample(mt19937_64& rng) {
        Particle p{vector<double>(dimension), 0};

        for (double& x : p.cost) {
            x = uniform_real_distribution<double>(0, 5)(rng);
            p.sum += x;
        }

        return p;
    }

    // 範囲内は一定密度。範囲外は不可能。
    double log_prior(const Particle& p) const {
        for (double x : p.cost) {
            if (x < 0 || x > 5) {
                return -numeric_limits<double>::infinity();
            }
        }

        // MHの比で消える共通定数は省略できる。
        return 0;
    }

    // コピーせず、その粒子自身を変更する。
    auto propose_in_place(Particle& p, mt19937_64& rng) {
        int i = uniform_int_distribution<int>(0, dimension - 1)(rng);
        Undo old{i, p.cost[i], p.sum};
        p.cost[i] += normal_distribution<double>(0, 0.3)(rng);
        p.sum += p.cost[i] - old.old_value;

        // 対称提案なのでlog_correction=0。
        return ParticleCloud<Particle>::MoveProposal<Undo>{old, 0};
    }

    // 棄却時、ユーザーキャッシュも完全に戻す。
    void undo(Particle& p, const Undo& old) {
        p.cost[old.index] = old.old_value;
        p.sum = old.old_sum;
    }

    // 合計観測はキャッシュを利用。
    double log_likelihood(const Particle& p, const Observation& o) const {
        double prediction = o.index == -1 ? p.sum : p.cost[o.index];
        double z = (o.value - prediction) / 0.5;
        return -0.5 * z * z - log(0.5 * sqrt(2 * numbers::pi));
    }

    // 配列をコピーせず射影する。
    const vector<double>& project(const Particle& p) const {
        return p.cost;
    }
};

int main() {
    int g, n;
    cin >> g >> n;

    // Gは正の整数で、生成後は固定。
    ParticleFilter<Model> filter(Model{g}, {.count = 2048});

    for (int i = 0; i < n; ++i) {
        // 区画番号も履歴に保持する。
        Model::Observation o;
        cin >> o.index >> o.value;

        // 必要時の再探索に差分提案が自動で使われる。
        if (!filter.observe(o).ok) {
            return 1;
        }
    }

    // 21成分以上なら表示だけが省略される。
    cout << fixed << setprecision(6) << filter.estimate() << '\n';
}
```

**入力例**

```text
3 4
0 1
1 2
2 3
-1 6
```

**実行結果**

```text
Estimate{mean=[1.002760, 2.009921, 2.991288], selected_mass=1.000000}
```

### 3.14 例14：合計1という制約を保って3種類の混合比を推定する

**具体的に解く問題**

袋から復元抽出した品物の種類0・1・2を観測し、袋の中の構成割合を推定する。3つの割合の合計は1。

**モデルへの対応**

独立に3数を動かすと合計1の制約を壊す。そこで2成分の合計を保ったまま、その内訳だけを変更する。初期分布は「どの組合せも同じ密度」の三角形上の一様分布を使う。

**入力・出力**

入力：n、n個の種類番号0〜2。出力：3種類の構成割合。

**実行可能なコード**

同梱ファイル：`examples/case_14.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 非負で合計1の配列。
    using Particle = array<double, 3>;

    // 抽出された種類。
    using Observation = int;

    // 独立な指数乱数を正規化すると一様な混合比を作れる。
    Particle sample(mt19937_64& rng) {
        Particle p;
        double total = 0;

        for (double& x : p) {
            x = exponential_distribution<double>(1)(rng);
            total += x;
        }

        for (double& x : p) {
            x /= total;
        }

        return p;
    }

    // 制約内の密度は一定。
    double log_prior(const Particle& p) const {
        for (double x : p) {
            if (x < 0 || x > 1) {
                return -numeric_limits<double>::infinity();
            }
        }

        return abs(accumulate(p.begin(), p.end(), 0.0) - 1) < 1e-10
                   ? 0
                   : -numeric_limits<double>::infinity();
    }

    // 2成分の合計を保つ対称提案。
    void propose(Particle& p, mt19937_64& rng) {
        int i = uniform_int_distribution<int>(0, 2)(rng);
        int j = (i + 1 + uniform_int_distribution<int>(0, 1)(rng)) % 3;
        double total = p[i] + p[j];
        double fraction = generate_canonical<double, 53>(rng);

        // 範囲と合計の制約を同時に保つ。
        p[i] = total * fraction;
        p[j] = total - p[i];
    }

    // 観測種類の確率。
    double likelihood(const Particle& p, int category) const {
        return p[category];
    }
};

int main() {
    int n;
    cin >> n;

    // 全粒子が制約を満たすように生成する。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        int category;
        cin >> category;

        // 種類の情報を蓄積する。
        if (!filter.observe(category).ok) {
            return 1;
        }
    }

    // 成分平均も合計1になる。
    cout << fixed << setprecision(6) << filter.estimate() << '\n';
}
```

**入力例**

```text
8
0 0 1 0 2 1 0 0
```

**実行結果**

```text
Estimate{mean=[0.549169, 0.271123, 0.179708], selected_mass=1.000000}
```

### 3.15 例15：角度を数値のまま平均せず、方向を推定する

**具体的に解く問題**

未知の向きを、誤差付きのx方向・y方向成分から測る。179度と−179度は近い方向なので、単純平均して0度にしたくない。

**モデルへの対応**

粒子は角度だが、projectは[cos角度,sin角度]を返す。方向成分を平均してからatan2で角度へ戻す。平均ベクトルが短い場合は向きが曖昧であり、その長さも出す。

**入力・出力**

入力：n、n行の測定x y。出力：方向の角度（度）と平均ベクトルの長さ。成分誤差標準偏差は0.2。

**実行可能なコード**

同梱ファイル：`examples/case_15.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// p[0]はラジアン。−πからπの範囲。
struct Model : UniformBox<1> {
    // 誤差付きの方向成分。
    using Observation = array<double, 2>;

    // 一様な方向の事前分布。
    Model() : UniformBox<1>({-numbers::pi}, {numbers::pi}) {}

    // x,yの独立な正規密度の対数。
    double log_likelihood(const Particle& p, const Observation& o) const {
        double dx = (o[0] - cos(p[0])) / 0.2;
        double dy = (o[1] - sin(p[0])) / 0.2;
        return -0.5 * (dx * dx + dy * dy) - log(2 * numbers::pi * 0.2 * 0.2);
    }

    // 方向へ射影。
    array<double, 2> project(const Particle& p) const {
        return {cos(p[0]), sin(p[0])};
    }
};

int main() {
    int n;
    cin >> n;

    // 角度そのものの平均は使わない。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});

    for (int i = 0; i < n; ++i) {
        // 2成分が1回分の観測。
        Model::Observation o;
        cin >> o[0] >> o[1];

        // この測定を反映。
        if (!filter.observe(o).ok) {
            return 1;
        }
    }

    // Model.projectの2成分を平均する。
    const auto direction = filter.estimate().mean;
    double length = hypot(direction[0], direction[1]);
    cout << fixed << setprecision(6)
         << atan2(direction[1], direction[0]) * 180 / numbers::pi << ' ' << length
         << '\n';

    // lengthがほぼ0なら、角度を断定せず追加観測や保守的な行動を検討する。
}
```

**入力例**

```text
3
-1 0.02
-0.9 -0.01
-1.1 0.03
```

**実行結果**

```text
179.554743 0.993695
```

### 3.16 例16：通行止め地図を推定し、経路を選ぶ

**具体的に解く問題**

4区間のどこが通行止めか分からない。区間ごとの不正確な検査から地図を推定し、区間0・1を通る経路Aと区間2・3を通る経路Bを比較する。

**モデルへの対応**

1粒子が地図全体を表す4ビットの整数。費用は、Aが基本2、Bが基本3、通行止めを1つでも通るなら追加20とする。全粒子での期待費用と、少数仮説を抽出した近似費用の両方を求める。

**入力・出力**

入力：n、n行の区間番号0〜3と検査結果0/1。1は通行止めと報告された意味。出力：厳密な粒子集計費用A B、抽出による費用A B、選ぶ経路番号0/1。

**実行可能なコード**

同梱ファイル：`examples/case_16.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 下位4ビットが各区間の通行止め仮説。
    using Particle = unsigned;

    // 検査した区間と報告。
    struct Observation {
        int edge;
        bool blocked;
    };

    // この例は標準32bit乱数器を明示して使用する。
    unsigned sample(mt19937& rng) {
        unsigned mask = 0;

        for (int i = 0; i < 4; ++i) {
            if (bernoulli_distribution(0.2)(rng)) {
                mask |= 1u << i;
            }
        }

        return mask;
    }

    // 検査は90%正しい。
    double likelihood(unsigned mask, const Observation& o) const {
        return bool((mask >> o.edge) & 1) == o.blocked ? 0.9 : 0.1;
    }
};

double cost(unsigned map, int route) {
    return (route == 0 ? 2.0 : 3.0) + ((map & (route == 0 ? 3u : 12u)) ? 20 : 0);
}

int main() {
    int n;
    cin >> n;

    // RNG型とseedを明示。
    ParticleFilter<Model, mt19937> filter(Model{}, {.count = 4096}, mt19937(7));

    for (int i = 0; i < n; ++i) {
        int edge, blocked;
        cin >> edge >> blocked;

        // 区間検査を地図仮説へ反映する。
        if (!filter.observe({edge, blocked != 0}).ok) {
            return 1;
        }
    }

    array<double, 2> exact{}, sampled{};

    // top_k省略は全粒子。コールバック内でfilterを変更しない。
    // 正規化重みで任意の量を集計する。
    filter.for_each_weighted([&](unsigned map, double weight) {
        for (int r = 0; r < 2; ++r) {
            exact[r] += weight * cost(map, r);
        }
    });

    vector<int> indices(256);
    mt19937_64 sampling_rng(99);

    // 評価用の外部RNGで、復元抽出。推定用RNGを進めない。
    filter.sample_indices(indices, sampling_rng);

    // 読み取り専用view。次の状態変更までに使い終える。
    const auto particles = filter.particles();

    // 抽出結果は等重みで平均する。元の重みをもう一度掛けない。
    for (int i : indices) {
        for (int r = 0; r < 2; ++r) {
            sampled[r] += cost(particles[i], r) / indices.size();
        }
    }

    cout << fixed << setprecision(6) << exact[0] << ' ' << exact[1] << '\n';
    cout << sampled[0] << ' ' << sampled[1] << '\n' << int(exact[1] < exact[0]) << '\n';
}
```

**入力例**

```text
4
0 1
1 0
2 0
3 0
```

**実行結果**

```text
15.861727 3.988024
16.296875 4.250000
1
```

### 3.17 例17：まれに大きく外れる測定を混合モデルで扱う

**具体的に解く問題**

通常は標準偏差1の誤差だが、5%の確率で−20〜50の無関係な値を返すセンサーから一定温度を推定する。

**モデルへの対応**

「通常測定の密度×95%」と「異常測定の密度×5%」を足す。外れ値だからと後から都合よく捨てず、外れ方も観測モデルの一部として指定する。

**入力・出力**

入力：n、n個の測定値。出力：推定温度。真値の事前範囲は0〜30。

**実行可能なコード**

同梱ファイル：`examples/case_17.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// 真値だけを推定し、外れ率5%は既知とする。
struct Model : UniformBox<1> {
    // 観測された値。
    using Observation = double;

    // 真値候補の範囲。
    Model() : UniformBox<1>({0}, {30}, 0.1) {}

    // 2つの起こり方の確率密度を足す。
    double likelihood(const Particle& p, double y) const {
        double z = y - p[0];
        double normal = exp(-0.5 * z * z) / sqrt(2 * numbers::pi);
        double outlier = -20 <= y && y <= 50 ? 1.0 / 70 : 0;
        return 0.95 * normal + 0.05 * outlier;
    }
};

int main() {
    int n;
    cin >> n;

    // 計算手順は通常の測定と同じ。
    ParticleFilter<Model> filter(Model{}, {.count = 2048});

    for (int i = 0; i < n; ++i) {
        double y;
        cin >> y;

        // 外れ値も指定したモデルで評価する。
        if (!filter.observe(y).ok) {
            return 1;
        }
    }

    // 温度の全粒子平均。
    cout << fixed << setprecision(6) << filter.estimate().mean[0] << '\n';
}
```

**入力例**

```text
6
10.2 9.8 10.1 39.0 10.3 9.9
```

**実行結果**

```text
10.059661
```

### 3.18 例18：動く位置を追跡し、欠測ターンも進める

**具体的に解く問題**

対象は毎ターン、入力された移動指示uに加えてランダムなずれだけ動く。位置センサーが欠測するターンもある。

**モデルへの対応**

固定パラメータと違い、粒子自体の位置をtransitionで進める。observeは遷移してから新位置の測定を評価する。欠測なら尤度1を返せば、測定で重みを変えず時間だけ進められる。

**入力・出力**

入力：n、n行のu flag y。flag=1なら測定あり、0ならyを無視。出力：各ターンの位置平均と成功観測数。初期位置は平均0・標準偏差2。

**実行可能なコード**

同梱ファイル：`examples/case_18.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 現在位置。時間とともに変化する。
    using Particle = double;

    // 移動指示と測定を一緒に渡す。
    struct Observation {
        double control;
        bool measured;
        double value;
    };

    // 移動前の初期位置。
    double sample(mt19937_64& rng) {
        return normal_distribution<double>(0, 2)(rng);
    }

    // 真の状態遷移をサンプルする。
    void transition(double& x, const Observation& o, mt19937_64& rng) {
        // この定義で動的モデルとして扱われる。
        x += o.control + normal_distribution<double>(0, 0.3)(rng);
    }

    // 遷移後の位置を評価する。
    double log_likelihood(double x, const Observation& o) const {
        // log(1)=0。欠測でもobserveは成功し、ターン数が増える。
        if (!o.measured) {
            return 0;
        }

        double z = (o.value - x) / 0.5;
        return -0.5 * z * z - log(0.5 * sqrt(2 * numbers::pi));
    }
};

int main() {
    int n;
    cin >> n;

    // 動的モデルでは固定モデル用の再探索設定は使われない。
    ParticleFilter<Model> filter(Model{}, {.count = 4096});
    cout << fixed << setprecision(6);

    for (int i = 0; i < n; ++i) {
        double u, y;
        int flag;
        cin >> u >> flag >> y;

        // 遷移と測定更新を成功時にまとめて確定する。
        const auto result = filter.observe({u, flag != 0, y});

        // 失敗時は位置も元の状態のまま。
        if (!result.ok) {
            return 1;
        }

        // 現時点の位置の平均。
        cout << filter.estimate().mean << ' ';

        // 欠測も含む、成功したobserveの回数。
        cout << filter.observation_count() << '\n';
    }

    // 動的モデルではrefineとhistoryは利用できない。observe外で重複して遷移させない。
}
```

**入力例**

```text
4
1 1 0.8
1 0 0
1 1 3.2
-1 1 2.1
```

**実行結果**

```text
0.799887 1
1.798315 2
3.056085 3
2.074191 4
```

### 3.19 例19：精度の高い観測に合わせて次状態を提案する

**具体的に解く問題**

移動のずれは大きい一方、位置測定はかなり正確な追跡問題。移動だけで作った候補では、観測付近の候補が少なくなる。

**モデルへの対応**

旧位置と今回の測定値の両方から次位置を生成する。その生成分布は本来の移動分布と違うので、propose_transitionは「本来の移動密度÷実際の提案密度」の対数を返す。観測尤度はlog_likelihoodで別に反映する。コードで使う分散は標準偏差の二乗。移動予測と測定値を分散の逆数で重み付けして混ぜるので、ばらつきの小さい測定側へ候補が寄る。

**入力・出力**

入力：n、n行の移動指示uと測定位置y。出力：各ターンの位置平均とESS。移動分散は1、測定分散は0.04。

**実行可能なコード**

同梱ファイル：`examples/case_19.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 現在位置。
    using Particle = double;

    // 今回の移動指示と測定。
    struct Observation {
        double control, value;
    };

    // 分散を受け取る密度計算。
    static double log_normal(double x, double mean, double variance) {
        return -0.5 *
               ((x - mean) * (x - mean) / variance + log(2 * numbers::pi * variance));
    }

    // 移動前の初期分布。
    double sample(mt19937_64& rng) {
        return normal_distribution<double>(0, 2)(rng);
    }

    // この関数があればtransitionより優先され、動的モデルとして扱われる。
    double propose_transition(
        double& next,         // 提案する遷移後の位置を書き込む。
        const double& old,    // 遷移前の位置。変更しない。
        const Observation& o, // 今回の移動指示と測定。
        mt19937_64& rng       // 候補を生成する乱数器。
    ) {
        const double predicted = old + o.control;
        const double motion_var = 1;
        const double sensor_var = 0.04;

        // 2つの正規分布の積から得る分散。
        const double variance = 1 / (1 / motion_var + 1 / sensor_var);

        // 精度が高い方へ寄る平均。
        const double mean = variance * (predicted / motion_var + o.value / sensor_var);

        // 提案用の分布から候補を生成。
        next = normal_distribution<double>(mean, sqrt(variance))(rng);

        // 遷移／提案の補正だけ。
        return log_normal(next, predicted, motion_var) -
               log_normal(next, mean, variance);
    }

    // 今回の測定密度はここで1回だけ加える。
    double log_likelihood(double x, const Observation& o) const {
        return log_normal(o.value, x, 0.04);
    }
};

int main() {
    int n;
    cin >> n;

    // 観測誘導提案も通常のobserveで使う。
    ParticleFilter<Model> filter(Model{}, {.count = 2048});
    cout << fixed << setprecision(6);

    for (int i = 0; i < n; ++i) {
        // 制御と測定を保持。
        Model::Observation o;
        cin >> o.control >> o.value;

        // 提案補正と測定更新をライブラリが合成する。
        if (!filter.observe(o).ok) {
            return 1;
        }

        // 推定位置と重みの実効粒子数。
        cout << filter.estimate().mean << ' ' << filter.ess() << '\n';
    }
}
```

**入力例**

```text
3
1 1.1
1 2.0
1 3.2
```

**実行結果**

```text
1.090619 1226.643771
2.000174 1225.403811
3.196105 1223.347316
```

### 3.20 例20：仮の未来へ分岐し、途中で計算量を調整する

**具体的に解く問題**

装置を測定したあと、次回成功した場合と失敗した場合の推定をそれぞれ試す。本線を壊さず、計算余裕に応じて仮説数や再探索回数も変更したい。

**モデルへの対応**

フィルターのコピーには粒子・重み・履歴・乱数状態が含まれる。分岐へ仮観測を加えても本線は変わらない。paramをコピーしてset_paramへ渡すと、履歴を保って計算量を変えられる。resetは観測を全部忘れて再生成する。

**入力・出力**

入力：n、n個の成功0/1。出力：本線・成功分岐・失敗分岐の推定値、調整後の粒子数・履歴数、reset後の観測数。

**実行可能なコード**

同梱ファイル：`examples/case_20.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Coin {
    // 粒子1個が、装置の成功率pを表す。
    using Particle = double;

    // 1回の観測は成功true／失敗false。
    using Observation = bool;

    // 初期仮説を0以上1未満の一様分布から作る。
    double sample(mt19937_64& rng) {
        return generate_canonical<double, 53>(rng);
    }

    // 仮説pの下で今回の結果が出る確率。
    double likelihood(double p, bool success) const {
        // 「p自体が正しい確率」ではない。
        return success ? p : 1 - p;
    }
};

int main() {
    int n;
    cin >> n;

    // この例で使用するフィルター型。
    using Filter = ParticleFilter<Coin>;

    // 計算量や再探索の頻度を項目ごとに設定する。
    Filter::Param options{
        .count = 2048, // 保持する粒子の数。
        .ess_ratio = 0.5, // ESSが粒子数の半分未満なら再サンプリング。
        .move_steps = 2, // 再探索1回につき、各有効粒子へ2回提案する。
        .move_interval = 8, // 重みが偏らなくても8観測ごとに再探索する。
        .tempering_ess_ratio = 0.5, // 段階更新で目標とするESSの比率。
    };

    // モデル・設定・初期RNGを渡す。
    Filter filter(Coin{}, options, mt19937_64(0));

    for (int i = 0; i < n; ++i) {
        int success;
        cin >> success;

        // 本線へ実際の結果だけを追加。
        if (!filter.observe(success != 0).ok) {
            return 1;
        }
    }

    // コピー構築。将来の成功を試す分岐。
    auto if_success = filter;

    // コピー構築。将来の失敗を試す分岐。
    auto if_failure = filter;

    // それぞれへ仮観測。
    if (!if_success.observe(true).ok || !if_failure.observe(false).ok) {
        return 1;
    }

    // 本線の推定。
    cout << fixed << setprecision(6) << filter.estimate().mean << ' ';

    // 2つの将来の推定。
    cout << if_success.estimate().mean << ' ' << if_failure.estimate().mean << '\n';

    // コピー代入で元へ戻す。代入先の作業領域は再利用される。
    if_failure = filter;

    // ムーブ構築。以後if_successは使用しない。
    auto selected = std::move(if_success);

    // ムーブ代入。以後selectedは使用しない。
    if_failure = std::move(selected);

    // 設定のコピー。param()自体はconst参照を返す。
    auto adjusted = filter.param();
    adjusted.count = 4096;
    adjusted.move_steps = 3;
    adjusted.move_interval = 4;

    // 粒子数変更は即時再サンプリング。重みが等しくなる点に注意。
    filter.set_param(adjusted);

    // 観測を増やさず、全履歴を使って追加再探索する。
    const auto moved = filter.refine(2);

    // 試行数・受理数を使いたければcout/cerrへそのまま出せる。
    (void)moved;

    // bool観測のhistoryは読み取り専用range。
    cout << filter.size() << ' ' << filter.history().size() << '\n';

    // 全履歴を忘れる。RNGはseedへ戻らず、その続きを使う。
    filter.reset();

    // reset後は0。完全再現したい場合は新規構築する。
    cout << filter.observation_count() << '\n';
}
```

**入力例**

```text
4
1 1 0 1
```

**実行結果**

```text
0.667171 0.714012 0.573275
4096 4
0
```

### 3.21 例21：観測に矛盾があるとき、失敗を検出して状態を保つ

**具体的に解く問題**

隠れた整数0〜9について、「値はこの区間内」という確実な情報が届く。後で矛盾する区間が届く場合もある。

**モデルへの対応**

区間内の仮説は尤度1、区間外は0。全粒子が不適合ならobserveはok=falseを返し、粒子・重み・履歴・成功回数を保つ。外れた観測を勝手に弱めて取り込む処理はしない。

**入力・出力**

入力：n、n行の閉区間lower upper。出力：各更新の成功可否・成功回数・平均、最後に先頭3粒子の値と重みと対数重み。

**実行可能なコード**

同梱ファイル：`examples/case_21.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

struct Model {
    // 隠された整数。
    using Particle = int;

    // 確実に含まれると報告された閉区間。
    struct Observation {
        int lower, upper;
    };

    // 初期仮説。
    int sample(mt19937_64& rng) {
        return uniform_int_distribution<int>(0, 9)(rng);
    }

    // 確実な条件は0/1の確率で表す。
    double likelihood(int p, const Observation& o) const {
        return o.lower <= p && p <= o.upper ? 1 : 0;
    }
};

int main() {
    int n;
    cin >> n;

    // 各整数を十分な粒子で表現する。
    ParticleFilter<Model> filter(Model{}, {.count = 2048});
    cout << fixed << setprecision(6);

    for (int i = 0; i < n; ++i) {
        // 誤っていたら観測モデルや入力を確認する。
        Model::Observation o;
        cin >> o.lower >> o.upper;

        // 失敗なら以前の推定を保持する。
        const auto result = filter.observe(o);

        // 成功・成功回数・現在の平均を表示。
        cout << result.ok << ' ' << filter.observation_count() << ' '
             << filter.estimate().mean << '\n';

        // ok=falseでもRNG消費や外部副作用は巻き戻らない。安易な再試行はしない。
    }

    // 読み取り専用view。以下ではfilterを変更しない。
    const auto particles = filter.particles();

    // 合計1の通常重み。同じ添字が同じ粒子を指す。
    const auto weights = filter.weights();

    // 正規化後の対数重み。重み0は-infになり得る。
    const auto logs = filter.log_weights();

    // 先頭3粒子だけを表示。
    for (int i = 0; i < min(3, filter.size()); ++i) {
        cout << particles[i] << ' ' << weights[i] << ' ' << logs[i] << '\n';
    }
}
```

**入力例**

```text
3
2 7
4 6
8 9
```

**実行結果**

```text
1 1 4.531275
1 2 4.959961
0 2 4.959961
5 0.000488 -7.624619
5 0.000488 -7.624619
6 0.000488 -7.624619
```

### 3.22 例22：長い観測履歴をまとめて評価する

**具体的に解く問題**

成功・失敗が大量に蓄積した装置を、何度も再探索する。各候補で過去の成功確率を1回ずつ対数計算すると無駄が多い。

**モデルへの対応**

成功回数S、失敗回数Fを数えれば、履歴の対数尤度はS×log(p)+F×log(1−p)。historyの走査は残るが、対数計算は2回に減る。今回の観測用のlikelihoodも必要で、履歴用関数だけでは使えない。

**入力・出力**

入力：n、n個の成功0/1。出力：推定成功率と、保存された履歴の成功回数。pの事前範囲は0.01〜0.99。

**実行可能なコード**

同梱ファイル：`examples/case_22.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

// pの両端を除いた一様事前分布を仮定する。
struct Model : UniformBox<1> {
    // bool履歴はvector<bool>由来の読み取り専用range。
    using Observation = bool;

    // log(0)を避けるためではなく、この事前範囲をモデルとする。
    Model() : UniformBox<1>({0.01}, {0.99}) {}

    // 今回1回の対数尤度。
    double log_likelihood(const Particle& p, bool success) const {
        return success ? log(p[0]) : log1p(-p[0]);
    }

    // boolと通常の履歴に対応。
    double log_likelihood_history(const Particle& p, const auto& observations) const {
        int successes = 0, failures = 0;

        for (bool success : observations) {
            success ? ++successes : ++failures;
        }

        // 1回用の式の総和と、定数も含めて一致させる。
        return successes * log(p[0]) + failures * log1p(-p[0]);
    }
};

int main() {
    int n;
    cin >> n;

    // 再探索時に履歴用関数が自動で使われる。
    ParticleFilter<Model> filter(Model{}, {.count = 2048});

    for (int i = 0; i < n; ++i) {
        int success;
        cin >> success;

        // 履歴への追加はライブラリが行う。
        if (!filter.observe(success != 0).ok) {
            return 1;
        }
    }

    // 全履歴を使った再探索。履歴用関数の用途。
    const auto moved = filter.refine(3);
    (void)moved;

    int successes = 0;

    // 読み取りのみ。取得中にfilterを変更しない。
    for (bool success : filter.history()) {
        successes += success;
    }

    // 推定と履歴確認。
    cout << fixed << setprecision(6) << filter.estimate().mean[0] << ' ' << successes
         << '\n';
}
```

**入力例**

```text
10
1 1 0 1 0 1 1 1 0 1
```

**実行結果**

```text
0.666227 7
```

### 3.23 例23：少数の仮説を全列挙し、厳密に重みを更新する

**具体的に解く問題**

成功率が0.2・0.5・0.8のどれかだと分かっている装置を調べる。3候補の事前確率は0.2・0.5・0.3。

**モデルへの対応**

ParticleCloudへ3仮説を直接配置し、事前確率の対数をassignへ渡す。今回は候補を増減させず全列挙するので、丸め誤差を除けば離散的なベイズ更新そのものを計算できる。

**入力・出力**

入力：n、n個の成功0/1。出力：仮説・重み・対数重み、全平均、上位2平均、[平均,二乗平均]、成功率0.7以上の確率。

**実行可能なコード**

同梱ファイル：`examples/case_23.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

int main() {
    int n;
    cin >> n;

    // 直接制御用。構築直後は空であり、先にassign/initializeする。
    ParticleCloud<double> cloud;
    vector<double> hypotheses{0.2, 0.5, 0.8};
    array<double, 3> log_prior{log(0.2), log(0.5), log(0.3)};

    // 対数重みを正規化して設定する。
    if (!cloud.assign(hypotheses, log_prior)) {
        return 1;
    }

    cout << fixed << setprecision(6);

    for (int i = 0; i < n; ++i) {
        int success;
        cin >> success;

        // 今回の増分のみ。
        const auto result = cloud.update_log([&](double p) {
            return log(success ? p : 1 - p);
        });

        // 低水準版は履歴を保存せず、自動再サンプリングもしない。
        if (!result.ok) {
            return 1;
        }

        // 対数正規化定数も直接表示できる。
        if (i == n - 1) {
            cout << result << '\n';
        }
    }

    // 全候補を添字対応で確認する。
    for (int i = 0; i < cloud.size(); ++i) {
        // 同じ添字の仮説と重み。
        cout << cloud.particles()[i] << ' ' << cloud.weights()[i] << ' '
             << cloud.log_weights()[i] << '\n';
    }

    // Cloudでは射影を明示する。
    const auto all = cloud.estimate([](double p) {
        return p;
    });
    cout << all << '\n';

    // 3候補中、上位2つの平均。
    const auto top = cloud.estimate(
        [](double p) {
            return p;
        },
        2
    );
    cout << top << '\n';

    array<double, 2> buffer{};

    // 長さ2のdouble出力先を再利用。
    cloud.estimate_into(buffer, [](double p) {
        return array{p, p * p};
    });

    double high = 0;

    // 任意条件の確率を集計。
    cloud.for_each_weighted([&](double p, double w) {
        if (p >= 0.7) {
            high += w;
        }
    });

    cout << buffer[0] << ' ' << buffer[1] << ' ' << high << '\n';
}
```

**入力例**

```text
3
1 1 0
```

**実行結果**

```text
UpdateResult{ok=true, log_normalizer=-1.108197}
0.200000 0.059646 -2.819331
0.500000 0.582479 -0.540462
0.800000 0.357875 -1.027571
Estimate{mean=0.589469, selected_mass=1.000000}
Estimate{mean=0.614172, selected_mass=0.940354}
0.589469 0.377046 0.357875
```

### 3.24 例24：追跡のタイミングと粒子数を直接制御する

**具体的に解く問題**

1次元の対象を追跡し、測定後の重みを確認してから必要時だけ再サンプリングする。別の小さい粒子集団で、1ターン先を仮予測したい。

**モデルへの対応**

低水準版ではpredictとupdate_logを自分で組み合わせる。再サンプリングの自動判定も、resample_if_neededを呼んだ時点で行われる。resample_fromは元の集団を変更せず、別集団を作れる。

**入力・出力**

入力：n、n個の測定位置。出力：各ターンの更新後平均・ESS・再サンプリング有無、最後に別集団の次時点予測。nは1以上。

**実行可能なコード**

同梱ファイル：`examples/case_24.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

int main() {
    int n;
    cin >> n;
    mt19937_64 rng(0);

    // ESSが粒子数の60%未満なら、明示的な判定呼び出しで再サンプリング。
    ParticleCloud<double> cloud(0.6);

    // 初期分布から生成。
    cloud.initialize(
        512,
        [](auto& r) {
            return normal_distribution<double>(0, 2)(r);
        },
        rng
    );

    cout << fixed << setprecision(6);

    for (int t = 0; t < n; ++t) {
        double y;
        cin >> y;

        // 真の移動。
        cloud.predict(
            [](double& x, auto& r) {
                x += 1 + normal_distribution<double>(0, 0.3)(r);
            },
            rng
        );

        // 移動後の観測で重みを更新する。
        const auto update = cloud.update_log([&](double x) {
            double z = (y - x) / 0.5;
            return -0.5 * z * z - log(0.5 * sqrt(2 * numbers::pi));
        });

        // この方式では既に行ったpredictは戻らない。必要なら例25を使う。
        if (!update.ok) {
            return 1;
        }

        // 更新後の値。
        const auto estimate = cloud.estimate([](double x) {
            return x;
        });
        cout << estimate.mean << ' ' << cloud.ess() << ' ';

        // 実施ならtrue。未実施ならRNGも消費しない。
        cout << cloud.resample_if_needed(rng) << '\n';
    }

    // 計算余裕ができたとして粒子数を1024へ変更。
    const int parents = cloud.resample(rng, 1024);

    // 元集団とは別の予測用集団を作る。
    ParticleCloud<double> future;

    // sourceを保ち、128個を等重みで抽出する。
    future.resample_from(cloud, rng, 128);

    // 仮の1ターン。
    future.predict(
        [](double& x, auto& r) {
            x += 1 + normal_distribution<double>(0, 0.3)(r);
        },
        rng
    );

    // 読み取り用の復元抽出。元重みを再適用しない。
    array<int, 8> selected{};
    future.sample_indices(selected, rng);

    double sample_mean = 0;

    // 抽出後は等重み。
    for (int i : selected) {
        sample_mean += future.particles()[i] / selected.size();
    }

    // コピー構築。推定分布を保存する。
    auto saved = future;

    // コピー代入。粒子と重みを複製する。
    saved = cloud;

    // ムーブ構築。以後savedを読まない。
    auto transferred = std::move(saved);

    // ムーブ代入。以後transferredを読まない。
    saved = std::move(transferred);

    // 仮予測用の集団について、全粒子の平均を求める。
    const auto predicted = future.estimate([](double x) {
        return x;
    });

    // 粒子数・親数・仮予測の全平均・標本平均。
    cout << cloud.size() << ' ' << parents << ' ' << predicted.mean << ' '
         << sample_mean << '\n';

    // parentsは異なる親「添字」の数。同じ値の粒子をまとめた種類数ではない。
}
```

**入力例**

```text
3
1.2 1.8 3.1
```

**実行結果**

```text
1.152625 166.879203 1
1.958251 387.290179 0
3.014949 322.721715 0
1024 400 3.984680 3.689787
```

### 3.25 例25：状態遷移と重み更新をまとめて確定する

**具体的に解く問題**

整数位置の対象が、各ターン確率半々で1歩進むか停止する。測定は正確80%、±1ずれる確率が各10%。全仮説と合わない観測が来たら、位置の更新自体も取り消したい。

**モデルへの対応**

propose_update_logは旧粒子から作った候補に移動を適用し、返された増分対数重みで一括判定する。全候補の尤度が0なら粒子と重みを保つ。本来の移動と同じ分布で生成するので、この例の補正は観測尤度の対数だけ。

**入力・出力**

入力：n、n個の整数測定位置。出力：各更新の成功可否と位置平均。例の最後は説明不能な観測で、直前の推定を保つ。

**実行可能なコード**

同梱ファイル：`examples/case_25.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

int main() {
    int n;
    cin >> n;
    mt19937_64 rng(0);

    // 構造体ではなく整数粒子を直接使える。
    ParticleCloud<int> cloud;

    // 初期位置は全粒子0。重み省略は等重み。
    cloud.assign(vector<int>(1024, 0));
    cout << fixed << setprecision(6);

    for (int t = 0; t < n; ++t) {
        int y;
        cin >> y;

        // 返す値は増分対数重み。一般の別分布提案なら遷移／提案の補正も加える。
        // 候補上で遷移と測定更新を組み合わせる。
        const auto result = cloud.propose_update_log(
            [&](const int& old, int& candidate, auto& r) {
                // candidateはoldのコピー。旧状態を変更せず候補だけを編集する。
                candidate = old + int(bernoulli_distribution(0.5)(r));
                int error = abs(y - candidate);

                if (error == 0) {
                    return log(0.8);
                }

                if (error == 1) {
                    return log(0.1);
                }

                return -numeric_limits<double>::infinity();
            },
            rng
        );

        // 失敗時は旧平均。
        const auto estimate = cloud.estimate([](int x) {
            return x;
        });
        cout << result.ok << ' ' << estimate.mean << '\n';

        // RNG消費は失敗時にも残る。コールバックの外部副作用は巻き戻されない。
    }
}
```

**入力例**

```text
3
1 2 20
```

**実行結果**

```text
1 0.885764
1 1.889762
0 1.889762
```

### 3.26 例26：履歴を自分で管理してコピー方式の再探索を行う

**具体的に解く問題**

独自のシミュレータに粒子集団だけを組み込み、成功率の推定と追加再探索を自分で管理したい。

**モデルへの対応**

update_logには今回の観測だけを渡すが、rejuvenateの対象密度には事前分布と全観測を含める。この例の一様事前分布では、成功S回・失敗F回の対数密度はS log(p)+F log(1−p)。候補の採否はライブラリが判断する。

**入力・出力**

入力：n、n個の成功0/1。出力：再探索後の平均と試行数・受理数。

**実行可能なコード**

同梱ファイル：`examples/case_26.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

int main() {
    int n;
    cin >> n;
    mt19937_64 rng(0);
    int successes = 0, failures = 0;

    // 低水準の粒子集団。以下で初期化してから利用する。
    ParticleCloud<double> cloud;

    // 一様事前。
    cloud.initialize(
        4096,
        [](auto& r) {
            return generate_canonical<double, 53>(r);
        },
        rng
    );

    for (int t = 0; t < n; ++t) {
        int bit;
        cin >> bit;

        // 増分。
        const auto update = cloud.update_log([&](double p) {
            return bit ? log(p) : log1p(-p);
        });

        // 全仮説が無効なら、履歴集計も進めず終了する。
        if (!update.ok) {
            return 1;
        }

        // 成功した観測だけを自分の履歴集計へ反映する。
        bit ? ++successes : ++failures;
    }

    // 重みを等しくしてから再探索。これは利用側が明示的に選ぶタイミング。
    cloud.resample(rng);

    // 全履歴と一様事前分布を含む対数対象密度。
    auto target = [&](double p) {
        if (p < 0 || p > 1) {
            return -numeric_limits<double>::infinity();
        }

        return (successes ? successes * log(p) : 0) +
               (failures ? failures * log1p(-p) : 0);
    };

    // 各粒子へ4回のMH提案。
    const auto moved = cloud.rejuvenate(
        4,
        target,
        [](double& p, auto& r) {
            // 対称提案の補正0。範囲外はtargetで棄却。
            p += normal_distribution<double>(0, 0.1)(r);
            return 0.0;
        },
        rng
    );

    // 事後平均。
    const auto estimate = cloud.estimate([](double p) {
        return p;
    });
    cout << fixed << setprecision(6) << estimate.mean << '\n';

    // attemptedとaccepted。重みそのものはrejuvenateで変わらない。
    cout << moved << '\n';
}
```

**入力例**

```text
5
1 1 0 1 1
```

**実行結果**

```text
0.719287
MoveResult{attempted=16384, accepted=13339}
```

### 3.27 例27：2台の装置を、差分変更と巻き戻しで再探索する

**具体的に解く問題**

2台の装置の成功率をまとめて推定し、独自管理の粒子の1成分だけを変更したい。

**モデルへの対応**

低水準のrejuvenateでも、変更した成分の番号と古い値を返せば差分方式にできる。targetは2台の全観測を含み、undoは棄却された成分を正確に戻す。

**入力・出力**

入力：n、n行の装置番号0/1と成功0/1。出力：[装置0,装置1]の成功率平均、再探索の試行数・受理数。

**実行可能なコード**

同梱ファイル：`examples/case_27.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

int main() {
    int n;
    cin >> n;

    // 粒子が持つ2台の成功率。
    using Particle = array<double, 2>;

    // 戻す必要がある成分だけを記録。
    struct Undo {
        int index;
        double value;
    };

    array<int, 2> successes{}, failures{};
    mt19937_64 rng(0);

    // 配列を粒子とする低水準の集団。
    ParticleCloud<Particle> cloud;

    // 各装置に独立な一様事前分布。
    cloud.initialize(
        4096,
        [](auto& r) {
            return Particle{
                generate_canonical<double, 53>(r),
                generate_canonical<double, 53>(r)
            };
        },
        rng
    );

    for (int t = 0; t < n; ++t) {
        int machine, bit;
        cin >> machine >> bit;

        // 今回観測した装置の尤度だけを加える。
        const auto result = cloud.update_log([&](const Particle& p) {
            return bit ? log(p[machine]) : log1p(-p[machine]);
        });

        // 不可能な観測なら履歴側も増やさない。
        if (!result.ok) {
            return 1;
        }

        bit ? ++successes[machine] : ++failures[machine];
    }

    // ここでは全観測を入れてから一度だけ再サンプリングする。
    cloud.resample(rng);

    // 2台の全履歴の対数密度。
    auto target = [&](const Particle& p) {
        double value = 0;

        for (int i = 0; i < 2; ++i) {
            if (p[i] < 0 || p[i] > 1) {
                return -numeric_limits<double>::infinity();
            }

            value += (successes[i] ? successes[i] * log(p[i]) : 0) +
                     (failures[i] ? failures[i] * log1p(-p[i]) : 0);
        }

        return value;
    };

    // 差分提案版のオーバーロード。
    const auto moved = cloud.rejuvenate(
        4,
        target,
        [](Particle& p, auto& r) {
            int i = uniform_int_distribution<int>(0, 1)(r);
            Undo old{i, p[i]};
            p[i] += normal_distribution<double>(0, 0.15)(r);

            // 旧値と対称提案の補正を返す。
            return ParticleCloud<Particle>::MoveProposal<Undo>{old, 0};
        },

        // 棄却時だけundoを呼ぶ。
        [](Particle& p, const Undo& old) {
            p[old.index] = old.value;
        },
        rng
    );

    // 2成分平均。
    const auto estimate = cloud.estimate([](const Particle& p) {
        return p;
    });
    cout << fixed << setprecision(6) << estimate << '\n';

    // 受理された回数を表示する。
    cout << moved << '\n';
}
```

**入力例**

```text
6
0 1
1 0
0 1
1 1
0 0
1 0
```

**実行結果**

```text
Estimate{mean=[0.600654, 0.397810], selected_mass=1.000000}
MoveResult{attempted=16384, accepted=12960}
```

### 3.28 例28：既知の事後分布から直接引き直すカーネルを使う

**具体的に解く問題**

1分ごとの来客数から、1分当たりの平均来客数λを推定する。数学的に事後分布が求まる場合に、低水準版へその引き直し方を渡したい。

**モデルへの対応**

来客数kの確率はexp(−λ)×λのk乗÷k!。λの事前分布を平均1の指数分布にすると、n分間の合計S人を観測した事後分布は形状1+S・尺度1/(1+n)のガンマ分布になる。観測確率を掛けるとλのべき乗の指数は合計人数Sとなり、指数関数の部分はexp(−nλ)となる。事前のexp(−λ)も掛けた、λのS乗×exp(−(n+1)λ)に比例する山型が、このガンマ分布である。標準ライブラリのgamma_distributionで直接生成できる。形状は山の形、尺度は横方向の広がりを指定する数で、ここでは1+Sと1/(1+n)にする。この特別な場合は粒子法なしでも平均(1+S)/(1+n)を計算でき、比較基準にもなる。

**入力・出力**

入力：n、n個の非負整数の来客数。出力：粒子平均と解析的な平均。カーネルのrejuvenateは結果構造体を返さない。

**実行可能なコード**

同梱ファイル：`examples/case_28.cpp`。

```cpp
// 粒子フィルター本体。C++20でコンパイルする。
#include "particle_filter_v12.hpp"

using namespace std;

int main() {
    int n;
    cin >> n;
    mt19937_64 rng(0);
    int total = 0;

    // 低水準の粒子集団。以下で初期化してから利用する。
    ParticleCloud<double> cloud;

    // 平均1の指数事前分布。
    cloud.initialize(
        8192,
        [](auto& r) {
            return exponential_distribution<double>(1)(r);
        },
        rng
    );

    for (int t = 0; t < n; ++t) {
        int count;
        cin >> count;

        // 来客数のPoisson確率の対数。
        const auto result = cloud.update_log([&](double rate) {
            // 0×log(0)を作らない。
            return (count ? count * log(rate) : 0) - rate - lgamma(double(count) + 1);
        });

        // 更新失敗なら、後続の集計へ進めない。
        if (!result.ok) {
            return 1;
        }

        total += count;
    }

    // 等重みにしてから、事後分布を保つカーネルを適用する。
    cloud.resample(rng);

    // void戻り値。ユーザー側で対象分布を保つことを保証する必要がある。
    // target・MH採否・補正なしのカーネル版。
    cloud.rejuvenate(
        1,
        [&](double& rate, auto& r) {
            // 全観測の事後分布から直接生成。
            rate = gamma_distribution<double>(1 + total, 1.0 / (1 + n))(r);
        },
        rng
    );

    // 推定した平均来客数。
    const auto estimate = cloud.estimate([](double rate) {
        return rate;
    });
    cout << fixed << setprecision(6) << estimate.mean << ' ';

    // この問題での厳密な事後平均との比較。
    cout << double(1 + total) / (1 + n) << '\n';
}
```

**入力例**

```text
5
3 2 4 1 3
```

**実行結果**

```text
2.335538 2.333333
```


### 3.29 APIの網羅範囲と検証

| API・パターン | 掲載例 |
| --- | --- |
| ParticleFilterの構築・Particle/Observation型・Param | 1、2、3、16、20 |
| 通常尤度／対数尤度 | 1、2、3、4、6、7、8、17 |
| sample・prior／log_prior・対称／非対称propose | 11、12、14 |
| propose_in_place・undo・MoveProposal | 13、27 |
| project・射影なし／射影指定estimate・top_k | 1、3、9、10、15 |
| 両方のestimate_into・出力バッファ・span | 5 |
| for_each_weighted・sample_indices・外部RNG | 16 |
| transition・欠測・動的モデル | 18 |
| propose_transition・遷移提案の補正 | 19 |
| param・set_param・refine・reset・コピー／ムーブ | 12、20 |
| size・ess・observation_count・history・log_likelihood_history | 18、19、20、22 |
| particles・weights・log_weights・更新失敗 | 16、21 |
| 結果4型のoperator<<・各結果フィールド | 1、3、4、12、23、26、27 |
| UniformBox固定次元・実行時次元・継承したsample/log_prior/propose | 2、3、4、5、6、7、8、15、17、22 |
| ParticleCloud構築・assignの両形式・initialize | 23、24、25 |
| Cloudのupdate_log・predict・propose_update_log | 23、24、25 |
| Cloudのresample_if_needed・resample・resample_from | 24 |
| Cloudのコピー／ムーブ・sample_indices・読み取り | 23、24 |
| Cloudのestimate・estimate_into・for_each_weighted | 23 |
| Cloudのrejuvenate：コピー／差分／カーネル | 26、27、28 |

全例について、C++20で個別にコンパイル・実行し、警告の有無と数値的な整合を検査する。離散仮説は直接ベイズ計算、成功率はBeta分布の平均、構成比はDirichlet分布の平均、動的追跡は線形正規モデルの解析解、来客数はガンマ分布の平均と比較する。粒子近似が入る箇所では、計算上の丸めより大きい統計的な許容幅を使う。

例21と25では、失敗した観測で推定値が変わらないことを確認する。例2は通常尤度と対数尤度による近似推定値の整合、例5は二乗平均と平均の関係、例16は行動選択と抽出後の集計を検査する。

付属の`verify_examples.py`で再検証できる。実行コマンドは同梱READMEに記載する。原稿中のコードと個別ファイルの一致も検査する。検証スクリプトはこのマークダウンを自動更新しない。
