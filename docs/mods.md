# PC mod: mods (objects from the PC game)

Lets the player add objects converted from **their own** copy of The Sims 2 for PC: by default every
buy-mode **Decorative** and **Surfaces** object of the base game and of every expansion and stuff pack
installed (about 560 decorative objects and 170 counters, tables and desks with the Ultimate
Collection). Surfaces work as surfaces: things go on their tops at the PC height, sims prepare food on
the counters, eat at the tables and use computers on the desks, and counters join up in rows and
corners. Off unless a mod folder exists; with none, nothing in the game changes
(no extra memory, same determinism hash).

**Legal rule.** The PC game's files are EA's. Nothing converted from them goes into this repository,
its releases or its docs. The converter reads the user's install on the user's machine and writes into
the user's mod folder; it refuses to write inside the source tree.

## Using it

On Windows the quick way is `tools\windows\convert_pc_object.bat` (copy it next to `build_simmy.bat`).
Its PC install and disc paths are set at the top of the file.

| | |
|---|---|
| `convert_pc_object.bat` | every Decorative and Surfaces object of every pack under the install, into `mods` next to simmy.exe (progress per object; about 70 s for every pack of the Ultimate Collection on two cores) |
| `convert_pc_object.bat decorative` / `surfaces` | only that buy-mode category |
| `convert_pc_object.bat list ficus` | PC objects whose name contains "ficus" |
| `convert_pc_object.bat inventory` | only the inventory (`mods\pcobj_inventory.tsv`) |
| `convert_pc_object.bat "No-Fuss Ficus"` | one object (name or GUID), base chosen automatically |
| `convert_pc_object.bat "No-Fuss Ficus" plant_floor_rubbertree` | one object with this GameCube base |

By hand (Python 3 with numpy and Pillow, nothing else):

```
python tools/pcobj/pcobj_convert.py --pc "C:\Games\The Sims 2 Ultimate Collection" \
    --gc D:\Games\Sims2.iso --out C:\Games\SIMMY\mods --categories decorative,surfaces
```

`--pc` is the install folder (every `TSData/Res` under it is a pack); `--gc` the GameCube data the port
plays (disc image or extracted folder). `--categories decorative,surfaces` (or `all`; also
`--all-decorative`, `--all-surfaces`) picks the buy-mode categories. Other modes: `--object <GUID|name> [--base <GameCube object>]`,
`--batch list.tsv` (GUIDs in the first column, an optional base in the second), `--list <text>`,
`--inventory <file.tsv>` (classify only, write nothing else). Options: `--packs base,ep1,sp4,...` (by
home pack), `--jobs N` (worker processes; default CPUs - 1), `--texsize` (largest texture side, 256).
Converting again replaces the folders it wrote; `mods/pcdeco_inventory.tsv` and
`mods/pcsurf_inventory.tsv` keep one row per object (converted or skipped, and why).

Start the game. The mod folder is `mods=` in simmy.ini (relative to the ini), else `SIMMY_MODS`, else
`mods` next to the exe. The log says `[mods] <dir>: n mod(s), n object(s) (n multi-tile parts), n
resource(s); read in n ms` and `[mods] objects: n added (n multi-tile), n skipped; rows ...` (with
`SIMMY_MODS_VERBOSE=1`, or 20 objects or fewer, a line per object; a skipped object always says why).
The objects are in buy mode on the **Decorative** and **Surfaces** tabs with the PC name, description,
price and room rating and a rendered thumbnail. With a mod folder the **C stick** left/right pages that catalogue
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

## Surfaces

