/***************************************************************************
 * QuadTree ― 軸平行長方形（AABB: Axis-Aligned Bounding Box）を扱う四分木
 * =========================================================================
 * ■ 全体的な利用方法
 * -------------------------------------------------------------------------
 *   1. QuadTree qt(min_x, min_y, max_x, max_y [, capacity]);
 *        └ ルート領域を (min_x, min_y)-(max_x, max_y) で初期化。
 *          capacity は 1 ノードあたり格納できる長方形数の閾値
 *          （超えると自動的に分割）。省略時は 4。
 *
 *   2. qt.insert(id, x1, y1, x2, y2);
 *        └ 長方形 id を挿入。座標は左上 (x1, y1) 右下 (x2, y2)
 *          で軸平行長方形を指定する。
 *
 *   3. std::vector<int> ids = qt.query(qx1, qy1, qx2, qy2);
 *        └ クエリ領域と交差する長方形 id の一覧を取得。
 *
 *   4. qt.remove(id);
 *        └ id で指定した長方形を削除。存在しない場合は false。
 *
 *   5. qt.size();
 *        └ 現在木に格納されている長方形の総数を返す。
 *
 * ■ 計算量の目安
 * -------------------------------------------------------------------------
 *   挿入   : 平均 O(log N) 〜 O(1)   （領域が均等に分割される場合）
 *   削除   : 平均 O(log N) 〜 O(1)
 *   範囲検索: O(log N + k)          （k はヒット数）
 *   ※ capacity・データ分布に大きく依存するため、最悪 O(N)。
 *
 * ■ クラス／メソッドと引数の説明
 * -------------------------------------------------------------------------
 *   struct Rect
 *     int id      : ユーザ定義の一意識別子
 *     int x1,y1   : 左上座標
 *     int x2,y2   : 右下座標（x2>=x1, y2>=y1 を前提）
 *
 *   class QuadTree
 *     QuadTree(x1,y1,x2,y2,cap)
 *       x1,y1,x2,y2 : ルート領域
 *       cap         : ノード分割閾値
 *
 *     void insert(id,x1,y1,x2,y2)
 *       id          : 登録する長方形の識別子
 *       x1,y1,x2,y2 : 長方形の座標
 *
 *     bool remove(id)
 *       id          : 削除対象識別子
 *       戻り値       : 削除成功なら true
 *
 *     std::vector<int> query(qx1,qy1,qx2,qy2)
 *       qx1,qy1,qx2,qy2 : クエリ長方形座標
 *       戻り値           : 交差する id のリスト
 *
 * 2 つの矩形が少しでも領域を共有する（＝交差 or 内包 or 辺が接する）” 場合に検出
 *
 * 完全内包
 * r がクエリ矩形 (qx1,qy1)-(qx2,qy2) の内部に収まる場合、4 つの不等式はすべて false になるため !(false‖false‖false‖false) ⇒ true となり、ID が取得結果に含まれます。
 * 
 * 辺が重ならず 1 px 以上離れていれば未ヒット
 * たとえば r.x2 < qx1 が成り立つ（右端がクエリ左端より左）と矩形同士に空隙があるため除外されます。
 * 
 * 辺がぴったり接する場合もヒット
 * 条件式は < / > であって <= / >= ではないため、r.x2 == qx1 のように辺同士がちょうど接しているケースは「交差あり」とみなされます。
 * もし「接しているだけで面積が 0 のケースを除外したい」なら、比較演算子を <／> から <=／>= に変更してください。
 * 
 * したがって、query() は 辺が交差している長方形だけでなく、クエリ領域に完全に内包される長方形も（さらには辺が接するだけの長方形も）すべて返します。
 * 
 *     size_t size() const
 *       登録長方形数を返す。
 *
 * ■ 利用時のポイント・注意点
 * -------------------------------------------------------------------------
 *   ● root 域外の長方形は格納できない。挿入前に領域を十分広く取ること。
 *   ● ノード分割後も、子ノードに完全に収まらない長方形は親ノードに残る。
 *     そのため極端に大きい長方形を多数入れると性能が低下する。
 *   ● remove は O(M)（M は同ノードに存在する長方形数）で線形検索を行う。
 *     頻繁に削除する用途では capacity を小さめに設定しヒット数を抑える。
 *   ● 現在「マージ（子ノードが閾値未満になったら再統合）」は行わない。
 *     動的シーンで領域がスカスカになる場合は自前で再構築を検討する。
 ***************************************************************************/

#include <bits/stdc++.h>
using namespace std;

/*------------------------------*
 *  軸平行長方形を表す構造体
 *------------------------------*/
struct Rect {
    int id;          // 一意な識別子
    int x1, y1;      // 左上座標
    int x2, y2;      // 右下座標
};

/*==================================================================*
 *  QuadTree ― 長方形を格納・検索する四分木
 *==================================================================*/
class QuadTree {
    /*---------------------------------------------*
     *  内部ノード構造体
     *---------------------------------------------*/
    struct Node {
        // ノードがカバーする領域
        int x1, y1, x2, y2;
        // 現在このノードに直接格納されている長方形一覧
        vector<Rect> rects;
        // 子ノード (NW, NE, SW, SE) のポインタ
        array<Node*,4> children;
        // 分割済みかどうか
        bool divided;

