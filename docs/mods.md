# PC mod: mods (objects from the PC game)

Lets the player add objects converted from **their own** copy of The Sims 2 for PC: by default every
buy-mode **Decorative**, **Surfaces**, **Seating**, **Appliances**, **Lighting**, **Plumbing**, **Electronics**, **Hobbies**, **Storage** and **Kids** object and every **bed**
of the base game and of every expansion and stuff pack installed (about 560 decorative objects, 170
counters, tables and desks, 180 chairs, sofas, benches, stools and beds, 54 fridges, stoves, grills,
microwaves, toaster ovens, coffee makers, food processors, juicers, dishwashers, trash compactors,
food displays and food stands, 145 table, floor, wall and ceiling lamps, outdoor lights and
streetlamps, 36 toilets, showers, bathtubs, sinks and hot tubs, and 65 TVs, computers, stereos, speakers,
jukeboxes, DJ booths, arcade and pinball machines, phones, alarms, clocks and vending machines, and 42
bookcases, easels, instruments, telescopes, chess and pool tables, exercise machines and workbenches, and
45 dressers, armoires, lockers and closets, the coat rack, three remote-control toys and 17 toys and toy
boxes with the Ultimate Collection). Surfaces work as
surfaces: things go on their tops at the PC height, sims prepare food on the counters, eat at the
tables and use computers on the desks, and counters join up in rows and corners. Seating and beds
work as the GameCube's: sims sit on them (at the PC seat's height), pull dining chairs up to tables and
eat, watch TV from sofas, sleep, relax and make the bed (made / slept-in models), two to a double bed.
Appliances work as the GameCube's fridges, stoves, microwave, dishwasher, espresso machine, food
processor, blender and trash can (snacks, cooking, prep steps, drinks, washing up, fire), with the PC
model's own doors opening (fridge doors, oven doors, the dishwasher's door and rack). Lamps work as
the GameCube's: they light their room (the light grid, in the PC lamp's colour and reach) and its
light score, switch on by themselves at night and off by day, and show the PC lamp's lit look when on
(glowing shade and bulb, the pool of light it paints) and its unlit look when off; ceiling lamps hang
from the ceiling, inside only, with sims and furniture under them. Plumbing works as the GameCube's
toilets, showers, bathtubs, sinks and hot tubs (bladder, hygiene, comfort and fun; washing hands and
dishes; dirt, clogs, breaking and repairs), sims sitting, standing and lying in the PC model, with the
PC's dirty looks; counter sinks go into counters, converted ones too (which show a hole for them).
Electronics work as the GameCube's TVs, computers, stereos, DJ booth, arcade and pinball machines, game
console, phone, alarms and vending machines (watch TV and change channels, play, blog, design games, find a
job, dance, call, buy a drink), the PC TV or computer showing the GameCube's own screens (the channels, the
games, the blog, the job search) on its screen when on and the PC's switched-off screen when off.
Hobbies work as the GameCube's skill objects teaching the same skill (read, paint and sell the painting,
play the piano or guitar, play chess, stargaze, lift weights, run, kickbox, build and sell a rocket), the
sims at the PC object's bench, keys, eyepiece or treads, with the GameCube's canvas, chess pieces and rocket
on the PC object and its guitar, barbell, brush and book in their hands; the pool tables are the
GameCube's air hockey table. Dressers, armoires and lockers work as the GameCube's dresser and wardrobe
(Change Clothes: the sim steps up to the PC one and the outfit planner opens); the remote-control toys as
its game console (play, watch); toys and toy boxes are decorations (the GameCube has no children).
Off unless a mod folder exists; with none, nothing in the game changes
(no extra memory, same determinism hash).

**Legal rule.** The PC game's files are EA's. Nothing converted from them goes into this repository,
its releases or its docs. The converter reads the user's install on the user's machine and writes into
the user's mod folder; it refuses to write inside the source tree.

## Using it

On Windows the quick way is `tools\windows\convert_pc_object.bat` (copy it next to `build_simmy.bat`).
Its PC install and disc paths are set at the top of the file.

| | |
|---|---|
| `convert_pc_object.bat` | every Decorative, Surfaces, Seating (beds included), Appliances, Lighting, Plumbing, Electronics, Hobbies, Storage and Kids object of every pack under the install, into `mods` next to simmy.exe (progress per object with the percentage and time; about three minutes for every pack of the Ultimate Collection on two cores) |
| `convert_pc_object.bat decorative` / `surfaces` / `seating` / `appliances` / `lighting` / `plumbing` / `electronics` / `hobbies` / `storage` / `kids` | only that buy-mode category (several may be given: `decorative seating`) |
| `convert_pc_object.bat list ficus` | PC objects whose name contains "ficus" |
| `convert_pc_object.bat inventory` | only the inventory (`mods\pcobj_inventory.tsv`) |
| `convert_pc_object.bat "No-Fuss Ficus"` | one object (name or GUID), base chosen automatically |
| `convert_pc_object.bat "No-Fuss Ficus" plant_floor_rubbertree` | one object with this GameCube base |

By hand (Python 3 with numpy and Pillow, nothing else):

```
python tools/pcobj/pcobj_convert.py --pc "C:\Games\The Sims 2 Ultimate Collection" \
    --gc D:\Games\Sims2.iso --out C:\Games\SIMMY\mods --categories decorative,surfaces,seating,appliances,lighting,plumbing,electronics,hobbies,storage,kids
```

