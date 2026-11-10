# PC mod: mods (objects from the PC game)

Lets the player add objects converted from **their own** copy of The Sims 2 for PC: by default every
buy-mode **Decorative** object of the base game and of every expansion and stuff pack installed (about
560 with the Ultimate Collection). Off unless a mod folder exists; with none, nothing in the game changes
(no extra memory, same determinism hash).

**Legal rule.** The PC game's files are EA's. Nothing converted from them goes into this repository,
its releases or its docs. The converter reads the user's install on the user's machine and writes into
the user's mod folder; it refuses to write inside the source tree.

## Using it

On Windows the quick way is `tools\windows\convert_pc_object.bat` (copy it next to `build_simmy.bat`).
Its PC install and disc paths are set at the top of the file.

| | |
|---|---|
| `convert_pc_object.bat` | every Decorative object of every pack under the install, into `mods` next to simmy.exe (progress per object; about two minutes for every pack on two cores) |
| `convert_pc_object.bat list ficus` | PC objects whose name contains "ficus" |
| `convert_pc_object.bat inventory` | only the inventory (`mods\pcdeco_inventory.tsv`) |
| `convert_pc_object.bat "No-Fuss Ficus"` | one object (name or GUID), base chosen automatically |
| `convert_pc_object.bat "No-Fuss Ficus" plant_floor_rubbertree` | one object with this GameCube base |

By hand (Python 3 with numpy and Pillow, nothing else):

```
python tools/pcobj/pcobj_convert.py --pc "C:\Games\The Sims 2 Ultimate Collection" \
    --gc D:\Games\Sims2.iso --out C:\Games\SIMMY\mods --all-decorative
```

`--pc` is the install folder (every `TSData/Res` under it is a pack); `--gc` the GameCube data the port
plays (disc image or extracted folder). Other modes: `--object <GUID|name> [--base <GameCube object>]`,
`--batch list.tsv` (GUIDs in the first column, an optional base in the second), `--list <text>`,
`--inventory <file.tsv>` (classify only, write nothing else). Options: `--packs base,ep1,sp4,...` (by
home pack), `--jobs N` (worker processes; default CPUs - 1), `--texsize` (largest texture side, 256).
Converting again replaces the folders it wrote; `mods/pcdeco_inventory.tsv` keeps one row per object
(converted or skipped, and why).

