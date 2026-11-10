#!/usr/bin/env python3
"""The Sims 2 PC scenegraph (RCOL) resources: CRES, SHPE, GMND, GMDC, TXMT, TXTR.

Parsers return plain dicts. Only what the feasibility study and the converter need is decoded; each
parser checks that it consumed its block exactly where that is cheap to check.
"""
import struct
from dbpf import read_7bit_string

T_CRES, T_SHPE, T_GMND, T_GMDC, T_TXMT, T_TXTR, T_LIFO, T_ANIM = (
    0xE519C933, 0xFC6EB1F7, 0x7BA3838C, 0xAC4F8687, 0x49596978, 0x1C4A276C, 0xED534136, 0xFB00791E)


class R:
    def __init__(self, data, p=0):
        self.d = data; self.p = p
    def u8(self):
        v = self.d[self.p]; self.p += 1; return v
    def u16(self):
        v = struct.unpack_from('<H', self.d, self.p)[0]; self.p += 2; return v
    def i16(self):
        v = struct.unpack_from('<h', self.d, self.p)[0]; self.p += 2; return v
    def u32(self):
        v = struct.unpack_from('<I', self.d, self.p)[0]; self.p += 4; return v
    def i32(self):
        v = struct.unpack_from('<i', self.d, self.p)[0]; self.p += 4; return v
    def f32(self):
        v = struct.unpack_from('<f', self.d, self.p)[0]; self.p += 4; return v
    def fs(self, n):
        v = struct.unpack_from(f'<{n}f', self.d, self.p); self.p += 4 * n; return v
    def s(self):
        v, self.p = read_7bit_string(self.d, self.p); return v
    def raw(self, n):
        v = self.d[self.p:self.p + n]; self.p += n; return v


def rcol_header(data):
    """RCOL: [u32 n or 0xFFFF0001 marker][links: group, inst, (insthi), type][u32 blocks][block ids]."""
    r = R(data)
    n = r.u32(); hi = False
    if n == 0xFFFF0001:
        hi = True; n = r.u32()
    links = []
    for _ in range(n):
        g = r.u32(); i = r.u32(); ih = r.u32() if hi else 0; t = r.u32()
        links.append((t, g, i, ih))
    nb = r.u32()
    ids = [r.u32() for _ in range(nb)]
    return links, ids, r


def sgresource(r):
    if r.d[r.p] != 11:  # cResourceNode: a flag byte first
        r.p += 1
    assert r.s() == 'cSGResource'
    r.u32(); r.u32()
    return r.s()


def block_head(r):
    name = r.s(); bid = r.u32(); ver = r.u32()
    return name, bid, ver


def rcol_name(data):
    """The cSGResource file name of an RCOL resource's first block (cheap: no full parse)."""
    links, ids, r = rcol_header(data)
    block_head(r)
    return sgresource(r)


# ---- GMDC -------------------------------------------------------------------------------------------

ELEMENT_NAMES = {
    0x5B830781: 'Vertices', 0x3B83078B: 'Normals', 0xBB8307AB: 'UV', 0xDB830795: 'UVDeltas',
    0x1C4AFC56: 'BlendIndices', 0x5C4AFC5C: 'BlendWeights', 0x3BD70105: 'BoneWeights?', 0xFBD70111: 'BoneAssign?',
    0x89D92BA0: 'Binormals', 0x69D92B93: 'Tangents', 0xCB6F3A6A: 'NormalMorphDeltas', 0x5CF2CFE1: 'MorphVertexDeltas',
    0xDCF2CFDC: 'MorphVertexMap', 0x114113C3: 'ColorDeltas?', 0xCB7206A1: 'Color', 0xEB720693: 'ColorDeltas',
    0x7C4DEE82: 'TargetIndices', 0x9BB38AFB: 'BoneWeights2?',
}
FMT_SIZE = {0: 1, 1: 2, 2: 3, 3: 1, 4: 1}  # float1, float2, float3, (?), dword: components


