/*
 * tsp_solver: AHC 向けの TSP / Hamilton path ヒューリスティック solver
 *
 * 重要頂点の集合と頂点間距離を受け取り、全頂点を 1 回ずつ訪れる閉路または開路を構築・改善する。
 * nearest-neighbor 初期解を候補近傍付き 2-opt / Or-opt / swap /
 * block transposition と iterated local search で改善し、
 * 小区間の Held-Karp repair で局所最適を補強する。非対称距離では反転区間内の
 * 向きの変化も含めて正しく差分評価する。
 * 頂点 ID は bool を除く整数型であれば連続している必要はない。
 * 向きを保つ隣接ブロック転置を使い、探索構成は一種類に固定する。
 * 公開の Held-Karp 厳密解では、頂点対距離をキャッシュし、集合内・集合外の
 * 立っているビットだけを走査する。入力分布固有の規則は持たない。
 *
 * 主な使い方:
 *   std::vector<int> order(n);
 *   std::iota(order.begin(), order.end(), 0);
 *   auto dist = [&](int u, int v) { return distance[u][v]; };
 *
 *   tsp::TSPParam param;
 *   param.time_limit_ms = 100;
 *   param.cycle = true;
 *   param.symmetric = true;
 *   const long long cost = tsp::solve_tsp(order, dist, param);
 *
 * 既存解を改善する場合:
 *   const long long cost = tsp::improve_tsp(order, dist, param);
 *
 * 開路で両端を固定する場合:
 *   order.front() と order.back() に固定端点を置き、cycle=false、
 *   fixed_start=true、fixed_end=true を指定する。
 *
 * 前提と注意:
 * - dist(u, v) は静的な辺コストを返すこと
 * - symmetric=true は dist(u, v) == dist(v, u) の場合に限る
 * - 負辺自体は差分評価できるが、通常の TSP 距離として非負コストを想定する
 * - 距離和と差分が Calc に収まることを呼び出し側で保証する。オーバーフロー検査は行わない
 * - 時間制限は候補近傍構築と初期解構築を含む。時計確認間隔ぶん僅かに超過する場合がある
 */
#pragma once

#include <bits/stdc++.h>

namespace tsp {

struct TSPParam;
struct TSPStats;

template <class Calc = void, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto total_distance(std::span<const T> order, const F& dist, bool cycle = true);

template <class Calc = void, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto improve_tsp(std::span<T> order, const F& dist, const TSPParam& param = {},
                 TSPStats* stats = nullptr);

template <class Calc = void, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto solve_tsp(std::span<T> order, const F& dist, const TSPParam& param = {},
               TSPStats* stats = nullptr);

template <class Calc = void, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto held_karp_tsp(std::span<T> order, const F& dist, bool cycle = true,
                   bool fixed_start = false, bool fixed_end = false);

struct TSPStats {
    std::uint64_t move_evaluations = 0;
    std::uint64_t accepted_moves = 0;
    std::uint64_t local_search_runs = 0;
    std::uint64_t kicks = 0;
    std::uint64_t improved_kicks = 0;
    std::uint64_t exact_repairs = 0;
    std::uint64_t improved_exact_repairs = 0;
    double candidate_build_ms = 0.0;
    double construction_ms = 0.0;
    double search_ms = 0.0;
    double total_ms = 0.0;
};

struct TSPParam {
    int time_limit_ms = 100;
    std::uint64_t seed = 1;
    bool cycle = true;
    bool fixed_start = false;
    bool fixed_end = false;
    bool symmetric = true;
    std::uint64_t max_move_evaluations = 0;

private:
    template <class D>
    using default_calc_t = std::conditional_t<
        std::floating_point<std::remove_cvref_t<D>>, double, long long>;

    struct FastRng {
        std::uint64_t state;

        explicit FastRng(std::uint64_t seed) : state(seed) {}

        int index(int n) {
            assert(n > 0);
            std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return static_cast<int>((z ^ (z >> 31)) % static_cast<std::uint64_t>(n));
        }
    };

    struct SearchControl {
        static constexpr std::uint64_t time_check_interval = 512;

        std::chrono::steady_clock::time_point begin;
        std::chrono::steady_clock::time_point deadline;
        std::uint64_t max_evaluations;
        TSPStats* stats;
        std::uint64_t next_time_check;
        bool timed_out = false;

        SearchControl(const TSPParam& param, TSPStats* stats_)
            : begin(std::chrono::steady_clock::now()),
              deadline(begin + std::chrono::milliseconds(param.time_limit_ms)),
              max_evaluations(param.max_move_evaluations),
              stats(stats_),
              next_time_check(time_check_interval) {}

        bool expired_now() {
            if (max_evaluations != 0 && stats->move_evaluations >= max_evaluations) {
                timed_out = true;
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out = true;
                return true;
            }
            return false;
        }

        bool after_evaluation() {
            ++stats->move_evaluations;
            if (max_evaluations != 0 && stats->move_evaluations >= max_evaluations) {
                timed_out = true;
                return true;
            }
            if (stats->move_evaluations < next_time_check) return false;
            next_time_check = stats->move_evaluations + time_check_interval;
            if (std::chrono::steady_clock::now() >= deadline) {
                timed_out = true;
                return true;
            }
            return false;
        }
    };

    struct CandidateSet {
        int width = 0;
        std::vector<int> count;
        std::vector<int> data;

        std::span<const int> row(int vertex) const {
            if (width == 0) return {};
            return std::span<const int>(data.data() + vertex * width,
                                        static_cast<std::size_t>(count[vertex]));
        }
    };

    template <class Calc, class Distance>
    static Calc total_local(std::span<const int> order, const Distance& dist, bool cycle) {
        const int n = static_cast<int>(order.size());
        Calc result{};
        for (int i = 0; i + 1 < n; ++i) result += dist(order[i], order[i + 1]);
        if (cycle && n >= 2) result += dist(order[n - 1], order[0]);
        return result;
    }

    template <class Calc>
    struct LocalSearchBuffers {
        std::vector<int> position;
        std::vector<unsigned char> inactive;
        std::vector<int> touched;
        std::vector<Calc> reverse;
    };

