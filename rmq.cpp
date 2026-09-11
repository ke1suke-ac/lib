#include <bits/stdc++.h>
#include <atcoder/segtree>

using namespace std;

//==============================================================
// ナイーブ RMQ（検証用）
//==============================================================

struct NaiveRMQ {
    vector<int> a;
    void build(const vector<int>& v) {
        a = v;
    }
    int query(int l, int r) const {
        if (l > r) std::swap(l, r);
        int res = a[l];
        for (int i = l + 1; i <= r; ++i) {
            if (a[i] < res) res = a[i];
        }
        return res;
    }
};

//==============================================================
// 1. ACL segtree を使った RMQ
//==============================================================

int seg_op(int a, int b) { return std::min(a, b); }
int seg_e() { return std::numeric_limits<int>::max(); }

struct SegmentTreeRMQ {
    atcoder::segtree<int, seg_op, seg_e> seg;
    int n = 0;

    void build(const vector<int>& a) {
        n = (int)a.size();
        seg = atcoder::segtree<int, seg_op, seg_e>(a);
    }

    int query(int l, int r) const {
        if (l > r) std::swap(l, r);
        return seg.prod(l, r + 1); // [l, r]
    }
};

//==============================================================
// 2. Sparse Table RMQ
//==============================================================

struct SparseTableRMQ {
    vector<vector<int>> st;  // st[k][i] : 長さ 2^k 区間の最小値
    vector<int> lg;          // lg[i] : floor(log2(i))
    int n = 0;

    void build(const vector<int>& a) {
        n = (int)a.size();
        if (n == 0) return;

        int K = 1;
        while ((1 << K) <= n) ++K;
        st.assign(K, vector<int>(n));
        st[0] = a;

        for (int k = 1; k < K; ++k) {
            int len = 1 << k;
            int half = len >> 1;
            for (int i = 0; i + len <= n; ++i) {
                st[k][i] = std::min(st[k - 1][i], st[k - 1][i + half]);
            }
        }

        lg.assign(n + 1, 0);
        for (int i = 2; i <= n; ++i) lg[i] = lg[i >> 1] + 1;
    }

    int query(int l, int r) const {
        if (l > r) std::swap(l, r);
        int len = r - l + 1;
        int k = lg[len];
        int x = st[k][l];
        int y = st[k][r - (1 << k) + 1];
        return std::min(x, y);
    }
};

//==============================================================
// 3. ±1 配列 RMQ 用 テーブル (Block RMQ: Table Lookup)
//==============================================================

struct BlockRMQTable {
    static const int S = 16;
    // table[pattern][i*S + j] = (argmin position in [i, j]) - i
    vector<vector<uint8_t>> table;

    BlockRMQTable() {
        const int patterns = 1 << (S - 1);
        table.assign(patterns, vector<uint8_t>(S * S, 0));
        vector<int> d(S);

        for (int p = 0; p < patterns; ++p) {
            d[0] = 0;
            for (int k = 1; k < S; ++k) {
                // bit=1 を +1, bit=0 を -1 としてエンコード
                if (p & (1 << (k - 1))) d[k] = d[k - 1] + 1;
                else d[k] = d[k - 1] - 1;
            }
            for (int i = 0; i < S; ++i) {
                int min_pos = i;
                int min_val = d[i];
                for (int j = i; j < S; ++j) {
                    if (d[j] < min_val) {
                        min_val = d[j];
                        min_pos = j;
                    }
                    table[p][i * S + j] = (uint8_t)(min_pos - i);
                }
            }
        }
    }
};

//==============================================================
// ±1 配列上の RMQ (O(n) build, O(1) query)
//==============================================================

struct RMQPlusMinusOne {
    static BlockRMQTable bt;
    static const int S = BlockRMQTable::S;

    vector<int> a;
    int n = 0;
    int nb = 0;
    vector<int> pattern;
    vector<int> block_min_idx;

