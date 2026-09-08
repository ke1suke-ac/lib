#pragma once

#include <bits/stdc++.h>

using namespace std;

// atcoder::dsu では自然に扱えない Union-Find 変種と、その典型ユースケース solver 集
// 差分等式、法付き等式、成分加算、rollback、永続化、successor、要素移動、オンライン橋などを扱う
// 各データ構造と、そのデータ構造だけを使う solver を連続して配置し、必要な範囲だけコピーしやすくしている
// 頂点番号はすべて 0-indexed、区間はすべて半開区間 [left, right) とする

// ============================================================================
// PotentialDsu
// ============================================================================
// 【用途】potential[y]-potential[x]=difference という差分等式を追加し、相対値と矛盾を管理する重み付き Union-Find
// 【典型】距離差、座標差、高さ差、時刻差、相対順位、差分等式制約、同一成分内の値の差の問い合わせ
// 【使い方】PotentialDsu<long long> uf(n); と作り、uf.merge(x,y,d) で potential[y]-potential[x]=d を追加する
// 【取得】uf.difference(x,y) は potential[y]-potential[x]、uf.consistent(x) は成分内の全制約が整合するかを返す
// 【注意】T は加算・減算・等値比較・T{} による零元生成が可能であること。非可換な演算や不等式制約は扱わない
// 【コピー範囲】この struct だけで重み付き Union-Find として使用できる
// 【計算量】各操作ならし O(α(N))
template<class T>
struct PotentialDsu {
    enum class MergeResult {
        merged,
        already_consistent,
        contradiction,
    };

private:
    vector<int> parent_or_size;
    vector<T> difference_to_parent;
    vector<char> inconsistent;
    int components;

    pair<int, T> leader_and_difference(int vertex) {
        if (parent_or_size[vertex] < 0) return {vertex, T{}};

        int parent = parent_or_size[vertex];
        auto [root, parent_difference] = leader_and_difference(parent);
        difference_to_parent[vertex] = difference_to_parent[vertex] + parent_difference;
        parent_or_size[vertex] = root;
        return {root, difference_to_parent[vertex]};
    }

public:
    // count 個の独立な頂点で初期化する O(N)
    explicit PotentialDsu(int count)
        : parent_or_size(count, -1),
          difference_to_parent(count, T{}),
          inconsistent(count, false),
          components(count) {}

    // potential[y]-potential[x]=difference を追加し、追加結果を返す ならし O(α(N))
    MergeResult merge(int x, int y, const T& difference) {
        auto [root_x, weight_x] = leader_and_difference(x);
        auto [root_y, weight_y] = leader_and_difference(y);

        // 既に同じ成分なら、新しい等式が既存の相対値と一致するかを調べる
        if (root_x == root_y) {
            if (!(weight_y - weight_x == difference)) {
                inconsistent[root_x] = true;
                return MergeResult::contradiction;
            }
            return MergeResult::already_consistent;
        }

        // root_y を root_x の子にした場合の potential[root_y]-potential[root_x] を求める
        T root_difference = difference + weight_x - weight_y;
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) {
            swap(root_x, root_y);
            root_difference = T{} - root_difference;
        }

        // union by size で併合し、根同士の相対値と矛盾情報を新しい根へ集約する
        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        difference_to_parent[root_y] = root_difference;
        inconsistent[root_x] = inconsistent[root_x] || inconsistent[root_y];
        components--;
        return MergeResult::merged;
    }

    // vertex を含む成分の代表元を返す ならし O(α(N))
    int leader(int vertex) {
        return leader_and_difference(vertex).first;
    }

    // potential[vertex]-potential[leader(vertex)] を返す ならし O(α(N))
    T potential_from_leader(int vertex) {
        return leader_and_difference(vertex).second;
    }

    // x と y が同じ成分かを返す ならし O(α(N))
    bool same(int x, int y) {
        return leader(x) == leader(y);
    }

    // potential[y]-potential[x] を返し、非連結なら nullopt を返す ならし O(α(N))
    optional<T> difference(int x, int y) {
        auto [root_x, weight_x] = leader_and_difference(x);
        auto [root_y, weight_y] = leader_and_difference(y);
        if (root_x != root_y) return nullopt;
        return weight_y - weight_x;
    }

    // vertex を含む成分の全差分等式が整合しているかを返す ならし O(α(N))
    bool consistent(int vertex) {
        return !inconsistent[leader(vertex)];
    }

    // vertex を含む成分の頂点数を返す ならし O(α(N))
    int size(int vertex) {
        return -parent_or_size[leader(vertex)];
    }

    // 現在の連結成分数を返す O(1)
    int component_count() const {
        return components;
    }
};

// ============================================================================
// DifferenceEqualitySystemSolver
// ============================================================================
// 【用途】value[y]-value[x]=difference と value[x]=constant からなる等式制約系の整合性を判定し、解を1つ構成する
// 【典型】差分方程式、座標復元、基準点付きポテンシャル、未固定成分ごとに平行移動の自由度が残る等式系
// 【使い方】Equation と FixedValue を配列へ入れ、DifferenceEqualitySystemSolver<long long>::solve(n,equations,fixed_values) を呼ぶ
// 【取得】Result::consistent が true のとき Result::value が解の1つ。anchored[x] は x の絶対値が制約から一意に固定されたかを表す
// 【注意】未固定成分は代表元の値を0とした解を返す。矛盾時の value と anchored は利用しない
// 【コピー範囲】PotentialDsu とこの struct をコピーして使用する
// 【計算量】O((N+E+F)α(N)+N log N)
template<class T>
struct DifferenceEqualitySystemSolver {
    struct Equation {
        int x;
        int y;
        T difference;
    };

    struct FixedValue {
        int x;
        T value;
    };

    struct Result {
        bool consistent;
        int contradiction_kind;
        int contradiction_index;
        int free_components;
        vector<T> value;
        vector<char> anchored;
    };

    // 等式制約系を解き、整合性・矛盾位置・解の1つ・自由成分数を返す O((N+E+F)α(N)+N log N)
    static Result solve(
        int count,
        const vector<Equation>& equations,
        const vector<FixedValue>& fixed_values
    ) {
        int constant_vertex = count;
        PotentialDsu<T> uf(count + 1);
        int contradiction_kind = 0;
        int contradiction_index = -1;

        // 頂点間の差分等式を順に追加し、最初の矛盾位置を記録する
        for (int index = 0; index < static_cast<int>(equations.size()); index++) {
            const Equation& equation = equations[index];
            auto result = uf.merge(equation.x, equation.y, equation.difference);
            if (result == PotentialDsu<T>::MergeResult::contradiction && contradiction_kind == 0) {
                contradiction_kind = 1;
                contradiction_index = index;
            }
        }

        // constant_vertex の値を0とみなし、value[x]=constant を差分等式として追加する
        for (int index = 0; index < static_cast<int>(fixed_values.size()); index++) {
            const FixedValue& fixed = fixed_values[index];
            auto result = uf.merge(constant_vertex, fixed.x, fixed.value);
            if (result == PotentialDsu<T>::MergeResult::contradiction && contradiction_kind == 0) {
                contradiction_kind = 2;
                contradiction_index = index;
            }
        }

        bool all_consistent = contradiction_kind == 0;
        vector<T> values(count, T{});
        vector<char> anchored(count, false);
        vector<int> free_roots;
        free_roots.reserve(count);

        // 固定成分は定数頂点との差を、未固定成分は代表元との差を解として採用する
        for (int vertex = 0; vertex < count; vertex++) {
            if (!uf.consistent(vertex)) all_consistent = false;
            if (uf.same(constant_vertex, vertex)) {
                anchored[vertex] = true;
                values[vertex] = *uf.difference(constant_vertex, vertex);
            } else {
                values[vertex] = uf.potential_from_leader(vertex);
                free_roots.push_back(uf.leader(vertex));
            }
        }

        sort(free_roots.begin(), free_roots.end());
        free_roots.erase(unique(free_roots.begin(), free_roots.end()), free_roots.end());
        return {
            all_consistent,
            contradiction_kind,
            contradiction_index,
            static_cast<int>(free_roots.size()),
            move(values),
            move(anchored),
        };
    }
};

// ============================================================================
// DifferenceEqualityQuerySolver
// ============================================================================
// 【用途】差分等式をオンライン追加しながら、2変数間の差が未確定・矛盾・確定のどれかを問い合わせる
// 【典型】制約追加クエリ、座標差問い合わせ、矛盾した成分を区別する重み付き Union-Find 問題
// 【使い方】Operation の type に ADD_EQUATION または QUERY_DIFFERENCE を設定して solve を呼ぶ
// 【取得】各問い合わせの Answer::state は DISCONNECTED、INCONSISTENT、DETERMINED のいずれか
// 【注意】問い合わせ順に Answer を返す。difference は state==DETERMINED のときだけ有効
// 【コピー範囲】PotentialDsu とこの struct をコピーして使用する
// 【計算量】O(Qα(N))
template<class T>
struct DifferenceEqualityQuerySolver {
    static constexpr int ADD_EQUATION = 0;
    static constexpr int QUERY_DIFFERENCE = 1;
    static constexpr int DISCONNECTED = 0;
    static constexpr int INCONSISTENT = 1;
    static constexpr int DETERMINED = 2;

    struct Operation {
        int type;
        int x;
        int y;
        T difference;
    };

    struct Answer {
        int operation_index;
        int state;
        T difference;
    };

    // 操作列を処理し、差分問い合わせへの回答を出現順に返す O(Qα(N))
    static vector<Answer> solve(int count, const vector<Operation>& operations) {
        PotentialDsu<T> uf(count);
        vector<Answer> answers;

        for (int index = 0; index < static_cast<int>(operations.size()); index++) {
            const Operation& operation = operations[index];
            if (operation.type == ADD_EQUATION) {
                uf.merge(operation.x, operation.y, operation.difference);
                continue;
            }

            optional<T> value = uf.difference(operation.x, operation.y);
            if (!value.has_value()) {
                answers.push_back({index, DISCONNECTED, T{}});
            } else if (!uf.consistent(operation.x)) {
                answers.push_back({index, INCONSISTENT, T{}});
            } else {
                answers.push_back({index, DETERMINED, *value});
            }
        }
        return answers;
    }
};

// ============================================================================
// ModPotentialDsu
// ============================================================================
// 【用途】value[y]-value[x]≡difference (mod modulus) という法付き等式を追加し、相対値と矛盾を管理する
// 【典型】大きな法の循環関係、角度差、周期的時刻差、K*N 展開が大きすぎる mod 制約
// 【使い方】ModPotentialDsu uf(n,mod); と作り、uf.merge(x,y,d) で法付き差分等式を追加する
// 【取得】uf.difference(x,y) は [0,modulus) に正規化された value[y]-value[x] を返す
// 【注意】modulus は正。M が小さいだけの追加専用問題は ACL dsu の M*N 展開でも処理できるため本構造は大きな M 向け
// 【コピー範囲】この struct だけで法付き重み付き Union-Find として使用できる
// 【計算量】各操作ならし O(α(N))
struct ModPotentialDsu {
    enum class MergeResult {
        merged,
        already_consistent,
        contradiction,
    };

private:
    vector<int> parent_or_size;
    vector<long long> difference_to_parent;
    vector<char> inconsistent;
    long long modulus;
    int components;

    long long normalize(__int128 value) const {
        __int128 mod = modulus;
        value %= mod;
        if (value < 0) value += mod;
        return static_cast<long long>(value);
    }

    pair<int, long long> leader_and_difference(int vertex) {
        if (parent_or_size[vertex] < 0) return {vertex, 0};

        int parent = parent_or_size[vertex];
        auto [root, parent_difference] = leader_and_difference(parent);
        difference_to_parent[vertex] = normalize(
            static_cast<__int128>(difference_to_parent[vertex]) + parent_difference
        );
        parent_or_size[vertex] = root;
        return {root, difference_to_parent[vertex]};
    }

public:
    // count 個の独立な頂点と正の法 modulus で初期化する O(N)
    ModPotentialDsu(int count, long long mod)
        : parent_or_size(count, -1),
          difference_to_parent(count, 0),
          inconsistent(count, false),
          modulus(mod),
          components(count) {
        assert(modulus > 0);
    }

    // value[y]-value[x]≡difference (mod modulus) を追加し、追加結果を返す ならし O(α(N))
    MergeResult merge(int x, int y, long long difference) {
        auto [root_x, weight_x] = leader_and_difference(x);
        auto [root_y, weight_y] = leader_and_difference(y);
        long long normalized_difference = normalize(difference);

        // 同一成分なら、現在の差が新しい合同式を満たすかを調べる
        if (root_x == root_y) {
            long long current = normalize(static_cast<__int128>(weight_y) - weight_x);
            if (current != normalized_difference) {
                inconsistent[root_x] = true;
                return MergeResult::contradiction;
            }
            return MergeResult::already_consistent;
        }

        // root_y を root_x の子にした場合の value[root_y]-value[root_x] を求める
        long long root_difference = normalize(
            static_cast<__int128>(normalized_difference) + weight_x - weight_y
        );
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) {
            swap(root_x, root_y);
            root_difference = normalize(-static_cast<__int128>(root_difference));
        }

        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        difference_to_parent[root_y] = root_difference;
        inconsistent[root_x] = inconsistent[root_x] || inconsistent[root_y];
        components--;
        return MergeResult::merged;
    }

    // vertex を含む成分の代表元を返す ならし O(α(N))
    int leader(int vertex) {
        return leader_and_difference(vertex).first;
    }

    // value[vertex]-value[leader(vertex)] を [0,modulus) で返す ならし O(α(N))
    long long potential_from_leader(int vertex) {
        return leader_and_difference(vertex).second;
    }

    // x と y が同じ成分かを返す ならし O(α(N))
    bool same(int x, int y) {
        return leader(x) == leader(y);
    }

    // value[y]-value[x] を [0,modulus) で返し、非連結なら nullopt を返す ならし O(α(N))
    optional<long long> difference(int x, int y) {
        auto [root_x, weight_x] = leader_and_difference(x);
        auto [root_y, weight_y] = leader_and_difference(y);
        if (root_x != root_y) return nullopt;
        return normalize(static_cast<__int128>(weight_y) - weight_x);
    }

    // vertex を含む成分の全合同式が整合しているかを返す ならし O(α(N))
    bool consistent(int vertex) {
        return !inconsistent[leader(vertex)];
    }

    // vertex を含む成分の頂点数を返す ならし O(α(N))
    int size(int vertex) {
        return -parent_or_size[leader(vertex)];
    }

    // 使用している法を返す O(1)
    long long mod() const {
        return modulus;
    }

    // 現在の連結成分数を返す O(1)
    int component_count() const {
        return components;
    }
};

// ============================================================================
// ModEqualitySystemSolver
// ============================================================================
// 【用途】value[y]-value[x]≡difference と value[x]≡constant からなる法付き等式制約系を判定し、解を1つ構成する
// 【典型】周期変数の復元、大きな mod の合同方程式、固定値付き循環差分制約
// 【使い方】Equation と FixedValue を用意し、ModEqualitySystemSolver::solve(n,mod,equations,fixed_values) を呼ぶ
// 【取得】整合時は Result::value が [0,mod) の解の1つ、anchored は絶対剰余が固定された頂点を表す
// 【注意】未固定成分は代表元の値を0とした解を返す。矛盾時の解配列は利用しない
// 【コピー範囲】ModPotentialDsu とこの struct をコピーして使用する
// 【計算量】O((N+E+F)α(N)+N log N)
struct ModEqualitySystemSolver {
    struct Equation {
        int x;
        int y;
        long long difference;
    };

    struct FixedValue {
        int x;
        long long value;
    };

    struct Result {
        bool consistent;
        int contradiction_kind;
        int contradiction_index;
        int free_components;
        vector<long long> value;
        vector<char> anchored;
    };

    // 法付き等式制約系を解き、整合性・矛盾位置・解の1つ・自由成分数を返す O((N+E+F)α(N)+N log N)
    static Result solve(
        int count,
        long long modulus,
        const vector<Equation>& equations,
        const vector<FixedValue>& fixed_values
    ) {
        int constant_vertex = count;
        ModPotentialDsu uf(count + 1, modulus);
        int contradiction_kind = 0;
        int contradiction_index = -1;

        for (int index = 0; index < static_cast<int>(equations.size()); index++) {
            const Equation& equation = equations[index];
            auto result = uf.merge(equation.x, equation.y, equation.difference);
            if (result == ModPotentialDsu::MergeResult::contradiction && contradiction_kind == 0) {
                contradiction_kind = 1;
                contradiction_index = index;
            }
        }

        for (int index = 0; index < static_cast<int>(fixed_values.size()); index++) {
            const FixedValue& fixed = fixed_values[index];
            auto result = uf.merge(constant_vertex, fixed.x, fixed.value);
            if (result == ModPotentialDsu::MergeResult::contradiction && contradiction_kind == 0) {
                contradiction_kind = 2;
                contradiction_index = index;
            }
        }

        bool all_consistent = contradiction_kind == 0;
        vector<long long> values(count, 0);
        vector<char> anchored(count, false);
        vector<int> free_roots;
        free_roots.reserve(count);

        for (int vertex = 0; vertex < count; vertex++) {
            if (!uf.consistent(vertex)) all_consistent = false;
            if (uf.same(constant_vertex, vertex)) {
                anchored[vertex] = true;
                values[vertex] = *uf.difference(constant_vertex, vertex);
            } else {
                values[vertex] = uf.potential_from_leader(vertex);
                free_roots.push_back(uf.leader(vertex));
            }
        }

        sort(free_roots.begin(), free_roots.end());
        free_roots.erase(unique(free_roots.begin(), free_roots.end()), free_roots.end());
        return {
            all_consistent,
            contradiction_kind,
            contradiction_index,
            static_cast<int>(free_roots.size()),
            move(values),
            move(anchored),
        };
    }
};


// ============================================================================
// SignedPotentialDsu
// ============================================================================
// 【用途】value[y]=sign*value[x]+offset、sign∈{+1,-1} という符号付き等式と固定値を管理する Union-Find
// 【典型】value[x]+value[y]=sum、value[y]-value[x]=difference の混在、符号反転を含む等式系、半整数解の検出
// 【使い方】merge(x,y,sign,offset)、add_sum(x,y,sum)、add_difference(x,y,difference)、add_fixed(x,value) を使う
// 【取得】twice_value(x) は絶対値が決まれば 2*value[x]、relation(x,y) は y=sign*x+twice_offset/2 を返す
// 【注意】整数係数を正確に扱うため値を2倍して保持する。integer_consistent は整数解、consistent は半整数を許す解の存在を表す
// 【コピー範囲】この struct だけで符号付き等式 Union-Find として使用できる
// 【計算量】各操作ならし O(α(N))
struct SignedPotentialDsu {
    enum class MergeResult {
        merged,
        already_consistent,
        contradiction,
    };

    struct Relation {
        int sign;
        long long twice_offset;
    };

private:
    struct Transform {
        int root;
        int sign;
        long long twice_offset;
    };

    vector<int> parent_or_size;
    vector<int> sign_to_parent;
    vector<long long> twice_offset_to_parent;
    vector<char> root_fixed;
    vector<long long> root_twice_value;
    vector<char> inconsistent;
    int components;

    Transform leader_and_transform(int vertex) {
        if (parent_or_size[vertex] < 0) return {vertex, 1, 0};

        int parent = parent_or_size[vertex];
        int old_sign = sign_to_parent[vertex];
        long long old_offset = twice_offset_to_parent[vertex];
        Transform parent_transform = leader_and_transform(parent);
        sign_to_parent[vertex] = old_sign * parent_transform.sign;
        twice_offset_to_parent[vertex] = old_sign * parent_transform.twice_offset + old_offset;
        parent_or_size[vertex] = parent_transform.root;
        return {parent_transform.root, sign_to_parent[vertex], twice_offset_to_parent[vertex]};
    }

    bool fix_root(int root, long long twice_value) {
        if (!root_fixed[root]) {
            root_fixed[root] = true;
            root_twice_value[root] = twice_value;
            return true;
        }
        if (root_twice_value[root] == twice_value) return true;
        inconsistent[root] = true;
        return false;
    }

public:
    // count 個の独立な変数で初期化する O(N)
    explicit SignedPotentialDsu(int count)
        : parent_or_size(count, -1),
          sign_to_parent(count, 1),
          twice_offset_to_parent(count, 0),
          root_fixed(count, false),
          root_twice_value(count, 0),
          inconsistent(count, false),
          components(count) {}

    // value[y]=sign*value[x]+offset を追加し、追加結果を返す ならし O(α(N))
    MergeResult merge(int x, int y, int sign, long long offset) {
        assert(sign == 1 || sign == -1);
        Transform transform_x = leader_and_transform(x);
        Transform transform_y = leader_and_transform(y);

        // root_y = root_sign*root_x + root_offset/2 となる根同士の関係を導く
        int root_sign = transform_y.sign * sign * transform_x.sign;
        long long root_offset = transform_y.sign *
            (sign * transform_x.twice_offset + 2 * offset - transform_y.twice_offset);

        // 同じ根に戻る等式は、恒等式・矛盾・根の絶対値固定のいずれかになる
        if (transform_x.root == transform_y.root) {
            int root = transform_x.root;
            if (root_sign == 1) {
                if (root_offset != 0) {
                    inconsistent[root] = true;
                    return MergeResult::contradiction;
                }
                return MergeResult::already_consistent;
            }

            assert(root_offset % 2 == 0);
            long long fixed_value = root_offset / 2;
            if (!fix_root(root, fixed_value)) return MergeResult::contradiction;
            return MergeResult::already_consistent;
        }

        int root_x = transform_x.root;
        int root_y = transform_y.root;
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) {
            swap(root_x, root_y);
            root_offset = -root_sign * root_offset;
        }

        bool contradiction = false;
        if (root_fixed[root_x] && root_fixed[root_y]) {
            long long expected_child = root_sign * root_twice_value[root_x] + root_offset;
            if (root_twice_value[root_y] != expected_child) contradiction = true;
        } else if (!root_fixed[root_x] && root_fixed[root_y]) {
            root_fixed[root_x] = true;
            root_twice_value[root_x] = root_sign * root_twice_value[root_y] - root_sign * root_offset;
        }

