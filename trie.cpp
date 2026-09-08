#include <bits/stdc++.h>
// #include <atcoder/all>
// using namespace atcoder;
#ifdef LOCAL
#include <debug.h>
#define dbg(...) debug::debug(#__VA_ARGS__, __VA_ARGS__)
#else
#define dbg(...) void(0)
#endif
using namespace std;

/*===================== ここから――Trie 本体は触らない =====================*/
// トライ木
template <int char_size, int margin> struct Trie {
    struct Node {
        int nxt[char_size];
        int exist;
        vector<int> accept;
        Node() : exist(0) { memset(nxt, -1, sizeof(nxt)); }
    };

    vector<Node> node_buf;
    int max_id;
    int root;
    Trie() : max_id(0), root(max_id) { node_buf.push_back(Node()); }

    void add(const string& str, int str_idx, int node_idx, int id) {
        // ※addでは、node_bufの参照を持ってはいけない（node_buf.push_backするため）
        ++node_buf[node_idx].exist;

        if (str_idx == (int)str.size()) {
            node_buf[node_idx].accept.push_back(id);
        } else {
            const size_t c = str[str_idx] - margin;
            assert(c < char_size);
            if (node_buf[node_idx].nxt[c] == -1) {
                node_buf[node_idx].nxt[c] = (int)node_buf.size();
                node_buf.push_back(Node());
            }
            add(str, str_idx + 1, node_buf[node_idx].nxt[c], id);
        }
    }

    // 文字列を木に追加（idを指定）
    // O(strの長さ)
    void add(const string& str, int id) { add(str, 0, root, id); }

    // 文字列を木に追加（追加した順にidが採番される）
    // O(strの長さ)
    void add(const string& str) { add(str, ++max_id); }

    // 木から対象の文字列（または、その文字列で始まる全ての文字列）を削除する
    // ※削除時にノードの再利用はしていない
    // O(strの長さ + 完全一致文字列の数)
    // str: 削除対象文字列
    // for_sub_tree: trueなら、strで始まる全ての文字列を木から削除する
    int remove(const string& str, bool for_sub_tree, int str_idx, int node_idx) {
        auto& node = node_buf[node_idx];
        if (str_idx == (int)str.size()) {
            if (for_sub_tree) {
                auto ret = node.exist;
                node.accept.clear();
                node.exist = 0;
                memset(node.nxt, -1, sizeof(node.nxt));
                return ret;
            } else {
                auto ret = (int)node.accept.size();
                node.accept.clear();
                node.exist -= ret;
                return ret;
            }
        } else {
            const size_t c = str[str_idx] - margin;
            assert(c < char_size);
            if (node.nxt[c] == -1) return 0;
            auto ret = remove(str, for_sub_tree, str_idx + 1, node.nxt[c]);
            if (node_buf[node.nxt[c]].exist <= 0) node.nxt[c] = -1;
            node.exist -= ret;
            return ret;
        }
    }
    int remove(const string& str, bool for_sub_tree) { return remove(str, for_sub_tree, 0, root); }

    // 木の中で、strの prefixとなる文字列 のidを全て検索し、引数 f(int 見つかった文字列id, bool 完全一致ならtrue) に通知
    // O(strの長さ + 見つかった文字列の数)
    template <class F> void query_all_prefixes_of(const string& str, const F& f, int str_idx, int node_idx) {
        auto& node = node_buf[node_idx];
        auto exact_match = str_idx == (int)str.size();
        for (auto& idx : node.accept) f(idx, exact_match);
        if (str_idx == (int)str.size()) return;

        const size_t c = str[str_idx] - margin;
        assert(c < char_size);
        if (node.nxt[c] == -1) return;

        query_all_prefixes_of(str, f, str_idx + 1, node.nxt[c]);
    }
    template <class F> void query_all_prefixes_of(const string& str, const F& f) {
        query_all_prefixes_of(str, f, 0, root);
    }

    // 木が対象の文字列のprefixになる文字列を含むかチェック
    // O(strの長さ)
    bool contains_a_prefix_of(const string& str, int str_idx, int node_idx) const {
        auto& node = node_buf[node_idx];
        if (!node.accept.empty()) return true;
        if (str_idx == (int)str.size()) {
            return false;
        } else {
            const size_t c = str[str_idx] - margin;
            assert(c < char_size);
            if (node.nxt[c] == -1) return false;
            return contains_a_prefix_of(str, str_idx + 1, node.nxt[c]);
        }
    }
    bool contains_a_prefix_of(const string& str) const { return contains_a_prefix_of(str, 0, root); }

    // 木（またはサブ木）に含まれる全ての文字列をアルファベット順に 引数f に通知する
    // O(追加した文字列の長さの合計)
    template <class F> void enumerate_strs(const F& f, int node_idx, string& s) const {
        auto& node = node_buf[node_idx];
        if (!node.accept.empty()) f(s);
        for (size_t c = 0; c < char_size; c++) {
            if (node.nxt[c] != -1) {
                s.push_back(char(c + margin));
                enumerate_strs(f, node.nxt[c], s);
                s.pop_back();
            }
        }
    }
    template <class F> void enumerate_strs(const F& f, int node_idx) const { string s; enumerate_strs(f, node_idx, s); }
    template <class F> void enumerate_strs(const F& f) const { string s; enumerate_strs(f, root, s); }

    // strに該当するノードのインデックスを返す。見つからなかった場合は-1を返す
    // O(strの長さ)
    int find_a_node(const string& str, int str_idx, int node_idx) const {
        auto& node = node_buf[node_idx];
        if (str_idx == (int)str.size()) {
            return node_idx;
        } else {
            const size_t c = str[str_idx] - margin;
            assert(c < char_size);
            if (node.nxt[c] == -1) return -1;
            return find_a_node(str, str_idx + 1, node.nxt[c]);
        }
    }
    int find_a_node(const string& str) const { return find_a_node(str, 0, root); }

    // 木（またはサブ木）に含まれる全ての文字列id 引数f に通知する
    // O(追加した文字列の長さの合計)
    template <class F> void enumerate(const F& f, int node_idx) {
        auto& node = node_buf[node_idx];
        for (auto& idx : node.accept) f(idx);
        for (size_t c = 0; c < char_size; c++) {
            if (node.nxt[c] != -1) {
                enumerate(f, node.nxt[c]);
            }
        }
    }
    template <class F> void enumerate(const F& f) { enumerate(f, root); }

    // 木に含まれる文字列数 O(1)
    int count() const { return (node_buf[root].exist); }

    // 木のノード数（メモリ使用量）  O(1)
    int size() const { return ((int)node_buf.size()); }

    // （デバッグ用）木構造を見やすい形に整形
    void format(ostream& os, int node_idx, int indent) const {
        auto& node = node_buf[node_idx];
        os << "(" << node.accept.size() << ") ";
        if (indent > 80) {
            os << " ...";
            return;
        }
        indent += 4;
        bool first = true;
        for (size_t c = 0; c < char_size; c++) {
            if (node.nxt[c] != -1) {
                if (!first) {
                    os << endl;
                    for (int i = 0; i < indent; i++) os << ' ';
                }
                first = false;
                os << char(c + margin);
                format(os, node.nxt[c], indent + 1);
            }
        }
        indent -= 4;

        if (indent == 0) os << endl;
    }
    void format(ostream& os) const { format(os, root, 0); }
};
template <int char_size, int margin> ostream& operator<<(ostream& os, const Trie<char_size, margin>& t) {
    os << "count: " << t.count() << endl;
    t.format(os);
    return os;
}
/*===================== ここまで――Trie 本体は触らない =====================*/

