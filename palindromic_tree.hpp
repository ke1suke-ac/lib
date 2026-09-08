// ============================================================================
// Double-Ended Eertree (Palindromic Tree for Deque) - index-based, no new/delete
//   - Header-only, C++20 (gcc 12.2)
//
// 変更点（本版の特徴/今回の修正）:
//  - new/delete を廃止。ノードは vector<Node> に保持し、参照はすべて index（ポインタ不使用）。
//  - pop による削除ノードは free-list（再利用プール）へ格納し、次回の push で再利用。
//  - ルート odd/even はインデックス固定（0: odd(len=-1), 1: even(len=0)）。
//  - 両端 push/pop は償却 O(1)、問い合わせ（distinct / longest_prefix / longest_suffix）は O(1)。
//  - 【重要修正】par 探索・suffix-link 導出は “純正の suffix-link 降下” のみで実装（quick には頼らない）。
//    Eertree は suffix-link 降下だけでも償却 O(1) のため、理論性能は維持されます。
//  - surface-recording（presurf/sufsurf）は Nachia 系の式に忠実で、左右対称に実装。
// ============================================================================

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <algorithm>

// -------------------------------
// ライブラリ本体（index-based）
// -------------------------------
template<int SIGMA = 26, int MIN_CHAR = 'a'>
struct double_ended_eertree {
    // ---- 内部ノード（index 参照）----
    struct Node {
        int parent = 0;                        // 親（両端 1 文字ずつ短くした回文）
        int link   = 0;                        // suffix link（最大の真の回文ボーダ）
        std::array<int, SIGMA> next;           // 遷移: c P c（-1: なし）
        int len = 0;                           // 回文長
        int cnt = 0;                           // 表面上の生存カウント（pop で 0 になれば削除候補）
        int linkcnt = 0;                       // 自分を link に持つ子の数（生存管理）
        Node() : next{} { next.fill(-1); }
        void clear() { parent = link = 0; len = 0; cnt = 0; linkcnt = 0; next.fill(-1); }
    };

    // ---- deque 上の各位置に保持する情報（index 参照）----
    struct Slot {
        unsigned char ch;   // その位置の文字
        int presurf;        // その位置での「接頭 surface」（ノード index）
        int sufsurf;        // その位置での「接尾 surface」（ノード index）
    };

    // ---- データ ----
    std::vector<Node> nodes_;       // ノード配列（0: odd, 1: even）
    std::vector<int>  free_;        // free-list（再利用プール）
    std::deque<Slot>  deq_;         // 本文（文字＋surface）
    int odd_  = 0;                  // odd root index (len=-1)
    int even_ = 1;                  // even root index (len=0)
    long long num_nodes_ = 0;       // 「存在する」回文ノード数（根 2 つを除く）

    // ---- ヘルパ：文字をインデックス化 ----
    static inline int enc(unsigned char ch) {
        int idx = int(ch) - MIN_CHAR;
        assert(0 <= idx && idx < SIGMA);
        return idx;
    }

    // ---- ノード確保（free-list 再利用）----
    int alloc_node() {
        if (!free_.empty()) {
            int id = free_.back(); free_.pop_back();
            nodes_[id].clear();
            return id;
        }
        nodes_.emplace_back();
        return (int)nodes_.size() - 1;
    }
    void free_node(int id) {
        nodes_[id].clear();
        free_.push_back(id);
    }

    // ---- コンストラクタ ----
    double_ended_eertree() {
        nodes_.reserve(8);
        nodes_.emplace_back(); // 0 : odd root
        nodes_.emplace_back(); // 1 : even root
        odd_  = 0;
        even_ = 1;
        // odd root
        nodes_[odd_].len    = -1;
        nodes_[odd_].parent = odd_;
        nodes_[odd_].link   = odd_;
        // even root
        nodes_[even_].len    = 0;
        nodes_[even_].parent = odd_;
        nodes_[even_].link   = odd_;
        num_nodes_ = 0;
    }

