#!/usr/bin/env python3
"""Porting progress by area, from gen/functions.tsv (run the generator or a build first)."""
import collections, os, sys
port = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..')
tot, done = collections.Counter(), collections.Counter()
for line in open(os.path.join(port, 'gen/functions.tsv')):
    addr, unit, status, _ = line.rstrip('\n').split('\t', 3)
    area = unit.split('/')[0]
    tot[area] += 1
    done[area] += status == 'ported'
for area in sorted(tot, key=lambda a: -tot[a]):
    print(f'{area:10} {done[area]:6} / {tot[area]:6}  {100 * done[area] / tot[area]:5.1f}%')
print(f'{"total":10} {sum(done.values()):6} / {sum(tot.values()):6}  {100 * sum(done.values()) / sum(tot.values()):5.1f}%')
