// rollbackなし区間操作
// rollbackありより、変更操作が5～10倍速い
// 2秒で500万回以上

/********************************************************************
 * interval_treap2.cpp ― 動的区間管理 Treap（半開区間 [l, r)）
 * C++20 (gcc‑12.2)            last‑update : 2025‑04‑18 (テスト修正)
 *
 *  g++ -std=c++20 -O2 -pipe -static -s interval_treap2.cpp && ./a.out
 *******************************************************************/
#include <bits/stdc++.h>
using namespace std;
using HRClk = std::chrono::high_resolution_clock;

struct IntervalTreap {
    struct Node {
        int l, r;
        uint32_t pri;
        Node* ch[2];
        int sz;
        long long sum;
    };
    static int size(Node* t) { return t ? t->sz : 0; }
    static long long sum(Node* t) { return t ? t->sum : 0; }
    static Node* rightmost(Node* t) {
        while (t->ch[1]) t = t->ch[1];
        return t;
    }
    mt19937 rng_{0xC0FFEE};
    vector<Node*> pool_, free_;
    Node* root = nullptr;
    void pull(Node* t) {
        t->sz = 1 + size(t->ch[0]) + size(t->ch[1]);
        t->sum = 1LL * (t->r - t->l) + sum(t->ch[0]) + sum(t->ch[1]);
    }
    Node* new_node(int l, int r) {
        Node* t = free_.empty() ? (pool_.push_back(new Node), pool_.back())
                                : free_.back();
        if (!free_.empty()) free_.pop_back();
        *t = {l,
              r,
              static_cast<uint32_t>(rng_()),
              {nullptr, nullptr},
              1,
              1LL * (r - l)};
        return t;
    }
    void recycle(Node* t) {
        if (t) free_.push_back(t);
    }
    void split(Node* t, int key, Node*& a, Node*& b) {
        if (!t) {
            a = b = nullptr;
            return;
        }
        if (t->l < key) {
            split(t->ch[1], key, t->ch[1], b);
            pull(t);
            a = t;
        } else {
            split(t->ch[0], key, a, t->ch[0]);
            pull(t);
            b = t;
        }
    }
    Node* merge(Node* a, Node* b) {
        if (!a || !b) return a ? a : b;
        if (a->pri < b->pri) {
            a->ch[1] = merge(a->ch[1], b);
            pull(a);
            return a;
        }
        b->ch[0] = merge(a, b->ch[0]);
        pull(b);
        return b;
    }
    Node* pop_max(Node* t, Node*& res) {
        if (!t->ch[1]) {
            res = t;
            return t->ch[0];
        }
        t->ch[1] = pop_max(t->ch[1], res);
        pull(t);
        return t;
    }
    Node* pop_min(Node* t, Node*& res) {
        if (!t->ch[0]) {
            res = t;
            return t->ch[1];
        }
        t->ch[0] = pop_min(t->ch[0], res);
        pull(t);
        return t;
    }
    void insert(int l, int r) {
        if (l >= r) return;
        Node *A, *B;
        split(root, l, A, B);
        while (A) {
            Node* t = nullptr;
            A = pop_max(A, t);
            if (t->r < l) {
                t->ch[0] = nullptr;
                pull(t);
                A = merge(A, t);
                break;
            }
            l = min(l, t->l);
            r = max(r, t->r);
            recycle(t);
        }
        while (B) {
            Node* t = nullptr;
            B = pop_min(B, t);
            if (t->l > r) {
                t->ch[1] = nullptr;
                pull(t);
                B = merge(t, B);
                break;
            }
            l = min(l, t->l);
            r = max(r, t->r);
            recycle(t);
        }
        root = merge(merge(A, new_node(l, r)), B);
    }
    void erase(int l, int r) {
        if (l >= r) return;
        Node *A, *B, *C;
        split(root, l, A, B);
        split(B, r, B, C);
        if (A) {
            Node* ov = rightmost(A);
            if (ov->r > l) {
                Node* tmp = nullptr;
                A = pop_max(A, tmp);
                if (tmp->l < l) A = merge(A, new_node(tmp->l, l));
                if (tmp->r > r) C = merge(new_node(r, tmp->r), C);
                recycle(tmp);
            }
        }
        auto dfs = [&](auto&& self, Node* t) -> void {
            if (!t) return;
            self(self, t->ch[0]);
            self(self, t->ch[1]);
            if (t->r > r) C = merge(new_node(r, t->r), C);
            recycle(t);
        };
        dfs(dfs, B);
        root = merge(A, C);
    }
    bool contains(int x) const {
        const Node *t = root, *cand = nullptr;
        while (t) {
            if (t->l <= x) {
                cand = t;
                t = t->ch[1];
            } else
                t = t->ch[0];
        }
        return cand && cand->r > x;
    }
    int interval_count() const { return size(root); }
    long long covered_length() const { return sum(root); }
    const Node* nearest_left(int x) const {
        const Node *t = root, *res = nullptr;
        while (t) {
            if (t->l <= x) {
                if (t->r > x) return t;
                res = t;
                t = t->ch[1];
            } else
                t = t->ch[0];
        }
        return res;
    }
    const Node* nearest_right(int x) const {
        const Node *t = root, *res = nullptr;
        while (t) {
            if (t->l <= x) {
                if (t->r > x) return t;
                t = t->ch[1];
            } else {
                res = t;
                t = t->ch[0];
            }
        }
        return res;
    }
    template <class F>
    void inorder(Node* t, const F& f) const {
        if (!t) return;
        inorder(t->ch[0], f);
        f(t);
        inorder(t->ch[1], f);
    }
    template <class F>
    void iterate(const F& f) const {
        inorder(root, f);
    }
    ~IntervalTreap() {
        for (auto* p : pool_) delete p;
    }
};

