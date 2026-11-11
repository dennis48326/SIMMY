# PC mod: mods (objects from the PC game)

Lets the player add objects converted from **their own** copy of The Sims 2 for PC: by default every
buy-mode **Decorative**, **Surfaces** and **Seating** object and every **bed** of the base game and of
every expansion and stuff pack installed (about 560 decorative objects, 170 counters, tables and desks,
and 180 chairs, sofas, benches, stools and beds with the Ultimate Collection). Surfaces work as
surfaces: things go on their tops at the PC height, sims prepare food on the counters, eat at the
tables and use computers on the desks, and counters join up in rows and corners. Seating and beds
work as the GameCube's: sims sit on them (at the PC seat's height), pull dining chairs up to tables and
eat, watch TV from sofas, sleep, relax and make the bed (made / slept-in models), two to a double bed. Off unless a mod folder exists; with none, nothing in the game changes
(no extra memory, same determinism hash).

**Legal rule.** The PC game's files are EA's. Nothing converted from them goes into this repository,
its releases or its docs. The converter reads the user's install on the user's machine and writes into
the user's mod folder; it refuses to write inside the source tree.

## Using it

On Windows the quick way is `tools\windows\convert_pc_object.bat` (copy it next to `build_simmy.bat`).
Its PC install and disc paths are set at the top of the file.

| | |
|---|---|
| `convert_pc_object.bat` | every Decorative, Surfaces and Seating object (beds included) of every pack under the install, into `mods` next to simmy.exe (progress per object with the percentage and time; about 2 minutes for every pack of the Ultimate Collection on two cores) |
| `convert_pc_object.bat decorative` / `surfaces` / `seating` | only that buy-mode category (several may be given: `decorative seating`) |
| `convert_pc_object.bat list ficus` | PC objects whose name contains "ficus" |
| `convert_pc_object.bat inventory` | only the inventory (`mods\pcobj_inventory.tsv`) |
| `convert_pc_object.bat "No-Fuss Ficus"` | one object (name or GUID), base chosen automatically |
| `convert_pc_object.bat "No-Fuss Ficus" plant_floor_rubbertree` | one object with this GameCube base |

By hand (Python 3 with numpy and Pillow, nothing else):

```
python tools/pcobj/pcobj_convert.py --pc "C:\Games\The Sims 2 Ultimate Collection" \
    --gc D:\Games\Sims2.iso --out C:\Games\SIMMY\mods --categories decorative,surfaces,seating
```

`--pc` is the install folder (every `TSData/Res` under it is a pack); `--gc` the GameCube data the port
plays (disc image or extracted folder). `--categories decorative,surfaces,seating` (or `all`; also
`--all-decorative`, `--all-surfaces`, `--all-seating`) picks the buy-mode categories. Other modes: `--object <GUID|name> [--base <GameCube object>]`,
`--batch list.tsv` (GUIDs in the first column, an optional base in the second), `--list <text>`,
`--inventory <file.tsv>` (classify only, write nothing else). Options: `--packs base,ep1,sp4,...` (by
home pack), `--jobs N` (worker processes; default CPUs - 1), `--texsize` (largest texture side, 256).
Converting again replaces the folders it wrote; `mods/pcdeco_inventory.tsv`,
`mods/pcsurf_inventory.tsv` and `mods/pcseat_inventory.tsv` keep one row per object (converted or
skipped, and why).

Start the game. The mod folder is `mods=` in simmy.ini (relative to the ini), else `SIMMY_MODS`, else
`mods` next to the exe. The log says `[mods] <dir>: n mod(s), n object(s) (n multi-tile parts), n
resource(s); read in n ms` and `[mods] objects: n added (n multi-tile), n skipped; rows ...` (with
`SIMMY_MODS_VERBOSE=1`, or 20 objects or fewer, a line per object; a skipped object always says why).
The objects are in buy mode on the **Decorative**, **Surfaces** and **Seating & Beds** tabs with the PC name, description,
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

## Seating and beds

**Inventory.** Lead OBJDs with the catalogue function Seating (word 0x28 bit 0x01; the PC keeps its
beds there too); kind from the sub-sort (word 0x5E: 1 dining chairs, 2 living chairs, 4 sofas, 8 beds,
0x10 recliners and lounge chairs, 0x80 misc: bar stools, benches, ...), the semi-global
(ChairGlobals, SofaGlobals, BedGlobals) and the footprint. 187 with the Ultimate Collection: 181
converted; left out, with the reason in the inventory: the vampires' coffin (Nightlife), the Murphy
bed (folds out of a wall cabinet, 2x4), the two hammocks and the tent (Bon Voyage), the makeover
station (Open for Business): the GameCube has nothing they could be. Cribs and pet beds are not
Seating on the PC (General: Kids, Pets) and the GameCube has no babies or pets: not converted.

