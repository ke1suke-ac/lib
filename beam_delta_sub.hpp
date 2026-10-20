#pragma once
#include <bits/stdc++.h>

// beam_delta_sub_v12.hpp : ターン境界で比較をそろえる sub 探索。cost は小さいほど良い。
// W=境界幅、U=sub幅、G=生成数、E=展開数、D=経路長。利用側の処理費用は別途加算。
namespace bs {
#if __INCLUDE_LEVEL__ == 0
struct SubTestAccess;
#endif
template<class T>
concept CostType = ((std::integral<T> && std::is_signed_v<T>) || std::floating_point<T>) && !std::same_as<T, bool>;
template<class T>
concept HashType = std::unsigned_integral<T> && !std::same_as<T, bool> && sizeof(T) <= 8;
template<class H, class Hash>
concept CompatibleHashArg = HashType<std::remove_cvref_t<H>> && sizeof(std::remove_cvref_t<H>) <= sizeof(Hash);
template<class B> concept BoolArg = std::same_as<std::remove_cvref_t<B>, bool>;
template<class U, class T> concept ActionArg = std::same_as<std::remove_cvref_t<U>, T>;

namespace sub_detail {
inline std::uint64_t mix(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
inline std::int64_t to_us(double ms) noexcept {
    if (!(ms > 0.0)) return 0;
    const long double us = static_cast<long double>(ms) * 1000.0L;
    const auto upper = std::numeric_limits<std::int64_t>::max();
    if (us >= static_cast<long double>(upper) - 0.5L) return upper;
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(us + 0.5L));
}
#if __INCLUDE_LEVEL__ == 0
// 単体実行時だけの仮想時計。通常includeでは領域・分岐とも存在しない。
struct TestClock {
    static inline bool enabled = false;
    static inline std::int64_t value = 0;
    static inline std::uint64_t calls = 0;
};
#endif
inline std::int64_t now_us() {
#if __INCLUDE_LEVEL__ == 0
    ++TestClock::calls;
    if (TestClock::enabled) return TestClock::value;
#endif
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
template<class Cost> bool valid(Cost c) {
    if constexpr (std::floating_point<Cost>) return !std::isnan(c);
    else return true;
}
} // namespace sub_detail

template<CostType Cost = long long, HashType Hash = std::uint64_t>
struct Beam {
    using cost_type = Cost;
    using hash_type = Hash;
private:
#if __INCLUDE_LEVEL__ == 0
    friend struct SubTestAccess;
#endif
    template<class Value> class Engine;
public:
    enum class Next { Sub, Turn };
    enum class StopReason { None, Completed, Exhausted, TimeLimit, SweepLimit, InvalidInitial };
    enum class EventType { RunStart, SubStepEnd, TurnEnd, RunEnd };
    struct Param {
        int max_turn = 0;              // 完了させるターン数。負値は0。
        int beam_width = 1;            // 次ターン開始候補の幅。1以上。
        double time_limit_ms = 0;      // 探索全体のsoft limit。0以下・NaNは無制限。
        int time_check_interval = 64;  // 通常の時計確認間隔。改善時・層境界でも確認。
        bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
        bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
        bool max_turn_is_answer = true;
        bool use_hash_dedup = true;    // Hashが明示された候補だけを重複排除。

        int sub_beam_width = 0;        // 0以下ならbeam_widthを使用。
        bool use_turn_time_limit = false; // 残り時間を残りturnへ配分。全体制限が有効な場合だけ使用。
    };
    struct NodeView {
        int turn = 0; Cost cost{}; Hash hash{}; bool has_hash = false;
        std::int64_t sub_step = 0, depth = 0;
    };
    struct CandidateStats {
        std::uint64_t generated = 0, accepted = 0;
        std::uint64_t pruned_by_width = 0, pruned_by_hash = 0, replaced_by_hash = 0;
    };
    struct Runtime {
        std::uint64_t terminal_candidates = 0; // 有効なmax_turn到着候補数。候補プールには入れない。
        std::uint64_t answer_only_candidates = 0; // expand_answers=falseで除外したmax_turn未満の有効候補数。
        Param param;                  // 正規化した設定。幅等を別項目として重複保持しない。
        int turn = 0;
        std::int64_t sub_step = 0;
        std::int64_t elapsed_us = 0;  // 最後に時計を読んだ時点。
        // turn時刻はrun開始からのus。配分無効・turn未開始はstart/deadline=-1。
        // elapsedは最後の時計確認時点。TurnEndで固定し、最終復元・RunEndには加算しない。
        std::int64_t turn_start_us = -1, turn_deadline_us = -1, turn_elapsed_us = 0;
        bool turn_time_limit_reached = false; // 各turn開始時に解除。全体のtime_limit_reachedとは独立。
        bool time_limit_reached = false;
        std::uint64_t expanded_nodes = 0, invalid_candidates = 0;
        CandidateStats sub_stats, turn_stats;
        std::size_t frontier_remaining = 0, sub_candidate_count = 0, next_turn_candidate_count = 0;
        std::size_t peak_pending_candidates = 0;
        Cost sub_best_cost{}, sub_worst_cost{}, next_turn_best_cost{}, next_turn_worst_cost{};
        // 候補costは対応するcandidate_countが非0のときだけ有効。
        bool found = false;
        Cost best_cost{};
        int best_turn = 0;
        std::int64_t best_sub_step = 0, best_depth = 0;
        std::uint64_t best_update_count = 0;
        std::int64_t first_solution_us = -1, last_best_update_us = -1;
        std::uint64_t expanded_after_timeout = 0, timeout_turn_advances = 0;
        std::size_t used_nodes = 0, reserved_nodes = 0; // 構築済みNode数（回収待ちを含む）と確保済みスロット数。
        std::uint64_t move_forward_count = 0, move_backward_count = 0;
        StopReason stop_reason = StopReason::None;
        bool completed_max_turn = false;
    };
    struct NoOp {
        // 通知を無視する。O(1)。
        void operator()(EventType, const Runtime&) const noexcept {}
    };
    // LOCAL定義時のみCSVを記録する。未定義時は同じ呼び出し方で使える空実装。
    struct CsvStatHook {
#ifdef LOCAL
        struct Row { EventType event; Runtime runtime; };
        std::string filename;
        std::int64_t interval_us;
        std::vector<Row> rows;
        // 保存先と記録間隔。0以下・NaNは全イベントを記録。O(ファイル名長)。
        explicit CsvStatHook(std::string path = "beam_stat.csv", double interval_ms = 50)
            : filename(std::move(path)), interval_us(sub_detail::to_us(interval_ms)) {}
        // 進捗を蓄積する。償却O(1)、RunEndは行数に比例してCSVを出力する。
        void operator()(EventType event, const Runtime& rt) {
            if (event == EventType::RunStart) rows.clear();
            if (event == EventType::RunStart || event == EventType::RunEnd || rows.empty() ||
                interval_us == 0 || rt.elapsed_us - rows.back().runtime.elapsed_us >= interval_us) {
                rows.push_back({event, rt});
            }
            if (event == EventType::RunEnd) write();
        }
    private:
        static const char* event_name(EventType e) {
            switch (e) {
            case EventType::RunStart: return "RunStart";
            case EventType::SubStepEnd: return "SubStepEnd";
            case EventType::TurnEnd: return "TurnEnd";

            case EventType::RunEnd: return "RunEnd";
            }
            return "";
        }
        static const char* reason_name(StopReason r) {
            switch (r) {
            case StopReason::None: return "None";
            case StopReason::Completed: return "Completed";
            case StopReason::Exhausted: return "Exhausted";
            case StopReason::TimeLimit: return "TimeLimit";
            case StopReason::SweepLimit: return "SweepLimit";
            case StopReason::InvalidInitial: return "InvalidInitial";
            }
            return "";
        }
        void write() const {
            std::ofstream out(filename);
            if (!out) throw std::runtime_error("CSV file could not be opened: " + filename);
            out.imbue(std::locale::classic());
            out << std::setprecision(std::numeric_limits<long double>::max_digits10);
            bool first = true;
            auto cell = [&](const auto& x) {
                if (!first) out << ',';
                first = false;
                if constexpr (std::integral<std::remove_cvref_t<decltype(x)>>) out << +x;
                else out << x;
            };
            auto end = [&] { out << '\n'; first = true; };
            for (const char* s : {"event", "elapsed_ms", "turn", "sub_step", "expanded_nodes",
                 "sub_generated", "sub_accepted", "sub_pruned_by_width", "sub_pruned_by_hash", "sub_replaced_by_hash",
                 "turn_generated", "turn_accepted", "turn_pruned_by_width", "turn_pruned_by_hash", "turn_replaced_by_hash",
                 "invalid_candidates", "terminal_candidates", "answer_only_candidates", "frontier_remaining", "sub_candidate_count", "next_turn_candidate_count",
                 "peak_pending_candidates", "sub_best_cost", "sub_worst_cost", "next_turn_best_cost", "next_turn_worst_cost",
                 "found", "best_cost", "best_turn", "best_sub_step", "best_depth", "best_update_count",
                 "first_solution_ms", "last_best_update_ms", "time_limit_reached",
                 "turn_start_ms", "turn_deadline_ms", "turn_budget_ms", "turn_elapsed_ms", "turn_time_limit_reached",
                 "expanded_after_timeout",
                 "timeout_turn_advances", "used_nodes", "reserved_nodes", "move_forward_count", "move_backward_count",
                 "stop_reason", "completed_max_turn", "max_turn", "beam_width", "sub_beam_width",
                 "time_limit_ms", "use_turn_time_limit", "time_check_interval", "continue_on_timeout", "expand_answers", "max_turn_is_answer", "use_hash_dedup"}) cell(s);
            end();
            for (const auto& row : rows) {
                const auto& r = row.runtime;
                auto opt = [&](bool present, const auto& value) { if (present) cell(value); else cell(""); };
                auto stats = [&](const CandidateStats& s) {
                    cell(s.generated); cell(s.accepted); cell(s.pruned_by_width);
                    cell(s.pruned_by_hash); cell(s.replaced_by_hash);
                };
                cell(event_name(row.event)); cell(static_cast<double>(r.elapsed_us) / 1000.0); cell(r.turn); cell(r.sub_step);
                cell(r.expanded_nodes); stats(r.sub_stats); stats(r.turn_stats); cell(r.invalid_candidates); cell(r.terminal_candidates); cell(r.answer_only_candidates);
                cell(r.frontier_remaining); cell(r.sub_candidate_count); cell(r.next_turn_candidate_count);
                cell(r.peak_pending_candidates);
                opt(r.sub_candidate_count != 0, r.sub_best_cost); opt(r.sub_candidate_count != 0, r.sub_worst_cost);
                opt(r.next_turn_candidate_count != 0, r.next_turn_best_cost); opt(r.next_turn_candidate_count != 0, r.next_turn_worst_cost);
                cell(r.found); opt(r.found, r.best_cost); opt(r.found, r.best_turn);
                opt(r.found, r.best_sub_step); opt(r.found, r.best_depth); cell(r.best_update_count);
                opt(r.first_solution_us >= 0, static_cast<double>(r.first_solution_us) / 1000.0);
                opt(r.last_best_update_us >= 0, static_cast<double>(r.last_best_update_us) / 1000.0);
                cell(r.time_limit_reached);
                auto ms = [](std::int64_t us) { return static_cast<double>(us) * 0.001; };
                cell(r.turn_start_us < 0 ? -1.0 : ms(r.turn_start_us));
                cell(r.turn_deadline_us < 0 ? -1.0 : ms(r.turn_deadline_us));
                cell(r.turn_deadline_us < 0 ? -1.0 : ms(r.turn_deadline_us - r.turn_start_us));
                cell(ms(r.turn_elapsed_us)); cell(r.turn_time_limit_reached);
                cell(r.expanded_after_timeout); cell(r.timeout_turn_advances);
                cell(r.used_nodes); cell(r.reserved_nodes); cell(r.move_forward_count); cell(r.move_backward_count);
                cell(reason_name(r.stop_reason)); cell(r.completed_max_turn);
                const bool start = row.event == EventType::RunStart;
                const auto& p = r.param;
                opt(start, p.max_turn); opt(start, p.beam_width); opt(start, p.sub_beam_width);
                opt(start, p.time_limit_ms); opt(start, p.use_turn_time_limit); opt(start, p.time_check_interval);
                opt(start, p.continue_on_timeout); opt(start, p.expand_answers); opt(start, p.max_turn_is_answer); opt(start, p.use_hash_dedup);
                end();
            }
            out.flush();
            if (!out) throw std::runtime_error("CSV write failed: " + filename);
        }
#else
        explicit CsvStatHook(std::string = "beam_stat.csv", double = 50) {}
        void operator()(EventType, const Runtime&) {}
#endif
    };
    template<class Value> struct Result {
        bool found = false, time_limit_reached = false, completed_max_turn = false;
        Cost best_cost{};
        int best_turn = 0;
        std::int64_t best_sub_step = 0, best_depth = 0;
        std::vector<Value> path;
        std::vector<std::size_t> turn_offsets;
        Runtime runtime;
    };
    template<class Value> class Emitter {
    public:
        // 候補を追加する。期待償却O(log(2+幅)+保存費用)。trueは探索候補への一時保存。最良解だけ保存した場合はfalse。
        template<class U> requires ActionArg<U, Value>
        bool push(U&& value, Cost cost, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, hash_, false, false, next);
        }
        // 有効解の印を指定する。同じ計算量。expand_answers=falseなら保存だけを行い、展開対象へ登録しない。
        template<class U, class B> requires ActionArg<U, Value> && BoolArg<B>
        bool push(U&& value, Cost cost, B is_answer, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, hash_, false, is_answer, next);
        }
        // Hashを明示する。同じ計算量。同じ保存先内だけで重複排除する。
        template<class U, class H> requires ActionArg<U, Value> && CompatibleHashArg<H, Hash>
        bool push(U&& value, Cost cost, H hash, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, static_cast<Hash>(hash), true, false, next);
        }
        // Hashと有効解の印を指定する。同じ計算量。
        template<class U, class H, class B> requires ActionArg<U, Value> && CompatibleHashArg<H, Hash> && BoolArg<B>
        bool push(U&& value, Cost cost, H hash, B is_answer, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, static_cast<Hash>(hash), true, is_answer, next);
        }

    private:
        friend class Engine<Value>;
        Engine<Value>& engine_;
        Hash hash_;
        Emitter(Engine<Value>& engine, Hash hash) : engine_(engine), hash_(hash) {}
        template<class U>
        bool push_value(U&& value, Cost cost, Hash hash, bool has_hash, bool is_answer, Next next) {
            auto maker = [&]() -> decltype(auto) { return std::forward<U>(value); };
            return engine_.push_candidate(cost, hash, has_hash, is_answer, next, maker);
        }
    };
