#pragma once
#include <bits/stdc++.h>
#include <atcoder/segtree>
using namespace std;

/*
 * ACL の atcoder::segtree に渡す用途別モノイド集
 * 各 namespace は単体でコピーしやすいよう、S, op, e, 必要最小限のヘルパー, Seg を含む
 * 各 namespace の直後に、そのモノイドを使うユースケースソルバー関数を置く
 */

// 区間最小値を管理する。点更新、区間最小値、最初に閾値未満になる位置の探索に対応
namespace RMQ {
    using S = int;

    inline S op(S a, S b) {
        return min(a, b);
    }

    inline S e() {
        return numeric_limits<S>::max();
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RmqPointSetRangeMinQuery {
    int type;
    int a;
    int b;
};

// type=0: a番目をbに代入、type=1: [a,b) の最小値を返す O((N+Q)logN)
vector<int> solve_rmq_point_set_range_min(vector<int> a, const vector<RmqPointSetRangeMinQuery>& queries) {
    RMQ::Seg seg(a);
    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.a, q.b);
        } else {
            ans.push_back(seg.prod(q.a, q.b));
        }
    }
    return ans;
}

struct FirstLessThanQuery {
    int start;
    int x;
};

// start以降で初めて値がx未満になる位置を返し、存在しなければNを返す O((N+Q)logN)
vector<int> solve_first_less_than_x(const vector<int>& a, const vector<FirstLessThanQuery>& queries) {
    RMQ::Seg seg(a);
    vector<int> ans;
    for (const auto& q : queries) {
        int pos = seg.max_right(q.start, [&](int mn) {
            return mn >= q.x;
        });
        ans.push_back(pos);
    }
    return ans;
}

// 区間最大値を管理する。点更新、区間最大値、最初に閾値超過になる位置の探索に対応
namespace RMaxQ {
    using S = int;

    inline S op(S a, S b) {
        return max(a, b);
    }

    inline S e() {
        return numeric_limits<S>::min();
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RmaxqPointSetRangeMaxQuery {
    int type;
    int a;
    int b;
};

// type=0: a番目をbに代入、type=1: [a,b) の最大値を返す O((N+Q)logN)
vector<int> solve_rmaxq_point_set_range_max(vector<int> a, const vector<RmaxqPointSetRangeMaxQuery>& queries) {
    RMaxQ::Seg seg(a);
    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.a, q.b);
        } else {
            ans.push_back(seg.prod(q.a, q.b));
        }
    }
    return ans;
}

struct FirstGreaterThanQuery {
    int start;
    int x;
};

// start以降で初めて値がx超過になる位置を返し、存在しなければNを返す O((N+Q)logN)
vector<int> solve_first_greater_than_x(const vector<int>& a, const vector<FirstGreaterThanQuery>& queries) {
    RMaxQ::Seg seg(a);
    vector<int> ans;
    for (const auto& q : queries) {
        int pos = seg.max_right(q.start, [&](int mx) {
            return mx <= q.x;
        });
        ans.push_back(pos);
    }
    return ans;
}

// 区間和を管理する。点更新、点加算、区間和、非負列での累積和探索に対応
namespace RSQ {
    using S = long long;

    inline S op(S a, S b) {
        return a + b;
    }

