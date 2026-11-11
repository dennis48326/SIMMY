"""PC mod: mods. The buy-mode Seating objects (dining and living chairs, recliners, sofas and
loveseats, benches, bar stools) and Beds for pcobj_convert.py: which ones, what kind, which GameCube
base, the height of the seat or mattress (read off the mesh) against the one the GameCube's
animations sit or lie at, and the beds' made / slept-in models.

The GameCube's seating: every chair (dining, living, office) runs "seating_chair_sg" (sit; a dining
chair is pulled in to a table to eat: state 1 "in"), every sofa, loveseat and the park bench
"seating_sofa_sg" (a seat per tile: sit, watch TV, nap), the beds "bed_sg" (sleep, relax, make the
bed, cuddle and woohoo in a double; a single bed is a row of three parts, a double three by two with
a side for each sim: right (sub y 1) and left (sub y 0), each drawing its half of the bed and its
state: made, unmade, getting in, sleeping, ...). Its sit animation puts the pelvis 0.67 over the floor
(seat 0.57), its sleep animation 0.55 (mattress 0.46); the converted seat or bed keeps the base's
behaviour and draws the sim at the PC seat's height (mod.ini seat_offset).
"""
import numpy as np

from pcinstall import W_FUNCSORT, W_SUBSORT, W_RATINGS

F_SEATING = 0x01
# Seating sub-sort (OBJD word 0x5E)
SEAT_SUBSORTS = {1: 'dining', 2: 'living', 4: 'sofa', 8: 'bed', 0x10: 'recliner', 0x80: 'misc'}

# GameCube bases (object files). How much comfort sitting (energy sleeping) gives is the base's: its
# object file's trees and constants, not the definition's rating (measured: a converted sofa rated 6 or
# 10 on the 7 base gives the 7 base's comfort). So each kind has bases by tier, picked by the PC
# object's rating (catalogue comfort; a bed's energy), all of the same footprint and parts.
SEAT_TIERS = {
    # kind: [(highest PC rating for this base, base, its GameCube rating), ...]
    'dining': [(4, 'seating_chair_dining_plain', 3), (7, 'seating_chair_dining_modern', 6),
               (10, 'seating_chair_dining_expensive', 10)],     # pulled in to tables (out / in), eat
    'chair': [(7, 'seating_chair_comfy_modern', 6), (10, 'seating_chair_comfy_expensive', 10)],  # armchairs
    'sofa': [(5, 'seating_sofa_worn_3x1', 5), (8, 'seating_sofa_modern_3x1', 7),
             (10, 'seating_sofa_expensive_3x1', 10)],             # 3 seats, nap
    'loveseat': [(8, 'seating_sofa_loveseat_groovy', 7), (10, 'seating_sofa_loveseat_romantic', 10)],  # 2 seats
    'bench': [(10, 'seating_bench_public', 5)],                  # 2 seats
    # beds by the PC energy rating (1..8): parts (0,1) (1,1: draws) (2,1); doubles x 0..2, y 0..1 with
    # (1,1) the right half and (1,0) the left
    'bed_single': [(2, 'bed_single_cot', 4), (8, 'bed_single_groovy', 6)],
    'bed_double': [(3, 'bed_double_groovy', 7), (5, 'bed_double_modern', 8), (8, 'bed_double_japanese', 10)],
}
SEAT_BASES = {f'{k}{i}': b for k, tiers in SEAT_TIERS.items() for i, (_, b, _) in enumerate(tiers)}
KIND_BASE = {'dining': 'dining', 'barstool': 'dining', 'booth': 'dining', 'living': 'chair', 'recliner': 'chair',
             'lounge': 'chair', 'sectional': 'chair', 'sofa': 'sofa', 'loveseat': 'loveseat', 'living2': 'loveseat',
             'bench': 'bench', 'bed_single': 'bed_single', 'bed_double': 'bed_double'}
# where it sorts in the catalogue (mod folder prefix "pcc<k>_")
SEAT_SORT = {'dining': '1', 'booth': '1', 'barstool': '2', 'living': '3', 'living2': '3', 'recliner': '3', 'lounge': '3',
             'sectional': '4', 'sofa': '4', 'loveseat': '4', 'bench': '5', 'bed_single': '6', 'bed_double': '7'}

