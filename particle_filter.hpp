// 任意の粒子型に対する逐次推定、再サンプリング、MCMC再探索、加重平均
// C++20 / 単一スレッド / double。Particleは値としてコピー・代入できる型
// 粒子数・総試行回数はintの範囲内、有限対数値の加減算はdoubleの範囲内とする
// コールバック中の再入・構造変更は禁止。戻り値のviewは次の変更まで有効
// -infを無効な尤度に使うため、-ffast-math / Ofastは使用しない
// bool粒子・観測はParticleFilterで利用できる。ParticleCloudの直接操作ではboolを含むstruct等を使う
#pragma once
#include <bits/stdc++.h>

template<class Particle>
class ParticleCloud {
public:
    struct UpdateResult {
        bool ok;
        double log_normalizer;
    };
    struct MoveResult {
        int attempted = 0;
        int accepted = 0;
    };
    template<class Undo>
    struct MoveProposal {
        Undo undo;
        double log_correction = 0; // log q(旧|新) - log q(新|旧)。対象密度の差は含めない
    };
    template<class Value>
    struct Estimate {
        Value mean;
        double selected_mass;
    };

private:
    static constexpr double neg_inf = -std::numeric_limits<double>::infinity();
    double ess_ratio_;
    std::vector<Particle> particles_, buffer_;
    std::vector<double> log_weights_, weights_;
    mutable std::vector<double> scratch_; // 更新用対数重みと、抽出用CDFの作業領域を兼用する
    mutable std::vector<int> order_;
    mutable int selected_k_ = 0;
    mutable bool cdf_ready_ = false;
    double ess_ = 0;

    template<class Rng>
    static double uniform(Rng& rng) {
        return std::generate_canonical<double, 53>(rng);
    }

    void equal_weights() {
        const int n = size();
        log_weights_.assign(n, -std::log(double(n)));
        weights_.assign(n, 1.0 / n);
        ess_ = n;
        selected_k_ = 0;
        cdf_ready_ = false;
    }

    UpdateResult normalize() {
        cdf_ready_ = false; // scratch_は上書き済み。全無効で旧分布を保つ場合も再構築が必要
        // 全無効なら既存の粒子・重みを変更しない
        double largest = neg_inf;
        for (double value : scratch_) {
            assert(std::isfinite(value) || value == neg_inf);
            largest = std::max(largest, value);
        }
        if (largest == neg_inf) return {false, neg_inf};

        // 指数関数は各粒子につき一度だけ評価する
        const int n = int(scratch_.size());
        weights_.resize(n);
        double sum = 0;
        for (int i = 0; i < n; ++i) {
            scratch_[i] -= largest;
            weights_[i] = std::exp(scratch_[i]);
            sum += weights_[i];
        }
        const double log_sum = std::log(sum), inv_sum = 1.0 / sum;
        double squared_sum = 0;
        for (int i = 0; i < n; ++i) {
            weights_[i] *= inv_sum;
            squared_sum += weights_[i] * weights_[i];
            scratch_[i] -= log_sum;
        }
        log_weights_.swap(scratch_);
        ess_ = 1.0 / squared_sum;
        selected_k_ = 0;
        return {true, largest + log_sum};
    }

    static void copy_to(std::vector<Particle>& buffer, int destination, const Particle& source) {
        if (destination < int(buffer.size())) buffer[destination] = source;
        else buffer.push_back(source);
    }

    // 同じ重み・kの選択を再利用する。射影値は保存せず、状態変更後も読み直す。
    void select_indices(int top_k) const {
        if (selected_k_ == top_k) return;
        selected_k_ = top_k;
        if (top_k == 1) {
            order_.resize(1);
            order_[0] = int(std::max_element(log_weights_.begin(), log_weights_.end()) - log_weights_.begin());
            return;
        }
        order_.resize(size());
        std::iota(order_.begin(), order_.end(), 0);
        std::nth_element(order_.begin(), order_.begin() + top_k, order_.end(), [&](int a, int b) {
            return log_weights_[a] != log_weights_[b] ? log_weights_[a] > log_weights_[b] : a < b;
        });
    }

    // 戻り値方式と出力先指定方式で集計を共有する。spanはリサイズしない。
    template<class Output, class Project>
    double estimate_into_impl(Output& mean, Project&& project, int top_k) const {
        using Value = std::remove_cvref_t<std::invoke_result_t<Project&, const Particle&>>;
        bool initialized = false;
        return for_each_weighted([&](const Particle& particle, double weight) {
            const auto& value = std::invoke(project, particle);
            if constexpr (std::is_arithmetic_v<Value>) mean += weight * double(value);
            else {
                if constexpr (requires { mean.resize(std::size(value)); }) {
                    if (!initialized) {
                        mean.resize(std::size(value));
                        initialized = true;
                    }
                }
                assert(mean.size() == std::size(value));
                for (std::size_t j = 0; j < mean.size(); ++j) mean[j] += weight * double(value[j]);
            }
        }, top_k);
    }

public:
    // 再サンプリングを行うESS比率を設定する。0で自動判定を無効化する O(1)
    explicit ParticleCloud(double ess_ratio = 0.5) : ess_ratio_(ess_ratio) {
        assert(ess_ratio >= 0 && ess_ratio <= 1);
    }

    // 作業用バッファは複製しない。代入先の確保済み容量はそのまま再利用する。
    ParticleCloud(const ParticleCloud& other) { *this = other; }
    ParticleCloud& operator=(const ParticleCloud& other) {
        ess_ratio_ = other.ess_ratio_;
        particles_ = other.particles_;
        log_weights_ = other.log_weights_;
        weights_ = other.weights_;
        ess_ = other.ess_;
        selected_k_ = 0;
        cdf_ready_ = false;
        return *this;
    }
    ParticleCloud(ParticleCloud&&) = default;
    ParticleCloud& operator=(ParticleCloud&&) = default;

    // 事前分布から生成して等重みで初期化する O(N × 生成コスト)
    template<class Init, class Rng>
    void initialize(int count, Init&& init, Rng& rng) {
        assert(count > 0);
        particles_.clear();
        particles_.reserve(count);
        for (int i = 0; i < count; ++i) particles_.push_back(init(rng));
        equal_weights();
    }

    // 外部粒子と対数重みを設定する。全無効なら旧状態を維持する O(N)＋引数のコピー費用
    bool assign(std::vector<Particle> particles, std::span<const double> log_weights = {}) {
        assert(!particles.empty());
        assert(log_weights.empty() || log_weights.size() == particles.size());
        // 等重みなら指数関数での正規化を省略する
        if (log_weights.empty()) {
            particles_ = std::move(particles);
            equal_weights();
            return true;
        }
        // 内部viewを渡された場合も、入力を読み終えてから重みを更新する
        scratch_.assign(log_weights.begin(), log_weights.end());
        if (!normalize().ok) return false;
        particles_ = std::move(particles);
        return true;
    }

    // 今回の増分対数尤度で更新する。全無効なら旧重みを維持する O(N × 評価コスト)
    template<class LogIncrement>
    UpdateResult update_log(LogIncrement&& log_increment) {
        assert(size() > 0);
        scratch_.resize(size());
        for (int i = 0; i < size(); ++i) {
            const double increment = log_increment(std::as_const(particles_[i]));
            scratch_[i] = log_weights_[i] + increment;
        }
        return normalize();
    }

    // 真の状態遷移を適用する。重みは維持し、後続update失敗でも巻き戻さない O(N × 遷移コスト)
    template<class Transition, class Rng>
    void predict(Transition&& transition, Rng& rng) {
        assert(size() > 0);
        for (Particle& particle : particles_) transition(particle, rng);
    }

    // 元粒子のコピーに提案を適用し、返された対数補正と一括確定する O(N × (コピー＋提案コスト))
    // 対数重み-infの粒子は提案しない。増分-infの候補は元の状態を残し、重みだけ0にする。
    template<class Proposal, class Rng>
    UpdateResult propose_update_log(Proposal&& proposal, Rng& rng) {
        assert(size() > 0);
        // コピー先の容量を再利用する。失敗時もRNGや外部副作用は巻き戻さない
        const int n = size();
        buffer_.reserve(n);
        scratch_.resize(n);
        for (int i = 0; i < n; ++i) {
            copy_to(buffer_, i, particles_[i]);
            scratch_[i] = log_weights_[i];
            if (scratch_[i] == neg_inf) continue;
            const double increment = proposal(std::as_const(particles_[i]), buffer_[i], rng);
            scratch_[i] += increment;
            if (increment == neg_inf) buffer_[i] = particles_[i];
        }
        const UpdateResult result = normalize();
        if (result.ok) {
            buffer_.erase(buffer_.begin() + n, buffer_.end());
            particles_.swap(buffer_);
        }
        return result;
    }

    // ESSが閾値未満なら再サンプリングする。未実行時はRNGも維持する O(1) またはresampleの計算量
    template<class Rng>
    bool resample_if_needed(Rng& rng) {
        assert(size() > 0);
        if (ess_ >= ess_ratio_ * size()) return false;
        resample(rng);
        return true;
    }

    // 自身をSystematic法で再サンプリングする
    template<class Rng>
    int resample(Rng& rng, int new_count = 0) { return resample_from(*this, rng, new_count); }

    // sourceを保ったまま再サンプリングする。異なる親インデックス数を返す
    // sourceと自身が同じ場合も使用可能。O(N＋N' × コピーコスト)
    template<class Rng>
    int resample_from(const ParticleCloud& source, Rng& rng, int new_count = 0) {
        assert(source.size() > 0 && new_count >= 0);
        const int n = source.size(), count = new_count == 0 ? n : new_count;
        const bool in_place = this == &source;
        auto& output = in_place ? buffer_ : particles_;
        const auto& weights = source.weights_;
        int last = n - 1;
        while (weights[last] == 0) --last;
        int parent = 0, previous = -1, distinct = 0;
        double cumulative = weights[0];
        const double offset = uniform(rng), inverse_count = 1.0 / count;
        output.reserve(count);
        for (int j = 0; j < count; ++j) {
            const double position = (j + offset) * inverse_count;
            while (parent < last && (cumulative <= position || weights[parent] == 0))
                cumulative += weights[++parent];
            distinct += parent != previous;
            previous = parent;
            copy_to(output, j, source.particles_[parent]);
        }
        output.erase(output.begin() + count, output.end());
        if (in_place) particles_.swap(buffer_);
        equal_weights();
        return distinct;
    }

    // 全履歴を反映したtargetに対するMH再探索を行う O(N × (評価＋steps × (コピー＋提案＋評価コスト)))
    // 提案補正-infは確実棄却とし、候補のtargetは呼ばない。受理判定用RNGは通常どおり消費する。
    template<class LogTarget, class Propose, class Rng>
    MoveResult rejuvenate(int steps, LogTarget&& log_target, Propose&& propose, Rng& rng) {
        assert(size() > 0 && steps >= 0);
        MoveResult result;
        if (steps == 0) return result;
        Particle candidate = particles_[0];
        // 有限対数重みは、通常重みが0に丸まっていても再探索する
        for (int i = 0; i < size(); ++i) {
            if (log_weights_[i] == neg_inf) continue;
            double current = log_target(std::as_const(particles_[i]));
            assert(std::isfinite(current));
            for (int step = 0; step < steps; ++step) {
                candidate = particles_[i];
                const double correction = propose(candidate, rng);
                assert(std::isfinite(correction) || correction == neg_inf);
                const double next = correction == neg_inf ? neg_inf : log_target(std::as_const(candidate));
                assert(std::isfinite(next) || next == neg_inf);
                ++result.attempted;
                // 補正はlog q(旧|新) - log q(新|旧)。棄却時は元粒子を変更しない
                const double acceptance = next - current + correction;
                // log(U)は負なので、対数受理比の上限処理は不要
                if (std::log(uniform(rng)) < acceptance) {
                    using std::swap;
                    swap(particles_[i], candidate);
                    current = next;
                    ++result.accepted;
                }
            }
        }
        return result;
    }

    // 差分提案版。proposeはその場で変更してMoveProposal<Token>を返す。
    // undo(Particle&,const Token&)は棄却時に変更箇所・ユーザーキャッシュを完全に戻す。
    // O(N × (評価＋steps × (差分変更＋評価＋巻き戻しコスト)))。重みは維持する。
    template<class LogTarget, class Propose, class Undo, class Rng>
    MoveResult rejuvenate(int steps, LogTarget&& log_target, Propose&& propose, Undo&& undo, Rng& rng) {
        assert(size() > 0 && steps >= 0);
        MoveResult result;
        if (steps == 0) return result;
        for (int i = 0; i < size(); ++i) {
            if (log_weights_[i] == neg_inf) continue;
            double current = log_target(std::as_const(particles_[i]));
            assert(std::isfinite(current));
            for (int step = 0; step < steps; ++step) {
                auto proposal = propose(particles_[i], rng);
                const double correction = proposal.log_correction;
                assert(std::isfinite(correction) || correction == neg_inf);
                const double next = correction == neg_inf ? neg_inf : log_target(std::as_const(particles_[i]));
                assert(std::isfinite(next) || next == neg_inf);
                ++result.attempted;
                if (std::log(uniform(rng)) < next - current + correction) {
                    current = next;
                    ++result.accepted;
                } else undo(particles_[i], std::as_const(proposal.undo));
            }
        }
        return result;
    }

    // targetを保存するカーネルをコピーなしで適用する 0回はO(1)、その他O(N＋N × steps × カーネルコスト)
    template<class Kernel, class Rng>
    void rejuvenate(int steps, Kernel&& kernel, Rng& rng) {
        assert(size() > 0 && steps >= 0);
        if (steps == 0) return;
        for (int i = 0; i < size(); ++i) {
            if (log_weights_[i] == neg_inf) continue;
            for (int step = 0; step < steps; ++step) kernel(particles_[i], rng);
        }
    }

    // 再正規化重みを渡し、選択質量を返す。全体O(N × 集計コスト)。
    // 上位kは初回平均O(N＋k × 集計コスト)、同じ重み・kならO(k × 集計コスト)。
    template<class Consume>
    double for_each_weighted(Consume&& consume, int top_k = 0) const {
        assert(size() > 0 && top_k >= 0 && top_k <= size());
        if (top_k == 0 || top_k == size()) {
            for (int i = 0; i < size(); ++i) consume(particles_[i], weights_[i]);
            return 1;
        }
        select_indices(top_k);
        // 上位1粒子は選択済みの粒子をそのまま代表にする
        if (top_k == 1) {
            const int i = order_[0];
            consume(particles_[i], 1.0);
            return weights_[i];
        }
        // 選んだ集合の重みで再正規化する
        double mass = 0;
        for (int j = 0; j < top_k; ++j) mass += weights_[order_[j]];
        for (int j = 0; j < top_k; ++j) {
            const int i = order_[j];
            consume(particles_[i], weights_[i] / mass);
        }
        return mass;
    }

    // 粒子・重みを変えず、外部RNGで独立に復元抽出する。空出力は何もしない。
    // 初回O(N+M log N)、重み不変の再呼び出しO(M log N)。抽出後の平均は等重み1/M。
    template<class SamplingRng>
    void sample_indices(std::span<int> output, SamplingRng& rng) const {
        if (output.empty()) return;
        assert(size() > 0);
        if (!cdf_ready_) {
            scratch_.resize(weights_.size());
            std::partial_sum(weights_.begin(), weights_.end(), scratch_.begin());
            cdf_ready_ = true;
        }
        const double total = scratch_.back(), upper = std::nextafter(total, 0.0);
        for (int& index : output) {
            const double target = std::min(uniform(rng) * total, upper);
            index = int(std::upper_bound(scratch_.begin(), scratch_.end(), target) - scratch_.begin());
        }
    }

    // 数値または数値列への射影をdoubleで平均する。列は全粒子で同じ長さとする
    // arrayはarray<double,D>、vector/span等はvector<double>を返す。射影結果はコピーしない
    template<class Project>
    auto estimate(Project&& project, int top_k = 0) const {
        using Value = std::remove_cvref_t<std::invoke_result_t<Project&, const Particle&>>;
        auto mean = [] {
            if constexpr (std::is_arithmetic_v<Value>) return double{};
            else {
                static_assert(std::ranges::random_access_range<Value> && std::ranges::sized_range<Value>,
                              "Project must return a number or a sized random-access numeric range");
                static_assert(std::is_arithmetic_v<std::ranges::range_value_t<Value>>);
                if constexpr (requires { std::tuple_size<Value>::value; })
                    return std::array<double, std::tuple_size_v<Value>>{};
                else return std::vector<double>{};
            }
        }();
        const double mass = estimate_into_impl(mean, project, top_k);
        return Estimate<decltype(mean)>{std::move(mean), mass};
    }

    // 既存のdouble連続領域へ数値列の平均を上書きし、選択質量を返す。
    // 出力長は射影と同じ。出力は粒子・射影結果と独立した領域とする。
    template<class Output, class Project>
    double estimate_into(Output&& buffer, Project&& project, int top_k = 0) const {
        auto output = std::span(buffer);
        static_assert(std::is_same_v<typename decltype(output)::element_type, double>);
        std::fill(output.begin(), output.end(), 0.0);
        return estimate_into_impl(output, project, top_k);
    }

    // 現在の粒子数を返す O(1)
    int size() const { return int(particles_.size()); }

    // 現在の重みのESSを返す。粒子の種類数とは異なる O(1)
    double ess() const { assert(size() > 0); return ess_; }

    // 読み取り専用の粒子viewを返す O(1)
    std::span<const Particle> particles() const { return particles_; }

    // 正規化済みの重みviewを返す O(1)
    std::span<const double> weights() const { return weights_; }

