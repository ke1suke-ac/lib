#pragma once
#include <bits/stdc++.h>

// 2部グラフ最大マッチング用ヘッダオンリーライブラリ
// 左右の頂点集合を明示したCSR上で、greedy初期化、Hopcroft-Karp法、relabel型増加路法を自動選択して最大マッチングを求める
// 公開APIは入力の左側・右側基準を保ち、内部では必要に応じて小さい側を左側に反転する
//
// 典型ユースケース1: 最大マッチングのサイズとマッチング辺を出力する
// 入力の左頂点は0..L-1、右頂点は0..R-1の0-indexedとして扱う
//
// int L, R, M;
// std::cin >> L >> R >> M;
// bipartite_matching bm(L, R);
// bm.reserve_edges(M);
// for (int i = 0; i < M; ++i) {
//     int a, b;
//     std::cin >> a >> b;
//     bm.add_edge(a, b);
// }
// std::cout << bm.solve() << '\n';
// for (auto [a, b] : bm.pairs()) std::cout << a << ' ' << b << '\n';
//
// 典型ユースケース2: 各頂点の割当先をO(1)で参照する
// 例えば「左頂点iが割り当て済みか」「右頂点jを誰が使っているか」を後続処理で調べる
//
// int k = bm.solve();
// (void)k;
// const auto &left_match = bm.left_match();
// const auto &right_match = bm.right_match();
// if (left_match[i] != bipartite_matching::unmatched) {
//     int assigned_right = left_match[i];
// }
// if (right_match[j] != bipartite_matching::unmatched) {
//     int assigned_left = right_match[j];
// }
//
// 典型ユースケース3: 最小点被覆や最大独立集合を復元する
// 2部グラフでは最大マッチング後にKönigの定理に基づく最小点被覆をO(E+L+R)で得られる
//
// bm.solve();
// auto cover = bm.minimum_vertex_cover();
// auto independent = bm.maximum_independent_set();
// for (int left : cover.left) {
//     // 左側の最小点被覆頂点
// }
// for (int right : cover.right) {
//     // 右側の最小点被覆頂点
// }
//
// 典型的なモデリング例:
// ・人と仕事、問題と解法、行セグメントと列セグメントなど、一方から他方へ1対1に割り当てる最大個数を求める
// ・グリッド上の配置問題で、各候補を左右どちらかの頂点集合に分け、衝突しない最大選択数や最小除去数を求める
// ・DAGの最小パス被覆など、各頂点をin側/out側に複製して辺を張る定番変換に使う

class bipartite_matching {
public:
    static constexpr int unmatched = -1;

    struct vertex_set_pair {
        std::vector<int> left;
        std::vector<int> right;
    };

private:
    int n_left_ = 0;
    int n_right_ = 0;
    int core_left_ = 0;
    int core_right_ = 0;
    bool flipped_ = false;
    bool built_ = false;
    bool reverse_built_ = false;
    bool solved_ = false;

    std::vector<std::pair<int, int>> edges_;
    std::vector<int> offset_;
    std::vector<int> to_;
    std::vector<int> rev_offset_;
    std::vector<int> rev_to_;

    std::vector<int> core_match_left_;
    std::vector<int> core_match_right_;
    std::vector<int> match_left_;
    std::vector<int> match_right_;

    std::vector<int> level_;
    std::vector<int> iter_;
    std::vector<int> queue_;
    std::vector<int> stack_left_;
    std::vector<int> stack_right_;
    std::vector<int> active_;

    int matching_size_ = 0;
    int shortest_distance_ = unmatched;

    void decide_orientation() {
        flipped_ = n_right_ < n_left_;
        core_left_ = flipped_ ? n_right_ : n_left_;
        core_right_ = flipped_ ? n_left_ : n_right_;
    }

    int core_from_left(int left, int right) const {
        return flipped_ ? right : left;
    }

    int core_from_right(int left, int right) const {
        return flipped_ ? left : right;
    }

    void initialize_work_arrays() {
        core_match_left_.assign(core_left_, unmatched);
        core_match_right_.assign(core_right_, unmatched);
        match_left_.assign(n_left_, unmatched);
        match_right_.assign(n_right_, unmatched);
        level_.assign(std::max(core_left_, core_right_), 0);
        iter_.assign(core_left_, 0);
        queue_.assign(std::max(core_left_, core_right_), 0);
        stack_left_.assign(core_left_ + 1, 0);
        stack_right_.assign(core_left_ + 1, 0);
    }

