#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <type_traits>
#include <cassert>
#include <utility>

using namespace std;

// 互いに重ならず隣接もしない半開区間 [l,r) の集合を、乱数化 Treap で管理する。
// 追加は重なり/隣接を自動でマージ、削除は必要に応じて分割する。
// 区間ごとに getval(l,r) で得た値 S を保持し、左端昇順の op による積を高速に計算できる。
// 被覆長合計は T で保持するためオーバーフロー時の動作は未定義（負値は assert で検出）。
template <class T, class S, class Op, class E, class GetVal>
class IntervalTreap {
    static_assert(std::is_integral_v<T> && std::is_signed_v<T>,
                  "IntervalTreap<T,...>: T must be a signed integral type");
    static_assert(std::is_invocable_r_v<S, const Op&, S, S>,
                  "IntervalTreap: op must be callable as S(S,S) (const)");
    static_assert(std::is_invocable_r_v<S, const E&>,
                  "IntervalTreap: e must be callable as S() (const)");
    static_assert(std::is_invocable_r_v<S, const GetVal&, T, T>,
                  "IntervalTreap: getval must be callable as S(T,T) (const)");

public:
    // 区間と部分木の集計情報を保持するノード。
    // 返されたポインタ参照は、木の変更（挿入/削除/clear 等）で無効化されることがある。
    struct Node {
        T l, r;        // 区間の左端・右端（半開区間）
        int ch[2];     // 子ノードの添字（0:左, 1:右, -1:なし）
        unsigned pri;  // Treap の優先度（乱数）
        int size;      // 部分木に含まれるノード数
        T min_l;       // 部分木内の最小の左端
        T max_r;       // 部分木内の最大の右端
        T cov;         // 部分木内の被覆長合計（Tで保持）
        S val;         // この区間 [l,r) の値（getval の結果）
        S prod;        // 部分木内の積（左端昇順に op で結合）
    };

private:
    [[no_unique_address]] Op op_;
    [[no_unique_address]] E e_;
    [[no_unique_address]] GetVal getval_;

    vector<Node> nodes;
    vector<int> free_list;
    int root;
    uint32_t rng_state;
    S id_;

    static constexpr int null = -1;

    uint32_t next_rand() {
        uint32_t x = rng_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng_state = x ? x : 0x12345678u;
        return rng_state;
    }

    static inline T clamp_len(T l, T r) {
        T len = r - l;
        assert(len >= T(0));
        return len;
    }

    int new_node(T l, T r) {
        const unsigned pri = next_rand();
        const T len = clamp_len(l, r);
        const S v = getval_(l, r);
        Node nn{l, r, {null, null}, pri, 1, l, r, len, v, v};

        int idx;
        if (!free_list.empty()) {
            idx = free_list.back();
            free_list.pop_back();
            nodes[idx] = std::move(nn);
        } else {
            idx = (int)nodes.size();
            nodes.push_back(std::move(nn));
        }
        return idx;
    }

    void free_node(int idx) {
        free_list.push_back(idx);
    }

    void pull(int idx) {
        Node &x = nodes[idx];
        const int L = x.ch[0];
        const int R = x.ch[1];

        x.size = 1;
        x.min_l = x.l;
        x.max_r = x.r;
        x.cov = clamp_len(x.l, x.r);

        if (L != null) {
            const Node &ln = nodes[L];
            x.size += ln.size;
            x.min_l = ln.min_l;
            x.max_r = std::max(x.max_r, ln.max_r);
            x.cov += ln.cov;
            assert(x.cov >= T(0));
        }
        if (R != null) {
            const Node &rn = nodes[R];
            x.size += rn.size;
            x.max_r = std::max(x.max_r, rn.max_r);
            x.cov += rn.cov;
            assert(x.cov >= T(0));
        }

        S res = x.val;
        if (L != null) res = op_(nodes[L].prod, res);
        if (R != null) res = op_(res, nodes[R].prod);
        x.prod = std::move(res);
    }

    int merge(int a, int b) {
        if (a == null || b == null) return a == null ? b : a;
        if (nodes[a].pri > nodes[b].pri) {
            nodes[a].ch[1] = merge(nodes[a].ch[1], b);
            pull(a);
            return a;
        } else {
            nodes[b].ch[0] = merge(a, nodes[b].ch[0]);
            pull(b);
            return b;
        }
    }

    void split(int cur, T key, int &left, int &right) {
        if (cur == null) {
            left = right = null;
            return;
        }
        if (key <= nodes[cur].l) {
            split(nodes[cur].ch[0], key, left, nodes[cur].ch[0]);
            pull(cur);
            right = cur;
        } else {
            split(nodes[cur].ch[1], key, nodes[cur].ch[1], right);
            pull(cur);
            left = cur;
        }
    }

    int insert_node(int cur, int node) {
        if (cur == null) return node;
        if (nodes[node].pri > nodes[cur].pri) {
            split(cur, nodes[node].l, nodes[node].ch[0], nodes[node].ch[1]);
            pull(node);
            return node;
        }
        if (nodes[node].l < nodes[cur].l) {
            nodes[cur].ch[0] = insert_node(nodes[cur].ch[0], node);
        } else {
            nodes[cur].ch[1] = insert_node(nodes[cur].ch[1], node);
        }
        pull(cur);
        return cur;
    }

    int erase_by_key(int cur, T key) {
        if (cur == null) return null;
        if (key < nodes[cur].l) {
            nodes[cur].ch[0] = erase_by_key(nodes[cur].ch[0], key);
            pull(cur);
            return cur;
        }
        if (key > nodes[cur].l) {
            nodes[cur].ch[1] = erase_by_key(nodes[cur].ch[1], key);
            pull(cur);
            return cur;
        }
        const int L = nodes[cur].ch[0];
        const int R = nodes[cur].ch[1];
        free_node(cur);
        return merge(L, R);
    }

    int find_le_by_key(int cur, T key) const {
        int res = null;
        while (cur != null) {
            const Node &x = nodes[cur];
            if (x.l <= key) {
                res = cur;
                cur = x.ch[1];
            } else {
                cur = x.ch[0];
            }
        }
        return res;
    }

    int find_first_ge_by_key(int cur, T key) const {
        int res = null;
        while (cur != null) {
            const Node &x = nodes[cur];
            if (x.l >= key) {
                res = cur;
                cur = x.ch[0];
            } else {
                cur = x.ch[1];
            }
        }
        return res;
    }

