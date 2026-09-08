#pragma once
#include <bits/stdc++.h>

// suffix automaton の標準構築、出現回数付き構築、文字列集合用構築、
// 大 alphabet 用構築、rollback 構築と代表的な競技プログラミング用途を提供する

// ============================================================================
// SuffixAutomaton と対応 solver
// ============================================================================

// 固定 alphabet 上の標準 suffix automaton
// 構築 O(n)、状態数 O(n)、メモリ O(Kn)
template <int K = 26, int BASE = 'a'>
struct SuffixAutomaton {
    struct State {
        std::array<int, K> next;
        int link = -1;
        int len = 0;

        // 全遷移を未使用状態で初期化する O(K)
        State() {
            next.fill(-1);
        }
    };

    std::vector<State> states;
    int last = 0;

    // 空の suffix automaton を構築する O(1)
    explicit SuffixAutomaton(int max_length = 0) {
        reset(max_length);
    }

    // 内容を破棄して空の suffix automaton に戻す O(1)
    void reset(int max_length = 0) {
        states.clear();
        if (max_length > 0) states.reserve(static_cast<std::size_t>(max_length) * 2);
        states.emplace_back();
        last = 0;
    }

    // 末尾へ1文字追加して新しい last 状態を返す ならし O(1)
    int extend(char ch) {
        const int c = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= c && c < K);

        // 新しい文字列全体を表す状態を作る
        const int cur = static_cast<int>(states.size());
        states.emplace_back();
        states[cur].len = states[last].len + 1;

        // 新しく現れた suffix に対応する遷移を追加する
        int p = last;
        while (p != -1 && states[p].next[c] == -1) {
            states[p].next[c] = cur;
            p = states[p].link;
        }

        // 既存遷移との関係から suffix link を決める
        if (p == -1) {
            states[cur].link = 0;
        } else {
            const int q = states[p].next[c];
            if (states[p].len + 1 == states[q].len) {
                states[cur].link = q;
            } else {
                // q が表す長さ範囲を clone で分割する
                const int clone = static_cast<int>(states.size());
                states.push_back(states[q]);
                states[clone].len = states[p].len + 1;

                while (p != -1 && states[p].next[c] == q) {
                    states[p].next[c] = clone;
                    p = states[p].link;
                }

                states[q].link = clone;
                states[cur].link = clone;
            }
        }

        last = cur;
        return cur;
    }

    // 文字列全体から suffix automaton を構築する O(n)
    void build(std::string_view s) {
        reset(static_cast<int>(s.size()));
        for (char ch : s) extend(ch);
    }
};

// 想定ユースケース: pattern の存在確認後に、到達状態の len や独自に持たせた DP 値を参照する
// 入力: sam は文字列 S から構築済みの標準 SAM、pattern は探索する文字列
// 出力: root から pattern を読んだ到達状態番号、読めなければ -1、空文字列なら root の 0
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE>
int sam_find_state(const SuffixAutomaton<K, BASE>& sam, std::string_view pattern) {
    int v = 0;
    for (char ch : pattern) {
        const int c = static_cast<unsigned char>(ch) - BASE;
        if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
        v = sam.states[v].next[c];
    }
    return v;
}

// 想定ユースケース: クエリ文字列が構築元文字列 S の連続部分文字列かを判定する
// 入力: sam は文字列 S から構築済みの標準 SAM、pattern は判定する文字列
// 出力: pattern が S の部分文字列なら true、そうでなければ false、空文字列は true
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE>
bool sam_contains(const SuffixAutomaton<K, BASE>& sam, std::string_view pattern) {
    const auto find_state = [&]() {
        int v = 0;
        for (char ch : pattern) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
            v = sam.states[v].next[c];
        }
        return v;
    };

    return find_state() != -1;
}

// 想定ユースケース: 構築元文字列 S に含まれる異なる非空部分文字列の総数を求める
// 入力: sam は文字列 S から構築済みの標準 SAM
// 出力: S の異なる非空部分文字列数
// 処理量: 時間 O(M)、追加メモリ O(1)、M = sam.states.size()
template <int K, int BASE>
long long sam_count_distinct_substrings(const SuffixAutomaton<K, BASE>& sam) {
    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        answer += sam.states[v].len - sam.states[sam.states[v].link].len;
    }
    return answer;
}

// 想定ユースケース: 構築元文字列 S の異なる非空部分文字列を一度ずつ数えた長さの総和を求める
// 入力: sam は文字列 S から構築済みの標準 SAM
// 出力: S の異なる非空部分文字列すべての長さの総和
// 処理量: 時間 O(M)、追加メモリ O(1)、M = sam.states.size()
template <int K, int BASE>
long long sam_sum_distinct_substring_lengths(const SuffixAutomaton<K, BASE>& sam) {
    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        const long long left = sam.states[sam.states[v].link].len + 1LL;
        const long long right = sam.states[v].len;
        answer += (left + right) * (right - left + 1) / 2;
    }
    return answer;
}

// 想定ユースケース: 長さごとに、構築元文字列 S の異なる部分文字列数を集計する
// 入力: sam は文字列 S から構築済みの標準 SAM
// 出力: size が N + 1 の配列 answer、answer[L] は長さ L の異なる部分文字列数、answer[0] は 0
// 処理量: 時間 O(M + N)、追加メモリ O(N)、M = 状態数、N = |S|
template <int K, int BASE>
std::vector<long long> sam_count_distinct_substrings_by_length(
    const SuffixAutomaton<K, BASE>& sam
) {
    const int max_length = sam.states[sam.last].len;
    std::vector<long long> difference(max_length + 2, 0);

    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        const int left = sam.states[sam.states[v].link].len + 1;
        const int right = sam.states[v].len;
        ++difference[left];
        --difference[right + 1];
    }

    std::vector<long long> answer(max_length + 1, 0);
    for (int length = 1; length <= max_length; ++length) {
        answer[length] = answer[length - 1] + difference[length];
    }
    return answer;
}

// 想定ユースケース: 構築元文字列 S と別文字列 other の最長共通連続部分文字列を1つ復元する
// 入力: sam は文字列 S から構築済みの標準 SAM、other は比較対象文字列
// 出力: 最長共通部分文字列の1つ、共通する非空部分文字列がなければ空文字列
// 処理量: 時間 O(P)、出力を除く追加メモリ O(1)、P = |other|
template <int K, int BASE>
std::string sam_longest_common_substring(
    const SuffixAutomaton<K, BASE>& sam,
    std::string_view other
) {
    int v = 0;
    int length = 0;
    int best_length = 0;
    int best_end = -1;

    for (int i = 0; i < static_cast<int>(other.size()); ++i) {
        const int c = static_cast<unsigned char>(other[i]) - BASE;
        if (c < 0 || K <= c) {
            v = 0;
            length = 0;
            continue;
        }

        while (v != 0 && sam.states[v].next[c] == -1) {
            v = sam.states[v].link;
            length = sam.states[v].len;
        }

        if (sam.states[v].next[c] != -1) {
            v = sam.states[v].next[c];
            ++length;
        } else {
            v = 0;
            length = 0;
        }

        if (length > best_length) {
            best_length = length;
            best_end = i;
        }
    }

    if (best_length == 0) return {};
    return std::string(other.substr(best_end - best_length + 1, best_length));
}

// 想定ユースケース: 構築元文字列 S と other の両方に現れる異なる非空部分文字列数を求める
// 入力: sam は文字列 S から構築済みの標準 SAM、other は比較対象文字列
// 出力: S と other に共通する異なる非空部分文字列数
// 処理量: 時間 O(P + M + N)、追加メモリ O(M + N)、P = |other|、M = 状態数、N = |S|
template <int K, int BASE>
long long sam_count_common_distinct_substrings(
    const SuffixAutomaton<K, BASE>& sam,
    std::string_view other
) {
    const auto make_length_order = [&]() {
        int max_length = 0;
        for (const auto& state : sam.states) max_length = std::max(max_length, state.len);

        std::vector<int> count(max_length + 1, 0);
        for (const auto& state : sam.states) ++count[state.len];
        for (int i = 1; i <= max_length; ++i) count[i] += count[i - 1];

        std::vector<int> order(sam.states.size());
        for (int v = static_cast<int>(sam.states.size()) - 1; v >= 0; --v) {
            order[--count[sam.states[v].len]] = v;
        }
        return order;
    };

    const auto match_limits = [&](const std::vector<int>& order) {
        std::vector<int> best(sam.states.size(), 0);
        int v = 0;
        int length = 0;

        // other の各位置で最長一致状態と一致長を更新する
        for (char ch : other) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c) {
                v = 0;
                length = 0;
                continue;
            }

            while (v != 0 && sam.states[v].next[c] == -1) {
                v = sam.states[v].link;
                length = sam.states[v].len;
            }

            if (sam.states[v].next[c] != -1) {
                v = sam.states[v].next[c];
                ++length;
                best[v] = std::max(best[v], length);
            } else {
                v = 0;
                length = 0;
            }
        }

        // suffix link 側の状態へ一致可能長を伝播する
        for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) {
            const int state = order[i];
            const int parent = sam.states[state].link;
            best[parent] = std::max(
                best[parent],
                std::min(best[state], sam.states[parent].len)
            );
        }
        return best;
    };

    const std::vector<int> order = make_length_order();
    const std::vector<int> best = match_limits(order);

    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        const int lower = sam.states[sam.states[v].link].len;
        const int upper = std::min(sam.states[v].len, best[v]);
        if (lower < upper) answer += upper - lower;
    }
    return answer;
}

