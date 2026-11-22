"""Эксперимент №1: CP-SAT-перекомбинация с донорами разнообразия (без LNS).
Пул рецептов = рецепты текущей программы + рецепты «банка» — других программ той же головоломки
(git-история best/best.json; дополнительные CSV через --csv). Любой выигрыш — заслуга банка.

  python3 tools/bank_recomb.py --lo 60 --hi 180 --n 60 --slack 30 --cp 30 --jobs 4 [--dry] [--nobank]
"""
import argparse, glob, json, os, random, subprocess, sys, time
from multiprocessing import get_context

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "third_party/goodpjw_dag_lns"))
from store import load_best, load_puzzles as store_puzzles, offer  # noqa: E402
import elf  # noqa: E402
import recombine as RC  # noqa: E402

CACHE = os.path.join(ROOT, "work", "bank_git.json")


G = {}


def work(job):
    P, cur = G['P'], G['cur']
    pid, dn = job
    p = P[pid]; t0 = time.time()
    rec = RC.program_recipes(p, cur[pid])
    base = len(rec)
    for prog in dn:
        rec |= RC.program_recipes(p, prog)
    try:
        r = RC.recombine(p, rec, cur[pid], G['cp'], bridge=False)
    except Exception as e:
        return pid, None, len(dn), base, len(rec), f"err {e}", time.time() - t0
    prog = r[0] if r else None
    n = None
    if prog is not None:
        try:
            n = elf.replay(p, prog)
        except ValueError:
            prog = None
    ok = prog if (prog is not None and n < len(cur[pid])) else None
    return pid, ok, len(dn), base, len(rec), n, time.time() - t0


def git_bank():
    """pid -> список различных программ из всех версий best.json в git (кэш work/bank_git.json)."""
    commits = subprocess.run(["git", "-C", ROOT, "log", "--all", "--format=%H", "--", "best/best.json"],
                             capture_output=True, text=True).stdout.split()
    if os.path.exists(CACHE):
        c = json.load(open(CACHE))
        if c.get("commits") == len(commits):
            return c["bank"]
    bank = {}
    for h in commits:
        r = subprocess.run(["git", "-C", ROOT, "show", f"{h}:best/best.json"], capture_output=True, text=True)
        if r.returncode:
            continue
        try:
            d = json.loads(r.stdout)
        except ValueError:
            continue
        for pid, v in d.items():
            key = json.dumps(v["program"])
            bank.setdefault(pid, {})[key] = 1
    bank = {pid: [json.loads(k) for k in ks] for pid, ks in bank.items()}
    json.dump({"commits": len(commits), "bank": bank}, open(CACHE, "w"))
    return bank


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lo", type=int, default=60); ap.add_argument("--hi", type=int, default=180)
    ap.add_argument("--n", type=int, default=60); ap.add_argument("--ids-file", default="")
    ap.add_argument("--slack", type=int, default=30, help="брать программы банка длиной <= текущая + slack")
    ap.add_argument("--maxprog", type=int, default=12, help="не больше N программ банка на головоломку (самые короткие)")
    ap.add_argument("--cp", type=float, default=30); ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--csv", default="", help="glob доп. CSV id,program в банк")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--dry", action="store_true"); ap.add_argument("--nobank", action="store_true")
    a = ap.parse_args()
    P = elf.load_puzzles(os.path.join(ROOT, "data", "puzzles.csv"))
    B = load_best()
    cur = {k: v["program"] for k, v in B.items()}
    bank = {} if a.nobank else git_bank()
    if a.csv and not a.nobank:
        for f in sorted(glob.glob(a.csv)):
            for pid, prog in elf.read_submission(f).items():
                bank.setdefault(pid, []).append(prog)
    if a.ids_file:
        ids = [l.strip() for l in open(a.ids_file) if l.strip()]
    else:
        ids = sorted(k for k, v in cur.items() if a.lo <= len(v) <= a.hi)
        random.Random(a.seed).shuffle(ids); ids = ids[:a.n]

    def donors(pid):
        L = len(cur[pid]); seen = {json.dumps(cur[pid])}; out = []
        for prog in sorted(bank.get(pid, []), key=len):
            k = json.dumps(prog)
            if k in seen or len(prog) > L + a.slack:
                continue
            seen.add(k)
            try:
                elf.replay(P[pid], prog)
            except Exception:
                continue
            out.append(prog)
            if len(out) >= a.maxprog:
                break
        return out

    jobs = [(pid, donors(pid)) for pid in ids]
    jobs.sort(key=lambda j: -len(j[1]))

    G.update(P=P, cur=cur, cp=a.cp)
    t0 = time.time()
    with get_context("fork").Pool(a.jobs) as pool:
        res = pool.map(work, jobs, chunksize=1)
    cands, gain = {}, 0
    for pid, prog, nd, base, nrec, n, dt in res:
        L = len(cur[pid])
        print(f"{pid} len {L} donors {nd} recipes {base}->{nrec} cp {n} {'GAIN ' + str(L - len(prog)) if prog else ''} {dt:.0f}s", flush=True)
        if prog:
            cands[pid] = prog; gain += L - len(prog)
    with_d = sum(1 for j in jobs if j[1])
    print(f"puzzles {len(ids)} (with donors {with_d}), candidates {len(cands)}, gain {gain}, {time.time() - t0:.0f}s")
    if cands and not a.dry:
        imp, g = offer(cands, store_puzzles(), "bank")
        print(f"offer: improved {imp}, gained {g}")


if __name__ == "__main__":
    main()
