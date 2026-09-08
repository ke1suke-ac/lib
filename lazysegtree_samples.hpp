#pragma once
#include <bits/stdc++.h>
#include <atcoder/lazysegtree>
using namespace std;

/*
 * ACL の atcoder::lazy_segtree に渡す用途別モノイド・作用集
 * 各 namespace は単体でコピーしやすいよう、S, F, op, e, mapping, composition, id, 必要最小限のヘルパー, Seg を含む
 * 各 namespace の直後に、そのデータ構造を使うユースケースソルバー関数を置く
 */

// 区間加算と区間和を管理する。値と区間長を持ち、加算を和に反映する
namespace AddSumQ {
    using ll = long long;

    struct S {
        ll sum;
        int len;
    };

    using F = ll;

    inline S op(S a, S b) {
        return {a.sum + b.sum, a.len + b.len};
    }

    inline S e() {
        return {0, 0};
    }

    inline S mapping(F f, S x) {
        return {x.sum + f * x.len, x.len};
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    inline S make(ll value) {
        return {value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の和を返す O((N+Q)logN)
vector<long long> solve_add_sum(vector<long long> a, const vector<AddSumQuery>& queries) {
    vector<AddSumQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddSumQ::make(a[i]);
    AddSumQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r).sum);
        }
    }
    return ans;
}

struct AddSumPrefixLowerBoundQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: sum[0,i+1) >= x となる最小iを返す。全要素非負が保たれる前提 O((N+Q)logN)
vector<int> solve_add_sum_prefix_lower_bound(vector<long long> a, const vector<AddSumPrefixLowerBoundQuery>& queries) {
    vector<AddSumQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddSumQ::make(a[i]);
    AddSumQ::Seg seg(init);

    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else if (q.x <= 0) {
            ans.push_back(0);
        } else {
            int pos = seg.max_right(0, [&](AddSumQ::S s) {
                return s.sum < q.x;
            });
            ans.push_back(pos);
        }
    }
    return ans;
}

// 区間加算と区間最小値を管理する。最初に閾値未満になる位置の探索にも使える
namespace AddMinQ {
    using S = long long;
    using F = long long;
    constexpr long long INF = (1LL << 62);

    inline S op(S a, S b) {
        return min(a, b);
    }

    inline S e() {
        return INF;
    }

    inline S mapping(F f, S x) {
        if (x == INF) return INF;
        return x + f;
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddMinQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の最小値を返す O((N+Q)logN)
vector<long long> solve_add_min(vector<long long> a, const vector<AddMinQuery>& queries) {
    AddMinQ::Seg seg(a);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

struct AddFirstLessThanQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: l以降で初めて値がx未満になる位置を返す O((N+Q)logN)
vector<int> solve_add_first_less_than(vector<long long> a, const vector<AddFirstLessThanQuery>& queries) {
    AddMinQ::Seg seg(a);
    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            int pos = seg.max_right(q.l, [&](long long mn) {
                return mn >= q.x;
            });
            ans.push_back(pos);
        }
    }
    return ans;
}

// 区間加算と区間最大値を管理する。最初に閾値超過になる位置の探索にも使える
namespace AddMaxQ {
    using S = long long;
    using F = long long;
    constexpr long long NEG_INF = -(1LL << 62);

    inline S op(S a, S b) {
        return max(a, b);
    }

    inline S e() {
        return NEG_INF;
    }

    inline S mapping(F f, S x) {
        if (x == NEG_INF) return NEG_INF;
        return x + f;
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddMaxQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の最大値を返す O((N+Q)logN)
vector<long long> solve_add_max(vector<long long> a, const vector<AddMaxQuery>& queries) {
    AddMaxQ::Seg seg(a);
    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            ans.push_back(seg.prod(q.l, q.r));
        }
    }
    return ans;
}

struct AddFirstGreaterThanQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: l以降で初めて値がx超過になる位置を返す O((N+Q)logN)
vector<int> solve_add_first_greater_than(vector<long long> a, const vector<AddFirstGreaterThanQuery>& queries) {
    AddMaxQ::Seg seg(a);
    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            int pos = seg.max_right(q.l, [&](long long mx) {
                return mx <= q.x;
            });
            ans.push_back(pos);
        }
    }
    return ans;
}

// 区間加算と区間最小値・最大値を同時に管理する
namespace AddMinMaxQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);
    constexpr ll NEG_INF = -(1LL << 62);

    struct S {
        ll mn;
        ll mx;
        int len;
    };

    using F = ll;

    inline S op(S a, S b) {
        return {min(a.mn, b.mn), max(a.mx, b.mx), a.len + b.len};
    }

    inline S e() {
        return {INF, NEG_INF, 0};
    }

    inline S mapping(F f, S x) {
        if (x.len == 0) return x;
        return {x.mn + f, x.mx + f, x.len};
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    inline S make(ll value) {
        return {value, value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddMinMaxQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の最小値と最大値を返す O((N+Q)logN)
vector<pair<long long, long long>> solve_add_min_max(vector<long long> a, const vector<AddMinMaxQuery>& queries) {
    vector<AddMinMaxQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddMinMaxQ::make(a[i]);
    AddMinMaxQ::Seg seg(init);

    vector<pair<long long, long long>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.mn, res.mx});
        }
    }
    return ans;
}

// 区間加算と区間最小値・その個数を管理する
namespace AddMinCountQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);

    struct S {
        ll value;
        int count;
    };

    using F = ll;

    inline S op(S a, S b) {
        if (a.value < b.value) return a;
        if (a.value > b.value) return b;
        return {a.value, a.count + b.count};
    }

    inline S e() {
        return {INF, 0};
    }

    inline S mapping(F f, S x) {
        if (x.count == 0) return x;
        return {x.value + f, x.count};
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    inline S make(ll value) {
        return {value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddMinCountQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の最小値と個数を返す O((N+Q)logN)
vector<pair<long long, int>> solve_add_min_count(vector<long long> a, const vector<AddMinCountQuery>& queries) {
    vector<AddMinCountQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddMinCountQ::make(a[i]);
    AddMinCountQ::Seg seg(init);

    vector<pair<long long, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.count});
        }
    }
    return ans;
}

// 区間加算と区間最大値・その個数を管理する
namespace AddMaxCountQ {
    using ll = long long;
    constexpr ll NEG_INF = -(1LL << 62);

    struct S {
        ll value;
        int count;
    };

    using F = ll;

    inline S op(S a, S b) {
        if (a.value > b.value) return a;
        if (a.value < b.value) return b;
        return {a.value, a.count + b.count};
    }

    inline S e() {
        return {NEG_INF, 0};
    }

    inline S mapping(F f, S x) {
        if (x.count == 0) return x;
        return {x.value + f, x.count};
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    inline S make(ll value) {
        return {value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddMaxCountQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の最大値と個数を返す O((N+Q)logN)
vector<pair<long long, int>> solve_add_max_count(vector<long long> a, const vector<AddMaxCountQuery>& queries) {
    vector<AddMaxCountQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddMaxCountQ::make(a[i]);
    AddMaxCountQ::Seg seg(init);

    vector<pair<long long, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.count});
        }
    }
    return ans;
}

// 区間加算と区間最小値・最小indexを管理する。同値では最小indexを返す
namespace AddArgMinQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);
    constexpr int INF_INDEX = numeric_limits<int>::max();

    struct S {
        ll value;
        int index;
    };

    using F = ll;

    inline S op(S a, S b) {
        if (a.value < b.value) return a;
        if (a.value > b.value) return b;
        return (a.index <= b.index ? a : b);
    }

    inline S e() {
        return {INF, INF_INDEX};
    }

    inline S mapping(F f, S x) {
        if (x.index == INF_INDEX) return x;
        return {x.value + f, x.index};
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    inline S make(ll value, int index) {
        return {value, index};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddArgMinQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の最小値と最小indexを返す O((N+Q)logN)
vector<pair<long long, int>> solve_add_argmin(vector<long long> a, const vector<AddArgMinQuery>& queries) {
    vector<AddArgMinQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddArgMinQ::make(a[i], i);
    AddArgMinQ::Seg seg(init);

    vector<pair<long long, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.index});
        }
    }
    return ans;
}

