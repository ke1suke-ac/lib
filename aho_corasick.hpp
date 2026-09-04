#pragma once
#include <bits/stdc++.h>

/*
単体でコピー可能なAho-Corasick各種データ構造と、対応するユースケースソルバーを提供する
ソルバーは利用するデータ構造の直後に配置し、補助処理は必要な関数内へ閉じ込める
KとBASEを持つソルバーは、BASEから始まる文字コード上で連続するK文字すべてを使用する
非連続な文字集合は、事前に連続する文字番号へ変換してから利用する
数え上げソルバーは値型Valueをテンプレート引数で受け取り、剰余などの演算仕様をValue側へ委譲する
*/

// ============================================================
// 共通結果型
// ============================================================

struct AhoCorasickMatch {
    int pattern_id = -1;
    int left = 0;
    int right = -1;

    // マッチ情報を辞書順比較する O(1)
    auto operator<=>(const AhoCorasickMatch&) const = default;
};

// ============================================================
// AhoCorasick
// ============================================================

// 基本Aho-Corasick。パターンIDを保持せず、マッチ状態フラグのみを管理する
// BEGIN_DATA_STRUCTURE: AhoCorasick
template <int K = 26, int BASE = 'a'>
struct AhoCorasick {
    static_assert(K > 0);

private:
    using Transition = std::array<int, K>;

    std::vector<Transition> next_;
    std::vector<int> fail_;
    std::vector<std::uint8_t> matched_;
    bool built_ = false;

    int add_node() {
        next_.emplace_back();
        next_.back().fill(-1);
        fail_.push_back(0);
        matched_.push_back(0);
        return static_cast<int>(next_.size()) - 1;
    }

    static int symbol_index(char ch) {
        const int symbol = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= symbol && symbol < K);
        return symbol;
    }

public:
    AhoCorasick() {
        add_node();
    }

    // 状態数分の容量を確保する O(V)
    void reserve_nodes(int capacity) {
        next_.reserve(capacity);
        fail_.reserve(capacity);
        matched_.reserve(capacity);
    }

    // パターンを追加して終端状態を返す O(|pattern|K) 最悪、Kは固定定数
    int add(std::string_view pattern) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (char ch : pattern) {
            const int symbol = symbol_index(ch);
            if (next_[state][symbol] == -1) {
                next_[state][symbol] = add_node();
            }
            state = next_[state][symbol];
        }
        matched_[state] = 1;
        return state;
    }

    // fail link、全遷移、マッチ状態を構築する O(VK)
    void build() {
        assert(!built_);
        std::queue<int> q;

        // root直下のfailと、存在しないroot遷移を確定する
        for (int symbol = 0; symbol < K; ++symbol) {
            const int child = next_[0][symbol];
            if (child == -1) {
                next_[0][symbol] = 0;
            } else {
                fail_[child] = 0;
                q.push(child);
            }
        }

        // 浅い状態からfail、マッチ状態、完成済み遷移を構築する
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            matched_[state] = static_cast<std::uint8_t>(
                matched_[state] || matched_[fail_[state]]
            );

            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next_[state][symbol];
                if (child == -1) {
                    next_[state][symbol] = next_[fail_[state]][symbol];
                } else {
                    fail_[child] = next_[fail_[state]][symbol];
                    q.push(child);
                }
            }
        }
        built_ = true;
    }

    // テキストがいずれかのパターンを含むか返す O(|text|)
    bool contains_any(std::string_view text) const {
        assert(built_);
        int state = 0;
        for (char ch : text) {
            state = next_[state][symbol_index(ch)];
            if (matched_[state]) return true;
        }
        return false;
    }

    // 状態が何らかのパターンをsuffixとして含むか返す O(1)
    bool is_match_state(int state) const {
        assert(built_);
        return matched_[state] != 0;
    }

    // 1文字読んだ後の状態を返す O(1)
    int step(int state, char ch) const {
        assert(built_);
        return next_[state][symbol_index(ch)];
    }

    // 0以上K未満の文字番号を1つ読んだ後の状態を返す O(1)
    int step_index(int state, int symbol) const {
        assert(built_);
        assert(0 <= symbol && symbol < K);
        return next_[state][symbol];
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(next_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasick

// AhoCorasickを使うソルバー関数

// 複数パターンのいずれかがテキストに現れるか判定する
// 想定ユースケース:
//   NGワード検出など、1つでもマッチすれば十分な存在判定
// 入力:
//   patterns: 各要素が空でない検索パターン列
//   text: 検索対象文字列
// 出力:
//   textがpatternsのいずれかを部分文字列として含むならtrue、それ以外はfalse
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長とすると時間O(PK+N)、追加メモリO(VK)
template <int K = 26, int BASE = 'a'>
bool contains_any_pattern(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasick<K, BASE> automaton;
    std::size_t nodes = 1;
    for (const std::string& pattern : patterns) nodes += pattern.size();
    automaton.reserve_nodes(static_cast<int>(nodes));
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();
    return automaton.contains_any(text);
}

// 禁止パターンを含まない指定長文字列数を数える
// 想定ユースケース:
//   BASEから始まる連続K文字で長さ固定の文字列を作り、禁止部分文字列を避ける総数を求めるDP
// 入力:
//   length: 作る文字列の長さ
//   forbidden: 各要素が空でない禁止パターン列
//   テンプレート引数Value: 答えとDPに使う値型
//     intから構築でき、operator+=とゼロとの比較を利用できる型を指定する
//     剰余が必要ならatcoder::modint998244353などのACL modint型を指定する
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とする
// 出力:
//   条件を満たす文字列数をValue型で返す
// 処理量:
//   Pを禁止パターン総長、Vを状態数、Lをlengthとすると
//   時間O(PK+LVK)、追加メモリO(VK+V)
template <class Value, int K = 26, int BASE = 'a'>
Value count_strings_avoiding_patterns(
    int length,
    const std::vector<std::string>& forbidden
) {
    assert(length >= 0);
    AhoCorasick<K, BASE> automaton;
    for (const std::string& pattern : forbidden) automaton.add(pattern);
    automaton.build();

    const int states = automaton.state_count();
    std::vector<Value> dp(states, Value{}), next_dp(states, Value{});
    dp[0] = Value{1};

    // 長さを1ずつ伸ばし、禁止状態へ入る遷移を捨てる
    for (int position = 0; position < length; ++position) {
        std::fill(next_dp.begin(), next_dp.end(), Value{});
        for (int state = 0; state < states; ++state) {
            if (dp[state] == Value{} || automaton.is_match_state(state)) continue;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int next_state = automaton.step_index(state, symbol);
                if (automaton.is_match_state(next_state)) continue;
                next_dp[next_state] += dp[state];
            }
        }
        dp.swap(next_dp);
    }

    Value result{};
    for (int state = 0; state < states; ++state) {
        if (!automaton.is_match_state(state)) result += dp[state];
    }
    return result;
}

// 禁止パターンを含まない辞書順最小の指定長文字列を求める
// 想定ユースケース:
//   BASEから始まる連続K文字で禁止語を避け、存在する解のうち辞書順最小を復元する
// 入力:
//   length: 作る文字列の長さ
//   forbidden: 各要素が空でない禁止パターン列
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とし、その順を辞書順とする
// 出力:
//   解が存在すれば辞書順最小文字列、存在しなければstd::nullopt
// 処理量:
//   Pを禁止パターン総長、Vを状態数、Lをlengthとすると
//   時間O(PK+LVK)、追加メモリO(VK+LV)
template <int K = 26, int BASE = 'a'>
std::optional<std::string> lexicographically_smallest_string_avoiding_patterns(
    int length,
    const std::vector<std::string>& forbidden
) {
    assert(length >= 0);
    AhoCorasick<K, BASE> automaton;
    for (const std::string& pattern : forbidden) automaton.add(pattern);
    automaton.build();

    const int states = automaton.state_count();
    std::vector<std::uint8_t> can(static_cast<std::size_t>(length + 1) * states, 0);
    auto at = [&](int remaining, int state) -> std::uint8_t& {
        return can[static_cast<std::size_t>(remaining) * states + state];
    };

    // 残り0文字は現在が安全状態なら完成可能とする
    for (int state = 0; state < states; ++state) {
        at(0, state) = static_cast<std::uint8_t>(!automaton.is_match_state(state));
    }

    // 各状態から残り文字数を安全に埋められるかを後ろ向きに計算する
    for (int remaining = 1; remaining <= length; ++remaining) {
        for (int state = 0; state < states; ++state) {
            if (automaton.is_match_state(state)) continue;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int next_state = automaton.step_index(state, symbol);
                if (!automaton.is_match_state(next_state) && at(remaining - 1, next_state)) {
                    at(remaining, state) = 1;
                    break;
                }
            }
        }
    }

    if (!at(length, 0)) return std::nullopt;

    // 小さい文字から試し、残りを完成可能な最初の文字を採用する
    std::string result;
    result.reserve(length);
    int state = 0;
    for (int remaining = length; remaining > 0; --remaining) {
        for (int symbol = 0; symbol < K; ++symbol) {
            const int next_state = automaton.step_index(state, symbol);
            if (!automaton.is_match_state(next_state) && at(remaining - 1, next_state)) {
                result.push_back(static_cast<char>(BASE + symbol));
                state = next_state;
                break;
            }
        }
    }
    return result;
}

// 文字置換だけで禁止パターンを避ける最小変更回数を求める
// 想定ユースケース:
//   元文字列の長さを変えず、各位置をBASEから始まる連続K文字のいずれかへ置換して禁止語を除去する
// 入力:
//   source: 変更前の文字列
//   forbidden: 各要素が空でない禁止パターン列
//   テンプレート引数K, BASE: 置換後に使用できる文字をBASEから始まる連続K文字とする
// 出力:
//   禁止パターンを含まなくする最小変更回数、実現不可能なら-1
// 処理量:
//   Pを禁止パターン総長、Vを状態数、Nをsource長とすると
//   時間O(PK+NVK)、追加メモリO(VK+V)
template <int K = 26, int BASE = 'a'>
int minimum_changes_to_avoid_patterns(
    std::string_view source,
    const std::vector<std::string>& forbidden
) {
    AhoCorasick<K, BASE> automaton;
    for (const std::string& pattern : forbidden) automaton.add(pattern);
    automaton.build();

    const int states = automaton.state_count();
    const int inf = static_cast<int>(source.size()) + 1;
    std::vector<int> dp(states, inf), next_dp(states, inf);
    dp[0] = 0;

    // 各位置の候補文字を試し、安全状態への最小変更回数を更新する
    for (int position = 0; position < static_cast<int>(source.size()); ++position) {
        std::fill(next_dp.begin(), next_dp.end(), inf);
        for (int state = 0; state < states; ++state) {
            if (dp[state] == inf) continue;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int next_state = automaton.step_index(state, symbol);
                if (automaton.is_match_state(next_state)) continue;
                const char ch = static_cast<char>(BASE + symbol);
                next_dp[next_state] = std::min(
                    next_dp[next_state],
                    dp[state] + (source[position] != ch)
                );
            }
        }
        dp.swap(next_dp);
    }

    const int result = *std::min_element(dp.begin(), dp.end());
    return result == inf ? -1 : result;
}

// 禁止パターンを含まない巨大長文字列数を行列累乗で数える
// 想定ユースケース:
//   lengthが通常の位置DPでは処理できないほど大きい禁止文字列回避の数え上げ
// 入力:
//   length: 作る文字列の長さ
//   forbidden: 各要素が空でない禁止パターン列
//   テンプレート引数Value: 答え、状態ベクトル、行列要素に使う値型
//     intから構築でき、operator+=、operator*、ゼロとの比較を利用できる型を指定する
//     剰余が必要ならatcoder::modint998244353などのACL modint型を指定する
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とする
// 出力:
//   条件を満たす文字列数をValue型で返す
// 処理量:
//   Pを禁止パターン総長、Vを状態数、Sを安全状態数、Lをlengthとすると
//   時間O(PK+VK+S^3log L)、追加メモリO(VK+S^2)
template <class Value, int K = 26, int BASE = 'a'>
Value count_strings_avoiding_patterns_large_length(
    long long length,
    const std::vector<std::string>& forbidden
) {
    assert(length >= 0);

    struct Matrix {
        int n = 0;
        std::vector<Value> value;

        Matrix() = default;

        explicit Matrix(int size)
            : n(size), value(static_cast<std::size_t>(size) * size, Value{}) {}

        // 行列要素を参照する O(1)
        Value& at(int row, int col) {
            return value[static_cast<std::size_t>(row) * n + col];
        }

        // 行列要素を参照する O(1)
        const Value& at(int row, int col) const {
            return value[static_cast<std::size_t>(row) * n + col];
        }
    };

    auto multiply_matrix = [](const Matrix& lhs, const Matrix& rhs) -> Matrix {
        assert(lhs.n == rhs.n);
        Matrix result(lhs.n);

        // 0要素を飛ばし、連続するrhs行とresult行を走査する
        for (int i = 0; i < lhs.n; ++i) {
            for (int k = 0; k < lhs.n; ++k) {
                const Value& left = lhs.at(i, k);
                if (left == Value{}) continue;
                const std::size_t result_base = static_cast<std::size_t>(i) * lhs.n;
                const std::size_t rhs_base = static_cast<std::size_t>(k) * lhs.n;
                for (int j = 0; j < lhs.n; ++j) {
                    const Value& right = rhs.value[rhs_base + j];
                    if (right == Value{}) continue;
                    result.value[result_base + j] += left * right;
                }
            }
        }
        return result;
    };

    auto multiply_vector_matrix = [](const std::vector<Value>& values, const Matrix& matrix) {
        assert(static_cast<int>(values.size()) == matrix.n);
        std::vector<Value> result(matrix.n, Value{});

        for (int i = 0; i < matrix.n; ++i) {
            if (values[i] == Value{}) continue;
            const std::size_t base = static_cast<std::size_t>(i) * matrix.n;
            for (int j = 0; j < matrix.n; ++j) {
                const Value& transition = matrix.value[base + j];
                if (transition == Value{}) continue;
                result[j] += values[i] * transition;
            }
        }
        return result;
    };

    AhoCorasick<K, BASE> automaton;
    for (const std::string& pattern : forbidden) automaton.add(pattern);
    automaton.build();

    const int states = automaton.state_count();
    std::vector<int> safe_id(states, -1);
    int safe_count = 0;
    for (int state = 0; state < states; ++state) {
        if (!automaton.is_match_state(state)) safe_id[state] = safe_count++;
    }
    if (safe_id[0] == -1) return Value{};

    Matrix transition(safe_count);
    for (int state = 0; state < states; ++state) {
        if (safe_id[state] == -1) continue;
        for (int symbol = 0; symbol < K; ++symbol) {
            const int next_state = automaton.step_index(state, symbol);
            if (safe_id[next_state] == -1) continue;
            transition.at(safe_id[state], safe_id[next_state]) += Value{1};
        }
    }

    // 初期ベクトルへ二進累乗した遷移行列を右から作用させる
    std::vector<Value> current(safe_count, Value{});
    current[safe_id[0]] = Value{1};
    while (length > 0) {
        if (length & 1LL) current = multiply_vector_matrix(current, transition);
        length >>= 1LL;
        if (length > 0) transition = multiply_matrix(transition, transition);
    }

    Value result{};
    for (const Value& value : current) result += value;
    return result;
}