    void build_forward() {
        decide_orientation();
        const int m = static_cast<int>(edges_.size());

        // 次数をinclusive_scanし、各頂点の末尾から詰めてCSRを作る
        offset_.assign(core_left_ + 1, 0);
        for (const auto &[left, right] : edges_) ++offset_[core_from_left(left, right)];
        std::inclusive_scan(offset_.begin(), offset_.end(), offset_.begin());
        offset_.back() = m;

        to_.assign(m, 0);
        for (const auto &[left, right] : edges_) {
            const int a = core_from_left(left, right);
            const int b = core_from_right(left, right);
            to_[--offset_[a]] = b;
        }

        initialize_work_arrays();
        built_ = true;
    }

    void build_reverse() {
        const int m = static_cast<int>(edges_.size());

        // 逆向きCSRはrelabel型戦略でだけ必要になる
        rev_offset_.assign(core_right_ + 1, 0);
        for (const auto &[left, right] : edges_) ++rev_offset_[core_from_right(left, right)];
        std::inclusive_scan(rev_offset_.begin(), rev_offset_.end(), rev_offset_.begin());
        rev_offset_.back() = m;

        rev_to_.assign(m, 0);
        for (const auto &[left, right] : edges_) {
            const int a = core_from_left(left, right);
            const int b = core_from_right(left, right);
            rev_to_[--rev_offset_[b]] = a;
        }

        reverse_built_ = true;
    }

    void build(bool need_reverse) {
        if (!built_) build_forward();
        if (need_reverse && !reverse_built_) build_reverse();
    }

    void greedy_initial_matching() {
        matching_size_ = 0;
        for (int a = 0; a < core_left_; ++a) {
            for (int eid = offset_[a]; eid < offset_[a + 1]; ++eid) {
                const int b = to_[eid];
                if (core_match_right_[b] != unmatched) continue;
                core_match_left_[a] = b;
                core_match_right_[b] = a;
                ++matching_size_;
                break;
            }
        }
    }

    bool bfs_layers() {
        std::fill(level_.begin(), level_.begin() + core_left_, unmatched);

        int head = 0;
        int tail = 0;
        for (int a = 0; a < core_left_; ++a) {
            if (core_match_left_[a] != unmatched) continue;
            level_[a] = 0;
            queue_[tail++] = a;
        }

        shortest_distance_ = unmatched;
        while (head < tail) {
            const int a = queue_[head++];
            if (shortest_distance_ != unmatched && level_[a] >= shortest_distance_) continue;

            // 左側だけに距離を持ち、右側のマッチ辺を経由して次の左側へ進む
            for (int eid = offset_[a]; eid < offset_[a + 1]; ++eid) {
                const int b = to_[eid];
                const int c = core_match_right_[b];
                if (c == unmatched) {
                    shortest_distance_ = level_[a] + 1;
                } else if (level_[c] == unmatched &&
                           (shortest_distance_ == unmatched || level_[a] + 1 < shortest_distance_)) {
                    level_[c] = level_[a] + 1;
                    queue_[tail++] = c;
                }
            }
        }
        return shortest_distance_ != unmatched;
    }

    bool dfs_iterative(int root) {
        int depth = 0;
        stack_left_[0] = root;

        while (depth >= 0) {
            const int a = stack_left_[depth];
            bool advanced = false;

            // 再帰DFS相当の探索を固定配列スタックで行う
            while (iter_[a] < offset_[a + 1]) {
                const int b = to_[iter_[a]++];
                const int c = core_match_right_[b];

                if (c == unmatched) {
                    if (level_[a] + 1 != shortest_distance_) continue;
                    stack_right_[depth] = b;
                    for (int i = depth; i >= 0; --i) {
                        const int pa = stack_left_[i];
                        const int pb = stack_right_[i];
                        core_match_left_[pa] = pb;
                        core_match_right_[pb] = pa;
                    }
                    return true;
                }

                if (level_[c] == level_[a] + 1) {
                    stack_right_[depth] = b;
                    stack_left_[++depth] = c;
                    advanced = true;
                    break;
                }
            }
            if (advanced) continue;

            // 失敗済み頂点として同一フェーズ中の再訪を抑える
            level_[a] = unmatched;
            --depth;
        }
        return false;
    }

