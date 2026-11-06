# Input and control

## Overview

All game input is **four GameCube pads**, read once per frame in `ENgcControllerManager::Update`
(8032DD60, [`e_ngcctrlmanager.cpp`](../src/engine/e_ngcctrlmanager.cpp)) from aurora's PAD (SDL
gamepads). PC additions (keyboard and mouse, scripted test input, the F1 overlay's input block, record/
replay) all act on that same pad array at that one point, so everything the game reads can be recorded
(`SIMMY_DET_RECORD`). The pads become per-controller state (`EController`), mapped to players, turned
into **controller commands** by a command map, filtered by priority stacks (UI button filters, the
camera's filter), and finally into **interactor commands** by each interactor's input session.

## The pipeline

```
aurora PADRead (SDL gamepads, port 1 = first controller; ports 2-4 auto, set in pc_main.cpp)
 -> kbm_pads          keyboard as a pad on port 1 + this frame's KbmTick (pc_kbm.cpp)
 -> dev_pad_script / dev_pad_gate / dev_pad_soak   test input (pc_devtools, padgate, pc_soak)
 -> overlay_filter_pads   F1 overlay open: neutral pads (err kept, so no "disconnected")
 -> det_pads           SIMMY_DET_RECORD writes / SIMMY_DET_REPLAY replaces (pc_det.cpp)
 -> PADClampCircle, per-port ENgcController state (buttons in EA's bit layout), EControllerManager::Update
```

| Class | File | Role |
|---|---|---|
| `EController` | [`e_ctrl.cpp`](../src/engine/e_ctrl.cpp), [`econtroller.hpp`](../include/simmy/classes/econtroller.hpp) | One port: buttons/sticks, a stack of **filters** (each hides buttons from everything below it), the command map, auto-repeat, rumble. Button bits are EA's, not the SDK's (`ENgcController::GetButtons`) |
| `EControllerManager` | [`e_ctrlmanager.cpp`](../src/engine/e_ctrlmanager.cpp) | Ports, player -> port mapping (`MapPlayerToController`, `GetPlayerControllerIndex`, `GetPlayerMapped`) |
| Command map | `g_simsCtrlCommands` in [`appmain.cpp`](../src/game/appmain.cpp) (59 entries, from the ELF) | Controller command number -> EA button mask (`MapControllerCommands`) |

**Players and pads.** A player is mapped to the first pad that gives the accept command on the "press
start" screens (`EGlobal::TryAutoMapPlayer` 80042614, called from `ESimsApp::Update` while the front end
asks for it). With `p1_input=keyboard_mouse` player 1 always maps to port 1 (the keyboard) and player 2
never does. Unplugged pads raise the game's "reconnect" handling (GameData +0x30/+0x34).

**UI filters.** `AptViewer` keeps per player a stack of up to 10 button filters (`m_filters[2][10]`,
[`aptviewer.cpp`](../src/game/aptviewer.cpp): `PushAptButtonFilter`/`Pop...`/`AddCmdTo...`). A screen
that takes input pushes one for its player's pad; the action menu pushes a priority-100 filter. In game,
filters only work while AptViewer +0x3A8 is set (set by `LiveModeInitState::Reset`, cleared by
`ELiveMode::QuitToMainMenu`): without it every A leaks to the direct interactor. Dialogs: while a G2D
dialog is up the manipulators read no pad and the simulator is paused.

## Interactors

A player's active interactor decides what the stick and A/B do in live mode (numbers are GC's):
0 direct control, 1 classic cursor, 2 grab, 3/4/5 build tools, 6 place, 7 social menu. PC stand-ins replace
the interactor manager and input manager: `UpdateDirectInteractors` (directinteractor.cpp, called by
`ELiveMode::Update`), `PlayerInteractorType`, `ChangeActiveInteractor`, `UpdateSimInteractor`
([`siminteractor.cpp`](../src/game/siminteractor.cpp)), `UpdatePlaceManipulator` (build/buy,
[build-buy-and-world.md](build-buy-and-world.md)).

Each interactor maps controller commands to its own commands (tables in the ELF):

| Interactor | Table | Mapping |
|---|---|---|
| Direct (`DirectInteractor`) | 803FCA60 | 0x31 Z -> 0x11 (classic control); 0xE Y -> 0xD (next sim); 0xD -> 0xE (previous sim); A -> 0; B -> 4 |
| Classic (`SimInteractor`) | 803FDC74 / 803FDC9C | 5 -> 0 (A), 0x11 -> 1 (Y: cursor to the sim), 0x30 Z -> 0x10, 0xD <-> 0xE; left stick -> 7/8, C stick -> 0xB/0xC |
| Place / grab | 803FD970 | 5 -> 0 (A), 0x2E -> 1 (sell), 6 -> 4 (B), 0x13/0x14 -> 5/6 (rotate) |
| Build tools | 803FE0F8 / 803FE118 | 5 -> 0, 6 -> 4, 0x2F -> 1 (Y), 0x17 -> 2 (triggers); sticks -> 7/8, 0xB/0xC |

Controller command 0xD ("previous sim") is bound to button bit 1, which no pad button produces (GC too),
so Y cycles forward round the family.

### Direct control ([`directinteractor.cpp`](../src/game/directinteractor.cpp))

