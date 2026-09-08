// Thompson Sampling の方が UCB1 より収束が速く、また動作も高速
// また、時間経過による状況の変化にも対応しやすい
// ただし、Thompson Samplingは報酬 true/false にのみ対応。UCB1は[0.0-1.0]の報酬に対応

#include <bits/stdc++.h>
using namespace std;
// =============================================================================
//                               ライブラリ部
// =============================================================================
namespace bandit {

/// \brief 競技プログラミング向けの超軽量ヘッダオンリー UCB1 実装
/// 依存は STL のみ。操作はすべて O(K) (K = 腕の本数)。
class UCB1 {
    int K;                          // 腕の本数
    vector<int> pulls;              // 各腕が選択された回数
    vector<double> reward_sum;      // 各腕の累積報酬
    int total_pulls = 0;            // 総試行数
public:
    explicit UCB1(int k) : K(k), pulls(k, 0), reward_sum(k, 0.0) {
        assert(K > 0);
    }

    /// \brief 次に引く腕のインデックスを返す
    int select_arm() {
        // まずは各腕を 1 回ずつ試行して母数 0 を回避する
        for (int i = 0; i < K; ++i) if (pulls[i] == 0) return i;

        int best_arm = 0;
        double best_ucb = -numeric_limits<double>::infinity();
        const double log_total = log(static_cast<double>(total_pulls));
        for (int i = 0; i < K; ++i) {
            double avg = reward_sum[i] / pulls[i];
            double ucb = avg + sqrt(2.0 * log_total / pulls[i]);
            if (ucb > best_ucb) {
                best_ucb = ucb;
                best_arm = i;
            }
        }
        return best_arm;
    }

    /// \brief arm を引いて reward を観測した後に内部状態を更新する
    void update(int arm, double reward) {
        ++pulls[arm];
        reward_sum[arm] += reward;
        ++total_pulls;
    }

