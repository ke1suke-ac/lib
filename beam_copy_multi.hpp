#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace bs {

// ============================================================
// 公開concept
// ============================================================

// コスト最小化専用の軽量数値型である。
// 最大化問題では、ユーザー側で cost = -score のように変換する。
// floating point Cost も使えるが、NaN は比較順序を壊すため候補として棄却される。
template<class T>
concept CostType =
    ((std::integral<T> && std::is_signed_v<T>) || std::floating_point<T>) &&
    (!std::same_as<T, bool>);

// Hash重複排除用の型。64bit以下の符号なし整数のみを受け付ける。
template<class T>
concept HashType =
    std::integral<T> &&
    std::is_unsigned_v<T> &&
    (!std::same_as<T, bool>) &&
    (sizeof(T) <= sizeof(std::uint64_t));

// Emitter::push の引数誤用を減らすため、hash/step/bool/stateを概念で分ける。
template<class H, class Hash>
concept CompatibleHashArg =
    HashType<Hash> &&
    HashType<std::remove_cvref_t<H>> &&
    std::convertible_to<std::remove_cvref_t<H>, Hash> &&
    (sizeof(std::remove_cvref_t<H>) <= sizeof(Hash));

template<class S>
concept StepArg = std::same_as<std::remove_cvref_t<S>, int>;

template<class B>
concept BoolArg = std::same_as<std::remove_cvref_t<B>, bool>;

template<class U, class State>
concept StateArg = std::same_as<std::remove_cvref_t<U>, State>;

// ============================================================
// detail: 低レベル補助関数
// ============================================================

namespace detail {

// Beamに依存しない低レベル補助関数。
inline int ceil_pow2(int x) {
    int p = 1;
    while (p < x) p <<= 1;
    return p;
}

inline std::uint64_t mix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

template<HashType Hash>
inline std::uint64_t hash_to_u64(Hash x) noexcept {
    return static_cast<std::uint64_t>(x);
}

template<CostType Cost>
inline bool is_valid_cost(Cost cost) {
    if constexpr (std::floating_point<Cost>) {
        return !std::isnan(cost);
    } else {
        (void)cost;
        return true;
    }
}

// doubleのms指定を内部比較用のマイクロ秒へ丸める。正の極小値は1usとして扱う。
inline std::int64_t time_limit_to_microseconds(double time_limit_ms) {
    if (!(time_limit_ms > 0.0)) return 0;

    const double us = time_limit_ms * 1000.0;
    constexpr double max_us = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (us >= max_us) return std::numeric_limits<std::int64_t>::max();

    return std::max<std::int64_t>(1, static_cast<std::int64_t>(us + 0.5));
}

inline std::int64_t now_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace detail

// ============================================================
// 公開APIと探索本体
// ============================================================

template<CostType Cost = long long, HashType Hash = std::uint64_t>
struct Beam {
    using cost_type = Cost;
    using hash_type = Hash;

private:
    template<class State>
    class Engine;

public:


// 探索パラメータ。
// run 開始時に max_turn は 0 以上、beam_width は 1 以上、
// max_step は [1, max(1,max_turn)] に丸められる。
// 解候補は finished=true の候補、または max_turn_is_answer=true のときの max_turn 到達候補。
// finished=true は解候補として採用可能という印であり、自動的な展開停止ではない。
// 終端状態をそれ以上展開したくない場合は、ユーザー側の expand で候補を出さないこと。
// time_limit_ms は soft limit。expand実行中は中断しない。
// 制限到達を検出したturnでは、通常展開を止め、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
struct Param {
    int max_turn = 0;               // 探索終了ターン。候補は 0..max_turn に到着できる。
    int max_step = 1;               // 1回の展開で進める最大ターン。
    int beam_width = 1;             // 各到着ターンに残す候補数。

    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。過大値もbeam_width基準で丸める。

    double time_limit_ms = 0.0;    // 0なら無制限。正なら制限到達検出後に最良1本の縮退探索へ移るsoft limit。
    int time_check_interval = 64;   // 通常ビーム探索中、何ノード展開ごとに時間を見るか。expand中は中断しない。
    bool max_turn_is_answer = true; // trueならmax_turn到達候補も解候補にする。
    bool use_hash_dedup = true;     // trueならHash付きpushで同一到着ターン内の重複排除を行う。Hashなしpushは対象外。
    bool print_warnings = true;     // 現状はhash表満杯など、結果解釈に注意が必要な状態をstderrへ警告する。
};

// Hookの発火タイミング。
// CsvStatHookはRunStart/TurnEnd/RunEndだけを記録し、TurnStart/BestUpdate行は出力しない。
// 独自Hookは全イベントを受け取れる。BestUpdate時のsnapshot系統計は直前値のまま渡される。
enum class EventType {
    RunStart,
    RunEnd,
    TurnStart,
    TurnEnd,
    BestUpdate,
};

// expand に渡す現在状態の読み取りビュー。
// state はライブラリが保持する状態への const 参照であり、ユーザー側で直接変更しない。
// has_hash=false のとき hash は重複排除に使われていない。ユーザー側でも has_hash を確認して扱うこと。
template<class State>
struct StateView {
    int turn = 0;
    Cost cost{};
    const State& state;
    Hash hash{};
    bool has_hash = false;

    StateView(int turn_, Cost cost_, const State& state_, Hash hash_, bool has_hash_) noexcept
        : turn(turn_), cost(cost_), state(state_), hash(hash_), has_hash(has_hash_) {}
};

// Hookから観測できる実行時情報。
// expanded/generated/accepted/selected/best などの累積カウンタは常に更新される。
// live_*、selected_*、selector_*、time_remaining_ms などのsnapshotは、
// DebugHook有効時に各イベントで更新され、RunEndだけはNoOpでも最終値へ更新される。
// 途中経過のsnapshotを観測したい場合はDebugHookを指定すること。
struct Runtime {
    // 基本情報。
    int turn = 0;                 // 現在展開中のターン。RunEndでは最後に展開したターンを表す。
    int materialized_turn = 0;    // frontierとして実体化済みの最大ターン。探索完了確認にはこちらを見る。
    int max_turn = 0;
    int max_step = 1;
    int beam_width = 0;
    int hash_capacity = 0;

    // 各ターンの幅と、探索全体での最大幅。
    int active_width = 0;         // このターンで実際にexpandした状態数。縮退探索では1本だけになり得る。
    int selected_width = 0;       // 直近materializeでfrontierへ移した候補数。縮退探索中は最大1。
    int max_active_width = 0;
    int max_selected_width = 0;
    int beam_saturated_turns = 0; // selected_width が beam_width に到達したターン数。

    // 候補数。
    long long expanded_nodes = 0;
    long long generated_candidates = 0; // emit.push/push_lazy呼び出し総数。invalid step も含む。
    long long accepted_candidates = 0;  // Selectorに一時的にでも入り、ライブラリ内部でState保存まで進んだ候補数。push_lazyではmaker()呼び出し数に近い。通常pushでユーザー側が事前生成したState数ではない。
    long long selected_nodes = 0;      // 最終的にfrontierへ実体化された候補数。

    // 棄却・置換統計。
    long long pruned_by_width = 0;     // 満杯Selectorのworst cost以上で、hash lookup前に即棄却された候補数。同costは既存候補を優先する。
    long long pruned_by_hash = 0;      // width事前判定通過後、同一hashで改善しないか、hash表都合で棄却された候補数。
    long long duplicate_by_hash = 0;   // width事前判定通過後、同一hash候補を見つけた回数。改善置換も含む。
    long long replaced_by_width = 0;   // beam幅競争によりworst候補を置換した回数。hash同一置換は含まない。
    long long replaced_by_hash = 0;    // 同一hashのより良い候補で既存候補を置換した回数。
    long long invalid_candidates = 0;
    long long hash_table_full = 0;
    long long hash_rebuild_count = 0;

    // 保持中状態数。
    int live_nodes = 0;            // frontier と selector 内に保持中の状態数。
    int pending_candidates = 0;    // selector 内に保持中の候補数。
    int max_live_nodes = 0;
    int max_pending_candidates = 0;

    // CSVデバッグ用の軽量スナップショット。
    // DebugHook有効時、またはRunEndの最終Result用に更新される。
    double time_limit_ms = 0.0;
    double time_remaining_ms = -1.0;
    std::int64_t elapsed_us = 0;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;

    int selector_nonempty_count = 0;
    int selector_max_size = 0;
    int selector_full_count = 0;

    Cost live_best_cost{};         // frontier + selector 全体の保持中候補の最良cost。確定best_costとは別。
    Cost live_worst_cost{};
    double live_avg_cost = 0.0;
    int live_best_turn = 0;

    Cost selected_best_cost{};     // 直近materializeでfrontierへ進んだ候補のcost分布。縮退探索では1件分。
    Cost selected_worst_cost{};
    double selected_avg_cost = 0.0;
    double selected_step_avg = 0.0;
    int selected_step_max = 0;
    int width_pruned_cost_count = 0;
    Cost width_pruned_best_cost{};
    double pruned_best_gap = 0.0;

    int turns_since_best_update = 0;
    double ms_since_best_update = 0.0;

    bool found = false;
    Cost best_cost{};
    int best_turn = 0;
    int best_update_count = 0;
};

// デフォルトHook。何もしない。
struct NoOp {
    void operator()(EventType, const Runtime&) const noexcept {}
};

// CSV出力Hook。
// RunStart/TurnEnd/RunEnd の統計をCSVに出力する。
// TurnStart/BestUpdate行は出力しない。
// 行はメモリ上に保持し、RunEndで一括してファイルへ書き出す。
struct CsvStatHook {
    // RowはRuntimeのスナップショットに、CSV固有の期間統計・派生統計を足したもの。
    struct Row : Runtime {
        Row() = default;
        explicit Row(const Runtime& runtime) : Runtime(runtime) {}

