#!/usr/bin/env python3
"""Generate the PC-port scaffolding from the map symbols and the raw Ghidra output.

Outputs (under port/gen/):
  include/simmy/types_gen.hpp   every game class / method / free function, declared
  src/stubs/<unit>.cpp          a logging stub for every function not yet ported
  functions.tsv                 addr, unit, kind, declaration (index for tools)

A function counts as ported when its original address appears as a `// XXXXXXXX` comment
in any file under port/src/.  Remove the comment (or the function) and it is stubbed again.
Hand-recovered signatures go in port/overrides.tsv and replace the generated guesses.

usage: gen_port.py <port dir>                       (normal; reads only port/data/)
       gen_port.py <port dir> --extract <simmy root> (refresh port/data/ from Ghidra output + ELF)
"""
import bisect, collections, os, re, struct, sys

PORT = os.path.abspath(sys.argv[1])
GEN = os.path.join(PORT, 'gen')
DATA = os.path.join(PORT, 'data')
EXTRACT = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == '--extract' else None
SKIP_UNITS = ('sdk/', 'sn/', 'linkonce/', 'auto/', 'odemuexi2/')
LIBC_PREFIXES = ('wcs', 'str', 'mem', 'is', 'to')
LIBC_NAMES = {'abs', 'atoi', 'atof', 'printf', 'sprintf', 'snprintf', 'vsprintf', 'malloc', 'free', 'realloc',
              'calloc', 'qsort', 'rand', 'srand', 'exit', 'abort', 'sqrt', 'sin', 'cos', 'main'}

BUILTIN = {'void', 'char', 'short', 'int', 'long', 'float', 'double', 'bool', 'unsigned', 'signed',
           'const', 'volatile', 'wchar_t', '__int64', 'va_list'}
CPP_KEYWORDS = {'new', 'delete', 'this', 'class', 'struct', 'union', 'enum', 'template', 'typename',
                'namespace', 'operator', 'default', 'private', 'public', 'protected', 'virtual',
                'register', 'auto', 'explicit', 'friend', 'inline', 'static', 'extern', 'mutable',
                'typeid', 'typedef', 'using', 'and', 'or', 'not', 'xor', 'bitand', 'bitor', 'compl',
                'goto', 'export', 'catch', 'try', 'throw', 'asm', 'case', 'switch', 'return', 'do',
                'for', 'while', 'if', 'else', 'break', 'continue', 'sizeof', 'true', 'false', 'small',
                'near', 'far', 'interface'}

# --- inputs -------------------------------------------------------------------------------------
if EXTRACT:
    import shutil
    os.makedirs(DATA, exist_ok=True)
    for n in ('symbols.tsv', 'units.tsv'):
        shutil.copy(os.path.join(EXTRACT, 'ghidra', n), os.path.join(DATA, n))
    # Ghidra signature per address: return type + param count (from the raw decompilation)
    with open(os.path.join(DATA, 'ghidra_sigs.tsv'), 'w') as w:
        seen = set()
        for root, _, files in sorted(os.walk(os.path.join(EXTRACT, 'raw'))):
            for f in sorted(files):
                text = open(os.path.join(root, f), encoding='utf-8', errors='replace').read()
                for m in re.finditer(r'^// ([0-9A-F]{8})  size [0-9A-F]+  .*?\n(?:.*\n)*?^(\S[^\n{;]*?)\(([^\n]*)\)\s*\n\{',
                                     text, re.M):
                    if m.group(1) in seen:
                        continue
                    seen.add(m.group(1))
                    head = m.group(2).strip()
                    ret = head.rsplit(' ', 1)[0] if ' ' in head else ''
                    args = m.group(3).strip()
                    n = 0 if args in ('', 'void') else args.count(',') + 1
                    w.write(f'{m.group(1)}\t{ret}\t{n}\n')
    # vtable slots (GCC 2.95: 8-byte entries, function pointer at +4, first entry at +0x8)
    elf = open(os.path.join(EXTRACT, 'orig/u2_ngc/u2_ngc_release_dvd.elf'), 'rb').read()
    phoff, = struct.unpack_from('>I', elf, 0x1C)
    phentsize, phnum = struct.unpack_from('>HH', elf, 0x2A)
    SEGS = []
    for i in range(phnum):
        typ, off, va, _, fsz = struct.unpack_from('>IIIII', elf, phoff + i * phentsize)
        if typ == 1 and fsz:
            SEGS.append((off, va, fsz))
    def rd32(va):
        for off, base, sz in SEGS:
            if base <= va < base + sz:
                return struct.unpack_from('>I', elf, off + va - base)[0]
        return None
    with open(os.path.join(DATA, 'vtables.tsv'), 'w') as w:
        for line in open(os.path.join(DATA, 'symbols.tsv')):
            a, k, sz, ns, nm, dem = line.rstrip('\n').split('\t')
            m = re.fullmatch(r'(.+) virtual table', dem)
            if k == 'D' and m:
                base, size = int(a, 16), int(sz, 16)
                slots = [rd32(base + o) or 0 for o in range(0xC, size, 8)]
                w.write(f'{m.group(1)}\t' + ','.join(f'{x:08X}' for x in slots) + '\n')
    print('extracted port/data')