private:
    // 削除のないepoch表。stale登録が増えたら空きが十分あるうちに生存候補から再構築する。
    class FixedHashMap {
        std::vector<Hash> keys_;
        std::vector<int> values_;
        std::vector<std::uint32_t> epochs_;
        std::uint32_t epoch_ = 1;
        std::size_t mask_ = 0, occupied_ = 0;
    public:
        struct Hit { bool found; std::size_t slot; };
        static constexpr std::size_t absent = std::numeric_limits<std::size_t>::max();
        void init(int width) {
            const auto n = std::bit_ceil(static_cast<std::size_t>(width) * 4 + 17);
            mask_ = n - 1; occupied_ = 0;
            keys_.resize(n); values_.resize(n); epochs_.assign(n, 0);
        }
        void clear() {
            occupied_ = 0;
            if (++epoch_ == 0) { std::fill(epochs_.begin(), epochs_.end(), 0); epoch_ = 1; }
        }
        Hit lookup(Hash h) const {
            auto p = static_cast<std::size_t>(sub_detail::mix(h)) & mask_;
            for (std::size_t i = 0; i <= mask_; ++i, p = (p + 1) & mask_) {
                if (epochs_[p] != epoch_) return {false, occupied_ >= (mask_ + 1) - (mask_ + 1) / 4 ? absent : p};
                if (keys_[p] == h) return {true, p};
            }
            return {false, absent};
        }
        int get(std::size_t p) const { return values_[p]; }
        void set(std::size_t p, Hash h, int slot) {
            assert(p != absent);
            occupied_ += epochs_[p] != epoch_;
            keys_[p] = h; values_[p] = slot; epochs_[p] = epoch_;
        }
    };
    template<class Item> class Selector {
        int width_;
        std::size_t base_;
        bool hashed_;
        std::vector<int> tree_;
        std::vector<Cost> costs_;
        FixedHashMap map_;
        Cost min_cost_{};
        mutable int worst_ = -1;
        int worse(int a, int b) const {
            if (a < 0) return b;
            if (b < 0) return a;
            if (costs_[a] != costs_[b]) return costs_[a] < costs_[b] ? b : a;
            return std::max(a, b);
        }
        void rebuild_hash() {
            map_.clear();
            for (int i = 0; i < size(); ++i) if (items[i].has_hash) {
                const auto h = items[i].hash;
                map_.set(map_.lookup(h).slot, h, i);
            }
        }
    public:
        struct Choice { int slot = -1; std::size_t hash_slot = FixedHashMap::absent; bool duplicate = false; };
        std::vector<Item> items;
        Selector(int width, bool hashed)
            : width_(width), base_(std::bit_ceil(static_cast<std::size_t>(width))), hashed_(hashed) {
            items.reserve(static_cast<std::size_t>(width));
            costs_.resize(static_cast<std::size_t>(width)); tree_.assign(base_ * 2, -1);
            if (hashed_) map_.init(width);
        }
        int size() const { return static_cast<int>(items.size()); }
        bool empty() const { return items.empty(); }
        Cost best_cost() const { assert(!empty()); return min_cost_; }
        Cost worst_cost() const {
            assert(!empty());
            // 満杯前の重複改善で最悪スロットが変わったときだけ遅延再計算する。
            if (worst_ < 0) for (int i = 0; i < size(); ++i) worst_ = worse(worst_, i);
            return costs_[worst_];
        }
        void clear() {
            items.clear(); worst_ = -1;
            if (hashed_) map_.clear();
        }
        // State/Actionを作らずに保存位置を決める。幅判定をhash判定より先に行う。
        Choice choose(Cost cost, Hash h, bool has_hash, CandidateStats& stats) {
            const bool full = size() == width_;
            if (full && !(cost < costs_[worst_])) { ++stats.pruned_by_width; return {}; }
            Choice c;
            if (hashed_ && has_hash) {
                auto hit = map_.lookup(h);
                if (hit.found) {
                    const int i = map_.get(hit.slot);
                    if (i < size() && items[i].has_hash && items[i].hash == h) {
                        if (!(cost < costs_[i])) { ++stats.pruned_by_hash; return {}; }
                        return {i, hit.slot, true};
                    }
                }
                if (hit.slot == FixedHashMap::absent) { rebuild_hash(); hit = map_.lookup(h); }
                assert(hit.slot != FixedHashMap::absent);
                c.hash_slot = hit.slot;
            }
            c.slot = full ? worst_ : size();
            return c;
        }
        void put(Choice c, Item&& item, CandidateStats& stats) {
            const Cost cost = item.cost;
            const bool appended = c.slot == size();
            if (empty() || cost < min_cost_) min_cost_ = cost;
            if (appended) items.push_back(std::move(item));
            else items[c.slot] = std::move(item);
            costs_[c.slot] = cost;
            if (c.hash_slot != FixedHashMap::absent) map_.set(c.hash_slot, items[c.slot].hash, c.slot);
            ++stats.accepted;
            if (c.duplicate) ++stats.replaced_by_hash;
            if (size() == width_) {
                if (appended) {
                    // 未使用の葉は初期値-1のまま。内部節点は下から全て再計算する。
                    for (int i = 0; i < size(); ++i) tree_[base_ + static_cast<std::size_t>(i)] = i;
                    for (std::size_t p = base_; --p > 0;) tree_[p] = worse(tree_[p * 2], tree_[p * 2 + 1]);
                } else {
                    std::size_t p = base_ + static_cast<std::size_t>(c.slot);
                    while ((p /= 2) != 0) tree_[p] = worse(tree_[p * 2], tree_[p * 2 + 1]);
                }
                worst_ = tree_[1];
            } else if (!appended && worst_ == c.slot) worst_ = -1;
            else if (appended && (size() == 1 || worst_ >= 0)) worst_ = worse(worst_, c.slot);
        }
    };
    template<class Value> class Engine {
        // 空の組み込みHookでは、通知用の集計・時計取得も行わない。
        template<class Hook>
        static constexpr bool has_debug_hook =
            !std::same_as<std::remove_cvref_t<Hook>, NoOp>
#ifndef LOCAL
            && !std::same_as<std::remove_cvref_t<Hook>, CsvStatHook>
#endif
            ;
        friend class Emitter<Value>;
#if __INCLUDE_LEVEL__ == 0
        friend struct SubTestAccess;
#endif
        static_assert(std::copy_constructible<Value> && std::assignable_from<Value&, Value>);
        Runtime rt_;
        std::int64_t start_us_ = sub_detail::now_us(), limit_us_ = 0;
        int unchecked_ = 0;
        bool observe_time_ = false;
        bool turn_timing_active_ = false;
        struct Position { int turn = 0; std::int64_t sub_step = 0, depth = 0; };
        Position from_;
        static Param normalize(Param p) {
            p.max_turn = std::max(0, p.max_turn);
            p.beam_width = std::max(1, p.beam_width);
            p.sub_beam_width = p.sub_beam_width <= 0 ? p.beam_width : p.sub_beam_width;
            p.time_check_interval = std::max(1, p.time_check_interval);
            if (!(p.time_limit_ms > 0)) p.time_limit_ms = 0;
            return p;
        }
        void tick(bool force = false) {
            // 観測しない実行でも、確認カウンタを繰り越さない。
            unchecked_ = 0;
            if (!force && limit_us_ == 0 && !observe_time_) return;
            rt_.elapsed_us = sub_detail::now_us() - start_us_;
            if (turn_timing_active_) {
                rt_.turn_elapsed_us = rt_.elapsed_us - rt_.turn_start_us;
                rt_.turn_time_limit_reached |= rt_.elapsed_us >= rt_.turn_deadline_us;
            }
            if (limit_us_ > 0 && rt_.elapsed_us >= limit_us_) rt_.time_limit_reached = true;
        }
        // 外側のturnにつき一度だけ配分。Sub層を進めても同じ締切を使う。
        void begin_turn_time() {
            if (!rt_.param.use_turn_time_limit || limit_us_ == 0) return;
            tick();
            turn_timing_active_ = true;
            rt_.turn_start_us = rt_.elapsed_us;
            rt_.turn_elapsed_us = 0;
            const auto remaining = std::max<std::int64_t>(0, limit_us_ - rt_.elapsed_us);
            rt_.turn_deadline_us = rt_.elapsed_us + remaining / (rt_.param.max_turn - rt_.turn);
            rt_.turn_time_limit_reached = rt_.elapsed_us >= rt_.turn_deadline_us;
        }
        void best_metadata(Cost cost, const Position& p) {
            rt_.found = true; rt_.best_cost = cost; rt_.best_turn = p.turn;
            rt_.best_sub_step = p.sub_step; rt_.best_depth = p.depth;
            ++rt_.best_update_count;
        }
        void stamp_best() {
            if (rt_.first_solution_us < 0) rt_.first_solution_us = rt_.elapsed_us;
            rt_.last_best_update_us = rt_.elapsed_us;
        }
        void after_expand(std::uint64_t old_updates, bool was_expired) {
            ++rt_.expanded_nodes;
            if (was_expired) ++rt_.expanded_after_timeout;
            const bool improved = old_updates != rt_.best_update_count;
            if (improved) { tick(true); stamp_best(); }
            else if (++unchecked_ >= rt_.param.time_check_interval) tick();
        }
        template<class View, class Expand>
        void invoke_expand(Expand& expand, const View& now) {
            Emitter<Value> emit(*this, now.hash);
            if constexpr (std::is_invocable_v<Expand&, const View&, const Runtime&, Emitter<Value>&>) {
                snapshot();
                expand(now, static_cast<const Runtime&>(rt_), emit);
            } else {
                static_assert(std::is_invocable_v<Expand&, const View&, Emitter<Value>&>, "unsupported expand signature");
                expand(now, emit);
            }
        }
        template<class Hook> void notify(EventType event, Hook& hook) {
            if constexpr (has_debug_hook<Hook>) {
                snapshot();
                hook(event, static_cast<const Runtime&>(rt_));
                tick();
            }
        }
        bool destination(Next next, Cost cost, Position& to, CandidateStats*& stats) {
            if (next != Next::Sub && next != Next::Turn) { ++rt_.invalid_candidates; return false; }
            stats = next == Next::Sub ? &rt_.sub_stats : &rt_.turn_stats;
            ++stats->generated;
            if (!sub_detail::valid(cost) || from_.turn >= rt_.param.max_turn) {
                ++rt_.invalid_candidates; return false;
            }
            to.turn = from_.turn + (next == Next::Turn);
            if (to.turn == rt_.param.max_turn) { rt_.completed_max_turn = true; ++rt_.terminal_candidates; }
            return true;
        }
        bool improves(Cost cost, bool is_answer, const Position& to) const {
            return (is_answer || (to.turn == rt_.param.max_turn && rt_.param.max_turn_is_answer)) &&
                (!rt_.found || cost < rt_.best_cost);
        }
        void result_metadata(Result<Value>& result) {
            result.found = rt_.found;
            result.time_limit_reached = rt_.time_limit_reached;
            result.completed_max_turn = rt_.completed_max_turn;
            result.best_cost = rt_.best_cost; result.best_turn = rt_.best_turn;
            result.best_sub_step = rt_.best_sub_step; result.best_depth = rt_.best_depth;
            result.runtime = rt_;
        }

        struct Item {
            Cost cost;
            Hash hash;
            Position pos;
            bool has_hash;
            std::size_t parent;
            Value value;
        };
        using Pool = Selector<Item>;
        std::unique_ptr<Pool> sub_, next_;
        std::vector<Item> nodes_;
        std::vector<std::size_t> route_, buckets_;
        // frontierは整列・回収後に連続して追記し、番号の配列を保持しない。
        std::size_t active_node_ = 0, frontier_first_ = 0, frontier_size_ = 0;
        std::optional<Item> best_tail_;
        std::vector<std::size_t> order_;
        std::size_t reclaim_at_ = 4096;
        std::size_t ordering_root_ = 0; // 現turnの開始候補すべての共通祖先。整列の走査下限。
        // 現在地・候補・最良解から到達できる祖先だけを残す。ノード番号の順序を保つ。
        void collect_nodes() {
            const auto absent = std::numeric_limits<std::size_t>::max();
            buckets_.assign(nodes_.size(), absent);
            auto mark = [&](std::size_t i) {
                while (buckets_[i] == absent) {
                    buckets_[i] = 0;
                    if (i == 0) break;
                    i = nodes_[i].parent;
                }
            };
            mark(active_node_);
            for (const auto* pool : {sub_.get(), next_.get()})
                if (pool) for (const auto& item : pool->items) mark(item.parent);
            if (best_tail_) mark(best_tail_->parent);
            std::size_t kept = 0;
            for (std::size_t i = 0; i < nodes_.size(); ++i) if (buckets_[i] != absent) {
                const auto parent = nodes_[i].parent;
                buckets_[i] = kept;
                if (i != kept) nodes_[kept] = std::move(nodes_[i]);
                nodes_[kept++].parent = buckets_[parent];
            }
            active_node_ = buckets_[active_node_];
            ordering_root_ = buckets_[ordering_root_];
            for (auto* pool : {sub_.get(), next_.get()})
                if (pool) for (auto& item : pool->items) item.parent = buckets_[item.parent];
            if (best_tail_) best_tail_->parent = buckets_[best_tail_->parent];
            nodes_.resize(kept);
            reclaim_at_ = std::max<std::size_t>(4096, kept * 2);
        }
        bool sub_empty() const { return !sub_ || sub_->empty(); }
        bool next_exists() const { return rt_.completed_max_turn || (next_ && !next_->empty()); }
        bool stop_on_timeout() {
            if (!rt_.time_limit_reached || (rt_.param.continue_on_timeout && !rt_.found)) return false;
            rt_.stop_reason = StopReason::TimeLimit;
            return true;
        }
        void peak() {
            const std::size_t n = rt_.frontier_remaining + (sub_ ? sub_->items.size() : 0) + (next_ ? next_->items.size() : 0);
            rt_.peak_pending_candidates = std::max(rt_.peak_pending_candidates, n);
        }
        void snapshot() {
            rt_.sub_candidate_count = sub_ ? sub_->items.size() : 0;
            rt_.next_turn_candidate_count = next_ ? next_->items.size() : 0;
            if (rt_.sub_candidate_count) { rt_.sub_best_cost = sub_->best_cost(); rt_.sub_worst_cost = sub_->worst_cost(); }
            if (rt_.next_turn_candidate_count) { rt_.next_turn_best_cost = next_->best_cost(); rt_.next_turn_worst_cost = next_->worst_cost(); }
            rt_.used_nodes = nodes_.size(); rt_.reserved_nodes = nodes_.capacity();
        }
        template<class Maker>
        bool push_candidate(Cost cost, Hash hash, bool has_hash, bool is_answer, Next next, Maker&& maker) {
            Position to;
            CandidateStats* stats = nullptr;
            if (!destination(next, cost, to, stats)) return false;
            const bool best = improves(cost, is_answer, to);
            Pool* pool = nullptr;
            typename Pool::Choice choice;
            if (to.turn < rt_.param.max_turn) {
                if (!rt_.param.expand_answers && is_answer) ++rt_.answer_only_candidates;
                else {
                    if (next == Next::Sub && !sub_) sub_ = std::make_unique<Pool>(rt_.param.sub_beam_width, rt_.param.use_hash_dedup);
                    pool = next == Next::Sub ? sub_.get() : next_.get();
                    choice = pool->choose(cost, hash, has_hash, *stats);
                }
            }
            if (!best && choice.slot < 0) return false;
            to.sub_step = next == Next::Sub ? from_.sub_step + 1 : 0;
            to.depth = from_.depth + 1;
            // 最良解だけならActionを直接保存し、候補用のコピーを省く
            if (choice.slot < 0) {
                best_tail_.emplace(Item{cost, hash, to, has_hash, active_node_, Value(std::invoke(std::forward<Maker>(maker)))});
                best_metadata(cost, to);
                return false;
            }
            Value made(std::invoke(std::forward<Maker>(maker)));
            if (best) { best_tail_.emplace(Item{cost, hash, to, has_hash, active_node_, made}); best_metadata(cost, to); }
            const bool added = choice.slot == pool->size();
            pool->put(choice, Item{cost, hash, to, has_hash, active_node_, std::move(made)}, *stats);
            if (added) peak();
            return true;
        }
        const Item& item_at(std::size_t i) const { return nodes_[frontier_first_ + i]; }
        void order_mixed_parents(const Pool& pool) {
            // 深さが異なる親も同じ部分木ごとに並べる。親の列を比較のたびにたどらない。
            // 全候補の共通祖先より前は順序に影響しない。長い確定prefixの再走査を避ける。
            const auto first = ordering_root_;
            const auto count = nodes_.size() - first + 1, absent = std::numeric_limits<std::size_t>::max();
            auto parent_of = [&](std::size_t i) {
                const auto v = first + i - 1, parent = nodes_[v].parent;
                return parent < v && parent >= first ? parent - first + 1 : std::size_t{0};
            };
            // 0は複数の根を束ねる仮想根。実ノードは1以降へ対応させる。
            buckets_.assign(count * 2, absent);
            for (auto i = count; --i > 0;) {
                const auto parent = parent_of(i);
                buckets_[count + i] = buckets_[parent];
                buckets_[parent] = i;
            }
            std::size_t rank = 0, node = 0;
            for (;;) {
                const auto child = buckets_[node];
                buckets_[node] = rank++;
                if (child != absent) node = child;
                else {
                    while (node != 0 && buckets_[count + node] == absent) node = parent_of(node);
                    if (node == 0) break;
                    node = buckets_[count + node];
                }
            }
            std::iota(order_.begin(), order_.end(), std::size_t{0});
            std::stable_sort(order_.begin(), order_.end(), [&](auto a, auto b) {
                return buckets_[pool.items[a].parent - first + 1] < buckets_[pool.items[b].parent - first + 1];
            });
            // DFS順で最初と最後の親のLCAは、全親のLCAと一致する。
            auto a = pool.items[order_.front()].parent, b = pool.items[order_.back()].parent;
            while (nodes_[a].pos.depth > nodes_[b].pos.depth) a = nodes_[a].parent;
            while (nodes_[b].pos.depth > nodes_[a].pos.depth) b = nodes_[b].parent;
            while (a != b) { a = nodes_[a].parent; b = nodes_[b].parent; }
            ordering_root_ = a;
        }
        void materialize(Pool& pool, bool by_parent) {
            const std::size_t n = pool.items.size();
            order_.resize(n);
            if (by_parent || std::all_of(pool.items.begin(), pool.items.end(),
                    [&](const auto& item) { return item.parent >= frontier_first_; })) {
                // 同じfrontierの親IDは連続。計数整列で兄弟を隣接させる。
                buckets_.assign(frontier_size_ + 1, 0);
                for (const auto& item : pool.items) ++buckets_[item.parent - frontier_first_ + 1];
                std::partial_sum(buckets_.begin(), buckets_.end(), buckets_.begin());
                for (std::size_t i = 0; i < n; ++i) order_[buckets_[pool.items[i].parent - frontier_first_]++] = i;
            }
            else order_mixed_parents(pool);
            if (nodes_.size() >= reclaim_at_) collect_nodes();
            frontier_size_ = 0; frontier_first_ = nodes_.size();
            for (auto i : order_) {
                nodes_.push_back(std::move(pool.items[i])); ++frontier_size_;
            }
            if (!by_parent && n == 1) ordering_root_ = frontier_first_;
            pool.clear(); rt_.frontier_remaining = frontier_size_;
            rt_.turn = item_at(0).pos.turn; rt_.sub_step = item_at(0).pos.sub_step;
            peak();
        }
        template<class Forward, class Backward>
        void move_to(std::size_t target, Forward& forward, Backward& backward) {
            route_.clear();
            auto back = [&] {
                std::invoke(backward, static_cast<const Value&>(nodes_[active_node_].value));
                ++rt_.move_backward_count; active_node_ = nodes_[active_node_].parent;
            };
            while (nodes_[active_node_].pos.depth > nodes_[target].pos.depth) back();
            while (nodes_[target].pos.depth > nodes_[active_node_].pos.depth) {
                route_.push_back(target); target = nodes_[target].parent;
            }
            while (active_node_ != target) { back(); route_.push_back(target); target = nodes_[target].parent; }
            for (auto it = route_.rbegin(); it != route_.rend(); ++it) {
                std::invoke(forward, static_cast<const Value&>(nodes_[*it].value));
                ++rt_.move_forward_count; active_node_ = *it;
            }
        }
        void reconstruct(Result<Value>& result) {
            if (!rt_.found) return;
            result.turn_offsets.push_back(0);
            if (!best_tail_) return; // T=0の初期解。
            route_.clear();
            for (auto i = best_tail_->parent; i != 0; i = nodes_[i].parent) route_.push_back(i);
            result.path.reserve(route_.size() + 1);
            auto append = [&](const Item& item) {
                result.path.push_back(item.value);
                if (item.pos.turn > nodes_[item.parent].pos.turn) result.turn_offsets.push_back(result.path.size());
            };
            for (auto it = route_.rbegin(); it != route_.rend(); ++it) append(nodes_[*it]);
            append(*best_tail_);
        }
    public:
        Engine(const Param& param, Cost cost, Hash hash, bool has_hash) {
            static_assert(std::default_initializable<Value>, "Action must have a default constructor for the root");
            rt_.param = normalize(param); limit_us_ = sub_detail::to_us(rt_.param.time_limit_ms);
            if (!sub_detail::valid(cost)) { rt_.stop_reason = StopReason::InvalidInitial; return; }
            nodes_.push_back({cost, hash, {}, has_hash, 0, Value{}});
            if (rt_.param.max_turn == 0) {
                rt_.completed_max_turn = true; rt_.stop_reason = StopReason::Completed;
                if (rt_.param.max_turn_is_answer) best_metadata(cost, {});
                return;
            }
            next_ = std::make_unique<Pool>(rt_.param.beam_width, rt_.param.use_hash_dedup);
            frontier_size_ = rt_.frontier_remaining = 1; peak();
        }
        template<class Expand, class Forward, class Backward, class Hook>
        Result<Value> run(Expand&& expand, Forward&& forward, Backward&& backward, Hook&& hook) {
            observe_time_ = has_debug_hook<Hook> ||
                std::is_invocable_v<Expand&, const NodeView&, const Runtime&, Emitter<Value>&>;
            tick(true);
            if (rt_.found) stamp_best();
            notify(EventType::RunStart, hook);
            while (rt_.stop_reason == StopReason::None) {
                begin_turn_time();
                for (;;) {
                    tick();
                    if (stop_on_timeout()) break;
                    const std::size_t n = frontier_size_;
                    rt_.frontier_remaining = n;
                    bool ordered = false;
                    std::size_t i = 0;
                    for (; i < n; ++i) {
                        if (stop_on_timeout()) break;
                        if ((rt_.time_limit_reached || rt_.turn_time_limit_reached) && next_exists()) break;
                        if (rt_.time_limit_reached && !ordered) {
                            order_.resize(n);
                            std::iota(order_.begin(), order_.end(), std::size_t{0});
                            std::stable_sort(order_.begin() + static_cast<std::ptrdiff_t>(i), order_.end(),
                                [&](std::size_t a, std::size_t b) { return item_at(a).cost < item_at(b).cost; });
                            ordered = true;
                        }
                        const std::size_t j = ordered ? order_[i] : i;
                        move_to(frontier_first_ + j, forward, backward);
                        const auto& item = item_at(j);
                        from_ = item.pos;
                        rt_.turn = from_.turn; rt_.sub_step = from_.sub_step;
                        const NodeView now{from_.turn, item.cost, item.hash, item.has_hash, from_.sub_step, from_.depth};
                        const auto old_best = rt_.best_update_count;
                        const bool was_expired = rt_.time_limit_reached;
                        invoke_expand(expand, now);
                        --rt_.frontier_remaining;
                        after_expand(old_best, was_expired);
                    }
                    tick();
                    if (stop_on_timeout()) break;
                    if (i == n) notify(EventType::SubStepEnd, hook);
                    if ((rt_.time_limit_reached || rt_.turn_time_limit_reached) && next_exists()) {
                        if (rt_.time_limit_reached && (rt_.frontier_remaining != 0 || !sub_empty())) ++rt_.timeout_turn_advances;
                        break;
                    }
                    if (sub_empty()) break;
                    materialize(*sub_, true);
                }
                turn_timing_active_ = false;
                if (rt_.stop_reason != StopReason::None) break;
                notify(EventType::TurnEnd, hook);
                if (rt_.completed_max_turn) rt_.stop_reason = StopReason::Completed;
                else if (next_->empty()) rt_.stop_reason = StopReason::Exhausted;
                else {
                    if (sub_) sub_->clear();
                    materialize(*next_, false);
                }
            }
            Result<Value> result;
            if (!nodes_.empty()) move_to(0, forward, backward);
            reconstruct(result);
            tick(true); snapshot();
            hook(EventType::RunEnd, static_cast<const Runtime&>(rt_));
            result_metadata(result);
            return result;
        }
    };