        const char* event = "";
        double elapsed_ms = 0.0;
        double period_ms = 0.0;
        int period_turns = 0;

        int selector_total_candidates = 0;
        double selector_avg_size = 0.0;
        double selector_full_rate = 0.0;

        long long period_expanded_nodes = 0;
        long long period_generated_candidates = 0;
        long long period_accepted_candidates = 0;
        long long period_selected_nodes = 0;
        long long period_pruned_by_width = 0;
        long long period_pruned_by_hash = 0;
        long long period_duplicate_by_hash = 0;
        long long period_replaced_by_width = 0;
        long long period_replaced_by_hash = 0;
        long long period_hash_rebuild_count = 0;
        int period_best_updates = 0;

        double expand_per_sec = 0.0;
        double generate_per_sec = 0.0;
        double accept_per_sec = 0.0;
        double select_per_sec = 0.0;
        double accept_rate = 0.0;
        double width_prune_rate = 0.0;
        double hash_prune_rate = 0.0;
    };

    std::string filename;
    std::int64_t interval_us = 1;
    std::vector<Row> rows;
    std::int64_t next_record_us = 0;
    bool started = false;

    explicit CsvStatHook(std::string csv_filename = "beam_stat.csv", double interval_ms = 50.0)
        : filename(std::move(csv_filename)),
          interval_us(interval_ms <= 0.0
              ? 0
              : std::max<std::int64_t>(1, static_cast<std::int64_t>(interval_ms * 1000.0 + 0.5))) {}

    void operator()(EventType event_type, const Runtime& runtime) {
        switch (event_type) {
            case EventType::RunStart:
                rows.clear();
                started = true;
                next_record_us = runtime.elapsed_us + interval_us;
                push_row(event_type, runtime);
                return;

            case EventType::TurnEnd:
                if (started && (interval_us == 0 || runtime.elapsed_us >= next_record_us)) {
                    push_row(event_type, runtime);
                    if (interval_us > 0) next_record_us = runtime.elapsed_us + interval_us;
                }
                return;

            case EventType::RunEnd:
                if (started) {
                    push_row(event_type, runtime);
                    write_csv();
                    started = false;
                }
                return;

            case EventType::TurnStart:
            case EventType::BestUpdate:
                // CsvStatHookではTurnStart/BestUpdate行は出さない。
                // それらの情報はTurnEnd/RunEndの累積値で確認する。
                return;
        }
    }

private:
    static double elapsed_ms(std::int64_t elapsed_us) {
        return static_cast<double>(elapsed_us) * 0.001;
    }

    static const char* event_name(EventType event_type) {
        switch (event_type) {
            case EventType::RunStart: return "RunStart";
            case EventType::RunEnd: return "RunEnd";
            case EventType::TurnStart: return "TurnStart";
            case EventType::TurnEnd: return "TurnEnd";
            case EventType::BestUpdate: return "BestUpdate";
        }
        return "Unknown";
    }

    void fill_derived(Row& row) const {
        row.selector_total_candidates = row.pending_candidates;
        row.selector_avg_size = row.selector_nonempty_count > 0
            ? static_cast<double>(row.selector_total_candidates) / row.selector_nonempty_count
            : 0.0;
        row.selector_full_rate = row.selector_nonempty_count > 0
            ? static_cast<double>(row.selector_full_count) / row.selector_nonempty_count
            : 0.0;
    }

    void push_row(EventType event_type, const Runtime& runtime) {
        Row row(runtime);
        row.event = event_name(event_type);
        row.elapsed_ms = elapsed_ms(runtime.elapsed_us);
        fill_derived(row);

        if (!rows.empty()) {
            const Row& prev = rows.back();
            row.period_ms = row.elapsed_ms - prev.elapsed_ms;
            row.period_turns = row.materialized_turn - prev.materialized_turn;
            row.period_expanded_nodes = row.expanded_nodes - prev.expanded_nodes;
            row.period_generated_candidates = row.generated_candidates - prev.generated_candidates;
            row.period_accepted_candidates = row.accepted_candidates - prev.accepted_candidates;
            row.period_selected_nodes = row.selected_nodes - prev.selected_nodes;
            row.period_pruned_by_width = row.pruned_by_width - prev.pruned_by_width;
            row.period_pruned_by_hash = row.pruned_by_hash - prev.pruned_by_hash;
            row.period_duplicate_by_hash = row.duplicate_by_hash - prev.duplicate_by_hash;
            row.period_replaced_by_width = row.replaced_by_width - prev.replaced_by_width;
            row.period_replaced_by_hash = row.replaced_by_hash - prev.replaced_by_hash;
            row.period_hash_rebuild_count = row.hash_rebuild_count - prev.hash_rebuild_count;
            row.period_best_updates = row.best_update_count - prev.best_update_count;
            if (row.period_ms > 0.0) {
                row.expand_per_sec = static_cast<double>(row.period_expanded_nodes) * 1000.0 / row.period_ms;
                row.generate_per_sec = static_cast<double>(row.period_generated_candidates) * 1000.0 / row.period_ms;
                row.accept_per_sec = static_cast<double>(row.period_accepted_candidates) * 1000.0 / row.period_ms;
                row.select_per_sec = static_cast<double>(row.period_selected_nodes) * 1000.0 / row.period_ms;
            }
            if (row.period_generated_candidates > 0) {
                const double denom = static_cast<double>(row.period_generated_candidates);
                row.accept_rate = static_cast<double>(row.period_accepted_candidates) / denom;
                row.width_prune_rate = static_cast<double>(row.period_pruned_by_width) / denom;
                row.hash_prune_rate = static_cast<double>(row.period_pruned_by_hash) / denom;
            }
        }

        rows.push_back(row);
    }

    void write_csv() const {
        std::ostringstream ofs;
        ofs << "event,elapsed_ms,turn,materialized_turn,max_turn,max_step,beam_width,hash_capacity,";
        ofs << "active_width,selected_width,max_active_width,max_selected_width,beam_saturated_turns,";
        ofs << "expanded_nodes,generated_candidates,accepted_candidates,selected_nodes,";
        ofs << "pruned_by_width,pruned_by_hash,duplicate_by_hash,replaced_by_width,replaced_by_hash,invalid_candidates,hash_table_full,hash_rebuild_count,";
        ofs << "live_nodes,pending_candidates,max_live_nodes,max_pending_candidates,";
        ofs << "time_limit_ms,time_remaining_ms,";
        ofs << "selector_nonempty_count,selector_total_candidates,selector_max_size,selector_full_count,selector_avg_size,selector_full_rate,";
        ofs << "live_best_cost,live_worst_cost,live_avg_cost,live_best_turn,";
        ofs << "selected_best_cost,selected_worst_cost,selected_avg_cost,selected_step_avg,selected_step_max,";
        ofs << "width_pruned_cost_count,width_pruned_best_cost,pruned_best_gap,";
        ofs << "turns_since_best_update,ms_since_best_update,";
        ofs << "found,best_cost,best_turn,best_update_count,time_limit_reached,";
        ofs << "period_ms,period_turns,period_expanded_nodes,period_generated_candidates,period_accepted_candidates,period_selected_nodes,";
        ofs << "period_pruned_by_width,period_pruned_by_hash,period_duplicate_by_hash,period_replaced_by_width,period_replaced_by_hash,period_hash_rebuild_count,period_best_updates,";
        ofs << "expand_per_sec,generate_per_sec,accept_per_sec,select_per_sec,accept_rate,width_prune_rate,hash_prune_rate\n";
        ofs.setf(std::ios::fixed);
        ofs.precision(6);
        for (const Row& row : rows) {
            ofs << row.event << ','
                << row.elapsed_ms << ','
                << row.turn << ','
                << row.materialized_turn << ','
                << row.max_turn << ','
                << row.max_step << ','
                << row.beam_width << ','
                << row.hash_capacity << ','
                << row.active_width << ','
                << row.selected_width << ','
                << row.max_active_width << ','
                << row.max_selected_width << ','
                << row.beam_saturated_turns << ','
                << row.expanded_nodes << ','
                << row.generated_candidates << ','
                << row.accepted_candidates << ','
                << row.selected_nodes << ','
                << row.pruned_by_width << ','
                << row.pruned_by_hash << ','
                << row.duplicate_by_hash << ','
                << row.replaced_by_width << ','
                << row.replaced_by_hash << ','
                << row.invalid_candidates << ','
                << row.hash_table_full << ','
                << row.hash_rebuild_count << ','
                << row.live_nodes << ','
                << row.pending_candidates << ','
                << row.max_live_nodes << ','
                << row.max_pending_candidates << ','
                << row.time_limit_ms << ','
                << row.time_remaining_ms << ','
                << row.selector_nonempty_count << ','
                << row.selector_total_candidates << ','
                << row.selector_max_size << ','
                << row.selector_full_count << ','
                << row.selector_avg_size << ','
                << row.selector_full_rate << ','
                << row.live_best_cost << ','
                << row.live_worst_cost << ','
                << row.live_avg_cost << ','
                << row.live_best_turn << ','
                << row.selected_best_cost << ','
                << row.selected_worst_cost << ','
                << row.selected_avg_cost << ','
                << row.selected_step_avg << ','
                << row.selected_step_max << ','
                << row.width_pruned_cost_count << ','
                << row.width_pruned_best_cost << ','
                << row.pruned_best_gap << ','
                << row.turns_since_best_update << ','
                << row.ms_since_best_update << ','
                << static_cast<int>(row.found) << ','
                << row.best_cost << ','
                << row.best_turn << ','
                << row.best_update_count << ','
                << static_cast<int>(row.time_limit_reached) << ','
                << row.period_ms << ','
                << row.period_turns << ','
                << row.period_expanded_nodes << ','
                << row.period_generated_candidates << ','
                << row.period_accepted_candidates << ','
                << row.period_selected_nodes << ','
                << row.period_pruned_by_width << ','
                << row.period_pruned_by_hash << ','
                << row.period_duplicate_by_hash << ','
                << row.period_replaced_by_width << ','
                << row.period_replaced_by_hash << ','
                << row.period_hash_rebuild_count << ','
                << row.period_best_updates << ','
                << row.expand_per_sec << ','
                << row.generate_per_sec << ','
                << row.accept_per_sec << ','
                << row.select_per_sec << ','
                << row.accept_rate << ','
                << row.width_prune_rate << ','
                << row.hash_prune_rate << '\n';
        }
        std::FILE* fp = std::fopen(filename.c_str(), "wb");
        if (fp == nullptr) {
            std::cerr << "[bs] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        const std::string csv = ofs.str();
        std::fwrite(csv.data(), 1, csv.size(), fp);
        std::fclose(fp);
    }
};

// 探索結果。
// アクション列はライブラリ側では保持しない。必要ならユーザー側で State に含める。
// 結果の最良値は best_cost / best_turn に一本化する。
template<class State>
struct Result {
    bool found = false;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;
    // max_turn到着候補を少なくとも1件materializeした。
    // 全候補を通常幅で展開した保証ではない。
    bool completed_max_turn = false;
    bool hash_table_full = false;