    vector<vector<int>> st; // ブロック最小値用 Sparse Table (index)
    vector<int> lg;

    inline int better(int x, int y) const {
        if (x == -1) return y;
        if (y == -1) return x;
        if (a[x] < a[y]) return x;
        if (a[x] > a[y]) return y;
        return (x < y) ? x : y;
    }

    void build(const vector<int>& v) {
        a = v;
        n = (int)a.size();
        if (n == 0) return;

        nb = (n + S - 1) / S;
        pattern.assign(nb, 0);
        block_min_idx.assign(nb, 0);

        // 各ブロックのパターンとブロック内最小値を計算
        for (int b = 0; b < nb; ++b) {
            int start = b * S;
            int end = std::min(start + S, n);

            // ブロック内最小値
            int min_pos = start;
            for (int i = start + 1; i < end; ++i) {
                if (a[i] < a[min_pos] || (a[i] == a[min_pos] && i < min_pos)) {
                    min_pos = i;
                }
            }
            block_min_idx[b] = min_pos;

            // パターン（±1 差分）
            int p = 0;
            for (int k = 0; k < S - 1; ++k) {
                int i = start + k;
                int bit = 1;
                if (i + 1 < n) {
                    int diff = a[i + 1] - a[i]; // ±1 のはず
                    bit = (diff > 0) ? 1 : 0;
                }
                if (bit) p |= (1 << k);
            }
            pattern[b] = p;
        }

        // ブロック最小値に対する Sparse Table 構築
        if (nb > 0) {
            int K = 1;
            while ((1 << K) <= nb) ++K;
            st.assign(K, vector<int>(nb));
            st[0] = block_min_idx;
            for (int k = 1; k < K; ++k) {
                int len = 1 << k;
                int half = len >> 1;
                for (int i = 0; i + len <= nb; ++i) {
                    int x = st[k - 1][i];
                    int y = st[k - 1][i + half];
                    st[k][i] = better(x, y);
                }
            }
            lg.assign(nb + 1, 0);
            for (int i = 2; i <= nb; ++i) lg[i] = lg[i >> 1] + 1;
        }
    }

    // ブロック b のローカルインデックス [i,j] の最小値位置 (a の index)
    int intra_block(int b, int i, int j) const {
        int start = b * S;
        int len = n - start;
        if (len <= 0) return -1;
        int max_local = std::min(S, len) - 1;
        if (i < 0) i = 0;
        if (j > max_local) j = max_local;
        if (i > j) return -1;
        int p = pattern[b];
        const auto& tbl = bt.table[p];
        uint8_t off = tbl[i * S + j];
        return start + i + (int)off;
    }

    // ブロック範囲 [lb, rb] の最小値位置 (a の index)
    int block_range_min(int lb, int rb) const {
        if (lb > rb) return -1;
        if (lb == rb) return block_min_idx[lb];
        int len = rb - lb + 1;
        int k = lg[len];
        int x = st[k][lb];
        int y = st[k][rb - (1 << k) + 1];
        return better(x, y);
    }

    // 区間 [l, r] の最小値位置 (a の index)
    int query_pos(int l, int r) const {
        if (l > r) std::swap(l, r);
        int bl = l / S;
        int br = r / S;
        if (bl == br) {
            return intra_block(bl, l % S, r % S);
        }

        int res = intra_block(bl, l % S, S - 1);
        int res2 = intra_block(br, 0, r % S);
        res = better(res, res2);

        if (bl + 1 <= br - 1) {
            int mid = block_range_min(bl + 1, br - 1);
            res = better(res, mid);
        }
        return res;
    }

    int query(int l, int r) const {
        int pos = query_pos(l, r);
        return (pos == -1 ? std::numeric_limits<int>::max() : a[pos]);
    }
};

BlockRMQTable RMQPlusMinusOne::bt;

