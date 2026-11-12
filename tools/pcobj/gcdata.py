"""Read the GameCube game's data (the user's own disc image or extracted folder) for the PC object
converter: the resource archives (DATA/index.ind + *.arc, as tools/resdump.py), the "SimsObjects"
quickdata (object definitions) and the MODL/SHDR/TXFL resource headers.

Only reads; the disc image or folder is never written.
"""
import os, struct

def _crc_table():
    t = []
    for i in range(256):
        c = i
        for _ in range(8): c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
        t.append(c)
    return t
_T = _crc_table()

def symbol_id(name):
    """EChecksum::ComputeSymbol (see tools/resdump.py)."""
    crc = (_T[0xA0] ^ 0xFFFFFF) if name[:1].isdigit() else 0xFFFFFFFF
    for ch in name.encode():
        if 97 <= ch <= 122: ch -= 32
        elif not (65 <= ch <= 90 or 48 <= ch <= 57): ch = 95
        crc = (crc >> 8) ^ _T[(crc ^ ch) & 0xFF]
    return ~crc & 0xFFFFFFFF


class _Folder:
    def __init__(self, root): self.root = root
    def path(self, rel):
        cur = self.root
        for part in rel.split('/'):
            hit = [e for e in os.listdir(cur) if e.lower() == part.lower()]
            if not hit: raise FileNotFoundError(rel)
            cur = os.path.join(cur, hit[0])
        return cur
    def read(self, rel, off=0, size=None):
        with open(self.path(rel), 'rb') as f:
            f.seek(off); return f.read() if size is None else f.read(size)


class _Gcm:
    """A plain GameCube disc image (GCM/ISO): the FST is read once."""
    def __init__(self, path):
        self.f = open(path, 'rb')
        self.f.seek(0x424); fst_off, fst_size = struct.unpack('>II', self.f.read(8))
        self.f.seek(fst_off); fst = self.f.read(fst_size)
        n = struct.unpack_from('>I', fst, 8)[0]
        strs = n * 12
        self.files = {}
        def name(i):
            o = strs + (struct.unpack_from('>I', fst, i * 12)[0] & 0xFFFFFF)
            return fst[o:fst.index(b'\0', o)].decode('latin-1')
        def walk(start, end, prefix):
            i = start
            while i < end:
                w0, w1, w2 = struct.unpack_from('>III', fst, i * 12)
                nm = name(i)
                if w0 >> 24:
                    walk(i + 1, w2, prefix + nm + '/'); i = w2
                else:
                    self.files[(prefix + nm).lower()] = (w1, w2); i += 1
        walk(1, n, '')
    def read(self, rel, off=0, size=None):
        o, s = self.files[rel.lower()]
        self.f.seek(o + off)
        return self.f.read(s - off if size is None else size)


class GcData:
    MANAGERS = ('models', 'shaders', 'textures', 'quickdatas')

    def __init__(self, path):
        self.src = _Gcm(path) if os.path.isfile(path) else _Folder(path)
        self.index = {}
        for fn in ('DATA/index.ind', 'DATA/index1.ind'):
            try: b = self.src.read(fn)
            except (FileNotFoundError, KeyError): continue
            n = struct.unpack_from('>I', b, 0)[0]
            offs = struct.unpack_from(f'>{n + 1}I', b, 4)
            for i in range(0, n - 1, 2):
                nm = b[offs[i]:b.index(0, offs[i])].decode().lower()
                blk = offs[i + 1]
                count = struct.unpack_from('>I', b, blk)[0]
                ids = struct.unpack_from(f'>{count}I', b, blk + 4)
                ps = struct.unpack_from(f'>{2 * count}I', b, blk + 4 + 4 * count)
                self.index[nm] = {ids[k]: (ps[2 * k], ps[2 * k + 1]) for k in range(count)}
        if 'models' not in self.index:
            raise SystemExit(f'{path}: no GameCube Sims 2 data (DATA/index.ind) found')

    def has(self, mgr, rid): return rid in self.index.get(mgr, {})

    def get(self, mgr, rid):
        e = self.index.get(mgr, {}).get(rid)
        if not e: return None
        return self.src.read('DATA/' + mgr[:8] + '.arc', e[0], e[1])


def data_header(b):
    """EDataHeader: version, signature, variant, name length, name, data size."""
    ver, sig, var, nl = struct.unpack_from('>4I', b, 0)
    name = b[16:16 + nl].split(b'\0')[0].decode('latin-1')
    size = struct.unpack_from('>I', b, 16 + nl)[0]
    return ver, sig, var, name, 20 + nl, size


def make_header(ver, sig, var, name, data):
    nb = name.encode('latin-1') + b'\0'
    return struct.pack('>4I', ver, sig, var, len(nb)) + nb + struct.pack('>I', len(data)) + data


