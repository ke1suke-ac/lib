#pragma once
#include <bits/stdc++.h>

// 浮動小数点座標を主対象にした競技プログラミング用2次元幾何ライブラリ
// 判定は型ごとの固定 EPS を使い、EPS を引数で渡す複雑さを避ける
// デフォルト座標型は double、計算型や返り値型は座標型 T に揃える
// Point の operator== と operator< は STL で扱いやすいよう exact 比較とし、幾何的な同一点判定には same_point を使う
// 格子点数や Pick の定理など、整数座標でのみ意味を持つ機能は含めない

namespace fgeo {

template<class T>
inline constexpr T EPS = T(1e-10);

template<>
inline constexpr float EPS<float> = 1e-5f;

template<>
inline constexpr double EPS<double> = 1e-10;

template<>
inline constexpr long double EPS<long double> = 1e-12L;

template<class T>
inline constexpr T PI = T(3.141592653589793238462643383279502884L);

namespace internal {

template<class T>
int sign(T x) {
    if (x > EPS<T>) return 1;
    if (x < -EPS<T>) return -1;
    return 0;
}

template<class T>
bool eq(T a, T b) {
    return std::abs(a - b) <= EPS<T>;
}

template<class T>
bool lt(T a, T b) {
    return a < b - EPS<T>;
}

template<class T>
bool le(T a, T b) {
    return a <= b + EPS<T>;
}

template<class T>
bool gt(T a, T b) {
    return a > b + EPS<T>;
}

template<class T>
bool ge(T a, T b) {
    return a + EPS<T> >= b;
}


}  // namespace internal

// EPS 付き符号を返す O(1)
template<class T>
int sign(T x) {
    return internal::sign(x);
}

// EPS 付き等価判定を行う O(1)
template<class T>
bool eq(T a, T b) {
    return internal::eq(a, b);
}

// EPS 付き小なり判定を行う O(1)
template<class T>
bool lt(T a, T b) {
    return internal::lt(a, b);
}

// EPS 付き小なり等号判定を行う O(1)
template<class T>
bool le(T a, T b) {
    return internal::le(a, b);
}

// EPS 付き大なり判定を行う O(1)
template<class T>
bool gt(T a, T b) {
    return internal::gt(a, b);
}

// EPS 付き大なり等号判定を行う O(1)
template<class T>
bool ge(T a, T b) {
    return internal::ge(a, b);
}

// 度数法をラジアンに変換する O(1)
// 用途: 入力が度で与えられる回転・角度問題で使う
// 使い方: rotate(p, deg_to_rad(90.0)) のように渡す
// 注意: 返り値の型は引数Tに揃える
template<class T>
T deg_to_rad(T deg) {
    return deg * PI<T> / T(180);
}

// ラジアンを度数法に変換する O(1)
// 用途: 出力形式が度指定の問題やデバッグ表示で使う
// 使い方: rad_to_deg(angle(a,b,c)) のように使う
// 注意: 幾何判定では角度に戻さず内積・外積で比較する方が安定しやすい
template<class T>
T rad_to_deg(T rad) {
    return rad * T(180) / PI<T>;
}

// 角度を [0, 2π) に正規化する O(1)
// 用途: 偏角差、回転量、円周上の角度範囲を扱うときに使う
// 使い方: 負の角度や2π以上の角度を標準範囲へ戻す
// 注意: 2πにEPSで一致する値は0に丸める
template<class T>
T normalize_angle(T theta) {
    T two = T(2) * PI<T>;
    theta = std::fmod(theta, two);
    if (theta < T(0)) theta += two;
    if (internal::eq(theta, two)) theta = T(0);
    return theta;
}

// 2つの角度の小さい方の差を [0, π] で返す O(1)
// 用途: 向きの近さ、視野角、円周上の最短角距離を判定したいときに使う
// 使い方: 入力角は任意範囲でよく、内部で [0,2π) に正規化する
// 注意: 返り値は常に非負で、最大でもπになる
template<class T>
T angle_diff(T a, T b) {
    T d = std::abs(normalize_angle(a) - normalize_angle(b));
    T two = T(2) * PI<T>;
    if (d > PI<T>) d = two - d;
    return d;
}

template<class T = double>
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
    constexpr Point operator*(T k) const { return Point(x * k, y * k); }

    // スカラーで割った点を返す O(1)
    constexpr Point operator/(T k) const { return Point(x / k, y / k); }

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

    // 代入スカラー倍を行う O(1)
    constexpr Point& operator*=(T k) {
        x *= k;
        y *= k;
        return *this;
    }

    // 代入スカラー除算を行う O(1)
    constexpr Point& operator/=(T k) {
        x /= k;
        y /= k;
        return *this;
    }

    // exact な辞書順で比較する O(1)
    constexpr bool operator<(const Point& p) const {
        if (x != p.x) return x < p.x;
        return y < p.y;
    }

    // exact な辞書順で比較する O(1)
    constexpr bool operator>(const Point& p) const { return p < *this; }

    // exact に座標が一致するか判定する O(1)
    constexpr bool operator==(const Point& p) const { return x == p.x && y == p.y; }

    // exact に座標が異なるか判定する O(1)
    constexpr bool operator!=(const Point& p) const { return !(*this == p); }

    // 内積を返す O(1)
    constexpr T dot(const Point& p) const { return x * p.x + y * p.y; }

    // 外積を返す O(1)
    constexpr T cross(const Point& p) const { return x * p.y - y * p.x; }

    // 原点からの距離2乗を返す O(1)
    constexpr T norm2() const { return x * x + y * y; }

    // 原点からの距離を返す O(1)
    T norm() const { return std::sqrt(norm2()); }

    // 2点間距離2乗を返す O(1)
    constexpr T dist2(const Point& p) const {
        T dx = x - p.x;
        T dy = y - p.y;
        return dx * dx + dy * dy;
    }

    // 2点間距離を返す O(1)
    T dist(const Point& p) const { return std::sqrt(dist2(p)); }

    // 偏角をラジアンで返す O(1)
    T arg() const { return std::atan2(y, x); }

    // 単位ベクトルを返す O(1)
    Point unit() const {
        T d = norm();
        if (internal::sign(d) == 0) return Point();
        return *this / d;
    }

    // EPS 付きでゼロベクトルか判定する O(1)
    bool is_zero() const { return norm2() <= EPS<T> * EPS<T>; }

    // EPS 付きで平行か判定する O(1)
    bool is_parallel(const Point& p) const { return internal::sign(cross(p)) == 0; }

    // EPS 付きで直交するか判定する O(1)
    bool is_orthogonal(const Point& p) const { return internal::sign(dot(p)) == 0; }

    // 反時計回りに90度回転したベクトルを返す O(1)
    constexpr Point rot90() const { return Point(-y, x); }

    // 時計回りに90度回転したベクトルを返す O(1)
    constexpr Point rot270() const { return Point(y, -x); }

    // 角度thetaだけ反時計回りに回転したベクトルを返す O(1)
    Point rotate(T theta) const {
        T c = std::cos(theta);
        T s = std::sin(theta);
        return Point(x * c - y * s, x * s + y * c);
    }
};

