"""Улучшения, которые публичные ноутбуки держат прямо в исходнике (CSV не пишут), — синхронизация их не видит.
Известные форматы (только разбор данных, код ноутбука НЕ выполняется):
  pardheev «Best Public Score»:      GIFT = '''<base64(zlib(json {id: program}))>'''
  asteriospanoras «Gift per Run»:    CHANGE = {...}  (JSON-литерал)
  python3 tools/gift_sync.py   → store.offer для всего, что строго короче
"""
import base64, json, os, re, sys, urllib.parse, urllib.request, zlib
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from store import load_puzzles, offer  # noqa: E402
from tools.kaggle_auth import request as kreq  # noqa: E402
API = "https://www.kaggle.com/api/v1"
AUTHORS = ["pardheev", "asteriospanoras"]


def get(path, **q):
    with urllib.request.urlopen(kreq(f"{API}/{path}?{urllib.parse.urlencode(q)}"), timeout=120) as r:
        return json.loads(r.read())


def extract(src):
    try:
        nb = json.loads(src); text = "\n".join("".join(c["source"]) for c in nb["cells"])
    except ValueError:
        text = src
    out = {}
    for m in re.finditer(r"GIFT\s*=\s*'''(.*?)'''", text, re.S):
        try:
            out.update(json.loads(zlib.decompress(base64.b64decode("".join(m.group(1).split())))))
        except Exception:
            pass
    for m in re.finditer(r"CHANGE\s*=\s*(\{)", text):
        d, j = 0, m.start(1)
        for k in range(j, len(text)):
            d += text[k] == "{"; d -= text[k] == "}"
            if d == 0:
                try: out.update(json.loads(text[j:k + 1]))
                except ValueError: pass
                break
    return {k: v for k, v in out.items() if isinstance(v, list)}


def run(log=print):
    gifts = {}
    for user in AUTHORS:
        try:
            ks = get("kernels/list", user=user, competition="santa-2026", pageSize=20, sortBy="dateRun")
        except Exception as e:
            log(f"gift: list {user} failed: {e}"); continue
        for k in ks[:6]:
            owner, slug = k["ref"].split("/")
            try:
                src = get("kernels/pull", userName=owner, kernelSlug=slug).get("blob", {}).get("source", "")
            except Exception:
                continue
            g = extract(src)
            for pid, prog in g.items():
                if pid not in gifts or len(prog) < len(gifts[pid]):
                    gifts[pid] = prog
    if not gifts:
        return 0, 0
    imp, gain = offer(gifts, load_puzzles(), "gift")
    log(f"gift: {len(gifts)} programs in sources, improved {imp}, gain {gain}")
    return imp, gain


if __name__ == "__main__":
    run()


GITHUB = [("https://github.com/Hansen0223/Santa_2026", "external/github/Santa_2026")]


def run_github(log=print):
    """Публичные GitHub-репозитории с программами: при новом коммите — merge всех CSV id,program (только данные)."""
    import subprocess, glob as _g
    from store import merge as _merge
    imp_total = 0
    for url, d in GITHUB:
        path = os.path.join(ROOT, d)
        if not os.path.isdir(path):
            subprocess.run(["git", "clone", "-q", "--depth", "1", url, path], capture_output=True)
            before = ""
        else:
            before = subprocess.run(["git", "-C", path, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
            subprocess.run(["git", "-C", path, "fetch", "-q", "--depth", "1", "origin"], capture_output=True)
            subprocess.run(["git", "-C", path, "reset", "-q", "--hard", "FETCH_HEAD"], capture_output=True)
        after = subprocess.run(["git", "-C", path, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        if after == before:
            continue
        P = load_puzzles(); n = 0
        for f in _g.glob(os.path.join(path, "**", "*.csv"), recursive=True):
            try:
                if not open(f).readline().startswith("id,program"): continue
                n += 1; _merge(f, P)
            except Exception:
                continue
        log(f"github {url.split('/')[-2]}: new commit {after[:8]}, merged {n} csv")
    return imp_total