    int left_most_idx(int cur) const {
        if (cur == null) return null;
        while (nodes[cur].ch[0] != null) cur = nodes[cur].ch[0];
        return cur;
    }

    int right_most_idx(int cur) const {
        if (cur == null) return null;
        while (nodes[cur].ch[1] != null) cur = nodes[cur].ch[1];
        return cur;
    }

    T range_covered_length(int cur, T l, T r) const {
        if (cur == null) return T(0);
        T res = T(0);

        static thread_local vector<int> st;
        st.clear();
        st.push_back(cur);

        while (!st.empty()) {
            const int idx = st.back();
            st.pop_back();
            const Node &x = nodes[idx];

            if (x.max_r <= l || x.min_l >= r) continue;
            if (x.min_l >= l && x.max_r <= r) {
                res += x.cov;
                assert(res >= T(0));
                continue;
            }

            if (x.ch[0] != null) st.push_back(x.ch[0]);
            if (x.ch[1] != null) st.push_back(x.ch[1]);

            if (x.r > l && x.l < r) {
                const T a = x.l < l ? l : x.l;
                const T b = x.r > r ? r : x.r;
                if (b > a) {
                    res += clamp_len(a, b);
                    assert(res >= T(0));
                }
            }
        }
        return res;
    }

    S range_prod(int cur, T l, T r) const {
        if (cur == null) return id_;
        S res = id_;

        static thread_local vector<int> st;
        st.clear();

        int node = cur;
        while (node != null || !st.empty()) {
            while (node != null) {
                const Node &x = nodes[node];

                if (x.max_r <= l || x.min_l >= r) {
                    node = null;
                    break;
                }
                if (x.min_l >= l && x.max_r <= r) {
                    res = op_(res, x.prod);
                    node = null;
                    break;
                }

                st.push_back(node);
                node = x.ch[0];
            }

            if (st.empty()) break;

            const int idx = st.back();
            st.pop_back();
            const Node &x = nodes[idx];

            if (x.r > l && x.l < r) {
                const T a = x.l < l ? l : x.l;
                const T b = x.r > r ? r : x.r;
                if (b > a) {
                    if (a == x.l && b == x.r) res = op_(res, x.val);
                    else res = op_(res, getval_(a, b));
                }
            }
            node = x.ch[1];
        }

        return res;
    }

    template <class F>
    void inorder_all(int cur, const F &f) const {
        if (cur == null) return;
        const Node &x = nodes[cur];
        if (x.ch[0] != null) inorder_all(x.ch[0], f);
        f(x);
        if (x.ch[1] != null) inorder_all(x.ch[1], f);
    }

    template <class F>
    void inorder_range(int cur, T l, T r, const F &f) const {
        if (cur == null) return;
        const Node &x = nodes[cur];
        if (x.max_r <= l || x.min_l >= r) return;
        if (x.ch[0] != null) inorder_range(x.ch[0], l, r, f);
        if (x.r > l && x.l < r) f(x);
        if (x.ch[1] != null) inorder_range(x.ch[1], l, r, f);
    }

    const Node* find_impl(int cur, T x) const {
        while (cur != null) {
            const Node &n = nodes[cur];
            if (x < n.l) cur = n.ch[0];
            else if (x >= n.r) cur = n.ch[1];
            else return &n;
        }
        return nullptr;
    }

    int first_overlap_idx(int cur, T l, T r) const {
        if (cur == null || l >= r) return null;
        int idx = find_le_by_key(cur, l);
        if (idx != null && nodes[idx].r > l) return idx;
        idx = find_first_ge_by_key(cur, l);
        if (idx != null && nodes[idx].l < r) return idx;
        return null;
    }

public:
    // 空の集合を構築する。O(1)
    // functor を既定構築できない場合は、functor を受け取る構築を用いる。
    IntervalTreap(size_t reserve_nodes = 0)
        requires (std::is_default_constructible_v<Op> &&
                  std::is_default_constructible_v<E> &&
                  std::is_default_constructible_v<GetVal>)
        : op_(), e_(), getval_(),
          nodes(), free_list(), root(null), rng_state(0x12345678u), id_(e_()) {
        if (reserve_nodes) nodes.reserve(reserve_nodes);
    }

    // functor を受け取って構築する。O(1)
    // キャプチャありラムダ等はこの構築で渡し、内部に値として保持する。
    IntervalTreap(Op op, E e, GetVal getval, size_t reserve_nodes = 0)
        : op_(std::move(op)), e_(std::move(e)), getval_(std::move(getval)),
          nodes(), free_list(), root(null), rng_state(0x12345678u), id_(e_()) {
        if (reserve_nodes) nodes.reserve(reserve_nodes);
    }

    // 全要素を破棄して初期状態に戻す。O(n)
    void clear() {
        nodes.clear();
        free_list.clear();
        root = null;
        rng_state = 0x12345678u;
    }

    // 内部配列の容量を確保して再確保回数を減らす。O(n)
    // 容量増加が発生する場合は要素移動が発生する。
    void reserve(size_t n) {
        if (n > nodes.size()) nodes.reserve(n);
    }

    // 全区間の被覆長合計を返す。O(1)
    T covered_length() const {
        return root == null ? T(0) : nodes[root].cov;
    }

    // 指定範囲の被覆長合計を返す。期待計算量 O(log n + k)
    T covered_length(T l, T r) const {
        if (root == null || l >= r) return T(0);
        return range_covered_length(root, l, r);
    }

    // 管理している区間数を返す。O(1)
    int interval_count() const {
        return root == null ? 0 : nodes[root].size;
    }

    // 指定範囲に含まれる区間の積を返す。期待計算量 O(log n + k)
    // 両端が区間を部分的に含む場合は交差部分の getval を用いる。
    S prod(T l, T r) const {
        if (root == null || l >= r) return id_;
        return range_prod(root, l, r);
    }

    // 全区間の積を返す。O(1)
    S all_prod() const {
        return root == null ? id_ : nodes[root].prod;
    }

    // 区間を追加し、必要なら重なり・隣接とマージする。期待計算量 O((m+1) log n)
    void insert(T l, T r) {
        if (l >= r) return;
        if (root == null) {
            root = new_node(l, r);
            return;
        }
        int idx = find_le_by_key(root, l);
        if (idx != null && nodes[idx].r >= l) {
            const T ol = nodes[idx].l;
            const T or_ = nodes[idx].r;
            root = erase_by_key(root, ol);
            if (ol < l) l = ol;
            if (or_ > r) r = or_;
        }
        while (true) {
            idx = find_first_ge_by_key(root, l);
            if (idx == null) break;
            const T ol = nodes[idx].l;
            const T or_ = nodes[idx].r;
            if (ol > r) break;
            root = erase_by_key(root, ol);
            if (ol < l) l = ol;
            if (or_ > r) r = or_;
        }
        root = insert_node(root, new_node(l, r));
    }