// 0以上の整数を通常の10進表記で見て、禁止数字列を含まない個数を数える
// 想定ユースケース:
//   上限制約付きの整数集合から特定の連続数字列を含む数を除外するdigit DP
// 入力:
//   upper_bound: 数える閉区間の上端
//   forbidden_digits: 各要素が空でなく、'0'から'9'だけからなる禁止数字列
// 出力:
//   0以上upper_bound以下で禁止数字列を含まない整数数
//   答えがunsigned long longに収まることを前提とする
// 処理量:
//   Pを禁止数字列総長、Vを状態数、Dをupper_boundの桁数とすると
//   時間O(10P+10DV)、追加メモリO(10V+V)
inline unsigned long long count_nonnegative_integers_avoiding_patterns(
    unsigned long long upper_bound,
    const std::vector<std::string>& forbidden_digits
) {
    AhoCorasick<10, '0'> automaton;
    for (const std::string& pattern : forbidden_digits) automaton.add(pattern);
    automaton.build();

    const std::string digits = std::to_string(upper_bound);
    const int states = automaton.state_count();
    using Count = unsigned long long;
    std::vector<Count> dp(static_cast<std::size_t>(2) * 2 * states, 0);
    std::vector<Count> next_dp(dp.size(), 0);
    auto index = [states](int tight, int started, int state) {
        return (tight * 2 + started) * states + state;
    };
    dp[index(1, 0, 0)] = 1;

    // 先頭ゼロはACへ入力せず、数字開始後のみ禁止パターンを判定する
    for (char limit_char : digits) {
        std::fill(next_dp.begin(), next_dp.end(), 0);
        const int limit_digit = limit_char - '0';
        for (int tight = 0; tight <= 1; ++tight) {
            for (int started = 0; started <= 1; ++started) {
                for (int state = 0; state < states; ++state) {
                    const Count current = dp[index(tight, started, state)];
                    if (current == 0) continue;
                    const int upper = tight ? limit_digit : 9;
                    for (int digit = 0; digit <= upper; ++digit) {
                        const int next_tight = tight && digit == upper;
                        if (!started && digit == 0) {
                            next_dp[index(next_tight, 0, 0)] += current;
                            continue;
                        }
                        const int next_state = automaton.step_index(state, digit);
                        if (automaton.is_match_state(next_state)) continue;
                        next_dp[index(next_tight, 1, next_state)] += current;
                    }
                }
            }
        }
        dp.swap(next_dp);
    }

    Count result = 0;
    for (int tight = 0; tight <= 1; ++tight) {
        for (int state = 0; state < states; ++state) {
            result += dp[index(tight, 1, state)];
        }
    }

    // 先頭ゼロだけだった経路を、通常表記"0"として1回だけ判定する
    const int zero_state = automaton.step_index(0, 0);
    if (!automaton.is_match_state(zero_state)) ++result;
    return result;
}

// 禁止パターンを一度も含まない無限文字列が存在するか判定する
// 想定ユースケース:
//   BASEから始まる連続K文字を無限に追加し続けられるかを、AC状態グラフの到達可能サイクルで判定する
// 入力:
//   forbidden: 各要素が空でない禁止パターン列
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とする
// 出力:
//   条件を満たす無限文字列が存在するならtrue、それ以外はfalse
// 処理量:
//   Pを禁止パターン総長、Vを状態数とすると
//   時間O(PK+VK)、追加メモリO(VK+V)
template <int K = 26, int BASE = 'a'>
bool exists_infinite_string_avoiding_patterns(
    const std::vector<std::string>& forbidden
) {
    AhoCorasick<K, BASE> automaton;
    for (const std::string& pattern : forbidden) automaton.add(pattern);
    automaton.build();
    if (automaton.is_match_state(0)) return false;

    const int states = automaton.state_count();
    std::vector<std::uint8_t> color(states, 0);
    struct Frame {
        int state;
        int next_symbol;
    };
    std::vector<Frame> stack;
    stack.push_back({0, 0});
    color[0] = 1;

    // 到達可能な安全状態グラフを反復DFSし、灰色頂点への辺を探す
    while (!stack.empty()) {
        Frame& frame = stack.back();
        if (frame.next_symbol == K) {
            color[frame.state] = 2;
            stack.pop_back();
            continue;
        }

        const int symbol = frame.next_symbol++;
        const int next_state = automaton.step_index(frame.state, symbol);
        if (automaton.is_match_state(next_state)) continue;
        if (color[next_state] == 1) return true;
        if (color[next_state] == 0) {
            color[next_state] = 1;
            stack.push_back({next_state, 0});
        }
    }
    return false;
}

// ============================================================
// AhoCorasickCount
// ============================================================

// 全パターンの出現数集約に特化したAho-Corasick
// BEGIN_DATA_STRUCTURE: AhoCorasickCount
template <int K = 26, int BASE = 'a'>
struct AhoCorasickCount {
    static_assert(K > 0);

private:
    using Transition = std::array<int, K>;

    std::vector<Transition> next_;
    std::vector<int> fail_;
    std::vector<int> output_count_;
    bool built_ = false;

    int add_node() {
        next_.emplace_back();
        next_.back().fill(-1);
        fail_.push_back(0);
        output_count_.push_back(0);
        return static_cast<int>(next_.size()) - 1;
    }

    static int symbol_index(char ch) {
        const int symbol = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= symbol && symbol < K);
        return symbol;
    }

