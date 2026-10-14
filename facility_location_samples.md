# facility_location サンプルコード集

対象ヘッダは **facility_location_v13.hpp** です。C++20、GCC、標準ライブラリで使います。本書は、解く問題、公開API、独立して実行できる22個の利用例の順に説明します。

各例のコード全体を別々の example_XX.cpp として保存し、ヘッダを同じディレクトリへ置いてください。標準入力へ「入力例」を与えると、「出力例」の形式で結果を得られます。他の例のコードは必要ありません。ライブラリ側の自己テスト用mainは、include時に自動で除外されます。

次は例01のビルド・実行方法です。

~~~bash
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined example_01.cpp -o example_01
./example_01 < example_01.in
~~~

掲載したプログラムはGCC 13.3で、警告をエラーとして扱う設定でも単独ビルド・実行を確認しています。GCC 12.2での実行確認は行っていません。ヘッダ以外の外部ライブラリは必要ありません。例の入力は、各例に記載した前提を満たすものとします。

## 1. 何を解くライブラリか？

### 1.1 候補からK個を選び、各需要を最も安い候補で担当する

建てられる倉庫が20か所あり、配送先が100か所あるとします。倉庫を3か所選び、各配送先には、その3倉庫のうち最も安く配送できる倉庫から届けます。このときの配送費合計を小さくするのが、基本の問題です。

「候補施設」は選ぶ側、「需要点」はサービスを受ける側です。候補施設数と需要点数は違って構いません。施設には倉庫だけでなく、代表データ、センサー、採用する方式なども当てはめられます。需要にも、配送先だけでなく、説明したいデータや被覆したい要素を当てはめられます。

次の条件を指定できます。

- 重要な需要ほど費用に大きい重みを掛ける。
- 外注や現状維持の方が安ければ、新しい施設を使わない。その代替費用をfallbackと呼ぶ。
- 施設を選ぶこと自体の設置費を足す。負の値なら、選ぶことで得られる報酬になる。
- 必ず残す施設を決める。
- 追加で選べる候補を限定する。

選ぶ施設は**ちょうどK個**です。必須施設もこのK個に含みます。fallbackは数えません。選んだ施設を使う需要が0件でも、その施設はK個の1つです。

### 1.2 目的関数

選んだ施設IDの集合を$S$とします。目的は、次の総費用$J(S)$をできるだけ小さくすることです。

$$J(S)=\sum_ {i=0}^{Q-1}w_ {i}\min\left(b_ {i},\min_ {f\in S}c_ {f,i}\right)+\sum_ {f\in S}a_ {f}$$

選択集合には次の条件があります。

$$\lvert S\rvert=K,\quad F\subseteq S,\quad S\subseteq F\cup A$$

記号を順に読むと、次の意味です。「集合」は番号の集まり、「集合の大きさ」はその個数です。$\sum$は指定した範囲の値を全て足す記号、$\min$は最も小さい値を選ぶ記号です。

|記号|意味|コードとの対応・範囲|
|---|---|---|
|$Q$|需要点数|`demand_count`。1以上|
|$M$|候補施設数|`candidate_count`。1以上|
|$i$|需要点の番号|0以上Q未満|
|$f$|候補施設の番号|0以上M未満|
|$S$|実際に選んだ施設IDの集合|`result.facilities`。重複のないK個|
|$K$|選ぶ施設の総数|`facility_count`、または初期施設配列の長さ。1以上M以下|
|$c_ {f,i}$|施設fから需要iを担当する費用|`problem.cost[f * Q + i]`。有限・非負|
|$w_ {i}$|需要iの重要度・数量|`problem.demand_weight[i]`。有限・非負。省略時は1|
|$b_ {i}$|需要iを外注・現状維持で担当する費用|`problem.fallback_cost[i]`。有限・非負|
|$a_ {f}$|施設fを選ぶだけで掛かる費用|`problem.facility_cost[f]`。有限。正・0・負を指定でき、省略時は0|
|$F$|必ず選ぶ施設の集合|`restrictions.fixed_facilities`。省略時は空|
|$A$|追加選択を許す候補の集合|`restrictions.candidate_facilities`。空配列なら全候補を許可|
|$J(S)$|サービス費用と施設費用を足した総費用|`result.cost`。小さい方がよく、負になることもある|

$F\subseteq S$は「必須施設を全て選ぶ」、$S\subseteq F\cup A$は「必須施設か許可候補にある施設だけを選ぶ」という条件です。実行可能な入力には、必須施設数がK以下で、必須施設と許可候補の和集合にK個以上あることが必要です。

式の内側では、選んだ施設の中で需要iを最も安く担当できる費用を求めます。fallbackがあれば、それとも比較します。その安い方に需要の重みを掛け、全需要について足します。最後に、選択施設の単項費用を1施設につき1回足します。需要の重みは施設の単項費用には掛けません。

fallbackを指定しない場合、式の上では$b_ {i}=+\infty$と考えます。実際の入力に無限大を入れるという意味ではありません。`fallback_cost`を空にします。

例えば需要が2件あり、選んだ施設からの費用が4と2、重みが2と3なら、サービス費用は$2\times4+3\times2=14$です。外注費がどちらも3なら、最初の需要だけ外注を使って$2\times3+3\times2=12$です。施設の単項費用が-1なら、総費用は11です。

### 1.3 返る解と対象外の条件

返るのは、総費用、選択施設のID、需要ごとの担当施設ID、探索統計です。各需要の担当先は選択施設とfallbackの最小費用で決まります。fallbackの方が厳密に安ければ担当IDは-1になります。同額なら実施設を優先し、実施設同士が同額なら小さいIDを優先します。重み0の需要にも担当先を返します。

施設ごとの人数・配送量の容量、需要の分割、施設同士の同時選択禁止、選択施設の連結性などは扱いません。これらは需要間・施設間の条件を結び付けるためです。需要ごとの独立した費用の和で表せるかが、利用可否の判断点です。

また、最大配送距離を最小化する問題、自由な連続座標へ施設を置く問題、巡回路を作る問題はそのまま対象になりません。グラフを使う場合も、グラフを直接渡すのではなく、必要な費用表を利用側で作ります。

大域最適性を証明する結果は返りません。返却された集合と割当てを、利用する問題の出力形式に合わせて取り出します。

## 2. APIリファレンス

### 2.1 公開APIの全体

公開名は全てグローバル名前空間にあります。ライブラリ独自の公開列挙型は**ありません**。-1、0、空配列、最大値などに特別な意味がある設定は、以下で個別に説明します。

|公開される型・関数|役割|詳しい節|
|---|---|---|
|`facility_location_problem<Cost, Calc>`|費用表・重み・代替費用・施設費用|2.2|
|`facility_location_restrictions`|必須施設と許可候補|2.3|
|`facility_location_options`|`seed`、時間・探索量の設定|2.4|
|`facility_location_options::clock`|`std::chrono::steady_clock`の別名|2.4|
|`facility_location_result<Calc>`|解を保持する結果型|2.5|
|`facility_location_statistics`|探索回数と停止状況|2.5|
|`solve_k_median`：2形式|新しい施設集合を構築して解く|2.6|
|`improve_k_median`：2形式|既存の施設集合から改善する|2.7|
|`evaluate_k_median`|指定集合の目的値を計算する|2.8|
|`facility_location_workspace<Cost, Calc>`|同じ問題を繰り返し解くための作業領域|2.9|
|`workspace`のコンストラクタ・`solve`・`improve`|領域構築、新規解構築、既存解改善|2.9|
|結果型・統計型の`operator<<`：2形式|`cout`、`cerr`などへデバッグ出力する|2.10|

問題・制約・設定・結果・統計の構造体は、フィールドを直接読み書きする型です。専用のsetterや登録メソッドはありません。初期状態の`problem`や`result`を、そのまま有効な問題や解として使えるわけではありません。

### 2.2 `facility_location_problem<Cost, Calc = long long>`

`Cost`は費用表とfallbackの要素型、`Calc`は重み・施設費用・合計・差分・返却費用の型です。`Cost`に既定型はありません。`Calc`を省略すると`long long`になります。

|テンプレート引数|選び方|注意|
|---|---|---|
|`Cost`|`int`、`long long`、`double`など、費用表の値を表せる算術型|サービス費用は非負・有限。`std::numeric_limits`が使える型|
|`Calc`|符号付き整数、または浮動小数点型|負の改善差分を表すため符号なし型は使わない|
|`Calc`省略時|`long long`|`Cost`が`double`でも自動で`double`にはならない|
|大きい整数合計|GCCの`__int128_t`も使用可能|入力と単独の値の出力は標準iostreamが直接対応しない。例22を参照|

