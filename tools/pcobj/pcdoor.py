#!/usr/bin/env python3
"""PC mod: mods. Convert doors and archways (one and two tiles wide) from the user's own The Sims 2 (PC)
install (base game and every expansion and stuff pack found) into a SIMMY mod folder: build mode's
Doors tab then lists them next to the GameCube's own.

    pcdoor.py --pc "<The Sims 2 install>" --gc <GameCube disc image or data folder> --out <mods>
              [--packs base,ep1,...]
    pcdoor.py --pc "<install>" --inventory doors.tsv          list, no output

(pcobj_convert.py --categories ...,doors runs this too.)

PC: a door is an object (OBJD type 8, build-mode type 8: the Doors tab) of two parts per tile of its
width, one on each side of the wall (sub index x, y: y 0 the north side), whose model (CRES) has the
frame and the leaves on their own nodes (r_door_*, l_door_*; archways have no leaves) and whose wall
masks (TXMT "<model>_<x>_0_0_n_wallmask", a 64 x 128 alpha picture of one wall tile, 3 m high) cut the
hole into the wall; the far side uses the same picture mirrored. Diagonal versions (build-mode type 0)
are what the PC puts on diagonal walls, not catalogue entries.

GameCube: a door is the same kind of object (a group: a lead and a part per tile and side, type 8; the
wall takes the definition's style 3 and shows the wall models of its row of g_wallTable: the wall with
the door's hole). The 21 one-tile doors open one leaf (node 1, hinged at model +x, turned 90 degrees by
the state animation); the alien airlock (three tiles, its middle column the portal) slides two leaves.

A converted door is a copy of a GameCube door (mod.ini door = 1): one-tile doors copy a GameCube door
of the same kind (by name and price: the door a lot shows without the mod) with door_plain's skeleton and
animations (anim_base), the PC leaf bound to node 1 and turned about its own hinge (mirrored when the PC
hinge is on the other side); two-tile doors copy the airlock's middle and right columns (base_tiles),
its two sliding nodes turning the PC's two leaves 90 degrees (node = N:x,y,z:o<deg>/<slide>). Each part
gets the wall models of its side (wall_cut: the GameCube's nine plain wall end-cap variants with the PC
mask's hole cut out; the caps copied as they are). Gates (in fences on the PC), garage doors and the
two-storey doors are left out (the GameCube has no doors in fences, no cars and one storey).

Groups -> folders: pcd1_doors, pcd2_archways (one tile), pcd3_doubledoors, pcd4_doublearchways (two tiles):
mod.ini ([object] sections), models_*.res, shaders_*, textures_*, preview.png (for the user).
<out>/pcdoor_inventory.tsv lists every door found, converted or skipped (and why).

Never put the output in the source tree or share it: it is made of EA's PC game data, converted on the
user's machine from the user's own copy. See docs/mods.md.
"""
import argparse, glob, os, re, shutil, struct, sys, time

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gcdata, gctex, pcinstall, pcmesh, rcol, sg  # noqa: E402

T_TXMT = 0x49596978
GROUPS = ['doors', 'archways', 'doubledoors', 'doublearchways']
AIRLOCK = 'door_alien_airlock1_double'
ANIM_SINGLE = 'door_plain'          # the one-tile doors' skeleton and animations
GC_HINGE_X = 0.417                  # door_plain's node 1 (its leaf's hinge) at model +x
AIRLOCK_SLIDE = 0.2                 # how far the airlock's leaf nodes slide (fully open)
AIRLOCK_NODES = {'right': 1, 'left': 2}   # node 1 at model +1.41, node 2 at -1.41
# The GameCube's plain wall, its nine end-cap variants (g_wallTable row 0, eroom.cpp)
PLAIN_WALLS = [0xCAFD5303, 0xB1F46D33, 0x6F5F6D70, 0xD3C50619, 0xA187AB4B, 0x03343AF9, 0x3143E96E, 0x89E1197C,
               0xC2341803]
