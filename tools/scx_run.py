"""Прогон bin/scx (scaffold exchange) или любого движка в формате elfopt на выбранных головоломках.

  python3 tools/scx_run.py --ids scored_0766,scored_0101 --time 120 --jobs 4 [--bin bin/scx] [--extra "--beam 16"]
  python3 tools/scx_run.py --ids-file work/ids.txt ... [--dry]   (--dry: не предлагать в хранилище)
Каждая головоломка — отдельный процесс; улучшения проходят через store.offer (верификатор, строго короче).
"""
import argparse, os, random, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from store import load_best, load_puzzles, offer  # noqa: E402


def encode(pid, s, t, L, prog):
    head = ["P", pid, str(len(s)), *map(str, s), str(len(t)), *map(str, t), str(L), "48", str(len(prog))]
    return " ".join(head) + "\n" + "".join(f"{op} {a} {b} {d}\n" for op, a, b, d in prog)


def parse(text):
    res, lines, k = {}, text.split("\n"), 0
    while k < len(lines):
        ln = lines[k].split()
        if ln and ln[0] == "R":
            n = int(ln[3])
            res[ln[1]] = [[o, int(a), int(b), int(d)] for o, a, b, d in (lines[k + 1 + i].split() for i in range(n))]
            k += n + 1
        else:
            k += 1
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ids", default="")
    ap.add_argument("--ids-file", default="")
    ap.add_argument("--time", type=float, default=60)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--bin", default=os.path.join(ROOT, "bin/scx"))
    ap.add_argument("--extra", default="")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--tag", default="scx")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    ids = [x for x in a.ids.split(",") if x]
    if a.ids_file:
        ids += [l.strip() for l in open(a.ids_file) if l.strip()]
    P, B = load_puzzles(), load_best()
    seed0 = a.seed or random.randrange(1, 10**9)

    def one(i, pid):
        s, t, L = P[pid]
        inp = encode(pid, s, t, L, B[pid]["program"])
        cmd = [a.bin, "--time", str(a.time), "--seed", str(seed0 + i), *a.extra.split()]
        r = subprocess.run(cmd, input=inp, capture_output=True, text=True)
        return pid, parse(r.stdout).get(pid), r.stderr.strip()

    t0 = time.time()
    with ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(lambda x: one(*x), enumerate(ids)))
    cands = {}
    for pid, prog, log in res:
        print(log, flush=True)
        if prog:
            cands[pid] = prog
    if cands and not a.dry:
        imp, gain = offer(cands, P, a.tag)
        print(f"offer: improved {imp}, gained {gain}")
    print(f"done {len(ids)} puzzles, {len(cands)} candidates, {time.time() - t0:.0f}s", flush=True)


if __name__ == "__main__":
    main()
