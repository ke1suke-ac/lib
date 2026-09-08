#include <array>
#include <bitset>
#include <vector>
#include <iostream>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <tuple>
#include <string>
#include <functional>
#include <concepts>
#include <memory>
#include <numeric>
#include <cstring>

using namespace std;

enum ActionType{
	AddEdge,
	RemoveEdge,
	UpdateEdge,
	AddVertex,
	RemoveVertex,
	UpdateVertex,
};

static inline const char* action_type_name(ActionType t){
	switch(t){
		case AddEdge: return "AddEdge";
		case RemoveEdge: return "RemoveEdge";
		case UpdateEdge: return "UpdateEdge";
		case AddVertex: return "AddVertex";
		case RemoveVertex: return "RemoveVertex";
		case UpdateVertex: return "UpdateVertex";
	}
	return "Unknown";
}

// 操作ログ用の軽量データ型。種別と2つのIDを共通保持し、値はEまたはVのどちらかのみをunionで保持する。
// 注意：E,Vはtrivially copyableに限定し、コピー/ムーブはactive側のみを最小限コピーして高速化する。
template<class V, class E>
struct Action{
	static_assert(is_trivially_copyable_v<E>, "E must be trivially copyable.");
	static_assert(is_trivially_copyable_v<V>, "V must be trivially copyable.");

	ActionType type{RemoveEdge}; // 操作種別を保持する
	int x{0}; // 辺では始点、頂点更新では対象頂点IDを保持する
	int y{0}; // 辺では終点を保持する（それ以外では未使用）
	union{
		[[no_unique_address]] E e; // 辺の値を保持する（AddEdge/UpdateEdgeで有効）
		[[no_unique_address]] V v; // 頂点の値を保持する（AddVertex/UpdateVertexで有効）
	};

	// 既定状態（値なし）で初期化する。O(1)
	constexpr Action() noexcept : type(RemoveEdge), x(0), y(0) {}

	// 内容をコピーして生成する（active側のみコピー）。O(1) ※これがないと遅くなる
	constexpr Action(const Action& o) noexcept : type(o.type), x(o.x), y(o.y){ copy_payload_from_(o); }

	// 内容をムーブして生成する（trivial前提でコピー同等）。O(1)
	constexpr Action(Action&& o) noexcept : type(o.type), x(o.x), y(o.y){ copy_payload_from_(o); }

	// 内容をコピー代入する（active側のみコピー）。O(1)
	constexpr Action& operator=(const Action& o) noexcept{
		if(this == &o) return *this;
		type = o.type; x = o.x; y = o.y; copy_payload_from_(o);
		return *this;
	}

	// 内容をムーブ代入する（trivial前提でコピー同等）。O(1)
	constexpr Action& operator=(Action&& o) noexcept{
		if(this == &o) return *this;
		type = o.type; x = o.x; y = o.y; copy_payload_from_(o);
		return *this;
	}

	// 辺追加の操作を生成する。O(1)
	static Action make_add_edge(int fr, int to, const E& val){
		Action a;
		a.type = AddEdge;
		a.x = fr;
		a.y = to;
		a.e = val;
		return a;
	}

	// 辺削除の操作を生成する。O(1)
	static Action make_remove_edge(int fr, int to){
		Action a;
		a.type = RemoveEdge;
		a.x = fr;
		a.y = to;
		return a;
	}

	// 辺更新の操作を生成する。O(1)
	static Action make_update_edge(int fr, int to, const E& val){
		Action a;
		a.type = UpdateEdge;
		a.x = fr;
		a.y = to;
		a.e = val;
		return a;
	}

	// 頂点追加の操作を生成する。O(1)
	static Action make_add_vertex(const V& val){
		Action a;
		a.type = AddVertex;
		a.x = 0;
		a.y = 0;
		a.v = val;
		return a;
	}

	// 頂点削除の操作を生成する。O(1)
	static Action make_remove_vertex(){
		Action a;
		a.type = RemoveVertex;
		a.x = 0;
		a.y = 0;
		return a;
	}

	// 頂点更新の操作を生成する。O(1)
	static Action make_update_vertex(int v_id, const V& val){
		Action a;
		a.type = UpdateVertex;
		a.x = v_id;
		a.y = 0;
		a.v = val;
		return a;
	}

private:
	static constexpr bool has_e_(ActionType t) noexcept{ return t == AddEdge || t == UpdateEdge; }
	static constexpr bool has_v_(ActionType t) noexcept{ return t == AddVertex || t == UpdateVertex; }

	constexpr void copy_payload_from_(const Action& o) noexcept{
		if(has_e_(type)){
			e = o.e;
		}else if(has_v_(type)){
			v = o.v;
		}
	}
};

namespace dyn_detail {
	static inline uint64_t splitmix64(uint64_t x){
		x += 0x9e3779b97f4a7c15ULL;
		x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
		x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
		return x ^ (x >> 31);
	}
	template<class T>
	concept HasStdHash = requires(const T& x){
		{ std::hash<T>{}(x) } -> convertible_to<size_t>;
	};
	template<class T>
	concept HasAdlHashValue = requires(const T& x){
		{ hash_value(x) } -> convertible_to<size_t>;
	};

	template<class T>
	static inline uint64_t hash64(const T& x){
		if constexpr (HasStdHash<T>){
			return (uint64_t)std::hash<T>{}(x);
		}else if constexpr (HasAdlHashValue<T>){
			return (uint64_t)hash_value(x);
		}else{
			static_assert(sizeof(T) == 0, "Type must be hashable via std::hash or ADL hash_value(x).");
			return 0;
		}
	}

	template<size_t MaxN, class V>
	static inline uint64_t vertex_contrib(int v, const V& val){
		uint64_t hv = hash64(val);
		uint64_t x = 0xA0761D6478BD642FULL ^ (uint64_t)v * 0xE7037ED1A0B428DBULL ^ hv;
		return splitmix64(x);
	}

	template<size_t MaxN, class E>
	static inline uint64_t edge_contrib(int fr, int to, const E& val){
		uint64_t idx = (uint64_t)fr * (uint64_t)MaxN + (uint64_t)to;
		uint64_t hv = hash64(val);
		uint64_t x = 0xD1B54A32D192ED03ULL ^ idx * 0x9E3779B97F4A7C15ULL ^ hv;
		return splitmix64(x);
	}

	template<class V, class E>
	static inline uint64_t init_contrib(const V& v_init, const E& e_init){
		return splitmix64(0xC3A5C85C97CB3127ULL ^ hash64(v_init))
		     ^ splitmix64(0xB492B66FBE98F273ULL ^ hash64(e_init));
	}
	static inline uint64_t size_contrib(int n){
		return splitmix64(0x243F6A8885A308D3ULL ^ (uint64_t)n);
	}

	// 何もしないコールバックの既定実装。フックを不要にする用途で使用できる。
	struct NoopHook{
		// フックを何もしないで実行する。O(1)
		template<class Self, class Act, class E, class V>
		void operator()(const Self&, const Act&, const E&, const V&) const noexcept {}
	};
}

// MaxN上限の動的有向グラフ。bitset隣接行列とGCC拡張で高速な次数列挙を行い、更新と主要照会をO(1)で提供する。
// 注意：頂点削除は末尾のみで、削除対象に入出辺がない前提（ロールバック用途）とする。DAG制約は必要に応じて検査を有効化する。
template<size_t MaxN, bool IsDAG, class E, class V, class H = dyn_detail::NoopHook>
class DynamicDiGraph{
public:
	static_assert(is_trivially_copyable_v<E>, "E must be trivially copyable.");
	static_assert(is_trivially_copyable_v<V>, "V must be trivially copyable.");

	using Self = DynamicDiGraph<MaxN, IsDAG, E, V, H>;
	using ActionT = Action<V,E>;

