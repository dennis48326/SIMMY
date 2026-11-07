# Known gaps, kept GC behaviour, limitations

What is still missing or different, as of the current tree (checked against the code: `SIMMY_STUB`
calls, `Unported(...)` primitives, `gen/functions.tsv`). For the full list of unported functions run
`python tools/progress.py` and look at the `[stub #n]` lines of a run of the path you care about.
About 58% of the game's functions are ported (8463 of 14600 in `gen/functions.tsv`); much of the rest is
unreachable in this build (PDA, EyeToy, Urbz leftovers, Xbox/PS2 code) or replaced by PC code.

## Possible bugs found while writing these docs (worth a look)

- Stale header comments (they say "not ported" for ported code): livemode.cpp, hudtarget.hpp,
  aptviewer.cpp, `thesims.cpp`'s NewState comment.

## Simulation

- Unported primitives (logged once, then **treated as true**), none used by a tree on the disc: 0x0A
  tutorial, 0x15 show string, 0x22 UI effect, 0x3D particle effect, person 0x2F reach (it should use the router); 0x39-0x3C HUD
  primitives (only the debug object `zz_move_a__tron`). `TrySetToNext` kinds other than 0-12 return
  false.
- `TryUserEvent` formats its string (`ParseUIString`) without showing it, as GC does; `TryPreloadObject`
  never waits (object data is read on demand).
- Placement: a multi-tile container's side with room is not chosen for a surface object; the grid object
  (GUID 0x437: the port draws its model, the object is not made).
- `SAnimator2`: `TryIdleAnimate` syncing an object, `TryAnimate` stopping the idle track, event notes
  of kind 9; animation events of phase-locked tracks (`EAnimController::AddEventRange`).
- A dead player sim stays on the lot as a ghost (data 0x4A flag 4, now read): its autonomy offers only
  ghost entries (TTAB flag 0x10), in the tutorial lot that is "Shoo" with score 0, chosen again every
  few frames (seen in a 1P soak after the sim starved alone; no crash).
- Routing: portals are type-8 multi-tile parts without `cXPortalImpl` (scores in `m_portalValues`);
  `TrySnap` still uses the PC `PersonCanStandAt`. The GC leaves a "move out of the way" object in the
  world when it cannot be used (kept).
- `cXPersonImpl`: `Reset` (8011EB78), motive curves from a string set file (no definition on the disc
  names one).
- GlobalDispatch has no listener list: the house's messages go to `HouseImpl::DoCommand` through
  `HouseDispatch` where the port broadcasts them (0xEC, 0xED, 0xEF, 0xDE, and the simulator's 0xD8 / 0xD9).
  Other Commanders (the world, the object module, families) are not listeners.
- `HouseImpl` loading and saving (`Initialize`, `LoadFile`, `SaveFile`: the score curves, the name) are
  the world side's stand-ins (livemode.cpp `BGExec_LoadHouse`, lotsave.cpp); `ObjectModuleImpl::ComputeStats`
  is a stand-in (lotsave.cpp). The port saves no people, so an object chained under a person is its tile's
  object again at load.
- `MoveFamilyOntoLot`, `MenuObjectsNear`, the interactor manager, input manager and `SocialModeInteractor`
  are PC stand-ins (the interactors are tied to the PC keyboard/mouse and co-op layers).

## UI

- Unreachable in this build, not ported: the PDA (flows 0xB-0x14), OPT (flow 0x16; its movie is not on the
  disc), E2E/EyeToy, slot-B import, HDTV layouts, MUWrapper/DlgWrapper, cheats/author mode (the pristine
  reset is ported, `SIMMY_PRISTINE_SAVE`).
- `HUDTarget`: `ActionQueueHUD` (the PC queue widget is `ACTTarget` via H2D: same behaviour) and
  `AttachControllers` (a filter for commands nothing reads on PC).
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
  written but never read) ([camera.md](camera.md)). CAS morph/portrait paths depend on
  aurora readback (headless: blank).
- Walls: the wallpaper preview drawing (`ERoom::DrawWallpaperPreview`; the build tools draw nothing) and
  its per-room buffers; `ERoom::GetWallPaperCost` (no caller in the release build). Bound-tree level files
  (versions 0/1; not reached by any level loaded in the logged runs).
- A minimised window may accumulate GX work written outside `CanDraw` (untested on Windows).

## Audio

`AmbientScorePlayer::SetPause` pauses only the first valid sound (GC quirk kept). Rumble notes drive the
pad motor through `EVibrate` and aurora's `PADControlMotor` (one on/off motor, as on GC).

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