/* ----- 検証用の Naive 実装（省略せず前回と同じ） ----- */
struct Naive {
    map<int, int> mp;
    void insert(int l, int r) {
        if (l >= r) return;
        auto it = mp.lower_bound(l);
        if (it != mp.begin()) {
            auto pre = prev(it);
            if (pre->second >= l) {
                l = pre->first;
                r = max(r, pre->second);
                it = mp.erase(pre);
            }
        }
        while (it != mp.end() && it->first <= r) {
            r = max(r, it->second);
            it = mp.erase(it);
        }
        mp[l] = r;
    }
    void erase(int l, int r) {
        if (l >= r) return;
        auto it = mp.lower_bound(l);
        if (it != mp.begin()) {
            auto pre = prev(it);
            if (pre->second > l) {
                int a = pre->first, b = pre->second;
                mp.erase(pre);
                if (a < l) mp[a] = l;
                if (b > r) mp[r] = b;
            }
        }
        while (it != mp.end() && it->first < r) {
            int b = it->second;
            it = mp.erase(it);
            if (b > r) mp[r] = b;
        }
    }
    bool contains(int x) const {
        auto it = mp.upper_bound(x);
        if (it == mp.begin()) return false;
        --it;
        return it->second > x;
    }
    int interval_count() const { return (int)mp.size(); }
    long long covered_length() const {
        long long s = 0;
        for (auto [l, r] : mp) s += r - l;
        return s;
    }
};

/* ------------------------------------------------------------------ */
/*                         既存ユニットテスト                          */
/* ------------------------------------------------------------------ */
void basic_unit_tests() {
    IntervalTreap tr;
    tr.insert(10, 20);
    tr.insert(20, 25);
    tr.insert(15, 30);
    assert(tr.interval_count() == 1 && tr.covered_length() == 20);
    tr.erase(18, 22);
    assert(tr.interval_count() == 2 && tr.covered_length() == 16);
    auto L = tr.nearest_left(18), R = tr.nearest_right(18);
    assert(L && L->l == 10 && L->r == 18);
    assert(R && R->l == 22 && R->r == 30);
    tr.erase(22, 30);
    assert(tr.interval_count() == 1);
    tr.erase(0, 100);
    assert(tr.interval_count() == 0 && tr.covered_length() == 0);
}

