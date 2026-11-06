#!/usr/bin/env python3
"""Extract or hex-dump resources from the game's DATA/*.ARC archives.

usage:
  resdump.py <data root> list <manager>                 ids, offsets and sizes in one manager
  resdump.py <data root> dump <manager> <name|0xID> [n]  hex dump the first n bytes (default 256)
  resdump.py <data root> save <manager> <name|0xID> <out file>

<data root> is the folder holding DATA/ (e.g. Q:\\Claude\\SIMMY). Names are hashed the way the game
does it (EChecksum::ComputeSymbol)."""
import os, struct, sys

def _table():
    t = []
    for i in range(256):
        c = i
        for _ in range(8): c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
        t.append(c)
    return t
_T = _table()

def symbol_id(name):
    """EChecksum::ComputeSymbol: CRC-32 of the upper-cased name, non-alphanumerics folded to '_';
    names starting with a digit start from a different value."""
    crc = (_T[0xA0] ^ 0xFFFFFF) if name[:1].isdigit() else 0xFFFFFFFF
    for ch in name.encode():
        if 97 <= ch <= 122: ch -= 32
        elif not (65 <= ch <= 90 or 48 <= ch <= 57): ch = 95
        crc = (crc >> 8) ^ _T[(crc ^ ch) & 0xFF]
    return ~crc & 0xFFFFFFFF

def find(root, rel):
    cur = root
    for part in rel.split('/'):
        hit = [e for e in os.listdir(cur) if e.lower() == part.lower()]
        if not hit: raise SystemExit(f'not found: {rel}')
        cur = os.path.join(cur, hit[0])
    return cur

def index_blocks(root):
    out = {}
    for fn in ('DATA/INDEX.IND', 'DATA/INDEX1.IND'):
        try: b = open(find(root, fn), 'rb').read()
        except SystemExit: continue
        n = struct.unpack_from('>I', b, 0)[0]
        offs = struct.unpack_from(f'>{n+1}I', b, 4)
        for i in range(0, n - 1, 2):
            name = b[offs[i]:b.index(0, offs[i])].decode()
            out[name.lower()] = (b, offs[i + 1])
    return out

def entries(b, off):
    count = struct.unpack_from('>I', b, off)[0]
    ids = struct.unpack_from(f'>{count}I', b, off + 4)
    ps = struct.unpack_from(f'>{2*count}I', b, off + 4 + 4 * count)
    return [(ids[i], ps[2 * i], ps[2 * i + 1]) for i in range(count)]

def main():
    root, cmd, mgr = sys.argv[1], sys.argv[2], sys.argv[3]
    blocks = index_blocks(root)
    if mgr.lower() not in blocks: raise SystemExit('managers: ' + ', '.join(sorted(blocks)))
    ents = entries(*blocks[mgr.lower()])
    if cmd == 'list':
        for i, p, s in ents: print(f'{i:08X} {p:10d} {s:9d}')
        return
    key = sys.argv[4]
    rid = int(key, 16) if key.lower().startswith('0x') else symbol_id(key)
    hit = [e for e in ents if e[0] == rid]
    if not hit: raise SystemExit(f'{key} ({rid:08X}) not in {mgr}')
    _, pos, size = hit[0]
    arc = find(root, 'DATA/' + mgr[:8].upper() + '.ARC')
    with open(arc, 'rb') as f:
        f.seek(pos); data = f.read(size)
    if cmd == 'save':
        open(sys.argv[5], 'wb').write(data); print(f'{rid:08X}: {size} bytes -> {sys.argv[5]}')
    else:
        n = int(sys.argv[5]) if len(sys.argv) > 5 else 256
        print(f'{rid:08X} @ {pos}, {size} bytes')
        for o in range(0, min(n, size), 16):
            row = data[o:o + 16]
            print(f'{o:06X}  ' + ' '.join(f'{c:02X}' for c in row).ljust(48) + '  ' +
                  ''.join(chr(c) if 32 <= c < 127 else '.' for c in row))

main()
