// elfopt.cpp - Santa 2026 value-DAG optimizer (our own code).
// Build: g++ -std=c++17 -O3 -march=native -o elfopt elfopt.cpp
// Input/Output block format (stdin -> stdout), one block per puzzle:
//   P <id> <k> s1..sk <m> t1..tm <limit> <regs> <n>   then n lines "<op> <i> <j> <d>"
//   output: R <id> <n_old> <n_new> then n_new lines (only when strictly improved)
#include <bits/stdc++.h>
using namespace std;
typedef unsigned long long u64;
typedef __int128 i128;

static double now() { return chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count(); }
static mt19937_64 rng(1);
static inline u64 rnd(u64 n) { return rng() % n; }

// ---------------------------------------------------------------- hash map u64 (>0) -> int
struct HMap {
    vector<u64> k; vector<int> v; u64 mask = 0; size_t cnt = 0;
    void init(size_t n) {
        size_t cap = 64; while (cap < 2 * n + 64) cap <<= 1;
        if (k.size() != cap) { k.assign(cap, 0); v.assign(cap, -1); } else fill(k.begin(), k.end(), 0);
        mask = cap - 1; cnt = 0;
    }
    static inline u64 h(u64 x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; return x; }
    inline int get(u64 key) const {
        u64 i = h(key) & mask;
        for (;;) { u64 kk = k[i]; if (kk == key) return v[i]; if (!kk) return -1; i = (i + 1) & mask; }
    }
    inline bool put(u64 key, int val) {
        if ((cnt + 1) * 2 > k.size()) grow();
        u64 i = h(key) & mask;
        for (;;) {
            u64 kk = k[i];
            if (kk == key) return false;
            if (!kk) { k[i] = key; v[i] = val; cnt++; return true; }
            i = (i + 1) & mask;
        }
    }
    void grow() {
        vector<u64> ok = k; vector<int> ov = v;
        size_t cap = k.size() * 2; k.assign(cap, 0); v.assign(cap, -1); mask = cap - 1; cnt = 0;
        for (size_t i = 0; i < ok.size(); i++) if (ok[i]) put(ok[i], ov[i]);
    }
};

// ops: 1 '+', 2 '-', 3 '*', 4 '/'
static const char OPC[5] = {'?', '+', '-', '*', '/'};
static inline bool ap(int op, u64 a, u64 b, u64 L, u64& r) {
    switch (op) {
        case 1: r = a + b; return r <= L && r >= a;
        case 2: if (a <= b) return false; r = a - b; return true;
        case 3: { i128 p = (i128)a * b; if (p > (i128)L) return false; r = (u64)p; return true; }
        case 4: if (!b || a % b) return false; r = a / b; return r >= 1;
    }
    return false;
}
struct Step { int op; u64 a, b, r; };
// inverse: T = w op x (side 0) or T = x op w (side 1)
struct Inv { u64 w; int op; int side; };
static inline int inverses(u64 T, u64 x, u64 L, Inv* o) {
    int n = 0;
    if (T > x) o[n++] = {T - x, 1, 0};                       // w + x
    if (x <= L - T) o[n++] = {T + x, 2, 0};                  // w - x
    if (x > T) o[n++] = {x - T, 2, 1};                       // x - w
    if (x > 1 && T % x == 0) o[n++] = {T / x, 3, 0};         // w * x
    if (x > 1) { i128 p = (i128)T * x; if (p <= (i128)L) o[n++] = {(u64)p, 4, 0}; }  // w / x
    if (x > T && x % T == 0) o[n++] = {x / T, 4, 1};         // x / w
    return n;
}
static inline Step mk(const Inv& iv, u64 x, u64 T) { return iv.side == 0 ? Step{iv.op, iv.w, x, T} : Step{iv.op, x, iv.w, T}; }

// ---------------------------------------------------------------- searcher
struct Searcher {
    u64 L = 0;
    vector<u64> pool; HMap H0;
    vector<u64> f1v; vector<Step> f1s; HMap H1;
    vector<u64> sortedAll; vector<int> sortedCost;   // pool + S1 sorted by value, for window search
    int K4 = 48;          // pool prefix used in streamed depth-4 shapes
    bool deep = true, window = true;
    HMap need; vector<Step> needA, needB; vector<char> needK;

    void addS1(u64 r, Step s) {
        if (H0.get(r) >= 0) return;
        if (H1.put(r, (int)f1v.size())) { f1v.push_back(r); f1s.push_back(s); }
    }
    void pairS1(u64 a, u64 b) {
        if (a > b) swap(a, b);
        u64 r;
        if (ap(1, a, b, L, r)) addS1(r, {1, a, b, r});
        if (ap(3, a, b, L, r)) addS1(r, {3, a, b, r});
        if (b > a) addS1(b - a, {2, b, a, b - a});
        if (b % a == 0 && b != a) addS1(b / a, {4, b, a, b / a});
    }
    void setPool(const vector<u64>& p, u64 lim) {
        L = lim; pool = p; H0.init(pool.size());
        for (size_t i = 0; i < pool.size(); i++) H0.put(pool[i], (int)i);
        f1v.clear(); f1s.clear(); H1.init(pool.size() * pool.size() * 2);
        for (size_t i = 0; i < pool.size(); i++) for (size_t j = i; j < pool.size(); j++) pairS1(pool[i], pool[j]);
        sortedValid = false;
    }
    void extend(u64 v) {
        if (H0.get(v) >= 0) return;
        H0.put(v, (int)pool.size()); pool.push_back(v);
        for (size_t j = 0; j < pool.size(); j++) pairS1(pool[j], v);
        sortedValid = false;
    }
    bool sortedValid = false;
    vector<pair<u64, int>> sorted1;   // (value, -1 for pool or S1 idx)
    void buildSorted() {
        sorted1.clear(); sorted1.reserve(pool.size() + f1v.size());
        for (u64 x : pool) sorted1.push_back({x, -1});
        for (size_t i = 0; i < f1v.size(); i++) sorted1.push_back({f1v[i], (int)i});
        sort(sorted1.begin(), sorted1.end());
        sortedValid = true;
    }
    inline bool inP(u64 x) const { return H0.get(x) >= 0; }
    inline int inS1(u64 x) const { return H1.get(x); }
    // cost of x if <=1: returns 0/1 or -1, appends step
    inline int c01(u64 x, vector<Step>& o) const {
        if (inP(x)) return 0;
        int id = inS1(x); if (id >= 0) { o.push_back(f1s[id]); return 1; }
        return -1;
    }

