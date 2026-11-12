"""PC mod: mods. The buy-mode Plumbing objects (toilets, the toilet stall, showers, bathtubs, shower /
tub combinations, pedestal and counter sinks, hot tubs) for pcobj_convert.py: which ones, what kind,
which GameCube base, how the PC mesh is placed over the base so the GameCube's animations line up, and
the PC's clean / dirty / clogged looks for the base's states.

The GameCube's plumbing objects are animated objects (a character, state animations, effects: a
toilet's seat and flusher, its dirt and clog; a bathtub's water and scum; a shower's door; the water
and steam effects). A converted one keeps the base's character, states and effects (mod.ini
animated = 1) and shows the PC mesh bound to the base's nodes (anim_model): its body on the root, a
toilet's seat (and lid) on the base's seat node about the PC hinge, a bathtub's water on the base's
water node (shown while the tub is filled). The PC's dirty materials (MMAT object states: "..._clean"
/ "..._dirty", a toilet's "toiletwater_water_clogged") make the models of the base's dirty and clogged
states (mod.ini state_models).

What the GameCube has: toilets (cheap; porcelain with comfort), the toilet stall (two tiles), showers
(normal, sonic, hi-tech), bathtubs (regular, oriental, ornate: two tiles), pedestal and counter sinks
(wash hands, wash dishes; a counter sink sits in a counter, which shows its cut-out top), hot tubs
(heart-shaped 2x2, Japanese 2x3 with break / repair, the anti-gravity spa 3x3 with modes). The PC's
shower / tub combinations take a bathtub (their footprint and a bath); the PC hot tubs (3x3) the
Japanese hot tub (2x3: the PC mesh is narrowed to its width), raised ones drawing their sims at the
PC water (seat_offset). Not converted: the urinal (the GameCube's toilet trees sit every sim down for
number 2 and women always) and the Bon Voyage hot spring (6x6: no GameCube hot tub comes near).
"""
import numpy as np

from pcinstall import W_FUNCSORT, W_RATINGS

F_PLUMBING = 0x10
# Plumbing sub-sort (OBJD word 0x5E)
PLUMB_SUBSORTS = {1: 'toilet', 2: 'shower_tub', 4: 'sink', 8: 'hottub', 0: 'none'}

# Pie-menu / behaviour families by the PC object's semi-global (and name)
SG_KIND = {'toiletglobals': 'toilet', 'sinkglobals': 'sink', 'tubandshowerglobals': 'shower',
           'hottubglobals': 'hottub'}

# GameCube bases. Toilets by the PC comfort rating (the porcelain toilet gives comfort, clogs less),
# showers / tubs / sinks by the PC hygiene rating (the bases' hygiene gain grows with their price).
PLUMB_TIERS = {
    # kind: [(highest PC value for this base, base), ...]
    'toilet': [(4, 'plumbing_toilet_cheap'), (10, 'plumbing_toilet_porcelain')],
    'stall': [(10, 'plumbing_toilet_stall')],
    'shower': [(9, 'plumbing_shower_normal'), (10, 'plumbing_shower_hitech')],
    'tub': [(5, 'plumbing_bathtub_regular'), (7, 'plumbing_bathtub_oriental_2x1'), (10, 'plumbing_bathtub_ornate_2x1')],
    'combo': [(5, 'plumbing_bathtub_regular'), (7, 'plumbing_bathtub_oriental_2x1'), (10, 'plumbing_bathtub_ornate_2x1')],
    'sink': [(5, 'plumbing_sink_floor_porcelain'), (10, 'plumbing_sink_floor_ornate')],
    'sink_counter': [(5, 'plumbing_sink_counter_modern'), (10, 'plumbing_sink_counter_double_basin')],
    'hottub': [(10, 'plumbing_hottub_japanese')],
}
PLUMB_BASES = {f'{k}{i}': b for k, tiers in PLUMB_TIERS.items() for i, (_, b) in enumerate(tiers)}

# where it sorts in the catalogue (mod folder prefix "pcp<k>_")
PLUMB_SORT = {'toilet': '1', 'stall': '1', 'shower': '2', 'combo': '3', 'tub': '3', 'sink': '4', 'sink_counter': '4',
              'hottub': '5'}

# The base's footprint (GameCube tiles: x across, y along) and per-state looks:
#   seat: the node a toilet's seat hangs on; water: the node of a bathtub's water;
#   dirty / clogged: the states that show dirt / a clog (Sub - Set Graphic By State of each family)
BASE_INFO = {
    'plumbing_toilet_cheap': {'seat': 3, 'dirty': (2, 3), 'clogged': (4, 5), 'seat_h': 0.42},
    'plumbing_toilet_porcelain': {'seat': 1, 'dirty': (2, 3), 'clogged': (4, 5), 'seat_h': 0.42},
    'plumbing_toilet_stall': {'fp': (1, 2), 'seat_h': 0.42},
    'plumbing_shower_normal': {'dirty': (3, 4, 5)},
    'plumbing_shower_hitech': {'dirty': (3, 4, 5)},
    'plumbing_bathtub_regular': {'fp': (1, 2), 'water': 2, 'dirty': tuple(range(16, 34))},
    'plumbing_bathtub_oriental_2x1': {'fp': (1, 2), 'water': 3, 'dirty': tuple(range(16, 34))},
    'plumbing_bathtub_ornate_2x1': {'fp': (1, 2), 'water': 2, 'dirty': tuple(range(16, 34))},
    'plumbing_sink_floor_porcelain': {},
    'plumbing_sink_floor_ornate': {},
    'plumbing_sink_counter_modern': {},
    'plumbing_sink_counter_double_basin': {},
    'plumbing_hottub_japanese': {'fp': (2, 3), 'water_z': 0.0},
}

