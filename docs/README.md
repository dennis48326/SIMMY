# SIMMY engine documentation

How the port works, organised by engine system. Read this instead of rediscovering the engine from
the code. The user guide (settings, controls, building) is the repo [README](../README.md); the porting
approach (generator, address markers, 64-bit data loaders) is [PORTING.md](../PORTING.md). The raw
per-session working notes these pages were distilled from are in [archive/](archive/README.md); they are
history, not reference (many of their "not ported" lists are out of date).

## Pages

| Page | Covers |
|---|---|
| [architecture.md](architecture.md) | Boot to shutdown, the frame (`EApp::SystemUpdate`), frame vs sim tick, threads, the state machines (front end, pre-game, in-level, live mode), where `src/platform` hooks in |
| [simulation.md](simulation.md) | The simulator tick, objects and persons, the behaviour VM (BHAV trees, primitives, scopes), interactions and the action queue, autonomy, motives, wants and fears, relationships, routing, social mode |
| [build-buy-and-world.md](build-buy-and-world.md) | The lot: tiles, walls, floors, rooms, the house rebuild, buy/build catalogue, placement, grab mode, wall/floor/paint tools |
| [cas-and-neighbourhood.md](cas-and-neighbourhood.md) | Create-a-Sim, the neighbourhood (families, neighbours, houses), free play, lot changes, in-game CAS, portraits |
| [input-and-control.md](input-and-control.md) | Pads, the controller manager, players and pads, command maps, direct control, the classic cursor, sim switching, keyboard and mouse |
| [ui.md](ui.md) | Apt (Flash) runtime, UI targets and command tables, UIDB, screens, flows, the HUD and its widgets, pause menus, dialogs, widescreen HUD placement |
| [rendering.md](rendering.md) | GX through aurora, the render context and display lists, the state cache, EFB and internal resolution, aspect/widescreen, split screen and two windows, effects, the aurora patch |
| [camera.md](camera.md) | `ESimsCam` (the game camera), `CameraDirector` (shots), per-player cameras, what is still stubbed |
| [audio.md](audio.md) | The SDL3 mixer, engine audio, the BoxX sound director, UI sounds, ambience, footsteps, the separate sound RNG |
| [files-and-saves.md](files-and-saves.md) | The virtual file layer (disc image / folder), resources and archives, quickdata, the neighbourhood file, memory card saves, paths written |
| [coop-and-players.md](coop-and-players.md) | Two-player mode end to end, split modes, private screens, everything hard-coded to two players (for 3-4 player work) |
| [determinism-and-netplay.md](determinism-and-netplay.md) | `SIMMY_DET`, RNG streams, record/replay, hashing, rules for deterministic code, remaining netplay work |
| [shutdown-and-crashes.md](shutdown-and-crashes.md) | The exit sequence, why static destructors are skipped, crash capture, the watchdog |
| [launcher.md](launcher.md) | SimmyLauncher internals: disc check, ini editing, starting the game, failure reporting |
| [testing.md](testing.md) | How the port is tested: headless/offscreen runs, scripted input, the regression suite, every `SIMMY_*` variable |
| [known-gaps.md](known-gaps.md) | Unported/stubbed areas that matter, GameCube behaviour kept on purpose, known limitations |
| [porting-audit.md](porting-audit.md) | Ranked audit of every unported function and partial gap: what is missing in GC play, what is N/A (EyeToy, other consoles, dev tools, dead code) |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Keeping these pages current |

## Finding things

The port is one hand-written C++ file per original source file (`src/<area>/<file>.cpp`), on top of a
generated layer (`gen/`, do not edit) that declares every class and stubs every function not yet ported.