/* ランダム操作 vs Naive */
void exhaustive_random_test(int OPS, int COORD) {
    IntervalTreap tr;
    Naive nv;
    mt19937 rng(123);
    uniform_int_distribution<int> dpos(0, COORD);
    uniform_int_distribution<int> dop(0, 5);
    for (int i = 0; i < OPS; ++i) {
        int a = dpos(rng), b = dpos(rng);
        if (a > b) swap(a, b);
        ++b;
        switch (dop(rng)) {
            case 0:
                tr.insert(a, b);
                nv.insert(a, b);
                break;
            case 1:
                tr.erase(a, b);
                nv.erase(a, b);
                break;
            case 2:
                assert(tr.contains(a) == nv.contains(a));
                break;
            case 3:
                (void)tr.nearest_left(a);
                break;
            case 4:
                (void)tr.nearest_right(a);
                break;
            default:
                break;
        }
        assert(tr.interval_count() == nv.interval_count());
        assert(tr.covered_length() == nv.covered_length());
    }
    cerr << "[Random] " << OPS << " ops passed\n";
}

/* オリジナルシナリオ */
void scenario_test() {
    IntervalTreap tr;
    tr.insert(0, 10);
    tr.insert(40, 50);
    tr.insert(20, 30);
    assert(tr.interval_count() == 3);
    tr.insert(30, 40);
    assert(tr.interval_count() == 2 && tr.covered_length() == 40);
    tr.insert(-100, 200);
    assert(tr.interval_count() == 1 && tr.covered_length() == 300);
    tr.erase(-100, -90);
    tr.erase(190, 200);
    assert(tr.interval_count() == 1 && tr.covered_length() == 280);
    tr.erase(-50, 150);
    assert(tr.interval_count() == 2 && tr.covered_length() == 80);
}

/* 追加 API シナリオ */
void insert_scenarios() {
    {
        IntervalTreap tr;
        tr.insert(0, 10);
        tr.insert(20, 30);
        assert(tr.interval_count() == 2 && tr.covered_length() == 20);
    }
    {
        IntervalTreap tr;
        tr.insert(0, 5);
        tr.insert(3, 8);
        tr.insert(7, 12);
        assert(tr.interval_count() == 1 && tr.covered_length() == 12);
    }
    {
        IntervalTreap tr;
        tr.insert(10, 40);
        tr.insert(20, 30);
        assert(tr.interval_count() == 1 && tr.covered_length() == 30);
    }
}
void erase_scenarios() {
    {
        IntervalTreap tr;
        tr.insert(0, 10);
        tr.erase(0, 10);
        assert(tr.interval_count() == 0);
    }
    {
        IntervalTreap tr;
        tr.insert(0, 10);
        tr.erase(3, 7);
        auto l = tr.nearest_left(5), r = tr.nearest_right(5);
        assert(tr.interval_count() == 2 && l && l->r == 3 && r && r->l == 7);
    }
    {
        IntervalTreap tr;
        tr.insert(20, 30);
        tr.erase(25, 40);
        assert(tr.interval_count() == 1 && tr.nearest_left(29)->r == 25);
    }
}
void contains_scenarios() {
    IntervalTreap tr;
    tr.insert(0, 10);
    assert(tr.contains(5));
    assert(!tr.contains(10));
    assert(!tr.contains(100));
}

/* ★ 修正済み ★ */
void count_length_scenarios() {
    IntervalTreap tr;
    assert(tr.interval_count() == 0 && tr.covered_length() == 0);

    tr.insert(0, 5);
    tr.insert(5, 15);
    assert(tr.interval_count() == 1 && tr.covered_length() == 15);

    tr.erase(3, 12);  // ─→ 残るのは [0,3) + [12,15)
    assert(tr.interval_count() == 2 && tr.covered_length() == 6);

    /* 追加で境界も確認しておく */
    auto L = tr.nearest_left(2);
    assert(L && L->l == 0 && L->r == 3);
    auto R = tr.nearest_right(13);
    assert(R && R->l == 12 && R->r == 15);
}

