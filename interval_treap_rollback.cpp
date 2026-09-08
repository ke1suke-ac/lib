// rollback付き区間操作
// 100万回の操作で2秒かかる
// メモリ消費量も多い（完全永続でメモリを解放しない）
// 毎回 new が発生する

#include <bits/stdc++.h>
using namespace std;
using HRClk = std::chrono::high_resolution_clock;

/* ---------------- RNG ---------------- */
struct RNG{
    uint64_t s[2];
    static uint64_t rotl(uint64_t x,int k){return (x<<k)|(x>>(64-k));}
    explicit RNG(uint64_t seed=1){ splitmix64(seed); }
    void splitmix64(uint64_t z){
        for(int i=0;i<2;++i){
            z+=0x9e3779b97f4a7c15ULL;
            z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
            z=(z^(z>>27))*0x94d049bb133111ebULL;
            s[i]=z^(z>>31);
        }
    }
    uint64_t next(){
        uint64_t r=s[0]+s[1],t=s[0]^s[1];
        s[0]=rotl(s[0],55)^t^(t<<14); s[1]=rotl(t,36); return r;
    }
    uint32_t operator()(){ return static_cast<uint32_t>(next()); }
    uint64_t operator()(uint64_t mod){ return next()%mod; }
} rng(123456789);

/* ===========================================================
 *  IntervalTreap
 * -----------------------------------------------------------
 *  区間 [l, r) を動的に管理する Treap ベースのデータ構造。
 *  - 区間の追加・削除を行うたびに自動で重複区間を統合／分割。
 *  - 各操作は（期待）O(log N)。
 *  - ロールバック機能を備え、直前の状態へ戻ることが可能。
 *  - 内部では各ノードに区間長の部分和を保持し、全被覆長を
 *    O(1) で取得できる。
 *  - 挿入・削除は永続化（copy‑on‑write）しているため、
 *    テスト用に「以前の root」をヒストリとして保持できる。
 *  - 典型的なヒューリスティックでは
 *      ・区間被りチェック
 *      ・操作の undo / redo
 *      ・全体長の高速評価
 *    が求められる場面で有用。
 * ===========================================================
 */
struct IntervalTreap{
    /*--------- ノード構造 ---------*/
    struct Node{
        int l,r;                 // 区間 [l, r)
        uint32_t pri;            // Treap 優先度（乱数）
        Node* ch[2];             // 子ノード
        int sz;                  // 部分木サイズ
        long long len;           // 部分木が被覆する長さの総和
    };

    /* copy‑on‑write 用プール       */
    vector<Node*> pool;

    /* 内部ユーティリティ（実装詳細）*/
    Node* clone(Node* t){ if(!t) return nullptr; Node* c=new Node(*t); pool.push_back(c); return c; }
    Node* make(int l,int r){ Node* t=new Node{l,r,rng(),{nullptr,nullptr},1,(long long)(r-l)}; pool.push_back(t); return t; }

    /*--- サブツリーのメタ更新 ---*/
    static int SZ(Node* t){ return t? t->sz:0; }
    static long long LN(Node* t){ return t? t->len:0; }
    static void upd(Node* t){ t->sz=1+SZ(t->ch[0])+SZ(t->ch[1]); t->len=(t->r-t->l)+LN(t->ch[0])+LN(t->ch[1]); }

    /*--- 走査ヘルパ ---*/
    static Node* maxNode(Node* t){ while(t&&t->ch[1]) t=t->ch[1]; return t; }
    static Node* minNode(Node* t){ while(t&&t->ch[0]) t=t->ch[0]; return t; }

    /*--- Treap 基本操作（split / merge / erase_key）---*/
    pair<Node*,Node*> split(Node* t,int x){
        if(!t) return {nullptr,nullptr};
        t=clone(t);
        if(x<=t->l){ auto [L,R]=split(t->ch[0],x); t->ch[0]=R; upd(t); return {L,t}; }
        auto [L,R]=split(t->ch[1],x); t->ch[1]=L; upd(t); return {t,R};
    }
    Node* merge(Node* a,Node* b){
        if(!a||!b) return a? a:b;
        if(a->pri<b->pri){ a=clone(a); a->ch[1]=merge(a->ch[1],b); upd(a); return a; }
        b=clone(b); b->ch[0]=merge(a,b->ch[0]); upd(b); return b;
    }
    Node* erase_key(Node* t,int key){
        if(!t) return t;
        t=clone(t);
        if(key<t->l) t->ch[0]=erase_key(t->ch[0],key);
        else if(key>t->l) t->ch[1]=erase_key(t->ch[1],key);
        else return merge(t->ch[0],t->ch[1]);
        upd(t); return t;
    }