public:
    AhoCorasickCount() {
        add_node();
    }

    // 状態数分の容量を確保する O(V)
    void reserve_nodes(int capacity) {
        next_.reserve(capacity);
        fail_.reserve(capacity);
        output_count_.reserve(capacity);
    }

    // パターンを追加する O(|pattern|K) 最悪、Kは固定定数
    void add(std::string_view pattern) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (char ch : pattern) {
            const int symbol = symbol_index(ch);
            if (next_[state][symbol] == -1) {
                next_[state][symbol] = add_node();
            }
            state = next_[state][symbol];
        }
        ++output_count_[state];
    }

    // fail link、全遷移、状態ごとのマッチ数を構築する O(VK)
    void build() {
        assert(!built_);
        std::queue<int> q;

        // root直下のfailと、存在しないroot遷移を確定する
        for (int symbol = 0; symbol < K; ++symbol) {
            const int child = next_[0][symbol];
            if (child == -1) {
                next_[0][symbol] = 0;
            } else {
                fail_[child] = 0;
                q.push(child);
            }
        }

        // 浅い状態からfail、出現数、完成済み遷移を構築する
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            output_count_[state] += output_count_[fail_[state]];

            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next_[state][symbol];
                if (child == -1) {
                    next_[state][symbol] = next_[fail_[state]][symbol];
                } else {
                    fail_[child] = next_[fail_[state]][symbol];
                    q.push(child);
                }
            }
        }
        built_ = true;
    }

    // テキスト中の全パターン出現数を返す O(|text|)
    long long count_all_matches(std::string_view text) const {
        assert(built_);
        int state = 0;
        long long result = 0;
        for (char ch : text) {
            state = next_[state][symbol_index(ch)];
            result += output_count_[state];
        }
        return result;
    }

    // 各位置を右端とするパターン出現数を返す O(|text|)
    std::vector<int> count_ending_at_each_position(std::string_view text) const {
        assert(built_);
        std::vector<int> result(text.size());
        int state = 0;
        for (int i = 0; i < static_cast<int>(text.size()); ++i) {
            state = next_[state][symbol_index(text[i])];
            result[i] = output_count_[state];
        }
        return result;
    }

    // 状態で終わるパターン数をfail祖先込みで返す O(1)
    int match_count(int state) const {
        assert(built_);
        return output_count_[state];
    }

    // 1文字読んだ後の状態を返す O(1)
    int step(int state, char ch) const {
        assert(built_);
        return next_[state][symbol_index(ch)];
    }

    // 0以上K未満の文字番号を1つ読んだ後の状態を返す O(1)
    int step_index(int state, int symbol) const {
        assert(built_);
        assert(0 <= symbol && symbol < K);
        return next_[state][symbol];
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(next_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasickCount

// AhoCorasickCountを使うソルバー関数

// テキスト中に現れる全パターンの総出現回数を数える
// 想定ユースケース:
//   どのパターンかや出現位置は不要で、重なりを含む総マッチ数だけを求める
// 入力:
//   patterns: 各要素が空でない検索パターン列
//   text: 検索対象文字列
// 出力:
//   全パターンの出現回数合計をlong longで返す
//   同一パターンを複数登録した場合は登録数だけ重複して数える
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長とすると時間O(PK+N)、追加メモリO(VK)
template <int K = 26, int BASE = 'a'>
long long count_all_pattern_occurrences(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickCount<K, BASE> automaton;
    std::size_t nodes = 1;
    for (const std::string& pattern : patterns) nodes += pattern.size();
    automaton.reserve_nodes(static_cast<int>(nodes));
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();
    return automaton.count_all_matches(text);
}

// テキストの各位置を右端として終わるパターン数を求める
// 想定ユースケース:
//   各位置で何個の辞書語やイベントパターンが完成するかを後段DPへ渡す
// 入力:
//   patterns: 各要素が空でない検索パターン列
//   text: 検索対象文字列
// 出力:
//   textと同じ長さの配列を返し、要素iは0-indexed位置iで終わるパターン数
//   同一パターンを複数登録した場合は登録数だけ重複して数える
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長とすると時間O(PK+N)、追加メモリO(VK+N)
template <int K = 26, int BASE = 'a'>
std::vector<int> count_pattern_occurrences_ending_at_each_position(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickCount<K, BASE> automaton;
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();
    return automaton.count_ending_at_each_position(text);
}

// テキストの各位置を左端として始まるパターン数を求める
// 想定ユースケース:
//   各位置から開始できる辞書語数を求め、前向きDPや区間遷移へ利用する
// 入力:
//   patterns: 各要素が空でない検索パターン列
//   text: 検索対象文字列
// 出力:
//   textと同じ長さの配列を返し、要素iは0-indexed位置iから始まるパターン数
//   同一パターンを複数登録した場合は登録数だけ重複して数える
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長とすると時間O(PK+N)、追加メモリO(VK+N)
template <int K = 26, int BASE = 'a'>
std::vector<int> count_pattern_occurrences_starting_at_each_position(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickCount<K, BASE> automaton;
    for (const std::string& pattern : patterns) {
        std::string reversed = pattern;
        std::reverse(reversed.begin(), reversed.end());
        automaton.add(reversed);
    }
    automaton.build();

    std::vector<int> result(text.size());
    int state = 0;
    for (int i = static_cast<int>(text.size()) - 1; i >= 0; --i) {
        state = automaton.step(state, text[i]);
        result[i] = automaton.match_count(state);
    }
    return result;
}

// ============================================================
// AhoCorasickEnumerate
// ============================================================

// 全マッチ列挙に特化したAho-Corasick
// BEGIN_DATA_STRUCTURE: AhoCorasickEnumerate
template <int K = 26, int BASE = 'a'>
struct AhoCorasickEnumerate {
    static_assert(K > 0);

private:
    using Transition = std::array<int, K>;

    std::vector<Transition> next_;
    std::vector<int> fail_;
    std::vector<int> pattern_node_;
    std::vector<int> pattern_length_;
    std::vector<int> output_begin_;
    std::vector<int> output_ids_;
    std::vector<int> dict_link_;
    bool built_ = false;

    int add_node() {
        next_.emplace_back();
        next_.back().fill(-1);
        fail_.push_back(0);
        return static_cast<int>(next_.size()) - 1;
    }

    static int symbol_index(char ch) {
        const int symbol = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= symbol && symbol < K);
        return symbol;
    }

public:
    AhoCorasickEnumerate() {
        add_node();
    }

    // 状態数とパターン数分の容量を確保する O(V+P)
    void reserve(int node_capacity, int pattern_capacity) {
        next_.reserve(node_capacity);
        fail_.reserve(node_capacity);
        pattern_node_.reserve(pattern_capacity);
        pattern_length_.reserve(pattern_capacity);
    }

    // パターンを追加してIDを返す O(|pattern|K) 最悪、Kは固定定数
    int add(std::string_view pattern) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (char ch : pattern) {
            const int symbol = symbol_index(ch);
            if (next_[state][symbol] == -1) {
                next_[state][symbol] = add_node();
            }
            state = next_[state][symbol];
        }

        const int id = static_cast<int>(pattern_node_.size());
        pattern_node_.push_back(state);
        pattern_length_.push_back(static_cast<int>(pattern.size()));
        return id;
    }

    // fail link、全遷移、dictionary link、直接出力CSRを構築する O(VK+P)
    void build() {
        assert(!built_);
        const int states = static_cast<int>(next_.size());
        output_begin_.assign(states + 1, 0);

        // 各終端状態の直接出力数からCSRを構築する
        for (int state : pattern_node_) ++output_begin_[state + 1];
        for (int state = 0; state < states; ++state) {
            output_begin_[state + 1] += output_begin_[state];
        }
        output_ids_.assign(pattern_node_.size(), -1);
        std::vector<int> cursor = output_begin_;
        for (int id = 0; id < static_cast<int>(pattern_node_.size()); ++id) {
            output_ids_[cursor[pattern_node_[id]]++] = id;
        }

        // BFSでfail、dictionary link、完成済み遷移を構築する
        dict_link_.assign(states, -1);
        std::queue<int> q;
        for (int symbol = 0; symbol < K; ++symbol) {
            const int child = next_[0][symbol];
            if (child == -1) {
                next_[0][symbol] = 0;
            } else {
                fail_[child] = 0;
                q.push(child);
            }
        }

        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            const int fallback = fail_[state];
            dict_link_[state] = output_begin_[fallback] != output_begin_[fallback + 1]
                ? fallback
                : dict_link_[fallback];

            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next_[state][symbol];
                if (child == -1) {
                    next_[state][symbol] = next_[fallback][symbol];
                } else {
                    fail_[child] = next_[fallback][symbol];
                    q.push(child);
                }
            }
        }
        built_ = true;
    }

    // 状態で終わる全パターンIDを列挙する O(列挙数)
    template <class Callback>
    void for_each_output(int state, Callback&& callback) const {
        assert(built_);
        for (int output_state = state; output_state != -1;
             output_state = dict_link_[output_state]) {
            for (int i = output_begin_[output_state];
                 i < output_begin_[output_state + 1]; ++i) {
                callback(output_ids_[i]);
            }
        }
    }

    // テキスト中の全マッチを列挙する O(|text|+列挙数)
    template <class Callback>
    void enumerate_matches(std::string_view text, Callback&& callback) const {
        assert(built_);
        int state = 0;
        for (int right = 0; right < static_cast<int>(text.size()); ++right) {
            state = next_[state][symbol_index(text[right])];
            for_each_output(state, [&](int id) {
                callback(id, right - pattern_length_[id] + 1, right);
            });
        }
    }

    // パターン長を返す O(1)
    int pattern_length(int pattern_id) const {
        return pattern_length_[pattern_id];
    }

    // パターン数を返す O(1)
    int pattern_count() const {
        return static_cast<int>(pattern_node_.size());
    }

    // 1文字読んだ後の状態を返す O(1)
    int step(int state, char ch) const {
        assert(built_);
        return next_[state][symbol_index(ch)];
    }

    // 0以上K未満の文字番号を1つ読んだ後の状態を返す O(1)
    int step_index(int state, int symbol) const {
        assert(built_);
        assert(0 <= symbol && symbol < K);
        return next_[state][symbol];
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(next_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasickEnumerate

// AhoCorasickEnumerateを使うソルバー関数

// テキスト中の全パターン出現区間を列挙する
// 想定ユースケース:
//   パターンIDと出現位置が必要な区間処理、イベント生成、文字列DP
// 入力:
//   patterns: 各要素が空でない検索パターン列で、添字をpattern_idとして扱う
//   text: 検索対象文字列
// 出力:
//   全マッチのpattern_idと0-indexed閉区間[left, right]を格納した配列
//   重なりと同一文字列の重複登録もすべて列挙する
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長、Zを列挙マッチ数とすると
//   時間O(PK+N+Z)、追加メモリO(VK+Z)
template <int K = 26, int BASE = 'a'>
std::vector<AhoCorasickMatch> enumerate_pattern_occurrences(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickEnumerate<K, BASE> automaton;
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();

    std::vector<AhoCorasickMatch> result;
    automaton.enumerate_matches(text, [&](int id, int left, int right) {
        result.push_back({id, left, right});
    });
    return result;
}

// テキストを右から走査し、開始位置基準で全パターン出現区間を列挙する
// 想定ユースケース:
//   各位置から始まる全辞書語を列挙し、前向きDPやグラフの辺として利用する
// 入力:
//   patterns: 各要素が空でない検索パターン列で、添字をpattern_idとして扱う
//   text: 検索対象文字列
// 出力:
//   全マッチのpattern_idと0-indexed閉区間[left, right]を格納した配列
//   列挙順は開始位置の昇順を保証しない
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長、Zを列挙マッチ数とすると
//   時間O(PK+N+Z)、追加メモリO(VK+Z)
template <int K = 26, int BASE = 'a'>
std::vector<AhoCorasickMatch> enumerate_starting_pattern_occurrences(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickEnumerate<K, BASE> automaton;
    for (const std::string& pattern : patterns) {
        std::string reversed = pattern;
        std::reverse(reversed.begin(), reversed.end());
        automaton.add(reversed);
    }
    automaton.build();

    std::vector<AhoCorasickMatch> result;
    int state = 0;
    for (int left = static_cast<int>(text.size()) - 1; left >= 0; --left) {
        state = automaton.step(state, text[left]);
        automaton.for_each_output(state, [&](int id) {
            result.push_back({id, left, left + static_cast<int>(patterns[id].size()) - 1});
        });
    }
    return result;
}

// 文字列全体を辞書語の列へ分割する方法数を数える
// 想定ユースケース:
//   word breakの数え上げ版として、各辞書語マッチをDP遷移に変換する
// 入力:
//   text: 分割対象文字列
//   dictionary: 各要素が空でない辞書語列
//   テンプレート引数Value: 答えとDPに使う値型
//     intから構築でき、operator+=を利用できる型を指定する
//     剰余が必要ならatcoder::modint998244353などのACL modint型を指定する
// 出力:
//   text全体を辞書語だけで分割する方法数をValue型で返す
//   同じ辞書語を複数登録した場合は別の選択肢として数える
// 処理量:
//   Pを辞書総長、Vを状態数、Nをtext長、Zを辞書語マッチ数とすると
//   時間O(PK+N+Z)、追加メモリO(VK+N)
template <class Value, int K = 26, int BASE = 'a'>
Value count_dictionary_segmentations(
    std::string_view text,
    const std::vector<std::string>& dictionary
) {
    AhoCorasickEnumerate<K, BASE> automaton;
    for (const std::string& word : dictionary) automaton.add(word);
    automaton.build();

    std::vector<Value> dp(text.size() + 1, Value{});
    dp[0] = Value{1};
    int state = 0;

    // 各終了位置で終わる辞書語の開始位置からDPを遷移する
    for (int right = 0; right < static_cast<int>(text.size()); ++right) {
        state = automaton.step(state, text[right]);
        automaton.for_each_output(state, [&](int id) {
            const int left = right - automaton.pattern_length(id) + 1;
            dp[right + 1] += dp[left];
        });
    }
    return dp[text.size()];
}

// 文字列全体を辞書語へ分割するときの最小単語数を求める
// 想定ユースケース:
//   word breakの最適化版として、各辞書語マッチをコスト1のDP遷移に変換する
// 入力:
//   text: 分割対象文字列
//   dictionary: 各要素が空でない辞書語列
// 出力:
//   text全体を分割する最小単語数、分割不可能なら-1
// 処理量:
//   Pを辞書総長、Vを状態数、Nをtext長、Zを辞書語マッチ数とすると
//   時間O(PK+N+Z)、追加メモリO(VK+N)
template <int K = 26, int BASE = 'a'>
int minimum_dictionary_segments(
    std::string_view text,
    const std::vector<std::string>& dictionary
) {
    AhoCorasickEnumerate<K, BASE> automaton;
    for (const std::string& word : dictionary) automaton.add(word);
    automaton.build();

    const int inf = static_cast<int>(text.size()) + 1;
    std::vector<int> dp(text.size() + 1, inf);
    dp[0] = 0;
    int state = 0;

    // 各マッチ区間を1本の分割遷移として最小値を更新する
    for (int right = 0; right < static_cast<int>(text.size()); ++right) {
        state = automaton.step(state, text[right]);
        automaton.for_each_output(state, [&](int id) {
            const int left = right - automaton.pattern_length(id) + 1;
            if (dp[left] != inf) {
                dp[right + 1] = std::min(dp[right + 1], dp[left] + 1);
            }
        });
    }
    return dp[text.size()] == inf ? -1 : dp[text.size()];
}

// いずれかのパターン出現区間に覆われるテキスト位置数を求める
// 想定ユースケース:
//   辞書語や検出区間をすべて重ね合わせ、少なくとも1区間に含まれる文字数を数える
// 入力:
//   text: 検索対象文字列
//   patterns: 各要素が空でない検索パターン列
// 出力:
//   1つ以上の出現区間に含まれるtext内位置の総数
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長、Zを列挙マッチ数とすると
//   時間O(PK+N+Z)、追加メモリO(VK+N)
template <int K = 26, int BASE = 'a'>
int covered_length_by_pattern_occurrences(
    std::string_view text,
    const std::vector<std::string>& patterns
) {
    AhoCorasickEnumerate<K, BASE> automaton;
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();

    std::vector<int> difference(text.size() + 1, 0);
    automaton.enumerate_matches(text, [&](int, int left, int right) {
        ++difference[left];
        --difference[right + 1];
    });

    int active = 0;
    int result = 0;
    for (int i = 0; i < static_cast<int>(text.size()); ++i) {
        active += difference[i];
        result += active > 0;
    }
    return result;
}

// 1文字ワイルドカードを含むパターンの全開始位置を求める
// 想定ユースケース:
//   '?'など任意の1文字に一致する記号を含む1本のパターンを、固定部分へ分解して照合する
// 入力:
//   text: 検索対象文字列
//   pattern: wildcardが任意の1文字に一致する検索パターン
//   wildcard: ワイルドカードとして扱う文字
// 出力:
//   patternが一致する0-indexed開始位置を昇順に格納した配列
// 処理量:
//   Mをpattern長、Fを固定部分総長、Vを状態数、Nをtext長、Zを固定部分マッチ数とすると
//   時間O(M+FK+N+Z)、追加メモリO(VK+N)
template <int K = 26, int BASE = 'a'>
std::vector<int> find_wildcard_pattern_occurrences(
    std::string_view text,
    std::string_view pattern,
    char wildcard = '?'
) {
    const int text_length = static_cast<int>(text.size());
    const int pattern_length = static_cast<int>(pattern.size());
    if (pattern_length > text_length) return {};
    if (pattern_length == 0) {
        std::vector<int> result(text_length + 1);
        std::iota(result.begin(), result.end(), 0);
        return result;
    }

    struct Fragment {
        std::string text;
        int offset;
    };
    std::vector<Fragment> fragments;

    // ワイルドカード以外の最大区間へ分解する
    for (int i = 0; i < pattern_length;) {
        if (pattern[i] == wildcard) {
            ++i;
            continue;
        }
        const int begin = i;
        while (i < pattern_length && pattern[i] != wildcard) ++i;
        fragments.push_back({std::string(pattern.substr(begin, i - begin)), begin});
    }

    if (fragments.empty()) {
        std::vector<int> result(text_length - pattern_length + 1);
        std::iota(result.begin(), result.end(), 0);
        return result;
    }

    AhoCorasickEnumerate<K, BASE> automaton;
    for (const Fragment& fragment : fragments) automaton.add(fragment.text);
    automaton.build();
    std::vector<int> hit(text_length - pattern_length + 1, 0);

    // 固定部分の出現位置を元パターン開始位置へ変換して一致数を数える
    automaton.enumerate_matches(text, [&](int id, int left, int) {
        const int start = left - fragments[id].offset;
        if (0 <= start && start + pattern_length <= text_length) {
            ++hit[start];
        }
    });

    std::vector<int> result;
    for (int start = 0; start < static_cast<int>(hit.size()); ++start) {
        if (hit[start] == static_cast<int>(fragments.size())) {
            result.push_back(start);
        }
    }
    return result;
}

// 全パターンを少なくとも1回含む最短かつ辞書順最小の文字列を求める
// 想定ユースケース:
//   パターン数が小さいとき、AC状態と出現済み集合の直積グラフをBFSするshortest superstring型問題
// 入力:
//   patterns: 各要素が空でないパターン列で、要素数は20以下
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とし、その順を辞書順とする
// 出力:
//   解が存在すれば最短解のうち辞書順最小の文字列、存在しなければstd::nullopt
// 処理量:
//   Pをパターン総長、Vを状態数、Rをパターン数とすると
//   時間O(PK+V2^R K)、追加メモリO(VK+V2^R)
template <int K = 26, int BASE = 'a'>
std::optional<std::string> shortest_string_containing_all_patterns(
    const std::vector<std::string>& patterns
) {
    const int pattern_count = static_cast<int>(patterns.size());
    assert(pattern_count <= 20);
    if (pattern_count == 0) return std::string();

    AhoCorasickEnumerate<K, BASE> automaton;
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();

    const int states = automaton.state_count();
    const int mask_count = 1 << pattern_count;
    const int full_mask = mask_count - 1;
    std::vector<int> state_mask(states, 0);
    for (int state = 0; state < states; ++state) {
        automaton.for_each_output(state, [&](int id) {
            state_mask[state] |= 1 << id;
        });
    }

    const std::size_t total = static_cast<std::size_t>(states) * mask_count;
    assert(total <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    std::vector<int> parent(total, -2);
    std::vector<char> parent_char(total, 0);
    auto encode = [states](int state, int mask) {
        return mask * states + state;
    };

    std::queue<int> q;
    const int start = encode(0, state_mask[0]);
    parent[start] = -1;
    q.push(start);
    int goal = -1;

    // 文字番号の昇順に辺を張るBFSで最短かつ辞書順最小の到達列を得る
    while (!q.empty() && goal == -1) {
        const int encoded = q.front();
        q.pop();
        const int state = encoded % states;
        const int mask = encoded / states;
        if (mask == full_mask) {
            goal = encoded;
            break;
        }

        for (int symbol = 0; symbol < K; ++symbol) {
            const int next_state = automaton.step_index(state, symbol);
            const int next_mask = mask | state_mask[next_state];
            const int next_encoded = encode(next_state, next_mask);
            if (parent[next_encoded] != -2) continue;
            parent[next_encoded] = encoded;
            parent_char[next_encoded] = static_cast<char>(BASE + symbol);
            q.push(next_encoded);
        }
    }

    if (goal == -1) return std::nullopt;
    std::string result;
    while (parent[goal] != -1) {
        result.push_back(parent_char[goal]);
        goal = parent[goal];
    }
    std::reverse(result.begin(), result.end());
    return result;
}

// ============================================================
// AhoCorasickFailTree
// ============================================================

// 各パターンの出現回数とfail木処理に特化したAho-Corasick
// BEGIN_DATA_STRUCTURE: AhoCorasickFailTree
template <int K = 26, int BASE = 'a'>
struct AhoCorasickFailTree {
    static_assert(K > 0);

    struct EulerTour {
        std::vector<int> tin;
        std::vector<int> tout;
    };

private:
    using Transition = std::array<int, K>;

    std::vector<Transition> next_;
    std::vector<int> fail_;
    std::vector<int> pattern_node_;
    std::vector<int> bfs_order_;
    bool built_ = false;

    int add_node() {
        next_.emplace_back();
        next_.back().fill(-1);
        fail_.push_back(0);
        return static_cast<int>(next_.size()) - 1;
    }

    static int symbol_index(char ch) {
        const int symbol = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= symbol && symbol < K);
        return symbol;
    }

public:
    AhoCorasickFailTree() {
        add_node();
    }

    // 状態数とパターン数分の容量を確保する O(V+P)
    void reserve(int node_capacity, int pattern_capacity) {
        next_.reserve(node_capacity);
        fail_.reserve(node_capacity);
        pattern_node_.reserve(pattern_capacity);
        bfs_order_.reserve(node_capacity);
    }

    // パターンを追加してIDを返す O(|pattern|K) 最悪、Kは固定定数
    int add(std::string_view pattern) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (char ch : pattern) {
            const int symbol = symbol_index(ch);
            if (next_[state][symbol] == -1) {
                next_[state][symbol] = add_node();
            }
            state = next_[state][symbol];
        }

        const int id = static_cast<int>(pattern_node_.size());
        pattern_node_.push_back(state);
        return id;
    }

    // fail link、全遷移、BFS順を構築する O(VK)
    void build() {
        assert(!built_);
        bfs_order_.clear();
        bfs_order_.reserve(next_.size() - 1);
        std::queue<int> q;

        // root直下のfailと、存在しないroot遷移を確定する
        for (int symbol = 0; symbol < K; ++symbol) {
            const int child = next_[0][symbol];
            if (child == -1) {
                next_[0][symbol] = 0;
            } else {
                fail_[child] = 0;
                q.push(child);
            }
        }

        // 浅い状態からfail、BFS順、完成済み遷移を構築する
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            bfs_order_.push_back(state);

            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next_[state][symbol];
                if (child == -1) {
                    next_[state][symbol] = next_[fail_[state]][symbol];
                } else {
                    fail_[child] = next_[fail_[state]][symbol];
                    q.push(child);
                }
            }
        }
        built_ = true;
    }

    // テキスト中の各パターン出現回数を返す O(|text|+V+P)
    std::vector<long long> count_each_pattern(std::string_view text) const {
        assert(built_);
        std::vector<long long> count(next_.size(), 0);
        int state = 0;

        // 各テキスト位置で到達した最長suffix状態を数える
        for (char ch : text) {
            state = next_[state][symbol_index(ch)];
            ++count[state];
        }

        // fail木の深い状態から親へ訪問数を集約する
        for (int i = static_cast<int>(bfs_order_.size()) - 1; i >= 0; --i) {
            const int node = bfs_order_[i];
            count[fail_[node]] += count[node];
        }

        std::vector<long long> result(pattern_node_.size());
        for (int id = 0; id < static_cast<int>(pattern_node_.size()); ++id) {
            result[id] = count[pattern_node_[id]];
        }
        return result;
    }

    // fail木のEuler Tour区間を構築する O(V)
    EulerTour make_euler_tour() const {
        assert(built_);
        const int states = static_cast<int>(next_.size());
        std::vector<int> child_begin(states + 1, 0);
        for (int state = 1; state < states; ++state) {
            ++child_begin[fail_[state] + 1];
        }
        for (int state = 0; state < states; ++state) {
            child_begin[state + 1] += child_begin[state];
        }

        // fail木の子をCSR形式へ格納する
        std::vector<int> children(states - 1);
        std::vector<int> cursor = child_begin;
        for (int state = 1; state < states; ++state) {
            children[cursor[fail_[state]]++] = state;
        }

        EulerTour result;
        result.tin.resize(states);
        result.tout.resize(states);
        std::vector<int> next_child(states, 0);
        std::vector<int> stack;
        stack.reserve(states);
        stack.push_back(0);
        int timer = 0;

        // 再帰を使わずに各状態の入退場時刻を付ける
        result.tin[0] = timer++;
        while (!stack.empty()) {
            const int state = stack.back();
            const int begin = child_begin[state];
            const int end = child_begin[state + 1];
            if (begin + next_child[state] < end) {
                const int child = children[begin + next_child[state]++];
                result.tin[child] = timer++;
                stack.push_back(child);
            } else {
                result.tout[state] = timer;
                stack.pop_back();
            }
        }
        return result;
    }

    // パターンIDに対応する終端状態を返す O(1)
    int pattern_node(int pattern_id) const {
        return pattern_node_[pattern_id];
    }

    // 状態のfail link先を返す O(1)
    int fail_link(int state) const {
        assert(built_);
        return fail_[state];
    }

    // BFS順を返す O(1)
    const std::vector<int>& bfs_order() const {
        assert(built_);
        return bfs_order_;
    }

    // 1文字読んだ後の状態を返す O(1)
    int step(int state, char ch) const {
        assert(built_);
        return next_[state][symbol_index(ch)];
    }

    // 0以上K未満の文字番号を1つ読んだ後の状態を返す O(1)
    int step_index(int state, int symbol) const {
        assert(built_);
        assert(0 <= symbol && symbol < K);
        return next_[state][symbol];
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(next_.size());
    }

    // パターン数を返す O(1)
    int pattern_count() const {
        return static_cast<int>(pattern_node_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasickFailTree

// AhoCorasickFailTreeを使うソルバー関数

// 各パターンがテキスト中に何回現れるかを一括計算する
// 想定ユースケース:
//   出現位置の列挙をせず、入力順の各パターンについて重なりを含む頻度だけを求める
// 入力:
//   patterns: 各要素が空でない検索パターン列で、添字をpattern_idとして扱う
//   text: 検索対象文字列
// 出力:
//   patternsと同じ長さの配列を返し、要素iはpatterns[i]の出現回数
//   同一文字列を複数登録した場合、それぞれ同じ回数を返す
// 処理量:
//   Pをパターン総長、Vを状態数、Rをパターン数、Nをtext長とすると
//   時間O(PK+N+V+R)、追加メモリO(VK+V+R)
template <int K = 26, int BASE = 'a'>
std::vector<long long> count_each_pattern_occurrence(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickFailTree<K, BASE> automaton;
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();
    return automaton.count_each_pattern(text);
}

struct AhoCorasickPrefixQuery {
    int prefix_length = 0;
    int pattern_id = 0;
};

// 指定したテキストprefix内でのパターン出現回数クエリへ一括で答える
// 想定ユースケース:
//   prefix長とpattern_idが異なる多数のオフラインクエリをfail木の部分木和として処理する
// 入力:
//   patterns: 各要素が空でない検索パターン列で、添字をpattern_idとして扱う
//   text: 検索対象文字列
//   queries: prefix_lengthがtext[0, prefix_length)を表し、pattern_idが対象パターンを表すクエリ列
// 出力:
//   queriesと同じ順序の配列を返し、各要素は指定prefix内の対象パターン出現回数
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長、Qをクエリ数とすると
//   時間O(PK+V+Qlog Q+(N+Q)log V)、追加メモリO(VK+V+Q)
template <int K = 26, int BASE = 'a'>
std::vector<long long> count_pattern_occurrences_in_prefixes(
    const std::vector<std::string>& patterns,
    std::string_view text,
    const std::vector<AhoCorasickPrefixQuery>& queries
) {
    struct FenwickTree {
        int n = 0;
        std::vector<long long> bit;

        explicit FenwickTree(int size) : n(size), bit(size + 1, 0) {}

        // 位置indexへvalueを加算する O(log n)
        void add(int index, long long value) {
            for (int i = index + 1; i <= n; i += i & -i) {
                bit[i] += value;
            }
        }

        // 半開区間[0, right)の総和を返す O(log n)
        long long prefix_sum(int right) const {
            long long result = 0;
            for (int i = right; i > 0; i -= i & -i) {
                result += bit[i];
            }
            return result;
        }

        // 半開区間[left, right)の総和を返す O(log n)
        long long range_sum(int left, int right) const {
            return prefix_sum(right) - prefix_sum(left);
        }
    };

    AhoCorasickFailTree<K, BASE> automaton;
    for (const std::string& pattern : patterns) automaton.add(pattern);
    automaton.build();
    const auto tour = automaton.make_euler_tour();

    std::vector<int> order(queries.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        return queries[lhs].prefix_length < queries[rhs].prefix_length;
    });

    FenwickTree fenwick(automaton.state_count());
    std::vector<long long> result(queries.size());
    int state = 0;
    int processed = 0;

    // prefix長の昇順に状態訪問を追加し、fail部分木和を答える
    for (int query_id : order) {
        const auto& query = queries[query_id];
        assert(0 <= query.prefix_length && query.prefix_length <= static_cast<int>(text.size()));
        assert(0 <= query.pattern_id && query.pattern_id < automaton.pattern_count());
        while (processed < query.prefix_length) {
            state = automaton.step(state, text[processed++]);
            fenwick.add(tour.tin[state], 1);
        }
        const int node = automaton.pattern_node(query.pattern_id);
        result[query_id] = fenwick.range_sum(tour.tin[node], tour.tout[node]);
    }
    return result;
}

// ============================================================
// AhoCorasickWeighted
// ============================================================

// パターン出現の加算重みを管理するAho-Corasick
// BEGIN_DATA_STRUCTURE: AhoCorasickWeighted
template <int K = 26, int BASE = 'a'>
struct AhoCorasickWeighted {
    static_assert(K > 0);

private:
    using Transition = std::array<int, K>;

    std::vector<Transition> next_;
    std::vector<int> fail_;
    std::vector<long long> output_score_;
    bool built_ = false;

    int add_node() {
        next_.emplace_back();
        next_.back().fill(-1);
        fail_.push_back(0);
        output_score_.push_back(0);
        return static_cast<int>(next_.size()) - 1;
    }

    static int symbol_index(char ch) {
        const int symbol = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= symbol && symbol < K);
        return symbol;
    }

public:
    AhoCorasickWeighted() {
        add_node();
    }

    // 状態数分の容量を確保する O(V)
    void reserve_nodes(int capacity) {
        next_.reserve(capacity);
        fail_.reserve(capacity);
        output_score_.reserve(capacity);
    }

    // 重み付きパターンを追加する O(|pattern|K) 最悪、Kは固定定数
    void add(std::string_view pattern, long long weight) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (char ch : pattern) {
            const int symbol = symbol_index(ch);
            if (next_[state][symbol] == -1) {
                next_[state][symbol] = add_node();
            }
            state = next_[state][symbol];
        }
        output_score_[state] += weight;
    }

    // fail link、全遷移、状態ごとの出現重みを構築する O(VK)
    void build() {
        assert(!built_);
        std::queue<int> q;

        // root直下のfailと、存在しないroot遷移を確定する
        for (int symbol = 0; symbol < K; ++symbol) {
            const int child = next_[0][symbol];
            if (child == -1) {
                next_[0][symbol] = 0;
            } else {
                fail_[child] = 0;
                q.push(child);
            }
        }

        // 浅い状態からfail、出現重み、完成済み遷移を構築する
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            output_score_[state] += output_score_[fail_[state]];

            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next_[state][symbol];
                if (child == -1) {
                    next_[state][symbol] = next_[fail_[state]][symbol];
                } else {
                    fail_[child] = next_[fail_[state]][symbol];
                    q.push(child);
                }
            }
        }
        built_ = true;
    }

    // テキスト中の全パターン出現重みを返す O(|text|)
    long long score_text(std::string_view text) const {
        assert(built_);
        int state = 0;
        long long result = 0;
        for (char ch : text) {
            state = next_[state][symbol_index(ch)];
            result += output_score_[state];
        }
        return result;
    }

    // 状態へ遷移した瞬間に得る重みを返す O(1)
    long long transition_score(int state) const {
        assert(built_);
        return output_score_[state];
    }

    // 1文字読んだ後の状態を返す O(1)
    int step(int state, char ch) const {
        assert(built_);
        return next_[state][symbol_index(ch)];
    }

    // 0以上K未満の文字番号を1つ読んだ後の状態を返す O(1)
    int step_index(int state, int symbol) const {
        assert(built_);
        assert(0 <= symbol && symbol < K);
        return next_[state][symbol];
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(next_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasickWeighted

// AhoCorasickWeightedを使うソルバー関数

// テキスト中の重み付きパターン出現得点を合計する
// 想定ユースケース:
//   各パターンの出現ごとに報酬またはペナルティが発生する既存テキストを評価する
// 入力:
//   patterns: 空でないパターンと出現1回当たりの重みの組
//   text: 評価対象文字列
// 出力:
//   重なりを含む全出現の重み合計
//   同じパターンを複数登録した場合は各重みをすべて加算する
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長とすると時間O(PK+N)、追加メモリO(VK)
template <int K = 26, int BASE = 'a'>
long long score_pattern_occurrences(
    const std::vector<std::pair<std::string, long long>>& patterns,
    std::string_view text
) {
    AhoCorasickWeighted<K, BASE> automaton;
    for (const auto& [pattern, weight] : patterns) automaton.add(pattern, weight);
    automaton.build();
    return automaton.score_text(text);
}

// 指定長文字列を自由に作ったときに得られる最大パターン出現得点を求める
// 想定ユースケース:
//   BASEから始まる連続K文字を各位置で選び、ボーナス語やペナルティ語の合計を最大化する文字列DP
// 入力:
//   length: 作る文字列の長さ
//   patterns: 空でないパターンと出現1回当たりの重みの組
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とする
// 出力:
//   長さlengthの文字列で達成できる最大得点
// 処理量:
//   Pをパターン総長、Vを状態数、Lをlengthとすると
//   時間O(PK+LVK)、追加メモリO(VK+V)
template <int K = 26, int BASE = 'a'>
long long maximum_score_string(
    int length,
    const std::vector<std::pair<std::string, long long>>& patterns
) {
    assert(length >= 0);
    AhoCorasickWeighted<K, BASE> automaton;
    for (const auto& [pattern, weight] : patterns) automaton.add(pattern, weight);
    automaton.build();

    const long long negative_infinity = std::numeric_limits<long long>::lowest() / 4;
    const int states = automaton.state_count();
    std::vector<long long> dp(states, negative_infinity), next_dp(states, negative_infinity);
    dp[0] = 0;

    // 文字を1つ追加した際の出現得点を加えて最大値を更新する
    for (int position = 0; position < length; ++position) {
        std::fill(next_dp.begin(), next_dp.end(), negative_infinity);
        for (int state = 0; state < states; ++state) {
            if (dp[state] == negative_infinity) continue;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int next_state = automaton.step_index(state, symbol);
                next_dp[next_state] = std::max(
                    next_dp[next_state],
                    dp[state] + automaton.transition_score(next_state)
                );
            }
        }
        dp.swap(next_dp);
    }
    return *std::max_element(dp.begin(), dp.end());
}

// 最大得点を達成する指定長文字列のうち辞書順最小のものを求める
// 想定ユースケース:
//   得点最大化を第一目的、辞書順最小化を第二目的として最適文字列を復元する
// 入力:
//   length: 作る文字列の長さ
//   patterns: 空でないパターンと出現1回当たりの重みの組
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とし、その順を辞書順とする
// 出力:
//   最大得点を達成する長さlengthの文字列のうち辞書順最小のもの
// 処理量:
//   Pをパターン総長、Vを状態数、Lをlengthとすると
//   時間O(PK+LVK)、追加メモリO(VK+LV)
template <int K = 26, int BASE = 'a'>
std::string lexicographically_smallest_maximum_score_string(
    int length,
    const std::vector<std::pair<std::string, long long>>& patterns
) {
    assert(length >= 0);
    AhoCorasickWeighted<K, BASE> automaton;
    for (const auto& [pattern, weight] : patterns) automaton.add(pattern, weight);
    automaton.build();

    const long long negative_infinity = std::numeric_limits<long long>::lowest() / 4;
    const int states = automaton.state_count();
    std::vector<long long> best(static_cast<std::size_t>(length + 1) * states, 0);
    auto at = [&](int remaining, int state) -> long long& {
        return best[static_cast<std::size_t>(remaining) * states + state];
    };

    // 各状態から残り文字数で得られる最大追加得点を計算する
    for (int remaining = 1; remaining <= length; ++remaining) {
        for (int state = 0; state < states; ++state) {
            long long value = negative_infinity;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int next_state = automaton.step_index(state, symbol);
                value = std::max(
                    value,
                    automaton.transition_score(next_state) + at(remaining - 1, next_state)
                );
            }
            at(remaining, state) = value;
        }
    }

    // 最大得点を維持できる文字のうち最小のものを各位置で選ぶ
    std::string result;
    result.reserve(length);
    int state = 0;
    for (int remaining = length; remaining > 0; --remaining) {
        for (int symbol = 0; symbol < K; ++symbol) {
            const int next_state = automaton.step_index(state, symbol);
            const long long candidate =
                automaton.transition_score(next_state) + at(remaining - 1, next_state);
            if (candidate == at(remaining, state)) {
                result.push_back(static_cast<char>(BASE + symbol));
                state = next_state;
                break;
            }
        }
    }
    return result;
}

