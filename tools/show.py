#!/usr/bin/env python3
"""Print raw Ghidra output for functions by address or (substring of) demangled name.
usage: show.py <raw dir> ADDR|NAME [...]"""
import os, re, sys
raw = sys.argv[1]
blocks = {}
for root, _, files in os.walk(raw):
    for f in files:
        text = open(os.path.join(root, f), encoding='utf-8', errors='replace').read()
        parts = re.split(r'(?m)^(?=// [0-9A-F]{8}  size )', text)
        for p in parts[1:]:
            m = re.match(r'// ([0-9A-F]{8})  size \S+  (.*)', p)
            blocks[m.group(1)] = (m.group(2), os.path.relpath(os.path.join(root, f), raw), p.rstrip())
for q in sys.argv[2:]:
    hits = [k for k, v in blocks.items() if k == q.upper()] or [k for k, v in blocks.items() if q == v[0]] or \
           [k for k, v in blocks.items() if q in v[0]]
    for k in sorted(hits)[:20]:
        print(f'==== {blocks[k][1]}'); print(blocks[k][2]); print()
