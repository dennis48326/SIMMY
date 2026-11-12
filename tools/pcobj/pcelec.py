"""PC mod: mods. The buy-mode Electronics objects (TVs, computers, stereos and speakers, the jukebox and
DJ booths, arcade and pinball machines, the video game system, phones, alarms and clocks, vending
machines) for pcobj_convert.py: which ones, what kind, which GameCube base, where the PC mesh goes over
the base, and the screens: a converted TV or computer shows the GameCube base's own screen content.

The GameCube's electronics are animated objects (a character, state animations, effects). A converted
one keeps the base's character, states and effects (mod.ini animated = 1) and shows the PC mesh
(anim_model): its body on the base's body node, a phone's handset on the base's handset node, and its
screen made the way the base makes its own:
  - TVs: the base's screen strip uses the shader "tv_screen_search" (0x0E96A4E8), which each state
    replaces (ObjAnimDef +0x0C / +0x10: the channels, news, static; switched off: a dark screen). The PC
    screen strip takes that shader (its uvs laid over the screen as the base's quad has them), the mod
    keeps the base's shader changes (shader_swaps = 1) and the states that switch it off show the PC
    TV's own screen instead (state_shaders).
  - computers and arcade machines: one quad per screen on a node of its own (blog, chat, design,
    games, job, startup, shutdown, off; an arcade's attract and in-use screens), each with its own
    shader; the state animations show one node and hide the others. The PC screen is copied onto each
    of those nodes with that node's GameCube shader (the base's "off" quad, which is drawn with the
    body's texture: the PC's own screen material).
What the GameCube has (catalogue function 8): TVs (cheap on a stand, normal, plasma 2x1; channels,
watch, turn on/off, repair), computers (cheap, expensive, alien; blog, play, design, find a job, chat),
stereos (boombox, expensive 2x1, jukebox 2x1, DJ booth 3x2; turn on / off, dance, switch station),
arcade machines, pinball machines (1x2), the game console, the wall phone, burglar and fire alarms, the
alarm clock, the drink and hot dog vending machines, the hydraulic massage table (3x2).
"""
import numpy as np

from pcinstall import W_FUNCSORT, W_RATINGS

F_ELECTRONICS = 0x08
# Electronics sub-sort (OBJD word 0x5E)
ELEC_SUBSORTS = {1: 'entertainment', 2: 'tv_computer', 4: 'audio', 8: 'small', 0x80: 'misc', 0: 'none'}

# kinds left out, and why (inventory reason)
SKIP = {
    'rccar': 'a remote-controlled toy (Entertainment): the GameCube has no RC toys (left for the Misc / toys category)',
    'karaoke': 'a 3x3 karaoke stage: the GameCube has no karaoke (no singing); its stereos and DJ booth do not fit it',
    'register': 'a shop\'s cash register (Open for Business): the GameCube has no businesses to run',
    'ticket': 'a shop\'s ticket machine (Open for Business): the GameCube has no businesses to run',
    'kiosk': 'a shop\'s gadget kiosk (sells to customers): the GameCube has no businesses to run',
    'massage': ('a massage table (Hobbies; one sim massages another): the GameCube\'s massage table (Electronics, 3x2) '
                'is a machine whose animated robot arms do the massage, which the PC table has none of (and its trees '
                'stop with TREEASSERT 1101 on the PC footprint): left for later'),
}

ELEC_BASES = {
    'tv': 'electronics_tv_cheap_stand', 'tv1': 'electronics_tv_normal', 'tv2': 'electronics_tv_plasma_1x3',
    'computer': 'electronics_computer_cheap', 'computer1': 'electronics_computer_expensive',
    'stereo': 'electronics_stereo_boombox2', 'stereo2': 'electronics_stereo_expensive',
    'jukebox': 'electronics_stereo_jukebox', 'djbooth': 'electronics_stereo_djbooth',
    'arcade': 'electronics_arcade_regular', 'pinball': 'game_pinball_machine',
    'pinball_ufo': 'game_pinball_machine_ufo', 'pinball_shark': 'game_pinball_machine_shark',
    'console': 'electronics_game_console', 'phone': 'electronics_phone_wall',
    'alarm_burglar': 'electronics_alarm_burglar', 'alarm_smoke': 'electronics_alarm_fire',
    'alarm_clock': 'electronics_alarm_clock', 'clock_wall': 'art_wall_painting_01',
    'vending_drink': 'electronics_vending_drink', 'vending_food': 'electronics_vending_hotdog',
}

