#pragma once
#include <bits/stdc++.h>

// chokudai_copy_sub_v03.hpp : ターン境界で比較をそろえる sub 探索。cost は小さいほど良い。
// W=境界幅、U=sub幅、G=生成数、E=展開数、D=経路長。利用側の処理費用は別途加算。
namespace cs {
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
template<class U, class T> concept StateArg = std::same_as<std::remove_cvref_t<U>, T>;

namespace sub_detail {
inline std::uint64_t mix(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
inline std::int64_t to_us(double ms) noexcept {
    if (!(ms > 0)) return 0;
    const double x = ms * 1000;
    if (x >= static_cast<double>(INT64_MAX)) return INT64_MAX;
    return std::max<std::int64_t>(1, static_cast<std::int64_t>(x + 0.5));
}
#if __INCLUDE_LEVEL__ == 0
// 単体実行時だけの仮想時計。通常includeでは領域・分岐とも存在しない。
inline bool virtual_clock = false;
inline std::int64_t virtual_us = 0;
#endif
inline std::int64_t now_us() {
#if __INCLUDE_LEVEL__ == 0
    if (virtual_clock) return virtual_us;
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
struct ChokudaiCopySub {
    using cost_type = Cost;
    using hash_type = Hash;
private:
#if __INCLUDE_LEVEL__ == 0
    friend struct SubTestAccess;
#endif
    template<class Value> class Engine;
public:
    enum class Next { Sub, Turn };
    enum class StopReason { None, Completed, Exhausted, TimeLimit, SweepLimit, NodeLimit, InvalidInitial };
    enum class EventType { RunStart, SubStepEnd, TurnEnd, SweepEnd, RunEnd };
    struct Param {
        int max_turn = 0;              // 完了させるターン数。負値は0。
        int beam_width = 1;            // 次ターン開始候補の幅。1以上。
        double time_limit_ms = 0;      // 探索全体のsoft limit。0以下・NaNは無制限。
        int time_check_interval = 64;  // 通常の時計確認間隔。改善時・層境界でも確認。
        bool max_turn_is_answer = true;
        bool use_hash_dedup = true;    // Hashが明示された候補だけを重複排除。
        int chokudai_width = 1;        // 1ターン・1スイープで取り出す開始候補数。
        std::int64_t max_sweeps = 0;   // 通常スイープ上限。時間超過後の解なし救済には適用しない。
        int sub_beam_width = 0;        // 0以下ならbeam_widthを使用。
    };
    template<class Value> struct StateView {
        int turn = 0; Cost cost{}; const Value& state; Hash hash{}; bool has_hash = false;
        std::int64_t sub_step = 0, depth = 0;
    };
    struct CandidateStats {
        std::uint64_t generated = 0, accepted = 0;
        std::uint64_t pruned_by_width = 0, pruned_by_hash = 0, replaced_by_hash = 0;
    };
    struct Runtime {
        Param param;                  // 正規化した設定。幅等を別項目として重複保持しない。
        int turn = 0;
        std::int64_t sub_step = 0;
        std::int64_t elapsed_us = 0;  // 最後に時計を読んだ時点。
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
        std::int64_t sweep = 0, rescue_sweeps = 0;
        std::uint64_t popped_start_candidates = 0;
        std::size_t pending_start_candidates = 0;
        StopReason stop_reason = StopReason::None;
        bool completed_max_turn = false;
    };
    struct NoOp {
        // 通知を無視する。O(1)。
        void operator()(EventType, const Runtime&) const noexcept {}
    };
    struct CsvStatHook {
        struct Row { EventType event; Runtime runtime; };
        std::string filename;
        std::int64_t interval_us;
        std::vector<Row> rows;
        // 保存先と記録間隔。0以下・NaNは全イベントを記録。O(ファイル名長)。
        explicit CsvStatHook(std::string path = "chokudai_stat.csv", double interval_ms = 50)
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
            case EventType::SweepEnd: return "SweepEnd";
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
            case StopReason::NodeLimit: return "NodeLimit";
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
                 "invalid_candidates", "frontier_remaining", "sub_candidate_count", "next_turn_candidate_count",
                 "peak_pending_candidates", "sub_best_cost", "sub_worst_cost", "next_turn_best_cost", "next_turn_worst_cost",
                 "found", "best_cost", "best_turn", "best_sub_step", "best_depth", "best_update_count",
                 "first_solution_ms", "last_best_update_ms", "time_limit_reached", "expanded_after_timeout",
                 "timeout_turn_advances", "sweep", "rescue_sweeps", "popped_start_candidates", "pending_start_candidates",
                 "stop_reason", "completed_max_turn", "max_turn", "beam_width", "sub_beam_width",
                 "time_limit_ms", "time_check_interval", "max_turn_is_answer", "use_hash_dedup", "chokudai_width", "max_sweeps"}) cell(s);
            end();
            for (const auto& row : rows) {
                const auto& r = row.runtime;
                auto opt = [&](bool present, const auto& value) { if (present) cell(value); else cell(""); };
                auto stats = [&](const CandidateStats& s) {
                    cell(s.generated); cell(s.accepted); cell(s.pruned_by_width);
                    cell(s.pruned_by_hash); cell(s.replaced_by_hash);
                };
                cell(event_name(row.event)); cell(static_cast<double>(r.elapsed_us) / 1000.0); cell(r.turn); cell(r.sub_step);
                cell(r.expanded_nodes); stats(r.sub_stats); stats(r.turn_stats); cell(r.invalid_candidates);
                cell(r.frontier_remaining); cell(r.sub_candidate_count); cell(r.next_turn_candidate_count);
                cell(r.peak_pending_candidates);
                opt(r.sub_candidate_count != 0, r.sub_best_cost); opt(r.sub_candidate_count != 0, r.sub_worst_cost);
                opt(r.next_turn_candidate_count != 0, r.next_turn_best_cost); opt(r.next_turn_candidate_count != 0, r.next_turn_worst_cost);
                cell(r.found); opt(r.found, r.best_cost); opt(r.found, r.best_turn);
                opt(r.found, r.best_sub_step); opt(r.found, r.best_depth); cell(r.best_update_count);
                opt(r.first_solution_us >= 0, static_cast<double>(r.first_solution_us) / 1000.0);
                opt(r.last_best_update_us >= 0, static_cast<double>(r.last_best_update_us) / 1000.0);
                cell(r.time_limit_reached); cell(r.expanded_after_timeout); cell(r.timeout_turn_advances);
                cell(r.sweep); cell(r.rescue_sweeps); cell(r.popped_start_candidates); cell(r.pending_start_candidates);
                cell(reason_name(r.stop_reason)); cell(r.completed_max_turn);
                const bool start = row.event == EventType::RunStart;
                const auto& p = r.param;
                opt(start, p.max_turn); opt(start, p.beam_width); opt(start, p.sub_beam_width);
                opt(start, p.time_limit_ms); opt(start, p.time_check_interval);
                opt(start, p.max_turn_is_answer); opt(start, p.use_hash_dedup);
                opt(start, p.chokudai_width); opt(start, p.max_sweeps);
                end();
            }
            out.flush();
            if (!out) throw std::runtime_error("CSV write failed: " + filename);
        }
    };
    template<class Value> struct Result {
        bool found = false, time_limit_reached = false, completed_max_turn = false;
        Cost best_cost{};
        int best_turn = 0;
        std::int64_t best_sub_step = 0, best_depth = 0;
        std::optional<Value> best_state;
        Runtime runtime;
    };
    template<class Value> class Emitter {
    public:
        // 候補を追加する。期待償却O(log(2+幅)+保存費用)。trueは候補または最良解への一時保存。
        template<class U> requires StateArg<U, Value>
        bool push(U&& value, Cost cost, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, hash_, false, false, next);
        }
        // 有効解の印を指定する。同じ計算量。finishedは展開停止の指定ではない。
        template<class U, class B> requires StateArg<U, Value> && BoolArg<B>
        bool push(U&& value, Cost cost, B finished, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, hash_, false, finished, next);
        }
        // Hashを明示する。同じ計算量。同じ保存先内だけで重複排除する。
        template<class U, class H> requires StateArg<U, Value> && CompatibleHashArg<H, Hash>
        bool push(U&& value, Cost cost, H hash, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, static_cast<Hash>(hash), true, false, next);
        }
        // Hashと有効解の印を指定する。同じ計算量。
        template<class U, class H, class B> requires StateArg<U, Value> && CompatibleHashArg<H, Hash> && BoolArg<B>
        bool push(U&& value, Cost cost, H hash, B finished, Next next = Next::Turn) {
            return push_value(std::forward<U>(value), cost, static_cast<Hash>(hash), true, finished, next);
        }
        // 必要な候補だけmakerで生成する。期待償却O(log(2+幅)+採用時の生成・保存費用)。
        template<class Maker> requires std::invocable<Maker&&>
        bool push_lazy(Cost cost, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, hash_, false, false, next, std::forward<Maker>(maker));
        }
        // 有効解の印付きの遅延生成。同じ計算量。makerはtrue時1回、false時0回。
        template<class B, class Maker> requires BoolArg<B> && std::invocable<Maker&&>
        bool push_lazy(Cost cost, B finished, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, hash_, false, finished, next, std::forward<Maker>(maker));
        }
        // Hash付きの遅延生成。同じ計算量。
        template<class H, class Maker> requires CompatibleHashArg<H, Hash> && std::invocable<Maker&&>
        bool push_lazy(Cost cost, H hash, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, static_cast<Hash>(hash), true, false, next, std::forward<Maker>(maker));
        }
        // Hashと有効解の印付きの遅延生成。同じ計算量。
        template<class H, class B, class Maker>
        requires CompatibleHashArg<H, Hash> && BoolArg<B> && std::invocable<Maker&&>
        bool push_lazy(Cost cost, H hash, B finished, Maker&& maker, Next next = Next::Turn) {
            return engine_.push_candidate(cost, static_cast<Hash>(hash), true, finished, next, std::forward<Maker>(maker));
        }

    private:
        friend class Engine<Value>;
        Engine<Value>& engine_;
        Hash hash_;
        Emitter(Engine<Value>& engine, Hash hash) : engine_(engine), hash_(hash) {}
        template<class U>
        bool push_value(U&& value, Cost cost, Hash hash, bool has_hash, bool finished, Next next) {
            auto maker = [&]() -> decltype(auto) { return std::forward<U>(value); };
            return engine_.push_candidate(cost, hash, has_hash, finished, next, maker);
        }
    };
