# The lot: world, rooms, house, buy and build

## Overview

A lot has two representations that must stay in step:

- the **simulation's world** (`cFixedWorldImpl`, `Globs_pFixedWorld`, [`world.cpp`](../src/game/world.cpp)):
  per-tile floor pattern, flags, room id and walls (`TileWalls`), read from the house's `Arry` resources;
  rooms are computed from it (`RoomManager`, [`rooms.cpp`](../src/game/rooms.cpp));
- the **3D house** (`EHouse` in [`livemode.cpp`](../src/game/livemode.cpp), its `ERLevel`, `ERoom`
  wall parts, `EIFloor` floors, lights, object instances).

Build tools change the world, then **rebuild the house** (`EGlobal::RecalcHouse` -> `EHouse::ReCalcHouse`
80024E04, [`houserecalc.cpp`](../src/game/houserecalc.cpp)). Buy mode creates objects and places them
through the object placement code ([`objectplace.cpp`](../src/game/objectplace.cpp)).

## Coordinates

- Tiles: `CTilePt` (x, y, level). Object locations are FTilePt pairs in **1/16 tiles** with `[0]` = y and
  `[1]` = x ([`routing.hpp`](../include/simmy/classes/routing.hpp), [`roomresolve.hpp`](../include/simmy/roomresolve.hpp)).
  The 3D world frame swaps x and y (`CollisionMap`, `SAnimator2` comments).
- A tile's centre is +8/16. Person positions are already world positions; objects are drawn at
  integer tile + 0.5 (`ISimsObjectModel::SetObjOrient`). Doors and windows (2-part portals) use the
  group's centre so their origin sits on the wall line (`cXMTObjectImpl::IsDynamic` 800C71F0, used by
  both `SetObjOrient`s 80051B94 / 8005127C).
- Directions are eighths of a turn.

## Rooms

| Function | GC | Notes |
|---|---|---|
| `cFixedWorldImpl::ComputeRooms` | | Floods between walls; room 0 = outside; diagonal walls split tiles in halves (`RoomManager::ResolveDiagonal`) |
| `ObjectModuleImpl::UpdateRooms` | 800F5B10 | Every object's room again ([`roomresolve.cpp`](../src/game/roomresolve.cpp)) |
| `ResolveRoomID` | 800DF228 | A point's room (split tiles by side) |
| `UpdateObjectRoomID` | part of 800DF784 | Object data 0x1D on a move; called from objectplace.cpp and the animator |

Object data 0x1D (room) drives the indoor shadow blob, the room light set, the room scope (`InterpValue`
0x17) and sounds. Rooms are computed **before** objects' `PostLoad` at house load (load trees ask the room
scope). Not maintained: a person teleported into another room (person.cpp `SetLocation` spawn) keeps the
old room until it walks.

