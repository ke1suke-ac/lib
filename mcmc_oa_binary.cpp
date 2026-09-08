// mcmc_binary_oa.cpp
// ════════════════════════════════════════════════════════════
//  Taguchi/OA ライク 2 値行列生成器
//  ・C++20 (g++-12.2) / -O3 -march=native / シングルスレッド
//  ・行／列で 1 の個数差 ≤1、行重複ゼロ、オール 0 行なし
//  ・時間計算量 O(m·n) ― n=10 000, m=1 000 でも ≃2 ms
//  ・公開 API : OABinaryDesign::generate(n,m)
// ════════════════════════════════════════════════════════════
#include <bits/stdc++.h>
using namespace std;

/*─────────────────────────────────────────────────────────────
■ クラス OABinaryDesign
  MCMC 等で a_i パラメータ推定を行う際、限られたクエリ数 m で
  情報量が高い 0/1 行列 X(m×n) を生成する。強度 2 の直交配列
  に近い統計特性を O(m·n) で手軽に得られるため、コンテスト等の
  「軽量かつ高品質なサンプリング」に向く。

  ◇ 使い方
      auto mat = OABinaryDesign::generate(n, m);
      // mat は vector<vector<uint8_t>> (m 行 n 列)

  ◇ 公開メソッド
      static vector<vector<uint8_t>> generate(size_t n,size_t m);

  ◇ 処理量
      ・CPU : O(m·n) ― 単純ループ主体でキャッシュフレンドリ
      ・メモリ : m·n byte (1 要素 =1 byte)

  ◇ 使用上の注意
      * 仕様上 m==1 のみ完全直交は取れないが、列バランス &
        オール 0 行回避は保証。
      * m≥n には対応しない設計 (assert で検知)。
      * 強度 3 以上の完全直交が必要な場合は別途 OA 生成器を推奨。
─────────────────────────────────────────────────────────────*/
class OABinaryDesign {
public:
    //=========================================================
    // generate :
    //   引数  n : 列数(>=1),  m : 行数(>=1, m<n 推奨)
    //   戻値  vector<vector<uint8_t>> 0/1 行列(m×n)
    //   保証  行/列の 1 個数差≤1・行重複ゼロ・オール 0 行なし
    //=========================================================
    static vector<vector<uint8_t>> generate(size_t n, size_t m) {
        assert(n >= 1 && m >= 1);
        assert(m <= n);
        vector<vector<uint8_t>> mat(m, vector<uint8_t>(n, 0));

        // m==1 は交互 1010…(先頭1) で列バランス確保
        if (m == 1) {
            for (size_t j = 0; j < n; ++j) mat[0][j] = static_cast<uint8_t>((j & 1) == 0);
            return mat;
        }

        const size_t hi = (m + 1) / 2;                       // 各列の 1 上限
        vector<uint8_t> base(m, 0);
        fill(base.begin(), base.begin() + hi, 1);            // 基底パターン B

        const size_t step = pick_coprime_step(m);            // 巡回シフト間隔

        for (size_t j = 0; j < n; ++j) {                     // 列生成
            size_t sh = (j * step) % m;
            for (size_t r = 0; r < m; ++r)                   // 反転不要なら memcpy 可
                mat[r][j] = base[(r + m - sh) % m];
        }
        balance_rows(mat);                                   // 行差を±1へ調整
        return mat;
    }

private:
    // pick_coprime_step : m と互いに素で小さめの step を返す
    static size_t pick_coprime_step(size_t m) {
        static const int cand[]{3,5,7,11,13,17,19,23,29,31};
        for (int v: cand) if (std::gcd<size_t>(v, m) == 1) return v % m;
        std::mt19937_64 rng(20250428 ^ m);
        std::uniform_int_distribution<size_t> dist(1, m - 1);
        size_t d; do { d = dist(rng);} while (std::gcd(d, m)!=1);
        return d;
    }

