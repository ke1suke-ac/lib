// beam_skelton_v46.cpp
//
// ビームサーチ6種類とchokudaiサーチ3種類を、1ファイル内で切り替えて試すためのスケルトンです。
// 使うライブラリは BEAM_SKELTON_KIND で選びます。
//
//   1: beam_delta_multi_v83.hpp       turn + Action保持型
//   2: beam_delta_single_v59.hpp      turn統一 + Action保持型。各手は必ず +1 turn
//   3: beam_copy_multi_v58.hpp        turn + State保持型
//   4: beam_copy_single_v54.hpp       turn統一 + State保持型。各手は必ず +1 turn
//   5: chokudai_copy_multi_v31.hpp    chokudai + State保持型。各手のstepを指定
//   6: chokudai_copy_single_v29.hpp   chokudai + State保持型。各手は必ず +1 turn
//   7: beam_delta_sub_v26.hpp        turn内sub探索 + Action保持型
//   8: beam_copy_sub_v26.hpp         turn内sub探索 + State保持型
//   9: chokudai_copy_sub_v27.hpp     chokudai + turn内sub探索 + State保持型
//
// 例:
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=1 beam_skelton_v46.cpp
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=7 beam_skelton_v46.cpp
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=8 beam_skelton_v46.cpp
//   g++ -std=gnu++20 -O2 -DLOCAL -DBEAM_SKELTON_KIND=9 -DBEAM_SKELTON_USE_CSV_HOOK=1 beam_skelton_v46.cpp
//
// 注意:
//   使うヘッダをこのファイルと同じディレクトリに置くか、コンパイル時に -I で場所を指定してください。
//   chokudaiの型は cs::ChokudaiCopyMulti / cs::ChokudaiCopySingle / cs::ChokudaiCopySub です。
//   実探索で使うときは、Param の値と expand の中身を問題に合わせて書き換えてください。
//   Param は現在のデフォルト値で明示的に埋めています。そのため、そのまま実行すると max_turn=0 です。
//
// max_turn について:
//   9種類とも、ライブラリ側で max_turn の状態を展開しません。max_turn=0 なら expand は呼ばれません。
//   ビームの縮退探索、chokudaiの時間切れ後の追加スイープでも同じです。
//   したがって、expand の先頭に最大ターンを判定する防御的な早期 return は不要です。
//   single版の候補は必ず次のturnへ進みます。
//   multi版では、step は 1..max_step を指定し、到着turnが max_turn を超える候補はライブラリが棄却します。
//   push_lazy でも同じ判定を行い、到着turn超過の候補では maker を呼びません。
//   途中turnの is_answer=true は「解候補にする」フラグです。
//   expand_answers=falseなら、有効解は保存だけを行い、探索候補へ登録しません。
//   全9種類で、最良有効解を幅・Hash選別と独立して保存します。
//   max_turn到着候補は継続探索用プールへ入れず、必要なら最良解として保存します。
//   pushのfalseは探索候補への棄却であり、解なしとは限りません。解の有無はfoundで確認します。
//   copy型のpush_lazyは解だけの保存でもmakerを呼びます。各makerは1呼び出しにつき高々1回です。
//   有効解をすべて終端にする問題ではexpand_answers=falseにし、expand内の終端判定を省けます。
//   続けたい有効解と終端が混在する場合はtrueにし、終端だけ利用側でreturnします。
//   有効解候補の生成数はdebug hookのanswer_candidatesで確認します。
//   CSVのbest_unchanged_msは改善したexpand終了からの時間です。記録間隔には依存しません。
//   multiではgenerated_step_avgとexpanded_step_avgで、生成と実際の展開のstepを比較できます。
//
// 全種類の時間切れ方針:
//   continue_on_timeout=true（既定）では、解未発見の場合だけ時間超過後の救済探索を続けます。
//   falseなら時間切れ検出時に終了します。有効解を保存済みなら、どちらの設定でも終了します。
//   救済中に解を見つけた場合も現在のexpandが返ったところで終了します。途中中断は行いません。
//   通常single/multiは最良1本を中心に進め、subは次turnへ進める候補を優先します。
//   chokudaiは取出し幅1で救済スイープを繰り返します。解の発見保証や超過時間の上限はありません。
//
// 通常ビームのturn別時間配分（1 / 2 / 3 / 4 / 7 / 8）:
//   time_limit_msを正にしてuse_turn_time_limit=trueにすると、残り時間を残りturn数へ配分します。
//   各turn開始時に締切を決め、Sub層の途中では更新しません。time_check_interval=1なら毎展開確認します。
//   turn期限後は未来候補があれば進み、なければ候補ができるか尽きるまで探索します。
//   turn期限だけでは保持幅を縮めず、次turnで通常探索へ戻ります。全体期限後は各版の時間超過処理です。
//   Runtimeのturn_time_limit_reachedは各turnでリセットされ、全体のtime_limit_reachedとは独立です。
//   chokudaiの3種類にはuse_turn_time_limitはありません。
//
// deltaの返却状態（1 / 2 / 7）:
//   apply_best_on_return=false（既定）なら、正常返却時の外部Stateは初期状態です。
//   trueで有効解がある場合、初期状態へ戻してから最良path全体を同じforwardで一度だけ再生します。
//   解なしは初期状態、空pathなら移動なし。初期Stateも必要ならrun前に利用側でコピーします。
//   RunEndは再生後です。再生時間はelapsed_us、debug hookのforward回数はforward_callsへ含みます。
//   再生は時間超過でも完了させるため、その分の余裕を時間予算に残してください。
//
// chokudaiのパラメータとスイープについて（5 / 6 / 9）:
//   beam_width は各turnに保持する未展開候補の上限、sweep_width は1スイープで各turnから展開する上限です。
//   first_sweep_widthは初回だけの上限です。0以下はsweep_width、正値はbeam_width以下へ丸めます。
//   例: beam_width=1024、first_sweep_width=128、sweep_width=4。初回を広く探索し、残りを細く継続します。
//   初回幅をbeam_widthと等しくすると、初回完走でキューを使い切り、後続スイープがなくなります。
//   sub版の両sweep幅はturn開始候補だけへ作用し、sub_beam_widthは変えません。
//   スイープは浅いturnから順に進み、残った候補を次のスイープへ持ち越します。
//   Hashによる重複排除は同じ到着turnの未展開候補が対象で、展開済みのHashは保持しません。
//   max_sweeps <= 0 は回数無制限、time_limit_ms <= 0.0 は時間無制限です。
//   時間切れ時の救済はcontinue_on_timeoutで指定します。通常スイープ上限は救済には適用しません。
//   選別で棄却された有効解も解発見に含め、救済中なら現在のexpand終了後に打ち切ります。
//
// 途中層のcost分析（1 / 2 / 3 / 4 / 7 / 8 / 9）:
//   CSVのlayer_sizeとlayer_cost_min/mean/max/stddevは、選別後・展開前の1層全体を表します。
//   single/multiはTurnEndのturn、subはSubStepEndのturn/sub_stepで層を特定します。
//   全層を見る実行はCsvStatHookの記録間隔を0にします。複数層のcostは合算しません。
//   chokudai subのsub_step=0は取出し前のキュー全体。5 / 6のSweepEnd CSVには追加しません。
//   集計は診断有効時のみ。評価値の同点と、State自体の多様性は別に確認してください。
//
// sub版（7 / 8 / 9）について:
//   push / push_lazy のcost直後の Next::Sub は同turnの次sub層へ、Next::Turn は次turnの開始候補へ追加します。
//   delta subは親・部分木が近い候補をまとめて展開し、外部状態の往復を減らします。
//   不要Nodeはまとめて回収し、候補・最良解・現在地に必要な祖先は保持します。
//   不要Nodeを回収・再利用し、格納領域は必要に応じて自動拡張します。
//   Nextは省略できません。同じexpandで両方を生成でき、それぞれ別の候補群で比較します。
//   beam_width は次turnの開始候補幅、sub_beam_width はturn内の各sub層の幅（0以下ならbeam_width）です。
//   now.turn は完了turn数、now.sub_step は現在turn内のSub遷移数、now.depth は経路全体の遷移数です。
//   Turn遷移でsub_stepは0へ戻ります。Sub遷移は各経路で必ず有限回となるようにしてください。
//   次sub層が空なら次turnへ進みます。例では各層からTurn候補を生成し、Sub遷移は最大2回にしています。
//   time_limit_msは探索全体の制限です。未発見時の救済では、次turn候補があればturn内探索を切り上げます。
//   次turn候補もなければ、探索可能なsub候補から生成を試みます。
//   9ではsweep_width個までの開始候補からturn内を通常のビームで探索し、残りの開始候補は次スイープへ持ち越します。
//   有効解があれば全種類とも時間切れで終了します。実行中のexpandは中断しません。
//   9のmax_sweepsは通常スイープの上限です。時間超過後、有効解未発見時の救済には適用しません。
//   is_answer=true はSub / Turnのどちらでも指定でき、規定turn数に達する前の有効解にも使えます。
//
// Actionも遅延生成するpush_lazy（copy6種類）:
//   cost / hashなどの後に、make_action、make_stateの順で2つのmakerを渡せます。
//   make_action() -> Actionを生成後、make_state(const Action&) -> Stateを同期的に各1回呼びます。
//   保存不要な候補では両方とも呼びません。最良解だけを保存してfalseを返す場合には呼びます。
//   Action参照はmake_stateの実行中だけ有効です。参照やポインタをStateへ保持しないでください。
//   コストの高いAction生成はmake_actionの本体へ置き、キャプチャ初期化で先に生成しないでください。
//   maker内では候補1個を生成し、emitへの再入呼出しは行わないでください。
//   push_lazy(cost, ..., action, make_state)も使えます。make_stateはconst Action&を受け取ります。軽いActionでは両形式を比較してください。
//
// 操作履歴について:
//   copy6種類はrun<State, Action>で履歴を共有木に保存し、result.pathに返します。
//   操作列が不要ならrun<State> / Emitter<State>を使い、pushからAction引数を除きます。
//   履歴なしのpush_lazyには、引数なしでStateを返すmakerだけを渡します。
//   emit.history()はexpand中の経路の参照です。empty()、action()、parent()で祖先をたどれます。
//   Viewはexpand内だけ有効。Action参照はpush / push_lazyをまたいで保持せず再取得します。
//   祖先のStateは保持しません。必要な評価用情報は現在のStateへ持たせます。
//   multiのpathはEdge<Action>、single / subはActionです。subはturn_offsetsも返します。
//   delta3種類のノード格納領域は自動拡張。容量の事前設定は不要です。
//
// CSV Hook について:
//   BEAM_SKELTON_USE_CSV_HOOK=1 を指定すると、debug_hook に BS::CsvStatHook を使います。
//   CSV出力には -DLOCAL も必要です。LOCAL未定義時のCsvStatHookは、記録・出力を行わない空実装です。
//   0 の場合はNoOpです。独自hookの例はBEAM_SKELTON_USE_DEBUG_HOOK=1で有効になります。
//   全方式でRunStart / TurnEnd / SearchEnd / RunEndを通知します。
//   subはSubStepEnd、chokudaiはSweepStart / SweepEndも通知します。CSVは共通列と方式固有列です。
//   列順は経過・座標、最良解、層cost、探索量・選別、詳細診断、設定、方式識別です。
//   全9種類で共通列の相対順序を揃え、方式固有列は関連項目のまとまりに入れています。
//   CSVのschema_versionは7です。全列の出力順はガイド13.4を参照してください。
//   Hash照会率・Hash除外率・選別対象数を同じ母集団で並べます。除外0は重複なしとは限りません。
//   subのexpanded_sub_step_maxは記録区間内で実際に展開した最大sub_stepです。間引いた層も含みます。
//   search_totalのsweep_completedは最後のsweepの完了状態、未開始なら空欄です。
//   best_gainは区間両端に解がある場合のみ。search_totalでは空欄です。
//   time_limit_reachedはRunEndでは返却準備中の期限到達も含みます。
//   chokudai single/multiのCSVはSweepEnd単位です。RunStart / SearchEnd / RunEndも記録します。
//   初回・未完了のSweepEndは間隔によらず記録し、TurnEnd / SweepStartは記録しません。
//   診断はBSの第3テンプレート引数EnableDiagnostics（既定true）で切り替えます。
//   LOCALなし、またはfalse指定時は、診断用領域・更新・全debug hook通知をコンパイル時に除去します。
//   LOCALあり・trueではNoOpでも集計します。計測負荷を止めるにはBSの第3引数をfalseにします。

