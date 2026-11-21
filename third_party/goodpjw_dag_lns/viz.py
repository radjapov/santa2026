"""Hero dashboard for the notebook: where our instructions were saved, and what a rewrite looks like."""
import math, json, collections
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.lines import Line2D

# reference palette (validated light-mode categorical order) + text/surface tokens
SURF, INK, INK2, MUTED, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#8a8984", "#e6e5e0"
BLUE, ORANGE, AQUA, YELLOW, MAGENTA, GREEN, VIOLET, RED = (
    "#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948")
SEQ = ["#cde2fb", "#9ec5f4", "#6da7ec", "#3987e5", "#256abf", "#184f95", "#0d366b"]

def style():
    plt.rcParams.update({
        "figure.facecolor": SURF, "axes.facecolor": SURF, "savefig.facecolor": SURF,
        "axes.edgecolor": GRID, "axes.labelcolor": INK2, "axes.titlecolor": INK,
        "axes.titleweight": "bold", "axes.titlesize": 12.5, "axes.titlelocation": "left",
        "axes.labelsize": 10, "xtick.color": INK2, "ytick.color": INK2,
        "xtick.labelsize": 9, "ytick.labelsize": 9, "text.color": INK,
        "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.7,
        "axes.spines.top": False, "axes.spines.right": False, "axes.spines.left": False,
        "legend.frameon": False, "legend.fontsize": 9, "font.family": "DejaVu Sans",
        "axes.axisbelow": True, "figure.dpi": 110,
    })

