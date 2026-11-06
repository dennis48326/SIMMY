# Camera

## Overview

Each player has a game camera, `ESimsCam` (`_gd.m_players[p].m_cam`), with its own tuning
(`ESimsCam::s_params[2]`, `SimsCameraParameters`) and its own `CameraDirector`
(`_globals.CameraDirectorOf(p)`, `EGlobal::m_cameraDirectors[2]`) that plays scripted shots (camera
animations from object trees, the story intro, social mode) and owns the player's frame effects. All in
[`camera.cpp`](../src/game/camera.cpp) / [`camera.hpp`](../include/simmy/classes/camera.hpp). The camera is
ported; what is left is listed below (all unreached or without effect in the release build).

## ESimsCam

| Function | GC | What |
|---|---|---|
| `ESimsCam::Update` | 80016DB8 | Per frame from `ELiveMode::Update`: `SimsCameraParameters::Validate`; the noise setting (5 cursor idle / 6 moving, 4 while the selected sim routes); if the director runs a shot, only place the window; else follow (mode 1 `FollowSimStiff`, mode 0 `FollowPlayerInteractor`), then `HandleRotation`, `HandleZoom`, `PcKeyboardMouseCamera`, then `UpdateWin` |
| `HandleRotation` / `HandleZoom` | 8001709C / 800172E8 | C stick x turns (squared response, faster past a threshold; inverted per control mode by the options), y zooms; both pull the target toward an off-screen cursor (`PullTargetToCursor`, PC helper for the shared tail: 1% / 10% of the step, zoom only when zooming in) |
| `ApplyPan` / `ApplyZoom` | 80016FB8 / 80016FFC | Used by the C stick and by the mouse |
| `FollowPlayerInteractor` | 80016C34 | The classic cursor moves freely inside its **cursor area** (`m_cursorLimit`, +0x360); when it leaves it the target moves 60% of the overshoot per frame (`s_newControlParms` +0x7C) |
| `ValidateCursorPosition` | 8001942C | The cursor area: the ground under a screen trapezoid (`E3DWindow::Get3DQuadFromScreenTrapAtZ`; top 5% / bottom 90% of the screen, top width 0.5..0.45 by zoom), the far corners put 3..5 tiles beyond the near ones. Run after the interactors each frame (`UpdateDirectInteractors`, as `InteractorManager::UpdateActivePlayerInteractors`); never pins the cursor with the shipped tuning |
| `CenterOnSelectedSim` | 80018824 | Onto the selected sim (pelvis bone); in live mode the target is pulled 30% of the way the cursor is outside its area |
| `InterpolateActualCameraParamsToTarget` | 80018548 | The camera **eases** to its target (stiffnesses by zoom, 800184A0..); turns motion blur off when a director interpolation reaches the zoom |
| `SetWinPos` | 80019A04 | The rig (eye from the eased parameters) goes to the `CameraMotionSystem` (800160B8..), which snaps (type 0; nothing sets the rubber-band type 1); noise is added (`ApplyNoise`) |
| `SetActiveNoiseSetting` / `ApplyNoise` | 8001A328 / 8001A424 | Camera noise (random walk or noise animation). With the shipped tuning no setting is enabled and all amplitudes are zero, so the eye never moves |
| `ForceTarget` / `ForcePosition` / `SetPosAndReset` | 800192FC / 8001916C / 80018E74 | Cuts (Y back to the sim, `ImmediatelyCutCursorAndCameraToPos`), the director's `CalcSimsCam` |
| `UpdateWin` | 80017490 | Projection (horizontal FOV scaled by height/width), look-at, the eye's room (`CalRoomId`, then the room's lighting group) and `m_roomWallsOpaque`, the ground area around the target (+0x390) |
| `PcKeyboardMouseCamera` | PC | Mouse turn/zoom/pan from the frame's `KbmTick`, only where the C stick reaches the camera (`IsStickFiltered(1, ..., m_filterId)`); see [input-and-control.md](input-and-control.md) |

The camera reads the C stick through its own controller filter. Menus that own the stick hide it; the
build tools and the place/grab manipulators let the camera read the C stick unfiltered
(`SetCameraFilter`, [`buildtools.cpp`](../src/game/buildtools.cpp)). In two-player games
`ForceFullScreenViewport` and `m_fullScreen` decide whether a player's view takes the whole frame
(`ELiveMode::DrawMain`; the intro camera sets `m_fullScreen` while its shot plays).

## CameraDirector

