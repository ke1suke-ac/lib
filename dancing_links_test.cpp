/*
  dancing_links_test.cpp
  --------------------------------------------------------------
  dancing_links.hpp を用いた
    1. API 網羅テスト（エッジケース + ランダム → Naive 比較）
    2. solve_exact_cover のベンチマーク
  ビルド:
      g++ -std=c++20 -O2 -DNDEBUG dancing_links_test.cpp -o test
      ./test
*/
#include "dancing_links.hpp"

#include <algorithm>
#include <random>
#include <chrono>
#include <cassert>
#include <iomanip>

using namespace dancing_links;

/*===============================================================
  解の妥当性チェック
===============================================================*/
bool check_solution(const std::vector<std::vector<bool>>& mat,
                    const std::vector<int>& sol){
    if(mat.empty()) return sol.empty();
    const int m = static_cast<int>(mat.size());
    const int n = static_cast<int>(mat[0].size());

    std::vector<int> cover(n,0);
    for(int r:sol){
        if(r<0 || r>=m) return false;
        for(int j=0;j<n;++j)
            if(mat[r][j]) cover[j] += 1;
    }
    return std::all_of(cover.begin(), cover.end(),
                       [](int c){ return c==1; });
}

/*===============================================================
  Naive (全探索)  m<=15 程度で使用
===============================================================*/
bool naive_exact_cover(const std::vector<std::vector<bool>>& mat){
    const int m = static_cast<int>(mat.size());
    if(m==0) return true;
    const int n = static_cast<int>(mat[0].size());

    for(int mask=0; mask < (1<<m); ++mask){
        std::vector<int> cover(n,0);
        bool ok=true;
        for(int i=0;i<m && ok;++i)
            if(mask>>i & 1){
                for(int j=0;j<n;++j){
                    if(mat[i][j]) cover[j] += 1;
                    if(cover[j] > 1){ ok=false; break; }
                }
            }
        if(!ok) continue;
        ok = std::all_of(cover.begin(), cover.end(),
                         [](int c){ return c==1; });
        if(ok) return true;
    }
    return false;
}

/*===============================================================
  ランダム行列生成
===============================================================*/
std::vector<std::vector<bool>> random_matrix(int m,int n,double p=0.5){
    static std::mt19937_64 rng(std::random_device{}());
    std::bernoulli_distribution dist(p);
    std::vector<std::vector<bool>> v(m,std::vector<bool>(n,false));
    for(int i=0;i<m;++i) for(int j=0;j<n;++j) v[i][j]=dist(rng);
    return v;
}

/*===============================================================
  LMatrix 基本操作テスト
===============================================================*/
void unit_test_LMatrix_basic(){
    std::vector<std::vector<bool>> v={{1,1},{1,1}};
    bool** a = vec2raw(v);

    LMatrix M(a,2,2);
    auto* node = M.head()->right()->down(); // (row 0, col 0)

    std::cerr << M << std::endl;

    M.remove_row(node);
    assert(static_cast<Column*>(M.head()->right())->size()==1);
    M.restore_row(node);
    assert(static_cast<Column*>(M.head()->right())->size()==2);

    M.remove_column(node);
    assert(!M.is_trivial());
    M.restore_column(node);
    assert(!M.is_trivial());

    free_raw(a,2);
}

/*===============================================================
  solve_exact_cover エッジケース
===============================================================*/
void unit_test_edge_cases(){
    {
        // (m,n)=(0,0) → 自明に解無し
        std::vector<std::vector<bool>> v;
        bool** a=vec2raw(v);
        auto sol=solve_exact_cover(a,0,0);
        assert(sol.empty());
        free_raw(a,0);
    }
    {
        // 全 0 行列 → 解無し
        std::vector<std::vector<bool>> v(3,std::vector<bool>(4,false));
        bool** a=vec2raw(v);
        auto sol=solve_exact_cover(a,3,4);
        assert(sol.empty());
        free_raw(a,3);
    }
    {
        // 1x1 で 1 → 唯一解
        std::vector<std::vector<bool>> v={{1}};
        bool** a=vec2raw(v);
        auto sol=solve_exact_cover(a,1,1);
        assert(check_solution(v,sol));
        free_raw(a,1);
    }
    {
        // 唯一解ケース
        std::vector<std::vector<bool>> v={{1,0,1},{0,1,0}};
        bool** a=vec2raw(v);
        auto sol=solve_exact_cover(a,2,3);
        assert(check_solution(v,sol));
        free_raw(a,2);
    }
    {
        // 真に解無し例: 列 1 を覆う行が無い
        std::vector<std::vector<bool>> v={{1,0},{1,0}};
        bool** a=vec2raw(v);
        auto sol=solve_exact_cover(a,2,2);
        assert(sol.empty());
        free_raw(a,2);
    }
}