    // ---- 公開 API ----
    int size() const noexcept { return (int)deq_.size(); }
    std::string str() const {
        std::string s; s.resize(deq_.size());
        for (size_t i = 0; i < deq_.size(); ++i) s[i] = char(deq_[i].ch);
        return s;
    }
    // 異なる回文部分文字列の個数（根 2 つは除く）— O(1)
    long long distinct() const noexcept { return num_nodes_; }
    // 最長接尾回文長 — O(1)
    int longest_suffix() const noexcept { return deq_.empty() ? 0 : nodes_[deq_.back().sufsurf].len; }
    // 最長接頭回文長 — O(1)
    int longest_prefix() const noexcept { return deq_.empty() ? 0 : nodes_[deq_.front().presurf].len; }

    // 右に 1 文字追加（償却 O(1)）
    void push_back(char ch_raw) {
        unsigned char ch = (unsigned char)ch_raw;
        int c = enc(ch);
        const int n_before = (int)deq_.size(); // 追加前の長さ

        // par を「suffix-link 降下」で厳密に求める
        int par = (deq_.empty() ? odd_ : deq_.back().sufsurf);
        while (true) {
            int L = nodes_[par].len;
            if (L == -1) break;
            if (L < n_before && deq_[n_before - L - 1].ch == ch) break;
            par = nodes_[par].link;
        }

        int v = nodes_[par].next[c];
        int w = even_; // link 候補
        if (v == -1) {
            // --- 新規ノード生成 ---
            v = alloc_node();
            nodes_[v].parent = par;
            nodes_[v].len    = nodes_[par].len + 2;

            // 厳密な suffix link 構築（suffix-link 降下）
            if (nodes_[v].len == 1) {
                w = even_;
            } else {
                int u = nodes_[par].link;
                while (true) {
                    int Lu = nodes_[u].len;
                    if (Lu == -1) { w = nodes_[odd_].next[c]; break; }
                    if (Lu < n_before && deq_[n_before - Lu - 1].ch == ch) { w = nodes_[u].next[c]; break; }
                    u = nodes_[u].link;
                }
                assert(w != -1); // 理論上必ず存在
            }
            nodes_[v].link = w;
            nodes_[w].linkcnt += 1;

            // スロット追加（surface は一旦 even）
            deq_.push_back(Slot{ ch, even_, even_ });

            // par→v の遷移確定
            nodes_[par].next[c] = v;

            num_nodes_ += 1;
        } else {
            // --- 既存ノードへ ---
            deq_.push_back(Slot{ ch, even_, even_ });
            w = nodes_[v].link;
        }

        const int n = (int)deq_.size(); // 追加後
        // --- surface の差し替え ---
        deq_[n - 1].sufsurf             = v;
        deq_[n - nodes_[v].len].presurf = v;

        // push では「もし idx に w が残っていたら消す」
        if (nodes_[w].len >= 1) {
            int idx = n - nodes_[v].len + nodes_[w].len - 1;
            if (0 <= idx && idx < n && deq_[idx].sufsurf == w) {
                deq_[idx].sufsurf = even_;
            }
        }
        nodes_[v].cnt += 1;
    }

    // 左に 1 文字追加（償却 O(1)）
    void push_front(char ch_raw) {
        unsigned char ch = (unsigned char)ch_raw;
        int c = enc(ch);
        const int n_before = (int)deq_.size(); // 追加前

        // par を「suffix-link 降下」で厳密に求める（前側）
        int par = (deq_.empty() ? odd_ : deq_.front().presurf);
        while (true) {
            int L = nodes_[par].len;
            if (L == -1) break;
            if (L < n_before && deq_[L].ch == ch) break;
            par = nodes_[par].link;
        }

        int v = nodes_[par].next[c];
        int w = even_;
        if (v == -1) {
            v = alloc_node();
            nodes_[v].parent = par;
            nodes_[v].len    = nodes_[par].len + 2;

            // 厳密な suffix link 構築（前側、suffix-link 降下）
            if (nodes_[v].len == 1) {
                w = even_;
            } else {
                int u = nodes_[par].link;
                while (true) {
                    int Lu = nodes_[u].len;
                    if (Lu == -1) { w = nodes_[odd_].next[c]; break; }
                    if (Lu < n_before && deq_[Lu].ch == ch) { w = nodes_[u].next[c]; break; }
                    u = nodes_[u].link;
                }
                assert(w != -1);
            }
            nodes_[v].link = w;
            nodes_[w].linkcnt += 1;

            deq_.push_front(Slot{ ch, even_, even_ });

            nodes_[par].next[c] = v;

            num_nodes_ += 1;
        } else {
            deq_.push_front(Slot{ ch, even_, even_ });
            w = nodes_[v].link;
        }

        const int n = (int)deq_.size(); // 追加後
        // surface
        deq_[0].presurf                 = v;
        deq_[nodes_[v].len - 1].sufsurf = v;

        // push_front では「もし idx に w が残っていたら消す」
        if (nodes_[w].len >= 1) {
            int idx = nodes_[v].len - nodes_[w].len;
            if (0 <= idx && idx < n && deq_[idx].presurf == w) {
                deq_[idx].presurf = even_;
            }
        }
        nodes_[v].cnt += 1;
    }