#ifndef BEAM_SKELTON_KIND
#define BEAM_SKELTON_KIND 1
#endif

#ifndef BEAM_SKELTON_USE_CSV_HOOK
#define BEAM_SKELTON_USE_CSV_HOOK 0
#endif

#ifndef BEAM_SKELTON_USE_DEBUG_HOOK
#define BEAM_SKELTON_USE_DEBUG_HOOK 0
#endif

#include <cstdint>
#include <iostream>

#if BEAM_SKELTON_KIND == 1
#include "beam_delta_multi_v83.hpp"

namespace {

// turn + Action保持型。
// ライブラリはAction列だけを保持し、盤面などの外部状態は move_forward / move_backward でユーザーが更新します。
struct Action {
    int delta = 0;
};

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = bs::Beam<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / max_step / beam_width などを問題に合わせて変更します。
    BS::Param param{};
    param.apply_best_on_return = false; // trueなら、解があるとき最良解の外部Stateで返却します。
    param.max_turn = 0;
    param.max_step = 1;
    param.beam_width = 1;
    param.hash_capacity = 0;
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;

    int current_position = 0;

    // expand は NodeView と Emitter を受け取ります。
    // Emitterを含め、引数の型を明示しています。
    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::NodeView& now, const BS::Runtime& runtime, BS::Emitter<Action>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 有効解を終端にする場合は、param.expand_answers=falseを指定してください。
        const Action action{1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1; // 1以上、param.max_step以下。到着turnがmax_turnを超える候補は棄却されます。
        const bool is_answer = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: cost, step, hash, is_answer, action。
        // step は int 固定です。hash は unsigned かつ Hash 以下の幅の型を渡します。
        emit.push(next_cost, step, next_hash, is_answer, action);
    };

