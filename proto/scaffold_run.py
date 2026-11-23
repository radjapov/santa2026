import json, sys, subprocess
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tools.scx_run import encode
from store import load_puzzles
P = load_puzzles(); old = json.load(open(sys.argv[1])); ex = json.load(open('external/src/tkm2261_exposed.json'))
ids = open('work/poly_mixed.txt').read().split()
def one(job):
    pid, which = job; s, t, L = P[pid]
    prog = ex[pid] if which == 'tkm' else old[pid]['program']
    r = subprocess.run(['bin/scaffold_probe', '--polish', '30', '--reps', '3'], input=encode(pid, s, t, L, prog), capture_output=True, text=True)
    return which, r.stdout.strip()
jobs = [(p, w) for p in ids for w in ('tkm', 'old')]
with ThreadPoolExecutor(int(sys.argv[2])) as ex_:
    for which, line in ex_.map(one, jobs):
        print(which, line, flush=True)