    // cheapest derivation of T with cost <= maxc. returns cost or -1. Randomized tie-breaking via start offsets.
    int find(u64 T, int maxc, vector<Step>& out) {
        out.clear();
        if (inP(T)) return 0;
        if (maxc < 1) return -1;
        int id = inS1(T);
        if (id >= 0) { out.push_back(f1s[id]); return 1; }
        if (maxc < 2) return -1;
        Inv iv[6], iv2[6];
        size_t n = pool.size(), off = rnd(n);
        for (size_t ii = 0; ii < n; ii++) {
            u64 x = pool[(ii + off) % n]; int c = inverses(T, x, L, iv);
            for (int q = 0; q < c; q++) { int u = inS1(iv[q].w); if (u >= 0) { out.push_back(f1s[u]); out.push_back(mk(iv[q], x, T)); return 2; } }
        }
        if (T % 2 == 0) { int u = inS1(T / 2); if (u >= 0) { out.push_back(f1s[u]); out.push_back({1, T / 2, T / 2, T}); return 2; } }
        { u64 s = (u64)sqrtl((long double)T); while (s * s > T) s--; while ((s + 1) * (s + 1) <= T) s++;
          if (s * s == T) { int u = inS1(s); if (u >= 0) { out.push_back(f1s[u]); out.push_back({3, s, s, T}); return 2; } } }
        if (maxc < 3) return -1;
        // cost 3a: T = f op g
        size_t m1 = f1v.size(), off1 = m1 ? rnd(m1) : 0;
        for (size_t aa = 0; aa < m1; aa++) {
            size_t a = (aa + off1) % m1; u64 x = f1v[a]; if (inP(x)) continue;
            int c = inverses(T, x, L, iv);
            for (int q = 0; q < c; q++) {
                if (iv[q].w == x) continue;
                int u = inS1(iv[q].w);
                if (u >= 0) { out.push_back(f1s[a]); out.push_back(f1s[u]); out.push_back(mk(iv[q], x, T)); return 3; }
            }
        }
        // cost 3b: T = (f op y) op x
        for (size_t ii = 0; ii < n; ii++) {
            u64 x = pool[(ii + off) % n]; int c = inverses(T, x, L, iv);
            for (int q = 0; q < c; q++) {
                u64 w1 = iv[q].w; if (inP(w1)) continue;
                for (size_t j = 0; j < n; j++) {
                    u64 y = pool[j]; int c2 = inverses(w1, y, L, iv2);
                    for (int q2 = 0; q2 < c2; q2++) {
                        int u = inS1(iv2[q2].w);
                        if (u >= 0) { out.push_back(f1s[u]); out.push_back(mk(iv2[q2], y, w1)); out.push_back(mk(iv[q], x, T)); return 3; }
                    }
                }
                if (w1 % 2 == 0) { int u = inS1(w1 / 2); if (u >= 0) { out.push_back(f1s[u]); out.push_back({1, w1 / 2, w1 / 2, w1}); out.push_back(mk(iv[q], x, T)); return 3; } }
            }
        }
        if (maxc < 4) return -1;
        if (deep && find4(T, out) == 4) return 4;
        out.clear();
        if (maxc < 5 || !window) return -1;
        return findWindow(T, maxc, out);
    }

    // depth-4 by streaming S2 = S1 op pool[0..K) against a need-hash
    int find4(u64 T, vector<Step>& out) {
        Inv iv[6], iv2[6];
        size_t n = pool.size(), K = min(n, (size_t)K4);
        need.init(f1v.size() * 3 + n * K * 8 + 64); needA.clear(); needB.clear(); needK.clear();
        for (size_t a = 0; a < f1v.size(); a++) {     // T = w op g  (w in S2, g in S1)
            u64 x = f1v[a]; if (inP(x)) continue;
            int c = inverses(T, x, L, iv);
            for (int q = 0; q < c; q++) { u64 w = iv[q].w; if (inP(w) || inS1(w) >= 0) continue;
                if (need.put(w, needA.size())) { needA.push_back(f1s[a]); needB.push_back(mk(iv[q], x, T)); needK.push_back(0); } }
        }
        for (size_t i = 0; i < n; i++) {               // T = (w op y) op x
            u64 x = pool[i]; int c = inverses(T, x, L, iv);
            for (int q = 0; q < c; q++) {
                u64 w1 = iv[q].w; if (inP(w1) || inS1(w1) >= 0) continue;
                for (size_t j = 0; j < K; j++) {
                    u64 y = pool[j]; int c2 = inverses(w1, y, L, iv2);
                    for (int q2 = 0; q2 < c2; q2++) { u64 w = iv2[q2].w; if (inP(w) || inS1(w) >= 0) continue;
                        if (need.put(w, needA.size())) { needA.push_back(mk(iv2[q2], y, w1)); needB.push_back(mk(iv[q], x, T)); needK.push_back(1); } }
                }
            }
        }
        size_t m1 = f1v.size(), off1 = m1 ? rnd(m1) : 0;
        for (size_t aa = 0; aa < m1; aa++) {
            size_t a = (aa + off1) % m1; u64 u = f1v[a];
            for (size_t j = 0; j < K; j++) {
                u64 y = pool[j], r;
                for (int op = 1; op <= 4; op++) for (int side = 0; side < 2; side++) {
                    if (side && (op == 1 || op == 3)) continue;
                    if (!(side ? ap(op, y, u, L, r) : ap(op, u, y, L, r))) continue;
                    int id = need.get(r); if (id < 0) continue;
                    if (needK[id] == 0 && needA[id].r == r) continue;
                    out.clear(); out.push_back(f1s[a]); out.push_back(side ? Step{op, y, u, r} : Step{op, u, y, r});
                    out.push_back(needA[id]); out.push_back(needB[id]);
                    return 4;
                }
            }
        }
        return -1;
    }

