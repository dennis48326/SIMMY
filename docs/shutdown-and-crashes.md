# Shutdown, quitting and crashes

## Overview

There is exactly **one way out** of a running game: the game's own exit path (as on a GC reset), then
`platform_shutdown_and_exit` ([`pc_main.cpp`](../src/platform/pc_main.cpp)), which releases the window(s),
the overlay and aurora, logs `[simmy] clean exit (code N)` and ends the process **without running static
destructors** (`TerminateProcess` on Windows, `_Exit` elsewhere). Crashes on Windows are written as a
symbolized report + minidump by [`pc_crash_win.cpp`](../src/platform/pc_crash_win.cpp); the launcher shows a
popup only for real failures.

## The exit sequence

| Trigger | Log line |
|---|---|
| F1 overlay -> Quit -> confirm (`platform_request_quit`) | `[simmy] quit requested` |
| Closing window 1, Alt+F4, an SDL quit event (`AURORA_EXIT`) | `[simmy] quit requested by the window (...)` |
| `--frames N` | `[simmy] --frames N reached: quitting` |
| `SIMMY_DET_QUIT=<tick>` (pc_det.cpp, via `platform_request_quit`) | `[simmy] quit requested` |
| Closing **player 2's** window | nothing: it is only hidden |

Each sets the quit handler installed by `ENgcEngine::Idle` ([`e_ngcengine.cpp`](../src/engine/e_ngcengine.cpp)):
`s_bShuttingDown = 1` (the console's reset flag) and `_pApp->m_quit = 1`. Then:

1. `EApp::Main` leaves its loop after the current frame, calls `Shutdown` and
   `EEngine::ShutdownThreads(true)` ([`e_engine.cpp`](../src/engine/e_engine.cpp)).
2. `ShutdownThreads` (PC order): **`_pBackground->Flush()` first** (a house load or memory card save in
   progress finishes before what it uses is shut down; GC flushed it last), controllers, audio (closes the
   SDL stream: the mixer thread is gone), effects, resource managers, `SignalShutdown`.
3. `StartMainThread` returns; `EGlobalManager::Shutdown`; `simmy_game_main` returns;
   `[simmy] game main returned N; M distinct unported functions were called`.
4. `platform_shutdown_and_exit(rc)`: `window2_shutdown`, `overlay_shutdown`, `aurora_shutdown` (GPU
   device, SDL windows and subsystems), `[simmy] clean exit (code N)`, `fflush`, then the process ends with
   the game's exit code.

Memory card commits are synchronous (aurora CARD); simmy.ini is saved in place; stdout/stderr are
unbuffered, so nothing is lost by ending abruptly.

**Why no static destructors.** The game's globals were made for a console that never exits: several
destructors are unported stubs, others free into EA heaps or join threads. And on Windows
`ExitProcess` would run the DLLs' detach code (D3D12/driver DLLs whose worker threads were killed
mid-wait), the classic source of exit-time hangs and crashes. `TerminateProcess` after the device and
windows are released skips both. **Do not add `exit()`/`return` paths that bypass this**, and do not rely
on any global destructor running.

Benign noise at the end of a log: aurora's `Buffer mapping Aborted` (staging buffers destroyed with a
pending map) and Dawn's `Device lost: Device was destroyed`.

Quit to the main menu (not a process exit): pause -> options -> Quit -> "save before quitting?"
(yes saves first, no goes on) -> flow MainMenu -> `InLevelBaseState::HandleExitToMainMenu` (the pause
menu, HUD and memory card targets go; quit request DAT_80475C20) -> once the loading screen is up,
`ELiveMode::QuitToMainMenu` (8008DC90), all as on GC and in this order:

1. `PlumbBob::ShutdownAll`; `ObjectModuleImpl::KillAllObjects` (800F5E78: pending camera animations
   released, every object, people included, through `KillObject` with the "killing all" flag (+0x208C: no
   house recalculation), id and sim-flag lists cleared, marked sprites removed, passive influences flushed);
2. `InteractorModule::ShutdownInteractorModule` (80210464; PC: the stand-in interactors, place/grab
   manipulators, build tools and social-mode state; there is no CameraManager to clear);
