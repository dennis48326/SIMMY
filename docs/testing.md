# Testing

## Overview

There is no unit-test suite: the port is tested by **running the game** with scripted input, headless
(no window, no GPU: logic only, ~4x faster) or offscreen (aurora's headless surface, software-rendered:
screenshots), and checking its log lines, screenshots and (in det mode) state hashes. The scripts used
during development were scratch shell/Python files outside the repo; this page describes the approach
so it can be rebuilt, and lists every `SIMMY_*` switch (verified in the code).

## Run modes

| Mode | Command | Use |
|---|---|---|
| Window | `simmy --iso <disc>` (or `--data <dir>`) | Play, manual checks |
| Headless | `simmy --headless --frames N ...` | Logic, long runs, soak. No aurora: no GPU, no portraits (they give up after 600 frames), card in `./USA/Card A` of the working directory, GX FIFO dropped each frame |
| Offscreen | `simmy --offscreen --screenshot N out.bmp --frames M ...` | Pictures. Dawn falls back to llvmpipe (OpenGL ES) in a container: slow (640x480 ~10 fps, 1080p MSAA 4 ~1 fps). Card under the SDL pref path, i.e. `$XDG_DATA_HOME/SIMMY/USA/Card A` on Linux: set `XDG_DATA_HOME` per test run to keep cards apart |
| `--fs-test` | reads a few files through the game's file layer and exits | data source checks |
| `--render-test` | the renderer test pattern over every frame | new backend / renderer changes |

Practical notes: run one offscreen job at a time on a small machine; give each run its own working
directory and `XDG_DATA_HOME` (a fresh card = delete the `USA` folder); frame numbers differ between
headless and offscreen and between runs (wall-clock `_dt`, "press start" after 4 s), so prefer
`SIMMY_FRAMES_FROM_HOUSE=1`, `SIMMY_PAD_SEQ` (state-driven) or `SIMMY_PAD_GATE` over fixed frame numbers;
use `SIMMY_FIXED_DT=1` or `SIMMY_DET=1` for repeatable timing.

## Scripted input

