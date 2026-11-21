import sys, os, time, csv
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from store import load_puzzles, load_best, offer
from proto.prefix_resynth import resynth_prefix
P = load_puzzles(); B = load_best()
fam = {r['id']: r['fam'] for r in csv.DictReader(open('data/families_inferred.csv'))}
ids = [p for p in P if fam[p] == sys.argv[1]][:int(sys.argv[2])] if len(sys.argv) > 2 else list(P)
t0 = time.time(); found = {}; gain = 0
for i, pid in enumerate(ids):
    s, t, vl = P[pid]; prog = B[pid]["program"]
    t1 = time.time(); new = resynth_prefix(s, t, vl, prog); print(f"  {pid} {len(prog)} {time.time()-t1:.1f}s", flush=True)
    if new:
        found[pid] = new; gain += len(prog) - len(new)
        print(f"  {pid}: {len(prog)} -> {len(new)}", flush=True)
print(f"{len(ids)} puzzles, {len(found)} improved, gain {gain} ({time.time()-t0:.0f}s)")
if "--apply" in sys.argv:
    print(offer(found, P, "prefix_resynth"))
