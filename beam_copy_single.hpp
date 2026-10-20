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
    const long double us = static_cast<long double>(time_limit_ms) * 1000.0L;
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
inline std::int64_t now_microseconds() {
#if __INCLUDE_LEVEL__ == 0
    ++TestClock::calls;
    if (TestClock::enabled) return TestClock::value;
#endif
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
// time_limit_ms は探索全体のsoft limit。expand実行中は中断しない。
// use_turn_time_limit=trueでは各turn開始時に残り時間/残りturn数を配分する。
// turn期限後は未来候補があれば進み、なければ候補ができるか尽きるまで展開する。
// turn期限だけでは保持幅を縮めず、次turnで通常展開へ戻る。time_check_interval=1なら毎展開確認。
// 全体制限後は、解保存済みまたはcontinue_on_timeout=falseなら終了する。
// 未発見の救済時は、未展開候補から最良1本だけを追加展開する。
// 以後は各turnで最良1本だけを進め、到着候補も最良1件だけをmaterializeする。
// 解候補は is_answer=true、または max_turn_is_answer=true で最大ターンに到達した状態。
struct Param {
    int max_turn = 0;               // 初期状態を0とする最大ターン。負値は0に丸める。
    int beam_width = 1;             // 各frontier更新で残す候補数。
    int hash_capacity = 0;          // use_hash_dedup=true かつ 0なら自動設定。過大値もbeam_width基準で丸める。

    double time_limit_ms = 0.0;    // 0なら無制限。全体のsoft limit。到達後の救済はcontinue_on_timeoutで指定。
    int time_check_interval = 64;   // 何ノード展開ごとに時間を見るか。expand中は中断しない。
    bool continue_on_timeout = true; // 時間切れで解未発見なら救済探索を続ける。
    bool expand_answers = true; // falseなら有効解を保存のみ行い、探索候補へ登録しない。
    bool max_turn_is_answer = true; // trueなら turn==max_turn の状態を解候補として扱う。
    bool use_hash_dedup = true;     // trueならHash付きpushで同一frontier内の重複排除を行う。Hashなしpushは対象外。
    bool use_turn_time_limit = false; // 残り時間を残りturnへ配分。全体制限が有効な場合だけ使用。
};

// Hookの発火タイミング。
enum class EventType { RunStart, RunEnd, TurnEnd };

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

// 候補群のコスト分布。count=0のbest/worst/average()は0。
struct CostStats {
    int count = 0;
    Cost best{}, worst{};
    double sum = 0.0;
    bool add(Cost cost) {
        const bool improved = count == 0 || cost < best;
        if (improved) best = cost;
        if (count == 0 || worst < cost) worst = cost;
        ++count; sum += static_cast<double>(cost);
        return improved;
    }
    double average() const { return count > 0 ? sum / count : 0.0; }
};

// Hookから観測できる実行時情報。
// expanded/generated/accepted/selected/best などの累積カウンタは常に更新される。
// live_*、selected_* などのsnapshot系は DebugHook有効時、またはRunEndの最終Result用に更新される。
// best更新からの停滞量は、NoOp実行後の Result::runtime でも最終値を確認できる。
struct Runtime {
    Param param; // 探索に使用する設定。Engineとの二重保持を避ける。

    std::uint64_t terminal_candidates = 0; // 有効なmax_turn到着候補数。候補プールには入れない。
    std::uint64_t answer_only_candidates = 0; // expand_answers=falseで除外したmax_turn未満の有効候補数。
    // 基本情報。
    int turn = 0;                   // 現在処理中のターン。初期状態は0。
    int materialized_turn = 0;     // 到着候補の確定処理済みの最大turn。候補0件の場合も含む。

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
    long long accepted_candidates = 0;  // この呼び出しで探索候補群に保存した数。最良解だけの保存やmaker呼び出し数とは別。
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

    double time_remaining_ms = -1.0;
    std::int64_t elapsed_us = 0;
    // turn時刻はrun開始からのus。配分無効・turn未開始はstart/deadline=-1。
    // elapsedは最後の時計確認時点。TurnEndで固定し、最終復元・RunEndには加算しない。
    std::int64_t turn_start_us = -1, turn_deadline_us = -1, turn_elapsed_us = 0;
    bool turn_time_limit_reached = false; // 各turn開始時に解除。全体のtime_limit_reachedとは独立。
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;

    CostStats live_costs{}; // 最良・最悪・合計・件数を一つに保持。

    CostStats selected_costs{}; // 最良・最悪・合計・件数を一つに保持。

    int width_pruned_cost_count = 0;
    Cost width_pruned_best_cost{};
    double pruned_best_gap = 0.0;

    // best更新後の停滞量。NoOp実行でもRunEnd前に更新される。
    int turns_since_best_update = 0; // 最良解を生成した探索turnからの経過。解の到着turnとは別。
    long long expanded_nodes_since_best_update = 0;
    double ms_since_best_update = 0.0;

    bool found = false;
    Cost best_cost{};
    int best_turn = 0;
    int best_update_count = 0;

    bool exhausted = false;
    // 不正でないmax_turn到着候補を少なくとも1件生成した。
    // 全候補を通常幅で展開した保証ではない。
    bool completed_max_turn = false;
};

// デフォルトHook。何もしない。
struct NoOp {
    void operator()(EventType, const Runtime&) const noexcept {}
};

// CSV出力Hook。
// RunStart/TurnEnd/RunEnd の統計をCSVに出力する。
// 行はメモリ上に保持し、RunEndで一括してファイルへ書き出す。
// LOCAL定義時のみCSVを記録する。未定義時は同じ呼び出し方で使える空実装。
struct CsvStatHook {
#ifdef LOCAL
    struct Row : Runtime { const char* event = ""; };
    std::string filename;
    std::int64_t interval_us;
    std::vector<Row> rows;
    std::int64_t next_record_us = 0;
    bool started = false;

    explicit CsvStatHook(std::string path = "beam_stat.csv", double interval_ms = 50.0)
        : filename(std::move(path)), interval_us(detail::time_limit_to_microseconds(interval_ms)) {}

    void operator()(EventType event, const Runtime& rt) {
        if (event == EventType::RunStart) { rows.clear(); started = true; }
        if (!started) return;
        if (event != EventType::RunStart && event != EventType::RunEnd &&
            (event != EventType::TurnEnd || (interval_us > 0 && rt.elapsed_us < next_record_us))) return;
        const char* name = event == EventType::RunStart ? "RunStart" :
                           event == EventType::RunEnd ? "RunEnd" : "TurnEnd";
        rows.push_back({rt, name});
        next_record_us = rt.elapsed_us + std::min(interval_us,
            std::numeric_limits<std::int64_t>::max() - rt.elapsed_us);
        if (event == EventType::RunEnd) { write_csv(); started = false; }
    }

private:
    void write_csv() const {
        std::ofstream out(filename, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "[bs] warning: failed to open csv file: " << filename << '\n';
            return;
        }
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<long double>::max_digits10);
        out << "event,elapsed_ms,turn,materialized_turn,max_turn,beam_width,hash_capacity,"
               "active_width,selected_width,candidate_count,max_active_width,max_selected_width,beam_saturated_turns,"
               "expanded_nodes,terminal_candidates,answer_only_candidates,continue_on_timeout,expand_answers,generated_candidates,accepted_candidates,selected_nodes,"
               "pruned_by_width,pruned_by_hash,duplicate_by_hash,replaced_by_width,replaced_by_hash,invalid_candidates,hash_rebuild_count,"
               "live_nodes,max_live_nodes,"
               "time_limit_ms,time_remaining_ms,use_turn_time_limit,turn_start_ms,turn_deadline_ms,turn_budget_ms,turn_elapsed_ms,turn_time_limit_reached,"
               "live_best_cost,live_worst_cost,live_avg_cost,"
               "selected_best_cost,selected_worst_cost,selected_avg_cost,"
               "width_pruned_cost_count,width_pruned_best_cost,pruned_best_gap,"
               "turns_since_best_update,expanded_nodes_since_best_update,ms_since_best_update,"
               "found,best_cost,best_turn,best_update_count,time_limit_reached,exhausted,completed_max_turn,"
               "period_ms,period_turns,period_expanded_nodes,period_generated_candidates,period_accepted_candidates,period_selected_nodes,"
               "period_pruned_by_width,period_pruned_by_hash,period_duplicate_by_hash,period_replaced_by_width,period_replaced_by_hash,period_invalid_candidates,period_hash_rebuild_count,period_best_updates,"
               "expand_per_sec,generate_per_sec,accept_per_sec,select_per_sec,accept_rate,width_prune_rate,hash_prune_rate,"
               "branching_factor,duplicate_rate,accepted_per_expanded,selected_per_expanded\n";
        // 前回記録行との差分を出力時に計算し、同じ統計を二重に保持しない。
        auto rate = [](auto numerator, auto denominator) {
            return denominator > 0 ? static_cast<double>(numerator) / static_cast<double>(denominator) : 0.0;
        };
        auto ms = [](std::int64_t us) { return static_cast<double>(us) * 0.001; };
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            const auto& prev = rows[i == 0 ? 0 : i - 1];
            const double period_ms = ms(r.elapsed_us) - ms(prev.elapsed_us);
            auto cells = [&](const auto&... values) { ((out << ',' << +values), ...); };
            out << r.event;
            cells(ms(r.elapsed_us), r.turn, r.materialized_turn,
                  r.param.max_turn, r.param.beam_width, r.param.hash_capacity,
                  r.active_width, r.selected_width, r.candidate_count,
                  r.max_active_width, r.max_selected_width, r.beam_saturated_turns,
                  r.expanded_nodes, r.terminal_candidates, r.answer_only_candidates,
                  r.param.continue_on_timeout, r.param.expand_answers, r.generated_candidates,
                  r.accepted_candidates, r.selected_nodes, r.pruned_by_width,
                  r.pruned_by_hash, r.duplicate_by_hash, r.replaced_by_width,
                  r.replaced_by_hash, r.invalid_candidates, r.hash_rebuild_count,
                  r.live_nodes, r.max_live_nodes, r.param.time_limit_ms,
                  r.time_remaining_ms, r.param.use_turn_time_limit, (r.turn_start_us < 0 ? -1.0 : ms(r.turn_start_us)),
                  (r.turn_deadline_us < 0 ? -1.0 : ms(r.turn_deadline_us)), (r.turn_deadline_us < 0 ? -1.0 : ms(r.turn_deadline_us - r.turn_start_us)), ms(r.turn_elapsed_us),
                  r.turn_time_limit_reached, r.live_costs.best, r.live_costs.worst,
                  r.live_costs.average(), r.selected_costs.best, r.selected_costs.worst,
                  r.selected_costs.average(), r.width_pruned_cost_count, r.width_pruned_best_cost,
                  r.pruned_best_gap, r.turns_since_best_update, r.expanded_nodes_since_best_update,
                  r.ms_since_best_update, r.found);
            out << ','; if (r.found) out << +r.best_cost;
            out << ','; if (r.found) out << +r.best_turn;
            cells(r.best_update_count, r.time_limit_reached, r.exhausted,
                  r.completed_max_turn, period_ms, (r.materialized_turn - prev.materialized_turn),
                  (r.expanded_nodes - prev.expanded_nodes), (r.generated_candidates - prev.generated_candidates), (r.accepted_candidates - prev.accepted_candidates),
                  (r.selected_nodes - prev.selected_nodes), (r.pruned_by_width - prev.pruned_by_width), (r.pruned_by_hash - prev.pruned_by_hash),
                  (r.duplicate_by_hash - prev.duplicate_by_hash), (r.replaced_by_width - prev.replaced_by_width), (r.replaced_by_hash - prev.replaced_by_hash),
                  (r.invalid_candidates - prev.invalid_candidates), (r.hash_rebuild_count - prev.hash_rebuild_count), (r.best_update_count - prev.best_update_count),
                  rate(static_cast<double>(r.expanded_nodes - prev.expanded_nodes) * 1000.0, period_ms), rate(static_cast<double>(r.generated_candidates - prev.generated_candidates) * 1000.0, period_ms), rate(static_cast<double>(r.accepted_candidates - prev.accepted_candidates) * 1000.0, period_ms),
                  rate(static_cast<double>(r.selected_nodes - prev.selected_nodes) * 1000.0, period_ms), rate((r.accepted_candidates - prev.accepted_candidates), (r.generated_candidates - prev.generated_candidates)), rate((r.pruned_by_width - prev.pruned_by_width), (r.generated_candidates - prev.generated_candidates)),
                  rate((r.pruned_by_hash - prev.pruned_by_hash), (r.generated_candidates - prev.generated_candidates)), rate((r.generated_candidates - prev.generated_candidates), (r.expanded_nodes - prev.expanded_nodes)), rate((r.duplicate_by_hash - prev.duplicate_by_hash), (r.generated_candidates - prev.generated_candidates)),
                  rate((r.accepted_candidates - prev.accepted_candidates), (r.expanded_nodes - prev.expanded_nodes)), rate((r.selected_nodes - prev.selected_nodes), (r.expanded_nodes - prev.expanded_nodes)));
            out << '\n';
        }
        out.close();
        if (!out) std::cerr << "[bs] warning: failed to write csv file: " << filename << '\n';
    }
