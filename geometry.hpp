#pragma once
#include <bits/stdc++.h>

// 整数座標を主対象にした競技プログラミング用2次元幾何ライブラリ
// 判定・面積2倍・距離2乗・格子点数など、整数で正確に扱える値は整数のまま返す
// 交点・射影・距離・周長など、一般に整数で表せない値のみ浮動小数点型で返す
// デフォルト座標型は long long、内部で自動的な型拡張は行わない
// 2乗値や積和の精度が必要な場合は、利用側で十分な座標型を選ぶ
// 点集合・多角形の頻出補助関数も、整数で扱えるものを中心に同梱する


namespace geo {

// 円周率を返す定数 O(1)
// 用途: 角度変換、円の面積・周長、扇形面積など実数値が必要な計算に使う
// 使い方: geo::PI<double> のように浮動小数点型を指定して使う
// 注意: 整数座標の厳密判定には使わず、実数値を出力したい場面向け
template<class F>
inline constexpr F PI = F(3.141592653589793238462643383279502884L);

// 度数法をラジアンへ変換する O(1)
// 用途: 入力角度がdegreeで与えられ、rotateや三角関数に渡したいときに使う
// 使い方: auto rad = deg_to_rad<double>(90);
// 注意: 戻り値は浮動小数点で、判定用途ではない
template<class F = double, class X>
F deg_to_rad(X deg) {
    return static_cast<F>(deg) * PI<F> / F(180);
}

// ラジアンを度数法へ変換する O(1)
// 用途: 計算した角度をdegreeで出力したいときに使う
// 使い方: auto deg = rad_to_deg<double>(theta);
// 注意: 戻り値は浮動小数点で、判定用途ではない
template<class F = double, class X>
F rad_to_deg(X rad) {
    return static_cast<F>(rad) * F(180) / PI<F>;
}

// 角度を [0, 2π) に正規化する O(1)
// 用途: 偏角差、回転後の角度管理、円周上の範囲判定の前処理に使う
// 使い方: normalize_angle<double>(theta) は負角や2π以上の角も標準範囲へ移す
// 注意: 浮動小数点誤差を含むため、整数幾何の厳密判定には使わない
template<class F = double, class X>
F normalize_angle(X theta) {
    F two_pi = F(2) * PI<F>;
    F t = std::fmod(static_cast<F>(theta), two_pi);
    if (t < F(0)) t += two_pi;
    if (t >= two_pi) t -= two_pi;
    return t;
}

// 2角の小さい方の差を [0,π] で返す O(1)
// 用途: 向きの近さ、視野角、円周上の最短角度差を求めるときに使う
// 使い方: angle_diff<double>(a,b) は a と b の絶対的な角度差を返す
// 注意: 返り値はラジアン
template<class F = double, class X, class Y>
F angle_diff(X a, Y b) {
    F d = std::abs(normalize_angle<F>(a) - normalize_angle<F>(b));
    return std::min(d, F(2) * PI<F> - d);
}

namespace internal {

template<class T>
constexpr int sign(T x) {
    return (T(0) < x) - (x < T(0));
}

template<class T>
constexpr T abs_value(T x) {
    return x < T(0) ? -x : x;
}

template<class T>
T gcd_abs(T a, T b) {
    T x = abs_value(a);
    T y = abs_value(b);
    while (y != T(0)) {
        T r = x % y;
        x = y;
        y = r;
    }
    return x;
}

}  // namespace internal
template<class T = long long>
struct Point {
    T x{};
    T y{};

    // 点を生成する O(1)
    constexpr Point() = default;

    // 座標を指定して点を生成する O(1)
    constexpr Point(T x_, T y_) : x(x_), y(y_) {}

    // 別の座標型から点を生成する O(1)
    template<class U>
    explicit constexpr Point(const Point<U>& p) : x(static_cast<T>(p.x)), y(static_cast<T>(p.y)) {}

    // 単項プラスを返す O(1)
    constexpr Point operator+() const { return *this; }

    // 原点対称の点を返す O(1)
    constexpr Point operator-() const { return Point(-x, -y); }

    // 2点の和を返す O(1)
    constexpr Point operator+(const Point& p) const { return Point(x + p.x, y + p.y); }

    // 2点の差を返す O(1)
    constexpr Point operator-(const Point& p) const { return Point(x - p.x, y - p.y); }

    // スカラー倍した点を返す O(1)
    template<class S>
    constexpr Point<std::common_type_t<T, S>> operator*(S k) const {
        using R = std::common_type_t<T, S>;
        return Point<R>(static_cast<R>(x) * static_cast<R>(k), static_cast<R>(y) * static_cast<R>(k));
    }

    // スカラーで割った点を返す O(1)
    template<class S>
    constexpr Point<std::common_type_t<T, S>> operator/(S k) const {
        using R = std::common_type_t<T, S>;
        return Point<R>(static_cast<R>(x) / static_cast<R>(k), static_cast<R>(y) / static_cast<R>(k));
    }

    // 代入加算を行う O(1)
    constexpr Point& operator+=(const Point& p) {
        x += p.x;
        y += p.y;
        return *this;
    }

    // 代入減算を行う O(1)
    constexpr Point& operator-=(const Point& p) {
        x -= p.x;
        y -= p.y;
        return *this;
    }

    // 辞書順で比較する O(1)
    constexpr bool operator<(const Point& p) const {
        if (x != p.x) return x < p.x;
        return y < p.y;
    }

    // 辞書順で比較する O(1)
    constexpr bool operator>(const Point& p) const { return p < *this; }

    // 座標が一致するか判定する O(1)
    constexpr bool operator==(const Point& p) const { return x == p.x && y == p.y; }

    // 座標が異なるか判定する O(1)
    constexpr bool operator!=(const Point& p) const { return !(*this == p); }

    // 内積を返す O(1)
    template<class U>
    constexpr std::common_type_t<T, U> dot(const Point<U>& p) const {
        using R = std::common_type_t<T, U>;
        return static_cast<R>(x) * static_cast<R>(p.x) + static_cast<R>(y) * static_cast<R>(p.y);
    }

    // 外積を返す O(1)
    template<class U>
    constexpr std::common_type_t<T, U> cross(const Point<U>& p) const {
        using R = std::common_type_t<T, U>;
        return static_cast<R>(x) * static_cast<R>(p.y) - static_cast<R>(y) * static_cast<R>(p.x);
    }

    // 原点からの距離2乗を返す O(1)
    constexpr T norm2() const { return x * x + y * y; }

    // 2点間距離2乗を返す O(1)
    constexpr T dist2(const Point& p) const {
        T dx = x - p.x;
        T dy = y - p.y;
        return dx * dx + dy * dy;
    }

    // 原点からの距離を返す O(1)
    template<class F = double>
    F norm() const { return std::sqrt(static_cast<F>(norm2())); }

    // 2点間距離を返す O(1)
    template<class F = double>
    F dist(const Point& p) const { return std::sqrt(static_cast<F>(dist2(p))); }

    // 偏角をラジアンで返す O(1)
    template<class F = double>
    F arg() const { return std::atan2(static_cast<F>(y), static_cast<F>(x)); }

    // 単位ベクトルを返す O(1)
    template<class F = double>
    Point<F> unit() const {
        F d = norm<F>();
        if (d == F(0)) return Point<F>(0, 0);
        return Point<F>(static_cast<F>(x) / d, static_cast<F>(y) / d);
    }

    // 浮動小数点座標に変換する O(1)
    template<class F = double>
    Point<F> to_real() const { return Point<F>(static_cast<F>(x), static_cast<F>(y)); }

    // ゼロベクトルか判定する O(1)
    constexpr bool is_zero() const { return x == T(0) && y == T(0); }

    // 平行か判定する O(1)
    bool is_parallel(const Point& p) const { return internal::sign(cross(p)) == 0; }

    // 直交するか判定する O(1)
    bool is_orthogonal(const Point& p) const { return internal::sign(dot(p)) == 0; }

    // 反時計回りに90度回転したベクトルを返す O(1)
    constexpr Point rot90() const { return Point(-y, x); }

    // 時計回りに90度回転したベクトルを返す O(1)
    constexpr Point rot270() const { return Point(y, -x); }