OPF = {'+': lambda a, b: a + b, '-': lambda a, b: a - b, '*': lambda a, b: a * b, '/': lambda a, b: a // b}

def dag_of(p, prog):
    """value -> (op, a, b, step) for every value the program computes and a target depends on."""
    regs = list(p.starts) + [None] * (p.registers - len(p.starts))
    nodes = {v: (None, None, None, -1) for v in p.starts}
    for k, (op, i, j, d) in enumerate(prog):
        a, b = regs[i], regs[j]; v = OPF[op](a, b); regs[d] = v
        nodes.setdefault(v, (op, a, b, k))
    need, st = set(), list(set(p.targets))
    while st:
        x = st.pop()
        if x in need: continue
        need.add(x); op, a, b, _ = nodes[x]
        if op: st += [a, b]
    return {v: nodes[v] for v in need | set(p.starts)}

def layered_positions(dag):
    depth = {}
    for v in sorted(dag, key=lambda v: dag[v][3]):
        op, a, b, _ = dag[v]
        depth[v] = 0 if op is None else 1 + max(depth[a], depth[b])
    by = collections.defaultdict(list)
    for v, d in depth.items(): by[d].append(v)
    pos = {}
    for d, vs in by.items():
        vs.sort(key=lambda v: dag[v][3])
        for i, v in enumerate(vs): pos[v] = (d, (i + 0.5) / len(vs))
    return pos, depth

def draw_dag(ax, p, dag, highlight=frozenset(), title="", hl_color=ORANGE):
    pos, depth = layered_positions(dag)
    T = set(p.targets)
    for v, (op, a, b, _) in dag.items():
        if op is None: continue
        for u in {a, b}:
            x0, y0 = pos[u]; x1, y1 = pos[v]
            new = v in highlight
            ax.plot([x0, x1], [y0, y1], color=hl_color if new else "#c9c8c2", lw=1.1 if new else 0.5,
                    alpha=0.9 if new else 0.55, zorder=1 if not new else 2)
    for v in dag:
        x, y = pos[v]; op = dag[v][0]
        if op is None: c, s, m = INK, 46, "s"
        elif v in T: c, s, m = (hl_color if v in highlight else BLUE), 44, "o"
        else: c, s, m = (hl_color if v in highlight else "#b9b8b2"), 16, "o"
        ax.scatter([x], [y], s=s, c=c, marker=m, zorder=3, edgecolors=SURF, linewidths=1.2)
    ax.set_title(title, fontsize=11)
    ax.set_xticks([]); ax.set_yticks([]); ax.grid(False)
    for sp in ax.spines.values(): sp.set_visible(False)
    ax.set_xlabel("arithmetic depth from the starting values  →", fontsize=9, color=INK2)

def _comma(ax, axis="y"):
    from matplotlib.ticker import FuncFormatter
    f = FuncFormatter(lambda v, _: f"{v:,.0f}")
    (ax.yaxis if axis == "y" else ax.xaxis).set_major_formatter(f)

def dashboard(P, base, ours, fam, ladder, history, public_hist, showcase, out_png=None, score_label="ours", lb=None):
    """P: puzzles; base: public frontier programs; ours: our programs; fam: {pid: family};
    ladder: [(name, total)] public outputs; history: [(minutes, total)] ours;
    public_hist: [(minutes, total)] public frontier; showcase: pid for the rewrite anatomy."""
    style()
    fig = plt.figure(figsize=(17, 17.5))
    gs = fig.add_gridspec(3, 6, height_ratios=[1.0, 1.0, 1.15], hspace=0.42, wspace=0.6, top=0.875, bottom=0.04)
    b_tot = sum(len(base[k]) for k in P); o_tot = sum(len(ours[k]) for k in P)
    gain = {k: len(base[k]) - len(ours[k]) for k in P}
    n_imp = sum(1 for g in gain.values() if g > 0)
    n_t = sum(len(set(p.targets) - set(p.starts)) for p in P.values())
    fig.text(0.125, 0.985, "Santa 2026 \u00b7 Elf Calculator \u2014 a value-DAG search that shortens the public frontier",
             fontsize=18, fontweight="bold", va="top")
    tiles = [(f"{lb if lb else o_tot:,}", "Kaggle LB score (total instructions)" if lb else "total instructions"),
             (f"\u2212{b_tot - o_tot:,}", f"vs best public merge ({b_tot:,})"),
             (f"{n_imp}", "puzzles beat every public program"),
             (f"{o_tot / n_t:.3f}", f"instructions per target ({n_t:,} targets)")]
    for i, (big, small) in enumerate(tiles):
        x = 0.125 + i * 0.205
        fig.text(x, 0.955, big, fontsize=30, fontweight="bold", color=BLUE if i == 1 else INK, va="top")
        fig.text(x, 0.912, small, fontsize=10.5, color=INK2, va="top")

    # score ladder
    ax = fig.add_subplot(gs[0, :3])
    lad = sorted(ladder, key=lambda x: x[1])[:9]
    names = [n for n, _ in lad] + [score_label]; vals = [v for _, v in lad] + [o_tot]
    order = np.argsort(vals)[::-1]; names = [names[i] for i in order]; vals = [vals[i] for i in order]
    cols = [BLUE if n == score_label else "#b9cde6" for n in names]
    y = np.arange(len(vals))
    ax.barh(y, vals, color=cols, height=0.7)
    lo = min(vals) - 0.35 * (max(vals) - min(vals) + 100); hi = max(vals) + 0.22 * (max(vals) - lo)
    ax.set_xlim(lo, hi)
    ax.set_yticks(y); ax.set_yticklabels([n if len(n) < 42 else n[:40] + "\u2026" for n in names], fontsize=8.6)
    for yi, v, n in zip(y, vals, names):
        ax.text(v + 0.01 * (hi - lo), yi, f"{v:,}", va="center", fontsize=8.8,
                color=INK if n == score_label else INK2, fontweight="bold" if n == score_label else None)
    ax.set_title("Every public output we replayed, and ours")
    ax.set_xlabel("total instructions over 1,000 puzzles (lower is better)"); ax.grid(axis="y", visible=False); _comma(ax, "x")

    # progress
    ax = fig.add_subplot(gs[0, 3:])
    if public_hist:
        px = [h[0] for h in public_hist] + [max(h[0] for h in history + public_hist)]
        py = [h[1] for h in public_hist] + [public_hist[-1][1]]
        ax.step(px, py, where="post", color=MUTED, lw=1.6, ls="--")
        ax.text(px[0], py[0], "  best public notebook", va="bottom", ha="left", fontsize=9, color=INK2)
    if history:
        hx = [h[0] for h in history]; hy = [h[1] for h in history]
        ax.step(hx, hy, where="post", color=BLUE, lw=2.2)
        ax.scatter(hx, hy, color=BLUE, s=14, zorder=3)
        ax.text(hx[-1], hy[-1], f" {hy[-1]:,}", va="top", ha="right", fontsize=10, fontweight="bold", color=INK)
    ax.set_title("Score over search time (20 CPU cores)")
    ax.set_xlabel("minutes since we started"); ax.set_ylabel("total instructions"); _comma(ax)

    # savings map
    ax = fig.add_subplot(gs[1, :4])
    ids = sorted(P); W = 50
    grid = np.full((len(ids) // W, W), np.nan)
    for i, k in enumerate(ids): grid[i // W, i % W] = gain[k]
    from matplotlib.colors import ListedColormap, BoundaryNorm
    gmax = max(1, int(np.nanmax(grid)))
    cm = ListedColormap(["#efeeea"] + SEQ[2:][:min(5, gmax)])
    bounds = [-0.5, 0.5] + [x + 0.5 for x in range(1, min(5, gmax) + 1)]
    if gmax > 5: bounds[-1] = gmax + 0.5
    ax.imshow(grid, cmap=cm, norm=BoundaryNorm(bounds, cm.N), aspect="auto", interpolation="nearest")
    ax.set_xticks([]); ax.set_yticks(range(0, len(ids) // W, 5)); ax.set_yticklabels([f"{r * W + 1:04d}" for r in range(0, len(ids) // W, 5)], fontsize=8)
    ax.grid(False)
    ax.set_title("The 1,000 puzzles, one cell each \u2014 colour = instructions we removed")
    hand = [mpatches.Patch(color="#efeeea", label="unchanged")] + [mpatches.Patch(color=cm(i + 1), label=(f"\u2212{i + 1}" if i + 1 < min(5, gmax) or gmax <= 5 else f"\u2212{i + 1}+")) for i in range(min(5, gmax))]
    ax.legend(handles=hand, loc="upper left", bbox_to_anchor=(0, -0.02), ncol=7, fontsize=9)
    ax.set_ylabel("puzzle id (rows of 50)")

    # by family
    ax = fig.add_subplot(gs[1, 4:])
    fams = ["independent", "mixed", "shared", "multiplicative", "tiny"]
    cnt = [sum(1 for k in P if fam[k] == f) for f in fams]
    sv = [sum(gain[k] for k in P if fam[k] == f) for f in fams]
    imp = [sum(1 for k in P if fam[k] == f and gain[k] > 0) for f in fams]
    ax.barh(range(len(fams))[::-1], sv, color=BLUE, height=0.6)
    for i, (v, c, m) in enumerate(zip(sv, cnt, imp)):
        ax.text(v, len(fams) - 1 - i, f"  \u2212{v:,}  ({m}/{c} puzzles)", va="center", fontsize=9, color=INK2)
    ax.set_yticks(range(len(fams))[::-1]); ax.set_yticklabels(fams)
    ax.set_xlim(0, max(sv + [1]) * 1.9)
    ax.set_title("Instructions removed, by puzzle family*"); ax.grid(axis="y", visible=False)

    # rewrite anatomy
    p = P[showcase]; d0, d1 = dag_of(p, base[showcase]), dag_of(p, ours[showcase])
    ax = fig.add_subplot(gs[2, :3])
    rewrite_panel(ax, p, d0, d1, "old", f"{showcase} \u00b7 public program: the part we removed ({len(base[showcase])} instr.)")
    ax = fig.add_subplot(gs[2, 3:])
    rewrite_panel(ax, p, d0, d1, "new", f"{showcase} \u00b7 our replacement ({len(ours[showcase])} instr.)")
    handles = [mpatches.Patch(color="#eeede8", label="value kept from the program"),
               mpatches.Patch(color=BLUE, label="target"),
               mpatches.Patch(color=RED, label="removed instruction (op \u2192 value)"),
               mpatches.Patch(color=AQUA, label="new instruction (op \u2192 value)")]
    ax.legend(handles=handles, loc="upper center", bbox_to_anchor=(-0.1, -0.02), ncol=4, fontsize=9.5)
    fig.text(0.0, 0.005, "*family inferred from practice-set fingerprints (start-value size, target relations). "
             "Every program counted here was replayed by an exact referee.", fontsize=8.5, color=INK2)
    if out_png: fig.savefig(out_png, bbox_inches="tight", dpi=110)
    return fig

def fmt_val(v):
    return f"{v:,}"

def rewrite_panel(ax, p, dag_old, dag_new, which, title):
    """Draw only the part of the program that changed: nodes unique to `which` side, their operands
    (context) and the targets they feed. Each node is labelled with its value and recipe."""
    d = dag_old if which == "old" else dag_new
    other = dag_new if which == "old" else dag_old
    changed = [v for v in d if v not in other]
    T = set(p.targets)
    ctx = set()
    for v in changed:
        op, a, b, _ = d[v]
        if op: ctx.update(x for x in (a, b) if x not in changed)
    consumers = set()
    for v, (op, a, b, _) in d.items():
        if op and (a in changed or b in changed) and v not in changed:
            consumers.add(v); ctx.update(x for x in (a, b) if x not in changed)
    nodes = list(dict.fromkeys(list(ctx) + changed + list(consumers)))
    if not changed:
        ax.text(0.5, 0.5, "same program", ha="center", va="center", color=INK2); ax.axis("off"); ax.set_title(title, fontsize=11.5); return
    # depth inside the sub-graph
    dep = {}
    for v in sorted(nodes, key=lambda v: (d[v][3], v)):
        op, a, b, _ = d[v]
        if (v in ctx and v not in consumers) or op is None: dep[v] = 0
        else: dep[v] = 1 + max(dep.get(a, 0), dep.get(b, 0))
    cols = collections.defaultdict(list)
    for v in nodes: cols[dep[v]].append(v)
    pos = {}
    for c, vs in cols.items():
        vs.sort(key=lambda v: d[v][3])
        for i, v in enumerate(vs): pos[v] = (c, 1 - (i + 0.5) / max(len(vs), 1))
    hl = RED if which == "old" else AQUA
    for v in nodes:
        op, a, b, _ = d[v]
        if not op or (v in ctx and v not in consumers): continue
        for u in {a, b}:
            if u in pos:
                ax.annotate("", xy=pos[v], xytext=pos[u],
                            arrowprops=dict(arrowstyle="-|>", color=hl if v in changed else "#b9b8b2", lw=1.2,
                                            shrinkA=9, shrinkB=9, mutation_scale=9))
    for v in nodes:
        x, y = pos[v]; op, a, b, _ = d[v]
        if v in changed: fc, ec, tc = hl, hl, "white"
        elif v in T: fc, ec, tc = BLUE, BLUE, "white"
        else: fc, ec, tc = "#eeede8", "#c9c8c2", INK2
        label = fmt_val(v)
        if (v in changed or v in consumers) and op: label = f"{op} → {label}"
        ax.text(x, y, label, ha="center", va="center", fontsize=7.6, color=tc, zorder=4,
                bbox=dict(boxstyle="round,pad=0.32", fc=fc, ec=ec, lw=1))
    ax.set_xlim(-0.6, max(dep.values()) + 0.6 if dep else 1); ax.set_ylim(-0.05, 1.05)
    ax.axis("off"); ax.set_title(title, fontsize=11.5)
