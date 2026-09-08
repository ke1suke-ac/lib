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

// Emitter::push の引数誤用を減らすため、hash/bool/stateを概念で分ける。
// hash引数は Hash 型以下の64bit unsigned integral のみ受け付ける。
template<class H, class Hash>
concept CompatibleHashArg =
    HashType<Hash> &&
    HashType<std::remove_cvref_t<H>> &&
    std::convertible_to<std::remove_cvref_t<H>, Hash> &&
    (sizeof(std::remove_cvref_t<H>) <= sizeof(Hash));

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

template<CostType Cost = long long, HashType Hash = std::uint64_t>
struct Beam {
    using cost_type = Cost;
    using hash_type = Hash;


private:
    template<class State>
    class Engine;

public:
// 探索パラメータ。
// max_turn は初期状態をターン0とした生成上限。負値は0に丸める。
// beam_width は1以上に丸められる。
// time_limit_ms は soft limit。expand実行中は中断しない。
// 制限到達を検出したturnでは、通常展開を止め、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
// 解候補は finished=true、または max_turn_is_answer=true で最大ターンに到達した状態。
struct Param {
    int max_turn = 0;               // 初期状態を0とする最大ターン。負値は0に丸める。
    int beam_width = 1;             // 各frontier更新で残す候補数。
    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。過大値もbeam_width基準で丸める。

    double time_limit_ms = 0.0;    // 0なら無制限。正なら制限到達検出後に最良1本の縮退探索へ移るsoft limit。
    int time_check_interval = 64;   // 何ノード展開ごとに時間を見るか。expand中は中断しない。
    bool max_turn_is_answer = true; // trueなら turn==max_turn の状態を解候補として扱う。
    bool use_hash_dedup = true;     // trueならHash付きpushで同一frontier内の重複排除を行う。Hashなしpushは対象外。
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
    int turn = 0;                   // 初期状態を0とするライブラリ管理のターン。
    Cost cost{};
    const State& state;
    Hash hash{};
    bool has_hash = false;

    StateView(int turn_, Cost cost_, const State& state_, Hash hash_, bool has_hash_) noexcept
        : turn(turn_), cost(cost_), state(state_), hash(hash_), has_hash(has_hash_) {}
};

// Hookから観測できる実行時情報。
// expanded/generated/accepted/selected/best などの累積カウンタは常に更新される。
// live_*、selected_* などのsnapshot系は DebugHook有効時、またはRunEndの最終Result用に更新される。
// best更新からの停滞量は、NoOp実行後の Result::runtime でも最終値を確認できる。
struct Runtime {
    // 基本情報。
    int turn = 0;                   // 現在処理中のターン。初期状態は0。
    int materialized_turn = 0;     // frontierとして実体化済みの最大ターン。探索完了確認にはこちらを見る。
    int max_turn = 0;               // 初期状態を0とする最大ターン。負値指定は0に丸めた後の値。
    int beam_width = 0;
    int hash_capacity = 0;

    // 各ターンの幅と、探索全体での最大幅。
    int active_width = 0;           // このターンで実際にexpandした状態数。縮退探索では1本だけになり得る。
    int selected_width = 0;         // 直近materializeでfrontierへ移した候補数。縮退探索中は最大1。
    int candidate_count = 0;        // 直近ターン更新でSelectorに残った候補数。
    int max_active_width = 0;
    int max_selected_width = 0;
    int beam_saturated_turns = 0;  // selected_width が beam_width に到達したターン数。

    // 候補数。
    long long expanded_nodes = 0;
    long long generated_candidates = 0; // emit.push/push_lazy呼び出し総数。
    long long accepted_candidates = 0;  // Selectorに一時的にでも入り、ライブラリ内部でState保存まで進んだ候補数。
    long long selected_nodes = 0;       // 最終的にfrontierへ実体化された候補数。

    // 棄却・置換統計。
    long long pruned_by_width = 0;     // 満杯Selectorのworst cost以上で、hash lookup前に即棄却された候補数。同costは既存候補を優先する。
    long long pruned_by_hash = 0;      // width事前判定通過後、同一hashで改善しないか、hash表都合で棄却された候補数。
    long long duplicate_by_hash = 0;   // width事前判定通過後、同一hash候補を見つけた回数。改善置換も含む。
    long long replaced_by_width = 0;   // beam幅競争によりworst候補を置換した回数。hash同一置換は含まない。
    long long replaced_by_hash = 0;    // 同一hashのより良い候補で既存候補を置換した回数。
    long long invalid_candidates = 0;  // NaN cost や max_turn 超過など、不正候補として棄却した回数。
    long long hash_rebuild_count = 0;

    // 保持中状態数。
    int live_nodes = 0;            // frontier と selector 内に保持中の状態数。
    int max_live_nodes = 0;

    // CSVデバッグ用の軽量スナップショット。
    // DebugHook有効時、またはRunEndの最終Result用に更新される。
    double time_limit_ms = 0.0;
    double time_remaining_ms = -1.0;
    std::int64_t elapsed_us = 0;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;

    Cost live_best_cost{};         // frontier + selector 全体の保持中候補の最良cost。確定best_costとは別。
    Cost live_worst_cost{};
    double live_avg_cost = 0.0;

    Cost selected_best_cost{};     // 直近materializeでfrontierへ進んだ候補のcost分布。縮退探索では1件分。
    Cost selected_worst_cost{};
    double selected_avg_cost = 0.0;
    int width_pruned_cost_count = 0;
    Cost width_pruned_best_cost{};
    double pruned_best_gap = 0.0;

    // best更新後の停滞量。NoOp実行でもRunEnd前に更新される。
    int turns_since_best_update = 0;
    long long expanded_nodes_since_best_update = 0;
    double ms_since_best_update = 0.0;

    bool found = false;
    Cost best_cost{};
    int best_turn = 0;
    int best_update_count = 0;

    bool exhausted = false;
    // max_turn到着候補を少なくとも1件materializeした。
    // 全候補を通常幅で展開した保証ではない。
    bool completed_max_turn = false;
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
    // RowはRuntimeのスナップショットに、CSV固有の期間統計を足したもの。
    struct Row : Runtime {
        Row() = default;
        explicit Row(const Runtime& runtime) : Runtime(runtime) {}

        const char* event = "";
        double elapsed_ms = 0.0;
        double period_ms = 0.0;
        int period_turns = 0;

        long long period_expanded_nodes = 0;
        long long period_generated_candidates = 0;
        long long period_accepted_candidates = 0;
        long long period_selected_nodes = 0;
        long long period_pruned_by_width = 0;
        long long period_pruned_by_hash = 0;
        long long period_duplicate_by_hash = 0;
        long long period_replaced_by_width = 0;
        long long period_replaced_by_hash = 0;
        long long period_invalid_candidates = 0;
        long long period_hash_rebuild_count = 0;
        int period_best_updates = 0;

        double expand_per_sec = 0.0;
        double generate_per_sec = 0.0;
        double accept_per_sec = 0.0;
        double select_per_sec = 0.0;
        double accept_rate = 0.0;
        double width_prune_rate = 0.0;
        double hash_prune_rate = 0.0;
        double branching_factor = 0.0;
        double duplicate_rate = 0.0;
        double accepted_per_expanded = 0.0;
        double selected_per_expanded = 0.0;
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

    void push_row(EventType event_type, const Runtime& runtime) {
        Row row(runtime);
        row.event = event_name(event_type);
        row.elapsed_ms = elapsed_ms(runtime.elapsed_us);
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
            row.period_invalid_candidates = row.invalid_candidates - prev.invalid_candidates;
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
                row.duplicate_rate = static_cast<double>(row.period_duplicate_by_hash) / denom;
            }
            if (row.period_expanded_nodes > 0) {
                const double denom = static_cast<double>(row.period_expanded_nodes);
                row.branching_factor = static_cast<double>(row.period_generated_candidates) / denom;
                row.accepted_per_expanded = static_cast<double>(row.period_accepted_candidates) / denom;
                row.selected_per_expanded = static_cast<double>(row.period_selected_nodes) / denom;
            }
        }

        rows.push_back(row);
    }

