#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace bs {

// ============================================================
// detail: 低レベル補助関数
// ============================================================

namespace detail {

template<class B>
inline constexpr bool is_bool_arg_v = std::is_same_v<std::remove_cv_t<std::remove_reference_t<B>>, bool>;

template<class U, class State>
inline constexpr bool is_state_arg_v = std::is_same_v<std::remove_cv_t<std::remove_reference_t<U>>, State>;

template<class H, class Hash>
inline constexpr bool is_compatible_hash_arg_v =
    std::is_integral_v<std::remove_cv_t<std::remove_reference_t<H>>> &&
    std::is_unsigned_v<std::remove_cv_t<std::remove_reference_t<H>>> &&
    !std::is_same_v<std::remove_cv_t<std::remove_reference_t<H>>, bool> &&
    std::is_convertible_v<std::remove_cv_t<std::remove_reference_t<H>>, Hash> &&
    (sizeof(std::remove_cv_t<std::remove_reference_t<H>>) <= sizeof(Hash));

inline std::int64_t now_microseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace detail

template<class Cost = long long, class Hash = std::uint64_t>
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
// 縮小版ではdebug_hookを呼ばないため、独自HookとCsvStatHookにはイベントを通知しない。
// enum自体は既存コードの型参照を壊さないため残す。
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

// expandのRuntime付き overload と Result で参照できる最小限の実行時情報。
// debug_hookを使わない縮小版では、探索挙動に必要な値だけを保持する。
struct Runtime {
    int turn = 0;                   // 現在処理中のターン。初期状態は0。
    long long expanded_nodes = 0;   // 展開済みノード数。time_check_interval の判定にも使う。
    std::int64_t elapsed_us = 0;    // 探索開始からの経過時間。

    // time_limit_msに達したことを検出したか。expand中の即時中断はしない。
    bool time_limit_reached = false;
    bool found = false;
    Cost best_cost{};
    int best_turn = 0;
    bool completed_max_turn = false;
};

// デフォルトHook。何もしない。
struct NoOp {
    void operator()(EventType, const Runtime&) const noexcept {}
};

// CSV出力Hook。
// 縮小版ではコンストラクタの互換性だけを残し、debug_hookとして渡されても何もしない。
struct CsvStatHook {
    explicit CsvStatHook(std::string csv_filename = "beam_stat.csv", double interval_ms = 50.0) {
        (void)csv_filename;
        (void)interval_ms;
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
    requires detail::is_state_arg_v<U, State>
    bool push(U&& state, Cost cost) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, false, false);
    }