    // 右から 1 文字削除（償却 O(1)）
    void pop_back() {
        assert(!deq_.empty());
        int v = deq_.back().sufsurf;       // いま消える最長接尾回文
        unsigned char backChar = deq_.back().ch;
        int w = nodes_[v].link;            // v の link

        const int n = (int)deq_.size();
        if (nodes_[v].len >= 2) {
            int idx = n - nodes_[v].len + nodes_[w].len - 1;
            int pos = n - nodes_[v].len;
            if (0 <= idx && idx < n && 0 <= pos && pos < n) {
                // pop：必要なら w を復元、そうでなければ Even
                if (nodes_[deq_[idx].sufsurf].len < nodes_[w].len) {
                    deq_[idx].sufsurf = w;
                    deq_[pos].presurf = w;
                } else {
                    deq_[pos].presurf = even_;
                }
            }
        } else {
            int pos = n - nodes_[v].len; // v->len は 1 or 0
            if (0 <= pos && pos < n) deq_[pos].presurf = even_;
        }

        nodes_[v].cnt -= 1;
        if (nodes_[v].linkcnt == 0 && nodes_[v].cnt == 0) {
            int p = nodes_[v].parent;
            nodes_[p].next[enc(backChar)] = -1;
            nodes_[w].linkcnt -= 1;
            free_node(v);
            num_nodes_ -= 1;
        }
        deq_.pop_back();
    }

    // 左から 1 文字削除（償却 O(1)）
    void pop_front() {
        assert(!deq_.empty());
        int v = deq_.front().presurf;
        unsigned char frontChar = deq_.front().ch;
        int w = nodes_[v].link;

        const int n = (int)deq_.size();
        if (nodes_[v].len >= 2) {
            int idx = nodes_[v].len - nodes_[w].len;
            int pos = nodes_[v].len - 1;
            if (0 <= idx && idx < n && 0 <= pos && pos < n) {
                // 左側も対称：必要なら w を復元、そうでなければ Even
                if (nodes_[deq_[idx].presurf].len < nodes_[w].len) {
                    deq_[idx].presurf = w;
                    deq_[pos].sufsurf = w;
                } else {
                    deq_[pos].sufsurf = even_;
                }
            }
        } else {
            int pos = nodes_[v].len - 1; // v->len は 1 or 0
            if (0 <= pos && pos < n) deq_[pos].sufsurf = even_;
        }

        nodes_[v].cnt -= 1;
        if (nodes_[v].linkcnt == 0 && nodes_[v].cnt == 0) {
            int p = nodes_[v].parent;
            nodes_[p].next[enc(frontChar)] = -1;
            nodes_[w].linkcnt -= 1;
            free_node(v);
            num_nodes_ -= 1;
        }
        deq_.pop_front();
    }
};

// #endif // DOUBLE_ENDED_EERTREE_HPP


// ============================================================================
// テスト（単体 + ランダム + シナリオ）
//  - まとめて無効化しやすいよう #if 1 .. #endif で囲む
//  - ナイーブ検証（small）を交えて正しさを担保
//  - 多数のエッジケースとシナリオを追加
// ============================================================================

#if 0
#include <chrono>