// 想定ユースケース: 構築元文字列 S と others 内の全文字列に共通する最長部分文字列の長さを求める
// 入力: sam は文字列 S から構築済みの標準 SAM、others は残りの比較対象文字列群
// 出力: S と others の全要素に共通する最長連続部分文字列の長さ、others が空なら |S|
// 処理量: 時間 O(N + M + T + Q M)、追加メモリ O(M + N)、N = |S|、M = 状態数、Q = others.size()、T = 総文字数
template <int K, int BASE>
int sam_longest_common_substring_length_many(
    const SuffixAutomaton<K, BASE>& sam,
    const std::vector<std::string>& others
) {
    const auto make_length_order = [&]() {
        int max_length = 0;
        for (const auto& state : sam.states) max_length = std::max(max_length, state.len);

        std::vector<int> count(max_length + 1, 0);
        for (const auto& state : sam.states) ++count[state.len];
        for (int i = 1; i <= max_length; ++i) count[i] += count[i - 1];

        std::vector<int> order(sam.states.size());
        for (int v = static_cast<int>(sam.states.size()) - 1; v >= 0; --v) {
            order[--count[sam.states[v].len]] = v;
        }
        return order;
    };

    const auto match_limits = [&](std::string_view text, const std::vector<int>& order) {
        std::vector<int> best(sam.states.size(), 0);
        int v = 0;
        int length = 0;

        // text の各位置で最長一致状態と一致長を更新する
        for (char ch : text) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c) {
                v = 0;
                length = 0;
                continue;
            }

            while (v != 0 && sam.states[v].next[c] == -1) {
                v = sam.states[v].link;
                length = sam.states[v].len;
            }

            if (sam.states[v].next[c] != -1) {
                v = sam.states[v].next[c];
                ++length;
                best[v] = std::max(best[v], length);
            } else {
                v = 0;
                length = 0;
            }
        }

        // suffix link 側の状態へ一致可能長を伝播する
        for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) {
            const int state = order[i];
            const int parent = sam.states[state].link;
            best[parent] = std::max(
                best[parent],
                std::min(best[state], sam.states[parent].len)
            );
        }
        return best;
    };

    const std::vector<int> order = make_length_order();
    std::vector<int> common(sam.states.size());
    for (int v = 0; v < static_cast<int>(sam.states.size()); ++v) {
        common[v] = sam.states[v].len;
    }

    for (const auto& text : others) {
        const std::vector<int> best = match_limits(text, order);
        for (int v = 0; v < static_cast<int>(sam.states.size()); ++v) {
            common[v] = std::min(common[v], best[v]);
        }
    }

    return *std::max_element(common.begin(), common.end());
}

// 想定ユースケース: 構築元文字列 S と others 内の全文字列に共通する異なる非空部分文字列数を求める
// 入力: sam は文字列 S から構築済みの標準 SAM、others は残りの比較対象文字列群
// 出力: S と others の全要素に共通する異なる非空部分文字列数、others が空なら S の distinct 数
// 処理量: 時間 O(N + M + T + Q M)、追加メモリ O(M + N)、N = |S|、M = 状態数、Q = others.size()、T = 総文字数
template <int K, int BASE>
long long sam_count_common_distinct_substrings_many(
    const SuffixAutomaton<K, BASE>& sam,
    const std::vector<std::string>& others
) {
    const auto make_length_order = [&]() {
        int max_length = 0;
        for (const auto& state : sam.states) max_length = std::max(max_length, state.len);

        std::vector<int> count(max_length + 1, 0);
        for (const auto& state : sam.states) ++count[state.len];
        for (int i = 1; i <= max_length; ++i) count[i] += count[i - 1];

        std::vector<int> order(sam.states.size());
        for (int v = static_cast<int>(sam.states.size()) - 1; v >= 0; --v) {
            order[--count[sam.states[v].len]] = v;
        }
        return order;
    };

    const auto match_limits = [&](std::string_view text, const std::vector<int>& order) {
        std::vector<int> best(sam.states.size(), 0);
        int v = 0;
        int length = 0;

        // text の各位置で最長一致状態と一致長を更新する
        for (char ch : text) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c) {
                v = 0;
                length = 0;
                continue;
            }

            while (v != 0 && sam.states[v].next[c] == -1) {
                v = sam.states[v].link;
                length = sam.states[v].len;
            }

            if (sam.states[v].next[c] != -1) {
                v = sam.states[v].next[c];
                ++length;
                best[v] = std::max(best[v], length);
            } else {
                v = 0;
                length = 0;
            }
        }

        // suffix link 側の状態へ一致可能長を伝播する
        for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) {
            const int state = order[i];
            const int parent = sam.states[state].link;
            best[parent] = std::max(
                best[parent],
                std::min(best[state], sam.states[parent].len)
            );
        }
        return best;
    };

    const std::vector<int> order = make_length_order();
    std::vector<int> common(sam.states.size());
    for (int v = 0; v < static_cast<int>(sam.states.size()); ++v) {
        common[v] = sam.states[v].len;
    }

    for (const auto& text : others) {
        const std::vector<int> best = match_limits(text, order);
        for (int v = 0; v < static_cast<int>(sam.states.size()); ++v) {
            common[v] = std::min(common[v], best[v]);
        }
    }

    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        const int lower = sam.states[sam.states[v].link].len;
        if (lower < common[v]) answer += common[v] - lower;
    }
    return answer;
}

// 想定ユースケース: 構築元文字列 S の異なる非空部分文字列を辞書順に並べた k 番目を復元する
// 入力: sam は文字列 S から構築済みの標準 SAM、k は 1-indexed の順位
// 出力: k 番目の文字列、k <= 0 または総数を超える場合は std::nullopt
// 処理量: 時間 O(M + N + K(M + A))、追加メモリ O(M + N + A)、M = 状態数、N = |S|、A = 答えの長さ
template <int K, int BASE>
std::optional<std::string> sam_kth_distinct_substring(
    const SuffixAutomaton<K, BASE>& sam,
    long long k
) {
    if (k <= 0) return std::nullopt;

    const auto make_length_order = [&]() {
        int max_length = 0;
        for (const auto& state : sam.states) max_length = std::max(max_length, state.len);

        std::vector<int> count(max_length + 1, 0);
        for (const auto& state : sam.states) ++count[state.len];
        for (int i = 1; i <= max_length; ++i) count[i] += count[i - 1];

        std::vector<int> order(sam.states.size());
        for (int v = static_cast<int>(sam.states.size()) - 1; v >= 0; --v) {
            order[--count[sam.states[v].len]] = v;
        }
        return order;
    };

    const std::vector<int> order = make_length_order();
    std::vector<long long> paths(sam.states.size(), 0);

    // 各状態から開始する非空パス数を k で飽和させて数える
    for (int i = static_cast<int>(order.size()) - 1; i >= 0; --i) {
        const int v = order[i];
        long long total = 0;
        for (int c = 0; c < K; ++c) {
            const int to = sam.states[v].next[c];
            if (to == -1) continue;
            const long long block = paths[to] >= k - 1 ? k : paths[to] + 1;
            total = total >= k - block ? k : total + block;
        }
        paths[v] = total;
    }

    if (paths[0] < k) return std::nullopt;

    // 文字ごとの辞書順ブロックを飛ばしながら答えを復元する
    std::string answer;
    int v = 0;
    while (true) {
        for (int c = 0; c < K; ++c) {
            const int to = sam.states[v].next[c];
            if (to == -1) continue;
            const long long block = paths[to] >= k - 1 ? k : paths[to] + 1;
            if (k > block) {
                k -= block;
                continue;
            }

            answer.push_back(static_cast<char>(BASE + c));
            if (k == 1) return answer;
            --k;
            v = to;
            break;
        }
    }
}