// 左スカラー倍した点を返す O(1)
template<class T>
constexpr Point<T> operator*(T k, const Point<T>& p) {
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

template<class T = double>
using Vector = Point<T>;

// 内積を返す O(1)
template<class T>
constexpr T dot(const Point<T>& a, const Point<T>& b) {
    return a.dot(b);
}

// 外積を返す O(1)
template<class T>
constexpr T cross(const Point<T>& a, const Point<T>& b) {
    return a.cross(b);
}

// 原点からの距離2乗を返す O(1)
template<class T>
constexpr T norm2(const Point<T>& p) {
    return p.norm2();
}

// 原点からの距離を返す O(1)
template<class T>
T norm(const Point<T>& p) {
    return p.norm();
}

// 2点間距離2乗を返す O(1)
template<class T>
constexpr T dist2(const Point<T>& a, const Point<T>& b) {
    return a.dist2(b);
}

// 2点間距離を返す O(1)
template<class T>
T dist(const Point<T>& a, const Point<T>& b) {
    return a.dist(b);
}

// EPS 付きで幾何的に同一点か判定する O(1)
// 用途: 交点計算や円との交点など、丸め誤差を含む点を比較するときに使う
// 使い方: operator== は exact 比較なので、幾何判定では same_point(a, b) を使う
// 注意: std::sort / std::unique 用の比較とは分けて扱う
template<class T>
bool same_point(const Point<T>& a, const Point<T>& b) {
    return a.dist2(b) <= EPS<T> * EPS<T>;
}

// マンハッタン距離を返す O(1)
// 用途: 45度回転後の最大距離や、L1距離の評価に使う
// 使い方: 浮動小数点座標でも abs(dx)+abs(dy) として計算する
// 注意: EPS 判定は行わず、距離値そのものを返す
template<class T>
T manhattan_dist(const Point<T>& a, const Point<T>& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

// チェビシェフ距離を返す O(1)
// 用途: 斜め移動を含めた最大座標差や、L∞距離の評価に使う
// 使い方: max(abs(dx), abs(dy)) を返す
// 注意: EPS 判定は行わず、距離値そのものを返す
template<class T>
T chebyshev_dist(const Point<T>& a, const Point<T>& b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

// 点集合の外接矩形を返す O(N)
// 用途: 探索範囲の枝刈り、図形の大まかな重なり判定、座標圧縮の範囲確認に使う
// 使い方: 空ならnullopt、非空なら{左下, 右上}を返す
// 注意: 境界計算は exact な min/max で行う
template<class T>
std::optional<std::pair<Point<T>, Point<T>>> bounding_box(const std::vector<Point<T>>& ps) {
    if (ps.empty()) return std::nullopt;
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
    return std::make_pair(Point<T>(min_x, min_y), Point<T>(max_x, max_y));
}

// 点集合の最大マンハッタン距離を返す O(N)
// 用途: L1距離の最遠点対の距離だけが必要なときに使う
// 使い方: max(x+y)-min(x+y), max(x-y)-min(x-y) の最大値を返す
// 注意: 空集合・1点集合では0を返す
template<class T>
T max_manhattan_dist(const std::vector<Point<T>>& ps) {
    if (ps.size() <= 1) return T(0);
    T min_s = ps[0].x + ps[0].y, max_s = min_s;
    T min_d = ps[0].x - ps[0].y, max_d = min_d;
    for (const auto& p : ps) {
        T s = p.x + p.y;
        T d = p.x - p.y;
        min_s = std::min(min_s, s);
        max_s = std::max(max_s, s);
        min_d = std::min(min_d, d);
        max_d = std::max(max_d, d);
    }
    return std::max(max_s - min_s, max_d - min_d);
}

// 点集合の最大チェビシェフ距離を返す O(N)
// 用途: L∞距離の最遠点対の距離だけが必要なときに使う
// 使い方: x座標幅とy座標幅の最大値を返す
// 注意: 空集合・1点集合では0を返す
template<class T>
T max_chebyshev_dist(const std::vector<Point<T>>& ps) {
    auto box = bounding_box(ps);
    if (!box) return T(0);
    return std::max(box->second.x - box->first.x, box->second.y - box->first.y);
}

// 点を原点中心にthetaだけ反時計回りに回転する O(1)
// 用途: 座標変換、回転後の位置計算、方向ベクトル生成に使う
// 使い方: thetaはラジアンで指定する
// 注意: 返り値はTで丸められるため、T=floatでは誤差が大きくなりやすい
template<class T>
Point<T> rotate(const Point<T>& p, T theta) {
    return p.rotate(theta);
}

// 点をcenter中心にthetaだけ反時計回りに回転する O(1)
// 用途: 図形全体の回転、円周上の点生成に使う
// 使い方: rotate(p, theta, c) は c + rotate(p-c, theta) を返す
// 注意: thetaはラジアンで指定する
template<class T>
Point<T> rotate(const Point<T>& p, T theta, const Point<T>& center) {
    return center + (p - center).rotate(theta);
}

// 偏角thetaの単位ベクトルを返す O(1)
// 用途: 角度から方向ベクトルを作る、円周上の点を生成するときに使う
// 使い方: unit_vector(theta) は {cos(theta), sin(theta)} を返す
// 注意: thetaはラジアンで指定する
template<class T>
Point<T> unit_vector(T theta) {
    return Point<T>(std::cos(theta), std::sin(theta));
}

// 半径r・偏角thetaの点を返す O(1)
// 用途: 極座標入力、円周上の点生成、方向ベクトルの長さ指定に使う
// 使い方: polar(r, theta) は unit_vector(theta) * r を返す
// 注意: rが負なら反対方向の点になる
template<class T>
Point<T> polar(T r, T theta) {
    return unit_vector(theta) * r;
}

// 角abcをラジアンで返す O(1)
// 用途: 三角形の角度、円弧、角度制約の判定に使う
// 使い方: bを頂点とする0以上pi以下の角を返す
// 注意: a==bまたはc==bに近い場合は0を返す
template<class T>
T angle(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    Point<T> u = a - b;
    Point<T> v = c - b;
    T nu = u.norm();
    T nv = v.norm();
    if (internal::sign(nu) == 0 || internal::sign(nv) == 0) return T(0);
    T cs = u.dot(v) / (nu * nv);
    cs = std::max(T(-1), std::min(T(1), cs));
    return std::acos(cs);
}

enum Ccw : int {
    COUNTER_CLOCKWISE = 1,
    CLOCKWISE = -1,
    ONLINE_BACK = 2,
    ONLINE_FRONT = -2,
    ON_SEGMENT = 0,
};

// 3点の向きをEPS付きで返す O(1)
template<class T>
int orient(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return internal::sign((b - a).cross(c - a));
}

// 3点の詳細な位置関係をEPS付きで返す O(1)
template<class T>
Ccw ccw(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    Point<T> ab = b - a;
    Point<T> ac = c - a;
    int cr = internal::sign(ab.cross(ac));
    if (cr > 0) return COUNTER_CLOCKWISE;
    if (cr < 0) return CLOCKWISE;
    if (internal::sign(ab.dot(ac)) < 0) return ONLINE_BACK;
    if (internal::lt(ab.norm2(), ac.norm2())) return ONLINE_FRONT;
    return ON_SEGMENT;
}

// 3点がEPS付きで同一直線上にあるか判定する O(1)
// 用途: 退化三角形の除外、共線点圧縮、凸包後処理に使う
// 使い方: orient(a,b,c)==0 と同じ意味の読みやすい別名
// 注意: 固定EPSで判定するため、非常に大きい座標ではスケールに注意する
template<class T>
bool is_collinear(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return orient(a, b, c) == 0;
}

// 点集合がEPS付きで同一直線上にあるか判定する O(N)
// 用途: 退化多角形、凸包の全点共線ケースを判定する
// 使い方: 0点・1点・2点はtrueを返す
// 注意: ほぼ同一点は同一点として基準点から飛ばす
template<class T>
bool is_collinear(const std::vector<Point<T>>& ps) {
    if (ps.size() <= 2) return true;
    int i = 1;
    while (i < static_cast<int>(ps.size()) && same_point(ps[0], ps[i])) ++i;
    if (i == static_cast<int>(ps.size())) return true;
    for (int j = i + 1; j < static_cast<int>(ps.size()); ++j) {
        if (!is_collinear(ps[0], ps[i], ps[j])) return false;
    }
    return true;
}

namespace internal {

template<class T>
int arg_half(const Point<T>& p) {
    int sy = sign(p.y);
    int sx = sign(p.x);
    if (sy > 0 || (sy == 0 && sx >= 0)) return 0;
    return 1;
}

}  // namespace internal

// 原点から見た偏角順に比較する O(1)
// 用途: 偏角ソート、極座標順の走査、半直線イベントの処理に使う
// 使い方: std::sort(ps.begin(), ps.end(), arg_less<T>) のように使う
// 注意: ゼロベクトルは先頭、同じ偏角なら原点に近い順にする
template<class T>
bool arg_less(const Point<T>& a, const Point<T>& b) {
    bool za = a.is_zero();
    bool zb = b.is_zero();
    if (za || zb) return za && !zb;
    int ha = internal::arg_half(a);
    int hb = internal::arg_half(b);
    if (ha != hb) return ha < hb;
    int cr = internal::sign(a.cross(b));
    if (cr != 0) return cr > 0;
    if (!internal::eq(a.norm2(), b.norm2())) return a.norm2() < b.norm2();
    return a < b;
}

// 点列を原点から見た偏角順に並べる O(N log N)
template<class T>
void sort_by_arg(std::vector<Point<T>>& ps) {
    std::sort(ps.begin(), ps.end(), arg_less<T>);
}

// 原点から見た偏角順に並べた点列を返す O(N log N)
template<class T>
std::vector<Point<T>> arg_sort(std::vector<Point<T>> ps) {
    sort_by_arg(ps);
    return ps;
}

template<class T = double>
struct Line {
    Point<T> a;
    Point<T> b;

    // 直線を生成する O(1)
    Line() = default;

    // 2点を通る直線を生成する O(1)
    Line(Point<T> a_, Point<T> b_) : a(a_), b(b_) {}

    // 方向ベクトルを返す O(1)
    Point<T> vec() const { return b - a; }

    // 方向ベクトルの長さ2乗を返す O(1)
    T length2() const { return vec().norm2(); }

    // 方向ベクトルの長さを返す O(1)
    T length() const { return vec().norm(); }
};

template<class T = double>
struct Segment {
    Point<T> a;
    Point<T> b;

    // 線分を生成する O(1)
    Segment() = default;

    // 端点を指定して線分を生成する O(1)
    Segment(Point<T> a_, Point<T> b_) : a(a_), b(b_) {}

    // 方向ベクトルを返す O(1)
    Point<T> vec() const { return b - a; }

    // 線分長の2乗を返す O(1)
    T length2() const { return vec().norm2(); }

    // 線分長を返す O(1)
    T length() const { return vec().norm(); }
};

template<class T>
struct LineCoeff {
    T a{};
    T b{};
    T c{};

    // 係数を生成する O(1)
    LineCoeff() = default;

    // ax+by+c=0の係数を生成する O(1)
    LineCoeff(T a_, T b_, T c_) : a(a_), b(b_), c(c_) {}
};

// 点を直線に代入した符号付き値を返す O(1)
// 用途: 点が有向直線の左側か右側かを数値として扱いたいときに使う
// 使い方: 正なら l.a->l.b の左側、負なら右側、0付近なら直線上
// 注意: 値は正規化されていない外積値で、距離ではない
template<class T>
T side_value(const Line<T>& l, const Point<T>& p) {
    return l.vec().cross(p - l.a);
}

// 点が有向直線のどちら側にあるかをEPS付きで返す O(1)
// 用途: 半平面クリップ、凸多角形切断、左右判定に使う
// 使い方: 1=左側、0=直線上、-1=右側
// 注意: 固定EPSで判定する
template<class T>
int side(const Line<T>& l, const Point<T>& p) {
    return internal::sign(side_value(l, p));
}

// EPS付きで直線同士が平行か判定する O(1)
template<class T>
bool is_parallel(const Line<T>& x, const Line<T>& y) {
    return internal::sign(x.vec().cross(y.vec())) == 0;
}

// EPS付きで線分の方向が平行か判定する O(1)
template<class T>
bool is_parallel(const Segment<T>& x, const Segment<T>& y) {
    return internal::sign(x.vec().cross(y.vec())) == 0;
}

// EPS付きで直線同士が直交するか判定する O(1)
template<class T>
bool is_orthogonal(const Line<T>& x, const Line<T>& y) {
    return internal::sign(x.vec().dot(y.vec())) == 0;
}

// EPS付きで線分の方向が直交するか判定する O(1)
template<class T>
bool is_orthogonal(const Segment<T>& x, const Segment<T>& y) {
    return internal::sign(x.vec().dot(y.vec())) == 0;
}

// 点が直線上にあるかEPS付きで判定する O(1)
template<class T>
bool on_line(const Line<T>& l, const Point<T>& p) {
    if (same_point(l.a, l.b)) return same_point(l.a, p);
    return side(l, p) == 0;
}

// 点が線分上にあるかEPS付きで判定する O(1)
template<class T>
bool on_segment(const Segment<T>& s, const Point<T>& p) {
    if (same_point(s.a, s.b)) return same_point(s.a, p);
    if (orient(s.a, s.b, p) != 0) return false;
    return internal::le((s.a - p).dot(s.b - p), T(0));
}

// 直線を正規化された ax+by+c=0 の形で返す O(1)
// 用途: 直線の表示、同一直線判定の補助、点と直線の符号付き距離に使う
// 使い方: sqrt(a^2+b^2)=1、最初の非ゼロ係数が正になるように返す
// 注意: 退化直線では {0,0,0} を返す
template<class T>
LineCoeff<T> line_coeff(const Line<T>& l) {
    Point<T> v = l.vec();
    T a = -v.y;
    T b = v.x;
    T c = -(a * l.a.x + b * l.a.y);
    T z = std::sqrt(a * a + b * b);
    if (internal::sign(z) == 0) return LineCoeff<T>();
    a /= z;
    b /= z;
    c /= z;
    if (internal::sign(a) < 0 || (internal::sign(a) == 0 && internal::sign(b) < 0)) {
        a = -a;
        b = -b;
        c = -c;
    }
    if (internal::sign(a) == 0) a = T(0);
    if (internal::sign(b) == 0) b = T(0);
    if (internal::sign(c) == 0) c = T(0);
    return LineCoeff<T>(a, b, c);
}

// ax+by+c=0 から直線を生成する O(1)
// 用途: 入力が直線係数で与えられる問題をLineに変換するときに使う
// 使い方: a=b=0ならnullopt、それ以外なら直線上の2点からなるLineを返す
// 注意: 返る2点は代表点であり、係数の正規化はline_coeffで別途行う
template<class T>
std::optional<Line<T>> line_from_coeff(T a, T b, T c) {
    if (internal::sign(a) == 0 && internal::sign(b) == 0) return std::nullopt;
    if (std::abs(a) >= std::abs(b)) {
        Point<T> p(-c / a, T(0));
        Point<T> q(-(b + c) / a, T(1));
        return Line<T>(p, q);
    }
    Point<T> p(T(0), -c / b);
    Point<T> q(T(1), -(a + c) / b);
    return Line<T>(p, q);
}

// 2直線がEPS付きで同一直線か判定する O(1)
// 用途: 交点が一意か、重なり線分があり得るかを分岐したいときに使う
// 使い方: 平行かつ片方の端点がもう一方の直線上ならtrue
// 注意: 退化直線同士は、基準点が同一点なら同一直線扱いにする
template<class T>
bool same_line(const Line<T>& x, const Line<T>& y) {
    if (same_point(x.a, x.b)) return on_line(y, x.a);
    if (same_point(y.a, y.b)) return on_line(x, y.a);
    return is_parallel(x, y) && on_line(x, y.a);
}

// 点pを通り直線lに平行な直線を返す O(1)
// 用途: 平行移動した直線、スライド後の境界線を作るときに使う
// 使い方: lが退化している場合はpを始点とする退化直線を返す
// 注意: 距離dだけ離す処理ではなく、指定点を通る平行線を作る
template<class T>
Line<T> parallel_line(const Line<T>& l, const Point<T>& p) {
    return Line<T>(p, p + l.vec());
}

// 点pを通り直線lに垂直な直線を返す O(1)
// 用途: 垂線、最近点計算の補助、直交する境界線を作るときに使う
// 使い方: lの方向ベクトルを90度回転した直線を返す
// 注意: lが退化している場合はpを始点とする退化直線を返す
template<class T>
Line<T> perpendicular_line(const Line<T>& l, const Point<T>& p) {
    return Line<T>(p, p + l.vec().rot90());
}

// 2点の垂直二等分線を返す O(1)
// 用途: 外心、ボロノイ境界、2点から等距離の点集合を扱うときに使う
// 使い方: aとbが同一点に近い場合は中点を始点とする退化直線を返す
// 注意: 返り値はLineで、線分ではない
template<class T>
Line<T> perpendicular_bisector(const Point<T>& a, const Point<T>& b) {
    Point<T> m = (a + b) / T(2);
    return Line<T>(m, m + (b - a).rot90());
}

// 点から有向直線への符号付き距離を返す O(1)
// 用途: 半平面境界からの余裕、左側なら正・右側なら負の距離を使いたいときに使う
// 使い方: 絶対値は distance(l,p) と一致する
// 注意: 退化直線では符号が定義できないため、点l.aからの距離を非負で返す
template<class T>
T signed_distance(const Line<T>& l, const Point<T>& p) {
    T len = l.vec().norm();
    if (internal::sign(len) == 0) return p.dist(l.a);
    return side_value(l, p) / len;
}

// 2直線が交わるか判定する O(1)
template<class T>
bool intersect(const Line<T>& x, const Line<T>& y) {
    return !is_parallel(x, y) || same_line(x, y);
}

// 直線と線分が交わるか判定する O(1)
template<class T>
bool intersect(const Line<T>& l, const Segment<T>& s) {
    if (same_point(s.a, s.b)) return on_line(l, s.a);
    int sa = side(l, s.a);
    int sb = side(l, s.b);
    return sa == 0 || sb == 0 || sa != sb;
}

// 線分と直線が交わるか判定する O(1)
template<class T>
bool intersect(const Segment<T>& s, const Line<T>& l) {
    return intersect(l, s);
}

// 2線分が端点接触・重なりを含めて交わるか判定する O(1)
template<class T>
bool intersect(const Segment<T>& s, const Segment<T>& t) {
    if (same_point(s.a, s.b)) return on_segment(t, s.a);
    if (same_point(t.a, t.b)) return on_segment(s, t.a);
    return ccw(s.a, s.b, t.a) * ccw(s.a, s.b, t.b) <= 0 &&
           ccw(t.a, t.b, s.a) * ccw(t.a, t.b, s.b) <= 0;
}

// 2線分が内部同士で交わるか判定する O(1)
// 用途: 端点接触や重なりを除き、真に交差する辺だけを数えたいときに使う
// 使い方: 自己交差判定では、隣接辺以外に proper_intersect があれば単純でない
// 注意: 共線重なりや端点接触はfalseを返す
template<class T>
bool proper_intersect(const Segment<T>& s, const Segment<T>& t) {
    int a = orient(s.a, s.b, t.a);
    int b = orient(s.a, s.b, t.b);
    int c = orient(t.a, t.b, s.a);
    int d = orient(t.a, t.b, s.b);
    return a * b < 0 && c * d < 0;
}

// 2つの共線線分の重なり区間を返す O(1)
// 用途: 線分交差で、1点交差と区間重なりを区別したいときに使う
// 使い方: 重なりが1点なら退化線分、重なりなしならnulloptを返す
// 注意: 同一直線上でない場合もnulloptを返す
template<class T>
std::optional<Segment<T>> overlap_segment(Segment<T> s, Segment<T> t) {
    if (!same_line(Line<T>(s.a, s.b), Line<T>(t.a, t.b))) return std::nullopt;
    Point<T> dir = s.vec();
    bool by_x = std::abs(dir.x) >= std::abs(dir.y);
    auto less_axis = [&](const Point<T>& p, const Point<T>& q) {
        if (by_x) {
            if (!internal::eq(p.x, q.x)) return p.x < q.x;
            return p.y < q.y;
        }
        if (!internal::eq(p.y, q.y)) return p.y < q.y;
        return p.x < q.x;
    };
    if (less_axis(s.b, s.a)) std::swap(s.a, s.b);
    if (less_axis(t.b, t.a)) std::swap(t.a, t.b);
    Point<T> l = less_axis(s.a, t.a) ? t.a : s.a;
    Point<T> r = less_axis(s.b, t.b) ? s.b : t.b;
    if (less_axis(r, l) && !same_point(l, r)) return std::nullopt;
    return Segment<T>(l, r);
}

// 2直線の一意な交点を返す O(1)
template<class T>
std::optional<Point<T>> cross_point(const Line<T>& x, const Line<T>& y) {
    Point<T> vx = x.vec();
    Point<T> vy = y.vec();
    T d = vx.cross(vy);
    if (internal::sign(d) == 0) return std::nullopt;
    T t = (y.a - x.a).cross(vy) / d;
    return x.a + vx * t;
}

// 直線と線分の一意な交点を返す O(1)
template<class T>
std::optional<Point<T>> cross_point(const Line<T>& l, const Segment<T>& s) {
    if (!intersect(l, s)) return std::nullopt;
    if (same_point(s.a, s.b)) return s.a;
    Line<T> ls(s.a, s.b);
    if (same_line(l, ls)) return std::nullopt;
    return cross_point(l, ls);
}

// 線分と直線の一意な交点を返す O(1)
template<class T>
std::optional<Point<T>> cross_point(const Segment<T>& s, const Line<T>& l) {
    return cross_point(l, s);
}

// 2線分の一意な交点を返す O(1)
template<class T>
std::optional<Point<T>> cross_point(const Segment<T>& s, const Segment<T>& t) {
    if (!intersect(s, t)) return std::nullopt;
    bool sd = same_point(s.a, s.b);
    bool td = same_point(t.a, t.b);
    if (sd && td) return same_point(s.a, t.a) ? std::optional<Point<T>>(s.a) : std::nullopt;
    if (sd) return on_segment(t, s.a) ? std::optional<Point<T>>(s.a) : std::nullopt;
    if (td) return on_segment(s, t.a) ? std::optional<Point<T>>(t.a) : std::nullopt;
    Line<T> ls(s.a, s.b);
    Line<T> lt(t.a, t.b);
    if (is_parallel(ls, lt)) {
        auto ov = overlap_segment(s, t);
        if (ov && same_point(ov->a, ov->b)) return ov->a;
        return std::nullopt;
    }
    return cross_point(ls, lt);
}

// 点を直線へ射影した点を返す O(1)
template<class T>
Point<T> project(const Line<T>& l, const Point<T>& p) {
    Point<T> v = l.vec();
    T len2 = v.norm2();
    if (internal::sign(len2) == 0) return l.a;
    return l.a + v * ((p - l.a).dot(v) / len2);
}

// 点を線分を含む直線へ射影した点を返す O(1)
template<class T>
Point<T> project(const Segment<T>& s, const Point<T>& p) {
    return project(Line<T>(s.a, s.b), p);
}

// 点に最も近い直線上の点を返す O(1)
// 用途: 最近点の座標、直線へのスナップ、距離だけでなく接地点が必要なときに使う
// 使い方: closest_point(l, p) は project(l, p) と同じ結果を返す
template<class T>
Point<T> closest_point(const Line<T>& l, const Point<T>& p) {
    return project(l, p);
}

// 点に最も近い線分上の点を返す O(1)
// 用途: 点から線分への最近点、線分経路上の最寄り位置、衝突候補点の取得に使う
// 使い方: 射影が線分外なら近い端点、退化線分ならその端点を返す
template<class T>
Point<T> closest_point(const Segment<T>& s, const Point<T>& p) {
    Point<T> v = s.vec();
    T len2 = v.norm2();
    if (internal::sign(len2) == 0) return s.a;
    T t = (p - s.a).dot(v) / len2;
    if (internal::le(t, T(0))) return s.a;
    if (internal::ge(t, T(1))) return s.b;
    return s.a + v * t;
}

// 点を直線に関して反射した点を返す O(1)
template<class T>
Point<T> reflect(const Line<T>& l, const Point<T>& p) {
    return project(l, p) * T(2) - p;
}

// 点を線分を含む直線に関して反射した点を返す O(1)
template<class T>
Point<T> reflect(const Segment<T>& s, const Point<T>& p) {
    return reflect(Line<T>(s.a, s.b), p);
}

// 点と直線の距離を返す O(1)
template<class T>
T distance(const Line<T>& l, const Point<T>& p) {
    Point<T> v = l.vec();
    T len = v.norm();
    if (internal::sign(len) == 0) return p.dist(l.a);
    return std::abs(v.cross(p - l.a)) / len;
}

// 点と直線の距離を返す O(1)
template<class T>
T distance(const Point<T>& p, const Line<T>& l) {
    return distance(l, p);
}

// 点と線分の距離を返す O(1)
template<class T>
T distance(const Segment<T>& s, const Point<T>& p) {
    Point<T> v = s.vec();
    T len2 = v.norm2();
    if (internal::sign(len2) == 0) return p.dist(s.a);
    if (internal::le((p - s.a).dot(v), T(0))) return p.dist(s.a);
    if (internal::le((p - s.b).dot(-v), T(0))) return p.dist(s.b);
    return distance(Line<T>(s.a, s.b), p);
}

// 点と線分の距離を返す O(1)
template<class T>
T distance(const Point<T>& p, const Segment<T>& s) {
    return distance(s, p);
}

// 2線分の距離を返す O(1)
template<class T>
T distance(const Segment<T>& s, const Segment<T>& t) {
    if (intersect(s, t)) return T(0);
    return std::min({distance(s, t.a), distance(s, t.b), distance(t, s.a), distance(t, s.b)});
}

// 2直線の距離を返す O(1)
template<class T>
T distance(const Line<T>& x, const Line<T>& y) {
    if (!is_parallel(x, y)) return T(0);
    return distance(x, y.a);
}

// 直線と線分の距離を返す O(1)
template<class T>
T distance(const Line<T>& l, const Segment<T>& s) {
    if (intersect(l, s)) return T(0);
    return std::min(distance(l, s.a), distance(l, s.b));
}

// 線分と直線の距離を返す O(1)
template<class T>
T distance(const Segment<T>& s, const Line<T>& l) {
    return distance(l, s);
}

template<class T = double>
struct Circle {
    Point<T> c;
    T r{};

    // 円を生成する O(1)
    Circle() = default;

    // 中心と半径を指定して円を生成する O(1)
    Circle(Point<T> c_, T r_) : c(c_), r(r_) {}
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

// 円の半径2乗を返す O(1)
template<class T>
T radius2(const Circle<T>& c) {
    return c.r * c.r;
}

// 円と点の包含関係をEPS付きで返す O(1)
template<class T>
Contains contains(const Circle<T>& c, const Point<T>& p) {
    T d = c.c.dist(p);
    if (internal::eq(d, c.r)) return ON_EDGE;
    return d < c.r ? INSIDE : OUTSIDE;
}

// 2円の位置関係をEPS付きで返す O(1)
// 用途: 円同士の交点数、接線数、包含判定を場合分けしたいときに使う
// 使い方: SAME/CONTAIN/INTERNAL_TANGENT/INTERSECT/EXTERNAL_TANGENT/SEPARATE に分類する
// 注意: 同心同半径はCIRCLE_SAME、同心異半径はCIRCLE_CONTAINを返す
template<class T>
CircleRelation circle_relation(const Circle<T>& a, const Circle<T>& b) {
    T d = a.c.dist(b.c);
    T sr = a.r + b.r;
    T dr = std::abs(a.r - b.r);
    if (internal::sign(d) == 0 && internal::eq(a.r, b.r)) return CIRCLE_SAME;
    if (internal::gt(d, sr)) return CIRCLE_SEPARATE;
    if (internal::eq(d, sr)) return CIRCLE_EXTERNAL_TANGENT;
    if (internal::lt(d, dr)) return CIRCLE_CONTAIN;
    if (internal::eq(d, dr)) return CIRCLE_INTERNAL_TANGENT;
    return CIRCLE_INTERSECT;
}

// 円の面積を返す O(1)
// 用途: 円の面積、円同士の共通面積との比較に使う
// 使い方: PI<T>*r*r を返す
// 注意: 半径は非負を想定する
template<class T>
T area(const Circle<T>& c) {
    return PI<T> * c.r * c.r;
}

// 円周長を返す O(1)
// 用途: 円周上移動距離、円弧長の基準値に使う
// 使い方: 2*PI<T>*r を返す
// 注意: 半径は非負を想定する
template<class T>
T circumference(const Circle<T>& c) {
    return T(2) * PI<T> * c.r;
}

// 点pの円cに対する方べき値を返す O(1)
// 用途: 点が円の内外どちらにあるか、根軸、反転幾何の補助に使う
// 使い方: 正なら外部、0付近なら円周上、負なら内部
// 注意: 判定自体は sign(power(c,p)) でEPS付きにする
template<class T>
T power(const Circle<T>& c, const Point<T>& p) {
    return c.c.dist2(p) - c.r * c.r;
}

// 2円の根軸を返す O(1)
// 用途: 2円への方べきが等しい点集合、円束、円交点の補助に使う
// 使い方: 同心円では根軸が有限直線として表せないため退化直線を返す
// 注意: 交点がある場合、その交点はこの直線上にもある
template<class T>
Line<T> radical_axis(const Circle<T>& a, const Circle<T>& b) {
    Point<T> n = b.c - a.c;
    T len2 = n.norm2();
    if (internal::sign(len2) == 0) return Line<T>(a.c, a.c);
    T c0 = (a.c.norm2() - b.c.norm2() - a.r * a.r + b.r * b.r) / T(2);
    Point<T> p = n * (-c0 / len2);
    return Line<T>(p, p + n.rot90());
}

// 2円の共通部分の面積を返す O(1)
// 用途: 円同士の重なり面積、確率・積分系の幾何問題に使う
// 使い方: 離れていれば0、片方が含まれれば小さい円の面積を返す
// 注意: 固定EPSで接触・包含を判定する
template<class T>
T intersection_area(const Circle<T>& a, const Circle<T>& b) {
    T d = a.c.dist(b.c);
    T r1 = a.r;
    T r2 = b.r;
    if (internal::ge(d, r1 + r2)) return T(0);
    T mn = std::min(r1, r2);
    T mx = std::max(r1, r2);
    if (internal::le(d + mn, mx)) return PI<T> * mn * mn;
    T x1 = (d * d + r1 * r1 - r2 * r2) / (T(2) * d * r1);
    T x2 = (d * d + r2 * r2 - r1 * r1) / (T(2) * d * r2);
    x1 = std::max(T(-1), std::min(T(1), x1));
    x2 = std::max(T(-1), std::min(T(1), x2));
    T part = (-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2);
    part = std::sqrt(std::max(T(0), part)) / T(2);
    return r1 * r1 * std::acos(x1) + r2 * r2 * std::acos(x2) - part;
}

// 円周と直線が交わるか判定する O(1)
template<class T>
bool intersect(const Circle<T>& c, const Line<T>& l) {
    return internal::le(distance(l, c.c), c.r);
}

// 円周と線分が交わるか判定する O(1)
// 用途: 線分が円の境界を横切る、または接するかを判定する
// 使い方: 線分全体が円内にあるだけならfalse、円盤との交差はintersect_diskを使う
// 注意: 「円周」と「円盤」の交差判定を混同しない
template<class T>
bool intersect(const Circle<T>& c, const Segment<T>& s) {
    T dmin = distance(s, c.c);
    T dmax = std::max(c.c.dist(s.a), c.c.dist(s.b));
    return internal::le(dmin, c.r) && internal::ge(dmax, c.r);
}

// 線分と円周が交わるか判定する O(1)
template<class T>
bool intersect(const Segment<T>& s, const Circle<T>& c) {
    return intersect(c, s);
}

// 円盤と線分が共通部分を持つか判定する O(1)
// 用途: 障害物の円に線分経路が入るか、円盤との衝突を見たいときに使う
// 使い方: 線分が円内に完全に含まれる場合もtrueを返す
// 注意: 円周だけとの交差を見たい場合はintersect(Circle, Segment)を使う
template<class T>
bool intersect_disk(const Circle<T>& c, const Segment<T>& s) {
    return internal::le(distance(s, c.c), c.r);
}

// 2円の円周が交わるか判定する O(1)
template<class T>
bool intersect(const Circle<T>& a, const Circle<T>& b) {
    CircleRelation rel = circle_relation(a, b);
    return rel != CIRCLE_SEPARATE && rel != CIRCLE_CONTAIN;
}

// 円と直線の交点を返す O(1)
template<class T>
std::vector<Point<T>> cross_points(const Circle<T>& c, const Line<T>& l) {
    std::vector<Point<T>> res;
    Point<T> v = l.vec();
    T len = v.norm();
    if (internal::sign(len) == 0) {
        if (contains(c, l.a) == ON_EDGE) res.push_back(l.a);
        return res;
    }
    Point<T> h = project(l, c.c);
    T d = h.dist(c.c);
    if (internal::gt(d, c.r)) return res;
    if (internal::eq(d, c.r)) {
        res.push_back(h);
        return res;
    }
    T w = std::sqrt(std::max(T(0), c.r * c.r - d * d));
    Point<T> u = v / len;
    res.push_back(h - u * w);
    res.push_back(h + u * w);
    return res;
}

// 円と線分の交点を返す O(1)
template<class T>
std::vector<Point<T>> cross_points(const Circle<T>& c, const Segment<T>& s) {
    std::vector<Point<T>> res;
    for (const auto& p : cross_points(c, Line<T>(s.a, s.b))) {
        if (on_segment(s, p)) {
            bool dup = false;
            for (const auto& q : res) dup = dup || same_point(p, q);
            if (!dup) res.push_back(p);
        }
    }
    return res;
}

// 2円の交点を返す O(1)
template<class T>
std::vector<Point<T>> cross_points(const Circle<T>& a, const Circle<T>& b) {
    std::vector<Point<T>> res;
    if (circle_relation(a, b) == CIRCLE_SAME) return res;
    Point<T> v = b.c - a.c;
    T d = v.norm();
    if (internal::sign(d) == 0) return res;
    if (!intersect(a, b)) return res;
    T x = (a.r * a.r - b.r * b.r + d * d) / (T(2) * d);
    T h2 = a.r * a.r - x * x;
    if (h2 < -EPS<T>) return res;
    h2 = std::max(T(0), h2);
    Point<T> u = v / d;
    Point<T> base = a.c + u * x;
    if (internal::sign(h2) == 0) {
        res.push_back(base);
        return res;
    }
    Point<T> off = u.rot90() * std::sqrt(h2);
    res.push_back(base - off);
    res.push_back(base + off);
    return res;
}

// 円外または円上の点から円への接点を返す O(1)
// 用途: 点から円への接線、視線の接触点、外部点からの可視範囲計算に使う
// 使い方: 円内なら空、円上なら1点、円外なら2点を返す
// 注意: 返る点は円周上の点で、接線そのものが欲しい場合はtangent_linesを使う
template<class T>
std::vector<Point<T>> tangent_points(const Circle<T>& c, const Point<T>& p) {
    std::vector<Point<T>> res;
    Point<T> v = p - c.c;
    T d = v.norm();
    if (internal::lt(d, c.r)) return res;
    if (internal::eq(d, c.r)) {
        res.push_back(p);
        return res;
    }
    T theta = std::atan2(v.y, v.x);
    T delta = std::acos(c.r / d);
    res.emplace_back(c.c.x + c.r * std::cos(theta + delta), c.c.y + c.r * std::sin(theta + delta));
    res.emplace_back(c.c.x + c.r * std::cos(theta - delta), c.c.y + c.r * std::sin(theta - delta));
    return res;
}

// 点から円への接線を返す O(1)
// 用途: 円への接線方程式、接線グラフ、可視接線を作るときに使う
// 使い方: 円内なら空、円上なら1本、円外なら2本のLineを返す
// 注意: 円上の点では半径に垂直な直線を返す
template<class T>
std::vector<Line<T>> tangent_lines(const Circle<T>& c, const Point<T>& p) {
    std::vector<Line<T>> res;
    Point<T> v = p - c.c;
    T d = v.norm();
    if (internal::lt(d, c.r)) return res;
    if (internal::eq(d, c.r)) {
        Point<T> dir = v.rot90();
        if (dir.is_zero()) return res;
        res.emplace_back(p, p + dir);
        return res;
    }
    for (const auto& q : tangent_points(c, p)) res.emplace_back(p, q);
    return res;
}

// 2円の共通接線を返す O(1)
// 用途: 円を避ける最短経路、円同士の接線グラフ、外接線・内接線の列挙に使う
// 使い方: 返り値の本数は通常4本、接する場合は重複を除いて減る
// 注意: 同一円の無限本の接線は表現できないため空を返す
template<class T>
std::vector<Line<T>> common_tangent_lines(const Circle<T>& a, const Circle<T>& b) {
    std::vector<Line<T>> res;
    Point<T> v = b.c - a.c;
    T d = v.norm();
    if (internal::sign(d) == 0) return res;
    Point<T> u = v / d;
    for (int sgn : {1, -1}) {
        T c = (a.r - static_cast<T>(sgn) * b.r) / d;
        if (c > T(1) + EPS<T> || c < T(-1) - EPS<T>) continue;
        c = std::max(T(-1), std::min(T(1), c));
        T h = std::sqrt(std::max(T(0), T(1) - c * c));
        for (int k : {1, -1}) {
            if (internal::sign(h) == 0 && k == -1) continue;
            Point<T> n = u * c + u.rot90() * (h * static_cast<T>(k));
            Point<T> p1 = a.c + n * a.r;
            Point<T> p2 = b.c + n * (static_cast<T>(sgn) * b.r);
            Line<T> line;
            if (same_point(p1, p2)) {
                Point<T> dir = (p1 - a.c).rot90();
                if (dir.is_zero()) dir = (p1 - b.c).rot90();
                if (dir.is_zero()) continue;
                line = Line<T>(p1, p1 + dir);
            } else {
                line = Line<T>(p1, p2);
            }
            bool dup = false;
            for (const auto& old : res) dup = dup || same_line(old, line);
            if (!dup) res.push_back(line);
        }
    }
    return res;
}

// 2円の共通接線を返す O(1)
template<class T>
std::vector<Line<T>> tangent_lines(const Circle<T>& a, const Circle<T>& b) {
    return common_tangent_lines(a, b);
}

// 三角形の外心を返す O(1)
// 用途: 3点を通る円、最小包含円、ドロネー判定の補助に使う
// 使い方: 3点がほぼ一直線ならnulloptを返す
// 注意: 返り値は浮動小数点誤差を含む
template<class T>
std::optional<Point<T>> circumcenter(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    Point<T> ab = b - a;
    Point<T> ac = c - a;
    T d = T(2) * ab.cross(ac);
    if (internal::sign(d) == 0) return std::nullopt;
    T ab2 = ab.norm2();
    T ac2 = ac.norm2();
    Point<T> p = a + Point<T>(ac.y * ab2 - ab.y * ac2, ab.x * ac2 - ac.x * ab2) / d;
    return p;
}

// 三角形の外接円を返す O(1)
// 用途: 3点から円を作る、最小包含円の候補円を作るときに使う
// 使い方: 3点がほぼ一直線ならnulloptを返す
// 注意: 半径は外心から頂点までの距離で計算する
template<class T>
std::optional<Circle<T>> circumcircle(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto o = circumcenter(a, b, c);
    if (!o) return std::nullopt;
    return Circle<T>(*o, o->dist(a));
}

// 三角形の内心を返す O(1)
// 用途: 角の二等分線の交点、内接円の中心を求めるときに使う
// 使い方: 退化三角形ならnulloptを返す
// 注意: 各頂点の重みは対辺長になる
template<class T>
std::optional<Point<T>> incenter(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    T la = b.dist(c);
    T lb = c.dist(a);
    T lc = a.dist(b);
    T sum = la + lb + lc;
    if (internal::sign(sum) == 0 || orient(a, b, c) == 0) return std::nullopt;
    return Point<T>((a.x * la + b.x * lb + c.x * lc) / sum,
                    (a.y * la + b.y * lb + c.y * lc) / sum);
}

// 三角形の内接円を返す O(1)
// 用途: 三角形内の最大円、内接円半径を求めるときに使う
// 使い方: 退化三角形ならnulloptを返す
// 注意: 半径は内心から辺までの距離で計算する
template<class T>
std::optional<Circle<T>> incircle(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto p = incenter(a, b, c);
    if (!p) return std::nullopt;
    T r = distance(Line<T>(a, b), *p);
    return Circle<T>(*p, r);
}

// 三角形の重心を返す O(1)
// 用途: 三角形の中線の交点、3点の平均位置を求めるときに使う
// 使い方: (a+b+c)/3 を返し、退化三角形でも定義される
// 注意: 多角形の面積重心とは別の三角形専用の重心
template<class T>
Point<T> triangle_centroid(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return (a + b + c) / T(3);
}

// 三角形の垂心を返す O(1)
// 用途: オイラー線、三角形中心、垂線の交点を求めるときに使う
// 使い方: 3点がほぼ一直線ならnulloptを返す
// 注意: 外心Oに対して H = a+b+c-2O で計算する
template<class T>
std::optional<Point<T>> orthocenter(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    auto o = circumcenter(a, b, c);
    if (!o) return std::nullopt;
    return a + b + c - (*o) * T(2);
}

template<class T = double>
using Polygon = std::vector<Point<T>>;

// 三角形の符号付き面積2倍を返す O(1)
template<class T>
T signed_area2(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return (b - a).cross(c - a);
}

// 三角形の面積2倍を返す O(1)
template<class T>
T area2(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return std::abs(signed_area2(a, b, c));
}

// 三角形の面積を返す O(1)
template<class T>
T area(const Point<T>& a, const Point<T>& b, const Point<T>& c) {
    return area2(a, b, c) / T(2);
}

// 多角形の符号付き面積2倍を返す O(N)
template<class T>
T signed_area2(const Polygon<T>& p) {
    T s = T(0);
    int n = static_cast<int>(p.size());
    for (int i = 0; i < n; ++i) s += p[i].cross(p[(i + 1) % n]);
    return s;
}

// 多角形の面積2倍を返す O(N)
template<class T>
T area2(const Polygon<T>& p) {
    return std::abs(signed_area2(p));
}

// 多角形の面積を返す O(N)
template<class T>
T area(const Polygon<T>& p) {
    return area2(p) / T(2);
}

// 多角形の周長を返す O(N)
template<class T>
T perimeter(const Polygon<T>& p) {
    T s = T(0);
    int n = static_cast<int>(p.size());
    for (int i = 0; i < n; ++i) s += p[i].dist(p[(i + 1) % n]);
    return s;
}

// 一般多角形が点を含むかEPS付きで判定する O(N)
template<class T>
Contains contains(const Polygon<T>& g, const Point<T>& p) {
    bool in = false;
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) {
        Point<T> a = g[i];
        Point<T> b = g[(i + 1) % n];
        if (on_segment(Segment<T>(a, b), p)) return ON_EDGE;
        bool cross_y = (a.y > p.y) != (b.y > p.y);
        if (cross_y) {
            T x = a.x + (b.x - a.x) * (p.y - a.y) / (b.y - a.y);
            if (x > p.x + EPS<T>) in = !in;
        }
    }
    return in ? INSIDE : OUTSIDE;
}

// 多角形が凸かEPS付きで判定する O(N)
template<class T>
bool is_convex(const Polygon<T>& p, bool strict = false) {
    int n = static_cast<int>(p.size());
    if (n < 3) return false;
    int dir = 0;
    for (int i = 0; i < n; ++i) {
        int o = orient(p[i], p[(i + 1) % n], p[(i + 2) % n]);
        if (o == 0) {
            if (strict) return false;
            continue;
        }
        if (dir == 0) dir = o;
        else if (dir != o) return false;
    }
    return true;
}

// 連続する重複点や共線点を圧縮した多角形を返す O(N)
// 用途: 凸包やミンコフスキー和の後処理、余分な頂点を消したいときに使う
// 使い方: 3点が一直線に並ぶ連続頂点の中央を削除する
// 注意: 形状の自己交差は修正しない
template<class T>
Polygon<T> compress_collinear(const Polygon<T>& poly) {
    Polygon<T> q;
    for (const auto& p : poly) {
        if (q.empty() || !same_point(q.back(), p)) q.push_back(p);
    }
    if (q.size() >= 2 && same_point(q.front(), q.back())) q.pop_back();
    bool changed = true;
    while (changed && q.size() >= 3) {
        changed = false;
        Polygon<T> r;
        int n = static_cast<int>(q.size());
        for (int i = 0; i < n; ++i) {
            const Point<T>& a = q[(i - 1 + n) % n];
            const Point<T>& b = q[i];
            const Point<T>& c = q[(i + 1) % n];
            if (same_point(a, b) || same_point(b, c)) {
                changed = true;
                continue;
            }
            if (orient(a, b, c) == 0 && internal::le((a - b).dot(c - b), T(0))) {
                changed = true;
                continue;
            }
            r.push_back(b);
        }
        q.swap(r);
    }
    return q;
}

// 点集合の凸包を反時計回りで返す O(N log N)
template<class T>
Polygon<T> convex_hull(std::vector<Point<T>> ps, bool keep_collinear = false) {
    std::sort(ps.begin(), ps.end());
    std::vector<Point<T>> u;
    for (const auto& p : ps) {
        if (u.empty() || !same_point(u.back(), p)) u.push_back(p);
    }
    ps.swap(u);
    int n = static_cast<int>(ps.size());
    if (n <= 1) return ps;
    if (is_collinear(ps)) {
        if (keep_collinear) return ps;
        return Polygon<T>{ps.front(), ps.back()};
    }
    Polygon<T> lower, upper;
    for (const auto& p : ps) {
        while (lower.size() >= 2) {
            int o = orient(lower[lower.size() - 2], lower.back(), p);
            if (keep_collinear ? o < 0 : o <= 0) lower.pop_back();
            else break;
        }
        lower.push_back(p);
    }
    for (int i = n - 1; i >= 0; --i) {
        const auto& p = ps[i];
        while (upper.size() >= 2) {
            int o = orient(upper[upper.size() - 2], upper.back(), p);
            if (keep_collinear ? o < 0 : o <= 0) upper.pop_back();
            else break;
        }
        upper.push_back(p);
    }
    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return keep_collinear ? lower : compress_collinear(lower);
}

// 多角形が単純多角形か判定する O(N^2)
// 用途: 一般多角形の入力検証、自己交差の検出に使う
// 使い方: 隣接辺の共有端点は許し、それ以外の交差や重複頂点があればfalse
// 注意: 3頂点未満やゼロ長辺を含む場合はfalseを返す
template<class T>
bool is_simple_polygon(const Polygon<T>& p) {
    int n = static_cast<int>(p.size());
    if (n < 3) return false;
    for (int i = 0; i < n; ++i) {
        if (same_point(p[i], p[(i + 1) % n])) return false;
        for (int j = i + 1; j < n; ++j) {
            if (same_point(p[i], p[j])) return false;
        }
    }
    for (int i = 0; i < n; ++i) {
        Segment<T> a(p[i], p[(i + 1) % n]);
        for (int j = i + 1; j < n; ++j) {
            if (i == j) continue;
            if ((i + 1) % n == j || (j + 1) % n == i) continue;
            Segment<T> b(p[j], p[(j + 1) % n]);
            if (intersect(a, b)) return false;
        }
    }
    return true;
}

// 凸多角形が点を含むかEPS付きで判定する O(N)
// 用途: 凸包後の点包含、半平面切断後の検証に使う
// 使い方: 返り値は一般多角形のcontainsと同じOUTSIDE/ON_EDGE/INSIDE
// 注意: 入力は凸多角形を想定するが、実装はcontainsに委譲する
template<class T>
Contains convex_contains(const Polygon<T>& g, const Point<T>& p) {
    return contains(g, p);
}

// 凸多角形を有向直線の左側半平面で切る O(N)
// 用途: 半平面クリップ、凸多角形と半平面の共通部分、凸多角形同士の交差に使う
// 使い方: l.a->l.b の左側と境界上の点を残す
// 注意: 入力は反時計回りの凸多角形を想定する
template<class T>
Polygon<T> convex_cut(const Polygon<T>& g, const Line<T>& l) {
    Polygon<T> res;
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) {
        Point<T> a = g[i];
        Point<T> b = g[(i + 1) % n];
        int sa = side(l, a);
        int sb = side(l, b);
        if (sa >= 0) res.push_back(a);
        if (sa * sb < 0) {
            auto cp = cross_point(l, Line<T>(a, b));
            if (cp) res.push_back(*cp);
        }
    }
    return compress_collinear(res);
}

// 凸多角形同士の共通部分を返す O(NM)
// 用途: 2つの凸領域の交差、多角形クリッピング、共通面積計算に使う
// 使い方: 各辺の左側半平面で順に切る。返り値は空・点・線分・凸多角形になり得る
// 注意: 入力は凸多角形を想定し、時計回りなら内部で反転して扱う
template<class T>
Polygon<T> convex_intersection(Polygon<T> a, Polygon<T> b) {
    a = compress_collinear(a);
    b = compress_collinear(b);
    if (a.empty() || b.empty()) return {};
    if (signed_area2(a) < T(0)) std::reverse(a.begin(), a.end());
    if (signed_area2(b) < T(0)) std::reverse(b.begin(), b.end());
    Polygon<T> res = a;
    int n = static_cast<int>(b.size());
    for (int i = 0; i < n && !res.empty(); ++i) {
        res = convex_cut(res, Line<T>(b[i], b[(i + 1) % n]));
    }
    return compress_collinear(res);
}

// 凸多角形同士の共通面積を返す O(NM)
// 用途: 2つの凸領域の重なり面積だけが必要なときに使う
// 使い方: convex_intersection(a,b) の面積を返す
// 注意: 点や線分だけで接する場合の面積は0
template<class T>
T convex_intersection_area(const Polygon<T>& a, const Polygon<T>& b) {
    return area(convex_intersection(a, b));
}

// 半平面集合の共通部分を大きな正方形でクリップして返す O(KV)
// 用途: 線形不等式の共通領域、半平面制約の簡易処理、凸領域の逐次クリップに使う
// 使い方: 各Lineの左側を残し、boundで初期正方形 [-bound,bound]^2 を指定する
// 注意: 非有界な共通部分は指定正方形で切られた多角形として返る
template<class T>
Polygon<T> half_plane_intersection(const std::vector<Line<T>>& hs, T bound) {
    Polygon<T> res{Point<T>(-bound, -bound), Point<T>(bound, -bound), Point<T>(bound, bound), Point<T>(-bound, bound)};
    for (const auto& h : hs) {
        res = convex_cut(res, h);
        if (res.empty()) break;
    }
    return compress_collinear(res);
}

// 半平面集合の共通部分をデフォルト正方形でクリップして返す O(KV)
// 用途: 座標範囲が十分小さい問題で、簡単に半平面交差を試したいときに使う
// 使い方: 精度や座標範囲が重要な場合はbound明示版を使う
template<class T>
Polygon<T> half_plane_intersection(const std::vector<Line<T>>& hs) {
    return half_plane_intersection(hs, T(1e9));
}

// 凸多角形が点を含むかEPS付きで判定する O(log N)
// 用途: 同じ凸多角形に対して大量の点包含クエリを処理するときに使う
// 使い方: 入力は反時計回りの凸多角形を想定し、返り値はOUTSIDE/ON_EDGE/INSIDE
// 注意: 共線頂点を含まない凸包済みの点列で使うと安定しやすい
template<class T>
Contains convex_contains_log(const Polygon<T>& g, const Point<T>& p) {
    int n = static_cast<int>(g.size());
    if (n == 0) return OUTSIDE;
    if (n == 1) return same_point(g[0], p) ? ON_EDGE : OUTSIDE;
    if (n == 2) return on_segment(Segment<T>(g[0], g[1]), p) ? ON_EDGE : OUTSIDE;
    if (on_segment(Segment<T>(g[0], g[1]), p) || on_segment(Segment<T>(g[0], g[n - 1]), p)) return ON_EDGE;
    if (orient(g[0], g[1], p) < 0) return OUTSIDE;
    if (orient(g[0], g[n - 1], p) > 0) return OUTSIDE;
    int l = 1, r = n - 1;
    while (r - l > 1) {
        int m = (l + r) / 2;
        if (orient(g[0], g[m], p) >= 0) l = m;
        else r = m;
    }
    int o = orient(g[l], g[(l + 1) % n], p);
    if (o < 0) return OUTSIDE;
    if (o == 0) return ON_EDGE;
    return INSIDE;
}

// 一般多角形領域と線分が交わるか判定する O(N)
// 用途: 線分経路が障害物多角形に入るか、境界を横切るかを判定する
// 使い方: 線分端点が内部・境界上にある場合もtrueを返す
// 注意: 多角形は閉領域として扱う
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
template<class T>
T distance(const Polygon<T>& g, const Point<T>& p) {
    if (g.empty()) return std::numeric_limits<T>::infinity();
    if (contains(g, p) != OUTSIDE) return T(0);
    T best = std::numeric_limits<T>::infinity();
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) best = std::min(best, distance(Segment<T>(g[i], g[(i + 1) % n]), p));
    return best;
}

// 点と一般多角形領域の距離を返す O(N)
template<class T>
T distance(const Point<T>& p, const Polygon<T>& g) {
    return distance(g, p);
}

// 一般多角形領域と線分の距離を返す O(N)
// 用途: 線分経路と障害物領域の最短距離を求めるときに使う
// 使い方: 交差していれば0、空多角形ならinfinity
// 注意: 多角形は閉領域として扱う
template<class T>
T distance(const Polygon<T>& g, const Segment<T>& s) {
    if (g.empty()) return std::numeric_limits<T>::infinity();
    if (intersect(g, s)) return T(0);
    T best = std::min(distance(g, s.a), distance(g, s.b));
    int n = static_cast<int>(g.size());
    for (int i = 0; i < n; ++i) best = std::min(best, distance(Segment<T>(g[i], g[(i + 1) % n]), s));
    return best;
}

// 線分と一般多角形領域の距離を返す O(N)
template<class T>
T distance(const Segment<T>& s, const Polygon<T>& g) {
    return distance(g, s);
}

// 一般多角形領域同士の距離を返す O(NM)
// 用途: 多角形障害物間の最短距離、衝突していない領域の間隔計算に使う
// 使い方: 交差していれば0、どちらかが空ならinfinity
// 注意: 多角形は閉領域として扱う
template<class T>
T distance(const Polygon<T>& a, const Polygon<T>& b) {
    if (a.empty() || b.empty()) return std::numeric_limits<T>::infinity();
    if (intersect(a, b)) return T(0);
    T best = std::numeric_limits<T>::infinity();
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    for (int i = 0; i < n; ++i) {
        Segment<T> ea(a[i], a[(i + 1) % n]);
        for (int j = 0; j < m; ++j) best = std::min(best, distance(ea, Segment<T>(b[j], b[(j + 1) % m])));
    }
    return best;
}


// dir方向の内積最大点のindexを返す O(N)
// 用途: 支持関数、凸図形の幅、回転キャリパーの補助に使う
// 使い方: 空なら-1を返す
// 注意: 同値の場合は最初に見つかった点を返す
template<class T>
int support_index(const std::vector<Point<T>>& ps, const Point<T>& dir) {
    if (ps.empty()) return -1;
    int best = 0;
    T val = ps[0].dot(dir);
    for (int i = 1; i < static_cast<int>(ps.size()); ++i) {
        T cur = ps[i].dot(dir);
        if (internal::gt(cur, val)) {
            val = cur;
            best = i;
        }
    }
    return best;
}

// dir方向の内積最大値を返す O(N)
// 用途: 支持関数値だけが必要なときに使う
// 使い方: 空なら -infinity を返す
// 注意: dirがゼロなら全点同値になり、先頭点の内積0を返す
template<class T>
T support_value(const std::vector<Point<T>>& ps, const Point<T>& dir) {
    int idx = support_index(ps, dir);
    if (idx < 0) return -std::numeric_limits<T>::infinity();
    return ps[idx].dot(dir);
}

// dir方向の内積最大点を返す O(N)
// 用途: 支持点そのものが必要なときに使う
// 使い方: 空ならPoint()を返す
// 注意: 同値の場合は最初に見つかった点を返す
template<class T>
Point<T> support_point(const std::vector<Point<T>>& ps, const Point<T>& dir) {
    int idx = support_index(ps, dir);
    if (idx < 0) return Point<T>();
    return ps[idx];
}

// dirに垂直な2本の支持線間の幅を返す O(N)
// 用途: 指定方向への凸図形の厚み、射影幅、分離軸判定の補助に使う
// 使い方: 幅は max(dot(p,dir))-min(dot(p,dir)) を |dir| で割った値
// 注意: 空集合またはゼロ方向ベクトルでは0を返す
template<class T>
T width(const std::vector<Point<T>>& ps, const Point<T>& dir) {
    T len = dir.norm();
    if (ps.empty() || internal::sign(len) == 0) return T(0);
    return (support_value(ps, dir) + support_value(ps, -dir)) / len;
}

// 凸多角形の最遠点対のindexを返す O(N^2)
// 用途: 凸包済み点列の直径の点対が必要なときに使う
// 使い方: 空なら{-1,-1}、1点なら{0,0}を返す
// 注意: シンプルさ優先で全点対を確認する
template<class T>
std::pair<int, int> convex_diameter_pair(const Polygon<T>& p) {
    int n = static_cast<int>(p.size());
    if (n == 0) return {-1, -1};
    if (n == 1) return {0, 0};
    T best = T(-1);
    std::pair<int, int> ans{0, 1};
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            T d = p[i].dist2(p[j]);
            if (d > best) {
                best = d;
                ans = {i, j};
            }
        }
    }
    return ans;
}

