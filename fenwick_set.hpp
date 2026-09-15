/*
 * 同じヘッダ内に fenwick_set を利用する fenwick_map<K, V> も収録する。
 * fenwick_set: 座標圧縮済み index 用の競技プログラミング向け順序付き set 実装。
 * 値域 [0, value_range) 上の 0/1 集合を Fenwick Tree と存在 bitset で保持し、rank / kth / 前駆 / 後継を高速に処理する。
 * Fenwick Tree は 64 座標ごとの個数を管理する。word 内の順位は popcount とビット走査で求める。
 * 重複要素は保持しない。座標圧縮、および元の値との相互変換はこのライブラリの責任範囲外。
 * insert する値は 0 <= x < value_range の圧縮済み index を想定する。値域外の insert / erase は何もしない。
 * 内部の Fenwick Tree のカウンタは uint32_t で保持するため、value_range は UINT32_MAX 以下を前提とする。
 * rank / rank2 / count / less_equal / greater_equal は値域外の問い合わせにも自然に動作する。
 *
 * 競技プログラミング前提で、ライブラリ内部の確保・値操作は成功することを前提とする。
 * 失敗を個別に捕捉・復旧せず、noexcept 境界から例外が出れば terminate する。専用の assert メッセージは出さない。
 * NDEBUG によらず失敗後の内容保持・再利用・後始末は保証しない。引数の構築・破棄とユーザー callback は対象外。
 * for_each 系の callback が受け取る参照は呼び出し中のみ有効。走査中の構造変更は禁止する。
 * テンプレートパラメータ:
 *   T : 32bit 以下の整数型（既定値 uint32_t）。実際の値は圧縮済み index として扱われる。
 * 配列から値域を推定する場合、非負キーは UINT32_MAX 未満でなければならない。
 * 型・値域・kth の前提条件は assert / static_assert で確認する。実行時の範囲外問い合わせは従来の意味を保つ。
 * 同値域コピーなどの範囲が既知の処理は再検査を省き、キーの const 性と value の寿命は常に保つ。
 */
#pragma once

#include <bits/stdc++.h>

template <typename T = std::uint32_t>
struct fenwick_set {
    static_assert(std::is_integral_v<T> && sizeof(T) <= sizeof(std::uint32_t),
                  "fenwick_set<T>: T must be an integral type of at most 32 bits");

    using count_type = std::uint32_t;

    // 空の集合を構築する。O(1)
    fenwick_set() = default;

    // 値域 [0, value_range) の空集合を構築する。O(value_range / 64)
    explicit fenwick_set(std::size_t range) { assign_range(range); }

    // 配列から重複を除いた順序付き集合を構築する。O(value_range / 64 + n)
    explicit fenwick_set(std::vector<T> a) {
        std::size_t range = 0;
        for (T x : a) {
            if (is_negative(x)) continue;
            range = std::max(range, static_cast<std::size_t>(x) + 1);
        }
        assign_range(range);
        build_from_values(a);
    }

    // 値域 [0, value_range) と配列から重複を除いた順序付き集合を構築する。O(value_range / 64 + n)
    fenwick_set(std::size_t range, std::vector<T> a) {
        assign_range(range);
        build_from_values(a);
    }

    // 集合を複製する。O(value_range / 64)
    fenwick_set(const fenwick_set& rhs) noexcept = default;

    // 先行確保はせず、vector のコピー代入に容量再利用も任せる
    // 配列とスカラーをメンバ単位で複製し、失敗に備えた更新順序の制御は行わない
    // 集合を複製して代入する。確保失敗後の内容は保証しない。O(value_range / 64)
    fenwick_set& operator=(const fenwick_set& rhs) noexcept = default;

    // 集合の所有権を移動し、移動元を値域 0 の空集合にする。O(1)
    fenwick_set(fenwick_set&& rhs) noexcept { *this = std::move(rhs); }

    // 集合の所有権を移動して代入し、移動元を値域 0 の空集合にする。O(1)
    fenwick_set& operator=(fenwick_set&& rhs) noexcept {
        if (this == &rhs) return *this;
        // 配列の所有権とスカラー情報を同時に移し、移動元にも一貫した空状態を残す
        bit = std::move(rhs.bit);
        words = std::move(rhs.words);
        value_range = std::exchange(rhs.value_range, 0);
        n = std::exchange(rhs.n, 0);
        min_idx = std::exchange(rhs.min_idx, 0);
        max_idx = std::exchange(rhs.max_idx, 0);
        return *this;
    }

    // 全要素を削除して空にする。O(value_range / 64)
    void clear() {
        if (n == 0) return;
        std::fill(bit.begin(), bit.end(), count_type{0});
        std::fill(words.begin(), words.end(), std::uint64_t{0});
        n = 0;
        min_idx = max_idx = 0;
    }

    // 要素 x を挿入する。既に存在する場合は何もしない。O(log value_range)
    void insert(T x) {
        // 値域外と重複を除き、存在ビットと word 単位の個数を更新する
        if (!in_range(x)) return;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (test_bit(idx)) return;
        set_bit(idx);
        fenwick_add(idx, +1);
        // 最小・最大値の問い合わせはキャッシュだけで返せるようにする
        if (n == 0) {
            min_idx = max_idx = idx;
        } else {
            if (idx < min_idx) min_idx = idx;
            if (idx > max_idx) max_idx = idx;
        }
        ++n;
    }

    // 要素 x を削除する。存在しない場合は何もしない。O(log value_range)
    void erase(T x) {
        // 存在する値だけを削除し、累積個数を減らす
        if (!in_range(x)) return;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (!test_bit(idx)) return;
        reset_bit(idx);
        fenwick_add(idx, -1);
        --n;
        if (n == 0) {
            min_idx = max_idx = 0;
            return;
        }
        // 端を削除した場合、残りが同じ word にあれば木の再探索を避ける
        if (idx == min_idx) {
            const std::uint64_t word = words[idx >> 6];
            min_idx = word ? (idx & ~std::size_t{63}) + std::countr_zero(word) : kth_index(0);
        }
        if (idx == max_idx) {
            const std::uint64_t word = words[idx >> 6];
            max_idx = word ? (idx & ~std::size_t{63}) + 63 - std::countl_zero(word) : kth_index(n - 1);
        }
    }

    // 現在の要素数を返す。O(1)
    std::size_t size() const { return n; }

    // 集合が空なら true を返す。O(1)
    bool empty() const { return n == 0; }

    // 最小要素を返す。空なら std::nullopt。O(1)
    std::optional<T> min() const {
        if (n == 0) return std::nullopt;
        return static_cast<T>(min_idx);
    }

    // 最大要素を返す。空なら std::nullopt。O(1)
    std::optional<T> max() const {
        if (n == 0) return std::nullopt;
        return static_cast<T>(max_idx);
    }

    // 昇順に全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each(F&& callback) const {
        for_each_impl<false>(callback);
    }

    // 降順に全要素へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each_r(F&& callback) const {
        for_each_impl<true>(callback);
    }

    // 0-indexed で k 番目に小さい要素を返す。O(log value_range)
    T operator[](std::size_t k) const {
        return static_cast<T>(kth_index(k));
    }

    // x より小さい要素数を返す。O(log value_range)
    std::size_t rank(T x) const {
        if (n == 0 || is_negative(x)) return 0;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx >= value_range) return n;
        return prefix_sum(idx);
    }

    // x 以下の要素数を返す。O(log value_range)
    std::size_t rank2(T x) const {
        if (n == 0 || is_negative(x)) return 0;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx >= value_range - 1) return n;
        return prefix_sum(idx + 1);
    }

    // x が存在するなら true を返す。O(1)
    bool contains(T x) const {
        return in_range(x) && test_bit(static_cast<std::size_t>(x));
    }

    // x が存在するかを 0 または 1 で返す。O(1)
    std::size_t count(T x) const { return contains(x); }

    // x 以下の最大要素を返す。存在しない場合は std::nullopt。O(log value_range)
    std::optional<T> less_equal(T x) const {
        if (n == 0 || is_negative(x)) return std::nullopt;
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx >= value_range) return static_cast<T>(max_idx);

        // 同一 word 内と直前の word はビット走査だけで求める
        const std::size_t wi = idx >> 6;
        const std::uint64_t word = words[wi] & (~std::uint64_t{0} >> (63 - (idx & 63)));
        if (word != 0) return static_cast<T>((wi << 6) + 63 - std::countl_zero(word));
        if (wi != 0 && words[wi - 1] != 0)
            return static_cast<T>(((wi - 1) << 6) + 63 - std::countl_zero(words[wi - 1]));

        // 局所探索で見つからなければ端を確認し、累積個数から探す
        if (idx < min_idx) return std::nullopt;
        if (idx > max_idx) return static_cast<T>(max_idx);
        const std::size_t count = prefix_sum(idx + 1);
        return static_cast<T>(kth_index(count - 1));
    }

    // x 以上の最小要素を返す。存在しない場合は std::nullopt。O(log value_range)
    std::optional<T> greater_equal(T x) const {
        if (n == 0) return std::nullopt;
        if (is_negative(x)) return static_cast<T>(min_idx);
        const std::size_t idx = static_cast<std::size_t>(x);
        if (idx >= value_range) return std::nullopt;

        // 同一 word 内と直後の word はビット走査だけで求める
        const std::size_t wi = idx >> 6;
        const std::uint64_t word = words[wi] & (~std::uint64_t{0} << (idx & 63));
        if (word != 0) return static_cast<T>((wi << 6) + std::countr_zero(word));
        if (wi + 1 < words.size() && words[wi + 1] != 0)
            return static_cast<T>(((wi + 1) << 6) + std::countr_zero(words[wi + 1]));

        // 局所探索で見つからなければ端を確認し、累積個数から探す
        if (idx > max_idx) return std::nullopt;
        if (idx < min_idx) return static_cast<T>(min_idx);
        return static_cast<T>(kth_index(prefix_sum(idx)));
    }

private:
    // map のキー管理と一括構築で、存在ビット・端のキャッシュ・木を共用する
    template <class, class> friend struct fenwick_map;

    std::size_t value_range = 0;
    std::size_t n = 0;
    std::size_t min_idx = 0;
    std::size_t max_idx = 0;
    std::vector<count_type> bit; // word ごとの要素数を 1-indexed で管理
    std::vector<std::uint64_t> words;

    static bool is_negative(T x) {
        if constexpr (std::is_signed_v<T>) return x < 0;
        return false;
    }

    // 32bit 以下のキーを size_t へ変換し、負数と値域外を除外する
    // 負数は上限検査より先に除外し、未使用領域へのアクセスを防ぐ
    bool in_range(T x) const {
        return !is_negative(x) && static_cast<std::size_t>(x) < value_range;
    }

    // 確保・値操作の失敗は noexcept 境界に任せ、半端な状態を呼び出し側へ戻さない
    // NDEBUG 時も復旧や途中状態の破棄を保証する経路は持たない

    // 新規構築時専用。スカラーはメンバ初期値を使い、配列だけを確保する
    // 最大キー + 1 は size_t で求め、自動推定・明示指定の上限をここで一括確認する
    void assign_range(std::size_t range) noexcept {
        assert(range <= std::numeric_limits<count_type>::max() &&
               "fenwick_set/map: value_range overflow");
        value_range = range;
        if (range == 0) return;
        bit.assign(((value_range + 63) >> 6) + 1, count_type{0});
        words.assign((value_range + 63) >> 6, std::uint64_t{0});
    }

    void build_from_values(const std::vector<T>& a) {
        for (T x : a) {
            if (in_range(x)) set_bit(static_cast<std::size_t>(x));
        }
        build_fenwick_from_words();
    }

    bool test_bit(std::size_t idx) const { return ((words[idx >> 6] >> (idx & 63)) & 1ULL) != 0; }
    void set_bit(std::size_t idx) { words[idx >> 6] |= std::uint64_t{1} << (idx & 63); }
    void reset_bit(std::size_t idx) { words[idx >> 6] &= ~(std::uint64_t{1} << (idx & 63)); }

    void build_fenwick_from_words() {
        // 新規構築時専用。bit は assign_range によって全て 0 に初期化済み
        n = 0;
        for (std::size_t i = 1; i <= words.size(); ++i) {
            const auto count = static_cast<count_type>(std::popcount(words[i - 1]));
            n += count;
            bit[i] += count;
            const std::size_t next = i + (i & -i);
            if (next <= words.size()) bit[next] += bit[i];
        }
        // 値域末尾の未使用ビットは立てず、非空時だけ端をキャッシュする
        if (n != 0) {
            min_idx = kth_index(0);
            max_idx = kth_index(n - 1);
        }
    }

    void fenwick_add(std::size_t idx, int delta) {
        for (idx = (idx >> 6) + 1; idx < bit.size(); idx += idx & -idx) {
            if (delta > 0) ++bit[idx];
            else --bit[idx];
        }
    }

    std::size_t prefix_sum(std::size_t end) const {
        // 完全に含まれる word の累積個数と、端の word の部分個数を足す
        const std::size_t wi = end >> 6;
        const unsigned offset = end & 63;
        std::size_t sum = offset ? std::popcount(words[wi] & ((std::uint64_t{1} << offset) - 1)) : 0;
        for (std::size_t i = wi; i; i -= i & -i) sum += bit[i];
        return sum;
    }

    std::size_t kth_index(std::size_t k) const {
        assert(k < n);
        // Fenwick Tree 上で k 番目の要素を含む word まで降りる
        std::size_t idx = 0;
        for (std::size_t step = std::bit_floor(words.size()); step; step >>= 1) {
            const std::size_t next = idx + step;
            if (next < bit.size() && bit[next] <= k) {
                idx = next;
                k -= bit[next];
            }
        }
        // word 内だけを選択する。消すビットは最大 63 個
        std::uint64_t word = words[idx];
        while (k != 0) {
            word &= word - 1;
            --k;
        }
        return (idx << 6) + static_cast<std::size_t>(std::countr_zero(word));
    }

    template <class F>
    static bool invoke_callback(F& callback, const T& x) {
        if constexpr (std::is_void_v<std::invoke_result_t<F&, const T&>>) {
            std::invoke(callback, x);
            return false;
        } else {
            return static_cast<bool>(std::invoke(callback, x));
        }
    }

    template <bool Reverse, class F>
    void for_each_impl(F& callback) const {
        if (n == 0) return;
        const std::size_t first = min_idx >> 6;
        const std::size_t last = (max_idx >> 6) + 1;
        // 要素の存在する範囲だけを走査し、未使用の前後部分を飛ばす
        if constexpr (Reverse) {
            for (const auto* it = words.data() + last; it != words.data() + first;) {
                std::uint64_t w = *--it;
                const std::size_t wi = static_cast<std::size_t>(it - words.data());
                while (w) {
                    const int b = 63 - __builtin_clzll(w);
                    const std::size_t idx = (wi << 6) + static_cast<std::size_t>(b);
                    if (invoke_callback(callback, static_cast<T>(idx))) return;
                    w &= ~(std::uint64_t{1} << b);
                }
            }
        } else {
            // 値域末尾の未使用ビットは常に 0 なので、各要素の再検査は不要
            for (const auto* it = words.data() + first; it != words.data() + last; ++it) {
                std::uint64_t w = *it;
                const std::size_t wi = static_cast<std::size_t>(it - words.data());
                while (w) {
                    const int b = __builtin_ctzll(w);
                    const std::size_t idx = (wi << 6) + static_cast<std::size_t>(b);
                    if (invoke_callback(callback, static_cast<T>(idx))) return;
                    w &= w - 1;
                }
            }
        }
    }
};

/*
 * fenwick_map: fenwick_set の存在ビットを V の構築済みフラグとして共用する整数キー map。
 * 座標圧縮は呼び出し側の責任とし、key は [0, value_range) の index。値域は構築後に変更しない。
 * 未使用座標で V のコンストラクタは呼ばない。erase / clear は値を破棄し、clear は各配列の capacity を保持する。
 * 値域外の insert / insert_or_assign / erase は何もしない。operator[] は値域内のみ使用できる。
 * min / max は非空時のみ使用できる。key は整数値、value は参照として返すため、key の保存領域は持たない。
 * for_each 系の callback は void または bool 変換可能な値を返す。true で中断し、走査中の構造変更は禁止する。
 * callback の key 参照は呼び出し中のみ有効。値の参照は該当キーの削除・clear・代入・破棄まで有効。
 * n は要素数、value_range は確保する座標数。メモリ使用量は O(value_range * sizeof(V))。値領域は未初期化で確保する。
 * std::allocator で確保した未構築領域へ construct_at し、生存値だけを destroy_at する。
 * 挿入時は構築成功後に存在ビットを立てる。確保・構築・代入・破棄の失敗は noexcept 境界で terminate し、復旧はしない。
 * 自明なコピーでは格納バイトをまとめて複製できる。未使用スロットのバイトは V として読み取らない。
 * コピー代入は同じ値域の領域を再利用する。失敗後の内容保持・再利用・後始末は保証しない。
 * テンプレートパラメータ: K は bool を除く 32bit 以下の整数型、V は value 型（ビット幅制限なし）。値を返す API は V のコピーを必要とする。
 */
template <class K, class V>
struct fenwick_map {
    static_assert(std::is_integral_v<K> && !std::is_same_v<K, bool> && sizeof(K) <= sizeof(std::uint32_t),
                  "fenwick_map<K, V>: K must be a non-bool integral type of at most 32 bits");
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;

    // 値域 0 の空 map をヒープ確保なしで構築する。O(1)
    fenwick_map() = default;

    // 値域 [0, value_range) の空 map を構築する。O(value_range)
    explicit fenwick_map(std::size_t range)
        : keys(range), values(allocate_values()) {}

    // 配列から map を構築する。非負 key の最大値 + 1 を値域とし、同 key は最初の値を残す。O(value_range + n)
    explicit fenwick_map(std::vector<value_type> a) : fenwick_map(required_range(a)) {
        // 値の構築と同時に存在ビットを設定し、最後に個数の木を一括構築する
        build_values(a);
    }

    // 値域と配列から map を構築する。値域外を無視し、同 key は入力で最初の値を残す。O(value_range + n)
    fenwick_map(std::size_t range, std::vector<value_type> a)
        : fenwick_map(range) {
        build_values(a);
    }

    // map を複製する。O(value_range)
    fenwick_map(const fenwick_map& rhs) requires std::is_copy_constructible_v<V>
        : keys(rhs.keys), values(allocate_values()) {
        // 失敗時の追跡は不要。完成済みの集合を直接複製し、ゼロ初期化後の上書きを避ける
        copy_values_from(rhs);
    }

    // map を複製して代入する。O(移動元と移動先の value_range の合計)
    fenwick_map& operator=(const fenwick_map& rhs) noexcept requires std::is_copy_constructible_v<V> {
        if (this == &rhs) return *this;
        // 異なる値域だけ領域を交換し、同じ値域は構築・代入の例外仕様によらず再利用する
        if (keys.value_range != rhs.keys.value_range) return *this = fenwick_map(rhs);
        if constexpr (std::is_trivially_copyable_v<V> || !std::is_copy_assignable_v<V>) {
            destroy_values();
            copy_values_from(rhs);
        } else {
            // 共通キーの value は代入で再利用する。文字列の容量確認専用の事前走査は不要
            // コピー元にない値だけ破棄し、新しいキーにだけコピー構築する
            // 同値域かつ生存キーなので範囲は既知。存在ビットだけを直接調べる
            keys.for_each([&](K key) {
                const auto idx = static_cast<std::size_t>(key);
                if (!rhs.keys.test_bit(idx)) std::destroy_at(values + idx);
            });
            rhs.keys.for_each([&](K key) {
                const auto idx = static_cast<std::size_t>(key);
                if (keys.test_bit(idx)) values[idx] = std::as_const(rhs.values[idx]);
                else std::construct_at(values + idx, std::as_const(rhs.values[idx]));
            });
        }
        keys = rhs.keys;
        return *this;
    }

    // map の所有権を移動し、移動元を値域 0 の空 map にする。O(1)
    fenwick_map(fenwick_map&& rhs) noexcept
        : keys(std::move(rhs.keys)), values(std::exchange(rhs.values, nullptr)) {}

    // map の所有権を移動して代入し、移動元を値域 0 の空 map にする。O(移動先の value_range)
    fenwick_map& operator=(fenwick_map&& rhs) noexcept {
        if (this == &rhs) return *this;
        release_values();
        keys = std::move(rhs.keys);
        values = std::exchange(rhs.values, nullptr);
        return *this;
    }

    // 構築済みの値を破棄し、確保した領域を解放する。O(value_range / 64 + n)、V が自明に破棄可能なら O(1)
    ~fenwick_map() { release_values(); }

    // 全要素を削除して内部 capacity を保持する。O(value_range / 64 + n)、V が自明に破棄可能なら O(value_range / 64)
    void clear() {
        destroy_values();
        keys.clear();
    }

    // key が存在しなければ key-value を挿入する。既に存在する場合は何もしない。O(log value_range)、既存なら O(1)
    void insert(K key, V value) { insert_impl<false>(key, std::move(value)); }

    // key-value を挿入する。既に存在する場合は value を代入する。O(log value_range)、既存なら O(1)
    void insert_or_assign(K key, V value) { insert_impl<true>(key, std::move(value)); }

    // key を削除する。存在しない場合は何もしない。O(log value_range)
    void erase(const K& key) noexcept {
        if (!contains(key)) return;
        const K erased_key = key; // key が削除対象 value の一部を参照していても保持する
        std::destroy_at(values + static_cast<std::size_t>(erased_key));
        keys.erase(erased_key);
    }

    // 現在の要素数を返す。O(1)
    std::size_t size() const { return keys.size(); }

