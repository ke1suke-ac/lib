/*
 * project_selection: ACL の最大フローを使い、二値のプロジェクト選択・燃やす埋める問題を解くライブラリ
 *
 * 【状態と目的関数】
 * 各変数の状態は、true が source 側、false が sink 側となる。
 * プロジェクト選択では true を「選ぶ」、燃やす埋めるでは true を「燃やす」とみなす。
 * source -> v の容量は v=false のコスト、v -> sink の容量は v=true のコスト、
 * u -> v の容量は u=true かつ v=false のコストを表す。
 * project_selection は追加された全スコアの合計を最大化し、追加された全コストの合計をそこから引く。
 * solve() は最大値 max_score と最適状態 state を返すため、コストだけを追加した問題の最小コストは
 * -result.max_score となる。ハード制約が矛盾する場合は result.feasible=false となる。
 *
 * 【典型ユースケース】
 * ・利益、導入費用、依存関係を持つプロジェクト・機能・商品・設備の選択
 * ・有向グラフ上の最大重み閉包。依存関係に閉路があってもよい
 * ・燃やすか埋めるか、採用か不採用か、0か1かを決める二値ラベリング
 * ・画像分割、グラフ上の平滑化、隣接要素の状態が異なる場合の境界コスト
 * ・どれかを使うと一度だけ必要になる共通設備費・契約費・起動費
 * ・全てを採用したときのセットボーナス、全てを不採用にしたときのボーナス
 * ・強化レベル、購入prefix長、研究段階などの単調な多段階選択
 * ・二部グラフの最小重み頂点被覆、最大重み独立集合、二部構造の競合付き選択
 *
 * 【project_selection の全 public メソッドを使う例】
 *
 * 1. 定数、単項スコア、単項コスト
 *   project_selection<long long> ps(4);       // 4 個の二値変数を作る
 *   ps.add_constant_score(2);                 // 状態によらずスコア +2
 *   ps.add_score(0, -3, 10);                 // 0=false なら -3、true なら +10
 *   ps.add_costs(1, 6, 2);                   // 1=false なら費用6、true なら費用2
 *   ps.add_gain(2, true, 5);                 // 2=true のとき利益5
 *   ps.add_cost(3, false, 4);                // 3=false のとき費用4
 *   auto result = ps.solve();                // 最大スコアと元の4変数の状態を得る
 *
 * 2. 強制、必須依存、違反可能な依存、同値制約
 *   project_selection<long long> ps(4);
 *   ps.force(0, true);                       // 0 を必ず選ぶ
 *   ps.add_implication(0, 1);                // 0 を選ぶなら 1 も必ず選ぶ
 *   ps.add_soft_implication(1, 2, 7);        // 1=true, 2=false なら費用7
 *   ps.add_equal(2, 3);                      // 2 と 3 を同じ状態にする
 *   auto result = ps.solve();
 *   // result.feasible=false ならハード制約が矛盾している
 *
 * 3. 二頂点の関係と一般の二変数項
 *   project_selection<long long> ps(3);
 *   ps.add_cost_if_different(0, 1, 5);       // 0 と 1 が異なると費用5
 *   ps.add_gain_if_same(1, 2, 4);            // 1 と 2 が同じなら利益4
 *   ps.add_pairwise_cost(0, 1, 0, 4, 6, 2); // 状態00,01,10,11のコスト
 *   ps.add_pairwise_score(1, 2, 3, 0, 1, 4);// 状態00,01,10,11のスコア
 *
 * 4. 集合全体に対するセットボーナスと共通費用
 *   std::vector<int> projects{0, 1, 2};
 *   project_selection<long long> ps(3);
 *   ps.add_gain_if_all(projects, true, 20);  // 全て選ぶと利益20
 *   ps.add_cost_if_any(projects, true, 12);  // 一つでも選ぶと共通費用12
 *   ps.add_gain_if_all(projects, false, 5);  // 全て不採用なら利益5
 *   ps.add_cost_if_any(projects, false, 3);  // 一つでも不採用なら費用3
 * 集合の頂点は相異なるものとする。空集合では all 条件は成立し、any 条件は成立しない。
 *
 * 【ユースケースソルバーの使用例】
 *
 * 1. 最大重み閉包。weight は選択時の利益または負の費用、(u,v) は u を選ぶなら v も選ぶ制約
 *   std::vector<long long> weight{8, -3, 6};
 *   std::vector<std::pair<int, int>> requirements{{0, 1}};
 *   auto closure = maximum_weight_closure(weight, requirements);
 *   // closure.maximum_weight, closure.selected
 *
 * 2. 燃やす埋める・二値ラベリング。unary[v][0] が埋める費用、unary[v][1] が燃やす費用
 *   std::vector<std::array<long long, 2>> unary{{5, 2}, {1, 4}};
 *   std::vector<binary_pairwise_cost<long long>> pairwise{
 *       {0, 1, 0, 3, 3, 0}                  // 異なる状態なら費用3
 *   };
 *   auto labeling = minimum_binary_labeling(unary, pairwise);
 *   // labeling.minimum_cost, labeling.state
 *
 * 3. 多段階選択。score[i][k] は項目 i をちょうどレベル k にしたときのスコア
 *   std::vector<std::vector<long long>> score{{0, 4, 7}, {0, 3, 5}};
 *   std::vector<monotone_level_implication> requirements{
 *       {0, 2, 1, 1}                         // 項目0がレベル2以上なら項目1をレベル1以上にする
 *   };
 *   auto levels = maximum_score_monotone_levels(score, requirements);
 *   // levels.maximum_score, levels.level
 *
 * 4. 二部グラフの重み付き頂点被覆と、競合しない最大重み集合
 *   std::vector<long long> left_weight{4, 7}, right_weight{5, 2};
 *   std::vector<std::pair<int, int>> edges{{0, 0}, {1, 0}, {1, 1}};
 *   auto cover = minimum_weight_bipartite_vertex_cover(left_weight, right_weight, edges);
 *   auto independent = maximum_weight_bipartite_independent_set(left_weight, right_weight, edges);
 *
 * 【このライブラリを直接使える条件】
 * ・目的関数が、定数項、各二値変数の単項項、以下の表現可能な相互作用の和に分解できる
 * ・各状態の利益・費用は正負どちらでもよい。add_score() と add_costs() は符号付き値を扱える
 * ・x=true なら y=true というハード依存、またはその違反時の有限ペナルティ
 * ・変数の強制、二変数の同値制約。依存グラフの閉路は同値関係として扱える
 * ・二変数が異なる場合のコスト、同じ場合の利益
 * ・全頂点が同じ指定状態の場合の利益、いずれかが同じ指定状態の場合の共通コスト
 * ・一般二変数コストが cost00 + cost11 <= cost01 + cost10 を満たす
 * ・一般二変数スコアが score00 + score11 >= score01 + score10 を満たす
 * ・多段階変数を「レベル k 以上か」という二値変数列に分解でき、依存関係が単調である
 * ・劣モジュラでない競合でも、相互作用グラフが二部グラフなら片側の状態を反転できる場合がある
 *   最大重み二部独立集合と最小重み二部頂点被覆は、この特殊変換を専用ソルバーで行う
 *
 * 一般二変数コストの表は、行を u=0,1、列を v=0,1 として次の順で渡す。
 *   cost00  cost01
 *   cost10  cost11
 * 複数の項が同じ二変数にかかる場合、項ごとでなく合計した表が劣モジュラなら表現できる。
 * 変数の true/false の意味を反転できる場合は、反転後の全ての項について条件を判定する。
 *
 * 【このライブラリでは一般には表現できない条件】
 * ・cost00 + cost11 > cost01 + cost10 となる、劣モジュラでない二変数コスト
 * ・両方を選ぶと罰金、または両方を選ぶことを禁止する一般グラフ上の競合制約
 * ・両方を選ばないと罰金、または少なくとも一方を選ぶという一般の OR 制約
 * ・異なる状態なら利益、同じ状態ならコスト、二変数を必ず異なる状態にする制約
 * ・x=true なら y=false、または x=false なら y=true という向きのハード含意
 * ・集合のいずれかが指定状態なら利益、または全てが指定状態ならコスト
 * ・集合内で高々一つ、少なくとも一つ、ちょうど一つを選ぶ制約
 * ・選択数を K 以下、K 以上、ちょうど K にする一般の個数制約
 * ・異なる指定状態が混在する集合に対する任意の AND、OR、排他条件
 * ・単調な閾値列へ分解できない多値変数、任意の多値間相互作用
 *
 * 上記の「一般には表現できない」条件でも、グラフが二部、木、列、区間などの特殊構造を持つ場合や、
 * 変数の意味を全体で矛盾なく反転できる場合は、状態反転、DP、最小費用流などで解けることがある。
 * 一般グラフ上の「両方を選べない」を扱えると最大重み独立集合を解けるため、単一の s-t min-cut では扱えない。
 *
 * 【容量型とオーバーフローの前提】
 * オーバーフロー検査は行わない。
 * Cap は ACL の mf_graph で使う容量型で、通常は int または long long を指定する。
 * Calc はスコア差、二変数項の変換、容量合計などの中間計算型で、デフォルトは long long とする。
 * 各辺容量、全有限容量の合計 + 1、最終結果は Cap に収まり、変換途中の式は Calc に収まることを前提とする。
 * 値が long long の範囲近くにあり、途中の加減算だけが範囲を超え得る場合は、
 * project_selection<long long, __int128_t> のように Calc を広げる。
 */