        // union by size で根を結び、絶対値固定と矛盾情報を新しい根へ集約する
        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        sign_to_parent[root_y] = root_sign;
        twice_offset_to_parent[root_y] = root_offset;
        inconsistent[root_x] =
            inconsistent[root_x] || inconsistent[root_y] || contradiction;
        components--;
        return contradiction ? MergeResult::contradiction : MergeResult::merged;
    }

    // value[y]-value[x]=difference を追加し、追加結果を返す ならし O(α(N))
    MergeResult add_difference(int x, int y, long long difference) {
        return merge(x, y, 1, difference);
    }

    // value[x]+value[y]=sum を追加し、追加結果を返す ならし O(α(N))
    MergeResult add_sum(int x, int y, long long sum) {
        return merge(x, y, -1, sum);
    }

    // value[x]=value を追加し、追加結果を返す ならし O(α(N))
    MergeResult add_fixed(int x, long long value) {
        Transform transform = leader_and_transform(x);
        long long root_value = transform.sign * (2 * value - transform.twice_offset);
        if (!fix_root(transform.root, root_value)) return MergeResult::contradiction;
        return MergeResult::already_consistent;
    }

    // vertex を含む成分の代表元を返す ならし O(α(N))
    int leader(int vertex) {
        return leader_and_transform(vertex).root;
    }

    // 2*value[vertex]=sign*2*value[leader]+twice_offset の式を返す ならし O(α(N))
    Relation expression_from_leader(int vertex) {
        Transform transform = leader_and_transform(vertex);
        return {transform.sign, transform.twice_offset};
    }

    // y=sign*x+twice_offset/2 の関係を返し、非連結なら nullopt を返す ならし O(α(N))
    optional<Relation> relation(int x, int y) {
        Transform transform_x = leader_and_transform(x);
        Transform transform_y = leader_and_transform(y);
        if (transform_x.root != transform_y.root) return nullopt;
        int relation_sign = transform_y.sign * transform_x.sign;
        long long relation_offset = transform_y.twice_offset - relation_sign * transform_x.twice_offset;
        return Relation{relation_sign, relation_offset};
    }

    // 絶対値が等式から決まる場合に 2*value[x] を返し、自由成分なら nullopt を返す ならし O(α(N))
    optional<long long> twice_value(int x) {
        Transform transform = leader_and_transform(x);
        if (!root_fixed[transform.root]) return nullopt;
        return transform.sign * root_twice_value[transform.root] + transform.twice_offset;
    }

    // x の絶対値が等式から一意に決まっているかを返す ならし O(α(N))
    bool determined(int x) {
        return root_fixed[leader(x)];
    }

    // x を含む成分が半整数を許せば整合しているかを返す ならし O(α(N))
    bool consistent(int x) {
        return !inconsistent[leader(x)];
    }

    // x を含む成分が全変数整数という条件でも整合しているかを返す ならし O(α(N))
    bool integer_consistent(int x) {
        int root = leader(x);
        return !inconsistent[root] && (!root_fixed[root] || root_twice_value[root] % 2 == 0);
    }

    // x と y が同じ成分かを返す ならし O(α(N))
    bool same(int x, int y) {
        return leader(x) == leader(y);
    }

    // x を含む成分の変数数を返す ならし O(α(N))
    int size(int x) {
        return -parent_or_size[leader(x)];
    }

    // 現在の連結成分数を返す O(1)
    int component_count() const {
        return components;
    }
};

// ============================================================================
// SignedEqualitySystemSolver
// ============================================================================
// 【用途】value[y]=sign*value[x]+offset と固定値からなる符号付き等式系を判定し、半整数単位で解を1つ構成する
// 【典型】和の等式 value[x]+value[y]=sum、差の等式、符号反転を含む連立等式、整数解と半整数解の区別
// 【使い方】Equation の sign を +1 または -1 にし、SignedEqualitySystemSolver::solve(n,equations,fixed_values) を呼ぶ
// 【取得】twice_value[x] は 2*value[x]。integer_consistent が true なら全要素が偶数で整数解として2で割れる
// 【注意】未固定成分は代表元を0とした解を返す。rational_consistent が false の場合は解配列を利用しない
// 【コピー範囲】SignedPotentialDsu とこの struct をコピーして使用する
// 【計算量】O((N+E+F)α(N)+N log N)
struct SignedEqualitySystemSolver {
    struct Equation {
        int x;
        int y;
        int sign;
        long long offset;
    };

    struct FixedValue {
        int x;
        long long value;
    };

    struct Result {
        bool rational_consistent;
        bool integer_consistent;
        int contradiction_kind;
        int contradiction_index;
        int free_components;
        vector<long long> twice_value;
        vector<char> determined;
    };

    // 符号付き等式系を解き、整合性・2倍値の解・自由成分数を返す O((N+E+F)α(N)+N log N)
    static Result solve(
        int count,
        const vector<Equation>& equations,
        const vector<FixedValue>& fixed_values
    ) {
        SignedPotentialDsu uf(count);
        int contradiction_kind = 0;
        int contradiction_index = -1;

        for (int index = 0; index < static_cast<int>(equations.size()); index++) {
            const Equation& equation = equations[index];
            auto result = uf.merge(equation.x, equation.y, equation.sign, equation.offset);
            if (result == SignedPotentialDsu::MergeResult::contradiction && contradiction_kind == 0) {
                contradiction_kind = 1;
                contradiction_index = index;
            }
        }
        for (int index = 0; index < static_cast<int>(fixed_values.size()); index++) {
            const FixedValue& fixed = fixed_values[index];
            auto result = uf.add_fixed(fixed.x, fixed.value);
            if (result == SignedPotentialDsu::MergeResult::contradiction && contradiction_kind == 0) {
                contradiction_kind = 2;
                contradiction_index = index;
            }
        }

        bool rational_consistent = contradiction_kind == 0;
        bool integer_consistent = contradiction_kind == 0;
        vector<long long> values(count, 0);
        vector<char> determined(count, false);
        vector<int> free_roots;

        // 固定成分は確定した2倍値を、自由成分は根の2倍値を0とした式の定数項を採用する
        for (int vertex = 0; vertex < count; vertex++) {
            if (!uf.consistent(vertex)) rational_consistent = false;
            if (!uf.integer_consistent(vertex)) integer_consistent = false;
            optional<long long> fixed_value = uf.twice_value(vertex);
            if (fixed_value.has_value()) {
                determined[vertex] = true;
                values[vertex] = *fixed_value;
            } else {
                values[vertex] = uf.expression_from_leader(vertex).twice_offset;
                free_roots.push_back(uf.leader(vertex));
            }
        }

        sort(free_roots.begin(), free_roots.end());
        free_roots.erase(unique(free_roots.begin(), free_roots.end()), free_roots.end());
        return {
            rational_consistent,
            integer_consistent,
            contradiction_kind,
            contradiction_index,
            static_cast<int>(free_roots.size()),
            move(values),
            move(determined),
        };
    }
};

// ============================================================================
// ComponentAddDsu
// ============================================================================
// 【用途】成分全体への加算、成分併合、各要素が累積して受け取った値の取得を処理する Union-Find
// 【典型】ギルド全員への経験値、グループ報酬、merge 前後をまたぐ成分加算、union/add/get 問題
// 【使い方】ComponentAddDsu<long long> uf(n); と作り、merge、add_component、get を呼ぶ
// 【取得】get(x) は x が所属してきた各成分への加算をすべて反映した x の現在値を返す
// 【注意】単なる成分合計ではなく各要素の値を保存する。T は加算・減算・T{} が可能であること
// 【コピー範囲】この struct だけで成分加算 Union-Find として使用できる
// 【計算量】各操作ならし O(α(N))
template<class T>
struct ComponentAddDsu {
private:
    vector<int> parent_or_size;
    vector<T> offset_to_parent;
    vector<T> component_add;

    int compress(int vertex) {
        if (parent_or_size[vertex] < 0) return vertex;
        int parent = parent_or_size[vertex];
        int root = compress(parent);
        offset_to_parent[vertex] = offset_to_parent[vertex] + offset_to_parent[parent];
        parent_or_size[vertex] = root;
        return root;
    }

public:
    // count 個の要素を値0で初期化する O(N)
    explicit ComponentAddDsu(int count)
        : parent_or_size(count, -1), offset_to_parent(count, T{}), component_add(count, T{}) {}

    // 各要素の初期値で初期化する O(N)
    explicit ComponentAddDsu(const vector<T>& initial_values)
        : parent_or_size(initial_values.size(), -1),
          offset_to_parent(initial_values.size(), T{}),
          component_add(initial_values) {}

    // x と y の成分を併合し、実際に併合したかを返す ならし O(α(N))
    bool merge(int x, int y) {
        int root_x = compress(x);
        int root_y = compress(y);
        if (root_x == root_y) return false;
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) swap(root_x, root_y);

        // root_y 側の各要素の現在値を変えないよう、根同士の加算差を辺へ持たせる
        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        offset_to_parent[root_y] = component_add[root_y] - component_add[root_x];
        return true;
    }

    // x を含む成分の全要素へ value を加算する ならし O(α(N))
    void add_component(int x, const T& value) {
        component_add[compress(x)] = component_add[compress(x)] + value;
    }

    // x が現在までに受け取った累積値を返す ならし O(α(N))
    T get(int x) {
        int root = compress(x);
        return component_add[root] + offset_to_parent[x];
    }

    // x と y が同じ成分かを返す ならし O(α(N))
    bool same(int x, int y) {
        return compress(x) == compress(y);
    }

    // x を含む成分の代表元を返す ならし O(α(N))
    int leader(int x) {
        return compress(x);
    }

    // x を含む成分の要素数を返す ならし O(α(N))
    int size(int x) {
        return -parent_or_size[compress(x)];
    }
};

// ============================================================================
// ComponentAddQuerySolver
// ============================================================================
// 【用途】成分併合、成分全体加算、1要素の値取得からなる操作列をそのまま処理する
// 【典型】経験値 Union-Find、ギルド報酬、union/add/get 形式のオンラインクエリ
// 【使い方】Operation の type を MERGE、ADD_COMPONENT、QUERY_VALUE のいずれかにして solve を呼ぶ
// 【取得】QUERY_VALUE の回答だけを出現順に vector<T> で返す
// 【注意】ADD_COMPONENT では x と value を、MERGE では x と y を使用する
// 【コピー範囲】ComponentAddDsu とこの struct をコピーして使用する
// 【計算量】O(Qα(N))
template<class T>
struct ComponentAddQuerySolver {
    static constexpr int MERGE = 0;
    static constexpr int ADD_COMPONENT = 1;
    static constexpr int QUERY_VALUE = 2;

    struct Operation {
        int type;
        int x;
        int y;
        T value;
    };

    // 操作列を処理し、値取得クエリへの回答を出現順に返す O(Qα(N))
    static vector<T> solve(int count, const vector<Operation>& operations) {
        ComponentAddDsu<T> uf(count);
        vector<T> answers;
        for (const Operation& operation : operations) {
            if (operation.type == MERGE) {
                uf.merge(operation.x, operation.y);
            } else if (operation.type == ADD_COMPONENT) {
                uf.add_component(operation.x, operation.value);
            } else {
                answers.push_back(uf.get(operation.x));
            }
        }
        return answers;
    }
};

// ============================================================================
// RollbackDsu
// ============================================================================
// 【用途】merge を任意の snapshot まで巻き戻せる Union-Find
// 【典型】時間 segment tree、オフライン動的連結性、DFS 中だけ有効な辺、分岐バージョン、一時的な仮定の検証
// 【使い方】int s=uf.snapshot(); の後に merge を行い、uf.rollback(s); で元の状態へ戻す
// 【取得】same、size、component_count は現在の履歴位置における状態を返す
// 【注意】rollback のためパス圧縮は行わない。union by size により leader は O(log N)
// 【コピー範囲】この struct だけで rollback Union-Find として使用できる
// 【計算量】leader・same・size・merge は O(log N)、undo 1回は O(1)、rollback は取り消す操作数に比例
struct RollbackDsu {
private:
    struct History {
        int root_x;
        int root_y;
        int parent_x;
        int parent_y;
    };

    vector<int> parent_or_size;
    vector<History> history;
    int components;

public:
    // count 個の独立な頂点で初期化する O(N)
    explicit RollbackDsu(int count) : parent_or_size(count, -1), components(count) {}

    // vertex を含む成分の代表元を返す O(log N)
    int leader(int vertex) const {
        while (parent_or_size[vertex] >= 0) vertex = parent_or_size[vertex];
        return vertex;
    }

    // x と y の成分を併合し、実際に併合したかを返す O(log N)
    bool merge(int x, int y) {
        int root_x = leader(x);
        int root_y = leader(y);
        if (root_x == root_y) {
            history.push_back({-1, -1, 0, 0});
            return false;
        }
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) swap(root_x, root_y);

        history.push_back({root_x, root_y, parent_or_size[root_x], parent_or_size[root_y]});
        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        components--;
        return true;
    }

    // x と y が同じ成分かを返す O(log N)
    bool same(int x, int y) const {
        return leader(x) == leader(y);
    }

    // vertex を含む成分の頂点数を返す O(log N)
    int size(int vertex) const {
        return -parent_or_size[leader(vertex)];
    }

    // 現在の連結成分数を返す O(1)
    int component_count() const {
        return components;
    }

    // 現在の履歴位置を返す O(1)
    int snapshot() const {
        return static_cast<int>(history.size());
    }

    // 直前の merge 呼び出し1回分を取り消す O(1)
    void undo() {
        assert(!history.empty());
        History record = history.back();
        history.pop_back();
        if (record.root_x < 0) return;
        parent_or_size[record.root_x] = record.parent_x;
        parent_or_size[record.root_y] = record.parent_y;
        components++;
    }

    // 履歴を指定 snapshot まで巻き戻す O(取り消す merge 呼び出し数)
    void rollback(int target_snapshot) {
        assert(0 <= target_snapshot && target_snapshot <= snapshot());
        while (snapshot() > target_snapshot) undo();
    }
};

// ============================================================================
// RollbackDsuDynamicConnectivitySolver
// ============================================================================
// 【用途】無向辺の追加・削除・連結判定が混在する操作列をオフラインで処理する
// 【典型】一般の削除を逆順追加にできない動的連結性、同じ辺の多重追加、時間区間中だけ存在する辺
// 【使い方】Operation を時系列順に並べ、ADD_EDGE、REMOVE_EDGE、QUERY_SAME を指定して solve を呼ぶ
// 【取得】QUERY_SAME の回答を出現順に vector<char> で返す
// 【注意】各 REMOVE_EDGE の時点で同じ無向辺が1本以上 active であること。辺は内部で (min,max) に正規化する
// 【コピー範囲】RollbackDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)、K は時間 segment tree に登録される辺イベント数
struct RollbackDsuDynamicConnectivitySolver {
    static constexpr int ADD_EDGE = 0;
    static constexpr int REMOVE_EDGE = 1;
    static constexpr int QUERY_SAME = 2;

    struct Operation {
        int type;
        int u;
        int v;
    };

private:
    struct Edge {
        int u;
        int v;
    };

    struct Event {
        int u;
        int v;
        int time;
        int delta;
    };

    struct Interval {
        int left;
        int right;
        Edge edge;
    };

    static vector<Interval> build_intervals(const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        vector<Event> events;
        events.reserve(query_count);

        for (int time = 0; time < query_count; time++) {
            const Operation& operation = operations[time];
            if (operation.type == QUERY_SAME) continue;
            int u = operation.u;
            int v = operation.v;
            if (u > v) swap(u, v);
            int delta = operation.type == ADD_EDGE ? 1 : -1;
            events.push_back({u, v, time, delta});
        }

        sort(events.begin(), events.end(), [](const Event& lhs, const Event& rhs) {
            return tie(lhs.u, lhs.v, lhs.time) < tie(rhs.u, rhs.v, rhs.time);
        });

        vector<Interval> intervals;
        for (int begin = 0; begin < static_cast<int>(events.size());) {
            int end = begin;
            while (
                end < static_cast<int>(events.size()) &&
                events[end].u == events[begin].u &&
                events[end].v == events[begin].v
            ) {
                end++;
            }

            int active_count = 0;
            int start_time = -1;
            for (int index = begin; index < end; index++) {
                if (events[index].delta > 0) {
                    if (active_count == 0) start_time = events[index].time;
                    active_count++;
                } else {
                    assert(active_count > 0);
                    active_count--;
                    if (active_count == 0) {
                        intervals.push_back({
                            start_time,
                            events[index].time,
                            {events[index].u, events[index].v},
                        });
                    }
                }
            }
            if (active_count > 0) {
                intervals.push_back({start_time, query_count, {events[begin].u, events[begin].v}});
            }
            begin = end;
        }
        return intervals;
    }

