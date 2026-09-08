/*****************************************************************************
 * QuadTree テストスイート & ベンチマーク                               *
 * --------------------------------------------------------------------------*
 * ■ 全体的な利用方法                                                       *
 *   - 「quad_tree.hpp」で定義した QuadTree クラスの挙動を確認するテスト。 *
 *   - 基本操作（insert / query / remove）の正当性と性能をまとめて検証。  *
 *                                                                          *
 * ■ 構成                                                                    *
 *   1) 単体テスト  : test_simple, test_overlap, test_boundary,              *
 *                    test_dynamic, test_random                              *
 *   2) 性能測定    : benchmark                                              *
 *                                                                          *
 * ■ 実行方法                                                                *
 *      g++ -std=c++20 -O2 test_quad_tree.cpp -o test && ./test              *
 *                                                                          *
 * ■ 依存                                                                    *
 *   - g++ 12.2 以上                                                         *
 *   - quad_tree.hpp（同一ディレクトリ）                                    *
 ****************************************************************************/

#include <bits/stdc++.h>
#include "quad_tree.hpp"

using namespace std;

/*===========================================================================
 *  1. ベンチマーク関数
 *===========================================================================*/
int benchmark() {
    using Clock    = chrono::high_resolution_clock;
    using Duration = chrono::duration<double, micro>;

    // --------------------------------------------------------------------
    // [問題設定・ユースケース]
    //   ランダム生成した 1000 個の矩形を何度も挿入 / クエリ / 削除し、
    //   操作 1 回あたりの平均時間を測定して QuadTree のスループットを確認。
    //
    // [QuadTree 使用方法・引数]
    //   QuadTree qt(minX, minY, maxX, maxY [, capacity])
    //   - insert(id, x1, y1, x2, y2)  : 矩形を登録
    //   - query(qx1, qy1, qx2, qy2)   : 範囲検索
    //   - remove(id)                  : 登録済み矩形を削除
    //
    // [再利用時のポイント・注意点]
    //   - ベンチマーク対象の領域は十分に大きく確保する。
    //   - 小規模データでは capacity を小さめにするとツリー階層が浅くなる。
    // --------------------------------------------------------------------
    const int N         = 1000;          // 矩形数
    const int spaceSize = 20000;         // ルート領域の片辺
    vector<Rect> rects; rects.reserve(N);

    // --- 問題設定 (矩形データ生成) --------------------------------------
    for (int i = 0; i < N; i++) {
        int x = i * 20, y = i * 20;
        rects.push_back({ i, x, y, x + 19, y + 19 });
    }

    /*--------------------*
     * 1) 挿入ベンチマーク *
     *--------------------*/
    const int iters_insert = 100;
    Duration   d_insert{ 0 };
    for (int it = 0; it < iters_insert; ++it) {
        auto t0 = Clock::now();
        QuadTree qt(0, 0, spaceSize, spaceSize);     // --- QuadTree 準備
        for (auto &r : rects)
            qt.insert(r.id, r.x1, r.y1, r.x2, r.y2); // --- QuadTree 挿入
        d_insert += Clock::now() - t0;
    }
    double total_insert_ms = d_insert.count() / 1000.0;
    double avg_insert_ms   = total_insert_ms / iters_insert;

    // --- 結果表示 --------------------------------------------------------
    cout << "Insert 1000 rects x" << iters_insert << ": total "
         << total_insert_ms << " ms, avg " << avg_insert_ms << " ms/run\n";

    /*---------------------*
     * 2) クエリベンチマーク *
     *---------------------*/
    QuadTree qt(0, 0, spaceSize, spaceSize);         // --- QuadTree 準備
    for (auto &r : rects)
        qt.insert(r.id, r.x1, r.y1, r.x2, r.y2);     // --- 全矩形を登録

    vector<int> overlapCounts = { 1, 10, 100, 1000 };   // 想定ヒット数
    vector<int> iters_query   = { 1000000, 200000, 20000, 2000 };
    for (size_t i = 0; i < overlapCounts.size(); ++i) {
        int k       = overlapCounts[i];
        int iters   = iters_query[i];
        int maxCoord = (k - 1) * 20 + 19;               // クエリ矩形サイズ調整

        auto t0 = Clock::now();
        for (int it = 0; it < iters; ++it)
            auto res = qt.query(0, 0, maxCoord, maxCoord); // --- QuadTree クエリ
        auto d = Clock::now() - t0;

        double total_query_ms = chrono::duration_cast<Duration>(d).count() / 1000.0;
        double avg_query_us   = chrono::duration_cast<Duration>(d).count() / static_cast<double>(iters);

        // --- 結果表示 ----------------------------------------------------
        cout << "Query overlap " << k << " rects x" << iters << ": total "
             << total_query_ms << " ms, avg " << avg_query_us << " μs/query\n";
    }

    /*------------------------*
     * 3) 削除＋再挿入ベンチマーク *
     *------------------------*/
    const int iters_update = 100000;
    Duration   d_update{ 0 };
    for (int it = 0; it < iters_update; ++it) {
        auto t0 = Clock::now();
        qt.remove(0);                                      // --- QuadTree 削除
        qt.insert(0, rects[0].x1, rects[0].y1,
                     rects[0].x2, rects[0].y2);            // --- QuadTree 再挿入
        d_update += Clock::now() - t0;
    }
    double total_update_ms = d_update.count() / 1000.0;
    double avg_update_us   = d_update.count() / static_cast<double>(iters_update);

    // --- 結果表示 --------------------------------------------------------
    cout << "Remove+Insert id0 x" << iters_update << ": total "
         << total_update_ms << " ms, avg " << avg_update_us << " μs/op\n";

    return 0;
}