#pragma once

#include <bits/stdc++.h>
#include <atcoder/maxflow>

template <class Cap = long long, class Calc = long long>
struct project_selection {
    struct result {
        bool feasible;
        Cap max_score;
        std::vector<bool> state;
    };

private:
    struct finite_edge {
        int from;
        int to;
        Cap capacity;
    };

    int original_vertex_count_;
    int vertex_count_;
    Calc base_score_ = 0;
    std::vector<Cap> source_capacity_;
    std::vector<Cap> sink_capacity_;
    std::vector<finite_edge> finite_edges_;
    std::vector<std::pair<int, int>> hard_edges_;
    std::vector<int> hard_source_edges_;
    std::vector<int> hard_sink_edges_;

    void check_original_vertex(int v) const {
        assert(0 <= v && v < original_vertex_count_);
        (void)v;
    }

    void add_source_capacity(int v, Calc capacity) {
        assert(capacity >= 0);
        source_capacity_[v] += static_cast<Cap>(capacity);
    }

    void add_sink_capacity(int v, Calc capacity) {
        assert(capacity >= 0);
        sink_capacity_[v] += static_cast<Cap>(capacity);
    }

    void add_finite_edge(int from, int to, Calc capacity) {
        assert(capacity >= 0);
        if (capacity == 0) return;
        finite_edges_.push_back({from, to, static_cast<Cap>(capacity)});
    }

    void add_hard_edge(int from, int to) {
        hard_edges_.push_back({from, to});
    }

    int add_auxiliary_vertex() {
        source_capacity_.push_back(Cap{});
        sink_capacity_.push_back(Cap{});
        return vertex_count_++;
    }

    void add_score_internal(int v, Calc score0, Calc score1) {
        if (score0 <= score1) {
            base_score_ += score1;
            add_source_capacity(v, score1 - score0);
        } else {
            base_score_ += score0;
            add_sink_capacity(v, score0 - score1);
        }
    }

    void add_costs_internal(int v, Calc cost0, Calc cost1) {
        if (cost0 <= cost1) {
            base_score_ -= cost0;
            add_sink_capacity(v, cost1 - cost0);
        } else {
            base_score_ -= cost1;
            add_source_capacity(v, cost0 - cost1);
        }
    }

    void add_gain_internal(int v, bool state, Cap gain) {
        assert(gain >= 0);
        base_score_ += gain;
        if (state) {
            add_source_capacity(v, gain);
        } else {
            add_sink_capacity(v, gain);
        }
    }

    void add_cost_internal(int v, bool state, Cap cost) {
        assert(cost >= 0);
        if (state) {
            add_sink_capacity(v, cost);
        } else {
            add_source_capacity(v, cost);
        }
    }

public:
    // n 個の二値変数を持つ空の問題を構築する。O(n)
    explicit project_selection(int n)
        : original_vertex_count_(n),
          vertex_count_(n),
          source_capacity_(n, Cap{}),
          sink_capacity_(n, Cap{}) {
        assert(n >= 0);
    }

