/*
 * suffix_array_solvers_v03.hpp
 *
 * ACL の suffix_array / lcp_array を利用した、競技プログラミング向けの典型ソルバー集。
 * namespace は使わず、各ソルバーをコメント区切りのブロック単位にまとめている。
 * それぞれのブロックは、必要な result 型・小さな補助処理・実装を近くに置き、
 * コンテスト中に必要な部分だけをコピーしやすい構成にしている。
 *
 * 前提:
 * - C++20 / gcc12.2
 * - AtCoder Library の <atcoder/string> が利用可能
 * - 文字列版の単体ソルバーは、ACL の string 版 suffix_array と同じく通常の char 文字列を対象にする
 * - 複数文字列や整数列を連結するソルバーでは、separator 衝突を避けるため vector<int> に変換する
 */
#pragma once
#include <bits/stdc++.h>
#include <atcoder/string>

// ------------------------------------------------------------
// 1. 異なる部分文字列数
// ------------------------------------------------------------

// 文字列 s の異なる部分文字列数を返す、O(n)
inline long long sa_distinct_substring_count(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return 0;

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    long long ans = 1LL * n * (n + 1) / 2;
    for (int x : lcp) ans -= x;
    return ans;
}

// ------------------------------------------------------------
// 2. K 番目の異なる部分文字列
// ------------------------------------------------------------

// 辞書順で k 番目の異なる部分文字列を返す、k は 1-indexed、O(n)
inline std::string sa_kth_distinct_substring(const std::string& s, long long k) {
    int n = int(s.size());
    assert(k >= 1);
    assert(n >= 1);

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    // suffix array 順に、各 suffix が新しく生む prefix を長さ昇順に数える。
    for (int i = 0; i < n; i++) {
        int same_as_previous = (i == 0 ? 0 : lcp[i - 1]);
        long long add = n - sa[i] - same_as_previous;
        if (k > add) {
            k -= add;
            continue;
        }
        int len = same_as_previous + int(k);
        return s.substr(sa[i], len);
    }

    assert(false);
    return std::string();
}

// ------------------------------------------------------------
// 3. パターン検索
// ------------------------------------------------------------

/*
 * SaPatternSearch
 *
 * 使い方:
 *   SaPatternSearch search(s);
 *   bool ok = search.contains(p);
 *   int c = search.count(p);
 *   auto [l, r] = search.range(p);
 *   vector<int> pos = search.positions(p);
 *
 * 用途:
 *   固定文字列 s に対して、パターン p の出現判定・出現回数・出現位置列挙を行う。
 *   range(p) は p を prefix に持つ suffix の suffix array 上の区間 [l, r) を返すので、
 *   位置制約つき検索をしたい場合は、この区間に対して別途 Segment Tree などを載せる。
 *
 * 注意:
 *   positions(p) は出現位置を昇順にするため sort する。
 *   空パターンは全 suffix に一致する扱いで、count("") は s.size() になる。
 */
struct SaPatternSearch {
    std::string s;
    std::vector<int> sa;

    // 文字列 s の suffix array を構築する、O(n)
    SaPatternSearch(const std::string& s_) : s(s_), sa(atcoder::suffix_array(s)) {}

    // suffix s[pos..] と pattern p を比較し、suffix<p なら -1、p が prefix なら 0、suffix>p なら 1 を返す、O(|p|)
    int compare_suffix_with_pattern(int pos, const std::string& p) const {
        int n = int(s.size());
        int m = int(p.size());

        for (int i = 0; i < m; i++) {
            if (pos + i == n) return -1;
            unsigned char a = static_cast<unsigned char>(s[pos + i]);
            unsigned char b = static_cast<unsigned char>(p[i]);
            if (a < b) return -1;
            if (a > b) return 1;
        }
        return 0;
    }

    // pattern p を prefix に持つ suffix の suffix array 上の区間 [l, r) を返す、O(|p| log n)
    std::pair<int, int> range(const std::string& p) const {
        int n = int(sa.size());

        // lower: suffix >= p となる最初の位置。
        int low = 0, high = n;
        while (low < high) {
            int mid = (low + high) / 2;
            if (compare_suffix_with_pattern(sa[mid], p) < 0) low = mid + 1;
            else high = mid;
        }
        int left = low;

        // upper: p を prefix に持つ suffix 群を越え、suffix > p-prefix-range となる最初の位置。
        low = 0, high = n;
        while (low < high) {
            int mid = (low + high) / 2;
            if (compare_suffix_with_pattern(sa[mid], p) <= 0) low = mid + 1;
            else high = mid;
        }
        int right = low;

        return {left, right};
    }

    // pattern p が s に出現するかを返す、O(|p| log n)
    bool contains(const std::string& p) const {
        auto [l, r] = range(p);
        return l < r;
    }

    // pattern p の出現回数を返す、O(|p| log n)
    int count(const std::string& p) const {
        auto [l, r] = range(p);
        return r - l;
    }

    // pattern p の出現開始位置を昇順で返す、O(|p| log n + occ log occ)
    std::vector<int> positions(const std::string& p) const {
        auto [l, r] = range(p);
        std::vector<int> res;
        res.reserve(r - l);
        for (int i = l; i < r; i++) res.push_back(sa[i]);
        std::sort(res.begin(), res.end());
        return res;
    }
};

// ------------------------------------------------------------
// 4. substring LCP / 比較
// ------------------------------------------------------------

/*
 * SaSubstringLcpCompare
 *
 * 使い方:
 *   SaSubstringLcpCompare cmp(s);
 *   int x = cmp.suffix_lcp(i, j);
 *   int y = cmp.substring_lcp(l1, r1, l2, r2);
 *   int ord = cmp.compare_substring(l1, r1, l2, r2);
 *   bool same = cmp.equal_substring(l1, r1, l2, r2);
 *
 * 用途:
 *   固定文字列 s 上で、任意の suffix / substring の LCP・辞書順比較・等価判定を高速に行う。
 *   substring の配列を辞書順ソートしたい場合は、compare_substring を比較関数内で使う。
 *   Rolling Hash と違い、衝突のない比較として使える。
 *
 * 注意:
 *   空文字列 s に対して構築はできるが、suffix_lcp や suffix_rank は有効な suffix がないため呼べない。
 *   substring_lcp / compare_substring は半開区間 [l, r) を渡す。
 */
struct SaSubstringLcpCompare {
    /*
     * SparseTable
     *
     * SaSubstringLcpCompare の内部実装用の区間最小 Sparse Table。
     * ユーザーが直接使う必要は基本的になく、suffix_lcp から間接的に利用される。
     */
    struct SparseTable {
        std::vector<int> lg;
        std::vector<std::vector<int>> st;

        // 空の Sparse Table を作る、O(1)
        SparseTable() = default;

