#pragma once
#include "template.hpp"

struct FastHash {
    ull value;
    FastHash() : value(0) {}
    FastHash(ull value_) : value(value_) {}
    ull get() const { return value; }
    FastHash& reset() { value = 0; return *this; }
    template<typename T> FastHash& add(size_t offset, const T& val) {
        value ^= CustomHasher(offset, val).val;
        return *this;
    }
    template<typename Iter> FastHash& add_iter(size_t offset, Iter begin, Iter end) {
        size_t nth = 0;
        for (auto it = begin; it != end; ++it) {
            value ^= CustomHasher(offset, nth, *it).val;
            nth++;
        }
        return *this;
    }
    template<typename Container> FastHash& add_iter(size_t offset, const Container& container) {
        return add_iter(offset, container.begin(), container.end());
    }
    template<typename T> FastHash& replace(const size_t offset, const T& old_val, const T& new_val) {
        return replace_at(offset, 0, old_val, new_val);
    }
    template<typename T> FastHash& replace_at(const size_t offset, size_t nth, const T& old_val, const T& new_val) {
        ull old_c = CustomHasher(offset, nth, old_val).val;
        ull new_c = CustomHasher(offset, nth, new_val).val;
        value ^= old_c ^ new_c;
        return *this;
    }
    bool operator==(const FastHash& other) const { return value == other.value; }
    bool operator<(const FastHash& other) const { return value < other.value; }

    template<std::integral T>
    friend T operator%(const FastHash& a, T m) {
        return (T)a.value % m;
    }

    // struct CustomHash {
    //     static inline ull splitmix64(ull x) {
    //         x += 0x9e3779b97f4a7c15ULL;
    //         x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    //         x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    //         return x ^ (x >> 31);
    //     }
    //     size_t inline operator()(ull x) const {
    //         // static const ull FIXED_RANDOM = chrono::steady_clock::now().time_since_epoch().count();
    //         return splitmix64(x);
    //     }
    // };
    struct CustomHasher{
        ull val;
        CustomHasher(): val(0x9e3779b97f4a7c15ULL){};
        template<typename T> inline CustomHasher(const T& t): CustomHasher(){ add(t); };
        template<typename T> inline CustomHasher(size_t offset, const T& t): CustomHasher(){ add(offset, t); };
        template<typename T> inline CustomHasher(size_t offset, size_t nth, const T& t): CustomHasher(){ add(offset, nth, t); };
        template<typename T> inline CustomHasher& add(const T& t) {
            constexpr ull K = 0xf1357aea2e62a9c5ULL;
            val += static_cast<ull>(t);
            val *= K;
            return *this;
        }
        // template<typename T> inline CustomHasher& add(const T& t) {
        //     // splitmix64
        //     val += static_cast<ull>(t);
        //     val = (val ^ (val >> 30)) * 0xbf58476d1ce4e5b9ULL;
        //     val = (val ^ (val >> 27)) * 0x94d049bb133111ebULL;
        //     val ^= (val >> 31);
        //     return *this;
        // }
        template<typename T> inline CustomHasher& add(size_t offset, const T& t) {
            return add(offset, 0, t);
        }
        template<typename T> inline CustomHasher& add(size_t offset, size_t nth, const T& t) {
            return add(offset + nth).add(t);
        }
    };
    // template<typename T> static inline ull fxhash64(const T& val) {
    //     // CustomHash hasher;
    //     // return hasher(static_cast<ull>(val));
    //     return static_cast<ull>(val) * 0xf1357aea2e62a9c5ULL;        
    // }

    // static inline ull hash_element(size_t offset, size_t nth, ull item_hash) {
    //     ull pos_hash = fxhash64(offset + nth);
    //     ull rotated1 = rotl(pos_hash, 33);
    //     ull mixed = rotated1 * 0x32b320fa3ac8b5ddULL;
    //     ull xor_val = mixed ^ item_hash;
    //     ull rotated2 = rotl(xor_val, 17);
    //     ull result = rotated2 * 0xaf92cbcaaf562eeaULL;
    //     return result;
    // }
};
ostream& operator<<(ostream& os, const FastHash& t) { os << hex << t.value << dec; return os; }

// unordered_setでFastHashをキーとして使うためのstd::hashの特殊化
namespace std {
    template<>
    struct hash<FastHash> {
        size_t operator()(const FastHash& fh) const noexcept {
            return hash<ull>()(fh.value);
        }
    };
}