    void write_csv() const {
        std::ostringstream ofs;
        ofs << "event,elapsed_ms,turn,materialized_turn,max_turn,beam_width,hash_capacity,";
        ofs << "active_width,selected_width,candidate_count,max_active_width,max_selected_width,beam_saturated_turns,";
        ofs << "expanded_nodes,generated_candidates,accepted_candidates,selected_nodes,";
        ofs << "pruned_by_width,pruned_by_hash,duplicate_by_hash,replaced_by_width,replaced_by_hash,invalid_candidates,hash_rebuild_count,";
        ofs << "live_nodes,max_live_nodes,";
        ofs << "time_limit_ms,time_remaining_ms,";
        ofs << "live_best_cost,live_worst_cost,live_avg_cost,";
        ofs << "selected_best_cost,selected_worst_cost,selected_avg_cost,";
        ofs << "width_pruned_cost_count,width_pruned_best_cost,pruned_best_gap,";
        ofs << "turns_since_best_update,expanded_nodes_since_best_update,ms_since_best_update,";
        ofs << "found,best_cost,best_turn,best_update_count,time_limit_reached,exhausted,completed_max_turn,";
        ofs << "period_ms,period_turns,period_expanded_nodes,period_generated_candidates,period_accepted_candidates,period_selected_nodes,";
        ofs << "period_pruned_by_width,period_pruned_by_hash,period_duplicate_by_hash,period_replaced_by_width,period_replaced_by_hash,period_invalid_candidates,period_hash_rebuild_count,period_best_updates,";
        ofs << "expand_per_sec,generate_per_sec,accept_per_sec,select_per_sec,accept_rate,width_prune_rate,hash_prune_rate,";
        ofs << "branching_factor,duplicate_rate,accepted_per_expanded,selected_per_expanded\n";
        ofs.setf(std::ios::fixed);
        ofs.precision(6);
        for (const Row& row : rows) {
            ofs << row.event << ','
                << row.elapsed_ms << ','
                << row.turn << ','
                << row.materialized_turn << ','
                << row.max_turn << ','
                << row.beam_width << ','
                << row.hash_capacity << ','
                << row.active_width << ','
                << row.selected_width << ','
                << row.candidate_count << ','
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
                << row.hash_rebuild_count << ','
                << row.live_nodes << ','
                << row.max_live_nodes << ','
                << row.time_limit_ms << ','
                << row.time_remaining_ms << ','
                << row.live_best_cost << ','
                << row.live_worst_cost << ','
                << row.live_avg_cost << ','
                << row.selected_best_cost << ','
                << row.selected_worst_cost << ','
                << row.selected_avg_cost << ','
                << row.width_pruned_cost_count << ','
                << row.width_pruned_best_cost << ','
                << row.pruned_best_gap << ','
                << row.turns_since_best_update << ','
                << row.expanded_nodes_since_best_update << ','
                << row.ms_since_best_update << ','
                << static_cast<int>(row.found) << ','
                << row.best_cost << ','
                << row.best_turn << ','
                << row.best_update_count << ','
                << static_cast<int>(row.time_limit_reached) << ','
                << static_cast<int>(row.exhausted) << ','
                << static_cast<int>(row.completed_max_turn) << ','
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
                << row.period_invalid_candidates << ','
                << row.period_hash_rebuild_count << ','
                << row.period_best_updates << ','
                << row.expand_per_sec << ','
                << row.generate_per_sec << ','
                << row.accept_per_sec << ','
                << row.select_per_sec << ','
                << row.accept_rate << ','
                << row.width_prune_rate << ','
                << row.hash_prune_rate << ','
                << row.branching_factor << ','
                << row.duplicate_rate << ','
                << row.accepted_per_expanded << ','
                << row.selected_per_expanded << '\n';
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
// exhausted=true は、時間制限による途中終了ではなく、展開可能なfrontierが尽きたことを表す。
template<class State>
struct Result {
    bool found = false;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;
    bool exhausted = false;
    // max_turn到着候補を少なくとも1件materializeした。
    // 全候補を通常幅で展開した保証ではない。
    bool completed_max_turn = false;

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
    template<class U>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, false, false);
    }

    template<class U, class B>
    requires StateArg<U, State> && BoolArg<B>
    bool push(U&& state, Cost cost, B finished) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, false, finished);
    }

    template<class U, class H>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), true, false);
    }

    template<class U, class H, class B>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push(U&& state, Cost cost, H hash, B finished) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), true, finished);
    }

    template<class Maker>
    bool push_lazy(Cost cost, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, false, false, std::forward<Maker>(maker));
    }

    template<class B, class Maker>
    requires BoolArg<B>
    bool push_lazy(Cost cost, B finished, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, false, finished, std::forward<Maker>(maker));
    }

    template<class H, class Maker>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), true, false, std::forward<Maker>(maker));
    }

    template<class H, class B, class Maker>
    requires CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push_lazy(Cost cost, H hash, B finished, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), true, finished, std::forward<Maker>(maker));
    }

private:
    friend class Engine<State>;

    // EmitterはEngineへの参照を直接保持し、push_lazyのMakerをテンプレートのまま即時転送する。
    // これにより、関数ポインタや型消去を挟まず、必要になった候補だけStateを生成できる。
    Emitter(Engine<State>& engine, Hash now_hash, int child_turn) noexcept
        : engine_(engine), now_hash_(now_hash), child_turn_(child_turn) {}

    template<class U>
    bool push_state_impl(U&& state, Cost cost, Hash hash, bool has_hash, bool finished) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return push_lazy_impl(cost, hash, has_hash, finished, maker);
    }

    template<class Maker>
    bool push_lazy_impl(Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
        return engine_.push_candidate(cost, hash, child_turn_, has_hash, finished, std::forward<Maker>(maker));
    }

    Engine<State>& engine_;
    Hash now_hash_{};
    int child_turn_ = 0;
};