void nearest_scenarios() {
    IntervalTreap tr;
    tr.insert(0, 10);
    tr.insert(20, 30);
    auto n = tr.nearest_left(5);
    assert(n && n->l == 0);
    n = tr.nearest_right(5);
    assert(n && n->l == 0);
    n = tr.nearest_left(15);
    assert(n && n->l == 0);
    n = tr.nearest_right(15);
    assert(n && n->l == 20);
    n = tr.nearest_left(100);
    assert(n && n->l == 20);
    n = tr.nearest_right(100);
    assert(n == nullptr);
}
void iterate_scenarios() {
    IntervalTreap tr;
    vector<pair<int, int>> v;
    tr.insert(30, 40);
    tr.insert(0, 10);
    tr.insert(20, 25);
    tr.iterate(
        [&](const IntervalTreap::Node* n) { v.emplace_back(n->l, n->r); });
    vector<pair<int, int>> expect = {{0, 10}, {20, 25}, {30, 40}};
    assert(v == expect);
    tr.insert(10, 30);
    v.clear();
    expect = {{0, 40}};
    tr.iterate(
        [&](const IntervalTreap::Node* n) { v.emplace_back(n->l, n->r); });
    assert(v == expect);
    tr.erase(0, 100);
    v.clear();
    tr.iterate([&](auto) { v.push_back({-1, -1}); });
    assert(v.empty());
}

/* ---------------- RNG ---------------- */
struct RNG {
    uint64_t s[2];
    static uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
    explicit RNG(uint64_t seed = 1) { splitmix64(seed); }
    void splitmix64(uint64_t z) {
        for (int i = 0; i < 2; ++i) {
            z += 0x9e3779b97f4a7c15ULL;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            s[i] = z ^ (z >> 31);
        }
    }
    uint64_t next() {
        uint64_t r = s[0] + s[1], t = s[0] ^ s[1];
        s[0] = rotl(s[0], 55) ^ t ^ (t << 14);
        s[1] = rotl(t, 36);
        return r;
    }
    uint32_t operator()() { return static_cast<uint32_t>(next()); }
    uint64_t operator()(uint64_t mod) { return next() % mod; }
} rng(123456789);

/*==================== Benchmark helpers ====================*/
template <class F>
void bench(const string& name, size_t iterations, F&& fn) {
    using namespace std::chrono;
    auto t0 = HRClk::now();
    fn();
    auto t1 = HRClk::now();
    double total_ms =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
            t1 - t0)
            .count();
    cerr << std::fixed << setprecision(3);
    cerr << "[BENCH] " << name << " : total " << total_ms << " ms"
         << " | avg " << (total_ms * 1e3 / static_cast<double>(iterations))
         << " µs/op\n";
}

