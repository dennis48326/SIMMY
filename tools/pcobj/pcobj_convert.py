#!/usr/bin/env python3
"""PC mod: mods. Convert buy-mode objects (Decorative, Surfaces, Seating and beds, Appliances, Lighting, Plumbing, Electronics,
Hobbies, Storage, Kids, Miscellaneous: every buy-mode tab) from the user's own The Sims 2 (PC)
install (base game and every expansion and stuff pack found) into a SIMMY mod folder.

    pcobj_convert.py --pc "<The Sims 2 install>" --gc <GameCube disc image or data folder> --out <mods>
                     --categories decorative,surfaces,seating,appliances,lighting,plumbing,electronics,hobbies,storage,kids,misc
                                                        every
                                                        such object of every pack (or: all, --all-decorative, ...)
    pcobj_convert.py ... --object <GUID | name>  [--base <GameCube object file>]   one object
    pcobj_convert.py ... --batch list.tsv         the GUIDs in the list's first column ([2nd: base])
    pcobj_convert.py --pc "<install>" --list ficus                                 find objects
    pcobj_convert.py --pc "<install>" --categories all --inventory inv.tsv         list, no output
    pcobj_convert.py ... --categories walls,floors   build mode's wall and floor coverings (pcwall.py; "all"
                                                     takes them too): <out>/pcw<k>_<group>/, pcf<k>_<group>/

Each object becomes <out>/pc<k>_<name>_<guid>/ (surfaces: pcs<k>_..., seating: pcc<k>_..., appliances: pca<k>_...,
lighting: pcl<k>_..., plumbing: pcp<k>_..., electronics: pce<k>_..., hobbies: pch<k>_..., storage and kids:
pck<k>_..., misc: pcm<k>_...): mod.ini (a
copy of a GameCube object, its "base": decorative objects by where they go (wall, floor, table top,
ceiling, rug), surfaces a GameCube counter or table of the same footprint, seating and beds a GameCube
chair, sofa, bench or bed of the same footprint and comfort tier (seat height, made / slept-in beds); plumbing the
GameCube toilet, shower, bathtub, sink or hot tub of that kind and tier (seat / water height, dirty and
clogged looks, a counter sink's hole and flange); electronics the GameCube TV, computer, stereo, arcade or
pinball machine, phone, alarm or vending machine that does the same (the GameCube's screens on the PC
screen); hobbies the GameCube skill object or game that does the same; storage the GameCube dresser or
wardrobe (dressers, armoires, lockers: "Change Clothes"), kids the game console (the remote-control toys)
or a decoration (toys, toy boxes; the baby and toddler furniture is left out); misc (the rest of the PC's
Miscellaneous tab) the GameCube drink bar, keg, bubble blower, bonfire, trash can, wardrobe (clothing booths), a
fridge (buffets), the smoke alarm (the sprinkler) or a decoration (cakes, balloons, arches, pet furniture; cars,
shop displays and pet supplies are left out); with the PC object's GUID, name,
description, price, room rating, tiles, a surface's top height and a counter's pieces) and its
resources in the GameCube's own formats (models_*: MODL, float indexed strips per material; shaders_*;
textures_*: CMPR/RGB5A3 with mips; a thumbnail). <out>/pcdeco_inventory.tsv, pcsurf_inventory.tsv and
pcseat_inventory.tsv (and one per further category: pcappl_, pclight_, pcplumb_, pcelec_, pchobby_,
pcstorage_, pckids_, pcmisc_inventory.tsv) list every object found, converted or skipped (and why).

Never put the output in the source tree or share it: it is made of EA's PC game data, converted on the
user's machine from the user's own copy. See docs/mods.md.
"""
import argparse, os, re, shutil, struct, sys, time, zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rcol, sg, gcdata, gctex, pcmesh, pcinstall, pcsurf, pcseat, pcappl, pclight, pcplumb, pcelec, pchobby, pckids, pcmisc  # noqa: E402
import pcwall  # noqa: E402
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
# Beds: which end of the GameCube bed's long axis (model x) the head goes to (+1: -x)
BED_SIGN = int(os.environ.get('PCOBJ_BED_SIGN', '1'))
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

def make_modl(name, parts, lights=(), by_node=False, raw_subs=(), raw_scale=None):
    """parts: [(shader id, P, N, U, T[, node[, colour]])] in GameCube model space. One sub-model; an
    indexed strip (GX_TRIANGLES) per part, split at 21000 triangles (16-bit counts). Fixed point as the
    disc's models (vertex format 6: s16 positions in model units times the model's scale, s16 4.12 uvs)
    when FIXED_POINT and the uvs fit; else float (vertex format 7). A part with a colour (RGBA bytes) has
    vertex colours (its shader draws them without lighting: a lamp's glowing shade). `lights`: the
    model's SimsLightInfo records (0x1C bytes each: a lamp's light). by_node: a sub-model per node (its
    node field set: the node's hidden flag hides it, as a GameCube lamp's on and off shades).
    raw_subs: sub-models copied as stored from a disc model ([(node, bytes)]: a hobby object's props, the
    base's own canvas, guitar or barbell), whose positions are in that model's units: `raw_scale`, its
    scale, becomes this model's (the float strips are written in its units)."""
    split = []
    for sid, P, N, U, T, *rest in parts:
        node = rest[0] if len(rest) > 0 else None
        col = rest[1] if len(rest) > 1 else None
        for k in range(0, len(T), 21000):
            t = T[k:k + 21000]
            used = np.unique(t)
            remap = np.full(len(P), -1, np.int64); remap[used] = np.arange(len(used))
            split.append((sid, P[used], N[used], U[used], remap[t], node, col))
    # an animated appliance's model: every strip bound to a node of the base's character (a matrix record
    # before it: matrix `node` in skinning slot 0; the sub-model has no node of its own)
    bound = any(p[5] is not None for p in split)
    allP = np.concatenate([p[1] for p in split])
    allU = np.concatenate([p[3] for p in split])
    # Fixed point (vertex format 6, as most disc models) needs the model's scale, which the game applies
    # through the object's animation controller (its character): the converted objects have none, so
    # their models stay float (format 7).
    fixed = FIXED_POINT and float(np.abs(allU).max()) < 7.9 and not raw_subs
    extent = max(float(np.abs(allP).max()), 1e-3)
    scale = 2.0 ** np.ceil(np.log2(extent / 32000.0)) if fixed else 1.0
    if raw_subs and raw_scale:
        scale = float(raw_scale)
    body = bytearray()
    body += struct.pack('>I', 0) + b'\0' * 48 + b'\0'
    body += struct.pack('>4I', 0, 0, 0, 0)
    body += struct.pack('>I', len(lights)) + b''.join(lights)
    groups = [(0xFFFFFFFF if bound else 0, split)]
    if by_node and bound:
        nodes = sorted({p[5] or 0 for p in split})
        groups = [(n, [p for p in split if (p[5] or 0) == n]) for n in nodes]
    body += b'\0' + struct.pack('>f', scale) + struct.pack('>I', len(groups) + len(raw_subs))
    for gnode, gsplit in groups:
        body += struct.pack('>II', gnode, len(gsplit))
        for sid, P, N, U, T, node, col in gsplit:
            flags = 0x2A | (0x10 if fixed else 0) | (0x04 if col is not None else 0)  # uvs | normals | indexed (| fixed point) (| colours)
            body += struct.pack('>III', flags, sid, 1) + b'\0' + struct.pack('>f', 1.0)
            if bound:
                body += bytes([1, 0, node or 0, 0])
            body += b'\0' + struct.pack('>I', len(P))
            if fixed:
                q = np.clip(np.round(P / scale), -32767, 32767).astype('>i2')
                body += np.concatenate([q, np.zeros((len(P), 1), '>i2')], 1).tobytes()
                body += np.clip(np.round(U * 4096.0), -32767, 32767).astype('>i2').tobytes()
            else:
                body += np.concatenate([P / scale, np.zeros((len(P), 1))], 1).astype('>f4').tobytes()
                body += U.astype('>f4').tobytes()
            if col is not None:
                body += np.tile(np.asarray(col, np.uint8)[None, :4], (len(P), 1)).tobytes()
            n = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
            nn = np.clip(np.round(np.nan_to_num(n) * 127), -127, 127).astype(np.int8)
            body += np.concatenate([nn, np.full((len(N), 1), 127, np.int8)], 1).tobytes()
            idx = T.reshape(-1).astype(np.uint16)
            dl = bytearray([0x96 if fixed else 0x97]) + struct.pack('>H', len(idx))  # GX_TRIANGLES | GX_VTXFMT6/7
            # an index per attribute: position, normal, (colour,) uv
            dl += np.repeat(idx[:, None], 4 if col is not None else 3, 1).astype('>u2').tobytes()
            dl += b'\0' * ((-len(dl)) % 32)
            body += struct.pack('>IBII', len(idx), 3, len(dl), 3) + dl
            body += b'\x06'
    for rnode, raw in raw_subs:
        body += raw
    # bounds (model units: divided by the scale)
    lo, hi = allP.min(0) / scale, allP.max(0) / scale
    c = (lo + hi) / 2; r = float(np.linalg.norm(allP / scale - c, axis=1).max())
    body += struct.pack('>4f', *c, r) + struct.pack('>6f', *lo, *hi) * 2 + bytes([0, 0, 0, 0])
    return gcdata.make_header(0x3A, 0x4D4F444C, 0xFFFFFFFF, name, bytes(body))


def make_shader(template, name, texture, mode, alpha_ref=0.5, unlit=False):
    """A copy of a disc shader (`template`: its resource bytes) with one layer: `texture`, and
    mode 'opaque' (z test + write), 'cutout' (alpha test), 'blend' (alpha blending + a low alpha test).
    unlit: drawn without lighting (geometry modes 0: the vertex colour times the texture), as the
    GameCube's lamps draw their lit shades (its "neon_white_on")."""
    ver, sig, var, _, off, size = gcdata.data_header(template)
    d = bytearray(template[off:off + size])
    d[0] = 1
    if unlit:
        struct.pack_into('>I', d, 8, 0)
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
            for f in (list(BASES.values()) + list(pcsurf.SURF_BASES.values()) + list(pcseat.SEAT_BASES.values()) +
                      list(pcappl.APPL_BASES.values()) + list(pclight.LIGHT_BASES.values()) + list(pcplumb.PLUMB_BASES.values()) +
                      list(pcelec.ELEC_BASES.values()) + list(pchobby.HOBBY_BASES.values()) +
                      list(pckids.KID_BASES.values()) + list(pcmisc.MISC_BASES.values())):
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
    def __init__(self, cat, guid, category='decorative'):
        self.guid = guid
        self.category = category
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
        self.surface = None
        self.variants = []
        self.model_used = ''
        self.seat = {}
        self.mkind = ''

    def footprint(self):
        if getattr(self, 'fp', None):
            return self.fp
        if not self.tiles:
            return 1, 1
        return max(x for (x, y), g in self.tiles) + 1, max(y for (x, y), g in self.tiles) + 1

    def subsort_name(self):
        if self.category == 'seating':
            return pcseat.SEAT_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category == 'surfaces':
            return pcsurf.SURF_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category == 'appliances':
            return pcappl.APPL_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category == 'lighting':
            return pclight.LIGHT_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category == 'plumbing':
            return pcplumb.PLUMB_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category == 'electronics':
            return pcelec.ELEC_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category == 'hobbies':
            return pchobby.HOBBY_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category in ('storage', 'kids'):
            if self.guid in pckids.RC_GUIDS:
                return pcelec.ELEC_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
            return pckids.MISC_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
        if self.category == 'misc':
            return pcmisc.MISC_SUBSORTS.get(self.subsort, f'0x{self.subsort:x}')
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