// チェックマクロ（失敗時に詳細を出力）
#define CHECK_EQ(msg, a, b, extra) \
    do { \
        auto _va = (a); auto _vb = (b); \
        if (!((_va) == (_vb))) { \
            std::cerr << "[CHECK_EQ FAIL] " << msg << "\n"; \
            std::cerr << "  expected: " << #a << " == " << #b << "\n"; \
            std::cerr << "  values  : " << _va << " vs " << _vb << "\n"; \
            extra; \
            std::abort(); \
        } \
    } while (0)

// ---- ナイーブ：回文かどうか ----
static bool is_pal(const std::string& s, int l, int r) {
    while (l < r) if ((unsigned char)s[l++] != (unsigned char)s[r--]) return false;
    return true;
}

// ---- ナイーブ：distinct（種類数）と prefix/suffix の最長長 ----
static long long naive_distinct(const std::string& s) {
    const int n = (int)s.size();
    std::unordered_set<std::string> st;
    st.reserve((size_t)n * 2 + 3);
    for (int i = 0; i < n; ++i)
        for (int j = i; j < n; ++j)
            if (is_pal(s,i,j)) st.insert(s.substr(i, j-i+1));
    return (long long)st.size();
}
static int naive_longest_prefix(const std::string& s) {
    const int n = (int)s.size();
    for (int L = n; L >= 1; --L) if (is_pal(s,0,L-1)) return L;
    return 0;
}
static int naive_longest_suffix(const std::string& s) {
    const int n = (int)s.size();
    for (int L = n; L >= 1; --L) if (is_pal(s,n-L,n-1)) return L;
    return 0;
}

// ---- ユーティリティ: 実装の簡単検査 ----
template<int SIGMA, int MIN_CHAR>
static void assert_invariants(const double_ended_eertree<SIGMA,MIN_CHAR>& T) {
    CHECK_EQ("invariant: 0<=longest_prefix<=size", true,
        (0 <= T.longest_prefix() && T.longest_prefix() <= T.size()), {
            std::cerr << "  s=" << T.str() << "\n";
        });
    CHECK_EQ("invariant: 0<=longest_suffix<=size", true,
        (0 <= T.longest_suffix() && T.longest_suffix() <= T.size()), {
            std::cerr << "  s=" << T.str() << "\n";
        });
}

