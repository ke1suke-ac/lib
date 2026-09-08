#pragma once
#include <bits/stdc++.h>
#include <atcoder/twosat>

// 2-SAT典型を素早く書くための小型ソルバー集
// すべて atcoder::two_sat を内部で使うヘッダ実装で、namespace は付けない
// 各構造体は互いに依存せず、必要な構造体だけを単体で切り出して使える
// 2択系ソルバーでは choice 0 が true、choice 1 が false に対応する

// -----------------------------------------------------------------------------
// BoolSatSolver
// -----------------------------------------------------------------------------
// 生の2-SATをそのまま書きたいときに使う
// 「A または B」「A なら B」「A と B は同時不可」「x_i == x_j」など、
// 問題文の条件がリテラル2個以下の論理式として直接表せる場合に向く
//
// リテラルは Lit{var, val} で表し、これは「x_var = val」を意味する
// 例: Lit{3, true} は x_3 が true、Lit{3, false} は x_3 が false
//
// 典型的な使い方:
//   BoolSatSolver solver(n);
//   solver.add_or({i, true}, {j, false});   // x_i = true または x_j = false
//   solver.imply({i, true}, {j, true});     // x_i = true なら x_j = true
//   solver.forbid({i, true}, {j, false});   // x_i = true かつ x_j = false を禁止
//   solver.force({k, true});                // x_k = true に固定
//   solver.same(a, b);                      // x_a == x_b
//   solver.diff(a, b);                      // x_a != x_b
//   if (solver.solve()) {
//       auto ans = solver.answer();         // ans[i] が x_i の値
//   }
//
// at_most_one(lits) は lits のうち高々1つだけ真という制約を全ペア禁止で入れる
// 集合サイズkに対してO(k^2)節を追加するため、大きな集合に多用する場合は注意する
struct BoolSatSolver {
    // BoolSatSolver の add_or / forbid / imply / force で使うリテラル
    // var は変数番号、val はその変数に要求する値を表す
    // 例: Lit{2, false} は x_2 = false という条件を表す
    struct Lit {
        int var;
        bool val;
    };

    int n;
    atcoder::two_sat ts;
    std::vector<bool> ans;

    // n変数の2-SATソルバーを作る、O(n)
    explicit BoolSatSolver(int n_) : n(n_), ts(n_), ans() {}

    // a または b を追加する、O(1)
    void add_or(Lit a, Lit b) {
        ts.add_clause(a.var, a.val, b.var, b.val);
    }

    // a と b を同時に真にすることを禁止する、O(1)
    void forbid(Lit a, Lit b) {
        add_or({a.var, !a.val}, {b.var, !b.val});
    }

    // a ならば b を追加する、O(1)
    void imply(Lit a, Lit b) {
        add_or({a.var, !a.val}, b);
    }

    // a を真に固定する、O(1)
    void force(Lit a) {
        add_or(a, a);
    }

    // x_i == x_j を追加する、O(1)
    void same(int i, int j) {
        ts.add_clause(i, false, j, true);
        ts.add_clause(i, true, j, false);
    }

    // x_i != x_j を追加する、O(1)
    void diff(int i, int j) {
        ts.add_clause(i, false, j, false);
        ts.add_clause(i, true, j, true);
    }

    // lits のうち高々1つだけ真にできる制約を追加する、O(k log k + k^2)
    void at_most_one(const std::vector<Lit>& lits) {
        std::vector<Lit> v = lits;
        std::sort(v.begin(), v.end(), [](const Lit& a, const Lit& b) {
            if (a.var != b.var) return a.var < b.var;
            return a.val < b.val;
        });
        v.erase(std::unique(v.begin(), v.end(), [](const Lit& a, const Lit& b) {
            return a.var == b.var && a.val == b.val;
        }), v.end());

        // 重複を取り除いたリテラル集合の全ペアを同時禁止にする
        for (int i = 0; i < (int)v.size(); i++) {
            for (int j = i + 1; j < (int)v.size(); j++) {
                forbid(v[i], v[j]);
            }
        }
    }

    // 現在の制約が充足可能か判定し、可能なら解を保持する、O(n + m)
    bool solve() {
        if (!ts.satisfiable()) {
            ans.clear();
            return false;
        }
        ans = ts.answer();
        return true;
    }

    // 直近の solve が成功したときの解を返す、O(1)
    const std::vector<bool>& answer() const {
        return ans;
    }
};

// -----------------------------------------------------------------------------
// SameDiffSatSolver
// -----------------------------------------------------------------------------
// 変数同士の「同じ」「異なる」と、値の固定だけを短く書くためのソルバー
// 2色塗り、行・列反転、スイッチのON/OFF、パリティ制約などに向く
//
// 典型的な使い方:
//   SameDiffSatSolver solver(n);
//   solver.same(i, j);        // x_i == x_j
//   solver.diff(u, v);        // x_u != x_v
//   solver.force(0, true);    // x_0 = true に固定
//   if (solver.solve()) {
//       auto ans = solver.answer();
//   }
//
// 純粋なsame/diffだけならパリティ付きUnion-Findでも解けるが、
// atcoder::two_satで統一したい場合や、解のtrue/false割当が欲しい場合に便利
struct SameDiffSatSolver {
    int n;
    atcoder::two_sat ts;
    std::vector<bool> ans;

    // n変数の同値・相違制約ソルバーを作る、O(n)
    explicit SameDiffSatSolver(int n_) : n(n_), ts(n_), ans() {}