SH_LOW, SH_FACE, SH_LOWTOP = 0xC26D593A, 0x2B7EF272, 0x0906522F   # the wall's lower band, face, band top
WALL_H, LOW_H = 3.0, 0.3
# GameCube one-tile doors a converted door copies (and a lot falls back to): by words in its name
GC_BY_WORD = [
    (('shoji', 'fusuma', 'bamboo', 'asian', 'emperor', 'sesame'), 'door_shoji_screen'),
    (('swing', 'saloon', 'western', 'country', 'hatch', 'barn'), 'door_western_plain'),
    (('security', 'metal', 'tech', 'futur', 'atomic', 'steel', 'yacas', 'werknothom'), 'door_metal_smallwindow'),
    (('glass', 'mullion', 'transom', 'french', 'beveled', 'pane', 'storefront', 'legit'), 'door_glasspane'),
    (('moroccan', 'medieval', 'lionheart', 'nautical', 'ship'), 'door_nauti_plain'),
    (('deco', 'empire', 'arch-duke', 'helier', 'luxur', 'plush', 'elegan', 'ornate', 'storybook', 'eloquence'),
     'door_fancy_detail'),
    (('restroom', 'men\'s', 'women\'s', 'teen', 'stay out', 'dorm', 'myne'), 'door_painted_plain'),
]
INV_COLUMNS = ['pack', 'name', 'pc_guid', 'group', 'width', 'price', 'status', 'reason', 'gc_base', 'model', 'leaves',
               'masks', 'hole', 'textures', 'folder', 'desc']


def log_print(*a):
    print(*a, flush=True)


def clean_text(s):
    s = str(s or '').replace('\r', ' ').replace('\n', ' ').strip()
    return re.sub(r'\s+', ' ', s)


class Door:
    def __init__(self, o, cat):
        self.guid = o.guid
        self.objd = o
        self.home = cat.home(o.guid)
        name, desc = cat.catalog_text(o)
        self.name = clean_text(name or o.name)
        self.desc = clean_text(desc)
        self.objname = o.name
        self.price = o.w(pcinstall.W_PRICE)
        self.buildtype = o.w(pcinstall.W_BUILDTYPE)
        self.parts = cat.parts(o)
        subs = sorted({(p.sub >> 8, p.sub & 0xFF) for p in self.parts})
        self.width = 1 + max([s[0] for s in subs] or [0])
        self.depth = 1 + max([s[1] for s in subs] or [0])
        self.tiles = {}
        for p in self.parts:
            self.tiles.setdefault((p.sub >> 8, p.sub & 0xFF), p.guid)
        self.models = cat.models(o)
        self.status = self.reason = self.folder = ''
        self.group = ''
        self.gc_base = ''
        self.leaves = []
        self.masks = ''
        self.hole = ''
        self.textures = []
        self.out = {}
        self.thumb = None
        self.ini = ''


def inventory(cat):
    doors = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.w(pcinstall.W_TYPE) != 8 or not o.is_lead() or o.w(pcinstall.W_BUILDTYPE) == 0:
            continue
        doors.append(Door(o, cat))
    doors.sort(key=lambda d: (d.home.order, d.name.lower(), d.guid))
    return doors


def skip_reason(d):
    low = (d.objname + ' ' + d.name).lower()
    if 'template' in low:
        return 'a template (not for sale)'
    if d.buildtype == 1 or 'garage' in low:
        return 'garage door (no cars on the GameCube)'
    if low.startswith('gate') or 'gate -' in low or d.buildtype == 4:
        return 'gate: goes into a fence on the PC (the GameCube has no doors in fences)'
    if d.depth > 2 or d.width > 2:
        return f'{d.width} x {d.depth // 2} tiles, two storeys (the GameCube has one storey)'
    if d.depth != 2:
        return f'unexpected footprint {d.width}x{d.depth}'
    if not d.models:
        return 'no model'
    return ''


def gc_single_base(d):
    low = (d.name + ' ' + d.objname).lower()
    for words, base in GC_BY_WORD:
        if any(w in low for w in words):
            return base
    if d.price >= 300:
        return 'door_expensive_plain'
    if d.price >= 180:
        return 'door_inlay'
    return 'door_plain'


# ---------------------------------------------------------------------------------------------------
# GameCube wall models: split into strips (the caps are copied as stored), the face cut with the hole

