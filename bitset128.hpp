// ※64bit超128以下でシフト演算を多用する場合に速い。and, xorなどの複合演算は遅くなる

// C++20 (GCC 12.2) - fast fixed-size bitset up to 128 bits using unsigned __int128.
// Interface: close to std::bitset (plus a small extension: to_u128()).
// Tests: rich debug, continue after failures, report at the end.
// Benchmarks: multiple use-cases, compare against std::bitset.
// NOTE (requested change):
//   - all() now compares against a fixed static constexpr mask (kMask) directly
//     to encourage simplest possible compare codegen.

#include <cstddef>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <limits>
#include <ostream>
#include <istream>

using namespace std;

template <size_t N>
class bitset128 {
    static_assert(N >= 1 && N <= 128, "bitset128<N>: N must be in [1, 128].");

public:
    using u128 = unsigned __int128;

    class reference {
        friend class bitset128;
        bitset128* bs_ = nullptr;
        size_t pos_ = 0;

        constexpr reference(bitset128& bs, size_t pos) noexcept : bs_(&bs), pos_(pos) {}

    public:
        reference() = delete;

        constexpr reference& operator=(bool v) noexcept {
            bs_->set_unchecked(pos_, v);
            return *this;
        }
        constexpr reference& operator=(const reference& r) noexcept { return (*this = static_cast<bool>(r)); }

        constexpr operator bool() const noexcept { return bs_->get_unchecked(pos_); }
        constexpr bool operator~() const noexcept { return !bs_->get_unchecked(pos_); }

        constexpr reference& flip() noexcept {
            bs_->flip_unchecked(pos_);
            return *this;
        }
    };

private:
    alignas(16) u128 bits_ = 0;

    // ---- Fixed static mask (requested) ----
    static consteval u128 make_mask() noexcept {
        if constexpr (N == 128) return ~u128(0);
        else return (u128(1) << N) - 1;
    }
    static constexpr u128 kMask = make_mask();

    static constexpr u128 one_at(size_t pos) noexcept { return (u128(1) << pos); }

    constexpr void sanitize() noexcept { bits_ &= kMask; }

    constexpr bool get_unchecked(size_t pos) const noexcept { return (bits_ >> pos) & 1; }

    constexpr void set_unchecked(size_t pos, bool v) noexcept {
        const u128 m = one_at(pos);
        bits_ = v ? (bits_ | m) : (bits_ & ~m);
    }

    constexpr void flip_unchecked(size_t pos) noexcept { bits_ ^= one_at(pos); }

    static inline size_t popcount_u128(u128 v) noexcept {
        const unsigned long long lo = static_cast<unsigned long long>(v);
        const unsigned long long hi = static_cast<unsigned long long>(v >> 64);
        return static_cast<size_t>(__builtin_popcountll(lo) + __builtin_popcountll(hi));
    }

    static inline void throw_oob(const char* what) { throw out_of_range(what); }

public:
    // --- constructors ---
    constexpr bitset128() noexcept = default;

    // Like std::bitset: low bits are taken, higher bits truncated.
    constexpr bitset128(unsigned long long v) noexcept : bits_(static_cast<u128>(v)) { sanitize(); }

    // GCC extension convenience (still fully standard C++ as a type, but non-std API).
    constexpr explicit bitset128(u128 v) noexcept : bits_(v) { sanitize(); }

    // std::bitset-like string constructor:
    // - pos > str.size(): out_of_range
    // - invalid character: invalid_argument
    // - reads substring of length rlen = min(n, str.size()-pos)
    // - maps last char of substring to bit 0, first to bit (rlen-1)
    explicit bitset128(const string& str,
                       size_t pos = 0,
                       size_t n = string::npos,
                       char zero = '0',
                       char one = '1')
        : bits_(0) {
        if (pos > str.size()) throw out_of_range("bitset128::bitset128(string): pos > size");
        const size_t avail = str.size() - pos;
        const size_t rlen = (n == string::npos) ? avail : (n < avail ? n : avail);
        const size_t use = (rlen < N) ? rlen : N;

        // take rightmost `use` chars from the selected substring
        for (size_t i = 0; i < use; ++i) {
            const char c = str[pos + (rlen - 1 - i)];
            if (c == one) bits_ |= one_at(i);
            else if (c == zero) {
                // keep 0
            } else {
                throw invalid_argument("bitset128::bitset128(string): invalid character");
            }
        }
        sanitize();
    }

    // --- basic ops ---
    [[nodiscard]] static constexpr size_t size() noexcept { return N; }

    // Extension: fast access to underlying masked value.
    [[nodiscard]] constexpr u128 to_u128() const noexcept { return bits_; }

    // Like std::bitset: unchecked (UB if pos >= N).
    [[nodiscard]] constexpr bool operator[](size_t pos) const noexcept { return get_unchecked(pos); }
    [[nodiscard]] constexpr reference operator[](size_t pos) noexcept { return reference(*this, pos); }

    [[nodiscard]] bool test(size_t pos) const {
        if (pos >= N) throw_oob("bitset128::test");
        return get_unchecked(pos);
    }

    bitset128& set() noexcept {
        bits_ = kMask;
        return *this;
    }

    bitset128& set(size_t pos, bool value = true) {
        if (pos >= N) throw_oob("bitset128::set");
        set_unchecked(pos, value);
        return *this;
    }

    bitset128& reset() noexcept {
        bits_ = 0;
        return *this;
    }

    bitset128& reset(size_t pos) {
        if (pos >= N) throw_oob("bitset128::reset");
        set_unchecked(pos, false);
        return *this;
    }

    bitset128& flip() noexcept {
        bits_ = (~bits_) & kMask;
        return *this;
    }

    bitset128& flip(size_t pos) {
        if (pos >= N) throw_oob("bitset128::flip");
        flip_unchecked(pos);
        return *this;
    }

    [[nodiscard]] size_t count() const noexcept {
        if constexpr (N <= 64) {
            return static_cast<size_t>(__builtin_popcountll(static_cast<unsigned long long>(bits_)));
        } else {
            return popcount_u128(bits_);
        }
    }

    [[nodiscard]] constexpr bool any() const noexcept { return bits_ != 0; }
    [[nodiscard]] constexpr bool none() const noexcept { return bits_ == 0; }

