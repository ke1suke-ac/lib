//  ------------------------------
//  FFD bin-packing (single thread)
//
//  変更点
//    1. すべての std::size_t → int へ置換（reserve_bins も int 化）
//    2. 関数第 4 引数に max_bins=0 を追加
//         ・max_bins>0 かつ bin 数が超過した時点で -1 を即時返却
//    3. 既存の機能（容量超過アイテムは assign=-1 ＆ エラー）を維持
//
//  g++ -std=c++20 -O3 -march=native ffd.cpp
//  ------------------------------
#include <algorithm>    // sort
#include <chrono>       // benchmark (sample 用)
#include <iostream>     // cout
#include <numeric>      // iota
#include <vector>       // vector

//---------------------------------
// FFD 本体
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
