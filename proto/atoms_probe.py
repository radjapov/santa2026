"""Есть ли в программах значения-«атомы»: v >= 1e5, такие что (t - e) % v == 0 для >= 3 целей при |e| <= E."""
import json, csv, sys, statistics as st
sys.path.insert(0, '.')
from store import load_best, load_puzzles
OPS = {'+': lambda a, b: a + b, '-': lambda a, b: a - b, '*': lambda a, b: a * b, '/': lambda a, b: a // b}
E = 24
def values(p, prog):
    s, t, L = p; regs = list(s) + [None] * 48; out = set()
    for op, i, j, d in prog:
        v = OPS[op](regs[i], regs[j]); regs[d] = v; out.add(v)
    return out
def atoms(p, vals):
    s, T, L = p; res = []
    for v in vals:
        if v < 10**5: continue
        c = sum(1 for t in T if t > v and min((t - e) % v for e in range(-E, E + 1)) == 0)
        if c >= 3: res.append((v, c))
    return res
P = load_puzzles(); old = json.load(open(sys.argv[1])); ex = json.load(open('external/src/tkm2261_exposed.json'))
fam = {r[list(r)[0]]: r[list(r)[1]] for r in csv.DictReader(open('data/families_inferred.csv'))}
agg = {}
for pid, prog in ex.items():
    if len(prog) >= old[pid]['score']: continue
    f = fam.get(pid)
    a = atoms(P[pid], values(P[pid], old[pid]['program'])); b = atoms(P[pid], values(P[pid], prog))
    g = agg.setdefault(f, [0, 0, 0, 0, 0])
    g[0] += 1; g[1] += len(a); g[2] += len(b); g[3] += sum(c for _, c in a); g[4] += sum(c for _, c in b)
    if pid in ('scored_0217', 'scored_0119', 'scored_0369'):
        print(pid, f, 'targets', len(P[pid][1]), 'old atoms', sorted(a, key=lambda x: -x[1])[:5], '| tkm atoms', sorted(b, key=lambda x: -x[1])[:5])
for f, g in agg.items():
    print(f, g[0], 'puzzles | atoms per program old', round(g[1] / g[0], 1), 'tkm', round(g[2] / g[0], 1), '| covered-target links old', round(g[3] / g[0], 1), 'tkm', round(g[4] / g[0], 1))