    void hopcroft_karp_from_current_matching() {
        const int upper = std::min(core_left_, core_right_);
        while (matching_size_ < upper && bfs_layers()) {
            std::copy(offset_.begin(), offset_.begin() + core_left_, iter_.begin());

            // 現フェーズの最短増加路をまとめて反転する
            for (int a = 0; a < core_left_ && matching_size_ < upper; ++a) {
                if (core_match_left_[a] != unmatched || level_[a] != 0) continue;
                if (dfs_iterative(a)) ++matching_size_;
            }
        }
    }

    void relabel_left_bfs() {
        const int inf = core_left_ + core_right_ + 5;
        std::fill(level_.begin(), level_.begin() + core_right_, inf);

        int head = 0;
        int tail = 0;
        for (int b = 0; b < core_right_; ++b) {
            if (core_match_right_[b] != unmatched) continue;
            level_[b] = 0;
            queue_[tail++] = b;
        }

        // 未マッチ右側から逆辺とマッチ辺で右側距離ラベルを作る
        while (head < tail) {
            const int b = queue_[head++];
            for (int eid = rev_offset_[b]; eid < rev_offset_[b + 1]; ++eid) {
                const int a = rev_to_[eid];
                const int c = core_match_left_[a];
                if (c != unmatched && level_[c] > level_[b] + 2) {
                    level_[c] = level_[b] + 2;
                    queue_[tail++] = c;
                }
            }
        }
    }

    void relabel_left_from_scratch() {
        std::fill(core_match_left_.begin(), core_match_left_.end(), unmatched);
        std::fill(core_match_right_.begin(), core_match_right_.end(), unmatched);
        matching_size_ = 0;

        active_.clear();
        active_.reserve(static_cast<std::size_t>(core_left_) + to_.size());
        for (int a = 0; a < core_left_; ++a) active_.push_back(a);

        const int inf = core_left_ + core_right_ + 5;
        const int period = std::max(1, core_left_ + core_right_);
        int steps = 0;
        int active_head = 0;
        relabel_left_bfs();

        // 左側を能動側として、最も近い右側へ押し出す
        while (active_head < static_cast<int>(active_.size())) {
            if (steps == period) {
                steps = 0;
                relabel_left_bfs();
            }

            const int a = active_[active_head++];
            int best = unmatched;
            int best_level = inf;
            for (int eid = offset_[a]; eid < offset_[a + 1]; ++eid) {
                const int b = to_[eid];
                if (level_[b] < best_level) {
                    best_level = level_[b];
                    best = b;
                }
            }

            if (best != unmatched && best_level < inf) {
                const int old = core_match_right_[best];
                if (old != unmatched) {
                    core_match_left_[old] = unmatched;
                    active_.push_back(old);
                } else {
                    ++matching_size_;
                }
                core_match_left_[a] = best;
                core_match_right_[best] = a;
                level_[best] += 2;
            }
            ++steps;
        }
    }

    void relabel_right_bfs() {
        const int inf = core_left_ + core_right_ + 5;
        std::fill(level_.begin(), level_.begin() + core_left_, inf);

        int head = 0;
        int tail = 0;
        for (int a = 0; a < core_left_; ++a) {
            if (core_match_left_[a] != unmatched) continue;
            level_[a] = 0;
            queue_[tail++] = a;
        }

        // 未マッチ左側から通常辺とマッチ辺で左側距離ラベルを作る
        while (head < tail) {
            const int a = queue_[head++];
            for (int eid = offset_[a]; eid < offset_[a + 1]; ++eid) {
                const int b = to_[eid];
                const int c = core_match_right_[b];
                if (c != unmatched && level_[c] > level_[a] + 2) {
                    level_[c] = level_[a] + 2;
                    queue_[tail++] = c;
                }
            }
        }
    }

    void relabel_right_from_scratch() {
        std::fill(core_match_left_.begin(), core_match_left_.end(), unmatched);
        std::fill(core_match_right_.begin(), core_match_right_.end(), unmatched);
        matching_size_ = 0;

        active_.clear();
        active_.reserve(static_cast<std::size_t>(core_right_) + to_.size());
        for (int b = 0; b < core_right_; ++b) active_.push_back(b);

        const int inf = core_left_ + core_right_ + 5;
        const int period = std::max(1, core_left_ + core_right_);
        int steps = 0;
        int active_head = 0;
        relabel_right_bfs();

        // 右側を能動側として、最も近い左側へ押し出す
        while (active_head < static_cast<int>(active_.size())) {
            if (steps == period) {
                steps = 0;
                relabel_right_bfs();
            }

            const int b = active_[active_head++];
            int best = unmatched;
            int best_level = inf;
            for (int eid = rev_offset_[b]; eid < rev_offset_[b + 1]; ++eid) {
                const int a = rev_to_[eid];
                if (level_[a] < best_level) {
                    best_level = level_[a];
                    best = a;
                }
            }

            if (best != unmatched && best_level < inf) {
                const int old = core_match_left_[best];
                if (old != unmatched) {
                    core_match_right_[old] = unmatched;
                    active_.push_back(old);
                } else {
                    ++matching_size_;
                }
                core_match_left_[best] = b;
                core_match_right_[b] = best;
                level_[best] += 2;
            }
            ++steps;
        }
    }

