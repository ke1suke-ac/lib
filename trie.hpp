#pragma once
#include <bits/stdc++.h>
#include "hash_map.hpp"
using namespace std;

/*
  Trie系データ構造のライブラリ
  文字列prefix集合、複数方式の疎な列prefix集合、整数bit集合、xor遅延集合、xor mex集合、永続Binary Trie、マージ可能Binary Trieを扱う
*/

/* ============================================================
 * Trie
 * ============================================================ */

template<int K = 26, int BASE = 'a'>
struct Trie {
    static_assert(K > 0, "K must be positive");

    struct Node {
        array<int, K> next;
        int pass;
        int end;

        Node() : next(), pass(0), end(0) {
            next.fill(-1);
        }
    };

    vector<Node> nodes;
    int total;

    // 空のTrieを構築する O(1)
    Trie() : nodes(1), total(0) {}

    // 登録文字列数を重複込みで返す O(1)
    int size() const {
        return total;
    }

    // 文字列sに対応するノード番号を返し、存在しなければ-1を返す O(|s|)
    int find_node(string_view s) const {
        int vertex = 0;
        for (const char ch : s) {
            const int index = char_index(ch);
            assert(0 <= index && index < K);
            const int to = nodes[vertex].next[index];
            if (to == -1) {
                return -1;
            }
            vertex = to;
        }
        return vertex;
    }

    // 文字列sを1個追加し、終端ノード番号を返す O(|s|)
    int insert(string_view s) {
        int vertex = 0;
        ++nodes[vertex].pass;

        // 文字を順に辿り、存在しない辺はその場で作る
        for (const char ch : s) {
            const int index = char_index(ch);
            assert(0 <= index && index < K);
            int to = nodes[vertex].next[index];
            if (to == -1) {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next[index] = to;
                nodes.emplace_back();
            }
            vertex = to;
            ++nodes[vertex].pass;
        }

        ++nodes[vertex].end;
        ++total;
        return vertex;
    }

    // 文字列sを1個削除し、削除できたかを返す O(|s|)
    bool erase(string_view s) {
        if (count(s) == 0) {
            return false;
        }

        int vertex = 0;
        --nodes[vertex].pass;

        // ノード自体は残し、通過数と終端数だけを減らす
        for (const char ch : s) {
            const int index = char_index(ch);
            assert(0 <= index && index < K);
            vertex = nodes[vertex].next[index];
            --nodes[vertex].pass;
        }

        --nodes[vertex].end;
        --total;
        return true;
    }

    // 文字列sの登録個数を返す O(|s|)
    int count(string_view s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].end;
    }

    // 文字列sが1個以上登録されているかを返す O(|s|)
    bool contains(string_view s) const {
        return count(s) > 0;
    }

    // prefixをsに持つ登録文字列数を返す O(|s|)
    int prefix_count(string_view s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].pass;
    }

    // 登録文字列のうちsのprefixであるものの個数を返す O(|s|)
    int count_prefixes(string_view s) const {
        int vertex = 0;
        int result = nodes[vertex].end;

        // 辿れる限り進み、各prefix終端の個数を足す
        for (const char ch : s) {
            const int index = char_index(ch);
            assert(0 <= index && index < K);
            const int to = nodes[vertex].next[index];
            if (to == -1) {
                return result;
            }
            vertex = to;
            result += nodes[vertex].end;
        }

        return result;
    }

    // 辞書順でk番目の文字列を0-indexedで返す O(KL)
    string kth(int k) const {
        assert(0 <= k && k < total);
        string result;
        int vertex = 0;

        // 各ノードで終端を最初に見て、その後に子を文字順に見る
        while (true) {
            if (k < nodes[vertex].end) {
                return result;
            }
            k -= nodes[vertex].end;

            bool found = false;
            for (int index = 0; index < K; ++index) {
                const int to = nodes[vertex].next[index];
                if (to == -1 || nodes[to].pass == 0) {
                    continue;
                }
                if (k < nodes[to].pass) {
                    result.push_back(static_cast<char>(BASE + index));
                    vertex = to;
                    found = true;
                    break;
                }
                k -= nodes[to].pass;
            }
            assert(found);
        }
    }

    // 登録文字列のうちsより辞書順で小さいものの個数を返す O(K|s|)
    int count_less(string_view s) const {
        int vertex = 0;
        int result = 0;

        // 現在位置で終わる短い文字列と、次文字が小さい部分木を足す
        for (const char ch : s) {
            result += nodes[vertex].end;
            const int index = char_index(ch);
            assert(0 <= index && index < K);
            for (int small = 0; small < index; ++small) {
                const int to = nodes[vertex].next[small];
                if (to != -1) {
                    result += nodes[to].pass;
                }
            }
            const int to = nodes[vertex].next[index];
            if (to == -1) {
                return result;
            }
            vertex = to;
        }

        return result;
    }

private:
    static int char_index(char ch) {
        return ch - BASE;
    }
};

/* ------------------------------------------------------------
 * Trieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

// 想定ユースケース:
//   固定辞書に対し、各文字列をprefixに持つ登録文字列数をまとめて求める
// 入力:
//   words: [BASE, BASE + K)の文字だけからなる登録文字列列、重複を許す
//   queries: 問い合わせるprefix列
// 出力:
//   result[i] = queries[i]をprefixに持つwords内の文字列数
// 処理量:
//   S = wordsの総文字数、T = queriesの総文字数、M = queries.size()として
//   時間 O(S + T)、追加メモリ O(KS + M)
template<int K = 26, int BASE = 'a'>
vector<int> trie_prefix_count_queries(const vector<string>& words, const vector<string>& queries) {
    Trie<K, BASE> trie;
    for (const string& word : words) {
        trie.insert(word);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const string& query : queries) {
        result.push_back(trie.prefix_count(query));
    }
    return result;
}

// 想定ユースケース:
//   固定辞書に対し、各文字列が完全一致で何個登録されているかをまとめて求める
// 入力:
//   words: [BASE, BASE + K)の文字だけからなる登録文字列列、重複を許す
//   queries: 登録個数を問い合わせる文字列列
// 出力:
//   result[i] = words内におけるqueries[i]の登録個数
// 処理量:
//   S = wordsの総文字数、T = queriesの総文字数、M = queries.size()として
//   時間 O(S + T)、追加メモリ O(KS + M)
template<int K = 26, int BASE = 'a'>
vector<int> trie_word_count_queries(const vector<string>& words, const vector<string>& queries) {
    Trie<K, BASE> trie;
    for (const string& word : words) {
        trie.insert(word);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const string& query : queries) {
        result.push_back(trie.count(query));
    }
    return result;
}

// 想定ユースケース:
//   文字列列の組のうち、片方がもう片方のprefixである組数を求める
// 入力:
//   words: [BASE, BASE + K)の文字だけからなる文字列列、重複を許す
// 出力:
//   0 <= i < j < words.size()かつwords[i]とwords[j]の片方が他方のprefixである組数
//   同一文字列同士の組は1組として数える
// 処理量:
//   S = wordsの総文字数として、時間 O(S)、追加メモリ O(KS)
template<int K = 26, int BASE = 'a'>
long long trie_count_prefix_pairs(const vector<string>& words) {
    Trie<K, BASE> trie;
    long long result = 0LL;
    for (const string& word : words) {
        const int prefixes = trie.count_prefixes(word);
        const int extensions = trie.prefix_count(word);
        const int same = trie.count(word);
        result += 1LL * prefixes + extensions - same;
        trie.insert(word);
    }
    return result;
}

// 想定ユースケース:
//   重複を含む固定辞書から、辞書順で指定順位の文字列をまとめて取り出す
// 入力:
//   words: [BASE, BASE + K)の文字だけからなる登録文字列列、重複を許す
//   ks: 0-indexedの順位列、各kは0 <= k < words.size()を満たす
// 出力:
//   result[i] = wordsを重複込みで辞書順に並べたときのks[i]番目の文字列
// 処理量:
//   S = wordsの総文字数、M = ks.size()、L = words内の最大文字列長として
//   時間 O(S + MKL)、返却値を含む追加メモリ O(KS + ML)
template<int K = 26, int BASE = 'a'>
vector<string> trie_kth_strings(const vector<string>& words, const vector<int>& ks) {
    Trie<K, BASE> trie;
    for (const string& word : words) {
        trie.insert(word);
    }
    vector<string> result;
    result.reserve(ks.size());
    for (const int k : ks) {
        result.push_back(trie.kth(k));
    }
    return result;
}

// 想定ユースケース:
//   固定辞書に対し、各文字列の辞書順rankをまとめて求める
// 入力:
//   words: [BASE, BASE + K)の文字だけからなる登録文字列列、重複を許す
//   queries: rankを問い合わせる文字列列
// 出力:
//   result[i] = words内でqueries[i]より辞書順で小さい文字列数、重複も個別に数える
// 処理量:
//   S = wordsの総文字数、T = queriesの総文字数、M = queries.size()として
//   時間 O(S + KT)、追加メモリ O(KS + M)
template<int K = 26, int BASE = 'a'>
vector<int> trie_rank_queries(const vector<string>& words, const vector<string>& queries) {
    Trie<K, BASE> trie;
    for (const string& word : words) {
        trie.insert(word);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const string& query : queries) {
        result.push_back(trie.count_less(query));
    }
    return result;
}

/* ============================================================
 * LinearSparseTrie
 * ============================================================ */

template<class Key = int>
struct LinearSparseTrie {
    struct Node {
        vector<pair<Key, int>> next;
        int pass;
        int end;

        Node() : next(), pass(0), end(0) {}
    };

    vector<Node> nodes;
    int total;

    // 空のLinearSparseTrieを構築する O(1)
    LinearSparseTrie() : nodes(1), total(0) {}

    // 登録列数を重複込みで返す O(1)
    int size() const {
        return total;
    }

    // 列sに対応するノード番号を返し、存在しなければ-1を返す O(|s|deg)
    template<class Range>
    int find_node(const Range& s) const {
        int vertex = 0;
        for (const Key& key : s) {
            const int to = find_child(vertex, key);
            if (to == -1) {
                return -1;
            }
            vertex = to;
        }
        return vertex;
    }

    // 列sを1個追加し、終端ノード番号を返す O(|s|deg)
    template<class Range>
    int insert(const Range& s) {
        int vertex = 0;
        ++nodes[vertex].pass;

        // 各ノードの子は少数を想定し、線形探索で探す
        for (const Key& key : s) {
            int to = find_child(vertex, key);
            if (to == -1) {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next.push_back({key, to});
                nodes.emplace_back();
            }
            vertex = to;
            ++nodes[vertex].pass;
        }

        ++nodes[vertex].end;
        ++total;
        return vertex;
    }

    // 列sを1個削除し、削除できたかを返す O(|s|deg)
    template<class Range>
    bool erase(const Range& s) {
        if (count(s) == 0) {
            return false;
        }

        int vertex = 0;
        --nodes[vertex].pass;

        // ノードと辺は残し、通過数と終端数だけを減らす
        for (const Key& key : s) {
            vertex = find_child(vertex, key);
            --nodes[vertex].pass;
        }

        --nodes[vertex].end;
        --total;
        return true;
    }

    // 列sの登録個数を返す O(|s|deg)
    template<class Range>
    int count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].end;
    }

    // 列sが1個以上登録されているかを返す O(|s|deg)
    template<class Range>
    bool contains(const Range& s) const {
        return count(s) > 0;
    }

    // prefixをsに持つ登録列数を返す O(|s|deg)
    template<class Range>
    int prefix_count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].pass;
    }

private:
    int find_child(int vertex, const Key& key) const {
        for (const pair<Key, int>& edge : nodes[vertex].next) {
            if (edge.first == key) {
                return edge.second;
            }
        }
        return -1;
    }
};

/* ------------------------------------------------------------
 * LinearSparseTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

// 想定ユースケース:
//   Keyの値域は大きいが各ノードの分岐数が小さい列集合に対し、prefix個数を求める
// 入力:
//   sequences: 登録するKey列、重複を許す
//   queries: 問い合わせるprefix列
// 出力:
//   result[i] = queries[i]をprefixに持つsequences内の列数
// 処理量:
//   S = sequencesの総要素数、T = queriesの総要素数、D = Trieノードの最大分岐数として
//   時間 O((S + T)D)、追加メモリ O(S + queries.size())
template<class Key = int>
vector<int> linear_sparse_trie_prefix_count_queries(const vector<vector<Key>>& sequences, const vector<vector<Key>>& queries) {
    LinearSparseTrie<Key> trie;
    for (const vector<Key>& sequence : sequences) {
        trie.insert(sequence);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const vector<Key>& query : queries) {
        result.push_back(trie.prefix_count(query));
    }
    return result;
}

/* ============================================================
 * SparseTrie
 * ============================================================ */

