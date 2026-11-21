"""Хранилище лучших программ по каждой головоломке + сборка сабмита.

  python store.py merge subs/candidate.csv   # проверить кандидата, взять улучшения
  python store.py export subs/submit.csv     # собрать сабмит (с полной проверкой)
  python store.py status                     # текущий суммарный счёт

best/best.json: {puzzle_id: {"program": [...], "score": n, "source": "..."}}
Любая программа попадает в хранилище только после прохождения verifier.run.
"""
from __future__ import annotations

import csv
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


def read_sub(path):
    with open(path, newline="") as f:
        return {row["id"]: row["program"] for row in csv.DictReader(f)}


def merge(path, puzzles):
    best = load_best()
    sub = read_sub(path)
    improved = gained = bad = 0
    for pid, s in sub.items():
        if pid not in puzzles:
            bad += 1
            continue
        try:
            prog = parse_program(s)
            score = run(prog, *puzzles[pid])
        except InvalidProgram as e:
            bad += 1
            print(f"  invalid {pid}: {e}")
            continue
        old = best.get(pid)
        if old is None or score < old["score"]:
            gained += (old["score"] - score) if old else 0
            best[pid] = {"program": prog, "score": score, "source": os.path.basename(path)}
            improved += 1
    save_best(best)
    print(f"merged {path}: improved {improved}, gained {gained}, invalid {bad}")


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
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["id", "program"])
        w.writerows(rows)
    # повторная проверка уже записанного файла
    back = read_sub(path)
    assert len(back) == len(puzzles)
    assert sum(run(parse_program(back[p]), *puzzles[p]) for p in puzzles) == total
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