        // 区間最小用 Sparse Table を構築する、O(n log n)
        SparseTable(const std::vector<int>& v) { build(v); }

        // 区間最小用 Sparse Table を構築し直す、O(n log n)
        void build(const std::vector<int>& v) {
            int n = int(v.size());
            lg.assign(n + 1, 0);
            for (int i = 2; i <= n; i++) lg[i] = lg[i >> 1] + 1;

            if (n == 0) {
                st.clear();
                return;
            }

            int h = lg[n] + 1;
            st.assign(h, std::vector<int>(n));
            st[0] = v;
            for (int k = 1; k < h; k++) {
                int len = 1 << k;
                int half = len >> 1;
                for (int i = 0; i + len <= n; i++) {
                    st[k][i] = std::min(st[k - 1][i], st[k - 1][i + half]);
                }
            }
        }

        // 半開区間 [l, r) の最小値を返す、l<r が必要、O(1)
        int query(int l, int r) const {
            assert(l < r);
            int k = lg[r - l];
            return std::min(st[k][l], st[k][r - (1 << k)]);
        }
    };

    std::string s;
    std::vector<int> sa;
    std::vector<int> rank;
    std::vector<int> lcp;
    SparseTable st;

    // 文字列 s の suffix array、rank、LCP RMQ を構築する、O(n log n)
    SaSubstringLcpCompare(const std::string& s_) : s(s_), sa(atcoder::suffix_array(s)) {
        int n = int(s.size());
        rank.assign(n, 0);
        for (int i = 0; i < n; i++) rank[sa[i]] = i;
        if (n >= 1) lcp = atcoder::lcp_array(s, sa);
        st.build(lcp);
    }

    // suffix s[i..] と s[j..] の LCP 長を返す、O(1)
    int suffix_lcp(int i, int j) const {
        int n = int(s.size());
        assert(0 <= i && i < n);
        assert(0 <= j && j < n);
        if (i == j) return n - i;

        int ri = rank[i];
        int rj = rank[j];
        if (ri > rj) std::swap(ri, rj);
        return st.query(ri, rj);
    }

    // 部分文字列 s[l1..r1) と s[l2..r2) の LCP 長を返す、O(1)
    int substring_lcp(int l1, int r1, int l2, int r2) const {
        int n = int(s.size());
        assert(0 <= l1 && l1 <= r1 && r1 <= n);
        assert(0 <= l2 && l2 <= r2 && r2 <= n);

        int len1 = r1 - l1;
        int len2 = r2 - l2;
        if (len1 == 0 || len2 == 0) return 0;

        int x = suffix_lcp(l1, l2);
        return std::min({x, len1, len2});
    }

    // 部分文字列 s[l1..r1) と s[l2..r2) を辞書順比較し、左<右なら -1、等しいなら 0、左>右なら 1 を返す、O(1)
    int compare_substring(int l1, int r1, int l2, int r2) const {
        int n = int(s.size());
        assert(0 <= l1 && l1 <= r1 && r1 <= n);
        assert(0 <= l2 && l2 <= r2 && r2 <= n);

        int len1 = r1 - l1;
        int len2 = r2 - l2;
        int common = substring_lcp(l1, r1, l2, r2);
        if (common == std::min(len1, len2)) {
            if (len1 == len2) return 0;
            return len1 < len2 ? -1 : 1;
        }

        unsigned char a = static_cast<unsigned char>(s[l1 + common]);
        unsigned char b = static_cast<unsigned char>(s[l2 + common]);
        return a < b ? -1 : 1;
    }

    // 部分文字列 s[l1..r1) と s[l2..r2) が等しいかを返す、O(1)
    bool equal_substring(int l1, int r1, int l2, int r2) const {
        if (r1 - l1 != r2 - l2) return false;
        return substring_lcp(l1, r1, l2, r2) == r1 - l1;
    }

    // suffix s[i..] の suffix array 上の順位を返す、O(1)
    int suffix_rank(int i) const {
        assert(0 <= i && i < int(rank.size()));
        return rank[i];
    }
};

// ------------------------------------------------------------
// 5. 最長重複部分文字列
// ------------------------------------------------------------

/*
 * SaLongestRepeatedSubstringResult
 *
 * sa_longest_repeated_substring(s) の戻り値。
 * length は 2 回以上出現する最長部分文字列の長さ、pos1 と pos2 は代表的な 2 つの開始位置。
 * length == 0 の場合は、該当する正の長さの部分文字列がなく、pos1 = pos2 = -1 になる。
 *
 * 使い方:
 *   auto res = sa_longest_repeated_substring(s);
 *   string t = (res.length == 0 ? "" : s.substr(res.pos1, res.length));
 */
struct SaLongestRepeatedSubstringResult {
    int length;
    int pos1;
    int pos2;
};

// 2 回以上出現する最長部分文字列の長さと代表 2 位置を返す、O(n)
inline SaLongestRepeatedSubstringResult sa_longest_repeated_substring(const std::string& s) {
    int n = int(s.size());
    if (n <= 1) return {0, -1, -1};

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    SaLongestRepeatedSubstringResult res{0, -1, -1};
    for (int i = 0; i < int(lcp.size()); i++) {
        if (lcp[i] > res.length) {
            res.length = lcp[i];
            res.pos1 = sa[i];
            res.pos2 = sa[i + 1];
        }
    }
    return res;
}

// ------------------------------------------------------------
// 6. K 回以上出現する最長部分文字列
// ------------------------------------------------------------

// K 回以上出現する部分文字列の最大長を返す、O(n)
inline int sa_longest_k_repeated_substring_length(const std::string& s, int k) {
    int n = int(s.size());
    if (k <= 1) return n;
    if (k > n || n == 0) return 0;

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    int width = k - 1;
    int ans = 0;
    std::deque<int> dq;

    // LCP 配列上の長さ width の sliding minimum が、連続 k suffix の共通 prefix 長になる。
    for (int i = 0; i < int(lcp.size()); i++) {
        while (!dq.empty() && lcp[dq.back()] >= lcp[i]) dq.pop_back();
        dq.push_back(i);

        int left = i - width + 1;
        while (!dq.empty() && dq.front() < left) dq.pop_front();

        if (left >= 0) ans = std::max(ans, lcp[dq.front()]);
    }
    return ans;
}

// ------------------------------------------------------------
// 7. 非重複で 2 回出る最長部分文字列
// ------------------------------------------------------------

/*
 * SaLongestNonoverlapRepeatedSubstringResult
 *
 * sa_longest_nonoverlap_repeated_substring(s) の戻り値。
 * length は重ならずに 2 回出現する最長部分文字列の長さ、pos1 と pos2 は代表的な 2 つの開始位置。
 * [pos1, pos1 + length) と [pos2, pos2 + length) は重ならない。
 * length == 0 の場合は、該当する正の長さの部分文字列がなく、pos1 = pos2 = -1 になる。
 *
 * 使い方:
 *   auto res = sa_longest_nonoverlap_repeated_substring(s);
 *   if (res.length > 0) { ... }
 */
