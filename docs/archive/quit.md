# quit: "the game crashed when I did Quit from the F1 overlay"

Tree /home/claude/gfx/port (+ /home/claude/gfx/aurora, unchanged), build /home/claude/gfx_build.
Diff vs base80: `quit.diff`. Sanitizer build: /home/claude/gfx_asan (gcc 13, -fsanitize=address; see below).

## Root cause (confirmed by the user: "an error shown in a launcher popup")

Not a game crash. The game quit cleanly (exit code 0; its log ends exactly like a clean Linux quit:
`game main returned 0`, then the stub static destructors `BString::~BString` ... `EScratchBuffMan::~EScratchBuffMan`,
which is the normal last line of every run). build_simmy.bat leaked `SIMMY_WATCHDOG=40` into the
launcher's environment, the game inherited it, and the watchdog thread wrote `simmy_hang.txt` 40 s into the
run (a timed snapshot, not a hang detector). On exit the launcher (launcher_main.cpp pollGame) treated any
`simmy_crash.txt` / `simmy_hang.txt` newer than the launch as a failure and showed "Uh-oh! Something went
wrong ... The game stopped responding and wrote a hang report. Exit code 0."

The aurora warnings at the end of game_out.txt are benign shutdown noise: `Buffer mapping Aborted`
is aurora's staging buffers (lib/gfx/frame.cpp map_staging_buffer) whose pending MapAsync is cancelled when
gfx::shutdown destroys them (the callback only resets aurora's own still-alive slot state);
`Device lost: Device was destroyed` is Dawn's device-lost callback for the explicit device destruction.

## Fixes

1. Launcher (src/launcher/launcher_main.cpp, game_process.cpp/.hpp)
   - Every `SIMMY_*` variable in the launcher's environment except `SIMMY_INI` (and the three the
     launcher sets itself: SIMMY_SKIP_INTRO, SIMMY_TWO_PLAYER, SIMMY_CAS_ACCEPT) is removed from the
     game's environment (prefix rule rather than a list, so new test switches are covered:
     SIMMY_WATCHDOG, SIMMY_DET*, SIMMY_TEST_*, SIMMY_SHOTS, SIMMY_PAD_SEQ, SIMMY_GFX, ...).
     `SIMMY_LAUNCHER_KEEP_ENV=1` keeps them (development). The removed names are written to
     simmy_launcher_log.txt.
   - Popup only for real failures: nonzero exit code, or a `simmy_crash.txt` written during this run.
     "Force quit" is not a failure. The watchdog snapshot is now `simmy_watchdog.txt` and the launcher
     ignores it. Wording: "The game crashed and wrote a crash report." / "The game stopped unexpectedly:
     <description>." + exit code + "Please send these files with your report: game_log.txt, game_out.txt,
     simmy_crash.txt, simmy_crash.dmp (the button below opens their folder)."
   - simmy_launcher_log.txt: exit code in decimal and hex (as before), "(force quit)", the crash report's
     path, the minidump's path and the report's first 40 lines.
2. Game exit path (single sequence for every exit)
   - All exits go through the game's own exit path: F1 Quit (`platform_request_quit`), closing window 1 /
     Alt+F4 / SDL quit (AURORA_EXIT), SIMMY_DET_QUIT, --frames. Each logs why
     (`[simmy] quit requested`, `... by the window`, `--frames N reached`). Closing player 2's window only
     hides it (unchanged).
   - EApp::Main -> EEngine::ShutdownThreads: PC addition first flushes the background thread
     (`_pBackground->Flush()`), so a house load or a memory-card save running when the player quits
     finishes before controllers, audio, effects and the resource managers it uses are shut down (the
     original order flushes it last, inside ShutdownResourceManagers). Audio: ENgcAudio::Shutdown already
     closes the SDL stream (mixer thread gone). Memory card commits are synchronous (aurora card.cpp).
   - pc_main.cpp `platform_shutdown_and_exit(rc)`: window 2, overlay, aurora_shutdown (GPU device, SDL
     windows/subsystems), `[simmy] clean exit (code N)` as the log's last line, fflush, then the process
     ends without static destructors: Windows `TerminateProcess(GetCurrentProcess(), rc)`, else `_Exit`.
     Why: the game's globals were made for a console that never exits (stub destructors, heaps, threads);
     TerminateProcess after the device and windows are released skips both static destructors and the
     DLLs' DLL_PROCESS_DETACH code (d3d12 / driver DLLs whose worker threads ExitProcess would kill
     mid-wait), the usual source of exit-time hangs/crashes. stdout/stderr are unbuffered (writes are
     already in the files), simmy.ini is saved in place, memory card files committed. Exit code = the
     game's (0).