// ----------------------------
// 基本テスト：固定列 + エッジケース拡充
// ----------------------------
static void test_basic_fixed_and_edges() {
    using Deq = double_ended_eertree<26,'a'>;

    // 0) 空列
    {
        Deq T;
        std::string s;
        CHECK_EQ("empty distinct", 0LL, T.distinct(), {});
        CHECK_EQ("empty preflen",  0,   T.longest_prefix(), {});
        CHECK_EQ("empty sufflen",  0,   T.longest_suffix(), {});
        assert_invariants(T);
    }

    // 1) 右だけ追加 → 部分的に pop_back
    {
        Deq T;
        std::string s;
        std::string t = "abacaba";
        for (char c : t) { T.push_back(c); s.push_back(c); assert_invariants(T); }

        CHECK_EQ("distinct mismatch (#1)",
            naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_prefix mismatch (#1)",
            naive_longest_prefix(s), T.longest_prefix(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_suffix mismatch (#1)",
            naive_longest_suffix(s), T.longest_suffix(), { std::cerr << "  s=" << s << "\n"; });

        // pop_back で 1 文字ずつ戻す（"abacab" の prefix=3 を含む）
        for (int k = 0; k < 3; ++k) {
            T.pop_back(); s.pop_back(); assert_invariants(T);
            CHECK_EQ("distinct mismatch (#1-pop)",
                naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
            CHECK_EQ("longest_prefix mismatch (#1-pop)",
                naive_longest_prefix(s), T.longest_prefix(), { std::cerr << "  s=" << s << "\n"; });
            CHECK_EQ("longest_suffix mismatch (#1-pop)",
                naive_longest_suffix(s), T.longest_suffix(), { std::cerr << "  s=" << s << "\n"; });
        }

        // さらに全て削除して空に戻す → 再利用
        while (!s.empty()) { T.pop_back(); s.pop_back(); assert_invariants(T); }
        CHECK_EQ("empty after pops distinct", 0LL, T.distinct(), {});
        // 再利用（完全回文）
        for (char c : std::string("racecar")) { T.push_back(c); s.push_back(c); assert_invariants(T); }
        CHECK_EQ("reuse palindrome preflen", (int)s.size(), T.longest_prefix(), { std::cerr << " s="<<s<<"\n"; });
        CHECK_EQ("reuse palindrome sufflen", (int)s.size(), T.longest_suffix(), { std::cerr << " s="<<s<<"\n"; });
        CHECK_EQ("reuse palindrome distinct", naive_distinct(s), T.distinct(), {});
    }

    // 2) 左だけ追加 → 部分的に pop_front
    {
        Deq T;
        std::string s;
        std::string t = "abacaba";
        for (char c : t) { T.push_front(c); s.insert(s.begin(), c); assert_invariants(T); }

        CHECK_EQ("distinct mismatch (#2)",
            naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_prefix mismatch (#2)",
            naive_longest_prefix(s), T.longest_prefix(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_suffix mismatch (#2)",
            naive_longest_suffix(s), T.longest_suffix(), { std::cerr << "  s=" << s << "\n"; });

        // pop_front で戻す
        for (int k = 0; k < 3; ++k) {
            T.pop_front(); s.erase(s.begin()); assert_invariants(T);
            CHECK_EQ("distinct mismatch (#2-pop)",
                naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
            CHECK_EQ("longest_prefix mismatch (#2-pop)",
                naive_longest_prefix(s), T.longest_prefix(), { std::cerr << "  s=" << s << "\n"; });
            CHECK_EQ("longest_suffix mismatch (#2-pop)",
                naive_longest_suffix(s), T.longest_suffix(), { std::cerr << "  s=" << s << "\n"; });
        }
    }

    // 3) 両端交互（小サイズ）
    {
        Deq T;
        std::string s;
        T.push_back('a'); s.push_back('a'); assert_invariants(T);
        T.push_back('a'); s.push_back('a'); assert_invariants(T);
        T.push_front('a'); s.insert(s.begin(), 'a'); assert_invariants(T);
        T.push_back('b'); s.push_back('b'); assert_invariants(T);
        T.push_front('b'); s.insert(s.begin(), 'b'); assert_invariants(T);
        T.push_back('b'); s.push_back('b'); assert_invariants(T);

        CHECK_EQ("distinct mismatch (#3)", naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_prefix mismatch (#3)", naive_longest_prefix(s), T.longest_prefix(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_suffix mismatch (#3)", naive_longest_suffix(s), T.longest_suffix(), { std::cerr << "  s=" << s << "\n"; });

        // 交互に pop
        T.pop_back(); s.pop_back(); assert_invariants(T);
        T.pop_front(); s.erase(s.begin()); assert_invariants(T);
        CHECK_EQ("distinct mismatch (#3-pop)", naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_prefix mismatch (#3-pop)", naive_longest_prefix(s), T.longest_prefix(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("longest_suffix mismatch (#3-pop)", naive_longest_suffix(s), T.longest_suffix(), { std::cerr << "  s=" << s << "\n"; });
    }

    // 4) 常に回文になる操作（左右対称）
    {
        Deq T;
        std::string s;
        std::vector<std::pair<char,bool>> ops = {
            {'a',true}, {'b',true}, {'a',true}, {'b',false}, {'a',false}
        };
        for (auto [c, right] : ops) {
            if (right) { T.push_back(c); s.push_back(c); }
            else       { T.push_front(c); s.insert(s.begin(), c); }
            std::string rs = s; std::reverse(rs.begin(), rs.end());
            if (s == rs) {
                CHECK_EQ("suffix==n (#4)", (int)s.size(), T.longest_suffix(), { std::cerr << "  s=" << s << "\n"; });
                CHECK_EQ("prefix==n (#4)", (int)s.size(), T.longest_prefix(), { std::cerr << "  s=" << s << "\n"; });
            }
            assert_invariants(T);
        }
        CHECK_EQ("distinct mismatch (#4)", naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
    }

    // 5) 一様文字列（x^n の逐次検証：毎 step で +1）
    {
        Deq T;
        std::string s;
        for (int i = 1; i <= 30; ++i) {
            T.push_back('x'); s.push_back('x'); assert_invariants(T);
            CHECK_EQ("uniform incremental distinct (#5 step)", naive_distinct(s), T.distinct(), {
                std::cerr << "  step=" << i << " s=" << s << " len=" << s.size() << "\n";
            });
            CHECK_EQ("uniform preflen (#5 step)", naive_longest_prefix(s), T.longest_prefix(), {});
            CHECK_EQ("uniform sufflen (#5 step)", naive_longest_suffix(s), T.longest_suffix(), {});
        }

        // まとめチェック
        CHECK_EQ("distinct mismatch (#5 total)", naive_distinct(s), T.distinct(), {
            std::cerr << "  s=" << s << " len=" << s.size() << "\n";
        });

        // 途中まで pop_front/pop_back 混在で削る → 再検証
        for (int i = 0; i < 10; ++i) { T.pop_front(); s.erase(s.begin()); assert_invariants(T); }
        for (int i = 0; i < 5;  ++i) { T.pop_back();  s.pop_back();       assert_invariants(T); }
        CHECK_EQ("distinct mismatch (#5-2)", naive_distinct(s), T.distinct(), { std::cerr << "  s=" << s << "\n"; });
        CHECK_EQ("preflen mismatch (#5-2)",  naive_longest_prefix(s), T.longest_prefix(), {});
        CHECK_EQ("sufflen mismatch (#5-2)",  naive_longest_suffix(s), T.longest_suffix(), {});
    }

    // 6) 全て異なる文字列
    {
        Deq T; std::string s;
        for (int i = 0; i < 26; ++i) { char c = (char)('a' + i); T.push_back(c); s.push_back(c); assert_invariants(T); }
        CHECK_EQ("distinct mismatch (#6)", naive_distinct(s), T.distinct(), {});
    }

    // 7) 交互文字列 "abababab..."（偶数長で偶回文が多数）
    {
        Deq T; std::string s;
        for (int i = 0; i < 30; ++i) {
            char c = (i % 2 ? 'b' : 'a');
            T.push_back(c); s.push_back(c); assert_invariants(T);
        }
        CHECK_EQ("distinct mismatch (#7)", naive_distinct(s), T.distinct(), {});
        // 部分削除後も検査
        for (int i = 0; i < 7; ++i) { T.pop_front(); s.erase(s.begin()); assert_invariants(T); }
        for (int i = 0; i < 6; ++i) { T.pop_back();  s.pop_back();       assert_invariants(T); }
        CHECK_EQ("distinct mismatch (#7-2)", naive_distinct(s), T.distinct(), {});
    }

    // 8) “中心が動く”テスト: 右に伸ばし続け、一定周期で左を詰める
    {
        Deq T; std::string s;
        for (int i = 0; i < 120; ++i) {
            char c = (char)('a' + (i % 3));
            T.push_back(c); s.push_back(c); assert_invariants(T);
            if (i % 5 == 0 && !s.empty()) { T.pop_front(); s.erase(s.begin()); assert_invariants(T); }
            CHECK_EQ("rolling distinct (#8)", naive_distinct(s), T.distinct(), {});
            CHECK_EQ("rolling preflen (#8)", naive_longest_prefix(s), T.longest_prefix(), {});
            CHECK_EQ("rolling sufflen (#8)", naive_longest_suffix(s), T.longest_suffix(), {});
        }
    }
}

// ----------------------------
// ランダム操作テスト（push/pop 混在、逐次検証）
// ----------------------------
static void test_random_ops(int trials = 300) {
    using Deq = double_ended_eertree<26,'a'>;
    std::mt19937_64 rng(123456789);

    for (int it = 0; it < trials; ++it) {
        Deq T;
        std::string s;
        int steps = 400; // 適度な回数（やや強め）
        for (int k = 0; k < steps; ++k) {
            int op = int(rng() % 6); // 操作の幅を増やす（push 3/6, pop 3/6）
            if (op == 0) {
                char c = char('a' + int(rng()%3));
                T.push_back(c); s.push_back(c);
            } else if (op == 1) {
                char c = char('a' + int(rng()%3));
                T.push_front(c); s.insert(s.begin(), c);
            } else if (op == 2) {
                char c = char('a' + int(rng()%2)); // 2色で偏りを作る
                T.push_back(c); s.push_back(c);
            } else if (op == 3) {
                if (!s.empty()) { T.pop_back(); s.pop_back(); }
                else { char c = 'a'; T.push_back(c); s.push_back(c); }
            } else if (op == 4) {
                if (!s.empty()) { T.pop_front(); s.erase(s.begin()); }
                else { char c = 'b'; T.push_front(c); s.insert(s.begin(), c); }
            } else {
                // 交互に前後ポップ（空なら push）
                if (!s.empty()) {
                    if (k % 2) { T.pop_front(); s.erase(s.begin()); }
                    else       { T.pop_back();  s.pop_back();       }
                } else {
                    char c = 'a'; T.push_back(c); s.push_back(c);
                }
            }
            // 逐次検証（小規模長なのでナイーブで十分）
            long long d1 = naive_distinct(s);
            int lp = naive_longest_prefix(s);
            int ls = naive_longest_suffix(s);

            if (!(d1 == T.distinct() && lp == T.longest_prefix() && ls == T.longest_suffix())) {
                std::cerr << "[random_ops FAIL] it=" << it << " step=" << k << "\n";
                std::cerr << "  s=" << s << "\n";
                std::cerr << "  distinct naive=" << d1 << " tree=" << T.distinct() << "\n";
                std::cerr << "  preflen  naive=" << lp << " tree=" << T.longest_prefix() << "\n";
                std::cerr << "  sufflen  naive=" << ls << " tree=" << T.longest_suffix() << "\n";
                std::abort();
            }
            assert_invariants(T);
        }
    }
}

// ----------------------------
// 追加シナリオテスト（できるだけ多く）
// ----------------------------
static void scenario_usecases_extra() {
    using Deq = double_ended_eertree<26,'a'>;

    // S1) “鏡像”シナリオ: 右に作った文字列と、左から逆順に作った文字列で同じ結果か（最終状態）
    {
        std::string t = "abacabbacaba"; // そこそこ回文を含む列
        Deq A, B; std::string sA, sB;
        for (char c : t) { A.push_back(c); sA.push_back(c); }
        for (int i = (int)t.size()-1; i >= 0; --i) { B.push_front(t[i]); sB.insert(sB.begin(), t[i]); }

        CHECK_EQ("S1 distinct", naive_distinct(t), A.distinct(), {});
        CHECK_EQ("S1 distinct (mirror)", naive_distinct(t), B.distinct(), {});
        CHECK_EQ("S1 preflen", naive_longest_prefix(t), A.longest_prefix(), {});
        CHECK_EQ("S1 preflen (mirror)", naive_longest_prefix(t), B.longest_prefix(), {});
        CHECK_EQ("S1 sufflen", naive_longest_suffix(t), A.longest_suffix(), {});
        CHECK_EQ("S1 sufflen (mirror)", naive_longest_suffix(t), B.longest_suffix(), {});
    }

    // S2) “対称 push/pop”: 同じだけ左右に push した後、逆順に同数 pop する
    {
        Deq T; std::string s;
        std::string left = "abcabc";
        std::string right= "bcabca";
        for (char c : left)  { T.push_front(c); s.insert(s.begin(), c); }
        for (char c : right) { T.push_back(c);  s.push_back(c);        }

        CHECK_EQ("S2 distinct", naive_distinct(s), T.distinct(), {});
        CHECK_EQ("S2 preflen",  naive_longest_prefix(s), T.longest_prefix(), {});
        CHECK_EQ("S2 sufflen",  naive_longest_suffix(s), T.longest_suffix(), {});

        // pop: 右→左→右→左… と往復
        for (int i = 0; i < 6; ++i) { T.pop_back(); s.pop_back(); CHECK_EQ("S2 step", naive_distinct(s), T.distinct(), {}); }
        for (int i = 0; i < 6; ++i) { T.pop_front(); s.erase(s.begin()); CHECK_EQ("S2 step2", naive_distinct(s), T.distinct(), {}); }

        CHECK_EQ("S2 empty distinct", 0LL, T.distinct(), {});
        CHECK_EQ("S2 empty preflen",  0,   T.longest_prefix(), {});
        CHECK_EQ("S2 empty sufflen",  0,   T.longest_suffix(), {});
    }

    // S3) “ホットスポット”: 片側集中 + 反対側ノイズ
    {
        Deq T; std::string s;
        for (int rep = 0; rep < 5; ++rep) {
            for (int i = 0; i < 20; ++i) { T.push_back('z'); s.push_back('z'); }
            for (int j = 0; j < 5;  ++j) { char c = (j%2 ? 'a' : 'b'); T.push_front(c); s.insert(s.begin(), c); }
            CHECK_EQ("S3 distinct", naive_distinct(s), T.distinct(), {});
            CHECK_EQ("S3 preflen",  naive_longest_prefix(s), T.longest_prefix(), {});
            CHECK_EQ("S3 sufflen",  naive_longest_suffix(s), T.longest_suffix(), {});
        }
        // まとめて削る
        for (int i = 0; i < 17 && !s.empty(); ++i) { T.pop_back(); s.pop_back(); }
        for (int i = 0; i < 7  && !s.empty(); ++i) { T.pop_front(); s.erase(s.begin()); }
        CHECK_EQ("S3 after pops distinct", naive_distinct(s), T.distinct(), {});
    }

    // S4) “短い極限”: size=1,2,3 の全操作列（SIGMA=2）を毎回 base から再構築
    {
        using Deq2 = double_ended_eertree<2,'a'>;
        std::vector<std::string> seeds = {"", "a", "b", "aa", "ab", "ba", "bb", "aba", "aab", "abb"};
        std::vector<std::string> ops   = {"pf a","pf b","pb a","pb b","popf","popb"};

        for (auto base : seeds) {
            for (size_t i = 0; i < ops.size(); ++i) {
                Deq2 Tc; std::string sc;
                for (char c : base) { Tc.push_back(c); sc.push_back(c); }

                auto apply = [&](const std::string& op) {
                    if (op=="pf a") { Tc.push_front('a'); sc.insert(sc.begin(),'a'); }
                    else if (op=="pf b") { Tc.push_front('b'); sc.insert(sc.begin(),'b'); }
                    else if (op=="pb a") { Tc.push_back('a'); sc.push_back('a'); }
                    else if (op=="pb b") { Tc.push_back('b'); sc.push_back('b'); }
                    else if (op=="popf") { if(!sc.empty()) { Tc.pop_front(); sc.erase(sc.begin()); } }
                    else if (op=="popb") { if(!sc.empty()) { Tc.pop_back(); sc.pop_back(); } }
                };
                apply(ops[i]);

                CHECK_EQ("S4 distinct after op", naive_distinct(sc), Tc.distinct(), {
                    std::cerr << "  base="<<base<<" op="<<ops[i]<<" s="<<sc<<"\n";
                });
                CHECK_EQ("S4 preflen after op", naive_longest_prefix(sc), Tc.longest_prefix(), {});
                CHECK_EQ("S4 sufflen after op", naive_longest_suffix(sc), Tc.longest_suffix(), {});
            }
        }
    }

    // S5) “全削除→再構築の繰返し”: クリア後の挙動
    {
        Deq T; std::string s;
        for (int round = 0; round < 5; ++round) {
            std::string t = (round%2==0 ? "banana" : "civic");
            for (char c : t) { T.push_back(c); s.push_back(c); }
            CHECK_EQ("S5 distinct build", naive_distinct(s), T.distinct(), {});
            while (!s.empty()) { T.pop_front(); s.erase(s.begin()); }
            CHECK_EQ("S5 distinct cleared", 0LL, T.distinct(), {});
        }
    }
}

// ----------------------------
// メイン
// ----------------------------
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    test_basic_fixed_and_edges();
    test_random_ops();
    scenario_usecases_extra();

    std::cout << "All tests passed.\n";
    return 0;
}
#endif
