// scx.cpp — scaffold exchange: вырезать helper вместе со ВСЕМИ его потребителями-целями и пересобрать
// группу совместно лучевым поиском (порядок целей x альтернативные рецепты с разными промежуточными значениями).
// Структуры DAG/поиска/планирования регистров берём из elfopt (third_party, Apache-2.0).
// Build: g++ -std=c++17 -O3 -march=native -o bin/scx cpp/scx.cpp
// Формат входа/выхода как у elfopt: блоки P ... на stdin, R <id> <n_old> <n_new> на stdout (только строгие улучшения).
#define NO_MAIN
#include "../third_party/goodpjw_dag_lns/elfopt.cpp"

// ---------------------------------------------------------------- альтернативные рецепты одной цели
// Рецепты различаются набором промежуточных значений: именно он определяет пул для остальных целей.
struct Recipe { vector<Step> st; };

static void altRecipes(Searcher& S, u64 T, int maxc, int M, vector<Recipe>& out) {
    out.clear();
    if (S.inP(T)) { out.push_back({}); return; }
    int id = S.inS1(T);
    if (id >= 0) { out.push_back({{S.f1s[id]}}); return; }      // стоимость 1: альтернативы не нужны
    if (maxc < 2) return;
    Inv iv[6], iv2[6];
    unordered_set<u64> seen;
    size_t n = S.pool.size(), off = rnd(max<size_t>(n, 1));
    // стоимость 2: T = w op p, w in S1
    for (size_t ii = 0; ii < n && (int)out.size() < M; ii++) {
        u64 x = S.pool[(ii + off) % n]; int c = inverses(T, x, S.L, iv);
        for (int q = 0; q < c && (int)out.size() < M; q++) {
            int u = S.inS1(iv[q].w); if (u < 0 || !seen.insert(iv[q].w).second) continue;
            out.push_back({{S.f1s[u], mk(iv[q], x, T)}});
        }
    }
    if (T % 2 == 0) { int u = S.inS1(T / 2); if (u >= 0 && seen.insert(T / 2).second) out.push_back({{S.f1s[u], {1, T / 2, T / 2, T}}}); }
    if (!out.empty() || maxc < 3) return;
    // стоимость 3a: T = f op g, f,g in S1
    size_t m1 = S.f1v.size(), off1 = m1 ? rnd(m1) : 0;
    set<pair<u64, u64>> seen2;
    for (size_t aa = 0; aa < m1 && (int)out.size() < M; aa++) {
        size_t a = (aa + off1) % m1; u64 x = S.f1v[a]; if (S.inP(x)) continue;
        int c = inverses(T, x, S.L, iv);
        for (int q = 0; q < c; q++) {
            u64 w = iv[q].w; if (w == x) continue;
            int u = S.inS1(w); if (u < 0) continue;
            if (!seen2.insert({min(x, w), max(x, w)}).second) continue;
            out.push_back({{S.f1s[a], S.f1s[u], mk(iv[q], x, T)}});
            if ((int)out.size() >= M) break;
        }
    }
    // стоимость 3b: T = (f op y) op x
    for (size_t ii = 0; ii < n && (int)out.size() < M; ii++) {
        u64 x = S.pool[(ii + off) % n]; int c = inverses(T, x, S.L, iv);
        for (int q = 0; q < c && (int)out.size() < M; q++) {
            u64 w1 = iv[q].w; if (S.inP(w1) || S.inS1(w1) >= 0) continue;
            for (size_t j = 0; j < n && (int)out.size() < M; j++) {
                u64 y = S.pool[j]; int c2 = inverses(w1, y, S.L, iv2);
                for (int q2 = 0; q2 < c2; q2++) {
                    int u = S.inS1(iv2[q2].w); if (u < 0) continue;
                    if (!seen2.insert({S.f1v[u], w1}).second) continue;
                    out.push_back({{S.f1s[u], mk(iv2[q2], y, w1), mk(iv[q], x, T)}});
                    break;
                }
            }
        }
    }
    if (!out.empty() || maxc < 4) return;
    // стоимость >= 4: одна (случайная) находка штатного поиска, пару попыток для разнообразия
    vector<Step> o;
    for (int r = 0; r < 2; r++) {
        int c = S.find(T, maxc, o);
        if (c < 0) break;
        out.push_back({o});
        if (c < 4) break;
    }
}

// ---------------------------------------------------------------- совместная пересборка группы
struct GroupRebuild {
    LNS& L; int beam = 8, alts = 6, cheapC = 3, prune = 0; bool dbg = false;
    long long nodes = 0;
    GroupRebuild(LNS& l) : L(l) {}

    struct State { vector<u64> extra; vector<Step> steps; vector<int> rem; int used = 0; double score = 0; u64 key = 0; };