    // x_i == x_j を追加する、O(1)
    void same(int i, int j) {
        ts.add_clause(i, false, j, true);
        ts.add_clause(i, true, j, false);
    }

    // x_i != x_j を追加する、O(1)
    void diff(int i, int j) {
        ts.add_clause(i, false, j, false);
        ts.add_clause(i, true, j, true);
    }

    // x_i = val に固定する、O(1)
    void force(int i, bool val) {
        ts.add_clause(i, val, i, val);
    }

    // 現在の制約が充足可能か判定し、可能なら解を保持する、O(n + m)
    bool solve() {
        if (!ts.satisfiable()) {
            ans.clear();
            return false;
        }
        ans = ts.answer();
        return true;
    }

    // 直近の solve が成功したときの解を返す、O(1)
    const std::vector<bool>& answer() const {
        return ans;
    }
};

// -----------------------------------------------------------------------------
// TwoChoiceSolver
// -----------------------------------------------------------------------------
// 各対象iが候補0または候補1のどちらかを選ぶ問題用の中核ソルバー
// 候補ペアの禁止、含意、固定、2対象間の許可表を手で追加したいときに使う
//
// 候補番号と内部の真偽値の対応は全2択系ソルバーで共通:
//   choice 0 <=> x_i = true
//   choice 1 <=> x_i = false
// solve後の choice(i) は、実際に選ばれた候補番号0/1を返す
//
// 典型的な使い方:
//   TwoChoiceSolver solver(n);
//   solver.forbid(i, 0, j, 1);  // iが候補0、jが候補1を同時に選ぶことを禁止
//   solver.imply(i, 1, j, 0);   // iが候補1ならjは候補0
//   solver.force(k, 0);         // kは候補0に固定
//   solver.same(a, b);          // aとbは同じ候補番号を選ぶ
//   solver.diff(a, b);          // aとbは異なる候補番号を選ぶ
//   if (solver.solve()) {
//       auto choices = solver.choices();     // choices[i] は0または1
//   }
//
// 2対象間の4通りの許可表がある場合は add_relation を使う
//   ok[ci][cj] = true  なら i=ci, j=cj は許可
//   ok[ci][cj] = false なら i=ci, j=cj は禁止
// 文字列反転の辞書順、隣接状態遷移、タイルの局所整合などを短く書ける
struct TwoChoiceSolver {
    int n;
    atcoder::two_sat ts;
    std::vector<bool> ans;

    // n個の対象がそれぞれ候補0/1を選ぶソルバーを作る、O(n)
    explicit TwoChoiceSolver(int n_) : n(n_), ts(n_), ans() {}

    // iがciを選ぶ、またはjがcjを選ぶ制約を追加する、O(1)
    void add_or(int i, int ci, int j, int cj) {
        ts.add_clause(i, value(ci), j, value(cj));
    }

    // iがciを選び、jがcjを選ぶことを同時禁止する、O(1)
    void forbid(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, !value(cj));
    }

    // iがciを選ぶならjがcjを選ぶ制約を追加する、O(1)
    void imply(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, value(cj));
    }

    // iの選択をciに固定する、O(1)
    void force(int i, int ci) {
        ts.add_clause(i, value(ci), i, value(ci));
    }

    // iとjが同じ候補番号を選ぶ制約を追加する、O(1)
    void same(int i, int j) {
        forbid(i, 0, j, 1);
        forbid(i, 1, j, 0);
    }

    // iとjが異なる候補番号を選ぶ制約を追加する、O(1)
    void diff(int i, int j) {
        forbid(i, 0, j, 0);
        forbid(i, 1, j, 1);
    }

    // ok[ci][cj] が false の組み合わせを禁止する、O(1)
    void add_relation(int i, int j, const std::array<std::array<bool, 2>, 2>& ok) {
        for (int ci = 0; ci < 2; ci++) {
            for (int cj = 0; cj < 2; cj++) {
                if (!ok[ci][cj]) forbid(i, ci, j, cj);
            }
        }
    }

    // 現在の制約が充足可能か判定し、可能なら解を保持する、O(n + m)
    bool solve() {
        if (!ts.satisfiable()) {
            ans.clear();
            return false;
        }
        ans = ts.answer();
        return true;
    }

    // 直近の solve が成功したときの i の候補番号を返す、O(1)
    int choice(int i) const {
        return ans[i] ? 0 : 1;
    }

    // 直近の solve が成功したときの全候補番号を返す、O(n)
    std::vector<int> choices() const {
        std::vector<int> res(n);
        for (int i = 0; i < n; i++) res[i] = choice(i);
        return res;
    }

private:
    static bool value(int choice) {
        return choice == 0;
    }
};

// -----------------------------------------------------------------------------
// TwoChoiceResourceSolver
// -----------------------------------------------------------------------------
// 各対象iが候補0/1を選び、選んだ候補が使う資源の重複を禁止するソルバー
// 「同じマスを使えない」「同じ辺を使えない」「同じ値を選べない」など、
// 候補が使用する資源IDを列挙できる場合に向く
//
// 使う流れ:
//   1. use(i, ci, resource) で「iの候補ciがresourceを使う」と登録する
//   2. build() で同じresourceを使う候補ペアの同時禁止制約を生成する
//   3. solve() で充足可能性を判定し、choices() で選択を得る
//
// 典型的な使い方:
//   TwoChoiceResourceSolver solver(n);
//   solver.use(i, 0, cell_id);      // iの候補0がcell_idを使う
//   solver.use(j, 1, cell_id);      // jの候補1も同じcell_idを使うなら同時選択不可
//   solver.build();
//   if (solver.solve()) {
//       auto choices = solver.choices();
//   }
//
// resourceはlong longで管理するため、座標・辺・時刻などは利用側でID化する
// 同じ候補に同じresourceを複数回登録しても、build内で重複削除される
// 同じ対象の候補0と候補1が同じresourceを使うことは自己矛盾にはしない
struct TwoChoiceResourceSolver {
    // 資源利用の内部登録用データ
    // 通常は直接作らず、use(i, ci, resource) を呼んで登録する
    struct Entry {
        long long resource;
        int item;
        int choice;
    };

