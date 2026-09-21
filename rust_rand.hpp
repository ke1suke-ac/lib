#pragma once

// AtCoder Heuristic Contest の公式 local tools 互換を主目的とした
// Rust 互換乱数ライブラリ。
//
// 対象は主に rand 0.8 / rand_chacha 0.3 / rand_distr 0.4 系と、
// AHC で実際に使われた Normal / Exp / Perlin / PermutationTable 周辺である。
// C++20 / GCC 12.2 / 標準ライブラリのみ / シングルスレッド前提。

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <numbers>
#include <sstream>
#include <queue>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace rust_rand {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u128 = unsigned __int128;
using i128 = __int128_t;

namespace detail {

template <class T>
inline constexpr bool is_std_int_v =
    std::is_integral_v<T> && !std::is_same_v<T, bool>;

template <class T, class Enable = void>
struct unsigned_equiv {
    using type = std::make_unsigned_t<T>;
};
template <>
struct unsigned_equiv<i128, void> { using type = u128; };
template <>
struct unsigned_equiv<u128, void> { using type = u128; };
template <class T>
using unsigned_equiv_t = typename unsigned_equiv<T>::type;

template <class T>
constexpr unsigned_equiv_t<T> as_unsigned(T x) {
    return static_cast<unsigned_equiv_t<T>>(x);
}

template <class T>
constexpr T from_unsigned(unsigned_equiv_t<T> x) {
    return static_cast<T>(x);
}

template <class T>
constexpr T wrapping_add(T a, T b) {
    return from_unsigned<T>(as_unsigned(a) + as_unsigned(b));
}

template <class T>
constexpr T wrapping_sub(T a, T b) {
    return from_unsigned<T>(as_unsigned(a) - as_unsigned(b));
}

template <class T>
constexpr T wrapping_sub_one(T x) {
    return from_unsigned<T>(as_unsigned(x) - unsigned_equiv_t<T>(1));
}

template <class T>
constexpr int bit_size_v = int(sizeof(T) * 8);

template <class T>
using wider_unsigned_t = std::conditional_t<
    (sizeof(T) <= 4),
    u32,
    std::conditional_t<(sizeof(T) <= 8), u64, void>>;

template <class T>
constexpr std::pair<T, T> wmul(T a, T b) {
    if constexpr (sizeof(T) <= 4) {
        std::uint64_t p = std::uint64_t(a) * std::uint64_t(b);
        return {T(p >> 32), T(p)};
    } else {
        unsigned __int128 p = static_cast<unsigned __int128>(a) * static_cast<unsigned __int128>(b);
        return {T(p >> 64), T(p)};
    }
}

inline constexpr u32 rotl32(u32 x, int k) {
    return std::rotl(x, k);
}

inline constexpr u32 rotr32(u32 x, int k) {
    return std::rotr(x, k);
}

inline constexpr u64 rotr64(u64 x, int k) {
    return std::rotr(x, k);
}

template <class Seed>
Seed seed_from_u64_impl(u64 state) {
    auto pcg32 = [](u64& s) -> std::array<u8, 4> {
        constexpr u64 MUL = 6364136223846793005ULL;
        constexpr u64 INC = 11634580027462260723ULL;
        s = s * MUL + INC;
        u64 v = s;
        u32 xorshifted = static_cast<u32>(((v >> 18) ^ v) >> 27);
        u32 rot = static_cast<u32>(v >> 59);
        u32 x = rotr32(xorshifted, int(rot));
        return std::array<u8, 4>{u8(x), u8(x >> 8), u8(x >> 16), u8(x >> 24)};
    };
    Seed seed{};
    auto* p = seed.data();
    std::size_t n = seed.size();
    std::size_t i = 0;
    while (i + 4 <= n) {
        auto bytes = pcg32(state);
        std::memcpy(p + i, bytes.data(), 4);
        i += 4;
    }
    if (i < n) {
        auto bytes = pcg32(state);
        std::memcpy(p + i, bytes.data(), n - i);
    }
    return seed;
}

template <class Float>
inline Float prev_float(Float x) {
    return std::nextafter(x, Float(0));
}

template <class Float> struct float_traits;
template <> struct float_traits<float> {
    using uint_type = u32;
    static constexpr int fraction_bits = 23;
    static constexpr int exponent_bias = 127;
    static constexpr int bits_to_discard_uniform = 32 - 23;
    static constexpr int bits_to_discard_open = 32 - 23;
    static constexpr int precision = 24;
    static constexpr float epsilon = std::numeric_limits<float>::epsilon();
};
template <> struct float_traits<double> {
    using uint_type = u64;
    static constexpr int fraction_bits = 52;
    static constexpr int exponent_bias = 1023;
    static constexpr int bits_to_discard_uniform = 64 - 52;
    static constexpr int bits_to_discard_open = 64 - 52;
    static constexpr int precision = 53;
    static constexpr double epsilon = std::numeric_limits<double>::epsilon();
};

template <class Float>
inline Float into_float_with_exponent(typename float_traits<Float>::uint_type fraction, int exponent) {
    using U = typename float_traits<Float>::uint_type;
    constexpr int frac = float_traits<Float>::fraction_bits;
    constexpr int bias = float_traits<Float>::exponent_bias;
    U exp_bits = U(bias + exponent) << frac;
    return std::bit_cast<Float>(fraction | exp_bits);
}

template <class T>
inline std::string hex_bits(T value) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    if constexpr (std::is_same_v<T, float>) {
        oss << std::setw(8) << std::bit_cast<u32>(value);
    } else if constexpr (std::is_same_v<T, double>) {
        oss << std::setw(16) << std::bit_cast<u64>(value);
    } else if constexpr (std::is_same_v<T, u128> || std::is_same_v<T, i128>) {
        u128 x = static_cast<u128>(value);
        u64 hi = static_cast<u64>(x >> 64);
        u64 lo = static_cast<u64>(x);
        oss << std::setw(16) << hi << std::setw(16) << lo;
    } else if constexpr (sizeof(T) <= 4) {
        oss << std::setw(sizeof(T) * 2) << +static_cast<unsigned long long>(static_cast<unsigned_equiv_t<T>>(value));
    } else {
        oss << std::setw(sizeof(T) * 2) << static_cast<unsigned long long>(static_cast<unsigned_equiv_t<T>>(value));
    }
    return oss.str();
}

template <class T>
inline constexpr bool is_finite_scalar(T x) {
    if constexpr (std::is_floating_point_v<T>) return std::isfinite(x);
    else return true;
}

template <class T>
inline constexpr bool non_negative_scalar(T x) {
    if constexpr (std::is_floating_point_v<T>) return x >= T(0);
    else if constexpr (std::is_signed_v<T>) return x >= T(0);
    else return true;
}

} // namespace detail

template <class T, class Enable = void>
class Uniform;

// noise::PermutationTable 用の XorShiftRng 互換実装。
class XorShiftRng {
public:
    using Seed = std::array<u8, 16>;

    XorShiftRng() : XorShiftRng(Seed{}) {}
    explicit XorShiftRng(const Seed& seed) { from_seed(seed); }

    static XorShiftRng seed_from_u64(u64 seed) {
        return XorShiftRng(detail::seed_from_u64_impl<Seed>(seed));
    }

    void from_seed(const Seed& seed) {
        auto read_le = [&](int off) -> u32 {
            return u32(seed[off]) | (u32(seed[off + 1]) << 8) | (u32(seed[off + 2]) << 16) | (u32(seed[off + 3]) << 24);
        };
        x_ = read_le(0);
        y_ = read_le(4);
        z_ = read_le(8);
        w_ = read_le(12);
        if ((x_ | y_ | z_ | w_) == 0) {
            x_ = y_ = z_ = w_ = 0x0BAD5EEDu;
        }
    }

    u32 next_u32() {
        u32 x = x_;
        u32 t = x ^ (x << 11);
        x_ = y_;
        y_ = z_;
        z_ = w_;
        u32 w = w_;
        w_ = w ^ (w >> 19) ^ (t ^ (t >> 8));
        return w_;
    }

    u64 next_u64() {
        u64 lo = next_u32();
        u64 hi = next_u32();
        return (hi << 32) | lo;
    }

    void fill_bytes(u8* dest, std::size_t n) {
        std::size_t i = 0;
        while (i + 4 <= n) {
            u32 v = next_u32();
            dest[i + 0] = u8(v);
            dest[i + 1] = u8(v >> 8);
            dest[i + 2] = u8(v >> 16);
            dest[i + 3] = u8(v >> 24);
            i += 4;
        }
        if (i < n) {
            u32 v = next_u32();
            for (std::size_t j = 0; i + j < n; ++j) dest[i + j] = u8(v >> (8 * j));
        }
    }

    template <class T>
    T gen() {
        if constexpr (std::is_same_v<T, u32>) return next_u32();
        else if constexpr (std::is_same_v<T, u64>) return next_u64();
        else static_assert(sizeof(T) == 0, "unsupported type for XorShiftRng::gen()");
    }

    u32 gen_range(u32 low, u32 high) {
        assert(low < high && "XorShiftRng::gen_range: low >= high");
        u32 range = high - low;
        if (range == 0) return next_u32();
        u32 zone = (range << std::countl_zero(range)) - 1u;
        while (true) {
            auto [hi, lo] = detail::wmul(next_u32(), range);
            if (lo <= zone) return low + hi;
        }
    }

private:
    u32 x_ = 0x0BAD5EEDu;
    u32 y_ = 0x0BAD5EEDu;
    u32 z_ = 0x0BAD5EEDu;
    u32 w_ = 0x0BAD5EEDu;
};

// rand_chacha::ChaCha20Rng 互換実装。
// seed_from_u64 の挙動も rand_core に合わせている。
class ChaCha20Rng {
public:
    using Seed = std::array<u8, 32>;

    ChaCha20Rng() = default;
    explicit ChaCha20Rng(const Seed& seed) { from_seed(seed); }

    static ChaCha20Rng seed_from_u64(u64 seed) {
        return ChaCha20Rng(detail::seed_from_u64_impl<Seed>(seed));
    }

    void from_seed(const Seed& seed) {
        seed_ = seed;
        for (int i = 0; i < 8; ++i) {
            key_[i] = u32(seed[i * 4 + 0]) | (u32(seed[i * 4 + 1]) << 8) | (u32(seed[i * 4 + 2]) << 16) | (u32(seed[i * 4 + 3]) << 24);
        }
        counter_ = 0;
        stream_ = 0;
        index_ = buffer_.size();
    }

    const Seed& seed() const { return seed_; }

    u32 next_u32() {
        if (index_ >= buffer_.size()) refill4();
        return buffer_[index_++];
    }

    u64 next_u64() {
        u64 lo = next_u32();
        u64 hi = next_u32();
        return (hi << 32) | lo;
    }

    void fill_bytes(u8* dest, std::size_t n) {
        std::size_t i = 0;
        while (i + 4 <= n) {
            u32 v = next_u32();
            dest[i + 0] = u8(v);
            dest[i + 1] = u8(v >> 8);
            dest[i + 2] = u8(v >> 16);
            dest[i + 3] = u8(v >> 24);
            i += 4;
        }
        if (i < n) {
            u32 v = next_u32();
            for (std::size_t j = 0; i + j < n; ++j) dest[i + j] = u8(v >> (8 * j));
        }
    }

    u128 get_word_pos() const {
        if (index_ >= buffer_.size()) {
            return u128(counter_) * 16u;
        }
        return u128(counter_ - 4) * 16u + u128(index_);
    }

    void set_word_pos(u128 word_offset) {
        counter_ = static_cast<u64>(word_offset / 16u);
        refill4();
        index_ = static_cast<std::size_t>(word_offset % 16u);
    }

    void set_stream(u64 stream) {
        if (stream_ == stream) return;
        if (index_ < buffer_.size()) {
            u128 wp = get_word_pos();
            stream_ = stream;
            set_word_pos(wp);
        } else {
            stream_ = stream;
        }
    }

    u64 get_stream() const {
        return stream_;
    }

    template <class T>
    T gen() {
        if constexpr (std::is_same_v<T, u8>) {
            return static_cast<u8>(next_u32());
        } else if constexpr (std::is_same_v<T, u16>) {
            return static_cast<u16>(next_u32());
        } else if constexpr (std::is_same_v<T, u32>) {
            return next_u32();
        } else if constexpr (std::is_same_v<T, u64>) {
            return next_u64();
        } else if constexpr (std::is_same_v<T, u128>) {
            u128 lo = next_u64();
            u128 hi = next_u64();
            return (hi << 64) | lo;
        } else if constexpr (std::is_same_v<T, std::size_t>) {
            if constexpr (sizeof(std::size_t) == 8) return static_cast<std::size_t>(next_u64());
            else return static_cast<std::size_t>(next_u32());
        } else if constexpr (std::is_same_v<T, i8>) {
            return static_cast<i8>(gen<u8>());
        } else if constexpr (std::is_same_v<T, i16>) {
            return static_cast<i16>(gen<u16>());
        } else if constexpr (std::is_same_v<T, i32>) {
            return static_cast<i32>(gen<u32>());
        } else if constexpr (std::is_same_v<T, i64>) {
            return static_cast<i64>(gen<u64>());
        } else if constexpr (std::is_same_v<T, i128>) {
            return static_cast<i128>(gen<u128>());
        } else if constexpr (std::is_same_v<T, std::ptrdiff_t>) {
            if constexpr (sizeof(std::ptrdiff_t) == 8) return static_cast<std::ptrdiff_t>(next_u64());
            else return static_cast<std::ptrdiff_t>(next_u32());
        } else if constexpr (std::is_same_v<T, float>) {
            constexpr int precision = detail::float_traits<float>::precision;
            constexpr float scale = 1.0f / float(u32(1) << precision);
            u32 value = gen<u32>() >> (32 - precision);
            return scale * static_cast<float>(value);
        } else if constexpr (std::is_same_v<T, double>) {
            constexpr int precision = detail::float_traits<double>::precision;
            constexpr double scale = 1.0 / double(u64(1) << precision);
            u64 value = gen<u64>() >> (64 - precision);
            return scale * static_cast<double>(value);
        } else {
            static_assert(sizeof(T) == 0, "unsupported type for ChaCha20Rng::gen()");
        }
    }

    template <class Dist>
    auto sample(const Dist& dist) -> typename Dist::result_type {
        return dist.sample(*this);
    }

    template <class T>
    T gen_range(T low, T high) {
        return Uniform<T>::sample_single(low, high, *this);
    }

    template <class T>
    T gen_range_inclusive(T low, T high) {
        return Uniform<T>::sample_single_inclusive(low, high, *this);
    }

    bool gen_bool(double p);

private:
    static inline void quarter_round(u32& a, u32& b, u32& c, u32& d) {
        a += b; d ^= a; d = detail::rotl32(d, 16);
        c += d; b ^= c; b = detail::rotl32(b, 12);
        a += b; d ^= a; d = detail::rotl32(d, 8);
        c += d; b ^= c; b = detail::rotl32(b, 7);
    }

    void refill4() {
        constexpr u32 C0 = 0x61707865u;
        constexpr u32 C1 = 0x3320646eu;
        constexpr u32 C2 = 0x79622d32u;
        constexpr u32 C3 = 0x6b206574u;
        for (int block = 0; block < 4; ++block) {
            u32 state[16];
            state[0] = C0; state[1] = C1; state[2] = C2; state[3] = C3;
            for (int i = 0; i < 8; ++i) state[4 + i] = key_[i];
            u64 ctr = counter_ + static_cast<u64>(block);
            state[12] = static_cast<u32>(ctr);
            state[13] = static_cast<u32>(ctr >> 32);
            state[14] = static_cast<u32>(stream_);
            state[15] = static_cast<u32>(stream_ >> 32);

            u32 x[16];
            std::memcpy(x, state, sizeof(x));
            for (int round = 0; round < 10; ++round) {
                quarter_round(x[0], x[4], x[8], x[12]);
                quarter_round(x[1], x[5], x[9], x[13]);
                quarter_round(x[2], x[6], x[10], x[14]);
                quarter_round(x[3], x[7], x[11], x[15]);
                quarter_round(x[0], x[5], x[10], x[15]);
                quarter_round(x[1], x[6], x[11], x[12]);
                quarter_round(x[2], x[7], x[8], x[13]);
                quarter_round(x[3], x[4], x[9], x[14]);
            }
            for (int i = 0; i < 16; ++i) {
                buffer_[block * 16 + i] = x[i] + state[i];
            }
        }
        counter_ += 4;
        index_ = 0;
    }

