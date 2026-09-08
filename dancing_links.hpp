#pragma once
/*
  ---------------------------------------------------------------------------
  Dancing Links / Exact Cover  ヘッダ専用ライブラリ
  ---------------------------------------------------------------------------
  - C++20 (gcc 12.2) でそのままインクルード可能
  - 旧 gkaranikas/dancing-links 由来の API を保持しつつ整理・近代化
  - 名前空間を単一の `dancing_links` に統一
  - 主要 API に日本語ドキュメントを付与
  - シングルスレッド前提（排他制御なし）
  ---------------------------------------------------------------------------
*/
#include <vector>
#include <stack>
#include <iostream>
#include <cassert>

namespace dancing_links {   //================================================================

/* ------------------------------------------------------------------------
   汎用ノード (左右上下の双方向リンク + 任意データ)
   * テンプレート T で保持データ型を指定
   * すべてのリンク操作は O(1)
   * 外部からリンクを直接触るのは非推奨
--------------------------------------------------------------------------*/
template<class T>
class MNode_t {
public:
    MNode_t(const T& d = {}, MNode_t* r = nullptr, MNode_t* l = nullptr,
            MNode_t* u = nullptr, MNode_t* dwn = nullptr)
        : _data(d), right_link(r), left_link(l), up_link(u), down_link(dwn) {}

    // --- アクセサ ---
    void   set_data (const T& d) { _data = d; }
    T&     data     () const     { return const_cast<T&>(_data); } // 旧 API 互換で非 const
    MNode_t* right() const { return right_link; }
    MNode_t* left () const { return left_link;  }
    MNode_t* up   () const { return up_link;    }
    MNode_t* down () const { return down_link;  }

    // --- リンク操作 : いずれも O(1) ---
    void set_right(MNode_t* x){ right_link = x; }
    void set_left (MNode_t* x){ left_link  = x; }
    void set_up   (MNode_t* x){ up_link    = x; }
    void set_down (MNode_t* x){ down_link  = x; }

private:
    T         _data;
    MNode_t  *right_link {}, *left_link {}, *up_link {}, *down_link {};
};

/* ヘルパ : 水平 / 垂直に 2 ノードを双方向連結（O(1)） */
template<class T> inline void join_lr(MNode_t<T>* a, MNode_t<T>* b){ a->set_right(b); b->set_left(a); }
template<class T> inline void join_du(MNode_t<T>* a, MNode_t<T>* b){ a->set_up(b);   b->set_down(a); }

/* ------------------------------------------------------------------------
   行列位置データ : ノードが属する (行番号, 列ヘッダ) を保持
   * 列ヘッダ自身は row_id == -1
--------------------------------------------------------------------------*/
class Column;                    // 前方宣言
class MData {
public:
    MData(int r = -1, Column* c = nullptr): row_id(r), column_id(c) {}
    int     row_id;     // 行番号（列ヘッダの場合 -1）
    Column* column_id;  // 所属列ヘッダ
};

using MNode = MNode_t<MData>;    // “通常ノード” の別名

/* ------------------------------------------------------------------------
   列ヘッダノード : Column
   * size() で列中の要素数にアクセス
   * Row 操作は LMatrix 経由で自動更新されるため直接変更は非推奨
--------------------------------------------------------------------------*/
class Column : public MNode {
public:
    Column(int sz = 0): MNode(MData(-1, this)), _size(sz) {}

    /* 列の要素数を返す（O(1)）*/
    int  size()               const { return _size; }

    /* ライブラリ内部用：要素数を直接設定（O(1)）  
       - API としては公開するが通常は呼ばないこと */
    void set_size(int n)            { _size = n; }