def modl_split(b):
    """(header pieces, scale, node, [(shader, raw strip bytes)], tail) of a one-sub-model MODL."""
    ver, sig, var, name, off, size = gcdata.data_header(b)
    d = b[off:off + size]
    be = lambda o: struct.unpack_from('>I', d, o)[0]
    p = 0x35
    n = be(p); p += 4 + n * 0x40
    n = be(p); p += 4
    for _ in range(n):
        if be(p) != 0: p += 4; continue
        nx, ny, nz, cnt = struct.unpack_from('>4I', d, p + 0x88)
        p += 0x98 + (cnt * nx * ny * nz * 12 if cnt > 1 else 0)
    for psize in (0x50, 0x60):
        n = be(p); p += 4
        for _ in range(n): p += 4 if be(p) != 0 else 0x4C + be(p + 0x48) * psize
    n = be(p); p += 4 + n * 0x1C
    head = d[:p + 5]                      # ... the byte and the scale
    scale = struct.unpack_from('>f', d, p + 1)[0]
    nsub = be(p + 5); p += 9
    assert nsub == 1, 'wall model with several sub-models'
    node = be(p); ns = be(p + 4); p += 8
    strips = []
    for _ in range(ns):
        start = p
        flags = be(p); sh = be(p + 4); nstr = be(p + 8); p += 12 + nstr + 4
        while True:
            tag = d[p]; p += 1
            if tag == 0:
                vc = be(p); p += 4
                fixed = flags & 0x10
                p += vc * (8 if fixed else 16)
                if flags & 2: p += vc * (4 if flags & 0x40 else 2) * (2 if fixed else 4)
                if flags & 4: p += vc * 4
                if flags & 8: p += vc * (4 if ver >= 0x3A else 3)
                if flags & 0x20:
                    dls = be(p + 5); p += 13 + dls
            elif tag == 1:
                p += 3
            elif tag == 6:
                break
        strips.append((sh, bytes(d[start:p])))
    return (ver, sig, var, name), head, scale, node, strips, bytes(d[p:])


def float_strip(sid, P, U, N, T, scale):
    """One indexed float strip (vertex format 7), as pcobj_convert.make_modl writes them."""
    body = bytearray(struct.pack('>III', 0x2A, sid, 1) + b'\0' + struct.pack('>f', 1.0))
    body += b'\0' + struct.pack('>I', len(P))
    body += np.concatenate([P / scale, np.zeros((len(P), 1))], 1).astype('>f4').tobytes()
    body += U.astype('>f4').tobytes()
    nn = np.clip(np.round(N * 127), -127, 127).astype(np.int8)
    body += np.concatenate([nn, np.full((len(N), 1), 127, np.int8)], 1).tobytes()
    idx = T.reshape(-1).astype(np.uint16)
    dl = bytearray([0x97]) + struct.pack('>H', len(idx))
    dl += np.repeat(idx[:, None], 3, 1).astype('>u2').tobytes()
    dl += b'\0' * ((-len(dl)) % 32)
    body += struct.pack('>IBII', len(idx), 3, len(dl), 3) + dl
    body += b'\x06'
    return bytes(body)


def rects_minus(x0, x1, z0, z1, holes):
    """The rectangle [x0, x1] x [z0, z1] less the hole rectangles, as rectangles on a common grid."""
    xs = sorted({x0, x1} | {v for h in holes for v in (h[0], h[1]) if x0 < v < x1})
    zs = sorted({z0, z1} | {v for h in holes for v in (h[2], h[3]) if z0 < v < z1})
    out = []
    prev = []      # spans of the row below: [(xa, xb, za)] open
    for k in range(len(zs) - 1):
        za, zb = zs[k], zs[k + 1]
        zm = (za + zb) / 2
        spans = []
        cur = None
        for i in range(len(xs) - 1):
            xm = (xs[i] + xs[i + 1]) / 2
            solid = not any(h[0] <= xm <= h[1] and h[2] <= zm <= h[3] for h in holes)
            if solid:
                cur = (cur[0], xs[i + 1]) if cur else (xs[i], xs[i + 1])
            elif cur:
                spans.append(cur); cur = None
        if cur:
            spans.append(cur)
        nxt = []
        for s in spans:
            m = next((q for q in prev if q[0] == s[0] and q[1] == s[1]), None)
            if m:
                prev.remove(m)
                nxt.append(m)
            else:
                nxt.append((s[0], s[1], za))
        for q in prev:                 # closed below this row
            out.append((q[0], q[1], q[2], za))
        prev = nxt
    for q in prev:
        out.append((q[0], q[1], q[2], z1))
    return out


