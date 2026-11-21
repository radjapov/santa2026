import sys, time, os
from collections import Counter
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from store import load_puzzles, load_best
from proto.pool_probe import Pool, NearMiss
P = load_puzzles(); B = load_best()
pid = sys.argv[1]; n = int(sys.argv[2])
s, t, vl = P[pid]; prog = B[pid]["program"]
reg = list(s) + [None] * 48; use = Counter(); vals = set(s)
for op, a, b, d in prog:
    x, y = reg[a], reg[b]; use[x] += 1; use[y] += 1
    v = {'+': x + y, '-': x - y, '*': x * y, '/': x // y}[op]; reg[d] = v; vals.add(v)
small = sorted((v for v in vals if v < 2**30), key=lambda v: -use[v])[:n]
pool = Pool(small, vl); nm = NearMiss(pool)
t0 = time.time(); costs = []
for x in t:
    c = pool.cost(x)
    if c >= 6: c = nm.cost(x)
    costs.append(c)
print(f"{pid} best {len(prog)}  N={n}  costs {dict(sorted(Counter(costs).items()))}  sum={sum(costs)}  "
      f"total~{n+sum(costs)}  ({time.time()-t0:.0f}s)", flush=True)
