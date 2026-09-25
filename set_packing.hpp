// 重み付き集合パッキング：競合辺・単位資源容量・必須/禁止を扱う単一スレッドの近似solver
// Nは1e7以下、M・重複除去前のCSR要素数はint内、整数重みの絶対値合計はLLONG_MAX/4以下、実数の絶対値合計は有限値
// time_limit_usは-1または0以上1e12以下。時間上限は初期化・単一操作・復元の完了分だけ超過し得る
#pragma once
#include <bits/stdc++.h>
using namespace std;

template<class Weight = long long>
struct SetPackingProblem {
    vector<Weight> weight;
    vector<pair<int, int>> conflicts;
    vector<vector<int>> resources;
    vector<int> capacity;
    vector<int8_t> fixed;
};

template<class Weight = long long>
struct SetPackingResult {
    bool feasible = false;
    Weight value{};
    vector<int> selected;
    double elapsed_us = 0;
    long long iterations = 0;
};

template<class Weight = long long>
class SetPackingSolver {
    static_assert(is_same_v<Weight, long long> || is_same_v<Weight, double>);
public:
    using Problem = SetPackingProblem<Weight>;
    using Result = SetPackingResult<Weight>;
    using Clock = chrono::steady_clock;
    struct Options {
        double time_limit_us = 1'950'000.0;
        optional<Clock::time_point> deadline;
        long long iteration_limit = -1;
    };
    struct Update {
        vector<pair<int, Weight>> weights;
        vector<pair<int, int>> capacities;
        vector<pair<int, int8_t>> fixed;
    };

private:
    struct Csr {
        vector<int> offset, data;
        span<const int> operator[](int i) const {
            return span<const int>(data).subspan((size_t)offset[i], (size_t)(offset[i + 1] - offset[i]));
        }
        void build(int size, const auto& visit, bool normalize = false) {
            offset.assign((size_t)size + 1, 0);
            visit([&](int i, int) { ++offset[i + 1]; });
            partial_sum(offset.begin(), offset.end(), offset.begin());
            data.resize((size_t)offset.back());
            auto cursor = offset;
            visit([&](int i, int j) { data[cursor[i]++] = j; });
            if (!normalize) return;
            int out = 0;
            for (int i = 0; i < size; ++i) {
                int first = offset[i], last = offset[i + 1];
                auto begin = data.begin() + first, end = data.begin() + last;
                sort(begin, end);
                end = unique(begin, end);
                offset[i] = out;
                for (auto it = begin; it != end; ++it) data[out++] = *it;
            }
            offset[size] = out;
            data.resize((size_t)out);
        }
    };
    struct Rng {
        uint64_t state = 1;
        uint64_t next() {
            auto z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }
        int index(int n) { return (int)((uint32_t)next() % (uint32_t)n); }
        double unit() { return (double)(next() >> 11) * 0x1.0p-53; }
    } rng_;

    int n_ = 0, m_ = 0, mandatory_ = 0;
    vector<Weight> weight_;
    vector<int> capacity_;
    vector<int8_t> fixed_;
    Csr adjacent_, resources_, members_;
    vector<int> owner_, location_, occupants_, use_, block_, position_, selected_, active_;
    vector<uint8_t> eligible_;
    vector<uint32_t> mark_;
    uint32_t stamp_ = 0, work_ = 0;
    vector<int> removed_, added_, pool_;
    vector<double> priority_;
    double scale_ = 0;
    Weight value_{};
    Result best_;
    bool dirty_ = true, weights_changed_ = false, stopped_ = false, constructed_ = false;
    long long steps_ = 0;
    Clock::time_point stop_ = Clock::time_point::max();