	// 初期頂点数と初期値を設定して構築する。O(MaxN^2)
	DynamicDiGraph(int init_n = 0, const E& e_init = E{}, const V& v_init = V{}, H hook = H{})
		: n_(clamp_n_(init_n))
		, e_init_(e_init)
		, v_init_(v_init)
		, hook_(std::move(hook))
	{
		for(size_t i=0;i<MaxN;i++){
			val_v_[i] = v_init_;
			out_e_[i].reset();
			in_e_[i].reset();
			out_deg_[i] = 0;
			in_deg_[i] = 0;
		}
		for(size_t i=0;i<MaxN;i++){
			for(size_t j=0;j<MaxN;j++){
				val_e_[i][j] = e_init_;
			}
		}
		m_ = 0;

		tmp_in_froms_.reserve(MaxN);

		init_hash_ = dyn_detail::init_contrib(v_init_, e_init_);
		verts_xor_ = 0;
		for(int i=0;i<n_;++i){
			verts_xor_ ^= dyn_detail::vertex_contrib<MaxN>(i, val_v_[i]);
		}
		edges_xor_ = 0;
		size_hash_ = dyn_detail::size_contrib(n_);
		recalc_hash_();
	}

	// 操作を1件適用し、適用できたか返す。O(1)（DAG検査有効時はO(N*(MaxN/64))）
	// cycle_check=false時は呼び出し側でサイクル不在を保証する
	bool apply(const ActionT& action, bool cycle_check = IsDAG, bool warn = true){
		return apply_internal_(action, cycle_check && IsDAG, warn, true, true);
	}

	// 操作列を順に適用し、全ての結果をANDで返す。O(k)（各操作は同上）
	bool apply(const vector<ActionT>& actions, bool cycle_check = IsDAG, bool warn = true){
		bool all = true;
		for(auto const& a: actions) all &= apply(a, cycle_check, warn);
		return all;
	}

	// 現在の履歴サイズを返す。O(1)
	int snapshot() const{ return (int)rollback_actions.size(); }

	// 指定サイズまで履歴を逆操作で巻き戻す。O(戻す回数)
	void rollback(int snapshot_size){
		if(snapshot_size < 0) snapshot_size = 0;
		if((size_t)snapshot_size > rollback_actions.size()) snapshot_size = (int)rollback_actions.size();
		while((int)rollback_actions.size() > snapshot_size){
			ActionT op = std::move(rollback_actions.back());
			rollback_actions.pop_back();
			(void)apply_internal_(op, false, false, false, false);
		}
	}

	// 履歴を破棄する。O(1)
	void clear_history(){ rollback_actions.clear(); }

	// 現在状態のハッシュを返す。O(1)
	uint64_t hash() const{ return state_hash_; }

	// 状態ハッシュを全走査で再計算する。O(N^2)
	uint64_t hash_slow() const{
		uint64_t vx = 0, ex = 0;
		for(int v=0; v<n_; ++v) vx ^= dyn_detail::vertex_contrib<MaxN>(v, val_v_[v]);
		for(int fr=0; fr<n_; ++fr){
			for_each_out_edge(fr, [&](int to){
				ex ^= dyn_detail::edge_contrib<MaxN>(fr, to, val_e_[fr][to]);
			});
		}
		return init_hash_ ^ dyn_detail::size_contrib(n_) ^ vx ^ ex;
	}

	// 到達可能性を返す（bitset BFS）。O(N*(MaxN/64))
	bool reachable(int src, int tgt) const{
		if(!valid_v_(src) || !valid_v_(tgt)) return false;
		if(src == tgt) return true;

		bitset<MaxN> visited;
		bitset<MaxN> frontier;
		visited.set((size_t)src);
		frontier.set((size_t)src);

		while(frontier.any()){
			bitset<MaxN> nxt;
			size_t v = frontier._Find_first();
			while(v < (size_t)n_){
				nxt |= out_e_[v];
				v = frontier._Find_next(v);
			}
			nxt &= ~visited;
			if(nxt.test((size_t)tgt)) return true;
			if(!nxt.any()) break;
			visited |= nxt;
			frontier = nxt;
		}
		return false;
	}

	// 到達可能集合を返す（bitset BFS）。O(N*(MaxN/64))
	bitset<MaxN> reachable_set(int src) const{
		bitset<MaxN> visited;
		if(!valid_v_(src)) return visited;

		bitset<MaxN> frontier;
		visited.set((size_t)src);
		frontier.set((size_t)src);

		while(frontier.any()){
			bitset<MaxN> nxt;
			size_t v = frontier._Find_first();
			while(v < (size_t)n_){
				nxt |= out_e_[v];
				v = frontier._Find_next(v);
			}
			nxt &= ~visited;
			if(!nxt.any()) break;
			visited |= nxt;
			frontier = nxt;
		}
		return visited;
	}

	// 辺追加でサイクルができるか判定する。O(N*(MaxN/64))
	bool check_cycle_on_add_edge(int fr, int to) const{
		if(fr == to) return true;
		if(!valid_v_(fr) || !valid_v_(to)) return false;
		return reachable(to, fr);
	}

	// サイクル回避のために切るべきfrへの入辺元を列挙する。O(N*(MaxN/64) + 入次数)
	// 自己ループは常にサイクル扱いとなり、入辺削除だけでは回避できない
	bool find_in_edges_to_cut_to_avoid_cycle_on_add(int fr, int to, vector<int>& in_froms_to_remove) const{
		in_froms_to_remove.clear();
		if(!valid_v_(fr) || !valid_v_(to)) return false;
		if(fr == to) return true;

		bitset<MaxN> reach = reachable_set(to);
		if(!reach.test((size_t)fr)) return false;

		bitset<MaxN> cut = in_e_[(size_t)fr] & reach;
		size_t u = cut._Find_first();
		while(u < (size_t)n_){
			in_froms_to_remove.push_back((int)u);
			u = cut._Find_next(u);
		}
		return true;
	}

	// DAGを保つための削除と追加の操作列を生成する。O(N*(MaxN/64) + 生成数)
	// 自己ループは生成不能として空列を返す
	void create_actions_to_add_edge_keeping_dag(int fr, int to, const E& e, vector<ActionT>& actions_to_add) const{
		actions_to_add.clear();
		if(fr == to) return;

		tmp_in_froms_.clear();
		bool cycle = find_in_edges_to_cut_to_avoid_cycle_on_add(fr, to, tmp_in_froms_);
		if(cycle){
			for(int u: tmp_in_froms_) actions_to_add.push_back(ActionT::make_remove_edge(u, fr));
		}
		actions_to_add.push_back(ActionT::make_add_edge(fr, to, e));
	}

	// 出辺先を列挙してコールバックを呼ぶ。O(出次数)
	// 返り値がbool変換可能な場合はtrueで打ち切る
	template<class F>
	void for_each_out_edge(int fr, F&& f) const{
		if(!valid_v_(fr)) return;
		auto const& bs = out_e_[(size_t)fr];
		size_t i = bs._Find_first();
		while(i < (size_t)n_){
			using R = invoke_result_t<F,int>;
			if constexpr (!is_void_v<R> && is_convertible_v<R, bool>){
				if((bool)f((int)i)) break;
			}else{
				f((int)i);
			}
			i = bs._Find_next(i);
		}
	}

	// 入辺元を列挙してコールバックを呼ぶ。O(入次数)
	// 返り値がbool変換可能な場合はtrueで打ち切る
	template<class F>
	void for_each_in_edge(int to, F&& f) const{
		if(!valid_v_(to)) return;
		auto const& bs = in_e_[(size_t)to];
		size_t i = bs._Find_first();
		while(i < (size_t)n_){
			using R = invoke_result_t<F,int>;
			if constexpr (!is_void_v<R> && is_convertible_v<R, bool>){
				if((bool)f((int)i)) break;
			}else{
				f((int)i);
			}
			i = bs._Find_next(i);
		}
	}

	// 辺の存在を返す。O(1)
	bool has_edge(int fr, int to) const{
		if(!valid_v_(fr) || !valid_v_(to)) return false;
		return out_e_[(size_t)fr].test((size_t)to);
	}

	// 頂点数を返す。O(1)
	int vertex_size() const{ return n_; }

	// 辺数を返す。O(1)
	int edge_size() const{ return m_; }

	// 辺の値参照を返す。O(1)
	// 存在しない辺は初期値が入っている
	const E& edge_value_at(int fr, int to) const{
		assert(valid_v_(fr) && valid_v_(to));
		return val_e_[(size_t)fr][(size_t)to];
	}