    void relabel_from_scratch() {
        build(true);
        if (static_cast<long long>(core_right_) <= static_cast<long long>(core_left_) * 2) {
            relabel_right_from_scratch();
        } else {
            relabel_left_from_scratch();
        }
    }

    bool prefer_direct_relabel() {
        decide_orientation();
        const int upper = std::min(core_left_, core_right_);
        if (upper == 0) return false;
        if (core_left_ < 50000 || core_right_ < 50000) return false;
        return static_cast<long long>(edges_.size()) <= 4LL * upper;
    }

    void export_original_matches() {
        std::fill(match_left_.begin(), match_left_.end(), unmatched);
        std::fill(match_right_.begin(), match_right_.end(), unmatched);

        if (!flipped_) {
            for (int left = 0; left < core_left_; ++left) {
                const int right = core_match_left_[left];
                if (right == unmatched) continue;
                match_left_[left] = right;
                match_right_[right] = left;
            }
            return;
        }

        for (int right = 0; right < core_left_; ++right) {
            const int left = core_match_left_[right];
            if (left == unmatched) continue;
            match_left_[left] = right;
            match_right_[right] = left;
        }
    }

public:
    // 左右の頂点数を指定して空の2部グラフを作る O(L+R)
    explicit bipartite_matching(int left_size, int right_size)
        : n_left_(left_size), n_right_(right_size) {
        assert(0 <= n_left_);
        assert(0 <= n_right_);
        decide_orientation();
        match_left_.assign(n_left_, unmatched);
        match_right_.assign(n_right_, unmatched);
    }

    // 追加予定の辺数ぶんの領域を確保する O(m)
    void reserve_edges(int m) {
        assert(!built_ && !solved_);
        assert(0 <= m);
        edges_.reserve(m);
    }

    // 入力の左頂点leftと右頂点rightを結ぶ辺を追加する O(1)
    void add_edge(int left, int right) {
        assert(!built_ && !solved_);
        assert(0 <= left && left < n_left_);
        assert(0 <= right && right < n_right_);
        edges_.emplace_back(left, right);
    }

    // 最大マッチングを求め、サイズを返す O(E sqrt(L+R))またはrelabel型戦略依存
    int solve() {
        if (solved_) return matching_size_;

        if (prefer_direct_relabel()) {
            relabel_from_scratch();
        } else {
            build(false);
            greedy_initial_matching();

            const int upper = std::min(core_left_, core_right_);
            if (matching_size_ < upper) {
                const int free_right_count = core_right_ - matching_size_;
                if (free_right_count <= 64) {
                    hopcroft_karp_from_current_matching();
                } else {
                    relabel_from_scratch();
                }
            }
        }

        export_original_matches();
        edges_.clear();
        solved_ = true;
        return matching_size_;
    }

    // 現在の最大マッチングサイズを返す O(1)
    int size() const {
        assert(solved_);
        return matching_size_;
    }

    // 左頂点leftのマッチ先の右頂点を返し、存在しなければ-1を返す O(1)
    int match_left(int left) const {
        assert(solved_);
        assert(0 <= left && left < n_left_);
        return match_left_[left];
    }

    // 右頂点rightのマッチ先の左頂点を返し、存在しなければ-1を返す O(1)
    int match_right(int right) const {
        assert(solved_);
        assert(0 <= right && right < n_right_);
        return match_right_[right];
    }

    // 左頂点ごとのマッチ先配列を返す O(1)
    const std::vector<int> &left_match() const {
        assert(solved_);
        return match_left_;
    }

    // 右頂点ごとのマッチ先配列を返す O(1)
    const std::vector<int> &right_match() const {
        assert(solved_);
        return match_right_;
    }

    // マッチング辺を入力の左頂点昇順で返す O(L)
    std::vector<std::pair<int, int>> pairs() const {
        assert(solved_);
        std::vector<std::pair<int, int>> res;
        res.reserve(matching_size_);
        for (int left = 0; left < n_left_; ++left) {
            if (match_left_[left] != unmatched) res.emplace_back(left, match_left_[left]);
        }
        return res;
    }

