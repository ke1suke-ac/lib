/*
 * mex_calculator: mex を高速に求める小型構造体群
 *
 * mex_bitmask64 は mex が 64 以下の場面向けの uint64_t 実装
 * mex_timestamp は多数回の mex 計算で初期化コストを抑える timestamp 配列実装
 *
 * 各構造体は clear(), add(x), mex() の最小 API を持つ
 * add(x) は保持範囲外の値を無視し、mex() は保持範囲がすべて埋まった場合に capacity を返す
 */
#pragma once

#include <bits/stdc++.h>

struct mex_bitmask64 {
private:
    std::uint64_t mask = 0;

public:
    static constexpr int capacity = 64;

    // 現在の集合を空にする、O(1)
    void clear() {
        mask = 0;
    }

    // 値 x を追加する、O(1)
    void add(int x) {
        if (x < 0 || x >= capacity) {
            return;
        }
        mask |= std::uint64_t{1} << static_cast<unsigned>(x);
    }

    // 現在の集合の mex を返す、O(1)
    int mex() const {
        const std::uint64_t inv = ~mask;
        if (inv == 0) {
            return capacity;
        }
        return __builtin_ctzll(inv);
    }
};

struct mex_timestamp {
private:
    std::vector<int> stamp;
    int token = 1;
    int limit = 0;

public:
    // capacity 個の保持領域を作る、O(capacity)
    explicit mex_timestamp(int capacity) {
        if (capacity < 0) {
            capacity = 0;
        }
        limit = capacity;
        stamp.assign(static_cast<std::size_t>(limit), 0);
    }

    // 現在の集合を空にする、償却 O(1)
    void clear() {
        if (token == std::numeric_limits<int>::max()) {
            std::fill(stamp.begin(), stamp.end(), 0);
            token = 1;
            return;
        }
        ++token;
    }

    // 値 x を追加する、O(1)
    void add(int x) {
        if (x < 0 || x >= limit) {
            return;
        }
        stamp[static_cast<std::size_t>(x)] = token;
    }

    // 現在の集合の mex を返す、O(mex+1)
    int mex() const {
        for (int i = 0; i < limit; ++i) {
            if (stamp[static_cast<std::size_t>(i)] != token) {
                return i;
            }
        }
        return limit;
    }
};

#if __INCLUDE_LEVEL__ == 0

static volatile std::uint64_t benchmark_sink = 0;

static void benchmark_barrier(const void* ptr) {
    asm volatile("" : : "g"(ptr) : "memory");
}

static void consume_u64(std::uint64_t value) {
    benchmark_sink = value;
}

static int naive_mex_capacity(const std::vector<int>& values, int capacity) {
    if (capacity < 0) {
        capacity = 0;
    }

    std::vector<char> seen(static_cast<std::size_t>(capacity), 0);
    for (int x : values) {
        if (0 <= x && x < capacity) {
            seen[static_cast<std::size_t>(x)] = 1;
        }
    }

    for (int i = 0; i < capacity; ++i) {
        if (seen[static_cast<std::size_t>(i)] == 0) {
            return i;
        }
    }
    return capacity;
}

template <class Mex>
static int run_mex(Mex& mx, const std::vector<int>& values) {
    mx.clear();
    for (int x : values) {
        mx.add(x);
    }
    return mx.mex();
}

static std::uint32_t next_random(std::uint32_t& state) {
    state = state * 1664525U + 1013904223U;
    return state;
}

static int random_int(std::uint32_t& state, int low, int high) {
    const std::uint32_t width = static_cast<std::uint32_t>(high - low + 1);
    return low + static_cast<int>(next_random(state) % width);
}

static std::vector<int> make_random_values(int count, int low, int high, std::uint32_t seed) {
    std::vector<int> values(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        values[static_cast<std::size_t>(i)] = random_int(seed, low, high);
    }
    return values;
}

