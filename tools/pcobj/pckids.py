"""PC mod: mods. The buy-mode Storage objects (dressers, armoires, wardrobes, lockers, closets, the coat
rack) and Kids objects (cribs, high chairs, potty chairs, changing tables, toy boxes, teddy bears, the
dollhouse, toddler toys, the activity table, the spring rider, the lemonade stand) and the three
remote-control toys, for pcobj_convert.py: which ones, what kind, which GameCube base, where the PC mesh
goes over the base.

The PC files these on its Miscellaneous tab (catalogue function 0x40): sub-sort 0x2 "Dressers" (storage)
and 0x10 "Kids"; the remote-control toys are on its Electronics tab (Entertainment), left there by the
Electronics category for this one.

The GameCube has no babies, toddlers or children: no crib, high chair, potty chair, changing table, toy
box or children's toys, and no shop to sell lemonade from. Its Misc tab (function 0x40) has the dresser
(transition_cas_dresser_groovy, 2x1) and the wardrobe (transition_cas_wardrobe, 1x1); both offer the same
one interaction, "Change Clothes" (the sim changes, the game opens the outfit planner). A PC dresser,
armoire or locker that does that job on the PC becomes one of these by its width: two tiles wide the
dresser, one tile the wardrobe. A converted one is an animated object (mod.ini animated = 1) with the
base's character, states and trees and the PC mesh on the base's body node, as the Hobbies objects
(pchobby.py). The remote-control toys (play with them for fun, others watch) become the GameCube's game
console (Electronics: "play", "Watch", "CallOver"), converted by the Electronics path (pcelec.py).

The children's toys, toy boxes and the coat rack have nothing a GameCube sim can do with them (the PC
lets only toddlers and children play with the toys; nobody uses the coat rack): they become decorations
(the Decorative path: a floor or table-top sculpture, "view"). The baby and toddler furniture and the
lemonade stand are left out (no GameCube feature to keep).
"""
import numpy as np

from pcinstall import W_FUNCSORT, W_SUBSORT

F_MISC = 0x40              # PC: the Miscellaneous tab; GameCube: the Misc tab
F_GC_MISC = 0x40
F_GC_ELEC = 0x08
F_GC_DECO = 0x20
F_PC_ELEC = 0x08
SUB_STORAGE = 0x02         # PC Miscellaneous sub-sort "Dressers"
SUB_KIDS = 0x10            # PC Miscellaneous sub-sort "Kids"
MISC_SUBSORTS = {2: 'dressers', 8: 'party', 0x10: 'kids', 0x20: 'cars', 0x40: 'pets', 0x80: 'misc'}

# the remote-control toys (PC Electronics, Entertainment), left by the Electronics category for this one
RC_GUIDS = {0xCC948545, 0x33D7144D, 0x73D734BF}

SKIP = {
    'crib': 'a crib: the GameCube has no babies or toddlers (nothing would sleep in it)',
    'highchair': 'a high chair: the GameCube has no toddlers to feed in it',
    'potty': 'a potty chair: the GameCube has no toddlers to potty-train',
    'changing': 'a changing table: the GameCube has no babies or toddlers',
    'lemonade': 'a lemonade stand children run to sell lemonade (Open for Business): the GameCube has no children '
                'and no businesses',
    'closet': 'an Apartment Life closet built into a wall like a door (the sim walks through it): the GameCube has '
              'no in-wall objects (as the secret-door bookcase, Hobbies)',
}

# kinds made decorations (no interaction a GameCube sim could keep): placed as the Decorative path places them
DECOR = {'toybox', 'teddy', 'dollhouse', 'toddlertoy', 'activitytable', 'springrider', 'coatrack'}

KID_BASES = {'dresser': 'transition_cas_dresser_groovy', 'wardrobe': 'transition_cas_wardrobe',
             'rc': 'electronics_game_console'}

