// closure.cpp — совместный отбор набора значений (идея «greedy closure» из публичного ноутбука wangjakie,
// реализация своя). Универсум = значения текущей программы + библиотека значений, которые получаются из пар
// универсума >= wmin способами. Рандомизированное удаление: выбрасываем значения в случайном порядке, пока все
// цели достижимы по арифметическому замыканию. Длина программы = число оставшихся нестартовых значений.
// Найденный меньший набор расписывается по регистрам планировщиком elfopt (Dag::fixPressure/emit).
// Вход/выход — формат elfopt (P-блок → R-блок только при строгом улучшении).
// Build: g++ -std=c++17 -O3 -march=native -pthread -o bin/closure cpp/closure.cpp
#define NO_MAIN
#include "../third_party/goodpjw_dag_lns/elfopt.cpp"
#include <thread>
#include <mutex>
#include <atomic>

struct Rule { int a, b, op; };

struct Closure {
    const Puzzle& P; Dag G;
    int ks = 0, n0 = 0, n = 0, W = 0;
    vector<u64> vals; unordered_map<u64, int> id;
    vector<vector<Rule>> rules;           // правила получения каждого значения
    vector<int> goals;
    Closure(const Puzzle& p) : P(p) {}

    static void walk(u64 a, u64 b, u64 L, const function<void(u64, int, bool)>& f) {
        u64 r;
        if (ap(1, a, b, L, r)) f(r, 1, false);
        if (ap(2, a, b, L, r)) f(r, 2, false);
        if (ap(2, b, a, L, r)) f(r, 2, true);
        if (ap(3, a, b, L, r)) f(r, 3, false);
        if (ap(4, a, b, L, r)) f(r, 4, false);
        if (a != b && ap(4, b, a, L, r)) f(r, 4, true);
    }

    bool init(int budget, int wmin, int levels, u64 seed) {
        if (!G.fromProgram(P)) return false;
        ks = G.ks; n0 = G.N.size();
        for (auto& x : G.N) { id[x.v] = vals.size(); vals.push_back(x.v); }
        mt19937_64 rr(seed);
        for (int lv = 0; lv < levels; lv++) {
            int m = vals.size();
            unordered_map<u64, int> wit;
            for (int i = 0; i < m; i++) for (int j = 0; j <= i; j++)
                walk(vals[i], vals[j], P.L, [&](u64 x, int, bool) { if (x != vals[i] && x != vals[j] && !id.count(x)) wit[x]++; });
            vector<pair<u64, u64>> rk;
            for (auto& [x, w] : wit) if (w >= wmin) rk.push_back({((u64)min(w, 8) << 40) | (rr() & ((1ULL << 40) - 1)), x});
            sort(rk.rbegin(), rk.rend());
            int take = min<int>(budget / levels, rk.size());
            for (int k = 0; k < take; k++) { id[rk[k].second] = vals.size(); vals.push_back(rk[k].second); }
        }
        n = vals.size(); W = (n + 63) / 64;
        rules.assign(n, {});
        for (int i = 0; i < n; i++) for (int j = 0; j <= i; j++)
            walk(vals[i], vals[j], P.L, [&](u64 x, int op, bool sw) {
                auto it = id.find(x); if (it == id.end()) return;
                int t = it->second; if (t < ks || t == i || t == j) return;
                if (sw) rules[t].push_back({j, i, op}); else rules[t].push_back({i, j, op});
            });
        for (u64 t : P.targets) goals.push_back(id.at(t));
        return true;
    }

    inline bool on(const vector<u64>& s, int v) const { return (s[v >> 6] >> (v & 63)) & 1; }
    // все цели достижимы, используя только allowed (+ старты)?
    bool covers(const vector<u64>& allowed, vector<u64>& seen) const {
        seen.assign(W, 0);
        for (int v = 0; v < ks; v++) seen[v >> 6] |= 1ULL << (v & 63);
        bool prog = true;
        while (prog) {
            prog = false;
            for (int v = ks; v < n; v++) {
                if (!on(allowed, v) || on(seen, v)) continue;
                for (auto& r : rules[v]) if (on(seen, r.a) && on(seen, r.b)) { seen[v >> 6] |= 1ULL << (v & 63); prog = true; break; }
            }
            bool ok = true; for (int g : goals) if (!on(seen, g)) { ok = false; break; }
            if (ok) return true;
        }
        return false;
    }

