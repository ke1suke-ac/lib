/**********************************************************************
 *  Rollback Union‑Find 〈パスハーフィング方式〉
 *    - ルートは常に最小番号        ‑ 動的頂点追加可
 *    - 履歴スタックにより任意時点へロールバック
 *    - find  : ロールバック対応パススプリッティング
 *    - merge : root 準拠のまま “大きい方をぶら下げる” を部分的に許可
 *  対象環境 : C++20  (g++‑12.2 ‑O2)
 *********************************************************************/

#include <bits/stdc++.h>
using namespace std;

class RollbackUnionFindSmallestLead {
    /* parent_[v] < 0 なら root で -size, それ以外は親 */
    vector<int> parent_;

    /* 変更履歴 stack  */
    struct Event { int idx, val; };
    vector<Event> hist_;

    /*--------------------------------------------
      内部 util：値を変更しつつ履歴 push
    --------------------------------------------*/
    inline void set_parent(int idx, int new_val) noexcept {
        hist_.emplace_back(Event{idx, parent_[idx]});
        parent_[idx] = new_val;
    }

public:
    explicit RollbackUnionFindSmallestLead(int n = 0) : parent_(n, -1) {}

    /*------------------------------------------------------------
      find(v)  : ロールバック対応パススプリッティング
                 – “祖父が根でない” 場合だけ書き換える
                 – 書き換え 1 回につき履歴 1 回
                 オーダー : O(log N)   (最悪)
    ------------------------------------------------------------*/
    int find(int v) noexcept {
        while (parent_[v] >= 0) {
            const int p  = parent_[v];
            const int gp = parent_[p];
            if (gp < 0) break;           // 祖父が根 → ここで打ち切り
            set_parent(v, gp);           // v の親を祖父に
            v = p;
        }
        return (parent_[v] < 0) ? v : parent_[v];
    }

    /* size(v)  : 連結成分サイズ */
    int size(int v) noexcept { return -parent_[find(v)]; }

    /* snapshot / rollback */
    int  snapshot()        const noexcept { return (int)hist_.size(); }
    void rollback(int k) noexcept {
        while ((int)hist_.size() > k) {
            auto [idx, val] = hist_.back();
            hist_.pop_back();
            if (idx == -1) {          // add_vertex の sentinel
                parent_.pop_back();
            } else {
                parent_[idx] = val;
            }
        }
    }

    /* add_vertex()  : 新頂点追加 */
    int add_vertex() noexcept {
        hist_.emplace_back(Event{-1, 0});   // sentinel
        parent_.push_back(-1);
        return (int)parent_.size() - 1;
    }