// 区間加算と区間最大値・最小indexを管理する。同値では最小indexを返す
namespace AddArgMaxQ {
    using ll = long long;
    constexpr ll NEG_INF = -(1LL << 62);
    constexpr int INF_INDEX = numeric_limits<int>::max();

    struct S {
        ll value;
        int index;
    };

    using F = ll;

    inline S op(S a, S b) {
        if (a.value > b.value) return a;
        if (a.value < b.value) return b;
        return (a.index <= b.index ? a : b);
    }

    inline S e() {
        return {NEG_INF, INF_INDEX};
    }

    inline S mapping(F f, S x) {
        if (x.index == INF_INDEX) return x;
        return {x.value + f, x.index};
    }

    inline F composition(F f, F g) {
        return f + g;
    }

    inline F id() {
        return 0;
    }

    inline S make(ll value, int index) {
        return {value, index};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddArgMaxQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) の最大値と最小indexを返す O((N+Q)logN)
vector<pair<long long, int>> solve_add_argmax(vector<long long> a, const vector<AddArgMaxQuery>& queries) {
    vector<AddArgMaxQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddArgMaxQ::make(a[i], i);
    AddArgMaxQ::Seg seg(init);

    vector<pair<long long, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, q.x);
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.index});
        }
    }
    return ans;
}

// 区間代入と区間和を管理する。代入なしをFのhas_valueで表す
namespace SetSumQ {
    using ll = long long;

    struct S {
        ll sum;
        int len;
    };

    struct F {
        bool has_value;
        ll value;
    };

    inline S op(S a, S b) {
        return {a.sum + b.sum, a.len + b.len};
    }

    inline S e() {
        return {0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f.has_value || x.len == 0) return x;
        return {f.value * x.len, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_value) return f;
        return g;
    }

    inline F id() {
        return {false, 0};
    }

    inline F set_value(ll value) {
        return {true, value};
    }

    inline S make(ll value) {
        return {value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct SetSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) をxに代入、type=1: [l,r) の和を返す O((N+Q)logN)
vector<long long> solve_set_sum(vector<long long> a, const vector<SetSumQuery>& queries) {
    vector<SetSumQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = SetSumQ::make(a[i]);
    SetSumQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, SetSumQ::set_value(q.x));
        } else {
            ans.push_back(seg.prod(q.l, q.r).sum);
        }
    }
    return ans;
}

// 区間代入と区間最小値を管理する
namespace SetMinQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);

    struct S {
        ll value;
        int len;
    };

    struct F {
        bool has_value;
        ll value;
    };

    inline S op(S a, S b) {
        return {min(a.value, b.value), a.len + b.len};
    }

    inline S e() {
        return {INF, 0};
    }

    inline S mapping(F f, S x) {
        if (!f.has_value || x.len == 0) return x;
        return {f.value, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_value) return f;
        return g;
    }

    inline F id() {
        return {false, 0};
    }

    inline F set_value(ll value) {
        return {true, value};
    }

    inline S make(ll value) {
        return {value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct SetMinQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) をxに代入、type=1: [l,r) の最小値を返す O((N+Q)logN)
vector<long long> solve_set_min(vector<long long> a, const vector<SetMinQuery>& queries) {
    vector<SetMinQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = SetMinQ::make(a[i]);
    SetMinQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, SetMinQ::set_value(q.x));
        } else {
            ans.push_back(seg.prod(q.l, q.r).value);
        }
    }
    return ans;
}

// 区間代入と区間最大値を管理する
namespace SetMaxQ {
    using ll = long long;
    constexpr ll NEG_INF = -(1LL << 62);

    struct S {
        ll value;
        int len;
    };

    struct F {
        bool has_value;
        ll value;
    };

    inline S op(S a, S b) {
        return {max(a.value, b.value), a.len + b.len};
    }

    inline S e() {
        return {NEG_INF, 0};
    }

    inline S mapping(F f, S x) {
        if (!f.has_value || x.len == 0) return x;
        return {f.value, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_value) return f;
        return g;
    }

    inline F id() {
        return {false, 0};
    }

    inline F set_value(ll value) {
        return {true, value};
    }

    inline S make(ll value) {
        return {value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct SetMaxQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) をxに代入、type=1: [l,r) の最大値を返す O((N+Q)logN)
vector<long long> solve_set_max(vector<long long> a, const vector<SetMaxQuery>& queries) {
    vector<SetMaxQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = SetMaxQ::make(a[i]);
    SetMaxQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, SetMaxQ::set_value(q.x));
        } else {
            ans.push_back(seg.prod(q.l, q.r).value);
        }
    }
    return ans;
}

// 区間代入と区間最小値・最大値を同時に管理する
namespace SetMinMaxQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);
    constexpr ll NEG_INF = -(1LL << 62);

    struct S {
        ll mn;
        ll mx;
        int len;
    };

    struct F {
        bool has_value;
        ll value;
    };

    inline S op(S a, S b) {
        return {min(a.mn, b.mn), max(a.mx, b.mx), a.len + b.len};
    }

    inline S e() {
        return {INF, NEG_INF, 0};
    }

    inline S mapping(F f, S x) {
        if (!f.has_value || x.len == 0) return x;
        return {f.value, f.value, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_value) return f;
        return g;
    }

    inline F id() {
        return {false, 0};
    }

    inline F set_value(ll value) {
        return {true, value};
    }

    inline S make(ll value) {
        return {value, value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct SetMinMaxQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) をxに代入、type=1: [l,r) の最小値と最大値を返す O((N+Q)logN)
vector<pair<long long, long long>> solve_set_min_max(vector<long long> a, const vector<SetMinMaxQuery>& queries) {
    vector<SetMinMaxQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = SetMinMaxQ::make(a[i]);
    SetMinMaxQ::Seg seg(init);

    vector<pair<long long, long long>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, SetMinMaxQ::set_value(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.mn, res.mx});
        }
    }
    return ans;
}

// 区間代入と区間最小値・その個数を管理する
namespace SetMinCountQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);

    struct S {
        ll value;
        int count;
        int len;
    };

    struct F {
        bool has_value;
        ll value;
    };

    inline S op(S a, S b) {
        if (a.value < b.value) return {a.value, a.count, a.len + b.len};
        if (a.value > b.value) return {b.value, b.count, a.len + b.len};
        return {a.value, a.count + b.count, a.len + b.len};
    }

    inline S e() {
        return {INF, 0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f.has_value || x.len == 0) return x;
        return {f.value, x.len, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_value) return f;
        return g;
    }

    inline F id() {
        return {false, 0};
    }

    inline F set_value(ll value) {
        return {true, value};
    }

    inline S make(ll value) {
        return {value, 1, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct SetMinCountQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) をxに代入、type=1: [l,r) の最小値と個数を返す O((N+Q)logN)
vector<pair<long long, int>> solve_set_min_count(vector<long long> a, const vector<SetMinCountQuery>& queries) {
    vector<SetMinCountQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = SetMinCountQ::make(a[i]);
    SetMinCountQ::Seg seg(init);

    vector<pair<long long, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, SetMinCountQ::set_value(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.count});
        }
    }
    return ans;
}

// 区間代入と区間最大値・その個数を管理する
namespace SetMaxCountQ {
    using ll = long long;
    constexpr ll NEG_INF = -(1LL << 62);

    struct S {
        ll value;
        int count;
        int len;
    };

    struct F {
        bool has_value;
        ll value;
    };

    inline S op(S a, S b) {
        if (a.value > b.value) return {a.value, a.count, a.len + b.len};
        if (a.value < b.value) return {b.value, b.count, a.len + b.len};
        return {a.value, a.count + b.count, a.len + b.len};
    }

