#pragma once
#include <bits/stdc++.h>
using namespace std;

/*
 * ACL lazy_segtree を最小限改造した Segment Tree Beats と用途別ライブラリ。
 * 各データ構造は S/F/op/e/mapping/composition/id を定義し、segtree_beats に渡して使う。
 * mapping が区間情報だけでは処理できない場合は S::fail=true を返し、segtree_beats が子へ作用を降ろして再構築する。
 */

template <class S,
          S (*op)(S, S),
          S (*e)(),
          class F,
          S (*mapping)(F, S),
          F (*composition)(F, F),
          F (*id)()>
struct segtree_beats {
  public:
    // 空の列で初期化する O(1)
    segtree_beats() : segtree_beats(0) {}

    // 長さn、全要素e()で初期化する O(n)
    explicit segtree_beats(int n) : segtree_beats(vector<S>(n, e())) {}

    // 配列vで初期化する O(n)
    explicit segtree_beats(const vector<S>& v) : _n(int(v.size())) {
        log = ceil_pow2(_n);
        size = 1 << log;
        d = vector<S>(2 * size, e());
        lz = vector<F>(size, id());
        for (int i = 0; i < _n; i++) d[size + i] = v[i];
        for (int i = size - 1; i >= 1; i--) update(i);
    }

    // p番目をxに変更する O(logN)
    void set(int p, S x) {
        assert(0 <= p && p < _n);
        p += size;
        for (int i = log; i >= 1; i--) push(p >> i);
        d[p] = x;
        for (int i = 1; i <= log; i++) update(p >> i);
    }

    // p番目の値を返す O(logN)
    S get(int p) {
        assert(0 <= p && p < _n);
        p += size;
        for (int i = log; i >= 1; i--) push(p >> i);
        return d[p];
    }

    // [l,r) の集約値を返す O(logN + K)、Kは必要な追加降下数
    S prod(int l, int r) {
        assert(0 <= l && l <= r && r <= _n);
        if (l == r) return e();

        // 参照する境界上の遅延作用を先に下ろす
        l += size;
        r += size;
        for (int i = log; i >= 1; i--) {
            if (((l >> i) << i) != l) push(l >> i);
            if (((r >> i) << i) != r) push((r - 1) >> i);
        }

        // 左右から半開区間を分解して集約する
        S sml = e(), smr = e();
        while (l < r) {
            if (l & 1) sml = op(sml, d[l++]);
            if (r & 1) smr = op(d[--r], smr);
            l >>= 1;
            r >>= 1;
        }
        return op(sml, smr);
    }

    // 全区間の集約値を返す O(1)
    S all_prod() { return d[1]; }

    // p番目にfを適用する O(logN)
    void apply(int p, F f) {
        assert(0 <= p && p < _n);
        p += size;
        for (int i = log; i >= 1; i--) push(p >> i);
        d[p] = mapping(f, d[p]);
        assert(!d[p].fail);
        for (int i = 1; i <= log; i++) update(p >> i);
    }

    // [l,r) にfを適用する O(logN + K)、Kはmapping失敗により追加で降下した頂点数
    void apply(int l, int r, F f) {
        assert(0 <= l && l <= r && r <= _n);
        if (l == r) return;

        // 更新前に境界上の遅延作用を下ろす
        l += size;
        r += size;
        for (int i = log; i >= 1; i--) {
            if (((l >> i) << i) != l) push(l >> i);
            if (((r >> i) << i) != r) push((r - 1) >> i);
        }

        // 被覆する頂点に作用を適用する
        int l2 = l, r2 = r;
        while (l < r) {
            if (l & 1) all_apply(l++, f);
            if (r & 1) all_apply(--r, f);
            l >>= 1;
            r >>= 1;
        }
        l = l2;
        r = r2;

        // 境界から根方向に再構築する
        for (int i = 1; i <= log; i++) {
            if (((l >> i) << i) != l) update(l >> i);
            if (((r >> i) << i) != r) update((r - 1) >> i);
        }
    }

    // g(prod(l,r))がtrueとなる最大のrを返す O(logN + K)
    template <bool (*g)(S)> int max_right(int l) {
        return max_right(l, [](S x) { return g(x); });
    }

    // g(prod(l,r))がtrueとなる最大のrを返す O(logN + K)
    template <class G> int max_right(int l, G g) {
        assert(0 <= l && l <= _n);
        assert(g(e()));
        if (l == _n) return _n;

        // 探索開始位置までの遅延作用を下ろす
        l += size;
        for (int i = log; i >= 1; i--) push(l >> i);
        S sm = e();

        // 左から順に、条件が壊れる最初の頂点を探す
        do {
            while (l % 2 == 0) l >>= 1;
            if (!g(op(sm, d[l]))) {
                while (l < size) {
                    push(l);
                    l = 2 * l;
                    if (g(op(sm, d[l]))) {
                        sm = op(sm, d[l]);
                        l++;
                    }
                }
                return l - size;
            }
            sm = op(sm, d[l]);
            l++;
        } while ((l & -l) != l);
        return _n;
    }

    // g(prod(l,r))がtrueとなる最小のlを返す O(logN + K)
    template <bool (*g)(S)> int min_left(int r) {
        return min_left(r, [](S x) { return g(x); });
    }

    // g(prod(l,r))がtrueとなる最小のlを返す O(logN + K)
    template <class G> int min_left(int r, G g) {
        assert(0 <= r && r <= _n);
        assert(g(e()));
        if (r == 0) return 0;

        // 探索開始位置までの遅延作用を下ろす
        r += size;
        for (int i = log; i >= 1; i--) push((r - 1) >> i);
        S sm = e();

        // 右から順に、条件が壊れる最初の頂点を探す
        do {
            r--;
            while (r > 1 && (r % 2)) r >>= 1;
            if (!g(op(d[r], sm))) {
                while (r < size) {
                    push(r);
                    r = 2 * r + 1;
                    if (g(op(d[r], sm))) {
                        sm = op(d[r], sm);
                        r--;
                    }
                }
                return r + 1 - size;
            }
            sm = op(d[r], sm);
        } while ((r & -r) != r);
        return 0;
    }

  private:
    int _n = 0;
    int size = 1;
    int log = 0;
    vector<S> d;
    vector<F> lz;

    static int ceil_pow2(int n) {
        int x = 0;
        while ((1U << x) < (unsigned int)(n)) x++;
        return x;
    }

    void update(int k) { d[k] = op(d[2 * k], d[2 * k + 1]); }