|フィールド|型|既定値|指定する内容・有効範囲|
|---|---|---|---|
|`demand_count`|`int`|0|Q。利用前に1以上へ設定|
|`candidate_count`|`int`|0|M。利用前に1以上へ設定|
|`cost`|`std::vector<Cost>`|空|必須。正確にM×Q要素。全要素は有限・非負|
|`demand_weight`|`std::vector<Calc>`|空|空なら全需要の重み1。指定するならQ要素、有限・非負|
|`fallback_cost`|`std::vector<Cost>`|空|空なら代替手段なし。指定するならQ要素、有限・非負|
|`facility_cost`|`std::vector<Calc>`|空|空なら全施設0。指定するならM要素、有限。負値も可|

`cost`は**候補施設を行、需要点を列**にした表です。施設f・需要iの添字は、C++では`static_cast<std::size_t>(f) * demand_count + i`です。施設0の全需要費用、施設1の全需要費用、という順で並べます。行列を逆向きに詰めても要素数は同じなので、入力時に向きを確認してください。

単項費用とは、他の選択施設や担当人数に依存せず、その施設を選ぶだけで1回発生する費用です。利用者数に比例する料金を`facility_cost`へ入れると、意図した目的関数になりません。

費用表だけでO(MQ)のメモリが必要です。例えばM=Q=10000で`Cost`が4バイトなら、費用表は約400MBです。さらに探索用の作業配列が必要になります。

### 2.3 `facility_location_restrictions`

|フィールド|型|既定値|意味・入力条件|
|---|---|---|---|
|`fixed_facilities`|`std::vector<int>`|空|必ず選び、改善中にも削除しない施設ID。全て0以上M未満、配列内に重複なし|
|`candidate_facilities`|`std::vector<int>`|空|追加選択を許すID。全て0以上M未満、配列内に重複なし。空なら全候補を許可|

`fixed_facilities`と`candidate_facilities`の**間**で同じIDが現れるのは構いません。固定施設は候補配列の外にあっても選べます。内部では固定施設を可動候補から除いて扱います。

必要な条件は、固定施設数がK以下で、固定施設と許可候補を合わせてK個以上の異なるIDがあることです。初期集合を渡す場合、その集合もこの条件を満たす必要があります。

候補配列を空にしても「追加を禁止する」設定にはなりません。既存のK施設を全て動かしたくない場合は、そのK施設を`fixed_facilities`へ指定します。固定は施設の選択に対する条件であり、「この需要は固定施設を利用しなければならない」という割当制約ではありません。

### 2.4 `facility_location_options`と`clock`

`clock`は`std::chrono::steady_clock`の別名です。`deadline`にはこの時計の`time_point`を使います。経過ミリ秒の整数や`system_clock`の時刻を直接入れるものではありません。

|フィールド|型|既定値|値の意味・範囲・選び方|
|---|---|---|---|
|`deadline`|`clock::time_point`|`clock::time_point::max()`|最大値は時刻停止なし。それ以外は絶対終了時刻。外側に締切があれば同じ時刻を渡す|
|`seed`|`std::uint64_t`|1|0から型の最大値まで。探索順を変えたいときに変える|
|`initial_trials`|`int`|4|1以上。初期集合を作る試行数。増やすと開始点を増やせるが、構築時間も増える|
|`max_perturbations`|`int`|-1|-1以上。下表を参照|
|`max_candidate_evaluations`|`std::uint64_t`|`std::numeric_limits<std::uint64_t>::max()`|最大値は評価回数の制限なし。0なら追加候補の交換評価をしない。正なら1回の呼出し全体の上限|

`max_perturbations`は、局所的に改善した解を少し動かして再び改善を試す回数を制限します。この「揺さぶり」をILSと呼びます。

|値|意味|
|---|---|
|-1、`deadline`なし|揺さぶりを最大32回行う|
|-1、有限の`deadline`あり|時刻や評価上限などの停止条件まで繰り返す|
|0|揺さぶりを行わない。初期構築後や`improve`開始後の1施設交換による改善は行う|
|1以上|指定回数まで揺さぶる。時間・評価上限が先に来ればそこで止まる|

通常は既定値から始め、短い部分修復なら`initial_trials`や`max_perturbations`を小さくします。0と-1は意味が大きく違います。`initial_trials`は`improve`では初期構築に使われませんが、入力条件として1以上が必要です。

候補評価1回とは、「未選択の追加施設1つについて、削除する施設を比較する評価」です。交換の組合せ1個でも、需要点1件でもありません。上限は初期試行と揺さぶりを通じた合計です。ただし初期構築、内部状態の再構築、最後の割当て作成は、この回数に含みません。上限到達後も、残りの初期構築試行が行われる場合があります。

初期構築だけを1回取り出すなら、`initial_trials`=1、`max_perturbations`=0、`max_candidate_evaluations`=0を合わせて使います。例19・22がこの設定です。

`deadline`は厳密な実行時間上限ではありません。途中で中断しない走査や結果作成があります。過去の`deadline`や0msを指定しても、初期集合の評価・実行可能な施設集合の確保・結果返却に必要な処理は行われます。

同じ入力・設定・`seed`・実行環境で、時刻に依存しない条件なら再現比較できます。時刻制限付きでは処理速度や負荷により結果が変わりえます。標準ライブラリやコンパイラをまたいだ探索順の完全一致は保証しません。

### 2.5 `facility_location_result<Calc>`と`facility_location_statistics`

`result`には`Calc`の既定型はありません。通常はautoで関数の戻り値を受け取ります。

|結果フィールド|型|既定値|探索から返ったときの意味|
|---|---|---|---|
|`cost`|`Calc`|`Calc{}`|選択集合の総費用。fallback・重み・施設費用を含む|
|`facilities`|`std::vector<int>`|空|選択したK施設の元の候補ID。昇順・重複なし|
|`assignment`|`std::vector<int>`|空|Q需要の担当施設ID。需要の入力順。fallbackなら-1|
|`statistics`|`facility_location_statistics`|全カウンタ0、全フラグfalse|今回の1呼出し全体の統計|

`assignment`に入るのは`facilities`配列の添字ではありません。選んだ施設が2と7なら、割当てにも2や7が入ります。-1を候補配列の添字にしないでください。`facilities`は昇順になるため、初期集合の並び順を使う処理は利用側で対応を取り直します。

|統計フィールド|型|既定値|意味|
|---|---|---|---|
|`candidate_evaluations`|`std::uint64_t`|0|追加候補を交換評価した回数|
|`accepted_swaps`|`std::uint64_t`|0|改善として受理した1施設交換の回数。揺さぶり自体の交換は含まない|
|`completed_initial_trials`|`int`|0|初期構築とその局所改善の呼出しが戻った試行数。途中で上限に達した試行も数える。`improve`では0|
|`completed_perturbations`|`int`|0|揺さぶりとその局所改善の呼出しが戻った回数。局所最適到達を意味しない|
|`deadline_reached`|`bool`|false|内部の時刻確認で締切到達を観測したか。返却時刻の事後判定ではない|
|`evaluation_limit_reached`|`bool`|false|有限の評価上限に到達したか。上限0なら評価0でもtrue|

統計は、最後に返る最良集合を作った試行だけの値ではありません。複数の試行・揺さぶりを含む、1回の呼出し全体の値です。`workspace`でも呼出しごとに初期化されます。

既定構築した`result`の`cost`=0は、「費用0の有効な解が見つかった」という意味ではありません。最良解を外側で保持する場合は、例15・16のように解の有無を別に管理します。

### 2.6 `solve_k_median`：新規に解く

次の2形式があります。`Cost`と`Calc`は`problem`から推論されます。

|呼出し形式|戻り値|
|---|---|
|`solve_k_median(problem, facility_count, options = {})`|`facility_location_result<Calc>`|
|`solve_k_median(problem, facility_count, restrictions, options = {})`|`facility_location_result<Calc>`|

|引数|型|省略・既定値|意味・前提|
|---|---|---|---|
|`problem`|`const facility_location_problem<Cost, Calc>&`|必須|2.2の条件を満たす問題。入力内容を変更しない|
|`facility_count`|`int`|必須|選ぶK個。1以上M以下で、固定・候補条件も実行可能にする|
|`restrictions`|`const facility_location_restrictions&`|制約なしの形式を使えば固定なし・全候補許可|2.3の条件。必須施設もKに含む|
|`options`|`facility_location_options`|{}。2.4の既定値|呼出し単位の探索設定。値渡し|

初期施設集合を利用側で作る必要はありません。返却された`facilities`を外側の問題へ戻して使えます。最適性・近似比の証明や、入力誤りを表すエラー結果は返しません。

例01は設定省略、例02は`options`指定、例11は`restrictions`指定です。

