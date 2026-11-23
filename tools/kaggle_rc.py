"""Перекомбинация (elfopt LNS + CP-SAT) на приватных CPU-ноутбуках Kaggle.

  python3 tools/kaggle_rc.py push   [--shards 4] [--lo 30] [--hi 180] [--hours 11]   # отправить/перезапустить
  python3 tools/kaggle_rc.py status                                                  # состояние ноутбуков
  python3 tools/kaggle_rc.py pull                                                    # забрать best.csv и влить через верификатор

Каждый ноутбук — приватный скрипт radjapovanvar/elf-rc-<k>: внутри упакованы elfopt.cpp и скрипты
перекомбинации (third_party/goodpjw_dag_lns, Apache-2.0) и текущее хранилище; ноутбук гоняет свою долю
головоломок (k-я из shards по кругу) до --hours часов и пишет /kaggle/working/best.csv после каждого прохода.
Повторный push = новая версия ноутбука (стартует от свежего хранилища).
"""
import argparse, base64, io, json, os, sys, tarfile, time, urllib.error, urllib.parse, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from store import load_best, load_puzzles, merge  # noqa: E402
from tools.kaggle_auth import request as kreq  # noqa: E402

API = "https://www.kaggle.com/api/v1"
USER = "radjapovanvar"
G = os.path.join(ROOT, "third_party/goodpjw_dag_lns")
OUT = os.path.join(ROOT, "work", "kaggle_rc")

KERNEL = r'''# Private compute notebook (Santa 2026). Payload: elfopt + recombination scripts (Apache-2.0) + programs.
import base64, glob, io, os, shutil, subprocess, sys, tarfile, time
T0 = time.time(); HOURS = __HOURS__; SHARD = __SHARD__
R = "/kaggle/tmp/rc"; os.makedirs(R, exist_ok=True)
PAYLOAD = "__PAYLOAD__"
tarfile.open(fileobj=io.BytesIO(base64.b64decode(PAYLOAD)), mode="r:xz").extractall(R)
pz = glob.glob("/kaggle/input/**/puzzles.csv", recursive=True)
os.makedirs(f"{R}/data", exist_ok=True); shutil.copy(pz[0], f"{R}/data/puzzles.csv")
try:
    import ortools  # noqa
except ImportError:
    subprocess.run([sys.executable, "-m", "pip", "install", "-q", "ortools"], check=True)
os.makedirs(f"{R}/bin", exist_ok=True)
subprocess.run(["g++", "-std=c++17", "-O3", "-march=native", "-o", f"{R}/bin/elfopt", f"{R}/src/elfopt.cpp"], check=True)
jobs = os.cpu_count() or 4
print("shard", SHARD, "cpus", jobs, "setup", round(time.time() - T0), "s", flush=True)
k = 0
while time.time() - T0 < HOURS * 3600 - 45 * 60:
    k += 1
    subprocess.run([sys.executable, "src/recomb_pass.py", "--only", "work/focus_ids.txt", "--time", "15", "--cp", "25",
                    "--jobs", str(jobs), "--bin", "bin/elfopt", "--tag", f"kg{SHARD}"], cwd=R)
    for d in glob.glob(f"{R}/work/rc_*"): shutil.rmtree(d, ignore_errors=True)
    shutil.copy(f"{R}/work/best.csv", "/kaggle/working/best.csv")
    print("pass", k, "done at", round((time.time() - T0) / 60), "min", flush=True)
'''


def api(path, data=None, method=None):
    body = json.dumps(data).encode() if data is not None else None
    h = {"Content-Type": "application/json"} if body else {}
    with urllib.request.urlopen(kreq(f"{API}/{path}", data=body, headers=h, method=method), timeout=300) as r:
        return json.loads(r.read())


def payload(ids):
    """tar.xz: src/{elfopt.cpp, *.py}, work/best.csv (всё хранилище), work/focus_ids.txt (доля)."""
    P, B = load_puzzles(), load_best()
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:xz") as t:
        def add(name, data):
            ti = tarfile.TarInfo(name); ti.size = len(data); ti.mtime = int(time.time()); t.addfile(ti, io.BytesIO(data))
        for f in ["elfopt.cpp", "elf.py", "pfmt.py", "recombine.py", "recomb_pass.py"]:
            add(f"src/{f}", open(os.path.join(G, f), "rb").read())
        add("work/best.csv", export_csv(P, B).encode())
        add("work/focus_ids.txt", "\n".join(ids).encode())
    return base64.b64encode(buf.getvalue()).decode()


