"""Заголовок авторизации Kaggle для локальных машин.
В облачной среде токен подставляет прокси, файла нет — заголовок не добавляется.
Локально: токен из $KAGGLE_API_TOKEN или ~/.kaggle/access_token. Отправляется только на www.kaggle.com."""
import os
import urllib.parse
import urllib.request


def _token():
    t = os.environ.get("KAGGLE_API_TOKEN", "").strip()
    if t and t not in ("proxy-injected",):
        return t
    p = os.path.expanduser("~/.kaggle/access_token")
    if os.path.exists(p):
        return open(p).read().strip()
    return None


def request(url, data=None, headers=None, method=None):
    h = dict(headers or {})
    tok = _token()
    if tok and urllib.parse.urlparse(url).hostname == "www.kaggle.com":
        h["Authorization"] = f"Bearer {tok}"
    return urllib.request.Request(url, data=data, headers=h, method=method)
