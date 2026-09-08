#pragma once
#include <bits/stdc++.h>

#include "config.hpp"
#include "start_time.hpp"
#include "debug_records.hpp"
#include "fast_rand.hpp"

using namespace std;

#define DEBUG_PARAM_LIST         \
    X(Iterations)                \
    X(Elapsed)                   \
    X(BestScore)                 \
    X(BestScoreImproved)         \
    X(CurrentScore)              \
    X(Accepted)                  \
    X(WorseAccepted)             \
    X(WorseAcceptedMax)          \
    X(WorseAcceptedAvg)          \
    X(NoImprovementCount)        \
    X(LnsTries)                  \
    X(LnsProcessed)              \
    X(Temperature)               \
    X(TabuRejectCount)           \
    X(PeriodIterations)          \
    X(PeriodBestScore)           \
    X(PeriodAccepted)            \
    X(PeriodWorseAccepted)       \
    X(PeriodWorseAcceptedMax)    \
    X(PeriodWorseAcceptedAvg)    \
    X(Length)

enum class SAMinimumEnum : int {
#define X(name) name,
    DEBUG_PARAM_LIST
#undef X
};
constexpr const char* SAMinimumEnumToStr[] = {
#define X(name) #name,
    DEBUG_PARAM_LIST
#undef X
};
#undef DEBUG_PARAM_LIST

enum class AcceptRefType { Best, Current };
enum class ScheduleType { Exponential, Linear };

template<class S>
struct NoLns{
    static std::optional<S> do_lns(const S &) { return nullopt; }
};

