#pragma once
#include <bits/stdc++.h>

// 共有物の費用・容量、採用数、配置先別個数を伴う割当を最小化する単一ヘッダ
// N: 要素数、K: グループ数、R: 資源数、E: 辺数、P: 延べ所属数
// L: 入力全体の大きさ、D: 個数下限の診断で調べる辺・配置先の組数
// 負荷は非負整数。合計は int64_t 内、費用と中間計算は有限値に収まること
// 通常グループは [0,K)、未採用は -1。実行可能解の発見・最適性は保証しない
class HypergraphPartitioner {
public:
    static constexpr int64_t unlimited = std::numeric_limits<int64_t>::max();
    struct CountCost {
        double active = 0, extra = 0, cut_value = 0;
        std::vector<double> table;
        // 使用の有無に対する費用を作る、O(1)
        static CountCost activation(double w) { CountCost c; c.active = w; return c; }
        // 2番目以降の使用先ごとの費用を作る、O(1)
        static CountCost connectivity(double w) { CountCost c; c.extra = w; return c; }
        // 個数2以上に対する費用を作る、O(1)
        static CountCost cut(double w) { CountCost c; c.cut_value = w; return c; }
        // 個数に比例する費用を作る、O(1)
        static CountCost linear(double w) { CountCost c; c.active = c.extra = w; return c; }
        // 0番目が0の費用表を所有する、O(表長)
        static CountCost lookup(std::vector<double> values) {
            CountCost c; c.table = std::move(values); return c;
        }
        // 個数に対する費用を返す、O(1)
        double operator()(int count) const {
            return (count == 0 ? 0 : count == 1 ? active : active + extra * (count-1) + cut_value)
                + (table.empty() ? 0 : table[count]);
        }
    };
    struct LoadPenalty {
        int64_t lower = 0, upper = unlimited;
        double under_linear = 0, under_quadratic = 0;
        double over_linear = 0, over_quadratic = 0;
        // 目標区間からの不足・超過費用を返す、O(1)
        double operator()(int64_t load) const {
            const double a = static_cast<double>(std::max(int64_t(0), lower - load));
            const double b = static_cast<double>(std::max(int64_t(0), load - upper));
            return a * (under_linear + under_quadratic * a)
                + b * (over_linear + over_quadratic * b);
        }
    };
    struct Vertex {
        std::vector<int64_t> load;
        std::vector<std::pair<int, std::vector<int64_t>>> load_by_group;
        double default_cost = 0;
        std::vector<std::pair<int, double>> cost_by_group;
        std::optional<std::vector<int>> allowed_groups;
        std::optional<int> fixed;
        bool allow_reject = false;
        double reject_cost = 0;
    };
    struct Group {
        std::vector<int64_t> lower, upper;
        std::vector<LoadPenalty> load_penalty;
        double open_cost = 0;
        bool lower_only_if_used = false;
    };
    struct Hyperedge {
        std::vector<int> vertices;
        CountCost span_cost, local_cost, selected_cost;
        std::vector<std::pair<int, CountCost>> local_cost_by_group;
        std::vector<int64_t> shared_load;
        std::vector<std::pair<int, std::vector<int64_t>>> shared_load_by_group;
        int min_span = 0, max_span = INT_MAX;
        int min_selected = 0, max_selected = INT_MAX;
        int local_lower = 0, local_upper = INT_MAX;
        std::vector<std::pair<int, int>> local_lower_by_group, local_upper_by_group;
    };
    struct Problem {
        int num_groups, num_resources;
        std::vector<Vertex> vertices;
        std::vector<Group> groups;
        std::vector<Hyperedge> hyperedges;
        double constant = 0;
        // 既定負荷を第0資源1、他0として問題を作る、O(NR+KR)
        Problem(int n = 0, int k = 1, int r = 1) : num_groups(k), num_resources(r) {
            assert(n >= 0 && k > 0 && r > 0);
            vertices.resize(n); groups.resize(k);
            for (auto& v : vertices) { v.load.assign(r, 0); v.load[0] = 1; }
            for (auto& g : groups) {
                g.lower.assign(r, 0); g.upper.assign(r, unlimited); g.load_penalty.resize(r);
            }
        }
        // 辺を追加しIDを返す、償却O(辺の所属数)
        int add_hyperedge(std::vector<int> vertices_, CountCost cost = CountCost::linear(0)) {
            Hyperedge e; e.vertices = std::move(vertices_); e.span_cost = std::move(cost);
            hyperedges.push_back(std::move(e)); return static_cast<int>(hyperedges.size()) - 1;
        }
    };
    enum class ViolationKind { Domain, LoadLower, LoadUpper, SpanLower, SpanUpper,
                               SelectedLower, SelectedUpper, LocalLower, LocalUpper };
    struct Violation {
        ViolationKind kind;
        int id = -1, group = -1, resource = -1;
        int64_t amount = 0;
    };
    struct Evaluation {
        double cost = 0;
        bool feasible = false;
        std::vector<Violation> violation;
    };
    struct Result : Evaluation {
        std::vector<int> assignment;
        bool scope_conflict = false;
        int64_t iterations = 0;
        double elapsed_us = 0;
    };
    struct Options {
        int64_t time_limit_us = 100000;
        bool annealing = true;
    };

    // デバッグ表示。改行を付けず、配列は先頭20件と省略件数を出す。
    friend std::ostream& operator<<(std::ostream& os, const Violation& v) {
        static constexpr const char* names[] = {"Domain", "LoadLower", "LoadUpper", "SpanLower", "SpanUpper",
            "SelectedLower", "SelectedUpper", "LocalLower", "LocalUpper"};
        const auto kind = static_cast<unsigned>(v.kind);
        return os << (kind < std::size(names) ? names[kind] : "Unknown")
                  << "{id=" << v.id << ", group=" << v.group << ", resource=" << v.resource
                  << ", amount=" << v.amount << '}';
    }
    friend std::ostream& operator<<(std::ostream& os, const Evaluation& e) {
        return print_evaluation(os << "Evaluation{", e) << '}';
    }
    friend std::ostream& operator<<(std::ostream& os, const Result& r) {
        print_evaluation(os << "Result{", r) << ", assignment=";
        print_list(os, r.assignment);
        return os << ", scope_conflict=" << (r.scope_conflict ? "true" : "false")
                  << ", iterations=" << r.iterations << ", elapsed_us=" << r.elapsed_us << '}';
    }