//==============================================================
// 4. Cartesian Tree + Euler Tour + RMQPlusMinusOne による RMQ
//==============================================================

struct CartesianRMQ {
    vector<int> a;
    int n = 0;

    vector<int> left_child, right_child, parent;
    int root = -1;

    vector<int> euler;  // オイラーツアー上のノード index
    vector<int> depth;  // 各ステップでの深さ
    vector<int> first;  // 各ノードが最初に現れる位置

    RMQPlusMinusOne rmq_depth;

    void build_cartesian() {
        left_child.assign(n, -1);
        right_child.assign(n, -1);
        parent.assign(n, -1);
        vector<int> st;
        st.reserve(n);
        root = -1;

        for (int i = 0; i < n; ++i) {
            int last = -1;
            while (!st.empty() && a[st.back()] > a[i]) {
                last = st.back();
                st.pop_back();
            }
            if (!st.empty()) {
                right_child[st.back()] = i;
                parent[i] = st.back();
            } else {
                root = i;
            }
            if (last != -1) {
                left_child[i] = last;
                parent[last] = i;
            }
            st.push_back(i);
        }
    }

    void build_euler() {
        euler.clear();
        depth.clear();
        first.assign(n, -1);
        if (n == 0) return;

        euler.reserve(2 * n - 1);
        depth.reserve(2 * n - 1);

        struct Frame { int v; int state; int d; };
        vector<Frame> st;
        st.reserve(2 * n);
        st.push_back({root, 0, 0});

        while (!st.empty()) {
            Frame f = st.back();
            st.pop_back();
            int v = f.v;
            int state = f.state;
            int d = f.d;

            if (state == 0) {
                if (first[v] == -1) first[v] = (int)euler.size();
                euler.push_back(v);
                depth.push_back(d);

                if (right_child[v] != -1) {
                    st.push_back({v, 2, d});
                    st.push_back({right_child[v], 0, d + 1});
                }
                if (left_child[v] != -1) {
                    st.push_back({v, 1, d});
                    st.push_back({left_child[v], 0, d + 1});
                }
            } else if (state == 1) {
                euler.push_back(v);
                depth.push_back(d);
                if (right_child[v] != -1) {
                    st.push_back({v, 2, d});
                    st.push_back({right_child[v], 0, d + 1});
                }
            } else { // state == 2
                euler.push_back(v);
                depth.push_back(d);
            }
        }
    }

    void build(const vector<int>& v) {
        a = v;
        n = (int)a.size();
        if (n == 0) return;
        build_cartesian();
        build_euler();
        rmq_depth.build(depth);
    }

    int query(int l, int r) const {
        if (n == 0) return std::numeric_limits<int>::max();
        if (l > r) std::swap(l, r);
        int L = first[l];
        int R = first[r];
        if (L > R) std::swap(L, R);
        int pos = rmq_depth.query_pos(L, R);
        int idx = euler[pos];
        return a[idx];
    }
};

//==============================================================
// 正しさ検証（NaiveRMQ と比較）
//==============================================================

template <class RMQType>
bool verify_algo(
    const string& name,
    const vector<int>& a,
    int Q_verify,
    NaiveRMQ& naive,
    std::mt19937_64& rng
) {
    cout << "  [" << name << "] correctness check\n";
    int n = (int)a.size();
    RMQType ds;
    ds.build(a);

    for (int i = 0; i < Q_verify; ++i) {
        int l = (int)(rng() % n);
        int r = (int)(rng() % n);
        if (l > r) std::swap(l, r);
        int expected = naive.query(l, r);
        int got = ds.query(l, r);
        if (expected != got) {
            cout << "    NG at i=" << i
                 << " l=" << l << " r=" << r
                 << " expected=" << expected
                 << " got=" << got << "\n";
            return false;
        }
    }
    cout << "    OK (" << Q_verify << " random queries)\n";
    return true;
}

