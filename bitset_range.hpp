// #pragma once
#include "template.hpp"

// #include <bitset>
// #include <cassert>
// #include <climits>

// using namespace std;

constexpr size_t bits_per_block = sizeof(unsigned long) * CHAR_BIT;

// -------------------------------------------------
// Extraction function: 指定区間（最大64ビット）を取得する（内部状態直接参照版）
// -------------------------------------------------
template <size_t N>
unsigned long extract_bits(const bitset<N>& bs, size_t pos, size_t width) {
    assert(width <= 64);
    assert(pos + width <= N);
    const unsigned long* blocks = reinterpret_cast<const unsigned long*>(&bs);
    size_t block_index = pos / bits_per_block;
    size_t offset = pos % bits_per_block;
    unsigned long mask = (width == 64) ? ~0UL : ((1UL << width) - 1);
    if (offset + width <= bits_per_block) {
        return (blocks[block_index] >> offset) & mask;
    } else {
        size_t bits_from_next = offset + width - bits_per_block;
        unsigned long lower = blocks[block_index] >> offset;
        unsigned long upper = blocks[block_index + 1] & ((bits_from_next == 64) ? ~0UL : ((1UL << bits_from_next) - 1));
        return lower | (upper << (bits_per_block - offset));
    }
}

// -------------------------------------------------
// Region update functions (内部状態直接参照版)
// -------------------------------------------------

// XOR: 指定区間のビットに対して、op の下位 width ビットと XOR する
template <size_t N>
void bitset_range_xor(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    if (width == 0) return;
    assert(pos + width <= N);
    unsigned long mask = (width == bits_per_block) ? ~0UL : ((1UL << width) - 1);
    op &= mask;
    unsigned long* blocks = reinterpret_cast<unsigned long*>(&bs);
    size_t block_index = pos / bits_per_block;
    size_t offset = pos % bits_per_block;
    if (offset + width <= bits_per_block) {
        unsigned long orig = blocks[block_index];
        unsigned long region = (orig >> offset) & mask;
        unsigned long new_region = region ^ op;
        blocks[block_index] = (orig & ~(mask << offset)) | (new_region << offset);
    } else {
        size_t lower_bits = bits_per_block - offset;
        size_t upper_bits = width - lower_bits;
        unsigned long orig_lower = (blocks[block_index] >> offset) & ((1UL << lower_bits) - 1);
        unsigned long orig_upper = blocks[block_index + 1] & ((1UL << upper_bits) - 1);
        unsigned long combined = orig_lower | (orig_upper << lower_bits);
        unsigned long new_combined = combined ^ op;
        unsigned long new_lower = new_combined & ((1UL << lower_bits) - 1);
        unsigned long new_upper = new_combined >> lower_bits;
        blocks[block_index] = (blocks[block_index] & ~(((1UL << lower_bits) - 1) << offset)) | (new_lower << offset);
        blocks[block_index + 1] = (blocks[block_index + 1] & ~((1UL << upper_bits) - 1)) | new_upper;
    }
}

// OR: 指定区間のビットに対して、op の下位 width ビットと OR する
template <size_t N>
void bitset_range_or(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    if (width == 0) return;
    assert(pos + width <= N);
    unsigned long mask = (width == bits_per_block) ? ~0UL : ((1UL << width) - 1);
    op &= mask;
    unsigned long* blocks = reinterpret_cast<unsigned long*>(&bs);
    size_t block_index = pos / bits_per_block;
    size_t offset = pos % bits_per_block;
    if (offset + width <= bits_per_block) {
        unsigned long orig = blocks[block_index];
        unsigned long region = (orig >> offset) & mask;
        unsigned long new_region = region | op;
        blocks[block_index] = (orig & ~(mask << offset)) | (new_region << offset);
    } else {
        size_t lower_bits = bits_per_block - offset;
        size_t upper_bits = width - lower_bits;
        unsigned long orig_lower = (blocks[block_index] >> offset) & ((1UL << lower_bits) - 1);
        unsigned long orig_upper = blocks[block_index + 1] & ((1UL << upper_bits) - 1);
        unsigned long combined = orig_lower | (orig_upper << lower_bits);
        unsigned long new_combined = combined | op;
        unsigned long new_lower = new_combined & ((1UL << lower_bits) - 1);
        unsigned long new_upper = new_combined >> lower_bits;
        blocks[block_index] = (blocks[block_index] & ~(((1UL << lower_bits) - 1) << offset)) | (new_lower << offset);
        blocks[block_index + 1] = (blocks[block_index + 1] & ~((1UL << upper_bits) - 1)) | new_upper;
    }
}