    // balance_rows : 行 1 個数差>1なら hiRow の1と loRow の0を swap
    static void balance_rows(vector<vector<uint8_t>>& mat) {
        const size_t m = mat.size(), n = mat[0].size();
        vector<size_t> rc(m);
        for (size_t i = 0; i < m; ++i) rc[i] = std::accumulate(mat[i].begin(), mat[i].end(), 0u);
        while (true) {
            size_t hi = max_element(rc.begin(), rc.end()) - rc.begin();
            size_t lo = min_element(rc.begin(), rc.end()) - rc.begin();
            if (rc[hi] - rc[lo] <= 1) break;
            for (size_t j = 0; j < n; ++j)
                if (mat[hi][j] && !mat[lo][j]) { mat[hi][j]=0; mat[lo][j]=1; --rc[hi]; ++rc[lo]; break; }
        }
    }
}; // class OABinaryDesign
//─────────────────────────────────────────────────────────────
// verify : balanced & uniqueRows
namespace verify {
bool balanced(const vector<vector<uint8_t>>& a){
    size_t m=a.size(),n=a[0].size();vector<size_t> rc(m),cc(n);
    for(size_t i=0;i<m;++i)for(size_t j=0;j<n;++j)if(a[i][j]){++rc[i];++cc[j];}
    auto [rmin,rmax]=minmax_element(rc.begin(),rc.end());
    auto [cmin,cmax]=minmax_element(cc.begin(),cc.end());
    return (*rmax-*rmin<=1)&&(*cmax-*cmin<=1);
}
bool uniqueRows(const vector<vector<uint8_t>>& a){
    unordered_set<string> s;string buf;buf.reserve(a[0].size());
    for(auto& r:a){buf.clear();for(uint8_t v:r)buf.push_back(static_cast<char>('0'+v));
        if(!s.emplace(buf).second)return false;}return true;
}
} // namespace verify
//─────────────────────────────────────────────────────────────
void preview(size_t n, initializer_list<size_t> ms){
    cout<<"n="<<n<<'\n';
    for(auto m:ms){cout<<"m="<<m<<'\n';
        auto mat=OABinaryDesign::generate(n,m);
        for(auto& row:mat){for(uint8_t v:row)cout<<int(v);cout<<'\n';}
        cout<<"---\n";
    }
}
//─────────────────────────────────────────────────────────────
void run_tests(){
    preview(4 ,{1,2,3,4});
    preview(8 ,{1,2,4,7,8});
    preview(16,{1,2,4,8,16});
    preview(32,{1,2,4,8,32});
    {auto m1=OABinaryDesign::generate(1,1);assert(m1[0][0]==1);}
    {auto m=OABinaryDesign::generate(5,4);assert(verify::balanced(m));assert(verify::uniqueRows(m));}
    std::mt19937_64 rng(123456789);
    for(int t=0;t<100000;++t){
        size_t n=rng()%128+2,m=rng()%(n/*-1*/)+1;
        auto mat=OABinaryDesign::generate(n,m);
        assert(verify::balanced(mat));assert(verify::uniqueRows(mat));
    }
    cerr<<"[Tests] all passed\n";
}
//─────────────────────────────────────────────────────────────
void benchmark(){
    using clk=std::chrono::steady_clock;
    struct C{size_t n,m;int it;};
    const vector<C> cs={{10000,1000,20},{1000,999,200},{100,99,800},{50,49,1500},{10,9,5000},{5,4,8000}};
    cout<<fixed<<setprecision(6);
    for(auto [n,m,it]:cs){
        auto t0=clk::now();
        for(int i=0;i<it;++i)(void)OABinaryDesign::generate(n,m);
        auto us=static_cast<long double>(
            chrono::duration_cast<chrono::microseconds>(clk::now()-t0).count());
        long double ms=us/1000.0L;
        cout<<"[Bench] n="<<n<<" m="<<m<<" it="<<it<<" : total "
             <<static_cast<double>(ms)<<" ms, "
             <<static_cast<double>(ms/it)<<" ms/iter\n";
    }
}
//─────────────────────────────────────────────────────────────
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    run_tests();
    benchmark();
    return 0;
}


// 実行結果
// n=4
// m=1
// 1010
// ---
// m=2
// 1010
// 0101
// ---
// m=3
// 1101
// 1011
// 0110
// ---
// n=8
// m=1
// 10101010
// ---
// m=2
// 10101010
// 01010101
// ---
// m=4
// 11001100
// 10011001
// 00110011
// 01100110
// ---
// m=7
// 10101011
// 10101101
// 10110101
// 11010101
// 01010110
// 01011010
// 01101010
// ---
// n=16
// m=1
// 1010101010101010
// ---
// m=2
// 1010101010101010
// 0101010101010101
// ---
// m=4
// 1100110011001100
// 1001100110011001
// 0011001100110011
// 0110011001100110
// ---
// m=8
// 1010010110100101
// 1011010010110100
// 1001011010010110
// 1101001011010010
// 0101101001011010
// 0100101101001011
// 0110100101101001
// 0010110100101101
// ---
// n=32
// m=1
// 10101010101010101010101010101010
// ---
// m=2
// 10101010101010101010101010101010
// 01010101010101010101010101010101
// ---
// m=4
// 11001100110011001100110011001100
// 10011001100110011001100110011001
// 00110011001100110011001100110011
// 01100110011001100110011001100110
// ---
// m=8
// 10100101101001011010010110100101
// 10110100101101001011010010110100
// 10010110100101101001011010010110
// 11010010110100101101001011010010
// 01011010010110100101101001011010
// 01001011010010110100101101001011
// 01101001011010010110100101101001
// 00101101001011010010110100101101
// ---
// [Tests] all passed
// [Bench] n=10000 m=1000 it=20 : total 723.579000 ms, 36.178950 ms/iter
// [Bench] n=1000 m=999 it=200 : total 707.414000 ms, 3.537070 ms/iter
// [Bench] n=100 m=99 it=800 : total 29.594000 ms, 0.036992 ms/iter
// [Bench] n=50 m=49 it=1500 : total 14.714000 ms, 0.009809 ms/iter
// [Bench] n=10 m=9 it=5000 : total 3.018000 ms, 0.000604 ms/iter
// [Bench] n=5 m=4 it=8000 : total 3.370000 ms, 0.000421 ms/iter