    int n;
    atcoder::two_sat ts;
    std::vector<bool> ans;
    std::vector<Entry> entries;

    // n個の対象がそれぞれ候補0/1を選ぶ資源重複禁止ソルバーを作る、O(n)
    explicit TwoChoiceResourceSolver(int n_) : n(n_), ts(n_), ans(), entries() {}

    // iがciを選ぶ、またはjがcjを選ぶ制約を追加する、O(1)
    void add_or(int i, int ci, int j, int cj) {
        ts.add_clause(i, value(ci), j, value(cj));
    }

    // iがciを選び、jがcjを選ぶことを同時禁止する、O(1)
    void forbid(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, !value(cj));
    }

    // iがciを選ぶならjがcjを選ぶ制約を追加する、O(1)
    void imply(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, value(cj));
    }

    // iの選択をciに固定する、O(1)
    void force(int i, int ci) {
        ts.add_clause(i, value(ci), i, value(ci));
    }

    // iの候補ciがresourceを使うことを登録する、O(1)
    void use(int i, int ci, long long resource) {
        entries.push_back({resource, i, ci});
    }

    // 登録済み資源から同じ資源を使う候補ペアの同時禁止制約を追加する、O(U log U + Σk^2)
    void build() {
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.resource != b.resource) return a.resource < b.resource;
            if (a.item != b.item) return a.item < b.item;
            return a.choice < b.choice;
        });
        entries.erase(std::unique(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.resource == b.resource && a.item == b.item && a.choice == b.choice;
        }), entries.end());

        // 同じ resource を持つ連続区間ごとに、異なる対象の候補同士を禁止する
        int l = 0;
        while (l < (int)entries.size()) {
            int r = l + 1;
            while (r < (int)entries.size() && entries[r].resource == entries[l].resource) r++;

            for (int a = l; a < r; a++) {
                for (int b = a + 1; b < r; b++) {
                    if (entries[a].item != entries[b].item) {
                        forbid(entries[a].item, entries[a].choice, entries[b].item, entries[b].choice);
                    }
                }
            }
            l = r;
        }
    }

    // 現在の制約が充足可能か判定し、可能なら解を保持する、O(n + m)
    bool solve() {
        if (!ts.satisfiable()) {
            ans.clear();
            return false;
        }
        ans = ts.answer();
        return true;
    }

    // 直近の solve が成功したときの i の候補番号を返す、O(1)
    int choice(int i) const {
        return ans[i] ? 0 : 1;
    }

    // 直近の solve が成功したときの全候補番号を返す、O(n)
    std::vector<int> choices() const {
        std::vector<int> res(n);
        for (int i = 0; i < n; i++) res[i] = choice(i);
        return res;
    }

private:
    static bool value(int choice) {
        return choice == 0;
    }
};

// -----------------------------------------------------------------------------
// TwoChoiceIntervalSolver
// -----------------------------------------------------------------------------
// 各対象iが候補区間0/1を選び、同じgroup内で選んだ区間同士の重なりを禁止するソルバー
// 会議の2候補時刻、予約枠、工事期間、広告掲載期間、機械を使う作業などに向く
//
// 区間は半開区間 [l, r) として扱う
// そのため [1, 3) と [3, 5) は重ならない
// groupが同じ候補区間同士だけを比較するため、部屋ID・人ID・機械IDなどをgroupにできる
// groupを省略するとすべて0になり、全候補が同じグループとして扱われる
//
// 典型的な使い方:
//   TwoChoiceIntervalSolver solver(n);
//   solver.set_interval(i, 0, l0, r0, room_id);
//   solver.set_interval(i, 1, l1, r1, room_id);
//   solver.build();                // 重なる候補ペアを自動でforbidする
//   if (solver.solve()) {
//       auto choices = solver.choices();
//   }
//
// buildは単純な全ペア比較なのでO(n^2)
// 大規模な区間集合では、問題ごとにsweep line等でforbidを追加する方がよい場合がある
struct TwoChoiceIntervalSolver {
    // 1つの候補区間を表す内部データ
    // 通常は直接触らず、set_interval(i, ci, l, r, group) で設定する
    // 区間は半開区間 [l, r) として扱う
    struct Interval {
        long long l;
        long long r;
        long long group;
    };

    int n;
    atcoder::two_sat ts;
    std::vector<bool> ans;
    std::vector<std::array<Interval, 2>> intervals;

    // n個の対象がそれぞれ候補区間0/1を選ぶ区間重複禁止ソルバーを作る、O(n)
    explicit TwoChoiceIntervalSolver(int n_) : n(n_), ts(n_), ans(), intervals(n_) {}

    // iがciを選ぶ、またはjがcjを選ぶ制約を追加する、O(1)
    void add_or(int i, int ci, int j, int cj) {
        ts.add_clause(i, value(ci), j, value(cj));
    }

