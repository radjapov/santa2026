"""Пересборка разгона: программа до первой цели заменяется более короткой,
которая оставляет в тех же регистрах те же значения, что читает хвост программы.

Шаги:
 1. Разрез c = индекс инструкции, дающей первую цель. Хвост = prog[c:].
 2. Интерфейс F: для каждого регистра r, который хвост читает до записи, F[r] = значение r в момент c.
 3. Нужные значения N = {F[r]} без тех, что уже лежат в r с самого начала (стартовые и не тронутые).
 4. Точный поиск минимального набора операций, дающего N из стартовых (замыкание + DFS по временным).
 5. Раскладка по регистрам: каждое значение F[r] пишется прямо в r, временные — в свободные регистры;
    порядок такой, чтобы не затереть ещё нужное значение. Результат проверяется verifier.run.
"""
from __future__ import annotations

import itertools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from verifier import InvalidProgram, run  # noqa: E402

OPF = {"+": lambda x, y: x + y, "-": lambda x, y: x - y, "*": lambda x, y: x * y, "/": lambda x, y: x // y}


def recipes_for(u, S, limit):
    """Все способы получить u одной операцией из множества S: (op, x, y)."""
    out = []
    for x in S:
        if u - x in S and u - x > 0:
            out.append(("+", x, u - x))
        if u + x in S:
            out.append(("-", u + x, x))
        if x % u == 0 and x // u in S:
            out.append(("/", x, x // u))
        if u % x == 0 and u // x in S:
            out.append(("*", x, u // x))
    return out


def closure(S0, required, limit):
    """Жадно получает всё, что можно получить одной операцией из текущего S. Возвращает (S, шаги)."""
    S = set(S0)
    steps = []
    todo = set(required) - S
    progress = True
    while todo and progress:
        progress = False
        for u in sorted(todo):
            r = recipes_for(u, S, limit)
            if r:
                op, x, y = r[0]
                steps.append((op, x, y, u))
                S.add(u)
                todo.discard(u)
                progress = True
    return S, steps, todo


def temp_candidates(S, todo, limit, cap=60):
    """Временные w: получаются одной операцией из S и открывают хотя бы одно u из todo."""
    cand = {}
    for u in todo:
        for x in S:
            for w in (u - x, u + x, x - u):
                if 0 < w <= limit and w not in S:
                    cand[w] = cand.get(w, 0) + 1
            if u % x == 0:
                cand[u // x] = cand.get(u // x, 0) + 1
            if x % u == 0:
                cand[x // u] = cand.get(x // u, 0) + 1
            if u * x <= limit:
                cand[u * x] = cand.get(u * x, 0) + 1
    good = [(c, w) for w, c in cand.items() if w not in S and recipes_for(w, S, limit)]
    good.sort(reverse=True)
    return [w for _, w in good[:cap]]


def solve(S0, required, limit, max_temps, cap=12, deadline=None):
    """Минимальный по числу временных набор шагов (до max_temps). None, если не найдено."""
    import time as _t
    best = None
    deadline = deadline or (_t.time() + 10)

    def dfs(S, steps, temps_left):
        nonlocal best
        S2, st, todo = closure(S, required, limit)
        if not todo:
            cand = steps + st
            if best is None or len(cand) < len(best):
                best = cand
            return
        if temps_left == 0 or _t.time() > deadline:
            return
        for w in temp_candidates(S2, todo, limit, cap):
            op, x, y = recipes_for(w, S2, limit)[0]
            dfs(S2 | {w}, steps + st + [(op, x, y, w)], temps_left - 1)
            if best is not None and len(best) <= len(steps) + len(st) + (max_temps - temps_left) + 1:
                return

    for k in range(max_temps + 1):
        dfs(set(S0), [], k)
        if best is not None:
            return best
    return None


def schedule(steps, start_regs, F, n_regs=48, tries=30, seed=0):
    """Раскладывает шаги (op,x,y,v) в регистры так, чтобы в конце reg[r]==F[r] для всех r из F.
    Значение из F пишется прямо в свой регистр; временные — в регистры вне F.
    Нельзя затирать значение, которое ещё нужно как операнд (если это его единственная копия)."""
    import random
    rnd = random.Random(seed)
    final_reg = {v: r for r, v in F.items()}
    for _ in range(tries):
        reg = dict(start_regs)
        remaining = list(steps)
        prog = []
        ok = True
        while remaining:
            order = list(range(len(remaining)))
            rnd.shuffle(order)
            done = False
            for i in order:
                op, x, y, v = remaining[i]
                rx = next((r for r, val in reg.items() if val == x), None)
                ry = next((r for r, val in reg.items() if val == y), None)
                if rx is None or ry is None:
                    continue
                rest = remaining[:i] + remaining[i + 1:]
                needed = {a for _, a, _, _ in rest} | {b for _, _, b, _ in rest}
                def can_overwrite(d):
                    old = reg.get(d)
                    if old is None:
                        return True
                    if old in needed and sum(1 for val in reg.values() if val == old) == 1:
                        return False
                    return True
                if v in final_reg:
                    cands = [final_reg[v]]
                else:
                    cands = [d for d in range(n_regs) if d not in F]
                    rnd.shuffle(cands)
                    cands.sort(key=lambda d: reg.get(d) is not None)
                d = next((d for d in cands if can_overwrite(d)), None)
                if d is None:
                    continue
                prog.append([op, rx, ry, d])
                reg[d] = v
                remaining = rest
                done = True
                break
            if not done:
                ok = False
                break
        if ok and all(reg.get(r) == v for r, v in F.items()):
            return prog
    return None


def resynth_prefix(starts, targets, limit, program, cap=25):
    """Возвращает более короткую программу или None."""
    need = set(targets) - set(starts)
    regs = list(starts) + [None] * (48 - len(starts))
    c = None
    for k, (op, a, b, d) in enumerate(program):
        v = OPF[op](regs[a], regs[b])
        regs_before = regs[:]
        regs[d] = v
        if v in need:
            c = k
            state_c = regs_before
            break
    if c is None or c == 0:
        return None
    # регистры, которые хвост читает до записи
    live_in, written = set(), set()
    for op, a, b, d in program[c:]:
        for r in (a, b):
            if r not in written:
                live_in.add(r)
        written.add(d)
    F = {r: state_c[r] for r in live_in}
    start_regs = {i: v for i, v in enumerate(starts)}
    # регистры, где уже лежит нужное стартовое значение, не трогаем; остальное надо вычислить
    F_comp = {r: v for r, v in F.items() if start_regs.get(r) != v}
    vals = list(F_comp.values())
    if len(vals) != len(set(vals)) or any(v in starts for v in vals):
        return None  # одно значение нужно в двух регистрах или стартовое не на месте: копий нет
    required = set(vals)
    old_len = c
    min_possible = len(required)
    if min_possible >= old_len:
        return None
    steps = solve(set(starts), required, limit, max_temps=min(old_len - 1 - len(required), 5), cap=cap, deadline=__import__("time").time()+20)
    if steps is None or len(steps) >= old_len:
        return None
    pre = schedule(steps, start_regs, F)
    if pre is None:
        return None
    cand = pre + [list(x) for x in program[c:]]
    try:
        run(cand, starts, targets, limit)
    except InvalidProgram:
        return None
    return cand if len(cand) < len(program) else None