private:
    template<class Value> class Engine {
        friend class Emitter<Value>;
#if __INCLUDE_LEVEL__ == 0
        friend struct SubTestAccess;
#endif
        static_assert(std::copy_constructible<Value> && std::assignable_from<Value&, Value>);
        Runtime rt_;
        std::int64_t start_us_ = sub_detail::now_us(), limit_us_ = 0;
        int unchecked_ = 0;
        bool observe_time_ = false, terminal_seen_ = false;
        struct Position { int turn = 0; std::int64_t sub_step = 0, depth = 0; };
        Position from_;
        static Param normalize(Param p) {
            p.max_turn = std::max(0, p.max_turn);
            p.beam_width = std::max(1, p.beam_width);
            p.sub_beam_width = p.sub_beam_width <= 0 ? p.beam_width : p.sub_beam_width;
            p.time_check_interval = std::max(1, p.time_check_interval);
            if (!(p.time_limit_ms > 0)) p.time_limit_ms = 0;
            p.chokudai_width = std::clamp(p.chokudai_width, 1, p.beam_width);
            p.max_sweeps = std::max<std::int64_t>(0, p.max_sweeps);
            return p;
        }
        void tick(bool force = false) {
            // 観測しない実行でも、確認カウンタを繰り越さない。
            unchecked_ = 0;
            if (!force && limit_us_ == 0 && !observe_time_) return;
            rt_.elapsed_us = sub_detail::now_us() - start_us_;
            if (limit_us_ > 0 && rt_.elapsed_us >= limit_us_) rt_.time_limit_reached = true;
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
            if constexpr (!std::same_as<std::remove_cvref_t<Hook>, NoOp>) {
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
            if (to.turn == rt_.param.max_turn) {
                rt_.completed_max_turn = true;
                terminal_seen_ = true;
            }
            return true;
        }
        bool improves(Cost cost, bool finished, const Position& to) const {
            return (finished || (to.turn == rt_.param.max_turn && rt_.param.max_turn_is_answer)) &&
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

        // turn/sub_stepはQueueで共有し、経路ごとのdepthはKeyに残す。
        struct Key {
            Cost cost{}; Hash hash{}; std::int64_t depth = 0;
            std::uint64_t serial = 0;
            int next_free = -1;
            bool has_hash = false;
        };
        struct Item { Cost cost; Hash hash; Position pos; bool has_hash; Value value; };
        struct Ends { int best = -1, worst = -1; };
        // 最良・最悪を一つの木で管理。Stateを比較用の配列から分離する。
        class Queue {
            int width_;
            bool hashed_;
            std::size_t base_;
            std::vector<std::optional<Value>> values_;
            std::vector<Key> keys_;
            std::vector<Ends> tree_;
            std::vector<int> table_;
            int free_ = -1, size_ = 0;
            Position position_;
            std::size_t bucket(Hash h) const { return static_cast<std::size_t>(sub_detail::mix(h)) & (table_.size() - 1); }
            bool before(int a, int b) const {
                if (keys_[a].cost != keys_[b].cost) return keys_[a].cost < keys_[b].cost;
                return keys_[a].serial < keys_[b].serial;
            }
            Ends ends(std::size_t p) const {
                if (p < base_) return tree_[p];
                const auto i = p - base_;
                return i < values_.size() && values_[i] ? Ends{static_cast<int>(i), static_cast<int>(i)} : Ends{};
            }
            void update(int slot) {
                std::size_t p = base_ + static_cast<std::size_t>(slot);
                while ((p /= 2) != 0) {
                    const auto a = ends(p * 2), b = ends(p * 2 + 1);
                    if (a.best < 0) tree_[p] = b;
                    else if (b.best < 0) tree_[p] = a;
                    else tree_[p] = {before(a.best, b.best) ? a.best : b.best,
                                     before(a.worst, b.worst) ? b.worst : a.worst};
                }
            }
            int find(Hash h) const {
                if (table_.empty()) return -1;
                auto p = bucket(h);
                while (table_[p] >= 0) {
                    if (keys_[table_[p]].hash == h) return table_[p];
                    p = (p + 1) & (table_.size() - 1);
                }
                return -1;
            }
            void erase(int slot, bool update_tree = true) {
                if (hashed_ && keys_[slot].has_hash) {
                    auto p = bucket(keys_[slot].hash);
                    while (table_[p] != slot) p = (p + 1) & (table_.size() - 1);
                    const auto mask = table_.size() - 1;
                    for (auto j = (p + 1) & mask; table_[j] >= 0; j = (j + 1) & mask) {
                        const auto home = bucket(keys_[table_[j]].hash);
                        if (((p - home) & mask) < ((j - home) & mask)) { table_[p] = table_[j]; p = j; }
                    }
                    table_[p] = -1;
                }
                values_[slot].reset();
                keys_[slot].next_free = free_; free_ = slot; --size_;
                if (update_tree) update(slot);
            }
            Item take_value(int slot) {
                const auto& k = keys_[slot];
                auto pos = position_; pos.depth = k.depth;
                return {k.cost, k.hash, pos, k.has_hash, std::move(*values_[slot])};
            }
        public:
            struct Choice { int slot = -1; bool duplicate = false; };
            Queue(int width, bool hashed)
                : width_(width), hashed_(hashed), base_(std::bit_ceil(static_cast<std::size_t>(width))) {
                values_.reserve(static_cast<std::size_t>(width)); keys_.reserve(static_cast<std::size_t>(width));
                tree_.resize(base_);
            }
            int size() const { return size_; }
            bool empty() const { return size_ == 0; }
            int best() const { return base_ == 1 ? 0 : tree_[1].best; }
            int worst() const { return base_ == 1 ? 0 : tree_[1].worst; }
            Cost best_cost() const { return keys_[best()].cost; }
            Cost worst_cost() const { return keys_[worst()].cost; }
            Choice choose(Cost cost, Hash hash, bool has_hash, CandidateStats& stats) const {
                const bool full = size_ == width_;
                if (full && !(cost < worst_cost())) { ++stats.pruned_by_width; return {}; }
                const int found = has_hash && hashed_ ? find(hash) : -1;
                if (found >= 0) {
                    if (!(cost < keys_[found].cost)) { ++stats.pruned_by_hash; return {}; }
                    return {found, true};
                }
                return {full ? worst() : (free_ >= 0 ? free_ : static_cast<int>(values_.size())), false};
            }
            // 受理位置へ保存。返り値は純増件数。初期Stateの挿入にも使う。
            int put(Choice choice, Item&& item, std::uint64_t serial) {
                if (empty()) position_ = item.pos;
                const int i = choice.slot;
                if (static_cast<std::size_t>(i) == values_.size()) { values_.emplace_back(); keys_.emplace_back(); }
                const bool replaced = values_[i].has_value();
                if (replaced) erase(i, false);
                if (free_ >= 0) { assert(free_ == i); free_ = keys_[i].next_free; }
                values_[i].emplace(std::move(item.value));
                keys_[i] = {item.cost, item.hash, item.pos.depth, serial, -1, item.has_hash};
                ++size_; update(i);
                if (hashed_ && item.has_hash) {
                    if (table_.empty()) table_.assign(std::bit_ceil(static_cast<std::size_t>(width_) * 2), -1);
                    auto p = bucket(item.hash);
                    while (table_[p] >= 0) p = (p + 1) & (table_.size() - 1);
                    table_[p] = i;
                }
                return replaced ? 0 : 1;
            }
            Item pop() {
                const int i = best(); Item item = take_value(i); erase(i); return item;
            }
            void clear() {
                values_.clear(); keys_.clear(); free_ = -1; size_ = 0;
                std::fill(tree_.begin(), tree_.end(), Ends{});
                std::fill(table_.begin(), table_.end(), -1);
            }
            void drain(std::vector<Item>& frontier) {
                frontier.clear(); frontier.reserve(static_cast<std::size_t>(size_));
                for (int i = 0; i < static_cast<int>(values_.size()); ++i) if (values_[i]) frontier.push_back(take_value(i));
                clear();
            }
        };

        std::vector<std::unique_ptr<Queue>> layers_;
        std::unique_ptr<Queue> sub_;
        std::vector<Item> frontier_;
        std::vector<std::size_t> order_;
        std::optional<Value> best_;
        std::size_t pending_ = 0;
        std::uint64_t serial_ = 0;
        std::int64_t normal_sweeps_ = 0;
        bool sub_empty() const { return !sub_ || sub_->empty(); }
        Queue& layer(int t) {
            auto& q = layers_[static_cast<std::size_t>(t)];
            if (!q) q = std::make_unique<Queue>(rt_.param.beam_width, rt_.param.use_hash_dedup);
            return *q;
        }
        Queue* next_queue() const {
            const auto t = static_cast<std::size_t>(rt_.turn) + 1;
            return t < layers_.size() ? layers_[t].get() : nullptr;
        }
        bool next_exists() const {
            const auto* q = next_queue();
            return terminal_seen_ || (q && !q->empty());
        }
        bool timed_out_with_answer() {
            if (rt_.time_limit_reached && rt_.found) { rt_.stop_reason = StopReason::TimeLimit; return true; }
            return false;
        }
        void peak() {
            const auto n = pending_ + rt_.frontier_remaining + (sub_ ? static_cast<std::size_t>(sub_->size()) : 0);
            rt_.peak_pending_candidates = std::max(rt_.peak_pending_candidates, n);
        }
        void snapshot() {
            rt_.pending_start_candidates = pending_;
            rt_.sub_candidate_count = sub_ ? static_cast<std::size_t>(sub_->size()) : 0;
            const auto* q = next_queue();
            rt_.next_turn_candidate_count = q ? static_cast<std::size_t>(q->size()) : 0;
            if (rt_.sub_candidate_count) { rt_.sub_best_cost = sub_->best_cost(); rt_.sub_worst_cost = sub_->worst_cost(); }
            if (rt_.next_turn_candidate_count) { rt_.next_turn_best_cost = q->best_cost(); rt_.next_turn_worst_cost = q->worst_cost(); }
        }
        template<class Maker>
        bool push_candidate(Cost cost, Hash hash, bool has_hash, bool finished, Next next, Maker&& maker) {
            Position to;
            CandidateStats* stats = nullptr;
            if (!destination(next, cost, to, stats)) return false;
            const bool best = improves(cost, finished, to);
            Queue* q = nullptr;
            typename Queue::Choice choice;
            if (to.turn < rt_.param.max_turn) {
                if (next == Next::Sub && !sub_) sub_ = std::make_unique<Queue>(rt_.param.sub_beam_width, rt_.param.use_hash_dedup);
                q = next == Next::Sub ? sub_.get() : &layer(to.turn);
                choice = q->choose(cost, hash, has_hash, *stats);
            }
            if (!best && choice.slot < 0) return false;
            to.sub_step = next == Next::Sub ? from_.sub_step + 1 : 0;
            to.depth = from_.depth + 1;
            // 中間Valueからの追加moveを避け、明示的な型変換は維持する。
            Item item{cost, hash, to, has_hash, Value(std::invoke(std::forward<Maker>(maker)))};
            if (best) { best_.emplace(item.value); best_metadata(cost, to); }
            if (choice.slot >= 0) {
                const int added = q->put(choice, std::move(item), serial_++);
                ++stats->accepted;
                if (choice.duplicate) ++stats->replaced_by_hash;
                if (next == Next::Turn) pending_ += static_cast<std::size_t>(added);
                if (added) peak();
            }
            return true;
        }
        template<class Expand> void expand_item(Item& item, Expand& expand) {
            from_ = item.pos; rt_.turn = from_.turn; rt_.sub_step = from_.sub_step;
            const StateView<Value> now{from_.turn, item.cost, item.value, item.hash, item.has_hash, from_.sub_step, from_.depth};
            const auto old = rt_.best_update_count;
            const bool expired = rt_.time_limit_reached;
            invoke_expand(expand, now);
            --rt_.frontier_remaining;
            after_expand(old, expired);
        }
        template<class Expand, class Hook> void visit(int t, Expand& expand, Hook& hook) {
            auto& q = *layers_[static_cast<std::size_t>(t)];
            rt_.turn = t; rt_.sub_step = 0; terminal_seen_ = false;
            const int count = std::min(rt_.time_limit_reached ? 1 : rt_.param.chokudai_width, q.size());
            int done = 0;
            for (; done < count; ++done) {
                if (timed_out_with_answer()) return;
                if (rt_.time_limit_reached && next_exists()) break;
                // 実際に展開する直前に1件だけpopする。未展開の開始候補は失わない。
                auto item = q.pop(); --pending_; ++rt_.popped_start_candidates;
                rt_.frontier_remaining = 1;
                expand_item(item, expand);
                if (timed_out_with_answer()) return;
            }
            tick();
            if (timed_out_with_answer()) return;
            if (done == count) notify(EventType::SubStepEnd, hook);
            while (!sub_empty() && !(rt_.time_limit_reached && next_exists())) {
                if (timed_out_with_answer()) return;
                sub_->drain(frontier_);
                rt_.frontier_remaining = frontier_.size();
                bool ordered = false;
                std::size_t i = 0;
                for (; i < frontier_.size(); ++i) {
                    if (timed_out_with_answer()) return;
                    if (rt_.time_limit_reached && next_exists()) break;
                    if (rt_.time_limit_reached && !ordered) {
                        order_.resize(frontier_.size()); std::iota(order_.begin(), order_.end(), std::size_t{0});
                        std::stable_sort(order_.begin() + static_cast<std::ptrdiff_t>(i), order_.end(),
                            [&](std::size_t a, std::size_t b) { return frontier_[a].cost < frontier_[b].cost; });
                        ordered = true;
                    }
                    expand_item(frontier_[ordered ? order_[i] : i], expand);
                }
                tick();
                if (timed_out_with_answer()) return;
                if (i == frontier_.size()) notify(EventType::SubStepEnd, hook);
            }
            if (timed_out_with_answer()) return;
            if (rt_.time_limit_reached && next_exists() &&
                (done < count || rt_.frontier_remaining != 0 || !sub_empty())) ++rt_.timeout_turn_advances;
            notify(EventType::TurnEnd, hook);
            frontier_.clear(); if (!sub_empty()) sub_->clear(); rt_.frontier_remaining = 0;
        }
    public:
        Engine(const Param& param, Value&& initial, Cost cost, Hash hash, bool has_hash) {
            rt_.param = normalize(param); limit_us_ = sub_detail::to_us(rt_.param.time_limit_ms);
            if (!sub_detail::valid(cost)) { rt_.stop_reason = StopReason::InvalidInitial; return; }
            if (rt_.param.max_turn == 0) {
                rt_.completed_max_turn = true; rt_.stop_reason = StopReason::Completed;
                if (rt_.param.max_turn_is_answer) { best_.emplace(std::move(initial)); best_metadata(cost, {}); }
                return;
            }
            layers_.resize(static_cast<std::size_t>(rt_.param.max_turn));
            layer(0).put({0, false}, {cost, hash, {}, has_hash, std::move(initial)}, serial_++);
            pending_ = 1; peak();
        }
        template<class Expand, class Hook>
        Result<Value> run(Expand&& expand, Hook&& hook) {
            observe_time_ = !std::same_as<std::remove_cvref_t<Hook>, NoOp> ||
                std::is_invocable_v<Expand&, const StateView<Value>&, const Runtime&, Emitter<Value>&>;
            tick(true); if (rt_.found) stamp_best();
            notify(EventType::RunStart, hook);
            while (rt_.stop_reason == StopReason::None) {
                tick();
                if (timed_out_with_answer()) break;
                if (pending_ == 0) { rt_.stop_reason = StopReason::Exhausted; break; }
                const bool rescue = rt_.time_limit_reached;
                if (!rescue && rt_.param.max_sweeps > 0 && normal_sweeps_ >= rt_.param.max_sweeps) {
                    rt_.stop_reason = StopReason::SweepLimit; break;
                }
                ++rt_.sweep;
                if (rescue) ++rt_.rescue_sweeps; else ++normal_sweeps_;
                for (int t = 0; t < rt_.param.max_turn; ++t) {
                    tick();
                    if (timed_out_with_answer()) break;
                    if (layers_[static_cast<std::size_t>(t)] && !layers_[static_cast<std::size_t>(t)]->empty()) visit(t, expand, hook);
                    if (rt_.stop_reason != StopReason::None) break;
                }
                if (rt_.stop_reason != StopReason::None) break;
                tick();
                if (timed_out_with_answer()) break;
                notify(EventType::SweepEnd, hook);
            }
            Result<Value> result;
            result.best_state = std::move(best_);
            tick(true); snapshot(); hook(EventType::RunEnd, static_cast<const Runtime&>(rt_));
            result_metadata(result); return result;
        }
    };
public:
    // Hash付き初期状態から探索する。期待O(G log(2+max(W,U))+E+状態保存費用)。
    // chokudai版はスイープのターン走査費用を加算する。状態の履歴は利用側で保持する。
    template<class State, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State> run(const Param& param, State initial_state, Cost initial_cost, H initial_hash,
                             Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
    }
    // Hashなし初期状態から探索する。同じ計算量。Hash付きpushまで重複排除に参加しない。
    template<class State, class Expand, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<State> run(const Param& param, State initial_state, Cost initial_cost,
                             Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, Hash{}, false);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
    }
};
} // namespace cs