    // --- アクセサ ---
    [[nodiscard]] const vector<int>& counts()  const { return pulls; }
    [[nodiscard]] const vector<double>& sums() const { return reward_sum; }
    [[nodiscard]] int total() const { return total_pulls; }
};

} // namespace bandit
// =============================================================================
//                               テスト & ベンチマーク
// =============================================================================
namespace test {
using bandit::UCB1;

static mt19937_64 rng(123456789ULL); // 再現性を保つ固定シード

// ----- 簡易アサートマクロ -----
#define TASSERT(expr, msg)                                                                            \
    do { if (!(expr)) { cerr << "[FAIL] " << msg << "\n"; exit(EXIT_FAILURE);} } while (0)

// ----- 隠れた真の平均 + ノイズを生成する腕モデル -----
struct HiddenArm {
    double base;             // 真の平均報酬
    uniform_real_distribution<double> noise; // ±幅の一様ノイズ
    HiddenArm(double b, double noise_width) : base(b), noise(-noise_width, noise_width) {}
    double operator()() { return base + noise(rng); }
};

// =============================================================================
//                               ユニットテスト
// =============================================================================
void test_basic_selection() {
    cerr << "[TEST] basic selection order\n";
    UCB1 algo(3);
    // 初回は順に 0,1,2 が返るはず
    TASSERT(algo.select_arm()==0, "first arm must be 0");
    algo.update(0,1.0);
    TASSERT(algo.select_arm()==1, "second arm must be 1");
    algo.update(1,0.5);
    TASSERT(algo.select_arm()==2, "third arm must be 2");
    cerr << "  passed\n";
}

void test_single_arm() {
    cerr << "[TEST] single arm degeneracy\n";
    UCB1 algo(1);
    for (int i=0;i<100;++i) {
        int a = algo.select_arm();
        TASSERT(a==0,"only arm is 0");
        algo.update(0,1.0);
    }
    cerr << "  passed\n";
}

void test_zero_rewards() {
    cerr << "[TEST] zero rewards stability\n";
    UCB1 algo(5);
    for (int i=0;i<1000;++i){
        int arm=algo.select_arm();
        algo.update(arm,0.0); // 全腕報酬 0
    }
    // 各腕が 1 回以上は選ばれていることだけ確認
    for(int c:algo.counts()) TASSERT(c>0,"each arm pulled at least once");
    cerr << "  passed\n";
}

// =============================================================================
//                       ランダムシナリオテスト (隠れ平均)
// =============================================================================
void test_random_hidden(int K, int pulls_per_run, int runs) {
    cerr << "[TEST] random hidden base scenario ("<<K<<" arms)\n";
    uniform_real_distribution<double> base_dist(0.3,0.9); // 真の平均を 0.3〜0.9 に設定
    constexpr double noise_width = 0.05; // ノイズ幅 ±0.05

    for(int iter=1; iter<=runs; ++iter) {
        vector<HiddenArm> arms;
        for(int i=0;i<K;++i) arms.emplace_back(base_dist(rng), noise_width);
        // 真の平均が最大の腕を取得
        int best_index = int(max_element(arms.begin(), arms.end(), [](const HiddenArm& a, const HiddenArm& b){return a.base < b.base;} ) - arms.begin());

        UCB1 algo(K);
        for(int t=0;t<pulls_per_run;++t){
            int arm = algo.select_arm();
            double reward = arms[arm]();
            algo.update(arm, reward);
        }

        const auto& cnt = algo.counts();
        int best_pulls = cnt[best_index];
        int max_pulls = *max_element(cnt.begin(), cnt.end());

        // 結果出力
        cerr << "Iteration "<<iter<<"\n  Bases :";
        for(const HiddenArm& h : arms){
            cerr << ' ' << fixed << setprecision(3) << h.base;
        }
        cerr << "\n  Counts:";
        for(int c:cnt){ cerr << ' ' << c; }
        cerr << "\n";

        // 当たり腕が最多であることを検証
        TASSERT(best_pulls == max_pulls, "best arm's count is maximum.");
    }
    cerr << "  all iterations passed\n";
}

// =============================================================================
//                             ベンチマーク (性能計測)
// =============================================================================
void benchmark_speed(int K = 1000, int iterations = 100000) {
    cerr << "[BENCH] speed test: K="<<K<<", iterations="<<iterations<<"\n";
    UCB1 algo(K);
    uniform_real_distribution<double> reward_dist(0.0,1.0);

    auto t0 = chrono::high_resolution_clock::now();
    for(int i=0;i<iterations;++i){
        int arm = algo.select_arm();
        // ここでは計測目的なのでコスト最小化のため定数報酬にする
        algo.update(arm, 1.0); // 固定報酬
    }
    auto t1 = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(t1 - t0).count();
    cerr << "  Elapsed: "<< fixed << setprecision(3) << ms << " ms (" << ms/iterations << " ms per op)\n";
}

// =============================================================================
//                               テスト実行関数
// =============================================================================
void run_all() {
    test_basic_selection();
    test_single_arm();
    test_zero_rewards();
    test_random_hidden(/*K*/5, /*pulls*/10000, /*runs*/10);
    benchmark_speed();
    cerr << "All tests + benchmark completed successfully.\n";
}

} // namespace test
// =============================================================================
//                                   main
// =============================================================================
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    test::run_all();
    return 0;
}

// 実行結果
// [TEST] basic selection order
//   passed
// [TEST] single arm degeneracy
//   passed
// [TEST] zero rewards stability
//   passed
// [TEST] random hidden base scenario (5 arms)
// Iteration 1
//   Bases : 0.509 0.460 0.382 0.317 0.821
//   Counts: 150 112 79 63 9596
// Iteration 2
//   Bases : 0.660 0.450 0.735 0.858 0.308
//   Counts: 312 89 662 8884 53
// Iteration 3
//   Bases : 0.758 0.594 0.882 0.345 0.887
//   Counts: 507 146 4255 52 5040
// Iteration 4
//   Bases : 0.568 0.849 0.878 0.477 0.438
//   Counts: 141 2885 6804 91 79
// Iteration 5
//   Bases : 0.864 0.804 0.450 0.842 0.840
//   Counts: 4137 1154 81 2353 2275
// Iteration 6
//   Bases : 0.501 0.896 0.681 0.815 0.577
//   Counts: 94 8359 270 1140 137
// Iteration 7
//   Bases : 0.818 0.588 0.453 0.803 0.611
//   Counts: 5718 224 105 3684 269
// Iteration 8
//   Bases : 0.502 0.824 0.470 0.870 0.712
//   Counts: 107 1966 90 7408 429
// Iteration 9
//   Bases : 0.324 0.381 0.802 0.743 0.707
//   Counts: 69 84 7394 1583 870
// Iteration 10
//   Bases : 0.859 0.537 0.671 0.720 0.790
//   Counts: 7711 134 320 516 1319
//   all iterations passed
// [BENCH] speed test: K=1000, iterations=100000
//   Elapsed: 444.044 ms (0.004 ms per op)
// All tests + benchmark completed successfully.
