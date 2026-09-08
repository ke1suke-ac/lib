#pragma once
#include "template.hpp"

struct UnionFind {
public:
    UnionFind() : _n(0), _num_groups(0) {}
    UnionFind(int n) : _n(n), _num_groups(n), parent_or_size(n, -1) {}

    int add_node() {
        parent_or_size.push_back(-1);
        _n++;
        _num_groups++;
        return _n - 1;
    }
    int merge(int a, int b) {
        assert(0 <= a && a < _n);
        assert(0 <= b && b < _n);
        int x = leader(a), y = leader(b);
        if (x == y) return x;
        if (-parent_or_size[x] < -parent_or_size[y]) swap(x, y);
        parent_or_size[x] += parent_or_size[y];
        parent_or_size[y] = x;
        _num_groups--;
        return x;
    }
    bool same(int a, int b) {
        assert(0 <= a && a < _n);
        assert(0 <= b && b < _n);
        return leader(a) == leader(b);
    }
    int leader(int a) {
        assert(0 <= a && a < _n);
        if (parent_or_size[a] < 0) return a;
        return parent_or_size[a] = leader(parent_or_size[a]);
    }
    int group_size(int a) {
        assert(0 <= a && a < _n);
        return -parent_or_size[leader(a)];
    }
    vector<vector<int>> groups() {
        vector<int> leader_buf(_n), group_size(_n);
        for (int i = 0; i < _n; i++) {
            leader_buf[i] = leader(i);
            group_size[leader_buf[i]]++;
        }
        vector<vector<int>> result(_n);
        for (int i = 0; i < _n; i++) {
            result[i].reserve(group_size[i]);
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

    int _n;
    int _num_groups;
    vector<int> parent_or_size; // leaderの場合は -グループサイズ, それ以外は親の添字
};
