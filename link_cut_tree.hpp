#pragma once
#include "template.hpp"

template <typename Tree, typename Node, typename T, T (*f)(T, T), T (*ts)(T)>
struct ReversibleBBST : Tree {
  using Tree::merge;
  using Tree::split;
  using typename Tree::Ptr;

  ReversibleBBST() = default;

  virtual void toggle(Ptr t) {
    if(!t) return;
    swap(t->l, t->r);
    t->sum = ts(t->sum);
    t->rev ^= true;
  }

  T fold(Ptr &t, int a, int b) {
    auto x = split(t, a);
    auto y = split(x.second, b - a);
    auto ret = sum(y.first);
    t = merge(x.first, merge(y.first, y.second));
    return ret;
  }

  void reverse(Ptr &t, int a, int b) {
    auto x = split(t, a);
    auto y = split(x.second, b - a);
    toggle(y.first);
    t = merge(x.first, merge(y.first, y.second));
  }

  Ptr update(Ptr t) override {
    if (!t) return t;
    t->cnt = 1;
    t->sum = t->key;
    if (t->l) t->cnt += t->l->cnt, t->sum = f(t->l->sum, t->sum);
    if (t->r) t->cnt += t->r->cnt, t->sum = f(t->sum, t->r->sum);
    return t;
  }

 protected:
  inline T sum(const Ptr t) { return t ? t->sum : T(); }

  void push(Ptr t) override {
    if (!t) return;
    if (t->rev) {
      if (t->l) toggle(t->l);
      if (t->r) toggle(t->r);
      t->rev = false;
    }
  }
};

template <typename Node>
struct SplayTreeBase {
  using Ptr = Node *;
  template <typename... Args>
  Ptr my_new(const Args &...args) {
    return new Node(args...);
  }
  void my_del(Ptr p) { delete p; }

  bool is_root(Ptr t) { return !(t->p) || (t->p->l != t && t->p->r != t); }

  int size(Ptr t) const { return count(t); }

  virtual void splay(Ptr t) {
    if (!t) return;
    push(t);
    while (!is_root(t)) {
      Ptr q = t->p;
      if (is_root(q)) {
        push(q), push(t);
        rot(t);
      } else {
        Ptr r = q->p;
        push(r), push(q), push(t);
        if (pos(q) == pos(t))
          rot(q), rot(t);
        else
          rot(t), rot(t);
      }
    }
  }

  Ptr get_left(Ptr t) {
    while (t->l) push(t), t = t->l;
    return t;
  }

  Ptr get_right(Ptr t) {
    while (t->r) push(t), t = t->r;
    return t;
  }

  pair<Ptr, Ptr> split(Ptr t, int k) {
    if (!t) return {nullptr, nullptr};
    if (k == 0) return {nullptr, t};
    if (k == count(t)) return {t, nullptr};
    push(t);
    if (k <= count(t->l)) {
      auto x = split(t->l, k);
      t->l = x.second;
      t->p = nullptr;
      if (x.second) x.second->p = t;
      return {x.first, update(t)};
    } else {
      auto x = split(t->r, k - count(t->l) - 1);
      t->r = x.first;
      t->p = nullptr;
      if (x.first) x.first->p = t;
      return {update(t), x.second};
    }
  }

  Ptr merge(Ptr l, Ptr r) {
    if (!l && !r) return nullptr;
    if (!l) return splay(r), r;
    if (!r) return splay(l), l;
    splay(l), splay(r);
    l = get_right(l);
    splay(l);
    l->r = r;
    r->p = l;
    update(l);
    return l;
  }

  using Key = decltype(Node::key);
  Ptr build(const vector<Key> &v) { return build(0, v.size(), v); }
  Ptr build(int l, int r, const vector<Key> &v) {
    if (l == r) return nullptr;
    if (l + 1 == r) return my_new(v[l]);
    return merge(build(l, (l + r) >> 1, v), build((l + r) >> 1, r, v));
  }

  template <typename... Args>
  void insert(Ptr &t, int k, const Args &...args) {
    splay(t);
    auto x = split(t, k);
    t = merge(merge(x.first, my_new(args...)), x.second);
  }

  void erase(Ptr &t, int k) {
    splay(t);
    auto x = split(t, k);
    auto y = split(x.second, 1);
    my_del(y.first);
    t = merge(x.first, y.second);
  }

  virtual Ptr update(Ptr t) = 0;

 protected:
  inline int count(Ptr t) const { return t ? t->cnt : 0; }

  virtual void push(Ptr t) = 0;

  Ptr build(const vector<Ptr> &v) { return build(0, v.size(), v); }