struct SaLongestNonoverlapRepeatedSubstringResult {
    int length;
    int pos1;
    int pos2;
};

// 重ならずに 2 回出現する最長部分文字列の長さと代表 2 位置を返す、O(n log n)
inline SaLongestNonoverlapRepeatedSubstringResult sa_longest_nonoverlap_repeated_substring(const std::string& s) {
    int n = int(s.size());
    if (n <= 1) return {0, -1, -1};

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    auto check = [&](int len) -> std::pair<bool, std::pair<int, int>> {
        if (len == 0) return {true, {-1, -1}};

        // lcp >= len でつながる suffix array 上の連続成分を調べる。
        for (int i = 0; i < n;) {
            int j = i;
            int mn = sa[i];
            int mx = sa[i];

            while (j < n - 1 && lcp[j] >= len) {
                j++;
                mn = std::min(mn, sa[j]);
                mx = std::max(mx, sa[j]);
            }

            // 同じ len 文字 prefix を持つ suffix 群の中で、開始位置差が len 以上なら非重複に置ける。
            if (j > i && mx - mn >= len) return {true, {mn, mx}};
            i = j + 1;
        }
        return {false, {-1, -1}};
    };

    int low = 0;
    int high = n + 1;
    while (high - low > 1) {
        int mid = (low + high) / 2;
        if (check(mid).first) low = mid;
        else high = mid;
    }

    auto [ok, pos] = check(low);
    if (!ok || low == 0) return {0, -1, -1};
    return {low, pos.first, pos.second};
}

// ------------------------------------------------------------
// 8. unique substring 系
// ------------------------------------------------------------

// 各位置から始まる最短 unique substring の長さを返し、存在しない位置は -1 にする、O(n)
inline std::vector<int> sa_shortest_unique_substring_lengths(const std::string& s) {
    int n = int(s.size());
    std::vector<int> res(n, -1);
    if (n == 0) return res;

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    // suffix array 上で隣接する suffix だけが、最長一致の候補になる。
    for (int i = 0; i < n; i++) {
        int overlap = 0;
        if (i > 0) overlap = std::max(overlap, lcp[i - 1]);
        if (i + 1 < n) overlap = std::max(overlap, lcp[i]);

        int need = overlap + 1;
        int pos = sa[i];
        if (need <= n - pos) res[pos] = need;
    }
    return res;
}

// 文字列全体でちょうど 1 回だけ出現する部分文字列数を返す、O(n)
inline long long sa_once_occurring_substring_count(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return 0;

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    long long ans = 0;
    for (int i = 0; i < n; i++) {
        int overlap = 0;
        if (i > 0) overlap = std::max(overlap, lcp[i - 1]);
        if (i + 1 < n) overlap = std::max(overlap, lcp[i]);

        int suffix_len = n - sa[i];
        if (suffix_len > overlap) ans += suffix_len - overlap;
    }
    return ans;
}

// ------------------------------------------------------------
// 9. 2 文字列の最長共通部分文字列
// ------------------------------------------------------------

/*
 * SaLongestCommonSubstringResult
 *
 * sa_longest_common_substring(a, b) の戻り値。
 * length は a と b に共通して現れる最長部分文字列の長さ。
 * pos_a は a 側の開始位置、pos_b は b 側の開始位置。
 * length == 0 の場合は、非空の共通部分文字列がなく、pos_a = pos_b = -1 になる。
 *
 * 使い方:
 *   auto res = sa_longest_common_substring(a, b);
 *   string t = (res.length == 0 ? "" : a.substr(res.pos_a, res.length));
 */
struct SaLongestCommonSubstringResult {
    int length;
    int pos_a;
    int pos_b;
};

// 2 文字列の最長共通部分文字列の長さと代表位置を返す、O((n+m) + alphabet)
inline SaLongestCommonSubstringResult sa_longest_common_substring(const std::string& a, const std::string& b) {
    int n = int(a.size());
    int m = int(b.size());

    std::vector<int> v;
    std::vector<int> owner;
    v.reserve(n + m + 1);
    owner.reserve(n + m + 1);

    // separator=0、文字=1..256 として、入力文字と separator の衝突を避ける。
    for (unsigned char c : a) {
        v.push_back(int(c) + 1);
        owner.push_back(0);
    }
    v.push_back(0);
    owner.push_back(-1);
    for (unsigned char c : b) {
        v.push_back(int(c) + 1);
        owner.push_back(1);
    }

    if (v.empty()) return {0, -1, -1};
    std::vector<int> sa = atcoder::suffix_array(v, 256);
    std::vector<int> lcp = atcoder::lcp_array(v, sa);

    SaLongestCommonSubstringResult res{0, -1, -1};
    for (int i = 0; i + 1 < int(sa.size()); i++) {
        int x = owner[sa[i]];
        int y = owner[sa[i + 1]];
        if (x < 0 || y < 0 || x == y) continue;
        if (lcp[i] > res.length) {
            res.length = lcp[i];
            if (x == 0) {
                res.pos_a = sa[i];
                res.pos_b = sa[i + 1] - (n + 1);
            } else {
                res.pos_a = sa[i + 1];
                res.pos_b = sa[i] - (n + 1);
            }
        }
    }
    if (res.length == 0) return {0, -1, -1};
    return res;
}

// ------------------------------------------------------------
// 10. 複数文字列の最長共通部分文字列
// ------------------------------------------------------------