    void all_apply(int k, F f) {
        d[k] = mapping(f, d[k]);
        if (k < size) {
            lz[k] = composition(f, lz[k]);
            if (d[k].fail) {
                push(k);
                update(k);
            }
        }
    }

    void push(int k) {
        all_apply(2 * k, lz[k]);
        all_apply(2 * k + 1, lz[k]);
        lz[k] = id();
    }
};

// 区間chminと区間和・最大値を管理する Segment Tree Beats
namespace ChminSumQ {
    using ll = long long;
    constexpr ll INF = (1LL << 61);
    constexpr ll NEG = -INF;

    struct S {
        ll mx;
        ll mx2;
        ll sum;
        int cnt_mx;
        int len;
        bool fail;
    };

    struct F {
        ll ub;

        // 区間chmin作用を作る O(1)
        static F chmin(ll x) { return {x}; }
    };

    inline S e() { return {NEG, NEG, 0, 0, 0, false}; }

    inline S make(ll x) { return {x, NEG, x, 1, 1, false}; }

    inline S op(S a, S b) {
        if (a.len == 0) return b;
        if (b.len == 0) return a;
        S r;
        r.sum = a.sum + b.sum;
        r.len = a.len + b.len;
        r.fail = false;
        if (a.mx == b.mx) {
            r.mx = a.mx;
            r.cnt_mx = a.cnt_mx + b.cnt_mx;
            r.mx2 = max(a.mx2, b.mx2);
        } else if (a.mx > b.mx) {
            r.mx = a.mx;
            r.cnt_mx = a.cnt_mx;
            r.mx2 = max(a.mx2, b.mx);
        } else {
            r.mx = b.mx;
            r.cnt_mx = b.cnt_mx;
            r.mx2 = max(a.mx, b.mx2);
        }
        return r;
    }

    inline S mapping(F f, S x) {
        if (x.len == 0 || f.ub >= x.mx) return x;
        if (x.len == 1) return make(min(x.mx, f.ub));
        if (f.ub > x.mx2) {
            x.sum += (f.ub - x.mx) * x.cnt_mx;
            x.mx = f.ub;
            return x;
        }
        x.fail = true;
        return x;
    }

    inline F composition(F f, F g) { return {min(f.ub, g.ub)}; }

    inline F id() { return {INF}; }

    using Seg = segtree_beats<S, op, e, F, mapping, composition, id>;
}

struct ChminSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// 区間chmin更新と区間和・最大値クエリを処理する O((N+Q)log^2N) 償却
vector<long long> solve_chmin_sum(vector<long long> a, const vector<ChminSumQuery>& queries) {
    vector<ChminSumQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = ChminSumQ::make(a[i]);
    ChminSumQ::Seg seg(v);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) seg.apply(q.l, q.r, ChminSumQ::F::chmin(q.x));
        else if (q.type == 1) ans.push_back(seg.prod(q.l, q.r).sum);
        else if (q.type == 2) ans.push_back(seg.prod(q.l, q.r).mx);
    }
    return ans;
}

// 区間chmaxと区間和・最小値を管理する Segment Tree Beats
namespace ChmaxSumQ {
    using ll = long long;
    constexpr ll INF = (1LL << 61);
    constexpr ll NEG = -INF;

    struct S {
        ll mn;
        ll mn2;
        ll sum;
        int cnt_mn;
        int len;
        bool fail;
    };

    struct F {
        ll lb;

        // 区間chmax作用を作る O(1)
        static F chmax(ll x) { return {x}; }
    };

    inline S e() { return {INF, INF, 0, 0, 0, false}; }

    inline S make(ll x) { return {x, INF, x, 1, 1, false}; }

    inline S op(S a, S b) {
        if (a.len == 0) return b;
        if (b.len == 0) return a;
        S r;
        r.sum = a.sum + b.sum;
        r.len = a.len + b.len;
        r.fail = false;
        if (a.mn == b.mn) {
            r.mn = a.mn;
            r.cnt_mn = a.cnt_mn + b.cnt_mn;
            r.mn2 = min(a.mn2, b.mn2);
        } else if (a.mn < b.mn) {
            r.mn = a.mn;
            r.cnt_mn = a.cnt_mn;
            r.mn2 = min(a.mn2, b.mn);
        } else {
            r.mn = b.mn;
            r.cnt_mn = b.cnt_mn;
            r.mn2 = min(a.mn, b.mn2);
        }
        return r;
    }

    inline S mapping(F f, S x) {
        if (x.len == 0 || f.lb <= x.mn) return x;
        if (x.len == 1) return make(max(x.mn, f.lb));
        if (f.lb < x.mn2) {
            x.sum += (f.lb - x.mn) * x.cnt_mn;
            x.mn = f.lb;
            return x;
        }
        x.fail = true;
        return x;
    }

    inline F composition(F f, F g) { return {max(f.lb, g.lb)}; }

    inline F id() { return {NEG}; }

    using Seg = segtree_beats<S, op, e, F, mapping, composition, id>;
}

struct ChmaxSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// 区間chmax更新と区間和・最小値クエリを処理する O((N+Q)log^2N) 償却
vector<long long> solve_chmax_sum(vector<long long> a, const vector<ChmaxSumQuery>& queries) {
    vector<ChmaxSumQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = ChmaxSumQ::make(a[i]);
    ChmaxSumQ::Seg seg(v);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) seg.apply(q.l, q.r, ChmaxSumQ::F::chmax(q.x));
        else if (q.type == 1) ans.push_back(seg.prod(q.l, q.r).sum);
        else if (q.type == 2) ans.push_back(seg.prod(q.l, q.r).mn);
    }
    return ans;
}

/*
 * 区間chmin・chmax・加算・代入と、区間min・max・sumをまとめて管理する Segment Tree Beats。
 * 次の典型用途を1つの構造で扱える。
 *   - 区間chmin更新: seg.apply(l, r, ChminChmaxAddSumQ::F::chmin(x))
 *   - 区間chmax更新: seg.apply(l, r, ChminChmaxAddSumQ::F::chmax(x))
 *   - RAQ / 区間加算: seg.apply(l, r, ChminChmaxAddSumQ::F::add(x))
 *   - RUQ / 区間代入: seg.apply(l, r, ChminChmaxAddSumQ::F::assign(x))
 *   - RSQ / 区間和: seg.prod(l, r).sum
 *   - RMQ / 区間最小値: seg.prod(l, r).lo
 *   - RMaxQ / 区間最大値: seg.prod(l, r).hi
 * 最小値の個数は seg.prod(l, r).nlo、最大値の個数は seg.prod(l, r).nhi で取得できる。
 */
