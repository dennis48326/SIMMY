# Simulation: objects, persons, the behaviour VM, autonomy

## Overview

The lot is simulated by `cSimulatorImpl` (`Globs_pSimulator`, [`simulator.cpp`](../src/game/simulator.cpp)):
each **tick** runs every object's behaviour tree once, in id order, on the SimAntics VM inherited from PC
The Sims (`TreeSimImpl`, [`treesim.cpp`](../src/game/treesim.cpp)). Objects (`cXObjectImpl`) and people
(`cXPersonImpl`, an object with motives, an action queue and an animator) live in the object module
(`Globs_pObjectModule`). Almost all gameplay (cooking, socials, careers, the story scenario, the
tutorial) is **tree data**, not C++: the C++ is the VM, its ~70 primitives, the variable scopes, motives,
autonomy scoring, routing and the person's animator. When gameplay misbehaves, trace the trees first
(`SIMMY_VM_TRACE`), then look at the primitive or scope they reach.

## The tick

| Function | GC | What |
|---|---|---|
| `cSimulatorImpl::Simulate` | 80141614 | Once per frame from `ELiveMode::Update`: `min(_dt, 1/15 s) x speed` into an accumulator (max 2520 ms); one tick per 42 ms; `_dt` becomes the simulated frame time until `RestoreTrueDt` |
| `cSimulatorImpl::SimulateOneTick` | 8014095C | Tick objects, then advance the clock 2 game seconds (a game minute = 30 ticks); dispatches minute/day/time-of-day changes |
| `cSimulatorImpl::TickAllObjects` | 80140E20 | Objects in id order: reset ones waiting (sim flag 4), run enabled ones (flag 1; while paused only flag 2). A tree error stops the tick at that object; the next tick resumes from it (the simulator is then "stalled") |

Speed is global 0x10: 0 normal, -1 half, -2 x5, -3 x30. Pause is global 0x11 (flags; modal dialogs and
the pause menu set it through `H2DTarget::IncPause/ReleasePause`). Simulator state is the "Edith
globals" (0x30 shorts, scope 6 in trees; names in [`simulator.hpp`](../include/simmy/classes/simulator.hpp)
`enum Global`): clock, funds, speed, free will. `SIMMY_SIM_TRACE=1` prints the dispatches.

## Objects

