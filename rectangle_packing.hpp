/*
 * rectangle_packing v10: AHC・競技プログラミング向けの直交矩形packing solver。
 *
 * 対応する問題:
 *   - 固定された 1 個のビンへ、必須矩形を優先しつつ利益の高い矩形を詰める
 *   - 幅固定の strip へ全矩形を詰め、使用高さを小さくする
 *   - 同じ大きさの複数ビンへ全矩形を詰め、使用ビン数を小さくする
 *   - 全矩形を囲む外接矩形の面積、周長、重み付き幅高さを小さくする
 *   - 固定ビンの既存解について、指定した矩形だけを動かす部分repair
 *
 * 基本方針:
 *   MaxRects の3評価則、辺索引付きContact Point、複数の大矩形優先順を組み合わせ、
 *   短い multi-start で最良解を選ぶ。空き領域をその場で更新し、同じ切断方向・境界で成立し得る包含だけを
 *   検査する。固定ビンとstripでは、配置候補が少ない矩形を
 *   先に置くfail-first構築も併用し、配置候補数を位置探索と同時に数える。
 *   複数ビンでは末尾ビンのpair-merge repair、
 *   外接矩形では最良幅を固定した高さ圧縮を行う。strip・外接矩形の完成配置は
 *   現在の外接枠内で右・下・左・上へ2巡詰め、幅と高さを増やさず隙間を減らす。
 *   全メソッドは90度単位の回転だけを扱い、座標と寸法は整数とする。
 *
 * 注意:
 *   - 座標・寸法・面積・目的値の演算結果と、配置済みprofitの合計は long long に収める
 *     strip・外接矩形では探索用の幅と高さの積もこの範囲に収める
 *   - strip・複数ビン・外接矩形では全入力をrequired=trueとする
 *   - ヘッダー単体のコンパイルでは末尾の自己テストを実行できる
 *   - 計算量表記の N は矩形数、K は試行構築数、F は最大空き矩形数、B はビン数
 *   - 任意角度回転、多角形、任意形状の障害物、guillotine 切断制約は扱わない
 *     （矩形障害物は部分repairの固定配置として表現できる）
 *   - deadline を指定しない場合も restart_limit / search_iteration_limit で必ず停止する
 */
#pragma once

#include <bits/stdc++.h>

struct rectangle_pack_item {
    // 正の整数寸法。profitは固定ビンpackingでだけ評価される。
    long long width = 0;
    long long height = 0;
    long long profit = 1;
    // rotatableなら90度回転を許す。requiredなら未配置数を最優先で最小化する。
    bool rotatable = true;
    bool required = true;
};

struct rectangle_pack_placement {
    // 左上原点の配置座標と、回転適用後の寸法。
    long long x = 0;
    long long y = 0;
    long long width = 0;
    long long height = 0;
    int bin = -1;  // 未配置は-1。stripと固定ビンは配置時に0。
    bool rotated = false;

    // 矩形が配置済みなら true を返す。O(1)
    bool placed() const { return bin >= 0; }
};

struct rectangle_pack_options;
struct rectangle_bounding_objective;

struct rectangle_pack_result {
    // 入力と同じ順序・要素数。未配置要素も含む。
    std::vector<rectangle_pack_placement> placements;
    // 全ビンを通した最大右端・最大下端。
    long long used_width = 0;
    long long used_height = 0;
    long long placed_profit = 0;
    long long placed_area = 0;
    int placed_count = 0;
    int missing_required_count = 0;
    int bin_count = 0;  // 使用したbin番号の最大値+1。

    // 必須矩形をすべて配置できたなら true を返す。O(1)
    bool feasible() const { return missing_required_count == 0; }

private:
    struct engine;
    friend struct rectangle_packing_test_access;  // 定義はテスト側のみ。
    friend void recompute_rectangle_packing_summary(
        std::span<const rectangle_pack_item> items,
        rectangle_pack_result& result);
    friend rectangle_pack_result pack_rectangles_fixed_bin(
        std::span<const rectangle_pack_item> items,
        long long bin_width,
        long long bin_height,
        const rectangle_pack_options& options);
    friend rectangle_pack_result improve_rectangles_fixed_bin(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& initial_solution,
        long long bin_width,
        long long bin_height,
        const rectangle_pack_options& options);
    friend rectangle_pack_result repack_rectangles_fixed_bin(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& initial_solution,
        std::span<const int> movable_item_ids,
        long long bin_width,
        long long bin_height,
        const rectangle_pack_options& options);
    friend rectangle_pack_result repair_rectangles_fixed_bin(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& initial_solution,
        std::span<const int> movable_item_ids,
        long long bin_width,
        long long bin_height,
        const rectangle_pack_options& options);
    friend rectangle_pack_result pack_rectangles_strip(
        std::span<const rectangle_pack_item> items,
        long long strip_width,
        const rectangle_pack_options& options);
    friend rectangle_pack_result improve_rectangles_strip(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& initial_solution,
        long long strip_width,
        const rectangle_pack_options& options);
    friend rectangle_pack_result pack_rectangles_multiple_bins(
        std::span<const rectangle_pack_item> items,
        long long bin_width,
        long long bin_height,
        const rectangle_pack_options& options);
    friend rectangle_pack_result improve_rectangles_multiple_bins(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& initial_solution,
        long long bin_width,
        long long bin_height,
        const rectangle_pack_options& options);
    friend rectangle_pack_result pack_rectangles_bounding_box(
        std::span<const rectangle_pack_item> items,
        const rectangle_bounding_objective& objective,
        const rectangle_pack_options& options);
    friend rectangle_pack_result improve_rectangles_bounding_box(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& initial_solution,
        const rectangle_bounding_objective& objective,
        const rectangle_pack_options& options);
    friend bool validate_rectangle_packing(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& result,
        long long bin_width,
        long long bin_height,
        bool require_all_items,
        std::string* error_message,
        bool allow_missing_required);
};

struct rectangle_pack_options {
    // max()なら回数制限だけを使う。指定時も初期候補を作ってから停止判定する。
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::time_point::max();
    // deadlineで打ち切られなければ、同じ入力・設定・seedで同じ結果を返す。
    std::uint64_t seed = 0x243f6a8885a308d3ULL;
    // 固定ビンの全再構築・部分再配置の順序multi-start回数。
    // 0なら決定的portfolioだけを使う。
    int restart_limit = 32;
    // strip・複数ビン・外接矩形の追加探索予算。重い構築は複数回分として換算する。
    // 0なら決定的portfolioとstrip・外接矩形の最終隙間詰めを行う。
    int search_iteration_limit = 32;
    // 停止判定を何回呼ぶごとに時計を読むか。判定は1候補・候補群の構築後、または隙間詰めの1方向前。
    int time_check_interval = 1;
};

enum class rectangle_bounding_objective_type {
    area,
    perimeter,
    weighted_sum,
};

struct rectangle_bounding_objective {
    rectangle_bounding_objective_type type = rectangle_bounding_objective_type::area;
    long long width_weight = 1;
    long long height_weight = 1;

    // 幅と高さに対する目的値を返す。O(1)
    long long evaluate(long long width, long long height) const {
        if (type == rectangle_bounding_objective_type::area) return width * height;
        if (type == rectangle_bounding_objective_type::perimeter) return width + height;
        return width_weight * width + height_weight * height;
    }
};

struct rectangle_pack_result::engine {
    struct rng64 {
        std::uint64_t state;

        explicit rng64(std::uint64_t seed) : state(seed ? seed : 0x9e3779b97f4a7c15ULL) {}

        std::uint64_t next() {
            state += 0x9e3779b97f4a7c15ULL;
            std::uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            return z ^ (z >> 31);
        }

        std::size_t index(std::size_t n) {
            assert(n > 0);
            return static_cast<std::size_t>(next() % n);
        }
    };

    struct deadline_checker {
        std::chrono::steady_clock::time_point deadline;
        int interval;
        int counter = 0;
        bool expired_cache = false;

        explicit deadline_checker(const rectangle_pack_options& options)
            : deadline(options.deadline), interval(std::max(1, options.time_check_interval)) {}

        bool expired() {
            if (expired_cache) return true;
            ++counter;
            if (counter < interval) return false;
            counter = 0;
            expired_cache = std::chrono::steady_clock::now() >= deadline;
            return expired_cache;
        }
    };

    struct box {
        long long x;
        long long y;
        long long width;
        long long height;
        long long right() const { return x + width; }
        long long bottom() const { return y + height; }
        bool intersects(const box& b) const {
            return x < b.right() && b.x < right() && y < b.bottom() && b.y < bottom();
        }
    };

    struct candidate {
        box rect{0, 0, 0, 0};
        bool rotated = false;
        bool valid = false;
        long long score1 = 0;
        long long score2 = 0;
        long long score3 = 0;
        long long score4 = 0;

        bool better_than(const candidate& other) const {
            if (!valid) return false;
            if (!other.valid) return true;
            return std::tie(score1, score2, score3, score4,
                            rect.y, rect.x, rotated) <
                   std::tie(other.score1, other.score2, other.score3, other.score4,
                            other.rect.y, other.rect.x, other.rotated);
        }
    };

    struct maxrects_state {
        enum class heuristic {
            best_short_side,
            best_area,
            bottom_left,
        };
        std::vector<box> free_rectangles;

        maxrects_state(long long width, long long height) {
            free_rectangles.reserve(64);
            free_rectangles.push_back({0, 0, width, height});
        }

        candidate find_position(const rectangle_pack_item& item,
                                heuristic rule, int* fit_count = nullptr) const {
            candidate best;

            // 各最大空き矩形について、通常向きと90度回転の両方を評価する
            for (const box& free_rect : free_rectangles) {
                for (int rotation = 0; rotation < 2; ++rotation) {
                    if (rotation == 1 && (!item.rotatable || item.width == item.height)) continue;
                    const long long w = rotation == 0 ? item.width : item.height;
                    const long long h = rotation == 0 ? item.height : item.width;
                    if (w > free_rect.width || h > free_rect.height) continue;

                    if (fit_count) ++*fit_count;
                    const long long leftover_w = free_rect.width - w;
                    const long long leftover_h = free_rect.height - h;
                    candidate current;
                    current.rect = {free_rect.x, free_rect.y, w, h};
                    current.rotated = rotation != 0;
                    current.valid = true;

                    if (rule == heuristic::best_short_side) {
                        current.score1 = std::min(leftover_w, leftover_h);
                        current.score2 = std::max(leftover_w, leftover_h);
                        current.score3 = free_rect.y + h;
                        current.score4 = free_rect.x;
                    } else if (rule == heuristic::best_area) {
                        current.score1 = free_rect.width * free_rect.height - w * h;
                        current.score2 = std::min(leftover_w, leftover_h);
                        current.score3 = free_rect.y + h;
                        current.score4 = free_rect.x;
                    } else {
                        current.score1 = free_rect.y + h;
                        current.score2 = free_rect.x;
                        current.score3 = std::min(leftover_w, leftover_h);
                        current.score4 = std::max(leftover_w, leftover_h);
                    }

                    if (current.better_than(best)) best = current;
                }
            }
            return best;
        }

