# rectangle_packing サンプルコード集

対象ヘッダーは`rectangle_packing_v11.hpp`。整数座標の矩形配置を、C++20でそのまま試せる例としてまとめた。本文とコードは、このヘッダーの公開APIに対応している。

1章で解く問題、2章で全公開API、3章で21個の独立した入出力付きプログラムを扱う。ライブラリはグローバル名前空間の自由関数を呼ぶ方式であり、solverオブジェクトや`solve()`の準備は不要である。

## 1. 何を解くライブラリか？

### 1.1 長方形を重ねずに並べる

幅と高さが決まった複数の長方形を、重ならないように配置する。画像を1枚にまとめる、部品を板へ割り当てる、ページへ情報を詰める、といった用途がある。「ビン」は長方形を置く先の領域を指し、このライブラリでは2次元である。

座標は左上原点で、右がxの正方向、下がyの正方向。寸法は正の整数、配置座標は0以上の整数である。矩形ごとに90度回転を許すかを指定できる。同じビンの矩形は内部が重なってはならないが、辺や角が接する配置は許される。

解く問題は次の4形式に分かれる。

| 形式 | 利用者が決めるもの | 求めたいもの |
|---|---|---|
| 固定ビン | 1個のビンの幅と高さ、必須・任意、各矩形の利益 | 必須を優先し、価値の高い矩形を置く配置 |
| 幅固定strip | 幅、全矩形を置くこと | 必要な高さが小さい配置 |
| 同一サイズの複数ビン | 各ビンの幅と高さ、全矩形を置くこと | 使用ビン数が少ない配置 |
| 自由な外接矩形 | 全矩形を置くこと、幅と高さの評価方法 | 面積・周長・重み付き寸法が小さい外枠 |

「必須」は必ず成功するという保証ではない。必須の未配置を優先して減らす指定である。返却された配置について、必須が全部置けたかを読み取る必要がある。

### 1.2 数式に登場する変数

入力配列の順に、矩形を0から$N-1$までの番号で識別する。以下の記号は説明のための数式であり、C++の変数名とは区別する。

| 記号 | 意味 |
|---|---|
| $N$ | 入力矩形の個数 |
| $i,j$ | 入力矩形の番号。0以上$N-1$以下 |
| $w_ {i},h_ {i}$ | 矩形$i$の入力時の幅と高さ |
| $p_ {i}$ | 矩形$i$を配置したときに得る整数の利益。負も指定できる |
| $q_ {i}$ | 必須なら1、任意なら0 |
| $z_ {i}$ | 結果で配置されたら1、未配置なら0 |
| $a_ {i},b_ {i}$ | 配置時の幅と高さ。通常は$w_ {i},h_ {i}$、回転したら$h_ {i},w_ {i}$ |
| $x_ {i},y_ {i}$ | 配置済み矩形$i$の左上座標 |
| $k_ {i}$ | 配置済み矩形$i$のビン番号。0から数える |
| $W,H$ | 利用者が指定するビン幅・高さ。stripでは$W$だけを指定する |
| $U,V$ | 配置済み矩形の最大右端と最大下端。結果の使用幅・使用高さ |
| $M$ | 未配置の必須矩形の個数 |
| $P$ | 配置済み矩形の利益合計 |
| $A$ | 配置済み矩形の面積合計 |
| $B$ | 配置済みの最大ビン番号に1を加えた値 |
| $T$ | ビン番号$B-1$の末尾ビンに置かれた矩形の面積合計 |
| $\alpha,\beta$ | 自由外接矩形で幅と高さに掛ける正の整数の重み |
| $f(U,V)$ | 自由外接矩形を評価する値。面積、半周長、重み付き和のいずれか |

集計値は次の意味を持つ。

$$M=\sum_ {i=0}^{N-1}q_ {i}(1-z_ {i}),\quad P=\sum_ {i=0}^{N-1}p_ {i}z_ {i},\quad A=\sum_ {i=0}^{N-1}w_ {i}h_ {i}z_ {i}.$$

$U$は配置済み矩形の$x_ {i}+a_ {i}$の最大値、$V$は$y_ {i}+b_ {i}$の最大値である。配置が1つもなければ$U,V,B,T$は0とする。

複数ビンの座標も、そのビン内の座標である。したがって$UV$は全ビンを合算した面積ではない。また、利用者が作った配置のビン番号に欠番があると、$B$には空番号の分も含まれる。実際の使用個数として扱いたい場合は、ビン番号を0始まりの連番へそろえる。

### 1.3 固定ビンの目的

配置済み矩形は$W\times H$の領域内に収める。結果を次の順に比較する。

$$\min_ {\mathrm{lex}}(M,-P,-A,UV,V,U).$$

$\min_ {\mathrm{lex}}$は「左から順に比較し、最初に異なる値が小さい方を選ぶ」という辞書式の比較を表す。これは重みを掛けて足し合わせる意味ではない。

1. 必須の未配置数$M$を減らす。
2. $M$が同じなら、利益$P$を増やす。
3. $M,P$が同じなら、配置面積$A$を増やす。
4. そこまで同じなら、外接面積$UV$、使用高さ$V$、使用幅$U$を順に小さくする。

たとえば必須欠落が1から0へ減る配置は、利益が下がっても優先される。全矩形を必須にすれば「全部を入れたい」という用途になる。任意矩形の利益だけを数えたいなら、必須矩形の利益を0に設定する。

負の利益を持つ任意矩形は、新しく置く候補から除かれる。ただし、利用者が固定した既存配置に含まれるものまで自動で消すわけではない。利益0の任意矩形は、面積の比較によって配置される場合がある。

### 1.4 Stripの目的

幅$W$を固定し、高さ方向には必要に応じて伸ばす。全入力を必須にする。

$$\min_ {\mathrm{lex}}(M,V,U).$$

まず必須欠落を減らし、全部置けた結果同士なら使用高さ$V$を減らす。高さが同じなら使用幅$U$で比較する。利益を最大化したり、任意矩形を選別したりする形式ではない。

### 1.5 複数ビンの目的

同じ$W\times H$のビンを必要なだけ使い、全入力を必須にする。

$$\min_ {\mathrm{lex}}(M,B,T).$$

まず必須欠落を減らし、同じ欠落数なら使用ビン数を減らす。さらに同じなら、末尾ビンに置かれた面積を減らす。各ビンの密度を均等にする目的ではない。

最大ビン数の制約を直接渡す引数はない。たとえば「3枚以内」を要求するなら、返った結果が全配置かつ`bin_count <= 3`かを利用者が調べる。

### 1.6 自由外接矩形の目的

幅・高さを先に固定せず、全入力を必須にして次の値を比較する。

$$\min_ {\mathrm{lex}}(M,f(U,V),UV,V,U).$$

| 選ぶ目的 | $f(U,V)$ | 意味 |
|---|---|---|
| 面積 | $UV$ | 外枠の面積を小さくする |
| 周長 | $U+V$ | 半周長を小さくする。実際の周長$2(U+V)$と最小化する配置は同じ |
| 重み付き和 | $\alpha U+\beta V$ | 横と縦で異なる1単位当たりの費用を使う |

まず必須欠落、次に選んだ目的、その後は面積・高さ・幅で比較する。重みを大きくしても、幅や高さの絶対上限を保証することにはならない。幅上限が必須ならstripなど適した形式を使う。

### 1.7 結果が意味する範囲

返るのは発見した配置である。最適性や、全配置が不可能であることの証明は返さない。座標を得られたこと、必須を全部置けたこと、最適であることは区別する。

任意角度の回転、3次元、多角形そのもの、矩形ごとの許可座標、切断手順、前後関係、異なる大きさのビンを混在させる指定は直接扱わない。矩形障害物、共通の隙間、規格候補、小数寸法などは3章で入力への変換を示す。

## 2. APIリファレンス

### 2.1 導入と公開APIの全体

通常の利用には`rectangle_packing_v11.hpp`だけが必要である。C++20とGCCの標準ライブラリ環境を使い、他のライブラリへの依存はない。ヘッダー内部で`bits/stdc++.h`とGCCの128bit整数を使用する。

以下が公開される型・関数・メソッドの一覧である。暗黙のコピー・ムーブなど、C++が通常提供する操作は個別のライブラリAPIとして数えていない。

| 種別 | 名前 | 用途 |
|---|---|---|
| 構造体 | `rectangle_pack_item` | 入力矩形 |
| 構造体 | `rectangle_pack_placement` | 1矩形の配置 |
| 構造体 | `rectangle_pack_result` | 配置全体と集計 |
| 構造体 | `rectangle_pack_options` | 時間・回数・乱数の設定 |
| 構造体 | `rectangle_bounding_objective` | 外接矩形の評価方法 |
| 列挙型 | `rectangle_bounding_objective_type` | 面積・周長・重み付き和 |
| メソッド | `rectangle_pack_placement::placed()` | 配置済み判定 |
| メソッド | `rectangle_pack_result::feasible()` | 必須充足判定 |
| メソッド | `rectangle_bounding_objective::evaluate(width, height)` | 目的値の計算 |
| 演算子 | `operator<<(ostream, result)` | 結果のデバッグ表示 |
| 自由関数 | `pack_rectangles_fixed_bin` | 新規の固定ビン配置 |
| 自由関数 | `improve_rectangles_fixed_bin` | 初期解を保護した固定ビン再探索 |
| 自由関数 | `repack_rectangles_fixed_bin` | 指定IDだけ再配置して候補を返す |
| 自由関数 | `repair_rectangles_fixed_bin` | 初期解を保護した部分再配置 |
| 自由関数 | `pack_rectangles_strip` | 新規strip配置 |
| 自由関数 | `improve_rectangles_strip` | 初期解を保護したstrip再探索 |
| 自由関数 | `pack_rectangles_multiple_bins` | 新規の複数ビン配置 |
| 自由関数 | `improve_rectangles_multiple_bins` | 初期解を保護した複数ビン再探索 |
| 自由関数 | `pack_rectangles_bounding_box` | 新規の外接矩形配置 |
| 自由関数 | `improve_rectangles_bounding_box` | 初期解を保護した外接矩形再探索 |
| 自由関数 | `recompute_rectangle_packing_summary` | 外部で変更した配置の再集計 |
| 自由関数 | `validate_rectangle_packing` | 配置と集計の検査 |

ヘッダー内の非公開実装や、単体実行用の自己テスト型は利用者向けAPIではない。通常のincludeでは自己テストの`main`は除外される。

### 2.2 入力型：rectangle_pack_item

集成体初期化の順番は`{width, height, profit, rotatable, required}`。ユーザー定義のコンストラクタはなく、必要なフィールドを代入して用意できる。

| フィールド | 型 | 既定値 | 取り得る値・意味 |
|---|---|---|---|
| `width` | `long long` | `0` | 入力時の幅。利用時は正の整数へ設定する |
| `height` | `long long` | `0` | 入力時の高さ。利用時は正の整数へ設定する |
| `profit` | `long long` | `1` | 固定ビンでの利益。正・0・負を指定できる。候補ごとの配置済み合計も型の範囲内にする |
| `rotatable` | `bool` | `true` | trueなら幅と高さを交換する90度回転を許す。falseなら入力向きだけ |
| `required` | `bool` | `true` | trueなら必須、falseなら任意。任意の選別は固定ビンの形式で使う |

strip・複数ビン・外接矩形では、全入力を`required=true`にする。利益はそれらの目的では比較しないが、結果の`placed_profit`には集計される。

既定構築しただけの入力は幅・高さが0なので、そのままsolverに渡せない。未配置の矩形でも入力寸法は正である必要がある。

### 2.3 配置型：rectangle_pack_placement

集成体初期化の順番は`{x, y, width, height, bin, rotated}`。

| フィールド | 型 | 既定値 | 取り得る値・意味 |
|---|---|---|---|
| `x` | `long long` | `0` | 配置済みなら0以上の左上x座標 |
| `y` | `long long` | `0` | 配置済みなら0以上の左上y座標 |
| `width` | `long long` | `0` | 配置済みなら正の回転適用後の幅 |
| `height` | `long long` | `0` | 配置済みなら正の回転適用後の高さ |
| `bin` | `int` | `-1` | 0以上なら配置済み。負なら未配置。固定ビン・strip・外接矩形の配置済みは0 |
| `rotated` | `bool` | `false` | trueなら入力の幅と高さを交換した配置 |

`bool placed() const`は引数なし、既定引数なしで、`bin >= 0`を返す。計算量は`O(1)`。座標や寸法の妥当性は調べない。

未配置へ戻すときは`placement = {}`とすると各フィールドを既定値へ戻せる。未配置要素の古い座標・寸法は利用しない。

回転後の寸法と`rotated`の対応は、正方形でも必要である。正方形で幅と高さが同じだからといって、回転禁止の矩形に`rotated=true`を付けてよいわけではない。

### 2.4 結果型：rectangle_pack_result

配置・改善を行う10個の関数は、すべてこの型を値で返す。戻り値には入力と同じ長さ・順序の`placements`が入る。自分で既定構築した結果では、配列は空、集計は0である。

| フィールド | 型 | 既定値 | 意味 |
|---|---|---|---|
| `placements` | `std::vector<rectangle_pack_placement>` | 空 | 要素iが入力矩形iの配置。未配置も要素として存在する |
| `used_width` | `long long` | `0` | 配置済みの最大右端。1章の$U$ |
| `used_height` | `long long` | `0` | 配置済みの最大下端。1章の$V$ |
| `placed_profit` | `long long` | `0` | 必須・任意・固定配置を含む配置済み利益合計$P$ |
| `placed_area` | `long long` | `0` | 固定障害物も含む配置面積合計$A$ |
| `placed_count` | `int` | `0` | 配置済み矩形の個数 |
| `missing_required_count` | `int` | `0` | 未配置の必須個数$M$ |
| `bin_count` | `int` | `0` | 配置済み最大bin番号+1、すなわち$B$ |