    // Лучшая совместная пересборка целей Q (узлы DAG) из basePool с бюджетом budget. Возвращает used или -1.
    int solve(const vector<u64>& basePool, const vector<int>& Q, int budget, vector<Step>& outSteps, vector<int>& outOwner) {
        Searcher S; S.K4 = L.S.K4; S.deep = L.S.deep; S.window = L.S.window;
        vector<State> cur(1); cur[0].rem.resize(Q.size()); iota(cur[0].rem.begin(), cur[0].rem.end(), 0);
        int best = budget + 1; vector<Step> bestSteps;
        vector<Recipe> rs;
        while (!cur.empty()) {
            vector<State> nxt;
            for (auto& st : cur) {
                vector<u64> pool = basePool; for (u64 v : st.extra) pool.push_back(v);
                S.setPool(pool, L.P.L);
                int left = st.rem.size();
                // минимальные стоимости оставшихся (дешёвая оценка, глубина <= cheapC, иначе cheapC+1.5)
                // цели без дешёвого рецепта не раскрываются (оценка cheapC+1, нижняя граница 1):
                // они могут стать дешёвыми после постройки других целей группы
                vector<int> cmin(left, 99); vector<vector<Recipe>> R(left); double sum = 0, lb = 0; int found = 0;
                int lim = budget + prune - st.used - (left - 1);
                if (lim < 1) continue;
                for (int i = 0; i < left; i++) {
                    u64 T = L.G.N[Q[st.rem[i]]].v;
                    altRecipes(S, T, min(lim, cheapC), alts, R[i]);
                    if (R[i].empty()) { sum += cheapC + 1; lb += 1; continue; }
                    for (auto& r : R[i]) cmin[i] = min(cmin[i], (int)r.st.size());
                    sum += cmin[i]; lb += cmin[i]; found++;
                }
                if (!found && lim > cheapC) {     // все оставшиеся дорогие: полный поиск
                    sum = lb = 0;
                    for (int i = 0; i < left; i++) {
                        altRecipes(S, L.G.N[Q[st.rem[i]]].v, lim, 1, R[i]);
                        if (R[i].empty()) { lb = 1e9; break; }
                        cmin[i] = R[i][0].st.size(); sum += cmin[i]; lb += cmin[i]; found++;
                    }
                }
                nodes++;
                if (dbg && st.used == 0) cerr << "G k " << Q.size() << " budget " << budget << " lb " << lb << " found " << found << "\n";
                if (!found || st.used + lb > budget + prune) continue;
                for (int i = 0; i < left; i++) for (auto& r : R[i]) {
                    if ((int)r.st.size() > cmin[i] + 1) continue;
                    State ch; ch.extra = st.extra; ch.steps = st.steps; ch.used = st.used;
                    for (auto& s : r.st) {
                        bool have = S.inP(s.r); for (u64 v : ch.extra) if (v == s.r) have = true;
                        if (have) continue;
                        ch.extra.push_back(s.r); ch.steps.push_back(s); ch.used++;
                    }
                    for (int j = 0; j < left; j++) if (j != i) ch.rem.push_back(st.rem[j]);
                    double rest = sum - cmin[i], restLB = lb - cmin[i];
                    if (ch.used + restLB > budget + prune) continue;
                    if (ch.rem.empty()) {
                        if (ch.used < best) { best = ch.used; bestSteps = ch.steps; }
                        continue;
                    }
                    ch.score = ch.used + rest + 1e-3 * (rng() % 1000);
                    vector<u64> k = ch.extra; sort(k.begin(), k.end()); u64 h = 1469598103934665603ULL;
                    for (u64 v : k) { h ^= v; h *= 1099511628211ULL; } for (int j : ch.rem) { h ^= j + 77; h *= 1099511628211ULL; }
                    ch.key = h;
                    nxt.push_back(move(ch));
                }
            }
            sort(nxt.begin(), nxt.end(), [](const State& a, const State& b) { return a.score < b.score; });
            cur.clear(); unordered_set<u64> ks;
            for (auto& s : nxt) { if ((int)cur.size() >= beam) break; if (s.score >= best) break; if (ks.insert(s.key).second) cur.push_back(move(s)); }
        }
        if (best > budget) return -1;
        outSteps = bestSteps; return best;
    }
};

// ---------------------------------------------------------------- LNS на групповых ходах
struct SCX {
    LNS L; GroupRebuild GR; int gmin = 2, gmax = 8; double eqProb = 0.2;
    long long tried = 0, acc = 0, strictN = 0;
    SCX(const Puzzle& p) : L(p), GR(L) {}

