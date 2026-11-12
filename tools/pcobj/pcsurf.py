"""PC mod: mods. The buy-mode Surfaces objects (counters, islands, dining/end/coffee tables, desks,
displays) for pcobj_convert.py: which ones, what kind, which GameCube base, which PC models (a
counter's connecting pieces), and the height of the top things are put on, read off the mesh.

The GameCube's surfaces all share one behaviour per family: every table (dining, end, desk; 1x1 to
4x2) runs the semi-global "table_sg" (eat here, things on it, chairs pulled up), every counter
"food_counter_sg" (prepare food, things on it). Their object slots (things put on them) are the same
pattern at one height (slot height kinds 3 table / 4 counter: 1.0 tile); a converted surface keeps
its base's slots and kinds and gives its own height (mod.ini `surface`, per tile in `tiles`).
"""
import re

import numpy as np

from pcinstall import W_FUNCSORT, W_SUBSORT

F_SURFACES = 0x02
# Surfaces sub-sort (OBJD word 0x5E)
SURF_SUBSORTS = {1: 'counter', 2: 'dining', 4: 'end', 8: 'desk', 0x10: 'coffee', 0x20: 'display', 0x80: 'misc'}

# GameCube bases (object files). Every table is "table_sg", every counter "food_counter_sg": the base
# is picked by footprint so multi-tile groups have the same shape of parts.
SURF_BASES = {
    'counter': 'counter_cheap1',              # 1x1, counter top (sinks drop in), prepare food
    'table1': 'table_end_plastic_1x1',        # 1x1 table: end, coffee, small dining tables
    'table2': 'table_dining_groovy_2x2',      # 2x1 table group (dining, coffee)
    'desk2': 'table_desk_normal_2x1',         # 2x1 desk group
    'table4': 'table_dining_cheap_red_2x2',   # 2x2 table group (and larger square ones)
}
# where it sorts in the catalogue (mod folder prefix "pcs<k>_")
SURF_SORT = {'counter': '1', 'island': '2', 'dining': '3', 'desk': '4', 'coffee': '5', 'end': '6',
             'display': '7', 'misc': '7', 'wallshelf': '8'}


def surface_guids(cat):
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_SURFACES:
            out.append(g)
    return out


def surface_kind(it):
    """'counter', 'island', 'dining', 'end', 'coffee', 'desk', 'display', 'misc' or 'wallshelf'."""
    sub = it.subsort
    sg = (it.semiglobal or '').lower()
    name = (it.objname + ' ' + (it.name or '')).lower()
    if sub == 1 or 'counterglobals' in sg:
        return 'island' if 'island' in it.objname.lower() else 'counter'
    if 'paintingglobals' in sg or 'wall shelf' in name or 'wallbohemian' in name.replace(' ', ''):
        return 'wallshelf'
    if sub == 0x10 and 'tableend' in sg:
        return 'end'
    if sub in SURF_SUBSORTS and sub not in (0x20, 0x80):
        return SURF_SUBSORTS[sub]
    if 'tabledining' in sg:
        return 'dining'
    if sub == 0x20 or 'display' in sg:
        return 'display'
    return 'misc'


def surface_base(kind, w, h):
    """The GameCube base object file for a surface of `kind` and footprint w x h (PC tiles)."""
    if kind in ('counter', 'island'):
        return SURF_BASES['counter']
    n = w * h
    if n == 1:
        return SURF_BASES['table1']
    if kind == 'desk' and n == 2:
        return SURF_BASES['desk2']
    if min(w, h) == 1:
        return SURF_BASES['table2']          # 2x1, 3x1, ...: a row of table tiles
    return SURF_BASES['table4']              # 2x2, 3x3, ...


# ---------------------------------------------------------------------------------------------------
# Models: a counter's pieces

def _norm(n):
    return n.lower().replace('_', '')


def pick_models(it, kind):
    """{'main': model, 'corner': ..., 'end_e': ..., 'end_w': ...} (PC CRES names) of the object; a
    counter's pieces from its STR# model list (the PC's counter globals pick them by neighbours)."""
    models = [m for m in it.models if m]
    real = [m for m in models if not _norm(m).startswith('template')] or models
    out = {}
    if kind not in ('counter', 'island'):
        out['main'] = real[0] if real else None
        return out
    nodoors = 'no doors' in it.objname.lower()
    plain = [m for m in real if not any(k in _norm(m) for k in ('corner', 'east', 'west', 'withsink', 'counteronly'))]
    if nodoors:
        nd = [m for m in plain if 'nodoor' in _norm(m) and 'nodrawer' not in _norm(m)]
        out['main'] = nd[0] if nd else (plain[0] if plain else real[0])
    else:
        out['main'] = plain[0] if plain else real[0]
    for m in real:
        n = _norm(m)
        if 'corner' in n and 'corner' not in out:
            out['corner'] = m
        elif 'east' in n and 'end_e' not in out:
            out['end_e'] = m
        elif 'west' in n and 'end_w' not in out:
            out['end_w'] = m
    return out