    // map が空なら true を返す。O(1)
    bool empty() const { return keys.empty(); }

    // 最小 key と value への参照を返す。空の場合の動作は未定義。O(1)
    std::pair<K, V&> min() {
        const K key = min_key();
        return {key, values[static_cast<std::size_t>(key)]};
    }

    // 最小 key と value への const 参照を返す。空の場合の動作は未定義。O(1)
    std::pair<K, const V&> min() const {
        const K key = min_key();
        return {key, values[static_cast<std::size_t>(key)]};
    }

    // 最大 key と value への参照を返す。空の場合の動作は未定義。O(1)
    std::pair<K, V&> max() {
        const K key = max_key();
        return {key, values[static_cast<std::size_t>(key)]};
    }

    // 最大 key と value への const 参照を返す。空の場合の動作は未定義。O(1)
    std::pair<K, const V&> max() const {
        const K key = max_key();
        return {key, values[static_cast<std::size_t>(key)]};
    }

    // key は既に構築済みの座標。optional の生成や空集合用の値生成は行わない
    // 最小 key を返す。空の場合の動作は未定義。O(1)
    K min_key() const { assert(!empty()); return static_cast<K>(keys.min_idx); }

    // 非空という呼び出し条件だけを debug 時に検査し、キャッシュを直接返す
    // 最大 key を返す。空の場合の動作は未定義。O(1)
    K max_key() const { assert(!empty()); return static_cast<K>(keys.max_idx); }

    // 昇順に全 key-value へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each(F&& callback) {
        keys.for_each([&](const K& key) -> decltype(auto) {
            return std::invoke(callback, key, values[static_cast<std::size_t>(key)]);
        });
    }

    // 昇順に全 key-value へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each(F&& callback) const {
        keys.for_each([&](const K& key) -> decltype(auto) {
            return std::invoke(callback, key, std::as_const(values[static_cast<std::size_t>(key)]));
        });
    }

    // 降順に全 key-value へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each_r(F&& callback) {
        keys.for_each_r([&](const K& key) -> decltype(auto) {
            return std::invoke(callback, key, values[static_cast<std::size_t>(key)]);
        });
    }

    // 降順に全 key-value へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each_r(F&& callback) const {
        keys.for_each_r([&](const K& key) -> decltype(auto) {
            return std::invoke(callback, key, std::as_const(values[static_cast<std::size_t>(key)]));
        });
    }

    // 昇順に全 key へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each_key(F&& callback) const { keys.for_each(callback); }

    // 降順に全 key へ callback を適用する。bool 変換可能な戻り値が true なら中断する。O(value_range / 64 + n)
    template <class F>
    void for_each_key_r(F&& callback) const { keys.for_each_r(callback); }

    // 0-indexed で k 番目に小さい key の key-value を返す。O(log value_range)
    value_type kth(std::size_t k) const noexcept {
        const K key = keys[k];
        return value_type(key, std::as_const(values[static_cast<std::size_t>(key)]));
    }

    // key が存在すれば value へのポインタを返す。存在しない場合は nullptr。O(1)
    V* find(const K& key) {
        return contains(key) ? values + static_cast<std::size_t>(key) : nullptr;
    }

    // key が存在すれば value へのポインタを返す。存在しない場合は nullptr。O(1)
    const V* find(const K& key) const {
        return contains(key) ? values + static_cast<std::size_t>(key) : nullptr;
    }

    // key の value への参照を返し、不在なら V{} を挿入する。値域内の key のみ有効。O(log value_range)、既存なら O(1)
    V& operator[](const K& key) noexcept {
        assert(keys.in_range(key));
        const std::size_t idx = static_cast<std::size_t>(key);
        if (!keys.test_bit(idx)) {
            std::construct_at(values + idx);
            // V の構築が参照引数 key の元の値を変えても、確定済みの座標を登録する
            keys.insert(static_cast<K>(idx));
        }
        return values[idx];
    }

    // key より小さい key の個数を返す。O(log value_range)
    std::size_t rank(const K& key) const { return keys.rank(key); }

    // key 以下の key の個数を返す。O(log value_range)
    std::size_t rank2(const K& key) const { return keys.rank2(key); }

    // key が存在するなら true を返す。O(1)
    bool contains(const K& key) const { return keys.contains(key); }

    // key が存在するかを 0 または 1 で返す。O(1)
    std::size_t count(const K& key) const { return contains(key); }

    // key 以下の最大 key の key-value を返す。存在しない場合は std::nullopt。O(log value_range)
    std::optional<value_type> less_equal(const K& key) const noexcept {
        const auto found = keys.less_equal(key);
        if (!found) return std::nullopt;
        return std::optional<value_type>(std::in_place, *found, std::as_const(values[static_cast<std::size_t>(*found)]));
    }

    // key 以上の最小 key の key-value を返す。存在しない場合は std::nullopt。O(log value_range)
    std::optional<value_type> greater_equal(const K& key) const noexcept {
        const auto found = keys.greater_equal(key);
        if (!found) return std::nullopt;
        return std::optional<value_type>(std::in_place, *found, std::as_const(values[static_cast<std::size_t>(*found)]));
    }

private:
    fenwick_set<K> keys;
    V* values = nullptr;

    // 値域構築とコピー構築で共用し、未使用 V は構築しない
    V* allocate_values() noexcept {
        return keys.value_range ? std::allocator<V>{}.allocate(keys.value_range) : nullptr;
    }

    static std::size_t required_range(const std::vector<value_type>& a) {
        // キーの一時配列を作らず、必要な値域だけを調べる
        std::size_t range = 0;
        for (const auto& entry : a) {
            if constexpr (std::is_signed_v<K>) if (entry.first < 0) continue;
            range = std::max(range, static_cast<std::size_t>(entry.first) + 1);
        }
        return range;
    }

    // 構築失敗はその場で終了するため、後始末用の生存ビット追跡は行わない
    // メタデータは呼び出し側で複製し、値コピーのための木の再構築は行わない
    void copy_values_from(const fenwick_map& rhs) noexcept {
        if constexpr (std::is_trivially_copyable_v<V> && std::is_trivially_copy_constructible_v<V>) {
            if (rhs.empty()) return;
            const auto first = rhs.keys.min_idx;
            const auto span = rhs.keys.max_idx - first + 1;
            // 空きスロットの余分な転送量は、生存値 1 個あたり索引 2 個分までに抑える
            // 自明な型の格納バイトだけを運び、空きスロットを V として評価しない
            if ((span - rhs.size()) * sizeof(V) <= rhs.size() * 2 * sizeof(std::size_t)) {
                std::memcpy(static_cast<void*>(values + first), rhs.values + first, span * sizeof(V));
                return;
            }
        }
        // raw pointer の指す値にも const を付け、非 const コピーの選択と元の値の変更を防ぐ
        rhs.keys.for_each([&](K key) {
            const auto idx = static_cast<std::size_t>(key);
            std::construct_at(values + idx, std::as_const(rhs.values[idx]));
        });
    }

    void destroy_values() noexcept {
        // 自明なデストラクタなら走査不要。未構築領域は読みも破棄もしない
        if constexpr (!std::is_trivially_destructible_v<V>) {
            keys.for_each([&](K key) { std::destroy_at(values + static_cast<std::size_t>(key)); });
        }
    }

    void release_values() {
        if (!values) return;
        destroy_values();
        std::allocator<V>{}.deallocate(values, keys.value_range);
    }

    void build_values(std::vector<value_type>& a) noexcept {
        // 同じキーは最初の値だけを構築する。値の構築が済んでからビットを立てる
        for (auto& [key, value] : a) {
            if (!keys.in_range(key)) continue;
            const std::size_t idx = static_cast<std::size_t>(key);
            if (!keys.test_bit(idx)) {
                std::construct_at(values + idx, std::move(value));
                keys.set_bit(idx);
            }
        }
        // キー用の一時配列や挿入ごとの累積更新を省く
        keys.build_fenwick_from_words();
    }

    template <bool AssignIfExists>
    void insert_impl(K key, V&& value) noexcept {
        // 値域外を無視し、既存キーでは値の代入だけを行う
        if (!keys.in_range(key)) return;
        const std::size_t idx = static_cast<std::size_t>(key);
        if (keys.test_bit(idx)) {
            if constexpr (AssignIfExists) values[idx] = std::move(value);
            return;
        }
        // 値を構築できてから集合へ反映し、未使用 V の構築を避ける
        std::construct_at(values + idx, std::move(value));
        keys.insert(key);
    }
};

#if __INCLUDE_LEVEL__ == 0
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fenwick_failure_self_test {
inline std::size_t failure_cases = 0;
inline std::size_t terminate_cases = 0;
inline std::size_t assertion_cases = 0;

// 失敗後のオブジェクトには触れず、子プロセスが検査境界で停止することを確認する
// 例外はテスト専用 terminate handler への到達を確認し、別のクラッシュを成功扱いにしない
// 前提条件の assert は別に SIGABRT とメッセージを確認する。NDEBUG で不正入力は実行しない
// テスト用の fork / pipe / waitpid は通常の include 時には一切含まれない
// 子プロセスの標準エラーは親が読み取り、想定内の assert 出力だけを収集する
// terminate の既定動作は終了。終了方法自体はテスト中だけ専用 exit code へ変更する
template <class F>
void expect_failure(F&& operation,
                    std::string_view message = {}) {
    std::cout.flush();
    std::cerr.flush();
    int channels[2];
    if (pipe(channels) != 0) std::abort();
    const pid_t child = fork();
    if (child < 0) std::abort();
    if (child == 0) {
        close(channels[0]);
        if (dup2(channels[1], STDERR_FILENO) < 0) _exit(120);
        close(channels[1]);
        const rlimit limit{0, 0};
        if (setrlimit(RLIMIT_CORE, &limit) != 0) _exit(121);
        // ライブラリから例外が漏れた場合や復帰した場合は試験失敗とする
        std::set_terminate([] { _exit(124); });
        try { operation(); } catch (...) { _exit(122); }
        _exit(123);
    }
    close(channels[1]);
    std::string error;
    char buffer[2048];
    for (;;) {
        const ssize_t length = read(channels[0], buffer, sizeof(buffer));
        if (length > 0) error.append(buffer, static_cast<std::size_t>(length));
        else if (length == 0) break;
        else if (errno != EINTR) std::abort();
    }
    close(channels[0]);
    int status = 0;
    while (waitpid(child, &status, 0) < 0) if (errno != EINTR) std::abort();
    if (message.empty()) {
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 124) {
            std::cerr << "failure did not reach terminate: " << status << '\n' << error;
            std::abort();
        }
        ++terminate_cases;
    } else {
        if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT || error.find(message) == std::string::npos) {
            std::cerr << "unexpected assertion: " << status << '\n' << error;
            std::abort();
        }
        ++assertion_cases;
    }
    ++failure_cases;
}
} // namespace fenwick_failure_self_test

namespace fenwick_set_self_test {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

template <class A, class B>
void require_eq(const A& actual, const B& expected, const std::string& message) {
    if (!(actual == expected)) {
        std::ostringstream oss;
        oss << message << " actual=" << actual << " expected=" << expected;
        fail(oss.str());
    }
}

std::string optional_to_string(const std::optional<int>& x) {
    if (!x) return "nullopt";
    return std::to_string(*x);
}

void require_eq_opt(const std::optional<int>& actual, const std::optional<int>& expected,
                    const std::string& message) {
    if (actual != expected) {
        fail(message + " actual=" + optional_to_string(actual) +
             " expected=" + optional_to_string(expected));
    }
}

template <class A, class B>
void require_vec_eq(const std::vector<A>& actual, const std::vector<B>& expected,
                    const std::string& message) {
    if (actual.size() != expected.size()) {
        std::ostringstream oss;
        oss << message << " size actual=" << actual.size() << " expected=" << expected.size();
        fail(oss.str());
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (!(actual[i] == expected[i])) {
            std::ostringstream oss;
            oss << message << " at " << i << " actual=" << actual[i]
                << " expected=" << expected[i];
            fail(oss.str());
        }
    }
}

std::optional<int> set_less_equal(const std::set<int>& ss, int x) {
    auto it = ss.upper_bound(x);
    if (it == ss.begin()) return std::nullopt;
    --it;
    return *it;
}

std::optional<int> set_greater_equal(const std::set<int>& ss, int x) {
    auto it = ss.lower_bound(x);
    if (it == ss.end()) return std::nullopt;
    return *it;
}

std::vector<int> candidate_values(const std::set<int>& ss, int value_range) {
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), -1000000007, -10, -1, 0, 1, 2, 3,
        value_range / 2 - 1, value_range / 2, value_range / 2 + 1,
        value_range - 2, value_range - 1, value_range, value_range + 1,
        1000000007, std::numeric_limits<int>::max()
    };
    for (int x : ss) {
        qs.push_back(x);
        if (x != std::numeric_limits<int>::min()) qs.push_back(x - 1);
        if (x != std::numeric_limits<int>::max()) qs.push_back(x + 1);
    }
    std::sort(qs.begin(), qs.end());
    qs.erase(std::unique(qs.begin(), qs.end()), qs.end());
    return qs;
}

template <class Set>
std::vector<int> collect_forward(const Set& xs) {
    std::vector<int> got;
    xs.for_each([&](const int& x) { got.push_back(x); });
    return got;
}

template <class Set>
std::vector<int> collect_reverse(const Set& xs) {
    std::vector<int> got;
    xs.for_each_r([&](const int& x) { got.push_back(x); });
    return got;
}

template <class Set>
void check_all_apis(const Set& xs, const std::set<int>& ss, int value_range,
                    const std::string& label) {
    const std::vector<int> expected(ss.begin(), ss.end());
    std::vector<int> expected_r = expected;
    std::reverse(expected_r.begin(), expected_r.end());

    require_eq(xs.size(), ss.size(), label + ": size");
    require_eq(xs.empty(), ss.empty(), label + ": empty");
    require_eq_opt(xs.min(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.begin()},
                   label + ": min");
    require_eq_opt(xs.max(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.rbegin()},
                   label + ": max");
    require_vec_eq(collect_forward(xs), expected, label + ": for_each order");
    require_vec_eq(collect_reverse(xs), expected_r, label + ": for_each_r order");

    for (std::size_t k = 0; k < expected.size(); ++k) {
        require_eq(xs[k], expected[k], label + ": operator[]");
    }

    for (int q : candidate_values(ss, value_range)) {
        const std::size_t rk = static_cast<std::size_t>(
            std::lower_bound(expected.begin(), expected.end(), q) - expected.begin());
        const std::size_t rk2 = static_cast<std::size_t>(
            std::upper_bound(expected.begin(), expected.end(), q) - expected.begin());
        require_eq(xs.rank(q), rk, label + ": rank");
        require_eq(xs.rank2(q), rk2, label + ": rank2");
        require_eq(xs.contains(q), ss.contains(q), label + ": contains");
        require_eq(xs.count(q), ss.count(q), label + ": count");
        require_eq_opt(xs.less_equal(q), set_less_equal(ss, q), label + ": less_equal");
        require_eq_opt(xs.greater_equal(q), set_greater_equal(ss, q), label + ": greater_equal");
    }
}

struct NotBoolConvertible {};

void test_empty_and_singleton() {
    constexpr int range = 32;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    check_all_apis(xs, ss, range, "empty with range");

    fenwick_set<int> zero;
    check_all_apis(zero, ss, 0, "empty default zero range");

    int calls = 0;
    xs.for_each([&](const int&) { ++calls; });
    xs.for_each_r([&](const int&) { ++calls; return true; });
    require_eq(calls, 0, "empty iteration callbacks are not called");

    xs.erase(123);
    xs.erase(-1);
    xs.clear();
    check_all_apis(xs, ss, range, "empty after erase/clear");

    xs.insert(10);
    ss.insert(10);
    check_all_apis(xs, ss, range, "singleton insert");

    xs.insert(10);
    ss.insert(10);
    xs.erase(11);
    ss.erase(11);
    check_all_apis(xs, ss, range, "singleton duplicate and missing erase");

    xs.erase(10);
    ss.erase(10);
    check_all_apis(xs, ss, range, "singleton erased");

    xs.insert(7);
    ss.insert(7);
    xs.clear();
    ss.clear();
    check_all_apis(xs, ss, range, "clear nonempty");

    xs.insert(7);
    ss.insert(7);
    check_all_apis(xs, ss, range, "reuse after clear");
}

void test_constructor_duplicates_and_boundaries() {
    std::vector<int> init = {5, 3, 5, 1, 0, 3, 99, 42, 42, 1};
    fenwick_set<int> xs(100, init);
    std::set<int> ss(init.begin(), init.end());
    check_all_apis(xs, ss, 100, "constructor duplicates and value boundaries");

    fenwick_set<int> auto_range(init);
    check_all_apis(auto_range, ss, 100, "auto range vector constructor");

    fenwick_set<int> empty_from_vector(std::vector<int>{});
    check_all_apis(empty_from_vector, std::set<int>{}, 0, "empty vector constructor");
}

void test_iteration_break_modes() {
    constexpr int range = 64;
    std::vector<int> init = {0, 2, 4, 8, 13, 20, 31, 63};
    fenwick_set<int> xs(range, init);
    std::set<int> ss(init.begin(), init.end());
    check_all_apis(xs, ss, range, "iteration source");

    std::vector<int> got;
    xs.for_each([&](const int& x) -> bool {
        got.push_back(x);
        return x >= 13;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8, 13}), "for_each bool break");

    got.clear();
    xs.for_each([&](const int& x) -> int {
        got.push_back(x);
        return x == 20;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8, 13, 20}), "for_each int break");

    got.clear();
    xs.for_each([&](const int& x) -> std::optional<int> {
        got.push_back(x);
        if (x == 8) return 1;
        return std::nullopt;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8}), "for_each optional break");

    got.clear();
    xs.for_each([&](const int& x) -> const int* {
        got.push_back(x);
        return x == 13 ? &got.back() : nullptr;
    });
    require_vec_eq(got, std::vector<int>({0, 2, 4, 8, 13}), "for_each pointer break");

    got.clear();
    xs.for_each_r([&](const int& x) -> bool {
        got.push_back(x);
        return x <= 8;
    });
    require_vec_eq(got, std::vector<int>({63, 31, 20, 13, 8}), "for_each_r bool break");

    got.clear();
    xs.for_each_r([&](const int& x) -> void { got.push_back(x); });
    std::vector<int> expected_r(init.rbegin(), init.rend());
    require_vec_eq(got, expected_r, "for_each_r void full scan");
}

void test_deterministic_updates() {
    constexpr int range = 128;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    const std::vector<int> values = {
        64, 63, 65, 0, 127, 1, 126, 2, 125, 3, 124, 4, 123,
        5, 122, 6, 121, 7, 120, 8, 119, 9, 118, 10, 117
    };
    for (int x : values) {
        xs.insert(x);
        ss.insert(x);
        check_all_apis(xs, ss, range, "deterministic insert");
    }
    for (int x : {0, 127, 64, 200, -1, 5, 122, 1, 126, 10, 117, 2, 125}) {
        xs.erase(x);
        ss.erase(x);
        check_all_apis(xs, ss, range, "deterministic erase/missing erase");
    }
    for (int x : values) {
        xs.insert(x);
        ss.insert(x);
    }
    check_all_apis(xs, ss, range, "deterministic refill");
}

void test_random_exhaustive() {
    constexpr int range = 181;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    std::mt19937 rng(123456789u);
    std::uniform_int_distribution<int> value_dist(0, range - 1);
    std::uniform_int_distribution<int> query_dist(-10, range + 10);
    std::uniform_int_distribution<int> op_dist(0, 11);

    for (int step = 0; step < 100000; ++step) {
        const int op = op_dist(rng);
        const int x = (op <= 5 ? value_dist(rng) : query_dist(rng));
        if (op <= 2) {
            xs.insert(x);
            ss.insert(x);
        } else if (op <= 5) {
            xs.erase(x);
            ss.erase(x);
        } else if (op == 6) {
            require_eq(xs.count(x), ss.count(x), "random count");
        } else if (op == 7) {
            const std::vector<int> v(ss.begin(), ss.end());
            const std::size_t expected = static_cast<std::size_t>(
                std::lower_bound(v.begin(), v.end(), x) - v.begin());
            require_eq(xs.rank(x), expected, "random rank");
        } else if (op == 8) {
            const std::vector<int> v(ss.begin(), ss.end());
            const std::size_t expected = static_cast<std::size_t>(
                std::upper_bound(v.begin(), v.end(), x) - v.begin());
            require_eq(xs.rank2(x), expected, "random rank2");
        } else if (op == 9) {
            require_eq_opt(xs.less_equal(x), set_less_equal(ss, x), "random less_equal");
        } else if (op == 10) {
            require_eq_opt(xs.greater_equal(x), set_greater_equal(ss, x), "random greater_equal");
        } else {
            require_eq_opt(xs.min(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.begin()},
                           "random min");
            require_eq_opt(xs.max(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.rbegin()},
                           "random max");
            if (!ss.empty()) {
                std::uniform_int_distribution<std::size_t> kth_dist(0, ss.size() - 1);
                const std::size_t k = kth_dist(rng);
                const std::vector<int> v(ss.begin(), ss.end());
                require_eq(xs[k], v[k], "random kth");
            }
        }
        if ((step % 257) == 0) check_all_apis(xs, ss, range, "random periodic full check");
    }
    check_all_apis(xs, ss, range, "random final full check");
}

