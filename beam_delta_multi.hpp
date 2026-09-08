#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
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

// Emitter::push の引数誤用を減らすため、hash/step/boolを概念で分ける。
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

template<class U, class Action>
concept ActionArg = std::same_as<std::remove_cvref_t<U>, Action>;

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
// Cost と Hash を固定したターン制ビームサーチ本体。
// using BS = bs::Beam<std::int64_t, std::uint64_t>; のように別名化して使う。
template<CostType Cost = long long, HashType Hash = std::uint64_t>
struct Beam {
    using cost_type = Cost;
    using hash_type = Hash;

private:
    template<class Action>
    class Engine;

public:
// ノード容量の自動設定方針。
// Safe は最大で beam_width * (max_turn + 1) 程度を確保する安全側の既定値。
// Compact は長いTで省メモリにしたい場合の明示指定用で、max_step 寄りに確保する。
enum class CapacityPolicy { Safe, Compact };

// 探索パラメータ。
// max_step=0 は 1 として扱う。複数ターン行動を使う問題では必ず明示すること。
// time_limit_ms は soft limit。expand実行中は中断しない。
// 制限到達を検出したturnでは、通常展開を止め、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
// Hash重複排除は既定で有効。hash_capacity=0 の場合は内部で自動設定される。
// 解候補は「finished=true の候補」または「 max_turn 到達候補」。
struct Param {
    int max_turn = 0;               // 探索終了ターン。候補は 0..max_turn に到着できる。
    int max_step = 1;               // 1手で進める最大ターン。0以下なら1扱い。
    int beam_width = 1;             // 各到着ターンに残す候補数。

    int nodes_capacity = 0;         // 探索木ノード容量。0なら capacity_policy に従って自動設定。
    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。

    double time_limit_ms = 0.0;    // 0なら無制限。正なら制限到達検出後に最良1本の縮退探索へ移るsoft limit。
    int time_check_interval = 64;   // 何ノード展開ごとに時間を見るか。
    bool max_turn_is_answer = true; // trueならmax_turn到達ノードも解候補にする。
    bool use_hash_dedup = true;     // trueならHash付きpushで同一ターン重複排除を行う。
    bool print_warnings = true;     // 危険状態を検出したら終了時にstderrへ1回だけ警告する。
    CapacityPolicy capacity_policy = CapacityPolicy::Safe;
};

// Hookの発火タイミング。CSVなどは外部Hookで実装する。
enum class EventType {
    RunStart,
    RunEnd,
    TurnStart,
    TurnEnd,
    BestUpdate,
    RootCompress,
};

// expand に渡す現在ノード情報。
struct NodeView {
    int turn = 0;
    Cost cost{};
    Hash hash{};
};

// Hookから観測できる実行時情報。
struct Runtime {
    int turn = 0;
    int max_turn = 0;
    int max_step = 1;
    int beam_width = 0;
    int nodes_capacity = 0;       // 設定されたノード容量。容量調整用。
    int hash_capacity = 0;        // 実際に使うHash表容量。0ならHash dedupなし。

    int active_width = 0;           // このターンで展開した葉数。
    int selected_width = 0;         // 次ターンSelectorから木に追加した候補数。
    int candidate_count = 0;        // 次ターンSelectorに残っていた候補数。

    long long expanded_nodes = 0;
    long long generated_candidates = 0;
    long long selected_nodes = 0;

    long long pruned_by_width = 0;
    long long pruned_by_hash = 0;      // 同一Hashで実際に棄却した候補数。
    long long duplicate_by_hash = 0;   // 同一Hashを検出した候補数。置換も含む。
    long long invalid_candidates = 0;
    long long overflow_nodes = 0;
    long long hash_table_full = 0;
    long long reused_nodes = 0;            // free listから再利用したノード数。
    long long stale_remove_entries = 0;    // 世代違いで無視した遅延削除entry数。
    long long remove_entry_overflow = 0;   // 遅延削除entry容量不足。通常は0になる設計。

    int live_nodes = 0;
    int used_nodes = 0;
    int pending_candidates = 0;
    int max_live_nodes = 0;       // 探索中のlive_nodes最大値。
    int max_used_nodes = 0;       // 探索中のused_nodes最大値。
    int max_pending_candidates = 0; // 探索中のpending_candidates最大値。
    int root_compressions = 0;

    // CSVデバッグ用の軽量スナップショット。
    // DebugHook有効時、またはRunEndの最終Result用に更新される。
    double time_limit_ms = 0.0;
    double time_remaining_ms = -1.0;
    std::int64_t elapsed_us = 0;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;
    double estimated_finish_ms = 0.0;

    int current_root_turn = 0;
    int committed_path_length = 0;
    int root_compressed_edges = 0;

    int free_node_count = 0;

    int selector_nonempty_count = 0;
    int selector_total_candidates = 0;
    int selector_max_size = 0;
    int selector_full_count = 0;
    double selector_avg_size = 0.0;
    double selector_full_rate = 0.0;

    int live_candidate_count = 0;
    Cost live_best_cost{};
    Cost live_worst_cost{};
    double live_avg_cost = 0.0;
    int live_best_turn = 0;

    Cost selected_best_cost{};
    Cost selected_worst_cost{};
    double selected_avg_cost = 0.0;
    double selected_step_avg = 0.0;
    int selected_step_max = 0;
    int selected_parent_unique = 0;
    int max_children_from_same_parent = 0;

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
// TurnStart/BestUpdate/RootCompress行は出力しない。
// 行はメモリ上に保持し、RunEndで一括してファイルへ書き出す。
struct CsvStatHook {
    struct Row : Runtime {
        Row() = default;
        explicit Row(const Runtime& runtime) : Runtime(runtime) {}

        const char* event = "";
        double elapsed_ms = 0.0;

        double node_pool_used_rate = 0.0;

        double period_ms = 0.0;
        int period_turns = 0;
        long long period_expanded_nodes = 0;
        long long period_generated_candidates = 0;
        long long period_selected_nodes = 0;
        long long period_pruned_by_width = 0;
        long long period_pruned_by_hash = 0;
        long long period_duplicate_by_hash = 0;
        int period_best_updates = 0;

        double expand_per_sec = 0.0;
        double generate_per_sec = 0.0;
        double select_per_sec = 0.0;
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
            case EventType::RootCompress:
                // CsvStatHookではTurnStart/BestUpdate/RootCompress行は出さない。
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
            case EventType::RootCompress: return "RootCompress";
        }
        return "Unknown";
    }

    void push_row(EventType event_type, const Runtime& runtime) {
        Row row(runtime);
        row.event = event_name(event_type);
        row.elapsed_ms = elapsed_ms(runtime.elapsed_us);
        row.node_pool_used_rate = runtime.nodes_capacity > 0
            ? static_cast<double>(runtime.used_nodes) / runtime.nodes_capacity
            : 0.0;

        if (!rows.empty()) {
            const Row& prev = rows.back();
            row.period_ms = row.elapsed_ms - prev.elapsed_ms;
            row.period_turns = row.turn - prev.turn;
            row.period_expanded_nodes = row.expanded_nodes - prev.expanded_nodes;
            row.period_generated_candidates = row.generated_candidates - prev.generated_candidates;
            row.period_selected_nodes = row.selected_nodes - prev.selected_nodes;
            row.period_pruned_by_width = row.pruned_by_width - prev.pruned_by_width;
            row.period_pruned_by_hash = row.pruned_by_hash - prev.pruned_by_hash;
            row.period_duplicate_by_hash = row.duplicate_by_hash - prev.duplicate_by_hash;
            row.period_best_updates = row.best_update_count - prev.best_update_count;
            if (row.period_ms > 0.0) {
                row.expand_per_sec = static_cast<double>(row.period_expanded_nodes) * 1000.0 / row.period_ms;
                row.generate_per_sec = static_cast<double>(row.period_generated_candidates) * 1000.0 / row.period_ms;
                row.select_per_sec = static_cast<double>(row.period_selected_nodes) * 1000.0 / row.period_ms;
            }
            const long long period_pruned = row.period_pruned_by_width + row.period_pruned_by_hash;
            const long long period_seen = row.period_generated_candidates;
            if (period_seen > 0) {
                row.width_prune_rate = static_cast<double>(row.period_pruned_by_width) / static_cast<double>(period_seen);
                row.hash_prune_rate = static_cast<double>(row.period_pruned_by_hash) / static_cast<double>(period_seen);
            } else if (period_pruned > 0) {
                row.width_prune_rate = row.hash_prune_rate = 0.0;
            }
        }

        rows.push_back(row);
    }

