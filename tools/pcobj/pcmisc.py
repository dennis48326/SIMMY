"""PC mod: mods. The rest of the PC's Miscellaneous tab (everything but its "Dressers" and "Kids" sub-sorts,
pckids.py): bars, the juice keg and nectar bar, the bubble blower, the bonfire and campfire, trash cans and
the trash chute, the clothing booths, the buffet tables, the ceiling sprinkler; the party decorations
(cakes, the toasting set, balloons, wedding arches), the windmill and the pet furniture; and the objects
left out (cars, shop displays, pet supplies...), for pcobj_convert.py: which ones, what kind, which
GameCube base, where the PC mesh goes over the base.

The PC files these on its Miscellaneous tab (catalogue function 0x40) under the sub-sorts 0x8 "Party",
0x20 "Cars", 0x40 "Pets", 0x80 "Miscellaneous" and none (the windmill, the community trash can).

Each becomes the GameCube object that does the same job (its trees, slots, states and animations; the PC
mesh on the base's body node, as the Hobbies objects, pchobby.py):
- a bar: the GameCube drink bar (Misc: "Pour Drink", "Pour Drinks", "Grab Drink"; the bartender stands
  behind the counter), contemporary or saloon by the PC price;
- the juice keg and the nectar bar: the pineapple keg (Appliances: "Grab Drink", "Drink from Tap");
- the bubble blower: the GameCube bubble blower (Appliances, 3x3);
- the bonfire and the campfire: the GameCube bonfire (Misc, 3x3: sit round it, tell stories, sing);
- an indoor trash can: the GameCube trash can (Misc: "Toss Trash", "Empty Trash"); the community trash
  can: the bottomless one of the GameCube's community lots ("Toss Trash", it never fills); the
  Apartment Life trash chute: the bottomless trash can on a wall (it never fills either);
- a clothing booth ("Try On", "Change Outfit"): the GameCube wardrobe ("Change Clothes");
- the buffet tables ("Grab A Plate"): a fridge (Appliances: a food source, as the food stands, pcappl.py);
- the ceiling sprinkler: the smoke alarm (Electronics: the fire wakes it, the fire fighter comes), on the
  wall's top (the GameCube has no ceiling objects that act).

What has nothing a GameCube sim could do with it but still looks right as a decoration becomes one (the
Decorative path: on the floor or a table, "view"): the cakes (the GameCube has no birthdays or weddings
and no cake to eat), the toasting set, balloons and wedding arches, the windmill (its wind setting does
nothing on the GameCube) and the pet furniture (beds, houses, the cat condo, the scratching post, the
cages and the dog pen: the GameCube has no pets). Left out: cars (no cars to own), shop displays and
counters, the photo booth, the restaurant podiums and hotel desks (no businesses, restaurants or
hotels), the pets' displays, bowls, toys and litter box (no pets; not decorations), and the pirate ship
(playground equipment: the user chose not to convert Entertainment / recreation objects).
"""
import numpy as np

from pcinstall import W_FUNCSORT, W_SUBSORT

F_MISC = 0x40              # PC: the Miscellaneous tab; GameCube: the Misc tab
F_GC_MISC = 0x40
F_GC_APPL = 0x04
F_GC_ELEC = 0x08
F_GC_DECO = 0x20
SUB_DONE = 0x02 | 0x10     # "Dressers" and "Kids": pckids.py
MISC_SUBSORTS = {0: 'none', 2: 'dressers', 8: 'party', 0x10: 'kids', 0x20: 'cars', 0x40: 'pets', 0x80: 'misc'}

SKIP = {
    'car': 'a car to own (Nightlife / Apartment Life): the GameCube has no cars or driving (sims leave by the '
           'carpool and taxi of the lot)',
    'shop': 'a shop display that sells what it shows (Open for Business): the GameCube has no businesses or shops',
    'petdisplay': 'a pet shop\'s display case for a pet on sale (Pets): the GameCube has no pets and no shops',
    'photobooth': 'a photo booth sims pay to have their photo taken in (Nightlife): the GameCube has no such '
                  'feature (no photos, no community lots to put it on)',
    'podium': 'a restaurant host\'s podium (Nightlife / Open for Business): the GameCube has no restaurants',
    'hoteldesk': 'a hotel\'s check-in desk (Bon Voyage): the GameCube has no hotels or holidays',
    'petbowl': 'a pet\'s food bowl (Pets): the GameCube has no pets to feed; an empty bowl on the floor is not a '
               'decoration',
    'pettoy': 'a pet toy (Pets): the GameCube has no pets to play with it; a toy lying on the floor is not a '
              'decoration',
    'litterbox': 'a cat\'s litter box (Pets): the GameCube has no pets',
    'pirateship': 'excluded: playground equipment (Entertainment / recreation: the user chose not to convert '
                  'these)',
}