#else
    explicit CsvStatHook(std::string = "beam_stat.csv", double = 50.0) {}
    void operator()(EventType, const Runtime&) {}
#endif
};

// 探索結果。
// Action型を指定した場合は操作列を共有木で保持し、pathに返す。
// exhausted=true は、時間制限による途中終了ではなく、展開可能なfrontierが尽きたことを表す。
template<class State, class Action = void> struct Result;
    template<class State> struct Result<State, void> {
    bool found = false;
    // time_limit_msに達したことを検出したか。
    // 検出後に探索を継続する場合は最良1本の縮退探索へ移る。
    // expand中の即時中断を意味しない。
    bool time_limit_reached = false;
    bool exhausted = false;
    // 不正でないmax_turn到着候補を少なくとも1件生成した。
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
// 有効解は幅・Hashの選別前に保存する。falseを返しても最良解を更新する場合がある。
// is_answer=true は解候補フラグ。expand_answers=falseなら保存だけを行う。
// push_lazy は探索候補または最良有効解の保存に必要な場合だけ maker() を1回呼ぶ。makerは保存されない。
template<class State, class Action = void> class Emitter;
    template<class State> class Emitter<State, void> {
public:
    template<class U>
    requires StateArg<U, State>
    bool push(U&& state, Cost cost) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, false, false);
    }

    template<class U, class B>
    requires StateArg<U, State> && BoolArg<B>
    bool push(U&& state, Cost cost, B is_answer) {
        return push_state_impl(std::forward<U>(state), cost, now_hash_, false, is_answer);
    }

    template<class U, class H>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash>
    bool push(U&& state, Cost cost, H hash) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), true, false);
    }

    template<class U, class H, class B>
    requires StateArg<U, State> && CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push(U&& state, Cost cost, H hash, B is_answer) {
        return push_state_impl(std::forward<U>(state), cost, static_cast<Hash>(hash), true, is_answer);
    }

    template<class Maker>
    bool push_lazy(Cost cost, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, false, false, std::forward<Maker>(maker));
    }

    template<class B, class Maker>
    requires BoolArg<B>
    bool push_lazy(Cost cost, B is_answer, Maker&& maker) {
        return push_lazy_impl(cost, now_hash_, false, is_answer, std::forward<Maker>(maker));
    }

    template<class H, class Maker>
    requires CompatibleHashArg<H, Hash>
    bool push_lazy(Cost cost, H hash, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), true, false, std::forward<Maker>(maker));
    }

    template<class H, class B, class Maker>
    requires CompatibleHashArg<H, Hash> && BoolArg<B>
    bool push_lazy(Cost cost, H hash, B is_answer, Maker&& maker) {
        return push_lazy_impl(cost, static_cast<Hash>(hash), true, is_answer, std::forward<Maker>(maker));
    }