    void write_csv() const {
        std::ostringstream ofs;
        ofs << "event,elapsed_ms,turn,max_turn,max_step,beam_width,nodes_capacity,hash_capacity,";
        ofs << "active_width,selected_width,candidate_count,";
        ofs << "expanded_nodes,generated_candidates,selected_nodes,pruned_by_width,pruned_by_hash,duplicate_by_hash,invalid_candidates,";
        ofs << "overflow_nodes,hash_table_full,reused_nodes,stale_remove_entries,remove_entry_overflow,";
        ofs << "live_nodes,used_nodes,pending_candidates,max_live_nodes,max_used_nodes,max_pending_candidates,root_compressions,";
        ofs << "time_limit_ms,time_remaining_ms,estimated_finish_ms,";
        ofs << "current_root_turn,committed_path_length,root_compressed_edges,";
        ofs << "free_node_count,node_pool_used_rate,";
        ofs << "selector_nonempty_count,selector_total_candidates,selector_max_size,selector_full_count,selector_avg_size,selector_full_rate,";
        ofs << "live_candidate_count,live_best_cost,live_worst_cost,live_avg_cost,live_best_turn,";
        ofs << "selected_best_cost,selected_worst_cost,selected_avg_cost,selected_step_avg,selected_step_max,selected_parent_unique,max_children_from_same_parent,";
        ofs << "width_pruned_cost_count,width_pruned_best_cost,pruned_best_gap,";
        ofs << "turns_since_best_update,ms_since_best_update,";
        ofs << "found,best_cost,best_turn,best_update_count,time_limit_reached,";
        ofs << "period_ms,period_turns,period_expanded_nodes,period_generated_candidates,period_selected_nodes,";
        ofs << "period_pruned_by_width,period_pruned_by_hash,period_duplicate_by_hash,period_best_updates,";
        ofs << "expand_per_sec,generate_per_sec,select_per_sec,width_prune_rate,hash_prune_rate\n";
        ofs.setf(std::ios::fixed);
        ofs.precision(6);
        for (const Row& row : rows) {
            ofs << row.event << ','
                << row.elapsed_ms << ','
                << row.turn << ','
                << row.max_turn << ','
                << row.max_step << ','
                << row.beam_width << ','
                << row.nodes_capacity << ','
                << row.hash_capacity << ','
                << row.active_width << ','
                << row.selected_width << ','
                << row.candidate_count << ','
                << row.expanded_nodes << ','
                << row.generated_candidates << ','
                << row.selected_nodes << ','
                << row.pruned_by_width << ','
                << row.pruned_by_hash << ','
                << row.duplicate_by_hash << ','
                << row.invalid_candidates << ','
                << row.overflow_nodes << ','
                << row.hash_table_full << ','
                << row.reused_nodes << ','
                << row.stale_remove_entries << ','
                << row.remove_entry_overflow << ','
                << row.live_nodes << ','
                << row.used_nodes << ','
                << row.pending_candidates << ','
                << row.max_live_nodes << ','
                << row.max_used_nodes << ','
                << row.max_pending_candidates << ','
                << row.root_compressions << ','
                << row.time_limit_ms << ','
                << row.time_remaining_ms << ','
                << row.estimated_finish_ms << ','
                << row.current_root_turn << ','
                << row.committed_path_length << ','
                << row.root_compressed_edges << ','
                << row.free_node_count << ','
                << row.node_pool_used_rate << ','
                << row.selector_nonempty_count << ','
                << row.selector_total_candidates << ','
                << row.selector_max_size << ','
                << row.selector_full_count << ','
                << row.selector_avg_size << ','
                << row.selector_full_rate << ','
                << row.live_candidate_count << ','
                << row.live_best_cost << ','
                << row.live_worst_cost << ','
                << row.live_avg_cost << ','
                << row.live_best_turn << ','
                << row.selected_best_cost << ','
                << row.selected_worst_cost << ','
                << row.selected_avg_cost << ','
                << row.selected_step_avg << ','
                << row.selected_step_max << ','
                << row.selected_parent_unique << ','
                << row.max_children_from_same_parent << ','
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
                << row.period_selected_nodes << ','
                << row.period_pruned_by_width << ','
                << row.period_pruned_by_hash << ','
                << row.period_duplicate_by_hash << ','
                << row.period_best_updates << ','
                << row.expand_per_sec << ','
                << row.generate_per_sec << ','
                << row.select_per_sec << ','
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

// 木の1辺。結果復元のため、開始ターンと消費ターンをActionと別に保持する。
template<class Action>
struct Edge {
    Action action{};
    int turn = 0;
    int step = 1;
    [[nodiscard]] int next_turn() const noexcept { return turn + step; }
};

// 探索結果。
// node_pool_exhausted==true の場合、best_cost/path は容量内で得られた部分探索結果であり、
// 完全なbeam search結果として扱わないこと。
template<class Action>
struct Result {
    bool found = false;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;
    // max_turn到着候補を少なくとも1件materializeした。
    // 全候補を通常幅で展開した保証ではない。
    bool completed_max_turn = false;
    bool node_pool_exhausted = false;
    bool hash_table_full = false;

    Cost best_cost{};
    int best_turn = 0;

    std::vector<Edge<Action>> path;
    Runtime runtime;
};

// expand に渡す候補追加器。
// 公開型なので、expand関数の引数型として明示できる。
// push の戻り値 true は、この呼び出し時点で候補がSelectorに入ったことを表す。
// 後続候補に置換される可能性があるため、最終結果に残る保証ではない。
// finished=true は解候補フラグであり、自動的な展開停止ではない。
template<class Action>
class Emitter {
public:
    // 引数順は action, cost, [hash,] step, [finished] とする。step は int 固定。
    template<class U, StepArg S>
    requires ActionArg<U, Action>
    bool push(U&& action, Cost cost, S step) {
        return push_impl(std::forward<U>(action), cost, now_hash_, step, false, false);
    }

    template<class U, StepArg S, BoolArg B>
    requires ActionArg<U, Action>
    bool push(U&& action, Cost cost, S step, B finished) {
        return push_impl(std::forward<U>(action), cost, now_hash_, step, false, finished);
    }

    template<class U, class H, StepArg S>
    requires ActionArg<U, Action> && CompatibleHashArg<H, Hash>
    bool push(U&& action, Cost cost, H hash, S step) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), step, true, false);
    }

    template<class U, class H, StepArg S, BoolArg B>
    requires ActionArg<U, Action> && CompatibleHashArg<H, Hash>
    bool push(U&& action, Cost cost, H hash, S step, B finished) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), step, true, finished);
    }

private:
    friend class Engine<Action>;

    Emitter(Engine<Action>& engine, int turn, int parent, Hash now_hash) noexcept
        : engine_(engine), turn_(turn), parent_(parent), now_hash_(now_hash) {}

    template<class U>
    requires ActionArg<U, Action>
    bool push_impl(U&& action, Cost cost, Hash hash, int step, bool has_hash, bool finished);

    Engine<Action>& engine_;
    int turn_ = 0;
    int parent_ = -1;
    Hash now_hash_{};
};


private:

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

    template<class Fn, class Action>
    static void call_move(Fn& fn, const Edge<Action>& edge) {
        if constexpr (std::is_invocable_v<Fn&, const Edge<Action>&>) {
            fn(edge);
        } else if constexpr (std::is_invocable_v<Fn&, Edge<Action>>) {
            fn(Edge<Action>(edge));
        } else if constexpr (std::is_invocable_v<Fn&, const Action&>) {
            fn(edge.action);
        } else if constexpr (std::is_invocable_v<Fn&, Action>) {
            fn(Action(edge.action));
        } else {
            static_assert(
                std::is_invocable_v<Fn&, const Edge<Action>&> ||
                std::is_invocable_v<Fn&, Edge<Action>> ||
                std::is_invocable_v<Fn&, const Action&> ||
                std::is_invocable_v<Fn&, Action>,
                "move callback must accept const Edge<Action>&, Edge<Action>, const Action&, or Action"
            );
        }
    }

    template<class Expand, class Action>
    static void call_expand(Expand& expand, const NodeView& now, Runtime& rt, Emitter<Action>& emit) {
        // expand は Runtime なし、または const Runtime& ありの2系統を許可する。
        // Runtime はライブラリ管理情報なので、ユーザー側から変更できないよう const 参照だけを渡す。
        if constexpr (std::is_invocable_v<Expand&, const NodeView&, const Runtime&, Emitter<Action>&>) {
            expand(now, static_cast<const Runtime&>(rt), emit);
        } else if constexpr (std::is_invocable_v<Expand&, const NodeView&, Emitter<Action>&>) {
            expand(now, emit);
        } else {
            static_assert(
                std::is_invocable_v<Expand&, const NodeView&, const Runtime&, Emitter<Action>&> ||
                std::is_invocable_v<Expand&, const NodeView&, Emitter<Action>&>,
                "expand must accept (const NodeView&, const Runtime&, Emitter&) or (const NodeView&, Emitter&). Runtime must be const."
            );
        }
    }

template<class Action>
class Engine {
    friend class Emitter<Action>;

    // 内部候補表現。ユーザーは Emitter::push で候補を追加する。
    struct Candidate {
        Edge<Action> edge{};
        Cost cost{};
        Hash hash{};
        int parent = -1;
        bool has_hash = false;
        bool finished = false;
    };

    static_assert(std::default_initializable<Action>,
                  "Action must be default initializable because the root node stores a dummy action");
    static_assert(std::copy_constructible<Action>,
                  "Action must be copy constructible because candidates and paths store actions");
    static_assert(std::move_constructible<Action>,
                  "Action must be move constructible");
    static_assert(std::assignable_from<Action&, Action>,
                  "Action must be assignable because selectors replace candidates in-place");

    // 1回の run で使うユーザーcallback群。
    // Engine自体はAction型だけに依存させ、関数ポインタなしのEmitterを維持する。
    template<class Expand, class MoveForward, class MoveBackward, class DebugHook>
    struct RunContext {
        Expand& expand;
        MoveForward& move_forward;
        MoveBackward& move_backward;
        DebugHook& debug_hook;
        static constexpr bool has_debug_hook = !std::same_as<std::decay_t<DebugHook>, NoOp>;
    };

    struct Node {
        Edge<Action> edge{};
        Cost cost{};
        Hash hash{};
        int turn = 0;
        int parent = -1;
        int child = -1;
        int left = -1;
        int right = -1;
        int future_children = 0;
        int generation = 0;
        bool alive = false;
        bool active = true;
        bool in_remove_queue = false;
        bool finished = false;
    };

    // 遅延削除entryはノードIDとは別プールで持つ。
    // ノードIDを即再利用しても、entry側のgeneration照合で古い予約を安全に無視できる。
    struct RemoveEntry {
        int node = -1;
        int generation = 0;
        int next = -1;
    };

    // 各到着ターンのSelector。push時点でbeam_width個だけを保持する。
    class Selector {
    public:
        struct PushResult {
            enum Flag : std::uint8_t {
                Accepted     = 1u << 0,
                Duplicate    = 1u << 1,
                HashRejected = 1u << 2,
                HashFull     = 1u << 3,
            };

            std::uint8_t flags = 0;

            constexpr PushResult() = default;
            constexpr explicit PushResult(unsigned f) : flags(static_cast<std::uint8_t>(f)) {}

            [[nodiscard]] constexpr bool has(Flag flag) const noexcept {
                return (flags & static_cast<std::uint8_t>(flag)) != 0;
            }
        };

        void init(int beam_width, int hash_capacity, bool use_hash, std::vector<Node>* nodes) {
            beam_width_ = std::max(1, beam_width);
            use_hash_ = use_hash;
            nodes_ = nodes;
            candidates_.reserve(beam_width_);
            if (use_hash_) hash_.init(std::max(hash_capacity, beam_width_ * 2 + 1));
            seg_n_ = 1;
            while (seg_n_ < beam_width_) seg_n_ <<= 1;
            seg_.assign(seg_n_ * 2, empty_worst());
            full_ = false;
        }

        void clear() {
            candidates_.clear();
            if (use_hash_) hash_.clear();
            full_ = false;
        }

        [[nodiscard]] bool empty() const { return candidates_.empty(); }
        [[nodiscard]] int size() const { return static_cast<int>(candidates_.size()); }
        [[nodiscard]] bool full() const { return size() >= beam_width_; }
        [[nodiscard]] bool rejects_by_width(Cost cost) const { return full_ && !cost_better_than_worst(cost); }
        std::vector<Candidate>& selected() { return candidates_; }
        const std::vector<Candidate>& selected() const { return candidates_; }

        PushResult push(Candidate cand) {
            return (use_hash_ && cand.has_hash) ? push_hashed(std::move(cand)) : push_plain(std::move(cand));
        }

    private:
        int beam_width_ = 1;
        int seg_n_ = 1;
        bool full_ = false;
        bool use_hash_ = false;
        std::vector<Node>* nodes_ = nullptr;
        std::vector<Candidate> candidates_;
        FixedHashMap hash_;
        std::vector<std::pair<Cost, int>> seg_;

        static std::pair<Cost, int> empty_worst() {
            return {std::numeric_limits<Cost>::lowest(), -1};
        }

        static std::pair<Cost, int> worse_pair(std::pair<Cost, int> a, std::pair<Cost, int> b) {
            if (a.first != b.first) return (a.first > b.first) ? a : b;
            return (a.second > b.second) ? a : b;
        }

        bool cost_better_than_worst(Cost cost) const {
            const auto worst = seg_[1];
            if (worst.second < 0) return true;
            return cost < worst.first;
        }

        void append_candidate(Candidate&& cand) {
            change_future_child(cand.parent, +1);
            candidates_.push_back(std::move(cand));
            if (static_cast<int>(candidates_.size()) == beam_width_) build_segment_tree();
        }

        void replace_candidate(int j, Candidate&& cand) {
            change_future_child(candidates_[j].parent, -1);
            change_future_child(cand.parent, +1);
            const Cost cost = cand.cost;
            candidates_[j] = std::move(cand);
            if (full_) seg_set(j, {cost, j});
        }

        PushResult push_plain(Candidate&& cand) {
            if (!full_) {
                append_candidate(std::move(cand));
                return PushResult(PushResult::Accepted);
            }
            const int j = seg_[1].second;
            if (j < 0) return {};
            replace_candidate(j, std::move(cand));
            return PushResult(PushResult::Accepted);
        }

