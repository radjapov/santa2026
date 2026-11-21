"""Collect engine outputs (R-blocks) from files, validate, merge into a best submission."""
import sys, os, glob
sys.path.insert(0, os.path.dirname(__file__))
from pfmt import *
def collect(base_csv, patterns, out_csv=None, verbose=True):
    P = load_puzzles(); best = read_submission(base_csv)
    base_total = sum(len(best[k]) for k in P)
    gained = {}
    for pat in patterns:
        for f in glob.glob(pat):
            try: res = parse_results(open(f).read())
            except Exception as e: print("parse fail", f, e); continue
            for pid, prog in res.items():
                try: n = replay(P[pid], prog)
                except ValueError as e: print("INVALID", pid, f, e); continue
                if n < len(best[pid]):
                    gained[pid] = len(best[pid]) - n if pid not in gained else gained[pid] + len(best[pid]) - n
                    best[pid] = prog
    tot = sum(len(best[k]) for k in P)
    if verbose: print(f"base {base_total} -> {tot}  (improved {len(gained)} puzzles, -{base_total - tot})")
    if out_csv: write_submission(out_csv, P, best)
    return best, tot
if __name__ == "__main__":
    collect(sys.argv[1], sys.argv[3:], sys.argv[2])