        /* コンストラクタ ― 領域を指定して初期化 */
        Node(int _x1, int _y1, int _x2, int _y2)
            : x1(_x1), y1(_y1), x2(_x2), y2(_y2), divided(false) {
            children.fill(nullptr);
        }

        /* デストラクタ ― 子ノードを再帰的に解放 */
        ~Node() {
            for (auto c : children) if (c) delete c;
        }

        /* 長方形 r がこのノード領域に完全に収まるか判定 */
        bool contains(const Rect &r) const {
            return r.x1 >= x1 && r.x2 <= x2 && r.y1 >= y1 && r.y2 <= y2;
        }

        /* ノード領域とクエリ領域 (qx1,qy1)-(qx2,qy2) が交差するか判定 */
        bool intersects(int qx1, int qy1, int qx2, int qy2) const {
            return !(qx2 < x1 || qx1 > x2 || qy2 < y1 || qy1 > y2);
        }
    };

    Node* root;                           // ルートノード
    size_t capacity;                      // 1ノードの格納上限
    unordered_map<int, Node*> lookup;     // id → 格納ノードの逆引き

    /*---------------------------------------------*
     *  ノード分割（4 分木化）
     *---------------------------------------------*/
    void subdivide(Node* node) {
        int mx = (node->x1 + node->x2) >> 1;          // 中央 x
        int my = (node->y1 + node->y2) >> 1;          // 中央 y
        // 子ノードを NW, NE, SW, SE の順で生成
        node->children[0] = new Node(node->x1, node->y1, mx,      my);
        node->children[1] = new Node(mx + 1,  node->y1, node->x2, my);
        node->children[2] = new Node(node->x1, my + 1,  mx,      node->y2);
        node->children[3] = new Node(mx + 1,  my + 1,  node->x2, node->y2);
        node->divided = true;

        // 既存長方形を子ノードへ移動できるものは移動
        auto old = move(node->rects);
        node->rects.clear();
        for (auto &r : old) {
            bool placed = false;
            for (auto c : node->children) {
                if (c->contains(r)) {
                    c->rects.push_back(r);
                    lookup[r.id] = c;
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                node->rects.push_back(r);     // どの子にも完全には収まらない
                lookup[r.id] = node;
            }
        }
    }

    /*---------------------------------------------*
     *  内部挿入処理（再帰）
     *---------------------------------------------*/
    void _insert(Node* node, const Rect &r) {
        // 既に分割済みなら、収まる子へ再帰
        if (node->divided) {
            for (auto c : node->children) {
                if (c->contains(r)) {
                    _insert(c, r);
                    return;
                }
            }
        }
        // 収まらない／未分割の場合はこのノードに格納
        node->rects.push_back(r);
        lookup[r.id] = node;

        // 格納数が閾値を超えたら分割
        if (!node->divided && node->rects.size() > capacity) {
            subdivide(node);
        }
    }

    /*---------------------------------------------*
     *  範囲検索（再帰）
     *---------------------------------------------*/
    void _query(Node* node, int qx1, int qy1, int qx2, int qy2, vector<int> &out) const {
        if (!node->intersects(qx1, qy1, qx2, qy2)) return;   // 交差しなければ打ち切り

        // このノードに保存されている長方形をチェック
        for (const auto &r : node->rects) {
            if (!(r.x2 < qx1 || r.x1 > qx2 || r.y2 < qy1 || r.y1 > qy2))
                out.push_back(r.id);
        }
        // 子ノードを再帰的に探索
        if (node->divided) {
            for (auto c : node->children) _query(c, qx1, qy1, qx2, qy2, out);
        }
    }

public:
    /*---------------------------------------------*
     *  コンストラクタ
     *---------------------------------------------*/
    QuadTree(int x1, int y1, int x2, int y2, size_t cap = 4)
        : capacity(cap) {
        root = new Node(x1, y1, x2, y2);
    }

    /*---------------------------------------------*
     *  デストラクタ
     *---------------------------------------------*/
    ~QuadTree() { delete root; }

    /*---------------------------------------------*
     *  長方形の追加
     *---------------------------------------------*/
    void insert(int id, int x1, int y1, int x2, int y2) {
        Rect r{ id, x1, y1, x2, y2 };
        _insert(root, r);
    }

    /*---------------------------------------------*
     *  長方形の削除
     *---------------------------------------------*/
    bool remove(int id) {
        auto it = lookup.find(id);
        if (it == lookup.end()) return false;
        Node* node = it->second;
        auto &v = node->rects;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i].id == id) {
                v[i] = v.back(); v.pop_back(); // O(1) で削除
                lookup.erase(it);
                return true;
            }
        }
        return false;   // ありえないが保険
    }

    /*---------------------------------------------*
     *  範囲検索
     *---------------------------------------------*/
    vector<int> query(int qx1, int qy1, int qx2, int qy2) const {
        vector<int> res;
        _query(root, qx1, qy1, qx2, qy2, res);
        return res;
    }

    /*---------------------------------------------*
     *  格納長方形数
     *---------------------------------------------*/
    size_t size() const {
        return lookup.size();
    }
};