    // 全ての割り当てに共通する score を目的関数へ加える。O(1)
    void add_constant_score(Cap score) {
        base_score_ += score;
    }

    // v=false なら score0、v=true なら score1 を目的関数へ加える。O(1)
    void add_score(int v, Cap score0, Cap score1) {
        check_original_vertex(v);
        add_score_internal(v, score0, score1);
    }

    // v=false なら cost0、v=true なら cost1 を目的関数から引く。O(1)
    void add_costs(int v, Cap cost0, Cap cost1) {
        check_original_vertex(v);
        add_costs_internal(v, cost0, cost1);
    }

    // v が state のとき非負の gain を目的関数へ加える。O(1)
    void add_gain(int v, bool state, Cap gain) {
        check_original_vertex(v);
        add_gain_internal(v, state, gain);
    }

    // v が state のとき非負の cost を目的関数から引く。O(1)
    void add_cost(int v, bool state, Cap cost) {
        check_original_vertex(v);
        add_cost_internal(v, state, cost);
    }

    // v を state に固定する。O(1)
    void force(int v, bool state) {
        check_original_vertex(v);
        if (state) {
            hard_source_edges_.push_back(v);
        } else {
            hard_sink_edges_.push_back(v);
        }
    }

    // from=true なら to=true を必須とする。O(1)
    void add_implication(int from, int to) {
        check_original_vertex(from);
        check_original_vertex(to);
        add_hard_edge(from, to);
    }

    // from=true かつ to=false のとき非負の cost を目的関数から引く。O(1)
    void add_soft_implication(int from, int to, Cap cost) {
        check_original_vertex(from);
        check_original_vertex(to);
        assert(cost >= 0);
        add_finite_edge(from, to, cost);
    }

    // u と v を同じ状態に固定する。O(1)
    void add_equal(int u, int v) {
        check_original_vertex(u);
        check_original_vertex(v);
        add_hard_edge(u, v);
        add_hard_edge(v, u);
    }

    // u と v の状態が異なるとき非負の cost を目的関数から引く。O(1)
    void add_cost_if_different(int u, int v, Cap cost) {
        check_original_vertex(u);
        check_original_vertex(v);
        assert(cost >= 0);
        add_finite_edge(u, v, cost);
        add_finite_edge(v, u, cost);
    }

    // u と v の状態が同じとき非負の gain を目的関数へ加える。O(1)
    void add_gain_if_same(int u, int v, Cap gain) {
        check_original_vertex(u);
        check_original_vertex(v);
        assert(gain >= 0);
        base_score_ += gain;
        add_finite_edge(u, v, gain);
        add_finite_edge(v, u, gain);
    }

    // (u, v) の各状態に対するスコアを加える。score00 + score11 >= score01 + score10 が必要。O(1)
    void add_pairwise_score(
        int u,
        int v,
        Cap score00,
        Cap score01,
        Cap score10,
        Cap score11) {
        check_original_vertex(u);
        check_original_vertex(v);
        assert(u != v);

        // スコアを定数項、二つの単項スコア、u=true かつ v=false のペナルティへ分解する
        const Calc s00 = score00;
        const Calc s01 = score01;
        const Calc s10 = score10;
        const Calc s11 = score11;
        const Calc capacity = s00 + s11 - s01 - s10;
        assert(capacity >= 0);

        base_score_ += s00;
        add_score_internal(u, Calc{}, s11 - s01);
        add_score_internal(v, Calc{}, s01 - s00);
        add_finite_edge(u, v, capacity);
    }

    // (u, v) の各状態に対するコストを引く。cost00 + cost11 <= cost01 + cost10 が必要。O(1)
    void add_pairwise_cost(
        int u,
        int v,
        Cap cost00,
        Cap cost01,
        Cap cost10,
        Cap cost11) {
        check_original_vertex(u);
        check_original_vertex(v);
        assert(u != v);

        // コストの符号を反転し、定数項、二つの単項スコア、有向ペナルティへ分解する
        const Calc c00 = cost00;
        const Calc c01 = cost01;
        const Calc c10 = cost10;
        const Calc c11 = cost11;
        const Calc capacity = c01 + c10 - c00 - c11;
        assert(capacity >= 0);

        base_score_ -= c00;
        add_score_internal(u, Calc{}, c01 - c11);
        add_score_internal(v, Calc{}, c00 - c01);
        add_finite_edge(u, v, capacity);
    }

    // vertices の全頂点が state のとき非負の gain を加える。頂点は相異なること。O(vertices.size())
    void add_gain_if_all(const std::vector<int>& vertices, bool state, Cap gain) {
        assert(gain >= 0);
        for (int v : vertices) check_original_vertex(v);

        // 空集合では条件が常に成立し、1 頂点または 2 頂点では補助頂点を使わず直接表現する
        if (vertices.empty()) {
            add_constant_score(gain);
            return;
        }
        if (vertices.size() == 1) {
            add_gain(vertices[0], state, gain);
            return;
        }
        if (vertices.size() == 2) {
            if (state) {
                add_pairwise_score(vertices[0], vertices[1], Cap{}, Cap{}, Cap{}, gain);
            } else {
                add_pairwise_score(vertices[0], vertices[1], gain, Cap{}, Cap{}, Cap{});
            }
            return;
        }

        // 補助変数が報酬を得られる状態を、全頂点が指定状態のときだけ選択可能にする
        const int auxiliary = add_auxiliary_vertex();
        add_gain_internal(auxiliary, state, gain);
        if (state) {
            for (int v : vertices) add_hard_edge(auxiliary, v);
        } else {
            for (int v : vertices) add_hard_edge(v, auxiliary);
        }
    }