    // depth-5 product shapes: T = X*Y +- R
    //  (a) X,Y,R in P u S1 (Y = sorted neighbours of T/X)   cost cx+cy+cr+2
    //  (b) X in P, Y in {T/X, T/X+1} built with 2 steps, R in P u S1
    //  (c) X in P, Y neighbour (cost<=1), R built with 2 steps
    int findWindow(u64 T, int maxc, vector<Step>& out) {
        if (!sortedValid) buildSorted();
        int best = maxc + 1; vector<Step> bestS, tmp;
        const auto& S = sorted1; size_t N = S.size();
        auto consider = [&](u64 X, int xid, u64 Y, int yid, int cy2, vector<Step>& ysteps) {
            i128 Pp = (i128)X * Y; if (Pp > (i128)L) return;
            u64 PP = (u64)Pp; int cx = xid < 0 ? 0 : 1; int cy = cy2 >= 0 ? cy2 : (yid < 0 ? 0 : 1);
            if (X == Y && xid >= 0) cy = 0;
            int base = cx + cy + 1;
            if (PP == T) { if (base < best) { best = base; bestS.clear(); if (xid >= 0) bestS.push_back(f1s[xid]); for (auto& s : ysteps) bestS.push_back(s); if (yid >= 0 && cy2 < 0) bestS.push_back(f1s[yid]); bestS.push_back({3, X, Y, T}); } return; }
            if (base + 2 >= best) return;
            u64 R = PP > T ? PP - T : T - PP;
            tmp.clear(); int cr = c01(R, tmp);
            if (cr < 0 && base + 3 < best && cy2 < 0 && xid < 0) cr = cost2(R, tmp);
            if (cr < 0) return;
            int tot = base + cr + 1;
            if (tot < best) {
                best = tot; bestS.clear();
                if (xid >= 0) bestS.push_back(f1s[xid]);
                for (auto& s : ysteps) bestS.push_back(s);
                if (yid >= 0 && cy2 < 0) bestS.push_back(f1s[yid]);
                bestS.push_back({3, X, Y, PP});
                for (auto& s : tmp) bestS.push_back(s);
                bestS.push_back(PP > T ? Step{2, PP, R, T} : Step{1, PP, R, T});
            }
        };
        vector<Step> none, ys;
        for (size_t xi = 0; xi < N; xi++) {
            u64 X = S[xi].first; if (X < 2) continue;
            int xid = S[xi].second;
            if ((xid < 0 ? 0 : 1) + 3 >= best) continue;
            u64 q = T / X; if (q < 1) break;
            size_t lo = lower_bound(S.begin(), S.end(), make_pair(q, INT_MIN)) - S.begin();
            for (int st = -2; st < 2; st++) {
                long long yi = (long long)lo + st; if (yi < 0 || yi >= (long long)N) continue;
                consider(X, xid, S[yi].first, S[yi].second, -1, none);
            }
            if (xid < 0 && best > 5) {           // (b): Y = q or q+1 built in 2 steps
                for (u64 Y = q; Y <= q + 1; Y++) {
                    if (Y < 2 || inP(Y) || inS1(Y) >= 0) continue;
                    ys.clear(); if (cost2(Y, ys) == 2) consider(X, xid, Y, -1, 2, ys);
                }
            }
        }
        if (best <= maxc) { out = dedupSteps(bestS); return (int)out.size(); }
        return -1;
    }
    // ---- joint search for two targets sharing one new helper h
    // A1(T): values w such that T = w op p (p in pool) or T = w+w / w*w ; returns (w, final step)
    void oneStepCompletions(u64 T, vector<pair<u64, Step>>& o) {
        o.clear(); Inv iv[6];
        for (u64 x : pool) { int c = inverses(T, x, L, iv); for (int q = 0; q < c; q++) if (!inP(iv[q].w)) o.push_back({iv[q].w, mk(iv[q], x, T)}); }
        if (T % 2 == 0 && !inP(T / 2)) o.push_back({T / 2, {1, T / 2, T / 2, T}});
        u64 s = (u64)sqrtl((long double)T); while (s * s > T) s--; while ((s + 1) * (s + 1) <= T) s++;
        if (s * s == T && !inP(s)) o.push_back({s, {3, s, s, T}});
    }
    // T costs exactly 2 more given helper w (w not in pool): T = w op f (f in S1) or T = (w op p) op q ; append steps
    bool twoGivenW(u64 T, u64 w, vector<Step>& o) {
        Inv iv[6], iv2[6];
        int c = inverses(T, w, L, iv);
        for (int q = 0; q < c; q++) { if (iv[q].w == w) continue; int u = inS1(iv[q].w); if (u >= 0) { o.push_back(f1s[u]); o.push_back(mk(iv[q], w, T)); return true; } }
        for (u64 x : pool) {
            int c1 = inverses(T, x, L, iv);
            for (int q = 0; q < c1; q++) {
                u64 w1 = iv[q].w; if (inP(w1) || w1 == w) continue;
                int c2 = inverses(w1, w, L, iv2);
                for (int q2 = 0; q2 < c2; q2++) if (inP(iv2[q2].w)) { o.push_back(mk(iv2[q2], w, w1)); o.push_back(mk(iv[q], x, T)); return true; }
            }
        }
        return false;
    }
    // best joint cost (<= maxc) for {T1,T2} using a shared helper; returns cost or -1
    int findPair(u64 T1, u64 T2, int maxc, vector<Step>& out) {
        if (maxc < 3) return -1;
        vector<pair<u64, Step>> a1, a2; oneStepCompletions(T1, a1); oneStepCompletions(T2, a2);
        HMap h1; h1.init(a1.size() + 8);
        for (size_t i = 0; i < a1.size(); i++) h1.put(a1[i].first, (int)i);
        vector<Step> tmp; int best = maxc + 1; vector<Step> bs;
        // total = cost(w) + 1 + 1
        for (auto& e : a2) {
            int i1 = h1.get(e.first); if (i1 < 0) continue;
            u64 w = e.first; tmp.clear();
            int cw = c01(w, tmp); if (cw < 0 && best > 4) cw = cost2(w, tmp);
            if (cw < 0) continue;
            if (cw + 2 < best) { best = cw + 2; bs = tmp; bs.push_back(a1[i1].second); bs.push_back(e.second); if (best == 3) break; }
        }
        // total = 1 + 1 + 2 : w in S1, one target one-step from w, the other two-step from w
        if (best > 4 && maxc >= 4) {
            for (int side = 0; side < 2 && best > 4; side++) {
                auto& A = side ? a2 : a1; u64 Tother = side ? T1 : T2;
                for (auto& e : A) {
                    int u = inS1(e.first); if (u < 0) continue;
                    tmp.clear();
                    if (twoGivenW(Tother, e.first, tmp)) { best = 4; bs.clear(); bs.push_back(f1s[u]); bs.push_back(e.second); for (auto& s : tmp) bs.push_back(s); break; }
                }
            }
        }
        if (best <= maxc) { out = dedupSteps(bs); return (int)out.size(); }
        return -1;
    }
    void pushXY(const pair<u64, int>& x, const pair<u64, int>& y, vector<Step>& o) {
        if (x.second >= 0) o.push_back(f1s[x.second]);
        if (y.second >= 0 && !(x.second >= 0 && x.first == y.first)) o.push_back(f1s[y.second]);
    }
    vector<Step> dedupSteps(const vector<Step>& s) {
        vector<Step> o; unordered_set<u64> seen;
        for (auto& x : s) { if (inP(x.r) || seen.count(x.r)) continue; seen.insert(x.r); o.push_back(x); }
        return o;
    }
    // cost exactly 2 for R (pool inversion), append steps
    int cost2(u64 R, vector<Step>& o) {
        Inv iv[6];
        for (size_t i = 0; i < pool.size(); i++) {
            u64 x = pool[i]; int c = inverses(R, x, L, iv);
            for (int q = 0; q < c; q++) { int u = inS1(iv[q].w); if (u >= 0) { o.push_back(f1s[u]); o.push_back(mk(iv[q], x, R)); return 2; } }
        }
        return -1;
    }
};

