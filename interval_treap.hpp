#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <type_traits>
#include <cassert>

using namespace std;

// 区間 [l, r) の集合を管理する乱数化 Treap。区間は常に「互いに非交差・非隣接」を保つ（隣接は自動マージ）。
// 注意: 被覆長は T で保持するため、合計が T の最大値を超える可能性がある（この場合の動作は未定義）。負値化は assert で検出。
template <class T>
class IntervalTreap {
    static_assert(std::is_integral_v<T> && std::is_signed_v<T>,
                  "IntervalTreap<T>: T must be a signed integral type");

public:
    // Treap ノード（区間と部分木集計を保持）。外部へ返す参照は const Node* で、木が変更されると無効になりうる。
    struct Node {
        T l, r;        // 管理する区間の左端・右端（半開区間）。
        int ch[2];     // 子ノードのインデックス（0:左, 1:右, -1:なし）。
        unsigned pri;  // Treap の優先度。
        int size;      // 部分木に含まれるノード数。
        T min_l;       // 部分木内の最小 l。
        T max_r;       // 部分木内の最大 r。
        T cov;         // 部分木内の被覆長合計（T で保持）。
    };

private:
    vector<Node> nodes;
    vector<int> free_list;
    int root;
    uint32_t rng_state;

    static constexpr int null = -1;

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
        T len = r - l;
        assert(len >= T(0));
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
        T len = x.r - x.l;
        assert(len >= T(0));
        x.cov = len;
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

    T range_covered_length(int cur, T l, T r) const {
        if (cur == null) return T(0);
        T res = T(0);
        static thread_local vector<int> st;
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
                assert(res >= T(0));
                continue;
            }
            if (x.ch[0] != null) st.push_back(x.ch[0]);
            if (x.ch[1] != null) st.push_back(x.ch[1]);
            if (x.r > l && x.l < r) {
                T a = x.l < l ? l : x.l;
                T b = x.r > r ? r : x.r;
                if (b > a) {
                    T len = b - a;
                    assert(len >= T(0));
                    res += len;
                    assert(res >= T(0));
                }
            }
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
    IntervalTreap(size_t reserve_nodes = 0)
        : nodes(), free_list(), root(null), rng_state(0x12345678u) {
        if (reserve_nodes) nodes.reserve(reserve_nodes);
    }

    // 全データを破棄して初期状態に戻す。O(n)
    void clear() {
        nodes.clear();
        free_list.clear();
        root = null;
        rng_state = 0x12345678u;
    }

    // 内部配列の確保量を増やして再確保回数を減らす。O(1) 平均
    void reserve(size_t n) {
        if (n > nodes.size()) nodes.reserve(n);
    }

    // 全区間の被覆長合計を返す。O(1)
    T covered_length() const {
        if (root == null) return T(0);
        return nodes[root].cov;
    }

    // 指定範囲の被覆長合計を返す。平均 O(log n + k) / 最悪 O(n)
    // k は範囲と重なる区間数（あるいは訪問ノード数）の目安。
    T covered_length(T l, T r) const {
        if (root == null || l >= r) return T(0);
        return range_covered_length(root, l, r);
    }

    // 管理している区間数を返す。O(1)
    int interval_count() const {
        return root == null ? 0 : nodes[root].size;
    }

    // 区間を追加し、必要なら重なり・隣接とマージする。O((m+1) log n)
    // m はマージにより削除される既存区間数。
    void insert(T l, T r) {
        if (l >= r) return;
        if (root == null) {
            root = new_node(l, r);
            return;
        }
        int idx = find_le_by_key(root, l);
        if (idx != null && nodes[idx].r >= l) {
            Node old = nodes[idx];
            root = erase_by_key(root, old.l);
            if (old.l < l) l = old.l;
            if (old.r > r) r = old.r;
        }
        while (true) {
            idx = find_first_ge_by_key(root, l);
            if (idx == null) break;
            Node old = nodes[idx];
            if (old.l > r) break;
            root = erase_by_key(root, old.l);
            if (old.l < l) l = old.l;
            if (old.r > r) r = old.r;
        }
        int node = new_node(l, r);
        root = insert_node(root, node);
    }