/*===========================================================================
 *  2. 単体テスト各種
 *===========================================================================*/

/*---------------------------------------------------------------------------
 * test_simple
 *   - 基本操作（insert / query / remove）の組み合わせを最小構成で検証。
 *---------------------------------------------------------------------------*/
void test_simple() {
    // --- 問題設定 --------------------------------------------------------
    QuadTree qt(0, 0, 100, 100);     // 100×100 のルート領域
    qt.insert(1, 10, 10, 20, 20);
    qt.insert(2, 30, 30, 40, 40);

    // --- 問題解決（クエリ）----------------------------------------------
    auto v = qt.query(0, 0, 25, 25); // (0,0)-(25,25) と交差する矩形検索
    sort(v.begin(), v.end());

    // --- 結果確認 --------------------------------------------------------
    assert(v == vector<int>({ 1 }));
    assert(qt.size() == 2);
    bool ok = qt.remove(1);          // id=1 を削除
    assert(ok);
    assert(qt.size() == 1);
    v = qt.query(0, 0, 25, 25);      // 再度クエリ → 空になるはず
    assert(v.empty());
}

/*---------------------------------------------------------------------------
 * test_overlap
 *   - 重なり合う矩形を複数登録し、クエリ矩形と交差する ID 一覧を検証。
 *---------------------------------------------------------------------------*/
void test_overlap() {
    // --- 問題設定 --------------------------------------------------------
    QuadTree qt(0, 0, 100, 100);
    qt.insert(1, 0, 0, 50, 50);
    qt.insert(2, 25, 25, 75, 75);
    qt.insert(3, 60, 60, 90, 90);

    // --- 問題解決（クエリ）----------------------------------------------
    auto v = qt.query(20, 20, 80, 80);
    sort(v.begin(), v.end());

    // --- 結果確認 --------------------------------------------------------
    assert(v == vector<int>({ 1, 2, 3 }));
}

/*---------------------------------------------------------------------------
 * test_boundary
 *   - 境界上にある矩形が正しくヒットするかを検証。
 *---------------------------------------------------------------------------*/
void test_boundary() {
    // --- 問題設定 --------------------------------------------------------
    QuadTree qt(0, 0, 100, 100);
    qt.insert(1, 0,   0,   0,   0);      // 左上隅に点
    qt.insert(2, 100, 100, 100, 100);    // 右下隅に点

    // --- 問題解決＆結果確認 --------------------------------------------
    auto v1 = qt.query(0, 0, 0, 0);
    assert(v1 == vector<int>({ 1 }));
    auto v2 = qt.query(100, 100, 100, 100);
    assert(v2 == vector<int>({ 2 }));
}

/*---------------------------------------------------------------------------
 * test_dynamic
 *   - capacity=2 として細かく分割し、挿入／削除を行った後の状態を検証。
 *---------------------------------------------------------------------------*/
void test_dynamic() {
    // --- 問題設定 --------------------------------------------------------
    QuadTree qt(0, 0, 16, 16, 2);  // capacity を 2 に設定
    for (int i = 1; i <= 10; i++)
        qt.insert(i, i, i, i + 1, i + 1);  // 10 矩形挿入

    // --- 問題解決（削除）------------------------------------------------
    for (int i = 1; i <= 5; i++) qt.remove(i);

    // --- 結果確認 --------------------------------------------------------
    auto v = qt.query(6, 6, 12, 12);
    sort(v.begin(), v.end());
    assert(v == vector<int>({ 6, 7, 8, 9, 10 }));
}

/*---------------------------------------------------------------------------
 * test_random
 *   - 1000 個の乱数矩形を登録し、100 クエリでブルートフォース結果と比較。
 *---------------------------------------------------------------------------*/
void test_random() {
    // --- 問題設定 --------------------------------------------------------
    const int N = 1000;
    QuadTree qt(0, 0, 1000, 1000);
    vector<Rect> rects;
    mt19937_64 rng(123);
    uniform_int_distribution<int> dist(0, 900);

    for (int i = 1; i <= N; i++) {
        int x = dist(rng), y = dist(rng);
        int w = dist(rng) % 100, h = dist(rng) % 100;
        qt.insert(i, x, y, x + w, y + h);
        rects.push_back({ i, x, y, x + w, y + h });
    }

    // --- 問題解決（クエリ比較）------------------------------------------
    for (int qi = 0; qi < 100; qi++) {
        int x = dist(rng), y = dist(rng);
        int w = dist(rng) % 100, h = dist(rng) % 100;
        auto ans1 = qt.query(x, y, x + w, y + h);

        // ブルートフォース解
        vector<int> ans2;
        for (auto &r : rects)
            if (!(r.x2 < x || r.x1 > x + w || r.y2 < y || r.y1 > y + h))
                ans2.push_back(r.id);

        sort(ans1.begin(), ans1.end());
        sort(ans2.begin(), ans2.end());
        assert(ans1 == ans2);
    }
}

/*===========================================================================
 *  3. メイン関数
 *===========================================================================*/
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ---------------- 単体テスト実行 ----------------
    test_simple();
    test_overlap();
    test_boundary();
    test_dynamic();
    test_random();
    cout << "All tests passed.\n";

    // ---------------- ベンチマーク実行 --------------
    benchmark();

    return 0;
}
