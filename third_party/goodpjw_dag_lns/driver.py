"""Production loop: repeatedly optimise the current best submission with the native engine.

Each round: shard work/best.csv -> run N engine processes (fresh seeds) -> validate every returned
program with the exact referee -> keep strictly shorter ones -> rewrite work/best.csv and append the
total to work/history.csv.
usage: python src/driver.py --rounds 10 --time 60 --jobs 20 [--bin bin/elfopt3] [--extra "--helpers 1"]
       [--only ids.txt]  (restrict to a puzzle subset)
"""
import argparse, os, sys, subprocess, time, glob, shutil, random
sys.path.insert(0, os.path.dirname(__file__))
from pfmt import *

ap = argparse.ArgumentParser()
ap.add_argument("--rounds", type=int, default=1)
ap.add_argument("--time", type=float, default=60)
ap.add_argument("--jobs", type=int, default=20)
ap.add_argument("--bin", default=os.path.join(ROOT, "bin/elfopt3"))
ap.add_argument("--extra", default="")
ap.add_argument("--only", default="")
ap.add_argument("--tag", default="r")
a = ap.parse_args()

P = load_puzzles()
BEST = os.path.join(ROOT, "work/best.csv"); HIST = os.path.join(ROOT, "work/history.csv")
T0 = time.time()

def total(S): return sum(len(S[k]) for k in P)

for rnd in range(a.rounds):
    S = read_submission(BEST)
    ids = list(P)
    if a.only: ids = [l.strip() for l in open(a.only) if l.strip()]
    # balance shards by program length (longer puzzles evaluate slower)
    ids.sort(key=lambda k: -len(S[k]))
    d = os.path.join(ROOT, "work", f"{a.tag}_{int(time.time())}")
    os.makedirs(d)
    shards = [[] for _ in range(a.jobs)]
    for i, pid in enumerate(ids): shards[i % a.jobs].append(encode(P[pid], S[pid]))
    procs = []
    for i, sh in enumerate(shards):
        open(f"{d}/in_{i:02d}.txt", "w").write("".join(sh))
        seed = random.randrange(1, 10**9)
        cmd = f"{a.bin} --time {a.time} --seed {seed} {a.extra} < {d}/in_{i:02d}.txt > {d}/out_{i:02d}.txt 2> {d}/log_{i:02d}.txt"
        procs.append(subprocess.Popen(cmd, shell=True))
    for p in procs: p.wait()
    before = total(S); gained = 0
    for f in glob.glob(f"{d}/out_*.txt"):
        for pid, prog in parse_results(open(f).read()).items():
            try: n = replay(P[pid], prog)
            except ValueError as e: print("INVALID", pid, e, flush=True); continue
            if n < len(S[pid]): S[pid] = prog; gained += 1
    # re-read best in case another process improved it meanwhile, merge per puzzle
    cur = read_submission(BEST)
    for k in P:
        if len(cur[k]) < len(S[k]): S[k] = cur[k]
    tmp = BEST + ".tmp"; write_submission(tmp, P, S); os.replace(tmp, BEST)
    tot = total(S)
    with open(HIST, "a") as h: h.write(f"{time.time():.0f},{a.tag},{tot}\n")
    print(f"[{time.strftime('%H:%M:%S')}] round {rnd} {a.tag}: {before} -> {tot} ({gained} puzzles improved) {d}", flush=True)