        PushResult push_hashed(Candidate&& cand) {
            auto found = hash_.lookup(cand.hash);
            if (found.found) {
                const int j = hash_.get(found.slot);
                if (is_live_hash_entry(j, cand.hash)) {
                    if (!(cand.cost < candidates_[j].cost)) {
                        return PushResult(PushResult::Duplicate | PushResult::HashRejected);
                    }
                    replace_candidate(j, std::move(cand));
                    return PushResult(PushResult::Accepted | PushResult::Duplicate);
                }
                // stale entryなので、このslotを再利用できる。
            }
            if (found.slot < 0 && !relookup_after_rebuild(cand.hash, found)) return PushResult(PushResult::HashFull);

            if (full_) {
                const int j = seg_[1].second;
                if (j < 0) return {};
                const Hash h = cand.hash;
                replace_candidate(j, std::move(cand));
                hash_.set(found.slot, h, j);
                return PushResult(PushResult::Accepted);
            }
            const int j = static_cast<int>(candidates_.size());
            const Hash h = cand.hash;
            append_candidate(std::move(cand));
            hash_.set(found.slot, h, j);
            return PushResult(PushResult::Accepted);
        }

        bool is_live_hash_entry(int j, Hash hash) const {
            return 0 <= j && j < static_cast<int>(candidates_.size()) &&
                   candidates_[j].has_hash && candidates_[j].hash == hash;
        }

        bool relookup_after_rebuild(Hash hash, FixedHashMap::Lookup& found) {
            rebuild_hash();
            found = hash_.lookup(hash);
            return found.slot >= 0;
        }

        void change_future_child(int node_id, int delta) {
            if (node_id < 0) return;
            (*nodes_)[node_id].future_children += delta;
            assert((*nodes_)[node_id].future_children >= 0);
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

        void build_segment_tree() {
            full_ = true;
            std::fill(seg_.begin(), seg_.end(), empty_worst());
            for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = {candidates_[i].cost, i};
            for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_pair(seg_[i << 1], seg_[i << 1 | 1]);
        }

        void seg_set(int pos, std::pair<Cost, int> value) {
            int i = seg_n_ + pos;
            seg_[i] = value;
            while (i >>= 1) seg_[i] = worse_pair(seg_[i << 1], seg_[i << 1 | 1]);
        }
    };

public:
    Engine(Param param_, Cost initial_cost_, Hash initial_hash_)
        : param(param_), initial_cost(initial_cost_), initial_hash(initial_hash_) {
        normalize_param();
    }

    template<class Expand, class MoveForward, class MoveBackward, class DebugHook>
    Result<Action> run(Expand&& expand, MoveForward&& move_forward,
                       MoveBackward&& move_backward, DebugHook&& debug_hook) {
        using Ctx = RunContext<std::remove_reference_t<Expand>, std::remove_reference_t<MoveForward>,
                               std::remove_reference_t<MoveBackward>, std::remove_reference_t<DebugHook>>;
        Ctx ctx{expand, move_forward, move_backward, debug_hook};
        rt = Runtime{};
        debug_enabled = Ctx::has_debug_hook;
        start_clock_us = detail::now_microseconds();
        limit_us = detail::time_limit_to_microseconds(param.time_limit_ms);
        use_time_limit = (limit_us > 0);

        allocate_buffers();
        initialize_root();

        rt.turn = 0;
        rt.max_turn = param.max_turn;
        rt.max_step = param.max_step;
        rt.beam_width = param.beam_width;
        rt.nodes_capacity = param.nodes_capacity;
        rt.hash_capacity = param.hash_capacity;
        rt.time_limit_ms = param.time_limit_ms;
        rt.best_cost = initial_cost;
        rt.best_turn = 0;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        emit_hook(EventType::RunStart, ctx);
        if (is_answer(root)) set_best(root, ctx);

        bool minimal_mode = rt.time_limit_reached;
        completed_max_turn = (param.max_turn == 0);

        for (int current_turn = 0; current_turn < param.max_turn; ++current_turn) {
            rt.turn = current_turn;
            rt.active_width = 0;
            rt.selected_width = 0;
            rt.candidate_count = 0;
            reset_turn_debug_stats();
            if (mark_time_limit_if_elapsed()) minimal_mode = true;
            emit_hook(EventType::TurnStart, ctx);

            // 通常時はDFSで全幅を展開する。時間切れ後は各ターンの最良葉だけを展開する。
            if (!minimal_mode) run_dfs(current_turn, minimal_mode, ctx);
            if (minimal_mode) run_minimal_dfs(current_turn, ctx);

            // 次ターンに到着した候補を木に接続する。時間切れ後は最良1候補だけを残す。
            materialize_arrival(current_turn + 1, minimal_mode, ctx);

            refresh_runtime_usage();
            if (mark_time_limit_if_elapsed()) minimal_mode = true;
            emit_hook(EventType::TurnEnd, ctx);

            if (completed_max_turn) break;
            if (node_pool_exhausted) break;
            if (rt.time_limit_reached) minimal_mode = true;

            // 未来候補もactive葉もない場合は、max_turnまで空回ししない。
            if (rt.pending_candidates == 0 && !nodes[root].active) break;
        }

        // 探索木の不要ノードを明示解放し、探索中に進めたroot prefix分を巻き戻す。
        cleanup_after_run();
        for (int i = static_cast<int>(committed_edges.size()) - 1; i >= 0; --i) {
            call_move(ctx.move_backward, committed_edges[i]);
        }
        finalize_runtime_for_result();
        emit_hook(EventType::RunEnd, ctx);
        Result<Action> res = build_result();
        print_result_warnings(res);
        return res;
    }

private:
    Param param;
    Cost initial_cost{};
    Hash initial_hash{};

    Runtime rt;
    std::int64_t start_clock_us = 0;
    std::int64_t limit_us = 0;
    bool use_time_limit = false;
    bool debug_enabled = false;

    std::vector<Node> nodes;
    std::vector<int> free_nodes;
    std::vector<Selector> selectors;
    std::vector<int> remove_heads;
    std::vector<RemoveEntry> remove_entries;
    std::vector<int> remove_free;
    std::vector<Edge<Action>> committed_edges;
    std::vector<Edge<Action>> best_path;
    std::vector<Edge<Action>> tmp_path;
    std::vector<int> tmp_nodes;
    std::vector<int> tmp_ids;

    int ring_size = 2;
    int root = 0;
    int used_nodes = 0;
    int live_nodes = 0;
    int next_generation = 1;
    bool node_pool_exhausted = false;
    bool completed_max_turn = false;
    int current_expand_max_step = 1;
    std::int64_t last_best_update_us = 0;

    void normalize_param() {
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
        param.max_step = std::min(std::max(1, param.max_step), std::max(1, param.max_turn));
        param.time_check_interval = std::max(1, param.time_check_interval);

        if (param.nodes_capacity <= 0) {
            const long long cap = (param.capacity_policy == CapacityPolicy::Safe)
                ? 1LL + 1LL * param.beam_width * (param.max_turn + 1)
                : 1LL * param.beam_width * (param.max_step + 3) + param.max_turn + 1024;
            param.nodes_capacity = static_cast<int>(std::min<long long>(std::max<long long>(cap, 2), std::numeric_limits<int>::max() / 4));
        }
        if (param.use_hash_dedup) {
            // Hash重複排除を使う場合、hash_capacity=0 は自動設定とする。
            // Selectorが保持する候補は高々beam_width個なので、過大指定はキャッシュ効率のため丸める。
            if (param.hash_capacity <= 0) param.hash_capacity = param.beam_width * 4 + 17;
            const int lower = param.beam_width * 2 + 1;
            const int upper = param.beam_width * 4 + 17;
            param.hash_capacity = std::min(std::max(param.hash_capacity, lower), upper);
        } else {
            param.hash_capacity = 0;
        }
    }

    void allocate_buffers() {
        nodes.clear();
        nodes.reserve(param.nodes_capacity);
        free_nodes.clear();
        free_nodes.reserve(param.nodes_capacity);
        selectors.clear();
        selectors.resize(param.max_step + 1);
        ring_size = param.max_step + 1;
        const bool use_hash = param.use_hash_dedup;
        const int hash_cap = use_hash ? param.hash_capacity : 0;
        for (Selector& s : selectors) s.init(param.beam_width, hash_cap, use_hash, &nodes);

        remove_heads.assign(ring_size, -1);
        const long long remove_cap_ll = 1LL * param.beam_width * (param.max_step + 2) + 8;
        const int remove_cap = static_cast<int>(std::min<long long>(std::max<long long>(remove_cap_ll, 8), std::numeric_limits<int>::max() / 4));
        remove_entries.assign(remove_cap, RemoveEntry{});
        remove_free.clear();
        remove_free.reserve(remove_cap);
        for (int i = remove_cap - 1; i >= 0; --i) remove_free.push_back(i);
        committed_edges.clear();
        best_path.clear();
        tmp_path.clear();
        tmp_nodes.clear();
        tmp_ids.clear();
        committed_edges.reserve(param.max_turn + 1);
        best_path.reserve(param.max_turn + 1);
        tmp_path.reserve(param.max_turn + 1);
        tmp_nodes.reserve(param.max_turn + 1);
        tmp_ids.reserve(param.beam_width);
    }

    void initialize_root() {
        root = 0;
        used_nodes = 0;
        live_nodes = 0;
        next_generation = 1;
        node_pool_exhausted = false;
        completed_max_turn = false;
        rt.pending_candidates = 0;
        rt.live_nodes = rt.used_nodes = 0;
        rt.max_live_nodes = rt.max_used_nodes = rt.max_pending_candidates = 0;

        Node root_node;
        root_node.cost = initial_cost;
        root_node.hash = initial_hash;
        root_node.turn = 0;
        root_node.parent = -1;
        root_node.active = true;
        root_node.finished = false;
        root = new_node(root_node);
        assert(root == 0);
        refresh_runtime_usage();
    }

    void refresh_runtime_usage() {
        rt.live_nodes = live_nodes;
        rt.used_nodes = used_nodes;
        if (rt.pending_candidates < 0) rt.pending_candidates = 0;
        rt.max_live_nodes = std::max(rt.max_live_nodes, live_nodes);
        rt.max_used_nodes = std::max(rt.max_used_nodes, used_nodes);
        rt.max_pending_candidates = std::max(rt.max_pending_candidates, rt.pending_candidates);
        rt.free_node_count = static_cast<int>(free_nodes.size());
    }

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
        rt.time_remaining_ms = use_time_limit
            ? std::max(0.0, static_cast<double>(limit_us - rt.elapsed_us) * 0.001)
            : -1.0;
    }

    void reset_turn_debug_stats() {
        if (!debug_enabled) return;
        rt.selected_best_cost = Cost{};
        rt.selected_worst_cost = Cost{};
        rt.selected_avg_cost = 0.0;
        rt.selected_step_avg = 0.0;
        rt.selected_step_max = 0;
        rt.selected_parent_unique = 0;
        rt.max_children_from_same_parent = 0;
        rt.width_pruned_cost_count = 0;
        rt.width_pruned_best_cost = Cost{};
        rt.pruned_best_gap = 0.0;
    }

    void update_width_pruned_debug(Cost cost) {
        if (!debug_enabled) return;
        if (rt.width_pruned_cost_count == 0 || cost < rt.width_pruned_best_cost) {
            rt.width_pruned_best_cost = cost;
        }
        ++rt.width_pruned_cost_count;
    }

