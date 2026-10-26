#pragma once
#include <bits/stdc++.h>

// 整数座標の外積と偏角ソート。座標は std::pair の (x, y)
// int は long long、long long は __int128 で計算し、各座標型の全範囲で正確に扱う
// arg_sort は正の x 軸から反時計回り。原点は先頭、同方向の点同士の順序は不定
// 使用例: std::vector<std::pair<int, int>> p = {{0, 1}, {1, 0}}; arg_sort(p);

// int 座標の外積 ax * by - ay * bx を long long で返す O(1)
inline long long cross(std::pair<int, int> a, std::pair<int, int> b) {
    return 1LL * a.first * b.second - 1LL * a.second * b.first;
}

// int の点列を偏角順に並べ替える O(N log N)
inline void arg_sort(std::vector<std::pair<int, int>>& points) {
    std::sort(points.begin(), points.end(), [](auto a, auto b) {
        // 原点、[0, π)、[π, 2π) の3群に分ける
        const auto [ax, ay] = a;
        const auto [bx, by] = b;
        const int ga = (ax == 0 && ay == 0) ? -1 : (ay < 0 || (ay == 0 && ax < 0));
        const int gb = (bx == 0 && by == 0) ? -1 : (by < 0 || (by == 0 && bx < 0));

        // 群が同じなら、外積が正になる順に並べる
        return ga != gb ? ga < gb : cross(a, b) > 0;
    });
}

// long long 座標の外積 ax * by - ay * bx を __int128 で返す O(1)
inline __int128 cross(std::pair<long long, long long> a, std::pair<long long, long long> b) {
    return static_cast<__int128>(a.first) * b.second
         - static_cast<__int128>(a.second) * b.first;
}

// long long の点列を偏角順に並べ替える O(N log N)
inline void arg_sort(std::vector<std::pair<long long, long long>>& points) {
    std::sort(points.begin(), points.end(), [](auto a, auto b) {
        // 原点、[0, π)、[π, 2π) の3群に分ける
        const auto [ax, ay] = a;
        const auto [bx, by] = b;
        const int ga = (ax == 0 && ay == 0) ? -1 : (ay < 0 || (ay == 0 && ax < 0));
        const int gb = (bx == 0 && by == 0) ? -1 : (by < 0 || (by == 0 && bx < 0));

        // 群が同じなら、外積が正になる順に並べる
        return ga != gb ? ga < gb : cross(a, b) > 0;
    });
}

#if __INCLUDE_LEVEL__ == 0
namespace arg_sort_test {

// 非負の分数を連分数で比較する参照実装。分母は正で、乗算は使わない
bool fraction_less(unsigned long long an, unsigned long long ad,
                   unsigned long long bn, unsigned long long bd) {
    bool reversed = false;
    for (;;) {
        // 整数部分が異なれば大小が決まる
        const auto aq = an / ad, bq = bn / bd;
        if (aq != bq) return reversed ? aq > bq : aq < bq;

        // 小数部分が0なら終了し、そうでなければ逆数にして大小を反転する
        const auto ar = an % ad, br = bn % bd;
        if (ar == 0 || br == 0) {
            if (ar == br) return false;
            return reversed ? br == 0 : ar == 0;
        }
        an = ad;
        ad = ar;
        bn = bd;
        bd = br;
        reversed = !reversed;
    }
}

// 原点と4象限に分け、象限内の傾きを連分数で比べる独立した参照実装
bool reference_less(const std::pair<long long, long long>& a,
                    const std::pair<long long, long long>& b) {
    const auto quadrant = [](const auto& p) {
        const auto [x, y] = p;
        if (x == 0 && y == 0) return -1;
        if (x > 0 && y >= 0) return 0;
        if (x <= 0 && y > 0) return 1;
        if (x < 0 && y <= 0) return 2;
        return 3;
    };
    const int qa = quadrant(a), qb = quadrant(b);
    if (qa != qb) return qa < qb;
    if (qa == -1) return false;

    // LLONG_MIN の絶対値も、符号なし整数への変換後に計算すれば保持できる
    const auto magnitude = [](long long v) {
        const auto u = static_cast<unsigned long long>(v);
        return v >= 0 ? u : 0ULL - u;
    };
    const auto ax = magnitude(a.first), ay = magnitude(a.second);
    const auto bx = magnitude(b.first), by = magnitude(b.second);
    if (qa % 2 == 0) return fraction_less(ay, ax, by, bx);
    return fraction_less(ax, ay, bx, by);
}

// 小さい入力を参照比較で安定な挿入ソートにかける
void reference_sort(std::vector<std::pair<long long, long long>>& points) {
    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto p = points[i];
        std::size_t j = i;
        while (j > 0 && reference_less(p, points[j - 1])) {
            points[j] = points[j - 1];
            --j;
        }
        points[j] = p;
    }
}

