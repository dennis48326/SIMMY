#!/usr/bin/env python3
"""PC mod: mods. Convert wall coverings (wallpaper, paint, paneling, tile, ...) and floor coverings
(carpet, tile, wood, stone, ...) from the user's own The Sims 2 (PC) install (base game and every
expansion and stuff pack found) into a SIMMY mod folder: build mode's wallpaper and floor catalogues
then list them after the GameCube's own.

    pcwall.py --pc "<The Sims 2 install>" --gc <GameCube disc image or data folder> --out <mods>
              [--kinds walls,floors] [--packs base,ep1,...] [--jobs N]
    pcwall.py --pc "<install>" --inventory coverings.tsv          list, no output

(pcobj_convert.py --categories ...,walls,floors runs this too.)

The PC keeps each covering as an XOBJ property set (type "wall" / "floor") in every pack's
Catalog/Patterns/catpatterns.bundle.package: GUID, name, description, cost, sub-sort, surface type and
its material (TXMT, in the pack's scenegraph packages) whose base texture is the picture: a wall's
256 x 512 (one wall tile wide, the full wall height, crown and kick molding drawn in), a floor's 256 x 256
for one tile (floorMaterialScaleU = n: n x n tiles). A covering appears in several packs' bundles: the
newest pack's definition wins, its home pack is the oldest that has it.

On the GameCube a covering is a row of the build sets quickdata ("WallSet" / "FloorSet": price, name,
shader, kind fields) whose shader both covers the walls / floors and is the catalogue's picture: a
128 x 64 (walls: x along the wall's height, the floor at x = 0, y along the wall) or 64 x 64 (floors)
8-bit paletted texture (GX C8, 256 RGBA8 colours, 4 mip levels). Its catalogue has no sub-groups (one
wallpaper tab, one floor tab), but each floor row names its footstep surface (wood, tile, carpet, metal,
stone, dirt, grass), and its strips show a picture mirrored as seen from above (the converter stores
PC floors mirrored). The converter keeps the PC's sub-sort as the mod folder's group (the catalogue's
order) and takes the GameCube covering of the same kind (floors: the same surface) and nearest colour as
its "base": the row it copies, and what a lot saved with it shows when the mod is gone.

Each group becomes <out>/pcw<k>_<group>/ (walls: 1 paint, 2 wallpaper, 3 paneling, 4 tile, 5 masonry,
6 brick, 7 siding, 8 poured, 9 other) or pcf<k>_<group>/ (floors: 1 carpet, 2 wood, 3 tile, 4 lino,
5 stone, 6 brick, 7 poured, 8 other): mod.ini ([wall] / [floor] sections: key, name, desc, price, shader,
base, sound, scale, group, pack), shaders_<id>.res and textures_<id>.res per covering, preview.png (the
group's pictures, for the user). <out>/pcwall_inventory.tsv and pcfloor_inventory.tsv list every
covering found, converted or skipped (and why).

Never put the output in the source tree or share it: it is made of EA's PC game data, converted on the
user's machine from the user's own copy. See docs/mods.md.
"""
import argparse, glob, html, os, re, shutil, struct, sys, time, zlib

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rcol, sg, gcdata, pcinstall  # noqa: E402
from dbpf import Package  # noqa: E402