  Ptr build(int l, int r, const vector<Ptr> &v) {
    if (l + 1 >= r) return v[l];
    return merge(build(l, (l + r) >> 1, v), build((l + r) >> 1, r, v));
  }

  inline int pos(Ptr t) {
    if (t->p) {
      if (t->p->l == t) return -1;
      if (t->p->r == t) return 1;
    }
    return 0;
  }

  virtual void rot(Ptr t) {
    Ptr x = t->p, y = x->p;
    if (pos(t) == -1) {
      if ((x->l = t->r)) t->r->p = x;
      t->r = x, x->p = t;
    } else {
      if ((x->r = t->l)) t->l->p = x;
      t->l = x, x->p = t;
    }
    update(x), update(t);
    if ((t->p = y)) {
      if (y->l == x) y->l = t;
      if (y->r == x) y->r = t;
    }
  }
};

template <typename T>
struct ReversibleSplayTreeNode {
  using Ptr = ReversibleSplayTreeNode *;
  Ptr l, r, p;
  T key, sum;
  int cnt;
  bool rev;

  ReversibleSplayTreeNode(const T &t = T())
      : l(), r(), p(), key(t), sum(t), cnt(1), rev(false) {}
};

template <typename T, T (*f)(T, T), T (*ts)(T)>
struct ReversibleSplayTree
    : ReversibleBBST<SplayTreeBase<ReversibleSplayTreeNode<T>>,
                     ReversibleSplayTreeNode<T>, T, f, ts> {
  using Node = ReversibleSplayTreeNode<T>;
};

template <typename Splay>
struct LinkCutBase : Splay {
  using Node = typename Splay::Node;
  using Ptr = Node*;

  virtual Ptr expose(Ptr t) {
    Ptr rp = nullptr;
    for (Ptr cur = t; cur; cur = cur->p) {
      this->splay(cur);
      cur->r = rp;
      this->update(cur);
      rp = cur;
    }
    this->splay(t);
    return rp;
  }

  virtual void link(Ptr u, Ptr v) {
    evert(u);
    expose(v);
    u->p = v;
  }

  void cut(Ptr u, Ptr v) {
    evert(u);
    expose(v);
    assert(u->p == v);
    v->l = u->p = nullptr;
    this->update(v);
  }

  void evert(Ptr t) {
    expose(t);
    this->toggle(t);
    this->push(t);
  }

  Ptr lca(Ptr u, Ptr v) {
    if (get_root(u) != get_root(v)) return nullptr;
    expose(u);
    return expose(v);
  }

  Ptr get_kth(Ptr x, int k) {
    expose(x);
    while (x) {
      this->push(x);
      if (x->r && x->r->sz > k) {
        x = x->r;
      } else {
        if (x->r) k -= x->r->sz;
        if (k == 0) return x;
        k -= 1;
        x = x->l;
      }
    }
    return nullptr;
  }

  Ptr get_root(Ptr x) {
    expose(x);
    while (x->l) this->push(x), x = x->l;
    return x;
  }

  Ptr get_parent(Ptr x) {
    expose(x);
    Ptr p = x->l;
    if(p == nullptr) return nullptr;
    while (true) {
      this->push(p);
      if (p->r == nullptr) return p;
      p = p->r;
    }
    exit(1);
  }

  virtual void set_key(Ptr t, const decltype(Node::key)& key) {
    this->splay(t);
    t->key = key;
    this->update(t);
  }

  virtual decltype(Node::key) get_key(Ptr t) { return t->key; }

  decltype(Node::key) fold(Ptr u, Ptr v) {
    evert(u);
    expose(v);
    return v->sum;
  }
};

// Link Cut Tree - 木(森)のオンライン処理
// 木の回転（根の変更）・辺の削除・辺の追加・LCA・パスクエリなどを O(log n) で行う
// my_new(key)：新しい頂点の追加。頂点のポインタが返る（配列等で要管理）
// expose(u)：根からuまでのパスをPreferred Edgeからなるパスにする
// evert(u): 頂点uを木全体の根とする
// link(u, v): 頂点uと頂点vをつなぐ(u, vは連結成分でないことを仮定)
// cut(u, v)：頂点u,vを切り離す
// get_parent(u)：uの親を返す（rootの場合、nullptr）
// lca(u, v) : u, vのLCAを返す（木が異なる場合、nullptr）
// get_root(u)：uの所属する木の根を返す
// set_key(u, key)：頂点uのkeyを書き換える
// get_key(u, key)：頂点uのkeyの値を得る
// fold(u, v) : u, v間のパスのkeyの和を得る
template <typename T, T (*f)(T, T), T (*ts)(T)>
struct LinkCutTree : LinkCutBase<ReversibleSplayTree<T, f, ts>> {};