template<class Key = int>
struct SparseTrie {
    struct Node {
        vector<pair<Key, int>> next;
        int pass;
        int end;

        Node() : next(), pass(0), end(0) {}
    };

    vector<Node> nodes;
    int total;

    // 空のSparseTrieを構築する O(1)
    SparseTrie() : nodes(1), total(0) {}

    // 登録列数を重複込みで返す O(1)
    int size() const {
        return total;
    }

    // 列sに対応するノード番号を返し、存在しなければ-1を返す O(|s|log deg)
    template<class Range>
    int find_node(const Range& s) const {
        int vertex = 0;
        for (const Key& key : s) {
            const int to = find_child(vertex, key);
            if (to == -1) {
                return -1;
            }
            vertex = to;
        }
        return vertex;
    }

    // 列sを1個追加し、終端ノード番号を返す O(|s|deg)
    template<class Range>
    int insert(const Range& s) {
        int vertex = 0;
        ++nodes[vertex].pass;

        // 子をKey昇順に保ち、検索は二分探索で行う
        for (const Key& key : s) {
            const int pos = child_position(vertex, key);
            int to = -1;
            if (pos < static_cast<int>(nodes[vertex].next.size()) && nodes[vertex].next[pos].first == key) {
                to = nodes[vertex].next[pos].second;
            } else {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next.insert(nodes[vertex].next.begin() + pos, {key, to});
                nodes.emplace_back();
            }
            vertex = to;
            ++nodes[vertex].pass;
        }

        ++nodes[vertex].end;
        ++total;
        return vertex;
    }

    // 列sを1個削除し、削除できたかを返す O(|s|log deg)
    template<class Range>
    bool erase(const Range& s) {
        if (count(s) == 0) {
            return false;
        }

        int vertex = 0;
        --nodes[vertex].pass;

        // ノードと辺は残し、通過数と終端数だけを減らす
        for (const Key& key : s) {
            vertex = find_child(vertex, key);
            --nodes[vertex].pass;
        }

        --nodes[vertex].end;
        --total;
        return true;
    }

    // 列sの登録個数を返す O(|s|log deg)
    template<class Range>
    int count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].end;
    }

    // 列sが1個以上登録されているかを返す O(|s|log deg)
    template<class Range>
    bool contains(const Range& s) const {
        return count(s) > 0;
    }

    // prefixをsに持つ登録列数を返す O(|s|log deg)
    template<class Range>
    int prefix_count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].pass;
    }

private:
    int child_position(int vertex, const Key& key) const {
        const vector<pair<Key, int>>& edges = nodes[vertex].next;
        const auto it = lower_bound(edges.begin(), edges.end(), key, [](const pair<Key, int>& edge, const Key& value) {
            return edge.first < value;
        });
        return static_cast<int>(it - edges.begin());
    }

    int find_child(int vertex, const Key& key) const {
        const int pos = child_position(vertex, key);
        if (pos == static_cast<int>(nodes[vertex].next.size()) || !(nodes[vertex].next[pos].first == key)) {
            return -1;
        }
        return nodes[vertex].next[pos].second;
    }
};

/* ------------------------------------------------------------
 * SparseTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

// 想定ユースケース:
//   大きいKey値域と中程度以上の分岐数を持つ静的な列集合に対し、prefix個数を求める
//   子をKey順に保持するため、構築後に検索クエリが多い場合に向く
// 入力:
//   sequences: 登録するKey列、重複を許す、Keyは<と==で比較可能であること
//   queries: 問い合わせるprefix列
// 出力:
//   result[i] = queries[i]をprefixに持つsequences内の列数
// 処理量:
//   S = sequencesの総要素数、T = queriesの総要素数、D = Trieノードの最大分岐数として
//   構築はvector挿入を含むため最悪 O(SD)、検索は O(T log(D + 1))
//   合計時間 O(SD + T log(D + 1))、追加メモリ O(S + queries.size())
template<class Key = int>
vector<int> sparse_trie_prefix_count_queries(const vector<vector<Key>>& sequences, const vector<vector<Key>>& queries) {
    SparseTrie<Key> trie;
    for (const vector<Key>& sequence : sequences) {
        trie.insert(sequence);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const vector<Key>& query : queries) {
        result.push_back(trie.prefix_count(query));
    }
    return result;
}

/* ============================================================
 * UnorderedMapTrie
 * ============================================================ */

template<class Key = int>
struct UnorderedMapTrie {
    struct Node {
        unordered_map<Key, int> next;
        int pass;
        int end;

        Node() : next(), pass(0), end(0) {}
    };

    vector<Node> nodes;
    int total;

    // 空のUnorderedMapTrieを構築する O(1)
    UnorderedMapTrie() : nodes(1), total(0) {}

    // 登録列数を重複込みで返す O(1)
    int size() const {
        return total;
    }

    // 列sに対応するノード番号を返し、存在しなければ-1を返す 期待O(|s|)
    template<class Range>
    int find_node(const Range& s) const {
        int vertex = 0;
        for (const Key& key : s) {
            const int to = find_child(vertex, key);
            if (to == -1) {
                return -1;
            }
            vertex = to;
        }
        return vertex;
    }

    // 列sを1個追加し、終端ノード番号を返す 期待償却O(|s|)
    template<class Range>
    int insert(const Range& s) {
        int vertex = 0;
        ++nodes[vertex].pass;

        // 各ノードの子をunordered_mapで持ち、平均O(1)で遷移する
        for (const Key& key : s) {
            auto it = nodes[vertex].next.find(key);
            int to = -1;
            if (it == nodes[vertex].next.end()) {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next.emplace(key, to);
                nodes.emplace_back();
            } else {
                to = it->second;
            }
            vertex = to;
            ++nodes[vertex].pass;
        }

        ++nodes[vertex].end;
        ++total;
        return vertex;
    }

    // 列sを1個削除し、削除できたかを返す 期待O(|s|)
    template<class Range>
    bool erase(const Range& s) {
        if (count(s) == 0) {
            return false;
        }

        int vertex = 0;
        --nodes[vertex].pass;

        // ノードと辺は残し、通過数と終端数だけを減らす
        for (const Key& key : s) {
            vertex = find_child(vertex, key);
            --nodes[vertex].pass;
        }

        --nodes[vertex].end;
        --total;
        return true;
    }

    // 列sの登録個数を返す 期待O(|s|)
    template<class Range>
    int count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].end;
    }

    // 列sが1個以上登録されているかを返す 期待O(|s|)
    template<class Range>
    bool contains(const Range& s) const {
        return count(s) > 0;
    }

    // prefixをsに持つ登録列数を返す 期待O(|s|)
    template<class Range>
    int prefix_count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].pass;
    }

private:
    int find_child(int vertex, const Key& key) const {
        const auto it = nodes[vertex].next.find(key);
        if (it == nodes[vertex].next.end()) {
            return -1;
        }
        return it->second;
    }
};

/* ------------------------------------------------------------
 * UnorderedMapTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

// 想定ユースケース:
//   std::hashを利用できる大きいKey値域の列集合に対し、平均定数時間の遷移でprefix個数を求める
// 入力:
//   sequences: 登録するKey列、重複を許す
//   queries: 問い合わせるprefix列
// 出力:
//   result[i] = queries[i]をprefixに持つsequences内の列数
// 処理量:
//   S = sequencesの総要素数、T = queriesの総要素数として
//   期待時間 O(S + T)、追加メモリ O(S + queries.size())
//   hash衝突が偏る場合の最悪計算量は保証しない
template<class Key = int>
vector<int> unordered_map_trie_prefix_count_queries(const vector<vector<Key>>& sequences, const vector<vector<Key>>& queries) {
    UnorderedMapTrie<Key> trie;
    for (const vector<Key>& sequence : sequences) {
        trie.insert(sequence);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const vector<Key>& query : queries) {
        result.push_back(trie.prefix_count(query));
    }
    return result;
}

/* ============================================================
 * HashMapTrie
 * ============================================================ */

template<class Key = int>
struct HashMapTrie {
    struct Node {
        hash_map<Key, int> next;
        int pass;
        int end;

        Node() : next(), pass(0), end(0) {}
    };

    vector<Node> nodes;
    int total;

    // 空のHashMapTrieを構築する O(1)
    HashMapTrie() : nodes(1), total(0) {}

    // 登録列数を重複込みで返す O(1)
    int size() const {
        return total;
    }

    // 列sに対応するノード番号を返し、存在しなければ-1を返す 期待O(|s|)
    template<class Range>
    int find_node(const Range& s) const {
        int vertex = 0;
        for (const Key& key : s) {
            const int to = find_child(vertex, key);
            if (to == -1) {
                return -1;
            }
            vertex = to;
        }
        return vertex;
    }

    // 列sを1個追加し、終端ノード番号を返す 期待償却O(|s|)
    template<class Range>
    int insert(const Range& s) {
        int vertex = 0;
        ++nodes[vertex].pass;

        // プロジェクトのhash_mapで整数系Keyの遷移を平均O(1)で処理する
        for (const Key& key : s) {
            const int* found = nodes[vertex].next.find(key);
            int to = -1;
            if (found == nullptr) {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next.insert(key, to);
                nodes.emplace_back();
            } else {
                to = *found;
            }
            vertex = to;
            ++nodes[vertex].pass;
        }

        ++nodes[vertex].end;
        ++total;
        return vertex;
    }

    // 列sを1個削除し、削除できたかを返す 期待O(|s|)
    template<class Range>
    bool erase(const Range& s) {
        if (count(s) == 0) {
            return false;
        }

        int vertex = 0;
        --nodes[vertex].pass;

        // ノードと辺は残し、通過数と終端数だけを減らす
        for (const Key& key : s) {
            vertex = find_child(vertex, key);
            --nodes[vertex].pass;
        }

        --nodes[vertex].end;
        --total;
        return true;
    }

    // 列sの登録個数を返す 期待O(|s|)
    template<class Range>
    int count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].end;
    }

    // 列sが1個以上登録されているかを返す 期待O(|s|)
    template<class Range>
    bool contains(const Range& s) const {
        return count(s) > 0;
    }

    // prefixをsに持つ登録列数を返す 期待O(|s|)
    template<class Range>
    int prefix_count(const Range& s) const {
        const int vertex = find_node(s);
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].pass;
    }

private:
    int find_child(int vertex, const Key& key) const {
        const int* found = nodes[vertex].next.find(key);
        if (found == nullptr) {
            return -1;
        }
        return *found;
    }
};

/* ------------------------------------------------------------
 * HashMapTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

// 想定ユースケース:
//   プロジェクトのhash_mapが扱える整数系Keyの列集合に対し、軽量なhash遷移でprefix個数を求める
// 入力:
//   sequences: 登録するKey列、重複を許す
//   queries: 問い合わせるprefix列
//   Keyはbool以外の整数型、またはそれらを要素に持つpair / tupleであること
// 出力:
//   result[i] = queries[i]をprefixに持つsequences内の列数
// 処理量:
//   S = sequencesの総要素数、T = queriesの総要素数として
//   期待償却時間 O(S + T)、追加メモリ O(S + queries.size())
template<class Key = int>
vector<int> hash_map_trie_prefix_count_queries(const vector<vector<Key>>& sequences, const vector<vector<Key>>& queries) {
    HashMapTrie<Key> trie;
    for (const vector<Key>& sequence : sequences) {
        trie.insert(sequence);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const vector<Key>& query : queries) {
        result.push_back(trie.prefix_count(query));
    }
    return result;
}

/* ============================================================
 * BinaryTrie
 * ============================================================ */

template<int BIT = 60>
struct BinaryTrie {
    static_assert(0 <= BIT && BIT <= 64, "BIT must be in [0, 64]");

    struct Node {
        array<int, 2> next;
        int cnt;

        Node() : next{-1, -1}, cnt(0) {}
    };

    vector<Node> nodes;
    int total;

    // 空のBinaryTrieを構築する O(1)
    BinaryTrie() : nodes(1), total(0) {}

    // 登録整数数を重複込みで返す O(1)
    int size() const {
        return total;
    }

    // 整数xを1個追加する O(BIT)
    void insert(unsigned long long x) {
        int vertex = 0;
        ++nodes[vertex].cnt;

        // 上位bitから順に辿り、存在しないノードは作る
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            int to = nodes[vertex].next[b];
            if (to == -1) {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next[b] = to;
                nodes.emplace_back();
            }
            vertex = to;
            ++nodes[vertex].cnt;
        }

