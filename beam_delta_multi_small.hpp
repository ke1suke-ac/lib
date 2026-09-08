#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace bs {

// ============================================================
// detail: 低レベル補助関数
// ============================================================

namespace detail {

// SFINAE用の軽量な型判定だけを内部に置く。
template<class T>
inline constexpr bool is_hash_type_v =
    std::is_integral_v<T> && std::is_unsigned_v<T> &&
    (!std::is_same_v<T, bool>) && (sizeof(T) <= sizeof(std::uint64_t));

template<class H, class Hash>
inline constexpr bool is_compatible_hash_arg_v =
    is_hash_type_v<Hash> && is_hash_type_v<std::remove_cv_t<std::remove_reference_t<H>>> &&
    std::is_convertible_v<std::remove_cv_t<std::remove_reference_t<H>>, Hash> &&
    (sizeof(std::remove_cv_t<std::remove_reference_t<H>>) <= sizeof(Hash));

template<class S>
inline constexpr bool is_step_arg_v = std::is_same_v<std::remove_cv_t<std::remove_reference_t<S>>, int>;

template<class B>
inline constexpr bool is_bool_arg_v = std::is_same_v<std::remove_cv_t<std::remove_reference_t<B>>, bool>;

template<class U, class Action>
inline constexpr bool is_action_arg_v = std::is_same_v<std::remove_cv_t<std::remove_reference_t<U>>, Action>;

// Beamに依存しない低レベル補助関数。
inline std::int64_t now_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace detail
// Cost と Hash を固定したターン制ビームサーチ本体。
// using BS = bs::Beam<std::int64_t, std::uint64_t>; のように別名化して使う。
template<class Cost = long long, class Hash = std::uint64_t>
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

// Hook種別。縮小版ではdebug_hookを発火しないが、公開API互換のため列挙値は残す。
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

// expand から観測でき、かつ探索制御にも使う最小限の実行時情報。
struct Runtime {
    int turn = 0;                         // 現在処理中のターン。Runtime縮小後も互換用に残す。
    int nodes_capacity = 0;               // node_pool_exhausted警告の表示に使う設定済み容量。
    int hash_capacity = 0;                // hash_table_full警告の表示に使うHash表容量。
    int used_nodes = 0;                   // ノードプール使用量。
    int max_used_nodes = 0;               // node_pool_exhausted警告の表示に使う最大使用量。
    long long expanded_nodes = 0;         // 時間確認間隔の判定に使う展開済みノード数。
    long long overflow_nodes = 0;         // Result::node_pool_exhausted の判定に使う。
    long long hash_table_full = 0;        // Result::hash_table_full の判定に使う。
    long long remove_entry_overflow = 0;  // 遅延削除entry不足の警告判定に使う。
    int pending_candidates = 0;           // 未到着候補数。探索打ち切り判定に使う。
    std::int64_t elapsed_us = 0;          // 探索開始からの経過時間。時間制限判定に使う。
    bool time_limit_reached = false;      // soft limit到達後の縮退探索判定に使う。
    bool found = false;                   // 最良解の有無。
    Cost best_cost{};                     // 現在の最良コスト。
    int best_turn = 0;                    // 現在の最良解到着ターン。
};

// デフォルトHook。何もしない。
struct NoOp {
    void operator()(EventType, const Runtime&) const noexcept {}
};


// CSV出力Hook。縮小版ではコンストラクタ互換だけを残し、記録は行わない。
struct CsvStatHook {
    explicit CsvStatHook(std::string csv_filename = "beam_stat.csv", double interval_ms = 50.0) {
        (void)csv_filename;
        (void)interval_ms;
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
    template<class U, class S>
    std::enable_if_t<detail::is_action_arg_v<U, Action> && detail::is_step_arg_v<S>, bool>
    push(U&& action, Cost cost, S step) {
        return push_impl(std::forward<U>(action), cost, now_hash_, step, false, false);
    }

    template<class U, class S, class B>
    std::enable_if_t<detail::is_action_arg_v<U, Action> &&
                     detail::is_step_arg_v<S> && detail::is_bool_arg_v<B>, bool>
    push(U&& action, Cost cost, S step, B finished) {
        return push_impl(std::forward<U>(action), cost, now_hash_, step, false, finished);
    }

    template<class U, class H, class S>
    std::enable_if_t<detail::is_action_arg_v<U, Action> &&
                     detail::is_compatible_hash_arg_v<H, Hash> && detail::is_step_arg_v<S>, bool>
    push(U&& action, Cost cost, H hash, S step) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), step, true, false);
    }

    template<class U, class H, class S, class B>
    std::enable_if_t<detail::is_action_arg_v<U, Action> &&
                     detail::is_compatible_hash_arg_v<H, Hash> &&
                     detail::is_step_arg_v<S> && detail::is_bool_arg_v<B>, bool>
    push(U&& action, Cost cost, H hash, S step, B finished) {
        return push_impl(std::forward<U>(action), cost, static_cast<Hash>(hash), step, true, finished);
    }