        void occupy(box placed) {
            const auto contains = [](const box& a, const box& b) {
                return a.x <= b.x && a.y <= b.y && b.right() <= a.right() && b.bottom() <= a.bottom();
            };
            const std::size_t original_count = free_rectangles.size();
            std::size_t unchanged_count = 0;

            // 元の範囲だけを読み、変更のない矩形を前詰めし、断片を末尾へ追加する
            // push_backで再確保されても読み取り元が失われないよう、矩形は値で持つ
            for (std::size_t index = 0; index < original_count; ++index) {
                const box free_rect = free_rectangles[index];
                if (!free_rect.intersects(placed)) {
                    free_rectangles[unchanged_count++] = free_rect;
                    continue;
                }

                if (placed.x > free_rect.x)
                    free_rectangles.push_back({free_rect.x, free_rect.y,
                                    placed.x - free_rect.x, free_rect.height});
                if (placed.right() < free_rect.right())
                    free_rectangles.push_back({placed.right(), free_rect.y,
                                    free_rect.right() - placed.right(), free_rect.height});
                if (placed.y > free_rect.y)
                    free_rectangles.push_back({free_rect.x, free_rect.y,
                                    free_rect.width, placed.y - free_rect.y});
                if (placed.bottom() < free_rect.bottom())
                    free_rectangles.push_back({free_rect.x, placed.bottom(),
                                    free_rect.width, free_rect.bottom() - placed.bottom()});
            }

            // 元の交差矩形の残骸だけを除き、非交差矩形と新しい断片を連続させる
            free_rectangles.erase(free_rectangles.begin() + static_cast<std::ptrdiff_t>(unchanged_count),
                                  free_rectangles.begin() + static_cast<std::ptrdiff_t>(original_count));
            auto& next = free_rectangles;

            // 断片を切断方向で分ける。異なる方向の断片同士は包含しない。
            const auto direction = [&](const box& b) {
                if (b.right() == placed.x) return 0;
                if (b.x == placed.right()) return 1;
                if (b.bottom() == placed.y) return 2;
                return 3;
            };
            std::array<std::size_t, 5> boundaries{};
            boundaries[0] = unchanged_count;
            for (int d = 0; d < 3; ++d) {
                const auto middle = std::partition(
                    next.begin() + static_cast<std::ptrdiff_t>(boundaries[d]), next.end(),
                    [&](const box& b) { return direction(b) == d; });
                boundaries[d + 1] = static_cast<std::size_t>(middle - next.begin());
            }
            boundaries[4] = next.size();
            std::size_t write = unchanged_count;
            // 左・右・上・下を同じ順に処理し、境界選択だけをコンパイル時に確定する。
            const auto prune_direction = [&]<int d>() {
                const std::size_t first = boundaries[d];
                std::size_t last = boundaries[d + 1];
                if (first == last) return;

                // 既存矩形が断片を含むには、その切断境界が一致する必要がある。
                for (std::size_t i = 0; i < unchanged_count; ++i) {
                    const box& f = next[i];
                    const bool border = d == 0 ? f.right() == placed.x
                        : d == 1 ? f.x == placed.right()
                        : d == 2 ? f.bottom() == placed.y : f.y == placed.bottom();
                    if (!border) continue;
                    const auto end = std::remove_if(
                        next.begin() + static_cast<std::ptrdiff_t>(first),
                        next.begin() + static_cast<std::ptrdiff_t>(last),
                        [&](const box& g) { return contains(f, g); });
                    last = static_cast<std::size_t>(end - next.begin());
                }

                // 同じ方向の断片だけで包含を調べ、削除は範囲末尾との入替で行う。
                for (std::size_t i = first; i < last;) {
                    bool erased = false;
                    for (std::size_t j = i + 1; j < last;) {
                        if (contains(next[i], next[j])) next[j] = next[--last];
                        else if (contains(next[j], next[i])) {
                            next[i] = next[--last];
                            erased = true;
                            break;
                        } else ++j;
                    }
                    if (!erased) ++i;
                }
                // 次の群の元の範囲を壊さないよう、確定した要素だけを前詰めする。
                for (std::size_t i = first; i < last; ++i) next[write++] = next[i];
            };
            prune_direction.template operator()<0>();
            prune_direction.template operator()<1>();
            prune_direction.template operator()<2>();
            prune_direction.template operator()<3>();
            next.resize(write);
        }
    };

    struct contact_point_state {
        struct edge_index {
            struct slot {
                long long coordinate = 0;
                int head = -1;
                unsigned char type = 0;
                bool used = false;
            };

            struct edge {
                long long begin;
                long long end;
                int next;
            };

            struct slot_change {
                std::size_t index;
                int old_head;
            };

            std::vector<slot> slots;
            std::vector<edge> edges;
            std::size_t mask = 0;
            std::size_t bucket_count = 0;

            explicit edge_index(std::size_t expected_item_count) {
                std::size_t capacity = 8;
                // 1矩形から4種類の辺が増える。load factorを最大1/2にする。
                while (capacity < expected_item_count * 4 + 1) capacity *= 2;
                slots.resize(capacity);
                edges.reserve(expected_item_count * 4);
                mask = capacity - 1;
            }

            std::size_t find_slot(long long coordinate, unsigned char type) const {
                std::uint64_t value = static_cast<std::uint64_t>(coordinate) ^
                    (0x9e3779b97f4a7c15ULL * (static_cast<std::uint64_t>(type) + 1));
                value ^= value >> 32;
                value *= 0x9e3779b97f4a7c15ULL;

                std::size_t index = static_cast<std::size_t>(value) & mask;
                while (slots[index].used &&
                       (slots[index].coordinate != coordinate || slots[index].type != type))
                    index = (index + 1) & mask;
                return index;
            }

            void grow() {
                std::vector<slot> old = std::move(slots);
                slots.assign(old.size() * 2, {});
                mask = slots.size() - 1;
                for (const slot& old_slot : old) {
                    if (!old_slot.used) continue;
                    const std::size_t index = find_slot(old_slot.coordinate, old_slot.type);
                    slots[index] = old_slot;
                }
            }

        };

        long long width;
        long long height;
        maxrects_state space;
        edge_index contacts;

        contact_point_state(long long width_, long long height_, std::size_t expected_items = 32)
            : width(width_), height(height_), space(width_, height_),
              contacts(expected_items) {}

        contact_point_state(long long width_, long long height_,
                            const maxrects_state& initial_space,
                            std::size_t expected_items)
            : width(width_), height(height_), space(initial_space),
              contacts(expected_items) {}

        candidate find_position(const rectangle_pack_item& item) const {
            const auto overlap_sum = [&](long long coordinate, unsigned char type,
                                         long long begin, long long end) -> long long {
                const std::size_t index = contacts.find_slot(coordinate, type);
                if (!contacts.slots[index].used) return 0;
                long long result = 0;
                for (int id = contacts.slots[index].head; id >= 0; id = contacts.edges[id].next) {
                    const edge_index::edge& current = contacts.edges[id];
                    result += std::max(0LL, std::min(end, current.end) -
                                              std::max(begin, current.begin));
                }
                return result;
            };
            const auto contact_score = [&](const box& current) {
                long long score = 0;
                const long long current_right = current.x + current.width;
                const long long current_bottom = current.y + current.height;
                if (current.x == 0 || current_right == width) score += current.height;
                if (current.y == 0 || current_bottom == height) score += current.width;

                // 種別: 0=左辺、1=右辺、2=上辺、3=下辺。
                score += overlap_sum(current.x, 1, current.y, current_bottom);
                score += overlap_sum(current_right, 0, current.y, current_bottom);
                score += overlap_sum(current.y, 3, current.x, current_right);
                score += overlap_sum(current_bottom, 2, current.x, current_right);
                return score;
            };
            candidate best;

            // 4隅を試し、接触辺長、空き面積、短辺残差の順に比較する
            for (const box& free_rect : space.free_rectangles) {
                for (int rotation = 0; rotation < 2; ++rotation) {
                    if (rotation == 1 && (!item.rotatable || item.width == item.height)) continue;
                    const long long w = rotation == 0 ? item.width : item.height;
                    const long long h = rotation == 0 ? item.height : item.width;
                    if (w > free_rect.width || h > free_rect.height) continue;
                    const std::array<long long, 4> xs{
                        free_rect.x, free_rect.x + free_rect.width - w,
                        free_rect.x, free_rect.x + free_rect.width - w};
                    const std::array<long long, 4> ys{
                        free_rect.y, free_rect.y,
                        free_rect.y + free_rect.height - h,
                        free_rect.y + free_rect.height - h};

                    for (int position = 0; position < 4; ++position) {
                        if ((position & 1) != 0 && w == free_rect.width) continue;
                        if (position >= 2 && h == free_rect.height) continue;
                        candidate current;
                        current.rect = {xs[position], ys[position], w, h};
                        current.rotated = rotation != 0;
                        current.valid = true;
                        current.score1 = -contact_score(current.rect);
                        current.score2 = free_rect.width * free_rect.height - w * h;
                        current.score3 = std::min(free_rect.width - w, free_rect.height - h);
                        current.score4 = current.rect.y + h;
                        if (current.better_than(best)) best = current;
                    }
                }
            }
            return best;
        }

        template<bool Logged = false>
        void add_contacts(const box& placed, std::vector<edge_index::slot_change>* changes = nullptr) {
            const auto add = [&](long long coordinate, unsigned char type,
                                 long long begin, long long end) {
                if constexpr (!Logged) (void)changes;
                std::size_t index = contacts.find_slot(coordinate, type);
                if (!contacts.slots[index].used) {
                    // Loggedでは呼出側が容量を確保済み。rehashはログの添字を壊すため行わない。
                    if constexpr (Logged) {
                        assert(changes && (contacts.bucket_count + 1) * 2 <= contacts.slots.size());
                    } else if ((contacts.bucket_count + 1) * 2 > contacts.slots.size()) {
                        contacts.grow();
                        index = contacts.find_slot(coordinate, type);
                    }
                    contacts.slots[index].used = true;
                    contacts.slots[index].coordinate = coordinate;
                    contacts.slots[index].type = type;
                    ++contacts.bucket_count;
                }
                if constexpr (Logged) changes->push_back({index, contacts.slots[index].head});
                contacts.edges.push_back({begin, end, contacts.slots[index].head});
                contacts.slots[index].head = static_cast<int>(contacts.edges.size()) - 1;
            };
            const long long placed_right = placed.right();
            const long long placed_bottom = placed.bottom();
            add(placed.x, 0, placed.y, placed_bottom);
            add(placed_right, 1, placed.y, placed_bottom);
            add(placed.y, 2, placed.x, placed_right);
            add(placed_bottom, 3, placed.x, placed_right);
        }

        void occupy(const box& placed) {
            space.occupy(placed);
            add_contacts(placed);
        }
    };

    static rectangle_pack_result empty_result(std::size_t n) {
        rectangle_pack_result result;
        result.placements.resize(n);
        return result;
    }

    static void summarize(std::span<const rectangle_pack_item> items,
                          rectangle_pack_result& result,
                          std::span<const rectangle_pack_placement> placements) {
        result.used_width = 0;
        result.used_height = 0;
        result.placed_profit = 0;
        result.placed_area = 0;
        result.placed_count = 0;
        result.missing_required_count = 0;
        result.bin_count = 0;
        // 正負のprofitが相殺する場合も、途中の合計を溢れさせない
        __int128_t profit = 0;

        for (std::size_t i = 0; i < items.size(); ++i) {
            const rectangle_pack_placement& p = placements[i];
            if (!p.placed()) {
                if (items[i].required) ++result.missing_required_count;
                continue;
            }
            result.used_width = std::max(result.used_width, p.x + p.width);
            result.used_height = std::max(result.used_height, p.y + p.height);
            profit += items[i].profit;
            result.placed_area += p.width * p.height;
            ++result.placed_count;
            result.bin_count = std::max(result.bin_count, p.bin + 1);
        }
        assert(std::numeric_limits<long long>::min() <= profit &&
               profit <= std::numeric_limits<long long>::max());
        result.placed_profit = static_cast<long long>(profit);
    }

    static bool better_fixed(const rectangle_pack_result& lhs,
                             const rectangle_pack_result& rhs) {
        if (rhs.placements.empty()) return true;
        // 必須欠落数を最小化し、利益と面積は符号を反転せず降順に比較する
        if (lhs.missing_required_count != rhs.missing_required_count)
            return lhs.missing_required_count < rhs.missing_required_count;
        if (lhs.placed_profit != rhs.placed_profit) return lhs.placed_profit > rhs.placed_profit;
        if (lhs.placed_area != rhs.placed_area) return lhs.placed_area > rhs.placed_area;
        return std::tuple(lhs.used_width * lhs.used_height, lhs.used_height, lhs.used_width) <
               std::tuple(rhs.used_width * rhs.used_height, rhs.used_height, rhs.used_width);
    }

    static bool better_strip(const rectangle_pack_result& lhs,
                             const rectangle_pack_result& rhs) {
        if (rhs.placements.empty()) return true;
        return std::tuple(lhs.missing_required_count, lhs.used_height, lhs.used_width) <
               std::tuple(rhs.missing_required_count, rhs.used_height, rhs.used_width);
    }

    static bool better_multiple_bins(const rectangle_pack_result& lhs,
                                     const rectangle_pack_result& rhs) {
        const auto last_bin_area = [](const rectangle_pack_result& result) -> long long {
            if (result.bin_count == 0) return 0;
            long long area = 0;
            for (const rectangle_pack_placement& p : result.placements)
                if (p.bin == result.bin_count - 1) area += p.width * p.height;
            return area;
        };
        if (rhs.placements.empty()) return true;
        return std::tuple(lhs.missing_required_count, lhs.bin_count, last_bin_area(lhs)) <
               std::tuple(rhs.missing_required_count, rhs.bin_count, last_bin_area(rhs));
    }

    template<class State, class... Heuristic>
    static rectangle_pack_result decode_from_state(
        std::span<const rectangle_pack_item> items,
        std::span<const int> order,
        rectangle_pack_result result,
        State state,
        Heuristic... heuristic) {
        for (int id : order) {
            const candidate position = state.find_position(items[id], heuristic...);
            if (!position.valid) continue;
            state.occupy(position.rect);
            result.placements[id] = {position.rect.x, position.rect.y,
                                     position.rect.width, position.rect.height,
                                     0, position.rotated};
        }
        summarize(items, result, result.placements);
        return result;
    }

    static rectangle_pack_result decode_maxrects(
        std::span<const rectangle_pack_item> items,
        std::span<const int> order,
        long long width,
        long long height,
        maxrects_state::heuristic rule) {
        return decode_from_state(
            items, order, empty_result(items.size()),
            maxrects_state(width, height), rule);
    }