    // одна рандомизированная попытка; возвращает размер (число нестартовых значений) и набор
    int lane(mt19937_64& rng, int thr, vector<u64>& allowed) const {
        allowed.assign(W, 0); int cnt = 0;
        for (int v = ks; v < n0; v++) { allowed[v >> 6] |= 1ULL << (v & 63); cnt++; }
        for (int v = n0; v < n; v++) if ((int)(rng() % 100) < thr) { allowed[v >> 6] |= 1ULL << (v & 63); cnt++; }
        vector<pair<double, int>> ord;
        uniform_real_distribution<double> U(0, 1);
        double bias = U(rng) * 1.6 - 0.75;
        for (int v = ks; v < n; v++) {
            if (!on(allowed, v)) continue;
            bool isGoal = false; for (int g : goals) if (g == v) isGoal = true;
            if (isGoal) continue;
            ord.push_back({U(rng) + (v >= n0 ? bias : 0.0), v});
        }
        sort(ord.begin(), ord.end());
        vector<u64> seen;
        for (auto& [_, v] : ord) {
            allowed[v >> 6] &= ~(1ULL << (v & 63));
            if (covers(allowed, seen)) cnt--; else allowed[v >> 6] |= 1ULL << (v & 63);
        }
        return cnt;
    }

    // набор → Dag (в порядке достижения) → clean → регистры
    bool realize(const vector<u64>& allowed, vector<array<int, 4>>& out, int& cnt) const {
        Dag D; D.P = &P; D.N.assign(G.N.begin(), G.N.begin() + ks); D.ks = ks;
        vector<int> at(n, -1); for (int v = 0; v < ks; v++) at[v] = v;
        bool prog = true;
        while (prog) {
            prog = false;
            for (int v = ks; v < n; v++) {
                if (!on(allowed, v) || at[v] >= 0) continue;
                for (auto& r : rules[v]) if (at[r.a] >= 0 && at[r.b] >= 0) {
                    at[v] = D.N.size(); D.N.push_back({vals[v], r.op, at[r.a], at[r.b], (double)D.N.size()}); prog = true; break;
                }
            }
        }
        D.reindex(); if (!D.covers()) return false;
        D.clean(); cnt = D.count();
        if (!D.fixPressure()) return false;
        return D.emit(out);
    }
};

int main(int argc, char** argv) {
    double tper = 60; u64 seed = 1; int budget = 256, wmin = 2, levels = 1, threads = 1;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--time") tper = atof(argv[++i]);
        else if (a == "--seed") seed = atoll(argv[++i]);
        else if (a == "--budget") budget = atoi(argv[++i]);
        else if (a == "--wmin") wmin = atoi(argv[++i]);
        else if (a == "--levels") levels = atoi(argv[++i]);
        else if (a == "--threads") threads = atoi(argv[++i]);
    }
    ios::sync_with_stdio(false);
    Puzzle p;
    while (readPuzzle(cin, p)) {
        double t0 = now(); int n0 = p.prog.size();
        Closure C(p);
        if (!C.init(budget, wmin, levels, seed)) { cerr << p.id << " bad-input\n"; continue; }
        size_t nr = 0; for (auto& r : C.rules) nr += r.size();
        int cur = C.G.count();
        mutex mu; vector<array<int, 4>> best; int bestLen = n0, bestSet = INT_MAX, pfail = 0;
        atomic<long> lanes{0};
        auto work = [&](int th) {
            mt19937_64 rng(seed * 1000003 + th);
            vector<u64> al;
            while (now() - t0 < tper) {
                int thr = 8 + (int)(rng() % 7) * 13;
                int c = C.lane(rng, thr, al); lanes++;
                { lock_guard<mutex> g(mu); bestSet = min(bestSet, c); }
                if (c >= bestLen) continue;
                vector<array<int, 4>> out; int cc;
                bool ok = C.realize(al, out, cc);
                lock_guard<mutex> g(mu);
                if (!ok) { pfail++; continue; }
                if ((int)out.size() < bestLen) { bestLen = out.size(); best = out; }
            }
        };
        vector<thread> T; for (int t = 0; t < threads; t++) T.emplace_back(work, t);
        for (auto& t : T) t.join();
        cerr << p.id << " n " << n0 << " vals " << C.n0 << "+" << (C.n - C.n0) << " rules " << nr << " lanes " << lanes
             << " bestset " << bestSet << " (cur " << cur << ") pfail " << pfail << " -> " << bestLen << "\n";
        if (bestLen < n0) {
            cout << "R " << p.id << " " << n0 << " " << best.size() << "\n";
            for (auto& x : best) cout << (char)x[0] << " " << x[1] << " " << x[2] << " " << x[3] << "\n";
            cout.flush();
        }
    }
}
