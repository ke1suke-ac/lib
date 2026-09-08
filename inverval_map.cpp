// ===============================================================
// OrderedIntervalMap.cpp
//   C++20  (g++‑12.2)
//   build: g++ -std=c++20 -O2 -Wall -Wextra OrderedIntervalMap.cpp
// ===============================================================
#include <bits/stdc++.h>
using namespace std;
using HRClk = std::chrono::high_resolution_clock;

//--------------------------------------------------
// 動的区間管理ライブラリ（半開 [l, r)）+ ロールバック
//--------------------------------------------------
namespace interval {

template <class T = long long>
class OrderedIntervalMap {
    static_assert(std::is_integral_v<T>,
                  "T は符号付き整数型である必要があります");

    // 左端 → 右端 (exclusive)
    std::map<T, T> mp_;

    //--------------------------------------------------
    // 差分ログ
    //--------------------------------------------------
    struct Change { T l, r; bool inserted; };
    std::vector<Change> log_;

    void log_insert(T l, T r) { log_.push_back({l, r, true}); }
    void log_erase (T l, T r) { log_.push_back({l, r, false}); }

  public:
    using iterator = typename std::map<T, T>::const_iterator;

    iterator begin() const noexcept { return mp_.begin(); }
    iterator end()   const noexcept { return mp_.end();   }

    //――― チェックポイント / ロールバック ―――
    [[nodiscard]] int checkpoint() const noexcept { return (int)log_.size(); }

    void rollback(int snap) {
        while ((int)log_.size() > snap) {
            auto [l, r, ins] = log_.back();
            log_.pop_back();
            if (ins) {
                // 挿入を巻き戻す → 区間 [l,r) を削除
                auto it = mp_.find(l);
                if (it != mp_.end() && it->second == r) mp_.erase(it);
            } else {
                // 削除を巻き戻す → 区間 [l,r) を再挿入
                mp_.emplace(l, r);
            }
        }
    }

    //――― insert : [l,r) を追加（併合）―――
    void insert(T l, T r) {
        if (l >= r) return;
        auto it = mp_.lower_bound(l);
        if (it != mp_.begin() && std::prev(it)->second >= l) it = std::prev(it);

        while (it != mp_.end() && it->first <= r) {
            log_erase(it->first, it->second);          // ログ
            l = std::min(l, it->first);
            r = std::max(r, it->second);
            it = mp_.erase(it);
        }
        mp_.emplace(l, r);
        log_insert(l, r);                              // ログ
    }

    //――― erase : [l,r) を削除（必要なら分割）―――
    void erase(T l, T r) {
        if (l >= r) return;
        auto it = mp_.lower_bound(l);
        if (it != mp_.begin() && std::prev(it)->second > l) it = std::prev(it);

        std::vector<std::pair<T, T>> rest;
        while (it != mp_.end() && it->first < r) {
            log_erase(it->first, it->second);          // 削除ログ
            if (it->first < l) rest.emplace_back(it->first, l);
            if (it->second > r) rest.emplace_back(r, it->second);
            it = mp_.erase(it);
        }
        for (auto [nl, nr] : rest) {
            mp_.emplace(nl, nr);
            log_insert(nl, nr);                        // 挿入ログ
        }
    }

    //――― contains / nearest_left ―――
    [[nodiscard]] bool contains(T x) const {
        auto it = mp_.upper_bound(x);
        if (it == mp_.begin()) return false;
        --it;
        return it->second > x;
    }

    iterator nearest_left(T x) const {
        auto it = mp_.upper_bound(x);
        if (it == mp_.begin()) return mp_.end();
        return std::prev(it);
    }

    //――― 補助 ―――
    [[nodiscard]] size_t interval_count() const noexcept { return mp_.size(); }

    [[nodiscard]] T covered_length() const {
        T sum = 0;
        for (auto [l, r] : mp_) sum += (r - l);
        return sum;
    }
};

} // namespace interval

//------------------------------------------------------------------
// テストユーティリティ
//------------------------------------------------------------------
using interval::OrderedIntervalMap;

#define ASSERT_TRUE(expr)                                                      \
    do {                                                                       \
        if (!(expr)) {                                                         \
            cerr << "Assertion failed at line " << __LINE__ << ": " << #expr   \
                 << '\n';                                                      \
            exit(1);                                                           \
        }                                                                      \
    } while (false)