// 元文字列を同じ長さの文字列へ置換したときの純得点最大値を求める
// 想定ユースケース:
//   パターン出現報酬と文字変更コストを同時に考慮して、各位置の文字を最適化する
// 入力:
//   source: 変更前の文字列
//   patterns: 空でないパターンと出現1回当たりの重みの組
//   change_cost: sourceと異なる文字を1位置へ置くたびに引くコスト
//   テンプレート引数K, BASE: 置換後に使用できる文字をBASEから始まる連続K文字とする
// 出力:
//   パターン出現得点合計から変更コスト合計を引いた値の最大値
// 処理量:
//   Pをパターン総長、Vを状態数、Nをsource長とすると
//   時間O(PK+NVK)、追加メモリO(VK+V)
template <int K = 26, int BASE = 'a'>
long long maximum_score_after_replacements(
    std::string_view source,
    const std::vector<std::pair<std::string, long long>>& patterns,
    long long change_cost = 1
) {
    AhoCorasickWeighted<K, BASE> automaton;
    for (const auto& [pattern, weight] : patterns) automaton.add(pattern, weight);
    automaton.build();

    const long long negative_infinity = std::numeric_limits<long long>::lowest() / 4;
    const int states = automaton.state_count();
    std::vector<long long> dp(states, negative_infinity), next_dp(states, negative_infinity);
    dp[0] = 0;

    // 文字変更コストとパターン出現得点を同時に評価する
    for (int position = 0; position < static_cast<int>(source.size()); ++position) {
        std::fill(next_dp.begin(), next_dp.end(), negative_infinity);
        for (int state = 0; state < states; ++state) {
            if (dp[state] == negative_infinity) continue;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int next_state = automaton.step_index(state, symbol);
                const char ch = static_cast<char>(BASE + symbol);
                const long long candidate = dp[state]
                    + automaton.transition_score(next_state)
                    - (source[position] == ch ? 0 : change_cost);
                next_dp[next_state] = std::max(next_dp[next_state], candidate);
            }
        }
        dp.swap(next_dp);
    }
    return *std::max_element(dp.begin(), dp.end());
}

