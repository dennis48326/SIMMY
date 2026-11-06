# iso: SIMMY reads its game data straight from a GameCube disc image

Tree `/home/claude/iso/port` (aurora `/home/claude/iso/aurora`, build `/home/claude/iso_build`).
Full diff vs `/home/claude/base74/port`: `$S/iso/iso.diff`. CMakeLists.txt unchanged (sources are globbed).

## Files
New:
- `src/platform/gcm_disc.hpp/.cpp`: `HostFile` (read-only native file, positional reads: `pread` on POSIX,
  `ReadFile` + OVERLAPPED offset on Windows, UTF-8 paths via `CreateFileW`; no shared seek state, so any
  number of threads can read one handle) and `GcmDisc` (boot.bin checks, FST parse into a hash map of
  normalized paths -> {offset,size}; folder set; game ID/title/version).
- `tools/make_test_iso.py` (stdlib): folder -> GCM (boot.bin with ID/magic/title/DOL/FST fields, bi2.bin,
  placeholder apploader header + DOL header, FST + strings, files 32 KB aligned (`--align`), `--exclude`
  (default `decomp`), `--full-size` sparse pad, `--id` default `SIMT01`).
Changed:
- `include/simmy/platform.hpp`: `data_root/set_data_root/resolve_data_path` replaced by the data-source API
  (`data_source_open_disc/folder`, `data_source_ready`, `data_source_description`, `DataFile`,
  `data_open/close/exists/size/read/seek/tell/error/read_at/read_file`).
- `src/platform/data_path.cpp`: rewritten as the virtual file layer (disc or folder backend), 64 KB read-ahead
  for sequential reads, per-handle mutex, required-file check, `SIMMY_IO_STATS=1`.
- `include/simmy/classes/engcfile.hpp`, `src/base/e_ngcfile.cpp`: ENgcFile on `simmy::DataFile*`; write modes
  refused (nothing in the game opens files for writing; the SN host device is a devkit thing).
- `src/base/e_ngcfilesystem.cpp`: warning uses `data_exists`.
- `src/engine/e_ngcstream.cpp` (music reader thread: own handle, `data_read_at`), `src/engine/e_ngcmovie.cpp`
  (THP: `data_read_at`, decoder thread + game thread no longer share a FILE* position),
  `src/engine/e_ngcmemorycard.cpp` (icon/banner TPL via `data_read_file`), `src/game/nghresfile.cpp`
  (`ReadFromFile` keeps the game path in `m_fileName`, houses read with `data_read_at`), `src/game/nghsave.cpp`
  (`ReadSection`).
- `src/platform/pc_settings.hpp/.cpp`: `isoPath` + key `iso` (aliases disc/discimage/isopath/gcm). Value kept
  verbatim (case, spaces, `;`, `#`), surrounding quotes stripped; on the iso line the whole rest is the value
  (no inline comment). `--iso <p>` / `--iso=<p>` parse through settings_parse_arg (CLI beats ini). Saved by F1
  "Save to simmy.ini" (kNames has iso): an existing line is rewritten in place keeping its quotes, an empty
  value never blanks the launcher's line, a new file gets `iso = <path>` appended.
- `src/platform/pc_main.cpp`: `open_game_data` after `settings_load`, before aurora init. Order: `--data` (wins,
  logged) > `--iso`/ini `iso=` > `SIMMY_DATA` env > error (stderr; `SDL_ShowSimpleMessageBox` on Windows when not
  headless/offscreen) and `return 1`. The old `./files` default is gone.
- `README.md` (saves/writes paragraph, build/usage block + "Game data" paragraphs, two examples),
  `simmy.ini.example` (header example, new `[game]` section with `;iso =`).

## Read path
startup: `GcmDisc::open` reads 0x440 bytes of boot.bin, rejects other formats, reads the FST once and builds
`unordered_map<lower-case "data/index.ind", {offset,size}>`; checks the image is long enough for every file;
then requires DATA/index.ind, quickdat/textures/shaders/fonts/datasets.arc and default.ngh (warns about
movies/audiostr/samples/icons). Game ID is logged; a non-`G4Z*` ID is only a note (any region works).
`data_open(path)` = hash lookup -> DataFile{shared image handle, base, size}; no OS call per open in disc mode.
Sequential reads (ENgcFile) go through a 64 KB read-ahead buffer; big reads go straight to the caller.
Threads (music reader, THP decoder) use `data_read_at` (positional, lock-free).

## Writes (none go near the data)
- Memory card saves: aurora CARD, `%APPDATA%\SIMMY\USA\Card A\*.gci` (SDL pref path; `CARDInit("G4ZE")` is
  hard-coded so a PAL disc also uses USA - existing saves stay findable). Headless runs (aurora not
  initialised) put the card in `./USA/Card A` (cwd) as before.