private:
    friend class Engine<State>;

    // EmitterはEngineへの参照を直接保持し、push_lazyのMakerをテンプレートのまま即時転送する。
    // これにより、関数ポインタや型消去を挟まず、必要になった候補だけStateを生成できる。
    Emitter(Engine<State>& engine, Hash now_hash, int child_turn) noexcept
        : engine_(engine), now_hash_(now_hash), child_turn_(child_turn) {}

    template<class U>
    bool push_state_impl(U&& state, Cost cost, Hash hash, bool has_hash, bool is_answer) {
        auto maker = [&]() -> decltype(auto) { return std::forward<U>(state); };
        return push_lazy_impl(cost, hash, has_hash, is_answer, maker);
    }

    template<class Maker>
    bool push_lazy_impl(Cost cost, Hash hash, bool has_hash, bool is_answer, Maker&& maker) {
        return engine_.push_candidate(cost, hash, child_turn_, has_hash, is_answer, std::forward<Maker>(maker));
    }

    Engine<State>& engine_;
    Hash now_hash_{};
    int child_turn_ = 0;
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
        friend struct Beam;
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
// 古いentryが増えたら空きが十分あるうちに rebuild する。
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
        mask_ = capacity_ - 1; occupied_ = 0;
        keys_.assign(static_cast<std::size_t>(capacity_), Hash{});
        values_.assign(static_cast<std::size_t>(capacity_), -1);
        used_.assign(static_cast<std::size_t>(capacity_), 0);
        epoch_ = 1;
    }

    void clear() {
        occupied_ = 0;
        ++epoch_;
        if (epoch_ == 0) {
            std::fill(used_.begin(), used_.end(), 0);
            epoch_ = 1;
        }
    }

    Lookup lookup(Hash key) const {
        int i = bucket(key);
        for (int probe = 0; probe < capacity_; ++probe) {
            if (used_[i] != epoch_) return Lookup{false, occupied_ >= capacity_ - capacity_ / 4 ? -1 : i};
            if (keys_[i] == key) return Lookup{true, i};
            i = (i + 1) & mask_;
        }
        return Lookup{false, -1};
    }

    int get(int slot) const { return values_[slot]; }

    void set(int slot, Hash key, int value) {
        if (slot < 0) return;
        occupied_ += used_[slot] != epoch_;
        used_[slot] = epoch_;
        keys_[slot] = key;
        values_[slot] = value;
    }