    template<class Cands>
    void collect_selected_debug_stats(const Cands& cands, bool minimal, int best_idx) {
        if (!debug_enabled) return;
        tmp_ids.clear();
        bool has = false;
        double cost_sum = 0.0;
        double step_sum = 0.0;
        int count = 0;
        for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
            if (minimal && i != best_idx) continue;
            const Candidate& cand = cands[i];
            if (!has) {
                rt.selected_best_cost = rt.selected_worst_cost = cand.cost;
                has = true;
            } else {
                if (cand.cost < rt.selected_best_cost) rt.selected_best_cost = cand.cost;
                if (rt.selected_worst_cost < cand.cost) rt.selected_worst_cost = cand.cost;
            }
            cost_sum += static_cast<double>(cand.cost);
            step_sum += cand.edge.step;
            rt.selected_step_max = std::max(rt.selected_step_max, cand.edge.step);
            tmp_ids.push_back(cand.parent);
            ++count;
        }
        if (count > 0) {
            rt.selected_avg_cost = cost_sum / count;
            rt.selected_step_avg = step_sum / count;
            std::sort(tmp_ids.begin(), tmp_ids.end());
            int unique_count = 0;
            int max_same = 0;
            for (int i = 0; i < static_cast<int>(tmp_ids.size());) {
                int j = i + 1;
                while (j < static_cast<int>(tmp_ids.size()) && tmp_ids[j] == tmp_ids[i]) ++j;
                ++unique_count;
                max_same = std::max(max_same, j - i);
                i = j;
            }
            rt.selected_parent_unique = unique_count;
            rt.max_children_from_same_parent = max_same;
        }
    }

    void update_debug_snapshot(bool force = false) {
        if (!force && !debug_enabled) return;

        rt.time_limit_ms = param.time_limit_ms;
        rt.time_remaining_ms = use_time_limit ? std::max(0.0, static_cast<double>(limit_us - rt.elapsed_us) * 0.001) : -1.0;
        rt.estimated_finish_ms = (rt.turn > 0) ? (static_cast<double>(rt.elapsed_us) * 0.001) * static_cast<double>(param.max_turn) / static_cast<double>(rt.turn) : 0.0;

        rt.current_root_turn = nodes[root].turn;
        rt.committed_path_length = static_cast<int>(committed_edges.size());
        rt.free_node_count = static_cast<int>(free_nodes.size());

        int selector_nonempty = 0;
        int selector_total = 0;
        int selector_max = 0;
        int selector_full = 0;

        bool has_frontier = false;
        double live_sum = 0.0;
        int live_best_turn = 0;

        auto add_live_candidate = [&](Cost cost, int turn) {
            if (!has_frontier) {
                rt.live_best_cost = rt.live_worst_cost = cost;
                live_best_turn = turn;
                has_frontier = true;
            } else {
                if (cost < rt.live_best_cost) {
                    rt.live_best_cost = cost;
                    live_best_turn = turn;
                }
                if (rt.live_worst_cost < cost) rt.live_worst_cost = cost;
            }
            live_sum += static_cast<double>(cost);
            ++rt.live_candidate_count;
        };

        rt.live_candidate_count = 0;
        for (const Selector& selector : selectors) {
            const int sz = selector.size();
            if (sz == 0) continue;
            ++selector_nonempty;
            selector_total += sz;
            selector_max = std::max(selector_max, sz);
            if (selector.full()) ++selector_full;
            for (const Candidate& cand : selector.selected()) {
                add_live_candidate(cand.cost, cand.edge.next_turn());
            }
        }
        for (int i = 0; i < used_nodes; ++i) {
            if (!nodes[i].alive || !nodes[i].active || nodes[i].child != -1) continue;
            add_live_candidate(nodes[i].cost, nodes[i].turn);
        }

        rt.selector_nonempty_count = selector_nonempty;
        rt.selector_total_candidates = selector_total;
        rt.selector_max_size = selector_max;
        rt.selector_full_count = selector_full;
        rt.selector_avg_size = selector_nonempty > 0 ? static_cast<double>(selector_total) / selector_nonempty : 0.0;
        rt.selector_full_rate = selector_nonempty > 0 ? static_cast<double>(selector_full) / selector_nonempty : 0.0;

        if (rt.live_candidate_count > 0) {
            rt.live_avg_cost = live_sum / rt.live_candidate_count;
            rt.live_best_turn = live_best_turn;
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
            rt.turns_since_best_update = std::max(0, rt.turn - rt.best_turn);
            rt.ms_since_best_update = static_cast<double>(rt.elapsed_us - last_best_update_us) * 0.001;
        } else {
            rt.turns_since_best_update = 0;
            rt.ms_since_best_update = 0.0;
        }
    }

    void finalize_runtime_for_result() {
        refresh_runtime_usage();
        update_time();
        update_debug_snapshot(true);
    }

    template<class Ctx>
    void emit_hook(EventType event_type, Ctx& ctx) {
        if constexpr (Ctx::has_debug_hook) {
            // RunEndは直前のfinalize_runtime_for_resultで最終snapshotを作っている。
            // ここで再計算せず、Result::runtimeと同じ値をHookへ渡す。
            if (event_type == EventType::RunEnd) {
                ctx.debug_hook(event_type, static_cast<const Runtime&>(rt));
                return;
            }

            // Hookに渡すRuntimeは観測専用とする。
            // BestUpdateはset_best()で時刻更新済みの瞬間イベントなので、重いsnapshotも追加の時刻更新も行わない。
            if (event_type != EventType::BestUpdate) {
                update_time();
                update_debug_snapshot();
            }
            ctx.debug_hook(event_type, static_cast<const Runtime&>(rt));
        } else {
            (void)event_type;
            (void)ctx;
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

    int new_node(const Node& node) {
        int id;
        if (!free_nodes.empty()) {
            id = free_nodes.back();
            free_nodes.pop_back();
            ++rt.reused_nodes;
            nodes[id] = node;
        } else {
            if (static_cast<int>(nodes.size()) >= param.nodes_capacity) {
                node_pool_exhausted = true;
                rt.overflow_nodes++;
                return -1;
            }
            id = static_cast<int>(nodes.size());
            nodes.push_back(node);
            used_nodes = static_cast<int>(nodes.size());
        }
        nodes[id].alive = true;
        nodes[id].generation = next_generation++;
        if (next_generation == std::numeric_limits<int>::max()) next_generation = 1;
        ++live_nodes;
        refresh_runtime_usage();
        return id;
    }

    void delete_node(int id) {
        if (id < 0 || id == root || !nodes[id].alive) return;
        nodes[id] = Node{};
        free_nodes.push_back(id);
        --live_nodes;
        refresh_runtime_usage();
    }

    template<class U>
    requires ActionArg<U, Action>
    bool push_candidate(int turn, int parent, U&& action, int step, Cost cost, Hash hash, bool has_hash, bool finished) {
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
        Candidate cand{Edge<Action>{std::forward<U>(action), turn, step}, cost, hash, parent, has_hash, finished};
        const int before = selector.size();
        const auto res = selector.push(std::move(cand));
        if (res.has(Selector::PushResult::Duplicate)) ++rt.duplicate_by_hash;
        if (res.has(Selector::PushResult::HashFull)) ++rt.hash_table_full;
        if (!res.has(Selector::PushResult::Accepted)) {
            if (res.has(Selector::PushResult::Duplicate) ||
                res.has(Selector::PushResult::HashRejected) ||
                res.has(Selector::PushResult::HashFull)) {
                ++rt.pruned_by_hash;
            } else {
                ++rt.pruned_by_width;
                update_width_pruned_debug(cost);
            }
            return false;
        }
        if (step > current_expand_max_step) current_expand_max_step = step;
        if (selector.size() > before) {
            ++rt.pending_candidates;
            rt.max_pending_candidates = std::max(rt.max_pending_candidates, rt.pending_candidates);
        }
        return true;
    }

    void remove_leaf(int start_v) {
        int v = start_v;
        while (true) {
            if (v < 0 || v >= used_nodes || !nodes[v].alive) return;
            if (v == root) return;
            if (nodes[v].future_children > 0 || nodes[v].in_remove_queue || nodes[v].child != -1) return;
            const int left = nodes[v].left;
            const int right = nodes[v].right;
            const int parent = nodes[v].parent;
            if (parent < 0) return;

            delete_node(v);
            if (left == -1) {
                nodes[parent].child = right;
                if (right != -1) nodes[right].left = -1;
                v = parent;
            } else {
                nodes[left].right = right;
                if (right != -1) nodes[right].left = left;
                return;
            }
        }
    }

    void enqueue_remove(int node_id, int turn) {
        if (remove_free.empty()) {
            ++rt.remove_entry_overflow;
            return;
        }
        const int entry_id = remove_free.back();
        remove_free.pop_back();
        const int slot = turn % ring_size;
        remove_entries[entry_id] = RemoveEntry{node_id, nodes[node_id].generation, remove_heads[slot]};
        remove_heads[slot] = entry_id;
        nodes[node_id].in_remove_queue = true;
    }

    void remove_useless_nodes(int turn) {
        int& head = remove_heads[turn % ring_size];
        int entry_id = head;
        head = -1;
        while (entry_id != -1) {
            const RemoveEntry entry = remove_entries[entry_id];
            const int next_entry = entry.next;
            remove_entries[entry_id] = RemoveEntry{};
            remove_free.push_back(entry_id);

            const int v = entry.node;
            if (0 <= v && v < used_nodes && nodes[v].alive && nodes[v].generation == entry.generation && nodes[v].in_remove_queue) {
                nodes[v].in_remove_queue = false;
                if (v != root && nodes[v].child == -1 && nodes[v].future_children == 0) remove_leaf(v);
            } else {
                ++rt.stale_remove_entries;
            }
            entry_id = next_entry;
        }
    }

    // active葉と未来候補のLCAがrootより下に進んだら、共通prefixを確定する。
    // 条件は「root直下の実体化済み子が1本、かつroot直下に未到着候補なし」。
    template<class Ctx>
    void update_root(Ctx& ctx) {
        int child = nodes[root].child;
        while (child != -1 && nodes[child].right == -1 && nodes[root].future_children == 0) {
            const int old_root = root;
            root = child;
            committed_edges.push_back(nodes[root].edge);
            call_move(ctx.move_forward, nodes[root].edge);

            // old_root から root への辺は committed_edges に保存済みなので、木から切り離して解放できる。
            nodes[root].parent = -1;
            nodes[root].left = -1;
            nodes[root].right = -1;
            delete_node(old_root);

            ++rt.root_compressions;
            ++rt.root_compressed_edges;
            emit_hook(EventType::RootCompress, ctx);
            child = nodes[root].child;
        }
    }

    template<class MoveForward>
    int move_to_leaf(int v, MoveForward& move_forward) {
        int child = nodes[v].child;
        while (child != -1) {
            while (child != -1 && !nodes[child].active) child = nodes[child].right;
            if (child == -1) break;
            nodes[v].active = false;
            v = child;
            call_move(move_forward, nodes[v].edge);
            child = nodes[v].child;
        }
        nodes[v].active = false;
        return v;
    }

    template<class MoveForward, class MoveBackward>
    int move_to_ancestor(int v, MoveForward& move_forward, MoveBackward& move_backward) {
        while (v != root) {
            call_move(move_backward, nodes[v].edge);
            int u = nodes[v].right;
            while (u != -1) {
                if (nodes[u].active) {
                    call_move(move_forward, nodes[u].edge);
                    return u;
                }
                u = nodes[u].right;
            }
            v = nodes[v].parent;
        }
        return root;
    }

    template<class MoveBackward>
    void move_direct_to_root(int v, MoveBackward& move_backward) {
        while (v != root) {
            call_move(move_backward, nodes[v].edge);
            v = nodes[v].parent;
        }
    }

    int add_leaf(const Candidate& cand) {
        const int parent = cand.parent;
        Node node;
        node.edge = cand.edge;
        node.cost = cand.cost;
        node.hash = cand.hash;
        node.turn = cand.edge.next_turn();
        node.parent = parent;
        node.child = -1;
        node.left = -1;
        node.right = nodes[parent].child;
        node.future_children = 0;
        node.active = true;
        node.in_remove_queue = false;
        node.finished = cand.finished;

        const int v = new_node(node);
        if (v < 0) {
            release_future_child(cand);
            return -1;
        }
        --nodes[parent].future_children;
        assert(nodes[parent].future_children >= 0);
        if (nodes[parent].child != -1) nodes[nodes[parent].child].left = v;
        nodes[parent].child = v;

        int u = parent;
        while (!nodes[u].active) {
            nodes[u].active = true;
            if (u == root) break;
            u = nodes[u].parent;
        }
        return v;
    }

    bool is_answer(int id) const {
        const Node& node = nodes[id];
        return node.finished || (param.max_turn_is_answer && node.turn == param.max_turn);
    }

    template<class Ctx>
    void expand_leaf(int v, Ctx& ctx) {
        ++rt.active_width;
        NodeView now{nodes[v].turn, nodes[v].cost, nodes[v].hash};
        current_expand_max_step = 1;
        Emitter<Action> emit(*this, now.turn, v, now.hash);
        if constexpr (std::is_invocable_v<decltype(ctx.expand)&, const NodeView&, const Runtime&, Emitter<Action>&>) {
            // Runtime付きexpandには保持量だけを最新化して渡す。
            // elapsed_usはtime limit判定・Hook・RunEndで更新し、expandごとの時刻取得を避ける。
            refresh_runtime_usage();
        }
        call_expand(ctx.expand, now, rt, emit);
        ++rt.expanded_nodes;

        // 削除は、この葉から最も遅く届く候補が到着するまでは遅延する。
        // future_childrenが残っていればremove_leaf側で削除されない。
        const int remove_turn = std::min(param.max_turn, nodes[v].turn + current_expand_max_step);
        enqueue_remove(v, remove_turn);
    }

    template<class Ctx>
    void run_dfs(int current_turn, bool& switch_to_minimal, Ctx& ctx) {
        remove_useless_nodes(current_turn);
        update_root(ctx);
        int v = root;
        if (!nodes[v].active) return;

        while (!switch_to_minimal) {
            v = move_to_leaf(v, ctx.move_forward);
            if (nodes[v].turn != current_turn) {
                // 本来は現在ターンの葉だけがactiveになる。安全のためroot状態へ戻して終了する。
                move_direct_to_root(v, ctx.move_backward);
                break;
            }
            expand_leaf(v, ctx);
            if (need_switch_to_minimal_by_time()) {
                // move_to_leaf() で展開経路の active flag を落としている。
                // ここで通常DFSを打ち切る場合も、残り兄弟を縮退探索で見つけられるよう親方向のactiveを復元する。
                deactivate_leaf_path(v);
                move_direct_to_root(v, ctx.move_backward);
                switch_to_minimal = true;
                break;
            }

            v = move_to_ancestor(v, ctx.move_forward, ctx.move_backward);
            if (v == root) break;
        }
    }

    void release_future_child(const Candidate& cand) {
        if (cand.parent >= 0 && cand.parent < used_nodes && nodes[cand.parent].alive) {
            --nodes[cand.parent].future_children;
            assert(nodes[cand.parent].future_children >= 0);
        }
    }

    static int best_candidate_index(const std::vector<Candidate>& cands) {
        int best = 0;
        for (int i = 1; i < static_cast<int>(cands.size()); ++i) {
            const Candidate& cand = cands[i];
            const Candidate& current = cands[best];
            if (cand.cost < current.cost) best = i;
        }
        return best;
    }

    template<class Ctx>
    void materialize_arrival(int arrival_turn, bool minimal, Ctx& ctx) {
        Selector& selector = selectors[arrival_turn % ring_size];
        rt.candidate_count = selector.size();
        if (selector.empty()) return;

        auto& cands = selector.selected();
        const int best_idx = minimal ? best_candidate_index(cands) : -1;
        collect_selected_debug_stats(cands, minimal, best_idx);

        for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
            if (minimal && i != best_idx) {
                release_future_child(cands[i]);
                continue;
            }
            const int id = add_leaf(cands[i]);
            if (id < 0) continue;
            ++rt.selected_width;
            ++rt.selected_nodes;
            if (is_answer(id)) set_best(id, ctx);
            completed_max_turn |= (nodes[id].turn == param.max_turn);
        }
        rt.pending_candidates -= selector.size();
        refresh_runtime_usage();
        selector.clear();
    }

    int best_active_leaf_at_turn(int turn) {
        int best = -1;
        tmp_nodes.clear();
        tmp_nodes.push_back(root);
        while (!tmp_nodes.empty()) {
            const int v = tmp_nodes.back();
            tmp_nodes.pop_back();
            if (v < 0 || v >= used_nodes) continue;
            const Node& node = nodes[v];
            if (!node.alive || !node.active || node.turn > turn) continue;
            if (node.child == -1) {
                if (node.turn == turn && (best < 0 || node.cost < nodes[best].cost)) {
                    best = v;
                }
                continue;
            }
            for (int c = node.child; c != -1; c = nodes[c].right) tmp_nodes.push_back(c);
        }
        return best;
    }

    void deactivate_leaf_path(int leaf) {
        nodes[leaf].active = false;
        int v = leaf;
        while (v != root) {
            const int parent = nodes[v].parent;
            bool any_active = false;
            for (int c = nodes[parent].child; c != -1; c = nodes[c].right) {
                if (nodes[c].active) { any_active = true; break; }
            }
            nodes[parent].active = any_active;
            v = parent;
        }
    }

    void discard_other_active_leaves_at_turn(int turn, int keep) {
        // 先に対象leafを集めてから破棄する。探索中に木を変更すると兄弟走査が壊れるため。
        tmp_nodes.clear();
        tmp_ids.clear();
        tmp_nodes.push_back(root);
        while (!tmp_nodes.empty()) {
            const int v = tmp_nodes.back();
            tmp_nodes.pop_back();
            if (v < 0 || v >= used_nodes) continue;
            const Node& node = nodes[v];
            if (!node.alive || !node.active || node.turn > turn) continue;
            if (node.child == -1) {
                if (node.turn == turn && v != keep) tmp_ids.push_back(v);
                continue;
            }
            for (int c = node.child; c != -1; c = nodes[c].right) tmp_nodes.push_back(c);
        }

        for (int leaf : tmp_ids) {
            if (leaf < 0 || leaf >= used_nodes || !nodes[leaf].alive || !nodes[leaf].active) continue;
            deactivate_leaf_path(leaf);
            if (nodes[leaf].child == -1 && nodes[leaf].future_children == 0 && !nodes[leaf].in_remove_queue) remove_leaf(leaf);
        }
    }

    template<class Ctx>
    void run_minimal_dfs(int current_turn, Ctx& ctx) {
        remove_useless_nodes(current_turn);
        update_root(ctx);
        int v = best_active_leaf_at_turn(current_turn);
        if (v < 0) return;

        // 時間切れ後は1本だけを補完する。選ばなかった同ターンのactive leafは捨て、
        // root圧縮が進みやすい一本道に寄せる。
        discard_other_active_leaves_at_turn(current_turn, v);
        update_root(ctx);
        if (!nodes[v].alive) v = best_active_leaf_at_turn(current_turn);
        if (v < 0) return;

        tmp_nodes.clear();
        for (int u = v; u != root; u = nodes[u].parent) tmp_nodes.push_back(u);
        for (int i = static_cast<int>(tmp_nodes.size()) - 1; i >= 0; --i) {
            call_move(ctx.move_forward, nodes[tmp_nodes[i]].edge);
        }
        expand_leaf(v, ctx);
        deactivate_leaf_path(v);
        move_direct_to_root(v, ctx.move_backward);
    }

    template<class Ctx>
    void set_best(int node_id, Ctx& ctx) {
        const Node& node = nodes[node_id];
        if (!rt.found) {
            rt.found = true;
        } else if (!(node.cost < rt.best_cost)) {
            return;
        }
        rt.best_cost = node.cost;
        rt.best_turn = node.turn;
        ++rt.best_update_count;
        update_time();
        last_best_update_us = rt.elapsed_us;

        best_path.clear();
        best_path.insert(best_path.end(), committed_edges.begin(), committed_edges.end());

        tmp_path.clear();
        for (int v = node_id; v != -1 && nodes[v].parent != -1; v = nodes[v].parent) {
            tmp_path.push_back(nodes[v].edge);
        }
        for (int i = static_cast<int>(tmp_path.size()) - 1; i >= 0; --i) {
            best_path.push_back(tmp_path[i]);
        }
        emit_hook(EventType::BestUpdate, ctx);
    }

    void cleanup_after_run() {
        // Selector内に残った未到着候補は、future_childrenだけを戻してから破棄する。
        for (Selector& selector : selectors) {
            for (const Candidate& cand : selector.selected()) release_future_child(cand);
            selector.clear();
        }
        rt.pending_candidates = 0;
        std::fill(remove_heads.begin(), remove_heads.end(), -1);
        remove_free.clear();
        for (int i = static_cast<int>(remove_entries.size()) - 1; i >= 0; --i) {
            remove_entries[i] = RemoveEntry{};
            remove_free.push_back(i);
        }

        // best_path は保持済みなので、探索木は現在rootだけを残して解放できる。
        for (int i = 0; i < used_nodes; ++i) {
            if (i != root && nodes[i].alive) delete_node(i);
        }
        nodes[root].parent = -1;
        nodes[root].child = -1;
        nodes[root].left = -1;
        nodes[root].right = -1;
        nodes[root].future_children = 0;
        nodes[root].in_remove_queue = false;
        nodes[root].active = true;
        refresh_runtime_usage();
    }

    void print_result_warnings(const Result<Action>& res) const {
        if (!param.print_warnings) return;
        if (res.node_pool_exhausted) {
            std::cerr << "[bs] warning: node_pool_exhausted; nodes_capacity="
                 << res.runtime.nodes_capacity
                 << ", max_used_nodes=" << res.runtime.max_used_nodes << '\n';
        }
        if (res.hash_table_full) {
            std::cerr << "[bs] warning: hash_table_full; hash_capacity="
                 << res.runtime.hash_capacity
                 << ", beam_width=" << param.beam_width << '\n';
        }
        if (res.runtime.remove_entry_overflow > 0) {
            std::cerr << "[bs] warning: remove_entry_overflow; count="
                 << res.runtime.remove_entry_overflow << '\n';
        }
    }

    Result<Action> build_result() {
        Result<Action> res;
        res.found = rt.found;
        res.time_limit_reached = rt.time_limit_reached;
        res.completed_max_turn = completed_max_turn;
        res.node_pool_exhausted = (rt.overflow_nodes > 0);
        res.hash_table_full = (rt.hash_table_full > 0);
        res.best_cost = rt.best_cost;
        res.best_turn = rt.best_turn;
        res.path = std::move(best_path);
        res.runtime = rt;
        return res;
    }
};

