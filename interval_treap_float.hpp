#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <type_traits>
#include <cassert>
#include <cmath>

using namespace std;

// 区間 [l, r) の集合を管理する Treap。
// 区間は常に「互いに非交差・非隣接」に正規化され、浮動小数では NaN/Inf を無効入力として扱う（eps は隣接マージ判定のみに使用）。
template <class T, class Len = T>
class IntervalTreap {
    static_assert(
        ((std::is_integral_v<T> && std::is_signed_v<T>) || std::is_floating_point_v<T>),
        "IntervalTreap<T,Len>: T must be a signed integral or floating point type"
    );
    static_assert(
        ((std::is_integral_v<Len> && std::is_signed_v<Len>) || std::is_floating_point_v<Len>),
        "IntervalTreap<T,Len>: Len must be a signed integral or floating point type"
    );
    static_assert(std::is_convertible_v<T, Len>,
                  "IntervalTreap<T,Len>: T must be convertible to Len");

public:
    // 管理区間と部分木集計を保持するノード（木の更新で参照は無効になりうる）。
    struct Node {
        T l, r;        // 区間の端点（半開区間）。
        int ch[2];     // 子ノードのインデックス（-1 はなし）。
        unsigned pri;  // 優先度。
        int size;      // 部分木のノード数。
        T min_l;       // 部分木内の最小 l。
        T max_r;       // 部分木内の最大 r。
        Len cov;       // 部分木内の被覆長合計（Len）。
    };

private:
    vector<Node> nodes;
    vector<int> free_list;
    int root;
    uint32_t rng_state;

    T abs_eps_;
    T rel_eps_;

    static constexpr int null = -1;

    static constexpr bool kTIsFloat = std::is_floating_point_v<T>;
    static constexpr bool kLenIsFloat = std::is_floating_point_v<Len>;

    static T canonicalize_value(T v) {
        if constexpr (kTIsFloat) {
            return (v == T(0)) ? T(0) : v;
        } else {
            return v;
        }
    }

    static bool is_valid_coord(T v) {
        if constexpr (kTIsFloat) {
            return std::isfinite(v);
        } else {
            return true;
        }
    }

    static bool normalize_and_check(T &v) {
        v = canonicalize_value(v);
        if (!is_valid_coord(v)) return false;
        return true;
    }

    static bool normalize_and_check_interval(T &l, T &r) {
        if (!normalize_and_check(l)) return false;
        if (!normalize_and_check(r)) return false;
        return true;
    }

    T tol(T a, T b) const {
        if constexpr (kTIsFloat) {
            T aa = std::fabs(a);
            T bb = std::fabs(b);
            T scale = std::max(aa, bb);
            return abs_eps_ + rel_eps_ * scale;
        } else {
            return T(0);
        }
    }

    bool ge_eps(T a, T b) const {
        if constexpr (kTIsFloat) {
            return a + tol(a, b) >= b;
        } else {
            return a >= b;
        }
    }

    bool gt_eps(T a, T b) const {
        if constexpr (kTIsFloat) {
            return a > b + tol(a, b);
        } else {
            return a > b;
        }
    }

    static void assert_len_ok(Len v) {
        if constexpr (kLenIsFloat) {
            assert(std::isfinite((long double)v));
        }
    }

    uint32_t next_rand() {
        uint32_t x = rng_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng_state = x ? x : 0x12345678u;
        return rng_state;
    }

    int new_node(T l, T r) {
        int idx;
        if (!free_list.empty()) {
            idx = free_list.back();
            free_list.pop_back();
        } else {
            idx = (int)nodes.size();
            nodes.push_back(Node());
        }
        Node &n = nodes[idx];
        n.l = l;
        n.r = r;
        n.ch[0] = n.ch[1] = null;
        n.pri = next_rand();
        n.size = 1;
        n.min_l = l;
        n.max_r = r;

        Len len = Len(r) - Len(l);
        assert(len >= Len(0));
        assert_len_ok(len);
        n.cov = len;
        return idx;
    }

    void free_node(int idx) {
        free_list.push_back(idx);
    }