	// 頂点の値参照を返す。O(1)
	const V& vertex_value_at(int v) const{
		assert(valid_v_(v));
		return val_v_[(size_t)v];
	}

	// 出次数を返す。O(1)
	int out_degree(int fr) const{
		if(!valid_v_(fr)) return 0;
		return out_deg_[(size_t)fr];
	}

	// 入次数を返す。O(1)
	int in_degree(int to) const{
		if(!valid_v_(to)) return 0;
		return in_deg_[(size_t)to];
	}

	// 辺が無いときの初期値を返す。O(1)
	const E& default_edge_value() const{ return e_init_; }

	// 未設定頂点の初期値を返す。O(1)
	const V& default_vertex_value() const{ return v_init_; }

private:
	array<bitset<MaxN>, MaxN> out_e_{}; // 各頂点の出辺先集合を保持する
	array<bitset<MaxN>, MaxN> in_e_{}; // 各頂点の入辺元集合を保持する（out_e_と整合）
	array<array<E, MaxN>, MaxN> val_e_{}; // 各有向辺の値を保持する（存在しない辺は初期値）
	array<V, MaxN> val_v_{}; // 各頂点の値を保持する（未使用領域は初期値）
	vector<ActionT> rollback_actions; // 巻き戻し用の逆操作ログを保持する

	int n_{0}; // 頂点数を保持する
	int m_{0}; // 辺数を保持する

	array<int, MaxN> out_deg_{}; // 出次数を保持する
	array<int, MaxN> in_deg_{}; // 入次数を保持する

	E e_init_{}; // 辺の初期値を保持する
	V v_init_{}; // 頂点の初期値を保持する

	[[no_unique_address]] H hook_{}; // 操作後に呼ぶフックを保持する

	mutable vector<int> tmp_in_froms_; // DAG維持用に一時領域を保持する

	uint64_t init_hash_{0}; // 初期値由来のハッシュ成分を保持する
	uint64_t size_hash_{0}; // 頂点数由来のハッシュ成分を保持する
	uint64_t verts_xor_{0}; // 頂点値由来のハッシュ成分を保持する
	uint64_t edges_xor_{0}; // 辺値由来のハッシュ成分を保持する
	uint64_t state_hash_{0}; // 現在状態のハッシュを保持する

	static int clamp_n_(int n){
		if(n < 0) return 0;
		if(n > (int)MaxN) return (int)MaxN;
		return n;
	}

	inline bool valid_v_(int v) const noexcept{ return (0 <= v && v < n_); }

	inline void update_size_hash_(){ size_hash_ = dyn_detail::size_contrib(n_); }

	inline void recalc_hash_(){ state_hash_ = init_hash_ ^ size_hash_ ^ verts_xor_ ^ edges_xor_; }

	inline void warn_ignored_(const ActionT& a, const char* reason) const{
		cerr << "[DynamicDiGraph] ignored " << action_type_name(a.type) << ": " << reason << "\n";
	}

	bool apply_internal_(const ActionT& action, bool cycle_check, bool warn, bool record_history, bool validate){
		constexpr bool kNoopHook = is_same_v<decay_t<H>, dyn_detail::NoopHook>;

		switch(action.type){
			case AddEdge: {
				int fr = action.x;
				int to = action.y;
				if(validate){
					if(!valid_v_(fr) || !valid_v_(to)){
						if(warn) warn_ignored_(action, "invalid vertex id");
						return false;
					}
					if(out_e_[(size_t)fr].test((size_t)to)){
						if(warn) warn_ignored_(action, "edge already exists");
						return false;
					}
					if(cycle_check && check_cycle_on_add_edge(fr, to)){
						if(warn) warn_ignored_(action, "would create a cycle");
						return false;
					}
				}else{
					assert(0 <= fr && fr < n_ && 0 <= to && to < n_);
					assert(!out_e_[(size_t)fr].test((size_t)to));
				}

				out_e_[(size_t)fr].set((size_t)to);
				in_e_[(size_t)to].set((size_t)fr);
				val_e_[(size_t)fr][(size_t)to] = action.e;

				++m_;
				++out_deg_[(size_t)fr];
				++in_deg_[(size_t)to];

				edges_xor_ ^= dyn_detail::edge_contrib<MaxN>(fr, to, val_e_[(size_t)fr][(size_t)to]);
				recalc_hash_();

				if(record_history) rollback_actions.push_back(ActionT::make_remove_edge(fr, to));
				if constexpr (!kNoopHook) hook_(*this, action, e_init_, v_init_);
				return true;
			}

			case RemoveEdge: {
				int fr = action.x;
				int to = action.y;
				if(validate){
					if(!valid_v_(fr) || !valid_v_(to)){
						if(warn) warn_ignored_(action, "invalid vertex id");
						return false;
					}
					if(!out_e_[(size_t)fr].test((size_t)to)){
						if(warn) warn_ignored_(action, "edge does not exist");
						return false;
					}
				}else{
					assert(0 <= fr && fr < n_ && 0 <= to && to < n_);
					assert(out_e_[(size_t)fr].test((size_t)to));
				}

				E old_e = val_e_[(size_t)fr][(size_t)to];
				edges_xor_ ^= dyn_detail::edge_contrib<MaxN>(fr, to, old_e);

				out_e_[(size_t)fr].reset((size_t)to);
				in_e_[(size_t)to].reset((size_t)fr);
				val_e_[(size_t)fr][(size_t)to] = e_init_;

				--m_;
				--out_deg_[(size_t)fr];
				--in_deg_[(size_t)to];

				recalc_hash_();

				if(record_history) rollback_actions.push_back(ActionT::make_add_edge(fr, to, old_e));
				if constexpr (!kNoopHook) hook_(*this, action, old_e, v_init_);
				return true;
			}

			case UpdateEdge: {
				int fr = action.x;
				int to = action.y;
				if(validate){
					if(!valid_v_(fr) || !valid_v_(to)){
						if(warn) warn_ignored_(action, "invalid vertex id");
						return false;
					}
					if(!out_e_[(size_t)fr].test((size_t)to)){
						if(warn) warn_ignored_(action, "edge does not exist");
						return false;
					}
				}else{
					assert(0 <= fr && fr < n_ && 0 <= to && to < n_);
					assert(out_e_[(size_t)fr].test((size_t)to));
				}

				E old_e = val_e_[(size_t)fr][(size_t)to];
				const E& new_e = action.e;

				edges_xor_ ^= dyn_detail::edge_contrib<MaxN>(fr, to, old_e);
				val_e_[(size_t)fr][(size_t)to] = new_e;
				edges_xor_ ^= dyn_detail::edge_contrib<MaxN>(fr, to, val_e_[(size_t)fr][(size_t)to]);

				recalc_hash_();

				if(record_history) rollback_actions.push_back(ActionT::make_update_edge(fr, to, old_e));
				if constexpr (!kNoopHook) hook_(*this, action, old_e, v_init_);
				return true;
			}

			case AddVertex: {
				if(validate){
					if(n_ >= (int)MaxN){
						if(warn) warn_ignored_(action, "vertex limit exceeded");
						return false;
					}
				}else{
					assert(n_ < (int)MaxN);
				}

				int v = n_;

				out_e_[(size_t)v].reset();
				in_e_[(size_t)v].reset();
				out_deg_[(size_t)v] = 0;
				in_deg_[(size_t)v] = 0;

				val_v_[(size_t)v] = action.v;

				++n_;
				update_size_hash_();

				verts_xor_ ^= dyn_detail::vertex_contrib<MaxN>(v, val_v_[(size_t)v]);
				recalc_hash_();

				if(record_history) rollback_actions.push_back(ActionT::make_remove_vertex());
				if constexpr (!kNoopHook) hook_(*this, action, e_init_, v_init_);
				return true;
			}

			case RemoveVertex: {
				if(validate){
					if(n_ <= 0){
						if(warn) warn_ignored_(action, "no vertex to remove");
						return false;
					}
					int v = n_ - 1;
					if(out_e_[(size_t)v].any() || in_e_[(size_t)v].any()){
						if(warn) warn_ignored_(action, "last vertex still has incident edges");
						return false;
					}
				}else{
					assert(n_ > 0);
					int v = n_ - 1;
					assert(!out_e_[(size_t)v].any() && !in_e_[(size_t)v].any());
				}

				int v = n_ - 1;
				V old_v = val_v_[(size_t)v];

				verts_xor_ ^= dyn_detail::vertex_contrib<MaxN>(v, old_v);

				val_v_[(size_t)v] = v_init_;
				out_e_[(size_t)v].reset();
				in_e_[(size_t)v].reset();
				out_deg_[(size_t)v] = 0;
				in_deg_[(size_t)v] = 0;

				--n_;
				update_size_hash_();
				recalc_hash_();

				if(record_history) rollback_actions.push_back(ActionT::make_add_vertex(old_v));
				if constexpr (!kNoopHook) hook_(*this, action, e_init_, old_v);
				return true;
			}

			case UpdateVertex: {
				int v = action.x;
				if(validate){
					if(!valid_v_(v)){
						if(warn) warn_ignored_(action, "invalid vertex id");
						return false;
					}
				}else{
					assert(0 <= v && v < n_);
				}

				V old_v = val_v_[(size_t)v];
				const V& new_v = action.v;

				verts_xor_ ^= dyn_detail::vertex_contrib<MaxN>(v, old_v);
				val_v_[(size_t)v] = new_v;
				verts_xor_ ^= dyn_detail::vertex_contrib<MaxN>(v, val_v_[(size_t)v]);

				recalc_hash_();

				if(record_history) rollback_actions.push_back(ActionT::make_update_vertex(v, old_v));
				if constexpr (!kNoopHook) hook_(*this, action, e_init_, old_v);
				return true;
			}
		}
		return false;
	}
};

