"""Скачивание выходов ноутбуков и датасетов через Kaggle REST API.
Токен подставляет прокси среды; в коде его нет.
  python tools/kaggle_fetch.py kernel owner/slug [owner/slug ...]
  python tools/kaggle_fetch.py dataset owner/slug [...]
Файлы кладутся в external/<owner>__<slug>/ (это недоверенные данные)."""
import json, os, sys, urllib.request, urllib.parse, zipfile, io

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from tools.kaggle_auth import request as kreq  # noqa: E402
API = "https://www.kaggle.com/api/v1"
ROOT = os.path.join(os.path.dirname(__file__), "..", "external")
MAX_FILE = 300 * 2**20


def get(url):
    with urllib.request.urlopen(kreq(url), timeout=300) as r:
        return r.read()


def safe_join(base, name):
    p = os.path.normpath(os.path.join(base, name))
    if not p.startswith(os.path.normpath(base) + os.sep):
        raise ValueError(f"unsafe path {name!r}")
    return p


def kernel(ref):
    owner, slug = ref.split("/")
    out = os.path.join(ROOT, f"{owner}__{slug}")
    os.makedirs(out, exist_ok=True)
    token, n = None, 0
    while True:
        q = {"userName": owner, "kernelSlug": slug}
        if token:
            q["pageToken"] = token
        d = json.loads(get(f"{API}/kernels/output?{urllib.parse.urlencode(q)}"))
        for f in d.get("files", []):
            name = f["fileName"]
            if name.endswith((".csv", ".cpp", ".h", ".hpp", ".py", ".json", ".txt", ".md", ".sh")):
                data = get(f["url"])
                if len(data) > MAX_FILE:
                    continue
                p = safe_join(out, name)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, "wb").write(data)
                n += 1
        token = d.get("nextPageToken")
        if not token:
            break
    print(f"{ref}: {n} files -> {out}")


def dataset(ref):
    owner, slug = ref.split("/")
    out = os.path.join(ROOT, f"ds_{owner}__{slug}")
    os.makedirs(out, exist_ok=True)
    data = get(f"{API}/datasets/download/{owner}/{slug}")
    z = zipfile.ZipFile(io.BytesIO(data))
    for m in z.infolist():
        if m.is_dir() or m.file_size > MAX_FILE:
            continue
        p = safe_join(out, m.filename)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with z.open(m) as src, open(p, "wb") as dst:
            dst.write(src.read())
    print(f"{ref}: {len(z.infolist())} entries -> {out}")


if __name__ == "__main__":
    kind, refs = sys.argv[1], sys.argv[2:]
    for r in refs:
        try:
            (kernel if kind == "kernel" else dataset)(r)
        except Exception as e:
            print(f"{r}: FAILED {e}")