# kinds made decorations (no interaction a GameCube sim could keep; they still look right as decorations)
DECOR = {'cake', 'toast', 'balloons', 'balloonarch', 'arch', 'windmill', 'petbed', 'pethouse', 'catcondo',
         'scratchpost', 'cage', 'dogpen'}
# decorations that stand on the floor whatever their size
FLOOR_ONLY = {'balloons', 'balloonarch', 'arch', 'windmill', 'petbed', 'pethouse', 'catcondo', 'scratchpost',
              'cage', 'dogpen'}
INIT_DATA = {}

MISC_BASES = {
    'bar': 'drinkbar_contemporary', 'bar1': 'drinkbar_saloon',
    'keg': 'appliance_keg_pineapple', 'bubbles': 'appliance_bubbleblower', 'bonfire': 'bonfire',
    'trashcan': 'trashcan_interior_stainless', 'trash_public': 'trashcan_outdoor_bottomless',
    'booth': 'transition_cas_wardrobe', 'sprinkler': 'electronics_alarm_fire', 'buffet': 'refrigerator_cheap',
}
BAR_TIER = 1800            # PC bars up to this price: the contemporary bar ($2500), dearer: the saloon ($3500)

# the path through the converter: 'hobby' (an animated base, the PC mesh on its body node), 'elec' (the
# Electronics path: the smoke alarm), 'appl' (the Appliances path: a fridge), else a decoration
PATH = {'bar': 'hobby', 'nectar': 'hobby', 'keg': 'hobby', 'bubbles': 'hobby', 'bonfire': 'hobby',
        'campfire': 'hobby', 'trashcan': 'hobby', 'trash_public': 'hobby', 'chute': 'hobby', 'booth': 'hobby',
        'sprinkler': 'elec', 'buffet': 'appl'}
# the kind those paths know it as
PATH_KIND = {'sprinkler': 'alarm_smoke', 'buffet': 'fridge'}

# The bases: their group's tiles (GameCube sub x, y), the GameCube tab, the node the PC mesh goes on, the
# base's own parts kept as props (the drink bar's glasses: its animations put them on the counter as the
# drinks are poured, "Grab Drink" takes them), `row`: the PC row (y) the mesh goes on (the drink bar's
# counter is its front row; the bartender stands in the middle of the back row), `turn`: quarter turns
BASE_INFO = {
    'drinkbar_contemporary': {'tiles': [(x, y) for x in range(3) for y in range(2)], 'tab': F_GC_MISC, 'body': 0,
                              'props': (1, 2, 3, 4, 5, 6, 7), 'row': 1},
    'drinkbar_saloon': {'tiles': [(x, y) for x in range(3) for y in range(2)], 'tab': F_GC_MISC, 'body': 0,
                        'props': (1, 2, 3, 4, 5, 6, 7), 'row': 1},
    'appliance_keg_pineapple': {'tiles': [(0, 0), (1, 0)], 'tab': F_GC_APPL, 'body': 0},
    'appliance_bubbleblower': {'tiles': [(x, y) for x in range(3) for y in range(3)], 'tab': F_GC_APPL, 'body': 0},
    'bonfire': {'tiles': [(x, y) for x in range(3) for y in range(3)], 'tab': F_GC_MISC, 'body': 0},
    'trashcan_interior_stainless': {'tiles': [(0, 0)], 'tab': F_GC_MISC, 'body': 0},
    'trashcan_outdoor_bottomless': {'tiles': [(0, 0)], 'tab': F_GC_MISC, 'body': 0},
    'transition_cas_wardrobe': {'tiles': [(0, 0)], 'tab': F_GC_MISC, 'body': 58, 'also_nodes': [55]},
}
TURN = {}
SHIFT = {}

MISC_SORT = {'bar': '1', 'nectar': '2', 'keg': '2', 'buffet': '2', 'bonfire': '3', 'campfire': '3', 'bubbles': '3',
             'booth': '4', 'trashcan': '5', 'trash_public': '5', 'chute': '5', 'sprinkler': '5', 'cake': '6',
             'toast': '6', 'balloons': '6', 'balloonarch': '7', 'arch': '7', 'windmill': '7', 'petbed': '8',
             'pethouse': '8', 'catcondo': '8', 'scratchpost': '8', 'cage': '8', 'dogpen': '8'}