// 想定ユースケース: alphabet [BASE, BASE + K) 上で、S に現れない最短文字列を求める
// 入力: sam は文字列 S から構築済みの標準 SAM
// 出力: S の部分文字列でない文字列のうち、長さ最小かつその中で辞書順最小のもの
// 処理量: 時間 O(M + N + K(M + A))、追加メモリ O(M + N + A)、M = 状態数、N = |S|、A = 答えの長さ
template <int K, int BASE>
std::string sam_shortest_absent_substring(const SuffixAutomaton<K, BASE>& sam) {
    const auto make_length_order = [&]() {
        int max_length = 0;
        for (const auto& state : sam.states) max_length = std::max(max_length, state.len);

        std::vector<int> count(max_length + 1, 0);
        for (const auto& state : sam.states) ++count[state.len];
        for (int i = 1; i <= max_length; ++i) count[i] += count[i - 1];

        std::vector<int> order(sam.states.size());
        for (int v = static_cast<int>(sam.states.size()) - 1; v >= 0; --v) {
            order[--count[sam.states[v].len]] = v;
        }
        return order;
    };

    const std::vector<int> order = make_length_order();
    const int inf = std::numeric_limits<int>::max() / 4;
    std::vector<int> distance(sam.states.size(), inf);

    // DAG を len 降順に処理して不足遷移までの最短距離を求める
    for (int i = static_cast<int>(order.size()) - 1; i >= 0; --i) {
        const int v = order[i];
        for (int c = 0; c < K; ++c) {
            const int to = sam.states[v].next[c];
            if (to == -1) {
                distance[v] = 1;
            } else {
                distance[v] = std::min(distance[v], 1 + distance[to]);
            }
        }
    }

    // 最短距離を保つ最小文字を選び続ける
    std::string answer;
    int v = 0;
    while (true) {
        for (int c = 0; c < K; ++c) {
            const int to = sam.states[v].next[c];
            if (to == -1 && distance[v] == 1) {
                answer.push_back(static_cast<char>(BASE + c));
                return answer;
            }
            if (to != -1 && distance[v] == 1 + distance[to]) {
                answer.push_back(static_cast<char>(BASE + c));
                v = to;
                break;
            }
        }
    }
}

// 想定ユースケース: suffix 判定を複数回行う前処理として、S の suffix に対応する状態を印付けする
// 入力: sam は文字列 S から構築済みの標準 SAM
// 出力: size が M の配列 terminal、terminal[v] は状態 v が S の suffix 状態なら true
// 処理量: 時間 O(M)、追加メモリ O(M)、M = sam.states.size()
template <int K, int BASE>
std::vector<char> sam_terminal_states(const SuffixAutomaton<K, BASE>& sam) {
    std::vector<char> terminal(sam.states.size(), false);
    for (int v = sam.last; v != -1; v = sam.states[v].link) terminal[v] = true;
    return terminal;
}

// 想定ユースケース: 複数の pattern について、構築元文字列 S の接尾辞かを高速に判定する
// 入力: sam は S の標準 SAM、terminal は sam_terminal_states(sam) の結果、pattern は判定対象
// 出力: pattern が S の suffix なら true、そうでなければ false、空文字列は true
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE>
bool sam_is_suffix(
    const SuffixAutomaton<K, BASE>& sam,
    const std::vector<char>& terminal,
    std::string_view pattern
) {
    const auto find_state = [&]() {
        int v = 0;
        for (char ch : pattern) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
            v = sam.states[v].next[c];
        }
        return v;
    };

    const int state = find_state();
    return state != -1 && terminal[state];
}


// ============================================================================
// SuffixAutomatonOcc と対応 solver
// ============================================================================

// 固定 alphabet 上の出現回数・代表位置付き suffix automaton
// 構築 O(n)、状態数 O(n)、メモリ O(Kn)
template <int K = 26, int BASE = 'a', class Count = long long>
struct SuffixAutomatonOcc {
    using count_type = Count;

    struct State {
        std::array<int, K> next;
        int link = -1;
        int len = 0;
        Count occurrence_seed = 0;
        int first_pos = -1;

        // 全遷移を未使用状態で初期化する O(K)
        State() {
            next.fill(-1);
        }
    };

    std::vector<State> states;
    int last = 0;

    // 空の suffix automaton を構築する O(1)
    explicit SuffixAutomatonOcc(int max_length = 0) {
        reset(max_length);
    }

    // 内容を破棄して空の suffix automaton に戻す O(1)
    void reset(int max_length = 0) {
        states.clear();
        if (max_length > 0) states.reserve(static_cast<std::size_t>(max_length) * 2);
        states.emplace_back();
        last = 0;
    }

    // 末尾へ1文字追加して新しい last 状態を返す ならし O(1)
    int extend(char ch) {
        const int c = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= c && c < K);

        // 新しい文字列全体を表す通常状態を作る
        const int cur = static_cast<int>(states.size());
        states.emplace_back();
        states[cur].len = states[last].len + 1;
        states[cur].occurrence_seed = 1;
        states[cur].first_pos = states[cur].len - 1;

        // 新しく現れた suffix に対応する遷移を追加する
        int p = last;
        while (p != -1 && states[p].next[c] == -1) {
            states[p].next[c] = cur;
            p = states[p].link;
        }

        // 既存遷移との関係から suffix link を決める
        if (p == -1) {
            states[cur].link = 0;
        } else {
            const int q = states[p].next[c];
            if (states[p].len + 1 == states[q].len) {
                states[cur].link = q;
            } else {
                // q が表す長さ範囲を clone で分割する
                const int clone = static_cast<int>(states.size());
                states.push_back(states[q]);
                states[clone].len = states[p].len + 1;
                states[clone].occurrence_seed = 0;

                while (p != -1 && states[p].next[c] == q) {
                    states[p].next[c] = clone;
                    p = states[p].link;
                }

                states[q].link = clone;
                states[cur].link = clone;
            }
        }

        last = cur;
        return cur;
    }

    // 文字列全体から suffix automaton を構築する O(n)
    void build(std::string_view s) {
        reset(static_cast<int>(s.size()));
        for (char ch : s) extend(ch);
    }
};

// 想定ユースケース: pattern の到達状態から出現回数や first_pos を直接参照する
// 入力: sam は文字列 S から構築済みの出現回数付き SAM、pattern は探索する文字列
// 出力: root から pattern を読んだ到達状態番号、読めなければ -1、空文字列なら root の 0
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE, class Count>
int sam_occ_find_state(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    std::string_view pattern
) {
    int v = 0;
    for (char ch : pattern) {
        const int c = static_cast<unsigned char>(ch) - BASE;
        if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
        v = sam.states[v].next[c];
    }
    return v;
}

// 想定ユースケース: 出現回数を使う各 solver の共通前処理として、状態ごとの endpos 数を集約する
// 入力: sam は文字列 S から構築済みの出現回数付き SAM
// 出力: size が M の配列 occurrences、occurrences[v] は状態 v が表す各部分文字列の出現回数
// 処理量: 時間 O(M + N)、追加メモリ O(M + N)、M = 状態数、N = |S|
template <int K, int BASE, class Count>
std::vector<Count> sam_occurrence_counts(const SuffixAutomatonOcc<K, BASE, Count>& sam) {
    int max_length = 0;
    for (const auto& state : sam.states) max_length = std::max(max_length, state.len);

    std::vector<int> count(max_length + 1, 0);
    for (const auto& state : sam.states) ++count[state.len];
    for (int i = 1; i <= max_length; ++i) count[i] += count[i - 1];

    std::vector<int> order(sam.states.size());
    for (int v = static_cast<int>(sam.states.size()) - 1; v >= 0; --v) {
        order[--count[sam.states[v].len]] = v;
    }

    std::vector<Count> occurrences(sam.states.size());
    for (int v = 0; v < static_cast<int>(sam.states.size()); ++v) {
        occurrences[v] = sam.states[v].occurrence_seed;
    }

    for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) {
        const int v = order[i];
        occurrences[sam.states[v].link] += occurrences[v];
    }
    return occurrences;
}

// 想定ユースケース: クエリ pattern が構築元文字列 S に何回現れるかを求める
// 入力: sam は S の出現回数付き SAM、occurrences は sam_occurrence_counts(sam) の結果
//       pattern は数える非空文字列
// 出力: pattern の出現回数、存在しない場合または空文字列の場合は 0
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE, class Count>
Count sam_count_occurrences(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    const std::vector<Count>& occurrences,
    std::string_view pattern
) {
    if (pattern.empty()) return 0;

    const auto find_state = [&]() {
        int v = 0;
        for (char ch : pattern) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
            v = sam.states[v].next[c];
        }
        return v;
    };

    const int state = find_state();
    return state == -1 ? Count{} : occurrences[state];
}

// 想定ユースケース: pattern が構築元文字列 S に最初に現れる位置を1件だけ求める
// 入力: sam は S から構築済みの出現回数付き SAM、pattern は探索する非空文字列
// 出力: 最初の出現開始位置を 0-indexed で返し、存在しない場合または空文字列なら std::nullopt
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE, class Count>
std::optional<int> sam_first_occurrence_start(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    std::string_view pattern
) {
    if (pattern.empty()) return std::nullopt;

    const auto find_state = [&]() {
        int v = 0;
        for (char ch : pattern) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
            v = sam.states[v].next[c];
        }
        return v;
    };

    const int state = find_state();
    if (state == -1) return std::nullopt;
    return sam.states[state].first_pos - static_cast<int>(pattern.size()) + 1;
}

