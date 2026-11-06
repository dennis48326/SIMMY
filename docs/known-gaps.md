# Known gaps, kept GC behaviour, limitations

What is still missing or different, as of the current tree (checked against the code: `SIMMY_STUB`
calls, `Unported(...)` primitives, `gen/functions.tsv`). For the full list of unported functions run
`python tools/progress.py` and look at the `[stub #n]` lines of a run of the path you care about.
About 55% of the game's functions are ported (8064 of 14600 in `gen/functions.tsv`); much of the rest is
unreachable in this build (PDA, EyeToy, Urbz leftovers, Xbox/PS2 code) or replaced by PC code.

## Possible bugs found while writing these docs (worth a look)

- `SimInteractor::OnCommandPressed` ([`siminteractor.cpp`](../src/game/siminteractor.cpp)): B (command 4)
  calls `impl->CancelLastAction()`, which is still the **generated stub** (8012606C is only referenced as
  "GC 8012606C" in hudtarget.cpp's `PersonCancelLastAction`, so the generator keeps stubbing the member).
  In classic control the HUD's B path may cover it; not verified.
- Same function: after Y it logs a `SIMMY_STUB` for `CheckNewSimInSocialMode` (80213C60) although
  `InteractorModule::CheckNewSimInSocialMode` is ported in [`familyselect.cpp`](../src/game/familyselect.cpp)
  and `DirectInteractor` calls it.
- Stale header comments (they say "not ported" for ported code): person.hpp/person.cpp, livemode.cpp,
  hudtarget.hpp, aptviewer.cpp, objects.hpp, interaction.cpp/hpp, `thesims.cpp`'s NewState
  comment, person.cpp's `PlayerSimDescription` comment (roommate insertion is ported in pregame.cpp).

## Simulation

- Unported primitives (logged once, then **treated as true**), none used by a tree on the disc: 0x0A
  tutorial, 0x15 show string, 0x22 UI effect, 0x3D particle effect, person 0x2F reach; 0x39-0x3C HUD
  primitives (only the debug object `zz_move_a__tron`). `TrySetToNext` kinds other than 0-12 return
  false.
- Passive influence: the person's reaction animation (`AwarenessManager::Start/StopPassiveInfluenceAnimation`)
  and the rest of the awareness manager (memory awareness, head turning, auto-carry in the rain).
- `TryUserEvent` formats its string (`ParseUIString`) without showing it, as GC does; `TryPreloadObject`
  never waits (object data is read on demand).
- Placement events (object trees on place/pick-up), slots in buy mode, the grid object (GUID 0x437).
- `SAnimator2`: awareness, mood animations, skill idle animations, low-motive runs, rumble notes.
- Routing is a PC A* (one try, no failure trees, no re-planning toward a moving target).
- Social mode flags live in a PC map (not in the person, not hashed); `IsInSocialMode` = first||second
  flag, GC = person data 0x28 != 0 (differs for the partner).
- `cXPersonImpl` constructor/Initialize/PostLoad parts (dying persons, old-save relationship pruning),
  service NPC records (`IsPet`, `IsMale` for NPCs), `CalcHappy` curves, `Interaction` names
  (`GetName`/`SetName`).
- `NeighborhoodImpl`: constants use defaults; `Save`'s simulator part, `AddToFamily`/`RemoveFromFamily`
  for people on the lot, visitor types.
- `HouseImpl` is a stand-in (house.cpp + lotsave.cpp); objects placed on an occupied tile are not linked
  into the tile chain (a window bought onto a lamp's tile misses its wall cut).
- `MoveFamilyOntoLot`, `MenuObjectsNear`, the interactor manager, input manager and `SocialModeInteractor`
  are PC stand-ins.

## UI

- Unreachable in this build, not ported: the PDA (flows 0xB-0x14), OPT (flow 0x16; its movie is not on the
  disc), E2E/EyeToy, slot-B import, HDTV layouts, MUWrapper/DlgWrapper, cheats/author mode (the pristine
  reset is ported, `SIMMY_PRISTINE_SAVE`).
- `HUDTarget`: `ActionQueueHUD` (the PC queue widget is `ACTTarget` via H2D: same behaviour) and
  `AttachControllers` (a filter for commands nothing reads on PC).
- Want/fear dialog (`Neighbor::SpawnWantFearDialog`), want/fear events on purchases and wall building.
- `O2TTarget`: the screen-position wizard (`UseOptionsScreenAdjustXY`; the PC scales the picture itself).
- CAS: representation shaders (EyeToy), the generating sound, saved grandparents (dev-tool data only).
- Mouse: no clicking on individual buttons (the game has no pointer); build/buy cursors are stick-driven.

## Rendering and camera

- With MSAA, CPU readback of render surfaces/EFB returns nothing (screenshots use a resolve path).
- Render surfaces render at their own pixel size, not the internal resolution.
- `hud=edges` is heuristic (split pieces can separate; slide-ins can switch anchor mid-animation; masks
  anchor with their own instance).
- 16:9 follows the window's own aspect (aurora has no `VILockAspectRatio`): no letterboxing in 16:10.
- Text placed in screen space from 3D positions would be in the 4:3 mapping.
- Camera: ported; left are paths the release build never reaches (unsmoothed follow, object animation
  modes, `ENCamera` cuts) and the camera's room opacity (`ERoom::IsRoomWallsOpaque/IsRoomRoofOpaque`,
  written but never read) ([camera.md](camera.md)). `cXPersonImpl::IsSleeping` is a stub, so the intro
  camera's "a family member is awake" wait treats sleepers as awake. CAS morph/portrait paths depend on
  aurora readback (headless: blank).
- ESim: the model cache / family info in `initModel`, costume skin-change flushing, the green ghost light.
- Walls: `ERoom::InitRoomLookupTab` (wallpaper preview/cost; the camera's opaque-room flag has no reader), deleting a wall
  does not kill its door/window (`KillArchitecturalObject`). Bound-tree level files (versions 0/1).
- A minimised window may accumulate GX work written outside `CanDraw` (untested on Windows).

## Audio

`AmbientScorePlayer::SetPause` pauses only the first valid sound (GC quirk kept); `IsChild` false for
service NPCs; rumble notes.

## GameCube behaviour deliberately kept

- Wall-clock timing in normal play (`_dt` from measured frames, Apt on the wall clock); the game never
  runs faster than 60 Hz.
- Food creation, object dialogs, pause and reward moments are modal for **one** player exactly as on GC
  (food creation is private per player only in co-op: [coop-and-players.md](coop-and-players.md)).
  Object dialogs and the pause menu stay shared in co-op.
- Switching sims keeps the running action; a busy partner at priority 100 makes Socialize wait (the
  hourglass shows it).
- The classic/direct control mode is not saved with the game; the pause menu remembers its last tab.
- The story family starts with §0 on lot 9; the plumb bob's particles never show (release build stops
  them); bloom has no notes in the shipped data; lightning is empty (`EHouse::TriggerLightningEffect`,
  room effect 0x34 kind 2); GBA communication (0x36) is false without EyeToy hardware; the CAS room has no ambient score.
- In two-player games audio is one stereo mix of both cameras' listeners; full-stage dialogs show for
  both players (in both windows with `split=windows`).
- `ProcessFrameEffects` and EGraphics bloom/blur parameters are empty in the release build.
- "call of undefined method" in movies is silent (`SIMMY_APT_WARN=1` to see).

## Known limitations of PC features

- Two windows: if the windows differ in size the EFB uses the larger for both halves (the smaller is
  downsampled); the first frame after entering/leaving the split view is at the previous EFB size; window
  2 is not vsynced (may tear without mailbox); no F1 and no keyboard in window 2; Alt+Enter and
  multi-monitor placement untested on real displays.
- Food creation in co-op: one at a time (one ING instance); costs game time; meanwhile the other player
  cannot pause, use a wardrobe or leave the lot.
- F1 overlay: movies keep playing under it; in 16:9 "extended" Apt art updates only when the game resumes.
- Saves are made by the port's stand-ins; GC compatibility of saves written by the port is not a goal.
- Netplay: see [determinism-and-netplay.md](determinism-and-netplay.md) (cross-compiler floats unverified).
- Windows-only code (crash handler, launcher process/env handling, two-window present on D3D12) was
  written on Linux and only syntax- or review-checked there; MSVC builds are the user's.