#if 0
#include <random>
#include <chrono>
#include <iomanip>

static volatile uint64_t g_sink = 0;

template<size_t MaxN, bool IsDAG, class E, class V>
struct NaiveGraph{
	static_assert(is_trivially_copyable_v<E>);
	static_assert(is_trivially_copyable_v<V>);

	using ActionT = Action<V,E>;

	int n{0};
	int m{0};
	E e_init{};
	V v_init{};

	vector<vector<unsigned char>> has;
	vector<vector<E>> val_e;
	vector<V> val_v;
	vector<int> out_deg, in_deg;

	vector<ActionT> rollback_actions;

	NaiveGraph(int init_n=0, const E& e0=E{}, const V& v0=V{})
		: n(max(0, min((int)MaxN, init_n)))
		, e_init(e0)
		, v_init(v0)
	{
		has.assign(MaxN, vector<unsigned char>(MaxN, 0));
		val_e.assign(MaxN, vector<E>(MaxN, e_init));
		val_v.assign(MaxN, v_init);
		out_deg.assign(MaxN, 0);
		in_deg.assign(MaxN, 0);
		m = 0;
	}

	bool valid_v(int v) const { return 0 <= v && v < n; }

	bool has_edge(int fr, int to) const{
		if(!valid_v(fr)||!valid_v(to)) return false;
		return has[fr][to] != 0;
	}
	const E& edge_value_at(int fr, int to) const{ assert(valid_v(fr)&&valid_v(to)); return val_e[fr][to]; }
	const V& vertex_value_at(int v) const{ assert(valid_v(v)); return val_v[v]; }
	int vertex_size() const{ return n; }
	int edge_size() const{ return m; }
	int out_degree(int fr) const{ return valid_v(fr)? out_deg[fr] : 0; }
	int in_degree(int to) const{ return valid_v(to)? in_deg[to] : 0; }

	template<class F>
	void for_each_out_edge(int fr, F&& f) const{
		if(!valid_v(fr)) return;
		for(int to=0; to<n; ++to){
			if(has[fr][to]){
				using R = invoke_result_t<F,int>;
				if constexpr (!is_void_v<R> && is_convertible_v<R, bool>){
					if((bool)f(to)) break;
				}else{
					f(to);
				}
			}
		}
	}
	template<class F>
	void for_each_in_edge(int to, F&& f) const{
		if(!valid_v(to)) return;
		for(int fr=0; fr<n; ++fr){
			if(has[fr][to]){
				using R = invoke_result_t<F,int>;
				if constexpr (!is_void_v<R> && is_convertible_v<R, bool>){
					if((bool)f(fr)) break;
				}else{
					f(fr);
				}
			}
		}
	}

	bool reachable(int src, int tgt) const{
		if(!valid_v(src) || !valid_v(tgt)) return false;
		if(src==tgt) return true;
		vector<unsigned char> vis(n, 0);
		vector<int> q;
		q.reserve(n);
		q.push_back(src);
		vis[src]=1;
		for(size_t qi=0; qi<q.size(); ++qi){
			int v=q[qi];
			for(int to=0; to<n; ++to){
				if(has[v][to] && !vis[to]){
					if(to==tgt) return true;
					vis[to]=1;
					q.push_back(to);
				}
			}
		}
		return false;
	}
	vector<unsigned char> reachable_set(int src) const{
		vector<unsigned char> vis(n, 0);
		if(!valid_v(src)) return vis;
		vector<int> q;
		q.reserve(n);
		q.push_back(src);
		vis[src]=1;
		for(size_t qi=0; qi<q.size(); ++qi){
			int v=q[qi];
			for(int to=0; to<n; ++to){
				if(has[v][to] && !vis[to]){
					vis[to]=1;
					q.push_back(to);
				}
			}
		}
		return vis;
	}

	bool check_cycle_on_add_edge(int fr, int to) const{
		if(fr==to) return true;
		if(!valid_v(fr)||!valid_v(to)) return false;
		return reachable(to, fr);
	}

	bool find_in_edges_to_cut_to_avoid_cycle_on_add(int fr, int to, vector<int>& in_froms_to_remove) const{
		in_froms_to_remove.clear();
		if(!valid_v(fr)||!valid_v(to)) return false;
		if(fr==to) return true;
		auto reach = reachable_set(to);
		if(!reach[fr]) return false;
		for(int u=0; u<n; ++u){
			if(has[u][fr] && reach[u]) in_froms_to_remove.push_back(u);
		}
		return true;
	}

	void create_actions_to_add_edge_keeping_dag(int fr, int to, const E& e, vector<ActionT>& actions_to_add) const{
		actions_to_add.clear();
		if(fr == to) return;
		vector<int> rm;
		bool cyc = find_in_edges_to_cut_to_avoid_cycle_on_add(fr,to,rm);
		if(cyc){
			for(int u: rm) actions_to_add.push_back(ActionT::make_remove_edge(u, fr));
		}
		actions_to_add.push_back(ActionT::make_add_edge(fr,to,e));
	}

	int snapshot() const{ return (int)rollback_actions.size(); }
	void clear_history(){ rollback_actions.clear(); }

	bool apply(const ActionT& a, bool cycle_check = IsDAG, bool warn = false){
		(void)warn;
		switch(a.type){
			case AddEdge:{
				int fr=a.x,to=a.y;
				if(!valid_v(fr)||!valid_v(to)) return false;
				if(has[fr][to]) return false;
				if(cycle_check && IsDAG && check_cycle_on_add_edge(fr,to)) return false;
				has[fr][to]=1;
				val_e[fr][to]=a.e;
				++m; ++out_deg[fr]; ++in_deg[to];
				rollback_actions.push_back(ActionT::make_remove_edge(fr,to));
				return true;
			}
			case RemoveEdge:{
				int fr=a.x,to=a.y;
				if(!valid_v(fr)||!valid_v(to)) return false;
				if(!has[fr][to]) return false;
				E old=val_e[fr][to];
				has[fr][to]=0;
				val_e[fr][to]=e_init;
				--m; --out_deg[fr]; --in_deg[to];
				rollback_actions.push_back(ActionT::make_add_edge(fr,to,old));
				return true;
			}
			case UpdateEdge:{
				int fr=a.x,to=a.y;
				if(!valid_v(fr)||!valid_v(to)) return false;
				if(!has[fr][to]) return false;
				E old=val_e[fr][to];
				val_e[fr][to]=a.e;
				rollback_actions.push_back(ActionT::make_update_edge(fr,to,old));
				return true;
			}
			case AddVertex:{
				if(n >= (int)MaxN) return false;
				val_v[n]=a.v;
				++n;
				rollback_actions.push_back(ActionT::make_remove_vertex());
				return true;
			}
			case RemoveVertex:{
				if(n<=0) return false;
				int v=n-1;
				for(int i=0;i<n;++i){
					if(has[v][i]||has[i][v]) return false;
				}
				V old=val_v[v];
				val_v[v]=v_init;
				--n;
				rollback_actions.push_back(ActionT::make_add_vertex(old));
				return true;
			}
			case UpdateVertex:{
				int v=a.x;
				if(!valid_v(v)) return false;
				V old=val_v[v];
				val_v[v]=a.v;
				rollback_actions.push_back(ActionT::make_update_vertex(v,old));
				return true;
			}
		}
		return false;
	}