    // 最小点被覆を入力の左右別頂点集合として返す O(E+L+R)
    vertex_set_pair minimum_vertex_cover() const {
        assert(solved_);

        std::vector<unsigned char> seen_left(core_left_, 0);
        std::vector<unsigned char> seen_right(core_right_, 0);
        std::vector<int> que(core_left_);
        int head = 0;
        int tail = 0;

        // 未マッチの内部左側から交互道で到達できる頂点を求める
        for (int a = 0; a < core_left_; ++a) {
            if (core_match_left_[a] != unmatched) continue;
            seen_left[a] = 1;
            que[tail++] = a;
        }
        while (head < tail) {
            const int a = que[head++];
            const int matched = core_match_left_[a];
            for (int eid = offset_[a]; eid < offset_[a + 1]; ++eid) {
                const int b = to_[eid];
                if (b == matched || seen_right[b]) continue;
                seen_right[b] = 1;
                const int c = core_match_right_[b];
                if (c != unmatched && !seen_left[c]) {
                    seen_left[c] = 1;
                    que[tail++] = c;
                }
            }
        }

        vertex_set_pair cover;
        if (!flipped_) {
            for (int left = 0; left < core_left_; ++left) {
                if (!seen_left[left]) cover.left.push_back(left);
            }
            for (int right = 0; right < core_right_; ++right) {
                if (seen_right[right]) cover.right.push_back(right);
            }
        } else {
            for (int left = 0; left < core_right_; ++left) {
                if (seen_right[left]) cover.left.push_back(left);
            }
            for (int right = 0; right < core_left_; ++right) {
                if (!seen_left[right]) cover.right.push_back(right);
            }
        }
        return cover;
    }

    // 最大独立集合を入力の左右別頂点集合として返す O(E+L+R)
    vertex_set_pair maximum_independent_set() const {
        assert(solved_);
        const vertex_set_pair cover = minimum_vertex_cover();

        // 最小点被覆の補集合として最大独立集合を構成する
        std::vector<unsigned char> in_left_cover(n_left_, 0);
        std::vector<unsigned char> in_right_cover(n_right_, 0);
        for (int left : cover.left) in_left_cover[left] = 1;
        for (int right : cover.right) in_right_cover[right] = 1;

        vertex_set_pair independent;
        independent.left.reserve(n_left_ - static_cast<int>(cover.left.size()));
        independent.right.reserve(n_right_ - static_cast<int>(cover.right.size()));
        for (int left = 0; left < n_left_; ++left) {
            if (!in_left_cover[left]) independent.left.push_back(left);
        }
        for (int right = 0; right < n_right_; ++right) {
            if (!in_right_cover[right]) independent.right.push_back(right);
        }
        return independent;
    }
};

#if __INCLUDE_LEVEL__ == 0