/*==================== Benchmarks ====================*/
void benchmark_tests() {
    constexpr int COORD = 1'000'000;
    constexpr int REP = 10;  // iteration multiplier (10×)
    auto rnd_interval = [&]() {
        int a = static_cast<int>(rng(COORD));
        int b = static_cast<int>(rng(COORD));
        if (a > b) swap(a, b);
        return pair<int, int>{a, b + 1};
    };

    /* ---- insert 1K ---- */
    {
        vector<pair<int, int>> vec(1000);
        for (auto& pr : vec) pr = rnd_interval();
        IntervalTreap tr;
        bench("insert 1K intervals", vec.size() * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (auto [l, r] : vec) tr.insert(l, r);
        });
    }
    /* ---- insert 1M ---- */
    {
        const size_t N = 1'000'000;
        vector<pair<int, int>> vec(N);
        for (auto& pr : vec) pr = rnd_interval();
        IntervalTreap tr;
        bench("insert 1M intervals", vec.size() * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (auto [l, r] : vec) tr.insert(l, r);
        });
    }
    /* ---- mixed ops 1K state ---- */
    {
        IntervalTreap tr;
        for (int i = 0; i < 1000; ++i) {
            auto [l, r] = rnd_interval();
            tr.insert(l, r);
        }
        const int OPS = 10'000;
        bench("1K state mixed insert/erase 10K", OPS * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < OPS; ++i) {
                    auto [l, r] = rnd_interval();
                    (i & 1) ? tr.erase(l, r) : tr.insert(l, r);
                }
        });
    }
    /* ---- mixed ops 1M state ---- */
    {
        IntervalTreap tr;
        for (int i = 0; i < 1'000'000; ++i) {
            auto [l, r] = rnd_interval();
            tr.insert(l, r);
        }
        const int OPS = 100'000;
        bench("1M state mixed insert/erase 100K", OPS * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < OPS; ++i) {
                    auto [l, r] = rnd_interval();
                    (i & 1) ? tr.erase(l, r) : tr.insert(l, r);
                }
        });
    }
    /* ---- queries 1K state ---- */
    {
        IntervalTreap tr;
        for (int i = 0; i < 1000; ++i) {
            auto [l, r] = rnd_interval();
            tr.insert(l, r);
        }
        const int Q = 10'000;
        // bench("1K state find_interval",Q*REP,[&](){ for(int rep=0; rep<REP;
        // ++rep) for(int i=0;i<Q;++i)
        // tr.find_interval(static_cast<int>(rng(COORD))); });
        bench("1K state contains", Q * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < Q; ++i)
                    tr.contains(static_cast<int>(rng(COORD)));
        });
        bench("1K state nearest_left", Q * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < Q; ++i)
                    tr.nearest_left(static_cast<int>(rng(COORD)));
        });
        bench("1K state nearest_right", Q * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < Q; ++i)
                    tr.nearest_right(static_cast<int>(rng(COORD)));
        });
    }
    /* ---- queries 1M state ---- */
    {
        IntervalTreap tr;
        for (int i = 0; i < 1'000'000; ++i) {
            auto [l, r] = rnd_interval();
            tr.insert(l, r);
        }
        const int Q = 100'000;
        // bench("1M state find_interval",Q*REP,[&](){ for(int rep=0; rep<REP;
        // ++rep) for(int i=0;i<Q;++i)
        // tr.find_interval(static_cast<int>(rng(COORD))); });
        bench("1M state contains", Q * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < Q; ++i)
                    tr.contains(static_cast<int>(rng(COORD)));
        });
        bench("1M state nearest_left", Q * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < Q; ++i)
                    tr.nearest_left(static_cast<int>(rng(COORD)));
        });
        bench("1M state nearest_right", Q * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (int i = 0; i < Q; ++i)
                    tr.nearest_right(static_cast<int>(rng(COORD)));
        });
    }
    /* ---- iterate 1K & 1M ---- */
    {
        IntervalTreap tr1, tr2;
        long long dummy = 0;
        for (int i = 0; i < 1000; ++i) {
            auto [l, r] = rnd_interval();
            tr1.insert(l, r);
        }
        for (int i = 0; i < 1'000'000; ++i) {
            auto [l, r] = rnd_interval();
            tr2.insert(l, r);
        }
        bench("iterate 1K", REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                tr1.iterate([&](const auto* n) { dummy += n->sz; });
        });
        bench("iterate 1M", REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                tr2.iterate([&](const auto* n) { dummy += n->sz; });
        });
    }
}

