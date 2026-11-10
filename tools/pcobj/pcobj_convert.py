#!/usr/bin/env python3
"""PC mod: mods. Convert a decorative object from the user's own The Sims 2 (PC) install into a SIMMY
mod folder.

    pcobj_convert.py --pc "<The Sims 2 install>" --gc <GameCube data folder or disc image> \
        --object <PC GUID | part of its name> --base <GameCube object file> --out <mods folder>
    pcobj_convert.py --pc "<install>" --list ficus          (find PC objects)

Reads the PC game's packages (objects.package, Sims3D/*.package) and the GameCube data at conversion
time; writes <out>/pc_<object>/: mod.ini (the object: a copy of the GameCube base object with the PC
object's GUID, name, description and price) and the converted resources in the GameCube's own formats
(models_<id>.res: MODL with one float indexed strip per material; shaders_<id>.res: the base model's
first shader with the converted texture; textures_<id>.res: RGBA8 TXFL; plus a catalogue thumbnail).

Never put the output in the source tree or share it: it is made of EA's PC game data, converted on the
user's machine from the user's own copy. See docs/mods.md.
"""
import argparse, glob, os, struct, sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dbpf import Package, fname64, strings_of_str  # noqa: E402
import rcol, sg, gcdata  # noqa: E402

T_OBJD, T_STR = 0x4F424A44, 0x53545223,
T_CTSS = 0x43545353


# ---------------------------------------------------------------------------------------------------
# The PC install

def res_dirs(root):
    """Every TSData/Res folder under `root` (the base game first, then the packs, sorted)."""
    root = os.path.abspath(root)
    hits = []
    for dp, dn, fn in os.walk(root):
        if os.path.basename(dp).lower() == 'res' and os.path.basename(os.path.dirname(dp)).lower() == 'tsdata':
            hits.append(dp); dn[:] = []
        elif dp.count(os.sep) - root.count(os.sep) > 4:
            dn[:] = []
    hits.sort(key=lambda p: (0 if os.sep + 'The Sims 2' + os.sep in p + os.sep and 'Sims 2 ' not in os.path.relpath(p, root).split(os.sep)[0] else 1, p))
    return hits


def object_packages(dirs):
    out = []
    for r in dirs:
        out += sorted(glob.glob(os.path.join(r, 'Objects', '*.package')))
    return out


def find_objects(pkgs, query):
    """(package, group, OBJD name, GUID) for objects whose GUID or OBJD name matches."""
    q = query.lower()
    qguid = int(query, 16) if all(c in '0123456789abcdefx' for c in q) and len(q) >= 6 else None
    out = []
    for path in pkgs:
        pk = Package(path)
        for e in pk.of_type(T_OBJD):
            d = pk.read(e)
            name = fname64(d); guid = struct.unpack_from('<I', d, 0x5C)[0]
            if guid == qguid or (qguid is None and q in name.lower()):
                out.append((pk, e.group, name, guid))
    return out


def object_strings(pk, group, inst, lang=1):
    for e in pk.entries:
        if e.group == group and e.type in (T_STR, T_CTSS) and e.inst == inst:
            return [v for (l, v, _) in strings_of_str(pk.read(e)) if l == lang]
    return []


# ---------------------------------------------------------------------------------------------------
# Textures

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
    pal = np.stack([p0, p1, p2, p3], 1)  # (n, 4, 3)
    bits = cb[:, 4:8].astype(np.uint32)
    idx = (bits[:, 0] | bits[:, 1] << 8 | bits[:, 2] << 16 | bits[:, 3] << 24)
    sel = np.stack([(idx >> (2 * k)) & 3 for k in range(16)], 1)  # (n, 16)
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
        return decode_dxt(data, w, h, {4: 4, 5: 5, 8: 8}[fmt])
    a = np.frombuffer(data, np.uint8)
    if fmt in (1, 7):  # BGRA
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
    d, e = inst.find(rcol.T_TXTR, name)
    if d is None: return None
    tx = rcol.parse_txtr(d)
    n = len(tx['levels'])
    best = None
    for i, (kind, val) in enumerate(tx['levels']):
        k = n - 1 - i
        w, h = max(1, tx['w'] >> k), max(1, tx['h'] >> k)
        if max(w, h) > maxdim: continue
        if kind == 'lifo':
            ld, le = inst.find(rcol.T_LIFO, val)
            if ld is None: continue
            val = rcol.parse_lifo(ld)['data']
        best = (w, h, val)  # levels go small -> large: keep the largest that fits
    if not best: return None
    w, h, data = best
    return decode_level(data, w, h, tx['format'])


