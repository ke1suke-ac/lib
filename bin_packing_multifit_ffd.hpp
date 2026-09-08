// 基本的に遅い
// 小規模 & 高精度要求のみ考える - K <= 10 && N <= 2*K


//  ------------------------------
//  FFD + Multifit bin-packing  (single thread)
//
//  本ファイルは 2 つの関数を収録する。
//
//   (1) partition()
//        └ First-Fit Decreasing (FFD) により
//          ・容量超過アイテムは assign=-1、戻り値 -1
//          ・max_bins を超えたら即座に -1
//
//   (2) multifit()
//        └ partition() を内部で呼び出し、
//          「K 個 (以下) の bin に収まる最小 bin_capacity」を
//          二分探索で求める Multifit 法。
//          成功時: その最小容量を返し、assign には最適時の割り当て。
//          想定: アイテム容量型 T は int / long long など整数型。
//
//  追加仕様
//    ・partition() 呼び出し時の reserve_bins を K → K+1 に拡大
//      → bin 個数が K を超えた瞬間に再配置が起きるのを防ぐ
//
//    ・main() でランダムケースを大量生成し Multifit をテスト
//      * 整合性チェック:
//          1. multifit が返した容量で partition() すると bin ≤ K
//          2. 容量-1 で失敗 (bin > K or -1)
//          3. assign のサイズが items と同じで -1 が含まれない
//
//  g++ -std=c++20 -O3 -march=native multifit.cpp
//  ------------------------------
#include <algorithm>    // sort, max
#include <cassert>      // assert
#include <chrono>       // benchmark / timing
#include <iostream>     // cout
#include <numeric>      // iota, accumulate
#include <random>       // random_device, mt19937, distributions
#include <type_traits>  // is_integral_v
#include <vector>       // vector

//---------------------------------
// 1. FFD partition
//---------------------------------
template <typename T>
int partition(
    const T bin_capacity,           // 各 bin の容量
    const std::vector<T>& items,    // アイテムのサイズ列
    std::vector<int>& assign,       // (出力) アイテム → bin インデックス
    int  max_bins   = 0,            // (任意) bin 上限。0 なら無制限
    int  reserve_bins = 0           // (任意) 初期予約 bin 数
) {
    const int n = static_cast<int>(items.size());
    assign.resize(n);

    // ① アイテムを降順に並び替えるためのインデックス配列を用意
    std::vector<int> ord(n);
    std::iota(ord.begin(), ord.end(), 0);
    std::sort(ord.begin(), ord.end(), [&](int a, int b) {
        return items[a] > items[b];
    });

    // ② bin の残容量リスト (連続メモリの vector1 本)
    std::vector<T> rem;
    rem.reserve(static_cast<std::size_t>(reserve_bins ? reserve_bins : n)); // 十分な容量を一括確保

    bool error = false;                // bin_capacity 超過アイテム検知フラグ

    // ③ FFD ループ
    for (int idx : ord) {
        const T w = items[idx];

        // ④ bin_capacity 超過ならスキップ。assign は -1、戻り値は後で -1
        if (w > bin_capacity) {        // [[unlikely]]
            assign[idx] = -1;
            error = true;
            continue;                  // 他アイテム処理は継続
        }

        bool placed = false;

        // ⑤ 既存 bin を先頭から走査
        for (int b = 0, m = static_cast<int>(rem.size()); b < m; ++b) {
            if (rem[b] >= w) {         // [[likely]]
                rem[b] -= w;
                assign[idx] = b;
                placed = true;
                break;
            }
        }

        // ⑥ 入らなければ新 bin を開く
        if (!placed) {
            rem.emplace_back(bin_capacity - w);
            assign[idx] = static_cast<int>(rem.size() - 1);

            // ⑦ bin 上限を超えたら即座にエラー終了
            if (max_bins > 0 && static_cast<int>(rem.size()) > max_bins) {
                return -1;
            }
        }
    }

    // ⑧ エラーがあれば -1, なければ使用 bin 数
    return error ? -1 : static_cast<int>(rem.size());
}

//---------------------------------
// 2. Multifit (二分探索で最小 bin_capacity)
//---------------------------------
template <typename T>
T multifit(
    const int K,                     // 目標とする bin 個数 (≧1)
    const std::vector<T>& items,     // アイテムのサイズ列
    std::vector<int>& assign         // (出力) アイテム → bin インデックス
) {
    static_assert(std::is_integral_v<T>,
                  "multifit は整数型 T を想定しています");

    // ------ 前処理：合計値・最大値を O(N) で取得 ------
    long long sum_ll = 0;
    T max_item = 0;
    for (T v : items) {
        sum_ll += v;
        if (v > max_item) max_item = v;
    }

    // アイテムが空の場合
    if (items.empty()) {
        assign.clear();
        return 0;
    }

    // ------ 二分探索の探索範囲 ------
    // L = ceil(total/K) と max_item の大きい方
    T L = static_cast<T>((sum_ll + K - 1) / K);   // ceil(sum/K)
    if (L < max_item) L = max_item;
    // R = total
    T R = static_cast<T>(sum_ll);

    // 追加考察:
    //   R′ = max_item + (sum - max_item) / (K - 1) などもあるが、
    //   K が 1 や 2 などの時に例外処理が煩雑となる。
    //   2 進探索幅は log2(R-L) ≲ 31 で十分小さいため
    //   実装のシンプルさと頑健さを優先。

    // ------ 二分探索 ------
    std::vector<int> best_assign;
    T best_cap = R;

    T lo = L, hi = R;
    int cnt = 0;
    while (lo <= hi) {
        T mid = lo + (hi - lo) / 2;           // オーバーフロー回避形
        if(cnt < 3 && hi == R){
            if(cnt == 0) mid = std::min(mid, lo + lo / 20 + 1);
            else if(cnt == 1) mid = std::min(mid, lo + lo / 4 + 1);
            else mid = std::min(mid, lo * 2);
            cnt++;
        }

        std::vector<int> tmp_assign;
        int bins = partition(mid, items, tmp_assign,
                             /*max_bins=*/K, /*reserve_bins=*/K+1); // reserve_bins=K+1 に変更
        if (bins != -1 && bins <= K) {
            // ---- 収まった → 容量を縮められるか試す ----
            best_cap = mid;
            best_assign.swap(tmp_assign);
            hi = mid - 1;
        } else {
            // ---- 収まらない → 容量を拡大 ----
            lo = mid + 1;
        }
    }

    // 結果反映
    assign.swap(best_assign);
    return best_cap;
}