// 符号と絶対値を分離し、符号なし整数で外積の期待値を計算する
__int128 reference_cross(std::pair<long long, long long> a,
                         std::pair<long long, long long> b) {
    const auto magnitude = [](long long v) -> unsigned __int128 {
        const auto u = static_cast<unsigned long long>(v);
        return v >= 0 ? u : 0ULL - u;
    };
    const auto p = magnitude(a.first) * magnitude(b.second);
    const auto q = magnitude(a.second) * magnitude(b.first);
    const bool negative_p = (a.first < 0) != (b.second < 0);
    const bool negative_q = (a.second < 0) != (b.first < 0);

    // 積の符号が異なる場合も、外積の絶対値は符号付き128ビットに収まる
    if (negative_p != negative_q) {
        assert(p + q < (static_cast<unsigned __int128>(1) << 127));
        const auto sum = static_cast<__int128>(p + q);
        return negative_p ? -sum : sum;
    }
    const auto difference = p >= q ? static_cast<__int128>(p - q)
                                   : -static_cast<__int128>(q - p);
    return negative_p ? -difference : difference;
}

// 実際のソートを独立した挿入ソートと比べ、点の欠落や重複も確認する
// 同方向の点同士には順序を要求しない
// テスト専用であり、ライブラリとして include した場合はコンパイルされない
template <class T>
void check_sort(std::vector<std::pair<T, T>> input) {
    std::vector<std::pair<long long, long long>> expected(input.begin(), input.end());
    reference_sort(expected);
    auto actual = input;
    arg_sort(actual);
    for (std::size_t i = 0; i < actual.size(); ++i) {
        assert(!reference_less(actual[i], expected[i]));
        assert(!reference_less(expected[i], actual[i]));
    }

    // 角度によらない辞書順で、入力の点がすべて残っていることを確認する
    std::sort(input.begin(), input.end());
    std::sort(actual.begin(), actual.end());
    assert(actual == input);
}

template <class T>
void run_tests() {
    using Point = std::pair<T, T>;
    const T lo = std::numeric_limits<T>::min(), hi = std::numeric_limits<T>::max();
    const auto check_cross = [](Point a, Point b) {
        assert(cross(a, b) == reference_cross(a, b));
        assert(cross(a, b) == -cross(b, a));
    };

    // 空列、1点、原点のみ、主要8方向を確認する
    check_sort<T>({});
    check_sort<T>({{-3, 4}});
    check_sort<T>(std::vector<Point>(100, {0, 0}));
    const std::vector<Point> directions = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1},
        {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
    };
    auto reversed = directions;
    std::reverse(reversed.begin(), reversed.end());
    arg_sort(reversed);
    assert(reversed == directions);

    // 外積が ±1 のほぼ平行な点、同方向の点、各型の最大級の外積
    const Point near_a{hi - 1, hi - 2}, near_b{hi, hi - 1};
    assert(cross(near_a, near_b) == 1);
    assert(cross(near_b, near_a) == -1);
    assert(cross(Point{lo, lo}, Point{lo, hi}) < 0);
    assert(cross(Point{lo, hi}, Point{lo, lo}) > 0);
    assert(cross(Point{1, 1}, Point{hi, hi}) == 0);
    check_sort<T>({near_b, near_a, {0, 0}, {1, 1}, {hi, hi}});

    // 境界値と小さい格子の全点対で外積を検証する
    const std::vector<T> values = {lo, T(lo + 1), T(-1000000000), T(-2), T(-1), T(0),
                                   T(1), T(2), T(1000000000), T(hi - 1), hi};
    std::vector<Point> boundary, grid;
    for (T x : values) {
        for (T y : values) boundary.emplace_back(x, y);
    }
    for (int x = -8; x <= 8; ++x) {
        for (int y = -8; y <= 8; ++y) grid.emplace_back(T(x), T(y));
    }
    for (const auto& points : {boundary, grid}) {
        for (auto a : points) {
            for (auto b : points) check_cross(a, b);
        }
        check_sort<T>(points);
    }

    // 原点を含む全3点の組合せを、実際の arg_sort へ渡して確認する
    std::vector<Point> small;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) small.emplace_back(T(x), T(y));
    }
    for (auto a : small) {
        for (auto b : small) {
            for (auto c : small) check_sort<T>({a, b, c});
        }
    }

    // 各型の全範囲の乱数に、境界値、原点、同方向の点を混ぜる
    std::mt19937_64 rng(0);
    const auto random_coordinate = [&]() -> T {
        if (rng() % 3 == 0) return values[rng() % values.size()];
        if (rng() % 2 == 0) return T(static_cast<int>(rng() % 17) - 8);
        return std::bit_cast<T>(static_cast<std::make_unsigned_t<T>>(rng()));
    };
    const auto random_point = [&]() -> Point {
        return {random_coordinate(), random_coordinate()};
    };
    for (int trial = 0; trial < 300000; ++trial) {
        check_cross(random_point(), random_point());
    }
    for (int trial = 0; trial < 5000; ++trial) {
        std::vector<Point> points(static_cast<std::size_t>(rng() % 81));
        for (auto& p : points) p = random_point();
        check_sort<T>(points);
    }
}

}  // namespace arg_sort_test

int main() {
    static_assert(std::is_same_v<decltype(cross(std::pair<int, int>{},
                                               std::pair<int, int>{})), long long>);
    static_assert(std::is_same_v<decltype(cross(std::pair<long long, long long>{},
                                               std::pair<long long, long long>{})), __int128>);
    arg_sort_test::run_tests<int>();
    arg_sort_test::run_tests<long long>();
    std::cout << "All tests passed (int and long long)\n";
}
#endif