private:

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
            if (used_[i] != epoch_) return Lookup{false, i};
            if (keys_[i] == key) return Lookup{true, i};
            i = (i + 1) & mask_;
        }
        return Lookup{false, -1};
    }

    int get(int slot) const { return values_[slot]; }

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
// 状態コピー型 beam 本体
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
        bool has_hash = false;
        bool finished = false;
        State state;

        template<class U>
        Item(U&& state_, Cost cost_, Hash hash_, int turn_, bool has_hash_, bool finished_)
            : cost(cost_),
              hash(hash_),
              turn(turn_),
              has_hash(has_hash_),
              finished(finished_),
              state(std::forward<U>(state_)) {}
    };

    // 次frontier候補用のSelector。push時点で beam_width 個だけを保持する。
    // 幅制限の比較はcost-only。同costでは既存候補を優先し、hashではtie-breakしない。
    // hashは同一frontier内の重複排除とStateView用にだけ使う。
    // State生成前に cost/hash で受理可能性を判定するため、push_lazy と通常pushの両方で内部保存の無駄を抑えられる。
    class Selector {
    public:
        // push結果はホットパスで返すため、複数boolではなくbit flagsにまとめる。
        struct PushResult {
            enum Flag : std::uint8_t {
                Accepted        = 1u << 0,
                Duplicate       = 1u << 1,
                HashFull        = 1u << 2,
                Appended        = 1u << 3,
                ReplacedByWidth = 1u << 4,
                ReplacedByHash  = 1u << 5,
                HashRebuilt     = 1u << 6,
            };
            std::uint8_t flags = 0;
            constexpr PushResult() = default;
            constexpr explicit PushResult(unsigned f) : flags(static_cast<std::uint8_t>(f)) {}

            [[nodiscard]] constexpr bool has(Flag flag) const noexcept {
                return (flags & static_cast<std::uint8_t>(flag)) != 0;
            }
        };

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

        template<class Maker>
        PushResult push(Cost cost, Hash hash, int turn, bool has_hash, bool finished, Maker&& maker) {
            // まずcostだけで幅棄却を行う。ここで落ちた候補はhash重複統計には入らない。
            if (full() && !candidate_better_than_worst(cost)) return {};
            return (use_hash_ && has_hash)
                ? push_hashed(cost, hash, turn, finished, std::forward<Maker>(maker))
                : push_plain(cost, hash, turn, has_hash, finished, std::forward<Maker>(maker));
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
        void append_candidate(Cost cost, Hash hash, int turn, bool has_hash, bool finished, Maker&& maker) {
            candidates_.emplace_back(std::invoke(std::forward<Maker>(maker)), cost, hash, turn, has_hash, finished);
            const int j = static_cast<int>(candidates_.size()) - 1;
            costs_[j] = cost;
            if (static_cast<int>(candidates_.size()) == beam_width_) build_segment_tree();
        }

        template<class Maker>
        void replace_candidate(int j, Cost cost, Hash hash, int turn, bool has_hash, bool finished, Maker&& maker) {
            Item& item = candidates_[j];
            item.cost = cost;
            item.hash = hash;
            item.turn = turn;
            item.has_hash = has_hash;
            item.finished = finished;
            item.state = std::invoke(std::forward<Maker>(maker));
            costs_[j] = cost;
            if (full()) seg_set(j);
        }

        template<class Maker>
        PushResult push_plain(Cost cost, Hash hash, int turn, bool has_hash, bool finished, Maker&& maker) {
            if (!full()) {
                append_candidate(cost, hash, turn, has_hash, finished, std::forward<Maker>(maker));
                return PushResult(PushResult::Accepted | PushResult::Appended);
            }
            const int j = worst_index_cache;
            if (j < 0) return {};
            replace_candidate(j, cost, hash, turn, has_hash, finished, std::forward<Maker>(maker));
            return PushResult(PushResult::Accepted | PushResult::ReplacedByWidth);
        }

        template<class Maker>
        PushResult push_hashed(Cost cost, Hash hash, int turn, bool finished, Maker&& maker) {
            auto found = hash_.lookup(hash);
            if (found.found) {
                const int j = hash_.get(found.slot);
                const bool live = 0 <= j && j < static_cast<int>(candidates_.size()) &&
                                  candidates_[j].has_hash && candidates_[j].hash == hash;
                if (live) {
                    if (!(cost < costs_[j])) return PushResult(PushResult::Duplicate);
                    replace_candidate(j, cost, hash, turn, true, finished, std::forward<Maker>(maker));
                    hash_.set(found.slot, hash, j);
                    return PushResult(PushResult::Accepted | PushResult::Duplicate | PushResult::ReplacedByHash);
                }
                // stale entry なので、この slot を上書きしてよい。候補本体は既に別候補で置換されている。
            }

            bool hash_rebuilt = false;
            if (found.slot < 0) {
                rebuild_hash();
                hash_rebuilt = true;
                found = hash_.lookup(hash);
                if (found.slot < 0) return PushResult(PushResult::HashFull | PushResult::HashRebuilt);
            }

            if (!full()) {
                const int j = static_cast<int>(candidates_.size());
                append_candidate(cost, hash, turn, true, finished, std::forward<Maker>(maker));
                hash_.set(found.slot, hash, j);
                return PushResult(PushResult::Accepted | PushResult::Appended | (hash_rebuilt ? PushResult::HashRebuilt : 0));
            }

            const int j = worst_index_cache;
            if (j < 0) return {};
            replace_candidate(j, cost, hash, turn, true, finished, std::forward<Maker>(maker));
            hash_.set(found.slot, hash, j);
            return PushResult(PushResult::Accepted | PushResult::ReplacedByWidth | (hash_rebuilt ? PushResult::HashRebuilt : 0));
        }

        void rebuild_hash() {
            if (!use_hash_) return;
            hash_.clear();
            for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
                if (!candidates_[i].has_hash) continue;
                auto slot = hash_.lookup(candidates_[i].hash);
                if (slot.slot >= 0) hash_.set(slot.slot, candidates_[i].hash, i);
            }
        }

        void update_worst_cache() {
            worst_index_cache = seg_[1];
            if (worst_index_cache >= 0) worst_cost_cache = costs_[worst_index_cache];
        }

        void build_segment_tree() {
            std::fill(seg_.begin(), seg_.end(), -1);
            for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = i;
            for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
            update_worst_cache();
        }

        void seg_set(int pos) {
            int i = seg_n_ + pos;
            seg_[i] = pos;
            while (i >>= 1) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
            update_worst_cache();
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
        start_clock_us = detail::now_microseconds();
        limit_us = detail::time_limit_to_microseconds(param.time_limit_ms);
        use_time_limit = (limit_us > 0);
        debug_enabled = !std::same_as<std::decay_t<DebugHook>, NoOp>;

        initialize_search();

        rt.turn = 0;
        rt.max_turn = param.max_turn;
        rt.beam_width = param.beam_width;
        rt.hash_capacity = param.hash_capacity;
        rt.time_limit_ms = param.time_limit_ms;
        rt.best_cost = initial_cost;
        rt.best_turn = 0;
        rt.materialized_turn = 0;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) {
            rt.time_limit_reached = true;
        }
        emit_hook(EventType::RunStart, debug_hook);
        if (!frontier.empty() && is_answer(frontier.front())) set_best(frontier.front(), debug_hook);
        rt.completed_max_turn = (param.max_turn == 0);
        if (!rt.completed_max_turn) run_loop(expand, debug_hook);

        finalize_runtime_for_result();
        emit_hook(EventType::RunEnd, debug_hook);
        return build_result();
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

    std::vector<Item> frontier;
    Selector selector;
    std::optional<State> best_state;

    int last_candidate_count = 0;
    long long expanded_at_last_best_update = 0;
    std::int64_t last_best_update_us = 0;

    // ユーザー指定値を内部で扱いやすい範囲へ正規化する。
    // hash_capacity はSelector保持数が高々beam_width個であることを前提に丸める。
    void normalize_param() {
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
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

    // frontierとselectorは1本ずつ持つ。探索開始時に初期状態をfrontierへ置く。
    void initialize_search() {
        frontier.clear();
        frontier.reserve(std::max(1, param.beam_width));
        selector.init(param.beam_width, param.hash_capacity, param.use_hash_dedup);
        frontier.emplace_back(std::move(initial_state), initial_cost, initial_hash, 0, initial_has_hash, false);
        refresh_runtime_usage();
    }

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
        rt.time_remaining_ms = use_time_limit
            ? std::max(0.0, static_cast<double>(limit_us - rt.elapsed_us) * 0.001)
            : -1.0;
    }

    void refresh_runtime_usage() {
        // 保持数はfrontierとselectorの実サイズから取る。手動カウンタを持たず、状態遷移時のずれを防ぐ。
        rt.live_nodes = static_cast<int>(frontier.size()) + selector.size();
        rt.max_live_nodes = std::max(rt.max_live_nodes, rt.live_nodes);
    }

    void update_width_pruned_debug(Cost cost) {
        if (!debug_enabled) return;
        if (rt.width_pruned_cost_count == 0 || cost < rt.width_pruned_best_cost) rt.width_pruned_best_cost = cost;
        ++rt.width_pruned_cost_count;
    }

    void update_best_stagnation_stats() {
        if (rt.best_update_count > 0) {
            rt.turns_since_best_update = std::max(0, rt.materialized_turn - rt.best_turn);
            rt.expanded_nodes_since_best_update = rt.expanded_nodes - expanded_at_last_best_update;
            rt.ms_since_best_update = static_cast<double>(rt.elapsed_us - last_best_update_us) * 0.001;
        } else {
            rt.turns_since_best_update = 0;
            rt.expanded_nodes_since_best_update = 0;
            rt.ms_since_best_update = 0.0;
        }
    }

    void finalize_runtime_for_result() {
        // RunEnd hook と Result::runtime に同じ最終snapshotを渡す。
        refresh_runtime_usage();
        update_time();
        update_debug_snapshot();
    }

    void update_debug_snapshot() {
        const int current_selector_size = selector.size();
        rt.candidate_count = current_selector_size > 0 ? current_selector_size : last_candidate_count;

        // NoOp実行ではmaterialize中にselected_*を計算しない。
        // RunEndの最終snapshotでは、現在保持しているfrontierから必要な値だけ復元する。
        // DebugHook有効時はmaterialize_arrival()で直近materializeの値を保持する。
        if (!debug_enabled) {
            bool has_selected = false;
            double selected_sum = 0.0;
            int selected_count = 0;
            for (const Item& item : frontier) {
                if (!has_selected) {
                    rt.selected_best_cost = rt.selected_worst_cost = item.cost;
                    has_selected = true;
                } else {
                    if (item.cost < rt.selected_best_cost) rt.selected_best_cost = item.cost;
                    if (rt.selected_worst_cost < item.cost) rt.selected_worst_cost = item.cost;
                }
                selected_sum += static_cast<double>(item.cost);
                ++selected_count;
            }
            rt.selected_avg_cost = selected_count > 0 ? selected_sum / selected_count : 0.0;
            if (!has_selected) {
                rt.selected_best_cost = Cost{};
                rt.selected_worst_cost = Cost{};
            }
        }

        bool has_live = false;
        double live_sum = 0.0;
        int live_count = 0;
        auto add_live = [&](Cost cost) {
            if (!has_live) {
                rt.live_best_cost = rt.live_worst_cost = cost;
                has_live = true;
            } else {
                if (cost < rt.live_best_cost) rt.live_best_cost = cost;
                if (rt.live_worst_cost < cost) rt.live_worst_cost = cost;
            }
            live_sum += static_cast<double>(cost);
            ++live_count;
        };
        for (const Item& item : frontier) add_live(item.cost);
        for (const Item& item : selector.selected()) add_live(item.cost);
        if (live_count > 0) {
            rt.live_avg_cost = live_sum / live_count;
        } else {
            rt.live_best_cost = rt.live_worst_cost = Cost{};
            rt.live_avg_cost = 0.0;
        }

        if (rt.selected_width > 0 && rt.width_pruned_cost_count > 0) {
            rt.pruned_best_gap = static_cast<double>(rt.width_pruned_best_cost) - static_cast<double>(rt.selected_worst_cost);
        } else {
            rt.pruned_best_gap = 0.0;
        }

        update_best_stagnation_stats();
    }

    template<class DebugHook>
    void emit_hook(EventType event_type, DebugHook& debug_hook) {
        // BestUpdate は瞬間イベントなので、CSV出力や重いスナップショット更新は行わない。
        // 独自Hookには通知するが、frontier/selector由来のsnapshot統計は直前の値のまま渡される。
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
    bool push_candidate(Cost cost, Hash hash, int turn, bool has_hash, bool finished, Maker&& maker) {
        static_assert(std::invocable<Maker&&>, "maker must be callable with no arguments");
        static_assert(std::constructible_from<State, std::invoke_result_t<Maker&&>>, "maker result must be constructible as State");
        ++rt.generated_candidates;
        // NaN cost や最大ターン超過は、幅やhashではなく不正候補として扱う。
        if (!detail::is_valid_cost(cost) || turn > param.max_turn) {
            ++rt.invalid_candidates;
            return false;
        }
        const auto res = selector.push(cost, hash, turn, has_hash, finished, std::forward<Maker>(maker));
        if (res.has(Selector::PushResult::Duplicate)) ++rt.duplicate_by_hash;
        if (res.has(Selector::PushResult::HashRebuilt)) ++rt.hash_rebuild_count;
        if (!res.has(Selector::PushResult::Accepted)) {
            if (res.has(Selector::PushResult::Duplicate) || res.has(Selector::PushResult::HashFull)) {
                ++rt.pruned_by_hash;
            } else {
                ++rt.pruned_by_width;
                update_width_pruned_debug(cost);
            }
            return false;
        }
        ++rt.accepted_candidates;
        if (res.has(Selector::PushResult::ReplacedByWidth)) ++rt.replaced_by_width;
        if (res.has(Selector::PushResult::ReplacedByHash)) ++rt.replaced_by_hash;
        if (res.has(Selector::PushResult::Appended)) refresh_runtime_usage();
        return true;
    }

    template<class Expand>
    void expand_item(Item& item, Expand& expand) {
        StateView<State> now(item.turn, item.cost, item.state, item.hash, item.has_hash);
        Emitter<State> emit(*this, item.hash, item.turn + 1);
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

    bool is_answer(const Item& item) const {
        return item.finished || (param.max_turn_is_answer && item.turn == param.max_turn);
    }

    template<class Expand, class DebugHook>
    void run_loop(Expand& expand, DebugHook& debug_hook) {
        bool minimal_mode = rt.time_limit_reached;
        while (!frontier.empty()) {
            rt.turn = frontier.front().turn;
            rt.materialized_turn = std::max(rt.materialized_turn, rt.turn);
            if (rt.turn >= param.max_turn) {
                rt.completed_max_turn = true;
                break;
            }

            begin_turn(debug_hook);
            minimal_mode |= rt.time_limit_reached;
            if (use_time_limit) minimal_mode = expand_turn_time_checked(minimal_mode, expand);
            else expand_turn_full(expand);

            frontier.clear();
            refresh_runtime_usage();
            materialize_arrival(minimal_mode, debug_hook);
            rt.exhausted = frontier.empty();
            end_turn(debug_hook);
            minimal_mode |= rt.time_limit_reached;

            if (rt.completed_max_turn) break;
        }
        rt.exhausted = frontier.empty();
    }

    template<class DebugHook>
    void begin_turn(DebugHook& debug_hook) {
        if (!frontier.empty()) rt.turn = frontier.front().turn;
        rt.active_width = 0;
        rt.selected_width = 0;
        rt.candidate_count = 0;
        last_candidate_count = 0;
        if (debug_enabled) {
            rt.selected_best_cost = Cost{};
            rt.selected_worst_cost = Cost{};
            rt.selected_avg_cost = 0.0;
            rt.width_pruned_cost_count = 0;
            rt.width_pruned_best_cost = Cost{};
            rt.pruned_best_gap = 0.0;
        }
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

    // time_limit_ms==0用の通常探索ホットパス。縮退探索用の分岐を混ぜない。
    template<class Expand>
    void expand_turn_full(Expand& expand) {
        const int n = static_cast<int>(frontier.size());
        if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
            for (int i = 0; i < n; ++i) {
                ++rt.active_width;
                expand_item(frontier[i], expand);
            }
        } else {
            rt.active_width = n;
            rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
            for (int i = 0; i < n; ++i) expand_item(frontier[i], expand);
        }
    }

    // 通常モードではfrontierを順に展開する。
    // 時間到達後は通常展開を止め、未展開部分から最良1本だけ追加で展開する。
    template<class Expand>
    bool expand_turn_time_checked(bool minimal_mode, Expand& expand) {
        auto expand_best_from = [&](int begin) {
            const int best = best_item_index(frontier, begin);
            if (best < 0) return;
            ++rt.active_width;
            rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
            expand_item(frontier[best], expand);
        };

        int next_index = 0;
        if (!minimal_mode) {
            for (; next_index < static_cast<int>(frontier.size()); ++next_index) {
                ++rt.active_width;
                rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
                expand_item(frontier[next_index], expand);
                if (need_switch_to_minimal_by_time()) {
                    minimal_mode = true;
                    ++next_index;
                    break;
                }
            }
        }

        if (minimal_mode) expand_best_from(next_index);
        return minimal_mode;
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
        expanded_at_last_best_update = rt.expanded_nodes;
        update_time();
        last_best_update_us = rt.elapsed_us;
        emit_hook(EventType::BestUpdate, debug_hook);
    }

    template<class DebugHook>
    void materialize_arrival(bool minimal, DebugHook& debug_hook) {
        // Selector内の候補を次frontierへ移す。
        // minimal=true ではSelector内の最良cost 1件だけを移し、他は破棄する。
        // finished候補は解候補としてbest判定するが、元版と同じく自動では展開停止しない。
        auto& selected = selector.selected();
        const int original_count = static_cast<int>(selected.size());
        const int best_idx = minimal ? best_item_index(selected) : -1;

        rt.candidate_count = original_count;
        rt.selected_width = minimal ? (best_idx >= 0 ? 1 : 0) : original_count;
        rt.max_selected_width = std::max(rt.max_selected_width, rt.selected_width);
        if (rt.selected_width >= param.beam_width) ++rt.beam_saturated_turns;
        last_candidate_count = original_count;
        if (debug_enabled) {
            bool has = false;
            double cost_sum = 0.0;
            int count = 0;
            for (int i = 0; i < original_count; ++i) {
                if (minimal && i != best_idx) continue;
                const Item& cand = selected[i];
                if (!has) {
                    rt.selected_best_cost = rt.selected_worst_cost = cand.cost;
                    has = true;
                } else {
                    if (cand.cost < rt.selected_best_cost) rt.selected_best_cost = cand.cost;
                    if (rt.selected_worst_cost < cand.cost) rt.selected_worst_cost = cand.cost;
                }
                cost_sum += static_cast<double>(cand.cost);
                ++count;
            }
            if (count > 0) rt.selected_avg_cost = cost_sum / count;
        }

        if (frontier.capacity() < static_cast<std::size_t>(param.beam_width)) frontier.reserve(param.beam_width);
        if (minimal) {
            if (best_idx >= 0) materialize_one(selected[best_idx], debug_hook);
        } else {
            for (Item& item : selected) materialize_one(item, debug_hook);
        }

        rt.selected_nodes += rt.selected_width;
        selector.clear();
        refresh_runtime_usage();
    }

    // best_stateはmove前にコピーする。これにより返却されるbest_stateはfrontierの寿命に依存しない。
    template<class DebugHook>
    void materialize_one(Item& item, DebugHook& debug_hook) {
        rt.materialized_turn = std::max(rt.materialized_turn, item.turn);
        if (is_answer(item)) set_best(item, debug_hook);
        rt.completed_max_turn |= (item.turn == param.max_turn);
        frontier.push_back(std::move(item));
    }

    Result<State> build_result() {
        Result<State> res;
        res.found = rt.found;
        res.time_limit_reached = rt.time_limit_reached;
        res.exhausted = rt.exhausted;
        res.completed_max_turn = rt.completed_max_turn;
        res.best_cost = rt.best_cost;
        res.best_turn = rt.best_turn;
        res.best_state = std::move(best_state);
        res.runtime = rt;
        return res;
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
    // 第4引数が整数ならHashあり版だけにマッチさせ、誤ったoverload選択を避ける。
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

static_assert(bs::CostType<int>);
static_assert(bs::CostType<double>);
static_assert(bs::CostType<long double>);
static_assert(!bs::CostType<unsigned int>);
static_assert(!bs::CostType<bool>);
static_assert(bs::HashType<std::uint32_t>);
static_assert(bs::HashType<std::uint64_t>);
static_assert(!bs::HashType<int>);
static_assert(!bs::HashType<bool>);
using BS = bs::Beam<int, std::uint64_t>;
using BS32 = bs::Beam<int, std::uint32_t>;
static_assert(std::same_as<decltype(BS::Param{}.time_limit_ms), double>);
static_assert(std::same_as<decltype(BS::Runtime{}.time_limit_ms), double>);

static_assert(bs::CompatibleHashArg<std::uint64_t, std::uint64_t>);
static_assert(bs::CompatibleHashArg<unsigned long long, std::uint64_t>);
static_assert(bs::CompatibleHashArg<unsigned int, std::uint64_t>);
static_assert(!bs::CompatibleHashArg<int, std::uint64_t>);
static_assert(!bs::CompatibleHashArg<bool, std::uint64_t>);

static std::int64_t test_now_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

#define BS_TEST_ASSERT(expr) do { \
    if (!(expr)) { \
        std::cerr << "[test] assertion failed: " #expr " at line " << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (false)

struct SmallState {
    int depth = 0;
    int x = 0;
    std::vector<int> actions;
};

struct TinyState {
    int depth = 0;
    int v = 0;
    int checksum = 0;
};

static_assert(!std::default_initializable<BS::Emitter<SmallState>>);

struct RefItem {
    TinyState state;
    int cost = 0;
    std::uint64_t hash = 0;
    bool finished = false;
};

struct ExplicitEmitterExpand {
    void operator()(const BS32::StateView<SmallState>& now, BS32::Emitter<SmallState>& emit) const {
        if (now.turn != 0) return;
        BS_TEST_ASSERT(!now.has_hash || now.hash == std::uint32_t{0});
        SmallState a = now.state;
        a.depth = 1;
        a.x = 111;
        SmallState b = now.state;
        b.depth = 1;
        b.x = 321;
        emit.push(std::move(a), 9, std::uint32_t{321}, true);
        emit.push(std::move(b), 4, std::uint32_t{321}, true);
    }
};

struct ExplicitEmitterRuntimeExpand {
    void operator()(const BS32::StateView<SmallState>& now, const BS32::Runtime& rt, BS32::Emitter<SmallState>& emit) const {
        BS_TEST_ASSERT(rt.max_turn == 1);
        if (now.turn != 0) return;
        BS_TEST_ASSERT(!now.has_hash);
        SmallState s = now.state;
        s.depth = 1;
        s.x = 654;
        emit.push_lazy(6, std::uint32_t{654}, true, [&]() { return s; });
    }
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

static std::pair<int, TinyState> run_reference(int seed, int max_turn, int beam_width, int branch, bool use_hash) {
    std::vector<RefItem> frontier;
    std::vector<RefItem> bucket;
    TinyState init{0, seed % 17, seed * 3 + 1};
    frontier.push_back(RefItem{init, 0, static_cast<std::uint64_t>(init.v), false});
    bool found = false;
    int best_cost = 0;
    TinyState best{};

    while (!frontier.empty()) {
        bucket.clear();
        for (const RefItem& now : frontier) {
            if (now.state.depth >= max_turn) continue;
            for (int k = 0; k < branch; ++k) {
                TinyState nx;
                nx.depth = now.state.depth + 1;
                nx.v = static_cast<int>((now.state.v * 37LL + now.state.checksum * 11LL + k * 19LL + nx.depth * 7LL + seed) % 1009);
                nx.checksum = static_cast<int>((now.state.checksum * 43LL + nx.v * 5LL + k + nx.depth) % 1000003);
                int delta = static_cast<int>((test_mix(nx.v + nx.checksum * 3ULL + k * 1009ULL + seed) % 41));
                int cost = now.cost + delta;
                std::uint64_t h = use_hash ? static_cast<std::uint64_t>(nx.v) : now.hash;
                bool finished = (nx.depth >= max_turn);
                ref_push(bucket, beam_width, RefItem{nx, cost, h, finished}, use_hash);
            }
        }
        frontier.clear();
        for (RefItem& item : bucket) {
            if (item.finished) {
                if (!found || item.cost < best_cost) {
                    found = true;
                    best_cost = item.cost;
                    best = item.state;
                }
            } else {
                frontier.push_back(item);
            }
        }
    }
    BS_TEST_ASSERT(found || max_turn == 0);
    return {best_cost, best};
}

static void test_edge_cases() {
    {
        BS::Param param;
        int expand_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>&, auto&) { ++expand_calls; };
        auto res = BS::run(param, SmallState{0, 42, {}}, 7, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(!res.exhausted);
        BS_TEST_ASSERT(res.best_cost == 7);
        BS_TEST_ASSERT(res.best_state->x == 42);
        BS_TEST_ASSERT(expand_calls == 0);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 0);
        BS_TEST_ASSERT(res.runtime.generated_candidates == 0);
    }
    {
        BS32::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        auto res = BS32::run(param, SmallState{}, 0, std::uint32_t{0}, ExplicitEmitterExpand{});
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 4);
        BS_TEST_ASSERT(res.best_state->x == 321);
        BS_TEST_ASSERT(res.runtime.duplicate_by_hash == 1);
        BS_TEST_ASSERT(res.runtime.replaced_by_hash == 1);
    }
    {
        using BSD = bs::Beam<double, std::uint64_t>;
        BSD::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        auto expand = [](const BSD::StateView<SmallState>& now, BSD::Emitter<SmallState>& emit) {
            if (now.turn != 0) return;
            SmallState bad = now.state;
            bad.depth = 1;
            bad.x = 1;
            emit.push(std::move(bad), std::numeric_limits<double>::quiet_NaN(), std::uint64_t{1}, true);
            SmallState good = now.state;
            good.depth = 1;
            good.x = 2;
            emit.push(std::move(good), 3.0, std::uint64_t{2}, true);
        };
        auto res = BSD::run(param, SmallState{}, 0.0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3.0);
        BS_TEST_ASSERT(res.best_state->x == 2);
        BS_TEST_ASSERT(res.runtime.invalid_candidates == 1);
    }
    {
        BS32::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        auto res = BS32::run(param, SmallState{}, 0, ExplicitEmitterRuntimeExpand{});
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 6);
        BS_TEST_ASSERT(res.best_state->x == 654);
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 1);
    }
    {
        // std::uint64_t が unsigned long の環境でも、unsigned long long リテラルを自然に渡せることを確認する。
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 4;
        auto expand = [](const BS::StateView<SmallState>& now, BS::Emitter<SmallState>& emit) {
            if (now.turn != 0) return;
            SmallState a = now.state;
            a.depth = 1;
            a.x = 10;
            emit.push(std::move(a), 10, 1ULL, true);
            emit.push_lazy(3, 2ULL, true, [&now]() {
                SmallState b = now.state;
                b.depth = 1;
                b.x = 20;
                return b;
            });
        };
        auto res = BS::run(param, SmallState{}, 0, 0ULL, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3);
        BS_TEST_ASSERT(res.best_state->x == 20);
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 2);
    }
    {
        // NoOp実行でも、best更新後の停滞統計がRunEnd時点で更新されることを確認する。
        BS::Param param;
        param.max_turn = 3;
        param.max_turn_is_answer = false;
        param.beam_width = 4;
        auto expand = [](const BS::StateView<SmallState>& now, BS::Emitter<SmallState>& emit) {
            if (now.state.x == 1) return;
            if (now.turn == 0) {
                SmallState best = now.state;
                best.depth = 1;
                best.x = 1;
                emit.push(std::move(best), 1, true);
                SmallState cont = now.state;
                cont.depth = 1;
                cont.x = 2;
                emit.push(std::move(cont), 100, false);
            } else if (now.turn < 3 && now.state.x == 2) {
                SmallState cont = now.state;
                cont.depth = now.turn + 1;
                cont.x = 2;
                emit.push(std::move(cont), 100 + now.turn, false);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 1);
        BS_TEST_ASSERT(res.runtime.materialized_turn == 3);
        BS_TEST_ASSERT(res.runtime.turns_since_best_update == 2);
        BS_TEST_ASSERT(res.runtime.expanded_nodes_since_best_update > 0);
        BS_TEST_ASSERT(res.runtime.ms_since_best_update >= 0.0);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 3;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth == 0) {
                SmallState s = now.state;
                s.depth = 1;
                s.x = 10;
                emit.push(std::move(s), 5, true);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_state->x == 10);
        BS_TEST_ASSERT(!res.exhausted);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.runtime.selected_nodes == 1);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.beam_width = 10;
        int expanded_finished = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth == 0) {
                SmallState done = now.state;
                done.depth = 1;
                done.x = 1;
                emit.push(std::move(done), 1, true);
                SmallState cont = now.state;
                cont.depth = 1;
                cont.x = 2;
                emit.push(std::move(cont), 100);
            } else {
                if (now.state.x == 1) ++expanded_finished;
                else BS_TEST_ASSERT(now.state.x == 2);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 1);
        BS_TEST_ASSERT(res.best_state->x == 1);
        BS_TEST_ASSERT(expanded_finished == 1);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 3);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_turn_is_answer = false;
        param.beam_width = 4;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) {
                SmallState finished = now.state;
                finished.depth = 1;
                finished.x = 10;
                emit.push(std::move(finished), 1, true);
                SmallState cont = now.state;
                cont.depth = 1;
                cont.x = 20;
                emit.push(std::move(cont), 100, false);
            } else if (now.turn == 1 && now.state.x == 20) {
                SmallState leaf = now.state;
                leaf.depth = 2;
                leaf.x = 30;
                emit.push(std::move(leaf), 200, false);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.best_turn == 1);
        BS_TEST_ASSERT(res.runtime.materialized_turn == 2);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 10;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth != 0) return;
            SmallState a = now.state; a.depth = 1; a.x = 10;
            SmallState b = now.state; b.depth = 1; b.x = 5;
            emit.push(std::move(a), 10, std::uint64_t{123}, true);
            emit.push(std::move(b), 5, std::uint64_t{123}, true);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_state->x == 5);
        BS_TEST_ASSERT(res.runtime.duplicate_by_hash == 1);
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 2);
        BS_TEST_ASSERT(res.runtime.replaced_by_hash == 1);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth != 0) return;
            SmallState a = now.state; a.depth = 1; a.x = 10;
            SmallState b = now.state; b.depth = 1; b.x = 5;
            emit.push(std::move(a), 10, std::uint64_t{10}, true);
            emit.push(std::move(b), 5, std::uint64_t{5}, true);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
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
        param.beam_width = 1;
        int maker_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth != 0) return;
            emit.push_lazy(1, std::uint64_t{1}, true, [&]() {
                ++maker_calls;
                SmallState s = now.state;
                s.depth = 1;
                s.x = 1;
                return s;
            });
            emit.push_lazy(100, std::uint64_t{2}, true, [&]() {
                ++maker_calls;
                SmallState s = now.state;
                s.depth = 1;
                s.x = 100;
                return s;
            });
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 1);
        BS_TEST_ASSERT(maker_calls == 1);
        BS_TEST_ASSERT(res.runtime.pruned_by_width == 1);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        auto expand = [](const BS::StateView<SmallState>& now, const BS::Runtime& rt, auto& emit) {
            BS_TEST_ASSERT(now.cost == rt.best_cost || !rt.found);
            if (now.state.depth != 0) return;
            SmallState s = now.state;
            s.depth = 1;
            s.x = 77;
            emit.push(std::move(s), 3, std::uint64_t{77}, true);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 3);
        BS_TEST_ASSERT(res.best_state->x == 77);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        param.use_hash_dedup = false;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth != 0) return;
            SmallState a = now.state; a.depth = 1; a.x = 1;
            SmallState b = now.state; b.depth = 1; b.x = 2;
            emit.push(std::move(a), 1, std::uint64_t{9}, true);
            emit.push(std::move(b), 2, std::uint64_t{9}, true);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.runtime.duplicate_by_hash == 0);
        BS_TEST_ASSERT(res.runtime.selected_nodes == 2);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 1;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth != 0) return;
            SmallState a = now.state; a.depth = 1; a.x = 10;
            SmallState b = now.state; b.depth = 1; b.x = 1;
            emit.push(std::move(a), 5, std::uint64_t{100}, true);
            emit.push(std::move(b), 5, std::uint64_t{1}, true);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_state->x == 10);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 5;
        param.hash_capacity = 1000000;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth != 0) return;
            SmallState s = now.state;
            s.depth = 1;
            s.x += 1;
            emit.push(std::move(s), now.cost + 1, std::uint64_t{1}, true);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.runtime.hash_capacity == 5 * 4 + 17);
    }
    {
        const char* filename = "beam_copy_single_v28_stat.csv";
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        BS::CsvStatHook hook(filename, 0.0);
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth == 0) {
                SmallState a = now.state; a.depth = 1; a.x = 1;
                SmallState b = now.state; b.depth = 1; b.x = 2;
                emit.push(std::move(a), 1, std::uint64_t{1}, true);
                emit.push(std::move(b), 2, std::uint64_t{2}, true);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand, hook);
        BS_TEST_ASSERT(res.found);
        std::ifstream ifs(filename);
        BS_TEST_ASSERT(ifs.good());
        std::string header;
        std::getline(ifs, header);
        const std::string removed_selector_avg = std::string("selector_") + "avg_size";
        const std::string removed_selector_rate = std::string("selector_") + "full_rate";
        const std::string removed_hash_full = std::string("hash_") + "table_full";
        BS_TEST_ASSERT(header.find("materialized_turn") != std::string::npos);
        BS_TEST_ASSERT(header.find("beam_saturated_turns") != std::string::npos);
        BS_TEST_ASSERT(header.find("turns_since_best_update") != std::string::npos);
        BS_TEST_ASSERT(header.find("exhausted") != std::string::npos);
        BS_TEST_ASSERT(header.find("completed_max_turn") != std::string::npos);
        BS_TEST_ASSERT(header.find("period_turns") != std::string::npos);
        BS_TEST_ASSERT(header.find("selector_full") == std::string::npos);
        BS_TEST_ASSERT(header.find("pending_candidates") == std::string::npos);
        BS_TEST_ASSERT(header.find("max_pending_candidates") == std::string::npos);
        BS_TEST_ASSERT(header.find("reached_turn") == std::string::npos);
        BS_TEST_ASSERT(header.find(removed_selector_avg) == std::string::npos);
        BS_TEST_ASSERT(header.find(removed_selector_rate) == std::string::npos);
        BS_TEST_ASSERT(header.find(removed_hash_full) == std::string::npos);
        BS_TEST_ASSERT(!hook.rows.empty());
        bool saw_turn_end = false;
        bool saw_full_candidate_turn = false;
        for (const auto& row : hook.rows) {
            BS_TEST_ASSERT(std::string(row.event) != "BestUpdate");
            BS_TEST_ASSERT(std::string(row.event).find("Frontier") == std::string::npos);
            if (std::string(row.event) == "TurnEnd") {
                saw_turn_end = true;
                if (row.candidate_count == 2 && row.beam_saturated_turns == 1 && row.completed_max_turn) {
                    saw_full_candidate_turn = true;
                }
            }
        }
        BS_TEST_ASSERT(saw_turn_end);
        BS_TEST_ASSERT(saw_full_candidate_turn);
    }
    {
        constexpr int target_depth = 3;
        BS::Param param;
        param.max_turn = target_depth;
        param.beam_width = 4;
        param.time_limit_ms = 1;
        param.time_check_interval = 1;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth == 0) std::this_thread::sleep_for(std::chrono::milliseconds(3));
            if (now.state.depth >= target_depth) return;
            for (int k = 0; k < 4; ++k) {
                SmallState s = now.state;
                ++s.depth;
                s.x = now.state.x * 10 + k;
                const bool finished = (s.depth >= target_depth);
                emit.push(std::move(s), now.cost + k, static_cast<std::uint64_t>(100 + s.depth * 10 + k), finished);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.time_limit_reached);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_state->depth == target_depth);
        BS_TEST_ASSERT(res.runtime.max_selected_width == 1);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 4;
        param.time_limit_ms = 1;
        param.time_check_interval = 1;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            if (now.state.depth != 0) return;
            SmallState s = now.state;
            s.depth = 1;
            s.x = 1;
            emit.push(std::move(s), 1, true);
        };
        auto res = BS::run(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.time_limit_reached);
    }
}