### 2.7 `improve_k_median`：既存集合から改善する

|呼出し形式|戻り値|
|---|---|
|`improve_k_median(problem, initial_facilities, options = {})`|`facility_location_result<Calc>`|
|`improve_k_median(problem, initial_facilities, restrictions, options = {})`|`facility_location_result<Calc>`|

|引数|型|省略・既定値|意味・前提|
|---|---|---|---|
|`problem`|`const facility_location_problem<Cost, Calc>&`|必須|現在の費用表・重みなど|
|`initial_facilities`|`std::vector<int>`|必須|初期施設の元ID。空不可、重複不可、全IDは0以上M未満|
|`restrictions`|`const facility_location_restrictions&`|制約なしの形式では固定なし・全候補許可|初期集合にも適用される。必須施設を全て含み、その他は許可候補内にする|
|`options`|`facility_location_options`|{}|2.4と同じ設定。`initial_trials`も1以上にする|

施設数は`initial_facilities.size()`で決まります。別のKを同時に渡す形式はありません。前の割当てや探索途中状態を引き継ぐ引数もありません。

自由関数の`initial_facilities`は値渡しです。通常の変数を渡すとコピーされ、元の配列は変わりません。不要になった配列なら`std::move`で渡す使い方もできます。返る集合は昇順です。

有効な整数計算の範囲では初期集合より悪い目的値を返しません。小数では加算順の丸め差にも注意し、初期集合を昇順にそろえて評価する、尺度に応じた許容誤差で比較するなどの対応を取ります。

例12は既存解改善、例13は制約付きの部分修復、例17は新しい問題に前の施設集合を渡す使い方です。

### 2.8 `evaluate_k_median`：任意の施設集合を評価する

形式は`evaluate_k_median(problem, facilities)`で、戻り値は`Calc`です。探索、選択集合の変更、ソート、割当て配列の作成は行いません。

|引数|型|省略・既定値|意味・前提|
|---|---|---|---|
|`problem`|`const facility_location_problem<Cost, Calc>&`|必須|費用表など。2.2のサイズと数値条件を満たす|
|`facilities`|`const std::vector<int>&`|必須|評価する施設ID集合。空不可、有効なID、重複なしを指定|

選んだ施設数をLとすると、計算量はO(QL+L)です。`restrictions`や`options`は受け取りません。固定・候補条件のチェックが必要なら呼出し前に利用側で行います。初期集合の評価や、小さい問題の全列挙に使えます。

fallbackだけを使った空集合の評価はできません。その場合の基準値は、重み×fallback費用の合計を利用側で計算します。施設単項費用の加算順は渡した施設順です。小数で`result.cost`と比較する場合、`result.facilities`の昇順のまま渡すと加算順もそろいます。

例12・21で使用します。

### 2.9 `facility_location_workspace<Cost, Calc = long long>`

同じ`problem`、K、`restrictions`で繰り返し解くときに、内部の作業配列などを再利用します。`problem_type`や`result_type`という内部の別名は公開APIではありません。

|公開メソッド|引数・既定値|戻り値・動作|
|---|---|---|
|コンストラクタ(`problem`, `facility_count`, `restrictions` = {})|`problem`はconst参照、`facility_count`は`int`、`restrictions`はconst参照。省略時は固定なし・全候補許可|作業領域を構築する。問題・K・制約の前提は`solve`と同じ|
|`solve(options = {})`|`facility_location_options`を値渡し。省略時は既定設定|`facility_location_result<Calc>`。新規構築から探索する|
|`improve(initial_facilities, options = {})`|`initial_facilities`は`const std::vector<int>&`で必須。`options`は値渡しで省略可|`facility_location_result<Calc>`。保存したKと同じ長さの初期集合から改善する|

次の条件を守って使います。

- `problem`は参照されるため、`workspace`より長く生存させる。
- `workspace`の生存中は`problem`の内容と所在を変えない。費用1要素や重み1個の変更も含む。
- Kや制約を変更するsetterはない。変更時は`workspace`を新しく作るか自由関数を使う。
- `restrictions`は構築時に内部へ取り込まれる。元の`restrictions`を書き換えても、その`workspace`の条件は変わらない。
- 次の`solve`は新規構築から始まる。前の解を初期値にする場合は`improve`へ明示して渡す。
- 呼出しごとに`options`、乱数状態、探索統計などは初期化される。前回の探索の一時停止・再開ではない。

`workspace`はコピーできません。単一スレッドで使う前提です。主要な配列の再利用はできますが、結果や初期構築の一時配列を含め、全てのメモリ確保がなくなるわけではありません。

例16は同じ問題の反復、例18は既定の制約での構築です。入力が変わる例17では自由関数を使います。

### 2.10 `operator<<`：結果と統計を出力する

|対象|形式|戻り値|
|---|---|---|
|結果|`std::ostream& operator<<(std::ostream& os, const facility_location_result<Calc>& value)`|渡したosへの参照|
|統計|`std::ostream& operator<<(std::ostream& os, const facility_location_statistics& value)`|渡したosへの参照|

`cout` << `result`、`cerr` << `result.statistics`のように使います。`ostringstream`にも対応します。結果や統計を変更せず、改行は自動で付けません。

結果は費用、選択施設、割当て、統計を1行にまとめます。各配列は先頭20要素まで表示し、sizeは総要素数、... (+N)は省略した要素数を表します。空配列は[]です。全要素が必要な提出用出力では、例01などのように配列を自分で走査してください。

真偽値は常にtrue／falseです。通常の数値は出力先の書式に従います。書式フラグ、精度、埋め文字は変更しません。`__int128_t`の費用は、出力先の基数設定によらず十進数で表示します。結果全体の出力が対応するだけで、`__int128_t`単体の標準ostream演算子を追加するものではありません。例22で使用します。

### 2.11 共通の制約と誤解しやすい点

|項目|対応・注意|
|---|---|
|不正入力|全エラーを回復可能な形で報告するAPIではない。`assert`の有無に頼らず、利用側で前提を満たす|
|Q=0、M=0、K=0|探索対象外。空需要・施設なしの処理は利用側で分ける|
|型の上限|最終和だけでなく、重みとの積、差分、削除補正、二乗を使う抽選用計算も範囲内にする|
|`Cost=double`で`Calc`省略|内部で整数へ変換される。小数計算なら`Calc`も`double`などへ指定する|
|NaN・無限大|入力しない。到達不能は例04のようにモデル上正しい有限の表現へ変換する|
|整数の最小負値を施設費用へ入れる|内部で負号反転する場面があるため避ける|
|小数の微小差|差分だけでなく交換後の総費用でも改善を確認する。epsilonの公開設定はない。丸めで改善を見逃す可能性は残る|
|ゼロ重みの需要|目的値には寄与しないが、割当ては通常どおり作られる|
|同額解|担当先の同額処理は定義されるが、同じ費用の施設集合から辞書順最小を返す保証はない|
|`candidate_facilities`が空|全候補を許す意味。候補を0個にする指定ではない|
|裸の{}を第3引数へ渡す|`solve`/`improve`では`options`と`restrictions`のどちらか曖昧になりうる。型付きの変数を渡す|
|時間上限・評価上限|最適性を保証する指定ではない。`deadline`も厳密な所要時間上限ではない|
|結果のデバッグ出力|配列は省略される。全件の外部出力として使うなら自分で列挙する|

### 2.12 公開APIとサンプルの対応

|API・主要設定|主な例|
|---|---|
|`solve_k_median`、制約なし|01〜10、14、15、19、20、22|
|`solve_k_median`、制約付き|11|
|`improve_k_median`、制約なし|12、17、19|
|`improve_k_median`、制約付き|13|
|`evaluate_k_median`|12、21|
|`workspace`の構築、`solve`、`improve`|16、18|
|`demand_weight`|02、07、17、20、21、22|
|`fallback_cost`|04、09、14、20、21|
|`facility_cost`|10、14、15、20、21|
|`fixed_facilities`、`candidate_facilities`|11、13。16でも固定施設を使用|
|`seed`、`initial_trials`、`max_perturbations`|02、12、13、16、17、19、22|
|`deadline`、`options`::`clock`|18|
|`max_candidate_evaluations`、停止統計|19、22|
|結果・統計の`operator<<`|18、19、22|

## 3. ユースケース

以下は全て、ヘッダとその例だけでコンパイルできる独立したプログラムです。入力IDは0始まりです。標準の`cost`／`facilities`／`assignment`という出力では、順に総費用、選択候補ID、需要ごとの担当候補IDを表します。例04はグラフ頂点IDへ変換し、例21は割当てを作らず、例22はデバッグ形式で表示します。