    inline S e() {
        return {NEG_INF, 0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f.has_value || x.len == 0) return x;
        return {f.value, x.len, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_value) return f;
        return g;
    }

    inline F id() {
        return {false, 0};
    }

    inline F set_value(ll value) {
        return {true, value};
    }

    inline S make(ll value) {
        return {value, 1, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct SetMaxCountQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) をxに代入、type=1: [l,r) の最大値と個数を返す O((N+Q)logN)
vector<pair<long long, int>> solve_set_max_count(vector<long long> a, const vector<SetMaxCountQuery>& queries) {
    vector<SetMaxCountQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = SetMaxCountQ::make(a[i]);
    SetMaxCountQ::Seg seg(init);

    vector<pair<long long, int>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, SetMaxCountQ::set_value(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.value, res.count});
        }
    }
    return ans;
}

// 区間加算・区間代入と区間和を管理する。Fは先に代入してから加算する関数を表す
namespace AddSetSumQ {
    using ll = long long;

    struct S {
        ll sum;
        int len;
    };

    struct F {
        bool has_set;
        ll set_value;
        ll add_value;
    };

    inline S op(S a, S b) {
        return {a.sum + b.sum, a.len + b.len};
    }

    inline S e() {
        return {0, 0};
    }

    inline S mapping(F f, S x) {
        if (x.len == 0) return x;
        if (f.has_set) return {(f.set_value + f.add_value) * x.len, x.len};
        return {x.sum + f.add_value * x.len, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_set) return f;
        if (g.has_set) return {true, g.set_value, g.add_value + f.add_value};
        return {false, 0, g.add_value + f.add_value};
    }

    inline F id() {
        return {false, 0, 0};
    }

    inline F add(ll value) {
        return {false, 0, value};
    }

    inline F set_value(ll value) {
        return {true, value, 0};
    }

    inline S make(ll value) {
        return {value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddSetSumQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) をxに代入、type=2: [l,r) の和を返す O((N+Q)logN)
vector<long long> solve_add_set_sum(vector<long long> a, const vector<AddSetSumQuery>& queries) {
    vector<AddSetSumQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddSetSumQ::make(a[i]);
    AddSetSumQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, AddSetSumQ::add(q.x));
        } else if (q.type == 1) {
            seg.apply(q.l, q.r, AddSetSumQ::set_value(q.x));
        } else {
            ans.push_back(seg.prod(q.l, q.r).sum);
        }
    }
    return ans;
}

// 区間加算・区間代入と区間最小値・最大値を管理する
namespace AddSetMinMaxQ {
    using ll = long long;
    constexpr ll INF = (1LL << 62);
    constexpr ll NEG_INF = -(1LL << 62);

    struct S {
        ll mn;
        ll mx;
        int len;
    };

    struct F {
        bool has_set;
        ll set_value;
        ll add_value;
    };

    inline S op(S a, S b) {
        return {min(a.mn, b.mn), max(a.mx, b.mx), a.len + b.len};
    }

    inline S e() {
        return {INF, NEG_INF, 0};
    }

    inline S mapping(F f, S x) {
        if (x.len == 0) return x;
        if (f.has_set) {
            ll v = f.set_value + f.add_value;
            return {v, v, x.len};
        }
        return {x.mn + f.add_value, x.mx + f.add_value, x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_set) return f;
        if (g.has_set) return {true, g.set_value, g.add_value + f.add_value};
        return {false, 0, g.add_value + f.add_value};
    }

    inline F id() {
        return {false, 0, 0};
    }

    inline F add(ll value) {
        return {false, 0, value};
    }

    inline F set_value(ll value) {
        return {true, value, 0};
    }

    inline S make(ll value) {
        return {value, value, 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AddSetMinMaxQuery {
    int type;
    int l;
    int r;
    long long x;
};

// type=0: [l,r) にxを加算、type=1: [l,r) をxに代入、type=2: [l,r) の最小値と最大値を返す O((N+Q)logN)
vector<pair<long long, long long>> solve_add_set_min_max(vector<long long> a, const vector<AddSetMinMaxQuery>& queries) {
    vector<AddSetMinMaxQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AddSetMinMaxQ::make(a[i]);
    AddSetMinMaxQ::Seg seg(init);

    vector<pair<long long, long long>> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, AddSetMinMaxQ::add(q.x));
        } else if (q.type == 1) {
            seg.apply(q.l, q.r, AddSetMinMaxQ::set_value(q.x));
        } else {
            auto res = seg.prod(q.l, q.r);
            ans.push_back({res.mn, res.mx});
        }
    }
    return ans;
}

// 区間一次変換 x -> a*x+b と区間和を固定modで管理する
namespace AffineSumModQ {
    using ll = long long;
    constexpr ll MOD = 998244353;

    struct S {
        ll sum;
        int len;
    };

    struct F {
        ll a;
        ll b;
    };

    inline ll norm(ll x) {
        x %= MOD;
        if (x < 0) x += MOD;
        return x;
    }

    inline S op(S x, S y) {
        return {(x.sum + y.sum) % MOD, x.len + y.len};
    }

    inline S e() {
        return {0, 0};
    }

    inline S mapping(F f, S x) {
        return {(f.a * x.sum + f.b * x.len) % MOD, x.len};
    }

    inline F composition(F f, F g) {
        return {f.a * g.a % MOD, (f.a * g.b + f.b) % MOD};
    }

    inline F id() {
        return {1, 0};
    }

    inline F affine(ll a, ll b) {
        return {norm(a), norm(b)};
    }

    inline S make(ll value) {
        return {norm(value), 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct AffineSumModQuery {
    int type;
    int l;
    int r;
    long long a;
    long long b;
};

// type=0: [l,r) に x -> a*x+b を適用、type=1: [l,r) の和modを返す O((N+Q)logN)
vector<long long> solve_affine_sum_mod(vector<long long> a, const vector<AffineSumModQuery>& queries) {
    vector<AffineSumModQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = AffineSumModQ::make(a[i]);
    AffineSumModQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, AffineSumModQ::affine(q.a, q.b));
        } else {
            ans.push_back(seg.prod(q.l, q.r).sum);
        }
    }
    return ans;
}

// 30bit非負整数列の区間bit演算更新と区間和を管理する。xor, and, or, assignを同じ作用で扱う
namespace BitwiseSumQ {
    using ll = long long;
    constexpr int BITS = 30;
    constexpr int ALL = (1 << BITS) - 1;

    struct S {
        array<int, BITS> cnt;
        int len;
    };

    struct F {
        int keep;
        int flip;
    };

    inline S op(S a, S b) {
        S res{};
        res.len = a.len + b.len;
        for (int i = 0; i < BITS; i++) res.cnt[i] = a.cnt[i] + b.cnt[i];
        return res;
    }

    inline S e() {
        S res{};
        res.len = 0;
        return res;
    }

    inline S mapping(F f, S x) {
        S res = x;
        f.keep &= ALL;
        f.flip &= ALL;
        for (int i = 0; i < BITS; i++) {
            bool keep = ((f.keep >> i) & 1) != 0;
            bool flip = ((f.flip >> i) & 1) != 0;
            if (keep) {
                res.cnt[i] = flip ? x.len - x.cnt[i] : x.cnt[i];
            } else {
                res.cnt[i] = flip ? x.len : 0;
            }
        }
        return res;
    }

    inline F composition(F f, F g) {
        return {(g.keep & f.keep) & ALL, ((g.flip & f.keep) ^ f.flip) & ALL};
    }

    inline F id() {
        return {ALL, 0};
    }

    inline F xor_mask(int mask) {
        return {ALL, mask & ALL};
    }

    inline F and_mask(int mask) {
        return {mask & ALL, 0};
    }

    inline F or_mask(int mask) {
        mask &= ALL;
        return {ALL ^ mask, mask};
    }