| Switch | Syntax |
|---|---|
| `SIMMY_PAD_SCRIPT` | `"frame:names[/hold],..."` presses at fixed game frames (pad 1; `p2:` prefix for pad 2, `p3:` / `p4:` for pads 3 and 4); with `SIMMY_PAD_SEQ` too, added to the sequence's presses (e.g. a button-code cheat once the house is up) |
| `SIMMY_PAD_SEQ` | `"start,a,right,a,up,a,a,left,a,house,cdown/150"`: each step fires once the watched UI state (managed screens, main menu state, dialog box) has been stable 90 frames / 1.5 s and 60 frames after the previous press; a press with no reaction in 300 frames is repeated up to 3 times; `house` waits for a house + 120 frames (after a quit to the main menu, for the next house; it logs the player-to-pad mapping, whether a load is in progress and pad 1's filter stack); `name/N` holds N frames; unknown names (`none/N`, `wait/N`) press nothing. `SIMMY_PAD_SEQ_UI=1` also counts UI variable changes as reactions (CAS, free play) |
| Names | `start a b x y z l r up down left right`, sticks `lup ldown lleft lright` / `cup cdown cleft cright` (full deflection), joined with `+`, `p2:` for pad 2 |
| `SIMMY_PAD_HOLD` | default hold frames (6) |
| `SIMMY_PAD_GATE` | holds pad 1's scripted presses in the house until conditions hold (`any free dlg idle calm soc rmd bb t<hex> nt<hex> on<id> g<guid> e<guid> w<n> x<cond> k !c a\|b`; see [`padgate.cpp`](../src/game/padgate.cpp)) |
| `SIMMY_SOAK=<seed>` | random play (walks, camera, interactions, B, Y, X, Z, d-pad, pause tabs, buy/build, quit to the main menu: options, Quit, then once the dialog box is up "No" to "save before quitting?"; the development menu re-enters the lot with `SIMMY_SOAK_LOT`; pad 1, and pad 2 in co-op, stay plugged in between presses; a quit during the lot's intro camera is refused as on GC, and story mode's tutorial dialogs take the pad, so quit soaks run best with `SIMMY_STORY=0`) with RSS/slow-frame/hang watchdog ([`pc_soak.cpp`](../src/platform/pc_soak.cpp)); its `[soak] rss` lines also give the lot's clock (days running, hour:minute). Sim-switch stress (test only, off by default): `SIMMY_SOAK_SWITCH=<weight>` adds a "switch" macro (Y on pad 1, pad 2 or both in the same frame, alone, right after A or while walking); `SIMMY_SOAK_SWAP=<weight>` a "swap" macro that calls `EGlobal::SwapSelectedSims` directly (GC: generic sim call 0x1E, used by no disc tree). Switching only succeeds where a family has more members than players, e.g. lot 13 (the Freebers) with `SIMMY_SOAK_LOT=13`; on lot 9 in co-op Y finds no other sim |
| `SIMMY_KBM_TEST` | `"<frame>:<action>;..."`: `key: keyup: tap: move:x,y glide:x,y/n at:<id> ldown lup lclick rdown rup rclick mdown mup wheel:n drag:dx,dy/n mouseout`; the mouse is drawn as a cross in shots |
| `SIMMY_OVERLAY_TEST` | `"<frame>:<action>,..."`: `f1 open close up down left right a b start l r set:<key>=<value> save sdlquit close2`; the overlay is painted into shots |
| `SIMMY_DET_REPLAY` | replay a recorded input file ([determinism-and-netplay.md](determinism-and-netplay.md)) |

Quick ways into the game: `SIMMY_SKIP_INTRO=1 SIMMY_CAS_ACCEPT=300 SIMMY_PAD_SEQ="start,a,right,a,up,a,a,left,a,house"`
reaches story lot 9 (the last `a` also opens the action menu at house load: add `b` after `house`).
`SIMMY_DEV_MENU=1` + Y on the main menu lists every lot. `SIMMY_STORY=0` keeps the scenario and tutorial
still. `SIMMY_SIM_START=x,y` puts the sims on a tile (e.g. 12,22 toilet, 18,11 sofa + TV on lot 9).

## Checks worth running after a change

| Check | How | Pass |
|---|---|---|
| Interaction suite ("B-cancel suite") | Headless, `SIMMY_STORY=0 SIMMY_FUNDS=1000`, one run each with the sim next to a toilet, sofa, fridge, shower, bed (`SIMMY_SIM_START` + `SIMMY_INTERACT_PICK=<name>`), plus one that presses B while an action runs; `SIMMY_SIM_TRACE=1` prints motives | The motive moves the right way (bladder/comfort/hunger/hygiene/energy, within ~±1 of earlier runs: wall-clock jitter), `cancels: 1` in the cancel run, no `[vm] TREEASSERT` / `keeps failing` |
| Story start | default story, `SIMMY_CAS_ACCEPT=300`, ~7000 frames | lot 9 up, tutorial dialogs, no TREEASSERT |
| Two players | `SIMMY_TWO_PLAYER=1 --split=wide` (and `windows`, `gc`, `4x3`) offscreen | both views/HUDs, no crash |
| Four players (PC mod) | `SIMMY_TWO_PLAYER=1 SIMMY_PLAYERS=4 SIMMY_FAKE_PADS=4`, `split=windows`, lot 13, `p3:start`, `p4:start` ([multiplayer.md](multiplayer.md) has the full script) | `[mp4] player 3 joins`, four views and `_p1`..`_p4` shots, the extra players' menu choices queue, save + quit drops them, clean exit |
| Save / Continue | save from the pause menu (O2T -> Save -> slot A), quit, Continue in a **new process** | `[save] house 9 saved: funds ...`, same funds/objects after Continue |
| Quit + re-enter | quit to the main menu and start again in one process: New Game, load the same and another memory card, several cycles, 1P and co-op, from buy/build mode (pause on the options tab: `start,a,down,a,down,a` once the pause menu remembers that tab; the first time `right` x6 first), RSS sampled per cycle; under ASan too. Run cycles with `SIMMY_STORY=0`: story mode's tutorial dialogs take the quit presses, and `SIMMY_PAD_SEQ`'s `house` step then goes on without a quit (count the `player 1's sim: object` lines: one per game). Also lot changes (`SIMMY_LEAVE_LOT_TEST=600,5,2,19`) and development-menu re-entry soaks (`SIMMY_SOAK_LOT`) | each game reaches live mode with fresh object ids (player 1's sim is object 149 again), no crash; RSS flat after the first few games except for a few one-off steps (caches filling, the allocator: see "Memory growth" below) |
| Free play | dev menu or FAM screens: enter lot 13, evict/move/bulldoze, Create-a-Family | logs `[hood] ...`, no crash |
| Exit path | the cases in [shutdown-and-crashes.md](shutdown-and-crashes.md) | exit 0, `[simmy] clean exit (code 0)` last |
| Determinism | record + replay with perturbations; `simmy --mathcheck` | hashes match; mathcheck prints `mathcheck 3a67a7608e988c00` on every build and CPU ([determinism-and-netplay.md](determinism-and-netplay.md)) |
| Visual | the same script on the old and new binary, `--screenshot`/`SIMMY_SHOTS`, diff images (only wall-clock animated pixels may differ) | |
| New stubs | diff the `[stub #n] ADDR` lines of a run before/after | only expected ones |

ASan: a gcc build with `-fsanitize=address` (headless runs, `detect_leaks=0`) found the CAS
use-after-free bugs and the person-kill one (animator after ESim); worth repeating after
memory-sensitive changes. With gcc 13, `-fsanitize=undefined` (its `null` check) breaks abseil's
constant expressions in the precompiled header: use
`-fsanitize=address,bounds,return,shift,signed-integer-overflow,vla-bound,bool,enum`; configure with
gcc and the `SDL_*`/`AURORA_*`/`FETCHCONTENT_SOURCE_DIR_*` cache values of a working build (headless:
`SDL_UNIX_CONSOLE_BUILD=ON`), but **not** its `FETCHCONTENT_BASE_DIR` (set it to the ASan build's own
`_deps`: otherwise the ASan build compiles SDL, fmt, zstd... into the working build's `_deps` and that
build links them next time). Known UBSan reports that are GC behaviour: `psystem.cpp` random seed
overflow, `SAnimator2::Update`'s event frame doubling for a looping animation with no duration (GC
also stores the overshoot as the fraction).

## Memory growth ([`tools/mtrack`](../tools/mtrack))

RSS sampled once per game (e.g. at each `house` step of a quit / re-entry sequence, 15-20 games) shows
whether memory grows; `tools/mtrack` shows what. `mtrack.so` is an `LD_PRELOAD` allocation tracker: every
allocation is recorded with its caller's backtrace and an epoch; `SIGUSR2` starts the next epoch and
`SIGUSR1` writes a dump. Send both at each house load (USR1, then USR2 a second later), then
`python3 tools/mtrack/mta.py <MTRACK_OUT> 3 17` lists the backtraces whose allocations made in games 3..17
are still live at the last dump, with their bytes per game: a leak shows the same amount in every game, a
cache a few uneven amounts that stop, a one-off allocation one game. The dumps' first table (live heap
per game) against RSS tells heap growth from allocator effects. With the tracker the process has about
200 MB more RSS (its tables). Headless runs never draw, so GPU-side caches (aurora's texture caches,
bounded LRUs) need an offscreen run.