units = sorted((int(a, 16), int(b, 16), u) for a, b, u in
               (l.rstrip('\n').split('\t') for l in open(os.path.join(DATA, 'units.tsv'))))
ustarts = [u[0] for u in units]
def unit_of(a):
    i = bisect.bisect_right(ustarts, a) - 1
    return units[i][2] if i >= 0 and a < units[i][1] else None

sdk_types = set(l.strip() for l in open(os.path.join(DATA, 'sdk_type_names.txt')) if l.strip())
sdk_funcs = set(l.strip() for l in open(os.path.join(DATA, 'sdk_function_names.txt')) if l.strip())

ghidra = {}
for line in open(os.path.join(DATA, 'ghidra_sigs.tsv')):
    a, ret, n = line.rstrip('\n').split('\t')
    ghidra[int(a, 16)] = (ret, int(n))

def write_if_changed(path, text):
    if not os.path.exists(path) or open(path).read() != text:
        open(path, 'w').write(text)

# --- demangled-name parsing ----------------------------------------------------------------------
def split_top(s, sep):
    out, depth, cur, i = [], 0, '', 0
    while i < len(s):
        c = s[i]
        if c in '<([': depth += 1
        elif c in '>)]': depth -= 1
        if depth == 0 and s.startswith(sep, i):
            out.append(cur); cur = ''; i += len(sep); continue
        cur += c; i += 1
    out.append(cur)
    return out

def parse_dem(dem):
    """-> (scope list, name, args string, const) or None"""
    s = dem.strip()
    const = False
    if s.endswith(' const'):
        s, const = s[:-6], True
    if not s.endswith(')'):
        return None
    depth = 0
    for i in range(len(s) - 1, -1, -1):
        if s[i] == ')': depth += 1
        elif s[i] == '(':
            depth -= 1
            if depth == 0:
                args, s = s[i + 1:-1], s[:i]
                break
    else:
        return None
    toks = split_top(s.strip(), ' ')
    q = toks[-1]
    if len(toks) >= 2 and toks[-2].endswith('operator'):
        q = toks[-2] + ' ' + toks[-1]
    parts = [p.strip() for p in split_top(q, '::')]
    if any(not p for p in parts):
        return None
    return parts[:-1], parts[-1], ('' if args.strip() == 'void' else args.strip()), const

def flat(path):
    """class path list -> flattened C++ identifier, e.g. ['A','B'] -> A__B"""
    return '__'.join(ident(p) for p in path)

def ident(s):
    s = s.replace('{anonymous}', 'anon')
    s = re.sub(r'[^A-Za-z0-9_]', '_', s)
    if s in CPP_KEYWORDS or s in BUILTIN:
        s += '_'
    return s