    inline F set_value(int value) {
        return {0, value & ALL};
    }

    inline S make(int value) {
        S res{};
        res.len = 1;
        value &= ALL;
        for (int i = 0; i < BITS; i++) res.cnt[i] = (value >> i) & 1;
        return res;
    }

    inline ll value(S x) {
        ll res = 0;
        for (int i = 0; i < BITS; i++) res += (1LL << i) * x.cnt[i];
        return res;
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct BitwiseSumQuery {
    int type;
    int l;
    int r;
    int x;
};

// type=0: xor、type=1: and、type=2: or、type=3: assign、type=4: [l,r) の和を返す O((N+Q)logN)
vector<long long> solve_bitwise_sum(vector<int> a, const vector<BitwiseSumQuery>& queries) {
    vector<BitwiseSumQ::S> init(a.size());
    for (int i = 0; i < (int)a.size(); i++) init[i] = BitwiseSumQ::make(a[i]);
    BitwiseSumQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, BitwiseSumQ::xor_mask(q.x));
        } else if (q.type == 1) {
            seg.apply(q.l, q.r, BitwiseSumQ::and_mask(q.x));
        } else if (q.type == 2) {
            seg.apply(q.l, q.r, BitwiseSumQ::or_mask(q.x));
        } else if (q.type == 3) {
            seg.apply(q.l, q.r, BitwiseSumQ::set_value(q.x));
        } else {
            ans.push_back(BitwiseSumQ::value(seg.prod(q.l, q.r)));
        }
    }
    return ans;
}

// 01列の区間反転と区間内の1の個数を管理する
namespace FlipCountOneQ {
    struct S {
        int zero;
        int one;
    };

    using F = bool;

    inline S op(S a, S b) {
        return {a.zero + b.zero, a.one + b.one};
    }

    inline S e() {
        return {0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f) return x;
        return {x.one, x.zero};
    }

    inline F composition(F f, F g) {
        return f ^ g;
    }

    inline F id() {
        return false;
    }

    inline S make(int bit) {
        return bit ? S{0, 1} : S{1, 0};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct FlipCountOneQuery {
    int type;
    int l;
    int r;
};

// type=0: [l,r) を反転、type=1: [l,r) の1の個数を返す O((N+Q)logN)
vector<int> solve_flip_count_one(vector<int> bits, const vector<FlipCountOneQuery>& queries) {
    vector<FlipCountOneQ::S> init(bits.size());
    for (int i = 0; i < (int)bits.size(); i++) init[i] = FlipCountOneQ::make(bits[i]);
    FlipCountOneQ::Seg seg(init);

    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, true);
        } else {
            ans.push_back(seg.prod(q.l, q.r).one);
        }
    }
    return ans;
}

// 01列の区間反転と転倒数を管理する。転倒数はi<jかつa[i]=1,a[j]=0の組数
namespace FlipInversionQ {
    using ll = long long;

    struct S {
        ll zero;
        ll one;
        ll inv;
    };

    using F = bool;

    inline S op(S a, S b) {
        return {a.zero + b.zero, a.one + b.one, a.inv + b.inv + a.one * b.zero};
    }

    inline S e() {
        return {0, 0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f) return x;
        return {x.one, x.zero, x.zero * x.one - x.inv};
    }

    inline F composition(F f, F g) {
        return f ^ g;
    }

    inline F id() {
        return false;
    }

    inline S make(int bit) {
        return bit ? S{0, 1, 0} : S{1, 0, 0};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct FlipInversionQuery {
    int type;
    int l;
    int r;
};

// type=0: [l,r) を反転、type=1: [l,r) の転倒数を返す O((N+Q)logN)
vector<long long> solve_flip_inversion(vector<int> bits, const vector<FlipInversionQuery>& queries) {
    vector<FlipInversionQ::S> init(bits.size());
    for (int i = 0; i < (int)bits.size(); i++) init[i] = FlipInversionQ::make(bits[i]);
    FlipInversionQ::Seg seg(init);

    vector<long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, true);
        } else {
            ans.push_back(seg.prod(q.l, q.r).inv);
        }
    }
    return ans;
}

// 01列の区間反転と最長連続1を管理する。反転のため0側の同種情報も保持する
namespace FlipLongestOneQ {
    struct S {
        int len;
        int pref0;
        int suff0;
        int best0;
        int pref1;
        int suff1;
        int best1;
    };

    using F = bool;

    inline S op(S a, S b) {
        S res{};
        res.len = a.len + b.len;
        res.pref0 = (a.pref0 == a.len ? a.len + b.pref0 : a.pref0);
        res.suff0 = (b.suff0 == b.len ? b.len + a.suff0 : b.suff0);
        res.best0 = max({a.best0, b.best0, a.suff0 + b.pref0});
        res.pref1 = (a.pref1 == a.len ? a.len + b.pref1 : a.pref1);
        res.suff1 = (b.suff1 == b.len ? b.len + a.suff1 : b.suff1);
        res.best1 = max({a.best1, b.best1, a.suff1 + b.pref1});
        return res;
    }

    inline S e() {
        return {0, 0, 0, 0, 0, 0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f) return x;
        swap(x.pref0, x.pref1);
        swap(x.suff0, x.suff1);
        swap(x.best0, x.best1);
        return x;
    }

    inline F composition(F f, F g) {
        return f ^ g;
    }

    inline F id() {
        return false;
    }

    inline S make(int bit) {
        if (bit) return {1, 0, 0, 0, 1, 1, 1};
        return {1, 1, 1, 1, 0, 0, 0};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct FlipLongestOneQuery {
    int type;
    int l;
    int r;
};

// type=0: [l,r) を反転、type=1: [l,r) の最長連続1の長さを返す O((N+Q)logN)
vector<int> solve_flip_longest_one(vector<int> bits, const vector<FlipLongestOneQuery>& queries) {
    vector<FlipLongestOneQ::S> init(bits.size());
    for (int i = 0; i < (int)bits.size(); i++) init[i] = FlipLongestOneQ::make(bits[i]);
    FlipLongestOneQ::Seg seg(init);

    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, true);
        } else {
            ans.push_back(seg.prod(q.l, q.r).best1);
        }
    }
    return ans;
}

// 括弧列の区間反転と区間正当性判定を管理する。'('を+1、')'を-1としてprefixの最小最大を持つ
namespace FlipBracketSeqQ {
    struct S {
        int sum;
        int min_prefix;
        int max_prefix;
    };

    using F = bool;

    inline S op(S a, S b) {
        return {
            a.sum + b.sum,
            min(a.min_prefix, a.sum + b.min_prefix),
            max(a.max_prefix, a.sum + b.max_prefix)
        };
    }

    inline S e() {
        return {0, 0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f) return x;
        return {-x.sum, -x.max_prefix, -x.min_prefix};
    }

    inline F composition(F f, F g) {
        return f ^ g;
    }

    inline F id() {
        return false;
    }

    inline S make(char c) {
        if (c == '(') return {1, 0, 1};
        return {-1, -1, 0};
    }

    inline bool valid(S x) {
        return x.sum == 0 && x.min_prefix >= 0;
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct FlipBracketSeqQuery {
    int type;
    int l;
    int r;
};

// type=0: [l,r) の括弧を反転、type=1: [l,r) が正しい括弧列なら1を返す O((N+Q)logN)
vector<int> solve_flip_bracket_sequence(string s, const vector<FlipBracketSeqQuery>& queries) {
    vector<FlipBracketSeqQ::S> init(s.size());
    for (int i = 0; i < (int)s.size(); i++) init[i] = FlipBracketSeqQ::make(s[i]);
    FlipBracketSeqQ::Seg seg(init);

    vector<int> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, true);
        } else {
            ans.push_back(FlipBracketSeqQ::valid(seg.prod(q.l, q.r)) ? 1 : 0);
        }
    }
    return ans;
}

// 文字列の区間文字代入と区間rolling hashを管理する。unsigned long longの自然overflowを使う
namespace SetRollingHashQ {
    using ull = unsigned long long;
    constexpr ull BASE = 1000003ULL;

