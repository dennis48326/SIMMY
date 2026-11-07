# Bucket `inline`: gaps inside ported code

Scope: the 189 inline `SIMMY_STUB(` sites in 66 files (src/, include/, not gen/), the `Unported(...)`
primitives, comment-only behaviour found by grepping for "not ported / stand-in / PC: always / PC: none"
and similar, and the PC stand-ins that known-gaps.md names. Verdicts are in `inline_verdicts.tsv`
(211 rows: 189 stub sites plus 22 comment-only or stand-in rows).

Counts: MISSING-H 6, MISSING-M 24, MISSING-L 75, PC-replaced 63, NA-unreachable 30, NA-platform 6,
UNSURE 7.

Caveats:
- `hits_raw.txt` holds old log lines. Some sites it lists are now dead code. For example,
  passiveinfluencemap.cpp:561/568 sit in a free function nothing calls, because awarenessmanager.cpp
  ports the whole AwarenessManager.
- `TryGenericSimCall` logs the calls it lacks with `printf`, not `SIMMY_STUB`, so they never reach the
  hit logs. Coverage of those calls is unknown.

## MISSING, by priority

### H1. The Free Will option does nothing (MISSING-H, effort S)
- GC: `ELiveMode::Update` (8008E4C0) calls `cXObject::SetFreeWill` every frame. The value is the
  options byte (OptionsRecon +0, the pause menu's Free Will), unless simulator global 0x27 overrides it.
- Port: `const int sFreeWill = 1` (src/game/autonomy.cpp:42). simulator.cpp:194 says "PC: free will is
  always on". The stub at src/game/livemode.cpp:1189 mentions "free will" only in its message.
- Player: turning Free Will off in the options has no effect. Selected sims still act on their own.
- Related: generic sim call 0x15 sets global 0x27 (the override) and is not ported (see H3).

### H2. No object highlights, and the direct-control A menu picks different objects (MISSING-H, effort M)
- `ISimsObjectModel::BlendHighlights` (8004F02C, 0x62C bytes) is not ported.
  - Site: src/game/isimsobjectmodel.cpp:679 (HIT).
  - The pulse timing is computed (`UpdateHighlightAnim`), but nothing applies it to the lights.
  - Player: objects under the classic cursor never glow.
- Direct control: `UpdateObjectHighlights` (80205C4C) and `ShouldHighlightObject` (80204C5C) are stubs.
  The PC stand-in `MenuObjectsNear` (src/game/flashpimenu.cpp:397) replaces them:
  - GC takes objects within 5 tiles and people within 1.5 tiles, from the influence map. Their bounds
    must touch the arc in front of the sim (`IsRectTouchingArc`, the sim's facing). They must also have
    interactions for that player. The chosen objects are highlighted every frame.
  - Port: any object within a 2-tile circle, nearest 4, regardless of facing. Nothing is highlighted.
  - Player: A can offer objects behind the sim, misses objects 2–5 tiles ahead, and gives no visual cue.

### H3. TryGenericSimCall: many calls missing (MISSING-H as a group, effort S each)
src/game/objectsim.cpp:2971. GC 80102A24 handles these calls, which the port logs as "not ported" and
treats as true:
- 0x13 / 0x14 / 0x15: set simulator globals 0x25 / 0x26 / 0x27 (0x27 is the free-will override).
- 0x30: adds temp 0 to the player family's total aspiration score (capped at kTotalAspirationMax),
  then updates the family's unlocks. Player: aspiration reward points from these trees are lost.
- 0x05: `Spend` (expense type 2) of a neighbour's value.
- 0x1E: `SwapSelectedSims`. 0x1F / 0x20: player 1 / 2 selects the person in temp 0.
- 0x0D: `CleanupPeople(stack object)`.
- 0x21: `ShowAllObjectDialogs`.
- 0x24: ringtone sound. 0x2B: count of modeless dialogs. 0x2A: camera state test.
- 0x03 / 0x04 / 0x06 / 0x0C / 0x17: neighbourhood and object calls; 0x17 is
  `TransitionToNewHouse`-like (object vtable 0x94).
- 0x25 / 0x28 are PDA calls (NA).

Which of these the disc's trees use is unknown. Run a story and free-play soak and grep the log for
"generic sim call 0x".

### H4. TrySetToNext kinds 5 and 9 return false (MISSING-H, effort S–M)
- Site: src/game/objectsim.cpp:572. Both kinds appear in the hit logs.
- GC 80101110: kind 5 walks the neighbour ids (Neighborhood vtable 0xE4). Kind 9 walks the objects on
  the 8 tiles around a local object. Kinds 3, 7, 10 and 11 are also missing.
- Player: trees that loop over neighbours find nobody. That affects phone lists, invitations, and
  visitor or want choices. Trees that look for an object next to another (for example a chair by a
  table) fail.

### M-group: simulation behaviour
- **TagGraphicsField** (objectsim.cpp:1010, HIT, effort S).
  - GC: when an Expression writes object data 0 (scope 3 or 4), the object's 3D model restarts its
    animation (TagGraphicsField 800EB8C4).
  - Port: `m_graphicsTagged` is read by `ISimsObjectModel::Update` but never set; the method is
    declared and never defined.
  - Player: when a tree sets the same state again, the animation does not replay (a second flush, a
    door, a TV).
- **TryNotifyStackObject 0x40** (objectsim.cpp:1687, HIT, effort S).
  - GC sets the target person's idle-animate count (+0x540, `m_idleAnimateCount`) to 0, which ends its
    idle-animate.
  - Port does nothing, so the sim idles on until its own count runs out.
- **ActionSkipped** (person.cpp:964, comment-only, effort S). GC runs the object's queue-skipped tree:
  `RunTree(4, my id)`. Without it, objects cannot release what they reserved when a queued action is
  cancelled.
- **TryCreateObject "init visitor"** (objectsim.cpp:2029, comment-only, effort S). GC runs the person's
  "init visitor" tree for one tick when operand +5 flag 4 is set and the person is a visitor. The port
  skips it.
- **TryCallNamedTree from check trees** (objectsim.cpp:2733/2746, HIT, effort M). This is a PC
  workaround: the call returns true without running the named tree, because some PC primitives wait.
  Menu availability and autonomy checks that depend on those trees can differ from GC.
- **Social interest** (treesim.cpp:708, HIT; socialmode.cpp:141 stand-in, effort M).
  - `IncrementSimSocialInterest` (8012BB8C) is not called. Every motive tick, GC moves relationship
    variable 1 toward a value derived from the daily relationship.
  - `TrySocialMode` in GC seeds motive 4 from `GetSimSocialInterestAsMotiveValue(partner)` on entry and
    writes it back with `SetSimSocialIntrestFromMotiveValue` on exit. The port does neither.
  - Player: social-mode outcomes may differ. The stand-in also drops the social camera, the idle
    states and the input priority.
- **Room lighting after a tree turns a light on or off** (objectsim.cpp:418/425, HIT, effort S–M).
  `TryUpdate` what=1 should send GlobalDispatch 0xEE/0xED, and what=4 should call the house light
  update (EHouse vtable 0x8C). Lamps switched by trees may not change the lighting. Verify with a lamp.
- **Lighting grid after placing a lamp** (placemanipulator.cpp:244, comment-only).
  `RecomputeLightingGrid` is empty (GC `EHouse::ReCalcHouse` for light sources).
- **Routes after wall edits** (wallmanipulator.cpp:274, comment-only).
  - GC calls `InvalidateAllRoutes` after a wall edit.
  - The port does it only on entering live mode (house.cpp:49).
  - Sims may keep routes planned through the old walls. Check by walling off a sim while it walks.
- **Visitor/LoadHouse stand-in** (livemode.cpp:135, MISSING-L): stray people with no family and data
  0x43 != this house are not removed on load.

### M-group: CAS, sims' looks, UI
- **Clothing unlocks** (objectsim.cpp:2957 HIT, cassimstate.cpp:909, effort M).
  - `CasClothingUnlocks::UnlockGroup` is not ported.
  - CAS marks every style unlocked, with flags 0.
  - Player: locked fashion is available from the start, and goal clothing rewards change nothing.
- **CAS turning the sim with the C stick** (cas.cpp:370, HIT, effort S). GC adds stick x × 90°/s to the
  rotation (8016FF08).
- **CasSimDescriptionS2C::Verify** (simmodel.cpp:344, HIT). It does nothing, so invalid style/gender
  combinations are kept.
- **Body part welding** (simmodel.cpp:837, HIT). Seams between parts can show.
- **Belt** (simmodel.cpp:1455, HIT, effort S). The belt is always shown, even under mid or outer tops.
- **Dialog icons** (edithdialogprim.cpp:326). Icon kinds 2 (neighbour portrait), 3 (object thumbnail)
  and 4 (DialogIconTable) show nothing.
- **Family/house localised names** (family.cpp:115, HIT). `GetNghFamilyName` / `GetNghHouseName` are
  not applied after load.
- **HUD lot name** (hudtarget.cpp:268). The `HUD_lot_name` localizable is empty.
- **ISimsObjectModel::ApplyMatrix** (isimsobjectmodel.cpp:1088, HIT). Portal rooms, the outside flag
  and the render-position override are missing, so door, window and outdoor lighting can differ.
- **UserCanPlace** (objectplace.cpp:1020). Slot types 5..7 are not refused, so buy mode may accept slots
  GC forbids.
- **Classic "Go Here"** (flashpimenu.cpp:238, HIT). The target is the tile centre instead of the
  person's `FindGoodLocation`, so a blocked spot fails instead of moving to the nearest free spot.
- **Unported() primitives** (objectsim.cpp:29): 0x0A TryTutorial, 0x15 TryShowString, 0x22 TryUIEffect,
  0x3D TryParticleEffect and 0x39–0x3C HUD. known-gaps says no disc tree uses them, and the hit logs
  show none. Kept at M until a tutorial-lot run with `SIMMY_VM_TRACE` confirms.

### L (minor or unlikely; see the TSV for each)
- Engine paths never logged: float strips, headerless particles, multi-state/multi-texture shader
  clone, AddEventRange animation events, 3D trapezoid fades, Apt setMask and imports.
- `ENgcRenderer::TriStripPackedInt` without a display list is HIT. A reflective layer may be missing on
  some model; find which one.
- Object age and birth data: `UpdateAge` and `JustBorn` (objectmodule.cpp:101/527). These matter only if
  a tree reads data 0x12, 0x30, 0x2D–0x2F, 0x18 or 0x15.
- Load fix-ups:
  - PostLoad auto-reset / placeholder reset, the placeholder kill, misc flag 0x80, tree-version reset.
  - `cXMTObjectImpl` dynamic groups after load (mtobject.cpp:192).
- Placement and build details:
  - Multi-tile container side; counter-top and hedge models as plain objects (iobjectman.cpp:101/110).
  - Counter height offset; walls near objects in slots; undo of a room-breaking wall delete.
- Sprite balloon alternation period: fixed 500 ms instead of temp 0. This is a one-line fix.
- `TryAnimateObject` preload sync wait, `TryCameraControl` bone/dummy targets, `TryPickup` not closing
  the menu, `TryDialog` busy checks, saving during social mode, the check-tree advertisement copy,
  person `Cleanup` extras, person `Reset` data clear.
- CAS: the generating sound, the ambient score, bone position.
  - known-gaps lists "CAS room has no ambient score" as kept GC behaviour.
  - GC `CasScene::Init` (801653FC) does start an `AmbientScorePlayer`. Re-check this.
- Weather in `EHouse::Update`, camera room opacity, the "press start" prompt (GC clears it within a
  frame), and the leftover globals in pregame.cpp:373/410.

## UNSURE (what to run)
- **thesims.cpp:572 GamePlayShutdown** (neighbourhood unload when the main menu returns 2). Quit to the
  menu, load a different slot, and check for stale families.
- **MoveFamilyOntoLot** (person.cpp:2385). Make a free-play CAS family of 3–4 sims and check that every
  member appears. The stand-in places only the player sims unless a load tree makes the others.
- **world.cpp:457/488 InfluenceMap** is never built. GC uses it for the direct-control highlights (H2)
  and maybe routing. Grep for any remaining reader.
- **isimsobjectmodel.cpp:420 vehicles** (HIT). Some object says `IsVehicle`. Check the carpool or bus
  when it arrives.
- **inlevel.cpp:344**: grep the logs for "[inlevel] flow request".
- **interactor/input manager stand-ins** (interactor.hpp:11). GC uses priority input sessions, HUD
  filters, and interactor types 2–7. Check that input is held back while menus or dialogs are up.
  directinteractor.cpp:334 says B-hold is not ported, but `StartCancelTimer` is called; the comment may
  be stale.

## Non-MISSING verdicts
- **PC-replaced (63).**
  - GC housekeeping the port does differently: heap compaction, HDD thread, resource loader flushes,
    stage timing, preloads (`LoadLevelGlobalData`, `FreeUnusedData`), FileLists and QuickResFiles,
    SmartDataFactory.
  - Teardown order: `ELiveMode::Reset` / `QuitToMainMenu`, ChangeHouse.
  - The ObjectModule people and portal lists, now derived by scanning.
  - `PostSim`: KillQueueFlush and AdvanceSelectedPerson are done elsewhere; NeighborhoodImpl::PostSim
    is empty.
  - Input sessions and plumb bob through the PC interactors. directinteractor.cpp:138's message is stale:
    the plumb bob is set 7 lines later.
  - HUD ActionQueueHUD and filters, the options screen-position wizard, the save path stand-ins
    (lotsave.cpp: ComputeStats matches GC 800F8D44), dead code in passiveinfluencemap.cpp.
- **NA-unreachable (30).** Saves older than the port writes (object, family, tree stack and inventory
  versions), author mode and cheats, debug output (PrintStats, memory meter, DisplayTiming, bound
  rectangles), PDA flows, slot-B import, MUWrapper, NPC editor, ENCamera cuts and object camera modes,
  bound-tree levels, saved grandparents, `PresetHouseGameStatus` (written, never read in GC), Urbz
  reputation strings.
- **NA-platform (6).** EyeToy representation shaders and images, Xbox/PS2 button sets, HDDThread.
- **Stale comments found.** wallmanipulator.cpp:12 (the room want event is sent), directinteractor.cpp:138
  (plumb bob), known-gaps "the port saves no people" (people are in the module and saved via
  AddPersonToModule), siminteractor.hpp/interactor.hpp "PlumbBob not ported" (PC has plumb bobs).