    /*--- ルートとロールバック履歴 ---*/
    Node* root=nullptr;
    vector<Node*> hist;          // 挿入 / 削除 前の root を保存

    /*========== 内部挿入 ==========*/
    void insert_raw(int l,int r){
        auto [A,B]=split(root,l);
        auto [M,C]=split(B,r);
        int nl=l,nr=r;
        /* 左側で [nl, nr) と重なる区間を吸収 */
        for(Node* lf=maxNode(A); lf&&lf->r>=nl; lf=maxNode(A)){
            nl=min(nl,lf->l); nr=max(nr,lf->r); A=erase_key(A,lf->l);
        }
        /* 中央 M は完全吸収対象 */
        if(M){
            vector<Node*> st{M};
            while(!st.empty()){
                Node* t=st.back(); st.pop_back();
                nr=max(nr,t->r);
                if(t->ch[0]) st.push_back(t->ch[0]);
                if(t->ch[1]) st.push_back(t->ch[1]);
            }
        }
        /* 右側で [nl, nr) と重なる区間を吸収 */
        for(Node* rf=minNode(C); rf&&rf->l<=nr; rf=minNode(C)){
            nr=max(nr,rf->r); C=erase_key(C,rf->l);
        }
        root=merge( merge(A,make(nl,nr)), C );
    }

    /*========== 内部削除 ==========*/
    void erase_raw(int l,int r){
        auto [A,B]=split(root,l);
        auto [M,C]=split(B,r);
        vector<pair<int,int>> res;          // 削除後に残す断片
        /* 左端と被る区間の分割処理 */
        if(Node* lf=maxNode(A); lf&&lf->r>l){
            int L=lf->l,R=lf->r; A=erase_key(A,L);
            if(L<l) res.emplace_back(L,l);
            if(R>r) res.emplace_back(r,R);
        }
        /* 完全にかかる区間の右端側断片 */
        if(M){
            vector<Node*> st{M};
            while(!st.empty()){
                Node* t=st.back(); st.pop_back();
                if(t->r>r) res.emplace_back(max(r,t->l),t->r);
                if(t->ch[0]) st.push_back(t->ch[0]);
                if(t->ch[1]) st.push_back(t->ch[1]);
            }
        }
        root=merge(A,C);
        /* 残った断片をまとめて再挿入 */
        sort(res.begin(),res.end());
        for(size_t i=0;i<res.size();){
            int L=res[i].first,R=res[i].second; ++i;
            while(i<res.size() && res[i].first<=R){ R=max(R,res[i].second); ++i; }
            insert_raw(L,R);
        }
    }

public:
    /* =================== Public API =================== */

    //-------------------------------------------------------------------------
    // insert(l, r)
    //-------------------------------------------------------------------------
    // [l, r) を追加する。連続または重複する既存区間とは自動でマージする。
    // 【計算量】期待 O(log N)（split + merge のみ）
    // 【注意点】l >= r の場合は何もしない。undo 可能（rollback）。
    void insert(int l,int r){ if(l<r){ hist.push_back(root); insert_raw(l,r);} }

    //-------------------------------------------------------------------------
    // erase(l, r)
    //-------------------------------------------------------------------------
    // [l, r) を削除する。部分的に重なる区間は分割して残る。
    // 【計算量】期待 O(log N + K) （K は分割して残す新区間数、通常小さい）
    // 【注意点】l >= r の場合は無効。undo 可能（rollback）。
    void erase(int l,int r){ if(l>=r) return; hist.push_back(root); erase_raw(l,r); }