    // 近い整数座標へ丸めて変換できるか判定する O(1)
    // 用途: 交点や射影を実数で求めた後、格子点として扱えるか確認したいときに使う
    // 使い方: auto [ok, q] = p.try_round_to_int<long long>(); ok=falseでもqにはround後の点が入る
    // 注意: 判定はeps以内の丸め確認だけで、厳密な有理数判定ではない
    template<class I = long long, class F = double>
    std::pair<bool, Point<I>> try_round_to_int(F eps = F(1e-9)) const {
        F vx = static_cast<F>(x);
        F vy = static_cast<F>(y);
        F rx = std::round(vx);
        F ry = std::round(vy);
        bool ok = std::abs(vx - rx) <= eps && std::abs(vy - ry) <= eps;
        return {ok, Point<I>(static_cast<I>(rx), static_cast<I>(ry))};
    }
};

// 左スカラー倍した点を返す O(1)
template<class S, class T>
constexpr Point<std::common_type_t<S, T>> operator*(S k, const Point<T>& p) {
    return p * k;
}

// 点を出力する O(1)
template<class T>
std::ostream& operator<<(std::ostream& os, const Point<T>& p) {
    return os << '[' << p.x << ',' << p.y << ']';
}

// 点を入力する O(1)
template<class T>
std::istream& operator>>(std::istream& is, Point<T>& p) {
    return is >> p.x >> p.y;
}

template<class T = long long>
using Vector = Point<T>;

// 内積を返す O(1)
template<class T, class U>
constexpr std::common_type_t<T, U> dot(const Point<T>& a, const Point<U>& b) {
    return a.dot(b);
}

// 外積を返す O(1)
template<class T, class U>
constexpr std::common_type_t<T, U> cross(const Point<T>& a, const Point<U>& b) {
    return a.cross(b);
}

// 原点からの距離2乗を返す O(1)
template<class T>
constexpr T norm2(const Point<T>& p) {
    return p.norm2();
}

// 2点間距離2乗を返す O(1)
template<class T>
constexpr T dist2(const Point<T>& a, const Point<T>& b) {
    return a.dist2(b);
}

// 2点間距離を返す O(1)
template<class F = double, class T>
F dist(const Point<T>& a, const Point<T>& b) {
    return a.template dist<F>(b);
}

// マンハッタン距離を返す O(1)
// 用途: グリッド上で上下左右移動だけが許される最短距離を求めるときに使う
// 使い方: manhattan_dist(a, b) は |ax-bx|+|ay-by| を返す
template<class T>
T manhattan_dist(const Point<T>& a, const Point<T>& b) {
    T dx = internal::abs_value(a.x - b.x);
    T dy = internal::abs_value(a.y - b.y);
    return dx + dy;
}

// チェビシェフ距離を返す O(1)
// 用途: 8近傍移動や正方形到達範囲での距離を求めるときに使う
// 使い方: chebyshev_dist(a, b) は max(|ax-bx|, |ay-by|) を返す
template<class T>
T chebyshev_dist(const Point<T>& a, const Point<T>& b) {
    T dx = internal::abs_value(a.x - b.x);
    T dy = internal::abs_value(a.y - b.y);
    return std::max(dx, dy);
}

// 点集合の軸平行包絡矩形を返す O(N)
// 用途: 点集合の範囲、座標圧縮前の最小最大、当たり判定の粗いフィルタに使う
// 使い方: auto box = bounding_box(ps); 空集合ならnullopt、値は{最小座標点, 最大座標点}
template<class T>
std::optional<std::pair<Point<T>, Point<T>>> bounding_box(const std::vector<Point<T>>& ps) {
    if (ps.empty()) return std::nullopt;
    Point<T> lo = ps[0];
    Point<T> hi = ps[0];
    for (const auto& p : ps) {
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
    }
    return std::pair<Point<T>, Point<T>>(lo, hi);
}

// 点集合の最大マンハッタン距離を返す O(N)
// 用途: 全点対のL1距離最大値をO(N^2)せずに求めたいときに使う
// 使い方: max_manhattan_dist(ps) は max(|xi-xj|+|yi-yj|) を返す
// 原理: x+y と x-y の最大差の大きい方が答えになる
template<class T>
T max_manhattan_dist(const std::vector<Point<T>>& ps) {
    if (ps.size() < 2) return T(0);
    T min_sum = ps[0].x + ps[0].y;
    T max_sum = min_sum;
    T min_diff = ps[0].x - ps[0].y;
    T max_diff = min_diff;
    for (const auto& p : ps) {
        T sum = p.x + p.y;
        T diff = p.x - p.y;
        min_sum = std::min(min_sum, sum);
        max_sum = std::max(max_sum, sum);
        min_diff = std::min(min_diff, diff);
        max_diff = std::max(max_diff, diff);
    }
    return std::max(max_sum - min_sum, max_diff - min_diff);
}

// 点集合の最大チェビシェフ距離を返す O(N)
// 用途: 全点対のL∞距離最大値や、正方形半径の最小値を求めたいときに使う
// 使い方: max_chebyshev_dist(ps) は max(max(|dx|,|dy|)) を返す
template<class T>
T max_chebyshev_dist(const std::vector<Point<T>>& ps) {
    if (ps.size() < 2) return T(0);
    T min_x = ps[0].x;
    T max_x = ps[0].x;
    T min_y = ps[0].y;
    T max_y = ps[0].y;
    for (const auto& p : ps) {
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    return std::max(max_x - min_x, max_y - min_y);
}

// 点を原点まわりに指定ラジアンだけ回転する O(1)
// 用途: 回転後の座標が整数とは限らない幾何計算や可視化補助に使う
// 使い方: rotate<double>(p, theta) のthetaはラジアン、返り値はPoint<double>
// 注意: 判定用途ではなく、実数座標が必要な場面向け
template<class F = double, class T>
Point<F> rotate(const Point<T>& p, F theta) {
    F co = std::cos(theta);
    F si = std::sin(theta);
    F x = static_cast<F>(p.x);
    F y = static_cast<F>(p.y);
    return Point<F>(x * co - y * si, x * si + y * co);
}

// 点を中心点まわりに指定ラジアンだけ回転する O(1)
// 用途: ある点を中心に図形や点を回転させた座標を求めたいときに使う
// 使い方: rotate<double>(p, theta, center) はcenterを固定してpを回す
// 注意: 判定用途ではなく、実数座標が必要な場面向け
template<class F = double, class T>
Point<F> rotate(const Point<T>& p, F theta, const Point<T>& center) {
    return center.template to_real<F>() + rotate<F>(p - center, theta);
}

// 指定角度の単位ベクトルを返す O(1)
// 用途: 角度から方向ベクトルを作りたいとき、レイや円周上の点の生成に使う
// 使い方: unit_vector<double>(theta) は {cos(theta), sin(theta)} を返す
// 注意: 実数座標を返す補助であり、整数判定には使わない
template<class F = double, class X>
Point<F> unit_vector(X theta) {
    F t = static_cast<F>(theta);
    return Point<F>(std::cos(t), std::sin(t));
}

// 極座標から点を返す O(1)
// 用途: 半径と角度から円周上・扇形上の点を生成したいときに使う
// 使い方: polar<double>(r,theta) は r*{cos(theta),sin(theta)} を返す
// 注意: 返り値は実数座標
template<class F = double, class R, class X>
Point<F> polar(R r, X theta) {
    return unit_vector<F>(theta) * static_cast<F>(r);
}

// 角abcの大きさをラジアンで返す O(1)
// 用途: 角度制約、扇形、円周角など、実数角度が必要な問題で使う
// 使い方: angle(a, b, c) は頂点bでの小さい方の角を[0,pi]で返す
// 注意: atan2を使うため浮動小数点誤差を含む
template<class F = double, class T>
F angle(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto u = a - b;
    auto v = c - b;
    F cr = std::abs(static_cast<F>(u.cross(v)));
    F dt = static_cast<F>(u.dot(v));
    return std::atan2(cr, dt);
}

enum Ccw : int {
    COUNTER_CLOCKWISE = 1,
    CLOCKWISE = -1,
    ONLINE_BACK = 2,
    ONLINE_FRONT = -2,
    ON_SEGMENT = 0,
};

enum Contains : int {
    OUTSIDE = 0,
    ON_EDGE = 1,
    INSIDE = 2,
};

enum CircleRelation : int {
    CIRCLE_SEPARATE = 0,
    CIRCLE_EXTERNAL_TANGENT = 1,
    CIRCLE_INTERSECT = 2,
    CIRCLE_INTERNAL_TANGENT = 3,
    CIRCLE_CONTAIN = 4,
    CIRCLE_SAME = 5,
};

// a->b->c の向きを返す O(1)
// 用途: 左右判定、線分交差、凸包、点の内包判定など整数幾何の基本判定に使う
// 使い方: 返り値は反時計回りなら1、時計回りなら-1、一直線上なら0
template<class T>
int orient(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return internal::sign((b - a).cross(c - a));
}

// 点cの直線abに対する詳細な位置関係を返す O(1)
// 用途: 線分上判定や端点より外側かどうかを、向き判定より詳しく知りたいときに使う
// 使い方: ccw(a,b,c) はa,bを基準にcがON_SEGMENT/ONLINE_FRONT/ONLINE_BACKなどを返す
template<class T>
Ccw ccw(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    Point<T> ab = b - a;
    Point<T> ac = c - a;
    int s = internal::sign(ab.cross(ac));
    if (s > 0) return COUNTER_CLOCKWISE;
    if (s < 0) return CLOCKWISE;
    if (internal::sign(ab.dot(ac)) < 0) return ONLINE_BACK;
    if (ab.norm2() < ac.norm2()) return ONLINE_FRONT;
    return ON_SEGMENT;
}

// 3点が同一直線上にあるか判定する O(1)
// 用途: 三角形が退化しているか、点が同一直線上に並ぶかを確認するときに使う
// 使い方: is_collinear(a, b, c) はorient(a,b,c)==0と同じ意味
template<class T>
bool is_collinear(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return orient(a, b, c) == 0;
}

// 点集合が同一直線上にあるか判定する O(N)
// 用途: 凸包や多角形処理の前に、点集合が退化しているか確認したいときに使う
// 使い方: 空集合、1点、全点同一点、2点以下はtrueを返す
template<class T>
bool is_collinear(const std::vector<Point<T>>& ps) {
    int n = static_cast<int>(ps.size());
    if (n <= 2) return true;
    int j = 1;
    while (j < n && ps[j] == ps[0]) ++j;
    if (j == n) return true;
    for (int i = j + 1; i < n; ++i) {
        if (orient(ps[0], ps[j], ps[i]) != 0) return false;
    }
    return true;
}

// 偏角ソート用の比較を行う O(1)
// 用途: atan2を使わず、整数外積でベクトルを反時計回り順に並べたいときに使う
// 使い方: std::sort(ps.begin(), ps.end(), geo::arg_less<T>) の比較関数として使う
// 注意: ゼロベクトルは先頭側に置く、同じ方向なら原点に近い順
template<class T>
bool arg_less(const Point<T>& a, const Point<T>& b) {
    if (a.is_zero() || b.is_zero()) {
        if (a.is_zero() != b.is_zero()) return a.is_zero();
        return false;
    }
    auto half = [](const Point<T>& p) {
        return (p.y > 0 || (p.y == 0 && p.x > 0)) ? 0 : 1;
    };
    int ha = half(a);
    int hb = half(b);
    if (ha != hb) return ha < hb;
    auto cr = a.cross(b);
    int s = internal::sign(cr);
    if (s != 0) return s > 0;
    return a.norm2() < b.norm2();
}

// 偏角順に破壊的ソートする O(N log N)
// 用途: 原点から見た方向順に点やベクトルを並べたいときに使う
// 使い方: sort_by_arg(v) はvを直接並べ替える、基準方向を変えたい場合はarg_sortを使う
template<class T>
void sort_by_arg(std::vector<Point<T>>& ps) {
    std::sort(ps.begin(), ps.end(), arg_less<T>);
}

// 偏角順にソートした列を返す O(N log N)
// 用途: 偏角順の新しい配列が欲しいとき、また開始方向を指定したいときに使う
// 使い方: arg_sort(v, {0,1}) は上方向以降から始まるよう循環シフトした結果を返す
// 注意: baseは順序の開始位置指定であり、点からbaseを引く処理ではない
template<class T>
std::vector<Point<T>> arg_sort(std::vector<Point<T>> ps, Point<T> base = Point<T>(1, 0)) {
    sort_by_arg(ps);
    if (ps.empty()) return ps;
    auto it = std::find_if(ps.begin(), ps.end(), [&](const Point<T>& p) {
        return !arg_less(p, base);
    });
    if (it != ps.end()) std::rotate(ps.begin(), it, ps.end());
    return ps;
}

template<class T = long long>
struct Line {
    Point<T> a{};
    Point<T> b{};

    // 直線を生成する O(1)
    Line() = default;

    // 2点を通る直線を生成する O(1)
    Line(const Point<T>& a_, const Point<T>& b_) : a(a_), b(b_) {}

    // 4座標から直線を生成する O(1)
    Line(T x1, T y1, T x2, T y2) : a(x1, y1), b(x2, y2) {}

    // 方向ベクトルを返す O(1)
    Point<T> vec() const { return b - a; }

    // 代表2点間の距離2乗を返す O(1)
    T length2() const { return a.dist2(b); }

    // 代表2点間の距離を返す O(1)
    template<class F = double>
    F length() const { return a.template dist<F>(b); }
};

template<class T = long long>
struct Segment {
    Point<T> a{};
    Point<T> b{};

    // 線分を生成する O(1)
    Segment() = default;

    // 2端点から線分を生成する O(1)
    Segment(const Point<T>& a_, const Point<T>& b_) : a(a_), b(b_) {}

    // 4座標から線分を生成する O(1)
    Segment(T x1, T y1, T x2, T y2) : a(x1, y1), b(x2, y2) {}

    // 方向ベクトルを返す O(1)
    Point<T> vec() const { return b - a; }

    // 長さ2乗を返す O(1)
    T length2() const { return a.dist2(b); }

    // 長さを返す O(1)
    template<class F = double>
    F length() const { return a.template dist<F>(b); }

    // 線分を含む直線を返す O(1)
    Line<T> line() const { return Line<T>(a, b); }
};

// 直線を出力する O(1)
template<class T>
std::ostream& operator<<(std::ostream& os, const Line<T>& l) {
    return os << l.a << "-" << l.b;
}

// 線分を出力する O(1)
template<class T>
std::ostream& operator<<(std::ostream& os, const Segment<T>& s) {
    return os << s.a << "-" << s.b;
}

// 直線を入力する O(1)
template<class T>
std::istream& operator>>(std::istream& is, Line<T>& l) {
    return is >> l.a >> l.b;
}

// 線分を入力する O(1)
template<class T>
std::istream& operator>>(std::istream& is, Segment<T>& s) {
    return is >> s.a >> s.b;
}

// 2直線が平行か判定する O(1)
template<class T>
bool is_parallel(const Line<T>& x, const Line<T>& y) {
    return internal::sign(x.vec().cross(y.vec())) == 0;
}

// 2線分の方向が平行か判定する O(1)
template<class T>
bool is_parallel(const Segment<T>& x, const Segment<T>& y) {
    return internal::sign(x.vec().cross(y.vec())) == 0;
}

// 2直線が直交するか判定する O(1)
template<class T>
bool is_orthogonal(const Line<T>& x, const Line<T>& y) {
    return internal::sign(x.vec().dot(y.vec())) == 0;
}

// 2線分の方向が直交するか判定する O(1)
template<class T>
bool is_orthogonal(const Segment<T>& x, const Segment<T>& y) {
    return internal::sign(x.vec().dot(y.vec())) == 0;
}

// 点が直線上にあるか判定する O(1)
template<class T>
bool on_line(const Line<T>& l, const Point<T>& p) {
    if (l.a == l.b) return p == l.a;
    return orient(l.a, l.b, p) == 0;
}

// 点が線分上にあるか判定する O(1)
template<class T>
bool on_segment(const Segment<T>& s, const Point<T>& p) {
    if (s.a == s.b) return p == s.a;
    if (orient(s.a, s.b, p) != 0) return false;
    return internal::sign((s.a - p).dot(s.b - p)) <= 0;
}

// 点の有向直線に対する符号付き外積値を返す O(1)
// 用途: 点が有向直線のどちら側にあるかを、符号だけでなく面積値として使いたいときに使う
// 使い方: side_value(l,p)>0ならl.a→l.bの左側、<0なら右側、0なら直線上
template<class T>
T side_value(const Line<T>& l, const Point<T>& p) {
    return (l.b - l.a).cross(p - l.a);
}

// 点が有向直線の左側・右側・上にあるかを返す O(1)
// 用途: 半平面、凸多角形の切断、左右判定を簡潔に書きたいときに使う
// 使い方: side(l,p) は左側1、右側-1、直線上0を返す
template<class T>
int side(const Line<T>& l, const Point<T>& p) {
    return internal::sign(side_value(l, p));
}

// 用途: 正規化した直線方程式をキーや比較対象として扱うための係数型
// 使い方: line_coeff(l) の返り値として受け取り、operator==やoperator<で比較する
template<class T>
struct LineCoeff {
    T a{};
    T b{};
    T c{};

    // 係数が一致するか判定する O(1)
    bool operator==(const LineCoeff& o) const { return a == o.a && b == o.b && c == o.c; }

    // 辞書順で比較する O(1)
    bool operator<(const LineCoeff& o) const {
        if (a != o.a) return a < o.a;
        if (b != o.b) return b < o.b;
        return c < o.c;
    }
};

// 整数直線 ax+by+c=0 の正規化係数を返す O(log C)
// 用途: 同じ直線をmapのキーにしたい、重複直線をまとめたい、直線の同一性を厳密に判定したいときに使う
// 使い方: auto f = line_coeff(l); f.a*x+f.b*y+f.c==0 が直線方程式になる
// 注意: l.a==l.bの退化直線は{0,0,0}を返す、%とgcdを使うため整数型向け
template<class T>
LineCoeff<T> line_coeff(const Line<T>& l) {
    if (l.a == l.b) return LineCoeff<T>{0, 0, 0};

    /* 2点を通る直線方程式を作り、gcdと符号で一意に正規化する */
    T a = l.a.y - l.b.y;
    T b = l.b.x - l.a.x;
    T c = -(a * l.a.x + b * l.a.y);
    T g = internal::gcd_abs(internal::gcd_abs(a, b), c);
    if (g != 0) {
        a /= g;
        b /= g;
        c /= g;
    }
    if (a < 0 || (a == 0 && b < 0) || (a == 0 && b == 0 && c < 0)) {
        a = -a;
        b = -b;
        c = -c;
    }
    return LineCoeff<T>{a, b, c};
}

// 直線係数 ax+by+c=0 から代表直線を生成する O(1)
// 用途: 入力が直線方程式で与えられる問題で、Lineとして交点・距離計算へ渡したいときに使う
// 使い方: line_from_coeff<double>(a,b,c) は実数座標の代表2点を返す、a=b=0ならnullopt
// 注意: 返る2点は代表点であり、整数座標とは限らない
template<class F = double, class T>
std::optional<Line<F>> line_from_coeff(T a, T b, T c) {
    F fa = static_cast<F>(a);
    F fb = static_cast<F>(b);
    F fc = static_cast<F>(c);
    if (fa == F(0) && fb == F(0)) return std::nullopt;
    if (std::abs(fa) >= std::abs(fb)) {
        Point<F> p(-fc / fa, F(0));
        Point<F> q(-(fb + fc) / fa, F(1));
        return Line<F>(p, q);
    }
    Point<F> p(F(0), -fc / fb);
    Point<F> q(F(1), -(fa + fc) / fb);
    return Line<F>(p, q);
}

// 点pを通り直線lに平行な直線を返す O(1)
// 用途: 境界線の平行移動、同じ方向を持つ補助線を整数座標のまま作りたいときに使う
// 使い方: parallel_line(l,p) は p と p+l.vec() を通るLineを返す
// 注意: 距離dだけ離れた平行線ではなく、指定点を通る平行線を作る
template<class T>
Line<T> parallel_line(const Line<T>& l, const Point<T>& p) {
    return Line<T>(p, p + l.vec());
}

// 点pを通り直線lに垂直な直線を返す O(1)
// 用途: 垂線、直交する補助線、最近点や外心計算の補助に使う
// 使い方: perpendicular_line(l,p) は l の方向ベクトルを90度回した直線を返す
// 注意: lが退化している場合はpを始点とする退化直線を返す
template<class T>
Line<T> perpendicular_line(const Line<T>& l, const Point<T>& p) {
    return Line<T>(p, p + l.vec().rot90());
}

// 2点の垂直二等分線を返す O(1)
// 用途: 外心、ボロノイ境界、2点から等距離な点集合を扱うときに使う
// 使い方: perpendicular_bisector<double>(a,b) は中点を通りabに垂直な実数直線を返す
// 注意: 中点が整数とは限らないためLine<F>を返す
template<class F = double, class T>
Line<F> perpendicular_bisector(const Point<T>& a, const Point<T>& b) {
    Point<F> af = a.template to_real<F>();
    Point<F> bf = b.template to_real<F>();
    Point<F> m = (af + bf) / F(2);
    return Line<F>(m, m + (bf - af).rot90());
}

// 点から有向直線への符号付き距離を返す O(1)
// 用途: 半平面境界からの余裕、左側なら正・右側なら負の距離が欲しいときに使う
// 使い方: signed_distance<double>(l,p) の絶対値は distance(l,p) と一致する
// 注意: lが退化している場合は点l.aからの距離を非負で返す
template<class F = double, class T>
F signed_distance(const Line<T>& l, const Point<T>& p) {
    if (l.a == l.b) return p.template dist<F>(l.a);
    return static_cast<F>(side_value(l, p)) / l.template length<F>();
}

// 2直線が同じ無限直線か判定する O(log C)
// 用途: 平行なだけでなく完全に同一直線かを厳密に判定したいときに使う
// 使い方: same_line(l1,l2) はline_coeff(l1)==line_coeff(l2)で判定する
template<class T>
bool same_line(const Line<T>& a, const Line<T>& b) {
    return line_coeff(a) == line_coeff(b);
}

// 2直線が共通点を持つか判定する O(1)
// 用途: 直線同士が交わるか、または同一直線として重なるかを判定したいときに使う
// 使い方: 平行で別直線ならfalse、非平行または同一直線ならtrue
template<class T>
bool intersect(const Line<T>& x, const Line<T>& y) {
    if (x.a == x.b) return on_line(y, x.a);
    if (y.a == y.b) return on_line(x, y.a);
    return !is_parallel(x, y) || on_line(x, y.a);
}

// 直線と線分が共通点を持つか判定する O(1)
// 用途: 線分が直線をまたぐか、端点が直線上にあるかを判定したいときに使う
// 使い方: 端点接触もtrue、線分が1点に退化している場合も扱う
template<class T>
bool intersect(const Line<T>& l, const Segment<T>& s) {
    if (l.a == l.b) return on_segment(s, l.a);
    if (s.a == s.b) return on_line(l, s.a);
    int a = orient(l.a, l.b, s.a);
    int b = orient(l.a, l.b, s.b);
    return a == 0 || b == 0 || a != b;
}

// 線分と直線が共通点を持つか判定する O(1)
template<class T>
bool intersect(const Segment<T>& s, const Line<T>& l) {
    return intersect(l, s);
}

// 2線分が交差するか判定する O(1)
// 用途: 線分交差、自己交差多角形判定、線分集合の愚直検査に使う
// 使い方: 端点接触、重なり区間、点に退化した線分の接触もtrue
template<class T>
bool intersect(const Segment<T>& s, const Segment<T>& t) {
    if (s.a == s.b && t.a == t.b) return s.a == t.a;
    if (s.a == s.b) return on_segment(t, s.a);
    if (t.a == t.b) return on_segment(s, t.a);
    int a = orient(s.a, s.b, t.a);
    int b = orient(s.a, s.b, t.b);
    int c = orient(t.a, t.b, s.a);
    int d = orient(t.a, t.b, s.b);
    if (a == 0 && b == 0) {
        bool okx = std::max(std::min(s.a.x, s.b.x), std::min(t.a.x, t.b.x)) <= std::min(std::max(s.a.x, s.b.x), std::max(t.a.x, t.b.x));
        bool oky = std::max(std::min(s.a.y, s.b.y), std::min(t.a.y, t.b.y)) <= std::min(std::max(s.a.y, s.b.y), std::max(t.a.y, t.b.y));
        return okx && oky;
    }
    return a * b <= 0 && c * d <= 0;
}

// 2線分が端点以外の内部で交差するか判定する O(1)
// 用途: 端点接触を無視して、線分の内部同士が本当に交差するかだけを見たいときに使う
// 使い方: proper_intersect(s,t) は共有端点や重なり、退化線分ではfalse
template<class T>
bool proper_intersect(const Segment<T>& s, const Segment<T>& t) {
    if (s.a == s.b || t.a == t.b) return false;
    return orient(s.a, s.b, t.a) * orient(s.a, s.b, t.b) < 0 &&
           orient(t.a, t.b, s.a) * orient(t.a, t.b, s.b) < 0;
}

// 同一直線上の2線分の共通部分を返す O(1)
// 用途: 交差判定がtrueのうち、線分が重なっている区間や接点を取り出したいときに使う
// 使い方: 戻り値がnulloptなら共通部分なし、a==bなら1点接触、a!=bなら重なり区間
// 注意: 同一直線上でない場合はnullopt
template<class T>
std::optional<Segment<T>> overlap_segment(Segment<T> s, Segment<T> t) {
    if (s.a == s.b && t.a == t.b) return s.a == t.a ? std::optional<Segment<T>>(s) : std::nullopt;
    if (s.a == s.b) return on_segment(t, s.a) ? std::optional<Segment<T>>(s) : std::nullopt;
    if (t.a == t.b) return on_segment(s, t.a) ? std::optional<Segment<T>>(t) : std::nullopt;
    if (!on_line(Line<T>(s.a, s.b), t.a) || !on_line(Line<T>(s.a, s.b), t.b)) return std::nullopt;
    auto less_along = [&](const Point<T>& p, const Point<T>& q) {
        if (s.a.x != s.b.x) return p.x < q.x || (p.x == q.x && p.y < q.y);
        return p.y < q.y || (p.y == q.y && p.x < q.x);
    };
    if (less_along(s.b, s.a)) std::swap(s.a, s.b);
    if (less_along(t.b, t.a)) std::swap(t.a, t.b);
    Point<T> l = less_along(s.a, t.a) ? t.a : s.a;
    Point<T> r = less_along(s.b, t.b) ? s.b : t.b;
    if (less_along(r, l)) return std::nullopt;
    return Segment<T>(l, r);
}

// 直線上の指定x座標に対応するy座標を返す O(1)
// 用途: 直線の高さ比較やスイープライン中の補助計算でy座標が欲しいときに使う
// 使い方: y_at<double>(l, x) は垂直線ならnullopt、そうでなければ直線上のyを返す
template<class F = double, class T, class X>
std::optional<F> y_at(const Line<T>& l, X x) {
    if (l.a.x == l.b.x) return std::nullopt;
    F dx = static_cast<F>(l.b.x) - static_cast<F>(l.a.x);
    F dy = static_cast<F>(l.b.y) - static_cast<F>(l.a.y);
    return static_cast<F>(l.a.y) + (static_cast<F>(x) - static_cast<F>(l.a.x)) * dy / dx;
}

// 線分を含む直線上の指定x座標に対応するy座標を返す O(1)
// 用途: 線分を含む無限直線のyを計算したいときに使う
// 使い方: xが線分の範囲外でも、延長直線上のyを返す、範囲確認は必要なら別途行う
template<class F = double, class T, class X>
std::optional<F> y_at(const Segment<T>& s, X x) {
    return y_at<F>(Line<T>(s.a, s.b), x);
}

// 2直線の交点を返す O(1)
// 用途: 交差する2直線の交点座標を実数で取り出したいときに使う
// 使い方: auto p = cross_point<double>(l1,l2); 平行または同一直線ならnullopt
// 注意: 交点は一般に整数でないため浮動小数点型Fで返す
template<class F = double, class T>
std::optional<Point<F>> cross_point(const Line<T>& x, const Line<T>& y) {
    Point<T> vx = x.vec();
    Point<T> vy = y.vec();
    T den = vx.cross(vy);
    if (internal::sign(den) == 0) return std::nullopt;
    T num = (y.a - x.a).cross(vy);
    F t = static_cast<F>(num) / static_cast<F>(den);
    return x.a.template to_real<F>() + vx.template to_real<F>() * t;
}

// 直線と線分の一意な交点を返す O(1)
// 用途: 直線と線分が1点で交わる場合に、その交点を取り出したいときに使う
// 使い方: 非交差ならnullopt、線分全体が直線上にある場合は一意でないためnullopt
template<class F = double, class T>
std::optional<Point<F>> cross_point(const Line<T>& l, const Segment<T>& s) {
    if (!intersect(l, s)) return std::nullopt;
    if (s.a == s.b) return s.a.template to_real<F>();
    return cross_point<F>(l, Line<T>(s.a, s.b));
}

// 線分と直線の一意な交点を返す O(1)
template<class F = double, class T>
std::optional<Point<F>> cross_point(const Segment<T>& s, const Line<T>& l) {
    return cross_point<F>(l, s);
}

// 2線分の一意な交点を返す O(1)
// 用途: 線分交差後に、交点が1点に定まる場合だけ座標を得たいときに使う
// 使い方: 非交差や重なり区間がある場合はnullopt、端点1点接触ならその点を返す
template<class F = double, class T>
std::optional<Point<F>> cross_point(const Segment<T>& s, const Segment<T>& t) {
    if (!intersect(s, t)) return std::nullopt;
    auto den0 = s.vec().cross(t.vec());
    if (internal::sign(den0) == 0) {
        auto ov = overlap_segment(s, t);
        if (!ov || ov->a != ov->b) return std::nullopt;
        return ov->a.template to_real<F>();
    }
    return cross_point<F>(Line<T>(s.a, s.b), Line<T>(t.a, t.b));
}

// 直線への射影点を返す O(1)
// 用途: 点から直線へ下ろした垂線の足、最近点、反射点の計算に使う
// 使い方: project<double>(l,p) はl上の実数座標点を返す、退化直線なら代表点l.aを返す
template<class F = double, class T>
Point<F> project(const Line<T>& l, const Point<T>& p) {
    Point<F> a = l.a.template to_real<F>();
    Point<F> v(static_cast<F>(l.b.x) - static_cast<F>(l.a.x), static_cast<F>(l.b.y) - static_cast<F>(l.a.y));
    F den = v.norm2();
    if (den == F(0)) return a;
    F r = (p.template to_real<F>() - a).dot(v) / den;
    return a + v * r;
}

// 線分を含む直線への射影点を返す O(1)
template<class F = double, class T>
Point<F> project(const Segment<T>& s, const Point<T>& p) {
    return project<F>(Line<T>(s.a, s.b), p);
}

// 点に最も近い直線上の点を返す O(1)
// 用途: 距離だけでなく接地点、射影点、スナップ先の座標が必要なときに使う
// 使い方: closest_point<double>(l,p) は project<double>(l,p) と同じ結果を返す
// 注意: 退化直線なら代表点l.aを返す
template<class F = double, class T>
Point<F> closest_point(const Line<T>& l, const Point<T>& p) {
    return project<F>(l, p);
}

// 点に最も近い線分上の点を返す O(1)
// 用途: 点から線分への最近点、線分経路への最近位置、衝突候補点の取得に使う
// 使い方: 射影が線分外なら近い端点、退化線分ならその端点を返す
// 注意: 最近点が整数とは限らないためPoint<F>を返す
template<class F = double, class T>
Point<F> closest_point(const Segment<T>& s, const Point<T>& p) {
    if (s.a == s.b) return s.a.template to_real<F>();
    auto v = s.vec();
    auto t_num = v.dot(p - s.a);
    if (internal::sign(t_num) <= 0) return s.a.template to_real<F>();
    if (t_num >= v.norm2()) return s.b.template to_real<F>();
    return project<F>(s, p);
}

// 直線を軸にした反射点を返す O(1)
// 用途: 鏡映変換、対称点の生成、幾何的な折り返し計算に使う
// 使い方: reflect<double>(l,p) は直線lでpを反射した実数座標を返す
template<class F = double, class T>
Point<F> reflect(const Line<T>& l, const Point<T>& p) {
    Point<F> q = project<F>(l, p);
    Point<F> pf = p.template to_real<F>();
    return q * F(2) - pf;
}

// 線分を含む直線を軸にした反射点を返す O(1)
template<class F = double, class T>
Point<F> reflect(const Segment<T>& s, const Point<T>& p) {
    return reflect<F>(Line<T>(s.a, s.b), p);
}

// 点と直線の距離を返す O(1)
template<class F = double, class T>
F distance(const Line<T>& l, const Point<T>& p) {
    if (l.a == l.b) return p.template dist<F>(l.a);
    T cr = l.vec().cross(p - l.a);
    return static_cast<F>(internal::abs_value(cr)) / l.template length<F>();
}

// 直線と点の距離を返す O(1)
template<class F = double, class T>
F distance(const Point<T>& p, const Line<T>& l) {
    return distance<F>(l, p);
}

// 点と線分の距離を返す O(1)
// 用途: 点が線分にどれだけ近いか、円と線分の近接判定などで使う
// 使い方: 垂線の足が線分外なら近い端点までの距離、線分内なら直線距離を返す
template<class F = double, class T>
F distance(const Segment<T>& s, const Point<T>& p) {
    if (s.a == s.b) return p.template dist<F>(s.a);
    if (internal::sign((s.b - s.a).dot(p - s.a)) < 0) return p.template dist<F>(s.a);
    if (internal::sign((s.a - s.b).dot(p - s.b)) < 0) return p.template dist<F>(s.b);
    return distance<F>(Line<T>(s.a, s.b), p);
}

// 線分と点の距離を返す O(1)
template<class F = double, class T>
F distance(const Point<T>& p, const Segment<T>& s) {
    return distance<F>(s, p);
}

// 2線分の距離を返す O(1)
// 用途: 線分同士の最短距離を求めたいときに使う
// 使い方: 交差または接触していれば0、そうでなければ各端点から相手線分への距離の最小値
template<class F = double, class T>
F distance(const Segment<T>& s, const Segment<T>& t) {
    if (intersect(s, t)) return F(0);
    return std::min({distance<F>(s, t.a), distance<F>(s, t.b), distance<F>(t, s.a), distance<F>(t, s.b)});
}

// 2直線の距離を返す O(1)
template<class F = double, class T>
F distance(const Line<T>& x, const Line<T>& y) {
    if (!is_parallel(x, y)) return F(0);
    return distance<F>(x, y.a);
}

// 直線と線分の距離を返す O(1)
// 用途: 線分が直線に届くか、直線からの最短距離を求めたいときに使う
// 使い方: 交差すれば0、交差しなければ線分端点から直線への距離の小さい方
template<class F = double, class T>
F distance(const Line<T>& l, const Segment<T>& s) {
    if (intersect(l, s)) return F(0);
    return std::min(distance<F>(l, s.a), distance<F>(l, s.b));
}

// 線分と直線の距離を返す O(1)
template<class F = double, class T>
F distance(const Segment<T>& s, const Line<T>& l) {
    return distance<F>(l, s);
}

template<class T = long long>
struct Circle {
    Point<T> c{};
    T r{};

    // 円を生成する O(1)
    Circle() = default;

    // 中心と半径から円を生成する O(1)
    Circle(const Point<T>& c_, T r_) : c(c_), r(r_) {}

    // 座標と半径から円を生成する O(1)
    Circle(T x, T y, T r_) : c(x, y), r(r_) {}
};

// 円を出力する O(1)
template<class T>
std::ostream& operator<<(std::ostream& os, const Circle<T>& c) {
    return os << c.c << "(r=" << c.r << ')';
}

// 円を入力する O(1)
template<class T>
std::istream& operator>>(std::istream& is, Circle<T>& c) {
    return is >> c.c >> c.r;
}

// 半径2乗を返す O(1)
template<class T>
T radius2(const Circle<T>& c) {
    return c.r * c.r;
}

// 円と点の位置関係を返す O(1)
// 用途: 点が円の内側・円周上・外側のどこにあるかを整数距離2乗で判定したいときに使う
// 使い方: contains(c,p) はINSIDE/ON_EDGE/OUTSIDEを返す
template<class T>
Contains contains(const Circle<T>& c, const Point<T>& p) {
    auto d = c.c.dist2(p);
    auto rr = radius2(c);
    if (d < rr) return INSIDE;
    if (d == rr) return ON_EDGE;
    return OUTSIDE;
}

// 2円の位置関係を返す O(1)
// 用途: 2円の交点数、接するか、包含関係にあるかを交点計算前に判定したいときに使う
// 使い方: circle_relation(a,b) は外離れ、外接、2交点、内接、包含、同一円を分類する
// 注意: 判定は距離2乗と半径和差2乗で行うため整数型Tの積が溢れない前提
template<class T>
CircleRelation circle_relation(const Circle<T>& a, const Circle<T>& b) {
    T d2 = a.c.dist2(b.c);
    T rs = a.r + b.r;
    T rd = internal::abs_value(a.r - b.r);
    T rs2 = rs * rs;
    T rd2 = rd * rd;
    if (d2 == 0 && a.r == b.r) return CIRCLE_SAME;
    if (d2 > rs2) return CIRCLE_SEPARATE;
    if (d2 == rs2) return CIRCLE_EXTERNAL_TANGENT;
    if (d2 < rd2) return CIRCLE_CONTAIN;
    if (d2 == rd2) return CIRCLE_INTERNAL_TANGENT;
    return CIRCLE_INTERSECT;
}

// 円の面積を返す O(1)
// 用途: 円面積、共通面積との比較、確率・積分系の出力に使う
// 使い方: area<double>(circle) は PI*r*r を返す
// 注意: 浮動小数点値を返す、半径は非負を想定
template<class F = double, class T>
F area(const Circle<T>& c) {
    F r = static_cast<F>(c.r);
    return PI<F> * r * r;
}

// 円周長を返す O(1)
// 用途: 円周上の移動距離、円弧長の基準、幾何量の出力に使う
// 使い方: circumference<double>(circle) は 2*PI*r を返す
// 注意: 浮動小数点値を返す、半径は非負を想定
template<class F = double, class T>
F circumference(const Circle<T>& c) {
    return F(2) * PI<F> * static_cast<F>(c.r);
}

// 点pの円cに対する方べき値を返す O(1)
// 用途: 点が円の内外どちらにあるか、根軸、接線長2乗の計算に使う
// 使い方: power(c,p)>0なら外部、0なら円周上、<0なら内部
// 注意: 整数のまま d^2-r^2 を返すため、積が溢れない型を利用側で選ぶ
template<class T>
T power(const Circle<T>& c, const Point<T>& p) {
    return c.c.dist2(p) - radius2(c);
}

// 2円の根軸を返す O(1)
// 用途: 2円への方べきが等しい点集合、円交点が乗る直線、円束の補助に使う
// 使い方: auto l = radical_axis<double>(c1,c2); 同心円など一意な直線でない場合はnullopt
// 注意: 根軸の点は一般に整数でないためLine<F>を返す
template<class F = double, class T>
std::optional<Line<F>> radical_axis(const Circle<T>& a, const Circle<T>& b) {
    T aa = T(2) * (b.c.x - a.c.x);
    T bb = T(2) * (b.c.y - a.c.y);
    T cc = a.c.norm2() - b.c.norm2() - a.r * a.r + b.r * b.r;
    return line_from_coeff<F>(aa, bb, cc);
}

// 2円の共通部分の面積を返す O(1)
// 用途: 円同士の重なり面積、確率・積分系の幾何問題に使う
// 使い方: 離れていれば0、片方が包含されれば小さい円の面積を返す
// 注意: 浮動小数点計算であり、接触や包含の境界は固定epsで丸める
template<class F = double, class T>
F intersection_area(const Circle<T>& a, const Circle<T>& b) {
    F d = a.c.template dist<F>(b.c);
    F r1 = static_cast<F>(a.r);
    F r2 = static_cast<F>(b.r);
    F eps = F(1e-12);
    if (d <= eps) {
        if (std::abs(r1 - r2) <= eps) return PI<F> * r1 * r1;
        F r = std::min(r1, r2);
        return PI<F> * r * r;
    }
    if (d >= r1 + r2 - eps) return F(0);
    if (d <= std::abs(r1 - r2) + eps) {
        F r = std::min(r1, r2);
        return PI<F> * r * r;
    }
    auto clamp = [](F x) { return std::max(F(-1), std::min(F(1), x)); };
    F a1 = std::acos(clamp((d * d + r1 * r1 - r2 * r2) / (F(2) * d * r1)));
    F a2 = std::acos(clamp((d * d + r2 * r2 - r1 * r1) / (F(2) * d * r2)));
    F s1 = r1 * r1 * a1;
    F s2 = r2 * r2 * a2;
    F s3 = F(0.5) * std::sqrt(std::max<F>(F(0), (-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2)));
    return s1 + s2 - s3;
}

// 円周と直線が交差するか判定する O(1)
// 用途: 無限直線が円周に接する、または横切るかを整数比較で判定したいときに使う
// 使い方: 円盤との距離判定ではなく、円周と直線の共通点があるかを返す
template<class T>
bool intersect(const Circle<T>& c, const Line<T>& l) {
    if (l.a == l.b) return contains(c, l.a) == ON_EDGE;
    auto cr = l.vec().cross(c.c - l.a);
    auto rr = radius2(c);
    auto len2 = l.length2();
    auto lhs = cr * cr;
    auto rhs = rr * len2;
    return lhs <= rhs;
}

// 円盤と線分が交差するか判定する O(1)
// 用途: 線分が円の内部に入るか、円盤と共有点を持つかを判定したいときに使う
// 使い方: 線分全体が円内にある場合もtrue、円周との交差だけならintersect(c,s)を使う
template<class T>
bool intersect_disk(const Circle<T>& c, const Segment<T>& s) {
    if (s.a == s.b) return contains(c, s.a) != OUTSIDE;
    auto v = s.vec();
    auto rr = radius2(c);
    if (internal::sign(v.dot(c.c - s.a)) < 0) return s.a.dist2(c.c) <= rr;
    if (internal::sign((-v).dot(c.c - s.b)) < 0) return s.b.dist2(c.c) <= rr;
    auto cr = v.cross(c.c - s.a);
    auto len2 = s.length2();
    auto lhs = cr * cr;
    auto rhs = rr * len2;
    return lhs <= rhs;
}

// 円周と線分が交差するか判定する O(1)
// 用途: 線分が円周に触れる、または円を横切るかを判定したいときに使う
// 使い方: 線分が完全に円内部にあるだけならfalse、円盤との共有点ならintersect_diskを使う
template<class T>
bool intersect(const Circle<T>& c, const Segment<T>& s) {
    if (s.a == s.b) return contains(c, s.a) == ON_EDGE;
    auto rr = radius2(c);
    auto d1 = s.a.dist2(c.c);
    auto d2 = s.b.dist2(c.c);
    auto mx = std::max(d1, d2);
    return intersect_disk(c, s) && rr <= mx;
}

// 線分と円周が交差するか判定する O(1)
template<class T>
bool intersect(const Segment<T>& s, const Circle<T>& c) {
    return intersect(c, s);
}

// 2円周が交差するか判定する O(1)
// 用途: 2つの円周が共有点を持つかを、交点座標を出さずに判定したいときに使う
// 使い方: 2交点、外接、内接、同一円でtrue、包含で交わらない場合や離れている場合はfalse
template<class T>
bool intersect(const Circle<T>& a, const Circle<T>& b) {
    auto r = circle_relation(a, b);
    return r == CIRCLE_INTERSECT || r == CIRCLE_EXTERNAL_TANGENT || r == CIRCLE_INTERNAL_TANGENT || r == CIRCLE_SAME;
}

// 円と直線の交点を返す O(1)
// 用途: 円周と無限直線の交点座標が必要なときに使う
// 使い方: 戻り値は0個、接線なら1個、通常は2個、座標はPoint<F>
// 注意: 交点は一般に整数でないため浮動小数点型Fで返す
template<class F = double, class T>
std::vector<Point<F>> cross_points(const Circle<T>& c, const Line<T>& l) {
    std::vector<Point<F>> ans;
    if (!intersect(c, l)) return ans;
    if (l.a == l.b) {
        ans.push_back(l.a.template to_real<F>());
        return ans;
    }
    Point<F> pr = project<F>(l, c.c);
    Point<F> e = l.vec().template unit<F>();
    F d = distance<F>(l, c.c);
    F h2 = static_cast<F>(c.r) * static_cast<F>(c.r) - d * d;
    if (h2 < F(0)) h2 = F(0);
    F h = std::sqrt(h2);
    ans.push_back(pr + e * h);
    if (h > F(1e-10)) ans.push_back(pr - e * h);
    return ans;
}

// 円と線分の交点を返す O(1)
// 用途: 線分上にある円周との交点だけを取り出したいときに使う
// 使い方: 戻り値は0個、接点なら1個、横切れば2個、端点接触も含む
// 注意: 線分範囲の絞り込みにはFのepsを使うため、判定だけならintersect(c,s)を優先する
template<class F = double, class T>
std::vector<Point<F>> cross_points(const Circle<T>& c, const Segment<T>& s) {
    std::vector<Point<F>> ans;
    if (!intersect(c, s)) return ans;
    if (s.a == s.b) {
        ans.push_back(s.a.template to_real<F>());
        return ans;
    }
    auto ps = cross_points<F>(c, Line<T>(s.a, s.b));
    for (const auto& p : ps) {
        F minx = std::min(static_cast<F>(s.a.x), static_cast<F>(s.b.x)) - F(1e-9);
        F maxx = std::max(static_cast<F>(s.a.x), static_cast<F>(s.b.x)) + F(1e-9);
        F miny = std::min(static_cast<F>(s.a.y), static_cast<F>(s.b.y)) - F(1e-9);
        F maxy = std::max(static_cast<F>(s.a.y), static_cast<F>(s.b.y)) + F(1e-9);
        if (minx <= p.x && p.x <= maxx && miny <= p.y && p.y <= maxy) {
            bool dup = false;
            for (const auto& q : ans) {
                if (std::abs(p.x - q.x) <= F(1e-9) && std::abs(p.y - q.y) <= F(1e-9)) dup = true;
            }
            if (!dup) ans.push_back(p);
        }
    }
    return ans;
}

// 2円の交点を返す O(1)
// 用途: 2円の交点座標や接点座標が必要なときに使う
// 使い方: 戻り値は0個、接するなら1個、通常交差なら2個、同一円は無限個なので空を返す
// 注意: 交点は一般に整数でないため浮動小数点型Fで返す
template<class F = double, class T>
std::vector<Point<F>> cross_points(const Circle<T>& a, const Circle<T>& b) {
    std::vector<Point<F>> ans;
    CircleRelation rel = circle_relation(a, b);
    if (rel == CIRCLE_SEPARATE || rel == CIRCLE_CONTAIN || rel == CIRCLE_SAME) return ans;
    F d = a.c.template dist<F>(b.c);
    if (d == F(0)) return ans;
    F r1 = static_cast<F>(a.r);
    F r2 = static_cast<F>(b.r);
    F x = (r1 * r1 - r2 * r2 + d * d) / (F(2) * d);
    F h2 = r1 * r1 - x * x;
    if (h2 < F(0)) h2 = F(0);
    F h = std::sqrt(h2);
    Point<F> v = (b.c - a.c).template to_real<F>() / d;
    Point<F> base = a.c.template to_real<F>() + v * x;
    Point<F> per = v.rot90() * h;
    ans.push_back(base + per);
    if (h > F(1e-10)) ans.push_back(base - per);
    return ans;
}

// 円外または円上の点から円への接点を返す O(1)
// 用途: 点から円へ引ける接線の接点、可視範囲、接線長の計算に使う
// 使い方: 点が円内なら空、円上なら1個、円外なら2個の接点を返す
template<class F = double, class T>
std::vector<Point<F>> tangent_points(const Circle<T>& c, const Point<T>& p) {
    std::vector<Point<F>> ans;
    auto d2 = c.c.dist2(p);
    auto rr = radius2(c);
    if (d2 < rr) return ans;
    if (d2 == rr) {
        ans.push_back(p.template to_real<F>());
        return ans;
    }
    Point<F> v = (p - c.c).template to_real<F>();
    F dd = static_cast<F>(d2);
    F r = static_cast<F>(c.r);
    F a = r * r / dd;
    F b = r * std::sqrt(dd - r * r) / dd;
    Point<F> center = c.c.template to_real<F>();
    Point<F> q = center + v * a;
    Point<F> w = v.rot90() * b;
    ans.push_back(q + w);
    ans.push_back(q - w);
    return ans;
}

// 円外または円上の点から円への接線を返す O(1)
// 用途: 点を通る円の接線そのものが必要なときに使う
// 使い方: 点が円内なら空、円上なら1本、円外なら2本のLine<F>を返す
template<class F = double, class T>
std::vector<Line<F>> tangent_lines(const Circle<T>& c, const Point<T>& p) {
    std::vector<Line<F>> ans;
    Point<F> pf = p.template to_real<F>();
    Point<F> cf = c.c.template to_real<F>();
    auto ts = tangent_points<F>(c, p);

    /* 円上の点では、接点と外部点が一致するため半径に垂直な方向で直線を作る */
    for (const auto& q : ts) {
        if ((q - pf).norm2() <= F(1e-20)) {
            Point<F> dir = (q - cf).rot90();
            ans.emplace_back(q, q + dir);
        } else {
            ans.emplace_back(pf, q);
        }
    }
    return ans;
}

// 2円に共通する接線を返す O(1)
// 用途: 2円の外接線・内接線、円障害物の接線グラフ、幾何最短路の候補生成に使う
// 使い方: common_tangent_lines<double>(a,b) は存在する共通接線をLine<double>で列挙する
// 注意: 同心円では空、接する場合は重複しないよう本数が減る
template<class F = double, class T>
std::vector<Line<F>> common_tangent_lines(const Circle<T>& a, const Circle<T>& b) {
    constexpr F eps = static_cast<F>(1e-12);
    std::vector<Line<F>> ans;
    Point<F> ca = a.c.template to_real<F>();
    Point<F> cb = b.c.template to_real<F>();
    Point<F> v = cb - ca;
    F z = v.norm2();
    if (z <= eps) return ans;
    F r1 = static_cast<F>(a.r);
    F r2 = static_cast<F>(b.r);

    /* 1つ目の円で法線の正側に接すると固定し、2つ目の円の接する側を変えて外接線・内接線を列挙する */
    for (int side : {1, -1}) {
        F r = r1 - static_cast<F>(side) * r2;
        F h2 = z - r * r;
        if (h2 < -eps) continue;
        h2 = std::max<F>(F(0), h2);
        F h = std::sqrt(h2);
        for (int sgn : {-1, 1}) {
            Point<F> normal = (v * r + v.rot90() * (h * static_cast<F>(sgn))) / z;
            Point<F> p1 = ca + normal * r1;
            Point<F> p2 = cb + normal * (static_cast<F>(side) * r2);
            if ((p1 - p2).norm2() <= eps * eps) p2 = p1 + normal.rot90();
            ans.emplace_back(p1, p2);
            if (h <= eps) break;
        }
    }
    return ans;
}

// 2円に共通する接線を返す O(1)
// 用途: common_tangent_linesの短い別名として使う
// 使い方: tangent_lines<double>(c1,c2) は2円の共通接線を列挙する
template<class F = double, class T>
std::vector<Line<F>> tangent_lines(const Circle<T>& a, const Circle<T>& b) {
    return common_tangent_lines<F>(a, b);
}

// 三角形の外心を返す O(1)
// 用途: 3点を通る円の中心、三角形の外接円、幾何構成で使う
// 使い方: circumcenter<double>(a,b,c) は3点が共線ならnulloptを返す
template<class F = double, class T>
std::optional<Point<F>> circumcenter(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    F ax = static_cast<F>(a.x), ay = static_cast<F>(a.y);
    F bx = static_cast<F>(b.x), by = static_cast<F>(b.y);
    F cx = static_cast<F>(c.x), cy = static_cast<F>(c.y);
    F d = F(2) * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(d) <= F(1e-18)) return std::nullopt;
    F aa = ax * ax + ay * ay;
    F bb = bx * bx + by * by;
    F cc = cx * cx + cy * cy;
    F ux = (aa * (by - cy) + bb * (cy - ay) + cc * (ay - by)) / d;
    F uy = (aa * (cx - bx) + bb * (ax - cx) + cc * (bx - ax)) / d;
    return Point<F>(ux, uy);
}

// 三角形の外接円を返す O(1)
// 用途: 3点を通る円を直接得たいときに使う
// 使い方: circumcircle<double>(a,b,c) は共線ならnullopt、そうでなければCircle<double>
template<class F = double, class T>
std::optional<Circle<F>> circumcircle(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto o = circumcenter<F>(a, b, c);
    if (!o) return std::nullopt;
    return Circle<F>(*o, o->template dist<F>(a.template to_real<F>()));
}

// 三角形の内心を返す O(1)
// 用途: 角の二等分線の交点、内接円の中心を求めたいときに使う
// 使い方: incenter<double>(a,b,c) は三角形が退化していればnulloptを返す
template<class F = double, class T>
std::optional<Point<F>> incenter(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    if (internal::sign((b - a).cross(c - a)) == 0) return std::nullopt;
    F wa = b.template dist<F>(c);
    F wb = c.template dist<F>(a);
    F wc = a.template dist<F>(b);
    F s = wa + wb + wc;
    if (s == F(0)) return std::nullopt;
    return (a.template to_real<F>() * wa + b.template to_real<F>() * wb + c.template to_real<F>() * wc) / s;
}

// 三角形の内接円を返す O(1)
// 用途: 三角形に内接する円の中心と半径をまとめて得たいときに使う
// 使い方: incircle<double>(a,b,c) は退化三角形ならnulloptを返す
template<class F = double, class T>
std::optional<Circle<F>> incircle(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto o = incenter<F>(a, b, c);
    if (!o) return std::nullopt;
    F per = a.template dist<F>(b) + b.template dist<F>(c) + c.template dist<F>(a);
    if (per == F(0)) return std::nullopt;
    F a2 = std::abs(static_cast<F>((b - a).cross(c - a)));
    F r = a2 / per;
    return Circle<F>(*o, r);
}

// 三角形の頂点平均を返す O(1)
// 用途: 三角形の重心、メディアンの交点、簡単な代表点が必要なときに使う
// 使い方: triangle_centroid<double>(a,b,c) は (a+b+c)/3 を実数座標で返す
// 注意: 面積が0でも頂点平均として返す
template<class F = double, class T>
Point<F> triangle_centroid(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return (a.template to_real<F>() + b.template to_real<F>() + c.template to_real<F>()) / F(3);
}

// 三角形の垂心を返す O(1)
// 用途: 高さの交点、オイラー線、三角形中心を扱う問題で使う
// 使い方: orthocenter<double>(a,b,c) は3点が共線ならnulloptを返す
// 注意: 外心を利用するため浮動小数点誤差を含む
template<class F = double, class T>
std::optional<Point<F>> orthocenter(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto o = circumcenter<F>(a, b, c);
    if (!o) return std::nullopt;
    return a.template to_real<F>() + b.template to_real<F>() + c.template to_real<F>() - *o * F(2);
}

template<class T = long long>
using Polygon = std::vector<Point<T>>;

// 三角形の符号付き面積2倍を返す O(1)
template<class T>
T signed_area2(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return (b - a).cross(c - a);
}

// 三角形の面積2倍を返す O(1)
template<class T>
T area2(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto s = signed_area2(a, b, c);
    return s < 0 ? -s : s;
}

// 三角形の面積を返す O(1)
template<class F = double, class T>
F area(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return static_cast<F>(area2(a, b, c)) / F(2);
}

// 多角形の符号付き面積2倍を返す O(N)
// 用途: 多角形の向き判定、面積比較、Pickの定理などで整数のまま面積を扱うときに使う
// 使い方: 反時計回りなら正、時計回りなら負、絶対値が面積の2倍
template<class T>
T signed_area2(const Polygon<T>& p) {
    T s = 0;
    int n = static_cast<int>(p.size());
    for (int i = 0; i < n; ++i) s += p[i].cross(p[(i + 1) % n]);
    return s;
}

// 多角形の面積2倍を返す O(N)
template<class T>
T area2(const Polygon<T>& p) {
    auto s = signed_area2(p);
    return s < 0 ? -s : s;
}

// 多角形の面積を返す O(N)
template<class F = double, class T>
F area(const Polygon<T>& p) {
    return static_cast<F>(area2(p)) / F(2);
}

// 多角形の周長を返す O(N)
template<class F = double, class T>
F perimeter(const Polygon<T>& p) {
    F ans = 0;
    int n = static_cast<int>(p.size());
    for (int i = 0; i < n; ++i) ans += p[i].template dist<F>(p[(i + 1) % n]);
    return ans;
}

// 一般多角形に対する点の内包関係を返す O(N)
// 用途: 凸とは限らない多角形に点が含まれるかを判定したいときに使う
// 使い方: contains(poly,p) はINSIDE/ON_EDGE/OUTSIDEを返す、境界判定を先に行う
// 注意: 自己交差多角形では通常の単純多角形としての意味が崩れるため、必要ならis_simple_polygonで確認する
template<class T>
Contains contains(const Polygon<T>& g, const Point<T>& p) {
    bool in = false;
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) {
        Point<T> a = g[i] - p;
        Point<T> b = g[(i + 1) % n] - p;
        if (internal::sign(a.cross(b)) == 0 && internal::sign(a.dot(b)) <= 0) return ON_EDGE;
        if (a.y > b.y) std::swap(a, b);
        if (a.y <= 0 && 0 < b.y && internal::sign(a.cross(b)) > 0) in = !in;
    }
    return in ? INSIDE : OUTSIDE;
}