**Room scores** (rooms.cpp, GC): `RoomManager::ComputeRooms` (80136274) re-collects each room whose
statistics are stale: per tile `Room::CollectTileStats` (80137BE0: area, floored area, wall sides of a scored
style `IsScoredStyle` 80137B58 and the papered ones; a diagonal tile counts half) and
`CollectObjectStats` (80137E44, the tile's `ObjectIterator`: objects, people, light givers (data 0x28 bit
0x400) and their light (data 0x33; doors/windows apart), room impact data 7), then `ComputeRoom` (801377D8):
inside, the area, light, impact over the effective area, floored share and papered share scored with the
`RoomScoreConstants` (801358D0, group 3 of the global object file's float constants) and clamped to +-100;
outside, the impact-free object count and the impact. The outside rooms give the outdoor score (with the
time of day's bonus); the inside area goes to simulator global 0x1B. Windows add no light: the outside
ambient level (`GetOutsideAmbientLevel` 80136FB4) reads `Globs::pLightingParameters`, which nothing on GC
writes (0). Recomputed when dirty from `ELiveMode::Update` (`UpdateRooms` 80136800, then the house's lot
data), on the world's room change (GlobalDispatch 0xEC), an object with a room impact changing rooms (0xEF:
both rooms) and primitive 0x07 TryUpdate 2. Primitive 0x07 TryUpdate (801058E0) also covers 1 (GlobalDispatch
0xEE: the object module re-sends 0xED with the object's room, which marks that room's statistics and light
stale: a lamp a tree switches changes its room's light; PC: `ObjectModuleDispatch` in house.cpp, the 0xEE
case of `ObjectModuleImpl::DoCommand` 800F6AB4), 3 (the object's model placed again, `SetObjOrient`) and 4
(the lot data, `HouseImpl::ComputeAndStoreLotData`). The person's room motive is its current room's score
(simulation.md). `SIMMY_ROOM_TRACE=1` prints the constants and each recomputed room.

**The house** (`HouseImpl`, house.cpp): its family (the family living in the house, else family 0, set
with simulator globals 9/10 as `NeighborhoodImpl::LoadHouse` does, before the objects' `PostLoad`),
`DoCommand` (800BBD40: the broadcasts it hears; PC: `HouseDispatch` where the port broadcasts them, as
GlobalDispatch has no listener list), `IsNative`/`IsResident`, `RefreshHouse`, `EnterLiveMode`,
`PrepareForBudgetWindow`, `ComputeAndStoreLotData` (the lot value and the family's phone/baby flags).
Loading/saving (`Initialize`, `LoadFile`, `SaveFile`) stay the world side's stand-ins (livemode.cpp,
lotsave.cpp).

**Sites and the tile chain** (objectplace.cpp, GC game/object.cpp): an object's site is a tile or an
entry of another object's slot vector (`cXObjectImpl::m_slotContents`: entry 0 the tile chain, k + 1
object slot k; data 0x1A the object above, 0xE the entry). The module's tile map names the last object
placed on a tile; the object it displaced is chained under it (`TestAndPlace`'s place half: `HierLink` /
`HierUnlink`, `HierSetSite` 800DCFB8, `HierSever` 800DD268, `HierGetSite`/`HierGetObject`/`HierGetChild`).
Picking an object up gives its site to the object under it. `ObjectIterator::operator++` (800F2AC0) walks
the vector depth first; `TestIntersection`, chair facing and the router walk a tile's chain (the object and
its entry-0 siblings). So a window bought onto a counter's or lamp's tile is its tile's object and
`GetWallModelId` finds it (the wall cut). People are sited too (every `Place`); PC: the tile a top-level
object holds is remembered (`m_siteTile`) as the PC walker moves a person's location between places
(`HierResite`), and an object chained under a person the save does not have (the port saves no people)
is its tile's object again at load.

## The 3D house

| Piece | File | Notes |
|---|---|---|
| `EHouse` | livemode.cpp, houserecalc.cpp | Build, cleanup, wall modes (up / cut-away / down: `SIMMY_WALLS`), room lights, weather, `Update`/`Draw` |
| `ERLevel::Draw` 80234C98 | [`e_rlevel.cpp`](../src/game/e_rlevel.cpp) | Sky, terrain, per-room shadows/objects/particles, the order table, fades |
| Walls `ERoom`/`ERoomWall`/`EIWallPart`, fences `EIFenceWall` | [`eroom.cpp`](../src/game/eroom.cpp) | Wall model from the portal on its tile (`GetWallModelId` 8002BD58, via `ObjectIterator`); diagonal walls (`ERoom::ProcDiagonalWalls` 8002EF04) |
| Object models `ISimsObjectModel` and kin | [`isimsobjectmodel.cpp`](../src/game/isimsobjectmodel.cpp), [`iobjectman.cpp`](../src/game/iobjectman.cpp) | `EIObjectMan::AllocSimsObjectInstance` (800550C0) picks the class from the definition: wall objects, counter tops (ResData +6), the "boxwood_hedge" model, multi-tile, plain. A **counter top** (`ISimsCounterTopObject`, 80052190) with a sink (ResData +7) on its tile draws the counter model table's cut-out model plus the table's sink model as an owned sub-object (`ECntrMdlLkupTable` in the SimsObjects quickdata: counter, sink, cut-out; `OrentSubObject` 80050F14; sub-objects take the counter's cursor flags, `PropigateFlagsToSubModels`). The **hedge** (`IShrubObject`, 800526EC) stretches 1.32 along an axis with a hedge beside it; no object on the disc uses that model (its id is not in the SimsObjects quickdata), so it never shows. `ApplyMatrix` (80052E3C) gives portals their two rooms (`PortalRoom`), notes whether the tile is outside (`m_objFlags` bit 29; no reader found on GC) and honours the simulator's render position override (`cXObjectImpl::OverrideRenderPosition` 800E2FF8; nothing sets it in the release). Still stubbed there: the height on a counter (data 4 bit 0x20, `GetHeightOffset`) |
| Floors `EIFloor` | [`ifloor.cpp`](../src/game/ifloor.cpp) | Strips incl. diagonal half tiles; per-vertex lighting (`DoLightingCalculation` 80046614) |
| Wall/object fades | [`interestfade.cpp`](../src/engine/interestfade.cpp), `SetupInterestFade` (livemode.cpp) | Trapezoid fade on walls, pyramid/line fades on objects so windows fade with their wall |
| Lights | [`e_lightgrid.cpp`](../src/game/e_lightgrid.cpp), [`roomlights.cpp`](../src/game/roomlights.cpp), [`e_ipointlight.cpp`](../src/game/e_ipointlight.cpp) | 63x63 light grid, lamps, window portal lights; see [rendering.md](rendering.md) |

## Buy and build mode

Entry: pause menu tab 4 (buy) / 5 (build) -> flow 0x24 `PAUSE_BUY` / 0x25 `PAUSE_BUILD` ->
`InLevelBaseState` makes **`PCTTarget`** ([`pcttarget.cpp`](../src/game/pcttarget.cpp), screen 0x1E "PCT";
mode 0 buy, 1 build, 2 tombstone). Tabs come from the ELF tables (`sPCT_TabInfo_Buy` 9, `_Build` 7); cells
are catalogue entries sorted by price; prices, ratings, locks (`GoalUnlock`) and funds
(`GetSimFunds`/`SetSimFunds`) are shown. A locked object's description is `cat_locked_msg` with its
`<unlock_aspiration_points>` tag filled in (`UIDB` string set by `GetLocalizable`, substituted by
`UIObjectBase::doUIQDStringSubstitution` 80177CA8 -> `UIQDTarget::UIQDSubstitutionStrings`). While open in a two-player game, only the build/buy player's view is
drawn, full screen (`GameData::SetBuildBuyPlayer`, `ELiveMode::DrawMain`).

**Interactors** (numbers as on GC; PC stand-ins run them from `UpdatePlaceManipulator` in
[`placemanipulator.cpp`](../src/game/placemanipulator.cpp), not from an interactor manager):

| # | What | Code | Pad (GC semantics from the ELF) |
|---|---|---|---|
| 6 | Place a new object | `PlaceManipulator`, `PlacementObject` (placemanipulator.cpp) | stick moves a tile, A place (0), B cancel (4), L/R rotate (0x13/0x14 -> 5/6) |
| 2 | Grab mode (move/sell placed objects) | `GrabManipulator` | A grab/drop, X sell (0x2E -> 1); the objects it may pick up near the cursor glow (`ObjectManipulator::UpdateObjectHighlights` 80219C74), A takes the lit one, several -> grab menu (`CreateGrabObjectMenu`) |
| 3 | Walls and fences | `WallManipulator` ([`wallmanipulator.cpp`](../src/game/wallmanipulator.cpp)) | A anchors, second A builds, B lets go/leaves, trigger held = room, Y next tool |
| 4 | Wallpaper | `WallPainter` ([`wallpainter.cpp`](../src/game/wallpainter.cpp)) | as above |
| 5 | Floors | `FloorPainter` ([`floorpainter.cpp`](../src/game/floorpainter.cpp)) | as above |

Placement legality is the GC object code ported over the port's world model (`ObjTestAndPlace` 800DD6E8,
`ObjTestIntersection` 800DEBB8, `CheckWallFlags` 8014A560, multi-tile groups); illegal placements are
shaded red. Placement events run the object's trees at once (`cXObjectImpl::RunTree`): 9 "placed" (it, and
its container) on `Place` / `Pickup`, 0xA "picked up", 6 wall adjacency changed, 0xB / 0xC placed / picked
up by the player (`UserPlace` 800E1FF4 / `UserPickup` 800E2128, also for what its slots hold; grabbing an
object picks it up through `UserPickup`), 7 its room changed (`ObjectModuleImpl::UpdateRooms`), 5 "allow
intersection" in `TestIntersection` (800DEBB8: both objects' trees, each with the other as the stack
object, before their footprints are compared). Surface
objects go into a slot of an object on the tile (`IsLegalToPlaceSingleTileAtLocation` 80211CD8: the first
slot of a matching height where `CanPlace` allows; the container's "placed" tree runs).
`SIMMY_PLACE_TRACE=1` prints the placement trees run; `SIMMY_PLACE_TEST` places, grabs, deletes walls and
papers rooms by script ([testing.md](testing.md)). A tool's change is made in the world, checked (`ERoom::PreviewWallBuild`,
`EIFloor::TestCreateFloors`; undone from a copy on failure), then rooms are recomputed and the house
rebuilt. There is no undo stack (GC has none). Selling refunds through the catalogue callbacks.
After a placement or pickup (`UpdateAllObjectsInWorldAfterFirstPickupOrFinalPlace` 80212390) the rooms are
recomputed and every object model placed again (`EIObjectMan::ReOrientHouse`: a counter sees a new sink);
a portal, a light source (object data 0x10 = 1) or a zero-extent-override object (`CheckForZeroExtentOverride`
80041F98: definition flags 6) rebuilds the house. A new light source in hand, a cancelled placement of one and
a sold one rebuild it too (`RecomputeLightingGrid` 80212458: the light grid). `UserCanPlace` (800E1E30)
refuses container slots of types 5..7 (`ObjectSlotHeight`; error 0xB) unless the move-objects cheat. A wall
edit makes every person plan its routes again (`InvalidateAllRoutes`, `WallManipulator::HandleFinalizeRequest`
80221028).

