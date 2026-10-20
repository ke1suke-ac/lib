#pragma once
#include <bits/stdc++.h>

#ifndef BEAM_COPY_HISTORY_STORAGE_V01
#define BEAM_COPY_HISTORY_STORAGE_V01
namespace beam_history_detail {
// 出力用履歴だけを共有する。祖先のStateは保持しない。
// indexで参照するためvector再確保でもLink/Viewは有効。Action参照は再取得が必要。
template<class Value>
class Pool {
public:
    static constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
private:
    struct Node {
        std::optional<Value> value;
        std::size_t parent = none, refs = 0;
    };
    std::vector<Node> nodes_;
    std::size_t free_ = none;
    void retain(std::size_t id) noexcept { if (id != none) ++nodes_[id].refs; }
    void release(std::size_t id) noexcept {
        // 長い経路も再帰せず解放。各ノードの解放費用は生成回数に償却される。
        while (id != none && --nodes_[id].refs == 0) {
            auto& node = nodes_[id];
            const auto parent = node.parent;
            node.value.reset(); node.parent = free_; free_ = id;
            id = parent;
        }
    }
public:
    class Link {
        friend class Pool;
        Pool* pool_ = nullptr;
        std::size_t id_ = none;
        Link(Pool* pool, std::size_t id) noexcept : pool_(pool), id_(id) {}
    public:
        Link() = default;
        Link(const Link& other) noexcept : pool_(other.pool_), id_(other.id_) {
            if (pool_) pool_->retain(id_);
        }
        Link(Link&& other) noexcept
            : pool_(std::exchange(other.pool_, nullptr)), id_(other.id_) {}
        Link& operator=(const Link& other) noexcept {
            if (this != &other) { Link copy(other); swap(copy); }
            return *this;
        }
        Link& operator=(Link&& other) noexcept {
            if (this != &other) { Link moved(std::move(other)); swap(moved); }
            return *this;
        }
        ~Link() { if (pool_) pool_->release(id_); }
        void swap(Link& other) noexcept {
            std::swap(pool_, other.pool_); std::swap(id_, other.id_);
        }
        std::size_t id() const noexcept { return pool_ ? id_ : none; }
    };
    Pool() = default;
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    template<class... Args>
    Link make(std::size_t parent, Args&&... args) {
        std::size_t id = free_;
        if (id == none) {
            id = nodes_.size();
            nodes_.push_back(Node{std::optional<Value>(std::in_place, std::forward<Args>(args)...), parent, 1});
        } else {
            auto& node = nodes_[id];
            node.value.emplace(std::forward<Args>(args)...);
            free_ = node.parent; node.parent = parent; node.refs = 1;
        }
        retain(parent);
        return Link(this, id);
    }
    const Value& value(std::size_t id) const noexcept { return *nodes_[id].value; }
    std::size_t parent(std::size_t id) const noexcept { return nodes_[id].parent; }
};
template<class State, class Value>
struct TracedState {
    State state;
    typename Pool<Value>::Link trace;
};
} // namespace beam_history_detail
#endif

// コピー型 chokudai サーチ（コスト最小化、C++20、単一スレッド）
// 各ターンに最大 W=beam_width 件を保持し、1スイープで各ターンから
// 最大 K=sweep_width 件を展開する。残りの候補は次のスイープへ持ち越す。
// State のコピー・移動を O(S)、最大ターンを T とするとメモリは O(T W S)
// ターンキューは初回の有効候補で確保する。その初回確保は O(W S)
// ハッシュは同じ到着ターンの未展開候補だけを重複排除し、展開済み集合は持たない。
// is_answer は解候補の印。途中の is_answer 状態も expand されるので、必要ならそこで打ち切る。
// 最良有効解は幅・Hashの選別と独立して保存する。操作列はAction型指定時だけ共有木で保持する。
// pushのboolは探索候補の受理を表す。falseでも最良解を保存する場合がある。
// push_lazyのmakerは探索候補または最良解の保存に必要な場合だけ、1回呼ぶ。
// 時間制限は expand の途中では検査しない。Runtime の時刻は直近の検査値。

#ifndef CS_CHOKUDAI_COPY_CONCEPTS_V01
#define CS_CHOKUDAI_COPY_CONCEPTS_V01
namespace cs {
template<class T>
concept CostType = ((std::integral<T> && std::is_signed_v<T>) || std::floating_point<T>) &&
                   !std::same_as<T, bool>;
template<class T>
concept HashType = std::integral<T> && std::is_unsigned_v<T> && !std::same_as<T, bool> &&
                   sizeof(T) <= sizeof(std::uint64_t);
template<class H, class Hash>
concept CompatibleHashArg = HashType<Hash> && HashType<std::remove_cvref_t<H>> &&
    std::convertible_to<std::remove_cvref_t<H>, Hash> && sizeof(std::remove_cvref_t<H>) <= sizeof(Hash);
template<class B> concept BoolArg = std::same_as<std::remove_cvref_t<B>, bool>;
template<class S> concept StepArg = std::same_as<std::remove_cvref_t<S>, int>;
template<class U, class State> concept StateArg = std::same_as<std::remove_cvref_t<U>, State>;
} // namespace cs
#endif

namespace cs {
namespace single_detail {
#if __INCLUDE_LEVEL__ == 0
// 単体実行時だけの仮想時計。通常includeでは領域・分岐とも存在しない。
struct TestClock {
    static inline bool enabled = false;
    static inline std::int64_t value = 0;
    static inline std::uint64_t calls = 0;
};
#endif
// 単調時計の現在時刻を取得する、O(1)
inline std::int64_t now_us() {
#if __INCLUDE_LEVEL__ == 0
    ++TestClock::calls;
    if (TestClock::enabled) return TestClock::value;
#endif
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
// ミリ秒を飽和付きでマイクロ秒へ変換する、O(1)
inline std::int64_t to_us(double ms) {
    if (!(ms > 0.0)) return 0;
    const long double us = static_cast<long double>(ms) * 1000.0L;
    const auto upper = std::numeric_limits<std::int64_t>::max();
    if (us >= static_cast<long double>(upper) - 0.5L) return upper;
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(us + 0.5L));
}
// 整数ハッシュを拡散する、O(1)
inline std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
} // namespace single_detail

template<CostType Cost = long long, HashType Hash = std::uint64_t>
struct ChokudaiCopySingle {
    using cost_type = Cost;
    using hash_type = Hash;
private:
    template<class State> class Engine;
public:
    // 既存の Param の並びを維持し、chokudai 固有項目を末尾に追加
    struct Param {
        int max_turn = 0;            // 負なら 0
        int beam_width = 1;          // 各ターンの保持上限。1 以上へ丸める
        int hash_capacity = 0;       // 0 は自動。実際には W 基準で丸めて確保
        double time_limit_ms = 0.0;  // 0 以下・NaN は無制限、expand は中断しない
        int time_check_interval = 64;
        bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
        bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
        bool max_turn_is_answer = true;
        bool use_hash_dedup = true;
        int sweep_width = 1;      // 各ターン・各スイープの取り出し上限。1..beam_widthへ丸める
        std::int64_t max_sweeps = 0; // 0 以下は無制限
    };
    enum class EventType { RunStart, SweepStart, SweepEnd, RunEnd };
    enum class StopReason { None, TimeLimit, Exhausted, SweepLimit };
    template<class State>
    struct StateView {
        int turn = 0;
        Cost cost{};
        const State& state;
        Hash hash{};
        bool has_hash = false;
        // 現在状態の読み取り専用ビューを構築する、O(1)
        StateView(int t, Cost c, const State& s, Hash h, bool hh) noexcept
            : turn(t), cost(c), state(s), hash(h), has_hash(hh) {}
    };
    struct Runtime {
        Param param; // 探索に使用する設定。Engineとの二重保持を避ける。

