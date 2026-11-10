"""A PC object's model as plain meshes: CRES (the transform hierarchy) -> SHPE (subset -> material,
the level-of-detail GMNDs) -> GMND -> GMDC (the geometry). Each mesh is in the object's own space
(Z up, 1 = a tile, the origin the middle of its first tile), with its node transforms applied.

Left out: shadow shapes and subsets, bounding boxes, levels of detail other than the highest.
"""
import numpy as np

import rcol

SKIP_WORDS = ('shadow', 'boundingbox', 'bounding_box', 'bbox', 'mirrorplane')


def _skip(name):
    n = (name or '').lower().replace('shadowbox', 'sbox')
    return any(w in n for w in SKIP_WORDS)


def shape_info(sh):
    """(GMND names (highest level of detail first), {subset: material}) from a SHPE's strings."""
    strings = sh['strings']
    gmnds = [s for s in strings if s.lower().endswith('_gmnd')]
    rest = [s for s in strings if s not in gmnds and s not in ('cReferentNode', 'cObjectGraphNode')]
    pairs = {}
    for k in range(0, len(rest) - 1, 2):
        pairs.setdefault(rest[k].lower(), rest[k + 1])
    return gmnds, pairs


def load_meshes(inst, cres_name, materials=None, log=lambda *a: None):
    """[(material name, subset, P, N, UV, T)] of the model; `materials` {subset: material} overrides the
    shapes' own (the object's default MMATs). Raises LookupError when the model is not found."""
    d, e = inst.find(rcol.T_CRES, cres_name)
    if d is None:
        raise LookupError(f'{cres_name}: model (CRES) not found')
    links, _, _ = rcol.rcol_header(d)
    try:
        xforms = rcol.cres_shape_transforms(d)
    except Exception:
        xforms = {}
    out = []
    seen_gmdc = set()
    for li, (t, g, i, ih) in enumerate(links):
        if t != rcol.T_SHPE:
            continue
        pk, se = inst.find_tgi(t, i, ih)
        if not se:
            log(f'  shape {li}: not found')
            continue
        sh = rcol.parse_shpe(pk.read(se))
        gmnds, pairs = shape_info(sh)
        if not gmnds or _skip(gmnds[0]) or all(_skip(m) for m in pairs.values()) and pairs:
            continue
        R, tr, rooted = xforms.get(li, (np.eye(3), (0.0, 0.0, 0.0), True))
        R = np.asarray(R, np.float64); tr = np.asarray(tr, np.float64)
        gn = gmnds[0]
        gd, _ = inst.find(rcol.T_GMND, gn.split('!')[-1])
        if gd is None:
            log(f'  {gn}: not found')
            continue
        for (t3, g3, i3, ih3) in rcol.parse_names_generic(gd)['links']:
            if t3 != rcol.T_GMDC:
                continue
            pk3, e3 = inst.find_tgi(t3, i3, ih3)
            if not e3:
                continue
            key = (i3, ih3, li)
            if key in seen_gmdc:
                continue
            seen_gmdc.add(key)
            gm = rcol.parse_gmdc(pk3.read(e3))
            bones = [_bone(q, tt) for (q, tt) in gm.get('transforms', [])]
            for gi, grp in enumerate(gm['groups']):
                subset = grp['name']
                mat = (materials or {}).get(subset.lower()) or pairs.get(subset.lower())
                if _skip(subset) or (mat and _skip(mat)) or grp['prim'] != 2:
                    continue
                m, tris = rcol.gmdc_group_mesh(gm, gi)
                if 'Vertices' not in m or not tris:
                    continue
                n = len(m['Vertices'])
                P = np.array(m['Vertices'], np.float64)[:, :3]
                N = np.array(m['Normals'], np.float64)[:n, :3] if len(m.get('Normals', [])) >= n else np.zeros_like(P)
                U = np.array(m['UV'], np.float64)[:n, :2] if len(m.get('UV', [])) >= n else np.zeros((n, 2))
                T = np.array(tris, np.int64)
                # rigid parts bound to one joint (GMDC bone transforms): into the model's space
                bi = m.get('BlendIndices')
                if bones and bi and grp['joints']:
                    P, N = _skin(P, N, bi, grp['joints'], bones)
                    log('skinned (bind pose)')
                P = P @ R.T + tr
                N = N @ R.T
                used = np.unique(T)
                remap = np.full(n, -1, np.int64)
                remap[used] = np.arange(len(used))
                out.append((mat or subset, subset, P[used], N[used], U[used], remap[T], rooted))
    return _place_orphans(out, log)


def _place_orphans(meshes, log):
    """Mirrors whose frame hangs from a node the CRES leaves unplaced (its place is in the object's
    animation): the frame is moved onto the reflection (same middle across and up; its front at the
    glass). Returns [(material, subset, P, N, UV, T)]."""
    glass = [m for m in meshes if m[6] and any(w in (m[0] + m[1]).lower() for w in ('reflection', 'glass', 'mirror'))]
    orphans = [m for m in meshes if not m[6]]
    gc = oc = None
    if glass and orphans:
        G = np.concatenate([m[2] for m in glass])
        O = np.concatenate([m[2] for m in orphans])
        gc = (G.min(0) + G.max(0)) / 2
        oc = (O.min(0) + O.max(0)) / 2
    if gc is not None and np.linalg.norm((gc - oc)[[0, 2]]) > 0.25:
        d = np.array([gc[0] - oc[0], G.max(0)[1] + 0.035 - O.max(0)[1], gc[2] - oc[2]])
        log('mirror frame moved onto its glass')
        meshes = [(m[0], m[1], m[2] + d, m[3], m[4], m[5], True) if not m[6] else m for m in meshes]
    elif orphans and gc is None:
        log(f'{len(orphans)} part(s) placed by animation (kept where the mesh puts them)')
    return [m[:6] for m in meshes]


def _bone(q, t):
    """A GMDC bone transform (the inverse bind pose): rotation quaternion (x, y, z, w) + translation."""
    R = np.asarray(rcol.quat_matrix(q), np.float64)
    return R, np.asarray(t, np.float64)


def _skin(P, N, bi, joints, bones):
    """Vertices bound to bones: v' = inverse(bone) v (the bind pose), per vertex's first bone."""
    P2 = P.copy(); N2 = N.copy()
    for v in range(len(P)):
        raw = bi[v][0] if v < len(bi) else 0
        b = int(raw) & 0xFF if not isinstance(raw, float) else int(raw)
        if b >= len(joints):
            continue
        j = joints[b]
        if j >= len(bones):
            continue
        R, t = bones[j]
        P2[v] = R.T @ (P[v] - t)
        N2[v] = R.T @ N[v]
    return P2, N2