template <class S, class NC, class LNS = NoLns<S>, class Rec = SAMinimumEnum> struct SA {
    using CostT = decltype(S().get_cost());
    using HashT = decltype(S().get_hash());
    using DebugRecordsT = DebugRecords<Rec>;
    
    static constexpr CostT NOT_FOUND = numeric_limits<CostT>::max();
    
    struct Config {
        bool debug_mode = false;
        int time_limit_millis = 0;
        size_t max_iter = 0;
        size_t lns_threshold = 0;
        bool tabu_enable = false;
        size_t tabu_tenure = 0;
        double start_temp = 1000.0;
        double end_temp = 1.0;
        ScheduleType temp_schedule_type = ScheduleType::Exponential;
        AcceptRefType accept_type = AcceptRefType::Current;

        static Config load_from_config(){
            Config conf;
            conf.debug_mode = ::Config::get_as_bool("debug_mode");
            conf.time_limit_millis = (int)::Config::get_as_ll("time_limit_millis");
            conf.max_iter = (size_t)::Config::get_as_ll("max_iter");
            conf.lns_threshold = ::Config::get_as_ll("lns_threshold");
            conf.tabu_enable = ::Config::get_as_bool("tabu_enable");
            conf.tabu_tenure = (size_t)::Config::get_as_ll("tabu_tenure");
            conf.start_temp = ::Config::get_as_double("start_temp");
            conf.end_temp = ::Config::get_as_double("end_temp");
            conf.temp_schedule_type = static_cast<ScheduleType>((int)::Config::get_as_ll("temp_schedule_type_i"));
            conf.accept_type = static_cast<AcceptRefType>((int)::Config::get_as_ll("accept_type_i"));
            return conf;
        }

        friend inline std::ostream& operator<<(std::ostream& os, const Config& conf){
            const auto old = os.flags();     // boolalpha 変更前の状態を保存
            os << std::boolalpha;            // true / false を文字列で出力

            os << "SA::Conf{"
            << "debug_mode="          << conf.debug_mode
            << ", time_limit_millis=" << conf.time_limit_millis
            << ", max_iter="          << conf.max_iter
            << ", lns_threshold="     << conf.lns_threshold
            << ", tabu_enable="       << conf.tabu_enable
            << ", tabu_tenure="       << conf.tabu_tenure
            << ", accept_type="       << (int)conf.accept_type
            << ", start_temp="        << conf.start_temp
            << ", end_temp="          << conf.end_temp
            << ", temp_schedule_type="<< (int)conf.temp_schedule_type
            << '}';

            os.flags(old);                // 元のフラグに戻す
            return os;
        };
    };

    Config conf;
    DebugRecordsT REC;
    int total_iterations = 0;

    SA(const Config& conf_, const char* const* enum_to_str_ = SAMinimumEnumToStr) : conf(conf_), REC(conf.debug_mode, enum_to_str_) {
        REC.set_default({Rec::BestScore, Rec::PeriodBestScore}, 1e10);
#ifndef LOCAL
        conf.debug_mode = false;
#endif
        if(conf.debug_mode) cerr << conf << endl;
    }

    // -----------------------------------------------------------------------------
    // HashTabu: タブーリスト管理
    template<typename H>
    struct HashTabu {
        bool enabled;
        size_t tabu_tenure;
        deque<H> que;
        unordered_set<H> set;

        HashTabu(bool en, size_t tenure) : enabled(en && tenure > 0), tabu_tenure(tenure) {}

        bool is_tabu(const H &h) const {
            return enabled && (set.find(h) != set.end());
        }
        void add(const H &h) {
            if (!enabled) return;
            que.push_back(h);
            set.insert(h);
            if (que.size() > tabu_tenure) {
                auto old = que.front();
                que.pop_front();
                set.erase(old);
            }
        }
    };

    // SA実行：最良状態を返す
    S run(const S &initial_state) {
        auto local_start_time = chrono::steady_clock::now();
        S state = initial_state;
        state.recalc();
        if(conf.debug_mode) {
            cerr << "SA run(" << state.get_cost() << ") with parameters: "
                 << "time_limit_millis=" << conf.time_limit_millis << ", max_iter=" << conf.max_iter << "\n";
        }
        S best_state = state;

        NC change(REC);

        REC.reset();
        REC.set(Rec::BestScore, (double)best_state.get_cost());

        size_t iteration = 0;
        size_t no_improve_count = 0;
        HashTabu<decltype(state.get_hash())> tabu(conf.tabu_enable, conf.tabu_tenure);
        tabu.add(state.get_hash());

        const unsigned long long DEBUG_INTERVAL_MILLIS = 10;
        unsigned long long debug_period_last = elapsed_ms() / DEBUG_INTERVAL_MILLIS;

        while (true) {
            total_iterations++;
            iteration++;
            int elapsed = (int)elapsed_ms();

            if ((elapsed / DEBUG_INTERVAL_MILLIS != debug_period_last) ||
                (conf.time_limit_millis > 0 && elapsed >= conf.time_limit_millis))
            {
                debug_period_last = elapsed / DEBUG_INTERVAL_MILLIS;
                REC.record();
                REC.reset(Rec::PeriodIterations, Rec::Length, false);
            }
            if ((conf.time_limit_millis > 0 && elapsed >= conf.time_limit_millis) ||
                (conf.max_iter > 0 && iteration > conf.max_iter))
            {
                break;
            }
            REC.add(Rec::Iterations, 1.0);
            REC.add(Rec::PeriodIterations, 1.0);

            if(conf.lns_threshold > 0 && no_improve_count > conf.lns_threshold) {
                REC.add(Rec::LnsTries, 1.0);
                auto new_state_opt = LNS::do_lns(state);
                if(new_state_opt.has_value()) {
                    REC.add(Rec::LnsProcessed, 1.0);
                    if(new_state_opt->get_cost() < best_state.get_cost())
                        best_state = new_state_opt.value();
                    state = new_state_opt.value();
                    no_improve_count = 0;
                    REC.set(Rec::NoImprovementCount, 0.0);
                }
            }

            double ratio = 
                conf.max_iter > 0 ? (double)iteration / (double)conf.max_iter :
                conf.time_limit_millis > 0 ? (double)elapsed / (double)conf.time_limit_millis :
                1.0;
            double temp = 
                conf.temp_schedule_type == ScheduleType::Exponential ? 
                    conf.start_temp * pow(conf.end_temp / conf.start_temp, ratio) : 
                    conf.start_temp + (conf.end_temp - conf.start_temp) * ratio;
            REC.set(Rec::Temperature, temp);

            bool improved = false;
            change.generate(state);
            if(change.valid()) {
                CostT new_cost = change.cost();
                HashT new_hash = change.hash();
                if(tabu.is_tabu(new_hash)) {
                    REC.add(Rec::TabuRejectCount, 1.0);
                } else {
                    CostT compare_score = (conf.accept_type == AcceptRefType::Best ? best_state.get_cost() : state.get_cost());
                    CostT diff = new_cost - compare_score;
                    bool better = (diff < 0);
                    bool accept = false;
                    if(better) {
                        accept = true;
                    } else {
                        double prob = exp(-((double)diff) / temp);
                        double r = (double)rng() / (double)FastRand::max();
                        accept = (r < prob);
                    }
                    if(accept) {
                        change.apply(state);
                        if(state.get_cost() < best_state.get_cost()) {
                            best_state = state;
                            REC.add(Rec::BestScoreImproved, 1.0);
                            REC.set(Rec::BestScore, (double)best_state.get_cost());
                            no_improve_count = 0;
                            REC.set(Rec::NoImprovementCount, 0.0);
                            improved = true;
                        }
                        if (!better) {
                            REC.chmax(Rec::WorseAcceptedMax, (double)diff);
                            REC.n_and_avg(Rec::WorseAccepted, Rec::WorseAcceptedAvg, (double)diff);
                            REC.chmax(Rec::PeriodWorseAcceptedMax, (double)diff);
                            REC.n_and_avg(Rec::PeriodWorseAccepted, Rec::PeriodWorseAcceptedAvg, (double)diff);
                        }
                        REC.set(Rec::CurrentScore, (double)state.get_cost());
                        REC.add(Rec::Accepted, 1.0);
                        REC.chmin(Rec::PeriodBestScore, (double)state.get_cost());
                        REC.add(Rec::PeriodAccepted, 1.0);
                        tabu.add(new_hash);
                    }
                }
            }
            if(!improved) {
                no_improve_count++;
                REC.add(Rec::NoImprovementCount, 1.0);
            }
        }
        auto total_time = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - local_start_time).count();
        cerr << "SA Finished: best_score=" << best_state.get_cost()
             << ", iteration=" << iteration
             << ", time=" << total_time << "ms\n";

        if(conf.debug_mode) {
            REC.write("sa_log.csv");
            auto prev_cost = best_state.get_cost();
            auto prev_hash = best_state.get_hash();
            best_state.recalc();
            assert(best_state.get_cost() == prev_cost);
            assert(best_state.get_hash() == prev_hash);
        } else {
            best_state.recalc();
        }
        return best_state;
    }
};