3. `HouseImpl::SetFamilyToNull`, `ESim::DeletePlayerCachedModel`, `cSoundPlayer::QuietAll`, DAT_80475C18;
4. `EGameStateMan::KillState` -> `ELiveMode::Reset` (8008CA58): heaps compacted, sounds fade (mode 3),
   `UnloadLevelGlobalData`, vibration off, the shader, plumb bobs and interactors again, the camera,
   `delete EHouse` (`EHouse::Cleanup` 80024CFC: instances out of the level, room lights, walls, floors, the
   instance manager, the level's light grid and room count, the level, the rain), the kill queue, the
   folder's unused data (stub), `cXPersonImpl::CleanupStaticMemory`, the particle systems, all effects, the
   folder's caches (stub), the room manager;
5. the simulator `SetMode(0)` and `Pause`.

Back on the front end, `TheSimsEnterMainMenuState::Reset` (80097EF0) clears the two-player mode and the
new-game / free-play / author-mode flags; `TheSimsEnterMainMenuState::Update` (8009817C) skips its background
stages once the fixed world exists, so `EGlobal::SetDefaults` (and `Globs::Startup`'s object folder, careers,
neighbourhood and sound player) runs once per boot, as on GC (running it again leaked the previous set every
cycle, the old sound system with every sample it had cached: see "Memory across games" below);
`TheSimsMainMenuState::Update` (80098838) clears a leftover quit request and, when the game returns to it, calls
`GameData::GamePlayShutdown` (8003B1E4): no current house, `NeighborhoodImpl::Unload` (neighbours, user
selectors, families). The next New Game or load reads the neighbourhood afresh. PC additions: the players'
model cache is emptied again on the way to the menu; a freed object stops being a passive-influence
target (`PassiveInfluenceSystem::ForgetObject`; GC drops it when it leaves the world); `KillObject` lets a
person's awareness release the animation controller before its instance goes; each player's action menu
(`FlashPiMenu`, kept by the `Player` across games) forgets its go-here object, and its module check
compares pointers only, as GC (it read the freed object: a crash when the next game's first action menu
closed after a co-op quit). Start is refused while the player's camera director letterboxes a shot (GC),
so a game cannot be quit during the lot's intro camera (that left the next game's loading screen "in
progress" and every pad ignored).

### Memory across games

Quit -> New Game / Continue, development-menu re-entry and lot changes keep RSS flat once the caches have
filled (measured over 20 games, 1P and co-op: [testing.md](testing.md) "Memory growth"). What used to
grow, and why:

- `EGlobal::SetDefaults` ran again at every return to the main menu (PC: `TheSimsEnterMainMenuState` did
  not skip its background stages as GC does once the fixed world exists). Each run's `Globs::Startup` made
  a new object folder (1,069 selectors, ~0.2 MB), careers, neighbourhood and sound player (with a new
  `cGZSndSys` and `cBoxX`) and dropped the old ones without freeing them; the old sound system kept every
  sample it had cached referenced, so a session that played many sounds lost several MB per game.
- `ObjectFolderImpl::DeleteUserSelectors` deleted the CAS sims' selectors without `DestroyThumbnail`
  (GC: `DestroySelector` 800ED6C0): each game's thumbnails (texture, shader) stayed.
- `SAnimator2::~SAnimator2` (8005C23C) only removed props and particles: every killed person (each quit,
  visitors leaving) kept its pending, impatient-idle, long-idle and low-motive animations referenced, and a
  sim quit in a costume leaked its own description.
- The action menu rows, the action queue's pending icons and their callers did not take / give back the
  references GC does (`ActionMenu::MenuItem::Setup` 80182FF4 / `ClearMenu` 801831B8,
  `FlashPiMenu::AddObjectInteractionsToMenu` 80039FF4 / `CreateInteractionMenu` 8003A2E0,
  `PCTTarget::CreateGrabObjectMenu` 801D63B8, `ActionQueue::AddActionToHUD` 80119F60, `ACTTarget` iqRecord):
  a CAS sim's portrait shown in a menu could never be freed.
- Apt's `createTextField` kept each field's character for the program's life (~50 fields, 8 KB, per main
  menu visit); it is now freed with the field (and its duplicates).