        std::uint64_t terminal_candidates = 0; // 有効なmax_turn到着候補数。候補プールには入れない。
        std::uint64_t answer_only_candidates = 0; // expand_answers=falseで除外したmax_turn未満の有効候補数。
        std::int64_t sweep = 0;      // 1 始まり。スイープ未開始なら 0
        bool sweep_completed = false;
        std::int64_t elapsed_us = 0; // 最後に時計を検査した時点
        bool time_limit_reached = false;
        std::uint64_t expanded_nodes = 0; // 完了した expand。実行中の1件は含まない
        std::uint64_t generated_candidates = 0; // 初期状態を含まない
        std::uint64_t accepted_candidates = 0; // 探索候補の受理数。最良解だけの保存・maker呼び出し数とは別。
        std::uint64_t pruned_by_width = 0;
        std::uint64_t replaced_by_width = 0; // accepted の内数
        std::uint64_t pruned_by_hash = 0;
        std::uint64_t replaced_by_hash = 0;  // accepted の内数
        std::uint64_t invalid_candidates = 0;
        std::uint64_t stored_states = 0; // 終端キューを含む。展開中と best_state を除く
        std::uint64_t peak_stored_states = 0;
        bool found = false;
        Cost best_cost{};            // found=false のとき未定義の解値（初期値 0）
        int best_turn = 0;
        std::uint64_t best_update_count = 0;
        std::int64_t last_best_update_sweep = 0;
        StopReason stop_reason = StopReason::None;
    };
    struct NoOp {
        // イベントを無視する、O(1)
        void operator()(EventType, const Runtime&) const noexcept {}
    };
    // LOCAL定義時のみCSVを記録する。未定義時は同じ呼び出し方で使える空実装。
    struct CsvStatHook {
#ifdef LOCAL
        struct Row : Runtime { const char* event = ""; };
        std::string filename;
        std::int64_t interval_us = 0;
        std::vector<Row> rows;
        // CSV の保存先と SweepEnd の最小記録間隔を指定する、O(ファイル名長)
        explicit CsvStatHook(std::string path = "chokudai_stat.csv", double interval_ms = 50.0)
            : filename(std::move(path)), interval_us(single_detail::to_us(interval_ms)) {}
        // スナップショットを記録する、償却 O(1)、RunEnd は行数に比例する書き出し
        void operator()(EventType event, const Runtime& rt) {
            // 新しい run では前回の時刻・差分の基準を残さない
            if (event == EventType::RunStart) {
                rows.clear();
                started_ = true;
                append("RunStart", rt);
            } else if (event == EventType::SweepEnd && started_ &&
                       (interval_us == 0 || rt.elapsed_us - rows.back().elapsed_us >= interval_us)) {
                append("SweepEnd", rt);
            } else if (event == EventType::RunEnd && started_) {
                append("RunEnd", rt);
                write_csv();
                started_ = false;
            }
        }
    private:
        bool started_ = false;
        void append(const char* event, const Runtime& rt) { rows.push_back({rt, event}); }
        static const char* reason_name(StopReason reason) {
            switch (reason) {
                case StopReason::None: return "None";
                case StopReason::TimeLimit: return "TimeLimit";
                case StopReason::Exhausted: return "Exhausted";
                case StopReason::SweepLimit: return "SweepLimit";
            }
            return "None";
        }
        void write_csv() const {
            // 探索状態は再走査せず、記録行どうしの差分から期間統計を出力する
            std::ofstream out(filename, std::ios::binary | std::ios::trunc);
            if (!out) {
                std::cerr << "chokudai: CSV open failed: " << filename << '\n';
                return;
            }
            out.imbue(std::locale::classic()); // 小数点・桁区切りをCSVの区切り文字と混同させない
            out << std::setprecision(std::numeric_limits<long double>::max_digits10);
            out << "event,sweep,sweep_completed,elapsed_ms,time_limit_reached,expanded_nodes,generated_candidates,accepted_candidates,pruned_by_width,replaced_by_width,pruned_by_hash,replaced_by_hash,invalid_candidates,terminal_candidates,answer_only_candidates,continue_on_timeout,expand_answers,stored_states,peak_stored_states,found,best_cost,best_turn,best_update_count,last_best_update_sweep,stop_reason,period_ms,expand_per_sec,accept_rate,period_best_updates,sweeps_since_best_update\n";
            for (std::size_t i = 0; i < rows.size(); ++i) {
                const auto& r = rows[i];
                const auto& prev = rows[i == 0 ? 0 : i - 1];
                const auto dt = r.elapsed_us - prev.elapsed_us;
                const auto generated = r.generated_candidates - prev.generated_candidates;
                out << r.event << ',' << r.sweep << ',' << r.sweep_completed << ',' << (static_cast<double>(r.elapsed_us) / 1000.0) << ',' << r.time_limit_reached << ','
                    << r.expanded_nodes << ',' << r.generated_candidates << ',' << r.accepted_candidates << ','
                    << r.pruned_by_width << ',' << r.replaced_by_width << ',' << r.pruned_by_hash << ',' << r.replaced_by_hash << ','
                    << r.invalid_candidates << ',' << r.terminal_candidates << ',' << r.answer_only_candidates << ',' << r.param.continue_on_timeout << ',' << r.param.expand_answers << ',' << r.stored_states << ',' << r.peak_stored_states << ',' << r.found << ',';
                if (r.found) out << +r.best_cost; // char系のCostも文字ではなく数値として出力
                out << ',';
                if (r.found) out << r.best_turn;
                out << ',' << r.best_update_count << ',';
                if (r.found) out << r.last_best_update_sweep;
                out << ',' << reason_name(r.stop_reason) << ',' << (static_cast<double>(dt) / 1000.0) << ',';
                if (dt > 0) out << static_cast<double>(r.expanded_nodes - prev.expanded_nodes) * 1e6 / static_cast<double>(dt);
                out << ',';
                if (generated > 0) out << static_cast<double>(r.accepted_candidates - prev.accepted_candidates) / static_cast<double>(generated);
                out << ',' << (r.best_update_count - prev.best_update_count) << ',';
                if (r.found) out << r.sweep - r.last_best_update_sweep;
                out << '\n';
            }
            out.close();
            if (!out) std::cerr << "chokudai: CSV write failed: " << filename << '\n';
        }
#else
        explicit CsvStatHook(std::string = "chokudai_stat.csv", double = 50.0) {}
        void operator()(EventType, const Runtime&) {}
#endif
    };
    template<class State, class Action = void> struct Result;
    template<class State> struct Result<State, void> {
        bool found = false;
        bool time_limit_reached = false;
        bool exhausted = false;
        bool completed_max_turn = false; // 不正でない最大ターン候補が1件でも生成された
        Cost best_cost{};
        int best_turn = 0;
        std::optional<State> best_state;
        Runtime runtime;

        bool sweep_limit_reached = false;
    };
    template<class State, class Action = void> class Emitter;
    template<class State> class Emitter<State, void> {
    public:
    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, cost, now_hash_, false, false, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class B>
    requires StateArg<U, State> && BoolArg<B>
    bool push(U&& state, Cost cost, B is_answer) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, cost, now_hash_, false, is_answer, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class H>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, cost, static_cast<Hash>(hash), true, false, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class H, class B>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push(U&& state, Cost cost, H hash, B is_answer) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, cost, static_cast<Hash>(hash), true, is_answer, maker);
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker>
    bool push_lazy(Cost cost, Maker&& maker) {
        return engine_.push_candidate(turn_, cost, now_hash_, false, false, std::forward<Maker>(maker));
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class B>
    requires BoolArg<B>
    bool push_lazy(Cost cost, B is_answer, Maker&& maker) {
        return engine_.push_candidate(turn_, cost, now_hash_, false, is_answer, std::forward<Maker>(maker));
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class H>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, Maker&& maker) {
        return engine_.push_candidate(turn_, cost, static_cast<Hash>(hash), true, false, std::forward<Maker>(maker));
    }

    // 探索候補または最良解の保存時だけ状態を生成する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class H, class B>
    requires CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push_lazy(Cost cost, H hash, B is_answer, Maker&& maker) {
        return engine_.push_candidate(turn_, cost, static_cast<Hash>(hash), true, is_answer, std::forward<Maker>(maker));
    }

    private:
        friend class Engine<State>;
        Emitter(Engine<State>& e, int t, Hash h) noexcept : engine_(e), turn_(t), now_hash_(h) {}
        Engine<State>& engine_;
        int turn_;
        Hash now_hash_;
    };

public:
    template<class Action> class HistoryView {
        template<class, class> friend class Emitter;
        using Pool = beam_history_detail::Pool<Action>;
        const Pool* pool_;
        std::size_t id_;
        HistoryView(const Pool& pool, std::size_t id) noexcept : pool_(&pool), id_(id) {}
    public:
        // expand内だけ有効。Action参照はpush/push_lazyをまたいで保持しない。
        bool empty() const noexcept { return id_ == Pool::none; }
        const Action& action() const noexcept { assert(!empty()); return pool_->value(id_); }
        HistoryView parent() const noexcept { assert(!empty()); return {*pool_, pool_->parent(id_)}; }
    };
    template<class State, class Action>
    struct Result : Result<State, void> {
        std::vector<Action> path;
    };
    // Action型指定時だけ有効。Stateと操作列は分離し、祖先Stateは保持しない。
    template<class State, class Action>
    class Emitter {
        friend struct ChokudaiCopySingle;
        using Value = Action;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Emitter<Stored>& raw_;
        Pool& pool_;
        std::size_t parent_;
        Emitter(Emitter<Stored>& raw, Pool& pool, std::size_t parent) noexcept
            : raw_(raw), pool_(pool), parent_(parent) {}
        template<class A, class Maker>
        bool emit(A&& action, Cost cost, Hash hash, bool has_hash, bool is_answer, Maker&& maker) {
            auto traced = [&]() {
                return Stored{State(std::invoke(std::forward<Maker>(maker))), pool_.make(parent_, std::forward<A>(action))};
            };
            if (has_hash) return raw_.push_lazy(cost, hash, is_answer, traced);
            return raw_.push_lazy(cost, is_answer, traced);
        }
    public:
        HistoryView<Action> history() const noexcept { return {pool_, parent_}; }
        template<class U, class A>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action>
        bool push(U&& state, A&& action, Cost cost) {
            return emit(std::forward<A>(action), cost, Hash{}, false, false, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&>
        bool push_lazy(A&& action, Cost cost, Maker&& maker) {
            return emit(std::forward<A>(action), cost, Hash{}, false, false, std::forward<Maker>(maker));
        }
        template<class U, class A, class B>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && BoolArg<B>
        bool push(U&& state, A&& action, Cost cost, B is_answer) {
            return emit(std::forward<A>(action), cost, Hash{}, false, is_answer, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class B, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && BoolArg<B>
        bool push_lazy(A&& action, Cost cost, B is_answer, Maker&& maker) {
            return emit(std::forward<A>(action), cost, Hash{}, false, is_answer, std::forward<Maker>(maker));
        }
        template<class U, class A, class H>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && CompatibleHashArg<H, Hash>
        bool push(U&& state, A&& action, Cost cost, H hash) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, false, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class H, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && CompatibleHashArg<H, Hash>
        bool push_lazy(A&& action, Cost cost, H hash, Maker&& maker) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, false, std::forward<Maker>(maker));
        }
        template<class U, class A, class H, class B>
        requires StateArg<U, State> && std::same_as<std::remove_cvref_t<A>, Action> && CompatibleHashArg<H, Hash> && BoolArg<B>
        bool push(U&& state, A&& action, Cost cost, H hash, B is_answer) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, is_answer, [&]() -> decltype(auto) { return std::forward<U>(state); });
        }
        template<class A, class H, class B, class Maker>
        requires std::same_as<std::remove_cvref_t<A>, Action> && std::invocable<Maker&&> && CompatibleHashArg<H, Hash> && BoolArg<B>
        bool push_lazy(A&& action, Cost cost, H hash, B is_answer, Maker&& maker) {
            return emit(std::forward<A>(action), cost, static_cast<Hash>(hash), true, is_answer, std::forward<Maker>(maker));
        }
    };

private:
    template<class State>
    class Engine {
        static_assert(std::copy_constructible<State>, "State must be copy constructible");
        static_assert(std::assignable_from<State&, State>, "State must be assignable");
        struct Key {
            Cost cost{};
            Hash hash{};
            // 使用中は受理順、空きスロットでは次の空き位置を格納する。
            union { std::uint64_t serial = 0; int next_free; };
            bool has_hash = false;
        };
        struct Ends { int best = -1; int worst = -1; };
        // 状態本体と比較用情報を分け、最良・最悪の両端を同じ木で管理する
        class Queue {
        public:
            std::vector<std::optional<State>> items;
            std::vector<Key> keys;
            int free_head = -1;
            std::vector<Ends> tree;
            std::vector<int> table; // スロット番号。-1=空
            std::size_t base;
            std::size_t hash_size = 0;
            int size = 0;
            // 空のターンキューを確保する、O(W)
            Queue(int width, std::size_t capacity)
                : base(std::bit_ceil(static_cast<std::size_t>(width))), hash_size(capacity) {
                keys.reserve(static_cast<std::size_t>(width));
                items.reserve(static_cast<std::size_t>(width));
                // 葉の情報は items から読み、木には内部ノードだけを置く。
                // State の領域は予約し、実際に使うスロットから構築する。
                tree.resize(base);
            }
            int free_slot() const { return free_head < 0 ? static_cast<int>(items.size()) : free_head; }
            // 非空キューの最良スロットを取得する、O(1)
            int best() const { return base == 1 ? 0 : tree[1].best; }
            // 非空キューの最悪スロットを取得する、O(1)
            int worst() const { return base == 1 ? 0 : tree[1].worst; }
            // キーに対応する現在保持中のスロットを探す、期待 O(1)
            int find(Hash hash) const {
                if (table.empty()) return -1;
                std::size_t pos = bucket(hash);
                while (table[pos] != -1) {
                    if (keys[static_cast<std::size_t>(table[pos])].hash == hash) return table[pos];
                    pos = (pos + 1) & (table.size() - 1);
                }
                return -1;
            }
            // 指定スロットを取り除く、期待 O(log W + S)
            void erase(int slot, bool updating = true) {
                // 線形探索の穴を詰め、削除マークと定期再構築を不要にする
                const auto id = static_cast<std::size_t>(slot);
                if (keys[id].has_hash && !table.empty()) {
                    std::size_t pos = bucket(keys[id].hash);
                    while (table[pos] != slot) pos = (pos + 1) & (table.size() - 1);
                    const auto mask = table.size() - 1;
                    std::size_t next = (pos + 1) & mask;
                    while (table[next] >= 0) {
                        const auto home = bucket(keys[static_cast<std::size_t>(table[next])].hash);
                        if (((pos - home) & mask) < ((next - home) & mask)) {
                            table[pos] = table[next];
                            pos = next;
                        }
                        next = (next + 1) & mask;
                    }
                    table[pos] = -1;
                }
                items[id].reset();
                --size;
                keys[id].next_free = free_head;
                free_head = slot;
                if (updating) update(slot);
            }
            // 検査済みの候補を指定スロットへ格納する、期待償却 O(log W + S)
            template<class Maker>
            void insert(int slot, Cost cost, Hash hash, bool hashed, std::uint64_t serial, Maker&& maker) {
                // maker の例外が出た場合には古い候補をまだ取り除いていない
                State made(std::invoke(std::forward<Maker>(maker)));
                const auto id = static_cast<std::size_t>(slot);
                if (id == items.size()) { items.emplace_back(); keys.emplace_back(); }
                if (items[id]) erase(slot, false);
                if (free_head >= 0) free_head = keys[id].next_free;
                items[id].emplace(std::move(made));
                keys[id] = Key{cost, hash, serial, hashed};
                ++size;
                update(slot);
                if (hashed && hash_size > 0) {
                    if (table.empty()) table.assign(hash_size, -1);
                    std::size_t pos = bucket(hash);
                    while (table[pos] >= 0) pos = (pos + 1) & (table.size() - 1);
                    table[pos] = slot;
                }
            }
        private:
            bool before(int a, int b) const {
                const Key& x = keys[static_cast<std::size_t>(a)];
                const Key& y = keys[static_cast<std::size_t>(b)];
                if (x.cost != y.cost) return x.cost < y.cost;
                return x.serial < y.serial;
            }
            void update(int slot) {
                // 空の子を除外して、同点なら受理順が早いものを best にする
                std::size_t pos = base + static_cast<std::size_t>(slot);
                for (pos /= 2; pos > 0; pos /= 2) {
                    auto child = [&](std::size_t i) {
                        if (i < base) return tree[i];
                        const auto j = i - base;
                        return j < items.size() && items[j] ? Ends{static_cast<int>(j), static_cast<int>(j)} : Ends{};
                    };
                    const Ends a = child(pos * 2), b = child(pos * 2 + 1);
                    if (a.best < 0) tree[pos] = b;
                    else if (b.best < 0) tree[pos] = a;
                    else tree[pos] = Ends{before(a.best, b.best) ? a.best : b.best,
                                          before(a.worst, b.worst) ? b.worst : a.worst};
                }
            }
            std::size_t bucket(Hash hash) const {
                return static_cast<std::size_t>(single_detail::mix(static_cast<std::uint64_t>(hash))) & (table.size() - 1);
            }
        };
        Runtime rt_;
        std::optional<State> best_state_;
        std::vector<std::unique_ptr<Queue>> layers_;
        std::unique_ptr<Queue> spare_; // 空になったキューの領域を次のターンへ再利用する。
        std::uint64_t pending_ = 0;
        std::uint64_t serial_ = 0;
        std::size_t hash_size_ = 0;
        std::int64_t start_us_ = 0;
        std::int64_t limit_us_ = 0;
        int unchecked_ = 0;
        bool completed_max_turn_ = false;
    public:
        // 初期状態とキューを準備する、O(T + W S)
        Engine(const Param& param, State&& initial, Cost cost, Hash hash, bool hashed) : rt_{param} {
            start_us_ = single_detail::now_us();
            // サイズ計算は符号付き int の加算・乗算を避ける

            rt_.param.max_turn = std::max(0, rt_.param.max_turn);
            rt_.param.beam_width = std::max(1, rt_.param.beam_width);
            rt_.param.sweep_width = std::clamp(rt_.param.sweep_width, 1, rt_.param.beam_width);
            rt_.param.time_check_interval = std::max(1, rt_.param.time_check_interval);
            limit_us_ = single_detail::to_us(rt_.param.time_limit_ms);
            if (rt_.param.use_hash_dedup) {
                const auto w = static_cast<std::uint64_t>(rt_.param.beam_width);
                // 削除時に穴を詰めるため、使用率を1/2以下にすれば空きバケットを保てる。
                const auto requested = rt_.param.hash_capacity <= 0 ? 2 * w : static_cast<std::uint64_t>(rt_.param.hash_capacity);
                const auto capacity = std::clamp(requested, 2 * w, 4 * w + 17);
                hash_size_ = std::bit_ceil(capacity);
            }
            layers_.resize(static_cast<std::size_t>(rt_.param.max_turn) + 1);
            // NaN の初期コストでは探索を開始しない（push の候補数には含めない）
            if (valid_cost(cost)) {
                completed_max_turn_ = rt_.param.max_turn == 0;
                Queue& q = layer(0);
                q.insert(0, cost, hash, hashed, serial_++, [&]() -> State&& { return std::move(initial); });
                rt_.stored_states = rt_.peak_stored_states = 1;
                pending_ = rt_.param.max_turn > 0 ? 1 : 0;
                if (rt_.param.max_turn == 0 && rt_.param.max_turn_is_answer) save_best(std::move(*q.items[0]), cost, 0);
            }
        }
        // 候補を検査し、探索候補または最良解の保存に必要なら状態を生成する、期待償却 O(log W + S)
        template<class Maker>
        bool push_candidate(int from, Cost cost, Hash hash, bool has_hash, bool is_answer, Maker&& maker) {
            static_assert(std::invocable<Maker&&>, "maker must be callable without arguments");
            static_assert(std::constructible_from<State, std::invoke_result_t<Maker&&>>, "maker must produce State");
            ++rt_.generated_candidates;
            // 展開元は必ずmax_turn未満。singleの遷移は1ターン固定
            if (!valid_cost(cost)) {
                ++rt_.invalid_candidates;
                return false;
            }
            const int turn = from + 1;
            const bool answer = is_answer || (rt_.param.max_turn_is_answer && turn == rt_.param.max_turn);
            // 棄却する候補でも、改善する有効解なら保存する。makerは受理側と合わせて高々1回。
            auto save_answer = [&]() {
                if (!rt_.found || cost < rt_.best_cost)
                    save_best(std::invoke(std::forward<Maker>(maker)), cost, turn);
            };
            if (turn == rt_.param.max_turn || (!rt_.param.expand_answers && is_answer)) {
                if (turn == rt_.param.max_turn) { completed_max_turn_ = true; ++rt_.terminal_candidates; }
                else ++rt_.answer_only_candidates;
                if (answer) save_answer();
                return false; // 最良解だけを保存し、キューを作らない。
            }
            Queue& q = layer(turn);
            const bool full = q.size == rt_.param.beam_width;
            // 満杯時は幅による棄却を先に判定する。同点は既存候補を優先する
            if (full && !(cost < q.keys[static_cast<std::size_t>(q.worst())].cost)) {
                ++rt_.pruned_by_width;
                if (answer) save_answer();
                return false;
            }
            int slot = has_hash ? q.find(hash) : -1;
            const bool duplicate = slot >= 0;
            if (duplicate && !(cost < q.keys[static_cast<std::size_t>(slot)].cost)) {
                ++rt_.pruned_by_hash;
                if (answer) save_answer();
                return false;
            }
            if (!duplicate) slot = full ? q.worst() : q.free_slot();
            q.insert(slot, cost, hash, has_hash, serial_++, std::forward<Maker>(maker));
            ++rt_.accepted_candidates;
            if (duplicate) ++rt_.replaced_by_hash;
            else if (full) ++rt_.replaced_by_width;
            else {
                ++rt_.stored_states;
                ++pending_;
                rt_.peak_stored_states = std::max(rt_.peak_stored_states, rt_.stored_states);
            }
            if (answer) save_best(*q.items[static_cast<std::size_t>(slot)], cost, turn);
            return true;
        }
        // 探索し、最良状態を返す、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
        template<class Expand, class Hook>
        Result<State> run(Expand&& expand, Hook&& hook) {
            sample_time();
            hook(EventType::RunStart, static_cast<const Runtime&>(rt_));
            sample_time();
            rt_.stop_reason = reason();
            // スイープの境界でも時間を検査するので、空のターンが多くても制限を検出する
            while (rt_.stop_reason == StopReason::None) do_sweep(expand, hook, false);
            while (rt_.time_limit_reached && rt_.param.continue_on_timeout && !rt_.found && pending_ > 0) {
                do_sweep(expand, hook, true);
            }
            hook(EventType::RunEnd, static_cast<const Runtime&>(rt_));
            // キューとは独立した best_state を移動して、engine の破棄後も有効にする
            Result<State> result;
            result.found = rt_.found;
            result.time_limit_reached = rt_.time_limit_reached;
            result.exhausted = rt_.stop_reason == StopReason::Exhausted;
            result.sweep_limit_reached = rt_.stop_reason == StopReason::SweepLimit;
            result.completed_max_turn = completed_max_turn_;
            result.best_cost = rt_.best_cost;
            result.best_turn = rt_.best_turn;
            result.best_state = std::move(best_state_);
            result.runtime = rt_;
            return result;
        }
    private:
        static bool valid_cost(Cost cost) {
            if constexpr (std::floating_point<Cost>) return !std::isnan(cost);
            else { (void)cost; return true; }
        }
        void initialize_layer(std::unique_ptr<Queue>& q) {
            q = spare_ ? std::move(spare_) : std::make_unique<Queue>(rt_.param.beam_width, hash_size_);
        }
        Queue& layer(int turn) {
            auto& q = layers_[static_cast<std::size_t>(turn)];
            if (!q) initialize_layer(q);
            return *q;
        }
        template<class U>
        void save_best(U&& state, Cost cost, int turn) {
            // キューとは独立した保存。受理したStateはコピーし、棄却時の生成結果は直接構築する。
            if (!rt_.found || cost < rt_.best_cost) {
                best_state_.emplace(std::forward<U>(state));
                rt_.found = true;
                rt_.best_cost = cost;
                rt_.best_turn = turn;
                ++rt_.best_update_count;
                rt_.last_best_update_sweep = rt_.sweep;
            }
        }
        void sample_time() {
            rt_.elapsed_us = single_detail::now_us() - start_us_;
            unchecked_ = 0;
            if (limit_us_ > 0 && rt_.elapsed_us >= limit_us_) rt_.time_limit_reached = true;
        }
        StopReason reason() const {
            if (rt_.time_limit_reached) return StopReason::TimeLimit;
            if (pending_ == 0) return StopReason::Exhausted;
            if (rt_.param.max_sweeps > 0 && rt_.sweep >= rt_.param.max_sweeps) return StopReason::SweepLimit;
            return StopReason::None;
        }
        bool remaining_in_sweep(int turn, int used, int width) const {
            if (used < width && layers_[static_cast<std::size_t>(turn)]->size > 0) return true;
            for (int t = turn + 1; t < rt_.param.max_turn; ++t) {
                const auto& q = layers_[static_cast<std::size_t>(t)];
                if (q && q->size > 0) return true;
            }
            return false;
        }
        template<class Expand, class Hook>
        void do_sweep(Expand& expand, Hook& hook, bool fallback) {
            ++rt_.sweep;
            rt_.sweep_completed = false;
            hook(EventType::SweepStart, static_cast<const Runtime&>(rt_));
            // SweepStartのHook時間も計上し、時間切れなら通常の展開を始めない。
            if (!fallback && limit_us_ > 0) sample_time();
            const int width = fallback ? 1 : rt_.param.sweep_width;
            bool stopped = !fallback && rt_.time_limit_reached;
            // 空の中間ターンを飛ばし、同じスイープ中に生成した未来の候補も展開する
            for (int turn = 0; turn < rt_.param.max_turn && !stopped; ++turn) {
                auto& ptr = layers_[static_cast<std::size_t>(turn)];
                if (!ptr) continue;
                Queue& q = *ptr;
                for (int k = 0; k < width && q.size > 0; ++k) {
                    const int slot = q.best();
                    const Key key = q.keys[static_cast<std::size_t>(slot)];
                    State item = std::move(*q.items[static_cast<std::size_t>(slot)]);
                    q.erase(slot);
                    --rt_.stored_states;
                    --pending_;
                    const StateView<State> now(turn, key.cost, item, key.hash, key.has_hash);
                    Emitter<State> emit(*this, turn, key.hash);
                    // Runtime ありの形式を優先し、利用するだけでは時計を追加で読まない
                    if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
                        expand(now, static_cast<const Runtime&>(rt_), emit);
                    } else {
                        static_assert(std::is_invocable_v<Expand&, const StateView<State>&, Emitter<State>&>, "invalid expand signature; Runtime must be const");
                        expand(now, emit);
                    }
                    ++rt_.expanded_nodes;
                    if (!fallback && limit_us_ > 0 && ++unchecked_ >= rt_.param.time_check_interval) sample_time();
                    if ((!fallback && rt_.time_limit_reached) || (fallback && rt_.found)) {
                        rt_.sweep_completed = !remaining_in_sweep(turn, k + 1, width);
                        stopped = true;
                        break;
                    }
                }
                if (q.size == 0) spare_ = std::move(ptr);
            }
            if (!stopped) rt_.sweep_completed = true;
            sample_time();
            rt_.stop_reason = reason();
            hook(EventType::SweepEnd, static_cast<const Runtime&>(rt_));
            // Hook 自身にかかった時間も、次のスイープ開始前に検査する
            sample_time();
            rt_.stop_reason = reason();
        }
    };
private:
    template<class State, class Action, class Expand> struct HistoryExpand {
        using Value = Action;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Pool& pool;
        Expand& expand;
        template<class... Context>
        void invoke(const StateView<Stored>& now, Emitter<Stored>& raw, Context&&... context) {
            const auto& stored = now.state;
            StateView<State> view{now.turn, now.cost, stored.state, now.hash, now.has_hash};
            Emitter<State, Action> emit(raw, pool, stored.trace.id());
            std::invoke(expand, view, std::forward<Context>(context)..., emit);
        }
        void operator()(const StateView<Stored>& now, Emitter<Stored>& raw)
            requires std::invocable<Expand&, const StateView<State>&, Emitter<State, Action>&>
            { invoke(now, raw); }
        void operator()(const StateView<Stored>& now, const Runtime& rt, Emitter<Stored>& raw)
            requires std::invocable<Expand&, const StateView<State>&, const Runtime&, Emitter<State, Action>&>
            { invoke(now, raw, rt); }
    };
    template<class State, class Action, class Expand, class DebugHook>
    static Result<State, Action> run_history(const Param& param, State initial_state, Cost initial_cost,
                                            Hash hash, bool has_hash, Expand& expand, DebugHook& hook) {
        static_assert(std::copy_constructible<Action>, "Action must be copy constructible for result.path");
        using Value = Action;
        using Pool = beam_history_detail::Pool<Value>;
        using Stored = beam_history_detail::TracedState<State, Value>;
        Pool pool;
        HistoryExpand<State, Action, Expand> bridge{pool, expand};
        Engine<Stored> engine(param, Stored{std::move(initial_state), {}}, initial_cost, hash, has_hash);
        auto raw = engine.run(bridge, hook);
        Result<State, Action> result;
        result.found = raw.found;
        result.time_limit_reached = raw.time_limit_reached;
        result.exhausted = raw.exhausted;
        result.completed_max_turn = raw.completed_max_turn;
        result.best_cost = raw.best_cost;
        result.best_turn = raw.best_turn;
        result.sweep_limit_reached = raw.sweep_limit_reached;
        result.runtime = raw.runtime;
        if (raw.best_state) {
            result.best_state.emplace(std::move(raw.best_state->state));
            std::vector<std::size_t> reverse;
            for (auto id = raw.best_state->trace.id(); id != Pool::none; id = pool.parent(id)) reverse.push_back(id);
            result.path.reserve(reverse.size());
            for (auto i = reverse.rbegin(); i != reverse.rend(); ++i) result.path.push_back(pool.value(*i));
        }
        return result;
    }
public:
    // ハッシュありで探索する、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
    template<class State, class Action = void, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State, Action> run(const Param& param, State initial_state, Cost initial_cost,
                             H initial_hash, Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        if constexpr (std::is_void_v<Action>) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));

        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true, expand, debug_hook);
        }
    }
    // ハッシュなしで探索する、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
    template<class State, class Action = void, class Expand, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<State, Action> run(const Param& param, State initial_state, Cost initial_cost,
                             Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        if constexpr (std::is_void_v<Action>) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, Hash{}, false);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));

        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, Hash{}, false, expand, debug_hook);
        }
    }
};
} // namespace cs

