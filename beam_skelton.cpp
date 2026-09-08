// beam_skelton_v08.cpp
//
// ビームサーチ4種類とchokudaiサーチ2種類を、1ファイル内で切り替えて試すためのスケルトンです。
// 使うライブラリは BEAM_SKELTON_KIND で選びます。
//
//   1: beam_delta_multi_v55.hpp       turn + Action保持型
//   2: beam_delta_single_v32.hpp      turn統一 + Action保持型。各手は必ず +1 turn
//   3: beam_copy_multi_v32.hpp        turn + State保持型
//   4: beam_copy_single_v28.hpp       turn統一 + State保持型。各手は必ず +1 turn
//   5: chokudai_copy_multi_v01.hpp    chokudai + State保持型。各手のstepを指定
//   6: chokudai_copy_single_v01.hpp   chokudai + State保持型。各手は必ず +1 turn
//
// 例:
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=1 beam_skelton_v08.cpp
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=5 beam_skelton_v08.cpp
//   g++ -std=gnu++20 -O2 -DBEAM_SKELTON_KIND=6 -DBEAM_SKELTON_USE_CSV_HOOK=1 beam_skelton_v08.cpp
//
// 注意:
//   使うヘッダをこのファイルと同じディレクトリに置くか、コンパイル時に -I で場所を指定してください。
//   ビーム4ヘッダは同じ namespace bs / bs::Beam を使うため、#if で1本だけ include します。
//   chokudaiの型は cs::ChokudaiCopyMulti / cs::ChokudaiCopySingle です。ここでも選択した1本を使います。
//   実探索で使うときは、Param の値と expand の中身を問題に合わせて書き換えてください。
//   Param は現在のデフォルト値で明示的に埋めています。そのため、そのまま実行すると max_turn=0 です。
//
// max_turn について:
//   Emitter::push で max_turn を超える到着候補を出しても、ライブラリ側で棄却されます。
//   そのためユーザーが必ず now.turn >= param.max_turn を判定しないと壊れるわけではありません。
//   ビーム版では、max_turn 到達後の無駄な候補生成を避ける目的で expand の先頭に早期 return を置きます。
//   chokudai版では max_turn の状態はライブラリ側で展開しないため、この早期 return は不要です。
//   finished=true は「解候補にする」フラグであり、自動的な展開停止ではありません。
//
// chokudai のパラメータとスイープについて:
//   beam_width は各turnに保持する未展開候補の上限、chokudai_width は1スイープで各turnから展開する上限です。
//   スイープは浅いturnから順に進み、残った候補を次のスイープへ持ち越します。
//   max_sweeps <= 0 は回数無制限、time_limit_ms <= 0.0 は時間無制限です。
//   finish_on_timeout=true は、解が未発見で時間切れになったときに、時間制限を超えて
//   chokudai_width=1 の追加スイープを高々1回行う設定です。
//
// CSV Hook について:
//   BEAM_SKELTON_USE_CSV_HOOK=1 を指定すると、debug_hook に BS::CsvStatHook を使います。
//   0 の場合は、型名を明示したラムダHookを使います。
//   ビーム版はturn単位、chokudai版はスイープ単位のイベントです。CSVの列もライブラリごとに異なります。

#ifndef BEAM_SKELTON_KIND
#define BEAM_SKELTON_KIND 1
#endif

#ifndef BEAM_SKELTON_USE_CSV_HOOK
#define BEAM_SKELTON_USE_CSV_HOOK 0
#endif

#include <cstdint>
#include <iostream>