    // 正規化済みの対数重みviewを返す O(1)
    std::span<const double> log_weights() const { return log_weights_; }
};

// 独立な一様事前分布を持つ実数パラメータ。Modelの基底として使える
// UniformBox<D>はarray、UniformBox<>は入力で長さが決まるvectorを使う
template<std::size_t D = std::dynamic_extent>
class UniformBox {
public:
    using Particle = std::conditional_t<D == std::dynamic_extent, std::vector<double>, std::array<double, D>>;

private:
    Particle lower_, upper_;
    double move_scale_;
    std::normal_distribution<double> normal_;

public:
    UniformBox(Particle lower, Particle upper, double move_scale = 0.2)
        : lower_(std::move(lower)), upper_(std::move(upper)), move_scale_(move_scale) {
        static_assert(D > 0);
        assert(!lower_.empty() && lower_.size() == upper_.size());
        assert(std::isfinite(move_scale) && move_scale > 0);
        for (std::size_t j = 0; j < lower_.size(); ++j)
            assert(std::isfinite(lower_[j]) && std::isfinite(upper_[j]) && lower_[j] < upper_[j]);
        move_scale_ /= std::sqrt(double(std::min<std::size_t>(4, lower_.size())));
    }

    template<class Rng>
    Particle sample(Rng& rng) {
        Particle result;
        if constexpr (D == std::dynamic_extent) result.resize(lower_.size());
        for (std::size_t j = 0; j < lower_.size(); ++j)
            result[j] = lower_[j] + (upper_[j] - lower_[j]) * std::generate_canonical<double, 53>(rng);
        return result;
    }

    // MHの比で相殺する定数を除いた対数事前密度
    double log_prior(const Particle& p) const {
        assert(p.size() == lower_.size());
        for (std::size_t j = 0; j < lower_.size(); ++j)
            if (!(lower_[j] <= p[j] && p[j] <= upper_[j]))
                return -std::numeric_limits<double>::infinity();
        return 0;
    }

    // 座標をランダムにmin(4,次元数)回選んで変更する対称提案。同じ座標を選んでもよい
    // 境界では反射させる。clampと異なり、この対称提案は一様分布を保つ
    template<class Rng>
    void propose(Particle& p, Rng& rng) {
        assert(p.size() == lower_.size());
        // 少数の大域提案を混ぜ、離れた仮説へ移る機会を保つ。反転は対称かつ体積保存。
        if (std::generate_canonical<double, 53>(rng) < 0.1) {
            for (std::size_t j = 0; j < p.size(); ++j) p[j] = lower_[j] + upper_[j] - p[j];
            return;
        }
        const std::size_t count = std::min<std::size_t>(4, p.size());
        for (std::size_t step = 0; step < count; ++step) {
            const auto j = std::uniform_int_distribution<std::size_t>(0, p.size() - 1)(rng);
            const double width = upper_[j] - lower_[j];
            p[j] += width * move_scale_ * normal_(rng);
            if (p[j] < lower_[j] || p[j] > upper_[j]) {
                double offset = std::fmod(p[j] - lower_[j], 2 * width);
                if (offset < 0) offset += 2 * width;
                p[j] = lower_[j] + std::min(offset, 2 * width - offset);
            }
        }
    }
};

// 通常利用用。ModelはParticle/Observation型、sample、likelihoodを定義する
// likelihoodとlog_likelihoodはどちらか一方。確率密度は1を超えてよい
// transitionまたはpropose_transitionがあるModelは動的状態、それ以外は固定パラメータ
// 固定パラメータでは、観測に過去の入力条件も値として保持する
template<class Model, class Rng = std::mt19937_64>
class ParticleFilter {
public:
    using Particle = typename Model::Particle;
    using Observation = typename Model::Observation;
    struct Param {
        int count = 1024;
        double ess_ratio = 0.5; // 新観測前の再サンプリング判定。0で無効、定期再探索とは独立
        int move_steps = 2;
        int move_interval = 8; // 固定パラメータのみ。0ならESS低下時だけ再探索
        double tempering_ess_ratio = 0.5; // 固定パラメータの段階更新。0で無効、move_steps=0でも無効
    };

private:
    static constexpr double neg_inf = -std::numeric_limits<double>::infinity();
    // 任意の観測誘導提案。戻り値はlog p(新|旧,制御) - log q(新|旧,観測)。
    // 今回の観測尤度は含めない。未定義なら通常のtransitionを使用する。
    static constexpr bool guided = requires(Model& m, Particle& p, const Particle& old,
                                            const Observation& o, Rng& r) {
        m.propose_transition(p, old, o, r);
    };
    static constexpr bool dynamic = guided || requires(Model& m, Particle& p, const Observation& o, Rng& r) {
        m.transition(p, o, r);
    };
    // 差分提案を定義した場合は通常のproposeより優先する。undoも必要。
    static constexpr bool inplace_moves = requires(Model& m, Particle& p, Rng& r) { m.propose_in_place(p, r); };
    static constexpr bool local_moves = inplace_moves || requires(Model& m, Particle& p, Rng& r) { m.propose(p, r); };
    static constexpr bool probability = requires(const Model& m, const Particle& p, const Observation& o) {
        m.likelihood(p, o);
    };
    static constexpr bool logarithmic = requires(const Model& m, const Particle& p, const Observation& o) {
        m.log_likelihood(p, o);
    };
    static_assert(probability != logarithmic, "Define exactly one of likelihood and log_likelihood");

    struct Entry {
        Particle value;
        double target; // 局所提案:対数事後密度、事前分布からの提案:累積対数尤度
    };
    using Cloud = ParticleCloud<Entry>;
    Model model_;
    Param param_;
    Rng rng_;
    Cloud cloud_, prepared_;
    std::vector<Observation> history_;
    std::vector<double> increments_;
    int turns_ = 0;

    static void validate_param(const Param& param) {
        assert(param.count > 0 && param.move_steps >= 0 && param.move_interval >= 0);
        assert(param.ess_ratio >= 0 && param.ess_ratio <= 1);
        assert(param.tempering_ess_ratio >= 0 && param.tempering_ess_ratio <= 1);
        (void)param;
    }

    static double checked_log(double value) {
        assert(std::isfinite(value) || value == neg_inf);
        return value;
    }

    static double to_log(double value) {
        assert(std::isfinite(value) && value >= 0);
        return value == 0 ? neg_inf : std::log(value);
    }

    double log_likelihood(const Particle& p, const Observation& observation) const {
        if constexpr (logarithmic) return checked_log(model_.log_likelihood(p, observation));
        else return to_log(model_.likelihood(p, observation));
    }

    double initial_target(const Particle& p) const {
        if constexpr (!dynamic && local_moves) {
            constexpr bool log_prior = requires { model_.log_prior(p); };
            constexpr bool prior = requires { model_.prior(p); };
            static_assert(log_prior != prior, "Local proposals need exactly one of prior and log_prior");
            if constexpr (log_prior) return checked_log(model_.log_prior(p));
            else return to_log(model_.prior(p));
        } else return 0; // 初期分布からの独立提案では、事前密度と提案比が相殺する
    }

    double full_target(const Particle& p) const {
        double value = initial_target(p);
        if (value == neg_inf) return value;
        if constexpr (requires { model_.log_likelihood_history(p, history()); }) {
            // 任意の高速化経路。各観測の対数尤度の総和と、共通定数も含めて一致させる
            return value + checked_log(model_.log_likelihood_history(p, history()));
        } else {
            for (const Observation& observation : history_) {
                // 確率のゼロは対数変換前に判定する。残りの履歴は評価しない
                if constexpr (probability) {
                    const double likelihood = model_.likelihood(p, observation);
                    if (likelihood == 0) return neg_inf;
                    value += to_log(likelihood);
                } else {
                    const double increment = checked_log(model_.log_likelihood(p, observation));
                    if (increment == neg_inf) return neg_inf;
                    value += increment;
                }
            }
            return value;
        }
    }

    typename Cloud::MoveResult move(Cloud& cloud, int steps,
                                    const Observation* observation = nullptr, double beta = 0) {
        const auto target = [](const Entry& entry) { return entry.target; };
        auto evaluate = [&](Entry& entry, double correction) {
            // 逆向き提案の確率が0なら棄却確定。候補の事前・尤度は評価しない。
            entry.target = correction == neg_inf ? neg_inf : full_target(entry.value);
            if (observation && beta > 0 && entry.target != neg_inf)
                entry.target += beta * log_likelihood(entry.value, *observation);
        };
        if constexpr (inplace_moves) {
            return cloud.rejuvenate(steps, target, [&](Entry& entry, Rng& rng) {
                const double old_target = entry.target;
                auto proposal = model_.propose_in_place(entry.value, rng);
                evaluate(entry, proposal.log_correction);
                using Token = std::pair<decltype(proposal.undo), double>;
                return typename Cloud::template MoveProposal<Token>{
                    {std::move(proposal.undo), old_target}, proposal.log_correction};
            }, [&](Entry& entry, const auto& token) {
                model_.undo(entry.value, token.first);
                entry.target = token.second;
            }, rng_);
        } else return cloud.rejuvenate(steps, target,
            [&](Entry& entry, Rng& rng) {
                double correction = 0;
                if constexpr (local_moves) {
                    if constexpr (std::is_void_v<decltype(model_.propose(entry.value, rng))>)
                        model_.propose(entry.value, rng); // voidは対称提案
                    else correction = model_.propose(entry.value, rng);
                } else entry.value = model_.sample(rng);
                evaluate(entry, correction);
                return correction;
            }, rng_);
    }

    // 重みの集中を途中で緩和しながら、今回の尤度を合計1乗だけ反映する
    // 最大12段階で打ち切り、最後は残り全量を反映する。モデルや観測は書き換えない
    typename Cloud::UpdateResult tempered_update(Cloud*& next, const Observation& observation,
                                                 typename Cloud::MoveResult& moved, bool& resampled) {
        double beta = 0, evidence = 0;
        increments_.resize(next->size());
        auto explore = [&] {
            next->resample(rng_);
            resampled = true;
            const auto result = move(*next, param_.move_steps, &observation, beta);
            moved.attempted += result.attempted;
            moved.accepted += result.accepted;
        };
        for (int stage = 0; ; ++stage) {
            for (int i = 0; i < next->size(); ++i)
                increments_[i] = log_likelihood(next->particles()[i].value, observation);
            auto ess = [&](double delta) {
                double maximum = neg_inf;
                for (int i = 0; i < next->size(); ++i)
                    maximum = std::max(maximum, next->log_weights()[i] + delta * increments_[i]);
                if (maximum == neg_inf) return 0.0;
                double sum = 0, squares = 0;
                for (int i = 0; i < next->size(); ++i) {
                    const double w = std::exp(next->log_weights()[i] + delta * increments_[i] - maximum);
                    sum += w;
                    squares += w * w;
                }
                return sum * sum / squares;
            };
            const double remaining = 1 - beta, threshold = param_.tempering_ess_ratio * next->size();
            double delta = remaining;
            const double full_ess = ess(delta);
            if (full_ess == 0) return {false, neg_inf};
            if (stage < 11 && full_ess < threshold) {
                // 最初の部分更新までは元の粒子を読むだけなので、コピーを遅延する。
                if (next == &cloud_) { prepared_ = cloud_; next = &prepared_; }
                if (next->ess() < threshold) {
                    explore();
                    for (int i = 0; i < next->size(); ++i)
                        increments_[i] = log_likelihood(next->particles()[i].value, observation);
                }
                double low = 0, high = remaining;
                for (int iteration = 0; iteration < 16; ++iteration) {
                    const double middle = (low + high) * 0.5;
                    if (ess(middle) < threshold) high = middle;
                    else low = middle;
                }
                delta = std::min(remaining, std::max(low, 0.0001));
            }
            int index = 0;
            const auto update = next->update_log([&](const Entry&) { return delta * increments_[index++]; });
            if (!update.ok) return update;
            evidence += update.log_normalizer;
            index = 0;
            next->predict([&](Entry& entry, Rng&) { entry.target += delta * increments_[index++]; }, rng_);
            if (delta == remaining) return {true, evidence};
            beta += delta;
            explore();
        }
    }

public:
    using MoveResult = typename Cloud::MoveResult;
    struct ObserveResult {
        bool ok;
        double log_predictive;
        bool resampled;
        MoveResult moves;
    };

    explicit ParticleFilter(Model model, Param param = {}, Rng rng = Rng(0))
        : model_(std::move(model)), param_(param), rng_(std::move(rng)) {
        validate_param(param);
        reset();
    }

    // 分岐に必要なモデル・履歴・粒子・RNGだけを複製する。外部参照の先は利用側で管理する。
    ParticleFilter(const ParticleFilter& other)
        requires (std::is_copy_constructible_v<Model> && std::is_copy_constructible_v<Rng>)
        : model_(other.model_), param_(other.param_), rng_(other.rng_), cloud_(other.cloud_),
          history_(other.history_), turns_(other.turns_) {}

    // 代入先の作業領域は保持し、以後のobserveで再利用する。
    ParticleFilter& operator=(const ParticleFilter& other)
        requires (std::is_copy_assignable_v<Model> && std::is_copy_assignable_v<Rng>) {
        model_ = other.model_;
        param_ = other.param_;
        rng_ = other.rng_;
        cloud_ = other.cloud_;
        history_ = other.history_;
        turns_ = other.turns_;
        return *this;
    }
    ParticleFilter(ParticleFilter&&) = default;
    ParticleFilter& operator=(ParticleFilter&&) = default;

    const Param& param() const { return param_; }

    // 履歴・累積評価・成功観測数を保って計算量を調整する。粒子数変更時だけ即時再サンプリング
    // 再サンプリング後は等重み。回数・頻度だけの変更では粒子・重み・RNGを変えない
    void set_param(Param param) {
        validate_param(param);
        if (param.count != size()) cloud_.resample(rng_, param.count);
        param_ = param;
    }

    // 初期分布から再生成する。乱数器は継続し、配列容量は再利用する
    void reset() {
        history_.clear();
        turns_ = 0;
        cloud_.initialize(param_.count, [&](Rng& rng) {
            Particle p = model_.sample(rng);
            const double target = initial_target(p);
            assert(std::isfinite(target));
            return Entry{std::move(p), target};
        }, rng_);
    }

    // 新観測を一度だけ反映する。段階更新でも尤度の指数の合計は必ず1
    // 最終段階の重みを保持するので、成功直後にも上位kを読める
    // 全無効なら粒子・重み・履歴・ターン数を維持する。RNGと外部副作用は巻き戻さない
    ObserveResult observe(const Observation& observation) {
        const bool periodic = !dynamic && param_.move_steps > 0 && param_.move_interval > 0
            && turns_ > 0 && turns_ % param_.move_interval == 0;
        const bool prepare = cloud_.ess() < param_.ess_ratio * size() || periodic;
        const bool temper = !dynamic && param_.tempering_ess_ratio > 0 && param_.move_steps > 0;
        Cloud* next = prepare ? &prepared_ : &cloud_;
        bool resampled = prepare;
        MoveResult moved;
        if (prepare) {
            prepared_.resample_from(cloud_, rng_);
            if constexpr (!dynamic) moved = move(prepared_, param_.move_steps);
        }

        typename Cloud::UpdateResult result;
        if constexpr (dynamic) {
            // 遷移と観測を候補上で実行し、成功したときだけ採用する
            result = next->propose_update_log([&](const Entry& old, Entry& entry, Rng& rng) {
                double correction = 0;
                if constexpr (guided) {
                    correction = checked_log(model_.propose_transition(entry.value, old.value, observation, rng));
                    if (correction == neg_inf) return neg_inf;
                } else model_.transition(entry.value, observation, rng);
                return correction + log_likelihood(entry.value, observation);
            }, rng_);
        } else if (temper) {
            result = tempered_update(next, observation, moved, resampled);
        } else {
            // 段階更新が無効なら、粒子をコピーせず成功後にキャッシュへ増分を加える
            increments_.resize(size());
            int i = 0;
            result = next->update_log([&](const Entry& entry) {
                return increments_[i++] = log_likelihood(entry.value, observation);
            });
            if (result.ok) {
                i = 0;
                next->predict([&](Entry& entry, Rng&) { entry.target += increments_[i++]; }, rng_);
            }
        }
        if (!result.ok) return {false, neg_inf, false, {}};
        if (next != &cloud_) std::swap(cloud_, prepared_);
        if constexpr (!dynamic) history_.push_back(observation);
        ++turns_;
        return {true, result.log_normalizer, resampled, moved};
    }

    // 固定パラメータを最新の観測まで使って追加再探索する。重みと履歴は維持する
    MoveResult refine(int steps) requires (!dynamic) { return move(cloud_, steps); }

    template<class Project> requires std::invocable<Project&, const Particle&>
    auto estimate(Project&& project, int top_k = 0) const {
        return cloud_.estimate([&](const Entry& entry) -> decltype(auto) {
            return std::invoke(project, entry.value);
        }, top_k);
    }

    // Model.projectを省略した場合は、数値または数値列の粒子自体を平均する
    auto estimate(int top_k = 0) const {
        return estimate([&](const Particle& p) -> decltype(auto) {
            if constexpr (requires { model_.project(p); }) return model_.project(p);
            else return p;
        }, top_k);
    }

    template<class Consume>
    double for_each_weighted(Consume&& consume, int top_k = 0) const {
        return cloud_.for_each_weighted([&](const Entry& entry, double weight) {
            consume(entry.value, weight);
        }, top_k);
    }

    template<class Output, class Project> requires std::invocable<Project&, const Particle&>
    double estimate_into(Output&& buffer, Project&& project, int top_k = 0) const {
        return cloud_.estimate_into(buffer, [&](const Entry& entry) -> decltype(auto) {
            return std::invoke(project, entry.value);
        }, top_k);
    }