// 多角形が凸か判定する O(N)
// 用途: 凸多角形専用APIを使ってよいか、入力多角形が凸かを確認したいときに使う
// 使い方: strict=falseなら同一直線上の連続頂点を許し、strict=trueなら許さない
template<class T>
bool is_convex(const Polygon<T>& p, bool strict = false) {
    int n = static_cast<int>(p.size());
    if (n < 3) return !strict || n <= 2;
    int dir = 0;
    for (int i = 0; i < n; ++i) {
        int s = orient(p[i], p[(i + 1) % n], p[(i + 2) % n]);
        if (s == 0) {
            if (strict) return false;
            continue;
        }
        if (dir == 0) dir = s;
        else if (dir != s) return false;
    }
    return true;
}

// 点集合の凸包を返す O(N log N)
// 用途: 点集合の外周、直径、凸包内包判定、回転キャリパーの前処理に使う
// 使い方: convex_hull(ps,false) は辺上の点を除く、trueなら辺上の点も残す
// 注意: 重複点は削除し、返り値は反時計回り、全点共線の場合も落ちない
template<class T>
Polygon<T> convex_hull(std::vector<Point<T>> ps, bool keep_collinear = false) {
    std::sort(ps.begin(), ps.end());
    ps.erase(std::unique(ps.begin(), ps.end()), ps.end());
    int n = static_cast<int>(ps.size());
    if (n <= 1) return ps;

    /* 下側凸包と上側凸包を別々に作り、最後に端点を除いて連結する */
    Polygon<T> lower, upper;
    for (const auto& p : ps) {
        while (lower.size() >= 2) {
            int s = orient(lower[lower.size() - 2], lower.back(), p);
            if (keep_collinear ? (s < 0) : (s <= 0)) lower.pop_back();
            else break;
        }
        lower.push_back(p);
    }
    for (int i = n - 1; i >= 0; --i) {
        const auto& p = ps[i];
        while (upper.size() >= 2) {
            int s = orient(upper[upper.size() - 2], upper.back(), p);
            if (keep_collinear ? (s < 0) : (s <= 0)) upper.pop_back();
            else break;
        }
        upper.push_back(p);
    }

    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    if (keep_collinear && area2(lower) == 0) return ps;
    return lower;
}