template <class T>
vector<pair<T, T>> dump(const OrderedIntervalMap<T>& im) {
    vector<pair<T, T>> v;
    for (auto [l, r] : im) v.emplace_back(l, r);
    return v;
}

template <class T>
void expect_eq(const vector<pair<T, T>>& got,
               const vector<pair<T, T>>& exp,
               string_view name) {
    if (got != exp) {
        cerr << "[FAIL] " << name << "\n  got:";
        for (auto [l, r] : got) cerr << " [" << l << "," << r << ")";
        cerr << "\n  exp:";
        for (auto [l, r] : exp) cerr << " [" << l << "," << r << ")";
        cerr << '\n';
        exit(1);
    }
}

//------------------------------------------------------------------
// ユニットテスト
//------------------------------------------------------------------
void unit_tests() {
    using I = long long;
    OrderedIntervalMap<I> im;

    // 1. insert / merge
    im.insert(10, 20);
    im.insert(30, 40);
    expect_eq<I>(dump(im), {{10, 20}, {30, 40}}, "insert-basic");

    im.insert(20, 30);                              // merge to [10,40)
    expect_eq<I>(dump(im), {{10, 40}}, "merge");

    // 2. contains
    ASSERT_TRUE(im.contains(15));
    ASSERT_TRUE(!im.contains(41));

    // 3. erase → split
    im.erase(15, 25);                           // → [10,15) , [25,40)
    expect_eq<I>(dump(im), {{10, 15}, {25, 40}}, "erase-split");

    // 4. nearest_left
    ASSERT_TRUE(im.nearest_left(5) == im.end());           // x 左に区間なし
    ASSERT_TRUE(im.nearest_left(17)->first == 10);
    ASSERT_TRUE(im.nearest_left(100)->first == 25);

    // 5. covered_length
    ASSERT_TRUE(im.covered_length() == 20);

    // 6. rollback
    int snap0 = im.checkpoint();                 // [10,15) , [25,40)
    im.insert(50, 60);
    im.erase(10, 12);
    int snap1 = im.checkpoint();                 // [12,15) ,[25,40) ,[50,60)
    im.insert(0, 100);                              // huge interval
    im.rollback(snap1);
    expect_eq<I>(dump(im), {{12, 15}, {25, 40}, {50, 60}}, "rollback-snap1");
    im.rollback(snap0);
    expect_eq<I>(dump(im), {{10, 15}, {25, 40}}, "rollback-snap0");

    cerr << "All unit tests passed.\n";
}