    template<class Output>
    double estimate_into(Output&& buffer, int top_k = 0) const {
        return estimate_into(buffer, [&](const Particle& p) -> decltype(auto) {
            if constexpr (requires { model_.project(p); }) return model_.project(p);
            else return p;
        }, top_k);
    }

    int size() const { return cloud_.size(); }
    double ess() const { return cloud_.ess(); }
    int observation_count() const { return turns_; }
    template<class SamplingRng>
    void sample_indices(std::span<int> output, SamplingRng& rng) const {
        cloud_.sample_indices(output, rng);
    }
    auto weights() const { return cloud_.weights(); }
    auto log_weights() const { return cloud_.log_weights(); }
    auto particles() const {
        return cloud_.particles() | std::views::transform([](const Entry& entry) -> const Particle& {
            return entry.value;
        });
    }
    // 通常はspan。boolはvector<bool>が非連続のため読み取り専用rangeを返す。
    // log_likelihood_historyは同じviewを受け取る。const auto&なら両方に対応できる。
    auto history() const requires (!dynamic) {
        if constexpr (std::is_same_v<Observation, bool>) return std::views::all(history_);
        else return std::span<const Observation>(history_);
    }
};

#if __INCLUDE_LEVEL__ == 0


// テストはヘッダを直接コンパイルした場合だけ有効。NDEBUGでも検証を省略しない
struct ParticleCloudTests {
    using PF = ParticleCloud<int>;
    using Rng = std::mt19937_64;
    static constexpr double inf = std::numeric_limits<double>::infinity();
    inline static int checks = 0;

    static void require(bool condition, const char* label) {
        ++checks;
        if (!condition) {
            std::cerr << "FAILED: " << label << '\n';
            std::exit(1);
        }
    }

    static void near(double actual, double expected, double tolerance = 1e-11) {
        ++checks;
        if (!(std::abs(actual - expected) <= tolerance * (1 + std::abs(expected)))) {
            std::cerr << std::setprecision(17) << "FAILED: " << actual << " != " << expected << '\n';
            std::exit(1);
        }
    }

    static double uniform(Rng& rng) { return std::generate_canonical<double, 53>(rng); }
    static double normal(Rng& rng) { return std::normal_distribution<double>{}(rng); }

    template<class P>
    static void invariant(const ParticleCloud<P>& pf) {
        double sum = 0, squares = 0;
        require(pf.weights().size() == pf.particles().size(), "weight size");
        require(pf.log_weights().size() == pf.particles().size(), "log weight size");
        for (int i = 0; i < pf.size(); ++i) {
            double weight = pf.weights()[i];
            require(weight >= 0 && weight <= 1, "weight range");
            near(weight, std::exp(pf.log_weights()[i]));
            sum += weight;
            squares += weight * weight;
        }
        near(sum, 1);
        near(pf.ess(), 1 / squares);
    }

    static void basics() {
        PF pf;
        require(pf.size() == 0 && pf.particles().empty() && pf.weights().empty() &&
                pf.log_weights().empty(), "empty constructor");
        Rng rng(42), saved = rng;
        int calls = 0;
        pf.initialize(5, [&](Rng& random) { ++calls; return int(random() % 100); }, rng);
        require(calls == 5, "initialize count");
        for (int p : pf.particles()) require(p == int(saved() % 100), "initialize RNG");
        near(pf.ess(), 5);
        auto before = rng;
        require(!pf.resample_if_needed(rng) && rng == before, "no resample consumes no RNG");
        pf.initialize(1, [](Rng&) { return 17; }, rng);
        near(pf.update_log([](int) { return 700.0; }).log_normalizer, 700);
        near(pf.estimate([](int p) { return double(p); }).mean, 17);
        invariant(pf);

        pf.assign({0, 1, 2});
        calls = 0;
        auto update = pf.update_log([&](int p) { ++calls; return std::log(double(p + 1)); });
        require(update.ok && calls == 3, "update once per particle");
        near(update.log_normalizer, std::log(2.0));
        near(pf.weights()[0], 1.0 / 6);
        near(pf.weights()[1], 2.0 / 6);
        near(pf.weights()[2], 3.0 / 6);
        const PF old = pf;
        update = pf.update_log([](int) { return -inf; });
        require(!update.ok && update.log_normalizer == -inf, "all impossible");
        require(std::ranges::equal(pf.weights(), old.weights()), "failed update keeps weights");
        require(!pf.assign({9, 8}, std::array{-inf, -inf}), "failed assign");
        require(std::ranges::equal(pf.particles(), old.particles()), "failed assign keeps particles");
        require(std::ranges::equal(pf.log_weights(), old.log_weights()), "failed assign keeps logs");

        pf.assign({0, 1}, std::array{-1000.0, 0.0});
        require(pf.weights()[0] == 0 && std::isfinite(pf.log_weights()[0]), "finite underflow");
        pf.update_log([](int p) { return p == 0 ? 1000.0 : 0.0; });
        near(pf.weights()[0], 0.5);
        pf.assign({0, 1, 2}, std::array{-inf, 0.0, -inf});
        pf.update_log([](int) { return 700.0; });
        require(pf.weights()[0] == 0 && pf.log_weights()[0] == -inf, "true zero remains zero");
        near(pf.ess(), 1);
        // 内部viewを初期重みとして渡しても入力を先に読み終える
        require(pf.assign({3, 4, 5}, pf.log_weights()), "assign borrowed log weights");
        near(pf.weights()[1], 1);

        PF disabled(0);
        disabled.assign({0, 1}, std::array{0.0, -inf});
        before = rng;
        require(!disabled.resample_if_needed(rng) && rng == before, "ESS disabled");
        PF boundary(0.5);
        boundary.assign({0, 1}, std::array{0.0, -inf});
        require(!boundary.resample_if_needed(rng), "ESS equality does not resample");
        PF enabled(1);
        enabled.assign({0, 1}, std::array{0.0, -inf});
        require(enabled.resample_if_needed(rng), "ESS triggers resample");
        invariant(enabled);
    }

    static void estimates() {
        PF pf;
        Rng rng(7);
        for (int n = 1; n <= 40; ++n) {
            std::vector<int> particles(n), order(n);
            std::iota(particles.begin(), particles.end(), 0);
            std::iota(order.begin(), order.end(), 0);
            std::vector<double> logs(n);
            for (double& value : logs) value = -double(rng() % 6);
            pf.assign(particles, logs);
            std::sort(order.begin(), order.end(), [&](int a, int b) {
                return logs[a] != logs[b] ? logs[a] > logs[b] : a < b;
            });
            for (int k = 0; k <= n; ++k) {
                const int selected = k == 0 ? n : k;
                double mass = 0, mean = 0;
                std::vector<bool> seen(n, false);
                for (int i = 0; i < selected; ++i) {
                    mass += pf.weights()[order[i]];
                    mean += pf.weights()[order[i]] * order[i];
                }
                const auto result = pf.estimate([](int p) { return double(p); }, k);
                near(result.mean, mean / mass);
                near(result.selected_mass, mass);
                const auto vector_result = pf.estimate([](int p) {
                    return std::array{double(p), double(p * p)};
                }, k);
                near(vector_result.mean[0], result.mean);
                double total = 0;
                const double visit_mass = pf.for_each_weighted([&](int p, double weight) {
                    require(!seen[p], "selected exactly once");
                    seen[p] = true;
                    total += weight;
                }, k);
                near(visit_mass, mass);
                near(total, 1);
                for (int i = 0; i < n; ++i) require(seen[order[i]] == (i < selected), "top k members");
            }
        }
        pf.assign({0, 1, 2}, std::array{-inf, 0.0, -inf});
        near(pf.estimate([](int p) { return double(p); }, 2).mean, 1);
    }

    struct EndpointRng {
        using result_type = std::uint64_t;
        result_type value;
        static constexpr result_type min() { return 0; }
        static constexpr result_type max() { return UINT64_MAX; }
        result_type operator()() { return value; }
    };

    static void resampling() {
        {
            Rng rng(8);
            PF pf;
            for (int trial = 0; trial < 250; ++trial) {
                int n = 1 + int(rng() % 80), count = 1 + int(rng() % 100);
                std::vector<int> particles(n);
                std::iota(particles.begin(), particles.end(), 0);
                std::vector<double> logs(n);
                for (double& value : logs) value = rng() % 3 == 0 ? -inf : -double(rng() % 12);
                logs[rng() % std::uint64_t(n)] = 0;
                pf.assign(particles, logs);
                // 独立なCDF配列＋upper_boundで親を求める
                std::vector<double> cdf(n);
                std::partial_sum(pf.weights().begin(), pf.weights().end(), cdf.begin());
                int last = n - 1;
                while (pf.weights()[last] == 0) --last;
                Rng reference_rng = rng;
                double offset = uniform(reference_rng);
                std::vector<int> expected;
                for (int j = 0; j < count; ++j) {
                    double u = (j + offset) / count;
                    int parent = int(std::upper_bound(cdf.begin(), cdf.end(), u) - cdf.begin());
                    expected.push_back(std::min(parent, last));
                }
                const int distinct = pf.resample(rng, count);
                require(std::ranges::equal(pf.particles(), expected), "CDF reference");
                require(rng == reference_rng, "resampling RNG consumption");
                require(distinct == int(std::set<int>(expected.begin(), expected.end()).size()), "distinct parents");
                invariant(pf);
            }
            for (std::uint64_t endpoint : {std::uint64_t(0), UINT64_MAX}) {
                EndpointRng endpoint_rng{endpoint};
                pf.assign({0, 1, 2, 3, 4}, std::array{-inf, 0.0, -inf, -1.0, -inf});
                pf.resample(endpoint_rng, 19);
                for (int p : pf.particles()) require(p == 1 || p == 3, "zero endpoints not selected");
            }
            std::array<double, 3> counts{};
            for (int repetition = 0; repetition < 8000; ++repetition) {
                pf.assign({0, 1, 2}, std::array{std::log(0.1), std::log(0.3), std::log(0.6)});
                pf.resample(rng, 11);
                for (int p : pf.particles()) ++counts[p];
            }
            for (int i = 0; i < 3; ++i) near(counts[i] / 88000, std::array{0.1, 0.3, 0.6}[i], 0.008);
        }
        PF equal;
        Rng rng(0);
        equal.assign({0, 1, 2, 3, 4, 5, 6});
        require(equal.resample(rng) == 7, "equal systematic keeps all parents");
    }

    static void state_updates() {
        PF pf;
        Rng rng(17);
        pf.assign({1, 2, 3}, std::array{0.0, -1.0, -2.0});
        const auto old_weights = std::vector<double>(pf.weights().begin(), pf.weights().end());
        pf.predict([](int& p, Rng&) { p += 10; }, rng);
        require(pf.particles()[0] == 11, "predict state");
        require(std::ranges::equal(pf.weights(), old_weights), "predict preserves weight");
        pf.update_log([](int) { return -inf; });
        require(pf.particles()[0] == 11, "failed observation does not undo predict");
        const PF saved = pf;
        auto saved_rng = rng;
        int calls = 0;
        auto result = pf.propose_update_log([&](const int& previous, int& candidate, Rng& random) {
            require(&previous != &candidate && previous == candidate, "separate proposal copy");
            candidate += int(random() % 10);
            ++calls;
            return -inf;
        }, rng);
        require(!result.ok && calls == 3 && rng != saved_rng, "atomic failure leaves RNG consumed");
        require(std::ranges::equal(pf.particles(), saved.particles()), "atomic state rollback");
        require(std::ranges::equal(pf.weights(), saved.weights()), "atomic weights rollback");
        result = pf.propose_update_log([](const int& previous, int& candidate, Rng&) {
            candidate = previous * 2;
            return std::log(double(previous));
        }, rng);
        require(result.ok, "atomic success");
        double sum = 0;
        for (int i = 0; i < 3; ++i) sum += saved.weights()[i] * saved.particles()[i];
        for (int i = 0; i < 3; ++i) {
            require(pf.particles()[i] == 2 * saved.particles()[i], "proposal candidate committed");
            near(pf.weights()[i], saved.weights()[i] * saved.particles()[i] / sum);
        }
        near(result.log_normalizer, std::log(sum));
        // 異なる粒子数を交互に使い、作業領域の古い粒子を混入させない
        for (int n : {20, 1, 7, 2, 31, 3}) {
            pf.resample(rng, n);
            require(pf.propose_update_log([](const int&, int& candidate, Rng&) {
                ++candidate;
                return 0.0;
            }, rng).ok, "buffer resize proposal");
            require(pf.size() == n, "buffer logical size");
            invariant(pf);
        }
    }

    struct RichParticle {
        inline static int copies = 0;
        std::vector<double> data;
        int cache;
        RichParticle() = delete;
        explicit RichParticle(int value) : data(8, double(value)), cache(value) {}
        RichParticle(const RichParticle& rhs) : data(rhs.data), cache(rhs.cache) { ++copies; }
        RichParticle& operator=(const RichParticle& rhs) {
            data = rhs.data;
            cache = rhs.cache;
            ++copies;
            return *this;
        }
        RichParticle(RichParticle&&) = default;
        RichParticle& operator=(RichParticle&&) = default;
    };

    static void moves_and_copies() {
        Rng rng(33);
        ParticleCloud<RichParticle> rich;
        rich.initialize(5, [](Rng&) { return RichParticle(1); }, rng);
        rich.resample(rng);
        int target_calls = 0, proposal_calls = 0;
        auto target = [&](const RichParticle& p) { ++target_calls; return p.cache == 1 ? 0.0 : -inf; };
        auto proposal = [&](RichParticle& p, Rng&) { ++proposal_calls; p.data[0] = 19; p.cache = 2; return 0.0; };
        auto result = rich.rejuvenate(0, target, proposal, rng);
        require(target_calls == 0 && proposal_calls == 0 && result.attempted == 0, "zero MH steps");
        result = rich.rejuvenate(4, target, proposal, rng);
        require(result.attempted == 20 && result.accepted == 0, "reject impossible MH");
        require(target_calls == 25 && proposal_calls == 20, "target cached inside steps");
        for (const auto& p : rich.particles()) require(p.data[0] == 1 && p.cache == 1, "reject keeps cache");
        result = rich.rejuvenate(2, [](const RichParticle&) { return 0.0; },
            [](RichParticle& p, Rng&) { ++p.cache; p.data[0] += 1; return 0.0; }, rng);
        require(result.accepted == 10, "certain MH acceptance");
        auto branch = rich;
        RichParticle::copies = 0;
        int calls = 0;
        rich.rejuvenate(0, [&](RichParticle&, Rng&) { ++calls; }, rng);
        rich.rejuvenate(3, [&](RichParticle& p, Rng&) { ++calls; ++p.cache; }, rng);
        require(calls == 15 && RichParticle::copies == 0, "kernel no copies");
        require(branch.particles()[0].cache == 3 && rich.particles()[0].cache == 6, "independent branch");
        auto moved = std::move(branch);
        require(moved.size() == 5, "move filter");

        PF pf;
        pf.assign({0, 1, 2}, std::array{-inf, -1000.0, 0.0});
        std::array<int, 3> visits{};
        pf.rejuvenate(2, [&](int& p, Rng&) { ++visits[p]; }, rng);
        require(visits == std::array{0, 2, 2}, "kernel distinguishes true zero and underflow");
        const auto integer_result = pf.rejuvenate(2, [](int) { return 0.0; }, [](int&, Rng&) { return -inf; }, rng);
        require(integer_result.attempted == 4 && integer_result.accepted == 0, "MH reverse support");

        // 非対称独立提案q(1)=0.8、target(1)=0.3。詳細釣り合いを別式で確認
        const std::array<double, 2> pi{0.7, 0.3}, q{0.2, 0.8};
        near(pi[0] * q[1] * std::min(1.0, pi[1] * q[0] / (pi[0] * q[1])),
             pi[1] * q[0] * std::min(1.0, pi[0] * q[1] / (pi[1] * q[0])));
        pf.initialize(16000, [](Rng&) { return 0; }, rng);
        pf.rejuvenate(30, [&](int p) { return std::log(pi[p]); }, [&](int& p, Rng& random) {
            const int previous = p;
            p = uniform(random) < q[1] ? 1 : 0;
            return std::log(q[previous]) - std::log(q[p]);
        }, rng);
        near(pf.estimate([](int p) { return double(p); }).mean, 0.3, 0.02);

        // 正のパラメータ: Gamma(2,1)を対数座標にし、Jacobianを含めてMH更新
        ParticleCloud<double> log_coordinate;
        log_coordinate.initialize(12000, [](Rng&) { return 0.0; }, rng);
        log_coordinate.rejuvenate(120, [](double z) { return 2 * z - std::exp(z); },
            [](double& z, Rng& random) { z += normal(random); return 0.0; }, rng);
        near(log_coordinate.estimate([](double z) { return std::exp(z); }).mean, 2, 0.035);
    }

