// beamcon.cpp — лучевой конструктор программ с нуля (идея: сильные команды строят mixed/shared заново, см. CLAUDE.md, tkm2261).
// Состояние = частичная программа (пул значений + шаги + оставшиеся цели). Расширение: K самых дешёвых целей ×
// A альтернативных рецептов (разные промежуточные значения). Оценка ребёнка = потрачено + w·Σ стоимость оставшихся
// целей при НОВОМ пуле (поиск до hlim) — взгляд вперёд: выигрывают рецепты, после которых дешевеют остальные цели.
// Пока ни одна цель не строится за <= maxc, каркас достраивается deepBuild elfopt (X·Y ± R) по одной из мелких целей.
// После сборки — короткая доводка elfopt-LNS (ослабленные регистры, затем 48). Много «свежих стартов» за --time.
// Формат входа/выхода elfopt (P-блок → R-блок при строгом улучшении); статистика в stderr.
// Build: g++ -std=c++17 -O3 -march=native -o bin/beamcon cpp/beamcon.cpp
#define NO_MAIN
#include "../third_party/goodpjw_dag_lns/elfopt.cpp"
#include "recipes.h"

struct BeamCon {
    const Puzzle& P;
    int B = 8, K = 3, A = 4, maxc = 4, selc = 3, hlim = 2; double w = 1.0, hfail = 3.5;
    long long evals = 0;
    BeamCon(const Puzzle& p) : P(p) {}
    struct St { Searcher S; vector<Step> steps; vector<u64> rem; double score = 0; };

    void commit(St& s, const vector<Step>& st) {
        for (auto& x : st) { if (s.S.inP(x.r)) continue; s.steps.push_back(x); s.S.extend(x.r); }
        s.rem.erase(remove_if(s.rem.begin(), s.rem.end(), [&](u64 t) { return s.S.inP(t); }), s.rem.end());
    }
    double h(St& s) {
        double sum = 0; vector<Step> o;
        for (u64 t : s.rem) { int c = s.S.find(t, hlim, o); sum += c >= 0 ? c : hfail; }
        evals++;
        return sum;
    }
    u64 key(const St& s) const {   // дедупликация по набору значений
        u64 k = 0; for (u64 v : s.S.pool) { u64 x = v * 0x9e3779b97f4a7c15ULL; x ^= x >> 29; k += x; } return k;
    }
    double T0 = 0;
    bool run(vector<Step>& out, double deadline) {
        T0 = now();
        St s0; vector<u64> pool;
        for (u64 x : P.start) if (find(pool.begin(), pool.end(), x) == pool.end()) pool.push_back(x);
        s0.S.setPool(pool, P.L);
        for (u64 t : P.targets) if (!s0.S.inP(t) && find(s0.rem.begin(), s0.rem.end(), t) == s0.rem.end()) s0.rem.push_back(t);
        if (!s0.S.inP(1)) commit(s0, {{4, P.start[0], P.start[0], 1}});
        vector<St> beam; beam.push_back(move(s0));
        while (true) {
            if (now() > deadline) return false;
            bool done = true; for (auto& s : beam) if (!s.rem.empty()) done = false;
            if (done) break;
            vector<St> kids; unordered_set<u64> seen;
            for (auto& s : beam) {
                if (s.rem.empty()) { if (seen.insert(key(s)).second) kids.push_back(s); continue; }
                // стоимости целей при текущем пуле
                vector<pair<int, int>> cs; vector<Step> o;
                for (size_t i = 0; i < s.rem.size(); i++) { int c = s.S.find(s.rem[i], selc, o); if (c >= 0) cs.push_back({c * 1000 + (int)rnd(1000), (int)i}); }
                if (cs.empty() && maxc > selc)   // дешёвых нет: поиск до maxc, но только до первой находки на цель
                    for (size_t i = 0; i < s.rem.size() && (int)cs.size() < K; i++) { int c = s.S.find(s.rem[i], maxc, o); if (c >= 0) cs.push_back({c * 1000 + (int)rnd(1000), (int)i}); }
                if (cs.empty()) {
                    // каркас: deepBuild одной из мелких целей (несколько вариантов для разнообразия)
                    vector<int> idx(s.rem.size()); iota(idx.begin(), idx.end(), 0);
                    sort(idx.begin(), idx.end(), [&](int a, int b) { return s.rem[a] < s.rem[b]; });
                    for (int v = 0; v < min<int>(K, idx.size()); v++) {
                        St c = s; Constructor C(P); C.S = c.S; C.maxc = maxc; C.deepBuild(c.rem[idx[v]]);
                        c.S = C.S; for (auto& x : C.steps) c.steps.push_back(x);
                        c.rem.erase(remove_if(c.rem.begin(), c.rem.end(), [&](u64 t) { return c.S.inP(t); }), c.rem.end());
                        c.score = c.steps.size() + w * h(c);
                        if (seen.insert(key(c)).second) kids.push_back(move(c));
                    }
                    continue;
                }
                sort(cs.begin(), cs.end());
                for (int j = 0; j < min<int>(K, cs.size()); j++) {
                    int i = cs[j].second, c0 = cs[j].first / 1000; u64 T = s.rem[i];
                    vector<Recipe> R; altRecipes(s.S, T, c0, A, R);
                    for (auto& r : R) {
                        if ((int)r.st.size() > c0 + 1) continue;
                        St c = s; commit(c, r.st);
                        c.score = c.steps.size() + w * h(c);
                        if (seen.insert(key(c)).second) kids.push_back(move(c));
                    }
                }
            }
            sort(kids.begin(), kids.end(), [](const St& a, const St& b) { return a.score < b.score; });
            if (getenv("BDBG") && !kids.empty()) cerr << "  step rem " << kids[0].rem.size() << " steps " << kids[0].steps.size() << " pool " << kids[0].S.pool.size() << " kids " << kids.size() << " score " << kids[0].score << " evals " << evals << " t " << fixed << setprecision(1) << now() - T0 << "\n";
            if ((int)kids.size() > B) kids.resize(B);
            beam.swap(kids);
        }
        int bi = 0; for (int i = 1; i < (int)beam.size(); i++) if (beam[i].steps.size() < beam[bi].steps.size()) bi = i;
        out = beam[bi].steps; return true;
    }
};