def classify(it, P, subsort=None):
    """Where a decorative object goes. `subsort`: the Decorative sub-sort to go by (default the object's)."""
    ss = it.subsort if subsort is None else subsort
    lo, hi = P.min(0), P.max(0)
    w, h = it.footprint()
    name = (it.objname + ' ' + (it.name or '')).lower()
    if 'roof' in name and 'solar' in name:
        return 'roof'
    flat = hi[2] < 0.15 and lo[2] > -0.06
    if ss == 4 or (flat and any(k in it.objname.lower() for k in ('rug', 'mat', 'floor', 'carpet'))):
        return 'rug'
    depth = hi[1] - lo[1]
    against_wall = lo[1] < -0.3 and hi[1] < -0.05 and depth < 0.45 and h == 1
    if ss == 0x20 and lo[1] < -0.3 and h == 1:
        return 'wall'                    # curtains: over a window, on the wall
    if against_wall and (lo[2] > 0.15 or depth < 0.1):
        return 'mirror' if ss == 0x10 else 'wall'
    if lo[1] < -0.38 and lo[2] > 1.0 and h == 1:
        return 'wall'                    # hangs out from the wall (shop signs)
    if lo[2] > 1.7 and hi[2] > 2.2:
        return 'ceiling'
    # goes outside too (rocks, signs, gnomes): a floor object; plants say "outside" for any room
    outdoor_only = bool(it.roomsort & 0x10) and ss != 8
    small = (hi[2] - lo[2]) < 0.75 and max(abs(lo[0]), abs(hi[0]), abs(lo[1]), abs(hi[1])) < 0.47
    if w * h == 1 and small and abs(lo[2]) < 0.1 and not outdoor_only:
        return 'surface_plant' if ss == 8 else 'surface'
    return 'floor_plant' if ss == 8 else 'floor'


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
    if 'additive' in mtype.lower():
        blend = 'additive'     # an AdditiveTextureAlphaBlend material (light cones, glows)
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