// ============================================================
// AhoCorasickWeightedForbidden
// ============================================================

// 禁止パターンと加算重みを同時に管理するAho-Corasick
// BEGIN_DATA_STRUCTURE: AhoCorasickWeightedForbidden
template <int K = 26, int BASE = 'a'>
struct AhoCorasickWeightedForbidden {
    static_assert(K > 0);

private:
    using Transition = std::array<int, K>;

    std::vector<Transition> next_;
    std::vector<int> fail_;
    std::vector<long long> output_score_;
    std::vector<std::uint8_t> bad_;
    bool built_ = false;

    int add_node() {
        next_.emplace_back();
        next_.back().fill(-1);
        fail_.push_back(0);
        output_score_.push_back(0);
        bad_.push_back(0);
        return static_cast<int>(next_.size()) - 1;
    }

    static int symbol_index(char ch) {
        const int symbol = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= symbol && symbol < K);
        return symbol;
    }

    int add_pattern(std::string_view pattern) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (char ch : pattern) {
            const int symbol = symbol_index(ch);
            if (next_[state][symbol] == -1) {
                next_[state][symbol] = add_node();
            }
            state = next_[state][symbol];
        }
        return state;
    }

public:
    AhoCorasickWeightedForbidden() {
        add_node();
    }

    // 状態数分の容量を確保する O(V)
    void reserve_nodes(int capacity) {
        next_.reserve(capacity);
        fail_.reserve(capacity);
        output_score_.reserve(capacity);
        bad_.reserve(capacity);
    }

    // 重み付きパターンを追加する O(|pattern|K) 最悪、Kは固定定数
    void add_weighted(std::string_view pattern, long long weight) {
        output_score_[add_pattern(pattern)] += weight;
    }

    // 禁止パターンを追加する O(|pattern|K) 最悪、Kは固定定数
    void add_forbidden(std::string_view pattern) {
        bad_[add_pattern(pattern)] = 1;
    }

    // fail link、全遷移、出現重み、禁止状態を構築する O(VK)
    void build() {
        assert(!built_);
        std::queue<int> q;

        // root直下のfailと、存在しないroot遷移を確定する
        for (int symbol = 0; symbol < K; ++symbol) {
            const int child = next_[0][symbol];
            if (child == -1) {
                next_[0][symbol] = 0;
            } else {
                fail_[child] = 0;
                q.push(child);
            }
        }

        // 浅い状態からfail、付加情報、完成済み遷移を構築する
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            output_score_[state] += output_score_[fail_[state]];
            bad_[state] = static_cast<std::uint8_t>(
                bad_[state] || bad_[fail_[state]]
            );

            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next_[state][symbol];
                if (child == -1) {
                    next_[state][symbol] = next_[fail_[state]][symbol];
                } else {
                    fail_[child] = next_[fail_[state]][symbol];
                    q.push(child);
                }
            }
        }
        built_ = true;
    }

    // 状態が禁止状態か返す O(1)
    bool is_bad_state(int state) const {
        assert(built_);
        return bad_[state] != 0;
    }

    // 状態へ遷移した瞬間に得る重みを返す O(1)
    long long transition_score(int state) const {
        assert(built_);
        return output_score_[state];
    }

    // 1文字読んだ後の状態を返す O(1)
    int step(int state, char ch) const {
        assert(built_);
        return next_[state][symbol_index(ch)];
    }

    // 0以上K未満の文字番号を1つ読んだ後の状態を返す O(1)
    int step_index(int state, int symbol) const {
        assert(built_);
        assert(0 <= symbol && symbol < K);
        return next_[state][symbol];
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(next_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasickWeightedForbidden

// AhoCorasickWeightedForbiddenを使うソルバー関数

// 禁止パターンを避けながら指定長文字列の最大得点を求める
// 想定ユースケース:
//   禁止語を一切出さず、BASEから始まる連続K文字でボーナス語やペナルティ語の出現得点を最大化する
// 入力:
//   length: 作る文字列の長さ
//   weighted: 空でない得点パターンと出現1回当たりの重みの組
//   forbidden: 各要素が空でない禁止パターン列
//   テンプレート引数K, BASE: BASEから始まる連続K文字すべてを使用可能文字とする
// 出力:
//   実現可能なら最大得点、条件を満たす文字列が存在しなければstd::nullopt
// 処理量:
//   Pをweightedとforbiddenの総パターン長、Vを状態数、Lをlengthとすると
//   時間O(PK+LVK)、追加メモリO(VK+V)
template <int K = 26, int BASE = 'a'>
std::optional<long long> maximum_score_string_avoiding_patterns(
    int length,
    const std::vector<std::pair<std::string, long long>>& weighted,
    const std::vector<std::string>& forbidden
) {
    assert(length >= 0);
    AhoCorasickWeightedForbidden<K, BASE> automaton;
    for (const auto& [pattern, weight] : weighted) {
        automaton.add_weighted(pattern, weight);
    }
    for (const std::string& pattern : forbidden) {
        automaton.add_forbidden(pattern);
    }
    automaton.build();

    const long long negative_infinity = std::numeric_limits<long long>::lowest() / 4;
    const int states = automaton.state_count();
    std::vector<long long> dp(states, negative_infinity), next_dp(states, negative_infinity);
    if (!automaton.is_bad_state(0)) dp[0] = 0;

    // 禁止状態を除外しながら重み付き遷移の最大値を更新する
    for (int position = 0; position < length; ++position) {
        std::fill(next_dp.begin(), next_dp.end(), negative_infinity);
        for (int state = 0; state < states; ++state) {
            if (dp[state] == negative_infinity) continue;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int next_state = automaton.step_index(state, symbol);
                if (automaton.is_bad_state(next_state)) continue;
                next_dp[next_state] = std::max(
                    next_dp[next_state],
                    dp[state] + automaton.transition_score(next_state)
                );
            }
        }
        dp.swap(next_dp);
    }

    const long long result = *std::max_element(dp.begin(), dp.end());
    if (result == negative_infinity) return std::nullopt;
    return result;
}

// ============================================================
// AhoCorasickBestMatch
// ============================================================

// 各状態で最良の1パターンだけを保持するAho-Corasick
// LONGEST_THEN_ID=trueは最長優先、falseは登録ID最小優先とする
// BEGIN_DATA_STRUCTURE: AhoCorasickBestMatch
template <int K = 26, int BASE = 'a', bool LONGEST_THEN_ID = true>
struct AhoCorasickBestMatch {
    static_assert(K > 0);

    struct Candidate {
        int pattern_id = -1;
        int length = 0;
    };

private:
    using Transition = std::array<int, K>;

    std::vector<Transition> next_;
    std::vector<int> fail_;
    std::vector<Candidate> best_;
    int pattern_count_ = 0;
    bool built_ = false;

    int add_node() {
        next_.emplace_back();
        next_.back().fill(-1);
        fail_.push_back(0);
        best_.emplace_back();
        return static_cast<int>(next_.size()) - 1;
    }

    static int symbol_index(char ch) {
        const int symbol = static_cast<unsigned char>(ch) - BASE;
        assert(0 <= symbol && symbol < K);
        return symbol;
    }

    static Candidate better(Candidate lhs, Candidate rhs) {
        if (lhs.pattern_id == -1) return rhs;
        if (rhs.pattern_id == -1) return lhs;

        if constexpr (LONGEST_THEN_ID) {
            if (lhs.length != rhs.length) {
                return lhs.length > rhs.length ? lhs : rhs;
            }
        }
        return lhs.pattern_id < rhs.pattern_id ? lhs : rhs;
    }

public:
    AhoCorasickBestMatch() {
        add_node();
    }

    // 状態数分の容量を確保する O(V)
    void reserve_nodes(int capacity) {
        next_.reserve(capacity);
        fail_.reserve(capacity);
        best_.reserve(capacity);
    }

    // パターンを追加してIDを返す O(|pattern|K) 最悪、Kは固定定数
    int add(std::string_view pattern) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (char ch : pattern) {
            const int symbol = symbol_index(ch);
            if (next_[state][symbol] == -1) {
                next_[state][symbol] = add_node();
            }
            state = next_[state][symbol];
        }

        const int id = pattern_count_++;
        best_[state] = better(
            best_[state],
            Candidate{id, static_cast<int>(pattern.size())}
        );
        return id;
    }

    // fail link、全遷移、各状態の最良マッチを構築する O(VK)
    void build() {
        assert(!built_);
        std::queue<int> q;

        // root直下のfailと、存在しないroot遷移を確定する
        for (int symbol = 0; symbol < K; ++symbol) {
            const int child = next_[0][symbol];
            if (child == -1) {
                next_[0][symbol] = 0;
            } else {
                fail_[child] = 0;
                q.push(child);
            }
        }

        // 浅い状態からfail、最良候補、完成済み遷移を構築する
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            best_[state] = better(best_[state], best_[fail_[state]]);

            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next_[state][symbol];
                if (child == -1) {
                    next_[state][symbol] = next_[fail_[state]][symbol];
                } else {
                    fail_[child] = next_[fail_[state]][symbol];
                    q.push(child);
                }
            }
        }
        built_ = true;
    }

    // 状態で得られる最良マッチを返す O(1)
    Candidate best_match(int state) const {
        assert(built_);
        return best_[state];
    }

    // 1文字読んだ後の状態を返す O(1)
    int step(int state, char ch) const {
        assert(built_);
        return next_[state][symbol_index(ch)];
    }

    // 0以上K未満の文字番号を1つ読んだ後の状態を返す O(1)
    int step_index(int state, int symbol) const {
        assert(built_);
        assert(0 <= symbol && symbol < K);
        return next_[state][symbol];
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(next_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasickBestMatch

// AhoCorasickBestMatchを使うソルバー関数

// leftmost-longest規則で採用した非重複マッチ列を返す
// 想定ユースケース:
//   左から最初に始まるマッチを採用し、同じ開始位置なら最長一致を優先する置換や字句解析
// 入力:
//   patterns: 各要素が空でない検索パターン列で、添字を登録優先度付きpattern_idとして扱う
//   text: 検索対象文字列
// 出力:
//   左から採用した非重複マッチのpattern_idと0-indexed閉区間[left, right]
//   同じ開始位置かつ同じ長さならpattern_idが小さいものを選ぶ
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長とすると時間O(PK+N)、追加メモリO(VK+N)
template <int K = 26, int BASE = 'a'>
std::vector<AhoCorasickMatch> leftmost_longest_non_overlapping_matches(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickBestMatch<K, BASE, true> automaton;
    for (const std::string& pattern : patterns) {
        std::string reversed = pattern;
        std::reverse(reversed.begin(), reversed.end());
        automaton.add(reversed);
    }
    automaton.build();

    std::vector<typename decltype(automaton)::Candidate> best(text.size());
    int state = 0;
    for (int i = static_cast<int>(text.size()) - 1; i >= 0; --i) {
        state = automaton.step(state, text[i]);
        best[i] = automaton.best_match(state);
    }

    std::vector<AhoCorasickMatch> result;
    for (int left = 0; left < static_cast<int>(text.size());) {
        if (best[left].pattern_id == -1) {
            ++left;
            continue;
        }
        const int right = left + best[left].length - 1;
        result.push_back({best[left].pattern_id, left, right});
        left = right + 1;
    }
    return result;
}

// leftmost-first規則で採用した非重複マッチ列を返す
// 想定ユースケース:
//   左から最初に始まるマッチを採用し、同じ開始位置なら登録順を優先する置換や字句解析
// 入力:
//   patterns: 各要素が空でない検索パターン列で、添字が登録優先度を表す
//   text: 検索対象文字列
// 出力:
//   左から採用した非重複マッチのpattern_idと0-indexed閉区間[left, right]
//   同じ開始位置では長さに関係なくpattern_idが小さいものを選ぶ
// 処理量:
//   Pをパターン総長、Vを状態数、Nをtext長とすると時間O(PK+N)、追加メモリO(VK+N)
template <int K = 26, int BASE = 'a'>
std::vector<AhoCorasickMatch> leftmost_first_non_overlapping_matches(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    AhoCorasickBestMatch<K, BASE, false> automaton;
    for (const std::string& pattern : patterns) {
        std::string reversed = pattern;
        std::reverse(reversed.begin(), reversed.end());
        automaton.add(reversed);
    }
    automaton.build();

    std::vector<typename decltype(automaton)::Candidate> best(text.size());
    int state = 0;
    for (int i = static_cast<int>(text.size()) - 1; i >= 0; --i) {
        state = automaton.step(state, text[i]);
        best[i] = automaton.best_match(state);
    }

    std::vector<AhoCorasickMatch> result;
    for (int left = 0; left < static_cast<int>(text.size());) {
        if (best[left].pattern_id == -1) {
            ++left;
            continue;
        }
        const int right = left + best[left].length - 1;
        result.push_back({best[left].pattern_id, left, right});
        left = right + 1;
    }
    return result;
}

// ============================================================
// AhoCorasickSparseCount
// ============================================================

// 大きなalphabetの出現数集約に特化したsparse Aho-Corasick
// BEGIN_DATA_STRUCTURE: AhoCorasickSparseCount
template <class Symbol = int, class Hash = std::hash<Symbol>>
struct AhoCorasickSparseCount {
private:
    struct EdgeKey {
        int state;
        Symbol symbol;

        bool operator==(const EdgeKey& rhs) const {
            return state == rhs.state && symbol == rhs.symbol;
        }
    };

    struct EdgeKeyHash {
        Hash symbol_hash;

        std::size_t operator()(const EdgeKey& key) const {
            const std::size_t h1 = std::hash<int>{}(key.state);
            const std::size_t h2 = symbol_hash(key.symbol);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };

    std::unordered_map<EdgeKey, int, EdgeKeyHash> edge_;
    std::vector<std::vector<std::pair<Symbol, int>>> children_{1};
    std::vector<int> fail_{0};
    std::vector<int> output_count_{0};
    bool built_ = false;

    int add_node() {
        children_.emplace_back();
        fail_.push_back(0);
        output_count_.push_back(0);
        return static_cast<int>(children_.size()) - 1;
    }

    int direct_next(int state, const Symbol& symbol) const {
        const auto it = edge_.find(EdgeKey{state, symbol});
        return it == edge_.end() ? -1 : it->second;
    }

public:
    AhoCorasickSparseCount() = default;

    // 状態数と辺数分の容量を確保する O(V)
    void reserve(int node_capacity, int edge_capacity) {
        children_.reserve(node_capacity);
        fail_.reserve(node_capacity);
        output_count_.reserve(node_capacity);
        edge_.reserve(edge_capacity);
    }

    // シンボル列パターンを追加する 期待O(|pattern|)
    void add(std::span<const Symbol> pattern) {
        assert(!built_);
        assert(!pattern.empty());

        int state = 0;
        for (const Symbol& symbol : pattern) {
            const EdgeKey key{state, symbol};
            const auto it = edge_.find(key);
            if (it == edge_.end()) {
                const int child = add_node();
                edge_.emplace(key, child);
                children_[state].push_back({symbol, child});
                state = child;
            } else {
                state = it->second;
            }
        }
        ++output_count_[state];
    }

    // fail linkと状態ごとのマッチ数を構築する 期待O(L×最大fail深さ)、通常はほぼO(L)
    void build() {
        assert(!built_);
        std::queue<int> q;

        // root直下のfailを設定する
        for (const auto& [symbol, child] : children_[0]) {
            static_cast<void>(symbol);
            fail_[child] = 0;
            q.push(child);
        }

        // 各trie辺についてfail先から同じシンボルの辺を探す
        while (!q.empty()) {
            const int state = q.front();
            q.pop();
            output_count_[state] += output_count_[fail_[state]];

            for (const auto& [symbol, child] : children_[state]) {
                int fallback = fail_[state];
                int to = direct_next(fallback, symbol);
                while (fallback != 0 && to == -1) {
                    fallback = fail_[fallback];
                    to = direct_next(fallback, symbol);
                }
                fail_[child] = to == -1 ? 0 : to;
                q.push(child);
            }
        }
        built_ = true;
    }

    // シンボルを1つ読んだ後の状態を返す 走査全体で償却期待O(1)
    int step(int state, const Symbol& symbol) const {
        assert(built_);
        int to = direct_next(state, symbol);
        while (state != 0 && to == -1) {
            state = fail_[state];
            to = direct_next(state, symbol);
        }
        return to == -1 ? 0 : to;
    }

    // シンボル列中の全パターン出現数を返す 償却期待O(|text|)
    long long count_all_matches(std::span<const Symbol> text) const {
        int state = 0;
        long long result = 0;
        for (const Symbol& symbol : text) {
            state = step(state, symbol);
            result += output_count_[state];
        }
        return result;
    }

    // シンボル列がいずれかのパターンを含むか返す 償却期待O(|text|)
    bool contains_any(std::span<const Symbol> text) const {
        int state = 0;
        for (const Symbol& symbol : text) {
            state = step(state, symbol);
            if (output_count_[state] > 0) return true;
        }
        return false;
    }

    // 状態数を返す O(1)
    int state_count() const {
        return static_cast<int>(children_.size());
    }
};
// END_DATA_STRUCTURE: AhoCorasickSparseCount

// AhoCorasickSparseCountを使うソルバー関数

// 大きなシンボル集合上の列に現れる全パターンの総出現回数を数える
// 想定ユースケース:
//   整数列、トークン列、色列など、固定幅dense遷移表を持ちにくい連続パターン検索
// 入力:
//   patterns: 各要素が空でないシンボル列パターン
//   text: 検索対象列を参照するstd::span
// 出力:
//   重なりを含む全パターン出現回数の合計
//   同一パターンを複数登録した場合は登録数だけ重複して数える
// 処理量:
//   Pをパターン総長、Dを最大fail深さ、Nをtext長とすると
//   ハッシュ表操作の期待O(1)の下で時間O(PD+N)、通常はほぼ期待O(P+N)、追加メモリO(P)
template <class Symbol = int, class Hash = std::hash<Symbol>>
long long count_integer_sequence_pattern_occurrences(
    const std::vector<std::vector<Symbol>>& patterns,
    std::span<const Symbol> text
) {
    AhoCorasickSparseCount<Symbol, Hash> automaton;
    std::size_t total_length = 0;
    for (const auto& pattern : patterns) total_length += pattern.size();
    automaton.reserve(static_cast<int>(total_length + 1), static_cast<int>(total_length));
    for (const auto& pattern : patterns) automaton.add(pattern);
    automaton.build();
    return automaton.count_all_matches(text);
}

// vectorで受け取った大きなシンボル集合上の列について総出現回数を数える
// 想定ユースケース:
//   std::vectorの整数列やトークン列を、そのままsparse Aho-Corasickへ渡して検索する
// 入力:
//   patterns: 各要素が空でないシンボル列パターン
//   text: std::vectorで保持された検索対象列
// 出力:
//   重なりを含む全パターン出現回数の合計
//   同一パターンを複数登録した場合は登録数だけ重複して数える
// 処理量:
//   Pをパターン総長、Dを最大fail深さ、Nをtext長とすると
//   ハッシュ表操作の期待O(1)の下で時間O(PD+N)、通常はほぼ期待O(P+N)、追加メモリO(P)
template <class Symbol = int, class Hash = std::hash<Symbol>>
long long count_integer_sequence_pattern_occurrences(
    const std::vector<std::vector<Symbol>>& patterns,
    const std::vector<Symbol>& text
) {
    AhoCorasickSparseCount<Symbol, Hash> automaton;
    std::size_t total_length = 0;
    for (const auto& pattern : patterns) total_length += pattern.size();
    automaton.reserve(static_cast<int>(total_length + 1), static_cast<int>(total_length));
    for (const auto& pattern : patterns) automaton.add(pattern);
    automaton.build();
    return automaton.count_all_matches(std::span<const Symbol>(text));
}

// ============================================================
// DynamicAhoCorasickCount
// ============================================================

// パターン追加に対応する二進分解型Aho-Corasick
// BEGIN_DATA_STRUCTURE: DynamicAhoCorasickCount
template <int K = 26, int BASE = 'a'>
struct DynamicAhoCorasickCount {
    static_assert(K > 0);

private:
    using Transition = std::array<int, K>;

    struct Bucket {
        std::vector<std::string> patterns;
        std::vector<Transition> next;
        std::vector<int> fail;
        std::vector<int> output_count;

        static int symbol_index(char ch) {
            const int symbol = static_cast<unsigned char>(ch) - BASE;
            assert(0 <= symbol && symbol < K);
            return symbol;
        }

        int add_node() {
            next.emplace_back();
            next.back().fill(-1);
            fail.push_back(0);
            output_count.push_back(0);
            return static_cast<int>(next.size()) - 1;
        }

        bool empty() const {
            return patterns.empty();
        }

        void clear() {
            *this = Bucket{};
        }

        void rebuild() {
            next.clear();
            fail.clear();
            output_count.clear();

            std::size_t node_capacity = 1;
            for (const std::string& pattern : patterns) node_capacity += pattern.size();
            next.reserve(node_capacity);
            fail.reserve(node_capacity);
            output_count.reserve(node_capacity);
            add_node();

            // bucket内の全パターンからtrieを構築する
            for (const std::string& pattern : patterns) {
                assert(!pattern.empty());
                int state = 0;
                for (char ch : pattern) {
                    const int symbol = symbol_index(ch);
                    if (next[state][symbol] == -1) {
                        next[state][symbol] = add_node();
                    }
                    state = next[state][symbol];
                }
                ++output_count[state];
            }

            // BFSでfail、出現数、完成済み遷移を構築する
            std::queue<int> q;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next[0][symbol];
                if (child == -1) {
                    next[0][symbol] = 0;
                } else {
                    fail[child] = 0;
                    q.push(child);
                }
            }
            while (!q.empty()) {
                const int state = q.front();
                q.pop();
                output_count[state] += output_count[fail[state]];
                for (int symbol = 0; symbol < K; ++symbol) {
                    const int child = next[state][symbol];
                    if (child == -1) {
                        next[state][symbol] = next[fail[state]][symbol];
                    } else {
                        fail[child] = next[fail[state]][symbol];
                        q.push(child);
                    }
                }
            }
        }

        long long count_all_matches(std::string_view text) const {
            int state = 0;
            long long result = 0;
            for (char ch : text) {
                state = next[state][symbol_index(ch)];
                result += output_count[state];
            }
            return result;
        }
    };

    std::vector<Bucket> buckets_;

public:
    DynamicAhoCorasickCount() = default;

    // パターンを追加する 償却O(追加された文字数×log M×K)
    void add(std::string pattern) {
        assert(!pattern.empty());
        std::vector<std::string> carry;
        carry.push_back(std::move(pattern));

        // 同じサイズのbucketを二進数の繰り上がりと同様に統合する
        for (int level = 0;; ++level) {
            if (level == static_cast<int>(buckets_.size())) {
                buckets_.emplace_back();
            }
            if (buckets_[level].empty()) {
                buckets_[level].patterns = std::move(carry);
                buckets_[level].rebuild();
                return;
            }

            carry.reserve(carry.size() + buckets_[level].patterns.size());
            for (std::string& existing : buckets_[level].patterns) {
                carry.push_back(std::move(existing));
            }
            buckets_[level].clear();
        }
    }

    // 現在の全パターン出現数を返す O(|text|log M)
    long long count_all_matches(std::string_view text) const {
        long long result = 0;
        for (const Bucket& bucket : buckets_) {
            if (!bucket.empty()) result += bucket.count_all_matches(text);
        }
        return result;
    }

    // 非空bucket数を返す O(log M)
    int active_bucket_count() const {
        int result = 0;
        for (const Bucket& bucket : buckets_) result += !bucket.empty();
        return result;
    }
};
// END_DATA_STRUCTURE: DynamicAhoCorasickCount

// DynamicAhoCorasickCountは操作列中で直接add/count_all_matchesを呼ぶ

// ============================================================
// DynamicAhoCorasickMultisetCount
// ============================================================

// パターン追加と削除を差分で管理する動的multiset Aho-Corasick
// BEGIN_DATA_STRUCTURE: DynamicAhoCorasickMultisetCount
template <int K = 26, int BASE = 'a'>
struct DynamicAhoCorasickMultisetCount {
    static_assert(K > 0);

private:
    using Transition = std::array<int, K>;

    struct Bucket {
        std::vector<std::string> patterns;
        std::vector<Transition> next;
        std::vector<int> fail;
        std::vector<int> output_count;

        static int symbol_index(char ch) {
            const int symbol = static_cast<unsigned char>(ch) - BASE;
            assert(0 <= symbol && symbol < K);
            return symbol;
        }

        int add_node() {
            next.emplace_back();
            next.back().fill(-1);
            fail.push_back(0);
            output_count.push_back(0);
            return static_cast<int>(next.size()) - 1;
        }

        bool empty() const {
            return patterns.empty();
        }

        void clear() {
            *this = Bucket{};
        }

        void rebuild() {
            next.clear();
            fail.clear();
            output_count.clear();

            std::size_t node_capacity = 1;
            for (const std::string& pattern : patterns) node_capacity += pattern.size();
            next.reserve(node_capacity);
            fail.reserve(node_capacity);
            output_count.reserve(node_capacity);
            add_node();

            // bucket内の全パターンからtrieを構築する
            for (const std::string& pattern : patterns) {
                assert(!pattern.empty());
                int state = 0;
                for (char ch : pattern) {
                    const int symbol = symbol_index(ch);
                    if (next[state][symbol] == -1) {
                        next[state][symbol] = add_node();
                    }
                    state = next[state][symbol];
                }
                ++output_count[state];
            }

            // BFSでfail、出現数、完成済み遷移を構築する
            std::queue<int> q;
            for (int symbol = 0; symbol < K; ++symbol) {
                const int child = next[0][symbol];
                if (child == -1) {
                    next[0][symbol] = 0;
                } else {
                    fail[child] = 0;
                    q.push(child);
                }
            }
            while (!q.empty()) {
                const int state = q.front();
                q.pop();
                output_count[state] += output_count[fail[state]];
                for (int symbol = 0; symbol < K; ++symbol) {
                    const int child = next[state][symbol];
                    if (child == -1) {
                        next[state][symbol] = next[fail[state]][symbol];
                    } else {
                        fail[child] = next[fail[state]][symbol];
                        q.push(child);
                    }
                }
            }
        }

        long long count_all_matches(std::string_view text) const {
            int state = 0;
            long long result = 0;
            for (char ch : text) {
                state = next[state][symbol_index(ch)];
                result += output_count[state];
            }
            return result;
        }
    };

    std::vector<Bucket> positive_;
    std::vector<Bucket> negative_;

    static void add_to(std::vector<Bucket>& buckets, std::string pattern) {
        assert(!pattern.empty());
        std::vector<std::string> carry;
        carry.push_back(std::move(pattern));

        // 同じサイズのbucketを二進数の繰り上がりと同様に統合する
        for (int level = 0;; ++level) {
            if (level == static_cast<int>(buckets.size())) {
                buckets.emplace_back();
            }
            if (buckets[level].empty()) {
                buckets[level].patterns = std::move(carry);
                buckets[level].rebuild();
                return;
            }

            carry.reserve(carry.size() + buckets[level].patterns.size());
            for (std::string& existing : buckets[level].patterns) {
                carry.push_back(std::move(existing));
            }
            buckets[level].clear();
        }
    }

    static long long count_in(
        const std::vector<Bucket>& buckets,
        std::string_view text
    ) {
        long long result = 0;
        for (const Bucket& bucket : buckets) {
            if (!bucket.empty()) result += bucket.count_all_matches(text);
        }
        return result;
    }

public:
    DynamicAhoCorasickMultisetCount() = default;

    // パターンを1個追加する 償却O(|pattern|log M×K)
    void add(std::string pattern) {
        add_to(positive_, std::move(pattern));
    }

    // 現在存在するパターンを1個削除扱いにする 償却O(|pattern|log M×K)
    void erase(std::string pattern) {
        add_to(negative_, std::move(pattern));
    }

    // 現在有効な全パターン出現数を返す O(|text|log M)
    long long count_all_matches(std::string_view text) const {
        return count_in(positive_, text) - count_in(negative_, text);
    }
};
// END_DATA_STRUCTURE: DynamicAhoCorasickMultisetCount

// DynamicAhoCorasickMultisetCountは操作列中で直接add/erase/count_all_matchesを呼ぶ

// ============================================================
// テスト
// ============================================================

#if __INCLUDE_LEVEL__ == 0

namespace aho_corasick_test {

template <std::uint32_t MOD>
struct TestModInt {
    std::uint32_t value = 0;

    TestModInt() = default;

    TestModInt(long long x) {
        long long normalized = x % static_cast<long long>(MOD);
        if (normalized < 0) normalized += static_cast<long long>(MOD);
        value = static_cast<std::uint32_t>(normalized);
    }

    TestModInt& operator+=(const TestModInt& rhs) {
        value += rhs.value;
        if (value >= MOD) value -= MOD;
        return *this;
    }

    friend TestModInt operator*(const TestModInt& lhs, const TestModInt& rhs) {
        return TestModInt{
            static_cast<long long>(
                static_cast<std::uint64_t>(lhs.value) * rhs.value % MOD
            )
        };
    }

    friend bool operator==(const TestModInt&, const TestModInt&) = default;

    std::uint32_t val() const {
        return value;
    }
};

using TestMint = TestModInt<1'000'000'007>;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "TEST FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::vector<AhoCorasickMatch> naive_matches(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    std::vector<AhoCorasickMatch> result;
    for (int id = 0; id < static_cast<int>(patterns.size()); ++id) {
        const std::string& pattern = patterns[id];
        for (int left = 0; left + static_cast<int>(pattern.size()) <= static_cast<int>(text.size()); ++left) {
            if (text.substr(left, pattern.size()) == pattern) {
                result.push_back({id, left, left + static_cast<int>(pattern.size()) - 1});
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

long long naive_total_count(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    return static_cast<long long>(naive_matches(patterns, text).size());
}

std::vector<int> naive_ending_count(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    std::vector<int> result(text.size(), 0);
    for (const auto& match : naive_matches(patterns, text)) ++result[match.right];
    return result;
}

std::vector<int> naive_starting_count(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    std::vector<int> result(text.size(), 0);
    for (const auto& match : naive_matches(patterns, text)) ++result[match.left];
    return result;
}

std::vector<long long> naive_each_count(
    const std::vector<std::string>& patterns,
    std::string_view text
) {
    std::vector<long long> result(patterns.size(), 0);
    for (const auto& match : naive_matches(patterns, text)) ++result[match.pattern_id];
    return result;
}

bool avoids(std::string_view text, const std::vector<std::string>& forbidden) {
    for (const std::string& pattern : forbidden) {
        if (text.find(pattern) != std::string_view::npos) return false;
    }
    return true;
}

long long naive_score(
    std::string_view text,
    const std::vector<std::pair<std::string, long long>>& patterns
) {
    long long result = 0;
    for (const auto& [pattern, weight] : patterns) {
        for (int left = 0; left + static_cast<int>(pattern.size()) <= static_cast<int>(text.size()); ++left) {
            if (text.substr(left, pattern.size()) == pattern) result += weight;
        }
    }
    return result;
}

template <class Callback>
void enumerate_strings(int length, std::string_view alphabet, Callback&& callback) {
    std::string current(length, '\0');
    auto dfs = [&](auto&& self, int position) -> void {
        if (position == length) {
            callback(current);
            return;
        }
        for (char ch : alphabet) {
            current[position] = ch;
            self(self, position + 1);
        }
    };
    dfs(dfs, 0);
}

unsigned long long naive_digit_count(
    unsigned long long upper_bound,
    const std::vector<std::string>& forbidden
) {
    unsigned long long result = 0;
    for (unsigned long long value = 0; value <= upper_bound; ++value) {
        if (avoids(std::to_string(value), forbidden)) ++result;
        if (value == upper_bound) break;
    }
    return result;
}

std::optional<std::string> naive_shortest_all_patterns(
    const std::vector<std::string>& patterns,
    std::string_view alphabet
) {
    int maximum_length = 0;
    for (const std::string& pattern : patterns) {
        maximum_length += static_cast<int>(pattern.size());
    }
    for (int length = 0; length <= maximum_length; ++length) {
        std::optional<std::string> answer;
        enumerate_strings(length, alphabet, [&](const std::string& candidate) {
            if (answer) return;
            bool ok = true;
            for (const std::string& pattern : patterns) {
                if (candidate.find(pattern) == std::string::npos) ok = false;
            }
            if (ok) answer = candidate;
        });
        if (answer) return answer;
    }
    return std::nullopt;
}

std::vector<AhoCorasickMatch> naive_leftmost(
    const std::vector<std::string>& patterns,
    std::string_view text,
    bool longest
) {
    std::vector<AhoCorasickMatch> result;
    for (int left = 0; left < static_cast<int>(text.size());) {
        int best_id = -1;
        int best_length = 0;
        for (int id = 0; id < static_cast<int>(patterns.size()); ++id) {
            const int length = static_cast<int>(patterns[id].size());
            if (left + length > static_cast<int>(text.size())) continue;
            if (text.substr(left, length) != patterns[id]) continue;
            if (best_id == -1
                || (longest && (length > best_length || (length == best_length && id < best_id)))
                || (!longest && id < best_id)) {
                best_id = id;
                best_length = length;
            }
        }
        if (best_id == -1) {
            ++left;
        } else {
            result.push_back({best_id, left, left + best_length - 1});
            left += best_length;
        }
    }
    return result;
}

void deterministic_tests() {
    {
        const std::vector<std::string> patterns;
        require(!contains_any_pattern(patterns, "abc"), "empty pattern set contains");
        require(count_all_pattern_occurrences(patterns, "abc") == 0, "empty pattern set count");
        require(enumerate_pattern_occurrences(patterns, "abc").empty(), "empty pattern set enumerate");
        require(
            count_strings_avoiding_patterns<TestMint>(1, patterns) == TestMint{26},
            "default character range count"
        );
        require(count_strings_avoiding_patterns<TestMint, 2, 'a'>(4, patterns) == TestMint{16}, "empty forbidden count");
        require(
            lexicographically_smallest_string_avoiding_patterns<2, 'a'>(3, patterns)
                == std::optional<std::string>("aaa"),
            "empty forbidden lexicographic"
        );
        require(shortest_string_containing_all_patterns<2, 'a'>(patterns) == std::optional<std::string>(""), "empty shortest all");
    }
    {
        const std::vector<std::string> patterns = {"he", "she", "his", "hers"};
        require(contains_any_pattern(patterns, "ushers"), "basic contains");
        require(!contains_any_pattern(patterns, "abc"), "basic not contains");
        require(count_all_pattern_occurrences(patterns, "ushers") == 3, "count total");
        require(
            count_pattern_occurrences_ending_at_each_position(patterns, "ushers")
                == std::vector<int>({0, 0, 0, 2, 0, 1}),
            "count ending"
        );
        auto actual = enumerate_pattern_occurrences(patterns, "ushers");
        std::sort(actual.begin(), actual.end());
        require(actual == naive_matches(patterns, "ushers"), "enumerate matches");
        require(
            count_each_pattern_occurrence(patterns, "ushers")
                == std::vector<long long>({1, 1, 0, 1}),
            "fail tree count"
        );
    }
    {
        const std::vector<std::string> patterns = {"a", "aa", "aaa"};
        require(count_all_pattern_occurrences(patterns, "aaaaa") == 12, "overlap count");
        require(
            count_pattern_occurrences_starting_at_each_position(patterns, "aaaaa")
                == std::vector<int>({3, 3, 3, 2, 1}),
            "reverse starting count"
        );
    }
    {
        const std::vector<std::string> patterns = {"abc", "abc", "bc"};
        require(
            count_each_pattern_occurrence(patterns, "abcabc")
                == std::vector<long long>({2, 2, 2}),
            "duplicate pattern count"
        );
    }
    {
        const std::vector<std::string> patterns = {"he", "she"};
        const std::vector<AhoCorasickPrefixQuery> queries = {
            {3, 0}, {4, 0}, {4, 1}, {6, 0}
        };
        require(
            count_pattern_occurrences_in_prefixes(patterns, "ushers", queries)
                == std::vector<long long>({0, 1, 1, 1}),
            "prefix queries"
        );
    }
    {
        const std::vector<std::string> forbidden = {"aa", "ab"};
        require(count_strings_avoiding_patterns<TestMint, 2, 'a'>(3, forbidden) == TestMint{2}, "forbidden count");
        require(
            lexicographically_smallest_string_avoiding_patterns<2, 'a'>(3, forbidden)
                == std::optional<std::string>("bba"),
            "forbidden lexicographic"
        );
        require(minimum_changes_to_avoid_patterns<2, 'a'>("aaa", forbidden) == 2, "minimum changes");
        require(
            count_strings_avoiding_patterns_large_length<TestMint, 2, 'a'>(8, forbidden)
                == count_strings_avoiding_patterns<TestMint, 2, 'a'>(8, forbidden),
            "matrix count"
        );
        require(exists_infinite_string_avoiding_patterns<2, 'a'>(forbidden), "infinite safe exists");
        require(!exists_infinite_string_avoiding_patterns<2, 'a'>(std::vector<std::string>{"a", "b"}), "infinite safe absent");
    }
    {
        const std::vector<std::string> forbidden = {"x", "y"};
        require(
            count_strings_avoiding_patterns<TestMint, 3, 'x'>(1, forbidden) == TestMint{1},
            "template character range count"
        );
        require(
            lexicographically_smallest_string_avoiding_patterns<3, 'x'>(1, forbidden)
                == std::optional<std::string>("z"),
            "template character range lexicographic"
        );
        require(
            minimum_changes_to_avoid_patterns<3, 'x'>("x", forbidden) == 1,
            "template character range replacement"
        );
    }
    {
        require(
            count_nonnegative_integers_avoiding_patterns(30, {"13"}) == 30,
            "digit dp"
        );
        require(
            count_nonnegative_integers_avoiding_patterns(20, {"0"}) == 18,
            "digit dp zero"
        );
    }
    {
        const std::vector<std::pair<std::string, long long>> weighted = {{"a", 1}, {"aa", 10}};
        require(score_pattern_occurrences(weighted, "aaa") == 23, "weighted text");
        require(maximum_score_string<2, 'a'>(3, weighted) == 23, "weighted maximum");
        require(
            lexicographically_smallest_maximum_score_string<2, 'a'>(3, weighted) == "aaa",
            "weighted lexicographic"
        );
        require(
            maximum_score_string_avoiding_patterns<2, 'a'>(3, weighted, {"aa"})
                == std::optional<long long>(2),
            "weighted forbidden"
        );
        require(maximum_score_after_replacements<2, 'a'>("bbb", weighted, 1) == 20, "weighted replacement");
    }
    {
        const std::vector<std::string> dictionary = {"a", "ab", "bc", "c"};
        require(count_dictionary_segmentations<TestMint>("abc", dictionary) == TestMint{2}, "segmentation count");
        require(minimum_dictionary_segments("abc", dictionary) == 2, "segmentation minimum");
        require(covered_length_by_pattern_occurrences("zabcx", dictionary) == 3, "covered length");
    }
    {
        require(
            find_wildcard_pattern_occurrences("abxcdabycd", "ab?cd") == std::vector<int>({0, 5}),
            "wildcard"
        );
        require(
            find_wildcard_pattern_occurrences("abc", "??") == std::vector<int>({0, 1}),
            "wildcard all"
        );
    }
    {
        const auto result = shortest_string_containing_all_patterns<2, 'a'>(
            std::vector<std::string>{"ab", "ba"}
        );
        require(result == std::optional<std::string>("aba"), "shortest all patterns");
    }
    {
        const std::vector<std::string> patterns = {"a", "ab", "abc", "bc"};
        require(
            leftmost_longest_non_overlapping_matches(patterns, "abc")
                == std::vector<AhoCorasickMatch>({{2, 0, 2}}),
            "leftmost longest"
        );
        require(
            leftmost_first_non_overlapping_matches(patterns, "abc")
                == std::vector<AhoCorasickMatch>({{0, 0, 0}, {3, 1, 2}}),
            "leftmost first"
        );
    }
    {
        const std::vector<std::vector<int>> patterns = {{1, 2, 3}, {2, 3}, {1, 2, 3}};
        const std::vector<int> text = {0, 1, 2, 3, 2, 3};
        require(count_integer_sequence_pattern_occurrences(patterns, text) == 4, "sparse integer");
    }
    {
        DynamicAhoCorasickCount<> dynamic;
        dynamic.add("he");
        dynamic.add("she");
        require(dynamic.count_all_matches("ushers") == 2, "dynamic insert");
        dynamic.add("hers");
        require(dynamic.count_all_matches("ushers") == 3, "dynamic carry");

        DynamicAhoCorasickMultisetCount<> multiset;
        multiset.add("he");
        multiset.add("she");
        multiset.add("hers");
        multiset.erase("she");
        require(multiset.count_all_matches("ushers") == 2, "dynamic erase");
    }
}

int random_int(std::mt19937_64& rng, int upper_exclusive) {
    assert(upper_exclusive > 0);
    return static_cast<int>(rng() % static_cast<unsigned long long>(upper_exclusive));
}

std::string random_string(std::mt19937_64& rng, int length, std::string_view alphabet) {
    std::string result(length, '\0');
    for (char& ch : result) {
        ch = alphabet[static_cast<std::size_t>(random_int(rng, static_cast<int>(alphabet.size())))];
    }
    return result;
}

void random_search_tests(std::mt19937_64& rng) {
    constexpr int iterations = 3000;
    const std::string alphabet = "abc";

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int pattern_count = random_int(rng, 7);
        std::vector<std::string> patterns;
        for (int i = 0; i < pattern_count; ++i) {
            patterns.push_back(random_string(rng, 1 + random_int(rng, 5), alphabet));
        }
        const std::string text = random_string(rng, random_int(rng, 11), alphabet);

        const auto expected_matches = naive_matches(patterns, text);
        auto actual_matches = enumerate_pattern_occurrences(patterns, text);
        std::sort(actual_matches.begin(), actual_matches.end());
        require(actual_matches == expected_matches, "random enumerate");
        require(count_all_pattern_occurrences(patterns, text) == static_cast<long long>(expected_matches.size()), "random total");
        require(count_pattern_occurrences_ending_at_each_position(patterns, text) == naive_ending_count(patterns, text), "random ending");
        require(count_pattern_occurrences_starting_at_each_position(patterns, text) == naive_starting_count(patterns, text), "random starting");
        require(count_each_pattern_occurrence(patterns, text) == naive_each_count(patterns, text), "random each");
        require(contains_any_pattern(patterns, text) == !expected_matches.empty(), "random contains");

        auto reverse_matches = enumerate_starting_pattern_occurrences(patterns, text);
        std::sort(reverse_matches.begin(), reverse_matches.end());
        require(reverse_matches == expected_matches, "random reverse enumerate");
        require(
            leftmost_longest_non_overlapping_matches(patterns, text)
                == naive_leftmost(patterns, text, true),
            "random leftmost longest"
        );
        require(
            leftmost_first_non_overlapping_matches(patterns, text)
                == naive_leftmost(patterns, text, false),
            "random leftmost first"
        );
        std::vector<std::uint8_t> covered(text.size(), 0);
        for (const auto& match : expected_matches) {
            for (int i = match.left; i <= match.right; ++i) covered[i] = 1;
        }
        const int expected_covered = std::accumulate(covered.begin(), covered.end(), 0);
        require(
            covered_length_by_pattern_occurrences(text, patterns) == expected_covered,
            "random covered length"
        );
    }
}

void random_dp_tests(std::mt19937_64& rng) {
    constexpr int iterations = 800;
    const std::string alphabet = "ab";

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int forbidden_count = random_int(rng, 4);
        std::vector<std::string> forbidden;
        for (int i = 0; i < forbidden_count; ++i) {
            forbidden.push_back(random_string(rng, 1 + random_int(rng, 4), alphabet));
        }
        const int length = random_int(rng, 7);

        long long expected_count = 0;
        std::optional<std::string> expected_lexicographic;
        enumerate_strings(length, alphabet, [&](const std::string& candidate) {
            if (!avoids(candidate, forbidden)) return;
            ++expected_count;
            if (!expected_lexicographic) expected_lexicographic = candidate;
        });

        require(
            count_strings_avoiding_patterns<TestMint, 2, 'a'>(length, forbidden)
                == TestMint{expected_count},
            "random forbidden count"
        );
        require(
            count_strings_avoiding_patterns_large_length<TestMint, 2, 'a'>(length, forbidden)
                == TestMint{expected_count},
            "random forbidden matrix"
        );
        require(
            lexicographically_smallest_string_avoiding_patterns<2, 'a'>(length, forbidden)
                == expected_lexicographic,
            "random forbidden lexicographic"
        );

        const std::string source = random_string(rng, length, alphabet);
        int expected_changes = length + 1;
        enumerate_strings(length, alphabet, [&](const std::string& candidate) {
            if (!avoids(candidate, forbidden)) return;
            int changes = 0;
            for (int i = 0; i < length; ++i) changes += candidate[i] != source[i];
            expected_changes = std::min(expected_changes, changes);
        });
        if (expected_changes == length + 1) expected_changes = -1;
        require(
            minimum_changes_to_avoid_patterns<2, 'a'>(source, forbidden) == expected_changes,
            "random minimum changes"
        );
    }
}

void random_weight_tests(std::mt19937_64& rng) {
    constexpr int iterations = 800;
    const std::string alphabet = "ab";

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int pattern_count = random_int(rng, 5);
        std::vector<std::pair<std::string, long long>> patterns;
        for (int i = 0; i < pattern_count; ++i) {
            patterns.push_back({
                random_string(rng, 1 + random_int(rng, 4), alphabet),
                static_cast<long long>(random_int(rng, 11) - 5)
            });
        }
        const std::string text = random_string(rng, random_int(rng, 8), alphabet);
        require(score_pattern_occurrences(patterns, text) == naive_score(text, patterns), "random weighted text");

        const int length = random_int(rng, 7);
        long long expected_score = std::numeric_limits<long long>::lowest();
        std::string expected_string;
        enumerate_strings(length, alphabet, [&](const std::string& candidate) {
            const long long score = naive_score(candidate, patterns);
            if (score > expected_score || (score == expected_score && candidate < expected_string)) {
                expected_score = score;
                expected_string = candidate;
            }
        });
        require(maximum_score_string<2, 'a'>(length, patterns) == expected_score, "random maximum score");
        require(
            lexicographically_smallest_maximum_score_string<2, 'a'>(length, patterns)
                == expected_string,
            "random maximum lexicographic"
        );

        const std::string source = random_string(rng, length, alphabet);
        const long long change_cost = static_cast<long long>(random_int(rng, 4));
        long long expected_replacement = std::numeric_limits<long long>::lowest();
        enumerate_strings(length, alphabet, [&](const std::string& candidate) {
            long long value = naive_score(candidate, patterns);
            for (int i = 0; i < length; ++i) {
                if (candidate[i] != source[i]) value -= change_cost;
            }
            expected_replacement = std::max(expected_replacement, value);
        });
        require(
            maximum_score_after_replacements<2, 'a'>(source, patterns, change_cost)
                == expected_replacement,
            "random weighted replacement"
        );

        std::vector<std::string> forbidden;
        const int forbidden_count = random_int(rng, 4);
        for (int i = 0; i < forbidden_count; ++i) {
            forbidden.push_back(random_string(rng, 1 + random_int(rng, 4), alphabet));
        }
        std::optional<long long> expected_safe_score;
        enumerate_strings(length, alphabet, [&](const std::string& candidate) {
            if (!avoids(candidate, forbidden)) return;
            const long long value = naive_score(candidate, patterns);
            if (!expected_safe_score || value > *expected_safe_score) {
                expected_safe_score = value;
            }
        });
        require(
            maximum_score_string_avoiding_patterns<2, 'a'>(length, patterns, forbidden)
                == expected_safe_score,
            "random weighted forbidden"
        );
    }
}

void random_segmentation_and_wildcard_tests(std::mt19937_64& rng) {
    constexpr int iterations = 800;
    const std::string alphabet = "ab";

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int dictionary_size = random_int(rng, 6);
        std::vector<std::string> dictionary;
        for (int i = 0; i < dictionary_size; ++i) {
            dictionary.push_back(random_string(rng, 1 + random_int(rng, 4), alphabet));
        }
        const std::string text = random_string(rng, random_int(rng, 9), alphabet);

        std::vector<long long> ways(text.size() + 1, 0);
        std::vector<int> minimum(text.size() + 1, static_cast<int>(text.size()) + 1);
        ways[0] = 1;
        minimum[0] = 0;
        for (int left = 0; left < static_cast<int>(text.size()); ++left) {
            for (const std::string& word : dictionary) {
                if (left + static_cast<int>(word.size()) <= static_cast<int>(text.size())
                    && text.substr(left, word.size()) == word) {
                    ways[left + word.size()] += ways[left];
                    if (minimum[left] <= static_cast<int>(text.size())) {
                        minimum[left + word.size()] = std::min(
                            minimum[left + word.size()], minimum[left] + 1
                        );
                    }
                }
            }
        }
        require(count_dictionary_segmentations<TestMint>(text, dictionary) == TestMint{ways.back()}, "random segmentation count");
        const int expected_minimum = minimum.back() > static_cast<int>(text.size()) ? -1 : minimum.back();
        require(minimum_dictionary_segments(text, dictionary) == expected_minimum, "random segmentation minimum");

        const int pattern_length = random_int(rng, 7);
        std::string pattern = random_string(rng, pattern_length, alphabet);
        for (char& ch : pattern) {
            if (random_int(rng, 3) == 0) ch = '?';
        }
        std::vector<int> expected;
        for (int start = 0; start + pattern_length <= static_cast<int>(text.size()); ++start) {
            bool ok = true;
            for (int i = 0; i < pattern_length; ++i) {
                if (pattern[i] != '?' && pattern[i] != text[start + i]) ok = false;
            }
            if (ok) expected.push_back(start);
        }
        require(find_wildcard_pattern_occurrences(text, pattern) == expected, "random wildcard");
    }
}

void random_sparse_and_dynamic_tests(std::mt19937_64& rng) {
    constexpr int iterations = 500;

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int pattern_count = random_int(rng, 7);
        std::vector<std::vector<int>> patterns;
        for (int i = 0; i < pattern_count; ++i) {
            const int length = 1 + random_int(rng, 5);
            std::vector<int> pattern(length);
            for (int& value : pattern) value = random_int(rng, 5) * 1000003 - 7;
            patterns.push_back(pattern);
        }
        const int text_length = random_int(rng, 11);
        std::vector<int> text(text_length);
        for (int& value : text) value = random_int(rng, 5) * 1000003 - 7;

        long long expected = 0;
        for (const auto& pattern : patterns) {
            for (int left = 0; left + static_cast<int>(pattern.size()) <= text_length; ++left) {
                if (std::equal(pattern.begin(), pattern.end(), text.begin() + left)) ++expected;
            }
        }
        require(count_integer_sequence_pattern_occurrences(patterns, text) == expected, "random sparse");

        DynamicAhoCorasickCount<3, 'a'> dynamic;
        std::vector<std::string> active;
        const int operations = 1 + random_int(rng, 30);
        for (int op = 0; op < operations; ++op) {
            if (active.empty() || random_int(rng, 2) == 0) {
                std::string pattern = random_string(rng, 1 + random_int(rng, 5), "abc");
                active.push_back(pattern);
                dynamic.add(pattern);
            } else {
                const std::string query = random_string(rng, random_int(rng, 10), "abc");
                require(dynamic.count_all_matches(query) == naive_total_count(active, query), "random dynamic");
            }
        }


        DynamicAhoCorasickMultisetCount<3, 'a'> multiset;
        std::vector<std::string> alive;
        for (int op = 0; op < operations; ++op) {
            const int type = random_int(rng, 3);
            if (alive.empty() || type == 0) {
                std::string pattern = random_string(rng, 1 + random_int(rng, 5), "abc");
                alive.push_back(pattern);
                multiset.add(pattern);
            } else if (type == 1) {
                const int index = random_int(rng, static_cast<int>(alive.size()));
                multiset.erase(alive[index]);
                alive.erase(alive.begin() + index);
            } else {
                const std::string query = random_string(rng, random_int(rng, 10), "abc");
                require(
                    multiset.count_all_matches(query) == naive_total_count(alive, query),
                    "random dynamic multiset"
                );
            }
        }
    }
}

void random_prefix_query_tests(std::mt19937_64& rng) {
    constexpr int iterations = 500;
    const std::string alphabet = "abc";

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int pattern_count = 1 + random_int(rng, 6);
        std::vector<std::string> patterns;
        for (int i = 0; i < pattern_count; ++i) {
            patterns.push_back(random_string(rng, 1 + random_int(rng, 5), alphabet));
        }
        const std::string text = random_string(rng, random_int(rng, 11), alphabet);
        const int query_count = 1 + random_int(rng, 15);
        std::vector<AhoCorasickPrefixQuery> queries(query_count);
        std::vector<long long> expected(query_count);
        for (int i = 0; i < query_count; ++i) {
            queries[i].prefix_length = random_int(rng, static_cast<int>(text.size()) + 1);
            queries[i].pattern_id = random_int(rng, pattern_count);
            const std::string_view prefix(text.data(), queries[i].prefix_length);
            expected[i] = naive_each_count(patterns, prefix)[queries[i].pattern_id];
        }
        require(count_pattern_occurrences_in_prefixes(patterns, text, queries) == expected, "random prefix queries");
    }
}