    Seed seed_{};
    std::array<u32, 8> key_{};
    u64 counter_ = 0;
    u64 stream_ = 0;
    std::array<u32, 64> buffer_{};
    std::size_t index_ = 64;
};

template <class T, class Enable>
class Uniform;

// rand::distributions::Uniform の整数版。
template <class T>
class Uniform<T, std::enable_if_t<detail::is_std_int_v<T>>> {
public:
    using result_type = T;
    using U = detail::unsigned_equiv_t<T>;
    using ULarge = detail::wider_unsigned_t<U>;
    static_assert(!std::is_void_v<ULarge>, "Uniform<int> supports up to 64-bit integers only");

    Uniform(T low, T high) : Uniform(new_tag{}, low, high) {}
    static Uniform inclusive(T low, T high) { return Uniform(inclusive_tag{}, low, high); }

    result_type sample(ChaCha20Rng& rng) const {
        ULarge range = static_cast<ULarge>(range_bits_);
        if (range > 0) {
            constexpr ULarge UMAX = std::numeric_limits<ULarge>::max();
            ULarge zone = UMAX - static_cast<ULarge>(reject_or_zone_);
            while (true) {
                ULarge v = rng.gen<ULarge>();
                auto [hi, lo] = detail::wmul(v, range);
                if (lo <= zone) {
                    U ans = static_cast<U>(detail::as_unsigned(low_) + static_cast<U>(hi));
                    return detail::from_unsigned<T>(ans);
                }
            }
        }
        return rng.gen<T>();
    }

    static result_type sample_single(T low, T high, ChaCha20Rng& rng) {
        assert(low < high && "Uniform::sample_single: low >= high");
        return sample_single_inclusive(low, detail::wrapping_sub_one(high), rng);
    }

    static result_type sample_single_inclusive(T low, T high, ChaCha20Rng& rng) {
        assert(low <= high && "Uniform::sample_single_inclusive: low > high");
        ULarge range = static_cast<ULarge>(static_cast<U>(detail::as_unsigned(high) - detail::as_unsigned(low) + U(1)));
        if (range == 0) return rng.gen<T>();

        ULarge zone;
        if constexpr (std::numeric_limits<U>::max() <= static_cast<U>(std::numeric_limits<u16>::max())) {
            constexpr ULarge UMAX = std::numeric_limits<ULarge>::max();
            ULarge ints_to_reject = (UMAX - range + 1) % range;
            zone = UMAX - ints_to_reject;
        } else {
            zone = (range << std::countl_zero(range)) - 1;
        }

        while (true) {
            ULarge v = rng.gen<ULarge>();
            auto [hi, lo] = detail::wmul(v, range);
            if (lo <= zone) {
                U ans = static_cast<U>(detail::as_unsigned(low) + static_cast<U>(hi));
                return detail::from_unsigned<T>(ans);
            }
        }
    }

private:
    struct new_tag {};
    struct inclusive_tag {};

    Uniform(new_tag, T low, T high) : Uniform(inclusive_tag{}, low, detail::wrapping_sub_one(high)) {
        assert(low < high && "Uniform::new: low >= high");
    }

    Uniform(inclusive_tag, T low, T high) : low_(low) {
        assert(low <= high && "Uniform::new_inclusive: low > high");
        constexpr ULarge UMAX = std::numeric_limits<ULarge>::max();
        U range = static_cast<U>(detail::as_unsigned(high) - detail::as_unsigned(low) + U(1));
        ULarge ints_to_reject = 0;
        if (range > 0) {
            ULarge r = static_cast<ULarge>(range);
            ints_to_reject = (UMAX - r + 1) % r;
        }
        range_bits_ = detail::from_unsigned<T>(range);
        reject_or_zone_ = detail::from_unsigned<T>(static_cast<U>(ints_to_reject));
    }

    T low_{};
    T range_bits_{};
    T reject_or_zone_{};
};

// rand::distributions::Uniform の浮動小数版。
template <class T>
class Uniform<T, std::enable_if_t<std::is_floating_point_v<T>>> {
public:
    using result_type = T;
    using U = typename detail::float_traits<T>::uint_type;

    Uniform(T low, T high) : Uniform(new_tag{}, low, high) {}
    static Uniform inclusive(T low, T high) { return Uniform(inclusive_tag{}, low, high); }

    result_type sample(ChaCha20Rng& rng) const {
        constexpr int discard = detail::float_traits<T>::bits_to_discard_uniform;
        U bits = rng.gen<U>() >> discard;
        T value1_2 = detail::into_float_with_exponent<T>(bits, 0);
        T value0_1 = value1_2 - T(1);
        return value0_1 * scale_ + low_;
    }

    static result_type sample_single(T low, T high, ChaCha20Rng& rng) {
        assert(low < high && "Uniform::sample_single: low >= high");
        T scale = high - low;
        assert(std::isfinite(scale) && "Uniform::sample_single: range overflow");
        constexpr int discard = detail::float_traits<T>::bits_to_discard_uniform;
        while (true) {
            U bits = rng.gen<U>() >> discard;
            T value1_2 = detail::into_float_with_exponent<T>(bits, 0);
            T value0_1 = value1_2 - T(1);
            T res = value0_1 * scale + low;
            if (res < high) return res;
            if (!std::isfinite(scale)) {
                assert(std::isfinite(low) && std::isfinite(high) && "Uniform::sample_single: low and high must be finite");
                scale = detail::prev_float(scale);
            }
        }
    }

    static result_type sample_single_inclusive(T low, T high, ChaCha20Rng& rng) {
        return Uniform::inclusive(low, high).sample(rng);
    }

private:
    struct new_tag {};
    struct inclusive_tag {};

    Uniform(new_tag, T low, T high) : low_(low) {
        assert(low < high && "Uniform::new: low >= high");
        constexpr int discard = detail::float_traits<T>::bits_to_discard_uniform;
        constexpr U umax = std::numeric_limits<U>::max();
        T max_rand = detail::into_float_with_exponent<T>(umax >> discard, 0) - T(1);
        T scale = high - low;
        assert(std::isfinite(scale) && "Uniform::new: range overflow");
        while (scale * max_rand + low >= high) {
            scale = detail::prev_float(scale);
        }
        scale_ = scale;
    }

    Uniform(inclusive_tag, T low, T high) : low_(low) {
        assert(low <= high && "Uniform::new_inclusive: low > high");
        constexpr int discard = detail::float_traits<T>::bits_to_discard_uniform;
        constexpr U umax = std::numeric_limits<U>::max();
        T max_rand = detail::into_float_with_exponent<T>(umax >> discard, 0) - T(1);
        T scale = (high - low) / max_rand;
        assert(std::isfinite(scale) && "Uniform::new_inclusive: range overflow");
        while (scale * max_rand + low > high) {
            scale = detail::prev_float(scale);
        }
        scale_ = scale;
    }

    T low_{};
    T scale_{};
};

// rand::distributions::Bernoulli 互換実装。
class Bernoulli {
public:
    using result_type = bool;
    static constexpr u64 ALWAYS_TRUE = std::numeric_limits<u64>::max();
    static constexpr double SCALE = 2.0 * double(u64(1) << 63);

    explicit Bernoulli(double p) {
        if (!(0.0 <= p && p < 1.0)) {
            if (p == 1.0) {
                p_int_ = ALWAYS_TRUE;
                return;
            }
            throw std::invalid_argument("Bernoulli: p is outside [0,1]");
        }
        p_int_ = static_cast<u64>(p * SCALE);
    }

    static Bernoulli from_ratio(u32 numerator, u32 denominator) {
        if (numerator > denominator || denominator == 0) {
            throw std::invalid_argument("Bernoulli::from_ratio: invalid ratio");
        }
        if (numerator == denominator) {
            Bernoulli b(0.0);
            b.p_int_ = ALWAYS_TRUE;
            return b;
        }
        Bernoulli b(0.0);
        b.p_int_ = static_cast<u64>((double(numerator) / double(denominator)) * SCALE);
        return b;
    }

    bool sample(ChaCha20Rng& rng) const {
        if (p_int_ == ALWAYS_TRUE) return true;
        return rng.gen<u64>() < p_int_;
    }

private:
    u64 p_int_ = 0;
};

inline bool ChaCha20Rng::gen_bool(double p) {
    return Bernoulli(p).sample(*this);
}

// rand::distributions::WeightedIndex 互換実装。
template <class T>
class WeightedIndex {
public:
    using result_type = std::size_t;

    template <class Range>
    explicit WeightedIndex(const Range& weights) {
        assign(weights);
    }

    template <class Range>
    void assign(const Range& weights) {
        cumulative_weights_.clear();
        auto it = std::begin(weights);
        auto ed = std::end(weights);
        if (it == ed) throw std::invalid_argument("WeightedIndex: no weights");
        T total = *it;
        T zero = T{};
        if (!(total >= zero)) throw std::invalid_argument("WeightedIndex: invalid weight");
        ++it;
        for (; it != ed; ++it) {
            const T w = *it;
            if (!(w >= zero)) throw std::invalid_argument("WeightedIndex: invalid weight");
            cumulative_weights_.push_back(total);
            total += w;
        }
        if (total == zero) throw std::invalid_argument("WeightedIndex: all weights are zero");
        total_weight_ = total;
        weight_distribution_ = Uniform<T>(zero, total);
    }

    result_type sample(ChaCha20Rng& rng) const {
        T chosen_weight = weight_distribution_.sample(rng);
        auto it = std::upper_bound(cumulative_weights_.begin(), cumulative_weights_.end(), chosen_weight);
        return static_cast<std::size_t>(it - cumulative_weights_.begin());
    }

private:
    std::vector<T> cumulative_weights_;
    T total_weight_{};
    Uniform<T> weight_distribution_ = Uniform<T>::inclusive(T{}, T{});
};

struct StandardNormal {
    using result_type = double;

    static double open01(ChaCha20Rng& rng) {
        using U = u64;
        constexpr int frac = detail::float_traits<double>::fraction_bits;
        U value = rng.gen<U>();
        U fraction = value >> (64 - frac);
        return detail::into_float_with_exponent<double>(fraction, 0) - (1.0 - std::numeric_limits<double>::epsilon() / 2.0);
    }

    static double sample_f64(ChaCha20Rng& rng);
    double sample(ChaCha20Rng& rng) const { return sample_f64(rng); }
    float sample_f32(ChaCha20Rng& rng) const { return static_cast<float>(sample_f64(rng)); }
};

template <class Float = double>
class Normal {
public:
    using result_type = Float;
    Normal(Float mean, Float std_dev) : mean_(mean), std_dev_(std_dev) {
        if (!std::isfinite(std_dev_)) {
            throw std::invalid_argument("Normal: std_dev must be finite");
        }
    }
    Float sample(ChaCha20Rng& rng) const {
        if constexpr (std::is_same_v<Float, double>) {
            return mean_ + std_dev_ * StandardNormal().sample(rng);
        } else {
            return mean_ + std_dev_ * static_cast<Float>(StandardNormal().sample(rng));
        }
    }
    Float from_zscore(Float z) const { return mean_ + std_dev_ * z; }
    Float mean() const { return mean_; }
    Float std_dev() const { return std_dev_; }
private:
    Float mean_;
    Float std_dev_;
};

// rand_distr::Exp1 互換。レート 1 の標準指数分布を生成する。
struct Exp1 {
    using result_type = double;

    // 標準指数分布の double 値を生成する、期待 O(1)
    static double sample_f64(ChaCha20Rng& rng);

    // 標準指数分布の double 値を生成する、期待 O(1)
    double sample(ChaCha20Rng& rng) const { return sample_f64(rng); }

    // 標準指数分布の float 値を生成する、期待 O(1)
    float sample_f32(ChaCha20Rng& rng) const { return static_cast<float>(sample_f64(rng)); }
};

// rand_distr::Exp 互換。レート lambda の指数分布を生成する。
template <class Float = double>
class Exp {
    static_assert(std::is_same_v<Float, float> || std::is_same_v<Float, double>);

public:
    using result_type = Float;

    // レート lambda の指数分布を構築する、O(1)
    explicit Exp(Float lambda) {
        if (!(lambda >= Float(0))) {
            throw std::invalid_argument("Exp: lambda is negative or NaN");
        }
        lambda_inverse_ = Float(1) / lambda;
    }

    // 指数分布から 1 値を生成する、期待 O(1)
    Float sample(ChaCha20Rng& rng) const {
        if constexpr (std::is_same_v<Float, float>) {
            return Exp1().sample_f32(rng) * lambda_inverse_;
        } else {
            return Exp1().sample(rng) * lambda_inverse_;
        }
    }

private:
    Float lambda_inverse_{};
};