// AND: 指定区間のビットに対して、op の下位 width ビットと AND する
template <size_t N>
void bitset_range_and(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    if (width == 0) return;
    assert(pos + width <= N);
    unsigned long mask = (width == bits_per_block) ? ~0UL : ((1UL << width) - 1);
    op &= mask;
    unsigned long* blocks = reinterpret_cast<unsigned long*>(&bs);
    size_t block_index = pos / bits_per_block;
    size_t offset = pos % bits_per_block;
    if (offset + width <= bits_per_block) {
        unsigned long orig = blocks[block_index];
        unsigned long region = (orig >> offset) & mask;
        unsigned long new_region = region & op;
        blocks[block_index] = (orig & ~(mask << offset)) | (new_region << offset);
    } else {
        size_t lower_bits = bits_per_block - offset;
        size_t upper_bits = width - lower_bits;
        unsigned long orig_lower = (blocks[block_index] >> offset) & ((1UL << lower_bits) - 1);
        unsigned long orig_upper = blocks[block_index + 1] & ((1UL << upper_bits) - 1);
        unsigned long combined = orig_lower | (orig_upper << lower_bits);
        unsigned long new_combined = combined & op;
        unsigned long new_lower = new_combined & ((1UL << lower_bits) - 1);
        unsigned long new_upper = new_combined >> lower_bits;
        blocks[block_index] = (blocks[block_index] & ~(((1UL << lower_bits) - 1) << offset)) | (new_lower << offset);
        blocks[block_index + 1] = (blocks[block_index + 1] & ~((1UL << upper_bits) - 1)) | new_upper;
    }
}

// SET: 指定区間を true (1) または false (0) にセットする
template <size_t N>
void bitset_range_set(bitset<N>& bs, size_t pos, size_t width, bool value) {
    if (width == 0) return;
    assert(pos + width <= N);
    unsigned long* blocks = reinterpret_cast<unsigned long*>(&bs);
    size_t first_block = pos / bits_per_block;
    size_t last_block  = (pos + width - 1) / bits_per_block;
    size_t start_offset = pos % bits_per_block;
    size_t end_offset = (pos + width) % bits_per_block; // end_offset==0ならlast_block全体更新
    if (first_block == last_block) {
        unsigned long mask = ((width == bits_per_block) ? ~0UL : ((1UL << width) - 1)) << start_offset;
        if (value)
            blocks[first_block] = (blocks[first_block] & ~mask) | mask;
        else
            blocks[first_block] &= ~mask;
    } else {
        unsigned long mask_first = ~0UL << start_offset;
        if (value)
            blocks[first_block] = (blocks[first_block] & ~mask_first) | mask_first;
        else
            blocks[first_block] &= ~mask_first;
        for (size_t i = first_block + 1; i < last_block; ++i) {
            blocks[i] = value ? ~0UL : 0UL;
        }
        if (end_offset == 0) {
            blocks[last_block] = value ? ~0UL : 0UL;
        } else {
            unsigned long mask_last = (1UL << end_offset) - 1;
            if (value)
                blocks[last_block] = (blocks[last_block] & ~mask_last) | mask_last;
            else
                blocks[last_block] &= ~mask_last;
        }
    }
}

// FLIP: 指定区間内の各ビットを反転（flip）する
template <size_t N>
void bitset_range_flip(bitset<N>& bs, size_t pos, size_t width) {
    if (width == 0) return;
    assert(pos + width <= N);
    unsigned long* blocks = reinterpret_cast<unsigned long*>(&bs);
    size_t first_block = pos / bits_per_block;
    size_t last_block  = (pos + width - 1) / bits_per_block;
    size_t start_offset = pos % bits_per_block;
    size_t end_offset = (pos + width) % bits_per_block;
    if (first_block == last_block) {
        unsigned long mask = ((width == bits_per_block) ? ~0UL : ((1UL << width) - 1)) << start_offset;
        blocks[first_block] ^= mask;
    } else {
        unsigned long mask_first = ~0UL << start_offset;
        blocks[first_block] ^= mask_first;
        for (size_t i = first_block + 1; i < last_block; ++i) {
            blocks[i] ^= ~0UL;
        }
        if (end_offset == 0)
            blocks[last_block] ^= ~0UL;
        else {
            unsigned long mask_last = (1UL << end_offset) - 1;
            blocks[last_block] ^= mask_last;
        }
    }
}

// OVERWRITE: 指定区間を op の下位 width ビットで上書きする
template <size_t N>
void bitset_range_overwrite(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    if (width == 0) return;
    assert(pos + width <= N);
    unsigned long value = op & ((width == bits_per_block) ? ~0UL : ((1UL << width) - 1));
    unsigned long* blocks = reinterpret_cast<unsigned long*>(&bs);
    size_t first_block = pos / bits_per_block;
    size_t offset = pos % bits_per_block;
    if (offset + width <= bits_per_block) {
        unsigned long mask = ((width == bits_per_block) ? ~0UL : ((1UL << width) - 1)) << offset;
        blocks[first_block] = (blocks[first_block] & ~mask) | (value << offset);
    } else {
        size_t lower_bits = bits_per_block - offset;
        size_t upper_bits = width - lower_bits;
        unsigned long mask_first = (((1UL << lower_bits) - 1)) << offset;
        unsigned long value_first = value & ((1UL << lower_bits) - 1);
        blocks[first_block] = (blocks[first_block] & ~mask_first) | (value_first << offset);
        unsigned long mask_second = (upper_bits == bits_per_block) ? ~0UL : ((1UL << upper_bits) - 1);
        unsigned long value_second = value >> lower_bits;
        blocks[first_block + 1] = (blocks[first_block + 1] & ~mask_second) | value_second;
    }
}
