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
(`GetSimFunds`/`SetSimFunds`) are shown. While open in a two-player game, only the build/buy player's view is
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
shaded red. A tool's change is made in the world, checked (`ERoom::PreviewWallBuild`,
`EIFloor::TestCreateFloors`; undone from a copy on failure), then rooms are recomputed and the house
rebuilt. There is no undo stack (GC has none). Selling refunds through the catalogue callbacks.

HUD: `BBHTarget` ([`bbhtarget.cpp`](../src/game/bbhtarget.cpp), screen 0x29: mode, price, money, sell
pig), `FCMTarget` fire code meter ([`fcmtarget.cpp`](../src/game/fcmtarget.cpp); level from
`GetFireCodeValue`, PC: 0, `SIMMY_FIRE_CODE`), the 3D preview `UI3D` + `CUnlockDisplay`
([`ui3d.cpp`](../src/game/ui3d.cpp), [`unlockdisplay.cpp`](../src/game/unlockdisplay.cpp)).

Lot value: `cFixedWorldImpl::ComputeArchValue` (80150414) + objects (`lotsave.cpp` stand-ins for
`ComputeStats`/`ComputeAndStoreLotData`). Saving the lot: [files-and-saves.md](files-and-saves.md).

## Gotchas

- The story starts lot 9 with 0 simoleons: nothing is affordable and the wall builder's A does nothing.
  Use `SIMMY_FUNDS=<n>` (on the lot) or `SIMMY_BUY_FUNDS=<n>` (when the catalogue opens) in tests.
- New objects get GC's fixed default data and their init tree run at once (PC stand-in for
  `MakeNewOutOfWorldObject`'s Initialize/Reset), or placement flags are 0 in paused buy mode.
- Buy mode cannot put objects onto surfaces (tables, counters): the place manipulator's slot search is
  stubbed. Objects contained in another's slot (object data 2/3) are drawn at the container's slot offset
  (`ISimsObjectModel`, isimsobjectmodel.cpp; `SIMMY_SLOT_OFFSET_OFF=1` to compare, `SIMMY_SLOT_TRACE=1`).
- The grid tile object (GUID 0x437) cannot be constructed by the port's object module; its model is drawn
  directly. The build tools draw no cursor/line preview (their `PreDraw`/`Draw` are empty).
- Placement events (object trees 5/6/9/10/0xB/0xC on place/pick up) are not run.
- Mouse support stops here: build/buy cursors move with WASD / the stick ([input-and-control.md](input-and-control.md)).
- `SIMMY_BUILD_TRACE=1` reports rebuilds and tool state; `=2` dumps the wall segments; `SIMMY_DUMP_WORLD=1`
  dumps tiles, rooms and walls at house load.