namespace ziggurat_tables {
    inline constexpr double ZIG_NORM_R = 3.654152885361008796;
    inline constexpr std::array<double, 257> ZIG_NORM_X = {
        3.910757959537090045, 3.654152885361008796, 3.449278298560964462, 3.320244733839166074,
        3.224575052047029100, 3.147889289517149969, 3.083526132001233044, 3.027837791768635434,
        2.978603279880844834, 2.934366867207854224, 2.894121053612348060, 2.857138730872132548,
        2.822877396825325125, 2.790921174000785765, 2.760944005278822555, 2.732685359042827056,
        2.705933656121858100, 2.680514643284522158, 2.656283037575502437, 2.633116393630324570,
        2.610910518487548515, 2.589575986706995181, 2.569035452680536569, 2.549221550323460761,
        2.530075232158516929, 2.511544441625342294, 2.493583041269680667, 2.476149939669143318,
        2.459208374333311298, 2.442725318198956774, 2.426670984935725972, 2.411018413899685520,
        2.395743119780480601, 2.380822795170626005, 2.366237056715818632, 2.351967227377659952,
        2.337996148795031370, 2.324308018869623016, 2.310888250599850036, 2.297723348901329565,
        2.284800802722946056, 2.272108990226823888, 2.259637095172217780, 2.247375032945807760,
        2.235313384928327984, 2.223443340090905718, 2.211756642882544366, 2.200245546609647995,
        2.188902771624720689, 2.177721467738641614, 2.166695180352645966, 2.155817819875063268,
        2.145083634046203613, 2.134487182844320152, 2.124023315687815661, 2.113687150684933957,
        2.103474055713146829, 2.093379631137050279, 2.083399693996551783, 2.073530263516978778,
        2.063767547809956415, 2.054107931648864849, 2.044547965215732788, 2.035084353727808715,
        2.025713947862032960, 2.016433734904371722, 2.007240830558684852, 1.998132471356564244,
        1.989106007615571325, 1.980158896898598364, 1.971288697931769640, 1.962493064942461896,
        1.953769742382734043, 1.945116560006753925, 1.936531428273758904, 1.928012334050718257,
        1.919557336591228847, 1.911164563769282232, 1.902832208548446369, 1.894558525668710081,
        1.886341828534776388, 1.878180486290977669, 1.870072921069236838, 1.862017605397632281,
        1.854013059758148119, 1.846057850283119750, 1.838150586580728607, 1.830289919680666566,
        1.822474540091783224, 1.814703175964167636, 1.806974591348693426, 1.799287584547580199,
        1.791640986550010028, 1.784033659547276329, 1.776464495522344977, 1.768932414909077933,
        1.761436365316706665, 1.753975320315455111, 1.746548278279492994, 1.739154261283669012,
        1.731792314050707216, 1.724461502945775715, 1.717160915015540690, 1.709889657069006086,
        1.702646854797613907, 1.695431651932238548, 1.688243209434858727, 1.681080704722823338,
        1.673943330923760353, 1.666830296159286684, 1.659740822855789499, 1.652674147080648526,
        1.645629517902360339, 1.638606196773111146, 1.631603456932422036, 1.624620582830568427,
        1.617656869570534228, 1.610711622367333673, 1.603784156023583041, 1.596873794420261339,
        1.589979870021648534, 1.583101723393471438, 1.576238702733332886, 1.569390163412534456,
        1.562555467528439657, 1.555733983466554893, 1.548925085471535512, 1.542128153226347553,
        1.535342571438843118, 1.528567729435024614, 1.521803020758293101, 1.515047842773992404,
        1.508301596278571965, 1.501563685112706548, 1.494833515777718391, 1.488110497054654369,
        1.481394039625375747, 1.474683555695025516, 1.467978458615230908, 1.461278162507407830,
        1.454582081885523293, 1.447889631277669675, 1.441200224845798017, 1.434513276002946425,
        1.427828197027290358, 1.421144398672323117, 1.414461289772464658, 1.407778276843371534,
        1.401094763676202559, 1.394410150925071257, 1.387723835686884621, 1.381035211072741964,
        1.374343665770030531, 1.367648583594317957, 1.360949343030101844, 1.354245316759430606,
        1.347535871177359290, 1.340820365893152122, 1.334098153216083604, 1.327368577624624679,
        1.320630975217730096, 1.313884673146868964, 1.307128989027353860, 1.300363230327433728,
        1.293586693733517645, 1.286798664489786415, 1.279998415710333237, 1.273185207661843732,
        1.266358287014688333, 1.259516886060144225, 1.252660221891297887, 1.245787495544997903,
        1.238897891102027415, 1.231990574742445110, 1.225064693752808020, 1.218119375481726552,
        1.211153726239911244, 1.204166830140560140, 1.197157747875585931, 1.190125515422801650,
        1.183069142678760732, 1.175987612011489825, 1.168879876726833800, 1.161744859441574240,
        1.154581450355851802, 1.147388505416733873, 1.140164844363995789, 1.132909248648336975,
        1.125620459211294389, 1.118297174115062909, 1.110938046009249502, 1.103541679420268151,
        1.096106627847603487, 1.088631390649514197, 1.081114409698889389, 1.073554065787871714,
        1.065948674757506653, 1.058296483326006454, 1.050595664586207123, 1.042844313139370538,
        1.035040439828605274, 1.027181966030751292, 1.019266717460529215, 1.011292417434978441,
        1.003256679539591412, 0.995156999629943084, 0.986990747093846266, 0.978755155288937750,
        0.970447311058864615, 0.962064143217605250, 0.953602409875572654, 0.945058684462571130,
        0.936429340280896860, 0.927710533396234771, 0.918898183643734989, 0.909987953490768997,
        0.900975224455174528, 0.891855070726792376, 0.882622229578910122, 0.873271068082494550,
        0.863795545546826915, 0.854189171001560554, 0.844444954902423661, 0.834555354079518752,
        0.824512208745288633, 0.814306670128064347, 0.803929116982664893, 0.793369058833152785,
        0.782615023299588763, 0.771654424216739354, 0.760473406422083165, 0.749056662009581653,
        0.737387211425838629, 0.725446140901303549, 0.713212285182022732, 0.700661841097584448,
        0.687767892786257717, 0.674499822827436479, 0.660822574234205984, 0.646695714884388928,
        0.632072236375024632, 0.616896989996235545, 0.601104617743940417, 0.584616766093722262,
        0.567338257040473026, 0.549151702313026790, 0.529909720646495108, 0.509423329585933393,
        0.487443966121754335, 0.463634336771763245, 0.437518402186662658, 0.408389134588000746,
        0.375121332850465727, 0.335737519180459465, 0.286174591747260509, 0.215241895913273806,
        0.000000000000000000
    };
    inline constexpr std::array<double, 257> ZIG_NORM_F = {
        0.000477467764586655, 0.001260285930498598, 0.002609072746106363, 0.004037972593371872,
        0.005522403299264754, 0.007050875471392110, 0.008616582769422917, 0.010214971439731100,
        0.011842757857943104, 0.013497450601780807, 0.015177088307982072, 0.016880083152595839,
        0.018605121275783350, 0.020351096230109354, 0.022117062707379922, 0.023902203305873237,
        0.025705804008632656, 0.027527235669693315, 0.029365939758230111, 0.031221417192023690,
        0.033093219458688698, 0.034980941461833073, 0.036884215688691151, 0.038802707404656918,
        0.040736110656078753, 0.042684144916619378, 0.044646552251446536, 0.046623094902089664,
        0.048613553216035145, 0.050617723861121788, 0.052635418276973649, 0.054666461325077916,
        0.056710690106399467, 0.058767952921137984, 0.060838108349751806, 0.062921024437977854,
        0.065016577971470438, 0.067124653828023989, 0.069245144397250269, 0.071377949059141965,
        0.073522973714240991, 0.075680130359194964, 0.077849336702372207, 0.080030515814947509,
        0.082223595813495684, 0.084428509570654661, 0.086645194450867782, 0.088873592068594229,
        0.091113648066700734, 0.093365311913026619, 0.095628536713353335, 0.097903279039215627,
        0.100189498769172020, 0.102487158942306270, 0.104796225622867056, 0.107116667775072880,
        0.109448457147210021, 0.111791568164245583, 0.114145977828255210, 0.116511665626037014,
        0.118888613443345698, 0.121276805485235437, 0.123676228202051403, 0.126086870220650349,
        0.128508722280473636, 0.130941777174128166, 0.133386029692162844, 0.135841476571757352,
        0.138308116449064322, 0.140785949814968309, 0.143274978974047118, 0.145775208006537926,
        0.148286642733128721, 0.150809290682410169, 0.153343161060837674, 0.155888264725064563,
        0.158444614156520225, 0.161012223438117663, 0.163591108232982951, 0.166181285765110071,
        0.168782774801850333, 0.171395595638155623, 0.174019770082499359, 0.176655321444406654,
        0.179302274523530397, 0.181960655600216487, 0.184630492427504539, 0.187311814224516926,
        0.190004651671193070, 0.192709036904328807, 0.195425003514885592, 0.198152586546538112,
        0.200891822495431333, 0.203642749311121501, 0.206405406398679298, 0.209179834621935651,
        0.211966076307852941, 0.214764175252008499, 0.217574176725178370, 0.220396127481011589,
        0.223230075764789593, 0.226076071323264877, 0.228934165415577484, 0.231804410825248525,
        0.234686861873252689, 0.237581574432173676, 0.240488605941449107, 0.243408015423711988,
        0.246339863502238771, 0.249284212419516704, 0.252241126056943765, 0.255210669955677150,
        0.258192911338648023, 0.261187919133763713, 0.264195763998317568, 0.267216518344631837,
        0.270250256366959984, 0.273297054069675804, 0.276356989296781264, 0.279430141762765316,
        0.282516593084849388, 0.285616426816658109, 0.288729728483353931, 0.291856585618280984,
        0.294997087801162572, 0.298151326697901342, 0.301319396102034120, 0.304501391977896274,
        0.307697412505553769, 0.310907558127563710, 0.314131931597630143, 0.317370638031222396,
        0.320623784958230129, 0.323891482377732021, 0.327173842814958593, 0.330470981380537099,
        0.333783015832108509, 0.337110066638412809, 0.340452257045945450, 0.343809713148291340,
        0.347182563958251478, 0.350570941482881204, 0.353974980801569250, 0.357394820147290515,
        0.360830600991175754, 0.364282468130549597, 0.367750569780596226, 0.371235057669821344,
        0.374736087139491414, 0.378253817247238111, 0.381788410875031348, 0.385340034841733958,
        0.388908860020464597, 0.392495061461010764, 0.396098818517547080, 0.399720314981931668,
        0.403359739222868885, 0.407017284331247953, 0.410693148271983222, 0.414387534042706784,
        0.418100649839684591, 0.421832709231353298, 0.425583931339900579, 0.429354541031341519,
        0.433144769114574058, 0.436954852549929273, 0.440785034667769915, 0.444635565397727750,
        0.448506701509214067, 0.452398706863882505, 0.456311852680773566, 0.460246417814923481,
        0.464202689050278838, 0.468180961407822172, 0.472181538469883255, 0.476204732721683788,
        0.480250865911249714, 0.484320269428911598, 0.488413284707712059, 0.492530263646148658,
        0.496671569054796314, 0.500837575128482149, 0.505028667945828791, 0.509245245998136142,
        0.513487720749743026, 0.517756517232200619, 0.522052074674794864, 0.526374847174186700,
        0.530725304406193921, 0.535103932383019565, 0.539511234259544614, 0.543947731192649941,
        0.548413963257921133, 0.552910490428519918, 0.557437893621486324, 0.561996775817277916,
        0.566587763258951771, 0.571211506738074970, 0.575868682975210544, 0.580559996103683473,
        0.585286179266300333, 0.590047996335791969, 0.594846243770991268, 0.599681752622167719,
        0.604555390700549533, 0.609468064928895381, 0.614420723892076803, 0.619414360609039205,
        0.624450015550274240, 0.629528779928128279, 0.634651799290960050, 0.639820277456438991,
        0.645035480824251883, 0.650298743114294586, 0.655611470583224665, 0.660975147780241357,
        0.666391343912380640, 0.671861719900766374, 0.677388036222513090, 0.682972161648791376,
        0.688616083008527058, 0.694321916130032579, 0.700091918140490099, 0.705928501336797409,
        0.711834248882358467, 0.717811932634901395, 0.723864533472881599, 0.729995264565802437,
        0.736207598131266683, 0.742505296344636245, 0.748892447223726720, 0.755373506511754500,
        0.761953346841546475, 0.768637315803334831, 0.775431304986138326, 0.782341832659861902,
        0.789376143571198563, 0.796542330428254619, 0.803849483176389490, 0.811307874318219935,
        0.818929191609414797, 0.826726833952094231, 0.834716292992930375, 0.842915653118441077,
        0.851346258465123684, 0.860033621203008636, 0.869008688043793165, 0.878309655816146839,
        0.887984660763399880, 0.898095921906304051, 0.908726440060562912, 0.919991505048360247,
        0.932060075968990209, 0.945198953453078028, 0.959879091812415930, 0.977101701282731328,
        1.000000000000000000
    };
    inline constexpr double ZIG_EXP_R = 7.697117470131050077;
    inline constexpr std::array<double, 257> ZIG_EXP_X = {
        8.697117470131052741, 7.697117470131050077, 6.941033629377212577, 6.478378493832569696,
        6.144164665772472667, 5.882144315795399869, 5.666410167454033697, 5.482890627526062488,
        5.323090505754398016, 5.181487281301500047, 5.054288489981304089, 4.938777085901250530,
        4.832939741025112035, 4.735242996601741083, 4.644491885420085175, 4.559737061707351380,
        4.480211746528421912, 4.405287693473573185, 4.334443680317273007, 4.267242480277365857,
        4.203313713735184365, 4.142340865664051464, 4.084051310408297830, 4.028208544647936762,
        3.974606066673788796, 3.923062500135489739, 3.873417670399509127, 3.825529418522336744,
        3.779270992411667862, 3.734528894039797375, 3.691201090237418825, 3.649195515760853770,
        3.608428813128909507, 3.568825265648337020, 3.530315889129343354, 3.492837654774059608,
        3.456332821132760191, 3.420748357251119920, 3.386035442460300970, 3.352149030900109405,
        3.319047470970748037, 3.286692171599068679, 3.255047308570449882, 3.224079565286264160,
        3.193757903212240290, 3.164053358025972873, 3.134938858084440394, 3.106389062339824481,
        3.078380215254090224, 3.050890016615455114, 3.023897504455676621, 2.997382949516130601,
        2.971327759921089662, 2.945714394895045718, 2.920526286512740821, 2.895747768600141825,
        2.871364012015536371, 2.847360965635188812, 2.823725302450035279, 2.800444370250737780,
        2.777506146439756574, 2.754899196562344610, 2.732612636194700073, 2.710636095867928752,
        2.688959688741803689, 2.667573980773266573, 2.646469963151809157, 2.625639026797788489,
        2.605072938740835564, 2.584763820214140750, 2.564704126316905253, 2.544886627111869970,
        2.525304390037828028, 2.505950763528594027, 2.486819361740209455, 2.467904050297364815,
        2.449198932978249754, 2.430698339264419694, 2.412396812688870629, 2.394289099921457886,
        2.376370140536140596, 2.358635057409337321, 2.341079147703034380, 2.323697874390196372,
        2.306486858283579799, 2.289441870532269441, 2.272558825553154804, 2.255833774367219213,
        2.239262898312909034, 2.222842503111036816, 2.206569013257663858, 2.190438966723220027,
        2.174449009937774679, 2.158595893043885994, 2.142876465399842001, 2.127287671317368289,
        2.111826546019042183, 2.096490211801715020, 2.081275874393225145, 2.066180819490575526,
        2.051202409468584786, 2.036338080248769611, 2.021585338318926173, 2.006941757894518563,
        1.992404978213576650, 1.977972700957360441, 1.963642687789548313, 1.949412758007184943,
        1.935280786297051359, 1.921244700591528076, 1.907302480018387536, 1.893452152939308242,
        1.879691795072211180, 1.866019527692827973, 1.852433515911175554, 1.838931967018879954,
        1.825513128903519799, 1.812175288526390649, 1.798916770460290859, 1.785735935484126014,
        1.772631179231305643, 1.759600930889074766, 1.746643651946074405, 1.733757834985571566,
        1.720942002521935299, 1.708194705878057773, 1.695514524101537912, 1.682900062917553896,
        1.670349953716452118, 1.657862852574172763, 1.645437439303723659, 1.633072416535991334,
        1.620766508828257901, 1.608518461798858379, 1.596327041286483395, 1.584191032532688892,
        1.572109239386229707, 1.560080483527888084, 1.548103603714513499, 1.536177455041032092,
        1.524300908219226258, 1.512472848872117082, 1.500692176842816750, 1.488957805516746058,
        1.477268661156133867, 1.465623682245745352, 1.454021818848793446, 1.442462031972012504,
        1.430943292938879674, 1.419464582769983219, 1.408024891569535697, 1.396623217917042137,
        1.385258568263121992, 1.373929956328490576, 1.362636402505086775, 1.351376933258335189,
        1.340150580529504643, 1.328956381137116560, 1.317793376176324749, 1.306660610415174117,
        1.295557131686601027, 1.284481990275012642, 1.273434238296241139, 1.262412929069615330,
        1.251417116480852521, 1.240445854334406572, 1.229498195693849105, 1.218573192208790124,
        1.207669893426761121, 1.196787346088403092, 1.185924593404202199, 1.175080674310911677,
        1.164254622705678921, 1.153445466655774743, 1.142652227581672841, 1.131873919411078511,
        1.121109547701330200, 1.110358108727411031, 1.099618588532597308, 1.088889961938546813,
        1.078171191511372307, 1.067461226479967662, 1.056759001602551429, 1.046063435977044209,
        1.035373431790528542, 1.024687873002617211, 1.014005623957096480, 1.003325527915696735,
        0.992646405507275897, 0.981967053085062602, 0.971286240983903260, 0.960602711668666509,
        0.949915177764075969, 0.939222319955262286, 0.928522784747210395, 0.917815182070044311,
        0.907098082715690257, 0.896370015589889935, 0.885629464761751528, 0.874874866291025066,
        0.864104604811004484, 0.853317009842373353, 0.842510351810368485, 0.831682837734273206,
        0.820832606554411814, 0.809957724057418282, 0.799056177355487174, 0.788125868869492430,
        0.777164609759129710, 0.766170112735434672, 0.755139984181982249, 0.744071715500508102,
        0.732962673584365398, 0.721810090308756203, 0.710611050909655040, 0.699362481103231959,
        0.688061132773747808, 0.676703568029522584, 0.665286141392677943, 0.653804979847664947,
        0.642255960424536365, 0.630634684933490286, 0.618936451394876075, 0.607156221620300030,
        0.595288584291502887, 0.583327712748769489, 0.571267316532588332, 0.559100585511540626,
        0.546820125163310577, 0.534417881237165604, 0.521885051592135052, 0.509211982443654398,
        0.496388045518671162, 0.483401491653461857, 0.470239275082169006, 0.456886840931420235,
        0.443327866073552401, 0.429543940225410703, 0.415514169600356364, 0.401214678896277765,
        0.386617977941119573, 0.371692145329917234, 0.356399760258393816, 0.340696481064849122,
        0.324529117016909452, 0.307832954674932158, 0.290527955491230394, 0.272513185478464703,
        0.253658363385912022, 0.233790483059674731, 0.212671510630966620, 0.189958689622431842,
        0.165127622564187282, 0.137304980940012589, 0.104838507565818778, 0.063852163815001570,
        0.000000000000000000
    };
    inline constexpr std::array<double, 257> ZIG_EXP_F = {
        0.000167066692307963, 0.000454134353841497, 0.000967269282327174, 0.001536299780301573,
        0.002145967743718907, 0.002788798793574076, 0.003460264777836904, 0.004157295120833797,
        0.004877655983542396, 0.005619642207205489, 0.006381905937319183, 0.007163353183634991,
        0.007963077438017043, 0.008780314985808977, 0.009614413642502212, 0.010464810181029981,
        0.011331013597834600, 0.012212592426255378, 0.013109164931254991, 0.014020391403181943,
        0.014945968011691148, 0.015885621839973156, 0.016839106826039941, 0.017806200410911355,
        0.018786700744696024, 0.019780424338009740, 0.020787204072578114, 0.021806887504283581,
        0.022839335406385240, 0.023884420511558174, 0.024942026419731787, 0.026012046645134221,
        0.027094383780955803, 0.028188948763978646, 0.029295660224637411, 0.030414443910466622,
        0.031545232172893622, 0.032687963508959555, 0.033842582150874358, 0.035009037697397431,
        0.036187284781931443, 0.037377282772959382, 0.038578995503074871, 0.039792391023374139,
        0.041017441380414840, 0.042254122413316254, 0.043502413568888197, 0.044762297732943289,
        0.046033761076175184, 0.047316792913181561, 0.048611385573379504, 0.049917534282706379,
        0.051235237055126281, 0.052564494593071685, 0.053905310196046080, 0.055257689676697030,
        0.056621641283742870, 0.057997175631200659, 0.059384305633420280, 0.060783046445479660,
        0.062193415408541036, 0.063615431999807376, 0.065049117786753805, 0.066494496385339816,
        0.067951593421936643, 0.069420436498728783, 0.070901055162371843, 0.072393480875708752,
        0.073897746992364746, 0.075413888734058410, 0.076941943170480517, 0.078481949201606435,
        0.080033947542319905, 0.081597980709237419, 0.083174093009632397, 0.084762330532368146,
        0.086362741140756927, 0.087975374467270231, 0.089600281910032886, 0.091237516631040197,
        0.092887133556043569, 0.094549189376055873, 0.096223742550432825, 0.097910853311492213,
        0.099610583670637132, 0.101322997425953631, 0.103048160171257702, 0.104786139306570145,
        0.106537004050001632, 0.108300825451033755, 0.110077676405185357, 0.111867631670056283,
        0.113670767882744286, 0.115487163578633506, 0.117316899211555525, 0.119160057175327641,
        0.121016721826674792, 0.122886979509545108, 0.124770918580830933, 0.126668629437510671,
        0.128580204545228199, 0.130505738468330773, 0.132445327901387494, 0.134399071702213602,
        0.136367070926428829, 0.138349428863580176, 0.140346251074862399, 0.142357645432472146,
        0.144383722160634720, 0.146424593878344889, 0.148480375643866735, 0.150551185001039839,
        0.152637142027442801, 0.154738369384468027, 0.156854992369365148, 0.158987138969314129,
        0.161134939917591952, 0.163298528751901734, 0.165478041874935922, 0.167673618617250081,
        0.169885401302527550, 0.172113535315319977, 0.174358169171353411, 0.176619454590494829,
        0.178897546572478278, 0.181192603475496261, 0.183504787097767436, 0.185834262762197083,
        0.188181199404254262, 0.190545769663195363, 0.192928149976771296, 0.195328520679563189,
        0.197747066105098818, 0.200183974691911210, 0.202639439093708962, 0.205113656293837654,
        0.207606827724221982, 0.210119159388988230, 0.212650861992978224, 0.215202151075378628,
        0.217773247148700472, 0.220364375843359439, 0.222975768058120111, 0.225607660116683956,
        0.228260293930716618, 0.230933917169627356, 0.233628783437433291, 0.236345152457059560,
        0.239083290262449094, 0.241843469398877131, 0.244625969131892024, 0.247431075665327543,
        0.250259082368862240, 0.253110290015629402, 0.255985007030415324, 0.258883549749016173,
        0.261806242689362922, 0.264753418835062149, 0.267725419932044739, 0.270722596799059967,
        0.273745309652802915, 0.276793928448517301, 0.279868833236972869, 0.282970414538780746,
        0.286099073737076826, 0.289255223489677693, 0.292439288161892630, 0.295651704281261252,
        0.298892921015581847, 0.302163400675693528, 0.305463619244590256, 0.308794066934560185,
        0.312155248774179606, 0.315547685227128949, 0.318971912844957239, 0.322428484956089223,
        0.325917972393556354, 0.329440964264136438, 0.332998068761809096, 0.336589914028677717,
        0.340217149066780189, 0.343880444704502575, 0.347580494621637148, 0.351318016437483449,
        0.355093752866787626, 0.358908472948750001, 0.362762973354817997, 0.366658079781514379,
        0.370594648435146223, 0.374573567615902381, 0.378595759409581067, 0.382662181496010056,
        0.386773829084137932, 0.390931736984797384, 0.395136981833290435, 0.399390684475231350,
        0.403694012530530555, 0.408048183152032673, 0.412454465997161457, 0.416914186433003209,
        0.421428728997616908, 0.425999541143034677, 0.430628137288459167, 0.435316103215636907,
        0.440065100842354173, 0.444876873414548846, 0.449753251162755330, 0.454696157474615836,
        0.459707615642138023, 0.464789756250426511, 0.469944825283960310, 0.475175193037377708,
        0.480483363930454543, 0.485871987341885248, 0.491343869594032867, 0.496901987241549881,
        0.502549501841348056, 0.508289776410643213, 0.514126393814748894, 0.520063177368233931,
        0.526104213983620062, 0.532253880263043655, 0.538516872002862246, 0.544898237672440056,
        0.551403416540641733, 0.558038282262587892, 0.564809192912400615, 0.571723048664826150,
        0.578787358602845359, 0.586010318477268366, 0.593400901691733762, 0.600968966365232560,
        0.608725382079622346, 0.616682180915207878, 0.624852738703666200, 0.633251994214366398,
        0.641896716427266423, 0.650805833414571433, 0.660000841079000145, 0.669506316731925177,
        0.679350572264765806, 0.689566496117078431, 0.700192655082788606, 0.711274760805076456,
        0.722867659593572465, 0.735038092431424039, 0.747868621985195658, 0.761463388849896838,
        0.775956852040116218, 0.791527636972496285, 0.808421651523009044, 0.826993296643051101,
        0.847785500623990496, 0.871704332381204705, 0.900469929925747703, 0.938143680862176477,
        1.000000000000000000
    };
} // namespace ziggurat_tables