3. Crash capture (src/platform/pc_crash_win.cpp, Windows only)
   - Unhandled SEH exceptions on any thread (SetUnhandledExceptionFilter; re-installed after
     aurora_initialize in case SDL/Dawn/the driver replaced it), heap corruption where Windows raises
     0xC0000374 (vectored handler, that code only), abort/SIGABRT, std::terminate, pure virtual calls
     (_set_purecall_handler), CRT invalid parameter (_set_invalid_parameter_handler).
   - Written by a helper thread (works for stack overflow; the crashed thread may hold locks):
     simmy_crash.txt (symbolized stack, now with module+offset for offline symbolization) and
     simmy_crash.dmp (MiniDumpWriteDump, MiniDumpNormal | MiniDumpWithThreadInfo, with the exception
     record), then TerminateProcess with the exception code (3 for abort/terminate/purecall/invalid
     parameter) so the launcher can name it. One report per process (a second crash waits).
   - Not catchable in-process: __fastfail / /GS cookie failures (0xC0000409) and some heap-corruption
     fail-fasts: the launcher still shows and logs their exit code.
   - SIMMY_WATCHDOG=N now writes simmy_watchdog.txt ("WATCHDOG snapshot after N s").
4. Test hooks: `SIMMY_TEST_QUIT_BG=N` (background.cpp: request the quit when the Nth background routine
   is queued; 3 = the house load, 1 = the first front-end routine); overlay test actions `sdlquit` (pushes
   SDL_EVENT_QUIT) and `close2` (player 2's window close handler).

## Tests

Exit regression test: `exit_test.sh` (+ `q1.sh`, one run; header of exit_test.sh documents it):
`bash exit_test.sh [cases]`, `REPEAT=n`, `JOBS=n`, `ASAN=1` (uses /home/claude/gfx_asan/simmy, no forced
readback because that build has assertions on), `BIN=`. Cases: f1_game, f1_menu, f1_coop (split=windows,
player 2's window closed first), sdl_game (SDL quit event = window close / Alt+F4), load (quit while the
house load runs on the background thread), save_menu (quit as the first background routine is queued),
hl_game, hl_load (headless). Each must exit 0, log the expected quit trigger, end its log with
`[simmy] clean exit (code 0)` and have no sanitizer report. Logs in run/x_<case>_<n>/.

Results:
- Before the change (gfx_build as given): F1 Quit offscreen in a house exits 0 and the log ends with the
  same stub destructors as the user's log (run f1a). Headless quits at many points and during the house
  load: all exit 0.
- ASan (+ static destructors, by a temporary switch that returned from main; removed again): all 8
  cases exit 0, no ASan reports, including static destruction after the game's exit path
  (asan_static_dtors.txt). The ASan build is gcc 13 -fsanitize=address -O1 (clang has no sanitizer runtime
  here; gcc's UBSan breaks abseil's constexpr, so UBSan was dropped).
- New exit path, normal build: `REPEAT=2 exit_test.sh` 16/16 PASS (normal.txt); ASan build 8/8 PASS
  (asan3.txt).
- Launcher (stub game in lt/, SDL offscreen, --auto-launch): exit 0 + simmy_watchdog.txt + simmy_hang.txt
  -> no popup (lt/shot_clean.png); simmy_crash.txt + .dmp -> "The game crashed and wrote a crash report"
  popup and the report in simmy_launcher_log.txt (lt/shot_crashfile.png); exit 3 -> popup. SIMMY_WATCHDOG,
  SIMMY_DET, SIMMY_TEST_X were removed from the stub's environment, SIMMY_INI kept, logged.
- m16 suite (copy in suite/ pointed at gfx_build): cancels 1, 0 script errors/TREEASSERT; toilet 59.8 then
  56.7 on a rerun, sofa 28.3, fridge 31.7, shower 46.8 then 45.5, bed 32.6 (headless run-to-run variance).
- Determinism: L.rec replay with the boot flags: 600/600 hashes match linux_L.hash (det/).

## Windows-only, untested here
- pc_crash_win.cpp could not be compiled here (no MSVC/mingw); syntax-checked with clang against a
  minimal API shim (quit/shim/, signatures from the SDK docs). game_process.cpp's
  GetEnvironmentStringsW enumeration likewise review-only.
- TerminateProcess at the end of a normal run, minidump writing, the vectored handler, handler
  re-installation: not run on Windows.

## Recreating the ASan build (deleted after the tests to save disk)
```
mkdir /home/claude/gfx_asan && cd /home/claude/gfx_asan && Q=$S/quit
cmake -G Ninja -S /home/claude/gfx/port -B . -DBUILD_TESTING=OFF -DSDL_UNIX_CONSOLE_BUILD=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  "-DCMAKE_C_FLAGS=-fsanitize=address -fno-omit-frame-pointer" "-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer" \
  "-DCMAKE_C_FLAGS_RELWITHDEBINFO=-O1 -g1" "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g1" -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address \
  $(cat $Q/cacheopts.txt) $(cat $Q/sdlopts.txt) $(cat $Q/auroraopts.txt)
ninja -j2 simmy    # ~1 h on 2 cores
```