public:
    // 選別は期待O(G log(2+max(W,U)))。状態移動、境界整列、Node回収、path復元の費用を加える。
    // 正常な返却時には外部Stateを初期状態へ戻す。pathは最良解への全Action列。
    template<class Action, class H, class Expand, class MoveForward, class MoveBackward, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<Action> run(const Param& param, Cost initial_cost, H initial_hash, Expand&& expand,
                              MoveForward&& move_forward, MoveBackward&& move_backward,
                              DebugHook&& debug_hook = DebugHook{}) {
        Engine<Action> engine(param, initial_cost, static_cast<Hash>(initial_hash), true);
        return engine.run(std::forward<Expand>(expand), std::forward<MoveForward>(move_forward),
                          std::forward<MoveBackward>(move_backward), std::forward<DebugHook>(debug_hook));
    }
    // 初期Hashの省略形。生成候補の重複排除はHash付きpushで指定する。
    template<class Action, class Expand, class MoveForward, class MoveBackward, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<Action> run(const Param& param, Cost initial_cost, Expand&& expand,
                              MoveForward&& move_forward, MoveBackward&& move_backward,
                              DebugHook&& debug_hook = DebugHook{}) {
        Engine<Action> engine(param, initial_cost, Hash{}, false);
        return engine.run(std::forward<Expand>(expand), std::forward<MoveForward>(move_forward),
                          std::forward<MoveBackward>(move_backward), std::forward<DebugHook>(debug_hook));
    }

};
} // namespace bs