def parse_gmdc(data):
    links, ids, r = rcol_header(data)
    name, bid, ver = block_head(r)
    assert name == 'cGeometryDataContainer', name
    fname = sgresource(r)
    g = {'name': fname, 'version': ver, 'elements': [], 'links': [], 'groups': []}
    for _ in range(r.u32()):
        e = {}
        e['number'] = r.i32()
        e['id'] = r.u32()
        e['repeat'] = r.i32()
        e['bformat'] = r.i32()
        e['sformat'] = r.i32()
        ln = r.i32()
        if e['bformat'] == 4:
            e['data'] = list(struct.unpack_from(f'<{ln // 4}I', r.d, r.p))
        else:
            e['data'] = list(struct.unpack_from(f'<{ln // 4}f', r.d, r.p))
        r.p += ln
        cnt = r.i32()
        e['items'] = [r.u16() if ver == 4 else r.i32() for _ in range(cnt)]
        e['comps'] = FMT_SIZE.get(e['bformat'], 1)
        g['elements'].append(e)
    for _ in range(r.u32()):
        l = {}
        cnt = r.i32()
        l['items'] = [r.u16() if ver == 4 else r.i32() for _ in range(cnt)]
        l['refsize'] = r.i32()
        l['active'] = r.i32()
        l['aliases'] = []
        for _k in range(3):
            c = r.i32()
            l['aliases'].append([r.u16() if ver == 4 else r.i32() for _ in range(c)])
        g['links'].append(l)
    for _ in range(r.u32()):
        gr = {}
        gr['prim'] = r.u32()
        gr['link'] = r.i32()
        gr['name'] = r.s()
        c = r.i32()
        gr['faces'] = [r.u16() if ver == 4 else r.i32() for _ in range(c)]
        gr['opacity'] = r.i32()
        if ver != 1:
            c = r.i32()
            gr['joints'] = [r.u16() if ver == 4 else r.i32() for _ in range(c)]
        else:
            gr['joints'] = []
        g['groups'].append(gr)
    # model: transforms, blend group names, bounding mesh
    c = r.i32()
    g['transforms'] = [(r.fs(4), r.fs(3)) for _ in range(c)]
    c = r.i32()
    g['blendnames'] = [(r.s(), r.s()) for _ in range(c)]
    vc = r.i32()
    fc = r.i32() if vc else 0
    g['bbox_verts'] = [r.fs(3) for _ in range(vc)]
    g['bbox_faces'] = [r.u16() if ver == 4 else r.i32() for _ in range(fc)]
    # joints (subsets)
    c = r.i32()
    g['subsets'] = []
    for _ in range(c):
        vc = r.i32()
        if vc:
            fc = r.i32()
            vs = [r.fs(3) for _ in range(vc)]
            fs = [r.u16() if ver == 4 else r.i32() for _ in range(fc)]
        else:
            vs, fs = [], []
        g['subsets'].append((vs, fs))
    g['consumed'] = r.p == len(data)
    return g