    // ---- Requested: fixed constant compare (no helper call) ----
    [[nodiscard]] constexpr bool all() const noexcept { return bits_ == kMask; }

    unsigned long to_ulong() const {
        constexpr int D = numeric_limits<unsigned long>::digits;
        if (bits_ >> D) throw overflow_error("bitset128::to_ulong");
        return static_cast<unsigned long>(bits_);
    }

    unsigned long long to_ullong() const {
        constexpr int D = numeric_limits<unsigned long long>::digits; // usually 64
        if (bits_ >> D) throw overflow_error("bitset128::to_ullong");
        return static_cast<unsigned long long>(bits_);
    }

    [[nodiscard]] string to_string(char zero = '0', char one = '1') const {
        string s;
        s.resize(N);
        // std::bitset prints from high index to low index.
        for (size_t i = 0; i < N; ++i) {
            const size_t bit = (N - 1 - i);
            s[i] = get_unchecked(bit) ? one : zero;
        }
        return s;
    }

    template <class F>
    __attribute__((always_inline)) inline void for_each(F&& f) {
        using u64  = unsigned long long;
        u64 lo = (u64)bits_;
        u64 hi = (u64)(bits_ >> 64);

        // 下位64bit
        while (lo) {
            unsigned i = (unsigned)__builtin_ctzll(lo);
            f((int)i);
            lo &= (lo - 1); // 末尾の1を落とす
        }
        // 上位64bit
        while (hi) {
            unsigned i = (unsigned)__builtin_ctzll(hi);
            f((int)i + 64);
            hi &= (hi - 1);
        }
    }

    // --- bitwise ops ---
    bitset128& operator&=(const bitset128& r) noexcept {
        bits_ &= r.bits_;
        return *this;
    }

    bitset128& operator|=(const bitset128& r) noexcept {
        bits_ |= r.bits_;
        sanitize();
        return *this;
    }

    bitset128& operator^=(const bitset128& r) noexcept {
        bits_ ^= r.bits_;
        sanitize();
        return *this;
    }

    bitset128& operator<<=(size_t pos) noexcept {
        if (pos >= N) bits_ = 0;
        else {
            bits_ <<= pos;
            sanitize();
        }
        return *this;
    }

    bitset128& operator>>=(size_t pos) noexcept {
        if (pos >= N) bits_ = 0;
        else bits_ >>= pos;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const bitset128& r) const noexcept { return bits_ == r.bits_; }
    [[nodiscard]] constexpr bool operator!=(const bitset128& r) const noexcept { return bits_ != r.bits_; }

    // --- friends (non-members like std::bitset) ---
    friend bitset128 operator~(bitset128 v) noexcept { return v.flip(); }

    friend bitset128 operator&(bitset128 a, const bitset128& b) noexcept { return a &= b; }
    friend bitset128 operator|(bitset128 a, const bitset128& b) noexcept { return a |= b; }
    friend bitset128 operator^(bitset128 a, const bitset128& b) noexcept { return a ^= b; }

    friend bitset128 operator<<(bitset128 a, size_t pos) noexcept { return a <<= pos; }
    friend bitset128 operator>>(bitset128 a, size_t pos) noexcept { return a >>= pos; }

    friend ostream& operator<<(ostream& os, const bitset128& b) { return os << b.to_string(); }

    friend istream& operator>>(istream& is, bitset128& b) {
        string tok;
        is >> tok;
        if (!is) return is;

        // Strict behavior: require exactly N bits (easy to validate & fast).
        if (tok.size() != N) {
            is.setstate(ios::failbit);
            return is;
        }

        try {
            b = bitset128(tok);
        } catch (...) {
            is.setstate(ios::failbit);
        }
        return is;
    }
};

#if 1
// ---- tests / benchmark only ----
#include <iostream>
#include <bitset>
#include <random>
#include <chrono>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cmath>

using namespace std;

struct TestContext {
    long long checks = 0;
    long long failed = 0;
    int printed = 0;
    int print_limit = 120; // avoid flooding

    void note_fail_header(const char* file, int line) { cerr << "[FAIL] " << file << ":" << line << "\n"; }

    void check(bool cond, const char* expr, const char* file, int line, const string& msg = {}) {
        ++checks;
        if (cond) return;
        ++failed;
        if (printed < print_limit) {
            note_fail_header(file, line);
            cerr << "       expr: " << expr << "\n";
            if (!msg.empty()) cerr << "       msg : " << msg << "\n";
            ++printed;
            if (printed == print_limit) {
                cerr << "[NOTE] failure print limit reached; further failures will be counted but not printed.\n";
            }
        }
    }

    template <class Fn>
    void check_noexcept(Fn&& fn, const char* expr, const char* file, int line, const string& msg = {}) {
        try {
            check(static_cast<bool>(fn()), expr, file, line, msg);
        } catch (const exception& e) {
            check(false, expr, file, line, string("threw exception: ") + e.what());
        } catch (...) {
            check(false, expr, file, line, "threw unknown exception");
        }
    }
};

#define CHECK(ctx, cond) (ctx).check_noexcept([&]() { return (cond); }, #cond, __FILE__, __LINE__)
#define CHECK_MSG(ctx, cond, msg) (ctx).check_noexcept([&]() { return (cond); }, #cond, __FILE__, __LINE__, (msg))

