"""Exact recombination of many programs' arithmetic graphs with CP-SAT.

Input: for one puzzle, a set of recipes (op, a, b, v) harvested from many programs (our plateau-walk variants,
public banks). We add every cross recipe a op b = v with a, b, v all in the value set, then solve
    min sum x_v   s.t. targets built, each built value has one chosen recipe whose operands are built,
                       chosen recipes respect an acyclic order (integer levels).
The chosen graph is scheduled into <= 48 registers and replayed by the referee.
"""
import sys, os, json, time, collections
sys.path.insert(0, os.path.dirname(__file__))
from elf import *
from ortools.sat.python import cp_model

OPS = {'+': lambda a, b: a + b, '-': lambda a, b: a - b, '*': lambda a, b: a * b, '/': lambda a, b: a // b}

def program_recipes(p, prog):
    regs = list(p.starts) + [None] * (p.registers - len(p.starts)); out = set()
    for op, i, j, d in prog:
        a, b = regs[i], regs[j]; v = OPS[op](a, b); regs[d] = v
        out.add((op, a, b, v))
    return out

def cross_recipes(p, values):
    """all a op b = v with a, b, v in values (exact division only)."""
    vs = sorted(values); S = set(vs); out = set(); L = p.limit
    for i, a in enumerate(vs):
        for b in vs[i:]:
            s = a + b
            if s in S: out.add(('+', a, b, s))
            m = a * b
            if m <= L and m in S: out.add(('*', a, b, m))
            if b - a in S and b > a: out.add(('-', b, a, b - a))
            if b % a == 0 and b // a in S and b != a: out.add(('/', b, a, b // a))
    return out

def schedule(p, chosen, level):
    """chosen: value -> (op, a, b). Topological order by level, list-scheduled to keep live values <= regs.
    Returns a program or None."""
    starts = list(p.starts); sset = set(starts)
    users = collections.defaultdict(list)
    for v, (op, a, b) in chosen.items():
        users[a].append(v)
        if b != a: users[b].append(v)
    remaining = {v: len(users[v]) for v in list(chosen) + starts}
    indeg = {v: sum(1 for x in {chosen[v][1], chosen[v][2]} if x not in sset) for v in chosen}
    ready = [v for v in chosen if indeg[v] == 0]
    done = set(sset); order = []
    while ready:
        # prefer instructions that free registers, then lower level
        def key(v):
            op, a, b = chosen[v]
            fr = (remaining[a] == 1) + (b != a and remaining[b] == 1)
            return ((1 if users[v] else 0) - fr, level.get(v, 0))
        ready.sort(key=key); v = ready.pop(0); order.append(v); done.add(v)
        op, a, b = chosen[v]
        remaining[a] -= 1
        if b != a: remaining[b] -= 1
        for u in users[v]:
            indeg[u] -= 1 if True else 0
        for u in users[v]:
            if indeg[u] == 0 and u not in done and u not in ready:
                oa, ob = chosen[u][1], chosen[u][2]
                if (oa in done) and (ob in done): ready.append(u)
    if len(order) != len(chosen): return None
    # linear-scan register allocation
    last = {}
    for k, v in enumerate(order):
        op, a, b = chosen[v]; last[a] = k; last[b] = k
    reg_of = {}; occ = [None] * p.registers
    for k, s in enumerate(starts):
        if s not in reg_of: reg_of[s] = k; occ[k] = s
    for s in list(reg_of):
        if s not in last: occ[reg_of[s]] = None
    prog = []
    for k, v in enumerate(order):
        op, a, b = chosen[v]; ra, rb = reg_of[a], reg_of[b]
        if occ[ra] != a or occ[rb] != b: return None
        if last.get(a) == k: occ[ra] = None
        if last.get(b) == k: occ[rb] = None
        try: d = occ.index(None)
        except ValueError: return None
        prog.append([op, ra, rb, d]); reg_of[v] = d; occ[d] = v
        if last.get(v, -1) <= k: occ[d] = None
    return prog

def inverses(T, x, L):
    """(w, recipe-maker) with T = w op x or x op w."""
    out = []
    if T > x: out.append((T - x, lambda w: ('+', w, x, T)))
    if T + x <= L: out.append((T + x, lambda w: ('-', w, x, T)))
    if x > T: out.append((x - T, lambda w: ('-', x, w, T)))
    if x > 1 and T % x == 0: out.append((T // x, lambda w: ('*', w, x, T)))
    if x > 1 and T * x <= L: out.append((T * x, lambda w: ('/', w, x, T)))
    if x > T and x % T == 0: out.append((x // T, lambda w: ('/', x, w, T)))
    return out

def bridge_recipes(p, values, max_new=3000):
    """new intermediates w = a op b (a, b in values) that give some v in values as v = w op c (c in values)."""
    vs = sorted(values); S = set(vs); L = p.limit; s1 = {}
    for i, a in enumerate(vs):
        for b in vs[i:]:
            for w, r in ((a + b, ('+', a, b)), (a * b, ('*', a, b)), (b - a, ('-', b, a)), (b // a if b % a == 0 else 0, ('/', b, a))):
                if 1 <= w <= L and w not in S and w not in s1: s1[w] = r
    uses = collections.defaultdict(dict); sset = set(p.starts)
    for v in vs:
        if v in sset: continue
        for c in vs:
            for w, mk in inverses(v, c, L):
                if w in s1 and w != v and v not in uses[w]: uses[w][v] = mk(w)
    good = sorted((w for w in uses if len(uses[w]) >= 2), key=lambda w: -len(uses[w]))[:max_new]
    out = set()
    for w in good:
        out.add((*s1[w], w))
        for r in uses[w].values(): out.add(r)
    return out

def recombine(p, recipes, hint_prog=None, time_limit=20.0, cross=True, workers=1, bridge=True, order_prog=None):
    sset = set(p.starts); T = set(p.targets) - sset
    values = {v for (_, a, b, v) in recipes} | {a for (_, a, b, v) in recipes} | {b for (_, a, b, v) in recipes} | sset
    if cross: recipes = set(recipes) | cross_recipes(p, values)
    if bridge: recipes = set(recipes) | bridge_recipes(p, values)
    rec_by_v = collections.defaultdict(list)
    for r in recipes:
        op, a, b, v = r
        if v in sset or v == a or v == b: continue
        rec_by_v[v].append(r)
    V = [v for v in rec_by_v]
    if order_prog is not None:
        # fixed topological order: rank = position of v in order_prog; new values sit just before their first consumer
        rank = {v: -1.0 for v in sset}
        for k, (op, a, b, v) in enumerate(sorted(program_recipes(p, order_prog), key=lambda r: 0)):
            pass
        regs = list(p.starts) + [None] * (p.registers - len(p.starts))
        for k, (op, i, j, d) in enumerate(order_prog):
            a, b = regs[i], regs[j]; v = OPS[op](a, b); regs[d] = v; rank.setdefault(v, float(k))
        for v in V:
            if v in rank: continue
            cons = [rank[r[3]] for r in recipes if (r[1] == v or r[2] == v) and r[3] in rank]
            if cons: rank[v] = min(cons) - 0.5
        for v in list(rec_by_v):
            if v not in rank: del rec_by_v[v]; continue
            rec_by_v[v] = [r for r in rec_by_v[v] if rank.get(r[1], 1e18) < rank[v] and rank.get(r[2], 1e18) < rank[v]]
        V = [v for v in rec_by_v]
    m = cp_model.CpModel()
    x = {v: m.NewBoolVar("") for v in V}
    big = len(V) + 1
    lvl = {v: m.NewIntVar(1, big, "") for v in V}
    for t in T:
        if t not in x: return None
        m.Add(x[t] == 1)
    for v in V:
        ys = []
        for (op, a, b, _) in rec_by_v[v]:
            if (a not in sset and a not in x) or (b not in sset and b not in x): continue
            y = m.NewBoolVar(""); ys.append(((op, a, b), y))
            for o in {a, b}:
                if o in sset: continue
                m.AddImplication(y, x[o])
                if order_prog is None: m.Add(lvl[v] >= lvl[o] + 1).OnlyEnforceIf(y)
        if not ys: m.Add(x[v] == 0); continue
        m.Add(sum(y for _, y in ys) == x[v])
        rec_by_v[v] = ys
    m.Minimize(sum(x.values()))
    if hint_prog is not None:
        hr = {}
        for (op, a, b, v) in program_recipes(p, hint_prog): hr.setdefault(v, (op, a, b))
        # keep only values the targets need
        need, st = set(), list(T)
        while st:
            v = st.pop()
            if v in need or v in sset: continue
            need.add(v); op, a, b = hr[v]; st += [a, b]
        depth = {}
        def dep(v):
            if v in sset: return 0
            if v not in depth: op, a, b = hr[v]; depth[v] = 1 + max(dep(a), dep(b))
            return depth[v]
        for v in V:
            m.AddHint(x[v], 1 if v in need else 0)
            if v in need and order_prog is None: m.AddHint(lvl[v], dep(v))
            for rec, y in rec_by_v[v]:
                m.AddHint(y, 1 if (v in need and rec == hr[v]) else 0)
    s = cp_model.CpSolver(); s.parameters.max_time_in_seconds = time_limit; s.parameters.num_workers = workers
    st = s.Solve(m)
    if st not in (cp_model.OPTIMAL, cp_model.FEASIBLE): return None
    chosen, level = {}, {}
    for v in V:
        if s.Value(x[v]):
            for rec, y in rec_by_v[v]:
                if s.Value(y): chosen[v] = rec; level[v] = s.Value(lvl[v]); break
    prog = schedule(p, chosen, level)
    return prog, int(s.ObjectiveValue()), s.StatusName(st), len(V), sum(len(r) for r in rec_by_v.values())

def read_dump(path):
    out = collections.defaultdict(set)
    with open(path) as f:
        lines = f.read().split("\n"); k = 0
        while k < len(lines):
            ln = lines[k].split()
            if ln and ln[0] == "D":
                pid, n = ln[1], int(ln[2])
                for t in range(n):
                    op, a, b, v = lines[k + 1 + t].split(); out[pid].add((op, int(a), int(b), int(v)))
                k += n + 1
            else: k += 1
    return out

if __name__ == "__main__":
    import glob
    P = load_puzzles(); best = read_submission(sys.argv[1]); dumps = sys.argv[2]; tl = float(sys.argv[3]) if len(sys.argv) > 3 else 20
    extra_csvs = sys.argv[4:]
    D = read_dump(dumps); banks = [read_submission(f) for f in extra_csvs]
    gain = 0
    for pid, rec in D.items():
        p = P[pid]; rec = set(rec) | program_recipes(p, best[pid])
        for bk in banks:
            try: replay(p, bk[pid]); rec |= program_recipes(p, bk[pid])
            except Exception: pass
        t0 = time.time()
        r = recombine(p, rec, best[pid], tl, order_prog=best[pid] if os.environ.get('FIXORDER') else None)
        if r is None: print(pid, "no solution"); continue
        prog, obj, status, nv, nr = r
        ok = None
        if prog is not None:
            try: ok = replay(p, prog)
            except ValueError as e: ok = f"invalid {e}"
        print(f"{pid} best {len(best[pid])} cp-sat {obj} ({status}, |V|={nv}, recipes={nr}) scheduled {ok} {time.time() - t0:.1f}s", flush=True)
        if isinstance(ok, int) and ok < len(best[pid]):
            gain += len(best[pid]) - ok
            with open(dumps + ".results", "a") as f: f.write(json.dumps({"id": pid, "program": prog}) + "\n")
    print("total gain", gain)