    // vertices のいずれかが state のとき非負の cost を引く。頂点は相異なること。O(vertices.size())
    void add_cost_if_any(const std::vector<int>& vertices, bool state, Cap cost) {
        assert(cost >= 0);
        for (int v : vertices) check_original_vertex(v);

        // 空集合では条件が成立せず、1 頂点または 2 頂点では補助頂点を使わず直接表現する
        if (vertices.empty() || cost == 0) return;
        if (vertices.size() == 1) {
            add_cost(vertices[0], state, cost);
            return;
        }
        if (vertices.size() == 2) {
            if (state) {
                add_pairwise_cost(vertices[0], vertices[1], Cap{}, cost, cost, cost);
            } else {
                add_pairwise_cost(vertices[0], vertices[1], cost, cost, cost, Cap{});
            }
            return;
        }

        // 補助変数に共通費用を持たせ、いずれかの頂点が指定状態なら同じ状態を強制する
        const int auxiliary = add_auxiliary_vertex();
        add_cost_internal(auxiliary, state, cost);
        if (state) {
            for (int v : vertices) add_hard_edge(v, auxiliary);
        } else {
            for (int v : vertices) add_hard_edge(auxiliary, v);
        }
    }

    // 最大スコアと元の n 変数の最適状態を返す。ハード制約が矛盾する場合は feasible=false。最大フロー計算量に準ずる
    result solve() const {
        // 有限容量の総和より 1 大きい容量をハード制約に使う
        Calc finite_sum = 0;
        for (int v = 0; v < vertex_count_; ++v) {
            finite_sum += source_capacity_[v];
            finite_sum += sink_capacity_[v];
        }
        for (const finite_edge& edge : finite_edges_) {
            finite_sum += edge.capacity;
        }
        const Cap infinity = static_cast<Cap>(finite_sum + 1);

        // 蓄積した単項容量、有限辺、ハード制約を ACL の最大フローグラフへ変換する
        const int source = vertex_count_;
        const int sink = source + 1;
        atcoder::mf_graph<Cap> graph(vertex_count_ + 2);
        for (int v = 0; v < vertex_count_; ++v) {
            if (source_capacity_[v] != 0) graph.add_edge(source, v, source_capacity_[v]);
            if (sink_capacity_[v] != 0) graph.add_edge(v, sink, sink_capacity_[v]);
        }
        for (const finite_edge& edge : finite_edges_) {
            graph.add_edge(edge.from, edge.to, edge.capacity);
        }
        for (const auto& [from, to] : hard_edges_) graph.add_edge(from, to, infinity);
        for (int v : hard_source_edges_) graph.add_edge(source, v, infinity);
        for (int v : hard_sink_edges_) graph.add_edge(v, sink, infinity);

        // 流量上限まで流れた場合は、全てのハード制約を満たすカットが存在しない
        const Cap flow = graph.flow(source, sink, infinity);
        if (flow == infinity) return {false, Cap{}, {}};

        // 残余グラフで source から到達可能な元の頂点を true として復元する
        const std::vector<bool> cut = graph.min_cut(source);
        std::vector<bool> state(original_vertex_count_);
        for (int v = 0; v < original_vertex_count_; ++v) state[v] = cut[v];
        const Cap max_score = static_cast<Cap>(base_score_ - flow);
        return {true, max_score, std::move(state)};
    }
};

template <class Cap>
struct maximum_weight_closure_result {
    Cap maximum_weight;
    std::vector<bool> selected;
};

/*
 * 想定ユースケース: 利益と費用を持つプロジェクトを、選択の依存関係を守りながら選ぶ最大重み閉包問題
 * 入力: weight[v] は v を選ぶスコア、implications の (u, v) は u を選ぶなら v も選ぶ制約
 * 出力: 最大重みと、各頂点を選んだかを返す
 * 処理量: V=n+2、E=O(n+implications.size()) とした ACL 最大フロー計算量
 */
template <class Cap, class Calc = long long>
maximum_weight_closure_result<Cap> maximum_weight_closure(
    const std::vector<Cap>& weight,
    const std::vector<std::pair<int, int>>& implications) {
    const int n = static_cast<int>(weight.size());
    project_selection<Cap, Calc> ps(n);
    for (int v = 0; v < n; ++v) ps.add_score(v, Cap{}, weight[v]);
    for (const auto& [from, to] : implications) ps.add_implication(from, to);

    auto answer = ps.solve();
    assert(answer.feasible);
    return {answer.max_score, std::move(answer.state)};
}

template <class Cap>
struct binary_pairwise_cost {
    int u;
    int v;
    Cap cost00;
    Cap cost01;
    Cap cost10;
    Cap cost11;
};

template <class Cap>
struct minimum_binary_labeling_result {
    Cap minimum_cost;
    std::vector<bool> state;
};

/*
 * 想定ユースケース: 燃やす埋める、二値画像分割、頂点の二分類などの二値ラベリング問題
 * 入力: unary_cost[v][state] は単項コスト、pairwise_cost は劣モジュラな二変数コスト
 * 出力: 最小コストと、false=0・true=1 で表した各頂点の状態を返す
 * 処理量: V=n+2、E=O(n+pairwise_cost.size()) とした ACL 最大フロー計算量
 */
template <class Cap, class Calc = long long>
minimum_binary_labeling_result<Cap> minimum_binary_labeling(
    const std::vector<std::array<Cap, 2>>& unary_cost,
    const std::vector<binary_pairwise_cost<Cap>>& pairwise_cost) {
    const int n = static_cast<int>(unary_cost.size());
    project_selection<Cap, Calc> ps(n);
    for (int v = 0; v < n; ++v) {
        ps.add_costs(v, unary_cost[v][0], unary_cost[v][1]);
    }
    for (const auto& term : pairwise_cost) {
        ps.add_pairwise_cost(
            term.u,
            term.v,
            term.cost00,
            term.cost01,
            term.cost10,
            term.cost11);
    }

    auto answer = ps.solve();
    assert(answer.feasible);
    const Cap minimum_cost = static_cast<Cap>(-static_cast<Calc>(answer.max_score));
    return {minimum_cost, std::move(answer.state)};
}

struct monotone_level_implication {
    int from;
    int from_level;
    int to;
    int to_level;
};

template <class Cap>
struct maximum_score_monotone_levels_result {
    Cap maximum_score;
    std::vector<int> level;
};

/*
 * 想定ユースケース: 各項目の強化段階やprefix長を選び、上位段階間の依存関係を課す多段階選択
 * 入力: score[i][k] は項目 i のレベルをちょうど k にするスコア
 *       implication は level[from]>=from_level なら level[to]>=to_level という制約
 *       from_level と to_level はともに 1 以上で、それぞれの最大レベル以下とする
 * 出力: 最大スコアと各項目の選択レベルを返す
 * 処理量: B=sum(score[i].size()-1)、E=O(B+implications.size()) とした B+2 頂点の ACL 最大フロー計算量
 */