/*===============================================================
  ランダム正当性テスト (m<=12,n<=6)
===============================================================*/
void random_correctness_tests(){
    std::mt19937_64 rng(42);
    int cnt = 0;
    for(int tc=0; tc<20000; ++tc){
        int n = static_cast<int>(rng()%6 + 1);
        int m = static_cast<int>(rng()%12 + 1);
        auto mat = random_matrix(m,n,0.4);

        bool** a = vec2raw(mat);
        auto sol = solve_exact_cover(a,m,n);
        bool naive = naive_exact_cover(mat);
        bool fast_has = !sol.empty();

        assert(fast_has == naive);
        if(fast_has) assert(check_solution(mat,sol));
        if(fast_has) cnt++;
        free_raw(a,m);
    }
    std::cout << "fast_has: " << cnt << std::endl;
}

/*===============================================================
  ベンチマーク
===============================================================*/
void benchmark(){
    std::cout << "\n===== solve_exact_cover Benchmark =====\n";
    std::cout << " n  m    iters  total(ms)  avg(ms)\n";
    std::cout << "---------------------------------------\n";

    const std::vector<int> Ns={4,8,16,64,256};
    const std::vector<int> Ms={10,100,1000};

    for(int n:Ns) for(int m:Ms){
        int iters = (n<=16 && m<=100)? 1000 : 200;
        if(n>=64 && m==1000) iters = 2;

        auto start_all = std::chrono::steady_clock::now();
        for(int t=0;t<iters;++t){
            auto mat = random_matrix(m,n,0.5);
            bool** a = vec2raw(mat);
            auto sol = solve_exact_cover(a,m,n);
            free_raw(a,m);
        }
        auto end_all = std::chrono::steady_clock::now();
        double total_ms =
            std::chrono::duration<double,std::milli>(end_all-start_all).count();
        std::cout << std::setw(3)<<n<<" "
                  << std::setw(4)<<m<<" "
                  << std::setw(6)<<iters<<" "
                  << std::setw(10)<<std::fixed<<std::setprecision(2)<<total_ms<<" "
                  << std::setw(8)<<total_ms/iters << '\n';
    }
}

/*===============================================================
  main
===============================================================*/
int main(){
    std::cout << "== API 基本動作テスト ==" << std::endl;
    unit_test_LMatrix_basic();
    std::cout << "→ LMatrix remove/restore OK" << std::endl;

    unit_test_edge_cases();
    std::cout << "→ solve_exact_cover edge cases OK" << std::endl;

    random_correctness_tests();
    std::cout << "→ ランダムケース OK" << std::endl;

    benchmark();
    return 0;
}


// 実行結果
// == API 基本動作テスト ==
// → LMatrix remove/restore OK
// → solve_exact_cover edge cases OK
// → ランダムケース OK

// ===== solve_exact_cover Benchmark =====
//  n  m    iters  total(ms)  avg(ms)
// ---------------------------------------
//   4   10   1000       2.82     0.00
//   4  100   1000      24.55     0.02
//   4 1000    200      48.22     0.24
//   8   10   1000       4.81     0.00
//   8  100   1000      43.43     0.04
//   8 1000    200      82.55     0.41
//  16   10   1000       7.76     0.01
//  16  100   1000     273.34     0.27
//  16 1000    200     342.07     1.71
//  64   10    200       4.84     0.02
//  64  100    200     258.97     1.29
//  64 1000      2     430.91   215.46
// 256   10    200      29.18     0.15
// 256  100    200    1471.08     7.36
// 256 1000      2    4303.83  2151.92