# where it sorts in the catalogue (mod folder prefix "pce<k>_")
ELEC_SORT = {'tv': '1', 'tv_wall': '1', 'computer': '2', 'stereo': '3', 'speaker': '3', 'jukebox': '3', 'djbooth': '3',
             'arcade': '4', 'pinball': '4', 'console': '4', 'phone_wall': '5', 'phone_table': '5',
             'phone_booth': '5', 'alarm_burglar': '6', 'alarm_smoke': '6', 'alarm_clock': '6', 'clock_wall': '6',
             'vending_drink': '7', 'vending_food': '7'}

# The bases' groups: their parts' tiles (GameCube sub x, y; a tile listed twice: two parts on it, as the
# one-tile TVs have), the body node (the base's own body's), a phone's handset node; "screens": 'swap'
# (a TV: its screen shader is replaced per state) or 'nodes' (a quad per screen node).
BASE_INFO = {
    'electronics_tv_cheap_stand': {'tiles': [(0, 0), (0, 0)], 'screens': 'swap'},
    'electronics_tv_normal': {'tiles': [(0, 0), (0, 0)], 'screens': 'swap'},
    'electronics_tv_plasma_1x3': {'tiles': [(0, 0), (1, 0)], 'screens': 'swap'},
    'electronics_computer_cheap': {'screens': 'nodes'},
    'electronics_computer_expensive': {'screens': 'nodes'},
    'electronics_arcade_regular': {'screens': 'nodes'},
    'electronics_stereo_expensive': {'tiles': [(0, 0), (1, 0)]},
    'electronics_stereo_jukebox': {'tiles': [(0, 0), (1, 0)]},
    'electronics_stereo_djbooth': {'tiles': [(x, y) for x in range(3) for y in range(2)]},
    'game_pinball_machine': {'tiles': [(0, 0), (0, 1)]},
    'game_pinball_machine_ufo': {'tiles': [(0, 0), (0, 1)]},
    'game_pinball_machine_shark': {'tiles': [(0, 0), (0, 1)]},
    'electronics_phone_wall': {'handset': 1},
}

# Placement changes after the base's init tree (mod.ini init_data) and how the model is drawn
# (wall_model): data 4 the heights it may stand at (1 the floor, 0x8C tables / counters / end tables),
# data 8 bits 0x10 | 0x4 (a wall object: the GameCube's wall phone, alarms, paintings), data 0xD the
# wall it needs (0x4: one behind it).
INIT_DATA = {
    'tv_wall': '8:|0x14 0xD:0x4',               # a floor TV made a wall object: on a wall, not the floor
    'speaker': '4:0x1 8:|0x14 0xD:0x4',          # the boombox made a wall speaker
    'phone_table': '4:0x8C 8:&0xFFEB 0xD:0x0',   # the wall phone on tables, desks and counters
    'phone_booth': '8:&0xFFEB',                  # the wall phone standing on the floor (by a wall)
}
WALL_MODEL = {'phone_table': 0, 'phone_booth': 0}
# the caller stands in front (route_front): the wall phone's caller stands on its tile, a table's here
ROUTE_FRONT = {'phone_table'}
# found by the trees that look for the base's GUID (same_type): the fire notifies the smoke alarms
# (efx_fire "notify fire alarm"), an incoming call rings every phone ("Master - Set Ringing")
SAME_TYPE = {'phone_wall', 'phone_table', 'phone_booth', 'alarm_smoke', 'alarm_burglar', 'alarm_clock'}
# drawn as the GameCube's wall objects: the model's back at the wall (pushed 0.45 tiles toward it)
WALL_SHIFT = {'phone_wall', 'alarm_burglar', 'alarm_smoke', 'clock_wall'}


def electronics_guids(cat):
    """Buy-mode Electronics (catalogue function 8) and the massage table the PC files under Hobbies
    (the GameCube's own massage table is on its Electronics tab)."""
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if not o.is_lead():
            continue
        if o.w(W_FUNCSORT) & F_ELECTRONICS or (cat.semiglobal(o) or '').lower() == 'tablemassageglobals':
            out.append(g)
    return out