    static void add_interval(
        vector<vector<Edge>>& segment_tree,
        int node,
        int left,
        int right,
        int query_left,
        int query_right,
        const Edge& edge
    ) {
        if (query_right <= left || right <= query_left) return;
        if (query_left <= left && right <= query_right) {
            segment_tree[node].push_back(edge);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(segment_tree, node * 2, left, middle, query_left, query_right, edge);
        add_interval(segment_tree, node * 2 + 1, middle, right, query_left, query_right, edge);
    }

    static void dfs(
        const vector<vector<Edge>>& segment_tree,
        const vector<Operation>& operations,
        int node,
        int left,
        int right,
        RollbackDsu& uf,
        vector<char>& answers
    ) {
        int saved = uf.snapshot();
        for (const Edge& edge : segment_tree[node]) uf.merge(edge.u, edge.v);

        if (right - left == 1) {
            const Operation& operation = operations[left];
            if (operation.type == QUERY_SAME) {
                answers.push_back(uf.same(operation.u, operation.v));
            }
        } else {
            int middle = left + (right - left) / 2;
            dfs(segment_tree, operations, node * 2, left, middle, uf, answers);
            dfs(segment_tree, operations, node * 2 + 1, middle, right, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、連結判定への回答を出現順に返す O((Q+K)log Q log N)
    static vector<char> solve(int count, const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};

        vector<vector<Edge>> segment_tree(operations.size() * 4);
        vector<Interval> intervals = build_intervals(operations);
        for (const Interval& interval : intervals) {
            if (interval.left < interval.right) {
                add_interval(
                    segment_tree,
                    1,
                    0,
                    query_count,
                    interval.left,
                    interval.right,
                    interval.edge
                );
            }
        }

        RollbackDsu uf(count);
        vector<char> answers;
        dfs(segment_tree, operations, 1, 0, query_count, uf, answers);
        return answers;
    }
};

// ============================================================================
// RollbackDsuDynamicComponentCountSolver
// ============================================================================
// 【用途】無向辺の追加・削除に対し、各時点の連結成分数をオフラインで答える
// 【典型】動的な島数・クラスタ数・ネットワーク数、削除と再追加が混在する成分数クエリ
// 【使い方】ADD_EDGE、REMOVE_EDGE、QUERY_COMPONENT_COUNT を時系列順に並べて solve を呼ぶ
// 【取得】成分数問い合わせの回答を出現順に vector<int> で返す
// 【注意】各 REMOVE_EDGE は active な同一辺を1本削除する。多重辺を正しく扱う
// 【コピー範囲】RollbackDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)
struct RollbackDsuDynamicComponentCountSolver {
    static constexpr int ADD_EDGE = 0;
    static constexpr int REMOVE_EDGE = 1;
    static constexpr int QUERY_COMPONENT_COUNT = 2;

    struct Operation {
        int type;
        int u;
        int v;
    };

private:
    struct Edge { int u; int v; };
    struct Event { int u; int v; int time; int delta; };
    struct Interval { int left; int right; Edge edge; };

    static vector<Interval> build_intervals(const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        vector<Event> events;
        events.reserve(query_count);
        for (int time = 0; time < query_count; time++) {
            const Operation& operation = operations[time];
            if (operation.type == QUERY_COMPONENT_COUNT) continue;
            int u = operation.u;
            int v = operation.v;
            if (u > v) swap(u, v);
            events.push_back({u, v, time, operation.type == ADD_EDGE ? 1 : -1});
        }
        sort(events.begin(), events.end(), [](const Event& lhs, const Event& rhs) {
            return tie(lhs.u, lhs.v, lhs.time) < tie(rhs.u, rhs.v, rhs.time);
        });

        vector<Interval> intervals;
        for (int begin = 0; begin < static_cast<int>(events.size());) {
            int end = begin;
            while (end < static_cast<int>(events.size()) &&
                   events[end].u == events[begin].u && events[end].v == events[begin].v) {
                end++;
            }
            int active_count = 0;
            int start_time = -1;
            for (int index = begin; index < end; index++) {
                if (events[index].delta > 0) {
                    if (active_count == 0) start_time = events[index].time;
                    active_count++;
                } else {
                    assert(active_count > 0);
                    active_count--;
                    if (active_count == 0) {
                        intervals.push_back({start_time, events[index].time, {events[index].u, events[index].v}});
                    }
                }
            }
            if (active_count > 0) intervals.push_back({start_time, query_count, {events[begin].u, events[begin].v}});
            begin = end;
        }
        return intervals;
    }

    static void add_interval(vector<vector<Edge>>& tree, int node, int left, int right,
                             int ql, int qr, const Edge& edge) {
        if (qr <= left || right <= ql) return;
        if (ql <= left && right <= qr) {
            tree[node].push_back(edge);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(tree, node * 2, left, middle, ql, qr, edge);
        add_interval(tree, node * 2 + 1, middle, right, ql, qr, edge);
    }

    static void dfs(const vector<vector<Edge>>& tree, const vector<Operation>& operations,
                    int node, int left, int right, RollbackDsu& uf, vector<int>& answers) {
        int saved = uf.snapshot();
        for (const Edge& edge : tree[node]) uf.merge(edge.u, edge.v);
        if (right - left == 1) {
            if (operations[left].type == QUERY_COMPONENT_COUNT) answers.push_back(uf.component_count());
        } else {
            int middle = left + (right - left) / 2;
            dfs(tree, operations, node * 2, left, middle, uf, answers);
            dfs(tree, operations, node * 2 + 1, middle, right, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、成分数問い合わせへの回答を出現順に返す O((Q+K)log Q log N)
    static vector<int> solve(int count, const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};
        vector<vector<Edge>> tree(operations.size() * 4);
        for (const Interval& interval : build_intervals(operations)) {
            if (interval.left < interval.right) {
                add_interval(tree, 1, 0, query_count, interval.left, interval.right, interval.edge);
            }
        }
        RollbackDsu uf(count);
        vector<int> answers;
        dfs(tree, operations, 1, 0, query_count, uf, answers);
        return answers;
    }
};

// ============================================================================
// RollbackDsuDynamicComponentSizeSolver
// ============================================================================
// 【用途】無向辺の追加・削除に対し、指定頂点が属する成分サイズをオフラインで答える
// 【典型】動的グループ人数、動的島面積、削除を含むクラスタサイズ問い合わせ
// 【使い方】ADD_EDGE、REMOVE_EDGE、QUERY_COMPONENT_SIZE を時系列順に並べ、問い合わせ頂点を u に入れる
// 【取得】成分サイズ問い合わせの回答を出現順に vector<int> で返す
// 【注意】多重辺を扱う。QUERY_COMPONENT_SIZE の v は使用しない
// 【コピー範囲】RollbackDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)
struct RollbackDsuDynamicComponentSizeSolver {
    static constexpr int ADD_EDGE = 0;
    static constexpr int REMOVE_EDGE = 1;
    static constexpr int QUERY_COMPONENT_SIZE = 2;

    struct Operation { int type; int u; int v; };

private:
    struct Edge { int u; int v; };
    struct Event { int u; int v; int time; int delta; };
    struct Interval { int left; int right; Edge edge; };

    static vector<Interval> build_intervals(const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        vector<Event> events;
        events.reserve(query_count);
        for (int time = 0; time < query_count; time++) {
            const Operation& operation = operations[time];
            if (operation.type == QUERY_COMPONENT_SIZE) continue;
            int u = operation.u;
            int v = operation.v;
            if (u > v) swap(u, v);
            events.push_back({u, v, time, operation.type == ADD_EDGE ? 1 : -1});
        }
        sort(events.begin(), events.end(), [](const Event& lhs, const Event& rhs) {
            return tie(lhs.u, lhs.v, lhs.time) < tie(rhs.u, rhs.v, rhs.time);
        });
        vector<Interval> intervals;
        for (int begin = 0; begin < static_cast<int>(events.size());) {
            int end = begin;
            while (end < static_cast<int>(events.size()) &&
                   events[end].u == events[begin].u && events[end].v == events[begin].v) end++;
            int active_count = 0;
            int start_time = -1;
            for (int index = begin; index < end; index++) {
                if (events[index].delta > 0) {
                    if (active_count == 0) start_time = events[index].time;
                    active_count++;
                } else {
                    assert(active_count > 0);
                    active_count--;
                    if (active_count == 0) {
                        intervals.push_back({start_time, events[index].time, {events[index].u, events[index].v}});
                    }
                }
            }
            if (active_count > 0) intervals.push_back({start_time, query_count, {events[begin].u, events[begin].v}});
            begin = end;
        }
        return intervals;
    }

    static void add_interval(vector<vector<Edge>>& tree, int node, int left, int right,
                             int ql, int qr, const Edge& edge) {
        if (qr <= left || right <= ql) return;
        if (ql <= left && right <= qr) {
            tree[node].push_back(edge);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(tree, node * 2, left, middle, ql, qr, edge);
        add_interval(tree, node * 2 + 1, middle, right, ql, qr, edge);
    }

    static void dfs(const vector<vector<Edge>>& tree, const vector<Operation>& operations,
                    int node, int left, int right, RollbackDsu& uf, vector<int>& answers) {
        int saved = uf.snapshot();
        for (const Edge& edge : tree[node]) uf.merge(edge.u, edge.v);
        if (right - left == 1) {
            if (operations[left].type == QUERY_COMPONENT_SIZE) answers.push_back(uf.size(operations[left].u));
        } else {
            int middle = left + (right - left) / 2;
            dfs(tree, operations, node * 2, left, middle, uf, answers);
            dfs(tree, operations, node * 2 + 1, middle, right, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、成分サイズ問い合わせへの回答を出現順に返す O((Q+K)log Q log N)
    static vector<int> solve(int count, const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};
        vector<vector<Edge>> tree(operations.size() * 4);
        for (const Interval& interval : build_intervals(operations)) {
            if (interval.left < interval.right) {
                add_interval(tree, 1, 0, query_count, interval.left, interval.right, interval.edge);
            }
        }
        RollbackDsu uf(count);
        vector<int> answers;
        dfs(tree, operations, 1, 0, query_count, uf, answers);
        return answers;
    }
};

// ============================================================================
// RollbackDsuDynamicDisconnectedPairsSolver
// ============================================================================
// 【用途】無向辺の追加・削除に対し、異なる成分に属する unordered な頂点ペア数をオフラインで答える
// 【典型】動的不便さ、切断ペア数、辺削除と再追加が混在する連結ペア統計
// 【使い方】ADD_EDGE、REMOVE_EDGE、QUERY_DISCONNECTED_PAIRS を時系列順に並べて solve を呼ぶ
// 【取得】非連結ペア数問い合わせの回答を出現順に vector<long long> で返す
// 【注意】多重辺を扱う。自己ループはペア数に影響しない
// 【コピー範囲】RollbackDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)
struct RollbackDsuDynamicDisconnectedPairsSolver {
    static constexpr int ADD_EDGE = 0;
    static constexpr int REMOVE_EDGE = 1;
    static constexpr int QUERY_DISCONNECTED_PAIRS = 2;

    struct Operation { int type; int u; int v; };

private:
    struct Edge { int u; int v; };
    struct Event { int u; int v; int time; int delta; };
    struct Interval { int left; int right; Edge edge; };

    static vector<Interval> build_intervals(const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        vector<Event> events;
        events.reserve(query_count);
        for (int time = 0; time < query_count; time++) {
            const Operation& operation = operations[time];
            if (operation.type == QUERY_DISCONNECTED_PAIRS) continue;
            int u = operation.u;
            int v = operation.v;
            if (u > v) swap(u, v);
            events.push_back({u, v, time, operation.type == ADD_EDGE ? 1 : -1});
        }
        sort(events.begin(), events.end(), [](const Event& lhs, const Event& rhs) {
            return tie(lhs.u, lhs.v, lhs.time) < tie(rhs.u, rhs.v, rhs.time);
        });
        vector<Interval> intervals;
        for (int begin = 0; begin < static_cast<int>(events.size());) {
            int end = begin;
            while (end < static_cast<int>(events.size()) &&
                   events[end].u == events[begin].u && events[end].v == events[begin].v) end++;
            int active_count = 0;
            int start_time = -1;
            for (int index = begin; index < end; index++) {
                if (events[index].delta > 0) {
                    if (active_count == 0) start_time = events[index].time;
                    active_count++;
                } else {
                    assert(active_count > 0);
                    active_count--;
                    if (active_count == 0) {
                        intervals.push_back({start_time, events[index].time, {events[index].u, events[index].v}});
                    }
                }
            }
            if (active_count > 0) intervals.push_back({start_time, query_count, {events[begin].u, events[begin].v}});
            begin = end;
        }
        return intervals;
    }

    static void add_interval(vector<vector<Edge>>& tree, int node, int left, int right,
                             int ql, int qr, const Edge& edge) {
        if (qr <= left || right <= ql) return;
        if (ql <= left && right <= qr) {
            tree[node].push_back(edge);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(tree, node * 2, left, middle, ql, qr, edge);
        add_interval(tree, node * 2 + 1, middle, right, ql, qr, edge);
    }

    static void dfs(const vector<vector<Edge>>& tree, const vector<Operation>& operations,
                    int node, int left, int right, RollbackDsu& uf,
                    long long disconnected_pairs, vector<long long>& answers) {
        int saved = uf.snapshot();

        // この時間区間で有効な各辺を追加し、新しく連結になる頂点ペア数を差し引く
        for (const Edge& edge : tree[node]) {
            if (!uf.same(edge.u, edge.v)) {
                disconnected_pairs -= 1LL * uf.size(edge.u) * uf.size(edge.v);
            }
            uf.merge(edge.u, edge.v);
        }

        if (right - left == 1) {
            if (operations[left].type == QUERY_DISCONNECTED_PAIRS) answers.push_back(disconnected_pairs);
        } else {
            int middle = left + (right - left) / 2;
            dfs(tree, operations, node * 2, left, middle, uf, disconnected_pairs, answers);
            dfs(tree, operations, node * 2 + 1, middle, right, uf, disconnected_pairs, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、非連結ペア数問い合わせへの回答を出現順に返す O((Q+K)log Q log N)
    static vector<long long> solve(int count, const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};
        vector<vector<Edge>> tree(operations.size() * 4);
        for (const Interval& interval : build_intervals(operations)) {
            if (interval.left < interval.right) {
                add_interval(tree, 1, 0, query_count, interval.left, interval.right, interval.edge);
            }
        }
        RollbackDsu uf(count);
        vector<long long> answers;
        long long initial = 1LL * count * (count - 1) / 2;
        dfs(tree, operations, 1, 0, query_count, uf, initial, answers);
        return answers;
    }
};

// ============================================================================
// RollbackDsuVersionTreeConnectivitySolver
// ============================================================================
// 【用途】任意の過去バージョンから辺追加または連結問い合わせを分岐させる操作列を、version tree と rollback で処理する
// 【典型】操作履歴が木状に分岐するオフライン問題、完全永続 DSU を使わずに分岐バージョンを処理する問題
// 【使い方】各 Operation::base に -1 または自分より前の操作番号を指定し、MERGE または QUERY_SAME を設定する
// 【取得】QUERY_SAME の回答を操作出現順に vector<char> で返す
// 【注意】各操作自体が新しいバージョンになる。全操作が先に分かる場合は PersistentDsu より軽量
// 【コピー範囲】RollbackDsu とこの struct をコピーして使用する
// 【計算量】O(Q log N)
struct RollbackDsuVersionTreeConnectivitySolver {
    static constexpr int MERGE = 0;
    static constexpr int QUERY_SAME = 1;

    struct Operation {
        int type;
        int base;
        int u;
        int v;
    };

private:
    static void dfs(
        int node,
        int virtual_root,
        const vector<vector<int>>& children,
        const vector<Operation>& operations,
        const vector<int>& query_id,
        RollbackDsu& uf,
        vector<char>& answers
    ) {
        int saved = uf.snapshot();
        if (node != virtual_root) {
            const Operation& operation = operations[node];
            if (operation.type == MERGE) {
                uf.merge(operation.u, operation.v);
            } else {
                answers[query_id[node]] = uf.same(operation.u, operation.v);
            }
        }

        for (int child : children[node]) {
            dfs(child, virtual_root, children, operations, query_id, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 分岐する操作バージョンを処理し、連結問い合わせへの回答を出現順に返す O(Q log N)
    static vector<char> solve(int count, const vector<Operation>& operations) {
        int operation_count = static_cast<int>(operations.size());
        int virtual_root = operation_count;
        vector<vector<int>> children(operations.size() + 1);
        vector<int> query_id(operation_count, -1);
        int answer_count = 0;

        // base から派生する version tree を作り、問い合わせには出現順の id を振る
        for (int index = 0; index < operation_count; index++) {
            int base = operations[index].base;
            assert(base < index);
            int parent = base < 0 ? virtual_root : base;
            children[parent].push_back(index);
            if (operations[index].type == QUERY_SAME) query_id[index] = answer_count++;
        }

        RollbackDsu uf(count);
        vector<char> answers(answer_count, false);
        dfs(virtual_root, virtual_root, children, operations, query_id, uf, answers);
        return answers;
    }
};

// ============================================================================
// RollbackValueDsu
// ============================================================================
// 【用途】rollback 可能な成分併合に加えて、頂点値加算と成分合計を管理する Union-Find
// 【典型】辺追加・削除・頂点加算・成分和問い合わせが混在するオフライン動的グラフ
// 【使い方】snapshot 後に merge と add_value を行い、component_sum で取得し、rollback で状態を戻す
// 【取得】component_sum(x) は現在の履歴位置で x を含む成分の頂点値合計を返す
// 【注意】パス圧縮は行わない。add_value も履歴1件として記録され undo 対象になる
// 【コピー範囲】この struct だけで rollback 成分和 Union-Find として使用できる
// 【計算量】leader・merge・add_value・component_sum は O(log N)、undo 1回は O(1)
struct RollbackValueDsu {
private:
    static constexpr int NO_OPERATION = 0;
    static constexpr int MERGE_OPERATION = 1;
    static constexpr int ADD_OPERATION = 2;

    struct History {
        int type;
        int root_x;
        int root_y;
        int parent_x;
        int parent_y;
        long long sum_x;
        long long sum_y;
        int vertex;
        long long vertex_value;
    };

    vector<int> parent_or_size;
    vector<long long> component_value_sum;
    vector<long long> vertex_value;
    vector<History> history;
    int components;

public:
    // 初期頂点値で初期化する O(N)
    explicit RollbackValueDsu(const vector<long long>& initial_values)
        : parent_or_size(initial_values.size(), -1),
          component_value_sum(initial_values),
          vertex_value(initial_values),
          components(static_cast<int>(initial_values.size())) {}

    // vertex を含む成分の代表元を返す O(log N)
    int leader(int vertex) const {
        while (parent_or_size[vertex] >= 0) vertex = parent_or_size[vertex];
        return vertex;
    }

    // x と y の成分を併合し、実際に併合したかを返す O(log N)
    bool merge(int x, int y) {
        int root_x = leader(x);
        int root_y = leader(y);
        if (root_x == root_y) {
            history.push_back({NO_OPERATION, -1, -1, 0, 0, 0, 0, -1, 0});
            return false;
        }
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) swap(root_x, root_y);

        history.push_back({
            MERGE_OPERATION,
            root_x,
            root_y,
            parent_or_size[root_x],
            parent_or_size[root_y],
            component_value_sum[root_x],
            component_value_sum[root_y],
            -1,
            0,
        });
        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        component_value_sum[root_x] += component_value_sum[root_y];
        components--;
        return true;
    }

    // vertex の値へ delta を加算する O(log N)
    void add_value(int vertex, long long delta) {
        int root = leader(vertex);
        history.push_back({
            ADD_OPERATION,
            root,
            -1,
            0,
            0,
            component_value_sum[root],
            0,
            vertex,
            vertex_value[vertex],
        });
        component_value_sum[root] += delta;
        vertex_value[vertex] += delta;
    }

    // vertex を含む成分の頂点値合計を返す O(log N)
    long long component_sum(int vertex) const {
        return component_value_sum[leader(vertex)];
    }

    // vertex 自身の現在値を返す O(1)
    long long value(int vertex) const {
        return vertex_value[vertex];
    }

    // x と y が同じ成分かを返す O(log N)
    bool same(int x, int y) const {
        return leader(x) == leader(y);
    }

    // vertex を含む成分の頂点数を返す O(log N)
    int size(int vertex) const {
        return -parent_or_size[leader(vertex)];
    }

    // 現在の連結成分数を返す O(1)
    int component_count() const {
        return components;
    }

    // 現在の履歴位置を返す O(1)
    int snapshot() const {
        return static_cast<int>(history.size());
    }

    // 直前の merge または add_value 呼び出し1回分を取り消す O(1)
    void undo() {
        assert(!history.empty());
        History record = history.back();
        history.pop_back();
        if (record.type == NO_OPERATION) return;

        if (record.type == MERGE_OPERATION) {
            parent_or_size[record.root_x] = record.parent_x;
            parent_or_size[record.root_y] = record.parent_y;
            component_value_sum[record.root_x] = record.sum_x;
            component_value_sum[record.root_y] = record.sum_y;
            components++;
            return;
        }

        component_value_sum[record.root_x] = record.sum_x;
        vertex_value[record.vertex] = record.vertex_value;
    }

    // 履歴を指定 snapshot まで巻き戻す O(取り消す操作数)
    void rollback(int target_snapshot) {
        assert(0 <= target_snapshot && target_snapshot <= snapshot());
        while (snapshot() > target_snapshot) undo();
    }
};

// ============================================================================
// RollbackValueDsuDynamicComponentSumSolver
// ============================================================================
// 【用途】辺追加・辺削除・頂点値加算・成分合計問い合わせをオフラインで処理する
// 【典型】動的グラフ上の component sum、Library Checker Dynamic Graph Vertex Add Component Sum 型
// 【使い方】初期値と Operation 列を渡す。ADD_VALUE は vertex=u と delta=value を使用する
// 【取得】QUERY_COMPONENT_SUM の回答を出現順に vector<long long> で返す
// 【注意】頂点加算は実行時刻以降ずっと有効。辺の REMOVE_EDGE は active な同一辺を1本削除する
// 【コピー範囲】RollbackValueDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)
struct RollbackValueDsuDynamicComponentSumSolver {
    static constexpr int ADD_EDGE = 0;
    static constexpr int REMOVE_EDGE = 1;
    static constexpr int ADD_VALUE = 2;
    static constexpr int QUERY_COMPONENT_SUM = 3;

    struct Operation {
        int type;
        int u;
        int v;
        long long value;
    };

private:
    struct EdgeEvent { int u; int v; };
    struct ValueEvent { int vertex; long long delta; };
    struct SegmentEvent {
        int type;
        int u;
        int v;
        long long value;
    };
    struct EdgeTimelineEvent { int u; int v; int time; int delta; };
    struct EdgeInterval { int left; int right; EdgeEvent edge; };

    static vector<EdgeInterval> build_edge_intervals(const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        vector<EdgeTimelineEvent> events;
        events.reserve(query_count);
        for (int time = 0; time < query_count; time++) {
            const Operation& operation = operations[time];
            if (operation.type != ADD_EDGE && operation.type != REMOVE_EDGE) continue;
            int u = operation.u;
            int v = operation.v;
            if (u > v) swap(u, v);
            events.push_back({u, v, time, operation.type == ADD_EDGE ? 1 : -1});
        }
        sort(events.begin(), events.end(), [](const EdgeTimelineEvent& lhs, const EdgeTimelineEvent& rhs) {
            return tie(lhs.u, lhs.v, lhs.time) < tie(rhs.u, rhs.v, rhs.time);
        });

        vector<EdgeInterval> intervals;
        for (int begin = 0; begin < static_cast<int>(events.size());) {
            int end = begin;
            while (end < static_cast<int>(events.size()) &&
                   events[end].u == events[begin].u && events[end].v == events[begin].v) end++;
            int active_count = 0;
            int start_time = -1;
            for (int index = begin; index < end; index++) {
                if (events[index].delta > 0) {
                    if (active_count == 0) start_time = events[index].time;
                    active_count++;
                } else {
                    assert(active_count > 0);
                    active_count--;
                    if (active_count == 0) {
                        intervals.push_back({start_time, events[index].time, {events[index].u, events[index].v}});
                    }
                }
            }
            if (active_count > 0) intervals.push_back({start_time, query_count, {events[begin].u, events[begin].v}});
            begin = end;
        }
        return intervals;
    }

    static void add_interval(vector<vector<SegmentEvent>>& tree, int node, int left, int right,
                             int ql, int qr, const SegmentEvent& event) {
        if (qr <= left || right <= ql) return;
        if (ql <= left && right <= qr) {
            tree[node].push_back(event);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(tree, node * 2, left, middle, ql, qr, event);
        add_interval(tree, node * 2 + 1, middle, right, ql, qr, event);
    }

    static void dfs(const vector<vector<SegmentEvent>>& tree, const vector<Operation>& operations,
                    int node, int left, int right, RollbackValueDsu& uf,
                    vector<long long>& answers) {
        int saved = uf.snapshot();
        for (const SegmentEvent& event : tree[node]) {
            if (event.type == ADD_EDGE) {
                uf.merge(event.u, event.v);
            } else {
                uf.add_value(event.u, event.value);
            }
        }

        if (right - left == 1) {
            if (operations[left].type == QUERY_COMPONENT_SUM) {
                answers.push_back(uf.component_sum(operations[left].u));
            }
        } else {
            int middle = left + (right - left) / 2;
            dfs(tree, operations, node * 2, left, middle, uf, answers);
            dfs(tree, operations, node * 2 + 1, middle, right, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、成分合計問い合わせへの回答を出現順に返す O((Q+K)log Q log N)
    static vector<long long> solve(
        const vector<long long>& initial_values,
        const vector<Operation>& operations
    ) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};
        vector<vector<SegmentEvent>> tree(operations.size() * 4);

        // 辺が active な各時間区間を segment tree へ登録する
        for (const EdgeInterval& interval : build_edge_intervals(operations)) {
            if (interval.left < interval.right) {
                add_interval(
                    tree,
                    1,
                    0,
                    query_count,
                    interval.left,
                    interval.right,
                    {ADD_EDGE, interval.edge.u, interval.edge.v, 0}
                );
            }
        }

        // 頂点加算はその時刻から操作列の末尾まで有効なイベントとして登録する
        for (int time = 0; time < query_count; time++) {
            const Operation& operation = operations[time];
            if (operation.type == ADD_VALUE) {
                add_interval(
                    tree,
                    1,
                    0,
                    query_count,
                    time,
                    query_count,
                    {ADD_VALUE, operation.u, 0, operation.value}
                );
            }
        }

        RollbackValueDsu uf(initial_values);
        vector<long long> answers;
        dfs(tree, operations, 1, 0, query_count, uf, answers);
        return answers;
    }
};

// ============================================================================
// RollbackParityDsu
// ============================================================================
// 【用途】value[x] XOR value[y]=parity という二値等式を追加し、矛盾を含めて rollback できる Union-Find
// 【典型】動的二部グラフ、敵味方制約の追加削除、XOR 等式、時間区間付き parity 制約
// 【使い方】merge(x,y,p) で XOR 等式を追加し、relation、component_consistent、globally_consistent を利用する
// 【取得】relation(x,y) は連結なら value[x] XOR value[y]、非連結なら nullopt を返す
// 【注意】追加専用の parity 制約は ACL dsu の2N展開でも可能。本構造は rollback・削除が必要な場合向け
// 【コピー範囲】この struct だけで rollback parity Union-Find として使用できる
// 【計算量】各検索・merge は O(log N)、undo 1回は O(1)
struct RollbackParityDsu {
    enum class MergeResult {
        merged,
        already_consistent,
        contradiction,
    };

private:
    struct History {
        bool merged;
        int root_x;
        int root_y;
        int parent_x;
        int parent_y;
        int parity_y;
        int bad_x;
        int bad_y;
        int total_bad;
    };

    vector<int> parent_or_size;
    vector<int> parity_to_parent;
    vector<int> contradiction_count;
    vector<History> history;
    int total_contradictions;

    pair<int, int> leader_and_parity(int vertex) const {
        int parity = 0;
        while (parent_or_size[vertex] >= 0) {
            parity ^= parity_to_parent[vertex];
            vertex = parent_or_size[vertex];
        }
        return {vertex, parity};
    }

public:
    // count 個の独立な二値変数で初期化する O(N)
    explicit RollbackParityDsu(int count)
        : parent_or_size(count, -1),
          parity_to_parent(count, 0),
          contradiction_count(count, 0),
          total_contradictions(0) {}

    // value[x] XOR value[y]=parity を追加し、追加結果を返す O(log N)
    MergeResult merge(int x, int y, int parity) {
        parity &= 1;
        auto [root_x, value_x] = leader_and_parity(x);
        auto [root_y, value_y] = leader_and_parity(y);

        // 同一成分への制約は、矛盾数だけを増減できるよう履歴へ残す
        if (root_x == root_y) {
            history.push_back({
                false,
                root_x,
                -1,
                0,
                0,
                0,
                contradiction_count[root_x],
                0,
                total_contradictions,
            });
            if ((value_x ^ value_y) != parity) {
                contradiction_count[root_x]++;
                total_contradictions++;
                return MergeResult::contradiction;
            }
            return MergeResult::already_consistent;
        }

        int root_parity = value_x ^ value_y ^ parity;
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) swap(root_x, root_y);
        history.push_back({
            true,
            root_x,
            root_y,
            parent_or_size[root_x],
            parent_or_size[root_y],
            parity_to_parent[root_y],
            contradiction_count[root_x],
            contradiction_count[root_y],
            total_contradictions,
        });

        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        parity_to_parent[root_y] = root_parity;
        contradiction_count[root_x] += contradiction_count[root_y];
        return MergeResult::merged;
    }

    // vertex を含む成分の代表元を返す O(log N)
    int leader(int vertex) const {
        return leader_and_parity(vertex).first;
    }

    // x と y が同じ成分かを返す O(log N)
    bool same(int x, int y) const {
        return leader(x) == leader(y);
    }

    // value[x] XOR value[y] を返し、非連結なら nullopt を返す O(log N)
    optional<int> relation(int x, int y) const {
        auto [root_x, value_x] = leader_and_parity(x);
        auto [root_y, value_y] = leader_and_parity(y);
        if (root_x != root_y) return nullopt;
        return value_x ^ value_y;
    }

    // vertex を含む成分の parity 制約が整合しているかを返す O(log N)
    bool component_consistent(int vertex) const {
        return contradiction_count[leader(vertex)] == 0;
    }

    // 全成分の parity 制約が整合しているかを返す O(1)
    bool globally_consistent() const {
        return total_contradictions == 0;
    }

    // vertex を含む成分の頂点数を返す O(log N)
    int size(int vertex) const {
        return -parent_or_size[leader(vertex)];
    }

    // 現在の履歴位置を返す O(1)
    int snapshot() const {
        return static_cast<int>(history.size());
    }

    // 直前の merge 呼び出し1回分を取り消す O(1)
    void undo() {
        assert(!history.empty());
        History record = history.back();
        history.pop_back();
        total_contradictions = record.total_bad;
        contradiction_count[record.root_x] = record.bad_x;
        if (!record.merged) return;

        parent_or_size[record.root_x] = record.parent_x;
        parent_or_size[record.root_y] = record.parent_y;
        parity_to_parent[record.root_y] = record.parity_y;
        contradiction_count[record.root_y] = record.bad_y;
    }

    // 履歴を指定 snapshot まで巻き戻す O(取り消す merge 呼び出し数)
    void rollback(int target_snapshot) {
        assert(0 <= target_snapshot && target_snapshot <= snapshot());
        while (snapshot() > target_snapshot) undo();
    }
};

// ============================================================================
// RollbackParityDsuDynamicBipartiteSolver
// ============================================================================
// 【用途】無向辺の追加・削除に対し、各時点でグラフ全体が二部グラフかをオフライン判定する
// 【典型】動的二部グラフ、辺が時間区間中だけ存在する2彩色可能性、奇閉路の出現と消滅
// 【使い方】ADD_EDGE、REMOVE_EDGE、QUERY_BIPARTITE を時系列順に並べて solve を呼ぶ
// 【取得】二部グラフ判定の回答を出現順に vector<char> で返す
// 【注意】各辺は color[u] XOR color[v]=1 として扱う。多重辺と自己ループを正しく扱う
// 【コピー範囲】RollbackParityDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)
struct RollbackParityDsuDynamicBipartiteSolver {
    static constexpr int ADD_EDGE = 0;
    static constexpr int REMOVE_EDGE = 1;
    static constexpr int QUERY_BIPARTITE = 2;

    struct Operation { int type; int u; int v; };

private:
    struct Edge { int u; int v; };
    struct Event { int u; int v; int time; int delta; };
    struct Interval { int left; int right; Edge edge; };

    static vector<Interval> build_intervals(const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        vector<Event> events;
        events.reserve(query_count);
        for (int time = 0; time < query_count; time++) {
            const Operation& operation = operations[time];
            if (operation.type == QUERY_BIPARTITE) continue;
            int u = operation.u;
            int v = operation.v;
            if (u > v) swap(u, v);
            events.push_back({u, v, time, operation.type == ADD_EDGE ? 1 : -1});
        }
        sort(events.begin(), events.end(), [](const Event& lhs, const Event& rhs) {
            return tie(lhs.u, lhs.v, lhs.time) < tie(rhs.u, rhs.v, rhs.time);
        });
        vector<Interval> intervals;
        for (int begin = 0; begin < static_cast<int>(events.size());) {
            int end = begin;
            while (end < static_cast<int>(events.size()) &&
                   events[end].u == events[begin].u && events[end].v == events[begin].v) end++;
            int active_count = 0;
            int start_time = -1;
            for (int index = begin; index < end; index++) {
                if (events[index].delta > 0) {
                    if (active_count == 0) start_time = events[index].time;
                    active_count++;
                } else {
                    assert(active_count > 0);
                    active_count--;
                    if (active_count == 0) {
                        intervals.push_back({start_time, events[index].time, {events[index].u, events[index].v}});
                    }
                }
            }
            if (active_count > 0) intervals.push_back({start_time, query_count, {events[begin].u, events[begin].v}});
            begin = end;
        }
        return intervals;
    }

    static void add_interval(vector<vector<Edge>>& tree, int node, int left, int right,
                             int ql, int qr, const Edge& edge) {
        if (qr <= left || right <= ql) return;
        if (ql <= left && right <= qr) {
            tree[node].push_back(edge);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(tree, node * 2, left, middle, ql, qr, edge);
        add_interval(tree, node * 2 + 1, middle, right, ql, qr, edge);
    }

    static void dfs(const vector<vector<Edge>>& tree, const vector<Operation>& operations,
                    int node, int left, int right, RollbackParityDsu& uf, vector<char>& answers) {
        int saved = uf.snapshot();
        for (const Edge& edge : tree[node]) uf.merge(edge.u, edge.v, 1);
        if (right - left == 1) {
            if (operations[left].type == QUERY_BIPARTITE) {
                answers.push_back(uf.globally_consistent());
            }
        } else {
            int middle = left + (right - left) / 2;
            dfs(tree, operations, node * 2, left, middle, uf, answers);
            dfs(tree, operations, node * 2 + 1, middle, right, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、二部グラフ判定への回答を出現順に返す O((Q+K)log Q log N)
    static vector<char> solve(int count, const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};
        vector<vector<Edge>> tree(operations.size() * 4);
        for (const Interval& interval : build_intervals(operations)) {
            if (interval.left < interval.right) {
                add_interval(tree, 1, 0, query_count, interval.left, interval.right, interval.edge);
            }
        }
        RollbackParityDsu uf(count);
        vector<char> answers;
        dfs(tree, operations, 1, 0, query_count, uf, answers);
        return answers;
    }
};

// ============================================================================
// RollbackParityDsuDynamicConstraintSolver
// ============================================================================
// 【用途】ID付き XOR 等式の追加・削除と、全体整合性・2変数間関係の問い合わせをオフライン処理する
// 【典型】動的な同じ/異なる制約、敵味方制約、削除可能な二値等式、動的 parity 方程式
// 【使い方】ADD_CONSTRAINT で一意な id を登録し、REMOVE_CONSTRAINT では同じ id を指定する
// 【取得】QUERY_RELATION は DISCONNECTED、INCONSISTENT、DETERMINED を区別し、QUERY_CONSISTENCY は consistent を返す
// 【注意】同じ id は active 中に再追加しない。REMOVE_CONSTRAINT の u,v,parity は使用しない
// 【コピー範囲】RollbackParityDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)
struct RollbackParityDsuDynamicConstraintSolver {
    static constexpr int ADD_CONSTRAINT = 0;
    static constexpr int REMOVE_CONSTRAINT = 1;
    static constexpr int QUERY_RELATION = 2;
    static constexpr int QUERY_CONSISTENCY = 3;
    static constexpr int DISCONNECTED = 0;
    static constexpr int INCONSISTENT = 1;
    static constexpr int DETERMINED = 2;

    struct Operation {
        int type;
        int id;
        int u;
        int v;
        int parity;
    };

    struct Answer {
        int operation_index;
        int query_type;
        int state;
        int relation;
        bool consistent;
    };

private:
    struct Constraint { int u; int v; int parity; };
    struct Interval { int left; int right; Constraint constraint; };

    static vector<Interval> build_intervals(const vector<Operation>& operations) {
        int max_id = -1;
        for (const Operation& operation : operations) {
            if (operation.type == ADD_CONSTRAINT || operation.type == REMOVE_CONSTRAINT) {
                max_id = max(max_id, operation.id);
            }
        }
        vector<int> start(max_id + 1, -1);
        vector<Constraint> constraints(max_id + 1, {0, 0, 0});
        vector<Interval> intervals;

        for (int time = 0; time < static_cast<int>(operations.size()); time++) {
            const Operation& operation = operations[time];
            if (operation.type == ADD_CONSTRAINT) {
                assert(operation.id >= 0 && start[operation.id] < 0);
                start[operation.id] = time;
                constraints[operation.id] = {operation.u, operation.v, operation.parity & 1};
            } else if (operation.type == REMOVE_CONSTRAINT) {
                assert(operation.id >= 0 && start[operation.id] >= 0);
                intervals.push_back({start[operation.id], time, constraints[operation.id]});
                start[operation.id] = -1;
            }
        }

        int query_count = static_cast<int>(operations.size());
        for (int id = 0; id <= max_id; id++) {
            if (start[id] >= 0) intervals.push_back({start[id], query_count, constraints[id]});
        }
        return intervals;
    }

    static void add_interval(vector<vector<Constraint>>& tree, int node, int left, int right,
                             int ql, int qr, const Constraint& constraint) {
        if (qr <= left || right <= ql) return;
        if (ql <= left && right <= qr) {
            tree[node].push_back(constraint);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(tree, node * 2, left, middle, ql, qr, constraint);
        add_interval(tree, node * 2 + 1, middle, right, ql, qr, constraint);
    }

    static void dfs(const vector<vector<Constraint>>& tree, const vector<Operation>& operations,
                    int node, int left, int right, RollbackParityDsu& uf, vector<Answer>& answers) {
        int saved = uf.snapshot();
        for (const Constraint& constraint : tree[node]) {
            uf.merge(constraint.u, constraint.v, constraint.parity);
        }

        if (right - left == 1) {
            const Operation& operation = operations[left];
            if (operation.type == QUERY_CONSISTENCY) {
                answers.push_back({left, QUERY_CONSISTENCY, DETERMINED, 0, uf.globally_consistent()});
            } else if (operation.type == QUERY_RELATION) {
                optional<int> relation = uf.relation(operation.u, operation.v);
                if (!relation.has_value()) {
                    answers.push_back({left, QUERY_RELATION, DISCONNECTED, 0, uf.globally_consistent()});
                } else if (!uf.component_consistent(operation.u)) {
                    answers.push_back({left, QUERY_RELATION, INCONSISTENT, 0, uf.globally_consistent()});
                } else {
                    answers.push_back({left, QUERY_RELATION, DETERMINED, *relation, uf.globally_consistent()});
                }
            }
        } else {
            int middle = left + (right - left) / 2;
            dfs(tree, operations, node * 2, left, middle, uf, answers);
            dfs(tree, operations, node * 2 + 1, middle, right, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、整合性・関係問い合わせへの回答を出現順に返す O((Q+K)log Q log N)
    static vector<Answer> solve(int count, const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};
        vector<vector<Constraint>> tree(operations.size() * 4);
        for (const Interval& interval : build_intervals(operations)) {
            if (interval.left < interval.right) {
                add_interval(tree, 1, 0, query_count, interval.left, interval.right, interval.constraint);
            }
        }
        RollbackParityDsu uf(count);
        vector<Answer> answers;
        dfs(tree, operations, 1, 0, query_count, uf, answers);
        return answers;
    }
};

// ============================================================================
// RollbackPotentialDsu
// ============================================================================
// 【用途】potential[y]-potential[x]=difference という差分等式と矛盾情報を rollback できる重み付き Union-Find
// 【典型】追加・削除可能な距離差、時間区間付き座標制約、動的差分方程式、仮定を戻す等式制約探索
// 【使い方】merge で等式を追加し、difference と consistent を使い、snapshot/rollback で状態を戻す
// 【取得】difference(x,y) は非連結なら nullopt。成分内に矛盾があれば値は意味を持たないため component_consistent も確認する
// 【注意】パス圧縮は行わない。T は加減算・等値比較・T{} が可能であること
// 【コピー範囲】この struct だけで rollback 重み付き Union-Find として使用できる
// 【計算量】各検索・merge は O(log N)、undo 1回は O(1)
template<class T>
struct RollbackPotentialDsu {
    enum class MergeResult {
        merged,
        already_consistent,
        contradiction,
    };

private:
    struct History {
        bool merged;
        int root_x;
        int root_y;
        int parent_x;
        int parent_y;
        T difference_y;
        int bad_x;
        int bad_y;
        int total_bad;
    };

    vector<int> parent_or_size;
    vector<T> difference_to_parent;
    vector<int> contradiction_count;
    vector<History> history;
    int total_contradictions;

    pair<int, T> leader_and_difference(int vertex) const {
        T difference = T{};
        while (parent_or_size[vertex] >= 0) {
            difference = difference + difference_to_parent[vertex];
            vertex = parent_or_size[vertex];
        }
        return {vertex, difference};
    }

public:
    // count 個の独立な頂点で初期化する O(N)
    explicit RollbackPotentialDsu(int count)
        : parent_or_size(count, -1),
          difference_to_parent(count, T{}),
          contradiction_count(count, 0),
          total_contradictions(0) {}

    // potential[y]-potential[x]=difference を追加し、追加結果を返す O(log N)
    MergeResult merge(int x, int y, const T& difference) {
        auto [root_x, weight_x] = leader_and_difference(x);
        auto [root_y, weight_y] = leader_and_difference(y);

        // 同一成分への等式は既存の差と比較し、矛盾数を rollback 可能な形で記録する
        if (root_x == root_y) {
            history.push_back({
                false,
                root_x,
                -1,
                0,
                0,
                T{},
                contradiction_count[root_x],
                0,
                total_contradictions,
            });
            if (!(weight_y - weight_x == difference)) {
                contradiction_count[root_x]++;
                total_contradictions++;
                return MergeResult::contradiction;
            }
            return MergeResult::already_consistent;
        }

        // root_y を root_x の子にした場合の potential[root_y]-potential[root_x] を求める
        T root_difference = difference + weight_x - weight_y;
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) {
            swap(root_x, root_y);
            root_difference = T{} - root_difference;
        }
        history.push_back({
            true,
            root_x,
            root_y,
            parent_or_size[root_x],
            parent_or_size[root_y],
            difference_to_parent[root_y],
            contradiction_count[root_x],
            contradiction_count[root_y],
            total_contradictions,
        });

        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        difference_to_parent[root_y] = root_difference;
        contradiction_count[root_x] += contradiction_count[root_y];
        return MergeResult::merged;
    }

    // vertex を含む成分の代表元を返す O(log N)
    int leader(int vertex) const {
        return leader_and_difference(vertex).first;
    }

    // x と y が同じ成分かを返す O(log N)
    bool same(int x, int y) const {
        return leader(x) == leader(y);
    }

    // potential[y]-potential[x] を返し、非連結なら nullopt を返す O(log N)
    optional<T> difference(int x, int y) const {
        auto [root_x, weight_x] = leader_and_difference(x);
        auto [root_y, weight_y] = leader_and_difference(y);
        if (root_x != root_y) return nullopt;
        return weight_y - weight_x;
    }

    // vertex を含む成分の差分等式が整合しているかを返す O(log N)
    bool component_consistent(int vertex) const {
        return contradiction_count[leader(vertex)] == 0;
    }

    // 全成分の差分等式が整合しているかを返す O(1)
    bool globally_consistent() const {
        return total_contradictions == 0;
    }

    // vertex を含む成分の頂点数を返す O(log N)
    int size(int vertex) const {
        return -parent_or_size[leader(vertex)];
    }

    // 現在の履歴位置を返す O(1)
    int snapshot() const {
        return static_cast<int>(history.size());
    }

    // 直前の merge 呼び出し1回分を取り消す O(1)
    void undo() {
        assert(!history.empty());
        History record = history.back();
        history.pop_back();
        total_contradictions = record.total_bad;
        contradiction_count[record.root_x] = record.bad_x;
        if (!record.merged) return;

        parent_or_size[record.root_x] = record.parent_x;
        parent_or_size[record.root_y] = record.parent_y;
        difference_to_parent[record.root_y] = record.difference_y;
        contradiction_count[record.root_y] = record.bad_y;
    }

    // 履歴を指定 snapshot まで巻き戻す O(取り消す merge 呼び出し数)
    void rollback(int target_snapshot) {
        assert(0 <= target_snapshot && target_snapshot <= snapshot());
        while (snapshot() > target_snapshot) undo();
    }
};

// ============================================================================
// RollbackPotentialDsuDynamicEqualitySolver
// ============================================================================
// 【用途】ID付き差分等式の追加・削除と、全体整合性・2変数間差分の問い合わせをオフライン処理する
// 【典型】動的な距離差制約、削除可能な座標等式、時間区間付き重み付き Union-Find、動的等式制約系
// 【使い方】ADD_EQUATION で一意な id を登録し、REMOVE_EQUATION では同じ id を指定する
// 【取得】QUERY_DIFFERENCE は DISCONNECTED、INCONSISTENT、DETERMINED を区別し、QUERY_CONSISTENCY は consistent を返す
// 【注意】同じ id は active 中に再追加しない。REMOVE_EQUATION の x,y,difference は使用しない
// 【コピー範囲】RollbackPotentialDsu とこの struct をコピーして使用する
// 【計算量】O((Q+K)log Q log N)
template<class T>
struct RollbackPotentialDsuDynamicEqualitySolver {
    static constexpr int ADD_EQUATION = 0;
    static constexpr int REMOVE_EQUATION = 1;
    static constexpr int QUERY_DIFFERENCE = 2;
    static constexpr int QUERY_CONSISTENCY = 3;
    static constexpr int DISCONNECTED = 0;
    static constexpr int INCONSISTENT = 1;
    static constexpr int DETERMINED = 2;

    struct Operation {
        int type;
        int id;
        int x;
        int y;
        T difference;
    };

    struct Answer {
        int operation_index;
        int query_type;
        int state;
        T difference;
        bool consistent;
    };

private:
    struct Equation { int x; int y; T difference; };
    struct Interval { int left; int right; Equation equation; };

    static vector<Interval> build_intervals(const vector<Operation>& operations) {
        int max_id = -1;
        for (const Operation& operation : operations) {
            if (operation.type == ADD_EQUATION || operation.type == REMOVE_EQUATION) {
                max_id = max(max_id, operation.id);
            }
        }
        vector<int> start(max_id + 1, -1);
        vector<Equation> equations(max_id + 1, {0, 0, T{}});
        vector<Interval> intervals;

        for (int time = 0; time < static_cast<int>(operations.size()); time++) {
            const Operation& operation = operations[time];
            if (operation.type == ADD_EQUATION) {
                assert(operation.id >= 0 && start[operation.id] < 0);
                start[operation.id] = time;
                equations[operation.id] = {operation.x, operation.y, operation.difference};
            } else if (operation.type == REMOVE_EQUATION) {
                assert(operation.id >= 0 && start[operation.id] >= 0);
                intervals.push_back({start[operation.id], time, equations[operation.id]});
                start[operation.id] = -1;
            }
        }

        int query_count = static_cast<int>(operations.size());
        for (int id = 0; id <= max_id; id++) {
            if (start[id] >= 0) intervals.push_back({start[id], query_count, equations[id]});
        }
        return intervals;
    }

    static void add_interval(vector<vector<Equation>>& tree, int node, int left, int right,
                             int ql, int qr, const Equation& equation) {
        if (qr <= left || right <= ql) return;
        if (ql <= left && right <= qr) {
            tree[node].push_back(equation);
            return;
        }
        int middle = left + (right - left) / 2;
        add_interval(tree, node * 2, left, middle, ql, qr, equation);
        add_interval(tree, node * 2 + 1, middle, right, ql, qr, equation);
    }

    static void dfs(const vector<vector<Equation>>& tree, const vector<Operation>& operations,
                    int node, int left, int right, RollbackPotentialDsu<T>& uf,
                    vector<Answer>& answers) {
        int saved = uf.snapshot();
        for (const Equation& equation : tree[node]) {
            uf.merge(equation.x, equation.y, equation.difference);
        }

        if (right - left == 1) {
            const Operation& operation = operations[left];
            if (operation.type == QUERY_CONSISTENCY) {
                answers.push_back({left, QUERY_CONSISTENCY, DETERMINED, T{}, uf.globally_consistent()});
            } else if (operation.type == QUERY_DIFFERENCE) {
                optional<T> value = uf.difference(operation.x, operation.y);
                if (!value.has_value()) {
                    answers.push_back({left, QUERY_DIFFERENCE, DISCONNECTED, T{}, uf.globally_consistent()});
                } else if (!uf.component_consistent(operation.x)) {
                    answers.push_back({left, QUERY_DIFFERENCE, INCONSISTENT, T{}, uf.globally_consistent()});
                } else {
                    answers.push_back({left, QUERY_DIFFERENCE, DETERMINED, *value, uf.globally_consistent()});
                }
            }
        } else {
            int middle = left + (right - left) / 2;
            dfs(tree, operations, node * 2, left, middle, uf, answers);
            dfs(tree, operations, node * 2 + 1, middle, right, uf, answers);
        }
        uf.rollback(saved);
    }

public:
    // 操作列をオフライン処理し、整合性・差分問い合わせへの回答を出現順に返す O((Q+K)log Q log N)
    static vector<Answer> solve(int count, const vector<Operation>& operations) {
        int query_count = static_cast<int>(operations.size());
        if (query_count == 0) return {};
        vector<vector<Equation>> tree(operations.size() * 4);
        for (const Interval& interval : build_intervals(operations)) {
            if (interval.left < interval.right) {
                add_interval(tree, 1, 0, query_count, interval.left, interval.right, interval.equation);
            }
        }
        RollbackPotentialDsu<T> uf(count);
        vector<Answer> answers;
        dfs(tree, operations, 1, 0, query_count, uf, answers);
        return answers;
    }
};

// ============================================================================
// PartiallyPersistentDsu
// ============================================================================
// 【用途】merge は時刻順に追加しつつ、過去の任意時刻における連結性と成分サイズを問い合わせる部分永続 Union-Find
// 【典型】過去時点のネットワーク、初めて連結した時刻、成分サイズが増える履歴、追加専用グラフの履歴クエリ
// 【使い方】非減少な非負 time で merge(time,x,y) を呼び、same(time,x,y) や size(time,x) を利用する
// 【取得】first_same_time(x,y) は初めて連結した時刻を返し、最後まで非連結なら nullopt を返す
// 【注意】履歴は一本道で分岐しない。任意の過去バージョンから merge する場合は PersistentDsu を使う
// 【コピー範囲】この struct だけで部分永続 Union-Find として使用できる
// 【計算量】merge・same は O(log N)、size は O(log N+log Q)、first_same_time は O(log Q log N)
struct PartiallyPersistentDsu {
private:
    static constexpr int NEVER = numeric_limits<int>::max();

    vector<int> parent;
    vector<int> parent_time;
    vector<vector<pair<int, int>>> size_history;
    int latest_time;

public:
    // count 個の独立な頂点を時刻 -1 の状態として初期化する O(N)
    explicit PartiallyPersistentDsu(int count)
        : parent(count), parent_time(count, NEVER), size_history(count), latest_time(-1) {
        iota(parent.begin(), parent.end(), 0);
        for (int vertex = 0; vertex < count; vertex++) size_history[vertex].push_back({-1, 1});
    }

    // 指定時刻における vertex の代表元を返す O(log N)
    int leader(int time, int vertex) const {
        while (parent_time[vertex] <= time) vertex = parent[vertex];
        return vertex;
    }

    // 非減少な時刻 time で x と y を併合し、実際に併合したかを返す O(log N)
    bool merge(int time, int x, int y) {
        assert(time >= 0 && time >= latest_time);
        latest_time = time;
        int root_x = leader(time, x);
        int root_y = leader(time, y);
        if (root_x == root_y) return false;

        int size_x = size_history[root_x].back().second;
        int size_y = size_history[root_y].back().second;
        if (size_x < size_y) {
            swap(root_x, root_y);
            swap(size_x, size_y);
        }

        parent[root_y] = root_x;
        parent_time[root_y] = time;
        size_history[root_x].push_back({time, size_x + size_y});
        return true;
    }

    // 指定時刻に x と y が同じ成分かを返す O(log N)
    bool same(int time, int x, int y) const {
        return leader(time, x) == leader(time, y);
    }

    // 指定時刻に vertex を含む成分の頂点数を返す O(log N+log Q)
    int size(int time, int vertex) const {
        int root = leader(time, vertex);
        const vector<pair<int, int>>& history = size_history[root];
        auto iterator = upper_bound(
            history.begin(),
            history.end(),
            pair<int, int>{time, numeric_limits<int>::max()}
        );
        assert(iterator != history.begin());
        return prev(iterator)->second;
    }

    // x と y が初めて連結した時刻を返し、最後まで非連結なら nullopt を返す O(log Q log N)
    optional<int> first_same_time(int x, int y) const {
        if (x == y) return -1;
        if (latest_time < 0 || !same(latest_time, x, y)) return nullopt;

        int low = 0;
        int high = latest_time;
        while (low < high) {
            int middle = low + (high - low) / 2;
            if (same(middle, x, y)) {
                high = middle;
            } else {
                low = middle + 1;
            }
        }
        return low;
    }

    // 最後に merge を処理した時刻を返す O(1)
    int last_time() const {
        return latest_time;
    }
};

// ============================================================================
// PartiallyPersistentDsuHistoricalQuerySolver
// ============================================================================
// 【用途】時刻付き辺追加列から、過去時刻における連結性と成分サイズの問い合わせを処理する
// 【典型】時刻 t までに建設された道路、履歴付き友人関係、過去のクラスタサイズ
// 【使い方】Edge の time は非負でよく、入力順でなくても solve 内で時刻順に並べ替える
// 【取得】Query::type に対応した Answer を問い合わせ配列と同じ順番で返す
// 【注意】同じ時刻の全辺をその時刻に有効として扱う。QUERY_SIZE の y は使用しない
// 【コピー範囲】PartiallyPersistentDsu とこの struct をコピーして使用する
// 【計算量】O(M log M+(M+Q)log N+Q log M)
struct PartiallyPersistentDsuHistoricalQuerySolver {
    static constexpr int QUERY_SAME = 0;
    static constexpr int QUERY_SIZE = 1;

    struct Edge {
        int time;
        int u;
        int v;
    };

    struct Query {
        int type;
        int time;
        int x;
        int y;
    };

    struct Answer {
        int type;
        bool same;
        int size;
    };

    // 時刻付き辺を構築し、履歴問い合わせへの回答を入力順に返す O(M log M+(M+Q)log N+Q log M)
    static vector<Answer> solve(int count, vector<Edge> edges, const vector<Query>& queries) {
        sort(edges.begin(), edges.end(), [](const Edge& lhs, const Edge& rhs) {
            return tie(lhs.time, lhs.u, lhs.v) < tie(rhs.time, rhs.u, rhs.v);
        });
        PartiallyPersistentDsu uf(count);
        for (const Edge& edge : edges) uf.merge(edge.time, edge.u, edge.v);

        vector<Answer> answers;
        answers.reserve(queries.size());
        for (const Query& query : queries) {
            if (query.type == QUERY_SAME) {
                answers.push_back({QUERY_SAME, uf.same(query.time, query.x, query.y), 0});
            } else {
                answers.push_back({QUERY_SIZE, false, uf.size(query.time, query.x)});
            }
        }
        return answers;
    }
};

// ============================================================================
// PartiallyPersistentDsuEarliestConnectionSolver
// ============================================================================
// 【用途】時刻付き辺追加列に対し、複数の頂点ペアが初めて連結した時刻を求める
// 【典型】最初に通信可能になった時刻、初めて同じグループになった時刻、追加専用グラフの earliest query
// 【使い方】Edge 配列と PairQuery 配列を渡して solve を呼ぶ
// 【取得】各ペアについて初回連結時刻を optional<int> で返し、最後まで非連結なら nullopt
// 【注意】x==y は初期状態から連結しているため値 -1 を返す
// 【コピー範囲】PartiallyPersistentDsu とこの struct をコピーして使用する
// 【計算量】O(M log M+M log N+Q log T log N)
struct PartiallyPersistentDsuEarliestConnectionSolver {
    struct Edge {
        int time;
        int u;
        int v;
    };

    struct PairQuery {
        int x;
        int y;
    };

    // 各頂点ペアの初回連結時刻を入力順に返す O(M log M+M log N+Q log T log N)
    static vector<optional<int>> solve(
        int count,
        vector<Edge> edges,
        const vector<PairQuery>& queries
    ) {
        sort(edges.begin(), edges.end(), [](const Edge& lhs, const Edge& rhs) {
            return tie(lhs.time, lhs.u, lhs.v) < tie(rhs.time, rhs.u, rhs.v);
        });
        PartiallyPersistentDsu uf(count);
        for (const Edge& edge : edges) uf.merge(edge.time, edge.u, edge.v);

        vector<optional<int>> answers;
        answers.reserve(queries.size());
        for (const PairQuery& query : queries) answers.push_back(uf.first_same_time(query.x, query.y));
        return answers;
    }
};

// ============================================================================
// PersistentDsu
// ============================================================================
// 【用途】任意の過去バージョンから新しい union バージョンを分岐できる完全永続 Union-Find
// 【典型】分岐する操作履歴、過去版からの union、各バージョンの連結性・成分サイズ問い合わせ
// 【使い方】Version base を指定して uf.merge(base,x,y) を呼び、返された Version を保存する
// 【取得】same(version,x,y) と size(version,x) は指定バージョンを変更せず参照する
// 【注意】永続 segment tree で parent_or_size を保存するため ACL dsu より重い。パス圧縮は行わない
// 【コピー範囲】この struct だけで完全永続 Union-Find として使用できる
// 【計算量】leader・same・size は O(log²N)、merge は O(log²N)
struct PersistentDsu {
    using Version = int;

private:
    struct Node {
        int left;
        int right;
        int value;
    };

    int count;
    vector<Node> nodes;
    Version initial_root;

    int build(int left, int right) {
        int node = static_cast<int>(nodes.size());
        nodes.push_back({-1, -1, -1});
        if (right - left == 1) return node;
        int middle = left + (right - left) / 2;
        int left_child = build(left, middle);
        int right_child = build(middle, right);
        nodes[node].left = left_child;
        nodes[node].right = right_child;
        return node;
    }

    int get_value(int node, int left, int right, int position) const {
        if (right - left == 1) return nodes[node].value;
        int middle = left + (right - left) / 2;
        if (position < middle) return get_value(nodes[node].left, left, middle, position);
        return get_value(nodes[node].right, middle, right, position);
    }

    int set_value(int node, int left, int right, int position, int value) {
        int new_node = static_cast<int>(nodes.size());
        nodes.push_back(nodes[node]);
        if (right - left == 1) {
            nodes[new_node].value = value;
            return new_node;
        }
        int middle = left + (right - left) / 2;
        if (position < middle) {
            nodes[new_node].left = set_value(nodes[node].left, left, middle, position, value);
        } else {
            nodes[new_node].right = set_value(nodes[node].right, middle, right, position, value);
        }
        return new_node;
    }

public:
    // count 個の独立な頂点を持つ初期バージョンを構築する O(N)
    explicit PersistentDsu(int vertex_count) : count(vertex_count), initial_root(-1) {
        assert(count > 0);
        nodes.reserve(count * 2);
        initial_root = build(0, count);
    }

    // 初期バージョンを返す O(1)
    Version initial_version() const {
        return initial_root;
    }

    // 指定バージョンで vertex を含む成分の代表元を返す O(log²N)
    int leader(Version version, int vertex) const {
        int parent_or_size = get_value(version, 0, count, vertex);
        while (parent_or_size >= 0) {
            vertex = parent_or_size;
            parent_or_size = get_value(version, 0, count, vertex);
        }
        return vertex;
    }

    // base バージョンの x と y を併合した新バージョンを返す O(log²N)
    Version merge(Version base, int x, int y) {
        int root_x = leader(base, x);
        int root_y = leader(base, y);
        if (root_x == root_y) return base;

        int value_x = get_value(base, 0, count, root_x);
        int value_y = get_value(base, 0, count, root_y);
        if (-value_x < -value_y) {
            swap(root_x, root_y);
            swap(value_x, value_y);
        }
        Version version = set_value(base, 0, count, root_x, value_x + value_y);
        version = set_value(version, 0, count, root_y, root_x);
        return version;
    }

    // 指定バージョンで x と y が同じ成分かを返す O(log²N)
    bool same(Version version, int x, int y) const {
        return leader(version, x) == leader(version, y);
    }

    // 指定バージョンで vertex を含む成分の頂点数を返す O(log²N)
    int size(Version version, int vertex) const {
        int root = leader(version, vertex);
        return -get_value(version, 0, count, root);
    }

    // 永続 segment tree の node pool 容量を事前確保する O(追加確保量)
    void reserve_nodes(size_t capacity) {
        nodes.reserve(capacity);
    }

    // 現在確保済みの永続 node 数を返す O(1)
    int node_count() const {
        return static_cast<int>(nodes.size());
    }
};

// ============================================================================
// PersistentDsuVersionedConnectivitySolver
// ============================================================================
// 【用途】各操作が任意の過去操作バージョンを参照する union・連結判定を完全永続 DSU でオンライン順に処理する
// 【典型】Persistent Union Find、過去版から分岐する連結性、操作列をDFS順へ並べ替えられないバージョンクエリ
// 【使い方】Operation::base に -1 または自分より前の操作番号を指定し、MERGE または QUERY_SAME を設定する
// 【取得】QUERY_SAME の回答を操作出現順に vector<char> で返す
// 【注意】問い合わせ操作も base と同一内容の新バージョンになる。全操作が既知なら rollback version tree の方が軽い
// 【コピー範囲】PersistentDsu とこの struct をコピーして使用する
// 【計算量】O(Q log²N)
struct PersistentDsuVersionedConnectivitySolver {
    static constexpr int MERGE = 0;
    static constexpr int QUERY_SAME = 1;

    struct Operation {
        int type;
        int base;
        int u;
        int v;
    };

    // 分岐バージョン操作を処理し、連結問い合わせへの回答を出現順に返す O(Q log²N)
    static vector<char> solve(int count, const vector<Operation>& operations) {
        PersistentDsu uf(count);
        size_t reserve_size = operations.size() * 40 + count + count;
        uf.reserve_nodes(reserve_size);
        vector<PersistentDsu::Version> versions(operations.size());
        vector<char> answers;

        for (int index = 0; index < static_cast<int>(operations.size()); index++) {
            const Operation& operation = operations[index];
            assert(operation.base < index);
            PersistentDsu::Version base = operation.base < 0
                ? uf.initial_version()
                : versions[operation.base];
            if (operation.type == MERGE) {
                versions[index] = uf.merge(base, operation.u, operation.v);
            } else {
                answers.push_back(uf.same(base, operation.u, operation.v));
                versions[index] = base;
            }
        }
        return answers;
    }
};

// ============================================================================
// SuccessorDsu
// ============================================================================
// 【用途】削除済み位置を飛ばし、x 以上で未削除の最小位置を高速に求める一方向 Union-Find
// 【典型】次の未処理位置、区間塗り、区間代入、座席・駐車場割当て、使用済み番号のスキップ
// 【使い方】next(x) で次の未削除位置を取得し、処理後に erase(x) で削除済みにする
// 【取得】未削除位置がなければ番兵 count を返す
// 【注意】代表元を右方向へ強制するため ACL dsu の union by size では代用しにくい
// 【コピー範囲】この struct だけで successor Union-Find として使用できる
// 【計算量】各操作ならし O(α(N))
struct SuccessorDsu {
private:
    vector<int> parent;
    int count;

public:
    // 位置 0..count-1 と番兵 count を未削除状態で初期化する O(N)
    explicit SuccessorDsu(int position_count) : parent(position_count + 1), count(position_count) {
        iota(parent.begin(), parent.end(), 0);
    }

    // x 以上の未削除位置を返し、存在しなければ count を返す ならし O(α(N))
    int next(int x) {
        assert(0 <= x && x <= count);
        if (parent[x] == x) return x;
        parent[x] = next(parent[x]);
        return parent[x];
    }

    // x を削除済みにし、新しく削除した場合だけ true を返す ならし O(α(N))
    bool erase(int x) {
        assert(0 <= x && x < count);
        if (next(x) != x) return false;
        parent[x] = next(x + 1);
        return true;
    }

    // [left,right) の全位置を削除済みにする ならし O((新規削除数+1)α(N))
    void erase_range(int left, int right) {
        assert(0 <= left && left <= right && right <= count);
        int position = next(left);
        while (position < right) {
            erase(position);
            position = next(position);
        }
    }

    // x が未削除なら true を返す ならし O(α(N))
    bool available(int x) {
        assert(0 <= x && x < count);
        return next(x) == x;
    }

    // 管理している実位置数を返す O(1)
    int size() const {
        return count;
    }
};

// ============================================================================
// SuccessorDsuRangeAssignmentSolver
// ============================================================================
// 【用途】区間代入を逆順処理し、各位置へ最後に適用される値だけを書き込む
// 【典型】区間上書き、ポスター、最終色、各位置が最後に受ける更新、未処理位置スキップ
// 【使い方】Query に [left,right) と value を入れ、初期値 default_value とともに solve を呼ぶ
// 【取得】全クエリ適用後の長さ count の配列を返す
// 【注意】各位置を高々1回だけ書く。加算や最小化など上書き以外の演算にはそのまま使えない
// 【コピー範囲】SuccessorDsu とこの struct をコピーして使用する
// 【計算量】O((N+Q)α(N))
template<class T>
struct SuccessorDsuRangeAssignmentSolver {
    struct Query {
        int left;
        int right;
        T value;
    };

    // 区間代入列を適用した最終配列を返す O((N+Q)α(N))
    static vector<T> solve(int count, const vector<Query>& queries, const T& default_value = T{}) {
        vector<T> answer(count, default_value);
        SuccessorDsu successor(count);

        // 最後の代入から逆に見て、まだ値が確定していない位置だけを書き込む
        for (int query_index = static_cast<int>(queries.size()) - 1; query_index >= 0; query_index--) {
            const Query& query = queries[query_index];
            int position = successor.next(query.left);
            while (position < query.right) {
                answer[position] = query.value;
                successor.erase(position);
                position = successor.next(position);
            }
        }
        return answer;
    }
};

// ============================================================================
// SuccessorDsuNextAvailableSolver
// ============================================================================
// 【用途】各要求位置 x に対し、x 以上の最小未使用位置を割り当てて使用済みにする
// 【典型】座席割当て、駐車場所、未使用番号、締切以降の最小空きスロット
// 【使い方】要求位置列 requests を solve へ渡す
// 【取得】各要求への割当位置を返し、空きがなければ count を返す
// 【注意】循環割当ては行わない。割当てられた位置は直後に erase される
// 【コピー範囲】SuccessorDsu とこの struct をコピーして使用する
// 【計算量】O((N+Q)α(N))
struct SuccessorDsuNextAvailableSolver {
    // 各要求へ次の未使用位置を割り当てた結果を返す O((N+Q)α(N))
    static vector<int> solve(int count, const vector<int>& requests) {
        SuccessorDsu successor(count);
        vector<int> answer;
        answer.reserve(requests.size());
        for (int request : requests) {
            int position = successor.next(request);
            answer.push_back(position);
            if (position < count) successor.erase(position);
        }
        return answer;
    }
};

// ============================================================================
// SuccessorDsuCyclicAllocationSolver
// ============================================================================
// 【用途】要求位置 x 以上の空きを探し、なければ先頭へ回って最小空き位置を割り当てる
// 【典型】円形駐車場、循環ハッシュ、wrap-around する座席割当て
// 【使い方】0<=request<count の要求列を solve へ渡す
// 【取得】割当位置を返し、全位置使用済みなら -1 を返す
// 【注意】割当てた位置は再利用しない。削除解除は扱わない
// 【コピー範囲】SuccessorDsu とこの struct をコピーして使用する
// 【計算量】O((N+Q)α(N))
struct SuccessorDsuCyclicAllocationSolver {
    // 各要求へ循環的に空き位置を割り当てた結果を返す O((N+Q)α(N))
    static vector<int> solve(int count, const vector<int>& requests) {
        SuccessorDsu successor(count);
        vector<int> answer;
        answer.reserve(requests.size());
        for (int request : requests) {
            int position = successor.next(request);
            if (position == count) position = successor.next(0);
            if (position == count) {
                answer.push_back(-1);
            } else {
                answer.push_back(position);
                successor.erase(position);
            }
        }
        return answer;
    }
};

// ============================================================================
// IntervalUnionDsu
// ============================================================================
// 【用途】通常の頂点併合に加え、区間 [left,right) 内の全頂点を効率よく同じ成分へまとめる
// 【典型】区間連結クエリ、連続位置を一括 union、同じ隣接境界を何度も処理する問題
// 【使い方】merge_interval(left,right) で区間内の隣接頂点をすべて結び、same と size で問い合わせる
// 【取得】component_count は現在の連結成分数を返す
// 【注意】一度処理した隣接境界を successor で飛ばす。各境界は全操作を通じ高々1回だけ処理される
// 【コピー範囲】この struct だけで区間 union 構造として使用できる
// 【計算量】通常 merge はならし O(α(N))、全 merge_interval 合計 O((N+Q)α(N))
struct IntervalUnionDsu {
private:
    vector<int> parent_or_size;
    vector<int> next_boundary;
    int components;

    int find_root(int vertex) {
        if (parent_or_size[vertex] < 0) return vertex;
        parent_or_size[vertex] = find_root(parent_or_size[vertex]);
        return parent_or_size[vertex];
    }

    int find_boundary(int boundary) {
        if (next_boundary[boundary] == boundary) return boundary;
        next_boundary[boundary] = find_boundary(next_boundary[boundary]);
        return next_boundary[boundary];
    }

public:
    // count 個の独立な頂点で初期化する O(N)
    explicit IntervalUnionDsu(int count)
        : parent_or_size(count, -1),
          next_boundary(count + 1),
          components(count) {
        iota(next_boundary.begin(), next_boundary.end(), 0);
    }

    // x と y の成分を併合し、実際に併合したかを返す ならし O(α(N))
    bool merge(int x, int y) {
        int root_x = find_root(x);
        int root_y = find_root(y);
        if (root_x == root_y) return false;
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) swap(root_x, root_y);
        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        components--;
        return true;
    }

    // [left,right) 内の全頂点を同じ成分へまとめる ならし O((新規処理境界数+1)α(N))
    void merge_interval(int left, int right) {
        assert(0 <= left && left <= right && right <= static_cast<int>(parent_or_size.size()));
        if (right - left <= 1) return;

        // 境界 i は頂点 i と i+1 の間を表し、一度処理した境界は以後 successor で飛ばす
        int boundary = find_boundary(left);
        while (boundary < right - 1) {
            merge(boundary, boundary + 1);
            next_boundary[boundary] = find_boundary(boundary + 1);
            boundary = find_boundary(boundary);
        }
    }

    // x と y が同じ成分かを返す ならし O(α(N))
    bool same(int x, int y) {
        return find_root(x) == find_root(y);
    }

    // vertex を含む成分の代表元を返す ならし O(α(N))
    int leader(int vertex) {
        return find_root(vertex);
    }

    // vertex を含む成分の頂点数を返す ならし O(α(N))
    int size(int vertex) {
        return -parent_or_size[find_root(vertex)];
    }

    // 現在の連結成分数を返す O(1)
    int component_count() const {
        return components;
    }
};

// ============================================================================
// IntervalUnionDsuQuerySolver
// ============================================================================
// 【用途】単点 union、区間 union、連結判定、成分サイズの混在クエリを処理する
// 【典型】区間を一括接続するオンライン問題、連続セグメント統合、区間等価関係
// 【使い方】MERGE、MERGE_INTERVAL、QUERY_SAME、QUERY_SIZE の Operation を時系列順に渡す
// 【取得】問い合わせごとに Answer::type と Answer::value を出現順に返す
// 【注意】MERGE_INTERVAL は [x,y)、QUERY_SIZE は x のみを使用する
// 【コピー範囲】IntervalUnionDsu とこの struct をコピーして使用する
// 【計算量】O((N+Q)α(N)) と通常 merge 回数分
struct IntervalUnionDsuQuerySolver {
    static constexpr int MERGE = 0;
    static constexpr int MERGE_INTERVAL = 1;
    static constexpr int QUERY_SAME = 2;
    static constexpr int QUERY_SIZE = 3;

    struct Operation {
        int type;
        int x;
        int y;
    };

    struct Answer {
        int type;
        int value;
    };

    // 操作列を処理し、問い合わせへの回答を出現順に返す O((N+Q)α(N))
    static vector<Answer> solve(int count, const vector<Operation>& operations) {
        IntervalUnionDsu uf(count);
        vector<Answer> answers;
        for (const Operation& operation : operations) {
            if (operation.type == MERGE) {
                uf.merge(operation.x, operation.y);
            } else if (operation.type == MERGE_INTERVAL) {
                uf.merge_interval(operation.x, operation.y);
            } else if (operation.type == QUERY_SAME) {
                answers.push_back({QUERY_SAME, uf.same(operation.x, operation.y)});
            } else {
                answers.push_back({QUERY_SIZE, uf.size(operation.x)});
            }
        }
        return answers;
    }
};

// ============================================================================
// MovableDsu
// ============================================================================
// 【用途】集合併合に加え、1要素だけを現在の集合から別集合へ移動し、成分サイズと値合計を管理する
// 【典型】Almost Union-Find、要素移動、グループ変更、union/move/query 問題
// 【使い方】最大 move 回数を指定して構築し、merge(x,y)、move(x,destination)、size(x)、sum(x) を使う
// 【取得】size と sum は x が現在属する論理的な集合について返す
// 【注意】move ごとに新しい内部 node を1個使う。maximum_move_count を実際の move 回数以上にする
// 【コピー範囲】この struct だけで要素移動可能 Union-Find として使用できる
// 【計算量】各操作ならし O(α(N+Q))
struct MovableDsu {
private:
    vector<int> parent_or_size;
    vector<int> member_count;
    vector<long long> component_sum;
    vector<int> node_of_element;
    vector<long long> element_value;
    int next_node;

    int find_node(int node) {
        if (parent_or_size[node] < 0) return node;
        parent_or_size[node] = find_node(parent_or_size[node]);
        return parent_or_size[node];
    }

    bool merge_nodes(int node_x, int node_y) {
        int root_x = find_node(node_x);
        int root_y = find_node(node_y);
        if (root_x == root_y) return false;
        if (-parent_or_size[root_x] < -parent_or_size[root_y]) swap(root_x, root_y);
        parent_or_size[root_x] += parent_or_size[root_y];
        parent_or_size[root_y] = root_x;
        member_count[root_x] += member_count[root_y];
        component_sum[root_x] += component_sum[root_y];
        return true;
    }

public:
    // 各要素の値と最大 move 回数で初期化する O(N+maximum_move_count)
    MovableDsu(const vector<long long>& values, int maximum_move_count)
        : parent_or_size(values.size() + maximum_move_count, -1),
          member_count(values.size() + maximum_move_count, 0),
          component_sum(values.size() + maximum_move_count, 0),
          node_of_element(values.size()),
          element_value(values),
          next_node(static_cast<int>(values.size())) {
        assert(maximum_move_count >= 0);
        for (int element = 0; element < static_cast<int>(values.size()); element++) {
            member_count[element] = 1;
            component_sum[element] = values[element];
            node_of_element[element] = element;
        }
    }

    // x と y が属する集合を併合し、実際に併合したかを返す ならし O(α(N+Q))
    bool merge(int x, int y) {
        return merge_nodes(node_of_element[x], node_of_element[y]);
    }

    // x だけを destination の属する集合へ移動し、集合が変わった場合だけ true を返す ならし O(α(N+Q))
    bool move(int x, int destination) {
        int source_root = find_node(node_of_element[x]);
        int destination_root = find_node(node_of_element[destination]);
        if (source_root == destination_root) return false;
        assert(next_node < static_cast<int>(parent_or_size.size()));

        // 元集合から論理要素 x の寄与を除き、新しい内部 node で x を表し直す
        member_count[source_root]--;
        component_sum[source_root] -= element_value[x];
        int new_node = next_node++;
        parent_or_size[new_node] = -1;
        member_count[new_node] = 1;
        component_sum[new_node] = element_value[x];
        node_of_element[x] = new_node;
        merge_nodes(new_node, destination_root);
        return true;
    }

    // x と y が現在同じ論理集合に属するかを返す ならし O(α(N+Q))
    bool same(int x, int y) {
        return find_node(node_of_element[x]) == find_node(node_of_element[y]);
    }

    // x が現在属する集合の論理要素数を返す ならし O(α(N+Q))
    int size(int x) {
        return member_count[find_node(node_of_element[x])];
    }

    // x が現在属する集合の要素値合計を返す ならし O(α(N+Q))
    long long sum(int x) {
        return component_sum[find_node(node_of_element[x])];
    }

    // x 自身に設定された固定値を返す O(1)
    long long value(int x) const {
        return element_value[x];
    }
};

// ============================================================================
// MovableDsuAlmostUnionFindSolver
// ============================================================================
// 【用途】union、1要素 move、所属集合の size と sum 問い合わせからなる Almost Union-Find を処理する
// 【典型】UVa 11987 Almost Union-Find、グループ間の個人移籍、要素移動付き集合管理
// 【使い方】初期値と Operation 列を渡す。MOVE では x を y の集合へ移す
// 【取得】QUERY の回答を size と sum の組で出現順に返す
// 【注意】move 回数は操作列から自動計数して内部 node 容量を確保する
// 【コピー範囲】MovableDsu とこの struct をコピーして使用する
// 【計算量】O((N+Q)α(N+Q))
struct MovableDsuAlmostUnionFindSolver {
    static constexpr int MERGE = 0;
    static constexpr int MOVE = 1;
    static constexpr int QUERY = 2;

    struct Operation {
        int type;
        int x;
        int y;
    };

    struct Answer {
        int size;
        long long sum;
    };

    // 操作列を処理し、集合情報問い合わせへの回答を出現順に返す O((N+Q)α(N+Q))
    static vector<Answer> solve(
        const vector<long long>& values,
        const vector<Operation>& operations
    ) {
        int move_count = 0;
        for (const Operation& operation : operations) {
            if (operation.type == MOVE) move_count++;
        }
        MovableDsu uf(values, move_count);
        vector<Answer> answers;
        for (const Operation& operation : operations) {
            if (operation.type == MERGE) {
                uf.merge(operation.x, operation.y);
            } else if (operation.type == MOVE) {
                uf.move(operation.x, operation.y);
            } else {
                answers.push_back({uf.size(operation.x), uf.sum(operation.x)});
            }
        }
        return answers;
    }
};

// ============================================================================
// IncrementalBridgeConnectivity
// ============================================================================
// 【用途】無向辺をオンライン追加しながら、橋数・連結成分・二辺連結成分を管理する
// 【典型】辺追加後の橋数、同じ2-edge-connected componentか、閉路追加で既存の橋が消える問題
// 【使い方】add_edge(u,v) を順に呼び、bridge_count、same_connected_component、same_two_edge_component を使う
// 【取得】bridge_count は現在のグラフの橋総数を返す
// 【注意】辺削除は扱わない。通常の連結性だけなら ACL dsu の方が単純で高速
// 【コピー範囲】この struct だけでオンライン橋・二辺連結性を管理できる
// 【計算量】全 M 回の辺追加でならし O((N+M)α(N)) に近い実用計算量
struct IncrementalBridgeConnectivity {
private:
    vector<int> two_edge_component;
    vector<int> connected_component;
    vector<int> connected_component_size;
    vector<int> forest_parent;
    vector<int> last_visit;
    int lca_iteration;
    int bridges;

    int find_two_edge_component(int vertex) {
        if (vertex < 0) return -1;
        if (two_edge_component[vertex] == vertex) return vertex;
        two_edge_component[vertex] = find_two_edge_component(two_edge_component[vertex]);
        return two_edge_component[vertex];
    }

    int find_connected_component(int vertex) {
        vertex = find_two_edge_component(vertex);
        if (connected_component[vertex] == vertex) return vertex;
        connected_component[vertex] = find_connected_component(connected_component[vertex]);
        return connected_component[vertex];
    }

    void make_root(int vertex) {
        vertex = find_two_edge_component(vertex);
        int root = vertex;
        int child = -1;

        // 連結成分を表す森の根まで親子を反転し、vertex を新しい根にする
        while (vertex >= 0) {
            int parent = find_two_edge_component(forest_parent[vertex]);
            forest_parent[vertex] = child;
            connected_component[vertex] = root;
            child = vertex;
            vertex = parent;
        }
        assert(child >= 0);
        connected_component_size[root] = connected_component_size[child];
    }

    void merge_path(int vertex_a, int vertex_b) {
        lca_iteration++;
        vector<int> path_a;
        vector<int> path_b;
        int lca = -1;

        // 2点から交互に森を上り、今回の探索で初めて重なった頂点を LCA とする
        while (lca < 0) {
            if (vertex_a >= 0) {
                vertex_a = find_two_edge_component(vertex_a);
                path_a.push_back(vertex_a);
                if (last_visit[vertex_a] == lca_iteration) {
                    lca = vertex_a;
                    break;
                }
                last_visit[vertex_a] = lca_iteration;
                vertex_a = forest_parent[vertex_a];
            }
            if (vertex_b >= 0) {
                vertex_b = find_two_edge_component(vertex_b);
                path_b.push_back(vertex_b);
                if (last_visit[vertex_b] == lca_iteration) {
                    lca = vertex_b;
                    break;
                }
                last_visit[vertex_b] = lca_iteration;
                vertex_b = forest_parent[vertex_b];
            }
        }

        // 両経路上の LCA までを1つの二辺連結成分へ圧縮し、経路上の橋を消す
        for (int vertex : path_a) {
            two_edge_component[vertex] = lca;
            if (vertex == lca) break;
            bridges--;
        }
        for (int vertex : path_b) {
            two_edge_component[vertex] = lca;
            if (vertex == lca) break;
            bridges--;
        }
    }

public:
    // count 個の頂点と辺0本のグラフで初期化する O(N)
    explicit IncrementalBridgeConnectivity(int count)
        : two_edge_component(count),
          connected_component(count),
          connected_component_size(count, 1),
          forest_parent(count, -1),
          last_visit(count, 0),
          lca_iteration(0),
          bridges(0) {
        iota(two_edge_component.begin(), two_edge_component.end(), 0);
        iota(connected_component.begin(), connected_component.end(), 0);
    }

    // 無向辺を1本追加する ならし O(α(N)) 相当
    void add_edge(int vertex_a, int vertex_b) {
        vertex_a = find_two_edge_component(vertex_a);
        vertex_b = find_two_edge_component(vertex_b);
        if (vertex_a == vertex_b) return;

        int component_a = find_connected_component(vertex_a);
        int component_b = find_connected_component(vertex_b);
        if (component_a != component_b) {
            // 異なる連結成分を結ぶ辺は新しい橋になる。小さい成分側の木を付け替える
            bridges++;
            if (connected_component_size[component_a] > connected_component_size[component_b]) {
                swap(vertex_a, vertex_b);
                swap(component_a, component_b);
            }
            make_root(vertex_a);
            forest_parent[vertex_a] = vertex_b;
            connected_component[vertex_a] = vertex_b;
            connected_component_size[component_b] += connected_component_size[vertex_a];
        } else {
            // 同じ連結成分内への辺は閉路を作り、閉路上の橋を二辺連結成分へ圧縮する
            merge_path(vertex_a, vertex_b);
        }
    }

    // 現在のグラフに含まれる橋の総数を返す O(1)
    int bridge_count() const {
        return bridges;
    }

    // vertex_a と vertex_b が同じ連結成分かを返す ならし O(α(N))
    bool same_connected_component(int vertex_a, int vertex_b) {
        return find_connected_component(vertex_a) == find_connected_component(vertex_b);
    }

    // vertex_a と vertex_b が同じ二辺連結成分かを返す ならし O(α(N))
    bool same_two_edge_component(int vertex_a, int vertex_b) {
        return find_two_edge_component(vertex_a) == find_two_edge_component(vertex_b);
    }
};

// ============================================================================
// IncrementalBridgeCountSolver
// ============================================================================
// 【用途】無向辺を順に追加した各時点の橋数を求める
// 【典型】オンライン橋数、辺追加で橋が増減する履歴、追加専用ネットワークの脆弱性
// 【使い方】Edge 配列を追加順に渡して solve を呼ぶ
// 【取得】answer[i] は edges[i] を追加した直後の橋数
// 【注意】自己ループ・多重辺も扱う。辺削除には対応しない
// 【コピー範囲】IncrementalBridgeConnectivity とこの struct をコピーして使用する
// 【計算量】全体でならし O((N+M)α(N)) に近い実用計算量
struct IncrementalBridgeCountSolver {
    struct Edge {
        int u;
        int v;
    };

    // 各辺追加直後の橋数を返す ならし O((N+M)α(N)) 相当
    static vector<int> solve(int count, const vector<Edge>& edges) {
        IncrementalBridgeConnectivity graph(count);
        vector<int> answer;
        answer.reserve(edges.size());
        for (const Edge& edge : edges) {
            graph.add_edge(edge.u, edge.v);
            answer.push_back(graph.bridge_count());
        }
        return answer;
    }
};

// ============================================================================
// IncrementalTwoEdgeConnectivitySolver
// ============================================================================
// 【用途】辺追加と、2頂点が同じ二辺連結成分かという問い合わせをオンライン処理する
// 【典型】2本以上の辺素経路が存在するか、1本の辺故障では切れない関係、incremental 2-edge connectivity
// 【使い方】ADD_EDGE と QUERY_TWO_EDGE_CONNECTED の Operation を時系列順に渡す
// 【取得】二辺連結性問い合わせの回答を出現順に vector<char> で返す
// 【注意】頂点自身は自分と同じ二辺連結成分。辺削除は扱わない
// 【コピー範囲】IncrementalBridgeConnectivity とこの struct をコピーして使用する
// 【計算量】全体でならし O((N+Q)α(N)) に近い実用計算量
struct IncrementalTwoEdgeConnectivitySolver {
    static constexpr int ADD_EDGE = 0;
    static constexpr int QUERY_TWO_EDGE_CONNECTED = 1;
    static constexpr int QUERY_CONNECTED = 2;

    struct Operation {
        int type;
        int u;
        int v;
    };

    struct Answer {
        int type;
        bool value;
    };

    // 操作列をオンライン処理し、連結性問い合わせへの回答を出現順に返す ならし O((N+Q)α(N)) 相当
    static vector<Answer> solve(int count, const vector<Operation>& operations) {
        IncrementalBridgeConnectivity graph(count);
        vector<Answer> answers;
        for (const Operation& operation : operations) {
            if (operation.type == ADD_EDGE) {
                graph.add_edge(operation.u, operation.v);
            } else if (operation.type == QUERY_TWO_EDGE_CONNECTED) {
                answers.push_back({operation.type, graph.same_two_edge_component(operation.u, operation.v)});
            } else {
                answers.push_back({operation.type, graph.same_connected_component(operation.u, operation.v)});
            }
        }
        return answers;
    }
};

#if __INCLUDE_LEVEL__ == 0

// テスト用の一様乱数を [0, upper_exclusive) で返す
static int test_rand_int(mt19937& random_engine, int upper_exclusive) {
    assert(upper_exclusive > 0);
    uniform_int_distribution<int> distribution(0, upper_exclusive - 1);
    return distribution(random_engine);
}

// テスト用の一様乱数を [lower, upper] で返す
static int test_rand_int(mt19937& random_engine, int lower, int upper) {
    assert(lower <= upper);
    uniform_int_distribution<int> distribution(lower, upper);
    return distribution(random_engine);
}

// active_edge_count から無向グラフの連結成分番号を愚直に求める
static vector<int> test_component_labels(const vector<vector<int>>& active_edge_count) {
    int count = static_cast<int>(active_edge_count.size());
    vector<int> label(count, -1);
    int component = 0;
    for (int start = 0; start < count; start++) {
        if (label[start] >= 0) continue;
        queue<int> que;
        que.push(start);
        label[start] = component;
        while (!que.empty()) {
            int vertex = que.front();
            que.pop();
            for (int next = 0; next < count; next++) {
                if (vertex == next || active_edge_count[vertex][next] <= 0 || label[next] >= 0) continue;
                label[next] = component;
                que.push(next);
            }
        }
        component++;
    }
    return label;
}

// 成分番号から各頂点の成分サイズを求める
static vector<int> test_component_sizes(const vector<int>& label) {
    int maximum = -1;
    for (int value : label) maximum = max(maximum, value);
    vector<int> count(maximum + 1, 0);
    for (int value : label) count[value]++;
    vector<int> result(label.size());
    for (int vertex = 0; vertex < static_cast<int>(label.size()); vertex++) result[vertex] = count[label[vertex]];
    return result;
}

// 成分番号から非連結な unordered 頂点ペア数を求める
static long long test_disconnected_pairs(const vector<int>& label) {
    long long result = 0;
    for (int x = 0; x < static_cast<int>(label.size()); x++) {
        for (int y = x + 1; y < static_cast<int>(label.size()); y++) {
            if (label[x] != label[y]) result++;
        }
    }
    return result;
}

struct TestDifferenceAnalysis {
    vector<int> component;
    vector<long long> value;
    vector<char> bad_component;
};

// value[y]-value[x]=difference の制約グラフを BFS し、成分・相対値・矛盾を愚直に求める
static TestDifferenceAnalysis test_analyze_difference(
    int count,
    const vector<tuple<int, int, long long>>& equations
) {
    vector<vector<pair<int, long long>>> graph(count);
    for (auto [x, y, difference] : equations) {
        graph[x].push_back({y, difference});
        graph[y].push_back({x, -difference});
    }

    vector<int> component(count, -1);
    vector<long long> value(count, 0);
    vector<char> bad;
    int component_count = 0;
    for (int start = 0; start < count; start++) {
        if (component[start] >= 0) continue;
        bad.push_back(false);
        queue<int> que;
        que.push(start);
        component[start] = component_count;
        value[start] = 0;
        while (!que.empty()) {
            int vertex = que.front();
            que.pop();
            for (auto [next, difference] : graph[vertex]) {
                long long expected = value[vertex] + difference;
                if (component[next] < 0) {
                    component[next] = component_count;
                    value[next] = expected;
                    que.push(next);
                } else if (value[next] != expected) {
                    bad[component_count] = true;
                }
            }
        }
        component_count++;
    }
    return {move(component), move(value), move(bad)};
}

struct TestModAnalysis {
    vector<int> component;
    vector<long long> value;
    vector<char> bad_component;
};

// 法付き差分制約を BFS し、成分・相対値・矛盾を愚直に求める
static TestModAnalysis test_analyze_mod_difference(
    int count,
    long long modulus,
    const vector<tuple<int, int, long long>>& equations
) {
    auto normalize = [modulus](long long value) {
        value %= modulus;
        if (value < 0) value += modulus;
        return value;
    };
    vector<vector<pair<int, long long>>> graph(count);
    for (auto [x, y, difference] : equations) {
        difference = normalize(difference);
        graph[x].push_back({y, difference});
        graph[y].push_back({x, normalize(-difference)});
    }

    vector<int> component(count, -1);
    vector<long long> value(count, 0);
    vector<char> bad;
    int component_count = 0;
    for (int start = 0; start < count; start++) {
        if (component[start] >= 0) continue;
        bad.push_back(false);
        queue<int> que;
        que.push(start);
        component[start] = component_count;
        while (!que.empty()) {
            int vertex = que.front();
            que.pop();
            for (auto [next, difference] : graph[vertex]) {
                long long expected = normalize(value[vertex] + difference);
                if (component[next] < 0) {
                    component[next] = component_count;
                    value[next] = expected;
                    que.push(next);
                } else if (value[next] != expected) {
                    bad[component_count] = true;
                }
            }
        }
        component_count++;
    }
    return {move(component), move(value), move(bad)};
}

// PotentialDsu と差分等式 solver の固定・ランダムテスト
static void test_potential_and_equality(mt19937& random_engine) {
    {
        PotentialDsu<long long> uf(5);
        assert(uf.merge(0, 1, 3) == PotentialDsu<long long>::MergeResult::merged);
        assert(uf.merge(1, 2, -5) == PotentialDsu<long long>::MergeResult::merged);
        assert(uf.difference(0, 2).value() == -2);
        assert(uf.merge(0, 2, -2) == PotentialDsu<long long>::MergeResult::already_consistent);
        assert(uf.merge(0, 2, 7) == PotentialDsu<long long>::MergeResult::contradiction);
        assert(!uf.consistent(1));
        assert(!uf.difference(0, 4).has_value());
    }

    {
        using Solver = DifferenceEqualitySystemSolver<long long>;
        vector<Solver::Equation> equations = {{0, 1, 4}, {1, 2, -1}, {3, 4, 8}};
        vector<Solver::FixedValue> fixed_values = {{0, 10}};
        Solver::Result result = Solver::solve(5, equations, fixed_values);
        assert(result.consistent);
        assert(result.free_components == 1);
        assert(result.value[0] == 10 && result.value[1] == 14 && result.value[2] == 13);
        assert(result.value[4] - result.value[3] == 8);
        assert(result.anchored[0] && result.anchored[2] && !result.anchored[3]);

        fixed_values.push_back({2, 100});
        result = Solver::solve(5, equations, fixed_values);
        assert(!result.consistent);
        assert(result.contradiction_kind == 2);
        assert(result.contradiction_index == 1);
    }

    {
        using Solver = DifferenceEqualityQuerySolver<long long>;
        vector<Solver::Operation> operations = {
            {Solver::ADD_EQUATION, 0, 1, 2},
            {Solver::QUERY_DIFFERENCE, 0, 2, 0},
            {Solver::ADD_EQUATION, 1, 2, 3},
            {Solver::QUERY_DIFFERENCE, 0, 2, 0},
            {Solver::ADD_EQUATION, 0, 2, 9},
            {Solver::QUERY_DIFFERENCE, 1, 2, 0},
        };
        auto answers = Solver::solve(3, operations);
        assert(answers.size() == 3);
        assert(answers[0].state == Solver::DISCONNECTED);
        assert(answers[1].state == Solver::DETERMINED && answers[1].difference == 5);
        assert(answers[2].state == Solver::INCONSISTENT);
    }

    // 隠れた真値から整合する制約を作り、全連結ペアの差を比較する
    for (int test_case = 0; test_case < 400; test_case++) {
        int count = test_rand_int(random_engine, 1, 12);
        vector<long long> hidden(count);
        for (long long& value : hidden) value = test_rand_int(random_engine, -30, 30);
        PotentialDsu<long long> uf(count);
        vector<tuple<int, int, long long>> equations;
        int operation_count = test_rand_int(random_engine, 1, 60);
        for (int operation = 0; operation < operation_count; operation++) {
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);
            long long difference = hidden[y] - hidden[x];
            uf.merge(x, y, difference);
            equations.push_back({x, y, difference});

            TestDifferenceAnalysis naive = test_analyze_difference(count, equations);
            for (int a = 0; a < count; a++) {
                assert(uf.consistent(a));
                for (int b = 0; b < count; b++) {
                    bool connected = naive.component[a] == naive.component[b];
                    assert(uf.same(a, b) == connected);
                    optional<long long> actual = uf.difference(a, b);
                    assert(actual.has_value() == connected);
                    if (connected) assert(*actual == naive.value[b] - naive.value[a]);
                }
            }
        }

        // 連結済みの異なる2点へ誤った差を入れ、成分の矛盾検出も確認する
        if (count >= 2) {
            uf.merge(0, 1, hidden[1] - hidden[0]);
            uf.merge(0, 1, hidden[1] - hidden[0] + 1);
            assert(!uf.consistent(0));
        }
    }
}

// ModPotentialDsu と法付き等式 solver の固定・ランダムテスト
static void test_mod_potential_and_equality(mt19937& random_engine) {
    {
        ModPotentialDsu uf(4, 7);
        assert(uf.merge(0, 1, 9) == ModPotentialDsu::MergeResult::merged);
        assert(uf.merge(1, 2, -3) == ModPotentialDsu::MergeResult::merged);
        assert(uf.difference(0, 2).value() == 6);
        assert(uf.merge(0, 2, 6) == ModPotentialDsu::MergeResult::already_consistent);
        assert(uf.merge(0, 2, 5) == ModPotentialDsu::MergeResult::contradiction);
        assert(!uf.consistent(2));
    }

    {
        vector<ModEqualitySystemSolver::Equation> equations = {{0, 1, 4}, {1, 2, 8}};
        vector<ModEqualitySystemSolver::FixedValue> fixed_values = {{0, 9}};
        auto result = ModEqualitySystemSolver::solve(4, 11, equations, fixed_values);
        assert(result.consistent);
        assert(result.value[0] == 9 && result.value[1] == 2 && result.value[2] == 10);
        assert(result.free_components == 1);
        fixed_values.push_back({2, 3});
        result = ModEqualitySystemSolver::solve(4, 11, equations, fixed_values);
        assert(!result.consistent);
    }

    for (int test_case = 0; test_case < 300; test_case++) {
        int count = test_rand_int(random_engine, 1, 10);
        long long modulus = test_rand_int(random_engine, 2, 20);
        vector<long long> hidden(count);
        for (long long& value : hidden) value = test_rand_int(random_engine, 0, static_cast<int>(modulus) - 1);
        ModPotentialDsu uf(count, modulus);
        vector<tuple<int, int, long long>> equations;
        int operation_count = test_rand_int(random_engine, 1, 50);
        for (int operation = 0; operation < operation_count; operation++) {
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);
            long long difference = (hidden[y] - hidden[x]) % modulus;
            uf.merge(x, y, difference);
            equations.push_back({x, y, difference});
            TestModAnalysis naive = test_analyze_mod_difference(count, modulus, equations);
            for (int a = 0; a < count; a++) {
                assert(uf.consistent(a));
                for (int b = 0; b < count; b++) {
                    bool connected = naive.component[a] == naive.component[b];
                    assert(uf.same(a, b) == connected);
                    optional<long long> actual = uf.difference(a, b);
                    assert(actual.has_value() == connected);
                    if (connected) {
                        long long expected = (naive.value[b] - naive.value[a]) % modulus;
                        if (expected < 0) expected += modulus;
                        assert(*actual == expected);
                    }
                }
            }
        }
    }
}


// SignedPotentialDsu と符号付き等式 solver の固定・ランダムテスト
static void test_signed_equality(mt19937& random_engine) {
    {
        SignedPotentialDsu uf(3);
        assert(uf.add_difference(0, 1, 2) == SignedPotentialDsu::MergeResult::merged);
        assert(uf.add_sum(0, 1, 8) == SignedPotentialDsu::MergeResult::already_consistent);
        assert(uf.twice_value(0).value() == 6);
        assert(uf.twice_value(1).value() == 10);
        assert(uf.integer_consistent(0));
        auto relation = uf.relation(0, 1).value();
        assert(relation.sign == 1 && relation.twice_offset == 4);
    }

    {
        SignedPotentialDsu uf(2);
        uf.add_difference(0, 1, 0);
        uf.add_sum(0, 1, 5);
        assert(uf.consistent(0));
        assert(!uf.integer_consistent(0));
        assert(uf.twice_value(0).value() == 5);
        assert(uf.twice_value(1).value() == 5);
    }

    {
        vector<SignedEqualitySystemSolver::Equation> equations = {
            {0, 1, 1, 2},
            {0, 1, -1, 8},
            {1, 2, -1, 11},
        };
        vector<SignedEqualitySystemSolver::FixedValue> fixed_values;
        auto result = SignedEqualitySystemSolver::solve(4, equations, fixed_values);
        assert(result.rational_consistent && result.integer_consistent);
        assert(result.twice_value[0] == 6);
        assert(result.twice_value[1] == 10);
        assert(result.twice_value[2] == 12);
        assert(result.free_components == 1);
        assert(result.determined[0] && result.determined[2] && !result.determined[3]);

        equations.push_back({0, 2, 1, 100});
        result = SignedEqualitySystemSolver::solve(4, equations, fixed_values);
        assert(!result.rational_consistent);
    }

    // 隠れた整数解から符号付き等式を作り、相対式と構成解を照合する
    for (int test_case = 0; test_case < 350; test_case++) {
        int count = test_rand_int(random_engine, 1, 12);
        vector<long long> hidden(count);
        for (long long& value : hidden) value = test_rand_int(random_engine, -20, 20);
        SignedPotentialDsu uf(count);
        vector<SignedEqualitySystemSolver::Equation> equations;
        int equation_count = test_rand_int(random_engine, 1, 60);

        for (int index = 0; index < equation_count; index++) {
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);
            int sign = test_rand_int(random_engine, 2) == 0 ? -1 : 1;
            long long offset = hidden[y] - sign * hidden[x];
            equations.push_back({x, y, sign, offset});
            auto merge_result = uf.merge(x, y, sign, offset);
            assert(merge_result != SignedPotentialDsu::MergeResult::contradiction);

            for (int a = 0; a < count; a++) {
                assert(uf.consistent(a));
                assert(uf.integer_consistent(a));
                for (int b = 0; b < count; b++) {
                    optional<SignedPotentialDsu::Relation> relation = uf.relation(a, b);
                    assert(relation.has_value() == uf.same(a, b));
                    if (relation.has_value()) {
                        assert(2 * hidden[b] == relation->sign * 2 * hidden[a] + relation->twice_offset);
                    }
                }
            }
        }

        vector<SignedEqualitySystemSolver::FixedValue> fixed_values;
        for (int vertex = 0; vertex < count; vertex++) {
            if (test_rand_int(random_engine, 4) == 0) fixed_values.push_back({vertex, hidden[vertex]});
        }
        auto result = SignedEqualitySystemSolver::solve(count, equations, fixed_values);
        assert(result.rational_consistent && result.integer_consistent);
        for (const auto& equation : equations) {
            assert(
                result.twice_value[equation.y] ==
                equation.sign * result.twice_value[equation.x] + 2 * equation.offset
            );
        }
        for (const auto& fixed : fixed_values) assert(result.twice_value[fixed.x] == 2 * fixed.value);

        // 全変数を固定した後に誤った等式を加えれば必ず矛盾する
        vector<SignedEqualitySystemSolver::FixedValue> all_fixed;
        for (int vertex = 0; vertex < count; vertex++) all_fixed.push_back({vertex, hidden[vertex]});
        vector<SignedEqualitySystemSolver::Equation> contradictory = equations;
        int x = test_rand_int(random_engine, count);
        int y = test_rand_int(random_engine, count);
        contradictory.push_back({x, y, 1, hidden[y] - hidden[x] + 1});
        result = SignedEqualitySystemSolver::solve(count, contradictory, all_fixed);
        assert(!result.rational_consistent);
    }
}

// ComponentAddDsu と専用 solver の固定・ランダムテスト
static void test_component_add(mt19937& random_engine) {
    {
        ComponentAddDsu<long long> uf(vector<long long>{1, 2, 3, 4});
        uf.add_component(0, 10);
        uf.merge(0, 1);
        assert(uf.get(0) == 11 && uf.get(1) == 2);
        uf.add_component(1, 5);
        assert(uf.get(0) == 16 && uf.get(1) == 7);
        uf.merge(2, 0);
        uf.add_component(2, -2);
        assert(uf.get(2) == 1 && uf.get(0) == 14 && uf.get(1) == 5);
    }

    {
        using Solver = ComponentAddQuerySolver<long long>;
        vector<Solver::Operation> operations = {
            {Solver::ADD_COMPONENT, 0, 0, 5},
            {Solver::MERGE, 0, 1, 0},
            {Solver::QUERY_VALUE, 1, 0, 0},
            {Solver::ADD_COMPONENT, 1, 0, 3},
            {Solver::QUERY_VALUE, 0, 0, 0},
            {Solver::QUERY_VALUE, 1, 0, 0},
        };
        vector<long long> answer = Solver::solve(2, operations);
        assert((answer == vector<long long>{0, 8, 3}));
    }

    for (int test_case = 0; test_case < 400; test_case++) {
        int count = test_rand_int(random_engine, 1, 15);
        vector<long long> initial(count);
        for (long long& value : initial) value = test_rand_int(random_engine, -10, 10);
        ComponentAddDsu<long long> uf(initial);
        vector<int> group(count);
        iota(group.begin(), group.end(), 0);
        vector<long long> value = initial;

        for (int operation = 0; operation < 150; operation++) {
            int type = test_rand_int(random_engine, 3);
            int x = test_rand_int(random_engine, count);
            if (type == 0) {
                int y = test_rand_int(random_engine, count);
                int gx = group[x];
                int gy = group[y];
                bool expected = gx != gy;
                assert(uf.merge(x, y) == expected);
                if (expected) {
                    for (int vertex = 0; vertex < count; vertex++) {
                        if (group[vertex] == gy) group[vertex] = gx;
                    }
                }
            } else if (type == 1) {
                long long delta = test_rand_int(random_engine, -20, 20);
                int gx = group[x];
                uf.add_component(x, delta);
                for (int vertex = 0; vertex < count; vertex++) {
                    if (group[vertex] == gx) value[vertex] += delta;
                }
            } else {
                assert(uf.get(x) == value[x]);
            }
            for (int vertex = 0; vertex < count; vertex++) assert(uf.get(vertex) == value[vertex]);
        }
    }
}

// RollbackDsu 本体と4種類の動的グラフ solver、version tree solver のテスト
static void test_rollback_and_dynamic_graph(mt19937& random_engine) {
    // snapshot と rollback をランダムに繰り返し、愚直な集合ラベル履歴と比較する
    for (int test_case = 0; test_case < 250; test_case++) {
        int count = test_rand_int(random_engine, 1, 12);
        RollbackDsu uf(count);
        vector<int> initial(count);
        iota(initial.begin(), initial.end(), 0);
        vector<vector<int>> states(1, initial);

        for (int operation = 0; operation < 200; operation++) {
            bool do_rollback = uf.snapshot() > 0 && test_rand_int(random_engine, 5) == 0;
            if (do_rollback) {
                int target = test_rand_int(random_engine, uf.snapshot() + 1);
                uf.rollback(target);
                states.resize(target + 1);
            } else {
                int x = test_rand_int(random_engine, count);
                int y = test_rand_int(random_engine, count);
                vector<int> next = states.back();
                int group_x = next[x];
                int group_y = next[y];
                bool expected = group_x != group_y;
                assert(uf.merge(x, y) == expected);
                if (expected) {
                    for (int& group : next) {
                        if (group == group_y) group = group_x;
                    }
                }
                states.push_back(move(next));
            }

            const vector<int>& current = states.back();
            set<int> unique_groups(current.begin(), current.end());
            assert(uf.component_count() == static_cast<int>(unique_groups.size()));
            for (int x = 0; x < count; x++) {
                int expected_size = 0;
                for (int y = 0; y < count; y++) {
                    bool expected_same = current[x] == current[y];
                    assert(uf.same(x, y) == expected_same);
                    if (expected_same) expected_size++;
                }
                assert(uf.size(x) == expected_size);
            }
        }
    }

    // 同一の多重辺付き操作列を4種類の solver へ与え、各統計を BFS 再計算と比較する
    for (int test_case = 0; test_case < 220; test_case++) {
        int count = test_rand_int(random_engine, 1, 8);
        int operation_count = test_rand_int(random_engine, 20, 100);
        vector<vector<int>> active(count, vector<int>(count, 0));
        vector<tuple<int, int, int>> generic_operations;
        vector<char> expected_same;
        vector<int> expected_component_count;
        vector<int> expected_component_size;
        vector<long long> expected_disconnected_pairs;

        for (int time = 0; time < operation_count; time++) {
            vector<pair<int, int>> active_pairs;
            for (int u = 0; u < count; u++) {
                for (int v = u; v < count; v++) {
                    if (active[u][v] > 0) active_pairs.push_back({u, v});
                }
            }

            int choice = test_rand_int(random_engine, 100);
            if (!active_pairs.empty() && choice < 25) {
                auto [u, v] = active_pairs[test_rand_int(random_engine, static_cast<int>(active_pairs.size()))];
                generic_operations.push_back({1, u, v});
                active[u][v]--;
                if (u != v) active[v][u]--;
            } else if (choice < 58) {
                int u = test_rand_int(random_engine, count);
                int v = test_rand_int(random_engine, count);
                generic_operations.push_back({0, u, v});
                active[u][v]++;
                if (u != v) active[v][u]++;
            } else {
                int u = test_rand_int(random_engine, count);
                int v = test_rand_int(random_engine, count);
                generic_operations.push_back({2, u, v});
                vector<int> label = test_component_labels(active);
                vector<int> sizes = test_component_sizes(label);
                set<int> components(label.begin(), label.end());
                expected_same.push_back(label[u] == label[v]);
                expected_component_count.push_back(static_cast<int>(components.size()));
                expected_component_size.push_back(sizes[u]);
                expected_disconnected_pairs.push_back(test_disconnected_pairs(label));
            }
        }

        vector<RollbackDsuDynamicConnectivitySolver::Operation> connectivity_operations;
        vector<RollbackDsuDynamicComponentCountSolver::Operation> count_operations;
        vector<RollbackDsuDynamicComponentSizeSolver::Operation> size_operations;
        vector<RollbackDsuDynamicDisconnectedPairsSolver::Operation> pair_operations;
        for (auto [type, u, v] : generic_operations) {
            int connectivity_type = type == 0 ? RollbackDsuDynamicConnectivitySolver::ADD_EDGE
                : type == 1 ? RollbackDsuDynamicConnectivitySolver::REMOVE_EDGE
                : RollbackDsuDynamicConnectivitySolver::QUERY_SAME;
            int count_type = type == 0 ? RollbackDsuDynamicComponentCountSolver::ADD_EDGE
                : type == 1 ? RollbackDsuDynamicComponentCountSolver::REMOVE_EDGE
                : RollbackDsuDynamicComponentCountSolver::QUERY_COMPONENT_COUNT;
            int size_type = type == 0 ? RollbackDsuDynamicComponentSizeSolver::ADD_EDGE
                : type == 1 ? RollbackDsuDynamicComponentSizeSolver::REMOVE_EDGE
                : RollbackDsuDynamicComponentSizeSolver::QUERY_COMPONENT_SIZE;
            int pair_type = type == 0 ? RollbackDsuDynamicDisconnectedPairsSolver::ADD_EDGE
                : type == 1 ? RollbackDsuDynamicDisconnectedPairsSolver::REMOVE_EDGE
                : RollbackDsuDynamicDisconnectedPairsSolver::QUERY_DISCONNECTED_PAIRS;
            connectivity_operations.push_back({connectivity_type, u, v});
            count_operations.push_back({count_type, u, v});
            size_operations.push_back({size_type, u, v});
            pair_operations.push_back({pair_type, u, v});
        }

        assert(RollbackDsuDynamicConnectivitySolver::solve(count, connectivity_operations) == expected_same);
        assert(RollbackDsuDynamicComponentCountSolver::solve(count, count_operations) == expected_component_count);
        assert(RollbackDsuDynamicComponentSizeSolver::solve(count, size_operations) == expected_component_size);
        assert(RollbackDsuDynamicDisconnectedPairsSolver::solve(count, pair_operations) == expected_disconnected_pairs);
    }

    // 分岐 version tree を、各バージョンの愚直な集合ラベルコピーと比較する
    for (int test_case = 0; test_case < 250; test_case++) {
        int count = test_rand_int(random_engine, 1, 10);
        int operation_count = test_rand_int(random_engine, 1, 100);
        vector<RollbackDsuVersionTreeConnectivitySolver::Operation> operations;
        vector<vector<int>> versions;
        vector<char> expected;
        vector<int> initial(count);
        iota(initial.begin(), initial.end(), 0);

        for (int index = 0; index < operation_count; index++) {
            int base = test_rand_int(random_engine, index + 1) - 1;
            vector<int> state = base < 0 ? initial : versions[base];
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);
            if (test_rand_int(random_engine, 100) < 65) {
                operations.push_back({RollbackDsuVersionTreeConnectivitySolver::MERGE, base, x, y});
                int group_x = state[x];
                int group_y = state[y];
                if (group_x != group_y) {
                    for (int& group : state) {
                        if (group == group_y) group = group_x;
                    }
                }
            } else {
                operations.push_back({RollbackDsuVersionTreeConnectivitySolver::QUERY_SAME, base, x, y});
                expected.push_back(state[x] == state[y]);
            }
            versions.push_back(move(state));
        }
        assert(RollbackDsuVersionTreeConnectivitySolver::solve(count, operations) == expected);
    }
}

// RollbackValueDsu と動的成分和 solver の固定・ランダムテスト
static void test_rollback_value(mt19937& random_engine) {
    {
        RollbackValueDsu uf({1, 2, 3});
        int saved = uf.snapshot();
        uf.merge(0, 1);
        uf.add_value(1, 5);
        assert(uf.component_sum(0) == 8);
        assert(uf.value(1) == 7);
        uf.rollback(saved);
        assert(uf.component_sum(0) == 1 && uf.component_sum(1) == 2);
        assert(uf.value(1) == 2);
    }

    for (int test_case = 0; test_case < 180; test_case++) {
        int count = test_rand_int(random_engine, 1, 8);
        int operation_count = test_rand_int(random_engine, 20, 90);
        vector<long long> initial(count);
        for (long long& value : initial) value = test_rand_int(random_engine, -10, 10);
        vector<long long> values = initial;
        vector<vector<int>> active(count, vector<int>(count, 0));
        vector<RollbackValueDsuDynamicComponentSumSolver::Operation> operations;
        vector<long long> expected;

        for (int time = 0; time < operation_count; time++) {
            vector<pair<int, int>> active_pairs;
            for (int u = 0; u < count; u++) {
                for (int v = u; v < count; v++) {
                    if (active[u][v] > 0) active_pairs.push_back({u, v});
                }
            }
            int choice = test_rand_int(random_engine, 100);
            if (!active_pairs.empty() && choice < 20) {
                auto [u, v] = active_pairs[test_rand_int(random_engine, static_cast<int>(active_pairs.size()))];
                operations.push_back({RollbackValueDsuDynamicComponentSumSolver::REMOVE_EDGE, u, v, 0});
                active[u][v]--;
                if (u != v) active[v][u]--;
            } else if (choice < 43) {
                int u = test_rand_int(random_engine, count);
                int v = test_rand_int(random_engine, count);
                operations.push_back({RollbackValueDsuDynamicComponentSumSolver::ADD_EDGE, u, v, 0});
                active[u][v]++;
                if (u != v) active[v][u]++;
            } else if (choice < 70) {
                int vertex = test_rand_int(random_engine, count);
                long long delta = test_rand_int(random_engine, -15, 15);
                operations.push_back({RollbackValueDsuDynamicComponentSumSolver::ADD_VALUE, vertex, 0, delta});
                values[vertex] += delta;
            } else {
                int vertex = test_rand_int(random_engine, count);
                operations.push_back({RollbackValueDsuDynamicComponentSumSolver::QUERY_COMPONENT_SUM, vertex, 0, 0});
                vector<int> label = test_component_labels(active);
                long long sum = 0;
                for (int other = 0; other < count; other++) {
                    if (label[other] == label[vertex]) sum += values[other];
                }
                expected.push_back(sum);
            }
        }

        vector<long long> actual = RollbackValueDsuDynamicComponentSumSolver::solve(initial, operations);
        assert(actual == expected);
    }
}

// RollbackParityDsu と動的二部・XOR 等式 solver の固定・ランダムテスト
static void test_rollback_parity(mt19937& random_engine) {
    {
        RollbackParityDsu uf(4);
        int saved = uf.snapshot();
        assert(uf.merge(0, 1, 1) == RollbackParityDsu::MergeResult::merged);
        assert(uf.merge(1, 2, 1) == RollbackParityDsu::MergeResult::merged);
        assert(uf.relation(0, 2).value() == 0);
        assert(uf.merge(0, 2, 1) == RollbackParityDsu::MergeResult::contradiction);
        assert(!uf.globally_consistent());
        uf.rollback(saved);
        assert(uf.globally_consistent());
        assert(!uf.same(0, 1));
    }

    // 動的二部グラフを各時点の BFS 2彩色と比較する
    for (int test_case = 0; test_case < 180; test_case++) {
        int count = test_rand_int(random_engine, 1, 8);
        int operation_count = test_rand_int(random_engine, 20, 90);
        vector<vector<int>> active(count, vector<int>(count, 0));
        vector<RollbackParityDsuDynamicBipartiteSolver::Operation> operations;
        vector<char> expected;

        for (int time = 0; time < operation_count; time++) {
            vector<pair<int, int>> active_pairs;
            for (int u = 0; u < count; u++) {
                for (int v = u; v < count; v++) {
                    if (active[u][v] > 0) active_pairs.push_back({u, v});
                }
            }
            int choice = test_rand_int(random_engine, 100);
            if (!active_pairs.empty() && choice < 24) {
                auto [u, v] = active_pairs[test_rand_int(random_engine, static_cast<int>(active_pairs.size()))];
                operations.push_back({RollbackParityDsuDynamicBipartiteSolver::REMOVE_EDGE, u, v});
                active[u][v]--;
                if (u != v) active[v][u]--;
            } else if (choice < 58) {
                int u = test_rand_int(random_engine, count);
                int v = test_rand_int(random_engine, count);
                operations.push_back({RollbackParityDsuDynamicBipartiteSolver::ADD_EDGE, u, v});
                active[u][v]++;
                if (u != v) active[v][u]++;
            } else {
                operations.push_back({RollbackParityDsuDynamicBipartiteSolver::QUERY_BIPARTITE, 0, 0});
                vector<tuple<int, int, long long>> equations;
                for (int u = 0; u < count; u++) {
                    for (int v = u; v < count; v++) {
                        if (active[u][v] > 0) equations.push_back({u, v, 1});
                    }
                }
                TestModAnalysis analysis = test_analyze_mod_difference(count, 2, equations);
                bool consistent = true;
                for (char bad : analysis.bad_component) {
                    if (bad) consistent = false;
                }
                expected.push_back(consistent);
            }
        }
        assert(RollbackParityDsuDynamicBipartiteSolver::solve(count, operations) == expected);
    }

    // ID付き XOR 等式の追加削除と関係問い合わせを愚直な制約グラフと比較する
    for (int test_case = 0; test_case < 180; test_case++) {
        using Solver = RollbackParityDsuDynamicConstraintSolver;
        int count = test_rand_int(random_engine, 1, 7);
        int operation_count = test_rand_int(random_engine, 20, 90);
        vector<Solver::Operation> operations;
        vector<tuple<int, int, int>> constraint_by_id;
        vector<char> active;
        vector<Solver::Answer> expected;
        int next_id = 0;

        for (int time = 0; time < operation_count; time++) {
            vector<int> active_ids;
            for (int id = 0; id < next_id; id++) {
                if (active[id]) active_ids.push_back(id);
            }
            int choice = test_rand_int(random_engine, 100);
            if (!active_ids.empty() && choice < 20) {
                int id = active_ids[test_rand_int(random_engine, static_cast<int>(active_ids.size()))];
                operations.push_back({Solver::REMOVE_CONSTRAINT, id, 0, 0, 0});
                active[id] = false;
            } else if (choice < 48) {
                int u = test_rand_int(random_engine, count);
                int v = test_rand_int(random_engine, count);
                int parity = test_rand_int(random_engine, 2);
                operations.push_back({Solver::ADD_CONSTRAINT, next_id, u, v, parity});
                constraint_by_id.push_back({u, v, parity});
                active.push_back(true);
                next_id++;
            } else {
                vector<tuple<int, int, long long>> equations;
                for (int id = 0; id < next_id; id++) {
                    if (active[id]) {
                        auto [u, v, parity] = constraint_by_id[id];
                        equations.push_back({u, v, parity});
                    }
                }
                TestModAnalysis analysis = test_analyze_mod_difference(count, 2, equations);
                bool global_consistent = true;
                for (char bad : analysis.bad_component) {
                    if (bad) global_consistent = false;
                }

                if (choice < 72) {
                    operations.push_back({Solver::QUERY_CONSISTENCY, -1, 0, 0, 0});
                    expected.push_back({
                        time,
                        Solver::QUERY_CONSISTENCY,
                        Solver::DETERMINED,
                        0,
                        global_consistent,
                    });
                } else {
                    int u = test_rand_int(random_engine, count);
                    int v = test_rand_int(random_engine, count);
                    operations.push_back({Solver::QUERY_RELATION, -1, u, v, 0});
                    int state;
                    int relation = 0;
                    if (analysis.component[u] != analysis.component[v]) {
                        state = Solver::DISCONNECTED;
                    } else if (analysis.bad_component[analysis.component[u]]) {
                        state = Solver::INCONSISTENT;
                    } else {
                        state = Solver::DETERMINED;
                        relation = static_cast<int>((analysis.value[u] ^ analysis.value[v]) & 1LL);
                    }
                    expected.push_back({time, Solver::QUERY_RELATION, state, relation, global_consistent});
                }
            }
        }

        vector<Solver::Answer> actual = Solver::solve(count, operations);
        assert(actual.size() == expected.size());
        for (int index = 0; index < static_cast<int>(actual.size()); index++) {
            assert(actual[index].operation_index == expected[index].operation_index);
            assert(actual[index].query_type == expected[index].query_type);
            assert(actual[index].state == expected[index].state);
            assert(actual[index].relation == expected[index].relation);
            assert(actual[index].consistent == expected[index].consistent);
        }
    }
}

// RollbackPotentialDsu と動的差分等式 solver の固定・ランダムテスト
static void test_rollback_potential(mt19937& random_engine) {
    {
        RollbackPotentialDsu<long long> uf(4);
        int saved = uf.snapshot();
        uf.merge(0, 1, 7);
        uf.merge(1, 2, -2);
        assert(uf.difference(0, 2).value() == 5);
        uf.merge(0, 2, 6);
        assert(!uf.component_consistent(1));
        uf.rollback(saved);
        assert(uf.globally_consistent());
        assert(!uf.same(0, 1));
    }

    for (int test_case = 0; test_case < 180; test_case++) {
        using Solver = RollbackPotentialDsuDynamicEqualitySolver<long long>;
        int count = test_rand_int(random_engine, 1, 7);
        int operation_count = test_rand_int(random_engine, 20, 90);
        vector<Solver::Operation> operations;
        vector<tuple<int, int, long long>> equation_by_id;
        vector<char> active;
        vector<Solver::Answer> expected;
        int next_id = 0;

        for (int time = 0; time < operation_count; time++) {
            vector<int> active_ids;
            for (int id = 0; id < next_id; id++) {
                if (active[id]) active_ids.push_back(id);
            }
            int choice = test_rand_int(random_engine, 100);
            if (!active_ids.empty() && choice < 20) {
                int id = active_ids[test_rand_int(random_engine, static_cast<int>(active_ids.size()))];
                operations.push_back({Solver::REMOVE_EQUATION, id, 0, 0, 0});
                active[id] = false;
            } else if (choice < 48) {
                int x = test_rand_int(random_engine, count);
                int y = test_rand_int(random_engine, count);
                long long difference = test_rand_int(random_engine, -12, 12);
                operations.push_back({Solver::ADD_EQUATION, next_id, x, y, difference});
                equation_by_id.push_back({x, y, difference});
                active.push_back(true);
                next_id++;
            } else {
                vector<tuple<int, int, long long>> equations;
                for (int id = 0; id < next_id; id++) {
                    if (active[id]) equations.push_back(equation_by_id[id]);
                }
                TestDifferenceAnalysis analysis = test_analyze_difference(count, equations);
                bool global_consistent = true;
                for (char bad : analysis.bad_component) {
                    if (bad) global_consistent = false;
                }

                if (choice < 72) {
                    operations.push_back({Solver::QUERY_CONSISTENCY, -1, 0, 0, 0});
                    expected.push_back({
                        time,
                        Solver::QUERY_CONSISTENCY,
                        Solver::DETERMINED,
                        0,
                        global_consistent,
                    });
                } else {
                    int x = test_rand_int(random_engine, count);
                    int y = test_rand_int(random_engine, count);
                    operations.push_back({Solver::QUERY_DIFFERENCE, -1, x, y, 0});
                    int state;
                    long long difference = 0;
                    if (analysis.component[x] != analysis.component[y]) {
                        state = Solver::DISCONNECTED;
                    } else if (analysis.bad_component[analysis.component[x]]) {
                        state = Solver::INCONSISTENT;
                    } else {
                        state = Solver::DETERMINED;
                        difference = analysis.value[y] - analysis.value[x];
                    }
                    expected.push_back({time, Solver::QUERY_DIFFERENCE, state, difference, global_consistent});
                }
            }
        }

        vector<Solver::Answer> actual = Solver::solve(count, operations);
        assert(actual.size() == expected.size());
        for (int index = 0; index < static_cast<int>(actual.size()); index++) {
            assert(actual[index].operation_index == expected[index].operation_index);
            assert(actual[index].query_type == expected[index].query_type);
            assert(actual[index].state == expected[index].state);
            assert(actual[index].difference == expected[index].difference);
            assert(actual[index].consistent == expected[index].consistent);
        }
    }
}

// 部分永続・完全永続 Union-Find と各 solver の固定・ランダムテスト
static void test_persistent_variants(mt19937& random_engine) {
    // 部分永続 DSU を各時刻までの愚直な辺再適用と比較する
    for (int test_case = 0; test_case < 220; test_case++) {
        int count = test_rand_int(random_engine, 1, 10);
        int edge_count = test_rand_int(random_engine, 0, 35);
        vector<PartiallyPersistentDsuHistoricalQuerySolver::Edge> edges;
        PartiallyPersistentDsu uf(count);
        vector<pair<int, int>> raw_edges;

        for (int time = 0; time < edge_count; time++) {
            int u = test_rand_int(random_engine, count);
            int v = test_rand_int(random_engine, count);
            raw_edges.push_back({u, v});
            edges.push_back({time, u, v});
            uf.merge(time, u, v);
        }

        for (int time = -1; time < edge_count; time++) {
            vector<vector<int>> active(count, vector<int>(count, 0));
            for (int edge_index = 0; edge_index <= time; edge_index++) {
                auto [u, v] = raw_edges[edge_index];
                active[u][v]++;
                if (u != v) active[v][u]++;
            }
            vector<int> label = test_component_labels(active);
            vector<int> sizes = test_component_sizes(label);
            for (int x = 0; x < count; x++) {
                assert(uf.size(time, x) == sizes[x]);
                for (int y = 0; y < count; y++) assert(uf.same(time, x, y) == (label[x] == label[y]));
            }
        }

        vector<PartiallyPersistentDsuEarliestConnectionSolver::PairQuery> pair_queries;
        vector<optional<int>> expected_earliest;
        for (int query = 0; query < 30; query++) {
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);
            pair_queries.push_back({x, y});
            if (x == y) {
                expected_earliest.push_back(-1);
                continue;
            }
            optional<int> earliest;
            vector<vector<int>> active(count, vector<int>(count, 0));
            for (int time = 0; time < edge_count; time++) {
                auto [u, v] = raw_edges[time];
                active[u][v]++;
                if (u != v) active[v][u]++;
                vector<int> label = test_component_labels(active);
                if (label[x] == label[y]) {
                    earliest = time;
                    break;
                }
            }
            expected_earliest.push_back(earliest);
            assert(uf.first_same_time(x, y) == earliest);
        }

        vector<PartiallyPersistentDsuEarliestConnectionSolver::Edge> earliest_edges;
        for (const auto& edge : edges) earliest_edges.push_back({edge.time, edge.u, edge.v});
        assert(PartiallyPersistentDsuEarliestConnectionSolver::solve(count, earliest_edges, pair_queries) == expected_earliest);

        vector<PartiallyPersistentDsuHistoricalQuerySolver::Query> historical_queries;
        vector<PartiallyPersistentDsuHistoricalQuerySolver::Answer> expected_history;
        for (int query = 0; query < 30; query++) {
            int time = edge_count == 0 ? -1 : test_rand_int(random_engine, -1, edge_count - 1);
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);
            vector<vector<int>> active(count, vector<int>(count, 0));
            for (int edge_index = 0; edge_index <= time; edge_index++) {
                auto [u, v] = raw_edges[edge_index];
                active[u][v]++;
                if (u != v) active[v][u]++;
            }
            vector<int> label = test_component_labels(active);
            vector<int> sizes = test_component_sizes(label);
            if (test_rand_int(random_engine, 2) == 0) {
                historical_queries.push_back({PartiallyPersistentDsuHistoricalQuerySolver::QUERY_SAME, time, x, y});
                expected_history.push_back({PartiallyPersistentDsuHistoricalQuerySolver::QUERY_SAME, label[x] == label[y], 0});
            } else {
                historical_queries.push_back({PartiallyPersistentDsuHistoricalQuerySolver::QUERY_SIZE, time, x, 0});
                expected_history.push_back({PartiallyPersistentDsuHistoricalQuerySolver::QUERY_SIZE, false, sizes[x]});
            }
        }
        auto actual_history = PartiallyPersistentDsuHistoricalQuerySolver::solve(count, edges, historical_queries);
        assert(actual_history.size() == expected_history.size());
        for (int index = 0; index < static_cast<int>(actual_history.size()); index++) {
            assert(actual_history[index].type == expected_history[index].type);
            assert(actual_history[index].same == expected_history[index].same);
            assert(actual_history[index].size == expected_history[index].size);
        }
    }

    // 完全永続 DSU と versioned solver を各バージョンの愚直ラベルコピーと比較する
    for (int test_case = 0; test_case < 220; test_case++) {
        int count = test_rand_int(random_engine, 1, 10);
        int operation_count = test_rand_int(random_engine, 1, 100);
        PersistentDsu uf(count);
        vector<PersistentDsu::Version> direct_versions;
        vector<vector<int>> naive_versions;
        vector<int> initial(count);
        iota(initial.begin(), initial.end(), 0);
        vector<PersistentDsuVersionedConnectivitySolver::Operation> operations;
        vector<char> expected_answers;

        for (int index = 0; index < operation_count; index++) {
            int base_index = test_rand_int(random_engine, index + 1) - 1;
            PersistentDsu::Version base_version = base_index < 0
                ? uf.initial_version()
                : direct_versions[base_index];
            vector<int> state = base_index < 0 ? initial : naive_versions[base_index];
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);

            if (test_rand_int(random_engine, 100) < 65) {
                PersistentDsu::Version version = uf.merge(base_version, x, y);
                direct_versions.push_back(version);
                operations.push_back({PersistentDsuVersionedConnectivitySolver::MERGE, base_index, x, y});
                int group_x = state[x];
                int group_y = state[y];
                if (group_x != group_y) {
                    for (int& group : state) {
                        if (group == group_y) group = group_x;
                    }
                }
            } else {
                direct_versions.push_back(base_version);
                operations.push_back({PersistentDsuVersionedConnectivitySolver::QUERY_SAME, base_index, x, y});
                expected_answers.push_back(state[x] == state[y]);
            }
            naive_versions.push_back(state);

            PersistentDsu::Version current_version = direct_versions.back();
            for (int a = 0; a < count; a++) {
                int expected_size = 0;
                for (int b = 0; b < count; b++) {
                    bool expected_same = state[a] == state[b];
                    assert(uf.same(current_version, a, b) == expected_same);
                    if (expected_same) expected_size++;
                }
                assert(uf.size(current_version, a) == expected_size);
            }
        }
        assert(PersistentDsuVersionedConnectivitySolver::solve(count, operations) == expected_answers);
    }
}

// SuccessorDsu と区間代入・空き割当て solver の固定・ランダムテスト
static void test_successor(mt19937& random_engine) {
    for (int test_case = 0; test_case < 300; test_case++) {
        int count = test_rand_int(random_engine, 1, 80);
        SuccessorDsu successor(count);
        vector<char> available(count, true);
        for (int operation = 0; operation < 250; operation++) {
            if (test_rand_int(random_engine, 100) < 55) {
                int x = test_rand_int(random_engine, count);
                bool expected = available[x];
                assert(successor.erase(x) == expected);
                available[x] = false;
            } else {
                int x = test_rand_int(random_engine, count + 1);
                int expected = x;
                while (expected < count && !available[expected]) expected++;
                assert(successor.next(x) == expected);
            }
        }
    }

    for (int test_case = 0; test_case < 250; test_case++) {
        int count = test_rand_int(random_engine, 1, 50);
        int query_count = test_rand_int(random_engine, 0, 80);
        vector<SuccessorDsuRangeAssignmentSolver<int>::Query> queries;
        vector<int> expected(count, -1);
        for (int query = 0; query < query_count; query++) {
            int left = test_rand_int(random_engine, count + 1);
            int right = test_rand_int(random_engine, left, count);
            int value = test_rand_int(random_engine, -20, 20);
            queries.push_back({left, right, value});
            for (int position = left; position < right; position++) expected[position] = value;
        }
        assert(SuccessorDsuRangeAssignmentSolver<int>::solve(count, queries, -1) == expected);
    }

    {
        vector<int> requests = {2, 2, 0, 4, 1, 0};
        assert((SuccessorDsuNextAvailableSolver::solve(5, requests) == vector<int>{2, 3, 0, 4, 1, 5}));
        assert((SuccessorDsuCyclicAllocationSolver::solve(5, requests) == vector<int>{2, 3, 0, 4, 1, -1}));
    }

    for (int test_case = 0; test_case < 250; test_case++) {
        int count = test_rand_int(random_engine, 1, 40);
        int query_count = test_rand_int(random_engine, 1, 70);
        vector<int> requests(query_count);
        for (int& request : requests) request = test_rand_int(random_engine, count);

        vector<char> available(count, true);
        vector<int> expected_next;
        for (int request : requests) {
            int position = request;
            while (position < count && !available[position]) position++;
            expected_next.push_back(position);
            if (position < count) available[position] = false;
        }
        assert(SuccessorDsuNextAvailableSolver::solve(count, requests) == expected_next);

        fill(available.begin(), available.end(), true);
        vector<int> expected_cyclic;
        for (int request : requests) {
            int position = request;
            while (position < count && !available[position]) position++;
            if (position == count) {
                position = 0;
                while (position < count && !available[position]) position++;
            }
            if (position == count) {
                expected_cyclic.push_back(-1);
            } else {
                expected_cyclic.push_back(position);
                available[position] = false;
            }
        }
        assert(SuccessorDsuCyclicAllocationSolver::solve(count, requests) == expected_cyclic);
    }
}

// IntervalUnionDsu と query solver のランダムテスト
static void test_interval_union(mt19937& random_engine) {
    for (int test_case = 0; test_case < 300; test_case++) {
        int count = test_rand_int(random_engine, 1, 60);
        IntervalUnionDsu uf(count);
        vector<int> group(count);
        iota(group.begin(), group.end(), 0);
        vector<IntervalUnionDsuQuerySolver::Operation> operations;
        vector<IntervalUnionDsuQuerySolver::Answer> expected_answers;

        auto naive_merge = [&](int x, int y) {
            int group_x = group[x];
            int group_y = group[y];
            if (group_x == group_y) return;
            for (int& value : group) {
                if (value == group_y) value = group_x;
            }
        };

        for (int operation = 0; operation < 180; operation++) {
            int type = test_rand_int(random_engine, 4);
            if (type == 0) {
                int x = test_rand_int(random_engine, count);
                int y = test_rand_int(random_engine, count);
                uf.merge(x, y);
                naive_merge(x, y);
                operations.push_back({IntervalUnionDsuQuerySolver::MERGE, x, y});
            } else if (type == 1) {
                int left = test_rand_int(random_engine, count + 1);
                int right = test_rand_int(random_engine, left, count);
                uf.merge_interval(left, right);
                for (int position = left + 1; position < right; position++) naive_merge(left, position);
                operations.push_back({IntervalUnionDsuQuerySolver::MERGE_INTERVAL, left, right});
            } else if (type == 2) {
                int x = test_rand_int(random_engine, count);
                int y = test_rand_int(random_engine, count);
                bool expected = group[x] == group[y];
                assert(uf.same(x, y) == expected);
                operations.push_back({IntervalUnionDsuQuerySolver::QUERY_SAME, x, y});
                expected_answers.push_back({IntervalUnionDsuQuerySolver::QUERY_SAME, expected});
            } else {
                int x = test_rand_int(random_engine, count);
                int expected = 0;
                for (int value : group) {
                    if (value == group[x]) expected++;
                }
                assert(uf.size(x) == expected);
                operations.push_back({IntervalUnionDsuQuerySolver::QUERY_SIZE, x, 0});
                expected_answers.push_back({IntervalUnionDsuQuerySolver::QUERY_SIZE, expected});
            }
        }

        auto actual_answers = IntervalUnionDsuQuerySolver::solve(count, operations);
        assert(actual_answers.size() == expected_answers.size());
        for (int index = 0; index < static_cast<int>(actual_answers.size()); index++) {
            assert(actual_answers[index].type == expected_answers[index].type);
            assert(actual_answers[index].value == expected_answers[index].value);
        }
    }
}

// MovableDsu と Almost Union-Find solver の固定・ランダムテスト
static void test_movable(mt19937& random_engine) {
    {
        vector<long long> values = {1, 2, 3, 4, 5};
        MovableDsu uf(values, 5);
        uf.merge(0, 1);
        uf.merge(2, 3);
        assert(uf.size(0) == 2 && uf.sum(0) == 3);
        uf.move(1, 2);
        assert(uf.size(0) == 1 && uf.sum(0) == 1);
        assert(uf.size(1) == 3 && uf.sum(1) == 9);
        assert(uf.same(1, 3) && !uf.same(0, 1));
    }

    for (int test_case = 0; test_case < 300; test_case++) {
        int count = test_rand_int(random_engine, 1, 25);
        int operation_count = test_rand_int(random_engine, 1, 180);
        vector<long long> values(count);
        for (long long& value : values) value = test_rand_int(random_engine, -20, 20);
        vector<int> group(count);
        iota(group.begin(), group.end(), 0);
        vector<MovableDsuAlmostUnionFindSolver::Operation> operations;
        vector<MovableDsuAlmostUnionFindSolver::Answer> expected_answers;
        int move_count = 0;
        for (int operation = 0; operation < operation_count; operation++) {
            if (test_rand_int(random_engine, 3) == 1) move_count++;
        }
        MovableDsu uf(values, move_count + operation_count);

        for (int operation = 0; operation < operation_count; operation++) {
            int type = test_rand_int(random_engine, 3);
            int x = test_rand_int(random_engine, count);
            int y = test_rand_int(random_engine, count);
            if (type == 0) {
                operations.push_back({MovableDsuAlmostUnionFindSolver::MERGE, x, y});
                bool expected = group[x] != group[y];
                assert(uf.merge(x, y) == expected);
                if (expected) {
                    int source = group[y];
                    int destination = group[x];
                    for (int& value : group) {
                        if (value == source) value = destination;
                    }
                }
            } else if (type == 1) {
                operations.push_back({MovableDsuAlmostUnionFindSolver::MOVE, x, y});
                bool expected = group[x] != group[y];
                assert(uf.move(x, y) == expected);
                if (expected) group[x] = group[y];
            } else {
                operations.push_back({MovableDsuAlmostUnionFindSolver::QUERY, x, 0});
                int expected_size = 0;
                long long expected_sum = 0;
                for (int vertex = 0; vertex < count; vertex++) {
                    if (group[vertex] == group[x]) {
                        expected_size++;
                        expected_sum += values[vertex];
                    }
                }
                assert(uf.size(x) == expected_size);
                assert(uf.sum(x) == expected_sum);
                expected_answers.push_back({expected_size, expected_sum});
            }

            for (int a = 0; a < count; a++) {
                int expected_size = 0;
                long long expected_sum = 0;
                for (int b = 0; b < count; b++) {
                    assert(uf.same(a, b) == (group[a] == group[b]));
                    if (group[a] == group[b]) {
                        expected_size++;
                        expected_sum += values[b];
                    }
                }
                assert(uf.size(a) == expected_size);
                assert(uf.sum(a) == expected_sum);
            }
        }

        auto actual_answers = MovableDsuAlmostUnionFindSolver::solve(values, operations);
        assert(actual_answers.size() == expected_answers.size());
        for (int index = 0; index < static_cast<int>(actual_answers.size()); index++) {
            assert(actual_answers[index].size == expected_answers[index].size);
            assert(actual_answers[index].sum == expected_answers[index].sum);
        }
    }
}

struct TestBridgeAnalysis {
    int bridge_count;
    vector<int> connected_component;
    vector<int> two_edge_component;
};

// edge list から橋を各辺除去で愚直判定し、連結成分と二辺連結成分を求める
static TestBridgeAnalysis test_analyze_bridges(
    int count,
    const vector<pair<int, int>>& edges
) {
    auto labels_without = [&](int skipped_edge, const vector<char>* skip_flags) {
        vector<vector<int>> active(count, vector<int>(count, 0));
        for (int edge_index = 0; edge_index < static_cast<int>(edges.size()); edge_index++) {
            if (edge_index == skipped_edge) continue;
            if (skip_flags != nullptr && (*skip_flags)[edge_index]) continue;
            auto [u, v] = edges[edge_index];
            active[u][v]++;
            if (u != v) active[v][u]++;
        }
        return test_component_labels(active);
    };

    vector<int> base = labels_without(-1, nullptr);
    set<int> base_components(base.begin(), base.end());
    vector<char> bridge(edges.size(), false);
    int bridge_count = 0;
    for (int edge_index = 0; edge_index < static_cast<int>(edges.size()); edge_index++) {
        vector<int> removed = labels_without(edge_index, nullptr);
        set<int> removed_components(removed.begin(), removed.end());
        if (removed_components.size() > base_components.size()) {
            bridge[edge_index] = true;
            bridge_count++;
        }
    }
    vector<int> two_edge = labels_without(-1, &bridge);
    return {bridge_count, move(base), move(two_edge)};
}

// IncrementalBridgeConnectivity と2種類の solver を愚直な各辺除去判定と比較する
static void test_incremental_bridges(mt19937& random_engine) {
    {
        IncrementalBridgeConnectivity graph(4);
        graph.add_edge(0, 1);
        graph.add_edge(1, 2);
        assert(graph.bridge_count() == 2);
        graph.add_edge(2, 0);
        assert(graph.bridge_count() == 0);
        assert(graph.same_two_edge_component(0, 2));
        graph.add_edge(2, 3);
        assert(graph.bridge_count() == 1);
        assert(graph.same_connected_component(0, 3));
        assert(!graph.same_two_edge_component(0, 3));
        graph.add_edge(2, 3);
        assert(graph.bridge_count() == 0);
        assert(graph.same_two_edge_component(0, 3));
    }

    for (int test_case = 0; test_case < 450; test_case++) {
        int count = test_rand_int(random_engine, 1, 9);
        int edge_count = test_rand_int(random_engine, 0, 45);
        IncrementalBridgeConnectivity graph(count);
        vector<pair<int, int>> edges;
        vector<IncrementalBridgeCountSolver::Edge> solver_edges;
        vector<int> expected_bridge_counts;

        for (int edge_index = 0; edge_index < edge_count; edge_index++) {
            int u = test_rand_int(random_engine, count);
            int v = test_rand_int(random_engine, count);
            edges.push_back({u, v});
            solver_edges.push_back({u, v});
            graph.add_edge(u, v);
            TestBridgeAnalysis analysis = test_analyze_bridges(count, edges);
            expected_bridge_counts.push_back(analysis.bridge_count);
            assert(graph.bridge_count() == analysis.bridge_count);
            for (int x = 0; x < count; x++) {
                for (int y = 0; y < count; y++) {
                    assert(graph.same_connected_component(x, y) ==
                           (analysis.connected_component[x] == analysis.connected_component[y]));
                    assert(graph.same_two_edge_component(x, y) ==
                           (analysis.two_edge_component[x] == analysis.two_edge_component[y]));
                }
            }
        }
        assert(IncrementalBridgeCountSolver::solve(count, solver_edges) == expected_bridge_counts);
    }

    for (int test_case = 0; test_case < 250; test_case++) {
        int count = test_rand_int(random_engine, 1, 8);
        int operation_count = test_rand_int(random_engine, 1, 90);
        vector<pair<int, int>> edges;
        vector<IncrementalTwoEdgeConnectivitySolver::Operation> operations;
        vector<IncrementalTwoEdgeConnectivitySolver::Answer> expected;

        for (int operation = 0; operation < operation_count; operation++) {
            int u = test_rand_int(random_engine, count);
            int v = test_rand_int(random_engine, count);
            int choice = test_rand_int(random_engine, 100);
            if (choice < 55) {
                operations.push_back({IncrementalTwoEdgeConnectivitySolver::ADD_EDGE, u, v});
                edges.push_back({u, v});
            } else {
                TestBridgeAnalysis analysis = test_analyze_bridges(count, edges);
                if (choice < 78) {
                    operations.push_back({IncrementalTwoEdgeConnectivitySolver::QUERY_TWO_EDGE_CONNECTED, u, v});
                    expected.push_back({
                        IncrementalTwoEdgeConnectivitySolver::QUERY_TWO_EDGE_CONNECTED,
                        analysis.two_edge_component[u] == analysis.two_edge_component[v],
                    });
                } else {
                    operations.push_back({IncrementalTwoEdgeConnectivitySolver::QUERY_CONNECTED, u, v});
                    expected.push_back({
                        IncrementalTwoEdgeConnectivitySolver::QUERY_CONNECTED,
                        analysis.connected_component[u] == analysis.connected_component[v],
                    });
                }
            }
        }

        auto actual = IncrementalTwoEdgeConnectivitySolver::solve(count, operations);
        assert(actual.size() == expected.size());
        for (int index = 0; index < static_cast<int>(actual.size()); index++) {
            assert(actual[index].type == expected[index].type);
            assert(actual[index].value == expected[index].value);
        }
    }
}

int main() {
    mt19937 random_engine(0x5A17C3D9U);

    test_potential_and_equality(random_engine);
    cout << "[passed] potential DSU / difference equality systems: fixed + 400 random cases\n";

    test_mod_potential_and_equality(random_engine);
    cout << "[passed] modular potential DSU / modular equality systems: fixed + 300 random cases\n";

    test_signed_equality(random_engine);
    cout << "[passed] signed sum/difference equality systems: fixed + 350 random cases\n";

    test_component_add(random_engine);
    cout << "[passed] component-add DSU: fixed + 400 random cases\n";

    test_rollback_and_dynamic_graph(random_engine);
    cout << "[passed] rollback DSU / dynamic graph statistics / version tree: 720 random cases\n";

    test_rollback_value(random_engine);
    cout << "[passed] rollback component sums: fixed + 180 random cases\n";

    test_rollback_parity(random_engine);
    cout << "[passed] rollback parity / dynamic bipartiteness / XOR equalities: fixed + 360 random cases\n";

    test_rollback_potential(random_engine);
    cout << "[passed] rollback weighted DSU / dynamic difference equalities: fixed + 180 random cases\n";

    test_persistent_variants(random_engine);
    cout << "[passed] partially persistent / fully persistent DSU: 440 random cases\n";

    test_successor(random_engine);
    cout << "[passed] successor DSU / range assignment / allocation: fixed + 800 random cases\n";

    test_interval_union(random_engine);
    cout << "[passed] interval union DSU: 300 random cases\n";

    test_movable(random_engine);
    cout << "[passed] movable DSU / Almost Union-Find: fixed + 300 random cases\n";

    test_incremental_bridges(random_engine);
    cout << "[passed] incremental bridges / two-edge connectivity: fixed + 700 random cases\n";

    cout << "All tests passed\n";
    return 0;
}

#endif
