#!/usr/bin/env python3
"""PC mod: mods. Convert decorative objects from the user's own The Sims 2 (PC) install (base game and
every expansion and stuff pack found) into a SIMMY mod folder.

    pcobj_convert.py --pc "<The Sims 2 install>" --gc <GameCube disc image or data folder> --out <mods>
                     --all-decorative             every buy-mode decorative object of every pack
    pcobj_convert.py ... --object <GUID | name>  [--base <GameCube object file>]   one object
    pcobj_convert.py ... --batch list.tsv         the GUIDs in the list's first column ([2nd: base])
    pcobj_convert.py --pc "<install>" --list ficus                                 find objects
    pcobj_convert.py --pc "<install>" --inventory inv.tsv                          list, no output

Each object becomes <out>/pc<k>_<name>_<guid>/: mod.ini (a copy of a GameCube object, its "base",
chosen by where the object goes: wall, floor, table top, ceiling, rug; with the PC object's GUID, name,
description, price, room rating and tiles) and its resources in the GameCube's own formats (models_*:
MODL, float indexed strips per material; shaders_*; textures_*: CMPR/RGB5A3 with mips; a thumbnail).
<out>/pcdeco_inventory.tsv lists every decorative object found, converted or skipped (and why).

Never put the output in the source tree or share it: it is made of EA's PC game data, converted on the
user's machine from the user's own copy. See docs/mods.md.
"""
import argparse, os, re, shutil, struct, sys, time, zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rcol, sg, gcdata, gctex, pcmesh, pcinstall  # noqa: E402
from pcinstall import (W_PRICE, W_ROOMSORT, W_FUNCSORT, W_SUBSORT, W_RATINGS, SUBSORTS, F_DECORATIVE)  # noqa: E402

# ---------------------------------------------------------------------------------------------------
# GameCube bases: the object a converted one copies (behaviour, placement, catalogue tab), by where it
# goes. Each is a GameCube decorative object with that placement (data 4 heights, 8 flags, 0xD wall
# flags as their init trees set them):
BASES = {
    'wall': 'art_wall_painting_01',          # on a wall (0x814 / wall 0x4), "view"
    'mirror': 'skill_charisma_mirror',       # on a wall, "practice speech" (charisma)
    'floor': 'art_sculpture_contemporary',   # on the floor, "view"
    'floor_plant': 'plant_floor_rubbertree', # on the floor, needs watering
    'surface': 'art_sculpture_wooden_model', # on tables and counters (heights 0x8C)
    'surface_plant': 'art_plant_lavender',   # on tables and counters, a plant
    'rug': 'rug_outdoor_blanket',            # a rug group: lead + tiles, walkable, things go on it
    'ceiling': 'rug_outdoor_blanket',        # hangs from the ceiling: a rug tile (sims walk under it)
}
FIXED_POINT = False
SORT_KEY = {8: '1', 2: '2', 1: '3', 0x10: '4', 4: '5', 0x20: '6', 0x40: '7', 0x80: '7', 0: '7'}

T_TXTR, T_TXMT = rcol.T_TXTR, rcol.T_TXMT


def log_print(*a):
    print(*a, flush=True)


# ---------------------------------------------------------------------------------------------------
# PC textures

def _rgb565(c):
    r = ((c >> 11) & 31) * 255 // 31; g = ((c >> 5) & 63) * 255 // 63; b = (c & 31) * 255 // 31
    return np.stack([r, g, b], -1).astype(np.int32)


