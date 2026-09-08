#include "bitset_range.hpp"
// #include <iostream>
// #include <chrono>
// #include <cassert>
// #include <bitset>
// #include <climits>

// using namespace std;

// -------------------------------------------------
// Expected (逐次処理) implementations for testing
// -------------------------------------------------

template <size_t N>
unsigned long inline_expected_extract(const bitset<N>& bs, size_t pos, size_t width) {
    unsigned long result = 0;
    for (size_t i = 0; i < width; ++i) {
        if (bs.test(pos + i))
            result |= (1UL << i);
    }
    return result;
}

template <size_t N>
bitset<N> expected_range_xor(const bitset<N>& orig, size_t pos, size_t width, unsigned long op) {
    bitset<N> res = orig;
    for (size_t i = 0; i < width; ++i) {
        bool bit = orig.test(pos + i) ^ ((op >> i) & 1UL);
        res.set(pos + i, bit);
    }
    return res;
}

template <size_t N>
bitset<N> expected_range_or(const bitset<N>& orig, size_t pos, size_t width, unsigned long op) {
    bitset<N> res = orig;
    for (size_t i = 0; i < width; ++i) {
        bool bit = orig.test(pos + i) | ((op >> i) & 1UL);
        res.set(pos + i, bit);
    }
    return res;
}

template <size_t N>
bitset<N> expected_range_and(const bitset<N>& orig, size_t pos, size_t width, unsigned long op) {
    bitset<N> res = orig;
    for (size_t i = 0; i < width; ++i) {
        bool bit = orig.test(pos + i) & ((op >> i) & 1UL);
        res.set(pos + i, bit);
    }
    return res;
}

template <size_t N>
bitset<N> expected_range_set(const bitset<N>& orig, size_t pos, size_t width, bool value) {
    bitset<N> res = orig;
    for (size_t i = pos; i < pos + width; ++i)
        res.set(i, value);
    return res;
}

template <size_t N>
bitset<N> expected_range_flip(const bitset<N>& orig, size_t pos, size_t width) {
    bitset<N> res = orig;
    for (size_t i = pos; i < pos + width; ++i)
        res.flip(i);
    return res;
}

template <size_t N>
bitset<N> expected_range_overwrite(const bitset<N>& orig, size_t pos, size_t width, unsigned long op) {
    bitset<N> res = orig;
    for (size_t i = 0; i < width; ++i) {
        bool bit = (op >> i) & 1UL;
        res.set(pos + i, bit);
    }
    return res;
}

// -------------------------------------------------
// Naive implementations (1ビットずつ更新)
// -------------------------------------------------

template <size_t N>
void naive_range_xor(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    for (size_t i = pos; i < pos + width; ++i) {
        bool bit = bs.test(i) ^ ((op >> (i - pos)) & 1UL);
        bs.set(i, bit);
    }
}

template <size_t N>
void naive_range_or(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    for (size_t i = pos; i < pos + width; ++i) {
        bool bit = bs.test(i) | ((op >> (i - pos)) & 1UL);
        bs.set(i, bit);
    }
}

template <size_t N>
void naive_range_and(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    for (size_t i = pos; i < pos + width; ++i) {
        bool bit = bs.test(i) & ((op >> (i - pos)) & 1UL);
        bs.set(i, bit);
    }
}

template <size_t N>
void naive_range_set(bitset<N>& bs, size_t pos, size_t width, bool value) {
    for (size_t i = pos; i < pos + width; ++i)
        bs.set(i, value);
}

template <size_t N>
void naive_range_flip(bitset<N>& bs, size_t pos, size_t width) {
    for (size_t i = pos; i < pos + width; ++i)
        bs.flip(i);
}

template <size_t N>
void naive_range_overwrite(bitset<N>& bs, size_t pos, size_t width, unsigned long op) {
    for (size_t i = pos; i < pos + width; ++i) {
        bool bit = (op >> (i - pos)) & 1UL;
        bs.set(i, bit);
    }
}

// -------------------------------------------------
// Benchmark helper functions
// -------------------------------------------------