namespace ChminChmaxAddSumQ {
    using ll = long long;
    constexpr ll INF = (1LL << 61);
    constexpr ll NEG = -INF;

    inline ll second_lowest(ll a, ll a2, ll b, ll b2) {
        return a == b ? min(a2, b2) : a2 <= b ? a2 : b2 <= a ? b2 : max(a, b);
    }

    inline ll second_highest(ll a, ll a2, ll b, ll b2) {
        return a == b ? max(a2, b2) : a2 >= b ? a2 : b2 >= a ? b2 : min(a, b);
    }

    struct S {
        ll lo;       // 区間最小値。RMQの答え
        ll hi;       // 区間最大値。RMaxQの答え
        ll lo2;      // 区間内の2番目に小さい値。存在しない場合はINF
        ll hi2;      // 区間内の2番目に大きい値。存在しない場合はNEG
        ll sum;      // 区間和。RSQの答え
        unsigned sz; // 区間長
        unsigned nlo;// 区間最小値loの個数
        unsigned nhi;// 区間最大値hiの個数
        bool fail;   // mappingが一括適用できないときtrueにして子へ降ろす
    };

    // 作用は x -> clamp(x, lb, ub) + bias として表す
    struct F {
        ll lb;
        ll ub;
        ll bias;

        // 区間chmin更新 a[i]=min(a[i],x) を作る O(1)
        static F chmin(ll x) { return {-INF, x, 0}; }

        // 区間chmax更新 a[i]=max(a[i],x) を作る O(1)
        static F chmax(ll x) { return {x, INF, 0}; }

        // RAQ用の区間加算更新 a[i]+=x を作る O(1)
        static F add(ll x) { return {-INF, INF, x}; }

        // RUQ用の区間代入更新 a[i]=x を作る O(1)
        static F assign(ll x) { return {x, x, 0}; }
    };

    // 空区間の集約値を返す O(1)
    inline S e() { return {INF, NEG, INF, NEG, 0, 0, 0, 0, false}; }

    // 1要素xの葉を作る O(1)
    inline S make(ll x) { return {x, x, INF, NEG, x, 1, 1, 1, false}; }

    // 全要素x、長さszの区間を作る O(1)
    inline S make(ll x, unsigned sz) { return {x, x, INF, NEG, x * (ll)sz, sz, sz, sz, false}; }

    // 左右の区間情報を併合する O(1)
    inline S op(S l, S r) {
        S ret;
        ret.lo = min(l.lo, r.lo);
        ret.hi = max(l.hi, r.hi);
        ret.lo2 = second_lowest(l.lo, l.lo2, r.lo, r.lo2);
        ret.hi2 = second_highest(l.hi, l.hi2, r.hi, r.hi2);
        ret.sum = l.sum + r.sum;
        ret.sz = l.sz + r.sz;
        ret.nlo = l.nlo * (l.lo <= r.lo) + r.nlo * (r.lo <= l.lo);
        ret.nhi = l.nhi * (l.hi >= r.hi) + r.nhi * (r.hi >= l.hi);
        ret.fail = false;
        return ret;
    }

    // 区間情報xに作用fを適用する O(1)、失敗時はfail=trueを返す
    inline S mapping(F f, S x) {
        if (x.sz == 0) return e();

        // 空でない区間全体が同じ値になる場合は、sum/min/max/countをまとめて作れる
        if (x.lo == x.hi || f.lb == f.ub || f.lb >= x.hi || f.ub <= x.lo) {
            ll y = min(max(x.lo, f.lb), f.ub) + f.bias;
            return make(y, x.sz);
        }

        // 区間内の値が2種類だけなら、下側と上側を直接更新できる
        if (x.lo2 == x.hi) {
            x.lo = x.hi2 = max(x.lo, f.lb) + f.bias;
            x.hi = x.lo2 = min(x.hi, f.ub) + f.bias;
            x.sum = x.lo * (ll)x.nlo + x.hi * (ll)x.nhi;
            return x;
        }

        // chmaxが最小値群だけ、chminが最大値群だけに効く場合は一括適用できる
        if (f.lb < x.lo2 && f.ub > x.hi2) {
            ll nxt_lo = max(x.lo, f.lb);
            ll nxt_hi = min(x.hi, f.ub);
            x.sum += (nxt_lo - x.lo) * (ll)x.nlo - (x.hi - nxt_hi) * (ll)x.nhi + f.bias * (ll)x.sz;
            x.lo = nxt_lo + f.bias;
            x.hi = nxt_hi + f.bias;
            x.lo2 += f.bias;
            x.hi2 += f.bias;
            return x;
        }

        // 2番目の最小値・最大値をまたぐ更新は、子へ降ろして処理する
        x.fail = true;
        return x;
    }

    // 作用を合成する O(1)、ACLと同じく先にg、後にfを適用する f∘g
    inline F composition(F f, F g) {
        F ret;
        ret.lb = max(min(g.lb + g.bias, f.ub), f.lb) - g.bias;
        ret.ub = min(max(g.ub + g.bias, f.lb), f.ub) - g.bias;
        ret.bias = g.bias + f.bias;
        return ret;
    }

    // 何もしない作用を返す O(1)
    inline F id() { return {-INF, INF, 0}; }

    using Seg = segtree_beats<S, op, e, F, mapping, composition, id>;
}

/*
 * ChminChmaxAddSumQ用のサンプルクエリ。
 * type=0: [l,r) に chmin(x)
 * type=1: [l,r) に chmax(x)
 * type=2: [l,r) に add(x)    RAQ
 * type=3: [l,r) に assign(x) RUQ
 * type=4: [l,r) の sum       RSQ
 * type=5: [l,r) の min       RMQ
 * type=6: [l,r) の max       RMaxQ
 * type=7: [l,r) の min の個数
 * type=8: [l,r) の max の個数
 */