#if BEAM_SKELTON_KIND == 1
#include "beam_delta_multi_v55.hpp"

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

        // max_turn超過候補はライブラリ側でも棄却されます。
        // このreturnは必須ではありませんが、採用されない候補の生成を避けるために置いています。
        if (now.turn >= param.max_turn) return;

        const Action action{1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1;
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
#include "beam_delta_single_v32.hpp"

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
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;

    int current_position = 0;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::NodeView& now, const BS::Runtime& runtime, BS::Emitter<Action>& emit) {
        (void)runtime;

        // max_turn超過候補はライブラリ側でも棄却されます。
        // このreturnは必須ではありませんが、採用されない候補の生成を避けるために置いています。
        if (now.turn >= param.max_turn) return;

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
#include "beam_copy_multi_v32.hpp"

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
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        // max_turn超過候補はライブラリ側でも棄却されます。
        // このreturnは必須ではありませんが、採用されない候補の生成を避けるために置いています。
        if (now.turn >= param.max_turn) return;

        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1;
        const bool finished = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, step, finished。
        emit.push(next, next_cost, next_hash, step, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, step, finished, maker。
        // maker は候補が受理可能な場合だけ同期的に呼ばれ、保存されません。
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

    // Hashあり初期状態版の run を使います。
    // Hashなし初期状態版もありますが、重複排除を使う場合はHashあり版を使います。
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
#include "beam_copy_single_v28.hpp"

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
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;

    // Runtime 付きの最大引数形を使っています。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        // max_turn超過候補はライブラリ側でも棄却されます。
        // このreturnは必須ではありませんが、採用されない候補の生成を避けるために置いています。
        if (now.turn >= param.max_turn) return;

        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool finished = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, finished。
        emit.push(next, next_cost, next_hash, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, finished, maker。
        // maker は候補が受理可能な場合だけ同期的に呼ばれ、保存されません。
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

    // Hashあり初期状態版の run を使います。
    // Hashなし初期状態版もありますが、重複排除を使う場合はHashあり版を使います。
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
#include "chokudai_copy_multi_v01.hpp"

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
    param.hash_capacity = 0;
    param.time_limit_ms = 0.0;
    param.time_check_interval = 64;
    param.max_turn_is_answer = true;
    param.use_hash_dedup = true;
    param.print_warnings = true;
    param.chokudai_width = 1;     // 1スイープで各turnから展開する上限
    param.max_sweeps = 0;        // 0以下はスイープ数の制限なし
    param.finish_on_timeout = false;

    // Runtime 付きの最大引数形を使っています。
    // 現在のturnは now.turn、スイープ番号は runtime.sweep で参照します。
    // runtime.elapsed_us は最後に時計を検査した時点の値です。
    auto expand = [&param](const BS::StateView<State>& now, const BS::Runtime& runtime, BS::Emitter<State>& emit) {
        (void)runtime;

        // max_turnの状態は展開されないため、now.turn >= param.max_turn の早期returnは不要です。
        // 途中のturnでfinishedにした状態も展開されます。そこで止める場合はState等で判定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const int step = 1; // 1以上、param.max_step以下。到着turnがmax_turnを超える候補は棄却されます。
        const bool finished = (now.turn + step >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, step, finished。
        emit.push(next, next_cost, next_hash, step, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, step, finished, maker。
        // maker は候補が受理可能な場合だけ同期的に呼ばれ、保存されません。
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

    // Hashあり初期状態版の run を使います。
    // Hashなし初期状態版もありますが、重複排除を使う場合はHashあり版を使います。
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
#include "chokudai_copy_single_v01.hpp"

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
    param.hash_capacity = 0;
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

        // max_turnの状態は展開されないため、now.turn >= param.max_turn の早期returnは不要です。
        // 途中のturnでfinishedにした状態も展開されます。そこで止める場合はState等で判定してください。
        const State next{now.state.value + 1};
        const Cost next_cost = now.cost + 1;
        const Hash next_hash = static_cast<Hash>(now.hash + Hash{1});
        const bool finished = (now.turn + 1 >= param.max_turn);

        // Emitter::push の最大引数例: state, cost, hash, finished。
        emit.push(next, next_cost, next_hash, finished);

        // Emitter::push_lazy の最大引数例: cost, hash, finished, maker。
        // maker は候補が受理可能な場合だけ同期的に呼ばれ、保存されません。
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

    // Hashあり初期状態版の run を使います。
    // Hashなし初期状態版もありますが、重複排除を使う場合はHashあり版を使います。
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

#else
#error "BEAM_SKELTON_KIND must be 1, 2, 3, 4, 5, or 6."
#endif

int main() {
    return example_main();
}