| You have | Do |
|---|---|
| A GC address (`8008E4C0`) | `grep -rn "// 8008E4C0" src` finds the port. Not found: `grep 8008E4C0 gen/functions.tsv` gives the original file, status (`ported`/`stub`) and signature; `data/symbols.tsv` has every symbol (functions `F` and data) with size |
| A function name | `grep -n "Name" gen/functions.tsv` (address, original unit, status, signature) |
| A raw-decompiler vtable call `*(obj+X)+0xOFF` | `python tools/vslot.py Class 0xOFF` |
| The original decompilation | `python tools/show.py <raw dir> ADDR\|Name` (the `raw/` Ghidra output is in the decomp package, not in this repo) |
| An initialised table/string in the ELF | `python tools/elfdata.py <elf> <symbol\|0xADDR> [size]` |
| A resource in `DATA/*.ARC` | `python tools/resdump.py <data dir> list\|dump\|save <manager> <name\|0xID>` |
| Which original unit an address is in | `data/units.tsv` (address ranges per unit) |
| A class's generated declarations | `python tools/class_decls.py Class` |
| Progress by area | `python tools/progress.py` |
| A hand-written class layout | `include/simmy/classes/<name>.hpp` (`// @class Name [after A B]`); fields carry their GC offsets (`// +0x44`) and often the GC global (`DAT_80475BC8`) |
| A PC-only service | [`include/simmy/platform.hpp`](../include/simmy/platform.hpp) (the contract of `src/platform/`) |

A function is "ported" when its address appears as `// XXXXXXXX` above it anywhere in `src/` (the
generator, `tools/gen_port.py`, then stops stubbing it; `gen/functions.tsv` is rebuilt each build). A
comment such as `GC 8012606C` does **not** count, so the generated stub is still linked and still called
by any code that calls the member. Unported functions log once on first call:
`[stub #n] ADDR name` (`src/platform/stub.cpp`; de-duplicated by address, so two `SIMMY_STUB` calls with
the same address log only once). The log of a run is the to-do list for that path.

**Header comments go stale.** Many file and class header comments still say "not ported" for things
ported since (e.g. [`person.hpp`](../include/simmy/classes/person.hpp), [`livemode.cpp`](../src/game/livemode.cpp),
[`hudtarget.hpp`](../include/simmy/classes/hudtarget.hpp),
[`aptviewer.cpp`](../src/game/aptviewer.cpp)). Trust function bodies, `SIMMY_STUB` calls and
`gen/functions.tsv` over file headers.

## Glossary

