"""PC mod: mods. The buy-mode Lighting objects (table, floor, wall and ceiling lamps, outdoor lights,
streetlights) for pcobj_convert.py: which ones, where they go, which GameCube lamp they copy, which
parts of the PC lamp show when it is on or off, and its light (place, colour, reach).

The GameCube's lamps (semi-global lamp_sg: "Turn On", "Turn Off", lights that switch on by themselves
at night in rooms without daylight, a bulb that burns out and is fixed, the room's light score) are
animated objects: a character whose state animations (0 off, 1 on) show one of two copies of the shade
(nodes "..._off" / "..._on": the lit one drawn without lighting, i.e. glowing) and move and switch a point
light that the model carries (SimsLightInfo, bound to the character's "Omni" node; its colour and falloff
are the model's). A converted lamp keeps its base's character, animations and trees (mod.ini animated =
1) and shows a model whose strips are bound to those nodes (anim_model): the PC lamp's parts that look
the same on and off on the root node, the parts the PC switches between a lit and an unlit material (its
MMAT object states: "..._lit" / "..._unlit") twice, lit on the on node, unlit on the off node, and the
parts the PC only shows lit (glows, the pools of light it paints on the floor or the wall, flames,
beams) on the on node only. Glowing materials (additive, emissive, unlit on the PC) are drawn without
lighting, as the GameCube's own lit shades. The model's light is the PC lamp's: at its light node's
place, in its colour, with a falloff from its intensity (mod.ini light = x,y,z:r,g,b:falloff).

Where it goes: table lamps on tables and counters (the GameCube table lamp), floor lamps on the floor,
wall lamps on walls, outdoor lights outside only (the GameCube's street lamp); the GameCube has no
ceiling lamps: a ceiling lamp is a floor lamp of zero extent (sims walk under it, tables and chairs may
stand under it) that may only be placed inside (data 0x1E), hanging at the PC model's height.
"""
import numpy as np

import rcol
from pcinstall import W_FUNCSORT, W_SUBSORT, W_ROOMSORT

F_LIGHTING = 0x80
LIGHT_SUBSORTS = {1: 'table', 2: 'floor', 4: 'wall', 8: 'ceiling', 0x10: 'outdoor', 0x80: 'misc'}

# GameCube bases by where the lamp goes. Every GameCube lamp of one placement runs the same init tree
# (table: "SG_INIT table expensive", floor: "floor expensive tall", wall: "wall cheap": the same light for
# the room score and the same behaviour), so one per placement; the ones whose character has an off and
# an on shade node and a light node.
LIGHT_BASES = {
    'table': 'lamp_table_modern',      # nodes: 1 off, 2 on, 3 light (Omni01)
    'floor': 'lamp_floor_modern',      # 1 on, 2 off, 3 light
    'wall': 'lamp_wall_sconce',        # 1 off, 2 on, 3 light; on walls
    'ceiling': 'lamp_floor_modern',    # a floor lamp of zero extent, inside only (init_data below)
    'outdoor': 'scenery_lamppost',     # the street lamp: a floor lamp, outside only (its data 0x1E 1)
}
# (off node, on node) of each base's character
BASE_NODES = {
    'lamp_table_modern': (1, 2),
    'lamp_floor_modern': (2, 1),
    'lamp_wall_sconce': (1, 2),
    'scenery_lamppost': (1, 2),
}
# object data after the init tree: a ceiling lamp has zero extent (data 8 bit 2: people pass, other
# objects stand on its tile), no layer (data 0x3F) and may only be placed inside (data 0x1E 2)
INIT_DATA = {'ceiling': '8:|0x4 0x3F:0 0x1E:2'}
LIGHT_SORT = {'table': '1', 'floor': '2', 'wall': '3', 'ceiling': '4', 'outdoor': '5'}
# a light's reach: the GameCube lamps' falloff (3-5 tiles) scaled by the PC light's intensity (about
# 1.25 on average), within these bounds
FALLOFF_RANGE = (2.5, 6.0)
PC_INTENSITY = 1.25
# the top of the walls (GameCube wall models: 3 tiles high), where a ceiling lamp hangs from; the highest
# an indoor light goes
WALL_TOP = 3.0
CEILING = 2.95

EFFECT_WORDS = ('glow', 'lightpattern', 'flame', 'beam', 'smoke', 'halo', 'flare', 'lightcone', 'volumelight',
                'intlight', 'fire', 'rays')


def lighting_guids(cat):
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_LIGHTING:
            out.append(g)
    return out


