#pragma once

#include <algorithm>
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

// Emitter::push の引数誤用を減らすため、hash/bool/actionを概念で分ける。
template<class H, class Hash>
concept CompatibleHashArg =
    HashType<Hash> &&
    HashType<std::remove_cvref_t<H>> &&
    std::convertible_to<std::remove_cvref_t<H>, Hash> &&
    (sizeof(std::remove_cvref_t<H>) <= sizeof(Hash));

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

template<CostType Cost = long long, HashType Hash = std::uint64_t>
struct Beam {
    using cost_type = Cost;
    using hash_type = Hash;

private:
    template<class Action>
    class Engine;

public:
// 探索パラメータ。
// time_limit_ms は soft limit。expand実行中は中断しない。
// 制限到達を検出したturnでは、通常展開を止め、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
struct Param {
    int max_turn = 0;               // 初期ノードをturn 0とした、生成する最大turn。
    int beam_width = 1;             // 各turnに残す候補数。

    int nodes_capacity = 0;         // 探索木ノード容量。0なら自動設定。
    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。

    double time_limit_ms = 0.0;    // 0なら無制限。正なら制限到達検出後に最良1本の縮退探索へ移るsoft limit。
    int time_check_interval = 64;   // 何ノード展開ごとに時間を見るか。
    bool max_turn_is_answer = true; // trueならmax_turn到着ノードも解候補にする。
    bool use_hash_dedup = true;     // trueならHash付きpushで重複排除を行う。
    bool print_warnings = true;     // 危険状態を検出したらstderrへ警告する。
};

// Hookの発火タイミング。ログ収集は外部Hookに任せる。
enum class EventType {
    RunStart,
    RunEnd,
    TurnStart,
    TurnEnd,
    BestUpdate,
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
    int beam_width = 0;
    int nodes_capacity = 0;
    int hash_capacity = 0;

    int active_width = 0;
    int selected_width = 0;
    int candidate_count = 0;

    long long expanded_nodes = 0;
    long long generated_candidates = 0;
    long long selected_nodes = 0;

    // 状態移動回数。現在位置からLCA経由で差分移動する。
    long long move_forward_count = 0;
    long long move_backward_count = 0;

    long long pruned_by_width = 0;
    long long pruned_by_hash = 0;
    long long duplicate_by_hash = 0;
    long long invalid_candidates = 0;
    long long overflow_nodes = 0;
    long long hash_table_full = 0;

    int used_nodes = 0;
    int pending_candidates = 0;
    int max_used_nodes = 0;
    int max_pending_candidates = 0;

    double time_limit_ms = 0.0;
    double time_remaining_ms = -1.0;
    std::int64_t elapsed_us = 0;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;

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
    // CSV 1行分のスナップショット。
    // Runtime本体の値を基底として持ち、CSV固有の差分・派生値だけを追加する。
    struct Row : Runtime {
        Row() = default;
        explicit Row(const Runtime& runtime) : Runtime(runtime) {}

        const char* event = "";
        double elapsed_ms = 0.0;

        // 前回CSV行からの差分。CSV出力時の分析用であり、探索本体では使わない。
        double period_ms = 0.0;
        int period_turns = 0;
        long long period_expanded_nodes = 0;
        long long period_generated_candidates = 0;
        long long period_selected_nodes = 0;
        long long period_move_forward_count = 0;
        long long period_move_backward_count = 0;
        long long period_pruned_by_width = 0;
        long long period_pruned_by_hash = 0;
        long long period_duplicate_by_hash = 0;
        int period_best_updates = 0;

        double expand_per_sec = 0.0;
        double generate_per_sec = 0.0;
        double select_per_sec = 0.0;
        double move_forward_per_sec = 0.0;
        double move_backward_per_sec = 0.0;
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

    void push_row(EventType event_type, const Runtime& runtime) {
        Row row(runtime);
        row.event = event_name(event_type);
        row.elapsed_ms = elapsed_ms(runtime.elapsed_us);

        if (!rows.empty()) {
            const Row& prev = rows.back();
            row.period_ms = row.elapsed_ms - prev.elapsed_ms;
            row.period_turns = row.turn - prev.turn;
            row.period_expanded_nodes = row.expanded_nodes - prev.expanded_nodes;
            row.period_generated_candidates = row.generated_candidates - prev.generated_candidates;
            row.period_selected_nodes = row.selected_nodes - prev.selected_nodes;
            row.period_move_forward_count = row.move_forward_count - prev.move_forward_count;
            row.period_move_backward_count = row.move_backward_count - prev.move_backward_count;
            row.period_pruned_by_width = row.pruned_by_width - prev.pruned_by_width;
            row.period_pruned_by_hash = row.pruned_by_hash - prev.pruned_by_hash;
            row.period_duplicate_by_hash = row.duplicate_by_hash - prev.duplicate_by_hash;
            row.period_best_updates = row.best_update_count - prev.best_update_count;

            if (row.period_ms > 0.0) {
                row.expand_per_sec = static_cast<double>(row.period_expanded_nodes) * 1000.0 / row.period_ms;
                row.generate_per_sec = static_cast<double>(row.period_generated_candidates) * 1000.0 / row.period_ms;
                row.select_per_sec = static_cast<double>(row.period_selected_nodes) * 1000.0 / row.period_ms;
                row.move_forward_per_sec = static_cast<double>(row.period_move_forward_count) * 1000.0 / row.period_ms;
                row.move_backward_per_sec = static_cast<double>(row.period_move_backward_count) * 1000.0 / row.period_ms;
            }
            if (row.period_generated_candidates > 0) {
                row.width_prune_rate = static_cast<double>(row.period_pruned_by_width) / static_cast<double>(row.period_generated_candidates);
                row.hash_prune_rate = static_cast<double>(row.period_pruned_by_hash) / static_cast<double>(row.period_generated_candidates);
            }
        }

        rows.push_back(row);
    }