| Term | Meaning |
|---|---|
| GC | The GameCube original (`u2_ngc_release_dvd.elf`, The Sims 2 GC, build 1.mainline_SKU1.0.63). "GC behaviour" = what the original does |
| PC stand-in / PC deviation | Code marked `PC:` that replaces or changes an original function (e.g. the build tools' glue, the route walker) |
| aurora | The library that implements the Dolphin SDK (GX, PAD, CARD, VI, THP, OS) on SDL3 + Dawn (WebGPU). Pinned by `SIMMY_AURORA_TAG` |
| Frame | One `EApp::SystemUpdate` (802E24B8): pads, UI, game state machines, draw. Paced to 60 Hz ("retraces") |
| Retrace | 1/60 s; the game measures frame time in whole retraces (`EEngine::RetraceUpdate`) |
| Tick | One simulator step (`cSimulatorImpl::SimulateOneTick` 8014095C): every object's tree runs once; 42 ms of simulated time, 2 game seconds. 0..n per frame |
| `_dt` | The frame time the game uses (seconds); during `ELiveMode::Update` the simulator swaps in its simulated time and `RestoreTrueDt` puts it back |
| Object | `cXObjectImpl`: anything on the lot with a behaviour (furniture, the scenario controller, people). Has an id, 72 object data variables (`EdithVariableSet`), attributes, a tree stack |
| Person | `cXPersonImpl` (a `cXObjectImpl` + `cXPerson`): a sim on the current lot: motives, action queue, animator (`SAnimator2`) |
| Neighbour | `Neighbor` (neighbor.cpp): the persistent record of a sim in the neighbourhood (personality, skills, career, relationships, wants and fears); saved in `NBRS`. A person on the lot points to its neighbour (`cXPersonImpl::m_neighbor`) |
| Family | `FamilyImpl` (family.cpp): members (GUIDs), funds, house; the player family is `PlayerFamilyImpl` |
| Selector | `ObjSelector`: an object definition (catalogue entry, models, trees) from the "SimsObjects" quickdata; "user" selectors hold CAS-made sims' looks |
| ESim | The 3D side of a person (esim.cpp): models, shadow, head, censor |
| Player | A human: 0 or 1 (`_gd.m_players[2]`). Not a sim. A player controls a *selected person* (`EGlobal::GetSelectedPerson(p)`) |
| Primary neighbour | A player's own sim (`Player::GetPrimaryNeighbor`: player family member `p`); the selected sim can be another family member after Y |
| Interactor | A player's control mode: 0 direct control, 1 classic cursor (SimInteractor), 2 grab, 3/4/5 build tools, 6 place, 7 social menu |
| BHAV / tree | A behaviour tree (SimAntics, from PC The Sims), run by `TreeSimImpl` |
| TTAB | An object's tree table: the interactions it offers (check tree, action tree, name, flags) |
| Apt | EA's Flash player (`src/libaptz`): the whole UI is Flash movies (`flashes.arc`) |
| Target / TLA | A game-side UI object (`UIObjectBase` subclass) serving one screen's variables, named by a three-letter code: `H2D` HUD buttons, `MOD` mood widget, `CXM`/`CSM` context sim menu, `ACT` action queue, `INT` interest meter, `PAZ` pause, `PCT` catalogue, `ING` food creation, ... (see [ui.md](ui.md)) |
| UIDB | The UI's named integer/string variables (`UIDB::UIDBSetInt`), read by movies (`uidb:`) |
| Flow | A UI "goto" request (`flowGoto <name>`), turned into a number by `FlowGotoTarget` and served by the current state (e.g. `InLevelBaseState::Update`) |
| Quickdata | `ERQuickdata`: big-endian table images (UI strings, object definitions, game tables), read with `ERQuickdata::U32/S16/Ptr/...(row, off)` |
| `_gd`, `_globals` | `GameData` (gamedata.hpp) and `EGlobal` (eglobal.hpp): the game's two big global objects |
| `Globs_p*` | The simulation singletons (`Globs_pSimulator`, `Globs_pObjectModule`, `Globs_pNeighborhood`, `Globs_pFixedWorld`, `Globs_pRoomManager`, `Globs_pHouse`, `Globs_pSound`, ...) |
| E-prefixed classes / controller commands | `E*` = EA engine classes (`EController`, `EGraphics`, ...). Controller *commands* (`ECTRL_CMD`) are numbers the command map (`g_simsCtrlCommands`) binds to button masks; interactors use their own command numbers (see [input-and-control.md](input-and-control.md)) |
| Det mode | `SIMMY_DET=1`: virtual clock, one retrace of game time per frame ([determinism-and-netplay.md](determinism-and-netplay.md)) |

## Rules for new code (summary; details on each page)

- **MSVC must build it.** C++20, no VLAs, no GNU extensions, no designated initialisers, include what
  you use (`<algorithm>`, `<cstring>`, ...). Code is verified on Clang/Linux; Windows
  builds use MSVC. See [CONTRIBUTING.md](CONTRIBUTING.md).
- **Faithful first.** Port from the raw decompilation, keep the address marker above each function, name
  fields with their GC offset. Mark every deviation `PC:` with the reason.
- **Big-endian data stays big-endian** in loaded images; read it through `simmy::be16/be32` or
  `ERQuickdata` accessors (see [files-and-saves.md](files-and-saves.md)). 64-bit pointers never go into
  32-bit slots of game data (store object ids instead, as the shadow/light callbacks do).
- **Determinism:** simulation code draws only from `GetNextRandomNumber` / `simmy::sim_rand()`, UI code
  from `simmy::ui_rand()`, sound code from `simmy_sound_rand()`; never `std::rand`, never wall-clock time
  in the simulation ([determinism-and-netplay.md](determinism-and-netplay.md)).
- **UI must stay per player.** Anything a player opens takes that player's pad, draws in that player's
  half/window and uses that player's selected sim ([coop-and-players.md](coop-and-players.md)).
- **Do not run the game's static destructors** and do not add exit paths that bypass
  `platform_shutdown_and_exit` ([shutdown-and-crashes.md](shutdown-and-crashes.md)).
- **Input enters in one place** (`ENgcControllerManager::Update`) so it can be recorded and replayed.
- **New `SIMMY_*` switches** go in [testing.md](testing.md); new settings in the repo README and
  `simmy.ini.example`.