入力例は小さいため、別実装の全列挙とも比較して目的値を確認しています。一般の入力について、全ての例が大域最適を保証するわけではありません。複数の集合が同額なら、掲載例と異なる施設集合になることもあります。

時間制限を使う例18では標準エラーに出す統計が実行ごとに変わります。出力例は標準出力だけを掲載します。各コード内のライブラリ型・設定・呼出し・結果参照には、その行の意味と必要な注意をコメントしています。


|例|用途|
|---|---|
|01|費用表から配送拠点を選ぶ|
|02|配送量の多い地点を重視する|
|03|座標からマンハッタン距離の拠点配置を作る|
|04|一方通行の道路網で配送し、届かない場所は外注する|
|05|実在するデータから代表を選ぶk-medoids|
|06|候補コードブックから二乗誤差の小さい代表ベクトルを選ぶ|
|07|K個の集合で重要な要素をできるだけ覆う|
|08|代表サービスを選び、利用者の満足度を最大化する|
|09|安い外注サービスを残しながら新設施設を選ぶ|
|10|設置費や選択報酬も含めて施設を選ぶ|
|11|必須の既存施設を残し、許可された候補だけから選ぶ|
|12|手作りした初期配置を改善する|
|13|大きな配置の一部だけを修復する|
|14|既存網へR施設を追加する価値を求める|
|15|施設数そのものを外側で比較する|
|16|同じ問題をworkspaceで繰り返し解く|
|17|ターンごとに需要や費用が変わる配置を再計算する|
|18|外側の処理と残り時間を共有する|
|19|評価回数をそろえて探索量を管理する|
|20|小数・非対称の損失を扱う|
|21|候補が少ない問題を全列挙で厳密に評価する|
|22|64ビットを超える費用と長い結果をデバッグ出力する|

### 3.1 例01：費用表から配送拠点を選ぶ

配送先と候補地が別々に与えられ、どの候補地から配送しても費用が分かっている場面です。

**具体的な問題**

M候補からちょうどK施設を選び、Q需要点の最小配送費の合計を小さくします。全需要の重みは1です。

**入力**

1行目: M Q K。続くM行: 各候補からQ需要点への非負整数費用。

費用表は長方形でも構いません。需要数と候補数を同じにする必要はありません。

**実行コード：example_01.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    // 制約・options省略。固定施設なし、既定の探索設定で解く
    const auto result = solve_k_median(p, k);

    print_result(result); // cost、facilities、assignmentを利用する

    return 0;
}
```

**入力例**

```text
4 4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
```

**出力例（標準出力）**

```text
cost 3
facilities 0 2
assignment 0 0 2 2
```

### 3.2 例02：配送量の多い地点を重視する

同じ距離でも、荷物が多い地点の遠回りを強く避けたい場面です。重みは施設の容量ではありません。

**具体的な問題**

需要iの配送費に荷物量w[i]を掛け、その総和を小さくします。1施設が何需要を担当しても構いません。

**入力**

M Q K、M行の費用表、最後にQ個の非負整数重み。

**実行コード：example_02.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<int> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    p.demand_weight.resize(q); // 空なら全て1。この例では需要ごとの配送量を指定する
    for (auto& weight : p.demand_weight) { // 0は目的値に寄与しない需要
        std::cin >> weight;
    }

    facility_location_options options; // 指定しない設定は既定値のまま使う
    options.seed = 7; // 探索順を決めるseed。0も指定できる

    const auto result = solve_k_median(p, k, options); // 重み付きの総費用を小さくする

    print_result(result); // 重み0の需要にも担当施設IDは返る

    return 0;
}
```

**入力例**

```text
3 3 1
0 4 8
4 0 4
8 4 0
5 0 1
```

**出力例（標準出力）**

```text
cost 8
facilities 0
assignment 0 0 0
```

### 3.3 例03：座標からマンハッタン距離の拠点配置を作る

縦横の移動距離で配送費を近似する場面です。候補地と配送先は別の座標列で受け取ります。

**具体的な問題**

候補(fx,fy)から需要(ix,iy)への費用を、横方向の差の絶対値と縦方向の差の絶対値の和とします。

**入力**

M Q K、M行の候補座標x y、Q行の需要座標x y。座標は整数。

**実行コード：example_03.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }

    std::vector<std::pair<long long, long long>> sites(m), demands(q);
    for (auto& [x, y] : sites) {
        std::cin >> x >> y;
    }
    for (auto& [x, y] : demands) {
        std::cin >> x >> y;
    }

    facility_location_problem<long long> p; // 距離も合計もlong longで計算する
    p.candidate_count = m; // 返る施設IDはsitesの添字
    p.demand_count = q; // 割当て配列はdemandsの順
    p.cost.resize(static_cast<std::size_t>(m) * q); // 費用表は候補優先

    for (int f = 0; f < m; ++f) {
        for (int i = 0; i < q; ++i) {
            const auto [x, y] = sites[f];
            const auto [a, b] = demands[i];

            // 座標の差、絶対値、距離の総和がlong longに収まる入力を使う。
            // 候補fから需要iへの距離を費用へ変換する。
            p.cost[static_cast<std::size_t>(f) * q + i] =
                std::abs(x - a) + std::abs(y - b);
        }
    }

    const auto result = solve_k_median(p, k); // 座標は動かさず、入力した候補地から選ぶ

    print_result(result); // 選択IDでsitesを参照すれば拠点座標を取り出せる

    return 0;
}
```

**入力例**

```text
3 4 2
0 0
5 0
10 0
0 1
1 0
9 0
10 2
```

**出力例（標準出力）**

```text
cost 5
facilities 0 2
assignment 0 0 2 2
```

### 3.4 例04：一方通行の道路網で配送し、届かない場所は外注する

一方通行によって往路と復路が違う、小さめの道路網を想定します。配送不能な需要は外注できます。

**具体的な問題**

候補頂点から需要頂点への最短路費用と、需要別の外注費の安い方を足し合わせます。

**入力**

V E M Q K、E行の有向辺u v 長さ、M個の候補頂点、Q個の需要頂点、Q個の外注費。全IDは0以上V未満。候補頂点は重複なし。

この前処理はO(V³)です。大きい疎グラフなら候補頂点ごとのDijkstra法などに置き換えます。最短路計算は本ライブラリのAPIではありません。

**実行コード：example_04.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

int main() {
    int v, e, m, q, k;
    if (!(std::cin >> v >> e >> m >> q >> k)) {
        return 0;
    }

    // 道路網の最短路費用を求める。頂点IDは0からv-1。
    const long long inf = std::numeric_limits<long long>::max() / 4;
    std::vector<std::vector<long long>> distance(v, std::vector<long long>(v, inf));
    for (int i = 0; i < v; ++i) {
        distance[i][i] = 0;
    }
    for (int t = 0; t < e; ++t) {
        int a, b;
        long long length;
        std::cin >> a >> b >> length; // 長さは非負。全経路長はinf未満とする
        distance[a][b] = std::min(distance[a][b], length);
    }

    for (int h = 0; h < v; ++h) {
        for (int a = 0; a < v; ++a) {
            for (int b = 0; b < v; ++b) {
                if (distance[a][h] != inf && distance[h][b] != inf) {
                    distance[a][b] = std::min(
                        distance[a][b],
                        distance[a][h] + distance[h][b]
                    );
                }
            }
        }
    }

    std::vector<int> sites(m), demands(q);
    for (int& id : sites) {
        std::cin >> id;
    }
    for (int& id : demands) {
        std::cin >> id;
    }

    facility_location_problem<long long> p; // グラフ自体は渡さず、最短路から費用表を作る
    p.candidate_count = m; // 候補IDとグラフ頂点IDを区別する
    p.demand_count = q; // 需要IDはdemandsの添字

    p.fallback_cost.resize(q); // 需要ごとの外注費を登録する
    for (auto& fee : p.fallback_cost) { // 0以上inf未満。fee+1も表現可能にする
        std::cin >> fee;
    }

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先の最短路費用
    for (int f = 0; f < m; ++f) {
        for (int i = 0; i < q; ++i) {
            const auto d = distance[sites[f]][demands[i]];

            // 外注より高い距離を切り詰めても最小値は変わらない。
            p.cost[static_cast<std::size_t>(f) * q + i] =
                std::min(d, p.fallback_cost[i] + 1); // 外注費より1だけ高い有限値で打ち切る
        }
    }

    // 到達不能のinfをsolverに渡さず、必ず外注が勝つ有限値にする。
    const auto result = solve_k_median(p, k);

    std::cout << "cost " << result.cost << '\n'; // 外注分も含む費用

    std::cout << "facility_vertices";
    for (int id : result.facilities) { // 候補IDを頂点IDへ戻す
        std::cout << ' ' << sites[id];
    }
    std::cout << '\n';

    std::cout << "assigned_vertices";
    for (int id : result.assignment) { // -1を配列添字にしない
        std::cout << ' ' << (id == -1 ? -1 : sites[id]);
    }
    std::cout << '\n';

    return 0;
}
```