    void write_csv() const {
        std::ostringstream ofs;
        ofs << "event,elapsed_ms,turn,max_turn,beam_width,nodes_capacity,hash_capacity,";
        ofs << "active_width,selected_width,candidate_count,";
        ofs << "expanded_nodes,generated_candidates,selected_nodes,pruned_by_width,pruned_by_hash,duplicate_by_hash,invalid_candidates,";
        ofs << "overflow_nodes,hash_table_full,";
        ofs << "used_nodes,pending_candidates,max_used_nodes,max_pending_candidates,";
        ofs << "time_limit_ms,time_remaining_ms,";
        ofs << "found,best_cost,best_turn,best_update_count,time_limit_reached,";
        ofs << "period_ms,period_turns,period_expanded_nodes,period_generated_candidates,period_selected_nodes,";
        ofs << "period_pruned_by_width,period_pruned_by_hash,period_duplicate_by_hash,period_best_updates,";
        ofs << "expand_per_sec,generate_per_sec,select_per_sec,width_prune_rate,hash_prune_rate,";
        ofs << "move_forward_count,move_backward_count,period_move_forward_count,period_move_backward_count,";
        ofs << "move_forward_per_sec,move_backward_per_sec\n";
        ofs.setf(std::ios::fixed);
        ofs.precision(6);

        for (const Row& row : rows) {
            ofs << row.event << ','
                << row.elapsed_ms << ','
                << row.turn << ','
                << row.max_turn << ','
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
                << row.used_nodes << ','
                << row.pending_candidates << ','
                << row.max_used_nodes << ','
                << row.max_pending_candidates << ','
                << row.time_limit_ms << ','
                << row.time_remaining_ms << ','
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
                << row.hash_prune_rate << ','
                << row.move_forward_count << ','
                << row.move_backward_count << ','
                << row.period_move_forward_count << ','
                << row.period_move_backward_count << ','
                << row.move_forward_per_sec << ','
                << row.move_backward_per_sec << '\n';
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

    std::vector<Action> path;
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
    // action, cost のみを渡すと、現在ノードの hash を引き継ぐ。
    template<class U>
        requires ActionArg<U, Action>
    bool push(U&& action, Cost cost) {
        return push_impl(std::forward<U>(action), cost, now_hash_, false, false);
    }

    // 第3引数が bool の場合は finished を指定する。
    template<class U, class B>
        requires ActionArg<U, Action> && BoolArg<B>
    bool push(U&& action, Cost cost, B finished) {
        return push_impl(std::forward<U>(action), cost, now_hash_, false, static_cast<bool>(finished));
    }

    // 第3引数が unsigned hash の場合は hash を指定する。
    template<class U, class H>
        requires ActionArg<U, Action> && CompatibleHashArg<H, Hash>
    bool push(U&& action, Cost cost, H hash) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), true, false);
    }

    // hash と finished を同時に指定する。
    template<class U, class H, class B>
        requires ActionArg<U, Action> && CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push(U&& action, Cost cost, H hash, B finished) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), true, static_cast<bool>(finished));
    }

private:
    friend class Engine<Action>;

    Emitter(Engine<Action>& engine, int parent, Hash now_hash)
        : engine_(engine), parent_(parent), now_hash_(now_hash) {}

    template<class U>
    bool push_impl(U&& action, Cost cost, Hash hash, bool has_hash, bool finished);

    Engine<Action>& engine_;
    int parent_ = -1;
    Hash now_hash_{};
};


