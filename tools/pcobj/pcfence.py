#!/usr/bin/env python3
"""PC mod: mods. Convert fences (and the PC's half walls and railings, which are fences there too) from
the user's own The Sims 2 (PC) install (base game and every expansion and stuff pack found) into a SIMMY
mod folder: build mode's Walls tab then lists them after the GameCube's own three fences.

    pcfence.py --pc "<The Sims 2 install>" --gc <GameCube disc image or data folder> --out <mods>
               [--packs base,ep1,...]
    pcfence.py --pc "<install>" --inventory fences.tsv          list, no output

(pcobj_convert.py --categories ...,fences runs this too.)

The PC keeps each fence as a property set (type "fence", resource type 0x2CB230B8) in every pack's
Catalog/Patterns/catpatterns.bundle.package: GUID, name, description, cost, and three models (CRES, in
the pack's scenegraph packages): the rail between two posts one tile apart (x 0..1), the diagonal rail
(x 0..1.41, turned 45 degrees by the game), and the post (on the tile corners). posttilespacing 0 puts a
post only where a run ends, turns or meets another; maywalkover lets Sims step over it (flowerbed
borders, pet thresholds); ishalfwall marks a half wall, whose models are only the part above a
1 m wall the PC builds itself (with wallpaper on it).

On the GameCube a fence is a wall style (TileWalls' style byte: 2 picket, 0xC friendship arch, 0xD
Japanese) drawn as ONE model per tile edge (x -0.5..0.5 along the edge, z up; the diagonal along
x = -y): no separate posts. The converter bakes each fence's rail and posts into such models: "model"
(posts at both ends) and "model_diag"; for a fence with posts only at its ends also the variants
without posts (model_ends: none, the -x post, the +x post) that the game picks from the run's
neighbours. A half wall gets a plain plaster wall body under its PC top (the GameCube has no half
walls). A fence without a diagonal rail (greenhouse walls) gets its straight rail stretched.

Each group becomes <out>/pcn<k>_<group>/ (1 fences, 2 borders: Sims walk over them, 3 halfwalls):
mod.ini ([fence] sections: key, name, desc, price, thumbnail, model, model_diag, model_ends,
model_diag_ends, walk, base, group, pack), models_<id>.res, shaders_<id>.res, textures_<id>.res,
preview.png (the thumbnails, for the user). <out>/pcfence_inventory.tsv lists every fence found,
converted or skipped (and why).

Never put the output in the source tree or share it: it is made of EA's PC game data, converted on the
user's machine from the user's own copy. See docs/mods.md.
"""
import argparse, glob, html, os, re, shutil, sys, time

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gcdata, gctex, pcinstall, pcmesh, rcol, sg  # noqa: E402
from dbpf import Package  # noqa: E402

T_XFNC = 0x2CB230B8
GROUPS = ['fences', 'borders', 'halfwalls']
# The GameCube's fences (FenceSet rows: style, name): what a lot saved with a converted fence shows
# when its mod is gone.
GC_FENCES = {2: 'Picket Fence', 0xC: 'Friendship Arch Fence', 0xD: 'Japanese Fence'}
JAPANESE_WORDS = ('bamboo', 'tropical', 'shinto', 'japan', 'analogia', 'asian', 'tie', 'fir', 'pine', 'stump')
PICKET_WORDS = ('picket', 'stake', 'flowerbed', 'plank', 'wood', 'divide', 'gardener', 'antiquity', 'border',
                'floral', 'diner', 'chic', 'arboreal')
HALFWALL_BODY = (226, 220, 208)   # the plaster under a half wall's top (the PC paints it with wallpaper)
HALFWALL_DEPTH = 0.075             # half the body's thickness (tiles)
INV_COLUMNS = ['pack', 'name', 'pc_guid', 'group', 'price', 'status', 'reason', 'key', 'gc_base', 'walk', 'post_spacing',
               'halfwall', 'rail', 'diagrail', 'post', 'height', 'models', 'textures', 'folder', 'desc']


def log_print(*a):
    print(*a, flush=True)


def clean_text(s):
    s = str(s or '')
    try:  # UTF-8 the CPF reader took for Latin-1 (curly quotes, accents)
        s = s.encode('latin-1').decode('utf-8')
    except (UnicodeEncodeError, UnicodeDecodeError):
        pass
    s = html.unescape(s).replace('\r', ' ').replace('\n', ' ').strip()
    return re.sub(r'\s+', ' ', s)


