"""Find Sims 2 (PC) scenegraph resources by name across an install's packages.

Instance = CRC-24 (poly 0x864CFB, init 0xB704CE) of the lower-cased name | 0xFF000000, instance-high
= CRC-32 (MSB-first, poly 0x04C11DB7, init 0xFFFFFFFF, no final xor) of it; group 0x1C0532FA for the
base game's global scenegraph resources.
"""
import os, glob
from dbpf import Package
import rcol

SG_GROUP = 0x1C0532FA


def crc32_msb(s, poly=0x04C11DB7, init=0xFFFFFFFF):
    c = init
    for ch in s:
        c ^= ch << 24
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFFFFFFFF if c & 0x80000000 else (c << 1) & 0xFFFFFFFF
    return c


def crc24(s, poly=0x864CFB, init=0xB704CE):
    c = init
    for ch in s:
        c ^= ch << 16
        for _ in range(8):
            c <<= 1
            if c & 0x1000000: c ^= poly
    return c & 0xFFFFFF


def name_tgi(name):
    b = name.lower().encode('latin-1')
    return crc24(b) | 0xFF000000, crc32_msb(b)


SUFFIX = {rcol.T_CRES: '_cres', rcol.T_SHPE: '_shpe', rcol.T_GMND: '_gmnd', rcol.T_GMDC: '_gmdc',
          rcol.T_TXMT: '_txmt', rcol.T_TXTR: '_txtr', rcol.T_LIFO: '_lifo'}


class Install:
    """Scenegraph packages, opened lazily and searched in order (put the newest pack's first: a
    resource a later pack re-ships wins). `paths`: package files, or TSData/Res folders (their
    Sims3D, 3D, Catalog/Bins and Objects packages)."""
    def __init__(self, paths):
        self.paths = []
        for r in paths:
            if os.path.isdir(r):
                for sub in ('Sims3D', '3D', os.path.join('Catalog', 'Bins'), 'Objects'):
                    self.paths += sorted(glob.glob(os.path.join(r, sub, '*.package')))
            else:
                self.paths.append(r)
        self._pk = {}

    def pk(self, path):
        if path not in self._pk:
            self._pk[path] = Package(path)
        return self._pk[path]

    def find_tgi(self, t, inst, insthi, group=None):
        for p in self.paths:
            pk = self.pk(p)
            if not hasattr(pk, 'by_tii'):
                pk.by_tii = {}
                for e in pk.entries:
                    pk.by_tii.setdefault((e.type, e.inst, e.insthi), e)
            e = pk.by_tii.get((t, inst, insthi))
            if e: return pk, e
        return None, None

    def find(self, t, name):
        n = name if name.lower().endswith(SUFFIX.get(t, '')) else name + SUFFIX.get(t, '')
        if n.startswith('##0x'):  # "##0x1c050000!name": a group-qualified name
            n = n.split('!', 1)[1]
        inst, ih = name_tgi(n)
        pk, e = self.find_tgi(t, inst, ih)
        return (pk.read(e) if e else None), e
