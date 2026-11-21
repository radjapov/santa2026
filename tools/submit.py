"""Отправка сабмита в Kaggle через REST API (токен подставляет прокси среды или ~/.kaggle).
Перед отправкой файл проходит полную проверку store.check_file; при любой ошибке отправки нет.
  python tools/submit.py subs/submit.csv "описание"
"""
import json, os, sys, time, urllib.parse, urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, ROOT)
from store import check_file, load_puzzles  # noqa: E402

API = "https://www.kaggle.com/api/v1"
COMP = "santa-2026"


def req(url, data=None, headers=None, method=None):
    r = urllib.request.Request(url, data=data, headers=headers or {}, method=method)
    with urllib.request.urlopen(r, timeout=120) as resp:
        return resp.read()


def main(path, desc):
    total = check_file(path, load_puzzles())
    print(f"VALID, total {total}")
    st = os.stat(path)
    meta = {"type": 1, "name": os.path.basename(path), "contentLength": st.st_size,
            "contentType": "text/csv", "lastModifiedEpochSeconds": int(st.st_mtime)}
    blob = json.loads(req(f"{API}/blobs/upload", json.dumps(meta).encode(), {"Content-Type": "application/json"}))
    with open(path, "rb") as f:
        req(blob["createUrl"], f.read(), {"Content-Type": "text/csv"}, "PUT")
    form = urllib.parse.urlencode({"blobFileTokens": blob["token"],
                                   "submissionDescription": f"{desc} [verified {total}]"}).encode()
    res = json.loads(req(f"{API}/competitions/submissions/submit/{COMP}", form))
    ref = res.get("ref")
    print(res.get("message"), ref)
    for _ in range(40):
        time.sleep(15)
        subs = json.loads(req(f"{API}/competitions/submissions/list/{COMP}?pageSize=10"))
        s = next((x for x in subs if x.get("ref") == ref), {})
        if s.get("status") not in (None, "pending"):
            print(f"status {s.get('status')}, score {s.get('publicScore')} {s.get('errorDescription') or ''}")
            return
    print("still pending; check the Submissions page")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "auto")