    uint32_t stamp() {
        if (++stamp_ == 0) {
            fill(mark_.begin(), mark_.end(), 0);
            ++stamp_;
        }
        return stamp_;
    }
    bool expired() {
        stopped_ = stopped_ || Clock::now() >= stop_;
        return stopped_;
    }
#if __INCLUDE_LEVEL__ == 0
    long long test_stop_after_ = -1;
#endif
    bool tick() {
#if __INCLUDE_LEVEL__ == 0
        if (test_stop_after_ >= 0 && test_stop_after_-- == 0) stopped_ = true;
#endif
        if ((++work_ & 63U) == 0) expired();
        return stopped_;
    }
    bool can_add(int i) const {
        if (position_[i] >= 0 || block_[i] != 0) return false;
        for (int r : resources_[i]) if (use_[r] >= capacity_[r]) return false;
        return true;
    }
    void add(int i) {
        position_[i] = (int)selected_.size();
        selected_.push_back(i);
        value_ += weight_[i];
        for (int j : adjacent_[i]) ++block_[j];
        for (int p = resources_.offset[i]; p < resources_.offset[i + 1]; ++p) {
            int r = resources_.data[p];
            int slot = members_.offset[r] + use_[r]++;
            occupants_[slot] = p;
            location_[p] = slot;
        }
    }
    void remove(int i) {
        int last = selected_.back();
        selected_[position_[i]] = last;
        position_[last] = position_[i];
        selected_.pop_back();
        position_[i] = -1;
        value_ -= weight_[i];
        for (int j : adjacent_[i]) --block_[j];
        for (int p = resources_.offset[i]; p < resources_.offset[i + 1]; ++p) {
            int r = resources_.data[p];
            int moved = occupants_[members_.offset[r] + --use_[r]];
            occupants_[location_[p]] = moved;
            location_[moved] = location_[p];
        }
    }
    void save_best() {
        if (best_.feasible && !(value_ > best_.value)) return;
        if constexpr (is_floating_point_v<Weight>) {
            value_ = evaluate(selected_);
            if (best_.feasible && !(value_ > best_.value)) return;
        }
        best_.feasible = true;
        best_.value = value_;
        best_.selected = selected_;
    }
    bool prepare(span<const int> hint) {
        // 必須候補だけで実行可能性を確定し、更新前の状態を破棄する
        fill(position_.begin(), position_.end(), -1);
        fill(block_.begin(), block_.end(), 0);
        fill(use_.begin(), use_.end(), 0);
        selected_.clear();
        value_ = Weight{};
        active_.clear();
        scale_ = 0;
        fill(eligible_.begin(), eligible_.end(), 0);
        for (int i = 0; i < n_; ++i) if (fixed_[i] == 1) {
            if (!can_add(i)) return false;
            add(i);
        }
        mandatory_ = (int)selected_.size();
        // 必須集合と両立する正重みの自由候補を残し、静的な構築順を用意する
        for (int i = 0; i < n_; ++i) if (fixed_[i] == -1 && weight_[i] > Weight{} && can_add(i)) {
            eligible_[i] = 1;
            active_.push_back(i);
            scale_ += (double)weight_[i];
            double pressure = 1.0 + (double)adjacent_[i].size();
            for (int r : resources_[i]) {
                pressure += (double)(members_[r].size() - 1) / (double)(capacity_[r] - use_[r]);
            }
            priority_[i] = (double)weight_[i] / pressure;
        }
        if (!active_.empty()) scale_ /= (double)active_.size();
        auto tag = stamp();
        for (int i : hint) {
            assert(0 <= i && i < n_ && mark_[i] != tag);
            mark_[i] = tag;
            if (eligible_[i] && can_add(i)) add(i);
        }
        constructed_ = false;
        steps_ = 0;
        return true;
    }
    void greedy() {
        // 圧迫度当たりの利益が高い順に、追加できる候補を採用する
        pool_ = active_;
        sort(pool_.begin(), pool_.end(), [&](int a, int b) {
            return priority_[a] != priority_[b] ? priority_[a] > priority_[b] : a < b;
        });
        for (int i : pool_) {
            if (tick()) break;
            if (can_add(i)) add(i);
        }
        save_best();
    }
    void refill() {
        pool_.clear();
        auto tag = stamp();
        auto push = [&](int i) {
            if (eligible_[i] && mark_[i] != tag) {
                mark_[i] = tag;
                if (can_add(i)) pool_.push_back(i);
            }
        };
        // 全候補に接続する資源でも、一度に調べる所属候補数を制限する
        for (int i : removed_) {
            push(i);
            auto scan = [&](span<const int> list) {
                int count = min((int)list.size(), 8);
                int first = count < (int)list.size() ? rng_.index((int)list.size()) : 0;
                for (int k = 0; k < count; ++k) {
                    if (tick()) break;
                    push(list[(size_t)first]);
                    if (++first == (int)list.size()) first = 0;
                }
            };
            scan(adjacent_[i]);
            for (int r : resources_[i]) if (use_[r] < capacity_[r]) scan(members_[r]);
            if (pool_.size() >= 512 || stopped_) break;
        }
        for (int k = (int)pool_.size() - 1; k > 0; --k) swap(pool_[k], pool_[rng_.index(k + 1)]);
        for (int i : pool_) {
            if (tick()) break;
            if (can_add(i)) { add(i); added_.push_back(i); }
        }
    }
    void exchange(int target, double temperature) {
        if (position_[target] >= 0) return;
        removed_.clear();
        added_.clear();
        Weight before = value_, delta = weight_[target];
        auto drop = [&](int i) { delta -= weight_[i]; remove(i); removed_.push_back(i); };
        // 明示的な競合相手は全て除去する
        for (int j : adjacent_[target]) if (position_[j] >= 0) drop(j);
        // 容量が不足する資源から、小さい重みの自由候補をサンプルで探す
        for (int r : resources_[target]) if (use_[r] >= capacity_[r]) {
            int chosen = -1;
            int count = min(use_[r], 8);
            int offset = count < use_[r] ? rng_.index(use_[r]) : 0;
            for (int k = 0; k < count; ++k) {
                int p = occupants_[members_.offset[r] + offset];
                if (++offset == use_[r]) offset = 0;
                int j = owner_[p];
                if (fixed_[j] == 1) continue;
                if (chosen < 0 || weight_[j] < weight_[chosen]) chosen = j;
            }
            // サンプルが必須候補だけだった場合も、必須は決して外さない
            if (chosen < 0) {
                for (int k = 0; k < use_[r]; ++k) {
                    int j = owner_[occupants_[members_.offset[r] + k]];
                    if (fixed_[j] != 1) { chosen = j; break; }
                    if (tick()) break;
                }
            }
            if (chosen < 0 || tick()) {
                for (int j : removed_) add(j);
                value_ = before;
                return;
            }
            drop(chosen);
        }
        add(target);
        added_.push_back(target);
        refill();
        for (size_t k = 1; k < added_.size(); ++k) delta += weight_[added_[k]];
        bool accept = delta > Weight{};
        if (!accept && temperature > 0) accept = rng_.unit() < exp((double)delta / temperature);
        if (accept) { save_best(); return; }
        // 提案全体を逆順に戻し、実数の累積誤差も元の値へ戻す
        for (auto it = added_.rbegin(); it != added_.rend(); ++it) remove(*it);
        for (auto it = removed_.rbegin(); it != removed_.rend(); ++it) add(*it);
        value_ = before;
    }
    const Result& run(const Options& options, span<const int> hint, bool supplied, Clock::time_point begin) {
        assert(isfinite(options.time_limit_us) && (options.time_limit_us >= 0 || options.time_limit_us == -1));
        assert(options.iteration_limit >= -1);
        assert(options.time_limit_us >= 0 || options.deadline || options.iteration_limit >= 0);
        stop_ = options.deadline.value_or(Clock::time_point::max());
        if (options.time_limit_us >= 0) stop_ = min(stop_, begin + chrono::duration_cast<Clock::duration>(chrono::duration<double, micro>(options.time_limit_us)));
        stopped_ = false; work_ = 0;
        // 更新後は旧解を修復し、通常のimproveでは既存の最良解も保持する
        vector<int> previous;
        if (supplied) previous.assign(hint.begin(), hint.end());
        if (dirty_) {
            vector<int> old = move(best_.selected);
            best_ = Result{};
            if (supplied && old.empty()) { old = move(previous); supplied = false; }
            if (prepare(old)) save_best();
            dirty_ = false;
        }
        else if (weights_changed_ && best_.feasible) {
            // 制約と有効候補が同じなら、選択状態を保って目的値だけ再評価する
            value_ = evaluate(selected_);
            best_.value = evaluate(best_.selected);
            save_best();
        }
        weights_changed_ = false;
        if (supplied && best_.feasible && prepare(previous)) save_best();
        long long iterations = 0;
        if (best_.feasible && !active_.empty() && options.iteration_limit != 0 && !expired()) {
            if (!constructed_) { greedy(); constructed_ = true; }
            int cycle = max(256, 8 * (int)active_.size());
            double temperature = scale_ * 0.3 * pow(0.01, (double)(steps_ % cycle) / (double)cycle);
            while (!stopped_ && selected_.size() < active_.size() + (size_t)mandatory_ &&
                   (options.iteration_limit < 0 || iterations < options.iteration_limit)) {
                if ((iterations & 31) == 0 && expired()) break;
                ++iterations;
                int target = active_[(size_t)rng_.index((int)active_.size())];
                if ((steps_++ & 31) == 0) temperature = scale_ * 0.3 * pow(0.01, (double)(steps_ % cycle) / (double)cycle);
                exchange(target, temperature);
            }
        }
        best_.iterations = iterations;
        best_.elapsed_us = chrono::duration<double, micro>(Clock::now() - begin).count();
        return best_;
    }

#if __INCLUDE_LEVEL__ == 0
public:
    static long long test_internals() {
        mt19937_64 random(8721);
        auto draw = [&](int n) { return (int)(random() % (uint64_t)n); };
        long long checks = 0;
        auto equal = [](Weight a, Weight b) {
            if constexpr (is_floating_point_v<Weight>) return abs(a - b) <= 1e-9 * max(1.0, abs((double)b));
            else return a == b;
        };
        for (int trial = 0; trial < 160; ++trial) {
            Problem p;
            int n = 8 + draw(40), m = 1 + draw(15);
            p.weight.resize((size_t)n); p.resources.resize((size_t)n);
            p.capacity.resize((size_t)m); p.fixed.assign((size_t)n, -1);
            for (auto& w : p.weight) {
                w = (Weight)(1 + draw(100));
                if constexpr (is_floating_point_v<Weight>) w /= 7.0;
            }
            for (int& c : p.capacity) c = 1 + draw(5);
            for (int i = 0; i < n; ++i) {
                for (int r = 0; r < m; ++r) if (draw(5) == 0) p.resources[i].push_back(r);
                for (int j = 0; j < i; ++j) if (draw(14) == 0) p.conflicts.emplace_back(i, j);
            }
            if (trial % 3 == 0) p.fixed[0] = 1;
            SetPackingSolver s(p, (uint64_t)trial + 100);
            Options o; o.time_limit_us = -1; o.iteration_limit = 0;
            s.solve_view(o);
            s.greedy();
            auto verify = [&] {
                vector<int> positions((size_t)n, -1), blocks((size_t)n), used((size_t)m);
                Weight sum{};
                for (int k = 0; k < (int)s.selected_.size(); ++k) {
                    int i = s.selected_[k];
                    assert(positions[i] < 0); positions[i] = k;
                    sum += p.weight[i];
                    for (auto [a, b] : p.conflicts) { if (a == i) ++blocks[b]; if (b == i) ++blocks[a]; }
                    for (int r : p.resources[i]) ++used[r];
                    for (int at = s.resources_.offset[i]; at < s.resources_.offset[i + 1]; ++at) {
                        int r = s.resources_.data[at], location = s.location_[at];
                        assert(location >= s.members_.offset[r] && location < s.members_.offset[r] + s.use_[r]);
                        assert(s.occupants_[location] == at && s.owner_[at] == i);
                    }
                }
                assert(positions == s.position_ && blocks == s.block_ && used == s.use_);
                assert(equal(sum, s.value_));
                assert(s.is_feasible(s.selected_));
                assert(s.best_.feasible && s.is_feasible(s.best_.selected));
                assert(equal(s.best_.value, s.evaluate(s.best_.selected)));
                for (int i = 0; i < n; ++i) {
                    bool possible = positions[i] < 0 && blocks[i] == 0;
                    for (int r : p.resources[i]) possible = possible && used[r] < p.capacity[r];
                    assert(s.can_add(i) == possible);
                }
                ++checks;
            };
            verify();
            for (int step = 0; step < 700; ++step) {
                s.stopped_ = false;
                s.test_stop_after_ = step % 4 == 0 ? draw(35) : -1;
                if (!s.active_.empty()) {
                    int target = s.active_[(size_t)draw((int)s.active_.size())];
                    s.exchange(target, step % 5 == 0 ? numeric_limits<double>::infinity() : 0.0);
                }
                verify();
                if (step % 29 == 0) { while (!s.selected_.empty()) s.remove(s.selected_.back()); s.value_ = Weight{}; for (int i : s.best_.selected) s.add(i); s.value_ = s.best_.value; verify(); }
            }
            s.stamp_ = UINT32_MAX;
            assert(s.stamp() == 1);
            assert(all_of(s.mark_.begin(), s.mark_.end(), [](uint32_t x) { return x == 0; }));
        }
        // 重み単独更新のキャッシュと、同値更新が探索状態を変えないことを確認する
        for (int trial = 0; trial < 60; ++trial) {
            Problem p;
            int n = 12 + draw(24), m = n + 7;
            p.weight.resize((size_t)n); p.resources.resize((size_t)n);
            p.capacity.assign((size_t)m, 2); p.fixed.assign((size_t)n, -1);
            p.fixed[0] = 1;
            for (int i = 0; i < n; ++i) {
                p.weight[i] = (Weight)(1 + draw(100));
                if constexpr (is_floating_point_v<Weight>) p.weight[i] /= 7.0;
                p.resources[i] = {i % 5, m - 1};
            }
            p.capacity.back() = n / 2;
            SetPackingSolver s(p, (uint64_t)trial + 193);
            Options work; work.time_limit_us = -1; work.iteration_limit = 80;
            Options no_work = work; no_work.iteration_limit = 0;
            s.solve(work);
            for (int turn = 0; turn < 60; ++turn) {
                Update u;
                int i = draw(n);
                Weight w = (Weight)(draw(110) - 5);
                if constexpr (is_floating_point_v<Weight>) w /= 7.0;
                u.weights = {{i, w}}; p.weight[i] = w;
                Weight lower{};
                for (int j : s.best_.selected) lower += p.weight[j];
                s.update(u);
                auto r = s.solve(no_work);
                assert(r.feasible && s.is_feasible(r.selected));
                assert(r.value >= lower || equal(r.value, lower));
                assert(equal(s.value_, s.evaluate(s.selected_)));
                double total = 0;
                vector<int> mandatory_use((size_t)m);
                for (int j = 0; j < n; ++j) if (p.fixed[j] == 1)
                    for (int resource : p.resources[j]) ++mandatory_use[resource];
                for (int j : s.active_) {
                    total += (double)p.weight[j];
                    double pressure = 1.0 + (double)s.adjacent_[j].size();
                    for (int resource : p.resources[j])
                        pressure += (double)(s.members_[resource].size() - 1) / (p.capacity[resource] - mandatory_use[resource]);
                    double expected = (double)p.weight[j] / pressure;
                    assert(abs(s.priority_[j] - expected) < 1e-10 * max(1.0, expected));
                }
                if (!s.active_.empty()) total /= (double)s.active_.size();
                assert(abs(s.scale_ - total) < 1e-10 * max(1.0, total));
                s.solve(work);
                SetPackingSolver unchanged = s;
                u = {}; u.weights = {{i, p.weight[i]}};
                u.capacities = {{m - 1, p.capacity.back()}}; u.fixed = {{i, p.fixed[i]}};
                s.update(u);
                assert(!s.dirty_ && !s.weights_changed_);
                auto a = s.solve(work), b = unchanged.solve(work);
                assert(a.value == b.value && a.selected == b.selected && a.iterations == b.iterations);
                assert(s.selected_ == unchanged.selected_ && s.rng_.state == unchanged.rng_.state && s.steps_ == unchanged.steps_);
                ++checks;
            }
            auto state = s.rng_.state;
            s.rebuild(p);
            assert(s.rng_.state == state);
        }
        if constexpr (is_same_v<Weight, long long>) {
            // 除去対象の比較でも整数をdoubleへ丸めない
            Problem p; p.weight = {9007199254740993LL, 9007199254740992LL, 9007199254740994LL};
            p.resources = {{0}, {0}, {0}}; p.capacity = {2};
            SetPackingSolver s(p);
            Options o; o.time_limit_us = -1; o.iteration_limit = 0;
            s.improve(vector<int>{0, 1}, o);
            s.exchange(2, 0);
            assert(s.position_[0] >= 0 && s.position_[1] < 0 && s.position_[2] >= 0);
            assert(s.best_.value == p.weight[0] + p.weight[2]);
            ++checks;
        }
        // 再充填時は、重複する所属からも実行可能な候補だけを一度集める
        {
            Problem p; p.weight = {100, 20, 10, 9, 8, 7};
            p.resources = {{0}, {1, 2}, {1}, {0}, {2}, {2}}; p.capacity = {1, 1, 1};
            p.conflicts = {{1, 2}, {1, 3}, {1, 4}, {1, 5}};
            SetPackingSolver s(p, 43);
            Options o; o.time_limit_us = -1; o.iteration_limit = 0;
            s.improve(vector<int>{0, 1}, o);
            s.remove(1); s.add(2); s.removed_ = {1}; s.added_.clear();
            s.refill();
            auto pool = s.pool_; sort(pool.begin(), pool.end());
            assert((pool == vector<int>{4, 5}) && s.added_.size() == 1);
            assert(s.position_[0] >= 0 && s.position_[2] >= 0 && s.position_[1] < 0 && s.position_[3] < 0);
            assert(s.is_feasible(s.selected_) && equal(s.value_, s.evaluate(s.selected_)));
            assert(s.solve_view(o).value == (Weight)120);
            ++checks;
        }
        // 折り返し走査：資源リストの端を跨いでも、元の巡回サンプルと一致する
        for (int size : {1, 8, 9, 17, 36}) for (uint64_t seed = 1; seed <= 80; ++seed) {
            Problem p; p.weight.assign((size_t)size, (Weight)1);
            p.resources.assign((size_t)size, vector<int>{0}); p.capacity = {1};
            SetPackingSolver s(p, seed);
            Options o; o.time_limit_us = -1; o.iteration_limit = 0;
            s.improve(vector<int>{0}, o);
            s.remove(0); s.removed_ = {0}; s.added_.clear();
            auto random_copy = s.rng_;
            int count = min(size, 8), first = count < size ? random_copy.index(size) : 0;
            vector<int> expected{0};
            for (int k = 0; k < count; ++k) expected.push_back((first + k) % size);
            sort(expected.begin(), expected.end()); expected.erase(unique(expected.begin(), expected.end()), expected.end());
            s.refill();
            auto actual = s.pool_; sort(actual.begin(), actual.end());
            assert(actual == expected && s.added_.size() == 1);
            assert(s.is_feasible(s.selected_) && equal(s.value_, s.evaluate(s.selected_)));
            ++checks;
        }
        // 占有候補の巡回サンプルでも端を跨ぐ場合の最小重みを選ぶ
        for (int capacity : {8, 9, 17, 33}) for (uint64_t seed = 1; seed <= 80; ++seed) {
            Problem p; p.weight.resize((size_t)capacity + 1);
            iota(p.weight.begin(), p.weight.end(), (Weight)1);
            p.resources.assign((size_t)capacity + 1, vector<int>{0}); p.capacity = {capacity};
            vector<int> initial((size_t)capacity); iota(initial.begin(), initial.end(), 0);
            SetPackingSolver s(p, seed);
            Options o; o.time_limit_us = -1; o.iteration_limit = 0;
            s.improve(initial, o);
            auto random_copy = s.rng_;
            int count = min(capacity, 8), first = count < capacity ? random_copy.index(capacity) : 0;
            int victim = capacity;
            for (int k = 0; k < count; ++k) victim = min(victim, (first + k) % capacity);
            s.exchange(capacity, 0);
            assert(s.position_[victim] < 0 && s.position_[capacity] >= 0);
            assert((int)s.selected_.size() == capacity && s.is_feasible(s.selected_));
            assert(equal(s.value_, s.evaluate(s.selected_)));
            ++checks;
        }
        // 未整列・重複・空行を含め、3つのCSRを独立な行リストと照合する。
        // 大小の問題を同じインスタンスへrebuildし、旧サイズの状態も残らないことを確認。
        {
            SetPackingSolver s(Problem{}, 918273);
            mt19937_64 input(491201);
            auto draw_input = [&](int limit) { return (int)(input() % (uint64_t)limit); };
            for (int trial = 0; trial < 600; ++trial) {
                Problem p;
                int n = trial % 11 == 0 ? 0 : draw_input(90), m = draw_input(40);
                p.weight.assign((size_t)n, (Weight)1);
                p.resources.resize((size_t)n); p.capacity.resize((size_t)m);
                for (int& c : p.capacity) c = draw_input(8);
                vector<vector<int>> adjacent((size_t)n), resources((size_t)n), members((size_t)m);
                for (int i = 0; i < n; ++i) {
                    for (int j = 0; j < i; ++j) if (draw_input(12) == 0) {
                        int copies = 1 + draw_input(4);
                        for (int k = 0; k < copies; ++k) p.conflicts.emplace_back(k % 2 ? i : j, k % 2 ? j : i);
                        adjacent[i].push_back(j); adjacent[j].push_back(i);
                    }
                    if (m && trial % 9 != 0) for (int k = draw_input(25); k > 0; --k) p.resources[i].push_back(draw_input(m));
                    resources[i] = p.resources[i];
                    sort(resources[i].begin(), resources[i].end());
                    resources[i].erase(unique(resources[i].begin(), resources[i].end()), resources[i].end());
                    for (int r : resources[i]) members[r].push_back(i);
                }
                shuffle(p.conflicts.begin(), p.conflicts.end(), input);
                if (trial % 9 == 0) p.resources.clear();
                auto state = s.rng_.state;
                s.rebuild(p);
                assert(s.rng_.state == state);
                auto check = [&](const Csr& csr, const vector<vector<int>>& rows) {
                    assert(csr.offset.size() == rows.size() + 1 && csr.offset.front() == 0);
                    int size = 0;
                    for (int i = 0; i < (int)rows.size(); ++i) {
                        assert(csr.offset[i] == size);
                        assert(ranges::equal(csr[i], rows[i]));
                        size += (int)rows[i].size();
                    }
                    assert(csr.offset.back() == size && (int)csr.data.size() == size);
                };
                for (auto& row : adjacent) sort(row.begin(), row.end());
                check(s.adjacent_, adjacent); check(s.resources_, resources); check(s.members_, members);
                for (int i = 0; i < n; ++i) for (int at = s.resources_.offset[i]; at < s.resources_.offset[i + 1]; ++at) assert(s.owner_[at] == i);
                Options zero; zero.time_limit_us = -1; zero.iteration_limit = 0;
                const auto& result = s.solve_view(zero);
                assert(result.feasible && result.value == Weight{} && result.selected.empty());
                ++checks;
            }
        }
        // prepareは追加だけを行うため、整数・実数とも選択順の合計と厳密に一致する。
        // fixedの省略・明示とサイズ変更を交互に行い、前の固定指定が残らないことも検査。
        {
            SetPackingSolver s(Problem{}, 8173);
            for (int trial = 0; trial < 600; ++trial) {
                Problem p;
                int n = trial % 11 == 0 ? 0 : draw(90);
                p.weight.resize((size_t)n);
                for (auto& w : p.weight) {
                    w = (Weight)(draw(201) - 100);
                    if constexpr (is_floating_point_v<Weight>) w /= 7.0;
                }
                if (trial % 3) {
                    p.fixed.resize((size_t)n);
                    for (auto& f : p.fixed) f = (int8_t)(draw(3) - 1);
                }
                vector<int> hint((size_t)n), expected;
                iota(hint.begin(), hint.end(), 0);
                shuffle(hint.begin(), hint.end(), random);
                if (trial % 2) hint.resize(hint.size() / 2);
                auto flag = [&](int i) { return p.fixed.empty() ? -1 : p.fixed[i]; };
                for (int i = 0; i < n; ++i) if (flag(i) == 1) expected.push_back(i);
                for (int i : hint) if (flag(i) == -1 && p.weight[i] > Weight{}) expected.push_back(i);
                s.rebuild(p);
                assert(s.prepare(hint) && s.selected_ == expected);
                Weight total{};
                for (int i : expected) total += p.weight[i];
                assert(s.value_ == total && s.evaluate(s.selected_) == total);
                for (int i = 0; i < n; ++i) assert(s.fixed_[i] == flag(i));
                ++checks;
            }
        }
        return checks;
    }
private:
#endif

public:
    // 問題を所有し、重複を正規化して構築する O(N+M+E+L+隣接ID・資源IDの行ごとのソート量)
    explicit SetPackingSolver(const Problem& problem, uint64_t initial_seed = 1) {
        rng_.state = initial_seed;
        rebuild(problem);
    }
    // 問題を置換し、保存解を破棄する。乱数状態は維持する O(N+M+E+L+隣接ID・資源IDの行ごとのソート量)
    void rebuild(const Problem& problem) {
        n_ = (int)problem.weight.size(); m_ = (int)problem.capacity.size();
        assert(problem.resources.empty() || (int)problem.resources.size() == n_);
        assert(problem.fixed.empty() || (int)problem.fixed.size() == n_);
        weight_ = problem.weight; capacity_ = problem.capacity;
        fixed_ = problem.fixed; fixed_.resize((size_t)n_, -1);
        for (int i = 0; i < n_; ++i) {
            assert(-1 <= fixed_[i] && fixed_[i] <= 1);
            if constexpr (is_floating_point_v<Weight>) assert(isfinite(weight_[i]));
        }
        for (int c : capacity_) { assert(c >= 0); (void)c; }
        // 直接CSRへ詰め、各行のソートと重複除去で正規化する
        adjacent_.build(n_, [&](auto emit) {
            for (auto [i, j] : problem.conflicts) {
                assert(0 <= i && i < n_ && 0 <= j && j < n_ && i != j);
                emit(i, j); emit(j, i);
            }
        }, true);
        resources_.build(n_, [&](auto emit) {
            for (int i = 0; i < (int)problem.resources.size(); ++i) for (int r : problem.resources[i]) {
                assert(0 <= r && r < m_);
                emit(i, r);
            }
        }, true);
        owner_.resize(resources_.data.size());
        for (int i = 0; i < n_; ++i) for (int p = resources_.offset[i]; p < resources_.offset[i + 1]; ++p) owner_[p] = i;
        members_.build(m_, [&](auto emit) {
            for (int p = 0; p < (int)owner_.size(); ++p) emit(resources_.data[p], owner_[p]);
        });
        // 状態と作業配列の容量を確保し、次の探索で必須集合を判定する
        location_.resize(owner_.size()); occupants_.resize(owner_.size());
        use_.resize((size_t)m_); block_.resize((size_t)n_); position_.resize((size_t)n_);
        eligible_.resize((size_t)n_); priority_.resize((size_t)n_);
        mark_.assign((size_t)max(n_, m_), 0); stamp_ = 0;
        selected_.clear(); selected_.reserve((size_t)n_);
        best_ = Result{}; dirty_ = true; weights_changed_ = false;
    }
    // 乱数状態だけを設定する O(1)
    void seed(uint64_t value) { rng_.state = value; }
    // 数値と固定指定を一括変更する 通常O(変更件数)、再評価・必要な再初期化は次回探索時
    void update(const Update& changes) {
        // 制約や正重み候補の集合が変わる場合だけ、全体を再初期化する
        auto tag = stamp();
        for (auto [r, c] : changes.capacities) {
            assert(0 <= r && r < m_ && c >= 0 && mark_[r] != tag); mark_[r] = tag;
            dirty_ = dirty_ || capacity_[r] != c;
            capacity_[r] = c;
        }
        tag = stamp();
        for (auto [i, f] : changes.fixed) {
            assert(0 <= i && i < n_ && -1 <= f && f <= 1 && mark_[i] != tag); mark_[i] = tag;
            dirty_ = dirty_ || fixed_[i] != f;
            fixed_[i] = f;
        }
        tag = stamp();
        for (auto [i, w] : changes.weights) {
            assert(0 <= i && i < n_ && mark_[i] != tag); mark_[i] = tag;
            if constexpr (is_floating_point_v<Weight>) assert(isfinite(w));
            if (weight_[i] == w) continue;
            weights_changed_ = true;
            dirty_ = dirty_ || (fixed_[i] == -1 && (weight_[i] > Weight{}) != (w > Weight{}));
            if (!dirty_ && eligible_[i]) {
                priority_[i] = (priority_[i] / (double)weight_[i]) * (double)w;
                scale_ += ((double)w - (double)weight_[i]) / (double)active_.size();
            }
            weight_[i] = w;
        }
    }
    // 探索を継続し、次の非const操作まで有効な結果参照を返す O(初期化+探索)、参照返却はO(1)
    const Result& solve_view(const Options& options) { return run(options, {}, false, Clock::now()); }
    // 探索を継続し、所有する結果を返す O(初期化+探索+選択数)
    Result solve(const Options& options) {
        auto begin = Clock::now();
        Result result = run(options, {}, false, begin);
        result.elapsed_us = chrono::duration<double, micro>(Clock::now() - begin).count();
        return result;
    }
    // 初期候補集合を必要なら修復して改善する。実行可能な初期解を悪化させない O(初期化+探索+選択数)
    Result improve(span<const int> initial_selected, const Options& options) {
        auto begin = Clock::now();
        Result result = run(options, initial_selected, true, begin);
        result.elapsed_us = chrono::duration<double, micro>(Clock::now() - begin).count();
        return result;
    }
    // 現在の重みの合計を返す O(選択数)、assert有効時は重複検出を含めO(N+選択数)
    Weight evaluate(span<const int> selected) const {
        Weight value{};
#ifndef NDEBUG
        vector<uint8_t> seen((size_t)n_, 0);
#endif
        for (int i : selected) {
            assert(0 <= i && i < n_);
#ifndef NDEBUG
            assert(!seen[i]); seen[i] = 1;
#endif
            value += weight_[i];
        }
        return value;
    }
    // 固定・競合・容量の制約を確認する O(N+M+選択候補の次数と資源数の合計)
    bool is_feasible(span<const int> selected) const {
        // 選択集合から制約の使用量を再構成し、最後に必須候補の欠落を調べる
        vector<uint8_t> chosen((size_t)n_, 0);
        vector<int> usage((size_t)m_, 0);
        for (int i : selected) {
            assert(0 <= i && i < n_ && !chosen[i]);
            if (fixed_[i] == 0) return false;
            chosen[i] = 1;
            for (int j : adjacent_[i]) if (chosen[j]) return false;
            for (int r : resources_[i]) if (++usage[r] > capacity_[r]) return false;
        }
        for (int i = 0; i < n_; ++i) if (fixed_[i] == 1 && !chosen[i]) return false;
        return true;
    }
};