class Quickdata:
    """An XImage as stored (big-endian, offsets relative to the image; fixup set decoded)."""
    def __init__(self, raw):
        sz, doff, dsz, foff, fsz, base = struct.unpack('>6I', raw[:24])
        self.img = raw[doff:doff + dsz]; self.base = base
        fx = raw[foff:foff + fsz]; w = set(); i = 0; n = len(fx) // 4
        while i < n:
            v = struct.unpack_from('>I', fx, 4 * i)[0]; i += 1
            if v & 3 == 0: w.add(v)
            elif v & 3 == 1:
                st = struct.unpack_from('>I', fx, 4 * i)[0]; i += 1
                for k in range(v >> 2): w.add(st + 4 * k)
            else:
                for k in range(v & 3): w.add((v & ~3) + 4 * k)
        self.fix = w
        self.tables = {}
        for t in range(self.u32(8)):
            e = 0x18 + t * 0x14
            self.tables[self.cstr(self.u32(e))] = e
    def u32(self, o): return struct.unpack_from('>I', self.img, o)[0]
    def s16(self, o): return struct.unpack_from('>h', self.img, o)[0]
    def cstr(self, o): return self.img[o:self.img.index(b'\0', o)].decode('latin-1')
    def ptr(self, o): return self.u32(o) - self.base if o in self.fix and self.u32(o) else None
    def table(self, name):
        e = self.tables[name]
        return dict(rows=self.u32(e + 4), names=self.ptr(e + 8), count=self.u32(e + 12), size=self.u32(e + 16) or 1)
    def rowname(self, t, i):
        return self.cstr(self.u32(t['names'] + 4 * i)) if t['names'] is not None else None


class GcObjects:
    """The object definitions ("SimsObjects" quickdata 0x0C33DB41): per object file its definitions."""
    def __init__(self, gc):
        self.qd = q = Quickdata(gc.get('quickdatas', 0x0C33DB41))
        rf = q.table('ResFile')
        self.files = {}
        for i in range(rf['count']):
            row = rf['rows'] + i * rf['size']
            defs = q.ptr(row); n = q.u32(defs - 4) if defs is not None else 0
            out = []
            for j in range(n):
                d = q.ptr(defs + 4 * j)
                if d is None: continue
                rd = q.ptr(d + 0xC0)
                anim = q.ptr(rd) if rd is not None else None
                na = q.u32(anim - 4) if anim is not None else 0
                out.append(dict(guid=q.u32(d + 0x1C), type=q.s16(d + 0x12), master=q.s16(d + 0x14), sub=q.s16(d + 0x16),
                                price=q.s16(d + 0x24), functions=q.s16(d + 0x50) & 0xFFFF,
                                model=q.u32(anim) if na else 0, thumbnail=q.u32(rd + 0x1C) if rd is not None else 0,
                                # the state table (ObjAnimDef: model, effects, animation, shader replaced,
                                # its replacement, ...) and the animated character
                                states=[tuple(q.u32(anim + 0x20 * k + 4 * w) for w in range(8)) for k in range(na)],
                                character=q.u32(rd + 0x20) if rd is not None else 0))
            self.files[rf_name(q, rf, i)] = out
        self.guids = {d['guid'] for defs in self.files.values() for d in defs}


def rf_name(q, rf, i):
    return q.rowname(rf, i)


def modl_lights(b):
    """A MODL's SimsLightInfo records (0x1C bytes each, as stored: type, node, colour, intensity, falloff)."""
    ver, sig, var, name, off, size = data_header(b)
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
    n = be(p); p += 4
    return [bytes(d[p + 0x1C * k:p + 0x1C * (k + 1)]) for k in range(n)]


def parse_modl_shaders(b):
    """The shader ids a MODL uses, in order (a light parse: header fields up to the first sub-model)."""
    ver, sig, var, name, off, size = data_header(b)
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
    nsub = be(p + 5); p += 9
    out = []
    # first sub-model's shaders only (enough to pick a material to copy)
    p += 4; ns = be(p); p += 4
    for _ in range(min(ns, 1)):
        out.append(be(p + 4))
    return out


def modl_strips(b):
    """A MODL's strips: [{'sub', 'node', 'shader', 'flags', 'P' (model units), 'U'}] (positions in the
    sub-model's node space; uvs or None). Used for the GameCube screens a converted TV / computer copies."""
    import numpy as np
    ver, sig, var, name, off, size = data_header(b)
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
    scale = struct.unpack_from('>f', d, p + 1)[0]
    nsub = be(p + 5); p += 9
    out = []
    for si in range(nsub):
        node = be(p); ns = be(p + 4); p += 8
        for _ in range(ns):
            flags = be(p); sh = be(p + 4); nstr = be(p + 8); p += 12 + nstr + 4
            while True:
                tag = d[p]; p += 1
                if tag == 0:
                    vc = be(p); p += 4
                    fixed = flags & 0x10
                    if fixed:
                        P = np.frombuffer(d[p:p + vc * 8], '>i2').reshape(-1, 4)[:, :3] * scale
                    else:
                        P = np.frombuffer(d[p:p + vc * 16], '>f4').reshape(-1, 4)[:, :3].astype(np.float64)
                    p += vc * (8 if fixed else 16)
                    U = None
                    if flags & 2:
                        if fixed:
                            U = np.frombuffer(d[p:p + vc * 4], '>i2').reshape(-1, 2) / 4096.0
                        else:
                            U = np.frombuffer(d[p:p + vc * 8], '>f4').reshape(-1, 2).astype(np.float64)
                        p += vc * (4 if flags & 0x40 else 2) * (2 if fixed else 4)
                    if flags & 4: p += vc * 4
                    if flags & 8: p += vc * (4 if ver >= 0x3A else 3)
                    if flags & 0x20:
                        dls = be(p + 5); p += 13 + dls
                    if vc:
                        out.append({'sub': si, 'node': node, 'shader': sh, 'flags': flags, 'P': P, 'U': U})
                elif tag == 1:
                    p += 3
                elif tag == 6:
                    break
    return out