def light_kind(it, P=None):
    """'table', 'floor', 'wall', 'ceiling' or 'outdoor' from the PC sub-sort (and the mesh)."""
    sub = it.subsort
    if sub == 1:
        return 'table'
    if sub == 2:
        return 'floor'
    if sub == 4:
        return 'wall'
    if sub == 8:
        return 'ceiling'
    name = (it.objname + ' ' + (it.name or '')).lower()
    if P is not None:
        lo, hi = P.min(0), P.max(0)
        # hangs on a fence post or a wall (fence lamps, a deck post lantern): its mesh starts well above the
        # ground and reaches to the tile's edge
        if lo[2] > 0.4 and lo[1] < -0.3 and hi[1] < 0.1:
            return 'wall'
        if lo[2] > 1.7:
            return 'ceiling'
    outside = bool(it.roomsort & 0x10) and not (it.roomsort & ~0x10 & 0xFF)
    if sub == 0x10 or outside or 'street' in name:
        return 'outdoor'
    return 'floor'


def describe(kind):
    return {'table': 'a table lamp (on tables and counters)', 'floor': 'a floor lamp', 'wall': 'a wall lamp',
            'ceiling': 'a ceiling lamp (inside, sims walk under it)', 'outdoor': 'an outdoor light (outside only)'}[kind]


# ---------------------------------------------------------------------------------------------------
# Materials: what shows when

def txmt(inst, name):
    d, e = inst.find(rcol.T_TXMT, name.split('!')[-1])
    if d is None:
        return None
    try:
        return rcol.parse_txmt(d)
    except Exception:
        return {'type': '?', 'props': {}}


def _is_null(tm):
    return tm is None or tm['type'].lower() == 'null' or tm['props'].get('stdMatFillMode', 'solid') == 'none'


def _swap(mat, a, b):
    """mat with the last token a replaced by b (lightingx_shade_pink_lit -> ..._unlit), or None."""
    low = mat.lower()
    k = low.rfind(a)
    if k < 0:
        return None
    end = k + len(a)
    if end < len(low) and low[end] not in '_-':
        return None
    if k > 0 and low[k - 1] not in '_-':
        return None
    return mat[:k] + b + mat[end:]


def material_role(inst, subset, mat):
    """(role, lit material, unlit material): 'body' (the same on and off: mat), 'switch' (lit when on,
    unlit when off), 'on' (shown only when on: a glow, the light it paints)."""
    low = (mat or '').lower()
    lit = unlit = None
    for a, b in (('unlit', 'lit'), ('off', 'on')):
        if _swap(low, a, 'x') is not None:
            unlit = mat
            cand = _swap(mat, a, b)
            lit = cand if cand and txmt(inst, cand) is not None else None
            break
    if unlit is None:
        for a, b in (('lit', 'unlit'), ('on', 'off')):
            if _swap(low, a, 'x') is not None:
                lit = mat
                cand = _swap(mat, a, b)
                unlit = cand if cand and txmt(inst, cand) is not None else None
                break
    effect = any(w in (subset or '').lower() or w in low for w in EFFECT_WORDS)
    if lit and unlit:
        if _is_null(txmt(inst, unlit)):
            return 'on', lit, None
        if _is_null(txmt(inst, lit)):
            return 'off', None, unlit
        return 'switch', lit, unlit
    if lit:
        return ('on' if effect or 'lightpattern' in low else 'body'), lit, None
    if unlit:
        return 'body', unlit, unlit
    return ('on' if effect else 'body'), mat, mat


def glows(inst, subset, mat):
    """Whether the PC draws this (lit) material glowing: additive, emissive, or without lighting; then the
    GameCube draws it without lighting too (as its lit shades)."""
    tm = txmt(inst, mat)
    if tm is None:
        return False
    p = tm['props']
    if 'additive' in tm['type'].lower() or p.get('stdMatAlphaBlendMode', 'none').lower() == 'additive':
        return True
    if str(p.get('stdMatLightingEnabled', '1')).strip() in ('0', 'false'):
        return True
    try:
        em = sum(float(x) for x in str(p.get('stdMatEmissiveCoef', '0,0,0')).split(',')[:3])
    except ValueError:
        em = 0.0
    return em >= 0.3 or any(w in (subset or '').lower() for w in ('bulb', 'neon', 'glow', 'flame'))


def effect_alpha(inst, mat, subset=''):
    """An additive light effect (a glow, a light cone, a beam, a pool of light: the PC adds it to what is
    behind) is blended on the GameCube: its texture's alpha scaled by this (1: as it is)."""
    tm = txmt(inst, mat)
    if tm is None:
        return 1.0
    p = tm['props']
    additive = 'additive' in tm['type'].lower() or p.get('stdMatAlphaBlendMode', 'none').lower() == 'additive'
    effect = any(w in (subset or '').lower() or w in mat.lower() for w in EFFECT_WORDS + ('pattern',))
    return 0.6 if additive and effect else 1.0