def _tri_keys(meshes, digits=3):
    keys = []
    for (mat, sub, P, N, U, T) in meshes:
        R = np.round(P, digits)
        k = [tuple(sorted(map(tuple, R[t]))) for t in T]
        keys.append(k)
    return keys


def mesh_minus(a, b):
    """The triangles of meshes `a` that `b` does not have (same corners), as meshes."""
    have = set()
    for ks in _tri_keys(b):
        have.update(ks)
    out = []
    for (m, ks) in zip(a, _tri_keys(a)):
        mat, sub, P, N, U, T = m
        keep = np.array([k not in have for k in ks], bool)
        if keep.any():
            out.append((mat, sub, P, N, U, T[keep]))
    return out


# ---------------------------------------------------------------------------------------------------
# The top: where things go

KIND_TOP = {'counter': 1.15, 'island': 1.15, 'dining': 0.95, 'end': 0.95, 'coffee': 0.7, 'desk': 0.95,
            'display': 1.6, 'misc': 0.95, 'wallshelf': 1.8}


def surface_height(meshes, cx=0.0, cy=0.0, half=0.5, kind='misc', lo=0.12, hi=2.4):
    """The height of the top surface over the tile at (cx, cy) (PC object space): the levels of the
    upward-facing triangles there with a fair share of the tile (at least 12% of it and 35% of the
    largest level: not a splash back's edge or a drawer's bottom), the highest one a surface of this
    kind can have (a desk's writing top under its hutch), else the highest. None without one."""
    zs, areas = [], []
    for (mat, sub, P, N, U, T) in meshes:
        if not len(T):
            continue
        A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
        n = np.cross(B - A, C - A)
        ln = np.maximum(np.linalg.norm(n, axis=1), 1e-12)
        ar = ln / 2
        # up-facing: the GMDC's own normals (winding varies) or the face normal
        nz = N[T].mean(1)[:, 2] / np.maximum(np.linalg.norm(N[T].mean(1), axis=1), 1e-12) if len(N) else n[:, 2] / ln
        flat = np.abs(n[:, 2]) > 0.95 * ln
        up = flat & (nz > 0.5)
        ctr = (A + B + C) / 3
        inside = (np.abs(ctr[:, 0] - cx) <= half) & (np.abs(ctr[:, 1] - cy) <= half)
        z = ctr[:, 2]
        ok = up & inside & (z > lo) & (z < hi) & (ar > 1e-7)
        zs.append(z[ok]); areas.append(ar[ok])
    if not zs:
        return None
    z = np.concatenate(zs); a = np.concatenate(areas)
    if not len(z) or a.sum() < 0.02:
        return None
    bins = np.round(z / 0.02).astype(int)
    lv = {}
    for b, ar in zip(bins, a):
        lv[b] = lv.get(b, 0.0) + ar
    merged = {b: lv.get(b, 0) + lv.get(b - 1, 0) + lv.get(b + 1, 0) for b in lv}
    best = max(merged.values())
    cand = [b for b, v in merged.items() if v >= max(0.12 * (2 * half) ** 2, 0.35 * best)]
    if not cand:
        return None
    fit = [b for b in cand if b * 0.02 <= KIND_TOP.get(kind, 2.4)]
    top = max(fit) if fit else max(cand)
    sel = np.abs(bins - top) <= 1
    return float(np.average(z[sel], weights=a[sel]))


def end_side(end_meshes, main_meshes):
    """Which side (model x sign, PC object space) an end piece closes: +1 or -1 (0: unknown)."""
    extra = mesh_minus(end_meshes, main_meshes)
    if not extra:
        return 0
    P = np.concatenate([m[2][np.unique(m[5])] for m in extra if len(m[5])]) if any(len(m[5]) for m in extra) else None
    if P is None or not len(P):
        return 0
    x = float(np.mean(P[:, 0]))
    return 1 if x > 0.05 else (-1 if x < -0.05 else 0)


def describe(kind):
    return {'counter': 'counter', 'island': 'kitchen island', 'dining': 'dining table', 'end': 'end table',
            'coffee': 'coffee table', 'desk': 'desk', 'display': 'display', 'misc': 'table',
            'wallshelf': 'wall shelf'}.get(kind, kind)


# ---------------------------------------------------------------------------------------------------
# A counter with a sink in it: its top with a hole the sink's basin drops into

# The hole (PC object space, the counter's tile): inside every PC counter sink's rim (their "..._null"
# cut-out planes: x +-0.30 .. +-0.41, y -0.22 .. 0.29 and wider), and a wide one for the basins of
# x +-0.38 and more and the GameCube's own sinks (rims x +-0.46, y -0.28 .. 0.43 in PC space).
SINK_HOLES = {'sink': (-0.29, -0.21, 0.29, 0.25), 'sink_wide': (-0.37, -0.22, 0.37, 0.27)}
# The GameCube's counter sinks (basin and rim x -0.47..0.47, y -0.28..0.43 here, from the counter top's
# middle): they are as wide as a tile and deeper toward the front than a PC counter top (front edge
# 0.29); in a converted counter one is drawn moved back by the counter's `sink_shift` and stands in a
# hole of its own size (model_sink_gc)
GC_SINK_RIM = (-0.47, -0.28, 0.47, 0.43)


