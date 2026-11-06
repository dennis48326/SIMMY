# Architecture: boot, the frame, threads, game states

## Overview

The port keeps the GameCube program structure: EA's engine (`src/engine`, `src/base`) runs an
application object (`ESimsApp`, `src/game/appmain.cpp`) whose per-frame update drives nested **state
machines** (front end, pre-game, in-level) and a **game state manager** (live mode). Live mode owns the
house, the simulator and the players' cameras. EA's GameCube platform layer (`e_ngc*` files) is rewritten
for PC on aurora; `src/platform/pc_*` adds what the PC needs (window, settings, input, split screen,
overlay, determinism, exit). The app "thread" runs on the process main thread.

```
pc_main.cpp main()                       src/platform/pc_main.cpp
  settings, [keys], game data, aurora_initialize, overlay, pad ports
  simmy_game_main -> MainInit -> StartMainThread    src/engine/e_main.cpp
    ENgcEngine::Idle -> EApp::RunOnCallingThread    (PC: no app thread)
      EApp::Main: EEngine::Init, ESimsApp::Init, loop SystemUpdate until m_quit, ShutdownThreads
  platform_shutdown_and_exit(rc)          (no static destructors)
```

## Boot

| Step | Where | What |
|---|---|---|
| Process entry | [`pc_main.cpp`](../src/platform/pc_main.cpp) `main` (aurora renames it) | Unbuffered stdout/stderr; parses `--headless`, `--offscreen`, `--data`, `--frames`, `--screenshot`, `--render-test`, `--qd-test`, `--fs-test` and graphics settings (`settings_parse_arg`); `settings_load` (simmy.ini, also `iso=`); `kbm_init` ([keys]); `open_game_data` (`--data` > `--iso`/`iso=` > `SIMMY_DATA`, else error + exit 1) |
| aurora | same | `aurora_initialize` (window or headless surface for `--offscreen`, per-build pipeline cache dir `cache_<exe size>_<mtime>` under the pref path, MSAA/vsync/backend from settings), `overlay_init`, crash handler re-install (Windows), pad ports 2-4 set to auto, aurora's keyboard-to-pad bindings cleared (the keyboard is SIMMY's, [input-and-control.md](input-and-control.md)). `--headless` skips aurora entirely |
| `SIMMY_SKIP_INTRO` | same | Sets the config block's SkipEAMovie (+0x43C) / SkipIntroMovie (+0x3A8) |
| `MainInit` 802E391C | [`e_main.cpp`](../src/engine/e_main.cpp) | Heaps (`ENgcEngine::InitMemoryManager`: host-backed EAHeaps), console, `EGlobalManager::Startup` (global objects), args |
| `StartMainThread` 802E3984 | same | `CreateAndStartAppThread` only names the app; `_pEngine->Idle()` = `ENgcEngine::Idle` ([`e_ngcengine.cpp`](../src/engine/e_ngcengine.cpp)) installs the quit handler and runs `EApp::Main` on the main thread |
| `EApp::Main` 802E2290 | [`e_app.cpp`](../src/engine/e_app.cpp) | `EEngine::Init` then `SystemInit` (= `ESimsApp::Init`), then the frame loop |
| `EEngine::InitSubsystems` 802E3240 | [`e_engine.cpp`](../src/engine/e_engine.cpp) | Clock, file system, graphics (`_pGfx->Init`), scratch buffers, resource managers, effects system, controllers, memory card, audio, particle library, EyeToy (stub) |
| `ESimsApp::Init` 800044C8 | [`appmain.cpp`](../src/game/appmain.cpp) | Boot shaders/logo, levels, intro datasets/models/anims, `EGlobal::LoadIntroRequirements` (Apt viewer, UI, OptionsRecon, UI3D), controller command map, `NghResFile`, the game state manager (`DummyMode`, `ELiveMode`), `initContinue` |
| `initContinue` 80004C80 | same | Simulator globals, `StateMachineManager` with three machines: `'SIMS'` (`TheSimsStateMachine`: the game flow), `'MUST'` (`MUStateMachine`: memory card requests), `'LdSc'` (`LoadingScreenStateMachine`) |

## The frame

`EApp::SystemUpdate` (802E24B8) runs once per frame until `m_quit`:

1. Movie start/stop (`EApp::PlayMovie`; resource loads pause while a movie streams).
2. `EEngine::PreFrameUpdate` (802E2EC8): waits on the frame semaphore (at most two frames in flight),
   `det_frame_begin` (PC), clocks, `RetraceUpdate` (frame time -> 1..15 retraces), **`_dt` =
   retraces/60** (wall clock; fixed 1/60 with `SIMMY_FIXED_DT` or `SIMMY_DET`), `_pCtrlMan->Update()`
   (pads, see [input-and-control.md](input-and-control.md)), `_pGfx->BeginFrame()` (PC: opens the aurora
   frame, `ENgcRenderer::BeginFrame`), background and resource-loader updates.