def _load(inst, it, names, materials, joints=None):
    """The meshes of the first of `names` that loads, and that name; ([], None, errors) if none. With
    `joints` (a dict) the meshes carry their vertices' joints (pcmesh.load_meshes)."""
    errors = []
    for mname in names:
        if not mname:
            continue
        if joints is not None:
            joints.clear()
        try:
            meshes = pcmesh.load_meshes(inst, mname, materials, log=lambda m: it.notes.append(m) if m not in it.notes else None,
                                        joints=joints)
        except LookupError as e:
            errors.append(str(e)); continue
        except Exception as e:
            errors.append(f'{mname}: {type(e).__name__}: {e}'); continue
        if meshes:
            return meshes, mname, errors
    return [], None, errors


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
    surface = it.category == 'surfaces'
    seating = it.category == 'seating'
    appl = it.category == 'appliances'
    light = it.category == 'lighting'
    plumb = it.category == 'plumbing'
    elec = it.category == 'electronics'
    hobby = it.category == 'hobbies'
    # storage and kids (pckids): a dresser / wardrobe goes the Hobbies way (an animated base, the PC mesh on
    # its body node), a remote-control toy the Electronics way (the game console), the rest are decorations
    kid = it.category in ('storage', 'kids')
    # the rest of the Miscellaneous tab (pcmisc): an animated base the Hobbies way (bars, the keg, fires, trash
    # cans, booths), the smoke alarm the Electronics way (the sprinkler), a fridge the Appliances way (buffets),
    # the rest decorations
    misc = it.category == 'misc'
    hmod = pckids if kid else pcmisc if misc else pchobby
    kidkind = ''
    mkind = ''
    if misc:
        mkind = pcmisc.misc_kind(it)
        it.kind = it.mkind = mkind
        if not mkind:
            it.status, it.reason = 'skipped', f'unknown miscellaneous object ({it.semiglobal or "no semi-global"})'
            return it
        if mkind in pcmisc.SKIP:
            it.status, it.reason = 'skipped', pcmisc.SKIP[mkind]
            return it
        mpath = pcmisc.path(mkind)
        if mpath == 'elec':
            elec = True
        elif mpath == 'appl':
            appl = True
        elif mpath == 'hobby':
            hobby = True
    if kid:
        kidkind = pckids.kid_kind(it)
        it.kind = kidkind
        if not kidkind:
            it.status, it.reason = 'skipped', f'unknown {it.category} object ({it.semiglobal or "no semi-global"})'
            return it
        if kidkind in pckids.SKIP:
            it.status, it.reason = 'skipped', pckids.SKIP[kidkind]
            return it
        if kidkind == 'rc':
            elec = True
        elif kidkind not in pckids.DECOR:
            hobby = True
    txmt_exists = lambda name: inst.find(T_TXMT, name)[0] is not None
    picks = {}
    joints = {}
    meshes_j = None
    if appl:
        kind = pcmisc.PATH_KIND.get(mkind, '') if misc else pcappl.appl_kind(it)
        it.kind = kind
        if not kind:
            it.status, it.reason = 'skipped', f'unknown appliance family ({it.semiglobal or "no semi-global"})'
            return it
        # the stocked / working model (not "...Empty", "...UnderCounter" variants)
        order = it.models[:1] + [m for m in it.models[1:] if not any(k in m.lower() for k in ('empty', 'broken', 'dirty', 'burnt'))]
        meshes_j, used, errors = _load(inst, it, order, materials, joints)
        if misc:
            meshes_j = pcmisc.pre_place(meshes_j, mkind)
        meshes = [m[:6] for m in meshes_j]
    elif plumb:
        kind = pcplumb.plumb_kind(it)
        it.kind = kind
        if not kind:
            it.status, it.reason = 'skipped', f'unknown plumbing family ({it.semiglobal or "no semi-global"})'
            return it
        if kind in pcplumb.SKIP:
            it.status, it.reason = 'skipped', pcplumb.SKIP[kind]
            return it
        order = it.models[:1] + [m for m in it.models[1:] if not any(k in m.lower() for k in ('template', 'broken', 'dirty'))]
        meshes_j, used, errors = _load(inst, it, order, materials, joints)
        # the shapes' own materials can be a state's (a combo's tub "..._dirty"): the clean look by default
        meshes_j = [(pcplumb.swap_material(inst, m[0], '_dirty', '_clean', txmt_exists),) + tuple(m[1:]) for m in meshes_j]
        meshes = [m[:6] for m in meshes_j]
    elif elec:
        kind = kidkind or (pcmisc.PATH_KIND.get(mkind, '') if misc else '') or pcelec.elec_kind(it)
        it.kind = kind
        if not kind:
            it.status, it.reason = 'skipped', f'unknown electronics family ({it.semiglobal or "no semi-global"})'
            return it
        if kind in pcelec.SKIP and not (kid or misc):
            it.status, it.reason = 'skipped', pcelec.SKIP[kind]
            return it
        meshes_j, used, errors = _load(inst, it, pcelec.model_order(it.models), materials, joints)
        meshes_j = [m for m in meshes_j if not pcelec.is_dropped(m[1], m[0])]
        if misc:
            meshes_j = pcmisc.pre_place(meshes_j, mkind)
        if kind == 'computer':
            meshes_j, note = pcelec.open_lid(meshes_j, joints)
            if note:
                it.notes.append(note)
        meshes = [m[:6] for m in meshes_j]
    elif hobby:
        kind = kidkind or mkind or pchobby.hobby_kind(it)
        it.kind = kind
        if not kind:
            it.status, it.reason = 'skipped', f'unknown hobbies family ({it.semiglobal or "no semi-global"})'
            return it
        if kind in pchobby.SKIP and not (kid or misc):
            it.status, it.reason = 'skipped', pchobby.SKIP[kind]
            return it
        meshes_j, used, errors = _load(inst, it, hmod.model_order(it.models), materials, joints)
        meshes_j = [m for m in meshes_j if not hmod.is_dropped(m[1], m[0])]
        meshes = [m[:6] for m in meshes_j]
    elif seating:
        kind, why = pcseat.seat_kind(it)
        it.kind = kind
        if why:
            it.status, it.reason = 'skipped', why
            return it
        main = pcseat.pick_model(it, kind)
        meshes, used, errors = _load(inst, it, [main] + [m for m in it.models if m != main and 'broken' not in m.lower()], materials)
    elif surface:
        kind = pcsurf.surface_kind(it)
        picks = pcsurf.pick_models(it, kind)
        order = [picks.get('main')] + [m for m in it.models if m != picks.get('main') and
                                       not any(k in pcsurf._norm(m) for k in ('corner', 'east', 'west'))]
        meshes, used, errors = _load(inst, it, order, materials)
        picks['main'] = used
    if not (seating or surface or appl or plumb or elec or hobby):
        # (a lamp's second model is the debris it leaves when it breaks on the PC)
        kmod = pckids if kid else pcmisc
        meshes, used, errors = _load(inst, it, kmod.model_order(it.models)[:1] if (kid or misc) else it.models[:1], materials)
        if kid or misc:
            meshes = [m for m in meshes if not kmod.is_dropped(m[1], m[0])]
    if not meshes:
        it.status, it.reason = 'skipped', errors[0] if errors else 'model has no drawable mesh'
        return it
    it.model_used = used
    P_all = np.concatenate([m[2] for m in meshes])
    it.bounds = (P_all.min(0), P_all.max(0))
    W, H = it.footprint()
    seat = {}
    if seating:
        if kind in ('recliner', 'lounge'):
            # a recliner's leg rest tile is only used reclined (its mesh, folded, fits its seat's tile); a
            # lounge chair's legs reach into it. Both are GameCube chairs, which are one tile: the chair
            # trees do not work as a part of a group (TREEASSERT 37 in "Sit"), so the second tile is
            # dropped (the lounge chair's mesh overhangs its front tile)
            W, H = 1, 1
            it.notes.append('second tile dropped (1x1 chair)')
        comfort, energy = pcseat.ratings(it, kind)
        tier_base, tier_rating = pcseat.seat_base(kind, comfort, energy)
        base = base_override or tier_base
        bed = kind.startswith('bed')
        if bed:
            seat['height'] = pcseat.BED_SHEET
            seat['offset'] = pcseat.BED_SHEET - pcseat.GC_MATTRESS
            seat['top'] = pcseat.mattress_height(meshes)
        else:
            centres = [(x, y) for (x, y), g in it.tiles] if (W, H) != (1, 1) else [(0, 0)]
            hs = [h for h in (pcseat.seat_height(meshes, kind, x, y) for x, y in centres) if h]
            if not hs:
                it.status, it.reason = 'skipped', 'no seat found in its mesh'
                return it
            seat['height'] = float(np.median(hs))
            seat['offset'] = pcseat.seat_offset(seat['height'])
        it.surface = seat['height']
        seat['comfort'] = comfort
        # a bed's energy for the catalogue: the PC's 1..8 onto the GameCube beds' 4..10
        seat['energy'] = int(round(4 + (energy - 1) * 6 / 7)) if energy > 0 else -1
        seat['base_rating'] = tier_rating
        seat['seats'] = {'sofa': 3, 'loveseat': 2, 'living2': 2, 'bench': 2, 'bed_double': 2}.get(kind, 1)
        it.seat = seat
    elif surface:
        it.kind = kind
        if kind == 'wallshelf':
            it.status, it.reason = 'skipped', 'hangs on a wall (the GameCube has no wall-mounted surfaces)'
            return it
        if it.bounds[1][2] < 0.12:
            it.status, it.reason = 'skipped', 'a tray that goes on a counter (the GameCube cannot put a surface on another)'
            return it
        base = base_override or pcsurf.surface_base(kind, W, H)
    elif appl:
        base = base_override or pcappl.appl_base(it, kind)[0]
        hi = it.bounds[1]
        # the tiles its mesh covers (PC tile (x, y) has its middle at (x, y)); its OBJD parts can list more
        # (a stove's second part, a food stand's front row)
        W = max(1, int(np.floor(hi[0] + 0.4)) + 1)
        H = max(1, int(np.floor(hi[1] + 0.4)) + 1)
        it.fp = (W, H)
        if kind == 'dishwasher':
            W, H = 1, 2      # the GameCube dishwasher: the machine and the tile in front its door opens over
        elif W * H > 1:
            it.notes.append(f'{W}x{H}: a group of {W * H} copies of the base (each tile works)')
    elif plumb:
        base = base_override or pcplumb.plumb_base(it, kind)[0]
        pinfo = pcplumb.BASE_INFO.get(base, {})
        pw, ph = it.footprint()
        W, H = pinfo.get('fp', (1, 1))
        if (pw, ph) != (W, H):
            it.notes.append(f'{pw}x{ph} on the PC: the base\'s {W}x{H}')
        # the PC mesh over the base's layout (PC object space, the first tile's middle at 0)
        sx = sy = 1.0
        if kind == 'hottub':
            # a 3x3 hot tub over the 2x3 Japanese hot tub: narrowed to its width, its middle on the group's
            sx, sy = W / float(pw), H / float(ph)

        def place(P, N=None):
            P = P.copy()
            if kind in ('tub', 'combo'):
                # the faucet end (PC -y) to the base's (model -y after the mirror): turned half round
                P[:, 0] = -P[:, 0]
                P[:, 1] = (H - 1) - P[:, 1]
            elif kind == 'hottub':
                P[:, 0] = (P[:, 0] - (pw - 1) / 2.0) * sx + (W - 1) / 2.0
                P[:, 1] = (P[:, 1] - (ph - 1) / 2.0) * sy + (H - 1) / 2.0
            if N is None:
                return P
            N = N.copy()
            if kind in ('tub', 'combo'):
                N[:, 0] = -N[:, 0]
                N[:, 1] = -N[:, 1]
            elif kind == 'hottub':
                N[:, 0] /= sx
                N[:, 1] /= sy
            return P, N

        placed = []
        for m in meshes_j:
            P2, N2 = place(m[2], m[3])
            placed.append((m[0], m[1], P2, N2) + tuple(m[4:]))
        # a tub's / sink's / shower's water (the PC fills it by its own animation): a bathtub's on the
        # base's water node (shown, filling, while it is in use), else left out; a hot tub keeps its
        water_j = [m for m in placed if kind != 'hottub' and pcplumb.is_water(m[0], m[1])]
        meshes_j = [m for m in placed if not (kind != 'hottub' and pcplumb.is_water(m[0], m[1]))]
        if not pinfo.get('water'):
            water_j = []
        elif water_j:
            # the PC's water is a small sheet its own animation spreads over the tub as it fills (the
            # bathtub template's water level: x -0.29..0.30, y -0.23..1.27 at 0.45); the base's water node
            # only lifts it (0.39..0.47): spread over the tub here
            lo = np.min([m[2].min(0) for m in water_j], 0); hi = np.max([m[2].max(0) for m in water_j], 0)
            want_lo, want_hi = place(np.asarray([[-0.29, -0.23, 0.0], [0.30, 1.27, 0.0]]))
            want_lo, want_hi = np.minimum(want_lo, want_hi), np.maximum(want_lo, want_hi)
            spread = []
            for m in water_j:
                P = m[2].copy()
                for a in (0, 1):
                    P[:, a] = want_lo[a] + (P[:, a] - lo[a]) * (want_hi[a] - want_lo[a]) / max(hi[a] - lo[a], 1e-6)
                spread.append((m[0], m[1], P) + tuple(m[3:]))
            water_j = spread
        meshes = [m[:6] for m in meshes_j]
        joints = {j: place(np.asarray([o], np.float64))[0] for j, o in joints.items()}
        P_all = np.concatenate([m[2] for m in meshes])
        it.bounds = (P_all.min(0), P_all.max(0))
        seat = {}
        flange_j = []
        if kind == 'sink_counter':
            # the counter's hole it needs: its cut-out plane ("..._null" subset), the PC's mask for the
            # counter top under it
            # its rim: what it has at the counter top's height (not its "..._null" cut-out plane, a mask
            # the PC draws over the counter); the wide hole when the rim covers it
            rim = [m[2][(m[2][:, 2] > -0.03) & (m[2][:, 2] < 0.06)] for m in placed
                   if not (m[1] or '').lower().endswith('_null') and not pcplumb.is_water(m[0], m[1])]
            rim = np.concatenate([r for r in rim if len(r)] or [np.zeros((1, 3))])
            rlo, rhi = rim.min(0), rim.max(0)
            wx0, wy0, wx1, wy1 = pcsurf.SINK_HOLES['sink_wide']
            wide = rlo[0] <= wx0 + 0.01 and rhi[0] >= wx1 - 0.01 and rlo[1] <= wy0 + 0.01 and rhi[1] >= wy1 - 0.01
            it.notes.append(f'cut-out {"wide" if wide else "narrow"} (rim x {rlo[0]:.2f}..{rhi[0]:.2f} y {rlo[1]:.2f}..{rhi[1]:.2f})')
            pinfo = dict(pinfo, hole='wide' if wide else 'narrow')
            # In a GameCube counter the sink stands in its cut-out top, whose hole fits the GameCube's own
            # sinks (x -0.46..0.46, y -0.28..0.43 here): a flange in the rim's colour fills the gap
            # between the PC rim and that hole (model_flange: drawn by such a counter only, at its top,
            # 1.0 over the floor)
            body = [m for m in placed if not (m[1] or '').lower().endswith('_null') and not pcplumb.is_water(m[0], m[1])]
            near = [(m, (m[2][:, 2] > -0.03) & (m[2][:, 2] < 0.06)) for m in body]
            m0, sel0 = max(near, key=lambda ms: int(ms[1].sum()))
            uv = np.median(m0[4][sel0], 0) if sel0.any() else np.zeros(2)
            ox0, oy0, ox1, oy1 = -0.47, -0.29, 0.47, 0.432
            ix0, iy0, ix1, iy1 = rlo[0] + 0.02, rlo[1] + 0.02, rhi[0] - 0.02, rhi[1] - 0.02
            z = 1.003
            Pf = np.asarray([[ox0, oy0, z], [ox1, oy0, z], [ox1, oy1, z], [ox0, oy1, z],
                             [ix0, iy0, z], [ix1, iy0, z], [ix1, iy1, z], [ix0, iy1, z]], np.float32)
            Tf = np.asarray([[0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]], np.int64)
            Nf = np.tile(np.asarray([[0, 0, 1]], np.float32), (8, 1))
            Uf = np.tile(np.asarray([uv], np.float32), (8, 1))
            flange_j = [(m0[0], 'flange', Pf, Nf, Uf, Tf)]
        if kind in ('toilet', 'stall'):
            sh = pcplumb.seat_height(meshes)
            if sh:
                seat['height'] = sh
                seat['offset'] = float(np.clip(sh - pinfo.get('seat_h', 0.42), -0.15, 0.2))
        elif kind == 'hottub':
            wl = pcplumb.water_level(meshes)
            if wl is not None:
                seat['height'] = wl
                # a raised hot tub: its sims drawn at its water (the base's is at the floor)
                seat['offset'] = float(wl - pinfo.get('water_z', 0.0)) if wl > 0.25 else 0.0
        if seat:
            it.surface = seat['height']
            it.seat = {'offset': seat['offset']}
    elif elec:
        pw, ph = it.footprint()
        base = base_override or (pckids.kid_base(it, kind, meshes) if kid else pcelec.elec_base(it, kind, it.tiles))
        einfo = pcelec.BASE_INFO.get(base, {})
        btiles = einfo.get('tiles')
        W, H = pw, ph
        elec_parts = False
        if btiles:
            bw, bh = max(x for x, y in btiles) + 1, max(y for x, y in btiles) + 1
            if pw <= bw and ph <= bh:
                # the base's parts, tile for tile (a TV's two parts on its one tile); the PC mesh in the
                # middle of the base's footprint (a 2x2 DJ booth on the 3x2 one)
                W, H = bw, bh
                elec_parts = True
                if (pw, ph) != (W, H):
                    it.notes.append(f'{pw}x{ph} on the PC: the base\'s {W}x{H}, the mesh in its middle')
            else:
                it.notes.append(f'{pw}x{ph}: larger than the base ({bw}x{bh}): a part per tile, copies of the base\'s')
        dx, dy = (W - pw) / 2.0, (H - ph) / 2.0
        if dx or dy:
            meshes = [(m[0], m[1], m[2] + np.array([dx, dy, 0.0]), m[3], m[4], m[5]) for m in meshes]
            P_all = np.concatenate([m[2] for m in meshes])
            it.bounds = (P_all.min(0), P_all.max(0))
    elif hobby:
        base = base_override or (pckids.kid_base(it, kind, meshes) if kid else pcmisc.misc_base(it, kind, meshes) if misc
                                 else pchobby.hobby_base(it, kind))
        hinfo = hmod.BASE_INFO.get(base, {})
        btiles = hinfo.get('tiles') or [(0, 0)]
        W, H = max(x for x, y in btiles) + 1, max(y for x, y in btiles) + 1
        # the PC mesh over the base's footprint (its tiles centred, turned onto the base's long axis)
        meshes, pfp, note = hmod.place(meshes, kind, W, H, base) if (kid or misc) else hmod.place(meshes, kind, W, H)
        it.notes.append(note)
        P_all = np.concatenate([m[2] for m in meshes])
        it.bounds = (P_all.min(0), P_all.max(0))
        hobby_parts = len(btiles) > 1
    elif light:
        kind = pclight.light_kind(it, P_all)
        it.kind = kind
        base = base_override or pclight.LIGHT_BASES[kind]
        if W * H > 1:
            # the GameCube's lamps are one tile (a group of lamp copies would be as many lamps, each with
            # its own switch): the model over one tile, centred on it
            it.notes.append(f'{W}x{H} on the PC: one tile, the model centred on it')
    else:
        # (a kids', storage or misc decoration: placed by its mesh alone, its Misc sub-sort is not a Decorative one)
        kind = classify(it, P_all, subsort=0 if (kid or misc) else None)
        if kid:
            it.notes.append(pckids.describe(kidkind))
        if misc:
            it.notes.append(pcmisc.describe(mkind))
        if ((kid and kidkind in pckids.FLOOR_ONLY) or (misc and mkind in pcmisc.FLOOR_ONLY)) and kind != 'floor':
            it.notes.append(f'on the floor (placed as {kind} by its mesh)')
            kind = 'floor'
        if kind == 'roof':
            it.status, it.reason, it.kind = 'skipped', 'goes on roofs (no roof placement on the GameCube)', kind
            return it
        it.kind = kind
        base = base_override or BASES[kind]
    it.base = base
    bdefs = s.gco.files.get(base) if gc is not None else [{'master': 1 if kind in ('rug', 'ceiling') or ((surface or seating or appl or plumb) and W * H > 1) else 0}]
    if not bdefs:
        it.status, it.reason = 'failed', f'GameCube base {base} not found'
        return it
    group_base = any(d['master'] != 0 for d in bdefs)
    multi = (W * H > 1 or group_base) and not light
    if elec and kind == 'clock_wall':
        multi = False
    if hobby and not hobby_parts:
        multi = False
    # surfaces: the top's height over each tile (PC tile (x, y), its middle at (x, y))
    heights = {}
    if surface:
        for (x, y) in ([xy for xy, g in it.tiles] or [(0, 0)]):
            heights[(x, y)] = pcsurf.surface_height(meshes, x, y, kind=kind)
        known = [v for v in heights.values() if v]
        if not known:
            it.status, it.reason = 'skipped', 'no top to put things on found in its mesh'
            return it
        top = max(known) if kind not in ('desk', 'misc') else max(set(round(v, 2) for v in known), key=lambda v: sum(1 for k in known if abs(k - v) < 0.03))
        heights = {k: (v if v else top) for k, v in heights.items()}
        it.surface = top
    # counter pieces: (key, meshes); ends: which side (model x) each closes
    variants = []
    if surface and kind in ('counter', 'island'):
        loaded = {}
        for k in ('corner', 'end_e', 'end_w'):
            if picks.get(k):
                m, _, _ = _load(inst, it, [picks[k]], materials)
                if m:
                    loaded[k] = m
        if 'corner' in loaded:
            variants.append(('corner', loaded['corner']))
        if 'end_e' in loaded and 'end_w' in loaded:
            se = pcsurf.end_side(loaded['end_e'], meshes); sw = pcsurf.end_side(loaded['end_w'], meshes)
            if not (se and sw and se != sw):
                se, sw = 1, -1   # the counter globals' order: the first end closes +x
            ends = {se: loaded['end_e'], sw: loaded['end_w']}
            variants.append(('end_px', ends[1]))
            variants.append(('end_nx', ends[-1]))
            variants.append(('ends', ends[1] + pcsurf.mesh_minus(ends[-1], meshes)))
        if W * H == 1 and it.surface:
            # with a sink in it (a GameCube or converted counter sink on its tile): the top with a hole
            # (ISimsCounterTopObject::SetObjOrient; the GameCube's own counters have cut-out models)
            for k, rect in pcsurf.SINK_HOLES.items():
                holed = pcsurf.cut_hole(meshes, it.surface, rect)
                if holed:
                    variants.append((k, holed))
            # a GameCube sink: drawn moved back (sink_shift) in a hole of its size; its basins go 0.2
            # down: whatever stands in that band under the hole (an island's curved apron) is cut too
            shift, rect = pcsurf.gc_sink_fit(meshes, it.surface)
            holed = pcsurf.cut_hole(meshes, it.surface, rect, band=0.42) if rect else None
            if holed:
                variants.append(('sink_gc', holed))
                it.sink_shift = shift
    # the GameCube draws a group's model at the middle of its tiles; wall objects 0.45 off the tile's
    # middle toward the wall. PC: the first tile's middle, the wall at -y; the GameCube's model space is
    # the PC's mirrored in y (its world swaps x and y).
    cx, cy = (W - 1) / 2.0, (H - 1) / 2.0
    wall = kind in ('wall', 'mirror') or (elec and kind in pcelec.WALL_SHIFT)
    if light and W * H > 1:
        # a multi-tile PC lamp on one tile: its mesh's middle over the tile (a wall lamp's back stays at
        # the wall)
        lo, hi = it.bounds
        cx = float(lo[0] + hi[0]) / 2
        cy = 0.0 if wall else float(lo[1] + hi[1]) / 2
    bed_variants = {}
    if seating and kind.startswith('bed'):
        # PC: head at y 0, the bed along +y over tiles y 0..2, x across. GameCube: the bed's parts run
        # along sub x (0..2) and its sides are sub y (double: 0 left, 1 right; single: 1); its models lie
        # along model x. Turned a quarter (the head to model -x), then the usual mirror (gc_parts).
        sgn = BED_SIGN
        def turn(ms):
            out = []
            for (mat, sub, P, N, U, T) in ms:
                Q = P.copy(); M = N.copy()
                Q[:, 0] = sgn * (P[:, 1] - 1.0) + 1.0
                Q[:, 1] = -sgn * (P[:, 0] - (W - 1) / 2.0) + (2 - 1) / 2.0
                M[:, 0] = sgn * N[:, 1]; M[:, 1] = -sgn * N[:, 0]
                out.append((mat, sub, Q, M, U, T))
            return out
        made, slept = pcseat.bed_models(meshes)
        bed_variants = {'made': turn(made), 'slept': turn(slept)}
        meshes_pc = meshes
        meshes = bed_variants['made']
        W, H = 3, 2
        cx, cy = 1.0, 0.5
    tag = re.sub(r'[^a-z0-9]+', '', (it.name or it.objname).lower())[:20] or 'object'
    prefix = (f'pck{pckids.sort_key(kidkind, base)}' if kid
              else f'pcm{pcmisc.sort_key(mkind, base)}' if misc
              else f'pcs{pcsurf.SURF_SORT.get(kind, "7")}' if surface else f'pcc{pcseat.SEAT_SORT.get(kind, "8")}' if seating
              else f'pca{pcappl.APPL_SORT.get(kind, "6")}' if appl else f'pcl{pclight.LIGHT_SORT.get(kind, "6")}' if light
              else f'pcp{pcplumb.PLUMB_SORT.get(kind, "6")}' if plumb
              else f'pce{pcelec.ELEC_SORT.get(kind, "8")}' if elec
              else f'pch{pchobby.HOBBY_SORT.get(kind, "9")}' if hobby
              else f'pc{SORT_KEY.get(it.subsort, "7")}')
    folder = os.path.join(s.out or '.', f'{prefix}_{tag}_{it.guid:08x}')
    it.out = os.path.basename(folder)
    taken = set(); files = {}
    texcache = {}; skipped = set()

    shadercache = {}
    glowing = set()

    def gc_parts(meshes, rendered=None, emissive=(), dim=None):
        """`emissive`: materials drawn without lighting (a lamp's glowing parts; their shader ids go into
        `glowing`); `dim`: {material: alpha scale} (a lamp's additive light effects, blended here)."""
        parts = []
        for k, (mat, subset, P, N, U, T) in enumerate(meshes):
            if mat in skipped or not len(T):
                continue
            if mat not in texcache:
                img, dxt1, mode, desc = material_of(inst, mat, s.texsize)
                if mode == 'skip':
                    skipped.add(mat)
                    continue
                if dim and mat in dim and mode == 'blend':
                    img = img.copy()
                    img[..., 3] = (img[..., 3].astype(np.float32) * dim[mat]).astype(np.uint8)
                    dxt1 = None
                tid = new_id(gc, 'textures', f'pcmod_{it.guid:08x}_tex{len(texcache)}', taken)
                data, fmt, size = gctex.make_txfl(f'pc_{tag}_{len(texcache)}', img,
                                                  'cutout' if mode == 'cutout' else ('blend' if mode == 'blend' else 'opaque'),
                                                  s.texsize, dxt1)
                files[f'textures_{tid:08X}.res'] = data
                texcache[mat] = (tid, img, mode)
                it.textures.append(f'{desc}:{fmt}{size[0]}x{size[1]}:{mode}')
            tid, img, mode = texcache[mat]
            unlit = mat in emissive
            if (mat, unlit) not in shadercache:
                n = len(shadercache)
                sid = new_id(gc, 'shaders', f'pcmod_{it.guid:08x}_shader{n}' + ('_glow' if unlit else ''), taken)
                if gc is not None:
                    files[f'shaders_{sid:08X}.res'] = make_shader(s.shader_template, f'pc_{tag}_{n}', tid, mode, unlit=unlit)
                shadercache[(mat, unlit)] = sid
                if unlit:
                    glowing.add(sid)
            sid = shadercache[(mat, unlit)]
            G = P.copy()
            G[:, 0] -= cx
            G[:, 1] = -(G[:, 1] - cy)
            if wall:
                G[:, 1] -= 0.45
            GN = N.copy(); GN[:, 1] = -GN[:, 1]
            parts.append((sid, G, GN, U.copy(), T[:, ::-1]))
            if rendered is not None:
                rendered.append((P, N, U, T, img))
        return parts

    # a lamp: its parts by when they show (pclight.material_role), its light
    lamp = None
    if light:
        dz = 0.0
        lo_z, hi_z = float(it.bounds[0][2]), float(it.bounds[1][2])
        if kind == 'ceiling':
            # hangs from the ceiling: its top at the top of the walls (3 tiles over its floor, as the
            # GameCube's walls); a chandelier for two storeys comes down no lower than 1.9 (it reaches
            # into the floor above)
            dz = pclight.WALL_TOP - hi_z
            if dz < 0 and lo_z + dz < 1.9:
                dz = min(0.0, 1.9 - lo_z)
            if abs(dz) > 0.01:
                it.notes.append(f'{"raised" if dz > 0 else "lowered"} {abs(dz):.2f} to the ceiling')
            if hi_z + dz > pclight.WALL_TOP + 0.05:
                it.notes.append(f'reaches {hi_z + dz - pclight.WALL_TOP:.2f} above the ceiling')
        elif kind in ('outdoor', 'floor', 'table') and lo_z < -0.05:
            # sits on a fence post or a deck post on the PC: on the ground here
            dz = -lo_z
            it.notes.append(f'raised {dz:.2f} onto the ground (a post lamp: the GameCube has no fence posts)')
        body_m, off_m, on_m, glow, dim = [], [], [], set(), {}
        roles = {}
        for (mat, sub, P, N, U, T) in meshes:
            role, lit_m, unlit_m = pclight.material_role(inst, sub, mat)
            P = P + np.array([0.0, 0.0, dz])
            roles[role] = roles.get(role, 0) + 1
            if role == 'body':
                body_m.append((lit_m or unlit_m or mat, sub, P, N, U, T))
            if role in ('switch', 'off'):
                off_m.append((unlit_m, sub, P, N, U, T))
            if role in ('switch', 'on'):
                on_m.append((lit_m, sub, P, N, U, T))
                if pclight.glows(inst, sub, lit_m):
                    glow.add(lit_m)
                    a = pclight.effect_alpha(inst, lit_m, sub)
                    if a < 1.0:
                        dim[lit_m] = a
        if not on_m:
            it.notes.append('nothing of it changes when lit (the PC lamp only casts light)')
        lights = pclight.pc_lights(inst, used)
        lp, lcol, linten, lnotes = pclight.lamp_light(lights, meshes, kind)
        lp = lp + np.array([0.0, 0.0, dz])
        it.notes += lnotes
        lamp = {'body': body_m, 'off': off_m, 'on': on_m, 'glow': glow, 'dim': dim, 'pos': lp, 'col': lcol, 'intensity': linten,
                'pcl': len(lights),
                'roles': [f'{k}:{v}' for k, v in sorted(roles.items())] + ([f'glow:{len(glow)}'] if glow else [])}
        meshes = body_m + off_m       # the lamp switched off: the still model (catalogue preview, thumbnail)
    rendered = []
    parts = gc_parts(meshes, rendered)
    if not parts:
        it.status, it.reason = 'skipped', 'model has no drawable mesh'
        return it
    # seating: per-state models (a dining chair pulled in; a bed made / slept in, a double's two halves)
    seat_models = {}
    if seating and kind.startswith('bed'):
        slept = gc_parts(bed_variants['slept'])
        def half(ps, right):
            out = []
            for sid, G, GN, U, T in ps:
                keep = (G[T].mean(1)[:, 1] >= 0) == right
                if keep.any():
                    out.append((sid, G, GN, U, T[keep]))
            return out
        if kind == 'bed_double':
            for side, right in (('right', True), ('left', False)):
                seat_models[f'{side}_made'] = half(parts, right)
                seat_models[f'{side}_slept'] = half(slept, right)
        else:
            seat_models['slept'] = slept
        # the thumbnail from the PC mesh as it stands (head at the back)
        rendered = []
        gc_parts(meshes_pc, rendered)
    elif seating and kind in ('dining', 'barstool'):
        # pulled in to the table (state 1): the GameCube's chair slides toward the front (model -y)
        seat_models['in'] = [(sid, G + np.array([0.0, -pcseat.GC_PULL_IN, 0.0]), GN, U, T) for sid, G, GN, U, T in parts]
    it.variants = list(seat_models)
    # an appliance: the model its states show, each PC part on its node of the base's skeleton (doors,
    # racks), and the nodes' pivots (the PC hinges) in model space
    anim_parts, node_pivots = None, {}
    if appl:
        bound, pivots = pcappl.bind_parts(meshes_j, joints, base)
        anim_parts = []
        for m6, node in bound:
            for p in gc_parts([m6]):
                anim_parts.append(p + (node,))
        for node, (pv, spec, j) in sorted(pivots.items()):
            node_pivots[node] = ((pv[0] - cx, -(pv[1] - cy), pv[2]), spec)
            it.variants.append(f'{j}>node{node}{"(" + spec + ")" if spec else ""}')
        moving = sorted({j for j in joints if pcappl.part_role(j) and not any(j == p[2] for p in pivots.values())})
        if moving:
            it.notes.append('moves on the PC, still here: ' + ','.join(moving))
        if kind == 'dishwasher':
            it.surface = pcsurf.surface_height(meshes, 0, 0, kind='counter') or 0.95
    plumb_states = {}
    if plumb:
        # the body on the root node, a toilet's seat on the base's seat node (turning about the PC
        # hinge), a bathtub's water on its water node; per state: the PC's clean, dirty and clogged looks
        seat_node = pinfo.get('seat')
        def bind(ms):
            out = []
            for m in ms:
                J = m[6] if len(m) > 6 else None
                T = m[5]
                if seat_node is not None and J is not None and any(pcplumb.is_seat(j) for j in set(J.tolist())):
                    keys = np.asarray([seat_node if pcplumb.is_seat(j) else 0 for j in J.tolist()])
                    tri = keys[T[:, 0]]
                    for node in sorted(set(tri.tolist())):
                        out.append(((m[0], m[1], m[2], m[3], m[4], T[tri == node]), int(node)))
                else:
                    out.append((m[:6], 0))
            out += [(m[:6], pinfo['water']) for m in water_j]
            return out
        seat_joints = sorted({j for j in joints if pcplumb.is_seat(j)})
        if seat_node is not None and seat_joints:
            pv = joints[seat_joints[0]]
            node_pivots[seat_node] = ((float(pv[0] - cx), float(-(pv[1] - cy)), float(pv[2])), '')
            it.variants.append(f'{seat_joints[0]}>node{seat_node}')
        if water_j:
            it.variants.append(f'water>node{pinfo["water"]}')

        def looks(ms, swaps):
            out = []
            for m in ms:
                mat = m[0]
                for a, b in swaps:
                    mat = pcplumb.swap_material(inst, mat, a, b, txmt_exists)
                out.append((mat,) + tuple(m[1:]))
            return out
        variant_meshes = {'clean': meshes_j}
        dirty = looks(meshes_j, [('_clean', '_dirty')])
        if any(a[0] != b[0] for a, b in zip(dirty, meshes_j)):
            variant_meshes['dirty'] = dirty
        if pinfo.get('clogged'):
            clog = looks(variant_meshes.get('dirty', meshes_j), [('water_water', 'water_clogged')])
            if any(a[0] != b[0] for a, b in zip(clog, meshes_j)):
                variant_meshes['clogged'] = clog
        for look, ms in variant_meshes.items():
            ps = []
            for m6, node in bind(ms):
                for p in gc_parts([m6]):
                    ps.append(p + (node,))
            plumb_states[look] = ps
        anim_parts = plumb_states.pop('clean')
        it.variants += [f'{k}:{len(pinfo.get(k, ()))}st' for k in plumb_states if pinfo.get(k)]
        moving = sorted(j for j in joints if j and not pcplumb.is_seat(j) and any(w in j for w in ('door', 'knob', 'handle', 'rot')) and
                        j not in ('root_rot',))
        if moving:
            it.notes.append('moves on the PC, still here: ' + ','.join(moving))
    light_recs = []
    if lamp:
        bnode = pclight.BASE_NODES.get(base) or pclight.BASE_NODES[pclight.LIGHT_BASES[kind]]
        off_node, on_node = bnode
        anim_parts = [p + (0,) for p in gc_parts(lamp['body'])]
        anim_parts += [p + (off_node,) for p in gc_parts(lamp['off'])]
        on_parts = []
        for m6 in lamp['on']:
            for p in gc_parts([m6], emissive=lamp['glow'], dim=lamp['dim']):
                on_parts.append(p + (on_node, pclight.glow_color(inst, m6[0], m6[1]) if p[0] in glowing else None))
        anim_parts += on_parts
        # the light: the base model's (its node, flags, intensity: the base's animations switch it), at the
        # PC light's place, colour and reach
        P = lamp['pos']
        lp = (float(P[0] - cx), float(-(P[1] - cy)) - (0.45 if wall else 0.0), float(P[2]))
        base_rec, base_fo = None, 4.0
        if gc is not None:
            bm = [d['model'] for d in s.gco.files.get(base, []) if d.get('model')]
            recs = gcdata.modl_lights(gc.get('models', bm[0])) if bm else []
            if recs:
                base_rec = recs[0]
                base_fo = struct.unpack_from('>f', base_rec, 0x18)[0]
        fo = pclight.falloff(base_fo, lamp['intensity'])
        if base_rec is not None:
            node = struct.unpack_from('>I', base_rec, 4)[0]
            light_recs = [pclight.light_record(base_rec, node, lamp['col'], fo)]
        lamp['mod'] = (lp, lamp['col'], fo)
        it.surface = None
        it.variants += lamp['roles']
        it.variants.append(f'light {lp[0]:.2f},{lp[1]:.2f},{lp[2]:.2f} rgb {lamp["col"][0]:.2f},{lamp["col"][1]:.2f},'
                           f'{lamp["col"][2]:.2f} falloff {fo:.2f} (PC {lamp["intensity"]:.2f}, {lamp["pcl"]} light(s))')
    elec_ini = {}
    if elec and kind != 'clock_wall' and gc is not None:
        # the base's skeleton, states and effects; the PC body on the base's body node, a phone's handset
        # on its handset node, the screen as the base makes its own (pcelec)
        bd = [d for d in s.gco.files[base] if d['model'] and d['states']][0]
        strips = gcdata.modl_strips(gc.get('models', bd['model']))
        mode = einfo.get('screens')
        screens, body_node = pcelec.base_screens(strips, bd['states'], mode or 'none')
        if 'body' in einfo:
            # the base's largest part is not its body (the game console's: its hologram, node 0x45, drawn only
            # while a game runs): the node the base gives
            body_node = einfo['body']
        if not mode:
            screens = []
        hnode = einfo.get('handset')
        anim_parts = []
        scr = [m for m in meshes if pcelec.is_screen(m[1], m[0])]
        hand = []
        for m6 in meshes:
            if any(m6 is x for x in scr):
                continue
            node = hnode if hnode is not None and pcelec.is_handset(m6[1]) else body_node
            for p in gc_parts([m6]):
                anim_parts.append(p + (node,))
                if node == hnode and hnode is not None:
                    hand.append(p[1])
        if hand:
            c = np.concatenate(hand)
            c = (c.min(0) + c.max(0)) / 2
            node_pivots[hnode] = ((float(c[0]), float(c[1]), float(c[2])), '')
            it.variants.append(f'handset>node{hnode}')
        pc_screen = [p for m6 in scr for p in gc_parts([m6])]
        off_sid = None
        if pc_screen:
            allG = np.concatenate([p[1] for p in pc_screen])
            box = (allG.min(0), allG.max(0))
            if not screens:
                anim_parts += [p + (body_node,) for p in pc_screen]
            # the PC's own switched-off screen, unless its material is the PC's bare "screen" placeholder
            # (the PC paints that one at run time): then the base's off screen
            own_off = not all((m6[0] or '').lower() == 'screen' for m6 in scr)
            for node, sh, coef, up, off in screens:
                for sid, G, GN, U, T in pc_screen:
                    if off and own_off:
                        anim_parts.append((sid, G, GN, U, T, node))
                    else:
                        anim_parts.append((sh, G, GN, pcelec.apply_uv(G, coef, up, box), T, node, (128, 128, 128, 128)))
            off_sid = pc_screen[0][0]
            it.variants.append(f'screen>{mode or "body"}:' + ','.join(f'{n}{"off" if o else ""}' for n, sh, cf, up, o in screens))
            if mode == 'swap':
                # the base's switched-off states (state 0's replacement) show the PC screen
                st = bd['states']
                offs = [k for k, x in enumerate(st) if x[3] and x[4] == st[0][4]]
                elec_ini['state_shaders'] = ' '.join(f'{k}:0x{off_sid:08X}' for k in offs)
                # the base model's light (a TV lights its room when on: its animations switch it) in front of
                # the PC screen
                bm = [d['model'] for d in s.gco.files.get(base, []) if d.get('model')]
                light_recs = gcdata.modl_lights(gc.get('models', bm[0])) if bm else []
                if light_recs:
                    cpos = (box[0] + box[1]) / 2
                    elec_ini['light'] = f'{cpos[0]:.3f},{cpos[1] - 0.3:.3f},{cpos[2]:.3f}'
        elif mode:
            it.notes.append('no screen subset: the base\'s screens are not shown')
        elec_ini['body_node'] = body_node
    hobby_props = ([], None)
    if hobby and gc is not None and not hmod.BASE_INFO.get(base, {}).get('static'):
        # the base's skeleton, states and effects; the PC mesh on the base's body node, the base's own props
        # (sub-models of its prop nodes, as stored) where its animation puts them
        bd = [d for d in s.gco.files[base] if d['model'] and d['states']][0]
        bmodel = gc.get('models', bd['model'])
        strips = gcdata.modl_strips(bmodel)
        props = set(hmod.BASE_INFO.get(base, {}).get('props', ()))
        body = max([st for st in strips if st['node'] not in props] or strips, key=lambda st: len(st['P']))
        body_node = body['node'] if body['node'] != 0xFFFFFFFF else 0
        body_node = hmod.BASE_INFO.get(base, {}).get('body', body_node)
        anim_parts = [p + (body_node,) for p in gc_parts(meshes)]
        for n in hmod.BASE_INFO.get(base, {}).get('also_nodes', ()):
            # the base shows its body on one of these nodes at a time (the wardrobe: its facade while idle, its
            # body while in use): the PC mesh on each
            anim_parts += [p + (n,) for p in gc_parts(meshes)]
            it.variants.append(f'body>node{n}')
        if props:
            scale, subs = gcdata.modl_submodels(bmodel)
            raw = [(n, r) for n, r in subs if n in props]
            hobby_props = (raw, scale)
            it.variants.append('props:' + ','.join(str(n) for n, r in raw))
        it.variants.append(f'body>node{body_node}')
    it.verts = sum(len(p[1]) for p in parts); it.tris = sum(len(p[4]) for p in parts)
    vparts = [(k, gc_parts(m)) for k, m in variants]
    vparts = [(k, p) for k, p in vparts if p]
    it.variants = it.variants + [k for k, p in vparts]
    if not write:
        it.status = 'ok (dry run)'
        return it
    sids = {}
    for k, p in seat_models.items():
        if not p:
            continue
        sid_ = new_id(gc, 'models', f'pcmod_{it.guid:08x}_model_{k}', taken)
        files[f'models_{sid_:08X}.res'] = make_modl(f'pc_{tag}_{k}', p)
        sids[k] = sid_
    mid = new_id(gc, 'models', f'pcmod_{it.guid:08x}_model', taken)
    files[f'models_{mid:08X}.res'] = make_modl(f'pc_{tag}', parts)
    aid = None
    if anim_parts:
        aid = new_id(gc, 'models', f'pcmod_{it.guid:08x}_model_anim', taken)
        files[f'models_{aid:08X}.res'] = make_modl(f'pc_{tag}_anim', anim_parts, light_recs, by_node=bool(lamp) or elec or hobby,
                                                   raw_subs=hobby_props[0], raw_scale=hobby_props[1])
    plumb_ids = {}
    if plumb and flange_j:
        fp = gc_parts(flange_j)
        if fp:
            plumb_ids['flange'] = new_id(gc, 'models', f'pcmod_{it.guid:08x}_model_flange', taken)
            files[f'models_{plumb_ids["flange"]:08X}.res'] = make_modl(f'pc_{tag}_flange', fp)
    for k, p in plumb_states.items():
        if p:
            plumb_ids[k] = new_id(gc, 'models', f'pcmod_{it.guid:08x}_model_anim_{k}', taken)
            files[f'models_{plumb_ids[k]:08X}.res'] = make_modl(f'pc_{tag}_anim_{k}', p)
    vids = {}
    for k, p in vparts:
        vid = new_id(gc, 'models', f'pcmod_{it.guid:08x}_model_{k}', taken)
        files[f'models_{vid:08X}.res'] = make_modl(f'pc_{tag}_{k}', p)
        vids[k] = vid
    timg = render_thumb(rendered, 64)
    ttid = new_id(gc, 'textures', f'pcmod_{it.guid:08x}_thumbtex', taken)
    files[f'textures_{ttid:08X}.res'] = gctex.make_thumb_txfl(f'pc_{tag}_thumb', timg)
    thumb_id = new_id(gc, 'shaders', f'pcmod_{it.guid:08x}_thumb', taken)
    files[f'shaders_{thumb_id:08X}.res'] = make_shader(s.thumb_template, f'pc_{tag}_thumb', ttid, 'blend')
    # tiles: (x, y) as the GameCube's sub index (y mirrored with the model), with GUIDs (and a
    # surface's height over the tile)
    tiles = []
    if seating and kind.startswith('bed'):
        # the GameCube bed's parts (base_parts): single (0..2, 1), double (0..2, 0..1); the PC part at the
        # same place along the bed (head at x 0 or 2) and side gives its GUID
        gtaken = set()
        pcg = dict(it.tiles)
        rows = (1,) if kind == 'bed_single' else (0, 1)
        for gx in range(3):
            for gy in rows:
                py = gx if BED_SIGN > 0 else 2 - gx
                px = 0 if kind == 'bed_single' else ((1 - gy) if BED_SIGN > 0 else gy)
                g = pcg.get((px, py)) or tile_guid(it.guid, gx, gy, s.gco.guids, gtaken)
                if g in s.gco.guids or g == it.guid:
                    g = tile_guid(it.guid, gx, gy, s.gco.guids, gtaken)
                tiles.append((gx, gy, g, None))
    elif multi and hobby:
        # the base's parts, tile for tile (GameCube sub x, y); a PC part's GUID where the PC has a tile there
        gtaken = set()
        pcg = dict(it.tiles)
        for k, (x, y) in enumerate(hmod.BASE_INFO.get(base, {}).get('tiles') or [(0, 0)]):
            g = pcg.pop((x, H - 1 - y), None) or tile_guid(it.guid, x, y, s.gco.guids, gtaken)
            if g in s.gco.guids or g == it.guid:
                g = tile_guid(it.guid, x, y, s.gco.guids, gtaken)
            tiles.append((x, y, g, None))
    elif multi and elec:
        # GameCube sub (x, y); a PC part's GUID where the PC has a tile there (PC (x, H-1-y), the mesh
        # moved by (dx, dy) in the base's footprint)
        gtaken = set()
        pcg = dict(it.tiles)
        if elec_parts:
            coords = list(einfo['tiles'])
        else:
            coords = [(x, H - 1 - y) for (x, y) in (sorted(pcg) or [(x, y) for x in range(W) for y in range(H)])]
        for k, (x, y) in enumerate(coords):
            px, py = x - int(round(dx)), (H - 1 - y) - int(round(dy))
            g = pcg.pop((px, py), None) or tile_guid(it.guid, x, y + 16 * k, s.gco.guids, gtaken)
            if g in s.gco.guids or g == it.guid:
                g = tile_guid(it.guid, x, y + 16 * k, s.gco.guids, gtaken)
            tiles.append((x, y, g, None))
    elif multi:
        gtaken = set()
        pcg = dict(it.tiles)
        coords = list(pcg.keys()) or [(0, 0)]
        if appl or plumb:
            coords = [(x, y) for x in range(W) for y in range(H)]
        for (x, y) in sorted(coords):
            g = pcg.get((x, y)) or tile_guid(it.guid, x, y, s.gco.guids, gtaken)
            if g in s.gco.guids or g == it.guid:
                g = tile_guid(it.guid, x, y, s.gco.guids, gtaken)
            tiles.append((x, H - 1 - y, g, heights.get((x, y))))
        if appl and kind == 'dishwasher':
            # its parts in the base group's order: the machine (sub 0,1) first, where it is placed, then
            # the tile in front (the first part is the one placed on the tile the player picks)
            tiles.sort(key=lambda t: (t[0], -t[1]))
            # the game makes a group's parts in the order its selector table lists them (by the GUID's low
            # byte; GameCube/folder.cpp m_selectors[guid & 0xFF]) and places the first one made on the tile
            # picked: as the base's, the machine's GUID must come first (its tiles' GUIDs are the
            # converter's own: swapped when they would not)
            (x0, y0, g0, h0), (x1, y1, g1, h1) = tiles
            if (g0 & 0xFF) > (g1 & 0xFF) or (g0 & 0xFF) == (g1 & 0xFF):
                if (g0 & 0xFF) == (g1 & 0xFF):
                    g1 = tile_guid(it.guid ^ 0x5A5A5A5A, x1, y1, s.gco.guids, gtaken)
                g0, g1 = (g0, g1) if (g0 & 0xFF) < (g1 & 0xFF) else (g1, g0)
                tiles = [(x0, y0, g0, h0), (x1, y1, g1, h1)]
    if (plumb or (elec and elec_parts) or hobby) and tiles:
        # the parts in the base group's order: the game makes a group's parts in the order of their GUIDs'
        # low bytes (GameCube/folder.cpp m_selectors[guid & 0xFF]) and puts the first one made on the
        # tile picked; the base's part at each tile sets the order its parts are made in
        bsub = {(d['sub'] >> 8 & 0xFF, d['sub'] & 0xFF): d['guid'] & 0xFF for d in s.gco.files.get(base, []) if d['sub'] >= 0}
        want = sorted(tiles, key=lambda t: bsub.get((t[0], t[1]), 0x100))
        lows = sorted(t[2] & 0xFF for t in tiles)
        if len(set(lows)) == len(lows):
            # hand out the tiles' GUIDs (sorted by low byte) in the base's order
            gs = sorted((t[2] for t in tiles), key=lambda g: g & 0xFF)
            tiles = [(t[0], t[1], g, t[3]) for t, g in zip(want, gs)]
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
    name = it.name
    if surface and 'no doors' in it.objname.lower() and 'no doors' not in (name or '').lower():
        name = f'{name} (no doors)'
    with open(os.path.join(folder, 'mod.ini'), 'w', encoding='utf-8') as f:
        f.write('# PC mod: mods. Converted by tools/pcobj/pcobj_convert.py from the user\'s own The Sims 2 (PC)\n'
                '# install. EA game data: for this machine only, never share or commit it.\n')
        what = (f'{it.category}: {pckids.describe(kidkind)}' + (f', placed: {kind}' if kidkind in pckids.DECOR else '') if kid
                else f'misc: {pcmisc.describe(mkind)}' + (f', placed: {kind}' if mkind in pcmisc.DECOR else '') if misc
                else f'surface: {pcsurf.describe(kind)}' if surface else f'seating: {pcseat.describe(kind)}' if seating
                else f'appliance: {pcappl.describe(kind)}' if appl else f'lighting: {pclight.describe(kind)}' if light
                else f'plumbing: {pcplumb.describe(kind)}' if plumb
                else f'electronics: {pcelec.describe(kind)}' if elec
                else f'hobbies: {pchobby.describe(kind)}' if hobby
                else f'{it.subsort_name()}, placed: {kind}')
        f.write(f'# PC object "{it.objname}" ({it.home.name}), model {used}, {what}\n[object]\n')
        f.write(f'guid = 0x{it.guid:08X}\nbase = {base}\nname = {one(name)}\ndesc = {one(it.desc)}\n')
        f.write(f'price = {it.price}\nmodel = 0x{mid:08X}\nthumbnail = 0x{thumb_id:08X}\n')
        f.write(f'functions = 0x{(pcsurf.F_SURFACES if surface else pcseat.F_SEATING if seating else pcappl.F_APPLIANCES if appl else pclight.F_LIGHTING if light else pcplumb.F_PLUMBING if plumb else pcelec.F_ELECTRONICS if elec else hmod.tab_of(base) if hobby else F_DECORATIVE):X}\n'
                f'room = {max(0, min(10, it.room))}\n')
        if appl:
            # the base's skeleton, state animations and effects; the states show the bound model, the
            # catalogue's preview the still one (`model`)
            f.write('animated = 1\n')
            if aid:
                f.write(f'anim_model = 0x{aid:08X}\n')
            for node, (pv, spec) in sorted(node_pivots.items()):
                f.write(f'node = {node}:{pv[0]:.4f},{pv[1]:.4f},{pv[2]:.4f}{":" + spec if spec else ""}\n')
            if kind == 'dishwasher':
                f.write(f'base_parts = 1\nsurface = {it.surface:.3f}\n')
            if kind in pcappl.COUNTERTOP and W * H > 1:
                # a cart standing on the floor (the espresso cart) made from a counter-top base: data 4,
                # the heights it may be placed at, the floor's
                f.write('init_data = 4:0x1\n')
        if surface:
            f.write(f'surface = {it.surface:.3f}\n')
        if plumb:
            # the base's skeleton, state animations and effects (water, steam, flies); the PC mesh bound to
            # its nodes; the PC's dirty / clogged looks in the base's dirty / clogged states
            f.write(f'animated = 1\nanim_model = 0x{aid:08X}\n')
            for node, (pv, spec) in sorted(node_pivots.items()):
                f.write(f'node = {node}:{pv[0]:.4f},{pv[1]:.4f},{pv[2]:.4f}{":" + spec if spec else ""}\n')
            sm = []
            for look in ('dirty', 'clogged'):
                if look in plumb_ids or (look == 'clogged' and 'dirty' in plumb_ids):
                    mid_ = plumb_ids.get(look, plumb_ids.get('dirty'))
                    sm += [f'{st}:0x{mid_:08X}' for st in pinfo.get(look, ())]
            if sm:
                f.write('state_models = ' + ' '.join(sm) + '\n')
            if it.seat:
                what_ = 'water' if kind == 'hottub' else 'seat'
                f.write(f'# {what_} {it.surface:.3f}\nseat_offset = {it.seat["offset"]:.3f}\n')
            if tiles:
                f.write('base_parts = 1\n')
            # the catalogue's motive ratings: the PC's (written where the base shows one)
            for key, k in (('comfort', 1), ('hygiene', 2), ('bladder', 3), ('fun', 5)):
                v = it.o.s(pcinstall.W_RATINGS + k)
                if v > 0:
                    f.write(f'{key} = {min(10, v)}\n')
            if pinfo.get('hole'):
                # the hole a converted counter it stands in shows (model_sink / model_sink_wide)
                f.write(f'sink_hole = {pinfo["hole"]}\n')
            if plumb_ids.get('flange'):
                f.write(f'model_flange = 0x{plumb_ids["flange"]:08X}\n')
        if elec and kind != 'clock_wall':
            # the base's skeleton, state animations and effects; the PC mesh on its nodes (body, handset,
            # a copy of the screen on each of the base's screen nodes)
            f.write(f'animated = 1\nanim_model = 0x{aid:08X}\n')
            for node, (pv, spec) in sorted(node_pivots.items()):
                f.write(f'node = {node}:{pv[0]:.4f},{pv[1]:.4f},{pv[2]:.4f}{":" + spec if spec else ""}\n')
            if 'state_shaders' in elec_ini:
                # a TV: the base's channels on the screen's shader; switched off, the PC TV's screen
                f.write(f'shader_swaps = 1\nstate_shaders = {elec_ini["state_shaders"]}\n')
            if 'light' in elec_ini:
                f.write(f'light = {elec_ini["light"]}\n')
            if kind in pcelec.INIT_DATA:
                f.write(f'init_data = {pcelec.INIT_DATA[kind]}\n')
            if kind in pcelec.WALL_MODEL:
                f.write(f'wall_model = {pcelec.WALL_MODEL[kind]}\n')
            if kind in pcelec.ROUTE_FRONT:
                f.write('route_front = 1\n')
            if kind in pcelec.SAME_TYPE:
                f.write('same_type = 1\n')
            if tiles and elec_parts:
                f.write('base_parts = 1\n')
        if hobby and anim_parts:
            # the base's skeleton, state animations and effects; the PC mesh on its body node, the base's props
            f.write(f'animated = 1\nanim_model = 0x{aid:08X}\n')
            if hobby_props[0]:
                f.write('prop_nodes = ' + ','.join(str(n) for n, r in hobby_props[0]) + '\n')
            if tiles:
                f.write('base_parts = 1\n')
            # the base's trees test their own GUID (the cheap telescope's and bookshelf's skill rates, the air
            # hockey table's and hamster wheel's parts): the converted object answers as its base
            f.write('same_type = 1\n')
            hp = hmod.hide_props(base, kind)
            if hp:
                # the dress-up props of the base's own look the person would wear (the wheel round the runner)
                f.write('hide_props = ' + ' '.join(f'0x{h:08X}' for h in hp) + '\n')
            sk = hmod.skill_of(base)
            if sk:
                f.write(f'# teaches {sk} (the base\'s)\n')
        if misc and mkind in pcmisc.INIT:
            # the trash chute: the bottomless trash can on a wall (data 8 / 0xD: a wall object)
            f.write(f'init_data = {pcmisc.INIT[mkind]}\n')
        if hobby and not kid:
            v = max(0, it.o.s(pcinstall.W_RATINGS + 5))
            if v > 0:
                f.write(f'fun = {min(10, v)}\n')
        if elec:
            v = pcelec.fun(it)
            if v > 0:
                f.write(f'fun = {min(10, v)}\n')
        if lamp:
            # the base's skeleton, on/off state animations and light; the parts by state on its nodes
            (lx, ly, lz), lc, lfo = lamp['mod']
            f.write(f'animated = 1\nanim_model = 0x{aid:08X}\n')
            f.write(f'light = {lx:.3f},{ly:.3f},{lz:.3f}:{lc[0]:.3f},{lc[1]:.3f},{lc[2]:.3f}:{lfo:.2f}\n')
            if kind in pclight.INIT_DATA:
                f.write(f'init_data = {pclight.INIT_DATA[kind]}\n')
        if kid and kidkind in pckids.DECOR and kind in pckids.INIT_DATA:
            # a small toy: on the floor as well as on tables (data 4: the heights it may stand at)
            f.write(f'init_data = {pckids.INIT_DATA[kind]}\n')
        if kind == 'ceiling' and not (surface or seating or appl or light):
            # a decorative ceiling object (a rug of the GameCube): only inside, under a ceiling
            f.write('init_data = 0x1E:2\n')
        if seating:
            f.write(f'# {"mattress (slept in)" if kind.startswith("bed") else "seat"} {seat["height"]:.3f} against the '
                    f'GameCube\'s {pcseat.GC_MATTRESS if kind.startswith("bed") else pcseat.GC_SEAT:.2f}\n')
            f.write(f'seat_offset = {seat["offset"]:.3f}\n')
            f.write(f'comfort = {seat["comfort"]}\n')
            if seat['energy'] >= 0:
                f.write(f'energy = {seat["energy"]}\n')
            if 'in' in sids:
                f.write(f'state_models = 1:0x{sids["in"]:08X}\n')
            # a bar stool: sims sit on it to eat at the counter it faces (game: countereat.cpp); a dining
            # chair never (its seat is below a counter's reach even when tall)
            f.write(f'counter_seat = {1 if kind == "barstool" else 0}\n' if kind in ('dining', 'barstool') else '')
        if tiles:
            f.write('tiles = ' + ' '.join(f'{x},{y}:0x{g:08X}' + (f':{h:.3f}' if surface and h else '')
                                          for x, y, g, h in tiles) + '\n')
        if seating and kind.startswith('bed'):
            # made in state 0, slept in for every other (getting in/out, sleeping, relaxing, making it)
            states = range(1, 32)
            f.write('base_parts = 1\n')
            if kind == 'bed_double':
                for (x, y), side in (((1, 1), 'right'), ((1, 0), 'left')):
                    f.write(f'tile_model = {x},{y}:0x{sids[side + "_made"]:08X} ' +
                            ' '.join(f'{k}:0x{sids[side + "_slept"]:08X}' for k in states) + '\n')
            else:
                f.write(f'tile_model = 1,1:0x{mid:08X} ' + ' '.join(f'{k}:0x{sids["slept"]:08X}' for k in states) + '\n')
        if vids:
            f.write(f'connect = {"island" if kind == "island" else "counter"}\n')
            if 'sink_gc' in vids:
                f.write(f'sink_shift = {getattr(it, "sink_shift", 0.0):.3f}\n')
            for k, vid in vids.items():
                f.write(f'model_{k} = 0x{vid:08X}\n')
        f.write(f'pack = {it.home.name}\n')
    it.status = 'converted'
    return it


