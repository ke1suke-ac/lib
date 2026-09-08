#pragma once
#include "template.hpp"

struct UnionFindRollback {
    UnionFindRollback() : _n(0), _num_groups(0) {}
    UnionFindRollback(int n) : _n(n), _num_groups(n), parent_or_size(n, -1) {}

    int add_node() {
        history.push_back({-1, 0});
        parent_or_size.push_back(-1);
        _n++;
        _num_groups++;
        return _n - 1;
    }
    int merge(int a, int b) {
        assert(0 <= a && a < _n);
        assert(0 <= b && b < _n);
        a = leader(a);
        b = leader(b);
        if (a == b) return a;
        if (-parent_or_size[a] < -parent_or_size[b]) swap(a, b);
        history.push_back({b, parent_or_size[b]});
        parent_or_size[a] += parent_or_size[b];
        parent_or_size[b] = a;
        _num_groups--;
        return a; // 新しいleaderを返す
    }
    bool same(int a, int b) const {
        assert(0 <= a && a < _n);
        assert(0 <= b && b < _n);
        return leader(a) == leader(b);
    }
    int leader(int a) const {
        assert(0 <= a && a < _n);
        while (parent_or_size[a] >= 0)
            a = parent_or_size[a];
        return a;
    }
    int group_size(int a) const {
        assert(0 <= a && a < _n);
        return -parent_or_size[leader(a)];
    }
    vector<vector<int>> groups() const {
        vector<int> leader_buf(_n), group_size_vec(_n, 0);
        for (int i = 0; i < _n; i++) {
            leader_buf[i] = leader(i);
            group_size_vec[leader_buf[i]]++;
        }
        vector<vector<int>> result(_n);
        for (int i = 0; i < _n; i++) {
            result[i].reserve(group_size_vec[i]);
        }
        for (int i = 0; i < _n; i++) {
            result[leader_buf[i]].push_back(i);
        }
        result.erase(
            remove_if(result.begin(), result.end(),
                      [&](const vector<int>& v) { return v.empty(); }),
            result.end());
        return result;
    }
    vector<int> leaders() const {
        vector<int> res;
        for (int i = 0; i < _n; i++) {
            if (parent_or_size[i] < 0)
                res.push_back(i);
        }
        return res;
    }
    int num_groups() const {
        return _num_groups;
    }
    int size() const {
        return _n;
    }
    // snapshot
    int history_size() const {
        return static_cast<int>(history.size());
    }
    // rollback: 履歴の件数のスナップショット snapshot を引数として、その時点までロールバックする
    void rollback(int snapshot) {
        while (static_cast<int>(history.size()) > snapshot) {
            HistoryItem h = history.back();
            history.pop_back();
            if (h.b < 0) { // add_node の場合
                parent_or_size.pop_back();
                _n--;
                _num_groups--;
            } else { // merge の場合
                int a = parent_or_size[h.b];
                parent_or_size[a] = parent_or_size[a] - h.b_size;
                parent_or_size[h.b] = h.b_size;
                _num_groups++;
            }
        }
    }
    void clear_history() {
        history.clear();
    }
    struct HistoryItem { int b; int b_size; };
    int _n;
    int _num_groups;
    vector<int> parent_or_size; // 各 leader では -グループサイズ, それ以外は親の添字
    vector<HistoryItem> history;
};