#if __INCLUDE_LEVEL__ == 0
// ここからはヘッダを直接コンパイルした場合だけの検証・計測コード。
namespace sub_test {
constexpr int mode = 1; // 0=copy, 1=delta, 2=chokudai
using B = bs::Beam<>;
using Next = B::Next;
using Reason = B::StopReason;
using Event = B::EventType;
inline void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Rng {
    std::uint64_t state;
    std::uint64_t next() { state += 0x9e3779b97f4a7c15ULL; return bs::sub_detail::mix(state); }
    int pick(int n) { return static_cast<int>(next() % static_cast<unsigned>(n)); }
};
struct Edge {
    int to; long long cost; bool turn = false, is_answer = false, hashed = true;
    std::uint64_t hash = 0;
    std::int64_t maker_us = 0;
};
struct Graph {
    std::vector<std::vector<Edge>> edges;
    std::vector<std::int64_t> expand_us;
    std::int64_t hook_start_us = 0, hook_step_us = 0, hook_end_us = 0;
    explicit Graph(int n) : edges(static_cast<std::size_t>(n)), expand_us(static_cast<std::size_t>(n)) {}
    void add(int from, int to, long long cost, bool turn = false, bool is_answer = false, bool hashed = true, std::uint64_t hash = UINT64_MAX) {
        edges[from].push_back({to, cost, turn, is_answer, hashed, hash == UINT64_MAX ? static_cast<std::uint64_t>(to) : hash, 0});
    }
};
struct State { int id = 0; std::vector<int> path; };
struct Action { int from = 0, to = 0, edge = 0; bool turn = false; };
struct Trace {
    int id, turn; std::int64_t sub, depth; long long cost;
    std::uint64_t hash; bool hashed;
    std::vector<int> path;
    bool operator==(const Trace&) const = default;
};
struct Outcome {
    B::Runtime rt;
    std::vector<Trace> trace;
    std::vector<bool> pushes;
    std::vector<int> best_path;
    int best_id = -1;
    std::uint64_t makers = 0, forward = 0, backward = 0;
    std::vector<Event> events;
};
struct RefNode {
    State state;
    long long cost = 0;
    std::uint64_t hash = 0, serial = 0;
    bool hashed = true;
    int turn = 0;
    std::int64_t sub = 0, depth = 0;
    std::vector<std::size_t> node_path{0};
};
// 線形走査だけの独立選別器。比較木・ハッシュ表・ノードarenaを使わない。
struct RefPool {
    int width, count = 0;
    bool queue, dedup;
    std::vector<std::optional<RefNode>> slots;
    std::vector<int> free;
    RefPool(int w, bool q, bool h) : width(w), queue(q), dedup(h) {}
    bool better(int a, int b) const {
        if (slots[a]->cost != slots[b]->cost) return slots[a]->cost < slots[b]->cost;
        return queue ? slots[a]->serial < slots[b]->serial : a < b;
    }
    int best() const {
        int x = -1;
        for (int i = 0; i < static_cast<int>(slots.size()); ++i) if (slots[i] && (x < 0 || better(i, x))) x = i;
        return x;
    }
    int worst() const {
        int x = -1;
        for (int i = 0; i < static_cast<int>(slots.size()); ++i) if (slots[i] && (x < 0 || better(x, i))) x = i;
        return x;
    }
    std::pair<int,bool> choose(const RefNode& x, B::CandidateStats& stats) const {
        if (count == width && !(x.cost < slots[worst()]->cost)) { ++stats.pruned_by_width; return {-1, false}; }
        if (dedup && x.hashed) for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
            if (slots[i] && slots[i]->hashed && slots[i]->hash == x.hash) {
                if (!(x.cost < slots[i]->cost)) { ++stats.pruned_by_hash; return {-1, false}; }
                return {i, true};
            }
        }
        return {count == width ? worst() : (!free.empty() ? free.back() : static_cast<int>(slots.size())), false};
    }
    void put(int i, RefNode x) {
        if (i == static_cast<int>(slots.size())) slots.emplace_back();
        if (!slots[i]) { ++count; if (!free.empty()) { require(free.back() == i, "reference free list"); free.pop_back(); } }
        slots[i] = std::move(x);
    }
    RefNode pop() {
        const int i = best(); RefNode x = std::move(*slots[i]); slots[i].reset(); --count; free.push_back(i); return x;
    }
    std::vector<RefNode> drain() {
        std::vector<RefNode> result;
        for (auto& n : slots) if (n) result.push_back(std::move(*n));
        clear(); return result;
    }
    void clear() { slots.clear(); free.clear(); count = 0; }
};
struct Reference {
    B::Param p;
    const Graph& graph;
    Outcome out;
    std::int64_t clock = 0, last_clock = 0;
    int unchecked = 0;
    bool terminal = false;
    std::uint64_t serial = 1;
    std::size_t nodes = 1, node_serial = 1, reclaim_at = 4096;
    std::vector<std::size_t> previous_nodes{0}, best_nodes;
    std::vector<int> previous_path;
    std::vector<RefNode> frontier;
    RefPool sub, next;
    std::vector<RefPool> layers;
    Reference(B::Param param, const Graph& g) : p(param), graph(g),
        sub(std::max(1, p.sub_beam_width <= 0 ? p.beam_width : p.sub_beam_width), mode == 2, p.use_hash_dedup),
        next(std::max(1, p.beam_width), mode == 2, p.use_hash_dedup) {
        p.max_turn = std::max(0,p.max_turn); p.beam_width=std::max(1,p.beam_width);
        p.sub_beam_width=sub.width; p.time_check_interval=std::max(1,p.time_check_interval);
        out.rt.param=p;
        if constexpr(mode==2) for(int t=0;t<p.max_turn;++t) layers.emplace_back(p.beam_width,true,p.use_hash_dedup);
    }
    void tick() {
        last_clock=clock; unchecked=0;
        if(p.time_limit_ms>0 && clock>=bs::sub_detail::to_us(p.time_limit_ms)) out.rt.time_limit_reached=true;
    }
    void event(Event e) {
        out.events.push_back(e);
        if(e==Event::RunStart) clock+=graph.hook_start_us;
        if(e==Event::SubStepEnd) clock+=graph.hook_step_us;
        if(e==Event::RunEnd) clock+=graph.hook_end_us;
        if(e!=Event::RunEnd) tick();
    }
    void stamp() {
        if(out.rt.first_solution_us<0)out.rt.first_solution_us=clock;
        out.rt.last_best_update_us=clock;
    }
    void save(const RefNode& n) {
        out.rt.found=true;out.rt.best_cost=n.cost;out.rt.best_turn=n.turn;
        out.rt.best_sub_step=n.sub;out.rt.best_depth=n.depth;++out.rt.best_update_count;
        out.best_id=n.state.id;out.best_path=n.state.path;best_nodes=n.node_path;
    }
    void expand(const RefNode& parent, std::size_t, RefPool& target) {
        previous_nodes=parent.node_path;
        out.rt.turn=parent.turn;out.rt.sub_step=parent.sub;
        out.trace.push_back({parent.state.id,parent.turn,parent.sub,parent.depth,parent.cost,parent.hash,parent.hashed,parent.state.path});
        if constexpr(mode==1) {
            std::size_t common=0;
            while(common<previous_path.size()&&common<parent.state.path.size()&&previous_path[common]==parent.state.path[common])++common;
            out.backward+=previous_path.size()-common;out.forward+=parent.state.path.size()-common;previous_path=parent.state.path;
        }
        const auto old=out.rt.best_update_count;
        const bool expired=out.rt.time_limit_reached;
        const auto& edges=graph.edges[parent.state.id];
        for(int j=0;j<static_cast<int>(edges.size());++j) {
            const auto& e=edges[j];
            auto& s=e.turn?out.rt.turn_stats:out.rt.sub_stats;++s.generated;
            RefNode child=parent;child.state.id=e.to;child.state.path.push_back(parent.state.id*1000+j);
            child.cost=e.cost;child.hash=e.hashed?e.hash:parent.hash;child.hashed=e.hashed;
            child.turn+=e.turn;child.sub=e.turn?0:parent.sub+1;++child.depth;
            const bool end=child.turn==p.max_turn;
            if(end){terminal=true;out.rt.completed_max_turn=true;++out.rt.terminal_candidates;}
            const bool best=(e.is_answer||(end&&p.max_turn_is_answer))&&(!out.rt.found||child.cost<out.rt.best_cost);
            auto& pool=e.turn?target:sub;
            const bool answer_only=e.is_answer&&!p.expand_answers&&!end;
            if(answer_only)++out.rt.answer_only_candidates;
            auto choice=(end||answer_only)?std::pair{-1,false}:pool.choose(child,s);
            const bool accepted=best||choice.first>=0;
            out.pushes.push_back(choice.first>=0);
            if(!accepted)continue;
            ++out.makers; clock+=e.maker_us;
            if(best)save(child);
            if(choice.first>=0){child.serial=serial++;pool.put(choice.first,std::move(child));++s.accepted;if(choice.second)++s.replaced_by_hash;}
        }
        clock+=graph.expand_us[parent.state.id];++out.rt.expanded_nodes;
        if(expired)++out.rt.expanded_after_timeout;
        if(old!=out.rt.best_update_count){tick();stamp();}
        else if(++unchecked>=p.time_check_interval)tick();
    }
    bool materialize(RefPool& pool,bool grouped) {
        const auto previous_first=frontier.front().node_path.back();
        frontier=pool.drain();
        if constexpr(mode==1) {
            (void)grouped;
            const bool recent=grouped||std::all_of(frontier.begin(),frontier.end(),[&](const auto& n){return n.node_path.back()>=previous_first;});
            std::stable_sort(frontier.begin(),frontier.end(),[&](const auto&a,const auto&b){
                return recent?a.node_path.back()<b.node_path.back():a.node_path<b.node_path;
            });
            if(nodes>=reclaim_at) {
                std::set<std::size_t> live(previous_nodes.begin(),previous_nodes.end());
                live.insert(best_nodes.begin(),best_nodes.end());
                for(const auto& n:frontier)live.insert(n.node_path.begin(),n.node_path.end());
                for(const auto* q:{&sub,&next})for(const auto& n:q->slots)
                    if(n)live.insert(n->node_path.begin(),n->node_path.end());
                nodes=live.size();reclaim_at=std::max<std::size_t>(4096,nodes*2);
            }
            for(std::size_t i=0;i<frontier.size();++i){
                ++nodes;frontier[i].node_path.push_back(node_serial++);
            }
        }
        out.rt.turn=frontier[0].turn;out.rt.sub_step=frontier[0].sub;return true;
    }
    bool stop_timed() {
        if(out.rt.time_limit_reached&&(!p.continue_on_timeout||out.rt.found)){out.rt.stop_reason=Reason::TimeLimit;return true;}return false;
    }
    void beam() {
        while(out.rt.stop_reason==Reason::None){
            terminal=false;
            for(;;){
                tick();if(stop_timed())return;
                std::vector<std::size_t> indices(frontier.size());std::iota(indices.begin(),indices.end(),std::size_t{0});
                bool sorted=false;std::size_t i=0;
                for(;i<indices.size();++i){
                    if(stop_timed())return;
                    if(out.rt.time_limit_reached&&(next.count||terminal))break;
                    if(out.rt.time_limit_reached&&!sorted){std::stable_sort(indices.begin()+static_cast<std::ptrdiff_t>(i),indices.end(),[&](auto a,auto b){return frontier[a].cost<frontier[b].cost;});sorted=true;}
                    expand(frontier[indices[i]],indices[i],next);
                }
                tick();if(stop_timed())return;if(i==indices.size())event(Event::SubStepEnd);
                if(out.rt.time_limit_reached&&(next.count||terminal)){
                    if(i<indices.size()||sub.count)++out.rt.timeout_turn_advances;
                    break;
                }
                if(!sub.count)break;
                if(!materialize(sub,true))return;
            }
            event(Event::TurnEnd);
            if(terminal){out.rt.stop_reason=Reason::Completed;return;}
            if(!next.count){out.rt.stop_reason=Reason::Exhausted;return;}
            sub.clear();if(!materialize(next,false))return;
        }
    }
    Outcome run() {
        RefNode root;root.state.id=0;
        if(p.max_turn==0){out.rt.completed_max_turn=true;out.rt.stop_reason=Reason::Completed;if(p.max_turn_is_answer)save(root);}
        else if constexpr(mode==2)layers[0].put(0,root);
        else frontier.push_back(root);
        tick();if(out.rt.found)stamp();event(Event::RunStart);tick();
        if(out.rt.stop_reason==Reason::None){
            beam();
        }
        if constexpr(mode==1)out.backward+=previous_path.size();
        tick();out.rt.elapsed_us=last_clock;event(Event::RunEnd);return std::move(out);
    }
};