private:
    friend class Engine<Action>;

    Emitter(Engine<Action>& engine, int turn, int parent, Hash now_hash) noexcept
        : engine_(engine), turn_(turn), parent_(parent), now_hash_(now_hash) {}

    template<class U>
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
        capacity_ = 1;
        while (capacity_ < cap) capacity_ <<= 1;
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
        std::uint64_t h = static_cast<std::uint64_t>(key);
        h += 0x9e3779b97f4a7c15ULL;
        h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
        h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
        h ^= h >> 31;
        int i = static_cast<int>(h) & mask_;
        for (int probe = 0; probe < capacity_; ++probe) {
            if (used_[i] != epoch_) return Lookup{false, i};
            if (keys_[i] == key) return Lookup{true, i};
            i = (i + 1) & mask_;
        }
        return Lookup{false, -1};
    }

    void set(int slot, Hash key, int value) {
        if (slot < 0) return;
        used_[slot] = epoch_;
        keys_[slot] = key;
        values_[slot] = value;
    }
public:
    // FixedHashMap自体が非公開内部型なので、Selectorからslot値を直接読む。
    int capacity_ = 0;
    int mask_ = 0;
    std::uint32_t epoch_ = 1;
    std::vector<Hash> keys_;
    std::vector<int> values_;
    std::vector<std::uint32_t> used_;

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
            (void)fn;
            (void)edge;
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


    // 1回の run で使うユーザーcallback群。
    // debug_hookは縮小版では受け取るだけで使用しない。
    template<class Expand, class MoveForward, class MoveBackward>
    struct RunContext {
        Expand& expand;
        MoveForward& move_forward;
        MoveBackward& move_backward;
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
        friend class Emitter<Action>;
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
        std::vector<Candidate>& selected() { return candidates_; }
        const std::vector<Candidate>& selected() const { return candidates_; }

        PushResult push(Candidate cand) {
            if (!(use_hash_ && cand.has_hash)) {
                if (!full_) {
                    append_candidate(std::move(cand));
                    return PushResult(PushResult::Accepted);
                }
                const int j = seg_[1].second;
                if (j < 0) return {};
                replace_candidate(j, std::move(cand));
                return PushResult(PushResult::Accepted);
            }

            auto found = hash_.lookup(cand.hash);
            if (found.found) {
                const int j = hash_.values_[found.slot];
                if (0 <= j && j < static_cast<int>(candidates_.size()) &&
                    candidates_[j].has_hash && candidates_[j].hash == cand.hash) {
                    if (!(cand.cost < candidates_[j].cost)) {
                        return PushResult(PushResult::Duplicate | PushResult::HashRejected);
                    }
                    replace_candidate(j, std::move(cand));
                    return PushResult(PushResult::Accepted | PushResult::Duplicate);
                }
                // stale entryなので、このslotを再利用できる。
            }
            if (found.slot < 0) {
                hash_.clear();
                for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
                    if (!candidates_[i].has_hash) continue;
                    auto slot = hash_.lookup(candidates_[i].hash);
                    if (slot.slot >= 0) hash_.set(slot.slot, candidates_[i].hash, i);
                }
                found = hash_.lookup(cand.hash);
                if (found.slot < 0) return PushResult(PushResult::HashFull);
            }

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


        void append_candidate(Candidate&& cand) {
            change_future_child(cand.parent, +1);
            candidates_.push_back(std::move(cand));
            if (static_cast<int>(candidates_.size()) == beam_width_) {
                full_ = true;
                std::fill(seg_.begin(), seg_.end(), empty_worst());
                for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = {candidates_[i].cost, i};
                for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_pair(seg_[i << 1], seg_[i << 1 | 1]);
            }
        }

        void replace_candidate(int j, Candidate&& cand) {
            change_future_child(candidates_[j].parent, -1);
            change_future_child(cand.parent, +1);
            const Cost cost = cand.cost;
            candidates_[j] = std::move(cand);
            if (full_) {
                int i = seg_n_ + j;
                seg_[i] = {cost, j};
                while (i >>= 1) seg_[i] = worse_pair(seg_[i << 1], seg_[i << 1 | 1]);
            }
        }

        void change_future_child(int node_id, int delta) {
            if (node_id < 0) return;
            (*nodes_)[node_id].future_children += delta;
            assert((*nodes_)[node_id].future_children >= 0);
        }


    };

