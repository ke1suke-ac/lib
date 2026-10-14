#pragma once
#include <bits/stdc++.h>

// 前後関係・絶対位置・有向隣接費用を最小化する順列探索
// 必須先行関係、任意許可位置、呼出し単位の部分固定を扱う
// Costの全演算が型の範囲に収まる有限値を入力する。単一スレッド用
// n: 要素数、m: 可動要素数、e: 疎Cの項数、h: 直接先行辺数、d: 対象要素の関係数
// 時間指定は呼出し全体の終了目標。採点・検証・返却の最低限の時間は残る

template<class Cost = int64_t>
class LinearOrderingSolver {
    static_assert((std::is_integral_v<Cost> && std::is_signed_v<Cost>) ||
                  std::is_floating_point_v<Cost>);
public:
    using Clock = std::chrono::steady_clock;
    struct OrderTerm { int before, after; Cost cost; };
    enum class Status { Feasible, NotFound, ProvenInfeasible };
    struct Options {
        int64_t time_limit_us = 100'000;
        Clock::time_point deadline = Clock::time_point::max();
        uint64_t seed = 0;
        bool allow_infeasible_moves = false; // 初期違反の修復はfalseでも行う
    };
    struct Result {
        Status status = Status::NotFound;
        std::vector<int> order;
        Cost cost = 0;
        // 改行せず簡潔に表示する。順列は先頭20要素まで。
        friend std::ostream& operator<<(std::ostream& os, const Result& r) {
            const char* status = r.status == Status::Feasible ? "Feasible" :
                r.status == Status::NotFound ? "NotFound" :
                r.status == Status::ProvenInfeasible ? "ProvenInfeasible" : "Unknown";
            os << "Result{status=" << status;
            if (r.status == Status::Feasible) os << ", cost=" << +r.cost;
            os << ", order=[";
            const size_t shown = std::min<size_t>(r.order.size(), 20);
            for (size_t i = 0; i < shown; ++i) os << (i ? ", " : "") << r.order[i];
            if (r.order.size() > shown) os << ", ... (" << r.order.size() << " total)";
            return os << "]}";
        }
    };