    template <class Calc, class Distance>
    static Calc local_search(std::vector<int>& ls_route, const Distance& ls_dist,
                      const CandidateSet& ls_candidates, const TSPParam& ls_param,
                      SearchControl& ls_control, Calc ls_current, LocalSearchBuffers<Calc>& buffers) {
        const int ls_n = static_cast<int>(ls_route.size());
        if (ls_n <= 2 || ls_candidates.width == 0) return ls_current;
        ++ls_control.stats->local_search_runs;

        auto& ls_position = buffers.position;
        ls_position.resize(ls_n);
        for (int i = 0; i < ls_n; ++i) ls_position[ls_route[i]] = i;
        auto& inactive = buffers.inactive;
        inactive.assign(ls_n, 0);
        int cursor = 0;
        int inactive_in_a_row = 0;
        enum class MoveKind : unsigned char {
            None,
            TwoOpt,
            Relocate,
            Swap,
            BlockSwap,
        };

        struct Move {
            MoveKind kind = MoveKind::None;
            Calc delta{};
            int a = 0;
            int b = 0;
            int c = 0;
        };
        Move ls_move;
        auto& ls_touched = buffers.touched;

        static constexpr auto rebuild_reverse_prefix = [](std::span<const int> prefix_route,
            const Distance& prefix_dist, std::vector<Calc>& values) {
            const int count = static_cast<int>(prefix_route.size());
            values.assign(count, Calc{});
            for (int i = 0; i + 1 < count; ++i) {
                values[i + 1] = values[i] + prefix_dist(prefix_route[i + 1], prefix_route[i])
                    - prefix_dist(prefix_route[i], prefix_route[i + 1]);
            }
        };

        static constexpr auto find_move_for_vertex = [](int scan_vertex, std::vector<int>& scan_route,
                                  std::vector<int>& scan_position, const Distance& scan_dist,
                                  const CandidateSet& scan_candidates, const TSPParam& scan_param,
                                  std::span<const Calc> scan_reverse, SearchControl& scan_control,
                                  Move& scan_move, std::vector<int>& scan_touched) -> bool {
            static constexpr auto two_opt_delta = [](std::span<const int> opt_route, const Distance& opt_dist,
                               bool opt_cycle, bool opt_symmetric, std::span<const Calc> opt_reverse,
                               int l, int r) -> Calc {
                const int opt_n = static_cast<int>(opt_route.size());
                Calc delta{};

                if (opt_cycle || l > 0) {
                    const int previous = l > 0 ? l - 1 : opt_n - 1;
                    delta += opt_dist(opt_route[previous], opt_route[r]) - opt_dist(opt_route[previous], opt_route[l]);
                }
                if (opt_cycle || r + 1 < opt_n) {
                    const int next = r + 1 < opt_n ? r + 1 : 0;
                    delta += opt_dist(opt_route[l], opt_route[next]) - opt_dist(opt_route[r], opt_route[next]);
                }
                if (!opt_symmetric) delta += opt_reverse[r] - opt_reverse[l];
                return delta;
            };

            static constexpr auto valid_relocate = [](int valid_n, bool valid_cycle, bool valid_fixed_start, bool valid_fixed_end,
                                       int l, int length, int after) -> bool {
                const int r = l + length - 1;
                if (l < 0 || r >= valid_n || length >= valid_n) return false;
                if (after >= l - 1 && after <= r) return false;
                if (valid_cycle && valid_n - length < 2) return false;
                // 閉路は valid_route[0] を表現上の固定アンカーとする。
                // 0 を含む block の移動は切断辺と挿入辺が重なる特殊形になり、
                // 同じ閉路を 0 始まりに回転した表現で他の近傍から探索できるため除外する。
                if (valid_cycle && l == 0) return false;
                if (valid_fixed_start && l == 0) return false;
                if (!valid_cycle && valid_fixed_end && r == valid_n - 1) return false;
                if (!valid_cycle && valid_fixed_start && after < 0) return false;
                if (!valid_cycle && valid_fixed_end && after == valid_n - 1) return false;
                if (valid_cycle && (after < 0 || after >= valid_n)) return false;
                if (!valid_cycle && (after < -1 || after >= valid_n)) return false;
                return true;
            };

            static constexpr auto relocate_delta = [](std::span<const int> rel_route, const Distance& rel_dist, bool rel_cycle,
                                int l, int length, int after) -> Calc {
                const int rel_n = static_cast<int>(rel_route.size());
                const int r = l + length - 1;
                Calc delta{};

                const bool has_previous = rel_cycle || l > 0;
                const bool has_next = rel_cycle || r + 1 < rel_n;
                const int previous = l > 0 ? l - 1 : rel_n - 1;
                const int next = r + 1 < rel_n ? r + 1 : 0;
                if (has_previous) delta -= rel_dist(rel_route[previous], rel_route[l]);
                if (has_next) delta -= rel_dist(rel_route[r], rel_route[next]);
                if (has_previous && has_next) delta += rel_dist(rel_route[previous], rel_route[next]);

                const bool has_left = rel_cycle || after >= 0;
                const bool has_right = rel_cycle || after + 1 < rel_n;
                const int left = after >= 0 ? after : rel_n - 1;
                const int right = after + 1 < rel_n ? after + 1 : 0;
                if (has_left && has_right) delta -= rel_dist(rel_route[left], rel_route[right]);
                if (has_left) delta += rel_dist(rel_route[left], rel_route[l]);
                if (has_right) delta += rel_dist(rel_route[r], rel_route[right]);
                return delta;
            };

            static constexpr auto swap_delta = [](std::span<const int> swap_route, const Distance& swap_dist, bool swap_cycle, int swap_a, int swap_b) -> Calc {
                const int swap_n = static_cast<int>(swap_route.size());
                std::array<int, 4> edges = {swap_a - 1, swap_a, swap_b - 1, swap_b};
                for (int& edge : edges) {
                    if (swap_cycle) {
                        if (edge < 0) edge += swap_n;
                    }
                }
                std::sort(edges.begin(), edges.end());

                auto vertex_after_swap = [&](int swap_position) {
                    if (swap_position == swap_a) return swap_route[swap_b];
                    if (swap_position == swap_b) return swap_route[swap_a];
                    return swap_route[swap_position];
                };

                Calc delta{};
                int previous_edge = -2;
                for (int edge : edges) {
                    if (edge == previous_edge) continue;
                    previous_edge = edge;
                    if (!swap_cycle && (edge < 0 || edge + 1 >= swap_n)) continue;
                    const int next = (edge + 1 == swap_n) ? 0 : edge + 1;
                    delta -= swap_dist(swap_route[edge], swap_route[next]);
                    delta += swap_dist(vertex_after_swap(edge), vertex_after_swap(next));
                }
                return delta;
            };

            static constexpr auto find_block_swap_for_vertex = [](int block_vertex, std::vector<int>& block_route,
                                            std::vector<int>& block_position, const Distance& block_dist,
                                            const CandidateSet& block_candidates, const TSPParam& block_param,
                                            SearchControl& block_control, Move& block_move,
                                            std::vector<int>& block_touched) -> bool {
                if (block_control.timed_out) return false;
                static constexpr auto block_swap_delta = [](std::span<const int> delta_route, const Distance& delta_dist,
                                      bool delta_cycle, int i, int j, int k) -> Calc {
                    const int delta_n = static_cast<int>(delta_route.size());
                    Calc delta = delta_dist(delta_route[i], delta_route[j + 1]) + delta_dist(delta_route[k], delta_route[i + 1]) -
                                 delta_dist(delta_route[i], delta_route[i + 1]) - delta_dist(delta_route[j], delta_route[j + 1]);
                    if (delta_cycle || k + 1 < delta_n) {
                        const int next = k + 1 < delta_n ? k + 1 : 0;
                        delta += delta_dist(delta_route[j], delta_route[next]) - delta_dist(delta_route[k], delta_route[next]);
                    }
                    return delta;
                };
                const int block_n = static_cast<int>(block_route.size());
                const int i = block_position[block_vertex];

                // 新しい境界辺 A-C と B-D の両方を候補辺から選ぶ。
                // 第三の新辺 C-B は block の交換で一意に決まるため、探索量は O(candidate_count^2)。
                for (int c_start : block_candidates.row(block_vertex)) {
                    const int j = block_position[c_start] - 1;
                    if (i >= j || j + 1 >= block_n) continue;
                    const int b_end = block_route[j];
                    for (int d_start : block_candidates.row(b_end)) {
                        const int d_position = block_position[d_start];
                        if (!block_param.cycle && d_position == 0) continue;
                        const int k = d_position == 0 ? block_n - 1 : d_position - 1;
                        if (j >= k) continue;
                        // 開路では後続頂点 D が必要。固定終点は D 側に残る。
                        if (!block_param.cycle && k + 1 >= block_n) continue;

                        const Calc delta = block_swap_delta(block_route, block_dist, block_param.cycle, i, j, k);
                        if (block_control.after_evaluation()) return false;
                        if (delta < Calc{}) {
                            block_move = {MoveKind::BlockSwap, delta, i, j, k};
                            block_touched = {block_route[i], block_route[i + 1], block_route[j],
                                       block_route[j + 1], block_route[k], block_route[(k + 1) % block_n]};
                            return true;
                        }
                    }
                }
                return false;
            };
            const int scan_n = static_cast<int>(scan_route.size());
            const int pv = scan_position[scan_vertex];
            scan_move = Move{};

            auto consider_two_opt = [&](int l, int r, int other) {
                if (scan_control.timed_out) return false;
                if (l < 0 || r >= scan_n || l >= r) return false;
                if (scan_param.fixed_start && l == 0) return false;
                if (!scan_param.cycle && scan_param.fixed_end && r == scan_n - 1) return false;
                if (scan_param.cycle && l == 0 && r == scan_n - 1) return false;
                const Calc delta = two_opt_delta(scan_route, scan_dist, scan_param.cycle,
                                                       scan_param.symmetric, scan_reverse, l, r);
                if (scan_control.after_evaluation()) return false;
                if (delta < Calc{}) {
                    scan_move = {MoveKind::TwoOpt, delta, l, r, 0};
                    scan_touched = {scan_vertex, other, scan_route[l], scan_route[r]};
                    return true;
                }
                return false;
            };

            auto consider_relocate = [&](int l, int length, int after, int other) {
                if (scan_control.timed_out) return false;
                if (!valid_relocate(scan_n, scan_param.cycle, scan_param.fixed_start, scan_param.fixed_end,
                                    l, length, after)) {
                    return false;
                }
                const Calc delta = relocate_delta(scan_route, scan_dist, scan_param.cycle, l, length, after);
                if (scan_control.after_evaluation()) return false;
                if (delta < Calc{}) {
                    scan_move = {MoveKind::Relocate, delta, l, length, after};
                    scan_touched = {scan_vertex, other, scan_route[l], scan_route[l + length - 1]};
                    return true;
                }
                return false;
            };

            auto consider_swap = [&](int other) {
                if (scan_control.timed_out) return false;
                const int po = scan_position[other];
                if (po == pv) return false;
                if (scan_param.fixed_start && (pv == 0 || po == 0)) return false;
                if (!scan_param.cycle && scan_param.fixed_end && (pv == scan_n - 1 || po == scan_n - 1)) return false;
                const Calc delta = swap_delta(scan_route, scan_dist, scan_param.cycle, pv, po);
                if (scan_control.after_evaluation()) return false;
                if (delta < Calc{}) {
                    scan_move = {MoveKind::Swap, delta, pv, po, 0};
                    scan_touched = {scan_vertex, other};
                    return true;
                }
                return false;
            };

            for (int other : scan_candidates.row(scan_vertex)) {
                if (scan_control.timed_out) return false;
                const int po = scan_position[other];

                const int edge_left = std::min(pv, po);
                const int edge_right = std::max(pv, po);
                if (edge_left + 1 < edge_right &&
                    !(scan_param.cycle && edge_left == 0 && edge_right == scan_n - 1) &&
                    consider_two_opt(edge_left + 1, edge_right, other)) {
                    return true;
                }

                if (!scan_param.cycle && !scan_param.fixed_start && pv == 0 && po >= 2 &&
                    consider_two_opt(0, po - 1, other)) {
                    return true;
                }
                if (!scan_param.cycle && !scan_param.fixed_end && pv == scan_n - 1 && po + 1 < scan_n - 1 &&
                    consider_two_opt(po + 1, scan_n - 1, other)) {
                    return true;
                }

                for (int length = 1; length <= 3; ++length) {
                    for (int l = pv; l >= pv - length + 1; l -= std::max(1, length - 1)) {
                        if (consider_relocate(l, length, po, other)) return true;
                        int before = po - 1;
                        if (scan_param.cycle && before < 0) before += scan_n;
                        if (consider_relocate(l, length, before, other)) return true;
                    }
                }

                if (consider_swap(other)) return true;
            }
            if (find_block_swap_for_vertex(scan_vertex, scan_route, scan_position, scan_dist, scan_candidates,
                                                 scan_param, scan_control, scan_move, scan_touched)) {
                return true;
            }
            return false;
        };

        static constexpr auto apply_move = [](std::vector<int>& apply_route, std::vector<int>& apply_position,
                               const MoveKind kind, int apply_a, int apply_b, int apply_c) -> void {
            static constexpr auto update_positions = [](std::span<const int> update_route, std::vector<int>& update_position,
                                         int update_begin, int update_end) -> void {
                for (int i = update_begin; i < update_end; ++i) update_position[update_route[i]] = i;
            };
            if (kind == MoveKind::TwoOpt) {
                std::reverse(apply_route.begin() + apply_a, apply_route.begin() + apply_b + 1);
                update_positions(apply_route, apply_position, apply_a, apply_b + 1);
                return;
            }
            if (kind == MoveKind::Relocate) {
                const int l = apply_a;
                const int r = apply_a + apply_b - 1;
                if (apply_c < l) {
                    std::rotate(apply_route.begin() + apply_c + 1, apply_route.begin() + l, apply_route.begin() + r + 1);
                    update_positions(apply_route, apply_position, apply_c + 1, r + 1);
                } else {
                    std::rotate(apply_route.begin() + l, apply_route.begin() + r + 1, apply_route.begin() + apply_c + 1);
                    update_positions(apply_route, apply_position, l, apply_c + 1);
                }
                return;
            }
            if (kind == MoveKind::Swap) {
                std::swap(apply_route[apply_a], apply_route[apply_b]);
                apply_position[apply_route[apply_a]] = apply_a;
                apply_position[apply_route[apply_b]] = apply_b;
                return;
            }
            if (kind == MoveKind::BlockSwap) {
                std::rotate(apply_route.begin() + apply_a + 1, apply_route.begin() + apply_b + 1,
                            apply_route.begin() + apply_c + 1);
                update_positions(apply_route, apply_position, apply_a + 1, apply_c + 1);
            }
        };
        auto& ls_reverse = buffers.reverse;
        if (!ls_param.symmetric) rebuild_reverse_prefix(ls_route, ls_dist, ls_reverse);

        auto reactivate = [&](int ls_vertex) { inactive[ls_vertex] = 0; };

        while (inactive_in_a_row < ls_n && !ls_control.timed_out) {
            const int ls_vertex = ls_route[cursor];
            cursor = (cursor + 1 == ls_n) ? 0 : cursor + 1;
            if (inactive[ls_vertex]) {
                ++inactive_in_a_row;
                continue;
            }

            ls_touched.clear();
            if (find_move_for_vertex(ls_vertex, ls_route, ls_position, ls_dist, ls_candidates,
                                           ls_param, ls_reverse, ls_control, ls_move, ls_touched)) {
                apply_move(ls_route, ls_position, ls_move.kind, ls_move.a, ls_move.b, ls_move.c);
                ls_current += ls_move.delta;
                ++ls_control.stats->accepted_moves;
                inactive_in_a_row = 0;

                for (int v : ls_touched) {
                    reactivate(v);
                    for (int neighbor : ls_candidates.row(v)) reactivate(neighbor);
                }
                if (!ls_param.symmetric) {
                    rebuild_reverse_prefix(ls_route, ls_dist, ls_reverse);
                }
            } else {
                inactive[ls_vertex] = 1;
                ++inactive_in_a_row;
            }
        }
        return ls_current;
    }