private:
    int capacity_ = 0, occupied_ = 0;
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
    // 空の組み込みHookでは、通知用の集計・時計取得も行わない。
    template<class Hook>
    static constexpr bool has_debug_hook =
        !std::same_as<std::remove_cvref_t<Hook>, NoOp>
#ifndef LOCAL
        && !std::same_as<std::remove_cvref_t<Hook>, CsvStatHook>
#endif
        ;
    static_assert(std::copy_constructible<State>, "State must be copy constructible because best_state is copied when updated");
    static_assert(std::move_constructible<State>, "State must be move constructible");
    static_assert(std::assignable_from<State&, State>, "State must be assignable because selectors replace candidates in-place");

    friend class Emitter<State>;

    struct Item {
        // 選抜時に頻繁に読むメタ情報を先頭に置き、Stateが大きい場合のキャッシュ効率を少し良くする。
        Cost cost{};
        Hash hash{};
        bool has_hash = false;
        State state;

        template<class U>
        Item(U&& state_, Cost cost_, Hash hash_, bool has_hash_)
            : cost(cost_),
              hash(hash_),
              has_hash(has_hash_),
              state(std::forward<U>(state_)) {}
    };

    // 次frontier候補用のSelector。push時点で beam_width 個だけを保持する。
    // 幅制限の比較はcost-only。同costでは既存候補を優先し、hashではtie-breakしない。
    // hashは同一frontier内の重複排除とStateView用にだけ使う。
    // State生成前に cost/hash で受理可能性を判定するため、push_lazy と通常pushの両方で内部保存の無駄を抑えられる。
    class Selector {
        int width_ = 1, base_ = 1, worst_ = -1;
        bool hashed_ = false;
        Cost worst_cost_{};
        std::vector<Item> candidates_;
        std::vector<Cost> costs_;
        std::vector<int> tree_;
        FixedHashMap map_;

        int worse(int a, int b) const {
            if (a < 0) return b;
            if (b < 0) return a;
            if (costs_[a] != costs_[b]) return costs_[a] < costs_[b] ? b : a;
            return std::max(a, b);
        }
        // 再構築ループを候補選別へインライン展開せず、通常の照合処理を小さく保つ。
        [[gnu::noinline]] void rebuild_hash() {
            map_.clear();
            for (int i = 0; i < size(); ++i) if (candidates_[i].has_hash) {
                const auto h = candidates_[i].hash;
                map_.set(map_.lookup(h).slot, h, i);
            }
        }
    public:
        struct Choice {
            int slot = -1, hash_slot = -1;
            bool duplicate = false, rebuilt = false;
        };
        void init(int width, int hash_capacity, bool hashed) {
            width_ = width; hashed_ = hashed;
            base_ = detail::ceil_pow2(width);
            candidates_.reserve(width); costs_.resize(width); tree_.assign(base_ * 2, -1);
            if (hashed_) map_.init(hash_capacity);
        }
        void clear() {
            candidates_.clear(); worst_ = -1;
            if (hashed_) map_.clear();
        }
        int size() const { return static_cast<int>(candidates_.size()); }
        bool full() const { return size() == width_; }
        std::vector<Item>& selected() { return candidates_; }
        const std::vector<Item>& selected() const { return candidates_; }
        bool rejects_by_width(Cost cost) const { return full() && !(cost < worst_cost_); }

        // 幅判定の後、Stateを生成せず保存位置を決める。Sub版と同じchoose/putの二段階。
        Choice choose(Cost cost, Hash hash, bool has_hash) {
            Choice c;
            if (hashed_ && has_hash) {
                auto hit = map_.lookup(hash);
                if (hit.found) {
                    const int i = map_.get(hit.slot);
                    if (i < size() && candidates_[i].has_hash && candidates_[i].hash == hash) {
                        c.duplicate = true;
                        if (cost < costs_[i]) c.slot = i;
                        return c;
                    }
                }
                if (hit.slot < 0) { rebuild_hash(); hit = map_.lookup(hash); c.rebuilt = true; }
                if (hit.slot < 0) return c;
                c.hash_slot = hit.slot;
            }
            c.slot = full() ? worst_ : size();
            return c;
        }
        template<class Maker>
        void put(Choice c, Cost cost, Hash hash, bool has_hash, Maker&& maker) {
            const bool appended = c.slot == size();
            if (appended) {
                candidates_.emplace_back(std::invoke(std::forward<Maker>(maker)), cost, hash, has_hash);
            } else {
                auto& item = candidates_[c.slot];
                item.cost = cost; item.hash = hash; item.has_hash = has_hash;
                item.state = std::invoke(std::forward<Maker>(maker));
            }
            costs_[c.slot] = cost;
            if (c.hash_slot >= 0) map_.set(c.hash_slot, hash, c.slot);
            if (full()) {
                if (appended) {
                    for (int i = 0; i < width_; ++i) tree_[base_ + i] = i;
                    for (int i = base_ - 1; i > 0; --i) tree_[i] = worse(tree_[i * 2], tree_[i * 2 + 1]);
                } else {
                    for (int i = (base_ + c.slot) / 2; i > 0; i /= 2)
                        tree_[i] = worse(tree_[i * 2], tree_[i * 2 + 1]);
                }
                worst_ = tree_[1]; worst_cost_ = costs_[worst_];
            }
        }
    };

