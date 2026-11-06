#!/usr/bin/env python3
"""Print a class's generated in-class declarations (for starting a hand-written header).
Ported methods are marked; unported ones must keep this exact declaration so their stubs compile.
usage: class_decls.py CLASS"""
import os, sys
port = os.path.join(os.path.dirname(__file__), '..')
cls = sys.argv[1]
seen = set()
for line in open(os.path.join(port, 'gen/functions.tsv')):
    addr, unit, status, outdecl, c, indecl = line.rstrip('\n').split('\t')
    if c == cls and indecl not in seen:
        seen.add(indecl)
        print(f'  {indecl};  // {addr}' + ('  [ported]' if status == 'ported' else ''))