namespace detail {

template <class P, class Z>
inline double ziggurat(ChaCha20Rng& rng, bool symmetric, const std::array<double, 257>& x_tab,
                       const std::array<double, 257>& f_tab, P&& pdf, Z&& zero_case) {
    while (true) {
        u64 bits = rng.next_u64();
        std::size_t i = static_cast<std::size_t>(bits & 0xffu);
        double u = symmetric
            ? (into_float_with_exponent<double>(bits >> 12, 1) - 3.0)
            : (into_float_with_exponent<double>(bits >> 12, 0) - (1.0 - std::numeric_limits<double>::epsilon() / 2.0));
        double x = u * x_tab[i];
        double test_x = symmetric ? std::abs(x) : x;
        if (test_x < x_tab[i + 1]) return x;
        if (i == 0) return zero_case(rng, u);
        if (f_tab[i + 1] + (f_tab[i] - f_tab[i + 1]) * rng.gen<double>() < pdf(x)) return x;
    }
}

} // namespace detail

inline double StandardNormal::sample_f64(ChaCha20Rng& rng) {
    auto pdf = [](double x) -> double {
        return std::exp(-x * x / 2.0);
    };
    auto zero_case = [](ChaCha20Rng& rng2, double u) -> double {
        double x = 1.0;
        double y = 0.0;
        while (-2.0 * y < x * x) {
            double x_ = StandardNormal::open01(rng2);
            double y_ = StandardNormal::open01(rng2);
            x = std::log(x_) / ziggurat_tables::ZIG_NORM_R;
            y = std::log(y_);
        }
        if (u < 0.0) return x - ziggurat_tables::ZIG_NORM_R;
        return ziggurat_tables::ZIG_NORM_R - x;
    };
    return detail::ziggurat(rng, true, ziggurat_tables::ZIG_NORM_X, ziggurat_tables::ZIG_NORM_F, pdf, zero_case);
}

inline double Exp1::sample_f64(ChaCha20Rng& rng) {
    auto pdf = [](double x) -> double {
        return std::exp(-x);
    };
    auto zero_case = [](ChaCha20Rng& rng2, double) -> double {
        return ziggurat_tables::ZIG_EXP_R - std::log(rng2.gen<double>());
    };
    return detail::ziggurat(
        rng,
        false,
        ziggurat_tables::ZIG_EXP_X,
        ziggurat_tables::ZIG_EXP_F,
        pdf,
        zero_case);
}

// rand::seq 系 helper 群。
namespace seq {

// rand::seq::index 系 helper 群。
namespace index {

inline std::size_t gen_index(ChaCha20Rng& rng, std::size_t ubound) {
    if (ubound <= std::numeric_limits<u32>::max()) {
        return static_cast<std::size_t>(rng.gen_range<u32>(0, static_cast<u32>(ubound)));
    }
    return rng.gen_range<std::size_t>(0, ubound);
}

inline std::vector<std::size_t> sample_floyd(ChaCha20Rng& rng, u32 length, u32 amount) {
    bool floyd_shuffle = amount < 50;
    std::vector<u32> indices;
    indices.reserve(amount);
    for (u32 j = length - amount; j < length; ++j) {
        u32 t = rng.gen_range_inclusive<u32>(0, j);
        if (floyd_shuffle) {
            auto it = std::find(indices.begin(), indices.end(), t);
            if (it != indices.end()) {
                indices.insert(it, j);
                continue;
            }
        } else if (std::find(indices.begin(), indices.end(), t) != indices.end()) {
            indices.push_back(j);
            continue;
        }
        indices.push_back(t);
    }
    if (!floyd_shuffle) {
        for (u32 i = amount - 1; i >= 1; --i) {
            std::swap(indices[std::size_t(i)], indices[std::size_t(rng.gen_range_inclusive<u32>(0, i))]);
            if (i == 1) break;
        }
    }
    std::vector<std::size_t> out(indices.begin(), indices.end());
    return out;
}

inline std::vector<std::size_t> sample_inplace(ChaCha20Rng& rng, u32 length, u32 amount) {
    std::vector<u32> indices(length);
    for (u32 i = 0; i < length; ++i) indices[i] = i;
    for (u32 i = 0; i < amount; ++i) {
        u32 j = rng.gen_range<u32>(i, length);
        std::swap(indices[i], indices[j]);
    }
    indices.resize(amount);
    return std::vector<std::size_t>(indices.begin(), indices.end());
}

template <class X = std::size_t>
inline std::vector<std::size_t> sample_rejection(ChaCha20Rng& rng, std::size_t length, std::size_t amount) {
    std::vector<std::size_t> cache;
    cache.reserve(amount);
    Uniform<std::size_t> distr(0, length);
    std::vector<std::size_t> indices;
    indices.reserve(amount);
    while (indices.size() < amount) {
        std::size_t pos = distr.sample(rng);
        if (std::find(cache.begin(), cache.end(), pos) == cache.end()) {
            cache.push_back(pos);
            indices.push_back(pos);
        }
    }
    return indices;
}

inline std::vector<std::size_t> sample(ChaCha20Rng& rng, std::size_t length, std::size_t amount) {
    if (amount > length) throw std::invalid_argument("index::sample: amount > length");
    if (length > std::numeric_limits<u32>::max()) {
        return sample_rejection(rng, length, amount);
    }
    u32 a = static_cast<u32>(amount);
    u32 l = static_cast<u32>(length);
    if (a < 163) {
        constexpr float C[2][2] = {{1.6f, 8.0f / 45.0f}, {10.0f, 70.0f / 9.0f}};
        int j = l < 500000 ? 0 : 1;
        float amount_fp = static_cast<float>(a);
        float m4 = C[0][j] * amount_fp;
        if (a > 11 && static_cast<float>(l) < (C[1][j] + m4) * amount_fp) return sample_inplace(rng, l, a);
        return sample_floyd(rng, l, a);
    }
    constexpr float C[2] = {270.0f, 330.0f / 9.0f};
    int j = l < 500000 ? 0 : 1;
    if (static_cast<float>(l) < C[j] * static_cast<float>(a)) return sample_inplace(rng, l, a);
    return sample_rejection(rng, l, a);
}

template <class WeightFn>
inline std::vector<std::size_t> sample_weighted(ChaCha20Rng& rng, std::size_t length, WeightFn&& weight, std::size_t amount) {
    if (amount == 0) return {};
    if (amount > length) throw std::invalid_argument("index::sample_weighted: amount > length");

    struct Elem {
        std::size_t index;
        double key;
        bool operator<(const Elem& other) const noexcept {
            return key < other.key;
        }
    };

    std::priority_queue<Elem> candidates;
    for (std::size_t index = 0; index < length; ++index) {
        double w = static_cast<double>(weight(index));
        if (!(w >= 0.0)) throw std::invalid_argument("index::sample_weighted: invalid weight");
        double key = std::pow(rng.gen<double>(), 1.0 / w);
        candidates.push(Elem{index, key});
    }

    std::vector<std::size_t> out;
    out.reserve(amount);
    while (out.size() < amount) {
        out.push_back(candidates.top().index);
        candidates.pop();
    }
    return out;
}

} // namespace index

template <class T>
inline void shuffle(std::span<T> s, ChaCha20Rng& rng) {
    for (std::size_t i = s.size(); i > 1; --i) {
        std::swap(s[i - 1], s[index::gen_index(rng, i)]);
    }
}

template <class T>
inline std::pair<std::span<T>, std::span<T>> partial_shuffle(std::span<T> s, ChaCha20Rng& rng, std::size_t amount) {
    if (amount >= s.size()) {
        amount = s.size();
    }
    std::size_t end = s.size() - amount;
    for (std::size_t i = s.size(); i > end; --i) {
        std::swap(s[i - 1], s[index::gen_index(rng, i)]);
    }
    return {std::span<T>(s.data() + end, s.size() - end), std::span<T>(s.data(), end)};
}

template <class T>
inline const T* choose(std::span<const T> s, ChaCha20Rng& rng) {
    if (s.empty()) return nullptr;
    return &s[index::gen_index(rng, s.size())];
}

template <class T>
inline T* choose(std::span<T> s, ChaCha20Rng& rng) {
    if (s.empty()) return nullptr;
    return &s[index::gen_index(rng, s.size())];
}

template <class T, class WeightFn>
inline const T* choose_weighted(std::span<const T> s, ChaCha20Rng& rng, WeightFn&& weight_fn) {
    if (s.empty()) throw std::invalid_argument("choose_weighted: empty slice");
    using W = std::invoke_result_t<WeightFn, const T&>;
    std::vector<W> weights;
    weights.reserve(s.size());
    for (const auto& x : s) weights.push_back(weight_fn(x));
    WeightedIndex<W> dist(weights);
    return &s[dist.sample(rng)];
}

template <class T>
inline std::vector<const T*> choose_multiple(std::span<const T> s, ChaCha20Rng& rng, std::size_t amount) {
    if (amount > s.size()) amount = s.size();
    auto idx = index::sample(rng, s.size(), amount);
    std::vector<const T*> out;
    out.reserve(idx.size());
    for (std::size_t i : idx) out.push_back(&s[i]);
    return out;
}

namespace iter {

template <std::forward_iterator Iter, class Sent>
inline Iter choose_stable(Iter first, Sent last, ChaCha20Rng& rng);

// choose は Rust の IteratorRandom::choose を意識した実装で、
// 長さが事前に分かる範囲では 1 回の乱数で選ぶ。
// 長さが分からない範囲では size_hint 非依存の安定版に寄せる。
template <std::forward_iterator Iter, class Sent>
inline Iter choose(Iter first, Sent last, ChaCha20Rng& rng) {
    if (first == last) return first;
    if constexpr (std::sized_sentinel_for<Sent, Iter>) {
        auto diff = last - first;
        std::size_t n = static_cast<std::size_t>(diff);
        std::advance(first, static_cast<std::ptrdiff_t>(index::gen_index(rng, n)));
        return first;
    }
    return choose_stable(first, last, rng);
}

template <std::forward_iterator Iter, class Sent>
inline Iter choose_stable(Iter first, Sent last, ChaCha20Rng& rng) {
    std::size_t consumed = 0;
    std::optional<Iter> result;

    auto nth_consume = [&](std::size_t n) -> std::optional<Iter> {
        while (n > 0 && first != last) {
            ++first;
            --n;
        }
        if (first == last) return std::nullopt;
        Iter out = first;
        ++first;
        return out;
    };

    while (true) {
        // Rust の IteratorRandom::choose_stable は、size_hint().0 を使って
        // 「最低でもこれだけ要素がある」範囲をまとめて処理する。
        // C++ 標準 iterator には一般の size_hint が無いため、
        // sized_sentinel_for のときだけ lower bound を正確に再現し、
        // それ以外は lower = 0 として 1 要素ずつ処理する。
        std::size_t next = 0;
        std::size_t lower = 0;
        if constexpr (std::sized_sentinel_for<Sent, Iter>) {
            lower = static_cast<std::size_t>(last - first);
        }

        if (lower >= 2) {
            std::optional<std::size_t> highest_selected;
            for (std::size_t ix = 0; ix < lower; ++ix) {
                if (index::gen_index(rng, consumed + ix + 1) == 0) {
                    highest_selected = ix;
                }
            }

            consumed += lower;
            next = lower;

            if (highest_selected.has_value()) {
                result = nth_consume(*highest_selected);
                next -= *highest_selected + 1;
                assert(result.has_value() && "choose_stable: iterator shorter than lower bound");
            }
        }

        auto elem = nth_consume(next);
        if (!elem.has_value()) {
            return result.value_or(first);
        }

        if (index::gen_index(rng, consumed + 1) == 0) {
            result = elem;
        }
        ++consumed;
    }
}

} // namespace iter

} // namespace seq

