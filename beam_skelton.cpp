// beam_skelton_v19.cpp
//
// ビームサーチ6種類とchokudaiサーチ3種類を、1ファイル内で切り替えて試すためのスケルトンです。
// 使うライブラリは BEAM_SKELTON_KIND で選びます。
//
//   1: beam_delta_multi_v59.hpp       turn + Action保持型
//   2: beam_delta_single_v36.hpp      turn統一 + Action保持型。各手は必ず +1 turn
//   3: beam_copy_multi_v34.hpp        turn + State保持型
//   4: beam_copy_single_v30.hpp       turn統一 + State保持型。各手は必ず +1 turn
//   5: chokudai_copy_multi_v07.hpp    chokudai + State保持型。各手のstepを指定
//   6: chokudai_copy_single_v06.hpp   chokudai + State保持型。各手は必ず +1 turn
//   7: beam_delta_sub_v05.hpp        turn内sub探索 + Action保持型
//   8: beam_copy_sub_v04.hpp         turn内sub探索 + State保持型
//   9: chokudai_copy_sub_v03.hpp     chokudai + turn内sub探索 + State保持型
//
// 例:
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=1 beam_skelton_v19.cpp
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=7 beam_skelton_v19.cpp
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=8 beam_skelton_v19.cpp
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=9 -DBEAM_SKELTON_USE_CSV_HOOK=1 beam_skelton_v19.cpp
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
//   途中turnの finished=true は「解候補にする」フラグであり、自動的な展開停止ではありません。
//   通常ビームとchokudaiのsingle/multiでは、最良有効解を幅・Hash選別と独立して保存します。
//   pushのfalseは探索候補への棄却であり、解なしとは限りません。解の有無はfoundで確認します。
//   single/multiのcopy型のpush_lazyは解だけの保存でもmakerを呼びます。1呼び出しにつき高々1回です。
//   途中の終端状態を展開したくない場合だけ、Stateや外部状態を見てユーザー側で return してください。
//
// 通常ビームのturn別時間配分（1 / 2 / 3 / 4 / 7 / 8）:
//   time_limit_msを正にしてuse_turn_time_limit=trueにすると、残り時間を残りturn数へ配分します。
//   各turn開始時に締切を決め、Sub層の途中では更新しません。time_check_interval=1なら毎展開確認します。
//   turn期限後は未来候補があれば進み、なければ候補ができるか尽きるまで探索します。
//   turn期限だけでは保持幅を縮めず、次turnで通常探索へ戻ります。全体期限後は各版の時間超過処理です。
//   Runtimeのturn_time_limit_reachedは各turnでリセットされ、全体のtime_limit_reachedとは独立です。
//   chokudaiの3種類にはuse_turn_time_limitはありません。
//
// chokudai のmulti / single版（5 / 6）のパラメータとスイープについて:
//   beam_width は各turnに保持する未展開候補の上限、chokudai_width は1スイープで各turnから展開する上限です。
//   スイープは浅いturnから順に進み、残った候補を次のスイープへ持ち越します。
//   Hashによる重複排除は同じ到着turnの未展開候補が対象で、展開済みのHashは保持しません。
//   max_sweeps <= 0 は回数無制限、time_limit_ms <= 0.0 は時間無制限です。
//   finish_on_timeout=true は、解が未発見で時間切れになったときに、時間制限を超えて
//   chokudai_width=1 の追加スイープを高々1回行う設定です。
//   選別で棄却された有効解も解発見に含め、救済中なら現在のexpand終了後に打ち切ります。
//
// sub版（7 / 8 / 9）について:
//   push / push_lazy の末尾の Next::Sub は同turnの次sub層へ、Next::Turn は次turnの開始候補へ追加します。
//   delta subは親・部分木が近い候補をまとめて展開し、外部状態の往復を減らします。
//   不要Nodeはまとめて回収し、候補・最良解・現在地に必要な祖先は保持します。
//   nodes_capacityが正なら、回収しても枠が不足した場合にNodeLimitで終了します。
//   Next を省略すると Next::Turn です。同じexpandで両方を生成でき、それぞれ別の候補群で比較します。
//   beam_width は次turnの開始候補幅、sub_beam_width はturn内の各sub層の幅（0以下ならbeam_width）です。
//   now.turn は完了turn数、now.sub_step は現在turn内のSub遷移数、now.depth は経路全体の遷移数です。
//   Turn遷移でsub_stepは0へ戻ります。Sub遷移は各経路で必ず有限回となるようにしてください。
//   次sub層が空なら次turnへ進みます。例では各層からTurn候補を生成し、Sub遷移は最大2回にしています。
//   time_limit_ms は探索全体の制限です。時間超過時は次turn候補があればturn内探索を切り上げます。
//   次turn候補も有効解もない場合は、探索可能な候補が残っていれば制限時間を超えて探索を続けます。
//   9ではchokudai_width個までの開始候補からturn内を通常のビームで探索し、残りの開始候補は次スイープへ持ち越します。
//   9は時間超過かつ有効解ありで終了します。実行中のexpandは中断しません。finish_on_timeoutの設定はありません。
//   9のmax_sweepsは通常スイープの上限です。時間超過後、有効解未発見時の救済には適用しません。
//   finished=true はSub / Turnのどちらでも指定でき、規定turn数に達する前の有効解にも使えます。
//
// CSV Hook について:
//   BEAM_SKELTON_USE_CSV_HOOK=1 を指定すると、debug_hook に BS::CsvStatHook を使います。
//   0 の場合は、型名を明示したラムダHookを使います。
//   multi / single版は、ビームではturn単位、chokudaiではスイープ単位のイベントです。
//   sub版はSubStepEnd / TurnEndも通知し、chokudaiではSweepEndも通知します。CSVの列は方式ごとに異なります。