LeakSanitizer: the game ends with `_Exit`, which skips LeakSanitizer's own check; in an ASan build
`SIMMY_LSAN=1` (with `ASAN_OPTIONS=detect_leaks=1`) runs it at the end (`[simmy] leak check: ...`). It only
reports memory nothing points to any more (e.g. an object replaced without being freed), not memory kept
reachable by a cache or a list that grows: that is what mtrack is for.

## Scripts worth adding to `tools/` (not in the repo yet)

- `run.sh`/`run.py`: one headless or offscreen run with its own working dir and `XDG_DATA_HOME`, env and
  pad sequence as arguments, log + shots collected.
- The interaction suite above (prints the five motive values, the cancel count and the TREEASSERT count).
- `exit_test.sh`: the exit-path cases.
- `det_cmp.py`: first divergent tick of two `SIMMY_DET_HASH` files and which parts differ; `lines_cmp.py`
  for `SIMMY_DET_LINES`.
- A Windows replay check (PowerShell) for cross-compiler determinism (exists outside the repo as
  det_check.ps1: `--mathcheck` vs the Linux value, then the L.rec replay vs the Linux hashes).
- Apt helpers: extract a movie's BIG (`resdump.py save flashes <movie>` then unpack), an ActionScript
  disassembler, a timeline dumper.