What stays and is fine: the resource managers' caches of unreferenced resources (at most 800 each, oldest
evicted first: animations, models, textures, sound events...), the samples within the GC ARAM budget
([audio.md](audio.md)), aurora's GPU texture caches (LRU, byte budgets), the loading music, the sims' skin
shader clones (`EIStaticModel::ReplaceBaseTexture`: "cloneN" names, at most 99 per shader before the
shader cache is flushed, as GC), static scratch buffers (censor, GX FIFO). They fill during the first
games and plateau. Linux RSS also carries glibc's own retention: after a large block (e.g. a music stream's
buffer, several MB) is freed, glibc raises its mmap threshold and serves later large blocks from its heap,
which it keeps; that is ~20 MB of the RSS after the second game and the occasional one-off step of 2-6 MB
at a later game. With the threshold fixed (`GLIBC_TUNABLES=glibc.malloc.mmap_threshold=131072`) the same
20 games stay at 76-78 MiB with no steps (96 MiB otherwise); the live heap (tools/mtrack) does not jump at
those steps. Not changed in the game (Windows' allocator behaves differently; nothing is lost).
`ObjectFolderImpl::FreeUnusedData` / `ClearAllCaches` and `Path::DeallocateAllPathData` stay stubs: the PC
keeps no per-selector anim tables or path space partition for them to free.

## Crash capture (Windows, [`pc_crash_win.cpp`](../src/platform/pc_crash_win.cpp))

- Installed during static initialisation (before the game's globals), and re-installed after
  `aurora_initialize` (`crash_reinstall_handlers`) in case SDL/Dawn/the driver replaced it.
- Covers unhandled SEH exceptions on any thread (`SetUnhandledExceptionFilter`), heap corruption raised as
  `STATUS_HEAP_CORRUPTION` (vectored handler), `abort`/SIGABRT, `std::terminate`, pure virtual calls,
  CRT invalid parameters.
- A helper thread writes `simmy_crash.txt` (symbolized stack with module+offset, via dbghelp and
  `simmy.pdb` next to the exe) and `simmy_crash.dmp` (`MiniDumpNormal | MiniDumpWithThreadInfo` + the
  exception record), next to the exe; then `TerminateProcess` with the exception code (3 for
  abort/terminate/purecall/invalid parameter). One report per process.
- Not catchable in-process: `__fastfail` / `/GS` cookie failures (0xC0000409) and some heap fail-fasts;
  the launcher still reports their exit code.
- `SIMMY_WATCHDOG=N`: after N seconds every thread's stack (raw module-relative addresses) goes to
  `simmy_watchdog.txt`. A timed snapshot for diagnosing hangs, **not** a hang detector; the launcher ignores it.
- On Linux there is no crash handler: run under gdb (`gdb -batch -ex run -ex bt --args ...`) or an ASan
  build (gcc `-fsanitize=address`; clang's runtime may be missing in containers).

## Launcher handling ([launcher.md](launcher.md))

The launcher shows its error popup only for a **nonzero exit code** or a `simmy_crash.txt` written during
this run; it strips `SIMMY_*` variables (except `SIMMY_INI`) from the game's environment so dev switches
such as `SIMMY_WATCHDOG` cannot leak into normal play (`SIMMY_LAUNCHER_KEEP_ENV=1` keeps them).

## Testing the exit path

Cases worth running after touching shutdown, threads or the background thread (each must exit 0, log
its trigger and end with `[simmy] clean exit (code 0)`; with ASan: no report): F1 Quit in a house, in the
front end, in co-op with window 2 closed first (`SIMMY_OVERLAY_TEST` actions `f1`, `close2`, `sdlquit`);
an SDL quit event; quit while the house load runs (`SIMMY_TEST_QUIT_BG=3`: quit when the 3rd background
routine is queued) and as the first routine is queued (`=1`); headless quits. See [testing.md](testing.md).