struct ChminChmaxAddSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// 区間chmin・chmax・RAQ・RUQ・RSQ・RMQ・RMaxQ・min/max個数クエリを処理する O((N+Q)log^2N) 償却
vector<long long> solve_chmin_chmax_add_sum(vector<long long> a, const vector<ChminChmaxAddSumQuery>& queries) {
    // 初期配列を葉用のSに変換して、Segment Tree Beatsを構築する
    vector<ChminChmaxAddSumQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = ChminChmaxAddSumQ::make(a[i]);
    ChminChmaxAddSumQ::Seg seg(v);

    // typeごとに、更新はapply、取得はprod(l,r)の該当フィールドを使う
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) seg.apply(q.l, q.r, ChminChmaxAddSumQ::F::chmin(q.x));
        else if (q.type == 1) seg.apply(q.l, q.r, ChminChmaxAddSumQ::F::chmax(q.x));
        else if (q.type == 2) seg.apply(q.l, q.r, ChminChmaxAddSumQ::F::add(q.x));
        else if (q.type == 3) seg.apply(q.l, q.r, ChminChmaxAddSumQ::F::assign(q.x));
        else if (q.type == 4) ans.push_back(seg.prod(q.l, q.r).sum);
        else if (q.type == 5) ans.push_back(seg.prod(q.l, q.r).lo);
        else if (q.type == 6) ans.push_back(seg.prod(q.l, q.r).hi);
        else if (q.type == 7) ans.push_back(seg.prod(q.l, q.r).nlo);
        else if (q.type == 8) ans.push_back(seg.prod(q.l, q.r).nhi);
    }
    return ans;
}

// 非負30bit整数の区間bitwise AND/OR更新と区間最大値を管理する Segment Tree Beats
namespace BitwiseAndOrMaxQ {
    using UINT = uint32_t;
    constexpr int DIGIT = 30;
    constexpr UINT MASK = (UINT(1) << DIGIT) - 1;

    struct S {
        UINT mx;
        UINT upper;
        UINT lower;
        int len;
        bool fail;
    };

    struct F {
        UINT bit_and;
        UINT bit_or;

        // 区間bitwise AND作用を作る O(1)
        static F bit_and_update(UINT x) { return {x & MASK, 0}; }

        // 区間bitwise OR作用を作る O(1)
        static F bit_or_update(UINT x) { return {MASK, x & MASK}; }
    };

    inline S e() { return {0, 0, MASK, 0, false}; }

    inline S make(UINT x) {
        x &= MASK;
        return {x, x, x, 1, false};
    }

    inline S op(S a, S b) {
        if (a.len == 0) return b;
        if (b.len == 0) return a;
        return {max(a.mx, b.mx), a.upper | b.upper, a.lower & b.lower, a.len + b.len, false};
    }

    inline S mapping(F f, S x) {
        if (x.len == 0) return x;
        UINT changed = ((~f.bit_and) | f.bit_or) & MASK;
        UINT mixed = (x.upper ^ x.lower) & MASK;
        if (mixed & changed) {
            x.fail = true;
            return x;
        }
        x.mx = (x.mx & f.bit_and) | f.bit_or;
        x.upper = (x.upper & f.bit_and) | f.bit_or;
        x.lower = (x.lower & f.bit_and) | f.bit_or;
        return x;
    }

    inline F composition(F f, F g) {
        return {f.bit_and & g.bit_and, f.bit_or | (f.bit_and & g.bit_or)};
    }

    inline F id() { return {MASK, 0}; }

    using Seg = segtree_beats<S, op, e, F, mapping, composition, id>;
}

struct BitwiseAndOrMaxQuery {
    int type;
    int l;
    int r;
    uint32_t x;
};

// 区間bitwise AND/OR更新と区間最大値クエリを処理する O((N+Q)30logN) 償却
vector<uint32_t> solve_bitwise_and_or_max(vector<uint32_t> a, const vector<BitwiseAndOrMaxQuery>& queries) {
    vector<BitwiseAndOrMaxQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = BitwiseAndOrMaxQ::make(a[i]);
    BitwiseAndOrMaxQ::Seg seg(v);
    vector<uint32_t> ans;
    for (const auto& q : queries) {
        if (q.type == 0) seg.apply(q.l, q.r, BitwiseAndOrMaxQ::F::bit_and_update(q.x));
        else if (q.type == 1) seg.apply(q.l, q.r, BitwiseAndOrMaxQ::F::bit_or_update(q.x));
        else if (q.type == 2) ans.push_back(seg.prod(q.l, q.r).mx);
    }
    return ans;
}

// 正整数の区間代入・区間gcd更新と区間最大値・区間和を管理する Segment Tree Beats
namespace AssignGcdMaxSumQ {
    using UINT = uint32_t;
    constexpr UINT LCM_CAP = UINT(1) << 30;

    struct S {
        UINT mx;
        UINT lcm;
        UINT len;
        unsigned long long sum;
        bool fail;
    };

    struct F {
        UINT dogcd;
        UINT reset;

        // 区間gcd更新作用を作る O(1)
        static F gcd_update(UINT x) { return {x, 0}; }

        // 区間代入作用を作る O(1)、x=0は未対応
        static F assign(UINT x) { return {0, x}; }
    };

    inline UINT capped_lcm(UINT a, UINT b) {
        unsigned long long g = std::gcd(a, b);
        unsigned long long v = (unsigned long long)a / g * b;
        return (UINT)min<unsigned long long>(LCM_CAP, v);
    }

    inline S e() { return {0, 1, 0, 0, false}; }

    inline S make(UINT x, UINT len = 1) {
        if (len == 0) return e();
        return {x, min(x, LCM_CAP), len, (unsigned long long)x * len, false};
    }

    inline S op(S a, S b) {
        if (a.len == 0) return b;
        if (b.len == 0) return a;
        return {max(a.mx, b.mx), capped_lcm(a.lcm, b.lcm), a.len + b.len, a.sum + b.sum, false};
    }

    inline S mapping(F f, S x) {
        if (x.len == 0) return e();
        if (x.fail) return x;
        if (f.reset) x = make(f.reset, x.len);
        if (f.dogcd) {
            if (x.len == 1) {
                x = make((UINT)std::gcd(x.mx, f.dogcd));
            } else if (x.lcm == LCM_CAP || f.dogcd % x.lcm) {
                x.fail = true;
            }
        }
        return x;
    }

    inline F composition(F f, F g) {
        if (f.reset) return F::assign(f.reset);
        if (g.reset) return F::assign((UINT)std::gcd(f.dogcd, g.reset));
        return F::gcd_update((UINT)std::gcd(f.dogcd, g.dogcd));
    }

    inline F id() { return {0, 0}; }

    using Seg = segtree_beats<S, op, e, F, mapping, composition, id>;
}

struct AssignGcdMaxSumQuery {
    int type;
    int l;
    int r;
    uint32_t x;
};