// 想定ユースケース: pattern の全出現位置を列挙し、その後に区間処理などを行う
// 入力: sam は文字列 S から構築済みの出現回数付き SAM、pattern は探索する非空文字列
// 出力: pattern の全出現開始位置を 0-indexed の昇順で返し、存在しない場合または空文字列なら空配列
// 処理量: 時間 O(P + M + R log R)、追加メモリ O(M + R)、P = |pattern|、M = 状態数、R = 出現数
template <int K, int BASE, class Count>
std::vector<int> sam_all_occurrence_starts(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    std::string_view pattern
) {
    if (pattern.empty()) return {};

    const auto find_state = [&]() {
        int v = 0;
        for (char ch : pattern) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
            v = sam.states[v].next[c];
        }
        return v;
    };

    const int target = find_state();
    if (target == -1) return {};

    // suffix link tree を構築する
    std::vector<std::vector<int>> children(sam.states.size());
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        children[sam.states[v].link].push_back(v);
    }

    // target の部分木にある通常状態から終了位置を集める
    std::vector<int> starts;
    std::vector<int> stack{target};
    while (!stack.empty()) {
        const int v = stack.back();
        stack.pop_back();
        if (sam.states[v].occurrence_seed != 0) {
            starts.push_back(
                sam.states[v].first_pos - static_cast<int>(pattern.size()) + 1
            );
        }
        for (int to : children[v]) stack.push_back(to);
    }

    std::sort(starts.begin(), starts.end());
    return starts;
}

// 想定ユースケース: S 内で minimum_occurrences 回以上繰り返す最長部分文字列を1つ復元する
// 入力: source は構築元 S、sam は S の出現回数付き SAM、occurrences は集約結果
//       minimum_occurrences は必要な最小出現回数
// 出力: 条件を満たす最長部分文字列の1つ、存在しなければ空文字列
// 処理量: 時間 O(M + A)、出力を除く追加メモリ O(1)、M = 状態数、A = 答えの長さ
template <int K, int BASE, class Count>
std::string sam_longest_repeated_substring(
    std::string_view source,
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    const std::vector<Count>& occurrences,
    std::type_identity_t<Count> minimum_occurrences = 2
) {
    int best_state = -1;
    int best_length = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        if (occurrences[v] >= minimum_occurrences && sam.states[v].len > best_length) {
            best_state = v;
            best_length = sam.states[v].len;
        }
    }

    if (best_state == -1) return {};
    const int end = sam.states[best_state].first_pos;
    return std::string(source.substr(end - best_length + 1, best_length));
}

// 想定ユースケース: 部分文字列の長さと出現回数の積をスコアとして最大値を求める
// 入力: sam は S の出現回数付き SAM、occurrences は集約結果
//       minimum_occurrences は候補に含める最小出現回数
// 出力: 条件を満たす非空部分文字列について length × occurrence の最大値、候補なしなら 0
// 処理量: 時間 O(M)、追加メモリ O(1)、M = sam.states.size()
template <int K, int BASE, class Count>
long long sam_max_length_times_occurrence(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    const std::vector<Count>& occurrences,
    std::type_identity_t<Count> minimum_occurrences = 1
) {
    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        if (occurrences[v] < minimum_occurrences) continue;
        answer = std::max(
            answer,
            static_cast<long long>(sam.states[v].len) * static_cast<long long>(occurrences[v])
        );
    }
    return answer;
}

// 想定ユースケース: S 内で一定回数以上現れる異なる非空部分文字列数を求める
// 入力: sam は S の出現回数付き SAM、occurrences は集約結果
//       minimum_occurrences は必要な最小出現回数
// 出力: minimum_occurrences 回以上現れる異なる非空部分文字列数
// 処理量: 時間 O(M)、追加メモリ O(1)、M = sam.states.size()
template <int K, int BASE, class Count>
long long sam_count_distinct_substrings_occurring_at_least(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    const std::vector<Count>& occurrences,
    std::type_identity_t<Count> minimum_occurrences
) {
    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        if (occurrences[v] >= minimum_occurrences) {
            answer += sam.states[v].len - sam.states[sam.states[v].link].len;
        }
    }
    return answer;
}

// 想定ユースケース: S 内でちょうど指定回数現れる異なる非空部分文字列数を求める
// 入力: sam は S の出現回数付き SAM、occurrences は集約結果
//       exactly_occurrences は一致させる出現回数
// 出力: exactly_occurrences 回現れる異なる非空部分文字列数
// 処理量: 時間 O(M)、追加メモリ O(1)、M = sam.states.size()
template <int K, int BASE, class Count>
long long sam_count_distinct_substrings_occurring_exactly(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    const std::vector<Count>& occurrences,
    std::type_identity_t<Count> exactly_occurrences
) {
    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        if (occurrences[v] == exactly_occurrences) {
            answer += sam.states[v].len - sam.states[sam.states[v].link].len;
        }
    }
    return answer;
}

// 想定ユースケース: 各長さ L について、長さ L の部分文字列の最大出現回数をまとめて求める
// 入力: sam は S の出現回数付き SAM、occurrences は集約結果
// 出力: size が N + 1 の配列 answer、answer[L] は長さ L の部分文字列の最大出現回数
// 処理量: 時間 O(M + N)、追加メモリ O(N)、M = 状態数、N = |S|
template <int K, int BASE, class Count>
std::vector<Count> sam_max_occurrence_by_length(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    const std::vector<Count>& occurrences
) {
    const int max_length = sam.states[sam.last].len;
    std::vector<Count> answer(max_length + 1, Count{});

    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        answer[sam.states[v].len] = std::max(answer[sam.states[v].len], occurrences[v]);
    }
    for (int length = max_length - 1; length >= 1; --length) {
        answer[length] = std::max(answer[length], answer[length + 1]);
    }
    return answer;
}

// 想定ユースケース: 各部分文字列を S での出現回数だけ重複させた辞書順列の k 番目を求める
// 入力: sam は S の出現回数付き SAM、occurrences は集約結果、k は 1-indexed の順位
// 出力: k 番目の文字列、k <= 0 または重複込み総数を超える場合は std::nullopt
// 処理量: 時間 O(M + N + K(M + A))、追加メモリ O(M + N + A)、M = 状態数、N = |S|、A = 答えの長さ
template <int K, int BASE, class Count>
std::optional<std::string> sam_kth_substring_with_multiplicity(
    const SuffixAutomatonOcc<K, BASE, Count>& sam,
    const std::vector<Count>& occurrences,
    long long k
) {
    if (k <= 0) return std::nullopt;

    int max_length = 0;
    for (const auto& state : sam.states) max_length = std::max(max_length, state.len);
    std::vector<int> count(max_length + 1, 0);
    for (const auto& state : sam.states) ++count[state.len];
    for (int i = 1; i <= max_length; ++i) count[i] += count[i - 1];

    std::vector<int> order(sam.states.size());
    for (int v = static_cast<int>(sam.states.size()) - 1; v >= 0; --v) {
        order[--count[sam.states[v].len]] = v;
    }

    // 各状態から得られる重複込み文字列数を k で飽和させる
    std::vector<long long> paths(sam.states.size(), 0);
    for (int i = static_cast<int>(order.size()) - 1; i >= 0; --i) {
        const int v = order[i];
        long long total = 0;
        for (int c = 0; c < K; ++c) {
            const int to = sam.states[v].next[c];
            if (to == -1) continue;
            const long long own = std::min(k, static_cast<long long>(occurrences[to]));
            const long long block = paths[to] >= k - own ? k : own + paths[to];
            total = total >= k - block ? k : total + block;
        }
        paths[v] = total;
    }

    if (paths[0] < k) return std::nullopt;

    // 文字ごとの辞書順ブロックを飛ばしながら答えを復元する
    std::string answer;
    int v = 0;
    while (true) {
        for (int c = 0; c < K; ++c) {
            const int to = sam.states[v].next[c];
            if (to == -1) continue;

            const long long own = std::min(k, static_cast<long long>(occurrences[to]));
            const long long block = paths[to] >= k - own ? k : own + paths[to];
            if (k > block) {
                k -= block;
                continue;
            }

            answer.push_back(static_cast<char>(BASE + c));
            if (k <= own) return answer;
            k -= own;
            v = to;
            break;
        }
    }
}

// ============================================================================
// GeneralizedSuffixAutomaton と対応 solver
// ============================================================================

// 複数文字列または trie 上の root-to-node 文字列集合用 suffix automaton
// 固定 alphabet では構築 O(入力 trie サイズ)、状態数 O(入力 trie サイズ)
template <int K = 26, int BASE = 'a'>
struct GeneralizedSuffixAutomaton {
    struct State {
        std::array<int, K> next;
        int link = -1;
        int len = 0;

        // 全遷移を未使用状態で初期化する O(K)
        State() {
            next.fill(-1);
        }
    };

    std::vector<State> states;

    // 空の generalized suffix automaton を構築する O(1)
    explicit GeneralizedSuffixAutomaton(int max_total_length = 0) {
        reset(max_total_length);
    }

    // 内容を破棄して空の generalized suffix automaton に戻す O(1)
    void reset(int max_total_length = 0) {
        states.clear();
        if (max_total_length > 0) {
            states.reserve(static_cast<std::size_t>(max_total_length) * 2);
        }
        states.emplace_back();
    }