**Inventory.** Lead OBJDs with the catalogue function Surfaces (word 0x28 bit 0x02); kind from the
sub-sort (word 0x5E: 1 counters, 2 dining tables, 4 end tables, 8 desks, 0x10 coffee tables, 0x20
displays, 0x80 misc) and the semi-global (CounterGlobals, TableEnd_Globals, ...): counter, island
(a counter whose object name says so), dining, end, coffee, desk, display, misc (vanities, the tea and
food-judging tables), wall shelf. The "No Doors" counters are catalogue objects of their own (same
name: "(no doors)" is added). 173 with the Ultimate Collection: 168 converted; left out, with the
reason in the inventory: four wall-mounted shelves (the GameCube has no surface on a wall) and the
Magisplay tray (a surface that goes on a counter: the GameCube cannot stack surfaces).

**The GameCube's surfaces.** All its tables (dining 2x1 to 4x2, end tables, desks, the alien utility
table) run one semi-global, `table_sg` (eat here: a sim with a plate sits on a chair facing a table
part and puts the plate in that part's slot; computers, lamps, decorations go on them), and all its
counters `food_counter_sg` (sims prepare food on them on the way from the fridge to the stove, serve
there; sinks drop into them: `ISimsCounterTopObject` swaps in a cut-out). Every part of a table has the
same object slots (one in the middle, four 1/4 tile round it, height kind 3 "table"); a counter two
(middle and 1/8 tile back, kind 4 "counter"); both at the GameCube's one surface height, 1.0 tile
(altitude 1.633). Its counters do not change with their neighbours (their states are clean and dirty).
The base is picked by footprint so a group has the same shape of parts:

| PC kind, footprint | GameCube base |
|---|---|
| counter, island (1x1) | `counter_cheap1` |
| any table 1x1 (end, coffee, small dining, display, ...) | `table_end_plastic_1x1` |
| desk 2x1 | `table_desk_normal_2x1` (group) |
| other tables 2x1, 3x1 | `table_dining_groovy_2x2` (a 2x1 group; a 3x1 gets three parts) |
| 2x2, 3x3 | `table_dining_cheap_red_2x2` (group) |

**The top's height.** The PC keeps where things go in its SLOT resources, but their heights are in the
PC's own units and its standard heights; the converter reads the top off the mesh instead: per tile,
the levels of the upward-facing triangles over it with a fair share of the tile, the highest one a
surface of its kind has (a desk's writing top under its hutch, a counter's top under its splash back).
Counters come out at 1.0 (as the GameCube's), dining and end tables 0.8, coffee tables 0.5. mod.ini
`surface = 0.80` (and per tile in `tiles`, `x,y:GUID:height`); the game gives the base's object slots
that height (`ObjectSlotDescriptor`: a copy of the base's descriptor with the altitude, marked so
`ObjectSlotHeight` keeps it for the slot's kind), so plates, food, lamps, computers and decorations
(the GameCube's and converted ones) sit on the PC top, and the slot's kind still says what may go there.
Positions are the base's (middle and 1/4 tile round it).

**Counter pieces.** A PC counter's STR# lists its pieces (straight, corner, no-doors, ...; islands:
middle, the two ends, corner) and its counter globals pick one by the neighbours. The converter
makes the straight piece (the no-doors one for a "No Doors" counter) the model and the others extra
models (`connect = counter|island`, `model_corner`, `model_end_px` / `model_end_nx`: the end closing
the model's +x / -x side, `model_ends`: both ends, the second end's own triangles added to the first).
`ISimsCounterTopObject::SetObjOrient` (when the house is placed again: after every buy-mode placement
and at load) picks the piece: a counter in front of it facing along its side makes it a corner (drawn a
quarter turned for the other hand), else the sides with no counter next to them get end pieces. Any
counter top counts as a neighbour (the GameCube's too). A sink in a converted counter keeps the plain
piece and no cut-out (the GameCube's table of cut-outs only knows its own counters).

## Mod folder

```
mods/
  pcdeco_inventory.tsv      every decorative object found: GUID, name, pack, sub-sort, footprint,
                            placement, base, price, room, BHAVs, model, textures, recolours, status, ...
  pcsurf_inventory.tsv      every surface: the same, the kind as placement, the top's height, the pieces
  pc3_nofussficus_4cc4fd6c/ "pc<sort>_<name>_<GUID>" (sort: 1 plants, 2 sculptures, 3 wall art,
    mod.ini                 4 mirrors, 5 rugs, 6 curtains, 7 misc; surfaces "pcs<k>_": 1 counters,
                            2 islands, 3 dining, 4 desks, 5 coffee, 6 end tables, 7 displays/misc)
    models_96DA08B7.res     resources in the GameCube's own formats, as an archive holds them
    shaders_1FBB3FA3.res    ("<manager>_<id>.res": models, shaders, textures, ... any manager)
    textures_3A540C0D.res
    thumbnail.png           (for the user; not read)
```

mod.ini `[object]` keys: `guid`, `base` (object file) or `base_guid`, `name`, `desc`, `price`, `model`,
`thumbnail`, `functions` (catalogue function flags: the tab), `room` (catalogue room rating), `tiles`
(`x,y:GUID[:height] ...`: a multi-tile object), `surface` (a surface's top, tiles), `connect`,
`model_corner`, `model_end_px`, `model_end_nx`, `model_ends` (counter pieces), `pack` (informational). Mods are read in folder-name order; a GUID
or resource id given twice keeps the first. A resource id the disc has is never taken from a mod.

## How it works

| Piece | Where | What |
|---|---|---|
| Mod folder | `src/platform/pc_mods.cpp` (`simmy::mods_init`, `mods_objects`, `mods_resource`, `mods_tail_bytes`) | Read once at start-up (after `settings_load`), immutable afterwards (house loads run on a thread). Only the mod.ini files and the resource file **names** are read (~25 ms for 558 mods); a resource is read from its file each time the game loads it, so nothing of a mod sits in memory until the game uses it (thumbnails when the catalogue shows them, models when an object is in the house) |
| Resources | `EResourceLoaderImpl::DoLoadResource` (e_resloader.cpp) | An id the archive index lacks: the mod's bytes (read by `mods_resource`) through a memory `EFile`, loaded by the manager's own loader (`ERModel::LoadModel`, `ERShader`, `ERTexture`: CMPR 0x81 and RGB5A3 0x82 with mips are the disc's formats) |
| Object rows | `ObjectFolderImpl::AddModObjects` (src/game/modobjects.cpp), end of `LoadDatabase` | A copy of the base's `ObjDefinition` (GUID +0x1C, price +0x24, functions +0x50, room rating +0xB0), its ResData (thumbnail +0x1C, no animated character +0x20) and its ObjAnimDef table (the mod's model in every state; animation, particles, shader swaps dropped). The copies live in the "SimsObjects" image's **mod tail** (`ERQuickdata::ModTail`, `ModTailSize` = `simmy::mods_tail_bytes()`: sized from the mods' objects and tiles, ~1 MB for 558; only that image, only with a mod folder) so their self-relative pointers work like the originals'. The selector shares the base's `ResFile` row: trees, tree tables, routing slots, semi-globals |
| Multi-tile groups | the same | A lead (sub index -1, the catalogue entry; never in the world) and one part per tile (sub index x << 8 \| y), all with a master id of their own (from 0x100 up, per object file) so `GetNthSubSelector` / `GetMasterSelector` find them. From a single-tile base every part is a copy of the base (as the GameCube's two-tile paintings are copies of one-tile ones); from a group base (a rug) the lead is its lead and the parts its parts. The first tile draws the model (room rating on it only); the others have no ResData. Placement, the group's offsets and the drawing at the group's middle are the GameCube's own (`MakeNewOutOfWorldObject`, `PlacementGroup`, `MultiTileGroupCenter`) |
| Catalogue | `ObjSelector::GetCatalogName/Description/ShortName` (objselector_catalog.cpp), `PCTTarget::PageWithCStick` (pcttarget.cpp) | A mod object's own text (`m_modNameSlot`, `m_modDescSlot`); the cell list is a vector (no limit); the C stick pages four columns |
| Surfaces | `ObjectSlotDescriptor` / `ObjectSlotHeight` (xrouting.cpp), `ModCounterPiece` / `ISimsCounterTopObject::SetObjOrient` (isimsobjectmodel.cpp) | The selector's `m_modSurface` (per part) and `m_modObject`: object slots at the mod's top; counter pieces by neighbours (`SIMMY_COUNTER_TRACE=1` logs the choice) |
| Fire code | `EIObjectMan::CountObjects` (iobjectman.cpp) | A mod object's model counts at 5/7 of its size: what it would weigh with the disc's fixed-point vertices (16 bytes a vertex against 28) |
| Saves | unchanged | `objt` names types by GUID; a mod object (and each part) saves and loads like any other |

**Unknown GUIDs (mod removed).** The saved type has no selector, so `ObjectFolderImpl::ReconSelector`
gives the placeholder (GUID 0x3C08965F); `ObjectModuleImpl::DoStream` reads the object's stream and
kills the placeholders, as the GameCube does for any missing type. The lot loads without the object; the
next save no longer has it. Things that stood on a removed surface are on the floor. A tree frame owned by the missing type has no behaviour and is popped
(`TreeSimImpl::DoNodeAction`, GC).

**Development aids** (testing.md): `SIMMY_PLACE_TEST` `probe:<guid>@x,y,dir|...` (first legal spot,
nothing placed), `try:<guid>@...` (placed there), `info#<guid>` (an object's placement data),
`sell#<id>` (an object sold, e.g. to put a converted table where the lot's own stood; note that an object
file name in a place action may name a mod object, which copies the file: use GUIDs);
`SIMMY_COUNTER_TRACE=1` (counter pieces); `SIMMY_SLOT_TRACE=1` (slot heights);
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
| `pcsurf.py` | Surfaces: kinds, bases by footprint, counter pieces, the top's height from the mesh |
| `pcobj_convert.py` | Inventory, placement, base choice, materials, MODL/shaders, thumbnail, mod.ini; batch with worker processes |

## Limits

- The copy behaves like its GameCube base (room score from the PC room rating, price, the base's
  interactions); PC behaviour (BHAVs: aquariums' fish, fountains' water, clocks, shrines) is not
  converted. Animated objects show their mesh in its bind pose; no particles, no animated water.
- Surfaces behave as the GameCube's (eat, prepare food, things on them); PC extras are not
  converted (vanities' make-up, the food-judging table, displays' shop use, the tea table's ceremony).
  The GameCube's eat and computer animations are made for its 1.0 tables: at a PC table (0.8) plates
  and keyboards sit at the PC top while the sim's hands keep the GameCube's height. Counter pieces only
  for straight rows, corners and ends (no outside corners: the PC has none either; diagonal counters
  show the straight piece). Wall shelves and the counter tray are left out.
- Curtains are wall objects: they go on walls, not over windows (the GameCube's wall art may not
  overlap a window). Surface objects go on tables and counters only (not on the floor). Ceiling
  objects stand on a one-tile rug (no ceiling placement on the GameCube; not on top of another rug).
- Recolours: only the default (the inventory counts the others). Each would be another catalogue entry
  with its own shader/texture and a model copy (the MODL names its shaders): `--recolours` is not done.
- No LOD, no specular/environment maps (blended glass is flat), one texture per material.
- The fire-code meter counts a converted model at 5/7 of its size (what it would weigh in fixed point);
  PC meshes have more triangles than the disc's (decorative models ~37 KB, surfaces ~53 KB against
  ~12 KB for the disc's), so converted objects still fill a lot's meter two to three times faster
  than disc objects (a test lot with 22 converted surface models: 602 -> 1506 KB of models); then buy
  mode refuses more, as on the GameCube (`fire_code_limit=off` in simmy.ini turns the limit off). Fixed-point vertices (the disc's format 6, about half the size) need the
  model scale, which the game applies through the object's animation (character); converted objects
  have none, so the converter keeps float (`FIXED_POINT` in pcobj_convert.py).
