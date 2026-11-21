"""Строгий верификатор Santa 2026 — The Elf Calculator.

Правила сверены с вкладкой Data ("Calculator rules") и Evaluation
("Program limits"): 48 регистров, <=4096 инструкций, <=131072 байт UTF-8 в ячейке.
"""
from __future__ import annotations

import json
from dataclasses import dataclass

OPS = ("+", "-", "*", "/")


@dataclass(frozen=True)
class Rules:
    num_regs: int = 48
    # Data: "Initial registers may also be overwritten."
    allow_overwrite_start: bool = True
    # Data: "A target is covered when its value appears in any register, including at the start."
    start_counts_as_target: bool = True
    min_value: int = 1
    max_len: int = 4096
    max_cell_bytes: int = 131072


class InvalidProgram(Exception):
    pass


def run(program, starts, targets, value_limit, rules: Rules = Rules()):
    """Исполняет программу; возвращает число инструкций. Бросает InvalidProgram."""
    if not isinstance(program, list):
        raise InvalidProgram("program is not a list")
    if len(program) > rules.max_len:
        raise InvalidProgram(f"too long: {len(program)}")
    n = rules.num_regs
    if len(starts) > n:
        raise InvalidProgram("too many starts")
    reg: list[int | None] = [None] * n
    for i, v in enumerate(starts):
        reg[i] = int(v)
    need = {int(t) for t in targets}
    if rules.start_counts_as_target:
        need -= {int(v) for v in starts}
    k = len(starts)

    for step, ins in enumerate(program):
        if not (isinstance(ins, list) and len(ins) == 4):
            raise InvalidProgram(f"step {step}: bad instruction {ins!r}")
        op, a, b, d = ins
        if op not in OPS:
            raise InvalidProgram(f"step {step}: bad op {op!r}")
        for r in (a, b, d):
            if type(r) is not int or not 0 <= r < n:  # bool/float отсекаются
                raise InvalidProgram(f"step {step}: bad register {r!r}")
        x, y = reg[a], reg[b]
        if x is None or y is None:
            raise InvalidProgram(f"step {step}: read of undefined register")
        if d < k and not rules.allow_overwrite_start:
            raise InvalidProgram(f"step {step}: overwrite of start register {d}")
        if op == "-" and not x > y:
            raise InvalidProgram(f"step {step}: subtraction {x}-{y} not positive")
        if op == "+":
            v = x + y
        elif op == "-":
            v = x - y
        elif op == "*":
            v = x * y
        else:
            if y == 0 or x % y:
                raise InvalidProgram(f"step {step}: non-exact division {x}/{y}")
            v = x // y
        if not rules.min_value <= v <= value_limit:
            raise InvalidProgram(f"step {step}: value {v} out of [{rules.min_value},{value_limit}]")
        reg[d] = v
        need.discard(v)

    if need:
        raise InvalidProgram(f"uncovered targets: {sorted(need)[:5]}{'...' if len(need) > 5 else ''}")
    return len(program)


def parse_program(s: str, rules: Rules = Rules()):
    nbytes = len(s.encode("utf-8"))
    if nbytes > rules.max_cell_bytes:
        raise InvalidProgram(f"cell too long: {nbytes} bytes")
    try:
        return json.loads(s)
    except json.JSONDecodeError as e:
        raise InvalidProgram(f"bad JSON: {e}") from None