def quads(rects, plane, xlo, xhi, normal):
    """Rectangles on the wall face (plane 'face': y = -0.45; 'top': z = 0.3, y -0.55..-0.45) as
    (P, U, N, T); the face's uvs as the GameCube's: u = z / 3, v = (xhi - x) / (xhi - xlo)."""
    P, U, T = [], [], []
    for (a, b, c, d) in rects:
        k = len(P)
        if plane == 'face':
            P += [(a, -0.45, c), (b, -0.45, c), (b, -0.45, d), (a, -0.45, d)]
            U += [(z / WALL_H, (xhi - x) / (xhi - xlo)) for (x, _, z) in P[k:]]
        else:   # the band's top: a (x range) by (y range) rectangle at z 0.3
            P += [(a, c, LOW_H), (b, c, LOW_H), (b, d, LOW_H), (a, d, LOW_H)]
            U += [(0.0 if y > -0.5 else 0.1, (xhi - x) / (xhi - xlo)) for (x, y, _) in P[k:]]
        T += [(k, k + 1, k + 2), (k, k + 2, k + 3)]
    P = np.array(P, np.float64).reshape(-1, 3)
    T = np.array(T, np.int64).reshape(-1, 3)
    n = np.array(normal, np.float64)
    if len(T):   # front faces counter-clockwise seen from the normal's side
        cr = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
        flip = cr @ n < 0
        T[flip] = T[flip][:, ::-1]
    return P, np.array(U, np.float64).reshape(-1, 2), np.tile(n, (len(P), 1)), T


class WallCutter:
    def __init__(self, gc):
        self.plain = [modl_split(gc.get('models', mid)) for mid in PLAIN_WALLS]

    def model(self, variant, holes, name):
        """The plain wall's end-cap variant with `holes` ([(x0, x1, z0, z1)] in its own space) cut out."""
        (ver, sig, var, _), head, scale, node, strips, tail = self.plain[variant]
        # the variant's extent along the wall (extends past the tile edge where walls meet)
        xlo, xhi = PLAIN_EXTENTS[variant]
        keep = [raw for sh, raw in strips if sh not in (SH_LOW, SH_FACE, SH_LOWTOP)]
        made = []
        face = rects_minus(xlo, xhi, LOW_H, WALL_H, holes)
        low = rects_minus(xlo, xhi, 0.0, LOW_H, holes)
        # the band's top: the solid stretches of x at the band's height, the wall's depth
        top = [(a, b, -0.55, -0.45) for (a, b, c, d) in rects_minus(xlo, xhi, LOW_H - 1e-4, LOW_H + 1e-4, holes)]
        for sid, rects, plane, nrm in ((SH_LOW, low, 'face', (0, 1, 0)), (SH_FACE, face, 'face', (0, 1, 0)),
                                       (SH_LOWTOP, top, 'top', (0, 0, 1))):
            if not rects:
                continue
            P, U, N, T = quads(rects, plane, xlo, xhi, nrm)
            made.append(float_strip(sid, P, U, N, T, scale))
        body = head + struct.pack('>I', 1) + struct.pack('>II', node, len(keep) + len(made)) + b''.join(keep) + \
            b''.join(made) + tail
        return gcdata.make_header(ver, sig, var, name, body)


# x extents of the nine plain variants (none / both / right / left extended, caps): measured on the disc
PLAIN_EXTENTS = [(-0.5, 0.5), (-0.55, 0.55), (-0.55, 0.5), (-0.5, 0.55), (-0.5, 0.5), (-0.5, 0.5), (-0.5, 0.5),
                 (-0.55, 0.5), (-0.5, 0.55)]


def mask_holes(img):
    """Hole rectangles [(u0, u1, v0, v1)] (pixel edges) of a wall mask (alpha 0 = hole), rows merged."""
    a = np.asarray(img)
    hole = (a[..., 3] if a.ndim == 3 else a) < 128
    H, W = hole.shape
    rects, prev = [], {}
    for v in range(H + 1):
        runs = set()
        if v < H:
            row = hole[v]
            u = 0
            while u < W:
                if row[u]:
                    s = u
                    while u < W and row[u]:
                        u += 1
                    runs.add((s, u))
                else:
                    u += 1
        nxt = {}
        for r in runs:
            nxt[r] = prev.pop(r, v)
        for r, v0 in prev.items():
            rects.append((r[0], r[1], v0, v))
        prev = nxt
    return rects, W, H