// 凸多角形の直径2乗を返す O(N^2)
template<class T>
T convex_diameter2(const Polygon<T>& p) {
    auto [i, j] = convex_diameter_pair(p);
    if (i < 0) return T(0);
    return p[i].dist2(p[j]);
}

// 凸多角形の直径を返す O(N^2)
// 用途: 実距離が必要なときに使う
// 使い方: convex_diameter2 の平方根を返す
// 注意: 距離比較だけなら平方根を避けてconvex_diameter2を使う
template<class T>
T convex_diameter(const Polygon<T>& p) {
    return std::sqrt(convex_diameter2(p));
}

// 点集合の直径2乗を返す O(N log N + H^2)
// 用途: 点集合の最遠距離だけが必要なときに使う
// 使い方: 内部で凸包を作り、凸包上の全点対を確認する
// 注意: シンプルさ優先で回転キャリパーではなく全点対を使う
template<class T>
T diameter2(std::vector<Point<T>> ps) {
    return convex_diameter2(convex_hull(ps));
}

// 点集合の直径を返す O(N log N + H^2)
// 用途: 実距離として最遠距離が必要なときに使う
// 使い方: diameter2 の平方根を返す
// 注意: 距離比較だけなら平方根を避けてdiameter2を使う
template<class T>
T diameter(std::vector<Point<T>> ps) {
    return std::sqrt(diameter2(std::move(ps)));
}