# where it sorts in the catalogue (mod folder prefix "pck<k>_")
KID_SORT = {'dresser': '1', 'armoire': '1', 'closet2': '1', 'wardrobe': '2', 'locker': '2', 'coatrack': '3',
            'rc': '4', 'toybox': '5', 'teddy': '6', 'dollhouse': '6', 'toddlertoy': '6', 'activitytable': '7',
            'springrider': '7'}

# The bases: their group's tiles (GameCube sub x, y), the GameCube tab, the node the PC mesh goes on
# (the dresser's model has none: the root; the wardrobe's character has its body (node 55, shown while
# in use, with its doors 56 and 57 swinging open) and a facade (58, shown while idle): the PC mesh on
# both)
BASE_INFO = {
    'transition_cas_dresser_groovy': {'tiles': [(0, 0), (1, 0)], 'tab': F_GC_MISC, 'body': 0, 'turn': 2},
    'transition_cas_wardrobe': {'tiles': [(0, 0)], 'tab': F_GC_MISC, 'body': 58, 'also_nodes': [55]},
}
TURN = {}
SHIFT = {}


def is_rc(cat, g):
    return g in RC_GUIDS


def storage_guids(cat):
    """Buy-mode Storage: the PC Miscellaneous tab's "Dressers" sub-sort."""
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_MISC and o.w(W_SUBSORT) & SUB_STORAGE:
            out.append(g)
    return out


def kids_guids(cat):
    """Buy-mode Kids: the PC Miscellaneous tab's "Kids" sub-sort, and the three remote-control toys."""
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if not o.is_lead():
            continue
        if (o.w(W_FUNCSORT) & F_MISC and o.w(W_SUBSORT) & SUB_KIDS) or g in RC_GUIDS:
            out.append(g)
    return out


def claims(cat):
    """GUIDs this module takes from an earlier category in a combined run (the Electronics category would
    take the remote-control toys first and skip them)."""
    return {g for g in RC_GUIDS if g in cat.by_guid}


def kid_kind(it):
    """The kind: from the object's semi-global and name."""
    sg = (it.semiglobal or '').lower()
    name = it.objname.lower()
    if it.guid in RC_GUIDS or 'remote control' in name:
        return 'rc'
    if 'closet' in name:
        return 'closet2' if '2x2' in name else 'closet'
    if 'locker' in name:
        return 'locker'
    if 'armoire' in name:
        return 'armoire'
    if 'dresser' in name:
        return 'dresser'
    if 'coatrack' in name.replace(' ', ''):
        return 'coatrack'
    if 'crib' in sg:
        return 'crib'
    if 'highchair' in sg:
        return 'highchair'
    if 'potty' in name:
        return 'potty'
    if 'changing table' in name:
        return 'changing'
    if 'lemonade' in name:
        return 'lemonade'
    if 'toybox' in sg:
        return 'toybox'
    if 'stuffedanimal' in sg:
        return 'teddy'
    if 'dollhouse' in sg:
        return 'dollhouse'
    if 'activity table' in name:
        return 'activitytable'
    if 'spring rider' in name or 'bouncytoddler' in sg:
        return 'springrider'
    if name.startswith('toy') or 'toy ' in name:
        return 'toddlertoy'
    return ''


def category_kind(kind):
    return 'storage' if kind in ('dresser', 'armoire', 'wardrobe', 'locker', 'closet', 'closet2', 'coatrack') else 'kids'


def mesh_tiles(meshes):
    """The PC tiles the mesh covers (x, y count from the first tile)."""
    P = np.concatenate([m[2] for m in meshes])
    lo, hi = P.min(0), P.max(0)
    return (max(1, int(np.floor(hi[0] + 0.4)) - int(np.floor(lo[0] + 0.6)) + 1),
            max(1, int(np.floor(hi[1] + 0.4)) - int(np.floor(lo[1] + 0.6)) + 1),
            int(np.floor(lo[0] + 0.6)), int(np.floor(lo[1] + 0.6)))