# ---------------------------------------------------------------------------------------------------

INV_COLUMNS = ['guid', 'name', 'object', 'pack', 'packs', 'subsort', 'footprint', 'placement', 'base',
               'price', 'room', 'bhavs', 'semiglobal', 'model', 'textures', 'recolours', 'verts', 'tris',
               'status', 'reason', 'folder', 'notes', 'bounds', 'category', 'surface', 'pieces', 'seat_offset',
               'comfort', 'energy', 'seats', 'pc_tab', 'gc_tab']
INVENTORY = {'decorative': 'pcdeco_inventory.tsv', 'surfaces': 'pcsurf_inventory.tsv', 'seating': 'pcseat_inventory.tsv',
             'appliances': 'pcappl_inventory.tsv', 'lighting': 'pclight_inventory.tsv', 'plumbing': 'pcplumb_inventory.tsv',
             'electronics': 'pcelec_inventory.tsv', 'hobbies': 'pchobby_inventory.tsv',
             'storage': 'pcstorage_inventory.tsv', 'kids': 'pckids_inventory.tsv', 'misc': 'pcmisc_inventory.tsv'}
# buy-mode tabs: the PC's (OBJD function sort bits) and the GameCube's (definition function flags; the
# tab a converted object is put on)
PC_TABS = [(0x01, 'Comfort'), (0x02, 'Surfaces'), (0x04, 'Appliances'), (0x08, 'Electronics'), (0x10, 'Plumbing'),
           (0x20, 'Decorations'), (0x40, 'Miscellaneous'), (0x80, 'Lighting'), (0x100, 'Hobbies'),
           (0x400, 'Aspiration Rewards'), (0x800, 'Career Rewards')]