    static void mh_boundaries() {
        // 解析的な受理確率が二進数で表せる例で、境界・無効候補・非対称補正を確認する
        struct CountingRng : EndpointRng {
            int calls = 0;
            result_type operator()() { ++calls; return value; }
        };
        const std::array<std::uint64_t, 6> raw{0, UINT64_C(1) << 61, UINT64_C(1) << 62,
            UINT64_C(1) << 63, UINT64_C(7) << 61, UINT64_MAX};
        const std::array<double, 6> draws{0, 0.125, 0.25, 0.5, 0.875, std::nextafter(1.0, 0.0)};
        for (double target_ratio : {0.0, 0.25, 0.5, 1.0, 2.0, 4.0}) {
            for (double proposal_ratio : {0.0, 0.5, 1.0, 2.0}) {
                for (std::size_t j = 0; j < raw.size(); ++j) {
                    PF pf;
                    pf.assign({0});
                    CountingRng rng{{raw[j]}, 0};
                    auto result = pf.rejuvenate(1,
                        [&](int p) { return p == 0 ? 0.0 : std::log(target_ratio); },
                        [&](int& p, CountingRng&) { p = 1; return std::log(proposal_ratio); }, rng);
                    const bool accepted = draws[j] < std::min(1.0, target_ratio * proposal_ratio);
                    require(result.attempted == 1 && result.accepted == int(accepted), "MH exact probability boundary");
                    require(pf.particles()[0] == int(accepted), "MH boundary state");
                    require(rng.calls == 1, "MH consumes one acceptance draw");
                    near(pf.weights()[0], 1);
                    near(pf.log_weights()[0], 0);
                }
            }
        }
    }

    static void random_operations() {
        Rng rng(101);
        for (int run = 0; run < 35; ++run) {
            PF pf;
            std::vector<int> particles(9);
            std::iota(particles.begin(), particles.end(), 0);
            std::vector<double> logs(9, -std::log(9.0));
            pf.assign(particles);
            for (int turn = 0; turn < 120; ++turn) {
                int op = int(rng() % 7);
                if (op <= 2) {
                    std::array<double, 17> increments{};
                    for (double& value : increments) value = double(int(rng() % 9) - 4) / 7;
                    auto increment = [&](int p) { return increments[p % 17]; };
                    pf.update_log(increment);
                    for (std::size_t i = 0; i < logs.size(); ++i) logs[i] += increment(particles[i]);
                } else if (op == 3) {
                    pf.predict([](int& p, Rng&) { p = (p * 3 + 1) % 100; }, rng);
                    for (int& p : particles) p = (p * 3 + 1) % 100;
                } else if (op == 4) {
                    pf.propose_update_log([](const int& previous, int& candidate, Rng&) {
                        candidate = (previous + 7) % 100;
                        return -0.03 * candidate;
                    }, rng);
                    for (std::size_t i = 0; i < logs.size(); ++i) {
                        particles[i] = (particles[i] + 7) % 100;
                        logs[i] -= 0.03 * particles[i];
                    }
                } else if (op == 5) {
                    pf.resample(rng, 1 + int(rng() % 21));
                    particles.assign(pf.particles().begin(), pf.particles().end());
                    logs.assign(particles.size(), -std::log(double(particles.size())));
                } else {
                    auto copy = pf;
                    copy.rejuvenate(1, [](int& p, Rng&) { ++p; }, rng);
                    require(copy.particles()[0] == pf.particles()[0] + 1, "random copy branch");
                }
                const double largest = *std::max_element(logs.begin(), logs.end());
                double total = 0;
                for (double value : logs) total += std::exp(value - largest);
                for (std::size_t i = 0; i < logs.size(); ++i) {
                    logs[i] -= largest + std::log(total);
                    near(pf.log_weights()[i], logs[i]);
                }
                require(std::ranges::equal(pf.particles(), particles), "random particle states");
                invariant(pf);
            }
        }
        PF sequential, batch;
        sequential.assign({0, 1, 2, 3});
        batch = sequential;
        for (int t = 0; t < 100; ++t) sequential.update_log([&](int p) { return -0.001 * (p - t % 4) * (p - t % 4); });
        batch.update_log([](int p) {
            double sum = 0;
            for (int t = 0; t < 100; ++t) sum -= 0.001 * (p - t % 4) * (p - t % 4);
            return sum;
        });
        for (int i = 0; i < 4; ++i) near(sequential.weights()[i], batch.weights()[i]);
        PF shifted = batch, plain = batch;
        auto a = plain.update_log([](int p) { return -double(p); });
        auto b = shifted.update_log([](int p) { return 10000.0 - p; });
        near(b.log_normalizer - a.log_normalizer, 10000);
        for (int i = 0; i < 4; ++i) near(shifted.weights()[i], plain.weights()[i], 1e-10);
    }

    static void scenarios() {
        Rng rng(77);
        // 複数の固定パラメータ: グリッドの事前分布を逐次更新し、全履歴再計算と照合
        using Vector = std::array<double, 2>;
        ParticleCloud<Vector> regression;
        std::vector<Vector> grid;
        for (int a = -4; a <= 4; ++a) for (int b = -4; b <= 4; ++b) grid.push_back({a * 0.5, b * 0.5});
        regression.assign(grid);
        std::vector<std::pair<double, double>> history{{1, 1.5}, {-1, 0.2}, {2, 2.1}};
        for (auto [x, y] : history) regression.update_log([&](const Vector& p) {
            return -0.5 * std::pow(p[0] * x + p[1] - y, 2);
        });
        auto full = [&](const Vector& p) {
            double value = 0;
            for (auto [x, y] : history) value -= 0.5 * std::pow(p[0] * x + p[1] - y, 2);
            return value;
        };
        ParticleCloud<Vector> reference;
        reference.assign(grid);
        reference.update_log(full);
        for (int i = 0; i < regression.size(); ++i) near(regression.weights()[i], reference.weights()[i]);
        auto estimate = regression.estimate([](const Vector& p) { return p; }, 8);
        require(estimate.selected_mass > 0 && estimate.selected_mass <= 1, "multivariate top k");
        auto checkpoint = regression;
        auto rng_checkpoint = rng;
        regression.resample(rng, 120);
        auto resumed = checkpoint;
        resumed.resample(rng_checkpoint, 120);
        require(std::ranges::equal(regression.particles(), resumed.particles()), "checkpoint RNG replay");

        // 観測修正: 支持が共通な尤度比を適用。新たな仮説が必要なら呼び出し側で再初期化する
        checkpoint.update_log([&](const Vector& p) {
            auto [x, y] = history.back();
            return 0.5 * std::pow(p[0] * x + p[1] - y, 2)
                 - 0.5 * std::pow(p[0] * x + p[1] - 1.8, 2);
        });
        history.back().second = 1.8;
        reference.assign(grid);
        reference.update_log(full);
        for (int i = 0; i < checkpoint.size(); ++i) near(checkpoint.weights()[i], reference.weights()[i]);

        // 観測誘導提案: x'~N(x,1), y|x'~N(x',0.25)。補正は予測尤度に簡約できる
        ParticleCloud<double> guided;
        guided.initialize(24000, [](Rng&) { return 0.0; }, rng);
        const double y = 2, variance = 0.2, mean = 1.6;
        guided.propose_update_log([&](const double& previous, double& candidate, Rng& random) {
            candidate = previous + 0.8 * (y - previous) + std::sqrt(variance) * normal(random);
            return -0.5 * (std::log(2 * std::numbers::pi * 1.25) + (y - previous) * (y - previous) / 1.25);
        }, rng);
        near(guided.estimate([](double p) { return p; }).mean, mean, 0.008);
        near(guided.estimate([&](double p) { return (p - mean) * (p - mean); }).mean, variance, 0.012);
        near(guided.ess(), guided.size());

        // 条件付きGaussian: 予測分布の尤度で重みを更新し、粒子内の平均・分散も確定
        struct Conditional { int mode; double mean, variance; };
        ParticleCloud<Conditional> mixture;
        mixture.assign({{0, -1, 1}, {1, 2, 0.5}});
        const auto prior = mixture;
        auto result = mixture.propose_update_log([](const Conditional& old, Conditional& candidate, Rng&) {
            double s = old.variance + 0.25, gain = old.variance / s;
            candidate.mean = old.mean + gain * (0.3 - old.mean);
            candidate.variance = (1 - gain) * old.variance;
            return -0.5 * (std::log(2 * std::numbers::pi * s) + (0.3 - old.mean) * (0.3 - old.mean) / s);
        }, rng);
        require(result.ok, "conditional update");
        std::array<double, 2> likelihood{};
        for (int i = 0; i < 2; ++i) {
            auto p = prior.particles()[i];
            double s = p.variance + 0.25;
            likelihood[i] = std::exp(-0.5 * (0.3 - p.mean) * (0.3 - p.mean) / s) / std::sqrt(2 * std::numbers::pi * s);
        }
        near(mixture.weights()[0], likelihood[0] / (likelihood[0] + likelihood[1]));
        double mixture_mean = mixture.estimate([](const Conditional& p) { return p.mean; }).mean;
        double total_variance = mixture.estimate([&](const Conditional& p) {
            return p.variance + (p.mean - mixture_mean) * (p.mean - mixture_mean);
        }).mean;
        require(total_variance > mixture.estimate([&](const Conditional& p) {
            return (p.mean - mixture_mean) * (p.mean - mixture_mean);
        }).mean, "total variance includes within particle variance");

        // 意思決定: 期待費用は粒子ごとに計算し、非線形な費用に平均値だけを代入しない
        PF decisions;
        decisions.assign({0, 10}, std::array{std::log(0.9), std::log(0.1)});
        std::array<double, 2> costs{};
        decisions.for_each_weighted([&](int p, double weight) {
            costs[0] += weight * (p > 5 ? 100 : 0);
            costs[1] += weight * 3;
        });
        require(costs[0] > costs[1] && decisions.estimate([](int p) { return double(p); }).mean < 5,
                "distribution based decision");
    }

    static int death_test(std::string_view name) {
        PF pf;
        Rng rng(0);
        if (name == "empty") pf.update_log([](int) { return 0.0; });
        if (name == "ratio") { PF invalid(-0.1); }
        if (name == "initialize") pf.initialize(0, [](Rng&) { return 0; }, rng);
        if (name == "assign_empty") pf.assign({});
        if (name == "assign_size") pf.assign({0, 1}, std::array{0.0});
        pf.assign({0, 1});
        if (name == "nan") pf.update_log([](int) { return std::numeric_limits<double>::quiet_NaN(); });
        if (name == "infinity") pf.update_log([](int) { return inf; });
        if (name == "top_k") pf.estimate([](int p) { return double(p); }, 3);
        if (name == "negative_k") pf.for_each_weighted([](int, double) {}, -1);
        if (name == "resample") pf.resample(rng, -1);
        if (name == "steps") pf.rejuvenate(-1, [](int&, Rng&) {}, rng);
        if (name == "target") pf.rejuvenate(1, [](int) { return -inf; }, [](int&, Rng&) { return 0.0; }, rng);
        if (name == "proposal") pf.propose_update_log([](const int&, int&, Rng&) { return inf; }, rng);
        return 2; // assertしなければ親プロセス側の検証は失敗する
    }

    static int run() {
        basics();
        estimates();
        resampling();
        state_updates();
        moves_and_copies();
        mh_boundaries();
        random_operations();
        scenarios();
        std::cout << "PASS: 8 suites, " << checks << " checks\n";
        return 0;
    }
};

struct ManagedTests : ParticleCloudTests {
    struct Table {
        using Particle = int;
        using Observation = std::array<double, 3>;
        int index = 0;
        int sample(Rng&) { return index++ % 3; }
        double likelihood(int p, const Observation& o) const { return o[p]; }
        double project(int p) const { return double(p); }
    };
    struct LogTable {
        using Particle = int;
        using Observation = Table::Observation;
        int index = 0;
        int sample(Rng&) { return index++ % 3; }
        double log_likelihood(int p, const Observation& o) const { return std::log(o[p]); }
    };

    template<class A, class B>
    static void same(const A& a, const B& b) {
        require(a.size() == b.size(), "managed size");
        require(a.observation_count() == b.observation_count(), "managed turns");
        near(a.ess(), b.ess(), 0);
        for (int i = 0; i < a.size(); ++i) {
            require(a.particles()[i] == b.particles()[i], "managed particle");
            near(a.weights()[i], b.weights()[i], 0);
            require(a.log_weights()[i] == b.log_weights()[i], "managed log weight");
        }
    }

    static void exact_updates() {
        ParticleFilter<Table> pf(Table{}, {.count=3, .ess_ratio=0, .move_steps=0});
        ParticleFilter<LogTable> logarithmic(LogTable{}, {.count=3, .ess_ratio=0, .move_steps=0});
        std::array<double, 3> truth{1.0/3,1.0/3,1.0/3};
        Rng rng(180);
        for (int turn = 0; turn < 120; ++turn) {
            Table::Observation o{0.01 + 3 * uniform(rng),0.01 + 3 * uniform(rng),0.01 + 3 * uniform(rng)};
            const auto saved = o;
            const double z = std::inner_product(truth.begin(), truth.end(), o.begin(), 0.0);
            for (int j = 0; j < 3; ++j) truth[j] *= o[j] / z;
            auto result = pf.observe(o);
            auto log_result = logarithmic.observe(o);
            require(result.ok && log_result.ok && !result.resampled, "probability update");
            near(result.log_predictive, std::log(z));
            same(pf, logarithmic);
            for (int j = 0; j < 3; ++j) near(pf.weights()[j], truth[j]);
            o = {99,99,99};
            require(pf.history().back() == saved, "history owns observation");
            auto largest = int(std::max_element(truth.begin(), truth.end()) - truth.begin());
            near(pf.estimate(1).mean, largest);
            near(pf.estimate(1).selected_mass, truth[largest]);
            near(pf.estimate().mean, truth[1]+2*truth[2]);
            near(pf.estimate([](int p) { return double(p*p); }).mean, truth[1]+4*truth[2]);
            double mass = 0;
            pf.for_each_weighted([&](int, double w) { mass += w; }, 2);
            near(mass, 1);
        }
        auto before = pf;
        auto result = pf.observe({0,0,0});
        require(!result.ok && result.log_predictive == -inf && !result.resampled, "all zero likelihood");
        same(pf, before);
        require(pf.history().size() == 120, "failed history unchanged");
        require(pf.refine(0).attempted == 0, "zero refine");
        pf.reset();
        require(pf.history().empty() && pf.observation_count() == 0, "reset history");
        near(pf.estimate().mean, 1);

        ParticleFilter<Table> prepared(Table{}, {.count=300, .ess_ratio=0.8, .move_steps=2, .tempering_ess_ratio=0});
        require(prepared.observe({1,0,0}).ok, "concentrated observation");
        const auto old = prepared;
        require(!prepared.observe({0,0,0}).ok, "failed observation after preparation");
        same(prepared, old);
        require(prepared.history().size() == 1, "failed prepared history unchanged");
        auto second = prepared.observe({1,1,1});
        require(second.ok && second.resampled && second.moves.attempted == 600, "automatic resample and moves");
        near(prepared.estimate().mean, 0);
        near(prepared.ess(), 300);
    }

    struct Categorical {
        using Particle = int;
        using Observation = std::array<double, 3>;
        static constexpr std::array<double, 3> priors{0.1,0.2,0.7};
        int sample(Rng& rng) { return std::discrete_distribution<int>(priors.begin(),priors.end())(rng); }
        double likelihood(int p, const Observation& o) const { return o[p]; }
    };
    struct Symmetric : Categorical {
        double prior(int p) const { return priors[p]; }
        void propose(int& p, Rng& rng) { p = std::uniform_int_distribution<int>(0,2)(rng); }
    };
    struct Asymmetric : Categorical {
        double log_prior(int p) const { return std::log(priors[p]); }
        double propose(int& p, Rng& rng) {
            const std::array<double, 3> q{0.6,0.3,0.1};
            const int old = p;
            p = std::discrete_distribution<int>(q.begin(),q.end())(rng);
            return std::log(q[old])-std::log(q[p]);
        }
    };

    template<class Model>
    static void posterior_moves() {
        ParticleFilter<Model> pf(Model{}, {.count=16000,.ess_ratio=0,.move_steps=0});
        const std::array<double,3> o{0.9,0.4,0.1};
        require(pf.observe(o).ok, "categorical observe");
        auto before = pf;
        auto result = pf.refine(10);
        require(result.attempted == 160000 && result.accepted > 0, "MH performed");
        std::array<double,3> actual{};
        pf.for_each_weighted([&](int p, double w) { actual[p] += w; });
        for (int j=0;j<3;++j) near(actual[j], Categorical::priors[j]*o[j]/0.24, 0.016);
        for (int i=0;i<pf.size();++i) near(pf.weights()[i],before.weights()[i],0);
        require(pf.history().size()==1, "refine retains history");
    }

    struct Linear : UniformBox<2> {
        using Observation = std::array<double,2>;
        Linear() : UniformBox({-3,-3},{3,3}) {}
        double log_likelihood(const Particle& p, const Observation& o) const {
            const double error=p[0]*o[0]+p[1]-o[1];
            return -0.5*error*error-0.5*std::log(2*std::numbers::pi);
        }
    };
    struct Counted : Linear {
        std::shared_ptr<int> evaluations;
        explicit Counted(std::shared_ptr<int> count) : evaluations(std::move(count)) {}
        double log_likelihood_history(const Particle& p, std::span<const Observation> history) const {
            ++*evaluations;
            double value=0;
            for (const auto& o:history) value+=log_likelihood(p,o);
            return value;
        }
    };