    static rectangle_pack_result decode_contact_point(
        std::span<const rectangle_pack_item> items,
        std::span<const int> order,
        long long width,
        long long height) {
        return decode_from_state(
            items, order, empty_result(items.size()),
            contact_point_state(width, height, items.size()));
    }

    static rectangle_pack_result decode_maxrects_constrained_from_state(
        std::span<const rectangle_pack_item> items,
        std::span<const int> initial_order,
        rectangle_pack_result result,
        maxrects_state state,
        std::size_t window) {
        std::vector<int> remaining(initial_order.begin(), initial_order.end());

        // 先頭window内で配置可能位置が少ない矩形を優先するfail-first順序を動的に作る
        while (!remaining.empty()) {
            candidate best_position;
            std::size_t best_index = 0;
            int best_fit_count = std::numeric_limits<int>::max();
            const std::size_t limit = std::min(window, remaining.size());
            for (std::size_t i = 0; i < limit; ++i) {
                const int id = remaining[i];
                int fit_count = 0;
                candidate position = state.find_position(
                    items[id], maxrects_state::heuristic::best_short_side, &fit_count);
                if (!position.valid) continue;
                if (fit_count < best_fit_count ||
                    (fit_count == best_fit_count && position.better_than(best_position))) {
                    best_position = position;
                    best_index = i;
                    best_fit_count = fit_count;
                }
            }

            // window内が全滅した場合だけ、残りから最初の配置可能矩形を救済する
            if (!best_position.valid) {
                for (std::size_t i = limit; i < remaining.size(); ++i) {
                    candidate position = state.find_position(
                        items[remaining[i]], maxrects_state::heuristic::best_short_side);
                    if (!position.valid) continue;
                    best_position = position;
                    best_index = i;
                    break;
                }
            }
            if (!best_position.valid) break;

            const int id = remaining[best_index];
            state.occupy(best_position.rect);
            result.placements[id] = {best_position.rect.x, best_position.rect.y,
                                     best_position.rect.width, best_position.rect.height,
                                     0, best_position.rotated};
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(best_index));
        }
        summarize(items, result, result.placements);
        return result;
    }

    static rectangle_pack_result decode_maxrects_constrained(
        std::span<const rectangle_pack_item> items,
        std::span<const int> initial_order,
        long long width,
        long long height,
        std::size_t window) {
        return decode_maxrects_constrained_from_state(
            items, initial_order, empty_result(items.size()),
            maxrects_state(width, height), window);
    }

    enum class order_kind {
        area,
        long_side,
        perimeter,
        height,
        width,
        profit,
        profit_density,
    };

    static std::vector<int> make_order(std::span<const rectangle_pack_item> items,
                                       order_kind kind) {
        std::vector<int> order(items.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
            const rectangle_pack_item& a = items[lhs];
            const rectangle_pack_item& b = items[rhs];
            const long long area_a = a.width * a.height;
            const long long area_b = b.width * b.height;
            if (a.required != b.required) return a.required > b.required;
            if (kind == order_kind::long_side) {
                const long long key_a = std::max(a.width, a.height);
                const long long key_b = std::max(b.width, b.height);
                if (key_a != key_b) return key_a > key_b;
            }
            if (kind == order_kind::perimeter) {
                const long long key_a = a.width + a.height;
                const long long key_b = b.width + b.height;
                if (key_a != key_b) return key_a > key_b;
            }
            if (kind == order_kind::height && a.height != b.height) return a.height > b.height;
            if (kind == order_kind::width && a.width != b.width) return a.width > b.width;
            if (kind == order_kind::profit && a.profit != b.profit) return a.profit > b.profit;
            if (kind == order_kind::profit_density) {
                const __int128_t left = static_cast<__int128_t>(a.profit) * area_b;
                const __int128_t right_value = static_cast<__int128_t>(b.profit) * area_a;
                if (left != right_value) return left > right_value;
            }
            if (area_a != area_b) return area_a > area_b;
            return lhs < rhs;
        });
        return order;
    }

    static std::vector<std::vector<int>> make_base_orders(
        std::span<const rectangle_pack_item> items,
        bool include_profit) {
        const std::array<order_kind, 7> kinds{
            order_kind::long_side, order_kind::height, order_kind::perimeter,
            order_kind::area, order_kind::width, order_kind::profit_density,
            order_kind::profit};
        const int count = include_profit ? 7 : 5;
        std::vector<std::vector<int>> orders;
        orders.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            std::vector<int> order = make_order(items, kinds[i]);
            if (std::none_of(orders.begin(), orders.end(),
                             [&](const std::vector<int>& old) { return old == order; }))
                orders.push_back(std::move(order));
        }
        return orders;
    }

    static void perturb_order(std::vector<int>& order, rng64& rng, int strength) {
        if (order.size() < 2) return;
        const std::size_t n = order.size();
        const std::size_t window = std::max<std::size_t>(2, std::min<std::size_t>(
            n, static_cast<std::size_t>(std::sqrt(static_cast<long double>(n))) * 2));

        // 大矩形優先を大きく壊さないよう、近い順位を中心にswapまたはremove-insertする
        for (int move = 0; move < strength; ++move) {
            const std::size_t a = rng.index(n);
            const std::size_t low = a > window ? a - window : 0;
            const std::size_t high = std::min(n, a + window + 1);
            const std::size_t b = low + rng.index(high - low);
            if ((rng.next() & 1U) == 0U) {
                std::swap(order[a], order[b]);
            } else if (a != b) {
                const int value = order[a];
                order.erase(order.begin() + static_cast<std::ptrdiff_t>(a));
                order.insert(order.begin() + static_cast<std::ptrdiff_t>(b), value);
            }
        }
    }

    static int scaled_budget_ceil(int budget, int numerator, int denominator) {
        assert(budget >= 0 && 0 <= numerator && numerator <= denominator && denominator > 0);
        return budget / denominator * numerator +
               (budget % denominator * numerator + denominator - 1) / denominator;
    }

    static long long safe_unbounded_height(std::span<const rectangle_pack_item> items) {
        long long height = 0;
        for (const rectangle_pack_item& item : items) {
            assert(item.width > 0 && item.height > 0);
            assert(height <= std::numeric_limits<long long>::max() - item.width - item.height);
            height += item.width + item.height;
        }
        return std::max(1LL, height);
    }

    static long long strip_height_lower_bound(
        std::span<const rectangle_pack_item> items,
        long long width) {
        long long total_area = 0;
        long long item_bound = 0;
        for (const rectangle_pack_item& item : items) {
            total_area += item.width * item.height;
            long long height = std::numeric_limits<long long>::max();
            if (item.width <= width) height = item.height;
            if (item.rotatable && item.height <= width)
                height = std::min(height, item.width);
            if (height == std::numeric_limits<long long>::max()) return height;
            item_bound = std::max(item_bound, height);
        }
        return std::max(item_bound, total_area / width + (total_area % width != 0));
    }

    // 完成したstrip・外接配置を、現在の外接枠内で右・下・左・上へ2巡詰める。
    // 寸法・向き・所属ビン・配置集合は変えず、使用幅と使用高さを増やさない。
    // 各軸の移動後も有効な配置なので、期限による途中終了でもこの性質を保つ。
    static rectangle_pack_result compact_result(
        std::span<const rectangle_pack_item> items,
        rectangle_pack_result result,
        const rectangle_pack_options& options = {}) {
        std::vector<int> ids;
        for (std::size_t i = 0; i < result.placements.size(); ++i)
            if (result.placements[i].placed()) ids.push_back(static_cast<int>(i));
        deadline_checker timer(options);
        for (int round = 0; round < 2; ++round) {
            const long long width = result.used_width, height = result.used_height;
            for (int step = 0; step < 4; ++step) {
                if (timer.expired()) {
                    summarize(items, result, result.placements);
                    return result;
                }
                const bool horizontal = step % 2 == 0;
                const bool positive = step < 2;
                std::sort(ids.begin(), ids.end(), [&](int a, int b) {
                    const auto& x = result.placements[a];
                    const auto& y = result.placements[b];
                    const auto ka = horizontal ? x.x : x.y;
                    const auto kb = horizontal ? y.x : y.y;
                    return positive ? ka > kb : ka < kb;
                });
                for (std::size_t index = 0; index < ids.size(); ++index) {
                    const int id = ids[index];
                    auto& a = result.placements[id];
                    long long bound = positive ? (horizontal ? width - a.width : height - a.height) : 0;
                    for (std::size_t previous = 0; previous < index; ++previous) {
                        const int other = ids[previous];
                        const auto& b = result.placements[other];
                        if (a.bin != b.bin) continue;
                        if (horizontal && a.y < b.y + b.height && b.y < a.y + a.height) {
                            if (positive) bound = std::min(bound, b.x - a.width);
                            else bound = std::max(bound, b.x + b.width);
                        } else if (!horizontal && a.x < b.x + b.width && b.x < a.x + a.width) {
                            if (positive) bound = std::min(bound, b.y - a.height);
                            else bound = std::max(bound, b.y + b.height);
                        }
                    }
                    if (horizontal) a.x = bound;
                    else a.y = bound;
                }
            }
            summarize(items, result, result.placements);
        }
        return result;
    }

    static rectangle_pack_result construct_fixed(
        std::span<const rectangle_pack_item> items,
        long long width,
        long long height,
        const rectangle_pack_options& options,
        const rectangle_pack_result* initial = nullptr) {
        if (items.empty()) return empty_result(0);
        rectangle_pack_result best;
        if (initial) best = *initial;
        const bool has_optional = std::any_of(items.begin(), items.end(),
                                              [](const rectangle_pack_item& item) {
                                                  return !item.required || item.profit != 1;
                                              });
        std::vector<std::vector<int>> orders = make_base_orders(items, has_optional);
        for (std::vector<int>& order : orders) {
            std::erase_if(order, [&](int id) {
                return !items[id].required && items[id].profit < 0;
            });
        }
        deadline_checker timer(options);
        rng64 rng(options.seed);

        // 相補的な決定的構築法を全基本順序で試す
        for (const std::vector<int>& order : orders) {
            const std::array<rectangle_pack_result, 3> candidates{
                decode_maxrects(items, order, width, height,
                                maxrects_state::heuristic::best_short_side),
                decode_maxrects(items, order, width, height,
                                maxrects_state::heuristic::best_area),
                decode_maxrects(items, order, width, height,
                                maxrects_state::heuristic::bottom_left)};
            for (const rectangle_pack_result& result : candidates)
                if (better_fixed(result, best)) best = result;
            if (best.feasible() && best.placed_count == static_cast<int>(items.size())) break;
            if (timer.expired()) return best;
        }

        // 通常構築で失敗した場合、配置候補が少ない矩形を優先する動的順序を試す
        const int restarts = std::max(0, options.restart_limit);
        if (!best.feasible() && restarts > 0) {
            std::vector<int> order = make_order(items, order_kind::area);
            std::erase_if(order, [&](int id) {
                return !items[id].required && items[id].profit < 0;
            });
            rectangle_pack_result result = decode_maxrects_constrained(
                items, order, width, height, 64);
            if (better_fixed(result, best)) best = std::move(result);
            if (!best.feasible() && timer.expired()) return best;
        }

        // BSSFと4隅Contact Pointを交互に使う順序multi-startを行う
        for (int iteration = 0; iteration < restarts; ++iteration) {
            std::vector<int> order = orders[rng.index(orders.size())];
            perturb_order(order, rng, 1 + iteration % 5);
            const bool use_contact = (iteration & 1) != 0;
            rectangle_pack_result result = use_contact
                ? decode_contact_point(items, order, width, height)
                : decode_maxrects(items, order, width, height,
                                  maxrects_state::heuristic::best_short_side);
            if (better_fixed(result, best)) best = std::move(result);
            if (best.feasible() && best.placed_count == static_cast<int>(items.size())) break;
            if (timer.expired()) break;
        }
        return best;
    }

    static rectangle_pack_result construct_fixed_repair(
        std::span<const rectangle_pack_item> items,
        const rectangle_pack_result& initial,
        std::span<const int> movable_item_ids,
        long long width,
        long long height,
        const rectangle_pack_options& options,
        bool keep_initial) {
        if (movable_item_ids.empty()) return initial;

        std::vector<unsigned char> movable(items.size(), 0);
        for (int id : movable_item_ids) {
            assert(0 <= id && static_cast<std::size_t>(id) < items.size());
            assert(!movable[id]);
            movable[id] = 1;
        }
        rectangle_pack_result base = initial;
        for (std::size_t i = 0; i < movable.size(); ++i)
            if (movable[i]) base.placements[i] = {};
        summarize(items, base, base.placements);
        rectangle_pack_result best = keep_initial ? initial : base;

        const bool has_optional = std::any_of(
            movable_item_ids.begin(), movable_item_ids.end(), [&](int id) {
                return !items[id].required || items[id].profit != 1;
            });
        std::vector<std::vector<int>> orders = make_base_orders(items, has_optional);
        for (std::vector<int>& order : orders) {
            std::erase_if(order, [&](int id) {
                return !movable[id] || (!items[id].required && items[id].profit < 0);
            });
        }
        deadline_checker timer(options);
        rng64 rng(options.seed);
        maxrects_state locked_space(width, height);
        {
            std::vector<const rectangle_pack_placement*> locked;
            locked.reserve(base.placements.size());
            for (const rectangle_pack_placement& placement : base.placements)
                if (placement.placed()) locked.push_back(&placement);
            std::sort(locked.begin(), locked.end(), [](const auto* lhs, const auto* rhs) {
                return lhs->width * lhs->height > rhs->width * rhs->height;
            });
            for (const rectangle_pack_placement* pointer : locked) {
                const rectangle_pack_placement& placement = *pointer;
                assert(placement.bin == 0);
                locked_space.occupy({placement.x, placement.y,
                              placement.width, placement.height});
            }
        }
        for (const std::vector<int>& order : orders) {
            const std::array<rectangle_pack_result, 3> candidates{
                decode_from_state(items, order, base, locked_space,
                                  maxrects_state::heuristic::best_short_side),
                decode_from_state(items, order, base, locked_space,
                                  maxrects_state::heuristic::best_area),
                decode_from_state(items, order, base, locked_space,
                                  maxrects_state::heuristic::bottom_left)};
            for (const rectangle_pack_result& result : candidates)
                if (better_fixed(result, best)) best = result;
            if (best.feasible() && best.placed_count == static_cast<int>(items.size())) break;
            if (timer.expired()) return best;
        }

        const int restarts = std::max(0, options.restart_limit);
        if (!best.feasible() && restarts > 0) {
            std::vector<int> order = make_order(items, order_kind::area);
            std::erase_if(order, [&](int id) {
                return !movable[id] || (!items[id].required && items[id].profit < 0);
            });
            rectangle_pack_result result = decode_maxrects_constrained_from_state(
                items, order, base, locked_space,
                64);
            if (better_fixed(result, best)) best = std::move(result);
            if (!best.feasible() && timer.expired()) return best;
        }

        std::optional<contact_point_state> locked_contact;
        std::vector<contact_point_state::edge_index::slot_change> changes;
        for (int iteration = 0; iteration < restarts; ++iteration) {
            std::vector<int> order = orders[rng.index(orders.size())];
            perturb_order(order, rng, 1 + iteration % 5);
            rectangle_pack_result result;
            if ((iteration & 1) != 0) {
                if (!locked_contact) {
                    locked_contact.emplace(
                        width, height, locked_space, items.size());
                    for (const rectangle_pack_placement& placement : base.placements) {
                        if (!placement.placed()) continue;
                        locked_contact->add_contacts({
                            placement.x, placement.y,
                            placement.width, placement.height});
                    }
                }
                result = base;
                auto& state = *locked_contact;
                // 固定状態は使い回し、今回追加した空き領域・辺だけを呼出し後に戻す。
                maxrects_state original_space = state.space;
                const std::size_t edge_count = state.contacts.edges.size();
                const std::size_t bucket_count = state.contacts.bucket_count;
                auto& contacts = state.contacts;
                const std::size_t maximum_new_edges = order.size() * 4;
                while ((contacts.bucket_count + maximum_new_edges) * 2 > contacts.slots.size()) contacts.grow();
                contacts.edges.reserve(contacts.edges.size() + maximum_new_edges);
                changes.clear();
                changes.reserve(maximum_new_edges);

                for (int id : order) {
                    const candidate position = state.find_position(items[id]);
                    if (!position.valid) continue;
                    state.space.occupy(position.rect);
                    state.add_contacts<true>(position.rect, &changes);
                    result.placements[id] = {position.rect.x, position.rect.y,
                                             position.rect.width, position.rect.height,
                                             0, position.rotated};
                }
                summarize(items, result, result.placements);
                state.space = std::move(original_space);
                for (auto iterator = changes.rbegin(); iterator != changes.rend(); ++iterator) {
                    auto& changed = contacts.slots[iterator->index];
                    changed.head = iterator->old_head;
                    changed.used = iterator->old_head >= 0;
                }
                contacts.edges.resize(edge_count);
                contacts.bucket_count = bucket_count;
            } else {
                result = decode_from_state(
                    items, order, base, locked_space,
                    maxrects_state::heuristic::best_short_side);
            }
            if (better_fixed(result, best)) best = std::move(result);
            if (best.feasible() && best.placed_count == static_cast<int>(items.size())) break;
            if (timer.expired()) break;
        }
        return best;
    }

    static rectangle_pack_result construct_strip(
        std::span<const rectangle_pack_item> items,
        long long width,
        const rectangle_pack_options& options) {
        if (items.empty()) return empty_result(0);
        rectangle_pack_result best;
        std::vector<std::vector<int>> orders = make_base_orders(items, false);
        const long long unbounded_height = safe_unbounded_height(items);
        deadline_checker timer(options);
        rng64 rng(options.seed);

        // 高さ上限のないMaxRects Bottom-Leftで初期上界を求める
        for (const std::vector<int>& order : orders) {
            rectangle_pack_result result = decode_maxrects(
                items, order, width, unbounded_height, maxrects_state::heuristic::bottom_left);
            if (better_strip(result, best)) best = std::move(result);
            if (timer.expired()) return compact_result(items, std::move(best), options);
        }

        if (!best.feasible() || best.placed_count != static_cast<int>(items.size())) return compact_result(items, std::move(best), options);

        const long long lower_bound = strip_height_lower_bound(items, width);
        if (lower_bound == std::numeric_limits<long long>::max()) return compact_result(items, std::move(best), options);

        // Contact Pointは通常BSSFより重いため、探索予算の3/4回を実際の構築回数とする。
        // 前半はfail-first BSSF、後半はContact Pointで相補的に現在上界未満を狙う。
        const int search_budget = std::max(0, options.search_iteration_limit);
        const int iterations = search_budget - search_budget / 4;
        for (int iteration = 0; iteration < iterations; ++iteration) {
            std::vector<int> order = orders[rng.index(orders.size())];
            perturb_order(order, rng, 1 + iteration % 7);
            const long long gap = best.used_height - lower_bound;
            (void)rng.next();  // 次の順序摂動へ進む前に乱数を1個消費する
            const long long divisor = iterations <= 1
                ? 2 : 2 + 6LL * iteration / (iterations - 1);
            const long long target = std::max(
                lower_bound, best.used_height - std::max(1LL, gap / divisor));
            rectangle_pack_result result;
            if (iteration < iterations / 2) {
                result = decode_maxrects_constrained(
                    items, order, width, target, 16);
            } else {
                result = decode_contact_point(items, order, width, target);
            }
            if (result.placed_count == static_cast<int>(items.size()) &&
                better_strip(result, best))
                best = std::move(result);
            if (best.used_height == lower_bound || timer.expired()) break;
        }
        return compact_result(items, std::move(best), options);
    }

    static rectangle_pack_result construct_multiple_bins(
        std::span<const rectangle_pack_item> items,
        long long width,
        long long height,
        const rectangle_pack_options& options) {
        if (items.empty()) return empty_result(0);
        const auto decode = [&]<class State, class... Heuristic>(
            std::span<const int> order, Heuristic... heuristic) {
            rectangle_pack_result result = empty_result(items.size());
            std::vector<State> bins;

            // 状態型に応じて評価し、既存全ビンの最良位置を選ぶ
            for (int id : order) {
                candidate best_position;
                int best_bin = -1;
                for (std::size_t bin = 0; bin < bins.size(); ++bin) {
                    candidate current = bins[bin].find_position(items[id], heuristic...);
                    if (current.better_than(best_position)) {
                        best_position = current;
                        best_bin = static_cast<int>(bin);
                    }
                }
                if (!best_position.valid) {
                    bins.emplace_back(width, height);
                    best_bin = static_cast<int>(bins.size()) - 1;
                    best_position = bins.back().find_position(items[id], heuristic...);
                }
                if (!best_position.valid) continue;
                bins[best_bin].occupy(best_position.rect);
                result.placements[id] = {best_position.rect.x, best_position.rect.y,
                                         best_position.rect.width, best_position.rect.height,
                                         best_bin, best_position.rotated};
            }
            summarize(items, result, result.placements);
            return result;
    
        };
        rectangle_pack_result best;
        std::vector<std::vector<int>> orders = make_base_orders(items, false);
        deadline_checker timer(options);
        rng64 rng(options.seed);

        for (const std::vector<int>& order : orders) {
            rectangle_pack_result a = decode.template operator()<maxrects_state>(
                order, maxrects_state::heuristic::best_short_side);
            rectangle_pack_result b = decode.template operator()<maxrects_state>(
                order, maxrects_state::heuristic::best_area);
            if (better_multiple_bins(a, best)) best = std::move(a);
            if (better_multiple_bins(b, best)) best = std::move(b);
            if (timer.expired()) return best;
        }

        const int search_budget = std::max(0, options.search_iteration_limit);
        // Contact Pointは1回が重い一方で改善率が高いため、予算の5/8回を構築する。
        const int iterations = scaled_budget_ceil(search_budget, 5, 8);
        for (int iteration = 0; iteration < iterations; ++iteration) {
            std::vector<int> order = orders[rng.index(orders.size())];
            perturb_order(order, rng, 1 + iteration % 7);
            rectangle_pack_result result = (iteration & 1) != 0
                ? decode.template operator()<contact_point_state>(order)
                : decode.template operator()<maxrects_state>(order,
                                       maxrects_state::heuristic::best_short_side);
            if (better_multiple_bins(result, best)) best = std::move(result);
            if (timer.expired()) break;
        }
        if (search_budget == 0 || timer.expired()) return best;
        const int repair_restarts = std::min(8, search_budget);
        if (best.placed_count != static_cast<int>(items.size())) return best;
        const long long bin_area = width * height;
        deadline_checker repair_timer(options);

        for (;;) {
            if (best.bin_count <= 1) break;
            const int last_bin = best.bin_count - 1;
            std::vector<std::vector<int>> ids(static_cast<std::size_t>(best.bin_count));
            std::vector<long long> areas(static_cast<std::size_t>(best.bin_count), 0);
            for (std::size_t id = 0; id < best.placements.size(); ++id) {
                const rectangle_pack_placement& placement = best.placements[id];
                if (!placement.placed()) continue;
                ids[placement.bin].push_back(static_cast<int>(id));
                areas[placement.bin] += placement.width * placement.height;
            }

            bool merged = false;
            for (int target_bin = 0; target_bin < last_bin; ++target_bin) {
                if (areas[target_bin] + areas[last_bin] > bin_area) continue;
                std::vector<int> original_ids = ids[target_bin];
                original_ids.insert(original_ids.end(),
                                    ids[last_bin].begin(), ids[last_bin].end());
                std::vector<rectangle_pack_item> subitems;
                subitems.reserve(original_ids.size());
                for (int id : original_ids) subitems.push_back(items[id]);

                rectangle_pack_options repair_options = options;
                repair_options.restart_limit = repair_restarts;
                const std::uint64_t mixed_seed = options.seed ^
                    static_cast<std::uint64_t>(target_bin + 1) ^
                    (static_cast<std::uint64_t>(best.bin_count) << 32);
                repair_options.seed = rng64(mixed_seed).next();
                rectangle_pack_result packed = construct_fixed(
                    subitems, width, height, repair_options);
                if (packed.placed_count == static_cast<int>(subitems.size())) {
                    for (std::size_t i = 0; i < original_ids.size(); ++i) {
                        rectangle_pack_placement placement = packed.placements[i];
                        placement.bin = target_bin;
                        best.placements[original_ids[i]] = placement;
                    }
                    summarize(items, best, best.placements);
                    merged = true;
                }
                if (repair_timer.expired()) return best;
                if (merged) break;
            }
            if (!merged) break;
        }
        return best;
    }

    static bool better_bounding(const rectangle_pack_result& lhs,
                                const rectangle_pack_result& rhs,
                                const rectangle_bounding_objective& objective) {
        if (rhs.placements.empty()) return true;
        return std::tuple(lhs.missing_required_count,
                          objective.evaluate(lhs.used_width, lhs.used_height),
                          lhs.used_width * lhs.used_height,
                          lhs.used_height, lhs.used_width) <
               std::tuple(rhs.missing_required_count,
                          objective.evaluate(rhs.used_width, rhs.used_height),
                          rhs.used_width * rhs.used_height,
                          rhs.used_height, rhs.used_width);
    }

    static rectangle_pack_result construct_bounding(
        std::span<const rectangle_pack_item> items,
        const rectangle_bounding_objective& objective,
        const rectangle_pack_options& options) {
        if (items.empty()) return empty_result(0);
        rectangle_pack_result best;
        const auto widths = [&] {
            constexpr std::size_t limit = 17;
            long long total_area = 0;
            long long minimum_width = 1;
            long long maximum_width = 0;
            for (const rectangle_pack_item& item : items) {
                total_area += item.width * item.height;
                const long long min_item_width = item.rotatable
                    ? std::min(item.width, item.height) : item.width;
                minimum_width = std::max(minimum_width, min_item_width);
                maximum_width += std::max(item.width, item.rotatable ? item.height : item.width);
            }

            long double target = std::sqrt(static_cast<long double>(std::max(1LL, total_area)));
            if (objective.type == rectangle_bounding_objective_type::weighted_sum) {
                assert(objective.width_weight > 0 && objective.height_weight > 0);
                target *= std::sqrt(static_cast<long double>(objective.height_weight) /
                                    static_cast<long double>(objective.width_weight));
            }

            static constexpr std::array<long double, 15> factors{
                0.40L, 0.50L, 0.60L, 0.70L, 0.80L, 0.90L, 1.00L, 1.10L,
                1.20L, 1.35L, 1.50L, 1.70L, 2.00L, 2.40L, 3.00L};
            std::vector<long long> values;
            values.reserve(factors.size() + items.size() * 2 + 2);
            values.push_back(minimum_width);
            values.push_back(maximum_width);
            for (long double factor : factors) {
                const long long width = static_cast<long long>(std::llround(target * factor));
                values.push_back(std::clamp(width, minimum_width, maximum_width));
            }
            for (const rectangle_pack_item& item : items) {
                values.push_back(std::clamp(item.width, minimum_width, maximum_width));
                if (item.rotatable)
                    values.push_back(std::clamp(item.height, minimum_width, maximum_width));
            }
            std::sort(values.begin(), values.end());
            values.erase(std::unique(values.begin(), values.end()), values.end());
            if (values.size() <= limit) return values;
            std::vector<long long> sampled;
            sampled.reserve(limit);
            for (std::size_t i = 0; i < limit; ++i) {
                const std::size_t index = i * (values.size() - 1) / (limit - 1);
                sampled.push_back(values[index]);
            }
            return sampled;
        }();
        const std::array<std::vector<int>, 2> orders{
            make_order(items, order_kind::long_side),
            make_order(items, order_kind::height)};
        const long long height_limit = safe_unbounded_height(items);
        deadline_checker timer(options);
        rng64 rng(options.seed);

        // 面積下界付近から広い範囲の幅を試し、実際に使用した外接幅と高さで比較する
        for (long long width : widths) {
            for (const std::vector<int>& order : orders) {
                rectangle_pack_result result = decode_maxrects(
                    items, order, width, height_limit, maxrects_state::heuristic::bottom_left);
                if (better_bounding(result, best, objective)) best = std::move(result);
            }
            if (timer.expired()) return compact_result(items, std::move(best), options);
        }

        // 最良幅の近傍6点を追加し、離散的な幅候補の隙間を探索する
        const std::array<int, 6> percentages{-12, -7, -3, 3, 7, 12};
        for (int percentage : percentages) {
            const long long width = std::max(
                1LL, best.used_width + best.used_width * percentage / 100);
            for (const std::vector<int>& order : orders) {
                rectangle_pack_result result = decode_maxrects(
                    items, order, width, height_limit, maxrects_state::heuristic::bottom_left);
                if (better_bounding(result, best, objective)) best = std::move(result);
            }
            if (timer.expired()) return compact_result(items, std::move(best), options);
        }

        // 探索予算の5/8は幅探索、残りは最良幅を固定した高さ圧縮へ使う。
        const int search_budget = std::max(0, options.search_iteration_limit);
        const int random_iterations = scaled_budget_ceil(search_budget, 5, 8);
        const int refine_iterations = search_budget - random_iterations;
        for (int iteration = 0; iteration < random_iterations; ++iteration) {
            const long long radius = std::max(1LL, best.used_width / 8);
            const long long offset = static_cast<long long>(rng.next() %
                static_cast<std::uint64_t>(2 * radius + 1)) - radius;
            const long long width = std::max(1LL, best.used_width + offset);
            std::vector<int> order = orders[rng.index(orders.size())];
            perturb_order(order, rng, 1 + iteration % 7);
            rectangle_pack_result result = decode_maxrects(
                items, order, width, height_limit, maxrects_state::heuristic::bottom_left);
            if (better_bounding(result, best, objective)) best = std::move(result);
            if (timer.expired()) break;
        }
        if (timer.expired()) return compact_result(items, std::move(best), options);

        // 同じ幅で高さを詰める。前半はfail-first、後半はContact Pointを使う。
        for (int iteration = 0; iteration < refine_iterations; ++iteration) {
            const long long width = best.used_width;
            const long long lower_bound = strip_height_lower_bound(items, width);
            if (lower_bound == std::numeric_limits<long long>::max() ||
                best.used_height <= lower_bound)
                break;
            const long long gap = best.used_height - lower_bound;
            const long long divisor = refine_iterations <= 1
                ? 2 : 2 + 6LL * iteration / (refine_iterations - 1);
            const long long target = std::max(
                lower_bound, best.used_height - std::max(1LL, gap / divisor));
            std::vector<int> order = orders[rng.index(orders.size())];
            perturb_order(order, rng, 1 + iteration % 7);
            rectangle_pack_result result = iteration < refine_iterations / 2
                ? decode_maxrects_constrained(
                      items, order, width, target, 16)
                : decode_contact_point(items, order, width, target);
            if (result.placed_count == static_cast<int>(items.size()) &&
                better_bounding(result, best, objective))
                best = std::move(result);
            if (timer.expired()) break;
        }
        return compact_result(items, std::move(best), options);
    }

    static void assert_valid_items(std::span<const rectangle_pack_item> items) {
        assert(items.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
        for (const rectangle_pack_item& item : items) {
            assert(item.width > 0 && item.height > 0);
            (void)item;
        }
    }
};