    // 任意状態から1文字伸ばした状態を返す ならし O(1)
    int extend_from(int state, char ch) {
        const int c = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= state && state < static_cast<int>(states.size()));
        assert(0 <= c && c < K);

        // 既存遷移がある場合は必要なら遷移先を分割する
        if (states[state].next[c] != -1) {
            const int q = states[state].next[c];
            if (states[state].len + 1 == states[q].len) return q;

            const int clone = static_cast<int>(states.size());
            states.push_back(states[q]);
            states[clone].len = states[state].len + 1;

            int p = state;
            while (p != -1 && states[p].next[c] == q) {
                states[p].next[c] = clone;
                p = states[p].link;
            }
            states[q].link = clone;
            return clone;
        }

        // 新しい root-to-node 文字列を表す状態を作る
        const int cur = static_cast<int>(states.size());
        states.emplace_back();
        states[cur].len = states[state].len + 1;

        int p = state;
        while (p != -1 && states[p].next[c] == -1) {
            states[p].next[c] = cur;
            p = states[p].link;
        }

        // 既存遷移との関係から suffix link を決める
        if (p == -1) {
            states[cur].link = 0;
        } else {
            const int q = states[p].next[c];
            if (states[p].len + 1 == states[q].len) {
                states[cur].link = q;
            } else {
                const int clone = static_cast<int>(states.size());
                states.push_back(states[q]);
                states[clone].len = states[p].len + 1;

                while (p != -1 && states[p].next[c] == q) {
                    states[p].next[c] = clone;
                    p = states[p].link;
                }

                states[q].link = clone;
                states[cur].link = clone;
            }
        }

        return cur;
    }

    // 文字列集合の全 substring を受理する automaton を構築する O(総文字数)
    void build(const std::vector<std::string>& strings) {
        struct TrieNode {
            std::array<int, K> next;

            // 全遷移を未使用状態で初期化する O(K)
            TrieNode() {
                next.fill(-1);
            }
        };

        // 入力文字列集合を trie にまとめる
        std::size_t total_length = 0;
        for (const auto& s : strings) total_length += s.size();

        std::vector<TrieNode> trie;
        trie.reserve(total_length + 1);
        trie.emplace_back();

        for (const auto& s : strings) {
            int v = 0;
            for (char ch : s) {
                const int c = static_cast<unsigned char>(ch) - BASE;
                assert(0 <= c && c < K);
                if (trie[v].next[c] == -1) {
                    trie[v].next[c] = static_cast<int>(trie.size());
                    trie.emplace_back();
                }
                v = trie[v].next[c];
            }
        }

        // 深さ順に trie の各辺を generalized suffix automaton へ追加する
        reset(static_cast<int>(trie.size()));
        std::vector<int> sam_state(trie.size(), 0);
        std::queue<int> que;
        que.push(0);

        while (!que.empty()) {
            const int v = que.front();
            que.pop();

            for (int c = 0; c < K; ++c) {
                const int to = trie[v].next[c];
                if (to == -1) continue;
                sam_state[to] = extend_from(sam_state[v], static_cast<char>(BASE + c));
                que.push(to);
            }
        }
    }
};

// 想定ユースケース: 文字列集合内で pattern を読み、その到達状態を独自処理に利用する
// 入力: sam は文字列集合または trie から構築済みの generalized SAM、pattern は探索文字列
// 出力: root から pattern を読んだ到達状態番号、読めなければ -1、空文字列なら root の 0
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE>
int generalized_sam_find_state(
    const GeneralizedSuffixAutomaton<K, BASE>& sam,
    std::string_view pattern
) {
    int v = 0;
    for (char ch : pattern) {
        const int c = static_cast<unsigned char>(ch) - BASE;
        if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
        v = sam.states[v].next[c];
    }
    return v;
}

// 想定ユースケース: pattern が入力文字列集合の少なくとも1本に連続部分文字列として現れるか判定する
// 入力: sam は文字列集合から構築済みの generalized SAM、pattern は判定対象
// 出力: いずれかの文字列の部分文字列なら true、そうでなければ false、空文字列は true
// 処理量: 時間 O(P)、追加メモリ O(1)、P = |pattern|
template <int K, int BASE>
bool generalized_sam_contains(
    const GeneralizedSuffixAutomaton<K, BASE>& sam,
    std::string_view pattern
) {
    const auto find_state = [&]() {
        int v = 0;
        for (char ch : pattern) {
            const int c = static_cast<unsigned char>(ch) - BASE;
            if (c < 0 || K <= c || sam.states[v].next[c] == -1) return -1;
            v = sam.states[v].next[c];
        }
        return v;
    };

    return find_state() != -1;
}

// 想定ユースケース: 文字列集合のどれかに現れる異なる非空部分文字列の和集合サイズを求める
// 入力: sam は文字列集合または trie から構築済みの generalized SAM
// 出力: 入力集合の少なくとも1本に現れる異なる非空部分文字列数
// 処理量: 時間 O(M)、追加メモリ O(1)、M = sam.states.size()
template <int K, int BASE>
long long generalized_sam_count_distinct_substrings(
    const GeneralizedSuffixAutomaton<K, BASE>& sam
) {
    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        answer += sam.states[v].len - sam.states[sam.states[v].link].len;
    }
    return answer;
}

// 想定ユースケース: 文字列集合の部分文字列の和集合を、長さごとに集計する
// 入力: sam は文字列集合または trie から構築済みの generalized SAM
// 出力: size が L + 1 の配列 answer、answer[d] は長さ d の異なる部分文字列数
// 処理量: 時間 O(M + L)、追加メモリ O(L)、M = 状態数、L = sam 内の最大 len
template <int K, int BASE>
std::vector<long long> generalized_sam_count_distinct_substrings_by_length(
    const GeneralizedSuffixAutomaton<K, BASE>& sam
) {
    int max_length = 0;
    for (const auto& state : sam.states) max_length = std::max(max_length, state.len);
    std::vector<long long> difference(max_length + 2, 0);

    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        const int left = sam.states[sam.states[v].link].len + 1;
        const int right = sam.states[v].len;
        ++difference[left];
        --difference[right + 1];
    }

    std::vector<long long> answer(max_length + 1, 0);
    for (int length = 1; length <= max_length; ++length) {
        answer[length] = answer[length - 1] + difference[length];
    }
    return answer;
}

// 想定ユースケース: generalized SAM の構築から一括して、文字列集合の distinct substring 和集合を数える
// 入力: strings は対象となる文字列集合
// 出力: strings の少なくとも1本に現れる異なる非空部分文字列数
// 処理量: K 固定なら時間 O(T)、追加メモリ O(KT)、T = strings の総文字数
template <int K = 26, int BASE = 'a'>
long long sam_count_distinct_substrings_in_string_set(
    const std::vector<std::string>& strings
) {
    GeneralizedSuffixAutomaton<K, BASE> sam;
    sam.build(strings);

    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        answer += sam.states[v].len - sam.states[sam.states[v].link].len;
    }
    return answer;
}

// 想定ユースケース: ラベル付き根付き木の全 root-to-node 文字列に現れる部分文字列の和集合を数える
// 入力: parent[v] は v の親、頂点 0 は根、edge_character[v] は親から v への文字、添字 0 は未使用
// 出力: 全 root-to-node 文字列の少なくとも1本に現れる異なる非空部分文字列数
// 処理量: K 固定なら時間 O(n)、追加メモリ O(Kn)、n = parent.size()
template <int K = 26, int BASE = 'a'>
long long sam_count_distinct_substrings_in_rooted_tree(
    const std::vector<int>& parent,
    std::string_view edge_character
) {
    const int n = static_cast<int>(parent.size());
    assert(static_cast<int>(edge_character.size()) == n);
    if (n == 0) return 0;

    // 親配列から子リストを作る
    std::vector<std::vector<int>> children(n);
    for (int v = 1; v < n; ++v) {
        assert(0 <= parent[v] && parent[v] < n);
        children[parent[v]].push_back(v);
    }

    // 深さ順に各 root-to-node 文字列を generalized SAM へ追加する
    GeneralizedSuffixAutomaton<K, BASE> sam(n - 1);
    std::vector<int> sam_state(n, 0);
    std::queue<int> que;
    que.push(0);

    while (!que.empty()) {
        const int v = que.front();
        que.pop();
        for (int to : children[v]) {
            sam_state[to] = sam.extend_from(sam_state[v], edge_character[to]);
            que.push(to);
        }
    }

    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        answer += sam.states[v].len - sam.states[sam.states[v].link].len;
    }
    return answer;
}


// ============================================================================
// SuffixAutomatonSparse と対応 solver
// ============================================================================

// 大きい alphabet 上の suffix automaton
// std::map を用いるため構築 O(n log sigma)、メモリ O(n)
template <class Symbol = int, class Compare = std::less<Symbol>>
struct SuffixAutomatonSparse {
    struct State {
        std::map<Symbol, int, Compare> next;
        int link = -1;
        int len = 0;
    };

    std::vector<State> states;
    int last = 0;

    // 空の sparse suffix automaton を構築する O(1)
    explicit SuffixAutomatonSparse(int max_length = 0) {
        reset(max_length);
    }