# ---------------------------------------------------------------------------------------------------

class Converter:
    def __init__(self, pc, gc_path, log=log_print, texsize=256):
        self.log = log
        self.packs = pcinstall.find_packs(pc)
        if not self.packs:
            raise SystemExit(f'{pc}: no TSData/Res folder found')
        self.cat = pcinstall.Catalog(self.packs, log=lambda *a: None)
        paths = []
        for p in reversed(self.packs):
            paths += p.scenegraph_packages()
        self.inst = sg.Install(paths)
        self.texsize = texsize
        self.gc = gcdata.GcData(gc_path) if gc_path else None
        if self.gc:
            self.gco = gcdata.GcObjects(self.gc)
            defs = self.gco.files['art_sculpture_greek_female']
            sid = gcdata.parse_modl_shaders(self.gc.get('models', [d for d in defs if d['model']][0]['model']))[0]
            self.shader_template = self.gc.get('shaders', sid)
            self.thumb_template = self.gc.get('shaders', [d['thumbnail'] for d in defs if d['thumbnail']][0])
            self.cutter = WallCutter(self.gc)
        self.taken = set()
        self.gtaken = set()

    def convert(self, d):
        import pcobj_convert as po
        joints = {}
        meshes = pcmesh.load_meshes(self.inst, d.models[0], joints=joints)
        if not meshes:
            raise LookupError(f'{d.models[0]}: no meshes')
        W = d.width
        cx = (W - 1) / 2.0
        # leaves: by the node each vertex hangs from (r_door_*, l_door_*); the rest is the frame
        def leaf_of(j):
            j = (j or '').lower()
            if 'frame' in j or 'door' not in j:
                return ''
            return 'l' if j.startswith('l_') or '_l_' in j else 'r'
        P_all = np.concatenate([m[2] for m in meshes])
        ymid = float(np.median(P_all[:, 1]))
        if abs(ymid + 0.5) > 0.2:
            d.textures.append(f'frame-y:{ymid:.2f}')
        files = {}
        tex, shaders = {}, {}
        key = f'pcdoor_{d.guid:08x}'

        def shader_of(mat):
            if mat in shaders:
                return shaders[mat]
            img, dxt1, mode, desc = po.material_of(self.inst, mat, self.texsize)
            if mode == 'skip':
                shaders[mat] = None
                return None
            tid = po.new_id(self.gc, 'textures', f'{key}_tex{len(tex)}', self.taken)
            data, fmt, size = gctex.make_txfl(f'{key}_{len(tex)}', img,
                                              'cutout' if mode == 'cutout' else ('blend' if mode == 'blend' else 'opaque'),
                                              self.texsize, dxt1)
            files[f'textures_{tid:08X}.res'] = data
            tex[mat] = img
            d.textures.append(f'{desc}:{fmt}{size[0]}x{size[1]}:{mode}')
            sid = po.new_id(self.gc, 'shaders', f'{key}_shader{len(shaders)}', self.taken)
            files[f'shaders_{sid:08X}.res'] = po.make_shader(self.shader_template, f'{key}_{len(shaders)}', tid, mode)
            shaders[mat] = sid
            return sid

        def to_gc(P, N):
            G = P.copy(); G[:, 0] -= cx; G[:, 1] = -G[:, 1] - 0.5
            GN = N.copy(); GN[:, 1] = -GN[:, 1]
            return G, GN

        # leaves: a one-tile door's one leaf ('r'); a two-tile door's left and right leaves ('l' on model
        # -x, 'r' on +x: by where each triangle lies, whatever node the PC hangs it from)
        still, bound, rendered = [], [], []
        leaf_ext = {}
        for mat, sub, P, N, U, T, J in meshes:
            sid = shader_of(mat)
            if sid is None or not len(T):
                continue
            G, GN = to_gc(P, N)
            still.append((sid, G, GN, U.copy(), T[:, ::-1]))
            rendered.append((P, N, U, T, tex[mat]))
            leafy = np.array([bool(leaf_of(j)) for j in J.tolist()])[T[:, 0]]
            side = np.where(G[T].mean(1)[:, 0] < 0, 'l', 'r') if W == 2 else np.full(len(T), 'r')
            tri_role = np.where(leafy, side, '')
            for r in ('', 'l', 'r'):
                sel = T[tri_role == r]
                if not len(sel):
                    continue
                bound.append((r, (sid, G, GN, U.copy(), sel[:, ::-1])))
                if r:
                    Q = G[np.unique(sel)]
                    lo, hi = Q.min(0), Q.max(0)
                    if r in leaf_ext:
                        lo, hi = np.minimum(lo, leaf_ext[r][0]), np.maximum(hi, leaf_ext[r][1])
                    leaf_ext[r] = (lo, hi)
        if not still:
            raise LookupError('nothing to draw')
        # the leaves' hinges: a one-tile door's PC leaf node's place (GameCube model x) where it is at one of
        # the leaf's edges, else the edge nearer the frame; a two-tile door's leaves at their outer edges
        pivots = {}
        jx = [float(o[0]) - cx for jn, o in joints.items() if leaf_of(jn)]
        for r, (lo, hi) in sorted(leaf_ext.items()):
            hx = None
            for x in (jx if W == 1 else ()):   # two-tile doors: hinged at their outer edges
                if lo[0] - 0.12 <= x <= hi[0] + 0.12 and min(abs(x - lo[0]), abs(x - hi[0])) < 0.15:
                    hx = x
                    break
            if hx is None:
                if W == 2:
                    hx = float(lo[0]) if r == 'l' else float(hi[0])
                else:
                    hx = float(lo[0]) if abs(lo[0]) > abs(hi[0]) else float(hi[0])
            pivots[r] = (hx, 'n' if hx < (lo[0] + hi[0]) / 2 else 'p')    # hinge at the leaf's -x (n) or +x (p) edge
        d.leaves = [f'{r}:{pivots[r][0]:+.2f}{pivots[r][1]}' for r in sorted(pivots)]
        # nodes and the model the states show
        nodes = []
        anim = []
        if W == 1:
            for r, part in bound:
                anim.append(part + ((1 if r else 0),))
            if pivots:
                hx, side = pivots['r']
                nodes.append(f'1:{hx:.4f},0.0000,0.0000' + (':m' if side == 'n' else ''))
        else:
            which = {'l': 'left', 'r': 'right'}
            for r, part in bound:
                anim.append(part + ((AIRLOCK_NODES[which[r]] if r else 0),))
            for r, (hx, side) in sorted(pivots.items()):
                # swing toward model -y (as the GameCube door): a leaf hinged at its -x edge turns -90
                deg = -90 if side == 'n' else 90
                nodes.append(f'{AIRLOCK_NODES[which[r]]}:{hx:.4f},0.0000,0.0000:o{deg:+d}/{AIRLOCK_SLIDE}')
        def model(tag, parts):
            mid = po.new_id(self.gc, 'models', f'{key}_{tag}', self.taken)
            files[f'models_{mid:08X}.res'] = po.make_modl(f'{key}_{tag}', parts)
            return mid
        d.out['model'] = model('model', still)
        d.out['anim_model'] = model('anim', anim)
        # the wall: each column's hole from the PC mask, on both sides
        cuts = {}
        holes_desc = []
        stem = d.models[0].lower()
        for c in range(W):
            tm = f'{stem}_{c and f"{c}e" or 0}_0_0_n_wallmask_txmt'
            md, me = self.inst.find(T_TXMT, tm)
            img = None
            if md is not None:
                t = rcol.parse_txmt(md)
                tn = t['props'].get('stdMatBaseTextureName')
                r = po.load_texture(self.inst, tn, 1024) if tn else None
                img = r[0] if r else None
                if img is not None and str(t['props'].get('tsMirrorWallMask', '0')).strip() == '1':
                    img = np.ascontiguousarray(np.asarray(img)[:, ::-1])   # the picture drawn mirrored
            if img is None:
                raise LookupError(f'{tm}: wall mask not found')
            rects, mw, mh = mask_holes(img)
            if not rects:
                raise LookupError(f'{tm}: no hole in the wall mask')
            pcx = [(c + 0.5 - u1 / mw, c + 0.5 - u0 / mw, WALL_H * (1 - v1 / mh), WALL_H * (1 - v0 / mh))
                   for (u0, u1, v0, v1) in rects]
            zt = max(h[3] for h in pcx)
            xa, xb = min(h[0] for h in pcx), max(h[1] for h in pcx)
            holes_desc.append(f'{c}:{xa - c:+.2f}..{xb - c:+.2f}x{zt:.2f}/{len(rects)}')
            for y in (0, 1):
                # the wall part's own x: the side of tile (c, 0) looks at the wall from model -y (turned
                # half round), the side of tile (c, 1) from +y
                if y == 0:
                    holes = [(-(h[1] - c), -(h[0] - c), h[2], h[3]) for h in pcx]
                else:
                    holes = [(h[0] - c, h[1] - c, h[2], h[3]) for h in pcx]
                ids = []
                for v in range(9):
                    mid = po.new_id(self.gc, 'models', f'{key}_cut_{c}_{y}_{v}', self.taken)
                    files[f'models_{mid:08X}.res'] = self.cutter.model(v, holes, f'{key}_cut{c}{y}{v}')
                    ids.append(mid)
                cuts[(c, y)] = ids
        d.hole = ' '.join(holes_desc)
        d.masks = str(W)
        # the catalogue picture
        timg = po.render_thumb(rendered, 64)
        ttid = po.new_id(self.gc, 'textures', f'{key}_thumbtex', self.taken)
        files[f'textures_{ttid:08X}.res'] = gctex.make_thumb_txfl(f'{key}_thumb', timg)
        thumb = po.new_id(self.gc, 'shaders', f'{key}_thumb', self.taken)
        files[f'shaders_{thumb:08X}.res'] = po.make_shader(self.thumb_template, f'{key}_thumb', ttid, 'blend')
        d.thumb = timg
        # GUIDs: the PC's own for the lead and each part
        tiles = []
        for c in range(W):
            for y in (0, 1):
                g = d.tiles.get((c, y))
                if not g or g in self.gco.guids or g in self.gtaken:
                    g = po.tile_guid(d.guid, c, y, self.gco.guids, self.gtaken)
                self.gtaken.add(g)
                tiles.append((c, y, g))
        leafy = bool(pivots)
        if W == 1:
            d.group = 'doors' if leafy else 'archways'
            d.gc_base = gc_single_base(d)
        else:
            d.group = 'doubledoors' if leafy else 'doublearchways'
            d.gc_base = AIRLOCK
        lines = ['[object]', f'guid = 0x{d.guid:08X}', f'base = {d.gc_base}', f'name = {d.name}']
        if d.desc:
            lines.append(f'desc = {d.desc}')
        lines += [f'price = {d.price}', f'model = 0x{d.out["model"]:08X}', f'thumbnail = 0x{thumb:08X}', 'door = 1',
                  'tiles = ' + ' '.join(f'{x},{y}:0x{g:08X}' for x, y, g in tiles), 'base_parts = 1']
        if W == 2:
            lines.append('base_tiles = ' + ' '.join(f'{x + 1},{y}' for x, y, g in tiles))
        lines += ['animated = 1']
        if W == 1 and d.gc_base != ANIM_SINGLE:
            lines.append(f'anim_base = {ANIM_SINGLE}')
        lines.append(f'anim_model = 0x{d.out["anim_model"]:08X}')
        lines += [f'node = {n}' for n in nodes]
        for (c, y), ids in sorted(cuts.items()):
            lines.append(f'wall_cut = {c},{y}:' + ','.join(f'0x{i:08X}' for i in ids))
        lines += [f'pack = {d.home.name}', '']
        d.ini = '\n'.join(lines)
        return files


