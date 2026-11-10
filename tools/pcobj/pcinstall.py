"""The user's The Sims 2 (PC) install: its packs (base game, expansion and stuff packs), their packages,
and the buy-mode objects they define (with the packs' override order).

Reads only; nothing is written into the install. See docs/mods.md.

Packs: every TSData/Res folder under the install folder. The base game keeps its scenegraph in
Res/Sims3D, the packs in Res/3D; stuff packs from Celebration! on keep their own objects in
Res/StuffPack/Objects. Each expansion re-ships (and may change) earlier packs' objects in its own
objects.package: a GUID's newest definition wins (the packs' release order, below), and the object's
"home" pack is the oldest pack that has it.
"""
import glob, os, re, struct

from dbpf import Package, fname64, strings_of_str

T_OBJD, T_STR, T_CTSS, T_BHAV, T_MMAT, T_GLOB = 0x4F424A44, 0x53545223, 0x43545353, 0x42484156, 0x4C697E5A, 0x474C4F42

# Release order (later packs override earlier ones): (match in the folder name, short code, name).
PACKS = [
    (None, 'base', 'The Sims 2'),
    ('university', 'ep1', 'University'),
    ('nightlife', 'ep2', 'Nightlife'),
    ('business', 'ep3', 'Open for Business'),
    ('family fun', 'sp1', 'Family Fun Stuff'),
    ('glamour', 'sp2', 'Glamour Life Stuff'),
    ('pets', 'ep4', 'Pets'),
    ('holiday', 'sp3', 'Happy Holiday Stuff'),
    ('celebration', 'sp4', 'Celebration! Stuff'),
    ('fashion', 'sp5', 'H&M Fashion Stuff'),
    ('seasons', 'ep5', 'Seasons'),
    ('teen style', 'sp6', 'Teen Style Stuff'),
    ('bon voyage', 'ep6', 'Bon Voyage'),
    ('freetime', 'ep7', 'FreeTime'),
    ('free time', 'ep7', 'FreeTime'),
    ('kitchen', 'sp7', 'Kitchen & Bath Interior Design Stuff'),
    ('ikea', 'sp8', 'IKEA Home Stuff'),
    ('apartment', 'ep8', 'Apartment Life'),
    ('mansion', 'sp9', 'Mansion & Garden Stuff'),
]


class Pack:
    def __init__(self, res, order, code, name):
        self.res, self.order, self.code, self.name = res, order, code, name

    def object_packages(self):
        out = sorted(glob.glob(os.path.join(self.res, 'Objects', '*.package')))
        out += sorted(glob.glob(os.path.join(self.res, 'StuffPack', 'Objects', '*.package')))
        return out

    def material_packages(self):
        """Res/Catalog/Materials: the objects' material overrides (MMAT: recolours and defaults)."""
        return sorted(glob.glob(os.path.join(self.res, 'Catalog', 'Materials', '*.package')))

    def scenegraph_packages(self):
        """Res/Sims3D (base game) or Res/3D (packs): the object meshes, materials and textures (the
        Sims?? packages are people's: left out)."""
        out = []
        for sub in ('Sims3D', '3D'):
            for p in sorted(glob.glob(os.path.join(self.res, sub, '*.package'))):
                if not re.match(r'sims\d', os.path.basename(p).lower()):
                    out.append(p)
        return out

    def __repr__(self):
        return f'{self.code} {self.name}'


def _classify(folder):
    """(order, code, name) of a pack by its install folder's name."""
    low = folder.lower()
    for k, (key, code, name) in enumerate(PACKS):
        if key and key in low:
            return k, code, name
    if re.fullmatch(r'(the )?sims ?2', low.strip()) or low.strip() in ('the sims 2', 'sims2'):
        return 0, 'base', 'The Sims 2'
    return len(PACKS), 'x_' + re.sub(r'[^a-z0-9]+', '', low)[-12:], folder