	bool apply(const vector<ActionT>& actions, bool cycle_check = IsDAG, bool warn = false){
		bool all = true;
		for(auto const& a: actions) all &= apply(a, cycle_check, warn);
		return all;
	}

	void rollback(int snap){
		if(snap<0) snap=0;
		if((size_t)snap > rollback_actions.size()) snap = (int)rollback_actions.size();
		while((int)rollback_actions.size() > snap){
			ActionT op = rollback_actions.back();
			rollback_actions.pop_back();
			apply_unchecked(op);
		}
	}

	void apply_unchecked(const ActionT& a){
		switch(a.type){
			case AddEdge:{
				int fr=a.x,to=a.y;
				has[fr][to]=1;
				val_e[fr][to]=a.e;
				++m; ++out_deg[fr]; ++in_deg[to];
				break;
			}
			case RemoveEdge:{
				int fr=a.x,to=a.y;
				has[fr][to]=0;
				val_e[fr][to]=e_init;
				--m; --out_deg[fr]; --in_deg[to];
				break;
			}
			case UpdateEdge:{
				int fr=a.x,to=a.y;
				val_e[fr][to]=a.e;
				break;
			}
			case AddVertex:{
				val_v[n]=a.v;
				++n;
				break;
			}
			case RemoveVertex:{
				--n;
				val_v[n]=v_init;
				break;
			}
			case UpdateVertex:{
				int v=a.x;
				val_v[v]=a.v;
				break;
			}
		}
	}

	uint64_t hash_slow_like_fast() const{
		uint64_t vx = 0, ex = 0;
		for(int v=0; v<n; ++v) vx ^= dyn_detail::vertex_contrib<MaxN>(v, val_v[v]);
		for(int fr=0; fr<n; ++fr){
			for(int to=0; to<n; ++to){
				if(has[fr][to]){
					ex ^= dyn_detail::edge_contrib<MaxN>(fr, to, val_e[fr][to]);
				}
			}
		}
		return dyn_detail::init_contrib(v_init, e_init) ^ dyn_detail::size_contrib(n) ^ vx ^ ex;
	}
};

static bool is_dag_slow(int n, auto&& has_edge_fn){
	vector<int> indeg(n,0);
	for(int fr=0; fr<n; ++fr) for(int to=0; to<n; ++to) if(has_edge_fn(fr,to)) indeg[to]++;
	vector<int> q; q.reserve(n);
	for(int i=0;i<n;++i) if(indeg[i]==0) q.push_back(i);
	for(size_t qi=0; qi<q.size(); ++qi){
		int v=q[qi];
		for(int to=0; to<n; ++to) if(has_edge_fn(v,to)){
			if(--indeg[to]==0) q.push_back(to);
		}
	}
	return (int)q.size()==n;
}

template<class FastG, class NaiveG>
static void assert_same_state(const FastG& g, const NaiveG& ng, bool full){
	assert(g.vertex_size() == ng.vertex_size());
	assert(g.edge_size() == ng.edge_size());
	int n = g.vertex_size();

	for(int v=0; v<n; ++v){
		assert(g.vertex_value_at(v) == ng.vertex_value_at(v));
		assert(g.out_degree(v) == ng.out_degree(v));
		assert(g.in_degree(v) == ng.in_degree(v));
	}

	if(full){
		for(int fr=0; fr<n; ++fr) for(int to=0; to<n; ++to){
			assert(g.has_edge(fr,to) == ng.has_edge(fr,to));
			assert(g.edge_value_at(fr,to) == ng.edge_value_at(fr,to));
		}
		for(int fr=0; fr<n; ++fr){
			vector<int> a,b;
			g.for_each_out_edge(fr, [&](int to){ a.push_back(to); });
			ng.for_each_out_edge(fr, [&](int to){ b.push_back(to); });
			assert(a==b);
		}
		for(int to=0; to<n; ++to){
			vector<int> a,b;
			g.for_each_in_edge(to, [&](int fr){ a.push_back(fr); });
			ng.for_each_in_edge(to, [&](int fr){ b.push_back(fr); });
			assert(a==b);
		}
	}

	assert(g.hash() == g.hash_slow());
	assert(g.hash() == ng.hash_slow_like_fast());
}

static void basic_edge_case_tests(){
	using G = DynamicDiGraph<16, true, int, int, dyn_detail::NoopHook>;
	G g(0, -7, 3);

	assert(g.vertex_size()==0);
	assert(g.edge_size()==0);
	assert(g.hash() == g.hash_slow());

	for(int i=0;i<16;++i){
		bool ok = g.apply(G::ActionT::make_add_vertex(i), true, false);
		assert(ok);
	}
	assert(g.vertex_size()==16);

	bool ok = g.apply(G::ActionT::make_add_vertex(999), true, false);
	assert(!ok);

	ok = g.apply(G::ActionT::make_update_vertex(16, 1), true, false);
	assert(!ok);

	ok = g.apply(G::ActionT::make_add_edge(0, 99, 1), true, false);
	assert(!ok);

	ok = g.apply(G::ActionT::make_add_edge(0, 1, 10), true, false);
	assert(ok);
	assert(g.has_edge(0,1));
	assert(g.edge_value_at(0,1)==10);
	assert(g.out_degree(0)==1 && g.in_degree(1)==1);

	ok = g.apply(G::ActionT::make_add_edge(0, 1, 999), true, false);
	assert(!ok);

	ok = g.apply(G::ActionT::make_update_edge(0, 1, 11), true, false);
	assert(ok);
	assert(g.edge_value_at(0,1)==11);

	ok = g.apply(G::ActionT::make_update_edge(0, 2, 5), true, false);
	assert(!ok);

	ok = g.apply(G::ActionT::make_remove_edge(0, 1), true, false);
	assert(ok);
	assert(!g.has_edge(0,1));
	assert(g.edge_value_at(0,1)==g.default_edge_value());

	g.apply(G::ActionT::make_add_edge(0, 2, 1), true, false);
	g.apply(G::ActionT::make_add_edge(0, 3, 1), true, false);
	int cnt=0;
	g.for_each_out_edge(0, [&](int){ cnt++; return 1; });
	assert(cnt==1);

	assert(g.reachable(0,2) == true);
	assert(g.reachable(2,0) == false);
	auto rs = g.reachable_set(0);
	assert(rs.test(0) && rs.test(2) && rs.test(3));

	vector<G::ActionT> acts_self;
	g.create_actions_to_add_edge_keeping_dag(0, 0, 5, acts_self);
	assert(acts_self.empty());

	int s0 = g.snapshot();
	uint64_t h0 = g.hash();
	g.apply(G::ActionT::make_add_edge(1, 2, 7), true, false);
	g.apply(G::ActionT::make_update_vertex(5, 100), true, false);
	assert(g.hash()!=h0);
	g.rollback(s0);
	assert(g.hash()==h0);

	g.clear_history();
	assert(g.snapshot()==0);
}

