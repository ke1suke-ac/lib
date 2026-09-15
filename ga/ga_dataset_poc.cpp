// g++ -std=gnu++20 -O2 -Wall -Wextra -pedantic main.cpp && ./a.out
#include <bits/stdc++.h>
using namespace std;

//======================================================
// 1) 戦略の基底（型制約用の目印にだけ使う。中身なし）
//======================================================
struct CrossOver {};
struct Mutation {};

//======================================================
// 2) 要素仕様：Dataset と対応する Cross/Mut を束ねる
//======================================================
template <class Dataset, class Cross, class Mut>
struct ElemSpec {
    using DatasetT  = Dataset;
    using CrossoverT = Cross;
    using MutationT  = Mut;
};

//======================================================
// 3) 代表的な交叉／突然変異の実装
//    - 交叉: 一点交叉（配列／ビットセット）
//    - 突然変異: int配列リセット, double配列ガウス, bitsetフリップ
//======================================================

struct OnePointCrossover : CrossOver {
    std::mt19937* prng{};
    explicit OnePointCrossover(std::mt19937& rng) : prng(&rng) {}

    template <size_t N, class T>
    void apply(const array<T, N>& p1, const array<T, N>& p2, array<T, N>& c) const {
        std::uniform_int_distribution<size_t> cutDist(0, N); // [0,N]
        size_t k = cutDist(*prng);
        for (size_t i = 0; i < k; ++i) c[i] = p1[i];
        for (size_t i = k; i < N; ++i) c[i] = p2[i];
    }

    template <size_t N>
    void apply(const bitset<N>& p1, const bitset<N>& p2, bitset<N>& c) const {
        std::uniform_int_distribution<size_t> cutDist(0, N); // [0,N]
        size_t k = cutDist(*prng);
        c = p1;
        for (size_t i = k; i < N; ++i) c[i] = p2[i];
    }
};

struct IntResetMutation : Mutation {
    std::mt19937* prng{};
    int lo, hi;        // リセット範囲 [lo, hi]
    double rate;       // 各遺伝子のリセット確率

    IntResetMutation(std::mt19937& rng, int lo_, int hi_, double rate_)
        : prng(&rng), lo(lo_), hi(hi_), rate(rate_) {}

    template <size_t N, class T>
    void apply(array<T, N>& a) const requires std::is_integral_v<T> {
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        std::uniform_int_distribution<int> val(lo, hi);
        for (auto& x : a) {
            if (prob(*prng) < rate) x = static_cast<T>(val(*prng));
        }
    }

    template <size_t N>
    void apply(bitset<N>&) const {} // bitset は対象外（ここでは何もしない）
};

struct DoubleGaussianMutation : Mutation {
    std::mt19937* prng{};
    double sigma;    // 追加ノイズの標準偏差
    double rate;     // 各遺伝子にノイズを足す確率

    DoubleGaussianMutation(std::mt19937& rng, double sigma_, double rate_)
        : prng(&rng), sigma(sigma_), rate(rate_) {}

    template <size_t N, class T>
    void apply(array<T, N>& a) const requires std::is_floating_point_v<T> {
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        std::normal_distribution<double> noise(0.0, sigma);
        for (auto& x : a) {
            if (prob(*prng) < rate) x = static_cast<T>(x + noise(*prng));
        }
    }

    template <size_t N>
    void apply(bitset<N>&) const {} // bitset は対象外（ここでは何もしない）
};

struct BitFlipMutation : Mutation {
    std::mt19937* prng{};
    double rate; // 各ビットの反転確率

    BitFlipMutation(std::mt19937& rng, double rate_) : prng(&rng), rate(rate_) {}

    template <size_t N>
    void apply(bitset<N>& b) const {
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        for (size_t i = 0; i < N; ++i) if (prob(*prng) < rate) b.flip(i);
    }

    template <size_t N, class T>
    void apply(array<T, N>&) const {} // array は対象外（ここでは何もしない）
};

//======================================================
// 4) Gene 本体
//    - Gene<ElemSpec<Dataset, Cross, Mut>...>
//    - data_, cross_, mut_ を tuple で保持
//    - crossover(): 各要素で Cross::apply
//    - mutation():  各要素で Mut::apply
//======================================================
template <class... Specs>
class Gene {
    using DataTuple  = tuple<typename Specs::DatasetT...>;
    using CrossTuple = tuple<typename Specs::CrossoverT...>;
    using MutTuple   = tuple<typename Specs::MutationT...>;

public:
    DataTuple  data_;
    CrossTuple cross_;
    MutTuple   mut_;

    Gene() = default;
    Gene(DataTuple data, CrossTuple cross, MutTuple mut)
        : data_(std::move(data)), cross_(std::move(cross)), mut_(std::move(mut)) {}

    // 交叉：this と other から child を生成（各要素で対応する Cross を適用）
    void crossover(const Gene& other, Gene& child) const {
        crossover_impl(other, child, std::index_sequence_for<Specs...>{});
        // child の戦略は親の片方からコピー（ここでは this 側をコピー）
        child.cross_ = cross_;
        child.mut_   = mut_;
    }