static void test_max_turn_cases() {
    {
        BS::Param param;
        param.max_turn = 0;
        param.max_turn_is_answer = true;
        int expand_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>&, auto&) {
            ++expand_calls;
        };
        auto res = BS::run(param, SmallState{0, 99, {}}, 7, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(!res.exhausted);
        BS_TEST_ASSERT(res.best_cost == 7);
        BS_TEST_ASSERT(res.best_turn == 0);
        BS_TEST_ASSERT(res.best_state->x == 99);
        BS_TEST_ASSERT(expand_calls == 0);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 0);
    }
    {
        BS::Param param;
        param.max_turn = -1;
        param.max_turn_is_answer = true;
        int expand_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>&, auto&) {
            ++expand_calls;
        };
        auto res = BS::run(param, SmallState{0, 88, {}}, 9, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(!res.exhausted);
        BS_TEST_ASSERT(res.runtime.max_turn == 0);
        BS_TEST_ASSERT(res.best_cost == 9);
        BS_TEST_ASSERT(res.best_turn == 0);
        BS_TEST_ASSERT(res.best_state->x == 88);
        BS_TEST_ASSERT(expand_calls == 0);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 0);
    }
    {
        BS::Param param;
        param.max_turn = -5;
        param.max_turn_is_answer = false;
        int expand_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>&, auto&) {
            ++expand_calls;
        };
        auto res = BS::run(param, SmallState{0, 77, {}}, 4, expand);
        BS_TEST_ASSERT(!res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(!res.exhausted);
        BS_TEST_ASSERT(res.runtime.max_turn == 0);
        BS_TEST_ASSERT(expand_calls == 0);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 0);
    }
    {
        BS::Param param;
        param.max_turn = 0;
        param.max_turn_is_answer = false;
        int expand_calls = 0;
        auto expand = [&](const BS::StateView<SmallState>&, auto&) {
            ++expand_calls;
        };
        auto res = BS::run(param, SmallState{0, 99, {}}, 7, expand);
        BS_TEST_ASSERT(!res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(!res.exhausted);
        BS_TEST_ASSERT(expand_calls == 0);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 0);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.max_turn_is_answer = true;
        param.beam_width = 4;
        int depth1_expanded = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            BS_TEST_ASSERT(now.turn == now.state.depth);
            if (now.turn == 1) ++depth1_expanded;
            SmallState s = now.state;
            s.depth = now.turn + 1;
            s.x = 123;
            emit.push(std::move(s), 5, std::uint64_t{123}, false);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(!res.exhausted);
        BS_TEST_ASSERT(res.best_cost == 5);
        BS_TEST_ASSERT(res.best_turn == 1);
        BS_TEST_ASSERT(depth1_expanded == 0);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 1);
    }
    {
        BS::Param param;
        param.max_turn = 1;
        param.max_turn_is_answer = false;
        param.beam_width = 4;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState a = now.state; a.depth = now.turn + 1; a.x = 10;
            SmallState b = now.state; b.depth = now.turn + 1; b.x = 20;
            emit.push(std::move(a), 1, std::uint64_t{10}, false);
            emit.push(std::move(b), 2, std::uint64_t{20}, true);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.best_cost == 2);
        BS_TEST_ASSERT(res.best_turn == 1);
        BS_TEST_ASSERT(res.best_state->x == 20);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_turn_is_answer = false;
        param.beam_width = 4;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            SmallState s = now.state;
            s.depth = now.turn + 1;
            s.x = now.state.x * 10 + 1;
            emit.push(std::move(s), now.cost + 1, std::uint64_t{static_cast<std::uint64_t>(now.turn + 1)}, false);
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(!res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.runtime.materialized_turn == 2);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 2);
    }
    {
        BS::Param param;
        param.max_turn = 3;
        param.max_turn_is_answer = true;
        param.beam_width = 4;
        auto expand = [](const BS::StateView<SmallState>&, auto&) {
            // 候補を出さないので、max_turnより前にfrontierが尽きる。
        };
        auto res = BS::run(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(!res.found);
        BS_TEST_ASSERT(!res.completed_max_turn);
        BS_TEST_ASSERT(res.exhausted);
        BS_TEST_ASSERT(res.best_turn == 0);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 1);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_turn_is_answer = true;
        param.beam_width = 4;
        int expanded_finished_depth1 = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) {
                SmallState s = now.state;
                s.depth = 1;
                s.x = 1;
                emit.push(std::move(s), 10, std::uint64_t{1}, true);
            } else if (now.turn == 1) {
                ++expanded_finished_depth1;
                SmallState s = now.state;
                s.depth = 2;
                s.x = 2;
                emit.push(std::move(s), 1, std::uint64_t{2}, false);
            } else {
                BS_TEST_ASSERT(false);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(expanded_finished_depth1 == 1);
        BS_TEST_ASSERT(res.best_cost == 1);
        BS_TEST_ASSERT(res.best_turn == 2);
        BS_TEST_ASSERT(res.best_state->x == 2);
    }
    {
        BS::Param param;
        param.max_turn = 4;
        param.beam_width = 4;
        param.time_limit_ms = 5;
        param.time_check_interval = 1;
        int depth1_expanded = 0;
        int expanded_total_after_root = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 1) {
                ++depth1_expanded;
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
            if (now.turn > 0) ++expanded_total_after_root;
            for (int k = 0; k < 4; ++k) {
                SmallState s = now.state;
                s.depth = now.turn + 1;
                s.x = now.state.x * 10 + k;
                emit.push(std::move(s), now.cost + k, static_cast<std::uint64_t>(now.turn * 100 + k + 1), false);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, std::uint64_t{0}, expand);
        BS_TEST_ASSERT(res.time_limit_reached);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_turn == 4);
        // soft time limit expands best remaining node in same turn.
        BS_TEST_ASSERT(depth1_expanded == 2);
        BS_TEST_ASSERT(expanded_total_after_root == 4);
    }
}