// 複数文字列のうち少なくとも k 個に出現する部分文字列の最大長を返す、O(N + M + alphabet)
inline int sa_longest_common_substring_at_least_k(const std::vector<std::string>& ss, int k) {
    int m = int(ss.size());
    if (k <= 0) return 0;
    if (m == 0 || k > m) return 0;
    if (k == 1) {
        int ans = 0;
        for (const auto& s : ss) ans = std::max(ans, int(s.size()));
        return ans;
    }

    std::vector<int> v;
    std::vector<int> owner;
    int total = 0;
    for (const auto& s : ss) total += int(s.size()) + 1;
    v.reserve(total);
    owner.reserve(total);

    // separator は 0..m-1、文字は m..m+255 にして、全 separator をユニークにする。
    for (int id = 0; id < m; id++) {
        for (unsigned char c : ss[id]) {
            v.push_back(m + int(c));
            owner.push_back(id);
        }
        v.push_back(id);
        owner.push_back(-1);
    }

    std::vector<int> sa = atcoder::suffix_array(v, m + 255);
    std::vector<int> lcp = atcoder::lcp_array(v, sa);

    int n = int(sa.size());
    std::vector<int> cnt(m, 0);
    int kinds = 0;
    int left = 0;
    int ans = 0;
    std::deque<int> dq;

    auto add_owner = [&](int o) {
        if (o < 0) return;
        if (cnt[o] == 0) kinds++;
        cnt[o]++;
    };
    auto remove_owner = [&](int o) {
        if (o < 0) return;
        cnt[o]--;
        if (cnt[o] == 0) kinds--;
    };
    auto can_remove_left = [&](int o) -> bool {
        if (o < 0) return true;
        return kinds - (cnt[o] == 1 ? 1 : 0) >= k;
    };

    // suffix array 上の window を動かし、window 内 LCP の最小値を deque で管理する。
    for (int right = 0; right < n; right++) {
        if (right > 0) {
            int idx = right - 1;
            while (!dq.empty() && lcp[dq.back()] >= lcp[idx]) dq.pop_back();
            dq.push_back(idx);
            while (!dq.empty() && dq.front() < left) dq.pop_front();
        }
        add_owner(owner[sa[right]]);

        while (left <= right && can_remove_left(owner[sa[left]])) {
            remove_owner(owner[sa[left]]);
            left++;
            while (!dq.empty() && dq.front() < left) dq.pop_front();
        }

        if (kinds >= k && !dq.empty()) ans = std::max(ans, lcp[dq.front()]);
    }
    return ans;
}

// 複数文字列すべてに共通する部分文字列の最大長を返す、O(N + M + alphabet)
inline int sa_longest_common_substring_all(const std::vector<std::string>& ss) {
    return sa_longest_common_substring_at_least_k(ss, int(ss.size()));
}

// ------------------------------------------------------------
// 11. cyclic shift 系
// ------------------------------------------------------------

// 辞書順最小 cyclic shift の開始位置を 1 つ返す、O(n)
inline int sa_min_cyclic_shift_index(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return 0;

    std::string t = s + s;
    std::vector<int> sa = atcoder::suffix_array(t);
    for (int p : sa) {
        if (p < n) return p;
    }
    assert(false);
    return 0;
}

// 辞書順最大 cyclic shift の開始位置を 1 つ返す、O(n)
inline int sa_max_cyclic_shift_index(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return 0;

    std::string t = s + s;
    std::vector<int> sa = atcoder::suffix_array(t);
    for (int i = int(sa.size()) - 1; i >= 0; i--) {
        if (sa[i] < n) return sa[i];
    }
    assert(false);
    return 0;
}

// 辞書順最小 cyclic shift の文字列を返す、O(n)
inline std::string sa_min_cyclic_shift(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return std::string();
    int p = sa_min_cyclic_shift_index(s);
    return (s + s).substr(p, n);
}

// 辞書順最大 cyclic shift の文字列を返す、O(n)
inline std::string sa_max_cyclic_shift(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return std::string();
    int p = sa_max_cyclic_shift_index(s);
    return (s + s).substr(p, n);
}

// ------------------------------------------------------------
// 12. 異なる cyclic shift 数
// ------------------------------------------------------------

// 文字列 s の異なる cyclic shift の個数を返す、O(n)
inline int sa_distinct_cyclic_shift_count(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return 0;

    std::string t = s + s;
    std::vector<int> sa = atcoder::suffix_array(t);
    std::vector<int> lcp = atcoder::lcp_array(t, sa);

    int count = 0;
    int previous_target_rank = -1;
    int min_lcp_since_previous_target = std::numeric_limits<int>::max();

    // 開始位置 < n の suffix だけを長さ n の回転として見る。
    for (int rank = 0; rank < int(sa.size()); rank++) {
        if (rank > 0) {
            min_lcp_since_previous_target = std::min(min_lcp_since_previous_target, lcp[rank - 1]);
        }
        if (sa[rank] >= n) continue;

        if (previous_target_rank == -1) {
            count = 1;
        } else if (min_lcp_since_previous_target < n) {
            count++;
        }

        previous_target_rank = rank;
        min_lcp_since_previous_target = std::numeric_limits<int>::max();
    }
    return count;
}

// ------------------------------------------------------------
// 13. 整数列の異なる subarray 数
// ------------------------------------------------------------

// 0<=a[i]<=upper の整数列 a の異なる連続部分列数を返す、O(n + upper)
inline long long sa_distinct_subarray_count(const std::vector<int>& a, int upper) {
    assert(upper >= 0);
    int n = int(a.size());
    if (n == 0) return 0;
    for (int x : a) assert(0 <= x && x <= upper);

    std::vector<int> sa = atcoder::suffix_array(a, upper);
    std::vector<int> lcp = atcoder::lcp_array(a, sa);

    long long ans = 1LL * n * (n + 1) / 2;
    for (int x : lcp) ans -= x;
    return ans;
}

// ------------------------------------------------------------
// 14. 整数列の最長共通 subarray
// ------------------------------------------------------------

/*
 * SaLongestCommonSubarrayResult
 *
 * sa_longest_common_subarray(a, b) の戻り値。
 * length は 2 つの整数列に共通して現れる最長連続部分列の長さ。
 * pos_a は a 側の開始位置、pos_b は b 側の開始位置。
 * length == 0 の場合は、非空の共通連続部分列がなく、pos_a = pos_b = -1 になる。
 *
 * 使い方:
 *   auto res = sa_longest_common_subarray(a, b);
 *   // a[res.pos_a .. res.pos_a + res.length) が代表解
 */
struct SaLongestCommonSubarrayResult {
    int length;
    int pos_a;
    int pos_b;
};

// 2 つの整数列の最長共通連続部分列の長さと代表位置を返す、O((n+m) log(n+m))
inline SaLongestCommonSubarrayResult sa_longest_common_subarray(const std::vector<int>& a, const std::vector<int>& b) {
    int n = int(a.size());
    int m = int(b.size());

    std::vector<int> vals;
    vals.reserve(n + m);
    for (int x : a) vals.push_back(x);
    for (int x : b) vals.push_back(x);
    std::sort(vals.begin(), vals.end());
    vals.erase(std::unique(vals.begin(), vals.end()), vals.end());

    auto code = [&](int x) {
        return int(std::lower_bound(vals.begin(), vals.end(), x) - vals.begin()) + 1;
    };

    std::vector<int> v;
    std::vector<int> owner;
    v.reserve(n + m + 1);
    owner.reserve(n + m + 1);

    // separator=0、値は 1..|vals| として、separator 衝突を避ける。
    for (int x : a) {
        v.push_back(code(x));
        owner.push_back(0);
    }
    v.push_back(0);
    owner.push_back(-1);
    for (int x : b) {
        v.push_back(code(x));
        owner.push_back(1);
    }

    std::vector<int> sa = atcoder::suffix_array(v, int(vals.size()));
    std::vector<int> lcp = atcoder::lcp_array(v, sa);

    SaLongestCommonSubarrayResult res{0, -1, -1};
    for (int i = 0; i + 1 < int(sa.size()); i++) {
        int x = owner[sa[i]];
        int y = owner[sa[i + 1]];
        if (x < 0 || y < 0 || x == y) continue;
        if (lcp[i] > res.length) {
            res.length = lcp[i];
            if (x == 0) {
                res.pos_a = sa[i];
                res.pos_b = sa[i + 1] - (n + 1);
            } else {
                res.pos_a = sa[i + 1];
                res.pos_b = sa[i] - (n + 1);
            }
        }
    }
    if (res.length == 0) return {0, -1, -1};
    return res;
}