// ---------------------------------------------------------------- DAG
struct Node { u64 v; int op; int a, b; double pos; };
struct Puzzle { string id; vector<u64> start, targets; u64 L; int regs; vector<array<int, 4>> prog; };

struct Dag {
    const Puzzle* P = nullptr;
    vector<Node> N; int ks = 0;
    unordered_map<u64, int> idx;
    void reindex() { idx.clear(); for (int i = 0; i < (int)N.size(); i++) idx[N[i].v] = i; }
    int count() const { return (int)N.size() - ks; }
    bool fromProgram(const Puzzle& p) {
        P = &p; N.clear();
        vector<int> reg(p.regs, -1); unordered_map<u64, int> m;
        for (int k = 0; k < (int)p.start.size(); k++) {
            auto it = m.find(p.start[k]);
            if (it != m.end()) { reg[k] = it->second; continue; }
            N.push_back({p.start[k], 0, -1, -1, -1e9 + k}); m[p.start[k]] = N.size() - 1; reg[k] = N.size() - 1;
        }
        ks = N.size();
        for (size_t t = 0; t < p.prog.size(); t++) {
            auto& in = p.prog[t]; int op = in[0] == '+' ? 1 : in[0] == '-' ? 2 : in[0] == '*' ? 3 : 4;
            int a = reg[in[1]], b = reg[in[2]]; if (a < 0 || b < 0) return false;
            u64 r; if (!ap(op, N[a].v, N[b].v, p.L, r)) return false;
            auto it = m.find(r);
            if (it != m.end()) { reg[in[3]] = it->second; continue; }
            N.push_back({r, op, a, b, (double)t}); m[r] = N.size() - 1; reg[in[3]] = N.size() - 1;
        }
        clean(); return covers();
    }
    void clean() {
        int n = N.size(); vector<char> keep(n, 0);
        unordered_map<u64, int> m; for (int i = 0; i < n; i++) m[N[i].v] = i;
        vector<int> st;
        for (u64 t : P->targets) { auto it = m.find(t); if (it != m.end()) st.push_back(it->second); }
        while (!st.empty()) { int x = st.back(); st.pop_back(); if (keep[x]) continue; keep[x] = 1; if (N[x].op) { st.push_back(N[x].a); st.push_back(N[x].b); } }
        for (int i = 0; i < ks; i++) keep[i] = 1;
        vector<int> rem(n, -1); vector<Node> M;
        for (int i = 0; i < n; i++) if (keep[i]) { rem[i] = M.size(); M.push_back(N[i]); }
        for (auto& x : M) if (x.op) { x.a = rem[x.a]; x.b = rem[x.b]; }
        N.swap(M); reindex();
    }
    bool covers() const { for (u64 t : P->targets) if (!idx.count(t)) return false; return true; }
    // max simultaneous live values (values needed later) incl. the result being written
    int pressure() const {
        int n = N.size(); vector<int> last(n, -1);
        for (int i = 0; i < n; i++) if (N[i].op) { last[N[i].a] = max(last[N[i].a], i); last[N[i].b] = max(last[N[i].b], i); }
        // starts occupy their registers until last use (start registers are fixed but values may be dead)
        vector<int> dieAt(n + 1, 0);
        int cur = 0;
        for (int i = 0; i < ks; i++) if (last[i] >= ks) { cur++; dieAt[last[i]]++; }
        int mx = cur;
        for (int i = ks; i < n; i++) {
            // operands that die at i free their register before the write
            int need = cur - dieAt[i] + 1; mx = max(mx, need);
            cur -= dieAt[i];
            if (last[i] > i) { cur++; dieAt[last[i]]++; }
        }
        return mx;
    }
    void permute(const vector<int>& ord) {
        int n = N.size(); vector<int> rem(n); for (int i = 0; i < n; i++) rem[ord[i]] = i;
        vector<Node> M(n); for (int i = 0; i < n; i++) { M[i] = N[ord[i]]; if (M[i].op) M[i].pos = i; }
        for (auto& x : M) if (x.op) { x.a = rem[x.a]; x.b = rem[x.b]; }
        N.swap(M); reindex();
    }
    // greedy list scheduling minimizing pressure
    bool listSchedule() {
        int n = N.size(); vector<vector<int>> us(n); vector<int> indeg(n, 0), remUses(n, 0);
        for (int i = 0; i < n; i++) if (N[i].op) {
            us[N[i].a].push_back(i); indeg[i]++; remUses[N[i].a]++;
            if (N[i].b != N[i].a) { us[N[i].b].push_back(i); indeg[i]++; remUses[N[i].b]++; }
        }
        vector<int> ord; vector<char> ready(n, 0);
        for (int i = 0; i < ks; i++) ord.push_back(i);
        for (int i = 0; i < ks; i++) for (int y : us[i]) if (--indeg[y] == 0) ready[y] = 1;
        for (int step = ks; step < n; step++) {
            int best = -1; double bs = 1e18;
            for (int i = ks; i < n; i++) if (ready[i]) {
                int fr = 0;
                if (remUses[N[i].a] == 1) fr++;
                if (N[i].b != N[i].a && remUses[N[i].b] == 1) fr++;
                double s = (us[i].empty() ? 0 : 1) - fr + 1e-6 * N[i].pos;
                if (s < bs) { bs = s; best = i; }
            }
            if (best < 0) return false;
            ready[best] = 0; ord.push_back(best);
            remUses[N[best].a]--; if (N[best].b != N[best].a) remUses[N[best].b]--;
            for (int y : us[best]) if (--indeg[y] == 0) ready[y] = 1;
        }
        permute(ord); return true;
    }
    bool fixPressure() {
        if (pressure() <= P->regs) return true;
        Dag c = *this; if (!c.listSchedule()) return false;
        if (c.pressure() <= P->regs) { *this = c; return true; }
        return false;
    }
    bool emit(vector<array<int, 4>>& out) const {
        int n = N.size(); vector<int> last(n, -1);
        for (int i = 0; i < n; i++) if (N[i].op) { last[N[i].a] = max(last[N[i].a], i); last[N[i].b] = max(last[N[i].b], i); }
        vector<int> reg(n, -1), occ(P->regs, -1);
        for (int k = 0; k < (int)P->start.size(); k++) { int id = idx.at(P->start[k]); if (reg[id] < 0) { reg[id] = k; occ[k] = id; } }
        for (int i = 0; i < ks; i++) if (last[i] < 0 && reg[i] >= 0) occ[reg[i]] = -1;
        out.clear();
        for (int i = ks; i < n; i++) {
            int a = N[i].a, b = N[i].b, ra = reg[a], rb = reg[b];
            if (ra < 0 || rb < 0 || occ[ra] != a || occ[rb] != b) return false;
            if (last[a] == i) occ[ra] = -1;
            if (last[b] == i) occ[rb] = -1;
            int d = -1; for (int r = 0; r < P->regs; r++) if (occ[r] < 0) { d = r; break; }
            if (d < 0) return false;
            out.push_back({OPC[N[i].op], ra, rb, d}); reg[i] = d; occ[d] = i;
            if (last[i] <= i) occ[d] = -1;
        }
        return true;
    }
};