    Cost best_cost{};
    int best_turn = 0;

    std::optional<State> best_state;
    Runtime runtime;
};

// expand に渡す候補追加器。
// 公開型なので、expand関数の引数型として明示できる。
// push/push_lazy の戻り値 true は、この呼び出し時点で候補がSelectorに入ったことを表す。
// 後続候補に置換される可能性があるため、最終結果に残る保証ではない。
// finished=true は解候補フラグであり、自動的な展開停止ではない。
// push_lazy は候補が受理可能な場合だけ maker() を呼ぶ。maker は保存されない。
template<class State>
class Emitter {
public:
    template<class U, StepArg S>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost, S step) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, static_cast<int>(step), false, false);
    }

    template<class U, StepArg S, BoolArg B>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost, S step, B finished) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, static_cast<int>(step), false, finished);
    }

    template<class U, class H, StepArg S>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash, S step) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), static_cast<int>(step), true, false);
    }

    template<class U, class H, StepArg S, BoolArg B>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash, S step, B finished) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), static_cast<int>(step), true, finished);
    }

    template<StepArg S, class Maker>
    bool push_lazy(Cost cost, S step, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, static_cast<int>(step), false, false, std::forward<Maker>(maker));
    }

    template<StepArg S, BoolArg B, class Maker>
    bool push_lazy(Cost cost, S step, B finished, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, static_cast<int>(step), false, finished, std::forward<Maker>(maker));
    }

    template<class H, StepArg S, class Maker>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, S step, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), static_cast<int>(step), true, false, std::forward<Maker>(maker));
    }

    template<class H, StepArg S, BoolArg B, class Maker>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, S step, B finished, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), static_cast<int>(step), true, finished, std::forward<Maker>(maker));
    }

private:
    friend class Engine<State>;

    Emitter(Engine<State>& engine, int turn, Hash now_hash) noexcept
        : engine_(engine), turn_(turn), now_hash_(now_hash) {}

    template<class U>
    bool push_state_impl(U&& state, Cost cost, Hash hash, int step, bool has_hash, bool finished) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, step, cost, hash, has_hash, finished, maker);
    }

    template<class Maker>
    bool push_lazy_impl(Cost cost, Hash hash, int step, bool has_hash, bool finished, Maker&& maker) {
        return engine_.push_candidate(turn_, step, cost, hash, has_hash, finished, std::forward<Maker>(maker));
    }

    Engine<State>& engine_;
    int turn_ = 0;
    Hash now_hash_{};
};


private:

// ============================================================
// callback呼び出し補助
// ============================================================

template<class Expand, class State>
static void call_expand(Expand& expand, const StateView<State>& now, Runtime& rt, Emitter<State>& emit) {
    // expand は Runtime なし、または const Runtime& ありの2系統を許可する。
    // Runtime はライブラリ管理情報なので、ユーザー側から変更できないよう const 参照だけを渡す。
    if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
        expand(now, static_cast<const Runtime&>(rt), emit);
    } else if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, Emitter<State>&>) {
        expand(now, emit);
    } else {
        static_assert(
            std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&> ||
            std::is_invocable_v<Expand&, const StateView<State>&, Emitter<State>&>,
            "expand must accept (const StateView&, const Runtime&, Emitter&) or (const StateView&, Emitter&). Runtime must be const."
        );
    }
}

// ============================================================
// 固定長Hash表。Selector内の重複排除に使う。
// clear は配列初期化ではなく epoch 更新で行う。
// 削除は行わず、置換で生じる古いentryは参照時に候補側と照合して無視する。
// 古いentryで表が詰まった場合だけ rebuild する。
// ============================================================
class FixedHashMap {
public:
    struct Lookup {
        bool found = false;
        int slot = -1;
        int value = -1;
    };

    void init(int cap) {
        cap = std::max(4, cap);
        capacity_ = detail::ceil_pow2(cap);
        mask_ = capacity_ - 1;
        keys_.assign(static_cast<std::size_t>(capacity_), Hash{});
        values_.assign(static_cast<std::size_t>(capacity_), -1);
        used_.assign(static_cast<std::size_t>(capacity_), 0);
        epoch_ = 1;
    }

    void clear() {
        ++epoch_;
        if (epoch_ == 0) {
            std::fill(used_.begin(), used_.end(), 0);
            epoch_ = 1;
        }
    }

    Lookup lookup(Hash key) const {
        int i = bucket(key);
        for (int probe = 0; probe < capacity_; ++probe) {
            if (used_[i] != epoch_) return Lookup{false, i, -1};
            if (keys_[i] == key) return Lookup{true, i, values_[i]};
            i = (i + 1) & mask_;
        }
        return Lookup{false, -1, -1};
    }


    void set(int slot, Hash key, int value) {
        if (slot < 0) return;
        used_[slot] = epoch_;
        keys_[slot] = key;
        values_[slot] = value;
    }

private:
    int capacity_ = 0;
    int mask_ = 0;
    std::uint32_t epoch_ = 1;
    std::vector<Hash> keys_;
    std::vector<int> values_;
    std::vector<std::uint32_t> used_;

    int bucket(Hash key) const { return static_cast<int>(detail::mix64(detail::hash_to_u64(key))) & mask_; }
};

// ============================================================
// 状態コピー型 run 本体
// ============================================================

template<class State>
class Engine {
    static_assert(std::copy_constructible<State>, "State must be copy constructible because best_state is copied when updated");
    static_assert(std::move_constructible<State>, "State must be move constructible");
    static_assert(std::assignable_from<State&, State>, "State must be assignable because selectors replace candidates in-place");

    friend class Emitter<State>;

    struct Item {
        // 選抜時に頻繁に読むメタ情報を先頭に置き、Stateが大きい場合のキャッシュ効率を少し良くする。
        Cost cost{};
        Hash hash{};
        int turn = 0;
        int step = 0;
        bool has_hash = false;
        bool finished = false;
        State state;

        template<class U>
        Item(U&& state_, Cost cost_, Hash hash_, int turn_, int step_, bool has_hash_, bool finished_)
            : cost(cost_),
              hash(hash_),
              turn(turn_),
              step(step_),
              has_hash(has_hash_),
              finished(finished_),
              state(std::forward<U>(state_)) {}
    };

    // 各到着ターンのSelector。push時点で beam_width 個だけを保持する。
    // 幅制限の比較はcost-only。同costでは既存候補を優先し、hashではtie-breakしない。
    // hashは同一到着ターン内の重複排除とStateView用にだけ使う。
    // State生成前に cost/hash で受理可能性を判定するため、push_lazy と通常pushの両方で内部保存の無駄を抑えられる。
    class Selector {
        friend class Engine;

    public:
        void init(int beam_width, int hash_capacity, bool use_hash) {
            beam_width_ = std::max(1, beam_width);
            use_hash_ = use_hash;
            candidates_.reserve(beam_width_);
            costs_.assign(beam_width_, Cost{});
            if (use_hash_) hash_.init(std::max(hash_capacity, beam_width_ * 2 + 1));
            seg_n_ = 1;
            while (seg_n_ < beam_width_) seg_n_ <<= 1;
            seg_.assign(seg_n_ * 2, -1);
        }

        void clear() {
            candidates_.clear();
            worst_index_cache = -1;
            if (use_hash_) hash_.clear();
        }

        [[nodiscard]] int size() const { return static_cast<int>(candidates_.size()); }
        [[nodiscard]] bool full() const { return size() >= beam_width_; }
        std::vector<Item>& selected() { return candidates_; }
        const std::vector<Item>& selected() const { return candidates_; }

        // Stateを生成する前に、幅枝刈りで確実に落ちるか判定する。
        // fullでない場合や、現在の最悪候補より良い可能性がある場合はfalseを返す。
        [[nodiscard]] bool rejects_by_width(Cost cost) const {
            return full() && !candidate_better_than_worst(cost);
        }