// 正整数列の区間代入・区間gcd更新と区間最大値・区間和クエリを処理する O((N+Q)log^2N) 償却
vector<unsigned long long> solve_assign_gcd_max_sum(vector<uint32_t> a, const vector<AssignGcdMaxSumQuery>& queries) {
    vector<AssignGcdMaxSumQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = AssignGcdMaxSumQ::make(a[i]);
    AssignGcdMaxSumQ::Seg seg(v);
    vector<unsigned long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) seg.apply(q.l, q.r, AssignGcdMaxSumQ::F::assign(q.x));
        else if (q.type == 1) seg.apply(q.l, q.r, AssignGcdMaxSumQ::F::gcd_update(q.x));
        else if (q.type == 2) ans.push_back(seg.prod(q.l, q.r).mx);
        else if (q.type == 3) ans.push_back(seg.prod(q.l, q.r).sum);
    }
    return ans;
}


// 非負整数の区間mod更新と区間和・最大値を管理する Segment Tree Beats
namespace ModSumQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);

    struct S {
        ll mx;
        ll sum;
        int len;
        bool fail;
    };

    struct F {
        ll mod;

        // 区間mod作用を作る O(1)、xは正整数
        static F modulo(ll x) { return {x}; }
    };

    inline S e() { return {0, 0, 0, false}; }

    inline S make(ll x) { return {x, x, 1, false}; }

    inline S op(S a, S b) {
        if (a.len == 0) return b;
        if (b.len == 0) return a;
        return {max(a.mx, b.mx), a.sum + b.sum, a.len + b.len, false};
    }

    inline S mapping(F f, S x) {
        if (x.len == 0 || f.mod == INF || x.mx < f.mod) return x;
        if (x.len == 1) return make(x.mx % f.mod);
        x.fail = true;
        return x;
    }

    inline F composition(F f, F g) { return {min(f.mod, g.mod)}; }

    inline F id() { return {INF}; }

    using Seg = segtree_beats<S, op, e, F, mapping, composition, id>;
}

struct ModSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// 非負整数列の区間mod更新・一点代入と区間和・最大値クエリを処理する O((N+Q)logNlogA) 償却、modは正整数
vector<long long> solve_mod_sum(vector<long long> a, const vector<ModSumQuery>& queries) {
    vector<ModSumQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = ModSumQ::make(a[i]);
    ModSumQ::Seg seg(v);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) seg.apply(q.l, q.r, ModSumQ::F::modulo(q.x));
        else if (q.type == 1) ans.push_back(seg.prod(q.l, q.r).sum);
        else if (q.type == 2) ans.push_back(seg.prod(q.l, q.r).mx);
        else if (q.type == 3) seg.set(q.l, ModSumQ::make(q.x));
    }
    return ans;
}

// 非負整数の区間floor-sqrt更新と区間和・最大値を管理する Segment Tree Beats
namespace SqrtSumQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);

    struct S {
        ll mn;
        ll mx;
        ll sum;
        int len;
        bool fail;
    };

    struct F {
        int count;

        // 区間floor-sqrt作用を1回作る O(1)
        static F sqrt_update() { return {1}; }
    };

    inline ll isqrt(ll x) {
        ll r = (ll)sqrt((long double)x);
        while ((__int128)(r + 1) * (r + 1) <= x) r++;
        while ((__int128)r * r > x) r--;
        return r;
    }

    inline S e() { return {INF, 0, 0, 0, false}; }

    inline S make(ll x, int len = 1) {
        if (len == 0) return e();
        return {x, x, x * (ll)len, len, false};
    }

    inline S op(S a, S b) {
        if (a.len == 0) return b;
        if (b.len == 0) return a;
        return {min(a.mn, b.mn), max(a.mx, b.mx), a.sum + b.sum, a.len + b.len, false};
    }

    inline S apply_once_or_fail(S x, bool& ok) {
        if (x.len == 0 || x.mx <= 1) return x;
        ll nmn = isqrt(x.mn);
        ll nmx = isqrt(x.mx);
        if (x.mn == x.mx) return make(nmn, x.len);
        ll dmn = x.mn - nmn;
        ll dmx = x.mx - nmx;
        if (dmn == dmx) {
            x.mn -= dmn;
            x.mx -= dmx;
            x.sum -= dmn * (ll)x.len;
            return x;
        }
        ok = false;
        return x;
    }

    inline S mapping(F f, S x) {
        if (x.len == 0 || f.count == 0 || x.mx <= 1) return x;
        S cur = x;
        for (int i = 0; i < f.count; i++) {
            if (cur.mx <= 1) return cur;
            bool ok = true;
            S nxt = apply_once_or_fail(cur, ok);
            if (!ok) {
                x.fail = true;
                return x;
            }
            cur = nxt;
            cur.fail = false;
        }
        return cur;
    }

    inline F composition(F f, F g) { return {min(64, f.count + g.count)}; }

    inline F id() { return {0}; }

    using Seg = segtree_beats<S, op, e, F, mapping, composition, id>;
}

struct SqrtSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// 非負整数列の区間floor-sqrt更新・一点代入と区間和・最大値クエリを処理する O((N+Q)logNloglogA) 償却
vector<long long> solve_sqrt_sum(vector<long long> a, const vector<SqrtSumQuery>& queries) {
    vector<SqrtSumQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = SqrtSumQ::make(a[i]);
    SqrtSumQ::Seg seg(v);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) seg.apply(q.l, q.r, SqrtSumQ::F::sqrt_update());
        else if (q.type == 1) ans.push_back(seg.prod(q.l, q.r).sum);
        else if (q.type == 2) ans.push_back(seg.prod(q.l, q.r).mx);
        else if (q.type == 3) seg.set(q.l, SqrtSumQ::make(q.x));
    }
    return ans;
}

#if __INCLUDE_LEVEL__ == 0

long long naive_sum_ll(const vector<long long>& a, int l, int r) {
    long long s = 0;
    for (int i = l; i < r; i++) s += a[i];
    return s;
}

long long naive_min_ll(const vector<long long>& a, int l, int r) {
    long long v = ChminChmaxAddSumQ::INF;
    for (int i = l; i < r; i++) v = min(v, a[i]);
    return v;
}

long long naive_max_ll(const vector<long long>& a, int l, int r) {
    long long v = ChminChmaxAddSumQ::NEG;
    for (int i = l; i < r; i++) v = max(v, a[i]);
    return v;
}

uint32_t naive_max_u32(const vector<uint32_t>& a, int l, int r) {
    uint32_t v = 0;
    for (int i = l; i < r; i++) v = max(v, a[i]);
    return v;
}

unsigned long long naive_sum_u32(const vector<uint32_t>& a, int l, int r) {
    unsigned long long s = 0;
    for (int i = l; i < r; i++) s += a[i];
    return s;
}

uint32_t naive_gcd_u32(uint32_t a, uint32_t b) {
    return (uint32_t)std::gcd(a, b);
}