#if __INCLUDE_LEVEL__ == 0
// このファイルを直接コンパイルした場合だけテストを実行する
namespace {
using BS = cs::ChokudaiCopySingle<>;
using Clock = cs::single_detail::TestClock;
constexpr bool is_multi = false;
std::uint64_t checks = 0;
void check(bool ok, const char* expr, int line) {
    ++checks;
    if (!ok) {
        std::cerr << "FAIL single:" << line << " " << expr << '\n';
        std::abort();
    }
}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __LINE__)

template<class B> void max_step(typename B::Param& p, int step) {
    if constexpr (requires { p.max_step; }) p.max_step = step;
    else (void)step;
}
template<class B, class U>
bool push(typename B::template Emitter<std::remove_cvref_t<U>>& e, U&& state,
          typename B::cost_type cost, int step = 1, bool hashed = false,
          typename B::hash_type hash = {}, bool is_answer = false) {
    if constexpr (requires (typename B::Param p) { p.max_step; }) {
        if (hashed) return e.push(std::forward<U>(state), cost, hash, step, is_answer);
        return e.push(std::forward<U>(state), cost, step, is_answer);
    } else {
        (void)step;
        if (hashed) return e.push(std::forward<U>(state), cost, hash, is_answer);
        return e.push(std::forward<U>(state), cost, is_answer);
    }
}
template<class B, class S, class Maker>
bool lazy(typename B::template Emitter<S>& e, typename B::cost_type cost, int step,
          bool hashed, typename B::hash_type hash, bool is_answer, Maker&& maker) {
    if constexpr (requires (typename B::Param p) { p.max_step; }) {
        if (hashed) return e.push_lazy(cost, hash, step, is_answer, std::forward<Maker>(maker));
        return e.push_lazy(cost, step, is_answer, std::forward<Maker>(maker));
    } else {
        (void)step;
        if (hashed) return e.push_lazy(cost, hash, is_answer, std::forward<Maker>(maker));
        return e.push_lazy(cost, is_answer, std::forward<Maker>(maker));
    }
}
struct State {
    int node = 0;
    std::uint64_t path = 0;
    bool operator==(const State&) const = default;
};
struct Edge {
    int node;
    int step;
    long long cost;
    std::uint64_t hash;
    bool hashed;
    bool is_answer;
};
using Graph = std::vector<std::vector<Edge>>;
using Trace = std::vector<std::tuple<int, int, std::uint64_t, long long, std::uint64_t, bool>>;
using PushTrace = std::vector<bool>;
using Events = std::vector<std::pair<BS::EventType, BS::Runtime>>;
std::uint64_t mix(std::uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
State child(const State& parent, int edge, int node) {
    return {node, mix(parent.path + static_cast<std::uint64_t>(edge) + 0x9e3779b97f4a7c15ULL)};
}
auto counters(const BS::Runtime& r) {
    return std::tuple(r.sweep, r.sweep_completed, r.time_limit_reached, r.expanded_nodes,
        r.generated_candidates, r.accepted_candidates, r.pruned_by_width, r.replaced_by_width,
        r.pruned_by_hash, r.replaced_by_hash, r.invalid_candidates, r.terminal_candidates, r.stored_states,
        r.peak_stored_states, r.found, r.best_cost, r.best_turn, r.best_update_count,
        r.last_best_update_sweep, r.stop_reason);
}
void invariant(const BS::Runtime& r) {
    CHECK(r.generated_candidates == r.accepted_candidates + r.pruned_by_width + r.pruned_by_hash + r.invalid_candidates + r.terminal_candidates);
    CHECK(r.replaced_by_width + r.replaced_by_hash <= r.accepted_candidates);
    CHECK(r.stored_states <= r.peak_stored_states);
}

// vector と線形走査だけで候補管理する独立した参照実装
struct Reference {
    struct Item { State state; long long cost; std::uint64_t hash; bool hashed; std::uint64_t serial; };
    BS::Param p;
    BS::Runtime r;
    std::vector<std::vector<Item>> queues;
    std::optional<State> best;
    bool completed = false;
    std::uint64_t serial = 1;
    Trace trace;
    PushTrace pushed;
    Events events;
    explicit Reference(BS::Param param) : p(param), queues(static_cast<std::size_t>(std::max(0, p.max_turn)) + 1) {
        p.max_turn = std::max(0, p.max_turn);
        p.beam_width = std::max(1, p.beam_width);
        p.sweep_width = std::clamp(p.sweep_width, 1, p.beam_width);
        queues[0].push_back({{}, 0, 0, true, 0});
        r.stored_states = r.peak_stored_states = 1;
        if (p.max_turn == 0) {
            completed = true;
            if (p.max_turn_is_answer) save(queues[0][0], 0);
        }
    }
    static bool less(const Item& a, const Item& b) {
        return std::tie(a.cost, a.serial) < std::tie(b.cost, b.serial);
    }
    void save(const Item& a, int turn) {
        if (!r.found || a.cost < r.best_cost) {
            r.found = true; r.best_cost = a.cost; r.best_turn = turn; best = a.state;
            ++r.best_update_count; r.last_best_update_sweep = r.sweep;
        }
    }
    bool offer(const State& state, int turn, const Edge& e) {
        ++r.generated_candidates;
        int d = 1;
        if (e.step <= 0 || e.step > d || e.step > p.max_turn - turn) {
            ++r.invalid_candidates; return false;
        }
        const int t = turn + e.step;
        if (e.is_answer || (p.max_turn_is_answer && t == p.max_turn))
            save(Item{state, e.cost, e.hash, e.hashed, 0}, t);
        if (t == p.max_turn) { completed=true; ++r.terminal_candidates; return false; }
        auto& q = queues[static_cast<std::size_t>(t)];
        auto worst = std::max_element(q.begin(), q.end(), less);
        const bool full = q.size() == static_cast<std::size_t>(p.beam_width);
        if (full && e.cost >= worst->cost) { ++r.pruned_by_width; return false; }
        auto duplicate = q.end();
        if (e.hashed && p.use_hash_dedup) {
            duplicate = std::find_if(q.begin(), q.end(), [&](const Item& a) { return a.hashed && a.hash == e.hash; });
        }
        if (duplicate != q.end() && e.cost >= duplicate->cost) { ++r.pruned_by_hash; return false; }
        const Item candidate{state, e.cost, e.hash, e.hashed, serial++};
        if (duplicate != q.end()) { *duplicate = candidate; ++r.replaced_by_hash; }
        else if (full) { *worst = candidate; ++r.replaced_by_width; }
        else { q.push_back(candidate); ++r.stored_states; r.peak_stored_states = std::max(r.stored_states, r.peak_stored_states); }
        ++r.accepted_candidates;
        return true;
    }
    void event(BS::EventType type) { events.emplace_back(type, r); }
    BS::StopReason reason() const {
        bool empty = true;
        for (int t = 0; t < p.max_turn; ++t) empty = empty && queues[static_cast<std::size_t>(t)].empty();
        if (empty) return BS::StopReason::Exhausted;
        if (p.max_sweeps > 0 && r.sweep >= p.max_sweeps) return BS::StopReason::SweepLimit;
        return BS::StopReason::None;
    }
    void run(const Graph& graph) {
        event(BS::EventType::RunStart);
        r.stop_reason = reason();
        while (r.stop_reason == BS::StopReason::None) {
            ++r.sweep; r.sweep_completed = false; event(BS::EventType::SweepStart);
            for (int t = 0; t < p.max_turn; ++t) {
                auto& q = queues[static_cast<std::size_t>(t)];
                for (int k = 0; k < p.sweep_width && !q.empty(); ++k) {
                    const auto it = std::min_element(q.begin(), q.end(), less);
                    const Item a = *it; q.erase(it); --r.stored_states;
                    trace.emplace_back(t, a.state.node, a.state.path, a.cost, a.hash, a.hashed);
                    const auto& edges = graph[static_cast<std::size_t>(a.state.node)];
                    for (std::size_t i = 0; i < edges.size(); ++i) {
                        auto e = edges[i];
                        if (!e.hashed) e.hash = a.hash;
                        pushed.push_back(offer(child(a.state, static_cast<int>(i), e.node), t, e));
                    }
                    ++r.expanded_nodes;
                }
            }
            r.sweep_completed = true; r.stop_reason = reason(); event(BS::EventType::SweepEnd);
        }
        event(BS::EventType::RunEnd);
    }
};

BS::Result<State> compare(const Graph& graph, const BS::Param& param, bool use_lazy = true) {
    Reference ref(param); ref.run(graph);
    Trace trace; PushTrace pushed; Events events;
    std::uint64_t made = 0, needed = 0;
    const auto result = BS::run(param, State{}, 0LL, std::uint64_t{0},
        [&](const BS::StateView<State>& now, const BS::Runtime& rt, BS::Emitter<State>& e) {
            invariant(rt);
            trace.emplace_back(now.turn, now.state.node, now.state.path, now.cost, now.hash, now.has_hash);
            const auto& edges = graph[static_cast<std::size_t>(now.state.node)];
            for (std::size_t i = 0; i < edges.size(); ++i) {
                const auto& edge = edges[i];
                const int bound = [](const auto& config) { if constexpr (requires { config.max_step; }) return config.max_step; else return 1; }(param);
                const bool valid = edge.step > 0 && edge.step <= std::clamp(bound, 1, std::max(1, param.max_turn)) && edge.step <= param.max_turn - now.turn;
                const bool improves = valid && (edge.is_answer || (param.max_turn_is_answer && now.turn + edge.step == param.max_turn)) &&
                    (!rt.found || edge.cost < rt.best_cost);
                bool accepted;
                if (use_lazy) accepted = lazy<BS, State>(e, edge.cost, edge.step, edge.hashed, edge.hash, edge.is_answer, [&]() {
                    ++made; return child(now.state, static_cast<int>(i), edge.node);
                });
                else accepted = push<BS>(e, child(now.state, static_cast<int>(i), edge.node), edge.cost, edge.step, edge.hashed, edge.hash, edge.is_answer);
                needed += accepted || improves;
                pushed.push_back(accepted);
                invariant(rt);
            }
        }, [&](BS::EventType event, const BS::Runtime& r) { invariant(r); events.emplace_back(event, r); });
    CHECK(trace == ref.trace);
    CHECK(pushed == ref.pushed);
    CHECK(counters(result.runtime) == counters(ref.r));
    CHECK(result.best_state == ref.best);
    CHECK(result.runtime.peak_stored_states <= static_cast<std::uint64_t>(std::max(1,param.beam_width)) * (static_cast<std::uint64_t>(std::max(0,param.max_turn)) + 1));
    CHECK(result.completed_max_turn == ref.completed);
    CHECK(result.exhausted == (ref.r.stop_reason == BS::StopReason::Exhausted));
    CHECK(result.sweep_limit_reached == (ref.r.stop_reason == BS::StopReason::SweepLimit));
    CHECK(events.size() == ref.events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        CHECK(events[i].first == ref.events[i].first);
        CHECK(counters(events[i].second) == counters(ref.events[i].second));
    }
    if (use_lazy) CHECK(made == needed);
    return result;
}
void deterministic() {
    Clock::enabled = true; Clock::value = 0;
    BS::Param p;
    p.max_turn = 2; p.beam_width = 2; p.max_sweeps = 1;
    Graph graph(5);
    graph[0] = {{1,1,1,1,false,false},{2,1,2,2,false,false}};
    graph[1] = {{3,1,100,3,false,false}}; graph[2] = {{4,1,0,4,false,false}};
    CHECK(compare(graph,p).best_cost == 100);
    p.max_sweeps = 2; CHECK(compare(graph,p).best_cost == 0);
    p.beam_width = 1; CHECK(compare(graph,p).best_cost == 100);
    for (int width : {1,2,3,7,8,9,31,32,33}) {
        p.beam_width = width;
        for (int k : {1,2,width,width+1}) { p.sweep_width = k; compare(graph,p); }
    }
    p = {}; p.max_turn = 2; p.beam_width = 1; p.max_turn_is_answer = false;
    graph[0] = {{1,1,10,1,false,true},{2,1,1,2,false,false}};
    CHECK(compare(graph,p).best_cost == 10);
    std::reverse(graph[0].begin(),graph[0].end()); CHECK(compare(graph,p).best_cost == 10);
    p.beam_width = 3;
    graph[0] = {{1,1,10,7,true,false},{2,1,5,8,true,false},{3,1,5,7,true,false}};
    graph[1].clear();graph[2].clear();graph[3].clear();
    auto r=compare(graph,p); CHECK(r.runtime.replaced_by_hash == 1);
    p.max_turn=0;CHECK(!compare(graph,p).found);
    p.max_turn_is_answer=true;CHECK(compare(graph,p).best_turn==0);
    p.max_turn=-7;p.beam_width=-1;p.sweep_width=0;compare(graph,p);
    p={};p.max_turn=3;graph[0].clear();r=compare(graph,p);CHECK(!r.found&&r.exhausted);
    // 取り出したハッシュは閉じた集合に残さず、次のスイープで再登録できる
    p.beam_width=3;graph.resize(7);
    graph[0]={{1,1,0,1,true,false},{2,1,1,2,true,false}};
    graph[1]={{3,1,10,9,true,false}};graph[2]={{4,1,20,9,true,false}};
    graph[3]={{5,1,100,5,true,false}};graph[4]={{6,1,0,6,true,false}};
    r=compare(graph,p);CHECK(r.best_cost==0&&r.runtime.pruned_by_hash==0);
    // 物理バケットが同じ異なるキーを重複と見なさない
    p.beam_width=7;p.hash_capacity=1;p.max_turn=2;graph.assign(80,{});
    std::uint64_t hash=0;
    for(int id=1;id<=64;++id){
        while((cs::single_detail::mix(hash)&15)!=0)++hash;
        graph[0].push_back({id,1,-static_cast<long long>(id),hash++,true,false});
        graph[static_cast<std::size_t>(id)]={{65,1,static_cast<long long>(id),0,false,false}};
    }
    r=compare(graph,p);CHECK(r.runtime.pruned_by_hash==0&&r.runtime.replaced_by_width==57);
    if constexpr (is_multi) {
        p.max_turn=5;max_step<BS>(p,3);
        graph[0]={{1,3,0,1,true,false}};graph[1]={{2,2,0,1,true,false}};
        r=compare(graph,p);CHECK(r.found&&r.runtime.expanded_nodes==2&&r.best_turn==5);
        graph[0]={{1,0,0,1,true,false},{1,-1,0,1,true,false},{1,4,0,1,true,false},{1,INT_MAX,0,1,true,false},{1,3,0,1,true,false}};
        graph[1]={{2,3,0,1,true,false}};
        r=compare(graph,p);CHECK(r.runtime.invalid_candidates==5);
    }
}
void randomized() {
    std::mt19937_64 rng(0x283abe44);
    auto pick = [&](int n) { return static_cast<int>(rng() % static_cast<std::uint64_t>(n)); };
    std::uint64_t generated = 0;
    for (int seed = 0; seed < 10000; ++seed) {
        BS::Param p; p.max_turn=pick(11);p.beam_width=1+pick(16);p.sweep_width=1+pick(20);
        p.max_sweeps=pick(10);p.use_hash_dedup=pick(2)!=0;p.max_turn_is_answer=pick(2)!=0;
        p.hash_capacity=pick(3)==0?1:INT_MAX;max_step<BS>(p,1+pick(10));
        constexpr int count=7;
        Graph graph(static_cast<std::size_t>((p.max_turn+1)*count));
        for (int t=0;t<p.max_turn;++t) for(int id=0;id<count;++id) {
            auto& edges=graph[static_cast<std::size_t>(t*count+id)];
            const int n=pick(7);
            for(int j=0;j<n;++j) {
                const int step=is_multi?1+pick(p.max_turn-t):1;
                const int node=(t+step)*count+pick(count);
                edges.push_back({node,step,static_cast<long long>(pick(11)-5),mix(static_cast<std::uint64_t>(node)),pick(4)!=0,pick(7)==0});
            }
        }
        const auto a=compare(graph,p);generated+=a.runtime.generated_candidates;
        if (seed<500) {
            auto b=compare(graph,p,false);CHECK(a.best_state==b.best_state);
            auto changed=graph;
            for(auto& edges:changed)for(auto& e:edges){e.cost=3*e.cost+7;e.hash=5*e.hash+13;}
            const auto c=compare(changed,p);CHECK(a.best_state==c.best_state);
            if(a.found&&a.best_turn>0)CHECK(c.best_cost==3*a.best_cost+7);
            if(p.max_sweeps>0){++p.max_sweeps;const auto d=compare(graph,p);if(a.found)CHECK(d.found&&d.best_cost<=a.best_cost);}
        }
    }
    std::cout<<"random_cases=10000 generated="<<generated<<'\n';
}
void exhaustive() {
    std::mt19937_64 rng(512);
    for(int seed=0;seed<1000;++seed) {
        BS::Param p;p.max_turn=static_cast<int>(rng()%7);p.beam_width=1024;p.sweep_width=3;p.use_hash_dedup=false;
        max_step<BS>(p,6);
        Graph graph(static_cast<std::size_t>((p.max_turn+1)*3));
        for(int t=0;t<p.max_turn;++t) for(int id=0;id<3;++id) {
            const int branches=static_cast<int>(rng()%4);
            for(int j=0;j<branches;++j) {
                const int step=is_multi?1+static_cast<int>(rng()%static_cast<std::uint64_t>(p.max_turn-t)):1;
                const int node=(t+step)*3+static_cast<int>(rng()%3);
                graph[static_cast<std::size_t>(t*3+id)].push_back({node,step,static_cast<long long>(rng()%100),0,false,rng()%7==0});
            }
        }
        std::optional<long long> optimum;
        if(p.max_turn==0)optimum=0;
        std::function<void(int,int)> dfs=[&](int node,int t) {
            for(const auto& e:graph[static_cast<std::size_t>(node)]) {
                if(e.is_answer||t+e.step==p.max_turn)if(!optimum||e.cost<*optimum)optimum=e.cost;
                if(t+e.step<p.max_turn)dfs(e.node,t+e.step);
            }
        };
        dfs(0,0);
        const auto r=compare(graph,p);
        CHECK(r.found==optimum.has_value());if(optimum)CHECK(r.best_cost==*optimum);
        CHECK(r.runtime.pruned_by_width==0);
    }
    std::cout<<"exhaustive_cases=1000\n";
}
void stress() {
    BS::Param p;p.max_turn=20;p.beam_width=33;p.sweep_width=2;p.max_sweeps=40;p.hash_capacity=1;
    max_step<BS>(p,1);
    constexpr int nodes=128,branches=512;
    Graph graph(static_cast<std::size_t>((p.max_turn+1)*nodes));
    for(int t=0;t<p.max_turn;++t)for(int id=0;id<nodes;++id)for(int j=0;j<branches;++j) {
        const auto h=mix(static_cast<std::uint64_t>(id*branches+j+t));
        const int target=(t+1)*nodes+static_cast<int>(h%nodes);
        graph[static_cast<std::size_t>(t*nodes+id)].push_back({target,1,static_cast<long long>(h%100000),static_cast<std::uint64_t>(target),true,false});
    }
    std::uint64_t operations=0;
    for(bool dedup:{false,true}){p.use_hash_dedup=dedup;operations+=compare(graph,p).runtime.generated_candidates;}
    CHECK(operations>=1000000);
    std::cout<<"stress_candidate_operations="<<operations<<'\n';
}
struct Tracked {
    static inline int living=0;
    static inline int copies=0;
    int value;
    bool moved=false;
    explicit Tracked(int v):value(v){++living;}
    Tracked(const Tracked& a):value(a.value){++living;++copies;}
    Tracked(Tracked&& a) noexcept:value(a.value){++living;a.moved=true;}
    Tracked& operator=(const Tracked& a){value=a.value;++copies;return *this;}
    Tracked& operator=(Tracked&& a) noexcept {value=a.value;a.moved=true;return *this;}
    ~Tracked(){--living;}
};
void state_and_numeric() {
    BS::Param p;p.max_turn=1;p.beam_width=1;
    {
        const auto r=BS::run(p,Tracked(0),0LL,[&](const auto&,BS::Emitter<Tracked>& e){
            Tracked a(1);CHECK(!push<BS>(e,std::move(a),10));CHECK(a.moved);
            Tracked b(2);const int copies=Tracked::copies;
            CHECK(!push<BS>(e,std::move(b),20));CHECK(!b.moved);CHECK(Tracked::copies==copies);
            int made=0;CHECK(!lazy<BS,Tracked>(e,10,1,false,0,false,[&](){++made;return Tracked(3);}));CHECK(made==0);
        });
        CHECK(r.best_state->value==1);CHECK(Tracked::living==1);
    }
    CHECK(Tracked::living==0);
    using F=cs::ChokudaiCopySingle<double,std::uint8_t>;
    F::Param fp;fp.max_turn=1;fp.beam_width=3;
    int made=0;
    const auto f=F::run(fp,0,0.,[&](const auto&,F::Emitter<int>& e){
        CHECK(!lazy<F,int>(e,std::numeric_limits<double>::quiet_NaN(),1,false,0,false,[&](){++made;return 4;}));
        CHECK(!push<F>(e,1,std::numeric_limits<double>::infinity()));
        CHECK(!push<F>(e,2,-0.));CHECK(!push<F>(e,3,+0.));
        CHECK(!push<F>(e,4,-std::numeric_limits<double>::infinity()));
    });
    CHECK(made==0&&f.best_state==4&&f.best_cost==-std::numeric_limits<double>::infinity());
    const auto n=F::run(fp,0,std::numeric_limits<double>::quiet_NaN(),[](const auto&,auto&){CHECK(false);});
    CHECK(!n.found&&n.exhausted);
    const auto v=BS::run(p,std::vector<int>{1},0LL,[](const auto& now,BS::Emitter<std::vector<int>>& e){
        auto s=now.state;s.push_back(2);push<BS>(e,std::move(s),LLONG_MIN);
    });
    CHECK(v.best_state==std::vector<int>({1,2})&&v.best_cost==LLONG_MIN);
    struct Hook { int calls=0;Hook()=default;Hook(const Hook&)=delete;void operator()(BS::EventType,const BS::Runtime&){++calls;} } hook;
    struct Expand {
        int with_runtime=0;Expand()=default;Expand(const Expand&)=delete;
        void operator()(const BS::StateView<int>&,BS::Emitter<int>&){CHECK(false);}
        void operator()(const BS::StateView<int>&,const BS::Runtime&,BS::Emitter<int>& e){++with_runtime;push<BS>(e,1,0);}
    } expand;
    BS::run(p,0,0LL,expand,hook);CHECK(expand.with_runtime==1&&hook.calls==4);
}
void time_and_events() {
    BS::Param p;p.max_turn=5;p.beam_width=2;p.time_limit_ms=1;p.time_check_interval=1;p.continue_on_timeout=false;
    auto run=[&](int delta,bool runtime,bool goal){
        Clock::value=0;Clock::calls=0;Events events;
        auto body=[&](const BS::StateView<int>& n,BS::Emitter<int>& e){
            Clock::value+=delta;push<BS>(e,n.state+1,static_cast<long long>(n.turn),1,false,0,goal);
        };
        auto hook=[&](BS::EventType event,const BS::Runtime& r){events.emplace_back(event,r);};
        BS::Result<int> r;
        if(runtime)r=BS::run(p,0,0LL,[&](const auto& n,const BS::Runtime&,BS::Emitter<int>& e){body(n,e);},hook);
        else r=BS::run(p,0,0LL,body,hook);
        int balance=0;
        for(const auto& [e,rt]:events){
            if(e==BS::EventType::SweepStart){++balance;CHECK(!rt.sweep_completed);}
            if(e==BS::EventType::SweepEnd)--balance;
            CHECK(balance==0||balance==1);
        }
        CHECK(balance==0&&events.front().first==BS::EventType::RunStart&&events.back().first==BS::EventType::RunEnd);
        return r;
    };
    CHECK(!run(199,false,false).time_limit_reached);
    auto a=run(200,false,false);CHECK(a.time_limit_reached&&a.found&&a.runtime.sweep_completed&&!a.exhausted);
    auto b=run(1000,false,false);CHECK(!b.found&&b.runtime.expanded_nodes==1&&!b.runtime.sweep_completed);
    auto c=run(1001,false,true);CHECK(c.found&&c.time_limit_reached&&c.best_turn==1);
    for(int interval:{1,2,64,1024}){
        p.time_check_interval=interval;
        auto x=run(500,false,false);const auto calls=Clock::calls;
        auto y=run(500,true,false);CHECK(counters(x.runtime)==counters(y.runtime)&&calls==Clock::calls);
        if(interval>=64)CHECK(x.found);
    }
    p.time_check_interval=1;p.continue_on_timeout=true;
    auto d=run(1000,false,false);CHECK(d.found&&d.runtime.sweep==2&&d.runtime.expanded_nodes==5);
    CHECK(d.time_limit_reached&&!d.exhausted);
    auto e=run(1000,false,true);CHECK(e.runtime.sweep==1);
    p.max_turn_is_answer=false;
    auto f=run(1000,false,false);CHECK(!f.found&&f.runtime.sweep==2&&f.runtime.expanded_nodes==5);
    p.time_limit_ms=0;p.max_sweeps=1;
    auto g=run(1000,false,false);CHECK(g.exhausted&&!g.sweep_limit_reached);
    // RunStart のコストで時間切れになる場合、通常スイープを始めない
    p.time_limit_ms=1;p.continue_on_timeout=false;p.max_turn=3;Clock::value=0;
    const auto h=BS::run(p,0,0LL,[](const auto&,auto&){CHECK(false);},[](BS::EventType event,const auto&){if(event==BS::EventType::RunStart)Clock::value=1000;});
    CHECK(h.time_limit_reached&&h.runtime.sweep==0);
    Clock::value=0;p.continue_on_timeout=true;
    const auto dead=BS::run(p,0,0LL,[](const auto&,auto&){},[](BS::EventType event,const auto&){if(event==BS::EventType::RunStart)Clock::value=1000;});
    CHECK(!dead.found&&dead.runtime.sweep==1&&dead.runtime.expanded_nodes==1&&dead.time_limit_reached);
    // best 更新回数に比例した時計読み出しがないことを確認
    auto many=[&](int count){Clock::value=0;Clock::calls=0;BS::Param mp;mp.max_turn=1;
        auto result=BS::run(mp,0,0LL,[&](const auto&,BS::Emitter<int>& emit){for(int i=0;i<count;++i)push<BS>(emit,i,-static_cast<long long>(i));});
        CHECK(result.runtime.best_update_count==static_cast<std::uint64_t>(count));return Clock::calls;};
    const auto one=many(1);CHECK(many(1000)==one);
}
void csv() {
#ifdef LOCAL
    const auto path=std::filesystem::temp_directory_path()/"chokudai_single_test.csv";
    BS::CsvStatHook hook(path.string(),25);
    BS::Runtime rt;hook(BS::EventType::RunStart,rt);
    int i=0;
    for(int ms:{10,20,40,45,80}) {
        ++i;rt.elapsed_us=ms*1000;rt.sweep=i;rt.expanded_nodes=static_cast<std::uint64_t>(i*10);
        rt.generated_candidates=static_cast<std::uint64_t>(i*20);rt.accepted_candidates=static_cast<std::uint64_t>(i*10);
        hook(BS::EventType::SweepStart,rt);hook(BS::EventType::SweepEnd,rt);
    }
    rt.elapsed_us=90000;hook(BS::EventType::RunEnd,rt);
    CHECK(hook.rows.size()==4);
    CHECK(hook.rows[1].elapsed_us==40000&&hook.rows[2].elapsed_us==80000);
    std::ifstream in(path);std::string line;
    std::vector<std::vector<std::string>> csv_rows;
    while(std::getline(in,line)) {
        CHECK(std::count(line.begin(),line.end(),',')==29);
        std::vector<std::string> cells;
        std::size_t start=0;
        for(;;) {
            const auto end=line.find(',',start);
            cells.push_back(line.substr(start,end==std::string::npos?end:end-start));
            if(end==std::string::npos)break;
            start=end+1;
        }
        csv_rows.push_back(std::move(cells));
    }
    CHECK(csv_rows.size()==5);in.close();
    CHECK(csv_rows[2][26]=="750"&&csv_rows[3][26]=="500"&&csv_rows[4][26]=="0");
    CHECK(csv_rows[2][27]=="0.5"&&csv_rows[4][27].empty());
    CHECK(csv_rows[4][29].empty());
    rt={};hook(BS::EventType::RunStart,rt);hook(BS::EventType::RunEnd,rt);CHECK(hook.rows.size()==2);
    BS::CsvStatHook zero(path.string(),0);zero(BS::EventType::RunStart,rt);zero(BS::EventType::SweepEnd,rt);zero(BS::EventType::RunEnd,rt);CHECK(zero.rows.size()==3);
    std::filesystem::remove(path);
    BS::CsvStatHook bad((path/"missing.csv").string(),0);
    BS::Param p;const auto r=BS::run(p,0,0LL,[](const auto&,auto&){},bad);CHECK(r.found);
#endif
}
static_assert(cs::CostType<int> && cs::CostType<long long> && cs::CostType<double> && !cs::CostType<unsigned> && !cs::CostType<bool>);
static_assert(cs::HashType<std::uint8_t> && cs::HashType<std::uint16_t> && cs::HashType<std::uint32_t> && cs::HashType<std::uint64_t>);
static_assert(!cs::HashType<bool> && !cs::HashType<int>);
static_assert(!cs::CompatibleHashArg<std::uint64_t,std::uint32_t> && !cs::CompatibleHashArg<int,std::uint64_t>);
static_assert(!std::default_initializable<BS::Emitter<int>>);
static_assert(!std::default_initializable<Tracked>);
} // namespace
// 共通方針の境界テスト。実時間のsleepに依存せず、仮想時計で再現する。
namespace policy_test {
using Lib = cs::ChokudaiCopySingle<>;
using Clock = cs::single_detail::TestClock;
struct State { int id = 0; };
struct Action { int from = 0, to = 0; };
struct Edge {
    int to; long long cost; bool answer = false;
    int step = 1; bool sub = false; std::uint64_t hash = 7;
};
using Graph = std::vector<std::vector<Edge>>;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("policy: ")+message);
}
struct Options {
    int slow_node = -1;
    bool expire_at_start = false, eager = false;
};
struct Outcome {
    bool found = false, completed = false;
    long long cost = 0;
    int id = -1, turn = 0;
    std::uint64_t makers = 0;
    std::vector<int> visited, path;
    std::vector<bool> accepted;
    std::vector<std::size_t> offsets;
    Lib::Runtime rt;
};
template<bool History>
Outcome run(Lib::Param p, const Graph& graph, Options options = {}) {
    Clock::enabled = true; Clock::value = 0;
    Outcome out;

    auto expand = [&](const auto& now, auto& emit) {
        const int from = now.state.id;
        out.visited.push_back(from);
        if constexpr (History) {
            auto history=emit.history();
            if (from==0) require(history.empty(), "root history");
            else require(!history.empty() && history.action().to==from, "expand history reference");
        }
        for (const auto& edge : graph.at(static_cast<std::size_t>(from))) {
            auto maker=[&]{++out.makers;return State{edge.to};};
            bool accepted;
            if constexpr (History) {
                if(options.eager) accepted=emit.push(maker(),Action{from,edge.to},edge.cost,edge.hash, edge.answer);
                else accepted=emit.push_lazy(Action{from,edge.to},edge.cost,edge.hash, edge.answer,maker);
            } else {
                if(options.eager) accepted=emit.push(maker(),edge.cost,edge.hash, edge.answer);
                else accepted=emit.push_lazy(edge.cost,edge.hash, edge.answer,maker);
            }
            out.accepted.push_back(accepted);
        }
        if (from == options.slow_node) Clock::value += 1000;
    };
    auto hook = [&](Lib::EventType event, const Lib::Runtime&) {
        if (event == Lib::EventType::RunStart && options.expire_at_start) Clock::value += 1000;
    };
    auto result=[&] {
            if constexpr (History) return Lib::run<State,Action>(p,State{},0LL,std::uint64_t{0},expand,hook);
            else return Lib::run<State>(p,State{},0LL,std::uint64_t{0},expand,hook);
        }();
    out.found = result.found; out.completed = result.completed_max_turn;
    out.cost = result.best_cost; out.turn = result.best_turn; out.rt = result.runtime;
    if(result.found) out.id=result.best_state->id;
    if constexpr(History) { int id=0;
        for(const auto& item:result.path){const auto& a=item;require(a.from==id,"path continuity");id=a.to;out.path.push_back(id);}
        if(result.found) require(id==result.best_state->id,"State matches action path"); }
    return out;
}
template<bool History>
void cases() {
    Lib::Param p;
    require(p.continue_on_timeout && p.expand_answers, "common defaults");
    p.max_turn = 5; p.max_turn_is_answer = false; p.beam_width = 1;
    p.expand_answers = false; p.time_check_interval = 1;
    Graph graph(8);
    graph[0] = {{1,-10,true},{2,5,true},{3,0,false}};
    graph[1] = {{5,-100,true}};
    graph[3] = {{4,-20,true}};
    for (bool eager : {false,true}) {
        auto r = run<History>(p,graph,{-1,false,eager});
        require(r.found && r.cost == -20 && r.id == 4, "answer retained outside beam/hash selection");
        require(r.visited == std::vector<int>({0,3}), "answers do not consume width or expand");
        require(r.accepted == std::vector<bool>({false,false,true,false}), "push bool means search acceptance");
        require(r.rt.answer_only_candidates == 3 && r.rt.terminal_candidates == 0, "separate answer-only counter");
        if (!eager) require(r.makers==3, "lazy maker runs only for accepted/best candidates");
        if constexpr (History) require(r.path==std::vector<int>({3,4}), "saved history survives queue removal");
    }
    p.expand_answers = true;
    auto continued = run<History>(p,graph);
    require(continued.found && continued.cost == -100 && continued.id == 5, "answers may remain expandable");
    require(continued.rt.answer_only_candidates == 0, "no answer exclusions when enabled");
    // An expired search with no solution must either stop or complete the remaining path.
    graph = Graph(8); graph[0]={{1,0}}; graph[1]={{2,0}}; graph[2]={{3,-4,true}};
    graph[3]={{4,-100,true}};
    for (bool allow : {false,true}) for (bool expand_answers : {false,true}) {
        p.continue_on_timeout=allow; p.expand_answers=expand_answers; p.time_limit_ms=1;
        auto r=run<History>(p,graph,{0});
        require(r.rt.time_limit_reached && r.found==allow, "timeout without answer");
        require(r.visited == (allow ? std::vector<int>({0,1,2}) : std::vector<int>({0})), "stop immediately after answer/callback");
        if (allow) require(r.cost==-4 && r.id==3, "rescue preserves the first available answer");
        auto at_start=run<History>(p,graph,{-1,true});
        require(at_start.found==allow, "initialization/hook timeout policy");
        require(at_start.visited == (allow ? std::vector<int>({0,1,2}) : std::vector<int>{}), "RunStart deadline detected before expansion");
        // Save all improving answers in the current callback, even when none can enter the beam.
        Graph goals(8);goals[0]={{1,-100},{2,20,true},{3,10,true}};goals[1]={{4,-200,true}};
        auto saved=run<History>(p,goals,{0});
        require(saved.found && saved.cost==10 && saved.id==3 && saved.visited==std::vector<int>({0}), "saved answer ends overtime regardless of continuation flag");
        // The best solution may predate the callback that reaches the time limit.
        goals=Graph(8);goals[0]={{1,-100},{2,20,true}};goals[1]={{3,-200},{4,10,true}};
        auto later=run<History>(p,goals,{1});
        require(later.found && later.cost==10 && later.visited==std::vector<int>({0,1}), "existing answer stops after a later deadline");
        // The continuation policy is inactive without an overall time limit.
        p.time_limit_ms=0;
        auto unlimited=run<History>(p,graph,{0});
        require(unlimited.found && unlimited.cost==(expand_answers?-100:-4), "no limit means no early timeout stop");
    }
    p.continue_on_timeout=true;p.expand_answers=false;p.time_limit_ms=1;
    Graph dead(2);dead[0]={{1,0}};
    auto exhausted=run<History>(p,dead,{0});
    require(!exhausted.found && exhausted.visited==std::vector<int>({0,1}), "finite dead-end rescue");
    // Final-turn states stay terminal even when they are not answers.
    p.time_limit_ms=0;p.max_turn=1;p.expand_answers=true;
    Graph end(3);end[0]={{1,4,true},{2,0,false}};
    auto terminal=run<History>(p,end);
    require(terminal.found && terminal.cost==4 && terminal.completed, "explicit final-turn answer");
    require(terminal.rt.terminal_candidates==2 && terminal.rt.answer_only_candidates==0 && terminal.visited.size()==1, "final-turn counters and no expansion");
    p.max_turn_is_answer=true;
    auto automatic=run<History>(p,end);
    require(automatic.found && automatic.cost==0 && automatic.id==2, "implicit final-turn answer");
    p.max_turn=0;
    auto root=run<History>(p,end);
    require(root.found && root.completed && root.visited.empty(), "zero-turn solution");
    p={};p.max_turn=4;p.beam_width=2;p.sweep_width=1;p.max_sweeps=1;
    p.time_limit_ms=1;p.time_check_interval=1;p.max_turn_is_answer=false;p.expand_answers=false;
    Graph retry(4);retry[0]={{1,0,false,1,false,1},{2,10,false,1,false,2}};retry[2]={{3,20,true}};
    auto rescued=run<History>(p,retry,{0});
    require(rescued.found && rescued.id==3 && rescued.visited==std::vector<int>({0,1,2}),"rescue retries another sweep and ignores normal sweep limit");
    p.time_limit_ms=0;
    auto limited=run<History>(p,retry);
    require(!limited.found && limited.visited==std::vector<int>({0,1}),"sweep limit before timeout still applies");

}
void all() {
    const bool enabled=Clock::enabled;
    const auto value=Clock::value;
    cases<false>();
    cases<true>();
    Clock::enabled=enabled;Clock::value=value;
}
} // namespace policy_test

// max_turn=0では初期Stateを最良解へ移し、不要なコピーを行わない
static void test_root_state_transfer() {
    using B = cs::ChokudaiCopySingle<>;
    struct State {
        int* copies;
        std::vector<int> data{17};
        explicit State(int& count) : copies(&count) {}
        State(const State& s) : copies(s.copies), data(s.data) { ++*copies; }
        State(State&&) = default;
        State& operator=(const State& s) {
            copies = s.copies; data = s.data; ++*copies; return *this;
        }
        State& operator=(State&&) = default;
    };
    int copies = 0;
    typename B::Param p;
    const auto r = B::run(p, State(copies), 0, [](const auto&, auto&) { std::abort(); });
    if (!r.found || r.best_state->data != std::vector<int>{17} || copies != 0)
        throw std::runtime_error("zero-turn State must transfer without a copy");
}

int main() {
    test_root_state_transfer();
    policy_test::all();
    deterministic();state_and_numeric();time_and_events();csv();
    Clock::value=0;randomized();exhaustive();stress();
    std::cout<<"PASS single checks="<<checks<<'\n';
}
#undef CHECK
#endif