`bool feasible() const`は引数なし、既定引数なしで、`missing_required_count == 0`だけを返す。計算量は`O(1)`。全矩形を必須にした場合は全配置の確認に使える。任意矩形が未配置でもtrueになり得る。

重なり・境界・回転の検査ではない。また、既定構築した空のresultもtrueになる。外部で配置を編集した直後は集計を再計算し、必要な幾何検査を行う。

### 2.5 外接目的と列挙型

`rectangle_bounding_objective_type`は`enum class`であり、利用する値は次の3つ。

| 列挙値 | evaluateの返す値 | 何を意味するか |
|---|---|---|
| `area` | 幅×高さ | 外接面積 |
| `perimeter` | 幅+高さ | 半周長。実際の周長の数値が必要なら2倍する |
| `weighted_sum` | 幅重み×幅+高さ重み×高さ | 幅と高さに別の正の費用を付けた和 |

列挙値の内部整数を直接入力値からキャストせず、この3値のいずれかを選ぶ。

`rectangle_bounding_objective`の集成体初期化の順番は`{type, width_weight, height_weight}`。

| フィールド | 型 | 既定値 | 意味・条件 |
|---|---|---|---|
| `type` | `rectangle_bounding_objective_type` | `area` | 上記3値から選ぶ |
| `width_weight` | `long long` | `1` | weighted_sumでの幅の重み。正の整数 |
| `height_weight` | `long long` | `1` | weighted_sumでの高さの重み。正の整数 |

areaとperimeterでは両重みを評価に使わない。weighted_sumでは両方を正にする。重みを0にして幅固定や高さ固定を表す指定にはしない。

`long long evaluate(long long width, long long height) const`は、幅・高さを受け取り、表の目的値を返す。両引数に既定値はない。配置の外接寸法として0以上を渡し、加算・乗算結果を`long long`に収める。空配置なら両方0を渡せる。計算量は`O(1)`で、配置の検査やoverflowの検出は行わない。

### 2.6 探索設定：rectangle_pack_options

各solver関数の`options`を省略すると次の既定値になる。必要なフィールドだけ変更できる。

| フィールド | 型 | 既定値 | 意味・値の選び方 |
|---|---|---|---|
| `deadline` | `std::chrono::steady_clock::time_point` | `time_point::max()` | 絶対的な終了時刻。既定値なら回数予算で停止する |
| `seed` | `std::uint64_t` | `0x243f6a8885a308d3ULL` | 0以上の64bit整数。回数だけで制御すると同じ入力・設定・seedで再現できる |
| `restart_limit` | `int` | `32` | 固定ビンの新規配置・improve・repair・repackの追加探索回数 |
| `search_iteration_limit` | `int` | `32` | strip・複数ビン・外接矩形の追加探索予算 |
| `time_check_interval` | `int` | `1` | 内部で何回停止判定を行うごとに時計を読むか |

通常は非負の回数予算、正の`time_check_interval`を指定する。負の回数予算は内部で0相当に扱われ、1未満の`time_check_interval`は1に丸められる。seed=0は使用可能だが、内部では非零の初期乱数状態へ置き換えられる。

回数予算0でも初期候補は作る。即座に空結果を返す設定ではない。増やした予算がそのまま構築回数と一致するとは限らず、予算を増やせば必ず解が良くなるという保証もない。

| 使い方 | 設定の出発点 |
|---|---|
| まず試す | 既定値32を使う |
| 何度も呼ぶ部分修復 | `restart_limit=8`などから、実入力で時間と品質を比較する |
| strip等の追加探索 | `search_iteration_limit=32`と64などを同じ入力群で比較する |
| 再現性重視 | deadlineを既定値のままにし、seedと予算を固定する |
| 総時間の管理 | 同じsteady_clockの絶対deadlineを共有し、出力用の余裕を取る |

`deadline`は1矩形ごとに強制中断する仕組みではない。期限を過ぎた指定でも初期候補を作ってから停止判定する箇所があり、`time_check_interval=1`でも時刻を超える場合がある。実時間で打ち切った場合は環境の負荷で結果が変わり得る。

### 2.7 自由関数の共通引数と入力前提

以下の宣言欄はAPIの引数順を示す参照用であり、実行用プログラムは3章に載せる。

| 引数名 | 型 | 値・意味・既定値 |
|---|---|---|
| `items` | `std::span<const rectangle_pack_item>` | 入力順の矩形配列。空も可。長さはintで表せる範囲。全関数で省略不可 |
| `initial_solution` | `const rectangle_pack_result&` | 同じ入力・ID順・現在の条件に対する妥当な初期配置。省略不可 |
| `movable_item_ids` | `std::span<const int>` | 0以上items.size()未満の重複しないID。空も可。部分再配置で省略不可 |
| `bin_width`, `bin_height` | `long long` | solverでは正の枠寸法、省略不可。validatorだけは既定値0で、0以下ならその軸の上限を検査しない |
| `strip_width` | `long long` | 正の固定幅。省略不可 |
| `objective` | `const rectangle_bounding_objective&` | 既定値`{}`で面積目的。外接矩形関数だけで使う |
| `options` | `const rectangle_pack_options&` | 既定値`{}`。2.6節の全既定設定になる |

`std::vector`や`std::array`をそのままspan引数へ渡せる。spanは呼出中だけの配列の参照であり、関数は入力配列を書き換えない。初期解も参照先を変更せず、返り値として結果を得る。再集計関数だけは明示的に渡した結果の集計を書き換える。

初期解を渡す場合には、次を満たす。

- `placements.size() == items.size()`で、配列のID対応が一致する。
- 配置済みの寸法・回転・非負座標・同一ビン内の非重複・枠内条件が妥当である。
- 集計が現在の入力と配置に一致する。
- 固定ビン・strip・外接矩形では配置済みbinが0。部分repair/repackへ複数ビンを直接渡さない。
- 必須をまだ全部置けていない初期解も使える。その場合は不足を許す条件で幾何を検査する。

座標・寸法・面積・目的値の演算を`long long`の範囲内に収める。strip・外接矩形では、内部の高さ上限に使う各矩形の幅+高さの合計や探索幅との積も安全な範囲にする。外接幅候補の割合計算などの途中演算も含む。利益合計は128bitで足し合わせるが、各候補の最終合計は`long long`に収める。`recompute`やsolverを任意の破損数値の安全な検査器として使わない。

入力前提の多くはassertで確認され、`-DNDEBUG`ではその確認が無効になる。3章のサンプルでは小さい入力範囲を明記し、数値が安全な状態で幾何の検査を行う。

### 2.8 固定ビンの新規配置

~~~text
rectangle_pack_result pack_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {});
~~~

1個の固定ビンへ矩形を置く。`items`は必須・任意を混在できる。`bin_width`と`bin_height`は正。追加探索は`restart_limit`で制御する。

返り値は固定ビンの標準比較で選ばれた`rectangle_pack_result`。配置済みのbinは0、未配置も入力と同じ位置に残る。必須が欠けた結果も返す。`items`とoptionsを変えない。例01・02・08・19・21で使用する。

### 2.9 固定ビンの全体再探索

~~~text
rectangle_pack_result improve_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {});
~~~

初期解を保持候補にしたうえで全体を探索し直す。引数の意味は2.7節どおりで、追加探索は`restart_limit`を使う。

返り値は固定ビンの標準比較で初期解より悪くない結果。すべての座標や採用する任意矩形が変わり得る。初期配置の一部を動かさない指定ではない。初期解の幾何を自動検査する機能もない。例09・19で使用する。

### 2.10 指定IDだけを再配置するrepack

~~~text
rectangle_pack_result repack_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    std::span<const int> movable_item_ids,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {});
~~~

`movable_item_ids`だけをいったん配置から外し、指定外を固定して候補を作る。IDは入力配列の添字で、重複不可。指定外が未配置なら、その要素も未配置のままである。置きたい未配置矩形は可動集合へ入れる。

返り値は再配置候補であり、初期解を保護する比較は行わない。標準の利益や必須欠落が悪くなる可能性もある。独自の距離・移動費用などで、呼出側が候補を採用するか決める用途に使う。独自評価関数を内部へ渡す引数はない。可動集合が空なら初期解をそのまま返す。追加探索は`restart_limit`。例14で使用する。

### 2.11 指定IDだけを改善するrepair

~~~text
rectangle_pack_result repair_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    std::span<const int> movable_item_ids,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {});
~~~

可動・固定の意味はrepackと同じだが、初期解も保持候補にする。返り値は固定ビンの標準比較で初期解より悪くない。指定外の要素は座標・向き・配置済みかどうかを含めて変わらない。

標準比較で同点でも座標は変わる場合があり、任意の独自スコアや移動量についての非悪化を保証しない。可動集合が空なら初期解を返す。追加探索は`restart_limit`。例13・15・17・18で使用する。

### 2.12 Stripの新規配置

~~~text
rectangle_pack_result pack_rectangles_strip(
    std::span<const rectangle_pack_item> items,
    long long strip_width,
    const rectangle_pack_options& options = {});
~~~

全入力を`required=true`にして正の幅を渡す。高さ上限は引数にない。追加探索には`search_iteration_limit`を使う。

返り値の`used_height`が使用高さで、配置済みbinは0。許された向きのどちらでも幅に収まらない矩形などは未配置となり得る。全配置確認も必要である。例03・10で使用する。

### 2.13 Stripの全体再探索

~~~text
rectangle_pack_result improve_rectangles_strip(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    long long strip_width,
    const rectangle_pack_options& options = {});
~~~

同じ入力と幅に対する妥当な初期解を渡す。追加探索は`search_iteration_limit`。返り値はstripの比較順、すなわち必須欠落・高さ・幅の順で初期解より悪くない。

初期配置を局所的に動かすAPIではなく、全体の再探索結果と初期解を比較して返す。初期解自体は変更しない。例10で使用する。

### 2.14 複数ビンの新規配置

~~~text
rectangle_pack_result pack_rectangles_multiple_bins(
    std::span<const rectangle_pack_item> items,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {});
~~~

全入力を`required=true`にし、すべてのビンに共通する正の幅・高さを渡す。追加探索は`search_iteration_limit`。最大ビン数やビンごとの異なる費用は引数にない。

返り値の`placements[i].bin`が所属、`bin_count`が使用ビン数。1個のビンに単独でも収まらない矩形は未配置となり得る。必要な上限枚数は呼出側で確認する。例04で使用する。

### 2.15 複数ビンの全体再探索

~~~text
rectangle_pack_result improve_rectangles_multiple_bins(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {});
~~~

同じビン寸法の妥当な初期解を渡し、追加探索を`search_iteration_limit`で指定する。返り値は必須欠落・bin_count・末尾ビン面積の順で初期解より悪くない。

必須欠落を減らせた場合にはビン数が増える場合がある。全配置同士ならビン数は増えない。初期解のビン番号に欠番がある場合も、比較は実際の非空ビン個数ではなく最大番号+1を使う。所属ビンと座標は全体的に変わり得る。例11で使用する。

### 2.16 自由外接矩形の新規配置

~~~text
rectangle_pack_result pack_rectangles_bounding_box(
    std::span<const rectangle_pack_item> items,
    const rectangle_bounding_objective& objective = {},
    const rectangle_pack_options& options = {});
~~~

全入力を`required=true`にする。幅・高さの固定上限は指定しない。`objective`を省略すると面積目的。`options`を渡したい場合は、引数順に注意し、面積目的でも第2引数に`{}`を置いて`pack_rectangles_bounding_box(items, {}, options)`とする。

返り値の`used_width, used_height`が得られた外接寸法。目的値は`objective.evaluate(result.used_width, result.used_height)`で計算できる。追加探索は`search_iteration_limit`。例05・06・07・12で使用する。

### 2.17 自由外接矩形の全体再探索

~~~text
rectangle_pack_result improve_rectangles_bounding_box(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    const rectangle_bounding_objective& objective = {},
    const rectangle_pack_options& options = {});
~~~

同じ矩形群に対する妥当な初期解を渡す。返り値は、その呼出で渡したobjectiveによる外接矩形の比較順で、初期解より悪くない。

以前とは別のobjectiveを渡すこともできる。その場合、初期解も新しい目的で再評価される。元の目的についても非悪化になるという意味ではない。第3引数がobjective、第4引数がoptions。追加探索は`search_iteration_limit`。例12で使用する。

### 2.18 外部編集後の再集計

~~~text
void recompute_rectangle_packing_summary(
    std::span<const rectangle_pack_item> items,
    rectangle_pack_result& result);
~~~

両引数に既定値はない。`items`は現在の入力、`result`は集計を書き換える対象である。`placements.size() == items.size()`が必要。戻り値はvoidで、`placements`自体は変えず、使用幅・高さ・利益・面積・個数・必須欠落・ビン数を更新する。計算量は`O(N)`。

配置を編集した場合や、利益・必須フラグを変更した場合に使う。座標の修正、重なりの解消、IDの対応付け、数値の安全性検査はしない。安全な入力から集計し、幾何の検査はvalidatorで別に行う。例09・11・13・15・16・17・18・20で使用する。