GC_TABS = {'seating': 'Seating & Beds', 'appliances': 'Appliances', 'surfaces': 'Surfaces', 'decorative': 'Decorative',
           'lighting': 'Lighting', 'plumbing': 'Plumbing', 'electronics': 'Electronics', 'hobbies': 'Skill',
           'storage': 'Misc', 'kids': 'Misc', 'misc': 'Misc'}


def gc_tab(it):
    if it.category in ('storage', 'kids'):
        # the base's tab: the dresser and wardrobe Misc, the game console Electronics, a decoration Decorative
        if it.base in pckids.BASE_INFO:
            return 'Misc'
        return 'Electronics' if it.base == pckids.KID_BASES['rc'] else 'Decorative'
    if it.category == 'misc':
        # the base's tab: the drink bar, bonfire, trash cans and wardrobe Misc, the keg and bubble blower
        # Appliances, the fridge (buffets) Appliances, the smoke alarm Electronics, a decoration Decorative
        p = pcmisc.path(it.mkind)
        if p == 'hobby':
            return {pcmisc.F_GC_MISC: 'Misc', pcmisc.F_GC_APPL: 'Appliances'}.get(pcmisc.tab_of(it.base), 'Misc')
        return {'elec': 'Electronics', 'appl': 'Appliances', 'decor': 'Decorative'}.get(p, '')
    if it.category == 'hobbies':
        return {pchobby.F_GC_SKILL: 'Skill', pchobby.F_GC_MISC: 'Misc', pchobby.F_GC_DECO: 'Decorative'}.get(pchobby.tab_of(it.base), '')
    return GC_TABS.get(it.category, '')