// ------------------------------------------------------------
// 15. 整数列の最長重複 subarray
// ------------------------------------------------------------

/*
 * SaLongestRepeatedSubarrayResult
 *
 * sa_longest_repeated_subarray(a) の戻り値。
 * length は整数列 a の中で 2 回以上出現する最長連続部分列の長さ。
 * pos1 と pos2 は代表的な 2 つの開始位置。
 * length == 0 の場合は、該当する正の長さの連続部分列がなく、pos1 = pos2 = -1 になる。
 *
 * 使い方:
 *   auto res = sa_longest_repeated_subarray(a);
 *   if (res.length > 0) { ... }
 */
struct SaLongestRepeatedSubarrayResult {
    int length;
    int pos1;
    int pos2;
};

// 整数列 a の中で 2 回以上出現する最長連続部分列の長さと代表 2 位置を返す、O(n log n)
inline SaLongestRepeatedSubarrayResult sa_longest_repeated_subarray(const std::vector<int>& a) {
    int n = int(a.size());
    if (n <= 1) return {0, -1, -1};

    std::vector<int> vals = a;
    std::sort(vals.begin(), vals.end());
    vals.erase(std::unique(vals.begin(), vals.end()), vals.end());

    std::vector<int> v(n);
    for (int i = 0; i < n; i++) {
        v[i] = int(std::lower_bound(vals.begin(), vals.end(), a[i]) - vals.begin());
    }

    std::vector<int> sa = atcoder::suffix_array(v, int(vals.size()) - 1);
    std::vector<int> lcp = atcoder::lcp_array(v, sa);

    SaLongestRepeatedSubarrayResult res{0, -1, -1};
    for (int i = 0; i < int(lcp.size()); i++) {
        if (lcp[i] > res.length) {
            res.length = lcp[i];
            res.pos1 = sa[i];
            res.pos2 = sa[i + 1];
        }
    }
    return res;
}

// ------------------------------------------------------------
// 16. suffix LCP 和
// ------------------------------------------------------------

// res[i]=suffix s[i..] と全 suffix の LCP 長の和を返す、O(n)
inline std::vector<long long> sa_suffix_lcp_sum_with_all(const std::string& s) {
    int n = int(s.size());
    std::vector<long long> res(n, 0);
    if (n == 0) return res;

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    std::vector<long long> left(n, 0), right(n, 0);
    std::vector<std::pair<int, int>> stack;
    long long current = 0;

    // left[r] は suffix array rank r の suffix と、左側の全 suffix との LCP 和。
    stack.clear();
    current = 0;
    for (int i = 0; i < n - 1; i++) {
        int x = lcp[i];
        int count = 1;
        while (!stack.empty() && stack.back().first >= x) {
            current -= 1LL * stack.back().first * stack.back().second;
            count += stack.back().second;
            stack.pop_back();
        }
        stack.push_back({x, count});
        current += 1LL * x * count;
        left[i + 1] = current;
    }

    // right[r] は suffix array rank r の suffix と、右側の全 suffix との LCP 和。
    stack.clear();
    current = 0;
    for (int i = n - 2; i >= 0; i--) {
        int x = lcp[i];
        int count = 1;
        while (!stack.empty() && stack.back().first >= x) {
            current -= 1LL * stack.back().first * stack.back().second;
            count += stack.back().second;
            stack.pop_back();
        }
        stack.push_back({x, count});
        current += 1LL * x * count;
        right[i] = current;
    }

    // 自分自身との LCP は suffix 長そのもの。
    for (int r = 0; r < n; r++) {
        int pos = sa[r];
        res[pos] = left[r] + right[r] + (n - pos);
    }
    return res;
}

// 全ての unordered な suffix pair の LCP 和を返す、O(n)
inline long long sa_total_suffix_pair_lcp(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return 0;

    std::vector<long long> sums = sa_suffix_lcp_sum_with_all(s);
    long long total_with_self_and_double = 0;
    for (long long x : sums) total_with_self_and_double += x;

    long long self = 1LL * n * (n + 1) / 2;
    return (total_with_self_and_double - self) / 2;
}

// ------------------------------------------------------------
// 17. length × occurrence 最大化
// ------------------------------------------------------------

/*
 * SaMaxLengthTimesOccurrenceResult
 *
 * sa_max_length_times_occurrence(s) の戻り値。
 * score は length * occurrences の最大値。
 * length は選ばれた部分文字列の長さ、occurrences はその出現回数、pos は代表開始位置。
 * 空文字列の場合は score = length = occurrences = 0, pos = -1 になる。
 *
 * 使い方:
 *   auto res = sa_max_length_times_occurrence(s);
 *   string t = (res.length == 0 ? "" : s.substr(res.pos, res.length));
 */
struct SaMaxLengthTimesOccurrenceResult {
    long long score;
    int length;
    int occurrences;
    int pos;
};

// 部分文字列の length * occurrence_count の最大値と代表情報を返す、O(n)
inline SaMaxLengthTimesOccurrenceResult sa_max_length_times_occurrence(const std::string& s) {
    int n = int(s.size());
    if (n == 0) return {0, 0, 0, -1};

    std::vector<int> sa = atcoder::suffix_array(s);
    std::vector<int> lcp = atcoder::lcp_array(s, sa);

    // 1 回だけ出る部分文字列として、文字列全体を初期候補にする。
    SaMaxLengthTimesOccurrenceResult res{n, n, 1, 0};
    std::vector<int> stack;

    // LCP 配列をヒストグラムと見て、高さ=長さ、幅+1=出現回数として処理する。
    for (int i = 0; i <= int(lcp.size()); i++) {
        int x = (i == int(lcp.size()) ? 0 : lcp[i]);
        while (!stack.empty() && lcp[stack.back()] > x) {
            int idx = stack.back();
            stack.pop_back();

            int left = stack.empty() ? -1 : stack.back();
            int right = i;
            int width_edges = right - left - 1;
            int occurrences = width_edges + 1;
            int length = lcp[idx];
            long long score = 1LL * length * occurrences;
            int pos = sa[left + 1];

            if (score > res.score) {
                res = {score, length, occurrences, pos};
            }
        }
        stack.push_back(i);
    }
    return res;
}