static void dag_cycle_related_tests(){
	using G = DynamicDiGraph<32, true, int, int, dyn_detail::NoopHook>;
	G g(4, 0, 0);

	g.apply(G::ActionT::make_add_edge(0,1,1), true, false);
	g.apply(G::ActionT::make_add_edge(1,2,1), true, false);
	g.apply(G::ActionT::make_add_edge(0,3,1), true, false);
	g.apply(G::ActionT::make_add_edge(3,2,1), true, false);

	assert(g.check_cycle_on_add_edge(2,0) == true);
	assert(g.check_cycle_on_add_edge(0,2) == false);

	vector<int> rm;
	bool cyc = g.find_in_edges_to_cut_to_avoid_cycle_on_add(2,0,rm);
	assert(cyc);
	sort(rm.begin(), rm.end());
	assert((rm == vector<int>({1,3})));

	vector<G::ActionT> acts;
	g.create_actions_to_add_edge_keeping_dag(2,0,5,acts);
	assert(acts.size()==3);
	assert(acts[0].type==RemoveEdge && acts[1].type==RemoveEdge && acts[2].type==AddEdge);

	G g2 = g;
	g2.apply(acts, false, false);
	assert(g2.has_edge(2,0));
	assert(!g2.has_edge(1,2));
	assert(!g2.has_edge(3,2));
	assert(is_dag_slow(g2.vertex_size(), [&](int fr,int to){ return g2.has_edge(fr,to); }));
}

template<size_t MaxN, bool IsDAG>
static void random_consistency_tests(uint64_t seed){
	using Fast = DynamicDiGraph<MaxN, IsDAG, int, int, dyn_detail::NoopHook>;
	using Naive = NaiveGraph<MaxN, IsDAG, int, int>;

	mt19937_64 rng(seed);
	uniform_int_distribution<int> valdist(-1000, 1000);

	Fast g(0, -7, 3);
	Naive ng(0, -7, 3);

	vector<int> saved_snaps_fast;
	vector<int> saved_snaps_naive;

	auto rand_v = [&](int n)->int{
		if(n<=0) return 0;
		uniform_int_distribution<int> d(0, n-1);
		return d(rng);
	};

	const int STEPS = 9000;
	for(int step=1; step<=STEPS; ++step){
		int n = g.vertex_size();
		int r = (int)(rng()%100);

		if(r < 10){
			int x = valdist(rng);
			auto a = Fast::ActionT::make_add_vertex(x);
			bool a1 = g.apply(a, IsDAG, false);
			bool a2 = ng.apply(a, IsDAG, false);
			assert(a1==a2);
		}else if(r < 15){
			auto a = Fast::ActionT::make_remove_vertex();
			bool a1 = g.apply(a, IsDAG, false);
			bool a2 = ng.apply(a, IsDAG, false);
			assert(a1==a2);
		}else if(r < 30){
			int v = (n? rand_v(n) : 0);
			if(rng()%4==0) v = (int)(rng()%MaxN);
			int x = valdist(rng);
			auto a = Fast::ActionT::make_update_vertex(v, x);
			bool a1 = g.apply(a, IsDAG, false);
			bool a2 = ng.apply(a, IsDAG, false);
			assert(a1==a2);
		}else if(r < 55){
			if(n>=2){
				int fr = rand_v(n), to = rand_v(n);
				if(rng()%6==0) to = (int)(rng()%MaxN);
				int x = valdist(rng);
				auto a = Fast::ActionT::make_add_edge(fr,to,x);
				bool a1 = g.apply(a, IsDAG, false);
				bool a2 = ng.apply(a, IsDAG, false);
				assert(a1==a2);
			}
		}else if(r < 70){
			if(n>=1){
				int fr = rand_v(n), to = rand_v(n);
				auto a = Fast::ActionT::make_remove_edge(fr,to);
				bool a1 = g.apply(a, IsDAG, false);
				bool a2 = ng.apply(a, IsDAG, false);
				assert(a1==a2);
			}
		}else if(r < 85){
			if(n>=1){
				int fr = rand_v(n), to = rand_v(n);
				int x = valdist(rng);
				auto a = Fast::ActionT::make_update_edge(fr,to,x);
				bool a1 = g.apply(a, IsDAG, false);
				bool a2 = ng.apply(a, IsDAG, false);
				assert(a1==a2);
			}
		}else if(r < 90){
			saved_snaps_fast.push_back(g.snapshot());
			saved_snaps_naive.push_back(ng.snapshot());
		}else if(r < 96){
			int snap = 0;
			if(!saved_snaps_fast.empty()){
				uniform_int_distribution<int> d(0, (int)saved_snaps_fast.size()-1);
				int idx = d(rng);
				snap = saved_snaps_fast[idx];
				assert(saved_snaps_fast[idx] == saved_snaps_naive[idx]);
			}
			g.rollback(snap);
			ng.rollback(snap);
		}else{
			if(n>=2){
				int fr = rand_v(n), to = rand_v(n);
				int x = valdist(rng);
				vector<typename Fast::ActionT> acts;
				vector<typename Naive::ActionT> acts2;
				g.create_actions_to_add_edge_keeping_dag(fr,to,x,acts);
				ng.create_actions_to_add_edge_keeping_dag(fr,to,x,acts2);

				assert(acts.size() == acts2.size());
				for(size_t i=0;i<acts.size();++i) assert(acts[i].type == acts2[i].type);

				g.apply(acts, false, false);
				ng.apply(acts2, false, false);
			}
		}

		assert(g.vertex_size() == ng.vertex_size());
		assert(g.edge_size() == ng.edge_size());
		assert(g.hash() == g.hash_slow());
		assert(g.hash() == ng.hash_slow_like_fast());

		int nn = g.vertex_size();
		for(int k=0; k<12; ++k){
			if(nn==0) break;
			int fr = (int)(rng()%nn);
			int to = (int)(rng()%nn);
			assert(g.has_edge(fr,to) == ng.has_edge(fr,to));
			assert(g.edge_value_at(fr,to) == ng.edge_value_at(fr,to));
			assert(g.vertex_value_at(fr) == ng.vertex_value_at(fr));
		}

		if(step % 250 == 0){
			assert_same_state(g, ng, true);

			int nnn = g.vertex_size();
			if constexpr (IsDAG){
				assert(is_dag_slow(nnn, [&](int fr,int to){ return g.has_edge(fr,to); }));
				assert(is_dag_slow(nnn, [&](int fr,int to){ return ng.has_edge(fr,to); }));
			}

			for(int t=0; t<10; ++t){
				if(nnn<2) break;
				int a = (int)(rng()%nnn);
				int b = (int)(rng()%nnn);

				bool r1 = g.reachable(a,b);
				bool r2 = ng.reachable(a,b);
				assert(r1==r2);

				auto bs = g.reachable_set(a);
				auto vs = ng.reachable_set(a);
				for(int i=0;i<nnn;++i) assert((int)bs.test((size_t)i) == (int)vs[i]);

				bool c1 = g.check_cycle_on_add_edge(a,b);
				bool c2 = ng.check_cycle_on_add_edge(a,b);
				assert(c1==c2);

				vector<int> rm1, rm2;
				bool b1 = g.find_in_edges_to_cut_to_avoid_cycle_on_add(a,b,rm1);
				bool b2 = ng.find_in_edges_to_cut_to_avoid_cycle_on_add(a,b,rm2);
				assert(b1==b2);
				sort(rm1.begin(), rm1.end());
				sort(rm2.begin(), rm2.end());
				assert(rm1==rm2);
			}
		}
	}
	assert_same_state(g, ng, true);

	int nnn = g.vertex_size();
	if constexpr (IsDAG){
		assert(is_dag_slow(nnn, [&](int fr,int to){ return g.has_edge(fr,to); }));
		assert(is_dag_slow(nnn, [&](int fr,int to){ return ng.has_edge(fr,to); }));
	}
}

struct BenchPerOpStats{
	double avg_ns_per_op{0.0};
	double max_ns_per_op{0.0};
	double min_ns_per_op{0.0};
};