def find_packs(root):
    """The packs under `root` (an install folder, a pack folder or a TSData/Res folder), oldest first."""
    root = os.path.abspath(root)
    hits = []
    if os.path.basename(root).lower() == 'res' and os.path.isdir(os.path.join(root, 'Objects')):
        hits.append(root)
    else:
        for dp, dn, fn in os.walk(root):
            depth = dp.count(os.sep) - root.count(os.sep)
            if os.path.basename(dp).lower() == 'res' and os.path.basename(os.path.dirname(dp)).lower() == 'tsdata':
                hits.append(dp)
                dn[:] = []
            elif depth > 4:
                dn[:] = []
    packs = []
    for res in hits:
        folder = os.path.basename(os.path.dirname(os.path.dirname(res)))
        order, code, name = _classify(folder)
        packs.append(Pack(res, order, code, name))
    packs.sort(key=lambda p: (p.order, p.name))
    return packs


# ---------------------------------------------------------------------------------------------------
# OBJD fields (16-bit words after the 64-byte name; the GameCube's ObjDefinition rows keep the same
# word positions: byte offset = 2 x word).

W_TYPE, W_MASTER, W_SUB, W_GUID, W_PRICE, W_ROOMSORT, W_FUNCSORT = 0x09, 0x0A, 0x0B, 0x0E, 0x12, 0x27, 0x28
W_CATSTR = 0x29          # catalogue strings (CTSS) instance
W_RATINGS = 0x52         # hunger, comfort, hygiene, bladder, energy, fun, room (0x52..0x58)
W_BUILDTYPE = 0x45       # build mode type (doors, windows, ...): not buy mode
W_SUBSORT = 0x5E         # function sub-sort (decorative: 1 wall hangings, 2 sculptures, 4 rugs, 8 plants,
                         # 0x10 mirrors, 0x20 curtains, 0x40 ?, 0x80 miscellaneous)
SUBSORTS = {1: 'wallhanging', 2: 'sculpture', 4: 'rug', 8: 'plant', 0x10: 'mirror', 0x20: 'curtain',
            0x40: 'window', 0x80: 'misc'}
F_DECORATIVE = 0x20


class Objd:
    __slots__ = ('pkg', 'entry', 'name', 'words', 'guid', 'group', 'pack')

    def __init__(self, pkg, entry, data, pack):
        self.pkg, self.entry, self.pack = pkg, entry, pack
        self.name = fname64(data)
        n = (len(data) - 0x40) // 2
        self.words = list(struct.unpack_from(f'<{n}H', data, 0x40))
        self.guid = struct.unpack_from('<I', data, 0x5C)[0]
        self.group = entry.group

    def w(self, k):
        return self.words[k] if k < len(self.words) else 0

    def s(self, k):
        v = self.w(k)
        return v - 0x10000 if v & 0x8000 else v

    @property
    def master(self): return self.s(W_MASTER)

    @property
    def sub(self): return self.w(W_SUB)

    def is_lead(self):
        return self.master == 0 or self.sub == 0xFFFF