def folder_of(d):
    return f'pcd{GROUPS.index(d.group) + 1}_{d.group}'


def inv_row(d):
    return [d.home.code, d.name, f'0x{d.guid:08X}', d.group, str(d.width), str(d.price), d.status or 'converted', d.reason,
            d.gc_base, d.models[0] if d.models else '', ' '.join(d.leaves), d.masks, d.hole, ' '.join(d.textures), d.folder,
            d.desc]


def write_inventory(path, todo):
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write('\t'.join(INV_COLUMNS) + '\n')
        for d in todo:
            fh.write('\t'.join(str(v).replace('\t', ' ') for v in inv_row(d)) + '\n')


def preview(path, pics):
    if not pics:
        return
    cols = min(len(pics), 12)
    rows = (len(pics) + cols - 1) // cols
    sheet = np.full((rows * 66, cols * 66, 3), 255, np.uint8)
    for i, p in enumerate(pics):
        y, x = divmod(i, cols)
        a = p[..., 3:4].astype(np.float32) / 255
        sheet[y * 66:y * 66 + 64, x * 66:x * 66 + 64] = (p[..., :3] * a + 255 * (1 - a)).astype(np.uint8)
    Image.fromarray(sheet).save(path, optimize=True)


def run(pc, gc_path, out, packs_filter=None, log=log_print, inventory_only=None):
    """Convert (or, inventory_only = a .tsv path, just list) the doors; returns (found, converted,
    skipped)."""
    t0 = time.time()
    conv = Converter(pc, gc_path if not inventory_only else None, log=log)
    todo = []
    for d in inventory(conv.cat):
        if packs_filter and d.home.code not in packs_filter:
            continue
        r = skip_reason(d)
        if r:
            d.status, d.reason = 'skipped', r
        todo.append(d)
    log(f'Doors of {len(conv.packs)} pack(s): {len(todo)} found, {sum(1 for d in todo if not d.status)} to convert')
    if inventory_only:
        write_inventory(inventory_only, todo)
        log(f'wrote {inventory_only} ({len(todo)} doors, {time.time() - t0:.1f} s)')
        return
    if not out:
        raise SystemExit('--out is needed to convert')
    out_real = os.path.realpath(out)
    here = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', '..'))
    if out_real == here or out_real.startswith(here + os.sep):
        raise SystemExit('refusing to write converted PC game data into the source tree')
    os.makedirs(out, exist_ok=True)
    for dd in glob.glob(os.path.join(out, 'pcd[0-9]_*')):  # an earlier run's folders go
        if os.path.isfile(os.path.join(dd, 'mod.ini')):
            shutil.rmtree(dd)
    work = [d for d in todo if not d.status]
    groups = {}
    for k, d in enumerate(work):
        try:
            files = conv.convert(d)
        except Exception as e:  # noqa: BLE001 - reported per door
            d.status, d.reason = 'failed', f'{type(e).__name__}: {e}'
            log(f'[{k + 1}/{len(work)}] {d.name[:50]}: {d.reason}')
            continue
        d.folder = folder_of(d)
        groups.setdefault(d.folder, []).append((d, files))
        if k % 10 == 0 or k + 1 == len(work):
            log(f'[{k + 1}/{len(work)} {time.time() - t0:5.0f}s] {d.name[:50]}')
    for folder, lst in sorted(groups.items()):
        dd = os.path.join(out, folder)
        os.makedirs(dd, exist_ok=True)
        lst.sort(key=lambda df: (df[0].price, df[0].name.lower()))
        with open(os.path.join(dd, 'mod.ini'), 'w', encoding='utf-8') as fh:
            fh.write(f'# Doors converted from the PC game ({lst[0][0].group}), by tools/pcobj/pcdoor.py. '
                     f'Your own copy of the game\'s data: do not share it.\n\n')
            for d, files in lst:
                fh.write(d.ini + '\n')
        for d, files in lst:
            for name, data in files.items():
                with open(os.path.join(dd, name), 'wb') as fh:
                    fh.write(data)
        preview(os.path.join(dd, 'preview.png'), [d.thumb for d, files in lst])
    write_inventory(os.path.join(out, 'pcdoor_inventory.tsv'), todo)
    found, done = len(todo), sum(1 for d in todo if not d.status)
    log(f'doors done in {time.time() - t0:.0f} s: {found} found, {done} converted, {found - done} skipped')
    return found, done, found - done


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pc', required=True, help='The Sims 2 (PC) install folder (all packs under it are read)')
    ap.add_argument('--gc', help='GameCube data: the disc image, or the folder with DATA/')
    ap.add_argument('--out', help='mods folder to write into (never the source tree)')
    ap.add_argument('--packs', help='only doors whose home pack is one of these (codes: base, ep1..ep8, sp1..sp9)')
    ap.add_argument('--inventory', help='write the inventory (TSV) here and stop')
    a = ap.parse_args()
    packs = set(a.packs.split(',')) if a.packs else None
    if a.inventory:
        run(a.pc, None, None, packs, inventory_only=a.inventory)
        return
    if not (a.gc and a.out):
        raise SystemExit('--gc and --out are needed to convert')
    run(a.pc, a.gc, a.out, packs)


if __name__ == '__main__':
    main()
