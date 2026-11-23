import json, csv, sys, collections, statistics as st
sys.path.insert(0, '.')
from store import load_puzzles
OPS = {'+': lambda a, b: a + b, '-': lambda a, b: a - b, '*': lambda a, b: a * b, '/': lambda a, b: a // b}
def analyse(p, prog):
    s, T, L = p; T = set(T); S = set(s); regs = list(s) + [None] * 48; defs = {}
    for op, i, j, d in prog:
        a, b = regs[i], regs[j]; v = OPS[op](a, b); regs[d] = v
        if v not in defs and v not in S: defs[v] = (op, a, b)
    # для каждой цели: последняя операция и типы операндов
    kinds = collections.Counter(); tt = 0; lastop = collections.Counter()
    users = collections.Counter()
    def deps(v, acc):
        if v in S or v in acc: return
        acc.add(v); _, a, b = defs[v]; deps(a, acc); deps(b, acc)
    fan = collections.Counter()
    for t in T - S:
        acc = set(); deps(t, acc)
        for v in acc: fan[v] += 1
        op, a, b = defs[t]; lastop[op] += 1
        for x in (a, b):
            kinds['target' if x in T else 'start' if x in S else 'helper'] += 1
        tt += any(x in T for x in (a, b))
    # «шина»: значения, от которых зависит >= половины целей
    nT = len(T - S)
    bus = sum(1 for v, c in fan.items() if c >= nT / 2)
    # длина «личной» цепочки цели = число значений только для неё
    priv = collections.Counter()
    for v, c in fan.items():
        if c == 1: priv[v] += 1
    return dict(t_from_t=tt / nT, kinds=kinds, lastop=lastop, bus=bus, fan10=sum(1 for v, c in fan.items() if c >= 10))
P = load_puzzles(); old = json.load(open(sys.argv[1])); ex = json.load(open('external/src/tkm2261_exposed.json'))
fam = {r[list(r)[0]]: r[list(r)[1]] for r in csv.DictReader(open('data/families_inferred.csv'))}
for F in ['mixed', 'shared', 'independent']:
    A = []; Bv = []
    for pid, prog in ex.items():
        if fam.get(pid) != F or len(prog) >= old[pid]['score']: continue
        A.append(analyse(P[pid], old[pid]['program'])); Bv.append(analyse(P[pid], prog))
    def m(L, f): return round(st.mean(f(x) for x in L), 2)
    print(F, len(A))
    print('  targets built from another target:', m(A, lambda x: x['t_from_t']), '->', m(Bv, lambda x: x['t_from_t']))
    print('  values feeding >= half the targets:', m(A, lambda x: x['bus']), '->', m(Bv, lambda x: x['bus']))
    print('  values feeding >= 10 targets      :', m(A, lambda x: x['fan10']), '->', m(Bv, lambda x: x['fan10']))
    for k in ['target', 'helper', 'start']:
        print(f'  operand kind {k:7s}:', m(A, lambda x: x['kinds'][k]), '->', m(Bv, lambda x: x['kinds'][k]))
    for o in '+-*/':
        print(f'  last op {o}:', m(A, lambda x: x['lastop'][o]), '->', m(Bv, lambda x: x['lastop'][o]))