    // 入力のコピーを所有し、形式を検査・正規化する、O(入力サイズと各リストの整列)
    explicit HypergraphPartitioner(Problem problem, uint64_t seed = 0)
        : p_(std::move(problem)), rng_(seed) { normalize(); }
    // 新しい割当と全要素の探索範囲で開始する、O(初期化+探索)
    const Result& solve(const Options& options) {
        const auto start = Clock::now();
        scope_.reset(); ready_ = false; conflict_ = false;
        synchronize(); construct(options.time_limit_us); return search(options, start);
    }
    // 初期割当から改善する。範囲外は初期割当を保つ、O((L+PR) log(L+2)+探索)
    const Result& improve(std::span<const int> initial, const Options& options,
                         std::optional<std::vector<int>> movable = std::nullopt) {
        const auto start = Clock::now();
        std::vector<int> copy(initial.begin(), initial.end()); // 内部結果の参照も受け付ける
        assert(copy.size() == p_.vertices.size());
        for (int v = 0; v < n(); ++v) assert(permitted(v, copy[v]));
        scope_ = std::move(movable); ready_ = false; conflict_ = false;
        synchronize(); current_ = build(copy); best_ = current_.score; best_a_ = std::move(copy); dirty_result_ = true;
        ready_ = true; return search(options, start);
    }
    // 問題変更を同期して探索を継続する。初回は初期化する、O(同期+探索)
    const Result& resume(const Options& options) {
        const auto start = Clock::now();
        synchronize(); if (!ready_) construct(options.time_limit_us); return search(options, start);
    }
    // 割当の本来の費用と各違反を再計算する、O((L+PR+D) log(L+2))
    Evaluation evaluate(std::span<const int> assignment) const {
        assert(assignment.size() == p_.vertices.size());
        return diagnose(build(assignment));
    }
    // 最後の実行結果を参照する。次の変更操作まで有効、O(1)
    const Result& result() const { assert(ready_ && !dirty_); return result_; }
    // 要素の入力を置換し次回実行時に同期する、O(要素の入力サイズと整列)
    void set_vertex(int v, Vertex value) {
        assert(0 <= v && v < n()); normalize_vertex(value); p_.vertices[v] = std::move(value); dirty_ = true;
    }
    // グループ条件を置換し次回実行時に同期する、O(R)
    void set_group(int k, Group value) {
        assert(0 <= k && k < K()); check_group(value); p_.groups[k] = std::move(value); dirty_ = true;
    }
    // 辺を置換し、所属変更時だけ索引を再構築する、O(辺の入力サイズと整列)
    void set_hyperedge(int e, Hyperedge value) {
        assert(0 <= e && e < E()); normalize_edge(value);
        topology_dirty_ |= value.vertices != p_.hyperedges[e].vertices;
        p_.hyperedges[e] = std::move(value); dirty_ = true;
    }
    // サイズを含む問題全体を置換し、探索状態を捨てる、O(入力サイズと整列)
    void reset_problem(Problem problem) {
        p_ = std::move(problem); normalize(); scope_.reset(); ready_ = false;
        conflict_ = false; dirty_ = topology_dirty_ = true;
        current_ = State{}; best_a_.clear(); result_ = Result{};
    }
    // 乱数状態だけを初期化し、問題と解を保持する、O(1)
    void set_seed(uint64_t seed) { rng_ = seed; }

private:
    template<class T> static std::ostream& print_list(std::ostream& os, const std::vector<T>& values) {
        os << '[';
        const size_t shown = std::min<size_t>(20, values.size());
        for (size_t i = 0; i < shown; ++i) {
            if (i) os << ", ";
            os << values[i];
        }
        if (shown < values.size()) os << ", ... (+" << values.size() - shown << ')';
        return os << ']';
    }
    static std::ostream& print_evaluation(std::ostream& os, const Evaluation& e) {
        os << "feasible=" << (e.feasible ? "true" : "false") << ", cost=" << e.cost << ", violation=";
        return print_list(os, e.violation);
    }
    using Clock = std::chrono::steady_clock;
    using Pair = std::pair<int, int>;
    struct Score {
        double cost = 0, violation = 0;
        int64_t bad = 0;
        Score& operator+=(const Score& b) { cost += b.cost; violation += b.violation; bad += b.bad; return *this; }
        Score& operator-=(const Score& b) { cost -= b.cost; violation -= b.violation; bad -= b.bad; return *this; }
    };
    struct State {
        std::vector<int> a, size, span, selected, position;
        std::vector<int64_t> load;
        std::vector<std::vector<Pair>> count;
        std::vector<std::vector<int>> members;
        Score score;
    };
    struct Cell { int e, k, before, after; };
    struct EdgeDelta { int e, span, selected; };
    Problem p_;
    uint64_t rng_;
    State current_;
    Score best_;
    std::vector<int> best_a_, active_, offsets_, incidence_;
    std::optional<std::vector<int>> scope_;
    std::vector<unsigned char> movable_, touched_, local_active_;
    std::vector<double> units_;
    std::vector<int64_t> delta_load_;
    std::vector<int> delta_size_, groups_;
    std::vector<Pair> moves_;
    std::vector<std::tuple<int, int, int>> events_;
    std::vector<Cell> cells_;
    std::vector<EdgeDelta> edges_;
    Score proposed_;
    Result result_;
    bool ready_ = false, dirty_ = true, topology_dirty_ = true, conflict_ = false;
    double scale_ = 1;
    friend struct HypergraphPartitionerTest;