#define CHECK_THROWS(ctx, expr, ex_type)                                                         \
    do {                                                                                          \
        bool _threw = false;                                                                      \
        try {                                                                                     \
            (void)(expr);                                                                         \
        } catch (const ex_type&) {                                                                \
            _threw = true;                                                                        \
        } catch (const exception& e) {                                                            \
            (ctx).check(false, #expr, __FILE__, __LINE__, string("threw other std::exception: ") + e.what()); \
            break;                                                                                \
        } catch (...) {                                                                           \
            (ctx).check(false, #expr, __FILE__, __LINE__, "threw other non-std exception");       \
            break;                                                                                \
        }                                                                                         \
        (ctx).check(_threw, #expr " throws " #ex_type, __FILE__, __LINE__);                       \
    } while (0)

static inline unsigned __int128 rand_u128(mt19937_64& rng) {
    const unsigned __int128 lo = static_cast<unsigned long long>(rng());
    const unsigned __int128 hi = static_cast<unsigned long long>(rng());
    return lo | (hi << 64);
}

template <size_t N>
static constexpr unsigned __int128 mask_u128() {
    if constexpr (N == 128) return ~static_cast<unsigned __int128>(0);
    else return (static_cast<unsigned __int128>(1) << N) - 1;
}

template <size_t N>
static bitset<N> std_from_u128(unsigned __int128 v) {
    bitset<N> b;
    for (size_t i = 0; i < N; ++i) b[i] = static_cast<bool>((v >> i) & 1);
    return b;
}

template <size_t N>
static unsigned __int128 u128_from_std(const bitset<N>& b) {
    unsigned __int128 v = 0;
    for (size_t i = 0; i < N; ++i) {
        if (b[i]) v |= (static_cast<unsigned __int128>(1) << i);
    }
    return v;
}

template <size_t N>
static string hex_bits_u128(unsigned __int128 v) {
    v &= mask_u128<N>();
    const unsigned long long lo = static_cast<unsigned long long>(v);
    const unsigned long long hi = static_cast<unsigned long long>(v >> 64);
    const int digits = static_cast<int>((N + 3) / 4);

    ostringstream oss;
    oss << "0x" << hex << uppercase << setfill('0');
    if (digits <= 16) {
        oss << setw(digits) << lo;
    } else {
        oss << setw(digits - 16) << hi << setw(16) << lo;
    }
    return oss.str();
}

template <size_t N>
static void check_bits_eq_impl(TestContext& ctx,
                               const bitset128<N>& a,
                               const bitset<N>& b,
                               const char* expr,
                               const char* file,
                               int line,
                               const string& label) {
    ++ctx.checks;
    const auto va = a.to_u128();
    const auto vb = u128_from_std(b);
    if (va == vb) return;

    ++ctx.failed;
    if (ctx.printed < ctx.print_limit) {
        ctx.note_fail_header(file, line);
        cerr << "       expr: " << expr << "\n";
        if (!label.empty()) cerr << "       case: " << label << "\n";
        cerr << "       N   : " << N << "\n";
        cerr << "       our : " << a.to_string() << "  (" << hex_bits_u128<N>(va) << ")  count=" << a.count()
             << " any=" << a.any() << " all=" << a.all() << " none=" << a.none() << "\n";
        cerr << "       std : " << b.to_string() << "  (" << hex_bits_u128<N>(vb) << ")  count=" << b.count()
             << " any=" << b.any() << " all=" << b.all() << " none=" << b.none() << "\n";

        const auto diff = va ^ vb;
        const auto diff_bs = std_from_u128<N>(diff);
        cerr << "       xor : " << diff_bs.to_string() << "  (" << hex_bits_u128<N>(diff) << ")\n";

        ++ctx.printed;
        if (ctx.printed == ctx.print_limit) {
            cerr << "[NOTE] failure print limit reached; further failures will be counted but not printed.\n";
        }
    }
}

#define CHECK_BITS_EQ(ctx, a, b, label) \
    check_bits_eq_impl((ctx), (a), (b), #a " == " #b, __FILE__, __LINE__, (label))

// --- edge tests ---

template <size_t N>
static void edge_tests(TestContext& ctx) {
    using BS = bitset128<N>;

    // default
    {
        BS b;
        CHECK(ctx, b.size() == N);
        CHECK(ctx, b.none());
        CHECK(ctx, !b.any());
        CHECK(ctx, !b.all());
        CHECK(ctx, b.count() == 0);
        CHECK(ctx, b.to_string() == string(N, '0'));
        CHECK(ctx, b.to_u128() == 0);
    }

    // set/reset/flip (all)
    {
        BS b;
        b.set();
        CHECK(ctx, b.all());
        CHECK(ctx, b.any());
        CHECK(ctx, !b.none());
        CHECK(ctx, b.count() == N);
        CHECK(ctx, b.to_string() == string(N, '1'));

        b.reset();
        CHECK(ctx, b.none());
        CHECK(ctx, b.count() == 0);

        b.flip();
        CHECK(ctx, b.all());
        b.flip();
        CHECK(ctx, b.none());
    }

    // set/reset/flip (pos) + bounds checked methods throw
    {
        BS b;
        b.reset();

        b.set(0);
        CHECK(ctx, b.test(0));

        b.set(N - 1);
        CHECK(ctx, b.test(N - 1));

        const size_t expected = (N == 1) ? 1 : 2;
        CHECK(ctx, b.count() == expected);

        // reset bit 0
        b.reset(0);
        CHECK(ctx, !b.test(0));

        // ensure last bit is 1, then flip it to 0 (works also for N==1)
        b.set(N - 1, true);
        CHECK(ctx, b.test(N - 1));
        b.flip(N - 1);
        CHECK(ctx, !b.test(N - 1));

        // bounds
        CHECK_THROWS(ctx, b.test(N), out_of_range);
        CHECK_THROWS(ctx, b.set(N), out_of_range);
        CHECK_THROWS(ctx, b.reset(N), out_of_range);
        CHECK_THROWS(ctx, b.flip(N), out_of_range);
    }

    // operator[] proxy (unchecked like std::bitset)
    {
        BS b;
        b.reset();

        b[0] = true;
        CHECK(ctx, b.test(0));

        b[0] = b[0];
        CHECK(ctx, b.test(0));

        b[0].flip();
        CHECK(ctx, !b.test(0));

        bool v = b[0];
        CHECK(ctx, v == false);

        bool inv = ~b[0];
        CHECK(ctx, inv == true);

        // last bit
        b[N - 1] = true;
        CHECK(ctx, b.test(N - 1));
        b[N - 1].flip();
        CHECK(ctx, !b.test(N - 1));
    }

    // shift edges
    {
        // pattern: only LSB on
        BS b;
        b.reset();
        b.set(0);

        BS x = (b << 0);
        CHECK(ctx, x.test(0));

        x = (b << 1);
        if constexpr (N >= 2) CHECK(ctx, x.test(1));
        else CHECK(ctx, x.none());

        x = (b << (N - 1));
        CHECK(ctx, x.test(N - 1));

        x = (b << N);
        CHECK(ctx, x.none());

        x = (b >> 0);
        CHECK(ctx, x.test(0));

        x = (b >> 1);
        CHECK(ctx, x.none());

        x = (b >> N);
        CHECK(ctx, x.none());
    }

    // to_ulong / to_ullong
    {
        BS b;
        b.reset();
        b.set(0);
        CHECK(ctx, b.to_ulong() == 1UL);
        CHECK(ctx, b.to_ullong() == 1ULL);

        // overflow for to_ullong if possible
        if constexpr (N > numeric_limits<unsigned long long>::digits) {
            b.reset();
            b.set(numeric_limits<unsigned long long>::digits); // bit 64 if ull is 64-bit
            CHECK_THROWS(ctx, b.to_ullong(), overflow_error);
        }
    }

    // string ctor + to_string + pos/n + custom zero/one + exceptions
    {
        BS b;
        b.reset();
        b.set(); // all ones
        const string s = b.to_string();
        BS b2(s);
        CHECK(ctx, b2 == b);

        const string wrapped = string("xx") + s + string("yy");
        BS b3(wrapped, 2, s.size());
        CHECK(ctx, b3 == b);

        // custom alphabet
        string t = s;
        for (char& c : t) c = (c == '0') ? 'a' : 'b';
        BS b4(t, 0, string::npos, 'a', 'b');
        CHECK(ctx, b4 == b);

        // invalid_argument
        CHECK_THROWS(ctx, BS(string("x")), invalid_argument);

        // out_of_range
        CHECK_THROWS(ctx, BS(string("101"), 999), out_of_range);
    }

    // stream << >>
    {
        BS b;
        b.reset();
        b.set(N - 1);

        stringstream ss;
        ss << b;
        CHECK(ctx, ss.str() == b.to_string());

        BS c;
        ss.seekg(0);
        ss >> c;
        CHECK(ctx, c == b);

        // bad input length -> failbit (except N==3 where "101" is valid length)
        {
            stringstream ss2;
            ss2 << "101";
            BS d;
            ss2 >> d;
            if constexpr (N != 3) CHECK(ctx, ss2.fail());
        }
    }

    // masking behavior for N < 128 via u128 ctor
    {
        if constexpr (N < 128) {
            const unsigned __int128 v = (static_cast<unsigned __int128>(1) << N); // bit N is outside
            BS b(static_cast<typename BS::u128>(v));
            CHECK(ctx, b.none());
        }
    }
}

template <size_t N>
static void random_consistency_tests(TestContext& ctx, mt19937_64& rng, size_t cases) {
    using BS = bitset128<N>;
    constexpr unsigned __int128 M = mask_u128<N>();

    for (size_t t = 0; t < cases; ++t) {
        const unsigned __int128 va = rand_u128(rng) & M;
        const unsigned __int128 vb = rand_u128(rng) & M;

        BS a(static_cast<typename BS::u128>(va)), b(static_cast<typename BS::u128>(vb));
        bitset<N> sa = std_from_u128<N>(va);
        bitset<N> sb = std_from_u128<N>(vb);

        const string label = string("N=") + to_string(N) + " case=" + to_string(t);

        // basic queries
        CHECK(ctx, a.size() == N);
        CHECK(ctx, a.count() == sa.count());
        CHECK(ctx, a.any() == sa.any());
        CHECK(ctx, a.none() == sa.none());
        CHECK(ctx, a.all() == sa.all());
        CHECK(ctx, a.to_u128() == (va & M));

        // to_string roundtrip (occasionally)
        if ((t & 63) == 0) {
            const string s = a.to_string();
            CHECK_MSG(ctx, s == sa.to_string(), label + " to_string mismatch");
            BS rt(s);
            CHECK(ctx, rt == a);

            // custom alphabet roundtrip
            string u = s;
            for (char& c : u) c = (c == '0') ? 'x' : 'y';
            BS rt2(u, 0, string::npos, 'x', 'y');
            CHECK(ctx, rt2 == a);
        }

        // test()/operator[]
        {
            const size_t pos = static_cast<size_t>(rng() % N);
            CHECK(ctx, a.test(pos) == sa.test(pos));
            CHECK(ctx, static_cast<bool>(a[pos]) == static_cast<bool>(sa[pos]));

            BS x = a;
            bitset<N> sx = sa;

            const bool v = (rng() & 1) != 0;
            x.set(pos, v);
            sx.set(pos, v);
            CHECK_BITS_EQ(ctx, x, sx, label + " set(pos,val)");

            x.reset(pos);
            sx.reset(pos);
            CHECK_BITS_EQ(ctx, x, sx, label + " reset(pos)");

            x.flip(pos);
            sx.flip(pos);
            CHECK_BITS_EQ(ctx, x, sx, label + " flip(pos)");

            // proxy assign + flip
            x[pos] = v;
            sx[pos] = v;
            CHECK_BITS_EQ(ctx, x, sx, label + " proxy assign");

            x[pos].flip();
            sx.flip(pos);
            CHECK_BITS_EQ(ctx, x, sx, label + " proxy flip");
        }

        // set/reset/flip all
        {
            BS x = a;
            bitset<N> sx = sa;
            x.set();
            sx.set();
            CHECK_BITS_EQ(ctx, x, sx, label + " set()");

            x.reset();
            sx.reset();
            CHECK_BITS_EQ(ctx, x, sx, label + " reset()");

            x.flip();
            sx.flip();
            CHECK_BITS_EQ(ctx, x, sx, label + " flip()");
        }

        // bitwise
        {
            CHECK_BITS_EQ(ctx, (a & b), (sa & sb), label + " (a&b)");
            CHECK_BITS_EQ(ctx, (a | b), (sa | sb), label + " (a|b)");
            CHECK_BITS_EQ(ctx, (a ^ b), (sa ^ sb), label + " (a^b)");
            CHECK_BITS_EQ(ctx, (~a), (~sa), label + " (~a)");

            BS x = a;
            bitset<N> sx = sa;
            x &= b;
            sx &= sb;
            CHECK_BITS_EQ(ctx, x, sx, label + " &=");

            x = a;
            sx = sa;
            x |= b;
            sx |= sb;
            CHECK_BITS_EQ(ctx, x, sx, label + " |=");

            x = a;
            sx = sa;
            x ^= b;
            sx ^= sb;
            CHECK_BITS_EQ(ctx, x, sx, label + " ^=");
        }

        // shifts (include >=N)
        {
            const size_t sh = static_cast<size_t>(rng() % (N + 20));
            CHECK_BITS_EQ(ctx, (a << sh), (sa << sh), label + " (a<<sh)");
            CHECK_BITS_EQ(ctx, (a >> sh), (sa >> sh), label + " (a>>sh)");

            BS x = a;
            bitset<N> sx = sa;
            x <<= sh;
            sx <<= sh;
            CHECK_BITS_EQ(ctx, x, sx, label + " <<=");

            x = a;
            sx = sa;
            x >>= sh;
            sx >>= sh;
            CHECK_BITS_EQ(ctx, x, sx, label + " >>=");
        }

        // comparisons
        CHECK(ctx, (a == b) == (sa == sb));
        CHECK(ctx, (a != b) == (sa != sb));

        // to_ulong / to_ullong consistency with std::bitset
        {
            bool a_ok = true, s_ok = true;
            unsigned long ax = 0, sx = 0;
            try { ax = a.to_ulong(); } catch (const overflow_error&) { a_ok = false; }
            try { sx = sa.to_ulong(); } catch (const overflow_error&) { s_ok = false; }
            CHECK_MSG(ctx, a_ok == s_ok, label + " to_ulong throw mismatch");
            if (a_ok) CHECK_MSG(ctx, ax == sx, label + " to_ulong value mismatch");
        }
        {
            bool a_ok = true, s_ok = true;
            unsigned long long ax = 0, sx = 0;
            try { ax = a.to_ullong(); } catch (const overflow_error&) { a_ok = false; }
            try { sx = sa.to_ullong(); } catch (const overflow_error&) { s_ok = false; }
            CHECK_MSG(ctx, a_ok == s_ok, label + " to_ullong throw mismatch");
            if (a_ok) CHECK_MSG(ctx, ax == sx, label + " to_ullong value mismatch");
        }
    }
}

// --- Benchmark helpers (compare bitset128 vs std::bitset) ---

struct BenchStats {
    double avg_ms = 0;
    double min_ms = 0;
    double max_ms = 0;
    double sd_ms = 0;
    unsigned long long checksum_xor = 0;
};

static inline double sqr(double x) { return x * x; }

template <size_t N>
static unsigned long long checksum_final(const bitset128<N>& b) {
    const unsigned __int128 v = b.to_u128();
    const unsigned long long lo = static_cast<unsigned long long>(v);
    const unsigned long long hi = static_cast<unsigned long long>(v >> 64);
    return lo ^ (hi + 0x9E3779B97F4A7C15ULL) ^ static_cast<unsigned long long>(b.count());
}

template <size_t N>
static unsigned long long checksum_final(const bitset<N>& b) {
    unsigned long long lo = 0, hi = 0;
    const size_t lo_bits = (N < 64) ? N : 64;
    for (size_t i = 0; i < lo_bits; ++i) if (b[i]) lo |= (1ULL << i);

    if constexpr (N > 64) {
        const size_t hi_bits = ((N - 64) < 64) ? (N - 64) : 64;
        for (size_t i = 0; i < hi_bits; ++i) if (b[i + 64]) hi |= (1ULL << i);
    }
    return lo ^ (hi + 0x9E3779B97F4A7C15ULL) ^ static_cast<unsigned long long>(b.count());
}

template <class Fn>
static BenchStats run_bench(size_t iters, int trials, Fn&& fn) {
    vector<double> times;
    times.reserve(trials);

    unsigned long long checksum_x = 0;
    double sum = 0.0;

    double mn = numeric_limits<double>::infinity();
    double mx = 0.0;

    for (int tr = 0; tr < trials; ++tr) {
        const auto st = chrono::high_resolution_clock::now();
        const unsigned long long c = fn(iters);
        const auto ed = chrono::high_resolution_clock::now();

        const double ms = chrono::duration<double, milli>(ed - st).count();

        times.push_back(ms);
        sum += ms;
        mn = min(mn, ms);
        mx = max(mx, ms);
        checksum_x ^= c;
    }

    const double avg = sum / trials;
    double var = 0.0;
    for (double ms : times) var += sqr(ms - avg);
    var /= trials;
    const double sd = sqrt(var);

    BenchStats s;
    s.avg_ms = avg;
    s.min_ms = mn;
    s.max_ms = mx;
    s.sd_ms = sd;
    s.checksum_xor = checksum_x;
    return s;
}

// ---- use-case benches ----

template <size_t N, class BS>
static unsigned long long bench_mixed_ops(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.reset();
    const size_t mask = data.size() - 1;

    for (size_t i = 0; i < iters; ++i) {
        const BS& x = data[i & mask];
        acc ^= x;
        acc <<= (i & 63);
        acc ^= (x >> ((i * 7) & 63));
        acc.flip((i * 13) & (N - 1));
    }
    return checksum_final<N>(acc);
}

template <size_t N, class BS>
static unsigned long long bench_bitwise_combine(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.set();
    const size_t mask = data.size() - 1;

    for (size_t i = 0; i < iters; ++i) {
        const BS& a = data[i & mask];
        const BS& b = data[(i * 17) & mask];
        acc ^= (a & b);
        acc |= a;
        acc &= ~b;
    }
    return checksum_final<N>(acc);
}

template <size_t N, class BS>
static unsigned long long bench_random_bit_access(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.reset();
    const size_t mask = data.size() - 1;
    unsigned long long sink = 0;

    for (size_t i = 0; i < iters; ++i) {
        const BS& x = data[i & mask];
        acc ^= x;

        const size_t p0 = (i * 1315423911u) & (N - 1);
        const size_t p1 = (i * 2654435761u) & (N - 1);
        const bool v = ((i >> 5) & 1) != 0;

        acc.set(p0, v);
        acc.flip(p1);
        sink += static_cast<unsigned long long>(acc.test(p0));
        sink += static_cast<unsigned long long>(static_cast<bool>(acc[p1]));
    }

    return checksum_final<N>(acc) ^ (sink * 0x9E3779B97F4A7C15ULL);
}

template <size_t N, class BS>
static unsigned long long bench_query_heavy(const vector<BS>& data, size_t iters) {
    unsigned long long sum = 0;
    const size_t mask = data.size() - 1;

    for (size_t i = 0; i < iters; ++i) {
        const BS& x = data[i & mask];
        sum += static_cast<unsigned long long>(x.count());
        sum ^= static_cast<unsigned long long>(x.any());
        sum ^= static_cast<unsigned long long>(x.all()) << 1;
        sum ^= static_cast<unsigned long long>(x.none()) << 2;
    }
    return sum;
}

template <size_t N, class BS>
static unsigned long long bench_shift_heavy(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.reset();
    const size_t mask = data.size() - 1;

    for (size_t i = 0; i < iters; ++i) {
        acc ^= data[i & mask];
        acc <<= ((i * 3) & 63);
        acc >>= ((i * 5) & 63);
        acc.flip((i * 7) & (N - 1));
    }
    return checksum_final<N>(acc);
}

// ---- breakdown benches ----
template <size_t N, class BS>
static unsigned long long bench_and_only(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.set();
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) acc &= data[i & mask];
    return checksum_final<N>(acc);
}
template <size_t N, class BS>
static unsigned long long bench_or_only(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.reset();
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) acc |= data[i & mask];
    return checksum_final<N>(acc);
}
template <size_t N, class BS>
static unsigned long long bench_xor_only(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.reset();
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) acc ^= data[i & mask];
    return checksum_final<N>(acc);
}
template <size_t N, class BS>
static unsigned long long bench_not_assign_only(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.reset();
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) acc = ~data[i & mask];
    return checksum_final<N>(acc);
}
template <size_t N, class BS>
static unsigned long long bench_and_not_only(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.set();
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) acc &= ~data[i & mask];
    return checksum_final<N>(acc);
}
template <size_t N, class BS>
static unsigned long long bench_xor_of_and(const vector<BS>& data, size_t iters) {
    BS acc;
    acc.reset();
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) {
        const BS& a = data[i & mask];
        const BS& b = data[(i * 17) & mask];
        acc ^= (a & b);
    }
    return checksum_final<N>(acc);
}