| Piece | Where | Notes |
|---|---|---|
| Definitions | `ObjSelector`, `ObjectFolderImpl` ([`objectfolder.cpp`](../src/game/objectfolder.cpp), [`objects.hpp`](../include/simmy/classes/objects.hpp)) | One per `ObjDefinition` row of the "SimsObjects" quickdata (GUID, type 2 = person, 8 = portal, multi-tile master/sub index, models, states, trees). `objdef::` accessors read the big-endian row |
| The lot's objects | `ObjectModuleImpl` ([`objectmodule.cpp`](../src/game/objectmodule.cpp)) | Read from the house's `objt` (saved type -> GUID) and `ObjM` (each object's stream); ids are 1-based; `m_first` list + `m_byId`; sim flags; `PostLoad` runs load trees then kills demolished objects; `KillAllObjects` |
| An object | `cXObjectImpl` ([`object.cpp`](../src/game/object.cpp), [`objectsim.cpp`](../src/game/objectsim.cpp)) | Location (FTilePt in 1/16 tiles + level), 72 object data shorts (`EdithVariableSet`: e.g. 0x1D room, 0x22 hidden, 2/3 container, 0x1A tile-chain parent), attributes, temps, its `TreeSimImpl` stack |
| Multi-tile | `cXMTObjectImpl` ([`mtobject.cpp`](../src/game/mtobject.cpp)) | Parts of one object (doors/windows are 2-part portals; `IsDynamic` 800C71F0) |
| 3D side | `ISimsObjectModel` etc. ([`isimsobjectmodel.cpp`](../src/game/isimsobjectmodel.cpp), [`iobjectman.cpp`](../src/game/iobjectman.cpp)) | Models, states, lamps, particles, highlights |
| Iteration | `ObjectIterator` (objectmodule.cpp, `operator++` 800F2AC0) | Walks a tile's object, its tile chain (saved slot vector entry 0) and its slots |

**PC differences to know.** The port's `PlaceObject`/`SetLocation` keep containment in object data 2/3 and
"first object wins" in the tile map; GC linked a hierarchy (`HierSetSite`). Objects placed during play on
an occupied tile are therefore not in the tile chain (a window bought onto a lamp's tile misses its wall
cut). The lot's people are owned by a PC list in person.cpp (`g_lotPeople`); `KillAllObjects` calls
`RemoveFamilyFromLot()` first, or they are freed twice.

## The behaviour VM

Format (from [`treesim.hpp`](../include/simmy/classes/treesim.hpp)): a tree (`BHAV`, big-endian, read in
place) has a 12-byte header and 12-byte nodes `{opcode, true target, false target, 8 operand bytes}`.
Opcodes < 0x100 are primitives; 0x100-0xFFF global trees, 0x1000-0x1FFF the object's own file,
0x2000+ its semi-global file. Targets 0xFD error, 0xFE return true, 0xFF return false. A primitive returns
0 false, 1 true, 2 not done (runs again next tick: waiting), 3 pushed a frame, -1 error.

| Piece | Where |
|---|---|
| `Behavior` (an object's trees, constants, tree sets) | treesim.cpp |
| `TreeStack` / `StackElem` / `TreeSimImpl` (frames, Gosub, params/locals) | treesim.cpp |
| `cXObjectImpl::TryElement` 80109770 (object primitives) | objectsim.cpp; passive influence in [`passiveinfluencemap.cpp`](../src/game/passiveinfluencemap.cpp), the family inventory in [`inventory.cpp`](../src/game/inventory.cpp) |
| `cXPersonImpl::TryElement` (person primitives: 0x03 find best action, 0x06 change suit, 0x11 idle for input, 0x16 look towards, 0x1B goto relative, 0x1D motive delta, 0x1E gosub found action, 0x25 test interacting with, 0x2C animate, 0x2D goto routing slot, 0x3E social mode, 0x40 idle animate) | person.cpp, autonomy.cpp, socialmode.cpp |
| `cXObjectImpl::TryExpression` / `InterpValue` 8010AAC8 (the expression primitive and the variable scopes: my/stack object data, person data, globals, temps, params, locals, motives, room 0x17, neighbourhood...) | objectsim.cpp |
| `TryGenericSimCall` (0x01: house transitions 0x16, wardrobe 0x22, food creation 0x27, selected sim 0x1A/0x1B...) | objectsim.cpp, [`ingamecas.cpp`](../src/game/ingamecas.cpp) |
| Dialogs (`TryDialog` 0x24, `EdithDialogPrimitive`) | [`edithdialogprim.cpp`](../src/game/edithdialogprim.cpp) |
| Sounds (0x17 play, 0x30 kill) | [`soundprims.cpp`](../src/game/soundprims.cpp) |
| Balloons (0x29) | [`sprites.cpp`](../src/game/sprites.cpp) |
| Errors (`Error`, `HandleError`) | objectsim.cpp |

Unported primitives go through `Unported(op, name, addr)`: logged once, **then treated as true**, so a
tree carries on as if it worked. Still unported, and used by no tree on the disc (a scan of every
`edithtreesets` BHAV): 0x0A tutorial, 0x15 show string, 0x22 UI effect, 0x3D particle effect; person 0x2F
reach; 0x39-0x3C HUD create/add/manage/data are used only by the debug object `zz_move_a__tron` ("Debug
Mode - Test Job HUD"). Unknown opcodes set error 5. Unknown variable scopes (2, 5, above 0x2F) set error 9.

| Primitive | GC | Port notes |
|---|---|---|
| 0x09 burn | `TryBurn` 800FF520, `StartFireAtObjectLoc` 800FF2F8 | Fire spreads (efx_fire "spread to object", the genie lamp's bad wish): a new `efx_fire` on the tile, or the thing that burns there (object data 0x28 bit 0x20) catches fire; fireproof objects (data 8 bit 0x800) and fires stop it; at most 30 fires; a burning person is let go of everything first. Fires themselves are made by trees (CreateObject: stoves, tombstones, guitars...); burning objects (data 8 bit 0x200) are skipped by autonomy and FindFunctionalObject |
| 0x13 person in area | `TryIsPersonInArea` 800FA314 | Doors (door_sg): a person near the middle of the door's two sides, in either side's room, on the door's axis and facing it. GC quirks kept (a ghost ends the walk; after the first person in range the rooms compared are 0) |
| 0x1D (object) motive delta, 0x41 passive influence, 0x2C (object) animate | `TrySetMotiveDelta` 80109344, `TryPassiveInfluence` 80108EC8, `TryAnimate` 8010958C | The passive influence system ([`passiveinfluencemap.cpp`](../src/game/passiveinfluencemap.cpp), [`passiveinfluence.hpp`](../include/simmy/classes/passiveinfluence.hpp)): trash piles, bins, outhouses, stereos put a shape (wedge, half disc, hexagon, square in front) of tiles in their room on the lot's map with motive deltas; a person standing there has those motives moved every frame (from `SAnimator2::Update`, the passive-influence part of `AwarenessManager::AwarenessCheck`), up to the deltas' limits. Targets (definition +0x0A 0x1B/0x2B/0x3E) run tree entry 0x0D for an object influence of their type once a tick (`CheckObjects`). The person's reaction animation (`StartPassiveInfluenceAnimation`) is not ported; object 0x2C only records it. `SIMMY_INFLUENCE_TRACE=1` (2: motive changes) |
| 0x21 five worst motives | `TryFind5WorstMotives` 800FE068 | Temps 0..4; sorted with the game's qsort (BSD quicksort, [`sn_qsort.hpp`](../include/simmy/sn_qsort.hpp)) so ties come out as on GC |
| 0x23 user event | `TryUserEvent` 800F9F44 | Three ticks: the simulator steps once (global 0xB), then GlobalDispatch 0x108 (no listener in the game) |
| 0x27 inventory | `TryInventoryCommand` 80106A8C | The family inventory (garden, fruit tree and aquarium harvests; food creation's ingredients): [`inventory.cpp`](../src/game/inventory.cpp) over `PlayerFamilyImpl`'s slots |
| 0x33 preload | `TryPreloadObject` 80100C84 | PC: object data is read when needed, so a known GUID is ready at once (GC could wait) |
| 0x34 room effect | `TryRoomEffect` 80105F54 | Room light scale / colour, rain, fair weather; lightning (`EHouse::TriggerLightningEffect`) is empty in the release build |
| 0x36 GBA | `TryGbaCommunication` 80106568 | False, as on a GameCube with nothing connected (GC tests EyeToy hardware in one-player games) |
| 0x1F set to next, kind 9 | `TrySetToNext` 80101110 | The objects east, south, west, north of an object (counters and tables find their neighbours); `IsInteractionTile` (800E318C, flag 0x40) is a part with a "dynamic multi-tile" tree (function entry 8) |

Check-tree ad scopes (0x0C range, 0x1C personality, 0x1D min of a motive's ad): only for the TTAB entries
`IsPermittedToRunCheckTreeForAds` (801270E0) allows, autonomy copies the entry's ads
(`TTabScratchAds`, GC `TTabScratchEntry`), runs the check tree with `cXPerson::sbAllowingCheckTreeAds` set
so it can change them, zeroes the social ad when the object is in another room, and scores with the copy
([`autonomy.cpp`](../src/game/autonomy.cpp)). Elsewhere the scopes read a scratch 0, as on GC.

The tree RNG is `GetNextRandomNumber` (800A64F8, in [`cassimstate.cpp`](../src/game/cassimstate.cpp)): the
simulator's seed inside a tick, an outer seed for check trees run from menus.

## People

`cXPersonImpl` ([`person.hpp`](../include/simmy/classes/person.hpp), [`person.cpp`](../src/game/person.cpp))
= object + `cXPerson` interface (what the 3D side and camera ask: `GetPersonData`, `GetMotive`,
`GetSAnimator`, ...). Key members: `m_motives`, `m_actionQueue`, `m_animator` (`SAnimator2`),
`m_neighbor` (the persistent `Neighbor`), `m_instance` (the `ESim`).

- **Person data** (`GetPersonData`, `m_personData[0x100]`): the neighbour's 0x50 values (`Neighbor::m_pers`) are copied in at `Initialize` (personality,
  skills 9..0x12, career 0x38 / level 0x39 / performance 0x3F, censor bits 0x1E, social target 0x28,
  idle state 0 (3 = waiting: the hourglass)...).
- **Motives** (`Motives`, treesim.cpp): 16 values -100..100; 0 aspiration, 5 energy, 6 comfort, 7 hunger,
  8 hygiene, 9 bladder, 0xE social, 0xF fun. `Motives::Sim` every 60 ticks applies decay.
- **Animator** `SAnimator2` ([`sanimator2.cpp`](../src/game/sanimator2.cpp)): position in 1/16 tiles (tile
  centre at +8, already a world position: `positionCharacter` 80065094 must **not** add the house's
  0.5 offset; objects do add it in `SetObjOrient`), animation states, free-move walk/run for direct
  control, route walking, animation events (sounds, footsteps, particles, censor), passive influence on
  the motives (the awareness check's part). Stubbed: the rest of awareness (memory, head turning,
  auto-carry, passive-influence reactions),
  mood animations, skill idles, rumble.
- **Collision** (`CollisionMap`, [`collisionmap.cpp`](../src/game/collisionmap.cpp)): swept circle against
  walls and object footprints for direct control. `DoesCollideObject` uses GC's `CanIntersectPeople` (800EB6C4,
  objectplace.cpp): people pass zero-extent objects (flag 4) unless flag 2. Two steered people do not walk
  through each other (PC addition; `SIMMY_SIM_COLLIDE=0` turns it off).
- **Family on the lot**: `MoveFamilyOntoLot` (person.cpp, PC stand-in) takes the family's funds onto the
  lot and the players' sims from the people the load trees made; `SIMMY_FUNDS`, `SIMMY_SIM_START` apply here.

## Interactions and the action queue

`Interaction` ([`interaction.hpp`](../include/simmy/classes/interaction.hpp)) = one TTAB entry of an object
chosen for a person (from the menu, a push primitive or autonomy) with priority and flags. `ActionQueue`
is the person's ring of ten; the running one first.

Menu path: A in direct control -> `DirectInteractor::ExecuteActionMenu` (802057C8) ->
`FlashPiMenu::CreateObjectMenuFromObjList` over `MenuObjectsNear` (PC: objects within 2 tiles, nearest
first, max 4) -> `CollectInteractionsForObject` (TTAB walk + check trees) -> `HUDTarget::AddMenu/
ShowMenu` -> `ActionMenu` (priority-100 pad filter) -> `FlashPiMenu::ChooseAction` -> social mode, or
`StartMenuInteraction` -> `cXPersonImpl::AddAction`. In classic control the `SimInteractor` builds the
same menu from the highlighted objects. Priorities: the player's menu choices are 100; a queued action
interrupts the running one only with a strictly higher priority (`ShouldInterrupt`).

Cancel: B under the HUD goes through `H2DTarget::UpdateCancelButton` (GC `H2DTarget::Update` 801B8484):
short press -> `PersonCancelLastAction` (a PC helper in hudtarget.cpp for GC 8012606C), held 30 frames ->
the action queue widget's cancel mode. A walking sim can be interrupted: `TryGotoRoutingSlot` (80120640)
checks `ShouldInterrupt() && SAnimator2::IsInterruptable()` before and during the walk.

## Autonomy

[`autonomy.cpp`](../src/game/autonomy.cpp): `cXPersonImpl::TryFindBestAction` (80127114, person primitive
0x03). For a sim no player steers (NPCs, other family members, a player's sim after 120 s idle or while
the player is in classic control), every object's TTAB entries that may be chosen autonomously are scored
by how much their motive advertisements raise the sim's motive curves (`MotiveEffects`, nine
`PiecewiseFn` curves, defaults at 803DCC3C), attenuated with distance, plus personality ads; one of the best
few is picked at random (sim RNG). `SIMMY_AUTO_TRACE=1` (`=ads` for the ads) traces it;
`SIMMY_AUTONOMY_IDLE=<s>` changes the idle timeout.

## Wants, fears, relationships

- **Wants and fears** ([`wantfear.cpp`](../src/game/wantfear.cpp), [`wantfear.hpp`](../include/simmy/classes/wantfear.hpp)):
  data resource 33B78948 ("wantdata_gc.bin", big-endian, kept as loaded): events, trees, nodes. A
  `Neighbor` has 20 bookmarks (0..12 wants, 13..19 fears) and 7 active slots (0..3 wants, 4..6 fears).
  `Neighbor::Notify` (from primitive 0x42 and C++ events) advances them; the HUD gets icons through
  `H2DTarget::NewWantFearIcon`. `SIMMY_WANTS_TRACE=1`. Not ported: the want/fear dialog.
- **Relationships**: `RelMatrixImpl` per neighbour ([`neighbor.cpp`](../src/game/neighbor.cpp)), primitives
  0x18/0x1A, read by the relationships page (`R2LTarget`, `PlayerRelatedPeople`).

## Routing

[`xrouting.cpp`](../src/game/xrouting.cpp), [`routing.hpp`](../include/simmy/classes/routing.hpp): a
routing slot (the object's SLOT entry) -> `XRoute::ConstructGoals` (candidate spots scored, checked
against bounds, room, walls, room to stand) -> a path. **PC stand-in:** an A* over a quarter-tile grid of
the lot (walls, blocked tiles, footprints, target object excluded), smoothed by line of sight; GC used a
room partition (`SpacePartition`). One route try, no failure trees, no re-planning toward a moving target.
`SIMMY_ROUTE_TRACE=1`.

## Social mode

[`socialmode.cpp`](../src/game/socialmode.cpp): `TrySocialMode` (801282E0, person primitive 0x3E;
operand step: 0 start with the other, 4 join, 2 the social menu, 3 leave the menu, 1 end). "Socialize"
makes a `social_interactions_*` object whose trees run both sims. The player's social menu is a PC
stand-in for `SocialModeInteractor` (interactor 7). Flags live in a PC map (`Flags()`), not in the person.

GC person vtable slots used (handy when reading raw code): 0x334 IsInSocialMode, 0x33C SetFirst, 0x344
SetSecond, 0x34C IsFirst, 0x354 IsSecond, 0x35C IsWaiting, 0x364 SetWaiting, 0x36C GetSocialModeTarget,
0x374 CheckFirstPlayerForFailedSocialModeEntry (8012B8CC), 0x37C CheckSecond (8012B978); 0xF4
GetCurrentInteractionStackObject (8012640C). `DirectInteractor::Update` (80204838) runs both Check*
functions first each frame; without them a failed Socialize leaves the player stuck in social mode.

**Known difference:** the port's `IsInSocialMode` is first||second flag; GC's (8012CA2C) is "person data
0x28 != 0". They agree for the selected sim, not for the partner.

## Gotchas

- Primitives that "succeed" while unported can make trees take wrong branches silently: check the
  `[stub]` lines of a run before debugging tree logic.
- Wall-clock `_dt` makes motive values in tests jitter by a few tenths between identical runs (the suite
  in [testing.md](testing.md) allows for it).
- Never use `std::rand` in simulation code: use `GetNextRandomNumber` (tree semantics) or
  `simmy::sim_rand()` ([determinism-and-netplay.md](determinism-and-netplay.md)).
- Vtable offsets in comments are GC offsets: use `tools/vslot.py` to name them, never index C++ vtables.
- The scenario (story lot 9) and the tutorial are objects too; `SIMMY_STORY=0` keeps them still so an
  interaction can be tried on its own.