    template <class T, class F, class Calc>
    static Calc solve_impl(std::span<T> output, const F& original_dist, const TSPParam& param,
                    TSPStats* output_stats, bool improve_only) {
        assert(param.time_limit_ms >= 0);
        assert(!param.cycle || !param.fixed_end);
        TSPStats local_stats;
        TSPStats* stats = output_stats ? output_stats : &local_stats;
        *stats = TSPStats{};
        SearchControl control(param, stats);

        const int n = static_cast<int>(output.size());
        if (n <= 1) {
            stats->total_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - control.begin)
                                  .count();
            return Calc{};
        }

        std::vector<T> vertices(output.begin(), output.end());
        auto dist = [vertex_ids = std::span<const T>(vertices), &original_dist](int a, int b) -> Calc {
            return static_cast<Calc>(original_dist(vertex_ids[a], vertex_ids[b]));
        };
        FastRng rng(param.seed);
        std::vector<int> route(n);
        Calc current{};
        if (improve_only) {
            std::iota(route.begin(), route.end(), 0);
            current = total_local<Calc>(route, dist, param.cycle);
        }

        // 候補構築が期限を使い切っても入力順へ退化しないよう、先に高速な初期解を確保する。
        const auto construction_begin = std::chrono::steady_clock::now();
        if (!improve_only) {
            static constexpr auto nearest_neighbor_construct = [](std::vector<int>& nn_route, int nn_n, const decltype(dist)& nn_dist,
                                            const TSPParam& nn_param, FastRng& nn_rng,
                                            SearchControl& nn_control) -> Calc {
                nn_route.clear();

                std::vector<unsigned char> used(nn_n, 0);
                int nn_current = 0;
                if (!nn_param.fixed_start && !nn_param.cycle) {
                    nn_current = nn_rng.index(nn_n - (nn_param.fixed_end ? 1 : 0));
                }
                nn_route.push_back(nn_current);
                used[nn_current] = 1;

                const int reserved_end = (!nn_param.cycle && nn_param.fixed_end) ? nn_n - 1 : -1;
                while (static_cast<int>(nn_route.size()) < nn_n - (reserved_end >= 0 ? 1 : 0)) {
                    int best = -1;
                    Calc best_cost{};
                    for (int v = 0; v < nn_n; ++v) {
                        if (used[v] || v == reserved_end) continue;
                        const Calc cost = nn_dist(nn_current, v);
                        if (best < 0 || cost < best_cost) {
                            best_cost = cost;
                            best = v;
                        }
                    }
                    if (best < 0 || nn_control.expired_now()) break;
                    nn_route.push_back(best);
                    used[best] = 1;
                    nn_current = best;
                }

                for (int v = 0; v < nn_n; ++v) {
                    if (!used[v] && v != reserved_end) nn_route.push_back(v);
                }
                if (reserved_end >= 0) nn_route.push_back(reserved_end);
                return total_local<Calc>(nn_route, nn_dist, nn_param.cycle);
            };
            current = nearest_neighbor_construct(route, n, dist, param, rng, control);
        }
        const auto first_construction_end = std::chrono::steady_clock::now();
        stats->construction_ms = std::chrono::duration<double, std::milli>(
                                     first_construction_end - construction_begin)
                                     .count();

