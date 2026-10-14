// 重み付き集合パッキング：競合辺・単位資源容量・必須/禁止を扱う単一スレッドの近似solver
// Nは1e7以下、M・重複除去前のCSR要素数はint内、整数重みの絶対値合計はLLONG_MAX/4以下、実数の絶対値合計は有限値
// 通常は元のCSRを直接使い、固定指定があり有効候補が1/4以下のときだけ縮約して再利用する
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

    // デバッグ用の1行表示。選択IDは先頭20件までで、改行は呼び出し側に任せる。
    friend ostream& operator<<(ostream& os, const SetPackingResult& result) {
        os << "SetPackingResult{feasible=" << (result.feasible ? "true" : "false")
           << ", value=" << result.value << ", selected=[";
        size_t count = min(result.selected.size(), size_t{20});
        for (size_t i = 0; i < count; ++i) {
            if (i) os << ", ";
            os << result.selected[i];
        }
        if (count < result.selected.size()) os << ", ... (" << result.selected.size() << " total)";
        return os << "], elapsed_us=" << result.elapsed_us << ", iterations=" << result.iterations << '}';
    }
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
    struct Data {
        Csr adjacent, resources, members;
        vector<int> owner;
        bool indexed = false;
        void index(int m) {
            indexed = true;
            owner.resize(resources.data.size());
            for (int i = 0; i + 1 < (int)resources.offset.size(); ++i)
                for (int p = resources.offset[i]; p < resources.offset[i + 1]; ++p) owner[p] = i;
            members.build(m, [&](auto emit) {
                for (int p = 0; p < (int)owner.size(); ++p) emit(resources.data[p], owner[p]);
            });
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

    int n_ = 0, m_ = 0;
    vector<Weight> weight_;
    vector<int> capacity_;
    vector<int8_t> fixed_;
    Data data_, spare_;
    bool reduced_ = false, adjusted_ = false;
    vector<int> required_, original_capacity_, involved_;
    vector<int> location_, occupants_, use_, block_, position_, selected_, active_;
    vector<uint8_t> eligible_;
    vector<uint32_t> mark_;
    uint32_t stamp_ = 0, work_ = 0;
    vector<int> removed_, added_, pool_;
    vector<double> priority_;
    vector<int> order_;
    bool order_dirty_ = true;
    double scale_ = 0;
    Weight value_{};
    Result best_, result_;
    bool result_dirty_ = true;
    bool dirty_ = true, weights_changed_ = false, stopped_ = false, constructed_ = false;
    long long steps_ = 0;
    Clock::time_point stop_ = Clock::time_point::max();

    const Data& source() const { return reduced_ ? spare_ : data_; }
    const vector<int>& capacities() const { return adjusted_ ? original_capacity_ : capacity_; }
    Result& answer() { return required_.empty() ? best_ : result_; }

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
        for (int r : data_.resources[i]) if (use_[r] >= capacity_[r]) return false;
        return true;
    }
    void add(int i) {
        position_[i] = (int)selected_.size();
        selected_.push_back(i);
        value_ += weight_[i];
        for (int j : data_.adjacent[i]) ++block_[j];
        for (int p = data_.resources.offset[i]; p < data_.resources.offset[i + 1]; ++p) {
            int r = data_.resources.data[p];
            int slot = data_.members.offset[r] + use_[r]++;
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
        for (int j : data_.adjacent[i]) --block_[j];
        for (int p = data_.resources.offset[i]; p < data_.resources.offset[i + 1]; ++p) {
            int r = data_.resources.data[p];
            int moved = occupants_[data_.members.offset[r] + --use_[r]];
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
        result_dirty_ = true;
    }
    bool build_search() {
        order_dirty_ = true;
        // 元構造と元容量へ戻す。必須がなければ容量の複製は作らない。
        if (reduced_) { swap(data_, spare_); reduced_ = false; }
        if (adjusted_) { swap(capacity_, original_capacity_); adjusted_ = false; }
        fill(position_.begin(), position_.end(), -1);
        fill(block_.begin(), block_.end(), 0);
        fill(eligible_.begin(), eligible_.end(), 0);
        required_.clear(); active_.clear(); selected_.clear(); involved_.clear();
        value_ = Weight{}; scale_ = 0;
        int fixed_count = 0;
        for (int i = 0; i < n_; ++i) {
            fixed_count += fixed_[i] != -1;
            if (fixed_[i] != 1) continue;
            if (block_[i]) return false;
            if (!adjusted_) { original_capacity_ = capacity_; adjusted_ = true; }
            required_.push_back(i);
            for (int j : data_.adjacent[i]) ++block_[j];
            for (int r : data_.resources[i]) if (--capacity_[r] < 0) return false;
        }
        for (int i = 0; i < n_; ++i) if (fixed_[i] == -1 && weight_[i] > Weight{} && !block_[i]) {
            bool allowed = true;
            for (int r : data_.resources[i]) if (capacity_[r] == 0) { allowed = false; break; }
            if (allowed) { eligible_[i] = 1; active_.push_back(i); }
        }
        // 固定が少ない通常経路では元のCSRを直接使い、探索中の分岐を増やさない。
        if (fixed_count && active_.size() <= (size_t)n_ / 4) {
            spare_.adjacent.build(n_, [&](auto emit) {
                for (int i : active_) for (int j : data_.adjacent[i]) if (eligible_[j]) emit(i, j);
            });
            spare_.resources.build(n_, [&](auto emit) {
                for (int i : active_) for (int r : data_.resources[i]) emit(i, r);
            });
            spare_.index(m_);
            swap(data_, spare_); reduced_ = true;
            for (int r = 0; r < m_; ++r) if (!data_.members[r].empty()) involved_.push_back(r);
        } else if (!data_.indexed) data_.index(m_);
        location_.resize(data_.owner.size()); occupants_.resize(data_.owner.size());
        for (int i : active_) {
            scale_ += (double)weight_[i];
            double pressure = 1.0 + (double)data_.adjacent[i].size();
            for (int r : data_.resources[i]) pressure += (double)(data_.members[r].size() - 1) / (double)capacity_[r];
            priority_[i] = (double)weight_[i] / pressure;
        }
        if (!active_.empty()) scale_ /= (double)active_.size();
        return true;
    }
    void prepare(span<const int> hint, bool reset = true) {
        // 構造を作り直した直後は位置・有効候補の競合数が初期化済み。
        // 元CSRでは不使用候補の競合数も更新するため、再投入では全体を消す。
        if (reset) {
            if (reduced_) for (int i : active_) { position_[i] = -1; block_[i] = 0; }
            else { fill(position_.begin(), position_.end(), -1); fill(block_.begin(), block_.end(), 0); }
        }
        if (reduced_) for (int r : involved_) use_[r] = 0;
        else fill(use_.begin(), use_.end(), 0);
        selected_.clear(); value_ = Weight{};
        auto tag = stamp();
        for (int i : hint) {
            assert(0 <= i && i < n_ && mark_[i] != tag);
            mark_[i] = tag;
            if (eligible_[i] && can_add(i)) add(i);
        }
        constructed_ = false;
        steps_ = 0;
    }
    void greedy() {
        // 圧迫度当たりの利益が高い順に、追加できる候補を採用する
        if (order_dirty_) {
            order_ = active_;
            sort(order_.begin(), order_.end(), [&](int a, int b) {
                return priority_[a] != priority_[b] ? priority_[a] > priority_[b] : a < b;
            });
            order_dirty_ = false;
        }
        for (int i : order_) {
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
            scan(data_.adjacent[i]);
            for (int r : data_.resources[i]) if (use_[r] < capacity_[r]) scan(data_.members[r]);
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
        for (int j : data_.adjacent[target]) if (position_[j] >= 0) drop(j);
        // 容量が不足する資源から、小さい重みの自由候補をサンプルで探す
        for (int r : data_.resources[target]) if (use_[r] >= capacity_[r]) {
            int chosen = -1;
            int count = min(use_[r], 8);
            int offset = count < use_[r] ? rng_.index(use_[r]) : 0;
            for (int k = 0; k < count; ++k) {
                int p = occupants_[data_.members.offset[r] + offset];
                if (++offset == use_[r]) offset = 0;
                int j = data_.owner[p];
                if (chosen < 0 || weight_[j] < weight_[chosen]) chosen = j;
            }
            if (tick()) {
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
            vector<int> old = move(answer().selected);
            best_ = Result{};
            if (supplied && old.empty()) { old = move(previous); supplied = false; }
            result_dirty_ = true;
            if (build_search()) { prepare(old, false); save_best(); }
            dirty_ = false;
        }
        else if (weights_changed_ && best_.feasible) {
            // 制約と有効候補が同じなら、選択状態を保って目的値だけ再評価する
            value_ = evaluate(selected_);
            best_.value = evaluate(best_.selected);
            save_best();
        }
        weights_changed_ = false;
        if (supplied && best_.feasible) { prepare(previous); save_best(); }
        long long iterations = 0;
        if (best_.feasible && !active_.empty() && options.iteration_limit != 0 && !expired()) {
            if (!constructed_) { greedy(); constructed_ = true; }
            int cycle = max(256, 8 * (int)active_.size());
            double temperature = scale_ * 0.3 * pow(0.01, (double)(steps_ % cycle) / (double)cycle);
            while (!stopped_ && selected_.size() < active_.size() &&
                   (options.iteration_limit < 0 || iterations < options.iteration_limit)) {
                if ((iterations & 31) == 0 && expired()) break;
                ++iterations;
                int target = active_[(size_t)rng_.index((int)active_.size())];
                if ((steps_++ & 31) == 0) temperature = scale_ * 0.3 * pow(0.01, (double)(steps_ % cycle) / (double)cycle);
                exchange(target, temperature);
            }
        }
        // 必須集合と最良の自由集合は返却時にだけ合流する
        if (!required_.empty() && result_dirty_) {
            result_.feasible = best_.feasible;
            result_.selected.clear(); result_.value = Weight{};
            if (best_.feasible) {
                result_.selected = required_;
                result_.selected.insert(result_.selected.end(), best_.selected.begin(), best_.selected.end());
                result_.value = evaluate(result_.selected);
            }
        }
        result_dirty_ = false;
        auto& result = answer();
        result.iterations = iterations;
        result.elapsed_us = chrono::duration<double, micro>(Clock::now() - begin).count();
        return result;
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
                    for (int j : s.data_.adjacent[i]) ++blocks[j];
                    for (int r : p.resources[i]) ++used[r];
                    for (int at = s.data_.resources.offset[i]; at < s.data_.resources.offset[i + 1]; ++at) {
                        int r = s.data_.resources.data[at], location = s.location_[at];
                        assert(location >= s.data_.members.offset[r] && location < s.data_.members.offset[r] + s.use_[r]);
                        assert(s.occupants_[location] == at && s.data_.owner[at] == i);
                    }
                }
                for (int i : s.active_) assert(positions[i] == s.position_[i] && blocks[i] == s.block_[i]);
                for (int r : s.involved_) assert(used[r] == s.use_[r]);
                assert(equal(sum, s.value_));
                auto whole = s.required_; whole.insert(whole.end(), s.selected_.begin(), s.selected_.end());
                assert(s.is_feasible(whole));
                whole = s.required_; whole.insert(whole.end(), s.best_.selected.begin(), s.best_.selected.end());
                assert(s.best_.feasible && s.is_feasible(whole));
                assert(equal(s.best_.value, s.evaluate(s.best_.selected)));
                for (int i : s.active_) {
                    bool possible = positions[i] < 0 && blocks[i] == 0;
                    for (int r : p.resources[i]) possible = possible && used[r] < s.capacity_[r];
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
                for (int j : s.answer().selected) lower += p.weight[j];
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
                    double pressure = 1.0 + (double)s.data_.adjacent[j].size();
                    for (int resource : p.resources[j])
                        pressure += (double)(s.data_.members[resource].size() - 1) / (p.capacity[resource] - mandatory_use[resource]);
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
                check(s.source().adjacent, adjacent); check(s.source().resources, resources);
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
                assert(s.build_search()); s.prepare(hint);
                auto combined = s.required_; combined.insert(combined.end(), s.selected_.begin(), s.selected_.end());
                assert(combined == expected);
                Weight total{};
                for (int i : s.selected_) total += p.weight[i];
                assert(s.value_ == total && s.evaluate(s.selected_) == total);
                for (int i = 0; i < n; ++i) assert(s.fixed_[i] == flag(i));
                ++checks;
            }
        }
        // 固定部分を手動で切り出した問題と、元IDで解く問題を多数の更新後にも照合する
        {
            long long compared = 0;
            for (int trial = 0; trial < 120; ++trial) {
                int n = 15 + draw(90), m = 1 + draw(20);
                Problem p; p.weight.resize((size_t)n); p.fixed.assign((size_t)n, -1);
                p.capacity.resize((size_t)m); p.resources.resize((size_t)n);
                for (auto& w : p.weight) { w = (Weight)(draw(100) - 10); if constexpr (is_floating_point_v<Weight>) w /= 7.0; }
                for (auto& c : p.capacity) c = 1 + draw(4);
                for (int i = 0; i < n; ++i) {
                    for (int r = 0; r < m; ++r) if (draw(8) == 0) p.resources[i].push_back(r);
                    for (int j = 0; j < i; ++j) if (draw(20) == 0) p.conflicts.emplace_back(i, j);
                }
                SetPackingSolver s(p, (uint64_t)trial + 77);
                Options zero; zero.time_limit_us = -1; zero.iteration_limit = 0;
                Options work = zero; work.iteration_limit = 120;
                vector<int> current;
                for (int turn = 0; turn < 16; ++turn) {
                    // 前回の実行可能解を基準に範囲を変え、必須と禁止を同時に更新する
                    vector<uint8_t> chosen((size_t)n);
                    for (int i : current) chosen[i] = 1;
                    Update changes;
                    int free_count = 0;
                    for (int i = 0; i < n; ++i) {
                        bool free = draw(4) == 0 && free_count < n / 4;
                        free_count += free;
                        p.fixed[i] = free ? (int8_t)-1 : (int8_t)chosen[i];
                        changes.fixed.emplace_back(i, p.fixed[i]);
                    }
                    int id = draw(n);
                    p.weight[id] = (Weight)(draw(110) - 10);
                    if constexpr (is_floating_point_v<Weight>) p.weight[id] /= 7.0;
                    changes.weights.emplace_back(id, p.weight[id]);
                    s.update(changes);
                    auto initial = s.improve(current, zero);
                    assert(initial.feasible && s.is_feasible(initial.selected));

                    // 独立に残容量、境界の禁止、局所IDと資源IDの対応を作る
                    Problem local;
                    auto remaining = p.capacity;
                    vector<int> to_local((size_t)n, -1), ids, mandatory;
                    vector<uint8_t> forbidden((size_t)n);
                    for (int i = 0; i < n; ++i) if (p.fixed[i] == 1) {
                        mandatory.push_back(i);
                        for (int r : p.resources[i]) --remaining[r];
                    }
                    for (auto [a, b] : p.conflicts) {
                        if (p.fixed[a] == 1) forbidden[b] = 1;
                        if (p.fixed[b] == 1) forbidden[a] = 1;
                    }
                    vector<int> resource_ids((size_t)m, -1);
                    for (int i = 0; i < n; ++i) if (p.fixed[i] == -1 && p.weight[i] > Weight{} && !forbidden[i]) {
                        bool okay = true;
                        for (int r : p.resources[i]) if (remaining[r] == 0) okay = false;
                        if (!okay) continue;
                        to_local[i] = (int)ids.size(); ids.push_back(i);
                        local.weight.push_back(p.weight[i]); local.resources.push_back(p.resources[i]);
                        for (int r : p.resources[i]) resource_ids[r] = 0;
                    }
                    for (int r = 0; r < m; ++r) if (resource_ids[r] == 0) {
                        resource_ids[r] = (int)local.capacity.size(); local.capacity.push_back(remaining[r]);
                    }
                    for (auto& row : local.resources) for (int& r : row) r = resource_ids[r];
                    for (auto [a, b] : p.conflicts) if (to_local[a] >= 0 && to_local[b] >= 0)
                        local.conflicts.emplace_back(to_local[a], to_local[b]);
                    vector<int> hint;
                    for (int i : initial.selected) if (to_local[i] >= 0) hint.push_back(to_local[i]);
                    uint64_t seed = random();
                    SetPackingSolver compact(local, seed);
                    compact.improve(hint, zero);
                    s.improve(initial.selected, zero); s.seed(seed);
                    for (int repeat = 0; repeat < 3; ++repeat) {
                        auto a = s.solve(work), b = compact.solve(work);
                        vector<int> mapped = mandatory;
                        for (int i : b.selected) mapped.push_back(ids[i]);
                        assert(a.selected == mapped && equal(a.value, s.evaluate(mapped)));
                        assert(a.iterations == b.iterations && s.rng_.state == compact.rng_.state);
                        assert(s.is_feasible(a.selected));
                        assert(s.selected_.size() <= ids.size() && s.best_.selected.size() <= ids.size());
                        for (int i : s.data_.owner) assert(p.fixed[i] == -1);
                        current = a.selected; ++compared;
                    }
                    // 初期案の渡し直しと同値更新はCSRのバッファを再利用する
                    auto adjacency = s.data_.adjacent.data.data(), members = s.data_.members.data.data();
                    auto offsets = s.data_.resources.offset.data();
                    Update unchanged; unchanged.fixed = changes.fixed; unchanged.weights = changes.weights;
                    s.update(unchanged);
                    assert(!s.dirty_ && !s.weights_changed_);
                    auto saved = s.solve_view(zero).selected;
                    auto alias = s.improve(s.solve_view(zero).selected, zero);
                    assert(alias.selected == saved);
                    assert(adjacency == s.data_.adjacent.data.data() && members == s.data_.members.data.data());
                    assert(offsets == s.data_.resources.offset.data());
                    SetPackingSolver copied = s;
                    auto a = s.solve(work), b = copied.solve(work);
                    assert(a.value == b.value && a.selected == b.selected);
                    ++checks;
                }
            }
            cout << "PASS local equivalence: " << compared << " (" << (is_same_v<Weight, double> ? "double" : "long long") << ")\n";
        }
        // 大きい必須利益は自由部分の比較へ混ぜず、返却時だけ合計する
        {
            Problem p; p.weight = {(Weight)1e16, (Weight)1, (Weight)2}; p.fixed = {1, -1, -1};
            p.capacity = {2}; p.resources = {{0}, {0}, {0}};
            SetPackingSolver s(p);
            Options o; o.time_limit_us = -1; o.iteration_limit = 0;
            s.improve(vector<int>{0, 1}, o); s.exchange(2, 0);
            assert(s.best_.value == (Weight)2 && s.best_.selected == vector<int>{2});
            auto result = s.solve(o); assert((result.selected == vector<int>{0, 2}));
            assert(s.selected_.size() == 1 && s.capacity_[0] == 1);
            Update release; release.fixed = {{0, -1}, {1, 0}, {2, 0}};
            s.update(release); result = s.solve(o);
            assert(result.feasible && result.selected == vector<int>{0});
            ++checks;
        }
        // 通常・縮約・必須なしを繰り返し切り替え、全体解と元構造を保つ。
        {
            long long transitions = 0;
            for (int trial = 0; trial < 48; ++trial) {
                const int n = 96, m = 16;
                Problem p; p.weight.resize(n); p.fixed.assign(n, -1);
                p.resources.resize(n); p.capacity.assign(m, 8);
                for (int i = 0; i < n; ++i) {
                    p.weight[i] = (Weight)(1 + draw(100));
                    if constexpr (is_floating_point_v<Weight>) p.weight[i] /= 7.0;
                    p.resources[i] = {i % m, (i + 3) % m};
                    p.conflicts.emplace_back(i, (i + 1) % n);
                }
                SetPackingSolver s(p, (uint64_t)trial + 101);
                Options zero; zero.time_limit_us = -1; zero.iteration_limit = 0;
                Options work = zero; work.iteration_limit = 240;
                auto current = s.solve(work);
                auto original_edges = s.source().adjacent.data;
                auto original_uses = s.source().resources.data;
                auto original_members = s.source().members.data;
                for (int turn = 0; turn < 56; ++turn) {
                    int count = array<int, 7>{96, 95, 86, 48, 24, 8, 0}[(size_t)turn % 7];
                    vector<uint8_t> chosen(n), free(n);
                    for (int i : current.selected) chosen[i] = 1;
                    for (int k = 0; k < count; ++k) free[(k + 13 * turn) % n] = 1;
                    Update u;
                    for (int i = 0; i < n; ++i) {
                        p.fixed[i] = free[i] ? (int8_t)-1 : (int8_t)chosen[i];
                        u.fixed.emplace_back(i, p.fixed[i]);
                    }
                    if (turn % 4 == 0) {
                        int r = turn % m, used = 0;
                        for (int i : current.selected) used += (int)count_if(p.resources[i].begin(), p.resources[i].end(), [&](int x) { return x == r; });
                        p.capacity[r] = used + 1; u.capacities = {{r, p.capacity[r]}};
                    }
                    auto before = current;
                    s.update(u);
                    assert(s.is_feasible(before.selected));
                    current = s.improve(before.selected, zero);
                    assert(current.feasible && equal(current.value, before.value));
                    assert(s.is_feasible(current.selected) && equal(current.value, s.evaluate(current.selected)));
                    assert(s.source().adjacent.data == original_edges && s.source().resources.data == original_uses);
                    assert(s.source().members.data == original_members && s.capacities() == p.capacity);
                    if (count == n) assert(!s.reduced_ && !s.adjusted_ && &s.solve_view(zero) == &s.best_);
                    if (count <= n / 4) assert(s.reduced_);
                    for (int r = 0; r < m; ++r) if (!s.reduced_ || !s.data_.members[r].empty()) for (int k = 0; k < s.use_[r]; ++k) {
                        int member = s.occupants_[s.data_.members.offset[r] + k];
                        assert(p.fixed[s.data_.owner[member]] == -1);
                    }
                    SetPackingSolver copy = s;
                    auto a = s.solve(work), b = copy.solve(work);
                    assert(a.selected == b.selected && equal(a.value, b.value) && s.rng_.state == copy.rng_.state);
                    current = a;
                    SetPackingSolver assigned(Problem{}); assigned = s;
                    SetPackingSolver moved(move(assigned));
                    auto c = s.solve(work), d = moved.solve(work);
                    assert(c.selected == d.selected && equal(c.value, d.value));
                    current = c;
                    auto saved = s.solve_view(zero).selected;
                    auto alias = s.improve(s.solve_view(zero).selected, zero);
                    assert(alias.selected == saved);
                    ++transitions; ++checks;
                }
                s.rebuild(Problem{});
                auto empty = s.solve(zero);
                assert(empty.feasible && empty.selected.empty() && empty.value == Weight{});
            }
            cout << "PASS mode transitions: " << transitions << " (" << (is_same_v<Weight, double> ? "double" : "long long") << ")\n";
        }
        // 非正重みの隣接先も持つ通常経路で、improveを重ねても競合数を累積しない。
        {
            Problem p; p.weight = {(Weight)7, (Weight)-1, (Weight)3}; p.conflicts = {{0, 1}, {1, 2}};
            SetPackingSolver s(p); Options o; o.time_limit_us = -1; o.iteration_limit = 0;
            for (int repeat = 0; repeat < 100; ++repeat) {
                auto r = s.improve(vector<int>{0, 2}, o);
                assert(!s.reduced_ && r.value == (Weight)10 && s.block_[1] == 2);
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
        if (reduced_) { swap(data_, spare_); reduced_ = false; }
        adjusted_ = false; required_.clear(); data_.indexed = false;
        weight_ = problem.weight; capacity_ = problem.capacity;
        fixed_ = problem.fixed; fixed_.resize((size_t)n_, -1);
        for (int i = 0; i < n_; ++i) {
            assert(-1 <= fixed_[i] && fixed_[i] <= 1);
            if constexpr (is_floating_point_v<Weight>) assert(isfinite(weight_[i]));
        }
        for (int c : capacity_) { assert(c >= 0); (void)c; }
        // 直接CSRへ詰め、各行のソートと重複除去で正規化する
        data_.adjacent.build(n_, [&](auto emit) {
            for (auto [i, j] : problem.conflicts) {
                assert(0 <= i && i < n_ && 0 <= j && j < n_ && i != j);
                emit(i, j); emit(j, i);
            }
        }, true);
        data_.resources.build(n_, [&](auto emit) {
            for (int i = 0; i < (int)problem.resources.size(); ++i) for (int r : problem.resources[i]) {
                assert(0 <= r && r < m_);
                emit(i, r);
            }
        }, true);
        // 固定ゼロは通常経路をここで完成させる。縮約時に不要な逆引きは作らない。
        if (all_of(fixed_.begin(), fixed_.end(), [](int8_t f) { return f == -1; })) data_.index(m_);
        // 大きさが変わるため、全体IDで参照する配列だけをここで用意する
        use_.resize((size_t)m_); block_.resize((size_t)n_); position_.resize((size_t)n_);
        eligible_.resize((size_t)n_); priority_.resize((size_t)n_);
        mark_.assign((size_t)max(n_, m_), 0); stamp_ = 0;
        selected_.clear(); selected_.reserve((size_t)n_);
        best_ = Result{}; result_ = Result{}; dirty_ = true; weights_changed_ = false; result_dirty_ = true;
    }
    // 乱数状態だけを設定する O(1)
    void seed(uint64_t value) { rng_.state = value; }
    // 数値と固定指定を一括変更する 通常O(変更件数)、再評価・必要な再初期化は次回探索時
    void update(const Update& changes) {
        // 制約や正重み候補の集合が変わる場合だけ、全体を再初期化する
        auto tag = stamp();
        auto& original = adjusted_ ? original_capacity_ : capacity_;
        for (auto [r, c] : changes.capacities) {
            assert(0 <= r && r < m_ && c >= 0 && mark_[r] != tag); mark_[r] = tag;
            dirty_ = dirty_ || original[r] != c;
            original[r] = c;
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
            order_dirty_ = true;
            weights_changed_ = true; result_dirty_ = true;
            dirty_ = dirty_ || (fixed_[i] == -1 && (weight_[i] > Weight{}) != (w > Weight{}));
            if (!dirty_ && eligible_[i]) {
                priority_[i] = (priority_[i] / (double)weight_[i]) * (double)w;
                scale_ += ((double)w - (double)weight_[i]) / (double)active_.size();
            }
            weight_[i] = w;
        }
    }
    // 探索を継続し、次の非const操作まで有効な結果参照を返す O(初期化+探索+結果更新時の選択数)
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
            for (int j : source().adjacent[i]) if (chosen[j]) return false;
            for (int r : source().resources[i]) if (++usage[r] > capacities()[r]) return false;
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
