# Sim bucket audit (sim.tsv, 1678 stubs)

Verdicts: PC-replaced 932, NA-unreachable 640, MISSING-H 34, MISSING-M 45, MISSING-L 19, UNSURE 8.
Per-row verdicts are in `sim_verdicts.tsv`.

How I checked:
- I read the raw GC code and the port caller or comment for every hit and frontier row I could not decide from the name.
- I listed which stub symbols ported `.text` actually calls, using relocations in the newest object build I could find (`/home/claude/wld/build/asan`, from Nov 2, an older copy of the port). I then checked the current source by name.
- Only 11 sim stubs are called directly from ported code. All 11 are covered below.
- Most other stubs are virtual slots or helpers whose job the port does in its own PC classes.

## MISSING, by priority

### H1. Check trees overwrite the person's temps (S)
- **GC:** `ObjTestSim::~ObjTestSim` puts back the person's 8 temps after menu and autonomy check trees. It does this with `cXObjectImpl::SetTemp` (800EAF0C), a one-line setter of `m_temps[i]`.
- **Port:** `objtestsim.cpp:36` calls `SetTemp`, which is still the generated stub (logged in real runs). So the temps are never put back.
- **Callers:** `person.cpp:567` and `person.cpp:775` (TestInteraction / found-action gosub), plus every menu built while a sim runs an action.
- **Player sees:** a running interaction's tree reads temps that a menu or autonomy check changed. Results can be wrong or random: wrong branches, wrong targets, odd failures after opening a pie menu mid-action.
- **Fix:** write `m_temps[i] = v`.

### H2. Quitting never offers "save before quit" (S)
- **GC:** `EGlobal::IsSaveGameEnabled` (80042718) is true unless a player is in social mode or the HUD is busy.
- **Port:** the stub returns 0. `memcardscreens.cpp:395` (M2MTarget, source 7: quit from the game) therefore always shows `ngc_yesno_nosavebeforequit_body`.
- **Player sees:** quitting to the menu never asks to save. Progress is lost unless the player saved by hand first.
- **Related (L):** `EGlobal::IsPlayerInSocialMode` (800409D8) is assumed false in `memcardscreens.cpp:394`, `paztarget.cpp:166` and `o2ttarget.cpp:168`.

### H3. Direct control: objects are not highlighted, and the pie menu picks different objects (M)
- **GC:**
  - `DirectInteractor::UpdateObjectHighlights` (80205C4C) highlights, every frame:
    - objects from the InfluenceMap tile lists within 5 tiles that pass `ShouldHighlightObject` (80204C5C: a facing cone);
    - people within 1.5 tiles with no wall between (`CollisionMap::DoesCollideEnvironment`).
  - `ExecuteActionMenu` then builds the A-button menu from those highlighted instances (`EIObjectMan::GetObjectsInRect` 80054CA0).
  - Also stubbed: `TurnOffAllHighlights` (80054C4C) and the InfluenceMap group (MISSING-M below, needed for this).
- **Port:** `MenuObjectsNear` (`flashpimenu.cpp:397`, called from `directinteractor.cpp:475`) takes the nearest 4 objects within 2 tiles, in any direction, with no wall test.
- **Player sees:**
  - nothing glows near the sim;
  - A offers objects behind the sim or through a wall, and misses objects 2 to 5 tiles ahead that GC offered.
- This is the same area as the user's held-object bug.

### H4. Highlight glow is never drawn (M)
- **GC:** `ISimsObjectModel::BlendHighlights` (8004F02C) blends the highlight colour into the object's lights.
- **Port:** it is logged as a stub in `isimsobjectmodel.cpp:679` (`DoLightingCalculation`).
- **Player sees:** in classic (cursor) control the object under the cursor gets `SetHighlight` (`siminteractor.cpp:636`) but shows no glow. The player cannot see what A will act on.

### H5. Build tools draw no cursor or preview (M/L)
- **GC:** InteractorVisualizer draws the floor, wall and wallpaper tools' cursor model, rectangle, line and room previews.
  - Functions: CreateResources / DestroyResources / PreDraw / Draw for FloorPainter, WallPainter and WallManipulator (80213F98..80215C90), DrawFloorRectPreview, DrawFloorRoomPreview, DrawWallpaper*Preview, DrawWallRectPreview, DrawVerticalRect, DrawRoomPreview, DrawPreviewOnAffectedWalls, SelectShader.
  - Also `ERoom/ERoomWall::DrawWallpaperPreview`, GetWallDims, getWallNormalOnSideOfCursor, and `Player::GetInteractorColor` (800595AC).
- **Port:** the tools' PreDraw/Draw are empty: `floorpainter.cpp:313-315`, `wallmanipulator.cpp:463-465`, `wallpainter.cpp:348-350`. This is noted in `docs/build-buy-and-world.md:143`.
- **Player sees:** in build mode there is no cursor, wall line or floor/wallpaper rectangle while dragging. The player builds blind.