// 連続する共線頂点を取り除いた多角形を返す O(N)
// 用途: 凸包や多角形入力に含まれる不要な一直線上の頂点を削って扱いやすくしたいときに使う
// 使い方: compress_collinear(poly,false) は面積0の退化多角形なら空にする
// 注意: 非連続の共線点は削らず、辺の途中にある連続頂点だけを削る
template<class T>
Polygon<T> compress_collinear(Polygon<T> p, bool keep_degenerate = true) {
    if (p.empty()) return p;

    /* 連続重複点と、閉路入力で末尾に重複した始点を取り除く */
    Polygon<T> q;
    q.reserve(p.size());
    for (const auto& v : p) {
        if (q.empty() || q.back() != v) q.push_back(v);
    }
    if (q.size() >= 2 && q.front() == q.back()) q.pop_back();
    if (q.size() <= 2) return keep_degenerate ? q : Polygon<T>();

    if (area2(q) == 0) {
        if (!keep_degenerate) return {};
        auto mm = std::minmax_element(q.begin(), q.end());
        if (*mm.first == *mm.second) return Polygon<T>{*mm.first};
        return Polygon<T>{*mm.first, *mm.second};
    }

    Polygon<T> res;
    int n = static_cast<int>(q.size());
    res.reserve(q.size());
    for (int i = 0; i < n; ++i) {
        const auto& pre = q[(i + n - 1) % n];
        const auto& cur = q[i];
        const auto& nxt = q[(i + 1) % n];
        if (orient(pre, cur, nxt) != 0) res.push_back(cur);
    }
    return res;
}

// 注意: O(N^2)なので大きいNではスイープライン等を別途検討する
// 多角形が自己交差しない単純多角形か判定する O(N^2)
// 用途: 入力多角形に自己交差や隣接辺の重なりがないかを検査したいときに使う
// 使い方: 頂点列を閉じずに渡す、始点を末尾に重ねた入力はfalseになりやすいので先に整形する
template<class T>
bool is_simple_polygon(const Polygon<T>& p) {
    int n = static_cast<int>(p.size());
    if (n < 3) return false;
    for (int i = 0; i < n; ++i) {
        if (p[i] == p[(i + 1) % n]) return false;
    }

    /* 隣接辺は共有端点だけの交差を許し、非隣接辺は一切の交差を許さない */
    for (int i = 0; i < n; ++i) {
        Segment<T> a(p[i], p[(i + 1) % n]);
        for (int j = i + 1; j < n; ++j) {
            Segment<T> b(p[j], p[(j + 1) % n]);
            bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
            if (!intersect(a, b)) continue;
            if (!adjacent) return false;
            auto ov = overlap_segment(a, b);
            if (ov && ov->a != ov->b) return false;
        }
    }
    return area2(p) > 0;
}

// 凸多角形に対する点の内包関係を返す O(log N)
// 用途: 凸多角形への点クエリが多いとき、一般多角形のO(N)判定を高速化したいときに使う
// 使い方: 入力は反時計回りの凸多角形を想定、convex_hullの返り値ならそのまま使える
// 注意: 凸でない多角形を渡すと結果は保証しない
template<class T>
Contains convex_contains(const Polygon<T>& g, const Point<T>& p) {
    int n = static_cast<int>(g.size());
    if (n == 0) return OUTSIDE;
    if (n == 1) return p == g[0] ? ON_EDGE : OUTSIDE;
    if (n == 2) return on_segment(Segment<T>(g[0], g[1]), p) ? ON_EDGE : OUTSIDE;
    if (orient(g[0], g[1], p) < 0) return OUTSIDE;
    if (orient(g[0], g[n - 1], p) > 0) return OUTSIDE;
    if (on_segment(Segment<T>(g[0], g[1]), p) || on_segment(Segment<T>(g[0], g[n - 1]), p)) return ON_EDGE;
    int l = 1, r = n - 1;
    while (r - l > 1) {
        int m = (l + r) / 2;
        if (orient(g[0], g[m], p) >= 0) l = m;
        else r = m;
    }
    int s = orient(g[l], g[(l + 1) % n], p);
    if (s < 0) return OUTSIDE;
    if (s == 0) return ON_EDGE;
    return INSIDE;
}

// 指定方向の内積を最大にする点の添字を返す O(N)
// 用途: 支持点、射影最大値、凸図形の端点候補を求めたいときに使う
// 使い方: support_index(ps,dir) はdot(p,dir)が最大の点の添字を返し、空なら-1
template<class T, class U>
int support_index(const std::vector<Point<T>>& ps, const Point<U>& dir) {
    if (ps.empty()) return -1;
    int ans = 0;
    auto best = ps[0].dot(dir);
    for (int i = 1; i < static_cast<int>(ps.size()); ++i) {
        auto v = ps[i].dot(dir);
        if (v > best) {
            best = v;
            ans = i;
        }
    }
    return ans;
}

// 指定方向の最大内積値を返す O(N)
// 用途: 点集合を方向dirへ射影した最大値、半平面の上限値を求めたいときに使う
// 使い方: support_value(ps,dir) はmax dot(p,dir)を返し、空なら0を返す
template<class T, class U>
std::common_type_t<T, U> support_value(const std::vector<Point<T>>& ps, const Point<U>& dir) {
    int id = support_index(ps, dir);
    if (id < 0) return std::common_type_t<T, U>(0);
    return ps[id].dot(dir);
}

// 指定方向の内積を最大にする点を返す O(N)
// 用途: 方向dirから見た最も外側の点そのものが欲しいときに使う
// 使い方: support_point(ps,dir) は該当点を返し、空ならPoint<T>()を返す
template<class T, class U>
Point<T> support_point(const std::vector<Point<T>>& ps, const Point<U>& dir) {
    int id = support_index(ps, dir);
    if (id < 0) return Point<T>();
    return ps[id];
}

// 点集合の指定方向に対する幅を返す O(N)
// 用途: 点集合をある方向へ射影した長さ、凸図形の幅、支持関数の差を求めたいときに使う
// 使い方: width<double>(ps,dir) は (max dot - min dot) / |dir| を返す
// 注意: 空集合またはゼロ方向ベクトルでは0を返す、返り値は実数
template<class F = double, class T, class U>
F width(const std::vector<Point<T>>& ps, const Point<U>& dir) {
    if (ps.empty() || dir.is_zero()) return F(0);
    auto mx = support_value(ps, dir);
    auto mn_neg = support_value(ps, -dir);
    return static_cast<F>(mx + mn_neg) / dir.template norm<F>();
}

namespace internal {

template<class F>
bool same_real_point(const Point<F>& a, const Point<F>& b) {
    F eps = F(1e-10);
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps;
}

template<class F, class U>
Polygon<F> convex_cut_real(const Polygon<F>& g, const Line<U>& raw) {
    Polygon<F> res;
    int n = static_cast<int>(g.size());
    if (n == 0 || raw.a == raw.b) return res;
    Line<F> l(raw.a.template to_real<F>(), raw.b.template to_real<F>());
    Point<F> v = l.vec();
    F eps = F(1e-10);
    auto value = [&](const Point<F>& p) { return v.cross(p - l.a); };
    auto inside = [&](const Point<F>& p) { return value(p) >= -eps; };
    auto push_unique = [&](const Point<F>& p) {
        if (!res.empty() && same_real_point(res.back(), p)) return;
        res.push_back(p);
    };

    for (int i = 0; i < n; ++i) {
        Point<F> a = g[i];
        Point<F> b = g[(i + 1) % n];
        bool ina = inside(a);
        bool inb = inside(b);
        if (ina != inb) {
            F va = value(a);
            F vb = value(b);
            F den = va - vb;
            if (std::abs(den) > eps) push_unique(a + (b - a) * (va / den));
        }
        if (inb) push_unique(b);
    }
    if (res.size() >= 2 && same_real_point(res.front(), res.back())) res.pop_back();
    return res;
}

}  // namespace internal

// 凸多角形を有向直線の左側半平面で切断する O(N)
// 用途: 凸多角形と半平面の共通部分、クリッピング、制約を1本ずつ追加する処理に使う
// 使い方: convex_cut<double>(poly,line) はline.a→line.bの左側と直線上だけを残す
// 注意: 交点が整数とは限らないためPolygon<F>を返す、入力は凸多角形を想定
template<class F = double, class T>
Polygon<F> convex_cut(const Polygon<T>& g, const Line<T>& l) {
    Polygon<F> poly;
    poly.reserve(g.size());
    for (const auto& p : g) poly.push_back(p.template to_real<F>());
    return internal::convex_cut_real(poly, l);
}

// 凸多角形同士の共通部分を返す O(NM)
// 用途: 2つの凸領域の交差、多角形クリッピング、共通面積計算に使う
// 使い方: convex_intersection<double>(a,b) は空・点・線分・凸多角形のいずれかを返す
// 注意: 入力は凸多角形を想定し、交点が整数とは限らないためPolygon<F>を返す
template<class F = double, class T>
Polygon<F> convex_intersection(Polygon<T> a, Polygon<T> b) {
    a = compress_collinear(std::move(a));
    b = compress_collinear(std::move(b));
    if (a.empty() || b.empty()) return {};
    if (signed_area2(a) < T(0)) std::reverse(a.begin(), a.end());
    if (signed_area2(b) < T(0)) std::reverse(b.begin(), b.end());
    Polygon<F> res;
    res.reserve(a.size());
    for (const auto& p : a) res.push_back(p.template to_real<F>());
    int n = static_cast<int>(b.size());
    for (int i = 0; i < n && !res.empty(); ++i) res = internal::convex_cut_real(res, Line<T>(b[i], b[(i + 1) % n]));
    return res;
}

// 凸多角形同士の共通面積を返す O(NM)
// 用途: 2つの凸領域の重なり面積だけが必要なときに使う
// 使い方: convex_intersection_area<double>(a,b) は convex_intersection の面積を返す
// 注意: 点や線分だけで接する場合の面積は0
template<class F = double, class T>
F convex_intersection_area(const Polygon<T>& a, const Polygon<T>& b) {
    return area<F>(convex_intersection<F>(a, b));
}

// 半平面集合の共通部分を大きな正方形でクリップして返す O(KV)
// 用途: 線形不等式の共通領域、半平面制約の簡易処理、凸領域の逐次クリップに使う
// 使い方: 各Lineの左側を残し、boundで初期正方形 [-bound,bound]^2 を指定する
// 注意: 非有界な共通部分は指定正方形で切られた多角形として返る
template<class F = double, class T>
Polygon<F> half_plane_intersection(const std::vector<Line<T>>& hs, F bound) {
    Polygon<F> res{Point<F>(-bound, -bound), Point<F>(bound, -bound), Point<F>(bound, bound), Point<F>(-bound, bound)};
    for (const auto& h : hs) {
        res = internal::convex_cut_real(res, h);
        if (res.empty()) break;
    }
    return res;
}

// 半平面集合の共通部分をデフォルト正方形でクリップして返す O(KV)
// 用途: 座標範囲が十分小さい問題で、簡単に半平面交差を試したいときに使う
// 使い方: 精度や座標範囲が重要な場合はbound明示版を使う
// 注意: デフォルトboundは1e9
template<class F = double, class T>
Polygon<F> half_plane_intersection(const std::vector<Line<T>>& hs) {
    return half_plane_intersection<F>(hs, F(1e9));
}

// 一般多角形領域と線分が交わるか判定する O(N)
// 用途: 線分経路が障害物多角形に入るか、境界を横切るかを判定する
// 使い方: 線分端点が内部・境界上にある場合もtrueを返す
// 注意: 多角形は閉領域として扱い、自己交差多角形では通常の領域判定と意味がずれ得る
template<class T>
bool intersect(const Polygon<T>& g, const Segment<T>& s) {
    if (g.empty()) return false;
    if (contains(g, s.a) != OUTSIDE || contains(g, s.b) != OUTSIDE) return true;
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) {
        if (intersect(Segment<T>(g[i], g[(i + 1) % n]), s)) return true;
    }
    return false;
}

// 線分と一般多角形領域が交わるか判定する O(N)
template<class T>
bool intersect(const Segment<T>& s, const Polygon<T>& g) {
    return intersect(g, s);
}

// 一般多角形領域同士が交わるか判定する O(NM)
// 用途: 多角形障害物同士の衝突、領域の共通部分の有無判定に使う
// 使い方: 片方の頂点がもう片方に含まれる場合や、辺が交差する場合にtrue
// 注意: 多角形は閉領域として扱い、境界接触もtrue
template<class T>
bool intersect(const Polygon<T>& a, const Polygon<T>& b) {
    if (a.empty() || b.empty()) return false;
    for (const auto& p : a) if (contains(b, p) != OUTSIDE) return true;
    for (const auto& p : b) if (contains(a, p) != OUTSIDE) return true;
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    for (int i = 0; i < n; ++i) {
        Segment<T> ea(a[i], a[(i + 1) % n]);
        for (int j = 0; j < m; ++j) {
            if (intersect(ea, Segment<T>(b[j], b[(j + 1) % m]))) return true;
        }
    }
    return false;
}