    // Action型ライブラリでは、外部状態を move_forward / move_backward で更新します。
    // turn + Action型では Edge<Action> を受け取ると、開始turnとstepも参照できます。
    auto move_forward = [&current_position](const BS::Edge<Action>& edge) {
        current_position += edge.action.delta;
    };
    auto move_backward = [&current_position](const BS::Edge<Action>& edge) {
        current_position -= edge.action.delta;
    };

    // DebugHook は任意です。最大引数の run 例として明示的に渡しています。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、全イベントを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_delta_multi_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.best_cost, runtime.expanded_nodes などをログ出力できます。
    };
#else
    BS::NoOp debug_hook;
#endif

    const Cost initial_cost = 0;
    // 初期Hashは省略時0。差分Hashを使う場合はinitial_costの次に初期値を渡します。

    BS::Result<Action> result = BS::run<Action>(
        param,
        initial_cost,
        expand,
        move_forward,
        move_backward,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " path_size=" << result.path.size()
              << " current_position=" << current_position
              << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 2
#include "beam_delta_single_v59.hpp"

namespace {

// Action保持型。各Actionは必ず次のturnへ進みます。
// ライブラリはAction列だけを保持し、盤面などの外部状態は move_forward / move_backward でユーザーが更新します。
struct Action {
    int delta = 0;
};

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = bs::Beam<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / beam_width などを問題に合わせて変更します。
    BS::Param param{};
    param.apply_best_on_return = false; // trueなら、解があるとき最良解の外部Stateで返却します。
    param.max_turn = 0;
    param.beam_width = 1;
    param.hash_capacity = 0;
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;

    int current_position = 0;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::NodeView& now, const BS::Runtime& runtime, BS::Emitter<Action>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 有効解を終端にする場合は、param.expand_answers=falseを指定してください。
        const Action action{1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool is_answer = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: cost, hash, is_answer, action。
        emit.push(next_cost, next_hash, is_answer, action);
    };

    // depth/turn統一 + Action型では、move callback は Action を受け取ります。
    auto move_forward = [&current_position](const Action& action) {
        current_position += action.delta;
    };
    auto move_backward = [&current_position](const Action& action) {
        current_position -= action.delta;
    };

    // DebugHook はラムダでも、標準の CsvStatHook でも指定できます。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、全イベントを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_delta_single_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#else
    BS::NoOp debug_hook;
#endif

    const Cost initial_cost = 0;
    // 初期Hashは省略時0。差分Hashを使う場合はinitial_costの次に初期値を渡します。

    BS::Result<Action> result = BS::run<Action>(
        param,
        initial_cost,
        expand,
        move_forward,
        move_backward,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " path_size=" << result.path.size()
              << " current_position=" << current_position
              << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 3
#include "beam_copy_multi_v58.hpp"

namespace {

// State保持型。盤面はState、出力用の操作履歴はActionとして分けます。
struct State {
    int value = 0;
};

// Actionはコピー構築できる型にします。祖先Stateは保存されません。
struct Action { int delta = 0; };

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = bs::Beam<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / max_step / beam_width などを問題に合わせて変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.max_step = 1;
    param.beam_width = 1;
    param.hash_capacity = 0;
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State, Action>& emit) {
        (void)runtime;
        // 履歴を使わない場合、この呼び出し自体が不要です。
        auto history = emit.history();
        if (!history.empty()) {
            const Action& previous = history.action(); // コピーせず直前の操作を参照。
            (void)previous;
            // history.parent()はさらに1つ前の履歴。初期状態の履歴はemptyです。
        }
        // 上のAction参照はここまで。push後はhistory.action()から再取得します。

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 有効解を終端にする場合は、param.expand_answers=falseを指定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1; // 1以上、param.max_step以下。到着turnがmax_turnを超える候補は棄却されます。
        const bool is_answer = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: cost, step, hash, is_answer, action, state。
        emit.push(next_cost, step, next_hash, is_answer, Action{1}, next);

        // Emitter::push_lazy の最大引数例: cost, step, hash, is_answer, make_action, make_state。
        // Action生成、State生成の順で各1回呼びます。保存不要なら両方とも呼びません。
        emit.push_lazy(
            next_cost + 10,
            step,
            static_cast<Hash>(next_hash + Hash{100}),
            is_answer,
            []() -> Action { return Action{1}; },
            [&now](const Action& action) -> State { return State{now.state.value + action.delta}; }
        );
    };

    // DebugHook はラムダでも、標準の CsvStatHook でも指定できます。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、全イベントを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_copy_multi_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#else
    BS::NoOp debug_hook;
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State, Action> result = BS::run<State, Action>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " path_size=" << result.path.size()
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 4
#include "beam_copy_single_v54.hpp"

namespace {

// State保持型。各展開は必ず次のturnへ進みます。
// 盤面はState、出力用の1操作はActionへ分離します。履歴木はライブラリが保持します。
struct State {
    int value = 0;
};

// Actionはコピー構築できる型にします。祖先Stateは保存されません。
struct Action { int delta = 0; };

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = bs::Beam<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / beam_width などを問題に合わせて変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.beam_width = 1;
    param.hash_capacity = 0;
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State, Action>& emit) {
        (void)runtime;
        // 履歴を使わない場合、この呼び出し自体が不要です。
        auto history = emit.history();
        if (!history.empty()) {
            const Action& previous = history.action(); // コピーせず直前の操作を参照。
            (void)previous;
            // history.parent()はさらに1つ前の履歴。初期状態の履歴はemptyです。
        }
        // 上のAction参照はここまで。push後はhistory.action()から再取得します。

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 有効解を終端にする場合は、param.expand_answers=falseを指定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool is_answer = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: cost, hash, is_answer, action, state。
        emit.push(next_cost, next_hash, is_answer, Action{1}, next);

        // Emitter::push_lazy の最大引数例: cost, hash, is_answer, make_action, make_state。
        // Action生成、State生成の順で各1回呼びます。保存不要なら両方とも呼びません。
        emit.push_lazy(
            next_cost + 10,
            static_cast<Hash>(next_hash + Hash{100}),
            is_answer,
            []() -> Action { return Action{1}; },
            [&now](const Action& action) -> State { return State{now.state.value + action.delta}; }
        );
    };

    // DebugHook はラムダでも、標準の CsvStatHook でも指定できます。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、全イベントを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_copy_single_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#else
    BS::NoOp debug_hook;
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State, Action> result = BS::run<State, Action>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " path_size=" << result.path.size()
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 5
#include "chokudai_copy_multi_v31.hpp"

namespace {

// State保持型。各展開で到着turnまでのstepを指定します。
// 盤面はState、出力用の1操作はActionへ分離します。履歴木はライブラリが保持します。
struct State {
    int value = 0;
};

// Actionはコピー構築できる型にします。祖先Stateは保存されません。
struct Action { int delta = 0; };

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = cs::ChokudaiCopyMulti<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / max_step / beam_width / sweep_width や探索の制限を変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.max_step = 1;
    param.beam_width = 1;         // 各turnに保持する未展開候補の上限
    param.hash_capacity = 0;     // 0は自動。実容量はbeam_widthを基準に調整されます。
    param.time_limit_ms = 0.0;
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.sweep_width = 1;     // 1スイープで各turnから展開する上限
    param.first_sweep_width = 0; // 0以下は通常幅。正なら初回だけこの幅（beam_width以下）。
    param.max_sweeps = 0;        // 0以下はスイープ数の制限なし

    // Runtime 付きの最大引数形を使っています。
    // 現在のturnは now.turn、スイープ番号は runtime.sweep で参照します。
    // runtime.elapsed_us は最後に時計を検査した時点の値です。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State, Action>& emit) {
        (void)runtime;
        // 履歴を使わない場合、この呼び出し自体が不要です。
        auto history = emit.history();
        if (!history.empty()) {
            const Action& previous = history.action(); // コピーせず直前の操作を参照。
            (void)previous;
            // history.parent()はさらに1つ前の履歴。初期状態の履歴はemptyです。
        }
        // 上のAction参照はここまで。push後はhistory.action()から再取得します。

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 有効解を終端にする場合は、param.expand_answers=falseを指定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1; // 1以上、param.max_step以下。到着turnがmax_turnを超える候補は棄却されます。
        const bool is_answer = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: cost, step, hash, is_answer, action, state。
        emit.push(next_cost, step, next_hash, is_answer, Action{1}, next);

        // Emitter::push_lazy の最大引数例: cost, step, hash, is_answer, make_action, make_state。
        // Action生成、State生成の順で各1回呼びます。保存不要なら両方とも呼びません。
        emit.push_lazy(
            next_cost + 10,
            step,
            static_cast<Hash>(next_hash + Hash{100}),
            is_answer,
            []() -> Action { return Action{1}; },
            [&now](const Action& action) -> State { return State{now.state.value + action.delta}; }
        );
    };

    // 独自DebugHookにはTurnEnd / SweepStartも通知しますが、CSVはSweepEnd単位です。
    // interval_ms <= 0.0なら毎sweep記録。正ならSweepEndで間隔を判定します。
    // RunStart / SearchEnd / RunEndと初回・未完了のSweepEndは必ず記録します。
    // 間引いた行の処理量は複数sweepの合計。sweep_completedは最後のsweepの完了状態です。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("chokudai_copy_multi_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#else
    BS::NoOp debug_hook;
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State, Action> result = BS::run<State, Action>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " path_size=" << result.path.size()
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 6
#include "chokudai_copy_single_v29.hpp"

namespace {

// State保持型。各展開は必ず次のturnへ進みます。
// 盤面はState、出力用の1操作はActionへ分離します。履歴木はライブラリが保持します。
struct State {
    int value = 0;
};

// Actionはコピー構築できる型にします。祖先Stateは保存されません。
struct Action { int delta = 0; };

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = cs::ChokudaiCopySingle<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / beam_width / sweep_width や探索の制限を変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.beam_width = 1;         // 各turnに保持する未展開候補の上限
    param.hash_capacity = 0;     // 0は自動。実容量はbeam_widthを基準に調整されます。
    param.time_limit_ms = 0.0;
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.sweep_width = 1;     // 1スイープで各turnから展開する上限
    param.first_sweep_width = 0; // 0以下は通常幅。正なら初回だけこの幅（beam_width以下）。
    param.max_sweeps = 0;        // 0以下はスイープ数の制限なし

    // Runtime 付きの最大引数形を使っています。
    // 現在のturnは now.turn、スイープ番号は runtime.sweep で参照します。
    // runtime.elapsed_us は最後に時計を検査した時点の値です。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State, Action>& emit) {
        (void)runtime;
        // 履歴を使わない場合、この呼び出し自体が不要です。
        auto history = emit.history();
        if (!history.empty()) {
            const Action& previous = history.action(); // コピーせず直前の操作を参照。
            (void)previous;
            // history.parent()はさらに1つ前の履歴。初期状態の履歴はemptyです。
        }
        // 上のAction参照はここまで。push後はhistory.action()から再取得します。

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 有効解を終端にする場合は、param.expand_answers=falseを指定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool is_answer = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: cost, hash, is_answer, action, state。
        emit.push(next_cost, next_hash, is_answer, Action{1}, next);

        // Emitter::push_lazy の最大引数例: cost, hash, is_answer, make_action, make_state。
        // Action生成、State生成の順で各1回呼びます。保存不要なら両方とも呼びません。
        emit.push_lazy(
            next_cost + 10,
            static_cast<Hash>(next_hash + Hash{100}),
            is_answer,
            []() -> Action { return Action{1}; },
            [&now](const Action& action) -> State { return State{now.state.value + action.delta}; }
        );
    };

    // 独自DebugHookにはTurnEnd / SweepStartも通知しますが、CSVはSweepEnd単位です。
    // interval_ms <= 0.0なら毎sweep記録。正ならSweepEndで間隔を判定します。
    // RunStart / SearchEnd / RunEndと初回・未完了のSweepEndは必ず記録します。
    // 間引いた行の処理量は複数sweepの合計。sweep_completedは最後のsweepの完了状態です。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("chokudai_copy_single_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#else
    BS::NoOp debug_hook;
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State, Action> result = BS::run<State, Action>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " path_size=" << result.path.size()
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 7
#include "beam_delta_sub_v26.hpp"

namespace {

// Action保持型。各Actionで同turn内の継続か、次turnへの移動かを選びます。
// 盤面などの外部状態は move_forward / move_backward でユーザーが更新します。
struct Action {
    int delta = 0;
};

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = bs::Beam<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / beam_width / sub_beam_width や時間制限を変更します。
    BS::Param param{};
    param.apply_best_on_return = false; // trueなら、解があるとき最良解の外部Stateで返却します。
    param.max_turn = 0;
    param.beam_width = 1;         // 次turnの開始候補幅
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.sub_beam_width = 0;    // 0以下ならbeam_widthを使用

    int current_position = 0;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::NodeView& now, const BS::Runtime& runtime, BS::Emitter<Action>& emit) {
        (void)runtime;

        const Action action{1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool is_answer = (now.turn + 1 >= param.max_turn);

        // 最大引数例: cost, next, hash, is_answer, action。
        // 次turnの開始候補は、現在turn内の全sub層から生成した候補の中で比較されます。
        emit.push(next_cost, BS::Next::Turn, next_hash, is_answer, action);

        // 同じexpandから、同turn内を継続する候補も生成します。
        // この例ではSubを最大2回とし、各turnを1〜3回の遷移で完了させます。
        if (now.sub_step < 2) {
            emit.push(next_cost, BS::Next::Sub, next_hash, false, action);
        }
    };

    // Sub / TurnのどちらもAction単位で進めたり戻したりします。
    // 正常返却時は初期状態。apply_best_on_return=trueで解ありなら最良解の状態です。
    auto move_forward = [&current_position](const Action& action) {
        current_position += action.delta;
    };
    auto move_backward = [&current_position](const Action& action) {
        current_position -= action.delta;
    };

    // interval_ms <= 0.0 ではSubStepEnd / SearchEndを含む全イベントを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_delta_sub_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.turn, runtime.sub_step, runtime.sub_selection, runtime.turn_selection などを参照できます。
    };
#else
    BS::NoOp debug_hook;
#endif

    const Cost initial_cost = 0;
    // 初期Hashは省略時0。差分Hashを使う場合はinitial_costの次に初期値を渡します。

    BS::Result<Action> result = BS::run<Action>(
        param,
        initial_cost,
        expand,
        move_forward,
        move_backward,
        debug_hook
    );

    // result.pathはSub / Turnを含む全Action列です。
    // 完了turn t（0始まり）の区間は [turn_offsets[t], turn_offsets[t + 1]) です。
    // Sub途中で有効解になった場合、turn_offsets.back()以降は未完了turnのAction列です。
    // turn_offsetsはfound=falseなら空です。結果を使う前にfoundを確認してください。
    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn
              << " best_sub_step=" << result.best_sub_step
              << " best_depth=" << result.best_depth
              << " path_size=" << result.path.size()
              << " current_position=" << current_position
              << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 8
#include "beam_copy_sub_v26.hpp"

namespace {

// State保持型。各遷移で同turn内の継続か、次turnへの移動かを選びます。
// 盤面はState、出力用の1操作はActionへ分離します。履歴木はライブラリが保持します。
struct State {
    int value = 0;
};

// Actionはコピー構築できる型にします。祖先Stateは保存されません。
struct Action { int delta = 0; };

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = bs::Beam<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / beam_width / sub_beam_width や時間制限を変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.beam_width = 1;         // 次turnの開始候補幅
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.sub_beam_width = 0;    // 0以下ならbeam_widthを使用

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State, Action>& emit) {
        (void)runtime;
        // 履歴を使わない場合、この呼び出し自体が不要です。
        auto history = emit.history();
        if (!history.empty()) {
            const Action& previous = history.action(); // コピーせず直前の操作を参照。
            (void)previous;
            // history.parent()はさらに1つ前の履歴。初期状態の履歴はemptyです。
        }
        // 上のAction参照はここまで。push後はhistory.action()から再取得します。

        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool is_answer = (now.turn + 1 >= param.max_turn);

        // pushの最大引数例: cost, next, hash, is_answer, action, state。
        emit.push(next_cost, BS::Next::Turn, next_hash, is_answer, Action{1}, next);

        // push_lazyの最大引数例: cost, next, hash, is_answer, make_action, make_state。
        // Action生成、State生成の順で各1回呼びます。保存不要なら両方とも呼びません。
        // この例ではSubを最大2回とし、各turnを1〜3回の遷移で完了させます。
        if (now.sub_step < 2) {
            emit.push_lazy(
                next_cost,
                BS::Next::Sub,
                next_hash,
                false,
                []() -> Action { return Action{1}; },
                [&now](const Action& action) -> State { return State{now.state.value + action.delta}; }
            );
        }
    };

    // interval_ms <= 0.0 ではSubStepEnd / SearchEndを含む全イベントを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_copy_sub_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.turn, runtime.sub_step, runtime.sub_selection, runtime.turn_selection などを参照できます。
    };
#else
    BS::NoOp debug_hook;
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版のrunを使います。
    // 初期Hashを省略しても、use_hash_dedup=trueならHash付きpush / push_lazyの候補は重複排除されます。
    BS::Result<State, Action> result = BS::run<State, Action>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " path_size=" << result.path.size()
              << " best_turn=" << result.best_turn
              << " best_sub_step=" << result.best_sub_step
              << " best_depth=" << result.best_depth;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 9
#include "chokudai_copy_sub_v27.hpp"

namespace {

// State保持型。turn境界には未展開候補を残し、turn内は通常のビームでsub探索します。
// 盤面はState、出力用の1操作はActionへ分離します。履歴木はライブラリが保持します。
struct State {
    int value = 0;
};

// Actionはコピー構築できる型にします。祖先Stateは保存されません。
struct Action { int delta = 0; };

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = cs::ChokudaiCopySub<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / 各幅 / 探索の制限を変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.beam_width = 1;         // 各turnに保持する未展開の開始候補幅
    param.time_limit_ms = 0.0;
    param.time_check_interval = 64;
    param.continue_on_timeout = true; // 解未発見の時間切れでは救済探索を続けます。
    param.expand_answers = true; // 有効解も探索候補へ登録。終端にしたい問題ではfalse。
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.sweep_width = 1;    // 1turn・1スイープで取り出す開始候補数
    param.first_sweep_width = 0; // 0以下は通常幅。正なら初回だけこの幅（beam_width以下）。
    param.max_sweeps = 0;       // 通常スイープ数の上限。0以下は無制限
    param.sub_beam_width = 0;   // 0以下ならbeam_widthを使用

    // Runtime 付きの最大引数形を使っています。
    // now.turn / now.sub_stepで探索位置、runtime.sweepでスイープ番号を参照できます。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State, Action>& emit) {
        (void)runtime;
        // 履歴を使わない場合、この呼び出し自体が不要です。
        auto history = emit.history();
        if (!history.empty()) {
            const Action& previous = history.action(); // コピーせず直前の操作を参照。
            (void)previous;
            // history.parent()はさらに1つ前の履歴。初期状態の履歴はemptyです。
        }
        // 上のAction参照はここまで。push後はhistory.action()から再取得します。

        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool is_answer = (now.turn + 1 >= param.max_turn);

        // pushの最大引数例: cost, next, hash, is_answer, action, state。
        emit.push(next_cost, BS::Next::Turn, next_hash, is_answer, Action{1}, next);

        // push_lazyの最大引数例: cost, next, hash, is_answer, make_action, make_state。
        // Action生成、State生成の順で各1回呼びます。保存不要なら両方とも呼びません。
        // この例ではSubを最大2回とし、各turnを1〜3回の遷移で完了させます。
        if (now.sub_step < 2) {
            emit.push_lazy(
                next_cost,
                BS::Next::Sub,
                next_hash,
                false,
                []() -> Action { return Action{1}; },
                [&now](const Action& action) -> State { return State{now.state.value + action.delta}; }
            );
        }
    };

    // interval_ms <= 0.0 ではSweepStart / SubStepEnd / SearchEndを含む全イベントを記録します。
    // DebugRuntime.sweep_completedで、SweepEnd時の完走を確認できます。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("chokudai_copy_sub_stats.csv", 0.0);
#elif BEAM_SKELTON_USE_DEBUG_HOOK
    auto debug_hook = [](BS::EventType event_type, const BS::DebugRuntime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.sweep, runtime.pending_candidates, runtime.stop_reason などを参照できます。
    };
#else
    BS::NoOp debug_hook;
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版のrunを使います。
    // 初期Hashを省略しても、use_hash_dedup=trueならHash付きpush / push_lazyの候補は重複排除されます。
    BS::Result<State, Action> result = BS::run<State, Action>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " path_size=" << result.path.size()
              << " best_turn=" << result.best_turn
              << " best_sub_step=" << result.best_sub_step
              << " best_depth=" << result.best_depth;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#else
#error "BEAM_SKELTON_KIND must be an integer from 1 to 9."
#endif

int main() {
    return example_main();
}