    private:
        int beam_width_ = 1;
        int seg_n_ = 1;
        bool use_hash_ = false;
        std::vector<Item> candidates_;
        std::vector<Cost> costs_;
        FixedHashMap hash_;
        std::vector<int> seg_;
        Cost worst_cost_cache{};
        int worst_index_cache = -1;

        // segment treeでは「最悪候補」を上に上げる。同costでは後ろのindexを最悪とし、既存候補を残しやすくする。
        int worse_index(int lhs, int rhs) const {
            if (lhs < 0) return rhs;
            if (rhs < 0) return lhs;
            if (costs_[lhs] != costs_[rhs]) return costs_[lhs] < costs_[rhs] ? rhs : lhs;
            return lhs < rhs ? rhs : lhs;
        }

        // 満杯時に入れる候補は worst より strictly better な cost のみ。同costは棄却する。
        bool candidate_better_than_worst(Cost cost) const {
            if (worst_index_cache < 0) return true;
            return cost < worst_cost_cache;
        }

        template<class Maker>
        void append_candidate(int arrival_turn, int step, Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
            candidates_.emplace_back(std::invoke(std::forward<Maker>(maker)), cost, hash, arrival_turn, step, has_hash, finished);
            const int j = static_cast<int>(candidates_.size()) - 1;
            costs_[j] = cost;
            if (static_cast<int>(candidates_.size()) == beam_width_) build_segment_tree();
        }

        template<class Maker>
        void replace_candidate(int j, int arrival_turn, int step, Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
            Item& item = candidates_[j];
            item.cost = cost;
            item.hash = hash;
            item.turn = arrival_turn;
            item.step = step;
            item.has_hash = has_hash;
            item.finished = finished;
            item.state = std::invoke(std::forward<Maker>(maker));
            costs_[j] = cost;
            if (full()) seg_set(j);
        }

        bool is_live_hash_entry(int j, Hash hash) const {
            return 0 <= j && j < static_cast<int>(candidates_.size()) &&
                   candidates_[j].has_hash && candidates_[j].hash == hash;
        }

        void rebuild_hash() {
            hash_.clear();
            for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
                if (!candidates_[i].has_hash) continue;
                auto slot = hash_.lookup(candidates_[i].hash);
                if (slot.slot >= 0) hash_.set(slot.slot, candidates_[i].hash, i);
            }
        }

        void build_segment_tree() {
            std::fill(seg_.begin(), seg_.end(), -1);
            for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = i;
            for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
            worst_index_cache = seg_[1];
            if (worst_index_cache >= 0) worst_cost_cache = costs_[worst_index_cache];
        }

        void seg_set(int pos) {
            int i = seg_n_ + pos;
            seg_[i] = pos;
            while (i >>= 1) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
            worst_index_cache = seg_[1];
            if (worst_index_cache >= 0) worst_cost_cache = costs_[worst_index_cache];
        }
    };

public:
    Engine(Param param_, State&& initial_state_, Cost initial_cost_, Hash initial_hash_, bool initial_has_hash_)
        : param(param_),
          initial_state(std::move(initial_state_)),
          initial_cost(initial_cost_),
          initial_hash(initial_hash_),
          initial_has_hash(initial_has_hash_) {
        normalize_param();
    }

    template<class Expand, class DebugHook>
    Result<State> run(Expand&& expand, DebugHook&& debug_hook) {
        rt = Runtime{};
        completed_max_turn = false;
        best_state.reset();
        active_states_count = 0;
        rt.pending_candidates = 0;
        last_best_update_us = 0;

        start_clock_us = detail::now_microseconds();
        limit_us = detail::time_limit_to_microseconds(param.time_limit_ms);
        use_time_limit = (limit_us > 0);
        debug_enabled = !std::same_as<std::decay_t<DebugHook>, NoOp>;

        allocate_buffers();
        initialize_root();

        rt.max_turn = param.max_turn;
        rt.max_step = param.max_step;
        rt.beam_width = param.beam_width;
        rt.hash_capacity = param.hash_capacity;
        rt.time_limit_ms = param.time_limit_ms;
        rt.best_cost = initial_cost;
        rt.best_turn = 0;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        emit_hook(EventType::RunStart, debug_hook);
        if (param.max_turn_is_answer && param.max_turn == 0) set_best(frontiers[0][0], debug_hook);
        completed_max_turn = (param.max_turn == 0);

        if (!completed_max_turn) {
            if (use_time_limit) run_with_soft_time_limit(expand, debug_hook);
            else run_without_time_limit(expand, debug_hook);
        }

        finalize_runtime_for_result();
        emit_hook(EventType::RunEnd, debug_hook);
        Result<State> res = build_result();
        print_result_warnings(res);
        return res;
    }