    class Problem {
        friend class LinearOrderingSolver;
        struct Domain {
            int lo = 0, hi = 0;
            std::vector<uint64_t> bits;
            bool contains(int pos) const {
                return lo <= pos && pos < hi && (bits.empty() ||
                       ((bits[static_cast<size_t>(pos) / 64] >> (pos % 64)) & 1));
            }
        };
        int n_;
        Cost constant_ = 0;
        std::vector<Cost> c_, u_, t_;
        std::vector<std::vector<std::pair<int, Cost>>> sparse_;
        std::vector<std::pair<int, int>> precedences_;
        std::vector<Domain> domains_;
        uint64_t c_version_ = 1, p_version_ = 1, cost_version_ = 1, constraint_version_ = 1;
        void id(int i) const { assert(0 <= i && i < n_); (void)i; }
        static void finite(Cost value) {
            if constexpr (std::is_floating_point_v<Cost>) assert(std::isfinite(value));
            (void)value;
        }
        size_t at(int i, int j) const { return static_cast<size_t>(i) * n_ + j; }
        void matrix(std::vector<Cost>& dst, std::span<const Cost> src) {
            assert(src.size() == static_cast<size_t>(n_) * n_);
            for (Cost v : src) finite(v);
            dst.assign(src.begin(), src.end());
            ++cost_version_;
        }
        void entry(std::vector<Cost>& dst, int i, int j, Cost value, bool add) {
            id(i); id(j); finite(value);
            if (dst.empty()) dst.resize(static_cast<size_t>(n_) * n_);
            if (add) dst[at(i,j)] += value; else dst[at(i,j)] = value;
            ++cost_version_;
        }
        void order_entry(int i, int j, Cost value, bool add) {
            id(i); id(j); finite(value);
            if (!c_.empty()) entry(c_, i, j, value, add);
            else {
                auto& row = sparse_[i];
                auto it = std::lower_bound(row.begin(), row.end(), j,
                    [](const auto& a, int b) { return a.first < b; });
                if (it == row.end() || it->first != j) {
                    if (value != 0) row.insert(it, {j, value});
                } else {
                    if (add) it->second += value; else it->second = value;
                    if (it->second == 0) row.erase(it);
                }
                ++cost_version_;
            }
            ++c_version_;
        }
        Cost c(int i, int j) const {
            if (!c_.empty()) return c_[at(i,j)];
            const auto& row = sparse_[i];
            auto it = std::lower_bound(row.begin(), row.end(), j,
                [](const auto& a, int b) { return a.first < b; });
            return it != row.end() && it->first == j ? it->second : Cost(0);
        }
        Cost u(int i, int pos) const { return u_.empty() ? Cost(0) : u_[at(i,pos)]; }
        Cost t(int i, int j) const { return t_.empty() ? Cost(0) : t_[at(i,j)]; }
    public:
        // 全位置を許可するn要素の問題を作る O(n)
        explicit Problem(int n) : n_(n) {
            assert(n >= 0);
            sparse_.resize(static_cast<size_t>(n)); domains_.resize(static_cast<size_t>(n));
            for (auto& d : domains_) d.hi = n;
        }
        // 問題を複製する O(入力データ量)
        Problem(const Problem&) = default;
        // 問題の所有データを移す O(1)
        Problem(Problem&&) = default;
        Problem& operator=(const Problem&) = delete;
        Problem& operator=(Problem&&) = delete;
        // 要素数を返す O(1)
        int size() const { return n_; }
        // 指定方向の費用へ加算する 密O(1)、疎O(d)
        void add_order_cost(int i, int j, Cost delta) { order_entry(i,j,delta,true); }
        // 指定方向の費用を上書きする 密O(1)、疎O(d)
        void set_order_cost(int i, int j, Cost value) { order_entry(i,j,value,false); }
        // iをjより前にする希望の非負罰金を加える 密O(1)、疎O(d)
        void add_preference(int i, int j, Cost weight) { assert(weight >= 0); add_order_cost(j,i,weight); }
        // 行優先密行列で順序費用全体を置き換える O(n²+e)
        void set_order_costs(std::span<const Cost> values) {
            matrix(c_,values); for (auto& row : sparse_) row.clear(); ++c_version_;
        }
        // 疎レコードで順序費用全体を置き換え、同方向を合算する O(n+e log e)
        void set_order_terms(std::span<const OrderTerm> terms) {
            // 行へまとめてから重複を統合する
            c_.clear(); sparse_.assign(static_cast<size_t>(n_), {});
            for (auto [i,j,v] : terms) { id(i); id(j); finite(v); sparse_[i].emplace_back(j,v); }
            for (auto& row : sparse_) {
                std::sort(row.begin(),row.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                size_t k = 0;
                for (auto [j,v] : row) {
                    if (k && row[k-1].first == j) row[k-1].second += v;
                    else row[k++] = {j,v};
                }
                row.resize(k);
                std::erase_if(row, [](const auto& a) { return a.second == 0; });
            }
            ++c_version_; ++cost_version_;
        }
        // 順序費用を省略状態へ戻す O(n+e+n²)
        void clear_order_costs() { c_.clear(); for (auto& r : sparse_) r.clear(); ++c_version_; ++cost_version_; }
        // 位置費用へ加算する 初回O(n²)、以後O(1)
        void add_position_cost(int i, int pos, Cost delta) { entry(u_,i,pos,delta,true); }
        // 位置費用を上書きする 初回O(n²)、以後O(1)
        void set_position_cost(int i, int pos, Cost value) { entry(u_,i,pos,value,false); }
        // 行優先密行列で位置費用を置き換える O(n²)
        void set_position_costs(std::span<const Cost> values) { matrix(u_,values); }
        // 位置費用を省略状態へ戻す O(n²)
        void clear_position_costs() { u_.clear(); ++cost_version_; }
        // 有向隣接費用へ加算する 初回O(n²)、以後O(1)
        void add_adjacent_cost(int i, int j, Cost delta) { entry(t_,i,j,delta,true); }
        // 有向隣接費用を上書きする 初回O(n²)、以後O(1)
        void set_adjacent_cost(int i, int j, Cost value) { entry(t_,i,j,value,false); }
        // 行優先密行列で有向隣接費用を置き換える O(n²)
        void set_adjacent_costs(std::span<const Cost> values) { matrix(t_,values); }
        // 隣接費用を省略状態へ戻す O(n²)
        void clear_adjacent_costs() { t_.clear(); ++cost_version_; }
        // 定数費用へ加算する O(1)
        void add_constant_cost(Cost delta) { finite(delta); constant_ += delta; ++cost_version_; }
        // 定数費用を上書きする O(1)
        void set_constant_cost(Cost value) { finite(value); constant_ = value; ++cost_version_; }
        // 直接先行辺を重複なしで追加する O(h)
        void require_before(int i, int j) {
            id(i); id(j); auto edge = std::pair{i,j};
            auto it = std::lower_bound(precedences_.begin(),precedences_.end(),edge);
            if (it == precedences_.end() || *it != edge) {
                precedences_.insert(it,edge); ++p_version_; ++constraint_version_;
            }
        }
        // 直接先行辺全体を置き換える O(h log h)
        void set_precedences(std::span<const std::pair<int,int>> edges) {
            for (auto [i,j] : edges) { id(i); id(j); }
            precedences_.assign(edges.begin(),edges.end());
            std::sort(precedences_.begin(),precedences_.end());
            precedences_.erase(std::unique(precedences_.begin(),precedences_.end()),precedences_.end());
            ++p_version_; ++constraint_version_;
        }
        // 先行制約をすべて解除する O(h)
        void clear_precedences() { precedences_.clear(); ++p_version_; ++constraint_version_; }
        // 許可集合を半開区間へ置き換える O(1)
        void set_position_range(int i, int lo, int hi) {
            id(i); assert(0 <= lo && lo <= hi && hi <= n_);
            domains_[i] = {lo,hi,{}}; ++constraint_version_;
        }
        // 現在の許可集合を半開区間と交差させる O(1)
        void restrict_position_range(int i, int lo, int hi) {
            id(i); assert(0 <= lo && lo <= hi && hi <= n_);
            auto& d = domains_[i]; d.lo = std::max(d.lo,lo); d.hi = std::min(d.hi,hi); ++constraint_version_;
        }
        // 明示した位置集合へ置き換える O(n/64+集合サイズ)
        void set_positions(int i, std::span<const int> positions) {
            id(i); auto& d = domains_[i];
            d.bits.assign((static_cast<size_t>(n_) + 63) / 64, 0);
            d.lo=n_; d.hi=0;
            for (int pos : positions) { id(pos); d.bits[static_cast<size_t>(pos)/64] |= uint64_t(1) << (pos%64); d.lo=std::min(d.lo,pos); d.hi=std::max(d.hi,pos+1); }
            ++constraint_version_;
        }
        // 現在の許可集合を明示集合と交差させる O(n/64+集合サイズ)
        void restrict_positions(int i, std::span<const int> positions) {
            id(i); std::vector<int> kept;
            for (int pos : positions) { id(pos); if (domains_[i].contains(pos)) kept.push_back(pos); }
            set_positions(i,kept);
        }
        // 現在の許可集合を1位置へ絞る O(1)
        void fix_position(int i, int pos) { id(pos); restrict_position_range(i,pos,pos+1); }
        // 指定要素の位置制約を解除する O(1)
        void clear_position_restriction(int i) { set_position_range(i,0,n_); }
    };

private:
#if __INCLUDE_LEVEL__ == 0
public:
#endif
    Problem p_;
    std::vector<int> last_order_;
    Cost last_cost_ = 0;
    uint64_t last_cost_version_ = 0, last_constraint_version_ = 0;
    uint64_t prepared_c_ = 0, prepared_p_ = 0;
    std::vector<std::vector<std::pair<int,Cost>>> relations_;
    std::vector<std::vector<std::pair<int,int>>> precedence_relations_;
    bool cyclic_ = false;
    std::vector<int> topological_;
    std::vector<Cost> difference_;
    Cost difference(int i,int j) const {
        return difference_.empty()?p_.c(i,j)-p_.c(j,i):difference_[p_.at(i,j)];
    }
    struct Rng {
        uint64_t state;
        uint64_t next() {
            uint64_t x = (state += 0x9e3779b97f4a7c15ULL);
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            return x ^ (x >> 31);
        }
        int index(int n) { return static_cast<int>(next() % static_cast<uint64_t>(n)); }
        double unit() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
    };
    struct Delta { Cost cost = 0; int precedence = 0, domain = 0; };
    static bool expired(Clock::time_point end) { return Clock::now() >= end; }
    bool permutation(std::span<const int> a) const {
        if (a.size() != static_cast<size_t>(p_.n_)) return false;
        std::vector<uint8_t> seen(a.size(),0);
        for (int x : a) { if (x < 0 || x >= p_.n_ || seen[x]) return false; seen[x] = 1; }
        return true;
    }
    Cost value(std::span<const int> a, const std::vector<int>& pos) const {
        Cost f = p_.constant_;
        if (!p_.c_.empty()) {
            for (int i = 0; i < p_.n_; ++i) {
                const Cost* row = p_.c_.data() + static_cast<size_t>(a[i]) * p_.n_;
                for (int j = i+1; j < p_.n_; ++j) f += row[a[j]];
            }
        } else {
            for (int i = 0; i < p_.n_; ++i)
                for (auto [j,c] : p_.sparse_[i]) if (pos[i] < pos[j]) f += c;
        }
        for (int i = 0; i < p_.n_; ++i) {
            f += p_.u(a[i],i);
            if (i+1 < p_.n_) f += p_.t(a[i],a[i+1]);
        }
        return f;
    }
    std::pair<int,int> violations(const std::vector<int>& pos) const {
        int vp = 0, vd = 0;
        for (auto [i,j] : p_.precedences_) vp += pos[i] >= pos[j];
        for (int i = 0; i < p_.n_; ++i) vd += !p_.domains_[i].contains(pos[i]);
        return {vp,vd};
    }
    bool prepare_until(Clock::time_point end) {
        const int n = p_.n_;
        if (prepared_c_ != p_.c_version_) {
            // 疎入力だけに双方向の差分係数を作り、完成後に世代を確定する
            relations_.assign(static_cast<size_t>(n),{});
            if (p_.c_.empty()) {
                for (int i = 0; i < n; ++i) {
                    if ((i & 31) == 0 && expired(end)) return false;
                    for (auto [j,c] : p_.sparse_[i]) if (i != j) {
                        relations_[i].emplace_back(j,c); relations_[j].emplace_back(i,-c);
                    }
                }
            }
            difference_.clear();
            prepared_c_ = p_.c_version_;
        }
        // 短い呼出しでは大きな任意補助表を作らず、元の行列を直接使う
        double remaining=std::chrono::duration<double,std::micro>(end-Clock::now()).count();
        if(difference_.empty() && !p_.c_.empty() &&
           remaining>static_cast<double>(p_.c_.size())/32.0+50.0){
            difference_.resize(p_.c_.size());
            for(int i=0;i<n;++i){
                if((i&15)==0 && expired(end)){difference_.clear();return false;}
                for(int j=0;j<n;++j)difference_[p_.at(i,j)]=p_.c(i,j)-p_.c(j,i);
            }
        }
        if (prepared_p_ != p_.p_version_) {
            // 元の直接辺を両端から参照できるようにし、循環を判定する
            precedence_relations_.assign(static_cast<size_t>(n),{});
            std::vector<int> indegree(static_cast<size_t>(n),0), queue;
            size_t tick = 0;
            for (auto [i,j] : p_.precedences_) {
                if ((tick++ & 1023) == 0 && expired(end)) return false;
                precedence_relations_[i].emplace_back(j,1); precedence_relations_[j].emplace_back(i,-1);
                ++indegree[j];
            }
            for (int i = 0; i < n; ++i) if (!indegree[i]) queue.push_back(i);
            for (size_t k = 0; k < queue.size(); ++k) {
                if ((k & 255) == 0 && expired(end)) return false;
                for (auto [j,dir] : precedence_relations_[queue[k]])
                    if (dir == 1 && --indegree[j] == 0) queue.push_back(j);
            }
            cyclic_ = queue.size() != static_cast<size_t>(n);
            topological_ = std::move(queue);
            prepared_p_ = p_.p_version_;
        }
        return true;
    }

