#pragma once
#include "template.hpp"

/*
  高効率 Link‑Cut Tree（Splay ベース）
  ------------------------------------
  ・パス上のモノイド集約値を動的森 (Dynamic Tree) で管理
  ・TreeDPInfo で「頂点の値型 Info」「パスの集約型 Path」「圧縮関数
  compress」を差し替え可 ・以下では例として「Path＝和 / Info＝long
  long」を定義（IntSumInfo）
*/

/*======================================================================
  汎用 Link‑Cut Tree
======================================================================*/
template <class TreeDPInfo>
struct LinkCutTree {
    using Path = typename TreeDPInfo::Path;
    using Info = typename TreeDPInfo::Info;

    /* ---------------- 内部ノード構造 ---------------- */
    struct Node {
        Info info;        // 頂点自身の値
        Node *l, *r, *p;  // splay 子左右, 親
        Path sum, mus;    // ★sum: 部分木(根→葉)順, ★mus: 逆順
        bool rev;         // 実パス反転フラグ
        explicit Node(const Info& v)
            : info(v),
              l(nullptr),
              r(nullptr),
              p(nullptr),
              sum(TreeDPInfo::vertex(v)),
              mus(TreeDPInfo::vertex(v)),
              rev(false) {}
        bool is_root() const { return !p || (p->l != this && p->r != this); }
    };
    using NP = Node*;

    /* ---------------- 内部ユーティリティ（トグル・回転等） ---------------- */
    static void toggle(NP t) {
        swap(t->l, t->r);
        swap(t->sum, t->mus);
        t->rev ^= 1;
    }
    static void push(NP t) {
        if (!t || !t->rev) return;
        if (t->l) toggle(t->l);
        if (t->r) toggle(t->r);
        t->rev = false;
    }
    static void update(NP t) {
        Path key = TreeDPInfo::vertex(t->info);
        t->sum = t->mus = key;
        if (t->l) {
            t->sum = TreeDPInfo::compress(t->l->sum, t->sum);
            t->mus = TreeDPInfo::compress(t->mus, t->l->mus);
        }
        if (t->r) {
            t->sum = TreeDPInfo::compress(t->sum, t->r->sum);
            t->mus = TreeDPInfo::compress(t->r->mus, t->mus);
        }
    }
    static void rotr(NP t) {
        NP x = t->p, y = x->p;
        push(x);
        push(t);
        if ((x->l = t->r)) t->r->p = x;
        t->r = x;
        x->p = t;
        update(x);
        update(t);
        if ((t->p = y)) {
            if (y->l == x) y->l = t;
            if (y->r == x) y->r = t;
        }
    }
    static void rotl(NP t) {
        NP x = t->p, y = x->p;
        push(x);
        push(t);
        if ((x->r = t->l)) t->l->p = x;
        t->l = x;
        x->p = t;
        update(x);
        update(t);
        if ((t->p = y)) {
            if (y->l == x) y->l = t;
            if (y->r == x) y->r = t;
        }
    }
    static void splay(NP t) {
        push(t);
        while (!t->is_root()) {
            NP q = t->p;
            if (q->is_root()) {
                push(q);
                push(t);
                (q->l == t) ? rotr(t) : rotl(t);
            } else {
                NP r = q->p;
                push(r);
                push(q);
                push(t);
                if (r->l == q) {
                    (q->l == t) ? (rotr(q), rotr(t)) : (rotl(t), rotr(t));
                } else {
                    (q->r == t) ? (rotl(q), rotl(t)) : (rotr(t), rotl(t));
                }
            }
        }
    }
    static NP expose(NP t) {
        NP rightPath = nullptr;
        for (NP cur = t; cur; cur = cur->p) {
            splay(cur);
            cur->r = rightPath;
            update(cur);
            rightPath = cur;
        }
        splay(t);
        return rightPath;
    }

    /*------------------------------------------------------------------
      NP alloc(const Info& v)
        頂点を 1 つ動的生成し、そのポインタを返す
        ・グラフがオンラインで拡張される場合に使用
        ・返り値 NP を後続の API へ渡して操作する
    ------------------------------------------------------------------*/
    NP alloc(const Info& v) { return new Node(v); }

    /*------------------------------------------------------------------
      vector<NP> build(const vector<Info>& vs)
        Info 配列から頂点群を一括生成
        ・静的入力で初期化を簡潔に書きたいときに便利
        ・戻り値は各頂点のポインタを並べた vector
    ------------------------------------------------------------------*/
    vector<NP> build(const vector<Info>& vs) {
        vector<NP> res;
        res.reserve(vs.size());
        for (auto& x : vs) res.push_back(alloc(x));
        return res;
    }