class Catalog:
    """Every OBJD of every pack, by GUID (newest definition wins; the oldest pack is the home pack)."""

    def __init__(self, packs, log=print):
        self.packs = packs
        self.pkgs = {}
        self.by_guid = {}   # guid -> [Objd, ...] oldest pack first
        self.mmat = {}      # object guid -> {material name: MMAT properties}
        for pack in packs:
            for path in pack.object_packages():
                pk = self.pkgs[path] = Package(path)
                for e in pk.of_type(T_OBJD):
                    d = pk.read(e)
                    if len(d) < 0x40 + 2 * 0x60:
                        continue
                    o = Objd(pk, e, d, pack)
                    self.by_guid.setdefault(o.guid, []).append(o)
            for path in pack.material_packages() + pack.object_packages():
                pk = self.pkgs.get(path) or Package(path)
                for e in pk.of_type(T_MMAT):
                    try:
                        props = parse_cpf(pk.read(e))
                    except Exception:
                        continue
                    g = props.get('objectGUID')
                    if isinstance(g, int):
                        # newest pack last: a later MMAT of the same name replaces an earlier one
                        lst = self.mmat.setdefault(g & 0xFFFFFFFF, {})
                        lst[str(props.get('name', '')).lower()] = props
            log(f'  {pack.code:5s} {pack.name}: {sum(len(self.pkgs[p].entries) for p in pack.object_packages())} '
                f'resources in {len(pack.object_packages())} object package(s)')

    def recolours(self, guid):
        """The object's recolour families (MMAT 'family'), each {subset: material name}; the default
        material's family first."""
        fams = {}
        default = None
        for props in self.mmat.get(guid, {}).values():
            if props.get('objectStateIndex', -1) not in (-1, 0):
                continue
            fam = str(props.get('family', ''))
            fams.setdefault(fam, {})[str(props.get('subsetName', '')).lower()] = str(props.get('name', ''))
            if props.get('defaultMaterial'):
                default = fam
        order = sorted(fams, key=lambda f: (f != default, f))
        return [(f, fams[f]) for f in order], default

    def newest(self, guid):
        return self.by_guid[guid][-1]

    def home(self, guid):
        return self.by_guid[guid][0].pack

    def group_entries(self, o, types):
        return [e for e in o.pkg.entries if e.group == o.group and e.type in types]

    def strings(self, o, t, inst, lang=1):
        for e in o.pkg.entries:
            if e.group == o.group and e.type == t and e.inst == inst:
                vals = strings_of_str(o.pkg.read(e))
                own = [v for (l, v, _) in vals if l == lang]
                return own if own else [v for (l, v, _) in vals if l == 1]
        return []

    def catalog_text(self, o):
        """(name, description) from the object's CTSS (English)."""
        inst = o.w(W_CATSTR)
        ct = [e for e in o.pkg.entries if e.group == o.group and e.type == T_CTSS]
        if not ct:
            return None, ''
        e = next((e for e in ct if e.inst == inst), ct[0])
        vals = strings_of_str(o.pkg.read(e))
        en = [v for (l, v, _) in vals if l == 1]
        if not en:
            return None, ''
        return en[0].strip(), (en[1].strip() if len(en) > 1 else '')

    def models(self, o):
        """The object's model (CRES) names: STR# 0x85."""
        return [v.strip() for v in self.strings(o, T_STR, 0x85) if v.strip()]

    def parts(self, lead):
        """The lead's multi-tile parts (the same group's OBJDs with its master id), newest versions."""
        if lead.master == 0:
            return []
        out = []
        for e in lead.pkg.entries:
            if e.type != T_OBJD or e.group != lead.group:
                continue
            d = lead.pkg.read(e)
            o = Objd(lead.pkg, e, d, lead.pack)
            if o.master == lead.master and o.sub != 0xFFFF:
                out.append(o)
        return out

    def semiglobal(self, o):
        for e in o.pkg.entries:
            if e.group == o.group and e.type == T_GLOB:
                d = o.pkg.read(e)
                return d[64:].split(b'\0')[0][1:].decode('latin-1', 'replace')
        return ''

    def bhav_count(self, o):
        return sum(1 for e in o.pkg.entries if e.group == o.group and e.type == T_BHAV)


def parse_cpf(data):
    """A CPF property set (MMAT, GZPS, ...): {key: value}. Binary form only (XML: parsed loosely)."""
    out = {}
    if data[:4] == b'\xe0\x50\xe7\xcb':
        p = 6
        n = struct.unpack_from('<I', data, p)[0]; p += 4
        for _ in range(n):
            t = struct.unpack_from('<I', data, p)[0]; p += 4
            ln = struct.unpack_from('<I', data, p)[0]; p += 4
            k = data[p:p + ln].decode('latin-1'); p += ln
            if t == 0xEB61E4F7:      # uint
                v = struct.unpack_from('<I', data, p)[0]; p += 4
            elif t == 0x0C264712:    # int
                v = struct.unpack_from('<i', data, p)[0]; p += 4
            elif t == 0xABC78708:    # float
                v = struct.unpack_from('<f', data, p)[0]; p += 4
            elif t == 0xCBA908E1:    # bool
                v = bool(data[p]); p += 1
            else:                    # string
                ln = struct.unpack_from('<I', data, p)[0]; p += 4
                v = data[p:p + ln].decode('latin-1'); p += ln
            out[k] = v
    else:
        txt = data.decode('latin-1', 'replace')
        for m in re.finditer(r'<Any(\w+) key="([^"]+)"[^>]*>([^<]*)<', txt):
            kind, k, v = m.groups()
            if kind.lower() in ('uint32', 'int32'):
                try:
                    v = int(v, 0)
                except ValueError:
                    pass
            elif kind.lower() == 'boolean':
                v = v.strip().lower() == 'true'
            out[k] = v
    return out