    static void history_cache_and_box() {
        auto count=std::make_shared<int>(0);
        ParticleFilter<Counted> pf(Counted(count),{.count=256,.ess_ratio=0,.move_steps=0});
        ParticleFilter<Linear> ordinary(Linear{}, {.count=256,.ess_ratio=0,.move_steps=0});
        for(int turn=0;turn<30;++turn) {
            Linear::Observation o{(turn%7-3)*0.25, (turn%7-3)*0.3-0.7};
            pf.observe(o);
            ordinary.observe(o);
        }
        same(pf,ordinary);
        auto result=pf.refine(4);
        auto reference=ordinary.refine(4);
        require(result.attempted==reference.attempted && result.accepted==reference.accepted,"history fast path moves");
        require(*count>0 && *count<=256*4,"current target cached; invalid prior skipped");
        same(pf,ordinary);
        for(const auto& p:pf.particles()) for(double x:p) require(x>=-3 && x<=3,"box constraints");
        auto snapshot=ordinary;
        for(int turn=0;turn<20;++turn) {
            Linear::Observation o{turn*0.1,1.2*turn*0.1-0.7};
            ordinary.observe(o);
            snapshot.observe(o);
            same(ordinary,snapshot);
        }
        ordinary.refine(2);
        snapshot.refine(2);
        same(ordinary,snapshot); // RNGとnormal_distributionの内部状態もコピーされる
        auto mean=ordinary.estimate().mean;
        near(mean[0],1.2,0.15);
        near(mean[1],-0.7,0.15);

        ParticleFilter<Linear> periodic(Linear{}, {.count=32,.ess_ratio=0,.move_steps=1,.move_interval=3,.tempering_ess_ratio=0});
        for(int turn=0;turn<10;++turn) {
            auto r=periodic.observe({0,0});
            require(r.resampled==(turn>0 && turn%3==0),"periodic preparation");
            require(r.moves.attempted==(r.resampled?32:0),"periodic move count");
        }
    }

    struct Tracking {
        using Particle = int;
        struct Observation { int shift; bool impossible; };
        int index=0;
        int sample(Rng&) { return index++%3; }
        void transition(int& p,const Observation& o,Rng&) { p+=o.shift; }
        double likelihood(int p,const Observation& o) const { return o.impossible?0:(p%3==0?0.98:0.01); }
    };
    template<class Filter>
    static constexpr bool has_history=requires(const Filter& f) { f.history(); };

    static void dynamic_failure() {
        using Filter=ParticleFilter<Tracking>;
        static_assert(!has_history<Filter>);
        Filter pf(Tracking{}, {.count=300,.ess_ratio=0.8});
        auto initial=pf;
        require(!pf.observe({50,true}).ok,"failed initial transition");
        same(initial,pf);
        auto first=pf.observe({1,false});
        require(first.ok && !first.resampled,"dynamic first observation");
        auto before=pf;
        require(!pf.observe({100,true}).ok,"failed resampled transition");
        same(pf,before);
        auto second=pf.observe({2,false});
        require(second.ok && second.resampled && second.moves.attempted==0,"dynamic resample without static MH");
        for(int p:pf.particles())require(p>=3 && p<=5,"dynamic transition once");
        require(pf.observation_count()==2,"dynamic successful turn count");
    }

    struct HMM {
        using Particle=int;
        using Observation=int;
        static constexpr double f[3][3]={{0.8,0.15,0.05},{0.15,0.7,0.15},{0.05,0.15,0.8}};
        static constexpr double g[3][3]={{0.7,0.2,0.1},{0.2,0.6,0.2},{0.1,0.2,0.7}};
        int sample(Rng& r) { return std::uniform_int_distribution<int>(0,2)(r); }
        void transition(int& p,const int&,Rng& r) { p=std::discrete_distribution<int>(f[p],f[p]+3)(r); }
        double likelihood(int p,int o)const{return g[p][o];}
    };
    static void tracking_reference() {
        for(int seed=0;seed<8;++seed) {
            ParticleFilter<HMM> pf(HMM{}, {.count=8192},Rng(seed));
            Rng rng(seed+800);
            std::array<double,3> truth{1.0/3,1.0/3,1.0/3};
            for(int turn=0;turn<40;++turn) {
                int o=std::uniform_int_distribution<int>(0,2)(rng);
                std::array<double,3> predicted{},actual{};
                for(int j=0;j<3;++j)for(int k=0;k<3;++k) predicted[k]+=truth[j]*HMM::f[j][k];
                double z=0;
                for(int k=0;k<3;++k)z+=predicted[k]*HMM::g[k][o];
                for(int k=0;k<3;++k)truth[k]=predicted[k]*HMM::g[k][o]/z;
                auto result=pf.observe(o);
                require(result.ok,"HMM observe");
                near(result.log_predictive,std::log(z),0.025);
                pf.for_each_weighted([&](int p,double w){actual[p]+=w;});
                for(int k=0;k<3;++k)near(actual[k],truth[k],0.035);
            }
        }
    }

    struct Heavy {
        std::vector<double> values;
        inline static int copies=0;
        explicit Heavy(int n):values(n,1){}
        Heavy(const Heavy& p):values(p.values){++copies;}
        Heavy(Heavy&&)=default;
        Heavy& operator=(const Heavy& p){values=p.values;++copies;return *this;}
        Heavy& operator=(Heavy&&)=default;
    };
    struct HeavyModel {
        using Particle=Heavy;
        using Observation=double;
        Heavy sample(Rng&){return Heavy(64);}
        double likelihood(const Heavy& p,double o)const{return p.values[0]+o;}
    };
    static void copies_and_resample_from() {
        ParticleFilter<HeavyModel> pf(HeavyModel{}, {.count=128,.ess_ratio=0,.move_steps=0});
        Heavy::copies=0;
        for(int turn=0;turn<10;++turn)pf.observe(0.2);
        require(Heavy::copies==0,"static updates do not copy heavy particles");
        auto p=pf.estimate([](const Heavy& x){return x.values[0];});
        near(p.mean,1);
        for(int seed=0;seed<100;++seed) {
            PF source;
            source.assign({0,1,2,3},std::array{-inf,-4.0,0.0,-inf});
            const auto old=source;
            PF target;
            Rng a(seed),b(seed);
            int n=1+seed%12;
            auto reference=source;
            int distinct=target.resample_from(source,a,n);
            require(distinct==reference.resample(b,n),"resample_from distinct");
            require(a==b,"resample_from RNG");
            for(int i=0;i<n;++i)require(target.particles()[i]==reference.particles()[i],"resample_from values");
            for(int i=0;i<4;++i)require(source.log_weights()[i]==old.log_weights()[i],"resample_from preserves source");
        }
    }

    struct SmallRng {
        using result_type=std::uint64_t;
        std::uint64_t state;
        explicit SmallRng(std::uint64_t seed):state(seed+1){}
        static constexpr result_type min(){return 0;}
        static constexpr result_type max(){return UINT64_MAX;}
        result_type operator()(){state^=state<<13;state^=state>>7;return state^=state<<17;}
    };
    struct CustomRngModel {
        using Particle=double;
        using Observation=double;
        template<class Generator> double sample(Generator& rng){return std::generate_canonical<double,53>(rng);}
        double likelihood(double p,double o)const{return 1+p*o;}
    };
    static void custom_rng() {
        ParticleFilter<CustomRngModel,SmallRng> pf(CustomRngModel{}, {.count=64,.move_interval=2},SmallRng(3));
        auto copy=pf;
        for(int t=0;t<10;++t){pf.observe(0.3);copy.observe(0.3);same(pf,copy);}
        require(pf.estimate().mean>0 && pf.estimate().mean<1,"custom RNG estimates");
    }

    // 段階更新を無効化した経路と従来の手動手順が一致することを確認する
    static void manual_workflow() {
        for(int seed=0;seed<16;++seed) {
            Linear model;
            Rng rng(seed);
            ParticleCloud<Linear::Particle> manual(0.6);
            manual.initialize(128,[&](Rng& r){return model.sample(r);},rng);
            ParticleFilter<Linear> managed(Linear{}, {.count=128,.ess_ratio=0.6,.move_steps=3,.move_interval=4,.tempering_ess_ratio=0},Rng(seed));
            std::vector<Linear::Observation> history;
            auto move=[&](int steps) {
                return manual.rejuvenate(steps,[&](const Linear::Particle& p) {
                    double value=model.log_prior(p);
                    if(value==-inf)return value;
                    for(const auto& o:history)value+=model.log_likelihood(p,o);
                    return value;
                },[&](Linear::Particle& p,Rng& r){model.propose(p,r);return 0.0;},rng);
            };
            for(int turn=0;turn<40;++turn) {
                ParticleCloud<Linear::Particle>::MoveResult moved;
                bool resample=manual.ess()<0.6*manual.size() || (turn>0 && turn%4==0);
                if(resample){manual.resample(rng);moved=move(3);}
                Linear::Observation o{(turn%7-3)*0.3,1.2*(turn%7-3)*0.3-0.7+std::sin(double(turn))};
                auto a=managed.observe(o);
                auto b=manual.update_log([&](const Linear::Particle& p){return model.log_likelihood(p,o);});
                history.push_back(o);
                require(a.ok==b.ok && a.resampled==resample,"manual workflow status");
                near(a.log_predictive,b.log_normalizer,0);
                require(a.moves.attempted==moved.attempted && a.moves.accepted==moved.accepted,"manual workflow MH counts");
                if(turn%11==0){
                    auto ma=managed.refine(2);
                    auto mb=move(2);
                    require(ma.attempted==mb.attempted && ma.accepted==mb.accepted,"latest observation refinement");
                }
                for(int i=0;i<128;++i) {
                    require(managed.particles()[i]==manual.particles()[i],"manual workflow particles");
                    near(managed.weights()[i],manual.weights()[i],0);
                    require(managed.log_weights()[i]==manual.log_weights()[i],"manual workflow log weights");
                }
                near(managed.ess(),manual.ess(),0);
            }
        }
    }

    struct HardObservation {
        using Particle=bool;
        using Observation=int;
        std::shared_ptr<int> calls;
        bool sample(Rng& r){return std::bernoulli_distribution(0.5)(r);}
        double log_prior(bool)const{return 0;}
        void propose(bool& p,Rng&){p=!p;}
        double likelihood(bool p,int)const{++*calls;return p?0:0.8;}
    };
    static void impossible_candidate() {
        auto calls=std::make_shared<int>(0);
        ParticleFilter<HardObservation> pf(HardObservation{calls},{.count=128,.ess_ratio=0,.move_steps=0});
        for(int t=0;t<50;++t)pf.observe(t);
        *calls=0;
        auto result=pf.refine(2);
        require(result.attempted>0 && result.accepted==0,"impossible MH candidates rejected");
        require(*calls==result.attempted,"zero likelihood stops remaining history evaluation");
    }

    struct Fields {
        double a,b;
        double sum()const{return a+b;}
    };
    struct FieldsModel {
        using Particle=Fields;
        using Observation=int;
        Fields sample(Rng&){return {2,3};}
        double likelihood(const Fields&,int)const{return 1;}
    };
    static void member_projection() {
        ParticleFilter<FieldsModel> pf(FieldsModel{}, {.count=4,.move_steps=0});
        near(pf.estimate(&Fields::a).mean,2);
        near(pf.estimate(&Fields::sum).mean,5);
        ParticleCloud<Fields> cloud;
        cloud.assign({{1,2},{3,4}});
        near(cloud.estimate(&Fields::a).mean,2);
    }

    static void run() {
        int start=checks;
        exact_updates();
        posterior_moves<Categorical>();
        posterior_moves<Symmetric>();
        posterior_moves<Asymmetric>();
        history_cache_and_box();
        dynamic_failure();
        tracking_reference();
        copies_and_resample_from();
        custom_rng();
        manual_workflow();
        impossible_candidate();
        member_projection();
        std::cout<<"PASS: managed 12 suites, "<<checks-start<<" checks; combined "<<checks<<" checks\n";
    }
};

struct RevisionTests : ManagedTests {
    struct Sequence : std::vector<int> {
        using std::vector<int>::vector;
        inline static int copies = 0;
        Sequence(const Sequence& source) : std::vector<int>(source) { ++copies; }
        Sequence(Sequence&&) = default;
        Sequence& operator=(const Sequence& source) {
            std::vector<int>::operator=(source);
            ++copies;
            return *this;
        }
        Sequence& operator=(Sequence&&) = default;
    };
    struct SequenceModel {
        using Particle = Sequence;
        using Observation = int;
        int index = 0;
        Particle sample(Rng&) { return {++index, 2 * index, -index}; }
        double likelihood(const Particle& p, int observation) const { return double(p[0] + observation); }
    };
    struct ProjectedSequence : SequenceModel {
        const Particle& project(const Particle& p) const { return p; }
    };

    static void numeric_estimates() {
        ParticleCloud<int> scalar;
        scalar.assign({1, 2, 4}, std::array{std::log(1.0), std::log(2.0), std::log(3.0)});
        auto number = scalar.estimate([](int p) { return p; });
        static_assert(std::is_same_v<decltype(number.mean), double>);
        near(number.mean, 17.0 / 6);
        near(scalar.estimate([](int p) { return p == 4; }).mean, 0.5);
        near(scalar.estimate([](int p) { return float(p); }).mean, number.mean);

        ParticleFilter<SequenceModel> filter(SequenceModel{}, {.count=3, .move_steps=0});
        require(filter.observe(0).ok, "sequence observation");
        const auto saved = filter;
        for (int k : {0, 1, 2, 3}) {
            Sequence::copies = 0;
            const auto mean = filter.estimate(k);
            static_assert(std::is_same_v<std::remove_cvref_t<decltype(mean.mean)>, std::vector<double>>);
            require(Sequence::copies == 0, "default numeric sequence projection does not copy particles");
            int calls = 0;
            const auto from_reference = filter.estimate([&](const Sequence& p) -> const Sequence& {
                ++calls;
                return p;
            }, k);
            require(calls == (k == 0 ? 3 : k), "projection once per selected particle");
            require(Sequence::copies == 0, "reference projection not copied");
            const auto from_span = filter.estimate([](const Sequence& p) { return std::span<const int>(p); }, k);
            const auto from_array = filter.estimate([](const Sequence& p) {
                return std::array<int, 3>{p[0], p[1], p[2]};
            }, k);
            static_assert(std::is_same_v<std::remove_cvref_t<decltype(from_array.mean)>, std::array<double, 3>>);
            const auto from_temporary = filter.estimate([](const Sequence& p) {
                return std::vector<int>(p.begin(), p.end());
            }, k);
            for (int j = 0; j < 3; ++j) {
                double reference = 0;
                filter.for_each_weighted([&](const Sequence& p, double weight) { reference += weight * p[j]; }, k);
                near(mean.mean[j], reference, 0);
                near(from_reference.mean[j], reference, 0);
                near(from_span.mean[j], reference, 0);
                near(from_array.mean[j], reference, 0);
                near(from_temporary.mean[j], reference, 0);
            }
            near(mean.selected_mass, from_array.selected_mass, 0);
        }
        const auto owned = filter.estimate().mean;
        filter.reset();
        require(owned != filter.estimate().mean, "mean owns result after filter changes");
        auto a = saved, b = saved;
        a.estimate();
        a.observe(2);
        b.observe(2);
        same(a, b);

        ParticleFilter<ProjectedSequence> projected(ProjectedSequence{}, {.count=3});
        Sequence::copies = 0;
        near(projected.estimate().mean[0], 2);
        require(Sequence::copies == 0, "model projection retains reference");
        ParticleCloud<Sequence> cloud;
        cloud.assign({Sequence{1, 2}, Sequence{3, 4}});
        Sequence::copies = 0;
        near(cloud.estimate([](const Sequence& p) -> const Sequence& { return p; }).mean[1], 3);
        require(Sequence::copies == 0, "cloud projection retains reference");
        ParticleCloud<std::vector<int>> empty;
        empty.assign({{}, {}});
        require(empty.estimate([](const auto& p) -> const auto& { return p; }).mean.empty(), "empty numeric sequences");
        ParticleCloud<std::array<int, 0>> zero_array;
        zero_array.assign({{}, {}});
        require(zero_array.estimate([](const auto& p) -> const auto& { return p; }).mean.empty(), "zero length arrays");
        struct Fields { int coordinates[2]; };
        ParticleCloud<Fields> fields;
        fields.assign({{{1, 3}}, {{5, 7}}});
        require(fields.estimate(&Fields::coordinates).mean == std::vector<double>({3, 5}), "raw array member projection");
    }

    template<std::size_t D>
    static void box_equivalence() {
        std::array<double, D> lower, upper;
        for (std::size_t j = 0; j < D; ++j) { lower[j] = -double(j + 1); upper[j] = double(j + 2); }
        UniformBox<D> fixed(lower, upper);
        UniformBox<> runtime(std::vector<double>(lower.begin(), lower.end()),
                             std::vector<double>(upper.begin(), upper.end()));
        Rng a(31 + D), b(31 + D);
        for (int trial = 0; trial < 300; ++trial) {
            auto p = fixed.sample(a);
            auto q = runtime.sample(b);
            require(q.size() == D, "runtime box dimension");
            for (std::size_t j = 0; j < D; ++j) near(p[j], q[j], 0);
            for (int step = 0; step < 5; ++step) {
                fixed.propose(p, a);
                runtime.propose(q, b);
                for (std::size_t j = 0; j < D; ++j) near(p[j], q[j], 0);
                require(fixed.log_prior(p) == runtime.log_prior(q), "runtime box support");
            }
            require(a == b, "runtime and fixed boxes consume same RNG");
        }
    }

    struct RuntimeLinear : UniformBox<> {
        using Observation = Linear::Observation;
        RuntimeLinear() : UniformBox(std::vector<double>(2, -3), std::vector<double>(2, 3)) {}
        double log_likelihood(const Particle& p, const Observation& o) const {
            const double error = p[0] * o[0] + p[1] - o[1];
            return -0.5 * error * error - 0.5 * std::log(2 * std::numbers::pi);
        }
    };
    static void runtime_filter() {
        for (int seed = 0; seed < 8; ++seed) {
            ParticleFilter<Linear> fixed(Linear{}, {.count=128}, Rng(seed));
            ParticleFilter<RuntimeLinear> runtime(RuntimeLinear{}, {.count=128}, Rng(seed));
            for (int t = 0; t < 30; ++t) {
                Linear::Observation o{(t % 5 - 2) * 0.3, (t % 5 - 2) * 0.4 - 0.7};
                auto a = fixed.observe(o);
                auto b = runtime.observe(o);
                require(a.ok == b.ok && a.resampled == b.resampled, "runtime filter status");
                near(a.log_predictive, b.log_predictive, 0);
                require(a.moves.accepted == b.moves.accepted, "runtime filter moves");
                for (int i = 0; i < fixed.size(); ++i) {
                    for (int j = 0; j < 2; ++j) near(fixed.particles()[i][j], runtime.particles()[i][j], 0);
                    near(fixed.weights()[i], runtime.weights()[i], 0);
                }
                for (int k : {0, 1, 16}) {
                    auto x = fixed.estimate(k);
                    auto y = runtime.estimate(k);
                    for (int j = 0; j < 2; ++j) near(x.mean[j], y.mean[j], 0);
                }
            }
        }
    }