GC_SEAT = 0.57       # the seat the GameCube's sit animations are made for (its chairs and sofas: 0.56-0.59)
GC_MATTRESS = 0.46   # the mattress its sleep animations lie on (pelvis 0.55)
BED_SHEET = 0.47     # a slept-in PC bed: its blanket's head end lowered to the sheet (see bed_models)
# The GameCube's dining chair slides 0.24 tiles toward the table when pulled in (its "in" state); a
# converted chair's state 1 model is moved as far (measured: the sitting sim's pelvis, out and in).
GC_PULL_IN = 0.24


def seat_offset(height):
    """How much higher a sim is drawn in the seat (mod.ini seat_offset): the PC seat against the
    GameCube's, at most 0.12 lower (the sit animation's feet stay on the floor: a lounge chair's low
    seat would sink the shins) and 0.2 higher (a bar stool: the feet off the floor, on its rest)."""
    return max(-0.12, min(0.2, height - GC_SEAT))


def seating_guids(cat):
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_SEATING:
            out.append(g)
    return out


def seat_kind(it):
    """(kind, skip reason): kinds 'dining', 'barstool', 'booth', 'living', 'living2', 'recliner', 'lounge',
    'sectional', 'sofa', 'loveseat', 'bench', 'bed_single', 'bed_double'."""
    sub = it.subsort
    sg = (it.semiglobal or '').lower()
    obj = it.objname.lower()
    w, h = it.footprint()
    if 'salon' in obj:
        return 'misc', 'makeover station (Open for Business): its use is the makeover, the GameCube has none'
    if obj.startswith('tent') or obj == 'tent':
        return 'misc', 'a tent sims sleep inside (3x3): the GameCube has no such bed'
    if 'hammock' in obj:
        return 'misc', 'a hammock (strung between posts or a tree): the GameCube has no hammock to lie in'
    if 'coffin' in obj:
        return 'misc', 'the vampires\' coffin (Nightlife): sims lie inside a closed coffin; the GameCube has no vampires'
    if 'murphy' in obj:
        return 'misc', 'the Murphy bed folds down from a wall cabinet (2x4): the GameCube\'s beds cannot fold'
    if sub == 8 or 'bedglobals' in sg:
        if (w, h) == (1, 3):
            return 'bed_single', ''
        if (w, h) == (2, 3):
            return 'bed_double', ''
        return 'misc', f'a {w}x{h} bed: the GameCube\'s beds are 1x3 and 2x3'
    if 'sofaglobals' in sg:
        if sub == 0x80 or 'bench' in obj:
            return ('bench', '') if (w, h) == (2, 1) else ('misc', f'a {w}x{h} bench')
        if (w, h) == (3, 1):
            return 'sofa', ''
        if (w, h) == (2, 1):
            return 'loveseat', ''
        return 'misc', f'a {w}x{h} sofa: the GameCube\'s are 3x1 and 2x1'
    if 'booth' in obj:
        return 'booth', ''
    if 'sectional' in obj:
        return 'sectional', ''
    if 'barstool' in obj or 'bar stool' in obj:
        return 'barstool', ''
    if sub == 0x10:
        return ('lounge' if 'lounge' in obj else 'recliner'), ''
    if sub == 2:
        return ('living2', '') if (w, h) == (2, 1) else ('living', '')
    if sub == 1 or 'dining' in obj:
        return 'dining', ''
    if sub == 0x80:
        return 'living', ''
    return 'living', ''


def seat_base(kind, comfort, energy):
    """(GameCube base, its rating) for a PC object of `kind` with these ratings (seats by comfort, beds
    by the PC's energy rating 1..8)."""
    family = KIND_BASE[kind]
    r = energy if family.startswith('bed') else comfort
    for top, base, gc in SEAT_TIERS[family]:
        if r <= top:
            return base, gc
    return SEAT_TIERS[family][-1][1], SEAT_TIERS[family][-1][2]


def describe(kind):
    return {'dining': 'dining chair', 'barstool': 'bar stool', 'booth': 'dining booth', 'living': 'armchair',
            'living2': 'wide armchair (seats two)', 'recliner': 'recliner', 'lounge': 'lounge chair',
            'sectional': 'sectional sofa piece', 'sofa': 'sofa', 'loveseat': 'loveseat', 'bench': 'bench',
            'bed_single': 'single bed', 'bed_double': 'double bed'}.get(kind, kind)