private:
    Param param;
    State initial_state;
    Cost initial_cost{};
    Hash initial_hash{};
    bool initial_has_hash = false;

    Runtime rt;
    std::int64_t start_clock_us = 0;
    std::int64_t limit_us = 0;
    bool use_time_limit = false;
    bool debug_enabled = false;

    std::vector<std::vector<Item>> frontiers;
    std::vector<Selector> selectors;
    std::optional<State> best_state;

    int ring_size = 2;
    int active_states_count = 0;
    bool completed_max_turn = false;
    std::int64_t last_best_update_us = 0;

    // ユーザー指定値を内部で扱いやすい範囲へ正規化する。
    // hash_capacity はSelector保持数が高々beam_width個であることを前提に丸める。
    void normalize_param() {
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
        param.max_step = std::min(std::max(1, param.max_step), std::max(1, param.max_turn));
        param.time_check_interval = std::max(1, param.time_check_interval);

        if (param.use_hash_dedup) {
            // Selectorが保持する候補は高々beam_width個なので、過大なhash表はキャッシュ効率のため丸める。
            if (param.hash_capacity <= 0) param.hash_capacity = param.beam_width * 4 + 17;
            const int lower = param.beam_width * 2 + 1;
            const int upper = param.beam_width * 4 + 17;
            param.hash_capacity = std::min(std::max(param.hash_capacity, lower), upper);
        } else {
            param.hash_capacity = 0;
        }
    }

    // max_step+1 のリングで、現在frontierと未来到着selectorを管理する。
    void allocate_buffers() {
        ring_size = param.max_step + 1;
        frontiers.clear();
        frontiers.resize(ring_size);
        selectors.clear();
        selectors.resize(ring_size);
        for (auto& f : frontiers) f.reserve(std::max(1, param.beam_width));
        for (auto& s : selectors) s.init(param.beam_width, param.hash_capacity, param.use_hash_dedup);
    }

    void initialize_root() {
        frontiers[0].push_back(Item(std::move(initial_state), initial_cost, initial_hash, 0, 0, initial_has_hash, false));
        active_states_count = 1;
        rt.pending_candidates = 0;
        refresh_runtime_usage();
    }

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
        rt.time_remaining_ms = use_time_limit
            ? std::max(0.0, static_cast<double>(limit_us - rt.elapsed_us) * 0.001)
            : -1.0;
    }

    void refresh_runtime_usage() {
        rt.live_nodes = active_states_count + rt.pending_candidates;
        rt.max_live_nodes = std::max(rt.max_live_nodes, rt.live_nodes);
        rt.max_pending_candidates = std::max(rt.max_pending_candidates, rt.pending_candidates);
    }

    template<class DebugHook>
    void reset_turn_debug_stats(DebugHook&) {
        if constexpr (std::same_as<std::decay_t<DebugHook>, NoOp>) return;
        rt.selected_best_cost = Cost{};
        rt.selected_worst_cost = Cost{};
        rt.selected_avg_cost = 0.0;
        rt.selected_step_avg = 0.0;
        rt.selected_step_max = 0;
        rt.width_pruned_cost_count = 0;
        rt.width_pruned_best_cost = Cost{};
        rt.pruned_best_gap = 0.0;
    }

    void update_width_pruned_debug(Cost cost) {
        if (!debug_enabled) return;
        if (rt.width_pruned_cost_count == 0 || cost < rt.width_pruned_best_cost) rt.width_pruned_best_cost = cost;
        ++rt.width_pruned_cost_count;
    }

    template<class DebugHook, class Cands>
    void collect_selected_debug_stats(const Cands& cands, bool minimal, int best_idx, DebugHook&) {
        if constexpr (std::same_as<std::decay_t<DebugHook>, NoOp>) return;
        bool has = false;
        double cost_sum = 0.0;
        double step_sum = 0.0;
        int count = 0;
        for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
            if (minimal && i != best_idx) continue;
            const Item& cand = cands[i];
            if (!has) {
                rt.selected_best_cost = rt.selected_worst_cost = cand.cost;
                has = true;
            } else {
                if (cand.cost < rt.selected_best_cost) rt.selected_best_cost = cand.cost;
                if (rt.selected_worst_cost < cand.cost) rt.selected_worst_cost = cand.cost;
            }
            cost_sum += static_cast<double>(cand.cost);
            step_sum += static_cast<double>(cand.step);
            rt.selected_step_max = std::max(rt.selected_step_max, cand.step);
            ++count;
        }
        if (count > 0) {
            rt.selected_avg_cost = cost_sum / count;
            rt.selected_step_avg = step_sum / count;
        }
    }

    void update_debug_snapshot() {
        rt.time_limit_ms = param.time_limit_ms;

        int selector_nonempty = 0;
        int selector_max = 0;
        int selector_full = 0;
        for (const Selector& selector : selectors) {
            const int sz = selector.size();
            if (sz == 0) continue;
            ++selector_nonempty;
            selector_max = std::max(selector_max, sz);
            if (selector.full()) ++selector_full;
        }
        rt.selector_nonempty_count = selector_nonempty;
        rt.selector_max_size = selector_max;
        rt.selector_full_count = selector_full;

        // NoOp実行ではmaterialize中にselected_*を計算しない。
        // RunEndの最終snapshotでは、現在保持しているfrontierから必要な値だけ復元する。
        // DebugHook有効時はcollect_selected_debug_stats()で直近materializeの値を保持するため、ここでは触らない。
        if (!debug_enabled) {
            rt.selected_best_cost = Cost{};
            rt.selected_worst_cost = Cost{};
            rt.selected_avg_cost = 0.0;
            rt.selected_step_avg = 0.0;
            rt.selected_step_max = 0;
            if (rt.selected_width > 0 && !frontiers.empty()) {
                const auto& selected_frontier = frontiers[rt.materialized_turn % ring_size];
                bool has_selected = false;
                double selected_cost_sum = 0.0;
                double selected_step_sum = 0.0;
                int selected_count = 0;
                for (const Item& cand : selected_frontier) {
                    if (!has_selected) {
                        rt.selected_best_cost = rt.selected_worst_cost = cand.cost;
                        has_selected = true;
                    } else {
                        if (cand.cost < rt.selected_best_cost) rt.selected_best_cost = cand.cost;
                        if (rt.selected_worst_cost < cand.cost) rt.selected_worst_cost = cand.cost;
                    }
                    selected_cost_sum += static_cast<double>(cand.cost);
                    selected_step_sum += static_cast<double>(cand.step);
                    rt.selected_step_max = std::max(rt.selected_step_max, cand.step);
                    ++selected_count;
                }
                if (selected_count > 0) {
                    rt.selected_avg_cost = selected_cost_sum / selected_count;
                    rt.selected_step_avg = selected_step_sum / selected_count;
                }
            }
        }

        bool has_live = false;
        double live_sum = 0.0;
        int live_count = 0;
        int best_turn = 0;
        auto add_live = [&](Cost cost, int turn) {
            if (!has_live) {
                rt.live_best_cost = rt.live_worst_cost = cost;
                best_turn = turn;
                has_live = true;
            } else {
                if (cost < rt.live_best_cost) {
                    rt.live_best_cost = cost;
                    best_turn = turn;
                }
                if (rt.live_worst_cost < cost) rt.live_worst_cost = cost;
            }
            live_sum += static_cast<double>(cost);
            ++live_count;
        };
        for (const auto& frontier : frontiers) {
            for (const Item& item : frontier) add_live(item.cost, item.turn);
        }
        for (const Selector& selector : selectors) {
            for (const Item& item : selector.selected()) add_live(item.cost, item.turn);
        }
        if (live_count > 0) {
            rt.live_avg_cost = live_sum / live_count;
            rt.live_best_turn = best_turn;
        } else {
            rt.live_best_cost = rt.live_worst_cost = Cost{};
            rt.live_avg_cost = 0.0;
            rt.live_best_turn = rt.found ? rt.best_turn : 0;
        }

        if (rt.selected_width > 0 && rt.width_pruned_cost_count > 0) {
            rt.pruned_best_gap = static_cast<double>(rt.width_pruned_best_cost) - static_cast<double>(rt.selected_worst_cost);
        } else {
            rt.pruned_best_gap = 0.0;
        }

        if (rt.best_update_count > 0) {
            rt.turns_since_best_update = std::max(0, rt.materialized_turn - rt.best_turn);
            rt.ms_since_best_update = static_cast<double>(rt.elapsed_us - last_best_update_us) * 0.001;
        } else {
            rt.turns_since_best_update = 0;
            rt.ms_since_best_update = 0.0;
        }
    }

    void finalize_runtime_for_result() {
        refresh_runtime_usage();
        update_time();
        update_debug_snapshot();
    }

    template<class DebugHook>
    void emit_hook(EventType event_type, DebugHook& debug_hook) {
        // BestUpdate は瞬間イベントなので、CSV出力や重いスナップショット更新は行わない。
        // RunEnd の最終Runtimeは、呼び出し元でfinalize_runtime_for_result()により整える。
        if constexpr (!std::same_as<std::decay_t<DebugHook>, NoOp>) {
            if (event_type != EventType::RunEnd && event_type != EventType::BestUpdate) {
                update_debug_snapshot();
            }
            debug_hook(event_type, static_cast<const Runtime&>(rt));
        } else {
            (void)event_type;
            (void)debug_hook;
        }
    }

    bool mark_time_limit_if_elapsed() {
        if (!use_time_limit) return false;
        update_time();
        if (rt.elapsed_us < limit_us) return false;
        rt.time_limit_reached = true;
        return true;
    }

    // soft limit判定。expand完了後に間引いて呼び、到達後は縮退探索へ移る。
    // expandの途中はライブラリ側では中断しない。
    bool need_switch_to_minimal_by_time() {
        if (!use_time_limit) return false;
        if ((rt.expanded_nodes % param.time_check_interval) != 0) return false;
        return mark_time_limit_if_elapsed();
    }

    template<class Maker>
    bool push_candidate(int turn, int step, Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
        static_assert(std::invocable<Maker&&>, "maker must be callable with no arguments");
        static_assert(std::constructible_from<State, std::invoke_result_t<Maker&&>>, "maker result must be constructible as State");
        ++rt.generated_candidates;
        if (!detail::is_valid_cost(cost) || step <= 0 || step > param.max_step || turn + step > param.max_turn) {
            ++rt.invalid_candidates;
            return false;
        }

        const int arrival = turn + step;
        Selector& selector = selectors[arrival % ring_size];
        if (selector.rejects_by_width(cost)) {
            ++rt.pruned_by_width;
            update_width_pruned_debug(cost);
            return false;
        }

        auto accept_appended = [&]() {
            ++rt.accepted_candidates;
            ++rt.pending_candidates;
            rt.max_pending_candidates = std::max(rt.max_pending_candidates, rt.pending_candidates);
            rt.max_live_nodes = std::max(rt.max_live_nodes, active_states_count + rt.pending_candidates);
        };
        auto accept_replaced_width = [&]() {
            ++rt.accepted_candidates;
            ++rt.replaced_by_width;
        };

        if (!(selector.use_hash_ && has_hash)) {
            if (!selector.full()) {
                selector.append_candidate(arrival, step, cost, hash, has_hash, finished, std::forward<Maker>(maker));
                accept_appended();
                return true;
            }
            const int j = selector.worst_index_cache;
            if (j < 0) return false;
            selector.replace_candidate(j, arrival, step, cost, hash, has_hash, finished, std::forward<Maker>(maker));
            accept_replaced_width();
            return true;
        }

        auto found = selector.hash_.lookup(hash);
        if (found.found) {
            const int j = found.value;
            if (selector.is_live_hash_entry(j, hash)) {
                ++rt.duplicate_by_hash;
                if (!(cost < selector.costs_[j])) {
                    ++rt.pruned_by_hash;
                    return false;
                }
                selector.replace_candidate(j, arrival, step, cost, hash, true, finished, std::forward<Maker>(maker));
                selector.hash_.set(found.slot, hash, j);
                ++rt.accepted_candidates;
                ++rt.replaced_by_hash;
                return true;
            }
            // stale entry なので、この slot を上書きしてよい。候補本体は既に別候補で置換されている。
        }

        if (found.slot < 0) {
            selector.rebuild_hash();
            ++rt.hash_rebuild_count;
            found = selector.hash_.lookup(hash);
            if (found.slot < 0) {
                ++rt.hash_table_full;
                ++rt.pruned_by_hash;
                return false;
            }
        }

        if (!selector.full()) {
            const int j = static_cast<int>(selector.candidates_.size());
            selector.append_candidate(arrival, step, cost, hash, true, finished, std::forward<Maker>(maker));
            selector.hash_.set(found.slot, hash, j);
            accept_appended();
            return true;
        }

        const int j = selector.worst_index_cache;
        if (j < 0) return false;
        selector.replace_candidate(j, arrival, step, cost, hash, true, finished, std::forward<Maker>(maker));
        selector.hash_.set(found.slot, hash, j);
        accept_replaced_width();
        return true;
    }

    template<class Expand>
    void expand_item_body(Item& item, int current_turn, Expand& expand) {
        StateView<State> now(current_turn, item.cost, item.state, item.hash, item.has_hash);
        Emitter<State> emit(*this, current_turn, item.hash);
        if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
            // Runtime付きexpandには保持量だけを最新化して渡す。
            // elapsed_usはtime limit判定・Hook・RunEndで更新し、expandごとの時刻取得を避ける。
            rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
            refresh_runtime_usage();
        }
        call_expand(expand, now, rt, emit);
        ++rt.expanded_nodes;
    }

    // 縮退探索で使うcost-onlyの最良候補選択。同costでは先に残っている候補を優先する。
    static int best_item_index(const std::vector<Item>& items, int begin = 0) {
        if (begin >= static_cast<int>(items.size())) return -1;
        int best = begin;
        for (int i = begin + 1; i < static_cast<int>(items.size()); ++i) {
            if (items[i].cost < items[best].cost) best = i;
        }
        return best;
    }

    // time_limit_ms==0用の通常探索ホットパス。縮退探索用の分岐を混ぜない。
    template<class Expand, class DebugHook>
    void run_without_time_limit(Expand& expand, DebugHook& debug_hook) {
        for (int current_turn = 0; current_turn < param.max_turn; ++current_turn) {
            begin_turn(current_turn, debug_hook);
            expand_frontier_full(current_turn, expand);
            clear_frontier(current_turn);
            materialize_arrival<false>(current_turn + 1, debug_hook);
            end_turn(debug_hook);

            if (completed_max_turn) break;
            if (active_states_count == 0 && rt.pending_candidates == 0) break;
        }
    }

    // soft limit有効時の探索。制限到達後は各ターンで最良1本だけを進め、max_turn到達を試す。
    template<class Expand, class DebugHook>
    void run_with_soft_time_limit(Expand& expand, DebugHook& debug_hook) {
        bool minimal_mode = rt.time_limit_reached;
        for (int current_turn = 0; current_turn < param.max_turn; ++current_turn) {
            begin_turn(current_turn, debug_hook);
            if (rt.time_limit_reached) minimal_mode = true;
            expand_frontier_time_checked(current_turn, minimal_mode, expand);
            clear_frontier(current_turn);
            if (minimal_mode) materialize_arrival<true>(current_turn + 1, debug_hook);
            else materialize_arrival<false>(current_turn + 1, debug_hook);
            end_turn(debug_hook);
            if (rt.time_limit_reached) minimal_mode = true;

            if (completed_max_turn) break;
            if (active_states_count == 0 && rt.pending_candidates == 0) break;
        }
    }

    template<class DebugHook>
    void begin_turn(int current_turn, DebugHook& debug_hook) {
        rt.turn = current_turn;
        rt.active_width = 0;
        rt.selected_width = 0;
        reset_turn_debug_stats(debug_hook);
        if (use_time_limit) mark_time_limit_if_elapsed();
        else if constexpr (!std::same_as<std::decay_t<DebugHook>, NoOp>) update_time();
        emit_hook(EventType::TurnStart, debug_hook);
    }

    template<class DebugHook>
    void end_turn(DebugHook& debug_hook) {
        refresh_runtime_usage();
        if (use_time_limit) mark_time_limit_if_elapsed();
        else if constexpr (!std::same_as<std::decay_t<DebugHook>, NoOp>) update_time();
        emit_hook(EventType::TurnEnd, debug_hook);
    }

    template<class Expand>
    void expand_frontier_full(int current_turn, Expand& expand) {
        auto& cur = frontiers[current_turn % ring_size];
        const int n = static_cast<int>(cur.size());
        if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
            for (int i = 0; i < n; ++i) {
                ++rt.active_width;
                rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
                expand_item_body(cur[i], current_turn, expand);
            }
        } else {
            rt.active_width = n;
            rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
            for (int i = 0; i < n; ++i) expand_item_body(cur[i], current_turn, expand);
        }
    }

    // 通常モードではfrontierを順に展開する。時間到達後は、未展開部分から最良1本だけ追加で展開する。
    template<class Expand>
    void expand_frontier_time_checked(int current_turn, bool& minimal_mode, Expand& expand) {
        auto& cur = frontiers[current_turn % ring_size];
        int next_index = 0;

        if (!minimal_mode) {
            for (; next_index < static_cast<int>(cur.size()); ++next_index) {
                ++rt.active_width;
                expand_item_body(cur[next_index], current_turn, expand);
                if (need_switch_to_minimal_by_time()) {
                    minimal_mode = true;
                    ++next_index;
                    break;
                }
            }
            rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
        }

        if (minimal_mode) {
            const int best = best_item_index(cur, next_index);
            if (best >= 0) {
                ++rt.active_width;
                rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
                expand_item_body(cur[best], current_turn, expand);
            }
        }
    }

    void clear_frontier(int current_turn) {
        auto& cur = frontiers[current_turn % ring_size];
        active_states_count -= static_cast<int>(cur.size());
        if (active_states_count < 0) active_states_count = 0;
        cur.clear();
        refresh_runtime_usage();
    }

    bool is_answer(const Item& item) const {
        return item.finished || (param.max_turn_is_answer && item.turn == param.max_turn);
    }

    template<class DebugHook>
    void set_best(const Item& item, DebugHook& debug_hook) {
        // best更新もcost-only。同costの解候補では先に見つかったものを残す。
        if (rt.found && !(item.cost < rt.best_cost)) return;
        best_state.emplace(item.state);
        rt.found = true;
        rt.best_cost = item.cost;
        rt.best_turn = item.turn;
        ++rt.best_update_count;
        update_time();
        last_best_update_us = rt.elapsed_us;
        emit_hook(EventType::BestUpdate, debug_hook);
    }

    template<bool Minimal, class DebugHook>
    void materialize_arrival(int arrival_turn, DebugHook& debug_hook) {
        // 候補は到着ターンでmaterializeされたときに初めてanswer判定する。
        // Minimal=false: Selector内の全候補を次frontierへ移す。
        // Minimal=true : Selector内の最良cost 1件だけを移し、他は破棄する。
        Selector& selector = selectors[arrival_turn % ring_size];
        auto& selected = selector.selected();
        const int original_count = static_cast<int>(selected.size());
        int best_idx = -1;
        if constexpr (Minimal) best_idx = best_item_index(selected);

        rt.materialized_turn = std::max(rt.materialized_turn, arrival_turn);
        rt.selected_width = Minimal ? (best_idx >= 0 ? 1 : 0) : original_count;
        rt.max_selected_width = std::max(rt.max_selected_width, rt.selected_width);
        if (rt.selected_width >= param.beam_width) ++rt.beam_saturated_turns;
        collect_selected_debug_stats(selected, Minimal, best_idx, debug_hook);

        auto& dst = frontiers[arrival_turn % ring_size];
        dst.clear();
        if (dst.capacity() < static_cast<std::size_t>(param.beam_width)) dst.reserve(param.beam_width);

        if constexpr (Minimal) {
            if (best_idx >= 0) materialize_one(selected[best_idx], dst, debug_hook);
        } else {
            for (Item& item : selected) materialize_one(item, dst, debug_hook);
        }
        // completed_max_turn は「max_turn到着候補を実体化した」ことを表す。通常幅で全展開した保証ではない。
        if (rt.selected_width > 0 && arrival_turn == param.max_turn) completed_max_turn = true;

        rt.selected_nodes += rt.selected_width;
        rt.pending_candidates = std::max(0, rt.pending_candidates - original_count);
        active_states_count += rt.selected_width;
        selector.clear();
        refresh_runtime_usage();
    }

    // best_stateはmove前にコピーする。これにより返却されるbest_stateはfrontierの寿命に依存しない。
    template<class DebugHook>
    void materialize_one(Item& item, std::vector<Item>& dst, DebugHook& debug_hook) {
        if (is_answer(item)) set_best(item, debug_hook);
        dst.push_back(std::move(item));
    }

    Result<State> build_result() {
        Result<State> res;
        res.found = rt.found;
        res.time_limit_reached = rt.time_limit_reached;
        res.completed_max_turn = completed_max_turn;
        res.hash_table_full = (rt.hash_table_full > 0);
        res.best_cost = rt.best_cost;
        res.best_turn = rt.best_turn;
        res.best_state = std::move(best_state);
        res.runtime = rt;
        return res;
    }

    void print_result_warnings(const Result<State>& res) const {
        if (!param.print_warnings) return;
        if (res.hash_table_full) {
            std::cerr << "[bs] warning: hash_table_full; hash_capacity="
                      << res.runtime.hash_capacity
                      << ", beam_width=" << param.beam_width << '\n';
        }
    }
};