// ---------------------------------------------------------------- LNS
struct LNS {
    const Puzzle& P; Dag G; Searcher S;
    double eqProb = 0.3; int maxGroup = 3; int nOrders = 3; bool pairSearch = true;
    long long evals = 0, accepts = 0, strict = 0, pairHits = 0, helperAcc = 0, rejP = 0, rejPstrict = 0; int kicks = 0, kicksDone = 0, kickSlack = 1;
    bool collect = false, harvestAll = false; set<tuple<u64,int,u64,u64>> recipes;
    void harvest() { if (!collect) return; for (auto& x : G.N) if (x.op) recipes.insert({x.v, x.op, G.N[x.a].v, G.N[x.b].v}); } bool helperMoves = true;
    LNS(const Puzzle& p) : P(p) {}

    int n = 0, nt = 0;
    vector<u64> mask; vector<int> tnode; vector<vector<int>> users;
    void analyse() {
        n = G.N.size(); users.assign(n, {});
        for (int i = 0; i < n; i++) if (G.N[i].op) { users[G.N[i].a].push_back(i); if (G.N[i].b != G.N[i].a) users[G.N[i].b].push_back(i); }
        tnode.clear(); vector<char> isT(n, 0);
        for (u64 t : P.targets) { int id = G.idx.at(t); if (G.N[id].op && !isT[id]) { isT[id] = 1; tnode.push_back(id); } }
        nt = tnode.size(); mask.assign(n, 0);
        for (int k = 0; k < nt; k++) mask[tnode[k]] |= 1ULL << k;
        for (int i = n - 1; i >= 0; i--) if (G.N[i].op) { mask[G.N[i].a] |= mask[i]; mask[G.N[i].b] |= mask[i]; }
    }
    // build new DAG: keep nodes not in R plus new steps (dedupe by value); place new steps at given positions
    bool build(const vector<char>& inR, const vector<Step>& steps, const vector<double>& pos, Dag& out) {
        out.P = &P; out.ks = G.ks; out.N.clear();
        struct Tmp { u64 v; int op; u64 a, b; double pos; };
        vector<Tmp> all;
        for (int i = 0; i < n; i++) if (!inR[i]) { auto& x = G.N[i]; all.push_back({x.v, x.op, x.op ? G.N[x.a].v : 0, x.op ? G.N[x.b].v : 0, x.pos}); }
        for (size_t s = 0; s < steps.size(); s++) all.push_back({steps[s].r, steps[s].op, steps[s].a, steps[s].b, pos[s]});
        unordered_map<u64, int> m; vector<int> src;
        for (size_t i = 0; i < all.size(); i++) { if (m.count(all[i].v)) continue; m[all[i].v] = out.N.size(); out.N.push_back({all[i].v, all[i].op, -1, -1, all[i].pos}); src.push_back(i); }
        for (size_t k = 0; k < out.N.size(); k++) {
            auto& x = out.N[k]; if (!x.op) continue;
            auto& t = all[src[k]]; auto ia = m.find(t.a), ib = m.find(t.b);
            if (ia == m.end() || ib == m.end()) return false;
            x.a = ia->second; x.b = ib->second;
        }
        int nn = out.N.size(); vector<vector<int>> us(nn); vector<int> indeg(nn, 0);
        for (int i = 0; i < nn; i++) if (out.N[i].op) { us[out.N[i].a].push_back(i); indeg[i]++; if (out.N[i].b != out.N[i].a) { us[out.N[i].b].push_back(i); indeg[i]++; } }
        priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> pq;
        for (int i = 0; i < nn; i++) if (!indeg[i]) pq.push({out.N[i].op ? out.N[i].pos : -1e18 + i, i});
        vector<int> ord;
        while (!pq.empty()) { int x = pq.top().second; pq.pop(); ord.push_back(x); for (int y : us[x]) if (--indeg[y] == 0) pq.push({out.N[y].pos, y}); }
        if ((int)ord.size() != nn) return false;
        out.permute(ord); out.clean();
        return out.covers();
    }
    vector<u64> makePool(const vector<char>& excl) {
        vector<pair<int, int>> v;
        for (int i = 0; i < n; i++) if (!excl[i]) v.push_back({-((int)users[i].size() * 4 + (G.N[i].op ? 0 : 8) + (int)(rng() % 3)), i});
        sort(v.begin(), v.end());
        vector<u64> p; for (auto& x : v) p.push_back(G.N[x.second].v);
        return p;
    }
    // remove nodes with mask subset of qm; rebuild targets Q in given order
    bool tryGroup(u64 qm, vector<int> Q, bool allowEq, Dag& out) {
        vector<char> inR(n, 0); int rs = 0;
        for (int x = G.ks; x < n; x++) if (mask[x] && (mask[x] & ~qm) == 0) { inR[x] = 1; rs++; }
        return tryRemove(inR, rs, Q, allowEq, out);
    }
    // nodes dominated by v: every path from them to a target passes through v (v included)
    void dominated(int v, vector<char>& inR, int& rs) {
        // alive[x]: x reaches a target without passing through v
        vector<char> alive(n, 0);
        for (int x = n - 1; x >= G.ks; x--) {
            if (x == v) continue;
            bool a = false;
            for (int t : tnode) if (t == x) { a = true; break; }
            if (!a) for (int u : users[x]) if (u != v && alive[u]) { a = true; break; }
            alive[x] = a;
        }
        inR.assign(n, 0); rs = 0;
        for (int x = G.ks; x < n; x++) if (x == v || (!alive[x] && reachesV(x, v))) { inR[x] = 1; rs++; }
    }
    bool reachesV(int x, int v) { return x < v; }   // nodes after v cannot feed v; dead-without-v nodes before v do
    int slack = 0;   // >0 only during a kick: accept rebuilds up to `slack` instructions longer
    bool tryRemove(vector<char>& inR, int rs, vector<int> Q, bool allowEq, Dag& out) {
        evals++;
        int budget = rs - (allowEq ? 0 : 1) + slack;
        if (budget < (int)Q.size()) return false;
        vector<char> excl = inR;
        { vector<int> st(Q.begin(), Q.end()); while (!st.empty()) { int x = st.back(); st.pop_back(); for (int u : users[x]) if (!excl[u]) { excl[u] = 1; st.push_back(u); } } }
        vector<u64> basePool = makePool(excl);
        vector<Step> bestSteps; vector<double> bestPos; int bestUsed = budget + 1;
        int orders = Q.size() == 1 ? 1 : (Q.size() == 2 ? 2 : nOrders);
        for (int o = 0; o < orders; o++) {
            if (o > 0) { if (Q.size() == 2) swap(Q[0], Q[1]); else shuffle(Q.begin(), Q.end(), rng); }
            S.setPool(basePool, P.L);
            vector<Step> steps, part; vector<double> pos; int used = 0; bool ok = true;
            int lim = min(budget, bestUsed - 1);
            for (size_t qi = 0; qi < Q.size(); qi++) {
                int q = Q[qi]; int left = Q.size() - qi - 1;
                int c = S.find(G.N[q].v, lim - used - left + (harvestAll ? 2 : 0), part);
                if (collect && harvestAll && c >= 0) for (auto& st : part) recipes.insert({st.r, st.op, st.a, st.b});
                if (c > lim - used - left) c = -1;
                if (c < 0) { ok = false; break; }
                double p0 = G.N[q].pos;
                for (size_t s = 0; s < part.size(); s++) { steps.push_back(part[s]); pos.push_back(p0 - 0.5 + 0.45 * (s + 1) / (part.size() + 1)); if (qi + 1 < Q.size()) S.extend(part[s].r); }
                used += part.size();
            }
            if (collect && harvestAll && ok) for (auto& st : steps) recipes.insert({st.r, st.op, st.a, st.b});
            if (ok && used < bestUsed) { bestUsed = used; bestSteps = steps; bestPos = pos; }
        }
        if (Q.size() == 2 && pairSearch) {
            S.setPool(basePool, P.L);
            vector<Step> part; int lim = min(budget, bestUsed - 1);
            int c = S.findPair(G.N[Q[0]].v, G.N[Q[1]].v, lim, part);
            if (c >= 0 && c < bestUsed) {
                bestUsed = c; bestSteps = part; bestPos.clear();
                double p0 = min(G.N[Q[0]].pos, G.N[Q[1]].pos);
                for (size_t s = 0; s < part.size(); s++) bestPos.push_back(p0 - 0.5 + 0.45 * (s + 1) / (part.size() + 1));
                pairHits++;
            }
        }
        if (bestUsed > budget) return false;
        if (!build(inR, bestSteps, bestPos, out)) return false;
        int nc = out.count();
        if (nc > G.count() + slack || (slack == 0 && nc == G.count() && !allowEq)) return false;
        if (!out.fixPressure()) { rejP++; if (nc < G.count()) rejPstrict++; return false; }
        return true;
    }
    // enumerate alternative builds (cost <= maxc, depth <= 3) for every target's exclusive cone; returns recipes
    void enumerateAlts(int perTarget) {
        analyse();
        for (int k = 0; k < nt; k++) {
            u64 bit = 1ULL << k; int cone = 0; vector<char> inR(n, 0);
            for (int x = G.ks; x < n; x++) if (mask[x] && (mask[x] & ~bit) == 0) { inR[x] = 1; cone++; }
            int t = tnode[k]; u64 T = G.N[t].v;
            vector<char> excl = inR;
            { vector<int> st{t}; while (!st.empty()) { int x = st.back(); st.pop_back(); for (int u : users[x]) if (!excl[u]) { excl[u] = 1; st.push_back(u); } } }
            S.setPool(makePool(excl), P.L);
            int got = 0; Inv iv[6], iv2[6];
            auto add = [&](const Step& s) { recipes.insert({s.r, s.op, s.a, s.b}); };
            // cost 1
            { int u = S.inS1(T); if (u >= 0) { add(S.f1s[u]); got++; } }
            // cost 2: T = w op p
            for (u64 x : S.pool) { int c = inverses(T, x, P.L, iv);
                for (int q = 0; q < c && got < perTarget; q++) { int u = S.inS1(iv[q].w); if (u >= 0) { add(S.f1s[u]); add(mk(iv[q], x, T)); got++; } } }
            if (cone < 3) continue;
            // cost 3a: T = f op g
            for (size_t a = 0; a < S.f1v.size() && got < perTarget; a++) {
                u64 x = S.f1v[a]; int c = inverses(T, x, P.L, iv);
                for (int q = 0; q < c; q++) { if (iv[q].w == x) continue; int u = S.inS1(iv[q].w);
                    if (u >= 0) { add(S.f1s[a]); add(S.f1s[u]); add(mk(iv[q], x, T)); got++; } }
            }
            // cost 3b: T = (f op y) op x
            for (size_t i = 0; i < S.pool.size() && got < perTarget; i++) {
                u64 x = S.pool[i]; int c = inverses(T, x, P.L, iv);
                for (int q = 0; q < c; q++) { u64 w1 = iv[q].w; if (S.inP(w1)) continue;
                    for (u64 y : S.pool) { int c2 = inverses(w1, y, P.L, iv2);
                        for (int q2 = 0; q2 < c2; q2++) { int u = S.inS1(iv2[q2].w);
                            if (u >= 0) { add(S.f1s[u]); add(mk(iv2[q2], y, w1)); add(mk(iv[q], x, T)); got++; } } } }
            }
        }
    }
    bool doKick() {
        analyse(); bool kicked = false;
        for (int tr = 0; tr < 30 && !kicked; tr++) {
            int kk = rnd(nt); u64 qm = 1ULL << kk;
            if (rng() % 2 && nt > 1) { int k2 = rnd(nt); qm |= 1ULL << k2; }
            vector<int> Q;
            for (int k = 0; k < nt; k++) if (qm >> k & 1) { int t = tnode[k]; if ((mask[t] & ~qm) == 0) Q.push_back(t); }
            if (Q.empty()) continue;
            shuffle(Q.begin(), Q.end(), rng);
            slack = kickSlack; Dag nd;
            if (tryGroup(qm, Q, true, nd) && nd.count() > G.count()) { G = nd; kicked = true; }
            slack = 0;
        }
        kicksDone++;
        return kicked;
    }
    int run(double tlimit) {
        double t0 = now();
        Dag best = G; int bestC = G.count(); int stallRounds = 0;
        while (now() - t0 < tlimit) {
            analyse();
            // candidate groups: single targets, pairs/triples sharing a helper
            vector<pair<double, u64>> cand; set<u64> seen;
            for (int k = 0; k < nt; k++) { u64 m = 1ULL << k; seen.insert(m); cand.push_back({(double)(rng() % 1000), m}); }
            for (int v = G.ks; v < n; v++) {
                u64 m = mask[v]; int pc = __builtin_popcountll(m);
                if (pc < 2 || pc > maxGroup || seen.count(m)) continue;
                seen.insert(m); cand.push_back({(double)(rng() % 1000), m});
            }
            // random pairs too
            for (int r = 0; r < nt; r++) { int a = rnd(nt), b = rnd(nt); if (a == b) continue; u64 m = (1ULL << a) | (1ULL << b); if (seen.count(m)) continue; seen.insert(m); cand.push_back({(double)(rng() % 1000), m}); }
            sort(cand.begin(), cand.end());
            bool moved = false, improved = false;
            // helper-rooted moves: rebuild a non-target node whose dominated subtree has >= 2 nodes
            if (helperMoves) {
                vector<int> hv; vector<char> isT(n, 0); for (int t : tnode) isT[t] = 1;
                for (int v = G.ks; v < n; v++) if (!isT[v] && __builtin_popcountll(mask[v]) >= 2) hv.push_back(v);
                shuffle(hv.begin(), hv.end(), rng);
                for (int v : hv) {
                    if (now() - t0 > tlimit) break;
                    vector<char> inR; int rs; dominated(v, inR, rs);
                    if (rs < 2) continue;
                    bool eq = (rng() % 1000) < eqProb * 1000;
                    Dag nd;
                    if (tryRemove(inR, rs, {v}, eq, nd)) {
                        bool st = nd.count() < G.count(); G = nd; accepts++; helperAcc++; harvest();
                        if (st) { improved = true; strict++; } else moved = true;
                        break;
                    }
                }
                if (improved || moved) { if (G.count() < bestC) { best = G; bestC = G.count(); stallRounds = 0; } if (moved && ++stallRounds > 200) { G = best; stallRounds = 0; } continue; }
            }
            for (auto& cm : cand) {
                if (now() - t0 > tlimit) break;
                u64 qm = cm.second; vector<int> Q;
                for (int k = 0; k < nt; k++) if (qm >> k & 1) { int t = tnode[k]; if ((mask[t] & ~qm) == 0) Q.push_back(t); }
                if (Q.empty()) continue;
                shuffle(Q.begin(), Q.end(), rng);
                bool eq = (rng() % 1000) < eqProb * 1000;
                Dag nd;
                if (tryGroup(qm, Q, eq, nd)) {
                    bool st = nd.count() < G.count(); G = nd; accepts++; harvest();
                    if (st) { improved = true; strict++; } else moved = true;
                    break;
                }
            }
            if (G.count() < bestC) { best = G; bestC = G.count(); stallRounds = 0; }
            if (improved) continue;
            if (moved) { if (++stallRounds > 200) { G = best; stallRounds = 0; if (kicks > 0 && kicksDone < kicks) doKick(); } continue; }
            // no move at all: perturb by restarting from best
            if (kicks > 0 && kicksDone < kicks && now() - t0 < tlimit && doKick()) { stallRounds = 0; continue; }
            G = best; if (++stallRounds > 3) break;
        }
        G = best; return bestC;
    }
};