    // iがciを選び、jがcjを選ぶことを同時禁止する、O(1)
    void forbid(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, !value(cj));
    }

    // iがciを選ぶならjがcjを選ぶ制約を追加する、O(1)
    void imply(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, value(cj));
    }

    // iの選択をciに固定する、O(1)
    void force(int i, int ci) {
        ts.add_clause(i, value(ci), i, value(ci));
    }

    // iの候補ciの区間を半開区間[l, r)として設定する、O(1)
    void set_interval(int i, int ci, long long l, long long r, long long group = 0) {
        intervals[i][ci] = {l, r, group};
    }

    // 同じgroupで重なる候補区間ペアの同時禁止制約を追加する、O(n^2)
    void build() {
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                // 各対象の2候補ずつを比較し、同じ group の半開区間が交差する場合だけ禁止する
                for (int ci = 0; ci < 2; ci++) {
                    for (int cj = 0; cj < 2; cj++) {
                        if (overlap(intervals[i][ci], intervals[j][cj])) {
                            forbid(i, ci, j, cj);
                        }
                    }
                }
            }
        }
    }

    // 現在の制約が充足可能か判定し、可能なら解を保持する、O(n + m)
    bool solve() {
        if (!ts.satisfiable()) {
            ans.clear();
            return false;
        }
        ans = ts.answer();
        return true;
    }

    // 直近の solve が成功したときの i の候補番号を返す、O(1)
    int choice(int i) const {
        return ans[i] ? 0 : 1;
    }

    // 直近の solve が成功したときの全候補番号を返す、O(n)
    std::vector<int> choices() const {
        std::vector<int> res(n);
        for (int i = 0; i < n; i++) res[i] = choice(i);
        return res;
    }

private:
    static bool value(int choice) {
        return choice == 0;
    }

    static bool overlap(const Interval& a, const Interval& b) {
        return a.group == b.group && a.l < b.r && b.l < a.r;
    }
};

// -----------------------------------------------------------------------------
// TwoChoiceDistanceSolver
// -----------------------------------------------------------------------------
// 各対象iが候補点0/1を選び、同じgroup内で選んだ点同士の距離制約を満たすソルバー
// 選んだ2点の二乗距離が min_dist2 未満になる候補ペアを自動で禁止する
// 旗・施設・ラベル配置、最小距離最大化の判定部分などに向く
//
// 距離条件は「二乗距離 >= min_dist2」を要求する形
// ちょうど min_dist2 の距離は許可され、min_dist2 未満だけが禁止される
// groupが同じ点同士だけを比較するため、種類別・領域別に距離制約を分けられる
// groupを省略するとすべて0になり、全候補点が同じグループとして扱われる
//
// 典型的な使い方:
//   TwoChoiceDistanceSolver solver(n);
//   solver.set_point(i, 0, x0, y0);
//   solver.set_point(i, 1, x1, y1);
//   solver.build(1LL * D * D);      // 選んだ点同士の距離をD以上にする
//   if (solver.solve()) {
//       auto choices = solver.choices();
//   }
//
// 最大最小距離を求める場合は、このソルバーを判定器として利用側で二分探索する
// 距離計算には__int128を使い、long long座標の二乗和で起きやすいオーバーフローを避ける
struct TwoChoiceDistanceSolver {
    // 1つの候補点を表す内部データ
    // 通常は直接触らず、set_point(i, ci, x, y, group) で設定する
    // group が同じ点同士だけ距離制約の対象になる
    struct Point {
        long long x;
        long long y;
        long long group;
    };

    int n;
    atcoder::two_sat ts;
    std::vector<bool> ans;
    std::vector<std::array<Point, 2>> points;

    // n個の対象がそれぞれ候補点0/1を選ぶ距離制約ソルバーを作る、O(n)
    explicit TwoChoiceDistanceSolver(int n_) : n(n_), ts(n_), ans(), points(n_) {}

    // iがciを選ぶ、またはjがcjを選ぶ制約を追加する、O(1)
    void add_or(int i, int ci, int j, int cj) {
        ts.add_clause(i, value(ci), j, value(cj));
    }

    // iがciを選び、jがcjを選ぶことを同時禁止する、O(1)
    void forbid(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, !value(cj));
    }

    // iがciを選ぶならjがcjを選ぶ制約を追加する、O(1)
    void imply(int i, int ci, int j, int cj) {
        ts.add_clause(i, !value(ci), j, value(cj));
    }

    // iの選択をciに固定する、O(1)
    void force(int i, int ci) {
        ts.add_clause(i, value(ci), i, value(ci));
    }

    // iの候補ciの点を設定する、O(1)
    void set_point(int i, int ci, long long x, long long y, long long group = 0) {
        points[i][ci] = {x, y, group};
    }

    // 同じgroupで二乗距離がmin_dist2未満の候補点ペアを同時禁止する、O(n^2)
    void build(long long min_dist2) {
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                // 4通りの候補点ペアを比較し、距離閾値を満たさない組み合わせだけ禁止する
                for (int ci = 0; ci < 2; ci++) {
                    for (int cj = 0; cj < 2; cj++) {
                        if (too_close(points[i][ci], points[j][cj], min_dist2)) {
                            forbid(i, ci, j, cj);
                        }
                    }
                }
            }
        }
    }

    // 現在の制約が充足可能か判定し、可能なら解を保持する、O(n + m)
    bool solve() {
        if (!ts.satisfiable()) {
            ans.clear();
            return false;
        }
        ans = ts.answer();
        return true;
    }

    // 直近の solve が成功したときの i の候補番号を返す、O(1)
    int choice(int i) const {
        return ans[i] ? 0 : 1;
    }

    // 直近の solve が成功したときの全候補番号を返す、O(n)
    std::vector<int> choices() const {
        std::vector<int> res(n);
        for (int i = 0; i < n; i++) res[i] = choice(i);
        return res;
    }

