#include <iostream>
#include <vector>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <cassert>
using namespace std;

// DSaturアルゴリズムで用いる頂点状態の構造体
struct VertexState {
    int vertex;       // 頂点番号
    int saturation;   // 現在の彩度（隣接頂点で使用済みの色の種類数）
    int degree;       // グラフ上の次数

    // DSaturでは、彩度が大きい頂点を優先し、彩度が同じなら次数が大きいものを優先する
    bool operator<(const VertexState &other) const {
        if (saturation != other.saturation)
            return saturation < other.saturation;
        return degree < other.degree;
    }
};

/// DSaturアルゴリズムによるグラフ彩色
/// @param graph グラフは隣接リスト形式。graph[u] は u と隣接する頂点のリスト。
/// @return vector<int> 各頂点の色 (未彩色は -1)
vector<int> dsaturColoring(const vector<vector<int>> &graph) {
    int n = static_cast<int>(graph.size());
    vector<int> color(n, -1);              // 各頂点の色（初期はすべて未彩色）
    vector<unordered_set<int>> usedColors(n); // 各頂点の隣接頂点で使用された色の集合

    // 優先度付きキュー（最大値が先頭に来る）
    // lazy更新で使うので、キューに同一頂点の状態が複数格納される可能性がある
    priority_queue<VertexState> pq;
    // 各頂点の現在の彩度、初期は0, 次数はgraph[u].size()
    vector<int> currentSat(n, 0);
    vector<int> degree(n, 0);
    for (int u = 0; u < n; ++u) {
        degree[u] = static_cast<int>(graph[u].size());
        currentSat[u] = 0; // 初期はどの隣接も彩色されていない
        pq.push({u, currentSat[u], degree[u]});
    }

    // DSaturのメインループ：全頂点が彩色されるまで繰り返す
    while (!pq.empty()) {
        auto state = pq.top();
        pq.pop();
        int u = state.vertex;
        // すでに彩色済みの場合はスキップ
        if (color[u] != -1)
            continue;
        // lazy更新：キューから取り出した状態が現状と異なる場合はスキップ
        if (state.saturation != currentSat[u] || state.degree != degree[u])
            continue;
        
        // uの隣接で使用されている色（usedColors[u]）から、最小の使用されていない色を選ぶ
        int assignColor = 0;
        while (usedColors[u].find(assignColor) != usedColors[u].end())
            ++assignColor;
        color[u] = assignColor;
        
        // uに色を割り当てたので、uの隣接未彩色頂点 v に対して
        // usedColors[v] に assignColor を追加（もし新たに追加されたら彩度(currentSat)を更新）
        for (int v : graph[u]) {
            if (color[v] == -1) { // vが未彩色なら
                // すでに assignColor が v の隣接で使われていなければ追加
                if (usedColors[v].insert(assignColor).second) {
                    ++currentSat[v];
                    // 新たな状態をキューにプッシュ（lazy更新）
                    pq.push({v, currentSat[v], degree[v]});
                }
            }
        }
    }
    return color;
}

//////////////////////
// 利用例（テスト用）
//////////////////////
// #ifdef UNIT_TEST_DSATUR
#include <iomanip>
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    // 例: 6頂点の無向グラフ
    // 隣接リスト形式（各辺は両方向に含む）
    // 例として、以下のグラフを考える
    // 0: 1, 2
    // 1: 0, 2, 3
    // 2: 0, 1, 3, 4
    // 3: 1, 2, 4, 5
    // 4: 2, 3, 5
    // 5: 3, 4
    vector<vector<int>> graph = {
        {1,2},
        {0,2,3},
        {0,1,3,4},
        {1,2,4,5},
        {2,3,5},
        {3,4}
    };
    
    auto colors = dsaturColoring(graph);
    cout << "Vertex : Color\n";
    for (int u = 0; u < static_cast<int>(colors.size()); ++u)
        cout << u << " : " << colors[u] << "\n";
    return 0;
}
// #endif