    inline S e() {
        return 0;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RsqPointAddRangeSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目にxを加算、type=1: [l,r) の和を返す O((N+Q)logN)
vector<long long> solve_rsq_point_add_range_sum(vector<long long> a, const vector<RsqPointAddRangeSumQuery>& queries) {
    RSQ::Seg seg(a);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, seg.get(q.l) + q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 非負数列で、sum[0,i+1) >= target となる最小のiを返し、存在しなければNを返す O((N+Q)logN)
vector<int> solve_prefix_lower_bound_sum(const vector<long long>& a, const vector<long long>& targets) {
    RSQ::Seg seg(a);
    vector<int> ans;
    for (long long target : targets) {
        if (target <= 0) {
            ans.push_back(0);
            continue;
        }
        int pos = seg.max_right(0, [&](long long sum) {
            return sum < target;
        });
        ans.push_back(pos);
    }
    return ans;
}

// 01列で0-indexedのk番目の1の位置を返し、存在しなければNを返す O((N+Q)logN)
vector<int> solve_kth_one(const vector<int>& bits, const vector<int>& ks) {
    vector<long long> count(bits.size());
    for (int i = 0; i < (int)bits.size(); i++) count[i] = bits[i] ? 1 : 0;
    RSQ::Seg seg(count);

    vector<int> ans;
    for (int k : ks) {
        if (k < 0) {
            ans.push_back(-1);
            continue;
        }
        int pos = seg.max_right(0, [&](long long sum) {
            return sum <= k;
        });
        ans.push_back(pos);
    }
    return ans;
}

// 区間積を固定modで管理する。負数はmod正規化して扱う
namespace RProdModQ {
    using S = long long;
    constexpr long long MOD = 998244353;

    inline S norm(long long x) {
        x %= MOD;
        if (x < 0) x += MOD;
        return x;
    }

    inline S op(S a, S b) {
        return a * b % MOD;
    }

    inline S e() {
        return 1;
    }

    inline S make(long long x) {
        return norm(x);
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RprodModPointSetRangeProductQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目をxに代入、type=1: [l,r) のmod積を返す O((N+Q)logN)
vector<long long> solve_rprod_mod_point_set_range_product(vector<long long> a, const vector<RprodModPointSetRangeProductQuery>& queries) {
    vector<RProdModQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = RProdModQ::make(a[i]);
    RProdModQ::Seg seg(v);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, RProdModQ::make(q.x));
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 区間gcdを管理する。0を単位元として、値の符号に依存しない非負gcdを返す
namespace RGcdQ {
    using S = long long;

    inline S op(S a, S b) {
        return std::gcd(a, b);
    }

    inline S e() {
        return 0;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RgcdPointSetRangeGcdQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目をxに代入、type=1: [l,r) のgcdを返す O((N+Q)logN)
vector<long long> solve_rgcd_point_set_range_gcd(vector<long long> a, const vector<RgcdPointSetRangeGcdQuery>& queries) {
    RGcdQ::Seg seg(a);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 区間lcmを上限付きで管理する。結果がINFを超える場合はINFに丸める
namespace RLcmCapQ {
    using S = long long;
    constexpr long long INF = (1LL << 62);

    inline S abs_cap(S x) {
        if (x == numeric_limits<S>::min()) return INF;
        return x < 0 ? -x : x;
    }

    inline S op(S a, S b) {
        a = abs_cap(a);
        b = abs_cap(b);
        if (a == 0 || b == 0) return 0;
        long long g = std::gcd(a, b);
        __int128 v = (__int128)(a / g) * b;
        if (v > INF) return INF;
        return (long long)v;
    }

    inline S e() {
        return 1;
    }

    inline S make(long long x) {
        return abs_cap(x);
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RlcmCapPointSetRangeLcmQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目をxに代入、type=1: [l,r) の上限付きlcmを返す O((N+Q)logN)
vector<long long> solve_rlcm_cap_point_set_range_lcm(vector<long long> a, const vector<RlcmCapPointSetRangeLcmQuery>& queries) {
    vector<RLcmCapQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = RLcmCapQ::make(a[i]);
    RLcmCapQ::Seg seg(v);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, RLcmCapQ::make(q.x));
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 区間xorを管理する。点更新、区間xorに対応
namespace RXorQ {
    using S = long long;

    inline S op(S a, S b) {
        return a ^ b;
    }

    inline S e() {
        return 0;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RxorPointSetRangeXorQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目をxに代入、type=1: [l,r) のxorを返す O((N+Q)logN)
vector<long long> solve_rxor_point_set_range_xor(vector<long long> a, const vector<RxorPointSetRangeXorQuery>& queries) {
    RXorQ::Seg seg(a);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 区間bitwise-orを管理する。点更新、区間orに対応
namespace ROrQ {
    using S = long long;

    inline S op(S a, S b) {
        return a | b;
    }

    inline S e() {
        return 0;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RorPointSetRangeOrQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目をxに代入、type=1: [l,r) のbitwise-orを返す O((N+Q)logN)
vector<long long> solve_ror_point_set_range_or(vector<long long> a, const vector<RorPointSetRangeOrQuery>& queries) {
    ROrQ::Seg seg(a);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 区間bitwise-andを管理する。点更新、区間andに対応
namespace RAndQ {
    using S = long long;

    inline S op(S a, S b) {
        return a & b;
    }

    inline S e() {
        return ~0LL;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RandPointSetRangeAndQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目をxに代入、type=1: [l,r) のbitwise-andを返す O((N+Q)logN)
vector<long long> solve_rand_point_set_range_and(vector<long long> a, const vector<RandPointSetRangeAndQuery>& queries) {
    RAndQ::Seg seg(a);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 区間最小値とその出現回数を管理する。点更新、最小値個数取得に対応
namespace MinCountQ {
    struct S {
        int value;
        int count;
    };

    inline S op(S a, S b) {
        if (a.value < b.value) return a;
        if (a.value > b.value) return b;
        return {a.value, a.count + b.count};
    }

    inline S e() {
        return {numeric_limits<int>::max(), 0};
    }

    inline S make(int value) {
        return {value, 1};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct MinCountPointSetRangeQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに代入、type=1: [l,r) の最小値と個数を返す O((N+Q)logN)
vector<pair<int, int>> solve_min_count_point_set_range(vector<int> a, const vector<MinCountPointSetRangeQuery>& queries) {
    vector<MinCountQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = MinCountQ::make(a[i]);
    MinCountQ::Seg seg(v);

    vector<pair<int, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, MinCountQ::make(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.count});
        }
    }
    return ans;
}

// 区間最大値とその出現回数を管理する。点更新、最大値個数取得に対応
namespace MaxCountQ {
    struct S {
        int value;
        int count;
    };

    inline S op(S a, S b) {
        if (a.value > b.value) return a;
        if (a.value < b.value) return b;
        return {a.value, a.count + b.count};
    }

    inline S e() {
        return {numeric_limits<int>::min(), 0};
    }

    inline S make(int value) {
        return {value, 1};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct MaxCountPointSetRangeQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに代入、type=1: [l,r) の最大値と個数を返す O((N+Q)logN)
vector<pair<int, int>> solve_max_count_point_set_range(vector<int> a, const vector<MaxCountPointSetRangeQuery>& queries) {
    vector<MaxCountQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = MaxCountQ::make(a[i]);
    MaxCountQ::Seg seg(v);

    vector<pair<int, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, MaxCountQ::make(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.count});
        }
    }
    return ans;
}

// 区間最小値とその最小indexを管理する。同値の場合は小さいindexを返す
namespace ArgMinQ {
    struct S {
        int value;
        int index;
    };

    inline S op(S a, S b) {
        if (a.value != b.value) return a.value < b.value ? a : b;
        return a.index < b.index ? a : b;
    }

    inline S e() {
        return {numeric_limits<int>::max(), numeric_limits<int>::max()};
    }

    inline S make(int value, int index) {
        return {value, index};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct ArgMinPointSetRangeQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに代入、type=1: [l,r) の最小値と最小indexを返す O((N+Q)logN)
vector<pair<int, int>> solve_argmin_point_set_range(vector<int> a, const vector<ArgMinPointSetRangeQuery>& queries) {
    vector<ArgMinQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = ArgMinQ::make(a[i], i);
    ArgMinQ::Seg seg(v);

    vector<pair<int, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, ArgMinQ::make(q.x, q.l));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.index});
        }
    }
    return ans;
}

// 区間最大値とその最小indexを管理する。同値の場合は小さいindexを返す
namespace ArgMaxQ {
    struct S {
        int value;
        int index;
    };

    inline S op(S a, S b) {
        if (a.value != b.value) return a.value > b.value ? a : b;
        return a.index < b.index ? a : b;
    }

    inline S e() {
        return {numeric_limits<int>::min(), numeric_limits<int>::max()};
    }

    inline S make(int value, int index) {
        return {value, index};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct ArgMaxPointSetRangeQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに代入、type=1: [l,r) の最大値と最小indexを返す O((N+Q)logN)
vector<pair<int, int>> solve_argmax_point_set_range(vector<int> a, const vector<ArgMaxPointSetRangeQuery>& queries) {
    vector<ArgMaxQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = ArgMaxQ::make(a[i], i);
    ArgMaxQ::Seg seg(v);

    vector<pair<int, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, ArgMaxQ::make(q.x, q.l));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.index});
        }
    }
    return ans;
}

// 区間最小値と最大値を同時に管理する。区間幅や範囲判定に使う
namespace MinMaxQ {
    struct S {
        int mn;
        int mx;
    };

    inline S op(S a, S b) {
        return {min(a.mn, b.mn), max(a.mx, b.mx)};
    }

    inline S e() {
        return {numeric_limits<int>::max(), numeric_limits<int>::min()};
    }

    inline S make(int value) {
        return {value, value};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct MinMaxPointSetRangeQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに代入、type=1: [l,r) の最小値と最大値を返す O((N+Q)logN)
vector<pair<int, int>> solve_minmax_point_set_range(vector<int> a, const vector<MinMaxPointSetRangeQuery>& queries) {
    vector<MinMaxQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = MinMaxQ::make(a[i]);
    MinMaxQ::Seg seg(v);

    vector<pair<int, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, MinMaxQ::make(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.mn, res.mx});
        }
    }
    return ans;
}

// 区間の最大値と2番目の最大値、それぞれの個数を管理する
namespace Top2MaxQ {
    struct S {
        int max1;
        int cnt1;
        int max2;
        int cnt2;
    };

    inline S e() {
        int low = numeric_limits<int>::min();
        return {low, 0, low, 0};
    }

    inline void add(S& res, int value, int count) {
        if (count == 0) return;
        if (value == res.max1) {
            res.cnt1 += count;
        } else if (value > res.max1) {
            if (res.cnt1 > 0) {
                res.max2 = res.max1;
                res.cnt2 = res.cnt1;
            }
            res.max1 = value;
            res.cnt1 = count;
        } else if (value == res.max2) {
            res.cnt2 += count;
        } else if (value > res.max2) {
            res.max2 = value;
            res.cnt2 = count;
        }
    }

    inline S op(S a, S b) {
        S res = e();
        add(res, a.max1, a.cnt1);
        add(res, a.max2, a.cnt2);
        add(res, b.max1, b.cnt1);
        add(res, b.max2, b.cnt2);
        return res;
    }

    inline S make(int value) {
        return {value, 1, numeric_limits<int>::min(), 0};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct Top2MaxPointSetRangeQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに代入、type=1: [l,r) の2番目の最大値と個数を返す O((N+Q)logN)
vector<pair<int, int>> solve_top2max_point_set_range(vector<int> a, const vector<Top2MaxPointSetRangeQuery>& queries) {
    vector<Top2MaxQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = Top2MaxQ::make(a[i]);
    Top2MaxQ::Seg seg(v);

    vector<pair<int, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, Top2MaxQ::make(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.max2, res.cnt2});
        }
    }
    return ans;
}

// 区間の最小値と2番目の最小値、それぞれの個数を管理する
namespace Top2MinQ {
    struct S {
        int min1;
        int cnt1;
        int min2;
        int cnt2;
    };

    inline S e() {
        int high = numeric_limits<int>::max();
        return {high, 0, high, 0};
    }

    inline void add(S& res, int value, int count) {
        if (count == 0) return;
        if (value == res.min1) {
            res.cnt1 += count;
        } else if (value < res.min1) {
            if (res.cnt1 > 0) {
                res.min2 = res.min1;
                res.cnt2 = res.cnt1;
            }
            res.min1 = value;
            res.cnt1 = count;
        } else if (value == res.min2) {
            res.cnt2 += count;
        } else if (value < res.min2) {
            res.min2 = value;
            res.cnt2 = count;
        }
    }

    inline S op(S a, S b) {
        S res = e();
        add(res, a.min1, a.cnt1);
        add(res, a.min2, a.cnt2);
        add(res, b.min1, b.cnt1);
        add(res, b.min2, b.cnt2);
        return res;
    }

    inline S make(int value) {
        return {value, 1, numeric_limits<int>::max(), 0};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct Top2MinPointSetRangeQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに代入、type=1: [l,r) の2番目の最小値と個数を返す O((N+Q)logN)
vector<pair<int, int>> solve_top2min_point_set_range(vector<int> a, const vector<Top2MinPointSetRangeQuery>& queries) {
    vector<Top2MinQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = Top2MinQ::make(a[i]);
    Top2MinQ::Seg seg(v);

    vector<pair<int, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, Top2MinQ::make(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.min2, res.cnt2});
        }
    }
    return ans;
}

// 値の存在有無を0/1で管理する。値域[0,N)の動的mex取得に対応
namespace PresenceMexQ {
    using S = int;

    inline S op(S a, S b) {
        return min(a, b);
    }

    inline S e() {
        return 1;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct DynamicMexQuery {
    int type;
    int x;
};

// type=0: xを追加、type=1: xを削除、type=2: mexを返す O((N+Q)logN)
vector<int> solve_dynamic_mex(int value_limit, const vector<int>& initial_values, const vector<DynamicMexQuery>& queries) {
    vector<int> count(value_limit, 0);
    for (int x : initial_values) {
        if (0 <= x && x < value_limit) count[x]++;
    }

    vector<int> present(value_limit);
    for (int i = 0; i < value_limit; i++) present[i] = count[i] > 0 ? 1 : 0;
    PresenceMexQ::Seg seg(present);

    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            if (0 <= q.x && q.x < value_limit) {
                if (count[q.x] == 0) seg.set(q.x, 1);
                count[q.x]++;
            }
        } else if (q.type == 1) {
            if (0 <= q.x && q.x < value_limit && count[q.x] > 0) {
                count[q.x]--;
                if (count[q.x] == 0) seg.set(q.x, 0);
            }
        } else {
            int mex = seg.max_right(0, [](int mn) {
                return mn == 1;
            });
            ans.push_back(mex);
        }
    }
    return ans;
}

// 括弧列を管理する。区間の総和と最小prefixから正しい括弧列かを判定する
namespace BracketSeqQ {
    struct S {
        int sum;
        int min_prefix;
    };

    inline S op(S a, S b) {
        return {a.sum + b.sum, min(a.min_prefix, a.sum + b.min_prefix)};
    }

    inline S e() {
        return {0, 0};
    }

    inline S make(char c) {
        if (c == '(') return {1, 0};
        return {-1, -1};
    }

    inline bool is_valid(S x) {
        return x.sum == 0 && x.min_prefix >= 0;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct BracketSequenceQuery {
    int type;
    int l;
    int r;
    char c;
};

// type=0: l番目をcに変更、type=1: [l,r) が正しい括弧列なら1を返す O((N+Q)logN)
vector<int> solve_bracket_sequence_queries(string s, const vector<BracketSequenceQuery>& queries) {
    vector<BracketSeqQ::S> v(s.size());
    for (int i = 0; i < (int)s.size(); i++) v[i] = BracketSeqQ::make(s[i]);
    BracketSeqQ::Seg seg(v);

    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, BracketSeqQ::make(q.c));
            s[q.l] = q.c;
        } else {
            ans.push_back(BracketSeqQ::is_valid(seg.prod(q.l, q.r)) ? 1 : 0);
        }
    }
    return ans;
}

// 01列の区間転倒数を管理する。結合時に左の1と右の0の組を加える
namespace BinaryInversionQ {
    using ll = long long;

    struct S {
        ll zero;
        ll one;
        ll inv;
    };

    inline S op(S a, S b) {
        return {a.zero + b.zero, a.one + b.one, a.inv + b.inv + a.one * b.zero};
    }

    inline S e() {
        return {0, 0, 0};
    }

    inline S make(int bit) {
        if (bit) return {0, 1, 0};
        return {1, 0, 0};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct BinaryInversionQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに変更、type=1: [l,r) の01転倒数を返す O((N+Q)logN)
vector<long long> solve_binary_inversion_queries(vector<int> bits, const vector<BinaryInversionQuery>& queries) {
    vector<BinaryInversionQ::S> v(bits.size());
    for (int i = 0; i < (int)bits.size(); i++) v[i] = BinaryInversionQ::make(bits[i]);
    BinaryInversionQ::Seg seg(v);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, BinaryInversionQ::make(q.x));
            bits[q.l] = q.x;
        } else {
            ans.push_back(seg.prod(q.l, q.r).inv);
        }
    }
    return ans;
}

// 01列の最長連続1を管理する。区間長、prefix、suffix、最良値を持つ
namespace LongestOneQ {
    struct S {
        int len;
        int pref;
        int suff;
        int best;
    };

    inline S op(S a, S b) {
        return {
            a.len + b.len,
            a.pref == a.len ? a.len + b.pref : a.pref,
            b.suff == b.len ? b.len + a.suff : b.suff,
            max(max(a.best, b.best), a.suff + b.pref)
        };
    }

    inline S e() {
        return {0, 0, 0, 0};
    }

    inline S make(int bit) {
        if (bit) return {1, 1, 1, 1};
        return {1, 0, 0, 0};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct LongestOneQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: l番目をxに変更、type=1: [l,r) の最長連続1長を返す O((N+Q)logN)
vector<int> solve_longest_one_queries(vector<int> bits, const vector<LongestOneQuery>& queries) {
    vector<LongestOneQ::S> v(bits.size());
    for (int i = 0; i < (int)bits.size(); i++) v[i] = LongestOneQ::make(bits[i]);
    LongestOneQ::Seg seg(v);

    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, LongestOneQ::make(q.x));
            bits[q.l] = q.x;
        } else {
            ans.push_back(seg.prod(q.l, q.r).best);
        }
    }
    return ans;
}

// 区間最大部分配列和を管理する。空部分配列は許さず、空区間のbestはNEGになる
namespace MaxSubarrayQ {
    using ll = long long;
    constexpr ll NEG = -(1LL << 60);

    struct S {
        ll sum;
        ll pref;
        ll suff;
        ll best;
    };

    inline S op(S a, S b) {
        return {
            a.sum + b.sum,
            max(a.pref, a.sum + b.pref),
            max(b.suff, b.sum + a.suff),
            max(max(a.best, b.best), a.suff + b.pref)
        };
    }

    inline S e() {
        return {0, NEG, NEG, NEG};
    }

    inline S make(ll value) {
        return {value, value, value, value};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct MaxSubarrayQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: l番目をxに変更、type=1: [l,r) の最大部分配列和を返す O((N+Q)logN)
vector<long long> solve_max_subarray_queries(vector<long long> a, const vector<MaxSubarrayQuery>& queries) {
    vector<MaxSubarrayQ::S> v(a.size());
    for (int i = 0; i < (int)a.size(); i++) v[i] = MaxSubarrayQ::make(a[i]);
    MaxSubarrayQ::Seg seg(v);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.l, MaxSubarrayQ::make(q.x));
            a[q.l] = q.x;
        } else {
            ans.push_back(seg.prod(q.l, q.r).best);
        }
    }
    return ans;
}

// 一次関数 f(x)=a*x+b の列を管理する。prod(l,r) は f_{r-1}(...f_l(x)) を表す
namespace AffineCompositeQ {
    using ll = long long;
    constexpr ll MOD = 998244353;

    struct S {
        ll a;
        ll b;
    };

    inline ll norm(ll x) {
        x %= MOD;
        if (x < 0) x += MOD;
        return x;
    }

    inline S op(S left, S right) {
        return {right.a * left.a % MOD, (right.a * left.b + right.b) % MOD};
    }

    inline S e() {
        return {1, 0};
    }

    inline S make(ll a, ll b) {
        return {norm(a), norm(b)};
    }

    inline ll apply(S f, ll x) {
        return (f.a * norm(x) + f.b) % MOD;
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct AffineCompositeQuery {
    int type;
    int p;
    int l;
    int r;
    long long a;
    long long b;
    long long x;
};

// type=0: p番目をa*x+bに変更、type=1: [l,r) の合成関数をxに適用する O((N+Q)logN)
vector<long long> solve_affine_composite_queries(vector<pair<long long, long long>> funcs, const vector<AffineCompositeQuery>& queries) {
    vector<AffineCompositeQ::S> v(funcs.size());
    for (int i = 0; i < (int)funcs.size(); i++) v[i] = AffineCompositeQ::make(funcs[i].first, funcs[i].second);
    AffineCompositeQ::Seg seg(v);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.p, AffineCompositeQ::make(q.a, q.b));
            funcs[q.p] = {q.a, q.b};
        } else {
            ans.push_back(AffineCompositeQ::apply(seg.prod(q.l, q.r), q.x));
        }
    }
    return ans;
}

// 2x2行列の列を固定modで管理する。点更新、区間行列積に対応
namespace Matrix2x2Q {
    using ll = long long;
    constexpr ll MOD = 998244353;

    struct S {
        ll a00;
        ll a01;
        ll a10;
        ll a11;
    };

    inline ll norm(ll x) {
        x %= MOD;
        if (x < 0) x += MOD;
        return x;
    }

    inline S op(S a, S b) {
        return {
            (a.a00 * b.a00 + a.a01 * b.a10) % MOD,
            (a.a00 * b.a01 + a.a01 * b.a11) % MOD,
            (a.a10 * b.a00 + a.a11 * b.a10) % MOD,
            (a.a10 * b.a01 + a.a11 * b.a11) % MOD
        };
    }

    inline S e() {
        return {1, 0, 0, 1};
    }

    inline S make(ll a00, ll a01, ll a10, ll a11) {
        return {norm(a00), norm(a01), norm(a10), norm(a11)};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct Matrix2x2Query {
    int type;
    int p;
    int l;
    int r;
    Matrix2x2Q::S x;
};

// type=0: p番目をxに変更、type=1: [l,r) の行列積を返す O((N+Q)logN)
vector<Matrix2x2Q::S> solve_matrix2x2_product_queries(vector<Matrix2x2Q::S> matrices, const vector<Matrix2x2Query>& queries) {
    Matrix2x2Q::Seg seg(matrices);
    vector<Matrix2x2Q::S> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.p, q.x);
            matrices[q.p] = q.x;
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

// 文字列の64bit rolling hashを管理する。自然overflowを利用する
namespace RollingHashQ {
    using ull = unsigned long long;
    constexpr ull BASE = 1000003ULL;
    inline vector<ull> pow_base{1};

    struct S {
        ull hash;
        int len;
    };

    inline void ensure_pow(int n) {
        while ((int)pow_base.size() <= n) pow_base.push_back(pow_base.back() * BASE);
    }

    inline S op(S a, S b) {
        ensure_pow(b.len);
        return {a.hash * pow_base[b.len] + b.hash, a.len + b.len};
    }

    inline S e() {
        return {0, 0};
    }

    inline S make(char c) {
        return {(ull)(unsigned char)c + 1, 1};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct RollingHashQuery {
    int type;
    int l;
    int r;
    int p;
    char c;
};

// type=0: p番目をcに変更、type=1: [l,r) のhash値を返す O((N+Q)logN)
vector<unsigned long long> solve_rolling_hash_queries(string s, const vector<RollingHashQuery>& queries) {
    vector<RollingHashQ::S> v(s.size());
    for (int i = 0; i < (int)s.size(); i++) v[i] = RollingHashQ::make(s[i]);
    RollingHashQ::Seg seg(v);

    vector<unsigned long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.p, RollingHashQ::make(q.c));
            s[q.p] = q.c;
        } else {
            ans.push_back(seg.prod(q.l, q.r).hash);
        }
    }
    return ans;
}

// 数字列を10進連結した値を固定modで管理する。点更新、区間連結値取得に対応
namespace DecimalConcatModQ {
    using ll = long long;
    constexpr ll MOD = 998244353;
    inline vector<ll> pow10{1};

    struct S {
        ll value;
        int len;
    };

    inline void ensure_pow(int n) {
        while ((int)pow10.size() <= n) pow10.push_back(pow10.back() * 10 % MOD);
    }

    inline S op(S a, S b) {
        ensure_pow(b.len);
        return {(a.value * pow10[b.len] + b.value) % MOD, a.len + b.len};
    }

    inline S e() {
        return {0, 0};
    }

    inline S make_digit(int digit) {
        return {digit % 10, 1};
    }

    using Seg = atcoder::segtree<S, op, e>;
}

struct DecimalConcatQuery {
    int type;
    int l;
    int r;
    int p;
    int digit;
};

// type=0: p番目をdigitに変更、type=1: [l,r) の10進連結値modを返す O((N+Q)logN)
vector<long long> solve_decimal_concat_queries(vector<int> digits, const vector<DecimalConcatQuery>& queries) {
    vector<DecimalConcatModQ::S> v(digits.size());
    for (int i = 0; i < (int)digits.size(); i++) v[i] = DecimalConcatModQ::make_digit(digits[i]);
    DecimalConcatModQ::Seg seg(v);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.set(q.p, DecimalConcatModQ::make_digit(q.digit));
            digits[q.p] = q.digit;
        } else {
            ans.push_back(seg.prod(q.l, q.r).value);
        }
    }
    return ans;
}

#if __INCLUDE_LEVEL__ == 0

long long rand_ll(mt19937& rng, long long l, long long r) {
    return uniform_int_distribution<long long>(l, r)(rng);
}

bool eq_matrix(Matrix2x2Q::S a, Matrix2x2Q::S b) {
    return a.a00 == b.a00 && a.a01 == b.a01 && a.a10 == b.a10 && a.a11 == b.a11;
}

long long naive_lcm_cap(long long a, long long b) {
    return RLcmCapQ::op(a, b);
}

pair<int, int> naive_min_count(const vector<int>& a, int l, int r) {
    int value = numeric_limits<int>::max();
    int count = 0;
    for (int i = l; i < r; i++) {
        if (a[i] < value) {
            value = a[i];
            count = 1;
        } else if (a[i] == value) {
            count++;
        }
    }
    return {value, count};
}

pair<int, int> naive_max_count(const vector<int>& a, int l, int r) {
    int value = numeric_limits<int>::min();
    int count = 0;
    for (int i = l; i < r; i++) {
        if (a[i] > value) {
            value = a[i];
            count = 1;
        } else if (a[i] == value) {
            count++;
        }
    }
    return {value, count};
}

pair<int, int> naive_argmin(const vector<int>& a, int l, int r) {
    pair<int, int> res = {numeric_limits<int>::max(), numeric_limits<int>::max()};
    for (int i = l; i < r; i++) res = min(res, {a[i], i});
    return res;
}

pair<int, int> naive_argmax(const vector<int>& a, int l, int r) {
    pair<int, int> res = {numeric_limits<int>::min(), numeric_limits<int>::max()};
    for (int i = l; i < r; i++) {
        if (a[i] > res.first || (a[i] == res.first && i < res.second)) res = {a[i], i};
    }
    return res;
}

pair<int, int> naive_minmax(const vector<int>& a, int l, int r) {
    int mn = numeric_limits<int>::max();
    int mx = numeric_limits<int>::min();
    for (int i = l; i < r; i++) {
        mn = min(mn, a[i]);
        mx = max(mx, a[i]);
    }
    return {mn, mx};
}

Top2MaxQ::S naive_top2max(const vector<int>& a, int l, int r) {
    Top2MaxQ::S res = Top2MaxQ::e();
    for (int i = l; i < r; i++) Top2MaxQ::add(res, a[i], 1);
    return res;
}

Top2MinQ::S naive_top2min(const vector<int>& a, int l, int r) {
    Top2MinQ::S res = Top2MinQ::e();
    for (int i = l; i < r; i++) Top2MinQ::add(res, a[i], 1);
    return res;
}

bool naive_bracket_valid(const string& s, int l, int r) {
    int sum = 0;
    for (int i = l; i < r; i++) {
        sum += s[i] == '(' ? 1 : -1;
        if (sum < 0) return false;
    }
    return sum == 0;
}

long long naive_binary_inv(const vector<int>& bits, int l, int r) {
    long long inv = 0;
    for (int i = l; i < r; i++) {
        for (int j = i + 1; j < r; j++) {
            if (bits[i] == 1 && bits[j] == 0) inv++;
        }
    }
    return inv;
}

int naive_longest_one(const vector<int>& bits, int l, int r) {
    int best = 0;
    int cur = 0;
    for (int i = l; i < r; i++) {
        if (bits[i]) cur++;
        else cur = 0;
        best = max(best, cur);
    }
    return best;
}

long long naive_max_subarray(const vector<long long>& a, int l, int r) {
    if (l == r) return MaxSubarrayQ::NEG;
    long long best = MaxSubarrayQ::NEG;
    for (int i = l; i < r; i++) {
        long long sum = 0;
        for (int j = i; j < r; j++) {
            sum += a[j];
            best = max(best, sum);
        }
    }
    return best;
}

AffineCompositeQ::S naive_affine_compose(const vector<pair<long long, long long>>& funcs, int l, int r) {
    AffineCompositeQ::S res = AffineCompositeQ::e();
    for (int i = l; i < r; i++) {
        res = AffineCompositeQ::op(res, AffineCompositeQ::make(funcs[i].first, funcs[i].second));
    }
    return res;
}

Matrix2x2Q::S naive_matrix_product(const vector<Matrix2x2Q::S>& a, int l, int r) {
    Matrix2x2Q::S res = Matrix2x2Q::e();
    for (int i = l; i < r; i++) res = Matrix2x2Q::op(res, a[i]);
    return res;
}

unsigned long long naive_hash(const string& s, int l, int r) {
    unsigned long long h = 0;
    for (int i = l; i < r; i++) h = h * RollingHashQ::BASE + (unsigned long long)(unsigned char)s[i] + 1;
    return h;
}

long long naive_decimal_concat(const vector<int>& digits, int l, int r) {
    long long value = 0;
    for (int i = l; i < r; i++) value = (value * 10 + digits[i]) % DecimalConcatModQ::MOD;
    return value;
}

void test_empty_and_fixed_cases() {
    {
        RMQ::Seg seg(0);
        assert(seg.all_prod() == RMQ::e());
        assert(seg.prod(0, 0) == RMQ::e());
    }
    {
        RSQ::Seg seg(vector<long long>{5});
        assert(seg.prod(0, 1) == 5);
        seg.set(0, -3);
        assert(seg.all_prod() == -3);
    }
    {
        RLcmCapQ::Seg seg(vector<long long>{4000000000LL, 3999999999LL});
        assert(seg.all_prod() == RLcmCapQ::INF);
    }
    {
        vector<MaxSubarrayQ::S> v = {MaxSubarrayQ::make(-5), MaxSubarrayQ::make(-2), MaxSubarrayQ::make(-7)};
        MaxSubarrayQ::Seg seg(v);
        assert(seg.prod(0, 3).best == -2);
        assert(seg.prod(1, 1).best == MaxSubarrayQ::NEG);
    }
    {
        vector<BracketSeqQ::S> v;
        string s = "(()())";
        for (char c : s) v.push_back(BracketSeqQ::make(c));
        BracketSeqQ::Seg seg(v);
        assert(BracketSeqQ::is_valid(seg.prod(0, 6)));
        assert(!BracketSeqQ::is_valid(seg.prod(2, 5)));
    }
}

void test_numeric_random(mt19937& rng) {
    for (int tc = 0; tc < 120; tc++) {
        int n = (int)rand_ll(rng, 1, 30);
        vector<long long> a(n);
        for (int i = 0; i < n; i++) a[i] = rand_ll(rng, -20, 20);

        vector<int> ai(n);
        for (int i = 0; i < n; i++) ai[i] = (int)a[i];
        RMQ::Seg rmq(ai);
        RMaxQ::Seg rmaxq(ai);
        RSQ::Seg rsq(a);
        RGcdQ::Seg rgcd(a);
        RXorQ::Seg rxor(a);
        ROrQ::Seg ror(a);
        RAndQ::Seg randq(a);

        vector<RProdModQ::S> prod_v(n);
        vector<RLcmCapQ::S> lcm_v(n);
        for (int i = 0; i < n; i++) {
            prod_v[i] = RProdModQ::make(a[i]);
            lcm_v[i] = RLcmCapQ::make(a[i]);
        }
        RProdModQ::Seg rprod(prod_v);
        RLcmCapQ::Seg rlcm(lcm_v);

        for (int step = 0; step < 180; step++) {
            if (rand_ll(rng, 0, 3) == 0) {
                int p = (int)rand_ll(rng, 0, n - 1);
                long long x = rand_ll(rng, -20, 20);
                a[p] = x;
                ai[p] = (int)x;
                rmq.set(p, ai[p]);
                rmaxq.set(p, ai[p]);
                rsq.set(p, x);
                rgcd.set(p, x);
                rxor.set(p, x);
                ror.set(p, x);
                randq.set(p, x);
                rprod.set(p, RProdModQ::make(x));
                rlcm.set(p, RLcmCapQ::make(x));
            } else {
                int l = (int)rand_ll(rng, 0, n);
                int r = (int)rand_ll(rng, l, n);

                int mn = numeric_limits<int>::max();
                int mx = numeric_limits<int>::min();
                long long sum = 0;
                long long gcd_v = 0;
                long long xor_v = 0;
                long long or_v = 0;
                long long and_v = ~0LL;
                long long prod_v2 = 1;
                long long lcm_v2 = 1;
                for (int i = l; i < r; i++) {
                    mn = min(mn, ai[i]);
                    mx = max(mx, ai[i]);
                    sum += a[i];
                    gcd_v = std::gcd(gcd_v, a[i]);
                    xor_v ^= a[i];
                    or_v |= a[i];
                    and_v &= a[i];
                    prod_v2 = prod_v2 * RProdModQ::make(a[i]) % RProdModQ::MOD;
                    lcm_v2 = naive_lcm_cap(lcm_v2, a[i]);
                }
                assert(rmq.prod(l, r) == mn);
                assert(rmaxq.prod(l, r) == mx);
                assert(rsq.prod(l, r) == sum);
                assert(rgcd.prod(l, r) == gcd_v);
                assert(rxor.prod(l, r) == xor_v);
                assert(ror.prod(l, r) == or_v);
                assert(randq.prod(l, r) == and_v);
                assert(rprod.prod(l, r) == prod_v2);
                assert(rlcm.prod(l, r) == lcm_v2);
            }
        }
    }
}

void test_struct_random(mt19937& rng) {
    for (int tc = 0; tc < 120; tc++) {
        int n = (int)rand_ll(rng, 1, 30);
        vector<int> a(n);
        for (int i = 0; i < n; i++) a[i] = (int)rand_ll(rng, -8, 8);

        vector<MinCountQ::S> min_count_v(n);
        vector<MaxCountQ::S> max_count_v(n);
        vector<ArgMinQ::S> argmin_v(n);
        vector<ArgMaxQ::S> argmax_v(n);
        vector<MinMaxQ::S> minmax_v(n);
        vector<Top2MaxQ::S> top2max_v(n);
        vector<Top2MinQ::S> top2min_v(n);
        for (int i = 0; i < n; i++) {
            min_count_v[i] = MinCountQ::make(a[i]);
            max_count_v[i] = MaxCountQ::make(a[i]);
            argmin_v[i] = ArgMinQ::make(a[i], i);
            argmax_v[i] = ArgMaxQ::make(a[i], i);
            minmax_v[i] = MinMaxQ::make(a[i]);
            top2max_v[i] = Top2MaxQ::make(a[i]);
            top2min_v[i] = Top2MinQ::make(a[i]);
        }
        MinCountQ::Seg min_count_seg(min_count_v);
        MaxCountQ::Seg max_count_seg(max_count_v);
        ArgMinQ::Seg argmin_seg(argmin_v);
        ArgMaxQ::Seg argmax_seg(argmax_v);
        MinMaxQ::Seg minmax_seg(minmax_v);
        Top2MaxQ::Seg top2max_seg(top2max_v);
        Top2MinQ::Seg top2min_seg(top2min_v);

        for (int step = 0; step < 180; step++) {
            if (rand_ll(rng, 0, 3) == 0) {
                int p = (int)rand_ll(rng, 0, n - 1);
                int x = (int)rand_ll(rng, -8, 8);
                a[p] = x;
                min_count_seg.set(p, MinCountQ::make(x));
                max_count_seg.set(p, MaxCountQ::make(x));
                argmin_seg.set(p, ArgMinQ::make(x, p));
                argmax_seg.set(p, ArgMaxQ::make(x, p));
                minmax_seg.set(p, MinMaxQ::make(x));
                top2max_seg.set(p, Top2MaxQ::make(x));
                top2min_seg.set(p, Top2MinQ::make(x));
            } else {
                int l = (int)rand_ll(rng, 0, n);
                int r = (int)rand_ll(rng, l, n);

                auto min_count = min_count_seg.prod(l, r);
                auto max_count = max_count_seg.prod(l, r);
                auto argmin = argmin_seg.prod(l, r);
                auto argmax = argmax_seg.prod(l, r);
                auto minmax = minmax_seg.prod(l, r);
                auto top2max = top2max_seg.prod(l, r);
                auto top2min = top2min_seg.prod(l, r);
                auto n_min_count = naive_min_count(a, l, r);
                auto n_max_count = naive_max_count(a, l, r);
                auto n_argmin = naive_argmin(a, l, r);
                auto n_argmax = naive_argmax(a, l, r);
                auto n_minmax = naive_minmax(a, l, r);
                auto n_top2max = naive_top2max(a, l, r);
                auto n_top2min = naive_top2min(a, l, r);

                assert(pair(min_count.value, min_count.count) == n_min_count);
                assert(pair(max_count.value, max_count.count) == n_max_count);
                assert(pair(argmin.value, argmin.index) == n_argmin);
                assert(pair(argmax.value, argmax.index) == n_argmax);
                assert(pair(minmax.mn, minmax.mx) == n_minmax);
                assert(top2max.max1 == n_top2max.max1 && top2max.cnt1 == n_top2max.cnt1);
                assert(top2max.max2 == n_top2max.max2 && top2max.cnt2 == n_top2max.cnt2);
                assert(top2min.min1 == n_top2min.min1 && top2min.cnt1 == n_top2min.cnt1);
                assert(top2min.min2 == n_top2min.min2 && top2min.cnt2 == n_top2min.cnt2);
            }
        }
    }
}

void test_search_random(mt19937& rng) {
    for (int tc = 0; tc < 80; tc++) {
        int n = (int)rand_ll(rng, 1, 40);
        vector<int> a(n);
        vector<long long> nonneg(n);
        for (int i = 0; i < n; i++) {
            a[i] = (int)rand_ll(rng, -10, 10);
            nonneg[i] = rand_ll(rng, 0, 5);
        }
        RMQ::Seg rmq(a);
        RMaxQ::Seg rmaxq(a);
        RSQ::Seg rsq(nonneg);

        for (int step = 0; step < 100; step++) {
            int start = (int)rand_ll(rng, 0, n);
            int x = (int)rand_ll(rng, -12, 12);
            int first_less = n;
            int first_greater = n;
            for (int i = start; i < n; i++) {
                if (first_less == n && a[i] < x) first_less = i;
                if (first_greater == n && a[i] > x) first_greater = i;
            }
            assert(rmq.max_right(start, [&](int mn) { return mn >= x; }) == first_less);
            assert(rmaxq.max_right(start, [&](int mx) { return mx <= x; }) == first_greater);

            long long target = rand_ll(rng, 1, 120);
            long long sum = 0;
            int pos = n;
            for (int i = 0; i < n; i++) {
                sum += nonneg[i];
                if (sum >= target) {
                    pos = i;
                    break;
                }
            }
            assert(rsq.max_right(0, [&](long long s) { return s < target; }) == pos);
        }
    }
}

void test_presence_mex_random(mt19937& rng) {
    for (int tc = 0; tc < 80; tc++) {
        int n = (int)rand_ll(rng, 0, 40);
        vector<int> count(n, 0);
        for (int i = 0; i < n * 2 + 5; i++) {
            if (n == 0) break;
            count[(int)rand_ll(rng, 0, n - 1)]++;
        }
        vector<int> present(n);
        for (int i = 0; i < n; i++) present[i] = count[i] > 0 ? 1 : 0;
        PresenceMexQ::Seg seg(present);

        for (int step = 0; step < 160; step++) {
            int type = (int)rand_ll(rng, 0, 2);
            int x = n == 0 ? 0 : (int)rand_ll(rng, 0, n - 1);
            if (type == 0 && n > 0) {
                if (count[x] == 0) seg.set(x, 1);
                count[x]++;
            } else if (type == 1 && n > 0) {
                if (count[x] > 0) {
                    count[x]--;
                    if (count[x] == 0) seg.set(x, 0);
                }
            } else {
                int expected = n;
                for (int i = 0; i < n; i++) {
                    if (count[i] == 0) {
                        expected = i;
                        break;
                    }
                }
                int actual = seg.max_right(0, [](int mn) { return mn == 1; });
                assert(actual == expected);
            }
        }
    }
}

void test_sequence_random(mt19937& rng) {
    for (int tc = 0; tc < 100; tc++) {
        int n = (int)rand_ll(rng, 1, 35);
        string brackets(n, '(');
        vector<int> bits(n);
        vector<long long> values(n);
        for (int i = 0; i < n; i++) {
            brackets[i] = rand_ll(rng, 0, 1) ? '(' : ')';
            bits[i] = (int)rand_ll(rng, 0, 1);
            values[i] = rand_ll(rng, -20, 20);
        }

        vector<BracketSeqQ::S> bracket_v(n);
        vector<BinaryInversionQ::S> inv_v(n);
        vector<LongestOneQ::S> longest_v(n);
        vector<MaxSubarrayQ::S> maxsub_v(n);
        for (int i = 0; i < n; i++) {
            bracket_v[i] = BracketSeqQ::make(brackets[i]);
            inv_v[i] = BinaryInversionQ::make(bits[i]);
            longest_v[i] = LongestOneQ::make(bits[i]);
            maxsub_v[i] = MaxSubarrayQ::make(values[i]);
        }
        BracketSeqQ::Seg bracket_seg(bracket_v);
        BinaryInversionQ::Seg inv_seg(inv_v);
        LongestOneQ::Seg longest_seg(longest_v);
        MaxSubarrayQ::Seg maxsub_seg(maxsub_v);

        for (int step = 0; step < 160; step++) {
            if (rand_ll(rng, 0, 4) == 0) {
                int p = (int)rand_ll(rng, 0, n - 1);
                brackets[p] = rand_ll(rng, 0, 1) ? '(' : ')';
                bits[p] = (int)rand_ll(rng, 0, 1);
                values[p] = rand_ll(rng, -20, 20);
                bracket_seg.set(p, BracketSeqQ::make(brackets[p]));
                inv_seg.set(p, BinaryInversionQ::make(bits[p]));
                longest_seg.set(p, LongestOneQ::make(bits[p]));
                maxsub_seg.set(p, MaxSubarrayQ::make(values[p]));
            } else {
                int l = (int)rand_ll(rng, 0, n);
                int r = (int)rand_ll(rng, l, n);
                assert(BracketSeqQ::is_valid(bracket_seg.prod(l, r)) == naive_bracket_valid(brackets, l, r));
                assert(inv_seg.prod(l, r).inv == naive_binary_inv(bits, l, r));
                assert(longest_seg.prod(l, r).best == naive_longest_one(bits, l, r));
                assert(maxsub_seg.prod(l, r).best == naive_max_subarray(values, l, r));
            }
        }
    }
}

void test_algebra_random(mt19937& rng) {
    for (int tc = 0; tc < 100; tc++) {
        int n = (int)rand_ll(rng, 1, 30);
        vector<pair<long long, long long>> funcs(n);
        vector<Matrix2x2Q::S> mats(n);
        string s(n, 'a');
        vector<int> digits(n);
        for (int i = 0; i < n; i++) {
            funcs[i] = {rand_ll(rng, -5, 5), rand_ll(rng, -5, 5)};
            mats[i] = Matrix2x2Q::make(rand_ll(rng, -5, 5), rand_ll(rng, -5, 5), rand_ll(rng, -5, 5), rand_ll(rng, -5, 5));
            s[i] = char('a' + rand_ll(rng, 0, 3));
            digits[i] = (int)rand_ll(rng, 0, 9);
        }

        vector<AffineCompositeQ::S> affine_v(n);
        vector<RollingHashQ::S> hash_v(n);
        vector<DecimalConcatModQ::S> decimal_v(n);
        for (int i = 0; i < n; i++) {
            affine_v[i] = AffineCompositeQ::make(funcs[i].first, funcs[i].second);
            hash_v[i] = RollingHashQ::make(s[i]);
            decimal_v[i] = DecimalConcatModQ::make_digit(digits[i]);
        }
        AffineCompositeQ::Seg affine_seg(affine_v);
        Matrix2x2Q::Seg matrix_seg(mats);
        RollingHashQ::Seg hash_seg(hash_v);
        DecimalConcatModQ::Seg decimal_seg(decimal_v);

        for (int step = 0; step < 160; step++) {
            if (rand_ll(rng, 0, 4) == 0) {
                int p = (int)rand_ll(rng, 0, n - 1);
                funcs[p] = {rand_ll(rng, -5, 5), rand_ll(rng, -5, 5)};
                mats[p] = Matrix2x2Q::make(rand_ll(rng, -5, 5), rand_ll(rng, -5, 5), rand_ll(rng, -5, 5), rand_ll(rng, -5, 5));
                s[p] = char('a' + rand_ll(rng, 0, 3));
                digits[p] = (int)rand_ll(rng, 0, 9);
                affine_seg.set(p, AffineCompositeQ::make(funcs[p].first, funcs[p].second));
                matrix_seg.set(p, mats[p]);
                hash_seg.set(p, RollingHashQ::make(s[p]));
                decimal_seg.set(p, DecimalConcatModQ::make_digit(digits[p]));
            } else {
                int l = (int)rand_ll(rng, 0, n);
                int r = (int)rand_ll(rng, l, n);
                long long x = rand_ll(rng, -20, 20);
                auto affine_expected = naive_affine_compose(funcs, l, r);
                auto affine_actual = affine_seg.prod(l, r);
                assert(affine_actual.a == affine_expected.a && affine_actual.b == affine_expected.b);
                assert(AffineCompositeQ::apply(affine_actual, x) == AffineCompositeQ::apply(affine_expected, x));
                assert(eq_matrix(matrix_seg.prod(l, r), naive_matrix_product(mats, l, r)));
                assert(hash_seg.prod(l, r).hash == naive_hash(s, l, r));
                assert(decimal_seg.prod(l, r).value == naive_decimal_concat(digits, l, r));
            }
        }
    }
}

void test_solvers_fixed() {
    assert(solve_rmq_point_set_range_min({3, 1, 4}, {{1, 0, 3}, {0, 1, 5}, {1, 0, 3}}) == vector<int>({1, 3}));
    assert(solve_first_less_than_x({5, 4, 2, 7}, {{0, 3}, {2, 3}, {3, 10}}) == vector<int>({2, 2, 3}));
    assert(solve_rmaxq_point_set_range_max({3, 1, 4}, {{1, 0, 3}, {0, 2, -1}, {1, 0, 3}}) == vector<int>({4, 3}));
    assert(solve_first_greater_than_x({1, 3, 2, 5}, {{0, 2}, {2, 4}, {4, 0}}) == vector<int>({1, 3, 4}));
    assert(solve_rsq_point_add_range_sum({1, 2, 3}, {{1, 0, 3, 0}, {0, 1, 0, 5}, {1, 0, 2, 0}}) == vector<long long>({6, 8}));
    assert(solve_prefix_lower_bound_sum({2, 0, 3, 4}, {1, 2, 3, 6, 10}) == vector<int>({0, 0, 2, 3, 4}));
    assert(solve_kth_one({0, 1, 0, 1, 1}, {0, 1, 2, 3}) == vector<int>({1, 3, 4, 5}));
    assert(solve_rprod_mod_point_set_range_product({2, -3, 4}, {{1, 0, 3, 0}, {0, 1, 0, 5}, {1, 0, 3, 0}}) == vector<long long>({RProdModQ::make(-24), 40}));
    assert(solve_rgcd_point_set_range_gcd({12, 18, 30}, {{1, 0, 3, 0}, {0, 1, 0, 7}, {1, 0, 3, 0}}) == vector<long long>({6, 1}));
    assert(solve_rlcm_cap_point_set_range_lcm({2, 3, 4}, {{1, 0, 3, 0}, {0, 1, 0, 6}, {1, 0, 3, 0}}) == vector<long long>({12, 12}));
    assert(solve_rxor_point_set_range_xor({1, 2, 3}, {{1, 0, 3, 0}, {0, 1, 0, 7}, {1, 0, 3, 0}}) == vector<long long>({0, 5}));
    assert(solve_ror_point_set_range_or({1, 2, 4}, {{1, 0, 3, 0}, {0, 1, 0, 8}, {1, 0, 2, 0}}) == vector<long long>({7, 9}));
    assert(solve_rand_point_set_range_and({7, 3, 6}, {{1, 0, 3, 0}, {0, 1, 0, 5}, {1, 0, 2, 0}}) == vector<long long>({2, 5}));
    assert((solve_min_count_point_set_range({2, 1, 1, 3}, {{1, 0, 4, 0}, {0, 1, 0, 4}, {1, 0, 4, 0}}) == vector<pair<int, int>>({{1, 2}, {1, 1}})));
    assert((solve_max_count_point_set_range({2, 4, 4, 3}, {{1, 0, 4, 0}, {0, 2, 0, 1}, {1, 0, 4, 0}}) == vector<pair<int, int>>({{4, 2}, {4, 1}})));
    assert((solve_argmin_point_set_range({2, 1, 1, 3}, {{1, 0, 4, 0}, {0, 1, 0, 5}, {1, 0, 4, 0}}) == vector<pair<int, int>>({{1, 1}, {1, 2}})));
    assert((solve_argmax_point_set_range({2, 4, 4, 3}, {{1, 0, 4, 0}, {0, 1, 0, 1}, {1, 0, 4, 0}}) == vector<pair<int, int>>({{4, 1}, {4, 2}})));
    assert((solve_minmax_point_set_range({2, 4, -1, 3}, {{1, 0, 4, 0}, {0, 2, 0, 10}, {1, 1, 4, 0}}) == vector<pair<int, int>>({{-1, 4}, {3, 10}})));
    assert((solve_top2max_point_set_range({5, 3, 5, 2}, {{1, 0, 4, 0}, {0, 1, 0, 4}, {1, 0, 4, 0}}) == vector<pair<int, int>>({{3, 1}, {4, 1}})));
    assert((solve_top2min_point_set_range({1, 3, 1, 4}, {{1, 0, 4, 0}, {0, 1, 0, 2}, {1, 0, 4, 0}}) == vector<pair<int, int>>({{3, 1}, {2, 1}})));
    assert(solve_dynamic_mex(5, {0, 1, 1, 3}, {{2, 0}, {0, 2}, {2, 0}, {1, 1}, {1, 1}, {2, 0}}) == vector<int>({2, 4, 1}));
    assert(solve_bracket_sequence_queries("())(", {{1, 0, 4, 0}, {0, 1, 0, '('}, {0, 3, 0, ')'}, {1, 0, 4, 0}}) == vector<int>({0, 1}));
    assert(solve_binary_inversion_queries({1, 0, 1, 0}, {{1, 0, 4, 0}, {0, 1, 0, 1}, {1, 0, 4, 0}}) == vector<long long>({3, 3}));
    assert(solve_longest_one_queries({1, 1, 0, 1}, {{1, 0, 4, 0}, {0, 2, 0, 1}, {1, 0, 4, 0}}) == vector<int>({2, 4}));
    assert(solve_max_subarray_queries({-2, 3, -1, 4}, {{1, 0, 4, 0}, {0, 1, 0, -5}, {1, 0, 4, 0}}) == vector<long long>({6, 4}));
    assert(solve_affine_composite_queries({{2, 1}, {3, 4}}, {{1, 0, 0, 2, 0, 0, 5}, {0, 1, 0, 0, 1, 0, 0}, {1, 0, 0, 2, 0, 0, 5}}) == vector<long long>({37, 11}));

    vector<Matrix2x2Q::S> mats = {Matrix2x2Q::make(1, 1, 1, 0), Matrix2x2Q::make(1, 1, 1, 0)};
    auto matrix_ans = solve_matrix2x2_product_queries(mats, {{1, 0, 0, 2, Matrix2x2Q::e()}, {0, 1, 0, 0, Matrix2x2Q::make(1, 0, 0, 1)}, {1, 0, 0, 2, Matrix2x2Q::e()}});
    assert(eq_matrix(matrix_ans[0], Matrix2x2Q::make(2, 1, 1, 1)));
    assert(eq_matrix(matrix_ans[1], Matrix2x2Q::make(1, 1, 1, 0)));

    auto h = solve_rolling_hash_queries("abc", {{1, 0, 2, 0, 0}, {0, 0, 0, 1, 'a'}, {1, 0, 2, 0, 0}});
    assert(h[0] == naive_hash("abc", 0, 2));
    assert(h[1] == naive_hash("aac", 0, 2));
    assert(solve_decimal_concat_queries({1, 2, 3}, {{1, 0, 3, 0, 0}, {0, 0, 0, 1, 9}, {1, 0, 3, 0, 0}}) == vector<long long>({123, 193}));
}

int main() {
    mt19937 rng(123456789);
    test_empty_and_fixed_cases();
    test_numeric_random(rng);
    test_struct_random(rng);
    test_search_random(rng);
    test_presence_mex_random(rng);
    test_sequence_random(rng);
    test_algebra_random(rng);
    test_solvers_fixed();
    cout << "All tests passed\n";
    return 0;
}

#endif