static void test_random_cases() {
    std::mt19937 rng(1234567);
    for (int case_id = 0; case_id < 500; ++case_id) {
        int max_turn = 1 + static_cast<int>(rng() % 9);
        int beam_width = 1 + static_cast<int>(rng() % 12);
        int branch = 1 + static_cast<int>(rng() % 14);
        bool use_hash = (rng() & 1U) != 0;
        int seed = static_cast<int>(rng() % 100000);

        BS::Param param;
        param.max_turn = max_turn;
        param.beam_width = beam_width;
        param.use_hash_dedup = use_hash;

        TinyState init{0, seed % 17, seed * 3 + 1};
        auto expand = [=](const BS::StateView<TinyState>& now, auto& emit) {
            if (now.state.depth >= max_turn) return;
            for (int k = 0; k < branch; ++k) {
                TinyState nx;
                nx.depth = now.state.depth + 1;
                nx.v = static_cast<int>((now.state.v * 37LL + now.state.checksum * 11LL + k * 19LL + nx.depth * 7LL + seed) % 1009);
                nx.checksum = static_cast<int>((now.state.checksum * 43LL + nx.v * 5LL + k + nx.depth) % 1000003);
                int delta = static_cast<int>((test_mix(nx.v + nx.checksum * 3ULL + k * 1009ULL + seed) % 41));
                int cost = now.cost + delta;
                bool finished = (nx.depth >= max_turn);
                if (use_hash) emit.push(nx, cost, static_cast<std::uint64_t>(nx.v), finished);
                else emit.push(nx, cost, finished);
            }
        };
        auto res = use_hash
            ? BS::run(param, init, 0, static_cast<std::uint64_t>(init.v), expand)
            : BS::run(param, init, 0, expand);
        auto ref = run_reference(seed, max_turn, beam_width, branch, use_hash);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == ref.first);
        BS_TEST_ASSERT(res.best_state->v == ref.second.v);
        BS_TEST_ASSERT(res.best_state->checksum == ref.second.checksum);
    }
}