public:
    // 探索実行API。
    // Action型では外部状態を move_forward / move_backward で管理する。
    // turn型では Result::path は Edge<Action> の列として返る。
    // initial_hash は必須で、NodeView.hash と次候補のhash計算に使う。
    template<
        class Action,
        class InitialHash,
        class Expand,
        class MoveForward,
        class MoveBackward,
        class DebugHook = NoOp>
    requires CompatibleHashArg<InitialHash, Hash>
    static Result<Action> run(
        const Param& param,
        Cost initial_cost,
        InitialHash initial_hash,
        Expand&& expand,
        MoveForward&& move_forward,
        MoveBackward&& move_backward,
        DebugHook&& debug_hook = DebugHook{}) {
        Engine<Action> engine(param, initial_cost, static_cast<Hash>(initial_hash));
        return engine.run(std::forward<Expand>(expand),
                          std::forward<MoveForward>(move_forward),
                          std::forward<MoveBackward>(move_backward),
                          std::forward<DebugHook>(debug_hook));
    }

};

template<CostType Cost, HashType Hash>
template<class Action>
template<class U>
requires ActionArg<U, Action>
bool Beam<Cost, Hash>::Emitter<Action>::push_impl(U&& action, Cost cost, Hash hash, int step, bool has_hash, bool finished) {
    return engine_.push_candidate(turn_, parent_, std::forward<U>(action), step, cost, hash, has_hash, finished);
}


}  // namespace bs

