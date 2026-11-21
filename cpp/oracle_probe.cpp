// oracle_probe.cpp — зонд: насколько полный MITM-оракул (стоимость 4: S1∘S2, стоимость 5: S2∘S2)
// сильнее штатного find (elfopt) для целей с большим исключительным конусом.
// Build: g++ -std=c++17 -O3 -march=native -o bin/oracle_probe cpp/oracle_probe.cpp
#define NO_MAIN
#include "../third_party/goodpjw_dag_lns/elfopt.cpp"

struct Big {
    u64 L; vector<u64> P, S1, S2;   // S1, S2 отсортированы, без повторов, не пересекаются с меньшими уровнями
    void build(const vector<u64>& pool, u64 lim) {
        L = lim; P = pool; sort(P.begin(), P.end()); P.erase(unique(P.begin(), P.end()), P.end());
        S1.clear();
        for (size_t i = 0; i < P.size(); i++) for (size_t j = i; j < P.size(); j++) {
            u64 a = P[i], b = P[j], r;
            if (ap(1, a, b, L, r)) S1.push_back(r);
            if (ap(3, a, b, L, r)) S1.push_back(r);
            if (b > a) S1.push_back(b - a);
            if (b % a == 0 && b != a) S1.push_back(b / a);
        }
        sort(S1.begin(), S1.end()); S1.erase(unique(S1.begin(), S1.end()), S1.end());
        S1.erase(remove_if(S1.begin(), S1.end(), [&](u64 x) { return binary_search(P.begin(), P.end(), x); }), S1.end());
        S2.clear(); S2.reserve(S1.size() * P.size() * 3);
        for (u64 s : S1) for (u64 p : P) {
            u64 r;
            if (ap(1, s, p, L, r)) S2.push_back(r);
            if (ap(3, s, p, L, r)) S2.push_back(r);
            if (s > p) S2.push_back(s - p); else if (p > s) S2.push_back(p - s);
            if (s % p == 0) S2.push_back(s / p);
            if (p % s == 0) S2.push_back(p / s);
        }
        sort(S2.begin(), S2.end()); S2.erase(unique(S2.begin(), S2.end()), S2.end());
        auto inLow = [&](u64 x) { return binary_search(P.begin(), P.end(), x) || binary_search(S1.begin(), S1.end(), x); };
        S2.erase(remove_if(S2.begin(), S2.end(), inLow), S2.end());
    }
    bool inS2(u64 x) const { return binary_search(S2.begin(), S2.end(), x); }
    bool inS1(u64 x) const { return binary_search(S1.begin(), S1.end(), x); }
    bool inP(u64 x) const { return binary_search(P.begin(), P.end(), x); }
    int lvl(u64 x) const { return inP(x) ? 0 : inS1(x) ? 1 : inS2(x) ? 2 : 9; }
    // T = A op B, A in S1, B in S2 (все 6 обращений)
    bool cost4(u64 T) const {
        Inv iv[6];
        for (u64 a : S1) { int c = inverses(T, a, L, iv); for (int q = 0; q < c; q++) if (inS2(iv[q].w)) return true; }
        return false;
    }
    HMap H2;
    void hashS2() { H2.init(S2.size()); for (size_t i = 0; i < S2.size(); i++) H2.put(S2[i], (int)i); }
    // стоимость 5, формы с цепочкой: T = a op ((s op p) op q)  [S1∘S3chain]  и  T = p op (a op s2)  [P∘(S1∘S2)]
    bool cost5chain(u64 T) const {
        Inv iv[6], iv2[6];
        for (u64 a : S1) { int c = inverses(T, a, L, iv);
            for (int q = 0; q < c; q++) { u64 w1 = iv[q].w;
                for (u64 p : P) { int c2 = inverses(w1, p, L, iv2); for (int q2 = 0; q2 < c2; q2++) if (H2.get(iv2[q2].w) >= 0) return true; } } }
        for (u64 p : P) { int c = inverses(T, p, L, iv);
            for (int q = 0; q < c; q++) { u64 w1 = iv[q].w;
                for (u64 a : S1) { int c2 = inverses(w1, a, L, iv2); for (int q2 = 0; q2 < c2; q2++) if (H2.get(iv2[q2].w) >= 0) return true; } } }
        return false;
    }
    // T = A op B, A,B in S2
    bool cost5(u64 T) const {
        const auto& V = S2; size_t n = V.size(); if (!n) return false;
        // A + B = T : два указателя
        { size_t i = 0, j = n; while (j > 0 && V[j - 1] >= T) j--;
          if (j) { j--; while (i <= j) { u64 s = V[i] + V[j]; if (s == T) return true; if (s < T) i++; else { if (!j) break; j--; } } } }
        // A - B = T : для каждого B, A = T + B
        { size_t j = 0; for (size_t i = 0; i < n; i++) { if (V[i] > L - T) break; u64 need = V[i] + T; while (j < n && V[j] < need) j++; if (j == n) break; if (V[j] == need) return true; } }
        // A * B = T : делители T среди S2 (перебор A в S2 с A*A <= T и T % A == 0)
        for (size_t i = 0; i < n && (i128)V[i] * V[i] <= (i128)T; i++) if (T % V[i] == 0 && inS2(T / V[i])) return true;
        // A / B = T : A = T*B
        for (size_t i = 0; i < n; i++) { i128 a = (i128)T * V[i]; if (a > (i128)L) break; if (inS2((u64)a)) return true; }
        return false;
    }
};

int main(int argc, char** argv) {
    int cmin = 5; double tmax = 1e9;
    for (int i = 1; i < argc; i++) { string a = argv[i]; if (a == "--cmin") cmin = atoi(argv[++i]); else if (a == "--tmax") tmax = atof(argv[++i]); }
    Puzzle p; double t0 = now();
    while (readPuzzle(cin, p)) {
        LNS L(p); if (!L.G.fromProgram(p)) continue; L.analyse();
        auto& G = L.G; int n = L.n;
        for (int k = 0; k < L.nt && now() - t0 < tmax; k++) {
            u64 qm = 1ULL << k; int t = L.tnode[k];
            if ((L.mask[t] & ~qm) != 0) continue;           // цель питает другие цели
            vector<char> inR(n, 0); int rs = 0;
            for (int x = G.ks; x < n; x++) if (L.mask[x] == qm) { inR[x] = 1; rs++; }
            if (rs < cmin) continue;
            vector<char> excl = inR;
            vector<u64> pool = L.makePool(excl);
            double t1 = now();
            L.S.setPool(pool, p.L); vector<Step> o;
            int fc = L.S.find(G.N[t].v, rs - 1, o);
            double t2 = now();
            Big B; B.build(pool, p.L); double t3 = now();
            u64 T = G.N[t].v; int lv = B.lvl(T);
            bool c4 = lv <= 2 || B.cost4(T); double t4 = now();
            bool c5 = c4 || B.cost5(T); double t5 = now();
            bool c5b = false; double t6 = t5;
            if (!c5 && rs >= 6) { B.hashS2(); c5b = B.cost5chain(T); t6 = now(); }
            printf("%s tgt %d bits %d cone %d pool %zu S1 %zu S2 %zu | find %d | lvl %d c4 %d c5 %d c5b %d | t find %.2f build %.2f c4 %.2f c5 %.2f c5b %.1f\n",
                   p.id.c_str(), k, 64 - __builtin_clzll(T), rs, pool.size(), B.S1.size(), B.S2.size(), fc, lv, c4, c5, c5b, t2 - t1, t3 - t2, t4 - t3, t5 - t4, t6 - t5);
            fflush(stdout);
        }
    }
}