**入力例**

```text
4 3 2 3 1
0 1 2
1 2 3
3 2 1
0 3
1 2 3
4 6 2
```

**出力例（標準出力）**

```text
cost 5
facility_vertices 3
assigned_vertices -1 3 3
```

### 3.5 例05：実在するデータから代表を選ぶk-medoids

写真・文章・形状などのデータから、代表として実際のデータをK個取り出したい場面です。

**具体的な問題**

N個のデータを候補にも需要にもします。代表fがデータiを説明するときの非負の違いを費用とします。

**入力**

N K、N行N列の違いの表。行が代表側、列が説明される側。通常は対角成分0。

**実行コード：example_05.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int n, k;
    if (!(std::cin >> n >> k)) {
        return 0;
    }

    facility_location_problem<long long> p; // 費用に対称性や三角不等式は要求されない
    p.candidate_count = n; // 各データを代表候補とする
    p.demand_count = n; // 各データを説明する対象とする

    p.cost.resize(static_cast<std::size_t>(n) * n); // 代表側を行とする
    for (auto& value : p.cost) { // 非負の違いを読み込む
        std::cin >> value;
    }

    const auto result = solve_k_median(p, k); // 新しい平均点を作らず、既存データIDを選ぶ

    print_result(result); // assignmentは代表の元ID。0からK-1のクラスタ番号ではない

    return 0;
}
```

**入力例**

```text
4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
```

**出力例（標準出力）**

```text
cost 3
facilities 0 2
assignment 0 0 2 2
```

### 3.6 例06：候補コードブックから二乗誤差の小さい代表ベクトルを選ぶ

画像の色や特徴量を、使用可能な代表ベクトルのうちK個へ置き換える場面です。

**具体的な問題**

D次元ベクトル間の各成分の差を二乗し、その和を費用にします。各需要を選択済み代表の最小誤差で近似します。

**入力**

M Q K D、M行の候補ベクトル、Q行の需要ベクトル。Dは1以上で、各行はD個の整数。

**実行コード：example_06.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int m, q, k, d;
    if (!(std::cin >> m >> q >> k >> d)) {
        return 0;
    }

    std::vector<std::vector<long long>> sites(m, std::vector<long long>(d));
    std::vector<std::vector<long long>> points(q, std::vector<long long>(d));
    for (auto& row : sites) {
        for (auto& x : row) {
            std::cin >> x;
        }
    }
    for (auto& row : points) {
        for (auto& x : row) {
            std::cin >> x;
        }
    }

    facility_location_problem<long long> p; // 費用表にも二乗和を保持できる型を使う
    p.candidate_count = m; // 候補コードブックの行数
    p.demand_count = q; // 近似対象ベクトルの個数
    p.cost.assign(static_cast<std::size_t>(m) * q, 0); // 費用を0で初期化して成分ごとに足す

    for (int f = 0; f < m; ++f) {
        for (int i = 0; i < q; ++i) {
            for (int t = 0; t < d; ++t) {
                // 差・二乗・総和がlong longに収まる入力を使う。
                const long long delta = sites[f][t] - points[i][t];
                p.cost[static_cast<std::size_t>(f) * q + i] += delta * delta; // 二乗誤差を足す
            }
        }
    }

    const auto result = solve_k_median(p, k); // 入力した候補ベクトルからK個を選ぶ

    print_result(result); // 連続空間で重心を動かすk-meansとは候補条件が異なる

    return 0;
}
```

**入力例**

```text
3 4 2 2
0 0
5 5
10 10
0 1
1 0
9 10
10 9
```

**出力例（標準出力）**

```text
cost 4
facilities 0 2
assignment 0 0 2 2
```

### 3.7 例07：K個の集合で重要な要素をできるだけ覆う

設置するセンサーや実施する施策をK個選び、重要な対象を広くカバーしたい場面です。

**具体的な問題**

被覆できる費用を0、できない費用を1とします。未被覆の重み合計を最小化すると、被覆済み重みの最大化になります。

**入力**

M Q K、M行Q列の0/1被覆表（1なら覆える）、Q個の非負重み。

**実行コード：example_07.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 1の被覆を費用0へ変換する
        int covered;
        std::cin >> covered;
        assert(covered == 0 || covered == 1);
        value = 1 - covered;
    }

    p.demand_weight.resize(q); // 対象の重要度を未被覆罰則にする
    for (auto& weight : p.demand_weight) { // 重み0の対象は費用に影響しない
        std::cin >> weight;
    }

    const auto result = solve_k_median(p, k); // 目的値は未被覆重みの合計

    // 全対象の重みを合計する。
    const long long total = std::accumulate(
        p.demand_weight.begin(), p.demand_weight.end(), 0LL // 需要ごとの重み
    );
    std::cout << "covered_weight " << total - result.cost << '\n'; // 被覆済み重みへ読み替える
    print_result(result); // assignmentだけでは被覆判定不可。元の被覆表または費用0を確認する

    return 0;
}
```

**入力例**

```text
3 4 2
1 1 0 0
0 1 1 0
0 0 1 1
2 3 5 7
```

**出力例（標準出力）**

```text
covered_weight 17
cost 0
facilities 0 2
assignment 0 0 2 2
```

### 3.8 例08：代表サービスを選び、利用者の満足度を最大化する

利用者ごとに、各サービス案への評価点がある場面です。選んだ案の中で各利用者が最も高く評価する案を使います。

**具体的な問題**

各需要iの全候補中の最高点B[i]を求め、費用をB[i]-評価点とします。最小費用を最高点合計から引くと、実際の満足度合計になります。

**入力**

M Q K、M行Q列の整数評価点。負の評価点も入力できるが、差と合計はlong longに収める。

**実行コード：example_08.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    // 需要ごとの最高点を求め、評価点を非負の損失へ変換する。
    std::vector<long long> baseline(q);
    for (int i = 0; i < q; ++i) {
        baseline[i] = p.cost[i]; // 一時的に読み込んだ施設0の評価点を基準にする

        for (int f = 1; f < m; ++f) {
            // 需要iに対する全候補中の最高点を更新する。
            baseline[i] = std::max(
                baseline[i], p.cost[static_cast<std::size_t>(f) * q + i] // 候補fの評価点
            );
        }

        for (int f = 0; f < m; ++f) {
            // 最高点からの差を、solverへ渡す非負の損失にする。
            p.cost[static_cast<std::size_t>(f) * q + i] =
                baseline[i] - p.cost[static_cast<std::size_t>(f) * q + i]; // 元の評価点を引く
        }
    }

    const auto result = solve_k_median(p, k); // solverへ渡す時点では全費用が非負

    const long long upper = std::accumulate(baseline.begin(), baseline.end(), 0LL);
    std::cout << "satisfaction " << upper - result.cost << '\n'; // 損失から満足度へ戻す
    print_result(result); // costは満足度そのものではなく、最高点からの損失

    return 0;
}
```

**入力例**

```text
3 3 2
8 1 -2
2 7 -1
0 0 9
```

**出力例（標準出力）**

```text
satisfaction 18
cost 6
facilities 0 2
assignment 0 0 2
```

### 3.9 例09：安い外注サービスを残しながら新設施設を選ぶ

全需要を新設施設へ移す必要がなく、現在のサービスや外注の方が安ければそちらを使える場面です。

**具体的な問題**

需要ごとに、新設施設からの最小費用と外注費の小さい方を選びます。外注先は施設数Kに数えません。

**入力**

M Q K、M行の費用表、Q個の非負外注費。

**実行コード：example_09.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    p.fallback_cost.resize(q); // 空は外注なし。長さqなら需要ごとの代替費用
    for (auto& fee : p.fallback_cost) { // 重みを掛ける前の費用を指定
        std::cin >> fee;
    }

    const auto result = solve_k_median(p, k); // 外注に加えて、ちょうどK候補を選ぶ

    print_result(result); // -1は外注。実施設と同額なら実施設IDが返る

    return 0;
}
```

**入力例**

```text
3 3 1
1 9 7
8 1 7
5 5 5
3 4 2
```

**出力例（標準出力）**

```text
cost 6
facilities 1
assignment -1 1 -1
```

### 3.10 例10：設置費や選択報酬も含めて施設を選ぶ

便利だが土地代の高い場所と、不便だが安い場所を比較する場面です。選択することで報酬を得る候補も表せます。

**具体的な問題**

需要へのサービス費用に加え、選んだ施設の費用を1回ずつ足します。負の施設費用は報酬です。

**入力**

M Q K、M行の非負サービス費用、M個の施設費用。施設費用だけは負でもよい。

**実行コード：example_10.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    p.facility_cost.resize(m); // 候補ごとの単項費用。空なら全て0
    for (auto& fee : p.facility_cost) { // 最小の負整数は削除時の符号反転に危険なので使わない
        std::cin >> fee;
    }

    const auto result = solve_k_median(p, k); // 費用が高くてもK未満へ減らすことはしない

    print_result(result); // costには施設費用も含まれ、負値になることもある

    return 0;
}
```

