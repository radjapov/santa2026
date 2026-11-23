"""Сравнение структуры программ: старый публичный фронт vs утёкшие программы tkm2261 (209 пар)."""
import json, csv, collections, statistics as st, sys
sys.path.insert(0, '.')
from store import load_best, load_puzzles
OPS = {'+': lambda a, b: a + b, '-': lambda a, b: a - b, '*': lambda a, b: a * b, '/': lambda a, b: a // b}

def dag(p, prog):
    s, t, L = p
    regs = list(s) + [None] * (48 - len(s)); node = {}; defs = {}  # value -> (op,a,b)
    order = []; first_t = None; T = set(t)
    for k, (op, i, j, d) in enumerate(prog):
        a, b = regs[i], regs[j]; v = OPS[op](a, b); regs[d] = v
        if v not in defs and v not in s: defs[v] = (op, a, b); order.append(v)
        if first_t is None and v in T: first_t = k
    # маски целей: какие цели зависят от значения
    need = {}
    def deps(v, acc):
        if v in s or v in acc: return
        acc.add(v); op, a, b = defs[v]; deps(a, acc); deps(b, acc)
    users = collections.Counter()
    for tv in T - set(s):
        acc = set(); deps(tv, acc)
        for v in acc: users[v] += 1
    shared = sum(1 for v in order if users[v] >= 2)
    private = sum(1 for v in order if users[v] == 1)
    depth = {}
    for v in order:
        op, a, b = defs[v]; depth[v] = 1 + max(depth.get(a, 0), depth.get(b, 0))
    # пик регистров: живые значения
    last = {}
    for k, v in enumerate(order):
        op, a, b = defs[v]; last[a] = k; last[b] = k
    live = 0; peak = 0; alive = set(x for x in s if x in last)
    for k, v in enumerate(order):
        peak = max(peak, len(alive) + 1)
        op, a, b = defs[v]
        for x in (a, b):
            if last.get(x) == k: alive.discard(x)
        if v in last and last[v] > k: alive.add(v)
    ops = collections.Counter(defs[v][0] for v in order)
    bits = [v.bit_length() for v in order]
    return dict(n=len(prog), vals=set(order), shared=shared, private=private, first=first_t,
                depth=max(depth.values()), peak=peak, ops=ops, bits=bits, small=sum(1 for b in bits if b <= 20))

P = load_puzzles(); B = load_best()
old = json.load(open(sys.argv[1])); ex = json.load(open('external/src/tkm2261_exposed.json'))
fam = {r[list(r)[0]]: r[list(r)[1]] for r in csv.DictReader(open('data/families_inferred.csv'))}
rows = []
for pid, prog in ex.items():
    if len(prog) >= old[pid]['score']: continue
    a = dag(P[pid], old[pid]['program']); b = dag(P[pid], prog)
    jac = len(a['vals'] & b['vals']) / len(a['vals'] | b['vals'])
    rows.append((pid, fam.get(pid), a, b, jac))
print('pairs', len(rows))
def avg(f): return st.mean(f(r) for r in rows)
print(f"value overlap (Jaccard): mean {avg(lambda r: r[4]):.2f}, median {st.median(r[4] for r in rows):.2f}")
print(f"share of new values not in old: {avg(lambda r: len(r[3]['vals']-r[2]['vals'])/len(r[3]['vals'])):.2f}")
for key in ['n', 'shared', 'private', 'first', 'depth', 'peak', 'small']:
    print(f"{key:8s} old {avg(lambda r: r[2][key]):7.2f}  tkm {avg(lambda r: r[3][key]):7.2f}")
for op in '+-*/':
    print(f"op {op}    old {avg(lambda r: r[2]['ops'][op]):7.2f}  tkm {avg(lambda r: r[3]['ops'][op]):7.2f}")
print('mean bits old', round(avg(lambda r: st.mean(r[2]['bits'])), 2), 'tkm', round(avg(lambda r: st.mean(r[3]['bits'])), 2))
# по семействам
for f in sorted(set(r[1] for r in rows)):
    R = [r for r in rows if r[1] == f]
    print(f, len(R), 'jac', round(st.mean(r[4] for r in R), 2), 'shared old->tkm', round(st.mean(r[2]['shared'] for r in R), 1), round(st.mean(r[3]['shared'] for r in R), 1),
          'private', round(st.mean(r[2]['private'] for r in R), 1), round(st.mean(r[3]['private'] for r in R), 1))
# крупнейшие
for r in sorted(rows, key=lambda r: r[3]['n'] - r[2]['n'])[:6]:
    print(r[0], r[1], r[2]['n'], '->', r[3]['n'], 'jac', round(r[4], 2), 'shared', r[2]['shared'], '->', r[3]['shared'], 'private', r[2]['private'], '->', r[3]['private'], 'peak', r[2]['peak'], r[3]['peak'])