template<class Prepare, class Run, class Cleanup>
static BenchPerOpStats bench_case_per_op(
	const string& name,
	int runs,
	size_t ops_per_run,
	Prepare&& prepare,
	Run&& run,
	Cleanup&& cleanup
){
	using clk = chrono::steady_clock;
	vector<double> ns_per_op;
	ns_per_op.reserve(runs);

	for(int i=0;i<runs;++i){
		prepare(i);
		auto st = clk::now();
		run(i);
		auto ed = clk::now();
		cleanup(i);

		double ns = chrono::duration<double, nano>(ed-st).count();
		ns_per_op.push_back(ns / (double)ops_per_run);
	}

	double mx = *max_element(ns_per_op.begin(), ns_per_op.end());
	double mn = *min_element(ns_per_op.begin(), ns_per_op.end());
	double avg = accumulate(ns_per_op.begin(), ns_per_op.end(), 0.0) / (double)ns_per_op.size();

	cout << left << setw(45) << name
	     << " runs=" << runs
	     << " ops/run=" << ops_per_run
	     << " avg_ns/op=" << fixed << setprecision(2) << avg
	     << " max_ns/op=" << fixed << setprecision(2) << mx
	     << " min_ns/op=" << fixed << setprecision(2) << mn
	     << "\n";

	return {avg, mx, mn};
}

static inline pair<int,int> directed_edge_from_id(int n, uint64_t id){
	uint64_t fr = id / (uint64_t)(n-1);
	uint64_t rem = id - fr * (uint64_t)(n-1);
	uint64_t to = (rem < fr) ? rem : rem + 1;
	return {(int)fr, (int)to};
}

static vector<uint32_t> make_forward_offsets(int n){
	vector<uint32_t> off(n+1, 0);
	uint32_t acc = 0;
	for(int fr=0; fr<n; ++fr){
		off[fr] = acc;
		if(fr <= n-2) acc += (uint32_t)(n-1-fr);
	}
	off[n] = acc;
	return off;
}
static inline pair<int,int> forward_edge_from_id(const vector<uint32_t>& off, int n, uint32_t id){
	auto it = upper_bound(off.begin(), off.end(), id);
	int fr = (int)(it - off.begin()) - 1;
	uint32_t idx = id - off[fr];
	int to = fr + 1 + (int)idx;
	return {fr, to};
}
static inline uint64_t gcd_u64(uint64_t a, uint64_t b){
	while(b){ uint64_t t=a%b; a=b; b=t; }
	return a;
}

template<size_t MaxN>
static void run_benchmarks_for_size(){
	cout << "\n=== Benchmarks (MaxN=" << MaxN << ") ===\n";

	using G = DynamicDiGraph<MaxN, true, int, int, dyn_detail::NoopHook>;
	using Act = typename G::ActionT;

	const int runs = 7;
	mt19937_64 rng(1234567ULL + (uint64_t)MaxN);
	uniform_int_distribution<int> valdist(-1000, 1000);

	const int n = (int)MaxN;
	const uint64_t all_dir_m = (n>=2) ? (uint64_t)n * (uint64_t)(n-1) : 0;
	const uint64_t all_fwd_m = (n>=2) ? (uint64_t)n * (uint64_t)(n-1) / 2ULL : 0;

	{
		G g(n, 0, 0);

		size_t edges_per_round = 0;
		if(n <= 64) edges_per_round = min<uint64_t>(all_dir_m, 2048);
		else if(n <= 128) edges_per_round = min<uint64_t>(all_dir_m, 8192);
		else edges_per_round = min<uint64_t>(all_dir_m, 65536);

		if(edges_per_round == 0) edges_per_round = 1;

		const size_t ops_per_edge = 5;
		const size_t ops_per_round = edges_per_round * ops_per_edge;

		size_t target_ops = (n <= 128) ? 300000 : 200000;
		size_t rounds = (target_ops + ops_per_round - 1) / ops_per_round;

		vector<Act> actions;
		actions.reserve(rounds * ops_per_round);

		uint64_t start = rng() % all_dir_m;
		uint64_t step = (all_dir_m > 0) ? (rng() % all_dir_m) : 1;
		if(step == 0) step = 1;
		while(all_dir_m > 1 && gcd_u64(step, all_dir_m) != 1) step++;

		for(size_t r=0; r<rounds; ++r){
			for(size_t i=0; i<edges_per_round; ++i){
				uint64_t id = (start + (uint64_t)i * step) % all_dir_m;
				auto [fr,to] = directed_edge_from_id(n, id);
				int v1 = valdist(rng);
				int v2 = valdist(rng);
				int vf = valdist(rng);
				int vt = valdist(rng);

				actions.push_back(Act::make_add_edge(fr,to,v1));
				actions.push_back(Act::make_update_vertex(fr,vf));
				actions.push_back(Act::make_update_edge(fr,to,v2));
				actions.push_back(Act::make_update_vertex(to,vt));
				actions.push_back(Act::make_remove_edge(fr,to));
			}
		}

		const size_t ops_per_run = actions.size();

		bench_case_per_op(
			"1) mixed valid ops (cycle_check=false)",
			runs,
			ops_per_run,
			[&](int){ g.rollback(0); g.clear_history(); },
			[&](int){
				g.apply(actions, false, false);
				g_sink ^= g.hash();
			},
			[&](int){
				g.rollback(0);
				g.clear_history();
			}
		);
	}

	{
		G g(n, 0, 0);

		const double init_density = (n <= 128) ? 0.55 : 0.08;
		size_t init_edges = (size_t)min<uint64_t>(all_fwd_m, (uint64_t)(all_fwd_m * init_density));

		auto off = make_forward_offsets(n);

		uint64_t start = (all_fwd_m ? rng()%all_fwd_m : 0);
		uint64_t step = (all_fwd_m ? (rng()%all_fwd_m) : 1);
		if(step == 0) step = 1;
		while(all_fwd_m > 1 && gcd_u64(step, all_fwd_m) != 1) step++;

		for(size_t i=0; i<init_edges; ++i){
			uint32_t id = (uint32_t)((start + (uint64_t)i * step) % all_fwd_m);
			auto [fr,to] = forward_edge_from_id(off, n, id);
			g.apply(Act::make_add_edge(fr,to,1), false, false);
		}
		g.clear_history();

		size_t cand = (n <= 64) ? 256 : (n <= 128 ? 1024 : 4096);
		cand = min<uint64_t>(cand, all_fwd_m);
		vector<pair<int,int>> candidates;
		candidates.reserve(cand);

		for(size_t i=init_edges; candidates.size() < cand && i<init_edges + cand*2; ++i){
			uint32_t id = (uint32_t)((start + (uint64_t)i * step) % all_fwd_m);
			auto [fr,to] = forward_edge_from_id(off, n, id);
			if(!g.has_edge(fr,to)) candidates.push_back({fr,to});
		}
		if(candidates.empty()){
			if(n>=2) candidates.push_back({0,1});
			else candidates.push_back({0,0});
		}

		size_t iters = 0;
		if(n <= 64) iters = 60000;
		else if(n <= 128) iters = 80000;
		else iters = 15000;

		const size_t ops_per_run = iters;

		bench_case_per_op(
			"2) AddEdge DAG check (snapshot+add+rollback)",
			runs,
			ops_per_run,
			[&](int){ g.rollback(0); g.clear_history(); },
			[&](int){
				for(size_t i=0;i<iters;++i){
					auto [fr,to] = candidates[i % candidates.size()];
					int snap = g.snapshot();
					(void)g.apply(Act::make_add_edge(fr,to,valdist(rng)), true, false);
					g.rollback(snap);
				}
				g_sink ^= g.hash();
			},
			[&](int){
				g.rollback(0);
				g.clear_history();
			}
		);
	}

	{
		G g(n, 0, 0);

		const double init_density = (n <= 128) ? 0.35 : 0.05;
		size_t init_edges = (size_t)min<uint64_t>(all_fwd_m, (uint64_t)(all_fwd_m * init_density));

		auto off = make_forward_offsets(n);

		uint64_t start = (all_fwd_m ? rng()%all_fwd_m : 0);
		uint64_t step = (all_fwd_m ? (rng()%all_fwd_m) : 1);
		if(step == 0) step = 1;
		while(all_fwd_m > 1 && gcd_u64(step, all_fwd_m) != 1) step++;

		for(size_t i=0; i<init_edges; ++i){
			uint32_t id = (uint32_t)((start + (uint64_t)i * step) % all_fwd_m);
			auto [fr,to] = forward_edge_from_id(off, n, id);
			g.apply(Act::make_add_edge(fr,to,1), false, false);
		}
		g.clear_history();

		size_t iters = 0;
		if(n <= 64) iters = 25000;
		else if(n <= 128) iters = 20000;
		else iters = 5000;

		vector<pair<int,int>> cand;
		cand.reserve(iters);
		for(size_t i=0; i<iters; ++i){
			uint32_t id = (uint32_t)((start + (uint64_t)(init_edges + i) * step) % all_fwd_m);
			auto [a,b] = forward_edge_from_id(off, n, id);
			cand.push_back({b,a});
		}

		vector<Act> acts;
		acts.reserve(1 + MaxN);

		const size_t ops_per_run = iters;

		bench_case_per_op(
			"3) create_actions_to_add_edge_keeping_dag + apply",
			runs,
			ops_per_run,
			[&](int){
				g.rollback(0);
				g.clear_history();
			},
			[&](int){
				for(size_t i=0;i<iters;++i){
					auto [fr,to] = cand[i];
					g.create_actions_to_add_edge_keeping_dag(fr, to, valdist(rng), acts);
					g.apply(acts, false, false);
				}
				g_sink ^= g.hash();
			},
			[&](int){
				g.rollback(0);
				g.clear_history();
			}
		);
	}

	{
		G g(n, 0, 0);
		g.clear_history();

		const int REPS = 6;

		size_t batch = 0;
		if(n <= 64) batch = min<uint64_t>(all_dir_m, 3000);
		else if(n <= 128) batch = min<uint64_t>(all_dir_m, 12000);
		else batch = 60000;

		if(batch == 0) batch = 1;

		vector<pair<int,int>> edges;
		edges.reserve(batch);

		uint64_t start = (all_dir_m ? rng()%all_dir_m : 0);
		uint64_t step = (all_dir_m ? (rng()%all_dir_m) : 1);
		if(step == 0) step = 1;
		while(all_dir_m > 1 && gcd_u64(step, all_dir_m) != 1) step++;

		for(size_t i=0; i<batch; ++i){
			uint64_t id = (start + (uint64_t)i * step) % all_dir_m;
			edges.push_back(directed_edge_from_id(n, id));
		}

		const size_t ops_per_run = (size_t)REPS * batch * 2;

		bench_case_per_op(
			"4) snapshot+apply(BATCH)+rollback heavy",
			runs,
			ops_per_run,
			[&](int){
				g.rollback(0);
				g.clear_history();
			},
			[&](int){
				for(int rep=0; rep<REPS; ++rep){
					int snap = g.snapshot();
					for(size_t i=0;i<batch;++i){
						auto [fr,to] = edges[i];
						(void)g.apply(Act::make_add_edge(fr,to,valdist(rng)), false, false);
					}
					g.rollback(snap);
				}
				g_sink ^= g.hash();
			},
			[&](int){
				g.rollback(0);
				g.clear_history();
			}
		);
	}

	{
		G g(n, 0, 0);

		const double p = (n <= 128) ? 0.12 : 0.02;
		const uint64_t target_edges = (uint64_t)( (double)all_dir_m * p );

		uint64_t start = (all_dir_m ? rng()%all_dir_m : 0);
		uint64_t step = (all_dir_m ? (rng()%all_dir_m) : 1);
		if(step == 0) step = 1;
		while(all_dir_m > 1 && gcd_u64(step, all_dir_m) != 1) step++;

		uint64_t added = 0;
		for(uint64_t i=0; added < target_edges && i < all_dir_m; ++i){
			uint64_t id = (start + i * step) % all_dir_m;
			auto [fr,to] = directed_edge_from_id(n, id);
			if(g.apply(Act::make_add_edge(fr,to,1), false, false)) ++added;
		}
		g.clear_history();

		size_t iters = 0;
		if(n <= 64) iters = 250000;
		else if(n <= 128) iters = 250000;
		else iters = 80000;

		const size_t ops_per_run = iters;

		bench_case_per_op(
			"5) iterate adjacency (out+in via _Find_next)",
			runs,
			ops_per_run,
			[&](int){
			},
			[&](int){
				uint64_t acc = 0;
				for(size_t i=0;i<iters;++i){
					int v = (int)(i % (size_t)n);

					int sum_out = 0;
					g.for_each_out_edge(v, [&](int to){
						sum_out += to;
					});

					int sum_in = 0;
					g.for_each_in_edge(v, [&](int fr){
						sum_in += fr;
					});

					acc += (uint64_t)(sum_out * 1315423911u + sum_in);
				}
				g_sink ^= acc;
			},
			[&](int){
			}
		);
	}
}