// noise::PermutationTable 互換実装。
class PermutationTable {
public:
    using result_type = PermutationTable;
    static constexpr std::size_t TABLE_SIZE = 256;

    PermutationTable() {
        for (std::size_t i = 0; i < TABLE_SIZE; ++i) values_[i] = static_cast<u8>(i);
    }

    static PermutationTable sample(ChaCha20Rng& rng) {
        PermutationTable pt;
        seq::shuffle<u8>(std::span<u8>(pt.values_.data(), pt.values_.size()), rng);
        return pt;
    }

    static PermutationTable sample(XorShiftRng& rng) {
        PermutationTable pt;
        for (std::size_t i = pt.values_.size(); i > 1; --i) {
            u32 j = rng.gen_range(0, static_cast<u32>(i));
            std::swap(pt.values_[i - 1], pt.values_[j]);
        }
        return pt;
    }

    static PermutationTable new_with_seed(u32 seed) {
        std::array<u8, 16> real{};
        real[0] = 1;
        for (int i = 1; i < 4; ++i) {
            real[i * 4 + 0] = static_cast<u8>(seed);
            real[i * 4 + 1] = static_cast<u8>(seed >> 8);
            real[i * 4 + 2] = static_cast<u8>(seed >> 16);
            real[i * 4 + 3] = static_cast<u8>(seed >> 24);
        }
        XorShiftRng rng(real);
        return sample(rng);
    }

    explicit PermutationTable(u32 seed) : values_(new_with_seed(seed).values_) {}

    std::size_t hash(std::span<const std::ptrdiff_t> to_hash) const {
        if (to_hash.empty()) throw std::invalid_argument("PermutationTable::hash: empty input");
        std::size_t acc = static_cast<std::size_t>(to_hash[0] & 0xff);
        for (std::size_t i = 1; i < to_hash.size(); ++i) {
            std::size_t b = static_cast<std::size_t>(to_hash[i] & 0xff);
            acc = static_cast<std::size_t>(values_[acc]) ^ b;
        }
        return values_[acc];
    }

    const std::array<u8, TABLE_SIZE>& values() const { return values_; }

private:
    std::array<u8, TABLE_SIZE> values_;
};

namespace detail {

inline double quintic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
inline double lerp(double a, double b, double t) {
    // noise 0.9.0 の interpolate::linear に合わせる。
    return b * t + a * (1.0 - t);
}

} // namespace detail

// noise::Perlin 互換実装。
class Perlin {
public:
    static constexpr u32 DEFAULT_SEED = 0;

    explicit Perlin(u32 seed = DEFAULT_SEED) : seed_(seed), perm_table_(seed) {}

    Perlin set_seed(u32 seed) const {
        if (seed_ == seed) return *this;
        return Perlin(seed);
    }

    u32 seed() const { return seed_; }

    double get(const std::array<double, 1>& point) const {
        constexpr double SCALE_FACTOR = 2.0;
        std::ptrdiff_t corner = static_cast<std::ptrdiff_t>(point[0]);
        double distance = point[0] - static_cast<double>(corner);
        auto call_gradient = [&](std::ptrdiff_t xoff) -> double {
            double offset = distance - static_cast<double>(xoff);
            std::array<std::ptrdiff_t, 1> p{corner + xoff};
            switch (perm_table_.hash(std::span<const std::ptrdiff_t>(p.data(), 1)) & 0b1) {
                case 0: return offset;
                case 1: return -offset;
            }
            return 0.0;
        };
        double g0 = call_gradient(0);
        double g1 = call_gradient(1);
        double curve = detail::quintic(distance);
        double result = detail::lerp(g0, g1, curve) * SCALE_FACTOR;
        return std::clamp(result, -1.0, 1.0);
    }

    double get(const std::array<double, 2>& point) const {
        constexpr double SCALE_FACTOR = 2.0 / std::numbers::sqrt2;
        std::ptrdiff_t cx = static_cast<std::ptrdiff_t>(std::floor(point[0]));
        std::ptrdiff_t cy = static_cast<std::ptrdiff_t>(std::floor(point[1]));
        double dx = point[0] - static_cast<double>(cx);
        double dy = point[1] - static_cast<double>(cy);
        auto call_gradient = [&](std::ptrdiff_t xo, std::ptrdiff_t yo) -> double {
            double px = dx - static_cast<double>(xo);
            double py = dy - static_cast<double>(yo);
            std::array<std::ptrdiff_t, 2> p{cx + xo, cy + yo};
            switch (perm_table_.hash(std::span<const std::ptrdiff_t>(p.data(), 2)) & 0b11) {
                case 0: return  px + py;
                case 1: return -px + py;
                case 2: return  px - py;
                case 3: return -px - py;
            }
            return 0.0;
        };
        double g00 = call_gradient(0, 0);
        double g10 = call_gradient(1, 0);
        double g01 = call_gradient(0, 1);
        double g11 = call_gradient(1, 1);
        double cxq = detail::quintic(dx);
        double cyq = detail::quintic(dy);
        double result = detail::lerp(detail::lerp(g00, g01, cyq), detail::lerp(g10, g11, cyq), cxq) * SCALE_FACTOR;
        return std::clamp(result, -1.0, 1.0);
    }

    double get(const std::array<double, 3>& point) const {
        constexpr double SCALE_FACTOR = 1.1547005383792515;
        std::ptrdiff_t cx = static_cast<std::ptrdiff_t>(std::floor(point[0]));
        std::ptrdiff_t cy = static_cast<std::ptrdiff_t>(std::floor(point[1]));
        std::ptrdiff_t cz = static_cast<std::ptrdiff_t>(std::floor(point[2]));
        double dx = point[0] - static_cast<double>(cx);
        double dy = point[1] - static_cast<double>(cy);
        double dz = point[2] - static_cast<double>(cz);
        auto call_gradient = [&](std::ptrdiff_t xo, std::ptrdiff_t yo, std::ptrdiff_t zo) -> double {
            double px = dx - static_cast<double>(xo);
            double py = dy - static_cast<double>(yo);
            double pz = dz - static_cast<double>(zo);
            std::array<std::ptrdiff_t, 3> p{cx + xo, cy + yo, cz + zo};
            switch (perm_table_.hash(std::span<const std::ptrdiff_t>(p.data(), 3)) & 0b1111) {
                case 0: case 12: return  px + py;
                case 1: case 13: return -px + py;
                case 2: return  px - py;
                case 3: return -px - py;
                case 4: return  px + pz;
                case 5: return -px + pz;
                case 6: return  px - pz;
                case 7: return -px - pz;
                case 8: return  py + pz;
                case 9: case 14: return -py + pz;
                case 10: return py - pz;
                case 11: case 15: return -py - pz;
            }
            return 0.0;
        };
        double g000 = call_gradient(0,0,0);
        double g100 = call_gradient(1,0,0);
        double g010 = call_gradient(0,1,0);
        double g110 = call_gradient(1,1,0);
        double g001 = call_gradient(0,0,1);
        double g101 = call_gradient(1,0,1);
        double g011 = call_gradient(0,1,1);
        double g111 = call_gradient(1,1,1);
        double cxq = detail::quintic(dx);
        double cyq = detail::quintic(dy);
        double czq = detail::quintic(dz);
        double result = detail::lerp(
            detail::lerp(detail::lerp(g000, g001, czq), detail::lerp(g010, g011, czq), cyq),
            detail::lerp(detail::lerp(g100, g101, czq), detail::lerp(g110, g111, czq), cyq),
            cxq) * SCALE_FACTOR;
        return std::clamp(result, -1.0, 1.0);
    }

    double get(const std::array<double, 4>& point) const {
        constexpr double SCALE_FACTOR = 1.0;
        std::ptrdiff_t cx = static_cast<std::ptrdiff_t>(std::floor(point[0]));
        std::ptrdiff_t cy = static_cast<std::ptrdiff_t>(std::floor(point[1]));
        std::ptrdiff_t cz = static_cast<std::ptrdiff_t>(std::floor(point[2]));
        std::ptrdiff_t cw = static_cast<std::ptrdiff_t>(std::floor(point[3]));
        double dx = point[0] - static_cast<double>(cx);
        double dy = point[1] - static_cast<double>(cy);
        double dz = point[2] - static_cast<double>(cz);
        double dw = point[3] - static_cast<double>(cw);
        auto call_gradient = [&](std::ptrdiff_t xo, std::ptrdiff_t yo, std::ptrdiff_t zo, std::ptrdiff_t wo) -> double {
            double px = dx - static_cast<double>(xo);
            double py = dy - static_cast<double>(yo);
            double pz = dz - static_cast<double>(zo);
            double pw = dw - static_cast<double>(wo);
            std::array<std::ptrdiff_t, 4> p{cx + xo, cy + yo, cz + zo, cw + wo};
            switch (perm_table_.hash(std::span<const std::ptrdiff_t>(p.data(), 4)) & 0b11111) {
                case 0: case 28: return  px + py + pz;
                case 1: return -px + py + pz;
                case 2: return  px - py + pz;
                case 3: return  px + py - pz;
                case 4: return -px + py - pz;
                case 5: return  px - py - pz;
                case 6: return  px - py - pz;
                case 7: case 29: return px + py + pw;
                case 8: return -px + py + pw;
                case 9: return  px - py + pw;
                case 10: return px + py - pw;
                case 11: return px + py - pw;
                case 12: return px + py - pw;
                case 13: return -px - py - pw;
                case 14: case 30: return px + pz + pw;
                case 15: return -px + pz + pw;
                case 16: return  px - pz + pw;
                case 17: return  px + pz - pw;
                case 18: return  px + pz - pw;
                case 19: return  px + pz - pw;
                case 20: return -px - pz - pw;
                case 21: case 31: return py + pz + pw;
                case 22: return -py + pz + pw;
                case 23: return  py - pz + pw;
                case 24: return  py - pz - pw;
                case 25: return -py - pz - pw;
                case 26: return  py - pz - pw;
                case 27: return -py - pz - pw;
            }
            return 0.0;
        };
        double g0000 = call_gradient(0,0,0,0);
        double g1000 = call_gradient(1,0,0,0);
        double g0100 = call_gradient(0,1,0,0);
        double g1100 = call_gradient(1,1,0,0);
        double g0010 = call_gradient(0,0,1,0);
        double g1010 = call_gradient(1,0,1,0);
        double g0110 = call_gradient(0,1,1,0);
        double g1110 = call_gradient(1,1,1,0);
        double g0001 = call_gradient(0,0,0,1);
        double g1001 = call_gradient(1,0,0,1);
        double g0101 = call_gradient(0,1,0,1);
        double g1101 = call_gradient(1,1,0,1);
        double g0011 = call_gradient(0,0,1,1);
        double g1011 = call_gradient(1,0,1,1);
        double g0111 = call_gradient(0,1,1,1);
        double g1111 = call_gradient(1,1,1,1);
        double cxq = detail::quintic(dx);
        double cyq = detail::quintic(dy);
        double czq = detail::quintic(dz);
        double cwq = detail::quintic(dw);
        double result = detail::lerp(
            detail::lerp(
                detail::lerp(detail::lerp(g0000, g0001, cwq), detail::lerp(g0010, g0011, cwq), czq),
                detail::lerp(detail::lerp(g0100, g0101, cwq), detail::lerp(g0110, g0111, cwq), czq),
                cyq),
            detail::lerp(
                detail::lerp(detail::lerp(g1000, g1001, cwq), detail::lerp(g1010, g1011, cwq), czq),
                detail::lerp(detail::lerp(g1100, g1101, cwq), detail::lerp(g1110, g1111, cwq), czq),
                cyq),
            cxq) * SCALE_FACTOR;
        return std::clamp(result, -1.0, 1.0);
    }

private:
    u32 seed_;
    PermutationTable perm_table_;
};

} // namespace rust_rand


#if __INCLUDE_LEVEL__ == 0

#include <iostream>
#include <map>
#include <numeric>
#include <string_view>