static void test_random_max_turn_cases() {
    std::mt19937 rng(7654321);
    for (int case_id = 0; case_id < 300; ++case_id) {
        int max_turn = 1 + static_cast<int>(rng() % 8);
        int beam_width = 1 + static_cast<int>(rng() % 10);
        int branch = 1 + static_cast<int>(rng() % 12);
        bool use_hash = (rng() & 1U) != 0;
        int seed = static_cast<int>(rng() % 100000);

        BS::Param param;
        param.max_turn = max_turn;
        param.max_turn_is_answer = true;
        param.beam_width = beam_width;
        param.use_hash_dedup = use_hash;

        TinyState init{0, seed % 17, seed * 3 + 1};
        auto expand = [=](const BS::StateView<TinyState>& now, auto& emit) {
            BS_TEST_ASSERT(now.turn == now.state.depth);
            BS_TEST_ASSERT(now.turn < max_turn);
            for (int k = 0; k < branch; ++k) {
                TinyState nx;
                nx.depth = now.turn + 1;
                nx.v = static_cast<int>((now.state.v * 37LL + now.state.checksum * 11LL + k * 19LL + nx.depth * 7LL + seed) % 1009);
                nx.checksum = static_cast<int>((now.state.checksum * 43LL + nx.v * 5LL + k + nx.depth) % 1000003);
                int delta = static_cast<int>((test_mix(nx.v + nx.checksum * 3ULL + k * 1009ULL + seed) % 41));
                int cost = now.cost + delta;
                if (use_hash) emit.push(nx, cost, static_cast<std::uint64_t>(nx.v), false);
                else emit.push(nx, cost, false);
            }
        };
        auto res = use_hash
            ? BS::run(param, init, 0, static_cast<std::uint64_t>(init.v), expand)
            : BS::run(param, init, 0, expand);
        auto ref = run_reference(seed, max_turn, beam_width, branch, use_hash);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.completed_max_turn);
        BS_TEST_ASSERT(res.best_turn == max_turn);
        BS_TEST_ASSERT(res.best_cost == ref.first);
        BS_TEST_ASSERT(res.best_state->v == ref.second.v);
        BS_TEST_ASSERT(res.best_state->checksum == ref.second.checksum);
    }
}