def pick_model(it, kind):
    """The CRES to draw: a sectional's or booth's single piece (arms both sides)."""
    models = [m for m in it.models if m and 'broken' not in m.lower()] or it.models
    if kind in ('sectional', 'booth'):
        single = [m for m in models if 'single' in m.lower()]
        if single:
            return single[0]
    return models[0] if models else None


def ratings(it, kind):
    """(comfort, energy): the PC's catalogue comfort rating (1..10) and a bed's energy rating (1..8; seats
    keep the base's energy). They pick the base's tier (seat_base); mod.ini writes them for the catalogue
    (comfort; a bed's energy mapped onto the GameCube beds' 4..10)."""
    comfort = max(1, min(10, it.o.s(W_RATINGS + 1)))
    energy = -1
    if kind.startswith('bed'):
        energy = max(1, min(8, it.o.s(W_RATINGS + 4)))
    return comfort, energy


# ---------------------------------------------------------------------------------------------------
# Heights

def _levels(meshes, x0, x1, y0, y1, zlo, zhi, subsets=None):
    """{level (0.02 bins): area} of the upward-facing triangles over the box."""
    lv = {}
    for (mat, sub, P, N, U, T) in meshes:
        if not len(T) or (subsets is not None and not subsets(sub, mat)):
            continue
        A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
        n = np.cross(B - A, C - A)
        ln = np.maximum(np.linalg.norm(n, axis=1), 1e-12)
        nn = N[T].mean(1) if len(N) else n
        nz = nn[:, 2] / np.maximum(np.linalg.norm(nn, axis=1), 1e-12)
        ctr = (A + B + C) / 3
        ok = ((np.abs(n[:, 2]) > 0.7 * ln) & (nz > 0.5) & (ctr[:, 0] >= x0) & (ctr[:, 0] <= x1) &
              (ctr[:, 1] >= y0) & (ctr[:, 1] <= y1) & (ctr[:, 2] > zlo) & (ctr[:, 2] < zhi))
        for z, a in zip(ctr[ok, 2], ln[ok] / 2):
            b = int(round(z / 0.02))
            lv[b] = lv.get(b, 0.0) + a
    return lv


def seat_height(meshes, kind, cx=0.0, cy=0.0):
    """The seat's height over the tile at (cx, cy) (PC object space; the front is +y): the upward faces
    in the middle of the seat (between the arms, in front of the back) from 0.2 to 0.6 (a bar stool's to
    1.0: arms and back cushions are higher), the highest level with at least a quarter of the largest
    one's area (a cushion over a platform or frame; levels merged with their neighbours: a cushion's
    slight slope). None if none."""
    hi = 1.0 if kind == 'barstool' else 0.6
    lv = _levels(meshes, cx - 0.22, cx + 0.22, cy - 0.22, cy + 0.25, 0.2, hi)
    if not lv:
        lv = _levels(meshes, cx - 0.35, cx + 0.35, cy - 0.35, cy + 0.35, 0.2, hi)
    if not lv:
        return None
    merged = {b: lv.get(b - 1, 0) + lv[b] + lv.get(b + 1, 0) for b in lv}
    best = max(merged.values())
    cand = [b for b, v in merged.items() if v >= 0.25 * best]
    top = max(cand)
    zs = [b for b in (top - 1, top, top + 1) if b in lv]
    return sum(b * 0.02 * lv[b] for b in zs) / sum(lv[b] for b in zs)


def _is_bedding(sub, mat):
    s = (sub + ' ' + mat).lower()
    return 'bedding' in s or 'blanket' in s or 'sheet' in s or 'duvet' in s or 'quilt' in s