namespace rust_rand::self_test {

using namespace rust_rand;

inline void require(bool cond, std::string_view msg) {
    if (!cond) throw std::runtime_error(std::string(msg));
}

template <class T>
inline std::string hx(T value) {
    return detail::hex_bits(value);
}

template <class T>
inline void print_items(const std::vector<T>& v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << v[i];
    }
}

template <class T>
inline void print_items_hex(const std::vector<T>& v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << hx(v[i]);
    }
}

template <class T, std::size_t N>
inline void print_items(const std::array<T, N>& v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << v[i];
    }
}

template <class T, std::size_t N>
inline void print_items_hex(const std::array<T, N>& v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << hx(v[i]);
    }
}

template <class T>
inline void print_items(std::span<const T> v) {
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << v[i];
    }
}

template <class T>
inline void print_line(std::string_view label, const std::vector<T>& v) {
    std::cout << label;
    if (!v.empty()) std::cout << ' ';
    print_items(v);
    std::cout << "\n";
}

template <class T, std::size_t N>
inline void print_line(std::string_view label, const std::array<T, N>& v) {
    std::cout << label;
    if (!v.empty()) std::cout << ' ';
    print_items(v);
    std::cout << "\n";
}

template <class T, std::size_t N>
inline void print_line_hex(std::string_view label, const std::array<T, N>& v) {
    std::cout << label;
    if (!v.empty()) std::cout << ' ';
    print_items_hex(v);
    std::cout << "\n";
}

template <class T>
inline void print_line_hex(std::string_view label, const std::vector<T>& v) {
    std::cout << label;
    if (!v.empty()) std::cout << ' ';
    print_items_hex(v);
    std::cout << "\n";
}

inline std::string opt_value(const int* p) {
    return p ? std::to_string(*p) : "NONE";
}

template <class Iter, class Sent>
inline std::string opt_value_iter(Iter it, Sent last) {
    if (it == last) return "NONE";
    return std::to_string(*it);
}

inline std::string pt_singletons_hex_safe(const PermutationTable& pt) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 256; ++i) {
        std::ptrdiff_t x = i;
        oss << std::setw(2) << pt.hash(std::span<const std::ptrdiff_t>(&x, 1));
    }
    return oss.str();
}

inline void test_chacha_vectors() {
    {
        std::array<u8, 32> seed{};
        ChaCha20Rng rng(seed);
        const u32 expected1[16] = {
            0xade0b876u, 0x903df1a0u, 0xe56a5d40u, 0x28bd8653u,
            0xb819d2bdu, 0x1aed8da0u, 0xccef36a8u, 0xc70d778bu,
            0x7c5941dau, 0x8d485751u, 0x3fe02477u, 0x374ad8b8u,
            0xf4b8436au, 0x1ca11815u, 0x69b687c3u, 0x8665eeb2u
        };
        for (u32 x : expected1) require(rng.next_u32() == x, "ChaCha20 block0 mismatch");
        const u32 expected2[16] = {
            0xbee7079fu, 0x7a385155u, 0x7c97ba98u, 0x0d082d73u,
            0xa0290fcbu, 0x6965e348u, 0x3e53c612u, 0xed7aee32u,
            0x7621b729u, 0x434ee69cu, 0xb03371d5u, 0xd539d874u,
            0x281fed31u, 0x45fb0a51u, 0x1f0ae1acu, 0x6f4d794bu
        };
        for (u32 x : expected2) require(rng.next_u32() == x, "ChaCha20 block1 mismatch");
    }
}

inline void test_basic_properties() {
    for (u64 seed : std::array<u64,8>{
            0ULL, 1ULL, 2ULL, 3ULL,
            0x0123456789abcdefULL, 0x8000000000000000ULL,
            0xfedcba9876543210ULL, std::numeric_limits<u64>::max()}) {
        ChaCha20Rng a = ChaCha20Rng::seed_from_u64(seed);
        ChaCha20Rng b = ChaCha20Rng::seed_from_u64(seed);
        for (int i = 0; i < 1024; ++i) require(a.next_u64() == b.next_u64(), "seed reproducibility failed");
    }

    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(42);
        for (int i = 0; i < 20000; ++i) {
            auto a = rng.gen_range<i32>(-1, 2);
            auto b = rng.gen_range<i32>(std::numeric_limits<i32>::min(), std::numeric_limits<i32>::min() + 97);
            auto c = rng.gen_range<u32>(0u, 1u << 31);
            auto d = rng.gen_range<u64>((u64(1) << 63) - 123456u, (u64(1) << 63) + 123456u);
            auto e = rng.gen_range<double>(-1.0e-9, 1.0e-9);
            require(-1 <= a && a < 2, "gen_range i32");
            require(std::numeric_limits<i32>::min() <= b && b < std::numeric_limits<i32>::min() + 97, "gen_range i32 edge");
            require(c < (1u << 31), "gen_range u32");
            require((u64(1) << 63) - 123456u <= d && d < (u64(1) << 63) + 123456u, "gen_range u64");
            require(-1.0e-9 <= e && e < 1.0e-9, "gen_range f64");
        }
    }

    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(7);
        Bernoulli b0(0.0), b1(1.0), b25(0.25), b75(0.75);
        for (int i = 0; i < 1024; ++i) require(!b0.sample(rng), "Bernoulli(0)");
        for (int i = 0; i < 1024; ++i) require(b1.sample(rng), "Bernoulli(1)");
        int c25 = 0, c75 = 0;
        for (int i = 0; i < 200000; ++i) {
            c25 += int(b25.sample(rng));
            c75 += int(b75.sample(rng));
        }
        require(43000 < c25 && c25 < 57000, "Bernoulli(0.25) rate");
        require(143000 < c75 && c75 < 157000, "Bernoulli(0.75) rate");
    }

    {
        Normal<double> nd(2.5, 3.75);
        Normal<float> nf(-1.25f, 0.75f);
        Normal<double> neg(0.0, -1.0);
        Normal<double> zero(1.25, 0.0);
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(99);
        for (int i = 0; i < 64; ++i) {
            [[maybe_unused]] double x = nd.sample(rng);
            [[maybe_unused]] float y = nf.sample(rng);
        }
        require(neg.from_zscore(2.0) == -2.0, "Normal negative stddev");
        for (int i = 0; i < 8; ++i) require(zero.sample(rng) == 1.25, "Normal zero stddev");
        bool bad_inf = false;
        try {
            [[maybe_unused]] Normal<double> bad(0.0, std::numeric_limits<double>::infinity());
        } catch (const std::invalid_argument&) {
            bad_inf = true;
        }
        bool bad_nan = false;
        try {
            [[maybe_unused]] Normal<double> bad(0.0, std::numeric_limits<double>::quiet_NaN());
        } catch (const std::invalid_argument&) {
            bad_nan = true;
        }
        require(bad_inf && bad_nan, "Normal invalid stddev");
    }
}


inline void test_exp_properties() {
    {
        bool neg = false;
        bool nan = false;
        bool neg_inf = false;
        try { [[maybe_unused]] Exp<double> e(-1.0); } catch (const std::invalid_argument&) { neg = true; }
        try { [[maybe_unused]] Exp<double> e(std::numeric_limits<double>::quiet_NaN()); } catch (const std::invalid_argument&) { nan = true; }
        try { [[maybe_unused]] Exp<double> e(-std::numeric_limits<double>::infinity()); } catch (const std::invalid_argument&) { neg_inf = true; }
        require(neg && nan && neg_inf, "Exp invalid lambda");
    }
    {
        ChaCha20Rng a = ChaCha20Rng::seed_from_u64(3001);
        ChaCha20Rng b = ChaCha20Rng::seed_from_u64(3001);
        Exp1 e1;
        Exp<double> e(1.0);
        for (int i = 0; i < 4096; ++i) {
            require(std::bit_cast<u64>(e1.sample(a)) == std::bit_cast<u64>(e.sample(b)), "Exp(1) mismatch");
        }
    }
    {
        ChaCha20Rng a = ChaCha20Rng::seed_from_u64(3002);
        ChaCha20Rng b = ChaCha20Rng::seed_from_u64(3002);
        Exp1 e1;
        Exp<float> e(1.0f);
        for (int i = 0; i < 4096; ++i) {
            require(std::bit_cast<u32>(e1.sample_f32(a)) == std::bit_cast<u32>(e.sample(b)), "Exp<float>(1) mismatch");
        }
    }
    {
        ChaCha20Rng rp = ChaCha20Rng::seed_from_u64(3003);
        ChaCha20Rng rn = ChaCha20Rng::seed_from_u64(3003);
        ChaCha20Rng ri = ChaCha20Rng::seed_from_u64(3003);
        double zp = Exp<double>(0.0).sample(rp);
        double zn = Exp<double>(-0.0).sample(rn);
        double zi = Exp<double>(std::numeric_limits<double>::infinity()).sample(ri);
        require(std::isinf(zp) && !std::signbit(zp), "Exp(+0) result");
        require(std::isinf(zn) && std::signbit(zn), "Exp(-0) result");
        require(zi == 0.0 && !std::signbit(zi), "Exp(+inf) result");

        ChaCha20Rng rpf = ChaCha20Rng::seed_from_u64(3003);
        ChaCha20Rng rnf = ChaCha20Rng::seed_from_u64(3003);
        ChaCha20Rng rif = ChaCha20Rng::seed_from_u64(3003);
        float zpf = Exp<float>(0.0f).sample(rpf);
        float znf = Exp<float>(-0.0f).sample(rnf);
        float zif = Exp<float>(std::numeric_limits<float>::infinity()).sample(rif);
        require(std::isinf(zpf) && !std::signbit(zpf), "Exp<float>(+0) result");
        require(std::isinf(znf) && std::signbit(znf), "Exp<float>(-0) result");
        require(zif == 0.0f && !std::signbit(zif), "Exp<float>(+inf) result");
    }
    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(3004);
        Exp<double> e(2.0);
        double sum = 0.0;
        constexpr int count = 200000;
        for (int i = 0; i < count; ++i) {
            double x = e.sample(rng);
            require(x >= 0.0 && std::isfinite(x), "Exp sample range");
            sum += x;
        }
        double mean = sum / count;
        require(0.49 < mean && mean < 0.51, "Exp sample mean");
    }
}

inline void test_seq_properties() {
    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(1001);
        auto idx = seq::index::sample(rng, 50, 17);
        std::vector<int> seen(50, 0);
        for (std::size_t x : idx) {
            require(x < 50, "index::sample range");
            require(++seen[x] == 1, "index::sample duplicate");
        }
    }

    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(1002);
        std::vector<int> a(32);
        std::iota(a.begin(), a.end(), 0);
        seq::shuffle(std::span<int>(a.data(), a.size()), rng);
        std::vector<int> b = a;
        std::sort(b.begin(), b.end());
        for (int i = 0; i < 32; ++i) require(b[i] == i, "shuffle permutation");
    }

    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(1003);
        std::vector<int> a(20);
        std::iota(a.begin(), a.end(), 0);
        auto chosen = seq::choose_multiple<int>(std::span<const int>(a.data(), a.size()), rng, 7);
        std::vector<int> vals;
        for (auto p : chosen) vals.push_back(*p);
        std::sort(vals.begin(), vals.end());
        require(std::adjacent_find(vals.begin(), vals.end()) == vals.end(), "choose_multiple duplicate");
    }
}

inline void test_sample_weighted_properties() {
    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(2001);
        auto idx = seq::index::sample_weighted(rng, 7, [](std::size_t i) {
            static constexpr double w[7] = {0.0, 2.0, 0.0, 3.0, 0.0, 5.0, 0.0};
            return w[i];
        }, 3);
        std::sort(idx.begin(), idx.end());
        require((idx == std::vector<std::size_t>{1, 3, 5}), "sample_weighted sparse");
    }
    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(2002);
        auto idx = seq::index::sample_weighted(rng, 5, [](std::size_t i) { return double(i + 1); }, 5);
        std::sort(idx.begin(), idx.end());
        require((idx == std::vector<std::size_t>{0, 1, 2, 3, 4}), "sample_weighted all");
    }
    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(2005);
        auto idx = seq::index::sample_weighted(rng, 5, [](std::size_t i) { return double(i + 1); }, 0);
        require(idx.empty(), "sample_weighted zero amount");
    }
    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(2003);
        bool ok = false;
        try {
            (void)seq::index::sample_weighted(rng, 3, [](std::size_t i) { return i == 1 ? -1.0 : 1.0; }, 2);
        } catch (const std::invalid_argument&) {
            ok = true;
        }
        require(ok, "sample_weighted negative");
    }
    {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(2004);
        bool ok = false;
        try {
            (void)seq::index::sample_weighted(rng, 3, [](std::size_t) { return 1.0; }, 4);
        } catch (const std::invalid_argument&) {
            ok = true;
        }
        require(ok, "sample_weighted amount > len");
    }
}

inline void test_weighted_index_properties() {
    {
        WeightedIndex<double> wd(std::vector<double>{1.0, 2.0, 3.0, 0.0, 4.5});
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(3001);
        for (int i = 0; i < 1024; ++i) {
            std::size_t x = wd.sample(rng);
            require(x < 5 && x != 3, "WeightedIndex sample");
        }
    }
    {
        bool ok = false;
        try {
            [[maybe_unused]] WeightedIndex<double> wd(std::vector<double>{0.0, 0.0});
        } catch (const std::invalid_argument&) {
            ok = true;
        }
        require(ok, "WeightedIndex all zero");
    }
    {
        bool ok = false;
        try {
            [[maybe_unused]] WeightedIndex<double> wd(std::vector<double>{1.0, -2.0, 3.0});
        } catch (const std::invalid_argument&) {
            ok = true;
        }
        require(ok, "WeightedIndex negative");
    }
}

inline void test_quintic_properties() {
    require(detail::quintic(-0.75) == 0.0, "quintic negative clamp");
    require(detail::quintic(0.0) == 0.0, "quintic zero");
    require(detail::quintic(0.5) == 0.5, "quintic midpoint");
    require(detail::quintic(1.0) == 1.0, "quintic one");
    require(detail::quintic(1.25) == 1.0, "quintic positive clamp");
}

inline void test_perlin_properties() {
    {
        PermutationTable pt(123456789u);
        std::array<int, 256> seen{};
        for (int i = 0; i < 256; ++i) {
            std::ptrdiff_t x = i;
            std::size_t h = pt.hash(std::span<const std::ptrdiff_t>(&x, 1));
            require(h < 256, "PermutationTable hash range");
            ++seen[h];
        }
        for (int c : seen) require(c == 1, "PermutationTable not permutation");
    }
    {
        Perlin p = Perlin(1).set_seed(2);
        require(p.seed() == 2, "Perlin seed");
        double v1 = p.get(std::array<double,1>{0.125});
        double v2 = p.get(std::array<double,2>{0.125, -0.75});
        double v3 = p.get(std::array<double,3>{0.125, -0.75, 1.5});
        double v4 = p.get(std::array<double,4>{0.125, -0.75, 1.5, -2.25});
        require(-1.0 <= v1 && v1 <= 1.0, "Perlin 1D");
        require(-1.0 <= v2 && v2 <= 1.0, "Perlin 2D");
        require(-1.0 <= v3 && v3 <= 1.0, "Perlin 3D");
        require(-1.0 <= v4 && v4 <= 1.0, "Perlin 4D");
    }
}