# --- type collection ----------------------------------------------------------------------------
types = set()            # flattened non-template type names
nested = collections.defaultdict(set)   # flat outer -> {(inner simple name, flat inner)}
templates = {}           # base name -> list of 'T' / 'V' (type / value params)

TYPE_TOKEN = re.compile(r'[A-Za-z_][A-Za-z0-9_]*(?:\s*::\s*[A-Za-z_~][A-Za-z0-9_]*)*')

def norm_type(t):
    """Rewrite a demangled type string into a declarable C++ type, registering what it uses."""
    out, i = '', 0
    while i < len(t):
        m = TYPE_TOKEN.match(t, i)
        if not m:
            out += t[i]; i += 1; continue
        word = m.group(0)
        j = m.end()
        if word in BUILTIN:
            out += word; i = j; continue
        parts = [p.strip() for p in word.split('::')]
        # template arguments?
        if j < len(t) and t[j] == '<':
            depth, k = 0, j
            while k < len(t):
                if t[k] == '<': depth += 1
                elif t[k] == '>':
                    depth -= 1
                    if depth == 0: break
                k += 1
            inner = t[j + 1:k]
            args = [a.strip() for a in split_top(inner, ',')]
            kinds, nargs = [], []
            for a in args:
                if re.fullmatch(r'-?\d+[uUlL]*|true|false', a):
                    kinds.append('V'); nargs.append(a)
                else:
                    kinds.append('T'); nargs.append(norm_type(a))
            base = flat(parts)
            if base in sdk_types:
                base += '_game'
            prev = templates.get(base)
            if prev is None or len(kinds) > len(prev):
                templates[base] = kinds
            out += f'{base}<{", ".join(nargs)}>'
            i = k + 1
            continue
        out += register(parts)
        i = j
    return re.sub(r'\s+', ' ', out).strip()

def register(parts):
    if len(parts) == 1 and parts[0] in sdk_types:
        return parts[0]
    if len(parts) == 1 and parts[0].startswith('_') and parts[0][1:] in sdk_types:
        return parts[0][1:]   # struct tag (_GXRenderModeObj) -> SDK typedef (GXRenderModeObj)
    for n in range(1, len(parts) + 1):
        f = flat(parts[:n])
        if f in sdk_types:
            f += '_game'
        types.add(f)
        if n > 1:
            nested[flat(parts[:n - 1])].add((ident(parts[n - 1]), f))
    f = flat(parts)
    return f + '_game' if f in sdk_types else f

# --- functions ------------------------------------------------------------------------------------
funcs = []   # dict(addr, unit, cls_flat or None, cls_simple, name, args, const, static, ret, kind)
for line in open(os.path.join(DATA, 'symbols.tsv')):
    a, k, sz, ns, nm, dem = line.rstrip('\n').split('\t')
    if k != 'F':
        continue
    addr = int(a, 16)
    unit = unit_of(addr)
    if not unit or unit.startswith(SKIP_UNITS):
        continue
    p = parse_dem(dem)
    if p is None:
        # C-linkage names (listed without a parameter list). Skip ones that collide with the host
        # C library: e.g. the game's own 16-bit wcs* routines must be ported under new names.
        if ('(' in dem or ' ' in dem or not re.fullmatch(r'[A-Za-z_]\w*', dem)
                or dem.startswith(('__', '_GLOBAL_')) or dem.startswith(LIBC_PREFIXES) or dem in LIBC_NAMES or dem in sdk_funcs):
            continue
        funcs.append(dict(addr=addr, unit=unit, cls=None, simple=None, name=dem, args='...', const=False,
                          static=False, ret='RET_UNKNOWN', kind='cfunc', dem=dem, virtual=False))
        continue
    scope, name, args, const = p
    if any('<' in s for s in scope) or '<' in name:
        continue      # template instances: regenerated by the compiler from real templates
    if name.startswith('operator') and re.match(r'operator\s*(new|delete)', name):
        continue
    if name.startswith(('__', '_GLOBAL_', 'global ')) or 'static_initialization' in name:
        continue
    if '>::' in args:
        continue      # members of template instances in parameter types; port by hand
    if not scope and const:
        continue
    args = args.replace('unsigned wchar_t', 'unsigned short').replace('__va_list_tag *', 'va_list')
    try:
        nargs = norm_type(args) if args else ''
    except Exception:
        continue
    if '...' in args and nargs.strip() != '...':
        pass
    cls = None
    if scope:
        cls = register(scope)
    gret, gn = ghidra.get(addr, ('', None))
    argc = 0 if not nargs else len(split_top(nargs, ','))
    simple = ident(scope[-1]) if scope else None
    kind = 'ctor' if scope and name == scope[-1] else 'dtor' if scope and name == '~' + scope[-1] else \
           'conv' if re.match(r'operator [A-Za-z_]', name) else 'func'
    static = (bool(scope) and kind == 'func' and gn is not None and gn == argc and not const
              and not name.startswith('operator'))
    if gret in ('void',): ret = 'void'
    elif gret in ('float', 'double', 'bool'): ret = gret
    else: ret = 'RET_UNKNOWN'
    if kind != 'func': ret = ''
    funcs.append(dict(addr=addr, unit=unit, cls=cls, simple=simple, name=name, args=nargs,
                      const=const, static=static, ret=ret, kind=kind, dem=dem, virtual=False))