def decode_dxt(data, w, h, fmt):
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    bs = 8 if fmt == 4 else 16
    blocks = np.frombuffer(data[:bw * bh * bs], np.uint8).reshape(bh * bw, bs)
    cb = blocks[:, -8:]
    c0 = cb[:, 0].astype(np.int32) | cb[:, 1].astype(np.int32) << 8
    c1 = cb[:, 2].astype(np.int32) | cb[:, 3].astype(np.int32) << 8
    p0, p1 = _rgb565(c0), _rgb565(c1)
    four = (c0 > c1) | (fmt != 4)
    p2 = np.where(four[:, None], (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(four[:, None], (p0 + 2 * p1) // 3, 0)
    pal = np.stack([p0, p1, p2, p3], 1)
    bits = cb[:, 4:8].astype(np.uint32)
    idx = (bits[:, 0] | bits[:, 1] << 8 | bits[:, 2] << 16 | bits[:, 3] << 24)
    sel = np.stack([(idx >> (2 * k)) & 3 for k in range(16)], 1)
    rgb = np.take_along_axis(pal, sel[:, :, None].astype(np.int64).repeat(3, 2), 1)
    if fmt == 4:
        a = np.where((~four[:, None]) & (sel == 3), 0, 255)
    elif fmt == 5:
        ab = blocks[:, :8]
        a = np.stack([((ab[:, k // 2] >> (4 * (k & 1))) & 15) * 17 for k in range(16)], 1)
    else:
        a0 = blocks[:, 0].astype(np.int32); a1 = blocks[:, 1].astype(np.int32)
        ab = blocks[:, 2:8].astype(np.uint64)
        abits = sum(ab[:, k] << np.uint64(8 * k) for k in range(6))
        ai = np.stack([((abits >> np.uint64(3 * k)) & np.uint64(7)).astype(np.int32) for k in range(16)], 1)
        tab = [a0, a1]
        big = a0 > a1
        for k in range(2, 8):
            tab.append(np.where(big, ((8 - k) * a0 + (k - 1) * a1) // 7,
                                np.where(k < 6, ((6 - k) * a0 + (k - 1) * a1) // 5, 0 if k == 6 else 255)))
        tab = np.stack(tab, 1)
        a = np.take_along_axis(tab, ai.astype(np.int64), 1)
    px = np.concatenate([rgb, a[:, :, None]], 2).reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4)
    return px.reshape(bh * 4, bw * 4, 4)[:h, :w].astype(np.uint8)


def decode_level(data, w, h, fmt):
    if fmt in (4, 5, 8):
        return decode_dxt(data, w, h, fmt)
    a = np.frombuffer(data, np.uint8)
    if fmt in (1, 7):
        p = a[:w * h * 4].reshape(h, w, 4); return p[:, :, [2, 1, 0, 3]].copy()
    if fmt in (2, 9):
        p = a[:w * h * 3].reshape(h, w, 3)
        return np.concatenate([p[:, :, [2, 1, 0]], np.full((h, w, 1), 255, np.uint8)], 2)
    if fmt == 3:
        p = a[:w * h].reshape(h, w)
        return np.stack([np.full_like(p, 255)] * 3 + [p], 2)
    if fmt == 6:
        p = a[:w * h].reshape(h, w)
        return np.stack([p, p, p, np.full_like(p, 255)], 2)
    raise ValueError(f'texture format {fmt}')


def load_texture(inst, name, maxdim=256):
    """(RGBA of the largest level <= maxdim, [(w, h, DXT1 blocks)] large -> small when DXT1) or None."""
    d, e = inst.find(T_TXTR, name)
    if d is None:
        return None
    tx = rcol.parse_txtr(d)
    n = len(tx['levels'])
    levels = []
    for i, (kind, val) in enumerate(tx['levels']):
        k = n - 1 - i
        w, h = max(1, tx['w'] >> k), max(1, tx['h'] >> k)
        if kind == 'lifo':
            ld, le = inst.find(rcol.T_LIFO, val)
            if ld is None:
                continue
            val = rcol.parse_lifo(ld)['data']
        levels.append((w, h, val))
    levels.sort(key=lambda l: -l[0] * l[1])
    fit = [l for l in levels if max(l[0], l[1]) <= maxdim] or levels[-1:]
    if not fit:
        return None
    w, h, data = fit[0]
    img = decode_level(data, w, h, tx['format'])
    dxt1 = [(w2, h2, d2[:max(1, w2 // 4) * max(1, h2 // 4) * 8]) for (w2, h2, d2) in fit] if tx['format'] == 4 else None
    return img, dxt1


# ---------------------------------------------------------------------------------------------------
# GameCube resources

def make_modl(name, parts):
    """parts: [(shader id, P, N, U, T)] in GameCube model space. One sub-model; an indexed strip
    (GX_TRIANGLES) per part, split at 21000 triangles (16-bit counts). Fixed point as the disc's models
    (vertex format 6: s16 positions in model units times the model's scale, s16 4.12 uvs) when
    FIXED_POINT and the uvs fit; else float (vertex format 7)."""
    split = []
    for sid, P, N, U, T in parts:
        for k in range(0, len(T), 21000):
            t = T[k:k + 21000]
            used = np.unique(t)
            remap = np.full(len(P), -1, np.int64); remap[used] = np.arange(len(used))
            split.append((sid, P[used], N[used], U[used], remap[t]))
    allP = np.concatenate([p[1] for p in split])
    allU = np.concatenate([p[3] for p in split])
    # Fixed point (vertex format 6, as most disc models) needs the model's scale, which the game applies
    # through the object's animation controller (its character): the converted objects have none, so
    # their models stay float (format 7).
    fixed = FIXED_POINT and float(np.abs(allU).max()) < 7.9
    extent = max(float(np.abs(allP).max()), 1e-3)
    scale = 2.0 ** np.ceil(np.log2(extent / 32000.0)) if fixed else 1.0
    body = bytearray()
    body += struct.pack('>I', 0) + b'\0' * 48 + b'\0'
    body += struct.pack('>5I', 0, 0, 0, 0, 0)
    body += b'\0' + struct.pack('>f', scale) + struct.pack('>I', 1)
    body += struct.pack('>II', 0, len(split))
    for sid, P, N, U, T in split:
        flags = 0x2A | (0x10 if fixed else 0)  # uvs | normals | indexed (| fixed point)
        body += struct.pack('>III', flags, sid, 1) + b'\0' + struct.pack('>f', 1.0)
        body += b'\0' + struct.pack('>I', len(P))
        if fixed:
            q = np.clip(np.round(P / scale), -32767, 32767).astype('>i2')
            body += np.concatenate([q, np.zeros((len(P), 1), '>i2')], 1).tobytes()
            body += np.clip(np.round(U * 4096.0), -32767, 32767).astype('>i2').tobytes()
        else:
            body += np.concatenate([P, np.zeros((len(P), 1))], 1).astype('>f4').tobytes()
            body += U.astype('>f4').tobytes()
        n = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
        nn = np.clip(np.round(n * 127), -127, 127).astype(np.int8)
        body += np.concatenate([nn, np.full((len(N), 1), 127, np.int8)], 1).tobytes()
        idx = T.reshape(-1).astype(np.uint16)
        dl = bytearray([0x96 if fixed else 0x97]) + struct.pack('>H', len(idx))  # GX_TRIANGLES | GX_VTXFMT6/7
        dl += np.repeat(idx[:, None], 3, 1).astype('>u2').tobytes()
        dl += b'\0' * ((-len(dl)) % 32)
        body += struct.pack('>IBII', len(idx), 3, len(dl), 3) + dl
        body += b'\x06'
    # bounds (model units: divided by the scale)
    lo, hi = allP.min(0) / scale, allP.max(0) / scale
    c = (lo + hi) / 2; r = float(np.linalg.norm(allP / scale - c, axis=1).max())
    body += struct.pack('>4f', *c, r) + struct.pack('>6f', *lo, *hi) * 2 + bytes([0, 0, 0, 0])
    return gcdata.make_header(0x3A, 0x4D4F444C, 0xFFFFFFFF, name, bytes(body))


def make_shader(template, name, texture, mode, alpha_ref=0.5):
    """A copy of a disc shader (`template`: its resource bytes) with one layer: `texture`, and
    mode 'opaque' (z test + write), 'cutout' (alpha test), 'blend' (alpha blending + a low alpha test)."""
    ver, sig, var, _, off, size = gcdata.data_header(template)
    d = bytearray(template[off:off + size])
    d[0] = 1
    L = 0x64
    struct.pack_into('>I', d, L, texture)
    blend = 0x58 if mode == 'blend' else 0x18
    flags = {'opaque': 0x18, 'cutout': 0x1C, 'blend': 0x1C}[mode]
    struct.pack_into('>I', d, L + 0x0C, blend)
    struct.pack_into('>I', d, L + 0x10, flags)
    struct.pack_into('>f', d, L + 0x14, alpha_ref if mode == 'cutout' else 0.04)
    d[L + 0x1C:L + 0x20] = bytes([0, 1, 0, 1])
    return gcdata.make_header(ver, sig, var, name, bytes(d))


def new_id(gc, mgr, key, taken):
    rid = gcdata.symbol_id(key)
    while (gc is not None and gc.has(mgr, rid)) or rid in taken or rid == 0:
        rid = (rid * 1103515245 + 12345) & 0xFFFFFFFF
    taken.add(rid)
    return rid


def tile_guid(guid, x, y, gc_guids, taken):
    g = zlib.crc32(f'simmy pcmod tile {guid:08x} {x} {y}'.encode()) & 0xFFFFFFFF
    while g in gc_guids or g in taken or g == 0:
        g = (g * 1103515245 + 12345) & 0xFFFFFFFF
    taken.add(g)
    return g


# ---------------------------------------------------------------------------------------------------
# Thumbnail: a small software render (orthographic, three-quarter view from the object's front)

def render_thumb(meshes, size=64, ss=3):
    S = size * ss
    col = np.zeros((S, S, 4), np.float32); zb = np.full((S, S), -1e9, np.float32)
    allP = np.concatenate([m[0] for m in meshes])
    yaw, pitch = np.radians(150), np.radians(22)
    Rz = np.array([[np.cos(yaw), -np.sin(yaw), 0], [np.sin(yaw), np.cos(yaw), 0], [0, 0, 1]])
    Rx = np.array([[1, 0, 0], [0, np.cos(pitch), -np.sin(pitch)], [0, np.sin(pitch), np.cos(pitch)]])
    M = Rx @ Rz
    V = allP @ M.T
    lo, hi = V[:, [0, 2]].min(0), V[:, [0, 2]].max(0)
    scale = 0.92 * S / max(float(max(hi - lo)), 1e-6); ctr = (lo + hi) / 2
    light = np.array([0.3, 0.6, 0.75]); light /= np.linalg.norm(light)
    for P, N, U, T, img in meshes:
        Q = P @ M.T
        sx = (Q[:, 0] - ctr[0]) * scale + S / 2; sy = S / 2 - (Q[:, 2] - ctr[1]) * scale; sz = -Q[:, 1]
        shade = 0.55 + 0.45 * np.abs(N @ light) / np.maximum(np.linalg.norm(N, axis=1), 1e-6)
        th, tw = img.shape[:2]
        for a, b, c in T:
            x0, x1 = int(max(0, np.floor(min(sx[a], sx[b], sx[c])))), int(min(S - 1, np.ceil(max(sx[a], sx[b], sx[c]))))
            y0, y1 = int(max(0, np.floor(min(sy[a], sy[b], sy[c])))), int(min(S - 1, np.ceil(max(sy[a], sy[b], sy[c]))))
            if x1 < x0 or y1 < y0:
                continue
            den = (sy[b] - sy[c]) * (sx[a] - sx[c]) + (sx[c] - sx[b]) * (sy[a] - sy[c])
            if abs(den) < 1e-9:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((sy[b] - sy[c]) * (xs - sx[c]) + (sx[c] - sx[b]) * (ys - sy[c])) / den
            w1 = ((sy[c] - sy[a]) * (xs - sx[c]) + (sx[a] - sx[c]) * (ys - sy[c])) / den
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            z = w0 * sz[a] + w1 * sz[b] + w2 * sz[c]
            u = w0 * U[a, 0] + w1 * U[b, 0] + w2 * U[c, 0]; v = w0 * U[a, 1] + w1 * U[b, 1] + w2 * U[c, 1]
            tx = (np.mod(u, 1.0) * tw).astype(int).clip(0, tw - 1); ty = (np.mod(v, 1.0) * th).astype(int).clip(0, th - 1)
            texel = img[ty, tx].astype(np.float32)
            sh = w0 * shade[a] + w1 * shade[b] + w2 * shade[c]
            ok = inside & (texel[..., 3] >= 100)
            reg = zb[y0:y1 + 1, x0:x1 + 1]
            upd = ok & (z > reg)
            reg[upd] = z[upd]
            cr = col[y0:y1 + 1, x0:x1 + 1]
            cr[upd, :3] = texel[upd, :3] * sh[upd, None]
            cr[upd, 3] = 255
    out = col.reshape(size, ss, size, ss, 4).mean((1, 3))
    rgb = out[..., :3] / np.maximum(out[..., 3:4] / 255, 1e-6)
    return np.concatenate([rgb.clip(0, 255), out[..., 3:4]], 2).astype(np.uint8)


# ---------------------------------------------------------------------------------------------------
# The session: the install, the disc, the catalogue

class Session:
    def __init__(self, pc, gc_path, out=None, texsize=256, log=log_print, packs=None):
        self.log = log
        self.packs = pcinstall.find_packs(pc)
        if not self.packs:
            raise SystemExit(f'{pc}: no TSData/Res folder found')
        self.catalog = None
        self.gc_path = gc_path
        self.out = out
        self.texsize = texsize
        self._gc = None
        self._inst = None

    def load_catalog(self):
        if self.catalog is None:
            self.log(f'Reading the objects of {len(self.packs)} pack(s):')
            self.catalog = pcinstall.Catalog(self.packs, log=self.log)
        return self.catalog

    @property
    def inst(self):
        if self._inst is None:
            paths = []
            for p in reversed(self.packs):  # newest first
                paths += p.scenegraph_packages()
            self._inst = sg.Install(paths)
        return self._inst

    @property
    def gc(self):
        if self._gc is None:
            if not self.gc_path:
                raise SystemExit('--gc (the GameCube disc image or data folder) is needed to convert')
            self._gc = gcdata.GcData(self.gc_path)
            self.gco = gcdata.GcObjects(self._gc)
            q = self.gco.qd
            self.templates = {}
            for kind, f in BASES.items():
                defs = self.gco.files.get(f)
                if not defs:
                    raise SystemExit(f'GameCube object {f} not found on the disc')
            tmpl_model = [d for d in self.gco.files['art_sculpture_greek_female'] if d['model']][0]['model']
            sid = gcdata.parse_modl_shaders(self._gc.get('models', tmpl_model))[0]
            self.shader_template = self._gc.get('shaders', sid)
            thumbs = [d['thumbnail'] for d in self.gco.files['art_sculpture_greek_female'] if d['thumbnail']]
            self.thumb_template = self._gc.get('shaders', thumbs[0])
        return self._gc


# ---------------------------------------------------------------------------------------------------
# Inventory: every buy-mode decorative object

class Item:
    """One decorative object (its newest definition) and what the converter makes of it."""
    def __init__(self, cat, guid):
        self.guid = guid
        self.o = cat.newest(guid)
        self.home = cat.home(guid)
        self.packs = [x.pack.code for x in cat.by_guid[guid]]
        self.objname = self.o.name
        self.name, self.desc = cat.catalog_text(self.o)
        self.price = self.o.w(W_PRICE)
        self.subsort = self.o.w(W_SUBSORT)
        self.roomsort = self.o.w(W_ROOMSORT)
        self.room = self.o.s(W_RATINGS + 6)
        self.models = cat.models(self.o)
        self.parts = cat.parts(self.o)
        self.tiles = sorted({(p.sub >> 8, p.sub & 0xFF): p.guid for p in self.parts}.items())
        self.bhavs = cat.bhav_count(self.o)
        self.semiglobal = cat.semiglobal(self.o)
        fams, default = cat.recolours(guid)
        self.recolours = fams
        self.default_family = default
        self.status = ''
        self.reason = ''
        self.kind = ''
        self.base = ''
        self.out = ''
        self.textures = []
        self.verts = self.tris = 0
        self.bounds = None
        self.notes = []

    def footprint(self):
        if not self.tiles:
            return 1, 1
        return max(x for (x, y), g in self.tiles) + 1, max(y for (x, y), g in self.tiles) + 1

    def subsort_name(self):
        return SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')


def decorative_guids(cat):
    out = []
    for g, lst in cat.by_guid.items():
        o = lst[-1]
        if o.is_lead() and o.w(W_FUNCSORT) & F_DECORATIVE:
            out.append(g)
    return out


def skip_reason(it):
    """Why an object is not offered (not a buyable catalogue object), or ''."""
    if not it.name:
        return 'no catalogue name (not a buy-mode object)'
    if 'invisible' in it.objname.lower():
        return 'invisible helper object'
    if it.o.w(0x17) or it.o.w(0x18):
        return 'diagonal variant (the game swaps it in for its object placed diagonally)'
    if not it.models:
        return 'no model'
    return ''


# ---------------------------------------------------------------------------------------------------
# Placement: where the PC object goes, from its mesh and catalogue

def classify(it, P):
    lo, hi = P.min(0), P.max(0)
    w, h = it.footprint()
    name = (it.objname + ' ' + (it.name or '')).lower()
    if 'roof' in name and 'solar' in name:
        return 'roof'
    flat = hi[2] < 0.15 and lo[2] > -0.06
    if it.subsort == 4 or (flat and any(k in it.objname.lower() for k in ('rug', 'mat', 'floor', 'carpet'))):
        return 'rug'
    depth = hi[1] - lo[1]
    against_wall = lo[1] < -0.3 and hi[1] < -0.05 and depth < 0.45 and h == 1
    if it.subsort == 0x20 and lo[1] < -0.3 and h == 1:
        return 'wall'                    # curtains: over a window, on the wall
    if against_wall and (lo[2] > 0.15 or depth < 0.1):
        return 'mirror' if it.subsort == 0x10 else 'wall'
    if lo[1] < -0.38 and lo[2] > 1.0 and h == 1:
        return 'wall'                    # hangs out from the wall (shop signs)
    if lo[2] > 1.7 and hi[2] > 2.2:
        return 'ceiling'
    # goes outside too (rocks, signs, gnomes): a floor object; plants say "outside" for any room
    outdoor_only = bool(it.roomsort & 0x10) and it.subsort != 8
    small = (hi[2] - lo[2]) < 0.75 and max(abs(lo[0]), abs(hi[0]), abs(lo[1]), abs(hi[1])) < 0.47
    if w * h == 1 and small and abs(lo[2]) < 0.1 and not outdoor_only:
        return 'surface_plant' if it.subsort == 8 else 'surface'
    return 'floor_plant' if it.subsort == 8 else 'floor'


# ---------------------------------------------------------------------------------------------------
# Materials

def parse_color(s, default=(0.8, 0.8, 0.8)):
    try:
        v = [float(x) for x in s.split(',')]
        return tuple(v[:3]) if len(v) >= 3 else default
    except Exception:
        return default


def material_of(inst, mat, maxdim):
    """(RGBA image, DXT1 levels or None, mode, description) for a TXMT name."""
    d, e = inst.find(T_TXMT, mat.split('!')[-1])
    props = {}
    mtype = ''
    if d is not None:
        try:
            tm = rcol.parse_txmt(d)
            props = tm['props']; mtype = tm['type']
        except Exception:
            pass
    low = (mat + ' ' + mtype).lower()
    if mtype.lower() == 'null' or props.get('stdMatFillMode', 'solid') == 'none':
        return None, None, 'skip', 'null'
    tex = props.get('stdMatBaseTextureName') if props.get('stdMatBaseTextureEnabled', 'true') != 'false' else None
    if not tex and ('reflection' in low or 'mirror' in mtype.lower()):
        # a mirror's glass (the PC renders a reflection): pale blue-grey with a diagonal sheen
        y, x = np.mgrid[0:32, 0:32]
        sheen = np.exp(-((x + y - 30) / 5.0) ** 2) * 40 + np.exp(-((x + y - 44) / 2.5) ** 2) * 25
        base = 175 + (31 - y) * 1.2
        img = np.stack([base + sheen, base + 8 + sheen, base + 18 + sheen, np.full_like(base, 255)], -1)
        return img.clip(0, 255).astype(np.uint8), None, 'opaque', 'mirror'
    img = dxt1 = None
    if tex:
        r = load_texture(inst, tex, maxdim)
        if r:
            img, dxt1 = r
    if img is None:
        c = parse_color(props.get('stdMatDiffCoef', ''), (0.8, 0.8, 0.8))
        glass = props.get('stdMatAlphaBlendMode', 'none').lower() == 'blend'
        alpha = float(props.get('stdMatUntexturedDiffAlpha', '0.35' if glass else '1') or 1)
        if glass:
            alpha = min(alpha, 0.5)
        px = np.array([*(np.clip(np.array(c), 0, 1) * 255), np.clip(alpha, 0, 1) * 255], np.uint8)
        img = np.tile(px, (8, 8, 1))
        dxt1 = None
    blend = props.get('stdMatAlphaBlendMode', 'none').lower()
    if blend == 'additive':
        # glows and flames: blended with the brightness as alpha (the GameCube shader blends, it
        # does not add)
        img = img.copy()
        img[..., 3] = (img[..., 3].astype(np.uint16) * img[..., :3].max(-1) // 255).astype(np.uint8)
        blend = 'blend'
    test = props.get('stdMatAlphaTestEnabled', '0') == '1'
    kind = gctex.alpha_kind(img)
    if blend == 'blend' and kind != 'opaque':
        mode = 'blend'
    elif (test or blend == 'blend') and kind != 'opaque':
        mode = 'cutout'
    else:
        mode = 'opaque'
    if mode == 'opaque':
        img = img.copy(); img[..., 3] = 255
    return img, dxt1, mode, (tex or 'colour')


# ---------------------------------------------------------------------------------------------------

def convert(s, it, base_override=None, write=True):
    """Converts one Item into s.out. Sets it.status ('converted', 'skipped', 'failed') and it.reason."""
    cat, inst = s.catalog, s.inst
    gc = s.gc if (write or s.gc_path) else None
    if gc is not None and it.guid in s.gco.guids:
        it.status, it.reason = 'skipped', 'GUID is a GameCube object\'s'
        return it
    materials = None
    if it.recolours:
        materials = dict(it.recolours[0][1])
    meshes = []
    errors = []
    for mname in it.models[:1]:
        try:
            meshes = pcmesh.load_meshes(inst, mname, materials, log=lambda m: it.notes.append(m) if m not in it.notes else None)
        except LookupError as e:
            errors.append(str(e))
        except Exception as e:
            errors.append(f'{mname}: {type(e).__name__}: {e}')
    if not meshes:
        it.status, it.reason = 'skipped', errors[0] if errors else 'model has no drawable mesh'
        return it
    P_all = np.concatenate([m[2] for m in meshes])
    it.bounds = (P_all.min(0), P_all.max(0))
    kind = classify(it, P_all)
    if kind == 'roof':
        it.status, it.reason, it.kind = 'skipped', 'goes on roofs (no roof placement on the GameCube)', kind
        return it
    it.kind = kind
    base = base_override or BASES[kind]
    it.base = base
    bdefs = s.gco.files.get(base) if gc is not None else [{'master': 1 if kind in ('rug', 'ceiling') else 0}]
    if not bdefs:
        it.status, it.reason = 'failed', f'GameCube base {base} not found'
        return it
    W, H = it.footprint()
    group_base = any(d['master'] != 0 for d in bdefs)
    multi = W * H > 1 or group_base
    # the GameCube draws a group's model at the middle of its tiles; wall objects 0.45 off the tile's
    # middle toward the wall. PC: the first tile's middle, the wall at -y; the GameCube's model space is
    # the PC's mirrored in y (its world swaps x and y).
    cx, cy = (W - 1) / 2.0, (H - 1) / 2.0
    wall = kind in ('wall', 'mirror')
    tag = re.sub(r'[^a-z0-9]+', '', (it.name or it.objname).lower())[:20] or 'object'
    folder = os.path.join(s.out or '.', f'pc{SORT_KEY.get(it.subsort, "7")}_{tag}_{it.guid:08x}')
    it.out = os.path.basename(folder)
    taken = set(); files = {}
    parts = []; rendered = []; texcache = {}; skipped = set()
    for k, (mat, subset, P, N, U, T) in enumerate(meshes):
        if mat in skipped:
            continue
        if mat not in texcache:
            img, dxt1, mode, desc = material_of(inst, mat, s.texsize)
            if mode == 'skip':
                skipped.add(mat)
                continue
            tid = new_id(gc, 'textures', f'pcmod_{it.guid:08x}_tex{len(texcache)}', taken)
            data, fmt, size = gctex.make_txfl(f'pc_{tag}_{len(texcache)}', img,
                                              'cutout' if mode == 'cutout' else ('blend' if mode == 'blend' else 'opaque'),
                                              s.texsize, dxt1)
            files[f'textures_{tid:08X}.res'] = data
            sid = new_id(gc, 'shaders', f'pcmod_{it.guid:08x}_shader{len(texcache)}', taken)
            if gc is not None:
                files[f'shaders_{sid:08X}.res'] = make_shader(s.shader_template, f'pc_{tag}_{len(texcache)}', tid, mode)
            texcache[mat] = (sid, img)
            it.textures.append(f'{desc}:{fmt}{size[0]}x{size[1]}:{mode}')
        sid, img = texcache[mat]
        G = P.copy()
        G[:, 0] -= cx
        G[:, 1] = -(G[:, 1] - cy)
        if wall:
            G[:, 1] -= 0.45
        GN = N.copy(); GN[:, 1] = -GN[:, 1]
        UV = U.copy()
        parts.append((sid, G, GN, UV, T[:, ::-1]))
        rendered.append((P, N, U, T, img))
    if not parts:
        it.status, it.reason = 'skipped', 'model has no drawable mesh'
        return it
    it.verts = sum(len(p[1]) for p in parts); it.tris = sum(len(p[4]) for p in parts)
    if not write:
        it.status = 'ok (dry run)'
        return it
    mid = new_id(gc, 'models', f'pcmod_{it.guid:08x}_model', taken)
    files[f'models_{mid:08X}.res'] = make_modl(f'pc_{tag}', parts)
    timg = render_thumb(rendered, 64)
    ttid = new_id(gc, 'textures', f'pcmod_{it.guid:08x}_thumbtex', taken)
    files[f'textures_{ttid:08X}.res'] = gctex.make_thumb_txfl(f'pc_{tag}_thumb', timg)
    thumb_id = new_id(gc, 'shaders', f'pcmod_{it.guid:08x}_thumb', taken)
    files[f'shaders_{thumb_id:08X}.res'] = make_shader(s.thumb_template, f'pc_{tag}_thumb', ttid, 'blend')
    # tiles: (x, y) as the GameCube's sub index (y mirrored with the model), with GUIDs
    tiles = []
    if multi:
        gtaken = set()
        pcg = dict(it.tiles)
        coords = list(pcg.keys()) or [(0, 0)]
        for (x, y) in sorted(coords):
            g = pcg.get((x, y)) or tile_guid(it.guid, x, y, s.gco.guids, gtaken)
            if g in s.gco.guids or g == it.guid:
                g = tile_guid(it.guid, x, y, s.gco.guids, gtaken)
            tiles.append((x, H - 1 - y, g))
    if os.path.isdir(folder):
        shutil.rmtree(folder)
    os.makedirs(folder, exist_ok=True)
    for fn, data in files.items():
        with open(os.path.join(folder, fn), 'wb') as f:
            f.write(data)
    try:
        from PIL import Image
        Image.fromarray(timg, 'RGBA').save(os.path.join(folder, 'thumbnail.png'))
    except Exception:
        pass
    one = lambda v: (v or '').replace('\r', ' ').replace('\n', ' ').strip()
    with open(os.path.join(folder, 'mod.ini'), 'w', encoding='utf-8') as f:
        f.write('# PC mod: mods. Converted by tools/pcobj/pcobj_convert.py from the user\'s own The Sims 2 (PC)\n'
                '# install. EA game data: for this machine only, never share or commit it.\n')
        f.write(f'# PC object "{it.objname}" ({it.home.name}), model {", ".join(it.models[:1])}, '
                f'{it.subsort_name()}, placed: {kind}\n[object]\n')
        f.write(f'guid = 0x{it.guid:08X}\nbase = {base}\nname = {one(it.name)}\ndesc = {one(it.desc)}\n')
        f.write(f'price = {it.price}\nmodel = 0x{mid:08X}\nthumbnail = 0x{thumb_id:08X}\n')
        f.write(f'functions = 0x{F_DECORATIVE:X}\nroom = {max(0, min(10, it.room))}\n')
        if tiles:
            f.write('tiles = ' + ' '.join(f'{x},{y}:0x{g:08X}' for x, y, g in tiles) + '\n')
        f.write(f'pack = {it.home.name}\n')
    it.status = 'converted'
    return it


# ---------------------------------------------------------------------------------------------------

INV_COLUMNS = ['guid', 'name', 'object', 'pack', 'packs', 'subsort', 'footprint', 'placement', 'base',
               'price', 'room', 'bhavs', 'semiglobal', 'model', 'textures', 'recolours', 'verts', 'tris',
               'status', 'reason', 'folder', 'notes', 'bounds']


def inv_row(it):
    w, h = it.footprint()
    return [f'0x{it.guid:08X}', it.name or '', it.objname, it.home.name, ','.join(dict.fromkeys(it.packs)),
            it.subsort_name(), f'{w}x{h}', it.kind, it.base, str(it.price), str(it.room), str(it.bhavs),
            it.semiglobal, ','.join(it.models[:2]), ' '.join(it.textures), str(max(0, len(it.recolours) - 1)),
            str(it.verts), str(it.tris), it.status, it.reason, it.out, '; '.join(it.notes),
            ' '.join(f'{v:.2f}' for v in np.concatenate(it.bounds)) if it.bounds is not None else '']


def write_inventory(path, items):
    if os.path.dirname(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\t'.join(INV_COLUMNS) + '\n')
        for it in items:
            f.write('\t'.join(str(v).replace('\t', ' ') for v in inv_row(it)) + '\n')


_worker = None


def _work_init(pc, gc, out, texsize):
    global _worker
    _worker = Session(pc, gc, out, texsize, log=lambda *a: None)
    _worker.load_catalog()
    _ = _worker.gc


def _work(args):
    guid, base = args
    it = Item(_worker.catalog, guid)
    try:
        convert(_worker, it, base)
    except Exception as e:
        it.status, it.reason = 'failed', f'{type(e).__name__}: {e}'
    return inv_row(it)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pc', required=True, help='The Sims 2 (PC) install folder (all packs under it are read)')
    ap.add_argument('--gc', help='GameCube data: the disc image, or the folder with DATA/')
    ap.add_argument('--out', help='mods folder to write into (never the source tree)')
    ap.add_argument('--list', help='list PC objects whose name contains this text')
    ap.add_argument('--object', help='one PC object: GUID (hex) or part of its name')
    ap.add_argument('--base', help='with --object: the GameCube object to copy (default: chosen by placement)')
    ap.add_argument('--all-decorative', action='store_true', help='every buy-mode decorative object')
    ap.add_argument('--batch', help='a list of GUIDs (first column; optional second column: base)')
    ap.add_argument('--packs', help='only objects whose home pack is one of these (codes: base, ep1..ep8, sp1..sp9)')
    ap.add_argument('--inventory', help='write the decorative inventory (TSV) here and stop')
    ap.add_argument('--jobs', type=int, default=0, help='worker processes (default: CPUs - 1)')
    ap.add_argument('--texsize', type=int, default=256)
    ap.add_argument('--name', help='with --object: catalogue name')
    ap.add_argument('--price', type=int, help='with --object: price')
    a = ap.parse_args()
    t0 = time.time()
    s = Session(a.pc, a.gc, a.out, a.texsize)
    s.log('Packs: ' + ', '.join(f'{p.name} ({p.code})' for p in s.packs))
    cat = s.load_catalog()
    if a.list:
        q = a.list.lower()
        for g, lst in sorted(cat.by_guid.items(), key=lambda kv: kv[1][-1].name):
            o = lst[-1]
            nm, _ = cat.catalog_text(o) if o.is_lead() else (None, '')
            if q in o.name.lower() or (nm and q in nm.lower()) or q == f'{g:08x}':
                print(f'{g:08X}  {lst[0].pack.code:4s}  {o.name}  "{nm or ""}"  fn 0x{o.w(W_FUNCSORT):x}')
        return
    if a.out:
        out_real = os.path.realpath(a.out)
        here = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', '..'))
        if out_real == here or out_real.startswith(here + os.sep):
            raise SystemExit('refusing to write converted PC game data into the source tree')
    jobs = []
    if a.object:
        q = a.object.lower()
        qg = int(q, 16) if re.fullmatch(r'(0x)?[0-9a-f]{6,8}', q) else None
        hits = [g for g, lst in cat.by_guid.items() if lst[-1].is_lead() and
                (g == qg or (qg is None and (q in lst[-1].name.lower() or q in (cat.catalog_text(lst[-1])[0] or '').lower())))]
        if not hits:
            raise SystemExit(f'no PC object matches {a.object}')
        jobs = [(hits[0], a.base)]
    elif a.batch:
        for line in open(a.batch, encoding='utf-8'):
            f = line.strip().split('\t')
            if f and re.fullmatch(r'(0x)?[0-9a-fA-F]{6,8}', f[0]):
                jobs.append((int(f[0], 16), f[1] if len(f) > 1 and f[1] in BASES.values() else None))
    elif a.all_decorative or a.inventory:
        jobs = [(g, None) for g in decorative_guids(cat)]
    else:
        raise SystemExit('nothing to do: --all-decorative, --object, --batch, --inventory or --list')
    packs = set(a.packs.split(',')) if a.packs else None
    items = []
    for g, b in jobs:
        it = Item(cat, g)
        if packs and it.home.code not in packs:
            continue
        r = skip_reason(it)
        if r:
            it.status, it.reason = 'skipped', r
        items.append((it, b))
    items.sort(key=lambda ib: (SORT_KEY.get(ib[0].subsort, '7'), (ib[0].name or '').lower()))
    todo = [(it, b) for it, b in items if not it.status]
    s.log(f'{len(items)} object(s), {len(todo)} to convert ({time.time() - t0:.1f} s)')
    if a.inventory:
        for it, b in todo:
            try:
                convert(s, it, b, write=False)
            except Exception as e:
                it.status, it.reason = 'failed', f'{type(e).__name__}: {e}'
        write_inventory(a.inventory, [it for it, b in items])
        s.log(f'wrote {a.inventory} ({time.time() - t0:.1f} s)')
        return
    if not (a.out and a.gc):
        raise SystemExit('--gc and --out are needed to convert')
    os.makedirs(a.out, exist_ok=True)
    _ = s.gc
    if a.object and todo:
        it, b = todo[0]
        if a.name: it.name = a.name
        if a.price is not None: it.price = a.price
    n = len(todo)
    workers = a.jobs or max(1, (os.cpu_count() or 2) - 1)
    rows = {}
    if workers > 1 and n > 4 and not a.object:
        import multiprocessing as mp
        with mp.Pool(workers, _work_init, (a.pc, a.gc, a.out, a.texsize)) as pool:
            for k, row in enumerate(pool.imap_unordered(_work, [(it.guid, b) for it, b in todo], chunksize=2)):
                rows[int(row[0], 16)] = row
                s.log(f'[{k + 1}/{n}] {row[18]:9s} {row[1][:40]:40s} {row[3]:20.20s} {row[7]:13s} {row[19]}')
    else:
        for k, (it, b) in enumerate(todo):
            try:
                convert(s, it, b)
            except Exception as e:
                it.status, it.reason = 'failed', f'{type(e).__name__}: {e}'
            row = inv_row(it)
            rows[it.guid] = row
            s.log(f'[{k + 1}/{n}] {row[18]:9s} {row[1][:40]:40s} {row[3]:20.20s} {row[7]:13s} {row[19]}')
    inv = os.path.join(a.out, 'pcdeco_inventory.tsv')
    old = {}
    if os.path.exists(inv) and not a.object:
        for line in open(inv, encoding='utf-8').read().splitlines()[1:]:
            f = line.split('\t')
            old[f[0]] = f
    allrows = dict(old)
    for it, b in items:
        allrows[f'0x{it.guid:08X}'] = rows.get(it.guid) or inv_row(it)
    if not a.object:
        with open(inv, 'w', encoding='utf-8') as f:
            f.write('\t'.join(INV_COLUMNS) + '\n')
            for r in allrows.values():
                f.write('\t'.join(r) + '\n')
    stat = {}
    for it, b in items:
        st = (rows.get(it.guid) or inv_row(it))[18]
        stat[st] = stat.get(st, 0) + 1
    s.log(f'done in {time.time() - t0:.0f} s: ' + ', '.join(f'{v} {k}' for k, v in sorted(stat.items())) +
          ('' if a.object else f'; inventory: {inv}'))


if __name__ == '__main__':
    main()