    // 区間を削除し、必要なら区間を分割する。期待計算量 O((m+1) log n)
    void erase(T l, T r) {
        if (root == null || l >= r) return;
        while (true) {
            const int idx = first_overlap_idx(root, l, r);
            if (idx == null) break;
            const T ol = nodes[idx].l;
            const T or_ = nodes[idx].r;
            root = erase_by_key(root, ol);
            if (ol < l) {
                const T nl = ol;
                const T nr = std::min(or_, l);
                if (nl < nr) root = insert_node(root, new_node(nl, nr));
            }
            if (or_ > r) {
                const T nl = std::max(ol, r);
                const T nr = or_;
                if (nl < nr) root = insert_node(root, new_node(nl, nr));
            }
        }
    }

    // 指定点が被覆されているか判定する。期待計算量 O(log n)
    bool contains(T x) const { return find(x) != nullptr; }

    // 指定点を含む区間を返す（なければ nullptr）。期待計算量 O(log n)
    // 返されたポインタは木の変更後に無効化されることがある。
    const Node* find(T x) const { return find_impl(root, x); }

    // 指定点を含む区間、なければ左側で最も近い区間を返す（なければ nullptr）。期待計算量 O(log n)
    // 返されたポインタは木の変更後に無効化されることがある。
    const Node* nearest_left(T x) const {
        if (root == null) return nullptr;
        int cur = root;
        const Node* best = nullptr;
        while (cur != null) {
            const Node &n = nodes[cur];
            if (x < n.l) {
                cur = n.ch[0];
            } else if (x >= n.r) {
                best = &n;
                cur = n.ch[1];
            } else {
                return &n;
            }
        }
        return best;
    }

    // 指定点を含む区間、なければ右側で最も近い区間を返す（なければ nullptr）。期待計算量 O(log n)
    // 返されたポインタは木の変更後に無効化されることがある。
    const Node* nearest_right(T x) const {
        if (root == null) return nullptr;
        int cur = root;
        const Node* best = nullptr;
        while (cur != null) {
            const Node &n = nodes[cur];
            if (x < n.l) {
                best = &n;
                cur = n.ch[0];
            } else if (x >= n.r) {
                cur = n.ch[1];
            } else {
                return &n;
            }
        }
        return best;
    }

    // 最左の区間を返す（なければ nullptr）。期待計算量 O(log n)
    // 返されたポインタは木の変更後に無効化されることがある。
    const Node* left_most() const {
        const int idx = left_most_idx(root);
        return idx == null ? nullptr : &nodes[idx];
    }

    // 最右の区間を返す（なければ nullptr）。期待計算量 O(log n)
    // 返されたポインタは木の変更後に無効化されることがある。
    const Node* right_most() const {
        const int idx = right_most_idx(root);
        return idx == null ? nullptr : &nodes[idx];
    }

    // 全区間を左端昇順に列挙する。O(n)
    template <class F>
    void iterate(const F &f) const {
        inorder_all(root, f);
    }

    // 指定範囲と重なる区間を左端昇順に列挙する。期待計算量 O(log n + k)
    template <class F>
    void iterate(T l, T r, const F &f) const {
        if (root == null || l >= r) return;
        inorder_range(root, l, r, f);
    }
};

#if 0

#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>

static constexpr uint64_t MOD = 1000000007ULL;

struct Affine {
    uint64_t a, b;
};

static inline bool operator==(const Affine &x, const Affine &y) {
    return x.a == y.a && x.b == y.b;
}
static inline bool operator!=(const Affine &x, const Affine &y) { return !(x == y); }

static inline uint64_t mod_norm(long long x) {
    long long v = x % (long long)MOD;
    if (v < 0) v += (long long)MOD;
    return (uint64_t)v;
}

static Affine op_affine(Affine x, Affine y) {
    Affine z;
    z.a = (y.a * x.a) % MOD;
    z.b = (y.a * x.b + y.b) % MOD;
    return z;
}

static Affine e_affine() { return Affine{1, 0}; }

static Affine getval_affine(long long l, long long r) {
    const uint64_t L = mod_norm(l);
    const uint64_t R = mod_norm(r);
    const uint64_t len = mod_norm(r - l);
    Affine z;
    z.a = (L + 2 * R + 3 * len + 1) % MOD;
    z.b = (5 * L + 7 * R + 11 * len + 13) % MOD;
    return z;
}

struct OpAffine {
    Affine operator()(Affine x, Affine y) const { return op_affine(x, y); }
};
struct EAffine {
    Affine operator()() const { return e_affine(); }
};
struct GetValAffine {
    template <class U>
    Affine operator()(U l, U r) const {
        return getval_affine((long long)l, (long long)r);
    }
};

static int g_test_failures = 0;

template <class T>
using Treap = IntervalTreap<T, Affine, OpAffine, EAffine, GetValAffine>;

template <class T>
struct NaiveIntervalSet {
    vector<pair<T,T>> segs;

    void clear() { segs.clear(); }

    void insert(T l, T r) {
        if (l >= r) return;
        vector<pair<T,T>> res;
        res.reserve(segs.size() + 1);
        size_t i = 0, n = segs.size();
        while (i < n && segs[i].second < l) res.push_back(segs[i++]);

        T L = l, R = r;
        while (i < n && segs[i].first <= R && segs[i].second >= L) {
            L = std::min(L, segs[i].first);
            R = std::max(R, segs[i].second);
            ++i;
        }
        res.emplace_back(L, R);
        while (i < n) res.push_back(segs[i++]);
        segs.swap(res);
    }

