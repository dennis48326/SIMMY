#!/usr/bin/env python3
"""Name the virtual method behind a raw-decompiler vtable call.
Raw output reads `(**(code **)(*(int *)(obj + VPTR) + OFF))(...)`; OFF is the pfn offset.
usage: vslot.py CLASS [OFF ...]      (no OFF: list the whole vtable)"""
import os, sys
data = os.path.join(os.path.dirname(__file__), '..', 'data')
names = {}
for l in open(os.path.join(data, 'symbols.tsv')):
    a, k, sz, ns, nm, dem = l.rstrip('\n').split('\t')
    names.setdefault(a, dem)
cls = sys.argv[1]
for l in open(os.path.join(data, 'vtables.tsv')):
    c, slots = l.rstrip('\n').split('\t')
    if c == cls:
        slots = slots.split(',')
        offs = [int(x, 16) for x in sys.argv[2:]] or [0xC + 8 * i for i in range(len(slots))]
        for o in offs:
            i = (o - 0xC) // 8
            a = slots[i] if 0 <= i < len(slots) and (o - 0xC) % 8 == 0 else None
            print(f'+{o:#05x}  slot {i:2}  {names.get(a, "(pure/none)") if a else "?"}')
        break
else:
    print('no vtable for', cls)