namespace internal {

template<class T>
T closest_pair_rec(std::vector<Point<T>>& ps, int l, int r) {
    if (r - l <= 16) {
        T best = std::numeric_limits<T>::infinity();
        for (int i = l; i < r; ++i) {
            for (int j = i + 1; j < r; ++j) best = std::min(best, ps[i].dist2(ps[j]));
        }
        std::sort(ps.begin() + l, ps.begin() + r, [](const Point<T>& a, const Point<T>& b) {
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        });
        return best;
    }
    int m = (l + r) / 2;
    T mid_x = ps[m].x;
    T best = std::min(closest_pair_rec(ps, l, m), closest_pair_rec(ps, m, r));
    std::inplace_merge(ps.begin() + l, ps.begin() + m, ps.begin() + r, [](const Point<T>& a, const Point<T>& b) {
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });
    std::vector<Point<T>> buf;
    for (int i = l; i < r; ++i) {
        T dx = ps[i].x - mid_x;
        if (dx * dx > best + EPS<T>) continue;
        for (int j = static_cast<int>(buf.size()) - 1; j >= 0; --j) {
            T dy = ps[i].y - buf[j].y;
            if (dy * dy > best + EPS<T>) break;
            best = std::min(best, ps[i].dist2(buf[j]));
        }
        buf.push_back(ps[i]);
    }
    return best;
}

}  // namespace internal