private:
    static bool value(int choice) {
        return choice == 0;
    }

    static bool too_close(const Point& a, const Point& b, long long min_dist2) {
        if (a.group != b.group) return false;
        __int128 dx = (__int128)a.x - b.x;
        __int128 dy = (__int128)a.y - b.y;
        __int128 dist2 = dx * dx + dy * dy;
        return dist2 < (__int128)min_dist2;
    }
};

#if __INCLUDE_LEVEL__ == 0
#include <cassert>

// 単体テストでだけ使うリテラル表現
struct TestLit {
    int var;
    bool val;
};

// 単体テストでだけ使う2-CNF節
struct TestClause {
    TestLit a;
    TestLit b;
};

static bool test_eval_lit(const TestLit& lit, int mask) {
    return (((mask >> lit.var) & 1) != 0) == lit.val;
}

static bool test_eval_clauses(const std::vector<TestClause>& clauses, const std::vector<bool>& ans) {
    for (const TestClause& c : clauses) {
        bool av = ans[c.a.var] == c.a.val;
        bool bv = ans[c.b.var] == c.b.val;
        if (!av && !bv) return false;
    }
    return true;
}

static bool test_naive_sat(int n, const std::vector<TestClause>& clauses) {
    for (int mask = 0; mask < (1 << n); mask++) {
        bool ok = true;
        for (const TestClause& c : clauses) {
            if (!test_eval_lit(c.a, mask) && !test_eval_lit(c.b, mask)) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
}


static bool test_choice_value(int choice) {
    return choice == 0;
}

static void test_add_or_clause(std::vector<TestClause>& clauses, TestLit a, TestLit b) {
    clauses.push_back({a, b});
}

static void test_add_forbid_clause(std::vector<TestClause>& clauses, TestLit a, TestLit b) {
    clauses.push_back({{a.var, !a.val}, {b.var, !b.val}});
}

static void test_bool_solver() {
    {
        BoolSatSolver solver(0);
        assert(solver.solve());
        assert(solver.answer().empty());
    }
    {
        BoolSatSolver solver(1);
        solver.force({0, true});
        solver.force({0, false});
        assert(!solver.solve());
    }
    {
        BoolSatSolver solver(2);
        solver.same(0, 1);
        solver.force({0, true});
        assert(solver.solve());
        assert(solver.answer()[0] == solver.answer()[1]);
    }
    {
        BoolSatSolver solver(3);
        solver.at_most_one({{0, true}, {0, true}, {1, true}, {2, false}});
        solver.force({0, true});
        solver.force({1, true});
        assert(!solver.solve());
    }

    std::mt19937 rng(1);
    for (int rep = 0; rep < 2000; rep++) {
        int n = 1 + (int)(rng() % 8);
        int ops = (int)(rng() % 30);
        BoolSatSolver solver(n);
        std::vector<TestClause> clauses;

        auto random_lit = [&]() -> BoolSatSolver::Lit {
            return {(int)(rng() % n), (bool)(rng() & 1)};
        };
        auto to_test_lit = [](BoolSatSolver::Lit lit) -> TestLit {
            return {lit.var, lit.val};
        };

        for (int t = 0; t < ops; t++) {
            int type = (int)(rng() % 7);
            if (type == 0) {
                auto a = random_lit(), b = random_lit();
                solver.add_or(a, b);
                test_add_or_clause(clauses, to_test_lit(a), to_test_lit(b));
            } else if (type == 1) {
                auto a = random_lit(), b = random_lit();
                solver.forbid(a, b);
                test_add_forbid_clause(clauses, to_test_lit(a), to_test_lit(b));
            } else if (type == 2) {
                auto a = random_lit(), b = random_lit();
                solver.imply(a, b);
                test_add_or_clause(clauses, {a.var, !a.val}, to_test_lit(b));
            } else if (type == 3) {
                auto a = random_lit();
                solver.force(a);
                test_add_or_clause(clauses, to_test_lit(a), to_test_lit(a));
            } else if (type == 4) {
                int i = (int)(rng() % n), j = (int)(rng() % n);
                solver.same(i, j);
                test_add_or_clause(clauses, {i, false}, {j, true});
                test_add_or_clause(clauses, {i, true}, {j, false});
            } else if (type == 5) {
                int i = (int)(rng() % n), j = (int)(rng() % n);
                solver.diff(i, j);
                test_add_or_clause(clauses, {i, false}, {j, false});
                test_add_or_clause(clauses, {i, true}, {j, true});
            } else {
                int k = (int)(rng() % 6);
                std::vector<BoolSatSolver::Lit> lits;
                for (int i = 0; i < k; i++) lits.push_back(random_lit());
                solver.at_most_one(lits);

                std::sort(lits.begin(), lits.end(), [](const auto& a, const auto& b) {
                    if (a.var != b.var) return a.var < b.var;
                    return a.val < b.val;
                });
                lits.erase(std::unique(lits.begin(), lits.end(), [](const auto& a, const auto& b) {
                    return a.var == b.var && a.val == b.val;
                }), lits.end());
                for (int i = 0; i < (int)lits.size(); i++) {
                    for (int j = i + 1; j < (int)lits.size(); j++) {
                        test_add_forbid_clause(clauses, to_test_lit(lits[i]), to_test_lit(lits[j]));
                    }
                }
            }
        }

        bool got = solver.solve();
        bool want = test_naive_sat(n, clauses);
        assert(got == want);
        if (got) assert(test_eval_clauses(clauses, solver.answer()));
    }
}

static void test_same_diff_solver() {
    {
        SameDiffSatSolver solver(0);
        assert(solver.solve());
        assert(solver.answer().empty());
    }
    {
        SameDiffSatSolver solver(3);
        solver.diff(0, 1);
        solver.diff(1, 2);
        solver.diff(2, 0);
        assert(!solver.solve());
    }
    {
        SameDiffSatSolver solver(3);
        solver.same(0, 1);
        solver.diff(1, 2);
        solver.force(0, true);
        assert(solver.solve());
        const auto& ans = solver.answer();
        assert(ans[0] == ans[1]);
        assert(ans[1] != ans[2]);
    }

    std::mt19937 rng(2);
    for (int rep = 0; rep < 2000; rep++) {
        int n = 1 + (int)(rng() % 8);
        int ops = (int)(rng() % 30);
        SameDiffSatSolver solver(n);
        std::vector<TestClause> clauses;

        for (int t = 0; t < ops; t++) {
            int type = (int)(rng() % 3);
            if (type == 0) {
                int i = (int)(rng() % n), j = (int)(rng() % n);
                solver.same(i, j);
                test_add_or_clause(clauses, {i, false}, {j, true});
                test_add_or_clause(clauses, {i, true}, {j, false});
            } else if (type == 1) {
                int i = (int)(rng() % n), j = (int)(rng() % n);
                solver.diff(i, j);
                test_add_or_clause(clauses, {i, false}, {j, false});
                test_add_or_clause(clauses, {i, true}, {j, true});
            } else {
                int i = (int)(rng() % n);
                bool val = (bool)(rng() & 1);
                solver.force(i, val);
                test_add_or_clause(clauses, {i, val}, {i, val});
            }
        }

        bool got = solver.solve();
        bool want = test_naive_sat(n, clauses);
        assert(got == want);
        if (got) assert(test_eval_clauses(clauses, solver.answer()));
    }
}

static void test_two_choice_solver() {
    {
        TwoChoiceSolver solver(0);
        assert(solver.solve());
        assert(solver.choices().empty());
    }
    {
        TwoChoiceSolver solver(2);
        solver.same(0, 1);
        solver.force(0, 0);
        assert(solver.solve());
        assert(solver.choice(0) == 0);
        assert(solver.choice(1) == 0);
    }
    {
        TwoChoiceSolver solver(2);
        std::array<std::array<bool, 2>, 2> ok{};
        ok[0][0] = true;
        ok[0][1] = false;
        ok[1][0] = false;
        ok[1][1] = true;
        solver.add_relation(0, 1, ok);
        solver.force(0, 0);
        solver.force(1, 1);
        assert(!solver.solve());
    }

    std::mt19937 rng(3);
    for (int rep = 0; rep < 3000; rep++) {
        int n = 1 + (int)(rng() % 8);
        int ops = (int)(rng() % 35);
        TwoChoiceSolver solver(n);
        std::vector<TestClause> clauses;

        auto lit_choice = [](int i, int c) -> TestLit {
            return {i, test_choice_value(c)};
        };

        for (int t = 0; t < ops; t++) {
            int type = (int)(rng() % 7);
            int i = (int)(rng() % n), j = (int)(rng() % n);
            int ci = (int)(rng() % 2), cj = (int)(rng() % 2);
            if (type == 0) {
                solver.add_or(i, ci, j, cj);
                test_add_or_clause(clauses, lit_choice(i, ci), lit_choice(j, cj));
            } else if (type == 1) {
                solver.forbid(i, ci, j, cj);
                test_add_forbid_clause(clauses, lit_choice(i, ci), lit_choice(j, cj));
            } else if (type == 2) {
                solver.imply(i, ci, j, cj);
                test_add_or_clause(clauses, {i, !test_choice_value(ci)}, lit_choice(j, cj));
            } else if (type == 3) {
                solver.force(i, ci);
                test_add_or_clause(clauses, lit_choice(i, ci), lit_choice(i, ci));
            } else if (type == 4) {
                solver.same(i, j);
                test_add_forbid_clause(clauses, lit_choice(i, 0), lit_choice(j, 1));
                test_add_forbid_clause(clauses, lit_choice(i, 1), lit_choice(j, 0));
            } else if (type == 5) {
                solver.diff(i, j);
                test_add_forbid_clause(clauses, lit_choice(i, 0), lit_choice(j, 0));
                test_add_forbid_clause(clauses, lit_choice(i, 1), lit_choice(j, 1));
            } else {
                std::array<std::array<bool, 2>, 2> ok{};
                for (int a = 0; a < 2; a++) {
                    for (int b = 0; b < 2; b++) {
                        ok[a][b] = (bool)(rng() & 1);
                    }
                }
                solver.add_relation(i, j, ok);
                for (int a = 0; a < 2; a++) {
                    for (int b = 0; b < 2; b++) {
                        if (!ok[a][b]) test_add_forbid_clause(clauses, lit_choice(i, a), lit_choice(j, b));
                    }
                }
            }
        }

        bool got = solver.solve();
        bool want = test_naive_sat(n, clauses);
        assert(got == want);
        if (got) {
            std::vector<bool> raw(n);
            auto choices = solver.choices();
            for (int i = 0; i < n; i++) raw[i] = test_choice_value(choices[i]);
            assert(test_eval_clauses(clauses, raw));
        }
    }
}

static void test_two_choice_resource_solver() {
    {
        TwoChoiceResourceSolver solver(0);
        solver.build();
        assert(solver.solve());
        assert(solver.choices().empty());
    }
    {
        TwoChoiceResourceSolver solver(2);
        solver.use(0, 0, 100);
        solver.use(0, 0, 100);
        solver.use(1, 1, 100);
        solver.force(0, 0);
        solver.force(1, 1);
        solver.build();
        assert(!solver.solve());
    }
    {
        TwoChoiceResourceSolver solver(1);
        solver.use(0, 0, 7);
        solver.use(0, 1, 7);
        solver.build();
        assert(solver.solve());
    }

    std::mt19937 rng(4);
    for (int rep = 0; rep < 2000; rep++) {
        int n = 1 + (int)(rng() % 8);
        TwoChoiceResourceSolver solver(n);
        std::vector<TestClause> clauses;
        std::vector<TwoChoiceResourceSolver::Entry> entries;

        auto lit_choice = [](int i, int c) -> TestLit {
            return {i, test_choice_value(c)};
        };

        int manual_ops = (int)(rng() % 10);
        for (int t = 0; t < manual_ops; t++) {
            int type = (int)(rng() % 4);
            int i = (int)(rng() % n), j = (int)(rng() % n);
            int ci = (int)(rng() % 2), cj = (int)(rng() % 2);
            if (type == 0) {
                solver.add_or(i, ci, j, cj);
                test_add_or_clause(clauses, lit_choice(i, ci), lit_choice(j, cj));
            } else if (type == 1) {
                solver.forbid(i, ci, j, cj);
                test_add_forbid_clause(clauses, lit_choice(i, ci), lit_choice(j, cj));
            } else if (type == 2) {
                solver.imply(i, ci, j, cj);
                test_add_or_clause(clauses, {i, !test_choice_value(ci)}, lit_choice(j, cj));
            } else {
                solver.force(i, ci);
                test_add_or_clause(clauses, lit_choice(i, ci), lit_choice(i, ci));
            }
        }

        int u = (int)(rng() % 35);
        for (int t = 0; t < u; t++) {
            int item = (int)(rng() % n);
            int choice = (int)(rng() % 2);
            long long resource = (long long)(rng() % 8);
            solver.use(item, choice, resource);
            entries.push_back({resource, item, choice});
        }

        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            if (a.resource != b.resource) return a.resource < b.resource;
            if (a.item != b.item) return a.item < b.item;
            return a.choice < b.choice;
        });
        entries.erase(std::unique(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.resource == b.resource && a.item == b.item && a.choice == b.choice;
        }), entries.end());
        int l = 0;
        while (l < (int)entries.size()) {
            int r = l + 1;
            while (r < (int)entries.size() && entries[r].resource == entries[l].resource) r++;
            for (int a = l; a < r; a++) {
                for (int b = a + 1; b < r; b++) {
                    if (entries[a].item != entries[b].item) {
                        test_add_forbid_clause(clauses, lit_choice(entries[a].item, entries[a].choice), lit_choice(entries[b].item, entries[b].choice));
                    }
                }
            }
            l = r;
        }

        solver.build();
        bool got = solver.solve();
        bool want = test_naive_sat(n, clauses);
        assert(got == want);
        if (got) {
            std::vector<bool> raw(n);
            auto choices = solver.choices();
            for (int i = 0; i < n; i++) raw[i] = test_choice_value(choices[i]);
            assert(test_eval_clauses(clauses, raw));
        }
    }
}