**The GameCube's seating.** All its chairs (dining, armchairs, the office chair) run
`seating_chair_sg`: sit, and a dining chair is pulled in to a table to eat (its state 1, "in": the chair
slides 0.24 tiles toward the table with the sim). Sofas, loveseats and the park bench run
`seating_sofa_sg`: a seat per tile (a 3x1 sofa seats three), sit, watch TV; its beds `bed_sg`: sleep,
relax, make the bed; a double has a side per sim (cuddle, woohoo). A sim in a seat is in the seat's
object slot (height kind 7, altitude 0): the GameCube's sit animation lifts the pelvis to 0.67 (its
seats are all 0.56-0.59), its sleep animation lies at 0.55 (mattress about 0.46); its beds are a row
of three parts (single: sub (0,1), (1,1) draws, (2,1)) or three by two (double: (1,1) draws the right
half, (1,0) the left; each half has its own state: made, unmade, getting in, sleeping, ...). How much
comfort sitting gives, or energy sleeping, is the base's (its trees' constants: a converted sofa rated
6 or 10 on the 7 sofa gives the 7 sofa's), so the base is picked by footprint and the PC rating:

| PC kind | GameCube base (by the PC's comfort; beds by energy 1..8) |
|---|---|
| dining chair, bar stool, booth | `seating_chair_dining_plain` (3; comfort <= 4), `..._dining_modern` (6; <= 7), `..._dining_expensive` (10) |
| armchair, sectional piece, recliner, lounge chair | `seating_chair_comfy_modern` (6; <= 7), `seating_chair_comfy_expensive` (10) |
| sofa 3x1 | `seating_sofa_worn_3x1` (5; <= 5), `seating_sofa_modern_3x1` (7; <= 8), `seating_sofa_expensive_3x1` (10) |
| loveseat 2x1, wide armchair 2x1 | `seating_sofa_loveseat_groovy` (7; <= 8), `seating_sofa_loveseat_romantic` (10) |
| bench 2x1 | `seating_bench_public` |
| single bed (1x3) | `bed_single_cot` (energy <= 2), `bed_single_groovy` |
| double bed (2x3) | `bed_double_groovy` (<= 3), `bed_double_modern` (<= 5), `bed_double_japanese` |

Sectionals and the booth use their single piece (arms both sides). Recliners and lounge chairs are
1x2 on the PC; the GameCube's chairs are one tile and their trees do not work as a part of a group
(tried: "Sit" stops with TREEASSERT 37), so they are 1x1 chairs: a recliner's folded mesh fits its
tile, a lounge chair's legs overhang the tile in front (nothing stops things being put there).
mod.ini `comfort` / `energy` give the catalogue the PC's ratings.

**Seat height.** The converter reads the seat off the mesh: the upward faces in the middle of the
seat (between the arms, in front of the back), the highest level with a fair share of the area
between 0.2 and 0.6 (a bar stool's to 1.0). PC seats are 0.42-0.51 (sofas, chairs), lounge chairs
0.33, bar stools 0.70-0.72. mod.ini `seat_offset` = PC seat - 0.57 (the GameCube's), kept between
-0.12 and +0.2 (feet on the floor; a stool's sitter's feet off it). `SAnimator2::positionCharacter`
draws a sim whose container is such a seat (or a part of it) that much higher, blended by its pose
(none standing: pelvis 0.96 over its root; all of it sitting, 0.67, or lying), so the walk up and the
sit-down start and end on the floor; the pose's node matrices are moved with it (the controller keeps
a frame's matrices). Beds: the slept-in sheet is at 0.47 (offset +0.01).

**Models and states.** A dining chair (and a bar stool) gets a second model 0.24 tiles toward its
front for state 1 (pulled in: mod.ini `state_models = 1:0x...`). Beds are turned a quarter onto the
GameCube bed's long axis (PC: head at y 0, the bed along +y; GameCube: along model x) and take the
base's parts (mod.ini `base_parts = 1`: each tile copies the base part at the same sub index: its
slots, its side); `tile_model = x,y:0xMODEL state:0xMODEL ...` gives the drawing parts their models:
made in state 0, slept in in every other state (getting in and out, sleeping, relaxing, making it).
The slept-in model is the PC mesh with the blanket's head half lowered to the sheet (the PC
animates it; the pillow stays), so a sleeping sim lies on the sheet, covered from the waist down; a
double's two halves are split at its middle (each side its own state). A second full-length
blanket some double beds have inside the first (the PC's turned-down sheet, showing through in the
bind pose) is left out.

**Bar stools at counters.** On the GameCube a sim eats only at a table (or sitting without one: the
plate in the lap; or standing). The food's "Eat" (food_carry_sg) asks for the best *eating surface*:
`TryFindFunctionalObject` function 7 scores every object whose tree table has entry 0x19 and whose
check tree passes by its data 0x26 (a table's: 65 cheap, 80-81 the dearer and end tables) less 3 per
tile of distance. Only tables have that entry (table_sg "Function - Eating Surface"): its test wants
the part's middle slot free and a chair on a side tile, in the same room, facing the part, free or the
eater's, with the part's slot on that side free ("Find adjacent good chair"), and the held food's data
4 flag 3. Its tree routes to the part with seat goals (the routing finds those chairs and runs the
chair's entry 0x1A "Function - Sit"), plays the put-down (misc animation 9) and drops the plate into
the slot on that side (1/4 tile from the middle, as high as the top); "Eat Sitting Down" then pulls the
chair in (global "Sit - Scoot - In": state 1, 0.24 tiles) and eats. With no table: "Do - Sit" (any
chair, plate in the lap), else "Eat Standing Up". A plate already on a table (a served meal, an
interrupted one) is eaten from the chair on its side (the table's named tree
`ct_chair_id_from_food_id`), else picked up and the search runs again. Counters (food_counter_sg)
have no entry 0x19.

A converted bar stool is a *counter seat*: a counter (any object with the prep and surface entries
0x12 and 0x14: every GameCube counter, and the converted counters and islands copied from
`counter_cheap1`) with a counter seat on one of its four side tiles, facing it, in its room, is an
eating surface too (score 65, the cheapest GameCube table's: the nearer spot wins at equal quality).
The game runs PC trees for it, written in the GameCube's own SimAntics bytecode in
`src/game/countereat.cpp` (ids 0x1F00-0x1F03, past every object file's own) and run like the disc's:
the test (holding food; a stool for this sim with its spot free; the food flag), the eating surface
(sit on the stool through its own "Function - Sit", check again, the table's put-down, the plate into
the counter's *eating slot* on the stool's side), a stand-in for `ct_chair_id_from_food_id` (a plate in
an eating slot is eaten from its stool), and the counter's pickup for a plate in an eating slot (the
counter's own reaches from its front only, where the stool stands: the sim sits on the stool, as at a
table, and takes it). Primitive 0x60 (not used on the disc) finds and checks the stool. The eating
slots are the counter's middle slot moved 1/4 tile toward each side (indices 0x60 + side, past its
slot list: the counter's own trees, its slot count and the placement checks never see them; at the
counter's height, a converted counter's own top). A spot needs the counter's middle slot empty (no
appliance) and, on its front side, its prep slot too. Then everything else is the GameCube's: the
stool is pulled in (its state 1 model), the eat animation (the counter top is 1.0, as the GameCube's
tables), autonomy choosing to eat, "Serve" (the pot goes on a counter or table as before; the sims
grab plates and take them to eating spots, stools included), talking while eating, the dirty plate
cleared ("Throw Out" from the stool), getting up.

mod.ini `counter_seat = 1` makes a chair a counter seat (`0`: never); without the key a chair whose
`seat_offset` is 0.08 or more (a bar stool's height) is one. The converter writes it for every bar
stool (1) and dining chair (0). With no mod object that is a counter seat none of this runs: a game
without mods plays exactly as before (the determinism replay's hash is unchanged). Loading a save
without the mods while a sim sat on a stool: the stool is gone (as any missing object); the sim stands
up where it was and starts afresh (PC fix: before, it kept its sitting state and every route failed
until its behaviour was switched off); a plate left in a GameCube counter's eating slot stays there
and is cleared from the front. `SIMMY_COUNTER_EAT_TRACE=1` logs each stool search and check.

## Mod folder

```
mods/
  pcdeco_inventory.tsv      every decorative object found: GUID, name, pack, sub-sort, footprint,
                            placement, base, price, room, BHAVs, model, textures, recolours, status, ...
  pcsurf_inventory.tsv      every surface: the same, the kind as placement, the top's height, the pieces
  pcseat_inventory.tsv      every seat and bed: the kind, base, seat height, seat offset, ratings, seats
  pc3_nofussficus_4cc4fd6c/ "pc<sort>_<name>_<GUID>" (sort: 1 plants, 2 sculptures, 3 wall art,
    mod.ini                 4 mirrors, 5 rugs, 6 curtains, 7 misc; surfaces "pcs<k>_": 1 counters,
                            2 islands, 3 dining, 4 desks, 5 coffee, 6 end tables, 7 displays/misc;
                            seating "pcc<k>_": 1 dining chairs, 2 bar stools, 3 armchairs, recliners
                            and lounges, 4 sofas, loveseats, sectionals, 5 benches, 6 single beds,
                            7 double beds)
    models_96DA08B7.res     resources in the GameCube's own formats, as an archive holds them
    shaders_1FBB3FA3.res    ("<manager>_<id>.res": models, shaders, textures, ... any manager)
    textures_3A540C0D.res
    thumbnail.png           (for the user; not read)
```

mod.ini `[object]` keys: `guid`, `base` (object file) or `base_guid`, `name`, `desc`, `price`, `model`,
`thumbnail`, `functions` (catalogue function flags: the tab), `room` (catalogue room rating), `tiles`
(`x,y:GUID[:height] ...`: a multi-tile object), `surface` (a surface's top, tiles), `connect`,
`model_corner`, `model_end_px`, `model_end_nx`, `model_ends` (counter pieces), `seat_offset`,
`state_models`, `base_parts`, `tile_model` (repeatable, after `tiles`), `comfort`,
`energy`, `counter_seat` (seating and beds), `pack` (informational). Mods are read in folder-name order; a GUID
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
| Seating, beds | `SAnimator2::positionCharacter` / `ModSeatOffset` (sanimator2.cpp), `CopyDef` / `BasePart` (modobjects.cpp) | A sim in a converted seat or bed drawn at its seat's height (`SIMMY_SEAT_TRACE=1` logs container, state, root, pelvis and offset every 30 frames); per-state and per-part models, base parts by tile, ratings |
| Bar stools at counters | `src/game/countereat.cpp` (`simmy::countereat`), hooks in `TryFindFunctionalObject` / `TryCallFunctionalTree` / `TryCallNamedTree` / `TryElement` 0x60 (objectsim.cpp), `Behavior::GetTree` (treesim.cpp), `ObjectSlotDescriptor` (xrouting.cpp) | A counter with a counter seat facing it offers eating surface entry 0x19 through PC trees (SimAntics bytecode built in code, ids 0x1F00-0x1F03), its eating slots 0x60-0x63 and the pickup from them; nothing runs unless a loaded mod object is a counter seat |
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
`SIMMY_COUNTER_TRACE=1` (counter pieces); `SIMMY_COUNTER_EAT_TRACE=1` (bar stools at counters); `SIMMY_SLOT_TRACE=1` (slot heights); `SIMMY_SEAT_TRACE=1` (sims in seats and beds: offsets);
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
| `pcseat.py` | Seating and beds: kinds, bases by footprint and rating, the seat's height, the beds' made / slept-in models and halves |
| `pcobj_convert.py` | Inventory, placement, base choice, materials, MODL/shaders, thumbnail, mod.ini; batch with worker processes |

## Limits

- The copy behaves like its GameCube base (room score from the PC room rating, price, the base's
  interactions); PC behaviour (BHAVs: aquariums' fish, fountains' water, clocks, shrines) is not
  converted. Animated objects show their mesh in its bind pose; no particles, no animated water.
- Seating and beds behave as their GameCube base: no PC-only actions (recliners do not recline;
  bar stools are the exception: sims eat at counters on them, see "Bar stools at counters", but no bar
  drinks, and a sim does not eat standing at a counter; a stool pulled up to a table works too); comfort / energy gained are the base tier's. The sit and sleep animations are the
  GameCube's (a lounge chair is sat on upright; a sim on a PC chair leans its arms at the GameCube's
  table height). "Nap" on a sofa is the GameCube's own: its check refused it in our runs on the
  GameCube's sofa too. Beds have two static models (made, slept in) instead of the animated blanket.
  The coffin, Murphy bed, hammocks, tent and makeover station are left out; recolours are not
  converted.
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
