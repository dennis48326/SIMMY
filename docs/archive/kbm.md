# kbm: keyboard and mouse for player 1

Tree /home/claude/gfx/port (+ /home/claude/gfx/aurora, unchanged), build /home/claude/gfx_build (simmy, SimmyLauncher).
Diff vs base79: `kbm.diff` (this folder). Scripts: `os.sh` (offscreen run, 1P lot 13 by default, frames from the house,
`KBM=` -> SIMMY_KBM_TEST, `GFX=`, `TWO=1`, `EXTRA=`), `detrun.sh` (SIMMY_DET run into `det/`), `suite/` (m16 suite
pointed at this build). Outputs of the runs: `out/` (logs, shots), `det/` (hashes, recordings).

## Design

Hybrid, as asked: WASD walks the sim (direct control), the mouse drives the camera and the game's own classic
(cursor) control. Everything enters the game as per-frame input at one place, `ENgcControllerManager::Update`
(8032DD60), right after `PADRead`, before the scripted/soak/overlay filters and before `det_pads` records or
replaces the pads:

1. **Keyboard = a GameCube pad on port 1** (`kbm_pads`, src/platform/pc_kbm.cpp). Held keys (tracked from window 1's SDL
   events; released on focus loss; ignored while Alt/GUI is held, so Alt+Enter is not A) make a PADStatus: main stick
   (full = run, with Shift 0.5 of the game's full scale 40 = walk), A B X Y Z Start L R (+ analog triggers 255),
   d-pad, C stick. `p1_input=auto`: merged into port 1 with the first controller (buttons OR'ed, the keyboard's
   stick wins when pressed); `keyboard_mouse`: port 1 is the keyboard alone; `controller`: nothing.
   With auto / keyboard_mouse, port 1 is always "connected".
2. **Mouse buttons and wheel = pad presses** (same function): left button = A (held while held, >= 3 frames),
   right click without drag = B (3 frames), wheel = d-pad up/down (one 2-frame press per notch, 2-frame gap) where the
   camera does not take the C stick (menus, front end, CAS, pause, the interaction menu); otherwise the wheel zooms.
3. **What a pad cannot give = `simmy::KbmTick`** (platform.hpp), one per frame: flags (`kKbmCursor`: the mouse
   points at a floor point / object in player 1's 3D view and leads the classic cursor; `kKbmKeysMoved`: the
   stick is deflected), the cursor point (cx, cy, tiles), camera turn (degrees), zoom (tenths of the zoom range),
   pan (-1..1 x/y). It belongs to the player whose controller is port 1 (`KbmPlayer`, siminteractor.cpp).
   The game reads it at the points where it reads the pads:
   - `UpdateDirectInteractors` (directinteractor.cpp): `kKbmCursor` while the left stick reaches the direct
     interactor (camera's pad filter; no menu) and no camera director shot -> the Z path,
     `DirectInteractor::OnCommandPressed(0x11)` (8020506C): classic control, sim stands, HUD menu closed;
     marks the player "mouse classic" (`PcSetMouseClassic`). A real Z press clears that mark.
   - `UpdateSimInteractor` (siminteractor.cpp, the stand-in for InteractorManager::UpdateActivePlayerInteractors):
     while mouse classic and `kKbmKeysMoved` (and no `kKbmCursor`) -> back to direct control
     (`ChangeActiveInteractor(p, 0)`, `HUDTarget::CloseMenu`), *without* the Z path's CancelAllActions (a queued
     action keeps running, like going back with the stick on GC would if it could; B / Backspace cancels);
     `kKbmCursor` -> the frame's cursor point for `SimInteractor::Update` (8021D7D4), which puts `m_pos` there
     (clamped to the lot) in place of the stick step (`MoveCursorAlongCameraTargetVector` 8021097C), so the
     tile squares, highlights (`UpdateObjectHighlights` 8021ED2C), A (`ExecuteActionMenu` 8021EA98) and the beam
     (`InteractorVisualizer::Draw` 80216F54) all work as with the stick.
   - `ESimsCam::Update` (80016DB8): in mode 0 the camera does not `FollowPlayerInteractor` (80016C34) while the
     mouse leads the cursor (the mouse points at what is on screen; following would make it drift). New
     `ESimsCam::PcKeyboardMouseCamera`, after `HandleRotation` (8001709C) / `HandleZoom` (800172E8), while the C
     stick reaches the camera (`IsStickFiltered(1, ..., m_filterId)`, the same gate the C stick has): turn via
     `ApplyPan` (80016FB8, clears +0x474 like HandleRotation), zoom via `ApplyZoom` (80016FFC, x 0.1 x zoom range a
     unit), so the camera's smoothing (`InterpolateActualCameraParamsToTarget`) and tilt-from-zoom behave as with
     the C stick; pan (mouse classic only) moves the target with `MoveCursorAlongCameraTargetVector` at
     `GetCursorSpeed` (8001A00C), clamped to the lot.
   - `EGlobal::TryAutoMapPlayer` (80042614): with keyboard_mouse, player 1 is mapped to port 1 whichever pad
     pressed, player 2 never to port 1.
4. **Click while in direct control**: the press makes the switch first; A is held back (<= 12 frames) until the
   classic control runs and its cursor is in state 1 (`kbm_game_state().classicReady`), so A opens the menu at
   the cursor, not the direct interactor's "nearest objects" menu. In menus a click is A at once.
5. **Mouse activity**: the mouse leads the cursor once it moved > 4 px (or clicked) since the stick or Z was last
   used; stick (keys or a controller) / Z hand control back. While dragging the camera (middle button, or right
   button past 6 px) the cursor stays put. Edge pan: 6 px from window 1's edges while mouse classic
   (`mouse_edge_pan` off / on / fullscreen, default fullscreen). Numpad 8/2/4/6 pan.

`kbm_game_state` (siminteractor.cpp) is what the input maker asks the game (read-only): the port-1 player, live
(house, camera, no pause ref count / pause menu, no CAS, no camera director shot), cameraFree (C stick not filtered,
no interaction menu open: `HUDTarget::m_menus[p].m_state > 1`), cursorFree (no build/buy tool, direct or classic,
no social mode, selected sim, left stick not filtered, no menu), classicReady, mouseClassic. Only the input it
produces matters for the game, and that is recorded.

## Mouse -> classic cursor mapping

- `ELiveMode::DrawMain` (8008F7AC) / `InitPlayerXWindow` (8008F1B8): after a player's view is selected,
  `kbm_note_view` keeps `E3DWindow::m_lookAtProj` (world -> clip, row vectors, from `CalcLookAtDotProjection`
  802E5F7C), `m_lookPos` / `m_lookDir` (`LookAtMatrixChanged` 802E5E80) and `m_viewportOut` (the view's rectangle in
  the 640x480 logical frame: full, or a half in the split views), plus whether the frame is a split=windows frame.
  So the mapping is exactly what was drawn: widescreen (the projection's aspect), split halves, both windows.
- Window 1 mouse position -> logical frame: the EFB (aspect `fb_width/fb_height` from `aurora_get_window_size`)
  fitted into the window as aurora presents it (FIT: pillar/letterbox for 4:3; STRETCH: the whole window for
  widescreen and split views) -> u, v; split=windows: window 1 shows the frame's left half, so u x 0.5. Then
  x 640, y 480. Internal resolution does not matter (all in normalised units); HUD layout (hud=) does not move 3D
  views (display_map_viewport leaves perspective views alone).
- Outside the player's viewport (the other player's half in a one-window split) -> no cursor.
- Viewport -> NDC; a point at NDC z 0.5 through the inverted matrix (double precision) and the eye give the ray;
  it is oriented along m_lookDir. `kbm_pick` (siminteractor.cpp) first casts it against the house's object
  instances' boxes (`ISimInstance::m_bounds`; slab test; boxes nearer than 0.75 tiles - cut by the near plane -
  , boxes the eye is in, boxes larger than 3.2 x 3.2 x 3 tiles (trees) and the player's own sim are skipped): the
  nearest hit before the floor gives a point inside its footprint (the cursor's selection square then lights it).
  Otherwise the ray meets the plane z = 0 (the classic cursor's plane: its z never changes, `follow` 0 at
  80473524). Pointing at a table top, a fridge door or a phone on the wall thus selects that object, not the floor
  behind. It reads only (no `HasInteractions`: that refreshes a cache with the UI rand and runs check trees).
- Verified: back-projection of the floor point lands on the mouse (trace `[kbm] ray ... back x,y`), the beam and the
  tile square are drawn at the mouse in 16:9 hud=edges (1280x720) and 4:3 (1024x768, pillarboxed) shots.
  Offscreen shots lag the game by a few (sometimes many) frames with software rendering, so test shots are taken
  with the mouse still; the cross drawn into test shots is where the mouse was when the shot was requested.

## Settings, bindings, launcher, overlay

- simmy.ini (pc_settings.cpp, GraphicsSettings): `p1_input` (auto | controller | keyboard_mouse), `mouse_edge_pan`
  (off | on | fullscreen), `mouse_rotate_speed`, `mouse_zoom_speed` (0.1..10), `mouse_invert_x/_y/_wheel`; aliases
  like the other keys; bad values warned as `[gfx] ignoring ...`. `[keys]` section: `action = key, key` (also `+`),
  `none`; names: letters, digits, f1-f12, space, return/enter, kp_enter, escape, backspace, tab, l/rshift, shift,
  ctrl, alt, arrows, page/home/end/insert/delete, punctuation names, kp_0..9, or SDL's own names; positional
  (scancodes). Unknown actions / keys warned (`[keys] file:line: unknown key "x" for a`). F1/F2 bindings warned.
  The overlay's "Save" and the launcher now insert new keys before a trailing `[keys]`/`[launcher]` section.
- aurora: its keyboard-to-pad profile bindings are cleared at startup for all ports (`clear_bindings`, logged if
  any were set); keyboard_mouse also `clear_port(0)` (controllers start at port 2). No aurora patch needed.
- F1 overlay: while open, `kbm_pads` makes nothing (the pads are neutralised by `overlay_filter_pads`, keys stay
  latched until released, mouse presses begun in the overlay are ignored until released). Its Controls page lists
  the game's keys (from the bindings) and the mouse.
- SimmyLauncher Options: "Player 1 controls" (p1_input) and "Edge scrolling" (mouse_edge_pan) in the panel now
  called "CONTROLS, GAME & LAUNCHER" (5 rows).
- System pointer: hidden while it is over player 1's 3D view, the mouse leads the classic cursor and no menu is
  open; shown otherwise (menus, front end, F1, other window).

## Determinism

`kbm_pads` runs before `det_pads`; the record line gets ` m flags,cx,cy,rot,zoom,panx,pany` (floats as `%a`,
exact) when the KbmTick is non-zero; replay reads it into the tick (zero when absent / after the file), so old
pad-only recordings are unchanged in format. Headless runs make no keyboard/mouse input at all.

## Default keys (README "Controls: keyboard and mouse" has the full table)
WASD main stick (Shift: walk), Space/Enter/KP Enter A, Backspace B, F X, Tab Y, C Z, Esc Start, Q/E L/R,
arrows d-pad, I/K/J/L C stick, numpad 8/2/4/6 camera pan (mouse cursor control). Mouse: over the room = classic
cursor, left = A, right click = B, middle/right drag = turn (x) and zoom (y), wheel = zoom (menus: d-pad up/down).
Why arrows = d-pad: the d-pad is needed everywhere (menus, the pause row, the HUD side panels: left motives, up
skills, right wants and fears, down closes, and the action queue's cancel mode), so it gets the obvious keys; the
camera has the mouse plus the C stick keys, and pan keys on the numpad.

## Files touched
- new: src/platform/pc_kbm.cpp, pc_kbm.hpp (input maker, bindings, mapping, test switch, overlay text helpers)
- include/simmy/platform.hpp (KbmTick, kbm_* interface, KbmGame, kbm_pick), include/simmy/classes/siminteractor.hpp
  (KbmPlayer, PcMouseClassic, PcSetMouseClassic, PcTakeMouseCursor), include/simmy/classes/camera.hpp
  (ESimsCam::PcKeyboardMouseCamera)
- src/engine/e_ngcctrlmanager.cpp (kbm_pads after PADRead)
- src/game/siminteractor.cpp, directinteractor.cpp, camera.cpp, livemode.cpp, global.cpp (see Design)
- src/platform/pc_det.cpp (record/replay of the KbmTick), pc_main.cpp (events, aurora keyboard bindings, port 1 in
  keyboard_mouse, test cross in shots, kbm_init), pc_settings.cpp/.hpp (settings, [keys], saving before a trailing
  [keys]/[launcher]), pc_overlay.cpp (Controls page)
- src/launcher/launcher_main.cpp, launcher_ini.cpp/.hpp (Options rows, p1_input/edge pan, [keys] left alone)
- README.md (Controls section, the e_ngcctrlmanager line), simmy.ini.example (controls + [keys] defaults)

## Tests (offscreen, synthetic events via SIMMY_KBM_TEST; logs/shots in out/)
- WASD walk (stick 0,127 / 127,0 in the trace, sim moves; `kp`), Shift = 20 (half scale).
- Tab: "[ctl] player 1: next sim" (Winston -> Clarice; camera follows; `k169`).
- Mouse over the room -> "[ctl] player 1: classic control"; cursor follows exactly (trace `[kbm] ray ... back`
  equals the mouse); the beam and the tile square are at the mouse (16:9 hud=edges 1280x720 `t10_530`, `fa`;
  4:3 1024x768 `k43`). Object picks: counter, fridge, plant, topiary, Clarice ("action menu at ... (1/3 objects)").
- Left click -> object list menu at the cursor (`k169`, `k43`, `fa`), Space/click -> interactions, click -> chosen
  action runs ("queues Have Lunch ... runs Get Ingredients", "queues View on art_plant_wild"). Click in direct
  control switches first and A waits for the classic cursor (`t3`: "action menu at 18.97, 27.16").
- Right click: closes the object menu (`k43`), backs out of a pause page and closes the pause menu (`fb`), cancels
  actions in direct control ("cancels: 1", coop run).
- Wheel: zoom in live mode (eye height 1.6 -> ~8), d-pad down in the object menu (selection Fridge -> Horatio,
  `k43`) and in dialogs. Middle drag / right drag: camera turns (`fa_740`, `t7`), no B after a right drag.
- Esc: pause; arrows move along the icon row (Goals -> Progress, `fin_940`); click opens the Wants page (`fb_880`).
- W/S/A/D back from mouse classic: "[ctl] player 1: direct control (keys)".
- F1 overlay open: W held, wheel, click made nothing in the game (no walking, no zoom, `kp`); keys held when it
  closes stay neutral until released (overlay latch).
- Co-op split=windows, p1_input=keyboard_mouse, SIMMY_VIRTUAL_PADS=1: "[kbm] p1_input=keyboard_mouse: controllers
  start at port 2", "[coop] player 2: pad 1"; P1 mouse classic cursor + click + wheel zoom in window 1 only, P2's
  window unchanged (`coop_600_p1/_p2`).
- simmy.ini warnings: "[keys] file:6: unknown key "spcae" for a", "unknown action "foo" (known: ...)",
  "start on F1/F2 ...", "[gfx] ignoring mouseedgepan=sometimes (expected off, on or fullscreen)".
- Launcher: Options page shows the two rows (kbm_launcher_options.png); `--set p1_input=keyboard_mouse` writes the
  key outside a trailing [keys] section.

## Determinism
- KB/M session recorded (offscreen 640x360, 4:3 pillarboxed; SIMMY_DET=1, from the house): zoom wheel (became
  d-pad in the welcome dialog), W walk, mouse glide -> classic, click, Space, middle drag, right drag, numpad pan,
  click on an object, right click, Tab, A (back to direct), wheel, right click: `det/K.rec` (549 lines with an
  `m` field). Replay offscreen without the test events: **match 1300 ticks** (`K1`); replay **headless**:
  **match 1300 ticks** (`K2`).
- Pad-only: old boot recording `$S/det/out/B.rec` replayed headless with this binary: **match 22000 ticks**
  (vs B.hash). New house-start soak recording (SIMMY_SOAK=7, 20 macros, no `m` fields) record vs replay:
  **match 6000 ticks**. (The old house-start `L.rec` diverges at tick 1 in the RNG part against `L.hash`; the
  front end before tick 1 is not part of a house-start recording, and the boot recording, which covers it,
  matches, so this is the front-end path having changed in main since that recording; not investigated further.)

## Regression suite (`suite/`, this binary, run alone)
toilet 56.7, sofa 28.3, fridge 32.0, shower 45.6, bed 31.8; `grep -c "cancels: 1" s_cancel.log` = 1; script
error / TREEASSERT: 0. (A first run while two offscreen renders shared the 2 CPUs gave toilet 57.6 / shower 48.8
/ bed 32.9: non-det mode's wall-clock frame times; the unloaded rerun is the result.)

## Known limits
- No clicking on individual buttons (the game has no pointer of its own): click = A, right click = B.
- Build/buy cursors (place manipulator, grab, wall/floor/paint tools) are not mouse-driven: they move with WASD /
  the stick as on GC; there the mouse gives A/B. Tested: pause row -> buy mode with arrows + Return, the catalogue
  (`bb`): wheel = d-pad down through the items (Office Chair -> "ModMan" Office Chair). Turning/zooming the camera
  with the mouse while placing an object was not tested (it applies wherever the camera takes the C stick).
- Interactions/pie lists: navigated with keys / wheel; moving the mouse does not choose an item.
- The classic cursor is on the floor plane z = 0 like GC's; upper-floor floors cannot be pointed at (objects up
  there can, through their boxes). Box picking is coarse (axis-aligned boxes; trees and anything over 3.2 x 3.2 x 3
  tiles are ignored; objects closer than 0.75 tiles are looked through).
- Moving the mouse over the room always takes the cursor (after 4 px); use WASD/stick or C (Z) to go back.
- Edge pan / pan keys only move the camera while the mouse leads the classic cursor (in direct control the camera
  follows the sim, as on GC). Rotation sign/speed: drag right turns the view as if grabbing the world (0.3 deg/px x
  mouse_rotate_speed); vertical drag zooms (0.02 tenths of the range per px).
- With keyboard_mouse in one-player games a controller is pad 2 and does nothing (player 2's).
- The wheel-to-d-pad choice and the click hold-back read the previous frame's game state (harmless: what they
  produce is recorded).
- Offscreen test shots trail the game by a few to ~100 frames (software rendering queue); the cross in a shot is
  where the mouse was when the shot was requested. Tests keep the mouse still before shots.
- Real windows were not available here: SDL event handling (focus loss, enter/leave, the hidden system pointer,
  HiDPI point vs pixel coordinates) is written to SDL3's documented behaviour but only exercised with synthetic events.
- MSVC not compiled here; the new code avoids VLAs/GNU extensions (one const-ref operator== on KbmTick).

## Screenshots (/mnt/user-data/outputs/)
kbm_classic_cursor_menu_16x9.png (16:9 hud=edges: mouse on the plant, beam there, its "View / Flowers" menu),
kbm_classic_cursor_menu_4x3.png (4:3: mouse on the fridge, object list menu), kbm_camera_wheel_zoom_then_drag_rotate.png,
kbm_pause_keys_and_click.png (arrow keys along the pause row; a click opened the Wants page), kbm_coop_windows_p1_mouse_p2_pad.png,
kbm_f1_overlay_blocks_input.png, kbm_launcher_options.png.