template <size_t N, class BS>
static unsigned long long bench_count_only_query(const vector<BS>& data, size_t iters) {
    unsigned long long sum = 0;
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) sum += static_cast<unsigned long long>(data[i & mask].count());
    return sum;
}
template <size_t N, class BS>
static unsigned long long bench_any_only_query(const vector<BS>& data, size_t iters) {
    unsigned long long sum = 0;
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) sum += static_cast<unsigned long long>(data[i & mask].any());
    return sum;
}
template <size_t N, class BS>
static unsigned long long bench_all_only_query(const vector<BS>& data, size_t iters) {
    unsigned long long sum = 0;
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) sum += static_cast<unsigned long long>(data[i & mask].all());
    return sum;
}
template <size_t N, class BS>
static unsigned long long bench_none_only_query(const vector<BS>& data, size_t iters) {
    unsigned long long sum = 0;
    const size_t mask = data.size() - 1;
    for (size_t i = 0; i < iters; ++i) sum += static_cast<unsigned long long>(data[i & mask].none());
    return sum;
}

template <size_t N>
static void benchmark_compare(mt19937_64& rng) {
    constexpr unsigned __int128 M = mask_u128<N>();

    const size_t dataset_size = 1u << 12; // 4096
    vector<unsigned __int128> raw(dataset_size);
    for (auto& v : raw) v = rand_u128(rng) & M;

    vector<bitset128<N>> fast(dataset_size);
    vector<bitset<N>> ref(dataset_size);
    for (size_t i = 0; i < dataset_size; ++i) {
        fast[i] = bitset128<N>(static_cast<typename bitset128<N>::u128>(raw[i]));
        ref[i] = std_from_u128<N>(raw[i]);
    }

    auto print_stats = [&](const char* who, const BenchStats& s, size_t iters) {
        const double ns_per_iter = (s.avg_ms * 1e6) / static_cast<double>(iters);
        cout << "  " << left << setw(18) << who << ": " << fixed << setprecision(3) << s.avg_ms << " ms"
             << " (min=" << s.min_ms << ", max=" << s.max_ms << ", sd=" << s.sd_ms << ")"
             << "  [" << setprecision(2) << ns_per_iter << " ns/iter]"
             << "  [checksum_xor=" << s.checksum_xor << "]\n";
    };

    auto run_case = [&](const char* name, size_t iters, int trials,
                        auto&& fast_fn, auto&& ref_fn) {
        cout << "\nUse-case: " << name << "\n";
        cout << "  iters=" << iters << "  trials=" << trials << "\n";

        BenchStats s_fast = run_bench(iters, trials, [&](size_t it) { return fast_fn(it); });
        BenchStats s_ref = run_bench(iters, trials, [&](size_t it) { return ref_fn(it); });

        print_stats("bitset128", s_fast, iters);
        print_stats("std::bitset", s_ref, iters);

        if (s_fast.checksum_xor != s_ref.checksum_xor) {
            cout << "  [WARN] checksum mismatch (fast vs std). This indicates a bug or benchmark divergence.\n";
        }

        const double speedup = (s_fast.avg_ms > 0.0) ? (s_ref.avg_ms / s_fast.avg_ms) : 0.0;
        cout << "  speedup (std/fast) = " << fixed << setprecision(2) << speedup << " x\n";
        return speedup;
    };

    cout << "\n[Benchmark compare N=" << N << "]\n";
    cout << "  dataset_size = " << dataset_size << "\n";

    const double sp_mixed = run_case(
        "MixedOps (xor/shift/flip)",
        5'000'000, 5,
        [&](size_t it) { return bench_mixed_ops<N>(fast, it); },
        [&](size_t it) { return bench_mixed_ops<N>(ref, it); });

    const double sp_bitwise = run_case(
        "BitwiseCombine (and/or/xor/not)",
        5'000'000, 5,
        [&](size_t it) { return bench_bitwise_combine<N>(fast, it); },
        [&](size_t it) { return bench_bitwise_combine<N>(ref, it); });

    const double sp_random = run_case(
        "RandomBitAccess (set/reset/flip/test/[])",
        8'000'000, 5,
        [&](size_t it) { return bench_random_bit_access<N>(fast, it); },
        [&](size_t it) { return bench_random_bit_access<N>(ref, it); });

    const double sp_query = run_case(
        "QueryHeavy (count/any/all/none)",
        12'000'000, 5,
        [&](size_t it) { return bench_query_heavy<N>(fast, it); },
        [&](size_t it) { return bench_query_heavy<N>(ref, it); });

    const double sp_shift = run_case(
        "ShiftHeavy (<<= >>= mix)",
        5'000'000, 5,
        [&](size_t it) { return bench_shift_heavy<N>(fast, it); },
        [&](size_t it) { return bench_shift_heavy<N>(ref, it); });

    (void)sp_mixed;
    (void)sp_random;
    (void)sp_shift;

    cout << "\n[Breakdown: BitwiseCombine primitives N=" << N << "]\n";
    cout << "  (splitting: &=, |=, ^=, ~assign, &=~ , ^=(&))\n";

    const size_t bw_iters = 20'000'000;
    const int bw_trials = 5;

    run_case("AND only (acc &= x)", bw_iters, bw_trials,
             [&](size_t it) { return bench_and_only<N>(fast, it); },
             [&](size_t it) { return bench_and_only<N>(ref, it); });

    run_case("OR only (acc |= x)", bw_iters, bw_trials,
             [&](size_t it) { return bench_or_only<N>(fast, it); },
             [&](size_t it) { return bench_or_only<N>(ref, it); });

    run_case("XOR only (acc ^= x)", bw_iters, bw_trials,
             [&](size_t it) { return bench_xor_only<N>(fast, it); },
             [&](size_t it) { return bench_xor_only<N>(ref, it); });

    run_case("NOT assign (acc = ~x)", bw_iters, bw_trials,
             [&](size_t it) { return bench_not_assign_only<N>(fast, it); },
             [&](size_t it) { return bench_not_assign_only<N>(ref, it); });

    run_case("AND-NOT (acc &= ~x)", bw_iters, bw_trials,
             [&](size_t it) { return bench_and_not_only<N>(fast, it); },
             [&](size_t it) { return bench_and_not_only<N>(ref, it); });

    run_case("XOR of AND (acc ^= (a & b))", bw_iters, bw_trials,
             [&](size_t it) { return bench_xor_of_and<N>(fast, it); },
             [&](size_t it) { return bench_xor_of_and<N>(ref, it); });

    cout << "\n[Breakdown: QueryHeavy primitives N=" << N << "]\n";
    cout << "  (splitting: count(), any(), all(), none())\n";

    const size_t q_iters = 25'000'000;
    const int q_trials = 5;

    run_case("count() only", q_iters, q_trials,
             [&](size_t it) { return bench_count_only_query<N>(fast, it); },
             [&](size_t it) { return bench_count_only_query<N>(ref, it); });

    run_case("any() only", q_iters, q_trials,
             [&](size_t it) { return bench_any_only_query<N>(fast, it); },
             [&](size_t it) { return bench_any_only_query<N>(ref, it); });

    run_case("all() only", q_iters, q_trials,
             [&](size_t it) { return bench_all_only_query<N>(fast, it); },
             [&](size_t it) { return bench_all_only_query<N>(ref, it); });

    run_case("none() only", q_iters, q_trials,
             [&](size_t it) { return bench_none_only_query<N>(fast, it); },
             [&](size_t it) { return bench_none_only_query<N>(ref, it); });

    cout << "\n[Note] Combined use-case speedups (std/fast):\n";
    cout << "  BitwiseCombine = " << fixed << setprecision(2) << sp_bitwise << " x\n";
    cout << "  QueryHeavy     = " << fixed << setprecision(2) << sp_query << " x\n";
}

