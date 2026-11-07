# Porting audit: what is still unimplemented

An audit of every unported function and every partial gap inside ported code, checked against the
GameCube release build (Nov 2026, port base90). Its job: separate what a GC player could actually reach
from what does not matter on PC (EyeToy, other consoles, dev tools, dead code, things PC code already
does), and rank the real gaps.

Raw per-item verdicts and the four area reports are in [audit/](audit/) (`*_verdicts.tsv`,
`*_report.md`). Method: a static call graph of the GC ELF (direct `bl` calls plus data references such
as vtables), the stub lines from every logged run (soaks, tests and the user's own play logs), then a
read of the raw GC code and the port caller for anything not decided by its name.

## The numbers

Of 14,600 GC functions, 8,500 are ported and 6,100 are stubs. The stubs break down as:

| Verdict | Stubs | Meaning |
|---|---|---|
| PC-replaced | 2,968 | PC code or the port's own structure does the job (file and resource I/O, strings, containers, allocators, the Apt runtime internals, renderer backend) |
| NA-unreachable | 2,720 | Never reached in the release build: 2,137 have no caller at all; the rest are dev/debug/cheat tools, the Urbz PDA screens, unused classes |
| NA-platform | 224 | EyeToy, PS2/Xbox paths (HDD thread), GBA link, GC hold-to-reset, HDTV layouts |
| **Missing** | **178** | GC behaviour reachable in normal play (37 high, 89 medium, 52 low), grouped into the features below |
| Unsure | 10 | Needs a specific run to decide (listed at the end) |

A separate pass over the 189 inline `SIMMY_STUB` sites, `Unported(...)` primitives, comment-only GC
behaviour and PC stand-ins found 105 more gaps inside ported code (6 high, 24 medium, 75 low). Those are
merged into the features below.

No silently failing Flash script call was found: every built-in method and all 378 game commands the
disc's movies use either exist in the PC runtime or are absent from the GC binary too.

## High priority: likely to be seen in normal play

| # | Gap | What the player sees | Where | Effort |
|---|---|---|---|---|
| H1 | **Done**: `cXObjectImpl::SetTemp` (800EAF0C) ported (with `GetTemp`, `SetAttr`, `GetAttr`); `ObjTestSim` puts the temps back | — | object.cpp | — |
| H2 | **Done**: `ELiveMode::Update` sets `cXObject::SetFreeWill` (800DB954) from the options every frame unless global 0x27 overrides it; global 0x1E follows | — | livemode.cpp, object.cpp | — |
| H3 | **Done**: `EGlobal::IsSaveGameEnabled` (80042718) and `IsPlayerInSocialMode` (800409D8) ported; quitting asks to save; O2T Save and the pause menu's build/buy status check social mode | — | global.cpp, memcardscreens.cpp, o2ttarget.cpp, paztarget.cpp | — |
| H4 | ~~Faces are not morphed in live mode (`SimModel::UpdateReweld` 80072CBC)~~ Ported (with `UpdateReweldCAS` 80072D3C) | (was: every sim's face and head shape lose their Create-A-Sim sliders outside CAS) | simmodel.cpp | done |
| H5 | ~~Locked buy-mode text shows a raw tag (`doUIQDStringSubstitution` 80177CA8)~~ Ported | (was: "Earn &lt;unlock_aspiration_points&gt; aspiration points…") | uitargets.cpp | done |
| H6 | ~~Tattoos never appear (`GetTattooTextureID` 8016C850; the compositor also passed slot 0 for all three)~~ Ported, slots 0/1/2 fixed | (was: a tattoo chosen in CAS never shows on the body) | cassimstate.cpp, skincompositor.cpp | done |
| H7 | **Ported.** Highlight glow is never drawn (`ISimsObjectModel::BlendHighlights` 8004F02C) | The object under the classic cursor never glows | isimsobjectmodel.cpp:679 | M |
| H8 | **Ported** (with the InfluenceMap). Direct-control targeting is a stand-in (GC: objects within 5 tiles in the arc the sim faces, people within 1.5 tiles with no wall between, highlighted every frame; needs the InfluenceMap) | A offers things behind the sim or through walls, misses things 2–5 tiles ahead, nothing glows | flashpimenu.cpp:397 `MenuObjectsNear`, directinteractor.cpp | M |
| H9 | **Ported** (PC: the wall strip as EGEVerts). Build tools draw nothing (InteractorVisualizer 80213F98–80217550, wallpaper previews, `Player::GetInteractorColor`) | No cursor, wall line, floor or wallpaper rectangle while dragging in build mode | floorpainter.cpp:313, wallmanipulator.cpp:463, wallpainter.cpp:348 | M–L |
| H10 | **Done**: `TrySetToNext` kinds 3, 5, 7, 9, 10, 11 were already in; flag 0x40 on the generic kinds, kind 0xC (in the world), kind 8's in-world check, GC's keep-or-clear target on "none left", kind 4's restart and the 0xD–0xF error added. Left: kinds 0/1/2/6/0xC/8 walk ids (GC: the module's lists) | — | objectsim.cpp | — |
| H11 | **Done**: every call id a disc tree uses (BHAV scan of edithtre.arc) is ported, plus the simple unused ones and GC's errors; 0x1A/0x1B return true as GC does (r4 = 1; the decompiler shows r3). Left: 0x01 (no tree uses it) and 0x21 (debug object only) | — | objectsim.cpp | — |

## Medium priority: visible but rarer, or cosmetic

**Simulation**
- **Done** (mS): relationships drift between socials (`IncrementSimSocialInterest` 8012BB8C from
  `Motives::Sim`); social mode loads motive 4 from the social interest on entry and writes it back on
  exit (`GetSimSocialInterestAsMotiveValue` 8012BDD8, `SetSimSocialIntrestFromMotiveValue` 8012BF48).
- **Done** (mS): an Expression writing object data 0 restarts the model's animation
  (`TagGraphicsField` 800EB8C4, `EdithVariableSet::s_bTaggedGraphicField` set by `WriteVar`).
- **Done** (mS): `TryNotifyStackObject` 0x40 ends a sim's idle-animate (count 0).
- **Done** (mS): a skipped/cancelled queued action runs its object's queue-skipped tree (entry 4,
  `ActionSkipped` 8012AD98).