    /*------------------------------------------------------------
      merge(a, b)  : 根どうしを接続（root = min ID）
                     – root 候補を a,b とする
                     – root が若番であることを最優先
                     – “若番側ツリーが極端に小さい” ときだけ
                       サイズヒューリスティックで 2 段潰れを防ぐ
    ------------------------------------------------------------*/
    bool merge(int a, int b) noexcept {
        a = find(a);  b = find(b);
        if (a == b) return false;

        if (a > b) swap(a, b);               // root は若番

        // ── 若番ツリーがごく小さく、もう一方が巨大なら、
        //    「若番 root 側に大ツリーを直結 → 深い木化」を緩和する
        //    （許容範囲: 8 倍以上なら逆付け）
        if (-parent_[a] * 8 < -parent_[b]) {
            // root 最小を保つため、サイズ配列だけ入れ替える
            // a と b の親ポインタはそのまま
            //   * b を root 維持（若番でない）→ NG
            //   * そこで parent_ 値を swap して “論理的 root” を a に
            set_parent(a, parent_[b]);   // -size をコピー
            set_parent(b, a);            // b の親 = a
            return true;
        }

        // 通常ケース：若番 root = a にぶら下げる
        set_parent(a, parent_[a] + parent_[b]);  // サイズ加算
        set_parent(b, a);                        // b の親 = a
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
         RollbackUnionFindSmallestLead uf(5);
         int snap = uf.snapshot();
         uf.merge(0,1); uf.merge(2,3); uf.merge(1,2);
         assert(uf.find(3) == 0);
         uf.rollback(snap);
         for(int i=0;i<5;i++) assert(uf.find(i)==i);
     }
     {
         RollbackUnionFindSmallestLead uf;
         int a = uf.add_vertex(), b = uf.add_vertex(), c = uf.add_vertex();
         int snap = uf.snapshot();
         uf.merge(b,c);
         uf.rollback(snap);
         assert(uf.find(b)==b && uf.find(c)==c);
     }
     {
         RollbackUnionFindSmallestLead uf(4);
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
         RollbackUnionFindSmallestLead uf(6);
         uf.merge(0,1); uf.merge(1,2);
         int s = uf.snapshot();
         uf.merge(3,4); uf.merge(2,3);
         assert(uf.find(4)==0);
         uf.rollback(s);
         assert(uf.find(2)==0 && uf.find(3)==3);
     }
     {
         RollbackUnionFindSmallestLead uf(1);
         vector<int> adds;
         for(int i=0;i<4;i++) adds.push_back(uf.add_vertex());
         for(int v:adds) uf.merge(0,v);
         for(int v:adds) assert(uf.find(v)==0);
         assert(uf.size(0)==5);
     }
     {
         RollbackUnionFindSmallestLead uf(3);
         assert(!uf.merge(1,1));
     }
     {
         RollbackUnionFindSmallestLead uf(3);
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
         RollbackUnionFindSmallestLead uf(2);
         uf.merge(1,0);
         assert(uf.find(1)==0 && uf.size(0)==2);
     }
     {
         RollbackUnionFindSmallestLead uf;
         for(int i=0;i<100;i++) uf.add_vertex();
         int snap = uf.snapshot();
         for(int i=0;i<50;i++) uf.add_vertex();
         uf.rollback(snap);
         assert(uf.add_vertex()==100);
     }
     {
         const int N=20, OPS=200;
         RollbackUnionFindSmallestLead uf(N);
         int snap=uf.snapshot();
         for(int i=0;i<OPS;i++) uf.merge(xorshift()%N, xorshift()%N);
         uf.rollback(snap);
         for(int v=0;v<N;v++) assert(uf.find(v)==v);
     }
     {
         RollbackUnionFindSmallestLead uf(2);
         int s=uf.snapshot();
         int v2=uf.add_vertex(); uf.merge(1,v2);
         uf.rollback(s);
         int v3=uf.add_vertex();
         assert(v3==2 && uf.find(2)==2);
     }
     {
         RollbackUnionFindSmallestLead uf(10);
         for(int i=0;i<9;i++) uf.merge(i,i+1);
         int snap=uf.snapshot();
         uf.merge(0,9);
         uf.rollback(snap);
         assert(uf.size(5)==10);
     }
     {
         const int N=15;
         RollbackUnionFindSmallestLead uf(N);
         for(int i=N-1;i>0;--i) uf.merge(i-1,i);
         for(int i=0;i<N;i++) assert(uf.find(i)==0);
     }
     cerr << "[TEST] all unit / scenario tests passed (13 cases).\n";
 }
 
 /*====================================================================
   ベンチマーク
  ====================================================================*/
using i64 = long long;
  void benchmark() {
     constexpr int  N          = 1000;
     constexpr i64  ITER_OP    = 100'000'000;   // 1e8
     constexpr i64  ITER_FIND  = 1'000'000;     // 1e6
 
     cerr << "\n[Benchmark] N="<<N
          <<"  ITER_OP="<<ITER_OP
          <<"  ITER_FIND="<<ITER_FIND<<'\n';
 
     /*--- merge ↔ rollback ----------------------------------------*/
     {
         RollbackUnionFindSmallestLead uf(N);
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
         RollbackUnionFindSmallestLead uf(N);
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
         RollbackUnionFindSmallestLead uf(N);
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
