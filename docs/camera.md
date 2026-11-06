# Camera

## Overview

Each player has a game camera, `ESimsCam` (`_gd.m_players[p].m_cam`), with its own tuning
(`ESimsCam::s_params[2]`, `SimsCameraParameters`) and its own `CameraDirector`
(`_globals.CameraDirectorOf(p)`, `EGlobal::m_cameraDirectors[2]`) that plays scripted shots (camera
animations from object trees, the story intro, social mode) and owns the player's frame effects. All in
[`camera.cpp`](../src/game/camera.cpp) / [`camera.hpp`](../include/simmy/classes/camera.hpp). The file's
header comment predates most of the port: the game camera's follow, rotation, zoom and smoothing are
ported; the remaining stubs are listed below.

## ESimsCam

| Function | GC | What |
|---|---|---|
| `ESimsCam::Update` | 80016DB8 | Per frame from `ELiveMode::Update`: if the director runs a shot, only place the window; else follow (mode 1 `FollowSimStiff`, mode 0 `FollowPlayerInteractor`: the cursor or the selected sim), then `HandleRotation`, `HandleZoom`, `PcKeyboardMouseCamera`, then `UpdateWin` |
| `HandleRotation` | 8001709C | C stick x turns (squared response, faster past a threshold); inverted per control mode by the options (OptionsRecon +0x18 classic, +0x24 direct) |
| `HandleZoom` | 800172E8 | C stick y zooms within the tuning's range; tilt follows zoom (`CalcTiltFromZoom`) |
| `ApplyPan` / `ApplyZoom` | 80016FB8 / 80016FFC | Used by the C stick and by the mouse |
| `InterpolateActualCameraParamsToTarget` | 80018548 | The camera **eases** to its target (stiffnesses by zoom, 800184A0..); turns motion blur off when a director interpolation reaches the zoom |
| `GetFov` / `SetWinPos` / `UpdateWin` | | Horizontal FOV scaled by height/width; the window (`E3DWindow m_win`) placed for the view |
| `PcKeyboardMouseCamera` | PC | Mouse turn/zoom/pan from the frame's `KbmTick`, only where the C stick reaches the camera (`IsStickFiltered(1, ..., m_filterId)`); see [input-and-control.md](input-and-control.md) |

The camera reads the C stick through its own controller filter. Menus that own the stick hide it; the
build tools and the place/grab manipulators let the camera read the C stick unfiltered
(`SetCameraFilter`, [`buildtools.cpp`](../src/game/buildtools.cpp)). In two-player games
`ForceFullScreenViewport` and `m_fullScreen` decide whether a player's view takes the whole frame
(`ELiveMode::DrawMain`).

## CameraDirector

Shots: `StartAnim` (camera animation relative to `m_animBase`; node 0's matrix is the camera), cuts and
interpolations (`SetCameraCut`, `SetCameraInterp`), `LetterBox`, `ReleaseToGameCamera`,
`InterpToGameCamera` (8001C69C: the game camera eases back from the shot), `ForceDisableHud` (one player
only), `EnableControls`. Its `FrameEffectsManager` (created in the director's constructor) holds the
player's motion blur, bloom and depth of field ([rendering.md](rendering.md)); the social stance animations' notes turn DOF
on (`SAnimator2` effect notes -> `HandleEffectNote`; `TurnOffSocialModeEffects` turns it off). Shots are started by object trees (primitive 0x28
`TryCameraControl`, objectsim.cpp) and by live mode's intro camera (`ELiveMode::UpdateIntroCamera`, PC
stand-in that also does GC's `LetterBox(false)` for both directors).

## Other users of the camera

- Audio listeners: one per player camera (eye/target), summed ([audio.md](audio.md)).
- The mouse's floor point uses the view as drawn (`kbm_note_view`), not the camera object.
- Object/wall fades use the camera's point of interest (`SetupInterestFade`).
- Two players: each camera is updated with its own director selected (`_globals.SetCameraDirector`,
  `SetCam`); code that asks `_globals.GetCam()` gets the camera of the view being updated or drawn.

## Still stubbed (`SIMMY_STUB` in camera.cpp)

| Missing | Effect |
|---|---|
| `CameraMotionSystem` (800164D4) | The free camera's motion system; the ported follow/ease covers normal play |
| Camera noise (`SetActiveNoiseSetting` 8001A328, `ApplyNoise` 8001A424) | No idle sway (needs the camera noise animations) |
| Off-screen cursor pull in `HandleRotation`, cursor area in `UpdateWin` | Classic cursor only |
| `ESimsCam::CalRoomId`, `ERoom::IsRoomWallsOpaque/IsRoomRoofOpaque` | Only the "inside an opaque room" camera limits |
| `FollowSimStiff`'s unsmoothed path | The smoothing camera is the default |
| `ResetPos`: moving the player interactor to the selected sim | |
| Director: controller filters (Disable/EnableControls), stick cancel check, bloom/blur note-track interpolation, AnimRef property events, simulator-speed sync, `ENCamera` matrices for cuts/interps, `CalcCancelCam`, `ReleaseToGameCamera`'s `CalcSimsCam` branch, `ReleaseCurrentCamera`'s motion blur off | Rare scripted-shot details |

## Gotchas

- `ESimsCam` positions are in the world frame (x = tile y). The animation format (1/16 tiles) is the
  animator's; do not mix them ([build-buy-and-world.md](build-buy-and-world.md)).
- The camera eases using `_dt`, so in normal (non-det) mode its path depends on frame timing.
- Sim switching releases the director's current camera (`CameraDirector::ReleaseCurrentCamera`) and
  centres the camera on the new sim via `H2DTarget::SelectedPersonChanged`.
