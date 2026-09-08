#pragma once
#include <bits/stdc++.h>
/*
  fast_bellman_ford.hpp
  - Bellman–Ford の高速実装（階層化 / 束縛付き更新 / SPFA+SLF/LLL / SPFA FIFO）
  - C++20 (GCC 12.2) / ヘッダオンリー
  - 競プロ実運用向け：可読性と速度のバランスを重視

  本ヘッダは自己完結（main 付き）で、
  すべてのベンチマークで以下「9通り」を必ず計測します：

    1) BF basic
    2) BF layered (no bound)
    3) BF layered (bound)
    4) SPFA+SLF/LLL (no bound)
    5) SPFA+SLF/LLL (bound)
    6) SPFA FIFO (no bound)     ← New: 全ベンチで明示計測
    7) SPFA FIFO (bound)        ← New: 全ベンチで明示計測
    8) auto (no bound)
    9) auto (bound)

  仕様メモ
  - Options::bound_on_relax=true の場合、「cand < 上界(ub)」のときのみ緩和します。
    ub は cutoff と upper_limit[v] から決定（いずれも設定がなければ ∞）。
  - 現状のベンチの "bound" ケースは、これまでの設計を踏襲（cutoff=0 など）。
    ※bound の厳しさが過剰だと「本来必要な緩和を跳ねる」入力があり得ます。
      検証は verify_feasible_bounded() が担いますが、gold 突合は未導入です。
      （要求は「まず 9 通りを全ベンチで計測する」ため、ロジック改善は別途対応）

  収録ベンチ
    * DAG (medium density)
    * Dense-Local-Frontier（layered-favored）
    * Dense-Local-Frontier（stronger）
    * Alternating-wave
    * Wide-Fanin-Levels（layered-favored）
    * Negative-Dense-DAG（layered-favored, 負辺密・DAG）
    * NonDAG-Neg-Heavy（負辺密・非DAG・負閉路なし）
    * Zero-Sum-Bidirectional（SPFA-adversarial を狙うがゼロ和で穏当）
    * Adversarial-Potentials-SCC（dense, SPFA-unfriendly）
    * Cascading-Hub Multi-Path（SPFA-unfriendly）

  注意
  - 競プロ用途想定につき、無効インデックス等は未定義動作。
  - INF は saturating。整数は __int128 で飽和加算。
*/

namespace fastbf {

#if defined(__GNUC__)
  #define FASTBF_LIKELY(x)   (__builtin_expect(!!(x), 1))
  #define FASTBF_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
  #define FASTBF_LIKELY(x)   (x)
  #define FASTBF_UNLIKELY(x) (x)
#endif

// ---------- ユーティリティ：INF, 加算安全化 ----------
template <class W>
constexpr W POS_CAP() {
  if constexpr (std::numeric_limits<W>::has_infinity) {
    return std::numeric_limits<W>::infinity();
  } else {
    return std::numeric_limits<W>::max() / 4; // INF 代用（十分に大きい）
  }
}
template <class W>
constexpr W NEG_CAP() {
  if constexpr (std::numeric_limits<W>::has_infinity) {
    return -std::numeric_limits<W>::infinity();
  } else if constexpr (std::numeric_limits<W>::is_signed) {
    return std::numeric_limits<W>::lowest() / 4; // –INF 代用
  } else {
    return W(0); // unsigned の場合は使わない
  }
}

template <class W>
inline W cap_add(W a, W b) {
  // a==INF なら INF。整数は __int128 で飽和加算。
  if constexpr (std::numeric_limits<W>::has_infinity) {
    if (std::isinf((double)a)) return a;
    return a + b;
  } else {
    const W INF = POS_CAP<W>();
    if (a >= INF) return INF;
    using I128 = __int128_t;
    I128 s = (I128)a + (I128)b;
    I128 hi = (I128)POS_CAP<W>();
    I128 lo = (I128)NEG_CAP<W>();
    if (s > hi) s = hi;
    if constexpr (std::numeric_limits<W>::is_signed) {
      if (s < lo) s = lo;
    }
    return (W)s;
  }
}

// ---------- グラフ表現 ----------
template <class W>
struct Graph {
  struct Edge { int to; W w; };
  int n = 0;
  std::vector<std::vector<Edge>> g;               // 隣接リスト（SPFA/階層化で使用）
  std::vector<std::tuple<int,int,W>> edges_flat;  // 平坦化リスト（通常版で高速）

  Graph() = default;
  explicit Graph(int n_) { reset(n_); }
  void reset(int n_) { n = n_; g.assign(n, {}); edges_flat.clear(); }

  void add_edge(int u, int v, W w) {
    g[u].push_back({v, w});
    edges_flat.emplace_back(u, v, w);
  }
  void reserve_edges(size_t m) { edges_flat.reserve(m); }

