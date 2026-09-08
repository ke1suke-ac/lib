#include "link_cut_tree_dp.hpp"

/*======================================================================
  パス DP：整数値の和
======================================================================*/
struct IntSumInfo {
    struct Path {
        long long sum;
    };
    struct Info {
        long long val;
    };
    static Path vertex(const Info& v) { return {v.val}; }
    static Path compress(const Path& a, const Path& b) {
        return {a.sum + b.sum};
    }
};

/*======================================================================
  テスト
======================================================================*/
#define ASSERT_TRUE(cond)                                              \
    do {                                                               \
        if (!(cond)) {                                                 \
            cerr << "Assertion failed: " << #cond << " @ " << __LINE__ \
                 << endl;                                              \
            exit(1);                                                   \
        }                                                              \
    } while (0)

void test_alloc_build() {
    LinkCutTree<IntSumInfo> lct;
    auto n = lct.alloc({42});
    ASSERT_TRUE(n->info.val == 42);

    vector<IntSumInfo::Info> v = {{1}, {2}, {3}};
    auto nodes = lct.build(v);
    ASSERT_TRUE(nodes.size() == 3 && nodes[1]->info.val == 2);
}

void test_link_cut_conn() {
    LinkCutTree<IntSumInfo> lct;
    auto a = lct.alloc({1}), b = lct.alloc({2}), c = lct.alloc({3});
    lct.link(b, a);
    lct.link(c, b);
    ASSERT_TRUE(lct.is_connected(a, c));
    lct.cut(c);
    ASSERT_TRUE(!lct.is_connected(a, c));
}

void test_evert_query() {
    LinkCutTree<IntSumInfo> lct;
    auto r = lct.alloc({5}), x = lct.alloc({4}), y = lct.alloc({3});
    lct.link(x, r);
    lct.link(y, x);
    ASSERT_TRUE(lct.query_path(r, y).sum == 12);
    lct.evert(y);
    ASSERT_TRUE(lct.query_path(y, r).sum == 12);
    lct.set_key(x, {10});
    ASSERT_TRUE(lct.query_path(y, r).sum == 18);
}

void test_lca() {
    LinkCutTree<IntSumInfo> lct;
    auto a = lct.alloc({1}), b = lct.alloc({2}), c = lct.alloc({3}),
         d = lct.alloc({4});
    lct.link(b, a);
    lct.link(c, a);
    lct.link(d, c);
    ASSERT_TRUE(lct.lca(b, d) == a);
    ASSERT_TRUE(lct.lca(c, d) == c);
}

void test_find_first() {
    LinkCutTree<IntSumInfo> lct;
    vector<IntSumInfo::Info> vals = {{1}, {2}, {3}, {4}, {5}};
    auto v = lct.build(vals);
    auto r = v[0], a = v[1], b = v[2], c = v[3], d = v[4];
    lct.link(a, r);
    lct.link(b, a);
    lct.link(c, b);
    lct.link(d, c);
    long long lim = 6;
    auto res = lct.find_first(d, [&](auto p) { return p.sum >= lim; });
    /* 仕様：葉→根方向で初めに累積>=6 となるのは c(4) */
    ASSERT_TRUE(res.first == c && res.second.sum == 9);
}

void test_methods() {
    test_alloc_build();
    test_link_cut_conn();
    test_evert_query();
    test_lca();
    test_find_first();

    cout << "test_methods: All tests passed.\n";
}

// DP用の情報クラス（加算モノイド）
struct TreeDPInfo {
    using Path = int;
    using Info = int;
    // 頂点の値を Path に変換
    static Path vertex(const Info& u) { return u; }
    // 2つの Path の圧縮（加算）
    static Path compress(const Path& p, const Path& c) { return p + c; }
};

// この実装では、テンプレート引数に TreeDPInfo を渡す
using LCT = LinkCutTree<TreeDPInfo>;