    struct State {
        LinearOrderingSolver& solver;
        const Problem& p;
        Rng rng;
        Clock::time_point end;
        std::vector<int> a, pos, slots, slot_index, movable;
        std::vector<int> mark, proposed, changed, replacement, edge_mark;
        std::vector<Cost> row_cost, unary_table;
        std::vector<int> fixed_violations, element_index, lower, upper;
        std::vector<int> row_precedence;
        int stamp = 0, vp = 0, vd = 0;
        bool interrupted = false, allow_infeasible = false;
        Cost cost = 0;
        std::vector<int> best;
        Cost best_cost = 0;
        State(LinearOrderingSolver& s, std::span<const int> initial, std::span<const uint8_t> flags,
              uint64_t seed, Clock::time_point limit)
            : solver(s), p(s.p_), rng{seed}, end(limit), a(initial.begin(),initial.end()),
              pos(a.size()), slot_index(a.size(),-1), movable(a.size()), mark(a.size(),0),
              proposed(a.size()), replacement(a.size()), edge_mark(a.size(),0), row_cost(a.size()), row_precedence(a.size()) {
            for (int i = 0; i < p.n_; ++i) {
                int x = a[i]; pos[x] = i;
                movable[x] = (flags.empty() || flags[x]) && !(p.domains_[x].hi-p.domains_[x].lo==1 && p.domains_[x].contains(i));
                if (movable[x]) { slot_index[i] = static_cast<int>(slots.size()); slots.push_back(i); }
            }
            // 完全一致と世代番号を確認できた、前回の確定値だけを再利用する
            bool same=a==solver.last_order_;
            cost=same && solver.last_cost_version_==p.cost_version_ ? solver.last_cost_ : solver.value(a,pos);
            if(!same || solver.last_constraint_version_!=p.constraint_version_)
                std::tie(vp,vd)=solver.violations(pos);
            save();
        }
        void save() {
            if (vp == 0 && vd == 0 && (best.empty() || cost < best_cost)) { best = a; best_cost = cost; }
        }
        void reset(std::vector<int> next) {
            a = std::move(next);
            for (int i = 0; i < p.n_; ++i) pos[a[i]] = i;
            cost = solver.value(a,pos); std::tie(vp,vd) = solver.violations(pos); save();
        }
        bool allowed(int x, int absolute) const {
            if (!p.domains_[x].contains(absolute)) return false;
            // 伝播済みの上下限には、固定要素との先行制約も含まれる
            if(!lower.empty())return lower[x]<=absolute && absolute<=upper[x];
            for (auto [y,dir] : solver.precedence_relations_[x]) if (!movable[y]) {
                if (dir == 1 ? absolute >= pos[y] : absolute <= pos[y]) return false;
            }
            return true;
        }
        void begin(const std::vector<int>& positions, const std::vector<int>& values) {
            if (stamp == std::numeric_limits<int>::max()) {
                std::fill(mark.begin(),mark.end(),0);
                std::fill(edge_mark.begin(),edge_mark.end(),0);
                stamp = 0;
            }
            ++stamp; changed.clear();
            for (size_t k = 0; k < positions.size(); ++k) {
                int x = values[k];
                if (pos[x] == positions[k]) continue;
                mark[x] = stamp; proposed[x] = positions[k]; changed.push_back(x);
                replacement[positions[k]] = x;
            }
        }
        int next_pos(int x) const { return mark[x] == stamp ? proposed[x] : pos[x]; }
        int next_at(int j) const { return mark[a[j]] == stamp ? replacement[j] : a[j]; }
        Delta delta() {
            Delta d; interrupted = false;
            // 変更要素に触れるペアを1回だけ数える
            int ticks = 0;
            for (int x : changed) {
                if ((ticks++ & 7) == 0 && expired(end)) { interrupted = true; return {}; }
                auto pair_delta = [&](int y, Cost c) {
                    if (x == y || (!unary_table.empty()&&!movable[y]) || (mark[y] == stamp && y < x)) return;
                    int sign = int(next_pos(x) < next_pos(y)) - int(pos[x] < pos[y]);
                    if (sign != 0) d.cost += sign == 1 ? c : -c;
                };
                if (p.c_.empty()) for (auto [y,c] : solver.relations_[x]) pair_delta(y,c);
                else if(!unary_table.empty()) for(int absolute:slots)pair_delta(a[absolute],solver.difference(x,a[absolute]));
                else for (int y = 0; y < p.n_; ++y) pair_delta(y,solver.difference(x,y));
                for (auto [y,dir] : solver.precedence_relations_[x]) {
                    if ((!unary_table.empty()&&!movable[y]) || (mark[y] == stamp && y < x)) continue;
                    d.precedence += dir == 1 ? int(next_pos(x) >= next_pos(y))-int(pos[x] >= pos[y])
                                             : int(next_pos(y) >= next_pos(x))-int(pos[y] >= pos[x]);
                }
                if(unary_table.empty())d.cost += p.u(x,proposed[x])-p.u(x,pos[x]);
                else {
                    auto change=unary_delta(x,slot_index[pos[x]],slot_index[proposed[x]]);
                    d.cost+=change.cost;d.precedence+=change.precedence;
                }
                d.domain += int(!p.domains_[x].contains(proposed[x]))-int(!p.domains_[x].contains(pos[x]));
            }
            // 変更位置の左右の隣接辺を重複なしで評価する
            if (!p.t_.empty()) for (int x : changed) for (int j : {pos[x]-1,pos[x]}) {
                if (j < 0 || j+1 >= p.n_ || edge_mark[j] == stamp) continue;
                if(!unary_table.empty()&&(!movable[a[j]]||!movable[a[j+1]]))continue;
                edge_mark[j] = stamp;
                d.cost += p.t(next_at(j),next_at(j+1))-p.t(a[j],a[j+1]);
            }
            return d;
        }
        void apply(const Delta& d) {
            for (int x : changed) { pos[x] = proposed[x]; a[pos[x]] = x; }
            cost += d.cost; vp += d.precedence; vd += d.domain; save();
        }
        void insertion(int from, int to, std::vector<int>& z, std::vector<int>& v) const {
            z.clear(); v.clear();
            int x = a[slots[from]], dir = from < to ? 1 : -1;
            for (int j = from; j != to; j += dir) { z.push_back(slots[j]); v.push_back(a[slots[j+dir]]); }
            z.push_back(slots[to]); v.push_back(x);
        }
        bool cycle(std::vector<int>& z, std::vector<int>& v) {
            const int m = static_cast<int>(slots.size());
            std::vector<int> path, seen(static_cast<size_t>(m),-1);
            int cur = rng.index(m);
            // ランダムな許可辺をたどり、長さを固定しない巡回を抽出する
            for (int step = 0; step <= m; ++step) {
                if ((step & 15) == 0 && expired(end)) return false;
                if (seen[cur] >= 0) {
                    int start = seen[cur];
                    if (static_cast<int>(path.size())-start < 2) return false;
                    z.clear(); v.clear();
                    for (int k = start; k < static_cast<int>(path.size()); ++k) {
                        int next = k+1 < static_cast<int>(path.size()) ? path[k+1] : cur;
                        z.push_back(slots[next]); v.push_back(a[slots[path[k]]]);
                    }
                    return true;
                }
                seen[cur] = static_cast<int>(path.size()); path.push_back(cur);
                int x = a[slots[cur]], offset = rng.index(m), next = -1;
                for (int k = 0; k < m; ++k) {
                    if((k&255)==0 && expired(end))return false;
                    int j = (offset+k)%m;
                    if (j != cur && allowed(x,slots[j])) { next = j; break; }
                }
                if (next < 0) return false;
                cur = next;
            }
            return false;
        }
        // 実行可能解がある場合だけ、U/Tを使った別の初期配置を試す
        int construct(bool use_cost = false) {
            const int n = p.n_, m = static_cast<int>(slots.size());
            // 先行辺を満たす要素から、許可範囲の締切が早いものを順に置く
            // 固定要素も含めて最早・最遅位置を伝播する。空区間なら不能の証明となる
            {
                lower.resize(a.size());upper.resize(a.size());
                for(int x=0;x<n;++x){lower[x]=movable[x]?p.domains_[x].lo:pos[x];upper[x]=movable[x]?p.domains_[x].hi-1:pos[x];}
                for(int x:solver.topological_){
                    if(expired(end))return 0;
                    // 構築中だけ作業行を再利用し、要素ごとのUの定数を除く
                    if(use_cost && !p.u_.empty()){
                        const Cost* row=p.u_.data()+p.at(x,0);
                        row_cost[x]=*std::min_element(row,row+n);
                    }
                    while(lower[x]<=upper[x]&&!p.domains_[x].contains(lower[x]))++lower[x];
                    if(lower[x]>upper[x])return -1;
                    for(auto[y,dir]:solver.precedence_relations_[x])if(dir==1)lower[y]=std::max(lower[y],lower[x]+1);
                }
                for(auto it=solver.topological_.rbegin();it!=solver.topological_.rend();++it){
                    int x=*it;if(expired(end))return 0;
                    while(lower[x]<=upper[x]&&!p.domains_[x].contains(upper[x]))--upper[x];
                    if(lower[x]>upper[x])return -1;
                    for(auto[y,dir]:solver.precedence_relations_[x])if(dir==-1)upper[y]=std::min(upper[y],upper[x]-1);
                }
            }
            std::vector<int> indegree(static_cast<size_t>(n),0), next=a;
            std::vector<uint8_t> used(static_cast<size_t>(n),0);
            for(auto [x,y]:p.precedences_){(void)x;++indegree[y];}
            bool complete=true;
            for(int j=0;j<n;++j){
                if((j&31)==0 && expired(end))return 0;
                int choice=-1,deadline=n+1,offset=rng.index(std::max(1,m));Cost best_score=0;
                if(!movable[a[j]]){if(indegree[a[j]]==0)choice=a[j];}
                else for(int k=0;k<m;++k){
                    int index=k+offset;if(index>=m)index-=m;int x=a[slots[index]];
                    int bound=upper[x]+1;
                    if(!used[x] && !indegree[x] && allowed(x,j)){
                        Cost score=use_cost ? p.u(x,j)-row_cost[x]+(j?p.t(next[j-1],x):Cost(0)) : Cost(0);
                        if(bound<deadline || (bound==deadline && score<best_score)){choice=x;deadline=bound;best_score=score;}
                    }
                }
                if(choice<0){complete=false;break;}
                next[j]=choice;used[choice]=1;
                for(auto [y,dir]:solver.precedence_relations_[choice])if(dir==1)--indegree[y];
            }
            if(complete){reset(std::move(next));return 1;}
            // 再構築が失敗しても、既存の実行可能な探索状態を保つ
            if(!best.empty())return 0;
            if(!p.precedences_.empty()){constraint_backtrack();if(!best.empty())return 1;}
            std::vector<int> match(static_cast<size_t>(m),-1), visit(static_cast<size_t>(m),-1);
            unsigned tick = 0;
            bool timed_out = false;
            // 増加路が見つからなかった場合だけ割当不能とする。打切りと区別する
            auto augment = [&](auto&& self, int x) -> bool {
                if ((++tick & 127) == 0 && expired(end)) { timed_out = true; return false; }
                int start = rng.index(m);
                for (int k = 0; k < m; ++k) {
                    int j = (k+start)%m;
                    if ((k & 255)==0 && expired(end)) { timed_out = true; return false; }
                    if (!allowed(x,slots[j])) continue;
                    // 同じ増加路探索で調べた位置は、再び調べない
                    if (visit[j] == -2) continue;
                    visit[j] = -2;
                    if (match[j] < 0 || self(self,match[j])) { match[j] = x; return true; }
                    if (timed_out) return false;
                }
                return false;
            };
            for (int slot : slots) {
                std::fill(visit.begin(),visit.end(),-1);
                if (!augment(augment,a[slot])) return timed_out ? 0 : -1;
            }
            next = a;
            for (int j = 0; j < m; ++j) next[slots[j]] = match[j];
            reset(std::move(next));
            return 1;
        }
        // 初期構築だけに残り時間の1/8を使う。静的な分岐順は一度だけ整列する。
        // degreeの負値は配置済みを表し、別の使用済み配列を持たない。
        // 失敗・時間切れでは探索状態と乱数を変更しない。
        void constraint_backtrack() {
            const int n=p.n_;auto limit=Clock::now()+(end-Clock::now())/8;
            std::vector<int>degree(n),next=a,priority=a;
            for(auto[x,y]:p.precedences_){(void)x;++degree[y];}
            std::sort(priority.begin(),priority.end(),[&](int x,int y){return std::tie(upper[x],pos[x])<std::tie(upper[y],pos[y]);});
            auto dfs=[&](auto&&self,int j)->bool{
                if(expired(limit))return false;
                if(j==n){reset(std::move(next));return true;}
                for(int x:priority)if(degree[x]>=0){
                    if(upper[x]<j)return false;
                    if(degree[x] || !(movable[a[j]]?bool(movable[x]):x==a[j]) || !allowed(x,j))continue;
                    degree[x]=-1;next[j]=x;
                    for(auto[y,d]:solver.precedence_relations_[x])if(d==1)--degree[y];
                    if(self(self,j+1))return true;
                    for(auto[y,d]:solver.precedence_relations_[x])if(d==1)++degree[y];
                    degree[x]=0;
                    if(expired(limit))return false;
                }
                return false;
            };
            dfs(dfs,0);
        }
        void rank_initial() {
            // 順序費用の両方向差を合計し、前に置くと有利な要素から並べる
            // 可動要素だけを並べ直し、固定要素の位置を保持する
            std::vector<std::pair<Cost,int>> rank;
            for(int slot:slots){
                int x=a[slot];if(expired(end))return;
                Cost score=0;
                if(p.c_.empty())for(auto[y,c]:solver.relations_[x]){(void)y;score+=c;}
                else for(int y=0;y<p.n_;++y)score+=solver.difference(x,y);
                rank.emplace_back(score,x);
            }
            std::sort(rank.begin(),rank.end());auto candidate=a;
            for(size_t j=0;j<slots.size();++j)candidate[slots[j]]=rank[j].second;
            if(candidate!=a && solver.is_feasible(candidate))reset(std::move(candidate));
        }
        void prepare_reduction() {
            // 可動部分が小さく、構築時間を見込める場合だけ位置費用表を作る
            const int m=static_cast<int>(slots.size()),n=p.n_;
            if(m<2 || m>n/2)return;
            if(std::chrono::duration<double,std::micro>(end-Clock::now()).count() < static_cast<double>(m)*(n+m)/32.0+50.0)return;
            unary_table.resize(static_cast<size_t>(m)*m);
            fixed_violations.resize(unary_table.size());
            element_index.assign(a.size(),-1);
            std::vector<int> events(a.size());
            for(int index=0;index<m;++index){
                if(expired(end)){unary_table.clear();fixed_violations.clear();return;}
                int x=a[slots[index]];element_index[x]=index;
                // 行ごとに共通な定数を捨て、固定要素をまたぐ差分を累積する
                Cost fixed=0;int q=0;
                std::fill(row_cost.begin(),row_cost.end(),Cost(0));
                if(p.c_.empty())for(auto[y,c]:solver.relations_[x])row_cost[y]+=c;
                else for(int y=0;y<n;++y)row_cost[y]=solver.difference(x,y);
                std::fill(events.begin(),events.end(),0);
                for(auto[y,dir]:solver.precedence_relations_[x])if(!movable[y]){
                    q+=dir==1?slots[0]>=pos[y]:slots[0]<=pos[y];events[pos[y]]+=dir;
                }
                for(int j=0;j<m;++j){
                    if(j)for(int k=slots[j-1]+1;k<slots[j];++k)if(!movable[a[k]]){
                        fixed-=row_cost[a[k]];q+=events[k];
                    }
                    int absolute=slots[j];Cost v=fixed+p.u(x,absolute);
                    if(absolute>0&&!movable[a[absolute-1]])v+=p.t(a[absolute-1],x);
                    if(absolute+1<n&&!movable[a[absolute+1]])v+=p.t(x,a[absolute+1]);
                    size_t cell=static_cast<size_t>(index)*m+j;
                    unary_table[cell]=v;fixed_violations[cell]=q;
                }
            }
        }
        template<bool Full=false>
        Delta unary_delta(int x, int from, int to) const {
            int old = slots[from], next = slots[to];
            Delta d;
            d.domain = int(!p.domains_[x].contains(next))-int(!p.domains_[x].contains(old));
            if(!unary_table.empty()){
                size_t row=static_cast<size_t>(element_index[x])*slots.size();
                d.cost=unary_table[row+static_cast<size_t>(to)]-unary_table[row+static_cast<size_t>(from)];
                d.precedence=fixed_violations[row+static_cast<size_t>(to)]-fixed_violations[row+static_cast<size_t>(from)];
                return d;
            }
            d.cost = p.u(x,next)-p.u(x,old);
            if (Full || slots.size() == a.size()) return d;
            // 可動位置の間の固定要素と、固定側の隣接境界だけを加える
            int lo = std::min(old,next), hi = std::max(old,next);
            for (int k = lo+1; k < hi; ++k) if (!movable[a[k]]) {
                Cost diff = -solver.difference(x,a[k]);
                d.cost += old < next ? diff : -diff;
            }
            for (auto [y,dir] : solver.precedence_relations_[x]) if (!movable[y])
                d.precedence += dir == 1 ? int(next>=pos[y])-int(old>=pos[y]) : int(next<=pos[y])-int(old<=pos[y]);
            if (!p.t_.empty()) {
                auto boundary = [&](int j) {
                    Cost value = 0;
                    if (j>0 && !movable[a[j-1]]) value += p.t(a[j-1],x);
                    if (j+1<p.n_ && !movable[a[j+1]]) value += p.t(x,a[j+1]);
                    return value;
                };
                d.cost += boundary(next)-boundary(old);
            }
            return d;
        }
        template<bool Full,class Visit>
        void scan_insertions_impl(int from, Visit visit, bool prune) {
            const int m = static_cast<int>(slots.size()), x = a[slots[from]];
            // 選んだ要素の係数を作業行へ集める。疎入力でも候補ごとの表引きをしない
            std::fill(row_cost.begin(),row_cost.end(),Cost(0));
            std::fill(row_precedence.begin(),row_precedence.end(),0);
            if (p.c_.empty()) for (auto [y,c] : solver.relations_[x]) row_cost[y] += c;
            else for (int y = 0; y < p.n_; ++y) row_cost[y] = solver.difference(x,y);
            for (auto [y,dir] : solver.precedence_relations_[x]) row_precedence[y] += dir;
            for (int direction : {-1,1}) {
                Delta sum;
                for (int j = from+direction; j>=0 && j<m; j+=direction) {
                    if ((j&63)==0 && expired(end)) return;
                    int previous = j-direction, y = a[slots[j]];
                    auto dx = unary_delta<Full>(x,previous,j), dy = unary_delta<Full>(y,j,previous);
                    sum.cost += dx.cost+dy.cost+(direction==1 ? -row_cost[y] : row_cost[y]);
                    sum.precedence += dx.precedence+dy.precedence+direction*row_precedence[y];
                    sum.domain += dx.domain+dy.domain;
                    // 実行可能な状態では、ずらした要素の違反は挿入先を延ばしても消えない
                    if (prune && (dy.domain || sum.precedence)) break;
                    // 可動位置同士が元の配列でも隣接するときだけTを加える
                    if (!p.t_.empty()) {
                        int left=std::min(j,previous), right=left+1;
                        int before=direction==1?x:y, after=direction==1?y:x;
                        if (left>0 && slots[left-1]+1==slots[left]) {
                            int pred = direction==1 && previous!=from ? a[slots[previous]] : a[slots[left-1]];
                            sum.cost += p.t(pred,after)-p.t(pred,before);
                        }
                        if (slots[left]+1==slots[right]) sum.cost += p.t(after,before)-p.t(before,after);
                        if (right+1<m && slots[right]+1==slots[right+1]) {
                            int succ = direction==-1 && previous!=from ? a[slots[previous]] : a[slots[right+1]];
                            sum.cost += p.t(before,succ)-p.t(after,succ);
                        }
                    }
                    visit(j,sum);
                }
            }
        }
        // 実装を共有しつつ、全可動なら固定要素との計算をコンパイル時に省く
        template<class Visit>
        void scan_insertions(int from, Visit visit, bool prune=false) {
            if(slots.size()==a.size())scan_insertions_impl<true>(from,visit,prune);
            else scan_insertions_impl<false>(from,visit,prune);
        }
        void search() {
            const int m = static_cast<int>(slots.size());
            if (m < 2) return;
            prepare_reduction();
            std::vector<int> z, v;
            double scale = 1, total = 0;
            // 差分尺度と温度は定数費用に依存させず、残り時間に沿って下げる
            int samples = 0;
            const auto start = Clock::now();
            const double duration = std::max(1.0,std::chrono::duration<double,std::micro>(end-start).count());
            while (!expired(end)) {
                double progress = std::min(1.0,std::chrono::duration<double,std::micro>(Clock::now()-start).count()/duration);
                // 実行可能解を得るまでは従来の修復用温度・近傍を保つ
                bool repairing = best.empty();
                double temp = (repairing ? 0.3 : 1.0) * std::pow(0.003,progress);
                double penalty = 2.0 + 30.0*progress;
                auto energy = [&](const Delta& d) {
                    if (!allow_infeasible && !repairing && (vp+d.precedence || vd+d.domain)) return 1e100;
                    double dc = static_cast<double>(d.cost)/scale;
                    return repairing ? 0.01*dc + 2.0*(d.precedence+d.domain)
                                        : dc + penalty*(d.precedence+d.domain);
                };
                auto sample = [&](const Delta& d) {
                    if (samples < 64 && d.cost != 0) {
                        // 最大64サンプルを1/128で蓄積し、通常範囲の計算結果を保ったまま総和を小さくする
                        total += std::abs(static_cast<double>(d.cost))/128; ++samples;
                        scale = std::clamp(total/samples*128,std::numeric_limits<double>::denorm_min(),
                                           std::numeric_limits<double>::max());
                    }
                };
                // 挿入先の一括走査と、交換・許可巡回を同じ受理判定で扱う
                int i = rng.index(m), j = rng.index(m-1); if (j >= i) ++j;
                int kind = rng.index(100);
                Delta d;
                bool scanned = kind < 65;
                if (scanned) {
                    double best_energy = 0;
                    int target = -1;
                    scan_insertions(i,[&](int to,const Delta& candidate) {
                        sample(candidate); double e = energy(candidate);
                        if (e < best_energy) { best_energy=e; target=to; d=candidate; }
                    }, !allow_infeasible && !repairing && samples>=64);
                    if (target < 0) { if (kind<60) continue; scanned=false; }
                    else { insertion(i,target,z,v); begin(z,v); }
                }
                if (!scanned) {
                    if (kind < (repairing ? 75 : 95)) { z = {slots[i],slots[j]}; v = {a[slots[j]],a[slots[i]]}; }
                    else if (kind < 95) insertion(i,j,z,v);
                    else if (!cycle(z,v)) continue;
                    // 尺度推定後だけ早期棄却し、通常の受理判定と同じ乱数を消費する
                    if (!allow_infeasible && !repairing && samples>=64) {
                        bool valid=true;
                        for(size_t k=0;k<z.size();++k)if(!p.domains_[v[k]].contains(z[k])){valid=false;break;}
                        if(!valid){(void)rng.unit();continue;}
                    }
                    begin(z,v); d=delta(); if(interrupted)break; sample(d);
                }
                double de=energy(d);
                bool accept = de<=0 || (rng.unit()<std::exp(-de/std::max(0.01,temp)));
                if (accept) apply(d);
            }
        }
    };
    Result run(std::span<const int> initial, const Options& options, std::span<const uint8_t> movable,
               Clock::time_point start) {
        assert(options.time_limit_us >= 0);
        assert(movable.empty() || movable.size() == initial.size());
        auto available=std::chrono::duration_cast<std::chrono::microseconds>(Clock::time_point::max()-start).count();
        auto end = std::min(options.deadline,start + std::chrono::microseconds(std::min(options.time_limit_us,available)));
        State s(*this,initial,movable,options.seed,end);
        s.allow_infeasible=options.allow_infeasible_moves;
        // まず初期解を確保する。準備の途中で止まっても返却候補は失わない
        const bool initial_feasible=s.vp==0 && s.vd==0;
        const Cost initial_cost=s.cost;
        Status status = Status::NotFound;
        if ((s.slots.size()>=2 || !initial_feasible) && !expired(end) && prepare_until(end)) {
            if (cyclic_) status = Status::ProvenInfeasible;
            else {
                s.rank_initial();
                if ((s.best.empty() || (!p_.u_.empty() || !p_.t_.empty())) && s.construct(!s.best.empty()) < 0) status = Status::ProvenInfeasible;
                if (status != Status::ProvenInfeasible) s.search();
            }
        }
        Result result{status,std::move(s.best)};
        if (!result.order.empty() || p_.n_ == 0) {
            // 元問題で再検証し、浮動小数点の累積誤差で初期解を悪化させない
            result.status = Status::Feasible;
            result.cost = std::equal(result.order.begin(),result.order.end(),initial.begin(),initial.end()) ? initial_cost : evaluate(result.order);
            if (initial_feasible && initial_cost < result.cost) { result.order.assign(initial.begin(),initial.end()); result.cost = initial_cost; }
            bool valid=is_feasible(result.order);
            if(!movable.empty())for(int i=0;i<p_.n_;++i)
                valid=valid && (movable[initial[i]] || result.order[i]==initial[i]);
            if(!valid) {
                assert(false);
                if(initial_feasible){result.order.assign(initial.begin(),initial.end());result.cost=initial_cost;}
                else return {};
            }
            last_order_=result.order;last_cost_=result.cost;
            last_cost_version_=p_.cost_version_;last_constraint_version_=p_.constraint_version_;
        }
        return result;
    }
public:
    // 空のn要素問題を所有するsolverを作る O(n)
    explicit LinearOrderingSolver(int n) : p_(n) {}
    // 問題を所有するsolverを作る lvalueはO(入力量)、moveはO(1)
    explicit LinearOrderingSolver(Problem problem) : p_(std::move(problem)) {}
    // 所有する問題への設定窓口を返す O(1)
    Problem& problem() { return p_; }
    // 所有する問題を参照する O(1)
    const Problem& problem() const { return p_; }
    // 変更された問題依存の前処理を完了する 疎O(n+e+h)、密O(n²+h)
    void prepare() { (void)prepare_until(Clock::time_point::max()); }
    // 初期順序を生成して改善する 時間予算に応じた探索、最低限の採点は密O(n²)、疎O(n+e+h)
    Result solve(const Options& options = {}) {
        auto start = Clock::now();
        std::vector<int> initial(static_cast<size_t>(p_.n_)); std::iota(initial.begin(),initial.end(),0);
        return run(initial,options,{},start);
    }
    // 完全な順列を修復・改善する 時間予算に応じた探索、最低限の採点は密O(n²)、疎O(n+e+h)
    Result improve(std::span<const int> initial, const Options& options = {}, std::span<const uint8_t> movable = {}) {
        auto start = Clock::now(); assert(permutation(initial));
        return run(initial,options,movable,start);
    }
    // 完全な順列を最新の元費用で採点する 密O(n²)、疎O(n+e)
    Cost evaluate(std::span<const int> order) const {
        assert(permutation(order)); std::vector<int> pos(order.size());
        for (int i = 0; i < p_.n_; ++i) pos[order[i]] = i;
        return value(order,pos);
    }
    // 順列性と最新の制約を確認する O(n+h)
    bool is_feasible(std::span<const int> order) const {
        if (!permutation(order)) return false;
        std::vector<int> pos(order.size());
        for (int i = 0; i < p_.n_; ++i) pos[order[i]] = i;
        return violations(pos) == std::pair{0,0};
    }
};

#if __INCLUDE_LEVEL__ == 0
using Solver = LinearOrderingSolver<int64_t>;
struct Oracle {
    int n;
    std::vector<int64_t> c,u,t;
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<uint8_t>> allowed;
    int64_t constant = 0;
    explicit Oracle(int size) : n(size), c(static_cast<size_t>(n)*n),u(c.size()),t(c.size()),
        allowed(static_cast<size_t>(n),std::vector<uint8_t>(static_cast<size_t>(n),1)) {}
    std::tuple<int64_t,int,int> eval(const std::vector<int>& a) const {
        int64_t f = constant;
        int vp = 0,vd = 0;
        std::vector<int> pos(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            pos[a[i]] = i; vd += !allowed[a[i]][i];
            f += u[static_cast<size_t>(a[i])*n+i];
            if (i+1<n) f+=t[static_cast<size_t>(a[i])*n+a[i+1]];
            for (int j=i+1;j<n;++j) f+=c[static_cast<size_t>(a[i])*n+a[j]];
        }
        for(auto [x,y]:edges) vp += pos[x]>=pos[y];
        return {f,vp,vd};
    }
    void load(Solver& s,bool dense) const {
        auto& p=s.problem();
        if(dense) p.set_order_costs(c);
        else {
            std::vector<Solver::OrderTerm> terms;
            for(int i=0;i<n;++i) for(int j=0;j<n;++j)
                if(c[static_cast<size_t>(i)*n+j]) terms.push_back({i,j,c[static_cast<size_t>(i)*n+j]});
            p.set_order_terms(terms);
        }
        p.set_position_costs(u);p.set_adjacent_costs(t);p.set_precedences(edges);p.set_constant_cost(constant);
        for(int i=0;i<n;++i) {
            std::vector<int> d;
            for(int j=0;j<n;++j) if(allowed[i][j]) d.push_back(j);
            p.set_positions(i,d);
        }
    }
};
using Sol=LinearOrderingSolver<int64_t>;
void initialization_checks(){
 std::mt19937_64 rng(2961783);int checked=0,retained=0;
 for(int trial=0;trial<500;++trial){
  int n=3+int(rng()%12);Sol first(n),shifted(n);std::vector<int>a(n);std::iota(a.begin(),a.end(),0);std::shuffle(a.begin(),a.end(),rng);
  std::vector<int64_t>c(size_t(n)*n),u(c.size()),t(c.size()),us(c.size());
  for(auto*values:{&c,&u,&t})for(auto&v:*values)v=int64_t(rng()%61)-30;
  for(int x=0;x<n;++x){int64_t offset=int64_t(rng()%200001)-100000;for(int j=0;j<n;++j)us[size_t(x)*n+j]=u[size_t(x)*n+j]+offset;}
  for(Sol*s:{&first,&shifted}){s->problem().set_order_costs(c);s->problem().set_adjacent_costs(t);}
  first.problem().set_position_costs(u);shifted.problem().set_position_costs(us);
  std::vector<std::pair<int,int>>edges;
  for(int i=0;i<n;++i)for(int j=i+1;j<n;++j)if(rng()%9==0)edges.emplace_back(a[i],a[j]);
  for(Sol*s:{&first,&shifted})s->problem().set_precedences(edges);
  for(int j=0;j<n;++j){std::vector<int>d{j};for(int k=0;k<n;++k)if(rng()%3==0)d.push_back(k);for(Sol*s:{&first,&shifted})s->problem().set_positions(a[j],d);}
  std::vector<uint8_t> flags(n,1);if(trial%3==0)for(auto&f:flags)f=uint8_t(rng()%3!=0);
  first.prepare();shifted.prepare();uint64_t seed=rng();
  Sol::State s(first,a,flags,seed,Sol::Clock::time_point::max()),q(shifted,a,flags,seed,Sol::Clock::time_point::max());
  assert(s.vp==0&&s.vd==0);int status=s.construct(true),status2=q.construct(true);
  assert(status>=0&&status2==status);assert(s.a==q.a);
  assert(first.is_feasible(s.a)&&shifted.is_feasible(q.a));assert(s.cost==first.evaluate(s.a));assert(q.cost==shifted.evaluate(q.a));
  for(int j=0;j<n;++j){assert(s.pos[s.a[j]]==j);if(!flags[a[j]])assert(s.a[j]==a[j]);}
  if(status==0){assert(s.a==a);++retained;}++checked;
 }
 assert(retained>0);std::cout<<checked<<" cost-aware construction, feasibility, fixed-position and unary row-offset checks PASS; "<<retained<<" failed greedy attempts preserved the existing feasible state\n";
}

int main() {
    initialization_checks();
    std::mt19937_64 rng(728301);
    uint64_t checked=0;
    auto rnd=[&](int n){return static_cast<int>(rng()%static_cast<uint64_t>(n));};
    for(int trial=0;trial<650;++trial) {
        int n=2+rnd(15); Oracle o(n);
        for(auto* v:{&o.c,&o.u,&o.t}) for(auto& x:*v) x=rnd(31)-15;
        for(int i=0;i<n;++i) for(int j=i+1;j<n;++j) if(rnd(6)==0) o.edges.emplace_back(i,j);
        for(auto& row:o.allowed) for(auto& x:row) x=static_cast<uint8_t>(rnd(4)!=0);
        o.constant=rnd(100)-50;
        Solver s(n);o.load(s,trial%2);s.prepare();
        std::vector<int> a(static_cast<size_t>(n));std::iota(a.begin(),a.end(),0);std::shuffle(a.begin(),a.end(),rng);
        std::vector<uint8_t> flags(static_cast<size_t>(n));for(auto& x:flags)x=static_cast<uint8_t>(rnd(100)<(trial%3==0?20:trial%3==1?80:100));
        Solver::State state(s,a,flags,0,Solver::Clock::time_point::max());
        int m=static_cast<int>(state.slots.size());state.prepare_reduction();
        std::vector<int> z,v;
        auto check=[&](bool apply){
            auto after=state.a;
            for(size_t k=0;k<z.size();++k) after[z[k]]=v[k];
            auto [oldf,oldp,oldd]=o.eval(state.a); auto [newf,newp,newd]=o.eval(after);
            state.begin(z,v);auto d=state.delta();
            assert(d.cost==newf-oldf);assert(d.precedence==newp-oldp);assert(d.domain==newd-oldd);
            ++checked;
            if(apply){state.apply(d);assert(state.a==after);assert(state.cost==newf);assert(state.vp==newp);assert(state.vd==newd);}
        };
        for(int aidx=0;aidx<m;++aidx) for(int b=0;b<m;++b) if(aidx!=b) {
            state.insertion(aidx,b,z,v);check(false);
            z={state.slots[aidx],state.slots[b]};v={state.a[z[1]],state.a[z[0]]};check(false);
        }
        for(int from=0;from<m;++from) {
            state.scan_insertions(from,[&](int to,const Solver::Delta& d){
                state.insertion(from,to,z,v);
                auto after=state.a;for(size_t k=0;k<z.size();++k)after[z[k]]=v[k];
                auto [oldf,oldp,oldd]=o.eval(state.a);auto [newf,newp,newd]=o.eval(after);
                if(d.cost!=newf-oldf||d.precedence!=newp-oldp||d.domain!=newd-oldd){
                    std::cerr<<"scan mismatch trial "<<trial<<" from "<<from<<" to "<<to<<" costs "<<d.cost<<" "<<newf-oldf<<"\n";std::abort();
                }
                ++checked;
            });
        }
        for(int step=0;step<120;++step) {
            z=state.slots;v.clear();for(int j:z)v.push_back(state.a[j]);std::shuffle(v.begin(),v.end(),rng);check(rnd(2));
        }
        assert(s.evaluate(state.a)==std::get<0>(o.eval(state.a)));
    }
    // 小規模全列挙とAPIの意味・返却保証を比較する
    int solvable=0,found=0,impossible=0;
    for(int trial=0;trial<220;++trial) {
        int n=trial%8;Oracle o(n);Solver s(n);
        for(auto* v:{&o.c,&o.u,&o.t})for(auto& x:*v)x=rnd(41)-20;
        for(int i=0;i<n;++i) for(int j=i+1;j<n;++j) if(rnd(9)==0)o.edges.emplace_back(i,j);
        for(auto& row:o.allowed)for(auto& x:row)x=static_cast<uint8_t>(rnd(3)!=0);
        if(trial%7==0&&n>=2){o.edges.emplace_back(0,1);o.edges.emplace_back(1,0);std::sort(o.edges.begin(),o.edges.end());o.edges.erase(std::unique(o.edges.begin(),o.edges.end()),o.edges.end());}
        o.load(s,trial%2);
        std::vector<int>a(static_cast<size_t>(n)),witness;std::iota(a.begin(),a.end(),0);
        int64_t optimum=std::numeric_limits<int64_t>::max();bool exists=false;
        do {
            auto[f,vp,vd]=o.eval(a);assert(s.evaluate(a)==f);assert(s.is_feasible(a)==(vp==0&&vd==0));
            if(!vp&&!vd){exists=true;witness=a;optimum=std::min(optimum,f);}
        }while(std::next_permutation(a.begin(),a.end()));
        Solver::Options opt;opt.time_limit_us=1500;opt.seed=rng();auto r=s.solve(opt);
        if(exists){++solvable;assert(r.status!=Solver::Status::ProvenInfeasible);}
        else ++impossible;
        if(r.status==Solver::Status::Feasible){++found;assert(exists);assert(s.is_feasible(r.order));assert(r.cost==s.evaluate(r.order));assert(r.cost>=optimum);}
        else assert(r.order.empty());
        if(exists){auto q=s.improve(witness,opt);assert(q.status==Solver::Status::Feasible);assert(q.cost<=s.evaluate(witness));}
    }
    {
        Solver s(4);auto&p=s.problem();p.add_order_cost(0,1,10);p.add_order_cost(0,1,-3);p.set_order_cost(1,0,-5);
        p.add_position_cost(2,2,8);p.set_position_cost(2,2,6);p.add_adjacent_cost(0,1,4);p.set_adjacent_cost(0,1,3);
        p.add_constant_cost(9);p.set_constant_cost(1);
        std::vector<int>a{0,1,2,3};assert(s.evaluate(a)==17);
        s.prepare();p.set_order_cost(0,1,1);assert(s.evaluate(a)==11);p.set_order_cost(0,1,0);assert(s.evaluate(a)==10);
        p.clear_order_costs();p.clear_position_costs();p.clear_adjacent_costs();assert(s.evaluate(a)==1);
        p.require_before(0,1);p.require_before(0,1);assert(s.is_feasible(a));p.require_before(1,0);assert(!s.is_feasible(a));
        p.clear_precedences();assert(s.is_feasible(a));
        p.set_position_range(0,0,2);p.restrict_positions(0,std::vector<int>{0,2});assert(s.is_feasible(a));
        p.fix_position(0,1);assert(!s.is_feasible(a));p.clear_position_restriction(0);assert(s.is_feasible(a));
        p.set_positions(0,{});auto r=s.solve();assert(r.status==Solver::Status::ProvenInfeasible);
        p.set_positions(0,std::vector<int>{0});assert(s.is_feasible(a));
        Solver::Options opt;opt.time_limit_us=0;assert(s.improve(a,opt).order==a);
        p.set_position_range(0,3,4);assert(s.solve(opt).status==Solver::Status::NotFound);
        opt.time_limit_us=10000;opt.deadline=Solver::Clock::now()-std::chrono::seconds(1);assert(s.solve(opt).status==Solver::Status::NotFound);
        assert(!s.is_feasible(std::vector<int>{0,0,2,3}));assert(!s.is_feasible(std::vector<int>{-1,1,2,3}));
    }
    {
        Solver s(3);s.problem().add_preference(2,0,99);s.problem().fix_position(1,1);
        std::vector<int>a{0,1,2};std::vector<uint8_t>f{1,0,1};auto r=s.improve(a,{},f);assert((r.order==std::vector<int>{2,1,0}));
        s.problem().fix_position(0,0);auto bad=s.improve(r.order,{},std::vector<uint8_t>{0,1,1});assert(bad.status==Solver::Status::ProvenInfeasible);
    }
    {
        LinearOrderingSolver<double>s(7);std::vector<double> c(49),u(49),t(49);
        for(auto* v:{&c,&u,&t})for(auto&x:*v)x=static_cast<double>(rnd(1000)-500)/7;
        s.problem().set_order_costs(c);s.problem().set_position_costs(u);s.problem().set_adjacent_costs(t);
        auto r=s.solve();for(int k=0;k<10;++k){auto q=s.improve(r.order);assert(q.cost==s.evaluate(q.order));assert(q.cost<=r.cost);r=q;}
    }
    // 点更新・表の置換・固定の反復で、世代の古い情報が混ざらないことを確かめる
    for(int trial=0;trial<90;++trial){
        int n=3+rnd(7);Oracle o(n);Solver s(n);
        o.load(s,trial%2);std::vector<int>a(static_cast<size_t>(n));std::iota(a.begin(),a.end(),0);
        Solver::Options opt;opt.time_limit_us=300;
        for(int turn=0;turn<40;++turn){
            int i=rnd(n),j=rnd(n);int64_t v=rnd(31)-15;auto&p=s.problem();
            switch(turn%12){
                case 0:p.add_order_cost(i,j,v);o.c[static_cast<size_t>(i)*n+j]+=v;break;
                case 1:p.set_order_cost(i,j,v);o.c[static_cast<size_t>(i)*n+j]=v;break;
                case 2:p.add_position_cost(i,j,v);o.u[static_cast<size_t>(i)*n+j]+=v;break;
                case 3:p.set_position_cost(i,j,v);o.u[static_cast<size_t>(i)*n+j]=v;break;
                case 4:p.add_adjacent_cost(i,j,v);o.t[static_cast<size_t>(i)*n+j]+=v;break;
                case 5:p.set_adjacent_cost(i,j,v);o.t[static_cast<size_t>(i)*n+j]=v;break;
                case 6:p.add_constant_cost(v);o.constant+=v;break;
                case 7:p.set_constant_cost(v);o.constant=v;break;
                case 8:o.edges.clear();for(int k=0;k+1<n;++k)if(rnd(4)==0)o.edges.emplace_back(a[k],a[k+1]);p.set_precedences(o.edges);break;
                case 9:for(int x=0;x<n;++x){std::vector<int>d;for(int k=0;k<n;++k){o.allowed[x][k]=static_cast<uint8_t>(a[k]==x||rnd(3));if(o.allowed[x][k])d.push_back(k);}p.set_positions(x,d);}break;
                case 10:if((turn/12)%2)p.set_order_costs(o.c);else{std::vector<Solver::OrderTerm>terms;for(int x=0;x<n;++x)for(int y=0;y<n;++y){int64_t c=o.c[static_cast<size_t>(x)*n+y];terms.push_back({x,y,c-1});terms.push_back({x,y,1});}p.set_order_terms(terms);}break;
                case 11:p.clear_precedences();o.edges.clear();for(int x=0;x<n;++x){p.clear_position_restriction(x);std::fill(o.allowed[x].begin(),o.allowed[x].end(),1);}break;
            }
            assert(s.evaluate(a)==std::get<0>(o.eval(a)));assert(s.is_feasible(a));
            std::vector<uint8_t>fixed(static_cast<size_t>(n));for(auto&x:fixed)x=static_cast<uint8_t>(rnd(2));
            auto r=s.improve(a,opt,fixed);assert(r.status==Solver::Status::Feasible);assert(r.cost<=s.evaluate(a));assert(r.cost==std::get<0>(o.eval(r.order)));
            for(int k=0;k<n;++k)if(!fixed[a[k]])assert(r.order[k]==a[k]);
            a=r.order;
            if(turn%5==0){std::swap(a[rnd(n)],a[rnd(n)]);auto q=s.improve(a,opt);if(q.status==Solver::Status::Feasible)a=q.order;else{a=r.order;assert(s.is_feasible(a));}}
        }
    }
    // 長い巡回のみで移れる問題と固定を挟む先行制約を確認する
    for(int n:{4,7,13,31}){
        Solver s(n);auto&p=s.problem();std::vector<int>a(static_cast<size_t>(n));std::iota(a.begin(),a.end(),0);
        for(int x=0;x<n;++x){p.set_positions(x,std::vector<int>{x,(x+1)%n});p.set_position_cost(x,x,10);}
        Solver::Options opt;opt.time_limit_us=20000;auto r=s.improve(a,opt);assert(r.cost==0);assert(s.is_feasible(r.order));
    }
    {
        Solver s(5);auto&p=s.problem();p.require_before(0,2);p.require_before(2,4);
        std::vector<int>a{4,1,2,3,0};std::vector<uint8_t>flags{1,0,0,0,1};auto r=s.improve(a,{},flags);
        assert(r.status==Solver::Status::Feasible);assert(r.order[0]==0&&r.order[4]==4);
        auto copy=p;Solver t(copy);copy.add_constant_cost(99);assert(t.evaluate(r.order)==s.evaluate(r.order));
    }

    // C/U/Tを独立した部分集合DPで最適化し、返却費用の下界と照合する
    int dp_hits=0;
    for(int trial=0;trial<20;++trial){
        int n=8+trial%4;Oracle o(n);Solver s(n);
        for(auto* values:{&o.c,&o.u,&o.t})for(auto&x:*values)x=rnd(31)-15;
        o.load(s,trial%2);int count=1<<n;const int64_t inf=std::numeric_limits<int64_t>::max()/4;
        std::vector<int64_t>dp(static_cast<size_t>(count)*n,inf);
        for(int x=0;x<n;++x)dp[static_cast<size_t>(1<<x)*n+x]=o.u[static_cast<size_t>(x)*n];
        for(int mask=1;mask<count;++mask){
            int pos=std::popcount(static_cast<unsigned int>(mask));
            for(int last=0;last<n;++last)if(dp[static_cast<size_t>(mask)*n+last]!=inf)
                for(int x=0;x<n;++x)if(!(mask>>x&1)){
                    int64_t add=o.u[static_cast<size_t>(x)*n+pos]+o.t[static_cast<size_t>(last)*n+x];
                    for(int y=0;y<n;++y)if(mask>>y&1)add+=o.c[static_cast<size_t>(y)*n+x];
                    auto& dst=dp[static_cast<size_t>(mask|(1<<x))*n+x];dst=std::min(dst,dp[static_cast<size_t>(mask)*n+last]+add);
                }
        }
        int64_t optimum=*std::min_element(dp.end()-n,dp.end());Solver::Options opt;opt.time_limit_us=20000;auto r=s.solve(opt);
        assert(r.status==Solver::Status::Feasible&&r.cost>=optimum);dp_hits+=r.cost==optimum;
    }
    {
        Solver s(4);auto&p=s.problem();p.set_positions(0,std::vector<int>{0,2});p.set_positions(1,std::vector<int>{1,3});
        p.set_positions(2,std::vector<int>{1,2});p.set_positions(3,std::vector<int>{0,3});
        for(int x=0;x<4;++x)p.set_position_cost(x,x,10);
        auto r=s.improve(std::vector<int>{0,1,2,3});assert((r.order==std::vector<int>{3,2,0,1})&&r.cost==0);
    }
    std::cout<<"subset_dp_cases=20 optimal_hits="<<dp_hits<<'\n';

    // 浮動小数点の部分縮約と多要素差分を、独立した直接評価と比較する
    int floating_checks=0;
    for(int trial=0;trial<100;++trial){
        using DS=LinearOrderingSolver<double>;const int n=12;DS s(n);
        std::vector<double> c(n*n),u(n*n),t(n*n);
        for(auto* values:{&c,&u,&t})for(auto&x:*values)x=static_cast<double>(rnd(200)-100)/7.0;
        s.problem().set_order_costs(c);s.problem().set_position_costs(u);s.problem().set_adjacent_costs(t);s.prepare();
        std::vector<int>a(n);std::iota(a.begin(),a.end(),0);std::shuffle(a.begin(),a.end(),rng);
        std::vector<uint8_t>flags(n,0);for(int j=0;j<4;++j)flags[a[j*3]]=1;
        DS::State state(s,a,flags,1,DS::Clock::time_point::max());state.prepare_reduction();
        auto direct=[&](const std::vector<int>& order){
            double f=0;
            for(int j=0;j<n;++j){f+=u[static_cast<size_t>(order[j])*n+j];if(j+1<n)f+=t[static_cast<size_t>(order[j])*n+order[j+1]];for(int k=j+1;k<n;++k)f+=c[static_cast<size_t>(order[j])*n+order[k]];}
            return f;
        };
        for(int k=0;k<30;++k){
            auto z=state.slots;std::vector<int>v;for(int j:z)v.push_back(state.a[j]);std::shuffle(v.begin(),v.end(),rng);
            auto after=state.a;for(size_t j=0;j<z.size();++j)after[z[j]]=v[j];
            state.begin(z,v);auto delta=state.delta();assert(std::abs(delta.cost-(direct(after)-direct(state.a)))<1e-8);
            state.apply(delta);assert(std::abs(state.cost-direct(state.a))<1e-7);++floating_checks;
        }
    }
    // 目的値の定数移動に対する整合性を調べる
    for(int trial=0;trial<40;++trial){
        int n=2+rnd(7);Oracle o(n),shifted(n);for(auto* v:{&o.c,&o.u,&o.t})for(auto&x:*v)x=rnd(41)-20;
        shifted=o;int64_t offset=13;shifted.constant+=13;
        for(int i=0;i<n;++i)for(int j=i+1;j<n;++j){int64_t k=rnd(71)-35;shifted.c[static_cast<size_t>(i)*n+j]+=k;shifted.c[static_cast<size_t>(j)*n+i]+=k;offset+=k;}
        for(int i=0;i<n;++i){int64_t k=rnd(51)-25;offset+=k;for(int j=0;j<n;++j)shifted.u[static_cast<size_t>(i)*n+j]+=k;}
        for(auto& x:shifted.t)x+=7;
        offset+=7*(n-1);
        Solver a(n),b(n);o.load(a,trial%2);shifted.load(b,trial%2);std::vector<int>order(static_cast<size_t>(n));std::iota(order.begin(),order.end(),0);
        for(int k=0;k<30;++k){std::shuffle(order.begin(),order.end(),rng);assert(b.evaluate(order)-a.evaluate(order)==offset);}
    }
    std::cout<<"floating_delta_checks="<<floating_checks<<" constant_transform_checks=1200\n";

    // 部分固定を含めた全列挙で、修復・不能判定と返却保証を照合する
    int partial_cases=0;
    for(int trial=0;trial<300;++trial){
        int n=trial%8;Oracle o(n);Solver s(n);
        for(auto* v:{&o.c,&o.u,&o.t})for(auto&x:*v)x=rnd(21)-10;
        for(int x=0;x<n;++x)for(int y=0;y<n;++y)if(rnd(13)==0)o.edges.emplace_back(x,y);
        for(auto&row:o.allowed)for(auto&x:row)x=static_cast<uint8_t>(rnd(4)!=0);
        o.load(s,trial%2);
        std::vector<int> initial(static_cast<size_t>(n));std::iota(initial.begin(),initial.end(),0);std::shuffle(initial.begin(),initial.end(),rng);
        std::vector<uint8_t> flags(static_cast<size_t>(n));for(auto&x:flags)x=static_cast<uint8_t>(rnd(3)!=0);
        std::vector<int> candidate(static_cast<size_t>(n));std::iota(candidate.begin(),candidate.end(),0);
        bool exists=false;
        do{
            auto [f,vp,vd]=o.eval(candidate);(void)f;
            bool fixed=true;for(int j=0;j<n;++j)if(!flags[initial[j]]&&candidate[j]!=initial[j])fixed=false;
            exists=exists||(!vp&&!vd&&fixed);
        }while(std::next_permutation(candidate.begin(),candidate.end()));
        Solver::Options opt;opt.time_limit_us=trial%3==0?0:1000;opt.seed=rng();opt.allow_infeasible_moves=trial%2;
        auto r=s.improve(initial,opt,flags);
        if(exists)assert(r.status!=Solver::Status::ProvenInfeasible);
        if(r.status==Solver::Status::Feasible){
            assert(exists&&s.is_feasible(r.order));assert(r.cost==std::get<0>(o.eval(r.order)));
            for(int j=0;j<n;++j)if(!flags[initial[j]])assert(r.order[j]==initial[j]);
        }
        if(s.is_feasible(initial))assert(r.status==Solver::Status::Feasible&&r.cost<=s.evaluate(initial));
        ++partial_cases;
    }
    // 前処理の中断後も、新しい費用と制約で初期解を確認する
    {
        Solver s(3);std::vector<int>a{0,1,2};Solver::Options opt;opt.time_limit_us=0;
        s.problem().set_order_costs(std::vector<int64_t>{0,2,3,4,0,5,6,7,0});s.prepare();
        assert(s.improve(a,opt).cost==10);
        s.problem().set_order_terms(std::vector<Solver::OrderTerm>{{0,1,-11}});
        assert(!s.prepare_until(Solver::Clock::now()-std::chrono::seconds(1)));
        assert(s.improve(a,opt).cost==-11);s.prepare();
        s.problem().require_before(2,0);assert(s.improve(a,opt).status==Solver::Status::NotFound);
        s.problem().clear_precedences();assert(s.improve(a,opt).status==Solver::Status::Feasible);
    }
    std::cout<<"partial_exhaustive_cases="<<partial_cases<<" interrupted_prepare=PASS\n";
    std::cout<<"delta_checks="<<checked<<" small_solvable="<<solvable<<" found="<<found<<" impossible="<<impossible<<" PASS\n";
}

#endif