// 一般多角形領域と点の距離を返す O(N)
// 用途: 点が障害物領域からどれだけ離れているかを求めるときに使う
// 使い方: 点が内部または境界上なら0、空多角形ならinfinity
// 注意: 多角形の境界までの最短距離を使う
template<class F = double, class T>
F distance(const Polygon<T>& g, const Point<T>& p) {
    if (g.empty()) return std::numeric_limits<F>::infinity();
    if (contains(g, p) != OUTSIDE) return F(0);
    F best = std::numeric_limits<F>::infinity();
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) best = std::min(best, distance<F>(Segment<T>(g[i], g[(i + 1) % n]), p));
    return best;
}

// 点と一般多角形領域の距離を返す O(N)
template<class F = double, class T>
F distance(const Point<T>& p, const Polygon<T>& g) {
    return distance<F>(g, p);
}

// 一般多角形領域と線分の距離を返す O(N)
// 用途: 線分経路と障害物領域の最短距離を求めるときに使う
// 使い方: 交差していれば0、空多角形ならinfinity
// 注意: 多角形は閉領域として扱う
template<class F = double, class T>
F distance(const Polygon<T>& g, const Segment<T>& s) {
    if (g.empty()) return std::numeric_limits<F>::infinity();
    if (intersect(g, s)) return F(0);
    F best = std::min(distance<F>(g, s.a), distance<F>(g, s.b));
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) best = std::min(best, distance<F>(Segment<T>(g[i], g[(i + 1) % n]), s));
    return best;
}

// 線分と一般多角形領域の距離を返す O(N)
template<class F = double, class T>
F distance(const Segment<T>& s, const Polygon<T>& g) {
    return distance<F>(g, s);
}

// 一般多角形領域同士の距離を返す O(NM)
// 用途: 多角形障害物間の最短距離、衝突していない領域の間隔計算に使う
// 使い方: 交差していれば0、どちらかが空ならinfinity
// 注意: 多角形は閉領域として扱う
template<class F = double, class T>
F distance(const Polygon<T>& a, const Polygon<T>& b) {
    if (a.empty() || b.empty()) return std::numeric_limits<F>::infinity();
    if (intersect(a, b)) return F(0);
    F best = std::numeric_limits<F>::infinity();
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    for (int i = 0; i < n; ++i) {
        Segment<T> ea(a[i], a[(i + 1) % n]);
        for (int j = 0; j < m; ++j) best = std::min(best, distance<F>(ea, Segment<T>(b[j], b[(j + 1) % m])));
    }
    return best;
}

// 凸多角形の最遠点対の添字を返す O(N)
// 用途: 凸包上の直径の端点を、距離値だけでなく添字でも知りたいときに使う
// 使い方: 入力は反時計回り凸多角形を想定、空なら{-1,-1}、1点なら{0,0}
// 原理: 回転キャリパーで各辺に対する対蹠点を進める
template<class T>
std::pair<int, int> convex_diameter_pair(const Polygon<T>& p) {
    int n = static_cast<int>(p.size());
    if (n == 0) return {-1, -1};
    if (n == 1) return {0, 0};
    if (n == 2) return {0, 1};
    int j = 1;
    T best = 0;
    std::pair<int, int> ans{0, 0};
    for (int i = 0; i < n; ++i) {
        int ni = (i + 1) % n;
        Point<T> e = p[ni] - p[i];
        int guard = 0;
        while (guard < n) {
            int nj = (j + 1) % n;
            auto cur = internal::abs_value(e.cross(p[j] - p[i]));
            auto nxt = internal::abs_value(e.cross(p[nj] - p[i]));
            if (nxt > cur) j = nj;
            else break;
            ++guard;
        }
        for (int id : {i, ni}) {
            auto d = p[id].dist2(p[j]);
            if (d > best) {
                best = d;
                ans = {id, j};
            }
        }
    }
    return ans;
}

// 凸多角形の直径2乗を返す O(N)
// 用途: 凸包済み点集合の最遠点対距離を整数の2乗値で求めたいときに使う
// 使い方: convex_diameter2(convex_hull(ps)) のように使う、空なら0
template<class T>
T convex_diameter2(const Polygon<T>& p) {
    auto [i, j] = convex_diameter_pair(p);
    if (i < 0) return 0;
    return p[i].dist2(p[j]);
}

// 凸多角形の直径を返す O(N)
// 用途: 最遠点対の実距離が必要なときに使う
// 使い方: convex_diameter<double>(hull) は sqrt(convex_diameter2(hull)) を返す
// 注意: 比較だけなら平方根を避けてconvex_diameter2を使う
template<class F = double, class T>
F convex_diameter(const Polygon<T>& p) {
    return std::sqrt(static_cast<F>(convex_diameter2(p)));
}

// 点集合の直径2乗を返す O(N log N)
// 用途: 任意点集合の最遠点対距離を求めたいときに使う
// 使い方: diameter2(ps) は内部で凸包を作ってから回転キャリパーを行う
template<class T>
T diameter2(std::vector<Point<T>> ps) {
    return convex_diameter2(convex_hull(std::move(ps)));
}

// 点集合の直径を返す O(N log N)
// 用途: 任意点集合の最遠点対の実距離が必要なときに使う
// 使い方: diameter<double>(ps) は sqrt(diameter2(ps)) を返す
// 注意: 距離比較だけならdiameter2を使う
template<class F = double, class T>
F diameter(std::vector<Point<T>> ps) {
    return std::sqrt(static_cast<F>(diameter2(std::move(ps))));
}

// 最近点対の距離2乗を返す O(N log N)
// 用途: 点集合の最小距離をO(N^2)ではなく高速に求めたいときに使う
// 使い方: closest_pair2(ps) は距離2乗を返す、2点未満なら0、重複点があれば0
// 注意: 分割統治中に配列をy順へ並べ替えるため、引数は値渡しで受け取る
template<class T>
T closest_pair2(std::vector<Point<T>> ps) {
    using P = Point<T>;
    int n = static_cast<int>(ps.size());
    if (n < 2) return 0;
    std::sort(ps.begin(), ps.end());
    for (int i = 1; i < n; ++i) if (ps[i] == ps[i - 1]) return 0;
    std::vector<P> buf(n);
    T inf = std::numeric_limits<T>::max();

    auto rec = [&](auto&& self, int l, int r) -> T {
        if (r - l <= 1) return inf;
        int m = (l + r) / 2;
        T midx = ps[m].x;
        T d = std::min(self(self, l, m), self(self, m, r));
        std::inplace_merge(ps.begin() + l, ps.begin() + m, ps.begin() + r, [](const P& a, const P& b) {
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        });
        int sz = 0;
        for (int i = l; i < r; ++i) {
            T dx = ps[i].x - midx;
            if (dx * dx >= d) continue;
            for (int j = sz - 1; j >= 0; --j) {
                T dy = ps[i].y - buf[j].y;
                if (dy * dy >= d) break;
                d = std::min(d, ps[i].dist2(buf[j]));
            }
            buf[sz++] = ps[i];
        }
        return d;
    };
    return rec(rec, 0, n);
}

// 最近点対の距離を返す O(N log N)
// 用途: 最小距離の実数値が必要なときに使う
// 使い方: closest_pair_dist<double>(ps) は sqrt(closest_pair2(ps)) を返す
// 注意: 比較だけならclosest_pair2を使う
template<class F = double, class T>
F closest_pair_dist(std::vector<Point<T>> ps) {
    return std::sqrt(static_cast<F>(closest_pair2(std::move(ps))));
}

// 最近点対を返す O(N^2)
// 用途: 最近点対の点そのものが欲しい小規模ケースやデバッグ用に使う
// 使い方: closest_pair(ps) は2点未満なら{Point<T>(),Point<T>()}を返す
// 注意: 点対取得版はシンプルさ優先でO(N^2)、距離だけならclosest_pair2を使う
template<class T>
std::pair<Point<T>, Point<T>> closest_pair(const std::vector<Point<T>>& ps) {
    if (ps.size() < 2) return {Point<T>(), Point<T>()};
    auto best = ps[0].dist2(ps[1]);
    std::pair<Point<T>, Point<T>> ans{ps[0], ps[1]};
    for (int i = 0; i < static_cast<int>(ps.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(ps.size()); ++j) {
            auto d = ps[i].dist2(ps[j]);
            if (d < best) {
                best = d;
                ans = {ps[i], ps[j]};
            }
        }
    }
    return ans;
}

// 線分上の格子点数を返す O(log C)
template<class T>
T lattice_points_on_segment(const Segment<T>& s) {
    return internal::gcd_abs(s.a.x - s.b.x, s.a.y - s.b.y) + 1;
}

// 多角形の境界上の格子点数を返す O(N log C)
template<class T>
T boundary_lattice_points(const Polygon<T>& p) {
    T ans = 0;
    int n = static_cast<int>(p.size());
    for (int i = 0; i < n; ++i) ans += internal::gcd_abs(p[i].x - p[(i + 1) % n].x, p[i].y - p[(i + 1) % n].y);
    return ans;
}

// Pickの定理で多角形内部の格子点数を返す O(N log C)
// 用途: 格子多角形の内部格子点数を面積2倍と境界点数から求めたいときに使う
// 使い方: 頂点が整数座標の単純多角形を渡す、境界点を含めた総数はlattice_pointsを使う
template<class T>
T interior_lattice_points(const Polygon<T>& p) {
    auto a2 = area2(p);
    auto b = boundary_lattice_points(p);
    return (a2 - b + 2) / 2;
}

// Pickの定理で多角形内外境界込みの格子点数を返す O(N log C)
template<class T>
T lattice_points(const Polygon<T>& p) {
    return interior_lattice_points(p) + boundary_lattice_points(p);
}

// 一番下、同点なら一番左の頂点添字を返す O(N)
template<class T>
int bottom_left_index(const Polygon<T>& p) {
    if (p.empty()) return -1;
    int ans = 0;
    for (int i = 1; i < static_cast<int>(p.size()); ++i) {
        if (std::pair<T, T>(p[i].y, p[i].x) < std::pair<T, T>(p[ans].y, p[ans].x)) ans = i;
    }
    return ans;
}

// 頂点座標の単純平均を返す O(N)
template<class F = double, class T>
Point<F> vertex_mean(const std::vector<Point<T>>& ps) {
    if (ps.empty()) return Point<F>(0, 0);
    F sx = 0;
    F sy = 0;
    for (const auto& p : ps) {
        sx += static_cast<F>(p.x);
        sy += static_cast<F>(p.y);
    }
    F n = static_cast<F>(ps.size());
    return Point<F>(sx / n, sy / n);
}

// 多角形の面積重心を返す O(N)
// 用途: 多角形を一様な面として見た重心を求めたいときに使う
// 使い方: polygon_centroid<double>(poly) は面積重心を返す、面積0ならvertex_meanにフォールバック
// 注意: vertex_meanは頂点平均であり、こちらは面積重心
template<class F = double, class T>
Point<F> polygon_centroid(const Polygon<T>& p) {
    int n = static_cast<int>(p.size());
    if (n == 0) return Point<F>(0, 0);
    F sx = F(0);
    F sy = F(0);
    F cr_sum = F(0);
    for (int i = 0; i < n; ++i) {
        const auto& a = p[i];
        const auto& b = p[(i + 1) % n];
        F cr = static_cast<F>(a.x) * static_cast<F>(b.y) - static_cast<F>(a.y) * static_cast<F>(b.x);
        cr_sum += cr;
        sx += (static_cast<F>(a.x) + static_cast<F>(b.x)) * cr;
        sy += (static_cast<F>(a.y) + static_cast<F>(b.y)) * cr;
    }
    if (std::abs(cr_sum) <= F(1e-30)) return vertex_mean<F>(p);
    return Point<F>(sx / (F(3) * cr_sum), sy / (F(3) * cr_sum));
}

// 指定点から最も遠い点と距離を返す O(N)
// 用途: あるクエリ点から最も遠い入力点を愚直に探したいときに使う
// 使い方: auto [p,d] = furthest_point(ps,q); 空集合ではpはデフォルト点、dは0扱い
// 注意: 点集合全体の直径ならdiameter2を使う
template<class F = double, class T, class U>
std::pair<Point<T>, F> furthest_point(const std::vector<Point<T>>& ps, const Point<U>& q) {
    Point<T> best{};
    F best_d2 = F(-1);
    for (const auto& p : ps) {
        F dx = static_cast<F>(p.x) - static_cast<F>(q.x);
        F dy = static_cast<F>(p.y) - static_cast<F>(q.y);
        F d2 = dx * dx + dy * dy;
        if (d2 > best_d2) {
            best_d2 = d2;
            best = p;
        }
    }
    return {best, std::sqrt(std::max<F>(F(0), best_d2))};
}


// 一番下左の頂点が先頭になるよう回転する O(N)
template<class T>
void rotate_to_bottom_left(Polygon<T>& p) {
    int id = bottom_left_index(p);
    if (id > 0) std::rotate(p.begin(), p.begin() + id, p.end());
}

// 向きと開始位置を正規化した多角形を返す O(N)
// 用途: 多角形を比較したい、凸多角形のミンコフスキー和前に開始点と向きを揃えたいときに使う
// 使い方: normalized_polygon(poly,true) は反時計回りにし、一番下左の頂点を先頭にする
template<class T>
Polygon<T> normalized_polygon(Polygon<T> p, bool ccw_order = true) {
    if (p.empty()) return p;
    if ((signed_area2(p) > 0) != ccw_order) std::reverse(p.begin(), p.end());
    rotate_to_bottom_left(p);
    return p;
}

// 凸多角形同士のミンコフスキー和を返す O((N+M) log(N+M))
// 用途: 凸図形の和、到達可能領域、障害物膨張、ベクトル和の外周を求めるときに使う
// 使い方: minkowski_sum_convex(a,b) は入力を凸包化してから和の凸多角形を返す
// 注意: すでに正規化済み凸多角形でも、シンプルさ優先で内部でconvex_hullを呼ぶ
template<class T>
Polygon<T> minkowski_sum_convex(Polygon<T> a, Polygon<T> b) {
    if (a.empty() || b.empty()) return {};
    a = convex_hull(a, false);
    b = convex_hull(b, false);
    if (a.size() == 1 && b.size() == 1) return {a[0] + b[0]};
    rotate_to_bottom_left(a);
    rotate_to_bottom_left(b);
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    std::vector<Point<T>> ea(n), eb(m);
    for (int i = 0; i < n; ++i) ea[i] = a[(i + 1) % n] - a[i];
    for (int i = 0; i < m; ++i) eb[i] = b[(i + 1) % m] - b[i];
    Polygon<T> res;
    res.push_back(a[0] + b[0]);
    int i = 0, j = 0;
    while (i < n || j < m) {
        if (j == m || (i < n && internal::sign(ea[i].cross(eb[j])) >= 0)) res.push_back(res.back() + ea[i++]);
        else res.push_back(res.back() + eb[j++]);
    }
    if (!res.empty()) res.pop_back();
    return convex_hull(res, false);
}

// 点集合をすべて含む最小円を返す 期待O(N)
// 用途: 最小通信半径、クラスタ外接円、点集合を覆う最小円が必要な問題に使う
// 使い方: minimum_enclosing_circle<double>(ps) は空ならnullopt、1点なら半径0の円を返す
// 注意: 結果は一般に整数でないためCircle<F>を返す、固定シードでシャッフルする
template<class F = double, class T>
std::optional<Circle<F>> minimum_enclosing_circle(std::vector<Point<T>> ps) {
    if (ps.empty()) return std::nullopt;
    std::vector<Point<F>> qs;
    qs.reserve(ps.size());
    for (const auto& p : ps) qs.push_back(p.template to_real<F>());
    std::mt19937 rng(0);
    std::shuffle(qs.begin(), qs.end(), rng);
    Circle<F> c(qs[0], F(0));
    F eps = F(1e-10);
    auto inside = [&](const Circle<F>& cir, const Point<F>& p) {
        return cir.c.dist(p) <= cir.r + eps;
    };
    for (int i = 0; i < static_cast<int>(qs.size()); ++i) {
        if (inside(c, qs[i])) continue;
        c = Circle<F>(qs[i], F(0));
        for (int j = 0; j < i; ++j) {
            if (inside(c, qs[j])) continue;
            Point<F> cen = (qs[i] + qs[j]) / F(2);
            c = Circle<F>(cen, cen.dist(qs[i]));
            for (int k = 0; k < j; ++k) {
                if (inside(c, qs[k])) continue;
                auto cc = circumcircle<F>(qs[i], qs[j], qs[k]);
                if (cc) c = *cc;
            }
        }
    }
    return c;
}

// 用途: 線対称判定で、空集合・1点・全点同一直線・重複点の扱いを調整するための設定
// 使い方: SymmetryOption opt; opt.keep_multiplicity=false; のように変更してhas_reflection_symmetryへ渡す
struct SymmetryOption {
    bool allow_empty = true;
    bool allow_point = true;
    bool allow_line = true;
    bool allow_degenerate_polygon = true;
    bool keep_multiplicity = true;
};

// 点集合がいずれかの直線に対して線対称か判定する O(HN log N)
// 用途: 点集合や多角形頂点集合が何らかの反射対称性を持つか調べたいときに使う
// 使い方: has_reflection_symmetry(ps,opt) は凸包から対称軸候補を作り、全点を反射して照合する
// 注意: 反射後も整数座標に戻る場合だけtrue、重複点を区別するかはopt.keep_multiplicityで指定する
template<class T>
bool has_reflection_symmetry(std::vector<Point<T>> ps, SymmetryOption opt = {}) {
    if (ps.empty()) return opt.allow_empty;
    if (!opt.keep_multiplicity) {
        std::sort(ps.begin(), ps.end());
        ps.erase(std::unique(ps.begin(), ps.end()), ps.end());
    }
    bool all_same = true;
    for (const auto& p : ps) all_same = all_same && (p == ps[0]);
    if (all_same) return opt.allow_point;

    std::vector<Point<T>> sorted = ps;
    std::sort(sorted.begin(), sorted.end());
    auto hull = convex_hull(ps, false);
    int h = static_cast<int>(hull.size());
    if (h == 0) return false;

    T sx = 0;
    T sy = 0;
    for (const auto& p : hull) {
        sx += p.x;
        sy += p.y;
    }
    T scale = T(2) * h;
    Point<T> center2(sx * T(2), sy * T(2));

    auto test_axis = [&](Point<T> d) -> bool {
        if (d.is_zero()) return false;
        T den = d.norm2();
        T div = den * scale;
        std::vector<Point<T>> ref;
        ref.reserve(ps.size());
        bool all_on_axis = true;

        /* 拡大座標上で反射公式を使い、元の整数格子点に戻るかを剰余で検査する */
        for (const auto& p0 : ps) {
            Point<T> p(p0.x * scale, p0.y * scale);
            Point<T> v = p - center2;
            T dt = v.dot(d);
            Point<T> num = center2 * den + d * (dt * T(2)) - v * den;
            if (num.x % div != 0 || num.y % div != 0) return false;
            Point<T> rp(num.x / div, num.y / div);
            if (rp != p0) all_on_axis = false;
            ref.push_back(rp);
        }
        if (all_on_axis && !opt.allow_line) return false;
        if (all_on_axis && !opt.allow_degenerate_polygon && ps.size() > 2) return false;
        std::sort(ref.begin(), ref.end());
        return ref == sorted;
    };

    /* 凸包サイズ2では、線分自身の軸と垂直二等分線の両方を候補にする */
    if (h == 1) return opt.allow_point;
    if (h == 2) {
        Point<T> d1(hull[0].x * scale - center2.x, hull[0].y * scale - center2.y);
        Point<T> raw = hull[1] - hull[0];
        Point<T> d2 = raw.rot90() * scale;
        return test_axis(d1) || test_axis(d2);
    }

    for (int i = 0; i < h; ++i) {
        Point<T> dv(hull[i].x * scale - center2.x, hull[i].y * scale - center2.y);
        if (test_axis(dv)) return true;
        Point<T> dm((hull[i].x + hull[(i + 1) % h].x) * h - center2.x,
                    (hull[i].y + hull[(i + 1) % h].y) * h - center2.y);
        if (test_axis(dm)) return true;
    }
    return false;
}

namespace internal {

struct Fenwick {
    int n = 0;
    std::vector<int> bit;

    Fenwick() = default;

    explicit Fenwick(int n_) : n(n_), bit(n_ + 1, 0) {}

    void add(int idx, int val) {
        for (++idx; idx <= n; idx += idx & -idx) bit[idx] += val;
    }

    int sum_prefix(int idx) const {
        int s = 0;
        for (++idx; idx > 0; idx -= idx & -idx) s += bit[idx];
        return s;
    }

