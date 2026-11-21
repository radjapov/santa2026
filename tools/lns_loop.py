"""Фоновый трек: бесконечный цикл elfopt (value-DAG LNS, код goodpjw2008, Apache-2.0)
по головоломкам заданного диапазона длин. Каждая программа проверяется нашим
верификатором и попадает в хранилище только если строго короче.

  python tools/lns_loop.py --jobs 3 --time 15 --lo 60 --hi 180
Остановка: touch work/STOP_LNS
"""
import argparse, os, random, subprocess, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from store import load_best, load_puzzles, offer  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..")
ap = argparse.ArgumentParser()
ap.add_argument("--jobs", type=int, default=3)
ap.add_argument("--time", type=float, default=15)
ap.add_argument("--lo", type=int, default=60)
ap.add_argument("--hi", type=int, default=180)
ap.add_argument("--bin", default=os.path.join(ROOT, "bin/elfopt"))
ap.add_argument("--extra", default="--kicks 1000 --kslack 2")
ap.add_argument("--rounds", type=int, default=10**9)
a = ap.parse_args()

P = load_puzzles()
WORK = os.path.join(ROOT, "work")
os.makedirs(WORK, exist_ok=True)
STOP = os.path.join(WORK, "STOP_LNS")


def encode(pid, prog):
    s, t, vl = P[pid]
    head = ["P", pid, str(len(s)), *map(str, s), str(len(t)), *map(str, t), str(vl), "48", str(len(prog))]
    return " ".join(head) + "\n" + "".join(f"{o} {i} {j} {d}\n" for o, i, j, d in prog)


def parse(text):
    res, lines, k = {}, text.split("\n"), 0
    while k < len(lines):
        f = lines[k].split()
        if f and f[0] == "R":
            n = int(f[3])
            res[f[1]] = [[o, int(i), int(j), int(d)] for o, i, j, d in (lines[k + 1 + q].split() for q in range(n))]
            k += n + 1
        else:
            k += 1
    return res


for rnd in range(a.rounds):
    if os.path.exists(STOP):
        print("stop file found", flush=True)
        break
    best = load_best()
    ids = [p for p in P if a.lo <= best[p]["score"] <= a.hi]
    random.shuffle(ids)
    d = os.path.join(WORK, f"lns_{int(time.time())}")
    os.makedirs(d)
    procs = []
    for i in range(a.jobs):
        sh = ids[i::a.jobs]
        inp = os.path.join(d, f"in_{i}.txt")
        with open(inp, "w") as f:
            f.write("".join(encode(p, best[p]["program"]) for p in sh))
        cmd = [a.bin, "--time", str(a.time), "--seed", str(random.randrange(1, 10**9)), *a.extra.split()]
        procs.append(subprocess.Popen(cmd, stdin=open(inp), stdout=open(os.path.join(d, f"out_{i}.txt"), "w"),
                                      stderr=subprocess.DEVNULL))
    for pr in procs:
        pr.wait()
    cands = {}
    for i in range(a.jobs):
        cands.update(parse(open(os.path.join(d, f"out_{i}.txt")).read()))
    imp, gain = offer(cands, P, "elfopt")
    tot = sum(v["score"] for v in load_best().values())
    print(f"[{time.strftime('%H:%M:%S')}] round {rnd}: {len(ids)} puzzles, improved {imp}, gain {gain}, total {tot}",
          flush=True)
    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    os.rmdir(d)