        ++total;
    }

    // 整数xを1個削除し、削除できたかを返す O(BIT)
    bool erase(unsigned long long x) {
        if (count(x) == 0) {
            return false;
        }

        int vertex = 0;
        --nodes[vertex].cnt;

        // ノードは残し、個数だけを減らす
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            vertex = nodes[vertex].next[b];
            --nodes[vertex].cnt;
        }

        --total;
        return true;
    }

    // 整数xの登録個数を返す O(BIT)
    int count(unsigned long long x) const {
        int vertex = 0;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            vertex = nodes[vertex].next[b];
            if (vertex == -1) {
                return 0;
            }
        }
        return nodes[vertex].cnt;
    }

    // 昇順でk番目の整数を0-indexedで返す O(BIT)
    unsigned long long kth(int k) const {
        assert(0 <= k && k < total);
        unsigned long long result = 0ULL;
        int vertex = 0;

        // 0側の個数でkが属する部分木を判定する
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int left = nodes[vertex].next[0];
            const int left_count = node_count(left);
            if (k < left_count) {
                vertex = left;
            } else {
                k -= left_count;
                result |= (1ULL << bit);
                vertex = nodes[vertex].next[1];
            }
        }

        return result;
    }

    // 登録整数のうちx未満の個数を返す O(BIT)
    int count_less(unsigned long long x) const {
        if constexpr (BIT < 64) {
            if ((x >> BIT) != 0ULL) {
                return total;
            }
        }

        int result = 0;
        int vertex = 0;

        // xのbitが1なら、その桁が0の部分木はすべてx未満になる
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (vertex == -1) {
                break;
            }
            const int b = bit_at(x, bit);
            if (b == 1) {
                result += node_count(nodes[vertex].next[0]);
                vertex = nodes[vertex].next[1];
            } else {
                vertex = nodes[vertex].next[0];
            }
        }

        return result;
    }

    // 登録整数yのうち(x xor y)がlimit未満の個数を返す O(BIT)
    int count_xor_less(unsigned long long x, unsigned long long limit) const {
        if constexpr (BIT < 64) {
            if ((limit >> BIT) != 0ULL) {
                return total;
            }
        }

        int result = 0;
        int vertex = 0;

        // limitのbitが1ならxor bitが0の部分木を足し、1の側へ進む
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (vertex == -1) {
                break;
            }
            const int xb = bit_at(x, bit);
            const int lb = bit_at(limit, bit);
            if (lb == 1) {
                result += node_count(nodes[vertex].next[xb]);
                vertex = nodes[vertex].next[1 - xb];
            } else {
                vertex = nodes[vertex].next[xb];
            }
        }

        return result;
    }

    // min(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long min_xor(unsigned long long x) const {
        assert(total > 0);
        unsigned long long result = 0ULL;
        int vertex = 0;

        // 同じbitの子を優先し、なければ反対側へ進む
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int same = nodes[vertex].next[b];
            if (node_count(same) > 0) {
                vertex = same;
            } else {
                result |= (1ULL << bit);
                vertex = nodes[vertex].next[1 - b];
            }
        }

        return result;
    }

    // max(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long max_xor(unsigned long long x) const {
        assert(total > 0);
        unsigned long long result = 0ULL;
        int vertex = 0;

        // 反対bitの子を優先し、なければ同じ側へ進む
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int opposite = nodes[vertex].next[1 - b];
            if (node_count(opposite) > 0) {
                result |= (1ULL << bit);
                vertex = opposite;
            } else {
                vertex = nodes[vertex].next[b];
            }
        }

        return result;
    }

    // 登録整数の最小値を返す O(BIT)
    unsigned long long min_element() const {
        return kth(0);
    }

    // 登録整数の最大値を返す O(BIT)
    unsigned long long max_element() const {
        return kth(total - 1);
    }

private:
    static int bit_at(unsigned long long x, int bit) {
        return (x >> bit) & 1ULL;
    }

    int node_count(int vertex) const {
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].cnt;
    }
};

/* ------------------------------------------------------------
 * BinaryTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

// 想定ユースケース:
//   整数列から異なる2要素を選んだときのxor最大値を求める
// 入力:
//   a: BIT bit以内の非負整数列、同じ値を複数含んでもよい
// 出力:
//   max(a[i] xor a[j])を返す、対象は0 <= i < j < a.size()
//   a.size() < 2なら0を返す
// 処理量:
//   N = a.size()として、時間 O(N BIT)、追加メモリ O(N BIT)
template<int BIT = 60>
unsigned long long binary_trie_max_pair_xor(const vector<unsigned long long>& a) {
    if (a.size() < 2U) {
        return 0ULL;
    }
    BinaryTrie<BIT> trie;
    trie.insert(a[0]);
    unsigned long long result = 0ULL;
    for (size_t index = 1; index < a.size(); ++index) {
        result = max(result, trie.max_xor(a[index]));
        trie.insert(a[index]);
    }
    return result;
}

// 想定ユースケース:
//   整数列の連続部分列xorの最大値を求める
// 入力:
//   a: BIT bit以内の非負整数列
// 出力:
//   0 <= l < r <= a.size()に対するa[l] xor ... xor a[r - 1]の最大値
//   aが空なら0を返す
// 処理量:
//   N = a.size()として、時間 O(N BIT)、追加メモリ O(N BIT)
template<int BIT = 60>
unsigned long long binary_trie_max_subarray_xor(const vector<unsigned long long>& a) {
    BinaryTrie<BIT> trie;
    trie.insert(0ULL);
    unsigned long long prefix = 0ULL;
    unsigned long long result = 0ULL;
    for (const unsigned long long value : a) {
        prefix ^= value;
        result = max(result, trie.max_xor(prefix));
        trie.insert(prefix);
    }
    return result;
}

// 想定ユースケース:
//   xorが指定値未満となる連続部分列の個数を求める
// 入力:
//   a: BIT bit以内の非負整数列
//   limit: 判定する上限値、厳密不等号で比較する
// 出力:
//   0 <= l < r <= a.size()かつ(a[l] xor ... xor a[r - 1]) < limitとなる組数
// 処理量:
//   N = a.size()として、時間 O(N BIT)、追加メモリ O(N BIT)
template<int BIT = 60>
long long binary_trie_count_subarray_xor_less(const vector<unsigned long long>& a, unsigned long long limit) {
    BinaryTrie<BIT> trie;
    trie.insert(0ULL);
    unsigned long long prefix = 0ULL;
    long long result = 0LL;
    for (const unsigned long long value : a) {
        prefix ^= value;
        result += trie.count_xor_less(prefix, limit);
        trie.insert(prefix);
    }
    return result;
}

struct BinaryTrieQuery {
    int type;
    unsigned long long x;
    unsigned long long y;
    int k;
};

// 想定ユースケース:
//   挿入・1個削除を伴う整数multisetに対し、順位・大小・xor系クエリをオンライン処理する
// 入力:
//   queries: 以下の操作列、値はBIT bit以内とする
//   type=0: xをinsert、出力なし
//   type=1: xを1個eraseし、成功なら1、存在しなければ0を出力
//   type=2: count(x)を出力
//   type=3: 0-indexedのkth(k)を出力
//   type=4: count_less(x)を出力
//   type=5: min(x xor v)を出力
//   type=6: max(x xor v)を出力
//   type=7: 最小要素を出力
//   type=8: 最大要素を出力
//   type=9: (x xor v) < yとなる要素数を出力
// 出力:
//   出力を持つ操作の答えを、queriesに現れる順に格納した列
//   type=3のkとtype=5から8は、呼び出し時に有効な順位または非空集合であること
// 処理量:
//   Q = queries.size()、I = insert操作数、R = 出力数として
//   時間 O(Q BIT)、返却値を含む追加メモリ O(I BIT + R)、eraseしてもノードは解放しない
template<int BIT = 60>
vector<unsigned long long> binary_trie_dynamic_queries(const vector<BinaryTrieQuery>& queries) {
    BinaryTrie<BIT> trie;
    vector<unsigned long long> result;
    for (const BinaryTrieQuery& query : queries) {
        if (query.type == 0) {
            trie.insert(query.x);
        } else if (query.type == 1) {
            result.push_back(trie.erase(query.x) ? 1ULL : 0ULL);
        } else if (query.type == 2) {
            result.push_back(trie.count(query.x));
        } else if (query.type == 3) {
            result.push_back(trie.kth(query.k));
        } else if (query.type == 4) {
            result.push_back(trie.count_less(query.x));
        } else if (query.type == 5) {
            result.push_back(trie.min_xor(query.x));
        } else if (query.type == 6) {
            result.push_back(trie.max_xor(query.x));
        } else if (query.type == 7) {
            result.push_back(trie.min_element());
        } else if (query.type == 8) {
            result.push_back(trie.max_element());
        } else if (query.type == 9) {
            result.push_back(trie.count_xor_less(query.x, query.y));
        } else {
            assert(false);
        }
    }
    return result;
}

/* ============================================================
 * XorLazyBinaryTrie
 * ============================================================ */

template<int BIT = 60>
struct XorLazyBinaryTrie {
    static_assert(0 <= BIT && BIT <= 64, "BIT must be in [0, 64]");

    struct Node {
        array<int, 2> next;
        int cnt;

        Node() : next{-1, -1}, cnt(0) {}
    };

    vector<Node> nodes;
    int total;
    unsigned long long lazy;

    // 空のXorLazyBinaryTrieを構築する O(1)
    XorLazyBinaryTrie() : nodes(1), total(0), lazy(0ULL) {}

    // 登録整数数を重複込みで返す O(1)
    int size() const {
        return total;
    }

    // 整数xを1個追加する O(BIT)
    void insert(unsigned long long x) {
        insert_raw(x ^ lazy);
    }

    // 整数xを1個削除し、削除できたかを返す O(BIT)
    bool erase(unsigned long long x) {
        return erase_raw(x ^ lazy);
    }

    // 整数xの登録個数を返す O(BIT)
    int count(unsigned long long x) const {
        return count_raw(x ^ lazy);
    }

    // 全登録整数にxをxorする O(1)
    void xor_all(unsigned long long x) {
        lazy ^= x;
    }

    // 現在値の昇順でk番目の整数を0-indexedで返す O(BIT)
    unsigned long long kth(int k) const {
        assert(0 <= k && k < total);
        unsigned long long raw = 0ULL;
        int vertex = 0;

        // 現在値のbitが0になるraw側を先に見る
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int lb = bit_at(lazy, bit);
            const int zero_child = nodes[vertex].next[lb];
            const int zero_count = node_count(zero_child);
            int raw_bit = lb;
            if (k < zero_count) {
                vertex = zero_child;
            } else {
                k -= zero_count;
                raw_bit = 1 - lb;
                vertex = nodes[vertex].next[raw_bit];
            }
            if (raw_bit == 1) {
                raw |= (1ULL << bit);
            }
        }

        return raw ^ lazy;
    }

    // 現在値のうちx未満の個数を返す O(BIT)
    int count_less(unsigned long long x) const {
        if constexpr (BIT < 64) {
            if ((x >> BIT) != 0ULL) {
                return total;
            }
        }

        int result = 0;
        int vertex = 0;

        // xのbitが1なら、現在値のbitが0の部分木はすべてx未満になる
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (vertex == -1) {
                break;
            }
            const int xb = bit_at(x, bit);
            const int lb = bit_at(lazy, bit);
            if (xb == 1) {
                result += node_count(nodes[vertex].next[lb]);
                vertex = nodes[vertex].next[1 - lb];
            } else {
                vertex = nodes[vertex].next[lb];
            }
        }

        return result;
    }

    // 現在値yのうち(x xor y)がlimit未満の個数を返す O(BIT)
    int count_xor_less(unsigned long long x, unsigned long long limit) const {
        return count_raw_xor_less(x ^ lazy, limit);
    }

    // min(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long min_xor(unsigned long long x) const {
        return min_raw_xor(x ^ lazy);
    }

    // max(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long max_xor(unsigned long long x) const {
        return max_raw_xor(x ^ lazy);
    }

    // 現在値の最小値を返す O(BIT)
    unsigned long long min_element() const {
        return kth(0);
    }

    // 現在値の最大値を返す O(BIT)
    unsigned long long max_element() const {
        return kth(total - 1);
    }