- Save inspectors: list/validate sections of a `.ngh`/`.gci` (strip the 0x40-byte GCI header), decode
  `NGBH`/`NBRS`/`FAMI`/`User`.
- `stubunits.py`: map `[stub]` addresses of a log to their original units (`data/units.tsv`).

## Every `SIMMY_*` variable

Development only: the launcher removes all of them except `SIMMY_INI` from the game's environment.
"Trace" switches print to stdout/stderr.

### Data, settings, startup

| Variable | Effect |
|---|---|
| `SIMMY_DATA=<dir>` | Extracted data folder when neither `--data` nor `--iso`/`iso=` is given |
| `SIMMY_INI=<file>` | The settings file (also honoured by the launcher) |
| `SIMMY_GFX="k=v;k=v"`, `SIMMY_GFX_<KEY>=v` | Settings from the environment (below the command line) |
| `SIMMY_BACKEND`, `SIMMY_SPLIT` | Older aliases for `backend=` / `split=` |
| `SIMMY_SKIP_INTRO=1` | Skip the EA and intro movies |
| `SIMMY_TWO_PLAYER=1` | Start new games as two-player |
| `SIMMY_CAS_ACCEPT=<frames>` / `SIMMY_CAS_ACCEPT_WARDROBE=<frames>` | Auto-accept CAS / the wardrobe's CAS (the sim is saved into the neighbourhood as Done does; the memory card screens are skipped) |
| `SIMMY_DEV_MENU=1` | The main menu's development menu (lot lists) |
| `SIMMY_NET_LAG`, `_JITTER`, `_STALL`, `_DESYNC`, `_FAKE_BUILD/MATH/ISO`, `_ON_END`, `_DIR`, `_DELAY`, `_HASH_EVERY`, `_TIMEOUT`, `_TRACE` | Network play tests (two processes on 127.0.0.1): [netplay.md](netplay.md) |
| `SIMMY_STORY=0` | Keep the story scenario and tutorial objects still |
| `SIMMY_TUTORIAL_FROM=<tree hex>` | Start the tutorial's level tree at the node calling that tree |
| `SIMMY_UNLOCK_LOTS=2,3` | Count those locations as unlocked |
| `SIMMY_UNLOCK_CLOTHING=g1,g2` | Unlock those clothing groups (marked new) in family 1 when it is read |
| `SIMMY_VIRTUAL_PADS=N` | Attach N virtual SDL gamepads |
| `SIMMY_PLAYERS=N` | `players=N` (PC mod: 3-4 players, [multiplayer.md](multiplayer.md)) |
| `SIMMY_FAKE_PADS=N` | Scripted pads 1..N plugged in from the start (`p3:`/`p4:` script steps press pads 3 and 4) |
| `SIMMY_MP4_START`, `SIMMY_MP4_OBJECTS`, `SIMMY_MP4_PICK`, `SIMMY_MP4_TRACE` | Extra players' test aids: put the joining sim on a tile, put an object in their action menu, start the menu cursor on a row, trace pads and objects ([multiplayer.md](multiplayer.md)) |
| `SIMMY_IO_STATS=1` | File opens/reads summary at exit |
| `SIMMY_PERF=1` | The frame-hitch profiler ([performance.md](performance.md)); `SIMMY_PERF_LOG=<file>` where `perf_log.txt` goes (default: next to the exe), `SIMMY_PERF_MS=<ms>` the hitch threshold (default 25) |
| `SIMMY_DISC_CACHE=0\|1\|auto\|warm` | `disc_cache` for A/B tests ([performance.md](performance.md)) |
| `SIMMY_FRAME_PACING=precise\|sleep\|vsync` | `frame_pacing` for A/B tests |
| `SIMMY_DRAWS_BLOCKING=0` | Offscreen: skip draws whose GPU pipeline is not built yet, as a window does (default offscreen: wait for it), to measure pipeline costs |
| `SIMMY_LOG_LOADS=1` | Every resource load |
| `SIMMY_WATCHDOG=N` | (Windows) all thread stacks to `simmy_watchdog.txt` after N s |
| `SIMMY_LSAN=1` | (ASan builds) run LeakSanitizer's check at the exit (`_Exit` skips its own) |
| `SIMMY_LAUNCHER_KEEP_ENV=1` | (launcher) pass `SIMMY_*` through to the game |