public:
    // Hashあり初期状態版。
    // expand は (const StateView<State>&, Emitter<State>&)、または
    // (const StateView<State>&, const Runtime&, Emitter<State>&) を受け取れる。
    // Runtime は const 参照固定であり、expand から Runtime を変更することはできない。
    // 履歴が必要な場合はユーザー側で State に含める。
    template<class State, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State> run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        H initial_hash,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) {
        Engine<State> engine(
            param,
            std::move(initial_state),
            initial_cost,
            static_cast<Hash>(initial_hash),
            true
        );
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
    }

    // Hashなし初期状態版。
    // Hash付きpushを行うまでは has_hash=false として扱われ、重複排除されない。
    template<class State, class Expand, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<State> run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) {
        Engine<State> engine(
            param,
            std::move(initial_state),
            initial_cost,
            Hash{},
            false
        );
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
    }
};

} // namespace bs

#if __INCLUDE_LEVEL__ == 0

#include <array>
#include <cstdlib>
#include <fstream>
#include <random>
#include <thread>

namespace {

using BS = bs::Beam<int, std::uint64_t>;
using BS32 = bs::Beam<int, std::uint32_t>;
static_assert(std::same_as<decltype(BS::Param{}.time_limit_ms), double>);
static_assert(std::same_as<decltype(BS::Runtime{}.time_limit_ms), double>);

static std::int64_t test_now_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

#define BS_TEST_ASSERT(expr) do { \
    if (!(expr)) { \
        std::cerr << "[test] assertion failed: " #expr << " at line " << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (false)

struct SmallState {
    int x = 0;
    std::vector<int> actions;
};

struct TinyState {
    int v = 0;
    int checksum = 0;
};

struct RefItem {
    TinyState state;
    int cost = 0;
    std::uint64_t hash = 0;
    int turn = 0;
};

static std::uint64_t test_mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static bool ref_better(const RefItem& a, const RefItem& b) {
    return a.cost < b.cost;
}

static void ref_push(std::vector<RefItem>& bucket, int beam_width, const RefItem& cand, bool use_hash) {
    if (use_hash) {
        for (RefItem& old : bucket) {
            if (old.hash == cand.hash) {
                if (cand.cost < old.cost) old = cand;
                return;
            }
        }
    }
    if (static_cast<int>(bucket.size()) < beam_width) {
        bucket.push_back(cand);
        return;
    }
    int worst = 0;
    for (int i = 1; i < static_cast<int>(bucket.size()); ++i) {
        if (bucket[worst].cost < bucket[i].cost ||
            (bucket[worst].cost == bucket[i].cost && worst < i)) {
            worst = i;
        }
    }
    if (ref_better(cand, bucket[worst])) bucket[worst] = cand;
}

static std::pair<int, TinyState> run_reference(int seed, int max_turn, int beam_width, int max_step, int branch, bool use_hash) {
    std::vector<std::vector<RefItem>> frontier(max_turn + 1);
    std::vector<std::vector<RefItem>> buckets(max_turn + 1);
    TinyState init{seed % 17, seed * 3 + 1};
    frontier[0].push_back(RefItem{init, 0, static_cast<std::uint64_t>(init.v), 0});
    bool found = false;
    int best_cost = 0;
    TinyState best{};

    for (int t = 0; t < max_turn; ++t) {
        for (const RefItem& now : frontier[t]) {
            for (int k = 0; k < branch; ++k) {
                int step = 1 + static_cast<int>(test_mix(seed * 1000003ULL + now.state.v * 131ULL + t * 17ULL + k) % max_step);
                if (t + step > max_turn) step = max_turn - t;
                TinyState nx;
                nx.v = static_cast<int>((now.state.v * 37LL + now.state.checksum * 11LL + k * 19LL + step * 7LL + seed) % 1009);
                nx.checksum = static_cast<int>((now.state.checksum * 43LL + nx.v * 5LL + k + step) % 1000003);
                int delta = static_cast<int>((test_mix(nx.v + nx.checksum * 3ULL + k * 1009ULL + seed) % 41));
                int cost = now.cost + delta;
                std::uint64_t h = use_hash ? static_cast<std::uint64_t>(nx.v) : now.hash;
                RefItem cand{nx, cost, h, t + step};
                ref_push(buckets[t + step], beam_width, cand, use_hash);
            }
        }
        frontier[t + 1] = buckets[t + 1];
        if (t + 1 == max_turn) {
            for (const RefItem& item : frontier[t + 1]) {
                if (!found || item.cost < best_cost) {
                    found = true;
                    best_cost = item.cost;
                    best = item.state;
                }
            }
        }
    }
    BS_TEST_ASSERT(found || max_turn == 0);
    return {best_cost, best};
}

static void test_edge_cases() {
    {
        BS::Param param;
        param.max_turn = 0;
        param.print_warnings = false;
        SmallState init{42, {}};
        auto expand = [](const BS::StateView<SmallState>&, auto&) {
            BS_TEST_ASSERT(false);
        };
        auto res = BS::run<SmallState>(param, init, 7, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 7);
        BS_TEST_ASSERT(res.best_turn == 0);
        BS_TEST_ASSERT(res.best_state->x == 42);
    }
    {
        BS::Param param;
        param.max_turn = 3;
        param.max_turn_is_answer = false;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>&, auto&) {};
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(!res.found);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 1);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_step = 2;
        param.beam_width = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) {
                SmallState a = now.state;
                a.x = 100;
                emit.push(std::move(a), 100, std::uint64_t{100}, 2);
                SmallState b = now.state;
                b.x = 1;
                emit.push(std::move(b), 1, std::uint64_t{1}, 1);
            } else if (now.turn == 1 && now.state.x == 1) {
                SmallState c = now.state;
                c.x = 2;
                emit.push(std::move(c), 2, std::uint64_t{2}, 1);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 2);
        BS_TEST_ASSERT(res.best_state->x == 2);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 10;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.x = 10;
            SmallState b = now.state; b.x = 5;
            emit.push(std::move(a), 10, std::uint64_t{123}, 1);
            emit.push(std::move(b), 5, std::uint64_t{123}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_state->x == 5);
        BS_TEST_ASSERT(res.runtime.duplicate_by_hash == 1);
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 2);
        BS_TEST_ASSERT(res.runtime.replaced_by_hash == 1);
        BS_TEST_ASSERT(res.runtime.materialized_turn == 1);
    }
    {
        static_assert(bs::HashType<unsigned int>);
        static_assert(bs::HashType<unsigned long long>);
        static_assert(!bs::HashType<bool>);
        static_assert(bs::CompatibleHashArg<std::uint32_t, std::uint64_t>);
        static_assert(!bs::CompatibleHashArg<int, std::uint64_t>);
        static_assert(bs::StepArg<int>);
        static_assert(!bs::StepArg<long long>);
        static_assert(!std::default_initializable<BS::Emitter<SmallState>>);
        BS32::Param param;
        param.max_turn = 1;
        param.beam_width = 4;
        param.print_warnings = false;
        auto expand = [](const BS32::StateView<SmallState>& now, BS32::Emitter<SmallState>& emit) {
            BS_TEST_ASSERT(now.has_hash);
            BS_TEST_ASSERT(now.hash == std::uint32_t{9});
            SmallState a = now.state; a.x = 20;
            SmallState b = now.state; b.x = 3;
            emit.push(std::move(a), 20, std::uint32_t{555}, 1);
            emit.push(std::move(b), 3, std::uint32_t{555}, 1);
        };
        auto res = BS32::run<SmallState>(param, SmallState{}, 0, std::uint32_t{9}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3);
        BS_TEST_ASSERT(res.best_state->x == 3);
        BS_TEST_ASSERT(res.runtime.duplicate_by_hash == 1);

        auto expand_no_initial_hash = [](const BS32::StateView<SmallState>& now, BS32::Emitter<SmallState>& emit) {
            BS_TEST_ASSERT(!now.has_hash);
            SmallState s = now.state;
            s.x = 8;
            emit.push(std::move(s), 8, std::uint32_t{8}, 1);
        };
        auto res2 = BS32::run<SmallState>(param, SmallState{}, 0, expand_no_initial_hash);
        BS_TEST_ASSERT(res2.found);
        BS_TEST_ASSERT(res2.best_state->x == 8);
    }
    {
        using BSD = bs::Beam<double, std::uint64_t>;
        BSD::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        param.print_warnings = false;
        auto expand = [](const BSD::StateView<SmallState>& now, BSD::Emitter<SmallState>& emit) {
            SmallState bad = now.state;
            bad.x = -1;
            emit.push(std::move(bad), std::numeric_limits<double>::quiet_NaN(), std::uint64_t{1}, 1);
            SmallState good = now.state;
            good.x = 5;
            emit.push(std::move(good), 3.0, std::uint64_t{2}, 1);
        };
        auto res = BSD::run<SmallState>(param, SmallState{}, 0.0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3.0);
        BS_TEST_ASSERT(res.best_state->x == 5);
        BS_TEST_ASSERT(res.runtime.invalid_candidates == 1);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.x = 10;
            SmallState b = now.state; b.x = 5;
            emit.push(std::move(a), 10, std::uint64_t{10}, 1);
            emit.push(std::move(b), 5, std::uint64_t{5}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 2);
        BS_TEST_ASSERT(res.runtime.replaced_by_width == 1);
        BS_TEST_ASSERT(res.runtime.selected_nodes == 1);
        BS_TEST_ASSERT(res.runtime.max_selected_width == 1);
        BS_TEST_ASSERT(res.runtime.beam_saturated_turns == 1);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.x = 10;
            SmallState b = now.state; b.x = 2;
            emit.push(std::move(a), 5, std::uint64_t{10}, 1);
            emit.push(std::move(b), 2, std::uint64_t{2}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 2);
        // RunEndではNoOpでもfrontier/selector由来のsnapshotが最終値へ更新される。
        BS_TEST_ASSERT(res.runtime.live_best_cost == 2);
        BS_TEST_ASSERT(res.runtime.live_worst_cost == 5);
        BS_TEST_ASSERT(res.runtime.live_avg_cost == 3.5);
        BS_TEST_ASSERT(res.runtime.selector_nonempty_count == 0);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.print_warnings = false;
        int maker_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            emit.push_lazy(1, std::uint64_t{1}, 1, [&]() {
                ++maker_calls;
                SmallState s = now.state;
                s.x = 1;
                return s;
            });
            emit.push_lazy(100, std::uint64_t{2}, 1, [&]() {
                ++maker_calls;
                SmallState s = now.state;
                s.x = 100;
                return s;
            });
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 1);
        BS_TEST_ASSERT(maker_calls == 1);
        BS_TEST_ASSERT(res.runtime.pruned_by_width == 1);
    }
    {
        BS::Param param;
        param.max_turn = 5;
        param.max_turn_is_answer = false;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, const BS::Runtime& rt, auto& emit) {
            BS_TEST_ASSERT(now.turn == rt.turn);
            if (now.turn == 0) {
                SmallState s = now.state;
                s.x = 77;
                emit.push(std::move(s), 3, std::uint64_t{77}, 1, true);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 1);
        BS_TEST_ASSERT(res.best_cost == 3);
        BS_TEST_ASSERT(res.best_state->x == 77);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.max_step = 1;
        param.beam_width = 3;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState s = now.state;
            emit.push(s, 1, 0);  // invalid step
            emit.push(s, 2, 2);  // invalid step
            emit.push(s, 3, 1);  // valid
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3);
        BS_TEST_ASSERT(res.runtime.invalid_candidates == 2);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.beam_width = 3;
        param.time_limit_ms = 2000;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, const BS::Runtime& rt, auto& emit) {
            BS_TEST_ASSERT(now.turn == rt.turn);
            BS_TEST_ASSERT(rt.active_width >= 1);
            BS_TEST_ASSERT(rt.max_active_width >= rt.active_width);
            if (now.turn + 1 <= 2) {
                SmallState s = now.state;
                s.x += 1;
                emit.push(std::move(s), now.cost + 1, static_cast<std::uint64_t>(now.turn + 1), 1);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 2);
        BS_TEST_ASSERT(res.runtime.max_active_width >= 1);
    }
    {
        const char* filename = "beam_copy_multi_v32_stat.csv";
        BS::Param param;
        param.max_turn = 3;
        param.max_step = 2;
        param.beam_width = 2;
        param.print_warnings = false;
        BS::CsvStatHook hook(filename, 0.0);
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) {
                SmallState a = now.state;
                a.x += 1;
                emit.push(std::move(a), now.cost + 1, std::uint64_t{101}, 1);
                SmallState b = now.state;
                b.x += 2;
                emit.push(std::move(b), now.cost + 2, std::uint64_t{102}, 2);
            } else if (now.turn + 1 <= 3) {
                SmallState s = now.state;
                s.x += 1;
                emit.push(std::move(s), now.cost + 1, static_cast<std::uint64_t>(now.turn + 10), 1);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand, hook);
        BS_TEST_ASSERT(res.found);
        std::ifstream ifs(filename);
        BS_TEST_ASSERT(ifs.good());
        BS_TEST_ASSERT(!hook.rows.empty());
        bool saw_step2 = false;
        for (const auto& row : hook.rows) {
            BS_TEST_ASSERT(std::string(row.event) != "BestUpdate");
            saw_step2 = saw_step2 || (row.selected_step_max == 2);
        }
        BS_TEST_ASSERT(saw_step2);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.time_limit_ms = 1;
        param.time_check_interval = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            SmallState s = now.state;
            s.x = 1;
            emit.push(std::move(s), 1, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.time_limit_reached);
    }
    {
        BS::Param param;
        param.max_turn = 3;
        param.beam_width = 4;
        param.time_limit_ms = 1;
        param.time_check_interval = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) std::this_thread::sleep_for(std::chrono::milliseconds(3));
            for (int k = 0; k < 4; ++k) {
                SmallState s = now.state;
                s.x = now.state.x * 10 + k;
                const int cost = now.cost + k;
                emit.push(std::move(s), cost, static_cast<std::uint64_t>(100 + s.x), 1);
            }
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.time_limit_reached);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 3);
        BS_TEST_ASSERT(res.best_cost == 0);
        BS_TEST_ASSERT(res.runtime.max_selected_width == 1);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.x = 10;
            SmallState b = now.state; b.x = 1;
            emit.push(std::move(a), 5, std::uint64_t{100}, 1);
            emit.push(std::move(b), 5, std::uint64_t{1}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_state->x == 10);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_step = 100;
        param.beam_width = 5;
        param.hash_capacity = 1000000;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState s = now.state;
            s.x += 1;
            emit.push(std::move(s), now.cost + 1, std::uint64_t{static_cast<unsigned>(now.turn + 1)}, 1);
        };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.runtime.max_step == 2);
        BS_TEST_ASSERT(res.runtime.hash_capacity == 5 * 4 + 17);
    }
    {
        BS::Param param;
        param.max_turn = 0;
        param.max_step = 100;
        param.print_warnings = false;
        auto expand = [](const BS::StateView<SmallState>&, auto&) { BS_TEST_ASSERT(false); };
        auto res = BS::run<SmallState>(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.runtime.max_step == 1);
        BS_TEST_ASSERT(res.completed_max_turn);
    }
}