void run_correctness_tests() {
    std::mt19937_64 rng(987654321);
    // vector<int> Ns = {10, 1000, 100000};
    vector<int> Ns = {10, 1000};

    cout << "==== Correctness tests (with NaiveRMQ) ====\n";
    for (int N : Ns) {
        cout << "N = " << N << "\n";
        vector<int> a(N);
        for (int i = 0; i < N; ++i) {
            a[i] = (int)(rng() % 1000000007);
        }

        NaiveRMQ naive;
        naive.build(a);

        int Q_verify;
        if (N <= 100) Q_verify = 5000;
        else if (N <= 1000) Q_verify = 3000;
        else Q_verify = 500; // N=100000

        verify_algo<SegmentTreeRMQ>("SegmentTree (ACL)", a, Q_verify, naive, rng);
        verify_algo<SparseTableRMQ>("SparseTable", a, Q_verify, naive, rng);
        verify_algo<CartesianRMQ>("Cartesian+Euler+Block+Table", a, Q_verify, naive, rng);

        cout << "\n";
    }
}

//==============================================================
// ベンチマーク（ナイーブなし）
//==============================================================

int choose_build_reps(int n) {
    if (n <= 100) return 2000;
    if (n <= 1000) return 500;
    if (n <= 10000) return 200;
    if (n <= 100000) return 50;
    return 5; // n >= 1e6
}

template <class RMQType>
void benchmark_algo(
    const string& name,
    const vector<int>& a,
    const vector<pair<int,int>>& queries,
    int Q_total
) {
    using namespace std::chrono;
    cout << "  [" << name << "]\n";

    int n = (int)a.size();

    // ビルド時間
    {
        int reps = choose_build_reps(n);
        RMQType ds;
        auto t0 = steady_clock::now();
        for (int i = 0; i < reps; ++i) {
            ds.build(a);
        }
        auto t1 = steady_clock::now();
        double total_ms = duration<double, std::milli>(t1 - t0).count();
        double avg_ms = total_ms / reps;
        cout << "    build: reps=" << reps
             << ", total=" << total_ms << " ms"
             << ", avg=" << avg_ms << " ms\n";
    }

    // クエリ時間
    {
        RMQType ds;
        ds.build(a);
        using namespace std::chrono;
        auto t0 = steady_clock::now();
        long long checksum = 0;
        for (int i = 0; i < Q_total; ++i) {
            int l = queries[i].first;
            int r = queries[i].second;
            int val = ds.query(l, r);
            checksum += val;
        }
        auto t1 = steady_clock::now();
        double total_ms = duration<double, std::milli>(t1 - t0).count();
        double avg_ns = (total_ms * 1e6) / Q_total; // ns/query

        cout << "    query: Q=" << Q_total
             << ", total=" << total_ms << " ms"
             << ", avg=" << avg_ns << " ns/query"
             << ", checksum=" << checksum << "\n";
    }
}

//==============================================================
// main
//==============================================================

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // まず小さい N で NaiveRMQ を使った正しさ検証
    run_correctness_tests();

    // その後、大きな N も含めたベンチマーク
    std::mt19937_64 rng(123456789);

    // vector<int> Ns = {10, 100, 1000, 10000, 100000, 1000000};
    vector<int> Ns = {10, 100, 1000, 10000, 100000};
    const int Q_TOTAL = 1000000;

    cout << "==== Benchmark tests (without NaiveRMQ) ====\n";
    for (int N : Ns) {
        cout << "N = " << N << "\n";

        vector<int> a(N);
        for (int i = 0; i < N; ++i) {
            a[i] = (int)(rng() % 1000000007);
        }

        vector<pair<int,int>> queries;
        queries.reserve(Q_TOTAL);
        for (int i = 0; i < Q_TOTAL; ++i) {
            int l = (int)(rng() % N);
            int r = (int)(rng() % N);
            if (l > r) std::swap(l, r);
            queries.emplace_back(l, r);
        }

        benchmark_algo<SegmentTreeRMQ>("SegmentTree (ACL)", a, queries, Q_TOTAL);
        benchmark_algo<SparseTableRMQ>("SparseTable", a, queries, Q_TOTAL);
        benchmark_algo<CartesianRMQ>("Cartesian+Euler+Block+Table", a, queries, Q_TOTAL);

        cout << "\n";
    }

    return 0;
}