private:
    static int bit_at(unsigned long long x, int bit) {
        return (x >> bit) & 1ULL;
    }

    int node_count(int vertex) const {
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].cnt;
    }

    void insert_raw(unsigned long long x) {
        int vertex = 0;
        ++nodes[vertex].cnt;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            int to = nodes[vertex].next[b];
            if (to == -1) {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next[b] = to;
                nodes.emplace_back();
            }
            vertex = to;
            ++nodes[vertex].cnt;
        }
        ++total;
    }

    bool erase_raw(unsigned long long x) {
        if (count_raw(x) == 0) {
            return false;
        }
        int vertex = 0;
        --nodes[vertex].cnt;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            vertex = nodes[vertex].next[b];
            --nodes[vertex].cnt;
        }
        --total;
        return true;
    }

    int count_raw(unsigned long long x) const {
        int vertex = 0;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            vertex = nodes[vertex].next[b];
            if (vertex == -1) {
                return 0;
            }
        }
        return nodes[vertex].cnt;
    }

    int count_raw_xor_less(unsigned long long x, unsigned long long limit) const {
        if constexpr (BIT < 64) {
            if ((limit >> BIT) != 0ULL) {
                return total;
            }
        }
        int result = 0;
        int vertex = 0;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (vertex == -1) {
                break;
            }
            const int xb = bit_at(x, bit);
            const int lb = bit_at(limit, bit);
            if (lb == 1) {
                result += node_count(nodes[vertex].next[xb]);
                vertex = nodes[vertex].next[1 - xb];
            } else {
                vertex = nodes[vertex].next[xb];
            }
        }
        return result;
    }

    unsigned long long min_raw_xor(unsigned long long x) const {
        assert(total > 0);
        unsigned long long result = 0ULL;
        int vertex = 0;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int same = nodes[vertex].next[b];
            if (node_count(same) > 0) {
                vertex = same;
            } else {
                result |= (1ULL << bit);
                vertex = nodes[vertex].next[1 - b];
            }
        }
        return result;
    }

    unsigned long long max_raw_xor(unsigned long long x) const {
        assert(total > 0);
        unsigned long long result = 0ULL;
        int vertex = 0;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int opposite = nodes[vertex].next[1 - b];
            if (node_count(opposite) > 0) {
                result |= (1ULL << bit);
                vertex = opposite;
            } else {
                vertex = nodes[vertex].next[b];
            }
        }
        return result;
    }
};

/* ------------------------------------------------------------
 * XorLazyBinaryTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

struct XorLazyBinaryTrieQuery {
    int type;
    unsigned long long x;
    unsigned long long y;
    int k;
};

// 想定ユースケース:
//   全要素への一括xor更新を伴う整数multisetをオンライン処理する
// 入力:
//   queries: 以下の操作列、値とxor maskはBIT bit以内とする
//   type=0: xをinsert、出力なし
//   type=1: 現在値xを1個eraseし、成功なら1、存在しなければ0を出力
//   type=2: 全要素へxをxor、出力なし
//   type=3: 現在値xのcountを出力
//   type=4: 現在値の昇順で0-indexedのkth(k)を出力
//   type=5: 現在値がx未満の要素数を出力
//   type=6: 現在集合に対するmin(x xor v)を出力
//   type=7: 現在集合に対するmax(x xor v)を出力
//   type=8: 現在値の最小要素を出力
//   type=9: 現在値の最大要素を出力
//   type=10: 現在値vのうち(x xor v) < yとなる要素数を出力
// 出力:
//   出力を持つ操作の答えを、queriesに現れる順に格納した列
//   type=4のkとtype=6から9は、呼び出し時に有効な順位または非空集合であること
// 処理量:
//   Q = queries.size()、I = insert操作数、R = 出力数として
//   type=2は O(1)、その他は O(BIT)、全体で O(Q BIT)
//   返却値を含む追加メモリ O(I BIT + R)、eraseしてもノードは解放しない
template<int BIT = 60>
vector<unsigned long long> xor_lazy_binary_trie_queries(const vector<XorLazyBinaryTrieQuery>& queries) {
    XorLazyBinaryTrie<BIT> trie;
    vector<unsigned long long> result;
    for (const XorLazyBinaryTrieQuery& query : queries) {
        if (query.type == 0) {
            trie.insert(query.x);
        } else if (query.type == 1) {
            result.push_back(trie.erase(query.x) ? 1ULL : 0ULL);
        } else if (query.type == 2) {
            trie.xor_all(query.x);
        } else if (query.type == 3) {
            result.push_back(trie.count(query.x));
        } else if (query.type == 4) {
            result.push_back(trie.kth(query.k));
        } else if (query.type == 5) {
            result.push_back(trie.count_less(query.x));
        } else if (query.type == 6) {
            result.push_back(trie.min_xor(query.x));
        } else if (query.type == 7) {
            result.push_back(trie.max_xor(query.x));
        } else if (query.type == 8) {
            result.push_back(trie.min_element());
        } else if (query.type == 9) {
            result.push_back(trie.max_element());
        } else if (query.type == 10) {
            result.push_back(trie.count_xor_less(query.x, query.y));
        } else {
            assert(false);
        }
    }
    return result;
}

/* ============================================================
 * XorMexSet
 * ============================================================ */

template<int BIT = 60>
struct XorMexSet {
    static_assert(0 <= BIT && BIT <= 64, "BIT must be in [0, 64]");

    struct Node {
        array<int, 2> next;
        int cnt;

        Node() : next{-1, -1}, cnt(0) {}
    };

    vector<Node> nodes;
    int total;
    unsigned long long lazy;

    // 空のXorMexSetを構築する O(1)
    XorMexSet() : nodes(1), total(0), lazy(0ULL) {}

    // 登録整数数を重複なしで返す O(1)
    int size() const {
        return total;
    }

    // 整数xを追加し、追加されたかを返す O(BIT)
    bool insert(unsigned long long x) {
        const unsigned long long raw = x ^ lazy;
        if (contains_raw(raw)) {
            return false;
        }

        int vertex = 0;
        ++nodes[vertex].cnt;

        // setなので存在確認後に各ノードの個数を1だけ増やす
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(raw, bit);
            int to = nodes[vertex].next[b];
            if (to == -1) {
                to = static_cast<int>(nodes.size());
                nodes[vertex].next[b] = to;
                nodes.emplace_back();
            }
            vertex = to;
            ++nodes[vertex].cnt;
        }

        ++total;
        return true;
    }

    // 整数xを削除し、削除されたかを返す O(BIT)
    bool erase(unsigned long long x) {
        const unsigned long long raw = x ^ lazy;
        if (!contains_raw(raw)) {
            return false;
        }

        int vertex = 0;
        --nodes[vertex].cnt;

        // ノードは残し、集合要素数だけを減らす
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(raw, bit);
            vertex = nodes[vertex].next[b];
            --nodes[vertex].cnt;
        }

        --total;
        return true;
    }

    // 整数xが登録されているかを返す O(BIT)
    bool contains(unsigned long long x) const {
        return contains_raw(x ^ lazy);
    }

    // 全登録整数にxをxorする O(1)
    void xor_all(unsigned long long x) {
        lazy ^= x;
    }

    // 現在集合のmexを返す O(BIT)
    unsigned long long mex() const {
        return mex_xor(0ULL);
    }

    // {a xor x | a in S}のmexを返す O(BIT)
    unsigned long long mex_xor(unsigned long long x) const {
        const unsigned long long query = lazy ^ x;
        if constexpr (BIT < 63) {
            const unsigned long long universe = 1ULL << BIT;
            if (total == universe) {
                return universe;
            }
        }

        unsigned long long result = 0ULL;
        int vertex = 0;

        // 小さいmexを作るため、現在bitが0の側が満杯かを調べる
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (vertex == -1) {
                break;
            }
            const int qb = bit_at(query, bit);
            const int zero_child = nodes[vertex].next[qb];
            const unsigned long long capacity = 1ULL << bit;
            if (static_cast<unsigned long long>(node_count(zero_child)) < capacity) {
                vertex = zero_child;
            } else {
                result |= (1ULL << bit);
                vertex = nodes[vertex].next[1 - qb];
            }
        }

        return result;
    }

private:
    static int bit_at(unsigned long long x, int bit) {
        return (x >> bit) & 1ULL;
    }

    int node_count(int vertex) const {
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].cnt;
    }

    bool contains_raw(unsigned long long x) const {
        int vertex = 0;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            vertex = nodes[vertex].next[b];
            if (vertex == -1) {
                return false;
            }
        }
        return nodes[vertex].cnt > 0;
    }
};

/* ------------------------------------------------------------
 * XorMexSetを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

// 想定ユースケース:
//   固定集合に対し、各xor mask適用後のmexをまとめて求める
// 入力:
//   values: BIT bit以内の非負整数列、重複は集合として無視する
//   queries: 集合の各要素へxorするmask列
// 出力:
//   result[i] = mex({value xor queries[i] | value in values})
// 処理量:
//   N = values.size()、M = queries.size()、U = values内の異なる値数として
//   時間 O((N + M)BIT)、返却値を含む追加メモリ O(U BIT + M)
template<int BIT = 60>
vector<unsigned long long> xor_mex_static_queries(const vector<unsigned long long>& values, const vector<unsigned long long>& queries) {
    XorMexSet<BIT> mex_set;
    for (const unsigned long long value : values) {
        mex_set.insert(value);
    }
    vector<unsigned long long> result;
    result.reserve(queries.size());
    for (const unsigned long long query : queries) {
        result.push_back(mex_set.mex_xor(query));
    }
    return result;
}

struct XorMexSetQuery {
    int type;
    unsigned long long x;
};

// 想定ユースケース:
//   全要素への一括xor更新を伴う整数setに対し、存在判定とmex系クエリをオンライン処理する
// 入力:
//   queries: 以下の操作列、値とxor maskはBIT bit以内とする
//   type=0: xをinsertし、新規追加なら1、既存なら0を出力
//   type=1: 現在値xをeraseし、削除できれば1、存在しなければ0を出力
//   type=2: 全要素へxをxor、出力なし
//   type=3: 現在値xが存在すれば1、存在しなければ0を出力
//   type=4: 現在集合のmexを出力
//   type=5: {v xor x | vは現在集合の要素}のmexを出力
// 出力:
//   出力を持つ操作の答えを、queriesに現れる順に格納した列
// 処理量:
//   Q = queries.size()、I = insert操作数、R = 出力数として
//   type=2は O(1)、その他は O(BIT)、全体で O(Q BIT)
//   返却値を含む追加メモリ O(I BIT + R)、eraseしてもノードは解放しない
template<int BIT = 60>
vector<unsigned long long> xor_mex_dynamic_queries(const vector<XorMexSetQuery>& queries) {
    XorMexSet<BIT> mex_set;
    vector<unsigned long long> result;
    for (const XorMexSetQuery& query : queries) {
        if (query.type == 0) {
            result.push_back(mex_set.insert(query.x) ? 1ULL : 0ULL);
        } else if (query.type == 1) {
            result.push_back(mex_set.erase(query.x) ? 1ULL : 0ULL);
        } else if (query.type == 2) {
            mex_set.xor_all(query.x);
        } else if (query.type == 3) {
            result.push_back(mex_set.contains(query.x) ? 1ULL : 0ULL);
        } else if (query.type == 4) {
            result.push_back(mex_set.mex());
        } else if (query.type == 5) {
            result.push_back(mex_set.mex_xor(query.x));
        } else {
            assert(false);
        }
    }
    return result;
}

/* ============================================================
 * PersistentBinaryTrie
 * ============================================================ */

template<int BIT = 60>
struct PersistentBinaryTrie {
    static_assert(0 <= BIT && BIT <= 64, "BIT must be in [0, 64]");

    struct Node {
        array<int, 2> next;
        int cnt;

        Node() : next{-1, -1}, cnt(0) {}
    };

    vector<Node> nodes;

    // 空根0を持つ永続BinaryTrieを構築する O(1)
    PersistentBinaryTrie() : nodes(1) {}

    // rootに整数xを1個追加した新しいrootを返す O(BIT)
    int insert(int root, unsigned long long x) {
        const int new_root = clone_node(root);
        ++nodes[new_root].cnt;
        int old_vertex = root;
        int new_vertex = new_root;

        // 更新経路だけをコピーし、それ以外の部分木は共有する
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int old_child = child(old_vertex, b);
            const int new_child = clone_node(old_child);
            nodes[new_vertex].next[b] = new_child;
            old_vertex = old_child;
            new_vertex = new_child;
            ++nodes[new_vertex].cnt;
        }