// placementsを外部で変更した後、目的値比較に使う集計値を再計算する。O(N)
// 座標・寸法・重複の妥当性は検査しないため、必要ならこの後validatorを呼ぶ。
inline void recompute_rectangle_packing_summary(
    std::span<const rectangle_pack_item> items,
    rectangle_pack_result& result) {
    assert(result.placements.size() == items.size());
    rectangle_pack_result::engine::assert_valid_items(items);
    rectangle_pack_result::engine::summarize(items, result, result.placements);
}

// 固定ビンへ必須矩形を優先し、同率ならprofitと配置面積を最大化する。O(K N (F^2 + N F))
inline rectangle_pack_result pack_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {}) {
    assert(bin_width > 0 && bin_height > 0);
    rectangle_pack_result::engine::assert_valid_items(items);
    return rectangle_pack_result::engine::construct_fixed(
        items, bin_width, bin_height, options);
}

// 妥当な初期解を保持しつつ固定ビンpackingを再探索する。O(K N (F^2 + N F))
inline rectangle_pack_result improve_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {}) {
    assert(bin_width > 0 && bin_height > 0);
    assert(initial_solution.placements.size() == items.size());
    rectangle_pack_result::engine::assert_valid_items(items);
    return rectangle_pack_result::engine::construct_fixed(
        items, bin_width, bin_height, options, &initial_solution);
}