static void test_mex_bitmask64() {
    mex_bitmask64 mx;
    assert(mx.mex() == 0);

    mx.add(1);
    assert(mx.mex() == 0);

    mx.clear();
    mx.add(0);
    assert(mx.mex() == 1);

    mx.add(-1);
    mx.add(64);
    mx.add(1000);
    assert(mx.mex() == 1);

    for (int missing = 0; missing <= mex_bitmask64::capacity; ++missing) {
        std::vector<int> values;
        for (int x = 0; x < mex_bitmask64::capacity; ++x) {
            if (x != missing) {
                values.push_back(x);
            }
        }
        assert(run_mex(mx, values) == missing);
    }

    for (int bit_count = 0; bit_count <= 12; ++bit_count) {
        const int pattern_count = 1 << bit_count;
        for (int pattern = 0; pattern < pattern_count; ++pattern) {
            std::vector<int> values;
            for (int bit = 0; bit < bit_count; ++bit) {
                if ((pattern & (1 << bit)) != 0) {
                    values.push_back(bit);
                    values.push_back(bit);
                }
            }
            values.push_back(-5);
            values.push_back(100);
            const int expected = naive_mex_capacity(values, mex_bitmask64::capacity);
            assert(run_mex(mx, values) == expected);
        }
    }

    std::uint32_t seed = 1;
    for (int case_id = 0; case_id < 5000; ++case_id) {
        const int count = random_int(seed, 0, 150);
        std::vector<int> values;
        values.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            values.push_back(random_int(seed, -20, 100));
        }
        const int expected = naive_mex_capacity(values, mex_bitmask64::capacity);
        assert(run_mex(mx, values) == expected);
    }
}

static void test_mex_timestamp() {
    const std::vector<int> capacities = {0, 1, 2, 64, 128, 200};
    for (int capacity : capacities) {
        mex_timestamp mx(capacity);
        assert(mx.mex() == 0);

        std::vector<int> full_values;
        for (int x = 0; x < capacity; ++x) {
            full_values.push_back(x);
        }
        assert(run_mex(mx, full_values) == capacity);

        for (int missing = 0; missing <= capacity; ++missing) {
            std::vector<int> values;
            for (int x = 0; x < capacity; ++x) {
                if (x != missing) {
                    values.push_back(x);
                    values.push_back(x);
                }
            }
            values.push_back(-10);
            values.push_back(capacity);
            values.push_back(capacity + 100);
            assert(run_mex(mx, values) == missing);
        }
    }

    std::uint32_t seed = 7;
    mex_timestamp mx(300);
    for (int case_id = 0; case_id < 10000; ++case_id) {
        mx.clear();
        const int count = random_int(seed, 0, 500);
        std::vector<int> values;
        values.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            const int x = random_int(seed, -50, 400);
            values.push_back(x);
            mx.add(x);
        }
        assert(mx.mex() == naive_mex_capacity(values, 300));
    }
}

static void test_cross_implementation_random() {
    std::uint32_t seed = 8;
    mex_bitmask64 mx64;
    mex_timestamp ts64(64);

    for (int case_id = 0; case_id < 5000; ++case_id) {
        const int count = random_int(seed, 0, 160);
        std::vector<int> values;
        values.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            values.push_back(random_int(seed, -80, 120));
        }

        const int expected = naive_mex_capacity(values, 64);
        assert(run_mex(mx64, values) == expected);
        assert(run_mex(ts64, values) == expected);
    }
}

template <class Func>
static long long measure_ns(Func&& func) {
    const auto start = std::chrono::steady_clock::now();
    func();
    const auto finish = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
}

static void print_benchmark_result(const std::string& name, int operations, long long ns) {
    const double ms = static_cast<double>(ns) / 1000000.0;
    const double ns_per_operation = static_cast<double>(ns) / static_cast<double>(operations);
    std::cout << std::left << std::setw(34) << name
              << std::right << std::setw(12) << operations
              << std::setw(14) << std::fixed << std::setprecision(3) << ms
              << std::setw(14) << std::fixed << std::setprecision(3) << ns_per_operation
              << '\n';
}

static void benchmark_barrier_baseline(int iterations) {
    int value = 0;
    const long long ns = measure_ns([&]() {
        for (int iter = 0; iter < iterations; ++iter) {
            benchmark_barrier(&value);
        }
    });
    consume_u64(static_cast<std::uint64_t>(value));
    print_benchmark_result("barrier baseline", iterations, ns);
}

template <class Mex>
static void benchmark_clear(const std::string& name, Mex& mx, int iterations) {
    const long long ns = measure_ns([&]() {
        for (int iter = 0; iter < iterations; ++iter) {
            mx.clear();
            benchmark_barrier(&mx);
        }
    });
    consume_u64(static_cast<std::uint64_t>(mx.mex()));
    print_benchmark_result(name, iterations, ns);
}