### 2.19 配置の検査

~~~text
bool validate_rectangle_packing(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& result,
    long long bin_width = 0,
    long long bin_height = 0,
    bool require_all_items = false,
    std::string* error_message = nullptr,
    bool allow_missing_required = false);
~~~

| 引数 | 意味・値・既定値 |
|---|---|
| `items` | 現在の入力配列。省略不可。未配置の入力寸法も正である必要がある |
| `result` | 検査する配置と集計。省略不可。書き換えない |
| `bin_width` | 既定0。正なら各ビンの右端上限、0以下なら幅上限の検査なし |
| `bin_height` | 既定0。正なら各ビンの下端上限、0以下なら高さ上限の検査なし |
| `require_all_items` | 既定false。trueなら任意矩形も含めて全配置を要求する |
| `error_message` | 既定nullptr。失敗理由を書き込むstringのアドレス。理由不要ならnullptr |
| `allow_missing_required` | 既定false。require_all_items=falseの場合だけ、必須の未配置も許す |

| require_all_items | allow_missing_required | 未配置について許される状態 |
|---|---|---|
| false | false | 任意だけ未配置可。必須は全配置が必要 |
| false | true | 必須・任意とも未配置可 |
| true | false | 全矩形を配置する必要がある |
| true | true | 全矩形を配置する必要がある。require_all_itemsが優先される |

返り値がtrueなら、要素数、正の入力寸法、指定された配置要件、配置寸法と回転、非負座標、端点の演算範囲、指定枠、面積・利益合計の範囲、同一ビン内の非重複、集計の一致を通過したことを表す。計算量は`O(N^2)`。

失敗時はfalseを返し、error_messageがあれば理由を設定する。成功時に以前の理由文字列を空にする保証はないため、文字列ではなく返り値で判断する。

次の条件は検査しないので、用途に応じて呼出側で調べる。

- 単一ビン用APIへ渡す結果のbinが0だけであること。
- ビン番号の欠番、最大使用ビン数。
- 可動集合外や障害物の配置が前と同じであること。
- 隙間・外周余白・独自の配線条件。
- 解の最適性。

未配置は`bin < 0`で判定し、その要素の古い配置座標・配置寸法は幾何検査に使わない。ただし、対応する入力矩形の寸法は未配置でも正である必要がある。例20でフラグの意味を確認でき、ほかの例でも補助関数を通して使用する。

### 2.20 結果のストリーム出力

~~~text
std::ostream& operator<<(
    std::ostream& os,
    const rectangle_pack_result& result);
~~~

`os`は出力先、`result`は表示する結果。両引数に既定値はない。`std::cout << result`、`std::cerr << result`、`std::ostringstream`への出力に使える。返り値は渡したosへの参照で、`std::cerr << result << '\n';`のように連結できる。

保持している集計値と、入力ID付きの先頭20配置を表示する。未配置は`unplaced`、残りは`... (残り件数 more)`。空配列も表示できる。元の配列を切り詰める処理ではない。配置数に対する処理量は`O(min(N, 20))`、追加領域は`O(1)`。

boolはtrue/falseとして表示し、数値は出力先の書式設定に従う。書式設定を書き換えず、末尾の改行や明示的なflushも行わない。表示のために集計や妥当性検査はしない。例20で使用する。

### 2.21 APIの選び分け

| 希望 | 選ぶAPI・方法 |
|---|---|
| 初めて配置する | 対応する`pack_rectangles_*` |
| 全体を探索し直し、標準目的で初期解を保護する | 対応する`improve_rectangles_*` |
| 一部を固定し、標準目的で初期解を保護する | 固定ビンの`repair_rectangles_fixed_bin` |
| 一部を固定し、候補を独自の評価で採否判断する | 固定ビンの`repack_rectangles_fixed_bin` |
| 複数ビンのうち1ビンだけ部分編集する | 対象を単一ビンへ切り出してrepair。例17 |
| 入力が次のターンで変わる | ID対応と妥当な配置の継承を呼出側で行う。例18 |
| 外部で配置を編集した | 再集計後、必要な条件で検査する |
| ログを素早く見る | 結果をostreamへ出力する |

初期解は結果として引き継げるが、内部の探索状態を呼出間で保持する永続solverはない。毎回の入力・初期解・設定から関数を呼ぶ。

## 3. ユースケース

### 3.0 各サンプルの実行方法

各例のC++ブロックは`main`、入力、出力、必要な補助関数を含む独立したプログラムである。複数例を1つのcppへ連結せず、使う例を1つだけコピーする。別セクションから共通コードを持ってくる必要はない。

コピーしたソースを`sample.cpp`として、`rectangle_packing_v11.hpp`と同じディレクトリへ置けば、次のようにコンパイルできる。

~~~bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined -Werror sample.cpp -o sample
./sample < input.txt
~~~

同梱の実行用一式ではヘッダーがルート、cppがexamples配下なので、次のように`-I .`を指定する。

~~~bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined -Werror -I . examples/example01_fixed_bin.cpp -o sample
./sample < examples/example01_fixed_bin.in
~~~

どの例も記載された正しい入力形式を前提とする。各集合の矩形数0～50、通常の寸法・枠1～1000、座標0～100000、利益の絶対値1000000以下という小さい範囲を基本にしている。追加の制限は各例にも書く。例21は小数を100倍して使う。この範囲はサンプルの安全な実行範囲であり、ライブラリ全体の固定上限ではない。

多くの例では、出力の先頭に次のsummary行を置く。

~~~text
summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
~~~

その後の各行は`ID bin x y 配置幅 配置高さ 回転済み`。boolの数値表示は0がfalse、1がtrue。未配置行は`ID unplaced`となる。この出力形式はサンプル側で決めたもので、ライブラリが自動で標準出力へ書くものではない。各プログラム内のコメントにも説明している。

掲載した出力は実行確認した一例である。特にdeadlineを使う例19の配置は環境で変わり得る。利用者の要件に対する成功は、出力の充足状態・個数・寸法などから判断する。

| 例 | 用途 | 主なAPI |
|---|---|---|
| 01 | 固定領域へ全矩形を置く | pack_rectangles_fixed_bin |
| 02 | 必須を優先し、利益の高い任意矩形を選ぶ | pack_rectangles_fixed_bin |
| 03 | 幅固定のロール材を短く使う | pack_rectangles_strip |
| 04 | 同じ板を何枚使うか求める | pack_rectangles_multiple_bins |
| 05 | 外接枠の面積を小さくする | pack_rectangles_bounding_box（area） |
| 06 | 外周の長さを小さくする | pack_rectangles_bounding_box（perimeter） |
| 07 | 幅と高さに異なる費用を掛ける | pack_rectangles_bounding_box（weighted_sum） |
| 08 | 規格寸法の中から安い枠を選ぶ | pack_rectangles_fixed_bin |
| 09 | 外部の固定ビン配置を保護して再探索する | improve_rectangles_fixed_bin |
| 10 | 複数seedでロール材の配置を磨く | pack_rectangles_strip / improve_rectangles_strip |
| 11 | 既存のページ分割をまとめ直す | improve_rectangles_multiple_bins |
| 12 | 変更した重みで外接枠を再評価する | pack_rectangles_bounding_box / improve_rectangles_bounding_box |
| 13 | 指定した矩形だけを動かして改善する | repair_rectangles_fixed_bin |
| 14 | 独自の配線長で再配置候補の採否を決める | repack_rectangles_fixed_bin |
| 15 | 固定された矩形障害物を避ける | repair_rectangles_fixed_bin |
| 16 | 矩形間の隙間と外周余白を確保する | pack_rectangles_fixed_bin / recompute_rectangle_packing_summary |
| 17 | 複数ビンのうち1枚だけを部分修復する | repair_rectangles_fixed_bin / recompute_rectangle_packing_summary |
| 18 | ターン間で矩形を追加・削除・変更する | repair_rectangles_fixed_bin / recompute_rectangle_packing_summary |
| 19 | 複数回の探索で終了時刻を共有する | pack_rectangles_fixed_bin / improve_rectangles_fixed_bin / deadline |
| 20 | 中間状態を検査してデバッグ表示する | validate_rectangle_packing / operator<< |
| 21 | 小数2桁の寸法を正確に整数化する | pack_rectangles_fixed_bin |

### 3.01 例01：固定領域へ全矩形を置く

画面や板の大きさが決まっており、その中へ全部の部品を置きたい場面。各部品で回転の可否も指定する。

**具体的に解く問題**

幅W・高さHの1領域へN個を重ねずに配置する。置けなかった必須矩形があれば、その個数とIDも表示する。

**入力**

1行目はN W H。続くN行は幅 高さ 回転許可。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example01_fixed_bin.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 幅W・高さHの1領域へN個を重ねずに配置する。置けなかった必須矩形があれば、
// その個数とIDも表示する。
// 標準入力: 1行目はN W H。続くN行は幅 高さ 回転許可。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long width, height;

    if (!(std::cin >> n >> width >> height)) {
        return 0;
    }

    // 全入力がrequired=true。回転可否は矩形ごとの指定
    const auto items = read_items(n);

    // 配置を求める
    // optionsを省略し既定設定で探索する
    const auto result = pack_rectangles_fixed_bin(items, width, height);

    // 結果を検査して出力する
    // 部分解でも配置済みの幾何と集計を確認する
    check_geometry(items, result, width, height);
    // feasible=falseは探索で全配置できなかった意味。不可能性の証明ではない
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 8 6
3 2 0
2 4 1
3 3 1
```

**標準出力の例**

```text
summary 1 3 0 1 7 4 3 23
0 0 0 2 3 2 0
1 0 0 0 4 2 1
2 0 4 0 3 3 0
```

### 3.02 例02：必須を優先し、利益の高い任意矩形を選ぶ

必ず載せたい情報と、空きがあれば載せたい広告がある紙面のような場面。小さいものを多く置くことと、価値が高いものを選ぶことを区別する。

**具体的に解く問題**

1ビンへ必須矩形を優先して置き、同じ必須欠落数なら配置済み利益を最大化する。未採用の任意矩形も出力する。

**入力**

1行目はN W H。続くN行は幅 高さ 利益 回転許可 必須。利益は負でもよい。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example02_profit_selection.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 1ビンへ必須矩形を優先して置き、同じ必須欠落数なら配置済み利益を最大化する。
// 未採用の任意矩形も出力する。
// 標準入力: 1行目はN W H。続くN行は幅 高さ 利益 回転許可 必須。利益は負でもよい。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・利益・回転許可(0/1)・必須(0/1)
// 結果のIDはこの配列の添字と同じ
std::vector<rectangle_pack_item> read_items(int n) {
    // 入力矩形を用意する
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 必須と任意の矩形を同じ配列に入れる
    for (auto& item : items) {
        int rotation, required;
        // 正の寸法と整数利益
        std::cin >> item.width >> item.height >> item.profit >> rotation >> required;
        // 90度回転を許すかを設定する
        item.rotatable = rotation != 0;
        // falseなら配置しない選択も許す
        item.required = required != 0;
    }

    // 配列をコピーせず関数のspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long width, height;

    if (!(std::cin >> n >> width >> height)) {
        return 0;
    }

    // required=falseの矩形は配置しなくてもよい
    const auto items = read_items(n);

    // 探索条件を設定する
    // 他の設定には既定値を使う
    rectangle_pack_options options;
    // 同じ入力・回数予算なら再現できる乱数seed
    options.seed = 17;
    // 固定ビンの追加探索を指定する。回数を増やしても改善は保証しない
    options.restart_limit = 64;

    // 配置を求める
    // 必須欠落、利益、面積の順で比較する
    const auto result = pack_rectangles_fixed_bin(items, width, height, options);

    // 結果を検査して出力する
    // 任意の未配置と、必須が欠けた候補も幾何検査する
    check_geometry(items, result, width, height);
    // 利益合計には必須の利益も含む。任意利益だけ数えたいなら必須の利益を0にする
    print_result(result);

    return 0;
}
```

**入力例**

```text
4 6 4
2 4 0 0 1
4 4 8 0 0
2 4 7 0 0
2 4 6 0 0
```

**標準出力の例**

```text
summary 1 3 0 1 6 4 13 24
0 0 0 0 2 4 0
1 unplaced
2 0 2 0 2 4 0
3 0 4 0 2 4 0
```

### 3.03 例03：幅固定のロール材を短く使う

ロール紙や布の幅は固定で、長手方向の使用量を減らしたい場面。幅を資源量、高さを占有時間に見立てれば、連続した資源区間を使う単純な時間配置にも使える。

**具体的に解く問題**

幅Wの領域へ全矩形を置き、使用高さを小さくする。高さ上限は入力しない。

**入力**