// Func: (bitset<N>&, pos, width, op)
template <size_t N, typename Func>
unsigned long long benchmark_operation(Func opFunc, const bitset<N>& original, size_t pos, size_t width, unsigned long opValue, size_t iterations) {
    unsigned long long dummy = 0;
    for (size_t i = 0; i < iterations; ++i) {
        bitset<N> bs = original;
        opFunc(bs, pos, width, opValue);
        dummy += bs.count();
    }
    return dummy;
}

// Func: (bitset<N>&, pos, width)
template <size_t N, typename Func>
unsigned long long benchmark_operation_no_op(Func opFunc, const bitset<N>& original, size_t pos, size_t width, size_t iterations) {
    unsigned long long dummy = 0;
    for (size_t i = 0; i < iterations; ++i) {
        bitset<N> bs = original;
        opFunc(bs, pos, width);
        dummy += bs.count();
    }
    return dummy;
}

// Extraction benchmark
template <size_t N, typename Func>
unsigned long long benchmark_extraction(Func extractFunc, const bitset<N>& original, size_t pos, size_t width, size_t iterations) {
    unsigned long long dummy = 0;
    for (size_t i = 0; i < iterations; ++i) {
        dummy += extractFunc(original, pos, width);
    }
    return dummy;
}

int main() {
    constexpr size_t N = 1024;
    constexpr size_t iterations = 1000000;
    
    cout << "===== Functionality Tests =====" << endl;
    
    // Extraction Test
    {
        bitset<N> bs;
        for (size_t i = 0; i < N; ++i)
            bs.set(i, (i % 2 == 0));
        size_t pos = 100, width = 50;
        unsigned long custom_val = extract_bits(bs, pos, width);
        unsigned long expected_val = inline_expected_extract(bs, pos, width); // 使用するためにinlineラムダで実装
        auto inline_naive_extract = [&bs, pos, width]() -> unsigned long {
            unsigned long res = 0;
            for (size_t i = 0; i < width; ++i)
                if (bs.test(pos + i))
                    res |= (1UL << i);
            return res;
        };
        expected_val = inline_naive_extract();
        unsigned long naive_val = inline_naive_extract();
        assert(custom_val == expected_val && expected_val == naive_val);
        cout << "Extraction Test passed: pos " << pos << ", width " << width << ", value = " << custom_val << endl;
    }
    
    // XOR Test
    {
        bitset<N> bs;
        for (size_t i = 0; i < N; ++i)
            bs.set(i, (i % 2 == 0));
        size_t pos = 10, width = 20;
        unsigned long op = 0xABCDEF;
        bitset<N> bs_copy = bs;
        bitset<N> expected = expected_range_xor(bs_copy, pos, width, op);
        bitset_range_xor(bs, pos, width, op);
        assert(bs == expected);
        cout << "XOR Test passed: pos " << pos << ", width " << width << endl;
    }
    
    // OR Test
    {
        bitset<N> bs;
        for (size_t i = 0; i < N; ++i)
            bs.set(i, (i % 2 == 0));
        size_t pos = 60, width = 50;
        unsigned long op = 0x123456789ABCUL;
        bitset<N> bs_copy = bs;
        bitset<N> expected = expected_range_or(bs_copy, pos, width, op);
        bitset_range_or(bs, pos, width, op);
        assert(bs == expected);
        cout << "OR Test passed: pos " << pos << ", width " << width << endl;
    }
    
    // AND Test
    {
        bitset<N> bs;
        for (size_t i = 0; i < N; ++i)
            bs.set(i, (i % 2 == 0));
        size_t pos = 200, width = 50;
        unsigned long op = 0x0F0F0F0FUL;
        bitset<N> bs_copy = bs;
        bitset<N> expected = expected_range_and(bs_copy, pos, width, op);
        bitset_range_and(bs, pos, width, op);
        assert(bs == expected);
        cout << "AND Test passed: pos " << pos << ", width " << width << endl;
    }
    
    // SET Test
    {
        bitset<N> bs;
        bs.reset();
        size_t pos = 300, width = 50;
        bool set_value = true;
        bitset<N> bs_copy = bs;
        bitset<N> expected = expected_range_set(bs_copy, pos, width, set_value);
        bitset_range_set(bs, pos, width, set_value);
        assert(bs == expected);
        cout << "SET Test passed: pos " << pos << ", width " << width << endl;
    }
    
    // FLIP Test
    {
        bitset<N> bs;
        for (size_t i = 0; i < N; ++i)
            bs.set(i, (i % 2 == 0));
        size_t pos = 400, width = 50;
        bitset<N> bs_copy = bs;
        bitset<N> expected = expected_range_flip(bs_copy, pos, width);
        bitset_range_flip(bs, pos, width);
        assert(bs == expected);
        cout << "FLIP Test passed: pos " << pos << ", width " << width << endl;
    }
    
    // OVERWRITE Test
    {
        bitset<N> bs;
        for (size_t i = 0; i < N; ++i)
            bs.set(i, (i % 2 == 0));
        size_t pos = 500, width = 50;
        unsigned long op_overwrite = 0xDEADBEEFUL;
        bitset<N> bs_copy = bs;
        bitset<N> expected = expected_range_overwrite(bs_copy, pos, width, op_overwrite);
        bitset_range_overwrite(bs, pos, width, op_overwrite);
        assert(bs == expected);
        cout << "OVERWRITE Test passed: pos " << pos << ", width " << width << endl;
    }
    
    cout << "\nAll functional tests passed!" << endl << endl;
    
    cout << "===== Benchmark Tests =====" << endl;
    
    bitset<N> original;
    for (size_t i = 0; i < N; ++i)
        original.set(i, (i % 2 == 0));
    
    // Extraction Benchmark
    {
        size_t pos = 100, width = 50;
        auto start = chrono::high_resolution_clock::now();
        unsigned long long dummy_custom = benchmark_extraction<N>(extract_bits<N>, original, pos, width, iterations);
        auto end = chrono::high_resolution_clock::now();
        auto duration_custom = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        start = chrono::high_resolution_clock::now();
        unsigned long long dummy_naive = benchmark_extraction<N>(
            [](const bitset<N>& bs, size_t p, size_t w) -> unsigned long {
                unsigned long res = 0;
                for (size_t i = 0; i < w; ++i)
                    if (bs.test(p + i))
                        res |= (1UL << i);
                return res;
            }, original, pos, width, iterations);
        end = chrono::high_resolution_clock::now();
        auto duration_naive = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "Extraction - Custom: " << duration_custom << " us, dummy = " << dummy_custom << "\n";
        cout << "Extraction - Naive:  " << duration_naive << " us, dummy = " << dummy_naive << "\n\n";
    }
    
    // XOR Benchmark
    {
        size_t pos = 100, width = 50;
        unsigned long op = 0x123456789ABCUL;
        auto start = chrono::high_resolution_clock::now();
        unsigned long long dummy_custom = benchmark_operation<N>(bitset_range_xor<N>, original, pos, width, op, iterations);
        auto end = chrono::high_resolution_clock::now();
        auto duration_custom = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        start = chrono::high_resolution_clock::now();
        unsigned long long dummy_naive = benchmark_operation<N>(naive_range_xor<N>, original, pos, width, op, iterations);
        end = chrono::high_resolution_clock::now();
        auto duration_naive = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "XOR - Custom: " << duration_custom << " us, dummy = " << dummy_custom << "\n";
        cout << "XOR - Naive:  " << duration_naive << " us, dummy = " << dummy_naive << "\n\n";
    }
    
    // OR Benchmark
    {
        size_t pos = 200, width = 50;
        unsigned long op = 0xABCDEFUL;
        auto start = chrono::high_resolution_clock::now();
        unsigned long long dummy_custom = benchmark_operation<N>(bitset_range_or<N>, original, pos, width, op, iterations);
        auto end = chrono::high_resolution_clock::now();
        auto duration_custom = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        start = chrono::high_resolution_clock::now();
        unsigned long long dummy_naive = benchmark_operation<N>(naive_range_or<N>, original, pos, width, op, iterations);
        end = chrono::high_resolution_clock::now();
        auto duration_naive = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "OR  - Custom: " << duration_custom << " us, dummy = " << dummy_custom << "\n";
        cout << "OR  - Naive:  " << duration_naive << " us, dummy = " << dummy_naive << "\n\n";
    }
    
    // AND Benchmark
    {
        size_t pos = 300, width = 50;
        unsigned long op = 0x0F0F0F0FUL;
        auto start = chrono::high_resolution_clock::now();
        unsigned long long dummy_custom = benchmark_operation<N>(bitset_range_and<N>, original, pos, width, op, iterations);
        auto end = chrono::high_resolution_clock::now();
        auto duration_custom = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        start = chrono::high_resolution_clock::now();
        unsigned long long dummy_naive = benchmark_operation<N>(naive_range_and<N>, original, pos, width, op, iterations);
        end = chrono::high_resolution_clock::now();
        auto duration_naive = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "AND - Custom: " << duration_custom << " us, dummy = " << dummy_custom << "\n";
        cout << "AND - Naive:  " << duration_naive << " us, dummy = " << dummy_naive << "\n\n";
    }
    
    // SET Benchmark
    {
        size_t pos = 400, width = 50;
        bool set_value = true;
        auto start = chrono::high_resolution_clock::now();
        unsigned long long dummy_custom = 0;
        for (size_t i = 0; i < iterations; ++i) {
            bitset<N> bs = original;
            bitset_range_set(bs, pos, width, set_value);
            dummy_custom += bs.count();
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration_custom = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        start = chrono::high_resolution_clock::now();
        unsigned long long dummy_naive = 0;
        for (size_t i = 0; i < iterations; ++i) {
            bitset<N> bs = original;
            naive_range_set(bs, pos, width, set_value);
            dummy_naive += bs.count();
        }
        end = chrono::high_resolution_clock::now();
        auto duration_naive = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "SET - Custom: " << duration_custom << " us, dummy = " << dummy_custom << "\n";
        cout << "SET - Naive:  " << duration_naive << " us, dummy = " << dummy_naive << "\n\n";
    }
    
    // FLIP Benchmark
    {
        size_t pos = 500, width = 50;
        auto start = chrono::high_resolution_clock::now();
        unsigned long long dummy_custom = 0;
        for (size_t i = 0; i < iterations; ++i) {
            bitset<N> bs = original;
            bitset_range_flip(bs, pos, width);
            dummy_custom += bs.count();
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration_custom = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        start = chrono::high_resolution_clock::now();
        unsigned long long dummy_naive = 0;
        for (size_t i = 0; i < iterations; ++i) {
            bitset<N> bs = original;
            naive_range_flip(bs, pos, width);
            dummy_naive += bs.count();
        }
        end = chrono::high_resolution_clock::now();
        auto duration_naive = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "FLIP - Custom: " << duration_custom << " us, dummy = " << dummy_custom << "\n";
        cout << "FLIP - Naive:  " << duration_naive << " us, dummy = " << dummy_naive << "\n\n";
    }
    
    // OVERWRITE Benchmark
    {
        size_t pos = 600, width = 50;
        unsigned long op = 0xDEADBEEFUL;
        auto start = chrono::high_resolution_clock::now();
        unsigned long long dummy_custom = benchmark_operation<N>(bitset_range_overwrite<N>, original, pos, width, op, iterations);
        auto end = chrono::high_resolution_clock::now();
        auto duration_custom = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        start = chrono::high_resolution_clock::now();
        unsigned long long dummy_naive = benchmark_operation<N>(naive_range_overwrite<N>, original, pos, width, op, iterations);
        end = chrono::high_resolution_clock::now();
        auto duration_naive = chrono::duration_cast<chrono::microseconds>(end - start).count();
        
        cout << "OVERWRITE - Custom: " << duration_custom << " us, dummy = " << dummy_custom << "\n";
        cout << "OVERWRITE - Naive:  " << duration_naive << " us, dummy = " << dummy_naive << "\n\n";
    }
    
    cout << "===== Benchmarking Completed =====" << endl;
    
    return 0;
}
