#pragma once
#include <bits/stdc++.h>

// 容量を共有する複数要求を、単純路で接続するヒューリスティック。
// C++20 / GCC。includeして使用。単体コンパイルでは末尾の自己テストを実行。
// IDはint、消費・容量はint64_t、Costは64bit符号付き整数またはdoubleを想定。
// 配列長・全ての中間積・総和は採用型に収まること。時間指定は通常のコンテスト時間を想定。
// 費用・消費は非負。Modelの資源費用は有限・非減少かつf(0)=0。負の費用は非対応。
// Modelは純粋関数とし、ProblemとModelの参照先はSolver使用中に生存させる。
// Problem/参照先の変更後はrefresh、トポロジー変更後はresetを呼ぶ。
// 計算量表記: n頂点、m辺、r資源、k要求、u全入力の消費/上限指定数、h経路集計の走査量。
// 時間制限は協調的な中断。入力取込み・結果保存・ロールバックの時間は別途必要になり得る。
template<class Cost = int64_t>
class CapacitatedRouter {
    static_assert((std::is_integral_v<Cost> && std::is_signed_v<Cost> && sizeof(Cost) >= 8)
                  || std::is_same_v<Cost, double>);
public:
    using Clock = std::chrono::steady_clock;
    enum class Objective { PenaltyPlusCost, PenaltyThenCost };
    enum class StopReason { TimeLimit, IterationLimit, NoMovableRequests, FixedConflict };
    struct ResourceUse { int resource; int64_t per_unit = 1, per_use = 0; };
    struct LocalLimit { int resource; int64_t upper; };
    struct Resource { int64_t capacity = -1, background_load = 0; };
    struct Vertex { Cost cost = 0; bool enabled = true; std::vector<ResourceUse> uses; };
    struct Arc { int from, to; Cost cost = 1; bool enabled = true; std::vector<ResourceUse> uses; };
    struct Route {
        bool connected = false;
        std::vector<int> arcs;
        bool operator==(const Route&) const = default;
    };
    struct Request {
        int source, target;
        int64_t demand = 1;
        bool active = true, required = true;
        Cost reject_cost = 0, cost_scale = 1;
        bool consume_endpoints = true;
        std::vector<ResourceUse> uses;
        std::vector<LocalLimit> limits;
        std::optional<Route> fixed_route;
    };
    struct Solution { std::vector<Route> routes; bool operator==(const Solution&) const = default; };
    struct Problem {
        std::vector<Vertex> vertices;
        std::vector<Arc> arcs;
        std::vector<Resource> resources;
        std::vector<Request> requests;
        Objective objective = Objective::PenaltyPlusCost;
        // n頂点の問題を作る。O(n)。
        explicit Problem(int n = 0) { assert(n >= 0); vertices.resize(static_cast<size_t>(n)); }
        // 共有資源を追加する。償却O(1)。容量-1は上限なし。
        int add_resource(int64_t capacity = -1, int64_t background_load = 0) {
            assert(capacity >= -1 && background_load >= 0);
            resources.push_back({capacity, background_load});
            return static_cast<int>(resources.size()) - 1;
        }
        // 有向辺を追加。容量指定時は専用資源を作る。償却O(1)。
        int add_arc(int u, int v, Cost cost = 1, int64_t capacity = -1) {
            assert(u >= 0 && u < static_cast<int>(vertices.size()));
            assert(v >= 0 && v < static_cast<int>(vertices.size()) && cost >= 0 && capacity >= -1);
            arcs.push_back({u, v, cost, true, {}});
            if (capacity >= 0) arcs.back().uses.push_back({add_resource(capacity), 1, 0});
            return static_cast<int>(arcs.size()) - 1;
        }
        // 逆向き2辺を追加し、容量を1資源で共有する。償却O(1)。
        std::pair<int, int> add_undirected_edge(int u, int v, Cost cost = 1, int64_t capacity = -1) {
            assert(capacity >= -1);
            int a = add_arc(u, v, cost), b = add_arc(v, u, cost);
            if (capacity >= 0) {
                int r = add_resource(capacity);
                arcs[a].uses.push_back({r, 1, 0}); arcs[b].uses.push_back({r, 1, 0});
            }
            return {a, b};
        }
        // 頂点に独立した容量制約を追加する。償却O(1)。
        int add_vertex_capacity(int v, int64_t capacity) {
            assert(v >= 0 && v < static_cast<int>(vertices.size()) && capacity >= 0);
            int r = add_resource(capacity);
            vertices[v].uses.push_back({r, 1, 0});
            return r;
        }
        // 有効な必須要求を追加する。償却O(1)。
        int add_request(int s, int t, int64_t demand = 1) {
            assert(s >= 0 && s < static_cast<int>(vertices.size()));
            assert(t >= 0 && t < static_cast<int>(vertices.size()) && demand >= 0);
            requests.push_back({s, t, demand, true, true, 0, 1, true, {}, {}, {}});
            return static_cast<int>(requests.size()) - 1;
        }
    };
    struct DefaultModel {
        // 倍率適用前の辺費用を返す。O(1)。nulloptで使用禁止。
        std::optional<Cost> arc_cost(const Problem& p, int, int arc) const { return p.arcs[arc].cost; }
        // 倍率適用前の頂点費用を返す。O(1)。
        std::optional<Cost> vertex_cost(const Problem& p, int, int v) const { return p.vertices[v].cost; }
        // 共有資源の費用を返す。O(1)。有限容量では0〜容量内のみ呼ばれる。
        Cost resource_cost(const Problem&, int, int64_t) const { return 0; }
    };
    struct Value {
        Cost reject_cost = 0, route_cost = 0, resource_cost = 0;
        // 和目的の値を返す。O(1)。
        Cost total() const { return reject_cost + route_cost + resource_cost; }
        bool operator==(const Value&) const = default;
    };
    struct Summary {
        int missing_required = 0, overloaded_resources = 0, locally_invalid_requests = 0;
        int forbidden_requests = 0, fixed_mismatches = 0;
        std::optional<Value> value;
        // 全制約を満たすか。値が存在するだけでは実行可能とは限らない。O(1)。
        bool feasible() const {
            return !missing_required && !overloaded_resources && !locally_invalid_requests
                   && !forbidden_requests && !fixed_mismatches;
        }
    };
    struct Evaluation {
        struct LimitViolation { int request, resource; int64_t used, upper; };
        Summary summary;
        std::vector<int64_t> resource_load;
        std::vector<int> missing_required, overloaded_resources, forbidden_requests, fixed_mismatches;
        std::vector<LimitViolation> local_violations;
        // 全制約を満たすか。O(1)。
        bool feasible() const { return summary.feasible(); }
    };
    struct Limits {
        int64_t time_limit_us = 100'000;
        Clock::time_point deadline = Clock::time_point::max();
        int64_t max_iterations = -1;
    };
    struct Options { uint64_t seed = 0; };
    struct Statistics {
        int64_t iterations = 0, path_searches = 0, label_searches = 0, accepted = 0, elapsed_us = 0;
    };
    struct Result {
        Solution solution;
        Summary summary;
        StopReason stop_reason = StopReason::IterationLimit;
        Statistics statistics;
        // 全制約を満たすか。O(1)。
        bool feasible() const { return summary.feasible(); }
    };

    template<class Model = DefaultModel>
    class Solver {
        struct PathData {
            std::vector<std::pair<int, int64_t>> uses;
            Cost cost = 0;
            bool allowed = true, structure = true;
            int local_bad = 0;
            double local_excess = 0;
        };
        struct Quality {
            int missing = 0, bad = 0;
            double excess = 0;
            Value value;
        };
        struct Budget {
            Clock::time_point begin, end;
            explicit Budget(const Limits& l) : begin(Clock::now()), end(l.deadline) {
                assert(l.time_limit_us >= -1 && l.max_iterations >= -1);
                assert(l.time_limit_us >= 0 || l.max_iterations >= 0 || l.deadline != Clock::time_point::max());
                if (l.time_limit_us >= 0) end = std::min(end, begin + std::chrono::microseconds(l.time_limit_us));
            }
            bool expired() const { return Clock::now() >= end; }
        };
        struct Label {
            double distance;
            int vertex, parent, arc;
            bool alive = true;
        };
        const Problem* p_;
        std::optional<Model> model_;
        std::mt19937_64 random_;
        std::vector<std::pair<double,int>> heap_;
        std::vector<int> offset_, adjacency_, movable_, order_;
        std::vector<std::pair<int, int>> topology_;
        std::vector<Route> routes_;
        std::vector<PathData> data_;
        std::vector<int64_t> load_, base_, work_sum_;
        std::vector<int> work_seen_;
        std::vector<double> distance_, local_prices_;
        std::vector<int> limit_owner_;
        std::vector<int64_t> limit_upper_;
        std::vector<int> parent_, visited_;
        int order_pos_ = 0, missing_ = 0, overload_ = 0, local_bad_ = 0;
        double excess_ = 0, local_excess_ = 0, scale_ = 1, reward_scale_ = 1;
        Cost rejection_ = 0, route_cost_ = 0, resource_cost_ = 0;
        bool initialized_ = false, fixed_conflict_ = false, have_best_ = false;
        Result result_;
        Statistics statistics_;
        int64_t total_iterations_ = 0;