    static void changing_work() {
        for (int seed = 0; seed < 12; ++seed) {
            Linear model;
            Rng rng(seed);
            ParticleCloud<Linear::Particle> manual;
            manual.initialize(97, [&](Rng& r) { return model.sample(r); }, rng);
            ParticleFilter<Linear> filter(Linear{}, {.count=97,.tempering_ess_ratio=0}, Rng(seed));
            std::vector<Linear::Observation> history;
            for (int t = 0; t < 36; ++t) {
                auto param = filter.param();
                if (t == 5) param.count = 64;
                if (t == 10) param.count = 129;
                if (t == 11) { param.move_steps = 0; param.move_interval = 0; }
                if (t == 15) param.ess_ratio = 0;
                if (t == 20) { param.move_steps = 3; param.move_interval = 3; param.ess_ratio = 0.7; }
                if (t == 24) param.count = 1;
                if (t == 25) param.count = 257;
                if (t == 30) param.count = 83;
                if (manual.size() != param.count) manual.resample(rng, param.count);
                filter.set_param(param);
                require(filter.size() == param.count && filter.param().count == param.count, "new work count");
                require(filter.observation_count() == t && filter.history().size() == history.size(), "work change keeps history");
                bool prepare = manual.ess() < param.ess_ratio * manual.size()
                    || (param.move_steps > 0 && param.move_interval > 0 && t > 0 && t % param.move_interval == 0);
                ParticleCloud<Linear::Particle>::MoveResult moves;
                if (prepare) {
                    manual.resample(rng);
                    moves = manual.rejuvenate(param.move_steps, [&](const auto& p) {
                        double target = model.log_prior(p);
                        if (target == -inf) return target;
                        for (const auto& o : history) target += model.log_likelihood(p, o);
                        return target;
                    }, [&](auto& p, Rng& r) { model.propose(p, r); return 0.0; }, rng);
                }
                Linear::Observation o{(t % 7 - 3) * 0.3, 0.3 * (t % 5 - 2) - 0.5};
                const auto a = filter.observe(o);
                const auto b = manual.update_log([&](const auto& p) { return model.log_likelihood(p, o); });
                history.push_back(o);
                require(a.ok && a.resampled == prepare, "work schedule status");
                near(a.log_predictive, b.log_normalizer, 0);
                require(a.moves.attempted == moves.attempted && a.moves.accepted == moves.accepted, "work schedule moves");
                for (int i = 0; i < filter.size(); ++i) {
                    require(filter.particles()[i] == manual.particles()[i], "work change particles");
                    near(filter.weights()[i], manual.weights()[i], 0);
                    near(filter.log_weights()[i], manual.log_weights()[i], 0);
                }
            }
            filter.reset();
            manual.initialize(83, [&](Rng& r) { return model.sample(r); }, rng);
            require(filter.history().empty() && filter.observation_count() == 0, "reset after work change");
            for (int i = 0; i < 83; ++i) require(filter.particles()[i] == manual.particles()[i], "reset retains new count and RNG");
        }
    }

    static void dynamic_work_and_failure() {
        for (int seed = 0; seed < 8; ++seed) {
            Tracking model;
            Rng rng(seed);
            ParticleCloud<int> manual;
            manual.initialize(65, [&](Rng& r) { return model.sample(r); }, rng);
            ParticleFilter<Tracking> filter(Tracking{}, {.count=65}, Rng(seed));
            for (int t = 0; t < 20; ++t) {
                auto param = filter.param();
                param.count = t % 4 == 0 ? 7 : 129;
                param.ess_ratio = t % 3 == 0 ? 0 : 0.9;
                param.move_steps = 10;
                param.move_interval = 1;
                if (manual.size() != param.count) manual.resample(rng, param.count);
                filter.set_param(param);
                bool prepare = manual.ess() < param.ess_ratio * manual.size();
                if (prepare) manual.resample(rng);
                Tracking::Observation o{t % 3, false};
                auto a = filter.observe(o);
                auto b = manual.propose_update_log([&](const int&, int& p, Rng& r) {
                    model.transition(p, o, r);
                    return std::log(model.likelihood(p, o));
                }, rng);
                require(a.ok && a.resampled == prepare && a.moves.attempted == 0, "dynamic work schedule");
                near(a.log_predictive, b.log_normalizer, 0);
                for (int i = 0; i < filter.size(); ++i) {
                    require(filter.particles()[i] == manual.particles()[i], "dynamic resized particles");
                    near(filter.weights()[i], manual.weights()[i], 0);
                }
            }
            const auto before = filter;
            require(!filter.observe({10, true}).ok, "dynamic failure after resizing");
            same(filter, before);
        }
        ParticleFilter<Table> filter(Table{}, {.count=3, .move_steps=0});
        filter.observe({1, 2, 4});
        auto param = filter.param();
        param.count = 17;
        filter.set_param(param);
        const auto before = filter;
        require(!filter.observe({0, 0, 0}).ok, "static failure after resizing");
        same(filter, before);
        require(filter.history().size() == 1, "resized failure preserves history");
        auto changed = before, unchanged = before;
        auto p = changed.param();
        p.move_steps = 3;
        p.move_interval = 2;
        changed.set_param(p);
        same(changed, unchanged);
        changed.set_param(unchanged.param());
        changed.refine(3);
        unchanged.refine(3);
        same(changed, unchanged); // 設定のみの変更は乱数器やモデルの状態を進めない
    }

    static void run() {
        const int start = checks;
        numeric_estimates();
        box_equivalence<1>();
        box_equivalence<2>();
        box_equivalence<8>();
        runtime_filter();
        changing_work();
        dynamic_work_and_failure();
        std::cout << "PASS: revision 7 suites, " << checks - start << " checks; combined " << checks << " checks\n";
    }
};

struct OptimizationTests : ManagedTests {
    static void reflected_box() {
        UniformBox<3> box({-2, 10, -0.01}, {2, 30, 0.03}, 4.0);
        Rng rng(841);
        auto particle = box.sample(rng);
        std::array<double,3> sums{}, squares{};
        double cross = 0;
        constexpr int count = 100000;
        for (int i = 0; i < count; ++i) {
            box.propose(particle, rng);
            require(box.log_prior(particle) == 0, "reflected proposal stays in the box");
            const std::array<double,3> unit{(particle[0]+2)/4, (particle[1]-10)/20, (particle[2]+0.01)/0.04};
            for (int j = 0; j < 3; ++j) {
                sums[j] += unit[j];
                squares[j] += unit[j] * unit[j];
            }
            cross += unit[0] * unit[1];
        }
        for (int j = 0; j < 3; ++j) {
            near(sums[j] / count, 0.5, 0.005);
            near(squares[j] / count, 1.0/3, 0.005);
        }
        near(cross / count, 0.25, 0.005);
    }

    template<class Model>
    static void tempered_posterior() {
        for (int seed = 0; seed < 4; ++seed) {
            ParticleFilter<Model> filter(Model{}, {.count=8192}, Rng(seed+7801));
            std::array<double,3> exact = Categorical::priors;
            double actual_evidence = 0, exact_evidence = 0;
            int intermediate_moves = 0;
            for (const auto& observation : std::vector<std::array<double,3>>{
                    {12, 0.08, 0.002}, {0.03, 0.7, 20}, {0.04, 3, 0.2}, {5, 0.2, 2}}) {
                double z = 0;
                for (int j = 0; j < 3; ++j) z += exact[j] * observation[j];
                for (int j = 0; j < 3; ++j) exact[j] *= observation[j] / z;
                const auto status = filter.observe(observation);
                require(status.ok, "tempered density update succeeds");
                intermediate_moves += status.moves.attempted;
                actual_evidence += status.log_predictive;
                exact_evidence += std::log(z);
                std::array<double,3> actual{};
                filter.for_each_weighted([&](int p, double weight) { actual[p] += weight; });
                for (int j = 0; j < 3; ++j) near(actual[j], exact[j], 0.018);
                near(actual_evidence, exact_evidence, 0.045);
                require(filter.estimate(1).selected_mass < 0.01, "top-k reports selected mass after tempering");
            }
            require(intermediate_moves > 0, "tempering performs posterior moves");
        }
    }

    struct SharedRng {
        using result_type = Rng::result_type;
        std::shared_ptr<Rng> state;
        explicit SharedRng(int seed) : state(std::make_shared<Rng>(seed)) {}
        static constexpr result_type min() { return Rng::min(); }
        static constexpr result_type max() { return Rng::max(); }
        result_type operator()() { return (*state)(); }
    };
    struct CacheModel {
        using Particle = int;
        using Observation = std::array<double,3>;
        template<class Generator> int sample(Generator& rng) {
            return std::discrete_distribution<int>({0.1,0.2,0.7})(rng);
        }
        double log_prior(int p) const { return std::log(Categorical::priors[p]); }
        double log_likelihood(int p, const Observation& o) const { return o[p]; }
        template<class Generator> double propose(int& p, Generator& rng) {
            const std::array<double,3> q{0.6,0.3,0.1};
            const int old = p;
            p = std::discrete_distribution<int>(q.begin(), q.end())(rng);
            return std::log(q[old]) - std::log(q[p]);
        }
    };
    struct HistoryModel : CacheModel {
        double log_likelihood_history(int p, std::span<const Observation> observations) const {
            double value = 0;
            for (const auto& o : observations) value += o[p];
            return value;
        }
    };

    template<class Model>
    static void tempered_cache() {
        for (int seed = 0; seed < 16; ++seed) {
            SharedRng shared(seed+940);
            ParticleFilter<Model,SharedRng> filter(Model{}, {.count=256,.tempering_ess_ratio=0.8}, shared);
            std::vector<CacheModel::Observation> observations{{6, 0, -4}, {-4, 2, 6}, {2, 5, -1}};
            for (const auto& o : observations) require(filter.observe(o).ok, "log-density tempering");
            Rng manual_rng = *shared.state;
            Model model;
            ParticleCloud<int> manual;
            manual.assign(std::vector<int>(filter.particles().begin(), filter.particles().end()), filter.log_weights());
            const auto weights = std::vector<double>(filter.weights().begin(), filter.weights().end());
            const auto result = filter.refine(6);
            const auto reference = manual.rejuvenate(6, [&](int p) {
                double target = model.log_prior(p);
                for (const auto& o : observations) target += o[p];
                return target;
            }, [&](int& p, Rng& rng) { return model.propose(p, rng); }, manual_rng);
            require(result.accepted == reference.accepted, "cached tempered target equals full posterior");
            require(*shared.state == manual_rng, "tempered-cache reference RNG");
            for (int i = 0; i < filter.size(); ++i) {
                require(filter.particles()[i] == manual.particles()[i], "no repeated or missing observation exponent");
                near(filter.weights()[i], weights[i], 0);
            }
        }
    }

    static void failure_copy_and_controls() {
        ParticleFilter<Asymmetric> filter(Asymmetric{}, {.count=512});
        require(filter.observe({12,0.1,0.001}).ok, "sharp observation");
        auto copy = filter;
        auto before = filter;
        const auto failed = filter.observe({0,0,0});
        require(!failed.ok && !failed.resampled && failed.moves.attempted == 0, "failed tempering status");
        same(filter, before);
        require(filter.history().size() == 1, "failed tempering keeps history");
        // 失敗時のRNGは巻き戻さないため、成功した状態のコピー同士で再現性を検証する
        filter = copy;
        for (int turn = 0; turn < 10; ++turn) {
            const std::array<double,3> o{0.2 + turn, 1.0, 0.4};
            filter.observe(o);
            copy.observe(o);
            same(filter, copy);
        }
        auto param = filter.param();
        param.tempering_ess_ratio = 0;
        filter.set_param(param);
        same(filter, copy);
        param.tempering_ess_ratio = 1;
        param.count = 1;
        filter.set_param(param);
        require(filter.observe({0.2,0.8,0.5}).ok && filter.size() == 1, "one particle and ratio one terminate");
        filter.reset();
        require(filter.observation_count() == 0 && filter.history().empty(), "reset after tempered updates");
    }

    static int run() {
        checks = 0;
        reflected_box();
        tempered_posterior<Categorical>();
        tempered_posterior<Symmetric>();
        tempered_posterior<Asymmetric>();
        tempered_cache<CacheModel>();
        tempered_cache<HistoryModel>();
        failure_copy_and_controls();
        std::cout << "PASS: 7 optimization suites, " << checks << " checks\n";
        return 0;
    }
};


struct ImprovementTests : ManagedTests {
    struct Counted {
        int value;
        inline static int copies=0;
        explicit Counted(int x):value(x){}
        Counted(const Counted& p):value(p.value){++copies;}
        Counted& operator=(const Counted& p){value=p.value;++copies;return *this;}
        Counted(Counted&&)=default;
        Counted& operator=(Counted&&)=default;
    };
    struct CopyModel {
        using Particle=Counted;using Observation=int;
        Counted sample(Rng& r){return Counted(int(r()%3));}
        double likelihood(const Counted& p,int o)const {
            if(o<0)return 0;
            return o==0?1:(p.value==o?20:0.01);
        }
    };
    static void lazy_transaction() {
        ParticleFilter<CopyModel> f(CopyModel{}, {.count=96,.move_interval=0},Rng(783));
        Counted::copies=0;
        require(f.observe(0).ok,"flat update succeeds");
        require(Counted::copies==0,"no particle copy for a full tempering step");
        auto before=f;
        Counted::copies=0;
        require(!f.observe(-1).ok,"all-zero density fails");
        require(Counted::copies==0,"failed likelihood is detected before a copy");
        require(f.observation_count()==1,"failed step does not append history");
        for(int i=0;i<f.size();++i) {
            require(f.particles()[i].value==before.particles()[i].value,"failure preserves particles");
            near(f.weights()[i],before.weights()[i],0);
        }
        const auto sharp=f.observe(2);
        require(sharp.ok && sharp.resampled && sharp.moves.attempted>0,"partial tempering still resamples and moves");
        Counted::copies=0;
        auto clone=f;
        // ParticleCloud copies omit scratch particles; PF owns two active clouds.
        require(Counted::copies<=2*f.size(),"filter copy excludes resampling workspace");
        for(int t=0;t<12;++t) {
            f.observe(t%3);clone.observe(t%3);
            for(int i=0;i<f.size();++i) {
                require(f.particles()[i].value==clone.particles()[i].value,"clone preserves complete RNG state");
                near(f.weights()[i],clone.weights()[i],0);
            }
        }
    }
    static void cloud_copy() {
        ParticleCloud<Counted> source;
        Rng rng(775);source.initialize(17,[](Rng& r){return Counted(int(r()%100));},rng);
        source.resample(rng);
        source.propose_update_log([](const Counted&,Counted& p,Rng&){++p.value;return 0.0;},rng);
        source.estimate([](const Counted& p){return p.value;},5);
        Counted::copies=0;auto clone=source;
        require(Counted::copies==17,"cloud copy only copies visible particles");
        ParticleCloud<Counted> destination;
        destination.initialize(41,[](Rng&){return Counted(0);},rng);destination.resample(rng);
        Counted::copies=0;destination=source;
        require(Counted::copies==17,"assignment retains but does not copy workspace");
        const auto& self=source;source=self;
        source.resample(rng,9);
        destination=source;
        Rng a(843),b=a;
        for(int t=0;t<10;++t) {
            source.propose_update_log([](const Counted&,Counted& p,Rng& r){p.value+=int(r()%3);return -0.01*p.value;},a);
            destination.propose_update_log([](const Counted&,Counted& p,Rng& r){p.value+=int(r()%3);return -0.01*p.value;},b);
            source.resample(a);destination.resample(b);
            for(int i=0;i<source.size();++i) {
                require(source.particles()[i].value==destination.particles()[i].value,"workspace size reuse after assignment");
                near(source.weights()[i],destination.weights()[i],0);
            }
        }
        auto moved=std::move(clone);
        require(moved.size()==17,"move remains available after custom copy");
    }
    struct GuidedBinary {
        using Particle=int;using Observation=std::array<double,2>;
        int sample(Rng& r){return int(r()%2);}
        double likelihood(int p,const Observation& o)const{return o[p];}
        // A deliberately non-optimal q proves the library applies the correction.
        double propose_transition(int& p,const int& previous,const Observation&,Rng& r) {
            p=std::bernoulli_distribution(0.8)(r);
            const double transition=p==previous?0.85:0.15;
            return std::log(transition)-std::log(p?0.8:0.2);
        }
    };
    struct BothTransitions:GuidedBinary {
        void transition(int&,const Observation&,Rng&) {require(false,"guided method takes precedence");}
    };
    template<class Model> static void guided_exact() {
        for(int seed=0;seed<4;++seed) {
            ParticleFilter<Model> f(Model{}, {.count=65536},Rng(seed+5926));
            std::array<double,2> reference{0.5,0.5};
            for(const auto& o:std::vector<std::array<double,2>>{{0.2,0.8},{3,0.2},{0.7,0.3},{1,1},{0.01,1}}) {
                const double prior=0.85*reference[0]+0.15*reference[1];
                const double z=prior*o[0]+(1-prior)*o[1];
                reference={prior*o[0]/z,(1-prior)*o[1]/z};
                const auto status=f.observe(o);
                require(status.ok && status.moves.attempted==0,"guided dynamic filtering, no static MH");
                near(f.estimate().mean,reference[1],0.015);
                // The deliberately skewed q has only about 6000 effective samples
                // in the hardest step. Allow >4 standard errors for this MC check;
                // guided_manual separately checks the realized normalizer exactly.
                near(status.log_predictive,std::log(z),0.05);
            }
            auto snapshot=f;
            const auto result=f.observe({0,0});
            require(!result.ok && !result.resampled,"zero observation density rejects guided step");
            same(f,snapshot);
            require(f.observation_count()==5,"guided failure does not advance turn");
            f=snapshot;auto copy=f;
            f.observe({0.4,0.6});copy.observe({0.4,0.6});same(f,copy);
            f.reset();require(f.observation_count()==0,"guided reset");
        }
    }
    struct GuidedInvalid:GuidedBinary {
        double propose_transition(int& p,const int&,const Observation&,Rng&) {p=-999;return -inf;}
        double likelihood(int,const Observation&)const {require(false,"-inf proposal skips undefined likelihood");return 0;}
    };
    static void invalid_proposal() {
        ParticleFilter<GuidedInvalid> f(GuidedInvalid{}, {.count=20},Rng(5));
        auto old=f;
        require(!f.observe({1,1}).ok,"all invalid proposals fail atomically");same(f,old);
    }
    static void guided_manual() {
        // Compare the exact realized importance weights, not just Monte Carlo means.
        Rng rng(6843);
        ParticleFilter<GuidedBinary> filter(GuidedBinary{}, {.count=37,.ess_ratio=0},rng);
        GuidedBinary model;ParticleCloud<int> manual;
        manual.initialize(37,[&](Rng& r){return model.sample(r);},rng);
        for(const auto& o:std::vector<std::array<double,2>>{{0.2,0.8},{3,0.2},{0.7,0.3},{1,1},{0.01,1}}) {
            const auto result=filter.observe(o);
            const auto expected=manual.propose_update_log([&](const int& previous,int& candidate,Rng& r) {
                const double correction=model.propose_transition(candidate,previous,o,r);
                return correction+std::log(model.likelihood(candidate,o));
            },rng);
            near(result.log_predictive,expected.log_normalizer,0);
            for(int i=0;i<37;++i) {
                require(filter.particles()[i]==manual.particles()[i],"guided old/new callback arguments");
                near(filter.weights()[i],manual.weights()[i],0);
            }
        }
    }
    struct TiltedBox:UniformBox<2> {
        using Observation=int;
        TiltedBox():UniformBox({0,0},{1,1}){}
        double log_likelihood(const Particle& p,int)const{return p[0]+2*p[1];}
    };
    static void box_stationary() {
        TiltedBox model;Rng rng(967);
        ParticleCloud<TiltedBox::Particle> cloud;
        cloud.initialize(16384,[&](Rng& r){return model.sample(r);},rng);
        cloud.rejuvenate(160,[&](const auto& p){return model.log_prior(p)+model.log_likelihood(p,0);},
            [&](auto& p,Rng& r){model.propose(p,r);return 0.0;},rng);
        const auto mean=cloud.estimate([](const auto& p){return p;}).mean;
        near(mean[0],1/(1-std::exp(-1.0))-1,0.007);
        near(mean[1],1/(1-std::exp(-2.0))-0.5,0.007);
    }
    static int run() {
        checks=0;
        lazy_transaction();cloud_copy();guided_exact<GuidedBinary>();guided_exact<BothTransitions>();
        invalid_proposal();guided_manual();box_stationary();
        std::cout<<"PASS: 7 improvement suites, "<<checks<<" checks\n";
        return 0;
    }
};