    void erase(T l, T r) {
        if (l >= r) return;
        vector<pair<T,T>> res;
        res.reserve(segs.size());
        for (auto &seg : segs) {
            T a = seg.first;
            T b = seg.second;
            if (b <= l || a >= r) {
                res.emplace_back(a, b);
            } else {
                if (a < l) res.emplace_back(a, l);
                if (b > r) res.emplace_back(r, b);
            }
        }
        if (!res.empty()) {
            sort(res.begin(), res.end());
            vector<pair<T,T>> merged;
            merged.reserve(res.size());
            T curL = res[0].first;
            T curR = res[0].second;
            for (size_t i = 1; i < res.size(); ++i) {
                if (res[i].first <= curR) curR = max(curR, res[i].second);
                else {
                    merged.emplace_back(curL, curR);
                    curL = res[i].first;
                    curR = res[i].second;
                }
            }
            merged.emplace_back(curL, curR);
            segs.swap(merged);
        } else {
            segs.clear();
        }
    }

    T covered_length() const {
        T s = T(0);
        for (auto &seg : segs) {
            T len = seg.second - seg.first;
            assert(len >= T(0));
            s += len;
            assert(s >= T(0));
        }
        return s;
    }

    T covered_length(T l, T r) const {
        if (l >= r) return T(0);
        T s = T(0);
        for (auto &seg : segs) {
            T a = seg.first;
            T b = seg.second;
            if (b <= l) continue;
            if (a >= r) break;
            T L = max(a, l);
            T R = min(b, r);
            if (R > L) {
                T len = R - L;
                assert(len >= T(0));
                s += len;
                assert(s >= T(0));
            }
        }
        return s;
    }

    int interval_count() const { return (int)segs.size(); }

    bool contains(T x) const {
        for (auto &seg : segs) {
            if (x < seg.first) break;
            if (seg.first <= x && x < seg.second) return true;
        }
        return false;
    }

    const pair<T,T>* find(T x) const {
        for (auto &seg : segs) {
            if (x < seg.first) break;
            if (seg.first <= x && x < seg.second) return &seg;
        }
        return nullptr;
    }

    const pair<T,T>* nearest_left(T x) const {
        const pair<T,T>* f = find(x);
        if (f) return f;
        const pair<T,T>* best = nullptr;
        for (auto &seg : segs) {
            if (seg.first <= x) best = &seg;
            else break;
        }
        return best;
    }

    const pair<T,T>* nearest_right(T x) const {
        const pair<T,T>* f = find(x);
        if (f) return f;
        for (auto &seg : segs) {
            if (seg.first >= x) return &seg;
        }
        return nullptr;
    }

    const pair<T,T>* left_most() const {
        return segs.empty() ? nullptr : &segs.front();
    }

    const pair<T,T>* right_most() const {
        return segs.empty() ? nullptr : &segs.back();
    }

    Affine all_prod() const {
        Affine res = e_affine();
        for (auto &seg : segs) res = op_affine(res, getval_affine((long long)seg.first, (long long)seg.second));
        return res;
    }

    Affine prod(T l, T r) const {
        if (l >= r) return e_affine();
        Affine res = e_affine();
        for (auto &seg : segs) {
            T a = seg.first;
            T b = seg.second;
            if (b <= l) continue;
            if (a >= r) break;
            T L = max(a, l);
            T R = min(b, r);
            if (R > L) res = op_affine(res, getval_affine((long long)L, (long long)R));
        }
        return res;
    }
};

template <class T>
static void dump_interval_vec(const vector<pair<T,T>> &v, const char *name) {
    cerr << "  " << name << " (size=" << v.size() << "):";
    for (auto &p : v) cerr << " [" << p.first << "," << p.second << ")";
    cerr << "\n";
}

static void dump_affine(const Affine &x, const char *name) {
    cerr << "  " << name << " = {a=" << x.a << ", b=" << x.b << "}\n";
}

template <class T>
static void verify_small(const Treap<T> &t, const NaiveIntervalSet<T> &n, T MINX, T MAXX) {
    const char *tname = std::is_same_v<T,int> ? "int" : (std::is_same_v<T,long long> ? "long long" : "T");

    if (t.interval_count() != n.interval_count()) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << ">] interval_count mismatch\n";
        cerr << "  treap=" << t.interval_count() << " naive=" << n.interval_count() << "\n";
    }
    if (t.covered_length() != n.covered_length()) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << ">] covered_length() mismatch\n";
        cerr << "  treap=" << t.covered_length() << " naive=" << n.covered_length() << "\n";
    }
    if (t.all_prod() != n.all_prod()) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << ">] all_prod() mismatch\n";
        dump_affine(t.all_prod(), "treap");
        dump_affine(n.all_prod(), "naive");
    }

    for (T L = MINX - T(2); L <= MAXX + T(2); ++L) {
        for (T R = L; R <= MAXX + T(2); ++R) {
            T ct = t.covered_length(L, R);
            T cn = n.covered_length(L, R);
            if (ct != cn) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << ">] covered_length(L,R) mismatch\n";
                cerr << "  L=" << L << " R=" << R << "\n";
                cerr << "  treap=" << ct << " naive=" << cn << "\n";
            }

            const Affine pt = t.prod(L, R);
            const Affine pn = n.prod(L, R);
            if (pt != pn) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << ">] prod(L,R) mismatch\n";
                cerr << "  L=" << L << " R=" << R << "\n";
                dump_affine(pt, "treap");
                dump_affine(pn, "naive");
            }
        }
    }

    for (T x = MINX - T(2); x <= MAXX + T(2); ++x) {
        bool c1 = t.contains(x);
        bool c2 = n.contains(x);
        if (c1 != c2) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << ">] contains mismatch\n";
            cerr << "  x=" << x << " treap=" << c1 << " naive=" << c2 << "\n";
        }

        auto tn = t.find(x);
        auto nn = n.find(x);
        if ((nn == nullptr) != (tn == nullptr)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << ">] find nullptr mismatch\n";
            cerr << "  x=" << x << " treap_null=" << (tn == nullptr) << " naive_null=" << (nn == nullptr) << "\n";
        } else if (nn && tn) {
            if (!(tn->l == nn->first && tn->r == nn->second)) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << ">] find interval mismatch\n";
                cerr << "  x=" << x << " treap=[" << tn->l << "," << tn->r << ") naive=["
                     << nn->first << "," << nn->second << ")\n";
            }
        }

        auto tl = t.nearest_left(x);
        auto nl = n.nearest_left(x);
        if ((nl == nullptr) != (tl == nullptr)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << ">] nearest_left nullptr mismatch\n";
            cerr << "  x=" << x << " treap_null=" << (tl == nullptr) << " naive_null=" << (nl == nullptr) << "\n";
        } else if (nl && tl) {
            if (!(tl->l == nl->first && tl->r == nl->second)) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << ">] nearest_left interval mismatch\n";
                cerr << "  x=" << x << " treap=[" << tl->l << "," << tl->r << ") naive=["
                     << nl->first << "," << nl->second << ")\n";
            }
        }

        auto tr = t.nearest_right(x);
        auto nr = n.nearest_right(x);
        if ((nr == nullptr) != (tr == nullptr)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << ">] nearest_right nullptr mismatch\n";
            cerr << "  x=" << x << " treap_null=" << (tr == nullptr) << " naive_null=" << (nr == nullptr) << "\n";
        } else if (nr && tr) {
            if (!(tr->l == nr->first && tr->r == nr->second)) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << ">] nearest_right interval mismatch\n";
                cerr << "  x=" << x << " treap=[" << tr->l << "," << tr->r << ") naive=["
                     << nr->first << "," << nr->second << ")\n";
            }
        }
    }

    vector<pair<T,T>> v1;
    t.iterate([&](const typename Treap<T>::Node &node){
        v1.emplace_back(node.l, node.r);
    });
    if (v1 != n.segs) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << ">] iterate(all) mismatch\n";
        dump_interval_vec(v1, "treap");
        dump_interval_vec(n.segs, "naive");
    }

    for (T L = MINX - T(2); L <= MAXX + T(2); ++L) {
        for (T R = L; R <= MAXX + T(2); ++R) {
            if (R == L) continue;
            vector<pair<T,T>> vv1;
            t.iterate(L, R, [&](const typename Treap<T>::Node &node){
                vv1.emplace_back(node.l, node.r);
            });
            vector<pair<T,T>> vv2;
            for (auto &seg : n.segs) {
                if (seg.second > L && seg.first < R) vv2.push_back(seg);
            }
            if (vv1 != vv2) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << ">] iterate(L,R) overlap mismatch\n";
                cerr << "  L=" << L << " R=" << R << "\n";
                dump_interval_vec(vv1, "treap");
                dump_interval_vec(vv2, "naive");
                dump_interval_vec(n.segs, "naive_all");
            }
        }
    }

    auto tlm = t.left_most();
    auto nlm = n.left_most();
    if ((nlm == nullptr) != (tlm == nullptr)) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << ">] left_most nullptr mismatch\n";
    } else if (nlm && tlm) {
        if (!(tlm->l == nlm->first && tlm->r == nlm->second)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << ">] left_most interval mismatch\n";
            cerr << "  treap=[" << tlm->l << "," << tlm->r << ") naive=["
                 << nlm->first << "," << nlm->second << ")\n";
        }
    }
    auto trm = t.right_most();
    auto nrm = n.right_most();
    if ((nrm == nullptr) != (trm == nullptr)) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << ">] right_most nullptr mismatch\n";
    } else if (nrm && trm) {
        if (!(trm->l == nrm->first && trm->r == nrm->second)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << ">] right_most interval mismatch\n";
            cerr << "  treap=[" << trm->l << "," << trm->r << ") naive=["
                 << nrm->first << "," << nrm->second << ")\n";
        }
    }
}

