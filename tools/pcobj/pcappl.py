"""PC mod: mods. The buy-mode Appliances (refrigerators, stoves, grills, microwaves, toaster ovens,
coffee and espresso makers, food processors, juicers, dishwashers, trash compactors, the shops' food
displays and the vacation food stands) for pcobj_convert.py: which ones, what kind, which GameCube
base, and which parts of the PC mesh move (doors, racks) and on which node of the base's skeleton.

The GameCube's appliances are animated objects: a character (skeleton) whose nodes the state
animations move (refrigerator_*: two doors, nodes 1 and 2; stove_*: the oven door, node 1;
microwave_expensive: the door, node 3; counter_dishwasher: the door, node 1, and the rack, node 4),
with effects (flames, smoke, steam) on their states. A converted appliance keeps the base's character,
animations and effects (mod.ini animated = 1) and shows a model whose strips are bound to those nodes
(anim_model): the PC mesh split by the bone each vertex hangs from (its GMDC bone assignment, or the
shape node newer objects hang each moving part from: "l_door", "door_rot", "rack_trans", ...), every
part in the model's own space. The game turns each bound part by how far the base's animation turns
its node from rest, about the PC part's own hinge (mod.ini node = N:x,y,z[:m]; isimsobjectmodel.cpp
ModNodes), so the PC fridge's doors open where its hinges are, as the GameCube's sims open them.

What the GameCube has (behaviour of its own, by its object files' trees): fridges (snack, get
ingredients, cook a meal, serve), stoves and the grill (cook; a cheap stove and a poor cook can start a
fire; repair), the microwave (cooks like a stove, on a counter), the espresso machine (a drink: energy,
fun), the food processor and the blenders (a prep step that improves the food), the dishwasher (a
counter of two tiles: the machine and the tile in front its door needs; washes dishes), the kitchen
trash can (throw away, empty). Every PC appliance gets the GameCube object that does the same, or
the nearest: a toaster oven cooks like the microwave, coffee makers and the hot chocolate maker make
drinks like the espresso machine, the juicer is a blender, the trash compactor a trash can, the
shops' freezers and produce bins and the food stands are food sources (fridges).
"""
import numpy as np

from pcinstall import W_FUNCSORT, W_SUBSORT, W_RATINGS, W_PRICE

F_APPLIANCES = 0x04
# Appliances sub-sort (OBJD word 0x5E)
APPL_SUBSORTS = {1: 'cooking', 2: 'fridge', 4: 'small', 8: 'large', 0x80: 'misc'}

# kind by the object's semi-global (its behaviour family on the PC)
SG_KIND = {
    'fridgeglobals': 'fridge', 'stove_globals': 'stove', 'stovegrill_globals': 'grill',
    'microwaveglobals': 'microwave', 'toasterovenglobals': 'toaster', 'coffeemachineglobals': 'coffee',
    'foodprocessor_globals': 'processor', 'juicerglobals': 'juicer', 'dishwasherglobals': 'dishwasher',
    'trashglobals': 'compactor', 'shoppingrackglobals': 'display', 'foodstandglobals': 'foodstand',
}

# GameCube bases. Fridges by price (food quality and price of a snack are the base's), stoves by the PC
# hunger rating (cooking quality, fire risk: the cheap stove burns most).
APPL_TIERS = {
    # kind: [(highest PC value for this base, base), ...]   value: price (fridges), hunger rating (stoves)
    'fridge': [(999, 'refrigerator_cheap'), (1600, 'refrigerator_modern'), (99999, 'refrigerator_expensive')],
    'stove': [(1, 'stove_cheap'), (5, 'stove_modern'), (10, 'stove_expensive')],
    'grill': [(10, 'stove_bbq')],
    'microwave': [(10, 'microwave_expensive')],
    'toaster': [(10, 'microwave_expensive')],
    'coffee': [(10, 'food_espresso_machine')],
    'processor': [(10, 'food_processor')],
    'juicer': [(10, 'food_blender_expensive')],
    'dishwasher': [(10, 'counter_dishwasher')],
    'compactor': [(10, 'trashcan_interior_stainless')],
    'display': [(99999, 'refrigerator_cheap')],
    'foodstand': [(99999, 'refrigerator_modern')],
}
APPL_BASES = {f'{k}{i}': b for k, tiers in APPL_TIERS.items() for i, (_, b) in enumerate(tiers)}