public:
    explicit Engine(Param param) : rt{normalize(param)} {}

    template<class Expand, class DebugHook>
    Result<State> run(State&& initial_state, Cost initial_cost, Hash initial_hash, bool initial_has_hash,
                      Expand&& expand, DebugHook&& debug_hook) {

        start_clock_us = detail::now_microseconds();
        limit_us = detail::time_limit_to_microseconds(rt.param.time_limit_ms);
        use_time_limit = (limit_us > 0);
        debug_enabled = has_debug_hook<DebugHook>;

        frontier.reserve(rt.param.beam_width);
        selector.init(rt.param.beam_width, rt.param.hash_capacity, rt.param.use_hash_dedup);
        frontier.emplace_back(std::move(initial_state), initial_cost, initial_hash, initial_has_hash);
        refresh_runtime_usage();

        const bool valid_initial = detail::is_valid_cost(initial_cost);
        rt.best_cost = initial_cost;

        update_time();
        if (use_time_limit && rt.elapsed_us >= limit_us) {
            rt.time_limit_reached = true;
        }
        emit_hook(EventType::RunStart, debug_hook);
        if (valid_initial && rt.param.max_turn == 0 && rt.param.max_turn_is_answer) {
            best_state.emplace(std::move(frontier.front().state));
            set_best(initial_cost, 0);
            record_best_update();
        }
        rt.completed_max_turn = valid_initial && rt.param.max_turn == 0;
        if (!valid_initial) frontier.clear();
        if (valid_initial && !rt.completed_max_turn) {
            if (use_time_limit) run_loop<true>(expand, debug_hook);
            else run_loop<false>(expand, debug_hook);
        }

        turn_timing_active = false;
        finalize_runtime_for_result();
        emit_hook(EventType::RunEnd, debug_hook);
        return build_result();
    }

