// xopt.cpp - deep exact cone rebuild for Santa 2026 (own code).
// Input blocks: "P <id> <k> s.. <m> t.. <limit> <regs> <n>" + n lines "<op> <i> <j> <d>"
// Output: "R <id> <old> <new>" + program lines for each shortened puzzle.
// Move: for a node v with exclusive cone of size c, rebuild the value of v from all values outside the cone
// (and not depending on v) with an EXHAUSTIVE search of depth <= min(c-1, D), then schedule into registers.
#include <bits/stdc++.h>
using namespace std;
typedef unsigned long long u64;
typedef unsigned __int128 u128;
static double now() { return chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count(); }
static u64 LIMIT;

struct Step { char op; u64 x, y, r; };
struct Inv { u64 w; char op; bool wFirst; };   // T = w op x (wFirst) or x op w
// all w with T = w op x or T = x op w. additive entries first (their count is returned in nAdd)
static inline int inverses(u64 T, u64 x, Inv* o, int& nAdd) {
    int n = 0;
    if (T > x) o[n++] = {T - x, '+', true};
    if (x <= LIMIT - T) o[n++] = {T + x, '-', true};
    if (x > T) o[n++] = {x - T, '-', false};
    nAdd = n;
    if (x > 1 && T % x == 0) o[n++] = {T / x, '*', true};
    if (x > 1) { u128 p = (u128)T * x; if (p <= LIMIT) o[n++] = {(u64)p, '/', true}; }
    if (x > T && x % T == 0 && x / T > 1) o[n++] = {x / T, '/', false};
    return n;
}
static inline Step mk(const Inv& iv, u64 x, u64 T) { return iv.wFirst ? Step{iv.op, iv.w, x, T} : Step{iv.op, x, iv.w, T}; }
static inline u64 hsh(u64 x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 29; return x; }

struct Table {       // u64 -> int, keys > 0
    vector<u64> k; vector<int> v; u64 mask = 0; size_t cnt = 0; vector<u64> bloom; u64 bmask = 0;
    void init(size_t n) {
        size_t cap = 1024; while (cap < 2 * n + 64) cap <<= 1;
        k.assign(cap, 0); v.assign(cap, -1); mask = cap - 1; cnt = 0;
        size_t bb = 1 << 16; while (bb < 16 * n) bb <<= 1; bloom.assign(bb / 64, 0); bmask = bb - 1;
    }
    inline bool put(u64 key, int val) {
        u64 h = hsh(key), i = h & mask;
        for (;;) { u64 kk = k[i]; if (kk == key) return false; if (!kk) break; i = (i + 1) & mask; }
        k[i] = key; v[i] = val; cnt++; u64 b = (h >> 20) & bmask; bloom[b >> 6] |= 1ULL << (b & 63); return true;
    }
    inline int get(u64 key) const {
        u64 h = hsh(key); u64 b = (h >> 20) & bmask;
        if (!(bloom[b >> 6] >> (b & 63) & 1)) return -1;
        u64 i = h & mask;
        for (;;) { u64 kk = k[i]; if (kk == key) return v[i]; if (!kk) return -1; i = (i + 1) & mask; }
    }
};