  void sort_adj_by_weight() {
    for (auto &adj : g) {
      std::sort(adj.begin(), adj.end(),
                [](const Edge& a, const Edge& b){ return a.w < b.w; });
    }
  }
};

// ---------- 結果 ----------
template <class W>
struct Result {
  std::vector<W> dist;
  std::vector<int> prev; // 直前頂点（経路復元用）
  bool has_negative_cycle = false;
  std::vector<int> negative_cycle_nodes; // 検出できた場合のみ（簡易）
};

// ---------- オプション ----------
template <class W>
struct Options {
  bool use_slf = true;              // SPFA: Small Label First
  bool use_lll = true;              // SPFA: Large Label Last
  bool sort_edges = false;          // 出辺ソート（微効果）
  int  target = -1;                 // 単一ターゲット（指定しなくてもOK）
  W    cutoff = POS_CAP<W>();       // 全体上界（∞相当で無効化）
  std::vector<W> upper_limit;       // 頂点別上界（サイズ n か空）
  bool stop_early_on_target = true; // ターゲット改善後の自然な早期停止
  int  relax_limit_per_vertex = 0;  // 0=自動(n)。SPFAの監視
  bool bound_on_relax = false;      // true: cand>=ub なら緩和しない（無駄更新削減）
};

// ---------- 閉路復元（負閉路） ----------
template <class W>
static std::vector<int> extract_cycle(int v, const std::vector<int>& prev, int n) {
  if (v < 0) return {};
  int x = v;
  for (int i = 0; i < n; ++i) {
    if (x < 0) return {};
    x = prev[x];
  }
  if (x < 0) return {};
  int start = x;
  std::vector<int> cyc;
  int y = start;
  std::unordered_set<int> seen;
  do {
    cyc.push_back(y);
    seen.insert(y);
    y = prev[y];
    if (y < 0) return {};
  } while (y != start && (int)cyc.size() <= n + 5);
  std::reverse(cyc.begin(), cyc.end());
  return cyc;
}

// =====================================================
// 1) 通常 Bellman–Ford（基準）
// =====================================================
template <class W>
Result<W> bellman_ford_basic(const Graph<W>& G, int s, bool find_negative_cycle = true) {
  const int n = G.n;
  const W INF = POS_CAP<W>();
  Result<W> R;
  R.dist.assign(n, INF);
  R.prev.assign(n, -1);
  if (s < 0 || s >= n) return R;

  R.dist[s] = W(0);
  std::vector<char> reachable(n, 0);
  reachable[s] = 1;

  bool changed = true;
  for (int iter = 0; iter < n - 1 && changed; ++iter) {
    changed = false;
    for (const auto& [u, v, w] : G.edges_flat) {
      if (R.dist[u] >= INF) continue;
      W cand = cap_add<W>(R.dist[u], w);
      if (cand < R.dist[v]) {
        R.dist[v] = cand;
        R.prev[v] = u;
        changed = true;
        reachable[v] = 1;
      }
    }
  }

  if (find_negative_cycle) {
    int witness = -1;
    for (const auto& [u, v, w] : G.edges_flat) {
      if (!reachable[u]) continue; // s から到達可能か
      if (R.dist[u] >= INF) continue;
      W cand = cap_add<W>(R.dist[u], w);
      if (cand < R.dist[v]) { witness = v; break; }
    }
    if (witness != -1) {
      R.has_negative_cycle = true;
      R.negative_cycle_nodes = extract_cycle<W>(witness, R.prev, n);
    }
  }
  return R;
}

// =====================================================
// 2) 階層化 Bellman–Ford（frontier layering）
// =====================================================
template <class W>
Result<W> bellman_ford_layered(const Graph<W>& G, int s, const Options<W>& opt = {}) {
  const int n = G.n;
  const W INF = POS_CAP<W>();
  Result<W> R;
  R.dist.assign(n, INF);
  R.prev.assign(n, -1);
  if (s < 0 || s >= n) return R;

  R.dist[s] = W(0);
  std::vector<char> visited(n, 0), in_next(n, 0);
  visited[s] = 1;

  std::vector<int> curr, next;
  curr.reserve(n);
  next.reserve(n);
  curr.push_back(s);

  const bool has_ul = (!opt.upper_limit.empty() && (int)opt.upper_limit.size() == n);
  auto limit_of = [&](int v, W ub_base) -> W {
    if (has_ul) return std::min(ub_base, opt.upper_limit[v]);
    return ub_base;
  };

  W target_ub = opt.cutoff;
  if (opt.target >= 0 && opt.target < n) {
    if (R.dist[opt.target] < target_ub) target_ub = R.dist[opt.target];
  }

  bool any_change = true;
  for (int iter = 0; iter < n - 1 && any_change && !curr.empty(); ++iter) {
    any_change = false;
    std::fill(in_next.begin(), in_next.end(), 0);

    W ub_base = std::min(target_ub, opt.cutoff);

    for (int u : curr) {
      if (R.dist[u] >= INF) continue;
      const auto& adj = G.g[u];
      for (const auto& e : adj) {
        int v = e.to;
        W old = R.dist[v];
        W cand = cap_add<W>(R.dist[u], e.w);

        if (opt.bound_on_relax) {
          W ub_relax = limit_of(v, ub_base);
          if (!(cand < ub_relax)) continue;
        }

        if (cand < old) {
          R.dist[v] = cand;
          R.prev[v] = u;
          visited[v] = 1;
          any_change = true;

          W ub_push = limit_of(v, ub_base);
          if (cand < ub_push && !in_next[v]) { next.push_back(v); in_next[v] = 1; }

          if (v == opt.target && opt.stop_early_on_target) {
            if (R.dist[v] < target_ub) { target_ub = R.dist[v]; ub_base = std::min(target_ub, opt.cutoff); }
          }
        }
      }
    }
    curr.swap(next);
    next.clear();
  }

  // 負閉路検出（到達可能 + bound 整合）
  {
    W ub_base = std::min(target_ub, opt.cutoff);
    int witness = -1;
    for (int u = 0; u < n; ++u) if (visited[u] && R.dist[u] < INF) {
      for (const auto& e : G.g[u]) {
        int v = e.to;
        W cand = cap_add<W>(R.dist[u], e.w);
        W ub = limit_of(v, ub_base);
        if (R.dist[v] < ub && cand < R.dist[v]) { witness = v; break; }
      }
      if (witness != -1) {
        R.has_negative_cycle = true;
        R.negative_cycle_nodes = extract_cycle<W>(witness, R.prev, n);
        break;
      }
    }
  }
  return R;
}

// =====================================================
// 3) SPFA（SLF/LLL or FIFO）
//    - 上界は「キュー投入抑止」にのみ使用
//    - POP/RELAX の安全リミットは緩め（負閉路誤検出回避）
// =====================================================
template <class W>
Result<W> spfa_fast(const Graph<W>& G, int s, const Options<W>& opt = {}) {
  const int n = G.n;
  const W INF = POS_CAP<W>();
  Result<W> R;
  R.dist.assign(n, INF);
  R.prev.assign(n, -1);
  if (s < 0 || s >= n) return R;

  std::deque<int> q;
  std::vector<char> inq(n, 0);
  std::vector<int> relax_cnt(n, 0);
  R.dist[s] = W(0);
  q.push_back(s); inq[s] = 1;

  const int per_vertex_limit = (opt.relax_limit_per_vertex > 0) ? opt.relax_limit_per_vertex : std::max(1, n);

  const bool has_ul = (!opt.upper_limit.empty() && (int)opt.upper_limit.size() == n);
  auto limit_of = [&](int v, W ub_base) -> W {
    if (has_ul) return std::min(ub_base, opt.upper_limit[v]);
    return ub_base;
  };

  auto safe_val = [&](int v)->W { return R.dist[v]; };
  long double sum_in_q = 0.0L;
  auto on_push = [&](int v){
    if constexpr (!std::numeric_limits<W>::has_infinity) sum_in_q += (long double)safe_val(v);
    else if (!std::isinf((double)R.dist[v])) sum_in_q += (long double)R.dist[v];
  };
  auto on_pop = [&](int v){
    if constexpr (!std::numeric_limits<W>::has_infinity) sum_in_q -= (long double)safe_val(v);
    else if (!std::isinf((double)R.dist[v])) sum_in_q -= (long double)R.dist[v];
  };

  const long long M = (long long)G.edges_flat.size();
  const long long POP_LIMIT   = std::max( (long long)n * 10LL, 1000LL * M );
  const long long RELAX_LIMIT = std::max( (long long)n * 20LL, 2000LL * M );
  long long pop_count = 0, relax_trials = 0;

  W target_ub = opt.cutoff;

  while (!q.empty()) {
    int u = q.front(); q.pop_front(); inq[u] = 0; on_pop(u);
    if (++pop_count > POP_LIMIT) { R.has_negative_cycle = true; return R; }

    if (opt.use_lll && !q.empty()) {
      long double avg = sum_in_q / (long double)q.size();
      if ((long double)R.dist[u] > avg) { q.push_back(u); inq[u] = 1; on_push(u); continue; }
    }

    W ub_base = std::min(target_ub, opt.cutoff);

    for (const auto& e : G.g[u]) {
      if (R.dist[u] >= INF) continue;
      int v = e.to;
      W cand = cap_add<W>(R.dist[u], e.w);
      if (++relax_trials > RELAX_LIMIT) { R.has_negative_cycle = true; return R; }

      if (opt.bound_on_relax) {
        W ub_relax = limit_of(v, ub_base);
        if (!(cand < ub_relax)) continue;
      }

      bool updated = false;
      if (cand < R.dist[v]) {
        R.dist[v] = cand;
        R.prev[v] = u;
        updated = true;
        if (++relax_cnt[v] > per_vertex_limit) {
          R.has_negative_cycle = true;
          R.negative_cycle_nodes = extract_cycle<W>(v, R.prev, n);
          return R;
        }
      }

      if (updated) {
        W ub = limit_of(v, ub_base);
        if (!inq[v] && R.dist[v] < ub) {
          if (opt.use_slf && !q.empty()) {
            if (R.dist[v] < R.dist[q.front()]) { q.push_front(v); inq[v] = 1; on_push(v); }
            else { q.push_back(v); inq[v] = 1; on_push(v); }
          } else { q.push_back(v); inq[v] = 1; on_push(v); }
        }
        if (opt.target == v && opt.stop_early_on_target) {
          if (R.dist[v] < target_ub) { target_ub = R.dist[v]; ub_base = std::min(target_ub, opt.cutoff); }
        }
      }
    }
  }
  return R;
}

// =====================================================
// 4) 自動選択（密度で簡易切替）
// =====================================================
template <class W>
Result<W> bellman_ford_auto(const Graph<W>& G, int s, Options<W> opt = {}) {
  const int n = G.n;
  const size_t m = G.edges_flat.size();
  if (n == 0) return Result<W>{};
  long double density = (n > 1) ? (long double)m / (long double)(n - 1) : 0.0L;
  if (density <= 4.0L) {
    return spfa_fast<W>(G, s, opt);
  } else {
    return bellman_ford_layered<W>(G, s, opt);
  }
}

} // namespace fastbf