    template<class U, class B>
    requires detail::is_state_arg_v<U, State> && detail::is_bool_arg_v<B>
    bool push(U&& state, Cost cost, B finished) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, false, finished);
    }

    template<class U, class H>
    requires detail::is_state_arg_v<U, State> && detail::is_compatible_hash_arg_v<H, Hash>
    bool push(U&& state, Cost cost, H hash) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), true, false);
    }

    template<class U, class H, class B>
    requires detail::is_state_arg_v<U, State> &&
             detail::is_compatible_hash_arg_v<H, Hash> && detail::is_bool_arg_v<B>
    bool push(U&& state, Cost cost, H hash, B finished) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), true, finished);
    }

    template<class Maker>
    bool push_lazy(Cost cost, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, false, false, std::forward<Maker>(maker));
    }

    template<class B, class Maker>
    requires detail::is_bool_arg_v<B>
    bool push_lazy(Cost cost, B finished, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, false, finished, std::forward<Maker>(maker));
    }

    template<class H, class Maker>
    requires detail::is_compatible_hash_arg_v<H, Hash>
    bool push_lazy(Cost cost, H hash, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), true, false, std::forward<Maker>(maker));
    }

    template<class H, class B, class Maker>
    requires detail::is_compatible_hash_arg_v<H, Hash> && detail::is_bool_arg_v<B>
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
        // NaN cost や最大ターン超過は、幅やhashではなく不正候補として棄却する。
        if constexpr (std::is_floating_point_v<Cost>) {
            if (std::isnan(cost) || child_turn_ > engine_.param.max_turn) return false;
        } else {
            if (child_turn_ > engine_.param.max_turn) return false;
        }

        auto& selector = engine_.selector;
        // まずcostだけで幅棄却を行い、State生成を避ける。
        if (selector.full() && !(selector.worst_index_cache < 0 || cost < selector.worst_cost_cache)) return false;
        if (!(selector.use_hash_ && has_hash)) {
            if (!selector.full()) {
                selector.append_candidate(cost, hash, child_turn_, has_hash, finished, std::forward<Maker>(maker));
                return true;
            }
            const int j = selector.worst_index_cache;
            if (j < 0) return false;
            selector.replace_candidate(j, cost, hash, child_turn_, has_hash, finished, std::forward<Maker>(maker));
            return true;
        }

        auto found = selector.hash_.lookup(hash);
        if (found.found) {
            const int j = selector.hash_.values_[found.slot];
            const bool live = 0 <= j && j < static_cast<int>(selector.candidates_.size()) &&
                              selector.candidates_[j].has_hash && selector.candidates_[j].hash == hash;
            if (live) {
                if (!(cost < selector.costs_[j])) return false;
                selector.replace_candidate(j, cost, hash, child_turn_, true, finished, std::forward<Maker>(maker));
                selector.hash_.set(found.slot, hash, j);
                return true;
            }
            // stale entry なので、この slot を上書きしてよい。候補本体は既に別候補で置換されている。
        }

        if (found.slot < 0) {
            selector.hash_.clear();
            for (int i = 0; i < static_cast<int>(selector.candidates_.size()); ++i) {
                if (!selector.candidates_[i].has_hash) continue;
                auto slot = selector.hash_.lookup(selector.candidates_[i].hash);
                if (slot.slot >= 0) selector.hash_.set(slot.slot, selector.candidates_[i].hash, i);
            }
            found = selector.hash_.lookup(hash);
            if (found.slot < 0) return false;
        }

        if (!selector.full()) {
            const int j = static_cast<int>(selector.candidates_.size());
            selector.append_candidate(cost, hash, child_turn_, true, finished, std::forward<Maker>(maker));
            selector.hash_.set(found.slot, hash, j);
            return true;
        }

        const int j = selector.worst_index_cache;
        if (j < 0) return false;
        selector.replace_candidate(j, cost, hash, child_turn_, true, finished, std::forward<Maker>(maker));
        selector.hash_.set(found.slot, hash, j);
        return true;
    }

    Engine<State>& engine_;
    Hash now_hash_{};
    int child_turn_ = 0;
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


    void clear() {
        ++epoch_;
        if (epoch_ == 0) {
            std::fill(used_.begin(), used_.end(), 0);
            epoch_ = 1;
        }
    }

    Lookup lookup(Hash key) const {
        auto x = static_cast<std::uint64_t>(key) + 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        int i = static_cast<int>(x ^ (x >> 31)) & mask_;
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

private:
    template<class> friend class Engine;
    template<class> friend class Emitter;

    int capacity_ = 0;
    int mask_ = 0;
    std::uint32_t epoch_ = 1;
    std::vector<Hash> keys_;
    std::vector<int> values_;
    std::vector<std::uint32_t> used_;
};

// ============================================================
// 状態コピー型 beam 本体
// ============================================================

template<class State>
class Engine {
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
        friend class Engine;
        friend class Emitter<State>;

    public:
        [[nodiscard]] bool full() const { return static_cast<int>(candidates_.size()) >= beam_width_; }

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

        template<class Maker>
        void append_candidate(Cost cost, Hash hash, int turn, bool has_hash, bool finished, Maker&& maker) {
            candidates_.emplace_back(std::invoke(std::forward<Maker>(maker)), cost, hash, turn, has_hash, finished);
            const int j = static_cast<int>(candidates_.size()) - 1;
            costs_[j] = cost;
            if (static_cast<int>(candidates_.size()) == beam_width_) {
                std::fill(seg_.begin(), seg_.end(), -1);
                for (int i = 0; i < beam_width_; ++i) seg_[seg_n_ + i] = i;
                for (int i = seg_n_ - 1; i >= 1; --i) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
                worst_index_cache = seg_[1];
                if (worst_index_cache >= 0) worst_cost_cache = costs_[worst_index_cache];
            }
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
            if (full()) {
                int i = seg_n_ + j;
                seg_[i] = j;
                while (i >>= 1) seg_[i] = worse_index(seg_[i << 1], seg_[i << 1 | 1]);
                worst_index_cache = seg_[1];
                if (worst_index_cache >= 0) worst_cost_cache = costs_[worst_index_cache];
            }
        }

    };