Outcome actual(const B::Param& p,const Graph& graph) {
    bs::sub_detail::TestClock::enabled=true;bs::sub_detail::TestClock::value=0;
    Outcome out;
    State world;
    std::vector<int> payload{17,31,43};
    struct Undo{int pos;std::uint64_t checksum;};
    std::vector<Undo> undo;
    std::uint64_t checksum=19;
    auto forward=[&](const Action& a){
        require(world.id==a.from,"forward source");
        const int pos=(a.edge+world.id)%static_cast<int>(payload.size()+1);
        undo.push_back({pos,checksum});payload.insert(payload.begin()+pos,a.edge);
        checksum=std::rotl(checksum,7)^bs::sub_detail::mix(static_cast<std::uint64_t>(a.edge));
        world.id=a.to;world.path.push_back(a.edge);++out.forward;
    };
    auto backward=[&](const Action& a){
        require(world.id==a.to&&!world.path.empty()&&world.path.back()==a.edge,"backward order");
        const auto u=undo.back();undo.pop_back();require(payload[u.pos]==a.edge,"noncommutative undo");
        payload.erase(payload.begin()+u.pos);checksum=u.checksum;world.id=a.from;world.path.pop_back();++out.backward;
    };
    auto hook=[&](Event e,const B::Runtime& r){
        out.events.push_back(e);
        require(r.sub_candidate_count<=static_cast<std::size_t>(r.param.sub_beam_width),"Sub width");
        require(r.next_turn_candidate_count<=static_cast<std::size_t>(r.param.beam_width),"Turn width");
        if(r.sub_candidate_count)require(r.sub_best_cost<=r.sub_worst_cost,"Sub cost range");
        if(r.next_turn_candidate_count)require(r.next_turn_best_cost<=r.next_turn_worst_cost,"Turn cost range");
        if(e==Event::RunStart)bs::sub_detail::TestClock::value+=graph.hook_start_us;
        if(e==Event::SubStepEnd)bs::sub_detail::TestClock::value+=graph.hook_step_us;
        if(e==Event::RunEnd)bs::sub_detail::TestClock::value+=graph.hook_end_us;
    };
    auto expand=[&](const auto& now,const B::Runtime& rt,auto& emit){
        const State& state=[&]() -> const State& {
            if constexpr(mode==1)return world;
            else return now.state;
        }();
        require(now.turn==rt.turn&&now.sub_step==rt.sub_step,"Runtime coordinate");
        require(now.depth==static_cast<std::int64_t>(state.path.size()),"depth coordinate");
        out.trace.push_back({state.id,now.turn,now.sub_step,now.depth,now.cost,now.hash,now.has_hash,state.path});
        const auto& edges=graph.edges[state.id];
        for(int j=0;j<static_cast<int>(edges.size());++j){
            const auto& e=edges[j];const auto next=e.turn?Next::Turn:Next::Sub;
            bool accepted=false;
            const auto before_updates=rt.best_update_count;
            Action a{state.id,e.to,state.id*1000+j,e.turn};
            if(e.hashed)accepted=emit.push(a,e.cost,e.hash,e.is_answer,next);
            else accepted=emit.push(a,e.cost,e.is_answer,next);
            if(accepted||rt.best_update_count!=before_updates){++out.makers;bs::sub_detail::TestClock::value+=e.maker_us;}
            out.pushes.push_back(accepted);
        }
        bs::sub_detail::TestClock::value+=graph.expand_us[state.id];
    };
    auto result=B::run<Action>(p,0,0ULL,expand,forward,backward,hook);
    require(world.id==0&&world.path.empty()&&undo.empty()&&payload==std::vector<int>({17,31,43})&&checksum==19,"root restore");
    if(result.found){
        out.best_id=0;std::size_t turns=0;std::vector<std::size_t> offsets{0};
        for(const auto&a:result.path){require(out.best_id==a.from,"returned path continuity");out.best_id=a.to;out.best_path.push_back(a.edge);if(a.turn){++turns;offsets.push_back(out.best_path.size());}}
        require(offsets==result.turn_offsets,"turn offsets");require(turns==static_cast<std::size_t>(result.best_turn),"turn count");
        require(result.path.size()==static_cast<std::size_t>(result.best_depth),"path length");
    }else require(result.path.empty()&&result.turn_offsets.empty(),"empty failure path");
    require(out.forward==result.runtime.move_forward_count&&out.backward==result.runtime.move_backward_count,"move counters");
    out.rt=result.runtime;
    require(result.found==out.rt.found&&result.time_limit_reached==out.rt.time_limit_reached,"result snapshot");
    bs::sub_detail::TestClock::enabled=false;
    return out;
}
inline bool stats_equal(const B::CandidateStats&a,const B::CandidateStats&b){
    return std::tie(a.generated,a.accepted,a.pruned_by_width,a.pruned_by_hash,a.replaced_by_hash)==
           std::tie(b.generated,b.accepted,b.pruned_by_width,b.pruned_by_hash,b.replaced_by_hash);
}
Outcome check(const B::Param& p,const Graph& graph,const std::string& name){
    auto expected=Reference(p,graph).run();auto got=actual(p,graph);
    try{
        require(got.trace==expected.trace,"expand trace");require(got.pushes==expected.pushes,"push acceptance");
        require(got.best_id==expected.best_id&&got.best_path==expected.best_path,"best state/path");
        require(got.makers==expected.makers,"lazy construction count");
        const auto&a=got.rt;const auto&b=expected.rt;
        require(a.found==b.found&&a.completed_max_turn==b.completed_max_turn,"solution flags");
        require(a.stop_reason==b.stop_reason&&a.time_limit_reached==b.time_limit_reached,"stop status");
        require(a.expanded_nodes==b.expanded_nodes&&a.expanded_after_timeout==b.expanded_after_timeout&&a.timeout_turn_advances==b.timeout_turn_advances,"expansion counters");
        require(a.best_update_count==b.best_update_count&&(!a.found||std::tie(a.best_cost,a.best_turn,a.best_sub_step,a.best_depth)==std::tie(b.best_cost,b.best_turn,b.best_sub_step,b.best_depth)),"best metadata");
        require(a.first_solution_us==b.first_solution_us&&a.last_best_update_us==b.last_best_update_us,"solution timestamps");
        require(a.answer_only_candidates==b.answer_only_candidates,"answer-only counter");
        require(stats_equal(a.sub_stats,b.sub_stats)&&stats_equal(a.turn_stats,b.turn_stats),"selection counters");
        require(got.events==expected.events,"event sequence");
        require(got.forward==expected.forward&&got.backward==expected.backward,"LCA move distance");
    }catch(const std::exception&e){std::cerr<<"case="<<name<<" got_exp="<<got.rt.expanded_nodes<<" ref_exp="<<expected.rt.expanded_nodes<<"\n";throw std::runtime_error(name+": "+e.what());}
    return got;
}
Graph random_graph(std::uint64_t seed,int turns,int states=5,int levels=4){
    Rng rng{seed};const int layer_size=states*levels;Graph g((turns+1)*layer_size);
    for(int t=0;t<turns;++t)for(int l=0;l<levels;++l)for(int s=0;s<states;++s){
        const int id=t*layer_size+l*states+s;
        const int count=rng.pick(6);
        for(int j=0;j<count;++j){
            const bool turn=l+1==levels||rng.pick(3)==0;
            const int to=turn?(t+1)*layer_size+rng.pick(states):(t*layer_size+(l+1+rng.pick(levels-l-1))*states+rng.pick(states));
            g.add(id,to,rng.pick(41)-20,turn,rng.pick(17)==0,rng.pick(5)!=0);
            g.edges[id].back().maker_us=rng.pick(3);
        }
        g.expand_us[id]=rng.pick(6);
    }
    return g;
}
template<class P>void choku_settings(P&p,int width,std::int64_t sweeps){if constexpr(requires{p.sweep_width;p.max_sweeps;}){p.sweep_width=width;p.max_sweeps=sweeps;}}