// =====================================================
// ===================== テスト／ベンチ =================
// =====================================================
#if 1
#include <chrono>
#include <iomanip>
using namespace std;
using fastbf::Graph;
using fastbf::Result;
using fastbf::Options;
using i64 = long long;

static void expect(bool cond, const string& msg) {
  if (!cond) {
    cerr << "fast_bellman_ford.hpp self-test & benchmark\n";
    cerr << "Test failed: " << msg << "\n";
    exit(1);
  }
}

// Bellman-Ford の基本検査（標準 BF 性質）
template <class W>
static void verify_feasible_basic(const Graph<W>& G, const Result<W>& R) {
  const W INF = fastbf::POS_CAP<W>();
  if (R.has_negative_cycle) return;
  for (auto [u,v,w] : G.edges_flat) {
    if (R.dist[u] >= INF) continue;
    W cand = fastbf::cap_add<W>(R.dist[u], w);
    if (!(R.dist[v] <= cand)) {
      cerr << "Feasibility violated at edge " << u << "->" << v << " (w=" << (long long)w << ")\n";
      exit(2);
    }
  }
}

// 上界付き緩和抑止モードに整合した検査（cand<ub の辺のみチェック）
template <class W>
static void verify_feasible_bounded(const Graph<W>& G, const Result<W>& R, const Options<W>& opt) {
  const W INF = fastbf::POS_CAP<W>();
  const bool has_ul = (!opt.upper_limit.empty() && (int)opt.upper_limit.size() == G.n);
  auto limit_of = [&](int v, W ub_base) -> W {
    if (has_ul) return std::min(ub_base, opt.upper_limit[v]);
    return ub_base;
  };
  W target_ub = opt.cutoff;
  if (opt.target >= 0 && opt.target < G.n && R.dist[opt.target] < target_ub) target_ub = R.dist[opt.target];
  W ub_base = std::min(target_ub, opt.cutoff);

  if (R.has_negative_cycle) return;
  for (auto [u,v,w] : G.edges_flat) {
    if (R.dist[u] >= INF) continue;
    W cand = fastbf::cap_add<W>(R.dist[u], w);
    W ub   = limit_of(v, ub_base);
    if (!(cand < ub)) continue; // 抑止対象は検査外
    if (!(R.dist[v] <= cand)) {
      cerr << "Feasibility (bounded) violated at edge " << u << "->" << v
           << " (w=" << (long long)w << ", ub=" << (long long)ub << ")\n";
      exit(3);
    }
  }
}

// 経路復元（デバッグ用）
template <class W>
static vector<int> restore_path(int t, const Result<W>& R) {
  vector<int> path;
  for (int v = t; v != -1; v = R.prev[v]) path.push_back(v);
  reverse(path.begin(), path.end());
  return path;
}