private:
    Runtime rt;

    std::int64_t start_clock_us = 0;
    std::int64_t limit_us = 0;
    bool use_time_limit = false;
    bool turn_timing_active = false;
    bool debug_enabled = false;

    std::vector<Item> frontier;
    Selector selector;
    std::optional<State> best_state;

    long long expanded_at_last_best_update = 0;
    std::int64_t last_best_update_us = 0;
    int last_best_update_turn = 0;

    // ユーザー指定値を内部で扱いやすい範囲へ正規化する。
    // hash_capacity はSelector保持数が高々beam_width個であることを前提に丸める。
    static Param normalize(Param param) {
        param.max_turn = std::max(0, param.max_turn);
        param.beam_width = std::max(1, param.beam_width);
        param.time_check_interval = std::max(1, param.time_check_interval);
        if (param.max_turn == std::numeric_limits<int>::max() ||
            param.beam_width > ((1 << 30) - 17) / 4)
            throw std::length_error("beam parameters exceed internal index limits");

        if (param.use_hash_dedup) {
            // Selectorが保持する候補は高々beam_width個なので、過大なhash表はキャッシュ効率のため丸める。
            if (param.hash_capacity <= 0) param.hash_capacity = param.beam_width * 4 + 17;
            const int lower = param.beam_width * 2 + 1;
            const int upper = param.beam_width * 4 + 17;
            param.hash_capacity = std::min(std::max(param.hash_capacity, lower), upper);
        } else {
            param.hash_capacity = 0;
        }
        return param;
    }

    void update_time() {
        rt.elapsed_us = detail::now_microseconds() - start_clock_us;
        if (turn_timing_active) {
            rt.turn_elapsed_us = rt.elapsed_us - rt.turn_start_us;
            rt.turn_time_limit_reached |= rt.elapsed_us >= rt.turn_deadline_us;
        }
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
            rt.turns_since_best_update = std::max(0, rt.turn - last_best_update_turn);
            rt.expanded_nodes_since_best_update = rt.expanded_nodes - expanded_at_last_best_update;
            rt.ms_since_best_update = static_cast<double>(rt.elapsed_us - last_best_update_us) * 0.001;
        }
    }

    void finalize_runtime_for_result() {
        // RunEnd hook と Result::runtime に同じ最終snapshotを渡す。
        refresh_runtime_usage();
        update_time();
        update_debug_snapshot();
    }

    void collect_selected_debug_stats(std::span<const Item> cands) {
        auto& costs = rt.selected_costs;
        costs = {};
        for (const Item& item : cands) {
            costs.add(item.cost);
        }

    }

    void update_debug_snapshot() {
        if (selector.size() > 0) rt.candidate_count = selector.size();
        if (!debug_enabled) collect_selected_debug_stats(frontier);
        auto& live = rt.live_costs;
        live = {};
        for (const Item& item : frontier) live.add(item.cost);
        for (const Item& item : selector.selected()) live.add(item.cost);

        if (rt.selected_width > 0 && rt.width_pruned_cost_count > 0) {
            rt.pruned_best_gap = static_cast<double>(rt.width_pruned_best_cost) - static_cast<double>(rt.selected_costs.worst);
        } else {
            rt.pruned_best_gap = 0.0;
        }

        update_best_stagnation_stats();
    }

    template<class DebugHook>
    void emit_hook(EventType event_type, DebugHook& debug_hook) {
        if constexpr (has_debug_hook<DebugHook>) {
            // RunEndは直前に最終スナップショットを整えている。
            if (event_type != EventType::RunEnd) update_debug_snapshot();
            debug_hook(event_type, static_cast<const Runtime&>(rt));
        }
    }

    // 呼出し直前に全体の時計を更新する。turn中は締切を延長しない。
    void begin_turn_time() {
        if (!rt.param.use_turn_time_limit || !use_time_limit) return;
        turn_timing_active = true;
        rt.turn_start_us = rt.elapsed_us;
        rt.turn_elapsed_us = 0;
        const auto remaining = std::max<std::int64_t>(0, limit_us - rt.elapsed_us);
        rt.turn_deadline_us = rt.elapsed_us + remaining / (rt.param.max_turn - rt.turn);
        rt.turn_time_limit_reached = rt.elapsed_us >= rt.turn_deadline_us;
    }

    bool stop_on_timeout() const {
        return rt.time_limit_reached && (!rt.param.continue_on_timeout || rt.found);
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
        if ((rt.expanded_nodes % rt.param.time_check_interval) != 0) return false;
        return mark_time_limit_if_elapsed();
    }

    template<class Maker>
    bool push_candidate(Cost cost, Hash hash, int turn, bool has_hash, bool is_answer, Maker&& maker) {
        static_assert(std::invocable<Maker&&>, "maker must be callable with no arguments");
        static_assert(std::constructible_from<State, std::invoke_result_t<Maker&&>>, "maker result must be constructible as State");
        ++rt.generated_candidates;
        // NaN cost や最大ターン超過は、幅やhashではなく不正候補として扱う。
        if (!detail::is_valid_cost(cost) || turn > rt.param.max_turn) {
            ++rt.invalid_candidates;
            return false;
        }
        if (turn == rt.param.max_turn) { rt.completed_max_turn = true; ++rt.terminal_candidates; }
        if (is_answer || (rt.param.max_turn_is_answer && turn == rt.param.max_turn)) {
            const bool answer_only = !rt.param.expand_answers && turn < rt.param.max_turn;
            if (answer_only) ++rt.answer_only_candidates;
            if (!rt.found || cost < rt.best_cost) {
                best_state.emplace(std::invoke(std::forward<Maker>(maker)));
                set_best(cost, turn);
                if (turn == rt.param.max_turn || answer_only) return false;
                return select_candidate(cost, hash, has_hash, [&]() -> State { return *best_state; });
            }
            if (answer_only) return false;
        }
        if (turn == rt.param.max_turn) return false;
        return select_candidate(cost, hash, has_hash, std::forward<Maker>(maker));
    }

    template<class Maker>
    bool select_candidate(Cost cost, Hash hash, bool has_hash, Maker&& maker) {
        if (selector.rejects_by_width(cost)) {
            ++rt.pruned_by_width;
            update_width_pruned_debug(cost);
            return false;
        }
        const auto choice = selector.choose(cost, hash, has_hash);
        if (choice.duplicate) ++rt.duplicate_by_hash;
        if (choice.rebuilt) ++rt.hash_rebuild_count;
        if (choice.slot < 0) {
            ++rt.pruned_by_hash;
            return false;
        }
        const bool appended = choice.slot == selector.size();
        selector.put(choice, cost, hash, has_hash, std::forward<Maker>(maker));
        ++rt.accepted_candidates;
        if (choice.duplicate) ++rt.replaced_by_hash;
        else if (!appended) ++rt.replaced_by_width;
        if (appended) {
            refresh_runtime_usage();
        }
        return true;
    }

    template<class Expand>
    void expand_item(Item& item, Expand& expand) {
        ++rt.active_width;
        rt.max_active_width = std::max(rt.max_active_width, rt.active_width);
        StateView<State> now(rt.turn, item.cost, item.state, item.hash, item.has_hash);
        Emitter<State> emit(*this, item.hash, rt.turn + 1);
        if constexpr (std::is_invocable_v<Expand&, const StateView<State>&, const Runtime&, Emitter<State>&>) {
            // Runtime付きexpandには保持量だけを最新化して渡す。
            // elapsed_usはtime limit判定・Hook・RunEndで更新し、expandごとの時刻取得を避ける。
            refresh_runtime_usage();
        }
        const int before = rt.best_update_count;
        call_expand(expand, now, rt, emit);
        ++rt.expanded_nodes;
        if (rt.best_update_count != before) record_best_update();
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

    // 時間制限の有無だけをコンパイル時に分け、探索手順は一つに保つ。
    template<bool CheckTime, class Expand, class DebugHook>
    void run_loop(Expand& expand, DebugHook& debug_hook) {
        bool minimal_mode = false;
        for (int turn = 0; turn < rt.param.max_turn && !frontier.empty(); ++turn) {
            rt.turn = turn;
            begin_turn();
            if constexpr (CheckTime) {
                minimal_mode |= rt.time_limit_reached;
                if (stop_on_timeout()) break;
            }
            expand_frontier<CheckTime>(minimal_mode, expand);
            if constexpr (CheckTime) if (stop_on_timeout()) break;
            frontier.clear();
            refresh_runtime_usage();
            materialize_arrival(minimal_mode);
            rt.exhausted = frontier.empty() && !rt.completed_max_turn;
            end_turn(debug_hook);
            if (rt.completed_max_turn) break;
        }
        rt.exhausted = frontier.empty() && !rt.completed_max_turn;
    }

    void begin_turn() {
        rt.active_width = 0;
        rt.selected_width = 0;
        rt.candidate_count = 0;
        if (debug_enabled) {
            rt.selected_costs = {};
            rt.width_pruned_cost_count = 0;
            rt.width_pruned_best_cost = Cost{};
            rt.pruned_best_gap = 0.0;
        }
        if (use_time_limit) mark_time_limit_if_elapsed();
        begin_turn_time();
    }

    template<class DebugHook>
    void end_turn(DebugHook& debug_hook) {
        refresh_runtime_usage();
        if (use_time_limit) mark_time_limit_if_elapsed();
        else if constexpr (has_debug_hook<DebugHook>) update_time();
        turn_timing_active = false;
        emit_hook(EventType::TurnEnd, debug_hook);
    }

    template<bool CheckTime, class Expand>
    void expand_frontier(bool& minimal_mode, Expand& expand) {
        int i = 0;
        if (!minimal_mode) {
            for (; i < static_cast<int>(frontier.size()); ++i) {
                if constexpr (CheckTime) {
                    if (rt.turn_time_limit_reached && (selector.size() != 0 || rt.completed_max_turn)) break;
                }
                expand_item(frontier[i], expand);
                if constexpr (CheckTime) {
                    if (need_switch_to_minimal_by_time()) { minimal_mode = true; ++i; break; }
                }
            }
        }
        if constexpr (CheckTime) {
            // 全体時間切れ後は、未展開部分から最良1本だけを追加展開する。
            if (minimal_mode && !stop_on_timeout()) {
                const int best = best_item_index(frontier, i);
                if (best >= 0) expand_item(frontier[best], expand);
            }
        }
    }

    void set_best(Cost cost, int turn) {
        rt.found = true;
        rt.best_cost = cost;
        rt.best_turn = turn;
        last_best_update_turn = rt.turn;
        ++rt.best_update_count;
    }

    void record_best_update() {
        expanded_at_last_best_update = rt.expanded_nodes;
        update_time();
        last_best_update_us = rt.elapsed_us;
    }

    void materialize_arrival(bool minimal) {
        rt.materialized_turn = rt.turn + 1; // 候補0件も含む、到着候補の確定処理済みturn。
        // Selector内の候補を次frontierへ移す。
        // minimal=true ではSelector内の最良cost 1件だけを移し、他は破棄する。
        // 選別に残った候補を展開対象にする。expand_answers=falseの有効解は登録しない。
        auto& selected = selector.selected();
        const int original_count = static_cast<int>(selected.size());
        const int best_idx = minimal ? best_item_index(selected) : -1;

        rt.candidate_count = original_count;
        rt.selected_width = minimal ? (best_idx >= 0 ? 1 : 0) : original_count;
        rt.max_selected_width = std::max(rt.max_selected_width, rt.selected_width);
        if (rt.selected_width >= rt.param.beam_width) ++rt.beam_saturated_turns;

        if (minimal) {
            if (best_idx >= 0) frontier.push_back(std::move(selected[best_idx]));
        } else {
            frontier.swap(selected);
        }

        if (debug_enabled) collect_selected_debug_stats(frontier);
        rt.selected_nodes += rt.selected_width;
        selector.clear();
        refresh_runtime_usage();
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
        Engine<Stored> engine(param);
        auto raw = engine.run(Stored{std::move(initial_state), {}}, initial_cost, hash, has_hash, bridge, hook);
        Result<State, Action> result;
        result.found = raw.found;
        result.time_limit_reached = raw.time_limit_reached;
        result.exhausted = raw.exhausted;
        result.completed_max_turn = raw.completed_max_turn;
        result.best_cost = raw.best_cost;
        result.best_turn = raw.best_turn;
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
    // Hashあり初期状態版。
    // expand は (const StateView<State>&, Emitter<State>&)、または
    // (const StateView<State>&, const Runtime&, Emitter<State>&) を受け取れる。
    // Runtime は const 参照固定であり、expand から Runtime を変更することはできない。
    // 操作列が必要な場合はrun<State, Action>でAction型を指定する。
    template<class State, class Action = void, class H, class Expand, class DebugHook = NoOp>
    requires CompatibleHashArg<H, Hash>
    static Result<State, Action> run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        H initial_hash,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) {
        if constexpr (std::is_void_v<Action>) {
            Engine<State> engine(param);
            return engine.run(std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true,
                              std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));

        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, static_cast<Hash>(initial_hash), true, expand, debug_hook);
        }
    }

    // Hashなし初期状態版。
    // Hash付きpushを行うまでは has_hash=false として扱われ、重複排除されない。
    // 第4引数が整数ならHashあり版だけにマッチさせ、誤ったoverload選択を避ける。
    template<class State, class Action = void, class Expand, class DebugHook = NoOp>
    requires (!CompatibleHashArg<Expand, Hash>)
    static Result<State, Action> run(
        const Param& param,
        State initial_state,
        Cost initial_cost,
        Expand&& expand,
        DebugHook&& debug_hook = DebugHook{}
    ) {
        if constexpr (std::is_void_v<Action>) {
            Engine<State> engine(param);
            return engine.run(std::move(initial_state), initial_cost, Hash{}, false,
                              std::forward<Expand>(expand), std::forward<DebugHook>(debug_hook));

        } else {
            return run_history<State, Action>(param, std::move(initial_state), initial_cost, Hash{}, false, expand, debug_hook);
        }
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
static_assert(std::same_as<decltype(BS::Runtime{}.param.time_limit_ms), double>);

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
    bool is_answer = false;
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
        BS_TEST_ASSERT(rt.param.max_turn == 1);
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
                bool is_answer = (nx.depth >= max_turn);
                // 解は生成時に比較し、探索用の選別では失わない。
                if (is_answer && (!found || cost < best_cost)) {
                    found = true;
                    best_cost = cost;
                    best = nx;
                }
                ref_push(bucket, beam_width, RefItem{nx, cost, h, is_answer}, use_hash);
            }
        }
        frontier.clear();
        for (RefItem& item : bucket) {
            if (!item.is_answer) frontier.push_back(item);
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
        BS_TEST_ASSERT(res.runtime.duplicate_by_hash == 0);
        BS_TEST_ASSERT(res.runtime.replaced_by_hash == 0);
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
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 0);
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
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 0);
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
        BS_TEST_ASSERT(res.runtime.selected_nodes == 0);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.beam_width = 10;
        int expanded_is_answer = 0;
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
                if (now.state.x == 1) ++expanded_is_answer;
                else BS_TEST_ASSERT(now.state.x == 2);
            }
        };
        auto res = BS::run(param, SmallState{}, 0, expand);
        BS_TEST_ASSERT(res.found);
        BS_TEST_ASSERT(res.best_cost == 1);
        BS_TEST_ASSERT(res.best_state->x == 1);
        BS_TEST_ASSERT(expanded_is_answer == 1);
        BS_TEST_ASSERT(res.runtime.expanded_nodes == 3);
    }
    {
        BS::Param param;
        param.max_turn = 2;
        param.max_turn_is_answer = false;
        param.beam_width = 4;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) {
                SmallState is_answer = now.state;
                is_answer.depth = 1;
                is_answer.x = 10;
                emit.push(std::move(is_answer), 1, true);
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
        BS_TEST_ASSERT(res.runtime.duplicate_by_hash == 0);
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 0);
        BS_TEST_ASSERT(res.runtime.replaced_by_hash == 0);
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
        BS_TEST_ASSERT(res.runtime.accepted_candidates == 0);
        BS_TEST_ASSERT(res.runtime.replaced_by_width == 0);
        BS_TEST_ASSERT(res.runtime.selected_nodes == 0);
        BS_TEST_ASSERT(res.runtime.max_selected_width == 0);
        BS_TEST_ASSERT(res.runtime.beam_saturated_turns == 0);
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
        BS_TEST_ASSERT(res.runtime.pruned_by_width == 0);
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
        BS_TEST_ASSERT(res.runtime.selected_nodes == 0);
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
        BS_TEST_ASSERT(res.runtime.param.hash_capacity == 5 * 4 + 17);
    }