    int n() const { return static_cast<int>(p_.vertices.size()); }
    int K() const { return p_.num_groups; }
    int R() const { return p_.num_resources; }
    int E() const { return static_cast<int>(p_.hyperedges.size()); }
    uint64_t random64() {
        uint64_t z = (rng_ += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    int random_int(int end) { assert(end > 0); return static_cast<int>(random64() % static_cast<uint64_t>(end)); }
    double uniform() { return static_cast<double>(random64() >> 11) * 0x1.0p-53; }
    template<class T> static const T& pick(const std::vector<std::pair<int, T>>& list, int k, const T& fallback) {
        auto it = std::lower_bound(list.begin(), list.end(), k,
                                  [](const auto& a, int b) { return a.first < b; });
        return it != list.end() && it->first == k ? it->second : fallback;
    }
    template<class T> void sort_overrides(std::vector<std::pair<int, T>>& list) const {
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 0; i < list.size(); ++i) {
            assert(0 <= list[i].first && list[i].first < K());
            assert(i == 0 || list[i - 1].first != list[i].first);
        }
    }
    void check_load(const std::vector<int64_t>& load) const {
        assert(static_cast<int>(load.size()) == R());
        for ([[maybe_unused]] int64_t x : load) assert(x >= 0);
    }
    static void check_cost(const CountCost& c, int maximum) {
        assert(std::isfinite(c.active) && std::isfinite(c.extra) && std::isfinite(c.cut_value));
        assert(c.table.empty() || (static_cast<int>(c.table.size()) > maximum && c.table[0] == 0));
        for ([[maybe_unused]] double x : c.table) assert(std::isfinite(x));
        (void)maximum;
    }
    void normalize_vertex(Vertex& v) const {
        check_load(v.load); sort_overrides(v.load_by_group); sort_overrides(v.cost_by_group);
        for (const auto& x : v.load_by_group) check_load(x.second);
        for ([[maybe_unused]] const auto& x : v.cost_by_group) assert(std::isfinite(x.second));
        assert(std::isfinite(v.default_cost) && std::isfinite(v.reject_cost));
        // 空集合と未指定を区別し、固定を許可制約より優先させない
        if (v.allowed_groups) {
            auto& a = *v.allowed_groups; std::sort(a.begin(), a.end());
            a.erase(std::unique(a.begin(), a.end()), a.end());
            for ([[maybe_unused]] int k : a) assert(0 <= k && k < K());
            assert(!a.empty() || v.allow_reject);
        }
        if (v.fixed) assert(allowed(v, *v.fixed));
    }
    void check_group(const Group& g) const {
        check_load(g.lower); check_load(g.upper);
        assert(static_cast<int>(g.load_penalty.size()) == R() && std::isfinite(g.open_cost));
        for (int r = 0; r < R(); ++r) {
            assert(g.lower[r] <= g.upper[r]);
            [[maybe_unused]] const auto& c = g.load_penalty[r];
            assert(0 <= c.lower && c.lower <= c.upper);
            assert(std::isfinite(c.under_linear) && std::isfinite(c.under_quadratic));
            assert(std::isfinite(c.over_linear) && std::isfinite(c.over_quadratic));
        }
    }
    void normalize_edge(Hyperedge& h) const {
        std::sort(h.vertices.begin(), h.vertices.end());
        h.vertices.erase(std::unique(h.vertices.begin(), h.vertices.end()), h.vertices.end());
        for ([[maybe_unused]] int v : h.vertices) assert(0 <= v && v < n());
        const int size = static_cast<int>(h.vertices.size());
        check_cost(h.span_cost, std::min(K(), size)); check_cost(h.local_cost, size); check_cost(h.selected_cost, size);
        assert(0 <= h.min_span && h.min_span <= h.max_span);
        assert(0 <= h.min_selected && h.min_selected <= h.max_selected);
        assert(0 <= h.local_lower && h.local_lower <= h.local_upper);
        // 上書きは加算ではなく置換。共有負荷の省略は0として正規化する
        if (h.shared_load.empty()) h.shared_load.assign(R(), 0);
        check_load(h.shared_load);
        sort_overrides(h.local_cost_by_group); sort_overrides(h.shared_load_by_group);
        sort_overrides(h.local_lower_by_group); sort_overrides(h.local_upper_by_group);
        for (const auto& x : h.local_cost_by_group) check_cost(x.second, size);
        for (const auto& x : h.shared_load_by_group) check_load(x.second);
        for ([[maybe_unused]] auto [k, lo] : h.local_lower_by_group)
            assert(0 <= lo && lo <= pick(h.local_upper_by_group, k, h.local_upper));
        for ([[maybe_unused]] auto [k, hi] : h.local_upper_by_group)
            assert(pick(h.local_lower_by_group, k, h.local_lower) <= hi);
    }
    void normalize() {
        assert(K() > 0 && R() > 0 && static_cast<int>(p_.groups.size()) == K());
        assert(std::isfinite(p_.constant));
        for (auto& v : p_.vertices) normalize_vertex(v);
        for (const auto& g : p_.groups) check_group(g);
        for (auto& h : p_.hyperedges) normalize_edge(h);
        units_.assign(R(), 1);
    }
    bool allowed(const Vertex& v, int k) const {
        if (k == -1) return v.allow_reject;
        return 0 <= k && k < K() && (!v.allowed_groups ||
            std::binary_search(v.allowed_groups->begin(), v.allowed_groups->end(), k));
    }
    bool permitted(int v, int k) const {
        const auto& x = p_.vertices[v]; return allowed(x, k) && (!x.fixed || *x.fixed == k);
    }
    int choice_count(int v) const {
        const auto& x = p_.vertices[v];
        return (x.allowed_groups ? static_cast<int>(x.allowed_groups->size()) : K()) + x.allow_reject;
    }
    int choice(int v, int i) const {
        const auto& x = p_.vertices[v];
        if (x.allow_reject && i == choice_count(v) - 1) return -1;
        return x.allowed_groups ? (*x.allowed_groups)[i] : i;
    }
    double unary(int v, int k) const {
        const auto& x = p_.vertices[v];
        return k < 0 ? x.reject_cost : pick(x.cost_by_group, k, x.default_cost);
    }
    static Score bound(int64_t x, int64_t lo, int64_t hi, double unit = 1) {
        if (lo <= x && x <= hi) return {};
        return {0, (static_cast<double>(std::max(int64_t(0), lo - x)) +
                    static_cast<double>(std::max(int64_t(0), x - hi))) / unit,
                (x < lo) + (x > hi)};
    }
    Score group_score(int k, int size, const int64_t* load) const {
        const auto& g = p_.groups[k]; Score out; out.cost = size > 0 ? g.open_cost : 0;
        for (int r = 0; r < R(); ++r) {
            out.cost += g.load_penalty[r](load[r]);
            out += bound(load[r], (!g.lower_only_if_used || size > 0) ? g.lower[r] : 0, g.upper[r], units_[r]);
        }
        return out;
    }
    Score local_score(int e, int k, int c) const {
        const auto& h = p_.hyperedges[e];
        Score out = bound(c, pick(h.local_lower_by_group, k, h.local_lower),
                          pick(h.local_upper_by_group, k, h.local_upper));
        out.cost = pick(h.local_cost_by_group, k, h.local_cost)(c); return out;
    }
    Score edge_score(int e, int span, int selected) const {
        const auto& h = p_.hyperedges[e]; Score out = bound(span, h.min_span, h.max_span);
        out += bound(selected, h.min_selected, h.max_selected);
        out.cost = h.span_cost(span) + h.selected_cost(selected); return out;
    }
    static int count(const State& s, int e, int k) {
        const auto& a = s.count[e];
        if (a.size() <= 8) {
            for (auto [key, value] : a) if (key == k) return value;
            return 0;
        }
        auto it = std::lower_bound(a.begin(), a.end(), Pair{k, 0});
        return it != a.end() && it->first == k ? it->second : 0;
    }
    Evaluation diagnose(const State& s) const {
        Evaluation out; out.cost = s.score.cost + p_.constant;
        auto add = [&](ViolationKind kind, int id, int k, int r, int64_t amount) {
            if (amount > 0) out.violation.push_back({kind, id, k, r, amount});
        };
        for (int v = 0; v < n(); ++v)
            if (!permitted(v, s.a[v])) add(ViolationKind::Domain, v, s.a[v], -1, 1);
        // 容量違反は資源ごとに元の整数単位で報告する
        for (int k = 0; k < K(); ++k) for (int r = 0; r < R(); ++r) {
            const auto& g = p_.groups[k]; const int64_t x = s.load[k * R() + r];
            if (!g.lower_only_if_used || s.size[k] > 0)
                add(ViolationKind::LoadLower, k, k, r, g.lower[r] - x);
            add(ViolationKind::LoadUpper, k, k, r, x - g.upper[r]);
        }
        // 正の共通個数下限だけは、空の配置先も診断対象になる
        for (int e = 0; e < E(); ++e) {
            const auto& h = p_.hyperedges[e];
            add(ViolationKind::SpanLower, e, -1, -1, h.min_span - s.span[e]);
            add(ViolationKind::SpanUpper, e, -1, -1, s.span[e] - h.max_span);
            add(ViolationKind::SelectedLower, e, -1, -1, h.min_selected - s.selected[e]);
            add(ViolationKind::SelectedUpper, e, -1, -1, s.selected[e] - h.max_selected);
            if (h.local_lower > 0) {
                for (int k = 0; k < K(); ++k)
                    add(ViolationKind::LocalLower, e, k, -1,
                        pick(h.local_lower_by_group, k, h.local_lower) - count(s, e, k));
            } else {
                for (auto [k, lo] : h.local_lower_by_group)
                    add(ViolationKind::LocalLower, e, k, -1, lo - count(s, e, k));
            }
            for (auto [k, c] : s.count[e])
                add(ViolationKind::LocalUpper, e, k, -1,
                    c - pick(h.local_upper_by_group, k, h.local_upper));
        }
        out.feasible = out.violation.empty(); return out;
    }
    State build(std::span<const int> a) const {
        State s; s.a.assign(a.begin(), a.end()); s.size.assign(K(), 0); s.load.assign(K() * R(), 0);
        s.count.resize(E()); s.span.assign(E(), 0); s.selected.assign(E(), 0);
        s.members.resize(K() + 1); s.position.resize(n());
        // 定数項は出力時に加える。大きい定数で探索中の費用差を丸めない
        // 要素負荷、単項費用、使用グループを集計する
        for (int v = 0; v < n(); ++v) {
            const int k = a[v]; assert(-1 <= k && k < K());
            const int slot = k < 0 ? K() : k;
            s.position[v] = static_cast<int>(s.members[slot].size()); s.members[slot].push_back(v);
            s.score.cost += unary(v, k);
            if (!permitted(v, k)) { ++s.score.bad; ++s.score.violation; }
            if (k < 0) continue;
            ++s.size[k]; const auto& x = p_.vertices[v]; const auto& load = pick(x.load_by_group, k, x.load);
            for (int r = 0; r < R(); ++r) s.load[k * R() + r] += load[r];
        }
        // 所属数は辺ごとの疎な整列リスト。空の配置先を E*K 個保持しない
        std::vector<int> labels;
        for (int e = 0; e < E(); ++e) {
            const auto& h = p_.hyperedges[e]; auto& list = s.count[e];
            list.reserve(std::min(static_cast<int>(h.vertices.size()), K()));
            labels.clear();
            for (int v : h.vertices) if (a[v] >= 0) labels.push_back(a[v]);
            std::sort(labels.begin(), labels.end());
            for (int k : labels) {
                if (list.empty() || list.back().first != k) list.push_back({k, 1});
                else ++list.back().second;
            }
            s.span[e] = static_cast<int>(list.size()); s.selected[e] = static_cast<int>(labels.size());
            s.score += edge_score(e, s.span[e], s.selected[e]);
            // 空の配置先の下限違反は共通値を一括計算し、上書き分だけ補正する
            Score base = bound(0, h.local_lower, h.local_upper);
            s.score.violation += base.violation * K(); s.score.bad += base.bad * K();
            for (auto [k, lo] : h.local_lower_by_group) {
                s.score -= base; s.score += bound(0, lo, pick(h.local_upper_by_group, k, h.local_upper));
            }
            for (auto [k, c] : list) {
                s.score -= local_score(e, k, 0); s.score += local_score(e, k, c);
                const auto& load = pick(h.shared_load_by_group, k, h.shared_load);
                for (int r = 0; r < R(); ++r) s.load[k * R() + r] += load[r];
            }
        }
        for (int k = 0; k < K(); ++k) s.score += group_score(k, s.size[k], &s.load[k * R()]);
        return s;
    }
    void touch(int k) {
        if (touched_[k]) return;
        touched_[k] = 1; groups_.push_back(k); delta_size_[k] = 0;
        std::fill_n(delta_load_.begin() + k * R(), R(), 0);
    }
    void propose() {
        proposed_ = current_.score; events_.clear(); cells_.clear(); edges_.clear(); groups_.clear();
        // 交換の2本のCSR列は整列済みなので、再ソートせず併合する。
        size_t split = 0;
        // 各要素の負荷差と、辺・配置先ごとの所属数差を収集する
        for (auto [v, to] : moves_) {
            split = events_.size();
            const int from = current_.a[v];
            proposed_.cost += unary(v, to) - unary(v, from);
            const int d = static_cast<int>(!permitted(v, to)) - static_cast<int>(!permitted(v, from));
            proposed_.bad += d; proposed_.violation += d;
            for (auto [k, sign] : {Pair{from, -1}, Pair{to, 1}}) if (k >= 0) {
                touch(k); delta_size_[k] += sign;
                const auto& x = p_.vertices[v]; const auto& load = pick(x.load_by_group, k, x.load);
                for (int r = 0; r < R(); ++r) delta_load_[k * R() + r] += sign * load[r];
            }
            // 単点ではCSRの順序を利用し、整列済みのイベントを直接生成する
            const int low = std::min(from, to), high = std::max(from, to);
            for (int i = offsets_[v]; i < offsets_[v + 1]; ++i) {
                if (low >= 0) events_.emplace_back(incidence_[i], low, from < to ? -1 : 1);
                if (high >= 0) events_.emplace_back(incidence_[i], high, from < to ? 1 : -1);
            }
        }
        if (moves_.size() == 2) std::inplace_merge(events_.begin(), events_.begin() + static_cast<ptrdiff_t>(split), events_.end());
        else if (moves_.size() > 2) std::sort(events_.begin(), events_.end());
        // 同じ辺を共有する交換も、前後の個数を比較して1回だけ採点する
        for (size_t i = 0; i < events_.size();) {
            const auto [e, k, unused] = events_[i]; (void)unused; int d = 0;
            do { d += std::get<2>(events_[i++]); }
            while (i < events_.size() && std::get<0>(events_[i]) == e && std::get<1>(events_[i]) == k);
            if (d == 0) continue;
            const int before = count(current_, e, k), after = before + d;
            assert(after >= 0); cells_.push_back({e, k, before, after});
            if (local_active_[e]) {
                proposed_ -= local_score(e, k, before); proposed_ += local_score(e, k, after);
            }
            if (edges_.empty() || edges_.back().e != e) edges_.push_back({e, 0, 0});
            const int activation = (after > 0) - (before > 0);
            edges_.back().span += activation; edges_.back().selected += d;
            if (activation != 0) {
                const auto& h = p_.hyperedges[e]; const auto& load = pick(h.shared_load_by_group, k, h.shared_load);
                touch(k);
                for (int r = 0; r < R(); ++r) delta_load_[k * R() + r] += activation * load[r];
            }
        }
        for (const auto& d : edges_) if (d.span || d.selected) {
            proposed_ -= edge_score(d.e, current_.span[d.e], current_.selected[d.e]);
            proposed_ += edge_score(d.e, current_.span[d.e] + d.span, current_.selected[d.e] + d.selected);
        }
        // 非線形負荷費用も、複数要素の合計負荷差で評価する
        for (int k : groups_) {
            proposed_ -= group_score(k, current_.size[k], &current_.load[k * R()]);
            for (int r = 0; r < R(); ++r) delta_load_[k * R() + r] += current_.load[k * R() + r];
            proposed_ += group_score(k, current_.size[k] + delta_size_[k], &delta_load_[k * R()]);
            touched_[k] = 0;
        }
    }
    void commit() {
        // 所属数と集計値の更新は、候補採用後にまとめて行う
        for (const auto& c : cells_) {
            auto& list = current_.count[c.e]; auto it = std::lower_bound(list.begin(), list.end(), Pair{c.k, 0});
            if (c.before == 0) list.insert(it, {c.k, c.after});
            else if (c.after == 0) list.erase(it);
            else it->second = c.after;
        }
        for (const auto& d : edges_) { current_.span[d.e] += d.span; current_.selected[d.e] += d.selected; }
        for (int k : groups_) {
            current_.size[k] += delta_size_[k];
            std::copy_n(delta_load_.begin() + k * R(), R(), current_.load.begin() + k * R());
        }
        // 要素のグループ別一覧は末尾との交換で更新する
        for (auto [v, to] : moves_) {
            const int from = current_.a[v]; auto& old = current_.members[from < 0 ? K() : from];
            old[current_.position[v]] = old.back(); current_.position[old.back()] = current_.position[v]; old.pop_back();
            auto& next = current_.members[to < 0 ? K() : to];
            current_.position[v] = static_cast<int>(next.size()); next.push_back(v); current_.a[v] = to;
        }
        current_.score = proposed_;
    }
    static bool better(const Score& a, const Score& b) {
        if ((a.bad == 0) != (b.bad == 0)) return a.bad == 0;
        if (a.bad != 0 && std::abs(a.violation - b.violation) >
            2e-15 * std::max(std::abs(a.violation), std::abs(b.violation))) return a.violation < b.violation;
        return a.cost < b.cost - 2e-15 * std::max(std::abs(a.cost), std::abs(b.cost));
    }
    void remember() {
        if (better(current_.score, best_)) { best_ = current_.score; best_a_ = current_.a; dirty_result_ = true; }
    }
    void synchronize() {
        if (!dirty_ && ready_) return;
        // 所属関係を変えた場合だけCSRを構築する
        if (topology_dirty_) {
            offsets_.assign(n() + 1, 0);
            for (const auto& h : p_.hyperedges) for (int v : h.vertices) ++offsets_[v + 1];
            std::partial_sum(offsets_.begin(), offsets_.end(), offsets_.begin());
            incidence_.resize(offsets_.back()); auto next = offsets_;
            for (int e = 0; e < E(); ++e) for (int v : p_.hyperedges[e].vertices) incidence_[next[v]++] = e;
            topology_dirty_ = false;
        }
        movable_.assign(n(), scope_ ? 0 : 1); active_.clear();
        if (scope_) for (int v : *scope_) { assert(0 <= v && v < n()); movable_[v] = 1; }
        // 固定を除く前の範囲マスクで修復する。範囲外は勝手に動かさない
        conflict_ = false;
        if (ready_) for (auto* a : {&current_.a, &best_a_})
            for (int v = 0; v < n(); ++v) if (!permitted(v, (*a)[v])) {
                if (!movable_[v]) { conflict_ = true; continue; }
                (*a)[v] = p_.vertices[v].fixed ? *p_.vertices[v].fixed : choice(v, 0);
            }
        for (int v = 0; v < n(); ++v) {
            if (p_.vertices[v].fixed) movable_[v] = 0;
            if (movable_[v] && choice_count(v) > 1) active_.push_back(v);
        }
        // 資源の単位差を吸収する。採点結果には正規化前の違反を出す
        units_.assign(R(), 0); double weight = 0; int coefficients = 0;
        auto magnitude = [&](double x) { if (x != 0) { weight += std::abs(x); ++coefficients; } };
        auto count_magnitude = [&](const CountCost& c) {
            magnitude(c.active); magnitude(c.extra); magnitude(c.cut_value);
            for (size_t i = 1; i < c.table.size(); ++i) magnitude(c.table[i] - c.table[i - 1]);
        };
        for (const auto& v : p_.vertices) {
            for (int r = 0; r < R(); ++r) units_[r] += static_cast<double>(v.load[r]);
            magnitude(v.default_cost); magnitude(v.reject_cost);
            for (auto [k, c] : v.cost_by_group) { (void)k; magnitude(c - v.default_cost); }
        }
        for (auto& x : units_) x = std::max(1.0, x / std::max(1, n()));
        for (const auto& h : p_.hyperedges) {
            count_magnitude(h.span_cost); count_magnitude(h.selected_cost); count_magnitude(h.local_cost);
            for (const auto& x : h.local_cost_by_group) count_magnitude(x.second);
        }
        for (const auto& g : p_.groups) {
            magnitude(g.open_cost);
            for (const auto& f : g.load_penalty) {
                magnitude(f.under_linear); magnitude(f.under_quadratic);
                magnitude(f.over_linear); magnitude(f.over_quadratic);
            }
        }
        scale_ = coefficients ? weight / coefficients : 1;
        // 値が恒等的に0の局所採点だけを省く。条件更新時には必ず判定し直す。
        local_active_.clear();
        for (const auto& h : p_.hyperedges) {
            const auto& c = h.local_cost;
            local_active_.push_back(h.local_lower != 0 || h.local_upper < static_cast<int>(h.vertices.size())
                || !h.local_lower_by_group.empty() || !h.local_upper_by_group.empty()
                || !h.local_cost_by_group.empty() || c.active != 0 || c.extra != 0 || c.cut_value != 0 || !c.table.empty());
        }
        touched_.assign(K(), 0); delta_size_.resize(K()); delta_load_.resize(K() * R());
        // 条件変更後は旧最良解も再評価する。同じ割当の二重集計は省く
        if (ready_) {
            current_ = build(current_.a);
            best_ = current_.a == best_a_ ? current_.score : build(best_a_).score;
            remember();
        }
        dirty_ = false; dirty_result_ = true;
    }
    void construct(int64_t time_limit_us = 100000) {
        // 短時間や採否も選ぶ問題では費用との釣合いを保ち、必須要素の割当では違反を先に減らす
        const bool thorough = time_limit_us >= 10000 && std::ranges::none_of(p_.vertices, &Vertex::allow_reject);
        const int width = thorough ? 32 : 8;
        std::vector<int> a(n(), -1); current_ = build(a);
        std::vector<int> order(n()); std::iota(order.begin(), order.end(), 0);
        for (int i = n() - 1; i > 0; --i) std::swap(order[i], order[random_int(i + 1)]);
        // 固定要素を先に置き、残りは負荷と共有費用を見て逐次配置する
        std::stable_partition(order.begin(), order.end(), [&](int v) { return p_.vertices[v].fixed.has_value(); });
        for (int v : order) {
            const auto& x = p_.vertices[v]; const int choices = x.fixed ? 1 : choice_count(v);
            Score best_score; int dest = -2;
            // 同点で小さい番号に偏らないよう、列挙の開始位置をずらす
            const int first = thorough && choices <= width ? random_int(choices) : 0;
            for (int i = 0; i < (choices <= width ? choices : 8); ++i) {
                const int to = x.fixed ? *x.fixed : choice(v, choices <= width ? (first+i)%choices : random_int(choices));
                moves_ = {{v, to}}; propose();
                // 必須要素の通常予算では最良解と同じ比較、それ以外は従来の費用・違反ペナルティ
                if (dest == -2 || (thorough ? better(proposed_, best_score)
                    : proposed_.cost + 10 * scale_ * proposed_.violation < best_score.cost + 10 * scale_ * best_score.violation)) {
                    best_score = proposed_; dest = to;
                }
            }
            moves_ = {{v, dest}}; propose(); commit();
        }
        current_ = build(current_.a); best_ = current_.score; best_a_ = current_.a; ready_ = true; dirty_result_ = true;
    }
    int destination(int v) {
        if (offsets_[v + 1] > offsets_[v] && uniform() < 0.75) {
            const int e = incidence_[offsets_[v] + random_int(offsets_[v + 1] - offsets_[v])];
            const auto& vertices = p_.hyperedges[e].vertices;
            const int k = current_.a[vertices[random_int(static_cast<int>(vertices.size()))]];
            if (allowed(p_.vertices[v], k)) return k;
        }
        return choice(v, random_int(choice_count(v)));
    }
    bool generate() {
        moves_.clear(); if (active_.empty()) return false;
        const int v = active_[random_int(static_cast<int>(active_.size()))];
        // 全採用・全未採用でない固定個数の辺では、採用・未採用を直接入れ替える
        if (p_.vertices[v].allow_reject && random_int(4) == 0) {
            for (int i = offsets_[v]; i < offsets_[v+1]; ++i) {
                const auto& h = p_.hyperedges[incidence_[i]];
                if (h.min_selected != h.max_selected || h.min_selected <= 0
                    || h.min_selected >= static_cast<int>(h.vertices.size())) continue;
                const int w = h.vertices[random_int(static_cast<int>(h.vertices.size()))];
                const int a = current_.a[v], b = current_.a[w];
                if ((a < 0) != (b < 0) && movable_[w] && allowed(p_.vertices[v], b) && allowed(p_.vertices[w], a)) {
                    moves_ = {{v,b},{w,a}}; return true;
                }
                break;
            }
        }
        const int from = current_.a[v], to = destination(v);
        if (from == to) return false;
        moves_.push_back({v, to});
        // 交換は採用・未採用の入替えにも使い、厳しい容量を直接保つ候補を増やす
        if (random_int(100) < 25) {
            const auto& list = current_.members[to < 0 ? K() : to];
            if (list.empty()) return false;
            const int w = list[random_int(static_cast<int>(list.size()))];
            if (!movable_[w] || !allowed(p_.vertices[w], from)) return false;
            moves_.push_back({w, from});
        }
        return true;
    }
    const Result& search(const Options& options, Clock::time_point start, int64_t step_limit = INT64_MAX) {
        assert(options.time_limit_us >= 0);
        int64_t steps = 0; double penalty = 10 * scale_, temperature = scale_, cooling = 1;
        int feasible_visits = 0; const bool deterministic = step_limit != INT64_MAX;
        // 探索中は違反を許すが、最良実行可能解は独立して保管する
        if (!conflict_ && !active_.empty() && options.time_limit_us > 0) {
            while (steps < step_limit) {
                // 重い差分評価の後は、次の32試行を待たずに時間を確認する
                if ((steps & 31) == 0 || (events_.size() + 2) * static_cast<size_t>(R()) > 256) {
                    const double elapsed = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
                    const double progress = deterministic ? static_cast<double>(steps) / static_cast<double>(step_limit)
                                                          : elapsed / static_cast<double>(options.time_limit_us);
                    if (progress >= 1) break;
                    if ((steps & 31) == 0) {
                        cooling = std::pow(0.01, progress); temperature = scale_ * cooling;
                    }
                }
                ++steps;
                if (generate()) {
                    propose();
                    const double delta = proposed_.cost - current_.score.cost
                        + penalty * (proposed_.violation - current_.score.violation);
                    if (delta <= 0 || (options.annealing && uniform() < std::exp(-delta / (temperature +
                        (best_.bad ? penalty * cooling : 0))))) {
                        commit(); remember();
                    }
                }
                feasible_visits += current_.score.bad == 0;
                if ((steps & 511) == 0) {
                    if (feasible_visits < 64) penalty = std::min(penalty * 1.5, scale_ * 1e8);
                    else if (feasible_visits > 384) penalty = std::max(penalty / 1.1, scale_);
                    feasible_visits = 0;
                }
            }
        }
        // 時間0の既存状態では全評価を繰り返さない
        if (dirty_result_) {
            result_.assignment = best_a_;
            // 探索前に確定する結果には、同期・初期化で作った集計を再利用する
            static_cast<Evaluation&>(result_) = steps == 0 && current_.a == best_a_
                ? diagnose(current_) : evaluate(best_a_);
            result_.scope_conflict = conflict_; dirty_result_ = false;
        }
        result_.iterations = steps;
        result_.elapsed_us = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        return result_;
    }
    bool dirty_result_ = true;
};
#if __INCLUDE_LEVEL__ == 0
struct HypergraphPartitionerTest {
    using S = HypergraphPartitioner;
    using P = S::Problem;
    using C = S::CountCost;
    std::mt19937_64 rng{904621};
    int checks = 0;
    int ri(int n) { return static_cast<int>(rng() % static_cast<uint64_t>(n)); }
    static void require(bool ok) { if (!ok) { std::cerr << "TEST FAILED\n"; std::abort(); } }
    static void near(double a, double b) {
        if (!std::isfinite(a) || !std::isfinite(b) || std::abs(a - b) > 1e-7 * (1 + std::abs(b))) {
            std::cerr << std::setprecision(17) << a << " != " << b << '\n'; std::abort();
        }
    }
    static double cost(const C& c, int x) {
        double ans = x ? c.active : 0;
        if (x > 1) ans += c.extra * (x - 1) + c.cut_value;
        if (!c.table.empty()) ans += c.table[static_cast<size_t>(x)];
        return ans;
    }
    template<class T> static T over(const std::vector<std::pair<int,T>>& list, int k, T base) {
        for (const auto& [key, value] : list) if (key == k) base = value;
        return base;
    }
    static S::Score naive(const S& solver, const std::vector<int>& a) {
        const auto& p = solver.p_; const int k = p.num_groups, r = p.num_resources;
        S::Score ans; ans.cost = p.constant;
        std::vector<int> count(k);
        std::vector<std::vector<int64_t>> load(k, std::vector<int64_t>(r));
        auto violation = [&](int64_t val, int64_t lo, int64_t hi, double scale = 1) {
            if (val < lo) { ++ans.bad; ans.violation += static_cast<double>(lo - val) / scale; }
            if (val > hi) { ++ans.bad; ans.violation += static_cast<double>(val - hi) / scale; }
        };
        for (size_t v = 0; v < a.size(); ++v) {
            const auto& x = p.vertices[v]; const int g = a[v];
            bool permitted = g == -1 ? x.allow_reject : !x.allowed_groups ||
                std::find(x.allowed_groups->begin(), x.allowed_groups->end(), g) != x.allowed_groups->end();
            if (x.fixed && g != *x.fixed) permitted = false;
            if (!permitted) { ++ans.bad; ++ans.violation; }
            if (g < 0) ans.cost += x.reject_cost;
            else {
                ++count[g]; ans.cost += over(x.cost_by_group, g, x.default_cost);
                auto l = over(x.load_by_group, g, x.load);
                for (int j = 0; j < r; ++j) load[g][j] += l[j];
            }
        }
        for (const auto& h : p.hyperedges) {
            std::vector<int> counts(k);
            for (int v : h.vertices) if (a[v] >= 0) ++counts[a[v]];
            int selected = 0, span = 0;
            for (int g = 0; g < k; ++g) {
                selected += counts[g]; span += counts[g] > 0;
                ans.cost += cost(over(h.local_cost_by_group, g, h.local_cost), counts[g]);
                violation(counts[g], over(h.local_lower_by_group, g, h.local_lower),
                          over(h.local_upper_by_group, g, h.local_upper));
                if (counts[g]) {
                    auto l = over(h.shared_load_by_group, g, h.shared_load);
                    for (int j = 0; j < r; ++j) load[g][j] += l[j];
                }
            }
            ans.cost += cost(h.span_cost, span) + cost(h.selected_cost, selected);
            violation(span, h.min_span, h.max_span); violation(selected, h.min_selected, h.max_selected);
        }
        for (int g = 0; g < k; ++g) {
            const auto& x = p.groups[g];
            if (count[g]) ans.cost += x.open_cost;
            for (int j = 0; j < r; ++j) {
                violation(load[g][j], x.lower_only_if_used && !count[g] ? 0 : x.lower[j], x.upper[j], solver.units_[j]);
                const auto& f = x.load_penalty[j];
                const double a0 = static_cast<double>(std::max(int64_t(0), f.lower - load[g][j]));
                const double b0 = static_cast<double>(std::max(int64_t(0), load[g][j] - f.upper));
                ans.cost += f.under_linear * a0 + f.under_quadratic * a0 * a0
                    + f.over_linear * b0 + f.over_quadratic * b0 * b0;
            }
        }
        return ans;
    }
    void check(const S& s) {
        const auto b = naive(s, s.current_.a);
        near(s.current_.score.cost + s.p_.constant, b.cost); near(s.current_.score.violation, b.violation);
        require(s.current_.score.bad == b.bad);
        const auto fresh = s.build(s.current_.a);
        require(fresh.load == s.current_.load && fresh.count == s.current_.count);
        require(fresh.size == s.current_.size && fresh.span == s.current_.span && fresh.selected == s.current_.selected);
        for (int v = 0; v < s.n(); ++v) {
            const int k = s.current_.a[v] < 0 ? s.K() : s.current_.a[v];
            require(s.current_.members[k][s.current_.position[v]] == v);
        }
        auto ev = s.evaluate(s.current_.a);
        require(ev.feasible == (b.bad == 0)); require(static_cast<int64_t>(ev.violation.size()) == b.bad);
        near(ev.cost, b.cost); ++checks;
    }
    C random_cost(int maximum) {
        C c; c.active = (ri(21) - 10) * 0.2; c.extra = (ri(21) - 10) * 0.3; c.cut_value = ri(11) - 5;
        if (ri(3) == 0) {
            c.table.resize(maximum + 1);
            for (int i = 1; i <= maximum; ++i) c.table[i] = (ri(31) - 15) * 0.17;
        }
        return c;
    }
    P random_problem() {
        const int n = 1 + ri(14), k = 1 + ri(5), r = 1 + ri(3);
        P p(n, k, r); p.constant = -3.7;
        for (auto& x : p.vertices) {
            for (auto& l : x.load) l = ri(6);
            x.default_cost = (ri(31) - 15) * 0.23; x.reject_cost = (ri(31) - 15) * 0.3;
            x.allow_reject = ri(2) == 0;
            if (ri(3) == 0) {
                x.allowed_groups = std::vector<int>{};
                for (int g = 0; g < k; ++g) if (ri(2) == 0) x.allowed_groups->push_back(g);
                if (x.allowed_groups->empty()) x.allow_reject = true;
            }
            for (int g = 0; g < k; ++g) {
                if (ri(3) == 0) x.cost_by_group.emplace_back(g, (ri(21) - 10) * 0.4);
                if (ri(4) == 0) {
                    std::vector<int64_t> load(r); for (auto& l : load) l = ri(8);
                    x.load_by_group.emplace_back(g, load);
                }
            }
            if (ri(5) == 0) {
                if (x.allow_reject) x.fixed = -1;
                else x.fixed = x.allowed_groups ? x.allowed_groups->front() : ri(k);
            }
        }
        for (auto& g : p.groups) {
            g.open_cost = (ri(13) - 6) * 0.9; g.lower_only_if_used = ri(2) == 0;
            for (int j = 0; j < r; ++j) {
                g.lower[j] = ri(7); g.upper[j] = 7 + ri(20);
                auto& f = g.load_penalty[j]; f.lower = ri(10); f.upper = 10 + ri(10);
                f.under_linear = ri(4) * 0.2; f.over_linear = ri(4) * 0.4;
                f.under_quadratic = ri(4) * 0.03; f.over_quadratic = ri(4) * 0.02;
            }
        }
        const int m = ri(18);
        for (int e = 0; e < m; ++e) {
            std::vector<int> vs;
            for (int v = 0; v < n; ++v) if (ri(3) == 0) { vs.push_back(v); if (ri(4) == 0) vs.push_back(v); }
            p.add_hyperedge(vs); auto& h = p.hyperedges.back();
            const int maxc = static_cast<int>(vs.size());
            h.span_cost = random_cost(k); h.local_cost = random_cost(maxc); h.selected_cost = random_cost(maxc);
            h.min_span = ri(3); h.max_span = 2 + ri(3); h.min_selected = ri(3); h.max_selected = 2 + ri(n);
            h.local_lower = ri(3); h.local_upper = 3 + ri(4);
            h.shared_load.resize(r); for (auto& l : h.shared_load) l = ri(5);
            for (int g = 0; g < k; ++g) {
                if (ri(3) == 0) h.local_cost_by_group.emplace_back(g, random_cost(maxc));
                if (ri(3) == 0) h.local_lower_by_group.emplace_back(g, ri(3));
                if (ri(3) == 0) h.local_upper_by_group.emplace_back(g, 3 + ri(4));
                if (ri(3) == 0) {
                    std::vector<int64_t> l(r); for (auto& x : l) x = ri(5);
                    h.shared_load_by_group.emplace_back(g, l);
                }
            }
        }
        return p;
    }
    void random_deltas() {
        for (int instance = 0; instance < 300; ++instance) {
            S s(random_problem()); s.solve({0}); check(s);
            std::vector<int> ids(s.n()); std::iota(ids.begin(), ids.end(), 0);
            for (int step = 0; step < 400; ++step) {
                std::shuffle(ids.begin(), ids.end(), rng); s.moves_.clear();
                auto a = s.current_.a;
                for (int i = 0; i < std::min(s.n(), 1 + ri(6)); ++i) {
                    const int v = ids[i]; const auto& x = s.p_.vertices[v];
                    const int to = x.fixed ? *x.fixed : s.choice(v, ri(s.choice_count(v)));
                    if (to != a[v]) { s.moves_.emplace_back(v, to); a[v] = to; }
                }
                s.propose(); const auto expected = naive(s, a);
                near(s.proposed_.cost + s.p_.constant, expected.cost); near(s.proposed_.violation, expected.violation);
                require(s.proposed_.bad == expected.bad);
                if (ri(3)) s.commit(); // 不採用時に現状態を変更しないことも確認
                check(s);
            }
        }
    }
    void api_tests() {
        for (int rep = 0; rep < 80; ++rep) {
            S s(random_problem()); s.resume({0}); check(s);
            auto initial = s.result().assignment;
            s.improve(s.result().assignment, {0}); require(s.result().assignment == initial);
            std::vector<int> movable;
            for (int v = 0; v < s.n(); ++v) if (ri(2)) movable.push_back(v);
            s.improve(initial, {0}, movable);
            s.search({1}, S::Clock::now(), 1000); check(s);
            for (int v = 0; v < s.n(); ++v) if (std::find(movable.begin(), movable.end(), v) == movable.end())
                require(s.result().assignment[v] == initial[v]);
            for (int step = 0; step < 30; ++step) {
                const int v = ri(s.n()); auto vertex = s.p_.vertices[v]; vertex.default_cost += 1.2;
                vertex.load[0] += ri(3); s.set_vertex(v, vertex);
                auto group = s.p_.groups[0]; group.upper[0] += ri(3); group.open_cost -= 0.5; s.set_group(0, group);
                if (s.E()) {
                    auto edge = s.p_.hyperedges[0]; edge.span_cost.active += 0.7;
                    if (edge.local_cost.table.empty() && edge.selected_cost.table.empty() && edge.span_cost.table.empty()) {
                        // 表の範囲を変えない単純な所属更新
                        edge.vertices = {ri(s.n())}; edge.local_cost_by_group.clear();
                    }
                    s.set_hyperedge(0, edge);
                }
                s.resume({0}); check(s);
                near(s.result().cost, naive(s, s.result().assignment).cost);
            }
            s.reset_problem(P(0, 3, 2)); s.resume({0}); require(s.result().feasible && s.result().assignment.empty());
        }
        // スコープ外の固定変更を黙って反映しない
        P p(3, 2); S s(p); s.improve(std::vector<int>{0,0,0}, {0}, std::vector<int>{0});
        auto v = p.vertices[2]; v.fixed = 1; s.set_vertex(2, v); s.resume({0});
        require(s.result().scope_conflict && !s.result().feasible && s.result().assignment[2] == 0);
        v.fixed = 0; s.set_vertex(2, v); s.resume({0}); require(!s.result().scope_conflict && s.result().feasible);
        s.improve(s.result().assignment, {0}, std::vector<int>{}); s.search({1}, S::Clock::now(), 1000);
        require(s.result().iterations == 0);
        // 同じ乱数状態とステップ数なら再現する。呼出しごとの暗黙のseedリセットはしない
        auto problem = random_problem(); S a(problem, 18), b(problem, 18);
        a.solve({0}); b.solve({0}); a.search({1}, S::Clock::now(), 3000); b.search({1}, S::Clock::now(), 3000);
        require(a.result().assignment == b.result().assignment && a.rng_ == b.rng_);
        const auto state = a.rng_; a.resume({0}); require(a.rng_ == state);
        a.set_seed(14); require(a.rng_ == 14);
    }
    void boundaries() {
        near(C::activation(3)(0), 0); near(C::activation(3)(2), 3);
        near(C::connectivity(-2)(3), -4); near(C::cut(4)(1), 0); near(C::cut(4)(2), 4);
        near(C::linear(3)(4), 12); near(C::lookup({0, 2, -4})(2), -4);
        P p(1, 2, 2); p.vertices[0].load = {0,0}; p.groups[0].open_cost = 7;
        p.groups[0].lower = {1,0}; p.groups[0].lower_only_if_used = true;
        p.groups[1].load_penalty[0].lower = 3; p.groups[1].load_penalty[0].under_quadratic = 2;
        int e = p.add_hyperedge({0,0}); p.hyperedges[e].shared_load = {2,3};
        p.hyperedges[e].local_cost = C::activation(4);
        S s(p); auto ev = s.evaluate(std::vector<int>{0}); require(ev.feasible); near(ev.cost, 29);
        require(s.p_.hyperedges[0].vertices.size() == 1);
        p.add_hyperedge({0}); S s2(p); require(s2.p_.hyperedges.size() == 2);
        p.vertices[0].allow_reject = true; p.vertices[0].allowed_groups = std::vector<int>{}; p.vertices[0].fixed = -1;
        p.hyperedges[0].min_span = 1; S s3(p); s3.solve({0}); require(!s3.result().feasible);
        // 全未採用であっても辺の下限・空グループのソフト費用は残る
        near(s3.result().cost, 18); require(s3.result().assignment[0] == -1);
        // 一時違反の個数まで表を持つ
        P q(3, 3); q.add_hyperedge({0,1,2}, C::lookup({0,1,2,9})); q.hyperedges[0].max_span = 1;
        S s4(q); near(s4.evaluate(std::vector<int>{0,1,2}).cost, 9);
        // 固定要素が使う共有物は可動要素を除いても残る
        P w(2, 2); w.vertices[0].fixed = 0; w.add_hyperedge({0,1}); w.hyperedges[0].shared_load = {5};
        S s5(w); s5.improve(std::vector<int>{0,0}, {0}); s5.moves_ = {{1,1}}; s5.propose(); s5.commit();
        require(s5.current_.load == std::vector<int64_t>({6,6})); check(s5);
        // 1e12の負荷に対する二乗費用も整数で二乗しない
        P large(1, 1); large.vertices[0].load[0] = 1000000000000LL;
        large.groups[0].load_penalty[0].upper = 0; large.groups[0].load_penalty[0].over_quadratic = 0.5;
        S s6(large); near(s6.evaluate(std::vector<int>{0}).cost, 5e23);
    }
    void exhaustive_tests() {
        for (int rep = 0; rep < 60; ++rep) {
            P p(5, 3, 2);
            for (auto& v : p.vertices) {
                v.allow_reject = true; v.reject_cost = ri(7); v.load = {ri(4), ri(4)};
                for (int k = 0; k < 3; ++k) v.cost_by_group.emplace_back(k, ri(11) - 5);
            }
            for (auto& g : p.groups) { g.upper = {4 + ri(6), 4 + ri(6)}; g.open_cost = ri(5); }
            for (int e = 0; e < 4; ++e) {
                int id = p.add_hyperedge({ri(5),ri(5),ri(5)}, random_cost(3));
                p.hyperedges[id].shared_load = {ri(3),ri(3)};
                p.hyperedges[id].min_selected = ri(2);
            }
            S solver(p, 12); solver.solve({0});
            double optimum = std::numeric_limits<double>::infinity(); std::vector<int> a(5);
            for (int mask = 0; mask < 1024; ++mask) {
                int bits = mask; for (auto& v : a) { v = bits % 4 - 1; bits /= 4; }
                const auto expected = naive(solver, a); const auto eval = solver.evaluate(a);
                near(expected.cost, eval.cost); require(eval.feasible == (expected.bad == 0));
                if (eval.feasible) optimum = std::min(optimum, eval.cost);
            }
            solver.search({1}, S::Clock::now(), 2000);
            if (solver.result().feasible) require(solver.result().cost + 1e-8 >= optimum);
        }
        // 許可先のため単点・交換では実行可能性を保てない3要素の循環
        P p(3, 3);
        for (int v = 0; v < 3; ++v) {
            p.vertices[v].allowed_groups = std::vector<int>{v, (v+1)%3};
            p.groups[v].lower[0] = p.groups[v].upper[0] = 1;
        }
        S solver(p); solver.improve(std::vector<int>{0,1,2}, {0});
        solver.moves_ = {{0,1},{1,2},{2,0}}; solver.propose(); solver.commit();
        require(solver.current_.score.bad == 0); check(solver);
        // 方式の採用・未採用入替えと共有負荷を同じトランザクションで扱う
        P q(2, 1); for (auto& v : q.vertices) v.allow_reject = true;
        q.add_hyperedge({0,1}); q.hyperedges[0].min_selected = q.hyperedges[0].max_selected = 1;
        q.hyperedges[0].shared_load = {7};
        S s(q); s.improve(std::vector<int>{0,-1}, {0}); s.moves_ = {{0,-1},{1,0}};
        s.propose(); s.commit(); require(s.current_.load[0] == 8 && s.current_.score.bad == 0); check(s);
    }
    void update_and_symmetry_tests() {
        P p(2, 2); p.vertices[0].cost_by_group = {{0,0},{1,10}};
        S s(p); s.improve(std::vector<int>{0,0}, {0});
        s.moves_ = {{0,1}}; s.propose(); s.commit(); // 現在解だけを変更し旧最良解は保持
        p.vertices[0].cost_by_group[1].second = -10; s.set_vertex(0, p.vertices[0]);
        s.resume({0}); require(s.result().assignment[0] == 1); near(s.result().cost, -10);
        p.vertices[0].allowed_groups = std::vector<int>{0}; s.set_vertex(0, p.vertices[0]);
        s.resume({0}); require(s.result().feasible && s.result().assignment[0] == 0);
        s.improve(s.result().assignment, {0}, std::vector<int>{0});
        p.vertices[0].allowed_groups.reset(); p.vertices[0].fixed = 1;
        s.set_vertex(0, p.vertices[0]); s.resume({0});
        require(!s.result().scope_conflict && s.result().assignment == std::vector<int>({1,0}));
        // グループ名の置換では全てのグループ依存入力を対応させる
        for (int rep = 0; rep < 80; ++rep) {
            auto problem = random_problem(); S original(problem); original.solve({0});
            auto renamed = original.p_; const int k = renamed.num_groups;
            std::vector<int> permutation(k); std::iota(permutation.begin(), permutation.end(), 0);
            std::shuffle(permutation.begin(), permutation.end(), rng);
            auto groups = renamed.groups;
            for (int g = 0; g < k; ++g) renamed.groups[permutation[g]] = groups[g];
            auto remap = [&](auto& values) { for (auto& x : values) x.first = permutation[x.first]; };
            for (auto& v : renamed.vertices) {
                remap(v.load_by_group); remap(v.cost_by_group);
                if (v.allowed_groups) for (int& g : *v.allowed_groups) g = permutation[g];
                if (v.fixed && *v.fixed >= 0) v.fixed = permutation[*v.fixed];
            }
            for (auto& h : renamed.hyperedges) {
                remap(h.local_cost_by_group); remap(h.shared_load_by_group);
                remap(h.local_lower_by_group); remap(h.local_upper_by_group);
            }
            auto a = original.result().assignment;
            for (int& g : a) if (g >= 0) g = permutation[g];
            S other(renamed); auto ev = other.evaluate(a);
            near(ev.cost, original.result().cost);
            require(ev.feasible == original.result().feasible);
            require(ev.violation.size() == original.result().violation.size());
        }
    }
    void small_cost_test() {
        P p(2, 2); for (auto& v : p.vertices) v.cost_by_group = {{0,0},{1,1e-12}};
        S s(p); s.improve(std::vector<int>{1,1}, {0}); s.search({1,false}, S::Clock::now(), 2000);
        require(s.result().cost == 0);
        P q(3, 2); q.groups[1].load_penalty[0].upper = 0;
        q.groups[1].load_penalty[0].over_linear = 1e-12;
        S t(q); t.improve(std::vector<int>{1,1,1}, {0}); t.search({1,false}, S::Clock::now(), 2000);
        require(t.result().cost == 0);
    }
    void review_regressions() {
        // 目的値の定数項は初期化と探索の選択を変えない
        for (double shift : {0.0, 1e18, -1e18}) {
            P p(1, 2); p.constant = shift; p.vertices[0].cost_by_group = {{0,1},{1,0}};
            S s(p); require(s.solve({0}).assignment[0] == 1);
            s.improve(std::vector<int>{0}, {0}); s.search({1,false}, S::Clock::now(), 2000);
            require(s.result().assignment[0] == 1 && s.result().cost == shift);
        }
        // 正規化後に1e-9より小さくても、不可避な違反は少ない解を保持する
        P p(2, 2); p.vertices[0].load[0] = 1000000000000LL; p.vertices[0].fixed = 0;
        p.groups[0].upper[0] = 999999999999LL;
        p.vertices[1].cost_by_group = {{0,0},{1,1e-13}};
        S s(p); s.improve(std::vector<int>{0,1}, {0}); s.search({1,false}, S::Clock::now(), 2000);
        require(s.result().violation.size() == 1 && s.result().violation[0].amount == 1);
        // 範囲内の固定変更は修復し、範囲外の固定変更は衝突として保持する
        P q(6, 2); S t(q); t.improve(std::vector<int>(6,0), {0}, std::vector<int>{4,0,4,2});
        for (int v = 0; v < 6; ++v) { q.vertices[v].fixed = 1; t.set_vertex(v,q.vertices[v]); }
        t.resume({0}); require(t.result().scope_conflict);
        require(t.result().assignment == std::vector<int>({1,0,1,0,1,0}));
        require(t.result().violation.size() == 3); check(t);
    }
    void generated_neighborhood_tests() {
        for (int instance = 0; instance < 100; ++instance) {
            auto p = random_problem();
            for (size_t e = 0; e < p.hyperedges.size(); e += 2) {
                auto& h = p.hyperedges[e];
                const std::set<int> vertices(h.vertices.begin(), h.vertices.end());
                h.min_selected = h.max_selected = ri(static_cast<int>(vertices.size()) + 1);
            }
            S s(p, rng()); s.solve({0}); const auto initial = s.current_.a;
            std::vector<int> scope;
            for (int v = 0; v < s.n(); ++v) if (ri(2)) scope.push_back(v);
            s.improve(initial, {0}, scope);
            for (int step = 0; step < 500; ++step) {
                if (s.generate()) {
                    auto a = s.current_.a; std::set<int> moved;
                    for (auto [v, to] : s.moves_) {
                        require(s.movable_[v] && s.permitted(v, to) && moved.insert(v).second);
                        a[v] = to;
                    }
                    s.propose(); const auto expected = naive(s, a);
                    near(s.proposed_.cost + s.p_.constant, expected.cost);
                    near(s.proposed_.violation, expected.violation); require(s.proposed_.bad == expected.bad);
                    if (ri(2)) s.commit();
                }
                for (int v = 0; v < s.n(); ++v) if (!s.movable_[v]) require(s.current_.a[v] == initial[v]);
                check(s);
            }
        }
    }
    void initialization_regressions() {
        for (int k : {1,8,9,16,31,32,33,128}) for (int64_t budget : {0,9999,10000,100000}) {
            for (int seed = 0; seed < 16; ++seed) {
                P p(1,k); const int target = seed % k;
                p.vertices[0].allow_reject = k % 2;
                p.vertices[0].cost_by_group = {{target, 1e8}};
                p.add_hyperedge({0}); p.hyperedges[0].local_lower_by_group = {{target,1}};
                S s(p, static_cast<uint64_t>(seed)); s.synchronize(); s.construct(budget); check(s);
                if (k + p.vertices[0].allow_reject <= (budget < 10000 || p.vertices[0].allow_reject ? 8 : 32)) {
                    require(s.current_.score.bad == 0 && s.current_.a[0] == target);
                }
            }
        }
        // グループ総数ではなく、許可された選択肢数で列挙を決める
        P p(1,512); p.vertices[0].allowed_groups = std::vector<int>{3,100,200,300,511};
        p.add_hyperedge({0}); p.hyperedges[0].local_lower_by_group = {{511,1}};
        S a(p,17), b(p,17); a.solve({0}); b.resume({0});
        require(a.result().feasible && a.result().assignment[0] == 511);
        require(a.result().assignment == b.result().assignment && a.rng_ == b.rng_);
        check(a); check(b);
    }
    void performance_regressions() {
        P p(24,9,2);
        for (int v=0;v<24;++v) p.vertices[v].load={1+v%3,2+v%4};
        for (int e=0;e<14;++e) p.add_hyperedge({e,(e+1)%24,(e+8)%24,(e+13)%24},C::connectivity(0.25));
        S s(p,11); std::vector<int> initial(24);
        for (int v=0;v<24;++v) initial[v]=v%9;
        s.improve(initial,{0}); check(s);
        // No-local-score detection must be recomputed on every kind of override update.
        for (int rep=0;rep<48;++rep) {
            auto h=p.hyperedges[0]; const int phase=rep%8;
            if (phase==1) h.local_upper=1;
            if (phase==2) h.local_cost=C::activation(-0.25);
            if (phase==3) h.local_lower_by_group={{0,2}};
            if (phase==4) h.local_upper_by_group={{1,0}};
            if (phase==5) h.local_cost_by_group={{2,C::cut(-0.5)}};
            if (phase==6) h.local_cost=C::lookup({0,-1,2,-3,4});
            if (phase==7) h.local_lower=1;
            s.set_hyperedge(0,h); s.resume({0}); check(s);
            require(static_cast<bool>(s.local_active_[0]) == (phase!=0));
            for (int step=0;step<64;++step) {
                const int v=step%24, w=(step+7)%24;
                auto a=s.current_.a; s.moves_.clear();
                if (a[v]!=a[w]) { s.moves_={{v,a[w]},{w,a[v]}}; std::swap(a[v],a[w]); }
                s.propose(); const auto expected=naive(s,a);
                near(s.proposed_.cost,expected.cost); near(s.proposed_.violation,expected.violation);
                require(s.proposed_.bad==expected.bad); s.commit(); check(s);
            }
        }
        // Small/binary count lookup boundary, unchanged cells and merged exchanges.
        for (int k : {1,7,8,9,16,32,64,512}) {
            P q(48,k); std::vector<int> a(48);
            for (int v=0;v<48;++v) a[v]=v%k;
            std::vector<int> all(48); std::iota(all.begin(),all.end(),0);
            q.add_hyperedge(all,C::cut(0.3)); q.hyperedges[0].shared_load={3};
            q.hyperedges[0].selected_cost=C::linear(-0.2);
            S t(q,7); t.improve(a,{0}); check(t);
            for (int step=0;step<80;++step) {
                t.moves_.clear(); const int v=step%48, w=(step+13)%48;
                if (t.current_.a[v]!=t.current_.a[w]) t.moves_={{v,t.current_.a[w]},{w,t.current_.a[v]}};
                t.propose(); t.commit(); check(t);
            }
        }
    }

    void lifecycle_regressions() {
        // 範囲外の固定変更による衝突を、新しい探索・問題へ持ち越さない。
        for (int mode=0;mode<3;++mode) {
            P p(3,2); S s(p);
            s.improve(std::vector<int>{0,0,0},{0},std::vector<int>{0});
            p.vertices[2].fixed=1; s.set_vertex(2,p.vertices[2]);
            require(s.resume({0}).scope_conflict);
            if (mode==0) s.solve({0});
            else if (mode==1) s.improve(std::vector<int>{0,0,1},{0});
            else { s.reset_problem(P(0,1)); s.resume({0}); }
            require(!s.result().scope_conflict && s.result().feasible); check(s);
        }
    }

    void run() { lifecycle_regressions(); performance_regressions(); boundaries(); random_deltas(); api_tests(); exhaustive_tests(); update_and_symmetry_tests(); small_cost_test(); review_regressions(); generated_neighborhood_tests(); initialization_regressions(); std::cout << "PASS " << checks << " state checks, 120000 joint-move trials, 50000 generated-neighborhood trials, 2400 update sequences, 61440 exhaustive assignments, review and initialization regressions\n"; }
};
int main() { HypergraphPartitionerTest t; t.run(); }
#endif