# --- vtables: virtual methods and single inheritance -------------------------------------------------
by_addr = {f['addr']: f for f in funcs}
vtables = {}   # flat class -> [function addrs in slot order]
for line in open(os.path.join(DATA, 'vtables.tsv')):
    cname, slots = line.rstrip('\n').split('\t')
    if '<' in cname:
        continue
    owner = flat([p.strip() for p in split_top(cname, '::')])
    if owner in sdk_types or owner not in types:
        continue
    vtables.setdefault(owner, [int(x, 16) for x in slots.split(',') if x and int(x, 16)])
for owner, slots in vtables.items():
    for s in slots:
        f = by_addr.get(s)
        if f and f['cls'] == owner and f['kind'] in ('func', 'dtor', 'conv'):
            f['virtual'] = True
            f['static'] = False
# parent = class owning the most slots of ours among classes with their own (smaller) vtable
parent = {}
def slot_sig(a):
    f = by_addr.get(a)
    if not f:
        return None
    if f['kind'] == 'dtor':
        return '~'
    return (f['name'], f['args'], f['const'])
sigs = {c: [slot_sig(a) for a in sl] for c, sl in vtables.items()}
for owner, slots in vtables.items():
    owners = collections.Counter(by_addr[s]['cls'] for s in slots if s in by_addr and by_addr[s]['cls'] not in (None, owner))
    best = None
    for c, _ in owners.most_common():
        if c in vtables and len(vtables[c]) <= len(slots) and c != owner:
            if best is None or len(vtables[c]) > len(vtables[best]):
                best = c
    if best is None:
        # fully-overridden base: its slot signatures are a prefix of ours (longest wins)
        mine = sigs[owner]
        for c, theirs in sigs.items():
            if c == owner or len(theirs) < 2 or len(theirs) >= len(mine) or None in theirs:
                continue
            if mine[:len(theirs)] == theirs and (best is None or len(theirs) > len(sigs[best])):
                best = c
    if best:
        parent[owner] = best
# break cycles defensively
for c in list(parent):
    seen, x = set(), c
    while x in parent:
        if x in seen:
            del parent[c]; break
        seen.add(x); x = parent[x]

# EStorable's own virtuals were all inline (linkonce), so its subclasses are not found from vtables.
# Every class with a GetTypeInfo method and no other base is an EStorable.
storables = {f['cls'] for f in funcs if f['name'] == 'GetTypeInfo' and f['kind'] == 'func' and not f.get('static')}
for c in storables:
    if c != 'EStorable' and c not in parent:
        parent[c] = 'EStorable'