### M1. Objects that should run while paused or in buy/build never do (S)
- **GC:** `cXObjectImpl::UpdateSimFlags` (800E2748, logged) sets module sim flag 2 from `SimIndependent` (data 0x3C). It also sets flag 8.
- **Port:**
  - `objectmodule.cpp:156` calls the stub.
  - Flag 8 is replaced by `CanChooseAutonomously`.
  - Flag 2 survives only in sim-flag words loaded from a GC save. Objects bought or created later never get it.
  - `TickAllObjects` (`simulator.cpp:378`) skips them while paused.
- **Player sees:** sim-independent objects stop in pause, dialogs and build/buy mode. I did not check which objects set data 0x3C.

### M2. Default object data is missing on tree-created objects and multi-tile parts (S)
- **GC:** `cXObjectImpl::Initialize` (800DA3D8, logged) and `EdithVariableSet::Initialize` (800E33F4) set the data defaults:
  - 3 and 0xE = -1
  - 0x17 = 2 (turn step)
  - 0x18 = 7
  - 0x2D-0x2F = the purchase date
  - 0x31 = 0x32
- **Port:**
  - Only buy-mode objects get them (`placemanipulator.cpp:56` InitNewObject).
  - Multi-tile parts (`mtobject.cpp:80`) and objects made by the create-object primitive (`objectsim.cpp:2032`: food, plates, puddles, visitors) do not.
  - Slots and attributes are done by the PC constructor (`object.cpp:30-32`).
- **Player sees:** probably subtle (turning, container slot -1 checks).

### M3. Relationships never drift (S)
- **GC:** `Motives::Sim` calls `cXPersonImpl::IncrementSimSocialInterest` (8012BB8C) when the sim is not socialising. It moves each relationship's second value toward a target derived from the first.
  - Get/SetSimSocialInterest (8012BDD8, 8012BF48) convert it to and from motive values.
- **Port:** the call is an inline stub at `treesim.cpp:708`.
- **Player sees:** short-term relationship or interest values never move between socials.

### M4. Sinks on counters and shrub variants are drawn wrong (M)
- **GC:**
  - `EIObjectMan::AllocSimsObjectInstance` makes `ISimsCounterTopObject` for counters. The counter swaps to its sink-cutout model and draws the sink as a sub-model (counter model table, `IsSinkId`). `OrientSubObjects` / `OrentSubObject` (80050EB4, 80050F14) re-orient it.
  - It makes `IShrubObject` for shrubs and hedges, which picks the model and orientation from neighbours.
- **Port:** both are plain objects (`iobjectman.cpp:101-110` inline stubs). `OrientSubObjects` is called by `isimsobjectmodel.cpp:1160,1283` but has nothing to orient.
- **Player sees:** a sink placed in a counter is drawn on a solid counter top. Shrubs do not vary.

### M5. InfluenceMap is never built (M)
- 80048844..800492E8, `ObjectModuleImpl::UpdateInteractionInfluences` 800F5DD0, `cFixedWorldImpl::GetInfluenceMap` 8015235C.
- The per-tile lists of interaction areas are not built (`world.cpp:457,488` inline stubs).
- In release its only consumer is the direct-control highlight (H3). Port it with H3.

### L items
- **Wall delete after a failed room preview:** `ERoom::DeleteERoomWallContainingSegment` and its helpers (8002FEAC, 8002FD08, 8002F84C, 8002FA34, 8002FBEC). After the room preview fails, GC removes the containing ERoom wall and retries. The port refuses the delete (`wallmanipulator.cpp:172`). (S)
- **Object age:** `UpdateAge` (800DBB18, logged, `objectmodule.cpp:101`) and the JustBorn dates (`objectmodule.cpp:527`). Object age data 0x12/0x30 is never updated. (S)
- **Person reset after a tree error:** `cXPersonImpl::Reset` (8011EB78) / `cXObjectImpl::Reset` (800DA848). Only the stack and animator reset (`objectsim.cpp:195-212`). The person data is not cleared, the neighbourhood is not notified and the motive effects are not reloaded. (S)
- **Height on a container:** `ISimsObjectModel::GetHeightOffset` (8004C7F0). Objects with data 4 bit 0x20 get no height offset from the container model (`isimsobjectmodel.cpp:1101`). (S)
- **Swap sims call:** `EGlobal::SwapSelectedSims` (800420B4). Generic sim call 0x1E falls to the "not ported" default (`objectsim.cpp:3104`). It matters only if a tree uses it. (S)
- **Plumb bob on selection change:** `PlumbBobOwner::GrabPlumbBob` (8005B89C, inline stub at `global.cpp:215`). Check the plumb bob follows a newly selected sim. (S)
- **Integrity checks after load:** `UpdateSimObjects` / `CheckIntegrity` (800F62DC, 800F7F28, `objectmodule.cpp:82`). These are the integrity fixes after a load. (M)
- **Door/window wall style:** `cXPortalImpl::ApplyWallStyle` (8012FE1C). A door or window's own wall style is not applied. (S)
- **GUIDs for new CAS sims:** `StubObject::MakeNewGUID` (80145D58, `objectfolder.cpp:611`). Check new CAS sims get unique GUIDs. (S)
- **Placement grid tile:** `PlacementObject::CreateGridTile` / `InitializeGridObject` / `AssignOffsetsToGridObject` (8021B27C, 8021BC44, 8021BCA8). This is known; the port draws the model. (S)