void check_large_sample(const fenwick_set<int>& xs, const std::set<int>& ss, int value_range,
                        std::mt19937& rng, const std::string& label) {
    const std::vector<int> v(ss.begin(), ss.end());
    require_eq(xs.size(), ss.size(), label + ": size");
    require_eq_opt(xs.min(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.begin()},
                   label + ": min");
    require_eq_opt(xs.max(), ss.empty() ? std::optional<int>{} : std::optional<int>{*ss.rbegin()},
                   label + ": max");
    require_vec_eq(collect_forward(xs), v, label + ": for_each full order");
    std::vector<int> rv = v;
    std::reverse(rv.begin(), rv.end());
    require_vec_eq(collect_reverse(xs), rv, label + ": for_each_r full order");

    if (!v.empty()) {
        std::uniform_int_distribution<std::size_t> kth_dist(0, v.size() - 1);
        for (int i = 0; i < 128; ++i) {
            const std::size_t k = kth_dist(rng);
            require_eq(xs[k], v[k], label + ": sampled kth");
        }
    }

    std::uniform_int_distribution<int> query_dist(-1000, value_range + 1000);
    std::vector<int> qs = {
        std::numeric_limits<int>::min(), -1, 0, 1, value_range / 2,
        value_range - 1, value_range, value_range + 1, std::numeric_limits<int>::max()
    };
    for (int i = 0; i < 256; ++i) qs.push_back(query_dist(rng));
    if (!v.empty()) {
        std::uniform_int_distribution<std::size_t> idx_dist(0, v.size() - 1);
        for (int i = 0; i < 128; ++i) {
            const int y = v[idx_dist(rng)];
            qs.push_back(y);
            if (y != std::numeric_limits<int>::min()) qs.push_back(y - 1);
            if (y != std::numeric_limits<int>::max()) qs.push_back(y + 1);
        }
    }
    for (int q : qs) {
        const std::size_t rk = static_cast<std::size_t>(
            std::lower_bound(v.begin(), v.end(), q) - v.begin());
        const std::size_t rk2 = static_cast<std::size_t>(
            std::upper_bound(v.begin(), v.end(), q) - v.begin());
        require_eq(xs.rank(q), rk, label + ": sampled rank");
        require_eq(xs.rank2(q), rk2, label + ": sampled rank2");
        require_eq(xs.count(q), ss.count(q), label + ": sampled count");
        require_eq_opt(xs.less_equal(q), set_less_equal(ss, q), label + ": sampled less_equal");
        require_eq_opt(xs.greater_equal(q), set_greater_equal(ss, q), label + ": sampled greater_equal");
    }
}

void test_large_random_against_std_set() {
    constexpr int range = 400001;
    fenwick_set<int> xs(range);
    std::set<int> ss;
    std::mt19937 rng(987654321u);
    std::uniform_int_distribution<int> value_dist(0, range - 1);
    std::uniform_int_distribution<int> query_dist(-1000, range + 1000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    constexpr int operations = 300000;
    for (int step = 1; step <= operations; ++step) {
        const int op = op_dist(rng);
        const int x = (op <= 6 ? value_dist(rng) : query_dist(rng));
        if (op <= 3) {
            xs.insert(x);
            ss.insert(x);
        } else if (op <= 6) {
            xs.erase(x);
            ss.erase(x);
        } else if (op == 7) {
            require_eq(xs.count(x), ss.count(x), "large random count immediate");
        } else if (op == 8) {
            require_eq_opt(xs.less_equal(x), set_less_equal(ss, x), "large random less_equal immediate");
        } else {
            require_eq_opt(xs.greater_equal(x), set_greater_equal(ss, x), "large random greater_equal immediate");
        }

        if ((step % 5000) == 0) {
            check_large_sample(xs, ss, range, rng, "large random sample step " + std::to_string(step));
        }
    }
    check_large_sample(xs, ss, range, rng, "large random final");
}

void run_all_tests() {
    test_empty_and_singleton();
    test_constructor_duplicates_and_boundaries();
    test_iteration_break_modes();
    test_deterministic_updates();
    test_random_exhaustive();
    test_large_random_against_std_set();
}

} // namespace fenwick_set_self_test

namespace fenwick_set_self_benchmark {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

inline volatile u64 benchmark_sink = 0;

struct SplitMix64 {
    u64 x;
    explicit SplitMix64(u64 seed) : x(seed) {}