        // 小規模入力は構築結果を返し、無効なkickを繰り返さない。
        if (n <= 2 || (!param.cycle && param.fixed_start && param.fixed_end && n == 3)) {
            for (int i = 0; i < n; ++i) output[i] = vertices[route[i]];
            stats->total_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - control.begin).count();
            return current;
        }

        const auto candidate_begin = std::chrono::steady_clock::now();
        static constexpr auto build_candidates = [](int build_n, bool build_symmetric, const decltype(dist)& build_dist,
                                      SearchControl& build_control) -> CandidateSet {
            CandidateSet result;

            constexpr int candidate_count = 8;
            const int per_direction = std::min(candidate_count, build_n - 1);
            result.width = build_symmetric ? per_direction : std::min(build_n - 1, per_direction * 2);
            result.count.assign(build_n, 0);
            result.data.resize(static_cast<std::size_t>(build_n) * static_cast<std::size_t>(result.width));

            auto better = [](const auto& lhs, const auto& rhs) {
                if (lhs.first != rhs.first) return lhs.first < rhs.first;
                return lhs.second < rhs.second;
            };

            std::vector<std::pair<Calc, int>> values;
            values.reserve(per_direction);
            std::vector<int> mark(build_n, -1);

            auto collect = [&](int build_vertex, bool incoming) {
                values.clear();
                int worst = -1;
                for (int other = 0; other < build_n; ++other) {
                    if (build_vertex == other) continue;
                    const Calc cost = incoming ? build_dist(other, build_vertex) : build_dist(build_vertex, other);
                    const std::pair<Calc, int> item = {cost, other};
                    if (static_cast<int>(values.size()) < per_direction) {
                        values.push_back(item);
                        if (worst < 0 || better(values[worst], item)) {
                            worst = static_cast<int>(values.size()) - 1;
                        }
                    } else if (better(item, values[worst])) {
                        values[worst] = item;
                        worst = 0;
                        for (int i = 1; i < per_direction; ++i) {
                            if (better(values[worst], values[i])) worst = i;
                        }
                    }
                }
                std::sort(values.begin(), values.end(), better);
            };

            for (int i = 0; i < build_n; ++i) {
                if (build_control.expired_now()) break;
                collect(i, false);

                int size = 0;
                int* out = result.data.data() + i * result.width;
                for (const auto& item : values) {
                    const int build_vertex = item.second;
                    out[size++] = build_vertex;
                    mark[build_vertex] = i;
                }

                if (!build_symmetric && size < result.width) {
                    collect(i, true);
                    for (const auto& item : values) {
                        const int build_vertex = item.second;
                        if (mark[build_vertex] != i) out[size++] = build_vertex;
                    }
                }
                result.count[i] = size;
            }
            return result;
        };
        CandidateSet candidates = build_candidates(n, param.symmetric, dist, control);
        const auto candidate_end = std::chrono::steady_clock::now();
        stats->candidate_build_ms =
            std::chrono::duration<double, std::milli>(candidate_end - candidate_begin).count();