def gc_sink_fit(meshes, top):
    """(shift in PC y, hole rectangle) for a GameCube sink in this counter: the sink moved back so its
    rim's front meets the top's front edge (no further than 0.2), the hole 0.03 inside its rim."""
    ys = []
    for (mat, sub, P, N, U, T) in meshes:
        if not len(T):
            continue
        A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
        n = np.cross(B - A, C - A)
        ln = np.maximum(np.linalg.norm(n, axis=1), 1e-12)
        zc = (A[:, 2] + B[:, 2] + C[:, 2]) / 3
        sel = (np.abs(n[:, 2]) > 0.95 * ln) & (np.abs(zc - top) < 0.03)
        if sel.any():
            ys.append(np.concatenate([A[sel, 1], B[sel, 1], C[sel, 1]]))
    if not ys:
        return 0.0, None
    y = np.concatenate(ys)
    front = float(min(y.max(), 0.5))
    shift = float(np.clip(front - GC_SINK_RIM[3], -0.2, 0.0))
    x0, y0, x1, y1 = GC_SINK_RIM
    return shift, (x0 + 0.03, y0 + shift + 0.03, x1 - 0.03, min(y1 + shift - 0.03, front - 0.01))


def _clip(poly, axis, value, keep_below):
    """Sutherland-Hodgman: the part of `poly` (vertices: (position, normal, uv) arrays) on one side of
    the plane coordinate[axis] = value."""
    out = []
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        da, db = a[0][axis] - value, b[0][axis] - value
        ina, inb = (da <= 1e-9) if keep_below else (da >= -1e-9), (db <= 1e-9) if keep_below else (db >= -1e-9)
        if ina:
            out.append(a)
        if ina != inb:
            t = da / (da - db)
            out.append(tuple(a[k] + (b[k] - a[k]) * t for k in range(3)))
    return out


def cut_hole(meshes, top, rect, depth=0.3, band=0.0):
    """The meshes with the counter's flat (horizontal) triangles from `top` down to `depth` under it (its
    top, the top's underside, the cabinet's top board) cut by `rect` (x0, y0, x1, y1): a hole there
    for the basin (each triangle clipped to the four pieces around the rectangle; the uvs and normals
    carried along); everything else as it is. With `band`, every triangle wholly within `band` under the
    top is cut too, upright ones as well (a curved apron or rail under the top that a deep basin
    would cross). None when no top triangle was cut."""
    x0, y0, x1, y1 = rect
    out, cut = [], False
    for (mat, sub, P, N, U, T) in meshes:
        if not len(T):
            out.append((mat, sub, P, N, U, T))
            continue
        A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
        n = np.cross(B - A, C - A)
        ln = np.maximum(np.linalg.norm(n, axis=1), 1e-12)
        zc = (A[:, 2] + B[:, 2] + C[:, 2]) / 3
        flat = (np.abs(n[:, 2]) > 0.95 * ln) & (zc < top + 0.03) & (zc > top - depth)
        lo = np.minimum(np.minimum(A, B), C); hi = np.maximum(np.maximum(A, B), C)
        if band:
            flat |= (lo[:, 2] > top - band) & (hi[:, 2] < top + 0.03)
        touch = flat & (lo[:, 0] < x1) & (hi[:, 0] > x0) & (lo[:, 1] < y1) & (hi[:, 1] > y0)
        if not touch.any():
            out.append((mat, sub, P, N, U, T))
            continue
        cut = True
        newP, newN, newU, newT = [P], [N], [U], [T[~touch]]
        base = len(P)
        for t in T[touch]:
            tri = [(P[i].astype(np.float64), N[i].astype(np.float64), U[i].astype(np.float64)) for i in t]
            # the four pieces outside the rectangle: x <= x0; x >= x1; x0..x1 and y <= y0; x0..x1 and y >= y1
            pieces = [_clip(tri, 0, x0, True), _clip(tri, 0, x1, False)]
            mid = _clip(_clip(tri, 0, x0, False), 0, x1, True)
            if mid:
                pieces += [_clip(mid, 1, y0, True), _clip(mid, 1, y1, False)]
            for poly in pieces:
                if len(poly) < 3:
                    continue
                k0 = base
                for v in poly:
                    newP.append(v[0][None].astype(P.dtype)); newN.append(v[1][None].astype(N.dtype)); newU.append(v[2][None].astype(U.dtype))
                    base += 1
                for k in range(1, len(poly) - 1):
                    newT.append(np.asarray([[k0, k0 + k, k0 + k + 1]], T.dtype))
        out.append((mat, sub, np.concatenate(newP), np.concatenate(newN), np.concatenate(newU), np.concatenate(newT)))
    return out if cut else None
