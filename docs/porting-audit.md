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
| H1 | Check trees overwrite the running action's temps (`cXObjectImpl::SetTemp` 800EAF0C is a stub) | Odd branches, wrong targets or failures after opening a menu or after an autonomy check mid-action | objtestsim.cpp:36 | S (one line) |
| H2 | The Free Will option does nothing (GC sets it from the options every frame, `ELiveMode::Update` 8008E4C0) | Turning Free Will off has no effect | autonomy.cpp:42, simulator.cpp:194 | S |
| H3 | Quitting never offers to save (`EGlobal::IsSaveGameEnabled` 80042718 returns 0) | Quit always shows the "no save before quit" dialog | memcardscreens.cpp:395 | S |
| H4 | Faces are not morphed in live mode (`SimModel::UpdateReweld` 80072CBC) | Every sim's face and head shape lose their Create-A-Sim sliders outside CAS | esim.cpp:254, :395 | S |
| H5 | Locked buy-mode text shows a raw tag (`doUIQDStringSubstitution` 80177CA8) | "Earn &lt;unlock_aspiration_points&gt; aspiration points…" | pcttarget.cpp:578 | S |
| H6 | Tattoos never appear (`GetTattooTextureID` 8016C850; the compositor also passes slot 0 for all three) | A tattoo chosen in CAS never shows on the body | skincompositor.cpp:186 | S |
| H7 | Highlight glow is never drawn (`ISimsObjectModel::BlendHighlights` 8004F02C) | The object under the classic cursor never glows | isimsobjectmodel.cpp:679 | M |
| H8 | Direct-control targeting is a stand-in (GC: objects within 5 tiles in the arc the sim faces, people within 1.5 tiles with no wall between, highlighted every frame; needs the InfluenceMap) | A offers things behind the sim or through walls, misses things 2–5 tiles ahead, nothing glows | flashpimenu.cpp:397 `MenuObjectsNear`, directinteractor.cpp | M |
| H9 | Build tools draw nothing (InteractorVisualizer 80213F98–80217550, wallpaper previews, `Player::GetInteractorColor`) | No cursor, wall line, floor or wallpaper rectangle while dragging in build mode | floorpainter.cpp:313, wallmanipulator.cpp:463, wallpainter.cpp:348 | M–L |
| H10 | `TrySetToNext` kinds 5 and 9 return false (also 3, 7, 10, 11); both seen in runs | Trees that loop over neighbours find nobody (phone lists, invitations, visitors); trees looking for an object beside another fail | objectsim.cpp:572 | S–M |
| H11 | `TryGenericSimCall`: about 26 GC call ids missing, treated as true (0x30 aspiration points, 0x13–0x15 simulator globals incl. the free-will override, 0x05 spend, 0x1E–0x20 sim selection, …); 0x1A/0x1B return true where GC returns false | Lost aspiration points and other tree effects; which ids disc trees use is not yet logged | objectsim.cpp:2973 | S each |

## Medium priority: visible but rarer, or cosmetic

**Simulation**
- Relationships never drift between socials, and social mode does not load or save social interest
  (`IncrementSimSocialInterest` 8012BB8C and its get/set pair). treesim.cpp:708, socialmode.cpp:141. M.
- A tree setting the same object state again does not replay its animation (`TagGraphicsField`
  800EB8C4 never defined) — second flush, doors, TVs. objectsim.cpp:1010. S.
- `TryNotifyStackObject` 0x40 does not end a sim's idle-animate. objectsim.cpp:1687. S.
- A cancelled queued action does not run the object's queue-skipped tree, so reservations may stay.
  person.cpp:964 (comment only). S.
- Visitors skip their "init visitor" tree on creation. objectsim.cpp:2029 (comment only). S.
- `TryCallNamedTree` from a check tree returns true without running it (PC workaround), so some menu
  and autonomy checks can differ. objectsim.cpp:2733. M.
- Sim-independent objects stop while paused or in build/buy (`UpdateSimFlags` 800E2748 never sets
  flag 2 for new objects). objectmodule.cpp:156. S.
- Objects created by trees and multi-tile parts get no default object data (`Initialize` 800DA3D8).
  S.

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
- The game's button-code cheats do nothing (`ESimsApp::UpdateCheats`, `PlayerCheats`, 11 codes). M.
- Per-letter text effects missing (`ERFont::ProcessFontFX` 8031702C): 95 strings with `<ROCK_>` don't
  rock, `<OUTLINE_>` outlines are hidden, `<RUMBLE_>` doesn't shake. S–M.
- Dialog icon kinds 2–4 (neighbour portrait, object thumbnail, icon table) show nothing.
  edithdialogprim.cpp:326. S–M.
- Family and house names keep their "#" key (`GetNghFamilyName`, family.cpp:115); the HUD lot name is
  empty (hudtarget.cpp:268). S.

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
3. **Generic sim calls.** Grep a long story and free-play soak for "generic sim call 0x" to see which missing ids trees use (H11).
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
