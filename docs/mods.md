# PC mod: mods (objects from the PC game)

Lets the player add objects converted from **their own** copy of The Sims 2 for PC. Off unless a mod
folder exists; with none, nothing in the game changes (no extra memory, same determinism hash).

**Legal rule.** The PC game's files are EA's. Nothing converted from them goes into this repository,
its releases or its docs. The converter reads the user's install on the user's machine and writes into
the user's mod folder; it refuses to write inside the source tree.

## Using it

On Windows the quick way is `tools\windows\convert_pc_object.bat` (copy it next to `build_simmy.bat`):
with no arguments it converts the No-Fuss Ficus into `mods` next to simmy.exe; `list <text>` searches
PC object names; `"<PC object>" <GameCube base>` converts another one. Its PC and disc paths are set
at the top of the file. By hand:

1. Convert an object (Python 3 with numpy and Pillow):

   ```
   python tools/pcobj/pcobj_convert.py --pc "C:\Games\The Sims 2 Ultimate Collection" --list ficus
   python tools/pcobj/pcobj_convert.py --pc "C:\Games\The Sims 2 Ultimate Collection" \
       --gc D:\Games\Sims2.iso --object ficus --base plant_floor_rubbertree --out C:\Games\SIMMY\mods
   ```

   `--pc` is the install folder (every `TSData/Res` under it is searched, base game and packs);
   `--gc` the GameCube data the port plays (disc image or extracted folder); `--object` a PC GUID or
   part of the object's name; `--base` the GameCube object whose behaviour the new object copies.
   Optional: `--name`, `--price`, `--texsize` (default 256).
2. Start the game. The mod folder is `mods=` in simmy.ini (relative to the ini), else `SIMMY_MODS`,
   else `mods` next to the exe. The log says `[mods] <dir>: n mod(s), n object(s), n resource(s)`
   and one line per object added (or why one was skipped).
3. The object is in the buy catalogue on the base object's tab, with the PC name, description, price
   and a rendered thumbnail.

## Mod folder

```
mods/
  pc_plantfloorficus/
    mod.ini                 [object] guid, base (or base_guid), name, desc, price, model, thumbnail
    models_96DA08B7.res     resources in the GameCube's own formats, as an archive holds them
    shaders_1FBB3FA3.res    ("<manager>_<id>.res": models, shaders, textures, ... any manager)
    textures_3A540C0D.res
    thumbnail.png           (for the user; not read)
```

Mods are read in folder-name order; a GUID or resource id given twice keeps the first. A resource id
the disc has is never taken from a mod (the disc wins).

## How it works

| Piece | Where | What |
|---|---|---|
| Mod folder | `src/platform/pc_mods.cpp` (`simmy::mods_init`, `mods_objects`, `mods_resource`) | Read once at start-up (after `settings_load`), immutable afterwards (house loads run on a thread) |
| Resources | `EResourceLoaderImpl::DoLoadResource` (e_resloader.cpp) | An id the archive index lacks: the mod's bytes through a memory `EFile`, loaded by the manager's own loader (`ERModel::LoadModel`, `ERShader`, `ERTexture`) |
| Object rows | `ObjectFolderImpl::AddModObjects` (src/game/modobjects.cpp), end of `LoadDatabase` | A copy of the base's `ObjDefinition` (GUID +0x1C, price +0x24), its ResData (thumbnail +0x1C, no animated character +0x20) and its ObjAnimDef table (the mod's model in every state; animation, particles, shader swaps dropped). The copies live in the "SimsObjects" image's **mod tail** (`ERQuickdata::ModTail`, 64 KB after the cells, only with a mod folder) so their self-relative pointers work like the originals'. The selector shares the base's `ResFile` row: trees, tree tables, routing slots, semi-globals, catalogue function |
| Catalogue | `ObjSelector::GetCatalogName/Description/ShortName` (objselector_catalog.cpp) | A mod object's own text (`m_modNameSlot`, `m_modDescSlot`) |
| Saves | unchanged | `objt` names types by GUID; a mod object saves and loads like any other |

Only single-tile bases with a model are taken (not people, portals or multi-tile groups).

**Unknown GUIDs (mod removed).** The saved type has no selector, so `ObjectFolderImpl::ReconSelector`
gives the placeholder (GUID 0x3C08965F); `ObjectModuleImpl::DoStream` reads the object's stream and
kills the placeholders, as the GameCube does for any missing type. The lot loads without the object; the
next save no longer has it. A tree frame owned by the missing type has no behaviour and is popped
(`TreeSimImpl::DoNodeAction`, GC). Tested: save with three mod objects, load with and without the mod,
save without it, load with it again (`SIMMY_DUMP_WORLD=1`).

## The converter (tools/pcobj/)

| File | What |
|---|---|
| `dbpf.py` | DBPF packages 1.x (index 7.0-7.2), the DIR compression directory, QFS/RefPack |
| `rcol.py` | Scenegraph resources: CRES, SHPE, GMND (names, links), GMDC v4 (elements, links, groups), TXMT, TXTR (v9), LIFO |
| `sg.py` | Name -> instance: CRC-24 (poly 0x864CFB, init 0xB704CE) of the lower-cased name \| 0xFF000000, instance-high CRC-32 (MSB first); group 0x1C0532FA |
| `gcdata.py` | GameCube data (folder or GCM image), the SimsObjects quickdata, EDataHeader |
| `pcobj_convert.py` | OBJD (GUID +0x5C, price field 0x12) -> STR# 0x85 model name -> CRES -> SHPE (group -> material) -> GMND -> GMDC; shadow groups dropped; TXMT base texture -> TXTR/LIFO (DXT1/3/5, raw) -> RGBA8 TXFL (GX tiles, <= 256); a MODL v0x3A with one float indexed strip (GX_TRIANGLES, vertex format 7) per material, scale 1 (PC GMDC units are the GameCube's world units: Z up, 1 = a tile); the base model's first shader with the new texture (alpha test from the TXMT); a 64x64 software-rendered thumbnail with the base's thumbnail shader |

Semi-global groups in objects.package are `0x7F000000 | CRC-24(lower-case GLOB name)`.

## Limits

- Decorative objects only: the copy behaves exactly like its GameCube base (room score, price-driven
  value, its interactions if any). PC behaviour (BHAVs) is not converted: see the feasibility report.
- Placement footprint is the base's (one tile); big PC meshes overhang.
- No LOD, no PC lighting/specular materials, no animated or skinned meshes, one texture per material.
- The fire-code meter counts the converted model's size like any model.