template <class Mex>
static void benchmark_add(const std::string& name, Mex& mx, const std::vector<int>& values, int iterations) {
    const std::size_t mask = values.size() - 1;
    mx.clear();

    const long long ns = measure_ns([&]() {
        for (int iter = 0; iter < iterations; ++iter) {
            mx.add(values[static_cast<std::size_t>(iter) & mask]);
            benchmark_barrier(&mx);
        }
    });
    consume_u64(static_cast<std::uint64_t>(mx.mex()));
    print_benchmark_result(name, iterations, ns);
}

template <class Mex>
static void benchmark_mex_dense(const std::string& name, Mex& mx, int fill_count, int iterations) {
    mx.clear();
    for (int x = 0; x < fill_count; ++x) {
        mx.add(x);
    }

    std::uint64_t acc = 0;
    const long long ns = measure_ns([&]() {
        for (int iter = 0; iter < iterations; ++iter) {
            benchmark_barrier(&mx);
            acc += static_cast<std::uint64_t>(mx.mex());
        }
    });
    consume_u64(acc);
    print_benchmark_result(name, iterations, ns);
}

template <class Mex>
static void benchmark_grundy_random(const std::string& name, Mex& mx, const std::vector<int>& values, int states, int degree) {
    const std::size_t mask = values.size() - 1;
    std::size_t pos = 0;
    std::uint64_t acc = 0;

    const long long ns = measure_ns([&]() {
        for (int state_id = 0; state_id < states; ++state_id) {
            mx.clear();
            for (int edge_id = 0; edge_id < degree; ++edge_id) {
                mx.add(values[pos & mask]);
                ++pos;
            }
            acc += static_cast<std::uint64_t>(mx.mex());
            benchmark_barrier(&mx);
        }
    });
    consume_u64(acc);
    print_benchmark_result(name, states, ns);
}

template <class Mex>
static void benchmark_grundy_dense_sequence(const std::string& name, Mex& mx, int states, int degree) {
    std::uint64_t acc = 0;

    const long long ns = measure_ns([&]() {
        for (int state_id = 0; state_id < states; ++state_id) {
            mx.clear();
            for (int x = 0; x < degree; ++x) {
                mx.add(x);
            }
            acc += static_cast<std::uint64_t>(mx.mex());
            benchmark_barrier(&mx);
        }
    });
    consume_u64(acc);
    print_benchmark_result(name, states, ns);
}