// 点集合の最近点対距離2乗を返す O(N log N)
// 用途: 最近2点の距離だけが必要なときに使う
// 使い方: 2点未満なら infinity を返す
// 注意: 返り値は距離2乗で、sqrtを避けた比較に向く
template<class T>
T closest_pair2(std::vector<Point<T>> ps) {
    if (ps.size() < 2) return std::numeric_limits<T>::infinity();
    std::sort(ps.begin(), ps.end(), [](const Point<T>& a, const Point<T>& b) {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    });
    for (int i = 1; i < static_cast<int>(ps.size()); ++i) {
        if (same_point(ps[i - 1], ps[i])) return T(0);
    }
    return internal::closest_pair_rec(ps, 0, static_cast<int>(ps.size()));
}

// 点集合の最近点対距離を返す O(N log N)
// 用途: 実距離として最近距離が必要なときに使う
// 使い方: closest_pair2 の平方根を返し、2点未満ならinfinity
// 注意: 距離比較だけなら平方根を避けてclosest_pair2を使う
template<class T>
T closest_pair_dist(std::vector<Point<T>> ps) {
    return std::sqrt(closest_pair2(std::move(ps)));
}

// 点集合の最近点対を返す O(N^2)
// 用途: 距離だけでなく点対そのものが必要なときに使う
// 使い方: 2点未満なら{Point(), Point()}を返す
// 注意: シンプルさ優先で点対版は全探索にしている
template<class T>
std::pair<Point<T>, Point<T>> closest_pair(const std::vector<Point<T>>& ps) {
    if (ps.size() < 2) return {Point<T>(), Point<T>()};
    T best = std::numeric_limits<T>::infinity();
    std::pair<Point<T>, Point<T>> ans{ps[0], ps[1]};
    for (int i = 0; i < static_cast<int>(ps.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(ps.size()); ++j) {
            T d = ps[i].dist2(ps[j]);
            if (d < best) {
                best = d;
                ans = {ps[i], ps[j]};
            }
        }
    }
    return ans;
}

// 頂点座標の平均を返す O(N)
// 用途: 点集合の代表点、退化多角形の重心フォールバックに使う
// 使い方: 幾何的な面積重心ではなく、単純な座標平均
// 注意: 空ならPoint()を返す
template<class T>
Point<T> vertex_mean(const std::vector<Point<T>>& ps) {
    if (ps.empty()) return Point<T>();
    Point<T> s;
    for (const auto& p : ps) s += p;
    return s / static_cast<T>(ps.size());
}

// 多角形の面積重心を返す O(N)
// 用途: 一様な薄板としての多角形の重心を求めるときに使う
// 使い方: 面積がほぼ0ならvertex_meanにフォールバックする
// 注意: 自己交差多角形の重心としては扱わない
template<class T>
Point<T> polygon_centroid(const Polygon<T>& p) {
    T a2 = signed_area2(p);
    if (internal::sign(a2) == 0) return vertex_mean(p);
    Point<T> c;
    int n = static_cast<int>(p.size());
    for (int i = 0; i < n; ++i) {
        const Point<T>& u = p[i];
        const Point<T>& v = p[(i + 1) % n];
        T cr = u.cross(v);
        c += (u + v) * cr;
    }
    return c / (T(3) * a2);
}

// 最も下、同じなら左の頂点indexを返す O(N)
template<class T>
int bottom_left_index(const Polygon<T>& p) {
    if (p.empty()) return -1;
    int idx = 0;
    for (int i = 1; i < static_cast<int>(p.size()); ++i) {
        if (p[i].y < p[idx].y || (p[i].y == p[idx].y && p[i].x < p[idx].x)) idx = i;
    }
    return idx;
}

// 多角形の先頭を最下左頂点に回転する O(N)
template<class T>
void rotate_to_bottom_left(Polygon<T>& p) {
    int idx = bottom_left_index(p);
    if (idx >= 0) std::rotate(p.begin(), p.begin() + idx, p.end());
}

// 多角形の向きと先頭頂点を正規化して返す O(N)
template<class T>
Polygon<T> normalized_polygon(Polygon<T> p, bool ccw_order = true) {
    p = compress_collinear(p);
    if ((signed_area2(p) >= T(0)) != ccw_order) std::reverse(p.begin(), p.end());
    rotate_to_bottom_left(p);
    return p;
}

// 凸多角形同士のミンコフスキー和を返す O(NM log(NM))
// 用途: 2つの凸図形の和、ロボットの形状拡張、差ベクトル集合の凸包に使う
// 使い方: 各頂点和を全列挙して凸包を取るため、入力順に依存しない
// 注意: シンプルさ優先の実装で、大きいN,MではO(N+M)実装より重い
template<class T>
Polygon<T> minkowski_sum_convex(const Polygon<T>& a, const Polygon<T>& b) {
    if (a.empty() || b.empty()) return {};
    std::vector<Point<T>> ps;
    ps.reserve(a.size() * b.size());
    for (const auto& x : a) {
        for (const auto& y : b) ps.push_back(x + y);
    }
    return convex_hull(ps);
}

// 最小包含円を返す 期待O(N)
// 用途: 点集合をすべて覆う最小半径、円形通信範囲、クラスタの外接円に使う
// 使い方: 空ならnullopt、1点なら半径0の円を返す
// 注意: 固定シードでシャッフルするため、実行ごとに結果順は安定する
template<class T>
std::optional<Circle<T>> minimum_enclosing_circle(std::vector<Point<T>> ps) {
    if (ps.empty()) return std::nullopt;
    std::mt19937 rng(0);
    std::shuffle(ps.begin(), ps.end(), rng);
    Circle<T> c(ps[0], T(0));
    auto inside = [](const Circle<T>& cir, const Point<T>& p) {
        return internal::le(cir.c.dist(p), cir.r);
    };
    for (int i = 0; i < static_cast<int>(ps.size()); ++i) {
        if (inside(c, ps[i])) continue;
        c = Circle<T>(ps[i], T(0));
        for (int j = 0; j < i; ++j) {
            if (inside(c, ps[j])) continue;
            Point<T> cen = (ps[i] + ps[j]) / T(2);
            c = Circle<T>(cen, cen.dist(ps[i]));
            for (int k = 0; k < j; ++k) {
                if (inside(c, ps[k])) continue;
                auto cc = circumcircle(ps[i], ps[j], ps[k]);
                if (cc) c = *cc;
            }
        }
    }
    return c;
}

template<class T = double, bool strict = true>
struct IncrementalConvexHull {
    using P = Point<T>;

    std::vector<P> points;
    mutable Polygon<T> cached;
    mutable bool dirty = true;

    // 点を追加する O(1)
    void add(const P& p) {
        points.push_back(p);
        dirty = true;
    }

    // 現在の凸包を返す O(N log N) 初回または更新後、以降O(1)
    // 用途: 挿入のみの点集合について、必要な時点で凸包を取得する
    // 使い方: add後にhull/area/sideを呼ぶと遅延再構築する
    // 注意: 本格的な動的凸包ではなく、シンプルなlazy rebuild実装
    const Polygon<T>& hull() const {
        if (dirty) {
            cached = convex_hull(points, !strict);
            dirty = false;
        }
        return cached;
    }

    // 凸包頂点数を返す O(N log N) 初回または更新後、以降O(1)
    int size() const { return static_cast<int>(hull().size()); }

    // 点集合が空か判定する O(1)
    bool empty() const { return points.empty(); }

    // 点が現在の凸包の内部・境界・外部のどこにあるか返す O(N log N) 初回または更新後、以降O(H)
    // 用途: 挿入済み点集合の凸包に対する包含判定に使う
    // 使い方: 1=内部、0=境界上、-1=外部を返す
    // 注意: 退化凸包では点・線分として判定する
    int side(const P& p) const {
        const auto& h = hull();
        if (h.empty()) return -1;
        if (h.size() == 1) return same_point(h[0], p) ? 0 : -1;
        if (h.size() == 2) return on_segment(Segment<T>(h[0], h[1]), p) ? 0 : -1;
        Contains c = convex_contains(h, p);
        if (c == INSIDE) return 1;
        if (c == ON_EDGE) return 0;
        return -1;
    }

    // 凸包の面積2倍を返す O(N log N) 初回または更新後、以降O(H)
    T area2() const { return fgeo::area2(hull()); }

    // 凸包の面積を返す O(N log N) 初回または更新後、以降O(H)
    T area() const { return fgeo::area(hull()); }
};

// 等速直線運動する点を指定速度で迎撃できるか返す O(1)
// 用途: 移動目標への最短迎撃時刻、射撃・追跡問題に使う
// 使い方: {可能か, 最短非負時刻, 迎撃点} を返し、不能なら {false,0,Point()} を返す
// 注意: shooterから迎撃点までの距離が speed*time になる最小時刻を選ぶ
template<class T>
std::tuple<bool, T, Point<T>> point_of_impact(Point<T> shooter, T speed, Point<T> target, Point<T> target_velocity) {
    Point<T> r = target - shooter;
    if (r.norm() <= EPS<T>) return {true, T(0), target};
    T a = target_velocity.norm2() - speed * speed;
    T b = T(2) * r.dot(target_velocity);
    T c = r.norm2();
    std::vector<T> cand;
    if (internal::sign(a) == 0) {
        if (internal::sign(b) != 0) cand.push_back(-c / b);
    } else {
        T disc = b * b - T(4) * a * c;
        if (disc >= -EPS<T>) {
            disc = std::max(T(0), disc);
            T sq = std::sqrt(disc);
            cand.push_back((-b - sq) / (T(2) * a));
            cand.push_back((-b + sq) / (T(2) * a));
        }
    }
    T best = std::numeric_limits<T>::infinity();
    for (T t : cand) {
        if (t >= -EPS<T>) best = std::min(best, std::max(T(0), t));
    }
    if (!std::isfinite(best)) return {false, T(0), Point<T>()};
    return {true, best, target + target_velocity * best};
}

}  // namespace fgeo

#if __INCLUDE_LEVEL__ == 0
#include <chrono>

namespace {

using T = double;
using P = fgeo::Point<T>;
using L = fgeo::Line<T>;
using S = fgeo::Segment<T>;
using C = fgeo::Circle<T>;
using Poly = fgeo::Polygon<T>;

struct Tester {
    int checks = 0;

    void check(bool cond, const std::string& msg) {
        ++checks;
        if (!cond) {
            std::cerr << "[FAIL] " << msg << '\n';
            std::exit(1);
        }
    }

    void near(T a, T b, const std::string& msg, T eps = 1e-8) {
        ++checks;
        if (std::abs(a - b) > eps) {
            std::cerr << "[FAIL] " << msg << " expected=" << b << " actual=" << a << '\n';
            std::exit(1);
        }
    }

