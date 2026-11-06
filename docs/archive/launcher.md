# SimmyLauncher notes

Tree: /home/claude/gfx/port (diff vs base74: launcher.diff). Build: `ninja -C /home/claude/gfx_build simmy_launcher simmy` (clang, 0 warnings).

## Files
- src/launcher/launcher_main.cpp: app, pages (home / coop / options / playing + error modal), input (mouse, keys, gamepad with repeat), launch + crash detection, test switches.
- canvas.{hpp,cpp}: SDL_RenderGeometry drawing, 1px feathered AA convex shapes, gradients, glows, stb_truetype text atlas (Poppins Bold/Medium, em-sized, DPI-scaled).
- art.{hpp,cpp}: sky/clouds/hills, plumbob (hexagonal bipyramid, lit facets, sparkle), wordmark, disc, controller, layout pictures, icons.
- gcm_disc.{hpp,cpp}: GCM check + opening.bnr (RGB5A3 decode, BNR1/BNR2 names), detects RVZ/WIA/CISO/WBFS/GCZ/TGC/NKit/Wii/wrong game/truncated.
- launcher_ini.{hpp,cpp}: simmy.ini line editor (game's key aliases; [launcher] section; iso= top-level).
- game_process.{hpp,cpp}: CreateProcessW (handle-list inheritance, CREATE_NO_WINDOW, env set/restore) / posix_spawn (setsid).
- sfx.{hpp,cpp}: synthesized bloops via SDL audio stream.
- third_party/stb_truetype.h (imgui's 1.26 copy), fonts/ (Poppins OFL + OFL.txt), embed_file.cmake, res/simmy_launcher.{rc,ico}.
- Shared: CMakeLists.txt (exclude src/launcher from SIMMY_PORTED glob; simmy_launcher target), README.md, src/platform/pc_settings.cpp (LoadIni skips [launcher]).

## Mapping
- Play: `simmy --iso <disc> --split=gc`; Co-op: `--split=<windows|wide|4x3|gc>`.
- Env: SIMMY_SKIP_INTRO=1 if skip_intro or quick co-op; SIMMY_TWO_PLAYER=1 + SIMMY_CAS_ACCEPT=300 if quick co-op; otherwise these three are removed from the child env.
- cwd = simmy.exe's folder; stdout/stderr -> game_out.txt / game_log.txt next to the launcher (same folder as simmy.exe in a normal build); simmy_launcher_log.txt records each launch + exit code.
- simmy.ini path: $SIMMY_INI else next to the launcher (same rule as the game).

## Tests run (Linux, SDL offscreen + software renderer)
- make_test_iso.py -> discs/: good (banner from SIMMY/ngcbanner.tpl), PAL, no banner, RVZ, WIA, CISO, GCZ, random, Wii, NKit, other game, truncated, tiny: all classified correctly.
- Stub game (stub/simmy): args/env/cwd/log redirection verified for play + coop; nonzero exit, SIGSEGV + simmy_crash.txt, missing exe -> error panel; minimize/restore logged.
- ini editing on simmy.ini.example and on a missing file; game reads the result with no warnings except `iso` (unknown until the ISO worker lands it).
- Screenshots: /mnt/user-data/outputs/launcher_*.png (1x, plus 200% home).

## Open / untested
- Win32 paths (CreateProcessW, ShellExecuteW, .rc, WinMain via SDL_main.h, file dialog, DPI scale on real monitors) compile-reviewed only.
- `iso=` values containing ';' or '#' are cut by the game's ini parser (comment chars); the launcher always passes --iso on the command line, so launches are unaffected.
- build_simmy.bat reads iso= with findstr under chcp 65001 (non-ASCII paths); untested on Windows.
