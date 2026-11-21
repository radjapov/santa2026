"""Convert between submission CSV and the simple text block format used by the native engines."""
import sys, os, json
sys.path.insert(0, os.path.dirname(__file__))
from elf import *

def encode(p, prog):
    s = ['P', p.id, str(len(p.starts))] + [str(x) for x in p.starts]
    s += [str(len(p.targets))] + [str(x) for x in p.targets]
    s += [str(p.limit), str(p.registers), str(len(prog))]
    return '\n'.join([' '.join(s)] + ['%s %d %d %d' % tuple(x) for x in prog]) + '\n'

def parse_results(text):
    res = {}; lines = text.split('\n'); k = 0
    while k < len(lines):
        ln = lines[k].split()
        if ln and ln[0] == 'R':
            pid, n = ln[1], int(ln[3]); prog = []
            for t in range(n):
                op, i, j, d = lines[k + 1 + t].split(); prog.append([op, int(i), int(j), int(d)])
            res[pid] = prog; k += n + 1
        else: k += 1
    return res

if __name__ == "__main__":
    # split: pfmt.py split <csv> <outdir> <nshards> [order=cost|id]
    cmd = sys.argv[1]
    if cmd == "split":
        P = load_puzzles(); S = read_submission(sys.argv[2]); out = sys.argv[3]; n = int(sys.argv[4])
        os.makedirs(out, exist_ok=True)
        ids = sorted(P, key=lambda k: -len(S[k]))
        shards = [[] for _ in range(n)]
        for i, pid in enumerate(ids): shards[i % n].append(encode(P[pid], S[pid]))
        for i, sh in enumerate(shards): open(f"{out}/in_{i:02d}.txt", "w").write(''.join(sh))