#if __INCLUDE_LEVEL__ == 0

// ------------------------------------------------------------
// テスト用 naive 実装群
// ------------------------------------------------------------

// テスト用の乱数を [0, upper) の int に変換する、O(1)
static int test_rand_int(std::mt19937& rng, int upper) {
    assert(upper > 0);
    using result_type = std::mt19937::result_type;
    return static_cast<int>(rng() % static_cast<result_type>(upper));
}

static int test_lcp_string_suffix(const std::string& s, int i, int j) {
    int n = int(s.size());
    int ans = 0;
    while (i + ans < n && j + ans < n && s[i + ans] == s[j + ans]) ans++;
    return ans;
}

static std::vector<std::string> test_all_distinct_substrings(const std::string& s) {
    std::vector<std::string> v;
    int n = int(s.size());
    for (int l = 0; l < n; l++) {
        for (int r = l + 1; r <= n; r++) v.push_back(s.substr(l, r - l));
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

static int test_count_occurrence(const std::string& s, const std::string& p) {
    if (p.empty()) return int(s.size());
    int ans = 0;
    for (int i = 0; i + int(p.size()) <= int(s.size()); i++) {
        if (s.compare(i, p.size(), p) == 0) ans++;
    }
    return ans;
}

static std::vector<int> test_positions(const std::string& s, const std::string& p) {
    std::vector<int> res;
    if (p.empty()) {
        for (int i = 0; i < int(s.size()); i++) res.push_back(i);
        return res;
    }
    for (int i = 0; i + int(p.size()) <= int(s.size()); i++) {
        if (s.compare(i, p.size(), p) == 0) res.push_back(i);
    }
    return res;
}

static int test_longest_repeated_length(const std::string& s) {
    std::vector<std::string> v;
    int n = int(s.size());
    for (int l = 0; l < n; l++) {
        for (int r = l + 1; r <= n; r++) v.push_back(s.substr(l, r - l));
    }
    std::sort(v.begin(), v.end());
    int ans = 0;
    for (int i = 0; i < int(v.size());) {
        int j = i + 1;
        while (j < int(v.size()) && v[j] == v[i]) j++;
        if (j - i >= 2) ans = std::max(ans, int(v[i].size()));
        i = j;
    }
    return ans;
}

static int test_longest_k_repeated_length(const std::string& s, int k) {
    int n = int(s.size());
    if (k <= 1) return n;
    if (k > n) return 0;

    std::vector<std::string> v;
    for (int l = 0; l < n; l++) {
        for (int r = l + 1; r <= n; r++) v.push_back(s.substr(l, r - l));
    }
    std::sort(v.begin(), v.end());
    int ans = 0;
    for (int i = 0; i < int(v.size());) {
        int j = i + 1;
        while (j < int(v.size()) && v[j] == v[i]) j++;
        if (j - i >= k) ans = std::max(ans, int(v[i].size()));
        i = j;
    }
    return ans;
}

static int test_longest_nonoverlap_repeated_length(const std::string& s) {
    int n = int(s.size());
    int ans = 0;
    for (int len = 1; len <= n; len++) {
        for (int i = 0; i + len <= n; i++) {
            for (int j = i + len; j + len <= n; j++) {
                if (s.compare(i, len, s, j, len) == 0) ans = len;
            }
        }
    }
    return ans;
}

static std::vector<int> test_shortest_unique_lengths(const std::string& s) {
    int n = int(s.size());
    std::vector<int> res(n, -1);
    for (int i = 0; i < n; i++) {
        for (int len = 1; i + len <= n; len++) {
            int cnt = 0;
            for (int j = 0; j + len <= n; j++) {
                if (s.compare(i, len, s, j, len) == 0) cnt++;
            }
            if (cnt == 1) {
                res[i] = len;
                break;
            }
        }
    }
    return res;
}

static long long test_once_occurring_count(const std::string& s) {
    std::vector<std::string> v;
    int n = int(s.size());
    for (int l = 0; l < n; l++) {
        for (int r = l + 1; r <= n; r++) v.push_back(s.substr(l, r - l));
    }
    std::sort(v.begin(), v.end());
    long long ans = 0;
    for (int i = 0; i < int(v.size());) {
        int j = i + 1;
        while (j < int(v.size()) && v[j] == v[i]) j++;
        if (j - i == 1) ans++;
        i = j;
    }
    return ans;
}

static int test_longest_common_substring_length(const std::string& a, const std::string& b) {
    int ans = 0;
    for (int i = 0; i < int(a.size()); i++) {
        for (int len = 1; i + len <= int(a.size()); len++) {
            std::string p = a.substr(i, len);
            if (test_count_occurrence(b, p) > 0) ans = std::max(ans, len);
        }
    }
    return ans;
}

static int test_longest_common_substring_at_least_k(const std::vector<std::string>& ss, int k) {
    int m = int(ss.size());
    if (k <= 0 || k > m || m == 0) return 0;
    if (k == 1) {
        int ans = 0;
        for (const auto& s : ss) ans = std::max(ans, int(s.size()));
        return ans;
    }

    std::vector<std::pair<std::string, int>> all;
    for (int id = 0; id < m; id++) {
        std::vector<std::string> cur = test_all_distinct_substrings(ss[id]);
        for (auto& x : cur) all.push_back({x, id});
    }
    std::sort(all.begin(), all.end());

    int ans = 0;
    for (int i = 0; i < int(all.size());) {
        int j = i + 1;
        std::vector<int> ids;
        ids.push_back(all[i].second);
        while (j < int(all.size()) && all[j].first == all[i].first) {
            ids.push_back(all[j].second);
            j++;
        }
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        if (int(ids.size()) >= k) ans = std::max(ans, int(all[i].first.size()));
        i = j;
    }
    return ans;
}

static std::string test_rotation(const std::string& s, int p) {
    return s.substr(p) + s.substr(0, p);
}

static std::string test_min_cyclic_shift(const std::string& s) {
    if (s.empty()) return std::string();
    std::string ans = test_rotation(s, 0);
    for (int i = 1; i < int(s.size()); i++) ans = std::min(ans, test_rotation(s, i));
    return ans;
}

static std::string test_max_cyclic_shift(const std::string& s) {
    if (s.empty()) return std::string();
    std::string ans = test_rotation(s, 0);
    for (int i = 1; i < int(s.size()); i++) ans = std::max(ans, test_rotation(s, i));
    return ans;
}

static int test_distinct_cyclic_shift_count(const std::string& s) {
    if (s.empty()) return 0;
    std::vector<std::string> v;
    for (int i = 0; i < int(s.size()); i++) v.push_back(test_rotation(s, i));
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return int(v.size());
}

static long long test_distinct_subarray_count(std::vector<int> a) {
    std::vector<std::vector<int>> v;
    int n = int(a.size());
    for (int l = 0; l < n; l++) {
        std::vector<int> cur;
        for (int r = l; r < n; r++) {
            cur.push_back(a[r]);
            v.push_back(cur);
        }
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return int(v.size());
}

static int test_longest_common_subarray_length(const std::vector<int>& a, const std::vector<int>& b) {
    int ans = 0;
    for (int i = 0; i < int(a.size()); i++) {
        for (int j = 0; j < int(b.size()); j++) {
            int k = 0;
            while (i + k < int(a.size()) && j + k < int(b.size()) && a[i + k] == b[j + k]) k++;
            ans = std::max(ans, k);
        }
    }
    return ans;
}

static int test_longest_repeated_subarray_length(const std::vector<int>& a) {
    int n = int(a.size());
    int ans = 0;
    for (int len = 1; len <= n; len++) {
        std::vector<std::vector<int>> v;
        for (int i = 0; i + len <= n; i++) {
            v.push_back(std::vector<int>(a.begin() + i, a.begin() + i + len));
        }
        std::sort(v.begin(), v.end());
        for (int i = 1; i < int(v.size()); i++) {
            if (v[i] == v[i - 1]) ans = len;
        }
    }
    return ans;
}

static std::vector<long long> test_suffix_lcp_sum_with_all(const std::string& s) {
    int n = int(s.size());
    std::vector<long long> res(n, 0);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) res[i] += test_lcp_string_suffix(s, i, j);
    }
    return res;
}

static long long test_total_suffix_pair_lcp(const std::string& s) {
    int n = int(s.size());
    long long ans = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) ans += test_lcp_string_suffix(s, i, j);
    }
    return ans;
}

