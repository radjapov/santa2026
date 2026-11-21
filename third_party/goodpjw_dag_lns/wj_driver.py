"""Run Wang Jakie's public arithmetic graph-search strategies (third_party/wangjakie, Apache-2.0 notebook code)
in parallel on our current best programs; seeded variants also receive near-length variants as crossover seeds.
Every returned program is replayed by our referee; strictly shorter ones are merged into work/best.csv.
usage: python src/wj_driver.py --strategy anneal --rounds 95 --only work/focus_ids.txt --jobs 19 [--seeds-glob "outs*/*/*.csv"]
"""
import argparse, os, sys, subprocess, glob, time, json, random
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(__file__))
from elf import *

ap = argparse.ArgumentParser()
ap.add_argument("--strategy", required=True); ap.add_argument("--rounds", type=int, default=50)
ap.add_argument("--only", default=""); ap.add_argument("--jobs", type=int, default=19)
ap.add_argument("--batch", type=int, default=4); ap.add_argument("--seed", type=int, default=0)
ap.add_argument("--seeds-glob", default=""); ap.add_argument("--max-seeds", type=int, default=4)
ap.add_argument("--tag", default="wj")
ap.add_argument("--start-variant", action="store_true", help="start from a different historical program (seeds incl. the best)")
ap.add_argument("--archive", default="", help="jsonl plateau archive: read as extra seeds, append near-length distinct results")
ap.add_argument("--slack", type=int, default=3, help="seed programs may be at most this much longer than the best")
a = ap.parse_args()
P = load_puzzles(); BEST = os.path.join(ROOT, "work/best.csv"); HIST = os.path.join(ROOT, "work/history.csv")
EXE = os.path.join(ROOT, "third_party/wangjakie", a.strategy)
S = read_submission(BEST)
ids = [l.strip() for l in open(a.only) if l.strip()] if a.only else list(P)
seed0 = a.seed or random.randrange(1, 2**31)

extra = {}
if a.seeds_glob or a.archive:
    seen = {k: {json.dumps(S[k])} for k in ids}; cand = {k: [] for k in ids}; idset = set(ids)
    def offer(k, prog):
        if prog is None or len(prog) > len(S[k]) + a.slack: return
        key = json.dumps(prog)
        if key in seen[k]: return
        seen[k].add(key)
        try: replay(P[k], prog)
        except Exception: return
        cand[k].append(prog)
    for f in (sorted(glob.glob(a.seeds_glob)) if a.seeds_glob else []):
        try: B = read_submission(f)
        except Exception: continue
        for k in ids: offer(k, B.get(k))
    if a.archive and os.path.exists(a.archive):
        for ln in open(a.archive):
            try: r = json.loads(ln)
            except Exception: continue
            if r.get("k") in idset: offer(r["k"], r["p"])
    rnd = random.Random(seed0)
    for k in ids:
        if cand[k]: rnd.shuffle(cand[k]); extra[k] = cand[k][:a.max_seeds]
known = {k: {json.dumps(t) for t in [S[k]] + extra.get(k, [])} for k in ids}

def job_text(k):
    p = P[k]; tapes = [S[k]] + extra.get(k, [])
    if a.start_variant and extra.get(k):
        alt = random.choice(extra[k]); tapes = [alt, S[k]] + [t for t in extra[k] if t is not alt]
    rows = [f"{p.limit} {p.registers} {a.rounds} 0 {(seed0 + int(k.split('_')[1])) % 2**32}",
            " ".join(map(str, (len(p.starts), *p.starts))),
            " ".join(map(str, (len(set(p.targets)), *sorted(set(p.targets))))), str(len(tapes))]
    for t in tapes:
        rows.append(str(len(t))); rows += [f"{op} {x} {y} {d}" for op, x, y, d in t]
    return "\n".join(rows)

def run(batch):
    text = "\n".join([str(len(batch))] + [job_text(k) for k in batch]) + "\n"
    r = subprocess.run([EXE], input=text, capture_output=True, text=True)
    out, arch = {}, []
    for k, line in zip(batch, r.stdout.splitlines()):
        tok = line.split()
        if not tok: continue
        prog = [[tok[i], int(tok[i + 1]), int(tok[i + 2]), int(tok[i + 3])] for i in range(1, len(tok), 4)]
        try: n = replay(P[k], prog)
        except ValueError: continue
        if n < len(S[k]): out[k] = prog
        elif n <= len(S[k]) + 2 and json.dumps(prog) not in known[k]: arch.append((k, prog))
    return out, arch

ids.sort(key=lambda k: -len(S[k]))
batches = [ids[i::max(1, len(ids) // a.batch)] for i in range(max(1, len(ids) // a.batch))]
t0 = time.time(); found = {}; archived = []
with ThreadPoolExecutor(a.jobs) as pool:
    for res, arch in pool.map(run, batches): found.update(res); archived += arch
if a.archive and (archived or found):
    with open(a.archive, "a") as f:
        for k, prog in archived + list(found.items()):
            f.write(json.dumps({"k": k, "p": prog}, separators=(",", ":")) + "\n")
cur = read_submission(BEST); gain = 0
for k, prog in found.items():
    if len(prog) < len(cur[k]): gain += len(cur[k]) - len(prog); cur[k] = prog
tmp = BEST + ".tmp"; write_submission(tmp, P, cur); os.replace(tmp, BEST)
tot = sum(len(cur[k]) for k in P)
open(HIST, "a").write(f"{time.time():.0f},{a.tag}_{a.strategy},{tot}\n")
print(f"[{time.strftime('%H:%M:%S')}] {a.strategy} rounds={a.rounds} seeds={'on' if extra else 'off'} puzzles={len(ids)} "
      f"improved={len(found)} gain={gain} archived={len(archived)} total={tot} ({time.time() - t0:.0f}s)", flush=True)