struct Searcher {
    vector<u64> P; Table H0, H1; vector<Step> s1; vector<u64> v1;
    double deadline = 1e300;
    inline bool inP(u64 x) const { return H0.get(x) >= 0; }
    void add1(u64 r, char op, u64 a, u64 b) { if (H0.get(r) >= 0) return; if (H1.put(r, (int)s1.size())) { s1.push_back({op, a, b, r}); v1.push_back(r); } }
    void setPool(const vector<u64>& p) {
        P = p; sort(P.begin(), P.end()); P.erase(unique(P.begin(), P.end()), P.end());
        size_t n = P.size(); H0.init(n); for (size_t i = 0; i < n; i++) H0.put(P[i], (int)i);
        H1.init(n * n * 2 + 16); s1.clear(); v1.clear();
        for (size_t i = 0; i < n; i++) for (size_t j = i; j < n; j++) {
            u64 a = P[i], b = P[j];   // a <= b
            { u128 s = (u128)a + b; if (s <= LIMIT) add1((u64)s, '+', a, b); }
            { u128 s = (u128)a * b; if (s <= LIMIT) add1((u64)s, '*', a, b); }
            if (b > a) add1(b - a, '-', b, a);
            if (a > 1 && b % a == 0 && b != a) add1(b / a, '/', b, a);
            if (a == b && !inP(1)) add1(1, '/', a, a);
        }
    }
    inline int c01(u64 x, vector<Step>& o) const { if (inP(x)) return 0; int id = H1.get(x); if (id < 0) return -1; o.push_back(s1[id]); return 1; }
    // exact depth 2 for T (T not in P, not in S1): T = u op p, u in S1 ; or u+u, u*u
    bool d2(u64 T, vector<Step>& out) {
        Inv iv[6]; int na;
        for (u64 x : P) { int c = inverses(T, x, iv, na); for (int q = 0; q < c; q++) { int u = H1.get(iv[q].w); if (u >= 0) { out.push_back(s1[u]); out.push_back(mk(iv[q], x, T)); return true; } } }
        if (T % 2 == 0) { int u = H1.get(T / 2); if (u >= 0) { out.push_back(s1[u]); out.push_back({'+', T / 2, T / 2, T}); return true; } }
        u64 s = (u64)sqrtl((long double)T); while (s * s > T) s--; while ((s + 1) * (s + 1) <= T) s++;
        if (s > 1 && s * s == T) { int u = H1.get(s); if (u >= 0) { out.push_back(s1[u]); out.push_back({'*', s, s, T}); return true; } }
        return false;
    }
    // T = a op b with a, b in S1 (mulOnly: only multiplicative joins)
    bool d3pair(u64 T, vector<Step>& out, bool mulOnly) {
        Inv iv[6]; int na;
        for (size_t a = 0; a < v1.size(); a++) {
            u64 x = v1[a]; int c = inverses(T, x, iv, na);
            for (int q = mulOnly ? na : 0; q < c; q++) { int u = H1.get(iv[q].w); if (u >= 0) { out.push_back(s1[a]); if (iv[q].w != x) out.push_back(s1[u]); out.push_back(mk(iv[q], x, T)); return true; } }
        }
        return false;
    }
    // chain: T = W op p, W of cost <= left (left >= 2); two consecutive additive steps are skipped (other shapes cover them)
    bool chain(u64 T, int left, bool lastAdd, vector<Step>& out) {
        Inv iv[6]; int na;
        for (u64 x : P) {
            int c = inverses(T, x, iv, na);
            for (int q = lastAdd ? na : 0; q < c; q++) {
                u64 w = iv[q].w; if (inP(w)) continue; bool add = q < na;
                size_t mark = out.size(); bool ok = false; int u0 = H1.get(w);
                if (u0 >= 0) { out.push_back(s1[u0]); ok = true; }
                else if (left == 2) {
                    Inv iv2[6]; int na2;
                    for (u64 y : P) { int c2 = inverses(w, y, iv2, na2); for (int q2 = add ? na2 : 0; q2 < c2 && !ok; q2++) { int u = H1.get(iv2[q2].w); if (u >= 0) { out.push_back(s1[u]); out.push_back(mk(iv2[q2], y, w)); ok = true; } } if (ok) break; }
                } else {
                    if (left == 3) ok = d3pair(w, out, add);
                    if (!ok) { out.resize(mark); ok = chain(w, left - 1, add, out); }
                }
                if (ok) { out.push_back(mk(iv[q], x, T)); return true; }
                out.resize(mark);
            }
        }
        return false;
    }
    // exhaustive search for T with at most maxc new instructions (maxc <= 4)
    bool find(u64 T, int maxc, vector<Step>& out) {
        out.clear(); Inv iv[6]; int na;
        if (maxc < 1 || inP(T)) return false;
        if (c01(T, out) == 1) return true;
        if (maxc < 2) return false;
        if (d2(T, out)) return true;
        if (maxc < 3) return false;
        if (d3pair(T, out, false)) return true;
        out.clear();
        if (chain(T, 2, false, out)) return true;
        out.clear();
        if (maxc < 4) return false;
        // (i) T = l op W, l in S1, W = u op p (u in S1)
        for (size_t a = 0; a < v1.size(); a++) {
            if ((a & 1023) == 0 && now() > deadline) return false;
            u64 x = v1[a]; int c = inverses(T, x, iv, na);
            for (int q = 0; q < c; q++) {
                u64 w = iv[q].w; if (inP(w) || H1.get(w) >= 0) continue;
                out.clear(); out.push_back(s1[a]);
                if (d2(w, out)) { out.push_back(mk(iv[q], x, T)); return true; }
            }
        }
        out.clear();
        // (ii)+(iii) T = W op p with W of cost 3
        if (chain(T, 3, false, out)) return true;
        out.clear();
        if (T % 2 == 0) { u64 w = T / 2; vector<Step> t; if (!inP(w) && H1.get(w) < 0 && find(w, 3, t)) { out = t; out.push_back({'+', w, w, T}); return true; } }
        out.clear();
        return false;
    }
};

struct Node { char op; int a, b; u64 v; };
struct Puzzle { string id; vector<u64> starts, targets; u64 limit; int regs; vector<array<int, 4>> prog; vector<char> progOp; };
struct Graph { vector<Node> nodes; vector<int> order; };

