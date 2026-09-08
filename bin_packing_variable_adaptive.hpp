/******************************************************
 *  Best-Fit Decreasing (BFD) for Variable-Sized Bins  *
 *  ------------------------------------------------  *
 *  単体ビルド可能・依存：C++20 標準ライブラリのみ         *
 *  g++-12.2  (-std=c++20 -O2 -pipe -static -s)        *
 *****************************************************/

#include <algorithm>   // sort, iota
#include <iostream>    // cin, cout
#include <numeric>     // iota
#include <set>         // set
#include <utility>     // pair
#include <vector>      // vector

using namespace std;

template <typename T>
bool ffd(const vector<T>& bin_capacities,
               const vector<T>& items,
               vector<int>&      assign)
{
    const size_t n_bin   = bin_capacities.size();
    const size_t n_item  = items.size();
    assign.assign(n_item, -1);                     // -1 で初期化

    // (1) アイテムを「サイズ降順」で並べ替える（元のインデックス保持）
    vector<size_t> idx(n_item);
    iota(idx.begin(), idx.end(), 0);
    stable_sort(idx.begin(), idx.end(),
        [&items](size_t a, size_t b) { return items[a] > items[b]; });

    // (2) 各 bin の残容量を作業用バッファにコピー
    vector<T> rem = bin_capacities;

    // (3) 降順アイテムを 1 つずつ最初に入る bin に詰めていく
    for (size_t id : idx)
    {
        const T w = items[id];
        bool placed = false;
        // **メモリキャッシュ効率を重視** ― 連続領域を単純ループで走査
        for (size_t b = 0; b < n_bin; ++b)
        {
            if (rem[b] >= w)
            {
                rem[b]   -= w;
                assign[id] = static_cast<int>(b);
                placed     = true;
                break;                  // First-Fit
            }
        }
        if (!placed) return false;      // 1 つ入らなければ即失敗
    }
    return true;
}

template <typename T>
bool bfd(const vector<T>& bin_capacities,
               const vector<T>& items,
               vector<int>&     assign) {
    static_assert(is_arithmetic_v<T>,
                  "T must be an arithmetic type.");

    const int n = static_cast<int>(items.size());
    const int m = static_cast<int>(bin_capacities.size());
    assign.assign(n, -1);                     // -1 で初期化

    /* 残容量テーブル。vector で連続メモリを確保して
       キャッシュ効率を高める */
    vector<T> rem = bin_capacities;

    /* (残容量, bin_index) を残容量昇順で管理する平衡二分木
       - lower_bound で「入る中で最も残容量が小さい bin」を
       O(log M) で取得できる */
    set<pair<T, int>> pool;
    for (int i = 0; i < m; ++i) pool.emplace(rem[i], i);

    /* アイテムを大きい順に処理 (Best-Fit *Decreasing*) */
    vector<int> order(n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(),
         [&](int a, int b) { return items[a] > items[b]; });

    for (int idx : order) {
        const T w = items[idx];
        auto it   = pool.lower_bound({w, -1});    // 入る最小 bin
        if (it == pool.end()) return false;       // どこにも入らない

        const int b = it->second;                 // bin 番号
        pool.erase(it);                           // 一旦削除
        rem[b] -= w;                              // 残容量更新
        assign[idx] = b;                          // 割当を記録
        pool.emplace(rem[b], b);                  // 再登録
    }
    return true;
}

/*----------------------------------------------------
 *  partition()
 *----------------------------------------------------
 *  @brief  FFD / BFD でアイテムを詰める
 *
 *  @tparam T   整数または浮動小数 (容量・サイズ型)
 *  @param  bin_capacities  各 bin の初期容量
 *  @param  items           アイテムサイズ
 *  @param  assign          出力: items[i] を入れた bin 番号
 *  @return 全アイテムを詰められたら true, 失敗なら false
 *
 *  計算量 :  O(N log M)  (N = items 数, M = bin 数)
 *  メモリ :  O(N + M)
 *---------------------------------------------------*/
template<class T>
bool partition(const vector<T>& cap,
                const vector<T>& items,
                vector<int>&     asg)
{
    auto n = cap.size();
    if (n <= 1024)  return ffd(cap, items, asg);
    return bfd(cap, items, asg);
}