    /* ライブラリ内部用：要素数の増減（O(1)） */
    void add_to_size(int n)         { _size += n; }
private:
    int _size;   // 列サイズ
};

/* ------------------------------------------------------------------------
   LMatrix : Dancing Links アルゴリズム用リンクド行列
   ------------------------------------------------------------------------
   使い方:
     1. bool** から変換 or 空行列生成  
        LMatrix A(bool_matrix, m, n);
     2. solve_exact_cover(A) で厳密被覆を解く  
     3. 行や列を動的に削除・復元したい場合は remove_* / restore_* を使用
   処理量:
     - 行/列の削除・復元はリンク張替えのみで O(k) (k は削除対象ノード数)
     - 生成は O(mn)  (true 要素数に比例; スパースでも高速)
   制約・注意:
     - remove_* したら必ず restore_* で元に戻すこと（順序は FILO）
     - bool** 行列のメモリ管理は呼び出し側
     - デバッグ目的以外で DEBUG_display は重いので呼び出し過多に注意
--------------------------------------------------------------------------*/
class LMatrix {
public:
    /* 空行列を作成 */
    LMatrix(){
        root = new MNode(MData());
        join_lr(root, root);              // root の左右を自己ループ
        row_count = 0;
    }

    /* bool 行列を LMatrix に変換  
       A[i][j] == true なら 1, false なら 0  
       - 行数 m, 列数 n は必ず指定
    */
    LMatrix(bool** A, int m, int n){
        root = new MNode(MData());
        join_lr(root, root);      // <-- ★ 追加: 常に自己ループを張る

        if(m==0 || n==0){ row_count = 0; return; }

        /* --- 列ヘッダ生成 (O(n)) --- */
        MNode* c = new Column(0); join_lr(root, c);
        for(int j=1;j<n;++j){ join_lr(c, new Column(0)); c = c->right(); }
        join_lr(c, root);                 // 循環

        /* --- ノード生成（縦リンク, O(#1 の数)） --- */
        std::vector<std::vector<MNode*>> ptr(m, std::vector<MNode*>(n,nullptr));
        for(int j=0;j<n;++j){
            auto* col = static_cast<Column*>(root->right());
            for(int k=0;k<j;++k) col = static_cast<Column*>(col->right());
            MNode* last = col;
            for(int i=0;i<m;++i){
                if(A[i][j]){
                    ptr[i][j] = new MNode(MData(i, col));
                    join_du(ptr[i][j], last);
                    last = ptr[i][j];
                    col->add_to_size(1);
                }
            }
            join_du(col, last);           // 列を環状に
        }

        /* --- 最終非ゼロ行判定 (O(mn) だが行末から早期打切り) --- */
        int lastNonZero=-1;
        for(int i=m-1;i>=0 && lastNonZero==-1;--i)
            for(int j=0;j<n;++j) if(A[i][j]){ lastNonZero=i; break; }
        row_count = lastNonZero + 1;

        /* --- 横リンク (O(#1 の数)) --- */
        for(int i=0;i<row_count;++i){
            MNode *first=nullptr,*prev=nullptr;
            for(int j=0;j<n;++j){
                if(!ptr[i][j]) continue;
                if(!first) first=ptr[i][j];
                else       join_lr(prev, ptr[i][j]);
                prev=ptr[i][j];
            }
            if(first) join_lr(prev, first);   // 行を環状に
        }
    }

    /* デストラクタ : 全ノード手動解放 (O(#ノード)) */
    ~LMatrix(){
        for(MNode* col=root->right(); col!=root; ){
            for(MNode* n=col->down(); n!=col; ){
                MNode* del=n; n=n->down(); delete del;
            }
            MNode* del=col; col=col->right(); delete del;
        }
        delete root;
    }

    /* ------------------------------------------------------------------
       head()
       - 行列の “ルートノード” を返す。列ヘッダは head()->right() から巡回
       - アルゴリズム拡張時に内部構造へアクセスしたい場合のみ使用
       - 返り値のノードを削除・変更しないこと
    ------------------------------------------------------------------*/
    MNode* head() const { return root; }

    /* 行列が空 (列ヘッダが無い) かを判定 (O(1)) */
    bool is_trivial() const { return root->right()==root; }

    /* 行数の上界 (= 最大列サイズ) を返す。全探索用の配列確保などに便利 (O(#列)) */
    int  number_of_rows() const {
        int mx=0;
        for(auto* c=root->right(); c!=root; c=c->right())
            mx = std::max(mx, static_cast<Column*>(c)->size());
        return mx;
    }