static long long test_max_length_times_occurrence_score(const std::string& s) {
    int n = int(s.size());
    std::vector<std::string> v;
    for (int l = 0; l < n; l++) {
        for (int r = l + 1; r <= n; r++) v.push_back(s.substr(l, r - l));
    }
    std::sort(v.begin(), v.end());
    long long ans = 0;
    for (int i = 0; i < int(v.size());) {
        int j = i + 1;
        while (j < int(v.size()) && v[j] == v[i]) j++;
        ans = std::max(ans, 1LL * int(v[i].size()) * (j - i));
        i = j;
    }
    return ans;
}

static void test_fixed_cases() {
    std::vector<std::string> cases = {
        "", "a", "aa", "ab", "banana", "mississippi", "abababa", "abcdef", "aaaaaa", "abcabcabc"
    };

    for (const std::string& s : cases) {
        auto distinct = test_all_distinct_substrings(s);
        assert(sa_distinct_substring_count(s) == static_cast<long long>(distinct.size()));
        for (int k = 1; k <= int(distinct.size()); k++) {
            assert(sa_kth_distinct_substring(s, k) == distinct[k - 1]);
        }

        SaPatternSearch ps(s);
        std::vector<std::string> patterns = {"", "a", "b", "na", "ana", "x", s};
        for (const auto& p : patterns) {
            assert(ps.count(p) == test_count_occurrence(s, p));
            assert(ps.contains(p) == (test_count_occurrence(s, p) > 0));
            assert(ps.positions(p) == test_positions(s, p));
        }

        SaSubstringLcpCompare cmp(s);
        int n = int(s.size());
        for (int l1 = 0; l1 <= n; l1++) for (int r1 = l1; r1 <= n; r1++) {
            for (int l2 = 0; l2 <= n; l2++) for (int r2 = l2; r2 <= n; r2++) {
                std::string a = s.substr(l1, r1 - l1);
                std::string b = s.substr(l2, r2 - l2);
                int c1 = cmp.compare_substring(l1, r1, l2, r2);
                int c2 = (a < b ? -1 : (a > b ? 1 : 0));
                assert(c1 == c2);
                assert(cmp.equal_substring(l1, r1, l2, r2) == (a == b));
            }
        }

        auto rep = sa_longest_repeated_substring(s);
        assert(rep.length == test_longest_repeated_length(s));
        if (rep.length > 0) {
            assert(rep.pos1 != rep.pos2);
            assert(s.compare(rep.pos1, rep.length, s, rep.pos2, rep.length) == 0);
        }

        for (int k = 1; k <= n + 2; k++) {
            assert(sa_longest_k_repeated_substring_length(s, k) == test_longest_k_repeated_length(s, k));
        }

        auto nonover = sa_longest_nonoverlap_repeated_substring(s);
        assert(nonover.length == test_longest_nonoverlap_repeated_length(s));
        if (nonover.length > 0) {
            assert(std::abs(nonover.pos1 - nonover.pos2) >= nonover.length);
            assert(s.compare(nonover.pos1, nonover.length, s, nonover.pos2, nonover.length) == 0);
        }

        assert(sa_shortest_unique_substring_lengths(s) == test_shortest_unique_lengths(s));
        assert(sa_once_occurring_substring_count(s) == test_once_occurring_count(s));
        assert(sa_min_cyclic_shift(s) == test_min_cyclic_shift(s));
        assert(sa_max_cyclic_shift(s) == test_max_cyclic_shift(s));
        assert(sa_distinct_cyclic_shift_count(s) == test_distinct_cyclic_shift_count(s));
        assert(sa_suffix_lcp_sum_with_all(s) == test_suffix_lcp_sum_with_all(s));
        assert(sa_total_suffix_pair_lcp(s) == test_total_suffix_pair_lcp(s));
        assert(sa_max_length_times_occurrence(s).score == test_max_length_times_occurrence_score(s));
    }
}