class Fence:
    __slots__ = ('guid', 'name', 'desc', 'price', 'rail', 'diagrail', 'post', 'height', 'walk', 'spacing', 'halfwall',
                 'show', 'deprecated', 'home', 'newest', 'status', 'reason', 'key', 'group', 'gc_base', 'folder',
                 'models', 'textures', 'thumb')

    def __init__(self, d, pack):
        self.guid = int(d['guid']) & 0xFFFFFFFF
        self.home = self.newest = pack
        self.status, self.reason, self.folder = '', '', ''
        self.models, self.textures, self.thumb = {}, [], None
        self.key = f'pcfence_{self.guid:08x}'
        self.update(d, pack)

    def update(self, d, pack):
        self.newest = pack
        self.name = clean_text(d.get('name'))
        self.desc = clean_text(d.get('description'))
        self.price = int(d.get('cost') or 0)
        self.rail = str(d.get('rail') or '')
        self.diagrail = str(d.get('diagrail') or '')
        self.post = str(d.get('post') or '')
        try:
            self.height = float(d.get('height') or 0)
        except ValueError:
            self.height = 0.0
        self.walk = int(d.get('maywalkover') or 0) != 0
        sp = d.get('posttilespacing')
        self.spacing = 1 if sp is None or str(sp).strip() == '' else int(sp)
        self.halfwall = int(d.get('ishalfwall') or 0) != 0
        self.show = int(d.get('showincatalog') or 0)
        self.deprecated = int(d.get('deprecated') or 0)
        self.group = 'halfwalls' if self.halfwall else 'borders' if self.walk else 'fences'
        self.gc_base = gc_base(self)


def gc_base(f):
    """The GameCube fence (its style) a lot falls back to without the mod: by what it is made of."""
    low = f.name.lower()
    if any(w in low for w in JAPANESE_WORDS):
        return 0xD
    if f.walk or any(w in low for w in PICKET_WORDS):
        return 2
    return 0xC


def pattern_packages(pack):
    return sorted(glob.glob(os.path.join(pack.res, 'Catalog', 'Patterns', '*.package')))


def inventory(packs, log=log_print):
    """Every fence of the packs (oldest first): {guid: Fence}; a fence in several packs' bundles: the
    newest definition, the oldest pack as its home."""
    items = {}
    for pk in packs:
        n = 0
        for f in pattern_packages(pk):
            p = Package(f)
            for e in p.of_type(T_XFNC):
                d = pcinstall.parse_cpf(p.read(e))
                if d.get('type') != 'fence' or 'guid' not in d:
                    continue
                g = int(d['guid']) & 0xFFFFFFFF
                if g in items:
                    items[g].update(d, pk)
                else:
                    items[g] = Fence(d, pk)
                n += 1
        if n:
            log(f'  {pk.name}: {n} fence(s)')
    return items


def skip_reason(f):
    if not f.show:
        return 'not in the PC catalogue'
    if f.deprecated:
        return 'deprecated on the PC'
    if not f.rail:
        return 'no rail model'
    return ''


# ---------------------------------------------------------------------------------------------------
# Geometry: PC model space (x along the rail from its first post, y across, z up) -> the GameCube's
# fence model space (x -0.5..0.5 along the tile edge, the diagonal from (-0.5, 0.5) to (0.5, -0.5)),
# y mirrored as the converted objects are (and the triangles' winding with it).

R2 = np.sqrt(2.0)


def to_gc(P, N, diag):
    G = P.copy()
    G[:, 1] = -G[:, 1]
    GN = N.copy()
    GN[:, 1] = -GN[:, 1]
    if not diag:
        G[:, 0] -= 0.5
        return G, GN
    G[:, 0] -= R2 / 2
    c = s = np.sqrt(0.5)  # turned -45 degrees: +x -> (0.707, -0.707)
    x, y = G[:, 0].copy(), G[:, 1].copy()
    G[:, 0], G[:, 1] = x * c + y * s, -x * s + y * c
    nx, ny = GN[:, 0].copy(), GN[:, 1].copy()
    GN[:, 0], GN[:, 1] = nx * c + ny * s, -nx * s + ny * c
    return G, GN