public:
    Engine(Param param_, State&& initial_state_, Cost initial_cost_, Hash initial_hash_, bool initial_has_hash_)
        : param(param_),
          initial_state(std::move(initial_state_)),
          initial_cost(initial_cost_),
          initial_hash(initial_hash_),
          initial_has_hash(initial_has_hash_) {
        // ユーザー指定値を内部で扱いやすい範囲へ正規化する。
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

    template<class Expand, class DebugHook>
    Result<State> run(Expand&& expand, DebugHook&& debug_hook) {
        (void)debug_hook;
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

        // frontierとselectorは1本ずつ持つ。探索開始時に初期状態をfrontierへ置く。
        frontier.clear();
        frontier.reserve(std::max(1, param.beam_width));
        // Selectorもここで直接初期化し、一度しか呼ばれない初期化関数を置かない。
        selector.beam_width_ = std::max(1, param.beam_width);
        selector.use_hash_ = param.use_hash_dedup;
        selector.candidates_.clear();
        selector.candidates_.reserve(selector.beam_width_);
        selector.costs_.assign(selector.beam_width_, Cost{});
        if (selector.use_hash_) {
            int cap = std::max(param.hash_capacity, selector.beam_width_ * 2 + 1);
            cap = std::max(4, cap);
            selector.hash_.capacity_ = 1;
            while (selector.hash_.capacity_ < cap) selector.hash_.capacity_ <<= 1;
            selector.hash_.mask_ = selector.hash_.capacity_ - 1;
            selector.hash_.keys_.assign(static_cast<std::size_t>(selector.hash_.capacity_), Hash{});
            selector.hash_.values_.assign(static_cast<std::size_t>(selector.hash_.capacity_), -1);
            selector.hash_.used_.assign(static_cast<std::size_t>(selector.hash_.capacity_), 0);
            selector.hash_.epoch_ = 1;
        }
        selector.seg_n_ = 1;
        while (selector.seg_n_ < selector.beam_width_) selector.seg_n_ <<= 1;
        selector.seg_.assign(selector.seg_n_ * 2, -1);
        selector.worst_index_cache = -1;
        frontier.emplace_back(std::move(initial_state), initial_cost, initial_hash, 0, initial_has_hash, false);

        rt.turn = 0;
        rt.best_cost = initial_cost;
        rt.best_turn = 0;
        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) rt.time_limit_reached = true;
        if (!frontier.empty() && is_answer(frontier.front())) set_best(frontier.front());
        rt.completed_max_turn = (param.max_turn == 0);

        // 通常時は全frontierを展開し、時間到達後は各turnで最良1本だけを進める。
        bool minimal_mode = rt.time_limit_reached;
        while (!rt.completed_max_turn && !frontier.empty()) {
            rt.turn = frontier.front().turn;
            if (rt.turn >= param.max_turn) {
                rt.completed_max_turn = true;
                break;
            }

            if (use_time_limit) mark_time_limit_if_elapsed();
            minimal_mode |= rt.time_limit_reached;

            int next_index = 0;
            if (use_time_limit && !minimal_mode) {
                for (; next_index < static_cast<int>(frontier.size()); ++next_index) {
                    expand_item(frontier[next_index], expand);
                    // soft limit判定。expand完了後に間引いて呼び、到達後は縮退探索へ移る。
                    // expandの途中はライブラリ側では中断しない。
                    if ((rt.expanded_nodes % param.time_check_interval) == 0 && mark_time_limit_if_elapsed()) {
                        minimal_mode = true;
                        ++next_index;
                        break;
                    }
                }
            } else if (!use_time_limit) {
                const int n = static_cast<int>(frontier.size());
                for (int i = 0; i < n; ++i) expand_item(frontier[i], expand);
            }

            if (use_time_limit && minimal_mode) {
                const int best = best_item_index(frontier, next_index);
                if (best >= 0) expand_item(frontier[best], expand);
            }

            frontier.clear();

            // Selector内の候補を次frontierへ移す。縮退時は最良cost 1件だけを移す。
            auto& selected = selector.candidates_;
            const int best_idx = minimal_mode ? best_item_index(selected) : -1;
            if (frontier.capacity() < static_cast<std::size_t>(param.beam_width)) frontier.reserve(param.beam_width);
            if (minimal_mode) {
                if (best_idx >= 0) materialize_one(selected[best_idx]);
            } else {
                for (Item& item : selected) materialize_one(item);
            }
            selector.candidates_.clear();
            selector.worst_index_cache = -1;
            if (selector.use_hash_) selector.hash_.clear();

            if (use_time_limit) mark_time_limit_if_elapsed();
            minimal_mode |= rt.time_limit_reached;
            if (rt.completed_max_turn) break;
        }
        update_time();

        Result<State> res;
        res.found = rt.found;
        res.time_limit_reached = rt.time_limit_reached;
        res.exhausted = frontier.empty();
        res.completed_max_turn = rt.completed_max_turn;
        res.best_cost = rt.best_cost;
        res.best_turn = rt.best_turn;
        res.best_state = std::move(best_state);
        res.runtime = rt;
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

    std::vector<Item> frontier;
    Selector selector;
    std::optional<State> best_state;

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


    template<class Expand>
    void expand_item(Item& item, Expand& expand) {
        StateView<State> now(item.turn, item.cost, item.state, item.hash, item.has_hash);
        Emitter<State> emit(*this, item.hash, item.turn + 1);
        if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
            expand(now, static_cast<const Runtime&>(rt), emit);
        } else {
            expand(now, emit);
        }
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

    void set_best(const Item& item) {
        // best更新もcost-only。同costの解候補では先に見つかったものを残す。
        if (rt.found && !(item.cost < rt.best_cost)) return;
        best_state.emplace(item.state);
        rt.found = true;
        rt.best_cost = item.cost;
        rt.best_turn = item.turn;
        update_time();
    }

    // best_stateはmove前にコピーする。これにより返却されるbest_stateはfrontierの寿命に依存しない。
    void materialize_one(Item& item) {
        if (is_answer(item)) set_best(item);
        rt.completed_max_turn |= (item.turn == param.max_turn);
        frontier.push_back(std::move(item));
    }
};