// ----------------------------------------------
// 便利ランチャ（9通りの統一出力をしたい場合用）
// ----------------------------------------------
template <class W, class BuildFuncs>
static void run_all_9_variants(const string& bench_title,
                               const Graph<W>& G,
                               const BuildFuncs& make_opts,
                               int s,
                               int trials = 3) {
  using namespace std::chrono;
  cout << bench_title << "\n";

  auto [opt_bf_layered_no, opt_bf_layered_bo,
        opt_spfa_slf_no, opt_spfa_slf_bo,
        opt_spfa_fifo_no, opt_spfa_fifo_bo,
        opt_auto_no, opt_auto_bo] = make_opts();

  auto bench_run_basic = [&](auto func, const string& name){
    long long best_us=(1LL<<60), sum_us=0; int td=0; Result<W> R;
    for(int t=0;t<trials;++t){ auto a=steady_clock::now(); R=func(); auto b=steady_clock::now();
      long long us=duration_cast<microseconds>(b-a).count(); best_us=min(best_us,us); sum_us+=us; ++td; }
    cout<<setw(30)<<left<<name<<" avg="<<(sum_us/max(1,td))<<" us, best="<<best_us<<" us\n";
    verify_feasible_basic(G,R);
  };
  auto bench_run_bounded = [&](auto func, const string& name, const Options<W>& opt){
    long long best_us=(1LL<<60), sum_us=0; int td=0; Result<W> R;
    for(int t=0;t<trials;++t){ auto a=steady_clock::now(); R=func(); auto b=steady_clock::now();
      long long us=duration_cast<microseconds>(b-a).count(); best_us=min(best_us,us); sum_us+=us; ++td; }
    cout<<setw(30)<<left<<name<<" avg="<<(sum_us/max(1,td))<<" us, best="<<best_us<<" us\n";
    verify_feasible_bounded(G,R,opt);
  };

  // 1) BF basic
  bench_run_basic([&](){ return fastbf::bellman_ford_basic<W>(G, s); }, "BF basic");
  // 2) BF layered (no bound)
  bench_run_basic([&](){ return fastbf::bellman_ford_layered<W>(G, s, opt_bf_layered_no); }, "BF layered (no bound)");
  // 3) BF layered (bound)
  bench_run_bounded([&](){ return fastbf::bellman_ford_layered<W>(G, s, opt_bf_layered_bo); }, "BF layered (bound)", opt_bf_layered_bo);
  // 4) SPFA+SLF/LLL (no bound)
  bench_run_basic([&](){ return fastbf::spfa_fast<W>(G, s, opt_spfa_slf_no); }, "SPFA+SLF/LLL (no bound)");
  // 5) SPFA+SLF/LLL (bound)
  bench_run_bounded([&](){ return fastbf::spfa_fast<W>(G, s, opt_spfa_slf_bo); }, "SPFA+SLF/LLL (bound)", opt_spfa_slf_bo);
  // 6) SPFA FIFO (no bound)
  bench_run_basic([&](){ return fastbf::spfa_fast<W>(G, s, opt_spfa_fifo_no); }, "SPFA FIFO (no bound)");
  // 7) SPFA FIFO (bound)
  bench_run_bounded([&](){ return fastbf::spfa_fast<W>(G, s, opt_spfa_fifo_bo); }, "SPFA FIFO (bound)", opt_spfa_fifo_bo);
  // 8) auto (no bound)
  bench_run_basic([&](){ return fastbf::bellman_ford_auto<W>(G, s, opt_auto_no); }, "auto (no bound)");
  // 9) auto (bound)
  bench_run_bounded([&](){ return fastbf::bellman_ford_auto<W>(G, s, opt_auto_bo); }, "auto (bound)", opt_auto_bo);
}