def pow2_rgba(img, maxdim=256):
    from PIL import Image
    h, w = img.shape[:2]
    p2 = lambda v: 1 << max(2, min(maxdim.bit_length() - 1, (max(4, v) - 1).bit_length()))
    W, H = p2(w), p2(h)
    if (W, H) != (w, h):
        img = np.asarray(Image.fromarray(img, 'RGBA').resize((W, H), Image.LANCZOS))
    return img


def gx_rgba8(img):
    """RGBA rows (top first) -> GX RGBA8 4x4 tiles (AR plane, then GB plane per tile)."""
    h, w = img.shape[:2]
    t = img.reshape(h // 4, 4, w // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    ar = t[:, :, [3, 0]].reshape(-1, 32)
    gb = t[:, :, [1, 2]].reshape(-1, 32)
    return np.concatenate([ar, gb], 1).tobytes()


def make_txfl(name, img):
    h, w = img.shape[:2]
    # ETextureDef: +8 flags (wrap S/T, 0x80 swizzled, 0x400/0x800 as the disc's), +0x10 w, h, palette
    # size, mip count, +0x18 format 0x85 (RGBA8), palette format, bpp 32, palette bpp; +0x1C name.
    tdef = struct.pack('>IIII4H4BI', 0, 0, 0xC83, 0, w, h, 0, 0, 0x85, 0, 32, 0, 0)
    return gcdata.make_header(9, 0x5458464C, 0xFFFFFFFF, name, tdef + gx_rgba8(img))


# ---------------------------------------------------------------------------------------------------
# Meshes

def model_meshes(inst, cres_name, log):
    """[(material name, positions, normals, uvs, triangles)] of the CRES's shapes, shadows left out."""
    d, e = inst.find(rcol.T_CRES, cres_name)
    if d is None: raise SystemExit(f'{cres_name}_cres not found')
    links, _, _ = rcol.rcol_header(d)
    out = []
    for (t, g, i, ih) in links:
        if t != rcol.T_SHPE: continue
        pk, se = inst.find_tgi(t, i, ih)
        if not se: continue
        sh = rcol.parse_shpe(pk.read(se))
        strings = sh['strings']
        gmnds = [s for s in strings if s.lower().endswith('_gmnd')]
        pairs = {}
        rest = [s for s in strings if s not in gmnds and s not in ('cReferentNode', 'cObjectGraphNode')]
        for k in range(0, len(rest) - 1, 2):
            pairs[rest[k].lower()] = rest[k + 1]
        for gn in gmnds[:1]:  # the highest level of detail
            gd, _ = inst.find(rcol.T_GMND, gn.split('!')[-1])
            if gd is None: continue
            for (t3, g3, i3, ih3) in rcol.parse_names_generic(gd)['links']:
                if t3 != rcol.T_GMDC: continue
                pk3, e3 = inst.find_tgi(t3, i3, ih3)
                gm = rcol.parse_gmdc(pk3.read(e3))
                for gi, grp in enumerate(gm['groups']):
                    mat = pairs.get(grp['name'].lower())
                    if 'shadow' in grp['name'].lower() or (mat and 'shadow' in mat.lower()) or grp['prim'] != 2:
                        log(f'  skip group "{grp["name"]}" ({mat})'); continue
                    m, tris = rcol.gmdc_group_mesh(gm, gi)
                    if 'Vertices' not in m or not tris: continue
                    used = sorted({v for tri in tris for v in tri})
                    remap = {v: k for k, v in enumerate(used)}
                    P = np.array([m['Vertices'][v] for v in used], np.float32)
                    N = np.array([m['Normals'][v] for v in used], np.float32) if 'Normals' in m else np.zeros_like(P)
                    U = np.array([m['UV'][v] for v in used], np.float32) if 'UV' in m else np.zeros((len(used), 2), np.float32)
                    T = np.array([[remap[a] for a in tri] for tri in tris], np.int32)
                    log(f'  group "{grp["name"]}": {len(used)} vertices, {len(T)} triangles, material {mat}')
                    out.append((mat or grp['name'], P, N, U, T))
    return out


def material_texture(inst, mat):
    d, e = inst.find(rcol.T_TXMT, mat.split('!')[-1])
    if d is None: return None, {}
    tm = rcol.parse_txmt(d)
    return tm['props'].get('stdMatBaseTextureName'), tm['props']


def make_modl(name, parts):
    """parts: [(shader id, P, N, U, T)]. One sub-model; a float indexed strip (GX_TRIANGLES) per part."""
    body = bytearray()
    body += struct.pack('>I', 0) + b'\0' * 48 + b'\0'      # light positions, attachment colour
    body += struct.pack('>5I', 0, 0, 0, 0, 0)               # attachment verts, lattices, dummies, cameras, lights
    body += b'\0' + struct.pack('>f', 1.0) + struct.pack('>I', 1)  # flag, scale 1 (world units), one sub-model
    body += struct.pack('>II', 0, len(parts))
    allp = []
    for sid, P, N, U, T in parts:
        flags = 0x2A  # uvs | normals | indexed (float)
        body += struct.pack('>III', flags, sid, 1) + b'\0' + struct.pack('>f', 1.0)
        body += b'\0' + struct.pack('>I', len(P))
        pos = np.concatenate([P, np.zeros((len(P), 1), np.float32)], 1).astype('>f4')
        body += pos.tobytes()
        body += U.astype('>f4').tobytes()
        n = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
        nn = np.clip(np.round(n * 127), -127, 127).astype(np.int8)
        body += np.concatenate([nn, np.full((len(N), 1), 127, np.int8)], 1).tobytes()
        idx = T.reshape(-1).astype(np.uint16)
        dl = bytearray([0x97]) + struct.pack('>H', len(idx))  # GX_TRIANGLES | GX_VTXFMT7
        dl += np.repeat(idx[:, None], 3, 1).astype('>u2').tobytes()  # pos, nrm, tex0 indices
        dl += b'\0' * ((-len(dl)) % 32)
        body += struct.pack('>IBII', len(idx), 3, len(dl), 3) + dl
        body += b'\x06'
        allp.append(P)
    P = np.concatenate(allp)
    lo, hi = P.min(0), P.max(0)
    c = (lo + hi) / 2; r = float(np.linalg.norm(P - c, axis=1).max())
    body += struct.pack('>4f', *c, r) + struct.pack('>6f', *lo, *hi) * 2 + bytes([0, 0, 0, 0])
    return gcdata.make_header(0x3A, 0x4D4F444C, 0xFFFFFFFF, name, bytes(body))


def clone_shader(raw, name, texture, alpha_ref=None):
    ver, sig, var, _, off, size = gcdata.data_header(raw)
    d = bytearray(raw[off:off + size])
    struct.pack_into('>I', d, 0x64, texture)
    if alpha_ref is not None:
        L = 0x64
        struct.pack_into('>I', d, L + 0x10, struct.unpack_from('>I', d, L + 0x10)[0] | 4)  # alpha test
        struct.pack_into('>f', d, L + 0x14, alpha_ref)
    return gcdata.make_header(ver, sig, var, name, bytes(d))


# ---------------------------------------------------------------------------------------------------
# Thumbnail: a small software render (orthographic, three-quarter view)

def render_thumb(meshes, size=64, ss=4):
    S = size * ss
    col = np.zeros((S, S, 4), np.float32); zb = np.full((S, S), -1e9, np.float32)
    allP = np.concatenate([m[1] for m in meshes])
    yaw, pitch = np.radians(-35), np.radians(25)
    Ry = np.array([[np.cos(yaw), -np.sin(yaw), 0], [np.sin(yaw), np.cos(yaw), 0], [0, 0, 1]])
    Rx = np.array([[1, 0, 0], [0, np.cos(pitch), -np.sin(pitch)], [0, np.sin(pitch), np.cos(pitch)]])
    M = Rx @ Ry
    V = allP @ M.T
    lo, hi = V[:, [0, 2]].min(0), V[:, [0, 2]].max(0)
    scale = 0.9 * S / max(hi - lo); ctr = (lo + hi) / 2
    light = np.array([0.4, -0.6, 0.7]); light /= np.linalg.norm(light)
    for P, N, U, T, img in [(m[1], m[2], m[3], m[4], m[5]) for m in meshes]:
        Q = P @ M.T
        sx = (Q[:, 0] - ctr[0]) * scale + S / 2; sy = S / 2 - (Q[:, 2] - ctr[1]) * scale; sz = -Q[:, 1]
        shade = 0.55 + 0.45 * np.clip(N @ light, 0, 1)
        th, tw = img.shape[:2]
        for a, b, c in T:
            x0, x1 = int(max(0, np.floor(min(sx[a], sx[b], sx[c])))), int(min(S - 1, np.ceil(max(sx[a], sx[b], sx[c]))))
            y0, y1 = int(max(0, np.floor(min(sy[a], sy[b], sy[c])))), int(min(S - 1, np.ceil(max(sy[a], sy[b], sy[c]))))
            if x1 < x0 or y1 < y0: continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            den = (sy[b] - sy[c]) * (sx[a] - sx[c]) + (sx[c] - sx[b]) * (sy[a] - sy[c])
            if abs(den) < 1e-9: continue
            w0 = ((sy[b] - sy[c]) * (xs - sx[c]) + (sx[c] - sx[b]) * (ys - sy[c])) / den
            w1 = ((sy[c] - sy[a]) * (xs - sx[c]) + (sx[a] - sx[c]) * (ys - sy[c])) / den
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any(): continue
            z = w0 * sz[a] + w1 * sz[b] + w2 * sz[c]
            u = w0 * U[a, 0] + w1 * U[b, 0] + w2 * U[c, 0]; v = w0 * U[a, 1] + w1 * U[b, 1] + w2 * U[c, 1]
            tx = (np.mod(u, 1.0) * tw).astype(int).clip(0, tw - 1); ty = (np.mod(v, 1.0) * th).astype(int).clip(0, th - 1)
            texel = img[ty, tx].astype(np.float32)
            sh = w0 * shade[a] + w1 * shade[b] + w2 * shade[c]
            ok = inside & (texel[..., 3] >= 123)
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

def new_id(gc, mgr, key, taken):
    rid = gcdata.symbol_id(key)
    while gc.has(mgr, rid) or rid in taken or rid == 0:
        rid = (rid * 1103515245 + 12345) & 0xFFFFFFFF
    taken.add(rid)
    return rid


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pc', required=True, help='The Sims 2 (PC) install folder (or its TSData/Res)')
    ap.add_argument('--gc', help='GameCube data: the folder with DATA/, or the disc image')
    ap.add_argument('--list', help='list PC objects whose name contains this text')
    ap.add_argument('--object', help='PC object: GUID (hex) or part of its name')
    ap.add_argument('--base', help='GameCube object file to copy (behaviour, slots, catalogue tab)')
    ap.add_argument('--out', help='mods folder to write into (never the source tree)')
    ap.add_argument('--name', help='catalogue name (default: the PC catalogue name)')
    ap.add_argument('--price', type=int, help='price (default: the PC price)')
    ap.add_argument('--texsize', type=int, default=256)
    a = ap.parse_args()
    dirs = res_dirs(a.pc)
    if not dirs: raise SystemExit(f'{a.pc}: no TSData/Res folder found')
    pkgs = object_packages(dirs)
    log = print
    if a.list:
        for pk, g, name, guid in find_objects(pkgs, a.list):
            print(f'{guid:08X}  group {g:08X}  {name}  ({os.path.relpath(pk.path, a.pc)})')
        return
    if not (a.object and a.base and a.out and a.gc):
        raise SystemExit('--object, --base, --gc and --out are needed (or --list)')
    out_real = os.path.realpath(a.out)
    here = os.path.realpath(os.path.join(os.path.dirname(__file__), '..', '..'))
    if out_real == here or out_real.startswith(here + os.sep):
        raise SystemExit('refusing to write converted PC game data into the source tree')
    hits = find_objects(pkgs, a.object)
    if not hits: raise SystemExit(f'no PC object matches {a.object}')
    pk, group, objname, guid = hits[0]
    if len(hits) > 1: log(f'{len(hits)} objects match; taking the first: {objname}')
    d = [pk.read(e) for e in pk.entries if e.group == group and e.type == T_OBJD][0]
    price = struct.unpack_from('<H', d, 0x40 + 2 * 0x12)[0]  # OBJD field 0x12; 0x28 = catalogue function flags (the GC's bits)
    models = sorted({v for v in object_strings(pk, group, 0x85) if v})
    cat = object_strings(pk, group, next((e.inst for e in pk.entries if e.group == group and e.type == T_CTSS), 0x7D0))
    name = a.name or (cat[0] if cat else objname)
    desc = cat[1] if len(cat) > 1 else ''
    log(f'PC object "{objname}" GUID {guid:08X}, catalogue "{name}", price {price}, models {models}')
    gc = gcdata.GcData(a.gc)
    objs = gcdata.GcObjects(gc)
    if guid in objs.guids: raise SystemExit(f'GUID {guid:08X} is a GameCube object\'s')
    base = objs.files.get(a.base)
    if not base: raise SystemExit(f'GameCube object file {a.base} not found')
    bdef = base[0]
    if bdef['master'] != 0 or not bdef['model']:
        raise SystemExit(f'{a.base} is not a single-tile object with a model')
    base_shader = gc.get('shaders', gcdata.parse_modl_shaders(gc.get('models', bdef['model']))[0])
    base_thumb = gc.get('shaders', bdef['thumbnail']) if bdef['thumbnail'] else None

    inst = sg.Install(dirs)
    meshes = model_meshes(inst, models[0], log)
    if not meshes: raise SystemExit('no mesh found')
    tag = ''.join(c for c in objname.lower() if c.isalnum())[:24]
    folder = os.path.join(a.out, f'pc_{tag}')
    os.makedirs(folder, exist_ok=True)
    taken = set(); files = {}
    parts = []; rendered = []
    texcache = {}
    for k, (mat, P, N, U, T) in enumerate(meshes):
        tex, props = material_texture(inst, mat)
        if tex not in texcache:
            img = load_texture(inst, tex, a.texsize) if tex else None
            if img is None: img = np.full((8, 8, 4), 200, np.uint8)
            img = pow2_rgba(img, a.texsize)
            tid = new_id(gc, 'textures', f'pcmod_{guid:08x}_tex{k}', taken)
            files[f'textures_{tid:08X}.res'] = make_txfl(f'pc_{tag}_{k}', img)
            alpha = (props.get('stdMatAlphaTestEnabled') == '1' and img[..., 3].min() < 128)
            ref = int(props.get('stdMatAlphaRefValue', '128')) / 255.0 if alpha else None
            sid = new_id(gc, 'shaders', f'pcmod_{guid:08x}_shader{k}', taken)
            files[f'shaders_{sid:08X}.res'] = clone_shader(base_shader, f'pc_{tag}_{k}', tid, ref)
            texcache[tex] = (sid, img)
        sid, img = texcache[tex]
        parts.append((sid, P, N, U, T))
        rendered.append((mat, P, N, U, T, img))
    mid = new_id(gc, 'models', f'pcmod_{guid:08x}_model', taken)
    files[f'models_{mid:08X}.res'] = make_modl(f'pc_{tag}', parts)
    thumb_id = 0
    if base_thumb:
        timg = render_thumb(rendered, 64)
        ttid = new_id(gc, 'textures', f'pcmod_{guid:08x}_thumbtex', taken)
        files[f'textures_{ttid:08X}.res'] = make_txfl(f'pc_{tag}_thumb', timg)
        thumb_id = new_id(gc, 'shaders', f'pcmod_{guid:08x}_thumb', taken)
        files[f'shaders_{thumb_id:08X}.res'] = clone_shader(base_thumb, f'pc_{tag}_thumb', ttid)
        try:
            from PIL import Image
            Image.fromarray(timg, 'RGBA').save(os.path.join(folder, 'thumbnail.png'))
        except Exception:
            pass
    for fn, data in files.items():
        with open(os.path.join(folder, fn), 'wb') as f: f.write(data)
    one = lambda s: s.replace('\r', ' ').replace('\n', ' ').strip()
    with open(os.path.join(folder, 'mod.ini'), 'w', encoding='utf-8') as f:
        f.write('# PC mod: mods. Converted by tools/pcobj/pcobj_convert.py from the user\'s own The Sims 2 (PC)\n'
                '# install. EA game data: for this machine only, never share or commit it.\n')
        f.write(f'# PC object "{objname}" (models {", ".join(models)})\n[object]\n')
        f.write(f'guid = 0x{guid:08X}\nbase = {a.base}\nname = {one(name)}\ndesc = {one(desc)}\n')
        f.write(f'price = {a.price if a.price is not None else price}\nmodel = 0x{mid:08X}\n')
        if thumb_id: f.write(f'thumbnail = 0x{thumb_id:08X}\n')
    tris = sum(len(p[4]) for p in parts); verts = sum(len(p[1]) for p in parts)
    log(f'wrote {folder}: {len(files)} resources, model {mid:08X} ({verts} vertices, {tris} triangles)')


if __name__ == '__main__':
    main()
