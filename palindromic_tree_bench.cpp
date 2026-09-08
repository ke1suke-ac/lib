// benchmark_double_ended_eertree.cpp
// 実行方法の例: g++ -O2 -std=gnu++20 benchmark_double_ended_eertree.cpp -o bench && ./bench
//
// 本プログラムは、palindromic_tree.hpp をインクルードして、double_ended_eertree 構造体の
// ベンチマークを行います。計測はマイクロ秒単位です。
//
// ベンチマーク内容（1セット）:
//  ① 右・左交互にランダム文字を合計 N 文字追加
//  ② str() を 1 回呼ぶ
//  ③ 右・左交互に合計 N 回、(1) 1 文字削除 → (2) ランダム 1 文字追加 →
//     (3) distinct(), longest_suffix(), longest_prefix() をコール
//
// これを (10,000,000 / N) 回 繰り返した合計時間と、1 セット当たりの時間を表示します。
// ※ (10,000,000 / N) は 1 以上になるよう切り上げません。1 未満の場合は 1 回のみ実行します。

#include <bits/stdc++.h>
#include "palindromic_tree.hpp"

using namespace std;

// 便利な別名
using Eertree = double_ended_eertree<26, 'a'>;

struct BenchResult {
    int N;
    long long trials;
    long long total_us;
    double per_set_us;
    // 最適化抑制用の集計
    long long checksum;
};

BenchResult run_bench(int N) {
    // 試行回数: 10,000,000 / N 回（最低1）
    long long trials = 10000000LL / max(1, N);
    if (trials <= 0) trials = 1;

    // 乱数（毎回同じ結果になる固定シード）
    std::mt19937_64 rng(123456789);
    std::uniform_int_distribution<int> dist(0, 25);

    // 計測開始
    const auto t0 = std::chrono::steady_clock::now();

    long long checksum = 0; // 出力値の最適化抑制（副作用）

    for (long long it = 0; it < trials; ++it) {
        Eertree T;

        // ① 右・左交互に N 文字追加
        for (int i = 0; i < N; ++i) {
            char c = static_cast<char>('a' + dist(rng));
            if ((i & 1) == 0) T.push_back(c);
            else              T.push_front(c);
        }

        // ② str() を 1 回呼ぶ
        {
            string s = T.str();
            checksum += static_cast<long long>(s.size());
        }

        // ③ 右・左交互に N 回: 1 文字削除 → ランダム 1 文字追加 → distinct, suffix, prefix をコール
        for (int i = 0; i < N; ++i) {
            char c = static_cast<char>('a' + dist(rng));

            if ((i & 1) == 0) {
                T.pop_back();
                T.push_back(c);
            } else {
                T.pop_front();
                T.push_front(c);
            }

            // クエリ呼び出し（最適化抑制のため加算）
            checksum += T.distinct();
            checksum += T.longest_suffix();
            checksum += T.longest_prefix();
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    const long long total_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    const double per_set_us =
        (trials > 0) ? static_cast<double>(total_us) / static_cast<double>(trials) : 0.0;

    return BenchResult{N, trials, total_us, per_set_us, checksum};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<int> Ns = {100, 1000, 10000, 100000};

    cout << "double_ended_eertree benchmark (times in microseconds)\n";
    cout << "Trials = max(1, 10,000,000 / N)\n\n";
    cout << left
         << setw(10) << "N"
         << setw(14) << "Trials"
         << setw(18) << "Total(us)"
         << setw(18) << "PerSet(us)"
         << "Checksum\n";
    cout << string(10 + 14 + 18 + 18 + 8, '-') << "\n";

    long long grand_checksum = 0;
    for (int N : Ns) {
        BenchResult r = run_bench(N);
        grand_checksum += r.checksum;

        cout << left
             << setw(10) << r.N
             << setw(14) << r.trials
             << setw(18) << r.total_us
             << setw(18) << fixed << setprecision(2) << r.per_set_us
             << r.checksum << "\n";
    }

    // grand_checksum を出力（最適化抑制の保険）
    cout << "\nGrand checksum: " << grand_checksum << "\n";

    return 0;
}

// 実行結果(atcoder) - N=1億で550ms
// double_ended_eertree benchmark (times in microseconds)
// Trials = max(1, 10,000,000 / N)

// N         Trials        Total(us)         PerSet(us)        Checksum
// --------------------------------------------------------------------
// 100       100000        614232            6.14              362010007
// 1000      10000         555670            55.57             892342001
// 10000     1000          550637            550.64            3783047374
// 100000    100           542946            5429.46           10227809085

// Grand checksum: 15265208467