template <class Cap, class Calc = long long>
maximum_score_monotone_levels_result<Cap> maximum_score_monotone_levels(
    const std::vector<std::vector<Cap>>& score,
    const std::vector<monotone_level_implication>& implications) {
    const int n = static_cast<int>(score.size());
    std::vector<int> first_vertex(n + 1);
    for (int i = 0; i < n; ++i) {
        assert(!score[i].empty());
        first_vertex[i + 1] =
            first_vertex[i] + static_cast<int>(score[i].size()) - 1;
    }

    auto threshold_vertex = [&](int item, int level) {
        assert(0 <= item && item < n);
        assert(1 <= level && level < static_cast<int>(score[item].size()));
        return first_vertex[item] + level - 1;
    };

    project_selection<Cap, Calc> ps(first_vertex[n]);
    for (int item = 0; item < n; ++item) {
        ps.add_constant_score(score[item][0]);
        const int maximum_level = static_cast<int>(score[item].size()) - 1;
        for (int level = 1; level <= maximum_level; ++level) {
            const int vertex = threshold_vertex(item, level);
            const Calc difference =
                static_cast<Calc>(score[item][level]) -
                static_cast<Calc>(score[item][level - 1]);
            ps.add_score(vertex, Cap{}, static_cast<Cap>(difference));
            if (level >= 2) {
                ps.add_implication(vertex, threshold_vertex(item, level - 1));
            }
        }
    }
    for (const auto& implication : implications) {
        ps.add_implication(
            threshold_vertex(implication.from, implication.from_level),
            threshold_vertex(implication.to, implication.to_level));
    }

    auto answer = ps.solve();
    assert(answer.feasible);
    std::vector<int> level(n);
    for (int item = 0; item < n; ++item) {
        const int maximum_level = static_cast<int>(score[item].size()) - 1;
        for (int candidate = 1; candidate <= maximum_level; ++candidate) {
            if (answer.state[threshold_vertex(item, candidate)]) level[item] = candidate;
        }
    }
    return {answer.max_score, std::move(level)};
}

template <class Cap>
struct minimum_weight_bipartite_vertex_cover_result {
    Cap minimum_weight;
    std::vector<bool> left_in_cover;
    std::vector<bool> right_in_cover;
};

/*
 * 想定ユースケース: 二部グラフの全辺を少なくとも一方の端点で覆う最小重み頂点被覆
 * 入力: 左右の頂点重みと、0-indexed の (left, right) 辺
 * 出力: 最小重みと、左右それぞれの頂点を被覆へ入れたかを返す
 * 処理量: V=left_size+right_size+2、E=O(V+edges.size()) とした ACL 最大フロー計算量
 */
template <class Cap, class Calc = long long>
minimum_weight_bipartite_vertex_cover_result<Cap> minimum_weight_bipartite_vertex_cover(
    const std::vector<Cap>& left_weight,
    const std::vector<Cap>& right_weight,
    const std::vector<std::pair<int, int>>& edges) {
    const int left_size = static_cast<int>(left_weight.size());
    const int right_size = static_cast<int>(right_weight.size());
    project_selection<Cap, Calc> ps(left_size + right_size);

    // 左は false、右は true を被覆へ入れた状態とし、左を入れないなら右を入れる制約にする
    for (int left = 0; left < left_size; ++left) {
        ps.add_costs(left, left_weight[left], Cap{});
    }
    for (int right = 0; right < right_size; ++right) {
        ps.add_costs(left_size + right, Cap{}, right_weight[right]);
    }
    for (const auto& [left, right] : edges) {
        ps.add_implication(left, left_size + right);
    }

    auto answer = ps.solve();
    assert(answer.feasible);
    std::vector<bool> left_in_cover(left_size);
    std::vector<bool> right_in_cover(right_size);
    for (int left = 0; left < left_size; ++left) {
        left_in_cover[left] = !answer.state[left];
    }
    for (int right = 0; right < right_size; ++right) {
        right_in_cover[right] = answer.state[left_size + right];
    }
    const Cap minimum_weight = static_cast<Cap>(-static_cast<Calc>(answer.max_score));
    return {minimum_weight, std::move(left_in_cover), std::move(right_in_cover)};
}

template <class Cap>
struct maximum_weight_bipartite_independent_set_result {
    Cap maximum_weight;
    std::vector<bool> left_selected;
    std::vector<bool> right_selected;
};

/*
 * 想定ユースケース: 競合関係が二部グラフをなすプロジェクトから、競合しない最大重み集合を選ぶ
 * 入力: 左右の頂点重みと、同時に選べない 0-indexed の (left, right) 組
 * 出力: 最大重みと、左右それぞれの頂点を選んだかを返す
 * 処理量: V=left_size+right_size+2、E=O(V+conflicts.size()) とした ACL 最大フロー計算量
 */
template <class Cap, class Calc = long long>
maximum_weight_bipartite_independent_set_result<Cap>
maximum_weight_bipartite_independent_set(
    const std::vector<Cap>& left_weight,
    const std::vector<Cap>& right_weight,
    const std::vector<std::pair<int, int>>& conflicts) {
    const int left_size = static_cast<int>(left_weight.size());
    const int right_size = static_cast<int>(right_weight.size());
    project_selection<Cap, Calc> ps(left_size + right_size);

    // 左は true、右は false を選択状態とし、競合する左を選ぶなら右を非選択にする
    for (int left = 0; left < left_size; ++left) {
        ps.add_score(left, Cap{}, left_weight[left]);
    }
    for (int right = 0; right < right_size; ++right) {
        ps.add_score(left_size + right, right_weight[right], Cap{});
    }
    for (const auto& [left, right] : conflicts) {
        ps.add_implication(left, left_size + right);
    }

    auto answer = ps.solve();
    assert(answer.feasible);
    std::vector<bool> left_selected(left_size);
    std::vector<bool> right_selected(right_size);
    for (int left = 0; left < left_size; ++left) {
        left_selected[left] = answer.state[left];
    }
    for (int right = 0; right < right_size; ++right) {
        right_selected[right] = !answer.state[left_size + right];
    }
    return {answer.max_score, std::move(left_selected), std::move(right_selected)};
}