def misc_guids(cat):
    """Buy-mode Miscellaneous objects not in its "Dressers" or "Kids" sub-sorts (those: pckids.py)."""
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_MISC and not o.w(W_SUBSORT) & SUB_DONE:
            out.append(g)
    return out


def misc_kind(it):
    """The kind: from the object's semi-global and name."""
    sg = (it.semiglobal or '').lower()
    name = it.objname.lower()
    if 'carglobals' in sg or name.startswith('car ownable'):
        return 'car'
    if 'pirate ship' in name:
        return 'pirateship'
    if 'shoppingrackglobals' in sg or name.startswith('rack -') or 'souvenir rack' in name:
        return 'shop'
    if 'displaypetglobals' in sg:
        return 'petdisplay'
    if 'photoboothglobals' in sg:
        return 'photobooth'
    if 'diningglobals' in sg or 'podium' in name:
        return 'podium'
    if 'hoteldeskglobals' in sg:
        return 'hoteldesk'
    if 'petfooddishglobals' in sg:
        return 'petbowl'
    if name.startswith('pet toy'):
        return 'pettoy'
    if 'litterbox' in sg:
        return 'litterbox'
    if 'trashglobals' in sg:
        if 'bonfire' in name:
            return 'bonfire'
        if 'chute' in name:
            return 'chute'
        if 'community' in name:
            return 'trash_public'
        return 'trashcan'
    if 'barglobals' in sg:
        return 'bar'
    if 'nectar bar' in name:
        return 'nectar'
    if 'keg' in name:
        return 'keg'
    if 'bubble blower' in name:
        return 'bubbles'
    if 'campfire' in name:
        return 'campfire'
    if 'bonfire' in name:
        return 'bonfire'
    if 'clothingboothglobals' in sg:
        return 'booth'
    if 'buffetglobals' in sg:
        return 'buffet'
    if 'sprinkler' in name:
        return 'sprinkler'
    if 'cakeglobals' in sg:
        return 'cake'
    if 'toastingset' in sg:
        return 'toast'
    if 'balloonarch' in name.replace(' ', '') or 'ballonarch' in name.replace(' ', ''):
        return 'balloonarch'
    if 'balloon' in name:
        return 'balloons'
    if 'wedding' in name and 'arch' in name:
        return 'arch'
    if 'windmill' in name:
        return 'windmill'
    if 'petbedglobals' in sg:
        return 'pethouse' if 'house' in name else 'petbed'
    if 'cat condo' in name:
        return 'catcondo'
    if 'scratching post' in name:
        return 'scratchpost'
    if 'containedpet' in sg:
        return 'dogpen' if 'dog' in name else 'cage'
    return ''


def path(kind):
    """'hobby', 'elec', 'appl', 'decor' or '' (skipped)."""
    if kind in SKIP or not kind:
        return ''
    return PATH.get(kind, 'decor' if kind in DECOR else '')


def misc_base(it, kind, meshes=None):
    """The GameCube base (a bar by its PC price)."""
    if kind == 'bar':
        return MISC_BASES['bar'] if it.price <= BAR_TIER else MISC_BASES['bar1']
    if kind in ('keg', 'nectar'):
        return MISC_BASES['keg']
    if kind in ('bonfire', 'campfire'):
        return MISC_BASES['bonfire']
    if kind in ('trash_public', 'chute'):
        return MISC_BASES['trash_public']
    return MISC_BASES.get(kind, '')


def kid_base(it, kind, meshes):
    return misc_base(it, kind, meshes)


def describe(kind):
    return {'bar': 'bar (as the drink bar)', 'nectar': 'nectar bar (as the keg)', 'keg': 'juice keg',
            'bubbles': 'bubble blower', 'bonfire': 'bonfire', 'campfire': 'campfire (as the bonfire)',
            'trashcan': 'trash can', 'trash_public': 'community trash can (bottomless)',
            'chute': 'trash chute (a bottomless trash can on a wall)', 'booth': 'clothing booth (as the wardrobe)',
            'buffet': 'buffet table (a food source, as a fridge)', 'sprinkler': 'ceiling sprinkler (as the smoke alarm, '
            'on the wall)', 'cake': 'cake (decoration)', 'toast': 'toasting set (decoration)',
            'balloons': 'balloons (decoration)', 'balloonarch': 'balloon arch (decoration)',
            'arch': 'wedding arch (decoration)', 'windmill': 'windmill (decoration)', 'petbed': 'pet bed (decoration)',
            'pethouse': 'pet house (decoration)', 'catcondo': 'cat condo (decoration)',
            'scratchpost': 'scratching post (decoration)', 'cage': 'pet cage (decoration)',
            'dogpen': 'dog pen (decoration)'}.get(kind, kind)


