#!/usr/bin/env python3
"""Minimal reader for The Sims 2 (PC) DBPF packages.

Supports package versions 1.0/1.1/2.x, index 7.0/7.1/7.2 (with the instance-high word), the
compression directory (DIR, type 0xE86B1EEF) and QFS/RefPack decompression. Reads only what is asked
for: a package is opened, its index parsed, and resources are read (and decompressed) on demand.

Nothing is written; the package stays in the user's install.
"""
import struct, sys, os

DIR_TYPE = 0xE86B1EEF

TYPE_NAMES = {
    0x4F424A44: 'OBJD', 0x42484156: 'BHAV', 0x54544142: 'TTAB', 0x53545223: 'STR#', 0x43545353: 'CTSS',
    0x4F424A66: 'OBJf', 0x534C4F54: 'SLOT', 0x474C4F42: 'GLOB', 0x42434F4E: 'BCON', 0x54544173: 'TTAs',
    0x54505250: 'TPRP', 0x5452434E: 'TRCN', 0x4E524546: 'NREF', 0x50414C54: 'PALT', 0x53505232: 'SPR2',
    0x44475250: 'DGRP', 0x46574156: 'FWAV', 0x46434E53: 'FCNS', 0x54524545: 'TREE', 0x50444154: 'PDAT',
    0x43415453: 'CATS', 0x434D4D54: 'CMMT', 0x504F5349: 'POSI', 0x46414D49: 'FAMI', 0x48545320: 'HTS ',
    0xE519C933: 'CRES', 0xFC6EB1F7: 'SHPE', 0x7BA3838C: 'GMND', 0xAC4F8687: 'GMDC', 0x49596978: 'TXMT',
    0x1C4A276C: 'TXTR', 0xFB00791E: 'ANIM', 0xED534136: 'LIFO', 0x4C697E5A: 'MMAT', 0xCCCEF852: 'LxNR',
    0xC9C81B9B: 'LGHT(amb)', 0xC9C81BA3: 'LGHT(dir)', 0xC9C81BA9: 'LGHT(pt)', 0xC9C81BAD: 'LGHT(spot)',
    0x6B943B43: 'XOBJ?', 0xCCA8E925: 'XOBJ', 0x2C1FD8A1: 'XFLR?', 0x4C158081: 'XFNC?', 0x0C1FE246: 'XMOL',
    0xEBCF3E27: 'GZPS', 0xAC598EAC: 'AGED', 0xE86B1EEF: 'DIR', 0x856DDBAC: 'IMG/jpg', 0x8C3CE95A: 'JPG',
    0x0C560F39: 'BINX', 0x4D51F042: 'CINE', 0xD1954460: 'PMAP?', 0xAC506764: '3IDR', 0x6C589723: 'TATT?',
    0x2026960B: 'MP3', 0x8DB5E4C2: 'WGRA?', 0x0BF999E7: 'OBJT?', 0xCDB8BE6B: 'XHTN?', 0x6F626A74: 'objt',
    0x49FF7D76: 'BNRF?', 0x2A51171B: 'XROF?', 0x4DCADB7E: 'XWNT?', 0xABD0DC63: 'XNGB', 0x9D796DB4: 'TSSG',
    0x8C93BF6C: 'XFNC?', 0xACE46235: 'XSTN?', 0xB21BE28B: 'COLL', 0x0A284D0B: 'XFLR', 0x4C158081: 'XWNT',
    0x8A84D7B0: 'WRLD?', 0x04AF2CF4: 'ANIM?', 0xCB4387A1: 'XOBJ(collection?)',
}

def tname(t):
    return TYPE_NAMES.get(t, f'{t:08X}')

def qfs_decompress(src, pos=0):
    """RefPack: [u32 compressed size][u16 0x10FB][u24 BE uncompressed size] then the stream."""
    csize = struct.unpack_from('<I', src, pos)[0]
    magic = struct.unpack_from('>H', src, pos + 4)[0]
    if magic & 0xFEFF != 0x10FB:
        raise ValueError(f'not QFS ({magic:04X})')
    p = pos + 6
    if magic & 0x0100:  # (unused in TS2) 4-byte size
        usize = struct.unpack_from('>I', src, p)[0]; p += 4
    else:
        usize = src[p] << 16 | src[p + 1] << 8 | src[p + 2]; p += 3
    end = pos + csize
    out = bytearray(usize)
    o = 0
    while p < end:
        b0 = src[p]
        if b0 < 0x80:
            b1 = src[p + 1]; p += 2
            plain = b0 & 3
            n = ((b0 & 0x1C) >> 2) + 3
            off = ((b0 & 0x60) << 3) + b1 + 1
        elif b0 < 0xC0:
            b1, b2 = src[p + 1], src[p + 2]; p += 3
            plain = (b1 & 0xC0) >> 6
            n = (b0 & 0x3F) + 4
            off = ((b1 & 0x3F) << 8) + b2 + 1
        elif b0 < 0xE0:
            b1, b2, b3 = src[p + 1], src[p + 2], src[p + 3]; p += 4
            plain = b0 & 3
            n = ((b0 & 0x0C) << 6) + b3 + 5
            off = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1
        elif b0 < 0xFC:
            p += 1
            plain = ((b0 & 0x1F) << 2) + 4
            n = 0; off = 0
        else:
            p += 1
            plain = b0 & 3
            n = 0; off = 0
        if plain:
            out[o:o + plain] = src[p:p + plain]; p += plain; o += plain
        if n:
            s = o - off
            if off >= n:
                out[o:o + n] = out[s:s + n]
            else:
                for i in range(n): out[o + i] = out[s + i]
            o += n
        if b0 >= 0xFC:
            break
    return bytes(out)