1行目はN W。続くN行は幅 高さ 回転許可。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example03_strip.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 幅Wの領域へ全矩形を置き、使用高さを小さくする。高さ上限は入力しない。
// 標準入力: 1行目はN W。続くN行は幅 高さ 回転許可。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long width;

    if (!(std::cin >> n >> width)) {
        return 0;
    }

    // stripでは全入力をrequired=trueにする
    const auto items = read_items(n);

    // 探索条件を設定する
    // 期限なし・回数予算で探索する
    rectangle_pack_options options;
    // stripの追加探索に効く。restart_limitはここでは使わない
    options.search_iteration_limit = 64;

    // 配置を求める
    // 主目的は使用高さ
    const auto result = pack_rectangles_strip(items, width, options);

    // 結果を検査して出力する
    // 高さ0はvalidatorで上限なしの意味
    check_geometry(items, result, width, 0);
    // used_heightが必要なロールの長さ。高さと入力寸法の単位をそろえる
    print_result(result);

    return 0;
}
```

**入力例**

```text
4 8
4 3 0
4 2 0
2 3 1
2 2 0
```

**標準出力の例**

```text
summary 1 4 0 1 8 5 4 30
0 0 0 0 4 3 0
1 0 4 0 4 2 0
2 0 2 3 3 2 1
3 0 0 3 2 2 0
```

### 3.04 例04：同じ板を何枚使うか求める

同じ寸法の板やページを複数使える場面。各矩形をどの板に置くかと、板の中の座標を同時に決める。

**具体的に解く問題**

全矩形を同じW×Hのビンへ配置し、使用数を減らす。利用可能枚数L以内の結果が得られたかも判定する。

**入力**

1行目はN W H L。続くN行は幅 高さ 回転許可。Lは0以上。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example04_multiple_bins.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 全矩形を同じW×Hのビンへ配置し、使用数を減らす。
// 利用可能枚数L以内の結果が得られたかも判定する。
// 標準入力: 1行目はN W H L。続くN行は幅 高さ 回転許可。Lは0以上。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n, limit;
    long long width, height;

    if (!(std::cin >> n >> width >> height >> limit)) {
        return 0;
    }

    // 複数ビンでは全矩形を必須にする
    const auto items = read_items(n);

    // 複数のビンへ配置する
    // ビン数自体を入力上限で切らないAPI
    const auto result = pack_rectangles_multiple_bins(items, width, height);

    // 結果とビン数の上限を確認する
    // 幅・高さの上限は全ビンに共通
    check_geometry(items, result, width, height);
    // 上限判定は呼出側で行う
    std::cout << "within_limit " << (result.feasible() && result.bin_count <= limit)
              << '\n';
    // p.binで所属を読む。失敗してもL枚で不可能と証明したことにはならない
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 4 4 2
4 3 0
4 2 0
2 2 1
```

**標準出力の例**

```text
within_limit 1
summary 1 3 0 2 4 4 3 24
0 0 0 0 4 3 0
1 1 0 0 4 2 0
2 1 0 2 2 2 0
```

先頭のwithin_limitは、得られた全配置がL枚以内なら1。0でもL枚で配置不可能という証明ではない。

### 3.05 例05：外接枠の面積を小さくする

画像を1枚のatlasにまとめるなど、幅も高さも自由に選べる場面。枠全体の面積を小さくする。

**具体的に解く問題**

全矩形の外接枠について使用幅×使用高さを小さくする。規格寸法や幅上限は与えない。

**入力**

1行目はN。続くN行は幅 高さ 回転許可。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example05_bounding_area.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 全矩形の外接枠について使用幅×使用高さを小さくする。
// 規格寸法や幅上限は与えない。
// 標準入力: 1行目はN。続くN行は幅 高さ 回転許可。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;

    if (!(std::cin >> n)) {
        return 0;
    }

    // すべてrequired=true
    const auto items = read_items(n);

    // 面積が小さい外接矩形を求める
    // objectiveを省略すると面積、optionsも既定値
    const auto result = pack_rectangles_bounding_box(items);
    // 枠寸法が自由なので両軸とも上限なし
    check_geometry(items, result, 0, 0);

    // 目的値と配置を出力する
    // 既定のtypeはarea、重みはどちらも1
    rectangle_bounding_objective objective;
    // 空結果は0。まずfeasibleも確認する
    std::cout << "area " << objective.evaluate(result.used_width, result.used_height)
              << '\n';
    // used_widthとused_heightが見つかった外接寸法
    print_result(result);

    return 0;
}
```

**入力例**

```text
3
5 2 1
3 3 0
2 4 1
```

**標準出力の例**

```text
area 30
summary 1 3 0 1 6 5 3 27
0 0 4 0 2 5 1
1 0 0 2 3 3 0
2 0 0 0 4 2 1
```

### 3.06 例06：外周の長さを小さくする

外枠に沿って材料や配線を用意するなど、面積より外周の長さを抑えたい場面。

**具体的に解く問題**

全配置した外接矩形の周長を小さくする。APIの目的値は実際の周長の半分であることも出力で確認する。

**入力**

1行目はN。続くN行は幅 高さ 回転許可。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example06_bounding_perimeter.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 全配置した外接矩形の周長を小さくする。
// APIの目的値は実際の周長の半分であることも出力で確認する。
// 標準入力: 1行目はN。続くN行は幅 高さ 回転許可。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;

    if (!(std::cin >> n)) {
        return 0;
    }

    // 全矩形を必須として読む
    const auto items = read_items(n);

    // 半周長を目的として設定する
    // 評価方法を指定する設定
    rectangle_bounding_objective objective;
    // evaluateは幅+高さを返す
    objective.type = rectangle_bounding_objective_type::perimeter;

    // 配置を求めて検査する
    // 同じ型でも目的の設定を渡す
    const auto result = pack_rectangles_bounding_box(items, objective);
    // 自由外接枠の幾何を確認する
    check_geometry(items, result, 0, 0);

    // 半周長と周長を出力する
    // APIの半周長
    const long long half = objective.evaluate(result.used_width, result.used_height);
    // この例の小さい数値範囲なら倍も安全
    std::cout << "half_perimeter " << half << " perimeter " << 2 * half << '\n';
    // 必須充足と実際の寸法を確認する
    print_result(result);

    return 0;
}
```

**入力例**

```text
3
6 2 0
3 3 1
2 2 0
```

**標準出力の例**

```text
half_perimeter 11 perimeter 22
summary 1 3 0 1 6 5 3 25
0 0 0 0 6 2 0
1 0 0 2 3 3 0
2 0 3 2 2 2 0
```

### 3.07 例07：幅と高さに異なる費用を掛ける

横へ伸びると費用が高い設備など、幅と高さを同じ重みで扱いたくない場面。

**具体的に解く問題**

全配置した外接枠のa×使用幅+b×使用高さを小さくする。a,bは正の整数とする。

**入力**

1行目はN a b。続くN行は幅 高さ 回転許可。a,bは1～1000。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example07_bounding_weighted.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 全配置した外接枠のa×使用幅+b×使用高さを小さくする。a,bは正の整数とする。
// 標準入力: 1行目はN a b。続くN行は幅 高さ 回転許可。a,bは1～1000。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long a, b;

    if (!(std::cin >> n >> a >> b)) {
        return 0;
    }

    // 全矩形を必須として読む
    const auto items = read_items(n);

    // 幅と高さの重みを設定する
    // 幅と高さを評価する設定
    rectangle_bounding_objective objective;
    // 正の重み付き和を選ぶ
    objective.type = rectangle_bounding_objective_type::weighted_sum;
    // 幅1単位の費用。0や負は渡さない
    objective.width_weight = a;
    // 高さ1単位の費用。幅の上限制約を指定する値ではない
    objective.height_weight = b;

    // 配置を求めて検査する
    // 与えた目的で自由な枠を探す
    const auto result = pack_rectangles_bounding_box(items, objective);
    // 外接枠なので両軸に固定上限はない
    check_geometry(items, result, 0, 0);

    // 目的値と配置を出力する
    // 寸法を目的値へ変換する
    std::cout << "cost " << objective.evaluate(result.used_width, result.used_height)
              << '\n';
    // 幅に絶対上限が必要なら、この重みで代用しない
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 3 1
5 2 1
3 3 0
2 4 1
```

**標準出力の例**

```text
cost 20
summary 1 3 0 1 4 8 3 27
0 0 0 0 2 5 1
1 0 0 5 3 3 0
2 0 2 0 2 4 0
```

### 3.08 例08：規格寸法の中から安い枠を選ぶ

購入できる板や利用可能な画像サイズが離散的な候補に限られる場面。候補の価格は面積順とは限らない。

**具体的に解く問題**

各規格で全配置を試し、成功した規格の中から価格、面積、候補IDの順に小さいものを選ぶ。

**入力**

1行目はN K。続くN行は幅 高さ 回転許可。その後K行は規格幅 規格高さ 価格。Kは1～20、価格は0～1000000。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example08_catalog_bins.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 各規格で全配置を試し、成功した規格の中から価格、面積、
// 候補IDの順に小さいものを選ぶ。
// 標準入力: 1行目はN K。続くN行は幅 高さ 回転許可。その後K行は規格幅 規格高さ 価格。
// Kは1～20、価格は0～1000000。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n, k;

    if (!(std::cin >> n >> k)) {
        return 0;
    }

    // 全配置したい矩形を読む
    const auto items = read_items(n);

    // 見つかった最良候補を保持する
    // 採用できた候補がまだなければ空
    std::optional<rectangle_pack_result> best;
    std::tuple<long long, long long, int> best_key;
    int chosen = -1;

    // 各規格で配置を試す
    for (int id = 0; id < k; ++id) {
        long long width, height, price;
        std::cin >> width >> height >> price;
        // 規格ごとに独立した固定ビン問題を解く
        const auto candidate = pack_rectangles_fixed_bin(items, width, height);
        // その規格の寸法で検査する
        check_geometry(items, candidate, width, height);

        // 失敗した規格は、この探索では採用しない
        if (!candidate.feasible()) {
            continue;
        }

        const auto key = std::tuple{price, width * height, id};

        // 配置結果も保存する
        if (!best || key < best_key) {
            best = candidate;
            best_key = key;
            chosen = id;
        }
    }

    // 採用した規格と配置を出力する
    // -1は全候補で全配置を発見できなかった意味
    std::cout << "chosen " << chosen << '\n';

    // 発見した候補中の最良であり、規格の不可能性は証明していない
    if (best) {
        print_result(*best);
    }

    return 0;
}
```

**入力例**

```text
2 3
3 2 0
2 2 0
4 2 4
5 2 7
4 4 9
```

**標準出力の例**

```text
chosen 1
summary 1 2 0 1 5 2 2 10
0 0 0 0 3 2 0
1 0 3 0 2 2 0
```

chosenは0始まりの規格ID。候補を発見できなければ-1だけを出力する。

### 3.09 例09：外部の固定ビン配置を保護して再探索する

前のプログラムや人手で作った配置を出発点に、価値や欠落数を悪化させず探索し直したい場面。座標を固定する使い方ではない。

**具体的に解く問題**

利益付き矩形と初期配置を読み、標準の固定ビン比較で初期解より悪くない結果を返す。

**入力**

1行目はN W H。続くN行は幅 高さ 利益 回転許可 必須。その後N行はbin x y 回転済み。binは0か-1。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example09_improve_fixed.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 利益付き矩形と初期配置を読み、
// 標準の固定ビン比較で初期解より悪くない結果を返す。
// 標準入力: 1行目はN W H。続くN行は幅 高さ 利益 回転許可 必須。その後N行はbin x y
// 回転済み。binは0か-1。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・利益・回転許可(0/1)・必須(0/1)
// 結果のIDはこの配列の添字と同じ
std::vector<rectangle_pack_item> read_items(int n) {
    // 入力矩形を用意する
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 必須と任意の矩形を同じ配列に入れる
    for (auto& item : items) {
        int rotation, required;
        // 正の寸法と整数利益
        std::cin >> item.width >> item.height >> item.profit >> rotation >> required;
        // 90度回転を許すかを設定する
        item.rotatable = rotation != 0;
        // falseなら配置しない選択も許す
        item.required = required != 0;
    }

    // 配列をコピーせず関数のspan引数へ渡せる
    return items;
}

