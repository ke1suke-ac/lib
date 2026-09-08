#pragma once
#include <bits/stdc++.h>

// コピー型 chokudai サーチ（コスト最小化、C++20、単一スレッド）
// 各ターンに最大 W=beam_width 件を保持し、1スイープで各ターンから
// 最大 K=chokudai_width 件を展開する。残りの候補は次のスイープへ持ち越す。
// State のコピー・移動を O(S)、最大ターンを T とするとメモリは O(T W S)
// ターンキューは初回の有効候補で確保する。その初回確保は O(W S)
// ハッシュは同じ到着ターンの未展開候補だけを重複排除し、展開済み集合は持たない。
// finished は解候補の印。途中の finished 状態も expand されるので、必要ならそこで打ち切る。
// best_state は受理時にコピーする。履歴が必要な場合は State 自体に持たせる。
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
#if __INCLUDE_LEVEL__ == 0 || defined(CHOKUDAI_TESTING)
// 通常の include 時には存在しない、決定的な時間検証用の差し替え点
struct TestClock {
    static inline bool enabled = false;
    static inline std::int64_t value = 0;
    static inline std::uint64_t calls = 0;
};
#endif
// 単調時計の現在時刻を取得する、O(1)
inline std::int64_t now_us() {
#if __INCLUDE_LEVEL__ == 0 || defined(CHOKUDAI_TESTING)
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
        bool max_turn_is_answer = true;
        bool use_hash_dedup = true;
        int chokudai_width = 1;      // 各ターン・各スイープの展開上限。1 以上へ丸める
        std::int64_t max_sweeps = 0; // 0 以下は無制限
        bool finish_on_timeout = false; // 未発見で時間切れなら K=1 の追加スイープを高々1回
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
        std::int64_t sweep = 0;      // 1 始まり。スイープ未開始なら 0
        bool sweep_completed = false;
        std::int64_t elapsed_us = 0; // 最後に時計を検査した時点
        bool time_limit_reached = false;
        std::uint64_t expanded_nodes = 0; // 完了した expand。実行中の1件は含まない
        std::uint64_t generated_candidates = 0; // 初期状態を含まない
        std::uint64_t accepted_candidates = 0;
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
    struct CsvStatHook {
        struct Row : Runtime {
            const char* event = "";
            double elapsed_ms = 0.0;
            double period_ms = 0.0;
            std::optional<double> expand_per_sec;
            std::optional<double> accept_rate;
            std::uint64_t period_best_updates = 0;
            std::optional<std::int64_t> sweeps_since_best_update;
        };
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
        void append(const char* event, const Runtime& rt) {
            // 期間統計は直前の「記録された行」との差分を使う
            Row row;
            static_cast<Runtime&>(row) = rt;
            row.event = event;
            row.elapsed_ms = static_cast<double>(rt.elapsed_us) / 1000.0;
            if (!rows.empty()) {
                const Row& prev = rows.back();
                const auto dt = rt.elapsed_us - prev.elapsed_us;
                row.period_ms = static_cast<double>(dt) / 1000.0;
                if (dt > 0) row.expand_per_sec = static_cast<double>(rt.expanded_nodes - prev.expanded_nodes) * 1e6 / static_cast<double>(dt);
                const auto dg = rt.generated_candidates - prev.generated_candidates;
                if (dg > 0) row.accept_rate = static_cast<double>(rt.accepted_candidates - prev.accepted_candidates) / static_cast<double>(dg);
                row.period_best_updates = rt.best_update_count - prev.best_update_count;
            }
            if (rt.found) row.sweeps_since_best_update = rt.sweep - rt.last_best_update_sweep;
            rows.push_back(std::move(row));
        }
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
            // 走査を伴う統計計算は行わず、蓄積した行だけを出力する
            std::ofstream out(filename, std::ios::binary | std::ios::trunc);
            if (!out) {
                std::cerr << "chokudai: CSV open failed: " << filename << '\n';
                return;
            }
            out << std::setprecision(std::numeric_limits<long double>::max_digits10);
            out << "event,sweep,sweep_completed,elapsed_ms,time_limit_reached,expanded_nodes,generated_candidates,accepted_candidates,pruned_by_width,replaced_by_width,pruned_by_hash,replaced_by_hash,invalid_candidates,stored_states,peak_stored_states,found,best_cost,best_turn,best_update_count,last_best_update_sweep,stop_reason,period_ms,expand_per_sec,accept_rate,period_best_updates,sweeps_since_best_update\n";
            for (const Row& r : rows) {
                out << r.event << ',' << r.sweep << ',' << r.sweep_completed << ',' << r.elapsed_ms << ',' << r.time_limit_reached << ','
                    << r.expanded_nodes << ',' << r.generated_candidates << ',' << r.accepted_candidates << ','
                    << r.pruned_by_width << ',' << r.replaced_by_width << ',' << r.pruned_by_hash << ',' << r.replaced_by_hash << ','
                    << r.invalid_candidates << ',' << r.stored_states << ',' << r.peak_stored_states << ',' << r.found << ',';
                if (r.found) out << r.best_cost;
                out << ',';
                if (r.found) out << r.best_turn;
                out << ',' << r.best_update_count << ',';
                if (r.found) out << r.last_best_update_sweep;
                out << ',' << reason_name(r.stop_reason) << ',' << r.period_ms << ',';
                if (r.expand_per_sec) out << *r.expand_per_sec;
                out << ',';
                if (r.accept_rate) out << *r.accept_rate;
                out << ',' << r.period_best_updates << ',';
                if (r.sweeps_since_best_update) out << *r.sweeps_since_best_update;
                out << '\n';
            }
            out.close();
            if (!out) std::cerr << "chokudai: CSV write failed: " << filename << '\n';
        }
    };
    template<class State>
    struct Result {
        bool found = false;
        bool time_limit_reached = false;
        bool exhausted = false;
        bool completed_max_turn = false; // 最大ターンの候補が1件でも受理された
        Cost best_cost{};
        int best_turn = 0;
        std::optional<State> best_state;
        Runtime runtime;

        bool sweep_limit_reached = false;
    };
    template<class State>
    class Emitter {
    public:
    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, 1, cost, now_hash_, false, false, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class B>
    requires StateArg<U, State> && BoolArg<B>
    bool push(U&& state, Cost cost, B finished) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, 1, cost, now_hash_, false, finished, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class H>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, 1, cost, static_cast<Hash>(hash), true, false, maker);
    }

    // 状態を候補として追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class U, class H, class B>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push(U&& state, Cost cost, H hash, B finished) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return engine_.push_candidate(turn_, 1, cost, static_cast<Hash>(hash), true, finished, maker);
    }

    // 受理時だけ状態を生成して候補を追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker>
    bool push_lazy(Cost cost, Maker&& maker) {
        return engine_.push_candidate(turn_, 1, cost, now_hash_, false, false, std::forward<Maker>(maker));
    }

    // 受理時だけ状態を生成して候補を追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class B>
    requires BoolArg<B>
    bool push_lazy(Cost cost, B finished, Maker&& maker) {
        return engine_.push_candidate(turn_, 1, cost, now_hash_, false, finished, std::forward<Maker>(maker));
    }

    // 受理時だけ状態を生成して候補を追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class H>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, Maker&& maker) {
        return engine_.push_candidate(turn_, 1, cost, static_cast<Hash>(hash), true, false, std::forward<Maker>(maker));
    }

    // 受理時だけ状態を生成して候補を追加する、期待償却 O(log W + S)、到着キュー初回確保は O(W S)
    template<class Maker, class H, class B>
    requires CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push_lazy(Cost cost, H hash, B finished, Maker&& maker) {
        return engine_.push_candidate(turn_, 1, cost, static_cast<Hash>(hash), true, finished, std::forward<Maker>(maker));
    }

    private:
        friend class Engine<State>;
        Emitter(Engine<State>& e, int t, Hash h) noexcept : engine_(e), turn_(t), now_hash_(h) {}
        Engine<State>& engine_;
        int turn_;
        Hash now_hash_;
    };