    inline vector<ull> pow_base{1};
    inline vector<ull> repeat_sum{0};

    inline void ensure_pow(int n) {
        while ((int)pow_base.size() <= n) {
            pow_base.push_back(pow_base.back() * BASE);
            repeat_sum.push_back(repeat_sum.back() * BASE + 1);
        }
    }

    struct S {
        ull hash;
        int len;
    };

    struct F {
        bool has_value;
        ull value;
    };

    inline S op(S a, S b) {
        ensure_pow(b.len);
        return {a.hash * pow_base[b.len] + b.hash, a.len + b.len};
    }

    inline S e() {
        return {0, 0};
    }

    inline S mapping(F f, S x) {
        if (!f.has_value || x.len == 0) return x;
        ensure_pow(x.len);
        return {f.value * repeat_sum[x.len], x.len};
    }

    inline F composition(F f, F g) {
        if (f.has_value) return f;
        return g;
    }

    inline F id() {
        return {false, 0};
    }

    inline ull char_value(char c) {
        return (ull)(unsigned char)c + 1;
    }

    inline F set_char(char c) {
        return {true, char_value(c)};
    }

    inline S make(char c) {
        return {char_value(c), 1};
    }

    using Seg = atcoder::lazy_segtree<S, op, e, F, mapping, composition, id>;
}

struct SetRollingHashQuery {
    int type;
    int l;
    int r;
    char c;
};

// type=0: [l,r) を文字cに代入、type=1: [l,r) のhashを返す O((N+Q)logN)
vector<unsigned long long> solve_set_rolling_hash(string s, const vector<SetRollingHashQuery>& queries) {
    vector<SetRollingHashQ::S> init(s.size());
    for (int i = 0; i < (int)s.size(); i++) init[i] = SetRollingHashQ::make(s[i]);
    SetRollingHashQ::Seg seg(init);

    vector<unsigned long long> ans;
    for (const auto& q : queries) {
        if (q.type == 0) {
            seg.apply(q.l, q.r, SetRollingHashQ::set_char(q.c));
        } else {
            ans.push_back(seg.prod(q.l, q.r).hash);
        }
    }
    return ans;
}

#if __INCLUDE_LEVEL__ == 0

long long naive_sum(const vector<long long>& a, int l, int r) {
    long long res = 0;
    for (int i = l; i < r; i++) res += a[i];
    return res;
}

long long naive_min(const vector<long long>& a, int l, int r) {
    long long res = AddMinQ::INF;
    for (int i = l; i < r; i++) res = min(res, a[i]);
    return res;
}

long long naive_max(const vector<long long>& a, int l, int r) {
    long long res = AddMaxQ::NEG_INF;
    for (int i = l; i < r; i++) res = max(res, a[i]);
    return res;
}

pair<long long, int> naive_min_count(const vector<long long>& a, int l, int r) {
    long long mn = AddMinCountQ::INF;
    int cnt = 0;
    for (int i = l; i < r; i++) {
        if (a[i] < mn) {
            mn = a[i];
            cnt = 1;
        } else if (a[i] == mn) {
            cnt++;
        }
    }
    return {mn, cnt};
}

pair<long long, int> naive_max_count(const vector<long long>& a, int l, int r) {
    long long mx = AddMaxCountQ::NEG_INF;
    int cnt = 0;
    for (int i = l; i < r; i++) {
        if (a[i] > mx) {
            mx = a[i];
            cnt = 1;
        } else if (a[i] == mx) {
            cnt++;
        }
    }
    return {mx, cnt};
}

pair<long long, int> naive_argmin(const vector<long long>& a, int l, int r) {
    long long mn = AddArgMinQ::INF;
    int idx = AddArgMinQ::INF_INDEX;
    for (int i = l; i < r; i++) {
        if (a[i] < mn) {
            mn = a[i];
            idx = i;
        }
    }
    return {mn, idx};
}

pair<long long, int> naive_argmax(const vector<long long>& a, int l, int r) {
    long long mx = AddArgMaxQ::NEG_INF;
    int idx = AddArgMaxQ::INF_INDEX;
    for (int i = l; i < r; i++) {
        if (a[i] > mx) {
            mx = a[i];
            idx = i;
        }
    }
    return {mx, idx};
}

long long naive_inversion(const vector<int>& a, int l, int r) {
    long long res = 0;
    for (int i = l; i < r; i++) {
        if (a[i] == 0) continue;
        for (int j = i + 1; j < r; j++) if (a[j] == 0) res++;
    }
    return res;
}

int naive_longest_one(const vector<int>& a, int l, int r) {
    int best = 0;
    int cur = 0;
    for (int i = l; i < r; i++) {
        if (a[i]) {
            cur++;
            best = max(best, cur);
        } else {
            cur = 0;
        }
    }
    return best;
}

bool naive_valid_bracket(const string& s, int l, int r) {
    int bal = 0;
    for (int i = l; i < r; i++) {
        bal += (s[i] == '(' ? 1 : -1);
        if (bal < 0) return false;
    }
    return bal == 0;
}

unsigned long long naive_hash(const string& s, int l, int r) {
    unsigned long long h = 0;
    for (int i = l; i < r; i++) {
        h = h * SetRollingHashQ::BASE + SetRollingHashQ::char_value(s[i]);
    }
    return h;
}

void test_fixed_edges() {
    {
        vector<AddSumQ::S> v;
        AddSumQ::Seg seg(v);
        assert((seg.prod(0, 0).sum == 0));
        assert((seg.prod(0, 0).len == 0));
    }
    {
        vector<AddSumQ::S> v = {AddSumQ::make(5)};
        AddSumQ::Seg seg(v);
        seg.apply(0, 1, 3);
        assert((seg.get(0).sum == 8));
        assert((seg.prod(0, 0).sum == 0));
        assert((seg.all_prod().sum == 8));
    }
    {
        AddMinQ::Seg seg(vector<long long>{7});
        seg.apply(0, 1, -10);
        assert((seg.prod(0, 1) == -3));
        assert((seg.prod(0, 0) == AddMinQ::INF));
    }
    {
        vector<AddSetSumQ::S> v = {AddSetSumQ::make(1), AddSetSumQ::make(2), AddSetSumQ::make(3)};
        AddSetSumQ::Seg seg(v);
        seg.apply(0, 3, AddSetSumQ::set_value(5));
        seg.apply(1, 3, AddSetSumQ::add(2));
        assert((seg.prod(0, 3).sum == 19));
        assert((seg.get(1).sum == 7));
    }
    {
        vector<AffineSumModQ::S> v = {AffineSumModQ::make(1), AffineSumModQ::make(2)};
        AffineSumModQ::Seg seg(v);
        seg.apply(0, 2, AffineSumModQ::affine(3, 4));
        assert((seg.prod(0, 2).sum == 17));
    }
    {
        string s = "(()())";
        vector<FlipBracketSeqQ::S> v(s.size());
        for (int i = 0; i < (int)s.size(); i++) v[i] = FlipBracketSeqQ::make(s[i]);
        FlipBracketSeqQ::Seg seg(v);
        assert((FlipBracketSeqQ::valid(seg.prod(0, (int)s.size()))));
        seg.apply(0, 1, true);
        assert((!FlipBracketSeqQ::valid(seg.prod(0, (int)s.size()))));
    }
    {
        vector<FlipLongestOneQ::S> v = {
            FlipLongestOneQ::make(1), FlipLongestOneQ::make(0), FlipLongestOneQ::make(0), FlipLongestOneQ::make(1)
        };
        FlipLongestOneQ::Seg seg(v);
        seg.apply(1, 3, true);
        assert((seg.prod(0, 4).best1 == 4));
    }
    {
        string s = "abc";
        vector<SetRollingHashQ::S> v(s.size());
        for (int i = 0; i < (int)s.size(); i++) v[i] = SetRollingHashQ::make(s[i]);
        SetRollingHashQ::Seg seg(v);
        seg.apply(0, 2, SetRollingHashQ::set_char('z'));
        s[0] = s[1] = 'z';
        assert((seg.prod(0, 3).hash == naive_hash(s, 0, 3)));
    }
}

