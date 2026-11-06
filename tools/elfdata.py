#!/usr/bin/env python3
"""Dump initialised data from the original ELF, by symbol name or address.

usage: elfdata.py <u2_ngc_release_dvd.elf> <symbol|0xADDR> [size] [--u32|--u16|--str]
Symbols come from data/symbols.tsv. Prints big-endian words (default --u32)."""
import os, struct, sys

def load_segments(path):
    b = open(path, 'rb').read()
    phoff, = struct.unpack_from('>I', b, 0x1C)
    phentsize, phnum = struct.unpack_from('>HH', b, 0x2A)
    segs = []
    for i in range(phnum):
        typ, off, vaddr, _, filesz, memsz = struct.unpack_from('>6I', b, phoff + i * phentsize)
        if typ == 1: segs.append((vaddr, off, filesz, memsz))
    return b, segs

def read(b, segs, addr, size):
    for vaddr, off, filesz, memsz in segs:
        if vaddr <= addr < vaddr + memsz:
            rel = addr - vaddr
            data = b[off + rel: off + min(rel + size, filesz)]
            return data + b'\0' * (size - len(data))  # .bss reads as zeros
    raise SystemExit(f'{addr:08X} is not in a loaded segment')

def main():
    elf, key = sys.argv[1], sys.argv[2]
    mode = next((a for a in sys.argv[3:] if a.startswith('--')), '--u32')
    rest = [a for a in sys.argv[3:] if not a.startswith('--')]
    here = os.path.dirname(os.path.abspath(__file__))
    size = int(rest[0], 0) if rest else None
    if key.lower().startswith('0x'):
        addr = int(key, 16)
    else:
        for line in open(os.path.join(here, '..', 'data', 'symbols.tsv')):
            f = line.rstrip('\n').split('\t')
            if len(f) > 4 and (f[4] == key or (f[3] + '::' + f[4]) == key):
                addr = int(f[0], 16); size = size or int(f[2], 16); break
        else: raise SystemExit(f'no symbol {key}')
    b, segs = load_segments(elf)
    data = read(b, segs, addr, size or 4)
    if mode == '--str':
        print(data.split(b'\0')[0].decode('latin-1')); return
    w = 4 if mode == '--u32' else 2
    fmt = '>I' if w == 4 else '>H'
    vals = [struct.unpack_from(fmt, data, i)[0] for i in range(0, len(data) - w + 1, w)]
    for i in range(0, len(vals), 8):
        print(f'{addr + i * w:08X}: ' + ' '.join(f'{v:0{2*w}X}' for v in vals[i:i + 8]))

main()
