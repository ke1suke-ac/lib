#include "union_find_rollback.hpp"

int main(){
    cout << "Rollback DSU Test Suite" << endl << endl;
    
    // Test 1: Initialization and add_node rollback
    {
        cout << "Test 1: Initialization and add_node rollback" << endl;
        UnionFindRollback rdsu(3);
        assert(rdsu.size() == 3);
        int snap = rdsu.history_size();
        int new_node = rdsu.add_node();
        assert(new_node == 3);
        assert(rdsu.size() == 4);
        assert(rdsu.num_groups() == 4);
        cout << "After add_node: size = " << rdsu.size() 
             << ", groups = " << rdsu.num_groups() << endl;
        rdsu.rollback(snap);
        assert(rdsu.size() == 3);
        assert(rdsu.num_groups() == 3);
        cout << "After rollback: size = " << rdsu.size() 
             << ", groups = " << rdsu.num_groups() << endl;
        cout << "Test 1 passed" << endl << endl;
    }
    
    // Test 2: Merge operations and rollback of merge operations
    {
        cout << "Test 2: Merge and rollback merge operations" << endl;
        UnionFindRollback rdsu(5);
        int snap = rdsu.history_size();
        rdsu.merge(0, 1); // Merge group {0} and {1}
        assert(rdsu.same(0, 1));
        assert(rdsu.num_groups() == 4);
        int snap2 = rdsu.history_size();
        rdsu.merge(1, 2); // Merge group {0,1} and {2}
        assert(rdsu.same(0, 2));
        assert(rdsu.group_size(0) == 3);
        assert(rdsu.num_groups() == 3);
        cout << "Before rollback: groups = " << rdsu.num_groups() 
             << ", group size of 0 = " << rdsu.group_size(0) << endl;
        rdsu.rollback(snap2);
        assert(!rdsu.same(1, 2)); // rollback により merge(1,2) が取り消された
        assert(rdsu.group_size(0) == 2);
        assert(rdsu.num_groups() == 4);
        rdsu.rollback(snap);
        assert(!rdsu.same(0, 1));
        assert(rdsu.num_groups() == 5);
        cout << "After rollback to initial snapshot: groups = " << rdsu.num_groups() << endl;
        cout << "Test 2 passed" << endl << endl;
    }
    
    // Test 3: Mixed operations (add_node and merge) with partial rollback
    {
        cout << "Test 3: Mixed operations with partial rollback" << endl;
        UnionFindRollback rdsu(4);
        rdsu.merge(0, 1);
        int snap = rdsu.history_size();
        rdsu.merge(2, 3);
        rdsu.add_node(); // new node with index 4
        rdsu.merge(0, 2); // Merge groups {0,1} and {2,3}
        assert(rdsu.num_groups() == 2); // groups: {0,1,2,3} and {4}
        cout << "Before rollback: size = " << rdsu.size() 
             << ", groups = " << rdsu.num_groups() << endl;
        rdsu.rollback(snap);
        // rollback により、snap 以降の操作が取り消されるので、state は merge(0,1) のみの状態になる
        assert(rdsu.size() == 4); // add_node, merge(2,3), merge(0,2) が取り消される
        assert(rdsu.num_groups() == 3); // groups: {0,1}, {2}, {3}
        cout << "After rollback to snapshot: size = " << rdsu.size() 
             << ", groups = " << rdsu.num_groups() << endl;
        cout << "Test 3 passed" << endl << endl;
    }
    
    // Test 4: Multiple consecutive operations and complete rollback
    {
        cout << "Test 4: Multiple operations and complete rollback" << endl;
        UnionFindRollback rdsu;
        // Add 10 nodes
        for (int i = 0; i < 10; i++) {
            rdsu.add_node();
        }
        // 現在のサイズは 10 であるはず
        assert(rdsu.size() == 10);
        int initial_snap = rdsu.history_size();
        // 複数の merge を実施
        rdsu.merge(0, 1);
        rdsu.merge(2, 3);
        rdsu.merge(4, 5);
        rdsu.merge(6, 7);
        rdsu.merge(8, 9);
        rdsu.merge(0, 2);
        rdsu.merge(4, 6);
        rdsu.merge(0, 4);
        assert(rdsu.num_groups() < 10);
        cout << "After merges: size = " << rdsu.size() 
             << ", groups = " << rdsu.num_groups() << endl;
        // complete rollback: 初期状態に戻す
        rdsu.rollback(initial_snap);
        assert(rdsu.size() == 10);
        assert(rdsu.num_groups() == 10);
        cout << "After complete rollback: size = " << rdsu.size() 
             << ", groups = " << rdsu.num_groups() << endl;
        cout << "Test 4 passed" << endl << endl;
    }
    
    // Test 5: Redundant merge operations (同じ集合への merge は履歴に記録されない)
    {
        cout << "Test 5: Redundant merge operations" << endl;
        UnionFindRollback rdsu(5);
        int initial_history = rdsu.history_size();
        rdsu.merge(0, 1);
        int history_after_first_merge = rdsu.history_size();
        // 既に統合済みのノード同士の merge
        rdsu.merge(1, 0);
        int history_after_second_merge = rdsu.history_size();
        // redundant merge の場合、履歴は追加されない
        assert(history_after_first_merge == history_after_second_merge);
        rdsu.rollback(initial_history);
        assert(rdsu.num_groups() == 5);
        cout << "Test 5 passed" << endl << endl;
    }
    
    // Test 6: Rollback with snapshot equal to current history (何も変わらないこと)
    {
        cout << "Test 6: Rollback with snapshot equal to current history" << endl;
        UnionFindRollback rdsu(4);
        int snap = rdsu.history_size();
        rdsu.rollback(snap); // 何も操作していないので状態は変わらず
        assert(rdsu.size() == 4);
        assert(rdsu.num_groups() == 4);
        rdsu.merge(0, 1);
        int snap2 = rdsu.history_size();
        rdsu.rollback(snap2); // 現在の履歴数と同じ snapshot で rollback → 何も変わらない
        assert(rdsu.num_groups() == 3);
        cout << "Test 6 passed" << endl << endl;
    }
    
    // Test 7: Complex interleaved operations and multi-step rollback
    {
        cout << "Test 7: Complex interleaved operations and multi-step rollback" << endl;
        // 初期 DSU は 6 ノード
        UnionFindRollback rdsu(6);
        rdsu.merge(0, 1);   // groups: {0,1}, {2}, {3}, {4}, {5} → 5 groups
        // int snapA = rdsu.history_size();
        rdsu.merge(2, 3);   // groups: {0,1}, {2,3}, {4}, {5} → 4 groups
        int snapB = rdsu.history_size();
        rdsu.merge(0, 2);   // merge {0,1} and {2,3} → groups: {0,1,2,3}, {4}, {5} → 3 groups
        rdsu.merge(4, 5);   // merge {4} and {5} → groups: {0,1,2,3}, {4,5} → 2 groups
        rdsu.merge(0, 4);   // merge {0,1,2,3} and {4,5} → groups: {0,1,2,3,4,5} → 1 group
        assert(rdsu.num_groups() == 1);
        // rollback して snapB の状態に戻す
        rdsu.rollback(snapB);
        // expected: snapB 時点の状態は
        // DSU size = 6, groups: {0,1}, {2,3}, {4}, {5} → グループ数は 4
        assert(rdsu.size() == 6);
        assert(rdsu.num_groups() == 4);
        // その後、新たな操作を適用
        rdsu.merge(4, 5); // groups: {0,1}, {2,3}, {4,5} → 3 groups
        rdsu.merge(0, 2); // merge {0,1} and {2,3} → groups: {0,1,2,3}, {4,5} → 2 groups
        assert(rdsu.num_groups() == 2);
        cout << "Test 7 passed" << endl << endl;
    }
    
    // Test 8: Complete rollback and then new operations
    {
        cout << "Test 8: Complete rollback and then new operations" << endl;
        UnionFindRollback rdsu(5);
        rdsu.merge(0, 1);
        rdsu.merge(2, 3);
        rdsu.add_node();
        // 現在の状態から完全に rollback
        rdsu.rollback(0);
        assert(rdsu.size() == 5);
        assert(rdsu.num_groups() == 5);
        // 新たな操作
        rdsu.merge(1, 2);
        rdsu.add_node(); // new node: index 5, size becomes 6, new group added
        // 期待: groups → {0}, {1,2}, {3}, {4}, {5} なのでグループ数は 5
        assert(rdsu.size() == 6);
        assert(rdsu.num_groups() == 5);
        cout << "Test 8 passed" << endl << endl;
    }
    
    // Test 9: Rollback on DSU with no operations
    {
        cout << "Test 9: Rollback on DSU with no operations" << endl;
        UnionFindRollback rdsu(3);
        int snap = rdsu.history_size(); // should be 0
        rdsu.rollback(snap); // does nothing
        assert(rdsu.size() == 3);
        assert(rdsu.num_groups() == 3);
        cout << "Test 9 passed" << endl << endl;
    }
    
    // Test 10: Multiple redundant merges and rollback
    {
        cout << "Test 10: Multiple redundant merges and rollback" << endl;
        UnionFindRollback rdsu(4);
        rdsu.merge(0, 1);  // groups: {0,1}, {2}, {3} → 3 groups
        rdsu.merge(0, 1);  // redundant, no change
        rdsu.merge(1, 0);  // redundant, no change
        int snap = rdsu.history_size();
        rdsu.merge(2, 3);  // valid merge → groups: {0,1}, {2,3} → 2 groups
        assert(rdsu.num_groups() == 2);
        rdsu.rollback(snap);
        // rollback により merge(2,3) が取り消され、元の状態に戻る → groups: {0,1}, {2}, {3} → 3 groups
        assert(rdsu.num_groups() == 3);
        cout << "Test 10 passed" << endl << endl;
    }
    
    // Test 11: Nested rollback scenario
    {
        cout << "Test 11: Nested rollback scenario" << endl;
        UnionFindRollback rdsu(7);
        rdsu.merge(0, 1);   // groups: {0,1}, {2}, {3}, {4}, {5}, {6} → 6 groups
        int snap1 = rdsu.history_size();
        rdsu.merge(2, 3);   // groups: {0,1}, {2,3}, {4}, {5}, {6} → 5 groups
        // int snap2 = rdsu.history_size();
        rdsu.add_node();    // new node: index 7, DSU size becomes 8, groups: {0,1}, {2,3}, {4}, {5}, {6}, {7} → 6 groups
        int snap3 = rdsu.history_size();
        rdsu.merge(4, 5);   // groups: {0,1}, {2,3}, {4,5}, {6}, {7} → 5 groups
        rdsu.merge(0, 2);   // merge {0,1} and {2,3} → groups: {0,1,2,3}, {4,5}, {6}, {7} → 4 groups
        rdsu.merge(6, 7);   // merge {6} and {7} → groups: {0,1,2,3}, {4,5}, {6,7} → 3 groups
        assert(rdsu.num_groups() == 3);
        rdsu.rollback(snap3); // rollback merge(4,5), merge(0,2), merge(6,7)
        // snap3 時点の状態: DSU size = 8, groups: {0,1}, {2,3}, {4}, {5}, {6}, {7} → 6 groups
        assert(rdsu.num_groups() == 6);
        rdsu.rollback(snap1); // rollback merge(2,3) and add_node → state becomes as after merge(0,1)
        // 状態: DSU size = 7, groups: {0,1}, {2}, {3}, {4}, {5}, {6} → 6 groups
        assert(rdsu.num_groups() == 6);
        rdsu.rollback(0); // すべての操作を取り消す → 初期状態: 7 ノード, 7 groups
        assert(rdsu.size() == 7);
        assert(rdsu.num_groups() == 7);
        cout << "Test 11 passed" << endl << endl;
    }
    
    // Test 12: Clear history method
    {
        cout << "Test 12: Clear history method" << endl;
        UnionFindRollback rdsu(5);
        rdsu.merge(0, 1);  // groups become 4
        rdsu.add_node();   // size becomes 6, groups become 5
        int hsize = rdsu.history_size();
        assert(hsize > 0);
        rdsu.clear_history();
        assert(rdsu.history_size() == 0);
        // その後、さらに操作を行い、rollback(0) を実行しても、
        // clear_history 直後の状態からの操作のみが rollback される
        rdsu.merge(2, 3);  // merge(2,3) reduces groups from 5 to 4, size remains 6
        // int hsize2 = rdsu.history_size();
        rdsu.rollback(0);
        // clear_history 直後の状態は、merge(0,1) と add_node の操作が固定されている状態
        // その状態は、DSU size = 6, groups = 5
        assert(rdsu.size() == 6);
        assert(rdsu.num_groups() == 5);
        cout << "Test 12 passed" << endl << endl;
    }
    
    // Test 13: Clear history and then new operations
    {
        cout << "Test 13: Clear history and then new operations" << endl;
        UnionFindRollback rdsu(4);
        rdsu.merge(0, 1);
        // int snap = rdsu.history_size();
        rdsu.clear_history();
        assert(rdsu.history_size() == 0);
        rdsu.merge(2, 3);
        // rollback(0) で、clear_history 後の操作（merge(2,3)）が取り消される
        rdsu.rollback(0);
        // clear_history 直後の状態は、DSU size = 4, groups: {0,1}, {2}, {3} → 3 groups
        assert(rdsu.size() == 4);
        assert(rdsu.num_groups() == 3);
        cout << "Test 13 passed" << endl << endl;
    }
    
    cout << "All rollback DSU tests passed" << endl;
    return 0;
}
