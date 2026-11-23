"""Автосинхронизация с публичным фронтом.

Раз в --every секунд: берёт ноутбуки соревнования, перезапущенные с прошлого раза,
и свежие датасеты по запросу santa-2026; скачивает CSV вида id,program;
каждую программу проверяет verifier.run и сливает в хранилище только строго лучшие.
После улучшений коммитит и пушит best/best.json (если --push).

  python tools/sync_public.py --every 5400 --push
  python tools/sync_public.py --once
Остановка: touch work/STOP_SYNC
"""
import argparse, csv, io, json, os, sys, time, urllib.parse, urllib.request, zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, ROOT)
from store import commit_push, load_best, load_puzzles, offer  # noqa: E402
from verifier import InvalidProgram, parse_program  # noqa: E402
from tools.kaggle_auth import request as kreq  # noqa: E402

API = "https://www.kaggle.com/api/v1"
STATE = os.path.join(ROOT, "work", "sync_state.json")
STOP = os.path.join(ROOT, "work", "STOP_SYNC")
MAX_CSV = 40 * 2**20
MAX_ZIP = 200 * 2**20
csv.field_size_limit(10**8)


def get(url, limit):
    with urllib.request.urlopen(kreq(url), timeout=300) as r:
        data = r.read(limit + 1)
    if len(data) > limit:
        raise ValueError("too large")
    return data


def api(path, **q):
    return json.loads(get(f"{API}/{path}?{urllib.parse.urlencode(q)}", 20 * 2**20))


def programs_from_csv(data):
    text = data.decode("utf-8-sig", errors="replace")
    head = text.split("\n", 1)[0].replace('"', "").strip()
    if head != "id,program":
        return {}
    out = {}
    for row in csv.DictReader(io.StringIO(text)):
        try:
            out[row["id"]] = parse_program(row["program"])
        except (InvalidProgram, KeyError, TypeError):
            pass
    return out


def load_state():
    try:
        return json.load(open(STATE))
    except (OSError, ValueError):
        return {"kernels": {}, "datasets": {}}


def sync_once(P, state, log):
    found = []  # (source, {pid: prog})
    # 1) ноутбуки: последние перезапущенные
    for page in (1, 2):
        try:
            ks = api("kernels/list", competition="santa-2026", sortBy="dateRun", pageSize=100, page=page)
        except Exception as e:
            log(f"kernel list failed: {e}")
            break
        for k in ks:
            ref, run = k.get("ref"), k.get("lastRunTime", "")
            if not ref or state["kernels"].get(ref) == run:
                continue
            owner, slug = ref.split("/")
            try:
                d = api("kernels/output", userName=owner, kernelSlug=slug)
                for f in d.get("files", []):
                    if f.get("fileName", "").endswith(".csv"):
                        try:
                            progs = programs_from_csv(get(f["url"], MAX_CSV))
                        except ValueError:
                            continue
                        if progs:
                            found.append((f"k:{ref}", progs))
                state["kernels"][ref] = run
            except Exception as e:
                log(f"  {ref}: {e}")
    # 2) датасеты: недавно обновлённые по запросу
    try:
        ds = api("datasets/list", search="santa-2026", sortBy="updated", pageSize=40)
    except Exception as e:
        log(f"dataset list failed: {e}")
        ds = []
    for dset in ds:
        ref, upd = dset.get("ref"), dset.get("lastUpdated", "")
        if not ref or state["datasets"].get(ref) == upd:
            continue
        if int(dset.get("totalBytes") or 0) > MAX_ZIP:
            state["datasets"][ref] = upd
            continue
        owner, slug = ref.split("/")
        try:
            z = zipfile.ZipFile(io.BytesIO(get(f"{API}/datasets/download/{owner}/{slug}", MAX_ZIP)))
            for m in z.infolist():
                if m.filename.endswith(".csv") and m.file_size <= MAX_CSV:
                    progs = programs_from_csv(z.read(m))
                    if progs:
                        found.append((f"d:{ref}/{os.path.basename(m.filename)}", progs))
            state["datasets"][ref] = upd
        except Exception as e:
            log(f"  {ref}: {e}")
    # 3) слияние: каждый источник целиком через верификатор
    total_imp = total_gain = 0
    for src, progs in found:
        progs = {p: v for p, v in progs.items() if p in P}
        imp, gain = offer(progs, P, src[:120], quiet=True)
        if imp:
            log(f"  +{gain} from {src} ({imp} puzzles)")
        total_imp += imp
        total_gain += gain
    return len(found), total_imp, total_gain


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--every", type=int, default=5400)
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--push", action="store_true")
    a = ap.parse_args()
    P = load_puzzles()
    os.makedirs(os.path.dirname(STATE), exist_ok=True)

    def log(msg):
        print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)

    while not os.path.exists(STOP):
        state = load_state()
        n, imp, gain = sync_once(P, state, log)
        json.dump(state, open(STATE, "w"))
        try:  # улучшения, лежащие прямо в исходниках ноутбуков (pardheev GIFT, asteriospanoras CHANGE)
            from tools.gift_sync import run as gift_run
            gi, gg = gift_run(log); imp += gi; gain += gg
        except Exception as e:
            log(f"gift: error {e}")
        tot = sum(v["score"] for v in load_best().values())
        log(f"sync: {n} new files, improved {imp} puzzles, gain {gain}, total {tot}")
        if imp and a.push:
            log(commit_push(f"Store: {tot} (public sync)"))
        if a.once:
            break
        for _ in range(a.every // 30):
            if os.path.exists(STOP):
                break
            time.sleep(30)


if __name__ == "__main__":
    main()