def model_order(models):
    """The object's own model first (not a popped, burnt-down, boxed or emptied look). The bonfire: its
    burning stage (stage 2; stage 1, the unlit stack 2.3 high, hides the base's flames, which are effects
    at the fire's middle; stage 3 is the ash)."""
    stage2 = [m for m in models if 'stage2' in m.lower()]
    good = stage2 + [m for m in models if not any(k in m.lower() for k in ('popped', 'debris', 'empty', 'box', 'stage',
                                                                             'broken'))]
    return good + [m for m in models if m not in good]


def is_dropped(subset, mat):
    """Not drawn: the PC's masks and helper planes ("..._null"), shadows, mirror reflection planes."""
    s = f'{subset} {mat}'.lower()
    return s.endswith('_null') or '_null ' in s + ' ' or 'reflection' in s


def tab_of(base):
    return BASE_INFO.get(base, {}).get('tab', F_GC_MISC)


def mesh_tiles(meshes):
    """The PC tiles the mesh covers: (count x, count y, first x, first y)."""
    P = np.concatenate([m[2] for m in meshes])
    lo, hi = P.min(0), P.max(0)
    return (max(1, int(np.floor(hi[0] + 0.4)) - int(np.floor(lo[0] + 0.6)) + 1),
            max(1, int(np.floor(hi[1] + 0.4)) - int(np.floor(lo[1] + 0.6)) + 1),
            int(np.floor(lo[0] + 0.6)), int(np.floor(lo[1] + 0.6)))


def turn_quarters(meshes, n):
    """Turned n quarter turns (counter-clockwise from above) about the first tile's middle."""
    n %= 4
    if not n:
        return meshes
    out = []
    for m in meshes:
        P, N = m[2].copy(), m[3].copy()
        for _ in range(n):
            P[:, 0], P[:, 1] = -P[:, 1].copy(), P[:, 0].copy()
            N[:, 0], N[:, 1] = -N[:, 1].copy(), N[:, 0].copy()
        out.append((m[0], m[1], P, N) + tuple(m[4:]))
    return out


def place(meshes, kind, W, H, base=''):
    """The PC mesh over the base's W x H footprint (PC space, the first tile's middle at 0): turned to face
    where the base's sims stand, the tiles it covers centred in the footprint (or on the base's `row`).
    Returns (meshes, (pw, ph) covered, note)."""
    info = BASE_INFO.get(base, {})
    n = TURN.get(kind, info.get('turn', 0))
    meshes = turn_quarters(meshes, n)
    pw, ph, x0, y0 = mesh_tiles(meshes)
    row = info.get('row')
    dx = (W - pw) / 2.0 - x0
    if row is not None and ph == 1:
        dy = row - y0
    elif H == 1:
        dy = 0.0             # one row: the back stays where the PC has it (at a wall: PC -y), a front overhangs
    else:
        dy = (H - ph) / 2.0 - y0
    sx, sy, sz = SHIFT.get(kind, (0.0, 0.0, 0.0))
    d = np.array([dx + sx, dy + sy, sz])
    out = [(m[0], m[1], m[2] + d) + tuple(m[3:]) for m in meshes]
    where = f', on row {row}' if row is not None and ph == 1 else ''
    return out, (pw, ph), (f'{pw}x{ph} tile(s) (mesh) on the base\'s {W}x{H}{where}' +
                           (f', turned {n} quarter(s)' if n else ''))


def pre_place(meshes, kind):
    """Before the Electronics / Appliances paths place it: the ceiling sprinkler goes on the wall's top (its
    back at the wall, PC -y)."""
    if kind != 'sprinkler':
        return meshes
    P = np.concatenate([m[2] for m in meshes])
    d = np.array([-(P[:, 0].min() + P[:, 0].max()) / 2, -0.47 - P[:, 1].min(), min(0.0, 2.92 - P[:, 2].max())])
    return [(m[0], m[1], m[2] + d) + tuple(m[3:]) for m in meshes]


def hide_props(base, kind):
    return list(BASE_INFO.get(base, {}).get('hide_props', ()))


def skill_of(base):
    return ''


# a chute on the wall: the bottomless trash can made a wall object (data 8 |= 0x14, 0xD = 4: placed on a
# wall only, as the Electronics' wall speakers)
INIT = {'chute': '8:|0x14 0xD:0x4'}


def sort_key(kind, base=''):
    """Catalogue order (mod folder prefix pcm<k>_): bars, drinks and the buffet, fires and the bubble
    blower, booths, trash and the sprinkler, party decorations, arches and the windmill, pet furniture."""
    return MISC_SORT.get(kind, '9')
