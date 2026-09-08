#pragma once
/*
  ============================================================
    weighted_roulette.hpp   (C++20 / GCC 12.2)
  ------------------------------------------------------------
      動的に重みを更新しつつ高速にルーレット選択するユーティリティ
      • update : O(log N)
      • select : O(log N)
  ============================================================
*/
#include <vector>
#include <random>
#include <stdexcept>
#include <bit>          // std::bit_width
#include <cstddef>

/* ---------------- Fenwick Tree (BIT) 汎用 ------------------ */
template < class T >
class Fenwick {
public:
    explicit Fenwick(std::size_t n = 0, T id = T{}) : bit_(n + 1, id) {}

    /* idx (1‑based) に delta を加算 */
    void add(std::size_t idx, T delta) {
        for (std::size_t i = idx; i < bit_.size(); i += i & -i) bit_[i] += delta;
    }
    /* [1, idx] の累積和 */
    T sum(std::size_t idx) const {
        T s{};
        for (std::size_t i = idx; i; i -= i & -i) s += bit_[i];
        return s;
    }
    /* 累積値 k (< total) を初めて超える位置を 0‑based で返す */
    std::size_t kth(T k) const {
        std::size_t idx = 0;
        T acc{};
        for (std::size_t bit = 1ULL << std::bit_width(bit_.size() - 1); bit; bit >>= 1) {
            std::size_t nxt = idx + bit;
            if (nxt < bit_.size() && acc + bit_[nxt] <= k) {
                acc += bit_[nxt];
                idx  = nxt;
            }
        }
        return idx;      // 0‑based
    }
    T total() const { return sum(bit_.size() - 1); }
    std::size_t size() const { return bit_.size() - 1; }

private:
    std::vector<T> bit_;
};

/* ------------ WeightedRoulette : ユーザ向けクラス ------------ */
class WeightedRoulette {
public:
    using weight_t = double;

    explicit WeightedRoulette(std::vector<weight_t> w = {})
        : n_(w.size()), ft_(n_), w_(std::move(w)), rng_(0x9E3779B97F4A7C15ULL) {
        for (std::size_t i = 0; i < n_; ++i) ft_.add(i + 1, w_[i]);
        reset_distribution();
    }

    /* idx 番目 (0‑based) の重みを new_w に更新 */
    void set_weight(std::size_t idx, weight_t new_w) {
        weight_t delta = new_w - w_[idx];
        w_[idx]        = new_w;
        ft_.add(idx + 1, delta);
        reset_distribution();                 // 合計値が変わったら分布を更新
    }

    /* ルーレット選択（重みに比例したインデックスを返す）*/
    std::size_t sample() {
        if (ft_.total() <= 0.0)
            throw std::runtime_error("すべての重みが 0 です．");
        weight_t r = dist_(rng_);
        return ft_.kth(r);
    }

    /* 各種アクセサ */
    [[nodiscard]] weight_t   total_weight() const { return ft_.total(); }
    [[nodiscard]] std::size_t size()       const { return n_;          }
    [[nodiscard]] weight_t   weight(std::size_t idx) const { return w_[idx]; }

private:
    std::size_t                                 n_;
    Fenwick<weight_t>                           ft_;
    std::vector<weight_t>                       w_;
    std::mt19937_64                             rng_;
    std::uniform_real_distribution<weight_t>    dist_{0.0, 1.0};

    /* 分布の上下限を合計値に合わせて再設定 */
    void reset_distribution() {
        using param_t = typename decltype(dist_)::param_type;
        dist_.param(param_t(0.0, ft_.total()));
    }
};