- simmy.ini: next to the exe (or --config / SIMMY_INI); crash/hang reports: next to the exe (pc_crash_win.cpp);
  GPU cache: pref path; screenshots / SIMMY_AUDIO_WAV / SIMMY_PORTRAIT_DUMP: paths the developer names (cwd).
- Nothing wrote into the data folder before either (checked every fopen/CreateFile/SDL_IOFromFile); ENgcFile
  now refuses write modes outright. No migration needed.

## Measurements (headless, suite sequence to the house, SIMMY_FRAMES_FROM_HOUSE=1 --frames 1, 2-core sandbox)
| | wall to house | opens | host reads | MB | time in reads |
|---|---|---|---|---|---|
| ISO warm | 29.52 / 29.53 s | 32 (0.1 ms) | 460 | 31.9 | 7.2 ms |
| folder warm | 29.47 / 29.51 s | 32 (1.1 ms) | 460 | 31.9 | 6.6 ms |
| ISO cold (drop_caches) | 29.50 / 29.55 s | 32 (0.1 ms) | 460 | 31.9 | 24.1 / 25.6 ms |
| folder cold | 29.50 / 29.50 s | 32 (2.0 / 1.9 ms) | 460 | 31.9 | 26.2 / 24.7 ms |
Wall time is the 60 Hz frame pacing; the I/O itself is ~25 ms cold either way (ISO opens are free, folder
opens cost ~2 ms of directory scans). Full intro boot (EA + intro movies, 3300 frames): 12 opens, 1410 reads,
42.5 MB, 21.7 ms. Music test: same 236 reads / 14.9 MB, byte-identical WAV from ISO and folder.

## Tests
- `$S/iso/hl.sh`, `$S/iso/suite.sh` (copies; SRC="--iso ..."|"--data ...", RUN=dir). Suite toilet sofa fridge
  shower bed cancel, ISO (`run_iso/`) and folder (`run_data/`): 0 "script error", 0 TREEASSERT in all 12 logs,
  "cancels: 1" = 1 in both s_cancel.log; outputs identical except run-to-run motive jitter (bladder 56.7 vs 56.9,
  hygiene 45.6 vs 45.5; fix4 notes saw 57.0/56.9 between identical builds).
- Movies: EA (120 frames) + intro (1953 frames) from movies.arc on the ISO, sound recorded (run_iso/intro.wav);
  screenshot `/mnt/user-data/outputs/iso_intro_movie.png` (frame 1500).
- Music: SIMMY_AUDIO_TEST=music_48_title_track, ISO vs folder WAV md5 identical.
- Food creation: SIMMY_ING_TEST=150 offscreen from the ISO: "Target class for screen [ING] has loaded",
  `/mnt/user-data/outputs/iso_food_creation.png`; also "Get Ingredients" queued/run from the fridge headless.
- Shots: `/mnt/user-data/outputs/iso_start.png` (start screen, disc taken from iso= in an ini whose path has
  spaces, `#` and `;`), `/mnt/user-data/outputs/iso_house.png`.
- Save: from the ISO, pause > Save ("[save] house 9 saved: ..., 196 objects" -> run_save/USA/Card A/
  69-G4ZE-Sims 2 Save.gci); new process Continue: "saved neighbourhood ... entering house 9 (its house)",
  07:05 (the saved time). 0 script errors.
- ini round trip: F1 save (SIMMY_OVERLAY_TEST=320:save) with a quoted iso= line: file unchanged; unquoted
  stays unquoted; `[graphics]`/`[game]` sections and other comments kept.
- Error cases (`$S/iso/err`), all exit 1 with a message, no crash: missing path; a folder; fake RVZ/WIA/CISO/GCZ
  headers; Wii magic; NKit ("NKIT" at 0x200); random bytes (no GC magic); 100-byte file; image truncated to
  700 MB ("is incomplete: it is 734003200 bytes, but its files reach up to byte 1327498816"); a GCM without
  DATA (lists the missing files). No --iso/ini/--data: "Start SimmyLauncher.exe and choose the disc image...".
- Windows branch of gcm_disc.cpp syntax-checked with clang against a minimal windows.h shim (no MSVC here).

## Open issues
- Not built with MSVC here. Things to watch: none known (no VLAs/builtins; `strnlen`, `CreateFileW`,
  `ReadFile` OVERLAPPED, `std::filesystem` u8 conversions).
- Compressed images are rejected, not read (aurora's DVD layer links nod, which could read RVZ/WIA/GCZ/CISO
  if that is ever wanted).
- `build_simmy.bat` (outside the tree) may still pass `--data`; it keeps working, but the launcher/--iso is the
  documented way.
- A relative `iso=` path resolves against the working directory (the launcher writes absolute paths).
- Headless card location (cwd/USA) is unchanged dev behaviour.
- Test images: `$S/iso/test.iso` deleted after the tests (re-make in ~50 s with
  `python3 -I tools/make_test_iso.py /mnt/user-data/uploads/SIMMY $S/iso/test.iso`).