    void pull(int idx) {
        Node &x = nodes[idx];
        int L = x.ch[0];
        int R = x.ch[1];

        x.size = 1;
        x.min_l = x.l;
        x.max_r = x.r;

        Len len = Len(x.r) - Len(x.l);
        assert(len >= Len(0));
        assert_len_ok(len);

        x.cov = len;
        if (L != null) {
            const Node &ln = nodes[L];
            x.size += ln.size;
            x.min_l = ln.min_l;
            x.max_r = std::max(x.max_r, ln.max_r);
            x.cov += ln.cov;
            assert(x.cov >= Len(0));
            assert_len_ok(x.cov);
        }
        if (R != null) {
            const Node &rn = nodes[R];
            x.size += rn.size;
            x.max_r = std::max(x.max_r, rn.max_r);
            x.cov += rn.cov;
            assert(x.cov >= Len(0));
            assert_len_ok(x.cov);
        }
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
        int L = nodes[cur].ch[0];
        int R = nodes[cur].ch[1];
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

    Len range_covered_length(int cur, T l, T r) const {
        if (cur == null) return Len(0);

        Len res = Len(0);
        static thread_local std::vector<int> st;
        st.clear();
        st.push_back(cur);

        while (!st.empty()) {
            int idx = st.back();
            st.pop_back();
            if (idx == null) continue;

            const Node &x = nodes[idx];

            if (x.max_r <= l || x.min_l >= r) continue;

            if (x.min_l >= l && x.max_r <= r) {
                res += x.cov;
                assert(res >= Len(0));
                assert_len_ok(res);
                continue;
            }

            if (x.r > l && x.l < r) {
                T a = x.l < l ? l : x.l;
                T b = x.r > r ? r : x.r;
                if (b > a) {
                    Len len = Len(b) - Len(a);
                    assert(len >= Len(0));
                    assert_len_ok(len);
                    res += len;
                    assert(res >= Len(0));
                    assert_len_ok(res);
                }
            }

            int L = x.ch[0];
            int R = x.ch[1];
            if (L != null) st.push_back(L);
            if (R != null) st.push_back(R);
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

    static bool eps_valid(T v) {
        if constexpr (kTIsFloat) return std::isfinite(v) && (v >= T(0));
        else return true;
    }

public:
    // 空集合を構築する。O(1)
    // eps は隣接マージの許容誤差；例: floatならabs_eps=1e-5f, doubleならabs_eps=1e-12, rel_eps=0。
    IntervalTreap(size_t reserve_nodes = 0, T abs_eps = T(0), T rel_eps = T(0))
        : nodes(), free_list(), root(null), rng_state(0x12345678u),
          abs_eps_(T(0)), rel_eps_(T(0)) {

        if (reserve_nodes) nodes.reserve(reserve_nodes);

        abs_eps = canonicalize_value(abs_eps);
        rel_eps = canonicalize_value(rel_eps);

        if constexpr (kTIsFloat) {
            if (eps_valid(abs_eps) && eps_valid(rel_eps)) {
                abs_eps_ = abs_eps;
                rel_eps_ = rel_eps;
            } else {
                abs_eps_ = T(0);
                rel_eps_ = T(0);
            }
        } else {
            abs_eps_ = T(0);
            rel_eps_ = T(0);
        }
    }

    // eps を設定する。O(1)
    // 目安: abs_eps は「隣接とみなしたい最大誤差」；例: floatなら1e-5, doubleなら1e-12。
    void set_epsilon(T abs_eps, T rel_eps = T(0)) {
        abs_eps = canonicalize_value(abs_eps);
        rel_eps = canonicalize_value(rel_eps);
        if constexpr (kTIsFloat) {
            if (eps_valid(abs_eps) && eps_valid(rel_eps)) {
                abs_eps_ = abs_eps;
                rel_eps_ = rel_eps;
            } else {
                abs_eps_ = T(0);
                rel_eps_ = T(0);
            }
        } else {
            (void)abs_eps; (void)rel_eps;
            abs_eps_ = T(0);
            rel_eps_ = T(0);
        }
    }

    // abs eps を返す。O(1)
    T abs_epsilon() const { return abs_eps_; }

    // rel eps を返す。O(1)
    T rel_epsilon() const { return rel_eps_; }

    // 全データを破棄して初期状態に戻す。O(n)
    void clear() {
        nodes.clear();
        free_list.clear();
        root = null;
        rng_state = 0x12345678u;
    }

    // 内部領域を確保して再確保回数を減らす。O(n)（容量不足時）
    void reserve(size_t n) {
        if (n > nodes.size()) nodes.reserve(n);
    }

    // 全区間の被覆長合計を返す。O(1)
    Len covered_length() const {
        if (root == null) return Len(0);
        return nodes[root].cov;
    }

    // 指定範囲の被覆長合計を返す。期待 O(log n + k)（最悪 O(n)）
    // 無効入力（NaN/Inf 等）は 0 を返す。
    Len covered_length(T l, T r) const {
        if (!normalize_and_check_interval(l, r)) return Len(0);
        if (root == null || l >= r) return Len(0);
        return range_covered_length(root, l, r);
    }

    // 管理している区間数を返す。O(1)
    int interval_count() const {
        return root == null ? 0 : nodes[root].size;
    }

    // 区間を追加し、重なり・隣接した区間数をkとして 期待 O((k+1) log n)（最悪 O(n log n)）
    // 無効入力（NaN/Inf 等）は何もしない。
    void insert(T l, T r) {
        if (!normalize_and_check_interval(l, r)) return;
        if (l >= r) return;

        if (root == null) {
            root = new_node(l, r);
            return;
        }

        int idx = find_le_by_key(root, l);
        if (idx != null && ge_eps(nodes[idx].r, l)) {
            Node old = nodes[idx];
            root = erase_by_key(root, old.l);
            if (old.l < l) l = old.l;
            if (old.r > r) r = old.r;
        }

        while (true) {
            idx = find_first_ge_by_key(root, l);
            if (idx == null) break;
            Node old = nodes[idx];

            if (gt_eps(old.l, r)) break;

            root = erase_by_key(root, old.l);
            if (old.l < l) l = old.l;
            if (old.r > r) r = old.r;
        }

        int node = new_node(l, r);
        root = insert_node(root, node);
    }

    // 区間を削除し、影響する区間数をkとして 期待 O(k log n)（最悪 O(n log n)）
    // 無効入力（NaN/Inf 等）は何もしない。
    void erase(T l, T r) {
        if (!normalize_and_check_interval(l, r)) return;
        if (root == null || l >= r) return;

        while (true) {
            int idx = first_overlap_idx(root, l, r);
            if (idx == null) break;

            Node old = nodes[idx];
            root = erase_by_key(root, old.l);

            if (old.l < l) {
                T nl = old.l;
                T nr = std::min(old.r, l);
                if (nl < nr) root = insert_node(root, new_node(nl, nr));
            }

            if (old.r > r) {
                T nl = std::max(old.l, r);
                T nr = old.r;
                if (nl < nr) root = insert_node(root, new_node(nl, nr));
            }
        }
    }

    // 指定点が被覆されているか判定する。期待 O(log n)（最悪 O(n)）
    // 無効入力（NaN/Inf 等）は false を返す。
    bool contains(T x) const {
        return find(x) != nullptr;
    }

    // 指定点を含む区間を返す（なければ nullptr）。期待 O(log n)（最悪 O(n)）
    // 返るポインタは木の更新で無効になりうる。
    const Node* find(T x) const {
        if (!normalize_and_check(x)) return nullptr;
        return find_impl(root, x);
    }

    // 指定点を含む区間、なければ左側で最も近い区間を返す（なければ nullptr）。期待 O(log n)（最悪 O(n)）
    // 返るポインタは木の更新で無効になりうる。
    const Node* nearest_left(T x) const {
        if (!normalize_and_check(x)) return nullptr;
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

    // 指定点を含む区間、なければ右側で最も近い区間を返す（なければ nullptr）。期待 O(log n)（最悪 O(n)）
    // 返るポインタは木の更新で無効になりうる。
    const Node* nearest_right(T x) const {
        if (!normalize_and_check(x)) return nullptr;
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

    // 最左の区間を返す（なければ nullptr）。期待 O(log n)（最悪 O(n)）
    // 返るポインタは木の更新で無効になりうる。
    const Node* left_most() const {
        int idx = left_most_idx(root);
        return idx == null ? nullptr : &nodes[idx];
    }

    // 最右の区間を返す（なければ nullptr）。期待 O(log n)（最悪 O(n)）
    // 返るポインタは木の更新で無効になりうる。
    const Node* right_most() const {
        int idx = right_most_idx(root);
        return idx == null ? nullptr : &nodes[idx];
    }

    // 全区間を昇順に列挙する。O(n)
    template <class F>
    void iterate(const F &f) const {
        inorder_all(root, f);
    }

    // 指定範囲と重なる区間を昇順に列挙する。期待 O(log n + k)（最悪 O(n)）
    // 無効入力（NaN/Inf 等）は列挙しない。
    template <class F>
    void iterate(T l, T r, const F &f) const {
        if (!normalize_and_check_interval(l, r)) return;
        if (root == null || l >= r) return;
        inorder_range(root, l, r, f);
    }
};

#if 0

#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>
#include <string>

static int g_test_failures = 0;

template <class X>
static X abs_x(X v) {
    if constexpr (std::is_floating_point_v<X>) {
        return (X)std::fabs((long double)v);
    } else {
        return v < X(0) ? -v : v;
    }
}

template <class X>
static bool approx_equal(X a, X b, X abs_tol, X rel_tol) {
    if constexpr (!std::is_floating_point_v<X>) {
        return a == b;
    } else {
        if (!std::isfinite((long double)a) || !std::isfinite((long double)b)) return false;
        X diff = abs_x(a - b);
        X m = std::max(abs_x(a), abs_x(b));
        return diff <= abs_tol + rel_tol * m;
    }
}

template <class T>
static T default_coord_abs_tol() {
    if constexpr (std::is_same_v<T, float>) return (T)1e-5f;
    if constexpr (std::is_same_v<T, double>) return (T)1e-12;
    return (T)1e-12;
}

template <class T>
static T default_coord_rel_tol() {
    if constexpr (std::is_same_v<T, float>) return (T)1e-5f;
    if constexpr (std::is_same_v<T, double>) return (T)1e-12;
    return (T)1e-12;
}

template <class Len>
static Len default_len_abs_tol() {
    if constexpr (std::is_same_v<Len, float>) return (Len)1e-4f;
    if constexpr (std::is_same_v<Len, double>) return (Len)1e-9;
    if constexpr (std::is_same_v<Len, long double>) return (Len)1e-12L;
    return (Len)0;
}

template <class Len>
static Len default_len_rel_tol() {
    if constexpr (std::is_same_v<Len, float>) return (Len)1e-5f;
    if constexpr (std::is_same_v<Len, double>) return (Len)1e-12;
    if constexpr (std::is_same_v<Len, long double>) return (Len)1e-15L;
    return (Len)0;
}

template <class T>
static const char* type_name_T() {
    if constexpr (std::is_same_v<T, int>) return "int";
    if constexpr (std::is_same_v<T, long long>) return "long long";
    if constexpr (std::is_same_v<T, float>) return "float";
    if constexpr (std::is_same_v<T, double>) return "double";
    return "T";
}

template <class Len>
static const char* type_name_Len() {
    if constexpr (std::is_same_v<Len, int>) return "int";
    if constexpr (std::is_same_v<Len, long long>) return "long long";
    if constexpr (std::is_same_v<Len, float>) return "float";
    if constexpr (std::is_same_v<Len, double>) return "double";
    if constexpr (std::is_same_v<Len, long double>) return "long double";
    return "Len";
}

template <class T, class Len>
struct NaiveIntervalSet {
    vector<pair<T,T>> segs;
    T abs_eps = T(0);
    T rel_eps = T(0);

    static bool is_valid_coord(T v) {
        if constexpr (std::is_floating_point_v<T>) return std::isfinite(v);
        else return true;
    }
    static T canonicalize(T v) {
        if constexpr (std::is_floating_point_v<T>) return (v == T(0)) ? T(0) : v;
        else return v;
    }

    bool normalize_and_check(T &v) const {
        v = canonicalize(v);
        return is_valid_coord(v);
    }
    bool normalize_and_check_interval(T &l, T &r) const {
        if (!normalize_and_check(l)) return false;
        if (!normalize_and_check(r)) return false;
        return true;
    }

    T tol(T a, T b) const {
        if constexpr (std::is_floating_point_v<T>) {
            T aa = std::fabs(a);
            T bb = std::fabs(b);
            T scale = std::max(aa, bb);
            return abs_eps + rel_eps * scale;
        } else {
            return T(0);
        }
    }
    bool ge_eps(T a, T b) const {
        if constexpr (std::is_floating_point_v<T>) return a + tol(a,b) >= b;
        else return a >= b;
    }
    bool gt_eps(T a, T b) const {
        if constexpr (std::is_floating_point_v<T>) return a > b + tol(a,b);
        else return a > b;
    }

    void clear() { segs.clear(); }

    void insert(T l, T r) {
        if (!normalize_and_check_interval(l, r)) return;
        if (l >= r) return;

        vector<pair<T,T>> res;
        res.reserve(segs.size() + 1);
        size_t i = 0, n = segs.size();

        while (i < n && gt_eps(l, segs[i].second)) res.push_back(segs[i++]);

        T L = l, R = r;

        while (i < n) {
            if (gt_eps(segs[i].first, R)) break;
            if (gt_eps(L, segs[i].second)) { ++i; continue; }

            L = std::min(L, segs[i].first);
            R = std::max(R, segs[i].second);
            ++i;
        }

        res.emplace_back(L, R);
        while (i < n) res.push_back(segs[i++]);
        segs.swap(res);
    }

    void erase(T l, T r) {
        if (!normalize_and_check_interval(l, r)) return;
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

    Len covered_length() const {
        Len s = Len(0);
        for (auto &seg : segs) {
            Len len = Len(seg.second) - Len(seg.first);
            assert(len >= Len(0));
            s += len;
        }
        return s;
    }

    Len covered_length(T l, T r) const {
        if (!const_cast<NaiveIntervalSet*>(this)->normalize_and_check_interval(l, r)) return Len(0);
        if (l >= r) return Len(0);

        Len s = Len(0);
        for (auto &seg : segs) {
            T a = seg.first;
            T b = seg.second;
            if (b <= l) continue;
            if (a >= r) break;
            T L = max(a, l);
            T R = min(b, r);
            if (R > L) {
                Len len = Len(R) - Len(L);
                assert(len >= Len(0));
                s += len;
            }
        }
        return s;
    }

    int interval_count() const { return (int)segs.size(); }

    bool contains(T x) const {
        if (!const_cast<NaiveIntervalSet*>(this)->normalize_and_check(x)) return false;
        for (auto &seg : segs) {
            if (x < seg.first) break;
            if (seg.first <= x && x < seg.second) return true;
        }
        return false;
    }

    const pair<T,T>* find(T x) const {
        if (!const_cast<NaiveIntervalSet*>(this)->normalize_and_check(x)) return nullptr;
        for (auto &seg : segs) {
            if (x < seg.first) break;
            if (seg.first <= x && x < seg.second) return &seg;
        }
        return nullptr;
    }

    const pair<T,T>* nearest_left(T x) const {
        if (!const_cast<NaiveIntervalSet*>(this)->normalize_and_check(x)) return nullptr;
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
        if (!const_cast<NaiveIntervalSet*>(this)->normalize_and_check(x)) return nullptr;
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
};

template <class T, class Len>
static void dump_interval_vec(const vector<pair<T,T>> &v, const char *name) {
    cerr << "  " << name << " (size=" << v.size() << "):";
    cerr << std::setprecision(20);
    for (auto &p : v) cerr << " [" << p.first << "," << p.second << ")";
    cerr << "\n";
}

template <class T, class Len>
static bool interval_vec_equal_approx(const vector<pair<T,T>> &a, const vector<pair<T,T>> &b) {
    if (a.size() != b.size()) return false;
    const T cabs = default_coord_abs_tol<T>();
    const T crel = default_coord_rel_tol<T>();
    for (size_t i = 0; i < a.size(); ++i) {
        if (!approx_equal(a[i].first, b[i].first, cabs, crel)) return false;
        if (!approx_equal(a[i].second, b[i].second, cabs, crel)) return false;
    }
    return true;
}

template <class T, class Len>
static void verify_small(const IntervalTreap<T,Len> &t,
                         const NaiveIntervalSet<T,Len> &n,
                         int MINX, int MAXX) {
    const char *tname = type_name_T<T>();
    const char *lname = type_name_Len<Len>();

    if (t.interval_count() != n.interval_count()) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << "," << lname << ">] interval_count mismatch\n";
        cerr << "  treap=" << t.interval_count() << " naive=" << n.interval_count() << "\n";
    }

    Len ct_all = t.covered_length();
    Len cn_all = n.covered_length();
    Len labs = default_len_abs_tol<Len>();
    Len lrel = default_len_rel_tol<Len>();
    if (!approx_equal(ct_all, cn_all, labs, lrel)) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << "," << lname << ">] covered_length() mismatch\n";
        cerr << std::setprecision(20)
             << "  treap=" << (long double)ct_all << " naive=" << (long double)cn_all << "\n";
    }

    for (int Li = MINX - 2; Li <= MAXX + 2; ++Li) {
        for (int Ri = Li; Ri <= MAXX + 2; ++Ri) {
            T L = (T)Li;
            T R = (T)Ri;
            Len ct = t.covered_length(L, R);
            Len cn = n.covered_length(L, R);
            if (!approx_equal(ct, cn, labs, lrel)) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << "," << lname << ">] covered_length(L,R) mismatch\n";
                cerr << "  L=" << L << " R=" << R << "\n";
                cerr << std::setprecision(20)
                     << "  treap=" << (long double)ct << " naive=" << (long double)cn << "\n";
            }
        }
    }

    const T cabs = default_coord_abs_tol<T>();
    const T crel = default_coord_rel_tol<T>();

    for (int xi = MINX - 2; xi <= MAXX + 2; ++xi) {
        T x = (T)xi;

        bool c1 = t.contains(x);
        bool c2 = n.contains(x);
        if (c1 != c2) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << "," << lname << ">] contains mismatch\n";
            cerr << "  x=" << x << " treap=" << c1 << " naive=" << c2 << "\n";
        }

        auto tn = t.find(x);
        auto nn = n.find(x);
        if ((nn == nullptr) != (tn == nullptr)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << "," << lname << ">] find nullptr mismatch\n";
        } else if (nn && tn) {
            if (!(approx_equal(tn->l, nn->first, cabs, crel) &&
                  approx_equal(tn->r, nn->second, cabs, crel))) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << "," << lname << ">] find interval mismatch\n";
            }
        }

        auto tl = t.nearest_left(x);
        auto nl = n.nearest_left(x);
        if ((nl == nullptr) != (tl == nullptr)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << "," << lname << ">] nearest_left nullptr mismatch\n";
        } else if (nl && tl) {
            if (!(approx_equal(tl->l, nl->first, cabs, crel) &&
                  approx_equal(tl->r, nl->second, cabs, crel))) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << "," << lname << ">] nearest_left interval mismatch\n";
            }
        }

        auto tr = t.nearest_right(x);
        auto nr = n.nearest_right(x);
        if ((nr == nullptr) != (tr == nullptr)) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << "," << lname << ">] nearest_right nullptr mismatch\n";
        } else if (nr && tr) {
            if (!(approx_equal(tr->l, nr->first, cabs, crel) &&
                  approx_equal(tr->r, nr->second, cabs, crel))) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << "," << lname << ">] nearest_right interval mismatch\n";
            }
        }
    }

    vector<pair<T,T>> v1;
    t.iterate([&](const typename IntervalTreap<T,Len>::Node &node){
        v1.emplace_back(node.l, node.r);
    });
    if (!interval_vec_equal_approx<T,Len>(v1, n.segs)) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << "," << lname << ">] iterate(all) mismatch\n";
        dump_interval_vec<T,Len>(v1, "treap");
        dump_interval_vec<T,Len>(n.segs, "naive");
    }

    for (int Li = MINX - 2; Li <= MAXX + 2; ++Li) {
        for (int Ri = Li; Ri <= MAXX + 2; ++Ri) {
            if (Ri == Li) continue;
            T L = (T)Li;
            T R = (T)Ri;

            vector<pair<T,T>> vv1;
            t.iterate(L, R, [&](const typename IntervalTreap<T,Len>::Node &node){
                vv1.emplace_back(node.l, node.r);
            });

            vector<pair<T,T>> vv2;
            for (auto &seg : n.segs) {
                if (seg.second > L && seg.first < R) vv2.push_back(seg);
            }

            if (!interval_vec_equal_approx<T,Len>(vv1, vv2)) {
                ++g_test_failures;
                cerr << "[verify_small<" << tname << "," << lname << ">] iterate(L,R) overlap mismatch\n";
                cerr << "  L=" << L << " R=" << R << "\n";
                dump_interval_vec<T,Len>(vv1, "treap");
                dump_interval_vec<T,Len>(vv2, "naive");
            }
        }
    }

    auto tlm = t.left_most();
    auto nlm = n.left_most();
    if ((nlm == nullptr) != (tlm == nullptr)) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << "," << lname << ">] left_most nullptr mismatch\n";
    } else if (nlm && tlm) {
        if (!(approx_equal(tlm->l, nlm->first, cabs, crel) &&
              approx_equal(tlm->r, nlm->second, cabs, crel))) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << "," << lname << ">] left_most interval mismatch\n";
        }
    }

    auto trm = t.right_most();
    auto nrm = n.right_most();
    if ((nrm == nullptr) != (trm == nullptr)) {
        ++g_test_failures;
        cerr << "[verify_small<" << tname << "," << lname << ">] right_most nullptr mismatch\n";
    } else if (nrm && trm) {
        if (!(approx_equal(trm->l, nrm->first, cabs, crel) &&
              approx_equal(trm->r, nrm->second, cabs, crel))) {
            ++g_test_failures;
            cerr << "[verify_small<" << tname << "," << lname << ">] right_most interval mismatch\n";
        }
    }
}