//------------------------------------------------------------------
// シナリオテスト（ランダム insert/erase + rollback）
//------------------------------------------------------------------
void scenario_test() {
    using I = int;
    constexpr I LIM = 1'000'000;
    mt19937 rng(20250417);
    uniform_int_distribution<I> dist(0, LIM);

    OrderedIntervalMap<I> fast;
    struct Iv { I l, r; };
    vector<Iv> brute;

    vector<int> ck_stack;           // fast 側チェックポイント
    vector<vector<Iv>> brute_stack;

    auto rebuild = [&] {            // brute をマージ
        vector<Iv> v;
        for (auto [l, r] : brute) if (l < r) v.push_back({l, r});
        if (v.empty()) { brute.clear(); return; }
        sort(v.begin(), v.end(), [](auto a, auto b){ return a.l < b.l; });
        vector<Iv> m;
        I cl = v[0].l, cr = v[0].r;
        for (size_t i = 1; i < v.size(); ++i) {
            if (v[i].l <= cr) cr = max(cr, v[i].r);
            else { m.push_back({cl, cr}); cl = v[i].l; cr = v[i].r; }
        }
        m.push_back({cl, cr});
        brute.swap(m);
    };

    constexpr int OPS = 30000;
    for (int op = 0; op < OPS; ++op) {
        int act = rng() % 100;
        if (act < 45) {                       // insert
            I l = dist(rng), r = dist(rng); if (l > r) swap(l, r); ++r;
            fast.insert(l, r);
            brute.push_back({l, r});
            rebuild();
        } else if (act < 90) {                // erase
            I l = dist(rng), r = dist(rng); if (l > r) swap(l, r); ++r;
            fast.erase(l, r);
            vector<Iv> nxt;
            for (auto [bl, br] : brute) {
                if (br <= l || r <= bl) { nxt.push_back({bl, br}); continue; }
                if (bl < l) nxt.push_back({bl, l});
                if (r < br) nxt.push_back({r, br});
            }
            brute.swap(nxt);
        } else if (act < 95) {                // checkpoint push
            ck_stack.push_back(fast.checkpoint());
            brute_stack.push_back(brute);
        } else if (!ck_stack.empty()) {       // rollback pop
            fast.rollback(ck_stack.back()); ck_stack.pop_back();
            brute = brute_stack.back(); brute_stack.pop_back();
        }

        // 整合性チェック
        auto got = dump(fast);
        vector<pair<I,I>> exp;
        for (auto [l,r] : brute) exp.emplace_back(l,r);
        if (got != exp) {
            cerr << "Scenario mismatch at op=" << op << '\n';
            exit(1);
        }
    }
    cerr << "Scenario test passed (" << OPS << " ops)\n";
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
        OrderedIntervalMap tr;
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
        OrderedIntervalMap tr;
        bench("insert 1M intervals", vec.size() * REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for (auto [l, r] : vec) tr.insert(l, r);
        });
    }
    /* ---- mixed ops 1K state ---- */
    {
        OrderedIntervalMap tr;
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
        OrderedIntervalMap tr;
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
        OrderedIntervalMap tr;
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
        // bench("1K state nearest_right", Q * REP, [&]() {
        //     for (int rep = 0; rep < REP; ++rep)
        //         for (int i = 0; i < Q; ++i)
        //             tr.nearest_right(static_cast<int>(rng(COORD)));
        // });
    }
    /* ---- queries 1M state ---- */
    {
        OrderedIntervalMap tr;
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
        // bench("1M state nearest_right", Q * REP, [&]() {
        //     for (int rep = 0; rep < REP; ++rep)
        //         for (int i = 0; i < Q; ++i)
        //             tr.nearest_right(static_cast<int>(rng(COORD)));
        // });
    }
    /* ---- iterate 1K & 1M ---- */
    {
        OrderedIntervalMap tr1, tr2;
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
            for (int rep = 0; rep < REP; ++rep){
                for(auto it: tr1){
                    dummy += it.first;
                }
            }
        });
        bench("iterate 1M", REP, [&]() {
            for (int rep = 0; rep < REP; ++rep)
                for(auto it: tr2){
                    dummy += it.first;
                }
        });
    }
}

//------------------------------------------------------------------
// main
//------------------------------------------------------------------
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    unit_tests();
    scenario_test();

    cerr << "[BENCH] start benchmarks...\n";
    benchmark_tests();

    return 0;
}

// 実行結果（ローカル）
// All unit tests passed.
// Scenario test passed (30000 ops)
// [BENCH] start benchmarks...
// [BENCH] insert 1K intervals : total 1.482 ms | avg 0.148 µs/op
// [BENCH] insert 1M intervals : total 1154.710 ms | avg 0.115 µs/op
// [BENCH] 1K state mixed insert/erase 10K : total 20.965 ms | avg 0.210 µs/op
// [BENCH] 1M state mixed insert/erase 100K : total 180.842 ms | avg 0.181 µs/op
// [BENCH] 1K state contains : total 0.150 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_left : total 0.189 ms | avg 0.002 µs/op
// [BENCH] 1M state contains : total 1.514 ms | avg 0.002 µs/op
// [BENCH] 1M state nearest_left : total 1.546 ms | avg 0.002 µs/op
// [BENCH] iterate 1K : total 0.000 ms | avg 0.011 µs/op
// [BENCH] iterate 1M : total 0.000 ms | avg 0.004 µs/op

// 実行結果（AtCoder）
// All unit tests passed.
// Scenario test passed (30000 ops)
// [BENCH] start benchmarks...
// [BENCH] insert 1K intervals : total 0.718 ms | avg 0.072 µs/op
// [BENCH] insert 1M intervals : total 871.432 ms | avg 0.087 µs/op
// [BENCH] 1K state mixed insert/erase 10K : total 12.115 ms | avg 0.121 µs/op
// [BENCH] 1M state mixed insert/erase 100K : total 177.154 ms | avg 0.177 µs/op
// [BENCH] 1K state contains : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_left : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1M state contains : total 1.002 ms | avg 0.001 µs/op
// [BENCH] 1M state nearest_left : total 1.000 ms | avg 0.001 µs/op
// [BENCH] iterate 1K : total 0.000 ms | avg 0.005 µs/op
// [BENCH] iterate 1M : total 0.000 ms | avg 0.003 µs/op

