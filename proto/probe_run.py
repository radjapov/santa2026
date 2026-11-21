import sys, time, os
from collections import Counter
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from store import load_puzzles, load_best
from proto.pool_probe import Pool
P = load_puzzles(); B = load_best()
pid = sys.argv[1]; sizes = [int(x) for x in sys.argv[2].split(",")]; bits = int(sys.argv[3]) if len(sys.argv) > 3 else 30
s, t, vl = P[pid]; prog = B[pid]["program"]
reg = list(s) + [None] * 48; use = Counter(); vals = set(s)
for op, a, b, d in prog:
    x, y = reg[a], reg[b]; use[x] += 1; use[y] += 1
    v = {'+': x + y, '-': x - y, '*': x * y, '/': x // y}[op]; reg[d] = v; vals.add(v)
small = [v for v in vals if v < 2**bits]
small.sort(key=lambda v: -use[v])
print(pid, "best", len(prog), "targets", len(t), "small values in program", len(small))
for n in sizes:
    V = small[:n] if n else small
    t0 = time.time(); pool = Pool(V, vl)
    costs = [pool.cost(x) for x in t]
    c = Counter(costs)
    print(f"N={len(V):3d} |S1|={len(pool.S1):6d} |S2|={len(pool.S2):8d}  costs {dict(sorted(c.items()))}  "
          f"sum={sum(costs)}  => total ~ {len(V)}+{sum(costs)} = {len(V)+sum(costs)}  ({time.time()-t0:.0f}s)", flush=True)
