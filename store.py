"""Хранилище лучших программ по каждой головоломке + сборка сабмита.

  python store.py merge subs/candidate.csv   # проверить кандидата, взять улучшения
  python store.py export subs/submit.csv     # собрать сабмит (с полной проверкой)
  python store.py merge-best other/best.json # слить хранилище с другой машины
  python store.py check subs/any.csv         # финальная проверка файла перед отправкой
  python store.py status                     # текущий суммарный счёт

best/best.json: {puzzle_id: {"program": [...], "score": n, "source": "..."}}
Любая программа попадает в хранилище только после прохождения verifier.run.
"""
from __future__ import annotations

import contextlib
import csv
import fcntl
import json
import os
import sys
import tempfile

from verifier import InvalidProgram, parse_program, run

BEST = os.path.join(os.path.dirname(__file__), "best", "best.json")


DATA = os.path.join(os.path.dirname(__file__), "data")


def load_puzzles(name="puzzles.csv"):
    """{puzzle_id: (starts, targets, value_limit)} в порядке файла."""
    out = {}
    with open(os.path.join(DATA, name), newline="") as f:
        for row in csv.DictReader(f):
            assert int(row["registers"]) == 48, row["id"]
            out[row["id"]] = (json.loads(row["starting_values"]),
                              json.loads(row["targets"]), int(row["value_limit"]))
    return out


def load_best():
    if not os.path.exists(BEST):
        return {}
    with open(BEST) as f:
        return json.load(f)


def save_best(best):
    # атомарная запись, чтобы не потерять хранилище при обрыве
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(BEST))
    with os.fdopen(fd, "w") as f:
        json.dump(best, f, separators=(",", ":"))
    os.replace(tmp, BEST)


@contextlib.contextmanager
def locked():
    """Эксклюзивная блокировка хранилища (несколько процессов пишут параллельно)."""
    with open(BEST + ".lock", "w") as lf:
        fcntl.flock(lf, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(lf, fcntl.LOCK_UN)


def offer(cands, puzzles, source):
    """cands: {pid: program}. Проверяет каждую, под блокировкой берёт строго лучшие.
    Возвращает (число улучшенных головоломок, выигрыш в инструкциях)."""
    checked = {}
    for pid, prog in cands.items():
        try:
            checked[pid] = (prog, run(prog, *puzzles[pid]))
        except InvalidProgram as e:
            print(f"  INVALID {source} {pid}: {e}", flush=True)
    imp = gain = 0
    with locked():
        best = load_best()
        for pid, (prog, n) in checked.items():
            old = best.get(pid)
            if old is None or n < old["score"]:
                gain += (old["score"] - n) if old else 0
                imp += 1
                best[pid] = {"program": prog, "score": n, "source": source}
        if imp:
            save_best(best)
    return imp, gain


def read_sub(path):
    with open(path, newline="") as f:
        return {row["id"]: row["program"] for row in csv.DictReader(f)}


EXPECTED_IDS = [f"scored_{i:04d}" for i in range(1, 1001)]


def check_file(path, puzzles, expect_total=None):
    """Полная проверка файла сабмита по Evaluation. Возвращает счёт или падает."""
    with open(path, newline="", encoding="utf-8") as f:
        r = csv.reader(f)
        header = next(r)
        if header != ["id", "program"]:
            raise SystemExit(f"bad header {header!r}")
        rows = list(r)
    ids = [row[0] for row in rows]
    if any(len(row) != 2 for row in rows):
        raise SystemExit("row with wrong column count")
    if len(ids) != len(set(ids)) or sorted(ids) != EXPECTED_IDS:
        raise SystemExit("ID set != scored_0001..scored_1000 exactly once")
    total = 0
    for pid, s in rows:
        try:
            total += run(parse_program(s), *puzzles[pid])
        except InvalidProgram as e:
            raise SystemExit(f"INVALID {pid}: {e}")
    if expect_total is not None and total != expect_total:
        raise SystemExit(f"score mismatch {total} != {expect_total}")
    return total


def merge(path, puzzles):
    cands = {}
    bad = 0
    for pid, s in read_sub(path).items():
        if pid not in puzzles:
            bad += 1
            continue
        try:
            cands[pid] = parse_program(s)
        except InvalidProgram as e:
            bad += 1
            print(f"  invalid {pid}: {e}")
    imp, gain = offer(cands, puzzles, os.path.basename(path))
    print(f"merged {path}: improved {imp}, gained {gain}, unparsable {bad}")


def export(path, puzzles):
    best = load_best()
    missing = [p for p in puzzles if p not in best]
    if missing:
        sys.exit(f"нет программ для {len(missing)} головоломок, напр. {missing[:5]}")
    total = 0
    rows = []
    for pid in puzzles:  # порядок как в данных
        prog = best[pid]["program"]
        s = json.dumps(prog, separators=(",", ":"))
        total += run(parse_program(s), *puzzles[pid])  # проверяем ровно то, что пишем
        rows.append((pid, s))
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["id", "program"])
        w.writerows(rows)
    # повторная проверка уже записанного файла: формат, полный набор ID, счёт
    check_file(path, puzzles, expect_total=total)
    print(f"exported {path}: {len(rows)} puzzles, total score {total}")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "status"
    if cmd == "status":
        best = load_best()
        print(f"{len(best)} puzzles, total {sum(v['score'] for v in best.values())}")
    elif cmd == "merge":
        merge(sys.argv[2], load_puzzles())
    elif cmd == "export":
        export(sys.argv[2], load_puzzles())
    elif cmd == "merge-best":
        # слить чужой best.json (с другой машины): каждая программа снова проверяется
        with open(sys.argv[2]) as f:
            other = json.load(f)
        imp, gain = offer({p: v["program"] for p, v in other.items()}, load_puzzles(), "merge-best")
        print(f"merge-best {sys.argv[2]}: improved {imp}, gained {gain}")
    elif cmd == "check":
        print("VALID, total", check_file(sys.argv[2], load_puzzles()))