def elec_kind(it):
    sg = (it.semiglobal or '').lower()
    name = it.objname.lower()
    if 'remotecontrolcar' in sg:
        return 'rccar'
    if 'videogame' in sg:
        return 'console'
    if 'arcade' in sg:
        return 'arcade'
    if 'pinball' in sg:
        return 'pinball'
    if 'computer' in sg:
        return 'computer'
    if 'television' in sg:
        return 'tv_wall' if 'wall' in name else 'tv'
    if 'stereo' in sg:
        if 'karaoke' in name:
            return 'karaoke'
        if 'jukebox' in name:
            return 'jukebox'
        if 'dj booth' in name:
            return 'djbooth'
        if 'wall' in name or 'community' in name:
            return 'speaker'
        return 'stereo'
    if 'alarm - burglar' in name:
        return 'alarm_burglar'
    if 'alarm - smoke' in name:
        return 'alarm_smoke'
    if 'clock_alarm' in sg:
        return 'alarm_clock'
    if name.startswith('clock'):
        return 'clock_wall'
    if 'phone' in sg:
        return 'phone_booth' if 'booth' in name else 'phone_wall' if 'wall' in name else 'phone_table'
    if 'cashregister' in sg:
        return 'register'
    if 'bandatron' in name:
        return 'ticket'
    if 'vending machine - drink' in name:
        return 'vending_drink'
    if 'vending machine - food' in name:
        return 'vending_food'
    if 'kiosk' in name:
        return 'kiosk'
    if 'tablemassage' in sg:
        return 'massage'
    return ''


def fun(it):
    return max(0, it.o.s(W_RATINGS + 5))


def elec_base(it, kind, tiles):
    """The GameCube base: by function, footprint and (TVs, computers) the PC's fun / price tier."""
    n = len(tiles) or 1
    if kind in ('tv', 'tv_wall'):
        if n > 1:
            return ELEC_BASES['tv2']                 # plasma 2x1 (fun 5)
        return ELEC_BASES['tv'] if fun(it) <= 6 else ELEC_BASES['tv1']   # stand (3) / normal (4)
    if kind == 'computer':
        return ELEC_BASES['computer'] if it.price <= 1500 else ELEC_BASES['computer1']
    if kind == 'stereo':
        return ELEC_BASES['stereo2'] if n > 1 else ELEC_BASES['stereo']
    if kind == 'speaker':
        return ELEC_BASES['stereo']
    if kind == 'pinball':
        nm = it.objname.lower()
        return ELEC_BASES['pinball_ufo'] if 'ufo' in nm else ELEC_BASES['pinball_shark'] if 'shark' in nm else ELEC_BASES['pinball']
    if kind.startswith('phone'):
        return ELEC_BASES['phone']
    return ELEC_BASES.get(kind, '')


def describe(kind):
    return {'tv': 'TV', 'tv_wall': 'wall TV', 'computer': 'computer', 'stereo': 'stereo', 'speaker': 'wall speaker',
            'jukebox': 'jukebox', 'djbooth': 'DJ booth', 'arcade': 'arcade machine', 'pinball': 'pinball machine',
            'console': 'video game system', 'phone_wall': 'wall phone', 'phone_table': 'table phone',
            'phone_booth': 'public phone', 'alarm_burglar': 'burglar alarm', 'alarm_smoke': 'smoke alarm',
            'alarm_clock': 'alarm clock', 'clock_wall': 'wall clock', 'vending_drink': 'drink machine',
            'vending_food': 'snack machine', 'massage': 'massage table'}.get(kind, kind)


def model_order(models):
    """The model to convert first: not a placeholder, broken or empty variant."""
    good = [m for m in models if not any(k in m.lower() for k in ('placeholder', 'broken', 'empty'))]
    return good + [m for m in models if m not in good]


# ---------------------------------------------------------------------------------------------------
# Parts of the PC mesh

def is_screen(subset, mat):
    s, m = (subset or '').lower(), (mat or '').lower()
    return s == 'screen' or m == 'screen' or ('screen' in m and 'screenshot' not in m) or s.endswith('_screen')


def is_dropped(subset, mat):
    """Not drawn here: a vending machine's catalogue thumbnail plane, a helicopter's spin blur."""
    s = f'{subset} {mat}'.lower()
    return 'thumbnail' in s or s.startswith('spin')


def is_handset(subset):
    return 'handset' in (subset or '').lower()