        const auto search_begin = std::chrono::steady_clock::now();
        static constexpr auto run_search = [](std::vector<int>& ils_route, const decltype(dist)& ils_dist,
                        const CandidateSet& ils_candidates, const TSPParam& ils_param,
                        FastRng& ils_rng, SearchControl& ils_control, Calc ils_current) -> Calc {
            static constexpr auto kick_cycle = [](std::vector<int>& kc_route, FastRng& kc_rng, bool kc_fixed_start) -> void {
                const int kc_n = static_cast<int>(kc_route.size());
                if (kc_n < 8) {
                    const int kc_begin = kc_fixed_start ? 1 : 0;
                    if (kc_n - kc_begin >= 3) {
                        int l = kc_begin + kc_rng.index(kc_n - kc_begin - 1);
                        int r = l + 1 + kc_rng.index(kc_n - l - 1);
                        std::reverse(kc_route.begin() + l, kc_route.begin() + r + 1);
                    }
                    return;
                }

                std::array<int, 3> cut;
                do {
                    cut = {1 + kc_rng.index(kc_n - 1), 1 + kc_rng.index(kc_n - 1), 1 + kc_rng.index(kc_n - 1)};
                    std::sort(cut.begin(), cut.end());
                } while (cut[0] == cut[1] || cut[1] == cut[2]);
                std::rotate(kc_route.begin() + cut[0], kc_route.begin() + cut[1], kc_route.begin() + cut[2]);
            };

            static constexpr auto kick_path = [](std::vector<int>& kp_route, FastRng& kp_rng,
                                  bool kp_fixed_start, bool kp_fixed_end) -> void {
                const int kp_n = static_cast<int>(kp_route.size());
                const int kp_begin = kp_fixed_start ? 1 : 0;
                const int kp_end = kp_fixed_end ? kp_n - 1 : kp_n;
                if (kp_end - kp_begin < 3) return;

                int kp_a = kp_begin + kp_rng.index(kp_end - kp_begin - 1);
                int kp_b = kp_a + 1 + kp_rng.index(kp_end - kp_a - 1);
                if (kp_b + 1 < kp_end) {
                    const int kp_c = kp_b + 1 + kp_rng.index(kp_end - kp_b - 1);
                    std::rotate(kp_route.begin() + kp_a, kp_route.begin() + kp_b, kp_route.begin() + kp_c);
                } else {
                    std::reverse(kp_route.begin() + kp_a, kp_route.begin() + kp_b + 1);
                }
            };
            if (ils_control.expired_now()) return ils_current;
            LocalSearchBuffers<Calc> buffers;
            buffers.touched.reserve(8);
            ils_current = local_search<Calc>(ils_route, ils_dist, ils_candidates, ils_param, ils_control, ils_current, buffers);
            auto try_window_repair = [&]() -> bool {    
            const int repair_n = static_cast<int>(ils_route.size());
            const int repair_window = std::min(8, repair_n - 2);
            if (repair_window < 3 || ils_control.expired_now()) return false;
    
            // ils_route[0] を閉路の表現上のアンカーとして残し、両側の境界頂点を固定できる区間を選ぶ。
            const int repair_l = 1 + ils_rng.index(repair_n - repair_window - 1);
            const int repair_r = repair_l + repair_window - 1;
            const int repair_left = ils_route[repair_l - 1];
            const int repair_right = ils_route[repair_r + 1];
            std::vector<int> repair_vertex(ils_route.begin() + repair_l, ils_route.begin() + repair_r + 1);
    
            const std::uint64_t repair_states = std::uint64_t{1} << repair_window;
            const std::uint64_t repair_full = repair_states - 1;
            // 有限距離の完全グラフなので、集合内の各終点へ必ず到達できる。
            // 未設定の遷移先はparentで区別し、コスト値を番兵として使わない。
            std::vector<Calc> repair_dp(static_cast<std::size_t>(repair_states) * static_cast<std::size_t>(repair_window));
            std::vector<signed char> repair_parent(static_cast<std::size_t>(repair_states) *
                                            static_cast<std::size_t>(repair_window), -1);
    
            // 小区間内では同じ距離を何度も参照するため、callback 呼び出しと頂点 ID の
            // 間接参照を DP の外へ出す。行列・左右境界を一つの連続領域にまとめる。
            const std::size_t repair_matrix_size = static_cast<std::size_t>(repair_window) * repair_window;
            std::vector<Calc> repair_distance_cache(repair_matrix_size + static_cast<std::size_t>(repair_window) * 2);
            Calc* const repair_local_distance = repair_distance_cache.data();
            Calc* const repair_left_distance = repair_local_distance + repair_matrix_size;
            Calc* const repair_right_distance = repair_left_distance + repair_window;
            for (int repair_i = 0; repair_i < repair_window; ++repair_i) {
                repair_left_distance[repair_i] = ils_dist(repair_left, repair_vertex[repair_i]);
                repair_right_distance[repair_i] = ils_dist(repair_vertex[repair_i], repair_right);
                for (int repair_j = 0; repair_j < repair_window; ++repair_j) {
                    repair_local_distance[static_cast<std::size_t>(repair_i) * repair_window + repair_j] =
                        ils_dist(repair_vertex[repair_i], repair_vertex[repair_j]);
                }
            }
    
            auto repair_index = [repair_window](std::uint64_t repair_mask, int repair_last) {
                return static_cast<std::size_t>(repair_mask) * static_cast<std::size_t>(repair_window) +
                       static_cast<std::size_t>(repair_last);
            };
            for (int repair_first = 0; repair_first < repair_window; ++repair_first) {
                repair_dp[repair_index(std::uint64_t{1} << repair_first, repair_first)] = repair_left_distance[repair_first];
            }
    
            std::uint64_t repair_transitions = 0;
            for (std::uint64_t repair_mask = 1; repair_mask < repair_states; ++repair_mask) {
                std::uint64_t repair_last_bits = repair_mask;
                const std::uint64_t repair_remaining = repair_full ^ repair_mask;
                while (repair_last_bits != 0) {
                    const int repair_last = std::countr_zero(repair_last_bits);
                    repair_last_bits &= repair_last_bits - 1;
                    const Calc repair_value = repair_dp[repair_index(repair_mask, repair_last)];
                    std::uint64_t repair_next_bits = repair_remaining;
                    while (repair_next_bits != 0) {
                        const int repair_next = std::countr_zero(repair_next_bits);
                        repair_next_bits &= repair_next_bits - 1;
                        const std::uint64_t repair_next_mask = repair_mask | (std::uint64_t{1} << repair_next);
                        const Calc repair_candidate = repair_value +
                            repair_local_distance[static_cast<std::size_t>(repair_last) * repair_window + repair_next];
                        Calc& repair_destination = repair_dp[repair_index(repair_next_mask, repair_next)];
                        if (repair_parent[repair_index(repair_next_mask, repair_next)] < 0 ||
                            repair_candidate < repair_destination) {
                            repair_destination = repair_candidate;
                            repair_parent[repair_index(repair_next_mask, repair_next)] = static_cast<signed char>(repair_last);
                        }
                        ++repair_transitions;
                        if ((repair_transitions & 4095U) == 0 && ils_control.expired_now()) return false;
                    }
                }
            }
    
            Calc repair_best{};
            int repair_best_last = -1;
            for (int repair_last = 0; repair_last < repair_window; ++repair_last) {
                const Calc repair_candidate = repair_dp[repair_index(repair_full, repair_last)] + repair_right_distance[repair_last];
                if (repair_best_last < 0 || repair_candidate < repair_best) {
                    repair_best = repair_candidate;
                    repair_best_last = repair_last;
                }
            }
    
            Calc repair_old{};
            for (int repair_i = repair_l - 1; repair_i <= repair_r; ++repair_i) repair_old += ils_dist(ils_route[repair_i], ils_route[repair_i + 1]);
            ++ils_control.stats->exact_repairs;
            if (!(repair_best < repair_old)) return false;
    
            std::uint64_t repair_mask = repair_full;
            int repair_last = repair_best_last;
            for (int repair_position = repair_window - 1; repair_position >= 0; --repair_position) {
                ils_route[repair_l + repair_position] = repair_vertex[repair_last];
                const int repair_previous = repair_parent[repair_index(repair_mask, repair_last)];
                repair_mask ^= std::uint64_t{1} << repair_last;
                repair_last = repair_previous;
            }
            ils_current += repair_best - repair_old;
            ++ils_control.stats->improved_exact_repairs;
            return true;
        
            };
            if (!ils_control.timed_out &&
                try_window_repair()) {
                ils_current = local_search<Calc>(ils_route, ils_dist, ils_candidates, ils_param, ils_control, ils_current, buffers);
            }
            if (ils_control.timed_out) return ils_current;

            std::vector<int> best_route = ils_route;
            Calc best = ils_current;

            while (!ils_control.expired_now()) {
                if (ils_param.cycle) {
                    kick_cycle(ils_route, ils_rng, ils_param.fixed_start);
                } else {
                    kick_path(ils_route, ils_rng, ils_param.fixed_start, ils_param.fixed_end);
                }
                ++ils_control.stats->kicks;
                ils_current = total_local<Calc>(ils_route, ils_dist, ils_param.cycle);
                ils_current = local_search<Calc>(ils_route, ils_dist, ils_candidates, ils_param, ils_control, ils_current, buffers);
                if (!ils_control.timed_out && ils_control.stats->kicks % 4 == 0 &&
                    try_window_repair()) {
                    ils_current = local_search<Calc>(ils_route, ils_dist, ils_candidates, ils_param, ils_control, ils_current, buffers);
                }
                if (ils_current < best) {
                    best = ils_current;
                    best_route = ils_route;
                    ++ils_control.stats->improved_kicks;

                } else {

                    ils_route = best_route;
                }
            }
            ils_route = std::move(best_route);
            return best;
        };
        current = run_search(route, dist, candidates, param, rng, control, current);
        const auto search_end = std::chrono::steady_clock::now();
        stats->search_ms =
            std::chrono::duration<double, std::milli>(search_end - search_begin).count();

