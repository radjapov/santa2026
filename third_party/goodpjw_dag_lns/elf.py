"""Core utilities: load puzzles, exact validator, submission IO, merge."""
import csv, json, os, glob
csv.field_size_limit(10**9)
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

class Puzzle:
    __slots__ = ("id", "starts", "targets", "limit", "registers")
    def __init__(s, id, starts, targets, limit, registers):
        s.id, s.starts, s.targets, s.limit, s.registers = id, starts, targets, limit, registers

def load_puzzles(path=None):
    path = path or os.path.join(ROOT, "data", "puzzles.csv")
    out = {}
    with open(path, newline="", encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            out[r["id"]] = Puzzle(r["id"], json.loads(r["starting_values"]), json.loads(r["targets"]),
                                  int(r["value_limit"]), int(r["registers"]))
    return out

def replay(p, prog):
    if len(prog) > 4096: raise ValueError("too long")
    R = p.registers
    regs = list(p.starts) + [None] * (R - len(p.starts))
    need = set(p.targets) - set(p.starts)
    L = p.limit
    for k, ins in enumerate(prog):
        if not (isinstance(ins, (list, tuple)) and len(ins) == 4): raise ValueError(f"{k}: malformed")
        op, i, j, d = ins
        for r in (i, j, d):
            if type(r) is not int or not 0 <= r < R: raise ValueError(f"{k}: bad reg {r}")
        a, b = regs[i], regs[j]
        if a is None or b is None: raise ValueError(f"{k}: empty reg")
        if op == "+": v = a + b
        elif op == "-": v = a - b
        elif op == "*": v = a * b
        elif op == "/":
            if a % b: raise ValueError(f"{k}: inexact")
            v = a // b
        else: raise ValueError(f"{k}: op {op!r}")
        if not 1 <= v <= L: raise ValueError(f"{k}: range")
        regs[d] = v
        need.discard(v)
    if need: raise ValueError(f"{len(need)} targets missing")
    return len(prog)

def read_submission(path):
    with open(path, newline="", encoding="utf-8-sig") as f:
        rd = csv.reader(f); h = next(rd)
        assert h == ["id", "program"], h
        return {pid: json.loads(prog) for pid, prog in rd}

def write_submission(path, puzzles, programs):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(["id", "program"])
        for pid in puzzles:
            cell = json.dumps(programs[pid], separators=(",", ":"))
            assert len(cell.encode()) <= 131072
            w.writerow([pid, cell])

def score_dict(puzzles, progs):
    """Return {pid: len or None(invalid)}."""
    out = {}
    for pid, p in puzzles.items():
        try: out[pid] = replay(p, progs[pid]) if pid in progs else None
        except ValueError: out[pid] = None
    return out

def merge(puzzles, sources):
    """sources: dict name -> programs. Returns best programs and per-pid source name."""
    best, who = {}, {}
    for name, src in sources.items():
        for pid, prog in src.items():
            if pid not in puzzles: continue
            if pid in best and len(prog) >= len(best[pid]): continue
            try: replay(puzzles[pid], prog)
            except ValueError: continue
            best[pid], who[pid] = prog, name
    return best, who