def open_lid(meshes_j, joints):
    """A flat-panel monitor the PC raises by animation (its 'screen_rot' part lies face up in the bind
    pose): that part turned about its joint to stand facing the front, leaning back a little, and lifted
    clear of the desk. Returns (meshes, note) - unchanged when its screen already stands."""
    lid = 'screen_rot'
    if lid not in joints:
        return meshes_j, ''
    scr = [m[2][m[6] == lid] for m in meshes_j if len(m) > 6 and m[6] is not None and is_screen(m[1], m[0]) and (m[6] == lid).any()]
    if not scr or np.ptp(np.concatenate(scr)[:, 2]) > 0.03:
        return meshes_j, ''
    hy, hz = float(joints[lid][1]), float(joints[lid][2])
    th = np.radians(-80.0)               # face up -> facing the front (+y), 10 degrees back
    c, s = np.cos(th), np.sin(th)
    moved = []
    for m in meshes_j:
        if len(m) <= 6 or m[6] is None or not (m[6] == lid).any():
            moved.append(m)
            continue
        sel = m[6] == lid
        P2, N2 = m[2].copy(), m[3].copy()
        y, z = P2[sel, 1] - hy, P2[sel, 2] - hz
        P2[sel, 1], P2[sel, 2] = hy + c * y - s * z, hz + s * y + c * z
        ny, nz = N2[sel, 1].copy(), N2[sel, 2].copy()
        N2[sel, 1], N2[sel, 2] = c * ny - s * nz, s * ny + c * nz
        moved.append((m[0], m[1], P2, N2) + tuple(m[4:]) + (sel,))
    low = min(float(m[2][m[-1]][:, 2].min()) for m in moved if len(m) > 7)
    # on top of the arm the PC raises it on (the 'arm_rot' part, lying on the desk), else clear of the desk
    arm = [m[2][m[6] == 'arm_rot'] for m in meshes_j if len(m) > 6 and m[6] is not None and (m[6] == 'arm_rot').any()]
    rest = float(np.concatenate(arm)[:, 2].max()) if arm else 0.03
    lift = max(0.0, rest - low)
    out = []
    for m in moved:
        if len(m) > 7:
            P2 = m[2].copy()
            P2[m[-1], 2] += lift
            m = (m[0], m[1], P2) + tuple(m[3:7])
        out.append(m)
    return out, 'flat screen stood up (lying face up in the PC bind pose)'


# ---------------------------------------------------------------------------------------------------
# The base's screens

def fit_uv(Pq, Uq):
    """The GameCube quad's uvs as an affine function of where a point is across (x) and up (z, or y for
    a flat quad) its bounding box: coefficients (3x2) for [1, across, up]."""
    lo, hi = Pq.min(0), Pq.max(0)
    up = 2 if (hi[2] - lo[2]) >= (hi[1] - lo[1]) else 1
    a = (Pq[:, 0] - lo[0]) / max(hi[0] - lo[0], 1e-6)
    b = (Pq[:, up] - lo[up]) / max(hi[up] - lo[up], 1e-6)
    A = np.stack([np.ones_like(a), a, b], 1)
    coef, *_ = np.linalg.lstsq(A, Uq, rcond=None)
    return coef, up


def apply_uv(G, coef, up, box=None):
    """uvs for the PC screen's points G (GameCube model space) laid over its bounding box (`box`: (lo, hi)
    of the whole screen when it comes in pieces) as the base's quad has them."""
    lo, hi = box if box is not None else (G.min(0), G.max(0))
    upax = up if (hi[up] - lo[up]) > 1e-4 else (1 if up == 2 else 2)
    a = (G[:, 0] - lo[0]) / max(hi[0] - lo[0], 1e-6)
    b = (G[:, upax] - lo[upax]) / max(hi[upax] - lo[upax], 1e-6)
    return (np.stack([np.ones_like(a), a, b], 1) @ coef).astype(np.float32)


def base_screens(strips, states, mode):
    """The base's screens: [(node, shader, uv coefficients, up axis, is_off)] and its body node.
    'swap' bases: the strip whose shader the states replace; 'nodes' bases: every 4-vertex quad on a node
    of its own (is_off: drawn with the body's shader: the switched-off screen)."""
    if not strips:
        return [], 0
    body = max(strips, key=lambda s: len(s['P']))
    body_node = body['node'] if body['node'] != 0xFFFFFFFF else 0
    slots = {st[3] for st in states if st[3]}
    # the quads of a 'nodes' base drawn with vertex colours (flags 0x4): its screens; one without them
    # among such (or one with the body's shader) is the switched-off screen
    coloured = any(s['flags'] & 4 for s in strips if s['U'] is not None and len(s['P']) == 4 and s['node'] != body_node)
    out = []
    for s in strips:
        if s['U'] is None or len(s['P']) != 4:
            continue
        if (mode == 'swap' and s['shader'] in slots) or (mode == 'nodes' and s['node'] not in (0xFFFFFFFF, body_node)):
            coef, up = fit_uv(s['P'], s['U'])
            node = s['node'] if s['node'] != 0xFFFFFFFF else 0
            off = s['shader'] == body['shader'] or (mode == 'nodes' and coloured and not s['flags'] & 4)
            out.append((node, s['shader'], coef, up, off))
    return out, body_node