struct BenchStateTrivial {
    int depth = 0;
    int v = 0;
    std::uint64_t h = 0;
    std::array<int, 16> data{};
};

struct BenchStateVector {
    std::vector<int> data;
    int depth = 0;
    int v = 0;
};

static long long median_us(std::vector<long long> xs) {
    std::sort(xs.begin(), xs.end());
    return xs[xs.size() / 2];
}

static long long benchmark_trivial_state_once(double time_limit_ms) {
    constexpr int max_turn = 80;
    BS::Param param;
    param.max_turn = max_turn;
    param.beam_width = 256;
    param.time_limit_ms = time_limit_ms;

    auto run_once = [&]() {
        BenchStateTrivial init;
        auto expand = [](const BS::StateView<BenchStateTrivial>& now, auto& emit) {
            if (now.state.depth >= max_turn) return;
            for (int k = 0; k < 20; ++k) {
                BenchStateTrivial nx = now.state;
                ++nx.depth;
                nx.v = static_cast<int>((nx.v * 33LL + k * 17LL + nx.depth * 5LL) & 0x7fffffff);
                nx.h = test_mix(now.hash + static_cast<std::uint64_t>(k + 1) * 1000003ULL + static_cast<std::uint64_t>(nx.depth));
                nx.data[k & 15] += nx.v + nx.depth;
                int cost = now.cost + static_cast<int>(nx.h % 97);
                bool finished = (nx.depth >= max_turn);
                emit.push(std::move(nx), cost, nx.h, finished);
            }
        };
        auto beg = test_now_microseconds();
        auto res = BS::run(param, init, 0, std::uint64_t{0}, expand);
        auto end = test_now_microseconds();
        BS_TEST_ASSERT(res.found);
        return end - beg;
    };

    std::vector<long long> times;
    for (int i = 0; i < 7; ++i) times.push_back(run_once());
    return median_us(times);
}