        return new_root;
    }

    // rootに含まれる整数数を重複込みで返す O(1)
    int size(int root) const {
        return node_count(root);
    }

    // right_rootとleft_rootの差分に含まれる整数数を返す O(1)
    int size(int left_root, int right_root) const {
        return node_count(right_root) - node_count(left_root);
    }

    // 差分集合内の整数xの登録個数を返す O(BIT)
    int count(int left_root, int right_root, unsigned long long x) const {
        int left_vertex = left_root;
        int right_vertex = right_root;

        // 両方のrootを同時に辿り、最後に個数差を返す
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            left_vertex = child(left_vertex, b);
            right_vertex = child(right_vertex, b);
            if (right_vertex == -1) {
                return 0;
            }
        }

        return node_count(right_vertex) - node_count(left_vertex);
    }

    // 差分集合の昇順でk番目の整数を0-indexedで返す O(BIT)
    unsigned long long kth(int left_root, int right_root, int k) const {
        assert(0 <= k && k < size(left_root, right_root));
        unsigned long long result = 0ULL;
        int left_vertex = left_root;
        int right_vertex = right_root;

        // 0側の差分個数でkが属する部分木を判定する
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int left_zero = child(left_vertex, 0);
            const int right_zero = child(right_vertex, 0);
            const int zero_count = node_count(right_zero) - node_count(left_zero);
            if (k < zero_count) {
                left_vertex = left_zero;
                right_vertex = right_zero;
            } else {
                k -= zero_count;
                result |= (1ULL << bit);
                left_vertex = child(left_vertex, 1);
                right_vertex = child(right_vertex, 1);
            }
        }

        return result;
    }

    // 差分集合内の整数のうちx未満の個数を返す O(BIT)
    int count_less(int left_root, int right_root, unsigned long long x) const {
        if constexpr (BIT < 64) {
            if ((x >> BIT) != 0ULL) {
                return size(left_root, right_root);
            }
        }

        int result = 0;
        int left_vertex = left_root;
        int right_vertex = right_root;

        // xのbitが1なら、その桁が0の差分部分木を足す
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (right_vertex == -1) {
                break;
            }
            const int b = bit_at(x, bit);
            if (b == 1) {
                result += node_count(child(right_vertex, 0)) - node_count(child(left_vertex, 0));
                left_vertex = child(left_vertex, 1);
                right_vertex = child(right_vertex, 1);
            } else {
                left_vertex = child(left_vertex, 0);
                right_vertex = child(right_vertex, 0);
            }
        }

        return result;
    }

    // 差分集合内の整数yのうち(x xor y)がlimit未満の個数を返す O(BIT)
    int count_xor_less(int left_root, int right_root, unsigned long long x, unsigned long long limit) const {
        if constexpr (BIT < 64) {
            if ((limit >> BIT) != 0ULL) {
                return size(left_root, right_root);
            }
        }

        int result = 0;
        int left_vertex = left_root;
        int right_vertex = right_root;

        // limitのbitが1ならxor bitが0の差分部分木を足す
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (right_vertex == -1) {
                break;
            }
            const int xb = bit_at(x, bit);
            const int lb = bit_at(limit, bit);
            if (lb == 1) {
                result += node_count(child(right_vertex, xb)) - node_count(child(left_vertex, xb));
                left_vertex = child(left_vertex, 1 - xb);
                right_vertex = child(right_vertex, 1 - xb);
            } else {
                left_vertex = child(left_vertex, xb);
                right_vertex = child(right_vertex, xb);
            }
        }

        return result;
    }

    // 差分集合でmin(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long min_xor(int left_root, int right_root, unsigned long long x) const {
        assert(size(left_root, right_root) > 0);
        unsigned long long result = 0ULL;
        int left_vertex = left_root;
        int right_vertex = right_root;

        // 同じbitの差分部分木があれば優先する
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int same_count = node_count(child(right_vertex, b)) - node_count(child(left_vertex, b));
            if (same_count > 0) {
                left_vertex = child(left_vertex, b);
                right_vertex = child(right_vertex, b);
            } else {
                result |= (1ULL << bit);
                left_vertex = child(left_vertex, 1 - b);
                right_vertex = child(right_vertex, 1 - b);
            }
        }

        return result;
    }

    // 差分集合でmax(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long max_xor(int left_root, int right_root, unsigned long long x) const {
        assert(size(left_root, right_root) > 0);
        unsigned long long result = 0ULL;
        int left_vertex = left_root;
        int right_vertex = right_root;

        // 反対bitの差分部分木があれば優先する
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int opposite_count = node_count(child(right_vertex, 1 - b)) - node_count(child(left_vertex, 1 - b));
            if (opposite_count > 0) {
                result |= (1ULL << bit);
                left_vertex = child(left_vertex, 1 - b);
                right_vertex = child(right_vertex, 1 - b);
            } else {
                left_vertex = child(left_vertex, b);
                right_vertex = child(right_vertex, b);
            }
        }

        return result;
    }

private:
    static int bit_at(unsigned long long x, int bit) {
        return (x >> bit) & 1ULL;
    }

    int clone_node(int vertex) {
        if (vertex == -1) {
            nodes.emplace_back();
        } else {
            nodes.push_back(nodes[vertex]);
        }
        return static_cast<int>(nodes.size()) - 1;
    }

    int node_count(int vertex) const {
        if (vertex == -1) {
            return 0;
        }
        return nodes[vertex].cnt;
    }

    int child(int vertex, int b) const {
        if (vertex == -1) {
            return -1;
        }
        return nodes[vertex].next[b];
    }
};

/* ------------------------------------------------------------
 * PersistentBinaryTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

struct RangeXorQuery {
    int l;
    int r;
    unsigned long long x;
};

// 想定ユースケース:
//   静的配列の各区間について、指定値とのxor最大値を求める
// 入力:
//   a: BIT bit以内の非負整数列
//   queries: [l, r)とxを持つクエリ列、各区間は0 <= l < r <= a.size()
// 出力:
//   result[i] = max(a[j] xor queries[i].x), queries[i].l <= j < queries[i].r
// 処理量:
//   N = a.size()、Q = queries.size()として
//   時間 O((N + Q)BIT)、返却値を含む追加メモリ O(N BIT + Q)
template<int BIT = 60>
vector<unsigned long long> persistent_binary_trie_range_max_xor(const vector<unsigned long long>& a, const vector<RangeXorQuery>& queries) {
    PersistentBinaryTrie<BIT> trie;
    vector<int> roots(a.size() + 1U, 0);
    for (size_t index = 0; index < a.size(); ++index) {
        roots[index + 1U] = trie.insert(roots[index], a[index]);
    }
    vector<unsigned long long> result;
    result.reserve(queries.size());
    for (const RangeXorQuery& query : queries) {
        result.push_back(trie.max_xor(roots[query.l], roots[query.r], query.x));
    }
    return result;
}

// 想定ユースケース:
//   静的配列の各区間について、指定値とのxor最小値を求める
// 入力:
//   a: BIT bit以内の非負整数列
//   queries: [l, r)とxを持つクエリ列、各区間は0 <= l < r <= a.size()
// 出力:
//   result[i] = min(a[j] xor queries[i].x), queries[i].l <= j < queries[i].r
// 処理量:
//   N = a.size()、Q = queries.size()として
//   時間 O((N + Q)BIT)、返却値を含む追加メモリ O(N BIT + Q)
template<int BIT = 60>
vector<unsigned long long> persistent_binary_trie_range_min_xor(const vector<unsigned long long>& a, const vector<RangeXorQuery>& queries) {
    PersistentBinaryTrie<BIT> trie;
    vector<int> roots(a.size() + 1U, 0);
    for (size_t index = 0; index < a.size(); ++index) {
        roots[index + 1U] = trie.insert(roots[index], a[index]);
    }
    vector<unsigned long long> result;
    result.reserve(queries.size());
    for (const RangeXorQuery& query : queries) {
        result.push_back(trie.min_xor(roots[query.l], roots[query.r], query.x));
    }
    return result;
}

struct RangeKthQuery {
    int l;
    int r;
    int k;
};

// 想定ユースケース:
//   静的配列の各区間について、重複込みの昇順k番目を求める
// 入力:
//   a: BIT bit以内の非負整数列
//   queries: [l, r)と0-indexedのkを持つクエリ列
//   各クエリは0 <= l < r <= a.size()かつ0 <= k < r - lを満たす
// 出力:
//   result[i] = a[queries[i].l .. queries[i].r)を昇順に並べたqueries[i].k番目
// 処理量:
//   N = a.size()、Q = queries.size()として
//   時間 O((N + Q)BIT)、返却値を含む追加メモリ O(N BIT + Q)
template<int BIT = 60>
vector<unsigned long long> persistent_binary_trie_range_kth(const vector<unsigned long long>& a, const vector<RangeKthQuery>& queries) {
    PersistentBinaryTrie<BIT> trie;
    vector<int> roots(a.size() + 1U, 0);
    for (size_t index = 0; index < a.size(); ++index) {
        roots[index + 1U] = trie.insert(roots[index], a[index]);
    }
    vector<unsigned long long> result;
    result.reserve(queries.size());
    for (const RangeKthQuery& query : queries) {
        result.push_back(trie.kth(roots[query.l], roots[query.r], query.k));
    }
    return result;
}

struct RangeLessQuery {
    int l;
    int r;
    unsigned long long x;
};

// 想定ユースケース:
//   静的配列の各区間について、指定値未満の要素数を求める
// 入力:
//   a: BIT bit以内の非負整数列
//   queries: [l, r)とxを持つクエリ列、各区間は0 <= l <= r <= a.size()
// 出力:
//   result[i] = queries[i].l <= j < queries[i].rかつa[j] < queries[i].xとなる要素数
// 処理量:
//   N = a.size()、Q = queries.size()として
//   時間 O((N + Q)BIT)、返却値を含む追加メモリ O(N BIT + Q)
template<int BIT = 60>
vector<int> persistent_binary_trie_range_count_less(const vector<unsigned long long>& a, const vector<RangeLessQuery>& queries) {
    PersistentBinaryTrie<BIT> trie;
    vector<int> roots(a.size() + 1U, 0);
    for (size_t index = 0; index < a.size(); ++index) {
        roots[index + 1U] = trie.insert(roots[index], a[index]);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const RangeLessQuery& query : queries) {
        result.push_back(trie.count_less(roots[query.l], roots[query.r], query.x));
    }
    return result;
}

struct RangeXorLessQuery {
    int l;
    int r;
    unsigned long long x;
    unsigned long long limit;
};

// 想定ユースケース:
//   静的配列の各区間について、指定値とのxorが上限未満となる要素数を求める
// 入力:
//   a: BIT bit以内の非負整数列
//   queries: [l, r)、x、limitを持つクエリ列、各区間は0 <= l <= r <= a.size()
// 出力:
//   result[i] = queries[i].l <= j < queries[i].rかつ
//               (a[j] xor queries[i].x) < queries[i].limitとなる要素数
// 処理量:
//   N = a.size()、Q = queries.size()として
//   時間 O((N + Q)BIT)、返却値を含む追加メモリ O(N BIT + Q)
template<int BIT = 60>
vector<int> persistent_binary_trie_range_count_xor_less(const vector<unsigned long long>& a, const vector<RangeXorLessQuery>& queries) {
    PersistentBinaryTrie<BIT> trie;
    vector<int> roots(a.size() + 1U, 0);
    for (size_t index = 0; index < a.size(); ++index) {
        roots[index + 1U] = trie.insert(roots[index], a[index]);
    }
    vector<int> result;
    result.reserve(queries.size());
    for (const RangeXorLessQuery& query : queries) {
        result.push_back(trie.count_xor_less(roots[query.l], roots[query.r], query.x, query.limit));
    }
    return result;
}

/* ============================================================
 * MergeableBinaryTrie
 * ============================================================ */

template<int BIT = 60>
struct MergeableBinaryTrie {
    static_assert(0 <= BIT && BIT <= 64, "BIT must be in [0, 64]");

    struct Node {
        array<int, 2> next;
        int cnt;

        Node() : next{0, 0}, cnt(0) {}
    };

    vector<Node> nodes;

    // 空ノード0を持つマージ可能BinaryTrieを構築する O(1)
    MergeableBinaryTrie() : nodes(1) {}