# The base's moving nodes and which PC part goes on each: (node, role, side) with side the hinge's side
# in model x (-1, +1, 0 any). Roles: 'door', 'rack' (a rack sliding out of an oven or dishwasher).
# Measured in the game (SIMMY_OBJ_NODE_TRACE): the fridges' node 1 is the door hinged at model -x,
# node 2 the one at +x, both swing ~87 degrees out; the others below.
BASE_NODES = {
    # base: [(node, role, hinge side in model x, hinge 'v'ertical / 'h'orizontal at the foot), ...]
    'refrigerator_cheap': [(1, 'door', -1, 'v'), (2, 'door', +1, 'v')],
    'refrigerator_modern': [(1, 'door', -1, 'v'), (2, 'door', +1, 'v')],
    'refrigerator_expensive': [(1, 'door', -1, 'v'), (2, 'door', +1, 'v')],
    'stove_cheap': [(1, 'door', 0, 'h')],
    'stove_modern': [(1, 'door', 0, 'h')],
    'stove_expensive': [(1, 'door', 0, 'h')],
    'microwave_expensive': [(3, 'door', +1, 'v')],
    'counter_dishwasher': [(1, 'door', 0, 'h'), (4, 'rack', 0, '')],
}

# where it sorts in the catalogue (mod folder prefix "pca<k>_")
APPL_SORT = {'fridge': '1', 'display': '1', 'stove': '2', 'grill': '2', 'foodstand': '2', 'microwave': '3',
             'toaster': '3', 'coffee': '4', 'processor': '4', 'juicer': '4', 'dishwasher': '5', 'compactor': '5'}
COUNTERTOP = ('microwave', 'toaster', 'coffee', 'processor', 'juicer')


def appliance_guids(cat):
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_APPLIANCES:
            out.append(g)
    return out


def appl_kind(it):
    """'fridge', 'stove', 'grill', 'microwave', 'toaster', 'coffee', 'processor', 'juicer', 'dishwasher',
    'compactor', 'display', 'foodstand', or '' (unknown)."""
    sg = (it.semiglobal or '').lower()
    for k, v in SG_KIND.items():
        if k == sg or k in sg:
            return v
    name = (it.objname + ' ' + (it.name or '')).lower()
    for word, kind in (('fridge', 'fridge'), ('refrigerator', 'fridge'), ('grill', 'grill'), ('stove', 'stove'),
                       ('microwave', 'microwave'), ('toaster', 'toaster'), ('coffee', 'coffee'),
                       ('espresso', 'coffee'), ('processor', 'processor'), ('juicer', 'juicer'),
                       ('dishwasher', 'dishwasher'), ('trash', 'compactor')):
        if word in name:
            return kind
    return ''


def appl_base(it, kind):
    """(GameCube base object file, the PC value it was picked by)."""
    if kind == 'stove':
        v = max(0, it.o.s(W_RATINGS + 0))      # hunger
    else:
        v = it.price
    for top, base in APPL_TIERS[kind]:
        if v <= top:
            return base, v
    return APPL_TIERS[kind][-1][1], v


def describe(kind):
    return {'fridge': 'refrigerator', 'stove': 'stove / range', 'grill': 'grill', 'microwave': 'microwave',
            'toaster': 'toaster oven (cooks as the microwave)', 'coffee': 'coffee maker (a drink, as the espresso machine)',
            'processor': 'food processor', 'juicer': 'juicer (as a blender)', 'dishwasher': 'dishwasher',
            'compactor': 'trash compactor (as a trash can)', 'display': 'food display (a food source, as a fridge)',
            'foodstand': 'food stand (a food source, as a fridge)'}.get(kind, kind)


# ---------------------------------------------------------------------------------------------------
# Moving parts

ROOT_WORDS = ('master', 'root', 'stove_rot')