class Entry:
    __slots__ = ('type', 'group', 'inst', 'insthi', 'offset', 'size', 'usize')
    def __init__(self, t, g, i, ih, off, size):
        self.type, self.group, self.inst, self.insthi, self.offset, self.size = t, g, i, ih, off, size
        self.usize = None  # set when the DIR says it is compressed
    def key(self):
        return (self.type, self.group, self.inst, self.insthi)
    def __repr__(self):
        return f'{tname(self.type)} g{self.group:08X} i{self.insthi:08X}:{self.inst:08X} @{self.offset} {self.size}'


class Package:
    def __init__(self, path):
        self.path = path
        self.f = open(path, 'rb')
        h = self.f.read(96)
        if h[:4] != b'DBPF':
            raise ValueError(f'{path}: not a DBPF package')
        self.major, self.minor = struct.unpack_from('<II', h, 4)
        self.index_major, count, ioff, isize = struct.unpack_from('<IIII', h, 0x20)
        self.index_minor = struct.unpack_from('<I', h, 0x3C)[0] if (self.major, self.minor) >= (1, 1) else 0
        self.f.seek(ioff)
        raw = self.f.read(isize)
        esz = 24 if self.index_minor == 2 else 20
        self.entries = []
        for k in range(count):
            if esz == 24:
                t, g, i, ih, off, size = struct.unpack_from('<6I', raw, k * esz)
            else:
                t, g, i, off, size = struct.unpack_from('<5I', raw, k * esz); ih = 0
            self.entries.append(Entry(t, g, i, ih, off, size))
        self.by_key = {e.key(): e for e in self.entries}
        d = [e for e in self.entries if e.type == DIR_TYPE]
        if d:
            data = self.raw(d[0])
            dsz = 20 if self.index_minor == 2 else 16
            for k in range(len(data) // dsz):
                if dsz == 20:
                    t, g, i, ih, us = struct.unpack_from('<5I', data, k * dsz)
                else:
                    t, g, i, us = struct.unpack_from('<4I', data, k * dsz); ih = 0
                e = self.by_key.get((t, g, i, ih))
                if e: e.usize = us

    def raw(self, e):
        self.f.seek(e.offset)
        return self.f.read(e.size)

    def read(self, e):
        data = self.raw(e)
        if e.usize is not None and e.type != DIR_TYPE:
            try:
                return qfs_decompress(data)
            except Exception:
                return data
        return data

    def of_type(self, t):
        return [e for e in self.entries if e.type == t]

    def find(self, t, g=None, i=None):
        return [e for e in self.entries if e.type == t and (g is None or e.group == g) and (i is None or e.inst == i)]


# --- small helpers for the RCOL (Sims3D) and SimAntics formats ---

def fname64(data, pos=0):
    """The 64-byte file name at the start of most object resources (OBJD, BHAV, ...)."""
    return data[pos:pos + 64].split(b'\0', 1)[0].decode('latin-1')


def read_7bit_string(data, p):
    """A .NET-style 7-bit-length-prefixed string (used in RCOL blocks)."""
    n = 0; shift = 0
    while True:
        b = data[p]; p += 1
        n |= (b & 0x7F) << shift
        if not b & 0x80: break
        shift += 7
    return data[p:p + n].decode('latin-1'), p + n


def strings_of_str(data):
    """STR# / CTSS: [64 name][u16 format 0xFFFD][u16 count] then (lang, value\0, desc\0)."""
    fmt, count = struct.unpack_from('<Hh', data, 64)
    p = 68; out = []
    if fmt == 0xFFFD:
        for _ in range(count):
            lang = data[p]; p += 1
            e = data.index(b'\0', p); v = data[p:e].decode('utf-8', 'replace'); p = e + 1
            e = data.index(b'\0', p); d = data[p:e].decode('utf-8', 'replace'); p = e + 1
            out.append((lang, v, d))
    return out


if __name__ == '__main__':
    pk = Package(sys.argv[1])
    from collections import Counter
    c = Counter(e.type for e in pk.entries)
    comp = sum(1 for e in pk.entries if e.usize is not None)
    print(f'{os.path.basename(sys.argv[1])}: DBPF {pk.major}.{pk.minor} index {pk.index_major}.{pk.index_minor}, '
          f'{len(pk.entries)} resources, {comp} compressed')
    for t, n in c.most_common():
        print(f'  {tname(t):10s} {t:08X} {n}')