    int sum_range(int l, int r) const {
        if (r < l) return 0;
        return sum_prefix(r) - (l == 0 ? 0 : sum_prefix(l - 1));
    }
};

}  // namespace internal

// 軸平行線分群の水平線分と垂直線分の交差数を数える O(N log N)
// 用途: マンハッタン幾何で、水平線分と垂直線分の交差数を高速に数えたいときに使う
// 使い方: 入力線分は水平または垂直だけを想定、端点での交差も数える
// 注意: 水平同士・垂直同士の重なりは数えない、斜め線分は無視される
template<class T>
long long count_axis_aligned_intersections(std::vector<Segment<T>> segs) {
    struct Event {
        T x{};
        int type{};
        T y{};
        T y1{};
        T y2{};
        bool operator<(const Event& e) const {
            if (x != e.x) return x < e.x;
            return type < e.type;
        }
    };

    std::vector<Event> events;
    std::vector<T> ys;

    /* add -> query -> remove の順に同じx座標を処理し、端点交差も数える */
    for (auto s : segs) {
        if (s.a.y == s.b.y) {
            if (s.b.x < s.a.x) std::swap(s.a, s.b);
            events.push_back(Event{s.a.x, 0, s.a.y, {}, {}});
            events.push_back(Event{s.b.x, 2, s.a.y, {}, {}});
            ys.push_back(s.a.y);
        } else if (s.a.x == s.b.x) {
            if (s.b.y < s.a.y) std::swap(s.a, s.b);
            events.push_back(Event{s.a.x, 1, {}, s.a.y, s.b.y});
        }
    }
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    std::sort(events.begin(), events.end());
    internal::Fenwick fw(static_cast<int>(ys.size()));
    long long ans = 0;
    for (const auto& e : events) {
        if (e.type == 0) {
            int id = static_cast<int>(std::lower_bound(ys.begin(), ys.end(), e.y) - ys.begin());
            fw.add(id, 1);
        } else if (e.type == 2) {
            int id = static_cast<int>(std::lower_bound(ys.begin(), ys.end(), e.y) - ys.begin());
            fw.add(id, -1);
        } else {
            int l = static_cast<int>(std::lower_bound(ys.begin(), ys.end(), e.y1) - ys.begin());
            int r = static_cast<int>(std::upper_bound(ys.begin(), ys.end(), e.y2) - ys.begin()) - 1;
            ans += fw.sum_range(l, r);
        }
    }
    return ans;
}

// 用途: 点を逐次追加しながら、必要な時点の凸包・面積・内外判定を取得するための簡易動的凸包
// 使い方: strict=trueなら凸包辺上の点は頂点として残さず、strict=falseなら残す
template<class T = long long, bool strict = true>
struct IncrementalConvexHull {
    using P = Point<T>;

    std::vector<P> points_;
    mutable Polygon<T> hull_;
    mutable bool dirty_ = true;

    // 空の凸包を生成する O(1)
    IncrementalConvexHull() = default;

    // 用途: 点を逐次追加し、必要なタイミングで現在の凸包や面積を参照したいときに使う
    // 使い方: add(p)は点を保存してdirtyにするだけ、実際の凸包再構築はhull/size/side/area2で遅延実行される
    // 点を追加する O(1)
    void add(const P& p) {
        points_.push_back(p);
        dirty_ = true;
    }

    // 入力点が空か判定する O(1)
    bool empty() const { return points_.empty(); }

    // 用途: 追加済み点集合の現在の凸包頂点列を取得したいときに使う
    // 使い方: ich.hull() は内部キャッシュへのconst参照を返す、次にaddすると参照先は再構築されうる
    // 現在の凸包を返す O(N log N) dirty時、以降O(1)
    const Polygon<T>& hull() const {
        rebuild();
        return hull_;
    }

    // 用途: 追加済み点集合の現在の凸包頂点数だけ知りたいときに使う
    // 使い方: 初回またはadd後は内部で凸包を再構築し、その後はキャッシュされたサイズを返す
    // 凸包頂点数を返す O(N log N) dirty時、以降O(1)
    int size() const {
        rebuild();
        return static_cast<int>(hull_.size());
    }

    // 用途: 追加済み点集合の凸包に対して、クエリ点が内側・境界・外側かを知りたいときに使う
    // 使い方: side(p) は内部1、境界0、外部-1を返す
    // 点の凸包に対する位置を返す O(N log N) dirty時、以降O(log H)
    int side(const P& p) const {
        rebuild();
        Contains c = convex_contains(hull_, p);
        if (c == INSIDE) return 1;
        if (c == ON_EDGE) return 0;
        return -1;
    }

    // 用途: 逐次追加後の凸包面積を整数の2倍値で取得したいときに使う
    // 使い方: add後の初回呼び出しでは凸包を再構築する。通常の多角形area2と同じ向きなしの値を返す
    // 凸包の面積2倍を返す O(N log N) dirty時、以降O(H)
    T area2() const {
        rebuild();
        return geo::area2(hull_);
    }

    // 凸包の面積を返す O(N log N) dirty時、以降O(H)
    // 用途: 逐次追加後の凸包面積を実数値で取得したいときに使う
    // 使い方: area<double>() はarea2()/2を返す。誤差を避けたい比較にはarea2を使う
    template<class F = double>
    F area() const { return static_cast<F>(area2()) / F(2); }

private:
    void rebuild() const {
        if (!dirty_) return;
        hull_ = convex_hull(points_, !strict);
        dirty_ = false;
    }
};

// 等速直線運動する点へ到達できる最短時刻と到達点を返す O(1)
// 用途: 追跡・迎撃問題で、一定速度で動く対象に一定速さで到達可能かを判定したいときに使う
// 使い方: auto [ok,t,p] = point_of_impact(shooter,speed,target,velocity); ok=falseなら到達不能
// 注意: 連続時間の実数計算であり、整数幾何の厳密判定APIではない
template<class F = double>
std::tuple<bool, F, Point<F>> point_of_impact(Point<F> shooter, F speed, Point<F> target, Point<F> target_velocity) {
    Point<F> r = target - shooter;
    F a = target_velocity.dot(target_velocity) - speed * speed;
    F b = F(2) * r.dot(target_velocity);
    F c = r.dot(r);
    constexpr F eps = static_cast<F>(1e-12);
    if (std::abs(c) <= eps) return {true, F(0), target};

    std::vector<F> cand;
    if (std::abs(a) <= eps) {
        if (std::abs(b) > eps) cand.push_back(-c / b);
    } else {
        F d = b * b - F(4) * a * c;
        if (d < -eps) return {false, F(0), Point<F>()};
        d = std::max<F>(F(0), d);
        F sq = std::sqrt(d);
        cand.push_back((-b - sq) / (F(2) * a));
        cand.push_back((-b + sq) / (F(2) * a));
    }

    F best = std::numeric_limits<F>::infinity();
    for (F t : cand) {
        if (t >= -eps) best = std::min(best, std::max<F>(F(0), t));
    }
    if (!std::isfinite(best)) return {false, F(0), Point<F>()};
    return {true, best, target + target_velocity * best};
}

}  // namespace geo

#if __INCLUDE_LEVEL__ == 0
#include <chrono>