void random_digit_tests(std::mt19937_64& rng) {
    constexpr int iterations = 500;
    const std::string digits = "0123";

    for (int iteration = 0; iteration < iterations; ++iteration) {
        std::vector<std::string> forbidden;
        const int count = random_int(rng, 4);
        for (int i = 0; i < count; ++i) {
            forbidden.push_back(random_string(rng, 1 + random_int(rng, 3), digits));
        }
        const unsigned long long upper_bound = static_cast<unsigned long long>(random_int(rng, 501));
        require(
            count_nonnegative_integers_avoiding_patterns(upper_bound, forbidden)
                == naive_digit_count(upper_bound, forbidden),
            "random digit dp"
        );
    }
}

void random_shortest_all_tests(std::mt19937_64& rng) {
    constexpr int iterations = 300;
    const std::string alphabet = "ab";

    for (int iteration = 0; iteration < iterations; ++iteration) {
        std::vector<std::string> patterns;
        const int count = random_int(rng, 5);
        for (int i = 0; i < count; ++i) {
            patterns.push_back(random_string(rng, 1 + random_int(rng, 3), alphabet));
        }
        require(
            shortest_string_containing_all_patterns<2, 'a'>(patterns)
                == naive_shortest_all_patterns(patterns, alphabet),
            "random shortest all patterns"
        );
    }
}

}  // namespace aho_corasick_test

int main() {
    using namespace aho_corasick_test;

    deterministic_tests();

    std::mt19937_64 rng(0x5a17c0ULL);
    random_search_tests(rng);
    random_dp_tests(rng);
    random_weight_tests(rng);
    random_segmentation_and_wildcard_tests(rng);
    random_sparse_and_dynamic_tests(rng);
    random_prefix_query_tests(rng);
    random_digit_tests(rng);
    random_shortest_all_tests(rng);

    std::cout << "All Aho-Corasick tests passed\n";
    return 0;
}

#endif