/*---------- Naive 参照実装（検証用） ----------*/
struct NaiveTrie {
    int max_id = 0;
    unordered_map<string, vector<int>> mp;  // 文字列 → id 群

    void add(const string& s, int id) {
        mp[s].push_back(id);
        max_id = max(max_id, id);
    }
    void add(const string& s) { add(s, ++max_id); }

    int remove(const string& s, bool subtree) {
        int removed = 0;
        if (subtree) {
            vector<string> del;
            for (auto& [k, v] : mp) {
                if (k.rfind(s, 0) == 0) {  // prefix
                    removed += (int)v.size();
                    del.push_back(k);
                }
            }
            for (auto& k : del) mp.erase(k);
        } else {
            auto it = mp.find(s);
            if (it != mp.end()) {
                removed = (int)it->second.size();
                mp.erase(it);
            }
        }
        return removed;
    }

    template <class F> void query_all_prefixes_of(const string& str, const F& f) const {
        for (auto& [k, v] : mp) {
            if (str.rfind(k, 0) == 0) {  // k is prefix of str
                bool exact = (k.size() == str.size());
                for (int id : v) f(id, exact);
            }
        }
    }

    bool contains_a_prefix_of(const string& str) const {
        for (auto& [k, v] : mp) {
            if (str.rfind(k, 0) == 0) return true;
        }
        return false;
    }