    //-------------------------------------------------------------------------
    // find_interval(x) -> const Node*
    //-------------------------------------------------------------------------
    // 点 x を含む区間ノードを返す。存在しない場合は nullptr。
    // 【計算量】O(log N)
    const Node* find_interval(int x) const{
        Node* t=root; while(t){ if(x<t->l) t=t->ch[0]; else if(x>=t->r) t=t->ch[1]; else return t; } return nullptr;
    }

    //-------------------------------------------------------------------------
    // contains(x) -> bool
    //-------------------------------------------------------------------------
    // 点 x がいずれかの区間に含まれるか判定。
    // 【計算量】O(log N)
    bool contains(int x) const{ return find_interval(x)!=nullptr; }

    //-------------------------------------------------------------------------
    // nearest_left(x) -> const Node*
    //-------------------------------------------------------------------------
    // x より左側で最も右端が小さい区間を返す（strict に左側）。
    // 見つからなければ nullptr。
    // 【計算量】O(log N)
    const Node* nearest_left(int x) const{
        Node* t=root; const Node* best=nullptr;
        while(t){ if(t->r<x){ best=t; t=t->ch[1]; } else t=t->ch[0]; } return best;
    }

    //-------------------------------------------------------------------------
    // nearest_right(x) -> const Node*
    //-------------------------------------------------------------------------
    // x 以上で最も左端が小さい区間を返す。
    // 見つからなければ nullptr。
    // 【計算量】O(log N)
    const Node* nearest_right(int x) const{
        Node* t=root; const Node* best=nullptr;
        while(t){ if(x<=t->l){ best=t; t=t->ch[0]; } else t=t->ch[1]; } return best;
    }

    //-------------------------------------------------------------------------
    // size() -> int
    //-------------------------------------------------------------------------
    // 区間数（ノード数）を取得。
    // 【計算量】O(1)
    int size() const{ return root? root->sz:0; }

    //-------------------------------------------------------------------------
    // covered_length() -> long long
    //-------------------------------------------------------------------------
    // 登録されている全区間の長さの総和。
    // 【計算量】O(1)
    long long covered_length() const{ return root? root->len:0; }

    //-------------------------------------------------------------------------
    // iterate(f)
    //-------------------------------------------------------------------------
    // 区間を左端昇順で列挙しながらコールバック f を呼ぶ。
    // 【計算量】O(N)
    template<class F> void iterate(const F& f) const{
        function<void(Node*)> dfs=[&](Node* t){ if(!t) return; dfs(t->ch[0]); f(t); dfs(t->ch[1]); }; dfs(root);
    }

    //-------------------------------------------------------------------------
    // rollback() -> bool
    //-------------------------------------------------------------------------
    // 直前の状態へ巻き戻す。履歴が空なら false を返す。
    // 【計算量】O(1)
    bool rollback(){ if(hist.empty()) return false; root=hist.back(); hist.pop_back(); return true; }

    /* --------------------------------------------------
     * 以下はベンチマーク用の低レベル API（通常利用は非推奨）
     * --------------------------------------------------*/

    // bench_insert/erase: Copy‑on‑write を行わず直接操作。
    // ベンチマーク以外で使うと rollback が壊れるので注意。
    void bench_insert(int l,int r){ insert_raw(l,r); }
    void bench_erase(int l,int r){ erase_raw(l,r); }
};