    /*------------------------------------------------------------------
      void link(child, parent)
        child を parent の子にし、2 つの木を辺で連結する
        【前提条件】
          1. child が現在その連結成分の根であること
          2. child, parent が異なる連結成分 (is_connected()==false)
        【失敗時】 runtime_error 例外
        【典型用途】
          ・動的森で新しい辺を追加 (オンライングラフ / MST on‑line など)
    ------------------------------------------------------------------*/
    void link(NP child, NP parent) {
        if (is_connected(child, parent))
            throw runtime_error("already connected");
        expose(child);  // child を preferred‑path の根に
        if (child->l) throw runtime_error("child is not root");
        expose(parent);
        child->p = parent;
        parent->r = child;
        update(parent);
    }

    /*------------------------------------------------------------------
      void cut(child)
        child とその親との間の辺を切断する
        【前提条件】
          child は連結成分の根ではない (親が存在する)
        【失敗時】 runtime_error 例外
        【典型用途】
          ・動的 MST で重い辺を除去
          ・「辺を削除する」クエリが来たとき
    ------------------------------------------------------------------*/
    void cut(NP child) {
        expose(child);
        NP par = child->l;
        if (!par) throw runtime_error("child is root");
        child->l = nullptr;
        par->p = nullptr;
        update(child);
    }

    /*------------------------------------------------------------------
      void evert(v)
        連結成分内で v を新しい根にする（実パス方向を反転）
        【典型用途】
          ・パス u‑v を扱う前に 「u を根」 に統一したい場合
          ・根側からの距離 / 集約を楽に取得
    ------------------------------------------------------------------*/
    void evert(NP v) {
        expose(v);
        toggle(v);
        push(v);
    }

    /*------------------------------------------------------------------
      bool is_connected(u, v)
        u, v が同じ木に属しているかを判定
        【計算量】O(log N)
        【用途】
          ・link する前に接続チェック
          ・クエリ対象が同一木か確認
    ------------------------------------------------------------------*/
    bool is_connected(NP u, NP v) {
        expose(u);
        expose(v);
        return u == v || u->p;
    }

    /*------------------------------------------------------------------
      NP lca(u, v)
        u, v が接続されていれば最近共通祖先 (Lowest Common Ancestor) を返す
        接続されていなければ nullptr
        【用途】
          ・動的木上で距離や重みをパス分割して求める
    ------------------------------------------------------------------*/
    NP lca(NP u, NP v) {
        if (!is_connected(u, v)) return nullptr;
        expose(u);
        return expose(v);  // expose の戻り値が LCA
    }

    /*------------------------------------------------------------------
      void set_key(v, info)
        頂点 v の保持値を info に変更する（点更新）
        【用途】
          ・重み変更, 値の書き換えによるパス集約の即時反映
    ------------------------------------------------------------------*/
    void set_key(NP v, const Info& info) {
        expose(v);
        v->info = info;
        update(v);
    }

    /*------------------------------------------------------------------
      const Path& query_path(u)
        根 → u までのパス集約値 (sum) を返す
        【用途】
          ・木の根を基準にした距離 / コスト計算
    ------------------------------------------------------------------*/
    const Path& query_path(NP u) {
        expose(u);
        return u->sum;
    }

    /*------------------------------------------------------------------
      const Path& query_path(u, v)
        u‑v パスの集約値を返す
        実装：evert(u) して「根→v」の形に帰着
        【用途】
          ・パス和 / パス最小値 など動的パスクエリ
    ------------------------------------------------------------------*/
    const Path& query_path(NP u, NP v) {
        evert(u);
        return query_path(v);
    }

    /*------------------------------------------------------------------
      pair<NP,Path> find_first(u, check)
        根 → u パスを根側から順にたどり、累積 Path が
        check(累積) == true となる最初の頂点を返す
        返り値.first が nullptr の場合は該当なし
        【用途例】
          ・累積距離 >= L になる最初の点
          ・パス上で重みが 0 でなくなる最初の頂点 など
    ------------------------------------------------------------------*/
    template <class F>
    pair<NP, Path> find_first(NP u, const F& check) {
        expose(u);
        Path acc = TreeDPInfo::vertex(u->info);
        if (check(acc)) return {u, acc};
        u = u->l;
        while (u) {
            push(u);
            if (u->r) {
                Path nxt = TreeDPInfo::compress(u->r->sum, acc);
                if (check(nxt)) {
                    u = u->r;
                    continue;
                }
                acc = nxt;
            }
            Path nxt = TreeDPInfo::compress(TreeDPInfo::vertex(u->info), acc);
            if (check(nxt)) {
                splay(u);
                return {u, nxt};
            }
            acc = nxt;
            u = u->l;
        }
        return {nullptr, acc};
    }
};

/*
struct TreeDPInfo {
  struct Path {};
  struct Info {};
  static Path vertex(const Info& u) {}
  static Path compress(const Path& p, const Path& c) {}
};
*/