    // 区間を削除し、必要なら区間を分割する。O((m+1) log n)
    // m は削除・分割に関与する既存区間数（重なり区間数）。
    void erase(T l, T r) {
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

    // 指定点が被覆されているか判定する。O(log n)
    bool contains(T x) const {
        return find(x) != nullptr;
    }

    // 指定点を含む区間を返す（なければ nullptr）。O(log n)
    const Node* find(T x) const {
        return find_impl(root, x);
    }

    // 指定点を含む区間、なければ左側で最も近い区間を返す（なければ nullptr）。O(log n)
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

    // 指定点を含む区間、なければ右側で最も近い区間を返す（なければ nullptr）。O(log n)
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

    // 最左の区間を返す（なければ nullptr）。O(log n)
    const Node* left_most() const {
        int idx = left_most_idx(root);
        return idx == null ? nullptr : &nodes[idx];
    }

    // 最右の区間を返す（なければ nullptr）。O(log n)
    const Node* right_most() const {
        int idx = right_most_idx(root);
        return idx == null ? nullptr : &nodes[idx];
    }

    // 全区間を昇順に列挙する。O(n)
    template <class F>
    void iterate(const F &f) const {
        inorder_all(root, f);
    }

    // 指定範囲と重なる区間を昇順に列挙する。平均 O(log n + k) / 最悪 O(n)
    // k は列挙される区間数。
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

static int g_test_failures = 0;

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
};

template <class T>
static void dump_interval_vec(const vector<pair<T,T>> &v, const char *name) {
    cerr << "  " << name << " (size=" << v.size() << "):";
    for (auto &p : v) cerr << " [" << p.first << "," << p.second << ")";
    cerr << "\n";
}

template <class T>
static void verify_small(const IntervalTreap<T> &t, const NaiveIntervalSet<T> &n, T MINX, T MAXX) {
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
    t.iterate([&](const typename IntervalTreap<T>::Node &node){
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
            t.iterate(L, R, [&](const typename IntervalTreap<T>::Node &node){
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
        IntervalTreap<T> t;
        NaiveIntervalSet<T> n;

        if (t.interval_count() != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty interval_count != 0\n"; }
        if (t.covered_length() != T(0)) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty covered_length != 0\n"; }
        if (t.contains(T(0))) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty contains(0) should be false\n"; }
        if (t.find(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty find(0) should be nullptr\n"; }
        if (t.nearest_left(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty nearest_left(0) should be nullptr\n"; }
        if (t.nearest_right(T(0)) != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty nearest_right(0) should be nullptr\n"; }
        if (t.left_most() != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty left_most should be nullptr\n"; }
        if (t.right_most() != nullptr) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] empty right_most should be nullptr\n"; }

        int cnt = 0;
        t.iterate([&](const typename IntervalTreap<T>::Node &){ ++cnt; });
        if (cnt != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate(all) on empty should yield 0\n"; }
        t.iterate(T(-10), T(10), [&](const typename IntervalTreap<T>::Node &){ ++cnt; });
        if (cnt != 0) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate(l,r) on empty should yield 0\n"; }

        t.insert(T(0), T(10));
        n.insert(T(0), T(10));
        verify_small(t, n, T(-5), T(15));

        t.insert(T(10), T(20));
        n.insert(T(10), T(20));
        verify_small(t, n, T(-5), T(25));

        t.insert(T(5), T(25));
        n.insert(T(5), T(25));
        verify_small(t, n, T(-5), T(30));

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
        IntervalTreap<T> t;
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
        IntervalTreap<T> t;
        NaiveIntervalSet<T> n;
        auto add = [&](T l, T r) { t.insert(l, r); n.insert(l, r); };

        add(T(0),  T(10));
        add(T(20), T(30));
        add(T(40), T(50));

        {
            vector<pair<T,T>> v1;
            t.iterate(T(5), T(25), [&](const typename IntervalTreap<T>::Node &node){
                v1.emplace_back(node.l, node.r);
            });
            vector<pair<T,T>> v2;
            for (auto &seg : n.segs) if (seg.second > T(5) && seg.first < T(25)) v2.push_back(seg);
            if (v1 != v2) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate overlap mismatch [5,25)\n"; }
        }

        {
            vector<pair<T,T>> v1;
            t.iterate(T(10), T(20), [&](const typename IntervalTreap<T>::Node &node){
                v1.emplace_back(node.l, node.r);
            });
            if (!v1.empty()) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate overlap mismatch [10,20)\n"; }
        }

        {
            vector<pair<T,T>> v1;
            t.iterate(T(-5), T(100), [&](const typename IntervalTreap<T>::Node &node){
                v1.emplace_back(node.l, node.r);
            });
            vector<pair<T,T>> v2;
            for (auto &seg : n.segs) if (seg.second > T(-5) && seg.first < T(100)) v2.push_back(seg);
            if (v1 != v2) { ++g_test_failures; cerr << "[edge_case<" << tname << ">] iterate overlap mismatch [-5,100)\n"; }
        }
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
        IntervalTreap<T> t;
        NaiveIntervalSet<T> n;
        for (int op = 0; op < OPS; ++op) {
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
        IntervalTreap<T> t;
        NaiveIntervalSet<T> n;
        for (int op = 0; op < OPS; ++op) {
            T a = (T)dist(rng);
            T b = (T)dist(rng);
            T l = std::min(a, b);
            T r = std::max(a, b);
            if ((rng() & 7) == 0) r = l;
            else if (l == r) ++r;

            if (rng() & 1) { t.insert(l, r); n.insert(l, r); }
            else { t.erase(l, r); n.erase(l, r); }

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

            for (int i = 0; i < 3; ++i) {
                T x1 = (T)dist(rng);
                T x2 = (T)dist(rng);
                T L = std::min(x1, x2);
                T R = std::max(x1, x2);
                if (L == R) ++R;

                vector<pair<T,T>> vv1;
                t.iterate(L, R, [&](const typename IntervalTreap<T>::Node &node){
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
            IntervalTreap<T> t;
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
            IntervalTreap<T> t;
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
        IntervalTreap<T> t;
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
                t.iterate([&](const typename IntervalTreap<T>::Node &node){
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
                t.iterate(l, r, [&](const typename IntervalTreap<T>::Node &node){
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
//   Insert-all: iterations=5, ops=1000, avg_ms=0.063
//   Insert+Erase: iterations=5, ops=2000, avg_ms=0.605
//   covered_length(): calls=200000, avg_ns=0.3
//   covered_length(l,r): calls=200000, avg_ns=226.5
//   interval_count(): calls=200000, avg_ns=1.6
//   contains(x): calls=200000, avg_ns=70.7
//   find(x): calls=200000, avg_ns=71.0
//   nearest_left(x): calls=200000, avg_ns=70.2
//   nearest_right(x): calls=200000, avg_ns=69.7
//   left_most(): calls=200000, avg_ns=5.4
//   right_most(): calls=200000, avg_ns=3.5
//   iterate(all): calls=200, avg_ns=1621.4
//   iterate(l,r): calls=200, avg_ns=979.3
// Benchmark N=1000000 intervals (T=int)
//   Insert-all: iterations=3, ops=1000000, avg_ms=158.230
//   Insert+Erase: iterations=3, ops=2000000, avg_ms=1025.235
//   covered_length(): calls=100000, avg_ns=0.3
//   covered_length(l,r): calls=100000, avg_ns=1382.3
//   interval_count(): calls=100000, avg_ns=1.7
//   contains(x): calls=100000, avg_ns=544.5
//   find(x): calls=100000, avg_ns=559.9
//   nearest_left(x): calls=100000, avg_ns=562.7
//   nearest_right(x): calls=100000, avg_ns=561.0
//   left_most(): calls=100000, avg_ns=11.4
//   right_most(): calls=100000, avg_ns=8.9
//   iterate(all): calls=5, avg_ns=10872654.2
//   iterate(l,r): calls=5, avg_ns=3602802.6
// Benchmark N=1000 intervals (T=long long)
//   Insert-all: iterations=5, ops=1000, avg_ms=0.063
//   Insert+Erase: iterations=5, ops=2000, avg_ms=0.597
//   covered_length(): calls=200000, avg_ns=0.3
//   covered_length(l,r): calls=200000, avg_ns=224.1
//   interval_count(): calls=200000, avg_ns=0.3
//   contains(x): calls=200000, avg_ns=73.5
//   find(x): calls=200000, avg_ns=72.5
//   nearest_left(x): calls=200000, avg_ns=73.3
//   nearest_right(x): calls=200000, avg_ns=71.8
//   left_most(): calls=200000, avg_ns=4.9
//   right_most(): calls=200000, avg_ns=4.7
//   iterate(all): calls=200, avg_ns=2111.9
//   iterate(l,r): calls=200, avg_ns=985.8
// Benchmark N=1000000 intervals (T=long long)
//   Insert-all: iterations=3, ops=1000000, avg_ms=145.771
//   Insert+Erase: iterations=3, ops=2000000, avg_ms=960.724
//   covered_length(): calls=100000, avg_ns=0.3
//   covered_length(l,r): calls=100000, avg_ns=1544.0
//   interval_count(): calls=100000, avg_ns=0.3
//   contains(x): calls=100000, avg_ns=594.0
//   find(x): calls=100000, avg_ns=601.9
//   nearest_left(x): calls=100000, avg_ns=643.5
//   nearest_right(x): calls=100000, avg_ns=713.2
//   left_most(): calls=100000, avg_ns=10.9
//   right_most(): calls=100000, avg_ns=8.1
//   iterate(all): calls=5, avg_ns=12813452.6
//   iterate(l,r): calls=5, avg_ns=6011365.4
// All tests and benchmarks finished.
