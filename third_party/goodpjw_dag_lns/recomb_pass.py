"""One recombination pass: LNS with recipe harvesting, then free-order CP-SAT recombination per puzzle.
usage: python src/recomb_pass.py --only work/focus_ids.txt --time 20 --cp 20 --jobs 19
"""
import argparse, os, sys, subprocess, glob, time, random, json
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(__file__))
from pfmt import *
import recombine as RC

ap = argparse.ArgumentParser()
ap.add_argument("--only", default=""); ap.add_argument("--time", type=float, default=20)
ap.add_argument("--cp", type=float, default=20); ap.add_argument("--jobs", type=int, default=19)
ap.add_argument("--bin", default=os.path.join(ROOT, "bin/elfopt6"))
ap.add_argument("--extra", default="")
ap.add_argument("--tag", default="recomb")
ap.add_argument("--variants", type=int, default=0, help="also start LNS from up to N distinct bank programs (<= best+1) per puzzle")
ap.add_argument("--banks", default="", help="glob of id,program CSVs whose programs (<= best+3) join the recipe pool")
a = ap.parse_args()
P = load_puzzles(); BEST = os.path.join(ROOT, "work/best.csv"); HIST = os.path.join(ROOT, "work/history.csv")
S = read_submission(BEST)
ids = [l.strip() for l in open(a.only)] if a.only else list(P)
ids.sort(key=lambda k: -len(S[k]))
d = os.path.join(ROOT, "work", f"rc_{int(time.time())}"); os.makedirs(d)
blocks = [(k, S[k]) for k in ids]
if a.variants and a.banks:
    import json as _json
    var = {}
    for f in sorted(glob.glob(a.banks)):
        try: B = read_submission(f)
        except Exception: continue
        for pid in ids:
            prog = B.get(pid)
            if prog is None or len(prog) > len(S[pid]) + 1: continue
            key = _json.dumps(prog)
            if key == _json.dumps(S[pid]) or key in var.setdefault(pid, {}): continue
            if len(var[pid]) >= a.variants: continue
            try: replay(P[pid], prog)
            except Exception: continue
            var[pid][key] = prog
    for pid in ids:
        for prog in var.get(pid, {}).values(): blocks.append((pid, prog))
    print("LNS start blocks:", len(blocks), flush=True)
blocks.sort(key=lambda kb: -len(kb[1]))
procs = []
for i in range(a.jobs):
    sh = blocks[i::a.jobs]
    open(f"{d}/in_{i:02d}.txt", "w").write("".join(encode(P[k], prog) for k, prog in sh))
    cmd = (f"{a.bin} --time {a.time} --seed {random.randrange(1, 10**9)} --kicks 1000 --kslack 2 {a.extra} --dump {d}/dump_{i:02d}.txt "
           f"< {d}/in_{i:02d}.txt > {d}/out_{i:02d}.txt 2> {d}/log_{i:02d}.txt")
    procs.append(subprocess.Popen(cmd, shell=True))
for p in procs: p.wait()
# LNS gains
for f in glob.glob(f"{d}/out_*.txt"):
    for pid, prog in parse_results(open(f).read()).items():
        try: n = replay(P[pid], prog)
        except ValueError: continue
        if n < len(S[pid]): S[pid] = prog
lns_total = sum(len(S[k]) for k in P)
dumps = {}
for f in glob.glob(f"{d}/dump_*.txt"): dumps.update(RC.read_dump(f))
bank_rec = {}
if a.banks:
    import json as _json
    seen = {}
    for f in sorted(glob.glob(a.banks)):
        try: B = read_submission(f)
        except Exception: continue
        for pid in ids:
            prog = B.get(pid)
            if prog is None or len(prog) > len(S[pid]) + 3: continue
            key = _json.dumps(prog)
            if key in seen.setdefault(pid, set()): continue
            seen[pid].add(key)
            try: replay(P[pid], prog)
            except Exception: continue
            bank_rec.setdefault(pid, set()).update(RC.program_recipes(P[pid], prog))
    print("bank recipes for", len(bank_rec), "puzzles", flush=True)

def work(pid):
    p = P[pid]; rec = set(dumps.get(pid, set())) | RC.program_recipes(p, S[pid]) | bank_rec.get(pid, set())
    try:
        r = RC.recombine(p, rec, S[pid], a.cp, bridge=False)
    except Exception as e:
        return pid, None
    if r is None or r[0] is None: return pid, None
    prog = r[0]
    try: n = replay(p, prog)
    except ValueError: return pid, None
    return pid, prog if n < len(S[pid]) else None

with Pool(a.jobs) as pool:
    res = pool.map(work, ids, chunksize=1)
gain = 0
for pid, prog in res:
    if prog is not None: gain += len(S[pid]) - len(prog); S[pid] = prog
cur = read_submission(BEST)
for k in P:
    if len(cur[k]) < len(S[k]): S[k] = cur[k]
tmp = BEST + ".tmp"; write_submission(tmp, P, S); os.replace(tmp, BEST)
tot = sum(len(S[k]) for k in P)
open(HIST, "a").write(f"{time.time():.0f},{a.tag},{tot}\n")
print(f"[{time.strftime('%H:%M:%S')}] recomb pass: LNS stage -> {lns_total}, CP-SAT gain {gain}, final {tot}", flush=True)
