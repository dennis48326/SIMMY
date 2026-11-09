#!/usr/bin/env python3
# usage: [DEPTH=n] mta.py <prefix> [lo hi] [top]
# Reads the <prefix>.<k> dumps of mtrack.so (MTRACK_OUT=<prefix>). Prints the live heap at each dump, then,
# from the last dump, the backtraces whose allocations made in epochs lo..hi (default 2 .. last-2) are still
# live, largest first, with their bytes per epoch, symbolized with addr2line (DEPTH frames, default 8).
import sys, glob, subprocess, collections, re, os

pre = sys.argv[1]
files = sorted(glob.glob(pre + '.[0-9]*'), key=lambda f: int(f.rsplit('.', 1)[1]))
print('dump  epoch  live_allocs  live_MB   uord_MB  arena_MB  survivors(epoch<k-1)_MB')
for f in files:
    tot = 0; old = 0; info = ''
    ep = 0
    for l in open(f):
        if l.startswith('I '):
            p = l.split(); d = dict(zip(p[1::2], p[2::2])); ep = int(d['epoch']); info = d
        elif l.startswith('E '):
            _, e, c, b = l.split(); tot += int(b)
            if int(e) < ep - 1: old += int(b)
    print('%4s %6d %12s %8.2f %9.2f %9.2f %10.2f' % (f.rsplit('.', 1)[1], ep, info['live'], tot / 1e6,
          int(info['uordblks']) / 1e6, int(info['arena']) / 1e6, old / 1e6))

last = files[-1]
A = collections.defaultdict(dict); S = {}; M = []; ep = 0
for l in open(last):
    t = l.split()
    if t[0] == 'I': ep = int(t[2])
    elif t[0] == 'A': A[int(t[1])][int(t[2])] = (int(t[3]), int(t[4]))
    elif t[0] == 'S': S[int(t[1])] = [int(x, 16) for x in t[2:]]
    elif t[0] == 'M':
        m = re.match(r'M ([0-9a-f]+)-([0-9a-f]+) \S+ ([0-9a-f]+) \S+ \S+\s+(.*)', l)
        if m: M.append((int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16), m.group(4)))
lo = int(sys.argv[2]) if len(sys.argv) > 2 else 2
hi = int(sys.argv[3]) if len(sys.argv) > 3 else ep - 2
top = int(sys.argv[4]) if len(sys.argv) > 4 else 25
print('last dump epoch', ep, 'window', lo, hi)
rows = []
for s, d in A.items():
    b = sum(d.get(e, (0, 0))[1] for e in range(lo, hi + 1))
    c = sum(d.get(e, (0, 0))[0] for e in range(lo, hi + 1))
    if b: rows.append((b, c, s))
rows.sort(reverse=True)
tot = sum(r[0] for r in rows)
print('survivors in window: %.3f MB (%.3f MB per epoch)' % (tot / 1e6, tot / 1e6 / max(1, hi - lo + 1)))

cache = {}
def sym(a):
    for lo_, hi_, off, path in M:
        if lo_ <= a < hi_:
            rel = a - lo_ + off
            key = (path, rel)
            if key not in cache:
                r = subprocess.run(['addr2line', '-Cfipe', path, hex(rel - 1)], capture_output=True, text=True).stdout.strip()
                r = re.sub(r'\(inlined by\)', '<-', r)
                r = re.sub(r' at /[^ ]*/', ' @', r)
                cache[key] = (os.path.basename(path) + ': ' + r.replace('\n', ' '))[:260]
            return cache[key]
    return hex(a)
skip = ('libstdc++', 'operator new', 'libc.so', 'mtrack')
for b, c, s in rows[:top]:
    per = ' '.join('%d' % (A[s].get(e, (0, 0))[1]) for e in range(lo, hi + 1))
    print('\n%.1f kB  %d allocs  stack %d  per-epoch bytes: %s' % (b / 1e3, c, s, per))
    n = 0
    for a in S.get(s, []):
        t = sym(a)
        if any(k in t for k in skip) and n == 0: continue
        print('    ' + t); n += 1
        if n >= int(os.environ.get('DEPTH', 8)): break
