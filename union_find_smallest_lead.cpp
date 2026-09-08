/**********************************************************************
 *  Rollback Union‑Find 〈パスハーフィング方式〉
 *    ‑ ルートは常に最小番号        ‑ 動的頂点追加可
 *    ‑ 履歴スタックにより任意時点へロールバック
 *    ‑ find はパスハーフィングで O(log N)（ロールバック可能）
 *  対象環境 : C++20  (g++‑12.2 ‑O2)
 *********************************************************************/

 #include <bits/stdc++.h>
 using namespace std;
 using i64 = long long;
 
 /*====================================================================
   クラス定義
  ====================================================================*/
 class RollbackUnionFindPH {
     /* parent_[v] < 0 なら v は根で -size, それ以外は親 */
     std::vector<int> parent_;
 
     /* 変更履歴を積むスタック */
     struct Event { int idx, val; };        // idx == -1 なら頂点追加の sentry
     std::vector<Event> hist_;
 
 public:
     /*----------------------------------------------------------------
       コンストラクタ                     時間: O(N)
      ----------------------------------------------------------------*/
     explicit RollbackUnionFindPH(int n = 0) : parent_(n, -1) {}
 
     /*----------------------------------------------------------------
       find(v)  : ルートを返す (パスハーフィング)
                  時間: O(log N)   空間: O(log N)  ※履歴 push 数
      ----------------------------------------------------------------*/
     int find(int v) noexcept {
         while (parent_[v] >= 0) {
             int p  = parent_[v];          // 親
             int gp = parent_[p];          // 祖父（根なら負値）
             if (gp >= 0) {                // 祖父が存在 → パスハーフィング
                 hist_.push_back({v, parent_[v]});   // 元の親を記録
                 parent_[v] = gp;          // 親を祖父に付け替え
             }
             v = p;
         }
         return v;
     }
 
     /*----------------------------------------------------------------
       size(v)  : 連結成分のサイズ         時間: O(log N)
      ----------------------------------------------------------------*/
     int size(int v) noexcept { return -parent_[find(v)]; }
 
     /*----------------------------------------------------------------
       snapshot() : 現在の履歴サイズを返す 時間: O(1)
      ----------------------------------------------------------------*/
     int snapshot() const noexcept { return (int)hist_.size(); }
 
     /*----------------------------------------------------------------
       rollback(k) : snapshot()==k の状態へ戻す
                     時間: 巻き戻すイベント数に比例 (累積 O(変更総数))
      ----------------------------------------------------------------*/
     void rollback(int k) noexcept {
         while ((int)hist_.size() > k) {
             auto [idx, val] = hist_.back();
             hist_.pop_back();
             if (idx == -1) {             // 頂点追加の取り消し
                 parent_.pop_back();
             } else {
                 parent_[idx] = val;      // 値を復元
             }
         }
     }
 
     /*----------------------------------------------------------------
       add_vertex() : 新頂点を追加し ID を返す
                      時間: O(1)          空間: O(1)
      ----------------------------------------------------------------*/
     int add_vertex() noexcept {
         hist_.push_back({-1, 0});        // sentry
         parent_.push_back(-1);
         return (int)parent_.size() - 1;
     }
 
     /*----------------------------------------------------------------
       merge(a,b) : a,b を連結 (既に同一なら false)
                    時間: O(log N)
      ----------------------------------------------------------------*/
     bool merge(int a, int b) noexcept {
         a = find(a);  b = find(b);
         if (a == b) return false;
 
         if (a > b) std::swap(a, b);      // root は小さい番号
 
         hist_.push_back({a, parent_[a]});
         hist_.push_back({b, parent_[b]});
 
         parent_[a] += parent_[b];        // サイズ加算
         parent_[b]  = a;                 // 親設定
         return true;
     }
 };
 
 /*====================================================================
   疑似乱数 (xorshift32)            時間: O(1)
  ====================================================================*/
 static inline uint32_t xorshift() noexcept {
     static uint32_t x = 2463534242u;
     x ^= x << 13;  x ^= x >> 17;  x ^= x << 5;
     return x;
 }
 
 /*====================================================================
   単体 & シナリオテスト (13 ケース)
  ====================================================================*/
 void unit_tests() {
     /* 基本 3 ケース ------------------------------------------------*/
     {
         RollbackUnionFindPH uf(5);
         int snap = uf.snapshot();
         uf.merge(0,1); uf.merge(2,3); uf.merge(1,2);
         assert(uf.find(3) == 0);
         uf.rollback(snap);
         for(int i=0;i<5;i++) assert(uf.find(i)==i);
     }
     {
         RollbackUnionFindPH uf;
         int a = uf.add_vertex(), b = uf.add_vertex(), c = uf.add_vertex();
         int snap = uf.snapshot();
         uf.merge(b,c);
         uf.rollback(snap);
         assert(uf.find(b)==b && uf.find(c)==c);
     }
     {
         RollbackUnionFindPH uf(4);
         uf.merge(1,3);
         int snap = uf.snapshot();
         int v4 = uf.add_vertex();
         uf.merge(0,v4);
         assert(uf.find(v4)==0);
         uf.rollback(snap);
         assert(uf.find(0)==0 && uf.find(3)==1);
     }
 
     /* 追加シナリオ #1〜#10 ----------------------------------------*/
     {
         RollbackUnionFindPH uf(6);
         uf.merge(0,1); uf.merge(1,2);
         int s = uf.snapshot();
         uf.merge(3,4); uf.merge(2,3);
         assert(uf.find(4)==0);
         uf.rollback(s);
         assert(uf.find(2)==0 && uf.find(3)==3);
     }
     {
         RollbackUnionFindPH uf(1);
         vector<int> adds;
         for(int i=0;i<4;i++) adds.push_back(uf.add_vertex());
         for(int v:adds) uf.merge(0,v);
         for(int v:adds) assert(uf.find(v)==0);
         assert(uf.size(0)==5);
     }
     {
         RollbackUnionFindPH uf(3);
         assert(!uf.merge(1,1));
     }
     {
         RollbackUnionFindPH uf(3);
         int s0 = uf.snapshot();
         int v3 = uf.add_vertex();
         uf.merge(0,v3);
         int s1 = uf.snapshot();
         uf.merge(1,2); uf.merge(2,0);
         uf.rollback(s1);
         assert(uf.find(v3)==0 && uf.find(1)==1);
         uf.rollback(s0);
         for(int v:{0,1,2}) assert(uf.find(v)==v);
     }
     {
         RollbackUnionFindPH uf(2);
         uf.merge(1,0);
         assert(uf.find(1)==0 && uf.size(0)==2);
     }
     {
         RollbackUnionFindPH uf;
         for(int i=0;i<100;i++) uf.add_vertex();
         int snap = uf.snapshot();
         for(int i=0;i<50;i++) uf.add_vertex();
         uf.rollback(snap);
         assert(uf.add_vertex()==100);
     }
     {
         const int N=20, OPS=200;
         RollbackUnionFindPH uf(N);
         int snap=uf.snapshot();
         for(int i=0;i<OPS;i++) uf.merge(xorshift()%N, xorshift()%N);
         uf.rollback(snap);
         for(int v=0;v<N;v++) assert(uf.find(v)==v);
     }
     {
         RollbackUnionFindPH uf(2);
         int s=uf.snapshot();
         int v2=uf.add_vertex(); uf.merge(1,v2);
         uf.rollback(s);
         int v3=uf.add_vertex();
         assert(v3==2 && uf.find(2)==2);
     }
     {
         RollbackUnionFindPH uf(10);
         for(int i=0;i<9;i++) uf.merge(i,i+1);
         int snap=uf.snapshot();
         uf.merge(0,9);
         uf.rollback(snap);
         assert(uf.size(5)==10);
     }
     {
         const int N=15;
         RollbackUnionFindPH uf(N);
         for(int i=N-1;i>0;--i) uf.merge(i-1,i);
         for(int i=0;i<N;i++) assert(uf.find(i)==0);
     }
     cerr << "[TEST] all unit / scenario tests passed (13 cases).\n";
 }
 
 /*====================================================================
   ベンチマーク
  ====================================================================*/
 void benchmark() {
     constexpr int  N          = 1000;
     constexpr i64  ITER_OP    = 100'000'000;   // 1e8
     constexpr i64  ITER_FIND  = 1'000'000;     // 1e6
 
     cerr << "\n[Benchmark] N="<<N
          <<"  ITER_OP="<<ITER_OP
          <<"  ITER_FIND="<<ITER_FIND<<'\n';
 
     /*--- merge ↔ rollback ----------------------------------------*/
     {
         RollbackUnionFindPH uf(N);
         auto beg = chrono::high_resolution_clock::now();
         for(i64 i=0;i<ITER_OP;i++){
             int snap=uf.snapshot();
             uf.merge(xorshift()%N, xorshift()%N);
             uf.rollback(snap);
         }
         double sec = chrono::duration<double>(chrono::high_resolution_clock::now()-beg).count();
         cout << "[merge/rollback]  total " << sec << " sec   "
              << (sec/ITER_OP*1e9) << " ns/op\n";
     }
 
     /*--- add_vertex ↔ rollback -----------------------------------*/
     {
         RollbackUnionFindPH uf(N);
         auto beg = chrono::high_resolution_clock::now();
         for(i64 i=0;i<ITER_OP;i++){
             int snap=uf.snapshot();
             uf.add_vertex();
             uf.rollback(snap);
         }
         double sec = chrono::duration<double>(chrono::high_resolution_clock::now()-beg).count();
         cout << "[add/rollback]    total " << sec << " sec   "
              << (sec/ITER_OP*1e9) << " ns/op\n";
     }
 
     /*--- worst‑case find (線形鎖) --------------------------------*/
     {
         RollbackUnionFindPH uf(N);
         for(int i=N-1;i>0;--i) uf.merge(i, i-1);   // 鎖を構築
         const int leaf = N-1;
 
         volatile int sink = 0;                     // 最適化抑止
         auto beg = chrono::high_resolution_clock::now();
         for(i64 i=0;i<ITER_FIND;i++) sink += uf.find(leaf);
         auto end = chrono::high_resolution_clock::now();
 
         double sec = chrono::duration<double>(end-beg).count();
         cout << "[worst‑case find] total " << sec << " sec   "
              << (sec/ITER_FIND*1e9) << " ns/op   (sink="<<sink<<")\n";
     }
 }
 
 /*====================================================================
   main
  ====================================================================*/
 int main(){
     ios::sync_with_stdio(false);
     cin.tie(nullptr);
 
     unit_tests();
     benchmark();
     return 0;
 }
 
// 実行結果
// [TEST] all unit / scenario tests passed (13 cases).

// [Benchmark] N=1000  ITER_OP=100000000  ITER_FIND=1000000
// [merge/rollback]  total 1.59396 sec   15.9396 ns/op
// [add/rollback]    total 0.412965 sec   4.12965 ns/op
// [worst‑case find] total 0.00259665 sec   2.59665 ns/op   (sink=0)
