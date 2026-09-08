// 常に速い
// 小規模 & 高精度要求する場合のみ、ffd + 二分探索 を考える

/**********************************************************************
 *  Largest Differencing（Karmarkar–Karp, KK）k-way 拡張
 *  ---------------------------------------------------------------
 *  items を k 個の bin に分割し、最大 bin 和（≒必要な bin 容量）を
 *  可能な限り小さくすることを目指すヒューリスティック。
 *
 *  アルゴリズム概要
 *  ----------------
 *  1. items を降順ソート            … もっとも重いものから処理
 *  2. 「現在もっとも軽い bin」に順に入れる … greedy differencing
 *     - min-heap で常に O(log k) で取得
 *  3. すべて入れ終わった時点で max(bin_sum) が必要容量
 *
 *  計算量
 *  ------
 *  ソート O(n log n) + 挿入 n × O(log k) = O(n log n)  (通常 k ≪ n)
 *
 *********************************************************************/

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <vector>

/*---------------------------------------------------------------
 *  multifit
 *-------------------------------------------------------------*/
/**
 * @brief Largest Differencing (KK) k-way 拡張による bin packing
 *
 * @tparam T          アイテムサイズと bin 容量の型（整数／浮動小数点可）
 * @param  K          目標とする bin 数 (>=1)
 * @param  items      アイテムサイズ列
 * @param  assign     [out] 各アイテムが入る bin インデックス (0-indexed)
 * @param  presort    true: 降順ソートして精度向上 (デフォルト推奨)
 * @return T          得られた最小 bin 容量 (= 最大 bin 和)
 */
template <typename T>
T multifit(const int                       K,
           const std::vector<T>&           items,
           std::vector<int>&               assign,
           const bool                      presort = true)
{
    static_assert(std::is_arithmetic_v<T>,
                  "T must be an arithmetic (integer or floating-point) type.");
    assert(K >= 1 && "K (number of bins) must be >= 1");

    const int n = static_cast<int>(items.size());
    assign.assign(n, -1);                 // bin 割り当て結果
    if (n == 0) return T(0);              // 空入力は容量 0

    /*-- k 個超の空 bin を作らないよう min(K, n) にする --*/
    const int bins = std::min(K, n);

    /*-------------------------------------------------------
     *  0. 各 bin の現在和を保持する min-heap を作成
     *     (和, bin_id) で比較
     *-----------------------------------------------------*/
    std::vector<T> bin_sum(bins, T(0));   // bin ごとの和
    auto cmp = [](const std::pair<T, int>& a,
                  const std::pair<T, int>& b) {
        return a.first > b.first;         // 小さい和が優先（min-heap）
    };
    std::priority_queue<std::pair<T, int>,
                        std::vector<std::pair<T, int>>,
                        decltype(cmp)>
        pq(cmp);

    for (int i = 0; i < bins; ++i) pq.emplace(T(0), i);

    /*-------------------------------------------------------
     *  1. items を重い順にアクセスするため index 配列を作成
     *-----------------------------------------------------*/
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    if (presort) {
        std::sort(idx.begin(), idx.end(),
                  [&](int a, int b) { return items[a] > items[b]; });
    }

    /*-------------------------------------------------------
     *  2. Greedy Differencing: 最軽量 bin へ順に突っ込む
     *-----------------------------------------------------*/
    for (int id : idx) {
        auto [cur_sum, b] = pq.top();
        pq.pop();
        cur_sum += items[id];             // アイテムを入れる
        bin_sum[b] = cur_sum;
        assign[id] = b;
        pq.emplace(cur_sum, b);           // 更新した和で再挿入
    }

    /*-------------------------------------------------------
     *  3. 最大 bin 和が必要な bin 容量
     *-----------------------------------------------------*/
    return *std::max_element(bin_sum.begin(), bin_sum.end());
}