#ifndef BEAM_SKELTON_KIND
#define BEAM_SKELTON_KIND 1
#endif

#ifndef BEAM_SKELTON_USE_CSV_HOOK
#define BEAM_SKELTON_USE_CSV_HOOK 0
#endif

#include <cstdint>
#include <iostream>

#if BEAM_SKELTON_KIND == 1
#include "beam_delta_multi_v59.hpp"

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
    param.max_turn = 0;
    param.max_step = 1;
    param.beam_width = 1;
    param.nodes_capacity = 0;
    param.hash_capacity = 0;
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;
    param.capacity_policy = BS::CapacityPolicy::Safe;

    int current_position = 0;

    // expand は NodeView と Emitter を受け取ります。
    // IDE補完を使いやすくするため、引数型は auto ではなく明示しています。
    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::NodeView& now, const BS::Runtime& runtime, BS::Emitter<Action>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 途中turnの終端状態を展開したくない場合は、Stateや外部状態で判定してください。
        const Action action{1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1; // 1以上、param.max_step以下。到着turnがmax_turnを超える候補は棄却されます。
        const bool finished = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: action, cost, hash, step, finished。
        // step は int 固定です。hash は unsigned かつ Hash 以下の幅の型を渡します。
        emit.push(action, next_cost, next_hash, step, finished);
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
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、RunStart / 各TurnEnd / RunEnd を記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_delta_multi_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.best_cost, runtime.expanded_nodes などをログ出力できます。
    };