#if __INCLUDE_LEVEL__ == 0

bool test_mask_bit(int mask, int vertex) {
    return ((mask >> vertex) & 1) != 0;
}

void test_basic_project_selection() {
    {
        project_selection<long long> ps(2);
        ps.add_gain(0, true, 10);
        ps.add_cost(1, true, 3);
        ps.add_implication(0, 1);
        const auto answer = ps.solve();
        assert(answer.feasible);
        assert(answer.max_score == 7);
        assert(answer.state == std::vector<bool>({true, true}));
    }
    {
        project_selection<long long> ps(1);
        ps.force(0, true);
        ps.force(0, false);
        const auto answer = ps.solve();
        assert(!answer.feasible);
        assert(answer.state.empty());
    }
    {
        project_selection<long long> ps(0);
        ps.add_gain_if_all({}, true, 7);
        ps.add_cost_if_any({}, true, 5);
        const auto answer = ps.solve();
        assert(answer.feasible);
        assert(answer.max_score == 7);
        assert(answer.state.empty());
    }
    {
        constexpr long long large = 6'000'000'000'000'000'000LL;
        project_selection<long long, __int128_t> ps(2);
        ps.add_pairwise_score(0, 1, large, large, large, large);
        const auto answer = ps.solve();
        assert(answer.feasible);
        assert(answer.max_score == large);
    }
}

void test_random_project_selection() {
    std::mt19937 rng(1'234'567U);
    auto random_int = [&](int lower, int upper) {
        return std::uniform_int_distribution<int>(lower, upper)(rng);
    };

    for (int trial = 0; trial < 1'500; ++trial) {
        const int n = random_int(1, 7);
        const int assignment_count = 1 << n;
        project_selection<long long> ps(n);
        std::vector<long long> score_by_mask(assignment_count);
        std::vector<unsigned char> valid(assignment_count, 1);

        auto add_to_scores = [&](const auto& value) {
            for (int mask = 0; mask < assignment_count; ++mask) {
                score_by_mask[mask] += value(mask);
            }
        };
        auto restrict_assignments = [&](const auto& predicate) {
            for (int mask = 0; mask < assignment_count; ++mask) {
                if (!predicate(mask)) valid[mask] = 0;
            }
        };

        const long long constant_score = random_int(-5, 5);
        ps.add_constant_score(constant_score);
        add_to_scores([&](int) { return constant_score; });

        for (int v = 0; v < n; ++v) {
            const long long score0 = random_int(-6, 6);
            const long long score1 = random_int(-6, 6);
            ps.add_score(v, score0, score1);
            add_to_scores([&](int mask) {
                return test_mask_bit(mask, v) ? score1 : score0;
            });

            const bool gain_state = random_int(0, 1) != 0;
            const long long gain = random_int(0, 5);
            ps.add_gain(v, gain_state, gain);
            add_to_scores([&](int mask) {
                return test_mask_bit(mask, v) == gain_state ? gain : 0LL;
            });

            const bool cost_state = random_int(0, 1) != 0;
            const long long cost = random_int(0, 5);
            ps.add_cost(v, cost_state, cost);
            add_to_scores([&](int mask) {
                return test_mask_bit(mask, v) == cost_state ? -cost : 0LL;
            });

            const long long cost0 = random_int(-4, 4);
            const long long cost1 = random_int(-4, 4);
            ps.add_costs(v, cost0, cost1);
            add_to_scores([&](int mask) {
                return -(test_mask_bit(mask, v) ? cost1 : cost0);
            });
        }

        if (n >= 2) {
            for (int repeat = 0; repeat < 3; ++repeat) {
                const int u = random_int(0, n - 1);
                int v = random_int(0, n - 2);
                if (v >= u) ++v;

                const long long soft_cost = random_int(0, 5);
                ps.add_soft_implication(u, v, soft_cost);
                add_to_scores([&](int mask) {
                    return test_mask_bit(mask, u) && !test_mask_bit(mask, v)
                        ? -soft_cost
                        : 0LL;
                });

                const long long different_cost = random_int(0, 5);
                ps.add_cost_if_different(u, v, different_cost);
                add_to_scores([&](int mask) {
                    return test_mask_bit(mask, u) != test_mask_bit(mask, v)
                        ? -different_cost
                        : 0LL;
                });

                const long long same_gain = random_int(0, 5);
                ps.add_gain_if_same(u, v, same_gain);
                add_to_scores([&](int mask) {
                    return test_mask_bit(mask, u) == test_mask_bit(mask, v)
                        ? same_gain
                        : 0LL;
                });

                const long long score00 = random_int(-5, 5);
                const long long score01 = random_int(-5, 5);
                const long long score11 = random_int(-5, 5);
                const long long score_margin = random_int(0, 5);
                const long long score10 =
                    score00 + score11 - score01 - score_margin;
                ps.add_pairwise_score(u, v, score00, score01, score10, score11);
                add_to_scores([&](int mask) {
                    const int index =
                        (test_mask_bit(mask, u) ? 2 : 0) +
                        (test_mask_bit(mask, v) ? 1 : 0);
                    const std::array<long long, 4> values{
                        score00, score01, score10, score11};
                    return values[index];
                });

                const long long cost00 = random_int(-5, 5);
                const long long cost01 = random_int(-5, 5);
                const long long cost11 = random_int(-5, 5);
                const long long cost_margin = random_int(0, 5);
                const long long cost10 =
                    cost00 + cost11 - cost01 + cost_margin;
                ps.add_pairwise_cost(u, v, cost00, cost01, cost10, cost11);
                add_to_scores([&](int mask) {
                    const int index =
                        (test_mask_bit(mask, u) ? 2 : 0) +
                        (test_mask_bit(mask, v) ? 1 : 0);
                    const std::array<long long, 4> values{
                        cost00, cost01, cost10, cost11};
                    return -values[index];
                });
            }

            const int implication_from = random_int(0, n - 1);
            int implication_to = random_int(0, n - 2);
            if (implication_to >= implication_from) ++implication_to;
            ps.add_implication(implication_from, implication_to);
            restrict_assignments([&](int mask) {
                return !test_mask_bit(mask, implication_from) ||
                    test_mask_bit(mask, implication_to);
            });

            if (random_int(0, 2) == 0) {
                ps.add_equal(implication_from, implication_to);
                restrict_assignments([&](int mask) {
                    return test_mask_bit(mask, implication_from) ==
                        test_mask_bit(mask, implication_to);
                });
            }
        }

        std::vector<int> group(n);
        std::iota(group.begin(), group.end(), 0);
        std::shuffle(group.begin(), group.end(), rng);
        group.resize(random_int(0, n));
        const bool group_state = random_int(0, 1) != 0;
        const long long group_gain = random_int(0, 6);
        const long long group_cost = random_int(0, 6);
        ps.add_gain_if_all(group, group_state, group_gain);
        ps.add_cost_if_any(group, group_state, group_cost);
        add_to_scores([&](int mask) {
            bool all = true;
            bool any = false;
            for (int v : group) {
                const bool matches = test_mask_bit(mask, v) == group_state;
                all = all && matches;
                any = any || matches;
            }
            return (all ? group_gain : 0LL) - (any ? group_cost : 0LL);
        });

        if (random_int(0, 3) == 0) {
            const int forced_vertex = random_int(0, n - 1);
            const bool forced_state = random_int(0, 1) != 0;
            ps.force(forced_vertex, forced_state);
            restrict_assignments([&](int mask) {
                return test_mask_bit(mask, forced_vertex) == forced_state;
            });
            if (random_int(0, 9) == 0) {
                ps.force(forced_vertex, !forced_state);
                restrict_assignments([&](int mask) {
                    return test_mask_bit(mask, forced_vertex) != forced_state;
                });
            }
        }

        bool has_valid = false;
        long long best_score = std::numeric_limits<long long>::lowest();
        for (int mask = 0; mask < assignment_count; ++mask) {
            if (valid[mask] == 0) continue;
            has_valid = true;
            best_score = std::max(best_score, score_by_mask[mask]);
        }

        const auto answer = ps.solve();
        assert(answer.feasible == has_valid);
        if (!has_valid) continue;
        assert(answer.max_score == best_score);
        int answer_mask = 0;
        for (int v = 0; v < n; ++v) {
            if (answer.state[v]) answer_mask |= 1 << v;
        }
        assert(valid[answer_mask] != 0);
        assert(score_by_mask[answer_mask] == best_score);
    }
}