3. `ESimsApp::Update` (80005BA4): auto-map players to pads ("press start"); unless the F1 overlay pauses
   the game: `UpdateCheats`, `UpdateApt` (Apt reads pads and advances by wall-clock ms, max 66/frame),
   `UpdateGame` (`StateMachineManager::UpdateMachines(_dt)`); then `UpdateAudio`, `UpdateShaders`,
   `SimImageMaker::ServiceDeferredImages` (PC), `UpdateDraw` (opens an immediate RC, draws the machines,
   the CAS target and Apt, `dev_draw_overlay`), `UpdateReset`.
4. `EEngine::PostFrameUpdate`: `_pGfx->EndFrame()` (PC: `ENgcRenderer::EndFrame` -> `platform_end_frame`:
   overlay, screenshots, window 2 present, `aurora_end_frame`).
5. `simmy::platform_frame_tick` (PC): pump SDL events (quit, F1, F2, Alt+Enter, keyboard/mouse), clear the
   GX FIFO when headless (nothing drains it), `--frames`, det hashes (`det_frame_end`), and pacing: sleep
   to `settings_frame_interval_us()` (60 Hz, or 2/3/4 retraces for `fps_cap` 30/20/15). The game never runs
   faster than 60 Hz.

**Frame vs tick.** Live mode's `cSimulatorImpl::Simulate` (80141614) adds `min(_dt, 1/15 s) x speed`
to an accumulator (capped 2520 ms) and runs one `SimulateOneTick` per 42 ms: 0, 1 or several ticks per
frame. Per-frame work (direct-control walking, animators, routing movement, cameras, the UI) integrates
`_dt`, so the simulation depends on the sequence of frame times, not only on ticks. See
[simulation.md](simulation.md) and [determinism-and-netplay.md](determinism-and-netplay.md).

## Threads

| Thread | Code | Touches the simulation? |
|---|---|---|
| Main (app + window + GPU + input) | everything game-side | yes, the only one |
| Background worker (`BackgroundImpl`, an `EThread` on `std::thread`) | [`background.cpp`](../src/engine/background.cpp), [`e_thread_pc.cpp`](../src/base/e_thread_pc.cpp) | runs queued routines in order: house load (`BGExec_LoadHouse`), CAS init, memory card saves. The main loop keeps running loading-screen frames meanwhile. With `SIMMY_DET` each routine is waited for (`Flush`) |
| Resource loads | [`e_resloader.cpp`](../src/engine/e_resloader.cpp) | synchronous on the calling thread (GC had a loader thread); async requests complete at once |
| SDL audio callback (mixer, 5 ms frames) | [`pc_audio.cpp`](../src/platform/pc_audio.cpp) | no (voices complete in wall time; sound code has its own RNG) |
| Music stream reader | [`e_ngcstream.cpp`](../src/engine/e_ngcstream.cpp) | no (positional reads, own handle) |
| THP movie decoder | [`e_ngcmovie.cpp`](../src/engine/e_ngcmovie.cpp) | no |
| aurora GX processing | aurora (`SIMMY_GX_PROCESSING=thread\|inline\|drain`) | no; reads GX array data by pointer later, so geometry must stay alive until the frame is processed (`EGraphics::Flush`, see [rendering.md](rendering.md)) |
| Windows crash writer, `SIMMY_WATCHDOG`, soak watchdog | `pc_crash_win.cpp`, `pc_soak.cpp` | no |

## State machines and game states

`StateMachine` ([`statemachine.hpp`](../include/simmy/classes/statemachine.hpp)) is itself a
`StateMachineState`, so machines nest; `SetNextState` switches, `CallState`/`ReturnFromState` run a state
as a subroutine. `TheSimsStateMachine::Startup` (80098DB4, [`thesims.cpp`](../src/game/thesims.cpp)) adds
the states (ids in [`thesims.hpp`](../include/simmy/classes/thesims.hpp)):

| Id | State | Notes |
|---|---|---|
| 0-4 | NGC licence, memory card check, EA movie, Maxis logo, intro movie | `SIMMY_SKIP_INTRO` skips the movies |
| 5 | `TheSimsStartScreenStateMachine` ('STRT', [`startscreen.cpp`](../src/game/startscreen.cpp)) | Start is accepted only after 4 s in the screen (`m_timeInStart`), so its frame number varies between runs |
| 6, 7 | enter main menu, main menu (`MMUTarget`) | Story / Free Play / Credits; dev menu with `SIMMY_DEV_MENU=1` |
| 8 | `PreGameState` ([`pregame.cpp`](../src/game/pregame.cpp)) | New or continued game, one or two players, player 2's roommate, which CAS to call |
| 9 | `PreGameFreeplayState` ([`famtarget.cpp`](../src/game/famtarget.cpp)) | Free play's neighbourhood screen (`FAMTarget`) |
| 10, 11, 12 | CAS for player 1, player 2, Create-a-Family | `CreateASimBaseState`, [cas-and-neighbourhood.md](cas-and-neighbourhood.md) |
| 13 | `InLevelBaseState` ([`inlevel.cpp`](../src/game/inlevel.cpp)) | While a lot runs: updates/draws the game states and serves the UI's in-game flows (pause, submenus, save, quit, wardrobe, food creation, tombstone, lot leave) |
| 14 | `InLevelCreateASimState` ([`ingamecas.cpp`](../src/game/ingamecas.cpp)) | In-game CAS (wardrobe) |
| 15 | `MoviePlayerEndgameMovieState` | The story's ending movie |
| 16, 17 | Urbz leftovers (`UrbzModeNewGameState`, `UrbzCreditsState`) | Not reachable in this build |
| 18 | `LiveModeInitState` ([`livemode.cpp`](../src/game/livemode.cpp)) | The staged house load (one stage per frame, many waiting on the background thread), cameras, HUD (`HUDTarget`), simulation start |
| 19 | `LiveModeChangeHouseStateMachine` ('CHNG', [`changehouse.cpp`](../src/game/changehouse.cpp)) | Save, unload, load the next lot (taxi "Leave Lot") |