int main(){
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	cout << "Running edge-case tests...\n";
	basic_edge_case_tests();

	cout << "Running DAG cycle-related tests...\n";
	dag_cycle_related_tests();

	cout << "Running random consistency tests...\n";
	random_consistency_tests<64, true>(1);
	random_consistency_tests<64, true>(2);
	random_consistency_tests<64, false>(3);
	random_consistency_tests<64, false>(4);

	cout << "All tests passed.\n";

	run_benchmarks_for_size<64>();
	run_benchmarks_for_size<128>();
	run_benchmarks_for_size<1024>();

	cout << "sink=" << (uint64_t)g_sink << "\n";
	return 0;
}
#endif

// 実行結果 (AtCoder)
// Running edge-case tests...
// Running DAG cycle-related tests...
// Running random consistency tests...
// All tests passed.

// === Benchmarks (MaxN=64) ===
// 1) mixed valid ops (cycle_check=false)        runs=7 ops/run=307200 avg_ns/op=11.73 max_ns/op=24.54 min_ns/op=9.51
// 2) AddEdge DAG check (snapshot+add+rollback)  runs=7 ops/run=60000 avg_ns/op=66.74 max_ns/op=68.00 min_ns/op=65.52
// 3) create_actions_to_add_edge_keeping_dag + apply runs=7 ops/run=25000 avg_ns/op=58.10 max_ns/op=60.04 min_ns/op=55.68
// 4) snapshot+apply(BATCH)+rollback heavy       runs=7 ops/run=36000 avg_ns/op=9.87 max_ns/op=10.13 min_ns/op=9.66
// 5) iterate adjacency (out+in via _Find_next)  runs=7 ops/run=250000 avg_ns/op=14.98 max_ns/op=15.20 min_ns/op=14.84

// === Benchmarks (MaxN=128) ===
// 1) mixed valid ops (cycle_check=false)        runs=7 ops/run=327680 avg_ns/op=11.09 max_ns/op=23.27 min_ns/op=8.96
// 2) AddEdge DAG check (snapshot+add+rollback)  runs=7 ops/run=80000 avg_ns/op=200.08 max_ns/op=206.94 min_ns/op=197.85
// 3) create_actions_to_add_edge_keeping_dag + apply runs=7 ops/run=20000 avg_ns/op=203.68 max_ns/op=209.07 min_ns/op=201.36
// 4) snapshot+apply(BATCH)+rollback heavy       runs=7 ops/run=144000 avg_ns/op=10.31 max_ns/op=10.56 min_ns/op=10.07
// 5) iterate adjacency (out+in via _Find_next)  runs=7 ops/run=250000 avg_ns/op=50.19 max_ns/op=54.38 min_ns/op=48.65

// === Benchmarks (MaxN=1024) ===
// 1) mixed valid ops (cycle_check=false)        runs=7 ops/run=327680 avg_ns/op=15.09 max_ns/op=28.24 min_ns/op=12.67
// 2) AddEdge DAG check (snapshot+add+rollback)  runs=7 ops/run=15000 avg_ns/op=1681.40 max_ns/op=1691.44 min_ns/op=1671.29
// 3) create_actions_to_add_edge_keeping_dag + apply runs=7 ops/run=5000 avg_ns/op=3139.69 max_ns/op=3169.95 min_ns/op=3111.51
// 4) snapshot+apply(BATCH)+rollback heavy       runs=7 ops/run=720000 avg_ns/op=14.54 max_ns/op=15.13 min_ns/op=14.36
// 5) iterate adjacency (out+in via _Find_next)  runs=7 ops/run=80000 avg_ns/op=168.27 max_ns/op=171.77 min_ns/op=165.35
// sink=16919345349728337776