def part_role(joint):
    """The role of a PC part by its bone / shape node name: 'door', 'rack', 'lid', 'other' (moves on the
    PC: a carafe, a drawer, a bowl; nothing on the GameCube to move it) or '' (the body: the object's own
    node, "master", "root_trans", ...)."""
    j = (joint or '').lower()
    if not j or any(j.startswith(w) for w in ROOT_WORDS):
        return ''
    if 'access' in j:          # a dishwasher's kick plate: part of the body here
        return ''
    if 'door' in j and 'handle' not in j:
        return 'door'
    if 'rack' in j:
        return 'rack'
    if 'lid' in j:
        return 'lid'
    if j.endswith('_rot') or j.endswith('_trans') or any(w in j for w in ('drawer', 'carafe', 'bowl', 'pitcher')):
        return 'other'
    return ''


def bind_parts(meshes, joints, base):
    """[(mesh, node)]: each mesh's triangles grouped by the base node their PC part goes on (0: the body),
    and {node: (pivot in PC object space, spec, joint)}. spec: '' (the base node's turn as it is), 'm'
    (mirrored in x: a door hinged on the other side), or '+x' / '-z' / ... (the node's angle about that
    axis: the PC door is hinged another way than the base's, e.g. a toaster oven's door at its foot on the
    microwave's side-hinged door node). A door goes on the base's door node hinged on the same side
    (fridges: two doors); parts with no node (lids, carafes) stay with the body."""
    nodes = BASE_NODES.get(base, [])
    # each moving part's extent (PC object space)
    ext = {}
    for m in meshes:
        J = m[6] if len(m) > 6 else None
        if J is None:
            continue
        for j in set(J.tolist()):
            P = m[2][J == j]
            lo, hi = P.min(0), P.max(0)
            if j in ext:
                lo, hi = np.minimum(lo, ext[j][0]), np.maximum(hi, ext[j][1])
            ext[j] = (lo, hi)
    bound = {}       # joint -> (node, spec)
    doors = [(j, o) for j, o in joints.items() if part_role(j) == 'door' and j in ext]
    racks = [(j, o) for j, o in joints.items() if part_role(j) == 'rack' and j in ext]
    door_nodes = [n for n in nodes if n[1] == 'door']
    rack_nodes = [n for n in nodes if n[1] == 'rack']
    used = set()
    for j, o in sorted(doors, key=lambda jo: jo[1][0]):
        lo, hi = ext[j]
        # hinged at its foot (an oven door) or at a side (the pivot at one end of the door's width)
        foot = (o[2] - lo[2]) < 0.25 * max(hi[2] - lo[2], 1e-3) and lo[0] + 0.25 * (hi[0] - lo[0]) < o[0] < hi[0] - 0.25 * (hi[0] - lo[0])
        kind = 'h' if foot else 'v'
        mid = (lo[0] + hi[0]) / 2
        side = 0 if foot else (-1 if o[0] < mid else 1)
        pick = [n for n in door_nodes if n[0] not in used and n[3] == kind and (n[2] in (0, side))]
        spec = ''
        if not pick:
            pick = [n for n in door_nodes if n[0] not in used]
            if pick:
                if kind == pick[0][3]:
                    spec = 'm'              # the same kind of hinge on the other side
                elif kind == 'h':
                    spec = '+x'             # its top goes to the front (model -y)
                else:
                    spec = '+z' if side > 0 else '-z'   # its free edge goes to the front
        if pick:
            bound[j] = (pick[0][0], spec)
            used.add(pick[0][0])
    for (j, o), n in zip(racks, rack_nodes):
        bound[j] = (n[0], '')
    out = []
    for m in meshes:
        J = m[6] if len(m) > 6 else None
        if J is None:
            out.append((m[:6], 0))
            continue
        keys = np.asarray([bound[j][0] if j in bound else 0 for j in J.tolist()])
        T = m[5]
        tri_node = keys[T[:, 0]]
        for node in sorted(set(tri_node.tolist())):
            sel = T[tri_node == node]
            if len(sel):
                out.append(((m[0], m[1], m[2], m[3], m[4], sel), int(node)))
    pivots = {node: (np.asarray(joints[j], np.float64), spec, j) for j, (node, spec) in bound.items()}
    return out, pivots