#if __INCLUDE_LEVEL__ == 0
#include <fstream>
#include <memory>
#include <random>
#include <stdexcept>

namespace {

using BS = bs::Beam<int, std::uint64_t>;
using BS32 = bs::Beam<int, std::uint32_t>;
static_assert(std::same_as<decltype(BS::Param{}.time_limit_ms), double>);
static_assert(std::same_as<decltype(BS::Runtime{}.time_limit_ms), double>);

// ============================================================
// 自己テスト用の軽量テスト基盤
// ============================================================

struct TestContext {
    int passed = 0;
    int failed = 0;

    void check(bool cond, const std::string& name, const std::string& detail = "") {
        if (cond) {
            ++passed;
            return;
        }
        ++failed;
        std::cerr << "[FAIL] " << name;
        if (!detail.empty()) std::cerr << " : " << detail;
        std::cerr << '\n';
    }
};

// テスト用Action。デフォルト構築回数は、ノードプールの遅延構築確認に使う。
struct Act {
    static inline int default_constructed = 0;
    int x = 0;      // 外部状態・コスト再生用の差分値。
    int dt = 1;     // テスト用の補助値。実際のstepはEdge側を使う。
    int token = 0;  // LIFO復元確認用の識別値。

    Act() { ++default_constructed; }
    Act(int x_, int dt_ = 1, int token_ = 0) : x(x_), dt(dt_), token(token_) {}
};

static_assert(!std::default_initializable<BS::Emitter<Act>>);
static_assert(bs::HashType<std::uint64_t>);
static_assert(!bs::HashType<int>);
static_assert(bs::CompatibleHashArg<std::uint32_t, std::uint64_t>);
static_assert(!bs::CompatibleHashArg<int, std::uint64_t>);

using EdgeA = BS::Edge<Act>;

struct NoMove {
    template<class Edge>
    void operator()(const Edge&) const noexcept {}
};

// move_forward。状態値・呼び出し回数・LIFO確認用stackを任意に更新する。
struct Fwd {
    int* state = nullptr;
    int* count = nullptr;
    std::vector<int>* stack = nullptr;

    void operator()(const EdgeA& e) const {
        if (state) *state += e.action.x;
        if (count) ++*count;
        if (stack) stack->push_back(e.action.token);
    }
};

// move_backward。forwardの逆順で呼ばれていることを必要に応じて確認する。
struct Bwd {
    int* state = nullptr;
    int* count = nullptr;
    std::vector<int>* stack = nullptr;
    bool* lifo_ok = nullptr;

    void operator()(const EdgeA& e) const {
        if (stack) {
            if (stack->empty() || stack->back() != e.action.token) {
                if (lifo_ok) *lifo_ok = false;
            } else {
                stack->pop_back();
            }
        }
        if (state) *state -= e.action.x;
        if (count) ++*count;
    }
};

// DebugHookの発火順と、RootCompress時点の外部状態を記録する。
struct HookLog {
    std::vector<BS::EventType>* events = nullptr;
    std::vector<int>* states = nullptr;
    int* state = nullptr;

    void operator()(BS::EventType event, const BS::Runtime&) const {
        if (events) events->push_back(event);
        if (event == BS::EventType::RootCompress && states && state) states->push_back(*state);
    }
};

// 時間制限系テスト用の短いビジーウェイト。
void burn_ms(int ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    volatile std::uint64_t sink = 0;
    while (std::chrono::steady_clock::now() < end) {
        sink = sink + 0x9e3779b97f4a7c15ULL;
    }
    (void)sink;
}

enum class Mode {
    None,
    OneFinished,
    CsvPath,
    MaxStepDefault,
    MultiStep,
    HashDedup,
    NoDedup,
    FinishedEarly,
    SinglePath,
    SmallCapacity,
    StrictMaxTurn,
    HashlessInherit,
    HashStats,
    MinimalComplete,
    MinimalCleanup,
    HeavyExpand,
    StaleEntry,
    MaxStepClamp,
    LcaCompress,
};

// 仕様ごとの候補生成を1つのcallbackにまとめる。
// コストはすべて「累積値を渡す」前提で、Action.xは状態復元確認に使う。
struct Expand {
    Mode mode = Mode::None;
    int max_turn = 0;
    int* counter = nullptr;
    bool* hash_inherit_ok = nullptr;