#endif

    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    BS::Result<Action> result = BS::run<Action>(
        param,
        initial_cost,
        initial_hash,
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
#include "beam_delta_single_v36.hpp"

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
    param.max_turn = 0;
    param.beam_width = 1;
    param.nodes_capacity = 0;
    param.hash_capacity = 0;
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;

    int current_position = 0;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::NodeView& now, const BS::Runtime& runtime, BS::Emitter<Action>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 途中turnの終端状態を展開したくない場合は、Stateや外部状態で判定してください。
        const Action action{1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool finished = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: action, cost, hash, finished。
        emit.push(action, next_cost, next_hash, finished);
    };

    // depth/turn統一 + Action型では、move callback は Action を受け取ります。
    auto move_forward = [&current_position](const Action& action) {
        current_position += action.delta;
    };
    auto move_backward = [&current_position](const Action& action) {
        current_position -= action.delta;
    };

    // DebugHook はラムダでも、標準の CsvStatHook でも指定できます。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、RunStart / 各TurnEnd / RunEnd を記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_delta_single_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#endif

    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    BS::Result<Action> result = BS::run<Action>(
        param,
        initial_cost,
        initial_hash,
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
#include "beam_copy_multi_v34.hpp"

namespace {

// State保持型。履歴や盤面など、結果として欲しい情報はStateに入れます。
struct State {
    int value = 0;
};

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
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 途中turnの終端状態を展開したくない場合は、Stateや外部状態で判定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1; // 1以上、param.max_step以下。到着turnがmax_turnを超える候補は棄却されます。
        const bool finished = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, step, finished。
        emit.push(next, next_cost, next_hash, step, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, step, finished, maker。
        // makerは探索候補または最良有効解の保存に必要な場合だけ同期的に1回呼ばれ、保存されません。
        emit.push_lazy(
            next_cost + 10,
            static_cast<Hash>(next_hash + Hash{100}),
            step,
            finished,
            [next]() { return next; }
        );
    };

    // DebugHook はラムダでも、標準の CsvStatHook でも指定できます。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、RunStart / 各TurnEnd / RunEnd を記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_copy_multi_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State> result = BS::run<State>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 4
#include "beam_copy_single_v30.hpp"

namespace {

// State保持型。各展開は必ず次のturnへ進みます。
// 履歴や盤面など、結果として欲しい情報はStateに入れます。
struct State {
    int value = 0;
};

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
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 途中turnの終端状態を展開したくない場合は、Stateや外部状態で判定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool finished = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, finished。
        emit.push(next, next_cost, next_hash, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, finished, maker。
        // makerは探索候補または最良有効解の保存に必要な場合だけ同期的に1回呼ばれ、保存されません。
        emit.push_lazy(
            next_cost + 10,
            static_cast<Hash>(next_hash + Hash{100}),
            finished,
            [next]() { return next; }
        );
    };

    // DebugHook はラムダでも、標準の CsvStatHook でも指定できます。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、RunStart / 各TurnEnd / RunEnd を記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_copy_single_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State> result = BS::run<State>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 5
#include "chokudai_copy_multi_v07.hpp"

namespace {

// State保持型。各展開で到着turnまでのstepを指定します。
// 履歴や盤面など、結果として欲しい情報はStateに入れます。
struct State {
    int value = 0;
};

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = cs::ChokudaiCopyMulti<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / max_step / beam_width / chokudai_width や探索の制限を変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.max_step = 1;
    param.beam_width = 1;         // 各turnに保持する未展開候補の上限
    param.hash_capacity = 0;     // 0は自動。実容量はbeam_widthを基準に調整されます。
    param.time_limit_ms = 0.0;
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.chokudai_width = 1;     // 1スイープで各turnから展開する上限
    param.max_sweeps = 0;        // 0以下はスイープ数の制限なし
    param.finish_on_timeout = false;

    // Runtime 付きの最大引数形を使っています。
    // 現在のturnは now.turn、スイープ番号は runtime.sweep で参照します。
    // runtime.elapsed_us は最後に時計を検査した時点の値です。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 途中turnの終端状態を展開したくない場合は、Stateや外部状態で判定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1; // 1以上、param.max_step以下。到着turnがmax_turnを超える候補は棄却されます。
        const bool finished = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, step, finished。
        emit.push(next, next_cost, next_hash, step, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, step, finished, maker。
        // makerは探索候補または最良有効解の保存に必要な場合だけ同期的に1回呼ばれ、保存されません。
        emit.push_lazy(
            next_cost + 10,
            static_cast<Hash>(next_hash + Hash{100}),
            step,
            finished,
            [next]() { return next; }
        );
    };

    // DebugHook のイベントは RunStart / SweepStart / SweepEnd / RunEnd です。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、RunStart / 各SweepEnd / RunEnd を記録します。
    // 時間切れで途中終了したSweepEndも通知され、runtime.sweep_completedで完走したか判別できます。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("chokudai_copy_multi_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State> result = BS::run<State>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 6
#include "chokudai_copy_single_v06.hpp"

namespace {

// State保持型。各展開は必ず次のturnへ進みます。
// 履歴や盤面など、結果として欲しい情報はStateに入れます。
struct State {
    int value = 0;
};

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = cs::ChokudaiCopySingle<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / beam_width / chokudai_width や探索の制限を変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.beam_width = 1;         // 各turnに保持する未展開候補の上限
    param.hash_capacity = 0;     // 0は自動。実容量はbeam_widthを基準に調整されます。
    param.time_limit_ms = 0.0;
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.chokudai_width = 1;     // 1スイープで各turnから展開する上限
    param.max_sweeps = 0;        // 0以下はスイープ数の制限なし
    param.finish_on_timeout = false;

    // Runtime 付きの最大引数形を使っています。
    // 現在のturnは now.turn、スイープ番号は runtime.sweep で参照します。
    // runtime.elapsed_us は最後に時計を検査した時点の値です。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        // max_turnの状態はライブラリ側で展開しないため、ここでの最大ターン判定は不要です。
        // 途中turnの終端状態を展開したくない場合は、Stateや外部状態で判定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool finished = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, finished。
        emit.push(next, next_cost, next_hash, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, finished, maker。
        // makerは探索候補または最良有効解の保存に必要な場合だけ同期的に1回呼ばれ、保存されません。
        emit.push_lazy(
            next_cost + 10,
            static_cast<Hash>(next_hash + Hash{100}),
            finished,
            [next]() { return next; }
        );
    };

    // DebugHook のイベントは RunStart / SweepStart / SweepEnd / RunEnd です。
    // CsvStatHook の第2引数 interval_ms <= 0.0 の場合、RunStart / 各SweepEnd / RunEnd を記録します。
    // 時間切れで途中終了したSweepEndも通知され、runtime.sweep_completedで完走したか判別できます。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("chokudai_copy_single_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
    };
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版の run を使います。
    // Hashなし初期状態版でも、use_hash_dedup=true ならHash付きpush/push_lazyの候補を重複排除します。
    // 初期Hashの指定は、生成候補の重複排除を有効にするための必須条件ではありません。
    BS::Result<State> result = BS::run<State>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
              << " best_turn=" << result.best_turn;
    if (result.best_state.has_value()) {
        std::cout << " best_state.value=" << result.best_state->value;
    }
    std::cout << '\n';
    return 0;
}

} // namespace

#elif BEAM_SKELTON_KIND == 7
#include "beam_delta_sub_v05.hpp"

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
    param.max_turn = 0;
    param.beam_width = 1;         // 次turnの開始候補幅
    param.nodes_capacity = 0;    // 不要Nodeは回収・再利用。正ならrootを含むNode数の上限
    param.time_limit_ms = 0.0;
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。   // 探索全体の制限。0以下は無制限
    param.time_check_interval = 64;
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
        const bool finished = (now.turn + 1 >= param.max_turn);

        // 最大引数例: action, cost, hash, finished, next。
        // 次turnの開始候補は、現在turn内の全sub層から生成した候補の中で比較されます。
        emit.push(action, next_cost, next_hash, finished, BS::Next::Turn);

        // 同じexpandから、同turn内を継続する候補も生成します。
        // この例ではSubを最大2回とし、各turnを1〜3回の遷移で完了させます。
        if (now.sub_step < 2) {
            emit.push(action, next_cost, next_hash, false, BS::Next::Sub);
        }
    };

    // Sub / TurnのどちらもAction単位で進めたり戻したりします。
    // runが正常に返るとき、外部状態は初期状態に戻ります。
    auto move_forward = [&current_position](const Action& action) {
        current_position += action.delta;
    };
    auto move_backward = [&current_position](const Action& action) {
        current_position -= action.delta;
    };

    // interval_ms <= 0.0 ではRunStart / 各SubStepEnd / 各TurnEnd / RunEndを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_delta_sub_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.turn, runtime.sub_step, runtime.sub_stats, runtime.turn_stats などを参照できます。
    };
#endif

    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    BS::Result<Action> result = BS::run<Action>(
        param,
        initial_cost,
        initial_hash,
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
#include "beam_copy_sub_v04.hpp"

namespace {

// State保持型。各遷移で同turn内の継続か、次turnへの移動かを選びます。
// 履歴や盤面など、結果として欲しい情報はStateに入れます。
struct State {
    int value = 0;
};

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
    param.use_turn_time_limit = false; // 正の全体制限を残りturnへ配分する場合はtrue。   // 探索全体の制限。0以下は無制限
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.sub_beam_width = 0;    // 0以下ならbeam_widthを使用

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool finished = (now.turn + 1 >= param.max_turn);

        // pushの最大引数例: state, cost, hash, finished, next。
        emit.push(next, next_cost, next_hash, finished, BS::Next::Turn);

        // push_lazyの最大引数例: cost, hash, finished, maker, next。
        // makerは候補または最良解として保存するときだけ同期的に呼ばれ、保存されません。
        // この例ではSubを最大2回とし、各turnを1〜3回の遷移で完了させます。
        if (now.sub_step < 2) {
            emit.push_lazy(
                next_cost,
                next_hash,
                false,
                [&now]() { return State{now.state.value + 1}; },
                BS::Next::Sub
            );
        }
    };

    // interval_ms <= 0.0 ではRunStart / 各SubStepEnd / 各TurnEnd / RunEndを記録します。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("beam_copy_sub_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.turn, runtime.sub_step, runtime.sub_stats, runtime.turn_stats などを参照できます。
    };
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版のrunを使います。
    // 初期Hashを省略しても、use_hash_dedup=trueならHash付きpush / push_lazyの候補は重複排除されます。
    BS::Result<State> result = BS::run<State>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
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
#include "chokudai_copy_sub_v03.hpp"

namespace {

// State保持型。turn境界には未展開候補を残し、turn内は通常のビームでsub探索します。
// 履歴や盤面など、結果として欲しい情報はStateに入れます。
struct State {
    int value = 0;
};

int example_main() {
    using Cost = long long;
    using Hash = std::uint64_t;
    using BS = cs::ChokudaiCopySub<Cost, Hash>;

    // Param は現在のデフォルト値で明示的に埋めています。
    // 実探索では max_turn / 各幅 / 探索の制限を変更します。
    BS::Param param{};
    param.max_turn = 0;
    param.beam_width = 1;         // 各turnに保持する未展開の開始候補幅
    param.time_limit_ms = 0.0;   // 探索全体の制限。0以下は無制限
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.chokudai_width = 1;    // 1turn・1スイープで取り出す開始候補数
    param.max_sweeps = 0;       // 通常スイープ数の上限。0以下は無制限
    param.sub_beam_width = 0;   // 0以下ならbeam_widthを使用

    // Runtime 付きの最大引数形を使っています。
    // now.turn / now.sub_stepで探索位置、runtime.sweepでスイープ番号を参照できます。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool finished = (now.turn + 1 >= param.max_turn);

        // pushの最大引数例: state, cost, hash, finished, next。
        emit.push(next, next_cost, next_hash, finished, BS::Next::Turn);

        // push_lazyの最大引数例: cost, hash, finished, maker, next。
        // makerは候補または最良解として保存するときだけ同期的に呼ばれ、保存されません。
        // この例ではSubを最大2回とし、各turnを1〜3回の遷移で完了させます。
        if (now.sub_step < 2) {
            emit.push_lazy(
                next_cost,
                next_hash,
                false,
                [&now]() { return State{now.state.value + 1}; },
                BS::Next::Sub
            );
        }
    };

    // interval_ms <= 0.0 ではRunStart / 各SubStepEnd / 各TurnEnd / 各SweepEnd / RunEndを記録します。
    // multi / single版のSweepStartイベントやruntime.sweep_completedはありません。
#if BEAM_SKELTON_USE_CSV_HOOK
    BS::CsvStatHook debug_hook("chokudai_copy_sub_stats.csv", 0.0);
#else
    auto debug_hook = [](BS::EventType event_type, const BS::Runtime& runtime) {
        (void)event_type;
        (void)runtime;
        // runtime.sweep, runtime.pending_start_candidates, runtime.stop_reason などを参照できます。
    };
#endif

    const State initial_state{0};
    const Cost initial_cost = 0;
    const Hash initial_hash = Hash{0};

    // 最大引数の例として、Hashあり初期状態版のrunを使います。
    // 初期Hashを省略しても、use_hash_dedup=trueならHash付きpush / push_lazyの候補は重複排除されます。
    BS::Result<State> result = BS::run<State>(
        param,
        initial_state,
        initial_cost,
        initial_hash,
        expand,
        debug_hook
    );

    std::cout << "found=" << result.found
              << " best_cost=" << result.best_cost
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