**入力例**

```text
3 2 1
0 8
4 4
8 0
3 -10 0
```

**出力例（標準出力）**

```text
cost -2
facilities 1
assignment 1 1
```

### 3.11 例11：必須の既存施設を残し、許可された候補だけから選ぶ

廃止できない既存施設と、契約上新設できる土地のリストがある場面です。

**具体的な問題**

必須施設を全て含むK施設を選びます。残りは許可候補から選びます。必須施設は許可候補の外でも構いません。

**入力**

M Q K、M行の費用表、F A、F個の必須ID、A個の許可候補ID。A=0は全候補許可。

**実行コード：example_11.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    int f, a;
    std::cin >> f >> a;

    facility_location_restrictions restrictions; // 指定しなければ固定なし・全候補許可
    restrictions.fixed_facilities.resize(f); // 必須施設はK個の内数。F<=Kが必要
    restrictions.candidate_facilities.resize(a); // 空は「候補なし」ではなく「全候補」
    for (int& id : restrictions.fixed_facilities) { // 0..M-1、配列内の重複禁止
        std::cin >> id;
    }
    for (int& id : restrictions.candidate_facilities) { // 配列内の重複禁止。fixedとの重複は可
        std::cin >> id;
    }

    // fixedと候補の和集合にK個以上必要。全施設を固定したい場合はF=Kにする。
    const auto result = solve_k_median(p, k, restrictions); // 制約付き。optionsは既定値

    print_result(result); // 必須施設もfacilitiesに含まれる

    return 0;
}
```

**入力例**

```text
4 4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
1 2
0
2 3
```

**出力例（標準出力）**

```text
cost 3
facilities 0 2
assignment 0 0 2 2
```

### 3.12 例12：手作りした初期配置を改善する

別の方法で見つけた配置や、前回の探索結果から改善を始めたい場面です。

**具体的な問題**

入力したK施設を初期配置とし、同じ費用表でさらに良い集合を探します。

**入力**

M Q K、M行の費用表、K個の初期施設ID。IDは有効範囲内で重複なし。

**実行コード：example_12.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    std::vector<int> initial(k);
    for (int& id : initial) {
        std::cin >> id;
    }

    const auto before = evaluate_k_median(p, initial); // 初期配置を評価。探索や制約チェックはしない

    facility_location_options options; // 省略したseedなどは既定値
    options.max_perturbations = 0; // ILSによる揺さぶりを行わず、1施設交換で改善する

    const auto result = improve_k_median(p, initial, options); // Kはinitial.size()から決まる

    std::cout << "before " << before << '\n'; // 改善前の費用
    print_result(result); // 返却時の施設順は昇順。入力の順序は保存されない

    return 0;
}
```

**入力例**

```text
4 4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
0 1
```

**出力例（標準出力）**

```text
before 17
cost 3
facilities 1 2
assignment 1 1 2 2
```

### 3.13 例13：大きな配置の一部だけを修復する

全体解の数施設だけを動かして改善する、部分問題としての使い方です。

**具体的な問題**

現在のK施設のうち、指定された施設だけを入れ替え可能にします。入れ替えない施設を固定し、追加候補を狭めます。

**入力**

M Q K、M行の費用表、現在のK施設、R A、動かしてよいR施設のID、追加先として許すA候補のID。

**実行コード：example_13.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    std::vector<int> initial(k);
    for (int& id : initial) {
        std::cin >> id;
    }

    int r, a;
    std::cin >> r >> a;
    std::vector<unsigned char> movable(m, 0), allowed(m, 0);
    for (int t = 0, id; t < r; ++t) {
        std::cin >> id;
        movable[id] = 1;
        allowed[id] = 1;
    }
    for (int t = 0, id; t < a; ++t) {
        std::cin >> id;
        allowed[id] = 1;
    }

    // 動かしてよいIDは現在の施設の部分集合とする。
    // 現在の可動施設も許可候補に残す。
    facility_location_restrictions restrictions; // 動かさない施設を固定する
    for (int id : initial) {
        if (!movable[id]) {
            restrictions.fixed_facilities.push_back(id); // 固定施設も需要を担当する
        }
    }
    for (int id = 0; id < m; ++id) {
        if (allowed[id]) {
            restrictions.candidate_facilities.push_back(id); // 重複のない許可候補を作る
        }
    }

    facility_location_options options; // 小さい修復を想定した探索量
    options.max_perturbations = 4; // 可動施設の範囲内で揺さぶりも試す

    // 初期配置も制約を満たしている必要がある。
    const auto result = improve_k_median(p, initial, restrictions, options);

    print_result(result); // movableは「移動可能」であり「必ず移動」ではない

    return 0;
}
```

**入力例**

```text
4 4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
0 1
1 2
1
2 3
```

**出力例（標準出力）**

```text
cost 3
facilities 0 3
assignment 0 0 3 3
```

### 3.14 例14：既存網へR施設を追加する価値を求める

既存の拠点網をそのまま残し、追加候補からR施設を導入する場面です。

**具体的な問題**

現在の需要別費用をfallbackとし、新施設費用も含む導入後の総費用を最小化します。現状費用との差を純改善量とします。

**入力**

M Q R、M行の新候補費用表、Q個の現状サービス費用、M個の新施設費用。需要重みは1。

**実行コード：example_14.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、追加する施設数Rを読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    p.fallback_cost.resize(q); // 既存網全体の最小サービス費用を需要別に指定する
    for (auto& value : p.fallback_cost) { // 既存施設ごとの費用は利用側で最小値へ集約する
        std::cin >> value;
    }

    p.facility_cost.resize(m); // 新しく選ぶ候補の導入費用
    for (auto& value : p.facility_cost) { // 既存施設の費用は変更しない定数として除外する
        std::cin >> value;
    }

    // 導入前のサービス費用を合計する。
    const long long baseline = std::accumulate(
        p.fallback_cost.begin(), p.fallback_cost.end(), 0LL // 需要ごとの既存網の最小費用
    );
    const auto result = solve_k_median(p, k); // kは新設数R。既存施設数を足さない

    std::cout << "net_gain " << baseline - result.cost << '\n'; // 正なら改善、負なら悪化
    print_result(result); // assignment=-1は既存網。具体的な担当IDは別途保持する

    return 0;
}
```

**入力例**

```text
3 3 1
1 8 8
8 1 8
8 8 1
5 5 5
2 1 3
```

**出力例（標準出力）**

```text
net_gain 3
cost 12
facilities 1
assignment -1 1 -1
```

### 3.15 例15：施設数そのものを外側で比較する

施設が多いほど便利になる一方、施設数に応じて費用が増える場面です。

**具体的な問題**

最小数から最大数までKを変えて解き、サービス費用と1施設当たりの固定費の合計で選びます。

**入力**

M Q Kmin Kmax H、M行の費用表。Hは1施設ごとの非負固定費。1<=Kmin<=Kmax<=M。