template <class T, class Len>
static void edge_case_tests_T_floatlike() {
    const char *tname = type_name_T<T>();
    const char *lname = type_name_Len<Len>();

    const T abs_eps = std::is_same_v<T,float> ? (T)1e-5f : (T)1e-12;
    const T rel_eps = T(0);

    {
        IntervalTreap<T,Len> t(0, abs_eps, rel_eps);
        NaiveIntervalSet<T,Len> n;
        n.abs_eps = abs_eps;
        n.rel_eps = rel_eps;

        if (t.interval_count() != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty interval_count != 0\n"; }
        if (!approx_equal(t.covered_length(), Len(0), default_len_abs_tol<Len>(), default_len_rel_tol<Len>())) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty covered_length != 0\n"; }
        if (t.contains(T(0))) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty contains(0) should be false\n"; }
        if (t.find(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty find(0) should be nullptr\n"; }
        if (t.nearest_left(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty nearest_left(0) should be nullptr\n"; }
        if (t.nearest_right(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty nearest_right(0) should be nullptr\n"; }
        if (t.left_most() != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty left_most should be nullptr\n"; }
        if (t.right_most() != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] empty right_most should be nullptr\n"; }

        int cnt = 0;
        t.iterate([&](const typename IntervalTreap<T,Len>::Node &){ ++cnt; });
        if (cnt != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] iterate(all) on empty should yield 0\n"; }
        t.iterate(T(-10), T(10), [&](const typename IntervalTreap<T,Len>::Node &){ ++cnt; });
        if (cnt != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] iterate(l,r) on empty should yield 0\n"; }

        if constexpr (std::is_floating_point_v<T>) {
            T nan = std::numeric_limits<T>::quiet_NaN();
            T inf = std::numeric_limits<T>::infinity();

            t.insert(nan, T(1));
            t.insert(T(0), inf);
            t.erase(nan, T(1));
            t.erase(T(0), inf);

            if (t.contains(nan)) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] contains(NaN) should be false\n"; }
            if (t.find(nan) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] find(NaN) should be nullptr\n"; }
            if (t.nearest_left(nan) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] nearest_left(NaN) should be nullptr\n"; }
            if (t.nearest_right(inf) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] nearest_right(Inf) should be nullptr\n"; }

            Len c = t.covered_length(nan, T(1));
            if (!approx_equal(c, Len(0), default_len_abs_tol<Len>(), default_len_rel_tol<Len>())) {
                ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] covered_length(NaN,1) should be 0\n";
            }

            int c2 = 0;
            t.iterate(nan, T(1), [&](const typename IntervalTreap<T,Len>::Node &){ ++c2; });
            if (c2 != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] iterate(NaN,1) should yield 0\n"; }
        }

        t.insert(T(5), T(5));
        n.insert(T(5), T(5));
        verify_small(t, n, -5, 15);

        t.erase(T(7), T(7));
        n.erase(T(7), T(7));
        verify_small(t, n, -5, 15);

        t.insert(T(0), T(10));
        n.insert(T(0), T(10));
        verify_small(t, n, -5, 15);

        t.insert(T(10), T(20));
        n.insert(T(10), T(20));
        verify_small(t, n, -5, 25);

        t.insert(T(5), T(25));
        n.insert(T(5), T(25));
        verify_small(t, n, -5, 30);

        t.erase(T(5), T(10));
        n.erase(T(5), T(10));
        verify_small(t, n, -5, 30);

        if (!t.contains(T(0))) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] contains(0) should be true\n"; }
        if (t.contains(T(5))) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] contains(5) should be false (half-open)\n"; }

        {
            IntervalTreap<T,Len> tt(0, abs_eps, rel_eps);
            NaiveIntervalSet<T,Len> nn; nn.abs_eps = abs_eps; nn.rel_eps = rel_eps;

            T r1 = (T)0.3;
            T l2 = (T)0.1 + (T)0.2;
            tt.insert(T(0), r1);
            nn.insert(T(0), r1);
            tt.insert(l2, T(1));
            nn.insert(l2, T(1));

            if (tt.interval_count() != 1 || nn.interval_count() != 1) {
                ++g_test_failures;
                cerr << "[edge_case<" << tname << "," << lname << ">] eps-merge expected 1 interval\n";
            }
            verify_small(tt, nn, 0, 2);
        }

        if constexpr (std::is_floating_point_v<T>) {
            IntervalTreap<T,Len> tt(0, abs_eps, rel_eps);
            NaiveIntervalSet<T,Len> nn; nn.abs_eps = abs_eps; nn.rel_eps = rel_eps;

            T mzero = -T(0);
            tt.insert(mzero, T(1));
            nn.insert(mzero, T(1));

            auto p = tt.left_most();
            if (!p) { ++g_test_failures; cerr << "[edge_case<" << tname << "," << lname << ">] -0 insert: left_most nullptr\n"; }
            else {
                if (!(p->l == T(0))) {
                    ++g_test_failures;
                    cerr << "[edge_case<" << tname << "," << lname << ">] -0 should be canonicalized to +0\n";
                }
            }
            verify_small(tt, nn, -1, 2);
        }
    }
}

template <class T, class Len>
static void random_small_tests_T_floatlike() {
    std::mt19937_64 rng(123456789);
    const int MINX = -10;
    const int MAXX =  10;

    std::uniform_int_distribution<int> dist(MINX - 5, MAXX + 5);
    const int CASES = 200;
    const int OPS = 200;

    const T abs_eps = std::is_same_v<T,float> ? (T)1e-5f : (T)1e-12;
    const T rel_eps = T(0);

    for (int tc = 0; tc < CASES; ++tc) {
        IntervalTreap<T,Len> t(0, abs_eps, rel_eps);
        NaiveIntervalSet<T,Len> n;
        n.abs_eps = abs_eps;
        n.rel_eps = rel_eps;

        for (int op = 0; op < OPS; ++op) {
            T a = (T)dist(rng);
            T b = (T)dist(rng);
            T l = std::min(a, b);
            T r = std::max(a, b);

            if ((rng() & 7) == 0) r = l;
            else if (l == r) r = l + T(1);

            if (rng() & 1) { t.insert(l, r); n.insert(l, r); }
            else { t.erase(l, r); n.erase(l, r); }

            verify_small(t, n, MINX, MAXX);
        }
    }
}

template <class T, class Len>
static void random_large_tests_T_floatlike() {
    const char *tname = type_name_T<T>();
    const char *lname = type_name_Len<Len>();

    std::mt19937_64 rng(987654321);
    const int MINX = -1000000;
    const int MAXX =  1000000;

    std::uniform_int_distribution<int> dist(MINX, MAXX);
    const int CASES = 50;
    const int OPS = 300;

    const T abs_eps = std::is_same_v<T,float> ? (T)1e-5f : (T)1e-12;
    const T rel_eps = T(0);

    const Len labs = default_len_abs_tol<Len>();
    const Len lrel = default_len_rel_tol<Len>();
    const T  cabs = default_coord_abs_tol<T>();
    const T  crel = default_coord_rel_tol<T>();

    for (int tc = 0; tc < CASES; ++tc) {
        IntervalTreap<T,Len> t(0, abs_eps, rel_eps);
        NaiveIntervalSet<T,Len> n;
        n.abs_eps = abs_eps;
        n.rel_eps = rel_eps;

        for (int op = 0; op < OPS; ++op) {
            T a = (T)dist(rng);
            T b = (T)dist(rng);
            T l = std::min(a, b);
            T r = std::max(a, b);

            if ((rng() & 7) == 0) r = l;
            else if (l == r) r = l + T(1);

            if (rng() & 1) { t.insert(l, r); n.insert(l, r); }
            else { t.erase(l, r); n.erase(l, r); }

            for (int i = 0; i < 10; ++i) {
                T x = (T)dist(rng);

                bool c1 = t.contains(x);
                bool c2 = n.contains(x);
                if (c1 != c2) {
                    ++g_test_failures;
                    cerr << "[random_large<" << tname << "," << lname << ">] contains mismatch\n";
                }

                auto tn = t.find(x);
                auto nn = n.find(x);
                if ((nn == nullptr) != (tn == nullptr)) {
                    ++g_test_failures;
                    cerr << "[random_large<" << tname << "," << lname << ">] find nullptr mismatch\n";
                } else if (nn && tn) {
                    if (!(approx_equal(tn->l, nn->first, cabs, crel) &&
                          approx_equal(tn->r, nn->second, cabs, crel))) {
                        ++g_test_failures;
                        cerr << "[random_large<" << tname << "," << lname << ">] find interval mismatch\n";
                    }
                }

                auto tl = t.nearest_left(x);
                auto nl = n.nearest_left(x);
                if ((nl == nullptr) != (tl == nullptr)) {
                    ++g_test_failures;
                    cerr << "[random_large<" << tname << "," << lname << ">] nearest_left nullptr mismatch\n";
                } else if (nl && tl) {
                    if (!(approx_equal(tl->l, nl->first, cabs, crel) &&
                          approx_equal(tl->r, nl->second, cabs, crel))) {
                        ++g_test_failures;
                        cerr << "[random_large<" << tname << "," << lname << ">] nearest_left interval mismatch\n";
                    }
                }

                auto tr = t.nearest_right(x);
                auto nr = n.nearest_right(x);
                if ((nr == nullptr) != (tr == nullptr)) {
                    ++g_test_failures;
                    cerr << "[random_large<" << tname << "," << lname << ">] nearest_right nullptr mismatch\n";
                } else if (nr && tr) {
                    if (!(approx_equal(tr->l, nr->first, cabs, crel) &&
                          approx_equal(tr->r, nr->second, cabs, crel))) {
                        ++g_test_failures;
                        cerr << "[random_large<" << tname << "," << lname << ">] nearest_right interval mismatch\n";
                    }
                }
            }

            for (int i = 0; i < 5; ++i) {
                T x1 = (T)dist(rng);
                T x2 = (T)dist(rng);
                T L = std::min(x1, x2);
                T R = std::max(x1, x2);
                if (L == R) R = L + T(1);

                Len ct = t.covered_length(L, R);
                Len cn = n.covered_length(L, R);
                if (!approx_equal(ct, cn, labs, lrel)) {
                    ++g_test_failures;
                    cerr << "[random_large<" << tname << "," << lname << ">] covered_length mismatch\n";
                }
            }

            for (int i = 0; i < 3; ++i) {
                T x1 = (T)dist(rng);
                T x2 = (T)dist(rng);
                T L = std::min(x1, x2);
                T R = std::max(x1, x2);
                if (L == R) R = L + T(1);

                vector<pair<T,T>> vv1;
                t.iterate(L, R, [&](const typename IntervalTreap<T,Len>::Node &node){
                    vv1.emplace_back(node.l, node.r);
                });

                vector<pair<T,T>> vv2;
                for (auto &seg : n.segs) if (seg.second > L && seg.first < R) vv2.push_back(seg);

                if (!interval_vec_equal_approx<T,Len>(vv1, vv2)) {
                    ++g_test_failures;
                    cerr << "[random_large<" << tname << "," << lname << ">] iterate overlap mismatch\n";
                    cerr << "  L=" << L << " R=" << R << "\n";
                    dump_interval_vec<T,Len>(vv1, "treap");
                    dump_interval_vec<T,Len>(vv2, "naive");
                }
            }
        }
    }
}

template <class T, class Len>
static void benchmark_case_T(size_t N, std::mt19937_64 &rng) {
    using namespace std::chrono;
    cout << "Benchmark N=" << N << " intervals (T=" << type_name_T<T>()
         << ", Len=" << type_name_Len<Len>() << ")\n";

    vector<pair<T,T>> intervals;
    intervals.reserve(N);

    const T STEP = (T)8;
    const int MAX_LEN_INT = 4;

    for (size_t i = 0; i < N; ++i) {
        T l = (T)i * STEP;
        int len_i = 1 + int(rng() % MAX_LEN_INT);
        T r = l + (T)len_i;
        intervals.emplace_back(l, r);
    }

    const long long MAX_COORD_LL = (long long)((T)N * STEP + (T)(MAX_LEN_INT + 10));

    int repeat = (N <= 1000) ? 5 : 3;

    {
        long long total_ns = 0;
        for (int rep = 0; rep < repeat; ++rep) {
            IntervalTreap<T,Len> t;
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
            IntervalTreap<T,Len> t;
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
        IntervalTreap<T,Len> t;
        t.reserve(N);
        for (auto &seg : intervals) t.insert(seg.first, seg.second);

        const size_t Q_base = (N <= 1000) ? 200000 : 100000;
        std::uniform_int_distribution<long long> distCoord(0, MAX_COORD_LL);

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) sink = sink + (long double)t.covered_length();
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  covered_length(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T a = (T)distCoord(rng);
                T b = (T)distCoord(rng);
                T l = std::min(a, b);
                T r = std::max(a, b);
                if (l == r) r = l + (T)1;
                sink = sink + (long double)t.covered_length(l, r);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  covered_length(l,r): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) sink = sink + (long double)t.interval_count();
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  interval_count(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                sink = sink + (long double)t.contains(x);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  contains(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                auto p = t.find(x);
                if (p) sink = sink + (long double)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  find(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                auto p = t.nearest_left(x);
                if (p) sink = sink + (long double)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  nearest_left(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T x = (T)distCoord(rng);
                auto p = t.nearest_right(x);
                if (p) sink = sink + (long double)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  nearest_right(x): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                auto p = t.left_most();
                if (p) sink = sink + (long double)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  left_most(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = Q_base;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                auto p = t.right_most();
                if (p) sink = sink + (long double)(p->r - p->l);
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  right_most(): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = (N <= 1000) ? 200 : 5;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                t.iterate([&](const typename IntervalTreap<T,Len>::Node &node){
                    sink = sink + (long double)(node.r - node.l);
                });
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  iterate(all): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }

        {
            const size_t Q = (N <= 1000) ? 200 : 5;
            volatile long double sink = 0;
            auto t0 = high_resolution_clock::now();
            for (size_t i = 0; i < Q; ++i) {
                T a = (T)distCoord(rng);
                T b = (T)distCoord(rng);
                T l = std::min(a, b);
                T r = std::max(a, b);
                if (l == r) r = l + (T)1;
                t.iterate(l, r, [&](const typename IntervalTreap<T,Len>::Node &node){
                    sink = sink + (long double)(node.r - node.l);
                });
            }
            auto t1 = high_resolution_clock::now();
            const double avg_ns = (double)duration_cast<nanoseconds>(t1 - t0).count() / (double)Q;
            cout << "  iterate(l,r): calls=" << Q << ", avg_ns=" << fixed << setprecision(1) << avg_ns << "\n";
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "Running edge-case tests (float, Len=double)...\n";
    edge_case_tests_T_floatlike<float, double>();
    cout << "Edge-case tests (float) done.\n";

    cout << "Running random small tests (float, Len=double)...\n";
    random_small_tests_T_floatlike<float, double>();
    cout << "Random small tests (float) done.\n";

    cout << "Running random large tests (float, Len=double)...\n";
    random_large_tests_T_floatlike<float, double>();
    cout << "Random large tests (float) done.\n";

    cout << "Running edge-case tests (double, Len=long double)...\n";
    edge_case_tests_T_floatlike<double, long double>();
    cout << "Edge-case tests (double) done.\n";

    cout << "Running random small tests (double, Len=long double)...\n";
    random_small_tests_T_floatlike<double, long double>();
    cout << "Random small tests (double) done.\n";

    cout << "Running random large tests (double, Len=long double)...\n";
    random_large_tests_T_floatlike<double, long double>();
    cout << "Random large tests (double) done.\n";

    if (g_test_failures > 0) {
        cerr << "TOTAL TEST FAILURES: " << g_test_failures << "\n";
    } else {
        cout << "All tests passed.\n";
    }

    std::mt19937_64 rng(20251211);
    cout << "Running benchmarks...\n";
    benchmark_case_T<float, double>(1000, rng);
    benchmark_case_T<float, double>(1000000, rng);
    benchmark_case_T<double, long double>(1000, rng);
    benchmark_case_T<double, long double>(1000000, rng);

    cout << "All tests and benchmarks finished.\n";
    return 0;
}

#endif


// Running edge-case tests (float, Len=double)...
// Edge-case tests (float) done.
// Running random small tests (float, Len=double)...
// Random small tests (float) done.
// Running random large tests (float, Len=double)...
// Random large tests (float) done.
// Running edge-case tests (double, Len=long double)...
// Edge-case tests (double) done.
// Running random small tests (double, Len=long double)...
// Random small tests (double) done.
// Running random large tests (double, Len=long double)...
// Random large tests (double) done.
// All tests passed.
// Running benchmarks...
// Benchmark N=1000 intervals (T=float, Len=double)
//   Insert-all: iterations=5, ops=1000, avg_ms=0.074
//   Insert+Erase: iterations=5, ops=2000, avg_ms=0.625
//   covered_length(): calls=200000, avg_ns=3.0
//   covered_length(l,r): calls=200000, avg_ns=240.7
//   interval_count(): calls=200000, avg_ns=3.0
//   contains(x): calls=200000, avg_ns=75.2
//   find(x): calls=200000, avg_ns=72.7
//   nearest_left(x): calls=200000, avg_ns=73.3
//   nearest_right(x): calls=200000, avg_ns=73.3
//   left_most(): calls=200000, avg_ns=5.7
//   right_most(): calls=200000, avg_ns=5.1
//   iterate(all): calls=200, avg_ns=3133.4
//   iterate(l,r): calls=200, avg_ns=1364.6
// Benchmark N=1000000 intervals (T=float, Len=double)
//   Insert-all: iterations=3, ops=1000000, avg_ms=187.191
//   Insert+Erase: iterations=3, ops=2000000, avg_ms=1092.108
//   covered_length(): calls=100000, avg_ns=3.2
//   covered_length(l,r): calls=100000, avg_ns=1616.3
//   interval_count(): calls=100000, avg_ns=3.0
//   contains(x): calls=100000, avg_ns=618.9
//   find(x): calls=100000, avg_ns=610.6
//   nearest_left(x): calls=100000, avg_ns=653.3
//   nearest_right(x): calls=100000, avg_ns=673.3
//   left_most(): calls=100000, avg_ns=12.1
//   right_most(): calls=100000, avg_ns=9.4
//   iterate(all): calls=5, avg_ns=13693914.4
//   iterate(l,r): calls=5, avg_ns=4289605.8
// Benchmark N=1000 intervals (T=double, Len=long double)
//   Insert-all: iterations=5, ops=1000, avg_ms=0.110
//   Insert+Erase: iterations=5, ops=2000, avg_ms=0.697
//   covered_length(): calls=200000, avg_ns=3.0
//   covered_length(l,r): calls=200000, avg_ns=245.5
//   interval_count(): calls=200000, avg_ns=3.0
//   contains(x): calls=200000, avg_ns=73.1
//   find(x): calls=200000, avg_ns=72.9
//   nearest_left(x): calls=200000, avg_ns=74.3
//   nearest_right(x): calls=200000, avg_ns=77.1
//   left_most(): calls=200000, avg_ns=5.4
//   right_most(): calls=200000, avg_ns=4.8
//   iterate(all): calls=200, avg_ns=3276.9
//   iterate(l,r): calls=200, avg_ns=1536.1
// Benchmark N=1000000 intervals (T=double, Len=long double)
//   Insert-all: iterations=3, ops=1000000, avg_ms=323.329
//   Insert+Erase: iterations=3, ops=2000000, avg_ms=1512.272
//   covered_length(): calls=100000, avg_ns=3.5
//   covered_length(l,r): calls=100000, avg_ns=2270.6
//   interval_count(): calls=100000, avg_ns=3.4
//   contains(x): calls=100000, avg_ns=1043.8
//   find(x): calls=100000, avg_ns=1058.4
//   nearest_left(x): calls=100000, avg_ns=1035.4
//   nearest_right(x): calls=100000, avg_ns=1058.6
//   left_most(): calls=100000, avg_ns=11.1
//   right_most(): calls=100000, avg_ns=8.5
//   iterate(all): calls=5, avg_ns=21973776.6
//   iterate(l,r): calls=5, avg_ns=9795889.4
// All tests and benchmarks finished.
