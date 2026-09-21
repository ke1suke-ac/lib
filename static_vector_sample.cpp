/*
AHC向け boost::container::static_vector の使用例

使い方: このファイル全体をコンパイルして実行するか、必要な例をコピーする
コンパイル: g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-expansion-to-defined static_vector_sample_v01.cpp -o sample
実行: ./sample
Boostのヘッダが必要。追加のリンクオプションは不要

・static_vector<T, CAP> のCAPはコンパイル時定数。size()は実行中に変化する
・要素領域はオブジェクト内にあり、空でもCAP要素分の領域を持つ
・初期要素数は0。v[i]を使うには、その要素を構築・追加しておく
・reserve()は不要。容量は増えないので、常にsize() <= CAPを守る
・コピー、ムーブ、swapは要素数に応じた処理が必要
・大容量のローカル変数や大量の探索状態では、合計メモリ量に注意する
・以下のintやCandidateでは要素用の動的確保はない。要素自身が持つvector等は別
*/

#include <bits/stdc++.h>
#include <boost/container/static_vector.hpp>
using namespace std;
using boost::container::static_vector;

constexpr int MAX_N = 200;

// 実行時に決まる長さの配列を作り、初期化・再利用する例: O(n)
void sample_initialization() {
    const int n = 5;
    assert(0 <= n && n <= MAX_N);

    // vector<int>(n, -1)や、array<int, MAX_N>と有効長nの組を置き換える
    static_vector<int, MAX_N> label(n, -1);
    label[2] = 7;
    assert(label.size() == (size_t)n && label[2] == 7);

    // 引数が要素数だけの場合、intの各要素は0で初期化される
    static_vector<int, MAX_N> order(n);
    assert(order[0] == 0);
    iota(order.begin(), order.end(), 0);
    assert(order.back() == 4);

    // 全要素の再初期化はfill/assign。resizeは既存の要素を書き換えない
    fill(label.begin(), label.end(), 0);
    label.assign(n, -1);
    label.resize(n + 2, 9);  // 追加した2要素だけが9になる
    assert(label.front() == -1 && label.back() == 9);

    // 波括弧は要素の列を指定する。丸括弧との違いに注意する
    static_vector<int, MAX_N> values{5, 0};  // 5と0の2要素
    assert(values.size() == 2);
}

struct Candidate {
    int vertex;
    long long delta;  // コストの変化量。小さいほど良い
};

// 近傍候補を列挙して、コスト変化量が小さい上位k件を残す例: O(m log m)
void sample_candidates() {
    constexpr int MAX_CANDIDATES = 64;
    const array<long long, 5> deltas{12, -8, 4, -3, 7};
    static_vector<Candidate, MAX_CANDIDATES> candidates;
    assert(candidates.empty());

    // 要素数が変わる候補リスト。emplace_backの引数でCandidateを構築する
    for (int vertex = 0; vertex < (int)deltas.size(); ++vertex) {
        assert(candidates.size() < candidates.capacity());
        candidates.emplace_back(vertex, deltas[vertex]);
    }

    // begin/endを使う標準アルゴリズムはvectorと同様に利用できる
    sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return tie(a.delta, a.vertex) < tie(b.delta, b.vertex);
    });
    const int k = 3;
    candidates.resize(min(candidates.size(), (size_t)k));
    assert(candidates.size() == 3 && candidates.front().vertex == 1);
    assert(candidates.back().vertex == 2);

    // 次のターンでは中身だけ消して再利用する。capacity()は変化しない
    candidates.clear();
    assert(candidates.empty() && candidates.capacity() == MAX_CANDIDATES);
}

// 巡回経路や操作列の追加・挿入・削除・区間反転の例: 中間操作はO(n)
void sample_route() {
    static_vector<int, MAX_N> route{0, 2, 4, 1};

    // 末尾の追加・削除はO(1)
    route.push_back(5);  // 0, 2, 4, 1, 5
    assert(route.back() == 5);
    route.pop_back();    // 0, 2, 4, 1

    // 位置を指定する操作もvectorと同じ。後続要素が移動する
    route.insert(route.begin() + 2, 3);  // 0, 2, 3, 4, 1
    route.erase(route.begin() + 1);      // 0, 3, 4, 1
    assert(route.size() == 4 && route[1] == 3);

    // 焼きなましの2-optなど。[l, r)を反転し、棄却時は同じ区間を再反転する
    const int l = 1, r = 4;
    reverse(route.begin() + l, route.begin() + r);  // 0, 1, 4, 3
    assert(route[1] == 1);
    reverse(route.begin() + l, route.begin() + r);  // 0, 3, 4, 1
    assert(route[1] == 3);
}

// 未使用頂点など、順序が不要なリストから1要素を取り出す: O(1)
int take_vertex(static_vector<int, MAX_N>& remaining, int index) {
    assert(0 <= index && index < (int)remaining.size());
    const int vertex = remaining[index];
    remaining[index] = remaining.back();
    remaining.pop_back();
    return vertex;
}