/* ------------------------------------------------------------------ */
/*                                  main                              */
/* ------------------------------------------------------------------ */
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cerr << "[TEST] basic unit tests...\n";
    basic_unit_tests();
    cerr << "  passed\n";
    cerr << "[TEST] exhaustive random (1e5 ops)...\n";
    exhaustive_random_test(100000, 1'000'000);
    cerr << "  passed\n";
    cerr << "[TEST] scenario (original)...\n";
    scenario_test();
    cerr << "  passed\n";
    cerr << "[TEST] insert scenarios...\n";
    insert_scenarios();
    cerr << "  passed\n";
    cerr << "[TEST] erase scenarios...\n";
    erase_scenarios();
    cerr << "  passed\n";
    cerr << "[TEST] contains scenarios...\n";
    contains_scenarios();
    cerr << "  passed\n";
    cerr << "[TEST] count/length scenarios...\n";
    count_length_scenarios();
    cerr << "  passed\n";
    cerr << "[TEST] nearest scenarios...\n";
    nearest_scenarios();
    cerr << "  passed\n";
    cerr << "[TEST] iterate scenarios...\n";
    iterate_scenarios();
    cerr << "  passed\n";

    cout << "All tests passed\n";

    cerr << "[BENCH] start benchmarks...\n";
    benchmark_tests();

    return 0;
}

// 実行結果（ローカル）
// [TEST] basic unit tests...
//   passed
// [TEST] exhaustive random (1e5 ops)...
// [Random] 100000 ops passed
//   passed
// [TEST] scenario (original)...
//   passed
// [TEST] insert scenarios...
//   passed
// [TEST] erase scenarios...
//   passed
// [TEST] contains scenarios...
//   passed
// [TEST] count/length scenarios...
//   passed
// [TEST] nearest scenarios...
//   passed
// [TEST] iterate scenarios...
//   passed
// All tests passed
// [BENCH] start benchmarks...
// [BENCH] insert 1K intervals : total 0.184 ms | avg 0.018 µs/op
// [BENCH] insert 1M intervals : total 198.738 ms | avg 0.020 µs/op
// [BENCH] 1K state mixed insert/erase 10K : total 16.044 ms | avg 0.160 µs/op
// [BENCH] 1M state mixed insert/erase 100K : total 187.618 ms | avg 0.188 µs/op
// [BENCH] 1K state contains : total 0.202 ms | avg 0.002 µs/op
// [BENCH] 1K state nearest_left : total 0.202 ms | avg 0.002 µs/op
// [BENCH] 1K state nearest_right : total 0.289 ms | avg 0.003 µs/op
// [BENCH] 1M state contains : total 1.753 ms | avg 0.002 µs/op
// [BENCH] 1M state nearest_left : total 1.665 ms | avg 0.002 µs/op
// [BENCH] 1M state nearest_right : total 2.914 ms | avg 0.003 µs/op
// [BENCH] iterate 1K : total 0.001 ms | avg 0.077 µs/op
// [BENCH] iterate 1M : total 0.000 ms | avg 0.046 µs/op


// 実行結果(AtCoder)
// [TEST] basic unit tests...
//   passed
// [TEST] exhaustive random (1e5 ops)...
// [Random] 100000 ops passed
//   passed
// [TEST] scenario (original)...
//   passed
// [TEST] insert scenarios...
//   passed
// [TEST] erase scenarios...
//   passed
// [TEST] contains scenarios...
//   passed
// [TEST] count/length scenarios...
//   passed
// [TEST] nearest scenarios...
//   passed
// [TEST] iterate scenarios...
//   passed
// [BENCH] start benchmarks...
// [BENCH] insert 1K intervals : total 0.111 ms | avg 0.011 µs/op
// [BENCH] insert 1M intervals : total 110.804 ms | avg 0.011 µs/op
// [BENCH] 1K state mixed insert/erase 10K : total 9.255 ms | avg 0.093 µs/op
// [BENCH] 1M state mixed insert/erase 100K : total 105.636 ms | avg 0.106 µs/op
// [BENCH] 1K state contains : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_left : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_right : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1M state contains : total 1.009 ms | avg 0.001 µs/op
// [BENCH] 1M state nearest_left : total 1.002 ms | avg 0.001 µs/op
// [BENCH] 1M state nearest_right : total 1.001 ms | avg 0.001 µs/op
// [BENCH] iterate 1K : total 0.000 ms | avg 0.029 µs/op
// [BENCH] iterate 1M : total 0.000 ms | avg 0.022 µs/op