        for (int i = 0; i < n; ++i) output[i] = vertices[route[i]];
        stats->total_ms =
            std::chrono::duration<double, std::milli>(search_end - control.begin).count();
        return current;
    }

    template <class Calc, class T, class F>
    requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
    friend auto total_distance(std::span<const T> order, const F& dist, bool cycle);

    template <class Calc, class T, class F>
    requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
    friend auto improve_tsp(std::span<T> order, const F& dist, const TSPParam& param,
                     TSPStats* stats);

    template <class Calc, class T, class F>
    requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
    friend auto solve_tsp(std::span<T> order, const F& dist, const TSPParam& param,
                   TSPStats* stats);

    template <class Calc, class T, class F>
    requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
    friend auto held_karp_tsp(std::span<T> order, const F& dist, bool cycle,
                       bool fixed_start, bool fixed_end);
};

// 与えられた順序の総距離を返す。O(n)
template <class Calc, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto total_distance(std::span<const T> order, const F& dist, bool cycle) {
    using Result = std::conditional_t<std::same_as<Calc, void>,
                                      TSPParam::default_calc_t<std::invoke_result_t<F, T, T>>, Calc>;
    const int n = static_cast<int>(order.size());
    Result result{};
    for (int i = 0; i + 1 < n; ++i) result += static_cast<Result>(dist(order[i], order[i + 1]));
    if (cycle && n >= 2) result += static_cast<Result>(dist(order[n - 1], order[0]));
    return result;
}

// 既存の順序を ILS で改善し、改善後の総距離を返す。候補構築 O(n^2)、探索量は時間制限に依存
template <class Calc, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto improve_tsp(std::span<T> order, const F& dist, const TSPParam& param,
                 TSPStats* stats) {
    using Result = std::conditional_t<std::same_as<Calc, void>,
                                      TSPParam::default_calc_t<std::invoke_result_t<F, T, T>>, Calc>;
    return TSPParam::solve_impl<T, F, Result>(order, dist, param, stats, true);
}

// vector で与えた既存順序を改善し、改善後の総距離を返す。候補構築 O(n^2)、探索量は時間制限に依存
template <class Calc = void, class T, class Allocator, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto improve_tsp(std::vector<T, Allocator>& order, const F& dist,
                 const TSPParam& param = {}, TSPStats* stats = nullptr) {
    return improve_tsp<Calc>(std::span<T>(order), dist, param, stats);
}

// 頂点集合から nearest-neighbor 初期解を構築して ILS で改善し、総距離を返す。候補構築 O(n^2)、探索量は時間制限に依存
template <class Calc, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto solve_tsp(std::span<T> order, const F& dist, const TSPParam& param,
               TSPStats* stats) {
    using Result = std::conditional_t<std::same_as<Calc, void>,
                                      TSPParam::default_calc_t<std::invoke_result_t<F, T, T>>, Calc>;
    return TSPParam::solve_impl<T, F, Result>(order, dist, param, stats, false);
}

// vector で与えた頂点集合から経路を構築・改善し、総距離を返す。候補構築 O(n^2)、探索量は時間制限に依存
template <class Calc = void, class T, class Allocator, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto solve_tsp(std::vector<T, Allocator>& order, const F& dist,
               const TSPParam& param = {}, TSPStats* stats = nullptr) {
    return solve_tsp<Calc>(std::span<T>(order), dist, param, stats);
}

// Held-Karp法で厳密解へ書き換える。自由頂点数mに対しO(n^2 + m^2 2^m)時間、O(n^2 + m 2^m)メモリ
template <class Calc, class T, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto held_karp_tsp(std::span<T> order, const F& dist, bool cycle,
                   bool fixed_start, bool fixed_end) {
    using Result = std::conditional_t<std::same_as<Calc, void>,
                                      TSPParam::default_calc_t<std::invoke_result_t<F, T, T>>, Calc>;
    const int n = static_cast<int>(order.size());
    assert(n <= 22);
    assert(!cycle || !fixed_end);
    if (n <= 1) return Result{};

    const std::vector<T> vertices(order.begin(), order.end());
    // 固定端点は順序選択の対象から除き、DPの左右境界として扱う。
    const bool anchored = cycle || fixed_start;
    const int offset = anchored ? 1 : 0;
    const int m = n - offset - static_cast<int>(fixed_end);
    std::vector<Result> distance_cache(static_cast<std::size_t>(n) * n);
    for (int a = 0; a < n; ++a) {
        for (int b = 0; b < n; ++b) {
            if (a != b) distance_cache[static_cast<std::size_t>(a) * n + b] =
                static_cast<Result>(dist(vertices[a], vertices[b]));
        }
    }
    if (m == 0) return distance_cache[1]; // 2頂点の両端固定path
    const std::uint64_t states = std::uint64_t{1} << m;
    auto index = [m](std::uint64_t mask, int last) {
        return static_cast<std::size_t>(mask) * static_cast<std::size_t>(m) +
               static_cast<std::size_t>(last);
    };
    // 全距離が有限なので有効状態はすべて到達可能。最大整数も通常の費用として扱う。
    std::vector<Result> dp(static_cast<std::size_t>(states) * m);
    std::vector<signed char> parent(static_cast<std::size_t>(states) * m, -1);
    for (int first = 0; first < m; ++first) {
        dp[index(std::uint64_t{1} << first, first)] =
            anchored ? distance_cache[first + offset] : Result{};
    }
    for (std::uint64_t mask = 1; mask < states; ++mask) {
        std::uint64_t last_bits = mask;
        while (last_bits != 0) {
            const int last = std::countr_zero(last_bits);
            last_bits &= last_bits - 1;
            const std::uint64_t previous_mask = mask ^ (std::uint64_t{1} << last);
            if (previous_mask == 0) continue;
            Result best_value{};
            int best_previous = -1;
            std::uint64_t previous_bits = previous_mask;
            while (previous_bits != 0) {
                const int previous = std::countr_zero(previous_bits);
                previous_bits &= previous_bits - 1;
                const Result value = dp[index(previous_mask, previous)];
                const Result candidate = value + distance_cache[
                    static_cast<std::size_t>(previous + offset) * n + last + offset];
                if (best_previous < 0 || candidate < best_value) {
                    best_value = candidate;
                    best_previous = previous;
                }
            }
            dp[index(mask, last)] = best_value;
            parent[index(mask, last)] = static_cast<signed char>(best_previous);
        }
    }
    const std::uint64_t full = states - 1;
    Result best{};
    int last_best = -1;
    for (int last = 0; last < m; ++last) {
        Result candidate = dp[index(full, last)];
        if (cycle || fixed_end) candidate += distance_cache[
            static_cast<std::size_t>(last + offset) * n + (cycle ? 0 : n - 1)];
        if (last_best < 0 || candidate < best) { best = candidate; last_best = last; }
    }
    assert(last_best >= 0);
    std::uint64_t mask = full;
    int last = last_best;
    for (int position = m - 1; position >= 0; --position) {
        order[position + offset] = vertices[last + offset];
        const int previous = parent[index(mask, last)];
        mask ^= std::uint64_t{1} << last;
        last = previous;
    }
    return best;
}