private:
    template<class State>
    class Engine {
        static_assert(std::copy_constructible<State>, "State must be copy constructible");
        static_assert(std::move_constructible<State>, "State must be move constructible");
        static_assert(std::assignable_from<State&, State>, "State must be assignable");
        struct Item { State state; Cost cost; Hash hash; bool has_hash; };
        struct Key { Cost cost{}; std::uint64_t serial = 0; };
        struct Ends { int best = -1; int worst = -1; };
        // 状態本体と比較用情報を分け、最良・最悪の両端を同じ木で管理する
        class Queue {
        public:
            std::vector<std::optional<Item>> items;
            std::vector<Key> keys;
            std::vector<int> free_slots;
            std::vector<Ends> tree;
            struct Bucket { Hash hash{}; int slot = -1; }; // -1=空、-2=削除済み
            std::vector<Bucket> table;
            std::size_t base = 1;
            std::size_t hash_size = 0;
            std::size_t tombstones = 0;
            int size = 0;
            // 空のターンキューを確保する、O(W)
            Queue(int width, std::size_t capacity) : items(static_cast<std::size_t>(width)),
                keys(static_cast<std::size_t>(width)), hash_size(capacity) {
                // 葉を2冪にそろえ、空きスロットは小さい番号から使う
                while (base < static_cast<std::size_t>(width)) base *= 2;
                tree.resize(base * 2);
                free_slots.reserve(static_cast<std::size_t>(width));
                for (int i = width; i > 0; --i) free_slots.push_back(i - 1);
            }
            // 最良スロットを取得する、O(1)
            int best() const { return tree[1].best; }
            // 最悪スロットを取得する、O(1)
            int worst() const { return tree[1].worst; }
            // キーに対応する現在保持中のスロットを探す、期待 O(1)
            int find(Hash hash) const {
                if (table.empty()) return -1;
                std::size_t pos = bucket(hash);
                while (table[pos].slot != -1) {
                    if (table[pos].slot >= 0 && table[pos].hash == hash) return table[pos].slot;
                    pos = (pos + 1) & (table.size() - 1);
                }
                return -1;
            }
            // 指定スロットを取り除く、期待 O(log W + S)
            void erase(int slot) {
                // 削除マークは再挿入時に回収し、必要に応じて表全体を再構築する
                const auto id = static_cast<std::size_t>(slot);
                if (items[id]->has_hash && !table.empty()) {
                    std::size_t pos = bucket(items[id]->hash);
                    while (table[pos].slot != slot) pos = (pos + 1) & (table.size() - 1);
                    table[pos].slot = -2;
                    ++tombstones;
                }
                items[id].reset();
                --size;
                free_slots.push_back(slot);
                update(slot, false);
            }
            // 検査済みの候補を指定スロットへ格納する、期待償却 O(log W + S)
            template<class Maker>
            void insert(int slot, Cost cost, Hash hash, bool hashed, std::uint64_t serial, Maker&& maker) {
                // maker の例外が出た場合には古い候補をまだ取り除いていない
                State made(std::invoke(std::forward<Maker>(maker)));
                if (items[static_cast<std::size_t>(slot)]) erase(slot);
                assert(!free_slots.empty() && free_slots.back() == slot);
                free_slots.pop_back();
                const auto id = static_cast<std::size_t>(slot);
                items[id].emplace(Item{std::move(made), cost, hash, hashed});
                keys[id] = Key{cost, serial};
                ++size;
                update(slot, true);
                if (hashed && hash_size > 0) {
                    if (table.empty() || tombstones > items.size()) rebuild();
                    else add_hash(hash, slot);
                }
            }
        private:
            bool before(int a, int b) const {
                const Key& x = keys[static_cast<std::size_t>(a)];
                const Key& y = keys[static_cast<std::size_t>(b)];
                if (x.cost != y.cost) return x.cost < y.cost;
                return x.serial < y.serial;
            }
            void update(int slot, bool active) {
                // 空の子を除外して、同点なら受理順が早いものを best にする
                std::size_t pos = base + static_cast<std::size_t>(slot);
                tree[pos] = active ? Ends{slot, slot} : Ends{};
                for (pos /= 2; pos > 0; pos /= 2) {
                    const Ends a = tree[pos * 2], b = tree[pos * 2 + 1];
                    if (a.best < 0) tree[pos] = b;
                    else if (b.best < 0) tree[pos] = a;
                    else tree[pos] = Ends{before(a.best, b.best) ? a.best : b.best,
                                          before(a.worst, b.worst) ? b.worst : a.worst};
                }
            }
            std::size_t bucket(Hash hash) const {
                return static_cast<std::size_t>(single_detail::mix(static_cast<std::uint64_t>(hash))) & (table.size() - 1);
            }
            void add_hash(Hash hash, int slot) {
                std::size_t pos = bucket(hash);
                while (table[pos].slot >= 0) pos = (pos + 1) & (table.size() - 1);
                if (table[pos].slot == -2) --tombstones;
                table[pos] = Bucket{hash, slot};
            }
            void rebuild() {
                // 表は2W以上の空間を確保し、削除マークが W を超えたら再構築する
                table.assign(hash_size, Bucket{});
                tombstones = 0;
                for (std::size_t i = 0; i < items.size(); ++i) {
                    if (items[i] && items[i]->has_hash) add_hash(items[i]->hash, static_cast<int>(i));
                }
            }
        };
        Param p_;
        Runtime rt_;
        std::optional<State> best_state_;
        std::vector<std::unique_ptr<Queue>> layers_;
        std::uint64_t pending_ = 0;
        std::uint64_t serial_ = 0;
        std::size_t hash_size_ = 0;
        std::int64_t start_us_ = 0;
        std::int64_t limit_us_ = 0;
        int unchecked_ = 0;
        bool completed_max_turn_ = false;
    public:
        // 初期状態とキューを準備する、O(T + W S)
        Engine(const Param& param, State&& initial, Cost cost, Hash hash, bool hashed) : p_(param) {
            start_us_ = single_detail::now_us();
            // サイズ計算は符号付き int の加算・乗算を避ける
            p_.max_turn = std::max(0, p_.max_turn);
            p_.beam_width = std::max(1, p_.beam_width);
            p_.chokudai_width = std::max(1, p_.chokudai_width);
            p_.time_check_interval = std::max(1, p_.time_check_interval);
            limit_us_ = single_detail::to_us(p_.time_limit_ms);
            if (p_.use_hash_dedup) {
                const auto w = static_cast<std::uint64_t>(p_.beam_width);
                const auto requested = p_.hash_capacity <= 0 ? 4 * w + 17 : static_cast<std::uint64_t>(p_.hash_capacity);
                const auto capacity = std::clamp(requested, 2 * w + 1, 4 * w + 17);
                hash_size_ = 1;
                while (hash_size_ < capacity) hash_size_ *= 2;
            }
            layers_.resize(static_cast<std::size_t>(p_.max_turn) + 1);
            completed_max_turn_ = p_.max_turn == 0;
            // NaN の初期コストでは探索を開始しない（push の候補数には含めない）
            if (valid_cost(cost)) {
                Queue& q = layer(0);
                q.insert(q.free_slots.back(), cost, hash, hashed, serial_++, [&]() -> State&& { return std::move(initial); });
                rt_.stored_states = rt_.peak_stored_states = 1;
                pending_ = p_.max_turn > 0 ? 1 : 0;
                if (p_.max_turn == 0 && p_.max_turn_is_answer) save_best(*q.items[0], 0);
            }
        }
        // 候補を検査し、受理時だけ状態を生成する、期待償却 O(log W + S)
        template<class Maker>
        bool push_candidate(int from, int step, Cost cost, Hash hash, bool has_hash, bool finished, Maker&& maker) {
            static_assert(std::invocable<Maker&&>, "maker must be callable without arguments");
            static_assert(std::constructible_from<State, std::invoke_result_t<Maker&&>>, "maker must produce State");
            ++rt_.generated_candidates;
            // 到着ターンの加算前に検査し、巨大な step でもオーバーフローさせない
            if (!valid_cost(cost) || step <= 0 || step > 1 || step > p_.max_turn - from) {
                ++rt_.invalid_candidates;
                return false;
            }
            const int turn = from + step;
            Queue& q = layer(turn);
            const bool full = q.size == p_.beam_width;
            // 満杯時は幅による棄却を先に判定する。同点は既存候補を優先する
            if (full && !(cost < q.keys[static_cast<std::size_t>(q.worst())].cost)) {
                ++rt_.pruned_by_width;
                return false;
            }
            int slot = has_hash && p_.use_hash_dedup ? q.find(hash) : -1;
            const bool duplicate = slot >= 0;
            if (duplicate && !(cost < q.keys[static_cast<std::size_t>(slot)].cost)) {
                ++rt_.pruned_by_hash;
                return false;
            }
            const bool replacing = duplicate || full;
            if (!duplicate) slot = full ? q.worst() : q.free_slots.back();
            q.insert(slot, cost, hash, has_hash, serial_++, std::forward<Maker>(maker));
            ++rt_.accepted_candidates;
            if (duplicate) ++rt_.replaced_by_hash;
            else if (full) ++rt_.replaced_by_width;
            if (!replacing) {
                ++rt_.stored_states;
                if (turn < p_.max_turn) ++pending_;
                rt_.peak_stored_states = std::max(rt_.peak_stored_states, rt_.stored_states);
            }
            if (turn == p_.max_turn) completed_max_turn_ = true;
            if (finished || (p_.max_turn_is_answer && turn == p_.max_turn)) save_best(*q.items[static_cast<std::size_t>(slot)], turn);
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
            sample_time();
            rt_.stop_reason = reason();
            if (rt_.stop_reason == StopReason::TimeLimit && p_.finish_on_timeout && !rt_.found && pending_ > 0) {
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
        Queue& layer(int turn) {
            auto& q = layers_[static_cast<std::size_t>(turn)];
            if (!q) q = std::make_unique<Queue>(p_.beam_width, hash_size_);
            return *q;
        }
        void save_best(const Item& item, int turn) {
            // 受理時の解をコピーし、後から同じスロットが置換されても失わない
            if (!rt_.found || item.cost < rt_.best_cost) {
                best_state_.emplace(item.state);
                rt_.found = true;
                rt_.best_cost = item.cost;
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
            if (p_.max_sweeps > 0 && rt_.sweep >= p_.max_sweeps) return StopReason::SweepLimit;
            return StopReason::None;
        }
        bool remaining_in_sweep(int turn, int used, int width) const {
            if (used < width && layers_[static_cast<std::size_t>(turn)]->size > 0) return true;
            for (int t = turn + 1; t < p_.max_turn; ++t) {
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
            const int width = fallback ? 1 : p_.chokudai_width;
            bool stopped = false;
            // 空の中間ターンを飛ばし、同じスイープ中に生成した未来の候補も展開する
            for (int turn = 0; turn < p_.max_turn && !stopped; ++turn) {
                auto& ptr = layers_[static_cast<std::size_t>(turn)];
                if (!ptr) continue;
                Queue& q = *ptr;
                for (int k = 0; k < width && q.size > 0; ++k) {
                    const int slot = q.best();
                    Item item = std::move(*q.items[static_cast<std::size_t>(slot)]);
                    q.erase(slot);
                    --rt_.stored_states;
                    --pending_;
                    StateView<State> now(turn, item.cost, item.state, item.hash, item.has_hash);
                    Emitter<State> emit(*this, turn, item.hash);
                    // Runtime ありの形式を優先し、利用するだけでは時計を追加で読まない
                    if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
                        expand(now, static_cast<const Runtime&>(rt_), emit);
                    } else {
                        static_assert(std::is_invocable_v<Expand&, const StateView<State>&, Emitter<State>&>, "invalid expand signature; Runtime must be const");
                        expand(now, emit);
                    }
                    ++rt_.expanded_nodes;
                    if (!fallback && limit_us_ > 0 && ++unchecked_ >= p_.time_check_interval) sample_time();
                    if ((!fallback && rt_.time_limit_reached) || (fallback && rt_.found)) {
                        rt_.sweep_completed = !remaining_in_sweep(turn, k + 1, width);
                        stopped = true;
                        break;
                    }
                }
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
public:
    // ハッシュありで探索する、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
    template<class State, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State> run(const Param& param, State initial_state, Cost initial_cost,
                             H initial_hash, Expand&& expand, DebugHook&& debug_hook = DebugHook{}) {
        Engine<State> engine(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true);
        return engine.run(std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));
    }
    // ハッシュなしで探索する、候補数 G に対し期待 O(T W S + G(log W + S) + スイープ数 T)
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
          typename B::hash_type hash = {}, bool finished = false) {
    if constexpr (requires { typename B::Param{}.max_step; }) {
        if (hashed) return e.push(std::forward<U>(state), cost, hash, step, finished);
        return e.push(std::forward<U>(state), cost, step, finished);
    } else {
        (void)step;
        if (hashed) return e.push(std::forward<U>(state), cost, hash, finished);
        return e.push(std::forward<U>(state), cost, finished);
    }
}
template<class B, class S, class Maker>
bool lazy(typename B::template Emitter<S>& e, typename B::cost_type cost, int step,
          bool hashed, typename B::hash_type hash, bool finished, Maker&& maker) {
    if constexpr (requires { typename B::Param{}.max_step; }) {
        if (hashed) return e.push_lazy(cost, hash, step, finished, std::forward<Maker>(maker));
        return e.push_lazy(cost, step, finished, std::forward<Maker>(maker));
    } else {
        (void)step;
        if (hashed) return e.push_lazy(cost, hash, finished, std::forward<Maker>(maker));
        return e.push_lazy(cost, finished, std::forward<Maker>(maker));
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
    bool finished;
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
        r.pruned_by_hash, r.replaced_by_hash, r.invalid_candidates, r.stored_states,
        r.peak_stored_states, r.found, r.best_cost, r.best_turn, r.best_update_count,
        r.last_best_update_sweep, r.stop_reason);
}
void invariant(const BS::Runtime& r) {
    CHECK(r.generated_candidates == r.accepted_candidates + r.pruned_by_width + r.pruned_by_hash + r.invalid_candidates);
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
        p.chokudai_width = std::max(1, p.chokudai_width);
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
        if (t == p.max_turn) completed = true;
        if (e.finished || (p.max_turn_is_answer && t == p.max_turn)) save(candidate, t);
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
                for (int k = 0; k < p.chokudai_width && !q.empty(); ++k) {
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
    std::uint64_t made = 0;
    const auto result = BS::run(param, State{}, 0LL, std::uint64_t{0},
        [&](const BS::StateView<State>& now, const BS::Runtime& rt, BS::Emitter<State>& e) {
            invariant(rt);
            trace.emplace_back(now.turn, now.state.node, now.state.path, now.cost, now.hash, now.has_hash);
            const auto& edges = graph[static_cast<std::size_t>(now.state.node)];
            for (std::size_t i = 0; i < edges.size(); ++i) {
                const auto& edge = edges[i];
                bool accepted;
                if (use_lazy) accepted = lazy<BS, State>(e, edge.cost, edge.step, edge.hashed, edge.hash, edge.finished, [&]() {
                    ++made; return child(now.state, static_cast<int>(i), edge.node);
                });
                else accepted = push<BS>(e, child(now.state, static_cast<int>(i), edge.node), edge.cost, edge.step, edge.hashed, edge.hash, edge.finished);
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
    if (use_lazy) CHECK(made == result.runtime.accepted_candidates);
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
        for (int k : {1,2,width,width+1}) { p.chokudai_width = k; compare(graph,p); }
    }
    p = {}; p.max_turn = 2; p.beam_width = 1; p.max_turn_is_answer = false;
    graph[0] = {{1,1,10,1,false,true},{2,1,1,2,false,false}};
    CHECK(compare(graph,p).best_cost == 10);
    std::reverse(graph[0].begin(),graph[0].end()); CHECK(!compare(graph,p).found);
    p.beam_width = 3;
    graph[0] = {{1,1,10,7,true,false},{2,1,5,8,true,false},{3,1,5,7,true,false}};
    graph[1].clear();graph[2].clear();graph[3].clear();
    auto r=compare(graph,p); CHECK(r.runtime.replaced_by_hash == 1);
    p.max_turn=0;CHECK(!compare(graph,p).found);
    p.max_turn_is_answer=true;CHECK(compare(graph,p).best_turn==0);
    p.max_turn=-7;p.beam_width=-1;p.chokudai_width=0;compare(graph,p);
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
        BS::Param p; p.max_turn=pick(11);p.beam_width=1+pick(16);p.chokudai_width=1+pick(20);
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
        BS::Param p;p.max_turn=static_cast<int>(rng()%7);p.beam_width=1024;p.chokudai_width=3;p.use_hash_dedup=false;
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
                if(e.finished||t+e.step==p.max_turn)if(!optimum||e.cost<*optimum)optimum=e.cost;
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
    BS::Param p;p.max_turn=20;p.beam_width=33;p.chokudai_width=2;p.max_sweeps=40;p.hash_capacity=1;
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
            Tracked a(1);CHECK(push<BS>(e,std::move(a),10));CHECK(a.moved);
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
        CHECK(push<F>(e,1,std::numeric_limits<double>::infinity()));
        CHECK(push<F>(e,2,-0.));CHECK(push<F>(e,3,+0.));
        CHECK(push<F>(e,4,-std::numeric_limits<double>::infinity()));
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
    BS::Param p;p.max_turn=5;p.beam_width=2;p.time_limit_ms=1;p.time_check_interval=1;
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
    p.time_check_interval=1;p.finish_on_timeout=true;
    auto d=run(1000,false,false);CHECK(d.found&&d.runtime.sweep==2&&d.runtime.expanded_nodes==5);
    CHECK(d.time_limit_reached&&!d.exhausted);
    auto e=run(1000,false,true);CHECK(e.runtime.sweep==1);
    p.max_turn_is_answer=false;
    auto f=run(1000,false,false);CHECK(!f.found&&f.runtime.sweep==2&&f.runtime.expanded_nodes==5);
    p.time_limit_ms=0;p.max_sweeps=1;
    auto g=run(1000,false,false);CHECK(g.exhausted&&!g.sweep_limit_reached);
    // RunStart のコストで時間切れになる場合、通常スイープを始めない
    p.time_limit_ms=1;p.finish_on_timeout=false;p.max_turn=3;Clock::value=0;
    const auto h=BS::run(p,0,0LL,[](const auto&,auto&){CHECK(false);},[](BS::EventType event,const auto&){if(event==BS::EventType::RunStart)Clock::value=1000;});
    CHECK(h.time_limit_reached&&h.runtime.sweep==0);
    Clock::value=0;p.finish_on_timeout=true;
    const auto dead=BS::run(p,0,0LL,[](const auto&,auto&){},[](BS::EventType event,const auto&){if(event==BS::EventType::RunStart)Clock::value=1000;});
    CHECK(!dead.found&&dead.runtime.sweep==1&&dead.runtime.expanded_nodes==1&&dead.time_limit_reached);
    // best 更新回数に比例した時計読み出しがないことを確認
    auto many=[&](int count){Clock::value=0;Clock::calls=0;BS::Param mp;mp.max_turn=1;
        auto result=BS::run(mp,0,0LL,[&](const auto&,BS::Emitter<int>& emit){for(int i=0;i<count;++i)push<BS>(emit,i,-static_cast<long long>(i));});
        CHECK(result.runtime.best_update_count==static_cast<std::uint64_t>(count));return Clock::calls;};
    const auto one=many(1);CHECK(many(1000)==one);
}
void csv() {
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
    CHECK(hook.rows[1].elapsed_ms==40&&hook.rows[2].elapsed_ms==80);
    CHECK(*hook.rows[1].expand_per_sec==750&&*hook.rows[2].expand_per_sec==500&&*hook.rows[3].expand_per_sec==0);
    CHECK(*hook.rows[1].accept_rate==0.5&&!hook.rows[3].accept_rate);
    CHECK(!hook.rows[3].sweeps_since_best_update);
    std::ifstream in(path);std::string line;int lines=0;
    while(std::getline(in,line)){++lines;CHECK(std::count(line.begin(),line.end(),',')==25);}
    CHECK(lines==5);in.close();
    rt={};hook(BS::EventType::RunStart,rt);hook(BS::EventType::RunEnd,rt);CHECK(hook.rows.size()==2);
    BS::CsvStatHook zero(path.string(),0);zero(BS::EventType::RunStart,rt);zero(BS::EventType::SweepEnd,rt);zero(BS::EventType::RunEnd,rt);CHECK(zero.rows.size()==3);
    std::filesystem::remove(path);
    BS::CsvStatHook bad((path/"missing.csv").string(),0);
    BS::Param p;const auto r=BS::run(p,0,0LL,[](const auto&,auto&){},bad);CHECK(r.found);
}
static_assert(cs::CostType<int> && cs::CostType<long long> && cs::CostType<double> && !cs::CostType<unsigned> && !cs::CostType<bool>);
static_assert(cs::HashType<std::uint8_t> && cs::HashType<std::uint16_t> && cs::HashType<std::uint32_t> && cs::HashType<std::uint64_t>);
static_assert(!cs::HashType<bool> && !cs::HashType<int>);
static_assert(!cs::CompatibleHashArg<std::uint64_t,std::uint32_t> && !cs::CompatibleHashArg<int,std::uint64_t>);
static_assert(!std::default_initializable<BS::Emitter<int>>);
static_assert(!std::default_initializable<Tracked>);
} // namespace
int main() {
    deterministic();state_and_numeric();time_and_events();csv();
    Clock::value=0;randomized();exhaustive();stress();
    std::cout<<"PASS single checks="<<checks<<'\n';
}
#undef CHECK
#endif