int main() {
    test_methods();

    const int N = 1000;        // 頂点数
    const int ITER = 1000000;  // 各操作を1e6回実行

    LCT lct;

    // --- 連結状態のチェーンを構築 ---
    // chain_nodes[0..N-1] を生成し、i=1～N-1 で chain_nodes[i] を
    // chain_nodes[i-1] に link する
    vector<LCT::NP> chain_nodes(N);
    for (int i = 0; i < N; i++) {
        // 各頂点の初期値として i を設定
        chain_nodes[i] = lct.alloc(i);
    }
    for (int i = 1; i < N; i++) {
        lct.link(chain_nodes[i], chain_nodes[i - 1]);
    }

    // ランダムジェネレータの準備
    random_device rd;
    mt19937 rng(rd());
    // エッジ操作用：1～N-1（edge j で chain_nodes[j] と chain_nodes[j-1]
    // を結ぶエッジ）
    uniform_int_distribution<int> edge_dist(1, N - 1);
    // 頂点操作用：0～N-1
    uniform_int_distribution<int> node_dist(0, N - 1);

    //-------------------------------------------------------------------------
    // 1. Link ベンチマーク
    // 各イテレーションで、ランダムなエッジ j を一旦 cut してから、そのエッジを
    // link で再連結
    auto start_link = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++) {
        int j = edge_dist(rng);  // 1 <= j < N
        // 事前に切断（cut操作はここでは計測対象外）
        lct.cut(chain_nodes[j]);
        // 計測対象：cut 後に、元の状態に戻すための link 操作
        lct.link(chain_nodes[j], chain_nodes[j - 1]);
    }
    auto end_link = chrono::high_resolution_clock::now();
    auto duration_link =
        chrono::duration_cast<chrono::milliseconds>(end_link - start_link)
            .count();
    cout << "Link benchmark (1e6 calls): " << duration_link << " ms" << endl;

    //-------------------------------------------------------------------------
    // 2. Cut ベンチマーク
    // 各イテレーションで、ランダムなエッジ j に対して cut 操作を計測し、
    // その後 link で連結状態を復元する
    auto start_cut = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++) {
        int j = edge_dist(rng);
        // 計測対象：cut 操作
        lct.cut(chain_nodes[j]);
        // 連結状態を復元
        lct.link(chain_nodes[j], chain_nodes[j - 1]);
    }
    auto end_cut = chrono::high_resolution_clock::now();
    auto duration_cut =
        chrono::duration_cast<chrono::milliseconds>(end_cut - start_cut)
            .count();
    cout << "Cut benchmark (1e6 calls): " << duration_cut << " ms" << endl;

    //-------------------------------------------------------------------------
    // 3. Connectivity ベンチマーク
    // ランダムに選んだ2頂点の連結性を is_connected() によりチェック
    int connCount = 0;
    auto start_conn = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++) {
        int i1 = node_dist(rng);
        int i2 = node_dist(rng);
        bool connected = lct.is_connected(chain_nodes[i1], chain_nodes[i2]);
        connCount += connected ? 1 : 0;
    }
    auto end_conn = chrono::high_resolution_clock::now();
    auto duration_conn =
        chrono::duration_cast<chrono::milliseconds>(end_conn - start_conn)
            .count();
    cout << "Connectivity benchmark (1e6 calls): " << duration_conn << " ms"
         << endl;
    cout << "Connected count: " << connCount << endl;

    //-------------------------------------------------------------------------
    // 4. Fold (Path Query) ベンチマーク
    // ランダムに選んだ2頂点に対して、query_path(u, v)
    // によりパス上の集約値を求める query_path(u, v) は内部で evert(u)
    // を行い、u～v のパスの集約値を返す
    ll dummySum = 0;
    auto start_fold = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++) {
        int a = node_dist(rng);
        int b = node_dist(rng);
        // 2頂点間のパス上の和（加算モノイド）を求める
        dummySum += lct.query_path(chain_nodes[a], chain_nodes[b]);
    }
    auto end_fold = chrono::high_resolution_clock::now();
    auto duration_fold =
        chrono::duration_cast<chrono::milliseconds>(end_fold - start_fold)
            .count();
    cout << "Fold benchmark (1e6 calls): " << duration_fold << " ms" << endl;
    cout << "Dummy sum (to avoid optimization): " << dummySum << endl;

    //-------------------------------------------------------------------------
    // メモリ解放
    for (auto node : chain_nodes) delete node;

    return 0;
}

// 実行結果
// Link benchmark (1e6 calls): 270 ms
// Cut benchmark (1e6 calls): 269 ms
// Connectivity benchmark (1e6 calls): 327 ms
// Connected count: 1000000
// Fold benchmark (1e6 calls): 445 ms
// Dummy sum (to avoid optimization): 167193314011