void test_add_family_random(mt19937& rng) {
    uniform_int_distribution<int> n_dist(1, 35);
    uniform_int_distribution<int> val_dist(-5, 5);
    uniform_int_distribution<int> add_dist(-4, 4);

    for (int tc = 0; tc < 120; tc++) {
        int n = n_dist(rng);
        vector<long long> a(n);
        for (auto& x : a) x = val_dist(rng);

        vector<AddSumQ::S> sum_init(n);
        vector<AddMinMaxQ::S> minmax_init(n);
        vector<AddMinCountQ::S> mincnt_init(n);
        vector<AddMaxCountQ::S> maxcnt_init(n);
        vector<AddArgMinQ::S> argmin_init(n);
        vector<AddArgMaxQ::S> argmax_init(n);
        for (int i = 0; i < n; i++) {
            sum_init[i] = AddSumQ::make(a[i]);
            minmax_init[i] = AddMinMaxQ::make(a[i]);
            mincnt_init[i] = AddMinCountQ::make(a[i]);
            maxcnt_init[i] = AddMaxCountQ::make(a[i]);
            argmin_init[i] = AddArgMinQ::make(a[i], i);
            argmax_init[i] = AddArgMaxQ::make(a[i], i);
        }

        AddSumQ::Seg seg_sum(sum_init);
        AddMinQ::Seg seg_min(a);
        AddMaxQ::Seg seg_max(a);
        AddMinMaxQ::Seg seg_minmax(minmax_init);
        AddMinCountQ::Seg seg_mincnt(mincnt_init);
        AddMaxCountQ::Seg seg_maxcnt(maxcnt_init);
        AddArgMinQ::Seg seg_argmin(argmin_init);
        AddArgMaxQ::Seg seg_argmax(argmax_init);

        for (int qi = 0; qi < 240; qi++) {
            int type = uniform_int_distribution<int>(0, 5)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);

            if (type <= 1) {
                long long x = add_dist(rng);
                seg_sum.apply(l, r, x);
                seg_min.apply(l, r, x);
                seg_max.apply(l, r, x);
                seg_minmax.apply(l, r, x);
                seg_mincnt.apply(l, r, x);
                seg_maxcnt.apply(l, r, x);
                seg_argmin.apply(l, r, x);
                seg_argmax.apply(l, r, x);
                for (int i = l; i < r; i++) a[i] += x;
            } else if (type == 2) {
                assert((seg_sum.prod(l, r).sum == naive_sum(a, l, r)));
                assert((seg_min.prod(l, r) == naive_min(a, l, r)));
                assert((seg_max.prod(l, r) == naive_max(a, l, r)));
                auto mm = seg_minmax.prod(l, r);
                assert((mm.mn == naive_min(a, l, r)));
                assert((mm.mx == naive_max(a, l, r)));
                assert((mm.len == r - l));
            } else if (type == 3) {
                auto emn = naive_min_count(a, l, r);
                auto emx = naive_max_count(a, l, r);
                auto rmn = seg_mincnt.prod(l, r);
                auto rmx = seg_maxcnt.prod(l, r);
                assert((pair<long long, int>(rmn.value, rmn.count) == emn));
                assert((pair<long long, int>(rmx.value, rmx.count) == emx));
            } else if (type == 4) {
                auto e_min = naive_argmin(a, l, r);
                auto e_max = naive_argmax(a, l, r);
                auto r_min = seg_argmin.prod(l, r);
                auto r_max = seg_argmax.prod(l, r);
                assert((pair<long long, int>(r_min.value, r_min.index) == e_min));
                assert((pair<long long, int>(r_max.value, r_max.index) == e_max));
            } else {
                int start = uniform_int_distribution<int>(0, n)(rng);
                long long threshold = val_dist(rng);
                int less_pos = n;
                int greater_pos = n;
                for (int i = start; i < n; i++) {
                    if (less_pos == n && a[i] < threshold) less_pos = i;
                    if (greater_pos == n && a[i] > threshold) greater_pos = i;
                }
                int got_less = seg_min.max_right(start, [&](long long mn) { return mn >= threshold; });
                int got_greater = seg_max.max_right(start, [&](long long mx) { return mx <= threshold; });
                assert((got_less == less_pos));
                assert((got_greater == greater_pos));
            }
        }
    }

    for (int tc = 0; tc < 80; tc++) {
        int n = n_dist(rng);
        vector<long long> a(n);
        vector<AddSumQ::S> init(n);
        for (int i = 0; i < n; i++) {
            a[i] = uniform_int_distribution<int>(0, 5)(rng);
            init[i] = AddSumQ::make(a[i]);
        }
        AddSumQ::Seg seg(init);
        for (int qi = 0; qi < 120; qi++) {
            int type = uniform_int_distribution<int>(0, 2)(rng);
            if (type == 0) {
                int l = uniform_int_distribution<int>(0, n)(rng);
                int r = uniform_int_distribution<int>(0, n)(rng);
                if (l > r) swap(l, r);
                long long x = uniform_int_distribution<int>(0, 5)(rng);
                seg.apply(l, r, x);
                for (int i = l; i < r; i++) a[i] += x;
            } else {
                long long total = naive_sum(a, 0, n);
                long long target = uniform_int_distribution<long long>(0, total + 5)(rng);
                int expected = 0;
                if (target <= 0) {
                    expected = 0;
                } else {
                    long long cur = 0;
                    expected = n;
                    for (int i = 0; i < n; i++) {
                        cur += a[i];
                        if (cur >= target) {
                            expected = i;
                            break;
                        }
                    }
                }
                int got = (target <= 0 ? 0 : seg.max_right(0, [&](AddSumQ::S s) { return s.sum < target; }));
                assert((got == expected));
            }
        }
    }
}

void test_set_family_random(mt19937& rng) {
    uniform_int_distribution<int> n_dist(1, 35);
    uniform_int_distribution<int> val_dist(-7, 7);

    for (int tc = 0; tc < 120; tc++) {
        int n = n_dist(rng);
        vector<long long> a(n);
        vector<SetSumQ::S> sum_init(n);
        vector<SetMinQ::S> min_init(n);
        vector<SetMaxQ::S> max_init(n);
        vector<SetMinMaxQ::S> minmax_init(n);
        vector<SetMinCountQ::S> mincnt_init(n);
        vector<SetMaxCountQ::S> maxcnt_init(n);
        for (int i = 0; i < n; i++) {
            a[i] = val_dist(rng);
            sum_init[i] = SetSumQ::make(a[i]);
            min_init[i] = SetMinQ::make(a[i]);
            max_init[i] = SetMaxQ::make(a[i]);
            minmax_init[i] = SetMinMaxQ::make(a[i]);
            mincnt_init[i] = SetMinCountQ::make(a[i]);
            maxcnt_init[i] = SetMaxCountQ::make(a[i]);
        }

        SetSumQ::Seg seg_sum(sum_init);
        SetMinQ::Seg seg_min(min_init);
        SetMaxQ::Seg seg_max(max_init);
        SetMinMaxQ::Seg seg_minmax(minmax_init);
        SetMinCountQ::Seg seg_mincnt(mincnt_init);
        SetMaxCountQ::Seg seg_maxcnt(maxcnt_init);

        for (int qi = 0; qi < 240; qi++) {
            int type = uniform_int_distribution<int>(0, 3)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);

            if (type == 0) {
                long long x = val_dist(rng);
                seg_sum.apply(l, r, SetSumQ::set_value(x));
                seg_min.apply(l, r, SetMinQ::set_value(x));
                seg_max.apply(l, r, SetMaxQ::set_value(x));
                seg_minmax.apply(l, r, SetMinMaxQ::set_value(x));
                seg_mincnt.apply(l, r, SetMinCountQ::set_value(x));
                seg_maxcnt.apply(l, r, SetMaxCountQ::set_value(x));
                for (int i = l; i < r; i++) a[i] = x;
            } else if (type == 1) {
                assert((seg_sum.prod(l, r).sum == naive_sum(a, l, r)));
                assert((seg_min.prod(l, r).value == naive_min(a, l, r)));
                assert((seg_max.prod(l, r).value == naive_max(a, l, r)));
                auto mm = seg_minmax.prod(l, r);
                assert((mm.mn == naive_min(a, l, r)));
                assert((mm.mx == naive_max(a, l, r)));
            } else if (type == 2) {
                auto emn = naive_min_count(a, l, r);
                auto emx = naive_max_count(a, l, r);
                auto rmn = seg_mincnt.prod(l, r);
                auto rmx = seg_maxcnt.prod(l, r);
                assert((pair<long long, int>(rmn.value, rmn.count) == emn));
                assert((pair<long long, int>(rmx.value, rmx.count) == emx));
            } else {
                int p = uniform_int_distribution<int>(0, n - 1)(rng);
                assert((seg_sum.get(p).sum == a[p]));
                assert((seg_min.get(p).value == a[p]));
                assert((seg_max.get(p).value == a[p]));
            }
        }
    }
}