// vectorに対して厳密解を求める。自由頂点数mに対しO(n^2 + m^2 2^m)時間、O(n^2 + m 2^m)メモリ
template <class Calc = void, class T, class Allocator, class F>
requires (std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>)
auto held_karp_tsp(std::vector<T, Allocator>& order, const F& dist,
                   bool cycle = true, bool fixed_start = false, bool fixed_end = false) {
    return held_karp_tsp<Calc>(std::span<T>(order), dist, cycle, fixed_start, fixed_end);
}

}  // namespace tsp

#if __INCLUDE_LEVEL__ == 0

// 大きな有限費用、負辺、評価回数制限を独立した全列挙・再計算で検証する
void test_tsp_review_regressions() {
    auto check = [](bool ok) { if (!ok) std::abort(); };
    auto check_exact = [&]<class Cost>() {
        const Cost maximum = std::numeric_limits<Cost>::max();
        std::vector<int> two = {0, 1};
        auto extreme_dist = [&](int a, int b) -> Cost {
            return a == 0 && b == 1 ? maximum : Cost{};
        };
        check(tsp::held_karp_tsp<Cost>(two, extreme_dist) == maximum);
        two = {0, 1};
        check(tsp::held_karp_tsp<Cost>(two, extreme_dist, false, true) == maximum);

        // 小さい係数に大きな定数を掛け、すべての部分和が型に収まる入力を作る。
        std::mt19937_64 random(419020);
        const Cost scale = maximum / 64;
        for (int trial = 0; trial < 48; ++trial) {
            const int n = 2 + trial % 6;
            std::vector<std::vector<Cost>> matrix(n, std::vector<Cost>(n));
            for (int a = 0; a < n; ++a) for (int b = 0; b < n; ++b) {
                if (a != b) matrix[a][b] = scale *
                    static_cast<Cost>(static_cast<int>(random() % 5) + (trial % 2 ? -2 : 1));
            }
            auto dist = [&](int a, int b) { return matrix[(a + 7) / 3][(b + 7) / 3]; };
            for (bool cycle : {false, true}) for (bool start : {false, true}) {
                for (bool finish : {false, true}) {
                    if (cycle && finish) continue;
                    std::vector<int> order(n);
                    for (int i = 0; i < n; ++i) order[i] = 3 * i - 7;
                    const auto vertices = order;
                    Cost optimum = maximum;
                    do {
                        if (start && order.front() != vertices.front()) continue;
                        if (finish && order.back() != vertices.back()) continue;
                        Cost sum{};
                        for (int i = 1; i < n; ++i) sum += dist(order[i - 1], order[i]);
                        if (cycle) sum += dist(order.back(), order.front());
                        optimum = std::min(optimum, sum);
                    } while (std::next_permutation(order.begin(), order.end()));
                    order = vertices;
                    const Cost answer = tsp::held_karp_tsp<Cost>(order, dist, cycle, start, finish);
                    check(answer == optimum);
                    check(answer == tsp::total_distance<Cost>(std::span<const int>(order), dist, cycle));
                    if (start) check(order.front() == vertices.front());
                    if (finish) check(order.back() == vertices.back());
                    std::sort(order.begin(), order.end());
                    check(order == vertices);
                }
            }
        }
    };
    check_exact.template operator()<long long>();
    check_exact.template operator()<__int128_t>();

    // 小区間repairの部分和が大きな有限値でも、正常な順列と費用を返す。
    for (const long long cost : {std::numeric_limits<long long>::max() / 16,
                                600'000'000'000'000'000LL}) {
        std::vector<int> order = {0, 1, 2, 3, 4};
        auto dist = [&](int a, int b) { return a == b ? 0LL : cost; };
        tsp::TSPParam param;
        param.time_limit_ms = 1000;
        param.max_move_evaluations = 3000;
        tsp::TSPStats stats;
        check(tsp::improve_tsp(order, dist, param, &stats) == cost * 5);
        check(stats.exact_repairs > 0);
        std::sort(order.begin(), order.end());
        check(order == std::vector<int>({0, 1, 2, 3, 4}));
    }

    // 終了する近傍の種類によらず、上限後の次候補を評価しない。
    for (bool symmetric : {false, true}) for (int mode = 0; mode < 6; ++mode) {
        auto dist = [&](int a, int b) {
            if (symmetric && a > b) std::swap(a, b);
            return a == b ? 0LL : static_cast<long long>((a * 17 + b * 31 + mode) % 53);
        };
        for (unsigned limit = 1; limit <= 80; ++limit) {
            std::vector<int> order = {0, 1, 2, 3, 4, 5, 6, 7, 8};
            tsp::TSPParam param;
            param.time_limit_ms = 1000;
            param.max_move_evaluations = limit;
            param.symmetric = symmetric;
            param.cycle = mode == 0 || mode == 1;
            param.fixed_start = mode == 1 || mode == 3 || mode == 5;
            param.fixed_end = mode == 4 || mode == 5;
            tsp::TSPStats stats;
            const auto before = tsp::total_distance(std::span<const int>(order), dist, param.cycle);
            const auto result = tsp::improve_tsp(order, dist, param, &stats);
            check(stats.move_evaluations <= limit);
            check(result <= before);
            check(result == tsp::total_distance(std::span<const int>(order), dist, param.cycle));
            if (param.fixed_start) check(order.front() == 0);
            if (param.fixed_end) check(order.back() == 8);
            std::sort(order.begin(), order.end());
            check(order == std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8}));
        }
    }
}