    template <class F> void enumerate_strs(const F& f) const {
        vector<string> v;
        for (auto& [k, _] : mp) v.push_back(k);
        sort(v.begin(), v.end());
        for (auto& s : v) f(s);
    }
    int count() const {
        int c = 0;
        for (auto& [k, v] : mp) c += (int)v.size();
        return c;
    }
};

/*---------- 共通ユーティリティ ----------*/
using Trie26 = Trie<26, 'a'>;

static mt19937_64 rng(20250428);

string random_str(int max_len = 10) {
    uniform_int_distribution<int> lenDist(0, max_len);
    int len = lenDist(rng);
    string s(len, 'a');
    for (int i = 0; i < len; ++i) {
        s[i] = char('a' + (rng() % 26));
    }
    return s;
}

// 数字 → 26 進英小文字列（例: 0→"a", 1→"b", 26→"ba"...）
string num_to_letters(int x) {
    string t;
    do {
        t.push_back(char('a' + (x % 26)));
        x /= 26;
    } while (x);
    reverse(t.begin(), t.end());
    return t;
}

/*======================================================================
    1. メソッドごとの網羅的テスト
 ======================================================================*/
void test_method_add_and_count() {
    Trie26 tr;
    assert(tr.count() == 0);
    tr.add("a", 1);
    tr.add("ab", 2);
    tr.add("a", 3);               // 重複追加
    assert(tr.count() == 3);
}

void test_method_remove() {
    Trie26 tr;
    tr.add("abc", 1);
    tr.add("abcd", 2);
    tr.add("abcde", 3);
    assert(tr.remove("abc", false) == 1);         // 完全一致のみ削除
    assert(tr.count() == 2);
    assert(tr.remove("abc", true) == 2);          // prefix 全削除
    assert(tr.count() == 0);
    assert(tr.remove("zzz", false) == 0);         // 存在しない
}

void test_method_prefix_queries() {
    Trie26 tr;
    tr.add("a");
    tr.add("abc");
    vector<pair<int,bool>> got;
    tr.query_all_prefixes_of("abcd", [&](int id, bool exact){ got.emplace_back(id, exact); });
    // "a" と "abc" の2つが prefix
    assert(got.size() == 2);
    assert(tr.contains_a_prefix_of("abcd"));
    assert(!tr.contains_a_prefix_of("zzz"));
}

void test_method_enumerate() {
    Trie26 tr;
    vector<string> expected = { "a", "ab", "b" };
    for (auto& s : expected) tr.add(s);
    vector<string> got;
    tr.enumerate_strs([&](const string& s){ got.push_back(s); });
    assert(got == expected);   // 期待どおり lex 順
}

/*======================================================================
    2. エッジケーステスト
 ======================================================================*/
void test_edge_cases() {
    Trie26 tr;
    // 空文字列の取扱い
    tr.add("");
    assert(tr.count() == 1);
    assert(tr.contains_a_prefix_of("anything"));
    assert(tr.remove("", false) == 1);

    // 境界文字 'z'
    tr.add("zaza", 10);
    assert(tr.count() == 1);
    assert(tr.remove("z", true) == 1);

    // ノード再利用しないことの確認（size 増加）
    int before = tr.size();
    tr.add("reuse");
    assert(tr.size() > before);
}

/*======================================================================
    3. 10 パターンのシナリオテスト
 ======================================================================*/
void scenario_simple_chain() {
    Trie26 tr;
    tr.add("a");
    tr.add("ab");
    tr.add("abc");
    assert(tr.count() == 3);
    assert(tr.remove("ab", true) == 2);
    assert(tr.count() == 1);
}

void scenario_duplicate_ids() {
    Trie26 tr;
    tr.add("x", 1);
    tr.add("x", 2);
    tr.add("x", 3);
    assert(tr.count() == 3);
    assert(tr.remove("x", false) == 3);
    assert(tr.count() == 0);
}

void scenario_alternating_add_remove() {
    Trie26 tr;
    for (int i = 0; i < 100; ++i) {
        string s = "k" + num_to_letters(i);   // ← 数字を英小文字列に変換
        tr.add(s);
    }
    for (int i = 0; i < 100; i += 2) {
        string s = "k" + num_to_letters(i);
        tr.remove(s, false);
    }
    assert(tr.count() == 50);
}

void scenario_enumerate_order() {
    Trie26 tr;
    vector<string> vs = {"d", "c", "b", "a"};
    for (auto& s: vs) tr.add(s);
    vector<string> out;
    tr.enumerate_strs([&](auto& s){ out.push_back(s); });
    sort(vs.begin(), vs.end());
    assert(out == vs);
}