// 固定ビンの可動矩形だけを再配置する。固定数L、可動数Mとして O(L F^2 + K M(F^2+MF))
// それ以外の配置は座標・回転を含めて固定する。初期解との比較は行わないため、
// 問題固有スコアを呼出側で評価して採否を決める用途に使う。
inline rectangle_pack_result repack_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    std::span<const int> movable_item_ids,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {}) {
    assert(bin_width > 0 && bin_height > 0);
    assert(initial_solution.placements.size() == items.size());
    rectangle_pack_result::engine::assert_valid_items(items);
    return rectangle_pack_result::engine::construct_fixed_repair(
        items, initial_solution, movable_item_ids,
        bin_width, bin_height, options, false);
}

// 初期解より悪化させず部分再配置する。固定数L、可動数Mとして O(L F^2 + K M(F^2+MF))
inline rectangle_pack_result repair_rectangles_fixed_bin(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    std::span<const int> movable_item_ids,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {}) {
    assert(bin_width > 0 && bin_height > 0);
    assert(initial_solution.placements.size() == items.size());
    rectangle_pack_result::engine::assert_valid_items(items);
    return rectangle_pack_result::engine::construct_fixed_repair(
        items, initial_solution, movable_item_ids,
        bin_width, bin_height, options, true);
}

// 幅固定stripへ全矩形を詰め、使用高さを最小化する。最悪 O(K N(F^2+NF) + N^2 + N log N)
// 辺座標ごとの要素数を定数とみなす構築の期待計算量は O(K N F^2)。隙間詰めは O(N) 領域を使う。
inline rectangle_pack_result pack_rectangles_strip(
    std::span<const rectangle_pack_item> items,
    long long strip_width,
    const rectangle_pack_options& options = {}) {
    assert(strip_width > 0);
    rectangle_pack_result::engine::assert_valid_items(items);
    return rectangle_pack_result::engine::construct_strip(items, strip_width, options);
}

// 妥当な初期解より悪化させずstrip packingを再探索する。最悪 O(K N(F^2+NF) + N^2 + N log N)
// 辺座標ごとの要素数を定数とみなす構築の期待計算量は O(K N F^2)。隙間詰めは O(N) 領域を使う。
inline rectangle_pack_result improve_rectangles_strip(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    long long strip_width,
    const rectangle_pack_options& options = {}) {
    assert(strip_width > 0);
    assert(initial_solution.placements.size() == items.size());
    rectangle_pack_result::engine::assert_valid_items(items);
    rectangle_pack_result candidate = rectangle_pack_result::engine::construct_strip(
        items, strip_width, options);
    if (rectangle_pack_result::engine::better_strip(initial_solution, candidate))
        return initial_solution;
    return candidate;
}

// 同一サイズの複数ビンへ全矩形を詰め、使用ビン数を最小化する。最悪 O(K N(BNF+F^2) + B^2 N(F^2+NF))
// 構築の期待計算量は辺座標ごとの要素数を定数とみなすと O(K N(BF+F^2))
inline rectangle_pack_result pack_rectangles_multiple_bins(
    std::span<const rectangle_pack_item> items,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {}) {
    assert(bin_width > 0 && bin_height > 0);
    rectangle_pack_result::engine::assert_valid_items(items);
    return rectangle_pack_result::engine::construct_multiple_bins(
        items, bin_width, bin_height, options);
}

// 妥当な初期解より悪化させず複数ビンpackingを再探索する。最悪 O(K N(BNF+F^2) + B^2 N(F^2+NF))
// 構築の期待計算量は辺座標ごとの要素数を定数とみなすと O(K N(BF+F^2))
inline rectangle_pack_result improve_rectangles_multiple_bins(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    long long bin_width,
    long long bin_height,
    const rectangle_pack_options& options = {}) {
    assert(bin_width > 0 && bin_height > 0);
    assert(initial_solution.placements.size() == items.size());
    rectangle_pack_result::engine::assert_valid_items(items);
    rectangle_pack_result candidate = rectangle_pack_result::engine::construct_multiple_bins(
        items, bin_width, bin_height, options);
    if (rectangle_pack_result::engine::better_multiple_bins(initial_solution, candidate))
        return initial_solution;
    return candidate;
}

// 全矩形を囲む外接矩形の指定目的値を最小化する。最悪 O(K N(F^2+NF) + N^2 + N log N)
// 辺座標ごとの要素数を定数とみなす構築の期待計算量は O(K N F^2)。隙間詰めは O(N) 領域を使う。
inline rectangle_pack_result pack_rectangles_bounding_box(
    std::span<const rectangle_pack_item> items,
    const rectangle_bounding_objective& objective = {},
    const rectangle_pack_options& options = {}) {
    rectangle_pack_result::engine::assert_valid_items(items);
    return rectangle_pack_result::engine::construct_bounding(items, objective, options);
}

// 妥当な初期解より悪化させず外接矩形packingを再探索する。最悪 O(K N(F^2+NF) + N^2 + N log N)
// 辺座標ごとの要素数を定数とみなす構築の期待計算量は O(K N F^2)。隙間詰めは O(N) 領域を使う。
inline rectangle_pack_result improve_rectangles_bounding_box(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& initial_solution,
    const rectangle_bounding_objective& objective = {},
    const rectangle_pack_options& options = {}) {
    assert(initial_solution.placements.size() == items.size());
    rectangle_pack_result::engine::assert_valid_items(items);
    rectangle_pack_result candidate = rectangle_pack_result::engine::construct_bounding(
        items, objective, options);
    if (rectangle_pack_result::engine::better_bounding(initial_solution, candidate, objective))
        return initial_solution;
    return candidate;
}