static void test_two_choice_interval_solver() {
    {
        TwoChoiceIntervalSolver solver(0);
        solver.build();
        assert(solver.solve());
        assert(solver.choices().empty());
    }
    {
        TwoChoiceIntervalSolver solver(2);
        solver.set_interval(0, 0, 0, 10);
        solver.set_interval(0, 1, 20, 30);
        solver.set_interval(1, 0, 10, 20);
        solver.set_interval(1, 1, 5, 15);
        solver.force(0, 0);
        solver.force(1, 0);
        solver.build();
        assert(solver.solve());
    }
    {
        TwoChoiceIntervalSolver solver(2);
        solver.set_interval(0, 0, 0, 10);
        solver.set_interval(0, 1, 20, 30);
        solver.set_interval(1, 0, 5, 15);
        solver.set_interval(1, 1, 40, 50);
        solver.force(0, 0);
        solver.force(1, 0);
        solver.build();
        assert(!solver.solve());
    }
    {
        TwoChoiceIntervalSolver solver(2);
        solver.set_interval(0, 0, 0, 10, 1);
        solver.set_interval(1, 0, 5, 15, 2);
        solver.force(0, 0);
        solver.force(1, 0);
        solver.build();
        assert(solver.solve());
    }

    std::mt19937 rng(5);
    for (int rep = 0; rep < 2000; rep++) {
        int n = 1 + (int)(rng() % 8);
        TwoChoiceIntervalSolver solver(n);
        std::vector<std::array<TwoChoiceIntervalSolver::Interval, 2>> intervals(n);
        std::vector<TestClause> clauses;

        auto lit_choice = [](int i, int c) -> TestLit {
            return {i, test_choice_value(c)};
        };
        auto overlap = [](const auto& a, const auto& b) -> bool {
            return a.group == b.group && a.l < b.r && b.l < a.r;
        };

        for (int i = 0; i < n; i++) {
            for (int c = 0; c < 2; c++) {
                long long l = (long long)(rng() % 12);
                long long r = l + (long long)(rng() % 6);
                long long g = (long long)(rng() % 3);
                intervals[i][c] = {l, r, g};
                solver.set_interval(i, c, l, r, g);
            }
        }

        int manual_ops = (int)(rng() % 8);
        for (int t = 0; t < manual_ops; t++) {
            int i = (int)(rng() % n);
            int ci = (int)(rng() % 2);
            solver.force(i, ci);
            test_add_or_clause(clauses, lit_choice(i, ci), lit_choice(i, ci));
        }

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                for (int ci = 0; ci < 2; ci++) {
                    for (int cj = 0; cj < 2; cj++) {
                        if (overlap(intervals[i][ci], intervals[j][cj])) {
                            test_add_forbid_clause(clauses, lit_choice(i, ci), lit_choice(j, cj));
                        }
                    }
                }
            }
        }

        solver.build();
        bool got = solver.solve();
        bool want = test_naive_sat(n, clauses);
        assert(got == want);
        if (got) {
            std::vector<bool> raw(n);
            auto choices = solver.choices();
            for (int i = 0; i < n; i++) raw[i] = test_choice_value(choices[i]);
            assert(test_eval_clauses(clauses, raw));
        }
    }
}