int main() {
    TestContext ctx;
    mt19937_64 rng(0xC0FFEE123456789ULL);

    // Edge-case / comprehensive tests for representative sizes.
    edge_tests<1>(ctx);
    edge_tests<64>(ctx);
    edge_tests<65>(ctx);
    edge_tests<127>(ctx);
    edge_tests<128>(ctx);

    // Random consistency tests vs std::bitset.
    random_consistency_tests<1>(ctx, rng, 2000);
    random_consistency_tests<64>(ctx, rng, 4000);
    random_consistency_tests<65>(ctx, rng, 4000);
    random_consistency_tests<127>(ctx, rng, 3000);
    random_consistency_tests<128>(ctx, rng, 3000);

    cout << "\n[Test Summary]\n";
    cout << "  checks : " << ctx.checks << "\n";
    cout << "  failed : " << ctx.failed << "\n";
    cout << "  result : " << (ctx.failed == 0 ? "PASS" : "FAIL") << "\n";

    // Benchmarks (run regardless of test result)
    benchmark_compare<128>(rng);

    return (ctx.failed == 0) ? 0 : 1;
}
#endif


// 実行結果 (atcoder)
// [Test Summary]
//   checks : 513035
//   failed : 0
//   result : PASS

// [Benchmark compare N=128]
//   dataset_size = 4096