namespace geo_test {

using geo::Circle;
using geo::Line;
using geo::Point;
using geo::Polygon;
using geo::Segment;

int checks = 0;

void check(bool cond, const std::string& name) {
    ++checks;
    if (!cond) {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
}

template<class A, class B>
void check_eq(const A& a, const B& b, const std::string& name) {
    ++checks;
    if (!(a == b)) {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
}

void check_close(double a, double b, double eps, const std::string& name) {
    ++checks;
    if (std::abs(a - b) > eps) {
        std::cerr << "FAIL: " << name << " actual=" << static_cast<double>(a) << " expected=" << static_cast<double>(b) << '\n';
        std::exit(1);
    }
}

template<class F>
void check_point_close(const Point<F>& a, const Point<F>& b, double eps, const std::string& name) {
    check_close(a.x, b.x, eps, name + ".x");
    check_close(a.y, b.y, eps, name + ".y");
}

long long naive_manhattan(const std::vector<Segment<long long>>& ss) {
    long long ans = 0;
    for (const auto& a : ss) {
        bool ah = a.a.y == a.b.y;
        bool av = a.a.x == a.b.x && !ah;
        for (const auto& b : ss) {
            bool bh = b.a.y == b.b.y;
            bool bv = b.a.x == b.b.x && !bh;
            if (ah && bv) {
                long long hx1 = std::min(a.a.x, a.b.x), hx2 = std::max(a.a.x, a.b.x);
                long long vy1 = std::min(b.a.y, b.b.y), vy2 = std::max(b.a.y, b.b.y);
                if (hx1 <= b.a.x && b.a.x <= hx2 && vy1 <= a.a.y && a.a.y <= vy2) ++ans;
            } else if (av && bh) {
                long long hy1 = std::min(b.a.x, b.b.x), hy2 = std::max(b.a.x, b.b.x);
                long long vy1 = std::min(a.a.y, a.b.y), vy2 = std::max(a.a.y, a.b.y);
                if (hy1 <= a.a.x && a.a.x <= hy2 && vy1 <= b.a.y && b.a.y <= vy2) ++ans;
            }
        }
    }
    return ans / 2;
}

void test_point() {
    using P = Point<long long>;
    P a(3, 4), b(-1, 2);
    check_eq(a + b, P(2, 6), "Point +");
    check_eq(a - b, P(4, 2), "Point -");
    check_eq(-a, P(-3, -4), "Point unary -");
    check_eq(a * 2LL, P(6, 8), "Point * scalar");
    check_eq(2LL * a, P(6, 8), "scalar * Point");
    check_eq(a / 2LL, P(1, 2), "Point / scalar integer");
    P compound = a;
    compound += b;
    check_eq(compound, P(2, 6), "Point +=");
    compound -= b;
    check_eq(compound, a, "Point -=");
    check(a > b, "Point >");
    check(a != b, "Point !=");
    check_eq(a.dot(b), 5, "Point dot");
    check_eq(a.cross(b), 10, "Point cross");
    check_eq(a.norm2(), 25, "Point norm2");
    check_eq(a.dist2(b), 20, "Point dist2");
    check_eq(geo::dot(a, b), 5, "free dot");
    check_eq(geo::cross(a, b), 10, "free cross");
    check_eq(geo::norm2(a), 25, "free norm2");
    check_eq(geo::dist2(a, b), 20, "free dist2");
    check_close(a.norm<double>(), 5.0, 1e-12, "Point norm");
    check_close(a.dist<double>(b), std::sqrt(20.0), 1e-12, "Point dist");
    check_close(P(0, 1).arg<double>(), std::acos(-1.0) / 2, 1e-12, "Point arg");
    check_point_close(P(3, 4).unit<double>(), Point<double>(0.6, 0.8), 1e-12, "Point unit");
    check_point_close(P(0, 0).unit<double>(), Point<double>(0, 0), 1e-12, "Point unit zero");
    check_eq(a.rot90(), P(-4, 3), "Point rot90");
    check_eq(a.rot270(), P(4, -3), "Point rot270");
    check(!a.is_zero(), "Point is_zero false");
    check(P(0, 0).is_zero(), "Point is_zero true");
    check(P(2, 2).is_parallel(P(3, 3)), "Point is_parallel");
    check(P(1, 0).is_orthogonal(P(0, 5)), "Point is_orthogonal");
    auto [ok, ip] = Point<double>(2.0000000001, -3.0).try_round_to_int<long long>(1e-8);
    check(ok && ip == P(2, -3), "Point try_round_to_int ok");
    auto [ng, ip2] = Point<double>(2.1, -3.0).try_round_to_int<long long>(1e-8);
    check(!ng && ip2 == P(2, -3), "Point try_round_to_int ng");
    static_assert(std::is_same_v<decltype(Point<int>(3, 4).norm2()), int>);
    static_assert(std::is_same_v<decltype(Point<long long>(3, 4).norm2()), long long>);
#ifdef __SIZEOF_INT128__
    static_assert(std::is_same_v<decltype(Point<__int128_t>(3, 4).norm2()), __int128_t>);
#endif
    static_assert(std::is_same_v<decltype(Point<int>(3, 4).dot(Point<long long>(1, 2))), long long>);
    check(Point<long long>(3, 4).norm2() == 25, "Point norm2 keeps coordinate type");
    check_eq(geo::manhattan_dist(P(3, -4), P(-1, 2)), 10, "manhattan_dist");
    check_eq(geo::chebyshev_dist(P(3, -4), P(-1, 2)), 6, "chebyshev_dist");
    std::vector<P> box_ps = {P(3, -1), P(-2, 5), P(4, 0)};
    auto bb = geo::bounding_box(box_ps);
    check(bb && bb->first == P(-2, -1) && bb->second == P(4, 5), "bounding_box non-empty");
    check(!geo::bounding_box(std::vector<P>{}).has_value(), "bounding_box empty");
    check_eq(geo::max_manhattan_dist(box_ps), 11, "max_manhattan_dist");
    check_eq(geo::max_chebyshev_dist(box_ps), 6, "max_chebyshev_dist");
    check_eq(geo::max_manhattan_dist(std::vector<P>{}), 0, "max_manhattan_dist empty");
    check_eq(geo::max_manhattan_dist(std::vector<P>{P(1, 2)}), 0, "max_manhattan_dist singleton");
    check_eq(geo::max_chebyshev_dist(std::vector<P>{}), 0, "max_chebyshev_dist empty");
    check_eq(geo::max_chebyshev_dist(std::vector<P>{P(1, 2)}), 0, "max_chebyshev_dist singleton");
    double pi = std::acos(-1.0);
    check_close(geo::PI<double>, pi, 1e-15, "PI constant");
    check_close(geo::deg_to_rad<double>(180), pi, 1e-12, "deg_to_rad");
    check_close(geo::rad_to_deg<double>(pi / 2), 90.0, 1e-12, "rad_to_deg");
    check_close(geo::normalize_angle<double>(-pi / 2), 1.5 * pi, 1e-12, "normalize_angle negative");
    check_close(geo::normalize_angle<double>(5 * pi), pi, 1e-12, "normalize_angle large");
    check_close(geo::angle_diff<double>(0.1, 2 * pi - 0.1), 0.2, 1e-12, "angle_diff wrap");
    check_point_close(geo::unit_vector<double>(pi / 2), Point<double>(0, 1), 1e-12, "unit_vector");
    check_point_close(geo::polar<double>(2, pi), Point<double>(-2, 0), 1e-12, "polar");
    check_point_close(geo::rotate<double>(P(1, 0), pi / 2), Point<double>(0, 1), 1e-12, "rotate origin");
    check_point_close(geo::rotate<double>(P(2, 1), pi, P(1, 1)), Point<double>(0, 1), 1e-12, "rotate center");
    check_close(geo::angle<double>(P(1, 0), P(0, 0), P(0, 1)), pi / 2, 1e-12, "angle right");
    check_close(geo::angle<double>(P(-1, 0), P(0, 0), P(1, 0)), pi, 1e-12, "angle straight");
}

void test_orientation_and_arg() {
    using P = Point<long long>;
    P a(0, 0), b(2, 0);
    check_eq(geo::orient(a, b, P(1, 1)), 1, "orient ccw");
    check_eq(geo::orient(a, b, P(1, -1)), -1, "orient cw");
    check_eq(geo::orient(a, b, P(1, 0)), 0, "orient collinear");
    check(geo::is_collinear(a, b, P(5, 0)), "is_collinear 3 points true");
    check(!geo::is_collinear(a, b, P(5, 1)), "is_collinear 3 points false");
    check(geo::is_collinear(std::vector<P>{P(0, 0), P(1, 1), P(2, 2), P(1, 1)}), "is_collinear set true");
    check(geo::is_collinear(std::vector<P>{}), "is_collinear empty true");
    check(geo::is_collinear(std::vector<P>{P(1, 1), P(1, 1), P(1, 1)}), "is_collinear all same true");
    check(!geo::is_collinear(std::vector<P>{P(0, 0), P(1, 1), P(2, 3)}), "is_collinear set false");
    check_eq(geo::ccw(a, b, P(1, 1)), geo::COUNTER_CLOCKWISE, "ccw counter");
    check_eq(geo::ccw(a, b, P(1, -1)), geo::CLOCKWISE, "ccw clock");
    check_eq(geo::ccw(a, b, P(-1, 0)), geo::ONLINE_BACK, "ccw back");
    check_eq(geo::ccw(a, b, P(3, 0)), geo::ONLINE_FRONT, "ccw front");
    check_eq(geo::ccw(a, b, P(1, 0)), geo::ON_SEGMENT, "ccw on");
    std::vector<P> v = {P(0, -1), P(-1, 0), P(1, 0), P(0, 1), P(1, 1), P(2, 0)};
    geo::sort_by_arg(v);
    std::vector<P> expected = {P(1, 0), P(2, 0), P(1, 1), P(0, 1), P(-1, 0), P(0, -1)};
    check_eq(v, expected, "sort_by_arg order");
    auto w = geo::arg_sort(std::vector<P>{P(0, -2), P(0, 2)});
    check_eq(w, std::vector<P>({P(0, 2), P(0, -2)}), "arg_sort copy");
    auto wb = geo::arg_sort(std::vector<P>{P(1, 0), P(0, 1), P(-1, 0), P(0, -1)}, P(0, 1));
    check_eq(wb, std::vector<P>({P(0, 1), P(-1, 0), P(0, -1), P(1, 0)}), "arg_sort base");
}

void test_line_segment() {
    using P = Point<long long>;
    using S = Segment<long long>;
    using L = Line<long long>;
    L lx(P(0, 0), P(4, 0)), ly(P(0, 0), P(0, 5));
    std::stringstream lss;
    lss << lx;
    check_eq(lss.str(), std::string("[0,0]-[4,0]"), "Line ostream");
    std::stringstream sss;
    sss << S(P(1, 2), P(3, 4));
    check_eq(sss.str(), std::string("[1,2]-[3,4]"), "Segment ostream");
    std::stringstream lis("1 2 3 4");
    L li;
    lis >> li;
    check(li.a == P(1, 2) && li.b == P(3, 4), "Line istream");
    std::stringstream sis("5 6 7 8");
    S si;
    sis >> si;
    check(si.a == P(5, 6) && si.b == P(7, 8), "Segment istream");
    check(geo::is_orthogonal(lx, ly), "Line orthogonal");
    check(geo::is_parallel(lx, L(P(1, 1), P(5, 1))), "Line parallel");
    check(geo::on_line(lx, P(10, 0)), "on_line true");
    check(!geo::on_line(lx, P(10, 1)), "on_line false");
    check(geo::on_segment(S(P(0, 0), P(4, 0)), P(2, 0)), "on_segment middle");
    check(geo::on_segment(S(P(0, 0), P(0, 0)), P(0, 0)), "on_segment point");
    check(!geo::on_segment(S(P(0, 0), P(4, 0)), P(5, 0)), "on_segment outside");
    check_eq(geo::side_value(lx, P(2, 3)), 12, "side_value left");
    check_eq(geo::side(lx, P(2, 3)), 1, "side left");
    check_eq(geo::side(lx, P(2, -3)), -1, "side right");
    check_eq(geo::side(lx, P(2, 0)), 0, "side on line");
    auto lc = geo::line_coeff(L(P(0, 2), P(4, 2)));
    check(lc.a == 0 && lc.b == 1 && lc.c == -2, "line_coeff horizontal");
    auto lcv = geo::line_coeff(L(P(3, 1), P(3, 5)));
    check(lcv.a == 1 && lcv.b == 0 && lcv.c == -3, "line_coeff vertical");
    auto lcd = geo::line_coeff(L(P(0, 0), P(2, 2)));
    check(lcd.a == 1 && lcd.b == -1 && lcd.c == 0, "line_coeff diagonal");
    check(geo::same_line(L(P(0, 0), P(2, 2)), L(P(1, 1), P(3, 3))), "same_line true");
    check(!geo::same_line(L(P(0, 0), P(2, 2)), L(P(1, 2), P(3, 4))), "same_line false");
    auto lcdg = geo::line_coeff(L(P(1, 1), P(1, 1)));
    check(lcdg.a == 0 && lcdg.b == 0 && lcdg.c == 0, "line_coeff degenerate");
    auto lf = geo::line_from_coeff<double>(0, 1, -2);
    check(lf.has_value(), "line_from_coeff exists");
    check_close(geo::distance<double>(*lf, Point<double>(0, 2)), 0.0, 1e-12, "line_from_coeff horizontal on");
    check(!geo::line_from_coeff<double>(0, 0, 1).has_value(), "line_from_coeff degenerate none");
    auto pl = geo::parallel_line(lx, P(0, 3));
    check(geo::is_parallel(lx, pl) && geo::on_line(pl, P(2, 3)), "parallel_line");
    auto vl = geo::perpendicular_line(lx, P(2, 3));
    check(geo::is_orthogonal(lx, vl) && geo::on_line(vl, P(2, 10)), "perpendicular_line");
    auto pb = geo::perpendicular_bisector<double>(P(0, 0), P(2, 0));
    check_point_close(pb.a, Point<double>(1, 0), 1e-12, "perpendicular_bisector point");
    check(geo::on_line(pb, Point<double>(1, 5)), "perpendicular_bisector line");
    check_close(geo::signed_distance<double>(lx, P(2, 3)), 3.0, 1e-12, "signed_distance left");
    check_close(geo::signed_distance<double>(lx, P(2, -3)), -3.0, 1e-12, "signed_distance right");
    auto yv = geo::y_at<double>(L(P(0, 0), P(2, 2)), 3);
    check(yv.has_value(), "y_at line has value");
    check_close(*yv, 3.0, 1e-12, "y_at line value");
    check(!geo::y_at<double>(L(P(1, 0), P(1, 3)), 1).has_value(), "y_at vertical none");
    auto yvs = geo::y_at<double>(S(P(0, 0), P(2, 2)), 1);
    check(yvs.has_value(), "y_at segment has value");
    check_close(*yvs, 1.0, 1e-12, "y_at segment value");
    S c1(P(0, 0), P(4, 4)), c2(P(0, 4), P(4, 0));
    check(geo::intersect(c1, c2), "segment intersect cross");
    check(geo::proper_intersect(c1, c2), "segment proper intersect");
    check_point_close(*geo::cross_point<double>(c1, c2), Point<double>(2, 2), 1e-12, "segment cross point");
    S t1(P(0, 0), P(4, 0)), t2(P(4, 0), P(5, 0));
    check(geo::intersect(t1, t2), "segment touch endpoint");
    check(!geo::proper_intersect(t1, t2), "segment touch not proper");
    check_point_close(*geo::cross_point<double>(t1, t2), Point<double>(4, 0), 1e-12, "segment touch point");
    S o1(P(0, 0), P(5, 0)), o2(P(2, 0), P(7, 0));
    check(geo::intersect(o1, o2), "segment overlap intersect");
    auto ov = geo::overlap_segment(o1, o2);
    check(ov && ov->a == P(2, 0) && ov->b == P(5, 0), "overlap_segment interval");
    check(!geo::cross_point<double>(o1, o2), "overlap no unique point");
    check(!geo::intersect(S(P(0, 0), P(1, 0)), S(P(2, 0), P(3, 0))), "segment disjoint collinear");
    check(geo::intersect(S(P(1, 1), P(1, 1)), S(P(0, 1), P(2, 1))), "point segment intersects");
    check(!geo::intersect(S(P(3, 1), P(3, 1)), S(P(0, 1), P(2, 1))), "point segment disjoint");
    check_point_close(*geo::cross_point<double>(S(P(1, 1), P(1, 1)), S(P(0, 1), P(2, 1))), Point<double>(1, 1), 1e-12, "point segment cross point");
    check_point_close(*geo::cross_point<double>(L(P(0, 0), P(2, 2)), L(P(0, 2), P(2, 0))), Point<double>(1, 1), 1e-12, "line cross point");
    check(!geo::cross_point<double>(L(P(0, 0), P(2, 0)), L(P(0, 1), P(2, 1))), "parallel line no cross");
    check_point_close(geo::project<double>(L(P(0, 0), P(4, 0)), P(2, 3)), Point<double>(2, 0), 1e-12, "project");
    check_point_close(geo::closest_point<double>(L(P(0, 0), P(4, 0)), P(2, 3)), Point<double>(2, 0), 1e-12, "closest_point line");
    check_point_close(geo::closest_point<double>(S(P(0, 0), P(4, 0)), P(5, 3)), Point<double>(4, 0), 1e-12, "closest_point segment endpoint");
    check_point_close(geo::closest_point<double>(S(P(0, 0), P(4, 0)), P(2, 3)), Point<double>(2, 0), 1e-12, "closest_point segment middle");
    check_point_close(geo::closest_point<double>(S(P(1, 1), P(1, 1)), P(5, 3)), Point<double>(1, 1), 1e-12, "closest_point segment degenerate");
    check_point_close(geo::reflect<double>(L(P(0, 0), P(4, 0)), P(2, 3)), Point<double>(2, -3), 1e-12, "reflect");
    check_close(geo::distance<double>(L(P(0, 0), P(4, 0)), P(2, 3)), 3.0, 1e-12, "distance line point");
    check_close(geo::distance<double>(S(P(0, 0), P(4, 0)), P(5, 3)), std::sqrt(10.0), 1e-12, "distance segment point endpoint");
    check_close(geo::distance<double>(S(P(0, 0), P(4, 0)), S(P(5, 3), P(5, 4))), std::sqrt(10.0), 1e-12, "distance segment segment");
    check_close(geo::distance<double>(L(P(0, 0), P(4, 0)), L(P(0, 3), P(4, 3))), 3.0, 1e-12, "distance line line parallel");
    check_close(geo::distance<double>(L(P(0, 0), P(4, 0)), S(P(5, 3), P(5, 4))), 3.0, 1e-12, "distance line segment separated");
    check_close(geo::distance<double>(S(P(5, 3), P(5, 4)), L(P(0, 0), P(4, 0))), 3.0, 1e-12, "distance segment line separated");
    check_close(geo::distance<double>(L(P(0, 0), P(4, 0)), S(P(2, -1), P(2, 1))), 0.0, 1e-12, "distance line segment intersect");
}

void test_circle() {
    using P = Point<long long>;
    using S = Segment<long long>;
    using L = Line<long long>;
    using C = Circle<long long>;
    C c(P(0, 0), 5);
    std::stringstream csout;
    csout << c;
    check_eq(csout.str(), std::string("[0,0](r=5)"), "Circle ostream");
    std::stringstream csin("1 2 3");
    C cin_circle;
    csin >> cin_circle;
    check(cin_circle.c == P(1, 2) && cin_circle.r == 3, "Circle istream");
    check_eq(geo::radius2(c), 25, "radius2");
    check_eq(geo::contains(c, P(0, 0)), geo::INSIDE, "circle contains inside");
    check_eq(geo::contains(c, P(3, 4)), geo::ON_EDGE, "circle contains on");
    check_eq(geo::contains(c, P(6, 0)), geo::OUTSIDE, "circle contains outside");
    check_eq(geo::circle_relation(C(P(0, 0), 2), C(P(5, 0), 2)), geo::CIRCLE_SEPARATE, "circle_relation separate");
    check_eq(geo::circle_relation(C(P(0, 0), 2), C(P(4, 0), 2)), geo::CIRCLE_EXTERNAL_TANGENT, "circle external tangent");
    check_eq(geo::circle_relation(C(P(0, 0), 3), C(P(4, 0), 3)), geo::CIRCLE_INTERSECT, "circle intersect relation");
    check_eq(geo::circle_relation(C(P(0, 0), 5), C(P(3, 0), 2)), geo::CIRCLE_INTERNAL_TANGENT, "circle internal tangent");
    check_eq(geo::circle_relation(C(P(0, 0), 5), C(P(1, 0), 1)), geo::CIRCLE_CONTAIN, "circle contain relation");
    check_eq(geo::circle_relation(C(P(0, 0), 5), C(P(0, 0), 5)), geo::CIRCLE_SAME, "circle same relation");
    check_close(geo::area<double>(C(P(0, 0), 2)), 4.0 * std::acos(-1.0), 1e-12, "circle area");
    check_close(geo::circumference<double>(C(P(0, 0), 2)), 4.0 * std::acos(-1.0), 1e-12, "circle circumference");
    check_eq(geo::power(C(P(0, 0), 5), P(3, 4)), 0, "circle power on");
    check_eq(geo::power(C(P(0, 0), 5), P(0, 0)), -25, "circle power inside");
    auto ra = geo::radical_axis<double>(C(P(0, 0), 5), C(P(8, 0), 5));
    check(ra.has_value(), "radical_axis exists");
    check(geo::on_line(*ra, Point<double>(4, 3)) && geo::on_line(*ra, Point<double>(4, -3)), "radical_axis through intersections");
    check(!geo::radical_axis<double>(C(P(0, 0), 5), C(P(0, 0), 3)).has_value(), "radical_axis concentric none");
    check_close(geo::intersection_area<double>(C(P(0, 0), 1), C(P(3, 0), 1)), 0.0, 1e-12, "circle intersection_area separate");
    check_close(geo::intersection_area<double>(C(P(0, 0), 3), C(P(1, 0), 1)), std::acos(-1.0), 1e-12, "circle intersection_area contain");
    check_close(geo::intersection_area<double>(C(P(0, 0), 2), C(P(0, 0), 2)), 4.0 * std::acos(-1.0), 1e-12, "circle intersection_area same");
    check_close(geo::intersection_area<double>(C(P(0, 0), 2), C(P(2, 0), 2)), 2.0 * 4.0 * std::acos(0.5) - 0.5 * std::sqrt(12.0) * 2.0, 1e-10, "circle intersection_area crossing");
    check(geo::intersect(c, L(P(-10, 5), P(10, 5))), "circle line tangent");
    check(!geo::intersect(c, L(P(-10, 6), P(10, 6))), "circle line separate");
    auto cl = geo::cross_points<double>(c, L(P(-10, 0), P(10, 0)));
    check_eq(cl.size(), static_cast<size_t>(2), "circle line cross count");
    std::sort(cl.begin(), cl.end());
    check_point_close(cl[0], Point<double>(-5, 0), 1e-10, "circle line cross left");
    check_point_close(cl[1], Point<double>(5, 0), 1e-10, "circle line cross right");
    auto ct = geo::cross_points<double>(c, L(P(-10, 5), P(10, 5)));
    check_eq(ct.size(), static_cast<size_t>(1), "circle line tangent count");
    check(geo::intersect(c, S(P(-10, 0), P(10, 0))), "circle segment cross");
    check(geo::intersect(S(P(-10, 0), P(10, 0)), c), "segment circle symmetric intersect");
    check(!geo::intersect(c, S(P(-1, 0), P(1, 0))), "circle segment inside no circumference");
    check(geo::intersect_disk(c, S(P(-1, 0), P(1, 0))), "circle disk segment inside");
    auto cs = geo::cross_points<double>(c, S(P(-10, 0), P(0, 0)));
    check_eq(cs.size(), static_cast<size_t>(1), "circle segment one cross count");
    check_point_close(cs[0], Point<double>(-5, 0), 1e-10, "circle segment one cross point");
    auto cc = geo::cross_points<double>(C(P(0, 0), 5), C(P(8, 0), 5));
    check_eq(cc.size(), static_cast<size_t>(2), "circle circle cross count");
    std::sort(cc.begin(), cc.end());
    check_point_close(cc[0], Point<double>(4, -3), 1e-10, "circle circle cross low");
    check_point_close(cc[1], Point<double>(4, 3), 1e-10, "circle circle cross high");
    auto tg = geo::tangent_points<double>(C(P(0, 0), 1), P(2, 0));
    check_eq(tg.size(), static_cast<size_t>(2), "tangent points count");
    for (const auto& p : tg) check_close(p.norm2(), 1.0, 1e-10, "tangent point on circle");
    auto tg_on = geo::tangent_points<double>(C(P(0, 0), 5), P(3, 4));
    check_eq(tg_on.size(), static_cast<size_t>(1), "tangent on circle count");
    auto tg_in = geo::tangent_points<double>(C(P(0, 0), 5), P(0, 0));
    check(tg_in.empty(), "tangent inside empty");
    auto tls = geo::tangent_lines<double>(C(P(0, 0), 1), P(2, 0));
    check_eq(tls.size(), static_cast<size_t>(2), "tangent_lines outside count");
    for (const auto& ln : tls) {
        check_point_close(ln.a, Point<double>(2, 0), 1e-12, "tangent_lines outside through point");
        check_close(geo::distance<double>(ln, Point<double>(0, 0)), 1.0, 1e-10, "tangent_lines outside distance");
    }
    auto tl_on = geo::tangent_lines<double>(C(P(0, 0), 5), P(5, 0));
    check_eq(tl_on.size(), static_cast<size_t>(1), "tangent_lines on circle count");
    check_close(geo::distance<double>(tl_on[0], Point<double>(0, 0)), 5.0, 1e-10, "tangent_lines on circle distance");
    auto tl_in = geo::tangent_lines<double>(C(P(0, 0), 5), P(0, 0));
    check(tl_in.empty(), "tangent_lines inside empty");
    auto ctl = geo::common_tangent_lines<double>(C(P(0, 0), 1), C(P(4, 0), 1));
    check_eq(ctl.size(), static_cast<size_t>(4), "common_tangent_lines separate count");
    for (const auto& ln : ctl) {
        check_close(geo::distance<double>(ln, Point<double>(0, 0)), 1.0, 1e-10, "common_tangent_lines separate distance a");
        check_close(geo::distance<double>(ln, Point<double>(4, 0)), 1.0, 1e-10, "common_tangent_lines separate distance b");
    }
    check_eq(geo::tangent_lines<double>(C(P(0, 0), 1), C(P(4, 0), 1)).size(), static_cast<size_t>(4), "tangent_lines circle alias count");
    check_eq(geo::common_tangent_lines<double>(C(P(0, 0), 1), C(P(2, 0), 1)).size(), static_cast<size_t>(3), "common_tangent_lines external touch count");
    check_eq(geo::common_tangent_lines<double>(C(P(0, 0), 3), C(P(2, 0), 1)).size(), static_cast<size_t>(1), "common_tangent_lines internal touch count");
    check(geo::common_tangent_lines<double>(C(P(0, 0), 5), C(P(1, 0), 1)).empty(), "common_tangent_lines contained empty");
    check(geo::common_tangent_lines<double>(C(P(0, 0), 5), C(P(0, 0), 5)).empty(), "common_tangent_lines same circle empty");
    check(geo::common_tangent_lines<double>(C(P(0, 0), 5), C(P(0, 0), 3)).empty(), "common_tangent_lines concentric empty");
    auto co = geo::circumcenter<double>(P(0, 0), P(4, 0), P(0, 3));
    check(co.has_value(), "circumcenter exists");
    check_point_close(*co, Point<double>(2, 1.5), 1e-12, "circumcenter right triangle");
    check(!geo::circumcenter<double>(P(0, 0), P(1, 1), P(2, 2)).has_value(), "circumcenter collinear none");
    auto ccir = geo::circumcircle<double>(P(0, 0), P(4, 0), P(0, 3));
    check(ccir.has_value(), "circumcircle exists");
    check_point_close(ccir->c, Point<double>(2, 1.5), 1e-12, "circumcircle center");
    check_close(ccir->r, 2.5, 1e-12, "circumcircle radius");
    auto ic = geo::incenter<double>(P(0, 0), P(4, 0), P(0, 3));
    check(ic.has_value(), "incenter exists");
    check_point_close(*ic, Point<double>(1, 1), 1e-12, "incenter right triangle");
    check(!geo::incenter<double>(P(0, 0), P(1, 1), P(2, 2)).has_value(), "incenter collinear none");
    auto inc = geo::incircle<double>(P(0, 0), P(4, 0), P(0, 3));
    check(inc.has_value(), "incircle exists");
    check_point_close(inc->c, Point<double>(1, 1), 1e-12, "incircle center");
    check_close(inc->r, 1.0, 1e-12, "incircle radius");
    check_point_close(geo::triangle_centroid<double>(P(0, 0), P(4, 0), P(0, 3)), Point<double>(4.0 / 3.0, 1.0), 1e-12, "triangle_centroid");
    auto oh = geo::orthocenter<double>(P(0, 0), P(4, 0), P(0, 3));
    check(oh.has_value(), "orthocenter exists");
    check_point_close(*oh, Point<double>(0, 0), 1e-12, "orthocenter right triangle");
    check(!geo::orthocenter<double>(P(0, 0), P(1, 1), P(2, 2)).has_value(), "orthocenter collinear none");
}

void test_polygon() {
    using P = Point<long long>;
    Polygon<long long> rect = {P(0, 0), P(4, 0), P(4, 3), P(0, 3)};
    check_eq(geo::signed_area2(rect), 24, "polygon signed_area2");
    check_eq(geo::area2(rect), 24, "polygon area2");
    check_close(geo::area<double>(rect), 12.0, 1e-12, "polygon area");
    check_close(geo::perimeter<double>(rect), 14.0, 1e-12, "polygon perimeter");
    check_eq(geo::signed_area2(P(0, 0), P(4, 0), P(0, 3)), 12, "triangle signed_area2");
    check_eq(geo::area2(P(0, 0), P(4, 0), P(0, 3)), 12, "triangle area2");
    check_close(geo::area<double>(P(0, 0), P(4, 0), P(0, 3)), 6.0, 1e-12, "triangle area");
    check_eq(geo::contains(rect, P(2, 2)), geo::INSIDE, "polygon contains inside");
    check_eq(geo::contains(rect, P(4, 2)), geo::ON_EDGE, "polygon contains edge");
    check_eq(geo::contains(rect, P(5, 2)), geo::OUTSIDE, "polygon contains outside");
    Polygon<long long> concave = {P(0, 0), P(4, 0), P(2, 2), P(4, 4), P(0, 4)};
    check_eq(geo::contains(concave, P(1, 2)), geo::INSIDE, "concave contains inside");
    check_eq(geo::contains(concave, P(3, 2)), geo::OUTSIDE, "concave contains dent outside");
    check(geo::is_convex(rect), "is_convex rectangle");
    check(!geo::is_convex(concave), "is_convex concave false");
    Polygon<long long> colv = {P(0, 0), P(1, 0), P(2, 0), P(2, 1), P(0, 1)};
    check(geo::is_convex(colv, false), "is_convex non-strict collinear");
    check(!geo::is_convex(colv, true), "is_convex strict collinear false");
    std::vector<P> ps = {P(0, 0), P(1, 1), P(0, 2), P(2, 0), P(2, 2), P(1, 0), P(1, 2), P(0, 0)};
    auto hull = geo::convex_hull(ps);
    check_eq(hull, Polygon<long long>({P(0, 0), P(2, 0), P(2, 2), P(0, 2)}), "convex_hull strict");
    auto hull_col = geo::convex_hull(ps, true);
    check_eq(hull_col.size(), static_cast<size_t>(6), "convex_hull keep_collinear count");
    check_eq(geo::diameter2(ps), 8, "diameter2 point set");
    check_eq(geo::diameter2(std::vector<P>{}), 0, "diameter2 empty");
    check_eq(geo::diameter2(std::vector<P>{P(1, 2)}), 0, "diameter2 singleton");
    Polygon<long long> compressed = geo::compress_collinear(Polygon<long long>{P(0, 0), P(1, 0), P(4, 0), P(4, 3), P(0, 3), P(0, 0)});
    check_eq(compressed, rect, "compress_collinear polygon");
    auto deg_line = geo::compress_collinear(Polygon<long long>{P(0, 0), P(1, 0), P(3, 0), P(2, 0)});
    check_eq(deg_line, Polygon<long long>({P(0, 0), P(3, 0)}), "compress_collinear degenerate segment");
    auto deg_empty = geo::compress_collinear(Polygon<long long>{P(0, 0), P(1, 0), P(2, 0)}, false);
    check(deg_empty.empty(), "compress_collinear drop degenerate");
    check(geo::compress_collinear(Polygon<long long>{}).empty(), "compress_collinear empty");
    check(geo::is_simple_polygon(rect), "is_simple_polygon rectangle");
    check(!geo::is_simple_polygon(Polygon<long long>{P(0, 0), P(2, 2), P(0, 2), P(2, 0)}), "is_simple_polygon bowtie false");
    check(!geo::is_simple_polygon(Polygon<long long>{P(0, 0), P(2, 0), P(1, 0), P(0, 1)}), "is_simple_polygon overlapping adjacent false");
    check(!geo::is_simple_polygon(Polygon<long long>{P(0, 0), P(1, 0)}), "is_simple_polygon too small false");
    check(!geo::is_simple_polygon(Polygon<long long>{P(0, 0), P(1, 0), P(0, 1), P(0, 0)}), "is_simple_polygon closed duplicate false");
    check_eq(geo::convex_contains(hull, P(1, 1)), geo::INSIDE, "convex_contains inside");
    check_eq(geo::convex_contains(hull, P(2, 1)), geo::ON_EDGE, "convex_contains edge");
    check_eq(geo::convex_contains(hull, P(3, 1)), geo::OUTSIDE, "convex_contains outside");
    check_eq(geo::support_index(rect, P(1, 1)), 2, "support_index diag");
    check_eq(geo::support_value(rect, P(1, 1)), 7, "support_value diag");
    check_eq(geo::support_point(rect, P(-1, 0)), P(0, 0), "support_point tie first");
    check_eq(geo::support_index(std::vector<P>{}, P(1, 0)), -1, "support_index empty");
    check_eq(geo::support_value(std::vector<P>{}, P(1, 0)), 0, "support_value empty");
    check_eq(geo::support_point(std::vector<P>{}, P(1, 0)), P(0, 0), "support_point empty");
    check_close(geo::width<double>(rect, P(1, 0)), 4.0, 1e-12, "width x");
    check_close(geo::width<double>(rect, P(1, 1)), 7.0 / std::sqrt(2.0), 1e-12, "width diagonal");
    check_close(geo::width<double>(std::vector<P>{}, P(1, 0)), 0.0, 1e-12, "width empty");
    check_close(geo::width<double>(rect, P(0, 0)), 0.0, 1e-12, "width zero dir");
    check_eq(geo::convex_diameter2(hull), 8, "convex_diameter2 square");
    auto pair = geo::convex_diameter_pair(hull);
    check(hull[pair.first].dist2(hull[pair.second]) == 8, "convex_diameter_pair square");
    check_eq(geo::closest_pair2(std::vector<P>{P(0, 0), P(5, 0), P(2, 2), P(1, 1)}), 2, "closest_pair2 simple");
    check_eq(geo::closest_pair2(std::vector<P>{P(0, 0), P(0, 0), P(5, 5)}), 0, "closest_pair2 duplicate");
    check_eq(geo::lattice_points_on_segment(Segment<long long>(P(0, 0), P(6, 4))), 3, "lattice segment");
    check_eq(geo::boundary_lattice_points(rect), 14, "boundary lattice rectangle");
    check_eq(geo::interior_lattice_points(rect), 6, "interior lattice rectangle");
    check_eq(geo::lattice_points(rect), 20, "lattice total rectangle");
    check_eq(geo::bottom_left_index(Polygon<long long>{P(1, 1), P(0, 2), P(2, 0), P(-1, 0)}), 3, "bottom_left_index");
    check_point_close(geo::vertex_mean<double>(rect), Point<double>(2, 1.5), 1e-12, "vertex_mean");
    check_point_close(geo::vertex_mean<double>(std::vector<P>{}), Point<double>(0, 0), 1e-12, "vertex_mean empty");
    check_point_close(geo::polygon_centroid<double>(rect), Point<double>(2, 1.5), 1e-12, "polygon_centroid rectangle");
    check_point_close(geo::polygon_centroid<double>(Polygon<long long>{P(0, 0), P(4, 0), P(0, 3)}), Point<double>(4.0 / 3.0, 1.0), 1e-12, "polygon_centroid triangle");
    check_point_close(geo::polygon_centroid<double>(Polygon<long long>{P(0, 3), P(4, 3), P(4, 0), P(0, 0)}), Point<double>(2, 1.5), 1e-12, "polygon_centroid reversed");
    check_point_close(geo::polygon_centroid<double>(Polygon<long long>{P(0, 0), P(2, 0), P(4, 0)}), Point<double>(2, 0), 1e-12, "polygon_centroid degenerate fallback");
    check_point_close(geo::polygon_centroid<double>(Polygon<long long>{}), Point<double>(0, 0), 1e-12, "polygon_centroid empty");
    auto cut_empty_input = geo::convex_cut<double>(Polygon<long long>{}, Line<long long>(P(0, 0), P(1, 0)));
    check(cut_empty_input.empty(), "convex_cut empty input");
    auto cut = geo::convex_cut<double>(rect, Line<long long>(P(2, -1), P(2, 5)));
    check_eq(cut.size(), static_cast<size_t>(4), "convex_cut count");
    check_close(geo::area<double>(cut), 6.0, 1e-12, "convex_cut area");
    for (const auto& cp : cut) check(cp.x <= 2.0 + 1e-12, "convex_cut left side");
    auto cut_all = geo::convex_cut<double>(rect, Line<long long>(P(0, -1), P(0, 5)));
    check_close(geo::area<double>(cut_all), 0.0, 1e-12, "convex_cut boundary halfplane");
    auto cut_none = geo::convex_cut<double>(rect, Line<long long>(P(-1, -1), P(-1, 5)));
    check(cut_none.empty(), "convex_cut empty");
    auto cut_degenerate_line = geo::convex_cut<double>(rect, Line<long long>(P(0, 0), P(0, 0)));
    check(cut_degenerate_line.empty(), "convex_cut degenerate line");
    Polygon<long long> rect2 = {P(2, 1), P(6, 1), P(6, 5), P(2, 5)};
    auto ci = geo::convex_intersection<double>(rect, rect2);
    check_close(geo::area<double>(ci), 4.0, 1e-10, "convex_intersection area");
    check_close(geo::convex_intersection_area<double>(rect, rect2), 4.0, 1e-10, "convex_intersection_area");
    auto ci_none = geo::convex_intersection<double>(rect, Polygon<long long>{P(10, 10), P(11, 10), P(11, 11), P(10, 11)});
    check(ci_none.empty(), "convex_intersection empty");
    std::vector<Line<long long>> hs = {Line<long long>(P(0, 1), P(0, 0)), Line<long long>(P(2, 0), P(2, 1)), Line<long long>(P(0, 0), P(1, 0)), Line<long long>(P(1, 3), P(0, 3))};
    auto hp = geo::half_plane_intersection<double>(hs, 10.0);
    check_close(geo::area<double>(hp), 6.0, 1e-10, "half_plane_intersection bounded area");
    auto hp_default = geo::half_plane_intersection<double>(std::vector<Line<long long>>{Line<long long>(P(0, 0), P(1, 0))});
    check(!hp_default.empty(), "half_plane_intersection default non-empty");
    std::vector<Line<long long>> hs_empty = {Line<long long>(P(0, 0), P(1, 0)), Line<long long>(P(1, -1), P(0, -1))};
    check(geo::half_plane_intersection<double>(hs_empty, 10.0).empty(), "half_plane_intersection empty");
    check(geo::intersect(rect, Segment<long long>(P(-1, 1), P(5, 1))), "polygon segment intersect crossing");
    check(geo::intersect(Segment<long long>(P(1, 1), P(2, 1)), rect), "segment polygon intersect inside");
    check(!geo::intersect(rect, Segment<long long>(P(5, 5), P(6, 5))), "polygon segment intersect false");
    check(geo::intersect(rect, rect2), "polygon polygon intersect true");
    check(!geo::intersect(rect, Polygon<long long>{P(10, 10), P(11, 10), P(11, 11), P(10, 11)}), "polygon polygon intersect false");
    check_close(geo::distance<double>(rect, P(2, 2)), 0.0, 1e-12, "distance polygon point inside");
    check_close(geo::distance<double>(rect, P(6, 1)), 2.0, 1e-12, "distance polygon point outside");
    check_close(geo::distance<double>(P(6, 1), rect), 2.0, 1e-12, "distance point polygon outside");
    check_close(geo::distance<double>(rect, Segment<long long>(P(6, 1), P(6, 2))), 2.0, 1e-12, "distance polygon segment outside");
    check_close(geo::distance<double>(Segment<long long>(P(6, 1), P(6, 2)), rect), 2.0, 1e-12, "distance segment polygon outside");
    check_close(geo::distance<double>(rect, rect2), 0.0, 1e-12, "distance polygon polygon intersect");
    check_close(geo::distance<double>(rect, Polygon<long long>{P(6, 0), P(7, 0), P(7, 1), P(6, 1)}), 2.0, 1e-12, "distance polygon polygon outside");
    auto far = geo::furthest_point<double>(rect, P(1, 1));
    check_eq(far.first, P(4, 3), "furthest_point point");
    check_close(far.second, std::sqrt(13.0), 1e-12, "furthest_point distance");
    Polygon<long long> shifted = {P(4, 3), P(0, 3), P(0, 0), P(4, 0)};
    check_eq(geo::normalized_polygon(shifted), rect, "normalized_polygon");
    auto ms = geo::minkowski_sum_convex(Polygon<long long>{P(0, 0), P(1, 0), P(0, 1)}, Polygon<long long>{P(0, 0), P(2, 0), P(0, 2)});
    check_eq(geo::area2(ms), 9, "minkowski_sum_convex area2");
    check_close(geo::convex_diameter<double>(hull), std::sqrt(8.0), 1e-12, "convex_diameter");
    check_close(geo::diameter<double>(ps), std::sqrt(8.0), 1e-12, "diameter");
    check_close(geo::closest_pair_dist<double>(std::vector<P>{P(0, 0), P(5, 0), P(2, 2), P(1, 1)}), std::sqrt(2.0), 1e-12, "closest_pair_dist");
    auto mec_empty = geo::minimum_enclosing_circle<double>(std::vector<P>{});
    check(!mec_empty.has_value(), "minimum_enclosing_circle empty");
    auto mec_one = geo::minimum_enclosing_circle<double>(std::vector<P>{P(3, 4)});
    check(mec_one.has_value(), "minimum_enclosing_circle one exists");
    check_point_close(mec_one->c, Point<double>(3, 4), 1e-12, "minimum_enclosing_circle one center");
    check_close(mec_one->r, 0.0, 1e-12, "minimum_enclosing_circle one radius");
    auto mec_two = geo::minimum_enclosing_circle<double>(std::vector<P>{P(0, 0), P(2, 0)});
    check(mec_two.has_value(), "minimum_enclosing_circle two exists");
    check_point_close(mec_two->c, Point<double>(1, 0), 1e-12, "minimum_enclosing_circle two center");
    check_close(mec_two->r, 1.0, 1e-12, "minimum_enclosing_circle two radius");
    auto mec_tri = geo::minimum_enclosing_circle<double>(std::vector<P>{P(0, 0), P(4, 0), P(0, 3)});
    check(mec_tri.has_value(), "minimum_enclosing_circle triangle exists");
    check_point_close(mec_tri->c, Point<double>(2, 1.5), 1e-10, "minimum_enclosing_circle triangle center");
    check_close(mec_tri->r, 2.5, 1e-10, "minimum_enclosing_circle triangle radius");
}


void test_symmetry_manhattan_incremental() {
    using P = Point<long long>;
    using S = Segment<long long>;
    check(geo::has_reflection_symmetry(std::vector<P>{P(0, 0), P(2, 0), P(2, 2), P(0, 2)}), "symmetry square");
    check(geo::has_reflection_symmetry(std::vector<P>{P(-1, 0), P(1, 0)}), "symmetry two points");
    geo::SymmetryOption no_line;
    no_line.allow_line = false;
    check(geo::has_reflection_symmetry(std::vector<P>{P(-1, 0), P(1, 0)}, no_line), "symmetry two points no line via perpendicular");
    check(!geo::has_reflection_symmetry(std::vector<P>{P(0, 0), P(2, 0), P(1, 1), P(2, 3)}), "symmetry asymmetric false");
    std::vector<S> segs = {S(P(0, 0), P(4, 0)), S(P(2, -1), P(2, 1)), S(P(5, -1), P(5, 1)), S(P(0, 0), P(0, 3))};
    check_eq(geo::count_axis_aligned_intersections(segs), 2LL, "manhattan intersections simple");
    geo::IncrementalConvexHull<long long> ich;
    check(ich.empty(), "incremental empty");
    for (auto p : std::vector<P>{P(0, 0), P(2, 0), P(2, 2), P(0, 2), P(1, 1)}) ich.add(p);
    check_eq(ich.size(), 4, "incremental size");
    check_eq(ich.area2(), 8, "incremental area2");
    check_eq(ich.side(P(1, 1)), 1, "incremental side inside");
    check_eq(ich.side(P(2, 1)), 0, "incremental side edge");
    check_eq(ich.side(P(3, 1)), -1, "incremental side outside");
    auto [ok, tm, hit] = geo::point_of_impact(Point<double>(0, 0), 1.0, Point<double>(10, 0), Point<double>(-1, 0));
    check(ok, "point_of_impact catchable ok");
    check_close(tm, 5.0, 1e-12, "point_of_impact catchable time");
    check_point_close(hit, Point<double>(5, 0), 1e-12, "point_of_impact catchable point");
    auto [ng, ngt, ngh] = geo::point_of_impact(Point<double>(0, 0), 1.0, Point<double>(10, 0), Point<double>(1, 0));
    (void)ngt;
    (void)ngh;
    check(!ng, "point_of_impact impossible");
    auto [ok0, t0, h0] = geo::point_of_impact(Point<double>(0, 0), 1.0, Point<double>(0, 0), Point<double>(100, 100));
    check(ok0, "point_of_impact initial ok");
    check_close(t0, 0.0, 1e-12, "point_of_impact initial time");
    check_point_close(h0, Point<double>(0, 0), 1e-12, "point_of_impact initial point");
}

void test_randomized() {
    using P = Point<long long>;
    using S = Segment<long long>;
    std::mt19937_64 rng(123456789);

    for (int tc = 0; tc < 200; ++tc) {
        int n = 2 + static_cast<int>(rng() % 35);
        std::vector<P> ps;
        for (int i = 0; i < n; ++i) ps.emplace_back(static_cast<long long>(rng() % 41) - 20, static_cast<long long>(rng() % 41) - 20);
        auto got = geo::closest_pair2(ps);
        long long naive = std::numeric_limits<long long>::max();
        for (int i = 0; i < n; ++i) for (int j = i + 1; j < n; ++j) naive = std::min(naive, static_cast<long long>(ps[i].dist2(ps[j])));
        check(got == naive, "random closest_pair2");
        auto dia = geo::diameter2(ps);
        long long naive_dia = 0;
        for (int i = 0; i < n; ++i) for (int j = i; j < n; ++j) naive_dia = std::max(naive_dia, static_cast<long long>(ps[i].dist2(ps[j])));
        check(dia == naive_dia, "random diameter2");
        check_close(geo::diameter<double>(ps), std::sqrt(static_cast<double>(naive_dia)), 1e-12, "random diameter");
        check_close(geo::closest_pair_dist<double>(ps), std::sqrt(static_cast<double>(naive)), 1e-12, "random closest_pair_dist");
        auto mec = geo::minimum_enclosing_circle<double>(ps);
        check(mec.has_value(), "random minimum_enclosing_circle exists");
        for (const auto& p : ps) check(mec->c.dist(p.to_real<double>()) <= mec->r + 1e-9, "random minimum_enclosing_circle contains");
        long long naive_l1 = 0;
        long long naive_linf = 0;
        for (int i = 0; i < n; ++i) {
            for (int j = i; j < n; ++j) {
                naive_l1 = std::max(naive_l1, static_cast<long long>(geo::manhattan_dist(ps[i], ps[j])));
                naive_linf = std::max(naive_linf, static_cast<long long>(geo::chebyshev_dist(ps[i], ps[j])));
            }
        }
        check(geo::max_manhattan_dist(ps) == naive_l1, "random max_manhattan_dist");
        check(geo::max_chebyshev_dist(ps) == naive_linf, "random max_chebyshev_dist");
    }

    for (int tc = 0; tc < 200; ++tc) {
        int n = 1 + static_cast<int>(rng() % 40);
        std::vector<P> ps;
        for (int i = 0; i < n; ++i) ps.emplace_back(static_cast<long long>(rng() % 101) - 50, static_cast<long long>(rng() % 101) - 50);
        auto h = geo::convex_hull(ps);
        auto got = geo::convex_diameter2(h);
        long long naive = 0;
        for (int i = 0; i < static_cast<int>(h.size()); ++i) for (int j = i; j < static_cast<int>(h.size()); ++j) naive = std::max(naive, static_cast<long long>(h[i].dist2(h[j])));
        check(got == naive, "random convex_diameter2");
        check_close(geo::convex_diameter<double>(h), std::sqrt(static_cast<double>(naive)), 1e-12, "random convex_diameter");
        if (!h.empty()) check_close(geo::convex_intersection_area<double>(h, h), geo::area<double>(h), 1e-8, "random convex_intersection self area");
        for (const auto& p : ps) check(geo::convex_contains(h, p) != geo::OUTSIDE, "random hull contains all points");
    }

    for (int tc = 0; tc < 100; ++tc) {
        long long ax1 = static_cast<long long>(rng() % 31) - 15;
        long long ax2 = ax1 + 1 + static_cast<long long>(rng() % 10);
        long long ay1 = static_cast<long long>(rng() % 31) - 15;
        long long ay2 = ay1 + 1 + static_cast<long long>(rng() % 10);
        long long bx1 = static_cast<long long>(rng() % 31) - 15;
        long long bx2 = bx1 + 1 + static_cast<long long>(rng() % 10);
        long long by1 = static_cast<long long>(rng() % 31) - 15;
        long long by2 = by1 + 1 + static_cast<long long>(rng() % 10);
        Polygon<long long> a = {P(ax1, ay1), P(ax2, ay1), P(ax2, ay2), P(ax1, ay2)};
        Polygon<long long> b = {P(bx1, by1), P(bx2, by1), P(bx2, by2), P(bx1, by2)};
        long long wx = std::max(0LL, std::min(ax2, bx2) - std::max(ax1, bx1));
        long long wy = std::max(0LL, std::min(ay2, by2) - std::max(ay1, by1));
        check_close(geo::convex_intersection_area<double>(a, b), static_cast<double>(wx * wy), 1e-8, "random rectangle convex_intersection_area");
        bool touch_or_overlap = std::max(ax1, bx1) <= std::min(ax2, bx2) && std::max(ay1, by1) <= std::min(ay2, by2);
        check(geo::intersect(a, b) == touch_or_overlap, "random rectangle polygon intersect");
    }

    for (int tc = 0; tc < 200; ++tc) {
        int n = 1 + static_cast<int>(rng() % 35);
        std::vector<S> ss;
        for (int i = 0; i < n; ++i) {
            long long x1 = static_cast<long long>(rng() % 31) - 15;
            long long y1 = static_cast<long long>(rng() % 31) - 15;
            if (rng() & 1ULL) {
                long long x2 = static_cast<long long>(rng() % 31) - 15;
                ss.emplace_back(P(x1, y1), P(x2, y1));
            } else {
                long long y2 = static_cast<long long>(rng() % 31) - 15;
                ss.emplace_back(P(x1, y1), P(x1, y2));
            }
        }
        check_eq(geo::count_axis_aligned_intersections(ss), naive_manhattan(ss), "random manhattan intersections");
    }

    for (int tc = 0; tc < 100; ++tc) {
        geo::IncrementalConvexHull<long long> ich;
        std::vector<P> ps;
        int n = 1 + static_cast<int>(rng() % 30);
        for (int i = 0; i < n; ++i) {
            P p(static_cast<long long>(rng() % 41) - 20, static_cast<long long>(rng() % 41) - 20);
            ps.push_back(p);
            ich.add(p);
            auto h = geo::convex_hull(ps);
            check_eq(ich.area2(), geo::area2(h), "random incremental area2");
            check_eq(ich.size(), static_cast<int>(h.size()), "random incremental size");
        }
    }
}

void run_all() {
    test_point();
    std::cout << "[PASS] Point / Vector\n";
    test_orientation_and_arg();
    std::cout << "[PASS] orient / ccw / arg_sort\n";
    test_line_segment();
    std::cout << "[PASS] Line / Segment / distance / cross_point\n";
    test_circle();
    std::cout << "[PASS] Circle / tangents / triangle centers\n";
    test_polygon();
    std::cout << "[PASS] Polygon / convex_hull / lattice / cut / added utilities\n";
    test_symmetry_manhattan_incremental();
    std::cout << "[PASS] symmetry / manhattan / incremental hull\n";
    test_randomized();
    std::cout << "[PASS] randomized consistency tests\n";
    std::cout << "total checks: " << checks << '\n';
}

}  // namespace geo_test

int main() {
    auto st = std::chrono::steady_clock::now();
    geo_test::run_all();
    auto ed = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(ed - st).count();
    std::cout << "elapsed_ms: " << ms << '\n';
    return 0;
}
#endif
