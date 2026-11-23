// scaffold_probe.cpp — диагностика: где проигрываем — в выборе общего каркаса или в достройке целей поверх него?
// Берём программу (вход), оставляем только «каркас» = значения, от которых зависят >= 2 цели (замкнуто по зависимостям),
// выбрасываем все «личные» цепочки целей и достраиваем цели жадно (дешёвая цель первой, find <= maxc, иначе deepBuild),
// затем короткая доводка LNS. Печать: длина входа, размер каркаса, личных во входе, достройка, после доводки.
// Build: g++ -std=c++17 -O3 -march=native -o bin/scaffold_probe cpp/scaffold_probe.cpp
#define NO_MAIN
#include "../third_party/goodpjw_dag_lns/elfopt.cpp"

int main(int argc, char** argv) {
    double polish = 30; int maxc = 5, reps = 4; u64 seed = 1;
    for (int i = 1; i < argc; i++) { string a = argv[i];
        if (a == "--polish") polish = atof(argv[++i]); else if (a == "--maxc") maxc = atoi(argv[++i]);
        else if (a == "--reps") reps = atoi(argv[++i]); else if (a == "--seed") seed = atoll(argv[++i]); }
    rng.seed(seed);
    Puzzle p;
    while (readPuzzle(cin, p)) {
        int n0 = p.prog.size();
        LNS L0(p); if (!L0.G.fromProgram(p)) { cerr << p.id << " bad\n"; continue; }
        L0.analyse();
        Dag& G = L0.G; int n = G.N.size();
        unordered_set<u64> T(p.targets.begin(), p.targets.end());
        // каркас: значения с маской >= 2 целей (в порядке программы — топологический)
        vector<Step> scaf; int nscaf = 0;
        for (int x = G.ks; x < n; x++) if (__builtin_popcountll(L0.mask[x]) >= 2) { scaf.push_back({G.N[x].op, G.N[G.N[x].a].v, G.N[G.N[x].b].v, G.N[x].v}); nscaf++; }
        int bestB = INT_MAX, bestP = INT_MAX;
        for (int r = 0; r < reps; r++) {
            Constructor C(p); C.maxc = maxc; C.randTop = r == 0 ? 1 : 3;
            vector<u64> pool; for (u64 s : p.start) if (find(pool.begin(), pool.end(), s) == pool.end()) pool.push_back(s);
            C.S.setPool(pool, p.L);
            C.commit(scaf);
            vector<u64> rem; for (u64 t : p.targets) if (!C.S.inP(t) && find(rem.begin(), rem.end(), t) == rem.end()) rem.push_back(t);
            while (!rem.empty()) {   // жадная достройка (как Constructor::run)
                int bestC = maxc + 1; vector<int> bi; vector<vector<Step>> bs; vector<Step> o;
                for (size_t i = 0; i < rem.size(); i++) {
                    int c = C.S.find(rem[i], bestC, o); if (c < 0) continue;
                    if (c < bestC) { bestC = c; bi.clear(); bs.clear(); }
                    if (c == bestC) { bi.push_back(i); bs.push_back(o); }
                }
                if (!bi.empty()) { int k = rnd(min((int)bi.size(), C.randTop)); C.commit(bs[k]); rem.erase(rem.begin() + bi[k]); }
                else { size_t si = 0; for (size_t i = 1; i < rem.size(); i++) if (rem[i] < rem[si]) si = i; u64 t = rem[si]; rem.erase(rem.begin() + si); C.deepBuild(t); }
                rem.erase(remove_if(rem.begin(), rem.end(), [&](u64 t) { return C.S.inP(t); }), rem.end());
            }
            Dag D; if (!C.toDag(D)) continue; D.clean();
            int built = D.count(); bestB = min(bestB, built);
            Puzzle q = p; q.regs = 96; LNS L(q); L.G = D; L.G.P = &q; L.kicks = 1000000; L.kickSlack = 2;
            L.kicksDone = 0; L.run(polish);
            bestP = min(bestP, L.G.count());
        }
        printf("%s in %d scaffold %d private_in %d | rebuilt %d polished %d | rebuilt-in %+d polished-in %+d\n",
               p.id.c_str(), n0, nscaf, G.count() - nscaf, bestB, bestP, bestB - n0, bestP - n0);
        fflush(stdout);
    }
}