    u64 next() {
        u64 z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    u32 next_u32(u32 mod) { return static_cast<u32>(next() % mod); }
};

struct Timer {
    std::chrono::steady_clock::time_point t0;
    Timer() : t0(std::chrono::steady_clock::now()) {}
    double ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
};

struct BenchRow {
    std::string api;
    std::size_t n = 0;
    std::size_t q = 0;
    std::size_t fenwick_ops = 0;
    std::size_t std_ops = 0;
    double fenwick_ms = 0.0;
    double std_ms = -1.0;
    u64 fenwick_checksum = 0;
    u64 std_checksum = 0;
    std::string status;
};

std::vector<u32> sorted_unique_values(std::size_t n, u32 mul = 2, u32 add = 0) {
    std::vector<u32> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<u32>(i * mul + add);
    return v;
}

std::vector<u32> shuffled_unique_values(std::size_t n, u32 mul, u64 seed, u32 add = 0) {
    auto v = sorted_unique_values(n, mul, add);
    SplitMix64 rng(seed);
    for (std::size_t i = n; i > 1; --i) {
        std::swap(v[i - 1], v[static_cast<std::size_t>(rng.next() % i)]);
    }
    return v;
}

std::vector<u32> query_values(std::size_t q, u32 mod, u64 seed) {
    SplitMix64 rng(seed);
    std::vector<u32> qs(q);
    for (std::size_t i = 0; i < q; ++i) qs[i] = rng.next_u32(mod);
    return qs;
}

std::vector<std::size_t> query_indices(std::size_t q, std::size_t n, u64 seed) {
    SplitMix64 rng(seed);
    std::vector<std::size_t> qs(q);
    for (std::size_t i = 0; i < q; ++i) qs[i] = static_cast<std::size_t>(rng.next() % n);
    return qs;
}

BenchRow bench_default_construct(std::size_t q) {
    u64 cb = 0, cs = 0;
    double mb = 0.0, ms = 0.0;
    {
        Timer t;
        std::vector<fenwick_set<u32>> v(q);
        cb += v.size();
        mb = t.ms();
    }
    {
        Timer t;
        std::vector<std::set<u32>> v(q);
        cs += v.size();
        ms = t.ms();
    }
    return {"default_construct", 0, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_construct_range(std::size_t n) {
    u64 cb = 0;
    Timer tb;
    fenwick_set<u32> bs(n * 2 + 1);
    const double mb = tb.ms();
    cb = bs.size();
    return {"construct(range)", n, 1, 1, 0, mb, -1.0, cb, 0, cb == 0 ? "fenwick_only" : "wrong"};
}

BenchRow bench_construct_vector(std::size_t n, u64 seed) {
    const auto vals = shuffled_unique_values(n, 2, seed);
    u64 cb = 0, cs = 0;
    Timer tb;
    fenwick_set<u32> bs(vals);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    std::set<u32> ss(vals.begin(), vals.end());
    const double ms = ts.ms();
    cs = ss.size();
    return {"construct(vector)", n, 1, 1, 1, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_clear(std::size_t n, std::size_t q, u64 seed) {
    const auto vals = shuffled_unique_values(n, 2, seed);
    std::vector<fenwick_set<u32>> bv;
    std::vector<std::set<u32>> sv;
    bv.reserve(q);
    sv.reserve(q);
    for (std::size_t i = 0; i < q; ++i) {
        bv.emplace_back(n * 2 + 1, vals);
        sv.emplace_back(vals.begin(), vals.end());
    }
    u64 cb = 0, cs = 0;
    Timer tb;
    for (auto& x : bv) {
        x.clear();
        cb += x.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (auto& x : sv) {
        x.clear();
        cs += x.size();
    }
    const double ms = ts.ms();
    return {"clear", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_unique_empty(std::size_t q, u64 seed) {
    const auto vals = shuffled_unique_values(q, 2, seed);
    fenwick_set<u32> bs(q * 2 + 1);
    std::set<u32> ss;
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.insert(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.insert(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"insert(unique,empty)", q, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_missing(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto vals = shuffled_unique_values(q, 2, seed, 1);
    const std::size_t range = std::max(n, q) * 2 + 2;
    fenwick_set<u32> bs(range, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.insert(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.insert(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"insert(missing)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_insert_duplicate(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 i : qs) {
        bs.insert(init[i]);
        cb += bs.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (u32 i : qs) {
        ss.insert(init[i]);
        cs += ss.size();
    }
    const double ms = ts.ms();
    return {"insert(duplicate)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_erase_present(std::size_t n, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    auto vals = init;
    SplitMix64 rng(seed);
    for (std::size_t i = vals.size(); i > 1; --i) {
        std::swap(vals[i - 1], vals[static_cast<std::size_t>(rng.next() % i)]);
    }
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.erase(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.erase(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"erase(present)", n, n, n, n, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_erase_missing(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto vals = shuffled_unique_values(q, 2, seed, 1);
    const std::size_t range = std::max(n, q) * 2 + 2;
    fenwick_set<u32> bs(range, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : vals) bs.erase(x);
    const double mb = tb.ms();
    cb = bs.size();
    Timer ts;
    for (u32 x : vals) ss.erase(x);
    const double ms = ts.ms();
    cs = ss.size();
    return {"erase(missing)", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_size(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += bs.size();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += ss.size();
    }
    const double ms = ts.ms();
    return {"size", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_empty(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> fs(2 * n + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cf = 0, cs = 0;
    Timer tf;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cf += fs.empty();
    }
    const double mf = tf.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += ss.empty();
    }
    const double ms = ts.ms();
    return {"empty", n, q, q, q, mf, ms, cf, cs, cf == cs ? "ok" : "wrong"};
}

BenchRow bench_min(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += *bs.min();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += *ss.begin();
    }
    const double ms = ts.ms();
    return {"min", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_max(std::size_t n, std::size_t q, u64 seed) {
    const auto init = shuffled_unique_values(n, 2, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cb += *bs.max();
    }
    const double mb = tb.ms();
    Timer ts;
    for (std::size_t i = 0; i < q; ++i) {
        asm volatile("" ::: "memory");
        cs += *ss.rbegin();
    }
    const double ms = ts.ms();
    return {"max", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_for_each(std::size_t n, u64 seed, bool rev, bool brk) {
    (void)seed;
    const auto init = sorted_unique_values(n, 2, 0);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    const std::size_t limit = brk ? n / 2 : n;
    u64 cb = 0, cs = 0;
    std::size_t cbn = 0, csn = 0;
    Timer tb;
    if (!rev) {
        bs.for_each([&](const u32& x) -> bool {
            cb += x;
            return ++cbn >= limit;
        });
    } else {
        bs.for_each_r([&](const u32& x) -> bool {
            cb += x;
            return ++cbn >= limit;
        });
    }
    const double mb = tb.ms();
    Timer ts;
    if (!rev) {
        for (u32 x : ss) {
            cs += x;
            if (++csn >= limit) break;
        }
    } else {
        for (auto it = ss.rbegin(); it != ss.rend(); ++it) {
            cs += *it;
            if (++csn >= limit) break;
        }
    }
    const double ms = ts.ms();
    std::string name = rev ? "for_each_r" : "for_each";
    if (brk) name += "(break)";
    return {name, n, limit, cbn, csn, mb, ms, cb, cs, cb == cs && cbn == csn ? "ok" : "wrong"};
}

BenchRow bench_kth(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_indices(q, n, seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    u64 cb = 0;
    Timer tb;
    for (std::size_t k : qs) cb += bs[k];
    const double mb = tb.ms();
    return {"operator[]", n, q, q, 0, mb, -1.0, cb, 0, "fenwick_only"};
}

BenchRow bench_rank_like(std::string name, std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    u64 cb = 0;
    Timer tb;
    if (name == "rank") {
        for (u32 x : qs) cb += bs.rank(x);
    } else {
        for (u32 x : qs) cb += bs.rank2(x);
    }
    const double mb = tb.ms();
    return {name, n, q, q, 0, mb, -1.0, cb, 0, "fenwick_only"};
}

BenchRow bench_count(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.count(x);
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) cs += ss.count(x);
    const double ms = ts.ms();
    return {"count", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_contains(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(4 * n), seed);
    fenwick_set<u32> fs(4 * n + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cf = 0, cs = 0;
    Timer tf;
    for (u32 x : qs) cf += fs.contains(x);
    const double mf = tf.ms();
    Timer ts;
    for (u32 x : qs) cs += ss.contains(x);
    const double ms = ts.ms();
    return {"contains", n, q, q, q, mf, ms, cf, cs, cf == cs ? "ok" : "wrong"};
}

BenchRow bench_less_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.less_equal(x).value_or(0);
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) {
        auto it = ss.upper_bound(x);
        if (it != ss.begin()) {
            --it;
            cs += *it;
        }
    }
    const double ms = ts.ms();
    return {"less_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

BenchRow bench_greater_equal(std::size_t n, std::size_t q, u64 seed) {
    const auto init = sorted_unique_values(n, 2, 0);
    const auto qs = query_values(q, static_cast<u32>(n * 4 + 1), seed);
    fenwick_set<u32> bs(n * 2 + 1, init);
    std::set<u32> ss(init.begin(), init.end());
    u64 cb = 0, cs = 0;
    Timer tb;
    for (u32 x : qs) cb += bs.greater_equal(x).value_or(0);
    const double mb = tb.ms();
    Timer ts;
    for (u32 x : qs) {
        auto it = ss.lower_bound(x);
        if (it != ss.end()) cs += *it;
    }
    const double ms = ts.ms();
    return {"greater_equal", n, q, q, q, mb, ms, cb, cs, cb == cs ? "ok" : "wrong"};
}

template <class F>
BenchRow median_of_three(F&& f) {
    std::vector<BenchRow> rows;
    rows.reserve(3);
    for (int i = 0; i < 3; ++i) rows.push_back(f());
    std::sort(rows.begin(), rows.end(), [](const BenchRow& a, const BenchRow& b) {
        return a.fenwick_ms < b.fenwick_ms;
    });
    // 標準側も独立した中央値を使い、どの試行の不一致も見落とさない
    std::array<double, 3> std_times{};
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].status == "wrong" || rows[i].fenwick_checksum != rows[0].fenwick_checksum ||
            rows[i].std_checksum != rows[0].std_checksum) std::abort();
        std_times[i] = rows[i].std_ms;
    }
    std::sort(std_times.begin(), std_times.end());
    rows[1].std_ms = std_times[1];
    benchmark_sink = benchmark_sink + rows[1].fenwick_checksum + rows[1].std_checksum;
    return rows[1];
}

std::vector<BenchRow> run_benchmarks() {
    std::vector<BenchRow> rows;
    rows.push_back(median_of_three([] { return bench_default_construct(100000); }));
    rows.push_back(median_of_three([] { return bench_construct_range(200000); }));
    rows.push_back(median_of_three([] { return bench_construct_vector(200000, 1); }));
    rows.push_back(median_of_three([] { return bench_clear(100000, 5, 2); }));
    rows.push_back(median_of_three([] { return bench_insert_unique_empty(200000, 3); }));
    rows.push_back(median_of_three([] { return bench_insert_missing(200000, 500000, 4); }));
    rows.push_back(median_of_three([] { return bench_insert_duplicate(200000, 500000, 5); }));
    rows.push_back(median_of_three([] { return bench_erase_present(200000, 6); }));
    rows.push_back(median_of_three([] { return bench_erase_missing(200000, 500000, 7); }));
    rows.push_back(median_of_three([] { return bench_size(200000, 5000000, 8); }));
    rows.push_back(median_of_three([] { return bench_empty(200000, 5000000, 81); }));
    rows.push_back(median_of_three([] { return bench_min(200000, 5000000, 9); }));
    rows.push_back(median_of_three([] { return bench_max(200000, 5000000, 10); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 11, false, false); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 12, true, false); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 13, false, true); }));
    rows.push_back(median_of_three([] { return bench_for_each(200000, 14, true, true); }));
    rows.push_back(median_of_three([] { return bench_kth(50000, 100000, 15); }));
    rows.push_back(median_of_three([] { return bench_rank_like("rank", 200000, 500000, 16); }));
    rows.push_back(median_of_three([] { return bench_rank_like("rank2", 200000, 500000, 17); }));
    rows.push_back(median_of_three([] { return bench_contains(200000, 500000, 18); }));
    rows.push_back(median_of_three([] { return bench_count(200000, 500000, 18); }));
    rows.push_back(median_of_three([] { return bench_less_equal(200000, 500000, 19); }));
    rows.push_back(median_of_three([] { return bench_greater_equal(200000, 500000, 20); }));
    return rows;
}

void print_benchmark_table(const std::vector<BenchRow>& rows) {
    std::cout << "api,n,q,fenwick_ms,std_set_ms,fenwick_ns_per_op,std_set_ns_per_op,std_over_fenwick,status\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& r : rows) {
        const double f_ns = r.fenwick_ops ? r.fenwick_ms * 1e6 / static_cast<double>(r.fenwick_ops) : 0.0;
        const double s_ns = (r.std_ms >= 0.0 && r.std_ops) ? r.std_ms * 1e6 / static_cast<double>(r.std_ops) : -1.0;
        const double ratio = (r.std_ms >= 0.0 && r.fenwick_ms > 0.0) ? r.std_ms / r.fenwick_ms : -1.0;
        std::cout << r.api << ',' << r.n << ',' << r.q << ',' << r.fenwick_ms << ',';
        if (r.std_ms >= 0.0) std::cout << r.std_ms; else std::cout << "NA";
        std::cout << ',' << f_ns << ',';
        if (s_ns >= 0.0) std::cout << s_ns; else std::cout << "NA";
        std::cout << ',';
        if (ratio >= 0.0) std::cout << ratio; else std::cout << "NA";
        std::cout << ',' << r.status << '\n';
    }
}

} // namespace fenwick_set_self_benchmark

namespace fenwick_map_key_test {

void require(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "key iteration FAILED: " << label << '\n';
        std::abort();
    }
}

struct ExplicitBool {
    bool value;
    explicit operator bool() const { return value; }
};

template <class Map>
void check(Map& map, bool ordered) {
    using K = typename Map::key_type;
    const Map& cm = map;
    std::vector<K> expected, actual, backward;
    cm.for_each([&](const K& key, const auto&) { expected.push_back(key); });
    map.for_each_key([&](const auto& key) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(key)>>);
        actual.push_back(key);
    });
    require(actual == expected, "mutable object key order");
    actual.clear();
    cm.for_each_key([&](const K& key) { actual.push_back(key); });
    require(actual == expected, "const object key order");
    if (ordered) require(std::is_sorted(actual.begin(), actual.end()), "ascending keys");
    std::reverse(expected.begin(), expected.end());
    cm.for_each_key_r([&](const K& key) { backward.push_back(key); });
    require(backward == expected, "exact reverse order");
    std::size_t mutable_reverse_count = 0;
    map.for_each_key_r([&](const K&) { ++mutable_reverse_count; });
    require(mutable_reverse_count == actual.size(), "mutable reverse count");

    // 中断位置・戻り値型・方向の組合せを確認する
    const std::size_t n = actual.size();
    for (std::size_t stop : {std::size_t{1}, std::size_t{2}, n / 2 + 1, n, n + 1}) {
        for (bool reverse : {false, true}) {
            const auto& sequence = reverse ? backward : actual;
            auto run = [&](auto make_result) {
                std::size_t visits = 0;
                auto callback = [&](const K& key) {
                    require(key == sequence[visits], "break sequence");
                    ++visits;
                    return make_result(visits == stop);
                };
                if (reverse) cm.for_each_key_r(callback);
                else cm.for_each_key(callback);
                const std::size_t expected_visits = stop == 0 ? n : std::min(n, stop);
                require(visits == expected_visits, "break count");
            };
            run([](bool done) { return done; });
            run([](bool done) { return done ? 7 : 0; });
            run([](bool done) { return done ? std::optional<int>(0) : std::nullopt; });
            run([](bool done) { return ExplicitBool{done}; });
            int marker = 0;
            run([&](bool done) { return done ? &marker : nullptr; });
        }
    }

    // コピー不能 callback と、参照を返す callback もそのまま受け付ける
    std::size_t visits = 0;
    cm.for_each_key([state = std::make_unique<int>(0), &visits](const K&) mutable {
        ++*state;
        ++visits;
    });
    require(visits == n, "move-only callback");
    bool stop_ref = false;
    visits = 0;
    cm.for_each_key_r([&](const K&) -> bool& {
        stop_ref = ++visits == 1;
        return stop_ref;
    });
    require(visits == std::min(n, std::size_t{1}), "bool reference callback");
}

void run() {
    fenwick_map<int,int> map (50000);
    check(map, true);
    map.insert(0, 10);
    check(map, true);
    map.clear();
    check(map, true);

    // ブロック境界・空き slot・削除済み slot・再確保後の走査を検証する
    for (int i = 0; i < 30000; ++i) map.insert(i, i * 3);
    check(map, true);
    for (int i = 0; i < 30000; i += 3) map.erase(i);
    check(map, true);
    for (int i = 30000; i < 40000; ++i) map.insert(i, -i);
    check(map, true);
    map.for_each_key([&](int key) { *map.find(key) += 1; });
    check(map, true);
    map.clear();
    check(map, true);

    // 大量の更新中にも key-only と通常走査の一致を確認する
    std::mt19937 rng(20260914);
    for (int i = 0; i < 100000; ++i) {
        const int key = static_cast<int>(rng() % 4096);
        if ((rng() & 1U) != 0) map.insert_or_assign(key, i);
        else map.erase(key);
        if (i % 2048 == 0) check(map, true);
    }
    check(map, true);
    std::cout << "key_iteration_edges_and_random_100000_ops: ok\n";
}

} // namespace fenwick_map_key_test
namespace fenwick_map_self_test {

void require(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "fenwick_map FAILED: " << label << '\n';
        std::abort();
    }
}

template <class K>
void verify(fenwick_map<K, int>& map, const std::map<K, int>& ref, const std::vector<K>& queries) {
    const auto& cm = map;
    require(cm.size() == ref.size() && cm.empty() == ref.empty(), "size/empty");
    std::vector<std::pair<K, int>> actual, expected(ref.begin(), ref.end());
    cm.for_each([&](const K& key, const int& value) { actual.emplace_back(key, value); });
    require(actual == expected, "for_each values");
    actual.clear();
    cm.for_each_r([&](const K& key, const int& value) { actual.emplace_back(key, value); });
    std::reverse(expected.begin(), expected.end());
    require(actual == expected, "for_each_r values");
    if (!ref.empty()) {
        require(cm.min().first == ref.begin()->first && cm.min().second == ref.begin()->second, "min");
        require(cm.max().first == ref.rbegin()->first && cm.max().second == ref.rbegin()->second, "max");
        require(cm.min_key() == ref.begin()->first && map.min_key() == ref.begin()->first, "min_key");
        require(cm.max_key() == ref.rbegin()->first && map.max_key() == ref.rbegin()->first, "max_key");
        require(std::addressof(map.min().second) == map.find(ref.begin()->first), "min reference");
        require(std::addressof(cm.max().second) == cm.find(ref.rbegin()->first), "max const reference");
    }
    std::size_t k = 0;
    for (const auto& [key, value] : ref) {
        require(cm.kth(k++) == std::pair<K, int>(key, value), "kth");
        require(cm.find(key) && *cm.find(key) == value, "const find");
        require(map.find(key) && *map.find(key) == value, "mutable find");
    }
    for (K key : queries) {
        const auto low = ref.lower_bound(key), high = ref.upper_bound(key);
        require(cm.rank(key) == static_cast<std::size_t>(std::distance(ref.begin(), low)), "rank");
        require(cm.rank2(key) == static_cast<std::size_t>(std::distance(ref.begin(), high)), "rank2");
        require(cm.count(key) == ref.count(key) && cm.contains(key) == ref.contains(key), "count/contains");
        require((cm.find(key) != nullptr) == ref.contains(key), "find missing");
        std::optional<std::pair<K, int>> le, ge;
        if (high != ref.begin()) le = *std::prev(high);
        if (low != ref.end()) ge = *low;
        require(cm.less_equal(key) == le, "less_equal");
        require(cm.greater_equal(key) == ge, "greater_equal");
    }
}

void edge_tests() {
    using Map = fenwick_map<int, int>;
    static_assert(std::is_same_v<decltype(std::declval<Map&>().min()), std::pair<int, int&>>);
    static_assert(std::is_same_v<decltype(std::declval<const Map&>().max()), std::pair<int, const int&>>);
    Map empty;
    empty.insert(0, 1);
    empty.insert(-1, 2);
    empty.insert_or_assign(1, 3);
    empty.erase(0);
    empty.clear();
    require(empty.empty(), "default range zero");
    const std::vector<int> query{-100, -1, 0, 1, 2, 63, 64, 65, 127, 128, 129, 255, 256,
                                 std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
    for (std::size_t range : {0U, 1U, 2U, 63U, 64U, 65U, 127U, 128U, 129U, 256U}) {
        Map map(range);
        std::map<int, int> ref;
        verify(map, ref, query);
        for (int key : query) {
            map.insert(key, 3);
            if (key >= 0 && static_cast<std::size_t>(key) < range) ref.emplace(key, 3);
            map.insert(key, 9);
            verify(map, ref, query);
            map.insert_or_assign(key, 7);
            if (key >= 0 && static_cast<std::size_t>(key) < range) ref.insert_or_assign(key, 7);
            verify(map, ref, query);
        }
        if (!map.empty()) {
            map.min().second += 10;
            ref.begin()->second += 10;
            map.max().second -= 5;
            ref.rbegin()->second -= 5;
            verify(map, ref, query);
        }
        while (!ref.empty()) {
            const int key = ref.begin()->first;
            map.erase(key);
            ref.erase(key);
            verify(map, ref, query);
        }
        for (std::size_t i = 0; i < range; ++i) {
            map[static_cast<int>(i)] += 4;
            ref[static_cast<int>(i)] += 4;
        }
        verify(map, ref, query);
        map.clear();
        ref.clear();
        verify(map, ref, query);
        if (range != 0) { map[0] = 8; ref[0] = 8; }
        verify(map, ref, query);
    }

    const std::vector<std::pair<int, int>> input{{-1, 2}, {64, 640}, {1, 10}, {64, 641}, {0, 7}, {129, 9}};
    Map inferred(input);
    require(inferred.size() == 4 && *inferred.find(64) == 640, "inferred constructor first value");
    inferred.insert(128, 8);
    inferred.insert(130, 9);
    require(inferred.contains(128) && !inferred.contains(130), "inferred range");
    Map bounded(65, input);
    require(bounded.size() == 3 && bounded.max().first == 64, "bounded constructor");
    Map negatives(std::vector<std::pair<int,int>>{{-5,1},{-1,2}});
    require(negatives.empty(), "negative-only constructor");
    fenwick_map<std::uint32_t, int> u(65);
    u.insert(64, 4);
    u.insert(UINT32_MAX, 9);
    verify(u, std::map<std::uint32_t,int>{{64,4}}, std::vector<std::uint32_t>{0,64,65,UINT32_MAX});
    fenwick_map<std::int8_t, int> narrow(128);
    narrow.insert(127, 1);
    narrow.insert(-128, 3);
    verify(narrow, std::map<std::int8_t,int>{{127,1}}, std::vector<std::int8_t>{-128,0,126,127});
    fenwick_map<std::uint8_t, int> byte(256);
    byte.insert(255, 2);
    require(byte.max().first == 255 && byte.kth(0).first == 255, "u8 highest key");

    // コピー・move・自己代入と移動元再利用の整合性を確認する
    Map copied(bounded);
    copied[1] = 99;
    require(*bounded.find(1) == 10, "copy independence");
    Map moved(std::move(copied));
    require(copied.empty() && *moved.find(1) == 99, "move constructor");
    copied.insert(0, 1);
    require(copied.empty(), "moved-from zero range");
    copied = bounded;
    copied = copied;
    [](Map& dst, Map& src) { dst = std::move(src); }(copied, copied);
    require(copied.size() == bounded.size(), "self assignments");
    moved = std::move(copied);
    require(copied.empty() && moved.size() == bounded.size(), "move assignment");
    std::cout << "fenwick_map_edges_constructors_bounds_copy_move: ok\n";
}

struct Tracked {
    inline static int alive = 0;
    int value;
    Tracked() = delete;
    explicit Tracked(int v) : value(v) { ++alive; }
    Tracked(const Tracked& rhs) : value(rhs.value) { ++alive; }
    Tracked(Tracked&& rhs) noexcept : value(rhs.value) { ++alive; }
    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&&) = default;
    ~Tracked() { --alive; }
};

void lifetime_and_callback_tests() {
    {
        fenwick_map<int, Tracked> map(100000);
        require(Tracked::alive == 0, "unused values not constructed");
        map.insert(1, Tracked(7));
        map.insert(1, Tracked(8));
        require(Tracked::alive == 1 && map.find(1)->value == 7, "duplicate value lifetime");
        map.insert_or_assign(1, Tracked(9));
        require(Tracked::alive == 1 && map.find(1)->value == 9, "assigned value lifetime");
        map.erase(1);
        require(Tracked::alive == 0, "erase destroys value");
        map.insert(2, Tracked(4));
        map.clear();
        require(Tracked::alive == 0, "clear destroys value");
    }
    require(Tracked::alive == 0, "all lifetimes ended");
    fenwick_map<int, std::unique_ptr<int>> ptr(130);
    ptr.insert(64, std::make_unique<int>(7));
    ptr[129] = std::make_unique<int>(8);
    require(**ptr.find(64) == 7 && *ptr.max().second == 8, "move-only values");
    require(ptr.min_key() == 64 && std::as_const(ptr).max_key() == 129, "move-only value minmax_key");
    auto moved = std::move(ptr);
    require(ptr.empty() && **moved.find(64) == 7, "move-only map move");
    fenwick_map<int, bool> bits(100);
    bits[1] = true;
    require(*bits.find(1), "bool values return real references");
    struct alignas(64) Aligned { int x = 0; };
    fenwick_map<int, Aligned> aligned(5);
    aligned[1].x = 3;
    require(reinterpret_cast<std::uintptr_t>(aligned.find(1)) % 64 == 0, "value alignment");
    fenwick_map<int, std::string> text(20, {{2,"two"},{1,"one"},{2,"ignored"}});
    auto text_copy = text;
    text_copy[1] = "changed";
    require(*text.find(1) == "one", "string copy");

    // const / non-const と昇順 / 降順それぞれで値と戻り値を確認する
    fenwick_map<int,int> map(130, {{0,1},{63,2},{64,3},{129,4}});
    map.for_each([](const int&, int& v) { v += 10; });
    map.for_each_r([](const int&, int& v) { v -= 1; });
    require(*map.find(0) == 10 && *map.find(129) == 13, "mutable values");
    const auto& cm = map;
    for (std::size_t stop = 1; stop <= 5; ++stop) {
        auto check = [&](auto& xs, bool reverse, auto make_result) {
            std::size_t calls = 0;
            auto callback = [&](const int&, auto&) { return make_result(++calls == stop); };
            if (reverse) xs.for_each_r(callback); else xs.for_each(callback);
            require(calls == std::min(stop, xs.size()), "key-value callback break");
        };
        for (bool reverse : {false, true}) {
            check(map, reverse, [](bool b) { return b; });
            check(cm, reverse, [](bool b) { return b ? std::optional<int>(0) : std::nullopt; });
            check(map, reverse, [](bool b) { return b ? 2 : 0; });
            check(cm, reverse, [](bool b) { return fenwick_map_key_test::ExplicitBool{b}; });
        }
    }
    std::cout << "fenwick_map_lifetimes_move_only_callbacks: ok\n";
}

// キーの戻り値型とワード境界・最大座標・clear 後の再利用を確認する
void minmax_key_tests() {
    using Map = fenwick_map<int, int>;
    static_assert(std::is_same_v<decltype(std::declval<Map&>().min_key()), int>);
    static_assert(std::is_same_v<decltype(std::declval<const Map&>().min_key()), int>);
    static_assert(std::is_same_v<decltype(std::declval<Map&>().max_key()), int>);
    static_assert(std::is_same_v<decltype(std::declval<const Map&>().max_key()), int>);

    // 空の map には呼び出さず、各値域の両端から順に削除する
    for (int range : {1, 2, 63, 64, 65, 127, 128, 129, 256, 257, 4097}) {
        Map map(static_cast<std::size_t>(range));
        std::map<int, int> ref;
        for (int key = 0; key < range; ++key) {
            map.insert(key, key);
            ref.emplace(key, key);
        }
        while (!ref.empty()) {
            const Map& cm = map;
            require(cm.min_key() == ref.begin()->first, "min_key boundary");
            require(cm.max_key() == ref.rbegin()->first, "max_key boundary");
            const int key = (ref.size() & 1) ? ref.begin()->first : ref.rbegin()->first;
            map.erase(key);
            ref.erase(key);
        }
        map.insert(range - 1, 5);
        require(map.min_key() == range - 1 && map.max_key() == range - 1, "singleton maximum coordinate");
        map.clear();
        map.insert(0, 7);
        map.insert(-1, 8);
        map.insert(range, 9);
        require(map.min_key() == 0 && map.max_key() == 0, "minmax_key reuse and ignored keys");
    }

    // 狭い整数型・符号なし整数型、およびコピーと移動後の値を確認する
    fenwick_map<std::uint8_t, int> bytes(256, {{0, 1}, {255, 2}});
    static_assert(std::is_same_v<decltype(bytes.min_key()), std::uint8_t>);
    require(bytes.min_key() == 0 && bytes.max_key() == 255, "u8 minmax_key");
    fenwick_map<std::int8_t, int> small(128, {{0, 1}, {127, 2}});
    require(small.min_key() == 0 && small.max_key() == 127, "i8 minmax_key");
    fenwick_map<std::uint32_t, int> wide(130, {{0, 1}, {129, 2}});
    static_assert(std::is_same_v<decltype(wide.max_key()), std::uint32_t>);
    auto copy = wide;
    copy.erase(0);
    auto moved = std::move(copy);
    require(wide.min_key() == 0 && wide.max_key() == 129, "copy source minmax_key");
    require(moved.min_key() == 129 && moved.max_key() == 129 && copy.empty(), "move minmax_key");
    std::cout << "fenwick_map_minmax_key_edges_types_boundaries: ok\n";
}

void random_test(int range, int operations, unsigned seed) {
    fenwick_map<int,int> map(static_cast<std::size_t>(range));
    std::map<int,int> ref;
    std::mt19937 rng(seed);
    for (int step = 0; step < operations; ++step) {
        const int key = static_cast<int>(rng() % static_cast<unsigned>(range + 6)) - 3;
        const int value = static_cast<int>(rng() % 100000);
        switch (rng() % 9) {
        case 0:
        case 1:
            map.insert(key, value);
            if (0 <= key && key < range) ref.emplace(key, value);
            break;
        case 2:
            map.insert_or_assign(key, value);
            if (0 <= key && key < range) ref.insert_or_assign(key, value);
            break;
        case 3:
        case 4:
            map.erase(key); ref.erase(key); break;
        case 5:
            if (0 <= key && key < range) { map[key] += 1; ref[key] += 1; }
            break;
        case 6:
            if (auto* p = map.find(key)) { *p = value; ref[key] = value; }
            break;
        default:
            require(map.contains(key) == ref.contains(key), "random contains");
            break;
        }
        require(map.size() == ref.size() && map.count(key) == ref.count(key), "random update");
        if (!ref.empty()) {
            require(map.min_key() == ref.begin()->first, "random min_key");
            require(map.max_key() == ref.rbegin()->first, "random max_key");
        }
        if (step % 257 == 0) {
            verify(map, ref, std::vector<int>{-1, 0, key, 63, 64, range-1, range, INT_MAX});
        }
        if (step % 20003 == 0) { map.clear(); ref.clear(); }
    }
    verify(map, ref, std::vector<int>{-1, 0, 1, range-1, range, INT_MAX});
}

void run() {
    edge_tests();
    lifetime_and_callback_tests();
    minmax_key_tests();
    random_test(65, 100000, 27);
    std::cout << "fenwick_map_random_small_100000_ops: ok\n";
    random_test(4096, 500000, 92);
    std::cout << "fenwick_map_random_500000_ops: ok\n";
}

} // namespace fenwick_map_self_test

namespace fenwick_map_key_benchmark {

using Map = fenwick_map<std::uint32_t,std::uint64_t>;
using StdMap = std::map<std::uint32_t,std::uint64_t>;
using Clock = std::chrono::steady_clock;
inline volatile std::uint64_t sink = 0;

// 走査の外でのみバリアを置き、callback の最適化を妨げずに走査全体の省略を防ぐ
void escape(const void* p) { asm volatile("" : : "g"(p) : "memory"); }

template <class F>
double elapsed(F&& f) {
    const auto start = Clock::now();
    const std::uint64_t checksum = f();
    const auto end = Clock::now();
    sink = sink ^ checksum;
    return std::chrono::duration<double, std::nano>(end - start).count();
}

template <bool Reverse>
void bench(std::size_t n, bool break_half) {
    std::vector<std::pair<std::uint32_t, std::uint64_t>> input;
    for (std::size_t i = 0; i < n; ++i) input.emplace_back(i * 2 + 1, i + 17);
    std::mt19937_64 rng(54321);
    std::shuffle(input.begin(), input.end(), rng);
    Map map (2*n+1,input);
    StdMap ref;
    for (const auto& p : input) ref.emplace(p);
    const std::size_t visits = break_half ? std::max(std::size_t{1}, n / 2) : n;
    const std::size_t passes = std::max(std::size_t{1}, 1000000 / visits);
    std::array<std::vector<double>, 3> samples;

    // キーの大小順と訪問数から期待値を作り、各 variant の結果を計測外で検証する
    std::uint64_t expected_checksum = 0;
    auto expected_it = Reverse ? std::prev(ref.end()) : ref.begin();
    for (std::size_t i = 0; i < visits; ++i) {
        expected_checksum += expected_it->first;
        if (i + 1 != visits) {
            if constexpr (Reverse) --expected_it;
            else ++expected_it;
        }
    }
    expected_checksum *= passes;
    auto measure = [&](int variant) {
        std::uint64_t checksum = 0;
        const double ns = elapsed([&] {
            std::uint64_t sum = 0;
            for (std::size_t pass = 0; pass < passes; ++pass) {
                escape(&map);
                escape(&ref);
                auto walk = [&](auto&& callback) {
                    if (variant == 0) {
                        if constexpr (Reverse) map.for_each_key_r(callback);
                        else map.for_each_key(callback);
                    } else if (variant == 1) {
                        auto ignore_value = [&](const auto& key, const auto&) -> decltype(auto) {
                            return callback(key);
                        };
                        if constexpr (Reverse) map.for_each_r(ignore_value); else map.for_each(ignore_value);
                    } else {
                        if constexpr (Reverse) {
                            for (auto it = ref.rbegin(); it != ref.rend(); ++it)
                                if (callback(it->first)) break;
                        } else {
                            for (const auto& [key, value] : ref) {
                                (void)value;
                                if (callback(key)) break;
                            }
                        }
                    }
                };
                if (break_half) {
                    std::size_t count = 0;
                    walk([&](const std::uint32_t& key) { sum += key; return ++count == visits; });
                    if (count != visits) std::abort();
                } else {
                    walk([&](const std::uint32_t& key) { sum += key; return false; });
                }
            }
            checksum = sum;
            return sum;
        });
        if (checksum != expected_checksum) std::abort();
        return ns;
    };

    // 測定順を回転させ、特定の方式が常に先に実行される偏りを避ける
    for (int round = 0; round < 5; ++round) {
        for (int offset = 0; offset < 3; ++offset) {
            const int variant = (round + offset) % 3;
            samples[static_cast<std::size_t>(variant)].push_back(measure(variant));
        }
    }
    std::cout << (Reverse ? "for_each_key_r" : "for_each_key") << (break_half ? "(break_half)" : "")
              << ',' << n << ',' << visits << ',' << passes;
    for (auto& times : samples) {
        std::cout << ',';
        if (times.empty()) { std::cout << "NA"; continue; }
        std::sort(times.begin(), times.end());
        std::cout << times[times.size()/2] / static_cast<double>(passes * visits);
    }
    std::cout << ",ok\n";
}

void run() {
    std::cout << "key_benchmarks_begin,fenwick_map\n";
    std::cout << "api,n,visited_per_pass,passes,new_ns_per_key,existing_foreach_ns_per_key,std_ns_per_key,status\n";
    std::cout << std::fixed << std::setprecision(3);
    for (std::size_t n : {100U, 100000U}) {
        bench<false>(n, false);
        bench<true>(n, false);
        bench<false>(n, true);
        bench<true>(n, true);
    }
}

} // namespace fenwick_map_key_benchmark
namespace fenwick_map_self_benchmark {

using Key = std::uint32_t;
using Value = std::uint64_t;
using Pair = std::pair<Key, Value>;
using Map = fenwick_map<Key, Value>;
using Ref = std::map<Key, Value>;
using Clock = std::chrono::steady_clock;
inline volatile std::uint64_t sink = 0;

void escape(const void* p) { asm volatile("" : : "g"(p) : "memory"); }

template <class F>
double time_ns(F&& f) {
    const auto start = Clock::now();
    f();
    const auto end = Clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count();
}

struct Result {
    std::string api;
    std::size_t n, operations;
    double map_ns, std_ns;
};

template <class F>
Result median(const std::string& api, std::size_t n, std::size_t operations, F&& trial) {
    std::vector<double> ours, standard;
    for (int rep = 0; rep < 5; ++rep) {
        auto [a,b] = trial((rep & 1) != 0);
        ours.push_back(a);
        standard.push_back(b);
    }
    std::sort(ours.begin(), ours.end());
    std::sort(standard.begin(), standard.end());
    return {api, n, operations, ours[2], standard[2]};
}

template <class A, class B>
std::pair<double,double> pair_time(bool reverse, A&& ours, B&& standard) {
    double a, b;
    if (reverse) { b = standard(); a = ours(); }
    else { a = ours(); b = standard(); }
    return {a,b};
}

void run() {
    constexpr std::size_t n = 100000, range = 200001, q = 300000;
    std::vector<Pair> input;
    for (std::size_t i = 0; i < n; ++i) input.emplace_back(i * 2 + 1, i * 2 + 8);
    std::mt19937_64 rng(38127);
    std::shuffle(input.begin(), input.end(), rng);
    std::vector<Key> mixed(q), present(q), missing(n);
    for (std::size_t i = 0; i < q; ++i) {
        mixed[i] = static_cast<Key>(rng() % (2 * n));
        present[i] = static_cast<Key>(2 * (rng() % n) + 1);
    }
    std::iota(missing.begin(), missing.end(), Key{0});
    for (auto& key : missing) key *= 2;
    std::shuffle(missing.begin(), missing.end(), rng);
    std::vector<Result> rows;

    // 構築は破棄を計測から外し、構築・clear は 1 container あたりで表示する
    rows.push_back(median("default_construct", 0, n, [&](bool reverse) {
        std::optional<std::vector<Map>> a;
        std::optional<std::vector<Ref>> b;
        return pair_time(reverse,
            [&] { return time_ns([&] { a.emplace(n); escape(a->data()); }); },
            [&] { return time_ns([&] { b.emplace(n); escape(b->data()); }); });
    }));
    rows.push_back(median("construct(range)", 0, 1, [&](bool) {
        std::optional<Map> a;
        const double t = time_ns([&] { a.emplace(range); escape(&*a); });
        return std::pair{t, -1.0};
    }));
    for (bool inferred : {false, true}) {
        rows.push_back(median(inferred ? "construct(vector)" : "construct(range,vector)", n, 1, [&](bool reverse) {
            std::optional<Map> a;
            std::optional<Ref> b;
            auto times = pair_time(reverse,
                [&] { return time_ns([&] {
                    if (inferred) a.emplace(input); else a.emplace(range, input);
                    escape(&*a);
                }); },
                [&] { return time_ns([&] { b.emplace(input.begin(),input.end()); escape(&*b); }); });
            if (a->size() != b->size()) std::abort();
            return times;
        }));
    }
    rows.push_back(median("copy_construct", n, 1, [&](bool reverse) {
        Map source(range,input); Ref reference(input.begin(),input.end());
        std::optional<Map> a; std::optional<Ref> b;
        return pair_time(reverse,
            [&] { return time_ns([&] { a.emplace(source); escape(&*a); }); },
            [&] { return time_ns([&] { b.emplace(reference); escape(&*b); }); });
    }));
    rows.push_back(median("clear", n, 1, [&](bool reverse) {
        Map a(range,input); Ref b(input.begin(),input.end());
        return pair_time(reverse,
            [&] { return time_ns([&] { a.clear(); escape(&a); }); },
            [&] { return time_ns([&] { b.clear(); escape(&b); }); });
    }));

    // 挿入・削除系は準備と検証を計測外で行う
    for (const std::string name : {"insert(unique,empty)", "insert(missing)", "insert(duplicate)",
                                  "insert_or_assign(present)", "erase(present)", "erase(missing)",
                                  "operator[](existing)", "operator[](missing)"}) {
        const bool initially_empty = name == "insert(unique,empty)" || name == "operator[](missing)";
        const bool repeated = name == "insert(duplicate)" || name == "insert_or_assign(present)" ||
                              name == "erase(missing)" || name == "operator[](existing)";
        const std::size_t operations = repeated ? q : n;
        rows.push_back(median(name, initially_empty ? 0 : n, operations, [&](bool reverse) {
            Map a = initially_empty ? Map(range) : Map(range,input);
            Ref b;
            if (!initially_empty) b.insert(input.begin(), input.end());
            auto update = [&](auto& map) {
                std::uint64_t sum = 0;
                auto insert_value = [&](Key key, Value value) {
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(map)>, Map>) map.insert(key,value);
                    else map.emplace(key,value);
                };
                if (name == "insert(unique,empty)") {
                    for (const auto& [key,value] : input) insert_value(key,value);
                } else if (name == "insert(duplicate)") {
                    for (Key key : present) insert_value(key,key+7);
                } else if (name == "insert(missing)") {
                    for (std::size_t i=0;i<n;++i) insert_value(missing[i],i);
                } else if (name == "insert_or_assign(present)") {
                    for (std::size_t i=0;i<q;++i) map.insert_or_assign(present[i],i);
                } else if (name == "erase(present)") {
                    for (const auto& entry:input) map.erase(entry.first);
                } else if (name == "erase(missing)") {
                    for (Key key:present) map.erase(key-1);
                } else if (name == "operator[](existing)") {
                    for (Key key:present) sum += ++map[key];
                } else {
                    for (Key key:missing) sum += ++map[key];
                }
                escape(&map);
                sink = sink ^ sum;
            };
            auto times = pair_time(reverse,
                [&] { return time_ns([&] { update(a); }); },
                [&] { return time_ns([&] { update(b); }); });
            if (a.size() != b.size()) std::abort();
            a.for_each([&](Key key, Value value) {
                auto it = b.find(key);
                if (it == b.end() || it->second != value) std::abort();
            });
            return times;
        }));
    }

    // クエリは同一の hit/miss 列を使い、sum を比較して計算全体の消去を防ぐ
    Map a(range,input);
    Ref b(input.begin(),input.end());
    auto add_query = [&](const std::string& name, auto ours, auto standard) {
        rows.push_back(median(name, n, q, [&](bool reverse) {
            std::uint64_t sa=0,sb=0;
            auto times = pair_time(reverse,
                [&] { return time_ns([&] { for (Key key : mixed) sa += ours(key); escape(&sa); }); },
                [&] { return time_ns([&] { for (Key key : mixed) sb += standard(key); escape(&sb); }); });
            if (sa != sb) std::abort();
            sink = sink ^ sa;
            return times;
        }));
    };
    add_query("find", [&](Key key) { const auto* p=a.find(key); return p ? *p : 0; },
                        [&](Key key) { const auto it=b.find(key); return it==b.end()?0:it->second; });
    add_query("contains", [&](Key key) { return a.contains(key); }, [&](Key key) { return b.contains(key); });
    add_query("count", [&](Key key) { return a.count(key); }, [&](Key key) { return b.count(key); });
    add_query("less_equal", [&](Key key) { auto p=a.less_equal(key); return p?p->second:0; },
                              [&](Key key) { auto it=b.upper_bound(key); return it==b.begin()?0:std::prev(it)->second; });
    add_query("greater_equal", [&](Key key) { auto p=a.greater_equal(key); return p?p->second:0; },
                                 [&](Key key) { auto it=b.lower_bound(key); return it==b.end()?0:it->second; });
    for (const std::string name : {"rank", "rank2", "kth"}) {
        rows.push_back(median(name, n, q, [&](bool) {
            std::uint64_t sum = 0;
            const double t = time_ns([&] {
                if (name == "rank") {
                    for (Key key : mixed) sum += a.rank(key);
                } else if (name == "rank2") {
                    for (Key key : mixed) sum += a.rank2(key);
                } else {
                    for (Key key : mixed) sum += a.kth(static_cast<std::size_t>(key % n)).first;
                }
                escape(&sum);
            });
            sink = sink ^ sum;
            return std::pair{t,-1.0};
        }));
    }
    auto add_fast_query = [&](const std::string& name, auto ours, auto standard) {
        constexpr std::size_t calls = 1000000;
        rows.push_back(median(name, n, calls, [&](bool reverse) {
            std::uint64_t sa=0,sb=0;
            auto times = pair_time(reverse,
                [&] { return time_ns([&] {
                    for (std::size_t i=0;i<calls;++i) { escape(&a); sa+=ours(); }
                }); },
                [&] { return time_ns([&] {
                    for (std::size_t i=0;i<calls;++i) { escape(&b); sb+=standard(); }
                }); });
            if(sa!=sb) std::abort();
            sink = sink ^ sa;
            return times;
        }));
    };
    add_fast_query("size", [&] {return a.size();}, [&] {return b.size();});
    add_fast_query("empty", [&] {return a.empty();}, [&] {return b.empty();});
    add_fast_query("min", [&] {return a.min().first;}, [&] {return b.begin()->first;});
    add_fast_query("max", [&] {return a.max().first;}, [&] {return b.rbegin()->first;});
    add_fast_query("min_key", [&] {return a.min_key();}, [&] {return b.begin()->first;});
    add_fast_query("max_key", [&] {return a.max_key();}, [&] {return b.rbegin()->first;});
    for (bool reverse : {false,true}) {
        for (bool break_half : {false,true}) {
            const std::size_t visits = break_half ? n/2 : n;
            rows.push_back(median(std::string(reverse ? "for_each_r" : "for_each") +
                                  (break_half ? "(break_half)" : ""), n, visits*20, [&](bool std_first) {
                std::uint64_t sa=0,sb=0;
                auto times = pair_time(std_first,
                    [&] { return time_ns([&] {
                        for(int pass=0;pass<20;++pass) {
                            escape(&a);
                            std::size_t seen=0;
                            auto callback=[&](Key key, Value value) { sa+=key+value; return ++seen==visits; };
                            if(reverse) a.for_each_r(callback); else a.for_each(callback);
                        }
                    }); },
                    [&] { return time_ns([&] {
                        for(int pass=0;pass<20;++pass) {
                            escape(&b);
                            std::size_t seen=0;
                            if(reverse) {
                                for(auto it=b.rbegin();it!=b.rend();++it) {sb+=it->first+it->second;if(++seen==visits)break;}
                            } else {
                                for(auto& [key,value]:b) {sb+=key+value;if(++seen==visits)break;}
                            }
                        }
                    }); });
                if(sa!=sb) std::abort();
                sink = sink ^ sa;
                return times;
            }));
        }
    }
    std::cout << "fenwick_map_benchmarks_begin\n";
    std::cout << "api,n,operations,fenwick_ns_per_op,std_map_ns_per_op,std_over_fenwick\n";
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& row:rows) {
        std::cout<<row.api<<','<<row.n<<','<<row.operations<<','
                 <<row.map_ns/static_cast<double>(row.operations)<<',';
        if(row.std_ns < 0) std::cout<<"NA,NA\n";
        else std::cout<<row.std_ns/static_cast<double>(row.operations)<<','<<row.std_ns/row.map_ns<<'\n';
    }
}

} // namespace fenwick_map_self_benchmark


namespace fenwick_word_self_test {

void check(bool condition) {
    if (!condition) {
        std::cerr << "word-level Fenwick regression test failed\n";
        std::abort();
    }
}

// 構築・全順位・前駆後継・正逆走査をソート済みの基準列と比較する
void check_layout(int range, const std::vector<int>& input) {
    fenwick_set<int> set(static_cast<std::size_t>(range), input);
    std::vector<std::pair<int, std::uint64_t>> pairs;
    std::map<int, std::uint64_t> reference_map;
    std::vector<int> expected;
    for (int key : input) {
        const auto value = static_cast<std::uint64_t>(pairs.size()) + 100;
        pairs.emplace_back(key, value);
        if (0 <= key && key < range) {
            expected.push_back(key);
            reference_map.emplace(key, value);
        }
    }
    std::sort(expected.begin(), expected.end());
    expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
    fenwick_map<int, std::uint64_t> map(static_cast<std::size_t>(range), pairs);
    check(set.size() == expected.size() && map.size() == expected.size());
    check(set.empty() == expected.empty() && map.empty() == expected.empty());

    std::vector<int> actual;
    set.for_each([&](int key) { actual.push_back(key); });
    check(actual == expected);
    actual.clear();
    map.for_each_key([&](int key) { actual.push_back(key); });
    check(actual == expected);
    actual.clear();
    set.for_each_r([&](int key) { actual.push_back(key); });
    check(std::equal(actual.begin(), actual.end(), expected.rbegin(), expected.rend()));
    actual.clear();
    map.for_each_key_r([&](int key) { actual.push_back(key); });
    check(std::equal(actual.begin(), actual.end(), expected.rbegin(), expected.rend()));

    for (std::size_t k = 0; k < expected.size(); ++k) {
        check(set[k] == expected[k]);
        const auto entry = map.kth(k);
        check(entry.first == expected[k] && entry.second == reference_map.at(expected[k]));
    }
    if (!expected.empty()) {
        check(*set.min() == expected.front() && *set.max() == expected.back());
        check(map.min_key() == expected.front() && map.max_key() == expected.back());
    } else {
        check(!set.min() && !set.max());
    }

    // 各 word の両端に加え、小さい値域では全座標を調べる
    for (int key = -1; key <= range; ++key) {
        if (range > 1024 && key > 0 && key < range && (key & 63) > 2 && (key & 63) < 61) continue;
        const auto lower = std::lower_bound(expected.begin(), expected.end(), key);
        const auto upper = std::upper_bound(expected.begin(), expected.end(), key);
        const auto before = static_cast<std::size_t>(lower - expected.begin());
        const auto through = static_cast<std::size_t>(upper - expected.begin());
        const int previous = upper == expected.begin() ? -1 : *std::prev(upper);
        const int next = lower == expected.end() ? -1 : *lower;
        check(set.rank(key) == before && set.rank2(key) == through);
        check(map.rank(key) == before && map.rank2(key) == through);
        check(set.less_equal(key).value_or(-1) == previous && set.greater_equal(key).value_or(-1) == next);
        check(set.count(key) == through - before && map.count(key) == through - before);
        const auto pm = map.less_equal(key), nm = map.greater_equal(key);
        check(pm.has_value() == (previous >= 0) && nm.has_value() == (next >= 0));
        if (pm) check(pm->first == previous && pm->second == reference_map.at(previous));
        if (nm) check(nm->first == next && nm->second == reference_map.at(next));
    }
}

void exhaustive_words() {
    // 12bit の全パターンを word 内・境界をまたぐ位置・隣の word で確認する
    for (int shift : {0, 26, 58, 64}) {
        for (unsigned mask = 0; mask < (1U << 12); ++mask) {
            std::vector<int> input;
            for (int bit_index = 0; bit_index < 12; ++bit_index) {
                if ((mask >> bit_index) & 1U) input.push_back(shift + bit_index);
            }
            check_layout(shift + 12, input);
        }
    }
    std::cout << "word_exhaustive_16384_layouts: ok\n";
}

void boundaries_and_drain() {
    // 64の倍数およびFenwickの段数が変わる値域の前後を重点的に調べる
    for (int range : {0, 1, 2, 63, 64, 65, 127, 128, 129, 255, 256, 257,
                      4095, 4096, 4097, 65535, 65536, 65537}) {
        std::vector<int> dense, sparse;
        for (int key = 0; key < range; ++key) {
            dense.push_back(key);
            if ((key & 63) == 0 || key + 1 == range) sparse.push_back(key);
        }
        check_layout(range, dense);
        check_layout(range, sparse);
        check_layout(range, {-2, -1, 0, 0, range - 1, range - 1, range, range + 1});
        for (bool reverse : {false, true}) {
            fenwick_set<int> set(static_cast<std::size_t>(range), dense);
            for (int i = 0; i < range; ++i) {
                const int key = reverse ? range - 1 - i : i;
                check((reverse ? *set.max() : *set.min()) == key);
                set.erase(key);
            }
            check(set.empty() && !set.min() && !set.max());
            set.clear();
            for (int key : sparse) set.insert(key);
            check(set.size() == sparse.size());
        }
    }
    fenwick_set<std::uint32_t> unsigned_set(129, {0, 63, 64, 128});
    const auto limit = std::numeric_limits<std::uint32_t>::max();
    check(unsigned_set.rank(limit) == 4 && unsigned_set.rank2(limit) == 4);
    check(unsigned_set.less_equal(limit) == 128 && !unsigned_set.greater_equal(limit));
    // 値域外キーの無視は値域指定時に確認し、自動推定の上限違反は停止試験で検証する
    fenwick_map<std::uint32_t, int> bounded_extreme(65, {{limit, 1}, {64, 7}, {0, 2}, {64, 8}});
    check(bounded_extreme.size() == 2 && *bounded_extreme.find(64) == 7 && bounded_extreme.max_key() == 64);
    fenwick_map<int, int> negative_only({{-1, 1}, {-100, 2}});
    check(negative_only.empty() && !negative_only.find(0));
    fenwick_set<unsigned char> narrow(1000, {0, 63, 64, 255});
    check(narrow.size() == 4 && narrow[3] == 255 && narrow.rank2(255) == 4);
    fenwick_set<bool> boolean(2, {false, true, false});
    check(boolean.size() == 2 && boolean[0] == false && boolean[1] == true);
    std::cout << "word_boundaries_dense_sparse_drain_and_unsigned: ok\n";
}

void random_wide_and_clustered() {
    // 同じ乱数列でset/mapを同時更新し、疎・密・局所集中とclear後の再利用を比較する
    constexpr int operations = 100000;
    for (int range : {65, 8193, 262147, 4194319}) {
        for (bool clustered : {false, true}) {
            fenwick_set<int> set(static_cast<std::size_t>(range));
            fenwick_map<int, std::uint64_t> map(static_cast<std::size_t>(range));
            std::map<int, std::uint64_t> reference;
            std::mt19937_64 rng(0x378a249fde15ULL + static_cast<std::uint64_t>(range) + clustered);
            for (int op = 0; op < operations; ++op) {
                const int width = clustered ? std::min(range, 129) : range;
                const int start = clustered ? (range - width) / 2 : 0;
                const int key = start + static_cast<int>(rng() % static_cast<std::uint64_t>(width));
                const std::uint64_t value = rng();
                switch (rng() % 7) {
                case 0: case 1:
                    set.insert(key); map.insert(key, value); reference.emplace(key, value); break;
                case 2:
                    set.insert(key); map.insert_or_assign(key, value); reference[key] = value; break;
                case 3: case 4:
                    set.erase(key); map.erase(key); reference.erase(key); break;
                case 5:
                    set.insert(key); map[key] = value; reference[key] = value; break;
                default: {
                    const auto found = reference.find(key);
                    check(set.contains(key) == (found != reference.end()));
                    check(map.contains(key) == set.contains(key));
                    const auto* stored = map.find(key);
                    check((stored != nullptr) == (found != reference.end()));
                    if (stored) check(*stored == found->second);
                    break;
                }
                }
                check(set.size() == reference.size() && map.size() == reference.size());
                if (!reference.empty()) {
                    check(*set.min() == reference.begin()->first && *set.max() == reference.rbegin()->first);
                    check(map.min_key() == reference.begin()->first && map.max_key() == reference.rbegin()->first);
                }
                const int query = static_cast<int>(rng() % static_cast<std::uint64_t>(range + 2)) - 1;
                const auto lower = reference.lower_bound(query), upper = reference.upper_bound(query);
                const int prev = upper == reference.begin() ? -1 : std::prev(upper)->first;
                const int next = lower == reference.end() ? -1 : lower->first;
                check(set.less_equal(query).value_or(-1) == prev && set.greater_equal(query).value_or(-1) == next);
                if ((op & 2047) == 0) {
                    std::vector<int> expected, actual;
                    for (const auto& entry : reference) expected.push_back(entry.first);
                    set.for_each([&](int x) { actual.push_back(x); });
                    check(actual == expected);
                    for (int sample = 0; sample < 16 && !expected.empty(); ++sample) {
                        const auto k = static_cast<std::size_t>(rng() % expected.size());
                        check(set[k] == expected[k] && map.kth(k).first == expected[k]);
                        const int x = expected[k];
                        check(set.rank(x) == k && set.rank2(x) == k + 1);
                    }
                }
                if (op != 0 && op % 25000 == 0) {
                    set.clear(); map.clear(); reference.clear();
                    set.clear(); map.clear();
                    check(set.empty() && map.empty());
                }
            }
        }
    }
    std::cout << "word_random_wide_clustered_800000_ops: ok\n";
}

void run() {
    exhaustive_words();
    boundaries_and_drain();
    random_wide_and_clustered();
}

} // namespace fenwick_word_self_test

namespace fenwick_workload_benchmark {
// 旧版・試作・採用版を同じ入力で比較するためのベンチマーク
using Key = int;
using Value = std::uint64_t;
using Clock = std::chrono::steady_clock;
std::uint64_t sink = 0;
template <class T> void escape(const T& value) { asm volatile("" : : "g"(&value) : "memory"); }
struct Profile { std::string name; int range; int count; bool cluster; };
struct Dataset {
    Profile p;
    std::vector<Key> initial, query, hits, edge;
    std::vector<std::size_t> ranks;
    std::vector<std::pair<Key,Value>> entries;
    Dataset(Profile profile, std::uint64_t seed, int q) : p(std::move(profile)) {
        std::mt19937_64 rng(seed);
        std::vector<Key> pool;
        const int width = p.cluster ? std::max(p.count + p.count / 8, 1) : p.range;
        const int begin = p.cluster ? (p.range - width) / 2 : 0;
        pool.resize(static_cast<std::size_t>(width));
        std::iota(pool.begin(),pool.end(),begin);
        std::shuffle(pool.begin(),pool.end(),rng);
        initial.assign(pool.begin(),pool.begin()+p.count);
        for(Key key: initial) entries.emplace_back(key, static_cast<Value>(key)*11995408973635179863ULL);
        edge=initial;
        std::sort(edge.begin(),edge.end());
        query.reserve(static_cast<std::size_t>(q));
        hits.reserve(static_cast<std::size_t>(q));
        ranks.reserve(static_cast<std::size_t>(q));
        for(int i=0;i<q;++i) {
            query.push_back(static_cast<Key>(rng()%static_cast<std::uint64_t>(p.range)));
            hits.push_back(initial[static_cast<std::size_t>(rng()%initial.size())]);
            ranks.push_back(static_cast<std::size_t>(rng()%initial.size()));
        }
    }
};
struct Sample { double ns; std::uint64_t checksum; };
struct Task { std::string library, api, unit; std::size_t operations; std::function<Sample()> run; };
template <class F> Sample timed(F&& f) {
    asm volatile("" ::: "memory");
    const auto begin=Clock::now();
    const std::uint64_t sum=f();
    escape(sum);
    const auto end=Clock::now();
    sink ^= sum;
    return {std::chrono::duration<double,std::nano>(end-begin).count(),sum};
}
template <class S, class M> std::vector<Task> tasks(const std::shared_ptr<Dataset>& d) {
    std::vector<Task> out;
    auto s=std::make_shared<S>(static_cast<std::size_t>(d->p.range),d->initial);
    auto m=std::make_shared<M>(static_cast<std::size_t>(d->p.range),d->entries);
    const auto q=d->query.size();
    auto read_set=[&](std::string name, auto operation) {
        out.push_back({"set",std::move(name),"query",q,[=] {
            escape(*s); escape(d->query);
            return timed([&] {std::uint64_t sum=0;for(std::size_t i=0;i<q;++i) sum+=operation(*s,*d,i);return sum;});
        }});
    };
    read_set("contains",[](const S& set,const Dataset& a,std::size_t i){return set.contains(a.query[i]);});
    read_set("rank",[](const S& set,const Dataset& a,std::size_t i){return set.rank(a.query[i]);});
    read_set("rank2",[](const S& set,const Dataset& a,std::size_t i){return set.rank2(a.query[i]);});
    read_set("kth",[](const S& set,const Dataset& a,std::size_t i){return static_cast<std::uint64_t>(set[a.ranks[i]]);});
    read_set("less_equal",[](const S& set,const Dataset& a,std::size_t i){return static_cast<std::uint64_t>(set.less_equal(a.query[i]).value_or(-1));});
    read_set("greater_equal",[](const S& set,const Dataset& a,std::size_t i){return static_cast<std::uint64_t>(set.greater_equal(a.query[i]).value_or(-1));});
    auto read_map=[&](std::string name, auto operation) {
        out.push_back({"map",std::move(name),"query",q,[=] {
            escape(*m); escape(d->query);
            return timed([&] {std::uint64_t sum=0;for(std::size_t i=0;i<q;++i)sum+=operation(*m,*d,i);return sum;});
        }});
    };
    read_map("find",[](const M& map,const Dataset& a,std::size_t i){auto p=map.find(a.query[i]);return p?*p:0;});
    read_map("contains",[](const M& map,const Dataset& a,std::size_t i){return map.contains(a.query[i]);});
    read_map("rank",[](const M& map,const Dataset& a,std::size_t i){return map.rank(a.query[i]);});
    read_map("rank2",[](const M& map,const Dataset& a,std::size_t i){return map.rank2(a.query[i]);});
    read_map("greater_equal",[](const M& map,const Dataset& a,std::size_t i){auto p=map.greater_equal(a.query[i]);return p?p->second:0;});
    read_map("find_hit",[](const M& map,const Dataset& a,std::size_t i){auto p=map.find(a.hits[i]);return p?*p:0;});
    read_map("less_equal",[](const M& map,const Dataset& a,std::size_t i){auto p=map.less_equal(a.query[i]);return p?p->second:0;});
    read_map("kth",[](const M& map,const Dataset& a,std::size_t i){return map.kth(a.ranks[i]).second;});
    out.push_back({"set","insert_unique","insert",d->initial.size(),[=] {
        S x(static_cast<std::size_t>(d->p.range));
        return timed([&]{for(Key key:d->initial)x.insert(key);escape(x);return x.size();});
    }});
    out.push_back({"map","insert_unique","insert",d->entries.size(),[=] {
        M x(static_cast<std::size_t>(d->p.range));
        return timed([&]{for(const auto& e:d->entries)x.insert(e.first,e.second);escape(x);return x.size();});
    }});
    out.push_back({"set","erase_present","erase",d->initial.size(),[=] {
        S x(static_cast<std::size_t>(d->p.range),d->initial);
        return timed([&]{for(Key key:d->initial)x.erase(key);escape(x);return x.size();});
    }});
    out.push_back({"map","erase_present","erase",d->entries.size(),[=] {
        M x(static_cast<std::size_t>(d->p.range),d->entries);
        return timed([&]{for(Key key:d->initial)x.erase(key);escape(x);return x.size();});
    }});
    out.push_back({"map","insert_or_assign","assign",q,[=] {
        M x(static_cast<std::size_t>(d->p.range),d->entries);
        return timed([&]{for(Key key:d->hits)x.insert_or_assign(key,static_cast<Value>(key));escape(x);return x.size();});
    }});
    out.push_back({"map","operator_existing","query",q,[=] {
        M x(static_cast<std::size_t>(d->p.range),d->entries);
        return timed([&]{std::uint64_t sum=0;for(Key key:d->hits)sum+=x[key];escape(x);return sum;});
    }});
    auto update=[&](std::string name, bool toggle, bool duplicate) {
        out.push_back({"set",std::move(name),toggle?"erase+insert":"call",q,[=] {
            S x(static_cast<std::size_t>(d->p.range),d->initial);
            return timed([&] {
                if(toggle) {for(Key key:d->query){x.erase(key);x.insert(key);}}
                else if(duplicate) {for(Key key:d->hits)x.insert(key);}
                else {for(Key key:d->query)x.insert(key);}
                escape(x);return static_cast<std::uint64_t>(x.size());
            });
        }});
    };
    update("insert_mixed",false,false);
    update("insert_duplicate",false,true);
    update("erase_insert",true,false);
    out.push_back({"map","erase_insert","erase+insert",q,[=] {
        M x(static_cast<std::size_t>(d->p.range),d->entries);
        return timed([&] {for(Key key:d->query){x.erase(key);x.insert(key,static_cast<Value>(key));}escape(x);return x.size();});
    }});
    for(bool reverse:{false,true}) {
        out.push_back({"set",reverse?"erase_max":"erase_min","erase",d->initial.size(),[=] {
            S x(static_cast<std::size_t>(d->p.range),d->initial);
            return timed([&] {std::uint64_t sum=0;while(!x.empty()){Key key=reverse?*x.max():*x.min();sum+=static_cast<std::uint64_t>(key);x.erase(key);}escape(x);return sum;});
        }});
    }
    for(bool reverse:{false,true}) for(bool early:{false,true}) {
        const std::size_t visits=early?std::max(d->initial.size()/4,std::size_t{1}):d->initial.size();
        const std::size_t passes=std::max(std::size_t{1},std::min(std::size_t{20},q/visits));
        out.push_back({"set",std::string(reverse?"for_each_r":"for_each")+(early?"_break":""),"element",passes*visits,[=] {
            return timed([&] {
                std::uint64_t sum=0;
                for(std::size_t pass=0;pass<passes;++pass){
                    escape(*s);std::size_t seen=0;
                    if(early){auto cb=[&](Key key){sum+=static_cast<std::uint64_t>(key);return ++seen==visits;};if(reverse)s->for_each_r(cb);else s->for_each(cb);}
                    else{auto cb=[&](Key key){sum+=static_cast<std::uint64_t>(key);};if(reverse)s->for_each_r(cb);else s->for_each(cb);}
                }return sum;
            });
        }});
    }
    out.push_back({"map","for_each","element",d->initial.size(),[=] {
        escape(*m);return timed([&]{std::uint64_t sum=0;m->for_each([&](Key key,Value value){sum+=static_cast<std::uint64_t>(key)+value;});return sum;});
    }});
    out.push_back({"set","construct","container",1,[=] {
        std::optional<S> x;
        auto r=timed([&] {x.emplace(static_cast<std::size_t>(d->p.range),d->initial);escape(*x);return x->size();});
        return r;
    }});
    out.push_back({"map","construct","container",1,[=] {
        std::optional<M> x;
        auto r=timed([&]{x.emplace(static_cast<std::size_t>(d->p.range),d->entries);escape(*x);return x->size();});return r;
    }});
    out.push_back({"set","clear","container",1,[=] {
        S x(static_cast<std::size_t>(d->p.range),d->initial);
        return timed([&]{x.clear();escape(x);return x.size();});
    }});
    out.push_back({"map","clear","container",1,[=] {
        M x(static_cast<std::size_t>(d->p.range),d->entries);
        return timed([&]{x.clear();escape(x);return x.size();});
    }});
    return out;
}

// 密度・分布を変えて主要操作を測る。値は5回の中央値で、短い走査は反復する
void run() {
    const std::vector<Profile> profiles = {
        {"tiny", 127, 63, false}, {"medium", 262147, 131073, false},
        {"large", 4194319, 131072, false}, {"sparse", 4194319, 256, false},
        {"cluster", 4194319, 1024, true}
    };
    std::cout << "fenwick_workload_benchmarks_begin\n";
    std::cout << "profile,range,n,library,api,unit,operations,ns_per_unit\n";
    for (const auto& profile : profiles) {
        const auto data = std::make_shared<Dataset>(profile, 20260914, 100000);
        auto operations = tasks<fenwick_set<Key>, fenwick_map<Key, Value>>(data);
        for (auto& task : operations) {
            std::array<double, 5> samples{};
            const auto warm = task.run();
            for (double& sample : samples) {
                const auto result = task.run();
                if (result.checksum != warm.checksum) std::abort();
                sample = result.ns;
            }
            std::sort(samples.begin(), samples.end());
            std::cout << profile.name << ',' << profile.range << ',' << profile.count << ','
                      << task.library << ',' << task.api << ',' << task.unit << ',' << task.operations << ','
                      << std::fixed << std::setprecision(3) << samples[2] / static_cast<double>(task.operations) << '\n';
        }
    }
}

} // namespace fenwick_workload_benchmark

namespace fenwick_raw_values_extra_test {
void check(bool ok) { if (!ok) std::abort(); }

struct Tracked {
    static inline int alive = 0;
    static inline int copies_until_failure = -1;
    static inline int moves_until_failure = -1;
    static inline bool default_failure = false;
    static inline bool assignment_failure = false;
    int data;
    Tracked() : data(0) { if (default_failure) throw 1; ++alive; }
    explicit Tracked(int value) : data(value) { ++alive; }
    Tracked(const Tracked& other) : data(other.data) {
        if (copies_until_failure == 0) throw 2;
        if (copies_until_failure > 0) --copies_until_failure;
        ++alive;
    }
    Tracked(Tracked&& other) : data(other.data) {
        if (moves_until_failure == 0) throw 3;
        if (moves_until_failure > 0) --moves_until_failure;
        ++alive;
    }
    Tracked& operator=(const Tracked& other) {
        if (assignment_failure) throw 4;
        data = other.data;
        return *this;
    }
    Tracked& operator=(Tracked&& other) {
        if (assignment_failure) throw 5;
        data = other.data;
        return *this;
    }
    ~Tracked() { check(alive > 0); --alive; }
};

void failures() {
    using Map = fenwick_map<int, Tracked>;
    using fenwick_failure_self_test::expect_failure;
    {
        Map map(200);
        expect_failure([&] {
            Tracked::default_failure = true;
            (void)map[63];
        });
        // 以降は失敗した子とは別の親オブジェクトで、通常時の寿命とAPIを確認する
        check(map.empty() && map.find(63) == nullptr && Tracked::alive == 0);
        map[63].data = 42;
        auto* address = map.find(63);
        map.insert(64, Tracked(7));
        check(map.find(63) == address && address->data == 42);
        check(Tracked::alive == 2);
        expect_failure([&] {
            Tracked::copies_until_failure = 1;
            Map copy(map);
        });
        check(Tracked::alive == 2 && map.size() == 2);
        Map target(80);
        target.insert(5, Tracked(9));
        expect_failure([&] {
            Tracked::copies_until_failure = 1;
            target = map;
        });
        check(Tracked::alive == 3 && target.size() == 1 && target.find(5)->data == 9);
        target = map;
        check(Tracked::alive == 4 && target.find(63)->data == 42);
        auto* self = &target;
        target = *self;
        target = std::move(*self);
        check(target.size() == 2);
        Map moved(std::move(target));
        check(target.empty() && moved.size() == 2 && target.find(0) == nullptr);
        target = std::move(moved);
        check(moved.empty() && target.size() == 2);
        target.clear(); target.clear();
        check(Tracked::alive == 2);
    }
    check(Tracked::alive == 0);

    // 一括構築の途中、ビットは立ったが Fenwick をまだ構築していない段階で失敗させる
    for (bool inferred : {false, true}) {
        expect_failure([&] {
            std::vector<std::pair<int, Tracked>> entries;
            for (int i = 0; i < 40; ++i) entries.emplace_back(i * 3, Tracked(i));
            Tracked::moves_until_failure = 17;
            if (inferred) { Map map(std::move(entries)); }
            else { Map map(130, std::move(entries)); }
        });
        check(Tracked::alive == 0);
    }
    std::cout << "constructor_copy_assignment_failure_termination: ok\n";
}

struct alignas(128) Aligned {
    int data;
    Aligned() = delete;
    explicit Aligned(int x) : data(x) {}
};

void lifetime_stress() {
    using Map = fenwick_map<int, std::unique_ptr<int>>;
    Map map(10000);
    std::map<int, int> reference;
    std::mt19937 rng(20260914);
    for (int op = 0; op < 300000; ++op) {
        const int key = static_cast<int>(rng() % 10002) - 1;
        const int value = static_cast<int>(rng() & 0xffff);
        switch (rng() % 5) {
        case 0: map.insert(key, std::make_unique<int>(value));
                if (key >= 0 && key < 10000) reference.emplace(key, value);
                break;
        case 1: map.insert_or_assign(key, std::make_unique<int>(value));
                if (key >= 0 && key < 10000) reference[key] = value;
                break;
        case 2: map.erase(key); reference.erase(key); break;
        default: {
            auto p = map.find(key); auto it = reference.find(key);
            check((p != nullptr) == (it != reference.end()));
            if (p) check(**p == it->second);
        }}
        check(map.size() == reference.size());
        if (op % 5000 == 0) {
            std::map<int,int> actual;
            const auto& cm = map;
            cm.for_each([&](const int& k, const auto& v) {
                static_assert(std::is_const_v<std::remove_reference_t<decltype(v)>>);
                actual.emplace(k, *v);
            });
            check(actual == reference);
            Map moved(std::move(map));
            map = std::move(moved);
        }
        if (op % 37000 == 0) { map.clear(); reference.clear(); }
    }
    fenwick_map<int, Aligned> aligned(100);
    aligned.insert(63, Aligned(10));
    auto* p = aligned.find(63);
    check(reinterpret_cast<std::uintptr_t>(p) % 128 == 0 && p->data == 10);
    aligned.erase(63); aligned.insert(63, Aligned(11));
    check(aligned.find(63)->data == 11);
    static_assert(!std::is_copy_constructible_v<Map>);
    struct AliasValue {
        int data;
        ~AliasValue() { data = -1; }
    };
    fenwick_map<int, AliasValue> alias(100);
    alias.insert(17, AliasValue{17});
    alias.erase(alias.find(17)->data);
    check(alias.empty() && alias.find(17) == nullptr);
    std::cout << "move_only_300000_ops_const_alignment_reuse: ok\n";
    std::cout << "erase_key_aliases_value: ok\n";
}

void run() {
    failures(); lifetime_stress();
    std::cout << "raw_values_extra_tests_passed\n";
}

} // namespace fenwick_raw_values_extra_test

namespace fenwick_raw_copy_regression_test {

void require(bool condition) { if (!condition) std::abort(); }

struct NoAssign {
    const int data;
    explicit NoAssign(int value) noexcept : data(value) {}
    NoAssign(const NoAssign&) = default;
    NoAssign& operator=(const NoAssign&) = delete;
};

struct Counted {
    static inline int alive = 0;
    int data;
    explicit Counted(int value) noexcept : data(value) { ++alive; }
    Counted(const Counted& other) noexcept : data(other.data) { ++alive; }
    Counted(Counted&& other) noexcept : data(other.data) { ++alive; }
    Counted& operator=(const Counted&) = delete;
    ~Counted() { --alive; require(alive >= 0); }
};

struct alignas(64) Padded {
    char tag;
    int number;
};

template <class Map>
void verify(const Map& map, const std::map<int, int>& reference) {
    require(map.size() == reference.size());
    std::size_t rank = 0;
    map.for_each([&](int key, const auto& value) {
        auto it = reference.find(key);
        require(it != reference.end() && value.data == it->second);
        require(map.rank(key) == rank && map.rank2(key) == rank + 1);
        require(map.kth(rank).first == key && map.kth(rank).second.data == it->second);
        ++rank;
    });
    if (!reference.empty()) {
        require(map.min_key() == reference.begin()->first);
        require(map.max_key() == reference.rbegin()->first);
    }
}

void copy_boundaries_and_holes() {
    // 密・疎・密な小範囲のコピーと、古い生存スロットが残らないことを検証する
    for (int range : {0, 1, 2, 7, 63, 64, 65, 127, 128, 129, 1024, 4097}) {
        for (int pattern = 0; pattern < 5; ++pattern) {
            fenwick_map<int, NoAssign> source(static_cast<std::size_t>(range));
            std::map<int, int> reference;
            for (int key = 0; key < range; ++key) {
                const bool include = pattern == 0 || (pattern == 1 && key % 2 == 0) ||
                    (pattern == 2 && key % 65 == 0) || (pattern == 3 && key >= range / 2 && key < range / 2 + 5);
                if (include) { source.insert(key, NoAssign(key * 3)); reference[key] = key * 3; }
            }
            auto copy = source;
            verify(copy, reference);
            fenwick_map<int, NoAssign> target(static_cast<std::size_t>(range));
            for (int key = 0; key < range; key += 3) target.insert(key, NoAssign(-key));
            target = source;
            verify(target, reference);
            target.clear(); target = source;
            verify(target, reference);
            target = fenwick_map<int, NoAssign>{}; target = source;
            verify(target, reference);
            target = fenwick_map<int, NoAssign>(static_cast<std::size_t>(range) + 2);
            target = source;
            verify(target, reference);
            verify(source, reference);
        }
    }
    // bool・constメンバ・paddingを持つ型も、生存値の値を変えずにコピーする
    fenwick_map<int, bool> bits(1024);
    for (int key = 1; key < 1024; key += 2) bits.insert(key, key % 3 == 0);
    auto bit_copy = bits;
    for (int key = 0; key < 1024; ++key) {
        require(bit_copy.contains(key) == (key % 2 != 0));
        if (bit_copy.contains(key)) require(*bit_copy.find(key) == (key % 3 == 0));
    }
    fenwick_map<int, Padded> padded(193);
    for (int key = 0; key < 193; ++key) if (key % 5 != 0) padded.insert(key, Padded{'a', key});
    auto padded_copy = padded;
    padded_copy.for_each([](int key, const Padded& value) { require(value.tag == 'a' && value.number == key); });
    std::cout << "raw_copy_boundaries_holes_const_member_bool_padding: ok\n";
}

void lifetime_and_failures() {
    {
        fenwick_map<int, Counted> source(129), target(129);
        for (int key = 0; key < 129; key += 2) source.insert(key, Counted(key));
        for (int key = 0; key < 129; key += 3) target.insert(key, Counted(key + 1));
        auto copied = source;
        require(Counted::alive == static_cast<int>(source.size() + target.size() + copied.size()));
        target = source;
        require(Counted::alive == static_cast<int>(source.size() + target.size() + copied.size()));
        target.clear(); copied.clear();
        require(Counted::alive == static_cast<int>(source.size()));
    }
    require(Counted::alive == 0);
    using Tracked = fenwick_raw_values_extra_test::Tracked;
    using Map = fenwick_map<int, Tracked>;
    // 同じ値域への途中コピーも復旧せず、その場で停止することを確認する
    for (int failures_after : {0, 1, 15, 31}) {
        Map source(129), target(129);
        for (int key = 0; key < 64; key += 2) source.insert(key, Tracked(key));
        target.insert(65, Tracked(-5));
        const int expected_alive = Tracked::alive;
        fenwick_failure_self_test::expect_failure([&] {
            Tracked::copies_until_failure = failures_after;
            target = source;
        });
        // 親プロセスでは失敗していないため、その内容・寿命と正常な代入を引き続き検証する
        require(Tracked::alive == expected_alive && target.size() == 1 && target.find(65)->data == -5);
        require(source.size() == 32);
        for (int key = 0; key < 64; key += 2) require(source.find(key)->data == key);
        target = source;
        for (int key = 0; key < 64; key += 2) require(target.find(key)->data == key);
    }
    require(Tracked::alive == 0);
    std::cout << "raw_copy_lifetime_and_same_range_failure_termination: ok\n";
}

template <class String>
void strings() {
    using Char = typename String::value_type;
    fenwick_map<int, String> source(193), target(193);
    for (int key = 0; key < 193; ++key) {
        if (key % 3 == 0) source.insert(key, String(static_cast<std::size_t>(key % 81), Char('b')));
        target.insert(key, String(100, Char('a')));
    }
    std::map<int, const void*> value_addresses, buffer_addresses;
    source.for_each_key([&](int key) {
        value_addresses[key] = target.find(key);
        buffer_addresses[key] = target.find(key)->data();
    });
    target = source;
    require(target.size() == source.size());
    source.for_each([&](int key, const String& value) {
        require(*target.find(key) == value);
        require(target.find(key) == value_addresses[key]);
        require(target.find(key)->data() == buffer_addresses[key]);
    });
    // 容量不足なら文字列が再確保し、新しいキーには値を構築する
    source.insert_or_assign(0, String(1000, Char('c')));
    source.insert(1, String(40, Char('d')));
    target = source;
    source.for_each([&](int key, const String& value) { require(*target.find(key) == value); });
    auto* self = &target;
    target = *self;
    target = std::move(*self);
    require(target.size() == source.size());
    source.clear(); target = source;
    require(target.empty());
}

void random_copy_lifetimes() {
    std::mt19937 random(924681);
    fenwick_map<int, std::uint64_t> source(2051), target(2051);
    std::map<int, std::uint64_t> reference;
    for (int operation = 0; operation < 100000; ++operation) {
        const int key = static_cast<int>(random() % 2051U);
        const auto value = static_cast<std::uint64_t>(random());
        switch (random() % 7U) {
        case 0: source.erase(key); reference.erase(key); break;
        case 1: source.insert(key, value); reference.emplace(key, value); break;
        default: source.insert_or_assign(key, value); reference[key] = value; break;
        }
        if (operation % 251 == 0) {
            target = source;
            auto copied = source;
            std::map<int, std::uint64_t> actual;
            target.for_each([&](int k, auto v) { actual[k] = v; });
            require(actual == reference);
            actual.clear(); copied.for_each([&](int k, auto v) { actual[k] = v; });
            require(actual == reference);
            if (!reference.empty()) {
                const auto it = reference.begin();
                target.erase(it->first);
                require(source.contains(it->first));
            }
        }
        if (operation % 10007 == 0) { source.clear(); reference.clear(); }
    }
    strings<std::string>(); strings<std::wstring>(); strings<std::u16string>(); strings<std::u32string>();
    std::cout << "raw_copy_strings_buffer_reuse_fallback_and_random_100000_ops: ok\n";
}

void run() {
    copy_boundaries_and_holes();
    lifetime_and_failures();
    random_copy_lifetimes();
    std::cout << "raw_values_copy_regressions_passed\n";
}

} // namespace fenwick_raw_copy_regression_test

namespace fenwick_raw_copy_benchmark {
using Clock = std::chrono::steady_clock;
std::uint64_t sink = 0;
template <class T> void escape(const T& object) { asm volatile("" : : "g"(&object) : "memory"); }

template <class V>
V value_for(int key) {
    if constexpr (std::is_same_v<V, std::string>) return std::string(64, static_cast<char>('a' + key % 26));
    else return static_cast<V>(key);
}

template <class V>
void measure(const char* type, std::size_t range, std::size_t count) {
    fenwick_map<int, V> source(range);
    for (std::size_t i = 0; i < count; ++i) {
        const auto key = static_cast<int>(i * 2);
        source.insert(key, value_for<V>(key));
    }
    // 内容が異なる同じキー配置と、異なるキー配置へのコピーを分けて測る
    for (int mode = 0; mode < 3; ++mode) {
        std::array<double, 5> elapsed{};
        for (double& sample : elapsed) {
            fenwick_map<int, V> target(range);
            if (mode != 0) for (std::size_t i = 0; i < count; ++i) {
                const auto key = static_cast<int>(i * 2 + (mode == 2));
                target.insert(key, value_for<V>(key + 1));
            }
            std::optional<fenwick_map<int, V>> copied;
            escape(target); escape(source);
            const auto begin = Clock::now();
            if (mode == 0) copied.emplace(source);
            else target = source;
            if (mode == 0) escape(*copied); else escape(target);
            const auto end = Clock::now();
            const auto& result = mode == 0 ? *copied : target;
            if (result.size() != source.size()) std::abort();
            result.for_each([&](int key, const V& value) {
                if (value != *source.find(key)) std::abort();
                sink += static_cast<std::uint64_t>(key);
            });
            sample = std::chrono::duration<double, std::nano>(end - begin).count();
        }
        std::sort(elapsed.begin(), elapsed.end());
        std::cout << type << ',' << range << ',' << count << ','
                  << (mode == 0 ? "copy_construct" : mode == 1 ? "copy_assign_same_keys" : "copy_assign_disjoint")
                  << ',' << std::fixed << std::setprecision(3) << elapsed[2] << '\n';
    }
}

void run() {
    std::cout << "copy_benchmark,value_range,n,api,ns_per_container\n";
    measure<std::uint64_t>("u64", 1024, 512);
    measure<std::uint64_t>("u64", 262147, 131073);
    measure<std::string>("string", 65537, 16384);
    escape(sink);
}
} // namespace fenwick_raw_copy_benchmark

namespace fenwick_review_self_test {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "review regression FAILED: " << message << '\n';
        std::abort();
    }
}

// 移動元も通常の空集合として問い合わせ・clear・代入ができることを確認する
void set_copy_move_edges() {
    using Set = fenwick_set<int>;
    static_assert(std::is_copy_constructible_v<Set> && std::is_copy_assignable_v<Set>);
    static_assert(std::is_nothrow_move_constructible_v<Set> && std::is_nothrow_move_assignable_v<Set>);
    const std::set<int> empty;
    for (int range : {0, 1, 63, 64, 65, 129, 1025}) {
        Set source(static_cast<std::size_t>(range));
        std::set<int> expected;
        for (int key = 0; key < range; key += 7) { source.insert(key); expected.insert(key); }
        Set copied(source);
        Set moved(std::move(source));
        fenwick_set_self_test::check_all_apis(moved, expected, range, "move destination");
        fenwick_set_self_test::check_all_apis(source, empty, 0, "move source");
        source.insert(0); source.erase(0); source.clear();
        require(source.empty(), "moved-from set has zero range");

        // 自己コピー・自己moveは内容を維持し、移動元は異なる値域でも再利用できる
        auto* same = &moved;
        moved = *same;
        moved = std::move(*same);
        fenwick_set_self_test::check_all_apis(moved, expected, range, "self assignments");
        source = copied;
        source = Set(3, {2});
        source = std::move(moved);
        fenwick_set_self_test::check_all_apis(source, expected, range, "move assignment");
        fenwick_set_self_test::check_all_apis(moved, empty, 0, "move-assigned source");
        moved = Set(2, {1});
        require(moved.contains(1) && moved.rank2(INT_MAX) == 1, "source reused");
    }
    fenwick_set<std::uint32_t> source(130, {0, 63, 64, 129});
    auto moved = std::move(source);
    require(source.rank2(UINT32_MAX) == 0 && !source.contains(0), "unsigned empty after move");
    require(moved.rank2(UINT32_MAX) == 4 && moved.rank2(129) == 4, "unsigned rank2 limit");
    std::cout << "review_set_copy_move_empty_reuse_self_assignment: ok\n";
}

// 更新にコピー・move・swapを混ぜ、サイズや配列の対応が壊れないことを確認する
void random_set_ownership() {
    constexpr int range_limit = 259;
    std::array<fenwick_set<int>, 4> sets;
    std::array<std::set<int>, 4> reference;
    std::array<int, 4> ranges{};
    std::mt19937 rng(572617);
    for (int operation = 0; operation < 100000; ++operation) {
        const std::size_t i = rng() % sets.size(), j = rng() % sets.size();
        const int key = static_cast<int>(rng() % (range_limit + 4)) - 2;
        switch (rng() % 10U) {
        case 0: case 1: case 2:
            sets[i].insert(key);
            if (0 <= key && key < ranges[i]) reference[i].insert(key);
            break;
        case 3: sets[i].erase(key); reference[i].erase(key); break;
        case 4: sets[i] = sets[j]; reference[i] = reference[j]; ranges[i] = ranges[j]; break;
        case 5:
            sets[i] = std::move(sets[j]);
            if (i != j) {
                reference[i] = reference[j]; reference[j].clear();
                ranges[i] = ranges[j]; ranges[j] = 0;
            }
            break;
        case 6:
            std::swap(sets[i], sets[j]); std::swap(reference[i], reference[j]);
            std::swap(ranges[i], ranges[j]);
            break;
        case 7: sets[i].clear(); reference[i].clear(); break;
        case 8:
            ranges[i] = static_cast<int>(rng() % (range_limit + 1));
            sets[i] = fenwick_set<int>(static_cast<std::size_t>(ranges[i]));
            reference[i].clear();
            break;
        default: {
            auto copy = sets[i];
            require(copy.size() == reference[i].size(), "random copy size");
            break;
        }
        }
        for (std::size_t k = 0; k < sets.size(); ++k) {
            require(sets[k].size() == reference[k].size(), "random ownership size");
            require(sets[k].contains(key) == reference[k].contains(key), "random ownership contains");
            if (operation % 257 == 0)
                fenwick_set_self_test::check_all_apis(sets[k], reference[k], ranges[k], "random ownership");
        }
    }
    std::cout << "review_set_random_ownership_100000_ops: ok\n";
}

struct MutableCopyError {};
struct ConstCopyProbe {
    inline static int alive = 0;
    inline static int mutable_copies = 0;
    int value;
    explicit ConstCopyProbe(int number = 0) noexcept : value(number) { ++alive; }
    ConstCopyProbe(const ConstCopyProbe& rhs) noexcept : value(rhs.value) { ++alive; }
    ConstCopyProbe(ConstCopyProbe& rhs) : value(rhs.value) {
        ++mutable_copies;
        rhs.value = -999;
        throw MutableCopyError{};
    }
    ConstCopyProbe(ConstCopyProbe&& rhs) noexcept : value(rhs.value) { ++alive; }
    ~ConstCopyProbe() { --alive; }
};

struct DeletedMutableCopy {
    int value;
    explicit DeletedMutableCopy(int number = 0) : value(number) {}
    DeletedMutableCopy(const DeletedMutableCopy&) noexcept = default;
    DeletedMutableCopy(DeletedMutableCopy&) = delete;
    DeletedMutableCopy(DeletedMutableCopy&&) noexcept = default;
    ~DeletedMutableCopy() {} // バイト複製の経路ではなく、個別構築を検証する
};

// constオブジェクトからの複製・値取得は、常にconst参照のコピーを選択する
void const_value_access() {
    {
        using Map = fenwick_map<int, ConstCopyProbe>;
        Map source(193);
        for (int key : {0, 63, 64, 192}) source.insert(key, ConstCopyProbe(key + 7));
        const Map& view = source;
        Map copied(view), target(193), other_range(2);
        target.insert(1, ConstCopyProbe(-1));
        target = view;
        other_range = view;
        for (int key : {0, 63, 64, 192}) {
            require(view.find(key)->value == key + 7, "const copy source unchanged");
            require(copied.find(key)->value == key + 7 && target.find(key)->value == key + 7 &&
                    other_range.find(key)->value == key + 7, "const copy result");
            auto kth = view.kth(view.rank(key));
            auto le = view.less_equal(key), ge = view.greater_equal(key);
            require(kth.second.value == key + 7 && le->second.value == key + 7 &&
                    ge->second.value == key + 7, "const value-returning APIs");
        }
        require(ConstCopyProbe::mutable_copies == 0, "non-const overload never called");
        require(ConstCopyProbe::alive == 16, "copies own exactly the live values");
    }
    require(ConstCopyProbe::alive == 0, "no leaks after const copies");
    {
        fenwick_map<int, DeletedMutableCopy> source(130);
        source.insert(129, DeletedMutableCopy(42));
        const auto& view = source;
        auto copied = view;
        copied = view;
        require(view.kth(0).second.value == 42, "deleted mutable copy kth");
        require(view.less_equal(129)->second.value == 42 && view.greater_equal(129)->second.value == 42,
                "deleted mutable copy predecessor successor");
    }
    std::cout << "review_const_copy_overloads_source_integrity_and_lifetimes: ok\n";
}

struct MoveCounted {
    inline static int moves = 0;
    inline static int alive = 0;
    int value;
    explicit MoveCounted(int number) noexcept : value(number) { ++alive; }
    MoveCounted(const MoveCounted&) = delete;
    MoveCounted(MoveCounted&& rhs) noexcept : value(rhs.value) { ++moves; ++alive; }
    MoveCounted& operator=(MoveCounted&& rhs) noexcept { value = rhs.value; ++moves; return *this; }
    ~MoveCounted() { --alive; }
};

// 公開APIの値渡しは維持し、内部helperへの値渡しによる余分なmoveだけを除く
void value_move_counts_and_failures() {
    {
        fenwick_map<int, MoveCounted> map(129);
        MoveCounted::moves = 0;
        map.insert(64, MoveCounted(10));
        require(MoveCounted::moves == 1 && MoveCounted::alive == 1, "one move for insert");
        MoveCounted::moves = 0;
        map.insert(64, MoveCounted(20));
        map.insert(-1, MoveCounted(20));
        volatile int invalid_key = 129; // 値域外の実行時入力も検証する
        map.insert(invalid_key, MoveCounted(20));
        require(MoveCounted::moves == 0 && map.find(64)->value == 10, "no move for ignored insert");
        map.insert_or_assign(64, MoveCounted(30));
        require(MoveCounted::moves == 1 && map.find(64)->value == 30, "one move assignment");
    }
    require(MoveCounted::alive == 0, "move-only values destroyed");

    // 挿入・値返却・既存値への代入の失敗はいずれも検査境界で停止する
    using Tracked = fenwick_raw_values_extra_test::Tracked;
    using fenwick_failure_self_test::expect_failure;
    {
        fenwick_map<int, Tracked> map(130);
        map.insert(64, Tracked(42));
        expect_failure([&] {
            Tracked::moves_until_failure = 0;
            map.insert(65, Tracked(1));
        });
        require(map.size() == 1 && !map.contains(65) && Tracked::alive == 1,
                "parent remains separate from failure child");
        Tracked::moves_until_failure = 0;
        map.insert(64, Tracked(99)); // 重複挿入では、throwするmove自体を呼ばない
        map.insert(-1, Tracked(99));
        Tracked::moves_until_failure = -1;
        for (int query = 0; query < 3; ++query) {
            expect_failure([&] {
                Tracked::copies_until_failure = 0;
                if (query == 0) (void)std::as_const(map).kth(0);
                if (query == 1) (void)std::as_const(map).less_equal(64);
                if (query == 2) (void)std::as_const(map).greater_equal(64);
            });
            require(map.size() == 1 && map.find(64)->data == 42 && Tracked::alive == 1,
                    "normal parent lifetime after isolated query failure");
        }
        expect_failure([&] {
            Tracked::assignment_failure = true;
            map.insert_or_assign(64, Tracked(7));
        });
        auto target = map;
        expect_failure([&] {
            Tracked::assignment_failure = true;
            target = map;
        });
        target.insert_or_assign(64, Tracked(9));
        target = map;
        require(target.find(64)->data == 42, "normal copy assignment after independent death tests");
    }
    require(Tracked::alive == 0, "normal value operations destroy all objects");
    std::cout << "review_redundant_moves_removed_and_value_failure_termination: ok\n";
}

struct DefaultChangesKey {
    inline static int next_key = 64;
    inline static int alive = 0;
    const int saved_key;
    DefaultChangesKey() : saved_key(next_key++) { ++alive; }
    ~DefaultChangesKey() { --alive; }
};

// V のデフォルト構築が引数の参照元を変更しても、valueと存在ビットの座標を一致させる
void subscript_key_alias() {
    {
        fenwick_map<int, DefaultChangesKey> map(130);
        auto& value = map[DefaultChangesKey::next_key];
        require(value.saved_key == 64 && DefaultChangesKey::next_key == 65, "default constructor changes key");
        require(map.size() == 1 && map.contains(64) && !map.contains(65), "subscript uses original key");
        require(map.find(64) == &value, "subscript value position");
        map.erase(64);
        require(DefaultChangesKey::alive == 0 && map.empty(), "aliased insertion erased correctly");
        auto& next = map[DefaultChangesKey::next_key];
        require(next.saved_key == 65 && map.contains(65) && !map.contains(66), "key alias repeated");
    }
    require(DefaultChangesKey::alive == 0, "aliased subscript values destroyed");
    std::cout << "review_operator_subscript_key_alias_and_value_lifetime: ok\n";
}

void run() {
    set_copy_move_edges();
    random_set_ownership();
    const_value_access();
    value_move_counts_and_failures();
    subscript_key_alias();
}

} // namespace fenwick_review_self_test

namespace fenwick_review_v11_self_test {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "review_v11: " << message << '\n';
        std::abort();
    }
}

// 値域自動推定が不可能な正のキーを、黙って入力から落とさない
void inferred_range_limits() {
    fenwick_set<int> negative_set(std::vector<int>{-2, -1});
    fenwick_map<int, int> negative_map(std::vector<std::pair<int, int>>{{-2, 1}, {-1, 2}});
    require(negative_set.empty() && negative_map.empty(), "negative inferred input");
    fenwick_set<std::uint32_t> set(65, {0, 64, UINT32_MAX});
    fenwick_map<std::uint32_t, int> map(65, {{0, 2}, {64, 7}, {UINT32_MAX, 9}});
    require(set.size() == 2 && map.size() == 2 && !map.contains(UINT32_MAX), "explicit range still ignores invalid keys");
#ifndef NDEBUG
    // 不正な値域は事前条件違反。NDEBUG では不正入力を実行しない
    for (int position = 0; position < 3; ++position) {
        std::vector<std::uint32_t> input{0, 1, 64};
        input[static_cast<std::size_t>(position)] = UINT32_MAX;
        fenwick_failure_self_test::expect_failure([&] {
            fenwick_set<std::uint32_t> invalid(input);
        }, "fenwick_set/map: value_range overflow");
        fenwick_failure_self_test::expect_failure([&] {
            fenwick_map<std::uint32_t, int> invalid(std::vector<std::pair<std::uint32_t, int>>{
                {input[0], 2}, {input[1], 3}, {input[2], 7}});
        }, "fenwick_set/map: value_range overflow");
    }
#endif
    std::cout << "review_v11_inferred_range_overflow_and_explicit_range: ok\n";
}

struct DestructionError {};
struct DestructionValue {
    inline static bool fail = false;
    inline static int alive = 0;
    int value = 0;
    DestructionValue() { ++alive; }
    DestructionValue(const DestructionValue& rhs) : value(rhs.value) { ++alive; }
    DestructionValue& operator=(const DestructionValue&) = default;
    ~DestructionValue() noexcept(false) {
        --alive;
        if (fail) throw DestructionError{};
    }
};
struct NoAssignDestructionValue : DestructionValue {
    const int constant = 0;
};

// 破棄に失敗して生存ビットだけが残っても、呼び出し元に戻して利用を続けさせない
void destruction_failure_boundary() {
    using Map = fenwick_map<int, DestructionValue>;
    using fenwick_failure_self_test::expect_failure;
    for (int mode = 0; mode < 5; ++mode) {
        expect_failure([&] {
            Map target(130);
            target[64].value = 7;
            DestructionValue::fail = true;
            switch (mode) {
            case 0: target.erase(64); break;
            case 1: target.clear(); break;
            case 2: break; // スコープを抜ける際の map 自身のデストラクタ
            case 3: target = Map(65); break;
            case 4: {
                const Map source(130);
                target = source;
                break;
            }
            }
        });
        require(DestructionValue::alive == 0 && !DestructionValue::fail, "failure isolated in child");
    }
    // コピー代入不能な V の再構築経路でも、破棄失敗を同じ境界で検出する
    expect_failure([] {
        fenwick_map<int, NoAssignDestructionValue> target(130), source(130);
        target[64].value = 7;
        source[0].value = 2;
        DestructionValue::fail = true;
        target = source;
    });
    {
        Map original(130);
        original[64].value = 42;
        auto copy = original;
        require(DestructionValue::alive == 2 && copy.find(64)->value == 42, "normal copy lifetime");
        copy.erase(64);
        copy[0].value = 9;
        copy = original;
        require(copy.size() == 1 && copy.find(64)->value == 42, "normal copy assignment");
        copy.clear();
        auto moved = std::move(original);
        require(original.empty() && moved.contains(64), "normal move lifetime");
    }
    require(DestructionValue::alive == 0, "normal destruction finishes all live values");
    std::cout << "review_v11_destructor_failures_and_normal_lifetimes: ok\n";
}

// ユーザー callback の例外は停止境界の対象外であり、参照の const 性も保つ
void callback_exceptions() {
    fenwick_set<int> set(130, {0, 64, 129});
    fenwick_map<int, int> map(130, {{0, 1}, {64, 2}, {129, 3}});
    int caught = 0;
    auto expect_exception = [&](auto&& run) {
        try { run(); }
        catch (const DestructionError&) { ++caught; }
    };
    auto key_callback = [](const int&) -> bool { throw DestructionError{}; };
    auto value_callback = [](const int&, auto& value) -> void { (void)value; throw DestructionError{}; };
    expect_exception([&] { set.for_each(key_callback); });
    expect_exception([&] { set.for_each_r(key_callback); });
    expect_exception([&] { map.for_each_key(key_callback); });
    expect_exception([&] { map.for_each_key_r(key_callback); });
    expect_exception([&] { map.for_each(value_callback); });
    expect_exception([&] { map.for_each_r(value_callback); });
    expect_exception([&] { std::as_const(map).for_each(value_callback); });
    expect_exception([&] { std::as_const(map).for_each_r(value_callback); });
    require(caught == 8 && set.size() == 3 && map.size() == 3, "user callback exceptions propagate");
    require(map.kth(1) == std::pair<int, int>{64, 2}, "callback failure did not corrupt map");
    std::cout << "review_v11_user_callback_exceptions_remain_outside_failure_boundary: ok\n";
}

// 許可された符号付き・符号なし32bitキーの上位bitと上下限を検証する
// C++20 / GNU C++20 の両モードで同じ型と操作数を比較する
// 高位bitだけ異なる値域外キーを、有効な低位座標と混同しないことを調べる
template <class Key>
void integer_key_boundaries() {
    constexpr int range = 130;
    constexpr Key high = Key{1} << (std::numeric_limits<Key>::digits - 1);
    const Key maximum = std::numeric_limits<Key>::max();
    fenwick_set<Key> set(range);
    fenwick_map<Key, int> map(range);
    std::set<Key> reference_set;
    std::map<Key, int> reference_map;
    const std::vector<Key> probes{0, 1, 63, 64, 65, 129, 130, high, high + 1, high + 63, high + 64, high + 129, maximum};
    const auto verify = [&] {
        require(set.size() == reference_set.size() && map.size() == reference_map.size(), "wide sizes");
        std::vector<Key> actual;
        set.for_each([&](const Key& key) { actual.push_back(key); });
        require(actual == std::vector<Key>(reference_set.begin(), reference_set.end()), "wide iteration");
        std::size_t index = 0;
        for (const auto& [key, value] : reference_map) {
            require(set[index] == key && map.kth(index) == std::pair<Key, int>{key, value}, "wide kth");
            ++index;
        }
        for (Key key : probes) {
            const auto lower = reference_set.lower_bound(key), upper = reference_set.upper_bound(key);
            const auto before = static_cast<std::size_t>(std::distance(reference_set.begin(), lower));
            const auto through = static_cast<std::size_t>(std::distance(reference_set.begin(), upper));
            const std::optional<Key> previous = upper == reference_set.begin() ? std::nullopt : std::optional<Key>(*std::prev(upper));
            const std::optional<Key> next = lower == reference_set.end() ? std::nullopt : std::optional<Key>(*lower);
            require(set.rank(key) == before && set.rank2(key) == through, "wide set ranks");
            require(map.rank(key) == before && map.rank2(key) == through, "wide map ranks");
            require(set.less_equal(key) == previous && set.greater_equal(key) == next, "wide set bounds");
            const auto less = map.less_equal(key), greater = map.greater_equal(key);
            require(bool(less) == bool(previous) && bool(greater) == bool(next), "wide map bounds presence");
            if (less) {
                require(upper != reference_set.begin(), "wide previous exists");
                const Key previous_key = *std::prev(upper);
                require(less->first == previous_key && less->second == reference_map.at(previous_key), "wide previous value");
            }
            if (greater) {
                require(lower != reference_set.end(), "wide next exists");
                const Key next_key = *lower;
                require(greater->first == next_key && greater->second == reference_map.at(next_key), "wide next value");
            }
            require(set.contains(key) == reference_set.contains(key) && set.count(key) == reference_set.count(key), "wide set membership");
            const auto found = std::as_const(map).find(key);
            const auto expected = reference_map.find(key);
            require((found != nullptr) == (expected != reference_map.end()), "wide map find");
            if (found) require(*found == expected->second, "wide map value");
        }
    };

    // 高位bitを持つ正数だけでなく、符号付きでは負数も無視する
    for (Key key : probes) {
        set.insert(key); map.insert(key, 7);
        if (key < range) { reference_set.insert(key); reference_map.emplace(key, 7); }
    }
    if constexpr (std::is_signed_v<Key>) {
        for (Key key : {Key{-1}, -high, std::numeric_limits<Key>::min()}) {
            set.insert(key); map.insert(key, 8);
            require(!set.contains(key) && !map.find(key), "wide negative key");
            require(set.rank(key) == 0 && set.rank2(key) == 0 && !set.less_equal(key), "wide negative query");
        }
    }
    verify();
    for (Key key : probes) if (key >= range) { set.erase(key); map.erase(key); }
    verify();

    // 同じ低位bitの有効・無効キーを交互に混ぜ、更新後も全体と照合する
    std::mt19937_64 rng(981274 + std::is_signed_v<Key>);
    for (int operation = 0; operation < 20000; ++operation) {
        Key key = static_cast<Key>(rng() % 132);
        if (rng() & 1U) key += high;
        const bool valid = key < range;
        const int value = static_cast<int>(rng() & 65535);
        switch (rng() % 5) {
        case 0:
            set.insert(key); map.insert(key, value);
            if (valid) { reference_set.insert(key); reference_map.emplace(key, value); }
            break;
        case 1:
            set.insert(key); map.insert_or_assign(key, value);
            if (valid) { reference_set.insert(key); reference_map[key] = value; }
            break;
        default:
            set.erase(key); map.erase(key);
            reference_set.erase(key); reference_map.erase(key);
            break;
        }
        if (operation % 97 == 0) verify();
    }
    verify();
    fenwick_set<Key> bounded(range, {0, 64, 129, high + 64, maximum});
    fenwick_map<Key, int> bounded_map(range, {{0, 1}, {64, 2}, {129, 3}, {high + 64, 4}, {maximum, 5}});
    require(bounded.size() == 3 && bounded_map.size() == 3 && bounded_map.find(64) && *bounded_map.find(64) == 2, "wide bounded construction");
    // 自動推定できる範囲では、重複除去・先勝ちのvalue・上限座標を確認する
    fenwick_set<Key> inferred(std::vector<Key>{0, 64, 64, 129});
    fenwick_map<Key, int> inferred_map(std::vector<std::pair<Key, int>>{{0, 1}, {64, 2}, {64, 8}, {129, 3}});
    require(inferred.size() == 3 && inferred_map.size() == 3, "inferred sizes");
    require(inferred[2] == 129 && *inferred_map.find(64) == 2, "inferred duplicate and last key");
    inferred.insert(130); inferred_map.insert(130, 9);
    require(inferred.size() == 3 && inferred_map.size() == 3, "inferred range remains fixed");
}

void run() {
    inferred_range_limits();
    destruction_failure_boundary();
    callback_exceptions();
    integer_key_boundaries<std::int32_t>();
    integer_key_boundaries<std::uint32_t>();
    std::cout << "integer_key_signed_unsigned_32bit_40000_ops: ok\n";
}

} // namespace fenwick_review_v11_self_test

// 追加の確保失敗試験。-DFENWICK_TEST_ALLOCATION_FAILURE -Wl,--wrap=_Znwm で有効化する
#ifdef FENWICK_TEST_ALLOCATION_FAILURE
namespace fenwick_allocation_self_test {
inline int fail_after = -1;
inline std::size_t allocation_calls = 0;
}
extern "C" void* __real__Znwm(std::size_t bytes);
extern "C" void* __wrap__Znwm(std::size_t bytes) {
    using namespace fenwick_allocation_self_test;
    if (fail_after == 0) throw std::bad_alloc{};
    if (fail_after > 0) --fail_after;
    ++allocation_calls;
    return __real__Znwm(bytes);
}

namespace fenwick_allocation_self_test {

void run() {
    using fenwick_review_self_test::require;
    using fenwick_failure_self_test::expect_failure;
    // set の二配列、map の値領域まで、構築中の全確保失敗を停止検証する
    for (int failure : {0, 1}) {
        expect_failure([&] {
            fail_after = failure;
            fenwick_set<int> map(1025);
        });
    }
    for (int failure : {0, 1, 2}) {
        expect_failure([&] {
            fail_after = failure;
            fenwick_map<int, std::uint64_t> map(1025);
        });
    }
    // コピー代入の両配列の確保失敗を確認し、正常な親では引き続き全APIを比較する
    for (int range : {65, 129, 1025, 65537}) {
        fenwick_set<int> source(static_cast<std::size_t>(range), {0, 64, range - 1});
        const std::set<int> expected_source{0, 64, range - 1};
        for (int failure : {0, 1}) {
            fenwick_set<int> target(2, {1});
            expect_failure([&] {
                fail_after = failure;
                target = source;
            });
            expect_failure([&] {
                fail_after = failure;
                fenwick_set<int> copy(source);
            });
            fenwick_set_self_test::check_all_apis(target, {1}, 2, "unmodified parent target");
            fenwick_set_self_test::check_all_apis(source, expected_source, range, "copy parent source");
            target.insert(0);
            require(target.size() == 2, "normal target reusable");
            target = source;
            fenwick_set_self_test::check_all_apis(target, expected_source, range, "normal copy");
        }
    }

    // 同値域では値領域を再利用し、異値域ではキー・値領域と文字列を新規確保する
    using Map = fenwick_map<int, std::string>;
    Map source(1025);
    for (int key : {0, 64, 128, 1024}) source.insert(key, std::string(80, static_cast<char>('a' + key % 23)));
    for (bool same_range : {false, true}) {
        const int allocations_needed = same_range ? 4 : 7;
        for (int failure = 0; failure <= allocations_needed; ++failure) {
            Map target(same_range ? 1025 : 129);
            target.insert(1, std::string(81, 'z'));
            if (failure < allocations_needed) {
                expect_failure([&] {
                    fail_after = failure;
                    target = source;
                });
            } else {
                const std::size_t before_copy = allocation_calls;
                fail_after = failure;
                target = source;
                fail_after = -1;
                require(allocation_calls - before_copy == static_cast<std::size_t>(allocations_needed),
                        "map copy uses expected allocations");
                require(target.size() == source.size(), "map copy size");
                source.for_each([&](int key, const std::string& value) {
                    require(*target.find(key) == value, "map copied values");
                });
            }
            source.for_each([&](int key, const std::string& value) {
                require(value == std::string(80, static_cast<char>('a' + key % 23)), "normal copy source unchanged");
            });
        }
    }

    // デフォルト構築・clear・同じ値域の代入では、値領域の再確保を増やさない
    const auto before = allocation_calls;
    fenwick_set<int> empty_set;
    fenwick_map<int, std::uint64_t> empty_map;
    require(allocation_calls == before, "default constructors allocate nothing");
    fenwick_map<int, std::uint64_t> integers(1025, {{64, 7}}), target(1025, {{0, 9}});
    const auto before_copy = allocation_calls;
    target = integers;
    target.clear(); target.insert(128, 4);
    require(allocation_calls == before_copy, "same-range assignment clear refill reuse buffers");
    Map text_target(1025);
    source.for_each([&](int key, const std::string&) { text_target.insert(key, std::string(100, 'y')); });
    fail_after = 0;
    text_target = source;
    fail_after = -1;
    require(text_target.size() == source.size(), "string buffer reuse without allocation");
    std::cout << "review_allocation_failure_termination_and_no_allocation_reuse: ok\n";
}

} // namespace fenwick_allocation_self_test
#endif

namespace fenwick_key_type_self_test {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "key_type: " << message << '\n';
        std::abort();
    }
}

// キーの全対応幅で、端の値・負数・範囲外・通常更新・コピーとmoveを確認する
// 64bit/128bitキーのコンパイル拒否は、別途コンパイル試験で検証する
template <class Key>
void check_integer_type() {
    constexpr int range = static_cast<int>(std::min<std::size_t>(
        4097, static_cast<std::size_t>(std::numeric_limits<Key>::max()) + 1));
    const Key last = static_cast<Key>(range - 1);
    const Key maximum = std::numeric_limits<Key>::max();
    fenwick_set<Key> set(static_cast<std::size_t>(range));
    fenwick_map<Key, std::uint64_t> map(static_cast<std::size_t>(range));
    std::set<Key> reference;
    std::map<Key, std::uint64_t> reference_map;
    std::mt19937 rng(482719 + static_cast<unsigned>(sizeof(Key)) + std::is_signed_v<Key>);

    // 小さい型は表現可能な最大キーまで、それ以上は実際に確保した値域の端まで扱う
    auto verify = [&] {
        std::vector<Key> actual;
        set.for_each([&](const Key& key) { actual.push_back(key); });
        require(actual == std::vector<Key>(reference.begin(), reference.end()), "set ordered values");
        std::map<Key, std::uint64_t> actual_map;
        map.for_each([&](const Key& key, const std::uint64_t& value) { actual_map.emplace(key, value); });
        require(actual_map == reference_map, "map ordered values");
        require(set.size() == map.size() && set.empty() == reference.empty(), "size and empty");
        for (Key key : {Key{0}, Key{1}, last, maximum}) {
            const auto lower = reference.lower_bound(key), upper = reference.upper_bound(key);
            const auto before = static_cast<std::size_t>(std::distance(reference.begin(), lower));
            const auto through = static_cast<std::size_t>(std::distance(reference.begin(), upper));
            require(set.rank(key) == before && map.rank(key) == before, "rank at boundary");
            require(set.rank2(key) == through && map.rank2(key) == through, "rank2 at boundary");
            require(set.contains(key) == reference.contains(key) && map.contains(key) == reference.contains(key),
                    "contains at boundary");
            const std::optional<Key> previous = upper == reference.begin() ? std::nullopt : std::optional<Key>(*std::prev(upper));
            const std::optional<Key> next = lower == reference.end() ? std::nullopt : std::optional<Key>(*lower);
            require(set.less_equal(key) == previous && set.greater_equal(key) == next, "set bounds");
            const auto map_previous = map.less_equal(key), map_next = map.greater_equal(key);
            require(bool(map_previous) == bool(previous) && bool(map_next) == bool(next), "map bounds presence");
            if (previous) require(map_previous->first == *previous, "map previous key");
            if (next) require(map_next->first == *next, "map next key");
        }
        if (!reference.empty()) {
            require(set.min() == *reference.begin() && set.max() == *reference.rbegin(), "set min max");
            require(map.min_key() == *reference.begin() && map.max_key() == *reference.rbegin(), "map min max");
            const auto index = static_cast<std::size_t>(rng()) % set.size();
            const Key key = *std::next(reference.begin(), static_cast<std::ptrdiff_t>(index));
            require(set[index] == key && map.kth(index) == std::pair<Key, std::uint64_t>{key, reference_map.at(key)}, "kth");
        }
    };
    for (Key key : {Key{0}, Key{1}, last, maximum}) {
        set.insert(key); map.insert(key, 17);
        if (static_cast<std::size_t>(key) < static_cast<std::size_t>(range)) {
            reference.insert(key); reference_map.emplace(key, 17);
        }
    }
    if constexpr (std::is_signed_v<Key>) {
        for (Key key : {Key{-1}, std::numeric_limits<Key>::min()}) {
            set.insert(key); map.insert_or_assign(key, 23);
            set.erase(key); map.erase(key);
            require(!set.contains(key) && !map.find(key), "negative ignored");
            require(set.rank(key) == 0 && map.rank2(key) == 0, "negative rank");
        }
    }
    verify();

    // サイズ制限ではなく、各型で同じ操作数を標準コンテナと比較する
    for (int operation = 0; operation < 10000; ++operation) {
        const Key key = static_cast<Key>(rng() % static_cast<unsigned>(range));
        const auto value = static_cast<std::uint64_t>(rng()) << 32 | rng();
        switch (rng() % 4) {
        case 0:
            set.insert(key); map.insert(key, value);
            reference.insert(key); reference_map.emplace(key, value);
            break;
        case 1:
            set.insert(key); map.insert_or_assign(key, value);
            reference.insert(key); reference_map[key] = value;
            break;
        default:
            set.erase(key); map.erase(key);
            reference.erase(key); reference_map.erase(key);
            break;
        }
        if (operation % 211 == 0) verify();
    }
    verify();
    auto set_copy = set;
    auto map_copy = map;
    set.clear(); map.clear();
    set = std::move(set_copy); map = std::move(map_copy);
    require(set_copy.empty() && map_copy.empty(), "moved-from empty");
    verify();
}

void run() {
    // 既定型を変更しても空構築・明示値域・型推論でそのまま使える
    static_assert(std::is_same_v<fenwick_set<>, fenwick_set<std::uint32_t>>);
    fenwick_set<> empty;
    fenwick_set deduced(130);
    static_assert(std::is_same_v<decltype(deduced), fenwick_set<std::uint32_t>>);
    static_assert(std::is_same_v<decltype(deduced.min()), std::optional<std::uint32_t>>);
    require(empty.empty(), "default empty");
    deduced.insert(129);
    require(deduced.min() == 129 && deduced.rank2(UINT32_MAX) == 1, "default operations");
    check_integer_type<std::int8_t>();
    check_integer_type<std::uint8_t>();
    check_integer_type<std::int16_t>();
    check_integer_type<std::uint16_t>();
    check_integer_type<std::int32_t>();
    check_integer_type<std::uint32_t>();

    // キー型の制限をvalueには適用しない。64bit/128bit値の上位bitもコピー後に保存する
    fenwick_map<int, std::uint64_t> values64(65, {{64, UINT64_MAX}});
    require(values64.kth(0).second == UINT64_MAX, "64bit value unchanged");
    const __uint128_t large = (__uint128_t{1} << 100) + 73;
    fenwick_map<int, __uint128_t> values128(65);
    values128.insert(64, large);
    auto copy128 = values128;
    copy128[0] = large + 1;
    require(*values128.find(64) == large && copy128.min().second == large + 1, "128bit value unchanged");
    values128.clear();
    require(values128.empty() && copy128.size() == 2, "128bit clear and copy");
    std::cout << "key_types_default_i8_u8_i16_u16_i32_u32_60000_ops_and_wide_values: ok\n";
}

} // namespace fenwick_key_type_self_test

namespace fenwick_simplified_safety_self_test {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "simplified_safety: " << message << '\n';
        std::abort();
    }
}

template <class Key>
void check_ranges() {
    // 符号付き最小値を unsigned へ変換しても、範囲内の座標へ折り返さない
    const std::vector<Key> probes{
        std::numeric_limits<Key>::min(), Key{0}, Key{1}, static_cast<Key>(63),
        static_cast<Key>(64), static_cast<Key>(65), std::numeric_limits<Key>::max()};
    for (std::size_t range : {0U, 1U, 63U, 64U, 65U, 130U, 259U}) {
        fenwick_set<Key> set(range);
        std::set<Key> reference;
        for (Key key : probes) {
            const auto value = static_cast<std::int64_t>(key);
            if (value >= 0 && static_cast<std::uint64_t>(value) < range) reference.insert(key);
            set.insert(key);
            set.insert(key);
        }
        require(set.size() == reference.size(), "set valid coordinate filtering");
        for (Key key : probes) require(set.contains(key) == reference.contains(key), "set contains boundary");

        // 標準のメンバコピーに戻しても、独立性・自己代入・同容量再利用を維持する
        auto copy = set;
        auto* alias = &copy;
        copy = *alias;
        set.clear();
        set = copy;
        copy.erase(Key{0});
        require(set.size() == reference.size(), "set copy independence");
        for (Key key : probes) { set.erase(key); reference.erase(key); }
        require(set.empty() && reference.empty(), "set all erased");

        if constexpr (!std::is_same_v<Key, bool>) {
            fenwick_map<Key, std::string> source(range), target(range);
            std::map<Key, std::string> expected;
            for (Key key : probes) {
                const auto value = static_cast<std::int64_t>(key);
                source.insert(key, "source");
                target.insert_or_assign(key, "target");
                if (value >= 0 && static_cast<std::uint64_t>(value) < range) expected.emplace(key, "source");
            }
            target.erase(Key{0});
            target = source;
            require(target.size() == expected.size(), "same-range map copy size");
            for (Key key : probes) {
                const auto it = expected.find(key);
                const auto* value = target.find(key);
                require((value != nullptr) == (it != expected.end()), "map contains boundary");
                if (value) require(*value == it->second, "map copied value");
            }
            if (!expected.empty()) {
                require(target.min_key() == expected.begin()->first, "direct min key");
                require(std::as_const(target).max_key() == expected.rbegin()->first, "direct max key");
            }
            source.clear();
            target = source;
            require(target.empty(), "same-range map copy to empty");
        }
    }
}

void run() {
    using Set = fenwick_set<int>;
    using Map = fenwick_map<int, std::string>;
    static_assert(std::is_nothrow_copy_constructible_v<Set>);
    static_assert(std::is_nothrow_copy_assignable_v<Set>);
    static_assert(noexcept(std::declval<Map&>().erase(0)));
    static_assert(noexcept(std::declval<Map&>()[0]));
    static_assert(noexcept(std::declval<const Map&>().kth(0)));
    check_ranges<bool>();
    check_ranges<char>();
    check_ranges<std::int8_t>(); check_ranges<std::uint8_t>();
    check_ranges<std::int16_t>(); check_ranges<std::uint16_t>();
    check_ranges<std::int32_t>(); check_ranges<std::uint32_t>();
#ifndef NDEBUG
    // map の端キー取得は非空を前提とし、debug 時だけ明示的に検査する
    for (std::size_t range : {0U, 130U}) {
        const Map empty(range);
        fenwick_failure_self_test::expect_failure([&] { (void)empty.min_key(); }, "!empty()");
        fenwick_failure_self_test::expect_failure([&] { (void)empty.max_key(); }, "!empty()");
    }
#endif
    std::cout << "simplified_safety_integer_ranges_default_copy_and_minmax_keys: ok\n";
}

} // namespace fenwick_simplified_safety_self_test

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--copy-bench-only") {
        fenwick_raw_copy_benchmark::run();
        return 0;
    }
    if (argc > 1 && std::string_view(argv[1]) == "--workload-only") {
        fenwick_workload_benchmark::run();
        return 0;
    }
    using namespace fenwick_set_self_test;
    run_all_tests();
    std::cout << "edge_empty_singleton_clear: ok\n";
    std::cout << "edge_constructor_duplicates_boundaries: ok\n";
    std::cout << "edge_iteration_callbacks: ok\n";
    std::cout << "deterministic_update_edges: ok\n";
    std::cout << "random_full_check_100000_ops: ok\n";
    std::cout << "random_default_range_sampled_300000_ops: ok\n";
    std::cout << "all tests passed\n";
    fenwick_map_key_test::run();
    fenwick_map_self_test::run();
    fenwick_word_self_test::run();
    fenwick_raw_values_extra_test::run();
    fenwick_raw_copy_regression_test::run();
    fenwick_review_self_test::run();
    fenwick_review_v11_self_test::run();
    fenwick_key_type_self_test::run();
    fenwick_simplified_safety_self_test::run();
#ifdef FENWICK_TEST_ALLOCATION_FAILURE
    fenwick_allocation_self_test::run();
#endif
    std::cout << "failure_boundary_cases=" << fenwick_failure_self_test::failure_cases
              << " terminate=" << fenwick_failure_self_test::terminate_cases
              << " assert=" << fenwick_failure_self_test::assertion_cases << '\n';
    std::cout << "all set/map tests passed\n";
    if (argc > 1 && std::string_view(argv[1]) == "--test-only") return 0;
    auto rows = fenwick_set_self_benchmark::run_benchmarks();
    fenwick_set_self_benchmark::print_benchmark_table(rows);
    fenwick_map_key_benchmark::run();
    fenwick_map_self_benchmark::run();
    fenwick_workload_benchmark::run();
    fenwick_raw_copy_benchmark::run();
    std::cout << "fenwick_full_run_complete\n";
    return 0;
}
#endif