    // rootに整数xを1個追加し、更新後のrootを返す O(BIT)
    int insert(int root, unsigned long long x) {
        int result_root = root;
        if (result_root == 0) {
            result_root = new_node();
        }
        int vertex = result_root;
        ++nodes[vertex].cnt;

        // 破壊的にroot以下へ追加し、必要なノードだけ作る
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            int to = nodes[vertex].next[b];
            if (to == 0) {
                to = new_node();
                nodes[vertex].next[b] = to;
            }
            vertex = to;
            ++nodes[vertex].cnt;
        }

        return result_root;
    }

    // 2つのrootを破壊的に併合し、併合後のrootを返す O(共有ノード数)
    int merge(int root_a, int root_b) {
        return merge_nodes(root_a, root_b);
    }

    // rootに含まれる整数数を重複込みで返す O(1)
    int size(int root) const {
        return node_count(root);
    }

    // root内の整数xの登録個数を返す O(BIT)
    int count(int root, unsigned long long x) const {
        int vertex = root;
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (vertex == 0) {
                return 0;
            }
            const int b = bit_at(x, bit);
            vertex = nodes[vertex].next[b];
        }
        return node_count(vertex);
    }

    // root内の昇順でk番目の整数を0-indexedで返す O(BIT)
    unsigned long long kth(int root, int k) const {
        assert(0 <= k && k < size(root));
        unsigned long long result = 0ULL;
        int vertex = root;

        // 0側の個数でkが属する部分木を判定する
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int left = nodes[vertex].next[0];
            const int left_count = node_count(left);
            if (k < left_count) {
                vertex = left;
            } else {
                k -= left_count;
                result |= (1ULL << bit);
                vertex = nodes[vertex].next[1];
            }
        }

        return result;
    }

    // root内の整数のうちx未満の個数を返す O(BIT)
    int count_less(int root, unsigned long long x) const {
        if constexpr (BIT < 64) {
            if ((x >> BIT) != 0ULL) {
                return size(root);
            }
        }

        int result = 0;
        int vertex = root;

        // xのbitが1なら、その桁が0の部分木を足す
        for (int bit = BIT - 1; bit >= 0; --bit) {
            if (vertex == 0) {
                break;
            }
            const int b = bit_at(x, bit);
            if (b == 1) {
                result += node_count(nodes[vertex].next[0]);
                vertex = nodes[vertex].next[1];
            } else {
                vertex = nodes[vertex].next[0];
            }
        }

        return result;
    }

    // root内でmin(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long min_xor(int root, unsigned long long x) const {
        assert(size(root) > 0);
        unsigned long long result = 0ULL;
        int vertex = root;

        // 同じbitの子を優先し、なければ反対側へ進む
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int same = nodes[vertex].next[b];
            if (node_count(same) > 0) {
                vertex = same;
            } else {
                result |= (1ULL << bit);
                vertex = nodes[vertex].next[1 - b];
            }
        }

        return result;
    }

    // root内でmax(x xor y)を満たすxor値を返す O(BIT)
    unsigned long long max_xor(int root, unsigned long long x) const {
        assert(size(root) > 0);
        unsigned long long result = 0ULL;
        int vertex = root;

        // 反対bitの子を優先し、なければ同じ側へ進む
        for (int bit = BIT - 1; bit >= 0; --bit) {
            const int b = bit_at(x, bit);
            const int opposite = nodes[vertex].next[1 - b];
            if (node_count(opposite) > 0) {
                result |= (1ULL << bit);
                vertex = opposite;
            } else {
                vertex = nodes[vertex].next[b];
            }
        }

        return result;
    }

    // root内の最小値を返す O(BIT)
    unsigned long long min_element(int root) const {
        return kth(root, 0);
    }

    // root内の最大値を返す O(BIT)
    unsigned long long max_element(int root) const {
        return kth(root, size(root) - 1);
    }

private:
    static int bit_at(unsigned long long x, int bit) {
        return (x >> bit) & 1ULL;
    }

    int new_node() {
        nodes.emplace_back();
        return static_cast<int>(nodes.size()) - 1;
    }

    int node_count(int vertex) const {
        if (vertex == 0) {
            return 0;
        }
        return nodes[vertex].cnt;
    }

    int merge_nodes(int a, int b) {
        if (a == 0) {
            return b;
        }
        if (b == 0) {
            return a;
        }
        nodes[a].next[0] = merge_nodes(nodes[a].next[0], nodes[b].next[0]);
        nodes[a].next[1] = merge_nodes(nodes[a].next[1], nodes[b].next[1]);
        nodes[a].cnt += nodes[b].cnt;
        return a;
    }
};

/* ------------------------------------------------------------
 * MergeableBinaryTrieを使うユースケースソルバー関数
 * ------------------------------------------------------------ */

struct SubtreeXorQuery {
    int v;
    unsigned long long x;
};

// 想定ユースケース:
//   木の各部分木に含まれる頂点値と、指定値とのxor最大値をオフラインで求める
// 入力:
//   value: 各頂点のBIT bit以内の非負整数値
//   graph: value.size()頂点の無向木の隣接リスト
//   queries: 頂点vと値xを持つクエリ列
//   root: 部分木を定める根
// 出力:
//   result[i] = rootを根としたqueries[i].vの部分木に属する頂点uに対する
//               max(value[u] xor queries[i].x)
// 処理量:
//   N = value.size()、Q = queries.size()として、破壊的merge全体は作成ノード数に比例するため
//   時間 O((N + Q)BIT)、返却値を含む追加メモリ O(N BIT + N + Q)
template<int BIT = 60>
vector<unsigned long long> mergeable_binary_trie_subtree_max_xor(
    const vector<unsigned long long>& value,
    const vector<vector<int>>& graph,
    const vector<SubtreeXorQuery>& queries,
    int root = 0
) {
    const int n = static_cast<int>(value.size());
    assert(static_cast<int>(graph.size()) == n);
    assert(0 <= root && root < n);

    vector<vector<int>> query_ids(n);
    for (int index = 0; index < static_cast<int>(queries.size()); ++index) {
        assert(0 <= queries[index].v && queries[index].v < n);
        query_ids[queries[index].v].push_back(index);
    }

    vector<int> parent(n, -1);
    vector<int> order;
    order.reserve(n);
    parent[root] = root;
    vector<int> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        const int vertex = stack.back();
        stack.pop_back();
        order.push_back(vertex);
        for (const int to : graph[vertex]) {
            if (to == parent[vertex]) {
                continue;
            }
            parent[to] = vertex;
            stack.push_back(to);
        }
    }

    MergeableBinaryTrie<BIT> trie;
    vector<int> trie_roots(n, 0);
    vector<unsigned long long> result(queries.size(), 0ULL);

    // 子のクエリを先に答え、親へmergeされた後は子rootを使わない
    for (int order_index = n - 1; order_index >= 0; --order_index) {
        const int vertex = order[order_index];
        int trie_root = 0;
        trie_root = trie.insert(trie_root, value[vertex]);
        for (const int to : graph[vertex]) {
            if (parent[to] == vertex) {
                trie_root = trie.merge(trie_root, trie_roots[to]);
            }
        }
        trie_roots[vertex] = trie_root;
        for (const int query_id : query_ids[vertex]) {
            result[query_id] = trie.max_xor(trie_root, queries[query_id].x);
        }
    }

    return result;
}

/* ============================================================
 * テスト
 * ============================================================ */
#if __INCLUDE_LEVEL__ == 0

static string random_string(mt19937& rng, int alphabet, int max_len) {
    uniform_int_distribution<int> len_dist(0, max_len);
    uniform_int_distribution<int> char_dist(0, alphabet - 1);
    const int len = len_dist(rng);
    string result;
    result.reserve(len);
    for (int index = 0; index < len; ++index) {
        result.push_back(static_cast<char>('a' + char_dist(rng)));
    }
    return result;
}

static bool is_prefix_string(const string& prefix, const string& word) {
    if (prefix.size() > word.size()) {
        return false;
    }
    for (size_t index = 0; index < prefix.size(); ++index) {
        if (prefix[index] != word[index]) {
            return false;
        }
    }
    return true;
}

static int naive_string_count(const vector<string>& bag, const string& query) {
    int result = 0;
    for (const string& word : bag) {
        if (word == query) {
            ++result;
        }
    }
    return result;
}

static int naive_prefix_count(const vector<string>& bag, const string& query) {
    int result = 0;
    for (const string& word : bag) {
        if (is_prefix_string(query, word)) {
            ++result;
        }
    }
    return result;
}

static int naive_count_prefixes(const vector<string>& bag, const string& query) {
    int result = 0;
    for (const string& word : bag) {
        if (is_prefix_string(word, query)) {
            ++result;
        }
    }
    return result;
}

static int naive_string_count_less(const vector<string>& bag, const string& query) {
    int result = 0;
    for (const string& word : bag) {
        if (word < query) {
            ++result;
        }
    }
    return result;
}

static string naive_string_kth(vector<string> bag, int k) {
    sort(bag.begin(), bag.end());
    return bag[k];
}

static void test_trie() {
    Trie<3, 'a'> trie;
    vector<string> bag;
    const vector<string> initial = {"", "a", "ab", "abc", "b", "a", ""};
    for (const string& word : initial) {
        trie.insert(word);
        bag.push_back(word);
    }
    assert(trie.size() == static_cast<int>(bag.size()));
    assert(trie.count("") == 2);
    assert(trie.count("a") == 2);
    assert(trie.prefix_count("") == static_cast<int>(bag.size()));
    assert(trie.prefix_count("a") == 4);
    assert(trie.count_prefixes("abc") == 6);
    assert(trie.kth(0) == "");
    assert(trie.kth(2) == "a");
    assert(trie.count_less("ab") == naive_string_count_less(bag, "ab"));
    assert(trie.erase("a"));
    assert(!trie.erase("cc"));

    mt19937 rng(1U);
    Trie<3, 'a'> random_trie;
    vector<string> random_bag;
    uniform_int_distribution<int> op_dist(0, 4);
    for (int step = 0; step < 2000; ++step) {
        const int op = op_dist(rng);
        const string word = random_string(rng, 3, 5);
        if (op <= 1) {
            random_trie.insert(word);
            random_bag.push_back(word);
        } else if (op == 2) {
            const bool erased = random_trie.erase(word);
            bool naive_erased = false;
            for (auto it = random_bag.begin(); it != random_bag.end(); ++it) {
                if (*it == word) {
                    random_bag.erase(it);
                    naive_erased = true;
                    break;
                }
            }
            assert(erased == naive_erased);
        } else {
            assert(random_trie.count(word) == naive_string_count(random_bag, word));
            assert(random_trie.prefix_count(word) == naive_prefix_count(random_bag, word));
            assert(random_trie.count_prefixes(word) == naive_count_prefixes(random_bag, word));
            assert(random_trie.count_less(word) == naive_string_count_less(random_bag, word));
            assert(random_trie.contains(word) == (naive_string_count(random_bag, word) > 0));
        }
        assert(random_trie.size() == static_cast<int>(random_bag.size()));
        if (!random_bag.empty()) {
            uniform_int_distribution<int> k_dist(0, static_cast<int>(random_bag.size()) - 1);
            const int k = k_dist(rng);
            assert(random_trie.kth(k) == naive_string_kth(random_bag, k));
        }
    }

    const vector<string> words = {"a", "ab", "abc", "b", "", "a"};
    const vector<string> queries = {"", "a", "ab", "c"};
    assert((trie_prefix_count_queries<3, 'a'>(words, queries) == vector<int>{6, 4, 2, 0}));
    assert((trie_word_count_queries<3, 'a'>(words, queries) == vector<int>{1, 2, 1, 0}));
    assert((trie_count_prefix_pairs<3, 'a'>(words) == 11LL));
    assert((trie_rank_queries<3, 'a'>(words, queries) == vector<int>{0, 1, 3, 6}));
    assert((trie_kth_strings<3, 'a'>(words, vector<int>{0, 1, 2, 3, 4, 5}) == vector<string>{"", "a", "a", "ab", "abc", "b"}));
}

static vector<int> random_sequence(mt19937& rng, int alphabet, int max_len) {
    uniform_int_distribution<int> len_dist(0, max_len);
    uniform_int_distribution<int> value_dist(0, alphabet - 1);
    const int len = len_dist(rng);
    vector<int> result;
    result.reserve(len);
    for (int index = 0; index < len; ++index) {
        result.push_back(value_dist(rng) * 100 + index % 3);
    }
    return result;
}

static bool is_prefix_sequence(const vector<int>& prefix, const vector<int>& sequence) {
    if (prefix.size() > sequence.size()) {
        return false;
    }
    for (size_t index = 0; index < prefix.size(); ++index) {
        if (prefix[index] != sequence[index]) {
            return false;
        }
    }
    return true;
}