public:
    Engine(Param param_, Cost initial_cost_, Hash initial_hash_)
        : param(param_), initial_cost(initial_cost_), initial_hash(initial_hash_) {
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

    template<class Expand, class MoveForward, class MoveBackward, class DebugHook>
    Result<Action> run(Expand&& expand, MoveForward&& move_forward,
                       MoveBackward&& move_backward, DebugHook&& debug_hook) {
        using Ctx = RunContext<std::remove_reference_t<Expand>, std::remove_reference_t<MoveForward>,
                               std::remove_reference_t<MoveBackward>>;
        (void)debug_hook;
        Ctx ctx{expand, move_forward, move_backward};
        rt = Runtime{};
        start_clock_us = detail::now_microseconds();
        if (param.time_limit_ms > 0.0) {
            const double us = param.time_limit_ms * 1000.0;
            constexpr double max_us = static_cast<double>(std::numeric_limits<std::int64_t>::max());
            limit_us = (us >= max_us) ? std::numeric_limits<std::int64_t>::max()
                                      : std::max<std::int64_t>(1, static_cast<std::int64_t>(us + 0.5));
        } else {
            limit_us = 0;
        }
        use_time_limit = (limit_us > 0);

        // 1回の探索で使う内部バッファを確保し、Selectorと遅延削除表を初期化する。
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

        // rootノードを作り、探索木・使用量・最良解情報を初期状態へ戻す。
        root = 0;
        used_nodes = 0;
        live_nodes = 0;
        next_generation = 1;
        node_pool_exhausted = false;
        completed_max_turn = false;
        rt.nodes_capacity = param.nodes_capacity;
        rt.hash_capacity = param.hash_capacity;
        rt.used_nodes = 0;
        rt.max_used_nodes = 0;
        rt.pending_candidates = 0;

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

        rt.turn = 0;
        rt.best_cost = initial_cost;
        rt.best_turn = 0;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        if (is_answer(root)) set_best(root);

        bool minimal_mode = rt.time_limit_reached;
        completed_max_turn = (param.max_turn == 0);

        for (int current_turn = 0; current_turn < param.max_turn; ++current_turn) {
            rt.turn = current_turn;
            if (mark_time_limit_if_elapsed()) minimal_mode = true;

            if (!minimal_mode) {
                // 通常時はrootからDFSでactiveな葉をたどり、このturnの全幅を展開する。
                remove_useless_nodes(current_turn);
                update_root(ctx);
                int v = root;
                if (nodes[v].active) {
                    while (!minimal_mode) {
                        int child = nodes[v].child;
                        while (child != -1) {
                            while (child != -1 && !nodes[child].active) child = nodes[child].right;
                            if (child == -1) break;
                            nodes[v].active = false;
                            v = child;
                            call_move(ctx.move_forward, nodes[v].edge);
                            child = nodes[v].child;
                        }
                        nodes[v].active = false;
                        if (nodes[v].turn != current_turn) {
                            move_direct_to_root(v, ctx.move_backward);
                            break;
                        }
                        expand_leaf(v, ctx);
                        // soft limit判定。expand完了後に間引いて呼び、到達後は縮退探索へ移る。
                        // expandの途中はライブラリ側では中断しない。
                        if (use_time_limit && (rt.expanded_nodes % param.time_check_interval) == 0 && mark_time_limit_if_elapsed()) {
                            deactivate_leaf_path(v);
                            move_direct_to_root(v, ctx.move_backward);
                            minimal_mode = true;
                            break;
                        }
                        while (v != root) {
                            call_move(ctx.move_backward, nodes[v].edge);
                            int u = nodes[v].right;
                            while (u != -1) {
                                if (nodes[u].active) {
                                    call_move(ctx.move_forward, nodes[u].edge);
                                    v = u;
                                    break;
                                }
                                u = nodes[u].right;
                            }
                            if (u != -1) break;
                            v = nodes[v].parent;
                        }
                        if (v == root) break;
                    }
                }
            }

            if (minimal_mode) {
                // 時間切れ後は状態を巻き戻せる範囲で、現在turnの最良葉だけを展開する。
                remove_useless_nodes(current_turn);
                update_root(ctx);
                int v = best_active_leaf_at_turn(current_turn);
                if (v >= 0) {
                    tmp_nodes.clear();
                    tmp_ids.clear();
                    tmp_nodes.push_back(root);
                    while (!tmp_nodes.empty()) {
                        const int u = tmp_nodes.back();
                        tmp_nodes.pop_back();
                        if (u < 0 || u >= used_nodes) continue;
                        const Node& node = nodes[u];
                        if (!node.alive || !node.active || node.turn > current_turn) continue;
                        if (node.child == -1) {
                            if (node.turn == current_turn && u != v) tmp_ids.push_back(u);
                            continue;
                        }
                        for (int c = node.child; c != -1; c = nodes[c].right) tmp_nodes.push_back(c);
                    }
                    for (int leaf : tmp_ids) {
                        if (leaf < 0 || leaf >= used_nodes || !nodes[leaf].alive || !nodes[leaf].active) continue;
                        deactivate_leaf_path(leaf);
                        if (nodes[leaf].child == -1 && nodes[leaf].future_children == 0 && !nodes[leaf].in_remove_queue) remove_leaf(leaf);
                    }
                    update_root(ctx);
                    if (!nodes[v].alive) v = best_active_leaf_at_turn(current_turn);
                    if (v >= 0) {
                        tmp_nodes.clear();
                        for (int u = v; u != root; u = nodes[u].parent) tmp_nodes.push_back(u);
                        for (int i = static_cast<int>(tmp_nodes.size()) - 1; i >= 0; --i) call_move(ctx.move_forward, nodes[tmp_nodes[i]].edge);
                        expand_leaf(v, ctx);
                        deactivate_leaf_path(v);
                        move_direct_to_root(v, ctx.move_backward);
                    }
                }
            }

            // 次turnに到着した候補を探索木へ接続する。時間切れ後は最良候補だけを残す。
            Selector& selector = selectors[(current_turn + 1) % ring_size];
            if (!selector.empty()) {
                auto& cands = selector.selected();
                int best_idx = -1;
                if (minimal_mode) {
                    best_idx = 0;
                    for (int i = 1; i < static_cast<int>(cands.size()); ++i) if (cands[i].cost < cands[best_idx].cost) best_idx = i;
                }
                for (int i = 0; i < static_cast<int>(cands.size()); ++i) {
                    if (minimal_mode && i != best_idx) { release_future_child(cands[i]); continue; }
                    const Candidate& cand = cands[i];
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
                    const int id = new_node(node);
                    if (id < 0) { release_future_child(cand); continue; }
                    --nodes[parent].future_children;
                    assert(nodes[parent].future_children >= 0);
                    if (nodes[parent].child != -1) nodes[nodes[parent].child].left = id;
                    nodes[parent].child = id;
                    int u = parent;
                    while (!nodes[u].active) {
                        nodes[u].active = true;
                        if (u == root) break;
                        u = nodes[u].parent;
                    }
                    if (is_answer(id)) set_best(id);
                    completed_max_turn |= (nodes[id].turn == param.max_turn);
                }
                rt.pending_candidates -= selector.size();
                refresh_runtime_usage();
                selector.clear();
            }

            refresh_runtime_usage();
            if (mark_time_limit_if_elapsed()) minimal_mode = true;

            if (completed_max_turn) break;
            if (node_pool_exhausted) break;
            if (rt.time_limit_reached) minimal_mode = true;

            // 未来候補もactive葉もない場合は、max_turnまで空回ししない。
            if (rt.pending_candidates == 0 && !nodes[root].active) break;
        }

        // 探索木の不要ノードを明示解放し、探索中に進めたroot prefix分を巻き戻す。
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

        for (int i = static_cast<int>(committed_edges.size()) - 1; i >= 0; --i) {
            call_move(ctx.move_backward, committed_edges[i]);
        }
        refresh_runtime_usage();
        update_time();
        // Resultには、探索木を片付けた後に残した最良経路と最小Runtimeだけを詰める。
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

        if (param.print_warnings) {
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

    void refresh_runtime_usage() {
        rt.used_nodes = used_nodes;
        rt.max_used_nodes = std::max(rt.max_used_nodes, rt.used_nodes);
        if (rt.pending_candidates < 0) rt.pending_candidates = 0;
    }

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
    }

    bool mark_time_limit_if_elapsed() {
        if (!use_time_limit) return false;
        update_time();
        if (rt.elapsed_us < limit_us) return false;
        rt.time_limit_reached = true;
        return true;
    }

    // ノードプールからノードIDを確保する。容量超過はRuntimeに記録し、結果と警告へ反映する。
    int new_node(const Node& node) {
        int id;
        if (!free_nodes.empty()) {
            id = free_nodes.back();
            free_nodes.pop_back();
            nodes[id] = node;
        } else {
            if (static_cast<int>(nodes.size()) >= param.nodes_capacity) {
                node_pool_exhausted = true;
                ++rt.overflow_nodes;
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

            child = nodes[root].child;
        }
    }

    template<class MoveBackward>
    void move_direct_to_root(int v, MoveBackward& move_backward) {
        while (v != root) {
            call_move(move_backward, nodes[v].edge);
            v = nodes[v].parent;
        }
    }

    bool is_answer(int id) const {
        const Node& node = nodes[id];
        return node.finished || (param.max_turn_is_answer && node.turn == param.max_turn);
    }

    template<class Ctx>
    void expand_leaf(int v, Ctx& ctx) {
        // expand は Runtime なし、または const Runtime& ありの2系統を許可する。
        // Runtime はライブラリ管理情報なので、ユーザー側から変更できないよう const 参照だけを渡す。
        NodeView now{nodes[v].turn, nodes[v].cost, nodes[v].hash};
        current_expand_max_step = 1;
        Emitter<Action> emit(*this, now.turn, v, now.hash);
        if constexpr (std::is_invocable_v<decltype(ctx.expand)&, const NodeView&, const Runtime&, Emitter<Action>&>) {
            // Runtime付きexpandには保持量だけを最新化して渡す。
            // elapsed_usはtime limit判定・RunEndで更新し、expandごとの時刻取得を避ける。
            refresh_runtime_usage();
        }
        if constexpr (std::is_invocable_v<decltype(ctx.expand)&, const NodeView&, const Runtime&, Emitter<Action>&>) {
            ctx.expand(now, static_cast<const Runtime&>(rt), emit);
        } else if constexpr (std::is_invocable_v<decltype(ctx.expand)&, const NodeView&, Emitter<Action>&>) {
            ctx.expand(now, emit);
        } else {
            (void)now;
            (void)emit;
        }
        ++rt.expanded_nodes;

        // 削除は、この葉から最も遅く届く候補が到着するまでは遅延する。
        // future_childrenが残っていればremove_leaf側で削除されない。
        const int remove_turn = std::min(param.max_turn, nodes[v].turn + current_expand_max_step);
        if (remove_free.empty()) {
            ++rt.remove_entry_overflow;
        } else {
            const int entry_id = remove_free.back();
            remove_free.pop_back();
            const int slot = remove_turn % ring_size;
            remove_entries[entry_id] = RemoveEntry{v, nodes[v].generation, remove_heads[slot]};
            remove_heads[slot] = entry_id;
            nodes[v].in_remove_queue = true;
        }
    }

    void release_future_child(const Candidate& cand) {
        if (cand.parent >= 0 && cand.parent < used_nodes && nodes[cand.parent].alive) {
            --nodes[cand.parent].future_children;
            assert(nodes[cand.parent].future_children >= 0);
        }
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

    void set_best(int node_id) {
        const Node& node = nodes[node_id];
        if (!rt.found) {
            rt.found = true;
        } else if (!(node.cost < rt.best_cost)) {
            return;
        }
        rt.best_cost = node.cost;
        rt.best_turn = node.turn;
        update_time();

        best_path.clear();
        best_path.insert(best_path.end(), committed_edges.begin(), committed_edges.end());

        tmp_path.clear();
        for (int v = node_id; v != -1 && nodes[v].parent != -1; v = nodes[v].parent) {
            tmp_path.push_back(nodes[v].edge);
        }
        for (int i = static_cast<int>(tmp_path.size()) - 1; i >= 0; --i) {
            best_path.push_back(tmp_path[i]);
        }
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
    static std::enable_if_t<detail::is_compatible_hash_arg_v<InitialHash, Hash>, Result<Action>>
    run(
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

template<class Cost, class Hash>
template<class Action>
template<class U>
bool Beam<Cost, Hash>::Emitter<Action>::push_impl(U&& action, Cost cost, Hash hash, int step, bool has_hash, bool finished) {
    auto& e = engine_;
    if constexpr (std::is_floating_point_v<Cost>) {
        if (std::isnan(cost)) return false;
    }
    if (step <= 0 || step > e.param.max_step || turn_ + step > e.param.max_turn) return false;
    auto& selector = e.selectors[(turn_ + step) % e.ring_size];
    // Selectorが満杯なら、現在の最悪コスト以上の候補は幅制限で棄却する。
    if (selector.full_) {
        const auto worst = selector.seg_[1];
        if (worst.second >= 0 && !(cost < worst.first)) return false;
    }
    using Candidate = typename Beam<Cost, Hash>::template Engine<Action>::Candidate;
    Candidate cand{Edge<Action>{std::forward<U>(action), turn_, step}, cost, hash, parent_, has_hash, finished};
    const int before = selector.size();
    const auto res = selector.push(std::move(cand));
    using PushResult = typename Beam<Cost, Hash>::template Engine<Action>::Selector::PushResult;
    if (res.has(PushResult::HashFull)) ++e.rt.hash_table_full;
    if (!res.has(PushResult::Accepted)) return false;
    if (step > e.current_expand_max_step) e.current_expand_max_step = step;
    if (selector.size() > before) ++e.rt.pending_candidates;
    return true;
}


}  // namespace bs

#if __INCLUDE_LEVEL__ == 0
#include <memory>
#include <sstream>

namespace {

using BS = bs::Beam<int, std::uint64_t>;
using BS32 = bs::Beam<int, std::uint32_t>;

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

        const std::string prefix = "random stress case " + std::to_string(case_id);
        tc.check(ctx.ok, prefix + " state view and LIFO are consistent");
        tc.check(ctx.turn == 0 && ctx.sum == 0 && ctx.state_hash == 0 && ctx.stack.empty(),
                 prefix + " restores external state");
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
        int hook_calls = 0;
        auto hook = [&](BS::EventType, const BS::Runtime&) { ++hook_calls; };
        auto p = param_base(0);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::None, 0}, NoMove{}, NoMove{}, hook);
        tc.check(res.found && hook_calls == 0, "debug_hook is accepted but ignored in small version");
    }
    {
        auto p = param_base(3, 2, 1);
        BS::CsvStatHook hook("bs_csv_stat_hook_test.csv", 0.0);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::CsvPath, 3}, NoMove{}, NoMove{}, hook);
        tc.check(res.found, "CsvStatHook empty implementation is accepted");
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
        tc.check(res.found && res.best_cost == 1,
                 "hash_capacity=0 auto-enables capacity when dedup is requested");
    }
    {
        auto p = param_base(1, 4, 1);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::HashDedup, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 1, "hash dedup is enabled by default");
    }
    {
        auto p = param_base(1, 1, 1);
        p.use_hash_dedup = false;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::NoDedup, 1}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_cost == 3,
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
        tc.check(res.runtime.pending_candidates == 0,
                 "single path releases pending candidates");
    }
    {
        int state = 0;
        int hook_calls = 0;
        auto hook = [&](BS::EventType, const BS::Runtime&) { ++hook_calls; };
        auto p = param_base(5, 4, 3);
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::LcaCompress, 5}, Fwd{&state}, Bwd{&state}, hook);
        tc.check(res.found && hook_calls == 0, "LCA compression works and debug_hook remains ignored");
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
        tc.check(res.runtime.pending_candidates == 0,
                 "finished search releases pending candidates");
    }
    {
        BS::Param p;
        p.max_turn = 20;
        p.beam_width = 1;
        p.print_warnings = false;
        auto res = BS::run<Act>(p, 0, std::uint64_t{0}, Expand{Mode::SinglePath, 20}, NoMove{}, NoMove{});
        tc.check(res.found && res.best_turn == 20 && !res.node_pool_exhausted,
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
        tc.check(res.node_pool_exhausted,
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
        tc.check(res.best_cost == 2, "hashless candidates are not mixed into hash dedup");
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
        tc.check(res.best_cost == 3,
                 "hash duplicate improvement still replaces candidate");
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
        tc.check(res.runtime.pending_candidates == 0,
                 "minimal cleanup releases pending candidates");
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
        tc.check(res.found && !res.node_pool_exhausted && res.runtime.pending_candidates == 0,
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
        std::cout << "beam_delta_multi_small_v59 self tests passed: " << tc.passed << " checks\n";
        return 0;
    }
    std::cerr << "beam_delta_multi_small_v59 self tests failed: " << tc.failed << " / " << (tc.passed + tc.failed) << " checks\n";
    return 1;
}
#endif
