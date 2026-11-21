"""Анатомия программ: для каждой инструкции — каким целям она нужна.
exclusive = нужна ровно одной цели, shared = нескольким, dead = ни одной."""
import csv, sys, os
from collections import Counter, defaultdict
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from store import load_puzzles, load_best

def anatomy(starts, targets, prog):
    reg = [("s", i) for i in range(len(starts))] + [None] * 48
    val = list(starts) + [None] * 48
    deps = []  # per instruction: (src node a, src node b)
    first_t = {}
    ts = set(targets) - set(starts)
    for k, (op, a, b, d) in enumerate(prog):
        x, y = val[a], val[b]
        v = {'+': x + y, '-': x - y, '*': x * y, '/': x // y}[op]
        deps.append((reg[a], reg[b]))
        reg[d] = ("i", k); val[d] = v
        if v in ts and v not in first_t:
            first_t[v] = k
    users = defaultdict(set)  # instruction -> set of targets needing it
    for t, k in first_t.items():
        st = [k]
        while st:
            q = st.pop()
            if t in users[q]:
                continue
            users[q].add(t)
            for n in deps[q]:
                if n and n[0] == "i":
                    st.append(n[1])
    c = Counter()
    for k in range(len(prog)):
        u = len(users[k])
        c["dead" if u == 0 else "excl" if u == 1 else "shared"] += 1
    return c, len(first_t)

if __name__ == "__main__":
    P = load_puzzles(); B = load_best()
    fam = {r['id']: r['fam'] for r in csv.DictReader(open('data/families_inferred.csv'))}
    agg = defaultdict(Counter)
    for pid, (s, t, vl) in P.items():
        c, nt = anatomy(s, t, B[pid]["program"])
        agg[fam[pid]].update(c); agg[fam[pid]]["targets"] += nt
    for f, c in agg.items():
        nt = c["targets"]
        print(f"{f:15s} targets {nt:5d}  excl/tgt {c['excl']/nt:5.2f}  shared/tgt {c['shared']/nt:5.2f}  dead {c['dead']}")