T_XOBJ = 0xCCA8E925
BUILD_SETS = 0x19A16F2D
FMT_C8 = 0x8A               # GX C8 with a 256-entry RGBA8 palette (ENgcTextureC8_32), the disc's
WALL_SIZE = (128, 64)       # (width: the wall's height, height: along the wall), the disc's
FLOOR_TILE = 64             # floor pixels per tile, the disc's
FLOOR_MAX = 256             # a floor picture spanning several tiles: at most this
WALL_GROUPS = ['paint', 'wallpaper', 'paneling', 'tile', 'masonry', 'brick', 'siding', 'poured', 'other']
FLOOR_GROUPS = ['carpet', 'wood', 'tile', 'lino', 'stone', 'brick', 'poured', 'other']
# PC floor surface type -> the GameCube's footstep surface (FloorSet +8: 4 grass, 6 dirt, 9 wood, 10 tile,
# 11 carpet, 12 metal, 13 stone / concrete)
SURFACE_SOUND = {'cpet': 11, 'marble': 10, 'lino': 10, 'cment': 13, 'wood': 9, 'wdeck': 9, 'gravel': 6, 'grass': 4}
SUBSORT_SOUND = {'carpet': 11, 'wood': 9, 'tile': 10, 'lino': 10, 'stone': 13, 'brick': 13, 'poured': 13}
# The GameCube's wall coverings by their names -> the PC sub-sort they stand in for
GC_WALL_WORDS = [('tile', 'tile'), ('paneling', 'paneling'), ('plank', 'paneling'), ('wood', 'paneling'),
                 ('wainscot', 'paneling'), ('concrete', 'poured'), ('siding', 'siding'), ('clapboard', 'siding'),
                 ('stone', 'masonry'), ('marble', 'masonry'), ('wallpaper', 'wallpaper'), ('print', 'wallpaper'),
                 ('microhide', 'wallpaper'), ('fabric', 'wallpaper'), ('drywall', 'paint'), ('stucco', 'paint'),
                 ('sponge', 'paint'), ('paint', 'paint')]
PC_TO_GC_WALL = {'brick': 'masonry'}
INV_COLUMNS = ['pack', 'kind', 'name', 'pc_id', 'pc_subsort', 'gc_group', 'price', 'status', 'reason', 'key', 'gc_base',
               'gc_base_name', 'material', 'texture', 'scale', 'folder', 'description']
SOUND_NAMES = {4: 'grass', 6: 'dirt', 9: 'wood', 10: 'tile', 11: 'carpet', 12: 'metal', 13: 'stone'}


def log_print(*a):
    print(*a, flush=True)


def clean_text(s):
    s = html.unescape(str(s or '')).replace('\r', ' ').replace('\n', ' ').strip()
    return re.sub(r'\s+', ' ', s)


# ---------------------------------------------------------------------------------------------------
# PC side

class Covering:
    __slots__ = ('guid', 'kind', 'name', 'desc', 'price', 'subsort', 'surface', 'material', 'home', 'newest',
                 'show', 'deprecated', 'status', 'reason', 'key', 'group', 'gc_base', 'gc_base_name', 'texture',
                 'scale', 'folder', 'sound', 'shader')

    def __init__(self, d, pack):
        self.guid = int(d['guid']) & 0xFFFFFFFF
        self.kind = d['type']
        self.home = self.newest = pack
        self.status, self.reason = '', ''
        self.gc_base, self.gc_base_name, self.texture, self.scale, self.folder = 0, '', '', 1, ''
        self.sound, self.shader = -1, 0
        self.update(d, pack)

    def update(self, d, pack):
        self.newest = pack
        self.name = clean_text(d.get('name'))
        self.desc = clean_text(d.get('description'))
        self.price = int(d.get('cost') or 0)
        self.subsort = str(d.get('subsort') or 'other').lower()
        self.surface = str(d.get('surfacetype') or '').lower()
        self.material = str(d.get('material') or '')
        self.show = int(d.get('showincatalog') or 0)
        self.deprecated = int(d.get('deprecated') or 0)
        self.key = f'pc{"w" if self.kind == "wall" else "f"}_{self.guid:08x}'
        groups = WALL_GROUPS if self.kind == 'wall' else FLOOR_GROUPS
        self.group = self.subsort if self.subsort in groups else 'other'
        if self.kind == 'floor':  # the GameCube's footstep surface
            self.sound = SURFACE_SOUND.get(self.surface, SUBSORT_SOUND.get(self.subsort, 10))