    // 突然変異：this の各要素に対応する Mut を適用
    void mutation() {
        mutation_impl(std::index_sequence_for<Specs...>{});
    }

private:
    template <size_t... Is>
    void crossover_impl(const Gene& other, Gene& child, std::index_sequence<Is...>) const {
        ( std::get<Is>(cross_).apply(
              std::get<Is>(this->data_),
              std::get<Is>(other.data_),
              std::get<Is>(child.data_)
          ), ... );
    }

    template <size_t... Is>
    void mutation_impl(std::index_sequence<Is...>) {
        ( std::get<Is>(mut_).apply( std::get<Is>(this->data_) ), ... );
    }
};

//======================================================
// 5) 便利プリンタ
//======================================================
template <class T, size_t N>
ostream& operator<<(ostream& os, const array<T, N>& a) {
    os << "{";
    for (size_t i = 0; i < N; ++i) { if (i) os << ", "; os << a[i]; }
    os << "}";
    return os;
}
template <size_t N>
ostream& operator<<(ostream& os, const bitset<N>& b) {
    for (size_t i = 0; i < N; ++i) os << (b.test(N-1-i) ? '1' : '0'); // MSB→LSB表示
    return os;
}

//======================================================
// 6) デモ：array<int,5>, array<double,2>, bitset<60>, array<int,2>
//======================================================
int main() {
    std::mt19937 rng(42);

    using D0 = array<int,5>;
    using D1 = array<double,2>;
    using D2 = bitset<60>;
    using D3 = array<int,2>;

    using S0 = ElemSpec<D0, OnePointCrossover,        IntResetMutation>;
    using S1 = ElemSpec<D1, OnePointCrossover,        DoubleGaussianMutation>;
    using S2 = ElemSpec<D2, OnePointCrossover,        BitFlipMutation>;
    using S3 = ElemSpec<D3, OnePointCrossover,        IntResetMutation>;

    using G = Gene<S0, S1, S2, S3>;

    // 親A
    G::DataTuple  dataA{
        D0{1,2,3,4,5},
        D1{10.0, 20.0},
        []{ D2 b; for (int i=0;i<60;i+=3) b.set(i); return b; }(),
        D3{7, 8}
    };
    G::CrossTuple cross{
        OnePointCrossover(rng), OnePointCrossover(rng),
        OnePointCrossover(rng), OnePointCrossover(rng)
    };
    G::MutTuple mutA{
        IntResetMutation(rng, 0, 9, 0.3),     // 30% の確率で 0..9 にリセット
        DoubleGaussianMutation(rng, 0.5, 0.5),// 50% の確率で N(0,0.5) を加算
        BitFlipMutation(rng, 0.02),           // 2% で各ビット反転
        IntResetMutation(rng, 0, 9, 0.3)
    };
    G A{dataA, cross, mutA};

    // 親B
    G::DataTuple  dataB{
        D0{9,9,9,9,9},
        D1{-1.0, 3.14},
        []{ D2 b; for (int i=1;i<60;i+=4) b.set(i); return b; }(),
        D3{100, 200}
    };
    // 親Bの戦略は未使用でも良いが、形のために入れておく
    G::MutTuple mutB{
        IntResetMutation(rng, -5, 5, 0.1),
        DoubleGaussianMutation(rng, 1.0, 0.2),
        BitFlipMutation(rng, 0.01),
        IntResetMutation(rng, -5, 5, 0.1)
    };
    G B{dataB, cross, mutB};

    // 子
    G::DataTuple dataChild{
        D0{}, D1{}, D2{}, D3{}
    };
    G Child{dataChild, cross, mutA}; // とりあえず A の戦略を引き継ぐ

    // 交叉
    A.crossover(B, Child);

    // 表示
    auto& a0 = get<0>(A.data_); auto& b0 = get<0>(B.data_); auto& c0 = get<0>(Child.data_);
    auto& a1 = get<1>(A.data_); auto& b1 = get<1>(B.data_); auto& c1 = get<1>(Child.data_);
    auto& a2 = get<2>(A.data_); auto& b2 = get<2>(B.data_); auto& c2 = get<2>(Child.data_);
    auto& a3 = get<3>(A.data_); auto& b3 = get<3>(B.data_); auto& c3 = get<3>(Child.data_);

    cout << "== Crossover result ==\n";
    cout << "A[0] " << a0 << "   B[0] " << b0 << "   Child[0] " << c0 << "\n";
    cout << "A[1] " << a1 << "   B[1] " << b1 << "   Child[1] " << c1 << "\n";
    cout << "A[2] " << a2 << "   B[2] " << b2 << "   Child[2] " << c2 << "\n";
    cout << "A[3] " << a3 << "   B[3] " << b3 << "   Child[3] " << c3 << "\n";

    // 突然変異（子に適用）
    Child.mutation();

    cout << "\n== After mutation ==\n";
    cout << "Child[0] " << get<0>(Child.data_) << "\n";
    cout << "Child[1] " << get<1>(Child.data_) << "\n";
    cout << "Child[2] " << get<2>(Child.data_) << "\n";
    cout << "Child[3] " << get<3>(Child.data_) << "\n";
}