def glow_color(inst, mat, subset=''):
    """The vertex colour (RGBA bytes) a glowing material is drawn with (no lighting: colour x texture):
    a textured one in its PC diffuse colour plus its emissive one (a light pool decal's warm tint), an
    untextured one white (its colour is in its texture already). An additive light effect (a glow, a
    light cone, a beam: the PC adds it to what is behind) is blended here: drawn at about half its
    strength (effect_alpha, in its texture)."""
    tm = txmt(inst, mat)
    p = tm['props'] if tm else {}
    alpha = 255
    if not p.get('stdMatBaseTextureName') or str(p.get('stdMatBaseTextureEnabled', 'true')).lower() == 'false':
        return (255, 255, 255, alpha)
    def vec(k, d):
        try:
            v = [float(x) for x in str(p.get(k, d)).split(',')[:3]]
            return v if len(v) == 3 else [float(x) for x in d.split(',')]
        except ValueError:
            return [float(x) for x in d.split(',')]
    c = np.clip(np.array(vec('stdMatDiffCoef', '1,1,1')) + np.array(vec('stdMatEmissiveCoef', '0,0,0')), 0.0, 1.0)
    if c.max() < 0.35:
        c = c / max(c.max(), 1e-3) * 0.35
    return tuple(int(round(x * 255)) for x in c) + (alpha,)


# ---------------------------------------------------------------------------------------------------
# The light

def pc_lights(inst, cres_name):
    """[(place (PC model space), {'intensity', 'color', 'falloff', 'range'}, kind)] of the model's lights."""
    d, e = inst.find(rcol.T_CRES, cres_name)
    if d is None:
        return []
    links, _, _ = rcol.rcol_header(d)
    out = []
    try:
        found = rcol.cres_lights(d)
    except Exception:
        return []
    for k, kind, place in found:
        t, g, i, ih = links[k]
        pk, le = inst.find_tgi(t, i, ih)
        if not le or place is None or kind not in ('point', 'spot'):
            continue
        prm = rcol.parse_light(pk.read(le), kind)
        if prm:
            out.append((np.asarray(place, np.float64), prm, kind))
    return out


def lamp_light(lights, meshes, kind):
    """(place in PC model space, colour, PC intensity, notes) of the converted lamp's one light: the PC
    lights' middle (a recoloured neon's lights share a place: the default's colour, the first), or the
    middle of its glowing parts."""
    notes = []
    good = [(p, prm) for p, prm, k in lights if max(prm['color']) > 0.02]
    if good:
        P = np.mean([p for p, prm in good], 0)
        prm = good[0][1]
        col = np.clip(np.asarray(prm['color'], np.float64), 0, 1)
        inten = max(prm['intensity'] for p, prm in good) or PC_INTENSITY
        if len(good) > 1:
            same = all(np.allclose(p, good[0][0], atol=0.05) for p, prm in good)
            notes.append(f'{len(good)} PC lights -> 1' + (' (one per recolour)' if same else ' (their middle)'))
    else:
        allP = np.concatenate([m[2] for m in meshes]) if meshes else np.zeros((1, 3))
        lo, hi = allP.min(0), allP.max(0)
        P = np.array([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2] + 0.8 * (hi[2] - lo[2])])
        col = np.array([1.0, 0.94, 0.75])
        inten = PC_INTENSITY
        notes.append('no PC light: the base\'s colour, at the top of the lamp')
    if kind != 'outdoor' and P[2] > CEILING:
        P[2] = CEILING - 0.1
    m = float(col.max())
    if m > 0:
        col = col / m   # the brightest channel full: the GameCube's light colours are as bright
    return P, col, float(inten), notes


def falloff(base_falloff, intensity):
    f = base_falloff * float(np.sqrt(max(intensity, 0.05) / PC_INTENSITY))
    return float(np.clip(f, *FALLOFF_RANGE))


def light_record(base_rec, node, col, fo):
    """A SimsLightInfo (0x1C bytes) like the base model's: its type and flags, the base's light node and
    intensity, the PC colour and falloff."""
    import struct
    rec = bytearray(base_rec)
    struct.pack_into('>I', rec, 4, node)
    struct.pack_into('>3f', rec, 8, *[float(c) for c in col])
    struct.pack_into('>f', rec, 0x18, float(fo))
    return bytes(rec)