    // 内容を破棄して空の sparse suffix automaton に戻す O(1)
    void reset(int max_length = 0) {
        states.clear();
        if (max_length > 0) states.reserve(static_cast<std::size_t>(max_length) * 2);
        states.emplace_back();
        last = 0;
    }

    // 末尾へ1要素追加して新しい last 状態を返す ならし O(log sigma)
    int extend(const Symbol& symbol) {
        // 新しい列全体を表す状態を作る
        const int cur = static_cast<int>(states.size());
        states.emplace_back();
        states[cur].len = states[last].len + 1;

        // 新しく現れた suffix に対応する遷移を追加する
        int p = last;
        while (p != -1 && states[p].next.find(symbol) == states[p].next.end()) {
            states[p].next.emplace(symbol, cur);
            p = states[p].link;
        }

        // 既存遷移との関係から suffix link を決める
        if (p == -1) {
            states[cur].link = 0;
        } else {
            const int q = states[p].next.find(symbol)->second;
            if (states[p].len + 1 == states[q].len) {
                states[cur].link = q;
            } else {
                const int clone = static_cast<int>(states.size());
                states.push_back(states[q]);
                states[clone].len = states[p].len + 1;

                while (p != -1) {
                    auto it = states[p].next.find(symbol);
                    if (it == states[p].next.end() || it->second != q) break;
                    it->second = clone;
                    p = states[p].link;
                }

                states[q].link = clone;
                states[cur].link = clone;
            }
        }

        last = cur;
        return cur;
    }

    // 列全体から sparse suffix automaton を構築する O(n log sigma)
    void build(const std::vector<Symbol>& sequence) {
        reset(static_cast<int>(sequence.size()));
        for (const auto& symbol : sequence) extend(symbol);
    }
};

// 想定ユースケース: 大きい alphabet の列で pattern の到達状態を取得し、状態情報を利用する
// 入力: sam は列から構築済みの sparse SAM、pattern は探索する要素列
// 出力: root から pattern を読んだ到達状態番号、読めなければ -1、空列なら root の 0
// 処理量: 時間 O(P log sigma)、追加メモリ O(1)、P = pattern.size()、sigma = 異なる要素数
template <class Symbol, class Compare>
int sam_sparse_find_state(
    const SuffixAutomatonSparse<Symbol, Compare>& sam,
    const std::vector<Symbol>& pattern
) {
    int v = 0;
    for (const auto& symbol : pattern) {
        const auto it = sam.states[v].next.find(symbol);
        if (it == sam.states[v].next.end()) return -1;
        v = it->second;
    }
    return v;
}

// 想定ユースケース: 整数列などの pattern が構築元列の連続部分列かを判定する
// 入力: sam は元の列から構築済みの sparse SAM、pattern は判定対象の要素列
// 出力: pattern が連続部分列なら true、そうでなければ false、空列は true
// 処理量: 時間 O(P log sigma)、追加メモリ O(1)、P = pattern.size()、sigma = 異なる要素数
template <class Symbol, class Compare>
bool sam_sparse_contains(
    const SuffixAutomatonSparse<Symbol, Compare>& sam,
    const std::vector<Symbol>& pattern
) {
    const auto find_state = [&]() {
        int v = 0;
        for (const auto& symbol : pattern) {
            const auto it = sam.states[v].next.find(symbol);
            if (it == sam.states[v].next.end()) return -1;
            v = it->second;
        }
        return v;
    };

    return find_state() != -1;
}

// 想定ユースケース: 整数列など、大きい alphabet の列に含まれる異なる非空連続部分列数を求める
// 入力: sequence は対象となる要素列
// 出力: sequence に含まれる異なる非空連続部分列数
// 処理量: 時間 O(N log sigma)、追加メモリ O(N)、N = sequence.size()、sigma = 異なる要素数
template <class Symbol = int, class Compare = std::less<Symbol>>
long long sam_count_distinct_subarrays(const std::vector<Symbol>& sequence) {
    SuffixAutomatonSparse<Symbol, Compare> sam(static_cast<int>(sequence.size()));
    sam.build(sequence);

    long long answer = 0;
    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        answer += sam.states[v].len - sam.states[sam.states[v].link].len;
    }
    return answer;
}

// ============================================================================
// SuffixAutomatonRollback と対応 solver
// ============================================================================

// 末尾追加を snapshot 単位で取り消せる suffix automaton
// append-only 区間では構築 O(n)、rollback は変更数に比例
template <int K = 26, int BASE = 'a'>
struct SuffixAutomatonRollback {
    struct State {
        std::array<int, K> next;
        int link = -1;
        int len = 0;

        // 全遷移を未使用状態で初期化する O(K)
        State() {
            next.fill(-1);
        }
    };

    struct Change {
        int state = -1;
        int field = -1;
        int old_value = -1;
    };

    struct Snapshot {
        std::size_t state_size = 0;
        std::size_t history_size = 0;
        int last = 0;
        long long distinct_count = 0;
    };

    std::vector<State> states;
    std::vector<Change> history;
    int last = 0;
    long long distinct_count = 0;

    // 空の rollback suffix automaton を構築する O(1)
    explicit SuffixAutomatonRollback(int max_length = 0) {
        reset(max_length);
    }

    // 内容を破棄して空の rollback suffix automaton に戻す O(1)
    void reset(int max_length = 0) {
        states.clear();
        history.clear();
        if (max_length > 0) {
            states.reserve(static_cast<std::size_t>(max_length) * 2);
            history.reserve(static_cast<std::size_t>(max_length) * 4);
        }
        states.emplace_back();
        last = 0;
        distinct_count = 0;
    }

    // 現在状態を rollback 用に記録する O(1)
    Snapshot snapshot() const {
        return Snapshot{states.size(), history.size(), last, distinct_count};
    }

    // 末尾へ1文字追加する ならし O(1)
    int extend(char ch) {
        const int c = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= c && c < K);

        // 新しい文字列全体を表す状態を作る
        const int cur = static_cast<int>(states.size());
        states.emplace_back();
        states[cur].len = states[last].len + 1;

        // 新しく現れた suffix に対応する遷移を追加する
        int p = last;
        while (p != -1 && states[p].next[c] == -1) {
            assign_next(p, c, cur);
            p = states[p].link;
        }

        // 既存遷移との関係から suffix link を決める
        if (p == -1) {
            states[cur].link = 0;
        } else {
            const int q = states[p].next[c];
            if (states[p].len + 1 == states[q].len) {
                states[cur].link = q;
            } else {
                const int clone = static_cast<int>(states.size());
                states.push_back(states[q]);
                states[clone].len = states[p].len + 1;

                while (p != -1 && states[p].next[c] == q) {
                    assign_next(p, c, clone);
                    p = states[p].link;
                }

                assign_link(q, clone);
                states[cur].link = clone;
            }
        }

        last = cur;
        distinct_count += states[cur].len - states[states[cur].link].len;
        return cur;
    }

    // 指定 snapshot の状態へ戻す O(取り消す変更数)
    void rollback(const Snapshot& target) {
        assert(target.state_size <= states.size());
        assert(target.history_size <= history.size());

        // 既存状態への変更を逆順に復元する
        while (history.size() > target.history_size) {
            const Change change = history.back();
            history.pop_back();
            if (change.field == K) {
                states[change.state].link = change.old_value;
            } else {
                states[change.state].next[change.field] = change.old_value;
            }
        }

        // snapshot 後に追加された状態を削除する
        states.resize(target.state_size);
        last = target.last;
        distinct_count = target.distinct_count;
    }

    // 現在文字列の異なる非空部分文字列数を返す O(1)
    long long count_distinct_substrings() const {
        return distinct_count;
    }

private:
    void assign_next(int state, int c, int value) {
        history.push_back(Change{state, c, states[state].next[c]});
        states[state].next[c] = value;
    }

    void assign_link(int state, int value) {
        history.push_back(Change{state, K, states[state].link});
        states[state].link = value;
    }
};

// 想定ユースケース: ラベル付き根付き木の各頂点ごとに、その root-to-node 文字列の distinct 数を求める
// 入力: parent[v] は v の親、頂点 0 は根、edge_character[v] は親から v への文字、添字 0 は未使用
// 出力: size が n の配列 answer、answer[v] は root から v までの文字列の異なる非空部分文字列数
// 処理量: 時間 O(n + C)、追加メモリ O(n)、C = DFS 全体で記録した変更数、最悪 C = O(n^2)
template <int K = 26, int BASE = 'a'>
std::vector<long long> sam_distinct_substring_counts_on_root_paths(
    const std::vector<int>& parent,
    std::string_view edge_character
) {
    const int n = static_cast<int>(parent.size());
    assert(static_cast<int>(edge_character.size()) == n);
    if (n == 0) return {};

    // 親配列から子リストを作る
    std::vector<std::vector<int>> children(n);
    for (int v = 1; v < n; ++v) {
        assert(0 <= parent[v] && parent[v] < n);
        children[parent[v]].push_back(v);
    }

    using Sam = SuffixAutomatonRollback<K, BASE>;
    using Snapshot = typename Sam::Snapshot;
    struct Frame {
        int vertex;
        int next_child;
        Snapshot before_enter;
        bool rollback_on_exit;
    };

    // DFS の入退場で append と rollback を対応させる
    Sam sam(n - 1);
    std::vector<long long> answer(n, 0);
    std::vector<Frame> stack;
    stack.push_back(Frame{0, 0, sam.snapshot(), false});

    while (!stack.empty()) {
        Frame& frame = stack.back();
        if (frame.next_child < static_cast<int>(children[frame.vertex].size())) {
            const int to = children[frame.vertex][frame.next_child++];
            const Snapshot before = sam.snapshot();
            sam.extend(edge_character[to]);
            answer[to] = sam.count_distinct_substrings();
            stack.push_back(Frame{to, 0, before, true});
        } else {
            const Snapshot before = frame.before_enter;
            const bool rollback_on_exit = frame.rollback_on_exit;
            stack.pop_back();
            if (rollback_on_exit) sam.rollback(before);
        }
    }

    return answer;
}