// ---------------- ベンチ：DAG (medium density) ----------------
static void bench_suite_dag_medium() {
  const int N = 16000, M = 128000, TRIALS = 3;
  std::mt19937_64 rng(123456);
  std::uniform_int_distribution<int> Uv(0, N-1), Uw(-8, 32);
  Graph<i64> G(N);
  G.reserve_edges((size_t)M + (size_t)N);

  for (int i = 0; i + 1 < N; ++i) G.add_edge(i, i+1, 1);
  for (int i = 0; i < M; ++i) {
    int u = Uv(rng), v = Uv(rng);
    if (u == v) { v = (v + 1) % N; }
    if (u > v) std::swap(u, v); // DAG 化
    i64 w = Uw(rng);
    G.add_edge(u, v, w);
  }

  // bound 用にターゲット上界（簡単）を設定：t=N-1 の gold を使う
  auto gold = fastbf::bellman_ford_basic<i64>(G, 0);
  i64 ub_t = gold.dist[N-1];

  run_all_9_variants<i64>("--- Benchmark: DAG (medium density) ---", G,
    [=](){
      Options<i64> bf_layered_no;   bf_layered_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> bf_layered_bo;   bf_layered_bo.bound_on_relax = true; bf_layered_bo.target = N-1; bf_layered_bo.cutoff = ub_t;

      Options<i64> spfa_slf_no;     spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true;
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true; spfa_slf_bo.target = N-1; spfa_slf_bo.cutoff = ub_t;

      Options<i64> spfa_fifo_no;    spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false;
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true; spfa_fifo_bo.target = N-1; spfa_fifo_bo.cutoff = ub_t;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    /*s=*/0, TRIALS
  );
}

// ---------------- ベンチ：Dense-Local-Frontier（layered-favored） ----------------
static void bench_dense_local_frontier_layered_favored() {
  const int N = 6000, EXTRA_PER_NODE = 200, TRIALS = 3;
  std::mt19937_64 rng(987654);
  Graph<i64> G(N);
  G.reserve_edges((size_t)N * (EXTRA_PER_NODE + 2));

  for (int i = 0; i + 1 < N; ++i) G.add_edge(i, i+1, 1);
  std::uniform_int_distribution<int> Jump(1, min(100, N-1));
  for (int u = 0; u < N; ++u) for (int k = 0; k < EXTRA_PER_NODE; ++k) {
    int dv = Jump(rng); int v = u + dv; if (v >= N) break; G.add_edge(u, v, (i64)1'000'000);
  }
  const i64 best_upper = (i64)(N - 1);

  run_all_9_variants<i64>("--- Benchmark: Dense-Local-Frontier (layered-favored) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.target = N-1; bf_layered_no.cutoff = best_upper;
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true; spfa_slf_no.target = N-1; spfa_slf_no.cutoff = best_upper;
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false; spfa_fifo_no.target = N-1; spfa_fifo_no.cutoff = best_upper;
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    /*s=*/0, TRIALS
  );
}

// ---------------- ベンチ：Dense-Local-Frontier（stronger） ----------------
static void bench_dense_local_frontier_stronger() {
  const int N = 6000, EXTRA_PER_NODE = 1200, TRIALS = 3;
  std::mt19937_64 rng(246813579);
  Graph<i64> G(N);
  G.reserve_edges((size_t)N * (EXTRA_PER_NODE + 2));

  for (int i = 0; i + 1 < N; ++i) G.add_edge(i, i+1, 1);
  std::uniform_int_distribution<int> Jump(1, min(200, N-1));
  for (int u = 0; u < N; ++u) for (int k = 0; k < EXTRA_PER_NODE; ++k) {
    int dv = Jump(rng); int v = u + dv; if (v >= N) break; G.add_edge(u, v, (i64)1'000'000);
  }
  const i64 best_upper = (i64)(N - 1);

  run_all_9_variants<i64>("--- Benchmark: Dense-Local-Frontier (stronger) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.target = N-1; bf_layered_no.cutoff = best_upper;
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true; spfa_slf_no.target = N-1; spfa_slf_no.cutoff = best_upper;
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false; spfa_fifo_no.target = N-1; spfa_fifo_no.cutoff = best_upper;
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    /*s=*/0, TRIALS
  );
}

// ---------------- ベンチ：Alternating-wave ----------------
static void bench_alternating_wave() {
  const int L = 7000, N = 2 * (L + 1), EXTRA_PER_NODE = 300, TRIALS = 3;
  auto A = [&](int i){ return i; };
  auto B = [&](int i){ return i + L + 1; };

  Graph<i64> G(N);
  G.reserve_edges((size_t)N * (EXTRA_PER_NODE + 3));

  for (int i = 0; i <= L; ++i) {
    G.add_edge(A(i), B(i), 0);
    if (i < L) G.add_edge(B(i), A(i+1), 1);
  }
  for (int i = 0; i < L; ++i) G.add_edge(B(i), B(i+1), 0);

  std::mt19937_64 rng(13579);
  std::uniform_int_distribution<int> Jump(1, 200);
  for (int u = 0; u < N; ++u) for (int k = 0; k < EXTRA_PER_NODE; ++k) {
    int dv = Jump(rng); int v = u + dv; if (v >= N) break; G.add_edge(u, v, (i64)1'000'000);
  }

  const i64 best_upper = (i64)L;
  const int s = A(0), t = A(L);

  run_all_9_variants<i64>("--- Benchmark: Alternating-wave ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.target = t; bf_layered_no.cutoff = best_upper;
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true; spfa_slf_no.target = t; spfa_slf_no.cutoff = best_upper;
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false; spfa_fifo_no.target = t; spfa_fifo_no.cutoff = best_upper;
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    s, TRIALS
  );
}

// ---------------- ベンチ：Wide-Fanin-Levels（layered-favored） ----------------
static void bench_wide_fanin_levels_layered_favored() {
  const int L = 40, K = 120, N = (L+1)*K, TRIALS = 3;
  auto idx = [&](int level, int j){ return level*K + j; };
  Graph<i64> G(N);
  for (int j = 0; j+1 < K; ++j) G.add_edge(idx(0,j), idx(0,j+1), 0);

  std::mt19937_64 rng(20250919);
  std::uniform_int_distribution<int> Wd(1, 3);
  for (int i = 0; i < L; ++i) for (int u = 0; u < K; ++u) for (int v = 0; v < K; ++v) {
    G.add_edge(idx(i,u), idx(i+1,v), (i64)Wd(rng));
  }

  const i64 best_upper = (i64)(L + 2);
  const int s = idx(0,0), t = idx(L,0);

  run_all_9_variants<i64>("--- Benchmark: Wide-Fanin-Levels (layered-favored) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.target = t; bf_layered_no.cutoff = best_upper;
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true; spfa_slf_no.target = t; spfa_slf_no.cutoff = best_upper;
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false; spfa_fifo_no.target = t; spfa_fifo_no.cutoff = best_upper;
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    s, TRIALS
  );
}

// ---------------- ベンチ：Negative-Dense-DAG（layered-favored, DAG） ----------------
static void bench_negative_dense_dag_layered_favored() {
  const int L = 96, K = 96, N = (L + 1) * K, FAR_SPURS = 32, MAX_FAR = 6, TRIALS = 3;
  auto idx = [&](int level, int j){ return level*K + j; };

  Graph<i64> G(N);
  G.reserve_edges( (size_t)L * (size_t)K * (size_t)K + (size_t)N * FAR_SPURS + 100 );

  std::mt19937_64 rng(424242);
  std::uniform_int_distribution<int> NegW(1, 3);
  std::uniform_int_distribution<int> Jump2(2, MAX_FAR);
  std::uniform_int_distribution<int> FarW(100000, 200000);

  for (int i = 0; i < L; ++i) for (int u = 0; u < K; ++u) for (int v = 0; v < K; ++v) {
    G.add_edge(idx(i,u), idx(i+1,v), - (i64)NegW(rng));
  }
  for (int i = 0; i < L-1; ++i) for (int u = 0; u < K; ++u) {
    int from = idx(i,u);
    for (int t = 0; t < FAR_SPURS; ++t) {
      int j = i + Jump2(rng); if (j > L) j = L;
      int v = (u * 37 + t * 17) % K;
      int to = idx(j, v);
      if (from < to) G.add_edge(from, to, (i64)FarW(rng));
    }
  }

  const int s = idx(0,0), t = idx(L,0);

  run_all_9_variants<i64>("--- Benchmark: Negative-Dense-DAG (layered-favored) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.target = t; bf_layered_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true; bf_layered_bo.cutoff = 0;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true; spfa_slf_no.target = t; spfa_slf_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true; spfa_slf_bo.cutoff = 0;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false; spfa_fifo_no.target = t; spfa_fifo_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true; spfa_fifo_bo.cutoff = 0;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    s, TRIALS
  );
}

// ---------------- ベンチ：NonDAG-Neg-Heavy（負辺多・非DAG・負閉路なし） ----------------
static void bench_non_dag_negative_heavy_no_negcycle() {
  const int L = 90, K = 90, N = (L + 1) * K;
  const int BACK_MAX = 6, FAR_SPURS = 24, SAME_SPURS = 16;
  auto idx = [&](int level, int j){ return level*K + j; };

  Graph<i64> G(N);
  G.reserve_edges( (size_t)L*K*K + (size_t)L*K*BACK_MAX + (size_t)N*FAR_SPURS + (size_t)N*SAME_SPURS + 100 );

  std::mt19937_64 rng(777777);
  std::uniform_int_distribution<int> NegW(1, 3);
  std::uniform_int_distribution<int> BackQ(1, BACK_MAX);
  std::uniform_int_distribution<int> FarStep(2, 8);
  std::uniform_int_distribution<int> FarW(80000, 160000);
  std::uniform_int_distribution<int> Jpick(0, K-1);

  for (int i = 0; i < L; ++i) for (int u = 0; u < K; ++u) for (int v = 0; v < K; ++v)
    G.add_edge(idx(i,u), idx(i+1,v), - (i64)NegW(rng));

  for (int i = 0; i <= L; ++i) for (int u = 0; u < K; ++u) {
    int from = idx(i, u);
    for (int t = 0; t < BACK_MAX; ++t) {
      int q = BackQ(rng), j = i - q; if (j < 0) continue;
      int to = idx(j, Jpick(rng));
      i64 w = (i64)(3*q + 1); // 戻しは十分大
      G.add_edge(from, to, w);
    }
  }

  for (int i = 0; i <= L; ++i) for (int u = 0; u < K; ++u) {
    int from = idx(i, u);
    for (int t = 0; t < SAME_SPURS; ++t) {
      int v = Jpick(rng); if (v == u) continue;
      G.add_edge(from, idx(i, v), 0);
    }
  }

  for (int i = 0; i <= L; ++i) for (int u = 0; u < K; ++u) {
    int from = idx(i, u);
    for (int t = 0; t < FAR_SPURS; ++t) {
      int step = FarStep(rng);
      int j = i + step; if (j > L) j = L;
      int to = idx(j, (u*29 + t*7) % K);
      G.add_edge(from, to, (i64)FarW(rng));
    }
  }

  const int s = idx(0, 0), t = idx(L, 0);

  run_all_9_variants<i64>("--- Benchmark: NonDAG-Neg-Heavy (no neg cycles) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.target = t; bf_layered_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true; bf_layered_bo.cutoff = 0;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true; spfa_slf_no.target = t; spfa_slf_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true; spfa_slf_bo.cutoff = 0;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false; spfa_fifo_no.target = t; spfa_fifo_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true; spfa_fifo_bo.cutoff = 0;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    s, /*TRIALS=*/3
  );
}

// ---------------- ベンチ：Zero-Sum-Bidirectional（SPFA-adversarial） ----------------
static void bench_zero_sum_bidirectional_spfa_adversarial() {
  const int L = 64, K = 96, N = (L + 1) * K;
  auto idx = [&](int level, int j){ return level*K + j; };

  Graph<i64> G(N);
  G.reserve_edges( (size_t)2LL * L * K * K + (size_t)(L + 1) * (size_t)K * (size_t)(K - 1) + 100 );

  for (int i = 0; i < L; ++i)
    for (int u = 0; u < K; ++u)
      for (int v = 0; v < K; ++v)
        G.add_edge(idx(i,u), idx(i+1,v), -1);

  for (int i = 0; i < L; ++i)
    for (int v = 0; v < K; ++v)
      for (int u = 0; u < K; ++u)
        G.add_edge(idx(i+1,v), idx(i,u), +1);

  for (int i = 0; i <= L; ++i)
    for (int u = 0; u < K; ++u)
      for (int v = 0; v < K; ++v) if (u != v)
        G.add_edge(idx(i,u), idx(i,v), 0);

  const int s = idx(0, 0), t = idx(L, 0);

  run_all_9_variants<i64>("--- Benchmark: Zero-Sum-Bidirectional (SPFA-adversarial) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.target = t; bf_layered_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true; bf_layered_bo.cutoff = 0;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true; spfa_slf_no.target = t; spfa_slf_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true; spfa_slf_bo.cutoff = 0;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false; spfa_fifo_no.target = t; spfa_fifo_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true; spfa_fifo_bo.cutoff = 0;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    s, /*TRIALS=*/3
  );
}

// ---------------- ベンチ：Adversarial-Potentials-SCC（dense, SPFA-unfriendly） ----------------
static void bench_adversarial_potentials_scc_dense() {
  const int N = 1200, TRIALS = 3;
  const int SLACK_P0 = 85;

  std::mt19937_64 rng(20250918);
  std::uniform_int_distribution<int> H(0, 1'000'000);
  std::uniform_int_distribution<int> P(0, 99);

  std::vector<i64> h(N);
  for (int i = 0; i < N; ++i) h[i] = H(rng);

  Graph<i64> G(N);
  const size_t M = (size_t)N * (size_t)(N - 1);
  G.reserve_edges(M);

  for (int u = 0; u < N; ++u) {
    for (int v = 0; v < N; ++v) if (u != v) {
      int s = (P(rng) < SLACK_P0) ? 0 : 1;
      i64 w = (i64)h[v] - (i64)h[u] + (i64)s;
      G.add_edge(u, v, w);
    }
  }

  const int s = 0;

  run_all_9_variants<i64>("--- Benchmark: Adversarial-Potentials-SCC (dense, SPFA-unfriendly) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true; bf_layered_bo.cutoff = 0;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true;
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true; spfa_slf_bo.cutoff = 0;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false;
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true; spfa_fifo_bo.cutoff = 0;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    s, TRIALS
  );
}

// ---------------- ベンチ：Cascading-Hub Multi-Path（SPFA-unfriendly） ----------------
static void bench_cascading_hub_multipath_spfa_unfriendly() {
  /*
    q の距離を段階的（-1,-2,...) に改善させ、その度に q→w_j（Z 本）を毎回全走査させる。
    SPFA（特に SLF/LLL）に再投入オーバーヘッドを負わせる狙い。
    DAG 構造で負閉路なし。
  */
  const int R = 256;     // q 改善段数
  const int Z = 120000;  // q の out-degree
  const int C = 2000;
  const int N = 2 + R + Z;
  const int s = 0, q = 1;

  Graph<i64> G(N);
  G.reserve_edges((size_t)(2*R + Z));

  for (int i = 0; i < R; ++i) G.add_edge(s, 2+i, (i64)i * C);
  for (int i = 0; i < R; ++i) G.add_edge(2+i, q, - (i64)(i * C + i + 1));
  for (int j = 0; j < Z; ++j)  G.add_edge(q, 2+R+j, 0);

  run_all_9_variants<i64>("--- Benchmark: Cascading-Hub Multi-Path (SPFA-unfriendly) ---", G,
    [=](){
      Options<i64> bf_layered_no; bf_layered_no.cutoff = fastbf::POS_CAP<i64>();
      Options<i64> bf_layered_bo = bf_layered_no; bf_layered_bo.bound_on_relax = true; bf_layered_bo.cutoff = 0;

      Options<i64> spfa_slf_no; spfa_slf_no.use_slf = true; spfa_slf_no.use_lll = true;
      Options<i64> spfa_slf_bo = spfa_slf_no; spfa_slf_bo.bound_on_relax = true; spfa_slf_bo.cutoff = 0;

      Options<i64> spfa_fifo_no; spfa_fifo_no.use_slf = false; spfa_fifo_no.use_lll = false;
      Options<i64> spfa_fifo_bo = spfa_fifo_no; spfa_fifo_bo.bound_on_relax = true; spfa_fifo_bo.cutoff = 0;

      Options<i64> auto_no = bf_layered_no;
      Options<i64> auto_bo = bf_layered_bo;

      return std::tuple{bf_layered_no, bf_layered_bo,
                        spfa_slf_no, spfa_slf_bo,
                        spfa_fifo_no, spfa_fifo_bo,
                        auto_no, auto_bo};
    },
    s, /*TRIALS=*/3
  );
}

int main() {
  using namespace std;
  cout << "fast_bellman_ford.hpp self-test & benchmark\n";

  // ---------- エッジケース群 ----------
  {
    Graph<i64> G(1);
    auto Rb = fastbf::bellman_ford_basic<i64>(G, 0);
    auto Rf = fastbf::bellman_ford_layered<i64>(G, 0);
    auto Rs = fastbf::spfa_fast<i64>(G, 0);
    expect(!Rb.has_negative_cycle && !Rf.has_negative_cycle && !Rs.has_negative_cycle, "single node neg-cycle");
    expect(Rb.dist[0]==0 && Rf.dist[0]==0 && Rs.dist[0]==0, "single node dist");
  }
  {
    Graph<i64> G(4);
    G.add_edge(0,1,5);
    G.add_edge(1,2,3);
    auto Rb = fastbf::bellman_ford_basic<i64>(G, 0);
    auto Rf = fastbf::bellman_ford_layered<i64>(G, 0);
    auto Rs = fastbf::spfa_fast<i64>(G, 0);
    expect(Rb.dist[0]==0, "disconnected d0");
    expect(Rb.dist[1]==5 && Rf.dist[1]==5 && Rs.dist[1]==5, "disconnected d1");
    expect(Rb.dist[2]==8 && Rf.dist[2]==8 && Rs.dist[2]==8, "disconnected d2");
    const i64 INF = fastbf::POS_CAP<i64>();
    expect(Rb.dist[3]>=INF && Rf.dist[3]>=INF && Rs.dist[3]>=INF, "disconnected d3 INF");
    verify_feasible_basic(G, Rb); verify_feasible_basic(G, Rf); verify_feasible_basic(G, Rs);
  }
  {
    Graph<i64> G(3);
    G.add_edge(0,1,2);
    G.add_edge(1,2,-5);
    G.add_edge(0,2,10);
    auto Rb = fastbf::bellman_ford_basic<i64>(G, 0);
    auto Rf = fastbf::bellman_ford_layered<i64>(G, 0);
    auto Rs = fastbf::spfa_fast<i64>(G, 0);
    expect(!Rb.has_negative_cycle && !Rf.has_negative_cycle && !Rs.has_negative_cycle, "no neg cycle");
    expect(Rb.dist[2]==-3 && Rf.dist[2]==-3 && Rs.dist[2]==-3, "negative edge shortest");
    verify_feasible_basic(G, Rb); verify_feasible_basic(G, Rf); verify_feasible_basic(G, Rs);
  }
  {
    Graph<i64> G(3);
    G.add_edge(0,0,0);
    G.add_edge(0,1,4);
    G.add_edge(0,1,1);
    G.add_edge(1,2,0);
    auto R = fastbf::bellman_ford_layered<i64>(G, 0);
    expect(!R.has_negative_cycle, "self loop 0");
    expect(R.dist[1]==1 && R.dist[2]==1, "dup/zero");
  }
  {
    Graph<i64> G(3);
    G.add_edge(0,1,1);
    G.add_edge(1,2,-2);
    G.add_edge(2,0,-2);
    auto Rb = fastbf::bellman_ford_basic<i64>(G, 0, true);
    auto Rs = fastbf::spfa_fast<i64>(G, 0);
    expect(Rb.has_negative_cycle, "basic detects neg cycle");
    expect(Rs.has_negative_cycle,  "spfa detects neg cycle");
  }
  {
    Graph<i64> G(5);
    G.add_edge(0,1,1);
    G.add_edge(1,2,1);
    G.add_edge(2,4,1);
    G.add_edge(0,3,100);
    G.add_edge(3,4,100);
    Options<i64> opt;
    opt.target = 4;
    opt.cutoff = 50;
    opt.upper_limit.assign(5, fastbf::POS_CAP<i64>());
    opt.upper_limit[3] = 10;
    auto Rlayer = fastbf::bellman_ford_layered<i64>(G, 0, opt);
    auto Rbase  = fastbf::bellman_ford_basic<i64>(G, 0);
    expect(!Rlayer.has_negative_cycle, "bounded no neg");
    expect(Rlayer.dist == Rbase.dist,  "bounded equals basic");
  }

  cout << "[OK] Edge-case tests passed.\n";

  // ---------- ベンチ群（全て 9 通り計測） ----------
  bench_suite_dag_medium();
  bench_dense_local_frontier_layered_favored();
  bench_dense_local_frontier_stronger();
  bench_alternating_wave();
  bench_wide_fanin_levels_layered_favored();
  bench_negative_dense_dag_layered_favored();
  bench_non_dag_negative_heavy_no_negcycle();
  bench_zero_sum_bidirectional_spfa_adversarial();
  bench_adversarial_potentials_scc_dense();
  bench_cascading_hub_multipath_spfa_unfriendly();

  cout << "[DONE] Benchmarks.\n";
  return 0;
}
#endif


// 実行結果 atcoder
// fast_bellman_ford.hpp self-test & benchmark
// [OK] Edge-case tests passed.
// --- Benchmark: DAG (medium density) ---
// BF basic                       avg=15264 us, best=15058 us
// BF layered (no bound)          avg=15884 us, best=15309 us
// BF layered (bound)             avg=77 us, best=43 us
// SPFA+SLF/LLL (no bound)        avg=8152 us, best=8091 us
// SPFA+SLF/LLL (bound)           avg=20 us, best=11 us
// SPFA FIFO (no bound)           avg=16258 us, best=16220 us
// SPFA FIFO (bound)              avg=22 us, best=13 us
// auto (no bound)                avg=15544 us, best=15515 us
// auto (bound)                   avg=54 us, best=51 us
// --- Benchmark: Dense-Local-Frontier (layered-favored) ---
// BF basic                       avg=7868 us, best=7529 us
// BF layered (no bound)          avg=7439 us, best=7167 us
// BF layered (bound)             avg=7376 us, best=7197 us
// SPFA+SLF/LLL (no bound)        avg=3976 us, best=3940 us
// SPFA+SLF/LLL (bound)           avg=4153 us, best=4130 us
// SPFA FIFO (no bound)           avg=3923 us, best=3827 us
// SPFA FIFO (bound)              avg=4083 us, best=4051 us
// auto (no bound)                avg=7225 us, best=7022 us
// auto (bound)                   avg=7524 us, best=7369 us
// --- Benchmark: Dense-Local-Frontier (stronger) ---
// BF basic                       avg=49215 us, best=49086 us
// BF layered (no bound)          avg=40946 us, best=40560 us
// BF layered (bound)             avg=41933 us, best=41627 us
// SPFA+SLF/LLL (no bound)        avg=22004 us, best=21924 us
// SPFA+SLF/LLL (bound)           avg=23247 us, best=22994 us
// SPFA FIFO (no bound)           avg=22028 us, best=21919 us
// SPFA FIFO (bound)              avg=23337 us, best=23225 us
// auto (no bound)                avg=41031 us, best=40966 us
// auto (bound)                   avg=41941 us, best=41811 us
// --- Benchmark: Alternating-wave ---
// BF basic                       avg=38991 us, best=38809 us
// BF layered (no bound)          avg=28632 us, best=28241 us
// BF layered (bound)             avg=29241 us, best=28918 us
// SPFA+SLF/LLL (no bound)        avg=15338 us, best=15196 us
// SPFA+SLF/LLL (bound)           avg=15673 us, best=15516 us
// SPFA FIFO (no bound)           avg=15569 us, best=15412 us
// SPFA FIFO (bound)              avg=15822 us, best=15632 us
// auto (no bound)                avg=28567 us, best=28488 us
// auto (bound)                   avg=29367 us, best=29157 us
// --- Benchmark: Wide-Fanin-Levels (layered-favored) ---
// BF basic                       avg=3667 us, best=3489 us
// BF layered (no bound)          avg=2983 us, best=2774 us
// BF layered (bound)             avg=3225 us, best=3149 us
// SPFA+SLF/LLL (no bound)        avg=1782 us, best=1742 us
// SPFA+SLF/LLL (bound)           avg=1962 us, best=1943 us
// SPFA FIFO (no bound)           avg=1764 us, best=1733 us
// SPFA FIFO (bound)              avg=1881 us, best=1875 us
// auto (no bound)                avg=3019 us, best=2926 us
// auto (bound)                   avg=3273 us, best=3149 us
// --- Benchmark: Negative-Dense-DAG (layered-favored) ---
// BF basic                       avg=7678 us, best=7323 us
// BF layered (no bound)          avg=10677 us, best=10331 us
// BF layered (bound)             avg=6963 us, best=6713 us
// SPFA+SLF/LLL (no bound)        avg=4187 us, best=4069 us
// SPFA+SLF/LLL (bound)           avg=3958 us, best=3909 us
// SPFA FIFO (no bound)           avg=4947 us, best=4919 us
// SPFA FIFO (bound)              avg=3936 us, best=3844 us
// auto (no bound)                avg=10586 us, best=10336 us
// auto (bound)                   avg=6605 us, best=6355 us
// --- Benchmark: NonDAG-Neg-Heavy (no neg cycles) ---
// BF basic                       avg=7252 us, best=6801 us
// BF layered (no bound)          avg=11137 us, best=10770 us
// BF layered (bound)             avg=8228 us, best=8198 us
// SPFA+SLF/LLL (no bound)        avg=4535 us, best=4484 us
// SPFA+SLF/LLL (bound)           avg=4469 us, best=4420 us
// SPFA FIFO (no bound)           avg=5157 us, best=5127 us
// SPFA FIFO (bound)              avg=4826 us, best=4526 us
// auto (no bound)                avg=11187 us, best=11017 us
// auto (bound)                   avg=8057 us, best=7986 us
// --- Benchmark: Zero-Sum-Bidirectional (SPFA-adversarial) ---
// BF basic                       avg=12219 us, best=12067 us
// BF layered (no bound)          avg=11816 us, best=11621 us
// BF layered (bound)             avg=12169 us, best=11998 us
// SPFA+SLF/LLL (no bound)        avg=6348 us, best=6265 us
// SPFA+SLF/LLL (bound)           avg=6456 us, best=6418 us
// SPFA FIFO (no bound)           avg=6110 us, best=6062 us
// SPFA FIFO (bound)              avg=6599 us, best=6584 us
// auto (no bound)                avg=11490 us, best=11420 us
// auto (bound)                   avg=11971 us, best=11901 us
// --- Benchmark: Adversarial-Potentials-SCC (dense, SPFA-unfriendly) ---
// BF basic                       avg=9575 us, best=9258 us
// BF layered (no bound)          avg=8586 us, best=8305 us
// BF layered (bound)             avg=5623 us, best=5280 us
// SPFA+SLF/LLL (no bound)        avg=4343 us, best=4312 us
// SPFA+SLF/LLL (bound)           avg=3020 us, best=2926 us
// SPFA FIFO (no bound)           avg=4326 us, best=4305 us
// SPFA FIFO (bound)              avg=2951 us, best=2797 us
// auto (no bound)                avg=8549 us, best=8007 us
// auto (bound)                   avg=5565 us, best=5442 us
// --- Benchmark: Cascading-Hub Multi-Path (SPFA-unfriendly) ---
// BF basic                       avg=1003 us, best=836 us
// BF layered (no bound)          avg=1492 us, best=1362 us
// BF layered (bound)             avg=322 us, best=291 us
// SPFA+SLF/LLL (no bound)        avg=442024 us, best=441760 us
// SPFA+SLF/LLL (bound)           avg=97 us, best=72 us
// SPFA FIFO (no bound)           avg=1679 us, best=1629 us
// SPFA FIFO (bound)              avg=75 us, best=73 us
// auto (no bound)                avg=441968 us, best=441922 us
// auto (bound)                   avg=100 us, best=73 us
// [DONE] Benchmarks.