void scenario_large_id_values() {
    Trie26 tr;
    const int M = 10000;
    for (int i = 1; i <= M; ++i) {
        string s = "id" + num_to_letters(i);      // ← 数字を英小文字列に変換
        tr.add(s, 1'000'000 + i);
    }
    assert(tr.count() == M);
}

void scenario_subtree_removal_does_not_affect_others() {
    Trie26 tr;
    tr.add("abc");
    tr.add("abcd");
    tr.add("xyz");
    tr.remove("abc", true); // abc* を削除
    assert(tr.contains_a_prefix_of("xyz"));
    assert(tr.count() == 1);
}

void scenario_find_node() {
    Trie26 tr;
    tr.add("findme");
    assert(tr.find_a_node("findme") != -1);
    assert(tr.find_a_node("notfound") == -1);
}

void scenario_empty_trie_behaviour() {
    Trie26 tr;
    assert(!tr.contains_a_prefix_of("anything"));
    assert(tr.remove("nothing", false) == 0);
    assert(tr.find_a_node("none") == -1);
}

void scenario_long_strings() {
    Trie26 tr;
    string s(1000, 'a');
    tr.add(s);
    assert(tr.contains_a_prefix_of(string(1500,'a')));
    assert(tr.remove(s, false)==1);
}

void scenario_single_character_alphabet() {
    // char_size = 1 の特殊トライ木
    Trie<1,'x'> t1;
    t1.add("xxx");
    assert(t1.count()==1);
    assert(t1.contains_a_prefix_of("xxxxxxxx"));
}
/*======================================================================
    4. ランダム大量データテスト  (Trie vs Naive)
 ======================================================================*/
void random_stress_test(int ops, int max_len) {
    Trie26 tr;
    NaiveTrie nv;
    int next_id = 1;

    uniform_int_distribution<int> opDist(0,4);
    for (int i = 0; i < ops; ++i) {
        if((i % 100000) == 0) dbg(i);
        int op = opDist(rng);
        string s = random_str(max_len);
        switch (op) {
            case 0: {          // add
                tr.add(s, next_id);
                nv.add(s, next_id);
                ++next_id;
                break;
            }
            case 1: {          // remove exact
                int a = tr.remove(s,false);
                int b = nv.remove(s,false);
                assert(a==b);
                break;
            }
            case 2: {          // remove subtree
                int a = tr.remove(s,true);
                int b = nv.remove(s,true);
                assert(a==b);
                break;
            }
            case 3: {          // contains_a_prefix_of
                bool a = tr.contains_a_prefix_of(s);
                bool b = nv.contains_a_prefix_of(s);
                assert(a==b);
                break;
            }
            case 4: {          // query_all_prefixes_of
                vector<pair<int,bool>> A,B;
                tr.query_all_prefixes_of(s,[&](int id,bool ex){A.emplace_back(id,ex);});
                nv.query_all_prefixes_of(s,[&](int id,bool ex){B.emplace_back(id,ex);});
                sort(A.begin(),A.end());
                sort(B.begin(),B.end());
                assert(A==B);
                break;
            }
        }
        if ((i&1023)==0 || i == ops - 1) {     // 定期整合性チェック
            vector<string> a,b;
            tr.enumerate_strs([&](auto& t){a.push_back(t);});
            nv.enumerate_strs([&](auto& t){b.push_back(t);});
            assert(a==b);
            assert(tr.count()==nv.count());
        }
    }
}

/*======================================================================
    メイン
 ======================================================================*/
int main() {
    /* 1. メソッド網羅テスト */
    test_method_add_and_count();
    test_method_remove();
    test_method_prefix_queries();
    test_method_enumerate();

    /* 2. エッジケース */
    test_edge_cases();

    /* 3. 10 シナリオ */
    scenario_simple_chain();
    scenario_duplicate_ids();
    scenario_alternating_add_remove();
    scenario_enumerate_order();
    scenario_large_id_values();
    scenario_subtree_removal_does_not_affect_others();
    scenario_find_node();
    scenario_empty_trie_behaviour();
    scenario_long_strings();
    scenario_single_character_alphabet();

    /* 4. ランダム大量データ比較 */
    random_stress_test(1000000, 1);
    random_stress_test(1000000, 2);
    random_stress_test(1000000, 10);
    random_stress_test(1000000, 100);
    random_stress_test(100000, 1000);
    random_stress_test(5000, 10000);

    cout << "All tests passed ✔" << endl;
    return 0;
}