// Use-case: MixedOps (xor/shift/flip)
//   iters=5000000  trials=5
//   bitset128         : 13.783 ms (min=13.697, max=13.865, sd=0.060)  [2.76 ns/iter]  [checksum_xor=1195496942484285992]
//   std::bitset       : 52.321 ms (min=52.077, max=52.701, sd=0.207)  [10.46 ns/iter]  [checksum_xor=1195496942484285992]
//   speedup (std/fast) = 3.80 x

// Use-case: BitwiseCombine (and/or/xor/not)
//   iters=5000000  trials=5
//   bitset128         : 5.542 ms (min=5.499, max=5.600, sd=0.046)  [1.11 ns/iter]  [checksum_xor=6526338256462278655]
//   std::bitset       : 4.862 ms (min=4.792, max=4.965, sd=0.056)  [0.97 ns/iter]  [checksum_xor=6526338256462278655]
//   speedup (std/fast) = 0.88 x

// Use-case: RandomBitAccess (set/reset/flip/test/[])
//   iters=8000000  trials=5
//   bitset128         : 34.027 ms (min=33.871, max=34.104, sd=0.081)  [4.25 ns/iter]  [checksum_xor=16518032612712434320]
//   std::bitset       : 72.917 ms (min=72.637, max=73.323, sd=0.251)  [9.11 ns/iter]  [checksum_xor=16518032612712434320]
//   speedup (std/fast) = 2.14 x