**実行コード：example_15.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int m, q, kmin, kmax;
    long long fee;
    if (!(std::cin >> m >> q >> kmin >> kmax >> fee)) {
        return 0;
    }

    facility_location_problem<long long> p; // Kごとに同じ費用表を使う
    p.candidate_count = m; // 候補数
    p.demand_count = q; // 需要数

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先の費用表
    for (auto& value : p.cost) { // 非負サービス費用
        std::cin >> value;
    }
    p.facility_cost.assign(m, fee); // 施設を選ぶたびにHを足す。別途K*Hを二重加算しない

    // 既定のcost=0は初期解ではないので、有無のフラグを使う。
    facility_location_result<long long> best;
    bool has_best = false;
    int best_k = 0;

    for (int k = kmin; k <= kmax; ++k) {
        // 1回ごとの施設数は固定。可変Kの判断はこの外側ループで行う。
        const auto result = solve_k_median(p, k);

        if (!has_best || result.cost < best.cost) { // 同額なら先に調べた小さいKを残す
            best = result; // 施設集合・割当て・統計をまとめて保存
            best_k = k;
            has_best = true;
        }
    }

    std::cout << "chosen_k " << best_k << '\n';
    print_result(best); // 各Kの探索は近似なので、全Kに対する大域最適の証明にはならない

    return 0;
}
```

**入力例**

```text
3 3 1 3 5
0 4 8
4 0 4
8 4 0
```

**出力例（標準出力）**

```text
chosen_k 1
cost 13
facilities 1
assignment 1 1 1
```

### 3.16 例16：同じ問題をworkspaceで繰り返し解く

同じ費用表と固定条件で、seedを変えた複数の探索を試す場面です。

**具体的な問題**

作業配列を再利用し、各seedで新規構築と既存最良解の改善を試します。問題そのものは更新しません。

**入力**

M Q K、M行の費用表、F T、F個の必須ID、T個のuint64_tのseed。T>=1。

**実行コード：example_16.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    int f, t;
    std::cin >> f >> t;

    facility_location_restrictions restrictions; // 同じ固定条件を全呼出しで使う
    restrictions.fixed_facilities.resize(f); // 必須IDは重複なし。F<=K
    for (int& id : restrictions.fixed_facilities) { // 候補の空配列は全候補許可のまま
        std::cin >> id;
    }

    std::vector<std::uint64_t> seeds(t);
    for (auto& seed : seeds) {
        std::cin >> seed;
    }

    // pはworkspaceより長く生存し、内容・所在を変えない。
    facility_location_workspace<long long> workspace(p, k, restrictions);
    facility_location_result<long long> best; // まだ解がない状態を別のboolで表す
    bool has_best = false;

    for (std::uint64_t seed : seeds) {
        facility_location_options options; // 統計と乱数状態は各呼出しでリセットされる
        options.seed = seed; // 異なる探索順を試す
        options.max_perturbations = 8; // 1回の呼出しで行う揺さぶり回数

        // 毎回新規構築。前の解から自動的に再開するわけではない。
        auto candidate = workspace.solve(options);
        if (!has_best || candidate.cost < best.cost) { // 最良結果を外側で保持する
            best = candidate; // より良い結果を保存する
            has_best = true;
        }

        candidate = workspace.improve(best.facilities, options); // 初期集合を明示して渡す
        if (candidate.cost < best.cost) { // 同じ問題・K・制約で改善したか確認する
            best = candidate; // 配列を再利用して得た、より良い結果を保存する
        }
    }

    print_result(best); // best.statisticsは保存した1回分。全seedの累積統計ではない

    return 0;
}
```

**入力例**

```text
4 4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
1 3
0
1 2 3
```

**出力例（標準出力）**

```text
cost 3
facilities 0 2
assignment 0 0 2 2
```

### 3.17 例17：ターンごとに需要や費用が変わる配置を再計算する

シミュレーションの次のターンで、配送先の数や配送費、重要度が変わる場面です。

**具体的な問題**

候補IDの意味とKを固定したまま、毎ターン新しい問題を作り、前の施設集合から改善します。

**入力**

M K T、最初のK施設ID。各ターンにQ、M行Q列の費用、Q個の重み。Mと候補IDの意味は全ターン共通。

これは保存した探索途中状態への差分更新ではありません。問題が変わるたびに内部状態を作り直します。候補の削除・番号変更がある場合は、前のIDを新しい有効な初期集合へ直す処理も必要です。

**実行コード：example_17.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    int m, k, turns;
    if (!(std::cin >> m >> k >> turns)) {
        return 0;
    }

    std::vector<int> previous(k);
    for (int& id : previous) {
        std::cin >> id;
    }

    for (int turn = 0; turn < turns; ++turn) {
        int q;
        std::cin >> q;

        facility_location_problem<long long> p; // 各ターンで新規作成。古いworkspaceは流用しない
        p.candidate_count = m; // 候補IDが前ターンと同じ施設を指すことが必要
        p.demand_count = q; // 需要数は変わってよい

        p.cost.resize(static_cast<std::size_t>(m) * q); // 新しい需要数で費用表を作り直す
        for (auto& value : p.cost) { // 新しいターンの費用を読む
            std::cin >> value;
        }

        p.demand_weight.resize(q); // 新しい需要に対応する重み
        for (auto& weight : p.demand_weight) { // 長さは今回のq
            std::cin >> weight;
        }

        facility_location_options options; // 探索統計も毎ターン新しくなる
        options.seed = static_cast<std::uint64_t>(turn) + 1; // ターンごとに探索順を変える例
        options.max_perturbations = 4; // 1ターンの揺さぶり回数

        // 前回の割当てではなく施設集合だけ引き継ぐ。
        const auto result = improve_k_median(p, previous, options);

        std::cout << "turn " << turn << '\n';
        print_result(result); // 今回の需要数に合ったassignmentを受け取る
        previous = result.facilities; // 次のターンの初期施設集合
    }

    return 0;
}
```

**入力例**

```text
4 2 2
0 1
4
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
1 1 1 1
3
9 0 5
8 1 5
0 8 5
1 9 0
3 1 2
```

**出力例（標準出力）**

```text
turn 0
cost 3
facilities 1 2
assignment 1 1 2 2
turn 1
cost 3
facilities 0 3
assignment 3 0 3
```

### 3.18 例18：外側の処理と残り時間を共有する

コンテストの全体solverが残り時間を管理し、この部分問題にも同じ終了時刻を渡す場面です。

**具体的な問題**

有効な初期配置を入力し、指定時刻を目安に改善します。新しい初期配置を作る時間を省けます。

**入力**

M Q K、M行の費用表、K個の初期ID、0以上の予算ミリ秒。入力読取りはこの予算に含めない。

入力例では既に最適な1施設を初期配置とし、標準出力を安定させています。一般の入力では時間内に進む探索が変わるため、同じseedでも解や統計が変わりえます。

**実行コード：example_18.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    std::vector<int> initial(k);
    for (int& id : initial) {
        std::cin >> id;
    }
    long long budget_ms;
    std::cin >> budget_ms;

    using clock = facility_location_options::clock; // deadlineはsteady_clockの絶対時刻
    facility_location_options options; // max_perturbations=-1はdeadlineがあればその時刻まで探索
    // 既に外側の締切があれば、その時刻を直接代入する。
    options.deadline = clock::now() + std::chrono::milliseconds(budget_ms);

    // 固定・候補制約は省略。構築時間も今回の予算内に含める。
    facility_location_workspace<long long> workspace(p, k);
    const auto result = workspace.improve(initial, options); // pを変更せず、初期集合から改善する

    // 最終結果作成などは中断しないため、deadlineは厳密な所要時間上限ではない。
    print_result(result);
    std::cerr << result.statistics << '\n'; // 統計は標準エラー。時間や実行負荷によって変わる

    return 0;
}
```

**入力例**

```text
3 3 1
0 3 6
3 0 3
6 3 0
1
5
```

**出力例（標準出力）**

```text
cost 6
facilities 1
assignment 1 1 1
```

### 3.19 例19：評価回数をそろえて探索量を管理する

時間の揺らぎを減らして探索量を比較したい場面や、小さな修復を何度も呼びたい場面です。

**具体的な問題**

まず初期構築だけを行い、次に追加候補の評価回数を上限Bとして改善します。

**入力**

M Q K、M行の費用表、0以上の評価上限B。Bはuint64_tに収まる整数。

評価回数はCPU命令数や経過時間そのものではありません。初期構築、再構築、割当ての出力作成は候補評価数に含まれません。