void test_fixed_edges() {
    {
        ChminSumQ::Seg seg0(vector<ChminSumQ::S>{});
        assert(seg0.all_prod().len == 0);
        ChminSumQ::Seg seg(vector<ChminSumQ::S>{ChminSumQ::make(5)});
        assert(seg.prod(0, 0).len == 0);
        assert(seg.prod(0, 1).sum == 5);
        seg.apply(0, 1, ChminSumQ::F::chmin(3));
        assert(seg.get(0).mx == 3);
        assert(seg.all_prod().sum == 3);
    }
    {
        ChmaxSumQ::Seg seg(vector<ChmaxSumQ::S>{ChmaxSumQ::make(-5)});
        seg.apply(0, 1, ChmaxSumQ::F::chmax(-2));
        assert(seg.get(0).mn == -2);
        assert(seg.all_prod().sum == -2);
    }
    {
        vector<ChminChmaxAddSumQ::S> v;
        for (long long x : vector<long long>{5, -2, 7, 7, 1}) v.push_back(ChminChmaxAddSumQ::make(x));
        ChminChmaxAddSumQ::Seg seg(v);
        assert(seg.all_prod().sum == 18);
        seg.apply(0, 5, ChminChmaxAddSumQ::F::chmin(4));
        assert(seg.all_prod().sum == 11);
        seg.apply(1, 4, ChminChmaxAddSumQ::F::chmax(3));
        assert(seg.prod(0, 5).lo == 1);
        seg.apply(0, 5, ChminChmaxAddSumQ::F::add(2));
        assert(seg.prod(0, 5).hi == 6);
        seg.apply(2, 5, ChminChmaxAddSumQ::F::assign(-1));
        assert(seg.prod(0, 5).sum == 8);
    }
    {
        vector<BitwiseAndOrMaxQ::S> v;
        for (uint32_t x : vector<uint32_t>{1, 2, 3}) v.push_back(BitwiseAndOrMaxQ::make(x));
        BitwiseAndOrMaxQ::Seg seg(v);
        seg.apply(0, 3, BitwiseAndOrMaxQ::F::bit_or_update(4));
        assert(seg.all_prod().mx == 7);
        seg.apply(0, 2, BitwiseAndOrMaxQ::F::bit_and_update(5));
        assert(seg.prod(0, 3).mx == 7);
    }
    {
        vector<AssignGcdMaxSumQ::S> v;
        for (uint32_t x : vector<uint32_t>{6, 10, 15}) v.push_back(AssignGcdMaxSumQ::make(x));
        AssignGcdMaxSumQ::Seg seg(v);
        seg.apply(0, 3, AssignGcdMaxSumQ::F::gcd_update(6));
        assert(seg.all_prod().sum == 11);
        seg.apply(1, 3, AssignGcdMaxSumQ::F::assign(12));
        assert(seg.prod(0, 3).mx == 12);
        assert(seg.prod(0, 3).sum == 30);
    }
    {
        vector<ModSumQ::S> v;
        for (long long x : vector<long long>{10, 7, 4}) v.push_back(ModSumQ::make(x));
        ModSumQ::Seg seg(v);
        seg.apply(0, 2, ModSumQ::F::modulo(6));
        assert(seg.prod(0, 3).sum == 9);
        assert(seg.prod(0, 3).mx == 4);
        seg.set(2, ModSumQ::make(20));
        seg.apply(0, 3, ModSumQ::F::modulo(5));
        assert(seg.prod(0, 3).sum == 5);
    }
    {
        vector<SqrtSumQ::S> v;
        for (long long x : vector<long long>{16, 15, 1}) v.push_back(SqrtSumQ::make(x));
        SqrtSumQ::Seg seg(v);
        seg.apply(0, 3, SqrtSumQ::F::sqrt_update());
        assert(seg.prod(0, 3).sum == 8);
        assert(seg.prod(0, 3).mx == 4);
        seg.apply(0, 3, SqrtSumQ::F::sqrt_update());
        assert(seg.prod(0, 3).sum == 4);
    }
}

void test_chmin_sum_random(mt19937& rng) {
    for (int tc = 0; tc < 200; tc++) {
        int n = (int)(rng() % 25);
        vector<long long> a(n);
        for (auto& x : a) x = (int)(rng() % 101) - 50;
        vector<ChminSumQ::S> v(n);
        for (int i = 0; i < n; i++) v[i] = ChminSumQ::make(a[i]);
        ChminSumQ::Seg seg(v);
        for (int q = 0; q < 300; q++) {
            if (n == 0) {
                assert(seg.all_prod().len == 0);
                continue;
            }
            int l = (int)(rng() % n);
            int r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            int type = (int)(rng() % 5);
            if (type <= 1) {
                long long x = (int)(rng() % 121) - 60;
                seg.apply(l, r, ChminSumQ::F::chmin(x));
                for (int i = l; i < r; i++) a[i] = min(a[i], x);
            } else if (type == 2) {
                auto got = seg.prod(l, r);
                assert(got.sum == naive_sum_ll(a, l, r));
                if (l == r) assert(got.len == 0);
                else assert(got.mx == naive_max_ll(a, l, r));
            } else if (type == 3) {
                int p = (int)(rng() % n);
                auto got = seg.get(p);
                assert(got.sum == a[p] && got.mx == a[p]);
            } else {
                auto got = seg.all_prod();
                assert(got.sum == naive_sum_ll(a, 0, n));
            }
        }
    }
}

void test_chmax_sum_random(mt19937& rng) {
    for (int tc = 0; tc < 200; tc++) {
        int n = (int)(rng() % 25);
        vector<long long> a(n);
        for (auto& x : a) x = (int)(rng() % 101) - 50;
        vector<ChmaxSumQ::S> v(n);
        for (int i = 0; i < n; i++) v[i] = ChmaxSumQ::make(a[i]);
        ChmaxSumQ::Seg seg(v);
        for (int q = 0; q < 300; q++) {
            if (n == 0) {
                assert(seg.all_prod().len == 0);
                continue;
            }
            int l = (int)(rng() % n);
            int r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            int type = (int)(rng() % 5);
            if (type <= 1) {
                long long x = (int)(rng() % 121) - 60;
                seg.apply(l, r, ChmaxSumQ::F::chmax(x));
                for (int i = l; i < r; i++) a[i] = max(a[i], x);
            } else if (type == 2) {
                auto got = seg.prod(l, r);
                assert(got.sum == naive_sum_ll(a, l, r));
                if (l == r) assert(got.len == 0);
                else assert(got.mn == naive_min_ll(a, l, r));
            } else if (type == 3) {
                int p = (int)(rng() % n);
                auto got = seg.get(p);
                assert(got.sum == a[p] && got.mn == a[p]);
            } else {
                auto got = seg.all_prod();
                assert(got.sum == naive_sum_ll(a, 0, n));
            }
        }
    }
}