static int naive_sequence_count(const vector<vector<int>>& bag, const vector<int>& query) {
    int result = 0;
    for (const vector<int>& sequence : bag) {
        if (sequence == query) {
            ++result;
        }
    }
    return result;
}

static int naive_sequence_prefix_count(const vector<vector<int>>& bag, const vector<int>& query) {
    int result = 0;
    for (const vector<int>& sequence : bag) {
        if (is_prefix_sequence(query, sequence)) {
            ++result;
        }
    }
    return result;
}

template<class TrieType>
static void run_sequence_trie_basic_test() {
    TrieType trie;
    vector<vector<int>> bag;
    const vector<vector<int>> initial = {{}, {1}, {1, 2}, {1, 2, 3}, {100}, {1}};
    for (const vector<int>& sequence : initial) {
        trie.insert(sequence);
        bag.push_back(sequence);
    }
    assert(trie.size() == static_cast<int>(bag.size()));
    assert(trie.find_node(vector<int>{}) == 0);
    assert(trie.find_node(vector<int>{1}) != -1);
    assert(trie.find_node(vector<int>{1, 3}) == -1);
    assert(trie.count(vector<int>{}) == 1);
    assert(trie.count(vector<int>{1}) == 2);
    assert(trie.count(vector<int>{1, 2}) == 1);
    assert(trie.prefix_count(vector<int>{}) == static_cast<int>(bag.size()));
    assert(trie.prefix_count(vector<int>{1}) == 4);
    assert(trie.prefix_count(vector<int>{1, 2}) == 2);
    assert(trie.contains(vector<int>{100}));
    assert(!trie.contains(vector<int>{999}));
    assert(trie.erase(vector<int>{1}));
    assert(!trie.erase(vector<int>{999}));
    assert(trie.count(vector<int>{1}) == 1);
    assert(trie.prefix_count(vector<int>{1}) == 3);
    assert(trie.size() == static_cast<int>(bag.size()) - 1);

    // 根の分岐が大きいケースでも全遷移を正しく扱えることを確認する
    TrieType wide_trie;
    vector<vector<int>> wide_bag;
    for (int value = 80; value >= -80; --value) {
        const vector<int> sequence = {value, value * 3 + 1};
        wide_trie.insert(sequence);
        wide_bag.push_back(sequence);
        if (value % 5 == 0) {
            wide_trie.insert(vector<int>{value});
            wide_bag.push_back(vector<int>{value});
        }
    }
    assert(wide_trie.prefix_count(vector<int>{}) == static_cast<int>(wide_bag.size()));
    for (int value = -80; value <= 80; ++value) {
        assert(wide_trie.prefix_count(vector<int>{value}) == naive_sequence_prefix_count(wide_bag, vector<int>{value}));
        assert(wide_trie.count(vector<int>{value}) == naive_sequence_count(wide_bag, vector<int>{value}));
        assert(wide_trie.count(vector<int>{value, value * 3 + 1}) == 1);
    }
}

template<class TrieType>
static void run_sequence_trie_random_test(unsigned int seed) {
    mt19937 rng(seed);
    TrieType random_trie;
    vector<vector<int>> random_bag;
    uniform_int_distribution<int> op_dist(0, 4);
    for (int step = 0; step < 3000; ++step) {
        const int op = op_dist(rng);
        const vector<int> sequence = random_sequence(rng, 9, 6);
        if (op <= 1) {
            random_trie.insert(sequence);
            random_bag.push_back(sequence);
        } else if (op == 2) {
            const bool erased = random_trie.erase(sequence);
            bool naive_erased = false;
            for (auto it = random_bag.begin(); it != random_bag.end(); ++it) {
                if (*it == sequence) {
                    random_bag.erase(it);
                    naive_erased = true;
                    break;
                }
            }
            assert(erased == naive_erased);
        } else {
            assert(random_trie.count(sequence) == naive_sequence_count(random_bag, sequence));
            assert(random_trie.prefix_count(sequence) == naive_sequence_prefix_count(random_bag, sequence));
            assert(random_trie.contains(sequence) == (naive_sequence_count(random_bag, sequence) > 0));
            const int node = random_trie.find_node(sequence);
            if (naive_sequence_prefix_count(random_bag, sequence) > 0) {
                assert(node != -1);
            }
        }
        assert(random_trie.size() == static_cast<int>(random_bag.size()));
        assert(random_trie.prefix_count(vector<int>{}) == static_cast<int>(random_bag.size()));
    }
}

static void test_sparse_trie_sorted_order() {
    SparseTrie<int> trie;
    for (int value = 50; value >= -50; --value) {
        trie.insert(vector<int>{value});
    }
    for (const SparseTrie<int>::Node& node : trie.nodes) {
        for (int index = 1; index < static_cast<int>(node.next.size()); ++index) {
            assert(node.next[index - 1].first < node.next[index].first);
        }
    }
}

static void test_hash_map_trie_composite_key() {
    using Key = pair<int, int>;
    HashMapTrie<Key> trie;
    vector<vector<Key>> bag;
    const vector<vector<Key>> sequences = {
        {},
        {{1, 2}},
        {{1, 2}, {3, 4}},
        {{-1, 5}, {6, -7}, {8, 9}},
        {{1, 2}}
    };
    for (const vector<Key>& sequence : sequences) {
        trie.insert(sequence);
        bag.push_back(sequence);
    }
    assert(trie.size() == static_cast<int>(bag.size()));
    assert(trie.count(vector<Key>{{1, 2}}) == 2);
    assert(trie.prefix_count(vector<Key>{{1, 2}}) == 3);
    assert(trie.prefix_count(vector<Key>{{-1, 5}, {6, -7}}) == 1);
    assert(!trie.contains(vector<Key>{{9, 9}}));
    assert(trie.erase(vector<Key>{{1, 2}}));
    assert(trie.count(vector<Key>{{1, 2}}) == 1);
}

static void test_sparse_trie() {
    run_sequence_trie_basic_test<LinearSparseTrie<int>>();
    run_sequence_trie_basic_test<SparseTrie<int>>();
    run_sequence_trie_basic_test<UnorderedMapTrie<int>>();
    run_sequence_trie_basic_test<HashMapTrie<int>>();

    run_sequence_trie_random_test<LinearSparseTrie<int>>(2U);
    run_sequence_trie_random_test<SparseTrie<int>>(3U);
    run_sequence_trie_random_test<UnorderedMapTrie<int>>(4U);
    run_sequence_trie_random_test<HashMapTrie<int>>(5U);

    test_sparse_trie_sorted_order();
    test_hash_map_trie_composite_key();

    const vector<vector<int>> sequences = {{1}, {1, 2}, {1, 2, 3}, {}, {5}};
    const vector<vector<int>> queries = {{}, {1}, {1, 2}, {2}};
    const vector<int> expected = {5, 3, 2, 0};
    assert((sparse_trie_prefix_count_queries<int>(sequences, queries) == expected));
    assert((linear_sparse_trie_prefix_count_queries<int>(sequences, queries) == expected));
    assert((unordered_map_trie_prefix_count_queries<int>(sequences, queries) == expected));
    assert((hash_map_trie_prefix_count_queries<int>(sequences, queries) == expected));
}

static int naive_ull_count(const vector<unsigned long long>& bag, unsigned long long x) {
    int result = 0;
    for (const unsigned long long value : bag) {
        if (value == x) {
            ++result;
        }
    }
    return result;
}

static int naive_ull_count_less(const vector<unsigned long long>& bag, unsigned long long x) {
    int result = 0;
    for (const unsigned long long value : bag) {
        if (value < x) {
            ++result;
        }
    }
    return result;
}

static int naive_ull_count_xor_less(const vector<unsigned long long>& bag, unsigned long long x, unsigned long long limit) {
    int result = 0;
    for (const unsigned long long value : bag) {
        if ((value ^ x) < limit) {
            ++result;
        }
    }
    return result;
}

static unsigned long long naive_ull_kth(vector<unsigned long long> bag, int k) {
    sort(bag.begin(), bag.end());
    return bag[k];
}

static unsigned long long naive_ull_min_xor(const vector<unsigned long long>& bag, unsigned long long x) {
    unsigned long long result = ULLONG_MAX;
    for (const unsigned long long value : bag) {
        result = min(result, value ^ x);
    }
    return result;
}

static unsigned long long naive_ull_max_xor(const vector<unsigned long long>& bag, unsigned long long x) {
    unsigned long long result = 0ULL;
    for (const unsigned long long value : bag) {
        result = max(result, value ^ x);
    }
    return result;
}

static void test_binary_trie() {
    mt19937 rng(3U);
    BinaryTrie<6> trie;
    vector<unsigned long long> bag;
    uniform_int_distribution<int> value_dist(0, 63);
    uniform_int_distribution<int> limit_dist(0, 80);
    uniform_int_distribution<int> op_dist(0, 3);
    for (int step = 0; step < 3000; ++step) {
        const int op = op_dist(rng);
        const unsigned long long x = value_dist(rng);
        if (op <= 1) {
            trie.insert(x);
            bag.push_back(x);
        } else if (op == 2) {
            const bool erased = trie.erase(x);
            bool naive_erased = false;
            for (auto it = bag.begin(); it != bag.end(); ++it) {
                if (*it == x) {
                    bag.erase(it);
                    naive_erased = true;
                    break;
                }
            }
            assert(erased == naive_erased);
        } else {
            assert(trie.count(x) == naive_ull_count(bag, x));
            assert(trie.count_less(x) == naive_ull_count_less(bag, x));
            const unsigned long long limit = limit_dist(rng);
            assert(trie.count_xor_less(x, limit) == naive_ull_count_xor_less(bag, x, limit));
        }
        assert(trie.size() == static_cast<int>(bag.size()));
        if (!bag.empty()) {
            uniform_int_distribution<int> k_dist(0, static_cast<int>(bag.size()) - 1);
            const int k = k_dist(rng);
            assert(trie.kth(k) == naive_ull_kth(bag, k));
            assert(trie.min_element() == naive_ull_kth(bag, 0));
            assert(trie.max_element() == naive_ull_kth(bag, static_cast<int>(bag.size()) - 1));
            assert(trie.min_xor(x) == naive_ull_min_xor(bag, x));
            assert(trie.max_xor(x) == naive_ull_max_xor(bag, x));
        }
    }

    const vector<unsigned long long> values = {3ULL, 10ULL, 5ULL, 25ULL, 2ULL, 8ULL};
    assert(binary_trie_max_pair_xor<6>(values) == 28ULL);
    assert(binary_trie_max_subarray_xor<6>(vector<unsigned long long>{8ULL, 1ULL, 2ULL, 12ULL}) == 15ULL);
    assert(binary_trie_count_subarray_xor_less<6>(vector<unsigned long long>{1ULL, 2ULL, 3ULL}, 3ULL) == 4LL);
    const vector<BinaryTrieQuery> queries = {
        {0, 5ULL, 0ULL, 0}, {0, 1ULL, 0ULL, 0}, {0, 7ULL, 0ULL, 0},
        {2, 5ULL, 0ULL, 0}, {3, 0ULL, 0ULL, 1}, {4, 6ULL, 0ULL, 0},
        {5, 4ULL, 0ULL, 0}, {6, 4ULL, 0ULL, 0}, {7, 0ULL, 0ULL, 0},
        {8, 0ULL, 0ULL, 0}, {9, 4ULL, 4ULL, 0}, {1, 1ULL, 0ULL, 0}
    };
    assert((binary_trie_dynamic_queries<3>(queries) == vector<unsigned long long>{1ULL, 5ULL, 2ULL, 1ULL, 5ULL, 1ULL, 7ULL, 2ULL, 1ULL}));
}