void test_random_maximum_weight_closure() {
    std::mt19937 rng(2'345'678U);
    auto random_int = [&](int lower, int upper) {
        return std::uniform_int_distribution<int>(lower, upper)(rng);
    };

    for (int trial = 0; trial < 800; ++trial) {
        const int n = random_int(0, 8);
        std::vector<long long> weight(n);
        for (long long& value : weight) value = random_int(-10, 10);
        std::vector<std::pair<int, int>> implications;
        for (int from = 0; from < n; ++from) {
            for (int to = 0; to < n; ++to) {
                if (from != to && random_int(0, 7) == 0) {
                    implications.push_back({from, to});
                }
            }
        }

        long long expected = std::numeric_limits<long long>::lowest();
        const int assignment_count = 1 << n;
        for (int mask = 0; mask < assignment_count; ++mask) {
            bool valid = true;
            for (const auto& [from, to] : implications) {
                if (test_mask_bit(mask, from) && !test_mask_bit(mask, to)) valid = false;
            }
            if (!valid) continue;
            long long current = 0;
            for (int v = 0; v < n; ++v) {
                if (test_mask_bit(mask, v)) current += weight[v];
            }
            expected = std::max(expected, current);
        }

        const auto answer = maximum_weight_closure(weight, implications);
        assert(answer.maximum_weight == expected);
        long long actual = 0;
        for (int v = 0; v < n; ++v) {
            if (answer.selected[v]) actual += weight[v];
        }
        for (const auto& [from, to] : implications) {
            assert(!answer.selected[from] || answer.selected[to]);
        }
        assert(actual == expected);
    }
}

void test_random_binary_labeling() {
    std::mt19937 rng(3'456'789U);
    auto random_int = [&](int lower, int upper) {
        return std::uniform_int_distribution<int>(lower, upper)(rng);
    };

    for (int trial = 0; trial < 1'000; ++trial) {
        const int n = random_int(1, 7);
        std::vector<std::array<long long, 2>> unary_cost(n);
        for (auto& cost : unary_cost) {
            cost[0] = random_int(-6, 6);
            cost[1] = random_int(-6, 6);
        }

        std::vector<binary_pairwise_cost<long long>> pairwise_cost;
        const int term_count = random_int(0, 10);
        for (int term_index = 0; term_index < term_count && n >= 2; ++term_index) {
            const int u = random_int(0, n - 1);
            int v = random_int(0, n - 2);
            if (v >= u) ++v;
            const long long cost00 = random_int(-5, 5);
            const long long cost01 = random_int(-5, 5);
            const long long cost11 = random_int(-5, 5);
            const long long margin = random_int(0, 6);
            const long long cost10 = cost00 + cost11 - cost01 + margin;
            pairwise_cost.push_back({u, v, cost00, cost01, cost10, cost11});
        }

        long long expected = std::numeric_limits<long long>::max();
        int expected_mask = 0;
        const int assignment_count = 1 << n;
        for (int mask = 0; mask < assignment_count; ++mask) {
            long long current = 0;
            for (int v = 0; v < n; ++v) {
                current += unary_cost[v][test_mask_bit(mask, v) ? 1 : 0];
            }
            for (const auto& term : pairwise_cost) {
                const int index =
                    (test_mask_bit(mask, term.u) ? 2 : 0) +
                    (test_mask_bit(mask, term.v) ? 1 : 0);
                const std::array<long long, 4> values{
                    term.cost00, term.cost01, term.cost10, term.cost11};
                current += values[index];
            }
            if (current < expected) {
                expected = current;
                expected_mask = mask;
            }
        }

        const auto answer = minimum_binary_labeling(unary_cost, pairwise_cost);
        assert(answer.minimum_cost == expected);
        long long actual = 0;
        for (int v = 0; v < n; ++v) {
            actual += unary_cost[v][answer.state[v] ? 1 : 0];
        }
        for (const auto& term : pairwise_cost) {
            const int index =
                (answer.state[term.u] ? 2 : 0) +
                (answer.state[term.v] ? 1 : 0);
            const std::array<long long, 4> values{
                term.cost00, term.cost01, term.cost10, term.cost11};
            actual += values[index];
        }
        assert(actual == expected);
        (void)expected_mask;
    }
}