- **Done** (mS): visitors made by CreateObject flag 4 run "init visitor" (`IsVisitor` 8012C8A8). PC
  order difference: the port's `MakeNeighborPerson` still pushes the person's init tree instead of running
  it at once (GC `Reset(true)`), so "init visitor" runs before it.
- **Done** (mS): `TryCallNamedTree` modes 0/1 run the named tree at once (RunOneTickTree) from check trees
  too, and a tree that does not finish is GC's error 0x44F (PC workaround removed; a `[vm] ... did not
  finish` line names the tree and node if it ever happens).
- **Done** (mS): `UpdateSimFlags` (800E2748, with `SimIndependent` 800EAF44) at the end of a new object's
  Reset (single objects and each multi-tile part) and of PostLoad: sim flag 2 (runs while paused), 8.
- **Done** (mS): `cXObjectImpl::Initialize` (800DA3D8, `EdithVariableSet::Initialize` 800E33F4) for every
  object `MakeNewOutOfWorldObject` makes (trees, buy mode, multi-tile parts) and `JustBorn`'s purchase date
  (800DAC94). Left: the rest of `Reset(true)` (data 0x3A from the definition +0x62).

**World, building and lighting**
- Sinks set in counters draw on a solid top; shrubs and hedges don't vary with neighbours
  (`ISimsCounterTopObject`, `IShrubObject`). iobjectman.cpp:101–110. M.
- Lights: a lamp switched by a tree may not change room lighting (objectsim.cpp:418/425); placing a lamp
  does not recompute the lighting grid (placemanipulator.cpp:244); walls are lit from their first end
  only (`CalcLights4` merge, eroom.cpp:1008). S–M each.
- Sims keep routes planned through walls built since (no `InvalidateAllRoutes` after a wall edit).
  wallmanipulator.cpp:274. S.
- `ApplyMatrix` drops portal rooms, the outside flag and the render-position override (door, window
  and outdoor lighting). isimsobjectmodel.cpp:1088. M.
- Buy mode does not refuse slot types 5–7 (`UserCanPlace`). objectplace.cpp:1020. S.
- Classic "Go Here" targets the tile centre instead of the nearest free spot. flashpimenu.cpp:238. S.
- Fire-code meter is always empty, so the lot object limit and its warning never happen
  (`GetFireCodeValue` 80057A1C). pcttarget.cpp:154. M — a design choice for PC: keep the GC limit or not.

**Sims' looks and Create-A-Sim**
- Clothing and tattoo unlocks are ignored: everything is open from the start and fashion goal rewards
  unlock nothing (`CasClothingUnlocks`, 21 functions). M. **Save trap:** saves the port wrote mark
  everything locked and never ran `UnlockGroup(0)`; unlock group 0 at load when this is ported.
- Body parts are not welded after part changes (`WeldSharedVertices` 80071558): seams at neck, waist,
  wrists. simmodel.cpp:837. M.
- Belts show through jackets and long tops (`UpdateBelt`). simmodel.cpp:1455. S.
- Sim descriptions are never repaired (`CasSimDescriptionS2C::Verify`): bad part/style combinations stay.
  simmodel.cpp:343. M.
- CAS: no camera zoom on the C stick (cas.cpp:681, comment only) and no turning the sim with it
  (cas.cpp:370). S.

**Interface**
- **Done** (mI): the button-code cheats (`ESimsApp::UpdateCheats` 80005448, `PlayerCheats` 80005D40–80005FFC,
  `EGlobal::InitPlayerCheats` 8004246C, `TogglePlayerCheat` 80042388, `GetPlayerCheats` 80042338,
  simulator globals 0x2E/0x2F, the main menu's reset); the 11 codes are listed in [ui.md](ui.md).
- **Done** (mI): per-letter text effects (`ERFont::ProcessFontFX` 8031702C). Note: the disc's `<ROCK_>`
  strings are mostly on screens players do not reach; `<OUTLINE_>`/`<RUMBLE_>` only in a DEBUG string.
- **Done** (mI): dialog icon kinds 2–4 (`EdithDialogPrimitive::SetDialog` 800B56DC).
- **Done** (mI): family and house names (`GetNghFamilyName` 8004032C, `GetNghHouseName` 80040414; on GC
  too "#house06" becomes "house06", no "HouseData" row has that name) and `HUD_lot_name` (no disc movie
  was seen asking for it).

## Low priority

Details are in the audit reports. In short:
- **Sims:** the fixed culling box and visibility sphere, so a lying-down sim can pop at screen edges.
- **Camera:** cuts start one frame early.
- **Performance:** skeletons are not cached.
- **Unused today:** the end-of-frame texture unbind and the 8-bit texture copies.
- **Objects:**
  - object age data never updates;
  - person reset after a tree error is partial;
  - container height offsets;
  - load integrity fix-ups;
  - door and window wall styles;
  - new CAS sims' GUIDs;
  - the grid object.
- **Building:** wall delete after a failed room preview.
- **Text:** the sprite balloon period is fixed at 500 ms.
- **Lists:** the relationship list breaks name ties by first name only.
- **Leftovers:** various small branches in the tree primitives.

## Unsure: what to run

1. **Free-play family size.** Make a CAS family of 3–4 sims and check that all of them appear on the lot. `MoveFamilyOntoLot` is a stand-in.
2. **Old-format descriptions.** A run read a description without the 0xFEED mark (`ReadOldDescription` 80167B7C). Log the source at simmodel.cpp:229. If it is disc data, this becomes high priority.
3. **Generic sim calls.** Done (H11): a scan of the disc's trees lists the ids used; `SIMMY_GSC_TRACE=1` logs first uses at run time.
4. **Dynamic multi-tile merge.** Place two flag-8 objects side by side, such as counters (800C7690).
5. **Vehicles.** Watch the carpool or bus arrive (isimsobjectmodel.cpp:420).
6. **Neighbourhood unload.** Quit to the menu, then load another slot (`GamePlayShutdown`).
7. **`EShader::ChangeMaterial`** (802FA390). Grep a long soak for it.
8. **Input held back.** Check that input stays blocked while menus and dialogs are up (interactor stand-ins).

## Corrections to earlier docs found by this audit

- **Animation events:** `EAnimController::AddEventRange` is not a gap. The release build never collects those events.
- **CAS ambient score:** the CAS room does have one. GC `CasScene::Init` 801653FC starts an `AmbientScorePlayer`, so this is a low-priority gap, not kept behaviour.
- **Saving people:** the port does save people now; they are added to the object module.
- **Stale comments:**
  - wallmanipulator.cpp:12: the room want event is sent.
  - directinteractor.cpp:138: the plumb bob is set.
  - siminteractor.hpp and interactor.hpp: "PlumbBob not ported".
- **Dead code:** passiveinfluencemap.cpp `PassiveInfluenceAwarenessCheck`. The live one is in awarenessmanager.cpp.