HUD: `BBHTarget` ([`bbhtarget.cpp`](../src/game/bbhtarget.cpp), screen 0x29: mode, price, money, sell
pig), `FCMTarget` fire code meter ([`fcmtarget.cpp`](../src/game/fcmtarget.cpp); level from
`GetFireCodeValue`, see below), the 3D preview `UI3D` + `CUnlockDisplay`
([`ui3d.cpp`](../src/game/ui3d.cpp), [`unlockdisplay.cpp`](../src/game/unlockdisplay.cpp)).

**Fire code (lot object limit).** `GetFireCodeValue` (800427A8, pcttarget.cpp) polls the memory meter
(`EMemoryMeterWin::PollMemoryLevels` 80057A1C over `EIObjectMan::CountObjects` 80055404): the house's object
instances that are not people, without the grid tiles (GUID 0x437) and the first 15 objects whose definition
has flag 0x10 (+0xB6), give three shares: objects / 300, weighted objects / 500 (each 1 + its animated
character's nodes / 6.75) and model memory (KB - 420) / 1190, where the memory is the summed data sizes of
the different models they draw (`EResource` +0x10 >> 8, the MODL size recorded at load, the same disc data
as on GC) plus the house's own model (EHouse +0x20). The fill is the largest share + 0.3 x the middle one +
0.1 x the smallest, kept in 0..1 (`_gd.m_memoryMeter`, GameData +0x04). Buying warns above 0.66 and 0.95
(`firecode_text_1/2`) and `on_PCT_EnterPlaceMode` refuses an object at 1. Lot 9 at the story start reads
0.617 (106 objects 0.353, weighted 170 0.341, models 987 KB 0.477). The EOR config's fire code switch
(EGlobal +0x39C, off in the release) would cap a full meter at 0.97. PC: `fire_code_limit=off` in simmy.ini
(default on) makes it 0 (empty meter, no limit); `SIMMY_FIRE_CODE=<0..1>` forces a value,
`SIMMY_FIRE_CODE_TRACE=1` prints each poll.

Lot value: `HouseImpl::ComputeAndStoreLotData` (800BBAE8): `cFixedWorldImpl::ComputeArchValue` (80150414)
+ objects (`ObjectModuleImpl::ComputeStats`: a stand-in in `lotsave.cpp`), also the family's phone / baby
flags; run on every room update (`PrepareForBudgetWindow`), each day (0xD8) and on save. Saving the lot: [files-and-saves.md](files-and-saves.md).

## Gotchas

- The story starts lot 9 with 0 simoleons: nothing is affordable and the wall builder's A does nothing.
  Use `SIMMY_FUNDS=<n>` (on the lot) or `SIMMY_BUY_FUNDS=<n>` (when the catalogue opens) in tests.
- New objects get GC's default data, their init tree and one tick of their main tree at once
  (`MakeNewOutOfWorldObject`'s Initialize and `Reset(true)`, [simulation.md](simulation.md)), so their
  placement flags are set in paused buy mode.
- Grab mode lights, with nothing in hand, every object instance whose bounds meet the square of the
  selection radius (0.35..1 tile with the zoom) about the cursor's tile centre and that may be picked up
  (data 0x22 0, `UserCanPickup`); while the grab menu is open only its current object. A grabs the lit
  objects (`GetObjectsInRect`; PC: a multi-tile object once, by its lead).
- Doors and windows dress their wall: placing one (`cXPortalImpl::Place` 8012FDD0 -> `ApplyWallStyle`
  8012FE1C) gives the wall it needs on each part's tile its definition's wall style (+0x5C: 3 for most,
  the style the wall model, the router, the collision map and the wall tool read as "a door or window is
  here"); picking it up (`cXPortalImpl::Pickup` 801302B0, also when it is killed) puts style 1 (plain)
  back; both dirty the routes. As on GC this happens while the object follows the buy cursor too. PC
  guard: lifting a door off another door's wall leaves that wall (GC's grid object keeps the door from
  being placed there; see below). `SIMMY_PLACE_TRACE=1` prints each wall styled.
- A wall delete whose rooms fail the check (`FinalizeWallDel` 80220570) removes instead the whole wall
  run holding the line (`ERoom::DeleteERoomWallContainingSegment` 8002FEAC, `FindWallContainingSegment`,
  `ERoomWall::HasSegment` / `RemoveWallsFromWorld` / `DeleteWallAtTile` 8002F84C..8002FBEC; HasSegment does
  not look at the run's first tile, as on GC) and checks again; still failing, nothing is deleted.
- An object on a person (data 4 flag 0x20, data 2 its holder) is placed at the holder's bone 0 height
  (`GetHeightOffset` 8004C7F0, 0.001..1) and, when the holder has an instance, it and what it holds are
  drawn as carried (cursor flags 0xC0). PC guard: not when data 2 names the object itself (the template
  person does). Trash piles have the flag and no holder (height 0).
- Objects contained in another's slot (object data 2/3) are drawn at the container's slot offset
  (`ISimsObjectModel`, isimsobjectmodel.cpp; `SIMMY_SLOT_OFFSET_OFF=1` to compare, `SIMMY_SLOT_TRACE=1`). A
  multi-tile container's side with room (`IsLegalToPlaceSingleTileAtLocation`) is not chosen.
- The grid tile object (GUID 0x437, `PlacementObject::CreateGridTile` 8021B27C and helpers) is not made;
  its model is drawn directly. GC places the held object only where its grid object may go
  (`PlacementObject::Place` 8021BEE4); the port places it anyway and shades it.
- The build tools' cursors and previews (`InteractorVisualizer`, interactorvisualizer.cpp; drawn through the
  house's order table like the other interactors: `SetupDrawCallback` -> `BuildToolPreDraw`, the record's
  callback -> `BuildToolDraw`, buildtools.cpp). Floor tool: the cursor's two models; with the trigger the
  room under it in the pattern (`DrawFloorRoomPreview`); anchored, the corner models (the far one green) and
  the rectangle in the pattern just above the floor (`DrawFloorRectPreview`), lit red while invalid. Wall
  tool: the anchor and cursor models; dragging, the line-end model green or red and the line as an upright
  strip 3 high (`DrawVerticalRect`; a room: its four sides), the walls a delete would remove
  (`DrawPreviewOnAffectedWalls`). Wallpaper tool: the anchor and brush models; dragging, the paper on the wall
  sides it would cover (`DrawWallpaperRectPreview`) and the strip (red when none), with the trigger the
  paper round the room (`ERoom/ERoomWall::DrawWallpaperPreview`). The models are drawn in
  `Player::GetInteractorColor` (1, 1, 3). `DrawVerticalRect` sends its strip as arrays
  (positions, uvs and normals allocated from the context) through `ERC::TriStrip(int, float*, ...)`
  (802F68EC; immediate contexts draw it at once, `ENgcRC::TriStrip` 80338484).
- A wall holding a door or window is not deleted (GC: `ERoom::DeleteWallAtTile` 8002FF8C asks
  `KillArchitecturalObject` 80030104 without killing; the object's GUID is returned and the wall stays).
- Room wallpaper (the wall painter with the trigger held): `CountWallsInRoomSelection` 802256FC /
  `FinalizePaperForRoom` 80225D70 over the room's tiles and `ERoom::GetNumWalls` (8002DF7C, the rooms'
  lookup table `InitRoomLookupTab` 8002DCF0). GC quirk kept: a diagonal's sides are written after the
  previous segment's into a small array whose last entry is the room id. The preview is drawn per wall
  run (`ERoom::DrawWallpaperPreview` 8002E0C4, `ERoomWall::DrawWallpaperPreview` 8002D3C0, `GetWallDims`
  8002C860).
- Mouse support stops here: build/buy cursors move with WASD / the stick ([input-and-control.md](input-and-control.md)).
- `SIMMY_BUILD_TRACE=1` reports rebuilds and tool state; `=2` dumps the wall segments; `SIMMY_DUMP_WORLD=1`
  dumps tiles, rooms and walls at house load.