private:
template<class Fn, class Action>
static void call_move(Fn& fn, const Action& action) {
    if constexpr (std::is_invocable_v<Fn&, const Action&>) {
        fn(action);
    } else if constexpr (std::is_invocable_v<Fn&, Action>) {
        fn(Action(action));
    } else {
        static_assert(
            std::is_invocable_v<Fn&, const Action&> || std::is_invocable_v<Fn&, Action>,
            "move callback must accept const Action& or Action"
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
// ビームサーチ本体
// ============================================================

template<class Action>
class Engine {
    friend class Emitter<Action>;

    static_assert(std::default_initializable<Action>,
                  "Action must be default initializable because the root node stores a dummy action");
    static_assert(std::copy_constructible<Action>,
                  "Action must be copy constructible because candidates and paths store actions");
    static_assert(std::move_constructible<Action>,
                  "Action must be move constructible");
    static_assert(std::assignable_from<Action&, Action>,
                  "Action must be assignable because selectors replace candidates in-place");

    struct Candidate {
        Action action{};
        Cost cost{};
        Hash hash{};
        int parent = -1;
        bool has_hash = false;
        bool finished = false;
    };

    struct Node {
        Action action{};
        Cost cost{};
        Hash hash{};
        int turn = 0;
        int parent = -1;

        // 探索木はparentだけで表す。
        // current_nodeからparentをたどることで、LCA相当まで差分移動できる。
        bool finished = false;
    };

    template<class T>
    static bool cost_better(const T& a, const T& b) {
        return a.cost < b.cost;
    }

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

        void init(int beam_width, int hash_capacity, bool use_hash) {
            beam_width_ = std::max(1, beam_width);
            use_hash_ = use_hash;
            candidates_.clear();
            candidates_.reserve(beam_width_);
            seg_n_ = 1;
            while (seg_n_ < beam_width_) seg_n_ <<= 1;
            seg_.assign(seg_n_ * 2, -1);
            full_ = false;
            if (use_hash_) hash_.init(std::max(hash_capacity, beam_width_ * 2 + 1));
        }

        void clear() {
            candidates_.clear();
            if (use_hash_) hash_.clear();
            full_ = false;
        }

        [[nodiscard]] bool empty() const { return candidates_.empty(); }
        [[nodiscard]] int size() const { return static_cast<int>(candidates_.size()); }
        const std::vector<Candidate>& selected() const { return candidates_; }

        // Candidateを構築・コピーする前に、幅枝刈りで確実に落ちるか判定する。
        // fullでない場合や、現在の最悪候補より良い可能性がある場合はfalseを返す。
        [[nodiscard]] bool rejects_by_width(Cost cost) const {
            if (!full_) return false;
            const int worst = seg_[1];
            if (worst < 0) return false;
            return !(cost < candidates_[worst].cost);
        }

        PushResult push(Candidate cand) {
            return (use_hash_ && cand.has_hash) ? push_hashed(std::move(cand)) : push_plain(std::move(cand));
        }

    private:
        int beam_width_ = 1;
        int seg_n_ = 1;
        bool full_ = false;
        bool use_hash_ = false;
        std::vector<Candidate> candidates_;
        FixedHashMap hash_;
        std::vector<int> seg_;

        int worse_of(int a, int b) const {
            if (a < 0) return b;
            if (b < 0) return a;
            const Candidate& ca = candidates_[a];
            const Candidate& cb = candidates_[b];
            if (ca.cost != cb.cost) return ca.cost > cb.cost ? a : b;
            return a > b ? a : b;
        }

        void append_candidate(Candidate&& cand) {
            candidates_.push_back(std::move(cand));
            if (size() == beam_width_) build_segment_tree();
        }

        void replace_candidate(int j, Candidate&& cand) {
            candidates_[j] = std::move(cand);
            if (full_) seg_set(j, j);
        }

        PushResult push_plain(Candidate&& cand) {
            if (!full_) {
                append_candidate(std::move(cand));
                return PushResult(PushResult::Accepted);
            }
            const int j = seg_[1];
            if (j < 0) return {};
            replace_candidate(j, std::move(cand));
            return PushResult(PushResult::Accepted);
        }

        PushResult push_hashed(Candidate&& cand) {
            auto hit = hash_.lookup(cand.hash);
            if (hit.found) {
                const int j = hash_.get(hit.slot);
                const bool live = 0 <= j && j < size() && candidates_[j].has_hash && candidates_[j].hash == cand.hash;
                if (live) {
                    if (!(cand.cost < candidates_[j].cost)) {
                        return PushResult(PushResult::Duplicate | PushResult::HashRejected);
                    }
                    replace_candidate(j, std::move(cand));
                    return PushResult(PushResult::Accepted | PushResult::Duplicate);
                }
                // stale entryなので、このslotを再利用できる。
            } else if (hit.slot < 0) {
                rebuild_hash();
                hit = hash_.lookup(cand.hash);
                if (hit.slot < 0) return PushResult(PushResult::HashFull);
            }

            if (full_) {
                const int j = seg_[1];
                if (j < 0) return {};
                const Hash h = cand.hash;
                replace_candidate(j, std::move(cand));
                hash_.set(hit.slot, h, j);
                return PushResult(PushResult::Accepted);
            }

            const int j = static_cast<int>(candidates_.size());
            const Hash h = cand.hash;
            append_candidate(std::move(cand));
            hash_.set(hit.slot, h, j);
            return PushResult(PushResult::Accepted);
        }

        void build_segment_tree() {
            full_ = true;
            std::fill(seg_.begin(), seg_.end(), -1);
            for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = i;
            for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_of(seg_[i << 1], seg_[i << 1 | 1]);
        }

        void seg_set(int pos, int value) {
            int i = seg_n_ + pos;
            seg_[i] = value;
            while (i >>= 1) seg_[i] = worse_of(seg_[i << 1], seg_[i << 1 | 1]);
        }

        void rebuild_hash() {
            if (!use_hash_) return;
            hash_.clear();
            for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
                if (!candidates_[i].has_hash) continue;
                const auto slot = hash_.lookup(candidates_[i].hash);
                if (slot.slot >= 0) hash_.set(slot.slot, candidates_[i].hash, i);
            }
        }
    };

public:
    Engine(Param param_, Cost initial_cost_, Hash initial_hash_)
        : param(param_),
          initial_cost(initial_cost_),
          initial_hash(initial_hash_) {
        normalize_param();
    }

    template<class Expand, class MoveForward, class MoveBackward, class DebugHook>
    Result<Action> run(Expand&& expand, MoveForward&& move_forward, MoveBackward&& move_backward, DebugHook&& debug_hook) {
        rt = Runtime{};
        start_clock_us = detail::now_microseconds();
        limit_us = detail::time_limit_to_microseconds(param.time_limit_ms);
        use_time_limit = (limit_us > 0);

        initialize_search();

        rt.max_turn = param.max_turn;
        rt.beam_width = param.beam_width;
        rt.nodes_capacity = param.nodes_capacity;
        rt.hash_capacity = param.hash_capacity;
        rt.best_cost = initial_cost;
        rt.best_turn = 0;
        rt.time_limit_ms = param.time_limit_ms;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        emit_hook(EventType::RunStart, debug_hook);
        if (is_answer(root)) set_best(root, debug_hook);

        bool minimal_mode = rt.time_limit_reached;
        completed_max_turn = (param.max_turn == 0);

        for (int turn = 0; turn < param.max_turn; ++turn) {
            rt.turn = turn;
            rt.active_width = 0;
            rt.selected_width = 0;
            rt.candidate_count = 0;
            selector.clear();
            if (mark_time_limit_if_elapsed()) minimal_mode = true;
            emit_hook(EventType::TurnStart, debug_hook);

            if (use_time_limit) minimal_mode = expand_turn_time_checked(minimal_mode, expand, move_forward, move_backward);
            else expand_turn_full(expand, move_forward, move_backward);
            materialize_arrival(turn + 1, minimal_mode, debug_hook);
            refresh_runtime_usage();
            if (mark_time_limit_if_elapsed()) minimal_mode = true;
            emit_hook(EventType::TurnEnd, debug_hook);

            frontier.swap(next_frontier);
            next_frontier.clear();

            if (completed_max_turn) break;
            if (rt.overflow_nodes > 0) break;
            if (rt.time_limit_reached) minimal_mode = true;
            if (frontier.empty()) break;
        }

        // ライブラリ呼び出し後は、外部状態を初期状態へ戻す。
        move_to_node(root, move_forward, move_backward);
        finalize_runtime_for_result();
        emit_hook(EventType::RunEnd, debug_hook);

        auto res = build_result();
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

    std::vector<Node> nodes;
    std::vector<int> frontier;
    std::vector<int> next_frontier;

    // 現在の外部状態が対応している探索木ノード。
    // 本版ではpath全体を毎回構築せず、current_nodeから親をたどってLCA相当まで移動する。
    int current_node = 0;
    std::vector<int> forward_stack;

    // materialize時に候補を親ごとのbucketへ入れるための一時領域。
    // stable_sortを避け、frontier順にO(width)で次turnを作る。
    std::vector<int> parent_rank;
    std::vector<int> bucket_head;
    std::vector<int> bucket_tail;
    std::vector<int> bucket_next;

    Selector selector;

    int root = 0;
    int best_node_id = -1;
    bool completed_max_turn = false;

    void normalize_param() {
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
        param.time_check_interval = std::max(1, param.time_check_interval);

        if (param.nodes_capacity <= 0) {
            const long long cap = 1LL + 1LL * param.beam_width * param.max_turn;
            param.nodes_capacity = static_cast<int>(std::min<long long>(
                std::max<long long>(cap, 1), std::numeric_limits<int>::max() / 4));
        } else {
            param.nodes_capacity = std::max(1, param.nodes_capacity);
        }

        if (param.use_hash_dedup) {
            if (param.hash_capacity <= 0) param.hash_capacity = param.beam_width * 4 + 17;
            const int lower = param.beam_width * 2 + 1;
            const int upper = param.beam_width * 4 + 17;
            param.hash_capacity = std::min(std::max(param.hash_capacity, lower), upper);
        } else {
            param.hash_capacity = 0;
        }
    }

    void initialize_search() {
        nodes.clear();
        nodes.reserve(param.nodes_capacity);
        frontier.clear();
        next_frontier.clear();
        forward_stack.clear();
        parent_rank.clear();
        bucket_head.clear();
        bucket_tail.clear();
        bucket_next.clear();

        frontier.reserve(param.beam_width);
        next_frontier.reserve(param.beam_width);
        forward_stack.reserve(param.max_turn + 1);
        parent_rank.reserve(param.nodes_capacity);
        bucket_head.reserve(param.beam_width);
        bucket_tail.reserve(param.beam_width);
        bucket_next.reserve(param.beam_width);
        selector.init(param.beam_width, param.hash_capacity, param.use_hash_dedup);

        root = 0;
        best_node_id = -1;
        current_node = root;
        nodes.push_back(Node{Action{}, initial_cost, initial_hash, 0, -1, false});
        frontier.push_back(root);
        refresh_runtime_usage();
    }

    void refresh_runtime_usage() {
        rt.used_nodes = static_cast<int>(nodes.size());
        rt.pending_candidates = selector.size();
        rt.max_used_nodes = std::max(rt.max_used_nodes, rt.used_nodes);
        rt.max_pending_candidates = std::max(rt.max_pending_candidates, rt.pending_candidates);
    }

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
        rt.time_remaining_ms = use_time_limit
            ? std::max(0.0, static_cast<double>(limit_us - rt.elapsed_us) * 0.001)
            : -1.0;
    }

    void finalize_runtime_for_result() {
        refresh_runtime_usage();
        update_time();
    }

    template<class DebugHook>
    void emit_hook(EventType event_type, DebugHook& debug_hook) {
        // NoOp利用時はHook用の時刻・統計更新を省略する。
        // RunEndの最終Runtimeは、呼び出し元でfinalize_runtime_for_result()により整える。
        if constexpr (!std::same_as<std::decay_t<DebugHook>, NoOp>) {
            if (event_type != EventType::RunEnd) {
                refresh_runtime_usage();
                update_time();
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

    template<class U>
    bool push_candidate(int parent, U&& action, Cost cost, Hash hash, bool has_hash, bool finished) {
        ++rt.generated_candidates;
        if (!detail::is_valid_cost(cost)) {
            ++rt.invalid_candidates;
            return false;
        }

        // Selectorが満杯で、現在の最悪候補より明らかに悪い候補は、
        // Actionを内部Candidateへコピーする前に落とす。
        if (selector.rejects_by_width(cost)) {
            ++rt.pruned_by_width;
            return false;
        }

        Candidate cand{std::forward<U>(action), cost, hash, parent, has_hash, finished};
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
            }
            return false;
        }
        if (selector.size() > before) {
            rt.pending_candidates = selector.size();
            rt.max_pending_candidates = std::max(rt.max_pending_candidates, rt.pending_candidates);
        }
        return true;
    }

    bool is_answer(int id) const {
        const Node& node = nodes[id];
        return node.finished || (param.max_turn_is_answer && node.turn == param.max_turn);
    }

    static int best_node_index(const std::vector<int>& ids, const std::vector<Node>& nodes_, int begin = 0) {
        if (begin >= static_cast<int>(ids.size())) return -1;
        int best = ids[begin];
        for (int i = begin + 1; i < static_cast<int>(ids.size()); ++i) {
            const int id = ids[i];
            if (cost_better(nodes_[id], nodes_[best])) best = id;
        }
        return best;
    }

    // 現在の外部状態からtargetノードの状態へ移動する。
    // 本版ではtargetのrootパスを毎回作らず、current_nodeとtargetを親方向へ上げて
    // LCA相当まで戻り、そこからtarget側へ降りる。
    template<class MoveForward, class MoveBackward>
    void move_to_node(int target, MoveForward& move_forward, MoveBackward& move_backward) {
        if (current_node == target) return;

        int a = current_node;
        int b = target;
        forward_stack.clear();

        // turnをそろえる。a側は実際に状態を戻し、b側はあとで降りるノードを積む。
        while (nodes[a].turn > nodes[b].turn) {
            call_move(move_backward, nodes[a].action);
            ++rt.move_backward_count;
            a = nodes[a].parent;
        }
        while (nodes[b].turn > nodes[a].turn) {
            forward_stack.push_back(b);
            b = nodes[b].parent;
        }

        // 共通祖先まで同時に上げる。
        while (a != b) {
            call_move(move_backward, nodes[a].action);
            ++rt.move_backward_count;
            a = nodes[a].parent;

            forward_stack.push_back(b);
            b = nodes[b].parent;
        }

        // target側へ降りる。forward_stackは下から上へ積んでいるため逆順に適用する。
        for (int i = static_cast<int>(forward_stack.size()) - 1; i >= 0; --i) {
            call_move(move_forward, nodes[forward_stack[i]].action);
            ++rt.move_forward_count;
        }

        current_node = target;
    }


    template<class Expand, class MoveForward, class MoveBackward>
    void expand_node(int id, Expand& expand, MoveForward& move_forward, MoveBackward& move_backward) {
        move_to_node(id, move_forward, move_backward);

        const Node& node = nodes[id];
        NodeView now{node.turn, node.cost, node.hash};
        Emitter<Action> emit(*this, id, node.hash);

        ++rt.active_width;

        if constexpr (std::is_invocable_v<Expand&, const NodeView&,
                                           const Runtime&, Emitter<Action>&>) {
            // Runtime付きexpandには保持量だけを最新化して渡す。
            // elapsed_usはtime limit判定・Hook・RunEndで更新し、expandごとの時刻取得を避ける。
            refresh_runtime_usage();
        }
        call_expand(expand, now, rt, emit);
        ++rt.expanded_nodes;
    }

    // time_limit_ms==0用の通常探索ホットパス。縮退探索用の分岐を混ぜない。
    template<class Expand, class MoveForward, class MoveBackward>
    void expand_turn_full(Expand& expand, MoveForward& move_forward, MoveBackward& move_backward) {
        for (int id : frontier) expand_node(id, expand, move_forward, move_backward);
    }

    // 通常モードではfrontierを順に展開する。時間到達後は、未展開部分から最良1本だけ追加で展開する。
    template<class Expand, class MoveForward, class MoveBackward>
    bool expand_turn_time_checked(bool minimal_mode, Expand& expand, MoveForward& move_forward, MoveBackward& move_backward) {
        auto expand_best_from = [&](int begin_index) {
            if (begin_index >= static_cast<int>(frontier.size())) return;
            const int best = best_node_index(frontier, nodes, begin_index);
            expand_node(best, expand, move_forward, move_backward);
        };

        int next_index = 0;
        if (!minimal_mode) {
            for (; next_index < static_cast<int>(frontier.size()); ++next_index) {
                expand_node(frontier[next_index], expand, move_forward, move_backward);
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

    // Selectorに残った候補を探索木へ追加する。
    // 通常時はfrontierの親順bucketで次turnを作り、次turn展開時のLCA移動距離を短くする。
    // 縮退探索時は最良候補1本だけを追加する。
    template<class DebugHook>
    void materialize_arrival(int arrival_turn, bool minimal, DebugHook& debug_hook) {
        rt.candidate_count = selector.size();
        if (selector.empty()) return;

        const auto& cands = selector.selected();

        // 候補をノード化し、必要ならbestを更新する。
        // この処理はmaterialize_arrivalからしか使わないため、外へ分けず近くに置く。
        auto add_selected = [&](const Candidate& cand) {
            if (static_cast<int>(nodes.size()) >= param.nodes_capacity) {
                ++rt.overflow_nodes;
                return;
            }

            const int id = static_cast<int>(nodes.size());
            nodes.push_back(Node{cand.action, cand.cost, cand.hash, arrival_turn, cand.parent, cand.finished});
            next_frontier.push_back(id);
            ++rt.selected_width;
            ++rt.selected_nodes;
            refresh_runtime_usage();

            if (is_answer(id)) set_best(id, debug_hook);
            completed_max_turn |= (arrival_turn == param.max_turn);
        };

        if (minimal) {
            const Candidate* best = &cands[0];
            for (int i = 1; i < static_cast<int>(cands.size()); ++i) {
                if (cost_better(cands[i], *best)) best = &cands[i];
            }
            add_selected(*best);
            selector.clear();
            refresh_runtime_usage();
            return;
        }

        const int parent_count = static_cast<int>(frontier.size());
        const int cand_count = static_cast<int>(cands.size());
        if (parent_count == 0 || cand_count == 0) {
            selector.clear();
            refresh_runtime_usage();
            return;
        }

        if (static_cast<int>(parent_rank.size()) < static_cast<int>(nodes.size())) {
            parent_rank.resize(nodes.size(), -1);
        }
        bucket_head.assign(parent_count, -1);
        bucket_tail.assign(parent_count, -1);
        bucket_next.assign(cand_count, -1);

        // 親ノードIDからfrontier内順位を引く表を作り、候補を親ごとに連結する。
        for (int i = 0; i < parent_count; ++i) parent_rank[frontier[i]] = i;

        bool fallback = false;
        for (int i = 0; i < cand_count; ++i) {
            const int parent = cands[i].parent;
            const int rank = (0 <= parent && parent < static_cast<int>(parent_rank.size())) ? parent_rank[parent] : -1;
            if (rank < 0) {
                fallback = true;
                break;
            }

            if (bucket_head[rank] < 0) {
                bucket_head[rank] = bucket_tail[rank] = i;
            } else {
                bucket_next[bucket_tail[rank]] = i;
                bucket_tail[rank] = i;
            }
        }

        if (fallback) {
            // 通常は起きないが、親がfrontier外なら安全側でselector順に追加する。
            for (const Candidate& cand : cands) add_selected(cand);
        } else {
            for (int rank = 0; rank < parent_count; ++rank) {
                for (int i = bucket_head[rank]; i != -1; i = bucket_next[i]) {
                    add_selected(cands[i]);
                }
            }
        }

        for (int id : frontier) parent_rank[id] = -1;
        selector.clear();
        refresh_runtime_usage();
    }

    template<class DebugHook>
    void set_best(int node_id, DebugHook& debug_hook) {
        const Node& node = nodes[node_id];
        if (rt.found && !(node.cost < rt.best_cost)) return;
        rt.found = true;
        rt.best_cost = node.cost;
        rt.best_turn = node.turn;
        best_node_id = node_id;
        ++rt.best_update_count;
        emit_hook(EventType::BestUpdate, debug_hook);
    }

    std::vector<Action> build_path(int node_id) const {
        std::vector<Action> path;
        if (node_id < 0) return path;
        path.reserve(nodes[node_id].turn);
        for (int v = node_id; v != -1 && nodes[v].parent != -1; v = nodes[v].parent) {
            path.push_back(nodes[v].action);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    void print_result_warnings(const Result<Action>& res) const {
        if (!param.print_warnings) return;
        if (res.node_pool_exhausted) {
            std::cerr << "[bs] warning: node_pool_exhausted; nodes_capacity="
                      << res.runtime.nodes_capacity << ", max_used_nodes=" << res.runtime.max_used_nodes << '\n';
        }
        if (res.hash_table_full) {
            std::cerr << "[bs] warning: hash_table_full; hash_capacity="
                      << res.runtime.hash_capacity << ", beam_width=" << param.beam_width << '\n';
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
        res.path = build_path(best_node_id);
        res.runtime = rt;
        return res;
    }
};

public:
    // 探索実行API。
    // Action型では外部状態を move_forward / move_backward で管理する。
    // initial_hash は必須で、NodeView.hash と次候補のhash計算に使う。
    template<
        class Action,
        class H,
        class Expand,
        class MoveForward,
        class MoveBackward,
        class DebugHook = NoOp>
        requires CompatibleHashArg<H, Hash>
    static Result<Action> run(
        const Param& param,
        Cost initial_cost,
        H initial_hash,
        Expand&& expand,
        MoveForward&& move_forward,
        MoveBackward&& move_backward,
        DebugHook&& debug_hook = DebugHook{}) {
        Engine<Action> engine(param, initial_cost, static_cast<Hash>(initial_hash));
        return engine.run(
            std::forward<Expand>(expand),
            std::forward<MoveForward>(move_forward),
            std::forward<MoveBackward>(move_backward),
            std::forward<DebugHook>(debug_hook)
        );
    }
};

// ============================================================
// Emitter 内部push定義
// ============================================================
// Engine本体の定義後に置くことで、公開型のまま内部pushへ直接転送する。
template<CostType Cost, HashType Hash>
template<class Action>
template<class U>
bool Beam<Cost, Hash>::Emitter<Action>::push_impl(U&& action, Cost cost, Hash hash, bool has_hash, bool finished) {
    return engine_.push_candidate(parent_, std::forward<U>(action), cost, hash, has_hash, finished);
}

}  // namespace bs

#if __INCLUDE_LEVEL__ == 0

#include <cstdlib>
#include <fstream>
#include <thread>

namespace {

struct Act {
    int add = 0;
    std::uint64_t token = 0;
};

template<class BeamType>
concept HasCandidateAlias = requires { typename BeamType::template Candidate<Act>; };

template<class ResultType>
concept HasReachedTurn = requires(ResultType r) { r.reached_turn; };

static_assert(bs::HashType<std::uint64_t>);
static_assert(bs::HashType<std::uint32_t>);
static_assert(!bs::HashType<int>);
static_assert(!bs::CompatibleHashArg<int, std::uint64_t>);
static_assert(bs::CompatibleHashArg<std::uint32_t, std::uint64_t>);
static_assert(!std::default_initializable<bs::Beam<int>::Emitter<Act>>);
static_assert(!HasCandidateAlias<bs::Beam<int>>);
static_assert(!HasReachedTurn<bs::Beam<int>::Result<Act>>);


void require(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "[test] failed: " << msg << '\n';
        std::exit(1);
    }
}

void check(bool cond, const std::string& msg) {
    require(cond, msg);
    std::cout << "[test] ok: " << msg << '\n';
}

std::uint64_t smix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// 多数のテストを少数のfunctor型で実行し、O2でも自己テストを短時間でコンパイルできるようにする。
struct IntWorld {
    int mode = 0;
    int sum = 0;
    std::uint64_t hash = 0;
    std::vector<Act> stack;
    std::uint64_t seed = 0;
    int turn1_expanded = 0;
    int expanded_after_root = 0;

    void reset(int m) {
        mode = m;
        sum = 0;
        hash = 0;
        stack.clear();
        seed = smix(0xC0FFEEULL + static_cast<std::uint64_t>(m));
        turn1_expanded = 0;
        expanded_after_root = 0;
    }
};

struct IntExpand {
    IntWorld* w = nullptr;

    template<class Emit>
    void operator()(bs::Beam<int>::NodeView now, Emit& emit) {
        switch (w->mode) {
            case 0: {  // emitなし。rootや異常パラメータの確認用。
                return;
            }
            case 1: {  // 小さな固定木。状態・hash・path復元を確認する。
                require(now.cost == w->sum && now.hash == w->hash, "NodeView matches external state");
                if (now.turn == 0) {
                    emit.push(Act{5, 11}, 5, static_cast<std::uint64_t>(smix(11)));
                    emit.push(Act{1, 22}, 1, static_cast<std::uint64_t>(smix(22)));
                    emit.push(Act{9, 33}, 9, static_cast<std::uint64_t>(smix(33)));
                } else if (now.turn == 1) {
                    emit.push(Act{-5, 44}, now.cost - 5, static_cast<std::uint64_t>(w->hash ^ smix(44)));
                    emit.push(Act{4, 55}, now.cost + 4, static_cast<std::uint64_t>(w->hash ^ smix(55)));
                } else if (now.turn == 2) {
                    emit.push(Act{1, 66}, now.cost + 1, static_cast<std::uint64_t>(w->hash ^ smix(66)));
                }
                return;
            }
            case 2: {  // 一本道。LCA climbのmove回数を確認する。
                require(now.turn == w->sum && now.cost == w->sum, "line state matches");
                emit.push(Act{1, 1}, now.cost + 1, static_cast<std::uint64_t>(now.turn + 1));
                return;
            }
            case 3: {  // shallow/wide。満杯後の早期幅枝刈りを確認する。
                if (now.turn == 0) {
                    for (int i = 0; i <= 1000; ++i) {
                        emit.push(Act{i, static_cast<std::uint64_t>(i)}, i, static_cast<std::uint64_t>(i));
                    }
                }
                return;
            }
            case 4: {  // hash重複。改善は置換し、悪化は棄却する。
                emit.push(Act{1, 1}, 10, static_cast<std::uint64_t>(999));
                emit.push(Act{2, 2}, 3, static_cast<std::uint64_t>(999));
                emit.push(Act{3, 3}, 7, static_cast<std::uint64_t>(999));
                return;
            }
            case 5: {  // 複数finished。生成順ではなく最良costを選ぶ。
                emit.push(Act{100, 1}, 100, static_cast<std::uint64_t>(100), true);
                emit.push(Act{1, 2}, 1, static_cast<std::uint64_t>(1), true);
                return;
            }
            case 6: {  // finishedなし。max_turn_is_answer=falseなら解なし。
                emit.push(Act{1, 1}, -1);
                return;
            }
            case 7: {  // node capacity overflow。
                emit.push(Act{1, 1}, 1, static_cast<std::uint64_t>(1));
                emit.push(Act{2, 2}, 2, static_cast<std::uint64_t>(2));
                return;
            }
            case 8: {  // 小規模ランダムstress。状態復元とpath再生可能性を見る。
                require(now.cost == w->sum, "random state matches");
                std::uint64_t local = smix(w->seed ^ w->hash ^ static_cast<std::uint64_t>(w->sum + now.turn * 1009));
                int branch = static_cast<int>(local % 7);
                if ((local >> 8) % 17 == 0) branch = 0;
                for (int k = 0; k < branch; ++k) {
                    local = smix(local + static_cast<std::uint64_t>(k + 1));
                    const int add = static_cast<int>((local >> 8) % 41) - 15;
                    const std::uint64_t token = smix(local ^ 0xABCDEF1234567890ULL);
                    const bool finished = (now.turn + 1 >= 8) || (((local >> 16) & 31ULL) == 0);
                    emit.push(Act{add, token}, w->sum + add, static_cast<std::uint64_t>(w->hash ^ smix(token)), finished);
                }
                return;
            }
            case 9: {  // time limit検出後、同じturnの未展開候補から最良1本を追加展開する。
                if (now.turn == 1) {
                    ++w->turn1_expanded;
                    std::this_thread::sleep_for(std::chrono::milliseconds(8));
                }
                if (now.turn > 0) ++w->expanded_after_root;
                for (int k = 0; k < 4; ++k) {
                    emit.push(Act{k, static_cast<std::uint64_t>(now.turn * 100 + k + 1)},
                              now.cost + k,
                              static_cast<std::uint64_t>(now.turn * 100 + k + 1),
                              false);
                }
                return;
            }
        }
    }
};

struct IntForward {
    IntWorld* w = nullptr;
    void operator()(const Act& a) {
        w->sum += a.add;
        w->hash ^= smix(a.token);
        w->stack.push_back(a);
    }
};

struct IntBackward {
    IntWorld* w = nullptr;
    void operator()(const Act& a) {
        require(!w->stack.empty() && w->stack.back().token == a.token, "move_backward is LIFO");
        w->stack.pop_back();
        w->hash ^= smix(a.token);
        w->sum -= a.add;
    }
};

struct DoubleExpand {
    template<class Emit>
    void operator()(bs::Beam<double>::NodeView, Emit& emit) const {
        emit.push(Act{1, 1}, std::numeric_limits<double>::quiet_NaN());
        emit.push(Act{2, 2}, 2.5, true);
    }
};

// 公開Emitter型と32bit Hashを明示指定できることを確認する。
struct U32TypedExpand {
    void operator()(const bs::Beam<int, std::uint32_t>::NodeView& now,
                    bs::Beam<int, std::uint32_t>::Emitter<Act>& emit) const {
        require(now.hash == std::uint32_t{0}, "typed NodeView hash is uint32_t");
        emit.push(Act{4, 4}, 4, std::uint32_t{4});
        emit.push(Act{2, 2}, 2, std::uint32_t{2});
    }
};

void run_unit_tests() {
    using BS = bs::Beam<int>;
    using BSD = bs::Beam<double>;
    using BSU32 = bs::Beam<int, std::uint32_t>;
    static_assert(std::same_as<decltype(BS::Param{}.time_limit_ms), double>);
    static_assert(std::same_as<decltype(BS::Runtime{}.time_limit_ms), double>);
    using Param = BS::Param;
    static_assert(!std::is_default_constructible_v<BS::Emitter<Act>>);
    IntWorld world;
    IntExpand expand{&world};
    IntForward mf{&world};
    IntBackward mb{&world};

    // max_turn=0、max_turn_is_answer=false、異常パラメータ正規化。
    {
        world.reset(0);
        Param p;
        p.max_turn = 0;
        p.beam_width = 3;
        auto res = BS::run<Act>(p, 7, 0ULL, expand, mf, mb);
        check(res.found && res.best_cost == 7 && res.path.empty(), "root answer at max_turn zero");
        check(world.sum == 0 && world.hash == 0 && world.stack.empty(), "root state restored");
    }
    {
        world.reset(0);
        Param p;
        p.max_turn = 0;
        p.max_turn_is_answer = false;
        auto res = BS::run<Act>(p, 7, 0ULL, expand, mf, mb);
        check(!res.found && res.path.empty(), "root is not answer when disabled");
    }
    {
        world.reset(0);
        Param p;
        p.max_turn = -5;
        p.beam_width = 0;
        auto res = BS::run<Act>(p, 3, 0ULL, expand, mf, mb);
        check(res.found && res.runtime.max_turn == 0 && res.runtime.beam_width == 1,
              "invalid params are normalized");
    }

    // LCA差分移動、bucket materialize、path復元、root復元。
    {
        world.reset(1);
        Param p;
        p.max_turn = 3;
        p.beam_width = 2;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(res.found && res.best_cost == -3 && res.best_turn == 3, "basic tree best cost");
        check(res.path.size() == 3 && res.path[0].add == 1 && res.path[1].add == -5 && res.path[2].add == 1,
              "basic tree path restored");
        check(world.sum == 0 && world.hash == 0 && world.stack.empty(), "state restored to root after run");
    }
    {
        world.reset(2);
        Param p;
        p.max_turn = 100;
        p.beam_width = 1;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(res.found && res.best_cost == 100 && world.sum == 0, "line search succeeds");
        check(res.runtime.move_forward_count == 99 && res.runtime.move_backward_count == 99,
              "LCA climb move count is optimized");
    }

    // Candidate構築前の早期幅枝刈り。
    {
        world.reset(3);
        Param p;
        p.max_turn = 1;
        p.beam_width = 1;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(res.found && res.best_cost == 0 && res.runtime.pruned_by_width >= 1000,
              "early width prune works");
    }

    // hash重複、finished、max_turn_is_answer=false、容量不足。
    {
        world.reset(4);
        Param p;
        p.max_turn = 1;
        p.beam_width = 4;
        p.use_hash_dedup = true;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(res.found && res.best_cost == 3 && res.path[0].add == 2, "hash duplicate replacement");
        check(res.runtime.duplicate_by_hash == 2 && res.runtime.pruned_by_hash == 1, "hash stats counted");
    }
    {
        world.reset(5);
        Param p;
        p.max_turn = 1;
        p.beam_width = 4;
        p.max_turn_is_answer = false;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(res.found && res.best_cost == 1 && res.path[0].add == 1, "best finished candidate is selected");
    }
    {
        world.reset(6);
        Param p;
        p.max_turn = 1;
        p.beam_width = 2;
        p.max_turn_is_answer = false;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(!res.found, "no finished no answer when max-turn answer disabled");
    }
    {
        world.reset(7);
        Param p;
        p.max_turn = 2;
        p.beam_width = 3;
        p.nodes_capacity = 1;
        p.print_warnings = false;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(res.node_pool_exhausted && res.runtime.overflow_nodes > 0, "node capacity exhaustion reported");
    }

    // time limit 検出後、同じturnの未展開候補から最良1本を追加展開する。
    {
        world.reset(9);
        Param p;
        p.max_turn = 4;
        p.beam_width = 4;
        p.time_limit_ms = 5;
        p.time_check_interval = 1;
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
        check(res.time_limit_reached && res.completed_max_turn && res.found && res.best_turn == 4,
              "soft time limit completes by minimal path");
        check(world.turn1_expanded == 2 && world.expanded_after_root == 4,
              "soft time limit expands best remaining node in same turn");
        check(world.sum == 0 && world.hash == 0 && world.stack.empty(),
              "soft time limit restores state");
    }

    // NaNとCSV Hook。
    {
        BSD::Param p;
        p.max_turn = 1;
        p.beam_width = 2;
        auto noop = [](const Act&) {};
        auto res = BSD::run<Act>(p, 0.0, 0ULL, DoubleExpand{}, noop, noop);
        check(res.found && res.best_cost == 2.5 && res.runtime.invalid_candidates == 1, "NaN rejected");
    }
    {
        BSU32::Param p;
        p.max_turn = 1;
        p.beam_width = 2;
        auto noop = [](const Act&) {};
        auto res = BSU32::run<Act>(p, 0, std::uint32_t{0}, U32TypedExpand{}, noop, noop);
        check(res.found && res.best_cost == 2 && res.path[0].token == 2,
              "public Emitter and uint32_t hash work");
    }
    {
        world.reset(2);
        const std::string csv_file = "beam_delta_single_v32_stat.csv";
        std::remove(csv_file.c_str());
        Param p;
        p.max_turn = 2;
        p.beam_width = 2;
        BS::CsvStatHook csv_hook(csv_file, 0.0);
        auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb, csv_hook);
        std::ifstream ifs(csv_file);
        std::string line;
        bool has_header = false, has_restored_cols = false, has_run_end = false;
        while (std::getline(ifs, line)) {
            const bool has_move = line.find("move_forward_count") != std::string::npos;
            const bool has_period = line.find("period_ms") != std::string::npos;
            const bool has_rates = line.find("expand_per_sec") != std::string::npos;
            const bool has_caps = line.find("nodes_capacity") != std::string::npos;
            has_header |= has_move;
            has_restored_cols |= has_period && has_rates && has_caps;
            has_run_end |= line.find("RunEnd") != std::string::npos;
        }
        check(res.found && has_header && has_restored_cols && has_run_end, "CsvStatHook writes restored stats");
        std::remove(csv_file.c_str());
    }

    // 小規模ランダムstress。
    {
        long long expanded = 0;
        long long generated = 0;
        for (int tc = 0; tc < 40; ++tc) {
            world.reset(8);
            world.seed = smix(0xC0FFEEULL + static_cast<std::uint64_t>(tc) * 1000003ULL);
            Param p;
            p.max_turn = 8;
            p.beam_width = 1 + (tc % 6);
            p.use_hash_dedup = (tc % 2) == 0;
            p.nodes_capacity = 1 + p.beam_width * p.max_turn;
            p.print_warnings = false;
            auto res = BS::run<Act>(p, 0, 0ULL, expand, mf, mb);
            require(world.sum == 0 && world.hash == 0 && world.stack.empty(), "random state restored");
            if (res.found) {
                int replay_cost = 0;
                for (const Act& a : res.path) replay_cost += a.add;
                require(replay_cost == res.best_cost, "random path replay cost");
                require(static_cast<int>(res.path.size()) == res.best_turn, "random path replay turn");
            }
            expanded += res.runtime.expanded_nodes;
            generated += res.runtime.generated_candidates;
        }
        std::cout << "[random] cases=40 expanded=" << expanded << " generated=" << generated << '\n';
        check(expanded > 0 && generated > 0, "random stress completed");
    }
}

}  // namespace

int main() {
    run_unit_tests();
    std::cout << "[test] all beam_delta_single_v32 tests passed\n";
    return 0;
}

#endif