# Likewise StateMachineState: its default virtuals are inline, so states that keep any of them are
# recognised by those addresses in their vtables.
sms_inline = {0x803A024C, 0x803A0250, 0x803A0288, 0x803A02BC, 0x803A02C0, 0x803A02C4, 0x803A02C8}
for line in open(os.path.join(DATA, 'vtables.tsv')):
    cname, slots = line.rstrip('\n').split('\t')
    if '<' in cname:
        continue
    owner = flat([p.strip() for p in split_top(cname, '::')])
    addrs = [int(x, 16) for x in slots.split(',') if x]
    names = [by_addr[a]['name'] if a in by_addr else None for a in addrs[1:7]]
    sms_layout = names == ['Startup', 'Shutdown', 'Reset', 'Update', 'Draw', 'DrawTopmost']
    if owner in types and owner not in parent and owner != 'StateMachineState' and \
            (sms_inline & set(addrs) or sms_layout):
        parent[owner] = 'StateMachineState'

# Overrides must share one return type across a hierarchy: pick it per (root class, signature).
def root_of(c):
    while c in parent:
        c = parent[c]
    return c
vgroups = collections.defaultdict(list)
for f in funcs:
    if f['virtual'] and f['kind'] == 'func':
        vgroups[(root_of(f['cls']), f['name'], f['args'], f['const'])].append(f)
for fs in vgroups.values():
    rets = {f['ret'] for f in fs}
    if len(rets) > 1:
        r = next((x for x in ('float', 'double') if x in rets), 'RET_UNKNOWN')
        for f in fs:
            f['ret'] = r

# --- hand-recovered signatures (port/overrides.tsv: ADDR <tab> in-class declaration) -------------
OVR = re.compile(r'^\s*(static\s+|virtual\s+)?(.*?)\s*\b(~?\w+|operator\s*\S+)\s*\((.*)\)\s*(const)?\s*$')
ovr_path = os.path.join(PORT, 'overrides.tsv')
if os.path.exists(ovr_path):
    for line in open(ovr_path):
        if not line.strip() or line.startswith('#'):
            continue
        a, d = line.rstrip('\n').split('\t', 1)
        f = by_addr.get(int(a, 16))
        m = OVR.match(d)
        if not f or not m:
            print('bad override:', line.strip()); continue
        f['static'] = (m.group(1) or '').strip() == 'static'
        f['ret'] = m.group(2).strip() if f['kind'] in ('func', 'cfunc') else ''
        f['args'] = m.group(4).strip()
        f['const'] = bool(m.group(5))
        f['override'] = True

# --- emit header ---------------------------------------------------------------------------------
def mname(f):
    if f['kind'] == 'ctor': return f['cls']
    if f['kind'] == 'dtor': return '~' + f['cls']
    if f['name'].startswith('operator'):
        return f['name']
    return ident(f['name'])

def decl(f, in_class):
    nm = mname(f)
    if f['kind'] in ('ctor', 'dtor'):
        nm = nm if in_class else f"{f['cls']}::{nm}"
    elif not in_class and f['cls']:
        nm = f"{f['cls']}::{nm}"
    ret = (f['ret'] + ' ') if f['ret'] else ''
    st = 'static ' if (in_class and f['static']) else 'virtual ' if (in_class and f['virtual']) else ''
    if f['kind'] == 'cfunc':
        st = 'extern "C" '
    cst = ' const' if f['const'] else ''
    return f'{st}{ret}{nm}({f["args"]}){cst}'

by_cls = collections.defaultdict(list)
for f in funcs:
    by_cls[f['cls']].append(f)
# `static f()` and `f() const` cannot coexist: demote statics that clash with a const overload
for fs in by_cls.values():
    consts = {(f['name'], f['args']) for f in fs if f['const']}
    for f in fs:
        if f['static'] and (f['name'], f['args']) in consts:
            f['static'] = False

# a method signature must not repeat inside one class
def uniq(fs, in_class):
    seen, out = set(), []
    for f in sorted(fs, key=lambda f: f['addr']):
        key = (mname(f), f['args'], f['const'])
        if key in seen:
            continue
        seen.add(key)
        out.append(f)
    return out