// 入力: itemsと同じ順にbin・x・y・回転済み(0/1)。bin=-1なら残り3値は無視する
// 外部配置を結果型へ変換する
rectangle_pack_result read_initial(
    const std::vector<rectangle_pack_item>& items // 入力と配置は同じID順
) {
    rectangle_pack_result result; // 空の配置と0の集計値で初期化する
    result.placements.resize(items.size()); // 未配置も含めて要素を用意する

    for (std::size_t i = 0; i < items.size(); ++i) { // 入力IDの順に読む
        int bin, rotation;
        long long x, y;
        std::cin >> bin >> x >> y >> rotation;

        if (bin < 0) { // 既定のbin=-1のままなら未配置
            continue;
        }

        const auto& item = items[i]; // 回転後の寸法を元の入力から求める
        result.placements[i] = { // 構造体のフィールド順に設定する
            x, // 左上のx座標
            y, // 左上のy座標
            rotation ? item.height : item.width, // 配置幅
            rotation ? item.width : item.height, // 配置高さ
            bin, // 所属ビン
            rotation != 0 // 90度回転したか
        };
    }

    // 小さく安全な数値を前提に集計する。幾何の検査は別途行う。
    recompute_rectangle_packing_summary(items, result);

    return result; // 配置と集計がそろった結果を返す
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long width, height;

    if (!(std::cin >> n >> width >> height)) {
        return 0;
    }

    // 利益と必須・任意を読み込む
    const auto items = read_items(n);
    // 入力binは0か-1。入力順で配置を対応付けて集計する
    const auto initial = read_initial(items);
    // improveは初期解の幾何を自動検査しないので先に確認する
    check_geometry(items, initial, width, height);

    // 再探索の条件を設定する
    // 全体再探索の設定
    rectangle_pack_options options;
    // 初期解を作った探索と別のseedにしてよい
    options.seed = 23;
    // 固定ビンの追加探索回数
    options.restart_limit = 64;

    // 初期解を保持候補にして再探索する
    // 初期解を候補として保持する
    const auto result =
        improve_rectangles_fixed_bin(items, initial, width, height, options);

    // 結果を検査して出力する
    // 返り値を同じ入力・枠で検査する
    check_geometry(items, result, width, height);
    // initial自体は変更されない。任意の独自スコアでの非悪化保証ではない
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 6 4
2 4 0 0 1
2 4 7 0 0
2 4 6 0 0
0 0 0 0
-1 0 0 0
-1 0 0 0
```

**標準出力の例**

```text
summary 1 3 0 1 6 4 13 24
0 0 0 0 2 4 0
1 0 2 0 2 4 0
2 0 4 0 2 4 0
```

### 3.10 例10：複数seedでロール材の配置を磨く

一度得た配置を保持しながら、残った計算量で別の試行も行いたい場面。seedごとに最良の使用高さを引き継ぐ。

**具体的に解く問題**

固定幅で一度配置し、複数回improveを呼んで標準比較で悪化しない全体配置を得る。

**入力**

1行目はN W R。続くN行は幅 高さ 回転許可。Rは追加呼出回数で0～10。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example10_improve_strip.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 固定幅で一度配置し、複数回improveを呼んで標準比較で悪化しない全体配置を得る
// 。
// 標準入力: 1行目はN W R。続くN行は幅 高さ 回転許可。Rは追加呼出回数で0～10。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n, rounds;
    long long width;

    if (!(std::cin >> n >> width >> rounds)) {
        return 0;
    }

    // stripでは全矩形を必須にする
    const auto items = read_items(n);

    // 1回当たりの探索予算を設定する
    // 回数で探索を区切る
    rectangle_pack_options options;
    // 1回当たりの追加予算を小さくする
    options.search_iteration_limit = 8;

    // 初期解を作り、別のseedで改善する
    // まず初期の配置結果を得る
    auto best = pack_rectangles_strip(items, width, options);

    for (int round = 0; round < rounds; ++round) {
        // 再現可能な別seedを順に指定する
        options.seed = static_cast<std::uint64_t>(round + 1);
        // 座標を固定せず、結果比較で初期解を保護する
        best = improve_rectangles_strip(items, best, width, options);
    }

    // 結果を検査して出力する
    // stripなので高さ上限なし
    check_geometry(items, best, width, 0);
    // 使用高さだけでなく必須欠落数も読む
    print_result(best);

    return 0;
}
```

**入力例**

```text
3 6 3
4 2 0
2 3 0
3 2 1
```

**標準出力の例**

```text
summary 1 3 0 1 6 4 3 20
0 0 0 0 4 2 0
1 0 4 0 2 3 0
2 0 0 2 3 2 0
```

### 3.11 例11：既存のページ分割をまとめ直す

既存データが複数ページへ分かれており、ページ数を減らすために全体の割当てを見直したい場面。

**具体的に解く問題**

入力された複数ビンの配置を保持候補にし、全矩形を再配置して標準比較で悪化しない結果を返す。

**入力**

1行目はN W H。続くN行は幅 高さ 回転許可。その後N行はbin x y 回転済み。binは-1か0以上で、配置済みは0から連番のビンを使う。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example11_improve_multiple.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 入力された複数ビンの配置を保持候補にし、
// 全矩形を再配置して標準比較で悪化しない結果を返す。
// 標準入力: 1行目はN W H。続くN行は幅 高さ 回転許可。その後N行はbin x y 回転済み。
// binは-1か0以上で、配置済みは0から連番のビンを使う。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// 入力: itemsと同じ順にbin・x・y・回転済み(0/1)。bin=-1なら残り3値は無視する
// 外部配置を結果型へ変換する
rectangle_pack_result read_initial(
    const std::vector<rectangle_pack_item>& items // 入力と配置は同じID順
) {
    rectangle_pack_result result; // 空の配置と0の集計値で初期化する
    result.placements.resize(items.size()); // 未配置も含めて要素を用意する

    for (std::size_t i = 0; i < items.size(); ++i) { // 入力IDの順に読む
        int bin, rotation;
        long long x, y;
        std::cin >> bin >> x >> y >> rotation;

        if (bin < 0) { // 既定のbin=-1のままなら未配置
            continue;
        }

        const auto& item = items[i]; // 回転後の寸法を元の入力から求める
        result.placements[i] = { // 構造体のフィールド順に設定する
            x, // 左上のx座標
            y, // 左上のy座標
            rotation ? item.height : item.width, // 配置幅
            rotation ? item.width : item.height, // 配置高さ
            bin, // 所属ビン
            rotation != 0 // 90度回転したか
        };
    }

    // 小さく安全な数値を前提に集計する。幾何の検査は別途行う。
    recompute_rectangle_packing_summary(items, result);

    return result; // 配置と集計がそろった結果を返す
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long width, height;

    if (!(std::cin >> n >> width >> height)) {
        return 0;
    }

    // 全矩形を必須として読む
    const auto items = read_items(n);
    // 配置済みbinの番号は0始まりの連番を前提とする
    const auto initial = read_initial(items);
    // 同じビン内の重なりと各ビンの境界を調べる
    check_geometry(items, initial, width, height);

    // 再探索の条件を設定する
    // 複数ビンの追加探索設定
    rectangle_pack_options options;
    // 複数ビンではこちらの予算を使う
    options.search_iteration_limit = 64;

    // 所属ビンを含めて再探索する
    // 各矩形の所属ビンも変更され得る
    const auto result =
        improve_rectangles_multiple_bins(items, initial, width, height, options);

    // 結果とビン数の変化を確認する
    // 新しい所属ごとに幾何を検査する
    check_geometry(items, result, width, height);
    // 必須欠落が減った場合はビン数が増える場合もある
    std::cout << "bins " << initial.bin_count << ' ' << result.bin_count << '\n';
    // 旧bin番号と新bin番号が同じ板を表すと仮定しない
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 4 4
2 2 0
2 2 0
2 2 0
0 0 0 0
1 0 0 0
2 0 0 0
```

**標準出力の例**

```text
bins 3 1
summary 1 3 0 1 4 4 3 12
0 0 0 0 2 2 0
1 0 2 0 2 2 0
2 0 0 2 2 2 0
```

### 3.12 例12：変更した重みで外接枠を再評価する

面積重視で作った配置に対し、横方向の制約が厳しくなったので幅の費用を上げたい場面。寸法の幾何が同じなら、その配置を別目的の初期候補として使える。

**具体的に解く問題**

まず面積目的で配置し、同じ矩形群を正の重み付き和で評価し直してimproveを呼ぶ。

**入力**

1行目はN a b。続くN行は幅 高さ 回転許可。a,bは1～1000。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example12_improve_bounding.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: まず面積目的で配置し、同じ矩形群を正の重み付き和で評価し直してimproveを呼ぶ
// 。
// 標準入力: 1行目はN a b。続くN行は幅 高さ 回転許可。a,bは1～1000。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long a, b;

    if (!(std::cin >> n >> a >> b)) {
        return 0;
    }

    // 入力矩形は変更しない
    const auto items = read_items(n);

    // 面積目的で初期解を作る
    // まず既定の面積目的で配置を得る
    const auto initial = pack_rectangles_bounding_box(items);

    // 新しい目的関数を設定する
    // 新しく使いたい評価方法
    rectangle_bounding_objective objective;
    // 幅高さの重み付き和を選ぶ
    objective.type = rectangle_bounding_objective_type::weighted_sum;
    // 正の幅重み
    objective.width_weight = a;
    // 正の高さ重み
    objective.height_weight = b;

    // 新しい目的で再探索する
    // 非悪化の比較は新しい目的で行う
    const auto result = improve_rectangles_bounding_box(items, initial, objective);
    // 返り値の配置を確認する
    check_geometry(items, result, 0, 0);

    // 同じ目的で初期解と結果を比較する
    // 初期配置も新しい目的で評価する
    std::cout << "new_cost "
              << objective.evaluate(initial.used_width, initial.used_height)
              // 候補の新目的値
              << ' ' << objective.evaluate(result.used_width, result.used_height)
              << '\n';
    // 面積自体の非悪化を要求した例ではない
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 4 1
6 2 1
3 3 0
2 3 1
```

**標準出力の例**

```text
new_cost 29 23
summary 1 3 0 1 3 11 3 27
0 0 0 0 2 6 1
1 0 0 6 3 3 0
2 0 0 9 3 2 1
```

### 3.13 例13：指定した矩形だけを動かして改善する

すでに設置した部品は動かしたくないが、一部の部品だけは配置し直せる場面。未配置の部品を追加する場合も、そのIDを可動に含める。

**具体的に解く問題**

可動ID集合に含まれない配置をそのまま保ち、固定ビンの標準比較で初期解より悪くない配置を求める。

**入力**

1行目はN W H M。N行の幅 高さ 回転許可、N行のbin x y 回転済み、最後にM個の可動ID。binは0か-1。IDは0以上N未満で重複なし。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example13_partial_repair.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 可動ID集合に含まれない配置をそのまま保ち、
// 固定ビンの標準比較で初期解より悪くない配置を求める。
// 標準入力: 1行目はN W H M。N行の幅 高さ 回転許可、N行のbin x y 回転済み、
// 最後にM個の可動ID。binは0か-1。IDは0以上N未満で重複なし。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// 入力: itemsと同じ順にbin・x・y・回転済み(0/1)。bin=-1なら残り3値は無視する
// 外部配置を結果型へ変換する
rectangle_pack_result read_initial(
    const std::vector<rectangle_pack_item>& items // 入力と配置は同じID順
) {
    rectangle_pack_result result; // 空の配置と0の集計値で初期化する
    result.placements.resize(items.size()); // 未配置も含めて要素を用意する

    for (std::size_t i = 0; i < items.size(); ++i) { // 入力IDの順に読む
        int bin, rotation;
        long long x, y;
        std::cin >> bin >> x >> y >> rotation;

        if (bin < 0) { // 既定のbin=-1のままなら未配置
            continue;
        }

        const auto& item = items[i]; // 回転後の寸法を元の入力から求める
        result.placements[i] = { // 構造体のフィールド順に設定する
            x, // 左上のx座標
            y, // 左上のy座標
            rotation ? item.height : item.width, // 配置幅
            rotation ? item.width : item.height, // 配置高さ
            bin, // 所属ビン
            rotation != 0 // 90度回転したか
        };
    }

    // 小さく安全な数値を前提に集計する。幾何の検査は別途行う。
    recompute_rectangle_packing_summary(items, result);

    return result; // 配置と集計がそろった結果を返す
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n, m;
    long long width, height;

    if (!(std::cin >> n >> width >> height >> m)) {
        return 0;
    }

    // 全矩形を必須として読む
    const auto items = read_items(n);
    // 配置済みbinは0だけ。未配置は-1
    const auto initial = read_initial(items);

    // 動かしてよいIDを読む
    // 入力順の矩形IDで可動集合を表す
    std::vector<int> movable(static_cast<std::size_t>(m));

    // 範囲内・重複なしを入力条件とする
    for (int& id : movable) {
        std::cin >> id;
    }

    // 固定部分を残して再探索する
    // 初期解は幾何が妥当である必要がある
    check_geometry(items, initial, width, height);
    // 指定外の配置は未配置も含め固定する
    const auto result =
        repair_rectangles_fixed_bin(items, initial, movable, width, height);

    // 結果を検査して出力する
    // 返り値の幾何を確認する
    check_geometry(items, result, width, height);
    // 非悪化は標準目的について。可動集合が空なら初期解が返る
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 8 4 2
2 4 0
2 4 0
2 4 0
0 0 0 0
-1 0 0 0
-1 0 0 0
1 2
```

**標準出力の例**

```text
summary 1 3 0 1 6 4 3 24
0 0 0 0 2 4 0
1 0 2 0 2 4 0
2 0 4 0 2 4 0
```

### 3.14 例14：独自の配線長で再配置候補の採否を決める

部品が重ならないだけでなく、接続する部品同士も近づけたい場面。接続情報をライブラリ内部へ渡すAPIはないため、生成された候補を外側で評価する。

**具体的に解く問題**

全配置済みの初期解から可動矩形をrepackし、接続ペアの中心間マンハッタン距離の合計が減った場合だけ採用する。ここで距離は横方向の差の絶対値と縦方向の差の絶対値の和である。中心座標の小数を避けるため距離を2倍で計算する。

**入力**

1行目はN W H M E。N行の幅 高さ 回転許可、N行のbin x y 回転済み、M個の可動ID、E行の接続端点ID u v。初期解はbin=0の全配置。IDは範囲内、可動IDは重複なし。Eは0～100。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example14_external_wire_cost.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 全配置済みの初期解から可動矩形をrepackし、
// 接続ペアの中心間マンハッタン距離の合計が減った場合だけ採用する。
// ここで距離は横方向の差の絶対値と縦方向の差の絶対値の和である。
// 中心座標の小数を避けるため距離を2倍で計算する。
// 標準入力: 1行目はN W H M E。N行の幅 高さ 回転許可、N行のbin x y 回転済み、
// M個の可動ID、E行の接続端点ID u v。初期解はbin=0の全配置。IDは範囲内、
// 可動IDは重複なし。Eは0～100。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// 入力: itemsと同じ順にbin・x・y・回転済み(0/1)。bin=-1なら残り3値は無視する
// 外部配置を結果型へ変換する
rectangle_pack_result read_initial(
    const std::vector<rectangle_pack_item>& items // 入力と配置は同じID順
) {
    rectangle_pack_result result; // 空の配置と0の集計値で初期化する
    result.placements.resize(items.size()); // 未配置も含めて要素を用意する

    for (std::size_t i = 0; i < items.size(); ++i) { // 入力IDの順に読む
        int bin, rotation;
        long long x, y;
        std::cin >> bin >> x >> y >> rotation;

        if (bin < 0) { // 既定のbin=-1のままなら未配置
            continue;
        }

        const auto& item = items[i]; // 回転後の寸法を元の入力から求める
        result.placements[i] = { // 構造体のフィールド順に設定する
            x, // 左上のx座標
            y, // 左上のy座標
            rotation ? item.height : item.width, // 配置幅
            rotation ? item.width : item.height, // 配置高さ
            bin, // 所属ビン
            rotation != 0 // 90度回転したか
        };
    }

    // 小さく安全な数値を前提に集計する。幾何の検査は別途行う。
    recompute_rectangle_packing_summary(items, result);

    return result; // 配置と集計がそろった結果を返す
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

// サンプル側の独自評価。全矩形が配置済みのときだけ呼ぶ
// 配置結果と接続ペアを受け取る
long long wire_cost_twice(const rectangle_pack_result& result,
    const std::vector<std::pair<int, int>>& edges) {
    long long total = 0;

    for (const auto& [u, v] : edges) {
        // 接続元の配置
        const auto& a = result.placements[static_cast<std::size_t>(u)];
        // 接続先の配置
        const auto& b = result.placements[static_cast<std::size_t>(v)];
        // 中心x座標の差の2倍
        total += std::llabs((2 * a.x + a.width) - (2 * b.x + b.width));
        // 中心y座標の差の2倍
        total += std::llabs((2 * a.y + a.height) - (2 * b.y + b.height));
    }

    return total;
}

int main() {
    int n, m, e;
    long long width, height;

    if (!(std::cin >> n >> width >> height >> m >> e)) {
        return 0;
    }

    // この例では全矩形が必須
    const auto items = read_items(n);
    // 中心距離を読めるよう、全配置を入力条件とする
    auto current = read_initial(items);

    // 可動IDと接続ペアを読む
    // 動かせる入力IDだけを指定する
    std::vector<int> movable(static_cast<std::size_t>(m));

    for (int& id : movable) {
        std::cin >> id;
    }

    std::vector<std::pair<int, int>> edges(static_cast<std::size_t>(e));

    for (auto& [u, v] : edges) {
        std::cin >> u >> v;
    }

    // 初期解が全配置であることを確認する
    std::string error;

    // trueで全矩形の配置を要求する
    if (!validate_rectangle_packing(items, current, width, height, true, &error)) {
        std::cerr << error << '\n';

        return 1;
    }

    // 再配置の候補を作る
    // 初期解との標準比較をせず候補を返す
    const auto candidate =
        repack_rectangles_fixed_bin(items, current, movable, width, height);
    // 候補が部分解でも、幾何の妥当性は調べる
    check_geometry(items, candidate, width, height);

    // 独自スコアが改善したときだけ採用する
    // 初期解の独自評価
    const long long before = wire_cost_twice(current, edges);
    // 未配置がある候補の中心は読まない
    const bool accept =
        candidate.feasible() && wire_cost_twice(candidate, edges) < before;

    // 独自スコアが厳密に改善した場合だけ結果を置き換える
    if (accept) {
        current = candidate;
    }

    // 採否と独自スコアを出力する
    std::cout << "accepted " << accept << " wire_cost_x2 " << before << ' '
              << wire_cost_twice(current, edges) << '\n';
    // 配線長は候補の生成方針には反映されず、改善が起きる保証もない
    print_result(current);

    return 0;
}
```

**入力例**

```text
3 10 4 1 1
2 2 0
2 2 0
2 2 0
0 0 0 0
0 8 0 0
0 4 0 0
1
0 1
```

**標準出力の例**

```text
accepted 1 wire_cost_x2 16 4
summary 1 3 0 1 6 2 3 12
0 0 0 0 2 2 0
1 0 2 0 2 2 0
2 0 4 0 2 2 0
```

wire_cost_x2は中心間マンハッタン距離合計の2倍。accepted=0なら初期解をそのまま返す。

### 3.15 例15：固定された矩形障害物を避ける

板に使えない区画がある、またはすでに動かせない設備がある場面。障害物を利益0の固定矩形として同じ配置に入れる。

**具体的に解く問題**

互いに重ならないQ個の矩形障害物を固定し、残りの領域へN個の部品を追加する。

**入力**

1行目はN Q W H。続くQ行は障害物のx y 幅 高さ。その後N行は部品の幅 高さ 回転許可。障害物は枠内かつ互いに非重複。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example15_obstacles.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 互いに重ならないQ個の矩形障害物を固定し、残りの領域へN個の部品を追加する。
// 標準入力: 1行目はN Q W H。続くQ行は障害物のx y 幅 高さ。その後N行は部品の幅 高さ
// 回転許可。障害物は枠内かつ互いに非重複。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n, q;
    long long width, height;

    if (!(std::cin >> n >> q >> width >> height)) {
        return 0;
    }

    // 障害物を固定配置として登録する
    // [障害物, 部品]のID順で管理する
    std::vector<rectangle_pack_item> items;
    // 固定障害物だけが置かれた初期解
    rectangle_pack_result base;
    // 未配置の部品の要素も用意する
    base.placements.resize(static_cast<std::size_t>(q + n));

    for (int id = 0; id < q; ++id) {
        long long x, y, w, h;
        std::cin >> x >> y >> w >> h;
        // 障害物は利益0・回転禁止・必須として表す
        items.push_back({w, h, 0, false, true});
        // 指定位置に固定する
        base.placements[static_cast<std::size_t>(id)] = {x, y, w, h, 0, false};
    }

    // 追加する部品を登録して初期解を検査する
    // 追加したい通常の部品を読む
    const auto pieces = read_items(n);
    // 部品IDはq+iになる
    items.insert(items.end(), pieces.begin(), pieces.end());
    // 障害物を含む集計と部品の必須欠落を計算する
    recompute_rectangle_packing_summary(items, base);
    // 障害物の重なりや枠外もここで拒否する
    check_geometry(items, base, width, height);

    // 部品だけを可動にして配置する
    // 可動にするのは部品だけ
    std::vector<int> movable(static_cast<std::size_t>(n));
    // qからq+n-1までを可動IDとする
    std::iota(movable.begin(), movable.end(), q);
    // 障害物IDを渡さないので動かない
    const auto result =
        repair_rectangles_fixed_bin(items, base, movable, width, height);

    // 結果を検査して出力する
    // 障害物との非重複も含めて確認する
    check_geometry(items, result, width, height);
    // 先頭q個は障害物。placed_countとplaced_areaにも障害物が含まれる
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 1 8 6
3 2 2 2
3 2 0
2 2 1
2 3 0
```

**標準出力の例**

```text
summary 1 4 0 1 5 5 3 20
0 0 3 2 2 2 0
1 0 0 0 3 2 0
2 0 3 0 2 2 0
3 0 0 2 2 3 0
```

### 3.16 例16：矩形間の隙間と外周余白を確保する

画像のにじみを防ぐ隙間や、加工時の余白が必要な場面。全矩形に同じ隙間を要求する場合は、寸法を膨らませて配置した後に元へ戻せる。

**具体的に解く問題**

上下左右の外周からb以上離し、どの2矩形も横方向または縦方向にg以上の隙間を持つよう全配置する。斜めのユークリッド距離を制約する例ではない。

**入力**

1行目はN W H g b。続くN行は幅 高さ 回転許可。g,bは0～100、W>2b、H>2b。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example16_gap_border.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 上下左右の外周からb以上離し、
// どの2矩形も横方向または縦方向にg以上の隙間を持つよう全配置する。
// 斜めのユークリッド距離を制約する例ではない。
// 標準入力: 1行目はN W H g b。続くN行は幅 高さ 回転許可。g,bは0～100、W>2b、H>2b。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n;
    long long width, height, gap, border;

    if (!(std::cin >> n >> width >> height >> gap >> border)) {
        return 0;
    }

    // 元の寸法を保持する。全矩形は必須
    const auto items = read_items(n);

    // 隙間を含めた探索用の寸法へ変換する
    // 探索用だけ寸法を拡大する
    auto padded = items;

    // 両辺に同じ量を足すので90度回転とも整合する
    for (auto& item : padded) {
        item.width += gap;
        item.height += gap;
    }

    // 最後の矩形の右側には余分なgapを要求しない
    const long long inner_width = width - 2 * border + gap;
    // 下側についても同様
    const long long inner_height = height - 2 * border + gap;

    // 拡大した寸法で配置を求める
    // 拡大寸法で重ならない配置を求める
    auto result = pack_rectangles_fixed_bin(padded, inner_width, inner_height);
    // まず拡大寸法で検査する
    check_geometry(padded, result, inner_width, inner_height);

    // 元の寸法と座標系へ戻す
    // 入力順の各配置を元の座標系へ戻す
    for (auto& p : result.placements) {
        // 未配置の値は変換しない
        if (!p.placed()) {
            continue;
        }

        // 外周余白の分だけ移動する
        p.x += border;
        p.y += border;
        // 回転後の実際の幅・高さへ戻す
        p.width -= gap;
        p.height -= gap;
    }

    // 元の問題で再集計し、余白と隙間も検査する
    // 元寸法で面積と外接サイズを再計算する
    recompute_rectangle_packing_summary(items, result);
    // validator自体はgap・borderを検査しない
    check_geometry(items, result, width, height);

    // サンプル側で追加条件を検査する
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        // i番の実寸法配置
        const auto& a = result.placements[i];

        // 未配置の矩形は追加条件の対象外
        if (!a.placed()) {
            continue;
        }

        // 外周余白
        if (a.x < border || a.y < border || a.x + a.width > width - border
            || a.y + a.height > height - border) {
            return 1;
        }

        for (std::size_t j = 0; j < i; ++j) {
            // 比較対象の実寸法配置
            const auto& b = result.placements[j];

            // 未配置は読まない
            if (!b.placed()) {
                continue;
            }

            // 横または縦にgap以上離れていれば、隙間の条件を満たす。
            if (!(a.x + a.width + gap <= b.x || b.x + b.width + gap <= a.x
                    || a.y + a.height + gap <= b.y || b.y + b.height + gap <= a.y)) {
                return 1;
            }
        }
    }

    // 元の単位で配置を出力する
    // 出力する座標・寸法・面積は元の問題の値
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 12 9 1 1
3 2 0
2 3 1
2 2 0
```

**標準出力の例**

```text
summary 1 3 0 1 11 3 3 16
0 0 1 1 3 2 0
1 0 5 1 3 2 1
2 0 9 1 2 2 0
```

### 3.17 例17：複数ビンのうち1枚だけを部分修復する

全体は複数ページだが、今編集している1ページの一部だけを動かしたい場面。固定ビン用のrepairへ全体配置を直接渡さず、対象ビンを取り出す。

**具体的に解く問題**

対象ビンにすでに配置された矩形だけを局所問題へ写し、指定IDをrepairした後、元の全体配置へ戻す。ほかのビンは不変とする。

**入力**

1行目はN W H 対象bin M。N行の幅 高さ 回転許可、N行のbin x y 回転済み、M個の可動な全体ID。指定IDは対象binに属し、重複しない。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example17_one_bin_repair.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 対象ビンにすでに配置された矩形だけを局所問題へ写し、指定IDをrepairした後、
// 元の全体配置へ戻す。ほかのビンは不変とする。
// 標準入力: 1行目はN W H 対象bin M。N行の幅 高さ 回転許可、N行のbin x y 回転済み、
// M個の可動な全体ID。指定IDは対象binに属し、重複しない。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// 入力: itemsと同じ順にbin・x・y・回転済み(0/1)。bin=-1なら残り3値は無視する
// 外部配置を結果型へ変換する
rectangle_pack_result read_initial(
    const std::vector<rectangle_pack_item>& items // 入力と配置は同じID順
) {
    rectangle_pack_result result; // 空の配置と0の集計値で初期化する
    result.placements.resize(items.size()); // 未配置も含めて要素を用意する

    for (std::size_t i = 0; i < items.size(); ++i) { // 入力IDの順に読む
        int bin, rotation;
        long long x, y;
        std::cin >> bin >> x >> y >> rotation;

        if (bin < 0) { // 既定のbin=-1のままなら未配置
            continue;
        }

        const auto& item = items[i]; // 回転後の寸法を元の入力から求める
        result.placements[i] = { // 構造体のフィールド順に設定する
            x, // 左上のx座標
            y, // 左上のy座標
            rotation ? item.height : item.width, // 配置幅
            rotation ? item.width : item.height, // 配置高さ
            bin, // 所属ビン
            rotation != 0 // 90度回転したか
        };
    }

    // 小さく安全な数値を前提に集計する。幾何の検査は別途行う。
    recompute_rectangle_packing_summary(items, result);

    return result; // 配置と集計がそろった結果を返す
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int n, target_bin, m;
    long long width, height;

    if (!(std::cin >> n >> width >> height >> target_bin >> m)) {
        return 0;
    }

    // 全体の矩形入力
    const auto items = read_items(n);
    // 複数ビンの現在配置
    auto result = read_initial(items);
    // 切出し前に全体を検査する
    check_geometry(items, result, width, height);

    // 対象ビンを単一ビンの局所問題へ切り出す
    // 対象ビンだけの入力
    std::vector<rectangle_pack_item> local_items;
    // 局所問題では配置済みbinを0にそろえる
    rectangle_pack_result local;
    std::vector<int> global_ids, local_ids(static_cast<std::size_t>(n), -1);

    for (int id = 0; id < n; ++id) {
        // 元の配置をコピーする
        auto p = result.placements[static_cast<std::size_t>(id)];

        // 対象ビンの既配置だけを取り出す
        if (!p.placed() || p.bin != target_bin) {
            continue;
        }

        local_ids[static_cast<std::size_t>(id)] = static_cast<int>(global_ids.size());
        global_ids.push_back(id);
        // 局所ID順に入力を並べる
        local_items.push_back(items[static_cast<std::size_t>(id)]);
        // repairは単一ビン用なので0へ変換する
        p.bin = 0;
        // 入力と配置を同じ局所ID順にする
        local.placements.push_back(p);
    }

    // 全体IDを局所IDへ変換する
    std::vector<int> movable;

    for (int i = 0; i < m; ++i) {
        int global_id;
        std::cin >> global_id;

        if (global_id < 0 || global_id >= n
            || local_ids[static_cast<std::size_t>(global_id)] < 0) {
            return 1;
        }

        // repairへ渡すIDも局所IDへ変換する
        movable.push_back(local_ids[static_cast<std::size_t>(global_id)]);
    }

    // 局所問題を解いて全体へ書き戻す
    // 単一ビンとしての集計を作り直す
    recompute_rectangle_packing_summary(local_items, local);
    // 対象内の指定外も固定される
    const auto repaired =
        repair_rectangles_fixed_bin(local_items, local, movable, width, height);

    for (std::size_t i = 0; i < global_ids.size(); ++i) {
        // 局所結果を読み出す
        auto p = repaired.placements[i];

        // 配置済みだけ元のbin番号へ戻す
        if (p.placed()) {
            p.bin = target_bin;
        }

        // 元の全体IDの場所へ書き戻す
        result.placements[static_cast<std::size_t>(global_ids[i])] = p;
    }

    // 全体を再集計して検査・出力する
    // 全体の集計を更新する
    recompute_rectangle_packing_summary(items, result);
    // 他ビンを含めて妥当性を確認する
    check_geometry(items, result, width, height);
    // 局所の固定ビン目的での改善であり、複数ビン全体の目的とは別
    print_result(result);

    return 0;
}
```

**入力例**

```text
4 8 6 1 1
2 2 0
2 2 0
2 2 0
2 2 0
0 0 0 0
0 2 0 0
1 0 2 0
1 6 2 0
3
```

**標準出力の例**

```text
summary 1 4 0 2 4 4 4 16
0 0 0 0 2 2 0
1 0 2 0 2 2 0
2 1 0 2 2 2 0
3 1 0 0 2 2 0
```

### 3.18 例18：ターン間で矩形を追加・削除・変更する

画面に新しい要素が増えたり、既存要素の寸法や重要度が変わったりするターン制問題。新旧のIDを対応付け、再利用できる配置だけを残す。

**具体的に解く問題**

新IDから旧IDへの対応を読み、寸法・回転許可・新しい枠に適合する配置を引き継ぐ。新規、寸法変更、枠外、未配置の矩形は自動で可動にする。

**入力**

1行目は旧N 新N 旧W 旧H 新W 新H。旧N行の幅 高さ 利益 回転許可 必須、旧N行のbin x y 回転済み、新N行の幅 高さ 利益 回転許可 必須、新N行の旧ID 強制可動(0/1)。新規は旧ID=-1。0以上の旧IDは範囲内で重複なし。旧binは0か-1。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example18_turn_update.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 新IDから旧IDへの対応を読み、寸法・回転許可・新しい枠に適合する配置を引き継
// ぐ。新規、寸法変更、枠外、未配置の矩形は自動で可動にする。
// 標準入力: 1行目は旧N 新N 旧W 旧H 新W 新H。旧N行の幅 高さ 利益 回転許可 必須、
// 旧N行のbin x y 回転済み、新N行の幅 高さ 利益 回転許可 必須、新N行の旧ID
// 強制可動(0/1)。新規は旧ID=-1。0以上の旧IDは範囲内で重複なし。旧binは0か-1。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・利益・回転許可(0/1)・必須(0/1)
// 結果のIDはこの配列の添字と同じ
std::vector<rectangle_pack_item> read_items(int n) {
    // 入力矩形を用意する
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 必須と任意の矩形を同じ配列に入れる
    for (auto& item : items) {
        int rotation, required;
        // 正の寸法と整数利益
        std::cin >> item.width >> item.height >> item.profit >> rotation >> required;
        // 90度回転を許すかを設定する
        item.rotatable = rotation != 0;
        // falseなら配置しない選択も許す
        item.required = required != 0;
    }

    // 配列をコピーせず関数のspan引数へ渡せる
    return items;
}

// 入力: itemsと同じ順にbin・x・y・回転済み(0/1)。bin=-1なら残り3値は無視する
// 外部配置を結果型へ変換する
rectangle_pack_result read_initial(
    const std::vector<rectangle_pack_item>& items // 入力と配置は同じID順
) {
    rectangle_pack_result result; // 空の配置と0の集計値で初期化する
    result.placements.resize(items.size()); // 未配置も含めて要素を用意する

    for (std::size_t i = 0; i < items.size(); ++i) { // 入力IDの順に読む
        int bin, rotation;
        long long x, y;
        std::cin >> bin >> x >> y >> rotation;

        if (bin < 0) { // 既定のbin=-1のままなら未配置
            continue;
        }

        const auto& item = items[i]; // 回転後の寸法を元の入力から求める
        result.placements[i] = { // 構造体のフィールド順に設定する
            x, // 左上のx座標
            y, // 左上のy座標
            rotation ? item.height : item.width, // 配置幅
            rotation ? item.width : item.height, // 配置高さ
            bin, // 所属ビン
            rotation != 0 // 90度回転したか
        };
    }

    // 小さく安全な数値を前提に集計する。幾何の検査は別途行う。
    recompute_rectangle_packing_summary(items, result);

    return result; // 配置と集計がそろった結果を返す
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    int old_n, n;
    long long old_width, old_height, width, height;

    if (!(std::cin >> old_n >> n >> old_width >> old_height >> width >> height)) {
        return 0;
    }

    // 前ターンの入力
    const auto old_items = read_items(old_n);
    // 前ターンの配置を読み集計する
    const auto previous = read_initial(old_items);
    // 再利用前に旧問題で妥当性を確認する
    check_geometry(old_items, previous, old_width, old_height);

    // 新しい入力に合わせて初期解を作る
    // 新しい寸法・利益・回転許可・必須を読む
    const auto items = read_items(n);
    // 新ターン側のID順で初期解を作る
    rectangle_pack_result base;
    // 未配置を含めて新しい要素数へそろえる
    base.placements.resize(items.size());
    std::vector<int> movable;

    // 再利用できる配置を引き継ぐ
    for (int id = 0; id < n; ++id) {
        int old_id, force_move;
        std::cin >> old_id >> force_move;
        // 新しい矩形の条件
        const auto& item = items[static_cast<std::size_t>(id)];

        if (old_id >= 0) {
            // 対応する旧配置
            const auto& p = previous.placements[static_cast<std::size_t>(old_id)];
            // 旧向きを保った場合の新しい幅
            const long long w = p.rotated ? item.height : item.width;
            // 旧向きを保った場合の新しい高さ
            const long long h = p.rotated ? item.width : item.height;

            // 寸法・回転条件が一致し、新しい枠内に収まる場合だけ継承する。
            if (p.placed() && (!p.rotated || item.rotatable) && p.width == w
                && p.height == h && p.x + w <= width && p.y + h <= height) {
                base.placements[static_cast<std::size_t>(id)] = p;
            }
        }

        // 継承できない要素は必ず可動にする
        if (force_move || !base.placements[static_cast<std::size_t>(id)].placed()) {
            movable.push_back(id);
        }
    }

    // 新しい条件で集計し、可動部分を再探索する
    // 新しい利益・必須フラグで集計を作る
    recompute_rectangle_packing_summary(items, base);
    // 一意なID対応で保った配置の幾何を確認する
    check_geometry(items, base, width, height);
    // 有効な固定部分を残して再探索する
    const auto result =
        repair_rectangles_fixed_bin(items, base, movable, width, height);

    // 結果を検査して出力する
    // 新しい入力と枠に対する結果を検査する
    check_geometry(items, result, width, height);
    // 全配置できなければ、次の試行で可動集合を広げるなどの判断が必要
    print_result(result);

    return 0;
}
```

**入力例**

```text
3 3 8 6 6 4
2 2 1 0 1
3 2 1 0 1
2 2 1 0 1
0 0 0 0
0 2 0 0
0 6 0 0
2 2 5 0 1
3 3 7 0 1
2 1 3 0 1
0 0
1 0
-1 0
```

**標準出力の例**

```text
summary 1 3 0 1 5 3 15 15
0 0 0 0 2 2 0
1 0 2 0 3 3 0
2 0 0 2 2 1 0
```

### 3.19 例19：複数回の探索で終了時刻を共有する

AHC全体の時間制限の中で、配置と追加改善に同じ残り時間を使わせたい場面。絶対時刻を共有し、呼出ごとに予算の時計をリセットしない。

**具体的に解く問題**

入力開始時からの総時間予算の一部を出力用に残し、固定ビン配置と最大3回の改善に同じdeadlineを渡す。

**入力**

1行目はN W H budget_ms。続くN行は幅 高さ 回転許可。budget_msは5～2000。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example19_shared_deadline.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 入力開始時からの総時間予算の一部を出力用に残し、
// 固定ビン配置と最大3回の改善に同じdeadlineを渡す。
// 標準入力: 1行目はN W H budget_ms。続くN行は幅 高さ 回転許可。budget_msは5～2000。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・90度回転の許可(0/1)。全矩形を必須として読む
// 入力順が結果の矩形IDになる
std::vector<rectangle_pack_item> read_items(int n) {
    // N個の入力を作る
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 各矩形の入力フィールドを設定する
    for (auto& item : items) {
        int rotation;
        // 幅・高さは正の整数
        std::cin >> item.width >> item.height >> rotation;
        // trueなら幅と高さの交換を許す
        item.rotatable = rotation != 0;
        // この例では全矩形の配置を求める
        item.required = true;
        // 選別をしない例なので価値を1にそろえる
        item.profit = 1;
    }

    // 配列をそのままspan引数へ渡せる
    return items;
}

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 出力: summary 必須充足 配置数 必須欠落数 ビン数 使用幅 使用高さ 利益 面積
// 続く各行: ID bin x y 配置幅 配置高さ 回転済み。未配置は「ID unplaced」
// 集計と全配置を用途に合わせて出力する
void print_result(const rectangle_pack_result& result) {
    // 集計は保存済みの値。feasible()は必須欠落が0かだけを確認する。
    std::cout << "summary " << result.feasible() // 必須を全部配置できたか
              << ' ' << result.placed_count // 配置済みの個数
              << ' ' << result.missing_required_count // 必須の未配置数
              << ' ' << result.bin_count // 最大bin番号+1
              << ' ' << result.used_width // ビン内の最大右端
              << ' ' << result.used_height // ビン内の最大下端
              << ' ' << result.placed_profit // 配置済みの利益合計
              << ' ' << result.placed_area << '\n'; // 配置済みの面積合計

    // 配列は入力順。未配置の要素も同じIDの位置に残る。
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        const auto& p = result.placements[i]; // 入力i番の配置

        // binが負なら未配置なので、座標や寸法は出力しない。
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        std::cout << i // 入力ID
                  << ' ' << p.bin // 所属ビン
                  << ' ' << p.x // 左上のx座標
                  << ' ' << p.y // 左上のy座標
                  << ' ' << p.width // 回転後の幅
                  << ' ' << p.height // 回転後の高さ
                  << ' ' << p.rotated << '\n'; // 90度回転したか
    }
}

int main() {
    const auto start = std::chrono::steady_clock::now();
    int n, budget_ms;
    long long width, height;

    if (!(std::cin >> n >> width >> height >> budget_ms)) {
        return 0;
    }

    // 入力を読む時間もstartからの予算に含める
    const auto items = read_items(n);

    // 検査・出力時間を残して共通の締切を設定する
    const int reserve_ms = std::max(1, budget_ms / 20);
    // 複数呼出で同じ設定オブジェクトを使う
    rectangle_pack_options options;
    // steady_clockの絶対時刻
    options.deadline = start + std::chrono::milliseconds(budget_ms - reserve_ms);
    // 有限の大きな回数上限。deadlineと先に達した方で停止する
    options.restart_limit = 100000;
    // 内部停止確認のたびに時計を見る。1矩形ごとの確認ではない
    options.time_check_interval = 1;

    // 締切を変えずに初期探索と改善を行う
    // 期限が過ぎても初期候補の構築は行う
    auto best = pack_rectangles_fixed_bin(items, width, height, options);

    for (int round = 1; round <= 3; ++round) {
        // 呼出前にも期限を確認する
        if (std::chrono::steady_clock::now() >= options.deadline) {
            break;
        }

        // 期限を変更せずseedだけ変える
        options.seed = static_cast<std::uint64_t>(round);
        // 同じ締切と保持候補を使う
        best = improve_rectangles_fixed_bin(items, best, width, height, options);
    }

    // 結果を検査して出力する
    // この検査・出力の時間も全体予算に見込む
    check_geometry(items, best, width, height);
    // deadlineは厳密な壁時計上限ではない。余裕は実入力で測って調整する
    print_result(best);

    return 0;
}
```

**入力例**

```text
4 8 6 50
4 3 0
4 2 0
2 3 1
2 2 0
```

**標準出力の例**

```text
summary 1 4 0 1 8 5 4 30
0 0 0 0 4 3 0
1 0 4 0 4 2 0
2 0 4 2 3 2 1
3 0 0 3 2 2 0
```

実時間で打ち切るため、配置は環境により変わり得る。掲載出力は実行時の一例。

### 3.20 例20：中間状態を検査してデバッグ表示する

まだすべてを配置していない状態や、外部で編集した座標を確認したい場面。必須が欠けていることと、配置そのものが不正なことを分ける。

**具体的に解く問題**

小さい数値の外部配置を再集計し、全配置要求と必須欠落許可の指定に従って検査する。結果をそのままcerrへ表示する。

**入力**

1行目はN W H require_all allow_missing。N行の幅 高さ 利益 回転許可 必須、その後N行はbin x y 回転済み。両フラグは0/1。寸法・座標は共通の小さい入力範囲とし、重なりや枠外は検査対象として入力できる。

通常の数値範囲は、矩形数が各集合0～50、寸法・正の枠が1～1000、座標が0～100000、利益の絶対値が1000000以下。初期配置を使う例では、記載されたID対応・ビン・幾何の条件も満たす。

**実行できるコード：example20_validate_debug.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 小さい数値の外部配置を再集計し、
// 全配置要求と必須欠落許可の指定に従って検査する。結果をそのままcerrへ表示する。
// 標準入力: 1行目はN W H require_all allow_missing。N行の幅 高さ 利益 回転許可 必須、
// その後N行はbin x y 回転済み。両フラグは0/1。寸法・座標は共通の小さい入力範囲とし、
// 重なりや枠外は検査対象として入力できる。
// 数値前提: 矩形数は各集合0～50、寸法・正の枠は1～1000、座標は0～100000、
// 利益の絶対値は1000000以下。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// 入力: 各行が幅・高さ・利益・回転許可(0/1)・必須(0/1)
// 結果のIDはこの配列の添字と同じ
std::vector<rectangle_pack_item> read_items(int n) {
    // 入力矩形を用意する
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 必須と任意の矩形を同じ配列に入れる
    for (auto& item : items) {
        int rotation, required;
        // 正の寸法と整数利益
        std::cin >> item.width >> item.height >> item.profit >> rotation >> required;
        // 90度回転を許すかを設定する
        item.rotatable = rotation != 0;
        // falseなら配置しない選択も許す
        item.required = required != 0;
    }

    // 配列をコピーせず関数のspan引数へ渡せる
    return items;
}

// 入力: itemsと同じ順にbin・x・y・回転済み(0/1)。bin=-1なら残り3値は無視する
// 外部配置を結果型へ変換する
rectangle_pack_result read_initial(
    const std::vector<rectangle_pack_item>& items // 入力と配置は同じID順
) {
    rectangle_pack_result result; // 空の配置と0の集計値で初期化する
    result.placements.resize(items.size()); // 未配置も含めて要素を用意する

    for (std::size_t i = 0; i < items.size(); ++i) { // 入力IDの順に読む
        int bin, rotation;
        long long x, y;
        std::cin >> bin >> x >> y >> rotation;

        if (bin < 0) { // 既定のbin=-1のままなら未配置
            continue;
        }

        const auto& item = items[i]; // 回転後の寸法を元の入力から求める
        result.placements[i] = { // 構造体のフィールド順に設定する
            x, // 左上のx座標
            y, // 左上のy座標
            rotation ? item.height : item.width, // 配置幅
            rotation ? item.width : item.height, // 配置高さ
            bin, // 所属ビン
            rotation != 0 // 90度回転したか
        };
    }

    // 小さく安全な数値を前提に集計する。幾何の検査は別途行う。
    recompute_rectangle_packing_summary(items, result);

    return result; // 配置と集計がそろった結果を返す
}

int main() {
    int n, require_all, allow_missing;
    long long width, height;

    if (!(std::cin >> n >> width >> height >> require_all >> allow_missing)) {
        return 0;
    }

    // 必須と任意の違いも入力する
    const auto items = read_items(n);
    // 安全な小さい数値だけを前提に集計する。任意の破損データ用ではない
    const auto result = read_initial(items);

    // 指定した条件で配置を検査する
    std::string error;
    // 0以下の境界軸は上限なしとして検査する
    const bool valid = validate_rectangle_packing(items,
        result,
        width,
        height,
        // require_all=trueならallow_missingより優先される
        require_all != 0,
        &error,
        allow_missing != 0);

    // 検査結果と簡潔なデバッグ情報を出力する
    // 成否は戻り値で判定する
    std::cout << "valid " << valid << '\n';

    // 成功時に過去のerrorが消される保証はない
    if (!valid) {
        std::cout << "reason " << error << '\n';
    }

    // 集計と先頭20配置だけを表示。未配置と省略件数も表示する
    std::cerr << result << '\n';

    // 出力処理自体は検査や再集計を行わない
    return 0;
}
```

**入力例**

```text
3 6 4 0 1
2 2 3 0 1
2 2 5 0 1
2 2 9 0 0
0 0 0 0
-1 0 0 0
-1 0 0 0
```

**標準出力の例**

```text
valid 1
```

**標準エラー出力の例**

```text
rectangle_pack_result{feasible=false, placed_count=1, missing_required_count=1, bin_count=1, used_width=2, used_height=2, placed_profit=3, placed_area=4, placements(3)=[
  [0] {bin=0, x=0, y=0, width=2, height=2, rotated=false}
  [1] unplaced
  [2] unplaced
]}
```

stdoutのvalidは検査の成否。stderrのfeasibleは必須欠落数だけによる判定なので、validと異なる場合がある。デバッグ表示は最大20配置。

### 3.21 例21：小数2桁の寸法を正確に整数化する

センチメートルの小数2桁など、必要な精度が決まった寸法を扱う場面。小数を浮動小数で丸めず、文字列から100倍の整数へ変換する。

**具体的に解く問題**

寸法と座標を0.01単位の格子にそろえ、固定領域へ全配置する。出力時に100で割って小数2桁へ戻す。

**入力**

1行目はN W H。続くN行は幅 高さ 回転許可。寸法はすべて0.01～1000.00の正の数で、必ず小数点以下2桁を持つ文字列。Nは0～50。

**実行できるコード：example21_decimal_units.cpp**

```cpp
// このヘッダーだけで必要な型・関数を使える
#include "rectangle_packing_v11.hpp"

// 解く問題: 寸法と座標を0.01単位の格子にそろえ、固定領域へ全配置する。
// 出力時に100で割って小数2桁へ戻す。
// 標準入力: 1行目はN W H。続くN行は幅 高さ 回転許可。
// 寸法はすべて0.01～1000.00の正の数で、必ず小数点以下2桁を持つ文字列。Nは0～50。
// 数値前提: 寸法文字列は正の小数2桁で0.01～1000.00、Nは0～50。
// 記載の入力形式・ID範囲・初期配置条件を満たす入力を与える。
// 全配置の成功は戻り値で確認する。

// サンプル側の補助関数。必須の未配置を許す中間解も、座標・重なり・集計は検査する
// 同じID順の入力
void check_geometry(
    const std::vector<rectangle_pack_item>& items, // 入力と同じID順の矩形
    const rectangle_pack_result& result, // 検査する配置と集計
    long long width, // 0なら幅の上限なし
    long long height // 0なら高さの上限なし
) {
    std::string error;

    // 必須がまだ欠けている中間解も、座標・重なり・集計は検査する。
    if (!validate_rectangle_packing(
            items, // 元の入力
            result, // 検査対象
            width, // 幅の上限
            height, // 高さの上限
            false, // 任意矩形まで含めた全配置は要求しない
            &error, // 失敗理由の書き込み先
            true)) { // 必須矩形の未配置も許す
        // 成否は戻り値で判定する。成功時のerrorは判定に使わない。
        std::cerr << error << '\n';
        std::exit(1);
    }
}

// 入力仕様は正の「整数部.小数2桁」。例: "2.50"を250へ変換する
long long units(const std::string& text) {
    const std::size_t dot = text.find('.');

    return std::stoll(text.substr(0, dot)) * 100 + std::stoll(text.substr(dot + 1));
}

// 非負の整数単位を小数2桁へ戻す。例: 250を"2.50"と表示する
void print_units(long long value) {
    std::cout << value / 100 << '.' << std::setfill('0') << std::setw(2) << value % 100
              << std::setfill(' ');
}

int main() {
    int n;
    std::string width_text, height_text;

    if (!(std::cin >> n >> width_text >> height_text)) {
        return 0;
    }

    // 寸法を100倍の整数へ変換する
    const long long width = units(width_text), height = units(height_text);
    // 寸法は100倍の整数単位で渡す
    std::vector<rectangle_pack_item> items(static_cast<std::size_t>(n));

    // 全矩形で同じ単位を使う
    for (auto& item : items) {
        std::string w, h;
        int rotation;
        std::cin >> w >> h >> rotation;
        // 入力幅を文字列から正確に整数化する
        item.width = units(w);
        // 入力高さも同じ倍率にする
        item.height = units(h);
        // 回転許可は通常どおり設定する
        item.rotatable = rotation != 0;
        // 全矩形を置く
        item.required = true;
        // 選別しないので価値を1にする
        item.profit = 1;
    }

    // 整数の問題として配置を求める
    // 座標も100倍の整数で返る
    const auto result = pack_rectangles_fixed_bin(items, width, height);
    // 検査も整数化した単位で行う
    check_geometry(items, result, width, height);

    // 座標を小数2桁の表示へ戻す
    // 必須が全部配置できたか
    std::cout << "feasible " << result.feasible() << '\n';

    // 入力ID順に出力する
    for (std::size_t i = 0; i < result.placements.size(); ++i) {
        // i番の整数座標・回転後寸法
        const auto& p = result.placements[i];

        // 未配置の座標は読まない
        if (!p.placed()) {
            std::cout << i << " unplaced\n";
            continue;
        }

        // 所属ビン
        std::cout << i << ' ' << p.bin << ' ';
        // 座標を元の単位へ戻す
        print_units(p.x);
        std::cout << ' ';
        print_units(p.y);
        std::cout << ' ';
        // 配置幅・高さも元の単位へ戻す
        print_units(p.width);
        std::cout << ' ';
        print_units(p.height);
        // 回転フラグは整数化と無関係
        std::cout << ' ' << p.rotated << '\n';
    }

    return 0;
}
```

**入力例**

```text
2 6.00 4.00
2.50 1.25 0
1.75 2.00 1
```

**標準出力の例**

```text
feasible 1
0 0 0.00 0.00 2.50 1.25 0
1 0 2.50 0.00 2.00 1.75 1
```

この例だけ、座標・寸法の出力は元の小数2桁。整数化後の面積を元の単位へ戻す倍率は10000である。

### 3.22 検証とAPIの網羅

掲載した21プログラムを1ファイルずつ独立にコンパイルし、入出力例と境界・追加入力の計92入力を実行した。ASan/UBSanを有効にした結合検証でも同じ92入力が通過した。ライブラリのvalidatorに加え、独立した検査コードで、配置寸法・回転・枠内条件・非重複・集計値・固定部分・隙間・独自スコアを確認している。

検証環境はGCC 13.3、C++20。警告オプションは`-Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined -Werror`で、掲載サンプルのコンパイル警告は0件。対象のGCC 12.2はこの環境にないため未実行。ASan/UBSan実行時には環境の`/proc`制限によりLeakSanitizerのみ無効にした。

同梱の`verify_examples.py`はPython 3の標準ライブラリだけで動く。通常の独立コンパイルと実行は次のコマンドで再現できる。

```bash
python3 verify_examples.py
```

ASan/UBSanでの検証は次のとおり。こちらは各mainを独立した名前空間へまとめて1つの検証用実行ファイルを作り、例ごとに別プロセスで実行する。

```bash
python3 verify_examples.py --sanitize
```

いずれも生成物は`build/`へ出力する。例19は実時間の締切を使うため出力文字列の完全一致を要求せず、幾何と集計を確認する。

| 公開API・設定 | 使用する主な例 |
|---|---|
| `rectangle_pack_item`の全5フィールド | 01・02・18 |
| `rectangle_pack_placement`の全6フィールド、`placed()` | 09・13・16・17・18・20 |
| `rectangle_pack_result`の全集計・配置、`feasible()` | 01ほかの出力関数 |
| `rectangle_pack_options`の全5フィールド | 02・03・10・19 |
| 外接目的の全3列挙値・重み・`evaluate()` | 05・06・07・12 |
| `pack_rectangles_fixed_bin` | 01・02・08・19・21 |
| `improve_rectangles_fixed_bin` | 09・19 |
| `repack_rectangles_fixed_bin` | 14 |
| `repair_rectangles_fixed_bin` | 13・15・17・18 |
| `pack_rectangles_strip` | 03・10 |
| `improve_rectangles_strip` | 10 |
| `pack_rectangles_multiple_bins` | 04 |
| `improve_rectangles_multiple_bins` | 11 |
| `pack_rectangles_bounding_box` | 05・06・07・12 |
| `improve_rectangles_bounding_box` | 12 |
| `recompute_rectangle_packing_summary` | 09・11・13・15・16・17・18・20 |
| `validate_rectangle_packing`の全引数 | 20。ほかの例でも補助関数や直接呼出で使用 |
| `operator<<` | 20 |
