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
both rooms) and primitive 0x07 TryUpdate 2. The person's room motive is its current room's score
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
| 2 | Grab mode (move/sell placed objects) | `GrabManipulator` | A grab/drop, X sell (0x2E -> 1), several objects under the cursor -> grab menu (`CreateGrabObjectMenu`) |
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

HUD: `BBHTarget` ([`bbhtarget.cpp`](../src/game/bbhtarget.cpp), screen 0x29: mode, price, money, sell
pig), `FCMTarget` fire code meter ([`fcmtarget.cpp`](../src/game/fcmtarget.cpp); level from
`GetFireCodeValue`, PC: 0, `SIMMY_FIRE_CODE`), the 3D preview `UI3D` + `CUnlockDisplay`
([`ui3d.cpp`](../src/game/ui3d.cpp), [`unlockdisplay.cpp`](../src/game/unlockdisplay.cpp)).

Lot value: `HouseImpl::ComputeAndStoreLotData` (800BBAE8): `cFixedWorldImpl::ComputeArchValue` (80150414)
+ objects (`ObjectModuleImpl::ComputeStats`: a stand-in in `lotsave.cpp`), also the family's phone / baby
flags; run on every room update (`PrepareForBudgetWindow`), each day (0xD8) and on save. Saving the lot: [files-and-saves.md](files-and-saves.md).

## Gotchas

- The story starts lot 9 with 0 simoleons: nothing is affordable and the wall builder's A does nothing.
  Use `SIMMY_FUNDS=<n>` (on the lot) or `SIMMY_BUY_FUNDS=<n>` (when the catalogue opens) in tests.
- New objects get GC's fixed default data and their init tree run at once (PC stand-in for
  `MakeNewOutOfWorldObject`'s Initialize/Reset), or placement flags are 0 in paused buy mode.
- Objects contained in another's slot (object data 2/3) are drawn at the container's slot offset
  (`ISimsObjectModel`, isimsobjectmodel.cpp; `SIMMY_SLOT_OFFSET_OFF=1` to compare, `SIMMY_SLOT_TRACE=1`). A
  multi-tile container's side with room (`IsLegalToPlaceSingleTileAtLocation`) is not chosen.
- The grid tile object (GUID 0x437) cannot be constructed by the port's object module; its model is drawn
  directly.
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
  `Player::GetInteractorColor` (1, 1, 3). PC deviation: `DrawVerticalRect` sends EGEVerts
  (`ERC::TriStrip(EGEVert*)`; the arrays variant of `ERC::TriStrip` is not ported).
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