struct UpgradeTests {
    using Rng = std::mt19937_64;
    inline static int checks = 0;
    static constexpr double inf = std::numeric_limits<double>::infinity();
    static void check(bool ok, const char* message) {
        ++checks;
        if (!ok) { std::cerr << "UPGRADE FAIL: " << message << '\n'; std::abort(); }
    }
    static void near(double a, double b, double tolerance = 1e-12) {
        check(std::abs(a - b) <= tolerance * (1 + std::abs(b)), "numeric comparison");
    }

    static void selection_and_sampling() {
        ParticleCloud<int> cloud;
        Rng empty_rng(8), empty_before = empty_rng;
        cloud.sample_indices({}, empty_rng);
        check(empty_rng == empty_before, "empty output does not draw RNG");
        auto initialize = [&](int n, int shift) {
            std::vector<int> values(n); std::iota(values.begin(), values.end(), shift);
            std::vector<double> logs(n);
            for (int i = 0; i < n; ++i) logs[i] = i % 5 == 0 ? -inf : -0.3 * (i % 4);
            check(cloud.assign(std::move(values), logs), "weighted initialization");
        };
        auto verify = [&] {
            const int n = cloud.size();
            std::vector<int> sorted(n); std::iota(sorted.begin(), sorted.end(), 0);
            std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
                return cloud.log_weights()[a] != cloud.log_weights()[b] ?
                    cloud.log_weights()[a] > cloud.log_weights()[b] : a < b;
            });
            for (int k : {1, 1, 3, 3, n - 1, 0, n, 3}) {
                const int used = k == 0 ? n : k;
                double mass = 0, first = 0, second = 0;
                for (int j = 0; j < used; ++j) {
                    const int i = sorted[j]; const double p = cloud.particles()[i], w = cloud.weights()[i];
                    mass += w; first += w * p; second += w * p * p;
                }
                for (int offset : {0, 2, -3}) {
                    auto estimate = cloud.estimate([&](int p) { return double(p + offset); }, k);
                    near(estimate.mean, first / mass + offset);
                    near(estimate.selected_mass, mass);
                }
                std::array<double, 2> mean;
                int calls = 0;
                const double selected = cloud.estimate_into(mean, [&](int p) {
                    ++calls; return std::array{double(p), double(p) * p};
                }, k);
                check(calls == used, "one projection per selected particle");
                near(mean[0], first / mass); near(mean[1], second / mass); near(selected, mass);
            }
            Rng random(682), reference = random;
            std::vector<int> drawn(4096);
            const auto* states = cloud.particles().data(); const auto* weights = cloud.weights().data();
            cloud.sample_indices(drawn, random);
            std::vector<double> cdf(n);
            std::partial_sum(cloud.weights().begin(), cloud.weights().end(), cdf.begin());
            for (int index : drawn) {
                const double target = std::min(std::generate_canonical<double, 53>(reference) * cdf.back(),
                                               std::nextafter(cdf.back(), 0.0));
                int expected = 0;
                while (cdf[expected] <= target) ++expected;
                check(index == expected && cloud.weights()[index] > 0, "draw agrees with weighted interval");
            }
            check(random == reference, "draw consumes only external RNG");
            check(states == cloud.particles().data() && weights == cloud.weights().data(), "sampling keeps views valid");
        };
        initialize(17, 0); verify();
        Rng rng(632);
        cloud.predict([](int& p, Rng&) { p += 9; }, rng); verify();
        cloud.update_log([](int p) { return -0.07 * p; }); verify();
        check(!cloud.update_log([](int) { return -inf; }).ok, "failed update"); verify();
        check(!cloud.assign({99}, std::array{-inf}), "failed assign"); verify();
        auto saved = cloud;
        initialize(9, -5); verify(); cloud = saved; verify();
        cloud = cloud; verify();
        cloud.resample(rng, 23); verify();
        cloud.resample_from(saved, rng, 31); verify();
        cloud.propose_update_log([](const int& old, int& p, Rng&) { p = old + 4; return -.1 * old; }, rng); verify();
        check(!cloud.propose_update_log([](const int&, int& p, Rng&) { p = -99; return -inf; }, rng).ok,
              "failed transition preserves caches"); verify();
        cloud.rejuvenate(1, [](int& p, Rng&) { ++p; }, rng); verify();
        auto moved = std::move(cloud); cloud = std::move(moved); verify();
        cloud.initialize(7, [](Rng& r) { return int(r() % 50); }, rng); verify();
        cloud.assign({1, 2, 3, 4}); verify();
    }

    template<bool Maximum> struct EdgeRng {
        using result_type = std::uint64_t;
        static constexpr result_type min() { return 0; }
        static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
        result_type operator()() { return Maximum ? max() : min(); }
    };
    static void sampling_distribution() {
        ParticleCloud<int> cloud;
        cloud.assign({0, 1, 2, 3, 4, 5}, std::array{-inf, std::log(.01), -inf, std::log(.15), std::log(.84), -inf});
        EdgeRng<false> low; EdgeRng<true> high;
        std::array<int, 31> edge;
        cloud.sample_indices(edge, low);
        for (int i : edge) check(i == 1, "lower endpoint skips leading zero weights");
        cloud.sample_indices(edge, high);
        for (int i : edge) check(i == 4, "upper endpoint excludes trailing zeros");
        Rng rng(3141);
        std::vector<int> draws(200000); cloud.sample_indices(draws, rng);
        std::array<int, 6> counts{};
        int pair = 0;
        for (int i = 0; i < int(draws.size()); ++i) {
            ++counts[draws[i]];
            if (i % 2 == 1 && draws[i] == 4 && draws[i - 1] == 4) ++pair;
        }
        for (int i = 0; i < 6; ++i) near(double(counts[i]) / double(draws.size()), cloud.weights()[i], .003);
        near(double(pair) / 100000, .84 * .84, .004);
        cloud.assign({7}, std::array{0.0}); cloud.sample_indices(draws, rng);
        for (int i : draws) check(i == 0, "single particle sampling");
    }

    struct Heavy {
        std::vector<double> values;
        double cache = 0;
        inline static int copies = 0;
        explicit Heavy(int d, double x) : values(d, x) {}
        Heavy(const Heavy& p) : values(p.values), cache(p.cache) { ++copies; }
        Heavy& operator=(const Heavy& p) { values = p.values; cache = p.cache; ++copies; return *this; }
        Heavy(Heavy&&) = default;
        Heavy& operator=(Heavy&&) = default;
    };
    struct HeavyModel {
        using Particle = Heavy;
        using Observation = double;
        std::normal_distribution<double> normal;
        int dimension = 31;
        Heavy sample(Rng& r) { return Heavy(dimension, normal(r)); }
        double log_prior(const Heavy& p) const { return -.5 * p.values[0] * p.values[0]; }
        double log_likelihood(const Heavy& p, double y) const {
            return y == 999 ? -inf : -2 * (p.values[0] - y) * (p.values[0] - y);
        }
        void propose(Heavy& p, Rng& r) { p.values[0] += .3 * normal(r); }
    };
    struct DynamicModel : HeavyModel {
        void transition(Heavy& p, double, Rng& r) { p.values[0] += .2 * normal(r); }
    };
    template<class A, class B> static void same(const A& a, const B& b) {
        check(a.size() == b.size() && a.observation_count() == b.observation_count(), "same size and turns");
        near(a.ess(), b.ess(), 0);
        for (int i = 0; i < a.size(); ++i) {
            check(a.particles()[i].values == b.particles()[i].values, "same particles");
            near(a.particles()[i].cache, b.particles()[i].cache, 0);
            near(a.weights()[i], b.weights()[i], 0);
            check(a.log_weights()[i] == b.log_weights()[i], "same log weights");
        }
    }
    template<class Model> static void copy_branch() {
        using Filter = ParticleFilter<Model>;
        Filter filter(Model{}, {.count = 19, .ess_ratio = 1, .move_interval = 1, .tempering_ess_ratio = 0}, Rng(33));
        check(filter.observe(.5).ok && filter.observe(-.8).ok, "prepare branching state");
        auto project = [](const Heavy& p) { return p.values[0]; };
        filter.estimate(project, 3);
        std::vector<int> ids(100); Rng planning(34); filter.sample_indices(ids, planning);
        Heavy::copies = 0;
        auto branch = filter;
        check(Heavy::copies == filter.size(), "copy excludes prepared particle population");
        same(filter, branch);
        filter.sample_indices(ids, planning); // Must not alter the filtering RNG.
        auto a = filter.observe(.1); auto b = branch.observe(.1);
        check(a.moves.accepted == b.moves.accepted && a.resampled == b.resampled, "branch RNG and model state");
        near(a.log_predictive, b.log_predictive, 0); same(filter, branch);
        auto p = branch.param(); p.count = 7; branch.set_param(p); branch.observe(-.2);
        branch.estimate(project, 3); branch.sample_indices(ids, planning);
        Heavy::copies = 0; branch = filter;
        check(Heavy::copies == filter.size(), "assignment excludes workspace population");
        same(filter, branch);
        branch = branch; same(filter, branch);
        filter.observe(-.1); branch.observe(-.1); same(filter, branch);
        auto saved = filter;
        check(!filter.observe(999).ok, "branch failed observation"); same(filter, saved);
        filter = saved; filter.observe(.2); saved.observe(.2); same(filter, saved);
        auto moved = std::move(filter); filter = std::move(moved); same(filter, saved);
        filter.reset(); saved.reset(); same(filter, saved);
        check(branch.observation_count() != filter.observation_count(), "reset independent from branch");
    }

    struct BinaryModel {
        using Particle = Heavy;
        using Observation = std::array<double, 2>;
        Heavy sample(Rng& r) { return Heavy(23, std::bernoulli_distribution(.7)(r)); }
        double prior(const Heavy& p) const { return p.values[0] == 1 ? .7 : .3; }
        double likelihood(const Heavy& p, const Observation& o) const { return o[int(p.values[0])]; }
        double propose(Heavy& p, Rng& r) {
            const double old = p.values[0];
            p.values[0] = std::bernoulli_distribution(.85)(r);
            p.cache = p.values[0] + 100;
            return std::log(old == 1 ? .85 : .15) - std::log(p.values[0] == 1 ? .85 : .15);
        }
    };
    struct DeltaModel : BinaryModel {
        struct Undo {
            double value, cache;
            Undo(double v, double c) : value(v), cache(c) {}
            Undo(const Undo&) = delete;
            Undo(Undo&&) = default;
        };
        auto propose_in_place(Heavy& p, Rng& r) {
            Undo old(p.values[0], p.cache);
            double correction = propose(p, r);
            return ParticleCloud<Heavy>::MoveProposal<Undo>{std::move(old), correction};
        }
        void undo(Heavy& p, const Undo& old) { p.values[0] = old.value; p.cache = old.cache; }
    };
    template<class Base> struct CachedModel : Base {
        double log_likelihood_history(const Heavy& p, std::span<const BinaryModel::Observation> history) const {
            double sum = 0;
            for (const auto& o : history) {
                const double v = this->likelihood(p, o);
                if (v == 0) return -inf;
                sum += std::log(v);
            }
            return sum;
        }
    };
    template<class Copy, class Delta> static void delta_managed() {
        using A = ParticleFilter<Copy>; using B = ParticleFilter<Delta>;
        for (double temper : {0.0, .5}) for (int seed : {0, 1, 17, 100, 173}) {
            A a(Copy{}, {.count = 37, .ess_ratio = .8, .move_steps = 2, .move_interval = 2,
                         .tempering_ess_ratio = temper}, Rng(seed));
            B b(Delta{}, {.count = 37, .ess_ratio = .8, .move_steps = 2, .move_interval = 2,
                          .tempering_ess_ratio = temper}, Rng(seed));
            for (auto o : std::vector<BinaryModel::Observation>{{.3, .8}, {1, .02}, {0, 1}, {1.7, .4}, {1, 1}}) {
                const auto x = a.observe(o), y = b.observe(o);
                // Hard evidence can contradict every current particle. Both paths must fail atomically.
                check(x.ok == y.ok && x.resampled == y.resampled, "delta observe status");
                check(x.moves.attempted == y.moves.attempted && x.moves.accepted == y.moves.accepted, "delta acceptance decisions");
                check(x.log_predictive == y.log_predictive, "delta evidence including impossible observation"); same(a, b);
                check(std::ranges::equal(a.history(), b.history()), "delta observation history");
                for (int steps : {0, 3}) {
                    const auto u = a.refine(steps);
                    Heavy::copies = 0;
                    const auto v = b.refine(steps);
                    check(Heavy::copies == 0, "in-place refinement makes no particle copies");
                    check(u.attempted == v.attempted && u.accepted == v.accepted, "delta refinement decisions");
                    same(a, b);
                }
            }
            auto before = b;
            check(!b.observe({0, 0}).ok, "delta impossible observation"); same(b, before);
            b = before;
            b.refine(7); before.refine(7); same(b, before);
        }
    }
    static void delta_cloud() {
        ParticleCloud<int> cloud;
        cloud.assign(std::vector<int>(16384, 0));
        Rng rng(983);
        const auto target = [](int p) { return std::log(p ? .3 : .7); };
        auto propose = [](int& p, Rng& r) {
            const int old = p;
            p = std::bernoulli_distribution(.8)(r);
            return ParticleCloud<int>::MoveProposal<int>{old,
                std::log(old ? .8 : .2) - std::log(p ? .8 : .2)};
        };
        auto undo = [](int& p, const int& old) { p = old; };
        cloud.rejuvenate(60, target, propose, undo, rng);
        near(cloud.estimate([](int p) { return p; }).mean, .3, .01);
        ParticleCloud<int> finite;
        finite.assign({0, 1, 2}, std::array{0.0, -1000.0, -inf});
        auto changed = finite.rejuvenate(3, [](int) { return 0.0; },
            [](int& p, Rng&) { const int old = p; p = 99; return ParticleCloud<int>::MoveProposal<int>{old, -inf}; }, undo, rng);
        check(changed.attempted == 6 && changed.accepted == 0, "finite underflow still explored; -inf skipped");
        check(std::ranges::equal(finite.particles(), std::array{0, 1, 2}), "forced rejection restores states");
    }

    struct VectorModel {
        using Particle = std::vector<int>; using Observation = int;
        Particle sample(Rng& r) { const int x = int(r() % 5); return {x, x * x}; }
        double likelihood(const Particle&, int) const { return 1; }
    };
    struct ProjectedModel : HeavyModel {
        std::array<double, 2> project(const Heavy& p) const { return {p.values[0], p.cache}; }
    };
    static void buffers() {
        ParticleFilter<VectorModel> filter(VectorModel{}, {.count = 11}, Rng(7));
        for (int k : {0, 1, 3, 11}) {
            std::vector<double> vec(2); std::array<double, 2> array; double raw[2];
            auto expected = filter.estimate(k);
            const double mass = filter.estimate_into(vec, k);
            near(mass, expected.selected_mass);
            for (int i = 0; i < 2; ++i) near(vec[i], expected.mean[i]);
            filter.estimate_into(array, [](const auto& p) { return std::array{p[0], p[1]}; }, k);
            filter.estimate_into(std::span<double, 2>(raw), [](const auto& p) -> const auto& { return p; }, k);
            for (int i = 0; i < 2; ++i) { near(array[i], vec[i]); near(raw[i], vec[i]); }
        }
        ParticleCloud<std::vector<int>> empty;
        empty.assign({{}, {}});
        std::vector<double> output;
        near(empty.estimate_into(output, [](const auto& p) -> const auto& { return p; }), 1);
        check(output.empty(), "empty numeric output is supported");
        ParticleFilter<ProjectedModel> projected(ProjectedModel{}, {.count = 5});
        std::array<double, 2> two;
        projected.estimate_into(two, 3);
        check(two == projected.estimate(3).mean, "default Model.project with output buffer");
        std::vector<double> coordinates(31);
        projected.estimate_into(coordinates, &Heavy::values);
        check(coordinates == projected.estimate(&Heavy::values).mean, "member pointer projection with output buffer");
    }
    struct MoveOnlyModel {
        using Particle = int; using Observation = int;
        std::unique_ptr<int> value = std::make_unique<int>(1);
        int sample(Rng&) { return *value; }
        double likelihood(int, int) const { return 1; }
    };
    static void value_semantics() {
        using Filter = ParticleFilter<MoveOnlyModel>;
        static_assert(!std::is_copy_constructible_v<Filter> && !std::is_copy_assignable_v<Filter>);
        static_assert(std::is_move_constructible_v<Filter> && std::is_move_assignable_v<Filter>);
        Filter a(MoveOnlyModel{}, {.count = 3}); auto b = std::move(a); a = std::move(b);
        near(a.estimate().mean, 1);
    }
    static void run() {
        selection_and_sampling(); sampling_distribution(); copy_branch<HeavyModel>(); copy_branch<DynamicModel>();
        delta_managed<BinaryModel, DeltaModel>(); delta_managed<CachedModel<BinaryModel>, CachedModel<DeltaModel>>();
        delta_cloud(); buffers(); value_semantics();
        std::cout << "PASS: 9 v09 upgrade suites, " << checks << " checks\n";
    }
};