static bool readPuzzle(istream& in, Puzzle& p) {
    string tag; if (!(in >> tag) || tag != "P") return false;
    int k, m; in >> p.id >> k; p.start.resize(k); for (auto& x : p.start) in >> x;
    in >> m; p.targets.resize(m); for (auto& x : p.targets) in >> x;
    int n; in >> p.L >> p.regs >> n; p.prog.resize(n);
    for (auto& x : p.prog) { string op; in >> op >> x[1] >> x[2] >> x[3]; x[0] = op[0]; }
    return true;
}

// ---------------------------------------------------------------- constructive builder
// Greedy: repeatedly build the currently cheapest remaining target (exact depth<=4 + depth-5 product
// shapes over the whole value pool); when nothing is reachable in <=5, split the smallest target as
// T = X*Y +- R (X from the pool) and build Y and R recursively.
struct Constructor {
    const Puzzle& P; Searcher S; vector<Step> steps; int maxc = 5; int randTop = 1;
    Constructor(const Puzzle& p) : P(p) {}
    void commit(const vector<Step>& st) {
        for (auto& s : st) { if (S.inP(s.r)) continue; steps.push_back(s); S.extend(s.r); }
    }
    int quick(u64 v, vector<Step>& o, int lim) { return S.find(v, lim, o); }
    void deepBuild(u64 T, int depth = 0) {
        vector<Step> o;
        if (S.find(T, maxc, o) >= 0) { commit(o); return; }
        // choose split T = X*Y + R  or  X*(Y+1) - R
        double bestEst = 1e18; u64 bX = 0, bY = 0, bR = 0; int bSign = 0;
        vector<Step> oy, orr;
        for (u64 X : S.pool) {
            if (X < 2 || X > T) continue;
            u64 Y = T / X; u64 R = T - X * Y;
            for (int sg = 0; sg < 2; sg++) {
                u64 YY = Y + sg, RR = sg ? X * (Y + 1) - T : R;
                if (YY < 1) continue;
                if (sg && (i128)X * (Y + 1) > (i128)P.L) continue;
                auto est = [&](u64 v, vector<Step>& oo) -> double {
                    if (v == 0) return 0;
                    int c = quick(v, oo, 3); if (c >= 0) return c;
                    return 2.0 + log2((double)v + 1) / 9.0;   // rough cost of a deeper build
                };
                double e = est(YY, oy) + (RR ? est(RR, orr) + 1 : 0) + 1;
                e += 1e-3 * (rng() % 1000) / 1000.0;
                if (e < bestEst) { bestEst = e; bX = X; bY = YY; bR = RR; bSign = sg; }
            }
        }
        if (!bX) { // fallback: halve
            deepBuild(T / 2, depth + 1); vector<Step> st{{1, T / 2, T / 2, (T / 2) * 2}}; commit(st);
            if (T & 1) { vector<Step> o1; deepBuild(1, depth + 1); commit({{1, T - 1, 1, T}}); }
            return;
        }
        deepBuild(bY, depth + 1);
        u64 prod = bX * bY;
        if (prod != T) {
            if (!S.inP(bR)) deepBuild(bR, depth + 1);
            commit({{3, bX, bY, prod}});
            commit({bSign ? Step{2, prod, bR, T} : Step{1, prod, bR, T}});
        } else commit({{3, bX, bY, T}});
    }
    bool run() {
        vector<u64> pool; for (u64 s : P.start) if (find(pool.begin(), pool.end(), s) == pool.end()) pool.push_back(s);
        S.setPool(pool, P.L);
        vector<u64> rem; for (u64 t : P.targets) if (!S.inP(t) && find(rem.begin(), rem.end(), t) == rem.end()) rem.push_back(t);
        // value 1 is almost always useful
        if (!S.inP(1)) commit({{4, P.start[0], P.start[0], 1}});
        while (!rem.empty()) {
            int bestC = maxc + 1; vector<int> bestI; vector<vector<Step>> bestS; vector<Step> o;
            for (size_t i = 0; i < rem.size(); i++) {
                if (S.inP(rem[i])) { bestC = 0; bestI = {(int)i}; bestS = {{}}; break; }
                int c = S.find(rem[i], bestC, o);
                if (c < 0) continue;
                if (c < bestC) { bestC = c; bestI.clear(); bestS.clear(); }
                if (c == bestC) { bestI.push_back(i); bestS.push_back(o); }
            }
            if (!bestI.empty()) {
                int k = rnd(min((int)bestI.size(), randTop));
                if (getenv("CDBG")) cerr << "  tgt bits " << 64 - __builtin_clzll(rem[bestI[k]]) << " cost " << bestC << " pool " << S.pool.size() << "\n";
                commit(bestS[k]); rem.erase(rem.begin() + bestI[k]);
            } else {
                size_t si = 0; for (size_t i = 1; i < rem.size(); i++) if (rem[i] < rem[si]) si = i;
                u64 T = rem[si]; size_t b0 = steps.size(); deepBuild(T); rem.erase(rem.begin() + si);
                if (getenv("CDBG")) cerr << "  DEEP bits " << 64 - __builtin_clzll(T) << " cost " << steps.size() - b0 << " pool " << S.pool.size() << "\n";
            }
            rem.erase(remove_if(rem.begin(), rem.end(), [&](u64 t) { return S.inP(t); }), rem.end());
        }
        return true;
    }
    // convert to a Dag (commit order = topological order)
    bool toDag(Dag& G) {
        G.P = &P; G.N.clear(); unordered_map<u64, int> m;
        for (size_t k = 0; k < P.start.size(); k++) { if (m.count(P.start[k])) continue; m[P.start[k]] = G.N.size(); G.N.push_back({P.start[k], 0, -1, -1, -1e9 + (double)k}); }
        G.ks = G.N.size();
        for (size_t i = 0; i < steps.size(); i++) {
            auto& s = steps[i]; if (m.count(s.r)) continue;
            auto ia = m.find(s.a), ib = m.find(s.b); if (ia == m.end() || ib == m.end()) return false;
            m[s.r] = G.N.size(); G.N.push_back({s.r, s.op, ia->second, ib->second, (double)i});
        }
        G.clean(); return G.covers();
    }
};