def kid_base(it, kind, meshes):
    """The GameCube base. A dresser, armoire, locker or closet: by its width (the GameCube's dresser and
    wardrobe do the same, "Change Clothes"): two tiles the 2x1 dresser, one the 1x1 wardrobe."""
    if kind == 'rc':
        return KID_BASES['rc']
    if kind in ('dresser', 'armoire', 'locker', 'closet2'):
        pw, ph, x0, y0 = mesh_tiles(meshes)
        return KID_BASES['dresser'] if pw >= 2 else KID_BASES['wardrobe']
    return ''


def describe(kind):
    return {'dresser': 'dresser', 'armoire': 'armoire', 'wardrobe': 'wardrobe', 'locker': 'gym lockers',
            'closet2': 'walk-in closet (2 tiles)', 'coatrack': 'coat rack (decoration)',
            'rc': 'remote-control toy (as the game console)', 'toybox': 'toy box (decoration)',
            'teddy': 'teddy bear (decoration)', 'dollhouse': 'dollhouse (decoration)',
            'toddlertoy': 'toddler toy (decoration)', 'activitytable': 'activity table (decoration)',
            'springrider': 'spring rider (decoration)'}.get(kind, kind)


def model_order(models):
    """The object's own model first (not a broken / crashed look)."""
    good = [m for m in models if not any(k in m.lower() for k in ('broken', 'crashed', 'liner'))]
    return good + [m for m in models if m not in good]


def is_dropped(subset, mat):
    """Not drawn: the PC's masks and helper planes ("..._null"), mirror reflection planes."""
    s = f'{subset} {mat}'.lower()
    return s.endswith('_null') or '_null ' in s + ' ' or 'reflection' in s


def tab_of(base):
    return BASE_INFO.get(base, {}).get('tab', F_GC_MISC)


def turn_half(meshes):
    """Turned half round about the first tile's middle (PC space): x, y -> -x, -y."""
    out = []
    for m in meshes:
        P, N = m[2].copy(), m[3].copy()
        P[:, :2] = -P[:, :2]
        N[:, :2] = -N[:, :2]
        out.append((m[0], m[1], P, N) + tuple(m[4:]))
    return out


def place(meshes, kind, W, H, base=''):
    """The PC mesh over the base's W x H footprint (PC space, the first tile's middle at 0): turned to face
    where the base's sims stand (the GameCube dresser's front is its model's +y, the PC's -y: half round),
    the tiles it covers centred in the footprint. Returns (meshes, (pw, ph) covered, note)."""
    turned = BASE_INFO.get(base, {}).get('turn', 0) == 2
    if turned:
        meshes = turn_half(meshes)
    pw, ph, x0, y0 = mesh_tiles(meshes)
    # one row deep: the back stays at the wall (the PC's -y), the tiles centred across
    ph = 1 if H == 1 else ph
    dx, dy = (W - pw) / 2.0 - x0, (H - ph) / 2.0 - (y0 if H > 1 else 0)
    sx, sy, sz = SHIFT.get(kind, (0.0, 0.0, 0.0))
    d = np.array([dx + sx, dy + sy, sz])
    out = [(m[0], m[1], m[2] + d) + tuple(m[3:]) for m in meshes]
    return out, (pw, ph), f'{pw} tile(s) wide (mesh) on the base\'s {W}x{H}' + (', turned half round' if turned else '')


def hide_props(base, kind):
    return list(BASE_INFO.get(base, {}).get('hide_props', ()))


def skill_of(base):
    return ''


# decorations that stand on the floor whatever their size (a toy box is not a table ornament)
FLOOR_ONLY = {'toybox', 'dollhouse', 'activitytable', 'springrider', 'coatrack'}
# the table-top decoration base (art_sculpture_wooden_model, data 4 = 0x8C: tables and counters) made to
# stand on the floor too (0x1): a teddy bear or a toddler's toy goes on either
INIT_DATA = {'surface': '4:0x8D'}


def sort_key(kind, base=''):
    """Catalogue order (mod folder prefix pck<k>_): dressers, wardrobes, the coat rack, the remote-control
    toys, toy boxes, toys, the larger toys."""
    if base == KID_BASES['wardrobe']:
        return KID_SORT['wardrobe']
    return KID_SORT.get(kind, '8')