struct ReviewTests : ManagedTests {
    struct Calls {
        int transition = 0, likelihood = 0, prior = 0, undo = 0, history = 0;
    };
    struct PartialGuided {
        using Particle = std::vector<int>;
        using Observation = int;
        std::shared_ptr<Calls> calls;
        int index = 0;
        Particle sample(Rng&) { return {index++ % 3}; }
        double propose_transition(Particle& next, const Particle& old, int, Rng&) {
            ++calls->transition;
            require(old.size() == 1, "invalid old proposal must not reach next transition");
            if (old[0] == 0) {
                next.clear();
                return -inf;
            }
            next[0] += 3;
            return 0;
        }
        double likelihood(const Particle& p, int) const {
            ++calls->likelihood;
            require(p.size() == 1, "invalid proposal must not reach likelihood");
            return 1;
        }
    };
    static void partial_guided() {
        for (double ratio : {0.0, 0.5, 1.0}) {
            auto calls = std::make_shared<Calls>();
            ParticleFilter<PartialGuided> filter(PartialGuided{calls}, {.count=6, .ess_ratio=ratio});
            auto first = filter.observe(0);
            require(first.ok && !first.resampled, "partial invalid update succeeds");
            near(first.log_predictive, std::log(2.0 / 3));
            require(calls->transition == 6 && calls->likelihood == 4, "only valid proposals use likelihood");
            for (int i = 0; i < 6; ++i) {
                require(filter.particles()[i].size() == 1, "invalid proposal leaves original state readable");
                if (i % 3 == 0) {
                    require(filter.particles()[i][0] == 0 && filter.log_weights()[i] == -inf,
                            "rejected proposal retains original value with zero weight");
                }
            }
            near(filter.estimate().mean[0], 4.5);
            auto second = filter.observe(1);
            require(second.ok && second.resampled == (ratio == 1), "next turn works with or without resampling");
            const int live = ratio == 1 ? 6 : 4;
            require(calls->transition == 6 + live && calls->likelihood == 4 + live,
                    "true zero particles are not transitioned again");
            near(second.log_predictive, 0);
            if (ratio == 1) {
                const double mean = filter.estimate().mean[0];
                require(mean >= 7 && mean <= 8, "resampled prediction stays in valid support");
            } else near(filter.estimate().mean[0], 7.5);
        }
    }
    static void dead_and_underflow() {
        ParticleCloud<int> cloud;
        cloud.assign({0, 1, 2, 3}, std::array{-inf, -1000.0, 0.0, 0.0});
        Rng rng(32);
        std::array<int, 32> sampled;
        cloud.sample_indices(sampled, rng); // Populate CDF and top-k caches before the update.
        cloud.estimate([](int p) { return p; }, 1);
        int calls = 0;
        auto result = cloud.propose_update_log([&](const int& old, int& next, Rng&) {
            ++calls;
            require(old != 0, "true zero skips proposal callback");
            next = old == 2 ? -99 : old + 10;
            return old == 2 ? -inf : (old == 1 ? 1000.0 : 0.0);
        }, rng);
        require(result.ok && calls == 3, "finite log weight is explored despite normal underflow");
        require(std::ranges::equal(cloud.particles(), std::array{0, 11, 2, 13}),
                "partial rejection commits only valid proposal states");
        near(cloud.weights()[1], 0.5); near(cloud.weights()[3], 0.5);
        near(cloud.estimate([](int p) { return p; }).mean, 12);
        cloud.sample_indices(sampled, rng);
        for (int i : sampled) require(i == 1 || i == 3, "sampling cache excludes rejected proposals");
        const auto before = cloud;
        calls = 0;
        result = cloud.propose_update_log([&](const int& old, int& next, Rng&) {
            ++calls;
            require(old == 11 || old == 13, "only surviving particles reach later proposals");
            next = -100;
            return -inf;
        }, rng);
        require(!result.ok && calls == 2, "all surviving proposals invalid means atomic failure");
        require(std::ranges::equal(cloud.particles(), before.particles()) &&
                std::ranges::equal(cloud.log_weights(), before.log_weights()), "failure preserves old population");
    }
    static void impossible_cloud_moves() {
        for (bool in_place : {false, true}) {
            ParticleCloud<int> cloud;
            cloud.assign({1, 1, 1, 1});
            Rng rng(13), reference = rng;
            int targets = 0, undone = 0;
            const auto target = [&](int p) {
                ++targets;
                require(p == 1, "zero reverse probability skips candidate target");
                return 0.0;
            };
            ParticleCloud<int>::MoveResult moved;
            if (in_place) {
                moved = cloud.rejuvenate(3, target, [](int& p, Rng&) {
                    const int old = p;
                    p = -7;
                    return ParticleCloud<int>::MoveProposal<int>{old, -inf};
                }, [&](int& p, const int& old) { ++undone; p = old; }, rng);
            } else {
                moved = cloud.rejuvenate(3, target, [](int& p, Rng&) { p = -7; return -inf; }, rng);
            }
            for (int j = 0; j < 12; ++j) uniform(reference);
            require(rng == reference, "certain rejection keeps acceptance RNG consumption");
            require(targets == 4 && moved.attempted == 12 && moved.accepted == 0, "certain rejection statistics");
            require(undone == (in_place ? 12 : 0), "each in-place rejection invokes undo");
            for (int p : cloud.particles()) require(p == 1, "certain rejection preserves particle");
        }
    }
    struct RejectCopy {
        using Particle = int;
        using Observation = int;
        std::shared_ptr<Calls> calls;
        int sample(Rng&) { return 1; }
        double log_prior(int p) const {
            ++calls->prior;
            require(p == 1, "certain rejection skips user prior");
            return 0;
        }
        double likelihood(int p, int) const {
            ++calls->likelihood;
            require(p == 1, "certain rejection skips user likelihood");
            return 1;
        }
        double propose(int& p, Rng&) { p = -7; return -inf; }
    };
    struct RejectDelta : RejectCopy {
        auto propose_in_place(int& p, Rng&) {
            const int old = p; p = -7;
            return ParticleCloud<int>::MoveProposal<int>{old, -inf};
        }
        void undo(int& p, const int& old) { ++calls->undo; p = old; }
    };
    template<class Base> struct RejectHistory : Base {
        double log_likelihood_history(int p, std::span<const int> history) const {
            ++this->calls->history;
            require(p == 1, "certain rejection skips aggregate history");
            return double(history.size()) * 0;
        }
    };
    template<class Model> static void impossible_managed_moves() {
        auto calls = std::make_shared<Calls>();
        Model model; model.calls = calls;
        ParticleFilter<Model> filter(model, {.count=7, .move_steps=0});
        filter.observe(0);
        *calls = {};
        auto result = filter.refine(5);
        require(result.attempted == 35 && result.accepted == 0, "managed certain rejection statistics");
        require(calls->prior == 0 && calls->likelihood == 0 && calls->history == 0,
                "managed certain rejection avoids model evaluation");
        if constexpr (requires(Model& m, int& p, Rng& r) { m.propose_in_place(p, r); })
            require(calls->undo == 35, "managed undo restores state and cached target");
        near(filter.estimate().mean, 1);
        require(filter.observe(0).ok && filter.observation_count() == 2, "later update retains correct cache");
    }
    static void run() {
        const int start = checks;
        partial_guided(); dead_and_underflow(); impossible_cloud_moves();
        impossible_managed_moves<RejectCopy>(); impossible_managed_moves<RejectDelta>();
        impossible_managed_moves<RejectHistory<RejectCopy>>();
        impossible_managed_moves<RejectHistory<RejectDelta>>();
        std::cout << "PASS: review 7 suites, " << checks - start << " checks\n";
    }
};

struct BooleanHistoryTests : ManagedTests {
    template<class O, bool Discrete> struct Base {
        using Particle = std::conditional_t<Discrete, bool, double>;
        using Observation = O;
        Particle sample(Rng& rng) const {
            if constexpr (Discrete) return std::bernoulli_distribution(0.3)(rng);
            else return 0.05 + 0.9 * uniform(rng);
        }
        static double probability(Particle p, Observation o) {
            const double chance = Discrete ? (p ? 0.8 : 0.1) : double(p);
            return o ? chance : 1 - chance;
        }
    };
    template<class O, bool Discrete> struct Normal : Base<O, Discrete> {
        double likelihood(typename Base<O, Discrete>::Particle p, O o) const {
            return this->probability(p, o);
        }
    };
    template<class O, bool Discrete> struct Logarithmic : Base<O, Discrete> {
        double log_likelihood(typename Base<O, Discrete>::Particle p, O o) const {
            return std::log(this->probability(p, o));
        }
    };
    template<class Model> struct Aggregate : Model {
        std::shared_ptr<int> calls = std::make_shared<int>(0);
        double log_likelihood_history(typename Model::Particle p, const auto& history) const {
            ++*calls;
            double sum = 0;
            for (auto observed : history) sum += std::log(this->probability(p, observed));
            return sum;
        }
    };
    static void compare(const auto& a, const auto& b) {
        require(a.size() == b.size() && a.observation_count() == b.observation_count(), "history state sizes");
        require(std::ranges::equal(a.history(), b.history()), "history view values");
        for (int i = 0; i < a.size(); ++i) {
            near(double(a.particles()[i]), double(b.particles()[i]), 0);
            require(a.log_weights()[i] == b.log_weights()[i], "equal finite or zero log weights");
        }
    }
    template<template<class, bool> class Model, bool Discrete> static void scenario() {
        using Slow = ParticleFilter<Model<bool, Discrete>>;
        using Fast = ParticleFilter<Aggregate<Model<bool, Discrete>>>;
        using Int = ParticleFilter<Aggregate<Model<int, Discrete>>>;
        static_assert(std::is_same_v<decltype(std::declval<const Int&>().history()), std::span<const int>>);
        using View = decltype(std::declval<const Fast&>().history());
        static_assert(std::ranges::random_access_range<View> && std::ranges::sized_range<View>);
        static_assert(!std::indirectly_writable<std::ranges::iterator_t<View>, bool>);
        for (int seed : {0, 7, 19}) {
            Aggregate<Model<bool, Discrete>> model;
            const auto calls = model.calls;
            Fast fast(model, {.count=17}, Rng(seed));
            Slow slow({}, {.count=17}, Rng(seed));
            Int ints({}, {.count=17}, Rng(seed));
            require(fast.history().empty(), "empty boolean history");
            fast.refine(2); slow.refine(2); ints.refine(2);
            require(*calls > 0, "boolean aggregate path is selected for empty history");
            for (int turn = 0; turn < 137; ++turn) {
                const bool observed = turn % 7 < 4;
                const auto a = fast.observe(observed);
                const auto b = slow.observe(observed);
                const auto c = ints.observe(int(observed));
                require(a.ok && b.ok && c.ok, "boolean observations succeed");
                near(a.log_predictive, b.log_predictive, 0); near(a.log_predictive, c.log_predictive, 0);
                compare(fast, slow); compare(fast, ints);
                const auto h = fast.history();
                require(h.size() == std::size_t(turn + 1) && h.back() == observed, "history crosses bit storage boundaries");
                require(h.front(), "history front read");
            }
            *calls = 0;
            const auto a = fast.refine(3);
            const auto b = slow.refine(3);
            const auto c = ints.refine(3);
            require(*calls > 0, "boolean aggregate fast path is used");
            require(a.accepted == b.accepted && a.accepted == c.accepted, "aggregate MH equivalence");
            compare(fast, slow); compare(fast, ints);
            auto branch = fast;
            branch.observe(false);
            require(branch.history().size() == 138 && fast.history().size() == 137, "boolean history copy isolation");
            branch = fast;
            compare(branch, fast);
            auto param = branch.param(); param.count = 31;
            branch.set_param(param);
            require(branch.history().size() == 137 && branch.size() == 31, "resize keeps boolean history");
            branch.reset();
            require(branch.history().empty() && branch.observation_count() == 0, "reset clears boolean history");
            require(branch.observe(true).ok && branch.history()[0], "reuse cleared boolean history");
        }
    }
    struct Deterministic {
        using Particle = bool; using Observation = bool;
        bool sample(Rng& rng) { return std::bernoulli_distribution(0.5)(rng); }
        double likelihood(bool p, bool o) const { return p == o ? 1.0 : 0.0; }
    };
    struct CopyBoolean : Normal<bool, true> {
        double log_prior(bool p) const { return std::log(p ? 0.3 : 0.7); }
        void propose(bool& p, Rng&) const { p = !p; }
    };
    struct DeltaBoolean : CopyBoolean {
        auto propose_in_place(bool& p, Rng&) const {
            const bool old = p; p = !p;
            return ParticleCloud<bool>::MoveProposal<bool>{old, 0.0};
        }
        void undo(bool& p, const bool& saved) const { p = saved; }
    };
    static void boolean_delta() {
        ParticleFilter<Aggregate<CopyBoolean>> copied({}, {.count=31});
        ParticleFilter<Aggregate<DeltaBoolean>> delta({}, {.count=31});
        for (int turn = 0; turn < 20; ++turn) {
            const bool observation = turn % 3 != 0;
            require(copied.observe(observation).ok && delta.observe(observation).ok, "bool delta observation");
            compare(copied, delta);
            const auto a = copied.refine(3);
            const auto b = delta.refine(3);
            require(a.attempted == b.attempted && a.accepted == b.accepted, "bool copy and delta acceptance");
            compare(copied, delta);
        }
    }
    static void failed_update() {
        ParticleFilter<Deterministic> f({}, {.count=64, .ess_ratio=0, .move_steps=0});
        require(f.observe(false).ok, "deterministic first observation");
        const auto before = f;
        require(!f.observe(true).ok, "contradictory observation fails");
        compare(f, before);
        require(f.history().size() == 1 && !f.history()[0], "failure preserves boolean history");
        near(f.estimate().mean, 0);
    }
    static void run() {
        const int start = checks;
        scenario<Normal, false>(); scenario<Normal, true>();
        scenario<Logarithmic, false>(); scenario<Logarithmic, true>();
        failed_update(); boolean_delta();
        std::cout << "PASS: bool history 6 suites, " << checks - start << " checks\n";
    }
};

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--death") return ParticleCloudTests::death_test(argv[2]);
    ParticleCloudTests::run();
    ManagedTests::run();
    RevisionTests::run();
    OptimizationTests::run();
    ImprovementTests::run();
    UpgradeTests::run();
    ReviewTests::run();
    BooleanHistoryTests::run();
    return 0;
}

#endif