/*==================== Naive (functional‑test only) ====================*/
struct Naive{
    map<int,int> mp;
    /* 低速実装 ― テスト検証用 */
    void insert(int l,int r){
        if(l>=r) return;
        auto it=mp.lower_bound(l);
        if(it!=mp.begin()){
            auto pv=prev(it);
            if(pv->second>=l){ l=pv->first; r=max(r,pv->second); it=mp.erase(pv);}
        }
        while(it!=mp.end()&&it->first<=r){ r=max(r,it->second); it=mp.erase(it);}
        mp[l]=r;
    }
    void erase(int l,int r){
        if(l>=r) return;
        auto it=mp.lower_bound(l);
        if(it!=mp.begin()){
            auto pv=prev(it);
            if(pv->second>l){
                int b=pv->second; mp.erase(pv);
                if(l>pv->first) mp[pv->first]=l;
                if(b>r) mp[r]=b;
            }
        }
        while(it!=mp.end()&&it->first<r){
            int b=it->second; it=mp.erase(it);
            if(b>r) mp[r]=b;
        }
    }
    const pair<const int,int>* find_interval(int x) const{
        auto it=mp.upper_bound(x); if(it==mp.begin()) return nullptr; --it; return (it->second>x)? &*it: nullptr;
    }
    bool contains(int x) const{ return find_interval(x)!=nullptr; }
    const pair<const int,int>* nearest_left(int x) const{
        auto it=mp.upper_bound(x);
        while(it!=mp.begin()){ --it; if(it->second<x) return &*it; }
        return nullptr;
    }
    const pair<const int,int>* nearest_right(int x) const{
        auto it=mp.lower_bound(x); return (it==mp.end())? nullptr:&*it;
    }
    int size() const{return (int)mp.size();}
    long long covered_length() const{ long long s=0; for(auto &kv:mp) s+=kv.second-kv.first; return s; }
    vector<pair<int,int>> intervals() const{ return {mp.begin(),mp.end()}; }
};

/*==================== Functional tests ====================*/
static void compare(const IntervalTreap& tr,const Naive& nv){
    assert(tr.size()==nv.size());
    assert(tr.covered_length()==nv.covered_length());
    vector<pair<int,int>> tv; tr.iterate([&](const IntervalTreap::Node* n){ tv.emplace_back(n->l,n->r); });
    assert(tv==nv.intervals());
}
static void compare_queries(const IntervalTreap& tr,const Naive& nv,int x){
    auto tn=tr.find_interval(x); auto nn=nv.find_interval(x);
    assert(bool(tn)==bool(nn)); if(tn){ assert(tn->l==nn->first && tn->r==nn->second); }
    assert(tr.contains(x)==nv.contains(x));
    auto tl=tr.nearest_left(x);  auto nl=nv.nearest_left(x);
    assert(bool(tl)==bool(nl)); if(tl){ assert(tl->l==nl->first && tl->r==nl->second); }
    auto trr=tr.nearest_right(x); auto nr=nv.nearest_right(x);
    assert(bool(trr)==bool(nr)); if(trr){ assert(trr->l==nr->first && trr->r==nr->second); }
}
static void functional_tests(){
    const int COORD=100000;
    /* basic */
    IntervalTreap tr; Naive nv;
    tr.insert(10,20); nv.insert(10,20);
    tr.insert(20,30); nv.insert(20,30); compare(tr,nv);
    compare_queries(tr,nv,15); compare_queries(tr,nv,5);
    tr.erase(15,18); nv.erase(15,18); compare(tr,nv);
    compare_queries(tr,nv,16); compare_queries(tr,nv,22);
    tr.erase(10,30); nv.erase(10,30); compare(tr,nv);
    compare_queries(tr,nv,11);

    /* scenario */
    tr=IntervalTreap(); nv=Naive();
    tr.insert(0,10); nv.insert(0,10);
    tr.insert(20,30); nv.insert(20,30);
    tr.insert(10,20); nv.insert(10,20); compare(tr,nv);
    compare_queries(tr,nv,5); compare_queries(tr,nv,25);
    tr.erase(5,25); nv.erase(5,25); compare(tr,nv);
    compare_queries(tr,nv,5); compare_queries(tr,nv,25);

    /* random */
    const int OPS=30000;
    tr=IntervalTreap(); nv=Naive();
    vector<tuple<int,int,int>> hist;
    for(int op=0; op<OPS; ++op){
        int k=static_cast<int>(rng(100));
        if(k<50){
            int a=static_cast<int>(rng(COORD));
            int b=static_cast<int>(rng(COORD));
            if(a>b) swap(a,b);
            tr.insert(a,b+1); nv.insert(a,b+1); hist.emplace_back(0,a,b+1);
        }else if(k<90){
            int a=static_cast<int>(rng(COORD));
            int b=static_cast<int>(rng(COORD));
            if(a>b) swap(a,b);
            tr.erase(a,b+1); nv.erase(a,b+1); hist.emplace_back(1,a,b+1);
        }else{
            if(tr.rollback()){
                nv=Naive();
                for(size_t i=0;i<hist.size()-1;++i){
                    auto [tp,l,r]=hist[i];
                    (tp==0)? nv.insert(l,r): nv.erase(l,r);
                }
                hist.pop_back();
            }
            continue;
        }
        compare(tr,nv);
        for(int t=0;t<3;++t){
            int x=static_cast<int>(rng(COORD));
            compare_queries(tr,nv,x);
        }
    }
}