        static void check_cost(Cost c) { assert(c >= 0 && std::isfinite(static_cast<double>(c))); (void)c; }
        static double excess(int64_t x, int64_t upper) {
            return upper >= 0 && x > upper ? static_cast<double>(x - upper) / static_cast<double>(std::max<int64_t>(1, upper)) : 0;
        }
        uint64_t random_int(uint64_t n) { assert(n); return random_() % n; }
        double uniform() { return static_cast<double>(random_() >> 11) * 0x1.0p-53; }
        int64_t quantity(const ResourceUse& u, int k) const { return u.per_unit * p_->requests[k].demand + u.per_use; }
        Cost resource_value(int r, int64_t x) const {
            assert(x >= 0);
            int64_t cap = p_->resources[r].capacity;
            Cost v = model_->resource_cost(*p_, r, cap < 0 ? x : std::min(x, cap));
            check_cost(v); return v;
        }
        std::optional<Cost> vertex_cost(int k, int v) const {
            if (!p_->vertices[v].enabled) return {};
            auto c = model_->vertex_cost(*p_, k, v); if (c) check_cost(*c); return c;
        }
        std::optional<Cost> arc_cost(int k, int a) const {
            if (!p_->arcs[a].enabled) return {};
            auto c = model_->arc_cost(*p_, k, a); if (c) check_cost(*c); return c;
        }
        bool consume_vertex(int k, int v) const {
            const auto& q = p_->requests[k];
            return q.consume_endpoints || (v != q.source && v != q.target);
        }
        // 経路を正確に集計する。重複資源は一度合算してから上限を判定。
        PathData analyse(int k, const Route& route, std::vector<int64_t>& sum, std::vector<int>& seen) const {
            PathData d;
            const auto& q = p_->requests[k];
            if (!route.connected) { d.structure = route.arcs.empty(); d.allowed = d.structure; return d; }
            if (!q.active) { d.allowed = false; return d; }
            d.uses.reserve(std::min(route.arcs.size()+1,p_->resources.size()));
            auto use = [&](const std::vector<ResourceUse>& uses) {
                for (const auto& u : uses) {
                    int64_t x = quantity(u, k);
                    if (x && !sum[u.resource]) d.uses.push_back({u.resource, 0});
                    sum[u.resource] += x;
                }
            };
            auto vertex = [&](int v) {
                auto c = vertex_cost(k, v);
                if (c) d.cost += *c; else d.allowed = false;
                if (consume_vertex(k, v)) use(p_->vertices[v].uses);
                if (seen[v]) d.structure = false;
                seen[v] = 1;
            };
            int v = q.source;
            vertex(v); use(q.uses);
            for (int a : route.arcs) {
                assert(a >= 0 && a < static_cast<int>(p_->arcs.size()));
                const auto& e = p_->arcs[a];
                if (e.from != v) d.structure = false;
                auto c = arc_cost(k, a); if (c) d.cost += *c; else d.allowed = false;
                use(e.uses); v = e.to; vertex(v);
            }
            if (v != q.target) d.structure = false;
            d.cost *= q.cost_scale;
            for (const auto& lim : q.limits) if (sum[lim.resource] > lim.upper) {
                d.local_bad = 1; d.local_excess += excess(sum[lim.resource], lim.upper);
            }
            for (auto& [r, x] : d.uses) { x = sum[r]; sum[r] = 0; }
            seen[q.source] = 0;
            for (int a : route.arcs) seen[p_->arcs[a].to] = 0;
            d.allowed = d.allowed && d.structure;
            return d;
        }
        PathData analyse(int k, const Route& route) {
            return analyse(k, route, work_sum_, work_seen_);
        }
        bool better_value(const Value& a, const Value& b) const {
            if (p_->objective == Objective::PenaltyPlusCost) return a.total() < b.total();
            if (a.reject_cost != b.reject_cost) return a.reject_cost < b.reject_cost;
            return a.route_cost + a.resource_cost < b.route_cost + b.resource_cost;
        }
        Quality quality() const {
            return {missing_, overload_ + local_bad_, std::max(0.0, excess_ + local_excess_),
                    {rejection_, route_cost_, resource_cost_}};
        }
        bool better(const Quality& a, const Quality& b) const {
            if (a.missing != b.missing) return a.missing < b.missing;
            if ((!a.bad) != (!b.bad)) return !a.bad;
            if (a.bad && std::abs(a.excess - b.excess) > 1e-10) return a.excess < b.excess;
            return better_value(a.value, b.value);
        }
        bool accept(const Quality& a, const Quality& b) {
            if (better(a, b)) return true;
            double delta;
            if (a.missing != b.missing) delta = static_cast<double>(a.missing - b.missing) * 4;
            else if (a.bad != 0 || b.bad != 0) delta = (a.excess - b.excess) * 4;
            else if (p_->objective == Objective::PenaltyThenCost && a.value.reject_cost != b.value.reject_cost)
                delta = (static_cast<double>(a.value.reject_cost) - static_cast<double>(b.value.reject_cost)) / reward_scale_;
            else delta = (static_cast<double>(a.value.route_cost) + static_cast<double>(a.value.resource_cost)
                          - static_cast<double>(b.value.route_cost) - static_cast<double>(b.value.resource_cost)
                          + (p_->objective == Objective::PenaltyPlusCost ? static_cast<double>(a.value.reject_cost) - static_cast<double>(b.value.reject_cost) : 0)) / scale_;
            // 反復数による短い冷却周期。resumeの区切りではリセットしない。
            double phase = static_cast<double>(total_iterations_ % (32 * std::max<size_t>(1, movable_.size())))
                           / static_cast<double>(32 * std::max<size_t>(1, movable_.size()));
            double temperature = 0.25 * std::pow(0.04, phase);
            return delta <= 0 || uniform() < std::exp(-delta / temperature);
        }
        // 1経路の着脱。使用量と目的値を差分更新し、資源費用は合算後に計算。
        void change(int k, int sign) {
            const auto& q = p_->requests[k];
            if (!q.active) return;
            if (!routes_[k].connected) {
                if (q.required) missing_ += sign;
                else if (sign > 0) rejection_ += q.reject_cost; else rejection_ -= q.reject_cost;
                return;
            }
            const auto& d = data_[k];
            if (sign > 0) route_cost_ += d.cost; else route_cost_ -= d.cost;
            local_bad_ += sign * d.local_bad; local_excess_ += sign * d.local_excess;
            for (auto [r, x] : d.uses) {
                int64_t cap = p_->resources[r].capacity;
                overload_ -= cap >= 0 && load_[r] > cap;
                excess_ -= excess(load_[r], cap);
                resource_cost_ -= resource_value(r, load_[r]);
                load_[r] += sign * x;
                resource_cost_ += resource_value(r, load_[r]);
                excess_ += excess(load_[r], cap);
                overload_ += cap >= 0 && load_[r] > cap;
            }
        }
        void replace_route(int k, Route route, PathData data) {
            change(k, -1); routes_[k] = std::move(route); data_[k] = std::move(data); change(k, 1);
        }
        bool fits_capacity(const PathData& d) const {
            for (auto [r, x] : d.uses) {
                int64_t c = p_->resources[r].capacity;
                if (c >= 0 && load_[r] + x > c) return false;
            }
            return true;
        }
        void remember() {
            if (missing_ || overload_ || local_bad_) return;
            Value v{rejection_, route_cost_, resource_cost_};
            if (have_best_ && !better_value(v, *result_.summary.value)) return;
            if constexpr (std::is_floating_point_v<Cost>) {
                auto e = evaluate(Solution{routes_});
                assert(e.feasible() && e.summary.value);
                v = *e.summary.value;
                if (have_best_ && !better_value(v, *result_.summary.value)) return;
            }
            result_.solution.routes = routes_;
            result_.summary = {}; result_.summary.value = v; have_best_ = true;
        }
        // 入力更新後、禁止された旧経路を除き、固定と背景から状態を作り直す。
        void initialize(Solution initial) {
            have_best_ = false; fixed_conflict_ = false;
            routes_ = std::move(initial.routes); routes_.resize(p_->requests.size());
            data_.assign(routes_.size(), {}); movable_.clear();
            load_.resize(p_->resources.size());
            rejection_ = route_cost_ = resource_cost_ = 0;
            missing_ = overload_ = local_bad_ = 0; excess_ = local_excess_ = 0;
            for (int r = 0; r < static_cast<int>(load_.size()); ++r) {
                load_[r] = p_->resources[r].background_load;
                resource_cost_ += resource_value(r, load_[r]);
                overload_ += p_->resources[r].capacity >= 0 && load_[r] > p_->resources[r].capacity;
                excess_ += excess(load_[r], p_->resources[r].capacity);
            }
            for (int k = 0; k < static_cast<int>(routes_.size()); ++k) {
                const auto& q = p_->requests[k];
                if (q.fixed_route) routes_[k] = *q.fixed_route;
                if (!q.active) routes_[k] = {};
                data_[k] = analyse(k, routes_[k]);
                if (q.fixed_route) {
                    if (!data_[k].allowed || data_[k].local_bad || (q.active && q.required && !routes_[k].connected)
                        || (!q.active && *q.fixed_route != Route{})) fixed_conflict_ = true;
                    change(k, 1);
                } else {
                    if (!data_[k].allowed) { routes_[k] = {}; data_[k] = {}; }
                    if (q.active) movable_.push_back(k);
                }
            }
            base_ = load_;
            if (overload_) fixed_conflict_ = true;
            for (int k : movable_) change(k, 1);
            if (!fixed_conflict_) remember();
            order_ = movable_;
            std::shuffle(order_.begin(), order_.end(), random_);
            std::stable_sort(order_.begin(), order_.end(), [&](int a, int b) {
                const auto& x = p_->requests[a]; const auto& y = p_->requests[b];
                if (x.required != y.required) return x.required;
                return !x.required && x.reject_cost > y.reject_cost;
            });
            order_pos_ = 0; initialized_ = true;
        }
        // 1回の経路探索中は価格を固定する。厳守モードは単独でも超過する遷移を除外。
        [[gnu::always_inline]] inline double use_weight(int k, const std::vector<ResourceUse>& uses, bool hard, double local_price) const {
            double weight = 0;
            for (const auto& u : uses) {
                int r = u.resource; int64_t x = quantity(u, k), cap = p_->resources[r].capacity;
                if (!x) continue;
                if (cap >= 0 && (x > cap - base_[r] || (hard && x > cap - load_[r])))
                    return std::numeric_limits<double>::infinity();
                weight += static_cast<double>(resource_value(r, load_[r] + x) - resource_value(r, load_[r]));
                if (cap >= 0 && (load_[r] + x > cap || (p_->objective == Objective::PenaltyThenCost && total_iterations_ % 2 == 0))) {
                    double unit = static_cast<double>(x) / static_cast<double>(std::max<int64_t>(1, cap));
                    double fill = static_cast<double>(load_[r] + x) / static_cast<double>(std::max<int64_t>(1, cap));
                    weight += scale_ * (3 * std::max(0.0, fill - 1)
                              + (p_->objective == Objective::PenaltyThenCost && total_iterations_ % 2 == 0 ? 0.02 * fill * unit : 0));
                }
                if (limit_owner_[r] == k) {
                    int64_t upper = limit_upper_[r];
                    if (x > upper) return std::numeric_limits<double>::infinity();
                    weight += scale_ * local_price * local_prices_[r] * static_cast<double>(x)
                              / static_cast<double>(std::max<int64_t>(1, upper));
                }
            }
            return weight;
        }
        [[gnu::always_inline]] inline double edge_weight(int k, int a, bool hard, double local_price, uint64_t salt) const {
            const auto& e = p_->arcs[a];
            auto ac = arc_cost(k, a), vc = vertex_cost(k, e.to);
            if (!ac || !vc) return std::numeric_limits<double>::infinity();
            double w = static_cast<double>((*ac + *vc) * p_->requests[k].cost_scale);
            w += use_weight(k, e.uses, hard, local_price);
            if (consume_vertex(k, e.to)) w += use_weight(k, p_->vertices[e.to].uses, hard, local_price);
            if (salt) {
                uint64_t h = (static_cast<uint64_t>(a) + salt) * 0x9e3779b97f4a7c15ULL;
                w *= 0.8 + 0.4 * static_cast<double>(h >> 40) / 16777216.0;
            }
            return w;
        }
        std::optional<Route> shortest(int k, bool hard, Budget& budget, double local_price, uint64_t salt) {
            ++statistics_.path_searches;
            const auto& q = p_->requests[k];
            if (!vertex_cost(k, q.source)) return {};
            std::fill(distance_.begin(), distance_.end(), std::numeric_limits<double>::infinity());
            std::fill(visited_.begin(), visited_.end(), 0);
            using Item = std::pair<double, int>;
            auto& heap = heap_; heap.clear();
            auto push = [&](Item item) { heap.push_back(item); std::push_heap(heap.begin(), heap.end(), std::greater<Item>()); };
            distance_[q.source] = 0; parent_[q.source] = -1; push({0, q.source});
            int work = 0;
            while (!heap.empty()) {
                std::pop_heap(heap.begin(), heap.end(), std::greater<Item>());
                auto [d, v] = heap.back(); heap.pop_back();
                if ((++work & 127) == 0 && budget.expired()) return {};
                if (visited_[v]) continue;
                visited_[v] = 1;
                if (v == q.target) {
                    Route path{true, {}};
                    for (int x = v; x != q.source; x = p_->arcs[parent_[x]].from) path.arcs.push_back(parent_[x]);
                    std::reverse(path.arcs.begin(), path.arcs.end()); return path;
                }
                for (int z = offset_[v]; z < offset_[v + 1]; ++z) {
                    if ((++work & 127) == 0 && budget.expired()) return {};
                    int a = adjacency_[z], t = p_->arcs[a].to;
                    if (visited_[t]) continue;
                    double nd = d + edge_weight(k, a, hard, local_price, salt);
                    if (nd < distance_[t]) { distance_[t] = nd; parent_[t] = a; push({nd, t}); }
                }
            }
            return {};
        }
        // 上限違反した資源と要求別資源だけを追跡。1頂点4ラベル。総生成数が32n以上になると次の頂点展開前に打切り。
        // 消費量による枝刈りと単純路制約を併用する近似探索であり、可解性の完全判定ではない。
        std::optional<std::pair<Route, PathData>> labelled(int k, bool hard, const PathData& failed, Budget& budget, uint64_t salt) {
            ++statistics_.label_searches;
            const auto& q = p_->requests[k];
            std::vector<LocalLimit> dimensions = q.limits;
            for (auto [r, x] : failed.uses) {
                int64_t cap = p_->resources[r].capacity;
                if (cap < 0) continue;
                int64_t upper = cap - (hard ? load_[r] : base_[r]);
                if (x <= upper) continue;
                auto it = std::find_if(dimensions.begin(), dimensions.end(), [&](const auto& z) { return z.resource == r; });
                if (it == dimensions.end()) dimensions.push_back({r, upper}); else it->upper = std::min(it->upper, upper);
            }
            for (auto& dim : dimensions) {
                int64_t cap = p_->resources[dim.resource].capacity;
                if (cap >= 0) dim.upper = std::min(dim.upper, cap - (hard ? load_[dim.resource] : base_[dim.resource]));
            }
            if (dimensions.empty()) return {};
            auto add = [&](std::vector<int64_t>& used, const std::vector<ResourceUse>& uses) {
                for (const auto& u : uses) for (size_t j = 0; j < dimensions.size(); ++j)
                    if (u.resource == dimensions[j].resource) used[j] += quantity(u, k);
            };
            auto valid = [&](const std::vector<int64_t>& used) {
                for (size_t j = 0; j < used.size(); ++j) if (used[j] > dimensions[j].upper) return false;
                return true;
            };
            std::vector<int64_t> used(dimensions.size());
            std::vector<Label> labels;
            std::vector<std::vector<int>> at(p_->vertices.size());
            std::vector<int64_t> amounts(dimensions.size()); add(amounts, q.uses);
            if (consume_vertex(k, q.source)) add(amounts, p_->vertices[q.source].uses);
            if (!valid(amounts)) return {};
            labels.push_back({0, q.source, -1, -1, true}); at[q.source].push_back(0);
            using Item = std::pair<double, int>;
            std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap; heap.push({0, 0});
            size_t max_labels = p_->vertices.size() * 32;
            int work = 0;
            while (!heap.empty() && labels.size() < max_labels) {
                auto [distance, id] = heap.top(); heap.pop();
                if ((++work & 31) == 0 && budget.expired()) return {};
                if (!labels[id].alive) continue;
                int v = labels[id].vertex;
                if (v == q.target) {
                    Route path{true, {}};
                    for (int x = id; labels[x].parent >= 0; x = labels[x].parent) path.arcs.push_back(labels[x].arc);
                    std::reverse(path.arcs.begin(), path.arcs.end());
                    auto pd = analyse(k, path);
                    if (pd.allowed && !pd.local_bad && (!hard || fits_capacity(pd))) return std::pair{std::move(path), std::move(pd)};
                    continue;
                }
                for (int z = offset_[v]; z < offset_[v + 1]; ++z) {
                    if ((++work & 31) == 0 && budget.expired()) return {};
                    int a = adjacency_[z], t = p_->arcs[a].to;
                    // 同じ頂点を2回含めない。無効化済みの親ラベルも経路復元用に保持。
                    bool cycle = false;
                    for (int x = id; x >= 0; x = labels[x].parent) if (labels[x].vertex == t) { cycle = true; break; }
                    if (cycle) continue;
                    double nd = distance + edge_weight(k, a, hard, 0, salt);
                    if (!std::isfinite(nd)) continue;
                    std::copy_n(amounts.begin() + static_cast<size_t>(id) * used.size(), used.size(), used.begin());
                    add(used, p_->arcs[a].uses);
                    if (consume_vertex(k, t)) add(used, p_->vertices[t].uses);
                    if (!valid(used)) continue;
                    auto dominates = [&](int old_id) {
                        const auto& old = labels[old_id];
                        if (old.distance > nd) return false;
                        for (size_t j = 0; j < used.size(); ++j) if (amounts[static_cast<size_t>(old_id) * used.size() + j] > used[j]) return false;
                        return true;
                    };
                    auto& bucket = at[t];
                    if (std::any_of(bucket.begin(), bucket.end(), dominates)) continue;
                    if (bucket.size() >= 4) {
                        auto worst = std::max_element(bucket.begin(), bucket.end(), [&](int x, int y) { return labels[x].distance < labels[y].distance; });
                        if (labels[*worst].distance <= nd) continue;
                        labels[*worst].alive = false; bucket.erase(worst);
                    }
                    int next = static_cast<int>(labels.size());
                    amounts.insert(amounts.end(), used.begin(), used.end());
                    labels.push_back({nd, t, id, a, true}); bucket.push_back(next); heap.push({nd, next});
                }
            }
            return {};
        }
        std::optional<std::pair<Route, PathData>> candidate(int k, bool hard, Budget& budget, bool noisy) {
            const auto& limits = p_->requests[k].limits;
            for (const auto& lim : limits) {
                local_prices_[lim.resource] = 1; limit_owner_[lim.resource] = k; limit_upper_[lim.resource] = lim.upper;
            }
            double price = noisy ? uniform() * 2 : 0;
            uint64_t salt = noisy ? random_() : 0;
            auto path = shortest(k, hard, budget, price, salt);
            if (!path || budget.expired()) return {};
            auto pd = analyse(k, *path);
            if (pd.allowed && !pd.local_bad && (!hard || fits_capacity(pd))) return std::pair{std::move(*path), std::move(pd)};
            for (int retry = 0; pd.local_bad && retry < 3 && !budget.expired(); ++retry) {
                // 違反した上限だけを重くし、余裕のある別資源への迂回を妨げない。
                for (const auto& lim : limits) for (auto [r, x] : pd.uses)
                    if (r == lim.resource && x > lim.upper) local_prices_[r] *= 4;
                path = shortest(k, hard, budget, 1, salt);
                if (!path || budget.expired()) return {};
                pd = analyse(k,*path);
                if (pd.allowed && !pd.local_bad && (!hard || fits_capacity(pd))) return std::pair{std::move(*path),std::move(pd)};
            }
            auto labelled_path = labelled(k, hard, pd, budget, salt);
            if (!labelled_path || budget.expired()) return {};
            return labelled_path;
        }
        void construct(int k, Budget& budget, bool noisy) {
            Quality before = quality();
            auto path = candidate(k, true, budget, noisy);
            if (!path && !budget.expired() && p_->requests[k].required)
                path = candidate(k, false, budget, noisy);
            if (!path) return;
            replace_route(k, std::move(path->first), std::move(path->second));
            if (!better(quality(), before)) replace_route(k, {}, {});
        }
        std::vector<int> select_group() {
            int first = movable_[random_int(movable_.size())];
            // 未接続要求と混雑経路を優先して選ぶ。全要求を毎回走査しない。
            for (int j = 0; j < 6; ++j) {
                int k = movable_[random_int(movable_.size())];
                bool bad = !routes_[k].connected && p_->requests[k].required;
                for (auto [r, x] : data_[k].uses) { (void)x; bad |= p_->resources[r].capacity >= 0 && load_[r] > p_->resources[r].capacity; }
                if (bad) { first = k; break; }
            }
            std::vector<int> group{first};
            if (random_int(4) != 0) return group;
            int count = 2 + static_cast<int>(random_int(5));
            std::vector<int> related;
            {
                std::vector<char> touched(load_.size());
                for (auto [r, x] : data_[first].uses) { (void)x; touched[r] = 1; }
                if (!routes_[first].connected) {
                    // 未接続要求では端点周辺の資源を共有する経路も候補とする。
                    for (int v : {p_->requests[first].source, p_->requests[first].target}) {
                        for (const auto& u : p_->vertices[v].uses) touched[u.resource] = 1;
                        for (int z = offset_[v]; z < offset_[v + 1]; ++z)
                            for (const auto& u : p_->arcs[adjacency_[z]].uses) touched[u.resource] = 1;
                    }
                }
                for (int k : movable_) if (k != first) for (auto [r, x] : data_[k].uses) {
                    (void)x; if (touched[r]) { related.push_back(k); break; }
                }
                std::shuffle(related.begin(), related.end(), random_);
            }
            for (int k : related) { if (static_cast<int>(group.size()) >= count) break; group.push_back(k); }
            for (int j = 0; static_cast<int>(group.size()) < count && j < count * 4; ++j) {
                int k = movable_[random_int(movable_.size())];
                if (std::find(group.begin(), group.end(), k) == group.end()) group.push_back(k);
            }
            std::shuffle(group.begin(), group.end(), random_);
            std::stable_sort(group.begin(), group.end(), [&](int a, int b) { return p_->requests[a].required && !p_->requests[b].required; });
            return group;
        }
        void trial(const std::vector<int>& group, Budget& budget, bool initial) {
            if(budget.expired())return;
            Quality old = quality();
            std::vector<Route> saved_routes; std::vector<PathData> saved_data;
            for (int k : group) {
                change(k, -1); saved_routes.push_back(std::move(routes_[k])); saved_data.push_back(std::move(data_[k]));
                routes_[k] = {}; data_[k] = {}; change(k, 1);
            }
            for (int k : group) { if (budget.expired()) break; construct(k, budget, !initial); }
            bool accepted = !budget.expired() && (initial || accept(quality(), old));
            if (!accepted) for (size_t j = 0; j < group.size(); ++j)
                replace_route(group[j], std::move(saved_routes[j]), std::move(saved_data[j]));
            else { ++statistics_.accepted; remember(); }
        }
        const Result& run(const Limits& limits, Budget& budget) {
            if (fixed_conflict_) result_.stop_reason = StopReason::FixedConflict;
            else if (movable_.empty()) result_.stop_reason = StopReason::NoMovableRequests;
            else {
                while (!budget.expired() && (limits.max_iterations < 0 || statistics_.iterations < limits.max_iterations)) {
                    bool initial = order_pos_ < static_cast<int>(order_.size());
                    if (initial) trial({order_[order_pos_++]}, budget, true);
                    else trial(select_group(), budget, false);
                    ++statistics_.iterations; ++total_iterations_;
                }
                result_.stop_reason = budget.expired() ? StopReason::TimeLimit : StopReason::IterationLimit;
            }
            if (!have_best_) {
                result_.solution.routes = routes_;
                result_.summary = evaluate(result_.solution).summary;
            }
            statistics_.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - budget.begin).count();
            result_.statistics = statistics_; return result_;
        }
    public:
        // Problemを参照し、Modelを値で所有する。O(n+m+r+k+u)。
        explicit Solver(const Problem& problem, Model model = {}) : p_(&problem), model_(std::in_place, std::move(model)) { reset(problem); }
        // 別のProblemへ結び直す。Modelは保持し、探索状態は破棄。O(n+m+r+k+u)。
        void reset(const Problem& problem) {
            p_ = &problem; initialized_ = have_best_ = fixed_conflict_ = false; result_ = {};
            int n = static_cast<int>(p_->vertices.size());
            offset_.assign(static_cast<size_t>(n) + 1, 0); topology_.clear();
            for (const auto& e : p_->arcs) {
                assert(e.from >= 0 && e.from < n && e.to >= 0 && e.to < n);
                ++offset_[e.from + 1];
            }
#ifndef NDEBUG
            for(const auto& e:p_->arcs)topology_.push_back({e.from,e.to});
#endif
            std::partial_sum(offset_.begin(), offset_.end(), offset_.begin());
            adjacency_.resize(p_->arcs.size()); auto cursor = offset_;
            for (int a = 0; a < static_cast<int>(p_->arcs.size()); ++a) adjacency_[cursor[p_->arcs[a].from]++] = a;
            distance_.resize(n); parent_.resize(n); visited_.resize(n);
            refresh();
        }
        // Modelを置換し、同じトポロジーの旧経路を再評価。O(n+m+r+k+u+h)。
        void set_model(Model model) { model_.emplace(std::move(model)); refresh(); }
        // 固定分から新規探索。O(n+r+k+u+h+探索)。初期処理もlimitsの時間に含む。
        const Result& solve(const Limits& limits, const Options& options = {}) {
            Budget budget(limits); statistics_ = {}; random_.seed(options.seed); total_iterations_ = 0;
            initialize({}); return run(limits, budget);
        }
        // 外部解から改善。O(n+r+k+u+h+探索)。結果自身を引数にしてもよい。
        const Result& improve(const Solution& initial, const Limits& limits, const Options& options = {}) {
            Budget budget(limits);
            assert(initial.routes.size() <= p_->requests.size());
            statistics_ = {}; random_.seed(options.seed); total_iterations_ = 0;
            initialize(initial);
            return run(limits, budget);
        }
        // 同じ状態で継続。O(探索)。最良実行可能解がない場合は返却前にO(n+r+k+h)で全評価。
        const Result& resume(const Limits& limits) {
            assert(initialized_); Budget budget(limits); statistics_ = {}; return run(limits, budget);
        }
        // 最良解を基に属性・資源・要求の変更を反映。O(n+m+r+k+u+h)。トポロジー変更はresetが必要。
        void refresh() {
            assert(p_->vertices.size() == distance_.size() && p_->arcs.size() == topology_.size());
            for (size_t a = 0; a < topology_.size(); ++a) {
                assert(topology_[a] == std::make_pair(p_->arcs[a].from, p_->arcs[a].to));
            }
            auto check_uses = [&](const std::vector<ResourceUse>& uses) {
                for (const auto& u : uses) {
                    assert(u.resource >= 0 && u.resource < static_cast<int>(p_->resources.size()) && u.per_unit >= 0 && u.per_use >= 0);
                    (void)u;
                }
            };
            double cost = 0; reward_scale_ = 1;
            for (const auto& r : p_->resources) { assert(r.capacity >= -1 && r.background_load >= 0); (void)r; }
            for (const auto& v : p_->vertices) { check_cost(v.cost); check_uses(v.uses); }
            for (const auto& a : p_->arcs) { check_cost(a.cost); check_uses(a.uses); cost += static_cast<double>(a.cost); }
            for (const auto& q : p_->requests) {
                assert(q.source >= 0 && q.source < static_cast<int>(p_->vertices.size()));
                assert(q.target >= 0 && q.target < static_cast<int>(p_->vertices.size()) && q.demand >= 0);
                check_cost(q.reject_cost); check_cost(q.cost_scale); check_uses(q.uses);
                reward_scale_ = std::max(reward_scale_, static_cast<double>(q.reject_cost));
                for (const auto& lim : q.limits) {
                    (void)lim;
                    assert(lim.resource >= 0 && lim.resource < static_cast<int>(p_->resources.size()) && lim.upper >= 0);
                    assert(std::count_if(q.limits.begin(),q.limits.end(),[&](const auto& other){return other.resource==lim.resource;})==1);
                }
            }
            scale_ = std::max(1.0, cost / static_cast<double>(std::max<size_t>(1, p_->arcs.size()))
                             * std::sqrt(static_cast<double>(std::max<size_t>(1, p_->vertices.size()))));
            limit_owner_.assign(p_->resources.size(), -1); limit_upper_.resize(p_->resources.size());
            // 価格は候補生成時に設定。集計用の作業領域はanalyseから戻ると常に0。
            local_prices_.resize(p_->resources.size());
            work_sum_.resize(p_->resources.size()); work_seen_.resize(p_->vertices.size());
            if (initialized_) initialize(have_best_ ? result_.solution : Solution{routes_});
        }
        // キャッシュを使わず全経路を再評価する。O(n+r+k+h)。構造不正はassert。
        Evaluation evaluate(const Solution& solution) const {
            assert(solution.routes.size() == p_->requests.size());
            Evaluation out; Value value;
            out.resource_load.reserve(p_->resources.size());
            for (const auto& r : p_->resources) out.resource_load.push_back(r.background_load);
            std::vector<int64_t> sum(p_->resources.size()); std::vector<int> seen(p_->vertices.size());
            for (int k = 0; k < static_cast<int>(solution.routes.size()); ++k) {
                const auto& q = p_->requests[k]; const auto& route = solution.routes[k];
                auto d = analyse(k, route, sum, seen);
                assert(d.structure || (q.fixed_route && route == *q.fixed_route));
                if (q.fixed_route && route != *q.fixed_route) out.fixed_mismatches.push_back(k);
                if (!d.allowed) out.forbidden_requests.push_back(k);
                if (!q.active) continue;
                if (!route.connected) {
                    if (q.required) out.missing_required.push_back(k); else value.reject_cost += q.reject_cost;
                } else {
                    value.route_cost += d.cost;
                    for (auto [r, x] : d.uses) { out.resource_load[r] += x; sum[r] = x; }
                    for (const auto& lim : q.limits) if (sum[lim.resource] > lim.upper)
                        out.local_violations.push_back({k, lim.resource, sum[lim.resource], lim.upper});
                    for (auto [r, x] : d.uses) { (void)x; sum[r] = 0; }
                    out.summary.locally_invalid_requests += d.local_bad;
                }
            }
            for (int r = 0; r < static_cast<int>(p_->resources.size()); ++r) {
                int64_t cap = p_->resources[r].capacity;
                if (cap >= 0 && out.resource_load[r] > cap) out.overloaded_resources.push_back(r);
            }
            if (out.overloaded_resources.empty() && out.forbidden_requests.empty()) {
                for (int r = 0; r < static_cast<int>(p_->resources.size()); ++r) value.resource_cost += resource_value(r, out.resource_load[r]);
                out.summary.value = value;
            }
            out.summary.missing_required = static_cast<int>(out.missing_required.size());
            out.summary.overloaded_resources = static_cast<int>(out.overloaded_resources.size());
            out.summary.forbidden_requests = static_cast<int>(out.forbidden_requests.size());
            out.summary.fixed_mismatches = static_cast<int>(out.fixed_mismatches.size());
            return out;
        }
    };
    // Modelを推論してSolverを生成する。O(n+m+r+k+u)。
    template<class Model = DefaultModel>
    static Solver<Model> make_solver(const Problem& problem, Model model = {}) { return Solver<Model>(problem, std::move(model)); }
};

