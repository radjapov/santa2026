"""Скачать исходник публичного ноутбука (только для чтения, НЕ запускать):
  python3 tools/pull_src.py owner/slug [outdir]   → outdir/owner__slug.src (+ .txt с ячейками)"""
import json, os, sys, urllib.parse, urllib.request
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.kaggle_auth import request as kreq
ref = sys.argv[1]; out = sys.argv[2] if len(sys.argv) > 2 else "external/src"
os.makedirs(out, exist_ok=True); owner, slug = ref.split("/")
q = urllib.parse.urlencode({"userName": owner, "kernelSlug": slug})
with urllib.request.urlopen(kreq(f"https://www.kaggle.com/api/v1/kernels/pull?{q}"), timeout=120) as r:
    src = json.loads(r.read()).get("blob", {}).get("source", "")
fn = os.path.join(out, f"{owner}__{slug}.src"); open(fn, "w").write(src)
try:
    nb = json.loads(src)
    with open(fn + ".txt", "w") as f:
        for i, c in enumerate(nb["cells"]):
            s = "".join(c["source"]); f.write(f"=== cell {i} {c['cell_type']} {len(s)}\n{s}\n")
except ValueError:
    pass
print("saved", fn, len(src))