static u64 apply(char op, u64 a, u64 b) {
    switch (op) {
        case '+': { u128 r = (u128)a + b; return r <= LIMIT ? (u64)r : 0; }
        case '-': return a > b ? a - b : 0;
        case '*': { u128 r = (u128)a * b; return r <= LIMIT ? (u64)r : 0; }
        case '/': return (b && a % b == 0) ? a / b : 0;
    }
    return 0;
}
static Graph build(const Puzzle& P) {
    Graph g; vector<int> reg(P.regs, -1);
    for (size_t k = 0; k < P.starts.size(); k++) { g.nodes.push_back({0, -1, -1, P.starts[k]}); reg[k] = (int)k; }
    for (size_t k = 0; k < P.prog.size(); k++) {
        auto& in = P.prog[k]; int a = reg[in[1]], b = reg[in[2]];
        u64 v = apply(P.progOp[k], g.nodes[a].v, g.nodes[b].v);
        g.nodes.push_back({P.progOp[k], a, b, v}); int id = (int)g.nodes.size() - 1; g.order.push_back(id); reg[in[3]] = id;
    }
    return g;
}
// keep the first producer of every value, redirect operands to it, drop what no target needs; compact ids
static Graph prune(const Graph& g, const Puzzle& P) {
    int N = g.nodes.size(), K = P.starts.size(); unordered_map<u64, int> first; vector<int> canon(N);
    for (int k = 0; k < K; k++) { auto it = first.find(g.nodes[k].v); if (it == first.end()) { first[g.nodes[k].v] = k; canon[k] = k; } else canon[k] = it->second; }
    Graph h; h.nodes = g.nodes;
    for (int id : g.order) { Node& n = h.nodes[id]; n.a = canon[n.a]; n.b = canon[n.b]; auto it = first.find(n.v); if (it == first.end()) { first[n.v] = id; canon[id] = id; } else canon[id] = it->second; }
    vector<char> need(N, 0); vector<int> st;
    for (u64 t : P.targets) { int id = first.at(t); if (!need[id]) { need[id] = 1; st.push_back(id); } }
    while (!st.empty()) { int id = st.back(); st.pop_back(); if (id < K) continue; for (int c : {h.nodes[id].a, h.nodes[id].b}) if (!need[c]) { need[c] = 1; st.push_back(c); } }
    vector<int> rem(N, -1); Graph o;
    for (int k = 0; k < K; k++) { rem[k] = k; o.nodes.push_back(h.nodes[k]); }
    for (int id : g.order) if (need[id] && canon[id] == id && id >= K) { rem[id] = o.nodes.size(); Node n = h.nodes[id]; n.a = rem[n.a]; n.b = rem[n.b]; o.nodes.push_back(n); o.order.push_back(rem[id]); }
    return o;
}
// register allocation for a given order; false when it needs more registers than available
static bool emit(const Graph& g, const Puzzle& P, int regs, vector<array<int, 4>>& out, vector<char>& outOp) {
    int N = g.nodes.size(); vector<int> last(N, -1);
    for (int k = 0; k < (int)g.order.size(); k++) { int id = g.order[k]; for (int c : {g.nodes[id].a, g.nodes[id].b}) last[c] = k; }
    vector<int> reg(N, -1), holder(regs, -1), freeRegs;
    for (int r = regs - 1; r >= (int)P.starts.size(); r--) freeRegs.push_back(r);
    for (int k = 0; k < (int)P.starts.size(); k++) { reg[k] = k; holder[k] = k; if (last[k] < 0) { freeRegs.push_back(k); holder[k] = -1; } }
    out.clear(); outOp.clear();
    for (int k = 0; k < (int)g.order.size(); k++) {
        int id = g.order[k]; const Node& n = g.nodes[id]; int ra = reg[n.a], rb = reg[n.b];
        for (int c : {n.a, n.b}) if (last[c] == k && reg[c] >= 0 && holder[reg[c]] == c) { freeRegs.push_back(reg[c]); holder[reg[c]] = -1; }
        if (freeRegs.empty()) return false;
        int d = freeRegs.back(); freeRegs.pop_back();
        out.push_back({0, ra, rb, d}); outOp.push_back(n.op); reg[id] = d; holder[d] = id;
        if (last[id] < 0) { freeRegs.push_back(d); holder[d] = -1; }
    }
    return true;
}
// peak number of registers for the order (same accounting as emit)
static int pressure(const Graph& g, int K) {
    int N = g.nodes.size(); vector<int> last(N, -1);
    for (int k = 0; k < (int)g.order.size(); k++) { int id = g.order[k]; for (int c : {g.nodes[id].a, g.nodes[id].b}) last[c] = k; }
    vector<int> die(g.order.size() + 1, 0); int cur = 0, mx = 0;
    for (int k = 0; k < K; k++) if (last[k] >= 0) { cur++; die[last[k]]++; }
    mx = cur;
    for (int k = 0; k < (int)g.order.size(); k++) { int id = g.order[k]; cur -= die[k]; mx = max(mx, cur + 1); if (last[id] >= 0) { cur++; die[last[id]]++; } }
    return mx;
}
// randomized greedy list scheduling
static bool listSchedule(Graph& g, int K, mt19937_64& rng, double noise, const vector<double>& pos) {
    int N = g.nodes.size(); vector<vector<int>> us(N); vector<int> indeg(N, 0), rem(N, 0);
    for (int id : g.order) { const Node& n = g.nodes[id]; us[n.a].push_back(id); rem[n.a]++; if (n.a >= K) indeg[id]++; if (n.b != n.a) { us[n.b].push_back(id); rem[n.b]++; if (n.b >= K) indeg[id]++; } }
    vector<int> ready; for (int id : g.order) if (!indeg[id]) ready.push_back(id);
    vector<int> ord; ord.reserve(g.order.size()); uniform_real_distribution<double> U(0, 1);
    while (!ready.empty()) {
        int bi = -1; double bs = 1e18;
        for (int i = 0; i < (int)ready.size(); i++) {
            int id = ready[i]; const Node& n = g.nodes[id]; int fr = 0;
            if (rem[n.a] == 1) fr++;
            if (n.b != n.a && rem[n.b] == 1) fr++;
            double s = (us[id].empty() ? 0 : 1) - fr + 1e-4 * pos[id] + noise * U(rng);
            if (s < bs) { bs = s; bi = i; }
        }
        int id = ready[bi]; ready[bi] = ready.back(); ready.pop_back(); ord.push_back(id);
        const Node& n = g.nodes[id]; rem[n.a]--; if (n.b != n.a) rem[n.b]--;
        for (int y : us[id]) if (--indeg[y] == 0) ready.push_back(y);
    }
    if (ord.size() != g.order.size()) return false;
    g.order = ord; return true;
}
static bool reorder(Graph& g, const vector<double>& prio, int K) {
    int N = g.nodes.size(); vector<int> indeg(N, 0); vector<vector<int>> users(N);
    for (int id : g.order) { const Node& n = g.nodes[id]; if (n.a >= K) { indeg[id]++; users[n.a].push_back(id); } if (n.b >= K && n.b != n.a) { indeg[id]++; users[n.b].push_back(id); } }
    priority_queue<pair<double, int>, vector<pair<double, int>>, greater<>> pq;
    for (int id : g.order) if (!indeg[id]) pq.push({prio[id], id});
    vector<int> ord;
    while (!pq.empty()) { int id = pq.top().second; pq.pop(); ord.push_back(id); for (int u : users[id]) if (!--indeg[u]) pq.push({prio[u], u}); }
    if (ord.size() != g.order.size()) return false;
    g.order = ord; return true;
}
// try to fit g into regs registers; g.order is updated on success
static bool fit(Graph& g, const Puzzle& P, int regs, mt19937_64& rng, int tries) {
    int K = P.starts.size();
    if (pressure(g, K) <= regs) return true;
    vector<double> pos(g.nodes.size(), 0); for (int i = 0; i < (int)g.order.size(); i++) pos[g.order[i]] = i;
    Graph best = g; int bp = pressure(g, K);
    for (int t = 0; t < tries; t++) {
        Graph c = g; if (!listSchedule(c, K, rng, t == 0 ? 0 : 0.6 * (t % 5), pos)) continue;
        int p = pressure(c, K); if (p < bp) { bp = p; best = c; if (bp <= regs) break; }
    }
    if (bp <= regs) { g = best; return true; }
    return false;
}
// sched.h - register-pressure minimizing scheduler (simulated annealing over topological orders)
// pressure at step t = 1 + number of values defined before t and used after t
struct Sched {
    long long PW = 100000, PW2 = 1; int n = 0, K = 0;  // n computed nodes; ids 0..K-1 are start values, K..K+n-1 computed
    vector<int> A, B;                 // operands per node id (size K+n)
    vector<vector<int>> users;
    vector<int> ord, pos;             // ord[i] = node id at step i ; pos[id] (starts: -1)
    vector<int> diff, lastUse;
    void init(const Graph& g, int K_) {
        K = K_; n = g.order.size(); int N = g.nodes.size(); A.assign(N, -1); B.assign(N, -1); users.assign(N, {});
        ord = g.order; pos.assign(N, -1);
        for (int i = 0; i < n; i++) pos[ord[i]] = i;
        for (int id : ord) { A[id] = g.nodes[id].a; B[id] = g.nodes[id].b; users[A[id]].push_back(id); if (B[id] != A[id]) users[B[id]].push_back(id); }
        diff.assign(n + 2, 0); lastUse.assign(N, -1);
    }
    // cost: peak * BIG + sum over steps of max(0, p_t - theta)^2
    long long eval(int theta, int& peak) {
        fill(diff.begin(), diff.end(), 0);
        int N = A.size();
        for (int v = 0; v < N; v++) {
            if (v >= K && pos[v] < 0) continue;
            int l = -1; for (int u : users[v]) l = max(l, pos[u]);
            int d = v < K ? -1 : pos[v];
            if (l > d + 1) { diff[d + 1]++; diff[l]--; }
        }
        long long c = 0; int cur = 0; peak = 0;
        for (int t = 0; t < n; t++) { cur += diff[t]; int p = cur + 1; if (p > peak) peak = p; if (p > theta) c += (long long)(p - theta) * (p - theta); }
        return c * PW2 + PW * peak;
    }
    void window(int x, int& lo, int& hi) {
        lo = 0; hi = n - 1;
        if (A[x] >= K) lo = max(lo, pos[A[x]] + 1);
        if (B[x] >= K) lo = max(lo, pos[B[x]] + 1);
        for (int u : users[x]) hi = min(hi, pos[u] - 1);
    }
    void shift(int i, int j) {   // move ord[i] to position j
        int x = ord[i];
        if (j > i) { for (int t = i; t < j; t++) { ord[t] = ord[t + 1]; pos[ord[t]] = t; } }
        else { for (int t = i; t > j; t--) { ord[t] = ord[t - 1]; pos[ord[t]] = t; } }
        ord[j] = x; pos[x] = j;
    }
    // anneal; returns best peak; stops early when peak <= goal
    int run(mt19937_64& rng, long long iters, int goal, int theta, double T0, double T1) {
        int peak; long long cur = eval(theta, peak); vector<int> bestOrd = ord; long long best = cur; int bestPeak = peak;
        if (n < 3) return peak;
        uniform_real_distribution<double> U(0, 1);
        for (long long it = 0; it < iters && bestPeak > goal; it++) {
            double T = T0 * pow(T1 / T0, (double)it / iters);
            int i = rng() % n; int x = ord[i]; int lo, hi; window(x, lo, hi);
            if (hi <= lo) continue;
            int j = lo + rng() % (hi - lo + 1); if (j == i) continue;
            shift(i, j);
            int p2; long long c2 = eval(theta, p2);
            if (c2 <= cur || U(rng) < exp((double)(cur - c2) / T)) { cur = c2; if (c2 < best) { best = c2; bestOrd = ord; bestPeak = p2; } }
            else shift(j, i);
        }
        ord = bestOrd; for (int i = 0; i < n; i++) pos[ord[i]] = i;
        return bestPeak;
    }
};
// greedy list schedules first, then annealing over topological orders
static bool fitSA(Graph& g, const Puzzle& P, int regs, mt19937_64& rng, long long saIters) {
    if (fit(g, P, regs, rng, 100)) return true;
    if (saIters <= 0) return false;
    int K = P.starts.size();
    for (int rep = 0; rep < 2; rep++) {
        Sched S; S.init(g, K); S.PW = 100; int pk = S.run(rng, saIters, regs, regs - 2, rep ? 20 : 60, rep ? 0.5 : 1);
        if (pk <= regs) { Graph c = g; c.order = S.ord; if (pressure(c, K) <= regs) { g = c; return true; } }
    }
    return false;
}
// ---- depth 5: joins over the sorted table of all values of at most 2 instructions (packed value << 2 | cost)
static inline void comb2(u64 a, u64 b, int cost, vector<u64>& out) {
    if (a > b) swap(a, b);
    { u128 s = (u128)a + b; if (s <= LIMIT) out.push_back(((u64)s << 2) | cost); }
    { u128 s = (u128)a * b; if (s <= LIMIT) out.push_back(((u64)s << 2) | cost); }
    if (b > a) out.push_back(((b - a) << 2) | cost);
    if (a > 1 && b % a == 0) out.push_back(((b / a) << 2) | cost);
}
static void uniq2(vector<u64>& v) {
    sort(v.begin(), v.end()); size_t w = 0;
    for (size_t i = 0; i < v.size(); i++) if (w == 0 || (v[w - 1] >> 2) != (v[i] >> 2)) v[w++] = v[i];
    v.resize(w);
}
static int costIn(const vector<u64>& S, u64 v) { auto it = lower_bound(S.begin(), S.end(), v << 2); if (it != S.end() && (*it >> 2) == v) return *it & 3; return 99; }
struct Cand { int cost; vector<Step> top; };   // final steps; operands that are not pool values cost <= 2 each
// all formulas of cost <= maxc of the forms A+B, A-B, A*B, (A*B) +- p   (A, B from the table)
static void find5join(Searcher& S, u64 T, int maxc, vector<Cand>& out, vector<u64>& tab, size_t maxOut) {
    tab.clear(); tab.reserve(S.v1.size() * S.P.size() * 4 + S.v1.size() * 5 + S.P.size());
    for (u64 p : S.P) tab.push_back(p << 2);
    for (u64 x : S.v1) tab.push_back((x << 2) | 1);
    for (u64 x : S.v1) { for (u64 p : S.P) comb2(x, p, 2, tab); comb2(x, x, 2, tab); }
    uniq2(tab);
    size_t n = tab.size();
    { size_t i = 0, j = n - 1;
      while (i <= j && out.size() < maxOut) { u64 a = tab[i] >> 2, b = tab[j] >> 2; u128 s = (u128)a + b;
        if (s == T) { int c = (tab[i] & 3) + (tab[j] & 3) + 1; if (c <= maxc) out.push_back({c, {{'+', b, a, T}}}); i++; if (j == 0) break; j--; }
        else if (s < T) i++; else { if (j == 0) break; j--; } } }
    { size_t j = 0;
      for (size_t i = 0; i < n && out.size() < maxOut; i++) { u64 b = tab[i] >> 2; u128 want = (u128)T + b; if (want > LIMIT) break;
        while (j < n && (tab[j] >> 2) < want) j++;
        if (j < n && (tab[j] >> 2) == (u64)want) { int c = (tab[i] & 3) + (tab[j] & 3) + 1; if (c <= maxc) out.push_back({c, {{'-', (u64)want, b, T}}}); } } }
    // products: T2 = a * W with a of cost <= 1 (any cost when T2 == T), W from the table; T = T2, T2 + p or T2 - p
    vector<u64> small; for (u64 p : S.P) if (p > 1) small.push_back(p << 2); for (u64 x : S.v1) if (x > 1) small.push_back((x << 2) | 1);
    auto prod = [&](u64 T2, int extra, char op, u64 p) {
        for (u64 e : small) { u64 a = e >> 2; if (T2 % a) continue; u64 w = T2 / a; if (w < 2) continue; int c = costIn(tab, w);
            if (c < 99 && c + (int)(e & 3) + 1 + extra <= maxc && out.size() < maxOut) { Cand cd{c + (int)(e & 3) + 1 + extra, {{'*', a, w, T2}}}; if (extra) cd.top.push_back(op == '+' ? Step{'+', T2, p, T} : Step{'-', T2, p, T}); out.push_back(cd); } }
    };
    for (size_t i = 0; i < n && out.size() < maxOut; i++) { u64 a = tab[i] >> 2; if (a < 2) continue; if ((u128)a * a > T) break; if (T % a) continue;
        int c = costIn(tab, T / a); if (c < 99 && c + (int)(tab[i] & 3) + 1 <= maxc) out.push_back({c + (int)(tab[i] & 3) + 1, {{'*', T / a, a, T}}}); }
    for (u64 p : S.P) { if (T > p) prod(T - p, 1, '+', p); if (p <= LIMIT - T) prod(T + p, 1, '-', p); }
    sort(out.begin(), out.end(), [](const Cand& x, const Cand& y) { return x.cost < y.cost; });
}
// expand a candidate into full steps (operands of cost <= 2 are rebuilt by the exact search)
static bool expand(Searcher& S, const Cand& cd, vector<Step>& out) {
    out.clear(); set<u64> made; vector<Step> t;
    for (auto& s : cd.top) {
        for (u64 v : {s.x, s.y}) if (!S.inP(v) && !made.count(v)) { if (!S.find(v, 2, t)) return false; for (auto& q : t) out.push_back(q); made.insert(v); }
        out.push_back(s); made.insert(s.r);
    }
    return true;
}
// ---- depth 6 (partial): T = (A op B) +- p or p - (A op B), A, B from the cost<=2 table; p ordered by |T - p|
template <class F> static void scanSum(const vector<u64>& tab, u64 T, F cb) {      // A + B == T, A >= B
    size_t n = upper_bound(tab.begin(), tab.end(), (T << 2) | 3) - tab.begin(); if (!n) return; size_t i = 0, j = n - 1;
    while (i <= j) { u64 s = (tab[i] >> 2) + (tab[j] >> 2);
        if (s == T) { cb(tab[j], tab[i]); i++; if (j == 0) break; j--; } else if (s < T) i++; else { if (j == 0) break; j--; } }
}
template <class F> static void scanDiff(const vector<u64>& tab, u64 T, F cb) {     // A - B == T
    size_t n = tab.size(), j = lower_bound(tab.begin(), tab.end(), T << 2) - tab.begin(); u64 room = LIMIT - T;
    for (size_t i = 0; i < n && j < n; i++) { u64 b = tab[i] >> 2; if (b > room) break; u64 want = T + b;
        while (j < n && (tab[j] >> 2) < want) j++;
        if (j < n && (tab[j] >> 2) == want) cb(tab[j], tab[i]); }
}
static double t6budget = 60; static long long scans6 = 0, scansAll6 = 0;
static void find6join(Searcher& S, u64 T, int maxc, vector<Cand>& out, const vector<u64>& tab, size_t maxOut, double deadline) {
    if (tab.empty()) return;
    struct Q { u64 T2; u64 p; int mode; };
    vector<Q> qs;
    for (u64 p : S.P) { if (T > p) qs.push_back({T - p, p, 0}); if (p <= LIMIT - T) qs.push_back({T + p, p, 1}); if (p > T) qs.push_back({p - T, p, 2}); }
    sort(qs.begin(), qs.end(), [](const Q& a, const Q& b) { return a.T2 < b.T2; });
    double stop = min(deadline, now() + t6budget); scansAll6 += qs.size();
    for (auto& q : qs) {
        if (now() > stop || out.size() >= maxOut) break;
        u64 T2 = q.T2, p = q.p; scans6++;
        Step last = q.mode == 0 ? Step{'+', T2, p, T} : q.mode == 1 ? Step{'-', T2, p, T} : Step{'-', p, T2, T};
        if (costIn(tab, T2) < 99) continue;
        scanSum(tab, T2, [&](u64 a, u64 b) { int c = (a & 3) + (b & 3) + 2; if (c <= maxc && out.size() < maxOut) out.push_back({c, {{'+', a >> 2, b >> 2, T2}, last}}); });
        scanDiff(tab, T2, [&](u64 a, u64 b) { int c = (a & 3) + (b & 3) + 2; if (c <= maxc && out.size() < maxOut) out.push_back({c, {{'-', a >> 2, b >> 2, T2}, last}}); });
    }
    stable_sort(out.begin(), out.end(), [](const Cand& x, const Cand& y) { return x.cost < y.cost; });
}
// ---- depth 6, family (b): T = C*p + A, C*p - A, A - C*p   (A, C from the cost<=2 table, p a pool value)
static int fam6 = 1;
static void find6b(Searcher& S, u64 T, int maxc, vector<Cand>& out, const vector<u64>& tab, size_t maxOut, double deadline) {
    size_t n = tab.size(); if (!n) return;
    double stop = min(deadline, now() + t6budget);
    vector<u64> ps; for (u64 p : S.P) if (p >= 2) ps.push_back(p);
    sort(ps.begin(), ps.end(), greater<u64>());      // large multipliers first: short scans
    scansAll6 += ps.size();
    for (u64 p : ps) {
        if (now() > stop || out.size() >= maxOut) break;
        scans6++;
        size_t hiC = upper_bound(tab.begin(), tab.end(), ((LIMIT / p) << 2) | 3) - tab.begin();   // C*p <= LIMIT
        // T = C*p + A : C ascending, A descending
        { size_t j = upper_bound(tab.begin(), tab.end(), (T << 2) | 3) - tab.begin();
          for (size_t i = 0; i < hiC && j > 0; i++) { u64 c0 = tab[i] >> 2; u128 m = (u128)c0 * p; if (m >= T) break; u64 want = T - (u64)m;
              while (j > 0 && (tab[j - 1] >> 2) > want) j--;
              if (j > 0 && (tab[j - 1] >> 2) == want && c0 > 1) { int c = (tab[i] & 3) + (tab[j - 1] & 3) + 2; if (c <= maxc && out.size() < maxOut) out.push_back({c, {{'*', c0, p, (u64)m}, {'+', (u64)m, want, T}}}); } } }
        // T = C*p - A (C*p > T) and T = A - C*p : all pointers ascending
        { size_t j = 0, j2 = lower_bound(tab.begin(), tab.end(), T << 2) - tab.begin();
          for (size_t i = 0; i < hiC; i++) { u64 c0 = tab[i] >> 2; if (c0 < 2) continue; u64 m = c0 * p;
              if (m > T) { u64 want = m - T; while (j < n && (tab[j] >> 2) < want) j++;
                  if (j < n && (tab[j] >> 2) == want) { int c = (tab[i] & 3) + (tab[j] & 3) + 2; if (c <= maxc && out.size() < maxOut) out.push_back({c, {{'*', c0, p, m}, {'-', m, want, T}}}); } }
              if (m <= LIMIT - T) { u64 w2 = T + m; while (j2 < n && (tab[j2] >> 2) < w2) j2++;
                  if (j2 < n && (tab[j2] >> 2) == w2) { int c = (tab[i] & 3) + (tab[j2] & 3) + 2; if (c <= maxc && out.size() < maxOut) out.push_back({c, {{'*', c0, p, m}, {'-', w2, m, T}}}); } } } }
    }
    stable_sort(out.begin(), out.end(), [](const Cand& x, const Cand& y) { return x.cost < y.cost; });
}