static void test_xor_lazy_binary_trie() {
    mt19937 rng(4U);
    XorLazyBinaryTrie<6> trie;
    vector<unsigned long long> bag;
    uniform_int_distribution<int> value_dist(0, 63);
    uniform_int_distribution<int> limit_dist(0, 80);
    uniform_int_distribution<int> op_dist(0, 4);
    for (int step = 0; step < 3000; ++step) {
        const int op = op_dist(rng);
        const unsigned long long x = value_dist(rng);
        if (op <= 1) {
            trie.insert(x);
            bag.push_back(x);
        } else if (op == 2) {
            const bool erased = trie.erase(x);
            bool naive_erased = false;
            for (auto it = bag.begin(); it != bag.end(); ++it) {
                if (*it == x) {
                    bag.erase(it);
                    naive_erased = true;
                    break;
                }
            }
            assert(erased == naive_erased);
        } else if (op == 3) {
            trie.xor_all(x);
            for (unsigned long long& value : bag) {
                value ^= x;
            }
        } else {
            assert(trie.count(x) == naive_ull_count(bag, x));
            assert(trie.count_less(x) == naive_ull_count_less(bag, x));
            const unsigned long long limit = limit_dist(rng);
            assert(trie.count_xor_less(x, limit) == naive_ull_count_xor_less(bag, x, limit));
        }
        assert(trie.size() == static_cast<int>(bag.size()));
        if (!bag.empty()) {
            uniform_int_distribution<int> k_dist(0, static_cast<int>(bag.size()) - 1);
            const int k = k_dist(rng);
            assert(trie.kth(k) == naive_ull_kth(bag, k));
            assert(trie.min_element() == naive_ull_kth(bag, 0));
            assert(trie.max_element() == naive_ull_kth(bag, static_cast<int>(bag.size()) - 1));
            assert(trie.min_xor(x) == naive_ull_min_xor(bag, x));
            assert(trie.max_xor(x) == naive_ull_max_xor(bag, x));
        }
    }

    const vector<XorLazyBinaryTrieQuery> queries = {
        {0, 1ULL, 0ULL, 0}, {0, 4ULL, 0ULL, 0}, {2, 3ULL, 0ULL, 0},
        {8, 0ULL, 0ULL, 0}, {9, 0ULL, 0ULL, 0}, {3, 2ULL, 0ULL, 0},
        {4, 0ULL, 0ULL, 1}, {5, 5ULL, 0ULL, 0}, {6, 0ULL, 0ULL, 0},
        {7, 0ULL, 0ULL, 0}, {10, 0ULL, 4ULL, 0}, {1, 7ULL, 0ULL, 0}
    };
    assert((xor_lazy_binary_trie_queries<3>(queries) == vector<unsigned long long>{2ULL, 7ULL, 1ULL, 7ULL, 1ULL, 2ULL, 7ULL, 1ULL, 1ULL}));
}

static unsigned long long naive_mex_from_present(const vector<int>& present) {
    for (int value = 0; value < static_cast<int>(present.size()); ++value) {
        if (present[value] == 0) {
            return value;
        }
    }
    return present.size();
}

static unsigned long long naive_mex_xor_from_present(const vector<int>& present, unsigned long long x) {
    vector<int> transformed(present.size(), 0);
    for (int value = 0; value < static_cast<int>(present.size()); ++value) {
        if (present[value] != 0) {
            const unsigned long long to = value ^ x;
            transformed[to] = 1;
        }
    }
    return naive_mex_from_present(transformed);
}

static void test_xor_mex_set() {
    mt19937 rng(5U);
    XorMexSet<5> mex_set;
    vector<int> present(32U, 0);
    uniform_int_distribution<int> value_dist(0, 31);
    uniform_int_distribution<int> op_dist(0, 5);
    int naive_size = 0;
    for (int step = 0; step < 3000; ++step) {
        const int op = op_dist(rng);
        const int value = value_dist(rng);
        const unsigned long long x = value;
        if (op == 0) {
            const bool inserted = mex_set.insert(x);
            const bool naive_inserted = present[value] == 0;
            if (naive_inserted) {
                present[value] = 1;
                ++naive_size;
            }
            assert(inserted == naive_inserted);
        } else if (op == 1) {
            const bool erased = mex_set.erase(x);
            const bool naive_erased = present[value] != 0;
            if (naive_erased) {
                present[value] = 0;
                --naive_size;
            }
            assert(erased == naive_erased);
        } else if (op == 2) {
            mex_set.xor_all(x);
            vector<int> next_present(32U, 0);
            for (int current = 0; current < 32; ++current) {
                if (present[current] != 0) {
                    const int to = current ^ value;
                    next_present[to] = 1;
                }
            }
            present.swap(next_present);
        } else if (op == 3) {
            assert(mex_set.contains(x) == (present[value] != 0));
        } else if (op == 4) {
            assert(mex_set.mex() == naive_mex_from_present(present));
        } else {
            assert(mex_set.mex_xor(x) == naive_mex_xor_from_present(present, x));
        }
        assert(mex_set.size() == naive_size);
        assert(mex_set.mex() == naive_mex_from_present(present));
        assert(mex_set.mex_xor(x) == naive_mex_xor_from_present(present, x));
    }

    XorMexSet<3> full_set;
    for (int value = 0; value < 8; ++value) {
        assert(full_set.insert(value));
    }
    assert(full_set.mex() == 8ULL);
    assert(full_set.mex_xor(3ULL) == 8ULL);
    const vector<unsigned long long> values = {0ULL, 1ULL, 3ULL, 3ULL};
    const vector<unsigned long long> queries = {0ULL, 1ULL, 2ULL};
    assert((xor_mex_static_queries<3>(values, queries) == vector<unsigned long long>{2ULL, 3ULL, 0ULL}));
    const vector<XorMexSetQuery> dynamic_queries = {
        {0, 0ULL}, {0, 1ULL}, {0, 3ULL}, {4, 0ULL}, {5, 1ULL},
        {2, 2ULL}, {4, 0ULL}, {3, 2ULL}, {1, 2ULL}, {4, 0ULL}
    };
    assert((xor_mex_dynamic_queries<3>(dynamic_queries) == vector<unsigned long long>{1ULL, 1ULL, 1ULL, 2ULL, 3ULL, 0ULL, 1ULL, 1ULL, 0ULL}));
}

static void test_persistent_binary_trie() {
    mt19937 rng(6U);
    uniform_int_distribution<int> value_dist(0, 31);
    const int n = 20;
    vector<unsigned long long> a(n);
    for (int index = 0; index < n; ++index) {
        a[index] = value_dist(rng);
    }

    PersistentBinaryTrie<5> trie;
    vector<int> roots(n + 1, 0);
    for (int index = 0; index < n; ++index) {
        roots[index + 1] = trie.insert(roots[index], a[index]);
    }

    for (int left = 0; left <= n; ++left) {
        for (int right = left; right <= n; ++right) {
            vector<unsigned long long> bag;
            for (int index = left; index < right; ++index) {
                bag.push_back(a[index]);
            }
            assert(trie.size(roots[left], roots[right]) == static_cast<int>(bag.size()));
            for (int x = 0; x < 40; ++x) {
                const unsigned long long ux = x;
                if (x < 32) {
                    assert(trie.count(roots[left], roots[right], ux) == naive_ull_count(bag, ux));
                    assert(trie.count_xor_less(roots[left], roots[right], ux, ux) == naive_ull_count_xor_less(bag, ux, ux));
                }
                assert(trie.count_less(roots[left], roots[right], ux) == naive_ull_count_less(bag, ux));
            }
            if (!bag.empty()) {
                for (int k = 0; k < static_cast<int>(bag.size()); ++k) {
                    assert(trie.kth(roots[left], roots[right], k) == naive_ull_kth(bag, k));
                }
                for (int x = 0; x < 32; ++x) {
                    const unsigned long long ux = x;
                    assert(trie.min_xor(roots[left], roots[right], ux) == naive_ull_min_xor(bag, ux));
                    assert(trie.max_xor(roots[left], roots[right], ux) == naive_ull_max_xor(bag, ux));
                }
            }
        }
    }

    const vector<RangeXorQuery> xor_queries = {{0, 3, 4ULL}, {2, 5, 1ULL}};
    const vector<RangeKthQuery> kth_queries = {{0, 5, 0}, {0, 5, 2}, {1, 4, 1}};
    const vector<RangeLessQuery> less_queries = {{0, 5, 4ULL}, {1, 4, 10ULL}};
    const vector<RangeXorLessQuery> xor_less_queries = {{0, 5, 1ULL, 4ULL}, {1, 4, 3ULL, 2ULL}};
    const vector<unsigned long long> b = {5ULL, 1ULL, 7ULL, 3ULL, 9ULL};
    assert((persistent_binary_trie_range_max_xor<5>(b, xor_queries) == vector<unsigned long long>{5ULL, 8ULL}));
    assert((persistent_binary_trie_range_min_xor<5>(b, xor_queries) == vector<unsigned long long>{1ULL, 2ULL}));
    assert((persistent_binary_trie_range_kth<5>(b, kth_queries) == vector<unsigned long long>{1ULL, 5ULL, 3ULL}));
    assert((persistent_binary_trie_range_count_less<5>(b, less_queries) == vector<int>{2, 3}));
    assert((persistent_binary_trie_range_count_xor_less<5>(b, xor_less_queries) == vector<int>{2, 1}));
}

static void test_mergeable_binary_trie() {
    MergeableBinaryTrie<5> trie;
    int root_a = 0;
    int root_b = 0;
    int root_c = 0;
    vector<unsigned long long> bag;
    for (const unsigned long long value : vector<unsigned long long>{1ULL, 7ULL, 3ULL}) {
        root_a = trie.insert(root_a, value);
        bag.push_back(value);
    }
    for (const unsigned long long value : vector<unsigned long long>{4ULL, 7ULL}) {
        root_b = trie.insert(root_b, value);
        bag.push_back(value);
    }
    root_a = trie.merge(root_a, root_b);
    for (const unsigned long long value : vector<unsigned long long>{0ULL, 31ULL}) {
        root_c = trie.insert(root_c, value);
        bag.push_back(value);
    }
    root_a = trie.merge(root_a, root_c);
    assert(trie.size(root_a) == static_cast<int>(bag.size()));
    for (int x = 0; x < 32; ++x) {
        const unsigned long long ux = x;
        assert(trie.count(root_a, ux) == naive_ull_count(bag, ux));
        assert(trie.count_less(root_a, ux) == naive_ull_count_less(bag, ux));
        assert(trie.min_xor(root_a, ux) == naive_ull_min_xor(bag, ux));
        assert(trie.max_xor(root_a, ux) == naive_ull_max_xor(bag, ux));
    }
    for (int k = 0; k < static_cast<int>(bag.size()); ++k) {
        assert(trie.kth(root_a, k) == naive_ull_kth(bag, k));
    }
    assert(trie.min_element(root_a) == 0ULL);
    assert(trie.max_element(root_a) == 31ULL);

    mt19937 rng(7U);
    const int n = 30;
    vector<unsigned long long> value(n);
    uniform_int_distribution<int> value_dist(0, 31);
    for (int index = 0; index < n; ++index) {
        value[index] = value_dist(rng);
    }
    vector<vector<int>> graph(n);
    for (int vertex = 1; vertex < n; ++vertex) {
        uniform_int_distribution<int> parent_dist(0, vertex - 1);
        const int parent = parent_dist(rng);
        graph[parent].push_back(vertex);
        graph[vertex].push_back(parent);
    }

    vector<int> parent(n, -1);
    vector<int> order;
    order.reserve(n);
    parent[0] = 0;
    vector<int> stack = {0};
    while (!stack.empty()) {
        const int vertex = stack.back();
        stack.pop_back();
        order.push_back(vertex);
        for (const int to : graph[vertex]) {
            if (to == parent[vertex]) {
                continue;
            }
            parent[to] = vertex;
            stack.push_back(to);
        }
    }
    vector<vector<unsigned long long>> subtree_values(n);
    for (int order_index = n - 1; order_index >= 0; --order_index) {
        const int vertex = order[order_index];
        subtree_values[vertex].push_back(value[vertex]);
        for (const int to : graph[vertex]) {
            if (parent[to] == vertex) {
                for (const unsigned long long child_value : subtree_values[to]) {
                    subtree_values[vertex].push_back(child_value);
                }
            }
        }
    }

    vector<SubtreeXorQuery> queries;
    for (int vertex = 0; vertex < n; ++vertex) {
        for (int x = 0; x < 32; x += 7) {
            queries.push_back({vertex, static_cast<unsigned long long>(x)});
        }
    }
    const vector<unsigned long long> answers = mergeable_binary_trie_subtree_max_xor<5>(value, graph, queries, 0);
    for (int index = 0; index < static_cast<int>(queries.size()); ++index) {
        const SubtreeXorQuery& query = queries[index];
        const vector<unsigned long long>& bag_ref = subtree_values[query.v];
        assert(answers[index] == naive_ull_max_xor(bag_ref, query.x));
    }
}

int main() {
    test_trie();
    test_sparse_trie();
    test_binary_trie();
    test_xor_lazy_binary_trie();
    test_xor_mex_set();
    test_persistent_binary_trie();
    test_mergeable_binary_trie();
    cout << "All tests passed\n";
    return 0;
}

#endif
