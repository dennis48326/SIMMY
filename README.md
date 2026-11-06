# SIMMY PC port

A native PC port of The Sims 2 (GameCube), written as a non-matching decompilation on top of
[aurora](https://github.com/encounter/aurora). See `PORTING.md` for the full plan.

## Documentation

How the engine works, by system (architecture, simulation, UI, rendering, input, co-op, saves,
determinism, testing, known gaps): [docs/README.md](docs/README.md). Read it before changing the code;
update it with every fix or feature ([docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)).

**Status: the game boots to its start screen.** 1886 functions are ported. ESimsApp starts the game's
state machines ('SIMS', 'MUST', 'LdSc'), and the boot sequence runs through the game's own code:
"Licensed by Nintendo" (drawn with the game font), the memory-card check, the EA logo movie, the
Maxis screen, the Sims 2 intro movie, then the start screen.

```
[simmy] entering game main
  EEngine::Init: clock, file system, graphics, resource system (20 managers), controllers, memory card  OK
  ESimsApp::Init: datasets, textures, shaders, fonts, animations, quickdata (UI strings)               OK
  ESimsApp::initContinue: state machines (game flow, memory card, loading screen)                      OK
  boot screens: licence, memory card check, EA movie, Maxis logo (Wrapper UI), intro movie, start screen OK
  models (ERModel, E3DWindow), loading-screen plumb bob (SIMMY_TEST_LOADING=1 to see it)               OK
  Apt Flash UI: runtime, ActionScript interpreter, player, rendering; s2c_root; main menu screen        OK
  audio: SDL3 mixer, movie sound, music streams, UI samples                                             OK
[stub] main menu navigation (Apt pad input), the 3D world map behind it, everything after it
```

Dev switches: `SIMMY_SKIP_INTRO=1` skips the boot movies, `SIMMY_TEST_LOADING=1` shows the loading
screen, `--render-test` draws the renderer test overlay, `SIMMY_PAD_SCRIPT=5010:start` holds pad 1's
buttons from a frame on (e.g. Start on the start screen), `SIMMY_APT_TRACE=1` traces Apt bytecode and
edit-text strings.

Audio (SDL3; `DATA/SAMPLES.ARC` and `DATA/AUDIOSTR.ARC` hold the sound effects and music):
`SIMMY_AUDIO_WAV=out.wav` (or `SIMMY_AUDIO_DUMP=out.wav`) also records everything mixed, `SIMMY_AUDIO=0`
turns audio off, `SIMMY_AUDIO_TEST=music_48_title_track,sfx:0x374BDCEA` plays music and samples through
the game's calls (`loop:` before a music name loops it). Headless runs use SDL's silent "dummy" driver.
The game's sound director (cSoundPlayer/BoxX, game/gamesound.cpp, boxx.cpp, gzsndsys.cpp, patch.cpp)
plays sound events (soundeve.arc) and the mode music; `SIMMY_SOUND_TRACE=1` logs them ("[snd] ..."),
`SIMMY_SOUND_EVENT="200:stereo_pop@sim;600:v_sit_dislike@130"` plays events by name for a source, a
number of frames into live mode. Without `samples.arc` the effects and voices are silent.

Set `SIMMY_LOG_LOADS=1` to print every resource load:
```
[load] textures     7F5ABC2D    1604 bytes
[load] shaders      7F5ABC2D     264 bytes
[font] 67F664FB size 24: 185 glyphs, 1 pages (first page shader B21E3C8A)
```

**Threading model:** the game's app thread runs on the process main thread. It owns the window, the
GPU and input, and window events are pumped once per game frame. The GameCube renderer already did its
GX work on the app thread, so this matches the original. EA's other threads (`EThread`) are real
host threads, and the resource system's background worker is one.

**Saves** go where aurora keeps them: `%APPDATA%\SIMMY\USA\Card A\` as `.gci` files, the same format
Dolphin uses for its GCI folders (also when the game runs from a European disc). `simmy.ini` and the
crash report (`simmy_crash.txt` plus a small minidump `simmy_crash.dmp`; `simmy_watchdog.txt` with
`SIMMY_WATCHDOG=N`) are next to `simmy.exe`, the GPU cache
in `%APPDATA%\SIMMY\`. The game data (disc image or `--data` folder) is only read.

Ported so far:
- **Boot and app loop:** `e_main`, `e_app`, `e_engine` (init, shutdown, resource managers, frame timing),
  `ESimsApp`, `e_globalmanager`.
- **Platform, rewritten for PC:**
  - `e_ngcengine`, `e_ngcclock`
  - `e_standard_heap` (host-backed heaps)
  - threads, semaphores and sleep on std::thread
  - the file system on std::FILE, with a case-insensitive data root (`base/e_ngcfile`, `e_ngcfilesystem`)
  - `e_ngcctrlmanager`: the four pads on aurora PAD (SDL gamepads); player 1's keyboard and mouse join
    port 1 there (`pc_kbm.cpp`, see "Controls: keyboard and mouse")
  - `e_ngcmemorycard`: memory cards on aurora CARD
- **File system, ported as written:** `base/e_file`, `e_filesystem`, including the file-creator registry.
- **Resource system:** `EResource`, `EResourceManager`, `EResourceMap`, `FreeResourceManager`,
  `EResourceLoaderImpl` (loads run on the calling thread), `Background` (a worker thread),
  `EChecksum` (resource names to ids).
- **Controllers, ported as written:** `EController` (filters, command maps, auto-repeat, rumble),
  `EControllerData`, `EControllerManager`.
- **Graphics:** `EGraphics` and `ENgcGraphics` (init, frames, display lists, render contexts),
  `ENgcScheduler`, `EWindow` (2D views), and the renderer:
  - `ERC` / `ENgcRC`: the render context. Commands are recorded into `EDL` display lists (16-byte
    entries on PC, see `edl.hpp`); immediate contexts execute them at once.
  - `ENgcRenderer`: GX/vertex-format init, `Execute` (the 75-opcode dispatch), state commands
    (matrices, viewport, scissor, lights, material, geometry/raster modes, z/alpha test, blend),
    `SetShader`/`SetTexture`, `UpdateLightingEquation`, `UpdateTEVStages` (shader layers and blend
    modes to TEV stages and GX blending), `Rect`.
  - `ENgcRenderStateCache`: everything GX should be set to, applied before each primitive.
- **Serialisation:** `ETypeInfo` (the run-time type registry), `EStorable`, and `EFile`'s object-graph
  reader (`operator>>`, `ReadStructure`), with big-endian field readers.
- **Assets:** `EDataHeader` (the header on every resource), datasets (`ERDataset`,
  `EDatasetManager`), fonts (`ERFont`, `EFontData` and its sizes, pages and glyphs), animations
  (`ERAnim`; checked against all 3,426 in the archive), textures (`ERTexture`, `ETexture`, the GX
  texture classes for C4/C8/CMPR/RGB5A3/RGBA8 and 32-bit palettes, swizzling), shaders (`ERShader`,
  `EShader`, `ENgcShader`, `EShaderDef` read field by field from big-endian data).
- **Game:** `ESimsApp::Init` and the per-frame `Update`/`UpdateDraw`/`UpdateGame`/`UpdateShaders`,
  command-line parsing, the controller command map, `StateMachineManager`, the localised logo.
- **Math:** `EVec2/3/4` and the `EMat4` basics.

Every other function is a generated stub that logs on first call, so the log is the to-do list.

## Building on Windows

Needs Visual Studio 2022 or newer (with "Desktop development with C++"; VS 2026 works), CMake 3.25+,
Python 3 and Git. The easy way is `build_simmy.bat` next to `simmy_port.zip`: it finds Visual Studio
and CMake (installing CMake with pip if needed), unpacks, builds the game and its launcher, copies the
runtime DLLs, runs the render test (with the disc chosen in the launcher; skipped until one is chosen)
and starts the launcher, writing `build_log.txt` and `render_test.bmp`. By hand:

```
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo --target simmy simmy_launcher
build\RelWithDebInfo\SimmyLauncher.exe                                   (the launcher)
build\RelWithDebInfo\simmy.exe --iso "D:\Games\The Sims 2.iso"            (window)
build\RelWithDebInfo\simmy.exe --iso "D:\Games\The Sims 2.iso" --headless (log only)
build\RelWithDebInfo\simmy.exe --iso "D:\Games\The Sims 2.iso" --fs-test  (reads a few game files)
```

**Game data: your disc image.** The game reads everything straight from a GameCube disc image of
The Sims 2 (any region): a plain `.iso` / `.gcm` as Dolphin or a disc dump makes it. Normally
SimmyLauncher.exe asks for it once, writes `iso=<path>` into `simmy.ini` and starts the game with
`--iso "<path>"`; `simmy.exe --iso <file>` does the same by hand. Compressed images (`.rvz`, `.wia`,
`.gcz`, `.ciso`, NKit) are not read: convert them in Dolphin first (right-click the game, *Convert
File...*, Format *ISO*). Without a disc image the game says so and exits (a message box on Windows).
Nothing is ever written to the disc image or next to it.

For development and tests, `--data <dir>` reads an extracted disc instead (the folder that holds
`DATA\` and the `.NGH` files) and wins over the disc image; with neither, the `SIMMY_DATA`
environment variable names such a folder. `python tools/make_test_iso.py <folder> <out.iso>` packs
such a folder into a disc image (test game ID `SIMT01`). `SIMMY_IO_STATS=1` prints the files opened
and the reads made when the game exits.

Aurora and its dependencies (Dawn, SDL3, …) are downloaded on the first configure. Run without
`--headless` to get a window. The game loop owns the frame; until the loading screen and front end
are ported the window shows the game's clear color (`--render-test` draws the background gradient,
the logo and three test quads through the game's renderer). Closing the window asks the game to quit.
`--headless` prints a frame count every 300 frames.

### Playing: SimmyLauncher

`SimmyLauncher.exe` (built next to `simmy.exe`; `build_simmy.bat` builds it and starts it at the
end) is the way to play. `run_simmy.bat` and `run_simmy_coop.bat` are no longer needed (they still
work if you keep them). The launcher:

- **Choose your disc:** pick your The Sims 2 GameCube disc image (`.iso` / `.gcm`, or drop it on the
  window). It checks the disc, shows its own banner, title and region, and remembers it as `iso=` in
  `simmy.ini`. Compressed images (RVZ, WIA, GCZ, CISO, WBFS), NKit images, Wii discs and other
  games are refused with a hint (convert to a plain ISO with Dolphin's *Convert File...*).
- **Play** starts `simmy.exe --iso "<disc>" --split=gc`.
- **Co-op** shows the connected controllers and the screen layout (two windows, one wide window,
  two 4:3 halves, GameCube original) and starts `simmy.exe --iso "<disc>" --split=<layout>`.
  *Quick co-op start* (on by default, like the old co-op batch file) adds `SIMMY_TWO_PLAYER=1`,
  `SIMMY_CAS_ACCEPT=300` and `SIMMY_SKIP_INTRO=1`.
- **Options** edits the `simmy.ini` settings below (screen mode, window size, aspect, HUD, V-Sync,
  frame rate, render resolution, anti-aliasing, texture filtering, scaling filter, graphics API)
  plus *Skip intro movies* (`SIMMY_SKIP_INTRO=1`). Its own settings live in a `[launcher]` section
  of `simmy.ini`, which the game skips.
- While the game runs the launcher waits minimized and comes back when it ends. The game's output
  goes to `game_out.txt` / `game_log.txt` next to the launcher; if the game ended with a nonzero
  exit code or wrote a crash report (`simmy_crash.txt` / `simmy_crash.dmp`), the launcher says so,
  records the exit code (hex) and the report in `simmy_launcher_log.txt` and offers to open the log
  folder. A clean quit (exit code 0) shows nothing. The launcher removes `SIMMY_*` variables (test and
  diagnostic switches such as `SIMMY_WATCHDOG`) other than `SIMMY_INI` from the game's environment,
  unless `SIMMY_LAUNCHER_KEEP_ENV=1`.

Mouse, keyboard (arrows, Enter, Esc) and controllers (D-pad / stick, A, B) all work. Test switches:
`--screenshot out.bmp [--page home|coop|options|playing|error] [--frames N] [--scale S]`,
`--test-iso <file>`, `--game-exe <file>`, `--fake-pads N`, `--auto-launch play|coop`,
`--write-icon <dir>` (renders the plumbob icon; `src/launcher/res/simmy_launcher.ico` is made from it).

**Renderer test:** `simmy.exe --iso D:\Games\Sims2.iso --render-test` draws, through the game's own
render context and renderer, the two shaders startup loads (the background gradient full screen and
the Sims 2 logo in the middle) and three plain quads (red, green, half-transparent blue) along the
bottom. If that looks right, shaders, textures, TEV and blending work.

Development options:
```
--render-test            the test pattern above, over every frame
--screenshot N out.bmp   save frame N as a BMP (uses aurora's EFB readback)
--frames N               quit after N frames
--offscreen              render without a window (tests, CI)
```

## Graphics settings

Put a `simmy.ini` next to `simmy.exe` (the build copies the commented `simmy.ini.example` there once;
edit it, it is never overwritten). With no settings the game looks as on the GameCube: a 4:3 640x480
picture scaled to the window. Every setting can also be given on the command line, which wins over
the file: `--<setting> <value>` or `--<setting>=<value>`.

| Setting | Values | Default |
|---|---|---|
| `window` | `WIDTHxHEIGHT` (with `fullscreen=exclusive`: the display mode) | 1280x960, or 1280x720 at 16:9 |
| `fullscreen` | `off`, `borderless`, `exclusive` (also `--fullscreen`, `--windowed`) | `off` |
| `resolution` | internal render resolution: `auto` (the window's), `720p`, `1080p`, `1440p`, `2160p`, or a factor such as `2x` | `auto` |
| `aspect` | `4:3` (original) or `16:9` (wider 3D view, same vertical field of view) | `4:3` |
| `hud` | 16:9 only: `centered` (the 4:3 UI in the middle) or `edges` (HUD pieces move out to the screen edges) | `edges` |
| `msaa` | `1`, `2`, `4`, `8` | `1` |
| `anisotropy` | `1` to `16` | `16` |
| `vsync` | `on`, `off` | `off` |
| `fps_cap` | `60`, `30`, `20`, `15` | `60` |
| `backend` | `auto`, `d3d12`, `d3d11`, `vulkan`, `opengl`, `opengles` | `auto` |
| `resampler` | `bilinear`, `area` (scaling the picture to the window) | `bilinear` |
| `split` | two-player co-op display: `gc`, `4x3`, `wide` (one window) or `windows` (one window per player) | `gc` |
| `window2` | `split=windows`: player 2's window size, `WIDTHxHEIGHT` or `auto` (window 1's) | `auto` |
| `window2_pos` | its position on the desktop, `X,Y` or `auto` (remembered when the game quits) | `auto` |
| `window2_display` | the display it opens on (`0` = the first) or `auto` (another than window 1's) | `auto` |
| `fullscreen2` | `off` or `borderless` (fullscreen on its display) | `off` |

Example: `simmy.exe --iso D:\Games\Sims2.iso --aspect 16:9 --resolution 1440p --msaa 4 --fullscreen`.
Alt+Enter switches between windowed and fullscreen. The game keeps its own timing (it counts 60 Hz
display retraces), so it never runs faster than 60 frames a second, also on 120/144 Hz screens;
`fps_cap` can only lower that. The internal resolution is the size of the GameCube's frame buffer
(EFB) on the GPU, so screen copies (the frame effects, the screenshot) are made at that resolution
too. `aspect` applies to one player; the co-op display is `split` (also `--split=...`).
With two players, food creation (a fridge's "Get Ingredients": the ingredient screen, its recipe book
and its help and exit prompts) belongs to the player who opened it, in every `split` mode: it shows
in that player's window or half of the screen only and takes only their controller, while the other
player keeps playing and the lot keeps running. (On the GameCube it covered the whole TV, took both
controllers and froze the lot.) Meanwhile the other player cannot open the pause menu, use a
wardrobe or leave the lot; those wait until the screen is closed.
Tests can use `SIMMY_GFX="aspect=16:9;msaa=4"`, `SIMMY_GFX_<SETTING>=value` and `SIMMY_INI=<file>`
(`SIMMY_BACKEND` still works and now applies to windows too).

**In-game options (F1).** F1 opens an options menu over the game, from the front end, menus, Create-a-Sim
and live mode (also with two players). It has three pages: *Graphics* (every setting above), *Controls*
(the keys and buttons) and *Quit*. Use the mouse, the keyboard (arrows, Enter, Esc) or a controller (D-pad
or stick to move, A to choose, Left/Right to change a value, L/R or the shoulder buttons to change page,
B to go back, Start or F1 to close). While it is open the game is paused and gets no controller input.
These settings change at once: aspect, HUD placement, internal resolution, window size, fullscreen,
VSync, frame-rate cap, anisotropic filtering and the scaling filter. MSAA and the graphics API are marked
"(applies after restart)". "Save to simmy.ini" writes the current settings to the `simmy.ini` that was read
(next to `simmy.exe` unless `--config`/`SIMMY_INI` named another one), keeping its comments and any lines
it does not know; settings still at their default are only changed where the file already sets them.
"Reset to defaults" puts every setting back (not saved until you save) except the co-op display and the
second window's place. "Co-op display" switches the two-player layout at once; with "Two windows" a
"Player 2 window" row shows, hides or fullscreens the second window. *Quit* asks once more, then leaves the game the same way closing the
window does, so the memory card files and the window are closed normally.
Test switch: `SIMMY_OVERLAY_TEST="<frame>:<action>,..."` (actions `f1`, `open`, `close`, `up`, `down`,
`left`, `right`, `a`, `b`, `start`, `l`, `r`, `set:<key>=<value>`, `save`) drives the menu in offscreen
runs and paints it into `--screenshot`/`SIMMY_SHOTS` images (offscreen there is no window for aurora's
ImGui, so the overlay then runs in its own ImGui context and is drawn into the shot on the CPU).

## Controls: keyboard and mouse

Player 1 can play with the keyboard and the mouse, a controller, or both (`p1_input`, below). The
keyboard is a GameCube controller on port 1 (every function of the game is reachable); the mouse drives
the camera and the game's own *classic* (cursor) control.

| Key / mouse | GameCube | What it does |
|---|---|---|
| W A S D | main stick | walk / run (direct control); move in menus |
| Shift (held with W A S D) | half stick | walk instead of run |
| Space, Enter | A | choose, interact, OK |
| Backspace | B | back, cancel (held: the action queue's cancel mode) |
| F | X | X |
| Tab | Y | next sim of the family (classic control: also the cursor back to the sim) |
| C | Z | classic (cursor) control on / off |
| Esc | Start | pause menu |
| Q / E | L / R | as on the GameCube (e.g. turn the object being placed in buy mode) |
| Arrow keys | D-pad | menus; in live mode the side panels (left motives, up skills, right wants and fears, down closes) |
| I / K, J / L | C stick | zoom in / out, turn the camera (Create-a-Sim: turn the sim) |
| Numpad 8 / 2 / 4 / 6 | - | pan the camera (while the mouse leads the cursor) |
| Mouse over the room | - | classic control: the cursor (the beam) goes to the floor point or object under the mouse |
| Left click | A | A at the cursor (opens the object's or sim's action menu); A everywhere else |
| Right click | B | B (closes menus, cancels; in live mode with nothing open it cancels the sim's actions) |
| Middle drag, right drag | - | turn the camera (left/right) and zoom (up/down) |
| Mouse wheel | - | zoom; in menus D-pad up / down |
| F1 | - | options (the game gets no keyboard or mouse input while they are open) |

Moving the mouse over the 3D view switches player 1 to the classic control (as Z does) and the cursor
then follows the mouse exactly; the camera stays where it is (pan with the numpad keys or, with
`mouse_edge_pan`, at the screen edges). Using W A S D (or a controller's stick) goes back to direct
control. A click while in direct control first switches, then presses A at the cursor. Over objects
the cursor goes to the object the mouse points at (its box), else to the floor. Menus, dialogs, the
pause pages, Create-a-Sim and the catalogs are used with the keys and the click = A / B; clicking on
a particular button is not supported (the game has no mouse pointer of its own). The system mouse
pointer is hidden while it leads the game's cursor over the room.

| Setting | Values | Default |
|---|---|---|
| `p1_input` | `auto` (keyboard + mouse and the first controller), `keyboard_mouse` (the first controller goes to port 2, i.e. player 2 in co-op), `controller` (no keyboard / mouse in the game) | `auto` |
| `mouse_edge_pan` | `off`, `on`, `fullscreen` (only in fullscreen) | `fullscreen` |
| `mouse_rotate_speed`, `mouse_zoom_speed` | 0.1 .. 10 | 1 |
| `mouse_invert_x`, `mouse_invert_y`, `mouse_invert_wheel` | `on`, `off` | `off` |
| `[keys]` section | `action = key, key` per action (see `simmy.ini.example`; unknown names are reported) | the table above |

Co-op: with `p1_input=keyboard_mouse` the keyboard and mouse are player 1's and the first controller is
player 2's (it is on port 2; any button on the start screen still maps player 1 to the keyboard). With
`auto`, the keyboard joins the first controller as player 1 and the second controller is player 2. The
mouse counts only inside player 1's window (`split=windows`) or player 1's half of the screen. The
mouse-driven input is part of the per-frame input: `SIMMY_DET_RECORD` writes it with the pads (an `m`
field: cursor point, camera turn / zoom / pan) and `SIMMY_DET_REPLAY` replays it.
Test switches: `SIMMY_KBM_TEST="<frame>:<action>;..."` (synthetic keys and mouse in offscreen runs: `key:`,
`keyup:`, `tap:`, `move:x,y`, `glide:x,y/n`, `at:<object id>`, `lclick`, `rclick`, `mdown`/`mup`, `drag:dx,dy/n`,
`wheel:n`; the mouse is drawn as a cross in screenshots), `SIMMY_KBM_TRACE=1` (the input made each frame),
`SIMMY_KBM_NOPICK=1` (the cursor goes to the floor only, never onto an object's box).
Not done with the mouse: build and buy mode's cursors (placing objects, walls, floors) still move with
W A S D / the stick as on the GameCube (clicks are A / B there; in the catalogues the wheel moves the selection).
SimmyLauncher's Options page has *Player 1 controls* (`p1_input`) and *Edge scrolling*.

Verified so far with Clang 18 on Linux. The first MSVC build may need small fixes in the generated
code; send me the errors.

## How porting works

| Piece | Where | What |
|---|---|---|
| Ported code | `src/<area>/<file>.cpp` | Hand-written C++, one file per original source file |
| Address markers | `// 802E391C` above each function | Marks that function as ported. The generator stops stubbing it |
| Signature fixes | `overrides.tsv` | Real return types and static/member status as they are recovered |
| Class layouts | `include/simmy/classes/*.hpp` with `// @class Name [after A B]` | A hand-written definition (real members) that replaces the generated empty class. Unported methods keep their generated declarations so their stubs still compile |
| Virtual call lookup | `python tools/vslot.py Class 0xOFF` | Names the method behind raw `(**(code **)(*(int *)(p + X) + 0xOFF))` calls |
| Class template | `python tools/class_decls.py Class` | Prints a class's generated declarations to start a hand-written header |
| Raw lookup | `python tools/show.py <raw dir> Name\|ADDR` | Prints a function's raw decompilation |
| Enums | `SIMMY_ENUM_BODY(Name)` in `simmy/enum.hpp` | A recovered enum, as a struct holding an int (see `classes/efile_enums.hpp`) |
| Generated layer | `gen/` (do not edit) | All classes, methods, vtables/inheritance, plus stubs. Rebuilt on every build |
| Raw reference | `raw/<file>.c` in the decomp package | Ghidra output with real names, the starting point for each file |
| Progress | `python tools/progress.py` | Ported vs. total, by area |
| Archive contents | `python tools/resdump.py <game dir> list\|dump\|save <manager> <name\|0xID>` | Lists, hex-dumps or extracts one resource from `DATA/*.ARC` |
| Original data | `python tools/elfdata.py <elf> <symbol\|0xADDR> [size]` | Dumps an initialised table from the original ELF (e.g. `g_simsCtrlCommands`) |

The generator (`tools/gen_port.py`) reads only `data/`, which is extracted from EA's map, the Ghidra
output and the ELF. It recovers:
- 1,276 classes
- 3,389 virtual methods
- 213 inheritance links, from vtable contents and shapes
- static vs. member methods
- some return types

Member variables are still unknown. Recovering them is the main work as each class gets ported.

## Next up

1. **The loading screen:** `StateMachine`/`StateMachineState`, `LoadingScreenStateMachine` and the
   rest of `ESimsApp::initContinue`, so the game draws its own first screen.
2. **Models:** `ERModel`, `ESubModel`, `ESubModelShader` (vertex data, display lists built at load
   time) and the renderer's geometry commands (`TriStrip*`, `NgcGXDisplayList`, `QuadList`, `RectList`).
   Aurora reads GX vertex arrays and display lists as big-endian, so model geometry can be handed over
   as stored.
3. The Apt UI (flash front end), audio (AX/MIX shim), then gameplay.