#ifdef LOCAL
    {
        const char* filename = "beam_copy_single_v40_stat.csv";
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
        bool saw_terminal_turn = false;
        for (const auto& row : hook.rows) {
            BS_TEST_ASSERT(std::string(row.event).find("Frontier") == std::string::npos);
            if (std::string(row.event) == "TurnEnd") {
                saw_turn_end = true;
                if (row.candidate_count == 0 && row.terminal_candidates == 2 && row.completed_max_turn) {
                    saw_terminal_turn = true;
                }
            }
        }
        BS_TEST_ASSERT(saw_turn_end);
        BS_TEST_ASSERT(saw_terminal_turn);
    }
#endif
    {
        constexpr int target_depth = 3;
        BS::Param param;
        param.max_turn = target_depth;
        param.beam_width = 4;
        param.time_limit_ms = 1;
        param.time_check_interval = 1;
        auto expand = [](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.state.depth == 0) bs::detail::TestClock::value += 3000;
            if (now.state.depth >= target_depth) return;
            for (int k = 0; k < 4; ++k) {
                SmallState s = now.state;
                ++s.depth;
                s.x = now.state.x * 10 + k;
                const bool is_answer = (s.depth >= target_depth);
                emit.push(std::move(s), now.cost + k, static_cast<std::uint64_t>(100 + s.depth * 10 + k), is_answer);
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
            bs::detail::TestClock::value += 3000;
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
        BS_TEST_ASSERT(res.runtime.param.max_turn == 0);
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
        BS_TEST_ASSERT(res.runtime.param.max_turn == 0);
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
        int expanded_is_answer_depth1 = 0;
        auto expand = [&](const BS::StateView<SmallState>& now, auto& emit) {
            if (now.turn == 0) {
                SmallState s = now.state;
                s.depth = 1;
                s.x = 1;
                emit.push(std::move(s), 10, std::uint64_t{1}, true);
            } else if (now.turn == 1) {
                ++expanded_is_answer_depth1;
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
        BS_TEST_ASSERT(expanded_is_answer_depth1 == 1);
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
                bs::detail::TestClock::value += 8000;
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
                bool is_answer = (nx.depth >= max_turn);
                if (use_hash) emit.push(nx, cost, static_cast<std::uint64_t>(nx.v), is_answer);
                else emit.push(nx, cost, is_answer);
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
    std::array<std::uint32_t, 16> data{};
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
                nx.data[k & 15] += static_cast<std::uint32_t>(nx.v) + nx.depth;
                int cost = now.cost + static_cast<int>(nx.h % 97);
                bool is_answer = (nx.depth >= max_turn);
                emit.push(std::move(nx), cost, nx.h, is_answer);
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
                bool is_answer = (next_depth >= max_turn);
                emit.push_lazy(cost, h, is_answer, [&]() {
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

static void test_turn_end_expansion_counts() {
    using B = bs::Beam<int>;
    for (double limit : {0.0, 100000.0}) {
        B::Param p;
        p.max_turn = 3; p.beam_width = 3; p.use_hash_dedup = false;
        p.time_limit_ms = limit;
        int actual = 0, notifications = 0;
        auto expand = [&](const B::StateView<int>& now, B::Emitter<int>& emit) {
            if (now.turn == 0) {
                for (int i = 0; i < 3; ++i) emit.push(i, i);
            } else if (now.turn == 1) {
                ++actual;
                emit.push(actual, -actual, true);
            }
        };
        auto hook = [&](B::EventType event, const B::Runtime& rt) {
            if (event == B::EventType::TurnEnd && rt.turn == 1) {
                ++notifications;
                BS_TEST_ASSERT(rt.active_width == actual);
                BS_TEST_ASSERT(rt.max_active_width == actual);
            }
        };
        const auto result = B::run<int>(p, 0, 0, expand, hook);
        BS_TEST_ASSERT(notifications == 1 && result.best_cost == -3 && result.runtime.best_update_count == 3);
    }
}

// 共通方針の境界テスト。実時間のsleepに依存せず、仮想時計で再現する。
namespace policy_test {
using Lib = bs::Beam<>;
using Clock = bs::detail::TestClock;
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
    using B = bs::Beam<>;
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
    test_turn_end_expansion_counts();
    bs::detail::TestClock::enabled = true;
    test_edge_cases();
    test_max_turn_cases();
    test_random_cases();
    test_random_max_turn_cases();
    bs::detail::TestClock::enabled = false;
    benchmark_trivial_state();
    benchmark_vector_state_lazy();
    std::cout << "all tests passed" << std::endl;
    return 0;
}

#endif // __INCLUDE_LEVEL__ == 0