The left stick sets the animator's desired state and direction (walk/run cut at 0.98; Shift on the
keyboard gives half deflection = walk). `OnCommandPressed` (8020506C): A opens the action menu
(`ExecuteActionMenu` 802057C8), B cancels all actions (`[sim] player N cancels: n`), 0x11 switches to
classic control, 0xD/0xE switch sims. A, 0x11 and the switch are **ignored while the selected sim is in
social mode** (the GC gate). `Update` (80204838) first runs the failed-social-mode-entry checks
([simulation.md](simulation.md)). After 120 s without input (`SIMMY_AUTONOMY_IDLE`) the idle player's sim
is handed to autonomy. Under the HUD, B is read by `H2DTarget::UpdateCancelButton` instead (short: cancel
the last action; held 30 frames: the action queue's cancel mode).

### Classic (cursor) control ([`siminteractor.cpp`](../src/game/siminteractor.cpp), [`siminteractor.hpp`](../include/simmy/classes/siminteractor.hpp))

Z toggles. The stick moves a cursor over the lot (tile square, fading trail, beam in the player's
colour); objects near it light up (`UpdateObjectHighlights`); A opens their menu there (empty floor:
immediate Go Here); Y brings the cursor back to the sim. The mode is remembered per player in EGlobal
+0xB8 (`m_usedSimInteractor`) across pause, build/buy and lot changes, cleared by `PreGameState` on each game
start (not saved; GC). The action queue widget shows only in classic control. While a player is in
classic control `PlayerDirectInteractor(p)` is null, so autonomy may take that player's idle sim (GC).

### Switching sims

Y -> `EGlobal::AdvanceSelectedPerson` (80040CC8) / `SelectNextInFamily` (80040AE0, [`familyselect.cpp`](../src/game/familyselect.cpp)):
skips sims not on the lot, frozen (data 0x22) or selected by the other player (`SimInUseByOtherPlayer`)
-> `EGlobal::SetSelectedPerson` (800407DC, [`global.cpp`](../src/game/global.cpp)): animator control, HUD
queue access, `H2DTarget::SelectedPersonChanged` (801B8BA4: motives, want icons, camera centre) -> the
interactor's person, `SnapToPos`, and half a second later `CancelQueuedActions` (80204F3C): queued actions
go, **the running one stays**. A sim busy with a simulator-controlled action (e.g. "Be Talked With") does
not respond to the stick until B or a new action (GC behaviour). Everything per player reads
`_globals.GetSelectedPerson(p)` every frame; nothing else caches the person. `SIMMY_SWITCH_TRACE=1`.

## Keyboard and mouse ([`pc_kbm.cpp`](../src/platform/pc_kbm.cpp), PC only)

User-facing behaviour, defaults and settings are in the repo README ("Controls: keyboard and mouse").
Internals:

1. **Keyboard = a pad on port 1** (`kbm_pads`, right after `PADRead`): held keys from window 1's SDL
   events (released on focus loss, ignored while Alt/GUI is held) become a `PADStatus`; `p1_input=auto`
   ORs it with the first controller (the keyboard's stick wins when deflected), `keyboard_mouse` makes
   port 1 the keyboard alone (aurora's port 1 cleared: controllers start at port 2), `controller` makes
   nothing. aurora's own keyboard bindings are cleared at startup. `[keys]` in simmy.ini rebinds.
2. **Mouse buttons/wheel = pad presses**: left = A, right click = B, wheel = d-pad up/down where the camera
   does not take the C stick (menus), else zoom.
3. **`KbmTick`** ([`platform.hpp`](../include/simmy/platform.hpp)): what a pad cannot give, once a frame:
   cursor point (tiles), camera turn/zoom/pan, flags (`kKbmCursor` the mouse leads the classic cursor,
   `kKbmKeysMoved`). It belongs to the player on port 1 (`KbmPlayer`). Read by `UpdateDirectInteractors`
   (mouse over the room -> the Z path, "mouse classic"), `UpdateSimInteractor` (cursor to the mouse point;
   keys moved -> back to direct control without cancelling), `ESimsCam::Update`/`PcKeyboardMouseCamera`.
4. **Mouse -> floor point**: `ELiveMode::DrawMain`/`InitPlayerXWindow` call `kbm_note_view` with the view
   exactly as drawn (projection, eye, viewport in the 640x480 frame); the window position is mapped through
   aurora's FIT/STRETCH presentation (and halved for split=windows); the ray is cast against object boxes
   (`kbm_pick`, read-only) then the floor plane z = 0.
5. **Game state for the input maker**: `kbm_game_state` (siminteractor.cpp) reports whether the camera and
   cursor are free; only the produced input matters for the game, and it is recorded (`m` field).

## Co-op input

Both pads are meant to be read whichever window has focus (the comment in `pc_window2.cpp` says so; the
port sets no SDL background-events hint itself, so this relies on SDL/aurora defaults and is untested with
real windows on Windows). Keyboard, mouse and F1 belong to window 1 / player 1; the mouse counts only inside player 1's
window or half. See [coop-and-players.md](coop-and-players.md) for private screens taking only their
owner's pad.

## Gotchas

- Add new input at `ENgcControllerManager::Update` only, before `det_pads`; anything read elsewhere
  (SDL state, wall time) breaks record/replay.
- `kbm_pads` and the click hold-back read the previous frame's game state; harmless because what they
  produce is recorded.
- The F1 overlay neutralises pads but keeps `err`, and keeps each pad blocked until fully released after
  closing, so the B/Start that closed it never reaches the game.
- `SimInteractor::OnCommandPressed` still calls the generated stub `cXPersonImpl::CancelLastAction` for
  B and logs a stub for `CheckNewSimInSocialMode` although `InteractorModule::CheckNewSimInSocialMode`
  (80213C60) is ported in familyselect.cpp (see [known-gaps.md](known-gaps.md)).