`--pc` is the install folder (every `TSData/Res` under it is a pack); `--gc` the GameCube data the port
plays (disc image or extracted folder). `--categories decorative,surfaces,seating,appliances,lighting,plumbing,electronics,hobbies,storage,kids`
(or `all`; also `--all-decorative`, `--all-surfaces`, `--all-seating`, `--all-appliances`,
`--all-lighting`, `--all-plumbing`, `--all-electronics`, `--all-hobbies`, `--all-storage`, `--all-kids`) picks the buy-mode
categories. Other modes: `--object <GUID|name> [--base <GameCube object>]`,
`--batch list.tsv` (GUIDs in the first column, an optional base in the second), `--list <text>`,
`--inventory <file.tsv>` (classify only, write nothing else). Options: `--packs base,ep1,sp4,...` (by
home pack), `--jobs N` (worker processes; default CPUs - 1), `--texsize` (largest texture side, 256).
Converting again replaces the folders it wrote; `mods/pcdeco_inventory.tsv`,
`mods/pcsurf_inventory.tsv`, `mods/pcseat_inventory.tsv`, `mods/pcappl_inventory.tsv`,
`mods/pclight_inventory.tsv`, `mods/pcplumb_inventory.tsv`, `mods/pcelec_inventory.tsv`, `mods/pchobby_inventory.tsv`,
`mods/pcstorage_inventory.tsv` and `mods/pckids_inventory.tsv` keep one row per object (converted or skipped, and why; `pc_tab` the PC
buy-mode tab of the object's catalogue function, `gc_tab` the GameCube tab it is put on).

Start the game. The mod folder is `mods=` in simmy.ini (relative to the ini), else `SIMMY_MODS`, else
`mods` next to the exe. The log says `[mods] <dir>: n mod(s), n object(s) (n multi-tile parts), n
resource(s); read in n ms` and `[mods] objects: n added (n multi-tile), n skipped; rows ...` (with
`SIMMY_MODS_VERBOSE=1`, or 20 objects or fewer, a line per object; a skipped object always says why).
The objects are in buy mode on the **Decorative**, **Surfaces**, **Seating & Beds**, **Appliances**, **Lighting**, **Plumbing**, **Electronics**, **Skill** and **Misc** tabs (hobbies, storage and kids: their base's tab) with the PC name, description,
price and room rating and a rendered thumbnail. With a mod folder the **C stick** left/right pages that catalogue
four columns at a time (the GameCube's movie only moves a column at a time; the tab can hold 600+).

**Screenshot mode** (`tools/windows/show_pc_objects.bat`): buy mode lists only the mods' objects
(`SIMMY_MODS_ONLY=1`; the GameCube's own objects stay in the game and on lots, just not for sale; tabs
without converted objects are empty). The bat also gives the game its own memory card
(`SIMMY_CARD_DIR=<folder>`: `pc_objects_card` next to simmy.exe, so the player's saves are never touched;
not in a network session), 999,999 simoleons when buy mode opens (`SIMMY_BUY_FUNDS`) and no fire-code
limit (`SIMMY_FIRE_CODE_LIMIT=off`, the same as `fire_code_limit=off` in simmy.ini for that run only).

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
| ceiling | `rug_outdoor_blanket` (group) | a one-tile rug: sims walk under it; inside only (`init_data = 0x1E:2`: the placement refuses the yard, error 10) |

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

## Appliances

**Inventory.** Lead OBJDs with the catalogue function Appliances (word 0x28 bit 0x04); kind from the
semi-global (FridgeGlobals, Stove_Globals, StoveGrill_Globals, MicrowaveGlobals, ToasterOvenGlobals,
CoffeeMachineGlobals, FoodProcessor_Globals, juicerglobals, DishwasherGlobals, TrashGlobals,
ShoppingRackGlobals, FoodStandGlobals): fridge, stove, grill, microwave, toaster, coffee, processor,
juicer, dishwasher, compactor, display (the shops' freezers and produce bins), food stand. 54 with the
Ultimate Collection, all converted (the PC has no washers or dryers: those are later games').

**The GameCube's appliances.** Its fridges (`refrigerator_cheap` / `_modern` / `_expensive`, semi-global
`food_refrigerator_sg`: snack, get ingredients, cook a meal, serve), stoves (`stove_cheap` / `_modern` /
`_expensive` / `_alien`), the grill (`stove_bbq`), the microwave (`microwave_expensive`: a stove on a
counter) and the food processor share `food_stove_sg` (cook; the cheap stove and a poor cook start
fires; repair); the blenders `food_blender_sg` (a prep step: blenderize), the espresso machine its own
trees (a drink), the kitchen trash can `trash_sg` (throw away, empty), and the dishwasher
`counter_dishwasher` is a two-tile counter group: the machine (a counter: prepare food, things on it;
wash dishes) and the tile in front its door opens over. They are animated: a character whose nodes the
state animations move (measured with `SIMMY_OBJ_NODE_TRACE`): the fridges' doors (node 1 hinged at model
-x, node 2 at +x, both swing about 87 degrees out in state 1 and back in state 3), the oven door (node 1,
about 90 degrees down in states 1 and 2: putting the pot in and taking it out), the microwave's door
(node 3, hinged at +x, state 3), the dishwasher's door (node 1) and lower rack (node 4, sliding out 0.7
tiles; states 1 and 3); effects (flames, smoke, steam) hang on the states.

**What each PC appliance becomes** (the base: behaviour, interactions, placement; tiers where the
GameCube differentiates: a fridge's food and a stove's cooking and fire risk are its base's):

| PC kind | GameCube base | |
|---|---|---|
| fridge, mini fridge | `refrigerator_cheap` (price < 1000), `_modern` (< 1600), `_expensive` | snack, ingredients, cook, serve; both doors open |
| stove, range, cooktop | `stove_cheap` (PC hunger 1), `stove_modern` (2-5), `stove_expensive` (6-10) | cook, fire, repair; the oven door opens |
| grill | `stove_bbq` | cook outdoors (the lid stays: the GameCube grill has none) |
| microwave | `microwave_expensive` | cook on a counter; the door opens |
| toaster oven | `microwave_expensive` | cooks as the microwave; its foot-hinged door opens down (node turn re-axised) |
| coffee maker, espresso machine, hot chocolate maker | `food_espresso_machine` | a drink (energy, fun) |
| food processor | `food_processor` | the prep step while cooking |
| juicer | `food_blender_expensive` | the blender's prep step |
| dishwasher | `counter_dishwasher` (group: machine + front tile) | washes dishes; a counter at the PC top (0.95); door and rack move |
| trash compactor | `trashcan_interior_stainless` | throw away, empty |
| food display (freezer, produce bin) | `refrigerator_cheap` | a food source (snacks, ingredients); the freezer's door opens |
| food stand (4x4) | `refrigerator_modern` | a food source on every tile |

Two- and more-tile appliances (grills 2x1, the restaurant cooktop, the freezer bin, the espresso cart,
the food stands) are groups whose every tile is a copy of the base (each tile works; the first draws
the model at the group's middle). The espresso cart stands on the floor (`init_data = 4:0x1`: the
heights its base, a counter-top machine, may be placed at, after its init tree).

**Moving parts.** The converter reads which bone each PC vertex hangs from (the GMDC's bone
assignments; newer objects hang each moving part's shape from a node of its own: `l_door`,
`door_rot`, `rack_trans`) and where the bone is (the hinge), and binds each part to the base's node
of the same role and side: a fridge's door hinged at -x to node 1, at +x to node 2, an oven door to the
stove's door node, the dishwasher's door and rack to nodes 1 and 4, a microwave's door to node 3. The
model the states show (`anim_model`) has a matrix record before each strip (the strip drawn with that
node's matrix); its vertices stay in the model's own space. `ISimsObjectModel::ModNodes` (after the
controller's Compute) draws each node by how far the base's animation has moved it from its rest pose
(state 0, seen for half a second: the closed doors), turned about the PC part's own hinge (`node =
N:x,y,z`), its slide kept. `:m` mirrors the turn (a door hinged on the other side than any of the
base's), `:+x` etc. applies the node's angle about another axis (a toaster oven's door hinged at its
foot on the microwave's side-hinged door node). Parts with no node on the base (grill lids, carafes,
an oven's rack, a compactor's drawer) stay in place. The catalogue's 3D preview shows the still model
(`model`). The base's state effects are kept (the base's skeleton is there to carry them).

## Lighting

**Inventory.** Lead OBJDs with the catalogue function Lighting (word 0x28 bit 0x80; all have the
semi-global LampGlobals); where it goes from the sub-sort (word 0x5E: 1 table, 2 floor, 4 wall,
8 ceiling, 0x10 outdoor, 0x80 misc: streetlamps, the studio umbrella, a bollard) and the mesh (a lamp
on a fence or deck post: its mesh starts above the ground at the tile's back edge -> wall; misc and
outdoor-only room sorts -> outdoor). 145 with the Ultimate Collection, all converted.

**The GameCube's lamps.** All share the semi-global `lamp_sg`: 8 table lamps, 11 floor lamps (plus the
boom lamp group and the dance-floor lights), 8 wall lamps and the street lamp (`scenery_lamppost`); no
ceiling lamps. Every lamp of one placement runs the same init tree ("SG_INIT table expensive",
"floor expensive tall", "wall cheap"; the street lamp the floor one and data 0x1E 1: outside only):
its light amount (attribute 0xB, the semi-global's constants), a light source (data 0x10 1, data 0x28
flags) that the room score counts, breakable, bulb burns out and is fixed (function entry 0x1D). The
lights are automatic ("AutoLights": the main tree switches a lamp on in the evening and off in the
morning (in our runs on from 19:00 to before 06:00), by the room's exterior windows and whether it is
day; a room change or load re-evaluates); the "Turn On" / "Turn Off" interactions exist but their
tree-table entries carry flag 0x80 (debug), so the GameCube offers no manual switch. Each lamp is an
animated object: a character with an "..._off" and an "..._on" node (each a copy of the shade; the
on one drawn with a shader without lighting, geometry modes 0, and vertex colours: it glows) and an
"Omni" node that carries the model's light (SimsLightInfo: a point light, colour, falloff 2-6 tiles;
the light grid gives it a reach of falloff / 2). State 0 is off, state 1 on (the state animations show
one shade node or the other and switch the light; ObjAnimDef +0x14 = 1).

**What each PC lamp becomes.**

| PC placement | GameCube base | |
|---|---|---|
| table | `lamp_table_modern` (off node 1, on node 2) | on tables, desks and counters (also converted ones: the PC top's height) |
| floor | `lamp_floor_modern` (on 1, off 2) | on the floor |
| wall | `lamp_wall_sconce` (off 1, on 2) | on walls, as wall art (the model 0.45 toward the wall) |
| ceiling | `lamp_floor_modern` + `init_data = 8:\|0x4 0x3F:0 0x1E:2` | zero extent (sims walk under it; a table, chairs or a rug may stand on its tile), no layer, inside only (the yard: error 10); the model's top at the top of the walls (3 tiles over its floor) |
| outdoor, streetlamp | `scenery_lamppost` | outside only |

**On and off.** Per PC subset the converter sorts the materials by the PC's lamp states (its MMAT
object states and material names: "..._lit" / "..._unlit"): a subset the PC switches between a lit and
an unlit material is drawn twice, lit on the on node, unlit on the off node; one the PC only shows lit
(glows, the pool of light it paints on the floor or the wall, flames, beams, an unlit variant that is a
`Null` material) on the on node only; the rest (the body, shadows) on the root node. A lit material the
PC draws glowing (additive, emissive 0.3 or more, lighting off) gets a shader without lighting
(geometry modes 0, as the GameCube's lit shades) and vertex colours (its PC diffuse plus emissive
colour: a warm pool of light; white for untextured ones). The still model (catalogue preview, the
thumbnail) is the lamp switched off. The model the states show (`anim_model`) carries one
SimsLightInfo copied from the base model (its node, flags, intensity: the base's animations switch it),
with the PC light's colour (brightest channel 1) and a falloff from its intensity (the base's falloff x
sqrt(intensity / 1.25), 2.5 to 6). `light = x,y,z:r,g,b:falloff` puts the light where the PC lamp's
light node is (the CRES's cLightRefNode, composed to the root; several PC lights -> their middle: a
neon sign's are one per recolour at one place, the first one's colour), in model space:
`ISimsObjectModel::AnimateLights` places it there with the object's orientation instead of the base
node's (which is the base lamp's bulb). The light grid (lights per room, `SIMMY_LIGHT_TRACE=1`) and the
room score take it as a GameCube lamp's.

**Multi-tile PC lamps** (wall lights 2x1 to 4x1, the Neon Wedding Tack, the M&G chandelier 2x2, the
Nightlife gobo spotlights and rotating ball 4x4 / 5x5) become one tile with the mesh's middle over it (a
group of lamp copies would be as many lamps, each switched and counted on its own); a wall lamp keeps its
back at the wall. Lamps on PC fence or deck posts stand on the ground (raised by the post's height; the
GameCube has no fence posts). The two-storey chandelier comes down to 1.9 at its lowest (it reaches into
the floor above).

## Plumbing

**Inventory.** Lead OBJDs with the catalogue function Plumbing (word 0x28 bit 0x10), and the two hot
tubs the PC files elsewhere (the Love Tub: an aspiration reward; Bon Voyage's hot spring: Hobbies): the
GameCube keeps every hot tub on its Plumbing tab. The kind from the semi-global (ToiletGlobals,
SinkGlobals, TubAndShowerGlobals, HottubGlobals) and the object name (stall, urinal, counter sink,
"ShowerTub" combinations, "Tub", "Shower"). 38 with the Ultimate Collection: 36 converted; left out
the urinal (the GameCube's toilet trees sit every sim down: women always, men for number 2) and the
Bon Voyage hot spring (6x6; the GameCube's largest hot tub is 3x3).

**The GameCube's plumbing.** Semi-globals `plumbing_toilet_sg`, `plumbing_shower_sg`,
`plumbing_bathtub_sg`, `plumbing_sink_sg`, `plumbing_hottub_sg`. All are animated objects (a character,
per-state animations and effects). Toilets: Use (number 1 / number 2; men stand for number 1, the seat
raised), Flush, Clean, Unclog, Puke, read / play while sitting; states 0/1 clean (seat down / up), 2/3
dirty, 4/5 clogged ("Sub - Set Graphic By State": dirty when data 0x27 >= 600, clogged when attribute
2 is set); tiers by constants: the cheap, industrial and public toilets (no comfort, clog chance 7),
the porcelain and ornate ones (comfort, clog chance 3). Showers (1x1; the outdoor shower 2x2): Take a
Shower, Clean, Repair, break; states 3-5 dirty; the hygiene rate is the base's (normal 227, sonic 243,
hi-tech 307); the water spray and steam are effects at the object (attachment bone 0xFF). Bathtubs
(2x1 groups: lead, a drawing part and a plain one): Take a Bath (fill, get in, luxuriate, drain),
Clean; 36 states (0-15 clean, 16-33 dirty), the water on its own node (lifted 0.39-0.47 while full),
comfort / hygiene by tier (regular 113/144, oriental 162/180, ornate 210/208). Sinks: Wash Hands, Wash
Dishes (function 0x18: the kitchen's washing up), Repair; floor (pedestal) and counter sinks; tiers
420 / 520. A counter sink stands on a counter's tile (ResData +7: a sink); the counter
(`ISimsCounterTopObject::SetObjOrient`) finds it there and draws its row of the "ECntrMdlLkupTable":
the top with a cut-out (hole x -0.46..0.46, y -0.43..0.28 of model space) as a sub-object at the sink's
place, and its cabinet; the sink's own model is the basin and faucet (in counter-top space). Hot tubs:
heart-shaped (2x2), Japanese (2x3: Get In, Join, Call Over, Repair, break), the anti-gravity spa (3x3,
modes to turn on first, its own GUID tested in its trees).

**What each PC object becomes.**

| PC kind | GameCube base | |
|---|---|---|
| toilet | `plumbing_toilet_cheap` (PC comfort 0-4), `plumbing_toilet_porcelain` (5-10) | the seat (the PC's `seat_rot` part) on the base's seat node, turning about the PC hinge; `seat_offset` from the PC seat (0.46) against the GameCube's |
| toilet stall | `plumbing_toilet_stall` (1x2 group, `base_parts`) | the PC stall (toilet, walls, door) over the two tiles: the back one the toilet, the front one the door |
| shower | `plumbing_shower_normal` (PC hygiene 0-9), `plumbing_shower_hitech` (10) | 1x1; spray, steam and the dirty flies are the base's effects |
| bathtub | `plumbing_bathtub_regular` (PC hygiene 0-5), `_oriental_2x1` (6-7), `_ornate_2x1` (8-10) | 2x1 groups (`base_parts`); the PC mesh turned half round (its faucet end where the base's is); the PC water spread over the tub (the bathtub template's water level) on the base's water node |
| shower / tub | the same bathtubs | a bathtub of the PC's footprint (the GameCube has no combination; its showers are one tile): sims take baths in it |
| sink (pedestal) | `plumbing_sink_floor_porcelain` (hygiene 0-5), `_floor_ornate` (6-10) | the PC's soapy water and soap ring left out (the base's running water effect) |
| counter sink | `plumbing_sink_counter_modern` (0-5), `_counter_double_basin` (6-10) | on a counter (GameCube or converted); in a GameCube counter its cut-out plus the sink's flange (`model_flange`); in a converted counter that counter's top with a hole (`sink_hole`); a GameCube counter sink in a converted counter: its own hole (`model_sink_gc`, `sink_shift`) |
| hot tub | `plumbing_hottub_japanese` (2x3, `base_parts`) | the 3x3 PC mesh narrowed to 2 tiles; a raised tub draws its sims at the PC water (`seat_offset` 0.905), a sunken one at the base's |

Every converted plumbing object keeps its base's character, state animations and effects
(`animated = 1`); `anim_model` is the PC mesh bound to the base's nodes, `model` a still copy (the
catalogue preview). Dirty and clogged looks are the PC's own materials (MMAT object states: each
"..._clean" material's "..._dirty", the toilet water's "toiletwater_water_clogged"), as models of the
base's dirty and clogged states (`state_models`). A material the PC shows only in another state by
default (a combination's "..._dirty" tub) is replaced by its clean one. Parts the PC moves that the
base has no node for (a shower's glass door, knobs, a hot tub's access door) stay still.

**Counter sinks and converted counters.** The PC's counter sinks are basins with a faucet and a
"..._null" plane (the PC's mask for the counter top under them); they go onto counters, as the
GameCube's do. A converted counter (surfaces) now also gets `model_sink` and `model_sink_wide`: its
own model with every flat face from its top down to 0.3 under it cut by a rectangle (narrow: x
-0.29..0.29, y -0.21..0.25 of PC space; wide: x -0.37..0.37, y -0.22..0.27; uvs and normals carried
along). With a sink on its tile it shows one of them (the converted sink's `sink_hole`, chosen from its
rim: the wide one when its rim covers it); the sink draws itself. A GameCube counter sink is wider
than the PC rims and its rim reaches 0.43 to the front, past a PC counter's top (0.72 deep, front edge
at y 0.29): a converted counter also gets `model_sink_gc`, a hole 0.03 inside the GameCube rim (x
-0.44..0.44), moved back by `sink_shift` (the top's front edge less 0.43, -0.2..0; -0.168 for most
counters, -0.043 for the deeper Archipelago island), cut down to 0.42 under the top and through
anything standing in that band (an island's curved apron, rails), as the GameCube's basins go 0.2
deep; the GameCube sink in it is drawn `sink_shift` back (ISimsObjectModel::ApplyMatrix, drawing
only: its slot, routing and sims are unchanged), its rim's front on the top's front edge. Converted
counters without `model_sink_gc` (converted before it) show the wide hole. In a GameCube
counter the cut-out is the GameCube's (larger than the PC rims), so a converted sink's `model_flange`
(a flat frame in its rim's colour from the rim out to that hole, at the top's height) is drawn with the
counter's top, as a second sub-object. Kitchen sinks wash dishes as the GameCube's sinks do (Wash
Dishes, function 0x18).

## Electronics

**Inventory.** Lead OBJDs with the catalogue function Electronics (word 0x28 bit 0x08; sub-sorts 1
entertainment, 2 TVs and computers, 4 audio, 8 small electronics, 0x80 misc), and the PC's massage
table (filed under Hobbies; the GameCube keeps its massage table on Electronics). The kind from the
semi-global (TelevisionGlobals, ComputerGlobals, StereoGlobals, ArcadeGlobals, PinballMachineGlobals,
VideoGame_Globals, PhoneGlobals, Clock_Alarm_Globals, ...) and the object name (wall TVs, wall and
community speakers, the jukebox, DJ booths, the karaoke machine, burglar and smoke alarms, wall clocks,
the public phone, vending machines). 75 with the Ultimate Collection: 65 converted; left out, with the
reason in the inventory: the three remote-controlled toys (car, classic car, helicopter: converted with
Kids, also in a run of every category, where Electronics finds 72), the karaoke machine (3x3; no singing on the GameCube), the three cash registers,
the ticket machine and the gadget kiosk (shop objects: the GameCube has no businesses) and the massage
table (the GameCube's is a machine whose robot arms massage, which the PC table has none of).

**The GameCube's electronics** (catalogue function 8). TVs `electronics_tv_cheap_stand` and
`electronics_tv_normal` (one tile, each a group of two parts on that tile) and `electronics_tv_plasma_1x3`
(2x1): turn on / off, watch (standing or from a seat), channels, repair. Their screen is one strip with the
shader "tv_screen_search" that every state replaces (ObjAnimDef +0x0C the shader replaced, +0x10 its
replacement: news, food, music, horror, each an 8-frame flipbook; switched off the dark "screen 00").
Computers `electronics_computer_cheap` / `_expensive` (blog, play, design a game, find a job, chat; at a
desk, from a chair facing it): the screen is a quad per node (blog, chat, design, three games, job, off,
shutdown, startup), each with its own shader; the state animations show one node and hide the others.
Stereos: `electronics_stereo_boombox2` (on tables), `electronics_stereo_expensive` and
`electronics_stereo_jukebox` (2x1), `electronics_stereo_djbooth` (3x2: play music, DJ, fire, fog,
laser): play music, dance, switch station. `electronics_arcade_regular` (its attract and in-use screens
on nodes), pinball machines (1x2), `electronics_game_console`, `electronics_phone_wall` (calls,
services, pranks: the caller stands under the phone), `electronics_alarm_burglar`,
`electronics_alarm_fire` (on walls), `electronics_alarm_clock`, `electronics_vending_drink` /
`_hotdog`. All are animated objects (a character, state animations, effects).

**What each PC object becomes.**

| PC kind | GameCube base | |
|---|---|---|
| TV, 1 tile | `electronics_tv_cheap_stand` (PC fun <= 6), `electronics_tv_normal` | the base's two parts on the tile (`base_parts`, the tile listed twice) |
| TV, 2 or 3 tiles | `electronics_tv_plasma_1x3` | 2x1 as the base's parts; the 3x1 home theatre a part per tile |
| wall TV | as the TVs + `init_data = 8:\|0x14 0xD:0x4` | a wall object (on a wall, not on the floor), the PC mesh at its PC height |
| computer | `electronics_computer_cheap` (price <= 1500), `_expensive` | on desks and tables; a monitor the PC raises by animation (lying flat in its bind pose) stood up |
| stereo 1x1 / 2x1 | `electronics_stereo_boombox2` / `electronics_stereo_expensive` | |
| wall / community speaker | `electronics_stereo_boombox2` + `init_data = 4:0x1 8:\|0x14 0xD:0x4` | a wall object |
| jukebox, DJ booth | `electronics_stereo_jukebox`, `electronics_stereo_djbooth` | the 2x2 PC booth in the middle of the 3x2 GameCube footprint |
| arcade machine | `electronics_arcade_regular` | the GameCube's screens on the PC screen |
| pinball machine | `game_pinball_machine` / `_ufo` / `_shark` (1x2) | |
| video game system | `electronics_game_console` | the PC mesh on the console's root (`BASE_INFO` body 0: its largest part, node 0x45, is the hologram its game shows, drawn only while playing; converted before this build the system was invisible when idle) |
| wall phone | `electronics_phone_wall` | the handset on the base's handset node |
| table phone | `electronics_phone_wall` + `wall_model = 0`, `init_data = 4:0x8C 8:&0xFFEB 0xD:0x0`, `route_front = 1` | on tables, desks and counters; the caller stands in front of it |
| public phone | `electronics_phone_wall` + `wall_model = 0`, `init_data = 8:&0xFFEB` | on the floor by a wall |
| burglar / smoke alarm, alarm clock | `electronics_alarm_burglar`, `electronics_alarm_fire`, `electronics_alarm_clock` | |
| wall clock | `art_wall_painting_01` | wall art (the GameCube has no wall clock), on the Electronics tab |
| drink / snack machine | `electronics_vending_drink` / `electronics_vending_hotdog` | |

**Screens.** The PC's screen is its subset "screen" (or a material named so). A TV's screen strip takes the
base's screen shader (the disc's "tv_screen_search", 0x0E96A4E8), its uvs laid over the PC screen's
bounding box as the base's quad lays them (an affine fit of the base quad's uvs to where each corner is
across and up its box, applied to the PC screen's box); `shader_swaps = 1` keeps the base's per-state
shader changes, so the TV shows the GameCube's channels, and `state_shaders = 0:0x.. 4:0x..` gives the
states that switch it off (those whose replacement is state 0's) the PC TV's own switched-off screen.
A computer's or arcade machine's PC screen is copied onto each of the base's screen nodes with that
node's GameCube shader and uvs, the switched-off node with the PC's own screen material (or, when the PC
material is its bare "screen" placeholder, the base's off quad); the base's state animations show the
right one. The base model's light (a TV's glow) goes in front of the PC screen (`light`). All other PC
parts are on the base's body node; a phone's handset on the base's handset node (turning about the PC
handset's middle as the base's moves).

**Engine support** (all inert without a mod that uses it): `shader_swaps` / `state_shaders` (CopyDef keeps
or replaces ObjAnimDef +0x0C / +0x10), a tile listed twice in `tiles` with `base_parts` takes the base's
second part on that tile (`BasePart`), `wall_model` (ResData +5: drawn as a wall or a floor object),
`route_front` (`ModRouteFront` in xrouting.cpp, from `TryGotoRoutingSlot` / `TryGotoRelative` and the
snap-to-slot primitive: a routing slot on the object's own tile moved to the tile in front, facing it:
the GameCube's caller stands under the wall phone, on a table phone's tile stands a table),
`same_type` (`ObjSelector::m_modBaseGuid`: SetToNext by GUID (kind 4) and TestObjectType find the mod
when they look for its base's GUID: the fire's "notify fire alarm" wakes a converted smoke alarm,
"Master - Set Ringing" rings converted phones).

## Hobbies

**Inventory.** Lead OBJDs with the catalogue function Hobbies (word 0x28 bit 0x100; sub-sorts 1 creative,
2 knowledge, 4 exercise, 8 recreation, 0x80 misc); the kind from the semi-global (EaselGlobals,
MusicGlobals, BookCaseGlobals, TelescopeGlobals, ExercisemachineGlobals, ChessGlobal, PoolTable_Globals,
CraftingGlobals, ...) and the object name (upright / grand piano, drum kit, bass, guitar, violin,
synthesizer, treadmill, exercise bike, ballet barre, the robot workbench). 72 with the Ultimate
Collection: 42 converted; the massage table and the hot spring are Hobbies on the PC but go with
Electronics and Plumbing (both left out there); the rest is left out with the reason in the inventory
(28: the dance sphere, the party game, bowling, log rolling, the three game tables for four, basketball,
soccer, four rinks, the dartboard and axe throwing, the sauna, five playground pieces, three street
performers' mats and podium, the train table, the car to restore, the open-mic stage and the secret door
bookcase). The PC's Entertainment
sub-sort (Electronics) has nothing left with a GameCube Skill-tab home: its arcade and pinball
machines and the game console went with Electronics, its remote-controlled toys stay for a toys
category (Kids, below).

**The GameCube's skill objects** (catalogue function 0x100, the Skill tab; each teaches one skill:
person data 0xa cooking, 0xc mechanical, 0xb charisma, 0x11 body, 0x12 logic, 0xf creativity, 9
cleaning): the bookshelves (`bookshelf_cheap` / `_expensive`, 2x1, against a wall: Read, Put Away),
`skill_creative_easel` (Paint; the painting grows on the canvas, Sell Painting), the electric guitar
(2x1: Play, Listen, Shoot Flame), the upright and player pianos (2x2: Play Classical / Experimental,
Listen), the chess tables (normal, alien: Play Chess, Join, Cheer), the telescopes (normal,
professional: Stargaze at night, Spy by day), `skill_mechanical_invention_bench` (2x1: Make Rocket,
Launch Rocket), `skill_physical_weight_bench` (2x2: Light / Medium / High Weightlifting),
`skill_body_hamsterwheel` (3x2: Run), `skill_body_punching_bag` (2x1: Kickbox), the bonsai, the
charisma mirror and the chicken checkers. Its games (air hockey 3x2, foosball, the driving range, the
trampoline, ...) are on the Misc tab (0x40). All are animated objects (a character, state animations,
effects); the parts its sims handle are either nodes of the object the state animations move or show (the
easel's canvas: eleven nodes, the blank canvas and the painting as it grows; the chess pieces; the invention
bench's rocket in its stages; the hamster wheel) or dress-up props the trees put on the sim (the guitar, the
barbell, brush and palette, a book, the rocket parts, the paddle and puck, and also the wheel round the
runner and the punching bag itself).

**What each PC object becomes.**

| PC kind | GameCube base | |
|---|---|---|
| bookcase | `bookshelf_cheap` (PC price < 700), `bookshelf_expensive` (700 and up) | against a wall; Read (the books: cooking, mechanical, cleaning); a 1x1 PC bookcase in the middle of the 2x1 bookshelf |
| easel | `skill_creative_easel` | Paint (creativity); the base's canvas and the painting growing on it over the PC easel; Sell Painting |
| crafting station (toy bench, flower station, pottery wheel, sewing machine) | `skill_creative_easel` (a copy per tile) | the PC teaches creativity at them: paint, the canvas on the PC station, sell the painting (the GameCube has no crafts) |
| robot workbench | `skill_mechanical_invention_bench` | Make Rocket (mechanical), the rocket built on the PC bench, the patent sold |
| upright piano | `skill_creative_piano_upright` | Play (creativity), the sim on the PC bench at the PC keys |
| grand piano | `skill_creative_player_piano` | the dearer PC piano on the dearer GameCube one (2x2: the 2x3 PC mesh overhangs) |
| drum kit | `skill_creative_piano_upright` | played sitting, as a piano (creativity; the GameCube has no drums) |
| guitar, bass, violin, synthesizer | `skill_creative_electric_guitar` | Play (creativity) with the base's guitar in the sim's hands, at the PC amplifier / stand |
| telescope | `skill_logic_telescope` (price < 1000), `_professional` | Spy, Stargaze (logic) |
| chess table | `skill_mental_chess_normal` | Play Chess, Join (logic); the base's pieces on the PC board |
| weight machine (multipress, leg press, butterfly, bench press) | `skill_physical_weight_bench` (2x2) | weightlifting (body), the base's barbell |
| treadmill, exercise bike | `skill_body_hamsterwheel` (3x2) | Run (body) on the PC treads (turned onto the wheel's axis; `hide_props`: no wheel round the runner) |
| ballet barre | `skill_body_punching_bag` (2x1) | Kickbox (body) at the PC barre |
| pool table | `game_air_hockey` (3x2, Misc tab) | Play: the GameCube's air hockey on the PC table (paddles, puck) |
| ant farm | `art_sculpture_wooden_model` (Decorative) | a table-top decoration |

Every converted hobby object keeps its base's character, states, effects and trees (`animated = 1`,
`same_type = 1`: the base's own GUID tests, the cheap telescope's and cheap bookshelf's skill rates, the air
hockey table's and hamster wheel's parts, answer as for the base; with `base_parts` each part answers as the base
part it copies). `anim_model` is the PC mesh on the base's body node plus the base's **props**: the sub-models of
the base's model on the nodes whose parts the PC mesh has no counterpart for (the easel's canvas and the painting
growing on it, nodes 2-12; the invention bench's rocket in its stages, 3-7; the chess pieces, 1-11), copied as
stored (the converter writes its own strips in that model's units: the anim model takes the base model's scale);
`prop_nodes = 2,3,...` makes the game draw those nodes where the base's animation has them
(`ISimsObjectModel::ModNodes` leaves their matrices to the controller), not by their turn from rest as the PC
parts' nodes. What a sim holds is a dress-up prop the base's trees put on the sim (`TryChangeSuit`: the guitar,
the barbell, brush and palette, a book, the rocket parts, the air hockey paddle and puck): they stay; the two that
are the base's own body, the spinning wheel round the hamster wheel's runner (`0x8C0E134F`) and the punching bag
the kicker hits (`0x0D2D87FB`), are left off on a treadmill, bike and barre (`hide_props`; a synthesizer's player
holds no guitar, `0xB31496D7`). The PC mesh is placed over the base's footprint: the tiles it covers (PC
footprints count the tile the sim stands on; the mesh is measured) centred in the base's, turned where the PC's
long axis is the other one (treadmill, exercise bike: the console in front of the runner), then moved by a
measured shift per kind (`SHIFT` in pchobby.py: the treadmill 0.35 so the runner is on the treads, a chess table
0.06 down so the base's pieces stand on the PC board). Multi-tile objects are the base's groups (`base_parts`, the
base's part per tile); a PC object smaller than its base takes the base's footprint (a 1x1 bookcase on the 2x1
bookshelf, the mesh in its
middle). The tab is the base's: Skill, Misc for the pool tables, Decorative for the ant farm.

## Storage and kids

**Inventory.** The PC's Miscellaneous tab (catalogue function 0x40): sub-sort 0x2 "Dressers" is Storage
(dressers, armoires, the gym lockers, the Apartment Life closets, the coat rack: 45 with the Ultimate
Collection), sub-sort 0x10 is Kids (cribs, high chairs, potty chairs, changing tables, toy boxes, teddy
bears, the dollhouse, toddler toys, the activity table, the spring rider, the lemonade stand: 26); Kids also
takes the three remote-control toys (PC Electronics, Entertainment), which Electronics leaves for it, also
in a run of every category. The kind from the semi-global (CribGlobals, HighChairGlobals, ToyBoxGlobals,
StuffedAnimal_Globals, DollHouseGlobals, ...) and the name (dresser, armoire, locker, closet, coat rack,
potty, changing table, ...). PC wall shelves are Surfaces (left out there); playground pieces, the train
table and the other recreation games are Hobbies (left out there, not converted here).

**The GameCube has no babies, toddlers or children**: no crib, high chair, potty, changing table or toy
box. Its Misc tab (0x40) has `transition_cas_dresser_groovy` (2x1) and `transition_cas_wardrobe` (1x1);
both have one interaction, Change Clothes: the sim walks up (the dresser: to its front; the wardrobe:
into it, a sparkle), the house is saved and the outfit planner (CAS, wardrobe mode) opens; Done brings
the sim back in the new clothes.

| PC kind | GameCube base | |
|---|---|---|
| dresser, armoire, closet (2 tiles wide) | `transition_cas_dresser_groovy` (2x1, Misc) | Change Clothes at the PC dresser; turned half round (the GameCube dresser's front is its model's +y) |
| small dresser or armoire, gym lockers (1 tile) | `transition_cas_wardrobe` (1x1, Misc) | Change Clothes; the PC mesh on the wardrobe's facade node (58, shown while idle) and its body node (55, shown in use) |
| remote-control car, classic car, helicopter | `electronics_game_console` (Electronics) | play (fun), Watch, CallOver: the GameCube's console game played at the PC toy (the Electronics path) |
| toy box, dollhouse, activity table, spring rider, coat rack | `art_sculpture_contemporary` (Decorative) | a floor decoration (only toddlers and children use them on the PC; nobody uses the coat rack) |
| teddy bear, toddler toys (xylophone, peg box, Wobbly Wabbit, toy oven) | `art_sculpture_wooden_model` (Decorative) | a decoration on tables and the floor (`init_data = 4:0x8D`: the base's table heights and the floor) |
| crib, high chair, potty chair, changing table, lemonade stand | - | left out: no babies, toddlers or children (and no shop) |
| Apartment Life 1-tile closets | - | left out: built into a wall like a door (as the secret-door bookcase) |

A dresser or wardrobe is a Hobbies-style object (`animated = 1`, `anim_model` on the base's body node,
`base_parts` for the dresser's two parts, `same_type = 1`); no new keys. The choice between dresser and
wardrobe is the PC mesh's width (both do the same), not its PC footprint, which counts the tile in front.
The PC doors and drawers stay shut (the GameCube dresser has no moving parts; the wardrobe's own doors are
not drawn).

## Mod folder

```
mods/
  pcdeco_inventory.tsv      every decorative object found: GUID, name, pack, sub-sort, footprint,
                            placement, base, price, room, BHAVs, model, textures, recolours, status, ...
  pcsurf_inventory.tsv      every surface: the same, the kind as placement, the top's height, the pieces
  pcseat_inventory.tsv      every seat and bed: the kind, base, seat height, seat offset, ratings, seats
  pcappl_inventory.tsv      every appliance: the kind, base, footprint, moving parts and their nodes
                            ("pieces": l_door>node2 ...), parts left still (notes)
  pclight_inventory.tsv     every lamp: placement, base, its parts by state ("pieces": body:1 switch:3
                            on:1 glow:4) and its light (place, colour, falloff, the PC intensity)
  pcplumb_inventory.tsv     every plumbing object: kind, base, seat / water height and offset, the
                            parts on nodes and the dirty / clogged states ("pieces": seat_rot>node3
                            dirty:2st clogged:2st), a counter sink's cut-out (notes)
  pcelec_inventory.tsv      every electronics object: kind, base, footprint, its screen (the base's screen
                            nodes or swap) and handset ("pieces": screen>nodes:2,3,..,9off handset>node1)
  pchobby_inventory.tsv     every hobbies object: kind, base, footprint, the body node, the base's props
                            ("pieces": props:2,3,.. body>node1)
  pcstorage_inventory.tsv   every storage object: kind (dresser, armoire, locker, closet; a decoration's
                            placement), base, the body node(s)
  pckids_inventory.tsv      every kids object and the remote-control toys: the same (skipped: why)
                            (every inventory: pc_tab / gc_tab, the PC and the GameCube buy-mode tab)
  pc3_nofussficus_4cc4fd6c/ "pc<sort>_<name>_<GUID>" (sort: 1 plants, 2 sculptures, 3 wall art,
    mod.ini                 4 mirrors, 5 rugs, 6 curtains, 7 misc; surfaces "pcs<k>_": 1 counters,
                            2 islands, 3 dining, 4 desks, 5 coffee, 6 end tables, 7 displays/misc;
                            seating "pcc<k>_": 1 dining chairs, 2 bar stools, 3 armchairs, recliners
                            and lounges, 4 sofas, loveseats, sectionals, 5 benches, 6 single beds,
                            7 double beds; appliances "pca<k>_": 1 fridges and food displays,
                            2 stoves, grills, food stands, 3 microwaves and toaster ovens,
                            4 coffee makers, processors, juicers, 5 dishwashers, compactors;
                            lighting "pcl<k>_": 1 table, 2 floor, 3 wall, 4 ceiling, 5 outdoor lamps;
                            plumbing "pcp<k>_": 1 toilets, 2 showers, 3 bathtubs and shower / tubs,
                            4 sinks, 5 hot tubs; electronics "pce<k>_": 1 TVs, 2 computers, 3 stereos,
                            speakers, jukeboxes, DJ booths, 4 arcade and pinball machines, game
                            systems, 5 phones, 6 alarms and clocks, 7 vending machines; hobbies
                            "pch<k>_": 1 easels and crafting stations, 2 instruments, 3 bookcases,
                            4 telescopes and chess tables, 5 exercise machines, 6 workbenches, 7 pool
                            tables, 8 the ant farm; storage and kids "pck<k>_": 1 dressers,
                            2 wardrobes and lockers, 3 the coat rack, 4 remote-control toys, 5 toy
                            boxes, 6 toys and teddy bears, 7 the activity table and spring rider)
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
`energy`, `counter_seat` (seating and beds), `animated`, `anim_base`, `anim_model`, `node` (repeatable),
`init_data` (`index:value`, `index:|bits` sets bits, `index:&bits` keeps only them; appliances, lamps,
ceiling objects), `light` (`x,y,z[:r,g,b[:falloff]]`: a lamp's light), `model_sink`, `model_sink_wide`, `model_sink_gc`
(a counter's top with a hole for a sink; the last for a GameCube sink), `sink_shift` (a counter: how far
along the PC's y a GameCube sink in it is drawn, -0.5..0.5), `sink_hole` (`narrow` / `wide`: a counter sink's), `model_flange`
(a counter sink's flange in a GameCube counter), `shader_swaps`, `state_shaders` (a TV's channels; its
own screen when off), `wall_model` (0 / 1), `route_front`, `same_type` (electronics, hobbies, storage), `prop_nodes` (`2,7,8`: nodes drawn where the base's animation has
them: a hobby object's props, sub-models copied from the base's model), `hide_props` (`0x8C0E134F ...`:
dress-up props of the base the sim does not wear on this object), `pack` (informational). Mods are read in folder-name order; a GUID
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
| Appliances | `CopyDef` (modobjects.cpp: the character and state table kept), `ISimsObjectModel::ModNodes` (isimsobjectmodel.cpp), `GetObjSelector3dDisplayInfo` (pcttarget.cpp), `cXObjectImpl::ApplyModInitData` (modobjects.cpp, from `Reset` / `cXMTObjectImpl::Reset`) | The base's skeleton, animations and effects; the bound model's nodes drawn by their turn from rest about the PC hinges; the still model in the catalogue preview; `init_data` after each init tree. `SIMMY_OBJ_NODE_TRACE`, `SIMMY_OBJ_STATE_TRACE`, `SIMMY_OBJ_STATE_FORCE` (testing.md) |
| Lamps | `CopyDef` (as appliances: the base's character and on/off state table), `ISimsObjectModel::AnimateLights` (isimsobjectmodel.cpp), `ApplyModInitData` (`\|` / `&` ops) | The model's light (its SimsLightInfo, created by `SetupCharacter` and switched by the state animations as a GameCube lamp's) at `light`'s place with its colour and falloff; the on / off parts in sub-models on the base's on / off nodes, hidden by the state's node flags (`ERModel::DrawHierarchical`); a ceiling lamp's zero extent, layer and inside-only data after its init tree |
| Plumbing | `CopyDef` (as appliances; up to 48 states: a bathtub has 36), `ModNodes`, `ModSeatOffset` (a sim in a toilet or a hot tub is its container's), `ISimsCounterTopObject::SetObjOrient` (isimsobjectmodel.cpp) | The base's skeleton, states and effects; the seat on its node; a converted counter with a sink on its tile shows its holed top (`model_sink` / `model_sink_wide` by the sink's `sink_hole`; a GameCube sink: `model_sink_gc`, drawn `sink_shift` back by `ISimsObjectModel::ApplyMatrix`), a GameCube counter with a converted sink adds the sink's flange (`model_flange`) as a second sub-object next to its cut-out top; `SIMMY_COUNTER_TRACE=1` logs the choice. `SIMMY_CENSOR_OFF=1` (testing.md) turns the mosaic off for screenshots |
| Electronics | `CopyDef` (as appliances; `shader_swaps` / `state_shaders`, `wall_model`), `BasePart` (a tile listed twice), `ModRouteFront` (xrouting.cpp; from `TryGotoRoutingSlot`, `TryGotoRelative`, the snap primitive), `TrySetToNext` kind 4 / `TryTestObjectType` (objectsim.cpp: `m_modBaseGuid`) | The base's screens on the PC screen (its shader swaps, its screen nodes); a table phone's caller in front of it; the base's GUID searches find same-type mods |
| Hobbies | `CopyDef` (as appliances), `ModNodes` (`prop_nodes`: those nodes keep the controller's matrices), `cXPersonImpl::TryChangeSuit` (person.cpp: `hide_props`), `ObjectFolderImpl::AddModObjects` (same_type: each part answers as the base part it copies) | The base's skill trees, states, effects and dress-up props; its canvas, chess pieces and rocket drawn on their nodes over the PC mesh; the wheel round the runner and the punching bag left off; the trees' own GUID tests (the cheap telescope's and bookshelf's skill rates, the air hockey table's and hamster wheel's parts) answered as the base |
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
`SIMMY_COUNTER_TRACE=1` (counter pieces); `SIMMY_OBJ_NODE_TRACE=<file text>` (an object's skeleton poses: appliances' rest pose and turns), `SIMMY_OBJ_STATE_TRACE=1` (state changes as drawn), `SIMMY_OBJ_STATE_FORCE=<id>:<state>:<from>-<to>` (an appliance drawn with its doors open); `SIMMY_COUNTER_EAT_TRACE=1` (bar stools at counters); `SIMMY_SLOT_TRACE=1` (slot heights); `SIMMY_SEAT_TRACE=1` (sims in seats and beds: offsets);
`SIMMY_CAM_LOOK=x,y,distance,yaw,pitch[,height]` (the camera looks at a tile: screenshots).

## The converter (tools/pcobj/)

| File | What |
|---|---|
| `dbpf.py` | DBPF packages 1.x (index 7.0-7.2), the DIR compression directory, QFS/RefPack |
| `rcol.py` | Scenegraph resources: CRES (blocks, the transform hierarchy, light nodes), point / spot light resources, SHPE, GMND (names, links), GMDC v4 (elements, links with alias lists, groups), TXMT, TXTR (v9), LIFO |
| `sg.py` | Name -> instance: CRC-24 (poly 0x864CFB, init 0xB704CE) of the lower-cased name \| 0xFF000000, instance-high CRC-32 (MSB first); packages searched newest pack first |
| `pcinstall.py` | Packs (order, `Objects` / `StuffPack/Objects` / `Catalog/Materials` / `Sims3D` or `3D` packages), the OBJD catalogue across packs (newest wins, home pack), CTSS text, STR# 0x85 models, parts, MMAT recolours (CPF) |
| `pcmesh.py` | CRES -> SHPE -> GMND -> GMDC meshes in object space (node transforms, shadows/bounding boxes left out, orphan mirror frames); optionally each vertex's bone or part node and the bones' places |
| `gctex.py` | CMPR (DXT1 encoder, DXT1 -> CMPR transcode), RGB5A3, RGBA8, mips, TXFL |
| `gcdata.py` | GameCube data (folder or GCM image), the SimsObjects quickdata, EDataHeader |
| `pcsurf.py` | Surfaces: kinds, bases by footprint, counter pieces, the top's height from the mesh |
| `pcappl.py` | Appliances: kinds, bases by tier, the base skeletons' moving nodes, binding the PC parts (bones / part nodes) to them |
| `pcseat.py` | Seating and beds: kinds, bases by footprint and rating, the seat's height, the beds' made / slept-in models and halves |
| `pclight.py` | Lighting: placement, bases, the materials by lamp state (lit / unlit / effect, glowing), the PC lights (CRES light nodes, light resources) -> one light record |
| `pcplumb.py` | Plumbing: kinds, bases by tier, the bases' footprints, seat / water nodes and dirty / clogged states, the seat and water heights, clean / dirty material swaps |
| `pchobby.py` | Hobbies: kinds, bases by function and price tier, the bases' groups, body nodes, props and props to leave off, the mesh over the base's footprint (centred, turned, shifted per kind) |
| `pckids.py` | Storage and kids: the inventory (Miscellaneous sub-sorts, the remote-control toys), kinds, the dresser / wardrobe by width, what is left out or made a decoration, the mesh over the base (turned, centred) |
| `pcelec.py` | Electronics: kinds, bases by footprint and tier, the bases' groups, placement changes (wall TVs, speakers, table phones), the screen subset, the base's screens (swap or nodes, uv fit), a monitor stood up |
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
- Appliances behave as their GameCube base: the PC's own extras are not converted (no espresso
  cart vendor, no food-stand vendor NPC, no juice drink, no hot chocolate, no compactor bag; the
  toaster oven cooks as the microwave, a juicer is a blender step, the food stands and shop displays
  are food sources); a fridge's food, a stove's cooking and fire risk are the base tier's. Parts with
  no node on the base stay still (grill lids, carafes, an oven's rack, compactor drawers, a food stand's
  pans); a door is turned by the base's animation (its timing and angle) about the PC hinge. The rest
  pose is taken the first time the object is seen in state 0 (half a second): an appliance loaded from a
  save in another state shows its parts closed until then. Every tile of a 2x1 grill or a food stand is
  a copy of the base (each tile offers the base's interactions; a food stand's back tiles cannot be
  reached). The dishwasher keeps the GameCube's two tiles (the PC's is one): the tile in front stays
  free for its door. Converted appliance models are 2-4 times the size of the disc's (the fire-code
  meter fills sooner).
- Surfaces behave as the GameCube's (eat, prepare food, things on them); PC extras are not
  converted (vanities' make-up, the food-judging table, displays' shop use, the tea table's ceremony).
  The GameCube's eat and computer animations are made for its 1.0 tables: at a PC table (0.8) plates
  and keyboards sit at the PC top while the sim's hands keep the GameCube's height. Counter pieces only
  for straight rows, corners and ends (no outside corners: the PC has none either; diagonal counters
  show the straight piece). Wall shelves and the counter tray are left out.
- Curtains are wall objects: they go on walls, not over windows (the GameCube's wall art may not
  overlap a window). Surface objects go on tables and counters only (not on the floor). Decorative
  ceiling objects are a one-tile rug (no ceiling placement on the GameCube; not on top of another rug),
  inside only; they hang at the PC model's height.
- Lamps behave as the GameCube's (`lamp_sg`): the lights switch themselves on in the evening and off in
  the morning, as the GameCube's lamps do; the GameCube offers no manual "Turn On" / "Turn Off" (its
  entries are debug-only), so neither do converted lamps; breaking and fixing the bulb are the base's.
  One light per lamp (several PC lights: their middle, the first's colour; a neon sign's recolours keep
  the default's light colour), a point light (PC spot lights, the gobos' beams, light as points); the
  GameCube lights rooms per vertex from the light grid (no shadows, no light patterns on walls beyond
  the PC model's own light-pool decals). Room light score: the base's light amount for its placement
  (the GameCube does not grade lamps). Multi-tile PC lamps are one tile; lamps on PC fence posts stand on
  the ground; the two-storey chandelier reaches through the ceiling. Recolours: the default only.
- Plumbing behaves as its GameCube base (comfort, hygiene, bladder and fun gains, clog and break
  chances are the base tier's). The urinal and the hot spring are left out; shower / tub combinations
  are bathtubs (no shower in them: the GameCube's showers are one tile, its groups one object file);
  the PC hot tubs (3x3) are the GameCube's Japanese hot tub (2x3: the PC mesh narrowed by a third). The
  shower's spray and steam come from where the base's shower head is, not the PC one's; PC moving
  parts the base has no node for (glass doors, knobs, a hot tub's access door) stay still. A counter
  sink's hole in a converted counter is cut in the counter's own frame (a sink turned on its counter
  shows the hole turned); dirty counters with a sink in them show their clean holed top. GameCube
  counters keep their larger cut-out: the converted sink's flange fills it in the rim's colour.
- Electronics behave as their GameCube base (fun, the channels, the computer's interactions and job
  search, the phone's calls, the alarms; the base tier's fun). PC-only behaviour is not converted (no
  channel per PC TV, no PC computer games or careers, the community speakers play the GameCube's
  stations, a jukebox does not pick records, the DJ booth's lights and the pinball's lit playfield are
  static, the RC toys are converted with Kids, no karaoke, no shop registers or kiosks, no massage table). A phone's handset stays
  on the phone during a call (as the GameCube's: its handset node does not move); a table phone's caller
  plays the GameCube's wall-phone animation one tile in front of the table. Wall TVs and speakers are
  floor bases made wall objects (`init_data`): their mesh hangs at the PC height. The 2x2 DJ booths take
  the GameCube booth's 3x2 tiles; the 3x1 home theatre TV is a group of plasma copies. Wall clocks are
  wall art (no time on them). Recolours: the default only.
- Hobbies behave as their GameCube base (its skill, its rate, its fun; a painting, a rocket's patent as
  the base pays). PC-only behaviour is not converted: no crafts (toys, flowers, pottery, clothes: the
  stations paint, the GameCube's only creative product), no drums, violin or synthesizer of their own
  (the drum kit is played as a piano, the violin and bass with the GameCube's guitar, the synthesizer by
  hand), no pool (air hockey on the pool table), no secret door, no PC study menu (the GameCube's books).
  The base's footprint is kept: a 1x1 PC bookcase takes the bookshelf's 2x1, the 2x3 grand piano and the
  1x3 weight machines overhang the base's 2x2, a crafting station of 2x1 on the easel is two easels. The
  PC's moving parts (piano lids, weight stacks, treads, pedals, the telescope's tilt) stay still; the
  sims' animations are the GameCube's (lying for the bench press on every weight machine, running on the
  treadmill and the bike, kickboxing at the barre). Left out: dance sphere, bingo, bowling, log rolling,
  game tables for four, basketball, soccer, rinks, darts and axe throwing, sauna, playground pieces,
  street performers' mats, the train table, the car to restore, the open-mic stage, the secret door
  bookcase (reasons in the inventory).
- Storage and kids: dressers, armoires and lockers behave as the GameCube's dresser and wardrobe (Change
  Clothes and the outfit planner; no PC "Plan Outfit" menu of its own, no storage of the family's clothes);
  the choice of the two is the mesh's width, so a PC armoire two tiles wide is the 2x1 dresser (the sim
  stands at its front, not inside it), a one-tile dresser the wardrobe (the sim steps into it: the PC mesh
  stays shut, the GameCube wardrobe's own doors are not drawn). PC doors and drawers stay still. The
  2-tile Apartment Life closets are free-standing (their sliding mirror front at the front of the tile;
  no closet behind a wall); the 1-tile closets are left out (built into a wall). The remote-control toys
  are the game console (its game and fun; the toy does not drive or fly). The GameCube has no babies,
  toddlers or children: cribs, high chairs, potty chairs, changing tables and the lemonade stand are left
  out; toys, toy boxes, the dollhouse, the activity table, the spring rider and the coat rack are
  decorations (view only). Recolours: the default only.
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