    bool tryMask(u64 qm, bool allowEq, Dag& out) {
        auto& G = L.G; int n = L.n;
        vector<char> inR(n, 0); int rs = 0;
        for (int x = G.ks; x < n; x++) if (L.mask[x] && (L.mask[x] & ~qm) == 0) { inR[x] = 1; rs++; }
        vector<int> Q;
        for (int k = 0; k < L.nt; k++) if (qm >> k & 1) { int t = L.tnode[k]; if ((L.mask[t] & ~qm) == 0) Q.push_back(t); }
        if (Q.empty()) return false;
        int budget = rs - (allowEq ? 0 : 1);
        if (budget < (int)Q.size()) return false;
        vector<char> excl = inR;
        { vector<int> st(Q.begin(), Q.end()); while (!st.empty()) { int x = st.back(); st.pop_back(); for (int u : L.users[x]) if (!excl[u]) { excl[u] = 1; st.push_back(u); } } }
        vector<u64> basePool = L.makePool(excl);
        tried++;
        vector<Step> steps; vector<int> owner;
        int used = GR.solve(basePool, Q, budget, steps, owner);
        if (used < 0) return false;
        // позиции: перед самой ранней целью группы, в порядке рецептов
        double p0 = 1e18; for (int q : Q) p0 = min(p0, G.N[q].pos);
        vector<double> pos; for (size_t s = 0; s < steps.size(); s++) pos.push_back(p0 - 0.5 + 0.45 * (s + 1) / (steps.size() + 1));
        if (!L.build(inR, steps, pos, out)) return false;
        int nc = out.count();
        if (nc > G.count() || (nc == G.count() && !allowEq)) return false;
        if (!out.fixPressure()) return false;
        return true;
    }

    int run(double tlimit) {
        double t0 = now();
        Dag best = L.G; int bestC = L.G.count(); int stall = 0;
        while (now() - t0 < tlimit) {
            L.analyse();
            vector<pair<int, u64>> cand; set<u64> seen;
            for (int v = L.G.ks; v < L.n; v++) {
                u64 m = L.mask[v]; int pc = __builtin_popcountll(m);
                if (pc < gmin || pc > gmax || !seen.insert(m).second) continue;
                cand.push_back({(int)(rng() % 1000), m});
            }
            // объединения масок двух соседних helper-ов (расширенные группы)
            int base = cand.size();
            for (int r = 0; r < base; r++) {
                u64 m = cand[r].second | cand[rnd(base)].second; int pc = __builtin_popcountll(m);
                if (pc <= gmax && seen.insert(m).second) cand.push_back({(int)(rng() % 1000), m});
            }
            sort(cand.begin(), cand.end());
            bool moved = false, improved = false;
            for (auto& c : cand) {
                if (now() - t0 > tlimit) break;
                bool eq = (rng() % 1000) < eqProb * 1000;
                Dag nd;
                if (tryMask(c.second, eq, nd)) {
                    bool st = nd.count() < L.G.count(); L.G = nd; acc++;
                    if (st) { improved = true; strictN++; } else moved = true;
                    break;
                }
            }
            if (L.G.count() < bestC) { best = L.G; bestC = L.G.count(); stall = 0; }
            if (improved) continue;
            if (moved) { if (++stall > 60) { L.G = best; stall = 0; } continue; }
            break;   // полный проход без единого хода
        }
        L.G = best; return bestC;
    }
};

int main(int argc, char** argv) {
    double tper = 30; u64 seed = 1; int prune = 0, dbg = 0; int beam = 8, alts = 6, gmax = 8, gmin = 2, cheapC = 3; double eq = 0.2;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--time") tper = atof(argv[++i]);
        else if (a == "--seed") seed = atoll(argv[++i]);
        else if (a == "--beam") beam = atoi(argv[++i]);
        else if (a == "--alts") alts = atoi(argv[++i]);
        else if (a == "--gmax") gmax = atoi(argv[++i]);
        else if (a == "--gmin") gmin = atoi(argv[++i]);
        else if (a == "--cheap") cheapC = atoi(argv[++i]);
        else if (a == "--eq") eq = atof(argv[++i]);
        else if (a == "--prune") prune = atoi(argv[++i]);
        else if (a == "--dbg") dbg = 1;
    }
    rng.seed(seed); ios::sync_with_stdio(false);
    Puzzle p;
    while (readPuzzle(cin, p)) {
        SCX X(p); X.GR.beam = beam; X.GR.alts = alts; X.GR.cheapC = cheapC; X.gmax = gmax; X.gmin = gmin; X.eqProb = eq; X.GR.prune = prune; X.GR.dbg = dbg;
        double t0 = now(); int n0 = p.prog.size();
        if (!X.L.G.fromProgram(p)) { cerr << p.id << " parse-fail\n"; continue; }
        X.run(tper);
        vector<array<int, 4>> out;
        if (!X.L.G.fixPressure() || !X.L.G.emit(out)) { cerr << p.id << " emit-fail\n"; continue; }
        cerr << p.id << " " << n0 << " -> " << out.size() << " tried " << X.tried << " acc " << X.acc << " strict " << X.strictN
             << " nodes " << X.GR.nodes << " t " << fixed << setprecision(1) << now() - t0 << "\n";
        if ((int)out.size() < n0) {
            cout << "R " << p.id << " " << n0 << " " << out.size() << "\n";
            for (auto& x : out) cout << (char)x[0] << " " << x[1] << " " << x[2] << " " << x[3] << "\n";
            cout.flush();
        }
    }
}