def bed_models(meshes):
    """(made, slept_in) meshes of a PC bed (head at y = 0, its length along +y). Slept in: the covers
    turned down: the blanket's triangles over the head part (up to 42% of its length) are left out and
    a sheet (at BED_SHEET, in the colour of the blanket's folded-sheet edge) and the blanket's cut edge
    put in their place; the pillows (short bedding pieces at the head) stay, the fold's opacity plane
    goes (it belongs to the made bed). The PC animates its blanket instead."""
    beds = [m for m in meshes if _is_bedding(m[1], m[0])]
    if not beds:
        return meshes, meshes
    long_ = lambda m: (m[2][:, 1].max() - m[2][:, 1].min()) > 1.2
    longest = max(beds, key=lambda m: len(m[5]) if long_(m) else -1)
    if not long_(longest):
        return meshes, meshes
    # a second full-length blanket inside the first (the double beds' sheet for the PC's turned-down
    # animation: in its bind pose it shows through the blanket): left out of both
    def inner(m):
        return m is not longest and _is_bedding(m[1], m[0]) and long_(m) and m[2][:, 2].max() < longest[2][:, 2].max() - 0.005
    meshes = [m for m in meshes if not inner(m)]
    made = meshes
    mat, sub, P, N, U, T = longest
    y0, y1 = float(P[:, 1].min()), float(P[:, 1].max())
    x0, x1 = float(P[:, 0].min()), float(P[:, 0].max())
    top = float(P[:, 2].max())
    cut = y0 + 0.42 * (y1 - y0)
    ctr = P[T].mean(1)
    # uvs: the folded sheet (the blanket's top at the head end) and the blanket's top further down
    def uv_of(sel):
        if not sel.any():
            return U[T].mean((0, 1))
        return U[T[sel]].mean((0, 1))
    upper = ctr[:, 2] > top - 0.08
    sheet_uv = uv_of(upper & (ctr[:, 1] < y0 + 0.3))
    blanket_uv = uv_of(upper & (ctr[:, 1] > cut) & (ctr[:, 1] < cut + 0.4))
    out = []
    for m in meshes:
        low = (m[1] + ' ' + m[0]).lower()
        if 'opacityplane' in low or (low.split()[0] == 'unknown' and m[2][:, 1].max() < cut):
            continue
        if _is_bedding(m[1], m[0]) and long_(m):
            c = m[2][m[5]].mean(1)
            keep = c[:, 1] >= cut
            if keep.any():
                out.append((m[0], m[1], m[2], m[3], m[4], m[5][keep]))
            continue
        out.append(m)
    # the sheet over the head part and the blanket's cut edge (both faces of each: the winding of the
    # PC's meshes varies)
    # (in two halves across: a double bed's sides are split at its middle)
    S = BED_SHEET
    xm = (x0 + x1) / 2
    quads = []
    for xa, xb in ((x0, xm), (xm, x1)):
        quads += [((xa, y0, S), (xb, y0, S), (xb, cut, S), (xa, cut, S), (0, 0, 1), sheet_uv),
                  ((xa, cut, S), (xb, cut, S), (xb, cut, top), (xa, cut, top), (0, -1, 0), blanket_uv)]
    Pq, Nq, Uq, Tq = [], [], [], []
    for a_, b_, c_, d_, n, uv in quads:
        k = len(Pq)
        Pq += [a_, b_, c_, d_]
        Nq += [n] * 4
        Uq += [uv] * 4
        Tq += [(k, k + 1, k + 2), (k, k + 2, k + 3), (k, k + 2, k + 1), (k, k + 3, k + 2)]
    out.append((mat, sub, np.array(Pq, np.float64), np.array(Nq, np.float64), np.array(Uq, np.float64),
                np.array(Tq, np.int64)))
    return made, out


def mattress_height(meshes):
    """The made bed's top (blanket) and the frame's mattress, for the inventory."""
    lv = _levels(meshes, -0.3, 0.3, 0.3, 1.7, 0.15, 1.2)
    if not lv:
        return None
    b = max(lv, key=lambda k: lv[k])
    return b * 0.02


def split_halves(meshes, xmid):
    """A double bed's two halves by the triangles' middles (x < xmid, x >= xmid)."""
    a, b = [], []
    for (mat, sub, P, N, U, T) in meshes:
        if not len(T):
            continue
        cx = P[T].mean(1)[:, 0]
        left = cx < xmid
        if left.any():
            a.append((mat, sub, P, N, U, T[left]))
        if (~left).any():
            b.append((mat, sub, P, N, U, T[~left]))
    return a, b