static void test_random_cases() {
    std::mt19937 rng(1234567);
    for (int case_id = 0; case_id < 400; ++case_id) {
        int max_turn = 1 + static_cast<int>(rng() % 10);
        int max_step = 1 + static_cast<int>(rng() % 4);
        max_step = std::min(max_step, max_turn);
        int beam_width = 1 + static_cast<int>(rng() % 12);
        int branch = 1 + static_cast<int>(rng() % 14);
        bool use_hash = (rng() & 1U) != 0;
        int seed = static_cast<int>(rng() % 100000);

        BS::Param param;
        param.max_turn = max_turn;
        param.max_step = max_step;
        param.beam_width = beam_width;
        param.use_hash_dedup = use_hash;
        param.print_warnings = false;

        TinyState init{seed % 17, seed * 3 + 1};
        auto expand = [=](const BS::StateView<TinyState>& now, auto& emit) {
            for (int k = 0; k < branch; ++k) {
                int step = 1 + static_cast<int>(test_mix(seed * 1000003ULL + now.state.v * 131ULL + now.turn * 17ULL + k) % max_step);
                if (now.turn + step > max_turn) step = max_turn - now.turn;
                TinyState nx;
                nx.v = static_cast<int>((now.state.v * 37LL + now.state.checksum * 11LL + k * 19LL + step * 7LL + seed) % 1009);
                nx.checksum = static_cast<int>((now.state.checksum * 43LL + nx.v * 5LL + k + step) % 1000003);
                int delta = static_cast<int>((test_mix(nx.v + nx.checksum * 3ULL + k * 1009ULL + seed) % 41));
                int cost = now.cost + delta;
                if (use_hash) emit.push(nx, cost, static_cast<std::uint64_t>(nx.v), step);
                else emit.push(nx, cost, step);
            }
        };
        auto res = use_hash
            ? BS::run<TinyState>(param, init, 0, static_cast<std::uint64_t>(init.v), expand)
            : BS::run<TinyState>(param, init, 0, expand);
        auto ref = run_reference(seed, max_turn, beam_width, max_step, branch, use_hash);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == ref.first);
        BS_TEST_ASSERT(res.best_state->v == ref.second.v);
        BS_TEST_ASSERT(res.best_state->checksum == ref.second.checksum);
    }
}