SKIP = {
    'urinal': 'a wall urinal: the GameCube\'s toilet trees sit every sim down (women always, men for number 2)',
    'hotspring': 'a 6x6 natural hot spring: no GameCube hot tub comes near its size (largest 3x3)',
}


def plumbing_guids(cat):
    """Buy-mode plumbing (catalogue function 0x10) and the hot tubs the PC files elsewhere (the Love Tub:
    an aspiration reward; the Bon Voyage hot spring: Hobbies); the GameCube keeps every hot tub on its
    Plumbing tab."""
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if not o.is_lead():
            continue
        if o.w(W_FUNCSORT) & F_PLUMBING or 'hottub' in (cat.semiglobal(o) or '').lower():
            out.append(g)
    return out


def plumb_kind(it):
    """'toilet', 'stall', 'urinal', 'shower', 'tub', 'combo', 'sink', 'sink_counter', 'hottub',
    'hotspring' or '' (unknown)."""
    sg = (it.semiglobal or '').lower()
    name = it.objname.lower()
    kind = ''
    for k, v in SG_KIND.items():
        if k in sg:
            kind = v
    if not kind:
        for word, k in (('urinal', 'urinal'), ('toilet', 'toilet'), ('sink', 'sink'), ('shower', 'shower'),
                        ('tub', 'shower'), ('hot tub', 'hottub')):
            if word in name:
                kind = k
                break
    if 'urinal' in name:
        return 'urinal'
    if kind == 'toilet' and 'stall' in name:
        return 'stall'
    if kind == 'sink':
        return 'sink_counter' if 'counter' in name else 'sink'
    if kind == 'shower':
        if name.startswith('showertub'):
            return 'combo'
        if name.startswith('tub') or 'bathtub' in name:
            return 'tub'
        return 'shower'
    if kind == 'hottub':
        return 'hotspring' if 'spring' in name else 'hottub'
    return kind


def ratings(it):
    """PC comfort, hygiene (OBJD motive ratings)."""
    return max(0, it.o.s(W_RATINGS + 1)), max(0, it.o.s(W_RATINGS + 2))


def plumb_base(it, kind):
    """(GameCube base object file, the PC value it was picked by)."""
    comfort, hygiene = ratings(it)
    v = comfort if kind == 'toilet' else hygiene
    for top, base in PLUMB_TIERS[kind]:
        if v <= top:
            return base, v
    return PLUMB_TIERS[kind][-1][1], v


def describe(kind):
    return {'toilet': 'toilet', 'stall': 'toilet stall', 'shower': 'shower', 'tub': 'bathtub',
            'combo': 'shower / tub (a bathtub here)', 'sink': 'sink (pedestal)', 'sink_counter': 'counter sink',
            'hottub': 'hot tub'}.get(kind, kind)


# ---------------------------------------------------------------------------------------------------
# Placement: the PC mesh over the base

def seat_height(meshes):
    """A toilet's seat: the top of the up-facing ring around the bowl (PC object space), or None."""
    best = None
    for m in meshes:
        P, N = m[2], m[3]
        n = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
        sel = (n[:, 2] > 0.7) & (np.abs(P[:, 0]) < 0.22) & (P[:, 1] > -0.25) & (P[:, 1] < 0.3) & (P[:, 2] > 0.25) & (P[:, 2] < 0.62)
        if sel.any():
            z = float(np.percentile(P[sel, 2], 90))
            best = z if best is None else max(best, z)
    return best


def water_level(meshes):
    """A hot tub's water surface (the PC's water subset), or None."""
    zs = [float(np.median(m[2][:, 2])) for m in meshes if 'water' in (m[1] or '').lower() or 'water' in (m[0] or '').lower()]
    return max(zs) if zs else None


def is_water(mat, subset):
    s = f'{subset} {mat}'.lower()
    return ('soapywater' in s or 'foamedge' in s or 'waterlevel' in s or 'soapring' in s or 'ringosuds' in s or
            (subset or '').lower() == 'unknown')


def is_seat(joint):
    j = (joint or '').lower()
    return j.startswith('seat')


# ---------------------------------------------------------------------------------------------------
# Clean / dirty / clogged materials

def swap_material(inst, mat, find, repl, exists):
    """mat with `find` replaced by `repl` when such a material exists, else mat."""
    low = mat.lower()
    if find not in low:
        return mat
    alt = low.replace(find, repl)
    return alt if exists(alt) else mat