Shots: `StartAnim` (camera animation relative to `m_animBase`; node 0's matrix is the camera; it runs at
the simulator's speed in live mode), cuts and interpolations (`SetCameraCut`, `SetCameraInterp`),
`LetterBox`, `ReleaseToGameCamera` (with +0x320 set, `CalcSimsCam` works out the game camera's own view
first and the camera eases there from the shot), `InterpToGameCamera` (8001C69C), `InterpToCancelCamera`
/ `CalcCancelCam` (a shot the stick cancels, `CheckCancelled`), `ForceDisableHud`, `Disable/EnableControls`
(a controller filter masking the left stick while a shot plays). `Draw` (8001DFE0) draws the **letterbox**
(black top and bottom fifths) while a shot plays with `LetterBox` on; the HUD and H2D are hidden then too
(`HUDTarget::ShouldHUDBeVisible`, `H2DTarget::ShouldH2DBeVisible`). A shot started from an `AnimRef` runs
its animation's notes (`ProcessPropertyEventTags` -> `AnimEventHandler`: sounds, loading-screen events,
frame-effect notes via `anon::HandleEffectNote` 80015A50). Its `FrameEffectsManager` (created in the
director's constructor) holds the player's motion blur, bloom and depth of field ([rendering.md](rendering.md));
the social stance animations' notes turn DOF on. Shots are started by object trees (primitive 0x28
`TryCameraControl`, objectsim.cpp) and by live mode's **intro camera**.

## The lot intro camera

`ELiveMode::UpdateIntroCamera` (8008DF28), `StartIntroCamera` (8008FBFC), `EndIntroCamera` (8008FE28),
in [`livemode.cpp`](../src/game/livemode.cpp): when a family has just moved in (or comes back from CAS,
`EGlobal` +0xB0 = the CAS mode, set by `CASTarget`) and the lot is "recently unlocked" (player family
unlock type 2), the director plays `cam_intro_lotNN_story_part01` (or `cam_transition_cas_exit`,
attached to the sim) letterboxed, then holds up to 10 s of unpaused simulation and gives the camera
back. It waits for `ObjectModuleImpl::IsFamilyMemberAwakeAndVisible` (800F69BC) and the sim's instance
before the loading screen goes; the lot's "recently unlocked" bit is cleared, so each story intro plays
once. Otherwise both directors and cameras are reset. `SIMMY_CAM_TRACE=1` prints its steps.

## Other users of the camera

- Audio listeners: one per player camera (eye/target), summed ([audio.md](audio.md)).
- The mouse's floor point uses the view as drawn (`kbm_note_view`), not the camera object.
- Object/wall fades use the camera's point of interest (`SetupInterestFade`).
- Two players: each camera is updated with its own director selected (`_globals.SetCameraDirector`,
  `SetCam`); code that asks `_globals.GetCam()` gets the camera of the view being updated or drawn.

## Still stubbed / not reached

| Missing | Why |
|---|---|
| `FollowSimStiff`'s unsmoothed path (`LineCircleIntersect`) | Only with tuning +0x04 = 0; the shipped tuning smooths and nothing changes it |
| Animation modes 1-4 (`SetAnimationMode`, `GetObjectPosition/Orientation`) | Never called in the game |
| `SetCameraCut` / `SetCameraInterp` with an `ENCamera` | The game only passes none (`TryCameraControl`) |
| `ERoom::IsRoomWallsOpaque/IsRoomRoofOpaque` and `InitRoomLookupTab` | `m_roomWallsOpaque` / `m_roomId` are written but read by nothing in the release build; the ERoom functions stay stubs (0) |
| `CameraDirector::AnimEventHandler` event numbers 101-103 (`DAT_80476230 = -1`) | Logged |
| `cXPersonImpl::IsSleeping` (used by `IsFamilyMemberAwakeAndVisible`) | Person field +0xD4 not identified: everyone counts as awake |
| `DrawDebug`, `DrawCameraInfo`, `Quad::Draw` | Debug drawing |

## Gotchas

- `ESimsCam` positions are in the world frame (x = tile y). The animation format (1/16 tiles) is the
  animator's; do not mix them ([build-buy-and-world.md](build-buy-and-world.md)).
- The camera eases using `_dt`, so in normal (non-det) mode its path depends on frame timing.
- Sim switching releases the director's current camera (`CameraDirector::ReleaseCurrentCamera`) and
  centres the camera on the new sim via `H2DTarget::SelectedPersonChanged`.