def gmdc_group_mesh(g, gi):
    """(positions, normals, uvs, triangles) for group gi, resolved through its link's element order."""
    gr = g['groups'][gi]
    link = g['links'][gr['link']]
    out = {}
    for k, ei in enumerate(link['items']):
        e = g['elements'][ei]
        nm = ELEMENT_NAMES.get(e['id'], f"{e['id']:08X}")
        comps = e['comps']
        vals = e['data']
        verts = [tuple(vals[i * comps:(i + 1) * comps]) for i in range(len(vals) // comps)]
        alias = link['aliases'][min(k, 2)] if False else None
        out.setdefault(nm, verts)
    tris = [tuple(gr['faces'][i:i + 3]) for i in range(0, len(gr['faces']), 3)]
    return out, tris


# ---- GMND / SHPE / CRES (names and links only) -------------------------------------------------------

def parse_shpe(data):
    links, ids, r = rcol_header(data)
    name, bid, ver = block_head(r)
    assert name == 'cShape', name
    fname = sgresource(r)
    # cReferentNode? cShape: cSGResource, cReferentNode? We read: references (cObjectGraphNode)...
    # Rather than decode every sub-block, collect the strings: the GMND names and material names.
    strings = []
    p = r.p
    d = data
    while p < len(d) - 2:
        n = d[p]
        if 3 <= n < 120 and p + 1 + n <= len(d):
            s = d[p + 1:p + 1 + n]
            if all(32 <= c < 127 for c in s):
                strings.append(s.decode()); p += 1 + n; continue
        p += 1
    return {'name': fname, 'links': links, 'strings': strings}


def parse_names_generic(data):
    links, ids, r = rcol_header(data)
    name, bid, ver = block_head(r)
    k = r.d.find(b'\x0bcSGResource', r.p)
    r.p = k
    fname = sgresource(r)
    return {'block': name, 'name': fname, 'links': links, 'blockids': ids}


def cres_joints(data):
    """The cTransformNode names (bones/joints) of a CRES: every cObjectGraphNode tag string."""
    links, ids, r = rcol_header(data)
    out = []
    d = data
    # cTransformNode blocks follow a cObjectGraphNode with a name; scan for the pattern
    p = 0
    while True:
        p = d.find(b'\x10cObjectGraphNode', p)
        if p < 0: break
        q = p + 17 + 8
        try:
            c = struct.unpack_from('<I', d, q)[0]; q += 4
            q += c * 8  # extensions (enabled, dependant, index)
            if c > 1000: raise ValueError
            # version >= 4: name
            nm, q2 = read_7bit_string(d, q)
            out.append(nm)
        except Exception:
            pass
        p += 1
    return out


# ---- TXMT -------------------------------------------------------------------------------------------

def parse_txmt(data):
    links, ids, r = rcol_header(data)
    name, bid, ver = block_head(r)
    assert name == 'cMaterialDefinition', name
    fname = sgresource(r)
    desc = r.s(); mtype = r.s()
    props = {}
    for _ in range(r.u32()):
        k = r.s(); v = r.s(); props[k] = v
    texnames = []
    if ver > 8:
        for _ in range(r.u32()):
            texnames.append(r.s())
    return {'name': fname, 'desc': desc, 'type': mtype, 'props': props, 'textures': texnames, 'links': links}


# ---- TXTR -------------------------------------------------------------------------------------------

TXTR_FORMATS = {1: 'RAW_ARGB32', 2: 'RAW_RGB24', 3: 'ALPHA8', 4: 'DXT1', 5: 'DXT3', 6: 'GRAY8', 7: 'ALT_ARGB32', 8: 'DXT5', 9: 'RAW24'}


def parse_txtr(data):
    links, ids, r = rcol_header(data)
    name, bid, ver = block_head(r)
    assert name == 'cImageData', name
    fname = sgresource(r)
    w = r.u32(); h = r.u32(); fmt = r.u32(); mips = r.u32()
    r.f32(); textures = r.u32(); r.u32()  # purpose, outer loop (textures), unknown
    if ver == 9:
        filename2 = r.s()
    else:
        raise NotImplementedError(f'TXTR version {ver}')
    levels = []
    for _t in range(textures):
        n_m = r.u32()  # this texture's level count
        for _m in range(n_m):
            kind = r.u8()
            if kind == 0:
                n = r.u32(); levels.append(('data', r.raw(n)))
            else:
                levels.append(('lifo', r.s()))
    return {'name': fname, 'w': w, 'h': h, 'format': fmt, 'fmtname': TXTR_FORMATS.get(fmt, str(fmt)), 'mips': mips,
            'levels': levels}


def parse_lifo(data):
    links, ids, r = rcol_header(data)
    name, bid, ver = block_head(r)
    assert name == 'cLevelInfo', name
    fname = sgresource(r)
    w = r.u32(); h = r.u32(); z = r.u32(); n = r.u32()
    return {'name': fname, 'w': w, 'h': h, 'data': r.raw(n)}