// 配列と有効長による管理を置き換え、順序を保たずに削除する例: 初期化O(n)
void sample_unordered_erase() {
    static_vector<int, MAX_N> remaining(5);
    iota(remaining.begin(), remaining.end(), 0);  // 0, 1, 2, 3, 4
    const int selected = take_vertex(remaining, 1);
    assert(selected == 1);
    assert(remaining.size() == 4 && remaining[1] == 4);  // 0, 4, 2, 3

    // 最後の1要素も同じ処理で削除できる
    remaining.assign(1, 9);
    const int last = take_vertex(remaining, 0);
    assert(last == 9 && remaining.empty());
}

struct State {
    static_vector<int, MAX_N> route;
    long long cost = 0;
};

// ビームサーチの子状態作成や焼きなましの最良解保存の例: 1回のコピーO(n)
void sample_state_copy() {
    State parent;
    parent.route = {0, 1, 2};
    parent.cost = 100;

    // 通常のコピーで要素も複製される。経路の要素領域は共有しない
    State child = parent;
    child.route.push_back(3);
    child.cost -= 7;  // この追加によるコスト変化が-7だった例
    assert(parent.route.size() == 3 && child.route.size() == 4);

    // 最良更新時の保存も通常の代入でよい
    State best = parent;
    if (child.cost < best.cost) best = child;
    child.route[0] = 9;
    assert(best.route[0] == 0 && best.cost == 93);

    // 多数の状態を置く外側のvectorは動的確保を使う。各Stateの経路領域は内蔵
    vector<State> beam;
    beam.reserve(2);
    beam.push_back(parent);
    beam.push_back(best);
    assert(beam[1].route.size() == 4);
}

constexpr int MAX_H = 50;
constexpr int MAX_W = 50;
constexpr int MAX_CELLS = MAX_H * MAX_W;

// 壁'#'のある長方形グリッドの距離を求める。未到達は-1: O(HW)
static_vector<int, MAX_CELLS> grid_bfs(const vector<string>& grid, int sy, int sx) {
    const int h = (int)grid.size();
    assert(0 < h && h <= MAX_H);
    const int w = (int)grid[0].size();
    assert(0 < w && w <= MAX_W);
    assert(0 <= sy && sy < h && 0 <= sx && sx < w && grid[sy][sx] != '#');

    // 距離配列は先に全要素を作る。キューは空から追加する
    static_vector<int, MAX_CELLS> dist(h * w, -1);
    static_vector<int, MAX_CELLS> que;
    const int start = sy * w + sx;
    dist[start] = 0;
    que.push_back(start);

    // 常に4要素を使う方向配列はarrayのままでよい
    constexpr array<int, 4> dy{-1, 0, 1, 0};
    constexpr array<int, 4> dx{0, 1, 0, -1};
    for (size_t head = 0; head < que.size(); ++head) {
        const int v = que[head];
        const int y = v / w, x = v % w;
        for (int d = 0; d < 4; ++d) {
            const int ny = y + dy[d], nx = x + dx[d];
            if (ny < 0 || ny >= h || nx < 0 || nx >= w || grid[ny][nx] == '#') continue;
            const int to = ny * w + nx;
            if (dist[to] != -1) continue;
            dist[to] = dist[v] + 1;
            que.push_back(to);  // 各マスは1回だけ追加されるので容量内に収まる
        }
    }
    return dist;  // 値で返せる。コピー省略が働かなければ要素の移動が必要
}

#if __INCLUDE_LEVEL__ == 0
int main() {
    sample_initialization();
    sample_candidates();
    sample_route();
    sample_unordered_erase();
    sample_state_copy();

    // 壁を避けた距離、未到達、1マス、最大サイズを確認する
    const auto dist = grid_bfs({"...", ".#.", "..."}, 0, 0);
    assert(dist[8] == 4 && dist[4] == -1);
    const auto blocked = grid_bfs({".#."}, 0, 0);
    assert(blocked[2] == -1);
    const auto single = grid_bfs({"."}, 0, 0);
    assert(single.size() == 1 && single[0] == 0);
    const auto largest = grid_bfs(vector<string>(MAX_H, string(MAX_W, '.')), 0, 0);
    assert(largest.size() == MAX_CELLS && largest.back() == MAX_H + MAX_W - 2);

    // 空から容量ちょうどまで追加し、clear後も再利用できることを確認する
    static_vector<int, MAX_N> full;
    for (int i = 0; i < MAX_N; ++i) full.push_back(i);
    assert(full.size() == full.capacity() && full.back() == MAX_N - 1);
    full.clear();
    full.push_back(42);
    assert(full.size() == 1 && full[0] == 42);

    cout << "All samples passed\n";
}
#endif