/*==================== Benchmark helpers ====================*/
template<class F>
void bench(const string& name,size_t iterations,F&& fn){
    using namespace std::chrono;
    auto t0=HRClk::now();
    fn();
    auto t1=HRClk::now();
    double total_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1-t0).count();
    cerr<<std::fixed<<setprecision(3);
    cerr<<"[BENCH] "<<name<<" : total "<<total_ms<<" ms"
        <<" | avg "<<(total_ms*1e3/static_cast<double>(iterations))<<" µs/op\n";
}

/*==================== Benchmarks ====================*/
void benchmark_tests(){
    constexpr int COORD=1'000'000;
    constexpr int REP=10;                       // iteration multiplier (10×)
    auto rnd_interval=[&](){
        int a=static_cast<int>(rng(COORD));
        int b=static_cast<int>(rng(COORD));
        if(a>b) swap(a,b);
        return pair<int,int>{a,b+1};
    };

    /* ---- insert 1K ---- */
    {
        vector<pair<int,int>> vec(1000);
        for(auto&pr:vec) pr=rnd_interval();
        IntervalTreap tr;
        bench("insert 1K intervals",vec.size()*REP,[&](){
            for(int rep=0; rep<REP; ++rep)
                for(auto [l,r]:vec) tr.bench_insert(l,r);
        });
    }
    /* ---- insert 1M ---- */
    {
        const size_t N=1'000'000;
        vector<pair<int,int>> vec(N); for(auto&pr:vec) pr=rnd_interval();
        IntervalTreap tr;
        bench("insert 1M intervals",vec.size()*REP,[&](){
            for(int rep=0; rep<REP; ++rep)
                for(auto [l,r]:vec) tr.bench_insert(l,r);
        });
    }
    /* ---- mixed ops 1K state ---- */
    {
        IntervalTreap tr;
        for(int i=0;i<1000;++i){ auto [l,r]=rnd_interval(); tr.bench_insert(l,r);}
        const int OPS=10'000;
        bench("1K state mixed insert/erase 10K",OPS*REP,[&](){
            for(int rep=0; rep<REP; ++rep)
                for(int i=0;i<OPS;++i){
                    auto [l,r]=rnd_interval();
                    (i&1)? tr.bench_erase(l,r): tr.bench_insert(l,r);
                }
        });
    }
    /* ---- mixed ops 1M state ---- */
    {
        IntervalTreap tr;
        for(int i=0;i<1'000'000;++i){ auto [l,r]=rnd_interval(); tr.bench_insert(l,r);}
        const int OPS=100'000;
        bench("1M state mixed insert/erase 100K",OPS*REP,[&](){
            for(int rep=0; rep<REP; ++rep)
                for(int i=0;i<OPS;++i){
                    auto [l,r]=rnd_interval();
                    (i&1)? tr.bench_erase(l,r): tr.bench_insert(l,r);
                }
        });
    }
    /* ---- queries 1K state ---- */
    {
        IntervalTreap tr;
        for(int i=0;i<1000;++i){ auto [l,r]=rnd_interval(); tr.bench_insert(l,r);}
        const int Q=10'000;
        bench("1K state find_interval",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.find_interval(static_cast<int>(rng(COORD))); });
        bench("1K state contains",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.contains(static_cast<int>(rng(COORD))); });
        bench("1K state nearest_left",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.nearest_left(static_cast<int>(rng(COORD))); });
        bench("1K state nearest_right",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.nearest_right(static_cast<int>(rng(COORD))); });
    }
    /* ---- queries 1M state ---- */
    {
        IntervalTreap tr;
        for(int i=0;i<1'000'000;++i){ auto [l,r]=rnd_interval(); tr.bench_insert(l,r);}
        const int Q=100'000;
        bench("1M state find_interval",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.find_interval(static_cast<int>(rng(COORD))); });
        bench("1M state contains",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.contains(static_cast<int>(rng(COORD))); });
        bench("1M state nearest_left",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.nearest_left(static_cast<int>(rng(COORD))); });
        bench("1M state nearest_right",Q*REP,[&](){ for(int rep=0; rep<REP; ++rep) for(int i=0;i<Q;++i) tr.nearest_right(static_cast<int>(rng(COORD))); });
    }
    /* ---- iterate 1K & 1M ---- */
    {
        IntervalTreap tr1,tr2; long long dummy=0;
        for(int i=0;i<1000;++i){ auto [l,r]=rnd_interval(); tr1.bench_insert(l,r);}
        for(int i=0;i<1'000'000;++i){ auto [l,r]=rnd_interval(); tr2.bench_insert(l,r);}
        bench("iterate 1K",REP,[&](){ for(int rep=0; rep<REP; ++rep) tr1.iterate([&](const auto*n){ dummy+=n->len; }); });
        bench("iterate 1M",REP,[&](){ for(int rep=0; rep<REP; ++rep) tr2.iterate([&](const auto*n){ dummy+=n->len; }); });
    }
}