    template<class Emit>
    void operator()(const BS::NodeView& now, const BS::Runtime&, Emit& emit) const {
        switch (mode) {
            case Mode::None:
                return;

            case Mode::OneFinished:
                if (now.turn == 0) emit.push(Act{-5, 1, 101}, -5, std::uint64_t{1}, 1, true);
                return;

            case Mode::CsvPath:
                if (now.turn < max_turn) {
                    const int nt = now.turn + 1;
                    emit.push(Act{1, 1, 1000 + nt}, now.cost + 1, std::uint64_t(nt), 1, nt == max_turn);
                }
                return;

            case Mode::MaxStepDefault:
                if (now.turn == 0) {
                    emit.push(Act{-10, 2, 201}, -10, std::uint64_t{20}, 2, true);
                    emit.push(Act{1, 1, 202}, 1, std::uint64_t{21}, 1, false);
                }
                return;

            case Mode::MultiStep:
                if (now.turn == 0) {
                    emit.push(Act{3, 3, 301}, 3, std::uint64_t{300}, 3, true);
                    emit.push(Act{5, 1, 302}, 5, std::uint64_t{100}, 1, false);
                } else if (now.turn == 1) {
                    emit.push(Act{-3, 2, 303}, 2, std::uint64_t{200}, 2, true);
                }
                return;

            case Mode::HashDedup:
                if (now.turn == 0) {
                    emit.push(Act{5, 1, 401}, 5, std::uint64_t{42}, 1, true);
                    emit.push(Act{1, 1, 402}, 1, std::uint64_t{42}, 1, true);
                    emit.push(Act{3, 1, 403}, 3, std::uint64_t{42}, 1, true);
                }
                return;

            case Mode::NoDedup:
                if (now.turn == 0) {
                    emit.push(Act{5, 1, 501}, 5, std::uint64_t{77}, 1, true);
                    emit.push(Act{3, 1, 502}, 3, std::uint64_t{77}, 1, true);
                    emit.push(Act{7, 1, 503}, 7, std::uint64_t{77}, 1, true);
                }
                return;

            case Mode::FinishedEarly:
                if (now.turn == 0) {
                    emit.push(Act{1, 1, 601}, 1, std::uint64_t{61}, 1, false);
                } else if (now.turn == 1) {
                    emit.push(Act{2, 1, 602}, 3, std::uint64_t{62}, 1, true);
                }
                return;

            case Mode::SinglePath:
                if (now.turn < max_turn) {
                    const int nt = now.turn + 1;
                    emit.push(Act{1, 1, 700 + nt}, now.cost + 1, std::uint64_t(7000 + nt), 1, nt == max_turn);
                }
                return;

            case Mode::SmallCapacity:
                if (now.turn == 0) {
                    for (int i = 0; i < 4; ++i) {
                        emit.push(Act{i + 1, 1, 800 + i}, i + 1, std::uint64_t(8000 + i), 1, true);
                    }
                }
                return;

            case Mode::StrictMaxTurn:
                if (now.turn == 0) {
                    emit.push(Act{1, 1, 901}, 1, std::uint64_t{91}, 1, false);
                    emit.push(Act{5, 1, 902}, 5, std::uint64_t{92}, 1, true);
                }
                return;

            case Mode::HashlessInherit:
                if (now.turn == 0) {
                    // hashなしpushはdedup対象外。ただしNodeView.hashには親hashを引き継ぐ。
                    emit.push(Act{1, 1, 1001}, 1, 1, false);
                } else if (now.turn == 1) {
                    if (hash_inherit_ok && now.hash == 12345ULL) *hash_inherit_ok = true;
                    emit.push(Act{1, 1, 1002}, 2, 1, true);
                }
                return;

            case Mode::HashStats:
                if (now.turn == 0) {
                    emit.push(Act{10, 1, 1101}, 10, std::uint64_t{999}, 1, true);
                    emit.push(Act{3, 1, 1102}, 3, std::uint64_t{999}, 1, true);
                    emit.push(Act{7, 1, 1103}, 7, std::uint64_t{999}, 1, true);
                }
                return;

            case Mode::MinimalComplete:
                if (now.turn == 0) burn_ms(2);
                if (now.turn < max_turn) {
                    const int nt = now.turn + 1;
                    emit.push(Act{1, 1, 1200 + nt}, now.cost + 1, std::uint64_t(12000 + nt), 1, nt == max_turn);
                }
                return;

            case Mode::MinimalCleanup:
                if (now.turn == 0) {
                    burn_ms(2);
                    emit.push(Act{1, 1, 1301}, 1, std::uint64_t{1301}, 1, false);
                    emit.push(Act{2, 1, 1302}, 2, std::uint64_t{1302}, 1, false);
                } else if (now.turn < max_turn) {
                    const int nt = now.turn + 1;
                    emit.push(Act{1, 1, 1300 + nt}, now.cost + 1, std::uint64_t(13000 + nt), 1, nt == max_turn);
                }
                return;

            case Mode::HeavyExpand:
                if (now.turn == 0) {
                    for (int i = 0; i < 3; ++i) {
                        emit.push(Act{i + 1, 1, 1400 + i}, i + 1, std::uint64_t(1400 + i), 1, false);
                    }
                } else if (now.turn == 1) {
                    if (counter) ++*counter;
                    burn_ms(2);
                    emit.push(Act{1, 1, 1410 + (counter ? *counter : 0)}, now.cost + 1, std::uint64_t(14100 + now.cost), 1, false);
                } else if (now.turn < max_turn) {
                    const int nt = now.turn + 1;
                    emit.push(Act{1, 1, 14000 + nt}, now.cost + 1, std::uint64_t(14000 + nt), 1, nt == max_turn);
                }
                return;

            case Mode::StaleEntry:
                if (now.turn < max_turn) {
                    // 直近1手は低コストにして一本道を維持する。
                    emit.push(Act{1, 1, 15000 + now.turn * 10 + 1}, now.cost + 1,
                              std::uint64_t(150000 + now.turn * 10 + 1), 1, now.turn + 1 == max_turn);

                    // 遠い到着ターンには高コスト候補を広く置く。
                    // 後続ターンのより安い候補で置換されるため、古いremove entryが再現される。
                    const int remain = max_turn - now.turn;
                    const int lim = std::min(6, remain);
                    for (int step = 2; step <= lim; ++step) {
                        const int arrival = now.turn + step;
                        const int c = 100000 - now.turn * 100 + step;
                        emit.push(Act{step, step, 15000 + now.turn * 10 + step}, c,
                                  std::uint64_t(150000 + arrival), step, arrival == max_turn);
                    }
                }
                return;

            case Mode::MaxStepClamp:
                if (now.turn == 0) emit.push(Act{3, 3, 1601}, 3, std::uint64_t{1601}, 3, true);
                return;

            case Mode::LcaCompress:
                if (now.turn == 0) {
                    // rootの未来候補をいったん置き、turn1側の候補で置換してからLCA圧縮させる。
                    emit.push(Act{10, 1, 1701}, 0, std::uint64_t{1701}, 1, false);
                    emit.push(Act{99, 3, 1702}, 100, std::uint64_t{1703}, 3, false);
                } else if (now.turn == 1) {
                    emit.push(Act{1, 2, 1703}, 1, std::uint64_t{1703}, 2, false); // rootのstep3候補を置換する。
                    emit.push(Act{1, 3, 1704}, 2, std::uint64_t{1704}, 3, false);
                    emit.push(Act{1, 1, 1705}, 3, std::uint64_t{1702}, 1, false);
                } else if (now.turn < max_turn) {
                    const int nt = now.turn + 1;
                    emit.push(Act{1, 1, 1700 + nt}, now.cost + 1, std::uint64_t(17000 + nt), 1, nt == max_turn);
                }
                return;
        }
    }
};

BS::Param param_base(int max_turn, int beam_width = 1, int max_step = 1) {
    BS::Param p;
    p.max_turn = max_turn;
    p.beam_width = beam_width;
    p.max_step = max_step;
    p.print_warnings = false;
    return p;
}

std::string read_text_file(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

int csv_column_count(const std::string& line) {
    if (line.empty()) return 0;
    return static_cast<int>(std::count(line.begin(), line.end(), ',')) + 1;
}

std::string csv_line_at(const std::string& text, int line_index) {
    size_t start = 0;
    for (int i = 0; i < line_index; ++i) {
        start = text.find('\n', start);
        if (start == std::string::npos) return {};
        ++start;
    }
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// ============================================================
// ランダムストレス用モデル
// ============================================================

struct RandomCtx {
    int max_turn = 0;
    int max_step = 1;
    std::uint64_t seed = 0;
    int turn = 0;
    int sum = 0;
    std::uint64_t state_hash = 0;
    std::vector<int> stack;
    bool ok = true;
};

struct RandomFwd {
    RandomCtx* ctx = nullptr;
    void operator()(const EdgeA& e) const {
        ctx->turn += e.step;
        ctx->sum += e.action.x;
        ctx->state_hash ^= splitmix64(static_cast<std::uint64_t>(e.action.token));
        ctx->stack.push_back(e.action.token);
    }
};

struct RandomBwd {
    RandomCtx* ctx = nullptr;
    void operator()(const EdgeA& e) const {
        if (ctx->stack.empty() || ctx->stack.back() != e.action.token) {
            ctx->ok = false;
        } else {
            ctx->stack.pop_back();
        }
        ctx->state_hash ^= splitmix64(static_cast<std::uint64_t>(e.action.token));
        ctx->sum -= e.action.x;
        ctx->turn -= e.step;
    }
};

struct RandomExpand {
    RandomCtx* ctx = nullptr;

    template<class Emit>
    void operator()(const BS::NodeView& now, const BS::Runtime&, Emit& emit) const {
        if (now.turn != ctx->turn || now.cost != ctx->sum || now.hash != ctx->state_hash) {
            ctx->ok = false;
        }
        if (now.turn >= ctx->max_turn) return;

        const int remain = ctx->max_turn - now.turn;
        const int lim = std::min(ctx->max_step, remain);
        std::uint64_t z = splitmix64(ctx->seed ^ (std::uint64_t(now.turn) << 32) ^ now.hash ^ std::uint64_t(now.cost + 1000003));
        const int n = 1 + static_cast<int>(z % 4);

        // 必ず1手候補を出し、どのケースでもmax_turnへ到達可能にする。
        for (int i = 0; i < n; ++i) {
            z = splitmix64(z + std::uint64_t(i) * 0x9e3779b97f4a7c15ULL);
            const int step = (i == 0) ? 1 : 1 + static_cast<int>(z % lim);
            const int delta = -5 + static_cast<int>((z >> 8) % 13);
            const int token = static_cast<int>((ctx->seed & 0x7fffffffULL) ^ (std::uint64_t(now.turn + 1) * 1009ULL) ^ (std::uint64_t(i + 1) * 9176ULL) ^ (z & 0xffff));
            const std::uint64_t next_hash = now.hash ^ splitmix64(static_cast<std::uint64_t>(token));
            const int next_turn = now.turn + step;
            const bool finished = (next_turn == ctx->max_turn) || ((z >> 20) % 17 == 0);
            emit.push(Act{delta, step, token}, now.cost + delta, next_hash, step, finished);
        }
    }
};

void run_random_stress(TestContext& tc) {
    constexpr int CASES = 200;
    long long total_expanded = 0;
    long long total_reused = 0;

    for (int case_id = 0; case_id < CASES; ++case_id) {
        const std::uint64_t seed = splitmix64(0xC0FFEEULL + std::uint64_t(case_id) * 1234567ULL);
        RandomCtx ctx;
        ctx.seed = seed;
        ctx.max_turn = 4 + static_cast<int>(seed % 18);
        ctx.max_step = 1 + static_cast<int>((seed >> 5) % 5);

        BS::Param p = param_base(ctx.max_turn, 1 + static_cast<int>((seed >> 11) % 8), ctx.max_step);
        p.use_hash_dedup = ((seed >> 20) & 1) != 0;

        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, RandomExpand{&ctx}, RandomFwd{&ctx}, RandomBwd{&ctx});
        total_expanded += res.runtime.expanded_nodes;
        total_reused += res.runtime.reused_nodes;

        const std::string prefix = "random stress case " + std::to_string(case_id);
        tc.check(ctx.ok, prefix + " state view and LIFO are consistent");
        tc.check(ctx.turn == 0 && ctx.sum == 0 && ctx.state_hash == 0 && ctx.stack.empty(),
                 prefix + " restores external state");
        tc.check(res.runtime.live_nodes == 1, prefix + " releases live nodes");
        tc.check(res.runtime.pending_candidates == 0, prefix + " releases pending candidates");

        if (res.found) {
            int turn = 0;
            int cost = 0;
            for (const auto& e : res.path) {
                tc.check(e.turn == turn, prefix + " path turn continuity");
                turn += e.step;
                cost += e.action.x;
            }
            tc.check(turn == res.best_turn, prefix + " replayed turn equals best_turn");
            tc.check(cost == res.best_cost, prefix + " replayed cost equals best_cost");
            tc.check(static_cast<int>(res.path.size()) <= res.best_turn, prefix + " path length is bounded by turn");
        }
    }

    tc.check(total_expanded > 0, "random stress tests expanded nodes");
    tc.check(total_reused > 0, "random stress tests reused nodes");
}

// ============================================================
// 仕様一覧に対応する単体テスト
// ============================================================

void run_unit_tests(TestContext& tc) {
    {
        auto p = param_base(1);
        p.time_limit_ms = 0;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::OneFinished, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == -5, "time_limit_ms=0 means unlimited");
    }
    {
        std::vector<BS::EventType> events;
        auto p = param_base(0);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::None, 0}, NoMove{}, NoMove{}, HookLog{&events});
        tc.check(res.found, "hook order root found");
        tc.check(events.size() >= 3 && events[0] == BS::EventType::RunStart &&
                 events[1] == BS::EventType::BestUpdate && events.back() == BS::EventType::RunEnd,
                 "hook order starts with RunStart before BestUpdate");
    }
    {
        const std::string csv = "bs_csv_stat_hook_test.csv";
        std::remove(csv.c_str());
        auto p = param_base(3, 2, 1);
        BS::CsvStatHook hook(csv, 0.0);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::CsvPath, 3}, NoMove{}, NoMove{}, hook);
        const std::string s = read_text_file(csv);
        const std::string header = csv_line_at(s, 0);
        const std::string first_row = csv_line_at(s, 1);
        tc.check(res.found, "CsvStatHook search found");
        tc.check(s.find("expanded_nodes") != std::string::npos &&
                 s.find("generate_per_sec") != std::string::npos &&
                 s.find("max_live_nodes") != std::string::npos,
                 "CsvStatHook writes beam runtime statistics");
        tc.check(header.rfind("event,elapsed_ms,turn,max_turn,max_step,beam_width,nodes_capacity,hash_capacity,", 0) == 0,
                 "CsvStatHook writes leading metadata columns");
        tc.check(csv_column_count(header) == csv_column_count(first_row),
                 "CsvStatHook header and row column counts match",
                 "header=" + std::to_string(csv_column_count(header)) + ", row=" + std::to_string(csv_column_count(first_row)));
        std::remove(csv.c_str());
    }
    {
        auto p = param_base(1, 50000, 1);
        p.nodes_capacity = 1;
        p.time_limit_ms = 1;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::None, 1}, NoMove{}, NoMove{});
        tc.check(res.time_limit_reached && res.runtime.elapsed_us >= 1000,
                 "allocation time is included in time_limit_ms",
                 "elapsed_us=" + std::to_string(res.runtime.elapsed_us));
    }
    {
        Act::default_constructed = 0;
        auto p = param_base(0);
        p.nodes_capacity = 10000;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::None, 0}, NoMove{}, NoMove{});
        tc.check(res.found && Act::default_constructed < 100,
                 "nodes are constructed lazily instead of nodes_capacity times",
                 "default_constructed=" + std::to_string(Act::default_constructed));
    }
    {
        auto p = param_base(0);
        p.max_turn_is_answer = true;
        auto res = BS::run<Act>(p, 7, std::uint64_t{0}, Expand{Mode::None, 0}, NoMove{}, NoMove{});
        tc.check(res.found && res.completed_max_turn && res.best_turn == 0 && res.path.empty(),
                 "max_turn=0 can be accepted as max-turn finished");
    }
    {
        auto p = param_base(0);
        p.max_turn_is_answer = false;
        auto res = BS::run<Act>(p, 7, std::uint64_t{0}, Expand{Mode::None, 0}, NoMove{}, NoMove{});
        tc.check(!res.found && res.completed_max_turn,
                 "max_turn=0 is completed but not accepted in strict finished mode");
    }
    {
        auto p = param_base(1);
        auto ptr = std::make_unique<int>(-1);
        auto expand = [u = std::move(ptr)](const BS::NodeView& now, auto& emit) mutable {
            if (now.turn == 0) emit.push(Act{*u, 1, 1801}, *u, std::uint64_t{1801}, 1, true);
        };
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, std::move(expand), NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == -1, "move-only callable can be passed to run");
    }
    {
        auto p = param_base(2, 2, 0);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::MaxStepDefault, 2}, NoMove{}, NoMove{});
        tc.check(!res.found, "max_step=0 defaults to 1 and rejects step=2 finished candidate");
    }
    {
        auto p = param_base(3, 3, 3);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::MultiStep, 3}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 2 && res.path.size() == 2,
                 "NodeView and multi-step destination selection work");
    }
    {
        int state = 0;
        auto p = param_base(3, 3, 3);
        (void)BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::MultiStep, 3}, Fwd{&state}, Bwd{&state});
        tc.check(state == 0, "state is restored after multi-step search");
    }
    {
        auto p = param_base(1, 4, 1);
        p.hash_capacity = 1;
        p.use_hash_dedup = true;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::HashDedup, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 1, "hash dedup keeps lower cost and small capacity is safe");
    }
    {
        auto p = param_base(1, 4, 1);
        p.hash_capacity = 0;
        p.use_hash_dedup = true;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::HashDedup, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.runtime.duplicate_by_hash >= 1,
                 "hash_capacity=0 auto-enables capacity when dedup is requested");
    }
    {
        auto p = param_base(1, 4, 1);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::HashDedup, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.runtime.duplicate_by_hash >= 1, "hash dedup is enabled by default");
    }
    {
        auto p = param_base(1, 1, 1);
        p.use_hash_dedup = false;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::NoDedup, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 3 && res.runtime.duplicate_by_hash == 0,
                 "use_hash_dedup=false disables dedup but width pruning still works");
    }
    {
        auto p = param_base(3, 1, 1);
        p.max_turn_is_answer = false;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::FinishedEarly, 3}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_turn == 2 && res.best_cost == 3,
                 "finished candidate is accepted before max_turn");
    }
    {
        auto p = param_base(30, 1, 1);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SinglePath, 30}, NoMove{}, NoMove{});
        tc.check(res.found && res.path.size() == 30, "single path search reaches max_turn");
    }
    {
        int state = 0, fw = 0, bw = 0;
        auto p = param_base(30, 1, 1);
        (void)BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SinglePath, 30}, Fwd{&state, &fw}, Bwd{&state, &bw});
        tc.check(state == 0 && fw == bw && fw <= 70,
                 "root compression avoids quadratic prefix movement",
                 "fw=" + std::to_string(fw) + ", bw=" + std::to_string(bw));
    }
    {
        auto p = param_base(30, 1, 1);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SinglePath, 30}, NoMove{}, NoMove{});
        tc.check(res.runtime.live_nodes == 1 && res.runtime.pending_candidates == 0,
                 "single path releases unnecessary nodes");
    }
    {
        int state = 0;
        std::vector<int> states;
        auto p = param_base(5, 4, 3);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::LcaCompress, 5}, Fwd{&state}, Bwd{&state}, HookLog{nullptr, &states, &state});
        tc.check(res.found && std::find(states.begin(), states.end(), 10) != states.end(),
                 "LCA root compression works with multiple future parents");
    }
    {
        int state = 0;
        auto p = param_base(5, 4, 3);
        (void)BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::LcaCompress, 5}, Fwd{&state}, Bwd{&state});
        tc.check(state == 0, "LCA compression restores state");
    }
    {
        auto p = param_base(5, 4, 3);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::LcaCompress, 5}, NoMove{}, NoMove{});
        tc.check(res.runtime.live_nodes == 1 && res.runtime.pending_candidates == 0,
                 "finished search releases unnecessary nodes");
    }
    {
        BS::Param p;
        p.max_turn = 20;
        p.beam_width = 1;
        p.print_warnings = false;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SinglePath, 20}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_turn == 20 &&
                 res.runtime.max_used_nodes <= res.runtime.nodes_capacity && res.runtime.max_live_nodes > 0,
                 "default nodes_capacity reaches max_turn");
    }
    {
        BS::Param p;
        p.max_turn = 20;
        p.beam_width = 1;
        p.print_warnings = false;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SinglePath, 20}, NoMove{}, NoMove{});
        tc.check(res.best_cost == 20 && res.best_turn == 20,
                 "result exposes canonical best fields");
    }
    {
        auto p = param_base(3, 4, 1);
        p.nodes_capacity = 2;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SmallCapacity, 3}, NoMove{}, NoMove{});
        tc.check(res.node_pool_exhausted && res.runtime.overflow_nodes > 0,
                 "small nodes_capacity reports node_pool_exhausted");
    }
    {
        auto p = param_base(1, 4, 1);
        p.max_turn_is_answer = false;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::StrictMaxTurn, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 5,
                 "strict finished ignores non-finished max_turn candidate");
    }
    {
        bool inherit_ok = false;
        auto p = param_base(2, 3, 1);
        p.use_hash_dedup = true;
        auto res = BS::run<Act>(p, 0, std::uint64_t{12345}, Expand{Mode::HashlessInherit, 2, nullptr, &inherit_ok}, NoMove{}, NoMove{});
        tc.check(res.found && inherit_ok, "hashless push inherits parent hash in NodeView");
        tc.check(res.runtime.duplicate_by_hash == 0, "hashless candidates are not mixed into hash dedup");
    }
    {
        struct ExplicitEmitterExpand {
            void operator()(const BS32::NodeView& now,
                            BS32::Emitter<Act>& emit) const {
                if (now.turn == 0) emit.push(Act{1, 1, 1901}, 1, std::uint32_t{7}, 1, false);
                else if (now.turn == 1) emit.push(Act{2, 1, 1902}, 3, std::uint32_t{8}, 1, true);
            }
        };
        BS32::Param p;
        p.max_turn = 2;
        p.beam_width = 3;
        p.max_step = 1;
        p.print_warnings = false;
        auto res = BS32::run<Act>(p, 0, std::uint32_t{0}, ExplicitEmitterExpand{}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 3, "std::uint32_t Hash and explicit public Emitter type work");
    }
    {
        auto p = param_base(1, 4, 1);
        p.use_hash_dedup = true;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::HashStats, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 3, "hash duplicate improvement replaces candidate");
        tc.check(res.runtime.duplicate_by_hash == 2 && res.runtime.pruned_by_hash == 1,
                 "hash statistics separate duplicate detection and actual prune");
    }
    {
        auto p = param_base(4, 4, 1);
        p.time_limit_ms = 1;
        p.time_check_interval = 1;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::MinimalComplete, 4}, NoMove{}, NoMove{});
        tc.check(res.time_limit_reached && res.found && res.completed_max_turn,
                 "minimal completion reaches max_turn after time limit");
    }
    {
        int state = 0;
        auto p = param_base(4, 4, 1);
        p.time_limit_ms = 1;
        p.time_check_interval = 1;
        (void)BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::MinimalComplete, 4}, Fwd{&state}, Bwd{&state});
        tc.check(state == 0, "minimal completion restores state");
    }
    {
        auto p = param_base(8, 2, 1);
        p.time_limit_ms = 1;
        p.time_check_interval = 1000000;
        auto res = BS::run<Act>(p, 0, std::uint64_t{777}, Expand{Mode::MinimalCleanup, 8}, NoMove{}, NoMove{});
        tc.check(res.time_limit_reached && res.found && res.completed_max_turn,
                 "minimal cleanup still completes by best path");
        tc.check(res.runtime.root_compressions > 0,
                 "minimal cleanup enables root compression after discarding other leaves");
    }
    {
        int state = 0, fw = 0, bw = 0;
        auto p = param_base(8, 2, 1);
        p.time_limit_ms = 1;
        p.time_check_interval = 1000000;
        (void)BS::run<Act>(p, 0, std::uint64_t{777}, Expand{Mode::MinimalCleanup, 8}, Fwd{&state, &fw}, Bwd{&state, &bw});
        tc.check(state == 0 && fw == bw, "minimal cleanup restores state");
    }
    {
        int counter = 0;
        auto p = param_base(4, 3, 1);
        p.time_limit_ms = 1;
        p.time_check_interval = 1;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::HeavyExpand, 4, &counter}, NoMove{}, NoMove{});
        tc.check(res.time_limit_reached && counter == 2,
                 "heavy expand switches to one best remaining leaf after cutoff",
                 "counter=" + std::to_string(counter));
    }
    {
        int state = 0;
        auto p = param_base(4, 3, 1);
        p.time_limit_ms = 1;
        p.time_check_interval = 1;
        (void)BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::HeavyExpand, 4}, Fwd{&state}, Bwd{&state});
        tc.check(state == 0, "heavy expand time check restores state");
    }
    {
        int turn1_calls = 0;
        auto p = param_base(3, 4, 2);
        p.time_limit_ms = 5.0;
        p.time_check_interval = 1;
        auto expand = [&](const BS::NodeView& now, BS::Emitter<Act>& emit) {
            if (now.turn == 0) {
                // 先に入れたgood候補より、後に入れたbad候補がDFSで先に展開される。
                emit.push(Act{1, 1, 1801}, 0, std::uint64_t{1801}, 1, false);
                emit.push(Act{2, 1, 1802}, 100, std::uint64_t{1802}, 1, false);
            } else if (now.turn == 1) {
                ++turn1_calls;
                if (now.cost == 100) {
                    burn_ms(8);
                    emit.push(Act{3, 2, 1803}, 10000, std::uint64_t{1803}, 2, false);
                } else {
                    emit.push(Act{4, 1, 1804}, 1, std::uint64_t{1804}, 1, false);
                }
            } else if (now.turn == 2) {
                emit.push(Act{5, 1, 1805}, 2, std::uint64_t{1805}, 1, false);
            }
        };
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, expand, NoMove{}, NoMove{});
        tc.check(res.time_limit_reached && res.found && res.best_cost == 2 && turn1_calls == 2,
                 "time limit cutoff restores active siblings before minimal DFS",
                 "best=" + std::to_string(res.best_cost) + ", calls=" + std::to_string(turn1_calls));
    }
    {
        auto p = param_base(10, 1, 6);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::StaleEntry, 10}, NoMove{}, NoMove{});
        tc.check(res.runtime.stale_remove_entries > 0,
                 "stale remove bucket entry is reproduced and ignored",
                 "stale=" + std::to_string(res.runtime.stale_remove_entries));
        tc.check(res.runtime.reused_nodes > 0,
                 "node IDs are still reused despite stale remove entries");
        tc.check(!res.node_pool_exhausted && res.runtime.live_nodes == 1 && res.runtime.pending_candidates == 0,
                 "stale remove bucket ID does not break reused nodes");
    }
    {
        auto p = param_base(3, 2, 1000000);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::MaxStepClamp, 3}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_turn == 3, "max_step larger than max_turn is clamped safely");
    }
    {
        auto p = param_base(1, 4, 1);
        p.nodes_capacity = 1;
        p.print_warnings = true;
        std::ostringstream oss;
        auto* old = std::cerr.rdbuf(oss.rdbuf());
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SmallCapacity, 1}, NoMove{}, NoMove{});
        std::cerr.rdbuf(old);
        tc.check(res.node_pool_exhausted && oss.str().find("node_pool_exhausted") != std::string::npos,
                 "print_warnings=true prints node_pool_exhausted once");
    }
    {
        auto p = param_base(1, 4, 1);
        p.nodes_capacity = 1;
        p.print_warnings = false;
        std::ostringstream oss;
        auto* old = std::cerr.rdbuf(oss.rdbuf());
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SmallCapacity, 1}, NoMove{}, NoMove{});
        std::cerr.rdbuf(old);
        tc.check(res.node_pool_exhausted && oss.str().empty(),
                 "print_warnings=false suppresses warnings");
    }
}

}  // namespace

int main() {
    TestContext tc;
    run_unit_tests(tc);
    run_random_stress(tc);

    if (tc.failed == 0) {
        std::cout << "beam_delta_multi_v55 self tests passed: " << tc.passed << " checks\n";
        return 0;
    }
    std::cerr << "beam_delta_multi_v55 self tests failed: " << tc.failed << " / " << (tc.passed + tc.failed) << " checks\n";
    return 1;
}
#endif