    /* ------------------------------------------------------------------
       remove_row(p)
       - ノード p の属する行を *行リストから* 切り離す  
       - 各列ヘッダの size() が 1 行分減る  
       - 計算量 O(k) (k = 行のノード数)
       制約:
         * p は列ヘッダでも root でも nullptr でもないこと
         * 同一行を 2 回以上 remove しない
       使用上の注意:
         * 削除後に必ず restore_row を呼ぶこと（FILO）
    ------------------------------------------------------------------*/
    void remove_row(MNode* p){
        if(!p || p==root || p->data().column_id==p) return;
        for(MNode* k=p;;k=k->right()){
            join_du(k->down(), k->up());
            k->data().column_id->add_to_size(-1);
            if(k->right()==p) break;
        }
    }

    /* restore_row(p)
       - 直前の remove_row で切り離した行を元に戻す (O(k))
       - remove_row と同じ p を渡すこと
    */
    void restore_row(MNode* p){
        for(MNode* k=p;;k=k->left()){
            k->up()->set_down(k);
            k->down()->set_up(k);
            k->data().column_id->add_to_size(1);
            if(k->left()==p) break;
        }
    }

    /* remove_column / restore_column : 列版 (行と同様の制約) */
    void remove_column(MNode* p){
        if(!p || p==root) return;
        for(MNode* k=p;;k=k->up()){
            join_lr(k->left(), k->right());
            if(k->up()==p) break;
        }
    }
    void restore_column(MNode* p){
        for(MNode* k=p;;k=k->down()){
            k->right()->set_left(k);
            k->left()->set_right(k);
            if(k->down()==p) break;
        }
    }