void basic_cases(){
    B::Param p;p.max_turn=2;p.beam_width=2;p.sub_beam_width=2;
    Graph g(8);g.add(0,1,12,true);g.add(0,2,100);g.add(2,3,8,true);g.add(2,4,0);g.add(4,5,5,true);
    g.add(1,6,12,true);g.add(3,6,8,true);g.add(5,7,5,true);
    auto r=check(p,g,"F07 long-short boundary");require(r.rt.best_cost==5,"hand solution");
    for(int w:{0,1,2,3,7,8,9,31,32,33})for(int u:{0,1,8}){p.beam_width=w;p.sub_beam_width=u;check(p,g,"F02-F03 widths");}
    p.beam_width=1;p.sub_beam_width=1;p.max_turn=1;
    Graph saved(4);saved.add(0,1,-100);saved.add(0,2,10,false,true);saved.add(1,3,-200);
    r=check(p,saved,"F18 archived outside beam");require(r.rt.found&&r.rt.best_cost==10,"archive independent");
    p.max_turn=0;check(p,saved,"F01 zero turns");p.max_turn=-1;p.max_turn_is_answer=false;check(p,saved,"F01 negative turns no answer");
    p.max_turn=2;p.max_turn_is_answer=true;p.beam_width=8;p.sub_beam_width=3;
    Graph duplicates(10);
    for(int j=0;j<4000;++j)duplicates.add(0,1+j%4,4000-j,j%2!=0,j%101==0,true,static_cast<std::uint64_t>(j%17));
    for(int i=1;i<=4;++i)duplicates.add(i,5+i,0,true);
    check(p,duplicates,"F11-F16 replacements collisions ties");p.use_hash_dedup=false;check(p,duplicates,"F15 dedup disabled");
    p.use_hash_dedup=true;
    for(int interval:{1,2,64,1024})for(int limit:{1,3,8,15}){
        p.time_check_interval=interval;p.time_limit_ms=limit/1000.0;
        for(int i=0;i<8;++i)g.expand_us[i]=2;
        check(p,g,"T02-T05 deadline boundary");
    }
    p.time_limit_ms=0.001;p.time_check_interval=64;
    Graph dead(7);dead.hook_start_us=1;dead.add(0,1,-10);dead.add(0,2,-9);dead.add(2,3,-8);dead.add(3,4,-7,true);dead.add(4,5,-6,true);
    r=check(p,dead,"T01-T04 rescue dead best");require(r.rt.found,"rescue retains alternatives");
    p.time_limit_ms=0;p.max_turn_is_answer=false;Graph none(2);none.add(0,1,3,true);p.max_turn=1;
    r=check(p,none,"F20 terminal is not answer");require(r.rt.completed_max_turn&&!r.rt.found,"no fabricated answer");
    none.hook_end_us=1000;p.time_limit_ms=0.01;check(p,none,"T15 RunEnd excluded");
    p.max_turn=2;p.max_turn_is_answer=true;p.time_limit_ms=0;
}
void random_cases(int count){
    for(int seed=0;seed<count;++seed){
        Rng rng{static_cast<std::uint64_t>(seed)+101};B::Param p;
        p.max_turn=rng.pick(7);p.beam_width=1+rng.pick(12);p.sub_beam_width=1+rng.pick(12);
        p.max_turn_is_answer=rng.pick(4)!=0;p.use_hash_dedup=rng.pick(5)!=0;
        p.continue_on_timeout=seed%4!=0;p.expand_answers=seed%5!=0;
        p.time_check_interval=1+rng.pick(8);choku_settings(p,1+rng.pick(6),rng.pick(4));
        if(seed%3==0)p.time_limit_ms=(1+rng.pick(150))/1000.0;
        auto g=random_graph(static_cast<std::uint64_t>(seed)+7000,p.max_turn);
        if(seed%11==0)g.hook_start_us=200;
        if(seed%13==0)g.hook_step_us=3;
        check(p,g,"random seed "+std::to_string(seed));
    }
}