static void test_random_strings() {
    std::mt19937 rng(1234567);
    for (int tc = 0; tc < 300; tc++) {
        int n = test_rand_int(rng, 9);
        int sigma = 1 + test_rand_int(rng, 4);
        std::string s;
        for (int i = 0; i < n; i++) s.push_back(char('a' + test_rand_int(rng, sigma)));

        auto distinct = test_all_distinct_substrings(s);
        assert(sa_distinct_substring_count(s) == static_cast<long long>(distinct.size()));
        for (int k = 1; k <= int(distinct.size()); k++) {
            assert(sa_kth_distinct_substring(s, k) == distinct[k - 1]);
        }

        SaPatternSearch ps(s);
        for (int q = 0; q < 20; q++) {
            int len = test_rand_int(rng, 5);
            std::string p;
            for (int i = 0; i < len; i++) p.push_back(char('a' + test_rand_int(rng, 4)));
            assert(ps.count(p) == test_count_occurrence(s, p));
            assert(ps.contains(p) == (test_count_occurrence(s, p) > 0));
            assert(ps.positions(p) == test_positions(s, p));
        }

        SaSubstringLcpCompare cmp(s);
        for (int q = 0; q < 200; q++) {
            int l1 = test_rand_int(rng, n + 1);
            int r1 = l1 + test_rand_int(rng, n - l1 + 1);
            int l2 = test_rand_int(rng, n + 1);
            int r2 = l2 + test_rand_int(rng, n - l2 + 1);
            std::string a = s.substr(l1, r1 - l1);
            std::string b = s.substr(l2, r2 - l2);
            int c1 = cmp.compare_substring(l1, r1, l2, r2);
            int c2 = (a < b ? -1 : (a > b ? 1 : 0));
            assert(c1 == c2);
            assert(cmp.equal_substring(l1, r1, l2, r2) == (a == b));
        }

        assert(sa_longest_repeated_substring(s).length == test_longest_repeated_length(s));
        for (int k = 1; k <= n + 2; k++) {
            assert(sa_longest_k_repeated_substring_length(s, k) == test_longest_k_repeated_length(s, k));
        }
        assert(sa_longest_nonoverlap_repeated_substring(s).length == test_longest_nonoverlap_repeated_length(s));
        assert(sa_shortest_unique_substring_lengths(s) == test_shortest_unique_lengths(s));
        assert(sa_once_occurring_substring_count(s) == test_once_occurring_count(s));
        assert(sa_min_cyclic_shift(s) == test_min_cyclic_shift(s));
        assert(sa_max_cyclic_shift(s) == test_max_cyclic_shift(s));
        assert(sa_distinct_cyclic_shift_count(s) == test_distinct_cyclic_shift_count(s));
        assert(sa_suffix_lcp_sum_with_all(s) == test_suffix_lcp_sum_with_all(s));
        assert(sa_total_suffix_pair_lcp(s) == test_total_suffix_pair_lcp(s));
        assert(sa_max_length_times_occurrence(s).score == test_max_length_times_occurrence_score(s));
    }
}

static void test_common_strings() {
    std::mt19937 rng(7654321);
    std::vector<std::pair<std::string, std::string>> fixed = {
        {"", ""}, {"a", ""}, {"", "b"}, {"abc", "def"}, {"banana", "ananas"}, {"aaaa", "aa"},
        {"abcab", "bcabc"}
    };
    for (auto [a, b] : fixed) {
        auto res = sa_longest_common_substring(a, b);
        assert(res.length == test_longest_common_substring_length(a, b));
        if (res.length > 0) assert(a.compare(res.pos_a, res.length, b, res.pos_b, res.length) == 0);
    }

    for (int tc = 0; tc < 200; tc++) {
        int n = test_rand_int(rng, 8);
        int m = test_rand_int(rng, 8);
        std::string a, b;
        for (int i = 0; i < n; i++) a.push_back(char('a' + test_rand_int(rng, 4)));
        for (int i = 0; i < m; i++) b.push_back(char('a' + test_rand_int(rng, 4)));
        auto res = sa_longest_common_substring(a, b);
        assert(res.length == test_longest_common_substring_length(a, b));
        if (res.length > 0) assert(a.compare(res.pos_a, res.length, b, res.pos_b, res.length) == 0);
    }

    std::vector<std::vector<std::string>> multi_fixed = {
        {}, {""}, {"", ""}, {"abc", "bcd", "cde"}, {"banana", "ananas", "nana"}, {"aaaa", "aa", "aaa"}
    };
    for (auto ss : multi_fixed) {
        for (int k = 0; k <= int(ss.size()) + 1; k++) {
            assert(sa_longest_common_substring_at_least_k(ss, k) == test_longest_common_substring_at_least_k(ss, k));
        }
        assert(sa_longest_common_substring_all(ss) == test_longest_common_substring_at_least_k(ss, int(ss.size())));
    }

    for (int tc = 0; tc < 150; tc++) {
        int m = test_rand_int(rng, 5);
        std::vector<std::string> ss(m);
        for (int id = 0; id < m; id++) {
            int n = test_rand_int(rng, 7);
            for (int i = 0; i < n; i++) ss[id].push_back(char('a' + test_rand_int(rng, 4)));
        }
        for (int k = 0; k <= m + 1; k++) {
            assert(sa_longest_common_substring_at_least_k(ss, k) == test_longest_common_substring_at_least_k(ss, k));
        }
        assert(sa_longest_common_substring_all(ss) == test_longest_common_substring_at_least_k(ss, m));
    }
}

static void test_integer_arrays() {
    std::mt19937 rng(998244353);
    std::vector<std::vector<int>> fixed = {
        {}, {0}, {1, 1, 1}, {0, 1, 2}, {1, 2, 1, 2, 1}, {3, -1, 3, -1, 3}
    };

    for (auto a : fixed) {
        std::vector<int> vals = a;
        std::sort(vals.begin(), vals.end());
        vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
        std::vector<int> comp(a.size());
        for (int i = 0; i < int(a.size()); i++) {
            comp[i] = int(std::lower_bound(vals.begin(), vals.end(), a[i]) - vals.begin());
        }
        int upper = std::max(0, int(vals.size()) - 1);
        assert(sa_distinct_subarray_count(comp, upper) == test_distinct_subarray_count(comp));
        assert(sa_longest_repeated_subarray(a).length == test_longest_repeated_subarray_length(a));
    }

    for (int tc = 0; tc < 250; tc++) {
        int n = test_rand_int(rng, 9);
        int upper = test_rand_int(rng, 5);
        std::vector<int> a(n);
        for (int i = 0; i < n; i++) a[i] = test_rand_int(rng, upper + 1);
        assert(sa_distinct_subarray_count(a, upper) == test_distinct_subarray_count(a));
        assert(sa_longest_repeated_subarray(a).length == test_longest_repeated_subarray_length(a));

        int m = test_rand_int(rng, 9);
        std::vector<int> b(m);
        for (int i = 0; i < m; i++) b[i] = test_rand_int(rng, 7) - 3;
        std::vector<int> c(n);
        for (int i = 0; i < n; i++) c[i] = test_rand_int(rng, 7) - 3;
        auto res = sa_longest_common_subarray(c, b);
        assert(res.length == test_longest_common_subarray_length(c, b));
        if (res.length > 0) {
            for (int t = 0; t < res.length; t++) assert(c[res.pos_a + t] == b[res.pos_b + t]);
        }
    }
}

int main() {
    test_fixed_cases();
    std::cout << "fixed string tests: OK\n";

    test_random_strings();
    std::cout << "random string tests: OK\n";

    test_common_strings();
    std::cout << "common substring tests: OK\n";

    test_integer_arrays();
    std::cout << "integer array tests: OK\n";

    std::cout << "all suffix_array_solvers_v03 tests passed\n";
    return 0;
}

#endif
