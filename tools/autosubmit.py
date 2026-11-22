"""Авто-сабмит (разрешён пользователем 22 ноя): раз в --every секунд, если хранилище лучше лучшего
отправленного счёта хотя бы на --min, — export + полная проверка + отправка. Не больше --cap сабмитов в сутки (UTC).
  python3 tools/autosubmit.py --every 600 --min 3        остановка: touch work/STOP_AUTOSUBMIT
"""
import argparse, json, os, subprocess, sys, time, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from store import load_best  # noqa: E402
from tools.kaggle_auth import request as kreq  # noqa: E402

STOP = os.path.join(ROOT, "work", "STOP_AUTOSUBMIT")
API = "https://www.kaggle.com/api/v1/competitions/submissions/list/santa-2026?pageSize=50"


def submissions():
    with urllib.request.urlopen(kreq(API), timeout=60) as r:
        return json.loads(r.read())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--every", type=int, default=600)
    ap.add_argument("--min", type=int, default=3)
    ap.add_argument("--cap", type=int, default=30)
    a = ap.parse_args()
    while not os.path.exists(STOP):
        try:
            subs = submissions()
            done = [s for s in subs if s.get("status") == "complete" and s.get("publicScore")]
            pending = [s for s in subs if s.get("status") == "pending"]
            best_sub = min(float(s["publicScore"]) for s in done) if done else 1e18
            today = time.strftime("%Y-%m-%d", time.gmtime())
            n_today = sum(1 for s in subs if str(s.get("date", "")).startswith(today))
            total = sum(v["score"] for v in load_best().values())
            if not pending and total <= best_sub - a.min and n_today < a.cap:
                print(f"[{time.strftime('%H:%M:%S')}] store {total} < submitted {best_sub:.0f} by {best_sub - total:.0f}: submitting", flush=True)
                out = os.path.join(ROOT, "subs", "auto.csv")
                subprocess.run([sys.executable, "store.py", "export", out], cwd=ROOT, check=True)
                subprocess.run([sys.executable, "store.py", "check", out], cwd=ROOT, check=True)
                subprocess.run([sys.executable, "tools/submit.py", out, f"auto: store {total}"], cwd=ROOT, check=True)
        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] error: {e}", flush=True)
        for _ in range(a.every // 10):
            if os.path.exists(STOP):
                break
            time.sleep(10)


if __name__ == "__main__":
    main()