void test_add_set_random(mt19937& rng) {
    uniform_int_distribution<int> n_dist(1, 35);
    uniform_int_distribution<int> val_dist(-7, 7);
    uniform_int_distribution<int> add_dist(-5, 5);

    for (int tc = 0; tc < 160; tc++) {
        int n = n_dist(rng);
        vector<long long> a(n);
        vector<AddSetSumQ::S> sum_init(n);
        vector<AddSetMinMaxQ::S> minmax_init(n);
        for (int i = 0; i < n; i++) {
            a[i] = val_dist(rng);
            sum_init[i] = AddSetSumQ::make(a[i]);
            minmax_init[i] = AddSetMinMaxQ::make(a[i]);
        }
        AddSetSumQ::Seg seg_sum(sum_init);
        AddSetMinMaxQ::Seg seg_minmax(minmax_init);

        for (int qi = 0; qi < 320; qi++) {
            int type = uniform_int_distribution<int>(0, 4)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);

            if (type == 0) {
                long long x = add_dist(rng);
                seg_sum.apply(l, r, AddSetSumQ::add(x));
                seg_minmax.apply(l, r, AddSetMinMaxQ::add(x));
                for (int i = l; i < r; i++) a[i] += x;
            } else if (type == 1) {
                long long x = val_dist(rng);
                seg_sum.apply(l, r, AddSetSumQ::set_value(x));
                seg_minmax.apply(l, r, AddSetMinMaxQ::set_value(x));
                for (int i = l; i < r; i++) a[i] = x;
            } else if (type == 2) {
                assert((seg_sum.prod(l, r).sum == naive_sum(a, l, r)));
                auto mm = seg_minmax.prod(l, r);
                assert((mm.mn == naive_min(a, l, r)));
                assert((mm.mx == naive_max(a, l, r)));
            } else {
                int p = uniform_int_distribution<int>(0, n - 1)(rng);
                assert((seg_sum.get(p).sum == a[p]));
                auto mm = seg_minmax.get(p);
                assert((mm.mn == a[p]));
                assert((mm.mx == a[p]));
            }
        }
    }
}

void test_affine_random(mt19937& rng) {
    const long long MOD = AffineSumModQ::MOD;
    uniform_int_distribution<int> n_dist(1, 35);
    uniform_int_distribution<int> val_dist(-20, 20);

    for (int tc = 0; tc < 120; tc++) {
        int n = n_dist(rng);
        vector<long long> a(n);
        vector<AffineSumModQ::S> init(n);
        for (int i = 0; i < n; i++) {
            a[i] = AffineSumModQ::norm(val_dist(rng));
            init[i] = AffineSumModQ::make(a[i]);
        }
        AffineSumModQ::Seg seg(init);

        for (int qi = 0; qi < 240; qi++) {
            int type = uniform_int_distribution<int>(0, 2)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);
            if (type == 0) {
                long long aa = val_dist(rng);
                long long bb = val_dist(rng);
                long long na = AffineSumModQ::norm(aa);
                long long nb = AffineSumModQ::norm(bb);
                seg.apply(l, r, AffineSumModQ::affine(aa, bb));
                for (int i = l; i < r; i++) a[i] = (na * a[i] + nb) % MOD;
            } else {
                long long expected = 0;
                for (int i = l; i < r; i++) expected = (expected + a[i]) % MOD;
                assert((seg.prod(l, r).sum == expected));
            }
        }
    }
}

void test_bitwise_random(mt19937& rng) {
    uniform_int_distribution<int> n_dist(1, 35);
    uniform_int_distribution<int> val_dist(0, (1 << 10) - 1);

    for (int tc = 0; tc < 120; tc++) {
        int n = n_dist(rng);
        vector<int> a(n);
        vector<BitwiseSumQ::S> init(n);
        for (int i = 0; i < n; i++) {
            a[i] = val_dist(rng);
            init[i] = BitwiseSumQ::make(a[i]);
        }
        BitwiseSumQ::Seg seg(init);

        for (int qi = 0; qi < 260; qi++) {
            int type = uniform_int_distribution<int>(0, 4)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);
            int x = val_dist(rng);

            if (type == 0) {
                seg.apply(l, r, BitwiseSumQ::xor_mask(x));
                for (int i = l; i < r; i++) a[i] ^= x;
            } else if (type == 1) {
                seg.apply(l, r, BitwiseSumQ::and_mask(x));
                for (int i = l; i < r; i++) a[i] &= x;
            } else if (type == 2) {
                seg.apply(l, r, BitwiseSumQ::or_mask(x));
                for (int i = l; i < r; i++) a[i] |= x;
            } else if (type == 3) {
                seg.apply(l, r, BitwiseSumQ::set_value(x));
                for (int i = l; i < r; i++) a[i] = x;
            } else {
                long long expected = 0;
                for (int i = l; i < r; i++) expected += a[i];
                assert((BitwiseSumQ::value(seg.prod(l, r)) == expected));
            }
        }
    }
}

void test_flip_random(mt19937& rng) {
    uniform_int_distribution<int> n_dist(1, 35);

    for (int tc = 0; tc < 120; tc++) {
        int n = n_dist(rng);
        vector<int> a(n);
        vector<FlipCountOneQ::S> count_init(n);
        vector<FlipInversionQ::S> inv_init(n);
        vector<FlipLongestOneQ::S> longest_init(n);
        for (int i = 0; i < n; i++) {
            a[i] = uniform_int_distribution<int>(0, 1)(rng);
            count_init[i] = FlipCountOneQ::make(a[i]);
            inv_init[i] = FlipInversionQ::make(a[i]);
            longest_init[i] = FlipLongestOneQ::make(a[i]);
        }
        FlipCountOneQ::Seg seg_count(count_init);
        FlipInversionQ::Seg seg_inv(inv_init);
        FlipLongestOneQ::Seg seg_longest(longest_init);

        for (int qi = 0; qi < 240; qi++) {
            int type = uniform_int_distribution<int>(0, 3)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);

            if (type == 0) {
                seg_count.apply(l, r, true);
                seg_inv.apply(l, r, true);
                seg_longest.apply(l, r, true);
                for (int i = l; i < r; i++) a[i] ^= 1;
            } else {
                int ones = 0;
                for (int i = l; i < r; i++) ones += a[i];
                assert((seg_count.prod(l, r).one == ones));
                assert((seg_inv.prod(l, r).inv == naive_inversion(a, l, r)));
                assert((seg_longest.prod(l, r).best1 == naive_longest_one(a, l, r)));
            }
        }
    }
}