### Graphics and shots

| Variable | Effect |
|---|---|
| `SIMMY_SHOTS=f1,f2,...` | Extra screenshots (with `--screenshot`'s path + `_<frame>`) |
| `SIMMY_FRAMES_FROM_HOUSE=1` | `--screenshot`, `--frames`, `SIMMY_SHOTS`, `SIMMY_GFX_LOWRES_UNTIL` count from the first house frame |
| `SIMMY_GFX_LOWRES_UNTIL=<frame>` | Render at scale 1 until that frame (cheap 1080p tests) |
| `SIMMY_GFX_HIRES_SHOTS=1`, `SIMMY_GFX_LOWRES_SCALE=<f>` | Full resolution only for the frames before each shot; the scale of the other frames |
| `SIMMY_GPU_DEBUG=1` | aurora debug logging (offscreen) |
| `SIMMY_GX_PROCESSING=thread\|inline\|drain` | aurora's GX command processing mode |
| `SIMMY_RS_CACHE=0` | Disable the render state cache's change filtering |
| `SIMMY_FRAME_STATS=1` | Every 300 frames: game ms, aurora end ms, GX bytes |
| `SIMMY_HUD_TRACE=<frame\|d<devframe>>` | Log HUD anchor/window decisions |
| `SIMMY_WIN2_TRACE=<frame\|d<devframe>>` | Log every 2D mapping (split=windows) |
| `SIMMY_TEST_MODE=plain\|gradient\|logo\|font\|rawgx`, `SIMMY_TEST_CLEAR=1` | `--render-test` variants |
| `SIMMY_TEST_RENDERSURFACE=1` | Render-to-texture readback test |
| `SIMMY_TEST_LOADING=1` | Show the loading screen from the start |
| `SIMMY_QD_TEST=1` | Same as `--qd-test` (quickdata strings) |
| `SIMMY_SHADER_TRACE=1`, `SIMMY_SHADER_STATES=0` | Multi-state shader changes; keep them in their default state |
| `SIMMY_HOUR=<0..23>` | Set the clock's hour once |
| `SIMMY_LIGHT_TRACE=1` | Light grid rebuilds |
| `SIMMY_WALLS=0\|1\|2` | Walls up / cut-away / down at build |
| `SIMMY_PARTICLE_TRACE=1` | Effects attachments, emitters, bone/ghost/text particles |
| `SIMMY_FX_OBJECT=<object>=<attachment>` | Force an effects attachment on an object |
| `SIMMY_FX_FRAME=dof[=...],bloom[=row],blur[=level]` | Force frame effects |
| `SIMMY_BONE_FX=<attachment>,...` | Attach effects to player 1's sim |
| `SIMMY_GHOST_TEST=1` | Player 1's sim drawn as a ghost |
| `SIMMY_TEXT_PARTICLE_TEST=<type>:<word>` | Tag a word with a text particle |
| `SIMMY_TEXT_FX_TEST=<code>:<word>` | Put an embedded text code before every `<word>` in Apt text (e.g. `ROCK_L_500A:Welcome`, `OUTLINE_FF0000:Welcome`; [ui.md](ui.md) "Text effects") |
| `SIMMY_CENSOR_TEST=<bits>` | Censor player 1's sim |
| `SIMMY_CENSOR_OFF=1` | No censor mosaic on any sim (screenshots of sims in toilets, showers and tubs: alignment checks) |
| `SIMMY_FIND_TRACE=1` | Every candidate `TryFindFunctionalObject` scores (a function's object: the sink a dirty dish goes to, the table a plate goes on): its data, distance and score ([`objectsim.cpp`](../src/game/objectsim.cpp)) |
| `SIMMY_SKILL_METER_TEST=<fill>[,repair]` | Skill/repair meter over player 1's sim |
| `SIMMY_SPRITE_TRACE=1`, `SIMMY_SPRITE_HOLD=1` | Balloons: trace; keep up for shots |
| `SIMMY_SLOT_TRACE=1`, `SIMMY_SLOT_OFFSET_OFF=1` | Slot offsets: trace; disable |
| `SIMMY_COUNTER_TRACE=1` | PC mod: mods: the piece (plain, corner, ends) each converted counter shows and its neighbours |
| `SIMMY_COUNTER_EAT_TRACE=1` | PC mod: mods: each bar-stool search at a counter (who, which counter, the stool and eating slot found or none) and each check before the plate goes down ([mods.md](mods.md) "Bar stools at counters") |
| `SIMMY_SEAT_TRACE=1` | PC mod: mods: every 30 frames, each sim's container (seat, bed), its state, the sim's root and pelvis and the seat offset drawn ([mods.md](mods.md) "Seating and beds") |
| `SIMMY_OBJ_STATE_TRACE=1` | Every object's state change as drawn (`ISimsObjectModel::Update`): frame, object, old and new state, the state's model and animation |
| `SIMMY_OBJ_STATE_FORCE="<id>:<state>:<from>-<to>;..."` | Draw an object in that state between those frames (its animation, model, effects; the simulation's state untouched): appliance doors open for screenshots |
| `SIMMY_OBJ_NODE_TRACE=<text>[,<text>...]` | Objects whose file name contains one of the texts log their character nodes' object-space poses (translation, turn and axis) every 20 frames; PC mod: mods: an animated appliance's rest pose ([mods.md](mods.md) "Appliances") |
| `SIMMY_PORTRAIT_DUMP=<prefix>` | Write each finished portrait as BMP |
| `SIMMY_UI3D_TRACE=1` | UI3D displays every 60 frames |
| `SIMMY_GREEN_TEST=1` | Light player 1's sim as a green person (`ESim::Draw`'s ghost-green ambient) |
| `SIMMY_SHOT_ON_DIALOG=<text>` | Take `--screenshot` 100 frames after the first dialog containing text |

### UI

| Variable | Effect |
|---|---|
| `SIMMY_UI_TRACE=1` | Every UI command; dialog texts `[dialog] ...` |
| `SIMMY_APT_TRACE=1` | Apt bytecode trace |
| `SIMMY_APT_WARN=1` | "call of undefined method" warnings |
| `SIMMY_APT_TEST=<movie>,...` | Load and summarise Flash movies |
| `SIMMY_DIALOG_TRACE=1` | Tree dialogs |
| `SIMMY_DIALOG_AUTO=<frames>` | Answer modal dialogs with A after that long |
| `SIMMY_RMD_AUTO=<frames>` | Close reward moments with A |
| `SIMMY_REWARD_TEST=<n>[:<kind>]` | Spawn a reward moment on the n-th HUD update |
| `SIMMY_MONEY_TEST=<amount>` | Show that amount on the HUD |
| `SIMMY_ACT_SHOW=1` | Action queue widget in direct control too |
| `SIMMY_TOMBSTONE_TEST=<frames>` | Player 1's tombstone flow |
| `SIMMY_ING_TEST=<frames>[,<level>[,<object>]]` | Open food creation for player 1 |
| `SIMMY_RECIPES_UNLOCKED=1\|2` | Show every recipe unlocked (2: also new) |
| `SIMMY_FAM_TRACE=1` | Free-play lots and families |
| `SIMMY_FIRE_CODE=<0..1>` | Fire code meter value (overrides the GC meter and `fire_code_limit`) |
| `SIMMY_FIRE_CODE_TRACE=1` | Each fire code poll: objects, weighted objects, models, model memory and the fill |

### Simulation and control

| Variable | Effect |
|---|---|
| `SIMMY_FIXED_DT=1` | One retrace of game time per frame |
| `SIMMY_SIM_SPEED=<n>[@<house>]`, `SIMMY_SIM_SPEED_JOB=<n>` | Simulator speed once a house is up / once player 1 has a job |
| `SIMMY_SIM_TRACE=1` | Simulator dispatches, motives, routes, careers |
| `SIMMY_TRACE_IDS=98,126` | Also trace those objects |
| `SIMMY_GSC_TRACE=1` | Each generic sim call id (primitive 0x01) on its first use: the object and tree |
| `SIMMY_VM_TRACE=1\|<id>`, `SIMMY_VM_TRACE_TREES=<hex>,...`, `SIMMY_VM_TRACE_WHO=<text>` | Primitive trace (all / one object / some trees / name filter) |
| `SIMMY_INFLUENCE_TRACE=1\|2` | Passive influences added/removed, targets' trees; 2 also every motive change |
| `SIMMY_FIRE_TEST=<tick>,<id>[,burn]` | Start a fire at object `<id>` when the simulator reaches the tick (`,burn`: run the burn primitive 0x09 on it from player 1's sim) |
| `SIMMY_ROUTE_TRACE=1` | Routing |
| `SIMMY_WALK_TRACE=1` | Each `SAnimator2::BeginFollow` (points, start tile, follow mode, walk style, distance) |
| `SIMMY_AWARE_TRACE=1` | Awareness manager: memory awareness, aware-of objects, head turns, passive-influence reaction animations, auto-carry |
| `SIMMY_RUMBLE_TRACE=1` | Rumble notes (`SAnimator2::playRumble`: strength, kind, seconds, controller port) |
| `SIMMY_AUTO_TRACE=1\|ads` | Autonomy choices / advertisements |
| `SIMMY_AUTONOMY_IDLE=<s>` | Idle time before autonomy takes a player's sim |
| `SIMMY_AUTONOMOUS_JOB=1`, `SIMMY_PLAYER_WORK=1`, `SIMMY_CAREER_MOOD=<v>` | Career test harnesses |
| `SIMMY_WANTS_TRACE=1`, `SIMMY_WANTS_NOTIFY=<tick>\|story:<event>@<tick>[#house]` | Wants and fears trace (also the HUD's icons, the want/fear dialog and the MDI box) / forced notifications (a numeric tick fulfils player 1's first shown want: the new want flips in and its dialog follows once the lot intro is over, e.g. `SIMMY_DET=1 SIMMY_WANTS_NOTIFY=500`) |
| `SIMMY_NPC_TRACE=1` | Each service NPC record as the object folder loads it (gender, kind, name, model id: `ESim::initModel` poses that model for the NPC) |
| `SIMMY_FUNDS=<n>`, `SIMMY_BUY_FUNDS=<n>` | Family funds on the lot / when the catalogue opens |
| `SIMMY_SIM_START=x,y` | Put the sims on a tile at each house load |
| `SIMMY_INTERACT_PICK=<name>`, `SIMMY_INTERACT_PICK_PLAYER=1\|2`, `SIMMY_INTERACT_NEAR=x,y;...\|#id`, `SIMMY_MENU_ITEM=<n>` | A starts the nearest object's named interaction at once (instead of the menu; `a,b` any of them; with `SIMMY_INTERACT_NEAR`, `a;b;...` the n-th A press takes the n-th part); limit to a player; objects near a place; which menu item a check tree gives |
| `SIMMY_SOCIAL_PICK=<text>\|a,b\|@a,b`, `SIMMY_SOCIAL_ROUNDS=<n>` | Pick socials automatically; leave after n |
| `SIMMY_SIM_COLLIDE=0` | Turn off the PC people-vs-people collision for steered sims |
| `SIMMY_SAVED_STACKS=0` | Start every loaded object at its main tree |
| `SIMMY_SAVE_SELFTEST=<tick>` | Write and re-read every object's stream at that tick |
| `SIMMY_RECON_TRACE=1` | Report streams not read to their end; person data as read |
| `SIMMY_DUMP_WORLD=1` | Dump tiles, rooms, walls and objects at house load |
| `SIMMY_BUILD_TRACE=1\|2` | Build tools / house rebuild; 2 also wall segments; counters with a sink, hedges |
| `SIMMY_ROOM_TRACE=1` | The room score constants and every room recomputed (area, floor, walls, objects, impact, light, score) |
| `SIMMY_PLACE_TRACE=1` | Placement event trees run (placed, picked up, user placed / picked up, wall adjacency) |
| `SIMMY_MODS=<dir>` | PC mod: mods: the mod folder when simmy.ini has no `mods=` ([mods.md](mods.md)); place a mod object with `SIMMY_PLACE_TEST=<tick>:<its GUID>@x,y` |
| `SIMMY_MODS_VERBOSE=1` | PC mod: mods: a log line per mod object even with more than 20 ([mods.md](mods.md)) |
| `SIMMY_PLACE_TEST` mod actions | `probe:<guid>@x,y,dir\|x,y,dir...` first legal spot (nothing placed), `try:<guid>@...` place at the first legal spot, `info#<guid>` an object's placement data (heights, flags, wall flags, footprint; a group's parts each) ([`placemanipulator.cpp`](../src/game/placemanipulator.cpp)) |
| `SIMMY_CAM_LOOK=x,y,distance,yaw,pitch[,height]` | The house camera looks at a tile (screenshots; also during the lot intro's camera shots; [`camera.cpp`](../src/game/camera.cpp)) |
| `SIMMY_PLACE_TEST="<tick>:<action>;..."` | At sim ticks: `<guid hex or object file name>@x,y[,dir]` buy-place a new object, `model#<hex>` list the object files drawing that model, `grab#<id>` grab and put back, `sell#<id>` grab and destroy (sell), `create:<object file>@x,y` make an object and put it there as CreateObject does (dynamic ones such as `efx_flood` merge), `wall@x,y,seg` delete a wall segment, `chain@x,y` print a tile's object chain, `paperroom@x,y,i[,sell]` paper (strip) the room with WallSet entry i ([`placemanipulator.cpp`](../src/game/placemanipulator.cpp)) |
| `SIMMY_CTL_TRACE=1`, `SIMMY_COOP_TRACE=1`, `SIMMY_SWITCH_TRACE=1`, `SIMMY_CAM_TRACE=1` | Classic cursor (and its Go Here tile); both players; sim switching; the lot intro camera's steps |
| `SIMMY_HL_TRACE=1` | Direct control's highlights every 60 frames: the objects the influence map lists at the sim's tile, the lit ones, the sim's location and facing |
| `SIMMY_KBM_TRACE=1`, `SIMMY_KBM_NOPICK=1` | Keyboard/mouse input made; floor-only mouse picking |
| `SIMMY_LEAVE_LOT_TEST=<frames>[,<mode>[,<house>[,<n>]]]` | Leave the lot (mode 5 to a house, 4 the story end); `<n>` more times back and forth between the two lots (`[leave-lot]` lines) |
| `SIMMY_PRISTINE_SAVE=1` | Save as author mode (pristine reset first) |
| `SIMMY_TEST_QUIT_BG=N` | Quit when the N-th background routine is queued |

### Audio

`SIMMY_AUDIO=0`, `SIMMY_AUDIO_WAV` / `SIMMY_AUDIO_DUMP=<file>`, `SIMMY_AUDIO_TEST=<list>`,
`SIMMY_SOUND_TRACE=1`, `SIMMY_SOUND_EVENT="<frames>:<event>[@<source>];..."` ([audio.md](audio.md)).

### Input drivers and determinism

`SIMMY_PAD_SCRIPT`, `SIMMY_PAD_SEQ`, `SIMMY_PAD_SEQ_UI`, `SIMMY_PAD_HOLD`, `SIMMY_PAD_GATE`,
`SIMMY_KBM_TEST`, `SIMMY_OVERLAY_TEST` (above); `SIMMY_SOAK` with `SIMMY_SOAK_START`, `_EVERY`, `_QUIT`,
`_P2`, `_SPEED`, `_LOT`, `_NGH`, `_DOWN`, `_RSS`, `_HANG`, `_STALL`, `_SWITCH`, `_SWAP` ([`pc_soak.cpp`](../src/platform/pc_soak.cpp));
`SIMMY_DET`, `SIMMY_DET_SEED`, `_RECORD`, `_REPLAY`, `_BOOT`, `_HASH`, `_DUMP`, `_DUMP_TICKS`, `_REF`,
`_REF_AFTER`, `_REF_QUIT`, `_LINES`, `_RANDLOG`, `_SPEED`, `_JITTER`, `_NOPACE`, `_QUIT`
([determinism-and-netplay.md](determinism-and-netplay.md)); `simmy --mathcheck [file]` (the deterministic
math's self-test, same docs page).

When you add a switch, add it here (and keep it off by default).