// Use-case: QueryHeavy (count/any/all/none)
//   iters=12000000  trials=5
//   bitset128         : 21.180 ms (min=21.080, max=21.337, sd=0.087)  [1.76 ns/iter]  [checksum_xor=767252898]
//   std::bitset       : 16.491 ms (min=16.332, max=16.678, sd=0.112)  [1.37 ns/iter]  [checksum_xor=767252898]
//   speedup (std/fast) = 0.78 x

// Use-case: ShiftHeavy (<<= >>= mix)
//   iters=5000000  trials=5
//   bitset128         : 13.168 ms (min=13.105, max=13.259, sd=0.051)  [2.63 ns/iter]  [checksum_xor=6086527920696686022]
//   std::bitset       : 37.654 ms (min=37.467, max=37.861, sd=0.130)  [7.53 ns/iter]  [checksum_xor=6086527920696686022]
//   speedup (std/fast) = 2.86 x

// [Breakdown: BitwiseCombine primitives N=128]
//   (splitting: &=, |=, ^=, ~assign, &=~ , ^=(&))

// Use-case: AND only (acc &= x)
//   iters=20000000  trials=5
//   bitset128         : 7.512 ms (min=7.374, max=7.585, sd=0.080)  [0.38 ns/iter]  [checksum_xor=11400714819323198485]
//   std::bitset       : 7.625 ms (min=7.553, max=7.691, sd=0.048)  [0.38 ns/iter]  [checksum_xor=11400714819323198485]
//   speedup (std/fast) = 1.02 x

