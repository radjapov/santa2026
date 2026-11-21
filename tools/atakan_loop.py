"""Фоновый цикл оптимизаторов Atakan Aldemir (multiplier frontier / interface closure,
Apache-2.0, из ноутбука "Joint Recipes for Large Puzzles") по длинным головоломкам.
Параметры взяты из schedule8.sh goodpjw2008. Всё проверяется нашим верификатором.
  python tools/atakan_loop.py --lo 181 --hi 9999 --batch 4
Остановка: touch work/STOP_ATK
"""
import argparse, os, random, subprocess, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from store import load_best, load_puzzles, offer  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..")
ap = argparse.ArgumentParser()
ap.add_argument("--lo", type=int, default=181)
ap.add_argument("--hi", type=int, default=9999)
ap.add_argument("--batch", type=int, default=4)
a = ap.parse_args()

CL = ["90 26 7 {salt} 4150", "92 32 4 {salt} 5250", "100 28 9 {salt} 4950"]
FR = ["8 4200 9 10 9 {salt} 1500", "14 8000 12 13 9 {salt} 3000", "8 4200 9 10 13 {salt} 1500"]
P = load_puzzles()
STOP = os.path.join(ROOT, "work", "STOP_ATK")


def case(pid, prog):
    s, t, vl = P[pid]
    t = sorted(set(t))
    return "\n".join([f"{pid} {vl} {len(s)} {len(t)} {len(prog)}", " ".join(map(str, s)), " ".join(map(str, t))]
                     + [f"{o} {x} {y} {d}" for o, x, y, d in prog])


def parse(text):
    lines, out, pos = text.splitlines(), {}, 0
    while pos < len(lines):
        h = lines[pos].split(); pos += 1
        if len(h) < 2 or h[0] not in P:
            continue
        n = int(h[1])
        out[h[0]] = [[x[0], int(x[1]), int(x[2]), int(x[3])] for x in (l.split() for l in lines[pos:pos + n])]
        pos += n
    return out


it = 0
while not os.path.exists(STOP):
    best = load_best()
    ids = [p for p in P if a.lo <= best[p]["score"] <= a.hi]
    random.shuffle(ids)
    for kind, args in (("elf_interface_closure", CL[it % 3]), ("elf_multiplier_frontier", FR[it % 3])):
        imp_tot = gain_tot = 0
        t0 = time.time()
        for i in range(0, len(ids), a.batch):
            if os.path.exists(STOP):
                break
            best = load_best()
            batch = ids[i:i + a.batch]
            argv = [os.path.join(ROOT, "bin", kind)] + args.format(salt=random.randrange(1, 2**31)).split()
            r = subprocess.run(argv, input="\n".join(case(p, best[p]["program"]) for p in batch) + "\n",
                               capture_output=True, text=True)
            imp, gain = offer(parse(r.stdout), P, kind)
            imp_tot += imp; gain_tot += gain
        tot = sum(v["score"] for v in load_best().values())
        print(f"[{time.strftime('%H:%M:%S')}] {kind} '{args}' {len(ids)} puzzles: improved {imp_tot}, "
              f"gain {gain_tot}, total {tot} ({time.time() - t0:.0f}s)", flush=True)
    it += 1