def box(x0, x1, y0, y1, z0, z1):
    """A closed box's (P, N, U, T) (UVs 0: a plain colour)."""
    faces = [((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
             ((-1, 0, 0), [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)]),
             ((0, 1, 0), [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)]),
             ((0, -1, 0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
             ((0, 0, 1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)])]
    P, N, T = [], [], []
    for n, quad in faces:
        k = len(P)
        P += quad
        N += [n] * 4
        T += [(k, k + 1, k + 2), (k, k + 2, k + 3)]
    P, N = np.array(P, np.float64), np.array(N, np.float64)
    return P, N, np.zeros((len(P), 2)), np.array(T, np.int64)


class Converter:
    def __init__(self, pc, gc_path, log=log_print, texsize=256):
        self.log = log
        self.packs = pcinstall.find_packs(pc)
        if not self.packs:
            raise SystemExit(f'{pc}: no TSData/Res folder found')
        paths = []
        for p in reversed(self.packs):  # newest first
            paths += p.scenegraph_packages()
        self.inst = sg.Install(paths)
        self.texsize = texsize
        self.gc = gcdata.GcData(gc_path) if gc_path else None
        if self.gc:
            # the shader template: a disc object's model shader, as the object converter's
            gco = gcdata.GcObjects(self.gc)
            defs = gco.files['art_sculpture_greek_female']
            sid = gcdata.parse_modl_shaders(self.gc.get('models', [d for d in defs if d['model']][0]['model']))[0]
            self.shader_template = self.gc.get('shaders', sid)
            self.thumb_template = self.gc.get('shaders', [d['thumbnail'] for d in defs if d['thumbnail']][0])
        self.taken = set()

    def meshes(self, name):
        """[(material, P, N, U, T)] of a PC model ([] for none); raises LookupError when missing."""
        if not name:
            return []
        return [(m[0], m[2], m[3], m[4], m[5]) for m in pcmesh.load_meshes(self.inst, name)]

    def convert(self, f):
        """f's resources: {file name: bytes}; fills f.models, f.textures, f.thumb (RGBA picture)."""
        import pcobj_convert as po
        rail = self.meshes(f.rail)
        if not rail:
            raise LookupError(f'{f.rail}: no meshes')
        try:
            diag = self.meshes(f.diagrail)
        except LookupError:
            diag = []
        if not diag:  # (greenhouse walls) the straight rail stretched over the diagonal
            diag = [(m, P * np.array([R2, 1.0, 1.0]), N, U, T) for m, P, N, U, T in rail]
            f.textures.append('diag:stretched')
        post = self.meshes(f.post) if f.post else []
        if f.halfwall:  # the wall the PC builds under a half wall's top
            zb = float(min(P[:, 2].min() for m, P, N, U, T in rail))
            if zb > 0.3:
                rail = rail + [('__body__',) + box(0.0, 1.0, -HALFWALL_DEPTH, HALFWALL_DEPTH, 0.0, zb)]
                diag = diag + [('__body__',) + box(0.0, R2, -HALFWALL_DEPTH, HALFWALL_DEPTH, 0.0, zb)]
                f.textures.append(f'body:{zb:.2f}')
        files = {}
        tex, shaders = {}, {}

        def shader_of(mat):
            if mat in shaders:
                return shaders[mat]
            if mat == '__body__':
                img, dxt1, mode, desc = np.tile(np.array([*HALFWALL_BODY, 255], np.uint8), (8, 8, 1)), None, 'opaque', 'plaster'
            else:
                img, dxt1, mode, desc = po.material_of(self.inst, mat, self.texsize)
            if mode == 'skip':
                shaders[mat] = None
                return None
            tid = po.new_id(self.gc, 'textures', f'{f.key}_tex{len(tex)}', self.taken)
            data, fmt, size = gctex.make_txfl(f'{f.key}_{len(tex)}', img,
                                              'cutout' if mode == 'cutout' else ('blend' if mode == 'blend' else 'opaque'),
                                              self.texsize, dxt1)
            files[f'textures_{tid:08X}.res'] = data
            tex[mat] = img
            f.textures.append(f'{desc}:{fmt}{size[0]}x{size[1]}:{mode}')
            sid = po.new_id(self.gc, 'shaders', f'{f.key}_shader{len(shaders)}', self.taken)
            files[f'shaders_{sid:08X}.res'] = po.make_shader(self.shader_template, f'{f.key}_{len(shaders)}', tid, mode)
            shaders[mat] = sid
            return sid

        def parts(meshes, diagonal, posts):
            """GameCube model parts: the rail and a post at each end in `posts` (-1: the -x end, 1: +x)."""
            out = []
            ends = {-1: (-0.5, 0.0), 1: (0.5, 0.0)} if not diagonal else {-1: (-0.5, 0.5), 1: (0.5, -0.5)}
            for mat, P, N, U, T in meshes:
                sid = shader_of(mat)
                if sid is None or not len(T):
                    continue
                G, GN = to_gc(P, N, diagonal)
                out.append((sid, G, GN, U.copy(), T[:, ::-1]))
            for e in posts:
                ex, ey = ends[e]
                for mat, P, N, U, T in post:
                    sid = shader_of(mat)
                    if sid is None or not len(T):
                        continue
                    G = P.copy()
                    G[:, 1] = -G[:, 1]
                    G[:, 0] += ex
                    G[:, 1] += ey
                    GN = N.copy()
                    GN[:, 1] = -GN[:, 1]
                    out.append((sid, G, GN, U.copy(), T[:, ::-1]))
            return out

        def model(tag, p):
            mid = po.new_id(self.gc, 'models', f'{f.key}_{tag}', self.taken)
            files[f'models_{mid:08X}.res'] = po.make_modl(f'{f.key}_{tag}', p)
            f.models[tag] = mid
            return mid

        both = (-1, 1) if post else ()
        model('model', parts(rail, False, both))
        model('model_diag', parts(diag, True, both))
        if post and f.spacing == 0:  # posts only at a run's ends: the variants without them
            for tag, ends in (('none', ()), ('nx', (-1,)), ('px', (1,))):
                model(f'ends_{tag}', parts(rail, False, ends))
                model(f'diag_ends_{tag}', parts(diag, True, ends))
        # the catalogue thumbnail: two tiles of the fence seen from three quarters
        rendered = []
        for dx in (0.0, 1.0):
            for mat, P, N, U, T in rail:
                if shaders.get(mat):
                    rendered.append((P + np.array([dx, 0.0, 0.0]), N, U, T, tex[mat]))
        for dx in ((0.0, 1.0, 2.0) if post else ()):
            for mat, P, N, U, T in post:
                if shaders.get(mat):
                    rendered.append((P + np.array([dx, 0.0, 0.0]), N, U, T, tex[mat]))
        timg = po.render_thumb(rendered, 64)
        ttid = po.new_id(self.gc, 'textures', f'{f.key}_thumbtex', self.taken)
        files[f'textures_{ttid:08X}.res'] = gctex.make_thumb_txfl(f'{f.key}_thumb', timg)
        thumb = po.new_id(self.gc, 'shaders', f'{f.key}_thumb', self.taken)
        files[f'shaders_{thumb:08X}.res'] = po.make_shader(self.thumb_template, f'{f.key}_thumb', ttid, 'blend')
        f.models['thumbnail'] = thumb
        f.thumb = timg
        return files


def folder_of(f):
    return f'pcn{GROUPS.index(f.group) + 1}_{f.group}'


def ini_section(f):
    m = f.models
    lines = ['[fence]', f'key = {f.key}', f'name = {f.name}']
    if f.desc:
        lines.append(f'desc = {f.desc}')
    lines += [f'price = {f.price}', f'thumbnail = 0x{m["thumbnail"]:08X}', f'model = 0x{m["model"]:08X}',
              f'model_diag = 0x{m["model_diag"]:08X}']
    if 'ends_none' in m:
        lines.append('model_ends = ' + ' '.join(f'0x{m["ends_" + k]:08X}' for k in ('none', 'nx', 'px')))
        lines.append('model_diag_ends = ' + ' '.join(f'0x{m["diag_ends_" + k]:08X}' for k in ('none', 'nx', 'px')))
    if f.walk:
        lines.append('walk = 1')
    lines += [f'base = 0x{f.gc_base:X}', f'group = {f.group}', f'pack = {f.home.name}', '']
    return '\n'.join(lines)


def inv_row(f):
    return [f.home.code, f.name, f'0x{f.guid:08X}', f.group, str(f.price), f.status or 'converted', f.reason,
            f.key if not f.status else '', f'0x{f.gc_base:X} {GC_FENCES[f.gc_base]}', str(int(f.walk)), str(f.spacing),
            str(int(f.halfwall)), f.rail, f.diagrail, f.post, f'{f.height:g}', str(len([k for k in f.models if k != 'thumbnail'])),
            ' '.join(f.textures), f.folder, f.desc]


def write_inventory(path, todo):
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write('\t'.join(INV_COLUMNS) + '\n')
        for f in todo:
            fh.write('\t'.join(str(v).replace('\t', ' ') for v in inv_row(f)) + '\n')


def preview(path, pics):
    """The group's thumbnails in a grid, for the user."""
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
    """Convert (or, inventory_only = a .tsv path, just list) the fences; returns (found, converted,
    skipped)."""
    t0 = time.time()
    conv = Converter(pc, gc_path if not inventory_only else None, log=log)
    log(f'Reading the fences of {len(conv.packs)} pack(s):')
    items = inventory(conv.packs, log)
    todo = []
    for f in items.values():
        if packs_filter and f.home.code not in packs_filter:
            continue
        r = skip_reason(f)
        if r:
            f.status, f.reason = 'skipped', r
        todo.append(f)
    todo.sort(key=lambda f: (folder_of(f), f.name.lower(), f.guid))
    if inventory_only:
        write_inventory(inventory_only, todo)
        log(f'wrote {inventory_only} ({len(todo)} fences, {time.time() - t0:.1f} s)')
        return
    if not out:
        raise SystemExit('--out is needed to convert')
    out_real = os.path.realpath(out)
    here = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', '..'))
    if out_real == here or out_real.startswith(here + os.sep):
        raise SystemExit('refusing to write converted PC game data into the source tree')
    os.makedirs(out, exist_ok=True)
    for d in glob.glob(os.path.join(out, 'pcn[0-9]_*')):  # an earlier run's folders go
        if os.path.isfile(os.path.join(d, 'mod.ini')):
            shutil.rmtree(d)
    work = [f for f in todo if not f.status]
    log(f'{len(todo)} fence(s), {len(work)} to convert')
    groups = {}
    for k, f in enumerate(work):
        try:
            files = conv.convert(f)
        except Exception as e:  # noqa: BLE001 - reported per fence
            f.status, f.reason = 'failed', f'{type(e).__name__}: {e}'
            log(f'[{k + 1}/{len(work)}] {f.name[:50]}: {f.reason}')
            continue
        f.folder = folder_of(f)
        groups.setdefault(f.folder, []).append((f, files))
        if k % 25 == 0 or k + 1 == len(work):
            log(f'[{k + 1}/{len(work)} {time.time() - t0:5.0f}s] {f.name[:50]}')
    for folder, lst in sorted(groups.items()):
        d = os.path.join(out, folder)
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, 'mod.ini'), 'w', encoding='utf-8') as fh:
            fh.write(f'# Fences converted from the PC game ({lst[0][0].group}), by tools/pcobj/pcfence.py. '
                     f'Your own copy of the game\'s data: do not share it.\n\n')
            for f, files in lst:
                fh.write(ini_section(f) + '\n')
        for f, files in lst:
            for name, data in files.items():
                with open(os.path.join(d, name), 'wb') as fh:
                    fh.write(data)
        preview(os.path.join(d, 'preview.png'), [f.thumb for f, files in lst])
    write_inventory(os.path.join(out, 'pcfence_inventory.tsv'), todo)
    found, done = len(todo), sum(1 for f in todo if not f.status)
    log(f'fences done in {time.time() - t0:.0f} s: {found} found, {done} converted, {found - done} skipped')
    return found, done, found - done


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pc', required=True, help='The Sims 2 (PC) install folder (all packs under it are read)')
    ap.add_argument('--gc', help='GameCube data: the disc image, or the folder with DATA/')
    ap.add_argument('--out', help='mods folder to write into (never the source tree)')
    ap.add_argument('--packs', help='only fences whose home pack is one of these (codes: base, ep1..ep8, sp1..sp9)')
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