int main() {
    test_tsp_review_regressions();
    auto require_test = [&](bool condition, std::string_view message) {
        if (condition) return;
        std::cerr << "test failed: " << message << '\n';
        std::abort();
    };

    auto test_total_and_exact = [&]() {
        std::vector<std::vector<long long>> distance = {
            {0, 1, 9, 1},
            {1, 0, 1, 9},
            {9, 1, 0, 1},
            {1, 9, 1, 0},
        };
        auto dist = [&](int a, int b) { return distance[a][b]; };
        std::vector<int> order = {0, 2, 1, 3};
        const long long exact = tsp::held_karp_tsp(order, dist, true, true, false);
        require_test(exact == 4, "known cycle optimum");
        require_test(tsp::total_distance(std::span<const int>(order), dist, true) == exact,
                     "known cycle score");

        order = {0, 2, 1, 3};
        const long long path = tsp::held_karp_tsp(order, dist, false, true, true);
        require_test(order.front() == 0, "exact path fixed start");
        require_test(order.back() == 3, "exact path fixed end");
        require_test(tsp::total_distance(std::span<const int>(order), dist, false) == path,
                     "exact path score");
    };

    auto test_random_against_exact = [&]() {
        std::mt19937_64 rng(1234567);
        for (int tc = 0; tc < 80; ++tc) {
            const int n = 3 + static_cast<int>(rng() % 8);
            std::vector<long long> matrix(static_cast<std::size_t>(n) * static_cast<std::size_t>(n));
            const bool symmetric = (tc % 2 == 0);
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    if (i == j) {
                        matrix[i * n + j] = 0;
                    } else if (symmetric && j < i) {
                        matrix[i * n + j] = matrix[j * n + i];
                    } else {
                        matrix[i * n + j] = 1 + static_cast<long long>(rng() % 1000);
                    }
                }
            }
            auto dist = [&](int a, int b) { return matrix[a * n + b]; };

            for (bool cycle : {false, true}) {
                for (bool fixed_start : {false, true}) {
                    for (bool fixed_end : {false, true}) {
                        if (cycle && fixed_end) continue;
                        std::vector<int> exact_order(n);
                        std::iota(exact_order.begin(), exact_order.end(), 0);
                        const long long optimum = tsp::held_karp_tsp(
                            exact_order, dist, cycle, fixed_start, fixed_end);

                        std::vector<int> order(n);
                        std::iota(order.begin(), order.end(), 0);
                        std::shuffle(order.begin() + (fixed_start ? 1 : 0),
                                     order.end() - ((!cycle && fixed_end) ? 1 : 0), rng);
                        const int start = order.front();
                        const int finish = order.back();
                        const long long before = tsp::total_distance(
                            std::span<const int>(order), dist, cycle);

                        tsp::TSPParam param;
                        param.time_limit_ms = 1000;
                        param.max_move_evaluations = 20000;
                        param.seed = static_cast<std::uint64_t>(tc + 1);
                        param.cycle = cycle;
                        param.fixed_start = fixed_start;
                        param.fixed_end = fixed_end;
                        param.symmetric = symmetric;
                        const long long after = tsp::improve_tsp(order, dist, param);

                        const long long recomputed = tsp::total_distance(
                            std::span<const int>(order), dist, cycle);
                        if (after != recomputed) {
                            std::cerr << "score mismatch: tc=" << tc << " n=" << n
                                      << " symmetric=" << symmetric << " cycle=" << cycle
                                      << " fixed_start=" << fixed_start
                                      << " fixed_end=" << fixed_end << " after=" << after
                                      << " recomputed=" << recomputed << '\n';
                            std::abort();
                        }
                        require_test(after <= before, "improve_tsp worsened input");
                        if (fixed_start) {
                            require_test(order.front() == start, "random fixed start");
                        }
                        if (!cycle && fixed_end) {
                            require_test(order.back() == finish, "random fixed end");
                        }
                        require_test(after >= optimum, "heuristic score below optimum");
                        std::sort(order.begin(), order.end());
                        for (int i = 0; i < n; ++i) {
                            require_test(order[i] == i, "random result permutation");
                        }
                    }
                }
            }
        }
    };

    auto test_edge_cases_and_floating_distance = [&]() {
        auto integer_dist = [](int a, int b) {
            return static_cast<long long>(std::abs(a - b));
        };
        std::vector<int> empty;
        require_test(tsp::solve_tsp(empty, integer_dist) == 0, "empty route");

        std::vector<int> singleton = {123};
        require_test(tsp::solve_tsp(singleton, integer_dist) == 0, "singleton route");
        require_test(singleton.front() == 123, "singleton vertex preservation");

        const std::array<std::pair<double, double>, 6> point = {
            std::pair{0.0, 0.0}, std::pair{2.0, 0.0}, std::pair{3.0, 1.0},
            std::pair{2.0, 3.0}, std::pair{0.0, 2.0}, std::pair{1.0, 1.0},
        };
        auto floating_dist = [&](int a, int b) {
            return std::hypot(point[a].first - point[b].first,
                              point[a].second - point[b].second);
        };
        std::vector<int> order = {0, 1, 2, 3, 4, 5};
        tsp::TSPParam param;
        param.time_limit_ms = 10;
        const auto result = tsp::solve_tsp(order, floating_dist, param);
        static_assert(std::same_as<std::remove_cv_t<decltype(result)>, double>);
        const double recomputed = tsp::total_distance(std::span<const int>(order),
                                                      floating_dist, true);
        require_test(std::abs(result - recomputed) < 1e-9, "floating score");
        std::sort(order.begin(), order.end());
        require_test(order == std::vector<int>({0, 1, 2, 3, 4, 5}),
                     "floating result permutation");
    };

    auto test_search = [&]() {
        constexpr int n = 14;
        std::mt19937_64 rng(998244353);
        for (bool symmetric : {false, true}) {
            std::vector<long long> matrix(n * n);
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    if (i == j) {
                        matrix[i * n + j] = 0;
                    } else if (symmetric && j < i) {
                        matrix[i * n + j] = matrix[j * n + i];
                    } else {
                        matrix[i * n + j] = 1 + static_cast<long long>(rng() % 10'000ULL);
                    }
                }
            }
            auto dist = [&](int a, int b) { return matrix[a * n + b]; };

            for (bool cycle : {false, true}) {
                std::vector<int> order(n);
                std::iota(order.begin(), order.end(), 0);
                const int start = order.front();
                const int finish = order.back();
                tsp::TSPParam param;
                param.time_limit_ms = 1000;
                param.max_move_evaluations = 50'000;
                param.seed = 12345;
                param.cycle = cycle;
                param.fixed_start = true;
                param.fixed_end = !cycle;
                param.symmetric = symmetric;
                const long long result = tsp::solve_tsp(order, dist, param);
                require_test(result == tsp::total_distance(std::span<const int>(order),
                                                           dist, cycle),
                             "search score");
                require_test(order.front() == start, "search fixed start");
                if (!cycle) require_test(order.back() == finish, "search fixed end");
                std::sort(order.begin(), order.end());
                for (int i = 0; i < n; ++i) {
                    require_test(order[i] == i, "search result permutation");
                }
            }
        }
    };

    auto test_noncontiguous_ids = [&]() {
        const std::array<int, 7> id = {41, -3, 99, 7, 1001, 5, 22};
        auto dist = [](int a, int b) {
            const long long difference = static_cast<long long>(a) - static_cast<long long>(b);
            return std::llabs(difference);
        };
        std::vector<int> order(id.begin(), id.end());
        tsp::TSPParam param;
        param.time_limit_ms = 20;
        param.cycle = false;
        param.fixed_start = true;
        param.fixed_end = true;
        param.symmetric = true;
        const int start = order.front();
        const int finish = order.back();
        const long long result = tsp::solve_tsp(order, dist, param);
        require_test(result == tsp::total_distance(std::span<const int>(order), dist, false),
                     "noncontiguous score");
        require_test(order.front() == start, "noncontiguous fixed start");
        require_test(order.back() == finish, "noncontiguous fixed end");
        std::sort(order.begin(), order.end());
        std::vector<int> expected(id.begin(), id.end());
        std::sort(expected.begin(), expected.end());
        require_test(order == expected, "noncontiguous result permutation");
    };
    test_total_and_exact();
    test_random_against_exact();
    test_edge_cases_and_floating_distance();
    test_search();
    test_noncontiguous_ids();
    std::cout << "All tests passed.\n";
}
#endif