static long long benchmark_vector_state_lazy_once(double time_limit_ms) {
    constexpr int max_turn = 45;
    BS::Param param;
    param.max_turn = max_turn;
    param.beam_width = 128;
    param.time_limit_ms = time_limit_ms;

    auto run_once = [&]() {
        BenchStateVector init;
        init.data.assign(64, 0);
        auto expand = [](const BS::StateView<BenchStateVector>& now, auto& emit) {
            if (now.state.depth >= max_turn) return;
            for (int k = 0; k < 18; ++k) {
                int next_depth = now.state.depth + 1;
                int nv = static_cast<int>((now.state.v * 31LL + k * 13LL + next_depth) % 1000003);
                std::uint64_t h = test_mix(static_cast<std::uint64_t>(nv) + static_cast<std::uint64_t>(next_depth) * 1009ULL);
                int cost = now.cost + static_cast<int>(h % 89);
                bool finished = (next_depth >= max_turn);
                emit.push_lazy(cost, h, finished, [&]() {
                    BenchStateVector nx = now.state;
                    nx.depth = next_depth;
                    nx.v = nv;
                    nx.data[static_cast<std::size_t>(k) % nx.data.size()] += nv;
                    return nx;
                });
            }
        };
        auto beg = test_now_microseconds();
        auto res = BS::run(param, init, 0, std::uint64_t{0}, expand);
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
    test_max_turn_cases();
    test_random_cases();
    test_random_max_turn_cases();
    benchmark_trivial_state();
    benchmark_vector_state_lazy();
    std::cout << "all tests passed" << std::endl;
    return 0;
}

#endif // __INCLUDE_LEVEL__ == 0