// Use-case: OR only (acc |= x)
//   iters=20000000  trials=5
//   bitset128         : 7.507 ms (min=7.456, max=7.590, sd=0.047)  [0.38 ns/iter]  [checksum_xor=7046029254386353003]
//   std::bitset       : 7.707 ms (min=7.568, max=8.007, sd=0.154)  [0.39 ns/iter]  [checksum_xor=7046029254386353003]
//   speedup (std/fast) = 1.03 x

// Use-case: XOR only (acc ^= x)
//   iters=20000000  trials=5
//   bitset128         : 7.471 ms (min=7.419, max=7.527, sd=0.044)  [0.37 ns/iter]  [checksum_xor=5319768550272830776]
//   std::bitset       : 7.624 ms (min=7.592, max=7.654, sd=0.022)  [0.38 ns/iter]  [checksum_xor=5319768550272830776]
//   speedup (std/fast) = 1.02 x

// Use-case: NOT assign (acc = ~x)
//   iters=20000000  trials=5
//   bitset128         : 0.000 ms (min=0.000, max=0.000, sd=0.000)  [0.00 ns/iter]  [checksum_xor=737407242170041291]
//   std::bitset       : 0.000 ms (min=0.000, max=0.000, sd=0.000)  [0.00 ns/iter]  [checksum_xor=737407242170041291]
//   speedup (std/fast) = 4.37 x

// Use-case: AND-NOT (acc &= ~x)
//   iters=20000000  trials=5
//   bitset128         : 11.464 ms (min=11.365, max=11.537, sd=0.071)  [0.57 ns/iter]  [checksum_xor=11400714819323198485]
//   std::bitset       : 6.707 ms (min=6.656, max=6.737, sd=0.033)  [0.34 ns/iter]  [checksum_xor=11400714819323198485]
//   speedup (std/fast) = 0.59 x

// Use-case: XOR of AND (acc ^= (a & b))
//   iters=20000000  trials=5
//   bitset128         : 14.360 ms (min=14.319, max=14.395, sd=0.026)  [0.72 ns/iter]  [checksum_xor=17544605551632040805]
//   std::bitset       : 12.438 ms (min=12.355, max=12.570, sd=0.073)  [0.62 ns/iter]  [checksum_xor=17544605551632040805]
//   speedup (std/fast) = 0.87 x

// [Breakdown: QueryHeavy primitives N=128]
//   (splitting: count(), any(), all(), none())

// Use-case: count() only
//   iters=25000000  trials=5
//   bitset128         : 14.429 ms (min=14.326, max=14.484, sd=0.054)  [0.58 ns/iter]  [checksum_xor=1598443426]
//   std::bitset       : 14.355 ms (min=14.243, max=14.444, sd=0.065)  [0.57 ns/iter]  [checksum_xor=1598443426]
//   speedup (std/fast) = 0.99 x

// Use-case: any() only
//   iters=25000000  trials=5
//   bitset128         : 17.174 ms (min=17.090, max=17.287, sd=0.065)  [0.69 ns/iter]  [checksum_xor=25000000]
//   std::bitset       : 29.463 ms (min=28.752, max=30.002, sd=0.405)  [1.18 ns/iter]  [checksum_xor=25000000]
//   speedup (std/fast) = 1.72 x

// Use-case: all() only
//   iters=25000000  trials=5
//   bitset128         : 16.795 ms (min=16.691, max=16.900, sd=0.067)  [0.67 ns/iter]  [checksum_xor=0]
//   std::bitset       : 12.287 ms (min=12.180, max=12.332, sd=0.059)  [0.49 ns/iter]  [checksum_xor=0]
//   speedup (std/fast) = 0.73 x

// Use-case: none() only
//   iters=25000000  trials=5
//   bitset128         : 17.187 ms (min=17.131, max=17.286, sd=0.056)  [0.69 ns/iter]  [checksum_xor=0]
//   std::bitset       : 22.444 ms (min=21.885, max=22.822, sd=0.337)  [0.90 ns/iter]  [checksum_xor=0]
//   speedup (std/fast) = 1.31 x

// [Note] Combined use-case speedups (std/fast):
//   BitwiseCombine = 0.88 x
//   QueryHeavy     = 0.78 x
