"""GameCube textures for the PC object converter: CMPR (the GameCube's DXT1, in GX tile order) for
opaque and cut-out textures, RGB5A3 for textures with real alpha, RGBA8 kept for tests. Each with its
mip levels (down to 8x8), wrapped in the archive's TXFL resource (EDataHeader + ETextureDef).

PC DXT1 levels are moved over block for block (no re-compression); anything else is compressed here
(numpy, a principal-axis fit per 4x4 block).
"""
import struct

import numpy as np

import gcdata

FMT_CMPR, FMT_RGB5A3, FMT_RGBA8 = 0x81, 0x82, 0x85


# ---------------------------------------------------------------------------------------------------
# DXT1 / CMPR blocks

def _to565(c):
    c = np.clip(np.round(c), 0, 255).astype(np.int32)
    return ((c[..., 0] * 31 + 127) // 255) << 11 | ((c[..., 1] * 63 + 127) // 255) << 5 | ((c[..., 2] * 31 + 127) // 255)


def _from565(v):
    v = v.astype(np.int32)
    return np.stack([((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31], -1)


def encode_dxt1(img):
    """RGBA (h, w, 4), h and w multiples of 4 -> DXT1 blocks (n, 8) in PC order (row-major blocks; colours
    little-endian, pixel 0 in the low bits). Pixels with alpha < 128 become transparent (3-colour blocks)."""
    h, w = img.shape[:2]
    b = img.reshape(h // 4, 4, w // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4).astype(np.float64)
    rgb, a = b[..., :3], b[..., 3]
    trans = a < 128
    anyt = trans.any(1)
    wgt = (~trans).astype(np.float64)
    cnt = np.maximum(wgt.sum(1, keepdims=True), 1)
    mean = (rgb * wgt[..., None]).sum(1) / cnt
    d = (rgb - mean[:, None]) * wgt[..., None]
    cov = np.einsum('npi,npj->nij', d, d)
    v = np.ones((len(b), 3)) / np.sqrt(3)
    for _ in range(6):
        v = np.einsum('nij,nj->ni', cov, v)
        v /= np.maximum(np.linalg.norm(v, axis=1, keepdims=True), 1e-9)
    proj = np.einsum('npi,ni->np', rgb - mean[:, None], v)
    proj_o = np.where(trans, np.nan, proj)
    lo = np.nanmin(np.where(np.isnan(proj_o).all(1, keepdims=True), 0, proj_o), 1)
    hi = np.nanmax(np.where(np.isnan(proj_o).all(1, keepdims=True), 0, proj_o), 1)
    e0 = mean + v * hi[:, None]
    e1 = mean + v * lo[:, None]
    c0 = _to565(e0); c1 = _to565(e1)
    # 4-colour blocks need c0 > c1, 3-colour (transparent) blocks c0 <= c1
    swap4 = (~anyt) & (c0 < c1)
    swap3 = anyt & (c0 > c1)
    sw = swap4 | swap3
    c0, c1 = np.where(sw, c1, c0), np.where(sw, c0, c1)
    eq = (~anyt) & (c0 == c1)
    # equal endpoints in a 4-colour block: nudge so the block stays in 4-colour mode
    c1 = np.where(eq & (c1 > 0), c1 - 1, c1)
    c0 = np.where(eq & (c1 == 0) & (c0 == 0), 1, c0)
    p0, p1 = _from565(c0).astype(np.float64), _from565(c1).astype(np.float64)
    four = (c0 > c1)
    p2 = np.where(four[:, None], (2 * p0 + p1) / 3, (p0 + p1) / 2)
    p3 = np.where(four[:, None], (p0 + 2 * p1) / 3, np.nan)
    pal = np.stack([p0, p1, p2, p3], 1)  # (n, 4, 3)
    dist = ((rgb[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)  # (n, 16, 4)
    dist = np.where(np.isnan(dist), np.inf, dist)
    idx = dist.argmin(-1)
    idx = np.where(trans & anyt[:, None], 3, idx)
    bits = np.zeros(len(b), np.uint32)
    for k in range(16):
        bits |= (idx[:, k].astype(np.uint32) & 3) << (2 * k)
    out = np.zeros((len(b), 8), np.uint8)
    out[:, 0] = c0 & 0xFF; out[:, 1] = c0 >> 8
    out[:, 2] = c1 & 0xFF; out[:, 3] = c1 >> 8
    for k in range(4):
        out[:, 4 + k] = (bits >> (8 * k)) & 0xFF
    return out


def dxt1_to_cmpr(blocks, w, h):
    """PC DXT1 blocks (row-major, (h/4)*(w/4) of 8 bytes) -> GX CMPR image bytes (8x8 tiles of 2x2
    blocks; colours big-endian; each row's four 2-bit indices with the first pixel in the high bits)."""
    bw, bh = w // 4, h // 4
    blk = np.frombuffer(bytes(blocks), np.uint8).reshape(bh, bw, 8).copy()
    out = blk.copy()
    out[..., 0], out[..., 1] = blk[..., 1], blk[..., 0]
    out[..., 2], out[..., 3] = blk[..., 3], blk[..., 2]
    rows = blk[..., 4:8]
    # reverse the four 2-bit fields of each byte
    r = rows
    r = ((r & 0x03) << 6) | ((r & 0x0C) << 2) | ((r & 0x30) >> 2) | ((r & 0xC0) >> 6)
    out[..., 4:8] = r
    # tile order: 8x8 pixel tiles (2x2 blocks), row-major; inside, blocks TL, TR, BL, BR
    out = out.reshape(bh // 2, 2, bw // 2, 2, 8).transpose(0, 2, 1, 3, 4)
    return out.tobytes()


# ---------------------------------------------------------------------------------------------------
# Other formats

def gx_rgb5a3(img):
    h, w = img.shape[:2]
    a = img[..., 3].astype(np.uint16); r = img[..., 0].astype(np.uint16)
    g = img[..., 1].astype(np.uint16); bl = img[..., 2].astype(np.uint16)
    opaque = a >= 0xE0
    v_op = 0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | (bl >> 3)
    v_a = (a >> 5) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (bl >> 4)
    v = np.where(opaque, v_op, v_a).astype('>u2')
    t = v.reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3)
    return t.tobytes()


def gx_rgba8(img):
    h, w = img.shape[:2]
    t = img.reshape(h // 4, 4, w // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    ar = t[:, :, [3, 0]].reshape(-1, 32)
    gb = t[:, :, [1, 2]].reshape(-1, 32)
    return np.concatenate([ar, gb], 1).tobytes()


# ---------------------------------------------------------------------------------------------------

def _p2(v, maxdim, mindim=8):
    return 1 << max(mindim.bit_length() - 1, min(maxdim.bit_length() - 1, (max(mindim, v) - 1).bit_length()))


def resize(img, W, H):
    from PIL import Image
    if img.shape[1] == W and img.shape[0] == H:
        return img
    return np.asarray(Image.fromarray(np.ascontiguousarray(img), 'RGBA').resize((W, H), Image.LANCZOS))


def mip_chain(img, mindim=8):
    out = [img]
    while min(out[-1].shape[:2]) // 2 >= mindim:
        a = out[-1].astype(np.float32)
        h, w = a.shape[:2]
        rgb = a[..., :3] * (a[..., 3:4] / 255.0)
        rgb = rgb.reshape(h // 2, 2, w // 2, 2, 3).sum((1, 3))
        al = a[..., 3].reshape(h // 2, 2, w // 2, 2).sum((1, 3))
        col = np.where(al[..., None] > 0, rgb / np.maximum(al[..., None], 1e-6) * 255.0, 0)
        out.append(np.concatenate([col, al[..., None] / 4], -1).clip(0, 255).astype(np.uint8))
    return out


def alpha_kind(img):
    """'opaque', 'cutout' (alpha only near 0 or 255) or 'blend'."""
    a = img[..., 3]
    if a.min() >= 250:
        return 'opaque'
    mid = ((a > 24) & (a < 232)).mean()
    return 'cutout' if mid < 0.03 else 'blend'


def texdef(w, h, fmt, bpp, levels):
    flags = 0xC83 | (0x20 if levels > 1 else 0)
    return struct.pack('>IIII4H4BI', 0, 0, flags, 0, w, h, 0, levels if levels > 1 else 0, fmt, 0, bpp, 0, 0)


def make_txfl(name, img, kind=None, maxdim=256, dxt1_levels=None, mips=True):
    """A TXFL resource for RGBA `img` (any size; scaled to powers of two <= maxdim). `dxt1_levels`
    [(w, h, blocks)] large -> small, when the source is DXT1 at a fitting size: moved over as is.
    Returns (bytes, format name, (w, h))."""
    kind = kind or alpha_kind(img)
    if dxt1_levels and kind != 'blend':
        lv = [(w, h, b) for (w, h, b) in dxt1_levels if w >= 8 and h >= 8 and w <= maxdim and h <= maxdim
              and w & (w - 1) == 0 and h & (h - 1) == 0]
        if lv:
            w0, h0 = lv[0][0], lv[0][1]
            chain = []
            for k, (w, h, b) in enumerate(lv):
                if (w, h) != (max(8, w0 >> k), max(8, h0 >> k)) or (w0 >> k) < 8 or (h0 >> k) < 8:
                    break
                chain.append(dxt1_to_cmpr(b, w, h))
            if not mips:
                chain = chain[:1]
            data = b''.join(chain)
            return gcdata.make_header(9, 0x5458464C, 0xFFFFFFFF, name, texdef(w0, h0, FMT_CMPR, 4, len(chain)) + data), \
                'CMPR', (w0, h0)
    H, W = img.shape[:2]
    if kind == 'blend':
        maxdim = min(maxdim, 128)
    W2, H2 = _p2(W, maxdim), _p2(H, maxdim)
    base = resize(img, W2, H2)
    levels = mip_chain(base) if mips else [base]
    if kind == 'blend':
        data = b''.join(gx_rgb5a3(l) for l in levels)
        return gcdata.make_header(9, 0x5458464C, 0xFFFFFFFF, name, texdef(W2, H2, FMT_RGB5A3, 16, len(levels)) + data), \
            'RGB5A3', (W2, H2)
    data = []
    for l in levels:
        if kind == 'cutout':
            l = l.copy(); l[..., 3] = np.where(l[..., 3] >= 128, 255, 0)
        else:
            l = l.copy(); l[..., 3] = 255
        h, w = l.shape[:2]
        data.append(dxt1_to_cmpr(encode_dxt1(l).tobytes(), w, h))
    return gcdata.make_header(9, 0x5458464C, 0xFFFFFFFF, name, texdef(W2, H2, FMT_CMPR, 4, len(levels)) + b''.join(data)), \
        'CMPR', (W2, H2)


def make_thumb_txfl(name, img):
    """The catalogue thumbnail: RGB5A3, no mips."""
    H, W = img.shape[:2]
    return gcdata.make_header(9, 0x5458464C, 0xFFFFFFFF, name, texdef(W, H, FMT_RGB5A3, 16, 1) + gx_rgb5a3(img))


def decode_cmpr(data, w, h):
    """GX CMPR -> RGBA (tests)."""
    bw, bh = w // 4, h // 4
    blk = np.frombuffer(data[:bw * bh * 8], np.uint8).reshape(bh // 2, bw // 2, 2, 2, 8).transpose(0, 2, 1, 3, 4).reshape(bh, bw, 8)
    c0 = blk[..., 0].astype(np.int32) << 8 | blk[..., 1]
    c1 = blk[..., 2].astype(np.int32) << 8 | blk[..., 3]
    p0, p1 = _from565(c0), _from565(c1)
    four = c0 > c1
    p2 = np.where(four[..., None], (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(four[..., None], (p0 + 2 * p1) // 3, 0)
    pal = np.stack([p0, p1, p2, p3], 2)
    out = np.zeros((h, w, 4), np.uint8)
    for y in range(4):
        row = blk[..., 4 + y]
        for x in range(4):
            idx = (row >> (6 - 2 * x)) & 3
            col = np.take_along_axis(pal, idx[..., None, None].repeat(3, -1), 2)[:, :, 0]
            al = np.where((~four) & (idx == 3), 0, 255)
            out[y::4, x::4, :3] = col
            out[y::4, x::4, 3] = al
    return out