#ifndef NO_MAIN
int main(int argc, char** argv) {
    double tper = 10, eq = 0.3; u64 seed = 1; int K4 = 48, grp = 3, win = 1, deep = 1, ords = 3, pairs = 1, helpers = 1, kicks = 0, kslack = 1; string dumpPath; int hall = 0, alts = 0;
    int construct = 0, cmax = 5, crand = 1;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--construct") construct = atoi(argv[++i]);
        else if (a == "--cmax") cmax = atoi(argv[++i]);
        else if (a == "--crand") crand = atoi(argv[++i]);
    }
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--time") tper = atof(argv[++i]);
        else if (a == "--eq") eq = atof(argv[++i]);
        else if (a == "--seed") seed = atoll(argv[++i]);
        else if (a == "--K") K4 = atoi(argv[++i]);
        else if (a == "--group") grp = atoi(argv[++i]);
        else if (a == "--window") win = atoi(argv[++i]);
        else if (a == "--deep") deep = atoi(argv[++i]);
        else if (a == "--orders") ords = atoi(argv[++i]);
        else if (a == "--pairs") pairs = atoi(argv[++i]);
        else if (a == "--helpers") helpers = atoi(argv[++i]);
        else if (a == "--kicks") kicks = atoi(argv[++i]);
        else if (a == "--dump") dumpPath = argv[++i];
        else if (a == "--hall") hall = atoi(argv[++i]);
        else if (a == "--alts") alts = atoi(argv[++i]);
        else if (a == "--kslack") kslack = atoi(argv[++i]);
    }
    rng.seed(seed);
    ios::sync_with_stdio(false);
    Puzzle p;
    while (readPuzzle(cin, p)) {
        LNS L(p); L.eqProb = eq; L.maxGroup = grp; L.S.K4 = K4; L.S.window = win; L.S.deep = deep; L.nOrders = ords; L.pairSearch = pairs; L.helperMoves = helpers; L.kicks = kicks; L.kickSlack = kslack; L.collect = !dumpPath.empty(); L.harvestAll = hall;
        double t0 = now();
        int n0 = p.prog.size();
        if (construct) {
            int bestBuilt = INT_MAX; Dag bestG;
            for (int rep = 0; rep < construct; rep++) {
                Constructor C(p); C.S.K4 = K4; C.S.window = win; C.S.deep = deep; C.maxc = cmax; C.randTop = rep == 0 ? 1 : crand;
                C.run(); Dag G;
                if (!C.toDag(G)) { cerr << p.id << " construct-dag-fail\n"; continue; }
                int pr = G.pressure(); bool ok = G.fixPressure();
                cerr << p.id << " construct rep " << rep << " cost " << G.count() << " (input " << n0 << ") pressure " << pr << (ok ? "" : " PRESSURE-FAIL") << " t " << fixed << setprecision(1) << now() - t0 << "\n";
                if (ok && G.count() < bestBuilt) { bestBuilt = G.count(); bestG = G; }
            }
            if (bestBuilt == INT_MAX) continue;
            L.G = bestG; L.G.P = &p;
            if (bestBuilt >= n0 && tper <= 0) continue;
        } else if (!L.G.fromProgram(p)) { cerr << p.id << " parse-fail\n"; continue; }
        if (L.collect) L.harvest();
        L.run(tper);
        if (L.collect && alts > 0) L.enumerateAlts(alts);
        if (L.collect) {
            ofstream df(dumpPath, ios::app);
            df << "D " << p.id << " " << L.recipes.size() << "\n";
            for (auto& r : L.recipes) df << OPC[get<1>(r)] << " " << get<2>(r) << " " << get<3>(r) << " " << get<0>(r) << "\n";
        }
        vector<array<int, 4>> out;
        if (!L.G.fixPressure() || !L.G.emit(out)) { cerr << p.id << " emit-fail\n"; continue; }
        cerr << p.id << " " << n0 << " -> " << out.size() << " evals " << L.evals << " acc " << L.accepts << " strict " << L.strict << " pair " << L.pairHits << " help " << L.helperAcc << " rejP " << L.rejP << " rejPs " << L.rejPstrict << " kicks " << L.kicksDone << " t " << fixed << setprecision(1) << now() - t0 << "\n";
        if ((int)out.size() < n0) {
            cout << "R " << p.id << " " << n0 << " " << out.size() << "\n";
            for (auto& x : out) cout << (char)x[0] << " " << x[1] << " " << x[2] << " " << x[3] << "\n";
            cout.flush();
        }
    }
    return 0;
}

#endif