def pc_tab(it):
    f = it.o.w(W_FUNCSORT)
    return '+'.join(n for b, n in PC_TABS if f & b) or f'0x{f:x}'


def inv_row(it):
    w, h = it.footprint()
    return [f'0x{it.guid:08X}', it.name or '', it.objname, it.home.name, ','.join(dict.fromkeys(it.packs)),
            it.subsort_name(), f'{w}x{h}', it.kind, it.base, str(it.price), str(it.room), str(it.bhavs),
            it.semiglobal, it.model_used or ','.join(it.models[:2]), ' '.join(it.textures), str(max(0, len(it.recolours) - 1)),
            str(it.verts), str(it.tris), it.status, it.reason, it.out, '; '.join(it.notes),
            ' '.join(f'{v:.2f}' for v in np.concatenate(it.bounds)) if it.bounds is not None else '',
            it.category, f'{it.surface:.2f}' if it.surface else '', ' '.join(it.variants),
            f'{it.seat["offset"]:+.3f}' if it.seat else '', str(it.seat.get('comfort', '')) if it.seat else '',
            str(it.seat.get('energy', '')) if it.seat and it.seat.get('energy', -1) >= 0 else '',
            str(it.seat.get('seats', '')) if it.seat else '', pc_tab(it),
            gc_tab(it) if it.status == 'converted' or it.status.startswith('ok') else '']


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
    guid, base, category = args
    it = Item(_worker.catalog, guid, category)
    try:
        convert(_worker, it, base)
    except Exception as e:
        it.status, it.reason = 'failed', f'{type(e).__name__}: {e}'
    return inv_row(it)