#if __INCLUDE_LEVEL__ == 0
int main(int argc, char** argv) {
    using Solver = SetPackingSolver<>;
    using Problem = Solver::Problem;
    Solver::Options fixed_work; fixed_work.time_limit_us = -1; fixed_work.iteration_limit = 500;
    Solver::Options no_work; no_work.time_limit_us = -1; no_work.iteration_limit = 0;
    // 別プロセスから呼び、不正入力がassertで停止することを確認する
    if (argc == 2) {
        int mode = stoi(argv[1]);
        Problem p; p.weight = {1, 2}; p.resources = {{0}, {0}}; p.capacity = {1};
        if (mode == 1) p.conflicts = {{0, 0}};
        if (mode == 2) p.resources[0] = {1};
        if (mode == 3) p.capacity[0] = -1;
        if (mode == 4) p.fixed = {2, -1};
        if (mode == 5) p.fixed = {-1};
        if (mode == 6) p.resources.resize(1);
        Solver s(p);
        if (mode == 7) { Solver::Update u; u.weights = {{0, 1}, {0, 2}}; s.update(u); }
        if (mode == 8) { Solver::Update u; u.capacities = {{0, 1}, {0, 2}}; s.update(u); }
        if (mode == 9) { Solver::Update u; u.fixed = {{0, 0}, {0, 1}}; s.update(u); }
        if (mode == 10) { auto o = no_work; o.time_limit_us = -2; s.solve(o); }
        if (mode == 11) { auto o = no_work; o.iteration_limit = -1; s.solve(o); }
        if (mode == 12) { vector<int> ids{0, 0}; s.improve(ids, no_work); }
        if (mode == 13) { vector<int> ids{2}; s.evaluate(ids); }
        if (mode == 14) { vector<int> ids{0, 0}; s.is_feasible(ids); }
        if (mode == 15) { SetPackingProblem<double> q; q.weight = {numeric_limits<double>::infinity()}; SetPackingSolver<double> t(q); }
        if (mode == 16) { vector<int> ids{0, 0}; s.evaluate(ids); }
        return 0;
    }
    long long enumerated = 0;
    auto random_tests = [&]<class W>(W) {
        using S = SetPackingSolver<W>;
        using P = typename S::Problem;
        mt19937_64 random(619);
        auto draw = [&](int n) { return (int)(random() % (uint64_t)n); };
        auto equal = [](W a, W b) {
            if constexpr (is_floating_point_v<W>) return abs(a - b) <= 1e-9 * max(1.0, abs((double)b));
            else return a == b;
        };
        auto feasible = [](const P& p, uint64_t mask) {
            vector<int> usage(p.capacity.size());
            for (int i = 0; i < (int)p.weight.size(); ++i) {
                bool chosen = (mask >> i & 1) != 0;
                if (p.fixed[i] != -1 && chosen != (p.fixed[i] == 1)) return false;
                if (chosen) for (int r : p.resources[i]) if (++usage[r] > p.capacity[r]) return false;
            }
            for (auto [i, j] : p.conflicts) if ((mask >> i & 1) && (mask >> j & 1)) return false;
            return true;
        };
        for (int test = 0; test < 500; ++test) {
            P p;
            int n = 1 + draw(13), m = draw(8);
            p.weight.resize((size_t)n); p.resources.resize((size_t)n); p.fixed.assign((size_t)n, -1);
            p.capacity.resize((size_t)m);
            for (auto& w : p.weight) {
                w = (W)(draw(31) - 5);
                if constexpr (is_floating_point_v<W>) w /= test % 2 ? 8.0 : 7.0;
            }
            for (int& c : p.capacity) c = draw(4);
            for (int i = 0; i < n; ++i) {
                if (draw(8) == 0) p.fixed[i] = (int8_t)draw(2);
                for (int r = 0; r < m; ++r) if (draw(4) == 0) p.resources[i].push_back(r);
                for (int j = 0; j < i; ++j) if (draw(6) == 0) p.conflicts.emplace_back(i, j);
            }
            S s(p, (uint64_t)test + 1);
            auto verify = [&](bool enumerate) {
                bool exists = false;
                W optimum = numeric_limits<W>::lowest();
                if (enumerate) for (uint64_t mask = 0; mask < (1ULL << n); ++mask) {
                    vector<int> selected;
                    W value{};
                    for (int i = 0; i < n; ++i) if (mask >> i & 1) { selected.push_back(i); value += p.weight[i]; }
                    bool okay = feasible(p, mask);
                    assert(s.is_feasible(selected) == okay);
                    assert(equal(s.evaluate(selected), value));
                    if (okay) { exists = true; optimum = max(optimum, value); }
                    ++enumerated;
                }
                typename S::Options o; o.time_limit_us = -1; o.iteration_limit = 350;
                auto result = s.solve(o);
                if (enumerate) assert(result.feasible == exists);
                if (!result.feasible) { assert(result.selected.empty() && result.value == W{}); return; }
                uint64_t mask = 0;
                for (int i : result.selected) mask |= 1ULL << i;
                assert(feasible(p, mask) && equal(result.value, s.evaluate(result.selected)));
                if (enumerate) assert(result.value <= optimum || equal(result.value, optimum));
                auto again = s.solve(o);
                assert(again.value >= result.value || equal(again.value, result.value));
                auto initial = result.selected;
                shuffle(initial.begin(), initial.end(), random);
                auto improved = s.improve(initial, o);
                assert(improved.value >= again.value || equal(improved.value, again.value));
                const auto& view = s.solve_view(o);
                W before = view.value;
                auto alias = s.improve(view.selected, o);
                assert(alias.value >= before || equal(alias.value, before));
            };
            verify(true);
            for (int turn = 0; turn < 8; ++turn) {
                typename S::Update u;
                int i = draw(n); p.weight[i] = (W)(draw(31) - 5); u.weights.emplace_back(i, p.weight[i]);
                i = draw(n); p.fixed[i] = (int8_t)(draw(3) - 1); u.fixed.emplace_back(i, p.fixed[i]);
                if (m) { int r = draw(m); p.capacity[r] = draw(4); u.capacities.emplace_back(r, p.capacity[r]); }
                s.update(u); verify(turn == 7);
            }
        }
    };
    random_tests(0LL);
    random_tests(0.0);
    // 空問題、非正重み、必須矛盾、重複正規化、整数の精度境界
    {
        Problem p;
        Solver s(p);
        auto r = s.solve(no_work);
        assert(r.feasible && r.value == 0 && r.selected.empty());
        p.weight = {-10, 0, -20, 5}; p.fixed = {1, -1, -1, -1};
        s.rebuild(p); r = s.solve(fixed_work);
        assert(r.feasible && r.value == -5);
        p.conflicts = {{0, 3}, {3, 0}, {0, 3}}; p.fixed[3] = 1;
        s.rebuild(p); r = s.solve(no_work);
        assert(!r.feasible && r.value == 0 && r.selected.empty());
        p = Problem{}; p.weight = {11, 13, 7}; p.capacity = {1}; p.resources = {{0, 0}, {0}, {}};
        s.rebuild(p); r = s.solve(fixed_work);
        assert(r.feasible && r.value == 20);
        p = Problem{}; p.weight = {9007199254740992LL, 9007199254740993LL}; p.conflicts = {{0, 1}};
        s.rebuild(p); r = s.solve(fixed_work);
        assert(r.value == p.weight[1]);
    }
    // 更新で候補を再び利用でき、旧解と初期解の双方を新しい重みで比較する
    {
        Problem p; p.weight = {10, 20, -5, 7}; p.capacity = {1}; p.resources = {{0}, {0}, {}, {}};
        Solver s(p, 17);
        auto old = s.solve(fixed_work);
        assert(old.value == 27);
        Solver::Update u; u.weights = {{2, 50}}; u.fixed = {{1, 0}}; u.capacities = {{0, 0}};
        s.update(u);
        auto r = s.solve(fixed_work);
        assert(r.value == 57 && old.value == 27);
        u = {}; u.fixed = {{1, -1}}; u.capacities = {{0, 2}};
        s.update(u); r = s.solve(fixed_work); assert(r.value == 87);
        u = {}; u.weights = {{0, 11}}; s.update(u);
        vector<int> empty;
        r = s.improve(empty, no_work); assert(r.value == 88);
        u = {}; u.fixed = {{0, 1}, {1, 1}}; u.capacities = {{0, 1}}; s.update(u);
        assert(!s.solve(no_work).feasible);
        u = {}; u.fixed = {{0, -1}}; s.update(u);
        assert(s.solve(fixed_work).value == 77);
        s.update({});
        assert(s.solve(no_work).value == 77);
        p.weight[1] = 999; // solverは元のProblemを所有する
        assert(s.evaluate(vector<int>{1}) == 20);
    }
    // 初期解、所有する結果、構造の置換、同じ乱数・反復数での再現性
    {
        Problem p; p.weight = {12, 8, 8, 7, 6, 20}; p.capacity = {2};
        p.resources = {{0}, {0}, {0}, {0}, {}, {0}}; p.conflicts = {{0, 1}, {1, 2}, {4, 5}};
        Solver a(p, 888), b(p, 888);
        auto x = a.solve(fixed_work), y = b.solve(fixed_work);
        assert(x.selected == y.selected && x.value == y.value && x.iterations == y.iterations);
        a.seed(919); b.seed(919);
        x = a.solve(fixed_work); y = b.solve(fixed_work);
        assert(x.selected == y.selected && x.value == y.value);
        auto saved = x;
        auto o = no_work; o.time_limit_us = 0;
        vector<int> initial{1, 3, 4};
        auto r = a.improve(initial, o);
        assert(r.value >= 21 && r.value >= saved.value);
        vector<int> infeasible{0, 1, 2, 3, 4, 5};
        assert(a.improve(infeasible, o).value >= saved.value);
        a.rebuild(Problem{});
        assert(a.solve(no_work).value == 0 && saved.value == x.value);
        Problem replacement; replacement.weight = {9, 4, 8}; replacement.conflicts = {{0, 2}};
        a.rebuild(replacement);
        vector<int> mapped{1, 2}; r = a.improve(mapped, no_work);
        assert(r.feasible && r.value == 12);
    }
    // 容量1の資源を競合辺へ変換し、IDの置換・重みの定数倍も全列挙で照合する
    {
        mt19937_64 random(6718);
        auto draw = [&](int n) { return (int)(random() % (uint64_t)n); };
        for (int trial = 0; trial < 160; ++trial) {
            int n = 1 + draw(10), m = 1 + draw(7);
            Problem p; p.weight.resize((size_t)n); p.resources.resize((size_t)n);
            p.capacity.assign((size_t)m, 1); p.fixed.assign((size_t)n, -1);
            for (int i = 0; i < n; ++i) {
                p.weight[i] = draw(50) - 5;
                if (draw(6) == 0) p.fixed[i] = (int8_t)draw(2);
                for (int r = 0; r < m; ++r) if (draw(3) == 0) p.resources[i].push_back(r);
                for (int j = 0; j < i; ++j) if (draw(7) == 0) p.conflicts.emplace_back(i, j);
            }
            Problem graph = p;
            graph.resources.clear(); graph.capacity.clear();
            for (int i = 0; i < n; ++i) for (int j = 0; j < i; ++j)
                for (int r : p.resources[i]) if (find(p.resources[j].begin(), p.resources[j].end(), r) != p.resources[j].end())
                    graph.conflicts.emplace_back(i, j);
            vector<int> ids((size_t)n), resources((size_t)m);
            iota(ids.begin(), ids.end(), 0); iota(resources.begin(), resources.end(), 0);
            shuffle(ids.begin(), ids.end(), random); shuffle(resources.begin(), resources.end(), random);
            Problem permuted = p;
            for (auto& row : permuted.resources) row.clear();
            permuted.conflicts.clear();
            for (int i = 0; i < n; ++i) {
                permuted.weight[ids[i]] = 3 * p.weight[i]; permuted.fixed[ids[i]] = p.fixed[i];
                for (int r : p.resources[i]) permuted.resources[ids[i]].push_back(resources[r]);
            }
            for (auto [a, b] : p.conflicts) permuted.conflicts.emplace_back(ids[a], ids[b]);
            Solver a(p), b(graph), c(permuted);
            for (int mask = 0; mask < (1 << n); ++mask) {
                vector<int> selected, mapped;
                for (int i = 0; i < n; ++i) if (mask >> i & 1) { selected.push_back(i); mapped.push_back(ids[i]); }
                assert(a.is_feasible(selected) == b.is_feasible(selected));
                assert(a.is_feasible(selected) == c.is_feasible(mapped));
                assert(a.evaluate(selected) == b.evaluate(selected));
                assert(3 * a.evaluate(selected) == c.evaluate(mapped));
                ++enumerated;
            }
        }
    }
    // 時間指定と外部deadline。初期化・復元は時間切れでも完了させる
    {
        Problem p; p.weight.assign(1500, 1); p.capacity = {10}; p.resources.assign(1500, {0});
        Solver s(p);
        auto o = fixed_work; o.time_limit_us = 0;
        auto r = s.solve(o); assert(r.feasible && r.iterations == 0);
        o.time_limit_us = -1; o.deadline = Solver::Clock::now() - chrono::seconds(1);
        r = s.solve(o); assert(r.iterations == 0 && r.elapsed_us >= 0);
        o.deadline.reset(); o.time_limit_us = 2000; o.iteration_limit = -1;
        r = s.solve(o); assert(r.feasible && r.value == 10 && r.elapsed_us < 500000);
        o.time_limit_us = 100000; o.deadline = Solver::Clock::now() + chrono::milliseconds(1);
        r = s.solve(o); assert(r.elapsed_us < 500000);
    }
    // 部分問題の境界条件：外側の選択による容量減少と競合を明示する
    {
        Problem global; global.weight = {20, 10, 14, 9, 8};
        global.resources = {{0}, {0, 1}, {1}, {0}, {}}; global.capacity = {2, 1};
        global.conflicts = {{0, 4}}; global.fixed = {1, -1, -1, -1, -1};
        vector<int> outside{0}, map_to_global{1, 2, 3, 4};
        Problem local; local.capacity = {1, 1}; local.weight = {10, 14, 9, 8};
        local.resources = {{0, 1}, {1}, {0}, {}}; local.fixed = {-1, -1, -1, 0};
        Solver g(global), l(local);
        for (int mask = 0; mask < 16; ++mask) {
            vector<int> ls, gs = outside;
            for (int i = 0; i < 4; ++i) if (mask >> i & 1) { ls.push_back(i); gs.push_back(map_to_global[i]); }
            assert(l.is_feasible(ls) == g.is_feasible(gs));
            assert(l.evaluate(ls) + 20 == g.evaluate(gs));
        }
        auto r = l.solve(fixed_work);
        vector<int> combined = outside;
        for (int i : r.selected) combined.push_back(map_to_global[i]);
        assert(g.is_feasible(combined) && g.evaluate(combined) == 20 + r.value);
    }
    long long state_checks = Solver::test_internals() + SetPackingSolver<double>::test_internals();
    cout << "PASS: " << enumerated << " exhaustive API subsets, " << state_checks << " internal states, scenario tests\n";
}
#endif