inline void test_randomized_many() {
    for (u64 seed = 0; seed < 200; ++seed) {
        ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed * 0x9e3779b97f4a7c15ULL + 0x123456789abcdef0ULL);
        auto ui = Uniform<i32>(-1000, 1001);
        auto uf = Uniform<double>(-10.0, 10.0);
        for (int rep = 0; rep < 128; ++rep) {
            int a = ui.sample(rng);
            double b = uf.sample(rng);
            require(-1000 <= a && a < 1001, "Uniform<int>");
            require(-10.0 <= b && b < 10.0, "Uniform<double>");
        }
        auto idx = seq::index::sample_weighted(rng, 9, [](std::size_t i) { return double((i % 3) + 1); }, 4);
        std::vector<int> seen(9, 0);
        for (std::size_t x : idx) {
            require(x < 9, "sample_weighted range");
            require(++seen[x] == 1, "sample_weighted duplicate");
        }
    }
}

inline int dump_suite() {
    constexpr std::array<u64, 8> seeds{
        0ULL,
        1ULL,
        2ULL,
        3ULL,
        0x0123456789abcdefULL,
        0x8000000000000000ULL,
        0xfedcba9876543210ULL,
        std::numeric_limits<u64>::max()
    };

    for (u64 seed : seeds) {
        std::cout << "SEED " << seed << "\n";

        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<u32, 64> v{};
            for (auto& x : v) x = rng.next_u32();
            print_line_hex("NEXT_U32", v);
        }
        {
            ChaCha20Rng::Seed raw{};
            for (std::size_t i = 0; i < raw.size(); ++i) {
                raw[i] = static_cast<u8>(((seed >> ((i % 8) * 8)) & 0xffULL) ^ static_cast<u64>((i * 37 + 11) & 0xff));
            }
            ChaCha20Rng rng(raw);
            std::array<u32, 16> v{};
            for (auto& x : v) x = rng.next_u32();
            print_line_hex("FROM_SEED_U32", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<u8, 37> bytes{};
            rng.fill_bytes(bytes.data(), bytes.size());
            print_line_hex("FILL_BYTES_37", bytes);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            rng.set_stream(seed ^ 0x8899aabbccddeeffULL);
            rng.set_word_pos(u128(37u + static_cast<u32>(seed & 15ULL)));
            std::array<std::string, 4> v{
                hx(rng.get_stream()),
                hx(rng.get_word_pos()),
                hx(rng.next_u32()),
                hx(rng.get_word_pos())
            };
            print_line("STREAM_WORDPOS", v);
        }
        {
            XorShiftRng::Seed raw{};
            for (std::size_t i = 0; i < raw.size(); ++i) {
                raw[i] = static_cast<u8>(((seed >> ((i % 8) * 8)) & 0xffULL) ^ static_cast<u64>((i * 29 + 7) & 0xff));
            }
            XorShiftRng rng(raw);
            std::array<u32, 24> v{};
            for (auto& x : v) x = rng.next_u32();
            print_line_hex("XORSHIFT_FROM_SEED_U32", v);
        }
        {
            XorShiftRng rng = XorShiftRng::seed_from_u64(seed);
            std::array<std::string, 8> v{
                hx(rng.gen<u32>()),
                hx(rng.gen<u64>()),
                hx(rng.gen<u32>()),
                hx(rng.gen<u64>()),
                hx(rng.next_u32()),
                hx(rng.next_u64()),
                hx(rng.next_u32()),
                hx(rng.next_u64())
            };
            print_line("XORSHIFT_GEN_STD", v);
        }
        {
            XorShiftRng rng = XorShiftRng::seed_from_u64(seed);
            std::array<u8, 19> bytes{};
            rng.fill_bytes(bytes.data(), bytes.size());
            print_line_hex("XORSHIFT_FILL_BYTES_19", bytes);
        }
        {
            XorShiftRng rng = XorShiftRng::seed_from_u64(seed);
            std::array<std::string, 8> v{
                hx(rng.gen_range(0u, 1u)),
                hx(rng.gen_range(0u, 2u)),
                hx(rng.gen_range(7u, 8u)),
                hx(rng.gen_range(123456789u, 123456799u)),
                hx(rng.gen_range((1u << 31) - 123456u, (1u << 31) + 123456u)),
                hx(rng.gen_range(std::numeric_limits<u32>::max() - 1024u, std::numeric_limits<u32>::max())),
                hx(rng.gen_range(42u, 43u)),
                hx(rng.gen_range(1u, std::numeric_limits<u32>::max()))
            };
            print_line("XORSHIFT_RANGE_U32", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 14> v{
                hx(rng.gen<u8>()),
                hx(rng.gen<u16>()),
                hx(rng.gen<u32>()),
                hx(rng.gen<u64>()),
                hx(rng.gen<u128>()),
                hx(rng.gen<i8>()),
                hx(rng.gen<i16>()),
                hx(rng.gen<i32>()),
                hx(rng.gen<i64>()),
                hx(rng.gen<i128>()),
                hx(static_cast<u64>(rng.gen<std::size_t>())),
                hx(static_cast<i64>(rng.gen<std::ptrdiff_t>())),
                hx(rng.gen<float>()),
                hx(rng.gen<double>())
            };
            print_line("GEN_STD", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 6> v{
                hx(rng.gen_range<i32>(-10, 10)),
                hx(rng.gen_range<i32>(std::numeric_limits<i32>::min(), std::numeric_limits<i32>::min() + 97)),
                hx(rng.gen_range<i32>(std::numeric_limits<i32>::max() - 97, std::numeric_limits<i32>::max())),
                hx(rng.gen_range<i32>(-1000000000, 1000000000)),
                hx(rng.gen_range<i32>(0, 1)),
                hx(rng.gen_range<i32>(-1, 2))
            };
            print_line("RANGE_EX_I32", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 6> v{
                hx(rng.gen_range<u64>(0ULL, 1ULL)),
                hx(rng.gen_range<u64>(0ULL, 2ULL)),
                hx(rng.gen_range<u64>(123456789ULL, 123456799ULL)),
                hx(rng.gen_range<u64>((u64(1) << 63) - 123456ULL, (u64(1) << 63) + 123456ULL)),
                hx(rng.gen_range<u64>(std::numeric_limits<u64>::max() - 1024ULL, std::numeric_limits<u64>::max())),
                hx(rng.gen_range<u64>(42ULL, 43ULL))
            };
            print_line("RANGE_EX_U64", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 3> v{
                hx(rng.gen_range<float>(-3.0f, 7.0f)),
                hx(rng.gen_range<float>(-1.0e-6f, 1.0e-6f)),
                hx(rng.gen_range<float>(0.0f, 1.0f))
            };
            print_line("RANGE_EX_F32", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 3> v{
                hx(rng.gen_range<double>(-3.0, 7.0)),
                hx(rng.gen_range<double>(-1.0e-9, 1.0e-9)),
                hx(rng.gen_range<double>(0.0, 1.0))
            };
            print_line("RANGE_EX_F64", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 6> v{
                hx(rng.gen_range_inclusive<i32>(-10, 10)),
                hx(rng.gen_range_inclusive<i32>(std::numeric_limits<i32>::min(), std::numeric_limits<i32>::min())),
                hx(rng.gen_range_inclusive<i32>(std::numeric_limits<i32>::max() - 3, std::numeric_limits<i32>::max())),
                hx(rng.gen_range_inclusive<i32>(-1000000000, 1000000000)),
                hx(rng.gen_range_inclusive<i32>(0, 0)),
                hx(rng.gen_range_inclusive<i32>(-1, 2))
            };
            print_line("RANGE_IN_I32", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 6> v{
                hx(rng.gen_range_inclusive<u64>(0ULL, 0ULL)),
                hx(rng.gen_range_inclusive<u64>(0ULL, 1ULL)),
                hx(rng.gen_range_inclusive<u64>(123456789ULL, 123456799ULL)),
                hx(rng.gen_range_inclusive<u64>((u64(1) << 63) - 123456ULL, (u64(1) << 63) + 123456ULL)),
                hx(rng.gen_range_inclusive<u64>(std::numeric_limits<u64>::max() - 1024ULL, std::numeric_limits<u64>::max())),
                hx(rng.gen_range_inclusive<u64>(42ULL, 42ULL))
            };
            print_line("RANGE_IN_U64", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 3> v{
                hx(rng.gen_range_inclusive<float>(-3.0f, 7.0f)),
                hx(rng.gen_range_inclusive<float>(0.0f, 0.0f)),
                hx(rng.gen_range_inclusive<float>(1.0f, 1.0f))
            };
            print_line("RANGE_IN_F32", v);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 3> v{
                hx(rng.gen_range_inclusive<double>(-3.0, 7.0)),
                hx(rng.gen_range_inclusive<double>(0.0, 0.0)),
                hx(rng.gen_range_inclusive<double>(1.0, 1.0))
            };
            print_line("RANGE_IN_F64", v);
        }
        {
            for (double p : std::array<double, 4>{0.0, 0.3, 0.5, 1.0}) {
                ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed ^ 0x1111111111111111ULL ^ static_cast<u64>(p * 1024.0));
                std::cout << "GEN_BOOL " << hx(p) << ' ';
                for (int i = 0; i < 128; ++i) std::cout << int(rng.gen_bool(p));
                std::cout << "\n";
            }
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Uniform<i32> ui(-123, 456);
            Uniform<u64> uu(123456789ULL, 123456789ULL + 1000ULL);
            Uniform<double> ud(-9.5, 11.25);
            std::array<std::string, 16> iv{};
            std::array<std::string, 16> uv{};
            std::array<std::string, 16> fv{};
            for (auto& x : iv) x = hx(ui.sample(rng));
            for (auto& x : uv) x = hx(uu.sample(rng));
            for (auto& x : fv) x = hx(ud.sample(rng));
            print_line("UNIFORM_EX_I32", iv);
            print_line("UNIFORM_EX_U64", uv);
            print_line("UNIFORM_EX_F64", fv);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Uniform<u32> ua(0u, 17u);
            Uniform<u32> ub(std::numeric_limits<u32>::max() - 1000u, std::numeric_limits<u32>::max());
            std::array<std::string, 16> uv{};
            for (int i = 0; i < 8; ++i) uv[std::size_t(i)] = hx(ua.sample(rng));
            for (int i = 8; i < 16; ++i) uv[std::size_t(i)] = hx(ub.sample(rng));
            print_line("UNIFORM_EX_U32", uv);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Uniform<i64> ia(-1234567890123LL, 1234567890456LL);
            Uniform<i64> ib((i64(1) << 62) - 1000LL, (i64(1) << 62) + 1000LL);
            std::array<std::string, 16> iv{};
            for (int i = 0; i < 8; ++i) iv[std::size_t(i)] = hx(ia.sample(rng));
            for (int i = 8; i < 16; ++i) iv[std::size_t(i)] = hx(ib.sample(rng));
            print_line("UNIFORM_EX_I64", iv);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Uniform<float> fa(-1.5f, 2.5f);
            Uniform<float> fb(-1.0e-7f, 1.0e-7f);
            std::array<std::string, 16> fv{};
            for (int i = 0; i < 8; ++i) fv[std::size_t(i)] = hx(fa.sample(rng));
            for (int i = 8; i < 16; ++i) fv[std::size_t(i)] = hx(fb.sample(rng));
            print_line("UNIFORM_EX_F32", fv);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            auto ui = Uniform<i32>::inclusive(-7, 7);
            auto uu = Uniform<u64>::inclusive((u64(1) << 63) - 1000ULL, (u64(1) << 63) + 1000ULL);
            auto ud = Uniform<double>::inclusive(-1.5, 2.5);
            std::array<std::string, 16> iv{};
            std::array<std::string, 16> uv{};
            std::array<std::string, 16> fv{};
            for (auto& x : iv) x = hx(rng.sample(ui));
            for (auto& x : uv) x = hx(rng.sample(uu));
            for (auto& x : fv) x = hx(rng.sample(ud));
            print_line("UNIFORM_IN_I32", iv);
            print_line("UNIFORM_IN_U64", uv);
            print_line("UNIFORM_IN_F64", fv);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            auto ua = Uniform<u32>::inclusive(0u, 0u);
            auto ub = Uniform<u32>::inclusive(17u, 42u);
            auto uc = Uniform<u32>::inclusive(std::numeric_limits<u32>::max() - 3u, std::numeric_limits<u32>::max());
            std::array<std::string, 16> uv{};
            for (int i = 0; i < 4; ++i) uv[std::size_t(i)] = hx(rng.sample(ua));
            for (int i = 4; i < 12; ++i) uv[std::size_t(i)] = hx(rng.sample(ub));
            for (int i = 12; i < 16; ++i) uv[std::size_t(i)] = hx(rng.sample(uc));
            print_line("UNIFORM_IN_U32", uv);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            auto ia = Uniform<i64>::inclusive(-7LL, 7LL);
            auto ib = Uniform<i64>::inclusive((i64(1) << 62) - 3LL, (i64(1) << 62) + 3LL);
            std::array<std::string, 16> iv{};
            for (int i = 0; i < 8; ++i) iv[std::size_t(i)] = hx(rng.sample(ia));
            for (int i = 8; i < 16; ++i) iv[std::size_t(i)] = hx(rng.sample(ib));
            print_line("UNIFORM_IN_I64", iv);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            auto fa = Uniform<float>::inclusive(0.0f, 0.0f);
            auto fb = Uniform<float>::inclusive(-1.5f, 2.5f);
            auto fc = Uniform<float>::inclusive(1.0f, 1.0f);
            std::array<std::string, 16> fv{};
            for (int i = 0; i < 4; ++i) fv[std::size_t(i)] = hx(rng.sample(fa));
            for (int i = 4; i < 12; ++i) fv[std::size_t(i)] = hx(rng.sample(fb));
            for (int i = 12; i < 16; ++i) fv[std::size_t(i)] = hx(rng.sample(fc));
            print_line("UNIFORM_IN_F32", fv);
        }
        {
            for (double p : std::array<double, 4>{0.0, 1.0, 0.5, 2.0 / 7.0}) {
                ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed ^ 0x2222222222222222ULL ^ static_cast<u64>(p * 65536.0));
                Bernoulli b(p);
                std::cout << "BERNOULLI " << hx(p) << ' ';
                for (int i = 0; i < 128; ++i) std::cout << int(b.sample(rng));
                std::cout << "\n";
            }
        }
        {
            for (auto [num, den] : std::array<std::pair<u32, u32>, 4>{{ {0, 1}, {1, 7}, {2, 7}, {7, 7} }}) {
                ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed ^ 0x3333333333333333ULL ^ num ^ (u64(den) << 32));
                Bernoulli b = Bernoulli::from_ratio(num, den);
                std::cout << "BERNOULLI_RATIO " << num << '/' << den << ' ';
                for (int i = 0; i < 128; ++i) std::cout << int(b.sample(rng));
                std::cout << "\n";
            }
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            WeightedIndex<double> wd(std::vector<double>{1.0, 2.0, 3.0, 0.0, 4.5});
            std::array<std::size_t, 32> out{};
            for (auto& x : out) x = wd.sample(rng);
            print_line("WEIGHTED_D", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            WeightedIndex<u32> wu(std::vector<u32>{5u, 0u, 7u, 9u, 11u, 13u});
            std::array<std::size_t, 32> out{};
            for (auto& x : out) x = wu.sample(rng);
            print_line("WEIGHTED_U", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            WeightedIndex<u32> we(std::vector<u32>{1u, 1u, 1u, 1u});
            std::array<std::size_t, 32> out{};
            for (auto& x : out) x = we.sample(rng);
            print_line("WEIGHTED_EQ", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            WeightedIndex<double> ws(std::vector<double>{0.0, 5.0, 0.0, 7.0, 0.0, 0.0, 11.0});
            std::array<std::size_t, 32> out{};
            for (auto& x : out) x = ws.sample(rng);
            print_line("WEIGHTED_SPARSE", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            WeightedIndex<u64> wb(std::vector<u64>{1ULL, (1ULL << 40) + 3ULL, 7ULL, (1ULL << 39) + 5ULL});
            std::array<std::size_t, 32> out{};
            for (auto& x : out) x = wb.sample(rng);
            print_line("WEIGHTED_BIG_U64", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            WeightedIndex<double> wt(std::vector<double>{1.0e-12, 2.0e-12, 0.0, 3.0e-12});
            std::array<std::size_t, 32> out{};
            for (auto& x : out) x = wt.sample(rng);
            print_line("WEIGHTED_TINY_D", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> arr(20);
            std::iota(arr.begin(), arr.end(), 0);
            seq::shuffle(std::span<int>(arr.data(), arr.size()), rng);
            print_line("SHUFFLE", arr);
            print_line("INDEX_SAMPLE", seq::index::sample(rng, 20, 8));
            print_line("INDEX_SAMPLE_W_POS", seq::index::sample_weighted(rng, 12, [](std::size_t i) { return double(i + 1); }, 5));
            print_line("INDEX_SAMPLE_W_SPARSE", seq::index::sample_weighted(rng, 7, [](std::size_t i) {
                static constexpr double w[7] = {0.0, 2.0, 0.0, 3.0, 0.0, 5.0, 0.0};
                return w[i];
            }, 3));
            print_line("INDEX_SAMPLE_W_ZERO", seq::index::sample_weighted(rng, 5, [](std::size_t i) { return double(i + 1); }, 0));
            std::cout << "CHOOSE_EMPTY " << opt_value(seq::choose<int>(std::span<const int>(), rng)) << "\n";
            std::cout << "CHOOSE " << opt_value(seq::choose<int>(std::span<const int>(arr.data(), arr.size()), rng)) << "\n";
            auto many0 = seq::choose_multiple<int>(std::span<const int>(arr.data(), arr.size()), rng, 0);
            std::vector<int> mv0; for (auto p : many0) mv0.push_back(*p);
            print_line("CHOOSE_MULTIPLE_0", mv0);

            auto many1 = seq::choose_multiple<int>(std::span<const int>(arr.data(), arr.size()), rng, 1);
            std::vector<int> mv1; for (auto p : many1) mv1.push_back(*p);
            print_line("CHOOSE_MULTIPLE_1", mv1);

            auto many6 = seq::choose_multiple<int>(std::span<const int>(arr.data(), arr.size()), rng, 6);
            std::vector<int> mv6; for (auto p : many6) mv6.push_back(*p);
            print_line("CHOOSE_MULTIPLE_6", mv6);

            auto many19 = seq::choose_multiple<int>(std::span<const int>(arr.data(), arr.size()), rng, 19);
            std::vector<int> mv19; for (auto p : many19) mv19.push_back(*p);
            print_line("CHOOSE_MULTIPLE_19", mv19);

            auto many20 = seq::choose_multiple<int>(std::span<const int>(arr.data(), arr.size()), rng, 20);
            std::vector<int> mv20; for (auto p : many20) mv20.push_back(*p);
            print_line("CHOOSE_MULTIPLE_20", mv20);

            auto many_all = seq::choose_multiple<int>(std::span<const int>(arr.data(), arr.size()), rng, 30);
            std::vector<int> mv_all; for (auto p : many_all) mv_all.push_back(*p);
            print_line("CHOOSE_MULTIPLE_ALL", mv_all);

            std::cout << "ITER_CHOOSE " << opt_value_iter(seq::iter::choose(arr.begin(), arr.end(), rng), arr.end()) << "\n";
            std::cout << "ITER_CHOOSE_STABLE " << opt_value_iter(seq::iter::choose_stable(arr.begin(), arr.end(), rng), arr.end()) << "\n";
            std::cout << "CHOOSE_WEIGHTED " << *seq::choose_weighted<int>(std::span<const int>(arr.data(), arr.size()), rng, [](const int& x) { return double(x + 1); }) << "\n";
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> a0(20); std::iota(a0.begin(), a0.end(), 0);
            (void)seq::partial_shuffle<int>(std::span<int>(a0.data(), a0.size()), rng, 0);
            print_line("PARTIAL_SHUFFLE_0", a0);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> a1(20); std::iota(a1.begin(), a1.end(), 0);
            (void)seq::partial_shuffle<int>(std::span<int>(a1.data(), a1.size()), rng, 1);
            print_line("PARTIAL_SHUFFLE_1", a1);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> a6(20); std::iota(a6.begin(), a6.end(), 0);
            (void)seq::partial_shuffle<int>(std::span<int>(a6.data(), a6.size()), rng, 6);
            print_line("PARTIAL_SHUFFLE_6", a6);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> a19(20); std::iota(a19.begin(), a19.end(), 0);
            (void)seq::partial_shuffle<int>(std::span<int>(a19.data(), a19.size()), rng, 19);
            print_line("PARTIAL_SHUFFLE_19", a19);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> a20(20); std::iota(a20.begin(), a20.end(), 0);
            (void)seq::partial_shuffle<int>(std::span<int>(a20.data(), a20.size()), rng, 20);
            print_line("PARTIAL_SHUFFLE_20", a20);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> a30(20); std::iota(a30.begin(), a30.end(), 0);
            (void)seq::partial_shuffle<int>(std::span<int>(a30.data(), a30.size()), rng, 30);
            print_line("PARTIAL_SHUFFLE_30", a30);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 32> out{};
            for (auto& x : out) x = hx(StandardNormal().sample(rng));
            print_line("STD_NORMAL", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Normal<double> nd1(2.5, 3.75);
            Normal<double> nd2(-1.0, 2.25);
            std::array<std::string, 32> a{};
            std::array<std::string, 32> b{};
            for (auto& x : a) x = hx(nd1.sample(rng));
            for (auto& x : b) x = hx(nd2.sample(rng));
            print_line("NORMAL_POS", a);
            print_line("NORMAL_ALT", b);
        }

        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 64> out{};
            for (auto& x : out) x = hx(Exp1().sample(rng));
            print_line("EXP1_F64", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 64> out{};
            for (auto& x : out) x = hx(Exp1().sample_f32(rng));
            print_line("EXP1_F32", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Exp<double> e(0.25);
            std::array<std::string, 32> out{};
            for (auto& x : out) x = hx(e.sample(rng));
            print_line("EXP_F64_QUARTER", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Exp<double> e(1.0);
            std::array<std::string, 32> out{};
            for (auto& x : out) x = hx(e.sample(rng));
            print_line("EXP_F64_ONE", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Exp<double> e(2.0);
            std::array<std::string, 32> out{};
            for (auto& x : out) x = hx(e.sample(rng));
            print_line("EXP_F64_TWO", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Exp<float> e(0.5f);
            std::array<std::string, 32> out{};
            for (auto& x : out) x = hx(e.sample(rng));
            print_line("EXP_F32_HALF", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            Exp<float> e(2.0f);
            std::array<std::string, 32> out{};
            for (auto& x : out) x = hx(e.sample(rng));
            print_line("EXP_F32_TWO", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 12> out{};
            Exp<double> zero(0.0);
            Exp<double> neg_zero(-0.0);
            Exp<double> inf(std::numeric_limits<double>::infinity());
            for (int i = 0; i < 4; ++i) out[static_cast<std::size_t>(i)] = hx(zero.sample(rng));
            for (int i = 0; i < 4; ++i) out[static_cast<std::size_t>(i + 4)] = hx(neg_zero.sample(rng));
            for (int i = 0; i < 4; ++i) out[static_cast<std::size_t>(i + 8)] = hx(inf.sample(rng));
            print_line("EXP_SPECIAL_F64", out);
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::array<std::string, 6> out{
                hx(Exp1().sample(rng)),
                hx(Exp<double>(0.125).sample(rng)),
                hx(rng.next_u32()),
                hx(Exp<float>(3.5f).sample(rng)),
                hx(rng.next_u64()),
                hx(Exp<double>(std::numeric_limits<double>::infinity()).sample(rng))
            };
            print_line("EXP_MIXED_STATE", out);
        }
        {
            PermutationTable pt(static_cast<u32>(seed));
            std::cout << "PT_NEW_TABLE " << pt_singletons_hex_safe(pt) << "\n";
            std::array<std::size_t, 5> hs{
                [&](){ std::ptrdiff_t a[] = {-17, 23, 999, -123}; return pt.hash(std::span<const std::ptrdiff_t>(a, 4)); }(),
                [&](){ std::ptrdiff_t a[] = {0}; return pt.hash(std::span<const std::ptrdiff_t>(a, 1)); }(),
                [&](){ std::ptrdiff_t a[] = {255}; return pt.hash(std::span<const std::ptrdiff_t>(a, 1)); }(),
                [&](){ std::ptrdiff_t a[] = {-1}; return pt.hash(std::span<const std::ptrdiff_t>(a, 1)); }(),
                [&](){ std::ptrdiff_t a[] = {1, 2, 3}; return pt.hash(std::span<const std::ptrdiff_t>(a, 3)); }()
            };
            print_line("PT_NEW_HASH", hs);
        }
        {
            Perlin p(static_cast<u32>(seed));
            std::array<u32, 3> ss{
                Perlin().seed(),
                p.seed(),
                Perlin().set_seed(static_cast<u32>(seed)).seed()
            };
            print_line("PERLIN_SEED", ss);
            std::array<std::string, 4> a{
                hx(p.get(std::array<double,1>{0.125})),
                hx(p.get(std::array<double,2>{0.125, -0.75})),
                hx(p.get(std::array<double,3>{0.125, -0.75, 1.5})),
                hx(p.get(std::array<double,4>{0.125, -0.75, 1.5, -2.25}))
            };
            std::array<std::string, 4> b{
                hx(p.get(std::array<double,1>{-3.75})),
                hx(p.get(std::array<double,2>{-3.75, 4.5})),
                hx(p.get(std::array<double,3>{-3.75, 4.5, -1.25})),
                hx(p.get(std::array<double,4>{-3.75, 4.5, -1.25, 2.0}))
            };
            print_line("PERLIN_A", a);
            print_line("PERLIN_B", b);
        }
        {
            bool sw_neg = false;
            try {
                ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(1);
                (void)seq::index::sample_weighted(rng, 3, [](std::size_t i) { return i == 1 ? -1.0 : 1.0; }, 2);
            } catch (const std::invalid_argument&) {
                sw_neg = true;
            }
            bool bern_bad = false;
            try { [[maybe_unused]] Bernoulli b(-0.1); } catch (const std::invalid_argument&) { bern_bad = true; }
            bool wi_neg = false;
            try { [[maybe_unused]] WeightedIndex<double> w(std::vector<double>{1.0, -2.0, 3.0}); } catch (const std::invalid_argument&) { wi_neg = true; }
            bool wi_zero = false;
            try { [[maybe_unused]] WeightedIndex<double> w(std::vector<double>{0.0, 0.0, 0.0}); } catch (const std::invalid_argument&) { wi_zero = true; }
            bool cw_empty = false;
            try {
                ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(1);
                (void)seq::choose_weighted<int>(std::span<const int>(), rng, [](const int&) { return 1.0; });
            } catch (const std::invalid_argument&) {
                cw_empty = true;
            }
            bool normal_neg = false;
            try { [[maybe_unused]] Normal<double> n(0.0, -1.0); } catch (const std::invalid_argument&) { normal_neg = true; }
            bool normal_zero = false;
            try { [[maybe_unused]] Normal<double> n(0.0, 0.0); } catch (const std::invalid_argument&) { normal_zero = true; }
            bool normal_inf = false;
            try { [[maybe_unused]] Normal<double> n(0.0, std::numeric_limits<double>::infinity()); } catch (const std::invalid_argument&) { normal_inf = true; }
            bool normal_nan = false;
            try { [[maybe_unused]] Normal<double> n(0.0, std::numeric_limits<double>::quiet_NaN()); } catch (const std::invalid_argument&) { normal_nan = true; }
            bool exp_neg = false;
            try { [[maybe_unused]] Exp<double> e(-1.0); } catch (const std::invalid_argument&) { exp_neg = true; }
            bool exp_nan = false;
            try { [[maybe_unused]] Exp<double> e(std::numeric_limits<double>::quiet_NaN()); } catch (const std::invalid_argument&) { exp_nan = true; }
            bool exp_neg_inf = false;
            try { [[maybe_unused]] Exp<double> e(-std::numeric_limits<double>::infinity()); } catch (const std::invalid_argument&) { exp_neg_inf = true; }
            bool exp_zero = false;
            try { [[maybe_unused]] Exp<double> e(0.0); } catch (const std::invalid_argument&) { exp_zero = true; }
            bool exp_neg_zero = false;
            try { [[maybe_unused]] Exp<double> e(-0.0); } catch (const std::invalid_argument&) { exp_neg_zero = true; }
            bool exp_pos_inf = false;
            try { [[maybe_unused]] Exp<double> e(std::numeric_limits<double>::infinity()); } catch (const std::invalid_argument&) { exp_pos_inf = true; }

            std::cout << "STATUS_SAMPLE_WEIGHTED_NEG " << (sw_neg ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_BERNOULLI_BAD " << (bern_bad ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_WEIGHTED_INDEX_NEG " << (wi_neg ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_WEIGHTED_INDEX_ZERO " << (wi_zero ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_CHOOSE_WEIGHTED_EMPTY " << (cw_empty ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_NORMAL_NEG " << (normal_neg ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_NORMAL_ZERO " << (normal_zero ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_NORMAL_INF " << (normal_inf ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_NORMAL_NAN " << (normal_nan ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_EXP_NEG " << (exp_neg ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_EXP_NAN " << (exp_nan ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_EXP_NEG_INF " << (exp_neg_inf ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_EXP_ZERO " << (exp_zero ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_EXP_NEG_ZERO " << (exp_neg_zero ? "ERR" : "OK") << "\n";
            std::cout << "STATUS_EXP_POS_INF " << (exp_pos_inf ? "ERR" : "OK") << "\n";
        }
        {
            ChaCha20Rng rng = ChaCha20Rng::seed_from_u64(seed);
            std::vector<int> arr(16); std::iota(arr.begin(), arr.end(), 0);
            WeightedIndex<double> wd(std::vector<double>{1.0, 2.0, 3.0, 4.0});
            Uniform<i32> ui(-50, 51);
            Bernoulli b(0.3);
            auto idxw = seq::index::sample_weighted(rng, 9, [](std::size_t i) { return double((i % 4) + 1); }, 4);
            seq::shuffle(std::span<int>(arr.data(), arr.size()), rng);
            std::cout << "PIPELINE "
                      << hx(rng.next_u32()) << ' '
                      << hx(rng.gen<u64>()) << ' '
                      << hx(rng.gen_range<i32>(-100, 101)) << ' '
                      << int(rng.gen_bool(0.125)) << ' '
                      << hx(ui.sample(rng)) << ' '
                      << int(b.sample(rng)) << ' '
                      << wd.sample(rng) << ' ';
            print_items(idxw);
            std::cout << ' ' << *seq::choose<int>(std::span<const int>(arr.data(), arr.size()), rng)
                      << ' ' << opt_value_iter(seq::iter::choose_stable(arr.begin(), arr.end(), rng), arr.end())
                      << ' ' << hx(StandardNormal().sample(rng))
                      << ' ' << hx(Normal<double>(2.5, 3.75).sample(rng))
                      << ' ' << hx(Exp1().sample(rng))
                      << ' ' << hx(Exp<double>(2.5).sample(rng))
                      << "\n";
        }
    }
    return 0;
}

inline int run_all() {
    test_chacha_vectors();
    test_basic_properties();
    test_exp_properties();
    test_seq_properties();
    test_sample_weighted_properties();
    test_weighted_index_properties();
    test_quintic_properties();
    test_perlin_properties();
    test_randomized_many();
    std::cout << "OK\n";
    return 0;
}

} // namespace rust_rand::self_test

int main(int argc, char** argv) {
    try {
        if (argc >= 2 && std::string(argv[1]) == "--dump-suite") {
            return rust_rand::self_test::dump_suite();
        }
        return rust_rand::self_test::run_all();
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}

#endif
