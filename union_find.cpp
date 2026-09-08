#include "union_find.hpp"

int main() {
    // Test 1: Initialization test
    cout << "Test 1: Initialization test" << endl;
    UnionFind uf(5);
    assert(uf.size() == 5);
    assert(uf.num_groups() == 5);
    vector<int> initial_leaders = uf.leaders();
    sort(initial_leaders.begin(), initial_leaders.end());
    vector<int> expected_leaders = {0, 1, 2, 3, 4};
    assert(initial_leaders == expected_leaders);
    cout << "Initialization test passed" << endl << endl;

    // Test 2: merge, same, group_size test
    cout << "Test 2: merge, same, group_size test" << endl;
    uf.merge(0, 1); // 0と1を統合
    assert(uf.same(0, 1));
    // どちらかの代表元からサイズを取得すればOK
    int comp_size = uf.group_size(0);
    assert(comp_size == 2);
    assert(uf.num_groups() == 4);
    
    uf.merge(1, 2); // (0,1)のグループと2を統合
    assert(uf.same(0, 2));
    comp_size = uf.group_size(0);
    assert(comp_size == 3);
    assert(uf.num_groups() == 3);

    // 同じグループのノードを再統合してもグループ数は変化しない
    int leader_before = uf.leader(0);
    uf.merge(0, 2);
    assert(uf.leader(0) == leader_before);
    assert(uf.num_groups() == 3);
    cout << "merge, same, group_size test passed" << endl << endl;

    // Test 3: groups and leaders test
    cout << "Test 3: groups and leaders test" << endl;
    vector<vector<int>> group_list = uf.groups();
    cout << "Groups:" << endl;
    for (auto &group : group_list) {
        cout << "{ ";
        for (int node : group) {
            cout << node << " ";
        }
        cout << "}" << endl;
    }
    vector<int> leaders_list = uf.leaders();
    cout << "Leaders: { ";
    for (int leader : leaders_list) {
        cout << leader << " ";
    }
    cout << "}" << endl;
    assert(uf.num_groups() == (int)leaders_list.size());
    assert(uf.num_groups() == (int)group_list.size());
    cout << "groups and leaders test passed" << endl << endl;

    // Test 4: add_node test
    cout << "Test 4: add_node test" << endl;
    int new_node = uf.add_node();
    // 元々のノードは5個なので新たなノードのインデックスは5になる
    assert(new_node == 5);
    assert(uf.size() == 6);
    assert(uf.num_groups() == 4); // 新しいノードが独立グループとして追加されたのでグループ数が増加

    // 新たに追加したノードと既存のグループを統合
    uf.merge(new_node, 0);
    assert(uf.same(new_node, 0));
    assert(uf.num_groups() == 3);
    cout << "add_node test passed" << endl << endl;

    // 全体のノード数とグループ数の確認
    cout << "Total number of nodes: " << uf.size() << endl;
    cout << "Total number of groups: " << uf.num_groups() << endl;

    cout << "All tests passed" << endl;
    return 0;
}