// 枝刈りが起きないことを実測した上で、スケジューラを使わない全経路列挙と比較する。
void exhaustive_cases(int count) {
    for (int seed=0;seed<count;++seed) {
        B::Param p;p.max_turn=1+seed%3;p.beam_width=256;p.sub_beam_width=256;
        p.use_hash_dedup=false;p.max_turn_is_answer=seed%4!=0;choku_settings(p,256,0);
        auto g=random_graph(static_cast<std::uint64_t>(seed)+50000,p.max_turn,2,2);
        for(auto& edges:g.edges) if(edges.size()>2)edges.resize(2);
        auto r=actual(p,g);
        bool found=false;long long best=0;
        auto dfs=[&](auto&& self,int id,int t)->void {
            for(const auto&e:g.edges[id]) {
                const int nt=t+e.turn;
                if(e.is_answer||(nt==p.max_turn&&p.max_turn_is_answer)) {
                    if(!found||e.cost<best){found=true;best=e.cost;}
                }
                if(nt<p.max_turn)self(self,e.to,nt);
            }
        };
        dfs(dfs,0,0);
        require(r.rt.sub_stats.pruned_by_width==0&&r.rt.turn_stats.pruned_by_width==0,"exhaustive no width pruning");
        require(r.rt.found==found&&(!found||r.rt.best_cost==best),"exhaustive optimum");
    }
}

template<class Emit,class V> concept WrongHash = requires(Emit&e,V v){e.push(v,0,-1);};
template<class Emit,class V> concept WrongValue = requires(Emit&e,V v){e.push(v,0);};
static_assert(!WrongHash<B::Emitter<int>,int>);
static_assert(!WrongValue<B::Emitter<int>,std::string>);
static_assert(bs::CostType<int> && bs::CostType<double> && !bs::CostType<unsigned>);
static_assert(bs::HashType<std::uint32_t> && !bs::HashType<bool> && !bs::HashType<long long>);

struct Counted {
    inline static int copies=0,alive=0;
    int value=0;
    Counted(){++alive;}
    explicit Counted(int v):value(v){++alive;}
    Counted(const Counted&v):value(v.value){++copies;++alive;}
    Counted(Counted&&v)noexcept:value(v.value){++alive;}
    Counted&operator=(const Counted&v){value=v.value;++copies;return *this;}
    Counted&operator=(Counted&&v)noexcept{value=v.value;return *this;}
    ~Counted(){--alive;}
};
struct NonDefault {
    int value;
    NonDefault()=delete;
    explicit NonDefault(int v):value(v){}
};

void api_cases() {
    B::Param p;p.max_turn=2;p.beam_width=32;p.sub_beam_width=32;
    auto calls=[&](const auto& now, B::Emitter<int>& emit) {
        if(now.turn!=0||now.sub_step>0)return;
        // 既存4形式、Nextを末尾に付けた4形式、非const/const/rvalueを使う。
        int a=1;const int b=2;
        require(emit.push(a,1),"push plain");
        require(emit.push(b,0,true),"push is_answer");
        require(emit.push(3,-1,3U),"push hash");
        require(emit.push(4,-2,4ULL,true),"push both");
        require(emit.push(5,5,Next::Sub),"push Sub plain");
        require(emit.push(6,6,false,Next::Sub),"push Sub is_answer");
        require(emit.push(7,7,7U,Next::Sub),"push Sub hash");
        require(emit.push(8,8,8ULL,false,Next::Sub),"push Sub both");
        require(!emit.push(9,0,static_cast<Next>(37)),"invalid Next");
    };
    auto result=B::run<int>(p,0,0ULL,calls,[](int){},[](int){});
    require(result.found&&result.runtime.invalid_candidates==(mode==1?1U:2U),"API result");
    // 既に満杯の継続先で落ちる通常候補を内部コピーしない。
    p.max_turn=2;p.beam_width=1;p.sub_beam_width=1;Counted::copies=0;
    {
        auto expand=[](const auto&now,auto&e){
            if(now.turn!=0||now.sub_step!=0)return;
            e.push(Counted{1},-100,Next::Sub);
            const int old=Counted::copies;Counted rejected{2};
            require(!e.push(rejected,100,Next::Sub)&&Counted::copies==old,"reject without internal copy");
            require(!e.push(Counted{4},200,true,Next::Sub),"archive despite width");
            require(Counted::copies==old,"best-only Action is not copied");
        };
        auto r=B::run<Counted>(p,0,0ULL,expand,[](const Counted&){},[](const Counted&){});
        require(r.path.size()==1&&r.path[0].value==4,"archive Action lifetime");
    }
    require(Counted::alive==0,"State/Action lifetime balance");
}

void floating_cases() {
    using F=bs::Beam<double>;using N=F::Next;
    F::Param p;p.max_turn=1;p.beam_width=-1;p.sub_beam_width=-2;p.time_check_interval=0;
    p.time_limit_ms=std::numeric_limits<double>::quiet_NaN();
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    auto expand=[&](const auto&now,auto&e){
        if(now.sub_step!=0)return;
        require(!e.push(1,nan,N::Sub),"reject NaN");
        require(e.push(2,inf,N::Sub),"accept positive infinity");
        require(e.push(3,0.0,N::Sub),"zero improves infinity");
        require(!e.push(4,-0.0,N::Sub),"signed zero tie");
        require(e.push(5,-inf,true,N::Sub),"negative infinity valid answer");
        require(!e.push(6,inf,true,N::Sub),"no false infinity update");
    };
    int events=0;auto hook=[&](auto,const auto&){++events;};
    auto r=F::run<int>(p,0,0U,expand,[](int){},[](int){},hook);
    auto invalid=F::run<int>(p,nan,0U,expand,[](int){},[](int){},hook);
    require(r.found&&r.best_cost==-inf,"floating best");
    require(r.runtime.param.beam_width==1&&r.runtime.param.sub_beam_width==1&&r.runtime.param.time_check_interval==1&&r.runtime.param.time_limit_ms==0,"normalization");
    require(!invalid.found&&invalid.runtime.stop_reason==F::StopReason::InvalidInitial&&invalid.runtime.expanded_nodes==0,"NaN initial");
    require(events>=4,"invalid initial events");
}

void deep_case() {
    B::Param p;p.max_turn=2;p.beam_width=1;p.sub_beam_width=1;
    int world=0;
    auto expand=[&](const auto&now,auto&e){
        const auto expected=now.turn==0?now.sub_step:10001+now.sub_step;
        require(world==expected,"deep world");
        const auto to=now.sub_step==(now.turn==0?10000:2)?Next::Turn:Next::Sub;
        e.push(1,-now.depth-1,to);
    };
    auto r=B::run<int>(p,0,0U,expand,[&](int v){world+=v;},[&](int v){world-=v;});
    require(world==0&&r.path.size()==10004&&r.turn_offsets==std::vector<std::size_t>({0,10001,10004}),"deep path/root");
    require(r.found&&r.best_depth==10004,"deep iterative search");
}

void retention_and_transform_cases() {
    B::Param p;p.max_turn=2;p.beam_width=2;p.sub_beam_width=2;
    Graph g(110);g.add(0,101,10,true);g.add(0,1,100);
    for(int i=1;i<100;++i)g.add(i,i+1,100-i);
    g.add(100,102,-10,true);g.add(101,103,10,true);g.add(102,104,-10,true);
    auto r=check(p,g,"F08-F09 D01-D02 old boundary and different depth");
    require(r.rt.best_cost==-10&&r.rt.best_depth==102,"late exit retained");
    auto transformed=g;for(auto&es:transformed.edges)for(auto&e:es){e.cost=e.cost*3+17;e.hash=e.hash*17+31;}
    auto changed=check(p,transformed,"affine cost and bijective hash");
    require(r.best_path==changed.best_path&&r.rt.expanded_nodes==changed.rt.expanded_nodes&&r.rt.best_cost*3+17==changed.rt.best_cost,"metamorphic result");
    p.time_limit_ms=0.001;g.hook_start_us=1;g.edges[100][0].maker_us=9;
    check(p,g,"T12 old boundary timeout");
}