    /* DEBUG_display : 行列の可視化  
       - 複雑なバグの切り分けに便利  
       - アサートが多数入っているためリリースコードでは重い
    */
    void DEBUG_display(std::ostream& os = std::cout) const {
        os << "ROW DIAGRAM\n\n\t>H<";
        for(auto* c=root->right(); c!=root; c=c->right()) os << ">C<";
        os << "\trow -1\n";
        for(int i=0;i<row_count;++i){
            os << "\t   ";
            for(auto* col=root->right(); col!=root; col=col->right()){
                bool printed=false;
                for(auto* n=col->down(); n!=col; n=n->down())
                    if(n->data().row_id==i){ os<<">N<"; printed=true; break; }
                if(!printed) os<<"   ";
            }
            os << "\trow " << i << '\n';
        }
        os << "\nCOLUMN SIZES\n\n\t";
        for(auto* c=root->right(); c!=root; c=c->right())
            os << '>' << static_cast<Column*>(c)->size() << '<';
        os << '\n';
    }

private:
    MNode* root {nullptr};
    int    row_count {};
};
std::ostream& operator<<(std::ostream& os, const LMatrix& t) { os << "[LMatrix]" << std::endl; t.DEBUG_display(os); return os; }


/* ------------------------------------------------------------------------
   型エイリアス / 内部で使うヘルパ構造体
--------------------------------------------------------------------------*/
using S_Stack = std::vector<int>;   // 解（行番号のスタック）

enum class RC { row, column };     // 行/列の削除種別
struct RC_Item { MNode* node; RC type; };
using RC_Stack = std::stack<RC_Item>;   // 1 回の update が積む削除記録
using H_Stack  = std::stack<RC_Stack>;  // 再帰深さごとの履歴

/* ----------------------- 先行宣言 (内部関数) ------------------------- */
inline Column* choose_column(LMatrix&);
inline void    update   (LMatrix&, S_Stack&, H_Stack&, MNode*);
inline void    downdate (LMatrix&, S_Stack&, H_Stack&);
inline bool    DLX      (LMatrix&, S_Stack&, H_Stack&);

/* ------------------------------------------------------------------------
   solve_exact_cover(bool** matrix, int m, int n)
   solve_exact_cover(LMatrix& M)
   ------------------------------------------------------------------------
   使い方:
     - (1) 生の bool 行列を与える  
         auto sol = dancing_links::solve_exact_cover(mat, m, n);
     - (2) 既に LMatrix へ変換済みなら  
         auto sol = dancing_links::solve_exact_cover(matrix);

   返り値:
     - 解の行番号リスト（辞書順ではない）。空なら解なし

   処理量:
     - アルゴリズム X は最悪指数時間。  
       ただし Knuth の列最小ヒューリスティックで実用的には高速

   制約・注意:
     - bool** 版では matrix[i][j] が 0/1 を保証  
     - マルチスレッドで同じ LMatrix を同時に触らない
--------------------------------------------------------------------------*/
inline std::vector<int> solve_exact_cover(bool** mat, int m, int n){
    LMatrix M(mat,m,n);
    H_Stack hist;
    std::vector<int> sol; sol.reserve(m);
    DLX(M,sol,hist);
    sol.shrink_to_fit();
    return sol;
}
inline std::vector<int> solve_exact_cover(LMatrix& M){
    H_Stack hist;
    std::vector<int> sol; sol.reserve(M.number_of_rows());
    DLX(M,sol,hist);
    sol.shrink_to_fit();
    return sol;
}

/* ------------------------------------------------------------------------
   choose_column(M)
   - 列サイズが最も小さい列ヘッダを返す（ヒューリスティック）
   - 行列が空なら nullptr
   - 計算量 O(#列)
--------------------------------------------------------------------------*/
inline Column* choose_column(LMatrix& M){
    if(M.is_trivial()) return nullptr;
    auto* best = static_cast<Column*>(M.head()->right());
    for(auto* c = best; c != M.head(); c = static_cast<Column*>(c->right()))
        if(c->size() < best->size()) best = c;
    return best;
}

/* ------------------------------------------------------------------------
   update(M, sol, hist, r)
   - 行 r を選択し、アルゴリズム X の次状態へ更新  
   - 影響する行/列を remove_* し、履歴を hist に積む
   - 計算量 O(Σ (行・列の要素数))
   - remove と restore をペアに保つため必ず downdate を呼ぶ
--------------------------------------------------------------------------*/
inline void update(LMatrix& M, S_Stack& sol, H_Stack& hist, MNode* r){
    sol.push_back(r->data().row_id);
    RC_Stack stk;

    for(MNode* i=r->right(); i!=r; i=i->right()){
        for(MNode* j=i->up(); j!=i; j=j->up()){
            if(j->data().column_id==j) continue;   // 列ヘッダはスキップ
            M.remove_row(j);
            stk.push({j,RC::row});
        }
        M.remove_column(i);
        stk.push({i,RC::column});
    }
    for(MNode* j=r->up(); j!=r; j=j->up()){
        if(j->data().column_id==j) continue;
        M.remove_row(j);
        stk.push({j,RC::row});
    }
    M.remove_column(r);
    stk.push({r,RC::column});

    hist.push(stk);
}

/* ------------------------------------------------------------------------
   downdate(M, sol, hist)
   - 直前の update を完全に巻き戻す  
   - update / downdate は必ずペアで呼ぶ
--------------------------------------------------------------------------*/
inline void downdate(LMatrix& M, S_Stack& sol, H_Stack& hist){
    if(hist.empty()) return;
    sol.pop_back();
    auto stk = hist.top(); hist.pop();
    while(!stk.empty()){
        auto it = stk.top(); stk.pop();
        if(it.type==RC::row) M.restore_row(it.node);
        else                 M.restore_column(it.node);
    }
}

/* ------------------------------------------------------------------------
   DLX(M, sol, hist)  ― アルゴリズム X + Dancing Links 再帰
   - 内部呼び出し専用だが旧 API 互換のため公開
   - 解が見つかると true, 無いと false
--------------------------------------------------------------------------*/
inline bool DLX(LMatrix& M, S_Stack& sol, H_Stack& hist){
    Column* c = choose_column(M);
    if(!c) return true;          // 列が無ければ解

    for(MNode* r=c->down(); r!=c; r=r->down()){
        update(M,sol,hist,r);
        if(DLX(M,sol,hist)) return true;
        downdate(M,sol,hist);
    }
    return false;                // 解無し
}

/*===============================================================
  bool** ⇔ vector<vector<bool>> 変換ヘルパ
===============================================================*/
bool** vec2raw(const std::vector<std::vector<bool>>& v){
    const int m = static_cast<int>(v.size());
    const int n = (m ? static_cast<int>(v[0].size()) : 0);

    bool** a = new bool*[m];
    for(int i=0;i<m;++i){
        a[i] = new bool[n];
        for(int j=0;j<n;++j) a[i][j] = v[i][j];
    }
    return a;
}
void free_raw(bool** a,int m){
    for(int i=0;i<m;++i) delete[] a[i];
    delete[] a;
}

} // namespace dancing_links