def pattern_packages(pack):
    return sorted(glob.glob(os.path.join(pack.res, 'Catalog', 'Patterns', '*.package')))


def inventory(packs, kinds=('wall', 'floor'), log=log_print):
    """Every wall / floor covering of the packs (oldest first): {guid: Covering}."""
    items = {}
    for pk in packs:
        n = 0
        for f in pattern_packages(pk):
            p = Package(f)
            for e in p.of_type(T_XOBJ):
                d = pcinstall.parse_cpf(p.read(e))
                if d.get('type') not in kinds or 'guid' not in d:
                    continue
                g = int(d['guid']) & 0xFFFFFFFF
                if g in items:
                    items[g].update(d, pk)
                else:
                    items[g] = Covering(d, pk)
                n += 1
        if n:
            log(f'  {pk.name}: {n} wall / floor covering(s)')
    return items


def skip_reason(c):
    if not c.show:
        return 'not in the PC catalogue'
    if c.deprecated:
        return 'deprecated on the PC'
    if not c.material:
        return 'no material'
    return ''


# ---------------------------------------------------------------------------------------------------
# GameCube side: the build sets' rows and their pictures

def decode_c8(tex):
    """A disc texture resource (TXFL, C8 with a 32-bit palette, swizzled) -> RGBA of its first level."""
    _, _, _, _, off, size = gcdata.data_header(tex)
    d = tex[off:off + size]
    w, h = struct.unpack_from('>HH', d, 16)
    levels = struct.unpack_from('>H', d, 22)[0] or 1
    if d[24] != FMT_C8:
        return None
    W, H, total = w, h, 0
    for _ in range(levels):
        total += W * H
        W, H = max(1, W // 2), max(1, H // 2)
    px = np.frombuffer(d[0x20:0x20 + w * h], np.uint8).reshape(h // 4, w // 8, 4, 8).transpose(0, 2, 1, 3).reshape(h, w)
    pal = d[0x20 + total:0x20 + total + 1024]
    rg = np.frombuffer(pal[:512], np.uint8).reshape(256, 2)
    ba = np.frombuffer(pal[512:1024], np.uint8).reshape(256, 2)
    P = np.stack([rg[:, 1], rg[:, 0], ba[:, 1], ba[:, 0]], 1)
    return P[px]


class GcSets:
    """The disc's "WallSet" / "FloorSet" rows: index, name, price, shader, kind fields, mean colour."""

    def __init__(self, gc):
        raw = gc.get('quickdatas', BUILD_SETS)
        q = gcdata.Quickdata(raw)
        doff = struct.unpack_from('>I', raw, 4)[0]
        lang = struct.unpack_from('>6I', raw, 0x18)  # the first language (English)
        blob = raw[lang[1]:lang[1] + lang[2]]
        base = (len(q.img) + 3) & ~3

        def text(row):
            p = q.u32(row + 4)
            if not p:
                return ''
            sp = struct.unpack_from('>I', blob, p - base)[0] - base
            e = sp
            while e + 1 < len(blob) and blob[e:e + 2] != b'\0\0':
                e += 2
            return blob[sp:e].decode('utf-16-be').strip()
        self.rows = {}
        for kind, setname, tile, size in (('wall', 'WallSet', 'WallTile', 0x14), ('floor', 'FloorSet', 'FloorTile', 0x18)):
            st = q.table(setname)
            arr = q.ptr(st['rows'])
            n = q.u32(arr - 4)
            out = []
            for i in range(n):
                r = q.ptr(arr + 4 * i)
                w = [q.u32(r + 4 * k) for k in range(size // 4)]
                if kind == 'wall':
                    row = dict(index=i, name=text(r), price=w[0], shader=w[2], kindf=w[3], buy=w[4] >> 24, sound=-1)
                else:
                    row = dict(index=i, name=text(r), price=w[0], sound=w[2], shader=w[3], kindf=w[4], buy=w[5] >> 24)
                tex = gc.get('textures', row['shader'])
                img = decode_c8(tex) if tex else None
                row['mean'] = img[..., :3].reshape(-1, 3).mean(0) if img is not None else np.array([128.0] * 3)
                row['group'] = self.wall_group(row['name']) if kind == 'wall' else None
                out.append(row)
            self.rows[kind] = out

    @staticmethod
    def wall_group(name):
        low = name.lower()
        for word, g in GC_WALL_WORDS:
            if word in low:
                return g
        return 'other'

    def candidates(self, kind):
        """Coverings a converted one may stand on: for sale, not pattern 0 (no paper / the lot's grass),
        floors not grass, invisible or debug ones (kind 8, 9, 10; price 0)."""
        return [r for r in self.rows[kind] if r['index'] > 0 and r['buy'] and r['price'] > 0 and
                (kind == 'wall' or r['kindf'] not in (8, 9, 10))]

    def pick_base(self, c, mean):
        cands = self.candidates(c.kind)
        if c.kind == 'wall':
            want = PC_TO_GC_WALL.get(c.group, c.group)
            same = [r for r in cands if r['group'] == want] or [r for r in cands if r['group'] == 'paint']
        else:
            same = [r for r in cands if r['sound'] == c.sound] or cands
        best = min(same, key=lambda r: float(((r['mean'] - mean) ** 2).sum()))
        return best


# ---------------------------------------------------------------------------------------------------
# Pictures

def load_rgba(inst, name, maxdim):
    """The largest level <= maxdim of the PC texture (RGBA), or None."""
    import pcobj_convert
    r = pcobj_convert.load_texture(inst, name, maxdim)
    return None if r is None else r[0]


def resize(img, w, h):
    return np.asarray(Image.fromarray(img).resize((w, h), Image.LANCZOS))


def bake_bump(rgb, normal):
    """A bump-mapped PC wall (paint's fine grain, embossed paper): the normal map lit from the upper
    left, kept faint, multiplied in (the GameCube has no bump maps)."""
    n = normal[..., :3].astype(np.float32) / 127.5 - 1.0
    light = np.array([-0.35, 0.45, 0.82], np.float32)
    light /= np.linalg.norm(light)
    lit = (n * light).sum(-1)
    lit = lit - lit.mean()
    shade = np.clip(1.0 + 0.6 * lit, 0.75, 1.25)
    return np.clip(rgb.astype(np.float32) * shade[..., None], 0, 255).astype(np.uint8)


def mip_levels(rgb, mindim=8):
    out = [rgb]
    while min(out[-1].shape[:2]) > mindim and len(out) < 4:
        h, w = out[-1].shape[:2]
        out.append(np.asarray(Image.fromarray(out[-1]).resize((w // 2, h // 2), Image.BOX)))
    return out


def make_c8_txfl(name, rgb, flags):
    """A TXFL resource: GX C8 (8x4 tiles) with a 256-colour RGBA8 palette stored as its two IA8 halves
    (G,R then A,B), mip levels sharing the palette - the disc's own wall and floor textures' format."""
    levels = mip_levels(rgb)
    base = Image.fromarray(levels[0], 'RGB')
    pal_img = base.quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(pal_img.getpalette()[:768] + [0] * max(0, 768 - len(pal_img.getpalette()[:768])), np.uint8).reshape(256, 3)
    data = bytearray()
    for k, lv in enumerate(levels):
        q = np.asarray(Image.fromarray(lv, 'RGB').quantize(palette=pal_img, dither=Image.Dither.FLOYDSTEINBERG
                                                           if k == 0 else Image.Dither.NONE), np.uint8)
        h, w = q.shape
        data += q.reshape(h // 4, 4, w // 8, 8).transpose(0, 2, 1, 3).tobytes()
    rg = np.stack([pal[:, 1], pal[:, 0]], 1).astype(np.uint8)
    ba = np.stack([np.full(256, 255, np.uint8), pal[:, 2]], 1).astype(np.uint8)
    data += rg.tobytes() + ba.tobytes()
    h, w = levels[0].shape[:2]
    hdr = struct.pack('>IIII4H4BI', 0, 0, flags | 0x20 | 0x80, 0, w, h, 256, len(levels), FMT_C8, 2, 8, 0x20, 0)
    return gcdata.make_header(9, 0x5458464C, 0xFFFFFFFF, name, hdr + bytes(data))


def make_shader(template, name, texture):
    """A copy of the base covering's disc shader with its layer's texture changed."""
    ver, sig, var, _, off, size = gcdata.data_header(template)
    d = bytearray(template[off:off + size])
    struct.pack_into('>I', d, 0x64, texture)
    return gcdata.make_header(ver, sig, var, name, bytes(d))


def wall_picture(img):
    """A PC wall picture (rows from the wall's top down, columns along the wall) -> the GameCube's
    (columns from the floor up the wall's height, rows along the wall)."""
    rgb = img[..., :3]
    rot = np.ascontiguousarray(np.rot90(rgb, -1))  # PC bottom row -> left column (x = 0: the floor)
    return resize(rot, WALL_SIZE[0], WALL_SIZE[1])


# ---------------------------------------------------------------------------------------------------
# Converting

class Converter:
    def __init__(self, pc, gc_path, log=log_print):
        self.log = log
        self.packs = pcinstall.find_packs(pc)
        if not self.packs:
            raise SystemExit(f'{pc}: no TSData/Res folder found')
        paths = []
        for p in reversed(self.packs):  # newest first
            paths += p.scenegraph_packages()
        self.inst = sg.Install(paths)
        self.gc = gcdata.GcData(gc_path) if gc_path else None
        self.sets = GcSets(self.gc) if self.gc else None
        self._tex = {}

    def scale_of(self, props):
        try:
            s = float(props.get('floorMaterialScaleU', '1'))
        except ValueError:
            s = 1.0
        return max(1, min(8, int(round(s))))

    def picture(self, c):
        """(RGB picture in the GameCube's layout, its source texture name, scale) or raises."""
        td, _ = self.inst.find(rcol.T_TXMT, c.material)
        if td is None:
            raise LookupError(f'material {c.material} not found')
        t = rcol.parse_txmt(td)
        props = t['props']
        base = props.get('stdMatBaseTextureName')
        if not base:
            raise LookupError(f'material {c.material} has no texture')
        if c.kind == 'wall':
            img = load_rgba(self.inst, base, 512)
            if img is None:
                raise LookupError(f'texture {base} not found')
            src = f'{img.shape[1]}x{img.shape[0]}'
            if img.shape[0] < 64 or img.shape[1] < 32:  # a solid paint (4 x 4): its colour
                img = np.asarray(Image.fromarray(img).resize((256, 512), Image.BILINEAR))
            pic = wall_picture(img)
            bump = props.get('stdMatNormalMapTextureName')
            if bump and t['type'] == 'WallpaperBump':
                nimg = load_rgba(self.inst, bump, 512)
                if nimg is not None:
                    pic = bake_bump(pic, wall_picture(nimg))
                    src += '+bump'
            return pic, src, 1
        scale = self.scale_of(props)
        img = load_rgba(self.inst, base, 1024)
        if img is None:
            raise LookupError(f'texture {base} not found')
        src = f'{img.shape[1]}x{img.shape[0]}'
        side = min(FLOOR_MAX, FLOOR_TILE * scale)
        side = 1 << (side.bit_length() - 1)
        pic = resize(np.ascontiguousarray(img[..., :3]), side, side)
        # the GameCube's floor strips map a picture's columns mirrored as seen from above (its own floors
        # are stored that way): mirrored here, a PC floor reads as on the PC (lettering, inlays)
        return np.ascontiguousarray(pic[:, ::-1]), src, scale

    def convert(self, c, taken):
        """c's resources: {(manager, id): bytes}; fills c's base, shader, texture, scale."""
        pic, src, scale = self.picture(c)
        c.scale = scale
        c.texture = src
        base = self.sets.pick_base(c, pic.reshape(-1, 3).mean(0))
        c.gc_base, c.gc_base_name = base['shader'], base['name']
        rid = new_id(self.gc, c.key, taken)
        tmpl_tex = self.gc.get('textures', base['shader'])
        flags = struct.unpack_from('>I', tmpl_tex, gcdata.data_header(tmpl_tex)[4] + 8)[0] & ~0xA0 if tmpl_tex else 0x8C3
        tex = make_c8_txfl(c.key, pic, flags)
        shd = make_shader(self.gc.get('shaders', base['shader']), c.key, rid)
        c.shader = rid
        return {('textures', rid): tex, ('shaders', rid): shd}, pic


def new_id(gc, key, taken):
    """A resource id the disc's shaders and textures do not use (the same for both)."""
    rid = gcdata.symbol_id('pccover_' + key)
    while gc.has('shaders', rid) or gc.has('textures', rid) or rid in taken or rid == 0:
        rid = (rid * 1103515245 + 12345) & 0xFFFFFFFF
    taken.add(rid)
    return rid


def folder_of(c):
    if c.kind == 'wall':
        return f'pcw{WALL_GROUPS.index(c.group) + 1}_{c.group}'
    return f'pcf{FLOOR_GROUPS.index(c.group) + 1}_{c.group}'


def ini_section(c):
    lines = [f'[{c.kind}]', f'key = {c.key}', f'name = {c.name}']
    if c.desc:
        lines.append(f'desc = {c.desc}')
    lines += [f'price = {c.price}', f'shader = 0x{c.shader:08X}', f'base = 0x{c.gc_base:08X}']
    if c.kind == 'floor':
        lines.append(f'sound = {c.sound}')
        if c.scale > 1:
            lines.append(f'scale = {c.scale}')
    lines += [f'group = {c.group}', f'pack = {c.home.name}', '']
    return '\n'.join(lines)


def gc_group(c):
    """The GameCube catalogue's grouping: its tab, a floor's footstep surface."""
    if c.status:
        return ''
    if c.kind == 'wall':
        return 'Wallpaper'
    return f'Floors: {SOUND_NAMES.get(c.sound, str(c.sound))}'


def inv_row(c):
    return [c.home.code, c.kind, c.name, f'0x{c.guid:08X}', c.subsort, gc_group(c), str(c.price), c.status or 'converted',
            c.reason, c.key, f'0x{c.gc_base:08X}' if c.gc_base else '', c.gc_base_name, c.material, c.texture, str(c.scale),
            c.folder, c.desc]


def run(pc, gc_path, out, kinds=('wall', 'floor'), packs_filter=None, jobs=0, log=log_print, inventory_only=None):
    """Convert (or, inventory_only = a .tsv path, just list) the coverings; returns {kind: (found,
    converted, skipped)}."""
    t0 = time.time()
    conv = Converter(pc, gc_path if not inventory_only else None, log=log)
    log(f'Reading the wall and floor coverings of {len(conv.packs)} pack(s):')
    items = inventory(conv.packs, kinds, log)
    todo = []
    for c in items.values():
        if packs_filter and c.home.code not in packs_filter:
            continue
        r = skip_reason(c)
        if r:
            c.status, c.reason = 'skipped', r
        todo.append(c)
    todo.sort(key=lambda c: (c.kind != 'wall', folder_of(c), c.name.lower(), c.guid))
    if inventory_only:
        write_inventory(inventory_only, todo)
        log(f'wrote {inventory_only} ({len(todo)} coverings, {time.time() - t0:.1f} s)')
        return
    if not out:
        raise SystemExit('--out is needed to convert')
    out_real = os.path.realpath(out)
    here = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', '..'))
    if out_real == here or out_real.startswith(here + os.sep):
        raise SystemExit('refusing to write converted PC game data into the source tree')
    os.makedirs(out, exist_ok=True)
    work = [c for c in todo if not c.status]
    n = len(work)
    log(f'{len(todo)} covering(s) ({sum(c.kind == "wall" for c in todo)} walls, {sum(c.kind == "floor" for c in todo)} floors), '
        f'{n} to convert')
    # folders written by an earlier run of this kind go (converting again replaces them)
    for k in kinds:
        for d in glob.glob(os.path.join(out, 'pcw[0-9]_*' if k == 'wall' else 'pcf[0-9]_*')):
            if os.path.isfile(os.path.join(d, 'mod.ini')):
                shutil.rmtree(d)
    workers = jobs or max(1, (os.cpu_count() or 2) - 1)
    taken = set()
    results = {}
    args = [(c.guid, c.kind) for c in work]
    if workers > 1 and n > 8:
        import multiprocessing as mp
        with mp.Pool(workers, _pool_init, (pc, gc_path, items)) as pool:
            for k, (guid, res) in enumerate(pool.imap_unordered(_pool_work, args, chunksize=8)):
                results[guid] = res
                _progress(log, k, n, t0, items[guid], res)
    else:
        _pool_init(pc, gc_path, items, conv)
        for k, a in enumerate(args):
            guid, res = _pool_work(a)
            results[guid] = res
            _progress(log, k, n, t0, items[guid], res)
    # ids are made per worker: clashes (vanishingly rare) are renumbered here
    groups = {}
    for c in work:
        res = results.get(c.guid)
        if not res or 'error' in res:
            c.status, c.reason = 'failed', (res or {}).get('error', 'no result')
            continue
        for k in ('gc_base', 'gc_base_name', 'texture', 'scale', 'sound', 'shader'):
            setattr(c, k, res[k])
        if c.shader in taken:
            c.shader = new_id(conv.gc, c.key + '_', taken)
            res['shader_bytes'] = make_shader(conv.gc.get('shaders', c.gc_base), c.key, c.shader)
        else:
            taken.add(c.shader)
        c.folder = folder_of(c)
        groups.setdefault(c.folder, []).append((c, res))
    for folder, lst in sorted(groups.items()):
        d = os.path.join(out, folder)
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, 'mod.ini'), 'w', encoding='utf-8') as f:
            kind = lst[0][0].kind
            f.write(f'# {"Wall" if kind == "wall" else "Floor"} coverings converted from the PC game ({lst[0][0].group}), '
                    f'by tools/pcobj/pcwall.py. Your own copy of the game\'s data: do not share it.\n\n')
            for c, res in lst:
                f.write(ini_section(c) + '\n')
        for c, res in lst:
            with open(os.path.join(d, f'textures_{c.shader:08X}.res'), 'wb') as f:
                f.write(res['texture_bytes'])
            with open(os.path.join(d, f'shaders_{c.shader:08X}.res'), 'wb') as f:
                f.write(res['shader_bytes'])
        preview(os.path.join(d, 'preview.png'), [res['picture'] for c, res in lst], kind)
    stats = {}
    for c in todo:
        st = stats.setdefault(c.kind, [0, 0, 0])
        st[0] += 1
        st[1] += c.status == ''
        st[2] += c.status != ''
    for kind, fn in (('wall', 'pcwall_inventory.tsv'), ('floor', 'pcfloor_inventory.tsv')):
        if kind in kinds:
            write_inventory(os.path.join(out, fn), [c for c in todo if c.kind == kind])
    log(f'coverings done in {time.time() - t0:.0f} s: ' +
        ', '.join(f'{k}s {v[0]} found, {v[1]} converted, {v[2]} skipped' for k, v in stats.items()))
    return stats


_pool_items = None
_pool_conv = None


def _pool_init(pc, gc_path, items, conv=None):
    global _pool_items, _pool_conv
    _pool_items = items
    _pool_conv = conv or Converter(pc, gc_path, log=lambda *a: None)


def _pool_work(a):
    guid, kind = a
    c = _pool_items[guid]
    try:
        res, pic = _pool_conv.convert(c, set())
        d = dict(gc_base=c.gc_base, gc_base_name=c.gc_base_name, texture=c.texture, scale=c.scale, sound=c.sound,
                 shader=c.shader, texture_bytes=res[('textures', c.shader)], shader_bytes=res[('shaders', c.shader)],
                 picture=np.asarray(Image.fromarray(np.ascontiguousarray(pic[:, ::-1]) if kind == 'floor' else pic)
                                    .resize((32, 32) if kind == 'floor' else (64, 32), Image.BOX)))
        return guid, d
    except Exception as e:  # noqa: BLE001 - reported per covering
        return guid, {'error': f'{type(e).__name__}: {e}'}


def _progress(log, k, n, t0, c, res):
    if k % 50 == 0 or k + 1 == n or 'error' in res:
        st = res.get('error', 'converted')
        log(f'[{k + 1}/{n} {100 * (k + 1) // max(n, 1):3d}% {time.time() - t0:5.0f}s] {c.kind:5s} {c.name[:44]:44s} {st}')


def preview(path, pics, kind):
    """The group's pictures in a grid (walls stood up: the floor at the bottom), for the user."""
    if not pics:
        return
    if kind == 'wall':
        pics = [np.ascontiguousarray(np.rot90(p, 1)) for p in pics]  # 32 wide x 64 high
    h, w = pics[0].shape[:2]
    cols = min(len(pics), 24 if kind == 'wall' else 16)
    rows = (len(pics) + cols - 1) // cols
    sheet = np.full((rows * (h + 2), cols * (w + 2), 3), 255, np.uint8)
    for i, p in enumerate(pics):
        y, x = divmod(i, cols)
        sheet[y * (h + 2):y * (h + 2) + h, x * (w + 2):x * (w + 2) + w] = p[..., :3]
    Image.fromarray(sheet).save(path, optimize=True)


def write_inventory(path, todo):
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\t'.join(INV_COLUMNS) + '\n')
        for c in todo:
            f.write('\t'.join(str(v).replace('\t', ' ') for v in inv_row(c)) + '\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pc', required=True, help='The Sims 2 (PC) install folder (all packs under it are read)')
    ap.add_argument('--gc', help='GameCube data: the disc image, or the folder with DATA/')
    ap.add_argument('--out', help='mods folder to write into (never the source tree)')
    ap.add_argument('--kinds', default='walls,floors', help='walls, floors or both (default)')
    ap.add_argument('--packs', help='only coverings whose home pack is one of these (codes: base, ep1..ep8, sp1..sp9)')
    ap.add_argument('--inventory', help='write the inventory (TSV) here and stop')
    ap.add_argument('--jobs', type=int, default=0, help='worker processes (default: CPUs - 1)')
    a = ap.parse_args()
    kinds = tuple(k.strip().lower().rstrip('s') for k in a.kinds.split(',') if k.strip())
    bad = [k for k in kinds if k not in ('wall', 'floor')]
    if bad:
        raise SystemExit(f'unknown kind(s): {", ".join(bad)} (walls, floors)')
    packs = set(a.packs.split(',')) if a.packs else None
    if a.inventory:
        run(a.pc, None, None, kinds, packs, a.jobs, inventory_only=a.inventory)
        return
    if not (a.gc and a.out):
        raise SystemExit('--gc and --out are needed to convert')
    run(a.pc, a.gc, a.out, kinds, packs, a.jobs)


if __name__ == '__main__':
    main()