// 配置寸法・回転・境界・重なり・集計値が正しければ true を返す。O(N^2)
// allow_missing_required=trueなら、require_all_items=falseのときだけ未配置requiredを許す。
inline bool validate_rectangle_packing(
    std::span<const rectangle_pack_item> items,
    const rectangle_pack_result& result,
    long long bin_width = 0,
    long long bin_height = 0,
    bool require_all_items = false,
    std::string* error_message = nullptr,
    bool allow_missing_required = false) {
    const auto fail = [&](const std::string& message) {
        if (error_message) *error_message = message;
        return false;
    };
    if (result.placements.size() != items.size()) return fail("placements の要素数が不正");
    if (items.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return fail("矩形数がintの範囲を超過");

    rectangle_pack_result summary;
    __int128_t profit = 0;
    constexpr long long maximum = std::numeric_limits<long long>::max();

    // 不正な入力を集計へ渡さず、各配置の演算が安全だと確認してから加算する
    for (std::size_t i = 0; i < items.size(); ++i) {
        const rectangle_pack_item& item = items[i];
        const rectangle_pack_placement& p = result.placements[i];
        if (item.width <= 0 || item.height <= 0) return fail("入力矩形の寸法が不正");
        if (!p.placed()) {
            if (require_all_items || (item.required && !allow_missing_required))
                return fail("必須矩形が未配置");
            if (item.required) ++summary.missing_required_count;
            continue;
        }
        if (p.x < 0 || p.y < 0 || p.width <= 0 || p.height <= 0)
            return fail("座標または寸法が不正");
        const bool normal = p.width == item.width && p.height == item.height && !p.rotated;
        const bool rotated = item.rotatable && p.width == item.height &&
                             p.height == item.width && p.rotated;
        if (!normal && !rotated) return fail("入力矩形と配置寸法が一致しない");
        if (p.x > maximum - p.width || p.y > maximum - p.height)
            return fail("配置の右端または下端がlong longの範囲を超過");
        if (p.bin == std::numeric_limits<int>::max()) return fail("ビン数がintの範囲を超過");
        if (bin_width > 0 && p.x + p.width > bin_width) return fail("ビン幅を超過");
        if (bin_height > 0 && p.y + p.height > bin_height) return fail("ビン高さを超過");

        const __int128_t area = static_cast<__int128_t>(p.width) * p.height;
        if (area > maximum - summary.placed_area) return fail("配置面積合計がlong longの範囲を超過");
        summary.placed_area += static_cast<long long>(area);
        profit += item.profit;
        summary.used_width = std::max(summary.used_width, p.x + p.width);
        summary.used_height = std::max(summary.used_height, p.y + p.height);
        summary.bin_count = std::max(summary.bin_count, p.bin + 1);
        ++summary.placed_count;
    }
    if (profit < std::numeric_limits<long long>::min() || profit > maximum)
        return fail("profit合計がlong longの範囲を超過");
    summary.placed_profit = static_cast<long long>(profit);

    // 端点の加算が安全な配置だけを使い、同じビン内での重なりを調べる
    for (std::size_t i = 0; i < items.size(); ++i) {
        const rectangle_pack_placement& a = result.placements[i];
        if (!a.placed()) continue;
        const rectangle_pack_result::engine::box box_a{a.x, a.y, a.width, a.height};
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            const rectangle_pack_placement& b = result.placements[j];
            if (!b.placed() || a.bin != b.bin) continue;
            const rectangle_pack_result::engine::box box_b{b.x, b.y, b.width, b.height};
            if (box_a.intersects(box_b))
                return fail("矩形が重複");
        }
    }

    if (summary.used_width != result.used_width ||
        summary.used_height != result.used_height ||
        summary.placed_profit != result.placed_profit ||
        summary.placed_area != result.placed_area ||
        summary.placed_count != result.placed_count ||
        summary.missing_required_count != result.missing_required_count ||
        summary.bin_count != result.bin_count)
        return fail("集計値が配置と一致しない");
    return true;
}

#if __INCLUDE_LEVEL__ == 0

// 自己テストからのみ非公開の構築状態を検査する
struct rectangle_packing_test_access : rectangle_pack_result::engine {
    static bool contains(const box& a, const box& b) {
        return a.x <= b.x && a.y <= b.y && b.right() <= a.right() && b.bottom() <= a.bottom();
    }
    static long long right(const box& b) { return b.right(); }
    static long long bottom(const box& b) { return b.bottom(); }
    static bool better_candidate(const candidate& a, const candidate& b) { return a.better_than(b); }
};

