"""Run Atakan Aldemir's public optimizers (third_party/atakan: multiplier frontier / interface closure, from his notebook
"Santa 2026 | Joint Recipes for Large Puzzles", Apache-2.0) on our current best programs in parallel batches.
Every returned program is replayed by our referee; strictly shorter ones are merged into work/best.csv,
equal-length distinct ones are appended to the plateau archive.
usage: python src/atakan_driver.py --bin frontier --args "8 4200 9 10 9 {salt} 1500" --only work/focus_ids.txt
"""
import argparse, os, sys, subprocess, time, json, random
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(__file__))
from elf import *

ap = argparse.ArgumentParser()
ap.add_argument("--bin", required=True); ap.add_argument("--args", required=True)
ap.add_argument("--only", default=""); ap.add_argument("--jobs", type=int, default=19)
ap.add_argument("--batch", type=int, default=4); ap.add_argument("--tag", default="atakan")
ap.add_argument("--archive", default=""); ap.add_argument("--from-archive", action="store_true",
                help="start each puzzle from a random equal-length archive variant instead of the best")
a = ap.parse_args()
P = load_puzzles(); BEST = os.path.join(ROOT, "work/best.csv"); HIST = os.path.join(ROOT, "work/history.csv")
EXE = os.path.join(ROOT, "third_party/atakan", a.bin)
S = read_submission(BEST)
ids = [l.strip() for l in open(a.only) if l.strip()] if a.only else list(P)
salt0 = random.randrange(1, 2**31)

start = {k: S[k] for k in ids}
if a.from_archive and a.archive and os.path.exists(a.archive):
    alts = {}
    for ln in open(a.archive):
        try: r = json.loads(ln)
        except Exception: continue
        k = r.get("k")
        if k in start and len(r["p"]) == len(S[k]) and r["p"] != S[k]: alts.setdefault(k, []).append(r["p"])
    for k, v in alts.items(): start[k] = random.choice(v)

def case(k):
    p = P[k]; t = sorted(set(p.targets)); prog = start[k]
    return "\n".join([f"{k} {p.limit} {len(p.starts)} {len(t)} {len(prog)}", " ".join(map(str, p.starts)),
                      " ".join(map(str, t))] + [f"{op} {x} {y} {d}" for op, x, y, d in prog])

def run(job):
    i, batch = job
    argv = [EXE] + a.args.format(salt=salt0 + i).split()
    r = subprocess.run(argv, input="\n".join(case(k) for k in batch) + "\n", capture_output=True, text=True)
    lines = r.stdout.splitlines(); out, arch, pos = {}, [], 0
    while pos < len(lines):
        head = lines[pos].split(); pos += 1
        if len(head) < 2: continue
        k, n = head[0], int(head[1])
        prog = [[x[0], int(x[1]), int(x[2]), int(x[3])] for x in (l.split() for l in lines[pos:pos + n])]; pos += n
        if k not in S: continue
        try: m = replay(P[k], prog)
        except ValueError: continue
        if m < len(S[k]): out[k] = prog
        elif m == len(S[k]) and prog != S[k] and prog != start[k]: arch.append((k, prog))
    return out, arch, r.returncode, r.stderr[-200:]

ids.sort(key=lambda k: -len(S[k]))
nb = max(1, len(ids) // a.batch)
jobs = list(enumerate(ids[i::nb] for i in range(nb)))
t0 = time.time(); found, archived, errs = {}, [], 0
with ThreadPoolExecutor(a.jobs) as pool:
    for out, arch, rc, err in pool.map(run, jobs):
        found.update(out); archived += arch; errs += rc != 0
cur = read_submission(BEST); gain = 0
for k, prog in found.items():
    if len(prog) < len(cur[k]): gain += len(cur[k]) - len(prog); cur[k] = prog
tmp = BEST + ".tmp"; write_submission(tmp, P, cur); os.replace(tmp, BEST)
if a.archive and (archived or found):
    with open(a.archive, "a") as f:
        for k, prog in archived + list(found.items()): f.write(json.dumps({"k": k, "p": prog}, separators=(",", ":")) + "\n")
tot = sum(len(cur[k]) for k in P)
open(HIST, "a").write(f"{time.time():.0f},{a.tag}_{a.bin},{tot}\n")
print(f"[{time.strftime('%H:%M:%S')}] atakan {a.bin} '{a.args}' puzzles={len(ids)} improved={len(found)} gain={gain} "
      f"archived={len(archived)} errors={errs} total={tot} ({time.time() - t0:.0f}s)", flush=True)