int main(int argc, char** argv) {
    double tper = 300, polish = 60, relaxFrac = 0.5; u64 seed = 1; int B = 8, K = 3, A = 4, hlim = 2, regs0 = 96, selc = 3; double w = 1.0;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--time") tper = atof(argv[++i]); else if (a == "--seed") seed = atoll(argv[++i]);
        else if (a == "--beam") B = atoi(argv[++i]); else if (a == "--k") K = atoi(argv[++i]);
        else if (a == "--alts") A = atoi(argv[++i]); else if (a == "--hlim") hlim = atoi(argv[++i]);
        else if (a == "--w") w = atof(argv[++i]); else if (a == "--polish") polish = atof(argv[++i]);
        else if (a == "--regs0") regs0 = atoi(argv[++i]);
        else if (a == "--selc") selc = atoi(argv[++i]);
    }
    rng.seed(seed); ios::sync_with_stdio(false);
    Puzzle p0;
    while (readPuzzle(cin, p0)) {
        double t0 = now(); int n0 = p0.prog.size(); int bestLen = INT_MAX, starts = 0, bestCon = INT_MAX; vector<array<int, 4>> best;
        while (now() - t0 < tper) {
            double ts = now();
            BeamCon BC(p0); BC.B = B; BC.K = K; BC.A = A; BC.hlim = hlim; BC.w = w; BC.selc = selc;
            vector<Step> st;
            if (!BC.run(st, t0 + tper)) break;
            starts++;
            Constructor C(p0); C.steps = st; Dag G; if (!C.toDag(G)) continue;
            G.clean(); int con = G.count(); bestCon = min(bestCon, con);
            double tc = now() - ts;
            // доводка: LNS с ослабленными регистрами, затем строгие 48
            Puzzle p = p0; p.regs = regs0;
            LNS L(p); L.G = G; L.G.P = &p; L.kicks = 1000000; L.kickSlack = 2;
            double tp = min(polish, t0 + tper - now());
            if (tp > 1) { L.kicksDone = 0; L.run(tp * relaxFrac); }
            p.regs = p0.regs;
            int pol = L.G.count();
            if (!L.G.fixPressure()) { cerr << p0.id << " start " << starts << " con " << con << " pol " << pol << " pressure-fail " << L.G.pressure() << " t " << fixed << setprecision(0) << tc << "\n"; continue; }
            if (tp > 1) { L.kicksDone = 0; L.run(tp * (1 - relaxFrac)); }
            vector<array<int, 4>> out;
            if (!L.G.fixPressure() || !L.G.emit(out)) continue;
            cerr << p0.id << " start " << starts << " con " << con << " pol " << out.size() << " (cur " << n0 << ") t_con " << fixed << setprecision(0) << tc << "s evals " << BC.evals << "\n";
            if ((int)out.size() < bestLen) { bestLen = out.size(); best = out; }
        }
        cerr << p0.id << " DONE starts " << starts << " best_con " << bestCon << " best " << (bestLen == INT_MAX ? -1 : bestLen) << " cur " << n0 << "\n";
        if (bestLen < n0) {
            cout << "R " << p0.id << " " << n0 << " " << best.size() << "\n";
            for (auto& x : best) cout << (char)x[0] << " " << x[1] << " " << x[2] << " " << x[3] << "\n";
            cout.flush();
        }
    }
}