#if __INCLUDE_LEVEL__ == 0

int main(int argc, char** argv) {
    using R = CapacitatedRouter<int64_t>;
    int checks = 0;
    auto check = [&](bool ok, const char* what) {
        ++checks;
        if (!ok) { std::cerr << "FAILED: " << what << '\n'; std::abort(); }
    };
    R::Limits iterations; iterations.time_limit_us = -1; iterations.max_iterations = 100;
    R::Limits zero; zero.time_limit_us = 0;
    R::Options opt; opt.seed = 47;
    struct Model : R::DefaultModel {
        int mode = 0, forbidden_arc = -1, forbidden_vertex = -1, special_request = -1;
        std::optional<int64_t> arc_cost(const R::Problem& p, int k, int a) const {
            if (a == forbidden_arc && (special_request < 0 || k == special_request)) return {};
            return p.arcs[a].cost + (k == special_request ? 2 : 0);
        }
        std::optional<int64_t> vertex_cost(const R::Problem& p, int, int v) const {
            if (v == forbidden_vertex) return {};
            return p.vertices[v].cost;
        }
        int64_t resource_cost(const R::Problem& p, int r, int64_t load) const {
            assert(load >= 0 && (p.resources[r].capacity < 0 || load <= p.resources[r].capacity));
            (void)p; (void)r;
            if (mode == 1) return load * load;
            if (mode == 2) return load ? 13 : 0;
            return 0;
        }
    };
    // 本体の資源集計や差分処理を利用しない参照評価器。
    auto naive = [&](const R::Problem& p, const auto& model, const R::Solution& sol) {
        R::Evaluation out; R::Value val;
        for (auto r : p.resources) out.resource_load.push_back(r.background_load);
        for (int k = 0; k < static_cast<int>(p.requests.size()); ++k) {
            const auto& req = p.requests[k]; const auto& path = sol.routes[k];
            if (req.fixed_route && path != *req.fixed_route) out.fixed_mismatches.push_back(k);
            if (!req.active) { if (path.connected || !path.arcs.empty()) out.forbidden_requests.push_back(k); continue; }
            if (!path.connected) {
                if (!path.arcs.empty()) out.forbidden_requests.push_back(k);
                if (req.required) out.missing_required.push_back(k); else val.reject_cost += req.reject_cost;
                continue;
            }
            std::vector<int64_t> local(p.resources.size());
            std::vector<int> vertices{req.source};
            bool allowed = true;
            int last = req.source;
            int64_t path_cost = 0;
            auto use = [&](const auto& uses) { for (auto u : uses) local[u.resource] += u.per_unit * req.demand + u.per_use; };
            for (int a : path.arcs) {
                const auto& arc = p.arcs[a];
                if (arc.from != last) allowed = false;
                last = arc.to; vertices.push_back(last);
                auto cost = model.arc_cost(p, k, a);
                if (!arc.enabled || !cost) allowed = false;
                if (cost) path_cost += *cost;
                use(arc.uses);
            }
            if (last != req.target) allowed = false;
            std::set<int> seen;
            for (int v : vertices) {
                if (!seen.insert(v).second) allowed = false;
                auto cost = model.vertex_cost(p, k, v);
                if (!p.vertices[v].enabled || !cost) allowed = false;
                if (cost) path_cost += *cost;
                if (req.consume_endpoints || (v != req.source && v != req.target)) use(p.vertices[v].uses);
            }
            use(req.uses); val.route_cost += path_cost * req.cost_scale;
            if (!allowed) out.forbidden_requests.push_back(k);
            bool invalid = false;
            for (auto lim : req.limits) if (local[lim.resource] > lim.upper) {
                out.local_violations.push_back({k, lim.resource, local[lim.resource], lim.upper}); invalid = true;
            }
            out.summary.locally_invalid_requests += invalid;
            for (size_t r = 0; r < local.size(); ++r) out.resource_load[r] += local[r];
        }
        for (int r = 0; r < static_cast<int>(p.resources.size()); ++r) {
            if (p.resources[r].capacity >= 0 && out.resource_load[r] > p.resources[r].capacity) out.overloaded_resources.push_back(r);
        }
        if (out.overloaded_resources.empty() && out.forbidden_requests.empty()) {
            for (int r = 0; r < static_cast<int>(p.resources.size()); ++r) val.resource_cost += model.resource_cost(p, r, out.resource_load[r]);
            out.summary.value = val;
        }
        out.summary.missing_required = static_cast<int>(out.missing_required.size());
        out.summary.overloaded_resources = static_cast<int>(out.overloaded_resources.size());
        out.summary.forbidden_requests = static_cast<int>(out.forbidden_requests.size());
        out.summary.fixed_mismatches = static_cast<int>(out.fixed_mismatches.size());
        return out;
    };
    auto same = [&](const R::Evaluation& a, const R::Evaluation& b) {
        check(a.summary.value == b.summary.value, "objective vs naive");
        check(a.summary.feasible() == b.summary.feasible(), "feasible vs naive");
        check(a.summary.locally_invalid_requests == b.summary.locally_invalid_requests, "local invalid request count");
        check(a.resource_load == b.resource_load, "resource loads vs naive");
        check(a.missing_required == b.missing_required, "required diagnostics");
        check(a.overloaded_resources == b.overloaded_resources, "shared diagnostics");
        check(a.forbidden_requests == b.forbidden_requests, "forbidden diagnostics");
        check(a.fixed_mismatches == b.fixed_mismatches, "fixed diagnostics");
        check(a.local_violations.size() == b.local_violations.size(), "local diagnostics count");
        for (size_t j = 0; j < a.local_violations.size(); ++j) {
            auto x = a.local_violations[j], y = b.local_violations[j];
            check(std::tie(x.request,x.resource,x.used,x.upper) == std::tie(y.request,y.resource,y.used,y.upper), "local diagnostic values");
        }
    };
    auto verify = [&](const auto& solver, const R::Problem& p, const auto& model, const R::Result& result) {
        const auto a = solver.evaluate(result.solution); const auto b = naive(p, model, result.solution);
        same(a,b);
        check(result.summary.feasible() == a.feasible(), "result feasibility is current");
        check(result.summary.value == a.summary.value, "result objective is current");
    };
    if (argc > 1) {
        R::Problem p(2); p.add_arc(0,1); p.add_request(0,1);
        if (std::string(argv[1]) == "negative") p.arcs[0].cost = -1;
        if (std::string(argv[1]) == "duplicate") { int r = p.add_resource(); p.requests[0].limits = {{r,1},{r,2}}; }
        auto s = R::make_solver(p);
        if (std::string(argv[1]) == "topology") { p.arcs[0].to=0; s.refresh(); }
        if (std::string(argv[1]) == "resume") s.resume(zero);
        return 0;
    }
    {
        R::Problem p(4);
        auto a = p.add_undirected_edge(0,1,2,3);
        check(a == std::pair(0,1), "undirected IDs");
        check(p.arcs[0].uses[0].resource == p.arcs[1].uses[0].resource, "undirected shared capacity");
        int v = p.add_vertex_capacity(1,2); check(v==1, "vertex resource ID");
        int b=p.add_arc(1,3,1,2); check(b==2, "arc ID");
        p.add_request(0,3,2);
        auto s=R::make_solver(p); auto out=s.solve(iterations,opt);
        check(out.feasible(), "basic solve"); verify(s,p,R::DefaultModel{},out);
        check(out.solution.routes[0].arcs == std::vector<int>({0,2}), "edge IDs not vertices");
    }
    {
        R::Problem p(3); int r=p.add_resource(100,3);
        p.vertices[0].cost=1; p.vertices[1].cost=2; p.vertices[2].cost=3;
        p.vertices[0].uses={{r,0,2}}; p.vertices[1].uses={{r,0,3}}; p.vertices[2].uses={{r,0,5}};
        int a=p.add_arc(0,1,2), b=p.add_arc(1,2,4);
        p.arcs[a].uses={{r,2,1}}; p.arcs[b].uses={{r,0,7}};
        p.add_request(0,2,2); p.requests[0].uses={{r,1,4}}; p.requests[0].cost_scale=3;
        p.add_request(0,2); p.requests[1].required=false; p.requests[1].reject_cost=11;
        R::Solution sol{{{true,{a,b}}, {false,{}}}};
        Model m; m.mode=1; auto s=R::make_solver(p,m);
        auto e=s.evaluate(sol); same(e,naive(p,m,sol));
        check(e.resource_load[r]==31 && e.summary.value->route_cost==36 && e.summary.value->resource_cost==961, "combined consumption hand calculation");
        p.requests[0].limits={{r,28}}; p.resources[r].background_load=50; s.refresh();
        check(s.evaluate(sol).feasible(), "background excluded from local budget");
        p.requests[0].limits[0].upper=20; s.refresh(); e=s.evaluate(sol);
        check(!e.feasible() && e.summary.value && e.local_violations[0].used==28, "local violation has defined objective");
        p.requests[0].consume_endpoints=false; p.requests[0].limits[0].upper=21; s.refresh(); e=s.evaluate(sol);
        check(e.feasible() && e.resource_load[r]==71 && e.summary.value->route_cost==36, "endpoint exemption only affects consumption");
        p.resources[r].capacity=20; s.refresh(); e=s.evaluate(sol);
        check(!e.summary.value && e.overloaded_resources==std::vector<int>{r}, "over capacity objective undefined and callback in domain");
        p.resources[r].capacity=100; p.vertices[1].enabled=false; s.refresh();
        check(!s.evaluate(sol).summary.value, "disabled vertex enforced with custom Model");
        p.vertices[1].enabled=true; m.forbidden_arc=a; s.set_model(m);
        check(!s.evaluate(sol).summary.value, "Model prohibition");
        m.forbidden_arc=-1; m.forbidden_vertex=0; s.set_model(m);
        check(!s.evaluate(sol).feasible(), "endpoint exemption does not exempt prohibition");
    }
    {
        R::Problem p(1); int r=p.add_resource(3); p.vertices[0].cost=7; p.vertices[0].uses={{r,0,2}};
        p.add_request(0,0,0); p.requests[0].uses={{r,5,1}};
        Model m; m.mode=2; auto s=R::make_solver(p,m); auto out=s.solve(iterations,opt);
        check(out.feasible() && out.solution.routes[0].connected && out.solution.routes[0].arcs.empty(), "zero length connected route");
        check(out.summary.value->route_cost==7 && out.summary.value->resource_cost==13, "zero length counted once");
        verify(s,p,m,out);
        p.requests[0].consume_endpoints=false; s.refresh(); check(s.evaluate(out.solution).resource_load[r]==1, "zero length exemption");
        p.requests[0].uses.clear(); s.refresh(); check(s.evaluate(out.solution).summary.value->resource_cost==0, "zero consumption does not activate resource");
        p.requests[0].active=false; s.refresh(); out=s.resume(zero);
        check(out.feasible() && !out.solution.routes[0].connected, "inactive required ignored");
        verify(s,p,m,out);
    }
    {
        R::Problem p(2); p.add_arc(0,1,1'000'000'000'000LL); p.add_request(0,1);
        p.requests[0].required=false; p.requests[0].reject_cost=1;
        auto s=R::make_solver(p); auto out=s.solve(iterations,opt);
        check(out.feasible() && !out.solution.routes[0].connected, "sum objective rejects expensive path");
        p.objective=R::Objective::PenaltyThenCost; s.refresh(); out=s.resume(iterations);
        check(out.feasible() && out.solution.routes[0].connected, "lex objective exact, no giant weight");
        p.requests[0].reject_cost=0; s.refresh(); out=s.resume(iterations);
        check(!out.solution.routes[0].connected, "zero benefit optional rejected if costly");
    }
    {
        R::Problem p(2); p.add_arc(0,1,10); int cheap=p.add_arc(0,1,1); p.add_request(0,1);
        auto s=R::make_solver(p); auto out=s.solve(iterations,opt);
        check(out.solution.routes[0].arcs==std::vector<int>{cheap}, "parallel edge selection");
        p.requests[0].fixed_route=R::Route{true,{0}}; s.refresh(); out=s.resume(iterations);
        check(out.stop_reason==R::StopReason::NoMovableRequests && out.summary.value->route_cost==10, "fixed route has priority");
        p.arcs[0].enabled=false; s.refresh(); out=s.resume(iterations);
        check(out.stop_reason==R::StopReason::FixedConflict && !out.feasible(), "fixed forbidden diagnosed");
        p.arcs[0].enabled=true; p.requests[0].fixed_route=R::Route{}; s.refresh(); out=s.resume(iterations);
        check(out.stop_reason==R::StopReason::FixedConflict && !out.feasible(), "required fixed disconnected conflict");
        p.requests[0].required=false; s.refresh(); out=s.resume(iterations);
        check(out.feasible() && !out.solution.routes[0].connected, "optional disconnected can be fixed");
    }
    {
        // 非負価格だけでは選ばれない(3,3)の経路を資源状態から生成する。
        R::Problem p(5); int r0=p.add_resource(), r1=p.add_resource();
        for (int v=1;v<=3;++v) { p.add_arc(0,v,0); p.add_arc(v,4,0); }
        p.arcs[0].uses={{r1,0,2}}; p.arcs[1].uses={{r1,0,2}};
        p.arcs[2].uses={{r0,0,2}}; p.arcs[3].uses={{r0,0,2}};
        p.arcs[4].uses={{r0,0,1},{r1,0,1}}; p.arcs[5].uses={{r0,0,2},{r1,0,2}};
        p.add_request(0,4); p.requests[0].limits={{r0,3},{r1,3}};
        auto s=R::make_solver(p); auto out=s.solve(iterations,opt);
        check(out.feasible() && out.solution.routes[0].arcs==std::vector<int>({4,5}), "multidimensional alternative");
        verify(s,p,R::DefaultModel{},out);
        p.requests[0].fixed_route=out.solution.routes[0]; p.requests[0].limits[0].upper=2; s.refresh();
        check(s.resume(zero).stop_reason==R::StopReason::FixedConflict, "fixed local budget conflict");
    }
    {
        R::Problem p(3); int r=p.add_resource(1); p.add_arc(0,1,0); p.add_arc(1,2,0); p.add_arc(0,2,5);
        p.arcs[0].uses={{r,0,1}}; p.arcs[1].uses={{r,0,1}}; p.add_request(0,2);
        auto s=R::make_solver(p); auto out=s.solve(iterations,opt);
        check(out.feasible() && out.solution.routes[0].arcs==std::vector<int>{2}, "shared resource repeated on path");
        R::Solution bad{{{true,{0,1}}}}; check(!s.evaluate(bad).summary.value, "repeated resource exact evaluation");
        p.resources[r].capacity=0; s.refresh(); out=s.resume(iterations);
        check(out.feasible(), "zero capacity alternative and no division zero");
        p.resources[r].background_load=1; s.refresh();
        check(s.resume(zero).stop_reason==R::StopReason::FixedConflict, "background capacity conflict");
    }
    {
        R::Problem p(4); p.add_undirected_edge(0,1,1,1); p.add_undirected_edge(1,3,1,1);
        p.add_undirected_edge(0,2,1,1); p.add_undirected_edge(2,3,1,1);
        p.add_request(0,3); p.add_request(0,3);
        Model m; m.mode=1; auto s=R::make_solver(p,m); auto out=s.solve(iterations,opt); check(out.feasible(), "two competing routes");
        R::Solution original=out.solution;
        auto score=out.summary.value;
        const auto& alias=s.improve(s.resume(zero).solution,zero,opt);
        check(alias.feasible() && alias.solution==original && alias.summary.value==score, "own-result alias improve and zero budget");
        p.requests[0].fixed_route=original.routes[0]; s.refresh(); out=s.resume(iterations);
        check(out.solution.routes[0]==original.routes[0], "fixed outer path retained");
        p.requests[0].fixed_route.reset(); p.add_request(0,3); p.requests[2].required=false; p.requests[2].reject_cost=1;
        int dist=p.add_resource(); for(auto& a:p.arcs) a.uses.push_back({dist,0,1}); p.requests[2].limits={{dist,2}};
        s.refresh(); out=s.resume(iterations); verify(s,p,m,out);
        check(out.solution.routes.size()==3, "append request and resource");
        m.mode=2; check(s.evaluate(out.solution).summary.value->resource_cost!=naive(p,m,out.solution).summary.value->resource_cost, "Model owned copy");
        s.set_model(m); out=s.resume(zero); verify(s,p,m,out);
        R::Problem next(2); next.add_arc(0,1,1,2); next.add_request(0,1); s.reset(next);
        out=s.solve(iterations,opt); verify(s,next,m,out); check(out.summary.value->resource_cost==13, "reset keeps Model");
    }
    {
        R::Problem p(0); auto s=R::make_solver(p); auto out=s.solve(zero);
        check(out.feasible() && out.stop_reason==R::StopReason::NoMovableRequests, "empty problem");
        R::Problem disconnected(2); disconnected.add_request(0,1); s.reset(disconnected); out=s.solve(iterations,opt);
        check(!out.feasible() && out.summary.missing_required==1 && out.stop_reason==R::StopReason::IterationLimit, "failure not infeasibility proof");
        R::Limits past; past.time_limit_us=-1; past.deadline=R::Clock::now()-std::chrono::seconds(1);
        check(s.resume(past).statistics.iterations==0, "past absolute deadline");
        R::Limits no_iter; no_iter.time_limit_us=-1; no_iter.max_iterations=0;
        check(s.resume(no_iter).statistics.iterations==0, "iteration zero");
    }
    {
        using D=CapacitatedRouter<double>; D::Problem p(3); p.add_arc(0,1,0.125); p.add_arc(1,2,0.25); p.add_request(0,2);
        auto s=D::make_solver(p); D::Limits l; l.time_limit_us=-1; l.max_iterations=50;
        auto out=s.solve(l); check(out.feasible() && out.summary.value->route_cost==0.375, "double specialization");
    }
    {
        struct MoveModel : R::DefaultModel { std::unique_ptr<int> value; };
        R::Problem p(1); p.add_request(0,0); MoveModel m; m.value=std::make_unique<int>(3);
        auto s=R::make_solver(p,std::move(m)); auto out=s.solve(iterations);
        check(out.feasible(), "move-only Model construction"); MoveModel next; s.set_model(std::move(next));
        check(s.resume(zero).feasible(), "move-only Model replacement");
    }
    {
        R::Problem p(2); int r=p.add_resource(10,1); p.add_arc(0,1,2); p.arcs[0].uses={{r,0,1},{r,0,1}};
        p.add_request(0,1); p.add_request(0,1); Model m;m.mode=2;
        R::Solution full{{{true,{0}},{true,{0}}}}; p.requests[0].fixed_route=full.routes[0];
        auto s=R::make_solver(p,m);auto e=s.evaluate(full);
        check(e.resource_load[r]==5 && e.summary.value->resource_cost==13, "activation once after global aggregation");
        auto reduced=p;reduced.resources[r].background_load+=2;reduced.requests[0].active=false;reduced.requests[0].fixed_route.reset();
        auto partial=full;partial.routes[0]={};auto t=R::make_solver(reduced,m);auto f=t.evaluate(partial);
        check(f.resource_load==e.resource_load && f.summary.value->resource_cost==e.summary.value->resource_cost
              && f.summary.value->route_cost+2==e.summary.value->route_cost, "fixed vs background equivalence including nonlinear fee");
        p.requests[0].source=1;s.refresh();s.solve(zero); // 固定経路の始点が変わった場合も診断する。
        check(s.resume(zero).stop_reason==R::StopReason::FixedConflict, "changed fixed endpoint diagnosed");
    }
    {
        R::Problem p(3);p.add_arc(0,1,0);p.add_arc(1,0,0);p.add_arc(1,2,0);p.add_arc(0,2,1);p.add_request(0,2);
        auto s=R::make_solver(p);auto out=s.solve(iterations,opt);
        check(out.feasible() && out.summary.value->route_cost==0, "zero cost cycle produces simple path");
        R::Limits both;both.time_limit_us=0;both.deadline=R::Clock::now()+std::chrono::seconds(1);
        check(s.resume(both).statistics.iterations==0, "relative zero dominates future deadline");
        p.requests[0].target=1;s.refresh();out=s.resume(iterations);verify(s,p,R::DefaultModel{},out);
        check(out.solution.routes[0].arcs==std::vector<int>{0}, "endpoint update removes old incompatible route");
        p.add_arc(0,2,0);s.reset(p);out=s.improve(out.solution,iterations,opt);verify(s,p,R::DefaultModel{},out);
        check(out.feasible(), "topology change via reset and imported stable edge IDs");
    }
    // 極小問題の全単純路・要求の選択を列挙。最適値より良い不正な値を返さない。
    {
        struct ReferenceModel : R::DefaultModel {
            const int& factor;
            explicit ReferenceModel(const int& value):factor(value){}
            int64_t resource_cost(const R::Problem&,int,int64_t x) const{return factor*x;}
        };
        R::Problem p(2);p.add_arc(0,1,0,2);p.add_request(0,1);int x=2,y=7;
        auto s=R::make_solver(p,ReferenceModel(x));auto out=s.solve(iterations,opt);
        check(out.summary.value->resource_cost==2,"non-default-constructible Model with reference");
        s.set_model(ReferenceModel(y));out=s.resume(zero);
        check(out.summary.value->resource_cost==7,"non-assignable Model replacement");
        y=9;s.refresh();out=s.resume(zero);
        check(out.summary.value->resource_cost==9,"external referenced data refreshed");
    }
    std::mt19937_64 exact_rng(6431);
    int exact_feasible=0,exact_matched=0;
    for(int tc=0;tc<36;++tc) {
        R::Problem p(4);int r=p.add_resource(tc%3==0?-1:2);
        for(int u=0;u<4;++u)for(int v=0;v<4;++v)if(u!=v&&(exact_rng()%3 || v==(u+1)%4)) {
            int a=p.add_arc(u,v,static_cast<int64_t>(exact_rng()%4));
            if(tc%3 && exact_rng()%3==0)p.arcs[a].uses={{r,0,1}};
        }
        for(int k=0;k<3;++k) {
            p.add_request(k,(k+2)%4);p.requests[k].required=tc%3==0||k==0;p.requests[k].reject_cost=3+k;
            if(tc%4==0)p.requests[k].limits={{r,1}};
        }
        p.objective=tc%2?R::Objective::PenaltyThenCost:R::Objective::PenaltyPlusCost;
        Model model;model.mode=tc%3;auto s=R::make_solver(p,model);
        std::vector<std::vector<R::Route>> alternatives(3);
        for(int k=0;k<3;++k) {
            if(!p.requests[k].required)alternatives[k].push_back({});
            std::vector<int> path;std::array<bool,4> seen{};
            auto enumerate=[&](auto&& self,int v)->void {
                if(v==p.requests[k].target){alternatives[k].push_back({true,path});return;}
                seen[v]=true;
                for(int a=0;a<static_cast<int>(p.arcs.size());++a)if(p.arcs[a].from==v&&!seen[p.arcs[a].to]) {
                    path.push_back(a);self(self,p.arcs[a].to);path.pop_back();
                }
                seen[v]=false;
            };
            enumerate(enumerate,p.requests[k].source);
        }
        auto is_better=[&](R::Value a,R::Value b){
            if(p.objective==R::Objective::PenaltyPlusCost)return a.total()<b.total();
            return std::pair(a.reject_cost,a.route_cost+a.resource_cost)<std::pair(b.reject_cost,b.route_cost+b.resource_cost);
        };
        std::optional<R::Value> optimum;
        for(const auto& a:alternatives[0])for(const auto& b:alternatives[1])for(const auto& c:alternatives[2]) {
            auto e=naive(p,model,R::Solution{{a,b,c}});
            if(e.feasible()&&(!optimum||is_better(*e.summary.value,*optimum)))optimum=e.summary.value;
        }
        R::Limits l=iterations;l.max_iterations=300;auto result=s.solve(l,opt);verify(s,p,model,result);
        if(result.feasible()) {
            check(optimum.has_value()&&!is_better(*result.summary.value,*optimum), "exhaustive optimum bound");
            ++exact_feasible;exact_matched+=!is_better(*optimum,*result.summary.value);
        }
        if(tc%3==0)check(result.feasible()&&optimum&&!is_better(*optimum,*result.summary.value), "independent paths reach exact optimum");
    }
    std::cout<<"exhaustive cases=36 solver_feasible="<<exact_feasible<<" exact_matched="<<exact_matched<<'\n';

    {
        using D=CapacitatedRouter<double>;
        struct DecimalModel : D::DefaultModel {
            double resource_cost(const D::Problem&,int,int64_t x) const {return 0.1*static_cast<double>(x*x);}
        };
        D::Problem p(4);p.add_arc(0,1,0.1,1);p.add_arc(1,3,0.2,1);p.add_arc(0,2,0.15,1);p.add_arc(2,3,0.21,1);
        for(auto& v:p.vertices)v.cost=0.03;
        p.add_request(0,3);p.add_request(0,3);
        auto s=D::make_solver(p,DecimalModel{});D::Limits l;l.time_limit_us=-1;l.max_iterations=10000;
        auto result=s.solve(l);auto exact=s.evaluate(result.solution);
        check(result.feasible() && result.summary.value==exact.summary.value,"nonbinary floating costs use canonical final evaluation");
        check(std::abs(result.summary.value->total()-1.24)<1e-10,"floating nonlinear total");
        auto saved=result.solution;double old=result.summary.value->total();
        result=s.improve(saved,l);check(result.feasible()&&result.summary.value->total()<=old,"floating warm start never worsens canonical score");
    }

#ifdef ROUTER_INSTRUMENTED_TESTS
    // 時計確認の途中で必ず中断させ、経路着脱・探索中断・rollbackを検査する。
    {
        R::Problem p(400); R::Solution witness;
        std::vector<std::vector<int>> horizontal(20);
        for(int y=0;y<20;++y)for(int x=0;x<20;++x) {
            int v=y*20+x;
            if(x<19) horizontal[y].push_back(p.add_undirected_edge(v,v+1,1,2).first);
            if(y<19) p.add_undirected_edge(v,v+20,1,2);
        }
        for(int k=0;k<12;++k) {p.add_request(k*20,k*20+19);witness.routes.push_back({true,horizontal[k]});}
        auto s=R::make_solver(p);using S=decltype(s);
        R::Limits l;l.time_limit_us=-1;l.max_iterations=1000;
        for(int cutoff=0;cutoff<=300;++cutoff) {
            S::test_abort_after(-1);auto first=s.improve(witness,zero,opt);
            S::test_abort_after(cutoff);auto out=s.resume(l);S::test_abort_after(-1);
            check(out.stop_reason==R::StopReason::TimeLimit,"forced cooperative interruption");
            check(out.feasible() && out.summary.value->total()<=first.summary.value->total(),"timeout preserves best feasible solution");
            verify(s,p,R::DefaultModel{},out);
        }
    }
#endif
    {
        // 経路評価の再利用領域が、重複・ゼロ消費や不正な固定経路から汚染されない。
        R::Problem p(3); int r=p.add_resource(20);
        p.add_arc(0,1,1); p.add_arc(1,0,1); p.add_arc(1,2,1);
        p.arcs[0].uses={{r,0,0},{r,0,2},{r,0,3}};
        p.vertices[1].uses={{r,0,1}};
        p.add_request(0,2); p.add_request(0,2);
        p.requests[0].fixed_route=R::Route{true,{0,1,0,2}};
        R::Solution sol{{*p.requests[0].fixed_route,{true,{0,2}}}};
        auto s=R::make_solver(p);
        for(int repeat=0;repeat<3;++repeat) {
            auto e=s.evaluate(sol);
            check(e.forbidden_requests==std::vector<int>{0},"invalid fixed cycle does not contaminate following route");
            check(e.resource_load[r]==18,"zero and duplicate resource entries aggregate exactly");
        }
        p.requests[0].fixed_route.reset(); s.refresh();
        auto out=s.solve(iterations,opt); verify(s,p,R::DefaultModel{},out);
        check(out.feasible() && s.evaluate(out.solution).resource_load[r]==12,"work scratch cleared across repeated analyses");
    }
    {
        // 要求別価格が、別要求・資源IDの変更・固定解除・resetへ漏れない。
        R::Problem p(4);
        for (int r=0; r<64; ++r) p.add_resource();
        int a=p.add_arc(0,1,0), b=p.add_arc(1,3,0);
        int c=p.add_arc(0,2,1), d=p.add_arc(2,3,1);
        p.arcs[a].uses=p.arcs[b].uses={{63,0,1}};
        p.arcs[c].uses=p.arcs[d].uses={{2,0,1}};
        for (int k=0; k<3; ++k) p.add_request(0,3);
        p.requests[0].limits={{63,1}};
        p.requests[1].limits={{2,1}};
        auto s=R::make_solver(p);
        auto checked=[&](const R::Result& result) {
            verify(s,p,R::DefaultModel{},result);
            check(result.feasible(),"local prices preserve feasibility");
            check(result.solution.routes[0].arcs==std::vector<int>{c,d},"first local budget");
            check(result.solution.routes[1].arcs==std::vector<int>{a,b},"independent second local budget");
        };
        checked(s.solve(iterations));
        p.requests[0].limits={{2,2},{63,1}};
        s.refresh(); checked(s.resume(iterations));
        while (p.resources.size()<128) p.add_resource();
        p.arcs[a].uses=p.arcs[b].uses={{127,0,1}};
        p.requests[0].limits={{127,1}};
        p.requests[2].limits={{127,2},{2,0}};
        s.refresh(); checked(s.resume(iterations));
        p.resources.resize(8);
        p.arcs[a].uses=p.arcs[b].uses={{7,0,1}};
        p.requests[0].limits={{7,1},{2,2}};
        p.requests[2].limits.clear();
        s.refresh(); checked(s.resume(iterations));
        auto previous=s.resume(iterations).solution;
        checked(s.improve(previous,iterations));
        p.requests[0].fixed_route=previous.routes[0];
        s.refresh(); checked(s.resume(iterations));
        p.requests[0].fixed_route.reset();
        p.requests[1].limits={{2,1},{7,2}};
        p.requests[2].limits={{7,2}};
        s.refresh(); checked(s.resume(iterations));
#ifdef ROUTER_INSTRUMENTED_TESTS
        using S=decltype(s);
        for (int cutoff=0; cutoff<=100; ++cutoff) {
            S::test_abort_after(cutoff); auto result=s.resume(iterations);
            S::test_abort_after(-1); checked(result);
            checked(s.resume(iterations));
        }
#endif
        R::Problem next(2); int r=next.add_resource();
        next.add_arc(0,1,1); next.arcs[0].uses={{r,0,1}};
        next.add_request(0,1); next.requests[0].limits={{r,1}};
        s.reset(next); check(s.solve(iterations).feasible(),"reset resizes local-price workspace");
    }
    {
        // 無効要求と固定の組合せでも、返却診断を再評価と一致させる。
        const std::vector<R::Route> forms{{}, {true,{0}}, {false,{0}}, {true,{}}, {true,{0,1,0}}};
        for (bool active : {false,true}) for (bool required : {false,true}) for (size_t form=0; form<forms.size(); ++form) {
            R::Problem p(2); p.add_arc(0,1); p.add_arc(1,0); p.add_request(0,1);
            p.requests[0].fixed_route=forms[1];
            auto s=R::make_solver(p); check(s.solve(zero).feasible(),"fixed-state regression starts feasible");
            p.requests[0].active=active; p.requests[0].required=required; p.requests[0].fixed_route=forms[form];
            bool feasible=active ? form==1 || (form==0 && !required) : form==0;
            s.refresh();
            auto inspect=[&](const R::Result& out) {
                verify(s,p,R::DefaultModel{},out);
                check(out.feasible()==feasible,"fixed-state truth table");
                check(out.stop_reason==(feasible?R::StopReason::NoMovableRequests:R::StopReason::FixedConflict),"fixed-state stop reason");
            };
            inspect(s.resume(zero)); inspect(s.solve(zero));
            p.requests[0].fixed_route.reset(); p.requests[0].active=true;
            s.refresh(); auto out=s.resume(iterations);
            check(out.feasible(),"fixed conflict recovery"); verify(s,p,R::DefaultModel{},out);
        }
        using D=CapacitatedRouter<double>;
        D::Problem p(2); p.add_arc(0,1); p.add_request(0,1);
        p.requests[0].active=false; p.requests[0].fixed_route=D::Route{false,{0}};
        auto s=D::make_solver(p); auto out=s.solve(D::Limits{0});
        check(!out.feasible() && out.summary.fixed_mismatches==1,"double inactive malformed fixed route diagnosed");
        check(out.summary.value==s.evaluate(out.solution).summary.value,"double fixed-conflict evaluation agrees");
        p.requests[0].fixed_route=D::Route{}; s.refresh();
        check(s.resume(D::Limits{0}).feasible(),"double inactive fixed conflict recovery");
    }
    std::mt19937_64 gen(9182);
    // 多様な小規模問題で、全APIの更新・再開と独立評価の一致を反復確認。
    for (int tc=0;tc<160;++tc) {
        int n=4+static_cast<int>(gen()%9), count=1+static_cast<int>(gen()%8);
        R::Problem p(n);
        int shared=p.add_resource(3+static_cast<int64_t>(gen()%7));
        int distance=p.add_resource();
        for(int v=1;v<n;++v) p.add_undirected_edge(v-1,v,1+static_cast<int64_t>(gen()%5),1+static_cast<int64_t>(gen()%3));
        for(int j=0;j<n;++j) {
            int u=static_cast<int>(gen()%static_cast<uint64_t>(n)),v=static_cast<int>(gen()%static_cast<uint64_t>(n));
            if(u!=v) p.add_arc(u,v,static_cast<int64_t>(gen()%6),1+static_cast<int64_t>(gen()%3));
        }
        for(auto& a:p.arcs) { a.uses.push_back({distance,0,1}); if(gen()%4==0) a.uses.push_back({shared,0,1}); }
        for(int k=0;k<count;++k) {
            p.add_request(static_cast<int>(gen()%static_cast<uint64_t>(n)),static_cast<int>(gen()%static_cast<uint64_t>(n)),static_cast<int64_t>(gen()%3));
            auto& q=p.requests.back(); q.required=gen()%2!=0; q.reject_cost=1+static_cast<int64_t>(gen()%20);
            q.consume_endpoints=gen()%2!=0;
            if(gen()%2) q.limits={{distance,2+static_cast<int64_t>(gen()%8)}};
        }
        p.objective=tc%2 ? R::Objective::PenaltyPlusCost : R::Objective::PenaltyThenCost;
        Model model; model.mode=tc%3;
        auto s=R::make_solver(p,model); auto out=s.solve(iterations,opt); verify(s,p,model,out);
        auto t=R::make_solver(p,model); auto duplicate=t.solve(iterations,opt);
        check(duplicate.solution==out.solution && duplicate.statistics.iterations==out.statistics.iterations, "seed deterministic");
        R::Limits part=iterations; part.max_iterations=50;
        auto u=R::make_solver(p,model); u.solve(part,opt); auto split=u.resume(part);
        check(split.solution==out.solution, "resume split equals uninterrupted iterations");
        for(int turn=0;turn<12;++turn) {
            int k=static_cast<int>(gen()%static_cast<uint64_t>(count));
            if(turn%6==0 && out.feasible()) p.requests[k].fixed_route=out.solution.routes[k];
            else if(turn%6==1) p.requests[k].fixed_route.reset();
            else if(turn%6==2) p.requests[k].active=!p.requests[k].active;
            else if(turn%6==3) p.requests[k].demand=static_cast<int64_t>(gen()%3);
            else if(turn%6==4) { p.resources[shared].capacity=static_cast<int64_t>(gen()%8); p.requests[k].limits={{distance,static_cast<int64_t>(gen()%10)}}; }
            else { model.mode=(model.mode+1)%3; s.set_model(model); }
            if(!p.requests[k].active) p.requests[k].fixed_route.reset();
            s.refresh(); out=s.resume(iterations); verify(s,p,model,out);
        }
    }
    std::cout << "PASS checks=" << checks << " random_cases=160 turns=1920\n";
}

#endif