L = ['// GENERATED by tools/gen_port.py from u2_ngc_release_dvd.map + Ghidra output. Do not edit.',
     '// Every class, method and free function of the game, with best-guess signatures.',
     '// Nested types are flattened (A::B -> A__B) and re-exposed inside their parent with `using`.',
     '#pragma once', '#include "simmy/port.hpp"', '']
type_list = sorted(t for t in types if t not in templates)
for t in type_list:
    L.append(f'struct {t};')
# Hand-written templates: a header containing `// @template Name` defines the template for real
# (it is included here, before any class can use it, instead of the empty placeholder).
hand_templates = {}
for root, _, files in os.walk(os.path.join(PORT, 'include')):
    for fn in files:
        if fn.endswith('.hpp'):
            path = os.path.join(root, fn)
            for m in re.finditer(r'^//\s*@template\s+(\w+)\s*$', open(path).read(), re.M):
                hand_templates[m.group(1)] = os.path.relpath(path, os.path.join(PORT, 'include')).replace(os.sep, '/')
for base, kinds in sorted(templates.items()):
    if base in hand_templates:
        L.append(f'#include "{hand_templates[base]}"  // hand-written template')
        continue
    ps = ', '.join(f'class T{i}' if k == 'T' else f'long V{i}' for i, k in enumerate(kinds))
    L.append(f'template <{ps}> struct {base} {{}};')
L.append('')
# parents must be complete before their children
# Hand-written class definitions: a header under include/ containing `// @class Name [after A B]`
# replaces the generated (member-less) body. Its `struct Name : public Base` line sets the parent.
hand, after = {}, collections.defaultdict(list)
for root, _, files in os.walk(os.path.join(PORT, 'include')):
    for fn in files:
        if not fn.endswith('.hpp'):
            continue
        path = os.path.join(root, fn)
        text = open(path).read()
        for m in re.finditer(r'^//\s*@class\s+(\w+)(?:\s+after\s+([\w ]+))?\s*$', text, re.M):
            name = m.group(1)
            hand[name] = os.path.relpath(path, os.path.join(PORT, 'include')).replace(os.sep, '/')
            if m.group(2):
                after[name] += m.group(2).split()
            b = re.search(rf'^(?:struct|class)\s+{name}\s*:\s*(?:public\s+)?(\w+)', text, re.M)
            if b:
                parent[name] = b.group(1)
            elif name in parent:
                del parent[name]

# Classes known only from hand-written headers (e.g. ones whose methods were all inline, so the
# map never listed them) still get a forward declaration and a place in the ordering.
for name in sorted(hand):
    if name not in type_list:
        type_list.append(name)
        L.append(f'struct {name};')

# Generated overrides of a virtual declared in a hand-written ancestor take that return type.
hand_virtual = collections.defaultdict(dict)   # class -> {method name: return type}
for name, rel in hand.items():
    text = open(os.path.join(PORT, 'include', rel)).read()
    body = re.search(rf'^(?:struct|class)\s+{name}\b[^{{]*\{{(.*?)^\}};', text, re.M | re.S)
    if body:
        for m in re.finditer(r'^\s*virtual\s+(.+?)\s*\b(~?\w+)\s*\(([^)]*)\)', body.group(1), re.M):
            a = m.group(3).strip()
            n = 0 if a in ('', 'void') else len(split_top(a, ','))
            hand_virtual[name][(m.group(2), n)] = m.group(1).strip()
for f in funcs:
    # (non-virtual ones too: the same signature in a subclass is an override even when unmarked)
    if f['kind'] != 'func' or f['cls'] in hand or f.get('static'):
        continue
    c = parent.get(f['cls'])
    while c:
        key = (f['name'], 0 if not f['args'] else len(split_top(f['args'], ',')))
        if key in hand_virtual.get(c, {}):
            f['ret'] = hand_virtual[c][key]
            break
        c = parent.get(c)