int main() {
    using access = rectangle_packing_test_access;
    const auto fail = [&](const std::string& label) {
        std::cerr << "FAILED: " << label << '\n';
        std::abort();
    };
    const auto require = [&](bool condition, const std::string& label) {
        if (!condition) fail(label);
    };
    const auto require_valid = [&](std::span<const rectangle_pack_item> items,
                   const rectangle_pack_result& result,
                   long long width,
                   long long height,
                   bool require_all,
                   const std::string& label) {
        std::string error;
        if (!validate_rectangle_packing(items, result, width, height, require_all, &error))
            fail(label + ": " + error);
    };
    const auto fast_options = [&](std::uint64_t seed = 1) {
        rectangle_pack_options options;
        options.seed = seed;
        options.restart_limit = 4;
        options.search_iteration_limit = 8;
        return options;
    };
    const auto same_placements = [&](const rectangle_pack_result& a, const rectangle_pack_result& b) {
        if (a.placements.size() != b.placements.size()) return false;
        for (std::size_t i = 0; i < a.placements.size(); ++i) {
            const rectangle_pack_placement& x = a.placements[i];
            const rectangle_pack_placement& y = b.placements[i];
            if (std::tie(x.x, x.y, x.width, x.height, x.bin, x.rotated) !=
                std::tie(y.x, y.y, y.width, y.height, y.bin, y.rotated))
                return false;
        }
        return true;
    };
    // fixed_bin の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {5, 5, 1, false, true}, {5, 5, 1, false, true},
            {5, 5, 1, false, true}, {5, 5, 1, false, true}};
        rectangle_pack_result result = pack_rectangles_fixed_bin(items, 10, 10, fast_options());
        require(result.feasible(), "fixed feasible");
        require(result.placed_count == 4, "fixed count");
        require_valid(items, result, 10, 10, true, "fixed validate");
    }();
    // rotation_and_impossible の検証
    [&] {
        const std::vector<rectangle_pack_item> rotatable{{7, 4, 1, true, true}};
        rectangle_pack_result rotated = pack_rectangles_fixed_bin(rotatable, 4, 7, fast_options());
        require(rotated.feasible() && rotated.placements[0].rotated, "rotation");
        require_valid(rotatable, rotated, 4, 7, true, "rotation validate");

        const std::vector<rectangle_pack_item> fixed{{7, 4, 1, false, true}};
        rectangle_pack_result impossible = pack_rectangles_fixed_bin(fixed, 4, 7, fast_options());
        require(!impossible.feasible(), "impossible");
        require(!validate_rectangle_packing(fixed, impossible, 4, 7, true),
                "impossible require all");
    }();
    // optional_profit の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {10, 10, 5, false, false},
            {5, 10, 8, false, false},
            {5, 10, 7, false, false}};
        rectangle_pack_result result = pack_rectangles_fixed_bin(items, 10, 10, fast_options());
        require(result.feasible(), "optional feasible");
        require(result.placed_profit == 15, "optional profit");
        require_valid(items, result, 10, 10, false, "optional validate");
    }();
    // required_priority_and_negative_profit の検証
    [&] {
        const std::vector<rectangle_pack_item> priority_items{
            {10, 10, 1, false, true},
            {10, 10, 1000, false, false}};
        rectangle_pack_result priority = pack_rectangles_fixed_bin(
            priority_items, 10, 10, fast_options());
        require(priority.feasible() && priority.placements[0].placed(), "required priority");
        require(!priority.placements[1].placed(), "optional after required");
        require_valid(priority_items, priority, 10, 10, false, "required priority validate");

        const std::vector<rectangle_pack_item> negative_items{
            {5, 10, 5, false, true},
            {5, 10, -10, false, false}};
        rectangle_pack_result negative = pack_rectangles_fixed_bin(
            negative_items, 10, 10, fast_options());
        require(negative.placed_profit == 5 && !negative.placements[1].placed(),
                "negative optional skipped");
        require_valid(negative_items, negative, 10, 10, false, "negative validate");
    }();
    // strip の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {4, 3, 1, false, true}, {6, 3, 1, false, true},
            {5, 2, 1, false, true}, {5, 2, 1, false, true}};
        rectangle_pack_result result = pack_rectangles_strip(items, 10, fast_options());
        require(result.feasible() && result.placed_count == 4, "strip feasible");
        require(result.used_height == 5, "strip height");
        require_valid(items, result, 10, 0, true, "strip validate");
    }();
    // contact_point_and_single_strip_iteration の検証
    [&] {
        const std::vector<rectangle_pack_item> squares{
            {5, 5, 1, false, true}, {5, 5, 1, false, true},
            {5, 5, 1, false, true}, {5, 5, 1, false, true}};
        const std::array<int, 4> order{0, 1, 2, 3};
        const rectangle_pack_result contact =
            access::decode_contact_point(squares, order, 10, 10);
        require(contact.feasible() && contact.placed_count == 4, "contact point feasible");
        require_valid(squares, contact, 10, 10, true, "contact point validate");

        rectangle_pack_options options = fast_options();
        options.search_iteration_limit = 1;
        const rectangle_pack_result strip = pack_rectangles_strip(squares, 10, options);
        require(strip.feasible(), "single strip iteration feasible");
        require_valid(squares, strip, 10, 0, true, "single strip iteration validate");
    }();
    // contact_edge_index_growth の検証
    [&] {
        // 実際の辺登録・候補評価を通し、grow後の4種の辺と同一座標の連結を検査する。
        access::contact_point_state state(10000, 10000, 0);
        for (int i = 0; i < 100; ++i)
            state.add_contacts({10LL * i + 10, 20LL * i + 10, 3, 10});
        require(state.contacts.slots.size() > 8, "contact edge grew");
        const auto query = [&](access::box b, long long expected) {
            state.space.free_rectangles = {b};
            const auto answer = state.find_position({b.width,b.height,1,false,true});
            require(answer.valid && answer.score1 == -expected, "contact edge rehash/query");
        };
        for (int i = 0; i < 100; ++i) {
            const long long x = 10LL * i + 10, y = 20LL * i + 10;
            query({x-2,y+3,2,4},4);
            query({x+3,y+3,2,4},4);
            query({x+1,y-2,1,2},1);
            query({x+1,y+10,1,2},1);
        }
        query({9000,9000,3,4},0);
        state.add_contacts({180,3000,3,10});
        query({178,3002,2,6},6);
        query({178,353,2,4},4);  // 同一座標の旧辺も残る。
    }();
    // 公開APIからContact Pointのrollbackを複数回通し、固定配置と再現性を確認する
    [&] {
        const std::vector<rectangle_pack_item> items{
            {4,4,1,false,true}, {3,4,1,false,true}, {3,2,1,false,true},
            {5,3,1,true,true}, {2,4,1,true,true}, {3,3,1,false,true},
            {11,11,1,false,true}};
        rectangle_pack_result base;
        base.placements = {{0,0,4,4,0,false}, {4,0,3,4,0,false}, {7,0,3,2,0,false}, {}, {}, {}, {}};
        recompute_rectangle_packing_summary(items, base);
        const std::array<int,4> movable{3,4,5,6};
        auto options = fast_options(743);
        options.restart_limit = 40;
        const auto first = repack_rectangles_fixed_bin(items,base,movable,10,10,options);
        const auto second = repack_rectangles_fixed_bin(items,base,movable,10,10,options);
        require(same_placements(first,second), "public rollback deterministic");
        require(validate_rectangle_packing(items,first,10,10,false,nullptr,true), "public rollback valid");
        for (int i = 0; i < 3; ++i) {
            const auto& a = base.placements[i]; const auto& b = first.placements[i];
            require(std::tie(a.x,a.y,a.width,a.height,a.bin,a.rotated) ==
                    std::tie(b.x,b.y,b.width,b.height,b.bin,b.rotated), "public rollback keeps locked");
        }
    }();
    // multiple_bins の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {6, 6, 1, false, true}, {6, 6, 1, false, true},
            {4, 4, 1, false, true}, {4, 4, 1, false, true}};
        rectangle_pack_result result = pack_rectangles_multiple_bins(items, 10, 10, fast_options());
        require(result.feasible() && result.bin_count == 2, "multiple bins");
        require_valid(items, result, 10, 10, true, "multiple validate");
    }();
    // 複数ビンの追加探索・pair-mergeを公開APIで検証する
    [&] {
        std::mt19937_64 random(59131);
        for (int trial = 0; trial < 64; ++trial) {
            std::vector<rectangle_pack_item> items;
            for (int i = 0; i < 20; ++i)
                items.push_back({1 + static_cast<long long>(random()%9),
                                 1 + static_cast<long long>(random()%9),1,true,true});
            auto options = fast_options(random());
            options.search_iteration_limit = 0;
            const auto before = pack_rectangles_multiple_bins(items,16,16,options);
            options.search_iteration_limit = 16;
            const auto after = pack_rectangles_multiple_bins(items,16,16,options);
            require_valid(items,after,16,16,true,"public multi repair valid");
            require(!access::better_multiple_bins(before,after), "public multi repair nondegradation");
        }
    }();
    // bounding_box の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {4, 3, 1, true, true}, {4, 3, 1, true, true}};
        rectangle_pack_options options = fast_options();
        rectangle_bounding_objective perimeter;
        perimeter.type = rectangle_bounding_objective_type::perimeter;
        rectangle_pack_result result = pack_rectangles_bounding_box(items, perimeter, options);
        require(result.feasible() && result.placed_count == 2, "bounding feasible");
        require(perimeter.evaluate(result.used_width, result.used_height) <= 11,
                "bounding perimeter");
        require_valid(items, result, 0, 0, true, "bounding validate");
    }();
    // fixed_partial_repair の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {5, 5, 1, false, true}, {5, 5, 1, false, true},
            {5, 5, 1, false, true}, {5, 5, 1, false, true}};
        rectangle_pack_result damaged;
        damaged.placements = {
            {0, 0, 5, 5, 0, false}, {5, 0, 5, 5, 0, false},
            {0, 5, 5, 5, 0, false}, {}};
        recompute_rectangle_packing_summary(items, damaged);
        require(damaged.missing_required_count == 1, "repair damaged summary");
        std::string damaged_error;
        require(validate_rectangle_packing(
                    items, damaged, 10, 10, false, &damaged_error, true),
                "repair damaged geometry");

        const std::array<int, 1> movable{3};
        const rectangle_pack_result repacked = repack_rectangles_fixed_bin(
            items, damaged, movable, 10, 10, fast_options(123));
        require(repacked.feasible() && repacked.placed_count == 4,
                "partial repack restores item");
        require_valid(items, repacked, 10, 10, true, "partial repack validate");
        const rectangle_pack_result repaired = repair_rectangles_fixed_bin(
            items, damaged, movable, 10, 10, fast_options(123));
        require(repaired.feasible() && repaired.placed_count == 4,
                "partial repair restores item");
        require_valid(items, repaired, 10, 10, true, "partial repair validate");
        for (int id = 0; id < 3; ++id) {
            const rectangle_pack_placement& before = damaged.placements[id];
            const rectangle_pack_placement& after = repaired.placements[id];
            require(std::tie(before.x, before.y, before.width, before.height,
                             before.bin, before.rotated) ==
                    std::tie(after.x, after.y, after.width, after.height,
                             after.bin, after.rotated),
                    "partial repair keeps locked item");
        }

        const std::span<const int> none;
        const rectangle_pack_result unchanged = repair_rectangles_fixed_bin(
            items, repaired, none, 10, 10, fast_options(456));
        require(same_placements(repaired, unchanged), "empty repair is unchanged");
    }();
    // improve_and_determinism の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {6, 4, 3, true, true}, {5, 7, 2, true, true},
            {4, 4, 1, false, true}, {3, 8, 4, true, true},
            {2, 9, 5, true, true}, {7, 3, 2, true, true}};
        const rectangle_pack_options options = fast_options(998244353);

        rectangle_pack_result fixed = pack_rectangles_fixed_bin(items, 15, 15, options);
        rectangle_pack_result fixed_again = pack_rectangles_fixed_bin(items, 15, 15, options);
        require(same_placements(fixed, fixed_again), "deterministic fixed");
        rectangle_pack_result improved_fixed = improve_rectangles_fixed_bin(
            items, fixed, 15, 15, fast_options(2));
        require(!access::better_fixed(fixed, improved_fixed),
                "improve fixed does not worsen");

        rectangle_pack_result strip = pack_rectangles_strip(items, 15, options);
        rectangle_pack_result improved_strip = improve_rectangles_strip(
            items, strip, 15, fast_options(3));
        require(!access::better_strip(strip, improved_strip),
                "improve strip does not worsen");

        rectangle_pack_result multi = pack_rectangles_multiple_bins(items, 10, 10, options);
        rectangle_pack_result improved_multi = improve_rectangles_multiple_bins(
            items, multi, 10, 10, fast_options(4));
        require(!access::better_multiple_bins(multi, improved_multi),
                "improve multi does not worsen");

        const rectangle_bounding_objective objective;
        rectangle_pack_result bounding = pack_rectangles_bounding_box(items, objective, options);
        rectangle_pack_result improved_bounding = improve_rectangles_bounding_box(
            items, bounding, objective, fast_options(5));
        require(!access::better_bounding(
                    bounding, improved_bounding, objective),
                "improve bounding does not worsen");
    }();
    // empty の検証
    [&] {
        const std::vector<rectangle_pack_item> items;
        rectangle_pack_result fixed = pack_rectangles_fixed_bin(items, 10, 10, fast_options());
        rectangle_pack_result strip = pack_rectangles_strip(items, 10, fast_options());
        rectangle_pack_result multi = pack_rectangles_multiple_bins(items, 10, 10, fast_options());
        rectangle_pack_result bounding = pack_rectangles_bounding_box(items, {}, fast_options());
        require(fixed.placements.empty() && strip.placements.empty() &&
                multi.placements.empty() && bounding.placements.empty(), "empty");
    }();
    // expired_deadline の検証
    [&] {
        const std::vector<rectangle_pack_item> items{
            {9, 7, 1, true, true}, {8, 6, 1, true, true},
            {7, 5, 1, true, true}, {6, 4, 1, true, true}};
        rectangle_pack_options options;
        options.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        options.restart_limit = 1000000;
        options.search_iteration_limit = 1000000;
        options.time_check_interval = 1;

        const rectangle_pack_result fixed = pack_rectangles_fixed_bin(items, 20, 20, options);
        const rectangle_pack_result strip = pack_rectangles_strip(items, 20, options);
        const rectangle_pack_result multi = pack_rectangles_multiple_bins(items, 20, 20, options);
        const rectangle_pack_result bounding = pack_rectangles_bounding_box(items, {}, options);
        require_valid(items, fixed, 20, 20, true, "deadline fixed");
        require_valid(items, strip, 20, 0, true, "deadline strip");
        require_valid(items, multi, 20, 20, true, "deadline multi");
        require_valid(items, bounding, 0, 0, true, "deadline bounding");
    }();
    // random_validity の検証
    [&] {
        std::mt19937_64 rng(123456789);
        for (int test = 0; test < 300; ++test) {
            const int n = 1 + static_cast<int>(rng() % 40);
            std::vector<rectangle_pack_item> items;
            items.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                const long long w = 1 + static_cast<long long>(rng() % 30);
                const long long h = 1 + static_cast<long long>(rng() % 30);
                items.push_back({w, h, 1, (rng() & 1U) != 0U, true});
            }
            rectangle_pack_options options = fast_options(rng());
            rectangle_pack_result strip = pack_rectangles_strip(items, 60, options);
            require_valid(items, strip, 60, 0, true, "random strip");
            rectangle_pack_result multi = pack_rectangles_multiple_bins(items, 60, 60, options);
            require_valid(items, multi, 60, 60, true, "random multi");
            rectangle_pack_result bounding = pack_rectangles_bounding_box(items, {}, options);
            require_valid(items, bounding, 0, 0, true, "random bounding");

            for (rectangle_pack_item& item : items) item.required = false;
            rectangle_pack_result fixed = pack_rectangles_fixed_bin(items, 60, 60, options);
            require_valid(items, fixed, 60, 60, false, "random fixed");

            std::vector<int> movable;
            std::vector<unsigned char> movable_mask(items.size(), 0);
            for (int id = 0; id < n; ++id) {
                if ((rng() & 3U) != 0U) continue;
                movable.push_back(id);
                movable_mask[id] = 1;
            }
            rectangle_pack_result repaired = repair_rectangles_fixed_bin(
                items, fixed, movable, 60, 60, options);
            std::string repair_error;
            require(validate_rectangle_packing(
                        items, repaired, 60, 60, false, &repair_error, true),
                    "random repair validate: " + repair_error);
            require(!access::better_fixed(fixed, repaired),
                    "random repair does not worsen");
            for (std::size_t id = 0; id < items.size(); ++id) {
                if (movable_mask[id]) continue;
                const rectangle_pack_placement& before = fixed.placements[id];
                const rectangle_pack_placement& after = repaired.placements[id];
                require(std::tie(before.x, before.y, before.width, before.height,
                                 before.bin, before.rotated) ==
                        std::tie(after.x, after.y, after.width, after.height,
                                 after.bin, after.rotated),
                        "random repair keeps locked item");
            }
        }
    }();
    // maxrects_grid_oracle の検証
    [&] {
        using maxrects_state = access::maxrects_state;
        const auto contains = access::contains;
        std::mt19937_64 rng(0x102030405060ULL);
        for (int trial = 0; trial < 160; ++trial) {
            constexpr int side = 8;
            maxrects_state state(side, side);
            bool occupied[side][side]{};
            const auto empty = [&](int x, int y, int w, int h) {
                for (int yy = y; yy < y + h; ++yy)
                    for (int xx = x; xx < x + w; ++xx) if (occupied[yy][xx]) return false;
                return true;
            };
            for (int step = 0; step < 16; ++step) {
                // 候補探索で得た位置に限らず、自由な内部座標へ障害物を追加する
                const int x = static_cast<int>(rng() % side), y = static_cast<int>(rng() % side);
                const int w = 1 + static_cast<int>(rng() % static_cast<unsigned int>(side - x));
                const int h = 1 + static_cast<int>(rng() % static_cast<unsigned int>(side - y));
                if (empty(x, y, w, h)) {
                    state.occupy({x, y, w, h});
                    for (int yy = y; yy < y + h; ++yy)
                        for (int xx = x; xx < x + w; ++xx) occupied[yy][xx] = true;
                }
                for (std::size_t a = 0; a < state.free_rectangles.size(); ++a) {
                    const auto& f = state.free_rectangles[a];
                    require(empty(static_cast<int>(f.x), static_cast<int>(f.y),
                                  static_cast<int>(f.width), static_cast<int>(f.height)), "oracle free rectangle");
                    for (std::size_t b = 0; b < state.free_rectangles.size(); ++b)
                        if (a != b) require(!contains(f, state.free_rectangles[b]), "oracle noncontainment");
                }
                for (int y0 = 0; y0 < side; ++y0) for (int x0 = 0; x0 < side; ++x0)
                    for (int y1 = y0 + 1; y1 <= side; ++y1) for (int x1 = x0 + 1; x1 <= side; ++x1) {
                        const bool vacant = empty(x0, y0, x1 - x0, y1 - y0);
                        bool covered = false;
                        for (const auto& f : state.free_rectangles)
                            if (contains(f, {x0, y0, x1 - x0, y1 - y0})) covered = true;
                        require(vacant == covered, "oracle all empty rectangles covered");
                    }
            }
        }
    }();
    // contact_naive_oracle の検証
    [&] {
        using contact_point_state = access::contact_point_state;
        using box = access::box;
        using candidate = access::candidate;
        const auto right = access::right;
        const auto bottom = access::bottom;
        const auto better_candidate = access::better_candidate;
        std::mt19937_64 rng(0x99887766ULL);
        for (int trial = 0; trial < 200; ++trial) {
            contact_point_state state(37, 29, 0);
            std::vector<box> placed;
            for (int step = 0; step < 24; ++step) {
                const rectangle_pack_item item{1 + static_cast<long long>(rng() % 12),
                    1 + static_cast<long long>(rng() % 12), 1, (rng() & 1U) != 0U, true};
                candidate naive;
                for (const auto& f : state.space.free_rectangles) for (int r = 0; r < 2; ++r) {
                    if (r && (!item.rotatable || item.width == item.height)) continue;
                    const long long w = r ? item.height : item.width, h = r ? item.width : item.height;
                    if (w > f.width || h > f.height) continue;
                    for (int corner = 0; corner < 4; ++corner) {
                        const box b{f.x + ((corner & 1) ? f.width - w : 0),
                                    f.y + ((corner & 2) ? f.height - h : 0), w, h};
                        long long score = b.x == 0 || right(b) == state.width ? h : 0;
                        if (b.y == 0 || bottom(b) == state.height) score += w;
                        for (const auto& p : placed) {
                            if (b.x == right(p) || right(b) == p.x)
                                score += std::max(0LL, std::min(bottom(b), bottom(p)) - std::max(b.y, p.y));
                            if (b.y == bottom(p) || bottom(b) == p.y)
                                score += std::max(0LL, std::min(right(b), right(p)) - std::max(b.x, p.x));
                        }
                        candidate c{b, r != 0, true, -score, f.width * f.height - w * h,
                                    std::min(f.width - w, f.height - h), b.y + h};
                        if (better_candidate(c, naive)) naive = c;
                    }
                }
                const candidate actual = state.find_position(item);
                require(actual.valid == naive.valid && !better_candidate(actual, naive) && !better_candidate(naive, actual),
                        "contact indexed vs naive");
                if (actual.valid) { placed.push_back(actual.rect); state.occupy(actual.rect); }
            }
        }
    }();
    // extended_contracts の検証
    [&] {
        const std::vector<rectangle_pack_item> impossible{{11, 1, 1, false, true}, {2, 2, 1, false, true}};
        const auto partial = pack_rectangles_fixed_bin(impossible, 10, 10, fast_options());
        require(partial.missing_required_count == 1, "oversized partial count");
        require(validate_rectangle_packing(impossible, partial, 10, 10, false, nullptr, true), "oversized partial geometry");
        auto corrupt = partial; corrupt.placements[1].x = 20;
        recompute_rectangle_packing_summary(impossible, corrupt);
        require(!validate_rectangle_packing(impossible, corrupt, 10, 10, false, nullptr, true), "invalid partial rejected");
        require(!validate_rectangle_packing(impossible, partial, 10, 10, true, nullptr, true), "require all precedence");

        const std::vector<rectangle_pack_item> items{{9, 3, 1, true, true}, {7, 4, 1, false, true}, {5, 6, 1, true, true}};
        for (int k = 0; k < 4; ++k) {
            const rectangle_bounding_objective objective{
                k == 0 ? rectangle_bounding_objective_type::area : k == 1
                    ? rectangle_bounding_objective_type::perimeter : rectangle_bounding_objective_type::weighted_sum,
                k == 2 ? 9 : 1, k == 3 ? 9 : 1};
            const auto first = pack_rectangles_bounding_box(items, objective, fast_options(1));
            const auto again = improve_rectangles_bounding_box(items, first, objective, fast_options(2));
            require_valid(items, again, 0, 0, true, "all bounding objectives");
            require(!access::better_bounding(first, again, objective), "weighted improve nondegradation");
        }
        const std::vector<rectangle_pack_item> negative{{3, 3, -1, true, false}, {3, 3, 0, false, false}, {3, 3, 7, false, false}};
        const auto selected = pack_rectangles_fixed_bin(negative, 6, 3, fast_options());
        require(!selected.placements[0].placed() && selected.placed_profit == 7 && selected.placed_area == 18,
                "signed profit and zero profit area tie");
        const std::vector<rectangle_pack_item> large{{1000000, 3000000, 1, true, true}, {2000000, 2000000, 1, false, true}};
        require_valid(large, pack_rectangles_fixed_bin(large, 4000000, 4000000, fast_options()),
                      4000000, 4000000, true, "large integer coordinates");
    }();
    // compaction_contracts の検証
    [&] {
        const auto compact_result = access::compact_result;
        const std::vector<rectangle_pack_item> empty_items;
        require_valid(empty_items, compact_result(empty_items, access::empty_result(0), {}),
                      0, 0, true, "compact empty");
        const std::vector<rectangle_pack_item> line_items{
            {2, 2, 3, false, true}, {2, 2, 5, false, true}, {1, 2, 7, false, true}};
        auto line = access::empty_result(3);
        line.placements = {{3,4,2,2,0,false},{8,4,2,2,0,false},{12,4,1,2,0,false}};
        recompute_rectangle_packing_summary(line_items, line);
        rectangle_pack_options expired;
        expired.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        require(same_placements(line, compact_result(line_items, line, expired)), "compact expired deadline");
        const auto packed_line = compact_result(line_items, line, {});
        require_valid(line_items, packed_line, 5, 2, true, "compact known row");
        require(packed_line.used_width == 5 && packed_line.used_height == 2, "compact known dimensions");

        std::mt19937_64 rng(761928341);
        for (int trial = 0; trial < 300; ++trial) {
            const int n = 1 + static_cast<int>(rng() % 80);
            const long long scale = trial % 7 == 0 ? 1000000 : 1;
            std::vector<rectangle_pack_item> items;
            auto initial = access::empty_result(static_cast<std::size_t>(n));
            std::vector<int> cells(static_cast<std::size_t>(n));
            std::iota(cells.begin(), cells.end(), 0);
            std::shuffle(cells.begin(), cells.end(), rng);
            for (int i = 0; i < n; ++i) {
                const long long w = (1 + static_cast<long long>(rng() % 8)) * scale;
                const long long h = (1 + static_cast<long long>(rng() % 8)) * scale;
                const bool rotatable = (rng() & 1U) != 0;
                items.push_back({w, h, static_cast<long long>(rng() % 17) - 8, rotatable, (rng() & 1U) != 0});
                if (rng() % 5 == 0) continue;
                const bool rotated = rotatable && (rng() & 1U) != 0;
                const long long x = (cells[i] % 10 * 17 + 3 + static_cast<long long>(rng() % 5)) * scale;
                const long long y = (cells[i] / 10 * 17 + 3 + static_cast<long long>(rng() % 5)) * scale;
                initial.placements[i] = {x,y,rotated?h:w,rotated?w:h,static_cast<int>(rng()%3),rotated};
            }
            recompute_rectangle_packing_summary(items, initial);
            require(validate_rectangle_packing(items, initial, 0, 0, false, nullptr, true), "compact input geometry");
            const auto result = compact_result(items, initial, {});
            require(validate_rectangle_packing(items, result, initial.used_width, initial.used_height,
                                              false, nullptr, true), "compact output geometry");
            require(result.used_width <= initial.used_width && result.used_height <= initial.used_height,
                    "compact nonincreasing bounding dimensions");
            require(result.placed_profit == initial.placed_profit && result.placed_area == initial.placed_area &&
                    result.placed_count == initial.placed_count && result.bin_count == initial.bin_count &&
                    result.missing_required_count == initial.missing_required_count, "compact summary invariants");
            for (int i = 0; i < n; ++i) {
                const auto& a = initial.placements[i]; const auto& b = result.placements[i];
                require(std::tie(a.width,a.height,a.bin,a.rotated) == std::tie(b.width,b.height,b.bin,b.rotated),
                        "compact shape and assignment invariants");
                if (!a.placed()) require(a.x == b.x && a.y == b.y, "compact unplaced unchanged");
            }
            require(same_placements(result, compact_result(items, initial, {})), "compact deterministic");
        }
    }();
    // 利益の最小値と、途中だけ64bitを超える正負の相殺を公開APIで確認する
    [&] {
        constexpr long long hi = std::numeric_limits<long long>::max();
        constexpr long long lo = std::numeric_limits<long long>::min();
        for (const auto& profits : std::vector<std::vector<long long>>{
                 {lo}, {hi,1,-hi}, {lo,-1,hi}, {hi,-hi}, {lo,hi,1}}) {
            std::vector<rectangle_pack_item> items;
            __int128_t expected = 0;
            for (long long profit : profits) {
                items.push_back({1,1,profit,false,true});
                expected += profit;
            }
            const long long width = static_cast<long long>(items.size());
            const auto result = pack_rectangles_fixed_bin(items,width,1,fast_options(11));
            require(result.placed_profit == expected, "extreme profit summary");
            require_valid(items,result,width,1,true,"extreme profit valid");
            const auto improved = improve_rectangles_fixed_bin(items,result,width,1,fast_options(12));
            require(improved.placed_profit == expected && improved.feasible(), "extreme profit improve");
            std::vector<int> ids(items.size());
            std::iota(ids.begin(),ids.end(),0);
            const auto repaired = repair_rectangles_fixed_bin(items,result,ids,width,1,fast_options(13));
            const auto repacked = repack_rectangles_fixed_bin(items,result,ids,width,1,fast_options(14));
            require_valid(items,repaired,width,1,true,"extreme profit repair");
            require_valid(items,repacked,width,1,true,"extreme profit repack");
        }
        // 負の利益の任意矩形を固定したまま、別の矩形だけを動かす
        const std::vector<rectangle_pack_item> items{{1,1,lo,false,false},{1,1,0,false,true}};
        rectangle_pack_result base;
        base.placements = {{0,0,1,1,0,false},{}};
        recompute_rectangle_packing_summary(items,base);
        const std::array<int,1> movable{1};
        const auto repaired = repair_rectangles_fixed_bin(items,base,movable,2,1,fast_options());
        require(repaired.placed_profit == lo && repaired.feasible(), "extreme locked optional profit");
        require_valid(items,repaired,2,1,true,"extreme locked valid");

        // 面積下界の切上げは、面積合計に幅-1を足さずに計算する
        const std::vector<rectangle_pack_item> tall{
            {1,hi/2,1,false,true}, {1,hi/2,1,false,true}, {1,1,1,false,true}};
        require(access::strip_height_lower_bound(tall,2) == hi/2+1, "overflow-free ceiling");
    }();

    // 検証関数は極端な不正配置をfalseで返し、未定義動作を起こさない
    [&] {
        constexpr long long hi = std::numeric_limits<long long>::max();
        for (int kind = 0; kind < 9; ++kind) {
            std::vector<rectangle_pack_item> items{{2,2,1,false,true}};
            rectangle_pack_result result;
            result.placements = {{0,0,2,2,0,false}};
            if (kind == 0) result.placements[0].x = hi;
            if (kind == 1) result.placements[0].y = hi;
            if (kind == 2) result.placements[0].width = hi;
            if (kind == 3) result.placements[0].height = hi;
            if (kind == 4) result.placements[0].bin = std::numeric_limits<int>::max();
            if (kind == 5) {
                items[0] = {0,1,1,false,false};
                result.placements[0] = {};
            }
            if (kind == 6) {
                items[0].width = result.placements[0].width = hi;
            }
            if (kind >= 7) {
                const long long value = kind == 7 ? hi : std::numeric_limits<long long>::min();
                items = {{1,1,value,false,true},{1,1,kind == 7 ? 1 : -1,false,true}};
                result.placements = {{0,0,1,1,0,false},{1,0,1,1,0,false}};
            }
            std::string error;
            require(!validate_rectangle_packing(items,result,0,0,false,&error,true) && !error.empty(),
                    "invalid extreme input rejected");
        }
        // 表現可能な端点・ビン数の最大値は、境界ぎりぎりでも受理する
        const std::vector<rectangle_pack_item> items{{1,1,1,false,true}};
        rectangle_pack_result result;
        result.placements = {{hi-1,hi-1,1,1,std::numeric_limits<int>::max()-1,false}};
        recompute_rectangle_packing_summary(items,result);
        require_valid(items,result,hi,hi,true,"maximum representable endpoint and bin");
    }();

    // 小さな座標格子で、検証関数をセル占有表と128bit集計の独立実装に照合する
    [&] {
        std::mt19937_64 random(0x37101a571ULL);
        std::size_t accepted = 0, rejected = 0;
        constexpr long long hi = std::numeric_limits<long long>::max();
        constexpr long long lo = std::numeric_limits<long long>::min();
        const std::array<long long,7> profits{lo,lo+1,-1,0,1,hi-1,hi};
        for (int trial = 0; trial < 20000; ++trial) {
            const int n = 1 + static_cast<int>(random()%5);
            std::vector<rectangle_pack_item> items;
            rectangle_pack_result result;
            bool expected = true;
            bool occupied[2][8][8]{};
            __int128_t profit_sum = 0;
            const bool require_all = (random()&1U) != 0;
            const bool allow_missing = (random()&1U) != 0;
            for (int id = 0; id < n; ++id) {
                const long long w = 1 + static_cast<long long>(random()%3);
                const long long h = 1 + static_cast<long long>(random()%3);
                const long long profit = profits[random()%profits.size()];
                const bool rotatable = (random()&1U) != 0;
                const bool required = (random()&1U) != 0;
                items.push_back({w,h,profit,rotatable,required});
                const bool rotated = rotatable && (random()&1U) != 0;
                const long long pw = rotated ? h : w, ph = rotated ? w : h;
                const int bin = static_cast<int>(random()%3)-1;
                const long long x = static_cast<long long>(random()%8);
                const long long y = static_cast<long long>(random()%8);
                result.placements.push_back({x,y,pw,ph,bin,rotated});
                if (bin < 0) {
                    if (required) ++result.missing_required_count;
                    if (require_all || (required && !allow_missing)) expected = false;
                    continue;
                }
                profit_sum += profit;
                result.placed_area += w*h;
                result.used_width = std::max(result.used_width,x+pw);
                result.used_height = std::max(result.used_height,y+ph);
                ++result.placed_count;
                result.bin_count = std::max(result.bin_count,bin+1);
                if (x+pw > 8 || y+ph > 8) {
                    expected = false;
                    continue;
                }
                for (long long yy = y; yy < y+ph; ++yy) {
                    for (long long xx = x; xx < x+pw; ++xx) {
                        if (occupied[bin][yy][xx]) expected = false;
                        occupied[bin][yy][xx] = true;
                    }
                }
            }
            if (profit_sum < lo || profit_sum > hi) expected = false;
            else result.placed_profit = static_cast<long long>(profit_sum);
            if (trial%7 == 0) {
                ++result.placed_area;
                expected = false;
            }
            const bool actual = validate_rectangle_packing(items,result,8,8,require_all,nullptr,allow_missing);
            require(actual == expected, "validator grid and wide sum oracle");
            if (actual) ++accepted;
            else ++rejected;
        }
        require(accepted > 100 && rejected > 100, "validator oracle includes both outcomes");
        std::cout << "Validator oracle: " << accepted << " accepted, " << rejected << " rejected.\n";
    }();

    std::cout << "All rectangle_packing v10 tests passed.\n";
}

#endif
