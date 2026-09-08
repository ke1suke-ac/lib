#include "link_cut_tree.hpp"  // 事前に用意した link_cut_tree.hpp をインクルード

// 加算モノイドの関数
int add(int a, int b) {
    return a + b;
}

// 反転（逆順）用の恒等関数（名前を id_func とする）
int id_func(int a) {
    return a;
}

// テンプレート引数として、関数ポインタ add と id_func を指定する
using LCT = LinkCutTree<int, add, id_func>;

int main(){
    const int N = 1000;         // 頂点数
    const int ITER = 1000000;   // 各操作を 1e6 回実行

    LCT lct;

    // --- 連結状態のチェーンを構築 ---
    // chain_nodes[0..N-1] を連結してチェーンを作成（全体が1つの木となる）
    vector<typename LCT::Node*> chain_nodes(N);
    for (int i = 0; i < N; i++){
        chain_nodes[i] = new typename LCT::Node(i);  // key として i を設定
    }
    // 0～N-1 のチェーン：i=1～N-1 を chain_nodes[i-1] に link
    for (int i = 1; i < N; i++){
        lct.link(chain_nodes[i], chain_nodes[i-1]);
    }

    // ランダム生成器の準備
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> edge_dist(1, N - 1); // 切断・linkする際のエッジ番号（1～N-1）
    uniform_int_distribution<int> node_dist(0, N - 1);   // 連結チェック、fold の対象ノード

    //-------------------------------------------------------------------------
    // 1. Link ベンチマーク
    // 各イテレーションで、ランダムなエッジを一旦切断してから、そのエッジを link して木を復元する。
    // ここでは、link の操作のみの実行時間を計測します。
    auto start_link = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++){
        int j = edge_dist(rng);  // 1 <= j < N
        // 事前に切断しておく（この cut は計測対象外）
        lct.cut(chain_nodes[j], chain_nodes[j - 1]);
        // 計測対象：切断済み状態で、再び連結する link 操作
        lct.link(chain_nodes[j], chain_nodes[j - 1]);
    }
    auto end_link = chrono::high_resolution_clock::now();
    auto duration_link = chrono::duration_cast<chrono::milliseconds>(end_link - start_link).count();
    cout << "Link benchmark (1e6 calls): " << duration_link << " ms" << endl;

    //-------------------------------------------------------------------------
    // 2. Cut ベンチマーク
    // 各イテレーションで、ランダムなエッジを切断する操作（cut）を計測し、
    // その後 link で連結状態を復元して次回に備える。
    auto start_cut = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++){
        int j = edge_dist(rng);
        // 計測対象：切断操作
        lct.cut(chain_nodes[j], chain_nodes[j - 1]);
        // 連結状態を復元
        lct.link(chain_nodes[j], chain_nodes[j - 1]);
    }
    auto end_cut = chrono::high_resolution_clock::now();
    auto duration_cut = chrono::duration_cast<chrono::milliseconds>(end_cut - start_cut).count();
    cout << "Cut benchmark (1e6 calls): " << duration_cut << " ms" << endl;

    //-------------------------------------------------------------------------
    // 3. Connectivity ベンチマーク
    // ランダムに選んだ2頂点について、get_root() による連結チェックを実施
    int connCount = 0;
    auto start_conn = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++){
        int i1 = node_dist(rng);
        int i2 = node_dist(rng);
        bool connected = (lct.get_root(chain_nodes[i1]) == lct.get_root(chain_nodes[i2]));
        connCount += connected ? 1 : 0;
    }
    auto end_conn = chrono::high_resolution_clock::now();
    auto duration_conn = chrono::duration_cast<chrono::milliseconds>(end_conn - start_conn).count();
    cout << "Connectivity benchmark (1e6 calls): " << duration_conn << " ms" << endl;
    cout << "Connected count: " << connCount << endl;

    //-------------------------------------------------------------------------
    // 4. Fold（パスクエリ）ベンチマーク
    // ランダムに2頂点を選び、昇順に並べた上で fold() を実行（パス上の集約値を計算）
    ll dummySum = 0;
    auto start_fold = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITER; i++){
        int a = node_dist(rng);
        int b = node_dist(rng);
        if(a > b) swap(a, b);
        dummySum += lct.fold(chain_nodes[a], chain_nodes[b]);
    }
    auto end_fold = chrono::high_resolution_clock::now();
    auto duration_fold = chrono::duration_cast<chrono::milliseconds>(end_fold - start_fold).count();
    cout << "Fold benchmark (1e6 calls): " << duration_fold << " ms" << endl;
    cout << "Dummy sum (to avoid optimization): " << dummySum << endl;

    //-------------------------------------------------------------------------
    // メモリ解放
    for(auto p : chain_nodes)
        delete p;
    
    return 0;
}

// 実行結果
// Link benchmark (1e6 calls): 492 ms
// Cut benchmark (1e6 calls): 485 ms
// Connectivity benchmark (1e6 calls): 424 ms
// Connected count: 1000000
// Fold benchmark (1e6 calls): 532 ms
// Dummy sum (to avoid optimization): 166933173436