/*==================== main ====================*/
int main(){
    ios::sync_with_stdio(false); cin.tie(nullptr);

    cerr<<"[TEST] functional tests...\n";
    functional_tests();
    cerr<<"  all functional tests passed\n\n";

    cerr<<"[BENCH] start benchmarks...\n";
    benchmark_tests();
    return 0;
}


// 実行結果（ローカル）
// [TEST] functional tests...
//   all functional tests passed

// [BENCH] start benchmarks...
// [BENCH] insert 1K intervals : total 0.890 ms | avg 0.089 µs/op
// [BENCH] insert 1M intervals : total 1893.466 ms | avg 0.189 µs/op
// [BENCH] 1K state mixed insert/erase 10K : total 58.600 ms | avg 0.586 µs/op
// [BENCH] 1M state mixed insert/erase 100K : total 1470.648 ms | avg 1.471 µs/op
// [BENCH] 1K state find_interval : total 0.131 ms | avg 0.001 µs/op
// [BENCH] 1K state contains : total 0.101 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_left : total 0.113 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_right : total 0.119 ms | avg 0.001 µs/op
// [BENCH] 1M state find_interval : total 1.223 ms | avg 0.001 µs/op
// [BENCH] 1M state contains : total 1.235 ms | avg 0.001 µs/op
// [BENCH] 1M state nearest_left : total 1.234 ms | avg 0.001 µs/op
// [BENCH] 1M state nearest_right : total 1.052 ms | avg 0.001 µs/op
// [BENCH] iterate 1K : total 0.001 ms | avg 0.103 µs/op
// [BENCH] iterate 1M : total 0.000 ms | avg 0.025 µs/op


// 実行結果（AtCoder 途中まで）
// [TEST] functional tests...
//   all functional tests passed

// [BENCH] start benchmarks...
// [BENCH] insert 1K intervals : total 1.247 ms | avg 0.125 µs/op
// [BENCH] insert 1M intervals : total 1301.251 ms | avg 0.130 µs/op
// [BENCH] 1K state mixed insert/erase 10K : total 73.409 ms | avg 0.734 µs/op
// [BENCH] 1M state mixed insert/erase 100K : total 916.415 ms | avg 0.916 µs/op
// [BENCH] 1K state find_interval : total 0.111 ms | avg 0.001 µs/op
// [BENCH] 1K state contains : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_left : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1K state nearest_right : total 0.100 ms | avg 0.001 µs/op
// [BENCH] 1M state find_interval : total 1.026 ms | avg 0.001 µs/op
// [BENCH] 1M state contains : total 1.018 ms | avg 0.001 µs/op
// [BENCH] 1M state nearest_left : total 1.009 ms | avg 0.001 µs/op
// [BENCH] 1M state nearest_right : total 1.000 ms | avg 0.001 µs/op
