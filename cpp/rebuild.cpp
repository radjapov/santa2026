// rebuild.cpp — сборка программы с нуля (конструктор elfopt) + долгий LNS.
// Конструктор даёт программы с давлением регистров > 48, поэтому LNS сначала идёт с ослабленным лимитом
// (--regs0, по умолчанию 96), затем лимит опускается до 48 и поиск продолжается только с допустимой программы.
// Пишет в stderr траекторию "TRACE <id> <t> <count> <pressure>", в stdout — блок R при строгом улучшении.
// Build: g++ -std=c++17 -O3 -march=native -o bin/rebuild cpp/rebuild.cpp
#define NO_MAIN
#include "../third_party/goodpjw_dag_lns/elfopt.cpp"

int main(int argc, char** argv) {
    double tper = 600, relaxFrac = 0.5, chunk = 30; u64 seed = 1; int reps = 1, regs0 = 96, cmax = 5, crand = 3;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--time") tper = atof(argv[++i]);
        else if (a == "--seed") seed = atoll(argv[++i]);
        else if (a == "--reps") reps = atoi(argv[++i]);
        else if (a == "--regs0") regs0 = atoi(argv[++i]);
        else if (a == "--relax") relaxFrac = atof(argv[++i]);
        else if (a == "--chunk") chunk = atof(argv[++i]);
        else if (a == "--cmax") cmax = atoi(argv[++i]);
    }
    rng.seed(seed); ios::sync_with_stdio(false);
    Puzzle p0;
    while (readPuzzle(cin, p0)) {
        double t0 = now(); int n0 = p0.prog.size();
        Puzzle p = p0; p.regs = regs0;          // ослабленный лимит на время первой фазы
        // конструктор: лучшая из reps сборок (по числу инструкций, давление не проверяем)
        Dag best; int bestC = INT_MAX;
        for (int r = 0; r < reps; r++) {
            Constructor C(p); C.maxc = cmax; C.randTop = r == 0 ? 1 : crand;
            C.run(); Dag G;
            if (!C.toDag(G)) continue;
            if (G.count() < bestC) { bestC = G.count(); best = G; }
        }
        if (bestC == INT_MAX) { cerr << p.id << " construct-fail\n"; continue; }
        cerr << "TRACE " << p.id << " " << fixed << setprecision(0) << now() - t0 << " " << bestC << " " << best.pressure() << " construct (input " << n0 << ")\n";
        LNS L(p); L.G = best; L.G.P = &p; L.kicks = 1000000; L.kickSlack = 2;
        bool strictPhase = false;
        while (now() - t0 < tper) {
            if (!strictPhase && now() - t0 > tper * relaxFrac) {
                p.regs = p0.regs;
                if (L.G.fixPressure()) strictPhase = true;
                else { p.regs = max(p0.regs, p.regs); cerr << "TRACE " << p.id << " pressure-fix-failed " << L.G.pressure() << "\n"; p.regs = regs0; }
                if (strictPhase) cerr << "TRACE " << p.id << " strict-regs from " << L.G.count() << "\n";
            }
            L.kicksDone = 0;
            L.run(min(chunk, tper - (now() - t0)));
            cerr << "TRACE " << p.id << " " << fixed << setprecision(0) << now() - t0 << " " << L.G.count() << " " << L.G.pressure() << (strictPhase ? " strict" : " relaxed") << "\n";
        }
        p.regs = p0.regs;
        vector<array<int, 4>> out;
        if (!L.G.fixPressure() || !L.G.emit(out)) { cerr << p.id << " final-pressure-fail count " << L.G.count() << " (input " << n0 << ")\n"; continue; }
        cerr << p.id << " " << n0 << " -> " << out.size() << " t " << fixed << setprecision(0) << now() - t0 << "\n";
        if ((int)out.size() < n0) {
            cout << "R " << p.id << " " << n0 << " " << out.size() << "\n";
            for (auto& x : out) cout << (char)x[0] << " " << x[1] << " " << x[2] << " " << x[3] << "\n";
            cout.flush();
        }
    }
}