void test_chmin_chmax_add_sum_random(mt19937& rng) {
    for (int tc = 0; tc < 250; tc++) {
        int n = (int)(rng() % 30);
        vector<long long> a(n);
        for (auto& x : a) x = (int)(rng() % 101) - 50;
        vector<ChminChmaxAddSumQ::S> v(n);
        for (int i = 0; i < n; i++) v[i] = ChminChmaxAddSumQ::make(a[i]);
        ChminChmaxAddSumQ::Seg seg(v);
        for (int q = 0; q < 400; q++) {
            if (n == 0) {
                assert(seg.all_prod().sz == 0);
                continue;
            }
            int l = (int)(rng() % n);
            int r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            int type = (int)(rng() % 8);
            long long x = (int)(rng() % 121) - 60;
            if (type == 0) {
                seg.apply(l, r, ChminChmaxAddSumQ::F::chmin(x));
                for (int i = l; i < r; i++) a[i] = min(a[i], x);
            } else if (type == 1) {
                seg.apply(l, r, ChminChmaxAddSumQ::F::chmax(x));
                for (int i = l; i < r; i++) a[i] = max(a[i], x);
            } else if (type == 2) {
                x = (int)(rng() % 31) - 15;
                seg.apply(l, r, ChminChmaxAddSumQ::F::add(x));
                for (int i = l; i < r; i++) a[i] += x;
            } else if (type == 3) {
                seg.apply(l, r, ChminChmaxAddSumQ::F::assign(x));
                for (int i = l; i < r; i++) a[i] = x;
            } else if (type == 4) {
                auto got = seg.prod(l, r);
                assert(got.sum == naive_sum_ll(a, l, r));
                if (l == r) assert(got.sz == 0);
                else {
                    assert(got.lo == naive_min_ll(a, l, r));
                    assert(got.hi == naive_max_ll(a, l, r));
                }
            } else if (type == 5) {
                int p = (int)(rng() % n);
                auto got = seg.get(p);
                assert(got.sum == a[p] && got.lo == a[p] && got.hi == a[p]);
            } else if (type == 6) {
                auto got = seg.all_prod();
                assert(got.sum == naive_sum_ll(a, 0, n));
            } else {
                unsigned lim = (unsigned)(rng() % (n + 1));
                auto f = [&](ChminChmaxAddSumQ::S s) { return s.sz <= lim; };
                int pos = seg.max_right(0, f);
                int left = seg.min_left(n, f);
                assert(pos == (int)lim);
                assert(left == n - (int)lim);
            }
        }
    }
}

void test_bitwise_and_or_max_random(mt19937& rng) {
    constexpr uint32_t MASK = BitwiseAndOrMaxQ::MASK;
    for (int tc = 0; tc < 200; tc++) {
        int n = (int)(rng() % 30);
        vector<uint32_t> a(n);
        for (auto& x : a) x = rng() & MASK;
        vector<BitwiseAndOrMaxQ::S> v(n);
        for (int i = 0; i < n; i++) v[i] = BitwiseAndOrMaxQ::make(a[i]);
        BitwiseAndOrMaxQ::Seg seg(v);
        for (int q = 0; q < 350; q++) {
            if (n == 0) {
                assert(seg.all_prod().len == 0);
                continue;
            }
            int l = (int)(rng() % n);
            int r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            int type = (int)(rng() % 5);
            uint32_t x = rng() & MASK;
            if (type == 0) {
                seg.apply(l, r, BitwiseAndOrMaxQ::F::bit_and_update(x));
                for (int i = l; i < r; i++) a[i] &= x;
            } else if (type == 1) {
                seg.apply(l, r, BitwiseAndOrMaxQ::F::bit_or_update(x));
                for (int i = l; i < r; i++) a[i] = (a[i] | x) & MASK;
            } else if (type == 2) {
                auto got = seg.prod(l, r);
                if (l == r) assert(got.len == 0);
                else assert(got.mx == naive_max_u32(a, l, r));
            } else if (type == 3) {
                int p = (int)(rng() % n);
                assert(seg.get(p).mx == a[p]);
            } else {
                assert(seg.all_prod().mx == naive_max_u32(a, 0, n));
            }
        }
    }
}

void test_assign_gcd_max_sum_random(mt19937& rng) {
    for (int tc = 0; tc < 200; tc++) {
        int n = (int)(rng() % 30);
        vector<uint32_t> a(n);
        for (auto& x : a) x = (uint32_t)(rng() % 200 + 1);
        vector<AssignGcdMaxSumQ::S> v(n);
        for (int i = 0; i < n; i++) v[i] = AssignGcdMaxSumQ::make(a[i]);
        AssignGcdMaxSumQ::Seg seg(v);
        for (int q = 0; q < 350; q++) {
            if (n == 0) {
                assert(seg.all_prod().len == 0);
                continue;
            }
            int l = (int)(rng() % n);
            int r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            int type = (int)(rng() % 6);
            uint32_t x = (uint32_t)(rng() % 200 + 1);
            if (type == 0) {
                seg.apply(l, r, AssignGcdMaxSumQ::F::assign(x));
                for (int i = l; i < r; i++) a[i] = x;
            } else if (type == 1) {
                seg.apply(l, r, AssignGcdMaxSumQ::F::gcd_update(x));
                for (int i = l; i < r; i++) a[i] = naive_gcd_u32(a[i], x);
            } else if (type == 2) {
                auto got = seg.prod(l, r);
                if (l == r) assert(got.len == 0);
                else assert(got.mx == naive_max_u32(a, l, r));
            } else if (type == 3) {
                assert(seg.prod(l, r).sum == naive_sum_u32(a, l, r));
            } else if (type == 4) {
                int p = (int)(rng() % n);
                assert(seg.get(p).mx == a[p]);
            } else {
                auto got = seg.all_prod();
                assert(got.sum == naive_sum_u32(a, 0, n));
                assert(got.mx == naive_max_u32(a, 0, n));
            }
        }
    }
}