namespace {

void check(bool condition) {
    if (!condition) std::abort();
}

int naive_matching(int n_left, int n_right, const std::vector<std::pair<int, int>> &edges) {
    std::vector<std::vector<int>> graph(n_left);
    for (const auto &[left, right] : edges) graph[left].push_back(right);

    std::vector<int> mate_right(n_right, -1);
    int result = 0;
    for (int root = 0; root < n_left; ++root) {
        std::vector<unsigned char> seen(n_right, 0);
        auto dfs = [&](auto &&self, int left) -> bool {
            for (int right : graph[left]) {
                if (seen[right]) continue;
                seen[right] = 1;
                if (mate_right[right] == -1 || self(self, mate_right[right])) {
                    mate_right[right] = left;
                    return true;
                }
            }
            return false;
        };
        if (dfs(dfs, root)) ++result;
    }
    return result;
}

void verify_result(int n_left, int n_right, const std::vector<std::pair<int, int>> &edges) {
    bipartite_matching bm(n_left, n_right);
    bm.reserve_edges(static_cast<int>(edges.size()));
    for (const auto &[left, right] : edges) bm.add_edge(left, right);

    const int actual = bm.solve();
    const int expected = naive_matching(n_left, n_right, edges);
    check(actual == expected);
    check(bm.size() == expected);

    std::vector<std::pair<int, int>> sorted_edges = edges;
    std::sort(sorted_edges.begin(), sorted_edges.end());
    sorted_edges.erase(std::unique(sorted_edges.begin(), sorted_edges.end()), sorted_edges.end());

    int pair_count = 0;
    for (int left = 0; left < n_left; ++left) {
        const int right = bm.match_left(left);
        if (right == -1) continue;
        ++pair_count;
        check(0 <= right && right < n_right);
        check(bm.match_right(right) == left);
        check(std::binary_search(sorted_edges.begin(), sorted_edges.end(), std::pair<int, int>{left, right}));
    }
    for (int right = 0; right < n_right; ++right) {
        const int left = bm.match_right(right);
        if (left == -1) continue;
        check(0 <= left && left < n_left);
        check(bm.match_left(left) == right);
    }
    check(pair_count == expected);
    check(static_cast<int>(bm.pairs().size()) == expected);

    const auto cover = bm.minimum_vertex_cover();
    check(static_cast<int>(cover.left.size() + cover.right.size()) == expected);
    std::vector<unsigned char> left_cover(n_left, 0), right_cover(n_right, 0);
    for (int left : cover.left) left_cover[left] = 1;
    for (int right : cover.right) right_cover[right] = 1;
    for (const auto &[left, right] : edges) check(left_cover[left] || right_cover[right]);

    const auto independent = bm.maximum_independent_set();
    check(static_cast<int>(independent.left.size() + independent.right.size()) == n_left + n_right - expected);
}

void fixed_tests() {
    verify_result(0, 0, {});
    verify_result(3, 0, {});
    verify_result(0, 3, {});
    verify_result(1, 1, {{0, 0}});
    verify_result(4, 4, {{1, 1}, {2, 2}, {0, 0}, {3, 1}, {1, 2}, {2, 0}, {3, 2}});
    verify_result(4, 3, {{0, 0}, {0, 1}, {1, 1}, {2, 2}, {3, 2}});
    verify_result(3, 4, {{0, 0}, {0, 0}, {1, 1}, {2, 2}, {2, 3}});

    std::vector<std::pair<int, int>> complete;
    for (int left = 0; left < 5; ++left) {
        for (int right = 0; right < 7; ++right) complete.emplace_back(left, right);
    }
    verify_result(5, 7, complete);
}

void exhaustive_small_tests() {
    for (int n_left = 0; n_left <= 4; ++n_left) {
        for (int n_right = 0; n_right <= 4; ++n_right) {
            const int edge_count = n_left * n_right;
            if (edge_count > 16) continue;
            const int masks = 1 << edge_count;
            for (int mask = 0; mask < masks; ++mask) {
                std::vector<std::pair<int, int>> edges;
                for (int id = 0; id < edge_count; ++id) {
                    if (((mask >> id) & 1) == 0) continue;
                    edges.emplace_back(id / n_right, id % n_right);
                }
                verify_result(n_left, n_right, edges);
            }
        }
    }
}

void random_tests() {
    std::mt19937 rng(1234567);
    for (int tc = 0; tc < 1500; ++tc) {
        const int n_left = static_cast<int>(rng() % 9);
        const int n_right = static_cast<int>(rng() % 9);
        std::vector<std::pair<int, int>> edges;
        for (int left = 0; left < n_left; ++left) {
            for (int right = 0; right < n_right; ++right) {
                if ((rng() & 3U) != 0U) edges.emplace_back(left, right);
            }
        }
        if (!edges.empty() && (rng() & 1U) != 0U) edges.push_back(edges[rng() % edges.size()]);
        verify_result(n_left, n_right, edges);
    }
}


void medium_relabel_tests() {
    std::mt19937 rng(7654321);

    for (int tc = 0; tc < 80; ++tc) {
        const int n_left = 120;
        const int n_right = 200;
        std::vector<std::pair<int, int>> edges;
        for (int left = 0; left < 60; ++left) {
            for (int k = 0; k < 5; ++k) edges.emplace_back(left, static_cast<int>(rng() % n_right));
        }
        verify_result(n_left, n_right, edges);
    }

    for (int tc = 0; tc < 80; ++tc) {
        const int n_left = 400;
        const int n_right = 100;
        std::vector<std::pair<int, int>> edges;
        for (int left = 0; left < n_left; ++left) {
            for (int k = 0; k < 2; ++k) edges.emplace_back(left, static_cast<int>(rng() % 50));
        }
        verify_result(n_left, n_right, edges);
    }
}

} // namespace

int main() {
    fixed_tests();
    exhaustive_small_tests();
    random_tests();
    medium_relabel_tests();
    return 0;
}

#endif