States created with `NewState<T>(id)` have no `(int id)` constructor in the generated classes; ported
states with a real constructor use it directly.

**Game state manager.** `EGameStateMan` (`ESimsApp::m_gameStates`) holds `DummyMode` and `ELiveMode`;
`InLevelBaseState::Update`/`Draw` run it. `ELiveMode::Update` (8008E4C0) each frame: det hook,
sprites, **`Globs_pSimulator->Simulate()`**, camera directors, intro camera, the direct-control
interactors (`UpdateDirectInteractors`) and each player's `ESimsCam::Update`, then `_gamedt` and
`EHouse::Update` (animators, objects' 3D side), then pending house-change/save requests (globals
`m_field450`/`m_field458`). `ELiveMode::Draw`/`DrawMain` (8008F7AC) draw each player's view (or only the
build/buy player's) and the split divider ([rendering.md](rendering.md)).

**House load** (`BGCall_LoadHouse` -> `BGExec_LoadHouse` 80014C14 on the background thread): world, simulator,
object module, room manager made on first use; the house's resources from the neighbourhood file
(`NghResFile::LoadHouseContents`); `cFixedWorldImpl::Load`; the simulator's `SIMI` state (a change of
house keeps the clock); `ObjectModuleImpl::Load`; **rooms computed before `PostLoad`** (load trees ask the
room scope); `ObjectModuleImpl::PostLoad` (each object's load tree: the scenario makes the family's
people; then demolished objects are killed); day/night shaders; `MoveFamilyOntoLot` (PC stand-in for
`NeighborhoodImpl::LoadHouse`'s move-in); rooms again (`RefreshHouse`).

## Where the platform layer hooks in

| Hook | Called from | File |
|---|---|---|
| `platform_begin_frame` / `platform_end_frame` | `ENgcRenderer::BeginFrame` / `EndFrame` | pc_main.cpp |
| `platform_frame_tick` | end of `EApp::SystemUpdate` | pc_main.cpp |
| `det_frame_begin` | `EEngine::PreFrameUpdate` | pc_det.cpp |
| `kbm_pads`, `dev_pad_script/gate/soak`, `overlay_filter_pads`, `det_pads` | `ENgcControllerManager::Update` after `PADRead` | pc_kbm/devtools/padgate/soak/overlay/det |
| `overlay_pauses_game` | `ESimsApp::Update` | pc_overlay.cpp |
| `det_live_update` | `ELiveMode::Update` | pc_det.cpp |
| `split_screen_frame`, `split_screen_player`, `split_draws_divider` | `ELiveMode::DrawMain` / `InitPlayerXWindow` | pc_splitscreen.cpp |
| `display_map_viewport` | `ENgcRenderStateCache::Apply` | pc_splitscreen.cpp |
| `simmy_apt_hud_enter/next_pass/leave/extend` | `AptDisplayList::render`, aptauxpceorgl.cpp | pc_hud.cpp |
| `coop_set_private_screen` | INGTarget, RCPTarget, G2DTarget | pc_splitscreen.cpp, pc_hud.cpp |
| `kbm_note_view`, `kbm_game_state`, `kbm_pick` | live mode draw, siminteractor.cpp | pc_kbm.cpp |
| `data_*` | `ENgcFile`, streams, movies, NghResFile, memory card icons | data_path.cpp |
| `platform_request_quit` / quit handler | F1 Quit, window close, `--frames`, `SIMMY_DET_QUIT` | pc_main.cpp, e_ngcengine.cpp |
| `game_clock_ns` | `ENgcClockMan`, Apt `GetTimer`, sound clocks | pc_det.cpp |

## Gotchas

- `_dt` is wall-clock-derived in normal play: two runs never match frame for frame (animations, "press
  start" timing, motive values in tests jitter slightly). Use `SIMMY_FIXED_DT=1` or `SIMMY_DET=1` when you
  need repeatability.
- `ESimsApp::Update` stops the whole game (UI, state machines, shaders) while the F1 overlay is open, but
  still draws; movies keep playing.
- The background thread runs game code (house load) concurrently with loading-screen frames on the main
  thread. Anything the house load touches must not be touched by the loading screen.
- aurora frames are open during `ESimsApp::Update` (between `BeginFrame` and `EndFrame`), so GX work
  issued in the update phase belongs to the current frame.