// ============================================================================
// テスト
// ============================================================================

#if __INCLUDE_LEVEL__ == 0

namespace suffix_automaton_test {

long long check_count = 0;

void check(bool condition, const std::string& message) {
    ++check_count;
    if (!condition) {
        std::cerr << "TEST FAILED: " << message << '\n';
        std::abort();
    }
}

std::vector<std::string> naive_distinct_substrings(const std::string& s) {
    std::vector<std::string> result;
    for (int left = 0; left < static_cast<int>(s.size()); ++left) {
        for (int right = left + 1; right <= static_cast<int>(s.size()); ++right) {
            result.push_back(s.substr(left, right - left));
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

int naive_occurrence_count(const std::string& s, const std::string& pattern) {
    if (pattern.empty()) return 0;
    int count = 0;
    for (int i = 0; i + static_cast<int>(pattern.size()) <= static_cast<int>(s.size()); ++i) {
        if (s.compare(i, pattern.size(), pattern) == 0) ++count;
    }
    return count;
}

std::vector<int> naive_occurrence_starts(const std::string& s, const std::string& pattern) {
    std::vector<int> result;
    if (pattern.empty()) return result;
    for (int i = 0; i + static_cast<int>(pattern.size()) <= static_cast<int>(s.size()); ++i) {
        if (s.compare(i, pattern.size(), pattern) == 0) result.push_back(i);
    }
    return result;
}

std::string naive_shortest_absent(const std::string& s, int alphabet_size, char base) {
    std::queue<std::string> que;
    for (int c = 0; c < alphabet_size; ++c) que.push(std::string(1, static_cast<char>(base + c)));
    while (true) {
        std::string current = que.front();
        que.pop();
        if (s.find(current) == std::string::npos) return current;
        for (int c = 0; c < alphabet_size; ++c) {
            que.push(current + static_cast<char>(base + c));
        }
    }
}

std::vector<std::string> enumerate_strings(int alphabet_size, int max_length, char base) {
    std::vector<std::string> result{std::string()};
    std::vector<std::string> frontier{std::string()};
    for (int length = 1; length <= max_length; ++length) {
        std::vector<std::string> next;
        for (const auto& prefix : frontier) {
            for (int c = 0; c < alphabet_size; ++c) {
                next.push_back(prefix + static_cast<char>(base + c));
            }
        }
        result.insert(result.end(), next.begin(), next.end());
        frontier.swap(next);
    }
    return result;
}

void validate_standard_structure(const std::string& s) {
    SuffixAutomaton<3, 'a'> sam(static_cast<int>(s.size()));
    sam.build(s);

    check(sam.states[0].len == 0, "standard root len");
    check(sam.states[0].link == -1, "standard root link");
    check(sam.states.size() <= std::max<std::size_t>(1, 2 * s.size()), "standard state bound");

    for (int v = 1; v < static_cast<int>(sam.states.size()); ++v) {
        check(sam.states[v].link >= 0, "standard non-root link");
        check(
            sam.states[sam.states[v].link].len < sam.states[v].len,
            "standard link length"
        );
    }
    for (int v = 0; v < static_cast<int>(sam.states.size()); ++v) {
        for (int c = 0; c < 3; ++c) {
            const int to = sam.states[v].next[c];
            if (to != -1) check(sam.states[v].len < sam.states[to].len, "standard edge length");
        }
    }
}

void test_standard_exhaustive() {
    const auto strings = enumerate_strings(3, 6, 'a');
    for (const auto& s : strings) {
        validate_standard_structure(s);
        const auto naive = naive_distinct_substrings(s);

        SuffixAutomaton<3, 'a'> sam(static_cast<int>(s.size()));
        sam.build(s);

        check(sam_count_distinct_substrings(sam) == static_cast<long long>(naive.size()), "distinct count");

        long long naive_sum = 0;
        std::vector<long long> naive_by_length(s.size() + 1, 0);
        for (const auto& part : naive) {
            naive_sum += part.size();
            ++naive_by_length[part.size()];
            check(sam_contains(sam, part), "contains existing substring");
        }
        check(sam_sum_distinct_substring_lengths(sam) == naive_sum, "distinct length sum");
        check(sam_count_distinct_substrings_by_length(sam) == naive_by_length, "distinct by length");

        for (long long k = 1; k <= static_cast<long long>(naive.size()); ++k) {
            const auto actual = sam_kth_distinct_substring(sam, k);
            check(actual && *actual == naive[k - 1], "kth distinct");
        }
        check(!sam_kth_distinct_substring(sam, static_cast<long long>(naive.size()) + 1), "kth out of range");

        check(
            sam_shortest_absent_substring(sam) == naive_shortest_absent(s, 3, 'a'),
            "shortest absent"
        );

        const auto terminal = sam_terminal_states(sam);
        for (const auto& candidate : strings) {
            if (candidate.size() > s.size()) continue;
            const bool expected = candidate.empty() ||
                (s.size() >= candidate.size() && s.compare(s.size() - candidate.size(), candidate.size(), candidate) == 0);
            check(sam_is_suffix(sam, terminal, candidate) == expected, "suffix test");
        }
    }
}

void test_occurrence_exhaustive() {
    const auto strings = enumerate_strings(2, 7, 'a');
    for (const auto& s : strings) {
        const auto naive = naive_distinct_substrings(s);
        SuffixAutomatonOcc<2, 'a'> sam(static_cast<int>(s.size()));
        sam.build(s);
        const auto occurrences = sam_occurrence_counts(sam);

        std::vector<std::pair<std::string, int>> multiplicity;
        long long max_product = 0;
        for (const auto& part : naive) {
            const int expected = naive_occurrence_count(s, part);
            check(sam_count_occurrences(sam, occurrences, part) == expected, "occurrence count");
            check(sam_first_occurrence_start(sam, part) == std::optional<int>(s.find(part)), "first occurrence");
            check(sam_all_occurrence_starts(sam, part) == naive_occurrence_starts(s, part), "all occurrences");
            max_product = std::max(max_product, static_cast<long long>(part.size()) * expected);
            for (int i = 0; i < expected; ++i) multiplicity.emplace_back(part, expected);
        }
        std::sort(multiplicity.begin(), multiplicity.end());

        check(sam_max_length_times_occurrence(sam, occurrences) == max_product, "max length times occurrence");

        for (long long k = 1; k <= static_cast<long long>(multiplicity.size()); ++k) {
            const auto actual = sam_kth_substring_with_multiplicity(sam, occurrences, k);
            check(actual && *actual == multiplicity[k - 1].first, "kth multiplicity");
        }
        check(
            !sam_kth_substring_with_multiplicity(sam, occurrences, static_cast<long long>(multiplicity.size()) + 1),
            "kth multiplicity out of range"
        );

        for (int threshold = 1; threshold <= static_cast<int>(s.size()) + 1; ++threshold) {
            long long at_least = 0;
            long long exactly = 0;
            for (const auto& part : naive) {
                const int count = naive_occurrence_count(s, part);
                at_least += count >= threshold;
                exactly += count == threshold;
            }
            check(
                sam_count_distinct_substrings_occurring_at_least(sam, occurrences, threshold) == at_least,
                "count at least"
            );
            check(
                sam_count_distinct_substrings_occurring_exactly(sam, occurrences, threshold) == exactly,
                "count exactly"
            );
        }

        std::vector<long long> expected_by_length(s.size() + 1, 0);
        for (const auto& part : naive) {
            expected_by_length[part.size()] = std::max<long long>(
                expected_by_length[part.size()], naive_occurrence_count(s, part)
            );
        }
        check(sam_max_occurrence_by_length(sam, occurrences) == expected_by_length, "max occurrence by length");

        int best_repeated = 0;
        for (const auto& part : naive) {
            if (naive_occurrence_count(s, part) >= 2) best_repeated = std::max(best_repeated, static_cast<int>(part.size()));
        }
        check(
            static_cast<int>(sam_longest_repeated_substring(s, sam, occurrences).size()) == best_repeated,
            "longest repeated"
        );
    }
}

void test_common_random() {
    std::mt19937 rng(123456789);
    for (int test = 0; test < 3000; ++test) {
        const int n = static_cast<int>(rng() % 11U);
        const int m = static_cast<int>(rng() % 11U);
        std::string a(n, 'a');
        std::string b(m, 'a');
        for (char& ch : a) ch = static_cast<char>('a' + rng() % 3);
        for (char& ch : b) ch = static_cast<char>('a' + rng() % 3);

        SuffixAutomaton<3, 'a'> sam(n);
        sam.build(a);
        const auto da = naive_distinct_substrings(a);
        const auto db = naive_distinct_substrings(b);

        std::vector<std::string> common;
        std::set_intersection(da.begin(), da.end(), db.begin(), db.end(), std::back_inserter(common));
        int expected_lcs = 0;
        for (const auto& part : common) expected_lcs = std::max(expected_lcs, static_cast<int>(part.size()));

        check(
            static_cast<int>(sam_longest_common_substring(sam, b).size()) == expected_lcs,
            "two-string lcs"
        );
        check(
            sam_count_common_distinct_substrings(sam, b) == static_cast<long long>(common.size()),
            "two-string common distinct"
        );

        std::vector<std::string> others;
        const int count = 1 + rng() % 4;
        std::vector<std::vector<std::string>> all_sets;
        for (int i = 0; i < count; ++i) {
            const int length = static_cast<int>(rng() % 9U);
            std::string text(length, 'a');
            for (char& ch : text) ch = static_cast<char>('a' + rng() % 3);
            others.push_back(text);
            all_sets.push_back(naive_distinct_substrings(text));
        }

        std::vector<std::string> intersection = da;
        for (const auto& current : all_sets) {
            std::vector<std::string> next;
            std::set_intersection(
                intersection.begin(), intersection.end(),
                current.begin(), current.end(),
                std::back_inserter(next)
            );
            intersection.swap(next);
        }
        int expected_many_lcs = 0;
        for (const auto& part : intersection) expected_many_lcs = std::max(expected_many_lcs, static_cast<int>(part.size()));

        check(
            sam_longest_common_substring_length_many(sam, others) == expected_many_lcs,
            "many-string lcs"
        );
        check(
            sam_count_common_distinct_substrings_many(sam, others) == static_cast<long long>(intersection.size()),
            "many-string common distinct"
        );
    }
}

std::vector<std::string> naive_string_set_substrings(const std::vector<std::string>& strings) {
    std::vector<std::string> result;
    for (const auto& s : strings) {
        const auto current = naive_distinct_substrings(s);
        result.insert(result.end(), current.begin(), current.end());
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

void test_generalized_random() {
    std::mt19937 rng(987654321);
    for (int test = 0; test < 4000; ++test) {
        const int count = static_cast<int>(rng() % 6U);
        std::vector<std::string> strings(count);
        for (auto& s : strings) {
            s.resize(rng() % 8);
            for (char& ch : s) ch = static_cast<char>('a' + rng() % 3);
        }

        GeneralizedSuffixAutomaton<3, 'a'> sam;
        sam.build(strings);
        const auto naive = naive_string_set_substrings(strings);

        check(
            generalized_sam_count_distinct_substrings(sam) == static_cast<long long>(naive.size()),
            "generalized distinct"
        );
        check(
            sam_count_distinct_substrings_in_string_set<3, 'a'>(strings) == static_cast<long long>(naive.size()),
            "generalized convenience"
        );

        int max_length = 0;
        for (const auto& s : strings) max_length = std::max(max_length, static_cast<int>(s.size()));
        std::vector<long long> expected_by_length(max_length + 1, 0);
        for (const auto& part : naive) ++expected_by_length[part.size()];
        check(
            generalized_sam_count_distinct_substrings_by_length(sam) == expected_by_length,
            "generalized by length"
        );

        for (const auto& part : naive) check(generalized_sam_contains(sam, part), "generalized contains");
    }
}

void test_generalized_tree_random() {
    std::mt19937 rng(20240517);
    for (int test = 0; test < 2500; ++test) {
        const int n = 1 + static_cast<int>(rng() % 14U);
        std::vector<int> parent(n, -1);
        std::string edge(n, 'a');
        std::vector<std::string> paths(n);

        for (int v = 1; v < n; ++v) {
            parent[v] = static_cast<int>(rng() % static_cast<unsigned int>(v));
            edge[v] = static_cast<char>('a' + rng() % 3);
            paths[v] = paths[parent[v]] + edge[v];
        }

        const auto naive = naive_string_set_substrings(paths);
        check(
            sam_count_distinct_substrings_in_rooted_tree<3, 'a'>(parent, edge) == static_cast<long long>(naive.size()),
            "generalized tree"
        );
    }
}

std::vector<std::vector<int>> naive_distinct_subarrays(const std::vector<int>& a) {
    std::vector<std::vector<int>> result;
    for (int left = 0; left < static_cast<int>(a.size()); ++left) {
        for (int right = left + 1; right <= static_cast<int>(a.size()); ++right) {
            result.emplace_back(a.begin() + left, a.begin() + right);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

void test_sparse_random() {
    std::mt19937 rng(314159265);
    for (int test = 0; test < 3000; ++test) {
        const int n = static_cast<int>(rng() % 11U);
        std::vector<int> a(n);
        for (int& x : a) x = static_cast<int>(rng() % 9) - 4;

        const auto naive = naive_distinct_subarrays(a);
        check(
            sam_count_distinct_subarrays(a) == static_cast<long long>(naive.size()),
            "sparse distinct"
        );

        SuffixAutomatonSparse<int> sam(n);
        sam.build(a);
        for (const auto& part : naive) check(sam_sparse_contains(sam, part), "sparse contains");
    }
}

void test_rollback_random() {
    std::mt19937 rng(271828182);

    // 線形 append と snapshot rollback をランダムに比較する
    for (int test = 0; test < 1500; ++test) {
        SuffixAutomatonRollback<3, 'a'> sam(30);
        std::string current;
        std::vector<std::pair<typename SuffixAutomatonRollback<3, 'a'>::Snapshot, std::string>> snapshots;

        for (int operation = 0; operation < 100; ++operation) {
            if (!snapshots.empty() && rng() % 4 == 0) {
                const std::size_t index = static_cast<std::size_t>(rng()) % snapshots.size();
                sam.rollback(snapshots[index].first);
                current = snapshots[index].second;
                snapshots.resize(index + 1);
            } else {
                snapshots.emplace_back(sam.snapshot(), current);
                const char ch = static_cast<char>('a' + rng() % 3);
                sam.extend(ch);
                current.push_back(ch);
            }
            check(
                sam.count_distinct_substrings() == static_cast<long long>(naive_distinct_substrings(current).size()),
                "rollback current distinct"
            );
        }
    }

    // 木上の root-to-node 文字列を naive と比較する
    for (int test = 0; test < 2500; ++test) {
        const int n = 1 + static_cast<int>(rng() % 18U);
        std::vector<int> parent(n, -1);
        std::string edge(n, 'a');
        std::vector<std::string> paths(n);
        for (int v = 1; v < n; ++v) {
            parent[v] = static_cast<int>(rng() % static_cast<unsigned int>(v));
            edge[v] = static_cast<char>('a' + rng() % 3);
            paths[v] = paths[parent[v]] + edge[v];
        }

        const auto actual = sam_distinct_substring_counts_on_root_paths<3, 'a'>(parent, edge);
        for (int v = 0; v < n; ++v) {
            check(
                actual[v] == static_cast<long long>(naive_distinct_substrings(paths[v]).size()),
                "rollback tree path"
            );
        }
    }
}

void test_deterministic_cases() {
    const std::vector<std::string> cases = {
        "", "a", "aa", "aaaaaa", "ab", "abb", "aba", "ababa",
        "abcdef", "banana", "mississippi", "abcbc"
    };

    for (const auto& s : cases) {
        SuffixAutomaton<26, 'a'> sam(static_cast<int>(s.size()));
        sam.build(s);
        check(
            sam_count_distinct_substrings(sam) == static_cast<long long>(naive_distinct_substrings(s).size()),
            "deterministic standard"
        );

        SuffixAutomatonOcc<26, 'a'> occ_sam(static_cast<int>(s.size()));
        occ_sam.build(s);
        const auto occurrences = sam_occurrence_counts(occ_sam);
        for (const auto& part : naive_distinct_substrings(s)) {
            check(
                sam_count_occurrences(occ_sam, occurrences, part) == naive_occurrence_count(s, part),
                "deterministic occurrence"
            );
        }
    }
}

} // namespace suffix_automaton_test

int main() {
    using namespace suffix_automaton_test;

    test_deterministic_cases();
    test_standard_exhaustive();
    test_occurrence_exhaustive();
    test_common_random();
    test_generalized_random();
    test_generalized_tree_random();
    test_sparse_random();
    test_rollback_random();

    std::cout << "suffix_automaton_v03.hpp: all tests passed\n";
    std::cout << "checks: " << check_count << '\n';
    return 0;
}

#endif