def export_csv(P, B):
    import csv as _csv
    s = io.StringIO(); w = _csv.writer(s); w.writerow(["id", "program"])
    for pid in P:
        w.writerow([pid, json.dumps(B[pid]["program"])])
    return s.getvalue()


def shards(n, lo, hi, fresh=0):
    B = load_best()
    ids = sorted((pid for pid, v in B.items() if lo <= v["score"] <= hi), key=lambda p: -B[p]["score"])
    if fresh:   # только головоломки, изменившиеся за последние fresh часов (свежие программы в ~6 раз продуктивнее)
        import subprocess
        h = subprocess.run(["git", "-C", ROOT, "log", f"--before={int(fresh)} hours ago", "-1", "--format=%H", "--", "best/best.json"], capture_output=True, text=True).stdout.strip()
        old = json.loads(subprocess.run(["git", "-C", ROOT, "show", f"{h}:best/best.json"], capture_output=True, text=True).stdout)
        ids = [p for p in ids if json.dumps(B[p]["program"]) != json.dumps(old[p]["program"])]
    return [ids[k::n] for k in range(n)]


def push(a):
    for k, ids in enumerate(shards(a.shards, a.lo, a.hi, a.fresh)):
        if a.only is not None and k != a.only: continue
        src = KERNEL.replace("__HOURS__", str(a.hours)).replace("__SHARD__", str(k))
        text = src.replace("__PAYLOAD__", payload(ids))
        req = {"slug": f"{USER}/elf-rc-{k}", "newTitle": f"elf rc {k}", "text": text, "language": "python",
               "kernelType": "script", "isPrivate": True, "enableGpu": False, "enableTpu": False, "enableInternet": True,
               "datasetDataSources": [], "competitionDataSources": ["santa-2026"], "kernelDataSources": [], "categoryIds": []}
        try:
            r = api("kernels/push", req, "POST")
        except urllib.error.HTTPError as e:
            print(k, "HTTP", e.code, e.read()[:300]); continue
        print(f"shard {k}: {len(ids)} puzzles, {len(text) // 1024} KB ->", r.get("url") or r, r.get("error") or "")


def kstatus(k):
    try:
        return api("kernels/status?" + urllib.parse.urlencode({"userName": USER, "kernelSlug": f"elf-rc-{k}"})).get("status")
    except urllib.error.HTTPError as e:
        return f"http{e.code}"


def cycle(a):
    """Каждый закончившийся (или несуществующий) ноутбук: pull своей доли → push на свежих."""
    for k in range(a.shards):
        st = kstatus(k)
        if st in ("running", "queued"): continue
        print(f"[{time.strftime('%H:%M')}] shard {k}: {st} -> pull + push", flush=True)
        a.only = k
        if st == "complete": pull(a)
        push(a)
    a.only = None


def status(a):
    for k in range(a.shards):
        try:
            r = api("kernels/status?" + urllib.parse.urlencode({"userName": USER, "kernelSlug": f"elf-rc-{k}"}))
            print(k, r.get("status"), r.get("failureMessage") or "")
        except urllib.error.HTTPError as e:
            print(k, "HTTP", e.code)


def pull(a):
    os.makedirs(OUT, exist_ok=True); P = load_puzzles()
    for k in range(a.shards):
        if a.only is not None and k != a.only: continue
        q = urllib.parse.urlencode({"userName": USER, "kernelSlug": f"elf-rc-{k}"})
        try:
            d = api("kernels/output?" + q)
        except urllib.error.HTTPError as e:
            print(k, "HTTP", e.code); continue
        for f in d.get("files", []):
            if f["fileName"] == "best.csv":
                with urllib.request.urlopen(kreq(f["url"]), timeout=300) as r:
                    p = os.path.join(OUT, f"best_{k}.csv"); open(p, "wb").write(r.read())
                merge(p, P)
        log = d.get("log")
        if log:
            open(os.path.join(OUT, f"log_{k}.txt"), "w").write(log if isinstance(log, str) else json.dumps(log))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["push", "status", "pull", "cycle"])
    ap.add_argument("--shards", type=int, default=4); ap.add_argument("--lo", type=int, default=30)
    ap.add_argument("--hi", type=int, default=180); ap.add_argument("--hours", type=float, default=11)
    ap.add_argument("--fresh", type=float, default=0, help="только изменившиеся за N часов")
    ap.add_argument("--only", type=int, default=None, help="только ноутбук k")
    a = ap.parse_args()
    ap2 = None
    {"push": push, "status": status, "pull": pull, "cycle": cycle}[a.cmd](a)
