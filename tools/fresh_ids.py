"""Свежие головоломки: программа изменилась за последние N часов АКТИВНОСТИ хранилища (окно отсчитывается от последнего
коммита best.json, а не от «сейчас» — ночной простой ПК окно не обнуляет).
  python3 tools/fresh_ids.py HOURS LO HI > ids.txt"""
import json, os, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from store import load_best


def fresh_ids(hours, lo=30, hi=400):
    g = lambda *a: subprocess.run(["git", "-C", ROOT, *a], capture_output=True, text=True).stdout.strip()
    last = int(g("log", "-1", "--format=%ct", "--", "best/best.json") or 0)
    h = g("log", f"--before={last - int(hours * 3600)}", "-1", "--format=%H", "--", "best/best.json")
    B = load_best()
    if not h:
        return [p for p, v in B.items() if lo <= v["score"] <= hi]
    old = json.loads(g("show", f"{h}:best/best.json"))
    return [p for p, v in B.items() if lo <= v["score"] <= hi and json.dumps(v["program"]) != json.dumps(old[p]["program"])]


if __name__ == "__main__":
    print("\n".join(fresh_ids(float(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))))