static void run_benchmarks() {
    std::cout << "\n[benchmark] basic clear operation\n";
    std::cout << std::left << std::setw(34) << "name"
              << std::right << std::setw(12) << "ops"
              << std::setw(14) << "ms"
              << std::setw(14) << "ns/op"
              << '\n';

    const int clear_iterations = 3000000;
    benchmark_barrier_baseline(clear_iterations);

    mex_bitmask64 clear64;
    mex_timestamp clear_ts64(64);
    mex_timestamp clear_ts512(512);
    mex_timestamp clear_ts4096(4096);

    benchmark_clear("clear bitmask64", clear64, clear_iterations);
    benchmark_clear("clear timestamp(64)", clear_ts64, clear_iterations);
    benchmark_clear("clear timestamp(512)", clear_ts512, clear_iterations);
    benchmark_clear("clear timestamp(4096)", clear_ts4096, clear_iterations);

    std::cout << "\n[benchmark] add operation\n";
    std::cout << std::left << std::setw(34) << "name"
              << std::right << std::setw(12) << "ops"
              << std::setw(14) << "ms"
              << std::setw(14) << "ns/op"
              << '\n';

    const int add_iterations = 5000000;
    const std::vector<int> values64 = make_random_values(65536, 0, 63, 101);
    const std::vector<int> values512 = make_random_values(65536, 0, 511, 102);

    mex_bitmask64 add64;
    mex_timestamp add_ts64(64);
    mex_timestamp add_ts512(512);

    benchmark_add("add bitmask64", add64, values64, add_iterations);
    benchmark_add("add timestamp(64)", add_ts64, values64, add_iterations);
    benchmark_add("add timestamp(512)", add_ts512, values512, add_iterations);

    std::cout << "\n[benchmark] dense mex operation\n";
    std::cout << std::left << std::setw(34) << "name"
              << std::right << std::setw(12) << "ops"
              << std::setw(14) << "ms"
              << std::setw(14) << "ns/op"
              << '\n';

    const int mex_iterations = 1000000;
    mex_bitmask64 mex64;
    mex_timestamp mex_ts64(64);
    mex_timestamp mex_ts512(512);

    benchmark_mex_dense("mex bitmask64 filled 0..62", mex64, 63, mex_iterations);
    benchmark_mex_dense("mex bitmask64 filled 0..63", mex64, 64, mex_iterations);
    benchmark_mex_dense("mex timestamp(64) filled 0..62", mex_ts64, 63, mex_iterations);
    benchmark_mex_dense("mex timestamp(64) filled 0..63", mex_ts64, 64, mex_iterations);
    benchmark_mex_dense("mex timestamp(512) filled 0..510", mex_ts512, 511, mex_iterations);

    std::cout << "\n[benchmark] suitable operation sequences\n";
    std::cout << std::left << std::setw(34) << "name"
              << std::right << std::setw(12) << "states"
              << std::setw(14) << "ms"
              << std::setw(14) << "ns/state"
              << '\n';

    const std::vector<int> random64 = make_random_values(65536, 0, 63, 201);
    const std::vector<int> sparse64 = make_random_values(65536, 16, 63, 202);
    const std::vector<int> sparse4096 = make_random_values(65536, 64, 4095, 203);

    const int small_states = 500000;
    const int small_degree = 16;
    mex_bitmask64 small64;
    mex_timestamp small_ts64(64);
    benchmark_grundy_random("small degree16 bitmask64", small64, random64, small_states, small_degree);
    benchmark_grundy_random("small degree16 timestamp(64)", small_ts64, random64, small_states, small_degree);

    const int dense_states = 120000;
    const int dense_degree = 64;
    mex_bitmask64 dense64;
    mex_timestamp dense_ts64(64);
    benchmark_grundy_dense_sequence("dense 0..63 bitmask64", dense64, dense_states, dense_degree);
    benchmark_grundy_dense_sequence("dense 0..63 timestamp(64)", dense_ts64, dense_states, dense_degree);

    const int sparse_states = 800000;
    const int sparse_degree = 8;
    mex_bitmask64 sparse_bm64;
    mex_timestamp sparse_ts64(64);
    mex_timestamp sparse_ts4096(4096);
    benchmark_grundy_random("sparse cap64 bitmask64", sparse_bm64, sparse64, sparse_states, sparse_degree);
    benchmark_grundy_random("sparse cap64 timestamp(64)", sparse_ts64, sparse64, sparse_states, sparse_degree);
    benchmark_grundy_random("sparse cap4096 timestamp", sparse_ts4096, sparse4096, sparse_states, sparse_degree);

    std::cout << "benchmark_sink=" << benchmark_sink << '\n';
}

int main() {
    test_mex_bitmask64();
    test_mex_timestamp();
    test_cross_implementation_random();

    std::cout << "all functional tests passed\n";
    run_benchmarks();
    return 0;
}

#endif

// 実行結果(atcoder)
// all functional tests passed

// [benchmark] basic clear operation
// name                                       ops            ms         ns/op
// barrier baseline                       3000000         0.469         0.156
// clear bitmask64                        3000000         0.954         0.318
// clear timestamp(64)                    3000000         0.949         0.316
// clear timestamp(512)                   3000000         0.946         0.315
// clear timestamp(4096)                  3000000         0.947         0.316

// [benchmark] add operation
// name                                       ops            ms         ns/op
// add bitmask64                          5000000        10.553         2.111
// add timestamp(64)                      5000000         4.121         0.824
// add timestamp(512)                     5000000         4.185         0.837

// [benchmark] dense mex operation
// name                                       ops            ms         ns/op
// mex bitmask64 filled 0..62             1000000         0.516         0.516
// mex bitmask64 filled 0..63             1000000         0.506         0.506
// mex timestamp(64) filled 0..62         1000000        21.493        21.493
// mex timestamp(64) filled 0..63         1000000        22.248        22.248
// mex timestamp(512) filled 0..510       1000000       181.013       181.013

// [benchmark] suitable operation sequences
// name                                    states            ms      ns/state
// small degree16 bitmask64                500000         6.506        13.013
// small degree16 timestamp(64)            500000         6.022        12.043
// dense 0..63 bitmask64                   120000         2.794        23.280
// dense 0..63 timestamp(64)               120000         5.116        42.630
// sparse cap64 bitmask64                  800000         4.468         5.584
// sparse cap64 timestamp(64)              800000         5.007         6.259
// sparse cap4096 timestamp                800000         5.771         7.214
// benchmark_sink=0