public:
    // Hashあり初期状態版。
    // expand は (const StateView<State>&, Emitter<State>&)、または
    // (const StateView<State>&, const Runtime&, Emitter<State>&) を受け取れる。
    // Runtime は const 参照固定であり、expand から Runtime を変更することはできない。
    // 履歴が必要な場合はユーザー側で State に含める。
    template<class State, class H, class Expand, class DebugHook = NoOp>
    requires detail::is_compatible_hash_arg_v<H, Hash>
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
    requires (!detail::is_compatible_hash_arg_v<Expand, Hash>)
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
#include <iostream>
#include <random>
#include <thread>

namespace {

using BS = bs::Beam<int, std::uint64_t>;
using BS32 = bs::Beam<int, std::uint32_t>;


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
    void operator()(const BS32::StateView<SmallState>& now, const BS32::Runtime&, BS32::Emitter<SmallState>& emit) const {
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
        // 対象コードが残っている公開APIの基本形を実行時assertで確認する。
        BS_TEST_ASSERT((std::is_same_v<decltype(BS::Param{}.time_limit_ms), double>));
        BS_TEST_ASSERT(!std::is_default_constructible_v<BS::Emitter<SmallState>>);
    }
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
    }
    {
        BS32::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        auto res = BS32::run(param, SmallState{}, 0, std::uint32_t{0}, ExplicitEmitterExpand{});
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 4);
        BS_TEST_ASSERT(res.best_state->x == 321);
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
    }
    {
        BS32::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        auto res = BS32::run(param, SmallState{}, 0, ExplicitEmitterRuntimeExpand{});
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 6);
        BS_TEST_ASSERT(res.best_state->x == 654);
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
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 1);
    }
    {
        // CsvStatHookは縮小版では空実装だが、コンストラクタ互換とdebug_hook引数として渡せることは維持する。
        BS::Param param;
        param.max_turn = 1;
        param.beam_width = 2;
        BS::CsvStatHook hook("beam_copy_single_small_v35_stat.csv", 0.0);
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
        BS_TEST_ASSERT(res.completed_max_turn);
    }
    {
        // debug_hookは受け取るだけで使用しない仕様なので、独自Hookにもイベント通知しない。
        struct CountingHook {
            int* count = nullptr;
            void operator()(BS::EventType, const BS::Runtime&) const { ++*count; }
        };
        BS::Param param;
        param.max_turn = 1;
        int hook_calls = 0;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth != 0) return;
            SmallState s = now.state;
            s.depth = 1;
            s.x = 7;
            emit.push(std::move(s), 7, true);
        };
        auto res = BS::run(param, SmallState{}, 0, expand, CountingHook{&hook_calls});
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(hook_calls == 0);
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