#if __INCLUDE_LEVEL__ == 0
// ここからはヘッダを直接コンパイルした場合だけの検証・計測コード。
namespace sub_test {
constexpr int mode = 2; // 0=copy, 1=delta, 2=chokudai
using B = cs::ChokudaiCopySub<>;
using Next = B::Next;
using Reason = B::StopReason;
using Event = B::EventType;
inline void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Rng {
    std::uint64_t state;
    std::uint64_t next() { state += 0x9e3779b97f4a7c15ULL; return cs::sub_detail::mix(state); }
    int pick(int n) { return static_cast<int>(next() % static_cast<unsigned>(n)); }
};
struct Edge {
    int to; long long cost; bool turn = false, finished = false, hashed = true;
    std::uint64_t hash = 0;
    std::int64_t maker_us = 0;
};
struct Graph {
    std::vector<std::vector<Edge>> edges;
    std::vector<std::int64_t> expand_us;
    std::int64_t hook_start_us = 0, hook_step_us = 0, hook_end_us = 0;
    explicit Graph(int n) : edges(static_cast<std::size_t>(n)), expand_us(static_cast<std::size_t>(n)) {}
    void add(int from, int to, long long cost, bool turn = false, bool finished = false, bool hashed = true, std::uint64_t hash = UINT64_MAX) {
        edges[from].push_back({to, cost, turn, finished, hashed, hash == UINT64_MAX ? static_cast<std::uint64_t>(to) : hash, 0});
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
    std::size_t parent_rank = 0;
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
    std::size_t nodes = 1;
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
        if(p.time_limit_ms>0 && clock>=cs::sub_detail::to_us(p.time_limit_ms)) out.rt.time_limit_reached=true;
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
        out.best_id=n.state.id;out.best_path=n.state.path;
    }
    void expand(const RefNode& parent, std::size_t rank, RefPool& target) {
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
            child.turn+=e.turn;child.sub=e.turn?0:parent.sub+1;++child.depth;child.parent_rank=rank;
            const bool end=child.turn==p.max_turn;
            if(end){terminal=true;out.rt.completed_max_turn=true;}
            const bool best=(e.finished||(end&&p.max_turn_is_answer))&&(!out.rt.found||child.cost<out.rt.best_cost);
            auto& pool=e.turn?target:sub;
            auto choice=end?std::pair{-1,false}:pool.choose(child,s);
            const bool accepted=best||choice.first>=0;
            out.pushes.push_back(accepted);
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
        frontier=pool.drain();
        (void)grouped;
        out.rt.turn=frontier[0].turn;out.rt.sub_step=frontier[0].sub;return true;
    }
    bool stop_timed() {
        if(out.rt.time_limit_reached&&out.rt.found){out.rt.stop_reason=Reason::TimeLimit;return true;}return false;
    }
    void beam() {
        while(out.rt.stop_reason==Reason::None){
            terminal=false;
            for(;;){
                tick();
                std::vector<std::size_t> indices(frontier.size());std::iota(indices.begin(),indices.end(),std::size_t{0});
                bool sorted=false;std::size_t i=0;
                for(;i<indices.size();++i){
                    if(out.rt.time_limit_reached&&(next.count||terminal))break;
                    if(out.rt.time_limit_reached&&!sorted){std::stable_sort(indices.begin()+static_cast<std::ptrdiff_t>(i),indices.end(),[&](auto a,auto b){return frontier[a].cost<frontier[b].cost;});sorted=true;}
                    expand(frontier[indices[i]],indices[i],next);
                }
                tick();if(i==indices.size())event(Event::SubStepEnd);
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
    void chokudai() {
        std::int64_t normal=0;
        while(true){
            tick();if(stop_timed())return;
            int pending=0;for(auto&q:layers)pending+=q.count;
            if(pending==0){out.rt.stop_reason=Reason::Exhausted;return;}
            if(!out.rt.time_limit_reached&&p.max_sweeps>0&&normal>=p.max_sweeps){out.rt.stop_reason=Reason::SweepLimit;return;}
            ++out.rt.sweep;if(out.rt.time_limit_reached)++out.rt.rescue_sweeps;else++normal;
            for(int t=0;t<p.max_turn;++t){
                tick();if(stop_timed())return;
                if(!layers[t].count)continue;
                out.rt.turn=t;out.rt.sub_step=0;terminal=false;
                RefPool terminal_dummy(p.beam_width,true,p.use_hash_dedup);
                RefPool& target=t+1<p.max_turn?layers[t+1]:terminal_dummy;
                auto exists=[&]{return target.count>0||terminal;};
                const int limit=std::min(layers[t].count,out.rt.time_limit_reached?1:std::max(1,p.chokudai_width));
                int done=0;
                for(;done<limit;++done){
                    if(stop_timed())return;
                    if(out.rt.time_limit_reached&&exists())break;
                    auto root=layers[t].pop();++out.rt.popped_start_candidates;expand(root,0,target);
                    if(stop_timed())return;
                }
                tick();if(stop_timed())return;if(done==limit)event(Event::SubStepEnd);
                std::size_t left=0;
                while(sub.count&&!(out.rt.time_limit_reached&&exists())){
                    if(stop_timed())return;
                    frontier=sub.drain();std::vector<std::size_t> ids(frontier.size());std::iota(ids.begin(),ids.end(),std::size_t{0});
                    bool sorted=false;std::size_t i=0;
                    for(;i<ids.size();++i){
                        if(stop_timed())return;
                        if(out.rt.time_limit_reached&&exists())break;
                        if(out.rt.time_limit_reached&&!sorted){std::stable_sort(ids.begin()+static_cast<std::ptrdiff_t>(i),ids.end(),[&](auto a,auto b){return frontier[a].cost<frontier[b].cost;});sorted=true;}
                        expand(frontier[ids[i]],ids[i],target);
                    }
                    left=ids.size()-i;tick();if(stop_timed())return;if(left==0)event(Event::SubStepEnd);
                }
                if(stop_timed())return;
                if(out.rt.time_limit_reached&&exists()&&(done<limit||left||sub.count))++out.rt.timeout_turn_advances;
                event(Event::TurnEnd);sub.clear();
            }
            tick();if(stop_timed())return;event(Event::SweepEnd);
        }
    }
    Outcome run() {
        RefNode root;root.state.id=0;
        if(p.max_turn==0){out.rt.completed_max_turn=true;out.rt.stop_reason=Reason::Completed;if(p.max_turn_is_answer)save(root);}
        else if constexpr(mode==2)layers[0].put(0,root);
        else frontier.push_back(root);
        tick();if(out.rt.found)stamp();event(Event::RunStart);tick();
        if(out.rt.stop_reason==Reason::None){
            chokudai();
        }
        if constexpr(mode==1)out.backward+=previous_path.size();
        tick();out.rt.elapsed_us=last_clock;event(Event::RunEnd);return std::move(out);
    }
};

Outcome actual(const B::Param& p,const Graph& graph) {
    cs::sub_detail::virtual_clock=true;cs::sub_detail::virtual_us=0;
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
        checksum=std::rotl(checksum,7)^cs::sub_detail::mix(static_cast<std::uint64_t>(a.edge));
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
        if(e==Event::RunStart)cs::sub_detail::virtual_us+=graph.hook_start_us;
        if(e==Event::SubStepEnd)cs::sub_detail::virtual_us+=graph.hook_step_us;
        if(e==Event::RunEnd)cs::sub_detail::virtual_us+=graph.hook_end_us;
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
            auto maker=[&]{++out.makers;cs::sub_detail::virtual_us+=e.maker_us;State s=state;s.id=e.to;s.path.push_back(state.id*1000+j);return s;};
            if(e.hashed)accepted=emit.push_lazy(e.cost,e.hash,e.finished,maker,next);
            else accepted=emit.push_lazy(e.cost,e.finished,maker,next);
            out.pushes.push_back(accepted);
        }
        cs::sub_detail::virtual_us+=graph.expand_us[state.id];
    };
    (void)forward;(void)backward;
    auto result=B::run(p,State{},0,0ULL,expand,hook);
    if(result.found){out.best_id=result.best_state->id;out.best_path=result.best_state->path;}
    else require(!result.best_state,"empty failure state");
    out.rt=result.runtime;
    require(result.found==out.rt.found&&result.time_limit_reached==out.rt.time_limit_reached,"result snapshot");
    cs::sub_detail::virtual_clock=false;
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
        require(stats_equal(a.sub_stats,b.sub_stats)&&stats_equal(a.turn_stats,b.turn_stats),"selection counters");
        require(got.events==expected.events,"event sequence");
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
template<class P>void choku_settings(P&p,int width,std::int64_t sweeps){if constexpr(requires{p.chokudai_width;p.max_sweeps;}){p.chokudai_width=width;p.max_sweeps=sweeps;}}

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
    p.max_turn=3;p.max_turn_is_answer=true;p.time_limit_ms=0;p.beam_width=4;p.sub_beam_width=4;
    Graph improve(8);improve.add(0,1,0,true);improve.add(0,2,1,true);improve.add(1,3,10,true);improve.add(2,4,5,true);improve.add(3,5,10,true);improve.add(4,6,-10,true);
    for(int k:{1,2,10})for(int sweeps:{0,1,2}){p.chokudai_width=k;p.max_sweeps=sweeps;check(p,improve,"C01-C09 sweeps");}
    p.chokudai_width=1;p.max_sweeps=1;p.time_limit_ms=0.001;improve.hook_start_us=1;
    check(p,improve,"T08-T11 rescue overrides sweep cap");
}
void random_cases(int count){
    for(int seed=0;seed<count;++seed){
        Rng rng{static_cast<std::uint64_t>(seed)+101};B::Param p;
        p.max_turn=rng.pick(7);p.beam_width=1+rng.pick(12);p.sub_beam_width=1+rng.pick(12);
        p.max_turn_is_answer=rng.pick(4)!=0;p.use_hash_dedup=rng.pick(5)!=0;
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
                if(e.finished||(nt==p.max_turn&&p.max_turn_is_answer)) {
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
static_assert(cs::CostType<int> && cs::CostType<double> && !cs::CostType<unsigned>);
static_assert(cs::HashType<std::uint32_t> && !cs::HashType<bool> && !cs::HashType<long long>);

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
    B::Param p;p.max_turn=1;p.beam_width=32;p.sub_beam_width=32;
    auto calls=[&](const auto& now, B::Emitter<int>& emit) {
        if(now.sub_step>0)return;
        // 既存4形式、Nextを末尾に付けた4形式、非const/const/rvalueを使う。
        int a=1;const int b=2;
        require(emit.push(a,1),"push plain");
        require(emit.push(b,0,true),"push finished");
        require(emit.push(3,-1,3U),"push hash");
        require(emit.push(4,-2,4ULL,true),"push both");
        require(emit.push(5,5,Next::Sub),"push Sub plain");
        require(emit.push(6,6,false,Next::Sub),"push Sub finished");
        require(emit.push(7,7,7U,Next::Sub),"push Sub hash");
        require(emit.push(8,8,8ULL,false,Next::Sub),"push Sub both");
        require(!emit.push(9,0,static_cast<Next>(37)),"invalid Next");
        int made=0;auto maker=[&]{++made;return 10;};
        require(emit.push_lazy(-3,maker),"lazy plain");
        require(emit.push_lazy(-4,true,maker),"lazy finished");
        require(emit.push_lazy(-5,11U,maker),"lazy hash");
        require(emit.push_lazy(-6,12ULL,true,maker),"lazy both");
        require(emit.push_lazy(10,maker,Next::Sub),"lazy Sub plain");
        require(emit.push_lazy(11,false,maker,Next::Sub),"lazy Sub finished");
        require(emit.push_lazy(12,13U,maker,Next::Sub),"lazy Sub hash");
        require(emit.push_lazy(13,14ULL,false,maker,Next::Sub),"lazy Sub both");
        require(!emit.push_lazy(0,maker,static_cast<Next>(-1))&&made==8,"invalid Next lazy does not construct");
    };
    auto result=B::run(p,0,0,calls); // 初期Hashなし、Runtimeなし、明示Emitter。
    auto nondefault=B::run(p,NonDefault{0},0,[](const auto& now,auto&e){e.push(NonDefault{now.state.value+1},0);});
    require(nondefault.found&&nondefault.best_state->value==1,"non-default State");
    auto converted=B::run(p,NonDefault{0},0,[](const auto&now,auto&e){
        e.push_lazy(0,[&]{return now.state.value+1;});
    });
    require(converted.found&&converted.best_state->value==1,"maker explicit conversion");
    const NonDefault stored{9}; int reference_calls=0;
    auto referenced=B::run(p,NonDefault{0},0,[&](const auto&,auto&e){
        e.push_lazy(0,[&]() -> const NonDefault& {++reference_calls;return stored;});
    });
    require(referenced.found&&referenced.best_state->value==9&&reference_calls==1,"maker reference return");
    require(result.found&&result.runtime.invalid_candidates==(mode==1?1U:2U),"API result");
    // 既に満杯の継続先で落ちる通常候補を内部コピーしない。
    p.max_turn=2;p.beam_width=1;p.sub_beam_width=1;Counted::copies=0;
    {
        auto expand=[](const auto&now,auto&e){
            if(now.turn!=0||now.sub_step!=0)return;
            e.push(Counted{1},-100,Next::Sub);
            const int old=Counted::copies;Counted rejected{2};
            require(!e.push(rejected,100,Next::Sub)&&Counted::copies==old,"reject without internal copy");
            int made=0;
            require(!e.push_lazy(100,[&]{++made;return Counted{3};},Next::Sub)&&made==0,"reject without maker");
            require(e.push_lazy(200,true,[&]{++made;return Counted{4};},Next::Sub)&&made==1,"archive despite width");
        };
        auto r=B::run(p,Counted{0},0,expand);
        require(r.best_state->value==4,"archive State lifetime");
    }
    require(Counted::alive==0,"State/Action lifetime balance");
}

void floating_cases() {
    using F=cs::ChokudaiCopySub<double>;using N=F::Next;
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
    auto r=F::run(p,0,0,expand,hook);
    auto invalid=F::run(p,0,nan,expand,hook);
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
        require(now.state==expected,"deep State");
        const auto to=now.sub_step==(now.turn==0?10000:2)?Next::Turn:Next::Sub;
        e.push(static_cast<int>(expected+1),-now.depth-1,to);
    };
    (void)world;auto r=B::run(p,0,0,expand);
    require(*r.best_state==10004,"deep State result");
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
}

void csv_cases(const std::string& prefix) {
    std::filesystem::create_directories(std::filesystem::path(prefix).parent_path());
    B::Param p;p.max_turn=2;p.beam_width=2;p.sub_beam_width=3;
    auto run=[&](B::CsvStatHook& hook,bool answer) {
        cs::sub_detail::virtual_clock=true;cs::sub_detail::virtual_us=0;
        p.max_turn_is_answer=answer;
        auto expand=[](const auto&now,auto&e){
            const auto to=now.sub_step==2?Next::Turn:Next::Sub;
            e.push(1,-now.depth-1,to);
            cs::sub_detail::virtual_us+=20000;
        };
        auto r=B::run(p,0,0,expand,hook);
        cs::sub_detail::virtual_clock=false;
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
    using Small=cs::ChokudaiCopySub<signed char>;
    Small::CsvStatHook small(prefix+"_small.csv",0);Small::Runtime rt;
    small(Small::EventType::RunStart,rt);
    rt.found=true;rt.best_cost=-7;rt.elapsed_us=1234;
    small(Small::EventType::RunEnd,rt);
    std::locale::global(old);
}
} // namespace sub_test

namespace cs {
// private構造の照合窓口は直接コンパイル時だけ存在する。
struct SubTestAccess {
    static void clock_counter() {
        using namespace sub_test;
        B::Param p; p.max_turn = 1;
        using E = B::Engine<int>; E engine(p, 0, 0, 0ULL, true);
        engine.unchecked_ = std::numeric_limits<int>::max() - 1;
        engine.after_expand(0, false); engine.after_expand(0, false);
        sub_test::require(engine.unchecked_ >= 0 && engine.unchecked_ < std::numeric_limits<int>::max(), "unobserved clock counter does not overflow");
    }
    static void exercise(std::uint64_t operations) {
        using namespace sub_test;
        using E=B::Engine<int>;using Pool=E::Queue;using Item=E::Item;
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
                else if(!pool.empty()&&rng.pick(5)==0) {
                    auto a=pool.pop();auto b=ref.pop();
                    require(a.value==b.state.id&&a.cost==b.cost&&a.hash==b.hash&&a.has_hash==b.hashed,"queue pop/collision deletion");
                }
                else {
                    const long long cost=rng.pick(4)==0?-static_cast<long long>(i):rng.pick(101)-50;
                    const auto hash=i%3==0?colliding[static_cast<std::size_t>(rng.pick(64))]:rng.next();
                    const bool hashed=rng.pick(7)!=0;
                    RefNode node;node.cost=cost;node.hash=hash;node.hashed=hashed;node.serial=serial++;node.state.id=static_cast<int>(i);
                    auto a=pool.choose(cost,hash,hashed,got);auto b=ref.choose(node,want);
                    require(a.slot==b.first&&a.duplicate==b.second,"pool choose");
                    if(a.slot>=0){
                        pool.put(a,Item{cost,hash,{},hashed,node.state.id},node.serial);++got.accepted;if(a.duplicate)++got.replaced_by_hash;
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
} // namespace cs

int main(int argc,char**argv) {
    try {
        int count=argc>1?std::stoi(argv[1]):10000;
        sub_test::basic_cases();sub_test::api_cases();sub_test::floating_cases();
        sub_test::deep_case();sub_test::retention_and_transform_cases();
        sub_test::compound_cases();
        sub_test::csv_cases("build/csv_"+std::to_string(sub_test::mode));
        sub_test::random_cases(count);sub_test::exhaustive_cases(1000);
        cs::SubTestAccess::clock_counter();
        cs::SubTestAccess::exercise(1000000);
        std::cout<<"PASS mode="<<sub_test::mode<<" random="<<count<<" exhaustive=1000 pool_operations=1000000\n";
    } catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}

#endif