struct Stats { long long cones = 0, hits = 0, fitFail = 0, applied = 0; map<int, int> coneTried, coneHit; };
static long long saIters = 1500000; static vector<u64> tab; static int maxAlts = 16, minCone6 = 7, maxCone = 99;

static bool improveOnce(Graph& g, const Puzzle& P, Searcher& S, mt19937_64& rng, double deadline, int D, int minCone, int regs, Stats& st, set<u64>& tried) {
    int N = g.nodes.size(), K = P.starts.size();
    vector<vector<int>> users(N);
    for (int id : g.order) { users[g.nodes[id].a].push_back(id); if (g.nodes[id].b != g.nodes[id].a) users[g.nodes[id].b].push_back(id); }
    unordered_set<u64> targetSet(P.targets.begin(), P.targets.end());
    vector<int> cand(g.order); shuffle(cand.begin(), cand.end(), rng);
    for (int v : cand) {
        if (now() > deadline) return false;
        vector<char> inCone(N, 0); inCone[v] = 1; vector<int> cone{v};
        for (size_t q = 0; q < cone.size(); q++) {
            const Node& n = g.nodes[cone[q]];
            for (int c : {n.a, n.b}) {
                if (c < K || inCone[c] || targetSet.count(g.nodes[c].v)) continue;
                bool only = true; for (int u : users[c]) if (!inCone[u]) { only = false; break; }
                if (only) { inCone[c] = 1; cone.push_back(c); }
            }
        }
        int k = cone.size(); if (k < minCone || k > maxCone) continue;
        u64 sig = hsh(g.nodes[v].v) ^ (u64)k * 0x9e3779b97f4a7c15ULL; for (int c : cone) sig ^= hsh(g.nodes[c].v + 77);
        if (tried.count(sig)) continue;
        vector<char> desc(N, 0); vector<int> stk{v};
        while (!stk.empty()) { int x = stk.back(); stk.pop_back(); for (int u : users[x]) if (!desc[u]) { desc[u] = 1; stk.push_back(u); } }
        vector<u64> avail; unordered_map<u64, int> byVal;
        for (int k2 = 0; k2 < K; k2++) if (!byVal.count(g.nodes[k2].v)) { avail.push_back(g.nodes[k2].v); byVal.emplace(g.nodes[k2].v, k2); }
        for (int id : g.order) if (!inCone[id] && !desc[id] && !byVal.count(g.nodes[id].v)) { avail.push_back(g.nodes[id].v); byVal.emplace(g.nodes[id].v, id); }
        if (byVal.count(g.nodes[v].v)) continue;
        S.setPool(avail); S.deadline = deadline;
        st.cones++; st.coneTried[k]++;
        u64 T = g.nodes[v].v; vector<vector<Step>> alts; vector<Step> steps;
        if (S.find(T, min(k - 1, min(D, 4)), steps)) alts.push_back(steps);
        else if (D >= 5 && k >= 6 && now() < deadline) {
            vector<Cand> cs; find5join(S, T, min(k - 1, 5), cs, tab, 4096);
            if (cs.empty() && D >= 6 && k >= minCone6 && now() < deadline) { if (fam6 & 1) find6join(S, T, min(k - 1, 6), cs, tab, 4096, deadline); if (cs.empty() && (fam6 & 2)) find6b(S, T, min(k - 1, 6), cs, tab, 4096, deadline); }
            for (auto& cd : cs) { if ((int)alts.size() >= maxAlts) break; if (expand(S, cd, steps)) alts.push_back(steps); }
        }
        if (now() > deadline) return false;
        tried.insert(sig);
        bool hit = false;
        for (auto& al : alts) {
            vector<Step> ds; { unordered_set<u64> seen; for (auto& s : al) { if (byVal.count(s.r) || seen.count(s.r)) continue; seen.insert(s.r); ds.push_back(s); } }
            if ((int)ds.size() >= k || ds.empty() || ds.back().r != T) continue;
            bool bad = false; { unordered_set<u64> known; for (auto& s : ds) { for (u64 o : {s.x, s.y}) if (!byVal.count(o) && !known.count(o)) bad = true; known.insert(s.r); } }
            if (bad) continue;
            if (!hit) { hit = true; st.hits++; st.coneHit[k]++; }
            Graph h = g; vector<double> prio(N + ds.size(), 0);
            vector<int> posOf(N, -1); for (int i = 0; i < (int)g.order.size(); i++) posOf[g.order[i]] = i;
            for (int i = 0; i < N; i++) prio[i] = posOf[i];
            unordered_map<u64, int> local = byVal;
            for (size_t s = 0; s < ds.size(); s++) {
                const Step& s2 = ds[s]; int a = local.at(s2.x), b = local.at(s2.y);
                if (s + 1 == ds.size()) { h.nodes[v] = {s2.op, a, b, s2.r}; local[s2.r] = v; }
                else { h.nodes.push_back({s2.op, a, b, s2.r}); int id = h.nodes.size() - 1; prio[id] = prio[v] - 0.5 + 0.01 * s; local[s2.r] = id; }
            }
            h.order.clear();
            for (int id : g.order) if (!inCone[id] || id == v) h.order.push_back(id);
            for (size_t s = 0; s + 1 < ds.size(); s++) h.order.push_back(N + s);
            if (!reorder(h, prio, K)) continue;
            Graph hp = prune(h, P);
            if (!fitSA(hp, P, regs, rng, saIters)) { st.fitFail++; continue; }
            st.applied++; g = hp; return true;
        }
    }
    return false;
}
int main(int argc, char** argv) {
    double tper = 10; u64 seed = 1; int D = 4, minCone = 5, regsOverride = 0;
    for (int i = 1; i < argc; i++) { string a = argv[i];
        if (a == "--time") tper = atof(argv[++i]); else if (a == "--seed") seed = atoll(argv[++i]);
        else if (a == "--depth") D = atoi(argv[++i]); else if (a == "--mincone") minCone = atoi(argv[++i]); else if (a == "--regs") regsOverride = atoi(argv[++i]); else if (a == "--sa") saIters = atoll(argv[++i]); else if (a == "--alts") maxAlts = atoi(argv[++i]); else if (a == "--mincone6") minCone6 = atoi(argv[++i]); else if (a == "--t6") t6budget = atof(argv[++i]); else if (a == "--fam") fam6 = atoi(argv[++i]); else if (a == "--maxcone") maxCone = atoi(argv[++i]); }
    mt19937_64 rng(seed); string tag;
    while (cin >> tag) {
        if (tag != "P") continue;
        Puzzle P; int k, m, n; cin >> P.id >> k; P.starts.resize(k); for (auto& x : P.starts) cin >> x;
        cin >> m; P.targets.resize(m); for (auto& x : P.targets) cin >> x; cin >> P.limit >> P.regs >> n;
        P.prog.resize(n); P.progOp.resize(n);
        for (int i = 0; i < n; i++) { string op; cin >> op >> P.prog[i][1] >> P.prog[i][2] >> P.prog[i][3]; P.progOp[i] = op[0]; }
        LIMIT = P.limit; int regs = regsOverride ? regsOverride : P.regs;
        Graph g = prune(build(P), P);
        Searcher S; Stats st; set<u64> tried; double deadline = now() + tper; int moves = 0;
        while (now() < deadline && improveOnce(g, P, S, rng, deadline, D, minCone, regs, st, tried)) moves++;
        vector<array<int, 4>> prog; vector<char> ops;
        bool ok = fit(g, P, regs, rng, 200) && emit(g, P, max(regs, P.regs), prog, ops);
        cerr << P.id << " " << n << " -> " << (ok ? (int)prog.size() : -1) << " moves " << moves << " cones " << st.cones << " hits " << st.hits << " fitFail " << st.fitFail << " |";
        for (auto& e : st.coneTried) cerr << " c" << e.first << ":" << st.coneHit[e.first] << "/" << e.second;
        cerr << " scans6 " << scans6 << "/" << scansAll6 << (now() < deadline ? " done" : " timeout") << "\n"; scans6 = scansAll6 = 0;
        if (ok && (int)prog.size() < n) {
            cout << "R " << P.id << " " << n << " " << prog.size() << "\n";
            for (size_t i = 0; i < prog.size(); i++) cout << ops[i] << " " << prog[i][1] << " " << prog[i][2] << " " << prog[i][3] << "\n";
            cout.flush();
        }
    }
}