# Build mode's wall and floor coverings (pcwall.py): --categories walls,floors (in "all" too)
COVERINGS = {'walls': 'wall', 'floors': 'floor'}
CATEGORIES = {'decorative': decorative_guids, 'surfaces': pcsurf.surface_guids, 'seating': pcseat.seating_guids,
              'appliances': pcappl.appliance_guids, 'lighting': pclight.lighting_guids, 'plumbing': pcplumb.plumbing_guids,
              'electronics': pcelec.electronics_guids, 'hobbies': pchobby.hobby_guids,
              'storage': pckids.storage_guids, 'kids': pckids.kids_guids, 'misc': pcmisc.misc_guids}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pc', required=True, help='The Sims 2 (PC) install folder (all packs under it are read)')
    ap.add_argument('--gc', help='GameCube data: the disc image, or the folder with DATA/')
    ap.add_argument('--out', help='mods folder to write into (never the source tree)')
    ap.add_argument('--list', help='list PC objects whose name contains this text')
    ap.add_argument('--object', help='one PC object: GUID (hex) or part of its name')
    ap.add_argument('--base', help='with --object: the GameCube object to copy (default: chosen by placement)')
    ap.add_argument('--all-decorative', action='store_true', help='every buy-mode Decorative object')
    ap.add_argument('--all-surfaces', action='store_true', help='every buy-mode Surfaces object')
    ap.add_argument('--all-seating', action='store_true', help='every buy-mode Seating object and bed')
    ap.add_argument('--all-appliances', action='store_true', help='every buy-mode Appliances object')
    ap.add_argument('--all-lighting', action='store_true', help='every buy-mode Lighting object')
    ap.add_argument('--all-plumbing', action='store_true', help='every buy-mode Plumbing object (and the hot tubs)')
    ap.add_argument('--all-electronics', action='store_true', help='every buy-mode Electronics object (and the massage table)')
    ap.add_argument('--all-hobbies', action='store_true', help='every buy-mode Hobbies object')
    ap.add_argument('--all-storage', action='store_true', help='every buy-mode Storage object (Miscellaneous: dressers)')
    ap.add_argument('--all-kids', action='store_true', help='every buy-mode Kids object (Miscellaneous: kids) and the '
                    'remote-control toys')
    ap.add_argument('--all-misc', action='store_true', help='every other buy-mode Miscellaneous object (bars, party '
                    'objects, trash cans, booths, pet furniture...)')
    ap.add_argument('--categories', help='comma-separated categories: build mode\'s walls and floors (wall and floor '
                    'coverings, pcwall.py) and the buy-mode decorative, surfaces, seating, appliances, '
                    'lighting, plumbing, electronics, hobbies, storage, kids, misc (or all)')
    ap.add_argument('--batch', help='a list of GUIDs (first column; optional second column: base)')
    ap.add_argument('--packs', help='only objects whose home pack is one of these (codes: base, ep1..ep8, sp1..sp9)')
    ap.add_argument('--inventory', help='write the inventory (TSV) of the chosen categories here and stop')
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
    cats = []
    covers = []  # wall / floor coverings (pcwall.py)
    if a.categories:
        cats = list(CATEGORIES) + list(COVERINGS) if a.categories.strip().lower() == 'all' else \
            [c.strip().lower() for c in a.categories.split(',') if c.strip()]
        bad = [c for c in cats if c not in CATEGORIES and c not in COVERINGS]
        if bad:
            raise SystemExit(f'unknown categor{"y" if len(bad) == 1 else "ies"}: {", ".join(bad)} '
                             f'(known: {", ".join(list(CATEGORIES) + list(COVERINGS))})')
        covers = [COVERINGS[c] for c in cats if c in COVERINGS]
        cats = [c for c in cats if c not in COVERINGS]
        if covers and not cats and not (a.object or a.batch):
            # only coverings
            if a.inventory:
                pcwall.run(a.pc, None, None, tuple(covers), set(a.packs.split(',')) if a.packs else None, a.jobs,
                           log=s.log, inventory_only=a.inventory)
            else:
                if not (a.out and a.gc):
                    raise SystemExit('--gc and --out are needed to convert')
                pcwall.run(a.pc, a.gc, a.out, tuple(covers), set(a.packs.split(',')) if a.packs else None, a.jobs, log=s.log)
            return
    if a.all_decorative and 'decorative' not in cats:
        cats.append('decorative')
    if a.all_seating and 'seating' not in cats:
        cats.append('seating')
    if a.all_surfaces and 'surfaces' not in cats:
        cats.append('surfaces')
    if a.all_appliances and 'appliances' not in cats:
        cats.append('appliances')
    if a.all_lighting and 'lighting' not in cats:
        cats.append('lighting')
    if a.all_plumbing and 'plumbing' not in cats:
        cats.append('plumbing')
    if a.all_electronics and 'electronics' not in cats:
        cats.append('electronics')
    if a.all_hobbies and 'hobbies' not in cats:
        cats.append('hobbies')
    if a.all_storage and 'storage' not in cats:
        cats.append('storage')
    if a.all_kids and 'kids' not in cats:
        cats.append('kids')
    if a.all_misc and 'misc' not in cats:
        cats.append('misc')
    def category_of(o):
        f = o.w(W_FUNCSORT)
        if o.guid in pckids.RC_GUIDS:
            return 'kids'
        if f & F_DECORATIVE:
            return 'decorative'
        if f & pcsurf.F_SURFACES:
            return 'surfaces'
        if f & pcappl.F_APPLIANCES:
            return 'appliances'
        if f & pclight.F_LIGHTING:
            return 'lighting'
        if f & pcplumb.F_PLUMBING or 'hottub' in (cat.semiglobal(o) or '').lower():
            return 'plumbing'
        if f & pcelec.F_ELECTRONICS or (cat.semiglobal(o) or '').lower() == 'tablemassageglobals':
            return 'electronics'
        if f & pchobby.F_HOBBIES:
            return 'hobbies'
        if f & pckids.F_MISC and o.w(W_SUBSORT) & pckids.SUB_STORAGE:
            return 'storage'
        if f & pckids.F_MISC and o.w(W_SUBSORT) & pckids.SUB_KIDS:
            return 'kids'
        if f & pcmisc.F_MISC:
            return 'misc'
        return 'seating' if f & pcseat.F_SEATING else 'decorative'
    jobs = []
    if a.object:
        q = a.object.lower()
        qg = int(q, 16) if re.fullmatch(r'(0x)?[0-9a-f]{6,8}', q) else None
        hits = [g for g, lst in cat.by_guid.items() if lst[-1].is_lead() and
                (g == qg or (qg is None and (q in lst[-1].name.lower() or q in (cat.catalog_text(lst[-1])[0] or '').lower())))]
        if not hits:
            raise SystemExit(f'no PC object matches {a.object}')
        jobs = [(hits[0], a.base, category_of(cat.newest(hits[0])))]
    elif a.batch:
        known = (set(BASES.values()) | set(pcsurf.SURF_BASES.values()) | set(pcseat.SEAT_BASES.values()) |
                 set(pcappl.APPL_BASES.values()) | set(pclight.LIGHT_BASES.values()) | set(pcplumb.PLUMB_BASES.values()) |
                 set(pcelec.ELEC_BASES.values()) | set(pchobby.HOBBY_BASES.values()) | set(pckids.KID_BASES.values()) |
                 set(pcmisc.MISC_BASES.values()))
        for line in open(a.batch, encoding='utf-8'):
            f = line.strip().split('\t')
            if f and re.fullmatch(r'(0x)?[0-9a-fA-F]{6,8}', f[0]):
                g = int(f[0], 16)
                if g in cat.by_guid:
                    jobs.append((g, f[1] if len(f) > 1 and f[1] in known else None, category_of(cat.newest(g))))
    elif cats:
        seen = set()
        if 'kids' in cats:
            # the remote-control toys are the Kids category's even when Electronics comes first
            seen |= pckids.claims(cat)
            jobs += [(g, None, 'kids') for g in sorted(pckids.claims(cat))]
        for c in cats:
            for g in CATEGORIES[c](cat):
                if g not in seen:
                    seen.add(g)
                    jobs.append((g, None, c))
    elif a.inventory:
        jobs = [(g, None, 'decorative') for g in decorative_guids(cat)]
    else:
        raise SystemExit('nothing to do: --all-decorative, --all-surfaces, --all-seating, --all-appliances, --all-lighting, --all-plumbing, '
                         '--all-electronics, --all-hobbies, --all-storage, --all-kids, --all-misc, --categories, '
                         '--object, --batch, --inventory or --list')
    packs = set(a.packs.split(',')) if a.packs else None
    items = []
    for g, b, c in jobs:
        it = Item(cat, g, c)
        if packs and it.home.code not in packs:
            continue
        r = skip_reason(it)
        if r:
            it.status, it.reason = 'skipped', r
        items.append((it, b))
    def sort_key(ib):
        it = ib[0]
        if it.category == 'surfaces':
            return ('s', pcsurf.SURF_SORT.get(pcsurf.surface_kind(it), '7'), (it.name or '').lower())
        if it.category == 'seating':
            return ('t', pcseat.SEAT_SORT.get(pcseat.seat_kind(it)[0], '8'), (it.name or '').lower())
        if it.category == 'appliances':
            return ('u', pcappl.APPL_SORT.get(pcappl.appl_kind(it), '6'), (it.name or '').lower())
        if it.category == 'lighting':
            return ('v', pclight.LIGHT_SORT.get(pclight.light_kind(it), '6'), (it.name or '').lower())
        if it.category == 'plumbing':
            return ('w', pcplumb.PLUMB_SORT.get(pcplumb.plumb_kind(it), '6'), (it.name or '').lower())
        if it.category == 'electronics':
            return ('x', pcelec.ELEC_SORT.get(pcelec.elec_kind(it), '8'), (it.name or '').lower())
        if it.category in ('storage', 'kids'):
            return ('z', pckids.KID_SORT.get(pckids.kid_kind(it), '8'), (it.name or '').lower())
        if it.category == 'misc':
            return ('zz', pcmisc.MISC_SORT.get(pcmisc.misc_kind(it), '9'), (it.name or '').lower())
        if it.category == 'hobbies':
            return ('y', pchobby.HOBBY_SORT.get(pchobby.hobby_kind(it), '9'), (it.name or '').lower())
        return ('d', SORT_KEY.get(it.subsort, '7'), (it.name or '').lower())
    items.sort(key=sort_key)
    todo = [(it, b) for it, b in items if not it.status]
    s.log(f'{len(items)} object(s) ({", ".join(f"{sum(1 for it, b in items if it.category == c)} {c}" for c in CATEGORIES)}), '
          f'{len(todo)} to convert ({time.time() - t0:.1f} s)')
    if a.inventory:
        for it, b in todo:
            try:
                convert(s, it, b, write=False)
            except Exception as e:
                it.status, it.reason = 'failed', f'{type(e).__name__}: {e}'
        write_inventory(a.inventory, [it for it, b in items])
        s.log(f'wrote {a.inventory} ({time.time() - t0:.1f} s)')
        if covers:
            inv = os.path.splitext(a.inventory)[0] + '_coverings.tsv'
            pcwall.run(a.pc, None, None, tuple(covers), set(a.packs.split(',')) if a.packs else None, a.jobs,
                       log=s.log, inventory_only=inv)
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
    def progress(k, row):
        el = time.time() - t0
        s.log(f'[{k + 1}/{n} {100 * (k + 1) // max(n, 1):3d}% {el:5.0f}s] {row[18]:9s} {row[1][:40]:40s} {row[3]:20.20s} {row[7]:13s} {row[19]}')
    if workers > 1 and n > 4 and not a.object:
        import multiprocessing as mp
        with mp.Pool(workers, _work_init, (a.pc, a.gc, a.out, a.texsize)) as pool:
            for k, row in enumerate(pool.imap_unordered(_work, [(it.guid, b, it.category) for it, b in todo], chunksize=2)):
                rows[int(row[0], 16)] = row
                progress(k, row)
    else:
        for k, (it, b) in enumerate(todo):
            try:
                convert(s, it, b)
            except Exception as e:
                it.status, it.reason = 'failed', f'{type(e).__name__}: {e}'
            row = inv_row(it)
            rows[it.guid] = row
            progress(k, row)
    written = []
    for c, fn in INVENTORY.items():
        mine = [(it, b) for it, b in items if it.category == c]
        if not mine or a.object:
            continue
        inv = os.path.join(a.out, fn)
        old = {}
        if os.path.exists(inv):
            for line in open(inv, encoding='utf-8').read().splitlines()[1:]:
                f = line.split('\t')
                old[f[0]] = f + [''] * (len(INV_COLUMNS) - len(f))
        allrows = dict(old)
        for it, b in mine:
            allrows[f'0x{it.guid:08X}'] = rows.get(it.guid) or inv_row(it)
        with open(inv, 'w', encoding='utf-8') as f:
            f.write('\t'.join(INV_COLUMNS) + '\n')
            for r in allrows.values():
                f.write('\t'.join(r) + '\n')
        written.append(inv)
    stat = {}
    for it, b in items:
        st = (rows.get(it.guid) or inv_row(it))[18]
        stat[st] = stat.get(st, 0) + 1
    s.log(f'done in {time.time() - t0:.0f} s: ' + ', '.join(f'{v} {k}' for k, v in sorted(stat.items())) +
          (f'; inventory: {", ".join(written)}' if written else ''))
    if covers and not a.object:
        pcwall.run(a.pc, a.gc, a.out, tuple(covers), packs, a.jobs, log=s.log)


if __name__ == '__main__':
    main()