struct BenchStateTrivial {
    int v = 0;
    std::uint64_t h = 0;
    std::array<int, 16> data{};
};

struct BenchStateVector {
    std::vector<int> data;
    int v = 0;
};

static long long median_us(std::vector<long long> xs) {
    std::sort(xs.begin(), xs.end());
    return xs[xs.size() / 2];
}

static long long benchmark_trivial_state_once(double time_limit_ms) {
    BS::Param param;
    param.max_turn = 80;
    param.max_step = 3;
    param.beam_width = 256;
    param.time_limit_ms = time_limit_ms;
    param.print_warnings = false;

    auto run_once = [&]() {
        BenchStateTrivial init;
        auto expand = [](const BS::StateView<BenchStateTrivial>& now, auto& emit) {
            for (int k = 0; k < 20; ++k) {
                int step = 1 + (k % 3);
                if (now.turn + step > 80) step = 80 - now.turn;
                BenchStateTrivial nx = now.state;
                nx.v = static_cast<int>((nx.v * 33LL + k * 17LL + step * 5LL + now.turn) & 0x7fffffff);
                nx.h = test_mix(now.hash + static_cast<std::uint64_t>(k + 1) * 1000003ULL + step);
                nx.data[k & 15] += nx.v + step;
                int cost = now.cost + static_cast<int>(nx.h % 97);
                emit.push(std::move(nx), cost, nx.h, step);
            }
        };
        auto beg = test_now_microseconds();
        auto res = BS::run<BenchStateTrivial>(param, init, 0, std::uint64_t{0}, expand);
        auto end = test_now_microseconds();
        BS_TEST_ASSERT(res.found);
        return end - beg;
    };

    std::vector<long long> times;
    for (int i = 0; i < 7; ++i) times.push_back(run_once());
    return median_us(times);
}

static long long benchmark_vector_state_lazy_once(double time_limit_ms) {
    BS::Param param;
    param.max_turn = 50;
    param.max_step = 2;
    param.beam_width = 128;
    param.time_limit_ms = time_limit_ms;
    param.print_warnings = false;

    auto run_once = [&]() {
        BenchStateVector init;
        init.data.assign(64, 0);
        auto expand = [](const BS::StateView<BenchStateVector>& now, auto& emit) {
            for (int k = 0; k < 18; ++k) {
                int step = 1 + (k & 1);
                if (now.turn + step > 50) step = 50 - now.turn;
                int nv = static_cast<int>((now.state.v * 31LL + k * 13LL + step + now.turn) % 1000003);
                std::uint64_t h = test_mix(static_cast<std::uint64_t>(nv));
                int cost = now.cost + static_cast<int>(h % 89);
                emit.push_lazy(cost, h, step, [&]() {
                    BenchStateVector nx = now.state;
                    nx.v = nv;
                    nx.data[static_cast<std::size_t>(k) % nx.data.size()] += nv;
                    return nx;
                });
            }
        };
        auto beg = test_now_microseconds();
        auto res = BS::run<BenchStateVector>(param, init, 0, std::uint64_t{0}, expand);
        auto end = test_now_microseconds();
        BS_TEST_ASSERT(res.found);
        return end - beg;
    };

    std::vector<long long> times;
    for (int i = 0; i < 7; ++i) times.push_back(run_once());
    return median_us(times);
}

static void benchmark_trivial_state() {
    std::cout << "benchmark trivial_state time_limit_ms=0 median_us=" << benchmark_trivial_state_once(0) << '\n';
    std::cout << "benchmark trivial_state time_limit_ms=2000 median_us=" << benchmark_trivial_state_once(2000) << '\n';
}

static void benchmark_vector_state_lazy() {
    std::cout << "benchmark vector_state_push_lazy time_limit_ms=0 median_us=" << benchmark_vector_state_lazy_once(0) << '\n';
    std::cout << "benchmark vector_state_push_lazy time_limit_ms=2000 median_us=" << benchmark_vector_state_lazy_once(2000) << '\n';
}

} // namespace

int main() {
    test_edge_cases();
    test_random_cases();
    benchmark_trivial_state();
    benchmark_vector_state_lazy();
    std::cout << "all tests passed" << std::endl;
    return 0;
}

#endif // __INCLUDE_LEVEL__ == 0
