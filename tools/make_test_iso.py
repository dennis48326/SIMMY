#!/usr/bin/env python3
"""Packs a folder into a GameCube disc image (plain GCM / .iso) for testing SIMMY's disc reader.

    python tools/make_test_iso.py <folder> <out.iso> [--id SIMT01] [--title "..."] [--exclude NAME ...]
                                  [--align 32768] [--full-size]

<folder> is an extracted disc (DATA/, default.NGH, ...): its files become the disc's file system.
The image has what a real disc has in front of the files: boot.bin (game ID, the GameCube magic
0xC2339F3D, the title, the FST's place), bi2.bin, an apploader header and a DOL header (both empty
placeholders: the image cannot boot in an emulator, it only carries files) and the FST (file system
table) with its name strings. Files are aligned to 32 KB (--align; real discs use 32 KB or 4 bytes).
Standard library only.
"""
import argparse
import os
import struct
import sys

GC_MAGIC = 0xC2339F3D
FULL_DISC_SIZE = 1459978240  # a full-size GameCube disc (mini DVD)


def align(value, to):
    return (value + to - 1) // to * to


def build_tree(root, excludes):
    """[(name, path, children|None)] sorted like the SDK's makers (case-insensitive)."""
    items = []
    for name in sorted(os.listdir(root), key=lambda n: n.lower()):
        if name in excludes:
            continue
        path = os.path.join(root, name)
        if os.path.isdir(path):
            items.append((name, path, build_tree(path, excludes)))
        elif os.path.isfile(path):
            items.append((name, path, None))
    return items


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder")
    ap.add_argument("out")
    ap.add_argument("--id", default="SIMT01", help="6-character game ID (default SIMT01, a test ID)")
    ap.add_argument("--title", default="SIMMY test disc (The Sims 2 files)")
    ap.add_argument("--exclude", action="append", default=None,
                    help="file or folder name to leave out (repeatable; default: decomp)")
    ap.add_argument("--align", type=lambda v: int(v, 0), default=0x8000, help="file alignment (default 0x8000)")
    ap.add_argument("--full-size", action="store_true", help="pad (sparsely) to a full disc's 1.36 GiB")
    args = ap.parse_args()
    excludes = set(args.exclude if args.exclude is not None else ["decomp"])
    game_id = args.id.encode("ascii")
    if len(game_id) != 6:
        sys.exit("--id must be 6 characters")
    if args.align < 4 or args.align & 3:
        sys.exit("--align must be a multiple of 4")

    tree = build_tree(args.folder, excludes)

    # FST entries: (is_dir, name_offset, a, b, src_path). Files: a = offset, b = size. Folders:
    # a = parent index, b = index after the last entry inside it.
    entries = [[1, 0, 0, 0, None]]
    strings = bytearray(b"\0")  # the root's (empty) name
    files = []

    def add(items, parent):
        for name, path, children in items:
            name_off = len(strings)
            strings.extend(name.encode("utf-8") + b"\0")
            index = len(entries)
            if children is None:
                entries.append([0, name_off, 0, os.path.getsize(path), path])
                files.append(index)
            else:
                entries.append([1, name_off, parent, 0, None])
                add(children, index)
                entries[index][3] = len(entries)

    add(tree, 0)
    entries[0][3] = len(entries)

    # Layout: boot.bin, bi2.bin, apploader, DOL, FST, files.
    apploader_off = 0x2440
    apploader = bytearray(0x20)
    apploader[0:10] = b"2004/11/30"
    struct.pack_into(">III", apploader, 0x10, 0x81200000, 0, 0)  # entry point, size, trailer size
    dol_off = align(apploader_off + len(apploader), 0x100)
    dol = bytearray(0x100)
    struct.pack_into(">I", dol, 0xE0, 0x80003100)  # entry point; no sections (a placeholder)
    fst_off = align(dol_off + len(dol), 4)
    fst_size = len(entries) * 12 + len(strings)
    pos = align(fst_off + fst_size, args.align)
    for index in files:
        e = entries[index]
        e[2] = pos
        pos = align(pos + e[3], args.align) if index != files[-1] else pos + e[3]
    end = pos
    if end > 0xFFFFFFFF:
        sys.exit("the files do not fit a GameCube disc (32-bit offsets)")

    fst = bytearray()
    for is_dir, name_off, a, b, _ in entries:
        fst += struct.pack(">III", (is_dir << 24) | name_off, a, b)
    fst += strings

    boot = bytearray(0x440)
    boot[0:6] = game_id
    struct.pack_into(">I", boot, 0x1C, GC_MAGIC)
    title = args.title.encode("ascii", "replace")[:0x3DF]
    boot[0x20:0x20 + len(title)] = title
    struct.pack_into(">IIII", boot, 0x420, dol_off, fst_off, fst_size, fst_size)
    bi2 = bytearray(0x2000)
    struct.pack_into(">I", bi2, 0x04, 0x01800000)  # simulated memory size: 24 MB
    struct.pack_into(">I", bi2, 0x18, 1)           # country code: 1 (USA)

    with open(args.out, "wb") as out:
        for off, blob in ((0, boot), (0x440, bi2), (apploader_off, apploader), (dol_off, dol), (fst_off, fst)):
            out.seek(off)
            out.write(blob)
        for index in files:
            _, _, offset, size, path = entries[index]
            out.seek(offset)
            with open(path, "rb") as src:
                copied = 0
                while True:
                    chunk = src.read(1 << 22)
                    if not chunk:
                        break
                    out.write(chunk)
                    copied += len(chunk)
            if copied != size:
                sys.exit("%s changed while packing" % path)
        out.truncate(max(end, FULL_DISC_SIZE if args.full_size else 0))
    print("%s: %s \"%s\", %d files, %d folders, FST at 0x%X (%d bytes), %.1f MB" % (
        args.out, args.id, args.title, len(files), len(entries) - 1 - len(files), fst_off, fst_size,
        os.path.getsize(args.out) / 1048576.0))


if __name__ == "__main__":
    main()