### UNSURE
- **Dynamic multi-tile merge** (800C7690 MergeInPlace, 800C6F2C, 800C7364, 800C7434, 800C7240, 800C6EBC). Adjacent parts of the same master with definition flag 8 should join one group. To check: place two such objects side by side in buy mode (find which definitions have flag 8; counters are likely) and compare TrySetToNext / the interaction group with GC.
- **Family names** (8004032C GetNghFamilyName, 80040414 GetNghHouseName). GC strips the leading "#" from family and house names on load. The port keeps "#house06" (`family.cpp:115`). To check: look at family and lot names in family select, the save screens and the HUD.

## Comment-only and inline gaps found on the way (not bucket rows)
- **`cXObjectImpl::TryGenericSimCall`** (`objectsim.cpp:2973`):
  - The port handles calls 0x02, 0x0A, 0x0B, 0x0E, 0x0F, 0x16, 0x18, 0x19, 0x1A, 0x1B, 0x22, 0x26, 0x27, 0x29, 0x2C, 0x2D, 0x2E, 0x2F and 0x31.
  - GC also has 0, 1, 3, 4, 5, 6, 0xC, 0xD, 0x10-0x13, 0x15, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x2A, 0x2B and 0x30.
  - Unhandled calls return true with a one-time print `[vm] ... generic sim call 0x.. is not ported`. Search soak logs for that print to find which ones trees use.
  - GC returns 0 (false) for 0x1A/0x1B, but the port returns 1.
- **Clothing unlocks:** `TryCompleteGoal` kind 1 (`objectsim.cpp:2957`): `CasClothingUnlocks::UnlockGroup` is not done. Completing a fashion goal grants the unlock flag but unlocks no clothes (belongs with the CAS/data buckets).
- **Named trees from check trees:** `TryCallNamedTree` from a check tree returns true without running the tree (`objectsim.cpp:2733,2746`).
- **Stale code:** `passiveinfluencemap.cpp:554` `PassiveInfluenceAwarenessCheck` has inline stubs but is dead PC code. `AwarenessManager::PassiveInfluenceAwarenessCheck` (`awarenessmanager.cpp:646`) is the live one.

## What the non-MISSING verdicts cover
- **PC-replaced (932).**
  - Resource and quickdata accessors (TTAB, BHAV, BCON, OBJD, ObjFnTable, AnimTable, ObjectDataID). The port reads them in place (`treesim.cpp`, `autonomy.cpp`, `objdef::`).
  - Slot descriptors (`xrouting.cpp`, `sprites.cpp`) and CTilePt / rect helpers.
  - ObjectFolder / ObjSelector / ObjectModule / object / person / multi-tile members that the port has as its own PC members or field reads. No ported code calls the stub.
  - The interactor manager, input manager and SocialModeInteractor (PC stand-ins).
  - The buy-mode Grab/Place manipulator (`placemanipulator.cpp`).
  - The family inventory (`inventory.cpp`).
  - cXPortalImpl (portals are type-8 MT parts).
  - Commander / GlobalDispatch (HouseDispatch).
  - House, neighbourhood and family load/save stand-ins (`lotsave.cpp`, `livemode.cpp`, `person.cpp` MoveFamilyOntoLot / RemoveFamilyFromLot).
  - CameraManager (per-player `m_cam`).
  - Substitution strings, the screen adjust wizard, and helpers done inline (GetWallPrice, GetEventText, the zero-extent override, TrySnap, IsModelLoaded, the bookmark stream).
- **NA-unreachable (640).**
  - 482 dead functions (no GC caller and no data reference).
  - Interiors reachable only from dead stubs.
  - Debug, recording and type-info helpers; cheats and author mode (ECheats, SetAuthorModeCheats, BroadcastMessage "msg:show special").
  - Loading-stage timing.
  - The camera/room opacity, which is computed but never read.
  - Primitives no shipped tree uses (0x0A, 0x15, 0x22, 0x3D, 0x39-0x3C, person 0x2F reach).
  - ShowAllObjectDialogs (debug). Urbz RepTitleSupport and costumes. ComeSeeMe / PrepareAndTestLot (lot test).
  - HouseStats scores (only the dead GetOverallScore uses them).
  - BBI builder-item and category functions, used only by INVTarget (PDA inventory flow 0xB).
  - PresetHouseGameStatus: its table is never read anywhere in the GC binary. I checked every lis/addi load of 0x804864D0.
  - HouseImpl::SetLotSize (message 0x100, never sent).