void test_mod_sum_random(mt19937& rng) {
    for (int tc = 0; tc < 220; tc++) {
        int n = (int)(rng() % 30);
        vector<long long> a(n);
        for (auto& x : a) x = (long long)(rng() % 500);
        vector<ModSumQ::S> v(n);
        for (int i = 0; i < n; i++) v[i] = ModSumQ::make(a[i]);
        ModSumQ::Seg seg(v);
        for (int q = 0; q < 400; q++) {
            if (n == 0) {
                assert(seg.all_prod().len == 0);
                continue;
            }
            int l = (int)(rng() % n);
            int r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            int type = (int)(rng() % 6);
            if (type <= 1) {
                long long x = (long long)(rng() % 300 + 1);
                seg.apply(l, r, ModSumQ::F::modulo(x));
                for (int i = l; i < r; i++) a[i] %= x;
            } else if (type == 2) {
                auto got = seg.prod(l, r);
                assert(got.sum == naive_sum_ll(a, l, r));
                if (l == r) assert(got.len == 0);
                else assert(got.mx == naive_max_ll(a, l, r));
            } else if (type == 3) {
                int p = (int)(rng() % n);
                long long x = (long long)(rng() % 500);
                seg.set(p, ModSumQ::make(x));
                a[p] = x;
            } else if (type == 4) {
                int p = (int)(rng() % n);
                auto got = seg.get(p);
                assert(got.sum == a[p] && got.mx == a[p]);
            } else {
                auto got = seg.all_prod();
                assert(got.sum == naive_sum_ll(a, 0, n));
                assert(got.mx == naive_max_ll(a, 0, n));
            }
        }
    }
}

void test_sqrt_sum_random(mt19937& rng) {
    for (int tc = 0; tc < 220; tc++) {
        int n = (int)(rng() % 30);
        vector<long long> a(n);
        for (auto& x : a) x = (long long)(rng() % 1000000000000LL);
        vector<SqrtSumQ::S> v(n);
        for (int i = 0; i < n; i++) v[i] = SqrtSumQ::make(a[i]);
        SqrtSumQ::Seg seg(v);
        for (int q = 0; q < 400; q++) {
            if (n == 0) {
                assert(seg.all_prod().len == 0);
                continue;
            }
            int l = (int)(rng() % n);
            int r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            int type = (int)(rng() % 6);
            if (type <= 1) {
                seg.apply(l, r, SqrtSumQ::F::sqrt_update());
                for (int i = l; i < r; i++) a[i] = SqrtSumQ::isqrt(a[i]);
            } else if (type == 2) {
                auto got = seg.prod(l, r);
                assert(got.sum == naive_sum_ll(a, l, r));
                if (l == r) assert(got.len == 0);
                else assert(got.mx == naive_max_ll(a, l, r));
            } else if (type == 3) {
                int p = (int)(rng() % n);
                long long x = (long long)(rng() % 1000000000000LL);
                seg.set(p, SqrtSumQ::make(x));
                a[p] = x;
            } else if (type == 4) {
                int p = (int)(rng() % n);
                auto got = seg.get(p);
                assert(got.sum == a[p] && got.mn == a[p] && got.mx == a[p]);
            } else {
                auto got = seg.all_prod();
                assert(got.sum == naive_sum_ll(a, 0, n));
                assert(got.mx == naive_max_ll(a, 0, n));
            }
        }
    }
}

void test_solvers() {
    {
        vector<long long> a = {5, 1, 7, 3};
        vector<ChminSumQuery> qs = {{1, 0, 4, 0}, {0, 0, 3, 4}, {1, 0, 4, 0}, {2, 0, 4, 0}};
        vector<long long> expect = {16, 12, 4};
        assert(solve_chmin_sum(a, qs) == expect);
    }
    {
        vector<long long> a = {5, 1, 7, 3};
        vector<ChmaxSumQuery> qs = {{1, 0, 4, 0}, {0, 1, 4, 4}, {1, 0, 4, 0}, {2, 0, 4, 0}};
        vector<long long> expect = {16, 20, 4};
        assert(solve_chmax_sum(a, qs) == expect);
    }
    {
        vector<long long> a = {5, 1, 7, 3};
        vector<ChminChmaxAddSumQuery> qs = {
            {4, 0, 4, 0}, {0, 0, 4, 4}, {1, 1, 3, 3}, {2, 0, 2, 2},
            {4, 0, 4, 0}, {5, 0, 4, 0}, {6, 0, 4, 0}, {3, 2, 4, -1}, {4, 0, 4, 0}
        };
        vector<long long> expect = {16, 18, 3, 6, 9};
        auto got = solve_chmin_chmax_add_sum(a, qs);
        assert(got == expect);
    }
    {
        vector<uint32_t> a = {1, 2, 3};
        vector<BitwiseAndOrMaxQuery> qs = {{2, 0, 3, 0}, {1, 0, 3, 4}, {2, 0, 3, 0}, {0, 0, 2, 5}, {2, 0, 3, 0}};
        vector<uint32_t> expect = {3, 7, 7};
        assert(solve_bitwise_and_or_max(a, qs) == expect);
    }
    {
        vector<uint32_t> a = {6, 10, 15};
        vector<AssignGcdMaxSumQuery> qs = {{3, 0, 3, 0}, {1, 0, 3, 6}, {3, 0, 3, 0}, {2, 0, 3, 0}, {0, 1, 3, 12}, {3, 0, 3, 0}};
        vector<unsigned long long> expect = {31, 11, 6, 30};
        assert(solve_assign_gcd_max_sum(a, qs) == expect);
    }
    {
        vector<long long> a = {10, 7, 4};
        vector<ModSumQuery> qs = {{1, 0, 3, 0}, {0, 0, 2, 6}, {1, 0, 3, 0}, {2, 0, 3, 0}, {3, 2, 0, 20}, {0, 0, 3, 5}, {1, 0, 3, 0}};
        vector<long long> expect = {21, 9, 4, 5};
        assert(solve_mod_sum(a, qs) == expect);
    }
    {
        vector<long long> a = {16, 15, 1};
        vector<SqrtSumQuery> qs = {{1, 0, 3, 0}, {0, 0, 3, 0}, {1, 0, 3, 0}, {2, 0, 3, 0}, {3, 1, 0, 81}, {0, 0, 2, 0}, {1, 0, 3, 0}};
        vector<long long> expect = {32, 8, 4, 12};
        assert(solve_sqrt_sum(a, qs) == expect);
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    mt19937 rng(123456789);
    test_fixed_edges();
    test_chmin_sum_random(rng);
    test_chmax_sum_random(rng);
    test_chmin_chmax_add_sum_random(rng);
    test_bitwise_and_or_max_random(rng);
    test_assign_gcd_max_sum_random(rng);
    test_mod_sum_random(rng);
    test_sqrt_sum_random(rng);
    test_solvers();

    cout << "All tests passed\n";
    return 0;
}

#endif