static void test_two_choice_distance_solver() {
    {
        TwoChoiceDistanceSolver solver(0);
        solver.build(1);
        assert(solver.solve());
        assert(solver.choices().empty());
    }
    {
        TwoChoiceDistanceSolver solver(2);
        solver.set_point(0, 0, 0, 0);
        solver.set_point(0, 1, 100, 100);
        solver.set_point(1, 0, 3, 4);
        solver.set_point(1, 1, 100, 101);
        solver.force(0, 0);
        solver.force(1, 0);
        solver.build(25);
        assert(solver.solve());
    }
    {
        TwoChoiceDistanceSolver solver(2);
        solver.set_point(0, 0, 0, 0);
        solver.set_point(0, 1, 100, 100);
        solver.set_point(1, 0, 3, 4);
        solver.set_point(1, 1, 100, 101);
        solver.force(0, 0);
        solver.force(1, 0);
        solver.build(26);
        assert(!solver.solve());
    }
    {
        TwoChoiceDistanceSolver solver(2);
        solver.set_point(0, 0, 0, 0, 1);
        solver.set_point(1, 0, 0, 0, 2);
        solver.force(0, 0);
        solver.force(1, 0);
        solver.build(1);
        assert(solver.solve());
    }

    std::mt19937 rng(6);
    for (int rep = 0; rep < 2000; rep++) {
        int n = 1 + (int)(rng() % 8);
        long long min_dist2 = (long long)(rng() % 50);
        TwoChoiceDistanceSolver solver(n);
        std::vector<std::array<TwoChoiceDistanceSolver::Point, 2>> points(n);
        std::vector<TestClause> clauses;

        auto lit_choice = [](int i, int c) -> TestLit {
            return {i, test_choice_value(c)};
        };
        auto too_close = [](const auto& a, const auto& b, long long limit_dist2) -> bool {
            if (a.group != b.group) return false;
            __int128 dx = (__int128)a.x - b.x;
            __int128 dy = (__int128)a.y - b.y;
            return dx * dx + dy * dy < (__int128)limit_dist2;
        };

        for (int i = 0; i < n; i++) {
            for (int c = 0; c < 2; c++) {
                long long x = (long long)(rng() % 20) - 10;
                long long y = (long long)(rng() % 20) - 10;
                long long g = (long long)(rng() % 3);
                points[i][c] = {x, y, g};
                solver.set_point(i, c, x, y, g);
            }
        }

        int manual_ops = (int)(rng() % 8);
        for (int t = 0; t < manual_ops; t++) {
            int i = (int)(rng() % n);
            int ci = (int)(rng() % 2);
            solver.force(i, ci);
            test_add_or_clause(clauses, lit_choice(i, ci), lit_choice(i, ci));
        }

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                for (int ci = 0; ci < 2; ci++) {
                    for (int cj = 0; cj < 2; cj++) {
                        if (too_close(points[i][ci], points[j][cj], min_dist2)) {
                            test_add_forbid_clause(clauses, lit_choice(i, ci), lit_choice(j, cj));
                        }
                    }
                }
            }
        }

        solver.build(min_dist2);
        bool got = solver.solve();
        bool want = test_naive_sat(n, clauses);
        assert(got == want);
        if (got) {
            std::vector<bool> raw(n);
            auto choices = solver.choices();
            for (int i = 0; i < n; i++) raw[i] = test_choice_value(choices[i]);
            assert(test_eval_clauses(clauses, raw));
        }
    }
}

int main() {
    test_bool_solver();
    std::cout << "BoolSatSolver tests passed\n";

    test_same_diff_solver();
    std::cout << "SameDiffSatSolver tests passed\n";

    test_two_choice_solver();
    std::cout << "TwoChoiceSolver tests passed\n";

    test_two_choice_resource_solver();
    std::cout << "TwoChoiceResourceSolver tests passed\n";

    test_two_choice_interval_solver();
    std::cout << "TwoChoiceIntervalSolver tests passed\n";

    test_two_choice_distance_solver();
    std::cout << "TwoChoiceDistanceSolver tests passed\n";

    std::cout << "All tests passed\n";
    return 0;
}
#endif
