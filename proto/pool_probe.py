"""Зонд: сколько стоит цель, если строить её из статического пула V (точный поиск глубины <= 5).

Формы (k = число новых инструкций, включая последнюю):
  1: T = a op b                        a,b in V
  2: T = s op a | s op s               s in S1 = {a op b}
  3: T = s op s'  | u op a             u in S2 = {s op b}
  4: T = u op s | (u op a) op b | (s*s') op a
  5: T = u op u' (+/-) | ((u op s) op a)
Возвращает минимальное k (или 6, если не нашли за 5).
"""
from __future__ import annotations


def one_step(A, B, limit, same=False):
    """Все a op b для a in A, b in B (обе стороны для - и /)."""
    out = set()
    for a in A:
        for b in B:
            if same and b < a:
                continue
            s = a + b
            if s <= limit:
                out.add(s)
            p = a * b
            if p <= limit:
                out.add(p)
            if a != b:
                out.add(abs(a - b))
            if a % b == 0:
                out.add(a // b)
            if b % a == 0:
                out.add(b // a)
    out.discard(0)
    return out


def inverses(t, a, limit):
    """Все x такие что t = x op a или t = a op x."""
    r = [t - a, t + a, a - t]
    if t % a == 0:
        r.append(t // a)
    if a % t == 0:
        r.append(a // t)
    if t * a <= limit:
        r.append(t * a)
    return [x for x in r if 0 < x <= limit]


class Pool:
    def __init__(self, V, limit):
        self.V = sorted(set(V))
        self.limit = limit
        self.Vs = set(self.V)
        self.S1 = one_step(self.V, self.V, limit, same=True)
        self.S1l = sorted(self.S1)
        # S2: s op b, s in S1, b in V (новое значение из нового + старого)
        self.S2 = one_step(self.S1l, self.V, limit) | self.S1

    def cost(self, t):
        V, S1, S2, L = self.V, self.S1, self.S2, self.limit
        if t in self.Vs:
            return 0
        if t in S1:
            return 1
        # 2: s op a / s op s
        if any(x in S1 for a in V for x in inverses(t, a, L)):
            return 2
        if (t % 2 == 0 and t // 2 in S1) or _isqrt_in(t, S1):
            return 2
        # 3: s op s' / u op a
        if t in S2:
            return 3
        if any(x in S1 for s in self.S1l for x in inverses(t, s, L)):
            return 3
        # 4: u op s ; (u op a) op b ; (s*s') op a
        if any(x in S2 for s in self.S1l for x in inverses(t, s, L)):
            return 4
        for a in V:
            for x in inverses(t, a, L):
                if any(y in S2 for b in V for y in inverses(x, b, L)):
                    return 4
        return 5 if self._five(t) else 6

    def _five(self, t):
        V, S1l, S2, L = self.V, self.S1l, self.S2, self.limit
        # u op u' сумма/разность
        for u in S2:
            if (t - u) in S2 or (t + u) in S2:
                return True
        # ((u op s) op a)
        for a in V:
            for x in inverses(t, a, L):
                for s in S1l:
                    for y in inverses(x, s, L):
                        if y in S2:
                            return True
        return False


def _isqrt_in(t, S):
    import math
    r = math.isqrt(t)
    return r * r == t and r in S


import numpy as np


class NearMiss:
    """T = X*Y +- R, X in V∪S1, Y и R in V∪S1∪S2. Стоимость = cX + cY + cR + 2
    (верхняя оценка: общие промежуточные части не учитываются)."""

    def __init__(self, pool: Pool):
        self.p = pool
        allv = {v: 0 for v in pool.V}
        for v in pool.S1:
            allv.setdefault(v, 1)
        for v in pool.S2:
            allv.setdefault(v, 2)
        items = sorted(allv.items())
        self.S = np.array([k for k, _ in items], dtype=np.int64)
        self.C = np.array([c for _, c in items], dtype=np.int64)
        xs = [(v, c) for v, c in items if c <= 1]
        self.X = xs

    def cost(self, t):
        S, C = self.S, self.C
        best = 99
        tt = np.int64(t)
        for x, cx in self.X:
            if cx + 2 >= best:
                continue
            for sign in (1, -1):
                W = tt - sign * S                  # W = t - R  (sign=1)  или t + R (sign=-1)
                ok = (W > 0) & (W % x == 0)
                if not ok.any():
                    continue
                idx = np.nonzero(ok)[0]
                Y = W[idx] // x
                pos = np.searchsorted(S, Y)
                pos[pos >= len(S)] = len(S) - 1
                hit = S[pos] == Y
                if hit.any():
                    tot = cx + C[idx[hit]] + C[pos[hit]] + 2
                    best = min(best, int(tot.min()))
        return best