void test_bracket_random(mt19937& rng) {
    uniform_int_distribution<int> n_dist(1, 36);

    for (int tc = 0; tc < 120; tc++) {
        int n = n_dist(rng);
        string s(n, '(');
        vector<FlipBracketSeqQ::S> init(n);
        for (int i = 0; i < n; i++) {
            s[i] = uniform_int_distribution<int>(0, 1)(rng) ? '(' : ')';
            init[i] = FlipBracketSeqQ::make(s[i]);
        }
        FlipBracketSeqQ::Seg seg(init);

        for (int qi = 0; qi < 240; qi++) {
            int type = uniform_int_distribution<int>(0, 2)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);
            if (type == 0) {
                seg.apply(l, r, true);
                for (int i = l; i < r; i++) s[i] = (s[i] == '(' ? ')' : '(');
            } else {
                assert((FlipBracketSeqQ::valid(seg.prod(l, r)) == naive_valid_bracket(s, l, r)));
            }
        }
    }
}

void test_hash_random(mt19937& rng) {
    uniform_int_distribution<int> n_dist(1, 35);

    for (int tc = 0; tc < 100; tc++) {
        int n = n_dist(rng);
        string s(n, 'a');
        vector<SetRollingHashQ::S> init(n);
        for (int i = 0; i < n; i++) {
            s[i] = char('a' + uniform_int_distribution<int>(0, 25)(rng));
            init[i] = SetRollingHashQ::make(s[i]);
        }
        SetRollingHashQ::Seg seg(init);

        for (int qi = 0; qi < 220; qi++) {
            int type = uniform_int_distribution<int>(0, 2)(rng);
            int l = uniform_int_distribution<int>(0, n)(rng);
            int r = uniform_int_distribution<int>(0, n)(rng);
            if (l > r) swap(l, r);
            if (type == 0) {
                char c = char('a' + uniform_int_distribution<int>(0, 25)(rng));
                seg.apply(l, r, SetRollingHashQ::set_char(c));
                for (int i = l; i < r; i++) s[i] = c;
            } else {
                assert((seg.prod(l, r).hash == naive_hash(s, l, r)));
            }
        }
    }
}

void test_solvers_fixed() {
    assert((solve_add_sum({1, 2, 3}, {{1, 0, 3, 0}, {0, 1, 3, 5}, {1, 0, 3, 0}}) == vector<long long>({6, 16})));
    assert((solve_add_sum_prefix_lower_bound({1, 2, 3}, {{1, 0, 0, 4}, {0, 0, 2, 2}, {1, 0, 0, 4}}) == vector<int>({2, 1})));
    assert((solve_add_min({3, 1, 4}, {{1, 0, 3, 0}, {0, 1, 3, -5}, {1, 0, 3, 0}}) == vector<long long>({1, -4})));
    assert((solve_add_first_less_than({5, 4, 3}, {{1, 0, 0, 4}, {0, 0, 2, -3}, {1, 0, 0, 4}}) == vector<int>({2, 0})));
    assert((solve_add_max({3, 1, 4}, {{1, 0, 3, 0}, {0, 0, 2, 5}, {1, 0, 3, 0}}) == vector<long long>({4, 8})));
    assert((solve_add_first_greater_than({1, 2, 3}, {{1, 0, 0, 2}, {0, 0, 2, 5}, {1, 0, 0, 2}}) == vector<int>({2, 0})));
    assert((solve_add_min_max({2, 5, 1}, {{1, 0, 3, 0}, {0, 1, 3, 3}, {1, 0, 3, 0}}) == vector<pair<long long, long long>>({{1, 5}, {2, 8}})));
    assert((solve_add_min_count({2, 1, 1}, {{1, 0, 3, 0}, {0, 0, 1, -1}, {1, 0, 3, 0}}) == vector<pair<long long, int>>({{1, 2}, {1, 3}})));
    assert((solve_add_max_count({2, 3, 3}, {{1, 0, 3, 0}, {0, 0, 1, 1}, {1, 0, 3, 0}}) == vector<pair<long long, int>>({{3, 2}, {3, 3}})));
    assert((solve_add_argmin({2, 1, 1}, {{1, 0, 3, 0}, {0, 0, 1, -2}, {1, 0, 3, 0}}) == vector<pair<long long, int>>({{1, 1}, {0, 0}})));
    assert((solve_add_argmax({2, 3, 3}, {{1, 0, 3, 0}, {0, 0, 1, 2}, {1, 0, 3, 0}}) == vector<pair<long long, int>>({{3, 1}, {4, 0}})));

    assert((solve_set_sum({1, 2, 3}, {{1, 0, 3, 0}, {0, 1, 3, 4}, {1, 0, 3, 0}}) == vector<long long>({6, 9})));
    assert((solve_set_min({3, 1, 4}, {{1, 0, 3, 0}, {0, 0, 2, 5}, {1, 0, 3, 0}}) == vector<long long>({1, 4})));
    assert((solve_set_max({3, 1, 4}, {{1, 0, 3, 0}, {0, 0, 2, 5}, {1, 0, 3, 0}}) == vector<long long>({4, 5})));
    assert((solve_set_min_max({3, 1, 4}, {{1, 0, 3, 0}, {0, 0, 2, 5}, {1, 0, 3, 0}}) == vector<pair<long long, long long>>({{1, 4}, {4, 5}})));
    assert((solve_set_min_count({2, 1, 1}, {{1, 0, 3, 0}, {0, 0, 3, 5}, {1, 0, 3, 0}}) == vector<pair<long long, int>>({{1, 2}, {5, 3}})));
    assert((solve_set_max_count({2, 3, 3}, {{1, 0, 3, 0}, {0, 0, 3, 5}, {1, 0, 3, 0}}) == vector<pair<long long, int>>({{3, 2}, {5, 3}})));

    assert((solve_add_set_sum({1, 2, 3}, {{0, 0, 3, 2}, {1, 1, 3, 5}, {2, 0, 3, 0}}) == vector<long long>({13})));
    assert((solve_add_set_min_max({1, 2, 3}, {{0, 0, 3, 2}, {1, 1, 3, 5}, {2, 0, 3, 0}}) == vector<pair<long long, long long>>({{3, 5}})));
    assert((solve_affine_sum_mod({1, 2}, {{0, 0, 2, 2, 3}, {1, 0, 2, 0, 0}}) == vector<long long>({12})));
    assert((solve_bitwise_sum({1, 2, 3}, {{0, 0, 3, 1}, {4, 0, 3, 0}}) == vector<long long>({5})));
    assert((solve_flip_count_one({1, 0, 0}, {{1, 0, 3}, {0, 1, 3}, {1, 0, 3}}) == vector<int>({1, 3})));
    assert((solve_flip_inversion({1, 0, 0}, {{1, 0, 3}, {0, 0, 3}, {1, 0, 3}}) == vector<long long>({2, 0})));
    assert((solve_flip_longest_one({1, 0, 0, 1}, {{1, 0, 4}, {0, 1, 3}, {1, 0, 4}}) == vector<int>({1, 4})));
    assert((solve_flip_bracket_sequence("()()", {{1, 0, 4}, {0, 1, 3}, {1, 0, 4}}) == vector<int>({1, 1})));
    auto hash_ans = solve_set_rolling_hash("abc", {{1, 0, 3, 'x'}, {0, 0, 2, 'z'}, {1, 0, 3, 'x'}});
    string hs = "abc";
    unsigned long long h0 = naive_hash(hs, 0, 3);
    hs[0] = hs[1] = 'z';
    unsigned long long h1 = naive_hash(hs, 0, 3);
    assert((hash_ans == vector<unsigned long long>({h0, h1})));
}

int main() {
    mt19937 rng(123456789);
    test_fixed_edges();
    test_add_family_random(rng);
    test_set_family_random(rng);
    test_add_set_random(rng);
    test_affine_random(rng);
    test_bitwise_random(rng);
    test_flip_random(rng);
    test_bracket_random(rng);
    test_hash_random(rng);
    test_solvers_fixed();
    cout << "All tests passed\n";
    return 0;
}

#endif