template <class T>
static void edge_case_tests_T() {
    const char *tname = std::is_same_v<T,int> ? "int" : (std::is_same_v<T,long long> ? "long long" : "T");

    {
        Treap<T> t;
        NaiveIntervalSet<T> n;

        if (t.interval_count() != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty interval_count != 0\n"; }
        if (t.covered_length() != T(0)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty covered_length != 0\n"; }
        if (t.all_prod() != e_affine()) {
            ++g_test_failures;
            cerr << "[edge_case<" << tname << ">] empty all_prod != e\n";
        }
        if (t.prod(T(-5), T(5)) != e_affine()) {
            ++g_test_failures;
            cerr << "[edge_case<" << tname << ">] empty prod(l,r) != e\n";
        }

        if (t.contains(T(0))) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty contains(0) should be false\n"; }
        if (t.find(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty find(0) should be nullptr\n"; }
        if (t.nearest_left(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty nearest_left(0) should be nullptr\n"; }
        if (t.nearest_right(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty nearest_right(0) should be nullptr\n"; }
        if (t.left_most() != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty left_most should be nullptr\n"; }
        if (t.right_most() != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty right_most should be nullptr\n"; }

        int cnt = 0;
        t.iterate([&](const typename Treap<T>::Node &){ ++cnt; });
        if (cnt != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate(all) on empty should yield 0\n"; }
        t.iterate(T(-10), T(10), [&](const typename Treap<T>::Node &){ ++cnt; });
        if (cnt != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate(l,r) on empty should yield 0\n"; }

        t.insert(T(0), T(10));
        n.insert(T(0), T(10));
        verify_small(t, n, T(-5), T(15));

        if (t.all_prod() != getval_affine(0, 10)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] all_prod single interval mismatch\n"; }
        if (t.prod(T(3), T(7)) != getval_affine(3, 7)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] prod inside single interval mismatch\n"; }
        if (t.prod(T(0), T(10)) != getval_affine(0, 10)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] prod full interval mismatch\n"; }
        if (t.prod(T(10), T(20)) != e_affine()) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] prod no-overlap mismatch\n"; }

        t.insert(T(10), T(20));
        n.insert(T(10), T(20));
        verify_small(t, n, T(-5), T(25));
        if (t.interval_count() != 1) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] adjacency merge failed\n"; }
        if (t.all_prod() != getval_affine(0, 20)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] all_prod after merge mismatch\n"; }

        t.erase(T(5), T(10));
        n.erase(T(5), T(10));
        verify_small(t, n, T(-5), T(30));

        t.erase(T(-10), T(15));
        n.erase(T(-10), T(15));
        verify_small(t, n, T(-20), T(40));

        t.erase(T(0), T(100));
        n.erase(T(0), T(100));
        verify_small(t, n, T(-20), T(40));

        t.insert(T(5), T(5));
        n.insert(T(5), T(5));
        verify_small(t, n, T(0), T(10));

        t.erase(T(5), T(5));
        n.erase(T(5), T(5));
        verify_small(t, n, T(0), T(10));
    }

    {
        Treap<T> t;
        NaiveIntervalSet<T> n;
        auto add = [&](T l, T r) { t.insert(l, r); n.insert(l, r); };

        add(T(0),  T(10));
        add(T(20), T(30));
        add(T(40), T(50));

        const Affine expected = op_affine(op_affine(e_affine(), getval_affine(5, 10)),
                                          op_affine(getval_affine(20, 30), getval_affine(40, 45)));
        const Affine got = t.prod(T(5), T(45));
        if (got != expected) {
            ++g_test_failures;
            cerr << "[edge_case<" << tname << ">] prod boundary mix mismatch\n";
            dump_affine(got, "got");
            dump_affine(expected, "expected");
        }

        verify_small(t, n, T(-5), T(55));
    }

    {
        Treap<T> t;
        NaiveIntervalSet<T> n;

        n.insert(T(-10), T(-5));
        n.insert(T(0), T(10));
        n.insert(T(20), T(30));
        t.insert(T(-10), T(-5));
        t.insert(T(0), T(10));
        t.insert(T(20), T(30));

        vector<T> xs = {
            T(-100), T(-10), T(-7), T(-5), T(-1),
            T(0), T(5), T(10), T(15), T(20), T(25), T(30), T(100)
        };
        for (T x : xs) {
            auto tl = t.nearest_left(x);
            auto nl = n.nearest_left(x);
            if ((nl == nullptr) != (tl == nullptr)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] nearest_left nullptr mismatch\n"; }
            else if (nl && tl) {
                if (!(tl->l == nl->first && tl->r == nl->second)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] nearest_left interval mismatch\n"; }
            }

            auto tr = t.nearest_right(x);
            auto nr = n.nearest_right(x);
            if ((nr == nullptr) != (tr == nullptr)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] nearest_right nullptr mismatch\n"; }
            else if (nr && tr) {
                if (!(tr->l == nr->first && tr->r == nr->second)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] nearest_right interval mismatch\n"; }
            }
        }
    }

    {
        Treap<T> t;
        NaiveIntervalSet<T> n;
        auto add = [&](T l, T r) { t.insert(l, r); n.insert(l, r); };

        add(T(0),  T(10));
        add(T(20), T(30));
        add(T(40), T(50));

        {
            vector<pair<T,T>> v1;
            t.iterate(T(5), T(25), [&](const typename Treap<T>::Node &node){
                v1.emplace_back(node.l, node.r);
            });
            vector<pair<T,T>> v2;
            for (auto &seg : n.segs) if (seg.second > T(5) && seg.first < T(25)) v2.push_back(seg);
            if (v1 != v2) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate overlap mismatch [5,25)\n"; }
        }

        {
            vector<pair<T,T>> v1;
            t.iterate(T(10), T(20), [&](const typename Treap<T>::Node &node){
                v1.emplace_back(node.l, node.r);
            });
            if (!v1.empty()) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate overlap mismatch [10,20)\n"; }
        }

        {
            vector<pair<T,T>> v1;
            t.iterate(T(-5), T(100), [&](const typename Treap<T>::Node &node){
                v1.emplace_back(node.l, node.r);
            });
            vector<pair<T,T>> v2;
            for (auto &seg : n.segs) if (seg.second > T(-5) && seg.first < T(100)) v2.push_back(seg);
            if (v1 != v2) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate overlap mismatch [-5,100)\n"; }
        }

        verify_small(t, n, T(-10), T(60));
    }
}

template <class T>
static void random_small_tests_T() {
    std::mt19937_64 rng(123456789);
    const T MINX = T(-10);
    const T MAXX = T(10);
    std::uniform_int_distribution<long long> dist((long long)MINX - 5, (long long)MAXX + 5);
    const int CASES = 200;
    const int OPS = 200;

    for (int tc = 0; tc < CASES; ++tc) {
        Treap<T> t;
        NaiveIntervalSet<T> n;
        for (int opi = 0; opi < OPS; ++opi) {
            T a = (T)dist(rng);
            T b = (T)dist(rng);
            T l = std::min(a, b);
            T r = std::max(a, b);
            if ((rng() & 7) == 0) r = l;
            else if (l == r) ++r;

            if (rng() & 1) { t.insert(l, r); n.insert(l, r); }
            else { t.erase(l, r); n.erase(l, r); }

            verify_small(t, n, MINX, MAXX);
        }
    }
}

template <class T>
static void random_large_tests_T() {
    const char *tname = std::is_same_v<T,int> ? "int" : (std::is_same_v<T,long long> ? "long long" : "T");

    std::mt19937_64 rng(987654321);
    const T MINX = T(-1000000);
    const T MAXX = T(1000000);
    std::uniform_int_distribution<long long> dist((long long)MINX, (long long)MAXX);
    const int CASES = 50;
    const int OPS = 300;

    for (int tc = 0; tc < CASES; ++tc) {
        Treap<T> t;
        NaiveIntervalSet<T> n;
        for (int opi = 0; opi < OPS; ++opi) {
            T a = (T)dist(rng);
            T b = (T)dist(rng);
            T l = std::min(a, b);
            T r = std::max(a, b);
            if ((rng() & 7) == 0) r = l;
            else if (l == r) ++r;

            if (rng() & 1) { t.insert(l, r); n.insert(l, r); }
            else { t.erase(l, r); n.erase(l, r); }

            if (t.all_prod() != n.all_prod()) {
                ++g_test_failures;
                cerr << "[random_large<" << tname << ">] all_prod mismatch\n";
            }

            for (int i = 0; i < 10; ++i) {
                T x = (T)dist(rng);
                bool c1 = t.contains(x);
                bool c2 = n.contains(x);
                if (c1 != c2) { ++g_test_failures; cerr << "[random_large<" << tname << ">] contains mismatch\n"; }

                auto tn = t.find(x);
                auto nn = n.find(x);
                if ((nn == nullptr) != (tn == nullptr)) { ++g_test_failures; cerr << "[random_large<" << tname << ">] find nullptr mismatch\n"; }
                else if (nn && tn) {
                    if (!(tn->l == nn->first && tn->r == nn->second)) { ++g_test_failures; cerr << "[random_large<" << tname << ">] find interval mismatch\n"; }
                }

                auto tl = t.nearest_left(x);
                auto nl = n.nearest_left(x);
                if ((nl == nullptr) != (tl == nullptr)) { ++g_test_failures; cerr << "[random_large<" << tname << ">] nearest_left nullptr mismatch\n"; }
                else if (nl && tl) {
                    if (!(tl->l == nl->first && tl->r == nl->second)) { ++g_test_failures; cerr << "[random_large<" << tname << ">] nearest_left interval mismatch\n"; }
                }

                auto tr = t.nearest_right(x);
                auto nr = n.nearest_right(x);
                if ((nr == nullptr) != (tr == nullptr)) { ++g_test_failures; cerr << "[random_large<" << tname << ">] nearest_right nullptr mismatch\n"; }
                else if (nr && tr) {
                    if (!(tr->l == nr->first && tr->r == nr->second)) { ++g_test_failures; cerr << "[random_large<" << tname << ">] nearest_right interval mismatch\n"; }
                }
            }

            for (int i = 0; i < 5; ++i) {
                T x1 = (T)dist(rng);
                T x2 = (T)dist(rng);
                T L = std::min(x1, x2);
                T R = std::max(x1, x2);
                if (L == R) ++R;
                T ct = t.covered_length(L, R);
                T cn = n.covered_length(L, R);
                if (ct != cn) { ++g_test_failures; cerr << "[random_large<" << tname << ">] covered_length mismatch\n"; }
            }

            for (int i = 0; i < 5; ++i) {
                T x1 = (T)dist(rng);
                T x2 = (T)dist(rng);
                T L = std::min(x1, x2);
                T R = std::max(x1, x2);
                if ((rng() & 7) == 0) R = L;
                if (L == R) ++R;
                const Affine pt = t.prod(L, R);
                const Affine pn = n.prod(L, R);
                if (pt != pn) { ++g_test_failures; cerr << "[random_large<" << tname << ">] prod mismatch\n"; }
            }

            for (int i = 0; i < 3; ++i) {
                T x1 = (T)dist(rng);
                T x2 = (T)dist(rng);
                T L = std::min(x1, x2);
                T R = std::max(x1, x2);
                if (L == R) ++R;

                vector<pair<T,T>> vv1;
                t.iterate(L, R, [&](const typename Treap<T>::Node &node){
                    vv1.emplace_back(node.l, node.r);
                });
                vector<pair<T,T>> vv2;
                for (auto &seg : n.segs) if (seg.second > L && seg.first < R) vv2.push_back(seg);

                if (vv1 != vv2) {
                    ++g_test_failures;
                    cerr << "[random_large<" << tname << ">] iterate overlap mismatch\n";
                    cerr << "  L=" << L << " R=" << R << "\n";
                    dump_interval_vec(vv1, "treap");
                    dump_interval_vec(vv2, "naive");
                }
            }
        }
    }
}

template <class T>
static void benchmark_case_T(size_t N, std::mt19937_64 &rng) {
    using namespace std::chrono;
    cout << "Benchmark N=" << N << " intervals (T=" << (std::is_same_v<T,int> ? "int" : "long long") << ")\n";

    vector<pair<T,T>> intervals;
    intervals.reserve(N);
    const T STEP = T(8);
    const int MAX_LEN_INT = 4;
    for (size_t i = 0; i < N; ++i) {
        T l = T(i) * STEP;
        int len_i = 1 + int(rng() % MAX_LEN_INT);
        T len = T(len_i);
        T r = l + len;
        intervals.emplace_back(l, r);
    }
    const T MAX_COORD_T = T(N) * STEP + T(MAX_LEN_INT + 10);
    const long long MAX_COORD_LL = (long long)MAX_COORD_T;

    int repeat = (N <= 1000) ? 5 : 3;

    {
        long long total_ns = 0;
        for (int rep = 0; rep < repeat; ++rep) {
            Treap<T> t;
            t.reserve(N);
            auto t0 = high_resolution_clock::now();
            for (auto &seg : intervals) t.insert(seg.first, seg.second);
            auto t1 = high_resolution_clock::now();
            total_ns += duration_cast<nanoseconds>(t1 - t0).count();
        }
        double avg_ms = (double)total_ns / repeat / 1e6;
        cout << "  Insert-all: iterations=" << repeat
             << ", ops=" << N
             << ", avg_ms=" << fixed << setprecision(3) << avg_ms << "\n";
    }

    {
        long long total_ns = 0;
        for (int rep = 0; rep < repeat; ++rep) {
            Treap<T> t;
            t.reserve(N);
            for (auto &seg : intervals) t.insert(seg.first, seg.second);
            auto t0 = high_resolution_clock::now();
            for (auto &seg : intervals) {
                t.erase(seg.first, seg.second);
                t.insert(seg.first, seg.second);
            }
            auto t1 = high_resolution_clock::now();
            total_ns += duration_cast<nanoseconds>(t1 - t0).count();
        }
        double avg_ms = (double)total_ns / repeat / 1e6;
        cout << "  Insert+Erase: iterations=" << repeat
             << ", ops=" << (N * 2)
             << ", avg_ms=" << fixed << setprecision(3) << avg_ms << "\n";
    }

    {
        Treap<T> t;
        t.reserve(N);
        for (auto &seg : intervals) t.insert(seg.first, seg.second);

        const size_t Q_base = (N <= 1000) ? 200000 : 100000;
        std::uniform_int_distribution<long long> distCoord(0, MAX_COORD_LL);

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) sink = sink + (long long)t.covered_length();
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  covered_length(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T a = (T)distCoord(rng);
                T b = (T)distCoord(rng);
                T l = std::min(a, b);
                T r = std::max(a, b);
                if (l == r) ++r;
                sink = sink + (long long)t.covered_length(l, r);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  covered_length(l,r): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile uint64_t sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                const Affine x = t.all_prod();
                sink = sink ^ (x.a + 31 * x.b);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  all_prod(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = (N <= 1000) ? Q_base : (Q_base / 2);
            volatile uint64_t sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T a = (T)distCoord(rng);
                T b = (T)distCoord(rng);
                T l = std::min(a, b);
                T r = std::max(a, b);
                if (l == r) ++r;
                const Affine x = t.prod(l, r);
                sink = sink ^ (x.a + 131 * x.b);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  prod(l,r): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) sink = sink + t.interval_count();
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  interval_count(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                sink = sink + (long long)t.contains(x);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  contains(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                auto p = t.find(x);
                if (p) sink = sink + (long long)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  find(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                auto p = t.nearest_left(x);
                if (p) sink = sink + (long long)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  nearest_left(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                auto p = t.nearest_right(x);
                if (p) sink = sink + (long long)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  nearest_right(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                auto p = t.left_most();
                if (p) sink = sink + (long long)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  left_most(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                auto p = t.right_most();
                if (p) sink = sink + (long long)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  right_most(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = (N <= 1000) ? 200 : 5;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                t.iterate([&](const typename Treap<T>::Node &node){
                    sink = sink + (long long)(node.r - node.l);
                });
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  iterate(all): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = (N <= 1000) ? 200 : 5;
            volatile long long sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T a = (T)distCoord(rng);
                T b = (T)distCoord(rng);
                T l = std::min(a, b);
                T r = std::max(a, b);
                if (l == r) ++r;
                t.iterate(l, r, [&](const typename Treap<T>::Node &node){
                    sink = sink + (long long)(node.r - node.l);
                });
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = static_cast<double>(duration_cast<nanoseconds>(t1 - t0).count()) / static_cast<double>(Q);
            cout << "  iterate(l,r): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "Running edge-case tests (int)...\n";
    edge_case_tests_T<int>();
    cout << "Edge-case tests (int) done.\n";

    cout << "Running random small tests (int)...\n";
    random_small_tests_T<int>();
    cout << "Random small tests (int) done.\n";

    cout << "Running random large tests (int)...\n";
    random_large_tests_T<int>();
    cout << "Random large tests (int) done.\n";

    cout << "Running edge-case tests (long long)...\n";
    edge_case_tests_T<long long>();
    cout << "Edge-case tests (long long) done.\n";

    cout << "Running random small tests (long long)...\n";
    random_small_tests_T<long long>();
    cout << "Random small tests (long long) done.\n";

    cout << "Running random large tests (long long)...\n";
    random_large_tests_T<long long>();
    cout << "Random large tests (long long) done.\n";

    if (g_test_failures > 0) {
        cerr << "TOTAL TEST FAILURES: " << g_test_failures << "\n";
        return 1;
    } else {
        cout << "All tests passed.\n";
    }

    std::mt19937_64 rng(20251211);
    cout << "Running benchmarks...\n";
    benchmark_case_T<int>(1000, rng);
    benchmark_case_T<int>(1000000, rng);
    benchmark_case_T<long long>(1000, rng);
    benchmark_case_T<long long>(1000000, rng);

    cout << "All tests and benchmarks finished.\n";
    return 0;
}

#endif


// Running edge-case tests (int)...
// Edge-case tests (int) done.
// Running random small tests (int)...
// Random small tests (int) done.
// Running random large tests (int)...
// Random large tests (int) done.
// Running edge-case tests (long long)...
// Edge-case tests (long long) done.
// Running random small tests (long long)...
// Random small tests (long long) done.
// Running random large tests (long long)...
// Random large tests (long long) done.
// All tests passed.
// Running benchmarks...
// Benchmark N=1000 intervals (T=int)
//   Insert-all: iterations=5, ops=1000, avg_ms=0.096
//   Insert+Erase: iterations=5, ops=2000, avg_ms=0.687
//   covered_length(): calls=200000, avg_ns=0.3
//   covered_length(l,r): calls=200000, avg_ns=245.2
//   all_prod(): calls=200000, avg_ns=0.3
//   prod(l,r): calls=200000, avg_ns=247.6
//   interval_count(): calls=200000, avg_ns=1.8
//   contains(x): calls=200000, avg_ns=78.0
//   find(x): calls=200000, avg_ns=79.1
//   nearest_left(x): calls=200000, avg_ns=79.4
//   nearest_right(x): calls=200000, avg_ns=79.2
//   left_most(): calls=200000, avg_ns=5.5
//   right_most(): calls=200000, avg_ns=4.0
//   iterate(all): calls=200, avg_ns=2711.3
//   iterate(l,r): calls=200, avg_ns=1228.3
// Benchmark N=1000000 intervals (T=int)
//   Insert-all: iterations=3, ops=1000000, avg_ms=222.833
//   Insert+Erase: iterations=3, ops=2000000, avg_ms=1258.702
//   covered_length(): calls=100000, avg_ns=0.4
//   covered_length(l,r): calls=100000, avg_ns=1940.7
//   all_prod(): calls=100000, avg_ns=0.3
//   prod(l,r): calls=50000, avg_ns=1926.7
//   interval_count(): calls=100000, avg_ns=1.7
//   contains(x): calls=100000, avg_ns=768.9
//   find(x): calls=100000, avg_ns=732.5
//   nearest_left(x): calls=100000, avg_ns=825.1
//   nearest_right(x): calls=100000, avg_ns=861.7
//   left_most(): calls=100000, avg_ns=12.4
//   right_most(): calls=100000, avg_ns=9.9
//   iterate(all): calls=5, avg_ns=16946468.0
//   iterate(l,r): calls=5, avg_ns=4114993.8
// Benchmark N=1000 intervals (T=long long)
//   Insert-all: iterations=5, ops=1000, avg_ms=0.084
//   Insert+Erase: iterations=5, ops=2000, avg_ms=0.683
//   covered_length(): calls=200000, avg_ns=0.3
//   covered_length(l,r): calls=200000, avg_ns=240.8
//   all_prod(): calls=200000, avg_ns=0.3
//   prod(l,r): calls=200000, avg_ns=251.4
//   interval_count(): calls=200000, avg_ns=0.3
//   contains(x): calls=200000, avg_ns=79.4
//   find(x): calls=200000, avg_ns=79.6
//   nearest_left(x): calls=200000, avg_ns=82.5
//   nearest_right(x): calls=200000, avg_ns=79.2
//   left_most(): calls=200000, avg_ns=6.2
//   right_most(): calls=200000, avg_ns=3.6
//   iterate(all): calls=200, avg_ns=2456.5
//   iterate(l,r): calls=200, avg_ns=1288.1
// Benchmark N=1000000 intervals (T=long long)
//   Insert-all: iterations=3, ops=1000000, avg_ms=201.585
//   Insert+Erase: iterations=3, ops=2000000, avg_ms=1093.401
//   covered_length(): calls=100000, avg_ns=0.3
//   covered_length(l,r): calls=100000, avg_ns=1794.9
//   all_prod(): calls=100000, avg_ns=0.3
//   prod(l,r): calls=50000, avg_ns=1937.3
//   interval_count(): calls=100000, avg_ns=0.3
//   contains(x): calls=100000, avg_ns=754.0
//   find(x): calls=100000, avg_ns=768.3
//   nearest_left(x): calls=100000, avg_ns=755.0
//   nearest_right(x): calls=100000, avg_ns=771.7
//   left_most(): calls=100000, avg_ns=10.1
//   right_most(): calls=100000, avg_ns=7.3
//   iterate(all): calls=5, avg_ns=17464661.0
//   iterate(l,r): calls=5, avg_ns=5049481.6
// All tests and benchmarks finished.