ordered, done = [], set()
def visit(t):
    if t in done: return
    done.add(t)
    if t in parent: visit(parent[t])
    for d in after.get(t, ()): visit(d)
    ordered.append(t)
for t in type_list:
    visit(t)

# which functions are ported? (an original address in a `// XXXXXXXX` comment under port/src)
ported = set()
for root, _, files in os.walk(os.path.join(PORT, 'src')):
    for fn in files:
        if fn.endswith(('.cpp', '.c', '.hpp', '.h')):
            for m in re.finditer(r'^\s*//\s*([0-9A-Fa-f]{8})\b', open(os.path.join(root, fn)).read(), re.M):
                ported.add(int(m.group(1), 16))

emitted = set()
for t in ordered:
    if t in hand:
        L.append(f'#include "{hand[t]}"  // hand-written')
        for f in uniq(by_cls.get(t, []), True):
            emitted.add(f['addr'])
        continue
    L.append(f'struct {t}' + (f' : public {parent[t]}' if t in parent else '') + ' {')
    for simple, fl in sorted(nested.get(t, ())):
        L.append(f'    using {simple} = ::{fl};')
    ms = uniq(by_cls.get(t, []), True)
    if not any(f['kind'] == 'ctor' and not f['args'] for f in ms):
        L.append(f'    {t}() = default;  // generated: lets derived stubs construct the base')
    for f in ms:
        L.append(f'    {decl(f, True)};  // {f["addr"]:08X}')
        emitted.add(f['addr'])
    L.append('};')
L.append('')
L.append('// ---- free functions')
for f in uniq(by_cls.get(None, []), False):
    if ident(f['name']) in types:
        continue
    if f['kind'] == 'cfunc' and f['addr'] in ported and not f.get('override'):
        continue      # ported C function without an override: its own header declares it
    L.append(f'{decl(f, False)};  // {f["addr"]:08X}')
    emitted.add(f['addr'])
os.makedirs(os.path.join(GEN, 'include/simmy'), exist_ok=True)
write_if_changed(os.path.join(GEN, 'include/simmy/types_gen.hpp'), '\n'.join(L) + '\n')

# --- emit stubs --------------------------------------------------------------------------------
stubdir = os.path.join(GEN, 'src/stubs')
os.makedirs(stubdir, exist_ok=True)

per_unit = collections.defaultdict(list)
for f in funcs:   # every unit gets a file (possibly empty) so the build's file list never changes
    per_unit[re.sub(r'_part\d+', '', f['unit'])]
for f in funcs:
    if f['addr'] in emitted and f['addr'] not in ported:
        per_unit[re.sub(r'_part\d+', '', f['unit'])].append(f)
nstub = 0
for unit, fs in sorted(per_unit.items()):
    S = [f'// GENERATED stubs for unported functions of {unit}. Do not edit.',
         '#include "simmy/types_gen.hpp"', '']
    for f in fs:
        d = decl(f, False)
        if f['ret'] in ('', 'void') and f['kind'] != 'conv':
            body = f'SIMMY_STUB("{f["dem"]}", 0x{f["addr"]:08X});'
        else:
            body = f'SIMMY_STUB("{f["dem"]}", 0x{f["addr"]:08X}); return {{}};'
        S.append(f'{d} {{ {body} }}')
        nstub += 1
    write_if_changed(os.path.join(stubdir, unit.replace('/', '__').rsplit('.', 1)[0] + '.cpp'), '\n'.join(S) + '\n')

with open(os.path.join(GEN, 'functions.tsv'), 'w') as w:
    for f in funcs:
        w.write(f"{f['addr']:08X}\t{f['unit']}\t{'ported' if f['addr'] in ported else 'stub'}\t{decl(f, False)}"
                f"\t{f['cls'] or ''}\t{decl(f, True)}\n")
print(f'types={len(type_list)} templates={len(templates)} functions={len(funcs)} declared={len(emitted)} '
      f'ported={len(ported & {f["addr"] for f in funcs})} stubs={nstub} stubfiles={len(per_unit)}')