// 実行結果(atcoder)
// CartesianRMQは、遅すぎて使えず。。

// ==== Correctness tests (with NaiveRMQ) ====
// N = 10
//   [SegmentTree (ACL)] correctness check
//     OK (5000 random queries)
//   [SparseTable] correctness check
//     OK (5000 random queries)
//   [Cartesian+Euler+Block+Table] correctness check
//     OK (5000 random queries)

// N = 1000
//   [SegmentTree (ACL)] correctness check
//     OK (3000 random queries)
//   [SparseTable] correctness check
//     OK (3000 random queries)
//   [Cartesian+Euler+Block+Table] correctness check
//     OK (3000 random queries)

// ==== Benchmark tests (without NaiveRMQ) ====
// N = 10
//   [SegmentTree (ACL)]
//     build: reps=2000, total=0.040178 ms, avg=2.0089e-05 ms
//     query: Q=1000000, total=16.7197 ms, avg=16.7197 ns/query, checksum=221886474952933
//   [SparseTable]
//     build: reps=2000, total=0.091826 ms, avg=4.5913e-05 ms
//     query: Q=1000000, total=1.4328 ms, avg=1.4328 ns/query, checksum=221886474952933

// N = 100
//   [SegmentTree (ACL)]
//     build: reps=2000, total=0.160309 ms, avg=8.01545e-05 ms
//     query: Q=1000000, total=33.7541 ms, avg=33.7541 ns/query, checksum=35704511303318
//   [SparseTable]
//     build: reps=2000, total=0.507829 ms, avg=0.000253915 ms
//     query: Q=1000000, total=1.34454 ms, avg=1.34454 ns/query, checksum=35704511303318

// N = 1000
//   [SegmentTree (ACL)]
//     build: reps=500, total=0.357022 ms, avg=0.000714044 ms
//     query: Q=1000000, total=53.8697 ms, avg=53.8697 ns/query, checksum=14192494425618
//   [SparseTable]
//     build: reps=500, total=1.8367 ms, avg=0.0036734 ms
//     query: Q=1000000, total=1.38929 ms, avg=1.38929 ns/query, checksum=14192494425618

// N = 10000
//   [SegmentTree (ACL)]
//     build: reps=200, total=2.06037 ms, avg=0.0103018 ms
//     query: Q=1000000, total=73.6328 ms, avg=73.6328 ns/query, checksum=1576403324115
//   [SparseTable]
//     build: reps=200, total=11.1417 ms, avg=0.0557087 ms
//     query: Q=1000000, total=1.90583 ms, avg=1.90583 ns/query, checksum=1576403324115

// N = 100000
//   [SegmentTree (ACL)]
//     build: reps=50, total=5.57057 ms, avg=0.111411 ms
//     query: Q=1000000, total=93.3739 ms, avg=93.3739 ns/query, checksum=176742206457
//   [SparseTable]
//     build: reps=50, total=42.6555 ms, avg=0.853109 ms
//     query: Q=1000000, total=4.90114 ms, avg=4.90114 ns/query, checksum=176742206457

// N = 1000000
//   [SegmentTree (ACL)]
//     build: reps=5, total=13.2521 ms, avg=2.65042 ms
//     query: Q=1000000, total=114.432 ms, avg=114.432 ns/query, checksum=25210134370
//   [SparseTable]
//     build: reps=5, total=87.0141 ms, avg=17.4028 ms
//     query: Q=1000000, total=11.9064 ms, avg=11.9064 ns/query, checksum=25210134370