void test_random_monotone_levels() {
    std::mt19937 rng(4'567'890U);
    auto random_int = [&](int lower, int upper) {
        return std::uniform_int_distribution<int>(lower, upper)(rng);
    };

    for (int trial = 0; trial < 700; ++trial) {
        const int n = random_int(1, 4);
        std::vector<std::vector<long long>> score(n);
        std::vector<int> items_with_threshold;
        for (int item = 0; item < n; ++item) {
            const int maximum_level = random_int(0, 3);
            score[item].resize(maximum_level + 1);
            for (long long& value : score[item]) value = random_int(-8, 8);
            if (maximum_level >= 1) items_with_threshold.push_back(item);
        }

        std::vector<monotone_level_implication> implications;
        if (!items_with_threshold.empty()) {
            const int implication_count = random_int(0, 7);
            for (int i = 0; i < implication_count; ++i) {
                const int from = items_with_threshold[
                    random_int(0, static_cast<int>(items_with_threshold.size()) - 1)];
                const int to = items_with_threshold[
                    random_int(0, static_cast<int>(items_with_threshold.size()) - 1)];
                implications.push_back({
                    from,
                    random_int(1, static_cast<int>(score[from].size()) - 1),
                    to,
                    random_int(1, static_cast<int>(score[to].size()) - 1)});
            }
        }

        int assignment_count = 1;
        for (const auto& values : score) {
            assignment_count *= static_cast<int>(values.size());
        }
        long long expected = std::numeric_limits<long long>::lowest();
        std::vector<int> level(n);
        for (int code = 0; code < assignment_count; ++code) {
            int remaining = code;
            for (int item = 0; item < n; ++item) {
                const int level_count = static_cast<int>(score[item].size());
                level[item] = remaining % level_count;
                remaining /= level_count;
            }
            bool valid = true;
            for (const auto& implication : implications) {
                if (level[implication.from] >= implication.from_level &&
                    level[implication.to] < implication.to_level) {
                    valid = false;
                }
            }
            if (!valid) continue;
            long long current = 0;
            for (int item = 0; item < n; ++item) current += score[item][level[item]];
            expected = std::max(expected, current);
        }

        const auto answer = maximum_score_monotone_levels(score, implications);
        assert(answer.maximum_score == expected);
        long long actual = 0;
        for (int item = 0; item < n; ++item) actual += score[item][answer.level[item]];
        for (const auto& implication : implications) {
            assert(answer.level[implication.from] < implication.from_level ||
                answer.level[implication.to] >= implication.to_level);
        }
        assert(actual == expected);
    }
}

void test_random_bipartite_solvers() {
    std::mt19937 rng(5'678'901U);
    auto random_int = [&](int lower, int upper) {
        return std::uniform_int_distribution<int>(lower, upper)(rng);
    };

    for (int trial = 0; trial < 800; ++trial) {
        const int left_size = random_int(0, 4);
        const int right_size = random_int(0, 4);
        const int total_size = left_size + right_size;
        std::vector<long long> left_weight(left_size);
        std::vector<long long> right_weight(right_size);
        for (long long& value : left_weight) value = random_int(-5, 10);
        for (long long& value : right_weight) value = random_int(-5, 10);
        std::vector<std::pair<int, int>> edges;
        for (int left = 0; left < left_size; ++left) {
            for (int right = 0; right < right_size; ++right) {
                if (random_int(0, 2) == 0) edges.push_back({left, right});
            }
        }

        long long expected_cover = std::numeric_limits<long long>::max();
        long long expected_independent = std::numeric_limits<long long>::lowest();
        const int assignment_count = 1 << total_size;
        for (int mask = 0; mask < assignment_count; ++mask) {
            bool is_cover = true;
            bool is_independent = true;
            for (const auto& [left, right] : edges) {
                const bool left_selected = test_mask_bit(mask, left);
                const bool right_selected = test_mask_bit(mask, left_size + right);
                if (!left_selected && !right_selected) is_cover = false;
                if (left_selected && right_selected) is_independent = false;
            }
            long long weight_sum = 0;
            for (int left = 0; left < left_size; ++left) {
                if (test_mask_bit(mask, left)) weight_sum += left_weight[left];
            }
            for (int right = 0; right < right_size; ++right) {
                if (test_mask_bit(mask, left_size + right)) weight_sum += right_weight[right];
            }
            if (is_cover) expected_cover = std::min(expected_cover, weight_sum);
            if (is_independent) expected_independent = std::max(expected_independent, weight_sum);
        }

        const auto cover = minimum_weight_bipartite_vertex_cover(
            left_weight, right_weight, edges);
        assert(cover.minimum_weight == expected_cover);
        long long actual_cover = 0;
        for (int left = 0; left < left_size; ++left) {
            if (cover.left_in_cover[left]) actual_cover += left_weight[left];
        }
        for (int right = 0; right < right_size; ++right) {
            if (cover.right_in_cover[right]) actual_cover += right_weight[right];
        }
        for (const auto& [left, right] : edges) {
            assert(cover.left_in_cover[left] || cover.right_in_cover[right]);
        }
        assert(actual_cover == expected_cover);

        const auto independent = maximum_weight_bipartite_independent_set(
            left_weight, right_weight, edges);
        assert(independent.maximum_weight == expected_independent);
        long long actual_independent = 0;
        for (int left = 0; left < left_size; ++left) {
            if (independent.left_selected[left]) actual_independent += left_weight[left];
        }
        for (int right = 0; right < right_size; ++right) {
            if (independent.right_selected[right]) actual_independent += right_weight[right];
        }
        for (const auto& [left, right] : edges) {
            assert(!independent.left_selected[left] || !independent.right_selected[right]);
        }
        assert(actual_independent == expected_independent);
    }
}

int main() {
    test_basic_project_selection();
    test_random_project_selection();
    test_random_maximum_weight_closure();
    test_random_binary_labeling();
    test_random_monotone_levels();
    test_random_bipartite_solvers();
    std::cout << "project_selection: all tests passed\n";
}

#endif