**実行コード：example_19.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    std::uint64_t budget;
    std::cin >> budget;

    facility_location_options options; // 時刻による停止は既定で無効
    options.initial_trials = 1; // 初期構築を1試行に絞る
    options.max_perturbations = 0; // 初期構築だけを取り出すため揺さぶりも止める
    options.max_candidate_evaluations = 0; // 1-swap評価はしない。初期構築は行われる
    const auto initial = solve_k_median(p, k, options); // K個の実行可能な施設集合を構築する

    std::cout << "initial_cost " << initial.cost << '\n'; // 初期構築後の費用

    options.max_candidate_evaluations = budget; // 次の呼出し全体での候補評価上限
    options.max_perturbations = 4; // 揺さぶりも含め、評価回数の合計がBを超えない
    // 統計と評価予算はこの呼出しから数え直す。
    const auto result = improve_k_median(p, initial.facilities, options);

    print_result(result); // B=0でも初期集合の評価と返却は行う

    // 交換ペア数ではなく、追加候補の評価数を出力する。
    std::cout << "evaluations " << result.statistics.candidate_evaluations << '\n';

    std::cout << "limit_reached " << std::boolalpha
              << result.statistics.evaluation_limit_reached << '\n'; // 上限0なら評価0でもtrue
    std::cerr << result.statistics << '\n'; // 全統計はoperator<<でも確認できる

    return 0;
}
```

**入力例**

```text
4 4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
20
```

**出力例（標準出力）**

```text
initial_cost 3
cost 3
facilities 0 2
assignment 0 0 2 2
evaluations 10
limit_reached false
```

### 3.20 例20：小数・非対称の損失を扱う

実行方式ごとの誤差率や処理品質など、距離とは限らない小数の損失表を使う場面です。

**具体的な問題**

重み付き損失、現状維持の損失、選択費用を合わせて小さくします。表は非対称で構いません。

**入力**

M Q K、M行の小数費用表、Q個の重み、Q個のfallback費用、M個の施設費用。

**実行コード：example_20.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

// 費用、選択施設ID、各需要の担当施設IDを3行で出力する。
template <class Calc>
void print_result(const facility_location_result<Calc>& result) { // 返却値は変更しない
    std::cout << "cost " << result.cost << '\n'; // 重み・fallback・施設費用を含む合計

    std::cout << "facilities";
    for (int id : result.facilities) { // 昇順の候補ID。配列内の位置とは異なる
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    std::cout << "assignment";
    for (int id : result.assignment) { // 需要の入力順。-1はfallback利用
        std::cout << ' ' << id;
    }
    std::cout << '\n';
}

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    // 費用・重み・合計を全てdoubleで扱う。Calcを省略するとlong longになる
    facility_location_problem<double, double> p;
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    p.demand_weight.resize(q); // 小数の非負重み
    for (auto& value : p.demand_weight) { // 有限な値を使う
        std::cin >> value;
    }

    p.fallback_cost.resize(q); // 現状維持の非負損失
    for (auto& value : p.fallback_cost) { // NaN・無限大は使わない
        std::cin >> value;
    }

    p.facility_cost.resize(m); // 方式選択の単項費用。ここは負でもよい
    for (auto& value : p.facility_cost) { // 全中間計算も有限にする
        std::cin >> value;
    }

    // 小数型では、交換後の再計算でも改善している交換だけを受理する。
    const auto result = solve_k_median(p, k);

    std::cout << std::fixed << std::setprecision(6);
    print_result(result); // 表示桁数を指定しても、内部計算の精度自体は変わらない

    // 数値比較には費用の尺度に応じた許容誤差を用いる。
    // epsilonを設定するAPIはない。

    return 0;
}
```

**入力例**

```text
3 3 1
0.2 0.8 0.7
0.6 0.1 0.9
0.5 0.5 0.2
1.5 1 2
0.4 0.7 0.6
0.1 -0.1 0.2
```

**出力例（標準出力）**

```text
cost 1.700000
facilities 2
assignment -1 2 2
```

### 3.21 例21：候補が少ない問題を全列挙で厳密に評価する

候補数や選択数が小さく、全ての組合せを調べられる場面です。

**具体的な問題**

全てのK施設集合を利用側で列挙し、evaluate_k_medianで目的値を計算します。整数計算の範囲内で厳密な最良集合を返します。

**入力**

M Q K、M行の費用表、Q個の重み、Q個のfallback費用、M個の施設費用。組合せ数が十分小さい入力に限る。

全列挙機能はこのサンプル側の実装です。計算量は組合せ数×O(QK+K)。大きい問題にそのまま使うと時間が足りません。

**実行コード：example_21.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    facility_location_problem<long long> p; // 費用表の型を指定。省略したCalcはlong long
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    p.demand_weight.resize(q); // 非負重み。指定した数値が評価にもそのまま使われる
    for (auto& value : p.demand_weight) { // q要素
        std::cin >> value;
    }

    p.fallback_cost.resize(q); // 全列挙の各集合に共通の代替費用
    for (auto& value : p.fallback_cost) { // q要素、非負
        std::cin >> value;
    }

    p.facility_cost.resize(m); // 選択施設の単項費用
    for (auto& value : p.facility_cost) { // m要素、負も可
        std::cin >> value;
    }

    // K個を選ぶ全ての組合せについて費用を比較する。
    std::vector<int> take(m, 0), best_ids;
    std::fill(take.begin(), take.begin() + k, 1);
    long long best_cost = 0;
    bool has_best = false;

    do {
        std::vector<int> chosen;
        for (int id = 0; id < m; ++id) {
            if (take[id]) {
                chosen.push_back(id);
            }
        }

        // 非空・重複なしの集合を評価する。制約の確認や探索は行わない。
        const auto cost = evaluate_k_median(p, chosen);
        if (!has_best || cost < best_cost) {
            best_cost = cost;
            best_ids = chosen;
            has_best = true;
        }
    } while (std::prev_permutation(take.begin(), take.end()));

    std::cout << "cost " << best_cost << '\n' << "facilities";
    for (int id : best_ids) {
        std::cout << ' ' << id;
    }
    std::cout << '\n';

    // 固定施設・許可候補を課すなら、evaluateを呼ぶ前に利用側で集合を絞り込む。

    return 0;
}
```

**入力例**

```text
4 4 2
0 2 9 10
2 0 8 9
9 8 0 1
10 9 1 0
1 2 3 4
10 10 10 10
0 2 -1 0
```

**出力例（標準出力）**

```text
cost 7
facilities 0 2
```

### 3.22 例22：64ビットを超える費用と長い結果をデバッグ出力する

1件の費用は64ビットに収まるものの、重みとの積や合計が64ビットを超える場面です。

**具体的な問題**

CalcにGCCの符号付き128ビット整数を使い、結果全体をoperator<<で表示します。長い割当て配列は20要素で省略されます。

**入力**

M Q K、M行のlong longに収まる非負費用、Q個のlong longに収まる非負重み。

**実行コード：example_22.cpp**

```cpp
// 問題・探索・結果型を使うためのヘッダ
#include "facility_location_v13.hpp"

// 共通前提:
// ・需要数と候補数は1以上。1 <= K <= 候補数とする。
// ・探索は大域最適を保証しない。
// ・サービス費用・重み・fallbackは有限かつ非負とする。
// ・全中間演算が各型の範囲に収まる入力を使う。施設単項費用だけは負も可。
// 未設定の配列: 重みは全1、fallbackなし、施設単項費用は全0。
// optionsの既定値:
// ・seed = 1、初期試行4回、deadlineがない場合の揺さぶりは最大32回。
// ・時間と候補評価回数には上限を設けない。

int main() {
    // 候補数、需要数、選ぶ施設数を読み込む。
    int m, q, k;
    if (!(std::cin >> m >> q >> k)) {
        return 0;
    }
    assert(m > 0 && q > 0 && 1 <= k && k <= m);

    // 費用表はlong long、重み・合計・差分は128ビット
    facility_location_problem<long long, __int128_t> p;
    p.candidate_count = m; // 候補IDは0からm-1
    p.demand_count = q; // 需要IDは0からq-1

    p.cost.resize(static_cast<std::size_t>(m) * q); // 候補優先のm行q列を1次元で保持
    for (auto& value : p.cost) { // 施設0の全需要費用、施設1の全需要費用、…の順
        std::cin >> value;
    }

    p.demand_weight.resize(q); // 重みの格納先は128ビット
    for (auto& value : p.demand_weight) { // 標準cinは__int128_tへ直接読み込めない
        long long input;
        std::cin >> input;
        value = static_cast<__int128_t>(input); // この例の入力重みはlong longの範囲とする
    }

    facility_location_options options; // 出力例を再現しやすいよう、初期構築だけを1回行う
    options.initial_trials = 1; // 1試行の初期構築
    options.max_perturbations = 0; // 揺さぶりなし
    options.max_candidate_evaluations = 0; // 1-swap評価なし。一般のKでは初期解の品質に注意

    const auto result = solve_k_median(p, k, options); // 中間演算も128ビットに収まる必要がある

    // 128ビットのcostも十進表示。配列は先頭20要素、総数と省略数付き。
    std::cout << result << '\n';
    std::cout << result.statistics << '\n'; // 統計だけの表示も可能。末尾改行は利用側で付ける

    // std::cout << result.cost は標準ostreamの未対応型なので使わない。

    return 0;
}
```

**入力例**

```text
2 21 1
1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000 1000000000000000000
2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000 2000000000000000000
20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20
```

**出力例（標準出力）**

```text
facility_location_result{cost=420000000000000000000, facilities(size=1)=[0], assignment(size=21)=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... (+1)], statistics={candidate_evaluations=0, accepted_swaps=0, completed_initial_trials=1, completed_perturbations=0, deadline_reached=false, evaluation_limit_reached=true}}
{candidate_evaluations=0, accepted_swaps=0, completed_initial_trials=1, completed_perturbations=0, deadline_reached=false, evaluation_limit_reached=true}
```