void compound_cases() {
    constexpr int turns=4,levels=160,states=5,block=levels*states;
    Graph g((turns+1)*block);
    for(int t=0;t<turns;++t)for(int l=0;l<levels;++l)for(int s=0;s<states;++s) {
        const int id=t*block+l*states+s;g.expand_us[id]=2;
        if(l+1<levels)for(int j=0;j<3;++j) {
            const int to=t*block+(l+1)*states+(s+j)%states;
            g.add(id,to,10000-l*10+j+7);g.add(id,to,10000-l*10+j);
        }
        if(l==0||l==levels-1){
            const int to=(t+1)*block+s;
            g.add(id,to,1000-l*20+s,true);
        }
        if(l==70)g.add(id,t*block+(l+1)*states+s,100000,false,true);
    }
    B::Param p;p.max_turn=turns;p.beam_width=4;p.sub_beam_width=5;choku_settings(p,2,3);
    for(double limit:{0.0,0.001,0.1,1.0}) {
        p.time_limit_ms=limit;p.time_check_interval=8;
        check(p,g,"compound long Sub/merge/early goal/old ancestor/deadline/sweeps");
    }
    bs::sub_detail::TestClock::enabled=true;bs::sub_detail::TestClock::value=0;
    p.max_turn=2;p.time_limit_ms=0.001;p.beam_width=p.sub_beam_width=1;
    int world=0;
    auto r=B::run<int>(p,0,0U,[](const auto&,auto&e){e.push(1,0);},[&](int x){world+=x;},[&](int x){world-=x;bs::sub_detail::TestClock::value+=5;});
    require(world==0&&r.runtime.stop_reason==Reason::Completed&&r.time_limit_reached,"T15 restoration crosses deadline");
    bs::sub_detail::TestClock::enabled=false;
}

void csv_cases([[maybe_unused]] const std::string& prefix) {
#ifdef LOCAL
    std::filesystem::create_directories(std::filesystem::path(prefix).parent_path());
    B::Param p;p.max_turn=2;p.beam_width=2;p.sub_beam_width=3;
    auto run=[&](B::CsvStatHook& hook,bool answer) {
        bs::sub_detail::TestClock::enabled=true;bs::sub_detail::TestClock::value=0;
        p.max_turn_is_answer=answer;
        auto expand=[](const auto&now,auto&e){
            const auto to=now.sub_step==2?Next::Turn:Next::Sub;
            e.push(1,-now.depth-1,to);
            bs::sub_detail::TestClock::value+=20000;
        };
        auto r=B::run<int>(p,0,0U,expand,[](int){},[](int){},hook);
        bs::sub_detail::TestClock::enabled=false;
        require(hook.rows.front().event==Event::RunStart&&hook.rows.back().event==Event::RunEnd,"CSV start/end");
        require(r.found==answer&&r.runtime.elapsed_us==120000,"CSV timings");
        return r.runtime;
    };
    for(double interval:{0.0,-1.0,std::numeric_limits<double>::quiet_NaN(),50.0}) {
        const std::string label=std::isnan(interval)?"nan":std::to_string(static_cast<int>(interval));
        B::CsvStatHook hook(prefix+"_"+label+".csv",interval);
        auto rt=run(hook,true);const auto first_count=hook.rows.size();
        require(rt.first_solution_us==120000&&rt.last_best_update_us==120000,"first solution timestamp");
        require(interval==50?first_count==4:first_count>8,"CSV sampling");
        hook.filename=prefix+"_"+label+"_empty.csv";run(hook,false);
        require(hook.rows.size()==first_count,"CSV reused Hook resets");
    }
    struct Comma : std::numpunct<char> { char do_decimal_point()const override{return ',';} };
    const auto old=std::locale::global(std::locale(std::locale::classic(),new Comma));
    using Small=bs::Beam<signed char>;
    Small::CsvStatHook small(prefix+"_small.csv",0);Small::Runtime rt;
    small(Small::EventType::RunStart,rt);
    rt.found=true;rt.best_cost=-7;rt.elapsed_us=1234;
    small(Small::EventType::RunEnd,rt);
    std::locale::global(old);
#endif
}
} // namespace sub_test

namespace bs {
// private構造の照合窓口は直接コンパイル時だけ存在する。
struct SubTestAccess {
    static void clock_counter() {
        using namespace sub_test;
        B::Param p; p.max_turn = 1;
        using E = B::Engine<int>; E engine(p, 0, 0ULL, true);
        engine.unchecked_ = std::numeric_limits<int>::max() - 1;
        engine.after_expand(0, false); engine.after_expand(0, false);
        sub_test::require(engine.unchecked_ >= 0 && engine.unchecked_ < std::numeric_limits<int>::max(), "unobserved clock counter does not overflow");
    }
    static void exercise(std::uint64_t operations) {
        using namespace sub_test;
        struct Item{long long cost;std::uint64_t hash;bool has_hash;int value;};
        using Pool=B::Selector<Item>;
        const std::array<int,10> widths{1,2,3,7,8,9,31,32,33,129};
        std::uint64_t done=0,serial=0;Rng rng{987123};
        for(int w:widths)for(bool dedup:{true,false}) {
            Pool pool(w,dedup);RefPool ref(w,mode==2,dedup);B::CandidateStats got,want;
            std::vector<std::uint64_t> colliding;
            const auto mask=std::bit_ceil(static_cast<std::size_t>(w)*(mode==2?2:4)+(mode==2?0:17))-1;
            for(std::uint64_t h=0;colliding.size()<64;++h)if((sub_detail::mix(h)&mask)==mask)colliding.push_back(h);
            const auto n=operations/20+(done<operations%20?1:0);
            for(std::uint64_t i=0;i<n;++i,++done) {
                if(i%997==996){pool.clear();ref.clear();}
                else {
                    const long long cost=rng.pick(4)==0?-static_cast<long long>(i):rng.pick(101)-50;
                    const auto hash=i%3==0?colliding[static_cast<std::size_t>(rng.pick(64))]:rng.next();
                    const bool hashed=rng.pick(7)!=0;
                    RefNode node;node.cost=cost;node.hash=hash;node.hashed=hashed;node.serial=serial++;node.state.id=static_cast<int>(i);
                    auto a=pool.choose(cost,hash,hashed,got);auto b=ref.choose(node,want);
                    require(a.slot==b.first&&a.duplicate==b.second,"pool choose");
                    if(a.slot>=0){
                        pool.put(a,Item{cost,hash,hashed,node.state.id},got);
                        ref.put(b.first,std::move(node));++want.accepted;if(b.second)++want.replaced_by_hash;
                    }
                }
                require(pool.size()==ref.count&&stats_equal(got,want),"pool size/statistics");
                if(!pool.empty()&&i%8==0)require(pool.best_cost()==ref.slots[ref.best()]->cost&&pool.worst_cost()==ref.slots[ref.worst()]->cost,"pool cost range");
            }
        }
        require(done>=operations,"pool operation count");
    }
};
} // namespace bs

// 共通方針の境界テスト。実時間のsleepに依存せず、仮想時計で再現する。
namespace policy_test {
using Lib = bs::Beam<>;
using Clock = bs::sub_detail::TestClock;
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
    int world=0;
    auto expand = [&](const auto& now, auto& emit) {
        (void)now;
        const int from = world;
        out.visited.push_back(from);
        
        for (const auto& edge : graph.at(static_cast<std::size_t>(from))) {
            const Action action{from,edge.to};
            bool accepted;
            accepted=emit.push(action,edge.cost,edge.hash, edge.answer, edge.sub ? Lib::Next::Sub : Lib::Next::Turn);
            out.accepted.push_back(accepted);
        }
        if (from == options.slow_node) Clock::value += 1000;
    };
    auto hook = [&](Lib::EventType event, const Lib::Runtime&) {
        if (event == Lib::EventType::RunStart && options.expire_at_start) Clock::value += 1000;
    };
            auto forward=[&](const auto& edge){const auto& a=edge;require(world==a.from,"forward state");world=a.to;};
        auto backward=[&](const auto& edge){const auto& a=edge;require(world==a.to,"backward state");world=a.from;};
        auto result=Lib::run<Action>(p,0LL,std::uint64_t{0},expand,forward,backward,hook);
        require(world==0,"external state restored");
    out.found = result.found; out.completed = result.completed_max_turn;
    out.cost = result.best_cost; out.turn = result.best_turn; out.rt = result.runtime;
    int id=0;
        for(const auto& item:result.path){const auto& a=item;require(a.from==id,"path continuity");id=a.to;out.path.push_back(id);}
        out.offsets.assign(result.turn_offsets.begin(),result.turn_offsets.end());
    if(result.found) out.id=out.path.empty()?0:out.path.back();
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
        
        require(r.path==std::vector<int>({3,4}), "saved action path survives queue removal");
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
    p={};p.max_turn=3;p.beam_width=2;p.max_turn_is_answer=false;p.expand_answers=false;
    Graph mixed(5);mixed[0]={{1,2,false,1,true}};mixed[1]={{2,3}};mixed[2]={{3,1,true,1,true}};
    auto middle=run<History>(p,mixed);
    require(middle.found && middle.id==3 && middle.turn==1 && middle.rt.best_sub_step==1 && middle.rt.best_depth==3,"mid-sub answer position");
    require(middle.offsets==std::vector<std::size_t>({0,2}),"partial turn action suffix");

}
void all() {
    const bool enabled=Clock::enabled;
    const auto value=Clock::value;
    cases<false>();
    
    Clock::enabled=enabled;Clock::value=value;
}
} // namespace policy_test

int main(int argc,char**argv) {
    policy_test::all();
    try {
        int count=argc>1?std::stoi(argv[1]):10000;
        sub_test::basic_cases();sub_test::api_cases();sub_test::floating_cases();
        sub_test::deep_case();sub_test::retention_and_transform_cases();
        sub_test::compound_cases();
        sub_test::csv_cases("build/csv_"+std::to_string(sub_test::mode));
        sub_test::random_cases(count);sub_test::exhaustive_cases(1000);
        bs::SubTestAccess::clock_counter();
        bs::SubTestAccess::exercise(1000000);
        std::cout<<"PASS mode="<<sub_test::mode<<" random="<<count<<" exhaustive=1000 pool_operations=1000000\n";
    } catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}

#endif