    void point_near(P a, P b, const std::string& msg, T eps = 1e-8) {
        ++checks;
        if (a.dist(b) > eps) {
            std::cerr << "[FAIL] " << msg << " expected=" << b << " actual=" << a << '\n';
            std::exit(1);
        }
    }
};

T brute_closest2(const std::vector<P>& ps) {
    if (ps.size() < 2) return std::numeric_limits<T>::infinity();
    T best = std::numeric_limits<T>::infinity();
    for (int i = 0; i < static_cast<int>(ps.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(ps.size()); ++j) best = std::min(best, ps[i].dist2(ps[j]));
    }
    return best;
}

T brute_diameter2(const std::vector<P>& ps) {
    T best = 0;
    for (int i = 0; i < static_cast<int>(ps.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(ps.size()); ++j) best = std::max(best, ps[i].dist2(ps[j]));
    }
    return best;
}

void test_point_and_eps(Tester& tr) {
    tr.check(fgeo::sign(1e-9) == 1, "sign positive");
    tr.check(fgeo::sign(1e-12) == 0, "sign zero by eps");
    tr.check(fgeo::eq(1.0, 1.0 + 5e-11), "eq eps");
    tr.check(fgeo::lt(1.0, 1.0 + 1e-8), "lt eps");
    tr.check(fgeo::le(1.0, 1.0 + 5e-11), "le eps");
    tr.check(fgeo::gt(1.0 + 1e-8, 1.0), "gt eps");
    tr.check(fgeo::ge(1.0, 1.0 + 5e-11), "ge eps");
    tr.near(fgeo::PI<T>, std::acos(-1.0), "PI constant");
    tr.near(fgeo::deg_to_rad(180.0), std::acos(-1.0), "deg_to_rad");
    tr.near(fgeo::rad_to_deg(std::acos(-1.0) / 2), 90.0, "rad_to_deg");
    tr.near(fgeo::normalize_angle(-std::acos(-1.0) / 2), 3 * std::acos(-1.0) / 2, "normalize_angle negative");
    tr.near(fgeo::normalize_angle(5 * std::acos(-1.0)), std::acos(-1.0), "normalize_angle large");
    tr.near(fgeo::angle_diff(0.0, 3 * std::acos(-1.0) / 2), std::acos(-1.0) / 2, "angle_diff wrap");

    P a(3, 4), b(1, -2);
    fgeo::Vector<> alias_vec(1, 2);
    tr.point_near(alias_vec, P(1, 2), "Vector alias");
    fgeo::Point<long double> ld_from(P(1, 2));
    tr.check(ld_from.x == 1.0L && ld_from.y == 2.0L, "templated point constructor");
    tr.point_near(+a, P(3, 4), "unary plus");
    tr.point_near(-a, P(-3, -4), "unary minus");
    tr.point_near(a + b, P(4, 2), "point add");
    tr.point_near(a - b, P(2, 6), "point sub");
    tr.point_near(a * 2.0, P(6, 8), "point mul");
    tr.point_near(2.0 * a, P(6, 8), "left point mul");
    tr.point_near(a / 2.0, P(1.5, 2), "point div");
    P c = a;
    c += b;
    tr.point_near(c, P(4, 2), "point +=");
    c -= b;
    tr.point_near(c, a, "point -=");
    c *= 2;
    tr.point_near(c, P(6, 8), "point *=");
    c /= 2;
    tr.point_near(c, a, "point /=");
    tr.check(P(0, 0) < P(0, 1), "operator< exact");
    tr.check(P(0, 1) > P(0, 0), "operator> exact");
    tr.check(P(0.1 + 0.2, 0) != P(0.3, 0), "operator== exact");
    tr.check(fgeo::same_point(P(0.1 + 0.2, 0), P(0.3, 0)), "same_point eps");
    tr.check(!fgeo::same_point(P(0, 0), P(2e-10, 0)), "same_point outside eps");
    tr.near(a.dot(b), -5, "dot member");
    tr.near(fgeo::dot(a, b), -5, "dot free");
    tr.near(a.cross(b), -10, "cross member");
    tr.near(fgeo::cross(a, b), -10, "cross free");
    tr.near(a.norm2(), 25, "norm2 member");
    tr.near(fgeo::norm2(a), 25, "norm2 free");
    tr.near(a.norm(), 5, "norm member");
    tr.near(fgeo::norm(a), 5, "norm free");
    tr.near(a.dist2(b), 40, "dist2 member");
    tr.near(fgeo::dist2(a, b), 40, "dist2 free");
    tr.near(a.dist(b), std::sqrt(40.0), "dist member");
    tr.near(fgeo::dist(a, b), std::sqrt(40.0), "dist free");
    tr.near(P(0, 1).arg(), std::acos(-1.0) / 2, "arg");
    tr.point_near(P(3, 4).unit(), P(0.6, 0.8), "unit");
    tr.point_near(P(0, 0).unit(), P(0, 0), "unit zero");
    tr.check(P(1e-12, 0).is_zero(), "is_zero eps");
    tr.check(!P(2e-10, 0).is_zero(), "is_zero outside eps");
    tr.check(P(1, 1).is_parallel(P(2, 2 + 1e-12)), "point parallel eps");
    tr.check(P(1, 0).is_orthogonal(P(1e-12, 1)), "point orthogonal eps");
    tr.point_near(P(2, 3).rot90(), P(-3, 2), "rot90");
    tr.point_near(P(2, 3).rot270(), P(3, -2), "rot270");
    tr.point_near(P(1, 0).rotate(std::acos(-1.0) / 2), P(0, 1), "member rotate");
    tr.point_near(fgeo::rotate(P(1, 0), std::acos(-1.0) / 2), P(0, 1), "free rotate");
    tr.point_near(fgeo::rotate(P(2, 1), std::acos(-1.0) / 2, P(1, 1)), P(1, 2), "rotate center");
    tr.point_near(fgeo::unit_vector(std::acos(-1.0) / 2), P(0, 1), "unit_vector");
    tr.point_near(fgeo::polar(2.0, std::acos(-1.0)), P(-2, 0), "polar positive radius");
    tr.point_near(fgeo::polar(-2.0, 0.0), P(-2, 0), "polar negative radius");
    tr.near(fgeo::angle(P(1, 0), P(0, 0), P(0, 1)), std::acos(-1.0) / 2, "angle right");
    tr.near(fgeo::angle(P(0, 0), P(0, 0), P(1, 0)), 0, "angle degenerate");
    tr.near(fgeo::manhattan_dist(P(1, 2), P(4, -2)), 7, "manhattan");
    tr.near(fgeo::chebyshev_dist(P(1, 2), P(4, -2)), 4, "chebyshev");
    auto box = fgeo::bounding_box(std::vector<P>{P(1, 2), P(-3, 4), P(2, -5)});
    tr.check(box.has_value(), "bounding_box exists");
    tr.point_near(box->first, P(-3, -5), "bounding_box min");
    tr.point_near(box->second, P(2, 4), "bounding_box max");
    tr.check(!fgeo::bounding_box(std::vector<P>{}).has_value(), "bounding_box empty");
    tr.near(fgeo::max_manhattan_dist(std::vector<P>{P(0, 0), P(2, 3), P(-1, 4)}), 5, "max manhattan");
    tr.near(fgeo::max_chebyshev_dist(std::vector<P>{P(0, 0), P(2, 3), P(-1, 4)}), 4, "max chebyshev");
    tr.near(fgeo::max_manhattan_dist(std::vector<P>{}), 0, "max manhattan empty");
    tr.near(fgeo::max_chebyshev_dist(std::vector<P>{P(1, 1)}), 0, "max chebyshev single");
    std::stringstream ss;
    ss << P(1.5, -2.5);
    tr.check(ss.str() == "[1.5,-2.5]", "ostream point");
    std::stringstream is("3.5 -4.5");
    P read;
    is >> read;
    tr.point_near(read, P(3.5, -4.5), "istream point");
    fgeo::Point<long double> ld(1.0L, 2.0L);
    (void)ld;
    std::cout << "[PASS] Point / EPS / utility / polar\n";
}

void test_orientation_and_arg(Tester& tr) {
    tr.check(fgeo::orient(P(0, 0), P(1, 0), P(0, 1)) == 1, "orient ccw");
    tr.check(fgeo::orient(P(0, 0), P(0, 1), P(1, 0)) == -1, "orient cw");
    tr.check(fgeo::orient(P(0, 0), P(1, 1), P(2, 2 + 1e-12)) == 0, "orient eps collinear");
    tr.check(fgeo::ccw(P(0, 0), P(1, 0), P(-1, 0)) == fgeo::ONLINE_BACK, "ccw back");
    tr.check(fgeo::ccw(P(0, 0), P(1, 0), P(2, 0)) == fgeo::ONLINE_FRONT, "ccw front");
    tr.check(fgeo::ccw(P(0, 0), P(2, 0), P(1, 0)) == fgeo::ON_SEGMENT, "ccw on segment");
    tr.check(fgeo::is_collinear(P(0, 0), P(1, 1), P(2, 2)), "is_collinear 3");
    tr.check(fgeo::is_collinear(std::vector<P>{}), "is_collinear empty");
    tr.check(fgeo::is_collinear(std::vector<P>{P(0, 0), P(1, 1), P(2, 2)}), "is_collinear set");
    tr.check(!fgeo::is_collinear(std::vector<P>{P(0, 0), P(1, 1), P(2, 3)}), "is_collinear set false");
    std::vector<P> ps{P(0, -1), P(1, 0), P(0, 1), P(-1, 0), P(0, 0)};
    fgeo::sort_by_arg(ps);
    tr.point_near(ps[0], P(0, 0), "arg zero first");
    tr.point_near(ps[1], P(1, 0), "arg first positive x");
    auto sorted = fgeo::arg_sort(std::vector<P>{P(0, 1), P(1, 0)});
    tr.point_near(sorted[0], P(1, 0), "arg_sort return");
    tr.check(fgeo::arg_less(P(1, 0), P(0, 1)), "arg_less direct");
    std::cout << "[PASS] orient / ccw / arg_sort\n";
}

void test_line_segment(Tester& tr) {
    L x(P(0, 0), P(2, 2));
    L y(P(0, 2), P(2, 0));
    L z(P(2, 2), P(4, 4));
    L zp(P(0, 1), P(2, 3));
    S s(P(0, 0), P(2, 0));
    S t(P(1, -1), P(1, 1));
    L default_line;
    S default_segment;
    fgeo::LineCoeff<T> default_coeff;
    fgeo::LineCoeff<T> manual_coeff(1, 2, 3);
    tr.point_near(default_line.a, P(), "default line a");
    tr.point_near(default_segment.a, P(), "default segment a");
    tr.near(default_coeff.a + default_coeff.b + default_coeff.c, 0, "default line coeff");
    tr.near(manual_coeff.a + manual_coeff.b + manual_coeff.c, 6, "manual line coeff");
    tr.point_near(x.vec(), P(2, 2), "line vec");
    tr.near(x.length2(), 8, "line length2");
    tr.near(x.length(), std::sqrt(8.0), "line length");
    tr.point_near(s.vec(), P(2, 0), "segment vec");
    tr.near(s.length2(), 4, "segment length2");
    tr.near(s.length(), 2, "segment length");
    tr.check(fgeo::is_parallel(x, z), "line parallel");
    tr.check(fgeo::is_parallel(S(P(0, 0), P(1, 0)), S(P(1, 1), P(2, 1))), "segment parallel");
    tr.check(fgeo::is_orthogonal(L(P(0, 0), P(1, 0)), L(P(0, 0), P(0, 2))), "line orthogonal");
    tr.check(fgeo::is_orthogonal(S(P(0, 0), P(1, 0)), S(P(0, 0), P(0, 2))), "segment orthogonal");
    tr.check(fgeo::on_line(x, P(3, 3 + 1e-12)), "on_line eps");
    tr.check(fgeo::on_line(L(P(1, 1), P(1, 1)), P(1, 1 + 1e-12)), "on_line degenerate");
    tr.check(fgeo::on_segment(s, P(1, 1e-12)), "on_segment eps");
    tr.check(!fgeo::on_segment(s, P(3, 0)), "on_segment outside");
    tr.check(fgeo::on_segment(S(P(1, 1), P(1, 1)), P(1, 1)), "on_segment degenerate");
    auto coeff = fgeo::line_coeff(L(P(0, 2), P(3, 2)));
    tr.near(coeff.a, 0, "line_coeff horizontal a");
    tr.near(coeff.b, 1, "line_coeff horizontal b");
    tr.near(coeff.c, -2, "line_coeff horizontal c");
    coeff = fgeo::line_coeff(L(P(3, 0), P(3, 4)));
    tr.near(coeff.a, 1, "line_coeff vertical a");
    tr.near(coeff.b, 0, "line_coeff vertical b");
    tr.near(coeff.c, -3, "line_coeff vertical c");
    coeff = fgeo::line_coeff(L(P(1, 1), P(1, 1)));
    tr.near(coeff.a, 0, "line_coeff degenerate");
    auto from_coeff = fgeo::line_from_coeff<T>(2, 0, -4);
    tr.check(from_coeff.has_value(), "line_from_coeff vertical exists");
    tr.check(fgeo::on_line(*from_coeff, P(2, 7)), "line_from_coeff vertical on");
    from_coeff = fgeo::line_from_coeff<T>(0, 3, -6);
    tr.check(from_coeff.has_value() && fgeo::on_line(*from_coeff, P(5, 2)), "line_from_coeff horizontal on");
    tr.check(!fgeo::line_from_coeff<T>(0, 0, 1).has_value(), "line_from_coeff degenerate none");
    tr.check(fgeo::same_line(L(P(0, 0), P(1, 1)), L(P(2, 2), P(3, 3))), "same_line true");
    tr.check(!fgeo::same_line(L(P(0, 0), P(1, 1)), L(P(0, 1), P(1, 2))), "same_line false parallel");
    L pl = fgeo::parallel_line(L(P(0, 0), P(1, 1)), P(0, 1));
    tr.check(fgeo::is_parallel(pl, L(P(0, 0), P(1, 1))) && fgeo::on_line(pl, P(0, 1)), "parallel_line");
    L vl = fgeo::perpendicular_line(L(P(0, 0), P(2, 0)), P(1, 1));
    tr.check(fgeo::is_orthogonal(vl, L(P(0, 0), P(2, 0))) && fgeo::on_line(vl, P(1, 1)), "perpendicular_line");
    L pb = fgeo::perpendicular_bisector(P(0, 0), P(2, 0));
    tr.check(fgeo::on_line(pb, P(1, 0)) && fgeo::is_orthogonal(pb, L(P(0, 0), P(2, 0))), "perpendicular_bisector");
    tr.near(fgeo::signed_distance(L(P(0, 0), P(2, 0)), P(0, 3)), 3, "signed_distance left");
    tr.near(fgeo::signed_distance(L(P(0, 0), P(2, 0)), P(0, -3)), -3, "signed_distance right");
    tr.near(fgeo::signed_distance(L(P(1, 1), P(1, 1)), P(4, 5)), 5, "signed_distance degenerate");
    tr.check(!fgeo::intersect(x, zp), "parallel different line no intersect");
    tr.near(fgeo::side_value(L(P(0, 0), P(1, 0)), P(0, 2)), 2, "side_value");
    tr.check(fgeo::side(L(P(0, 0), P(1, 0)), P(0, 2)) == 1, "side left");
    tr.check(fgeo::side(L(P(0, 0), P(1, 0)), P(0, -2)) == -1, "side right");
    tr.check(fgeo::intersect(x, y), "line intersect");
    tr.check(fgeo::intersect(x, z), "same line intersect");
    tr.check(fgeo::intersect(L(P(0, 0), P(1, 0)), S(P(0, 1), P(0, -1))), "line segment intersect");
    tr.check(fgeo::intersect(S(P(0, 1), P(0, -1)), L(P(0, 0), P(1, 0))), "segment line intersect");
    tr.check(!fgeo::intersect(L(P(0, 0), P(1, 0)), S(P(0, 1), P(1, 1))), "line segment no intersect");
    tr.check(fgeo::intersect(s, t), "segment intersect cross");
    tr.check(fgeo::intersect(S(P(1, 0), P(1, 0)), s), "degenerate segment intersect on segment");
    tr.check(!fgeo::intersect(S(P(3, 0), P(3, 0)), s), "degenerate segment no intersect");
    tr.check(fgeo::intersect(S(P(0, 0), P(1, 0)), S(P(1, 0), P(2, 0))), "segment endpoint touch");
    tr.check(fgeo::intersect(S(P(0, 0), P(2, 0)), S(P(1, 0), P(3, 0))), "segment overlap");
    tr.check(!fgeo::intersect(S(P(0, 0), P(1, 0)), S(P(2, 0), P(3, 0))), "segment disjoint collinear");
    tr.check(fgeo::proper_intersect(S(P(0, 0), P(2, 2)), S(P(0, 2), P(2, 0))), "proper intersect true");
    tr.check(!fgeo::proper_intersect(S(P(0, 0), P(1, 0)), S(P(1, 0), P(2, 0))), "proper intersect endpoint false");
    auto ov = fgeo::overlap_segment(S(P(0, 0), P(2, 0)), S(P(1, 0), P(3, 0)));
    tr.check(ov.has_value(), "overlap exists");
    tr.point_near(ov->a, P(1, 0), "overlap a");
    tr.point_near(ov->b, P(2, 0), "overlap b");
    ov = fgeo::overlap_segment(S(P(0, 0), P(1, 0)), S(P(1, 0), P(2, 0)));
    tr.check(ov.has_value() && fgeo::same_point(ov->a, ov->b), "overlap one point");
    ov = fgeo::overlap_segment(S(P(0, 0), P(1, 0)), S(P(2, 0), P(3, 0)));
    tr.check(!ov.has_value(), "overlap none");
    auto cp = fgeo::cross_point(x, y);
    tr.check(cp.has_value(), "line cross_point exists");
    tr.point_near(*cp, P(1, 1), "line cross_point value");
    tr.check(!fgeo::cross_point(x, z).has_value(), "parallel cross_point none");
    cp = fgeo::cross_point(L(P(0, 0), P(1, 0)), t);
    tr.check(cp.has_value(), "line segment cross_point");
    tr.point_near(*cp, P(1, 0), "line segment cross_point value");
    cp = fgeo::cross_point(t, L(P(0, 0), P(1, 0)));
    tr.check(cp.has_value(), "segment line cross_point");
    cp = fgeo::cross_point(s, t);
    tr.check(cp.has_value(), "segment cross_point");
    tr.point_near(*cp, P(1, 0), "segment cross_point value");
    tr.check(!fgeo::cross_point(S(P(0, 0), P(2, 0)), S(P(1, 0), P(3, 0))).has_value(), "overlap cross_point none");
    cp = fgeo::cross_point(S(P(1, 0), P(1, 0)), s);
    tr.check(cp.has_value() && fgeo::same_point(*cp, P(1, 0)), "degenerate segment cross_point");
    tr.point_near(fgeo::project(L(P(0, 0), P(0, 0)), P(1, 3)), P(0, 0), "project degenerate line");
    tr.point_near(fgeo::project(L(P(0, 0), P(2, 0)), P(1, 3)), P(1, 0), "project line");
    tr.point_near(fgeo::project(S(P(0, 0), P(2, 0)), P(1, 3)), P(1, 0), "project segment line");
    tr.point_near(fgeo::closest_point(L(P(0, 0), P(2, 0)), P(1, 3)), P(1, 0), "closest_point line");
    tr.point_near(fgeo::closest_point(S(P(0, 0), P(2, 0)), P(1, 3)), P(1, 0), "closest_point segment middle");
    tr.point_near(fgeo::closest_point(S(P(0, 0), P(2, 0)), P(3, 3)), P(2, 0), "closest_point segment endpoint");
    tr.point_near(fgeo::closest_point(S(P(2, 2), P(2, 2)), P(5, 5)), P(2, 2), "closest_point degenerate segment");
    tr.point_near(fgeo::reflect(L(P(0, 0), P(2, 0)), P(1, 3)), P(1, -3), "reflect line");
    tr.point_near(fgeo::reflect(S(P(0, 0), P(2, 0)), P(1, 3)), P(1, -3), "reflect segment line");
    tr.near(fgeo::distance(L(P(0, 0), P(0, 0)), P(3, 4)), 5, "distance degenerate line point");
    tr.near(fgeo::distance(L(P(0, 0), P(2, 0)), P(1, 3)), 3, "distance line point");
    tr.near(fgeo::distance(P(1, 3), L(P(0, 0), P(2, 0))), 3, "distance point line");
    tr.near(fgeo::distance(S(P(0, 0), P(2, 0)), P(3, 4)), std::sqrt(17.0), "distance segment point endpoint");
    tr.near(fgeo::distance(P(1, 3), S(P(0, 0), P(2, 0))), 3, "distance point segment");
    tr.near(fgeo::distance(S(P(0, 0), P(1, 0)), S(P(2, 0), P(3, 0))), 1, "distance segment segment");
    tr.near(fgeo::distance(L(P(0, 0), P(1, 0)), L(P(0, 2), P(1, 2))), 2, "distance line line");
    tr.near(fgeo::distance(L(P(0, 0), P(1, 0)), S(P(0, 2), P(1, 2))), 2, "distance line segment");
    tr.near(fgeo::distance(S(P(0, 2), P(1, 2)), L(P(0, 0), P(1, 0))), 2, "distance segment line");
    std::cout << "[PASS] Line / Segment / intersection / distance / derived lines\n";
}

void test_circle_and_centers(Tester& tr) {
    C default_circle;
    tr.point_near(default_circle.c, P(), "default circle center");
    tr.near(default_circle.r, 0, "default circle radius");
    C c(P(0, 0), 5);
    tr.near(fgeo::radius2(c), 25, "radius2");
    tr.near(fgeo::area(c), 25 * std::acos(-1.0), "circle area");
    tr.near(fgeo::circumference(C(P(0, 0), 2)), 4 * std::acos(-1.0), "circle circumference");
    tr.near(fgeo::power(C(P(0, 0), 5), P(6, 0)), 11, "circle power outside");
    tr.near(fgeo::power(C(P(0, 0), 5), P(3, 4)), 0, "circle power on");
    L ra = fgeo::radical_axis(C(P(0, 0), 1), C(P(2, 0), 1));
    tr.check(fgeo::on_line(ra, P(1, 0)) && fgeo::is_parallel(ra, L(P(1, 0), P(1, 1))), "radical_axis equal radii");
    L ra_same_center = fgeo::radical_axis(C(P(0, 0), 1), C(P(0, 0), 2));
    tr.check(fgeo::same_point(ra_same_center.a, ra_same_center.b), "radical_axis concentric degenerate");
    tr.near(fgeo::intersection_area(C(P(0, 0), 1), C(P(3, 0), 1)), 0, "circle intersection area separate");
    tr.near(fgeo::intersection_area(C(P(0, 0), 3), C(P(1, 0), 1)), std::acos(-1.0), "circle intersection area contain");
    tr.near(fgeo::intersection_area(C(P(0, 0), 1), C(P(0, 0), 1)), std::acos(-1.0), "circle intersection area same");
    tr.near(fgeo::intersection_area(C(P(0, 0), 1), C(P(1, 0), 1)), 2 * std::acos(0.5) - std::sqrt(3.0) / 2, "circle intersection area lens", 1e-8);
    tr.check(fgeo::contains(c, P(0, 0)) == fgeo::INSIDE, "circle contains inside");
    tr.check(fgeo::contains(c, P(5, 0)) == fgeo::ON_EDGE, "circle contains on");
    tr.check(fgeo::contains(c, P(6, 0)) == fgeo::OUTSIDE, "circle contains outside");
    tr.check(fgeo::circle_relation(C(P(0, 0), 1), C(P(3, 0), 1)) == fgeo::CIRCLE_SEPARATE, "circle separate");
    tr.check(fgeo::circle_relation(C(P(0, 0), 1), C(P(2, 0), 1)) == fgeo::CIRCLE_EXTERNAL_TANGENT, "circle external tangent");
    tr.check(fgeo::circle_relation(C(P(0, 0), 2), C(P(2, 0), 2)) == fgeo::CIRCLE_INTERSECT, "circle intersect relation");
    tr.check(fgeo::circle_relation(C(P(0, 0), 3), C(P(2, 0), 1)) == fgeo::CIRCLE_INTERNAL_TANGENT, "circle internal tangent");
    tr.check(fgeo::circle_relation(C(P(0, 0), 5), C(P(1, 0), 1)) == fgeo::CIRCLE_CONTAIN, "circle contain");
    tr.check(fgeo::circle_relation(C(P(0, 0), 5), C(P(0, 0), 5)) == fgeo::CIRCLE_SAME, "circle same");
    tr.check(fgeo::intersect(c, L(P(-10, 0), P(10, 0))), "circle line intersect");
    tr.check(fgeo::intersect(c, L(P(-10, 5), P(10, 5))), "circle line tangent");
    tr.check(!fgeo::intersect(c, L(P(-10, 6), P(10, 6))), "circle line no");
    tr.check(fgeo::intersect(c, S(P(-10, 0), P(10, 0))), "circle segment cross");
    tr.check(!fgeo::intersect(c, S(P(-1, 0), P(1, 0))), "circle segment inside only false");
    tr.check(fgeo::intersect_disk(c, S(P(-1, 0), P(1, 0))), "circle disk segment inside true");
    tr.check(fgeo::intersect(S(P(-10, 0), P(10, 0)), c), "segment circle symmetric");
    tr.check(fgeo::intersect(C(P(0, 0), 1), C(P(2, 0), 1)), "circle circle tangent intersect");
    tr.check(!fgeo::intersect(C(P(0, 0), 5), C(P(1, 0), 1)), "circle circle contain no circumference intersection");
    auto ps = fgeo::cross_points(c, L(P(-10, 0), P(10, 0)));
    tr.check(ps.size() == 2, "circle line two points");
    tr.point_near(ps[0], P(-5, 0), "circle line point 0");
    tr.point_near(ps[1], P(5, 0), "circle line point 1");
    ps = fgeo::cross_points(C(P(0, 0), 1), L(P(1, 0), P(1, 0)));
    tr.check(ps.size() == 1 && fgeo::same_point(ps[0], P(1, 0)), "circle degenerate line point");
    ps = fgeo::cross_points(c, L(P(-10, 5), P(10, 5)));
    tr.check(ps.size() == 1, "circle line tangent one");
    tr.point_near(ps[0], P(0, 5), "circle line tangent point");
    ps = fgeo::cross_points(c, S(P(0, 0), P(10, 0)));
    tr.check(ps.size() == 1, "circle segment one");
    tr.point_near(ps[0], P(5, 0), "circle segment point");
    ps = fgeo::cross_points(C(P(0, 0), 5), C(P(8, 0), 5));
    tr.check(ps.size() == 2, "circle circle two points");
    for (auto p : ps) {
        tr.near(p.dist(P(0, 0)), 5, "circle circle point on a");
        tr.near(p.dist(P(8, 0)), 5, "circle circle point on b");
    }
    ps = fgeo::cross_points(C(P(0, 0), 1), C(P(2, 0), 1));
    tr.check(ps.size() == 1, "circle circle tangent one point");
    tr.point_near(ps[0], P(1, 0), "circle circle tangent point");
    ps = fgeo::cross_points(C(P(0, 0), 1), C(P(4, 0), 1));
    tr.check(ps.empty(), "circle circle no points");
    ps = fgeo::tangent_points(C(P(0, 0), 1), P(2, 0));
    tr.check(ps.size() == 2, "tangent points outside");
    for (auto p : ps) tr.near(p.norm(), 1, "tangent point on circle");
    ps = fgeo::tangent_points(C(P(0, 0), 1), P(1, 0));
    tr.check(ps.size() == 1, "tangent points on circle");
    ps = fgeo::tangent_points(C(P(0, 0), 1), P(0, 0));
    tr.check(ps.empty(), "tangent points inside");
    auto lines = fgeo::tangent_lines(C(P(0, 0), 1), P(2, 0));
    tr.check(lines.size() == 2, "tangent lines outside");
    for (auto line : lines) tr.near(fgeo::distance(line, P(0, 0)), 1, "tangent line distance");
    lines = fgeo::tangent_lines(C(P(0, 0), 1), P(1, 0));
    tr.check(lines.size() == 1, "tangent lines on circle");
    lines = fgeo::common_tangent_lines(C(P(0, 0), 1), C(P(4, 0), 1));
    tr.check(lines.size() == 4, "common tangent lines four");
    for (auto line : lines) {
        tr.near(fgeo::distance(line, P(0, 0)), 1, "common tangent dist a", 1e-7);
        tr.near(fgeo::distance(line, P(4, 0)), 1, "common tangent dist b", 1e-7);
    }
    lines = fgeo::tangent_lines(C(P(0, 0), 1), C(P(2, 0), 1));
    tr.check(lines.size() == 3, "common tangent external tangent three");
    for (auto line : lines) {
        tr.near(fgeo::distance(line, P(0, 0)), 1, "external tangent distance a", 1e-7);
        tr.near(fgeo::distance(line, P(2, 0)), 1, "external tangent distance b", 1e-7);
    }
    lines = fgeo::common_tangent_lines(C(P(0, 0), 2), C(P(3, 0), 1));
    tr.check(lines.size() == 3, "common tangent external tangent different radii three");
    lines = fgeo::common_tangent_lines(C(P(0, 0), 3), C(P(2, 0), 1));
    tr.check(lines.size() == 1, "common tangent internal tangent one");
    tr.near(fgeo::distance(lines[0], P(0, 0)), 3, "internal tangent distance a", 1e-7);
    tr.near(fgeo::distance(lines[0], P(2, 0)), 1, "internal tangent distance b", 1e-7);
    lines = fgeo::common_tangent_lines(C(P(0, 0), 3), C(P(1, 0), 1));
    tr.check(lines.empty(), "common tangent contain zero");
    auto cc = fgeo::circumcenter(P(0, 0), P(2, 0), P(0, 2));
    tr.check(cc.has_value(), "circumcenter exists");
    tr.point_near(*cc, P(1, 1), "circumcenter value");
    tr.check(!fgeo::circumcenter(P(0, 0), P(1, 1), P(2, 2)).has_value(), "circumcenter collinear none");
    auto cir = fgeo::circumcircle(P(0, 0), P(2, 0), P(0, 2));
    tr.check(cir.has_value(), "circumcircle exists");
    tr.point_near(cir->c, P(1, 1), "circumcircle center");
    tr.near(cir->r, std::sqrt(2.0), "circumcircle radius");
    auto ic = fgeo::incenter(P(0, 0), P(2, 0), P(0, 2));
    tr.check(ic.has_value(), "incenter exists");
    tr.point_near(*ic, P(2 - std::sqrt(2.0), 2 - std::sqrt(2.0)), "incenter value");
    tr.check(!fgeo::incenter(P(0, 0), P(1, 1), P(2, 2)).has_value(), "incenter collinear none");
    auto inc = fgeo::incircle(P(0, 0), P(2, 0), P(0, 2));
    tr.check(inc.has_value(), "incircle exists");
    tr.near(inc->r, 2 - std::sqrt(2.0), "incircle radius");
    tr.point_near(fgeo::triangle_centroid(P(0, 0), P(3, 0), P(0, 3)), P(1, 1), "triangle_centroid");
    auto oh = fgeo::orthocenter(P(0, 0), P(2, 0), P(0, 2));
    tr.check(oh.has_value(), "orthocenter exists");
    tr.point_near(*oh, P(0, 0), "orthocenter right triangle");
    tr.check(!fgeo::orthocenter(P(0, 0), P(1, 1), P(2, 2)).has_value(), "orthocenter collinear none");
    std::cout << "[PASS] Circle / tangents / triangle centers / circle extras\n";
}

void test_polygon_and_points(Tester& tr) {
    Poly tri{P(0, 0), P(4, 0), P(0, 3)};
    tr.near(fgeo::signed_area2(P(0, 0), P(4, 0), P(0, 3)), 12, "triangle signed_area2");
    tr.near(fgeo::area2(P(0, 0), P(4, 0), P(0, 3)), 12, "triangle area2");
    tr.near(fgeo::area(P(0, 0), P(4, 0), P(0, 3)), 6, "triangle area");
    tr.near(fgeo::signed_area2(tri), 12, "polygon signed_area2");
    tr.near(fgeo::area2(tri), 12, "polygon area2");
    tr.near(fgeo::area(tri), 6, "polygon area");
    tr.near(fgeo::perimeter(tri), 12, "perimeter");
    Poly rect{P(0, 0), P(4, 0), P(4, 3), P(0, 3)};
    tr.check(fgeo::contains(rect, P(2, 1)) == fgeo::INSIDE, "polygon contains inside");
    tr.check(fgeo::contains(rect, P(4, 1)) == fgeo::ON_EDGE, "polygon contains edge");
    tr.check(fgeo::contains(rect, P(5, 1)) == fgeo::OUTSIDE, "polygon contains outside");
    Poly concave{P(0, 0), P(4, 0), P(2, 1), P(4, 4), P(0, 4)};
    tr.check(fgeo::contains(concave, P(2, 0.5)) == fgeo::INSIDE, "concave contains inside");
    tr.check(fgeo::contains(concave, P(3, 1.5)) == fgeo::OUTSIDE, "concave contains outside dent");
    tr.check(fgeo::is_convex(rect), "is_convex rect");
    tr.check(!fgeo::is_convex(concave), "is_convex concave false");
    tr.check(fgeo::is_convex(Poly{P(0, 0), P(1, 0), P(2, 0), P(2, 1), P(0, 1)}, false), "is_convex non-strict collinear");
    tr.check(!fgeo::is_convex(Poly{P(0, 0), P(1, 0), P(2, 0), P(2, 1), P(0, 1)}, true), "is_convex strict collinear false");
    Poly comp = fgeo::compress_collinear(Poly{P(0, 0), P(1, 0), P(2, 0), P(2, 2), P(0, 2)});
    tr.check(comp.size() == 4, "compress_collinear size");
    auto hull = fgeo::convex_hull(std::vector<P>{P(0, 0), P(1, 1), P(0, 2), P(2, 0), P(2, 2), P(1, 1)});
    tr.check(hull.size() == 4, "convex_hull size");
    tr.near(fgeo::area(hull), 4, "convex_hull area");
    auto hull_line = fgeo::convex_hull(std::vector<P>{P(0, 0), P(1, 0), P(2, 0)}, true);
    tr.check(hull_line.size() == 3, "convex_hull keep collinear line");
    auto hull_keep = fgeo::convex_hull(std::vector<P>{P(0, 0), P(1, 0), P(2, 0), P(2, 2), P(0, 2)}, true);
    tr.check(hull_keep.size() == 5, "convex_hull keep collinear edge");
    tr.check(fgeo::is_simple_polygon(rect), "simple polygon true");
    tr.check(!fgeo::is_simple_polygon(Poly{P(0, 0), P(2, 2), P(0, 2), P(2, 0)}), "simple polygon crossing false");
    tr.check(!fgeo::is_simple_polygon(Poly{P(0, 0), P(1, 0), P(1, 0), P(0, 1)}), "simple polygon zero edge false");
    tr.check(fgeo::convex_contains(rect, P(2, 1)) == fgeo::INSIDE, "convex_contains inside");
    tr.check(fgeo::convex_contains(rect, P(4, 1)) == fgeo::ON_EDGE, "convex_contains edge");
    Poly cut = fgeo::convex_cut(rect, L(P(2, -1), P(2, 4)));
    tr.near(fgeo::area(cut), 6, "convex_cut area left half");
    for (auto p : cut) tr.check(fgeo::side(L(P(2, -1), P(2, 4)), p) >= 0, "convex_cut side");
    Poly rect2{P(2, -1), P(6, -1), P(6, 2), P(2, 2)};
    Poly inter = fgeo::convex_intersection(rect, rect2);
    tr.near(fgeo::area(inter), 4, "convex_intersection overlap area");
    tr.near(fgeo::convex_intersection_area(rect, rect2), 4, "convex_intersection_area overlap");
    Poly rect2_cw = rect2;
    std::reverse(rect2_cw.begin(), rect2_cw.end());
    tr.near(fgeo::convex_intersection_area(rect, rect2_cw), 4, "convex_intersection cw input");
    for (auto p : inter) {
        tr.check(fgeo::convex_contains(rect, p) != fgeo::OUTSIDE, "convex_intersection point in a");
        tr.check(fgeo::convex_contains(rect2, p) != fgeo::OUTSIDE, "convex_intersection point in b");
    }
    Poly rect_disjoint{P(5, 0), P(6, 0), P(6, 1), P(5, 1)};
    tr.check(fgeo::convex_intersection(rect, rect_disjoint).empty(), "convex_intersection disjoint empty");
    tr.near(fgeo::convex_intersection_area(rect, rect_disjoint), 0, "convex_intersection_area disjoint");
    Poly rect_inside{P(1, 1), P(2, 1), P(2, 2), P(1, 2)};
    tr.near(fgeo::area(fgeo::convex_intersection(rect, rect_inside)), 1, "convex_intersection contain area");
    Poly rect_touch_edge{P(4, 1), P(5, 1), P(5, 2), P(4, 2)};
    inter = fgeo::convex_intersection(rect, rect_touch_edge);
    tr.check(!inter.empty(), "convex_intersection edge touch nonempty");
    tr.near(fgeo::area(inter), 0, "convex_intersection edge touch area zero");
    Poly rect_touch_point{P(4, 3), P(5, 3), P(5, 4), P(4, 4)};
    inter = fgeo::convex_intersection(rect, rect_touch_point);
    tr.check(!inter.empty(), "convex_intersection point touch nonempty");
    tr.near(fgeo::area(inter), 0, "convex_intersection point touch area zero");
    tr.check(fgeo::convex_contains_log(rect, P(2, 1)) == fgeo::INSIDE, "convex_contains_log inside");
    tr.check(fgeo::convex_contains_log(rect, P(4, 1)) == fgeo::ON_EDGE, "convex_contains_log edge");
    tr.check(fgeo::convex_contains_log(rect, P(5, 1)) == fgeo::OUTSIDE, "convex_contains_log outside");
    tr.check(fgeo::convex_contains_log(Poly{}, P()) == fgeo::OUTSIDE, "convex_contains_log empty");
    tr.check(fgeo::convex_contains_log(Poly{P(1, 1)}, P(1, 1)) == fgeo::ON_EDGE, "convex_contains_log point");
    tr.check(fgeo::convex_contains_log(Poly{P(0, 0), P(2, 0)}, P(1, 0)) == fgeo::ON_EDGE, "convex_contains_log segment");
    std::vector<L> hs{L(P(0, 1), P(0, 0)), L(P(2, 0), P(2, 1)), L(P(0, 0), P(1, 0)), L(P(1, 1), P(0, 1))};
    Poly hp = fgeo::half_plane_intersection(hs, T(10));
    tr.near(fgeo::area(hp), 2, "half_plane_intersection bounded area");
    hp = fgeo::half_plane_intersection(std::vector<L>{L(P(0, 1), P(0, 0)), L(P(-1, 0), P(-1, 1))}, T(10));
    tr.check(hp.empty(), "half_plane_intersection empty");
    hp = fgeo::half_plane_intersection(std::vector<L>{L(P(0, 1), P(0, 0))}, T(5));
    tr.near(fgeo::area(hp), 50, "half_plane_intersection unbounded clipped");
    tr.check(fgeo::intersect(rect, S(P(-1, 1), P(1, 1))), "polygon segment intersect crossing");
    tr.check(fgeo::intersect(S(P(1, 1), P(2, 1)), rect), "segment polygon intersect inside");
    tr.check(!fgeo::intersect(rect, S(P(5, 5), P(6, 6))), "polygon segment intersect false");
    tr.check(fgeo::intersect(rect, rect2), "polygon polygon intersect overlap");
    tr.check(fgeo::intersect(rect, rect_touch_edge), "polygon polygon intersect edge touch");
    tr.check(!fgeo::intersect(rect, rect_disjoint), "polygon polygon intersect false");
    tr.near(fgeo::distance(rect, P(2, 1)), 0, "distance polygon point inside");
    tr.near(fgeo::distance(P(5, 1), rect), 1, "distance point polygon outside");
    tr.check(std::isinf(fgeo::distance(Poly{}, P(0, 0))), "distance empty polygon point inf");
    tr.near(fgeo::distance(rect, S(P(5, 1), P(6, 1))), 1, "distance polygon segment outside");
    tr.near(fgeo::distance(S(P(-1, 1), P(1, 1)), rect), 0, "distance segment polygon crossing");
    tr.near(fgeo::distance(rect, rect_disjoint), 1, "distance polygon polygon outside");
    tr.near(fgeo::distance(rect, rect_touch_edge), 0, "distance polygon polygon touching");
    tr.check(fgeo::support_index(rect, P(1, 1)) == 2, "support_index");
    tr.near(fgeo::support_value(rect, P(1, 1)), 7, "support_value");
    tr.point_near(fgeo::support_point(rect, P(1, 1)), P(4, 3), "support_point");
    tr.check(fgeo::support_index(std::vector<P>{}, P(1, 0)) == -1, "support_index empty");
    tr.check(std::isinf(fgeo::support_value(std::vector<P>{}, P(1, 0))), "support_value empty inf");
    tr.point_near(fgeo::support_point(std::vector<P>{}, P(1, 0)), P(), "support_point empty");
    tr.near(fgeo::width(rect, P(1, 0)), 4, "width x");
    tr.near(fgeo::width(rect, P(0, 1)), 3, "width y");
    tr.near(fgeo::width(rect, P(1, 1)), 7 / std::sqrt(2.0), "width diagonal");
    tr.near(fgeo::width(std::vector<P>{}, P(1, 0)), 0, "width empty");
    tr.near(fgeo::width(rect, P(0, 0)), 0, "width zero dir");
    auto pair = fgeo::convex_diameter_pair(rect);
    tr.check(pair.first >= 0 && pair.second >= 0, "convex_diameter_pair exists");
    tr.near(fgeo::convex_diameter2(rect), 25, "convex_diameter2");
    tr.near(fgeo::convex_diameter(rect), 5, "convex_diameter");
    tr.near(fgeo::diameter2(rect), 25, "diameter2");
    tr.near(fgeo::diameter(rect), 5, "diameter");
    std::vector<P> cps{P(0, 0), P(2, 0), P(5, 0), P(5, 1)};
    tr.near(fgeo::closest_pair2(cps), 1, "closest_pair2");
    tr.near(fgeo::closest_pair_dist(cps), 1, "closest_pair_dist");
    tr.check(std::isinf(fgeo::closest_pair2(std::vector<P>{P(0, 0)})), "closest_pair2 single inf");
    tr.check(std::isinf(fgeo::closest_pair_dist(std::vector<P>{P(0, 0)})), "closest_pair_dist single inf");
    auto cp = fgeo::closest_pair(cps);
    tr.near(cp.first.dist2(cp.second), 1, "closest_pair pair");
    auto cp_empty = fgeo::closest_pair(std::vector<P>{});
    tr.point_near(cp_empty.first, P(), "closest_pair empty first");
    tr.point_near(fgeo::vertex_mean(rect), P(2, 1.5), "vertex_mean");
    tr.point_near(fgeo::vertex_mean(std::vector<P>{}), P(), "vertex_mean empty");
    tr.point_near(fgeo::polygon_centroid(rect), P(2, 1.5), "polygon_centroid rect");
    tr.point_near(fgeo::polygon_centroid(Poly{P(0, 0), P(1, 0), P(2, 0)}), P(1, 0), "polygon_centroid degenerate fallback");
    tr.check(fgeo::bottom_left_index(Poly{P(1, 1), P(0, 2), P(-1, 1)}) == 2, "bottom_left_index");
    tr.check(fgeo::minkowski_sum_convex(Poly{}, rect).empty(), "minkowski_sum empty");
    Poly norm = fgeo::normalized_polygon(Poly{P(4, 3), P(0, 3), P(0, 0), P(4, 0)}, true);
    tr.point_near(norm[0], P(0, 0), "normalized_polygon start");
    tr.check(fgeo::signed_area2(norm) > 0, "normalized_polygon ccw");
    Poly norm_cw = fgeo::normalized_polygon(rect, false);
    tr.check(fgeo::signed_area2(norm_cw) < 0, "normalized_polygon cw");
    fgeo::rotate_to_bottom_left(norm);
    tr.point_near(norm[0], P(0, 0), "rotate_to_bottom_left");
    Poly ms = fgeo::minkowski_sum_convex(Poly{P(0, 0), P(1, 0), P(0, 1)}, Poly{P(0, 0), P(2, 0), P(0, 2)});
    tr.near(fgeo::area(ms), 4.5, "minkowski_sum area");
    auto mec = fgeo::minimum_enclosing_circle(std::vector<P>{P(0, 0), P(2, 0), P(0, 2)});
    tr.check(mec.has_value(), "minimum_enclosing_circle exists");
    tr.point_near(mec->c, P(1, 1), "minimum_enclosing_circle center");
    tr.near(mec->r, std::sqrt(2.0), "minimum_enclosing_circle radius");
    tr.check(!fgeo::minimum_enclosing_circle(std::vector<P>{}).has_value(), "minimum_enclosing_circle empty");
    mec = fgeo::minimum_enclosing_circle(std::vector<P>{P(0, 0), P(2, 0)});
    tr.check(mec.has_value(), "minimum_enclosing_circle two exists");
    tr.point_near(mec->c, P(1, 0), "minimum_enclosing_circle two center");
    tr.near(mec->r, 1, "minimum_enclosing_circle two radius");
    mec = fgeo::minimum_enclosing_circle(std::vector<P>{P(3, 4)});
    tr.check(mec.has_value(), "minimum_enclosing_circle single exists");
    tr.point_near(mec->c, P(3, 4), "minimum_enclosing_circle single center");
    tr.near(mec->r, 0, "minimum_enclosing_circle single radius");
    std::cout << "[PASS] Polygon / hull / point set / polygon extras / MEC\n";
}

void test_incremental_and_impact(Tester& tr) {
    fgeo::IncrementalConvexHull<T> ich;
    tr.check(ich.empty(), "incremental empty");
    tr.check(ich.side(P(0, 0)) == -1, "incremental side empty");
    ich.add(P(0, 0));
    tr.check(!ich.empty(), "incremental nonempty");
    tr.check(ich.size() == 1, "incremental size 1");
    tr.check(ich.side(P(0, 0)) == 0, "incremental side point on");
    tr.check(ich.side(P(1, 0)) == -1, "incremental side point out");
    ich.add(P(2, 0));
    tr.check(ich.size() == 2, "incremental size 2");
    tr.check(ich.side(P(1, 0)) == 0, "incremental side segment on");
    ich.add(P(2, 2));
    ich.add(P(0, 2));
    ich.add(P(1, 1));
    tr.check(ich.size() == 4, "incremental size square");
    tr.check(ich.side(P(1, 1)) == 1, "incremental side inside");
    tr.check(ich.side(P(0, 1)) == 0, "incremental side edge");
    tr.check(ich.side(P(3, 1)) == -1, "incremental side outside");
    tr.near(ich.area2(), 8, "incremental area2");
    tr.near(ich.area(), 4, "incremental area");
    auto h = ich.hull();
    tr.check(h.size() == 4, "incremental hull get");
    fgeo::IncrementalConvexHull<T, false> ich2;
    ich2.add(P(0, 0));
    ich2.add(P(1, 0));
    ich2.add(P(2, 0));
    tr.check(ich2.size() == 3, "incremental keep collinear non-strict");

    auto [ok, time, pos] = fgeo::point_of_impact(P(0, 0), 1.0, P(10, 0), P(0, 0));
    tr.check(ok, "impact stationary ok");
    tr.near(time, 10, "impact stationary time");
    tr.point_near(pos, P(10, 0), "impact stationary pos");
    std::tie(ok, time, pos) = fgeo::point_of_impact(P(0, 0), 2.0, P(10, 0), P(1, 0));
    tr.check(ok, "impact moving ok");
    tr.near(time, 10, "impact moving time");
    tr.point_near(pos, P(20, 0), "impact moving pos");
    std::tie(ok, time, pos) = fgeo::point_of_impact(P(0, 0), 1.0, P(10, 0), P(2, 0));
    tr.check(!ok, "impact impossible");
    std::tie(ok, time, pos) = fgeo::point_of_impact(P(0, 0), 1.0, P(0, 0), P(5, 5));
    tr.check(ok && fgeo::eq(time, 0.0), "impact same position time zero");
    std::cout << "[PASS] IncrementalConvexHull / point_of_impact\n";
}

void test_randomized(Tester& tr) {
    std::mt19937 rng(1);
    std::uniform_real_distribution<T> distv(-50.0, 50.0);
    for (int tc = 0; tc < 200; ++tc) {
        int n = 2 + (rng() % 30);
        std::vector<P> ps;
        for (int i = 0; i < n; ++i) ps.emplace_back(distv(rng), distv(rng));
        T got_closest = fgeo::closest_pair2(ps);
        T expected_closest = brute_closest2(ps);
        tr.near(got_closest, expected_closest, "random closest_pair2", 1e-7);
        T got_diameter = fgeo::diameter2(ps);
        T expected_diameter = brute_diameter2(ps);
        tr.near(got_diameter, expected_diameter, "random diameter2", 1e-7);
        T man = fgeo::max_manhattan_dist(ps);
        T brute_man = 0;
        T cheb = fgeo::max_chebyshev_dist(ps);
        T brute_cheb = 0;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                brute_man = std::max(brute_man, fgeo::manhattan_dist(ps[i], ps[j]));
                brute_cheb = std::max(brute_cheb, fgeo::chebyshev_dist(ps[i], ps[j]));
            }
        }
        tr.near(man, brute_man, "random max_manhattan", 1e-8);
        tr.near(cheb, brute_cheb, "random max_chebyshev", 1e-8);
        auto h = fgeo::convex_hull(ps);
        for (const auto& p : ps) tr.check(fgeo::convex_contains(h, p) != fgeo::OUTSIDE, "random hull contains all points");
        if (h.size() >= 3) {
            tr.check(fgeo::is_convex(h), "random hull convex");
            P q(distv(rng), distv(rng));
            tr.check(fgeo::convex_contains_log(h, q) == fgeo::convex_contains(h, q), "random convex_contains_log matches linear");
            tr.near(fgeo::convex_intersection_area(h, h), fgeo::area(h), "random convex_intersection self area", 1e-7);
        }
    }

    for (int tc = 0; tc < 100; ++tc) {
        P a(distv(rng), distv(rng));
        P b(distv(rng), distv(rng));
        P c(distv(rng), distv(rng));
        P d(distv(rng), distv(rng));
        L l1(a, b);
        L l2(c, d);
        auto cp = fgeo::cross_point(l1, l2);
        if (cp) {
            tr.check(fgeo::on_line(l1, *cp), "random line cp on l1");
            tr.check(fgeo::on_line(l2, *cp), "random line cp on l2");
        }
        C cir(a, std::abs(distv(rng)) + 1.0);
        auto cps = fgeo::cross_points(cir, l2);
        for (const auto& p : cps) {
            tr.near(p.dist(cir.c), cir.r, "random circle line point on circle", 1e-7);
            tr.check(fgeo::on_line(l2, p), "random circle line point on line");
        }
        C cir2(c, std::abs(distv(rng)) + 1.0);
        cps = fgeo::cross_points(cir, cir2);
        for (const auto& p : cps) {
            tr.near(p.dist(cir.c), cir.r, "random circle circle point on a", 1e-7);
            tr.near(p.dist(cir2.c), cir2.r, "random circle circle point on b", 1e-7);
        }
    }

    for (int tc = 0; tc < 100; ++tc) {
        std::vector<P> ps;
        int n = 1 + (rng() % 20);
        fgeo::IncrementalConvexHull<T> ich;
        for (int i = 0; i < n; ++i) {
            P p(distv(rng), distv(rng));
            ps.push_back(p);
            ich.add(p);
            auto h = fgeo::convex_hull(ps);
            tr.near(ich.area2(), fgeo::area2(h), "random incremental area2", 1e-7);
            tr.check(ich.size() == static_cast<int>(h.size()), "random incremental size");
        }
    }

    for (int tc = 0; tc < 100; ++tc) {
        int n = 1 + (rng() % 30);
        std::vector<P> ps;
        for (int i = 0; i < n; ++i) ps.emplace_back(distv(rng), distv(rng));
        auto mec = fgeo::minimum_enclosing_circle(ps);
        tr.check(mec.has_value(), "random mec exists");
        for (const auto& p : ps) tr.check(fgeo::le(mec->c.dist(p), mec->r), "random mec contains point");
    }

    for (int tc = 0; tc < 100; ++tc) {
        T x1 = std::min(distv(rng), distv(rng));
        T x2 = x1 + std::abs(distv(rng)) + 0.1;
        T y1 = std::min(distv(rng), distv(rng));
        T y2 = y1 + std::abs(distv(rng)) + 0.1;
        T u1 = std::min(distv(rng), distv(rng));
        T u2 = u1 + std::abs(distv(rng)) + 0.1;
        T v1 = std::min(distv(rng), distv(rng));
        T v2 = v1 + std::abs(distv(rng)) + 0.1;
        Poly a{P(x1, y1), P(x2, y1), P(x2, y2), P(x1, y2)};
        Poly b{P(u1, v1), P(u2, v1), P(u2, v2), P(u1, v2)};
        T ix = std::max(T(0), std::min(x2, u2) - std::max(x1, u1));
        T iy = std::max(T(0), std::min(y2, v2) - std::max(y1, v1));
        tr.near(fgeo::convex_intersection_area(a, b), ix * iy, "random rectangle intersection area", 1e-7);
        tr.check(fgeo::intersect(a, b) == (ix > 0 && iy > 0), "random rectangle region intersect positive area");
    }
    std::cout << "[PASS] randomized consistency tests\n";
}

}  // namespace

int main() {
    auto start = std::chrono::steady_clock::now();
    Tester tr;
    test_point_and_eps(tr);
    test_orientation_and_arg(tr);
    test_line_segment(tr);
    test_circle_and_centers(tr);
    test_polygon_and_points(tr);
    test_incremental_and_impact(tr);
    test_randomized(tr);
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "total checks: " << tr.checks << '\n';
    std::cout << "elapsed_ms: " << ms << '\n';
    return 0;
}
#endif