Start the game. The mod folder is `mods=` in simmy.ini (relative to the ini), else `SIMMY_MODS`, else
`mods` next to the exe. The log says `[mods] <dir>: n mod(s), n object(s) (n multi-tile parts), n
resource(s); read in n ms` and `[mods] objects: n added (n multi-tile), n skipped; rows ...` (with
`SIMMY_MODS_VERBOSE=1`, or 20 objects or fewer, a line per object; a skipped object always says why).
The objects are in buy mode on the **Decorative** tab with the PC name, description, price and room
rating and a rendered thumbnail. With a mod folder the **C stick** left/right pages that catalogue
four columns at a time (the GameCube's movie only moves a column at a time; the tab can hold 600+).

## What the converter makes of a PC object

**Inventory.** Packs are ordered by release (base, University, Nightlife, Open for Business, Family
Fun, Glamour Life, Pets, Happy Holiday, Celebration!, H&M, Seasons, Teen Style, Bon Voyage, FreeTime,
Kitchen & Bath, IKEA, Apartment Life, Mansion & Garden). Every pack re-ships earlier packs' objects in
its own `Objects/objects.package` (stuff packs from Celebration! on add `StuffPack/Objects`): a GUID's
newest definition wins, its home pack is the oldest that has it. Taken: lead OBJDs (single-tile, or
sub index -1) with the catalogue function Decorative (word 0x28 bit 0x20). Left out: no catalogue
name (CTSS), invisible helpers, diagonal variants (word 0x17/0x18 set: the game swaps them in), roof
objects (no roof placement on the GameCube).

**Placement** (the PC keeps it in the objects' init trees, so it is read off the mesh, the
sub-sort (word 0x5E: 1 wall hangings, 2 sculptures, 4 rugs, 8 plants, 0x10 mirrors, 0x20 curtains, 0x80
misc) and the room sort): against the wall (back at the tile's -y edge) and off the floor or very thin
-> **wall**; curtains -> wall; flat rugs/mats -> **rug**; high up -> **ceiling**; small, on the floor,
indoors -> **surface**; else **floor**. The GameCube base per placement (each checked in the game:
data 4 heights, data 8 flags, data 0xD wall flags as their init trees set them):

| Placement | GameCube base | Behaves as |
|---|---|---|
| wall | `art_wall_painting_01` | wall art: on a wall, "view" |
| mirror (wall) | `skill_charisma_mirror` | on a wall; practise speech (the PC mirrors' too) |
| floor | `art_sculpture_contemporary` | floor sculpture, "view" |
| floor plant | `plant_floor_rubbertree` | needs watering |
| surface | `art_sculpture_wooden_model` | on tables and counters (heights 0x8C), not the floor |
| surface plant | `art_plant_lavender` | the same, a plant |
| rug | `rug_outdoor_blanket` (group) | walkable, objects go on it |
| ceiling | `rug_outdoor_blanket` (group) | a one-tile rug: sims walk under it |

**Multi-tile objects.** The PC parts (OBJDs with the lead's master id; sub index x << 8 | y) give the
footprint and the tiles' GUIDs; mod.ini lists them (`tiles = x,y:GUID ...`). The game makes a group
from the base (see below). The GameCube draws a group's model at the middle of its tiles, so the mesh is
moved there (the PC's origin is the first tile's middle).

**Coordinates.** PC GMDC units are the GameCube's (Z up, 1 = a tile). The GameCube's model space is the
PC's mirrored in y (its world swaps x and y); wall objects are drawn 0.45 tiles off their tile's
middle toward the wall, so their meshes are moved by that (y_gc = -y_pc - 0.45). Mesh parts hang from
the CRES's transform nodes (parsed: cResourceNode, cTransformNode, cShapeRefNode; translation +
quaternion, composed to the root); a few mirrors' frames hang from nodes only their animation places:
they are moved onto their glass.

**Materials and textures.** Per subset the default MMAT (Catalog/Materials) else the SHPE's material;
the TXMT's base texture (or its diffuse colour); `Null` materials and shadow/bounding-box shapes are
dropped; mirror glass (no texture, reflection material) gets a pale sheen texture. Alpha test ->
cut-out (alpha test shader), alpha blend (and additive glows) -> blended. Textures are **CMPR** (the
GameCube's DXT1 in GX 8x8 tiles; a PC DXT1 level is moved over block for block, others are compressed
by a principal-axis fit) with mip levels down to 8x8, or **RGB5A3** (max 128) where the alpha is real.
The largest level <= `--texsize` (256) is taken. Shaders are copies of a disc shader with the layer's
blend (field 0x0C bit 0x40), alpha test (flags bit 4) and reference set. A 64x64 RGB5A3 thumbnail is
software-rendered from the PC mesh.

**Model.** MODL v0x3A, one sub-model, a float indexed strip (GX_TRIANGLES, vertex format 7) per material,
split at 21000 triangles. Mesh: the highest level of detail; skinned groups in their bind pose.

## Mod folder

```
mods/
  pcdeco_inventory.tsv      every decorative object found: GUID, name, pack, sub-sort, footprint,
                            placement, base, price, room, BHAVs, model, textures, recolours, status, ...
  pc3_nofussficus_4cc4fd6c/ "pc<sort>_<name>_<GUID>" (sort: 1 plants, 2 sculptures, 3 wall art,
    mod.ini                 4 mirrors, 5 rugs, 6 curtains, 7 misc: the catalogue's order)
    models_96DA08B7.res     resources in the GameCube's own formats, as an archive holds them
    shaders_1FBB3FA3.res    ("<manager>_<id>.res": models, shaders, textures, ... any manager)
    textures_3A540C0D.res
    thumbnail.png           (for the user; not read)
```

mod.ini `[object]` keys: `guid`, `base` (object file) or `base_guid`, `name`, `desc`, `price`, `model`,
`thumbnail`, `functions` (catalogue function flags: the tab), `room` (catalogue room rating), `tiles`
(`x,y:GUID ...`: a multi-tile object), `pack` (informational). Mods are read in folder-name order; a GUID
or resource id given twice keeps the first. A resource id the disc has is never taken from a mod.

## How it works

| Piece | Where | What |
|---|---|---|
| Mod folder | `src/platform/pc_mods.cpp` (`simmy::mods_init`, `mods_objects`, `mods_resource`, `mods_tail_bytes`) | Read once at start-up (after `settings_load`), immutable afterwards (house loads run on a thread). Only the mod.ini files and the resource file **names** are read (~25 ms for 558 mods); a resource is read from its file each time the game loads it, so nothing of a mod sits in memory until the game uses it (thumbnails when the catalogue shows them, models when an object is in the house) |
| Resources | `EResourceLoaderImpl::DoLoadResource` (e_resloader.cpp) | An id the archive index lacks: the mod's bytes (read by `mods_resource`) through a memory `EFile`, loaded by the manager's own loader (`ERModel::LoadModel`, `ERShader`, `ERTexture`: CMPR 0x81 and RGB5A3 0x82 with mips are the disc's formats) |
| Object rows | `ObjectFolderImpl::AddModObjects` (src/game/modobjects.cpp), end of `LoadDatabase` | A copy of the base's `ObjDefinition` (GUID +0x1C, price +0x24, functions +0x50, room rating +0xB0), its ResData (thumbnail +0x1C, no animated character +0x20) and its ObjAnimDef table (the mod's model in every state; animation, particles, shader swaps dropped). The copies live in the "SimsObjects" image's **mod tail** (`ERQuickdata::ModTail`, `ModTailSize` = `simmy::mods_tail_bytes()`: sized from the mods' objects and tiles, ~1 MB for 558; only that image, only with a mod folder) so their self-relative pointers work like the originals'. The selector shares the base's `ResFile` row: trees, tree tables, routing slots, semi-globals |
| Multi-tile groups | the same | A lead (sub index -1, the catalogue entry; never in the world) and one part per tile (sub index x << 8 \| y), all with a master id of their own (from 0x100 up, per object file) so `GetNthSubSelector` / `GetMasterSelector` find them. From a single-tile base every part is a copy of the base (as the GameCube's two-tile paintings are copies of one-tile ones); from a group base (a rug) the lead is its lead and the parts its parts. The first tile draws the model (room rating on it only); the others have no ResData. Placement, the group's offsets and the drawing at the group's middle are the GameCube's own (`MakeNewOutOfWorldObject`, `PlacementGroup`, `MultiTileGroupCenter`) |
| Catalogue | `ObjSelector::GetCatalogName/Description/ShortName` (objselector_catalog.cpp), `PCTTarget::PageWithCStick` (pcttarget.cpp) | A mod object's own text (`m_modNameSlot`, `m_modDescSlot`); the cell list is a vector (no limit); the C stick pages four columns |
| Saves | unchanged | `objt` names types by GUID; a mod object (and each part) saves and loads like any other |

**Unknown GUIDs (mod removed).** The saved type has no selector, so `ObjectFolderImpl::ReconSelector`
gives the placeholder (GUID 0x3C08965F); `ObjectModuleImpl::DoStream` reads the object's stream and
kills the placeholders, as the GameCube does for any missing type. The lot loads without the object; the
next save no longer has it. A tree frame owned by the missing type has no behaviour and is popped
(`TreeSimImpl::DoNodeAction`, GC).

**Development aids** (testing.md): `SIMMY_PLACE_TEST` `probe:<guid>@x,y,dir|...` (first legal spot,
nothing placed), `try:<guid>@...` (placed there), `info#<guid>` (an object's placement data);
`SIMMY_CAM_LOOK=x,y,distance,yaw,pitch[,height]` (the camera looks at a tile: screenshots).

## The converter (tools/pcobj/)

| File | What |
|---|---|
| `dbpf.py` | DBPF packages 1.x (index 7.0-7.2), the DIR compression directory, QFS/RefPack |
| `rcol.py` | Scenegraph resources: CRES (blocks and the transform hierarchy), SHPE, GMND (names, links), GMDC v4 (elements, links with alias lists, groups), TXMT, TXTR (v9), LIFO |
| `sg.py` | Name -> instance: CRC-24 (poly 0x864CFB, init 0xB704CE) of the lower-cased name \| 0xFF000000, instance-high CRC-32 (MSB first); packages searched newest pack first |
| `pcinstall.py` | Packs (order, `Objects` / `StuffPack/Objects` / `Catalog/Materials` / `Sims3D` or `3D` packages), the OBJD catalogue across packs (newest wins, home pack), CTSS text, STR# 0x85 models, parts, MMAT recolours (CPF) |
| `pcmesh.py` | CRES -> SHPE -> GMND -> GMDC meshes in object space (node transforms, shadows/bounding boxes left out, orphan mirror frames) |
| `gctex.py` | CMPR (DXT1 encoder, DXT1 -> CMPR transcode), RGB5A3, RGBA8, mips, TXFL |
| `gcdata.py` | GameCube data (folder or GCM image), the SimsObjects quickdata, EDataHeader |
| `pcobj_convert.py` | Inventory, placement, base choice, materials, MODL/shaders, thumbnail, mod.ini; batch with worker processes |

## Limits

- The copy behaves like its GameCube base (room score from the PC room rating, price, the base's
  interactions); PC behaviour (BHAVs: aquariums' fish, fountains' water, clocks, shrines) is not
  converted. Animated objects show their mesh in its bind pose; no particles, no animated water.
- Curtains are wall objects: they go on walls, not over windows (the GameCube's wall art may not
  overlap a window). Surface objects go on tables and counters only (not on the floor). Ceiling
  objects stand on a one-tile rug (no ceiling placement on the GameCube; not on top of another rug).
- Recolours: only the default (the inventory counts the others). Each would be another catalogue entry
  with its own shader/texture and a model copy (the MODL names its shaders): `--recolours` is not done.
- No LOD, no specular/environment maps (blended glass is flat), one texture per material.
- The fire-code meter counts the converted model's size like any model (float vertices: about 28 bytes
  per vertex; converted models average ~37 KB against ~12 KB for the disc's, a large PC object weighs
  100-300 KB), so converted objects fill a lot's meter about three times faster than disc objects (a
  test lot with 289 of them was well over); then buy mode refuses more, as on the GameCube. Fixed-point vertices (the disc's format 6, about half the size) need the
  model scale, which the game applies through the object's animation (character); converted objects
  have none, so the converter keeps float (`FIXED_POINT` in pcobj_convert.py).
